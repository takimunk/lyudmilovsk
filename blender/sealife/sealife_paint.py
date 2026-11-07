"""Procedural textures for the sea-life atlas (numpy only; noise / ramps / png writer from blender/fish/fish_paint.py).

Every rect is painted from its part's field: s (along the part 0 base/head .. 1 tip/tail), q (around: 0 back .. 1 belly for
bodies; across 0..1 for plates; radius 0 pupil .. 1 rim for eyes), a/b = surface metres (isotropic noise coordinates).
Texels outside the part's valid region are filled with the nearest valid colour inside the same rect only (no bleeding
between parts), the padding between rects gets the nearest rect colour, A = 0 there.

Paint dict keys: col | ramp [(q, rgb)], ramp_s [(s, rgb)] + ramp_s_a, grain (+ grain_sc, fraction of L),
alpha (plates: membrane opacity inside the outline, 0 outside), edge / edge_w / edge_a (plate margin colour), rag,
layers: dicts with type mottle | speckle | bands | tip | stripe | streak | spot | eye, plus optional windows s=(s0, s1), q=(q0, q1).
"""
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
FISH = os.path.join(os.path.dirname(HERE), "fish")
if FISH not in sys.path:
    sys.path.insert(0, FISH)

from fish_paint import fbm, vnoise, _hash, window, rgb, mix, ramp, write_png  # noqa: E402,F401
from fish_geo import smoothstep, W, H  # noqa: E402
from sealife_geo import INSET  # noqa: E402


def nearest_fill(img, cover):
    """Jump flooding: every uncovered texel takes the colour of the nearest covered texel (no wrap-around)."""
    h, w = cover.shape
    if cover.all() or not cover.any():
        return img
    INF = 1 << 30
    sy = np.where(cover, np.arange(h)[:, None], -INF).astype(np.int64)
    sx = np.where(cover, np.arange(w)[None, :], -INF).astype(np.int64)
    sy = np.broadcast_to(sy, (h, w)).copy()
    sx = np.broadcast_to(sx, (h, w)).copy()
    yy, xx = np.mgrid[0:h, 0:w]

    def dist(a, b):
        return np.where(a < -1e8, np.inf, (a - yy) ** 2.0 + (b - xx) ** 2.0)
    step = 1 << int(math.ceil(math.log2(max(h, w))))
    while step >= 1:
        for dy in (-step, 0, step):
            for dx in (-step, 0, step):
                if dy == 0 and dx == 0:
                    continue
                ya, yb = max(0, -dy), min(h, h - dy)
                xa, xb = max(0, -dx), min(w, w - dx)
                if yb <= ya or xb <= xa:
                    continue
                cy = np.full((h, w), -INF, np.int64)
                cx = np.full((h, w), -INF, np.int64)
                cy[ya:yb, xa:xb] = sy[ya + dy:yb + dy, xa + dx:xb + dx]
                cx[ya:yb, xa:xb] = sx[ya + dy:yb + dy, xa + dx:xb + dx]
                better = dist(cy, cx) < dist(sy, sx)
                sy = np.where(better, cy, sy)
                sx = np.where(better, cx, sx)
        step //= 2
    out = img[np.clip(sy, 0, h - 1), np.clip(sx, 0, w - 1)]
    return out


def dots(a, b, cell, prob, r, seed, star=0.0):
    """Jittered-grid dots (chromatophores): coverage 0..1.  a, b in cell units."""
    X, Y = a / cell, b / cell
    ix, iy = np.floor(X), np.floor(Y)
    cov = np.zeros(X.shape)
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            cx, cy = ix + dx, iy + dy
            pres = _hash(cx, cy, seed + 1) < prob
            px = cx + 0.15 + 0.7 * _hash(cx, cy, seed + 2)
            py = cy + 0.15 + 0.7 * _hash(cx, cy, seed + 3)
            rr = r * (0.55 + 0.9 * _hash(cx, cy, seed + 4))
            ddx, ddy = X - px, Y - py
            d = np.sqrt(ddx ** 2 + ddy ** 2)
            if star:
                ang = np.arctan2(ddy, ddx)
                k1 = 4 + np.floor(3 * _hash(cx, cy, seed + 6))          # 4..6 irregular arms
                rr = rr * (1 + star * (0.5 + _hash(cx, cy, seed + 7)) * np.cos(k1 * ang + 6.28 * _hash(cx, cy, seed + 5))
                           + 0.5 * star * np.cos(2 * ang + 6.28 * _hash(cx, cy, seed + 8)))
            cov = np.maximum(cov, (1 - smoothstep(0.55, 1.0, d / np.maximum(rr, 1e-6))) * pres)
    return cov


def paint_block(fld, pa, L, seed):
    s, q, a, b = fld["s"], fld["q"], fld["a"], fld["b"]
    shp = s.shape
    if "ramp" in pa:
        img = ramp(pa["ramp"], q)
    else:
        img = np.broadcast_to(rgb(pa.get("col", (128, 128, 128))), shp + (3,)).copy()
    if "ramp_s" in pa:
        img = mix(img, ramp(pa["ramp_s"], s), np.full(shp, pa.get("ramp_s_a", 1.0)))
    gs = pa.get("grain_sc", 0.02) * L
    n1 = fbm(a / gs, b / gs, seed, 3)
    img = img * (1 + (n1[..., None] - 0.5) * 2 * pa.get("grain", 0.06))
    for li, lay in enumerate(pa.get("layers", [])):
        img = layer(img, lay, fld, L, seed + 97 * (li + 1))
    A = np.ones(shp)
    if "alpha" in pa:                         # plate: outline from the field's v (|v| <= 1 inside)
        v = np.abs(fld.get("v", np.zeros(shp)))
        rag = pa.get("rag", 0.06)
        nz = vnoise(a / (0.01 * L), b / (0.004 * L), seed + 7)
        tipcut = 1 - smoothstep(0.97 - rag, 1.0, s + 0 * nz)
        lim = 1 - rag * nz
        inside = (1 - smoothstep(lim - 0.08, lim, v)) * tipcut
        A = pa["alpha"] * inside
        if "edge" in pa:
            img = mix(img, rgb(pa["edge"]), smoothstep(1 - pa.get("edge_w", 0.25), 1.0, np.maximum(v, smoothstep(0.75, 1.0, s)))
                      * pa.get("edge_a", 0.7))
        A = np.where(v > 1.05, 0.0, A)
    return np.clip(img, 0, 1), np.clip(A, 0, 1)


