"""Procedural, tileable textures of the ranger's hut (numpy; FFT noise is periodic, so every tile repeats without a seam).

make(name) -> (base RGBA float array [rows = V from the bottom, cols = U], normal RGBA array or None).
Base colours are sRGB values. The build packs them into the GLBs (hut_common.image). No paid generation, no files.
"""
import numpy as np


def _rng(seed):
    return np.random.default_rng(seed)


def noise(n, beta, seed, au=1.0, av=1.0):
    """Periodic fractal noise 0..1. beta = spectral slope (2 = cloudy, 1 = crisp); au/av > 1 damp the variation along
    that axis, so the pattern stretches along it (au for U = columns, av for V = rows)."""
    r = _rng(seed)
    f = np.fft.fftfreq(n)
    fu, fv = np.meshgrid(f * au, f * av)
    k = np.sqrt(fu ** 2 + fv ** 2)
    k[0, 0] = 1.0
    spec = (r.normal(size=(n, n)) + 1j * r.normal(size=(n, n))) / (k ** beta)
    spec[0, 0] = 0
    a = np.real(np.fft.ifft2(spec))
    lo, hi = np.percentile(a, 1), np.percentile(a, 99)
    return np.clip((a - lo) / (hi - lo), 0, 1)


def lerp(a, b, t):
    t = np.asarray(t)[..., None]
    return np.asarray(a) * (1 - t) + np.asarray(b) * t


def hexc(h):
    h = h.lstrip("#")
    return np.array([int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)])


def ramp(t, stops):
    """stops [(pos, '#hex'), ...] -> colours for t in 0..1."""
    out = np.zeros(t.shape + (3,))
    pos = [p for p, _ in stops]
    cols = [hexc(c) for _, c in stops]
    for ch in range(3):
        out[..., ch] = np.interp(t, pos, [c[ch] for c in cols])
    return out


def voronoi(n, pts, seed):
    """Periodic cellular noise: (distance to nearest, distance to second nearest, id of nearest)."""
    r = _rng(seed)
    p = r.random((pts, 2)) * n
    y, x = np.mgrid[0:n, 0:n].astype(np.float32)
    d1 = np.full((n, n), 1e9, np.float32)
    d2 = np.full((n, n), 1e9, np.float32)
    ids = np.zeros((n, n), np.int32)
    for i, (px, py) in enumerate(p):
        dx = np.abs(x - px)
        dx = np.minimum(dx, n - dx)
        dy = np.abs(y - py)
        dy = np.minimum(dy, n - dy)
        d = np.sqrt(dx * dx + dy * dy)
        new1 = d < d1
        d2 = np.where(new1, d1, np.minimum(d2, d))
        ids = np.where(new1, i, ids)
        d1 = np.where(new1, d, d1)
    return d1, d2, ids


def normal_from(h, strength):
    """Tangent normal map (OpenGL, +Y up) from a periodic height map 0..1."""
    du = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5
    dv = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5
    nx, ny, nz = -du * strength, -dv * strength, np.ones_like(h)
    ln = np.sqrt(nx ** 2 + ny ** 2 + nz ** 2)
    out = np.stack([nx / ln * 0.5 + 0.5, ny / ln * 0.5 + 0.5, nz / ln * 0.5 + 0.5, np.ones_like(h)], -1)
    return out


def rgba(rgb):
    return np.concatenate([np.clip(rgb, 0, 1), np.ones(rgb.shape[:2] + (1,))], -1).astype(np.float32)


# ---- the textures ---------------------------------------------------------------------------------------------------

