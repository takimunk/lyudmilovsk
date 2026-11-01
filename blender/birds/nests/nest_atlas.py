"""Procedural atlas for the nest kit: ONE 2048 x 2048 sheet (square, so lint_bird's size*size maths is exact).

Material cells are 256 x 256 (planks 512 x 512) and TILEABLE, so a face may be shifted by whole cells without a visible
seam; egg cells are 128 x 128 and are painted in the egg's own (azimuth, length) parameter space.
paint() -> (colour sRGB uint8, normal OpenGL uint8, roughness uint8) full-atlas arrays, written as PNG by write_png().
No PIL: a 20-line PNG writer below.
"""
import math
import os
import struct
import sys
import zlib

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from bird_common import normal_from_height, tile_noise  # noqa: E402

SIZE = 2048
TILE = 128

MAT_CELLS = ["bark_brown", "bark_grey", "bark_dark", "bark_silver", "bark_pale", "bark_green",
             "grass_dry", "grass_fine", "grass_bleached", "moss_green", "moss_dry", "lichen",
             "mud_wet", "mud_dry", "reed", "down", "feather", "guano", "clay", "pebbles", "earth", "wool",
             "rootlet", "seaweed", "leaf_dead", "soot", "wood_raw", "treebark", "wood_cut"]
PLANK_CELLS = ["plank_blue", "plank_white", "plank_ochre"]

# texels per millimetre of the real surface (how big the picture is drawn on a face)
DENS = {"bark_brown": 2.0, "bark_grey": 2.0, "bark_dark": 2.0, "bark_silver": 2.0, "bark_pale": 2.2, "bark_green": 2.0,
        "grass_dry": 3.0, "grass_fine": 3.5, "grass_bleached": 3.0, "moss_green": 4.0, "moss_dry": 4.0, "lichen": 4.0,
        "mud_wet": 2.5, "mud_dry": 3.0, "reed": 2.5, "down": 3.5, "feather": 3.0, "guano": 2.5, "clay": 2.5,
        "pebbles": 3.0, "earth": 3.0, "wool": 3.0, "rootlet": 4.0, "seaweed": 2.5, "leaf_dead": 2.0, "soot": 2.0,
        "wood_raw": 2.0, "treebark": 1.4, "wood_cut": 2.0, "plank_blue": 2.4, "plank_white": 2.4, "plank_ochre": 2.4}