def layer(img, lay, fld, L, seed):
    s, q, a, b = fld["s"], fld["q"], fld["a"], fld["b"]
    t = lay["type"]
    col = rgb(lay["col"]) if "col" in lay else None
    sr, qr = lay.get("s", (0.0, 1.0)), lay.get("q", (-1.0, 2.0))
    win = window(s, sr[0], sr[1], lay.get("ssoft", 0.02)) * window(q, qr[0], qr[1], lay.get("qsoft", 0.05))
    amt = lay.get("a", 0.8)
    if t == "mottle":
        sc = lay.get("sc", 0.05) * L
        n = fbm(a / sc, b / sc, seed, 4)
        th = lay.get("th", 0.55)
        w = smoothstep(th - lay.get("soft", 0.08), th + lay.get("soft", 0.08), n)
        return mix(img, col, w * win * amt)
    if t == "speckle":
        c = lay.get("cell", 0.02) * L
        cov = dots(a / c, b / c, 1.0, lay.get("prob", 0.5), lay.get("r", 0.25), seed, lay.get("star", 0.0))
        return mix(img, col, cov * win * amt)
    if t == "bands":
        w = np.zeros(s.shape)
        bw = lay.get("w", 0.01)
        for at in lay["at"]:
            if lay.get("rear"):                       # lighter / darker rear margin of each segment (before the boundary)
                w = np.maximum(w, window(s, at - bw, at, bw * 0.3))
            else:
                w = np.maximum(w, np.exp(-((s - at) / bw) ** 2))
        return mix(img, col, w * win * amt)
    if t == "tip":
        w = smoothstep(lay["s0"], lay["s1"], s)
        return mix(img, col, w * win * amt)
    if t == "stripe":
        w = window(q, lay["q0"], lay["q1"], lay.get("soft", 0.04))
        return mix(img, col, w * win * amt)
    if t == "streak":                              # lengthwise streaks (gut line, flipper digits)
        n = lay.get("n", 5)
        qq = q * n
        d = np.abs(qq - np.round(qq)) / n
        w = 1 - smoothstep(lay.get("w", 0.02) * 0.5, lay.get("w", 0.02), d)
        return mix(img, col, w * win * amt)
    if t == "spot":                                # soft ellipse at (s0, q0) with radii (rs, rq)
        d = np.sqrt(((s - lay["s0"]) / lay["rs"]) ** 2 + ((q - lay["q0"]) / lay["rq"]) ** 2)
        return mix(img, col, (1 - smoothstep(0.6, 1.0, d)) * win * amt)
    if t == "eye":                                 # s = radius 0 pole .. 1 rim
        out = mix(img, col, (1 - smoothstep(lay.get("r", 0.6) - 0.08, lay.get("r", 0.6) + 0.04, s)) * amt)
        if "facet" in lay:
            c = lay["facet"] * L
            n = vnoise(a / c, b / c, seed)
            out = out * (1 + (n[..., None] - 0.5) * 0.25)
        if "glint" in lay:
            gx, gy, gr = lay["glint"]
            ang = 2 * math.pi * q
            x, y = s * np.cos(ang), s * np.sin(ang)
            d = np.sqrt((x - gx) ** 2 + (y - gy) ** 2)
            out = mix(out, np.array([0.92, 0.92, 0.9]), (1 - smoothstep(gr * 0.6, gr, d)) * 0.8)
        return out
    raise ValueError(t)


def paint_texture(parts, layout, spec):
    L = spec["length"]
    seed = spec["seed"]
    img = np.zeros((H, W, 3))
    alpha = np.zeros((H, W))
    cover = np.zeros((H, W), bool)
    owner = {}
    for p in parts:
        if p.rect_key not in owner and not p.mirror.all():
            owner[p.rect_key] = p
    for p in parts:
        owner.setdefault(p.rect_key, p)
    for key, p in owner.items():
        x0, y0, w, h = layout.rects[key]
        sx, sy = layout.scale[key]
        ys, xs = np.mgrid[y0:y0 + h, x0:x0 + w]
        A = (xs + 0.5 - x0 - INSET) / sx
        B = (ys + 0.5 - y0 - INSET) / sy
        fld = p.field(A, B)
        col, al = paint_block(fld, p.paint, L, seed + sum(map(ord, key)))
        valid = fld["valid"]
        if not valid.all():
            col = nearest_fill(col, valid)
            if p.kind == "fin":
                al = np.where(valid, al, 0.0)
        img[y0:y0 + h, x0:x0 + w] = col
        alpha[y0:y0 + h, x0:x0 + w] = al
        cover[y0:y0 + h, x0:x0 + w] = True
    img = nearest_fill(img, cover)
    alpha[~cover] = 0.0
    return np.concatenate([np.clip(img, 0, 1), alpha[..., None]], 2).astype(np.float32)
