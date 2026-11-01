"""Procedural fish texture painting in numpy (deterministic per species seed).  Pure numpy, no bpy.

paint_texture(spec, body, layout) -> (512, 1024, 4) float32 RGBA in 0..1 (sRGB values).
A = fin membrane opacity (1 on body/eyes/rays; membranes 0.35-0.8; ragged edge to 0).
"""
import math
import zlib
import struct

import numpy as np

from fish_geo import W, H, PAD, smoothstep


# ------------------------------------------------------------------ noise
def _hash(ix, iy, seed):
    h = (ix.astype(np.int64) * 374761393 + iy.astype(np.int64) * 668265263 + int(seed) * 1442695041) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    h = h ^ (h >> 16)
    return (h & 0xFFFFFF) / float(0x1000000)


def vnoise(x, y, seed):
    ix, iy = np.floor(x), np.floor(y)
    fx, fy = x - ix, y - iy
    sx, sy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a = _hash(ix, iy, seed)
    b = _hash(ix + 1, iy, seed)
    c = _hash(ix, iy + 1, seed)
    d = _hash(ix + 1, iy + 1, seed)
    return (a * (1 - sx) + b * sx) * (1 - sy) + (c * (1 - sx) + d * sx) * sy


def fbm(x, y, seed, octaves=4):
    s, amp, tot, f = 0.0, 1.0, 0.0, 1.0
    for o in range(octaves):
        s = s + amp * vnoise(x * f, y * f, seed + 31 * o)
        tot += amp
        amp *= 0.5
        f *= 2.03
    return s / tot


def window(x, lo, hi, soft):
    soft = max(soft, 1e-6)
    return smoothstep(lo - soft, lo + soft, x) * (1 - smoothstep(hi - soft, hi + soft, x))


def rgb(c):
    return np.array(c, float) / 255.0


def mix(a, b, w):
    return a * (1 - w[..., None]) + b * w[..., None]


def ramp(stops, q):
    """Piecewise-smooth colour ramp over q; stops [(q, (r, g, b)), ...] 0..255."""
    qs = np.array([s[0] for s in stops], float)
    cs = np.array([rgb(s[1]) for s in stops])
    out = np.zeros(q.shape + (3,))
    for k in range(3):
        out[..., k] = np.interp(q, qs, cs[:, k])
    # soften the piecewise-linear joints
    return out


# ------------------------------------------------------------------ body fields
class Field:
    pass


def body_field(body, layout, side):
    """Texel fields for one face region (rows x cols): t, am (arc metres), th, q, valid, xm."""
    f = Field()
    L = body.L
    cols = np.arange(PAD, W - PAD)
    t = (cols + 0.5 - PAD) / (W - 2 * PAD) * body.tmax
    t = np.clip(t, 0.0015, body.tend)
    arcs, thg = body.arcs(t)
    row0 = PAD if side <= 0 or not layout.flat else PAD + layout.face_h + PAD
    nrows = int(math.floor(layout.face_h))
    rows = np.arange(nrows)
    am = (rows[:, None] + 0.5) / layout.dens_v * np.ones((1, len(cols)))
    th = np.zeros_like(am)
    for c in range(len(cols)):
        th[:, c] = np.interp(am[:, c], arcs[c], thg)
    f.valid = am <= arcs[:, -1][None, :]
    f.t = np.broadcast_to(t[None, :], am.shape)
    f.th = th
    f.am = am
    f.q = body.q_field(f.t, th)
    f.xm = f.t * L
    f.height = ((body.top(t) + body.bot(t)) * L)[None, :] * np.ones_like(am)
    f.rows = (row0, nrows)
    f.cols = (PAD, len(cols))
    f.arcs = arcs
    f.thg = thg
    return f


