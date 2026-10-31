"""Mesh lint for the bird builds (build_bird.py --lint runs it on every mesh it built, in the same Blender process).

Reports, per mesh:
    degenerate   zero-area or sliver triangles
    uv_zero      triangles with (near) zero uv area: breaks tangents / the normal map
    uv_stretch   uv -> surface mapping much more anisotropic than its neighbours in the same atlas cell, or texel
                 density far below the cell's median (the texture smeared over the face)
    uv_outside   uvs leaving the atlas cell the face samples (bleeding into a neighbour cell)
    zfight       two faces of different parts, normals within 15 deg, every vertex of one within ZF of the other's
                 plane (and not passing through it at an angle), overlapping: they fight in the depth buffer
    crossing     (info) faces that pass through each other within ZF: an intersection line, not a shared plane
    inside       the same, one face wholly inside the other (a face inside a face)
    near_layers  (info, not an issue) overlapping parallel faces of different parts ZF..2 mm apart: deliberate layers
                 (coverts on flight feathers, a feather's two layers are one part)
Parts: the mesh's face attribute "lint_part" (one per card / tube / shell); without it every loose piece is a part.
"""
import math
import os

import numpy as np

ZF = float(os.environ.get("LINT_ZF", 0.0001))         # m: closer than this and parallel = z-fighting (0.1 mm: real separations of layered feathers
                      # are 0.1-2 mm; depth precision at close-up range is far finer, but shadow maps and TAA are not)
COS = float(os.environ.get("LINT_COS", 0.966))   # normals within 15 deg count as parallel
NEAR = 0.002          # m: the "within ~2 mm" neighbourhood
PARAMETRIC = ("body", "beak", "legs", "eye", "eyering")
PUSH = []
DUMP = [] if os.environ.get("LINT_DUMP") else None
HIST = (0.00002, 0.00005, 0.0001, 0.00025, 0.0005, 0.001)


def _tris(ob):
    me = ob.data
    me.calc_loop_triangles()
    n = len(me.loop_triangles)
    co = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    mw = np.array(ob.matrix_world)
    co = co @ mw[:3, :3].T + mw[:3, 3]
    vi = np.empty(n * 3, int)
    me.loop_triangles.foreach_get("vertices", vi)
    li = np.empty(n * 3, int)
    me.loop_triangles.foreach_get("loops", li)
    pi = np.empty(n, int)
    me.loop_triangles.foreach_get("polygon_index", pi)
    mi = np.empty(n, int)
    me.loop_triangles.foreach_get("material_index", mi)
    uv = np.empty(len(me.loops) * 2)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    P = co[vi.reshape(-1, 3)]
    U = uv[li.reshape(-1, 3)]
    if "lint_part" in me.attributes:
        pp = np.empty(len(me.polygons), int)
        me.attributes["lint_part"].data.foreach_get("value", pp)
        part = pp[pi]
    else:
        part = _islands(me)[pi]
    return P, U, part, mi, pi


def _islands(me):
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(me)
    lab = np.full(len(bm.faces), -1)
    bm.faces.ensure_lookup_table()
    k = 0
    for f in bm.faces:
        if lab[f.index] >= 0:
            continue
        st = [f]
        lab[f.index] = k
        while st:
            g = st.pop()
            for e in g.edges:
                for h in e.link_faces:
                    if lab[h.index] < 0:
                        lab[h.index] = k
                        st.append(h)
        k += 1
    bm.free()
    return lab


def _overlap2d(a, b, shrink=0.04):
    """a: (3,2), b: (n,3,2) triangles; True where they overlap (separating axis), both shrunk a little toward their
    centroids so faces that only share an edge or a corner don't count."""
    a = a + (a.mean(0) - a) * shrink
    b = b + (b.mean(1, keepdims=True) - b) * shrink
    ok = np.ones(len(b), bool)

    def axes(t):   # (..., 3, 2) edge normals
        e = np.roll(t, -1, axis=-2) - t
        return np.stack([-e[..., 1], e[..., 0]], -1)
    for ax in np.concatenate([np.broadcast_to(axes(a), (len(b), 3, 2)), axes(b)], axis=1).transpose(1, 0, 2):
        pa = np.einsum("nk,jk->nj", ax, a)
        pb = np.einsum("nk,njk->nj", ax, b)
        sep = (pa.max(1) < pb.min(1)) | (pb.max(1) < pa.min(1))
        ok &= ~sep
    return ok


