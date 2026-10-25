"""Feather painting for the bird builds: each card on a bird points at a cell of that species' atlas, one cell per
(feather shape, style). A style is a base colour plus pattern layers, the way a field guide describes a feather:

    {"c": "#base", "fx": [("tip", "#fff", 0.2), ("fringe", "#c9a", 0.15, "all"), ...], "gloss": 0.8, "hue": 0.0, "rough": 0.5}

Pattern layers (applied in order, later ones on top):
    ("tip", col, w)              band along the tip outline, w = fraction of the feather length
    ("fringe", col, w, where)    edge band, w = fraction of the half-width; where: all | outer | inner | tip
    ("base", col, frac)          proximal part up to t = frac (wing patches and bars on flight feathers)
    ("band", col, t0, t1)        transverse band between t0 and t1 (following the tip outline)
    ("outer", col) ("inner", col)  a whole vane
    ("streak", col, w)           shaft streak, w = fraction of the half-width
    ("spot", col, size, pos)     drop spot on the shaft at t = pos (thrush breast)
    ("arrow", col, size, pos)    chevron pointing to the tip (fieldfare flanks)
    ("bars", col, n, w)          n transverse bars, w = duty 0..1
    ("spangle", col, size)       V tip spot (starling winter)
    ("mirror", col, pos, size)   round spot at t = pos on the inner vane (gull primaries)
    ("grad", col, amt)           blends toward col with t (darker tips, paler bases)
    ("shaft", col)               rachis colour
    ("mottle", col, amt)         soft blotches
    ("down", col)                colour of the fluffy base (default: base darkened)

Fields per texel: s across (-1 outer vane edge .. +1 inner vane edge), t along (0 base .. 1 tip), e = distance to the
outline in half-widths (0 at the edge), tend(s) = where the tip outline is at that column.
"""
import math

import numpy as np

# feather outlines: outer vane, inner vane, tip start, tip roundness (Murmuration's starling), plus emargination
SHAPES = {
    "primary": (0.38, 0.92, 0.80, 0.6), "secondary": (0.55, 0.92, 0.86, 1.0),
    "tertial": (0.78, 0.88, 0.80, 1.0), "tail": (0.72, 0.80, 0.90, 1.4),
    "gcovert": (0.7, 0.82, 0.62, 1.0), "pcovert": (0.6, 0.8, 0.62, 0.9),
    "underwing": (0.75, 0.8, 0.6, 1.0), "alula": (0.4, 0.8, 0.7, 0.8),
    "scapular": (0.8, 0.85, 0.6, 1.0),
}
# cell sizes in atlas pixels (w, h): close-up feathers (flight, tail) get the most
CELL = {
    "primary": (96, 576), "secondary": (96, 480), "tertial": (96, 416), "tail": (96, 528),
    "gcovert": (80, 256), "pcovert": (64, 224), "underwing": (64, 224), "alula": (48, 192), "scapular": (80, 192),
    "contour": (96, 96), "crest": (48, 192),
}


def srgb_lin(h):
    h = h.lstrip("#")
    out = []
    for i in range(3):
        c = int(h[2 * i:2 * i + 2], 16) / 255.0
        out.append(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4)
    return np.array(out)


def L(h):
    return srgb_lin(h) if isinstance(h, str) else np.asarray(h, float)


def _dist_to_edge(a, cap):
    """Chebyshev distance (px) of every inside pixel to the outside, up to cap."""
    d = np.zeros(a.shape)
    cur = a > 0.5
    for i in range(cap):
        if not cur.any():
            break
        d += cur
        nb = cur.copy()
        nb[1:, :] &= cur[:-1, :]
        nb[:-1, :] &= cur[1:, :]
        nb[:, 1:] &= cur[:, :-1]
        nb[:, :-1] &= cur[:, 1:]
        cur = nb
    return d


def outline_long(w, h, kind, seed, emarg=0.0, narrow=1.0, pointed=0.0):
    """Alpha, s, t, barb phase and edge distance for a flight / covert feather (Murmuration's paint_long outline).
    emarg: outer-vane notch depth (fingered primaries of crows, gulls); narrow scales both vanes; pointed sharpens the tip."""
    R = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:h, 0:w] + 0.5
    s = xx / w * 2 - 1
    t = yy / h
    wo, wi, t0, rnd = SHAPES[kind]
    wo, wi = wo * narrow, wi * narrow
    rnd = rnd * (1 - 0.6 * pointed)
    base = np.clip(t / 0.1, 0, 1) ** 0.5
    tip = np.where(t > t0, np.clip(1 - ((t - t0) / (1 - t0 + 1e-6)) ** (2 / rnd), 0, 1) ** 0.5, 1.0)
    prof = base * tip * 0.97
    vane = np.where(s < 0, wo, wi)
    if emarg > 0:  # the vanes step in toward the tip: a "finger"
        step = np.clip((t - 0.55) / 0.08, 0, 1)
        vane = vane * (1 - emarg * step * np.where(s < 0, 0.55, 0.4))
    barb = (t * h - np.abs(s) * w * 0.9) * 0.55
    ragged = 0.94 + 0.06 * np.sin(barb * 1.3 + R.uniform(0, 6)) * np.sin(barb * 0.37)
    lim = vane * prof * ragged
    a = (np.abs(s) < lim).astype(float)
    a = np.where(np.abs(s) < 0.035, (t < 0.995).astype(float), a)  # rachis to the tip
    return a, s, t, barb, lim


