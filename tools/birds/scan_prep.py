"""Turns a hi3d bird model (wings raised in a V) into the body data build_bird.py --scan reads: the body's shape and
colour as cross-sections along the bird's own axis, plus beak and leg colours.

    Blender -b --python tools/birds/scan_prep.py -- <Id> [--glb path] [--debug]

In:  blender/birds/scan_raw/<Id>/model.glb (git-ignored; URL and sha256 in blender/birds/scans/<Id>/hi3d.json)
     blender/birds/scans/overrides.json  {"<Id>": {"flip": true, "lateral": "x", "beak_base": 0.83, "body_end": 0.21}}
Out: blender/birds/scans/<Id>/scan.npz   (committed: small)
     blender/birds/scan_raw/<Id>/scan_debug.png

The model is turned so +F (forward) is the beak, +U up, +S the bird's left. Its mid-sagittal section (the plane of
symmetry: the raised wings and the legs stand off it) gives top and bottom outlines along F; the centreline is the
middle of each vertical chord, smoothed. Landmarks along F: the beak tip, the beak base (where the chord shrinks
from the head's depth to the bill's), the body end (where the thick body gives way to the thin tail plate).

Body sample: NU stations from body end to beak base (u = 0..1, plus a little beyond each end), at each station NT
angles th (0 = the bird's left, pi/2 = up) in the plane perpendicular to F: a ray from the centreline outward, first
hit = radius and colour. Rays that hit the wing roots or the thighs come out too long: each station's radius ring is
fitted with a low-order Fourier series by iteratively down-weighting long outliers, and points far above the fit are
replaced by it (colour taken from the nearest good angle). Everything is in units of L = the body-end-to-beak-base
distance, so build_bird.py can scale it to the species.
"""
import json
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
SID = argv[0]
GLB = argv[argv.index("--glb") + 1] if "--glb" in argv else os.path.join(ROOT, "blender", "birds", "scan_raw", SID, "model.glb")
DEBUG = "--debug" in argv
OVR = {}
_op = os.path.join(ROOT, "blender", "birds", "scans", "overrides.json")
if os.path.exists(_op):
    OVR = json.load(open(_op)).get(SID, {})

NU, NT = 112, 96          # stations along the body, angles round it
NUH, NTH = 256, 384       # the body texture (colour only)
U0, U1 = -0.06, 1.04      # sampled u range (a little past body end and beak base)


def load():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=GLB)
    obs = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in obs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = obs[0]
    if len(obs) > 1:
        bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    me = ob.data
    me.calc_loop_triangles()
    V = np.zeros(len(me.vertices) * 3)
    me.vertices.foreach_get("co", V)
    V = V.reshape(-1, 3)
    T = np.zeros(len(me.loop_triangles) * 3, int)
    me.loop_triangles.foreach_get("vertices", T)
    T = T.reshape(-1, 3)
    TL = np.zeros(len(me.loop_triangles) * 3, int)
    me.loop_triangles.foreach_get("loops", TL)
    TL = TL.reshape(-1, 3)
    uv = np.zeros(len(me.loops) * 2)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    img = None
    for m in me.materials:
        if m and m.use_nodes:
            for n in m.node_tree.nodes:
                if n.type == "BSDF_PRINCIPLED":
                    lk = n.inputs["Base Color"].links
                    if lk and lk[0].from_node.type == "TEX_IMAGE":
                        img = lk[0].from_node.image
    if img is None:
        raise SystemExit("no base colour texture in " + GLB)
    w, h = img.size
    px = np.zeros(w * h * 4, np.float32)
    img.pixels.foreach_get(px)
    px = px.reshape(h, w, 4)[..., :3]   # Blender stores linear floats for sRGB images? keep as read (see colour())
    srgb = img.colorspace_settings.name.lower().startswith("srgb")
    print("SCAN", SID, "verts", len(V), "tris", len(T), "tex", w, h, img.colorspace_settings.name)
    return V, T, TL, uv, px, srgb


def mirror_err(V, sub, axis, c):
    m = sub.copy()
    m[:, axis] = 2 * c[axis] - m[:, axis]
    # nearest-neighbour distance by a voxel hash (no scipy in Blender's python)
    step = (V.max(0) - V.min(0)).max() / 120
    key = np.floor((V - V.min(0)) / step).astype(int)
    grid = {}
    for i, k in enumerate(map(tuple, key)):
        grid.setdefault(k, []).append(i)
    err = []
    for p in m:
        k = np.floor((p - V.min(0)) / step).astype(int)
        cand = []
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                for dz in (-1, 0, 1):
                    cand += grid.get((k[0] + dx, k[1] + dy, k[2] + dz), [])
        err.append(np.min(np.linalg.norm(V[cand] - p, axis=1)) if cand else 3 * step)
    return float(np.median(err))