# ------------------------------------------------------------------ body paint
def paint_face(spec, body, fld, pa, blind=False):
    seed = spec["seed"] + (777 if blind else 0)
    L = body.L
    q, t, xm, am = fld.q, fld.t, fld.xm, fld.am
    # surface coordinates in metres that are continuous across the dorsal midline (mirror fold): (xm, am)
    base = ramp(pa["ramp"], q)
    n1 = fbm(xm / 0.012, am / 0.012, seed, 3)
    base = base * (1 + (n1[..., None] - 0.5) * pa.get("grain", 0.06))
    img = base.copy()
    for lay in pa.get("layers", []):
        img = apply_layer(img, lay, spec, body, fld, seed)
    # operculum / head
    hd = pa.get("head")
    if hd:
        tg = body.tg
        curve = hd.get("curve", 0.012)
        tl = tg + curve * (q - 0.45)
        zone = 1 - smoothstep(tl - 0.05, tl + 0.002, t)       # 1 in front of the gill edge
        img = mix(img, rgb(hd["tint"]) * (1 + (n1[..., None] - 0.5) * 0.08), zone * hd.get("amount", 0.3))
        gl = hd.get("line", (60, 60, 50))
        lw = hd.get("line_w", 0.0012) * L / 0.2
        d = np.abs(t - tl) * L
        # subtle darker edge: a soft, slightly wide shade of the local skin colour (never a hard dark stripe)
        line = (1 - smoothstep(lw * 0.5, lw * 2.6, d)) * window(q, 0.1, 0.95, 0.06)
        target = img * 0.76 * 0.8 + rgb(gl) * 0.2 * 0.76
        img = mix(img, target, line * min(hd.get("line_a", 0.5), 0.7) * 0.75)
    # mouth gape line
    mo = pa.get("mouth")
    if mo and body.mouth_depth:
        thm = body.theta_m
        am_m = np.array([np.interp(thm, fld.thg, a) for a in fld.arcs])[None, :]
        d = np.abs(am - am_m)
        w = mo.get("w", 0.0012) * L / 0.2
        mh = body.mouth_hold
        gate = 1 - smoothstep(body.mouth_len * (mh + (1 - mh) * 0.7), body.mouth_len * (mh + (1 - mh) * 1.1), t)
        img = mix(img, rgb(mo.get("col", (50, 30, 25))), (1 - smoothstep(w * 0.4, w * 1.4, d)) * gate * 0.85)
    return np.clip(img, 0, 1)