def _inside2d(a, b):
    """every vertex of each b (n,3,2) inside a (3,2)"""
    e = np.roll(a, -1, 0) - a
    sgn = np.sign(e[0, 0] * (a[2, 1] - a[0, 1]) - e[0, 1] * (a[2, 0] - a[0, 0]))
    res = np.ones(len(b), bool)
    for k in range(3):
        c = e[k, 0] * (b[..., 1] - a[k, 1]) - e[k, 1] * (b[..., 0] - a[k, 0])
        res &= np.all(c * sgn >= -1e-12, axis=1)
    return res


def lint(ob, atlas=None, label=None, part_names=None, verbose=True, collect=False):
    from mathutils import kdtree
    label = label or ob.name
    P, U, part, mi, pi = _tris(ob)
    n = len(P)
    e1, e2 = P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]
    cr = np.cross(e1, e2)
    area = 0.5 * np.linalg.norm(cr, axis=1)
    longest = np.max(np.stack([np.linalg.norm(e1, axis=1), np.linalg.norm(e2, axis=1), np.linalg.norm(P[:, 2] - P[:, 1], axis=1)]), axis=0)
    degenerate = (area < 1e-12) | (area < 2e-4 * longest ** 2)
    nrm = cr / np.maximum(2 * area, 1e-20)[:, None]
    res = {"tris": int(n), "degenerate": int(degenerate.sum())}
    size = atlas.size if atlas is not None else 2048
    f1, f2 = U[:, 1] - U[:, 0], U[:, 2] - U[:, 0]
    uva = 0.5 * np.abs(f1[:, 0] * f2[:, 1] - f1[:, 1] * f2[:, 0]) * size * size   # texels^2
    uv_zero = (uva < 0.002) & ~degenerate
    res["uv_zero"] = int(uv_zero.sum())
    # uv -> 3D jacobian: singular values
    det = f1[:, 0] * f2[:, 1] - f1[:, 1] * f2[:, 0]
    good = (np.abs(det) * size * size > 1e-3) & ~degenerate
    inv = np.zeros((n, 2, 2))
    inv[good, 0, 0], inv[good, 0, 1] = f2[good, 1] / det[good], -f2[good, 0] / det[good]
    inv[good, 1, 0], inv[good, 1, 1] = -f1[good, 1] / det[good], f1[good, 0] / det[good]
    E = np.stack([e1, e2], axis=2)          # (n,3,2)
    J = E @ inv                              # (n,3,2): d(3D)/d(uv)
    sv = np.linalg.svd(J, compute_uv=False)  # (n,2)
    aniso = np.where(good, sv[:, 0] / np.maximum(sv[:, 1], 1e-12), 1.0)
    dens = np.where(good, 1.0 / np.maximum(sv[:, 0] * sv[:, 1], 1e-20), 0.0)   # texel area per m^2 (relative)
    # cells: which atlas cell each face samples (its uv centroid), for density medians and the bleed test
    cell_id = np.full(n, -1)
    outside = np.zeros(n, bool)
    if atlas is not None:
        rects = list(atlas.cells.items())
        cu = U.mean(1) * size
        for ci, (key, (x0, y0, w, h)) in enumerate(rects):
            m = (cu[:, 0] >= x0) & (cu[:, 0] <= x0 + w) & (cu[:, 1] >= y0) & (cu[:, 1] <= y0 + h)
            cell_id[m] = ci
            px = U[m] * size
            tol = 0.02
            bad = np.any((px[..., 0] < x0 + 0.5 - tol) | (px[..., 0] > x0 + w - 0.5 + tol) | (px[..., 1] < y0 + 0.5 - tol) | (px[..., 1] > y0 + h - 0.5 + tol), axis=1)
            outside[np.where(m)[0][bad]] = True
        outside |= cell_id < 0
    res["uv_outside"] = int(outside.sum())
    stretch = np.zeros(n, bool)
    param = np.zeros(n, bool)
    if atlas is not None:
        # cells painted in the mesh's own parameter space (body: z x angle round the body; beak, legs, eye, eye ring
        # likewise): a squeeze toward a pole is that parametrisation, not a stretched picture. Checked for zero area
        # and bleeding only.
        for ci, (key, _) in enumerate(rects):
            if key in PARAMETRIC:
                param |= cell_id == ci
    res["param_cells"] = int(param.sum())
    for c in np.unique(cell_id):
        m = (cell_id == c) & good & ~param
        if m.sum() < 3:
            continue
        med_a = np.median(aniso[m])
        stretch |= m & (aniso > max(8.0, med_a * 4))
    # texel density: outliers inside one part (a card or tube maps one picture: a face far off its siblings is smeared;
    # different cards sharing a cell may differ in size on purpose)
    sel = np.where(good & ~param)[0]
    order = sel[np.argsort(part[sel], kind="stable")]
    bounds = np.flatnonzero(np.diff(part[order])) + 1
    for grp in np.split(order, bounds):
        if len(grp) < 3:
            continue
        med_d = np.median(dens[grp])
        bad = (dens[grp] < med_d / 8) | (dens[grp] > med_d * 8)
        stretch[grp[bad]] = True
    # a face whose uvs are a deliberate few-texel patch (one colour on purpose: legs' feathering, mouth lining) can't
    # show a stretched texture: counted apart, not as an issue
    patch = good & (uva < 2.0)
    res["solid_patches"] = int((stretch & patch).sum())
    stretch &= ~patch
    res["uv_stretch"] = int(stretch.sum())
    # z-fighting: nearby, parallel, coplanar within ZF, overlapping, different parts
    cen = P.mean(1)
    kd = kdtree.KDTree(n)
    for i, c in enumerate(cen):
        kd.insert(c, i)
    kd.balance()
    hist = [0] * len(HIST)
    zex = []
    ncross = [0]
    zf = np.zeros(n, bool)
    inside = np.zeros(n, bool)
    near = 0
    pairs = {}
    for i in range(n):
        if degenerate[i]:
            continue
        r = max(longest[i], NEAR)
        js = np.array([j for _, j, _ in kd.find_range(cen[i], r) if j > i], int)
        if not len(js):
            continue
        js = js[(part[js] != part[i]) & ~degenerate[js]]
        if not len(js):
            continue
        dots = np.abs(nrm[js] @ nrm[i])
        js = js[dots > COS]
        if not len(js):
            continue
        sd = (P[js] - P[i][0]) @ nrm[i]                 # (m,3) every vertex's signed distance to i's plane
        dmax = np.abs(sd).max(1)
        # and i's own vertices to j's planes (a small face near a big tilted one)
        sd2 = np.einsum("mk,jk->mj", nrm[js], P[i]) - np.einsum("mk,mk->m", nrm[js], P[js][:, 0])[:, None]
        dmax = np.maximum(dmax, np.abs(sd2).max(1))
        # faces that pass through each other at an angle (vertices clearly on both sides) meet in a line: that is an
        # intersection, not two surfaces in one plane; counted apart
        tol = ZF * 0.25
        cross = ((sd.max(1) > tol) & (sd.min(1) < -tol)) | ((sd2.max(1) > tol) & (sd2.min(1) < -tol))
        cand = (dmax < NEAR) & ~cross
        ncross[0] += int(((dmax < ZF) & cross).sum())
        js, dmax = js[cand], dmax[cand]
        if not len(js):
            continue
        # project to i's plane
        ax = e1[i] / (np.linalg.norm(e1[i]) + 1e-20)
        ay = np.cross(nrm[i], ax)
        A = np.stack([(P[i] - P[i][0]) @ ax, (P[i] - P[i][0]) @ ay], 1)
        B = np.stack([(P[js] - P[i][0]) @ ax, (P[js] - P[i][0]) @ ay], 2)
        ov = _overlap2d(A, B)
        close = ov & (dmax < ZF)
        for b_, lim_ in enumerate(HIST):
            hist[b_] += int((ov & (dmax < lim_)).sum())
        near += int((ov & (dmax >= ZF)).sum())
        if close.any():
            zf[i] = True
            zf[js[close]] = True
            ins = _inside2d(A, B[close]) | _inside2d_rev(B[close], A)
            inside[js[close][ins]] = True
            if collect:
                for jj, j in enumerate(js):
                    if close[jj]:
                        sg = 1.0 if sd[list(js).index(j)].mean() >= 0 else -1.0
                        PUSH.append((int(part[i]), int(part[j]), nrm[i] * sg, float(dmax[jj])))
            for j in js[close]:
                k = tuple(sorted((_pname(part_names, part[i], mi[i]), _pname(part_names, part[j], mi[j]))))
                pairs[k] = pairs.get(k, 0) + 1
                if DUMP is not None:
                    DUMP.append((k, tuple(np.round(cen[i] * 100, 2)), int(part[i]), int(part[j])))
                if pairs[k] <= 1:
                    zex.append(f"{k} at {np.round(cen[i] * 100, 2)} cm, parts {part[i]}/{part[j]}, gap {dmax[list(js).index(j)] * 1000:.3f} mm")
    if collect:
        res["_push"] = list(PUSH)
        PUSH.clear()
    res["zfight"] = int(zf.sum())
    res["inside"] = int(inside.sum())
    res["near_layers"] = near
    res["crossing"] = ncross[0]
    res["zfight_hist_mm"] = hist
    if DUMP is not None:
        with open(os.environ["LINT_DUMP"] + "_" + label.replace(" ", "_") + ".txt", "w") as fh:
            for row in DUMP:
                fh.write(repr(row) + "\n")
        DUMP.clear()
    res["zfight_pairs"] = sorted(pairs.items(), key=lambda kv: -kv[1])[:8]
    issues = {k: res[k] for k in ("degenerate", "uv_zero", "uv_stretch", "uv_outside", "zfight", "inside")}
    res["issues"] = int(sum(issues.values()))
    ex = {}
    cen0 = P.mean(1)
    for nm, m in (("degenerate", degenerate), ("uv_zero", uv_zero), ("uv_stretch", stretch), ("uv_outside", outside)):
        if m.any():
            cnt = {}
            for j in np.where(m)[0]:
                k = _pname(part_names, part[j], mi[j])
                cnt[k] = cnt.get(k, 0) + 1
            ex[nm] = sorted(cnt.items(), key=lambda kv: -kv[1])[:6]
            if verbose:
                for j in np.where(m)[0][:4]:
                    print(f"LINT     {nm} e.g. {_pname(part_names, part[j], mi[j])} at {np.round(cen0[j] * 100, 2)} cm aniso {aniso[j]:.1f} "
                          f"uv texels^2 {uva[j]:.3f} area mm^2 {area[j] * 1e6:.4f}")
    res["examples"] = ex
    if verbose:
        print(f"LINT {label}: tris {n} | " + " ".join(f"{k}={v}" for k, v in issues.items()) + f" | near_layers={near} crossing={ncross[0]}")
        for k, v in ex.items():
            print(f"LINT   {k}: {v}")
        print(f"LINT   overlapping parallel pairs closer than {[h * 1000 for h in HIST]} mm: {hist}")
        if res["zfight_pairs"]:
            print(f"LINT   zfight pairs: {res['zfight_pairs']}")
            for e in zex[:30]:
                print("LINT     zfight e.g.", e)
    return res