def outline_contour(n, seed, w=None, h=None, slim=1.0):
    """Body (contour) feather: rounded, a fluffy base (Murmuration's paint_contour outline)."""
    w = w or n
    h = h or n
    R = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:h, 0:w] + 0.5
    s = xx / w * 2 - 1
    t = yy / h
    wv = np.where(t < 0.55, np.sqrt(np.clip(t / 0.55, 0, 1)),
                  np.sqrt(np.clip(1 - ((t - 0.55) / 0.45) ** 2, 0, 1))) * 0.92 * slim
    barb = (t * h - np.abs(s) * w * 0.7) * 0.6
    lim = wv * (0.9 + 0.1 * np.sin(barb * 1.1 + R.uniform(0, 6)))
    a = (np.abs(s) < lim).astype(float)
    down = t < 0.22
    a = np.where(down, a * (R.random((h, w)) > 0.35), a)
    return a, s, t, barb, lim


def apply_style(style, a, s, t, barb, lim, w, h, seed, contour):
    """Colour (linear), height, roughness and iridescence for one cell."""
    R = np.random.default_rng(seed + 7)
    base = L(style["c"])
    col = np.broadcast_to(base, (h, w, 3)).copy()
    stripes = np.sin(barb * (2.0 if contour else 2.2))
    inside = a > 0
    edge_px = _dist_to_edge(a * (np.abs(s) >= 0.035) + (np.abs(s) < 0.035) * a, 40)
    halfw = max(1.0, np.max(lim) * w * 0.5)
    e = edge_px / halfw                          # 0 at the outline, ~1 at the shaft
    # where the tip outline is, per column
    ys = np.where(inside, t, -1.0)
    tend = ys.max(axis=0, keepdims=True)
    tend = np.where(tend < 0, 1.0, tend)
    dt = tend - t                                # 0 at the tip outline, grows toward the base
    for fx in style.get("fx", []):
        op = fx[0]
        if op == "tip":
            m = dt < fx[2]
        elif op == "fringe":
            where = fx[3] if len(fx) > 3 else "all"
            m = e < fx[2] * 1.0
            if where == "outer":
                m &= s < 0
            elif where == "inner":
                m &= s > 0
            elif where == "tip":
                m &= t > 0.55
        elif op == "base":
            m = (t < fx[2])
        elif op == "band":
            m = (dt > fx[2]) & (dt < fx[3])
        elif op == "outer":
            m = s < 0
        elif op == "inner":
            m = s > 0
        elif op == "streak":
            m = np.abs(s) < fx[2] * np.max(lim) * (1.15 - 0.45 * t)
        elif op == "spot":
            size, pos = fx[2], fx[3]
            m = (s / (size * 0.85)) ** 2 + ((t - pos) / (size * 1.25)) ** 2 * (1 + 0.6 * (t < pos)) < 1
        elif op == "arrow":
            size, pos = fx[2], fx[3]
            m = (np.abs((t - pos) - np.abs(s) * 0.55) < size * 0.35) & (np.abs(s) < size * 1.6)
        elif op == "bars":
            n, duty = fx[2], fx[3]
            m = (np.mod(dt * n + 0.15 * np.sin(s * 3), 1.0) < duty) & (t > 0.18)
        elif op == "spangle":
            size = fx[2]
            m = (t > 1 - size * 0.55 + (np.abs(s) / max(size, 1e-3)) * 0.16) & (np.abs(s) < size * (1.0 - t) * 2.4)
        elif op == "mirror":
            pos, size = fx[2], fx[3]
            m = ((s - 0.25) / size) ** 2 + ((t - pos) / (size * 1.6)) ** 2 < 1
        elif op == "grad":
            k = np.clip(t, 0, 1) ** 1.5 * fx[2]
            col = col * (1 - k[..., None]) + L(fx[1]) * k[..., None]
            continue
        elif op == "mottle":
            nz = R.standard_normal((h // 8 + 2, w // 8 + 2))
            yi = np.linspace(0, nz.shape[0] - 1.001, h)
            xi = np.linspace(0, nz.shape[1] - 1.001, w)
            y0, x0 = yi.astype(int), xi.astype(int)
            fy, fx_ = (yi - y0)[:, None], (xi - x0)[None, :]
            nz = (nz[y0][:, x0] * (1 - fy) * (1 - fx_) + nz[y0 + 1][:, x0] * fy * (1 - fx_)
                  + nz[y0][:, x0 + 1] * (1 - fy) * fx_ + nz[y0 + 1][:, x0 + 1] * fy * fx_)   # smooth blotches, not blocks
            k = np.clip(nz * fx[2], 0, 1)
            col = col * (1 - k[..., None]) + L(fx[1]) * k[..., None]
            continue
        elif op in ("shaft", "down"):
            continue
        else:
            raise ValueError(op)
        col = np.where(m[..., None], L(fx[1]), col)
    # barb texture, darker toward the base of body feathers, the shaft
    # barbs: a faint texture (strong stripes read as hatching up close), each variant a touch lighter or darker
    col *= (0.95 + 0.05 * stripes)[..., None] * (1.0 + R.uniform(-0.035, 0.035))
    if contour:   # a soft darker rim, so overlapping feathers read as feathers, not paint
        col *= (0.93 + 0.07 * np.clip(e * 2.5, 0, 1))[..., None]
    if contour:
        col *= (0.88 + 0.12 * np.clip(t * 1.7, 0, 1))[..., None]
        # the fluffy base mostly hides under the feather in front; near the base colour so gaps don't speckle
        down_c = next((L(f[1]) for f in style.get("fx", []) if f[0] == "down"), base * 0.7)
        down_c = down_c * 0.35 + base * 0.8 * 0.65
        col = np.where((t < 0.22)[..., None], down_c * (0.92 + 0.08 * stripes)[..., None], col)
    shaft_c = next((L(f[1]) for f in style.get("fx", []) if f[0] == "shaft"), None)
    rach = np.abs(s) < (0.03 if contour else 0.035)
    if shaft_c is None:
        shaft_c_arr = col * 1.15 + 0.01
    else:
        shaft_c_arr = np.broadcast_to(shaft_c, col.shape)
    col = np.where(rach[..., None], shaft_c_arr, col)
    # a low shaft ridge (a tall one glints along the whole card: a white streak down the back)
    height = np.where(rach, 0.45 if contour else 0.6, 0.0) + 0.12 * stripes * a + (1.2 if contour else 0.8) * np.clip(e * 0.3, 0, 0.35)
    rough = np.full((h, w), max(0.6, style.get("rough", 0.74)))   # feathers are never mirror-like; gloss is the film tint
    irid = np.full((h, w), style.get("gloss", 0.0)) * np.clip(t * 1.4, 0, 1) * (a > 0)
    return np.clip(col, 0, 1), height, rough, irid


def paint_cell(kind, style, w, h, seed, shape):
    """shape: per-species outline knobs for this kind (emarg, narrow, pointed, slim)."""
    if kind in ("contour", "crest"):
        a, s, t, barb, lim = outline_contour(w, seed, w, h, slim=shape.get("slim", 1.0 if kind == "contour" else 0.55))
        contour = True
    else:
        a, s, t, barb, lim = outline_long(w, h, kind, seed, emarg=shape.get("emarg", 0.0), narrow=shape.get("narrow", 1.0),
                                          pointed=shape.get("pointed", 0.0))
        contour = False
    col, height, rough, irid = apply_style(style, a, s, t, barb, lim, w, h, seed, contour)
    return col, a, height, rough, irid


class Atlas:
    """Shelf-packed cells in one square texture (rows of equal-height cells)."""

    def __init__(self, size=2048):
        self.size = size
        self.cells = {}      # key -> (x0, y0, w, h)
        self.requests = []   # (key, kind, style_name, w, h, seed, shape)

    def request(self, key, kind, style_name, w, h, seed, shape):
        if key not in self.cells and all(r[0] != key for r in self.requests):
            self.requests.append((key, kind, style_name, w, h, seed, shape))
        return key

    def pack(self):
        reqs = sorted(self.requests, key=lambda r: -r[4])
        while True:
            x = y = row_h = 0
            ok = True
            cells = {}
            for r in reqs:
                w, h = r[3], r[4]
                if x + w > self.size:
                    x, y = 0, y + row_h
                    row_h = 0
                if y + h > self.size:
                    ok = False
                    break
                cells[r[0]] = (x, y, w, h)
                x += w
                row_h = max(row_h, h)
            if ok:
                self.cells = cells
                return
            self.size *= 2

    def uv(self, key, s, t, flip=False):
        """Deferred: the cells are packed after the geometry is built; resolve() turns these into coordinates."""
        return ("f", key, s, t, flip)

    def uv_rect(self, key, u, v):
        return ("r", key, u, v)

    def resolve(self, item):
        if item[0] == "f":
            return self._uv(*item[1:])
        return self._uv_rect(*item[1:])

    def _uv(self, key, s, t, flip=False):
        x0, y0, w, h = self.cells[key]
        if flip:
            s = -s
        u = (x0 + (s * 0.5 + 0.5) * w) / self.size
        v = (y0 + min(max(t, 0.0), 1.0) * h) / self.size
        u = min(max(u, (x0 + 0.5) / self.size), (x0 + w - 0.5) / self.size)
        v = min(max(v, (y0 + 0.5) / self.size), (y0 + h - 0.5) / self.size)
        return (u, v)

    def _uv_rect(self, key, u, v):
        """Plain 0..1 coordinates inside a cell (body, beak, legs, eye)."""
        x0, y0, w, h = self.cells[key]
        u = min(max(u, 0.0), 1.0)
        v = min(max(v, 0.0), 1.0)
        return ((x0 + 0.5 + u * (w - 1)) / self.size, (y0 + 0.5 + v * (h - 1)) / self.size)