def apply_layer(img, lay, spec, body, fld, seed):
    kind = lay["type"]
    L = body.L
    q, t, xm, am = fld.q, fld.t, fld.xm, fld.am
    s2 = seed + lay.get("seed", 0) * 101
    col = rgb(lay["col"]) if "col" in lay else None
    qr = lay.get("q", (0.0, 1.0))
    tr = lay.get("t", (0.0, 1.0))
    m = window(q, qr[0], qr[1], lay.get("qsoft", 0.06)) * window(t, tr[0], tr[1], lay.get("tsoft", 0.02))
    a = lay.get("a", 0.8)
    if kind == "mottle":
        sc = lay.get("scale", 0.02)
        nz = fbm(xm / sc, am / (sc * lay.get("aspect", 1.0)), s2, lay.get("oct", 4))
        thr = lay.get("thr", 0.55)
        w = smoothstep(thr - lay.get("soft", 0.08), thr + lay.get("soft", 0.08), nz)
        return mix(img, col, w * m * a)
    if kind == "spots":
        cell = lay["cell"]
        rmin, rmax = lay["r"]
        prob = lay.get("prob", 0.7)
        asp = lay.get("aspect", 1.0)
        X, Y = xm / (cell * asp), am / cell
        ix, iy = np.floor(X), np.floor(Y)
        cov = np.zeros(X.shape)
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                cx, cy = ix + dx, iy + dy
                pres = _hash(cx, cy, s2 + 1) < prob
                px = cx + 0.15 + 0.7 * _hash(cx, cy, s2 + 2)
                py = cy + 0.15 + 0.7 * _hash(cx, cy, s2 + 3)
                rr = (rmin + (rmax - rmin) * _hash(cx, cy, s2 + 4)) / cell
                d = np.sqrt((X - px) ** 2 * asp ** 2 + (Y - py) ** 2)
                d = d / np.maximum(rr, 1e-6)
                c = (1 - smoothstep(0.65, 1.0, d)) * pres
                if lay.get("ring"):
                    c = c * smoothstep(0.0, 0.45, d) * 0.9 + c * 0.25
                cov = np.maximum(cov, c)
        return mix(img, col, cov * m * a)
    if kind == "bars":
        pos = lay.get("at")
        if pos is None:
            pos = list(np.arange(lay["t0"], lay["t1"], lay["period"]))
        wob = fbm(xm / 0.02, am / 0.02, s2, 2) - 0.5
        cov = np.zeros(t.shape)
        for p in pos:
            tp = p + wob * lay.get("wob", 0.02) + lay.get("slant", 0.0) * (q - 0.5)
            cov = np.maximum(cov, 1 - smoothstep(lay["w"] * 0.5, lay["w"] * 0.5 + lay.get("soft", 0.01), np.abs(t - tp)))
        return mix(img, col, cov * m * a)
    if kind == "stripe":
        qs = lay["at"] if isinstance(lay["at"], (list, tuple)) else [lay["at"]]
        cov = np.zeros(t.shape)
        wob = (fbm(xm / 0.03, am / 0.03, s2, 2) - 0.5) * lay.get("wob", 0.0)
        for qq in qs:
            d = np.abs(q + wob - qq)
            cov = np.maximum(cov, 1 - smoothstep(lay["w"] * 0.5, lay["w"] * 0.5 + lay.get("soft", 0.02), d))
        return mix(img, col, cov * m * a)
    if kind == "sheen":      # silvery gloss band
        c0 = lay["at"]
        w = np.exp(-((q - c0) / lay["w"]) ** 2)
        nz = fbm(xm / 0.01, am / 0.01, s2, 3)
        return mix(img, col, w * m * a * (0.75 + 0.5 * nz))
    if kind == "scales":
        s = lay["size"]
        wx = fbm(xm / (s * 4), am / (s * 4), s2 + 7, 2) - 0.5
        wy = fbm(xm / (s * 4) + 9, am / (s * 4) + 3, s2 + 8, 2) - 0.5
        X, Y = xm / s + wx * 0.5, am / (s * 0.8) + wy * 0.5
        best = np.full(X.shape, 9.0)
        ux = np.zeros(X.shape)
        for ox, oy in ((0.0, 0.0), (0.5, 0.5)):
            gx, gy = X - ox, Y - oy
            dx, dy = gx - np.round(gx), gy - np.round(gy)
            dd = np.sqrt(dx * dx + dy * dy)
            ux = np.where(dd < best, dx, ux)
            best = np.minimum(best, dd)
        # shingled: bright free (tail-side) edge, shadowed front rim
        rim = smoothstep(0.28, 0.55, best)
        sh = (ux * 1.2 + 0.1) * rim
        nz = fbm(xm / (s * 5), am / (s * 5), s2, 2)
        d = (-sh * lay.get("sheen", 0.06) + rim * lay.get("edge", 0.06)) * m * (0.6 + 0.8 * nz)
        return np.clip(img * (1 - d[..., None]) + np.clip(-d, 0, None)[..., None] * 0.5, 0, 1)
    if kind == "lateral":
        path = np.array(lay["path"], float)
        ql = np.interp(t[0], path[:, 0], path[:, 1])[None, :]
        d = np.abs(q - ql) * fld.height
        w = lay["w"] * L / 0.2
        c = 1 - smoothstep(w * 0.5, w * 1.2, d)
        if lay.get("pores"):
            c = c * (0.55 + 0.45 * (0.5 + 0.5 * np.cos(xm / lay["pores"] * 2 * math.pi)))
        c = c * window(t, lay.get("t0", 0.2), lay.get("t1", 0.85), 0.01)
        return mix(img, col, c * a)
    if kind == "rings":      # bony rings (pipefish): thin cross lines every `period` of t
        ph = ((t - lay["t0"]) / lay["period"]) % 1.0
        d = np.minimum(ph, 1 - ph) * lay["period"] * L
        w = lay["w"] * L / 0.2
        c = (1 - smoothstep(w * 0.5, w * 1.5, d)) * window(t, lay["t0"], lay["t1"], 0.01)
        return mix(img, col, c * m * a)
    if kind == "flush":     # plain tint over a q/t window (e.g. dark snout)
        return mix(img, col, m * a)
    raise ValueError(kind)


