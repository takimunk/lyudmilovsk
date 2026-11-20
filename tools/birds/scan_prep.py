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
    V, T, TL, uv, px, srgb = load()
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
    out = dict(us=us, ths=ths, R=R / L, CEN=(CEN - np.interp(lm["body_end"] + 0.45 * L, fs, mid)) / L, COL=COL.astype(np.float32), COLH=(np.clip(COLH, 0, 1) * 255).astype(np.uint8), RH=RH.astype(np.float32),
               BAD=bad, srgb=srgb,
               beak_len=(lm["tip"] - lm["beak_base"]) / L, head_depth=lm["head_depth"] / L, body_depth=lm["body_depth"] / L,
               tail_len=(lm["body_end"] - lm["tail_tip"]) / L,
               sect_f=(fs - lm["body_end"]) / L, sect_top=(top_s - np.interp(lm["body_end"] + 0.45 * L, fs, mid)) / L,
               sect_bot=(bot_s - np.interp(lm["body_end"] + 0.45 * L, fs, mid)) / L)
    d = os.path.join(ROOT, "blender", "birds", "scans", SID)
    os.makedirs(d, exist_ok=True)
    np.savez_compressed(os.path.join(d, "scan.npz"), **out)
    json.dump({k2: float(v) for k2, v in lm.items()} | {"L": float(L), "outlier_share": float(bad.mean()),
               "forward": F.tolist(), "srgb": bool(srgb)}, open(os.path.join(d, "scan_meta.json"), "w"), indent=1)
    print("SCAN saved", os.path.join(d, "scan.npz"), "outliers %.1f%%" % (100 * bad.mean()))
    debug_png(out, os.path.join(ROOT, "blender", "birds", "scan_raw", SID, "scan_debug.png"))


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