def _inside2d_rev(B, A):
    """A (3,2) wholly inside each of B (n,3,2)"""
    out = np.zeros(len(B), bool)
    for k in range(len(B)):
        out[k] = _inside2d(B[k], A[None])[0]
    return out


def _pname(names, p, m):
    nm = (names or {}).get(int(p), "?")
    return f"{nm}"


BODYCARD = ("contour", "plume", "crest")
MOVABLE = ("contour", "plume", "gcovert", "pcovert", "underwing", "alula", "crest", "primary", "secondary", "tertial", "tail")


def _close_pairs(P, part, faces):
    """Near-coplanar overlapping face pairs (the lint's z-fight test) with i in `faces`: [(i, j, n_i * side_of_j, gap)]."""
    from mathutils import kdtree
    n = len(P)
    e1, e2 = P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]
    cr = np.cross(e1, e2)
    area = 0.5 * np.linalg.norm(cr, axis=1)
    longest = np.max(np.stack([np.linalg.norm(e1, axis=1), np.linalg.norm(e2, axis=1), np.linalg.norm(P[:, 2] - P[:, 1], axis=1)]), axis=0)
    deg = (area < 1e-12) | (area < 2e-4 * longest ** 2)
    nrm = cr / np.maximum(2 * area, 1e-20)[:, None]
    cen = P.mean(1)
    kd = kdtree.KDTree(n)
    for i, c in enumerate(cen):
        kd.insert(c, i)
    kd.balance()
    out = []
    for i in faces:
        if deg[i]:
            continue
        js = np.array([j for _, j, _ in kd.find_range(cen[i], max(longest[i], NEAR)) if j != i], int)
        if not len(js):
            continue
        js = js[(part[js] != part[i]) & ~deg[js]]
        js = js[np.abs(nrm[js] @ nrm[i]) > COS] if len(js) else js
        if not len(js):
            continue
        sd = (P[js] - P[i][0]) @ nrm[i]
        sd2 = np.einsum("mk,jk->mj", nrm[js], P[i]) - np.einsum("mk,mk->m", nrm[js], P[js][:, 0])[:, None]
        dmax = np.maximum(np.abs(sd).max(1), np.abs(sd2).max(1))
        tol = ZF * 0.25
        cross = ((sd.max(1) > tol) & (sd.min(1) < -tol)) | ((sd2.max(1) > tol) & (sd2.min(1) < -tol))
        m = (dmax < ZF) & ~cross
        if not m.any():
            continue
        js, dmax, sd = js[m], dmax[m], sd[m]
        ax = e1[i] / (np.linalg.norm(e1[i]) + 1e-20)
        ay = np.cross(nrm[i], ax)
        A = np.stack([(P[i] - P[i][0]) @ ax, (P[i] - P[i][0]) @ ay], 1)
        B = np.stack([(P[js] - P[i][0]) @ ax, (P[js] - P[i][0]) @ ay], 2)
        ov = _overlap2d(A, B)
        for jj in np.where(ov)[0]:
            out.append((i, int(js[jj]), nrm[i] * (1.0 if sd[jj].mean() >= 0 else -1.0), float(dmax[jj])))
    return out