# ------------------------------------------------------------------ fins
def paint_fin(img, alpha, rect, f, seed):
    x0, y0, w, h = rect
    ys, xs = np.mgrid[y0:y0 + h, x0:x0 + w]
    u = np.clip((xs - x0 - 0.5) / max(w - 1, 1), 0, 1)
    v = np.clip(1 - (ys - y0 - 0.5) / max(h - 1, 1), 0, 1)
    col = rgb(f.get("col", (150, 140, 120)))
    ray = rgb(f.get("ray", tuple(int(c * 0.7) for c in f.get("col", (150, 140, 120)))))
    mem = f.get("alpha", 0.55)
    rays = max(int(f.get("rays", 10)), 2)
    ur = u * (rays - 1)
    dpx = np.abs(ur - np.round(ur)) / (rays - 1) * (w - 1)
    rw = f.get("ray_w", 1.0)
    rayline = 1 - smoothstep(0.35 * rw, 0.35 * rw + 0.9, dpx)
    nz = vnoise(u * 18, v * 3, seed)
    scal = f.get("scallop", 0.0)
    redge = 1 - f.get("rag", 0.05) - 0.07 * nz - scal * (1 - np.abs(np.cos(np.pi * ur))) ** 0.8
    rayend = 1 - 0.015 - scal * 0.15 * 0 - 0.04 * nz
    am = mem * (1 - smoothstep(redge - 0.14, redge, v)) * (0.8 + 0.4 * vnoise(u * 9 + 3, v * 2, seed + 5))
    am = np.clip(am, 0, 0.95)
    ra = rayline * (v < rayend) * f.get("ray_a", 0.95) * (1 - 0.35 * smoothstep(0.6, 1.0, v))
    A = np.maximum(am, ra)
    A = np.maximum(A, (v < 0.05) * 1.0)           # solid fin root
    streak = vnoise(u * rays * 1.0, v * 1.5, seed + 9)
    c = np.broadcast_to(col, u.shape + (3,)).copy()
    c = c * (0.85 + 0.3 * streak[..., None])
    c = mix(c, ray, rayline * f.get("ray_mix", 0.42))
    if "edge" in f:
        c = mix(c, rgb(f["edge"]), smoothstep(f.get("edge_from", 0.72), 1.0, v) * f.get("edge_a", 0.8))
    if "base" in f:
        c = mix(c, rgb(f["base"]), (1 - smoothstep(0.0, f.get("base_w", 0.3), v)) * f.get("base_a", 0.8))
    for band in f.get("bands", []):       # (v0, v1, colour, amount)
        c = mix(c, rgb(band[2]), window(v, band[0], band[1], 0.05) * band[3])
    sp = f.get("spots")
    if sp:
        cell = sp.get("cell", 0.12)
        X, Y = u / cell * (w / max(h, 1)) , v / cell
        ix, iy = np.floor(X), np.floor(Y)
        cov = np.zeros(X.shape)
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                cx, cy = ix + dx, iy + dy
                pres = _hash(cx, cy, seed + 1) < sp.get("prob", 0.5)
                px = cx + 0.2 + 0.6 * _hash(cx, cy, seed + 2)
                py = cy + 0.2 + 0.6 * _hash(cx, cy, seed + 3)
                rr = sp.get("r", 0.3) * (0.6 + 0.8 * _hash(cx, cy, seed + 4))
                d = np.sqrt((X - px) ** 2 + (Y - py) ** 2) / rr
                cov = np.maximum(cov, (1 - smoothstep(0.6, 1.0, d)) * pres)
        c = mix(c, rgb(sp["col"]), cov * sp.get("a", 0.8) * window(v, 0.12, 1.0, 0.05))
        A = np.maximum(A, cov * sp.get("alpha", 0.0) * (v < redge))
    img[y0:y0 + h, x0:x0 + w] = c
    alpha[y0:y0 + h, x0:x0 + w] = np.clip(A, 0, 1)


# ------------------------------------------------------------------ eyes and extras
def paint_eye(img, alpha, rect, e, ep, seed):
    x0, y0, w, h = rect
    ys, xs = np.mgrid[y0:y0 + h, x0:x0 + w]
    cx, cy = x0 + (w - 1) / 2, y0 + (h - 1) / 2
    rho = np.sqrt((xs - cx) ** 2 + (ys - cy) ** 2) / (w / 2 * 0.96)
    ang = np.arctan2(ys - cy, xs - cx)
    pupil = rgb(ep.get("pupil", (6, 6, 8)))
    iris = rgb(ep.get("iris", (170, 150, 90)))
    iris2 = rgb(ep.get("iris2", tuple(int(c * 0.55) for c in ep.get("iris", (170, 150, 90)))))
    rim = rgb(ep.get("rim", (40, 40, 38)))
    pr = ep.get("pupil_r", 0.42)
    fib = vnoise(ang * 6 + 100, rho * 3, seed)
    ring = mix(np.broadcast_to(iris, rho.shape + (3,)).copy(), iris2, np.clip(smoothstep(pr, 0.95, rho) * 0.7 + (fib - 0.5) * 0.35, 0, 1))
    # metallic glint: brighter on the upper-left
    glint = np.exp(-(((xs - cx) / (w * 0.22) + 0.8) ** 2 + ((ys - cy) / (h * 0.22) + 0.8) ** 2)) * 0.35
    ring = np.clip(ring + glint[..., None], 0, 1)
    c = np.where((rho < pr)[..., None], pupil, ring)
    edge = smoothstep(0.80, 0.98, rho)
    c = mix(c, rim, edge * 0.9)
    # soft pupil edge
    c = mix(c, pupil, (1 - smoothstep(pr - 0.06, pr + 0.02, rho)))
    img[y0:y0 + h, x0:x0 + w] = c
    alpha[y0:y0 + h, x0:x0 + w] = 1.0