def bark(n, inside):
    """Round-log skin. U = round the log (one tile = one circumference), V = along it. Furrows run along V."""
    f1 = noise(n, 1.6, 11 if not inside else 21, au=1.0, av=7.0)         # long furrows
    f2 = noise(n, 1.3, 12 if not inside else 22, au=1.0, av=3.0)
    f3 = noise(n, 2.0, 13, au=0.5, av=0.5)                                # blotches
    f4 = noise(n, 1.0, 14, au=0.35, av=2.5)                               # fine bark grain
    h = f1 * 0.5 + f2 * 0.2 + f3 * 0.1 + f4 * 0.2
    crack = np.clip((0.33 - f1) * 5.0, 0, 1)                              # dark cracks in the troughs
    if not inside:
        col = ramp(h, [(0, "#4a4036"), (0.35, "#6d6254"), (0.65, "#8a7e6e"), (1.0, "#a69b89")])   # weathered grey-brown
        col = lerp(col, hexc("#9aa089"), np.clip((f3 - 0.62) * 2.2, 0, 0.5) * 0.7)                # lichen
        col = col * (1 - crack[..., None] * 0.45)
    else:
        col = ramp(h, [(0, "#6b4524"), (0.4, "#9a6a3a"), (0.7, "#b98a52"), (1.0, "#cfa46a")])    # warm, smoke-aged
        col = col * (1 - crack[..., None] * 0.35)
        col = col * (0.88 + 0.12 * f3[..., None])
    nm = normal_from(h + crack * -0.3, 5.0 if not inside else 3.5)
    return rgba(col), nm


def endgrain(n):
    """Cut log end: annual rings round the middle (U, V 0..1 over the disc), radial cracks."""
    y, x = np.mgrid[0:n, 0:n] / n - 0.5
    w = noise(n, 2.2, 31)
    rr = np.sqrt(x * x + y * y) * 2.0 + (w - 0.5) * 0.10
    rings = 0.5 + 0.5 * np.sin(rr * 2 * np.pi * 9)
    ang = np.arctan2(y, x)
    crack = np.clip(1 - np.abs(np.sin(ang * 3.0 + w * 3)) * 9, 0, 1) * np.clip(rr * 1.4, 0, 1) * 0.5
    col = ramp(rings * 0.6 + w * 0.4, [(0, "#a98055"), (0.5, "#c59b69"), (1, "#d8b88a")])
    col = col * (1 - crack[..., None] * 0.55)
    col = lerp(col, hexc("#6e5a46"), np.clip(rr - 0.85, 0, 1) * 3)       # the bark's edge ring
    return rgba(col), None