def separate(ob, part_names, iters=40, verbose=True):
    """Lift apart what the lint calls z-fighting: for every near-coplanar overlapping pair, the feather card (whole
    card, along the other face's normal, away from it) moves to 3 x ZF off. Cards only (never the body, bill, eyes,
    legs). A fraction of a mm on a card is invisible; two cards in one plane are not. After the first full pass only
    the faces of moved cards are checked again."""
    me = ob.data
    pp = np.empty(len(me.polygons), int)
    me.attributes["lint_part"].data.foreach_get("value", pp)
    vpart = np.full(len(me.vertices), -1)
    for poly in me.polygons:
        for v in poly.vertices:
            vpart[v] = pp[poly.index]
    names = {p: (part_names or {}).get(p, "?").replace("_R", "") for p in np.unique(pp)}
    me.calc_loop_triangles()
    nt = len(me.loop_triangles)
    vi = np.empty(nt * 3, int)
    me.loop_triangles.foreach_get("vertices", vi)
    vi = vi.reshape(-1, 3)
    pi = np.empty(nt, int)
    me.loop_triangles.foreach_get("polygon_index", pi)
    tpart = pp[pi]
    co = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    faces = range(nt)
    moved = 0
    # the body's axis per x (Blender x = forward): body feathers only ever move outward from it, the later card
    # (anterior / outer) over the earlier, so passes converge instead of trading places
    bodyv = co[vpart == 1] if (vpart == 1).any() else co
    xb = np.linspace(bodyv[:, 0].min(), bodyv[:, 0].max(), 40)
    zc = np.array([bodyv[np.abs(bodyv[:, 0] - x) < (xb[1] - xb[0])][:, 2].mean() if (np.abs(bodyv[:, 0] - x) < (xb[1] - xb[0])).any() else np.nan for x in xb])
    zc = np.where(np.isnan(zc), np.nanmean(zc), zc)
    total = {}
    for it in range(iters):
        push = _close_pairs(co[vi], tpart, faces)
        if not push:
            break
        disp = {}
        for i, j, n_, d in push:
            pi_, pj = int(tpart[i]), int(tpart[j])
            need = ZF * 3.0 - d
            if names[pi_] in BODYCARD and names[pj] in BODYCARD:
                tgt = max(pi_, pj)
                c = co[vi[i]].mean(0)
                out = c - np.array([c[0], 0.0, np.interp(c[0], xb, zc)])
                nn_ = n_ if np.dot(n_, out) >= 0 else -n_
                vec = nn_ * need
                if np.linalg.norm(total.get(tgt, 0.0) + vec) > 0.0025:   # never more than 2.5 mm off its place
                    continue
            elif names[pj] in MOVABLE:
                tgt, vec = pj, n_ * need
            elif names[pi_] in MOVABLE:
                tgt, vec = pi_, -n_ * need
            else:
                continue
            cur = disp.get(tgt)
            if cur is None or np.linalg.norm(vec) > np.linalg.norm(cur):
                disp[tgt] = vec
        if not disp:
            if len(faces) == nt:
                break
            faces = range(nt)
            continue
        for p_, vec in disp.items():
            co[vpart == p_] += vec
            total[p_] = total.get(p_, 0.0) + vec
        # the moved cards' faces again; every 6th pass all faces (a capped pair elsewhere may have a free partner now)
        faces = range(nt) if (it + 1) % 6 == 0 else np.where(np.isin(tpart, list(disp)))[0]
        moved += len(disp)
        if verbose:
            print(f"SEPARATE {ob.name}: pass {it + 1}, {len(push)} pairs, {len(disp)} cards moved")
    me.vertices.foreach_set("co", co.ravel())
    me.update()
    # what is still in one plane after that: body feathers where a short neck crowds many rings into a few mm (they
    # overlap five or more deep there). Leave the later card (a plume first) out: one card fewer in such a pile is
    # not seen, two in one plane are
    left = _close_pairs(co[vi], tpart, range(nt))
    drop = set()
    for i, j, n_, d in left:
        a, b = int(tpart[i]), int(tpart[j])
        if a in drop or b in drop:
            continue
        if names[a] in BODYCARD and names[b] in BODYCARD:
            pa, pb = names[a] == "plume", names[b] == "plume"
            drop.add(a if (pa and not pb) else b if (pb and not pa) else max(a, b))
    if drop:
        import bmesh
        bm = bmesh.new()
        bm.from_mesh(me)
        bm.verts.ensure_lookup_table()
        kill = [bm.verts[k] for k in np.where(np.isin(vpart, list(drop)))[0]]
        bmesh.ops.delete(bm, geom=kill, context="VERTS")
        bm.to_mesh(me)
        bm.free()
        me.update()
        if verbose:
            print(f"SEPARATE {ob.name}: {len(drop)} crowded body feathers left out")
    return moved