def paint_cones(img, alpha, rect, ex, seed):
    """Tubercle / spine texture: vertical gradient, base colour (v=0) to tip colour (v=1), a little mottle."""
    x0, y0, w, h = rect
    ys, xs = np.mgrid[y0:y0 + h, x0:x0 + w]
    v = np.clip(1 - (ys - y0 - 0.5) / max(h - 1, 1), 0, 1)
    n = vnoise(xs / 3.0, ys / 3.0, seed)
    c = mix(np.broadcast_to(rgb(ex.get("col", (120, 120, 100))), v.shape + (3,)).copy(), rgb(ex.get("tip", ex.get("col", (160, 150, 120)))),
            smoothstep(0.15, 1.0, v))
    img[y0:y0 + h, x0:x0 + w] = c * (0.92 + 0.16 * n[..., None])
    alpha[y0:y0 + h, x0:x0 + w] = 1.0


def paint_sucker(img, alpha, rect, ex, seed):
    """Sucker disc (planar uv): dished centre, pale radial striations, rim."""
    x0, y0, w, h = rect
    ys, xs = np.mgrid[y0:y0 + h, x0:x0 + w]
    cx, cy = x0 + (w - 1) / 2, y0 + (h - 1) / 2
    rho = np.sqrt(((xs - cx) / (w / 2)) ** 2 + ((ys - cy) / (h / 2)) ** 2) / 0.96
    ang = np.arctan2((ys - cy) / (h / 2), (xs - cx) / (w / 2))
    n = vnoise(xs / 4.0, ys / 4.0, seed)
    base = rgb(ex.get("col", (150, 140, 120)))
    rim = rgb(ex.get("rim", (190, 180, 150)))
    mid = rgb(ex.get("mid", (110, 100, 85)))
    c = mix(np.broadcast_to(mid, rho.shape + (3,)).copy(), np.broadcast_to(base, rho.shape + (3,)).copy(), smoothstep(0.0, 0.55, rho))
    c = mix(c, np.broadcast_to(rim, rho.shape + (3,)).copy(), smoothstep(0.62, 0.9, rho) * (1 - smoothstep(0.96, 1.05, rho)) * 0.8)
    rad = 0.5 + 0.5 * np.cos(ang * ex.get("spokes", 22))
    c = mix(c, np.broadcast_to(rim, rho.shape + (3,)).copy(), rad * smoothstep(0.1, 0.6, rho) * (1 - smoothstep(0.7, 0.9, rho)) * 0.25)
    img[y0:y0 + h, x0:x0 + w] = c * (0.93 + 0.14 * n[..., None])
    alpha[y0:y0 + h, x0:x0 + w] = 1.0


def paint_flat_rect(img, alpha, rect, col, seed):
    x0, y0, w, h = rect
    ys, xs = np.mgrid[y0:y0 + h, x0:x0 + w]
    n = vnoise(xs / 6.0, ys / 6.0, seed)
    img[y0:y0 + h, x0:x0 + w] = rgb(col) * (0.9 + 0.2 * n[..., None])
    alpha[y0:y0 + h, x0:x0 + w] = 1.0


# ------------------------------------------------------------------ whole texture
def dilate(img, cover, iters=10):
    img = img.copy()
    cover = cover.copy()
    for _ in range(iters):
        if cover.all():
            break
        acc = np.zeros_like(img)
        cnt = np.zeros(cover.shape)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            sh = np.roll(np.roll(img, dy, 0), dx, 1)
            sc = np.roll(np.roll(cover, dy, 0), dx, 1)
            acc += sh * sc[..., None]
            cnt += sc
        new = (~cover) & (cnt > 0)
        img[new] = acc[new] / cnt[new][:, None]
        cover = cover | new
    return img


