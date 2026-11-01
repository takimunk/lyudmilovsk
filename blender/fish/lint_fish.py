"""Mesh lint for the fish builds (build_fish.py --lint runs it on both LODs inside the same Blender process).

Reported per mesh (counts; 'ok' when all are zero):
    degenerate     zero-area or sliver triangles (aspect longest^2 / 2A above SLIVER)
    uv_zero        triangles whose UV area is below 0.02 texel^2
    uv_outside     UVs leaving the texture rect the face belongs to (body strip / the part's own rect)
    uv_pad         texture rects closer than 4 px
    uv_overlap     triangles whose UVs overlap another face's UVs (the left/right mirror copies are exempt)
    uv_mirror      a mirror-flagged face whose texels are not covered by its right-side original
    uv_density     body faces whose texel density is far from the median (uneven mapping)
    zfight         faces of two different parts that are (anti)parallel within ZF of each other's plane and overlap
    open_edges     boundary edges on the body (must be a closed skin)    nonmanifold  edges used by more than 2 faces
    inverted       body/eye faces whose normal points inwards
    vcol           missing 'Col' attribute or values outside 0..1, R not following the x position, G jumping, B off paired fins
"""
import json
import math

import numpy as np

W, H = 1024, 512
SLIVER = 40.0
COS = math.cos(math.radians(15))


def from_object(ob):
    me = ob.data
    me.calc_loop_triangles()
    n = len(me.loop_triangles)
    co = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    vi = np.empty(n * 3, int)
    me.loop_triangles.foreach_get("vertices", vi)
    li = np.empty(n * 3, int)
    me.loop_triangles.foreach_get("loops", li)
    pi = np.empty(n, int)
    me.loop_triangles.foreach_get("polygon_index", pi)
    uv = np.empty(len(me.loops) * 2)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    d = dict(P=co[vi.reshape(-1, 3)], U=uv[li.reshape(-1, 3)], vi=vi.reshape(-1, 3), co=co)
    for name, key, dt in (("part", "part", int), ("mirror", "mirror", bool)):
        if name in me.attributes:
            a = np.empty(len(me.polygons), dt)
            me.attributes[name].data.foreach_get("value", a)
            d[key] = a[pi]
    if "Col" in me.attributes:
        c = np.empty(len(me.vertices) * 4)
        me.attributes["Col"].data.foreach_get("color", c)
        d["col"] = c.reshape(-1, 4)
    d["meta"] = json.loads(ob["fish_meta"])
    return d


def _uv_area(U):
    return 0.5 * np.abs((U[:, 1, 0] - U[:, 0, 0]) * (U[:, 2, 1] - U[:, 0, 1]) - (U[:, 2, 0] - U[:, 0, 0]) * (U[:, 1, 1] - U[:, 0, 1]))


def _raster(U, scale=2):
    """Pixel-centre rasterisation of uv triangles at `scale` x the texture size; returns list of flat index arrays."""
    out = []
    X = U[..., 0] * W * scale
    Y = (1 - U[..., 1]) * H * scale
    for k in range(len(U)):
        x, y = X[k], Y[k]
        x0, x1 = int(max(math.floor(x.min()), 0)), int(min(math.ceil(x.max()), W * scale - 1))
        y0, y1 = int(max(math.floor(y.min()), 0)), int(min(math.ceil(y.max()), H * scale - 1))
        if x1 < x0 or y1 < y0:
            out.append(np.empty(0, int))
            continue
        gy, gx = np.mgrid[y0:y1 + 1, x0:x1 + 1]
        px, py = gx + 0.5, gy + 0.5
        d = (y[1] - y[2]) * (x[0] - x[2]) + (x[2] - x[1]) * (y[0] - y[2])
        if abs(d) < 1e-12:
            out.append(np.empty(0, int))
            continue
        a = ((y[1] - y[2]) * (px - x[2]) + (x[2] - x[1]) * (py - y[2])) / d
        b = ((y[2] - y[0]) * (px - x[2]) + (x[0] - x[2]) * (py - y[2])) / d
        c = 1 - a - b
        eps = 0.02
        ins = (a > eps) & (b > eps) & (c > eps)
        out.append((gy[ins] * W * scale + gx[ins]).astype(int))
    return out


def _overlap2d(a, b, shrink=0.03):
    a = a + (a.mean(0) - a) * shrink
    b = b + (b.mean(1, keepdims=True) - b) * shrink
    ok = np.ones(len(b), bool)

    def axes(t):
        e = np.roll(t, -1, axis=-2) - t
        return np.stack([-e[..., 1], e[..., 0]], -1)
    for ax in np.concatenate([np.broadcast_to(axes(a), (len(b), 3, 2)), axes(b)], axis=1).transpose(1, 0, 2):
        pa = np.einsum("nk,jk->nj", ax, a)
        pb = np.einsum("nk,njk->nj", ax, b)
        sep = (pa.max(1) < pb.min(1)) | (pb.max(1) < pa.min(1))
        ok &= ~sep
    return ok