def planks(n, kind):
    """Boards: U along the board, V across. 8 boards per tile (a tile is 1.2 m: boards 15 cm).
    kind: out (weathered grey), in (natural pine), floor (painted red-brown)."""
    r = _rng(41 + len(kind))
    nb = 8
    bh = n // nb
    grain = noise(n, 1.5, 42, au=1.0, av=9.0)
    fine = noise(n, 1.0, 43, au=1.0, av=14.0)
    blot = noise(n, 2.2, 44)
    hgt = np.zeros((n, n))
    col = np.zeros((n, n, 3))
    for b in range(nb):
        sl = slice(b * bh, (b + 1) * bh)
        shift = r.integers(0, n)
        g = np.roll(grain, (shift, shift), (0, 1))[sl]
        fn = np.roll(fine, (shift, 0), (0, 1))[sl]
        tint = r.uniform(-1, 1)
        t = np.clip(g * 0.6 + fn * 0.25 + blot[sl] * 0.15 + tint * 0.08, 0, 1)
        if kind == "out":
            c = ramp(t, [(0, "#4d4640"), (0.5, "#7a7268"), (1, "#a49b8c")])
        elif kind == "in":
            c = ramp(t, [(0, "#a8783f"), (0.5, "#c9a063"), (1, "#e0be84")])
        else:
            c = ramp(t, [(0, "#5a2519"), (0.5, "#74321f"), (1, "#8a4128")])
            c = c * (0.9 + 0.1 * (1 - blot[sl][..., None]))                  # worn paint: lighter, patchy
            wear = np.clip((blot[sl] - 0.7) * 3, 0, 1)
            c = lerp(c, hexc("#a67447"), wear * 0.5)
        col[sl] = c
        hgt[sl] = t * 0.5
        # a knot every other board
        if b % 2 == 0:
            kx, ky = r.integers(n // 8, n - n // 8), b * bh + bh // 2
            yy, xx = np.mgrid[0:n, 0:n]
            dx = np.minimum(np.abs(xx - kx), n - np.abs(xx - kx))
            dd = np.sqrt((dx / 1.8) ** 2 + (yy - ky) ** 2)
            knot = np.clip(1 - dd / 12.0, 0, 1)
            m = np.zeros((n, n), bool)
            m[sl] = True
            kk = knot * m
            col = col * (1 - kk[..., None] * 0.45)
            hgt -= kk * 0.2
        # nails near the board's ends
        for nx in (n // 16, n - n // 16):
            yy, xx = np.mgrid[0:n, 0:n]
            dd = np.sqrt((xx - nx) ** 2 + (yy - (b * bh + bh // 2)) ** 2)
            nail = np.clip(1 - dd / 3.0, 0, 1)
            m = np.zeros((n, n), bool)
            m[sl] = True
            col = col * (1 - (nail * m)[..., None] * 0.6)
    # seams between boards (dark grooves, 3 px)
    for b in range(nb):
        for k in (-1, 0, 1):
            row = (b * bh + k) % n
            col[row] *= 0.45
            hgt[row] -= 0.35
    # end joint of every board somewhere in the tile
    for b in range(nb):
        jx = int(r.integers(n // 4, n - n // 4))
        col[b * bh:(b + 1) * bh, jx - 1:jx + 1] *= 0.45
        hgt[b * bh:(b + 1) * bh, jx - 1:jx + 1] -= 0.3
    nm = normal_from(hgt, 4.0)
    return rgba(col), nm


def roof(n):
    """Weathered corrugated sheet seen from above: U across the waves (along the ridge), V down the slope.
    Rust bleeds down the slope in streaks, with pale zinc between."""
    streak = noise(n, 1.3, 51, au=1.0, av=10.0)
    patch = noise(n, 2.0, 52)
    fine = noise(n, 1.0, 53, au=1.0, av=2.0)
    rust = np.clip((streak * 0.6 + patch * 0.5 - 0.40) * 2.6, 0, 1)
    zinc = ramp(fine, [(0, "#7c8284"), (0.5, "#9aa0a0"), (1, "#b5b9b6")])
    rustc = ramp(np.clip(patch * 0.8 + fine * 0.2, 0, 1), [(0, "#5c2c18"), (0.5, "#8a4524"), (1, "#b2672f")])
    col = lerp(zinc, rustc, rust)
    # old paint: green-grey flakes
    paint = np.clip((noise(n, 2.2, 54) - 0.55) * 3, 0, 1) * (1 - rust)
    col = lerp(col, hexc("#6c7d6a"), paint * 0.6)
    # darker along the wave seams (U period 1/14 of the tile)
    u = np.arange(n)[None, :]
    wave = 0.5 + 0.5 * np.cos(2 * np.pi * u / n * 20)
    col = col * (0.82 + 0.18 * wave[..., None])
    return rgba(col), None


def stone(n):
    d1, d2, ids = voronoi(n, 38, 61)
    edge = np.clip((d2 - d1) / 5.0, 0, 1)
    r = _rng(62)
    tint = r.random(40)[ids]
    cell = ramp(np.clip(tint * 0.7 + noise(n, 1.8, 63) * 0.5, 0, 1), [(0, "#6b6a64"), (0.5, "#8b8982"), (1, "#a8a59b")])
    moss = np.clip((noise(n, 2.2, 64) - 0.62) * 3, 0, 1)
    cell = lerp(cell, hexc("#5f6e46"), moss * 0.5)
    col = cell * (0.35 + 0.65 * edge[..., None])
    return rgba(col), normal_from(edge * 0.8 + noise(n, 1.4, 65) * 0.15, 4.0)


def iron(n):
    f = noise(n, 1.0, 71)
    s = noise(n, 2.2, 72)
    col = ramp(f * 0.4 + s * 0.6, [(0, "#1d1c1b"), (0.6, "#2c2a28"), (1, "#45403b")])
    rust = np.clip((s - 0.66) * 3, 0, 1)
    col = lerp(col, hexc("#6a3a1f"), rust * 0.6)
    return rgba(col), None


def brick(n):
    """Whitewashed brick of the stove: 8 courses x 4 bricks per tile, lime wash flaking off."""
    rows, cols = 8, 4
    bh, bw = n // rows, n // cols
    y, x = np.mgrid[0:n, 0:n]
    row = y // bh
    xs = (x + (row % 2) * (bw // 2)) % n
    cx = xs % bw
    cy = y % bh
    mortar = ((cx < 4) | (cy < 4)).astype(float)
    r = _rng(81)
    tint = r.random((rows, cols + 1))
    bid = (xs // bw) % cols
    t = tint[row, bid]
    nz = noise(n, 1.6, 82)
    brickc = ramp(np.clip(t * 0.5 + nz * 0.5, 0, 1), [(0, "#7d3322"), (0.5, "#9a4529"), (1, "#b45a35")])
    mort = np.broadcast_to(hexc("#b7b0a2"), brickc.shape)
    col = lerp(brickc, mort, mortar)
    wash = np.clip((noise(n, 2.0, 83) - 0.5) * 3.0, 0, 1)
    col = lerp(col, hexc("#e8e4d8"), wash * 0.92)
    return rgba(col), normal_from(1 - mortar * 0.6 + nz * 0.1, 3.0)


def wool(n):
    """Wool blanket: a green and brown check with a coarse weave."""
    y, x = np.mgrid[0:n, 0:n] / n
    band = lambda v, k: 0.5 + 0.5 * np.sin(2 * np.pi * v * k)
    stripe_u = (band(x, 4) > 0.62).astype(float)
    stripe_v = (band(y, 4) > 0.62).astype(float)
    base = np.broadcast_to(hexc("#56603f"), (n, n, 3))
    c1 = np.broadcast_to(hexc("#8c3a2a"), (n, n, 3))
    c2 = np.broadcast_to(hexc("#c9b98a"), (n, n, 3))
    col = lerp(base, c1, np.maximum(stripe_u, stripe_v) * 0.9)
    thin = ((band(x, 16) > 0.9) | (band(y, 16) > 0.9)).astype(float)
    col = lerp(col, c2, thin * 0.7)
    weave = 0.88 + 0.12 * np.sin(2 * np.pi * x * 160) * np.sin(2 * np.pi * y * 160)
    col = col * weave[..., None] * (0.9 + 0.1 * noise(n, 1.2, 91)[..., None])
    return rgba(col), None


def rug(n):
    """Woven runner: red-brown ground, three bands of dull colour, fringed ends (U 0..1 over the rug's width)."""
    y, x = np.mgrid[0:n, 0:n] / n
    base = np.broadcast_to(hexc("#7a2e22"), (n, n, 3))
    col = base.copy()
    for lo, hi, c in ((0.10, 0.16, "#d1b57a"), (0.20, 0.24, "#2f4b5a"), (0.76, 0.80, "#2f4b5a"), (0.84, 0.90, "#d1b57a")):
        m = ((x > lo) & (x < hi)).astype(float)
        col = lerp(col, np.broadcast_to(hexc(c), col.shape), m)
    d = np.abs(((x * 3 % 1) - 0.5)) + np.abs(((y * 6 % 1) - 0.5))
    lozenge = (d < 0.35).astype(float) * ((x > 0.3) & (x < 0.7))
    col = lerp(col, np.broadcast_to(hexc("#c58a3a"), col.shape), lozenge * 0.8)
    weave = 0.85 + 0.15 * np.sin(2 * np.pi * x * 120) * np.sin(2 * np.pi * y * 120)
    col = col * weave[..., None] * (0.85 + 0.15 * noise(n, 1.4, 101)[..., None])
    return rgba(col), None


def paper(n):
    f = noise(n, 1.6, 111)
    col = ramp(f, [(0, "#d9cfb4"), (1, "#ece4cc")])
    y = np.arange(n)[:, None]
    ruled = ((y % (n // 14)) < 2).astype(float) * 0.12
    col = col * (1 - ruled[..., None] * np.array([0.5, 0.4, 0.2]))
    return rgba(col), None


def mapquad(n):
    """A placeholder of the wall map (the game paints the real one later): paper with a coast, contours, a pencilled route."""
    y, x = np.mgrid[0:n, 0:n] / n
    f = noise(n, 2.4, 121)
    h = f + 0.25 * np.sin(x * 6) * 0.2
    col = ramp(np.clip(f, 0, 1), [(0, "#d6cba8"), (1, "#e8dfc2")])
    sea = (x + 0.12 * noise(n, 2.0, 122) < 0.28).astype(float)
    col = lerp(col, np.broadcast_to(hexc("#a9bfc4"), col.shape), sea)
    cont = (np.abs(((f * 12) % 1) - 0.5) < 0.03).astype(float) * (1 - sea)
    col = lerp(col, np.broadcast_to(hexc("#8a6a3e"), col.shape), cont * 0.8)
    route = (np.abs(y - (0.35 + 0.25 * np.sin(x * 7 + 1.0))) < 0.004).astype(float) * (x > 0.3)
    col = lerp(col, np.broadcast_to(hexc("#7a1a1a"), col.shape), route)
    border = ((x < 0.025) | (x > 0.975) | (y < 0.025) | (y > 0.975)).astype(float)
    col = lerp(col, np.broadcast_to(hexc("#3d3a33"), col.shape), border * 0.9)
    return rgba(col), None


def tin(n):
    """Label atlas for tins: 4 strips (V quarters), each a different 1970s label. U is round the can."""
    y, x = np.mgrid[0:n, 0:n] / n
    col = np.zeros((n, n, 3))
    designs = [("#d8d0b8", "#b52a2a", "#1c1c1c"), ("#2f5a3a", "#e9dd9a", "#e9dd9a"),
               ("#c9a23a", "#1f3f7a", "#1f3f7a"), ("#c8c4b8", "#8a1d1d", "#2a2a2a")]
    for i, (a, b, c) in enumerate(designs):
        m = ((y >= i / 4) & (y < (i + 1) / 4))
        yy = (y * 4) % 1
        c0 = np.broadcast_to(hexc(a), col.shape).copy()
        band = (np.abs(yy - 0.5) < 0.18).astype(float)
        c0 = lerp(c0, np.broadcast_to(hexc(b), col.shape), band)
        star = ((np.abs(x - 0.5) + np.abs(yy - 0.5) * 1.6) < 0.12).astype(float) if i % 2 == 0 else \
            ((np.sqrt((x - 0.5) ** 2 + ((yy - 0.5) * 1.6) ** 2)) < 0.12).astype(float)
        c0 = lerp(c0, np.broadcast_to(hexc(c), col.shape), star)
        col[m] = c0[m]
    col = col * (0.92 + 0.08 * noise(n, 1.4, 131)[..., None])
    return rgba(col), None


BUILDERS = {
    "bark_out": (lambda n: bark(n, False), 1024),
    "bark_in": (lambda n: bark(n, True), 1024),
    "endgrain": (endgrain, 512),
    "plank_out": (lambda n: planks(n, "out"), 1024),
    "plank_in": (lambda n: planks(n, "in"), 1024),
    "floor_in": (lambda n: planks(n, "floor"), 1024),
    "roof": (roof, 1024),
    "stone": (stone, 512),
    "iron": (iron, 256),
    "brick": (brick, 512),
    "wool": (wool, 512),
    "rug": (rug, 512),
    "paper": (paper, 256),
    "map": (mapquad, 512),
    "tin": (tin, 512),
}

_cache = {}


def make(name):
    if name not in _cache:
        fn, n = BUILDERS[name]
        _cache[name] = fn(n)
    return _cache[name]