# (L mm, B mm, ground, marks, taper) -- marks: (kind, colour, count, (size mm min, max), zone)
EGGS = {
    "Rook": (40, 28, "#a9c4a0", [("blotch", "#6a5a2c", 70, (1.5, 3.2), "all"), ("dot", "#8a7b45", 90, (0.6, 1.2), "all")], .14),
    "HoodedCrow": (43, 30, "#9fc2a6", [("blotch", "#5e5a2b", 80, (1.5, 3.4), "all"), ("dot", "#7f7a40", 90, (0.6, 1.2), "all")], .14),
    "CarrionCrow": (43, 30, "#a2c6a2", [("blotch", "#62572a", 75, (1.5, 3.4), "all"), ("dot", "#857a3e", 80, (0.6, 1.2), "all")], .14),
    "Jackdaw": (36, 26, "#b6d4d0", [("dot", "#2e2a22", 30, (0.7, 1.6), "blunt"), ("fleck", "#59503a", 40, (0.4, 0.8), "all")], .14),
    "Magpie": (34, 24, "#a8c0a0", [("fleck", "#6b5e32", 160, (0.5, 1.1), "all"), ("blotch", "#8a7a45", 25, (1.4, 2.4), "blunt")], .14),
    "Raven": (50, 34, "#9cbfae", [("blotch", "#3d3a1c", 50, (2.0, 4.2), "all"), ("dot", "#6b6330", 80, (0.8, 1.6), "all")], .14),
    "Jay": (31, 23, "#b2ae7e", [("fleck", "#7f7a4a", 260, (0.4, 0.9), "all"), ("dot", "#5c5530", 14, (0.8, 1.4), "blunt")], .13),
    "Nutcracker": (34, 25, "#b9cdb5", [("dot", "#7a6d48", 24, (0.8, 1.8), "all"), ("fleck", "#8d8260", 40, (0.4, 0.8), "all")], .14),
    "FeralPigeon": (39, 29, "#f2f0ea", [], .10),
    "WoodPigeon": (41, 30, "#f4f2ec", [], .10),
    "HerringGull": (70, 50, "#8c7e55", [("blotch", "#2c2314", 26, (2.5, 5.5), "all"), ("dot", "#4a3d22", 50, (1.0, 2.0), "all")], .15),
    "BlackHeadedGull": (52, 37, "#8f8758", [("blotch", "#2a2014", 30, (1.8, 4.0), "all"), ("dot", "#463b22", 50, (0.8, 1.6), "all")], .15),
    "CommonGull": (57, 41, "#8a8452", [("blotch", "#2b2114", 28, (2.0, 4.5), "all"), ("dot", "#463b22", 50, (0.9, 1.8), "all")], .15),
    "GreatTit": (18, 14, "#f4efe6", [("dot", "#a8553a", 26, (0.5, 1.1), "blunt"), ("fleck", "#c27b5e", 30, (0.3, 0.6), "all")], .13),
    "BlueTit": (16, 12, "#f6f1e8", [("dot", "#b0603f", 22, (0.4, 0.9), "blunt"), ("fleck", "#c98a6a", 24, (0.3, 0.5), "all")], .13),
    "Goldfinch": (17, 12.5, "#c8dde0", [("dot", "#4a2a20", 9, (0.4, 0.8), "blunt"), ("scribble", "#6b3d34", 3, (3, 6), "blunt")], .13),
    "Chaffinch": (19, 14, "#aab9b6", [("scribble", "#5a2f3a", 9, (6, 14), "all"), ("dot", "#7a4a4c", 12, (0.5, 1.0), "all")], .13),
    "Bullfinch": (20, 15, "#a9d2d0", [("dot", "#3d2a2a", 11, (0.4, 0.9), "blunt"), ("scribble", "#503038", 3, (4, 8), "blunt")], .13),
    "HouseSparrow": (22, 15, "#dcd8cc", [("dot", "#6a5f55", 70, (0.5, 1.2), "all"), ("fleck", "#8d8478", 80, (0.3, 0.6), "all")], .13),
    "TreeSparrow": (20, 14.5, "#d6d2c6", [("dot", "#5b5048", 90, (0.5, 1.2), "all"), ("fleck", "#7d7266", 80, (0.3, 0.6), "all")], .13),
    "Starling": (30, 21, "#b8dfe0", [], .12),
    "SongThrush": (27, 20, "#6fc3d9", [("dot", "#14121a", 16, (0.6, 1.3), "all"), ("fleck", "#2a2430", 18, (0.3, 0.6), "all")], .13),
    "Fieldfare": (29, 21, "#9dc4b0", [("fleck", "#8d4d36", 230, (0.4, 0.9), "all"), ("dot", "#7a4631", 40, (0.6, 1.2), "all")], .13),
    "Blackbird": (29, 21, "#8fc6b3", [("fleck", "#92563c", 260, (0.4, 0.9), "all"), ("dot", "#7f4a34", 40, (0.6, 1.1), "all")], .13),
    "Robin": (20, 15.5, "#f3efe6", [("fleck", "#b86a50", 140, (0.3, 0.7), "wreath"), ("fleck", "#d09a82", 100, (0.3, 0.6), "all")], .13),
    "WhiteWagtail": (20, 15, "#d9d6cf", [("fleck", "#6e665c", 150, (0.3, 0.7), "all"), ("dot", "#8a8174", 30, (0.5, 0.9), "blunt")], .13),
    "CommonSwift": (25, 16, "#f6f4ee", [], .17),
    "BarnSwallow": (19.5, 13.5, "#f3efe8", [("dot", "#a64b3a", 28, (0.6, 1.3), "wreath"), ("fleck", "#c5826c", 30, (0.3, 0.6), "all")], .13),
    "SandMartin": (18, 12.5, "#f7f5f0", [], .13),
    "Cormorant": (60, 38, "#bcd7d0", [("blotch", "#e9efe8", 16, (4, 9), "all"), ("fleck", "#d8e6de", 60, (1, 2), "all")], .13),
    "MuteSwan": (113, 73, "#cfd6c0", [("blotch", "#c4cdb2", 12, (6, 14), "all")], .10),
    "Sparrowhawk": (40, 32, "#efeee8", [("blotch", "#9a4a2c", 14, (3, 7), "all"), ("dot", "#b86a48", 40, (1, 2), "all")], .10),
    "Peregrine": (52, 41, "#a8633c", [("blotch", "#6a2e1b", 40, (1.5, 4), "all"), ("fleck", "#c58a5e", 140, (0.6, 1.4), "all")], .10),
    "WhiteTailedEagle": (74, 58, "#e6e2d4", [("blotch", "#cfc6ae", 14, (4, 9), "all")], .08),
    "GoldenEagle": (76, 60, "#e8e3d4", [("blotch", "#a58f6a", 12, (4, 10), "all"), ("fleck", "#b9a47e", 60, (1, 2), "all")], .08),
    "CommonBuzzard": (55, 43, "#d6ddd0", [("blotch", "#8a5a3a", 16, (3, 8), "all"), ("fleck", "#9a7a5c", 50, (1, 2), "all")], .09),
    "GreyHeron": (61, 43, "#a5cfc8", [], .14),
}
CELLS = {}      # name -> (x0, y0, w, h) texels, y up (v = y / SIZE)