def lint(d, label, verbose=True):
    P, U = d["P"], d["U"]
    meta = d["meta"]
    L = meta["length"]
    n = len(P)
    part = d.get("part", np.zeros(n, int))
    mirror = d.get("mirror", np.zeros(n, bool))
    kinds = meta["part_kinds"]               # part id -> kind
    res = {}

    e1, e2 = P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]
    cr = np.cross(e1, e2)
    area = 0.5 * np.linalg.norm(cr, axis=1)
    nrm = cr / np.maximum(np.linalg.norm(cr, axis=1), 1e-30)[:, None]
    edges = np.stack([np.linalg.norm(P[:, 1] - P[:, 0], axis=1), np.linalg.norm(P[:, 2] - P[:, 1], axis=1), np.linalg.norm(P[:, 0] - P[:, 2], axis=1)], 1)
    aspect = edges.max(1) ** 2 / np.maximum(2 * area, 1e-30)
    badtri = (area < 1e-10) | (aspect > SLIVER)
    res["degenerate"] = int(badtri.sum())
    d["deg_at"] = [(kinds[str(part[k])], int(part[k]), round(float(aspect[k]), 1), tuple(np.round(P[k].mean(0), 4))) for k in np.where(badtri)[0][:6]]

    ua = _uv_area(U)
    res["uv_zero"] = int((ua * W * H < 0.02).sum())

    # uv regions
    out = 0
    px = U[..., 0] * W
    py = (1 - U[..., 1]) * H
    rects = meta["rects"]
    for k in range(n):
        kind = kinds[str(part[k])]
        if kind == "body":
            y1 = meta["body_h"]
            bad = (px[k].min() < 0.99) or (px[k].max() > W - 0.99) or (py[k].min() < 0.99) or (py[k].max() > y1 + 0.01)
        else:
            x0, y0, w, h = rects[meta["part_rect"][str(part[k])]]
            bad = (px[k].min() < x0 - 0.01) or (px[k].max() > x0 + w + 0.01) or (py[k].min() < y0 - 0.01) or (py[k].max() > y0 + h + 0.01)
        out += int(bad)
    res["uv_outside"] = out
    rl = list(rects.values())
    pad = 0
    for i in range(len(rl)):
        for j in range(i + 1, len(rl)):
            a, b = rl[i], rl[j]
            gx = max(b[0] - (a[0] + a[2]), a[0] - (b[0] + b[2]))
            gy = max(b[1] - (a[1] + a[3]), a[1] - (b[1] + b[3]))
            if max(gx, gy) < 4:
                pad += 1
    res["uv_pad"] = pad

    # overlap / mirror
    real = np.where(~mirror)[0]
    own = np.full(W * 2 * H * 2, -1, np.int32)
    over = set()
    rr = _raster(U[real])
    for idx, px_ in zip(real, rr):
        prev = own[px_]
        hit = prev >= 0
        if hit.sum() > 3:
            over.add(int(idx))
            over.update(int(v) for v in np.unique(prev[hit]))
        own[px_] = np.where(prev < 0, idx, prev)
    res["uv_overlap"] = len(over)
    d["over_parts"] = sorted(set(int(part[i]) for i in over))
    mi = np.where(mirror)[0]
    bad = 0
    for idx, px_ in zip(mi, _raster(U[mi])):
        if len(px_) and (own[px_] < 0).mean() > 0.12:
            bad += 1
    res["uv_mirror"] = bad

    # texel density on the body
    tmean = 0.5 - P[:, :, 0].mean(1) / L
    bodyf = np.where((part == 0) & (tmean > 0.03) & (tmean < meta["tend"] - 0.03))[0]
    dens = np.sqrt(np.maximum(ua[bodyf] * W * H, 1e-9) / np.maximum(area[bodyf], 1e-12))
    med = np.median(dens)
    res["uv_density"] = int(((dens < med / 2.6) | (dens > med * 2.6)).sum())

    # z-fighting
    ZF = max(0.00004, 0.0004 * L)
    cen = P.mean(1)
    rad = np.linalg.norm(P - cen[:, None], axis=2).max(1)
    dpl = np.einsum("ij,ij->i", nrm, cen)
    zf = 0
    zf_at = []
    for i in range(n):
        near = np.where((part != part[i]) & (np.linalg.norm(cen - cen[i], axis=1) < rad + rad[i]))[0]
        near = near[near > i]
        if len(near) == 0:
            continue
        par = np.abs(nrm[near] @ nrm[i]) > COS
        near = near[par]
        if len(near) == 0:
            continue
        dA = np.abs(np.einsum("j,nkj->nk", nrm[i], P[near]) - dpl[i]).max(1)       # B verts to A plane
        dB = np.abs(np.einsum("nj,nkj->nk", nrm[near], P[i][None]) - dpl[near][:, None]).max(1)  # A verts to B planes
        close = near[(dA < ZF) & (dB < ZF)]
        if len(close) == 0:
            continue
        # project to A's plane and test overlap
        ax = np.cross(nrm[i], [0.3, 0.5, 0.8])
        ax /= np.linalg.norm(ax)
        ay = np.cross(nrm[i], ax)
        A2 = np.stack([P[i] @ ax, P[i] @ ay], 1)
        B2 = np.stack([P[close] @ ax, P[close] @ ay], -1)
        if _overlap2d(A2, B2).any():
            zf += 1
            zf_at.append(cen[i])
    res["zfight"] = zf
    d["zf_at"] = zf_at

    # manifold
    key = np.round(d["co"] * 1e6).astype(np.int64)
    uniq, inv = np.unique(key, axis=0, return_inverse=True)
    inv = inv.reshape(-1)
    wv = inv[d["vi"]]
    cnt = {}
    for k in range(n):
        if part[k] >= 0:
            a, b, c = wv[k]
            for e in ((a, b), (b, c), (c, a)):
                kk = (min(e), max(e))
                cnt.setdefault(kk, []).append(k)
    openb = nm = 0
    for e, fs in cnt.items():
        if len(fs) > 2:
            nm += 1
        elif len(fs) == 1 and kinds[str(part[fs[0]])] == "body":
            openb += 1
    res["open_edges"] = openb
    res["nonmanifold"] = nm

    # normals outward
    inv_ct = 0
    for pid, kind in kinds.items():
        pid = int(pid)
        sel = np.where(part == pid)[0]
        if len(sel) == 0 or kind not in ("body", "eye", "extra"):
            continue
        if kind == "body":
            ref = np.stack([cen[sel, 0], np.zeros(len(sel)), np.zeros(len(sel))], 1)
        else:
            ref = np.broadcast_to(cen[sel].mean(0), (len(sel), 3))
            if kind == "eye":
                ref = ref - 0.0
        dd = np.einsum("ij,ij->i", nrm[sel], cen[sel] - ref)
        inv_ct += int((dd < 0).sum()) if (dd < 0).sum() < 0.5 * len(sel) else len(sel)
    res["inverted"] = inv_ct

    # vertex colours
    vc = 0
    vcs = []
    col = d.get("col")
    if col is None:
        vc += 1; vcs.append("missing")
    else:
        co = d["co"]
        if (col < -1e-6).any() or (col > 1 + 1e-6).any() or not np.isfinite(col).all():
            vc += 1; vcs.append("range")
        if np.abs(col[:, 0] - (0.5 - co[:, 0] / L)).max() > 0.012:
            vc += 1; vcs.append("R")
        # G smooth over triangle edges
        gv = col[:, 1]
        bodyfaces = part == 0
        for a, b in ((0, 1), (1, 2), (2, 0)):
            if np.abs(gv[d["vi"][bodyfaces, a]] - gv[d["vi"][bodyfaces, b]]).max() > 0.2:
                vc += 1; vcs.append("G")
                break
        paired_ids = [int(k) for k, v in meta["part_paired"].items() if v]
        isp = np.zeros(len(co), bool)
        for k in range(n):
            if part[k] in paired_ids:
                isp[d["vi"][k]] = True
        if (np.abs(col[isp, 2] - 1) > 1e-6).any() or (np.abs(col[~isp, 2]) > 1e-6).any():
            vc += 1; vcs.append("B")
    res["vcol"] = vc
    d["vcol_tags"] = vcs

    bad = {k: v for k, v in res.items() if v}
    line = "LINT %s ok" % label if not bad else "LINT %s %s" % (label, " ".join("%s %d" % kv for kv in bad.items()))
    if verbose:
        print(line)
        if zf_at:
            print("   zfight near", [tuple(np.round(p, 4)) for p in zf_at[:5]])
        if d.get("deg_at") and res["degenerate"]:
            print("   degenerate:", d["deg_at"])
        if res["uv_overlap"]:
            print("   uv_overlap parts:", d["over_parts"])
        if d.get("vcol_tags"):
            print("   vcol:", d["vcol_tags"])
    return res, line