def paint_texture(spec, body, layout):
    pa = spec["paint"]
    img = np.zeros((H, W, 3))
    alpha = np.zeros((H, W))
    cover = np.zeros((H, W), bool)
    sides = [(-1, False)] + ([(1, True)] if body.flat else [])
    for side, blind in sides:
        fld = body_field(body, layout, side)
        if blind:
            face = paint_blind(spec, body, fld, pa)
        else:
            face = paint_face(spec, body, fld, pa)
        r0, nr = fld.rows
        c0, nc = fld.cols
        img[r0:r0 + nr, c0:c0 + nc] = face
        alpha[r0:r0 + nr, c0:c0 + nc] = 1.0
        cover[r0:r0 + nr, c0:c0 + nc] = fld.valid
        # extend each column's last valid texel downwards (mip levels must not pull in black)
        blk = img[r0:r0 + nr, c0:c0 + nc]
        for c in range(nc):
            v = np.where(fld.valid[:, c])[0]
            if len(v):
                blk[v[-1] + 1:, c] = blk[v[-1], c]
        cover[r0:r0 + nr, c0:c0 + nc] = True
    # make the whole body strip opaque-valid for dilation: bleed outwards
    img = dilate(img, cover, 24)
    seed = spec["seed"]
    covered2 = cover.copy()
    for key, rect in layout.rects.items():
        x0, y0, w, h = rect
        covered2[y0:y0 + h, x0:x0 + w] = True
    for key, rect in layout.rects.items():
        if key.startswith("eye"):
            ei = int(key[3:].split("_")[0])
            paint_eye(img, alpha, rect, spec["eyes"][ei], spec["paint"].get("eye", {}), seed + ei)
        elif key.startswith("bump"):
            ex = spec["extras"][int(key[4:])]
            if ex["kind"] == "sucker":
                paint_sucker(img, alpha, rect, ex, seed + 17)
            else:
                paint_cones(img, alpha, rect, ex, seed + 13)
        elif key == "barbel":
            ex = [e for e in spec["extras"] if e["kind"] == "barbel"][0]
            paint_flat_rect(img, alpha, rect, ex.get("col", (200, 190, 150)), seed + 5)
        else:
            base = key[:-2] if key.endswith(("_L", "_R")) else key
            f = [f for f in spec["fins"] if f["name"] == base][0]
            if key.endswith("_L") and body.flat and "blind" in f:
                f = dict(f, **f["blind"])
            paint_fin(img, alpha, rect, f, seed + sum(map(ord, key)))
    alpha[~covered2] = 0.0
    # body strip gaps outside the valid region: opaque
    out = np.concatenate([np.clip(img, 0, 1), alpha[..., None]], 2)
    # areas never painted (padding between rects): colour bleed from nearby painted texels
    full = covered2 | (alpha > 0)
    rgbd = dilate(out[..., :3], full, 12)
    left = ~(full | (rgbd.sum(2) > 0))
    rgbd[left] = rgbd[full].mean(0)
    out[..., :3] = rgbd
    return out.astype(np.float32)


def paint_blind(spec, body, fld, pa):
    bl = pa.get("blind", dict(ramp=[(0, (230, 228, 220)), (1, (240, 238, 232))], grain=0.05))
    img = paint_face(spec, body, fld, dict(ramp=bl["ramp"], grain=bl.get("grain", 0.05), layers=bl.get("layers", [])), True)
    return img


# ------------------------------------------------------------------ png
def write_png(path, arr):
    """arr (h, w, 3|4) uint8 or float 0..1"""
    a = np.asarray(arr)
    if a.dtype != np.uint8:
        a = np.clip(a * 255 + 0.5, 0, 255).astype(np.uint8)
    h, w, c = a.shape
    raw = b"".join(b"\x00" + a[y].tobytes() for y in range(h))

    def chunk(tag, data):
        c_ = struct.pack(">I", len(data)) + tag + data
        return c_ + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6 if c == 4 else 2, 0, 0, 0)) \
        + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b"")
    with open(path, "wb") as fh:
        fh.write(png)