def _alloc():
    used = np.zeros((SIZE // TILE, SIZE // TILE), bool)

    def put(name, w, h):
        tw, th = w // TILE, h // TILE
        for ty in range(0, SIZE // TILE - th + 1, th):
            for tx in range(0, SIZE // TILE - tw + 1, tw):
                if not used[ty:ty + th, tx:tx + tw].any():
                    used[ty:ty + th, tx:tx + tw] = True
                    CELLS[name] = (tx * TILE, ty * TILE, w, h)
                    return
        raise RuntimeError("atlas full")
    for n in PLANK_CELLS:
        put(n, 512, 512)
    for n in MAT_CELLS:
        put(n, 256, 256)
    for n in EGGS:
        put("egg_" + n, 128, 128)


_alloc()


class Atlas:
    """What lint_bird.lint(ob, atlas) wants: .size and .cells {key: (x0, y0, w, h)}"""
    size = SIZE
    cells = CELLS


# ---------------------------------------------------------------------------------------------- painting helpers
def hexc(h):
    h = h.lstrip("#")
    return np.array([int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)])


def ramp(t, stops):
    t = np.clip(t, 0, 1)
    xs = np.linspace(0, 1, len(stops))
    cs = np.array([hexc(s) for s in stops])
    return np.stack([np.interp(t, xs, cs[:, k]) for k in range(3)], -1)


def nz(seed, s, an=(1.0, 1.0), n=256):
    return tile_noise(n, seed, s, an)


def u01(a, k=0.22):
    return np.clip(0.5 + k * a, 0, 1)


def blur(img, s, n=256):
    k = np.fft.fftfreq(n)
    filt = np.exp(-((k[None, :] ** 2 + k[:, None] ** 2) * (s * math.pi) ** 2))
    return np.real(np.fft.ifft2(np.fft.fft2(img) * filt))


def strokes(rng, n, length, ang, jit, curl=0.0, size=256, step=1.0):
    img = np.zeros((size, size))
    for _ in range(n):
        x, y = rng.uniform(0, size, 2)
        a = ang + rng.normal(0, jit)
        for _k in range(max(2, int(length * rng.uniform(0.6, 1.4)))):
            img[int(y) % size, int(x) % size] += 1
            a += rng.normal(0, curl)
            x += math.cos(a) * step
            y += math.sin(a) * step
    return img


def voronoi(rng, npts, size=256):
    pts = rng.uniform(0, size, (npts, 2))
    yy, xx = np.mgrid[0:size, 0:size]
    d1 = np.full((size, size), 1e9)
    d2 = np.full((size, size), 1e9)
    idx = np.zeros((size, size), int)
    for i, (px, py) in enumerate(pts):
        dx = np.abs(xx - px)
        dx = np.minimum(dx, size - dx)
        dy = np.abs(yy - py)
        dy = np.minimum(dy, size - dy)
        d = np.sqrt(dx * dx + dy * dy)
        m = d < d1
        d2 = np.where(m, d1, np.minimum(d2, d))
        idx = np.where(m, i, idx)
        d1 = np.where(m, d, d1)
    return d1, d2, idx


def _out(rgb, h, rough, ns):
    return rgb, h, rough, ns


# ---------------------------------------------------------------------------------------------- material cells
def p_bark(name, seed, stops, flakes=False, green=False):
    rng = np.random.default_rng(seed)
    streak = nz(seed + 1, 2.6, (1, 9))
    broad = nz(seed + 2, 14)
    ridge = nz(seed + 3, 2.2, (1, 12))
    t = u01(streak * 0.7 + broad * 0.45)
    rgb = ramp(t, stops)
    crev = np.clip((-ridge - 0.7) * 1.6, 0, 1)
    rgb = rgb * (1 - 0.55 * crev[..., None])
    # lenticels: short pale dashes
    dash = strokes(rng, 60, 5, 0.0, 0.1)
    dash = blur(dash, 0.8) * 3
    rgb = rgb + np.clip(dash, 0, 1)[..., None] * 0.10
    if flakes:
        v, v2, _ = voronoi(rng, 28)
        edge = np.clip((v2 - v) / 3.0, 0, 1)
        rgb = rgb * (0.65 + 0.35 * edge[..., None])
    if green:
        leaf = np.clip(nz(seed + 9, 5) - 0.6, 0, 1)
        rgb = rgb * (1 - leaf[..., None] * 0.2) + hexc("#8aa84a") * leaf[..., None] * 0.2
    h = u01(ridge * 0.6 + streak * 0.4) - crev * 0.4
    return rgb, h, 0.88 - 0.1 * crev, 6.0


def p_grass(name, seed, stops, bright):
    rng = np.random.default_rng(seed)
    st = nz(seed, 1.5, (1, 14))
    st2 = nz(seed + 1, 3.0, (1, 20))
    blade = u01(st * 0.8 + st2 * 0.4)
    rgb = ramp(blade, stops)
    lines = np.clip(strokes(rng, 140, 90, math.pi / 2, 0.06, 0.02), 0, 1)
    lines = blur(lines, 0.7) * 4
    rgb = rgb + np.clip(lines, 0, 1)[..., None] * bright
    # dark gaps between blades
    gap = np.clip((-st - 0.9) * 1.5, 0, 1)
    rgb = rgb * (1 - 0.45 * gap[..., None])
    return rgb, blade * 0.6 + lines * 0.2, np.full((256, 256), 0.8), 7.0


def p_moss(name, seed, stops):
    c1 = nz(seed, 6)
    c2 = nz(seed + 1, 2.2)
    c3 = nz(seed + 2, 1.0)
    t = u01(c1 * 0.5 + c2 * 0.6 + c3 * 0.25)
    rgb = ramp(t, stops)
    return rgb, u01(c1 * 0.4 + c2 * 0.5 + c3 * 0.3), np.full((256, 256), 0.95), 9.0


def p_lichen(name, seed):
    rng = np.random.default_rng(seed)
    base = ramp(u01(nz(seed, 8)), ["#3d3c30", "#575847", "#6e6f58"])
    v, v2, idx = voronoi(rng, 70)
    sel = (rng.random(70) < 0.55)[idx]
    flake = np.clip((v2 - v) / 2.5, 0, 1) * sel
    crust = ramp(u01(nz(seed + 1, 2.5)), ["#a9b595", "#c7cfb0", "#dfe3cb"])
    m = np.clip(flake * 1.4 + np.clip(nz(seed + 2, 5) - 0.6, 0, 1) * 0.6, 0, 1)
    rgb = base * (1 - m[..., None]) + crust * m[..., None]
    return rgb, m * 0.8 + u01(nz(seed + 3, 1.2)) * 0.2, np.full((256, 256), 0.9), 8.0


def p_mud(name, seed, wet):
    rng = np.random.default_rng(seed)
    if wet:
        t = u01(nz(seed, 9) * 0.7 + nz(seed + 1, 2.0) * 0.3)
        rgb = ramp(t, ["#2b1f16", "#4a362a", "#69533f"])
        sheen = np.clip(nz(seed + 2, 4) - 0.5, 0, 1)
        h = u01(nz(seed, 9) * 0.6 + nz(seed + 1, 2.0) * 0.2)
        return rgb, h, 0.28 + 0.4 * (1 - sheen), 5.0
    v, v2, idx = voronoi(rng, 46)
    bump = np.clip(v / 14.0, 0, 1)
    crack = np.clip((v2 - v) / 2.2, 0, 1)
    cellc = rng.random(46)[idx]
    t = u01(nz(seed, 5) * 0.5 + (cellc - 0.5) * 1.2)
    rgb = ramp(t, ["#6f5a43", "#9a8265", "#b9a583"])
    rgb = rgb * (0.45 + 0.55 * crack[..., None]) * (0.85 + 0.15 * (1 - bump[..., None]))
    return rgb, (1 - bump) * 0.8 + crack * 0.2, np.full((256, 256), 0.9), 10.0


def p_reed(name, seed):
    rng = np.random.default_rng(seed)
    st = nz(seed, 1.4, (1, 14))
    t = u01(st * 0.7 + nz(seed + 1, 4, (1, 3)) * 0.4)
    rgb = ramp(t, ["#8a7340", "#c1a967", "#e1cf94"])
    y = np.arange(256)[:, None] * np.ones((1, 256))
    node = np.zeros((256, 256))
    for ny in (30, 100, 170, 235):
        node += np.exp(-((y - ny - 6 * nz(seed + ny, 6)[0][None, :]) ** 2) / 5.0)
    rgb = rgb * (1 - 0.45 * np.clip(node, 0, 1)[..., None])
    groove = np.clip((-st - 0.6) * 1.5, 0, 1)
    rgb = rgb * (1 - 0.3 * groove[..., None])
    return rgb, u01(st) * 0.5 + node * 0.4, np.full((256, 256), 0.7), 6.0


def p_down(name, seed):
    rng = np.random.default_rng(seed)
    base = ramp(u01(nz(seed, 6)), ["#bdb8ad", "#dcd8cd", "#efece4"])
    s = strokes(rng, 900, 24, rng.uniform(0, 6.28), 6.28, 0.18)
    s = np.clip(blur(s, 0.8) * 5, 0, 1)
    s2 = np.clip(blur(strokes(rng, 500, 20, 0, 6.28, 0.2), 0.8) * 5, 0, 1)
    rgb = base * (1 - 0.35 * s2[..., None]) + s[..., None] * 0.18
    return np.clip(rgb, 0, 1), s * 0.6 + u01(nz(seed + 1, 3)) * 0.3, np.full((256, 256), 0.95), 7.0


def p_feather(name, seed):
    rng = np.random.default_rng(seed)
    y, x = np.mgrid[0:256, 0:256].astype(float)
    ang = math.radians(32)
    barb = np.sin((x * math.cos(ang) + y * math.sin(ang)) * 2 * math.pi / 5.0 + nz(seed, 6) * 1.2)
    fine = nz(seed + 5, 1.2)
    t = u01(nz(seed + 1, 10) * 0.8 + barb * 0.10 + fine * 0.15)
    rgb = ramp(t, ["#8d7f6b", "#b5a78e", "#e0d6c0"])
    rgb = rgb * (0.95 + 0.05 * barb[..., None])
    return rgb, u01(barb * 0.6), np.full((256, 256), 0.85), 4.0


def p_guano(name, seed):
    rng = np.random.default_rng(seed)
    drip = nz(seed, 3.0, (1.0, 10.0))
    t = u01(drip * 0.6 + nz(seed + 1, 12) * 0.4, 0.2)
    rgb = ramp(t, ["#b9b49f", "#e6e3d6", "#fbfaf4"])
    stain = np.clip(nz(seed + 2, 7) - 0.5, 0, 1)
    rgb = rgb * (1 - stain[..., None] * 0.25) + hexc("#a8a06a") * stain[..., None] * 0.18
    return np.clip(rgb, 0, 1), u01(drip * 0.5 + nz(seed + 3, 2) * 0.2), 0.55 + 0.3 * u01(nz(seed + 4, 4)), 5.0


def p_clay(name, seed):
    t = u01(nz(seed, 8) * 0.6 + nz(seed + 1, 2.4) * 0.5)
    rgb = ramp(t, ["#6c4a2c", "#9c6c3d", "#c58f56"])
    scuff = np.clip(nz(seed + 2, 5, (3, 1)) - 0.7, 0, 1)
    rgb = rgb * (1 - 0.25 * scuff[..., None]) + hexc("#d8b88a") * scuff[..., None] * 0.2
    return rgb, u01(nz(seed + 3, 3)) * 0.8, np.full((256, 256), 0.9), 7.0


def p_pebbles(name, seed):
    rng = np.random.default_rng(seed)
    v, v2, idx = voronoi(rng, 90)
    cols = [hexc(c) for c in ("#6b6a66", "#8b8578", "#a89f8c", "#5a5650", "#9a8d7a", "#7c7f7a", "#b4a690")]
    pick = rng.integers(0, len(cols), 90)
    rgb = np.stack([np.array([cols[pick[i]][k] for i in range(90)])[idx] for k in range(3)], -1)
    rgb = rgb * (0.85 + 0.3 * u01(nz(seed + 1, 1.5))[..., None])
    gap = np.clip((v2 - v) / 2.2, 0, 1)
    rgb = rgb * (0.35 + 0.65 * gap[..., None])
    dome = np.clip(1 - v / 17.0, 0, 1)
    return rgb, dome * gap, np.full((256, 256), 0.85), 12.0


def p_earth(name, seed):
    rng = np.random.default_rng(seed)
    t = u01(nz(seed, 7) * 0.6 + nz(seed + 1, 1.6) * 0.5)
    rgb = ramp(t, ["#1f160f", "#3a2a1c", "#5c4630"])
    v, v2, idx = voronoi(rng, 130)
    grit = (rng.random(130) < 0.15)[idx] * np.clip(1 - v / 7, 0, 1)
    rgb = rgb * (1 - grit[..., None]) + hexc("#8a7f6c") * grit[..., None]
    return rgb, u01(nz(seed + 2, 2)) * 0.8 + grit * 0.2, np.full((256, 256), 0.95), 8.0


def p_wool(name, seed):
    rng = np.random.default_rng(seed)
    base = ramp(u01(nz(seed, 7)), ["#a79f8c", "#c9c0ac", "#e8e2d3"])
    s = np.clip(blur(strokes(rng, 700, 36, 0, 6.28, 0.3), 0.8) * 4, 0, 1)
    s2 = np.clip(blur(strokes(rng, 500, 30, 0, 6.28, 0.3), 0.8) * 4, 0, 1)
    rgb = base * (1 - 0.3 * s2[..., None]) + s[..., None] * 0.15
    return np.clip(rgb, 0, 1), s * 0.7 + 0.2, np.full((256, 256), 0.95), 7.0


def p_rootlet(name, seed):
    rng = np.random.default_rng(seed)
    base = ramp(u01(nz(seed, 5)), ["#120c08", "#2a1c11", "#463120"])
    s = np.clip(blur(strokes(rng, 700, 60, math.pi / 2, 1.2, 0.12), 0.7) * 4, 0, 1)
    rgb = base * (0.6 + 0.8 * s[..., None])
    return np.clip(rgb, 0, 1), s * 0.7, np.full((256, 256), 0.8), 8.0


def p_seaweed(name, seed):
    st = nz(seed, 2.5, (1, 3))
    t = u01(st * 0.8 + nz(seed + 1, 9) * 0.5)
    rgb = ramp(t, ["#161d0d", "#33411a", "#5b5a2a", "#7a7040"])
    wr = nz(seed + 2, 1.4, (1, 2))
    rgb = rgb * (0.8 + 0.2 * u01(wr)[..., None])
    return rgb, u01(st * 0.6 + wr * 0.4), 0.45 + 0.3 * u01(nz(seed + 3, 3)), 6.0


def p_leaf(name, seed):
    rng = np.random.default_rng(seed)
    v, v2, idx = voronoi(rng, 16)
    cellc = rng.random(16)[idx]
    rgb = ramp(u01(cellc * 1.2 - 0.1 + nz(seed, 3) * 0.3, 1.0), ["#3a2410", "#6c4219", "#a8772f", "#8a5a22"])
    vein = np.clip(1 - (v2 - v) / 2.0, 0, 1) * 0.35
    rgb = rgb * (1 - vein[..., None] * 0.5)
    return rgb, 1 - vein, np.full((256, 256), 0.8), 6.0


def p_soot(name, seed):
    t = u01(nz(seed, 4) * 0.8 + nz(seed + 1, 1.5) * 0.4)
    rgb = ramp(t, ["#040302", "#0b0806", "#17110c"])
    return rgb, t * 0.5, np.full((256, 256), 0.95), 3.0


def p_wood_raw(name, seed):
    g = nz(seed, 2.0, (1, 14))
    t = u01(g * 0.7 + nz(seed + 1, 10) * 0.4)
    rgb = ramp(t, ["#2f2217", "#4f3d28", "#76603f"])
    return rgb, u01(g), np.full((256, 256), 0.85), 5.0


def p_treebark(name, seed):
    ridge = nz(seed, 4.5, (1.0, 8))
    fine = nz(seed + 1, 1.6, (1, 6))
    h = u01(ridge * 0.9 + fine * 0.3)
    rgb = ramp(h, ["#1d1916", "#3a332c", "#5d5448", "#7d7160"])
    rgb = rgb * (0.88 + 0.12 * u01(nz(seed + 2, 12))[..., None])
    return rgb, h, np.full((256, 256), 0.92), 14.0


def p_wood_cut(name, seed):
    g = nz(seed, 3, (1, 10))
    rgb = ramp(u01(g * 0.6 + nz(seed + 1, 8) * 0.4), ["#a9885a", "#c9aa74", "#e0c898"])
    ring = 0.5 + 0.5 * np.sin((np.arange(256)[None, :] + 20 * nz(seed + 2, 9)) * 0.35)
    rgb = rgb * (0.9 + 0.1 * ring[..., None])
    return rgb, u01(g) * 0.5, np.full((256, 256), 0.8), 4.0


def p_plank(name, seed, col, under="#7b7567"):
    n = 512
    rng = np.random.default_rng(seed)
    x = np.arange(n)[None, :] * np.ones((n, 1))
    bid = (x // 256).astype(int)                      # two boards of 256 px (~105 mm)
    tone = np.array([1.0, 0.92])[bid]
    grain = nz(seed, 2.2, (1, 12), n=n)
    broad = nz(seed + 1, 20, n=n)
    wood = ramp(u01(grain * 0.7 + broad * 0.4), ["#4a4338", "#7b7567", "#a39d8c"])
    paint = hexc(col)[None, None, :] * (0.88 + 0.12 * u01(broad)[..., None]) * tone[..., None]
    paint = paint * (0.95 + 0.05 * u01(grain)[..., None])
    wear = np.clip(nz(seed + 2, 9, (1, 4), n=n) * 0.8 + nz(seed + 3, 3, (1, 6), n=n) * 0.4 - 0.55, 0, 1)
    wear = np.clip(wear * 3.0, 0, 1)
    rgb = paint * (1 - wear[..., None]) + wood * wear[..., None]
    # dirt streaks running down the boards
    dirt = np.clip(nz(seed + 4, 3, (1, 14), n=n) - 0.3, 0, 1) * 0.35
    rgb = rgb * (1 - dirt[..., None])
    # seams between boards
    seam = np.exp(-(((x % 256) - 1.5) ** 2) / 3.0)
    rgb = rgb * (1 - 0.8 * seam[..., None])
    # nail heads
    for nx in (128, 384):
        for ny in (24, 488):
            r2 = (x - nx) ** 2 + (np.arange(n)[:, None] - ny) ** 2
            rgb = rgb * (1 - 0.7 * np.exp(-r2 / 6.0)[..., None])
    h = u01(grain * 0.5) * 0.4 + (1 - wear) * 0.5 - seam * 0.6
    return rgb, h, 0.55 + 0.4 * wear, 5.0


PAINTERS = {
    "bark_brown": lambda s: p_bark("bark_brown", s, ["#2a1f17", "#4b3a2b", "#765d44"]),
    "bark_grey": lambda s: p_bark("bark_grey", s, ["#3b3935", "#6a655d", "#9a948a"]),
    "bark_dark": lambda s: p_bark("bark_dark", s, ["#14100c", "#2a2018", "#4a3b2c"]),
    "bark_silver": lambda s: p_bark("bark_silver", s, ["#605e57", "#9a978d", "#c8c5b9"]),
    "bark_pale": lambda s: p_bark("bark_pale", s, ["#6a4e36", "#a9805a", "#cfa878"], flakes=True),
    "bark_green": lambda s: p_bark("bark_green", s, ["#2a3d17", "#476328", "#7a9a40"], green=True),
    "grass_dry": lambda s: p_grass("grass_dry", s, ["#5e4c24", "#a08748", "#d2b97a"], 0.10),
    "grass_fine": lambda s: p_grass("grass_fine", s, ["#7c6d3a", "#b7a468", "#e0d19c"], 0.10),
    "grass_bleached": lambda s: p_grass("grass_bleached", s, ["#85806c", "#b7b29d", "#dedacb"], 0.08),
    "moss_green": lambda s: p_moss("moss_green", s, ["#16300e", "#33591b", "#699631", "#9fc052"]),
    "moss_dry": lambda s: p_moss("moss_dry", s, ["#2e2b18", "#59502a", "#8c8247", "#aaa066"]),
    "lichen": lambda s: p_lichen("lichen", s),
    "mud_wet": lambda s: p_mud("mud_wet", s, True),
    "mud_dry": lambda s: p_mud("mud_dry", s, False),
    "reed": lambda s: p_reed("reed", s),
    "down": lambda s: p_down("down", s),
    "feather": lambda s: p_feather("feather", s),
    "guano": lambda s: p_guano("guano", s),
    "clay": lambda s: p_clay("clay", s),
    "pebbles": lambda s: p_pebbles("pebbles", s),
    "earth": lambda s: p_earth("earth", s),
    "wool": lambda s: p_wool("wool", s),
    "rootlet": lambda s: p_rootlet("rootlet", s),
    "seaweed": lambda s: p_seaweed("seaweed", s),
    "leaf_dead": lambda s: p_leaf("leaf_dead", s),
    "soot": lambda s: p_soot("soot", s),
    "wood_raw": lambda s: p_wood_raw("wood_raw", s),
    "treebark": lambda s: p_treebark("treebark", s),
    "wood_cut": lambda s: p_wood_cut("wood_cut", s),
    "plank_blue": lambda s: p_plank("plank_blue", s, "#5f8590"),
    "plank_white": lambda s: p_plank("plank_white", s, "#d3d2c4"),
    "plank_ochre": lambda s: p_plank("plank_ochre", s, "#94422d"),
}


# ---------------------------------------------------------------------------------------------- eggs
def _stamp(img, cx, cy, rx, ry, col, alpha, size):
    x0, x1 = int(cx - rx * 2.2) - 1, int(cx + rx * 2.2) + 2
    y0, y1 = max(0, int(cy - ry * 2.2) - 1), min(size, int(cy + ry * 2.2) + 2)
    if y1 <= y0:
        return
    xs = np.arange(x0, x1)
    ys = np.arange(y0, y1)
    d = ((xs[None, :] - cx) / max(rx, 0.3)) ** 2 + ((ys[:, None] - cy) / max(ry, 0.3)) ** 2
    a = np.clip((1.25 - d) * 2.2, 0, 1) * alpha
    xi = xs % size
    sub = img[y0:y1][:, xi]
    img[y0:y1, xi] = sub * (1 - a[..., None]) + col * a[..., None]


def paint_egg(name):
    L, B, ground, marks, _ = EGGS[name]
    S = 128
    seed = zlib.crc32(name.encode()) % 100000
    rng = np.random.default_rng(seed)
    sx = S / (math.pi * B)          # texels per mm round the egg
    sy = S / (L * 1.06)             # texels per mm along it
    g = hexc(ground)
    tex = np.ones((S, S, 3)) * g
    shade = nz(seed, 3.0, n=S) * 0.5 + nz(seed + 1, 0.9, n=S) * 0.5
    tex = tex * (1 + 0.035 * shade[..., None])
    for kind, col, count, (smin, smax), zone in marks:
        c = hexc(col)
        for _ in range(count):
            if zone == "blunt":
                cy = abs(rng.normal(0, 0.28)) * S * 0.9 + 3
            elif zone == "wreath":
                cy = rng.normal(0.22, 0.12) * S
            else:
                cy = rng.uniform(4, S - 4)
            cy = float(np.clip(cy, 2, S - 3))
            cx = rng.uniform(0, S)
            r = rng.uniform(smin, smax)
            if kind in ("dot", "fleck"):
                _stamp(tex, cx, cy, r * sx * 0.5, r * sy * 0.5, c, rng.uniform(0.7, 1.0), S)
            elif kind == "blotch":
                ang = rng.uniform(0, 6.28)
                for k in range(3):
                    _stamp(tex, cx + math.cos(ang) * k * r * sx * 0.35, cy + math.sin(ang) * k * r * sy * 0.35,
                           r * sx * 0.5 * rng.uniform(0.6, 1.0), r * sy * 0.5 * rng.uniform(0.6, 1.0), c,
                           rng.uniform(0.55, 0.95), S)
            elif kind == "scribble":
                a = rng.uniform(0, 6.28)
                x, y = cx, cy
                for k in range(int(r * 2)):
                    _stamp(tex, x, y, 0.35 * sx, 0.35 * sy, c, 0.85, S)
                    a += rng.normal(0, 0.55)
                    x += math.cos(a) * 0.5 * sx
                    y += math.sin(a) * 0.5 * sy
    h = 0.5 + 0.03 * nz(seed + 5, 1.2, n=S)
    return np.clip(tex, 0, 1), h, np.full((S, S), 0.38) + 0.1 * u01(nz(seed + 6, 2, n=S)) , 2.5


# ---------------------------------------------------------------------------------------------- whole atlas
def paint():
    C = np.zeros((SIZE, SIZE, 3))
    N = np.zeros((SIZE, SIZE, 3))
    N[...] = (0.5, 0.5, 1.0)
    R = np.full((SIZE, SIZE), 0.8)
    for name, (x0, y0, w, h) in CELLS.items():
        if name.startswith("egg_"):
            rgb, ht, ro, ns = paint_egg(name[4:])
        else:
            rgb, ht, ro, ns = PAINTERS[name](zlib.crc32(name.encode()) % 100000)
        C[y0:y0 + h, x0:x0 + w] = rgb
        R[y0:y0 + h, x0:x0 + w] = ro
        N[y0:y0 + h, x0:x0 + w] = normal_from_height(ht, ns)
    to8 = lambda a: np.clip(a * 255 + 0.5, 0, 255).astype(np.uint8)
    return to8(C), to8(N), to8(np.repeat(R[..., None], 3, -1))


def write_png(path, arr):
    """arr: (H, W, 3) uint8 with row 0 = BOTTOM (v = 0); written top-down as PNG wants."""
    a = np.ascontiguousarray(arr[::-1])
    h, w, _ = a.shape
    raw = b"".join(b"\x00" + a[y].tobytes() for y in range(h))

    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) \
        + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b"")
    with open(path, "wb") as fh:
        fh.write(png)


def build(outdir):
    os.makedirs(outdir, exist_ok=True)
    c, n, r = paint()
    write_png(os.path.join(outdir, "T_NestKit_C.png"), c)
    write_png(os.path.join(outdir, "T_NestKit_N.png"), n)
    write_png(os.path.join(outdir, "T_NestKit_R.png"), r)
    return {k: list(v) for k, v in CELLS.items()}


if __name__ == "__main__":
    import json
    cells = build(sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else ".")
    print(len(cells), "cells")