def orient(V):
    c = (V.min(0) + V.max(0)) / 2
    rng = np.random.default_rng(0)
    sub = V[rng.choice(len(V), min(1500, len(V)), replace=False)]
    lat = {"x": 0, "y": 1}.get(OVR.get("lateral", ""), None)
    if lat is None:
        ex, ey = mirror_err(V, sub, 0, c), mirror_err(V, sub, 1, c)
        lat = 0 if ex < ey else 1
        print("SCAN mirror error x %.4f y %.4f -> lateral %s" % (ex, ey, "xy"[lat]))
    fwd = 1 - lat
    # the symmetry plane: refine its offset by the median of the mirrored match (use bbox centre: good enough)
    s0 = c[lat]
    near = V[np.abs(V[:, lat] - s0) < 0.06 * (V[:, lat].max() - V[:, lat].min())]
    f = near[:, fwd]
    lo, hi = np.quantile(f, 0.01), np.quantile(f, 0.99)
    z_lo = near[f <= lo, 2].mean()
    z_hi = near[f >= hi, 2].mean()
    sign = 1.0 if z_hi > z_lo else -1.0
    if OVR.get("flip"):
        sign = -sign
    # F, U, S coordinates (S = bird's left: right-handed F x S = U  ->  S = U x F)
    F = np.zeros(3)
    F[fwd] = sign
    U = np.array([0, 0, 1.0])
    S = np.cross(U, F)
    P = V - np.array([c[0] if lat == 0 else 0, c[1] if lat == 1 else 0, 0])
    out = np.stack([P @ F, P @ U, (V - c) @ S], 1)   # (f, u, s) with s = 0 on the symmetry plane
    return out, (F, U, S, c)


def section(Pf, T):
    """Segments where the triangles cross s = 0, as (f, u) pairs."""
    s = Pf[:, 2]
    segs = []
    for tri in T:
        ss = s[tri]
        if (ss > 0).all() or (ss < 0).all():
            continue
        pts = []
        for a, b in ((0, 1), (1, 2), (2, 0)):
            sa, sb = ss[a], ss[b]
            if (sa > 0) != (sb > 0):
                t = sa / (sa - sb)
                pa, pb = Pf[tri[a], :2], Pf[tri[b], :2]
                pts.append(pa + t * (pb - pa))
        if len(pts) == 2:
            segs.append(pts)
    return np.array(segs)


def chords(segs, n=600):
    f0, f1 = segs[..., 0].min(), segs[..., 0].max()
    fs = np.linspace(f0, f1, n)
    top, bot = np.full(n, np.nan), np.full(n, np.nan)
    a, b = segs[:, 0], segs[:, 1]
    for i, f in enumerate(fs):
        m = (np.minimum(a[:, 0], b[:, 0]) <= f) & (np.maximum(a[:, 0], b[:, 0]) >= f) & (np.abs(a[:, 0] - b[:, 0]) > 1e-9)
        if not m.any():
            continue
        t = (f - a[m, 0]) / (b[m, 0] - a[m, 0])
        u = a[m, 1] + t * (b[m, 1] - a[m, 1])
        top[i], bot[i] = u.max(), u.min()
    ok = ~np.isnan(top)
    return fs[ok], top[ok], bot[ok]


def smooth1(x, k):
    if k < 2:
        return x
    w = np.hanning(2 * k + 1)
    w /= w.sum()
    xp = np.pad(x, k, mode="edge")
    return np.convolve(xp, w, mode="valid")


def landmarks(fs, top, bot):
    th = top - bot
    n = len(fs)
    L = fs[-1] - fs[0]
    tip = fs[-1]
    # head depth: the deepest chord in the front 30 %
    front = fs > fs[-1] - 0.30 * L
    hd = th[front].max()
    # beak base: walking back from the tip, the first chord deeper than 62 % of the head
    i = n - 1
    while i > 0 and th[i] < 0.62 * hd:
        i -= 1
    beak_base = fs[i]
    # body end: walking forward from the tail tip, the first chord deeper than 45 % of the deepest body chord
    bd = th.max()
    j = 0
    while j < n - 1 and th[j] < 0.45 * bd:
        j += 1
    body_end = fs[j]
    if "beak_base" in OVR:
        beak_base = fs[0] + OVR["beak_base"] * L
    if "body_end" in OVR:
        body_end = fs[0] + OVR["body_end"] * L
    return dict(tip=tip, tail_tip=fs[0], beak_base=beak_base, body_end=body_end, head_depth=hd, body_depth=bd)


def main():
    global PX
    V, T, TL, uv, px, srgb = load()
    PX = px
    P, (F, U, S, c) = orient(V)
    # level the body: the model stands nose-up; rotate about S so the body's centreline (body end .. 55 % toward the
    # beak base) is horizontal, then slice again (stations must cut the body square, not obliquely)
    pitch_total = 0.0
    for _ in range(2):
        segs = section(P, T)
        fs, top, bot = chords(segs)
        k = max(2, len(fs) // 120)
        top_s, bot_s = smooth1(top, k), smooth1(bot, k)
        mid = (top_s + bot_s) / 2
        lm = landmarks(fs, top, bot)
        L = lm["beak_base"] - lm["body_end"]
        m = (fs >= lm["body_end"]) & (fs <= lm["body_end"] + 0.55 * L)
        pitch = math.atan(np.polyfit(fs[m], mid[m], 1)[0]) + OVR.get("pitch_deg", 0.0) * math.pi / 180 * (pitch_total == 0)
        if abs(pitch) < 0.01:
            break
        cp, sp = math.cos(-pitch), math.sin(-pitch)
        f0, u0 = P[:, 0].copy(), P[:, 1].copy()
        P[:, 0], P[:, 1] = f0 * cp - u0 * sp, f0 * sp + u0 * cp
        pitch_total += pitch
    print("SCAN levelled by %.1f deg" % math.degrees(pitch_total))
    print("SCAN landmarks", {k2: round(float(v), 4) for k2, v in lm.items()}, "L", round(float(L), 4))

    # BVH in (f, u, s) space
    bvh = BVHTree.FromPolygons([tuple(p) for p in P], [tuple(t) for t in T])
    def sample(nu, nt):
        us = np.linspace(U0, U1, nu)
        ths = np.linspace(0, 2 * math.pi, nt, endpoint=False)
        R = np.zeros((nu, nt))
        COL = np.zeros((nu, nt, 3))
        CEN = np.zeros(nu)
        H, W = px.shape[:2]
        # stations along the curved centreline (arc length from body end to beak base), each cut square to it
        cf = np.linspace(lm["body_end"] - 0.1 * L, lm["beak_base"] + 0.06 * L, 400)
        cu = smooth1(np.interp(cf, fs, mid), 6)
        seg = np.hypot(np.diff(cf), np.diff(cu))
        arc = np.r_[0, np.cumsum(seg)]
        a0 = float(np.interp(lm["body_end"], cf, arc))
        a1 = float(np.interp(lm["beak_base"], cf, arc))
        for i, u in enumerate(us):
            a = a0 + u * (a1 - a0)
            f = float(np.interp(a, arc, cf))
            cuv = float(np.interp(a, arc, cu))
            k = int(np.clip(np.searchsorted(arc, a), 1, len(arc) - 1))
            tf, tu = cf[k] - cf[k - 1], cu[k] - cu[k - 1]
            tn = math.hypot(tf, tu) or 1.0
            nf, nu = -tu / tn, tf / tn          # the normal in the sagittal plane, pointing up
            CEN[i] = cuv
            o = Vector((f, cuv, 0.0))
            for j, t in enumerate(ths):
                d = Vector((nf * math.sin(t), nu * math.sin(t), math.cos(t)))
                loc, nor, idx, dist = bvh.ray_cast(o, d, 4 * L)
                if loc is None:
                    R[i, j] = np.nan
                    continue
                R[i, j] = dist
                # barycentric colour lookup
                tri = T[idx]
                a, b, cc = (Vector(P[v]) for v in tri)
                v0, v1, v2 = b - a, cc - a, loc - a
                d00, d01, d11, d20, d21 = v0.dot(v0), v0.dot(v1), v1.dot(v1), v2.dot(v0), v2.dot(v1)
                den = d00 * d11 - d01 * d01 or 1e-12
                wb = (d11 * d20 - d01 * d21) / den
                wc = (d00 * d21 - d01 * d20) / den
                wa = 1 - wb - wc
                q = wa * uv[TL[idx][0]] + wb * uv[TL[idx][1]] + wc * uv[TL[idx][2]]
                x = int(np.clip(q[0] % 1.0 * W, 0, W - 1))
                y = int(np.clip(q[1] % 1.0 * H, 0, H - 1))
                COL[i, j] = px[y, x]
        return us, ths, R, COL, CEN
    us, ths, R, COL, CEN = sample(NU, NT)
    # the photographic body texture at a higher resolution (u along, th round), for build_bird.py --scan B
    _, _, RH, COLH, _ = sample(NUH, NTH)
    # outliers (wing roots, thighs): Fourier fit per station, long rays replaced
    bad = np.isnan(R)
    nf = 3
    A = np.stack([np.ones(NT)] + [g(m * ths) for m in range(1, nf + 1) for g in (np.cos, np.sin)], 1)
    for i in range(NU):
        r = R[i].copy()
        w = (~bad[i]).astype(float)
        r[bad[i]] = np.nanmedian(R[i]) if (~bad[i]).any() else 0.0
        for _ in range(6):
            coef = np.linalg.lstsq(A * w[:, None], r * w, rcond=None)[0]
            fit = A @ coef
            res = (r - fit) / max(np.median(fit), 1e-6)
            w = np.where(res > 0.12, 0.05, 1.0) * (~bad[i])
        out = bad[i] | (res > 0.18)
        R[i, out] = fit[out]
        if out.any() and (~out).any():
            good = np.nonzero(~out)[0]
            for j in np.nonzero(out)[0]:
                dj = np.abs(((good - j + NT // 2) % NT) - NT // 2)
                COL[i, j] = COL[i, good[np.argmin(dj)]]
        bad[i] = out
    # smooth the radius map a little along u (stations) so scan noise does not stand feathers up
    for j in range(NT):
        R[:, j] = smooth1(R[:, j], 1)

    # the texture's wing-root and thigh rays: longer than the cleaned radius there -> colour from the nearest good angle
    ush = np.linspace(U0, U1, NUH)
    thh = np.linspace(0, 2 * math.pi, NTH, endpoint=False)
    for i in range(NUH):
        fi = np.interp(ush[i], us, np.arange(NU))
        i0 = int(min(fi, NU - 2))
        a_ = fi - i0
        rfit = np.interp(thh, np.r_[ths, 2 * math.pi], np.r_[R[i0], R[i0][0]]) * (1 - a_) + np.interp(thh, np.r_[ths, 2 * math.pi], np.r_[R[i0 + 1], R[i0 + 1][0]]) * a_
        badh = np.isnan(RH[i]) | (RH[i] > rfit * 1.10)
        if badh.any() and (~badh).any():
            good = np.nonzero(~badh)[0]
            for j in np.nonzero(badh)[0]:
                dj = np.abs(((good - j + NTH // 2) % NTH) - NTH // 2)
                COLH[i, j] = COLH[i, good[np.argmin(dj)]]
    # wider mask: low-res outlier cells (wing roots, thighs) grown by a cell along and two round, mapped to the texture
    badw = bad.copy()
    for _ in range(2):
        badw = badw | np.roll(badw, 1, 1) | np.roll(badw, -1, 1)
    badw = badw | np.roll(badw, 1, 0) | np.roll(badw, -1, 0)
    for i in range(NUH):
        i0 = int(round(np.interp(ush[i], us, np.arange(NU))))
        row = badw[i0][(np.arange(NTH) * NT // NTH)]
        if row.any() and (~row).any():
            good = np.nonzero(~row)[0]
            for j in np.nonzero(row)[0]:
                dj = np.abs(((good - j + NTH // 2) % NTH) - NTH // 2)
                COLH[i, j] = COLH[i, good[np.argmin(dj)]]
    # birds are symmetric: average each side with its mirror (th -> pi - th), which also halves one-sided defects
    mj = (NTH // 2 - np.arange(NTH)) % NTH
    COLH = 0.5 * (COLH + COLH[:, mj])
    # the centreline, normalised (u = arc parameter, f/up relative to the body end / the mid-body centre), for the rig
    cf = np.linspace(lm["body_end"] - 0.1 * L, lm["beak_base"] + 0.06 * L, 400)
    cu = smooth1(np.interp(cf, fs, mid), 6)
    arc = np.r_[0, np.cumsum(np.hypot(np.diff(cf), np.diff(cu)))]
    a0, a1 = float(np.interp(lm["body_end"], cf, arc)), float(np.interp(lm["beak_base"], cf, arc))
    up_ref = float(np.interp(lm["body_end"] + 0.45 * L, fs, mid))
    tf, tu = np.gradient(cf), np.gradient(cu)
    tn = np.hypot(tf, tu)
    tf, tu = tf / tn, tu / tn
    CL = dict(cl_u=(arc - a0) / (a1 - a0), cl_f=(cf - lm["body_end"]) / L, cl_up=(cu - up_ref) / L, cl_tf=tf, cl_tu=tu,
              arc_per_u=(a1 - a0) / L)
    out = dict(us=us, ths=ths, R=R / L, CEN=(CEN - np.interp(lm["body_end"] + 0.45 * L, fs, mid)) / L, COL=COL.astype(np.float32), COLH=(np.clip(COLH, 0, 1) * 255).astype(np.uint8), RH=RH.astype(np.float32),
               BAD=bad, srgb=srgb,
               beak_len=(lm["tip"] - lm["beak_base"]) / L, head_depth=lm["head_depth"] / L, body_depth=lm["body_depth"] / L,
               tail_len=(lm["body_end"] - lm["tail_tip"]) / L,
               sect_f=(fs - lm["body_end"]) / L, sect_top=(top_s - np.interp(lm["body_end"] + 0.45 * L, fs, mid)) / L,
               sect_bot=(bot_s - np.interp(lm["body_end"] + 0.45 * L, fs, mid)) / L)
    d = os.path.join(ROOT, "blender", "birds", "scans", SID)
    os.makedirs(d, exist_ok=True)
    np.savez_compressed(os.path.join(d, "scan.npz"), **out, **CL)
    json.dump({k2: float(v) for k2, v in lm.items()} | {"L": float(L), "outlier_share": float(bad.mean()),
               "forward": F.tolist(), "srgb": bool(srgb)}, open(os.path.join(d, "scan_meta.json"), "w"), indent=1)
    print("SCAN saved", os.path.join(d, "scan.npz"), "outliers %.1f%%" % (100 * bad.mean()))
    debug_png(out, os.path.join(ROOT, "blender", "birds", "scan_raw", SID, "scan_debug.png"))
    if "--no-body" not in argv:
        cut_body(P, T, TL, uv, cf, cu, (arc - a0) / (a1 - a0), tf, tu, us, ths, R, lm, L, up_ref, badw)



def save_mesh_npz(P, T, TL, uv, kf, lm, L, up_ref, name, tgt):
    F = T[kf]
    me = bpy.data.meshes.new(name)
    me.vertices.add(len(P))
    me.vertices.foreach_set("co", P.astype(np.float32).ravel())
    me.loops.add(len(F) * 3)
    me.loops.foreach_set("vertex_index", F.ravel().astype(np.int32))
    me.polygons.add(len(F))
    me.polygons.foreach_set("loop_start", np.arange(0, len(F) * 3, 3, dtype=np.int32))
    me.polygons.foreach_set("loop_total", np.full(len(F), 3, dtype=np.int32))
    ul = me.uv_layers.new(name="UVMap")
    ul.data.foreach_set("uv", uv[TL[kf]].reshape(-1).astype(np.float32))
    me.update()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    mdf = ob.modifiers.new("dec", "DECIMATE")
    mdf.ratio = min(1.0, tgt / max(len(F), 1))
    dg = bpy.context.evaluated_depsgraph_get()
    m2 = ob.evaluated_get(dg).to_mesh()
    m2.calc_loop_triangles()
    co = np.zeros(len(m2.vertices) * 3)
    m2.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    tri = np.zeros(len(m2.loop_triangles) * 3, int)
    m2.loop_triangles.foreach_get("vertices", tri)
    tl = np.zeros(len(m2.loop_triangles) * 3, int)
    m2.loop_triangles.foreach_get("loops", tl)
    luv = np.zeros(len(m2.loops) * 2)
    m2.uv_layers.active.data.foreach_get("uv", luv)
    luv = luv.reshape(-1, 2)
    vn = np.stack([(co[:, 0] - lm["body_end"]) / L, (co[:, 1] - up_ref) / L, co[:, 2] / L], 1)
    np.savez_compressed(os.path.join(ROOT, "blender", "birds", "scan_raw", SID, name), v=vn.astype(np.float32),
                        f=tri.reshape(-1, 3).astype(np.int32), uv=luv[tl].reshape(-1, 3, 2).astype(np.float32))
    ob.evaluated_get(dg).to_mesh_clear()
    bpy.data.objects.remove(ob)
    bpy.data.meshes.remove(me)
    print("SCAN saved", name, len(tri) // 3, "tris")


def cut_body(P, T, TL, uv, cf, cu, ua, tf, tu, us, ths, R, lm, L, up_ref, badw):
    """The scan's body for build_bird.py --scan S: wings (outside the cleaned radius at the wing roots), legs (below
    it) and the tail plate (behind the body end) cut off, holes filled, decimated to the LOD budgets.
    Out: blender/birds/scan_raw/<Id>/body.npz (per LOD: verts in body-length units, f/up/s relative to the body end
    and the mid-body centre; faces; loop uvs) and body_tex.png (the base colour, 2048)."""
    n = len(P)
    uvv = np.zeros(n)
    nc = np.zeros(n)
    for a in range(0, n, 40000):
        q = P[a:a + 40000, :2]
        d2 = (q[:, None, 0] - cf[None]) ** 2 + (q[:, None, 1] - cu[None]) ** 2
        k = np.argmin(d2, 1)
        uvv[a:a + 40000] = ua[k]
        nc[a:a + 40000] = (q[:, 0] - cf[k]) * (-tu[k]) + (q[:, 1] - cu[k]) * tf[k]
    th = np.arctan2(nc, P[:, 2]) % (2 * math.pi)
    r = np.hypot(nc, P[:, 2])
    nt = len(ths)
    fu = np.clip(np.interp(np.clip(uvv, us[0], us[-1]), us, np.arange(len(us))), 0, len(us) - 1.001)
    i0, au = fu.astype(int), fu - fu.astype(int)
    ft = th / (2 * math.pi) * nt
    j0, bt = ft.astype(int) % nt, ft - ft.astype(int)
    j1 = (j0 + 1) % nt
    rfit = (R[i0, j0] * (1 - bt) + R[i0, j1] * bt) * (1 - au) + (R[i0 + 1, j0] * (1 - bt) + R[i0 + 1, j1] * bt) * au
    # where the wing roots and thighs were (the outlier cells, grown): cut at the cleaned radius itself
    rb = badw[np.clip(np.rint(fu).astype(int), 0, len(us) - 1), np.rint(ft).astype(int) % nt]
    margin = np.where(rb, OVR.get("root_margin", 0.985), OVR.get("cut_margin", 1.04))
    keep = (uvv >= OVR.get("tail_cut", -0.02)) & ((uvv > 1.0) | (r <= rfit * margin))
    # the whole scan, wings and all, decimated (raw-scan previews for the comparison sheets)
    if "--no-full" not in argv:
        save_mesh_npz(P, T, TL, uv, np.ones(len(T), bool), lm, L, up_ref, "full.npz", 300000)
    kf = keep[T].all(1)
    F = T[kf]
    print("SCAN body cut: kept %.1f%% of triangles" % (100 * kf.mean()))
    me = bpy.data.meshes.new("scanbody")
    me.vertices.add(n)
    me.vertices.foreach_set("co", P.astype(np.float32).ravel())
    me.loops.add(len(F) * 3)
    me.loops.foreach_set("vertex_index", F.ravel().astype(np.int32))
    me.polygons.add(len(F))
    me.polygons.foreach_set("loop_start", np.arange(0, len(F) * 3, 3, dtype=np.int32))
    me.polygons.foreach_set("loop_total", np.full(len(F), 3, dtype=np.int32))
    ul = me.uv_layers.new(name="UVMap")
    ul.data.foreach_set("uv", uv[TL[kf]].reshape(-1).astype(np.float32))
    me.update()
    me.validate()
    ob = bpy.data.objects.new("scanbody", me)
    bpy.context.scene.collection.objects.link(ob)
    for o in list(bpy.context.scene.objects):
        o.select_set(False)
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.delete_loose()
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.remove_doubles(threshold=L * 0.0005)
    # keep the biggest connected piece only (stray wing/leg crumbs), then close the cuts
    bpy.ops.mesh.select_all(action="DESELECT")
    bpy.ops.object.mode_set(mode="OBJECT")
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(me)
    seen, best = set(), None
    for f0 in bm.faces:
        if f0.index in seen:
            continue
        stack, comp = [f0], []
        seen.add(f0.index)
        while stack:
            f = stack.pop()
            comp.append(f)
            for e in f.edges:
                for g in e.link_faces:
                    if g.index not in seen:
                        seen.add(g.index)
                        stack.append(g)
        if best is None or len(comp) > len(best):
            best = comp
    bs = set(f.index for f in best)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.index not in bs], context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    holes = bmesh.ops.holes_fill(bm, edges=bm.edges[:], sides=0)
    tri = bmesh.ops.triangulate(bm, faces=holes["faces"])
    # the fill faces have no uvs of their own: each corner takes the uv its vertex has in the scan's own faces
    ul = bm.loops.layers.uv.active
    newf = set(tri["faces"]) | set(holes["faces"])
    for f in newf:
        if not f.is_valid:
            continue
        for lp in f.loops:
            for o in lp.vert.link_loops:
                if o.face not in newf:
                    lp[ul].uv = o[ul].uv
                    break
    bm.to_mesh(me)
    bm.free()
    me.update()
    print("SCAN body after cleanup: tris", len(me.polygons))
    out = {}
    full = ob
    for lod, tgt in ((0, OVR.get("tris0", 20000)), (1, 8000), (2, 3000), (3, 1200)):
        o2 = full.copy()
        o2.data = full.data.copy()
        bpy.context.scene.collection.objects.link(o2)
        mdf = o2.modifiers.new("dec", "DECIMATE")
        mdf.ratio = min(1.0, tgt / max(len(full.data.polygons), 1))
        mdf.use_collapse_triangulate = True
        dg = bpy.context.evaluated_depsgraph_get()
        m2 = o2.evaluated_get(dg).to_mesh()
        m2.calc_loop_triangles()
        nv = len(m2.vertices)
        co = np.zeros(nv * 3)
        m2.vertices.foreach_get("co", co)
        co = co.reshape(-1, 3)
        tri = np.zeros(len(m2.loop_triangles) * 3, int)
        m2.loop_triangles.foreach_get("vertices", tri)
        tl = np.zeros(len(m2.loop_triangles) * 3, int)
        m2.loop_triangles.foreach_get("loops", tl)
        luv = np.zeros(len(m2.loops) * 2)
        m2.uv_layers.active.data.foreach_get("uv", luv)
        luv = luv.reshape(-1, 2)
        vn = np.stack([(co[:, 0] - lm["body_end"]) / L, (co[:, 1] - up_ref) / L, co[:, 2] / L], 1)
        out[f"v{lod}"] = vn.astype(np.float32)
        out[f"f{lod}"] = tri.reshape(-1, 3).astype(np.int32)
        out[f"uv{lod}"] = luv[tl].reshape(-1, 3, 2).astype(np.float32)
        print("SCAN body LOD%d tris %d" % (lod, len(tri) // 3))
    np.savez_compressed(os.path.join(ROOT, "blender", "birds", "scan_raw", SID, "body.npz"), **out)
    try:
        wing_planform(P, T, TL, uv, ~keep, uvv, nc, lm, L)
    except Exception as e:   # a species without a clean raised wing keeps its painted wings
        print("SCAN wing planform FAILED", e)
    img = max((i for i in bpy.data.images if i.size[0] > 0), key=lambda i: i.size[0])
    img.scale(2048, 2048)
    img.filepath_raw = os.path.join(ROOT, "blender", "birds", "scan_raw", SID, "body_tex.png")
    img.file_format = "PNG"
    img.save()
    print("SCAN body texture", img.filepath_raw)


PX = None


def wing_planform(P, T, TL, uv, cut, uvv, nc, lm, L, res=(1024, 512)):
    """The raised left wing (cut off the body) seen square-on from above (dorsal: the side facing the tail in the V
    pose) and from below, as images over span xi (0 root .. 1 tip) and chord eta (span units, + toward the tail), by
    splatting the dense scan's vertices with a depth test. Also the leading and trailing edge per span column, so
    build_bird.py can map each template feather onto the photographed wing whatever their shapes."""
    sel = cut & (P[:, 2] > 0.02 * L) & (nc > 0.0) & (uvv > 0.05) & (uvv < 0.95)
    Q = P[sel]
    if len(Q) < 2000:
        raise RuntimeError("too few wing vertices")
    # vertex normals (area-weighted face normals): each side's image takes only vertices facing its camera
    fn = np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]])
    vn = np.zeros_like(P)
    for k in range(3):
        np.add.at(vn, T[:, k], fn)
    vn = vn[sel] / np.maximum(np.linalg.norm(vn[sel], axis=1), 1e-12)[:, None]
    vuv = np.zeros((len(P), 2))
    vuv[T.ravel()] = uv[TL.ravel()]
    H, W = PX.shape[:2]
    col = PX[np.clip((vuv[sel, 1] % 1.0 * H).astype(int), 0, H - 1), np.clip((vuv[sel, 0] % 1.0 * W).astype(int), 0, W - 1)]
    c0 = Q.mean(0)
    _, _, vt = np.linalg.svd(Q - c0, full_matrices=False)
    ax_span, ax_chord, nrm_ = vt[0], vt[1], vt[2]
    # root: the wing points nearest the body's mid-plane; span points from there to the tip
    root = Q[np.argsort(Q[:, 2])[: max(50, len(Q) // 40)]].mean(0)
    if np.dot(Q[np.argmax(Q[:, 2])] - root, ax_span) < 0:
        ax_span = -ax_span
    if ax_chord[0] > 0:          # chord + toward the tail (f decreasing)
        ax_chord = -ax_chord
    if np.dot(nrm_, np.cross(ax_span, ax_chord)) < 0:
        nrm_ = -nrm_
    dorsal = nrm_ if nrm_[0] < 0 else -nrm_      # the dorsal side faces the tail in the V pose
    rel = Q - root
    xs, ys, dep = rel @ ax_span, rel @ ax_chord, rel @ dorsal
    span = float(np.quantile(xs, 0.995))
    xi, eta = xs / span, ys / span
    e0, e1 = float(np.quantile(eta, 0.002)), float(np.quantile(eta, 0.998))
    nx, ny = res
    ix = np.clip((xi * (nx - 1)).astype(int), 0, nx - 1)
    iy = np.clip(((eta - e0) / (e1 - e0) * (ny - 1)).astype(int), 0, ny - 1)
    ok0 = (xi >= 0) & (xi <= 1.0)
    facing = vn @ dorsal
    out = {}
    for name, sgn in (("dorsal", 1.0), ("ventral", -1.0)):
        img = np.zeros((ny, nx, 3), np.float32)
        msk = np.zeros((ny, nx), bool)
        ok = ok0 & (sgn * facing > 0.15)
        o = np.argsort(sgn * dep[ok])          # far first, near last: the nearest wins
        img[iy[ok][o], ix[ok][o]] = col[ok][o]
        msk[iy[ok][o], ix[ok][o]] = True
        for _ in range(3):                     # close splat holes
            grow = ~msk & (np.roll(msk, 1, 0) | np.roll(msk, -1, 0) | np.roll(msk, 1, 1) | np.roll(msk, -1, 1))
            if not grow.any():
                break
            acc = np.zeros_like(img)
            cnt = np.zeros((ny, nx))
            for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                m2 = np.roll(msk, (dy, dx), (0, 1))
                acc += np.roll(img, (dy, dx), (0, 1)) * m2[..., None]
                cnt += m2
            img[grow] = acc[grow] / np.maximum(cnt[grow], 1)[:, None]
            msk |= grow
        out[name] = (np.clip(img, 0, 1) * 255).astype(np.uint8)
        out[name + "_mask"] = msk
    m = out["dorsal_mask"] | out["ventral_mask"]
    lead = np.full(nx, np.nan)
    trail = np.full(nx, np.nan)
    for x in range(int(0.12 * nx), nx):   # (the root columns hold scraps of body and thigh: edges from the wing proper)
        yy = np.nonzero(m[:, x])[0]
        if len(yy) > 3:
            lead[x], trail[x] = yy.min(), yy.max()
    good = ~np.isnan(lead)
    xg = np.arange(nx)
    lead = np.interp(xg, xg[good], lead[good]) / (ny - 1) * (e1 - e0) + e0
    trail = np.interp(xg, xg[good], trail[good]) / (ny - 1) * (e1 - e0) + e0
    np.savez_compressed(os.path.join(ROOT, "blender", "birds", "scan_raw", SID, "wing.npz"), **out, lead=lead, trail=trail,
                        e0=e0, e1=e1, span_L=span / L)
    print("SCAN wing planform: span %.2f L, chord %.2f..%.2f, %d verts" % (span / L, e0, e1, len(Q)))


def debug_png(o, path):
    """Left: the side section with centreline and stations; right: the colour map (u down, th across) and the radius map."""
    import bpy as _b
    R, COL = o["R"], o["COL"]
    h = R.shape[0]
    w = R.shape[1]
    rn = (R - R.min()) / (R.max() - R.min() + 1e-9)
    img = np.ones((h * 4, w * 4 * 2 + 8, 3))
    img[:, :w * 4] = np.kron(np.clip(COL, 0, 1), np.ones((4, 4, 1)))
    rr = np.kron(rn, np.ones((4, 4)))
    rr = np.stack([rr, rr, rr], -1)
    rr[np.kron(o["BAD"], np.ones((4, 4))) > 0] = [1, 0, 0]
    img[:, w * 4 + 8:] = rr
    im = _b.data.images.new("dbg", img.shape[1], img.shape[0])
    rgba = np.concatenate([img[::-1], np.ones(img.shape[:2] + (1,))], -1).astype(np.float32)
    im.pixels.foreach_set(rgba.ravel())
    im.filepath_raw = path
    im.file_format = "PNG"
    im.save()
    print("SCAN debug", path)


if __name__ == "__main__":
    main()
