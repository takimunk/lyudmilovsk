"""The descent craft's textures (docs/spacecraft.md): procedural, no outside source, the same seed gives the same
pictures (tools/spacecraft/textures.json pins the seed and the SHA-1 of every output; `--check` compares).

    python3 tools/spacecraft/textures.py [name ...] [--check]     -> textures/spacecraft/T_SC_*.png

  T_SC_<surface>_C.png  the kit's tiling surfaces (tx_sc_* slots, metre UVs): RGB colour (sRGB), A roughness
  T_SC_<surface>_N.png  their tangent normals (DirectX style, as Unreal wants them: on a bump red is brighter on its
                        right side, green on its lower side)
        paint_white 512 (2 m a tile)    worn off-white enamel: chips to the primer and the metal, scuffs, hand grime
        paint_green  512 (2 m)          the cabin's grey-green, the same wear
        metal        512 (1 m)          brushed aluminium, panel seams with rivets every 12.5 cm
        foil         512 (1 m)          crinkled gold foil
        quilt       1024 (2 pillows)    the padding: 2 x 2 of the kit's square pillows (its seams at 0 and 0.5), diamond
                                        stitching inside them
        shield       512 (1 m)          the ablator: filled honeycomb, charred in blotches
        rubber       512 (0.5 m)        ribbed black rubber
        fabric       512 (0.5 m)        woven strap
        chute        512 (two gores)    ripstop: U 0..0.5 orange, 0.5..1 white (a gore each), seams between; the
                                        canopy's UVs run 0..1 round and up, the material tiles them (gores / 2)
  T_SC_net_C.png        512 (0.25 m)    the stowage net: knotted cord, A = the cutout (no normal)
  (tile sizes are the kit's: Content/Spacecraft/kit.json "tiles" when it is there)
  T_SC_dials.png        the dial faces' atlas (rects, ranges and sweeps from Content/Spacecraft/kit.json when it is
                        there, else DIALS below); numerals are digits only
  T_SC_ball.png         the attitude ball, equirectangular (u = heading, v = pitch, +90 at the top)
  T_SC_digits.png       a seven-segment strip, 16 cells: 0..9, minus, blank, then spare (M_SC_Digits draws its
                        segments analytically; the strip is the fallback and the reference for the segment shapes)
  T_SC_glass.png        the porthole pane's masks (linear): R frost and G scorch as "arrival" fields (0 at the rim,
                        1 at the middle; the material thresholds them), B spray droplets (height), A rivulets (tiles in V)
  T_SC_noise.png        256, tiling, linear: R billows (1/f), G mid blotches, B streaks along V, A fine grain
  (the globe uses the planet's own T_EU_planet_C: nothing new; the island is tools/spacecraft/island_map.py's)
"""
import hashlib
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(REPO, "textures", "spacecraft")
PINS = os.path.join(HERE, "textures.json")
KIT_JSON = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "Spacecraft", "kit.json")
FONT = os.path.join(REPO, "fonts", "MPLUS1p-Bold.ttf")
sys.path.insert(0, os.path.join(REPO, "tools", "europa"))
import europa_textures as ET  # noqa: E402  (periodic_noise, band, smooth, streaks)

SEED = 7151
# one tile in metres (the import's TileInv = 1 / this) and the metalness of each tiling surface
TILE_M = {"paint_white": 2.0, "paint_green": 2.0, "metal": 1.0, "foil": 1.0, "quilt": 0.57923, "shield": 1.0,
          "rubber": 0.5, "fabric": 0.5, "net": 0.25}
METAL = {"metal": 0.9, "foil": 1.0}
# the dials when kit.json does not say: id -> (min, max, sweep degrees, major step, minors per major, zones)
# zones: (from, to, colour) arcs just inside the scale; angles run clockwise from `start` (degrees from 12 o'clock)
GREEN, AMBER, RED = (0.25, 0.75, 0.3), (0.95, 0.65, 0.1), (0.9, 0.16, 0.1)
# the dials when kit.json is not there (blender/spacecraft/spacecraft_kit.py DIALS): rect [x, y, w, h] px of 1024,
# angles in degrees clockwise from 12 o'clock, major and minor steps in the dial's units; show = how much of the
# rect's inscribed circle the bezel leaves visible
DIALS = {
    "alt": dict(rect=[0, 0, 512, 512], min=0, max=10, a_min=0, a_max=360, major=1, minor=0.2, show=0.934),
    "gmeter": dict(rect=[512, 0, 512, 512], min=0, max=6, a_min=-135, a_max=135, major=1, minor=0.2, show=0.922),
    "press": dict(rect=[0, 512, 256, 256], min=0, max=1000, a_min=-120, a_max=120, major=200, minor=50, show=0.871),
    "o2": dict(rect=[256, 512, 256, 256], min=0, max=40, a_min=-120, a_max=120, major=10, minor=2, show=0.871),
    "co2": dict(rect=[512, 512, 256, 256], min=0, max=3, a_min=-120, a_max=120, major=1, minor=0.2, show=0.871),
    "temp": dict(rect=[768, 512, 256, 256], min=0, max=40, a_min=-120, a_max=120, major=10, minor=2, show=0.871),
    "volts": dict(rect=[0, 768, 256, 256], min=20, max=32, a_min=-120, a_max=120, major=2, minor=0.5, show=0.871),
    "fuel": dict(rect=[256, 768, 256, 256], min=0, max=100, a_min=-120, a_max=120, major=25, minor=5, show=0.871),
}
# the coloured arcs (not the kit's business): (from, to, colour) in the dial's units
ZONES = {"gmeter": [(0, 2, GREEN), (4, 5, AMBER), (5, 6, RED)], "press": [(700, 800, GREEN), (0, 400, RED)],
         "o2": [(18, 26, GREEN), (0, 14, RED)], "co2": [(0, 1, GREEN), (2, 3, RED)], "temp": [(16, 26, GREEN), (32, 40, RED)],
         "volts": [(26, 30, GREEN), (20, 23, RED)], "fuel": [(0, 10, RED), (10, 25, AMBER)]}
smooth = ET.smooth
_written = {}


def srgb(c):
    return np.array(c, dtype=np.float32)


def save(name, arr):
    """arr float 0..1, HxW, HxWx3 or HxWx4."""
    os.makedirs(OUT, exist_ok=True)
    img = (np.clip(arr, 0, 1) * 255 + 0.5).astype(np.uint8)
    Image.fromarray(img).save(os.path.join(OUT, name), optimize=False, compress_level=6)
    _written[name] = {"sha1": hashlib.sha1(img.tobytes()).hexdigest(), "size": [int(img.shape[1]), int(img.shape[0])],
                      "channels": 1 if img.ndim == 2 else int(img.shape[2])}
    print("wrote", name, img.shape)


def save_ca(name, rgb, rough):
    save(name, np.concatenate([rgb, np.clip(rough, 0, 1)[..., None]], -1))


def normal_map(h, bump):
    """Tangent normal of a tiling height field h (0..1-ish); bump = the slope a height step of 1 over the whole tile
    would have, times the picture's size. Unreal's green: the bitangent runs down the picture (V)."""
    n = h.shape[0]
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5 * n * bump
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5 * n * bump
    v = np.stack([-dx, -dy, np.ones_like(dx)], -1)
    v /= np.linalg.norm(v, axis=-1, keepdims=True)
    return v * 0.5 + 0.5


def blur(a, sigma):
    """Periodic Gaussian blur (FFT)."""
    n0, n1 = a.shape
    f0, f1 = np.fft.fftfreq(n0), np.fft.fftfreq(n1)
    g = np.exp(-2 * (math.pi * sigma) ** 2 * (f0[:, None] ** 2 + f1[None, :] ** 2))
    return np.real(np.fft.ifft2(np.fft.fft2(a) * g))


def norm01(a):
    a = a - a.min()
    return a / max(a.max(), 1e-9)


def scratches(n, seed, count, length, width=0.7):
    """Tiling short straight scratches, 0..1."""
    rng = np.random.default_rng(seed)
    a = np.zeros((n, n), np.float32)
    for _ in range(count):
        x, y = rng.uniform(0, n, 2)
        ang = rng.uniform(0, math.pi)
        ln = int(length * rng.uniform(0.3, 1.0))
        t = np.arange(ln)
        k = rng.uniform(0.4, 1.0) * np.sin(np.pi * (t + 0.5) / ln)
        np.maximum.at(a, ((y + t * math.sin(ang)).astype(int) % n, (x + t * math.cos(ang)).astype(int) % n), k)
    return np.clip(blur(a, width) * (1.0 + 2.0 * width), 0, 1)


def brushed(n, seed, cut=0.02):
    rng = np.random.default_rng(seed)
    b = np.real(np.fft.ifft(np.fft.fft(rng.normal(size=(n, n)), axis=1) * (np.abs(np.fft.fftfreq(n)) < cut)[None, :], axis=1))
    return norm01(b)


def uv(n):
    u, v = np.meshgrid((np.arange(n) + 0.5) / n, (np.arange(n) + 0.5) / n)
    return u, v


# ---------------------------------------------------------------------------------------------------------------------
# tiling surfaces


def _paint(name, n, base, seed, chip_lo=0.845):
    tone = ET.periodic_noise(n, 1.5, seed + 1)
    fine = ET.periodic_noise(n, 0.5, seed + 2)
    field = ET.band(n, 5, 36, seed + 3) * 0.7 + ET.periodic_noise(n, 0.9, seed + 4) * 0.3
    chips = smooth(chip_lo, chip_lo + 0.025, field)
    primer = smooth(chip_lo - 0.03, chip_lo - 0.012, field) - chips
    grime = smooth(0.5, 0.9, ET.periodic_noise(n, 1.9, seed + 5)) * 0.5
    scr = scratches(n, seed + 6, 22 * n // 512, n * 0.10) * 0.7
    yellow = smooth(0.35, 0.8, ET.band(n, 1, 4, seed + 7))
    b = srgb(base)
    col = b[None, None, :] * (0.93 + 0.12 * tone[..., None])
    col = col * (1 - 0.07 * yellow[..., None] * srgb((0.0, 0.25, 1.0)))          # old enamel goes a little yellow
    col = col * (1 - 0.35 * grime[..., None]) + srgb((0.20, 0.17, 0.13)) * (0.35 * grime[..., None])
    col = col * (1 - 0.30 * scr[..., None]) + srgb((0.30, 0.29, 0.27)) * (0.30 * scr[..., None])
    col = col * (1 - primer[..., None]) + srgb((0.52, 0.30, 0.20)) * primer[..., None]
    col = col * (1 - chips[..., None]) + (srgb((0.46, 0.46, 0.47)) * (0.85 + 0.3 * fine[..., None])) * chips[..., None]
    rough = 0.42 + 0.18 * tone + 0.25 * grime + 0.2 * scr + 0.15 * primer - 0.12 * chips
    save_ca("T_SC_%s_C.png" % name, col, rough)
    h = 0.02 * fine + 0.01 * tone - 0.05 * primer - 0.09 * chips - 0.03 * scr
    save("T_SC_%s_N.png" % name, normal_map(h, 0.022))


def paint_white():
    _paint("paint_white", 512, (0.84, 0.83, 0.78), SEED + 10, chip_lo=0.80)


def paint_green():
    _paint("paint_green", 512, (0.37, 0.47, 0.42), SEED + 20, chip_lo=0.79)


def metal(n=512):
    u, v = uv(n)
    tone = ET.periodic_noise(n, 1.6, SEED + 31)
    br = brushed(n, SEED + 32)
    brf = brushed(n, SEED + 33, cut=0.12)
    scr = scratches(n, SEED + 34, 40, n * 0.25)
    # panel seams along the tile's edges and through its middle one way; rivets beside them
    du = np.minimum(u, 1 - u)
    dv = np.minimum(np.abs(v - 0.5), np.minimum(v, 1 - v))
    seam = np.maximum(1 - smooth(0.0, 0.004, du), 1 - smooth(0.0, 0.004, dv))
    riv = np.zeros((n, n), np.float32)
    rr = 0.011
    for k in range(8):
        c = (k + 0.5) / 8
        for (cu, cv) in ((0.035, c), (0.965, c), (c, 0.035), (c, 0.965), (c, 0.465), (c, 0.535)):
            d = np.hypot(np.minimum(np.abs(u - cu), 1 - np.abs(u - cu)), np.minimum(np.abs(v - cv), 1 - np.abs(v - cv)))
            riv = np.maximum(riv, np.sqrt(np.clip(1 - (d / rr) ** 2, 0, 1)))
    rim = np.clip(blur(riv, 2.0) * 1.6 - riv, 0, 1)                             # grime round each head
    val = 0.56 + 0.07 * (tone - 0.5) + 0.07 * (br - 0.5) + 0.04 * (brf - 0.5) + 0.05 * scr
    col = np.stack([val, val * 1.005, val * 1.02], -1)
    dirt = np.clip(seam * 0.8 + rim * 0.6, 0, 1)
    col = col * (1 - 0.6 * dirt[..., None]) + srgb((0.10, 0.09, 0.08)) * (0.6 * dirt[..., None])
    col = col * (1 + 0.10 * (riv[..., None] > 0.3))
    rough = 0.36 + 0.14 * br + 0.1 * tone + 0.3 * dirt - 0.08 * riv
    save_ca("T_SC_metal_C.png", col, rough)
    h = 0.012 * brf + 0.25 * riv - 0.12 * seam - 0.02 * scr
    save("T_SC_metal_N.png", normal_map(h, 0.03))


def _facets(n, count, seed, tilt):
    """A tiling sheet of flat facets (one per random point, each at its own tilt) meeting in sharp creases: its height
    and each pixel's distance to the nearest crease in pixels."""
    from scipy.spatial import cKDTree
    rng = np.random.default_rng(seed)
    pts = rng.random((count, 2))
    slope = rng.normal(size=(count, 2)) * tilt
    off = rng.normal(size=count) * tilt * 0.04
    u, v = uv(n)
    q = np.stack([u.ravel(), v.ravel()], -1)
    d, i = cKDTree(pts, boxsize=1.0).query(q, k=2)
    i0 = i[:, 0]
    dp = q - pts[i0]
    dp -= np.round(dp)
    h = (dp * slope[i0]).sum(-1) + off[i0]
    gap = pts[i[:, 1]] - pts[i0]
    gap -= np.round(gap)
    edge = (d[:, 1] ** 2 - d[:, 0] ** 2) / (2.0 * np.maximum(np.linalg.norm(gap, axis=-1), 1e-6))
    return h.reshape(n, n).astype(np.float32), (edge * n).reshape(n, n).astype(np.float32)


def foil(n=512):
    # crumpled and smoothed out again: flat facets at two sizes, sharp creases between them
    h1, e1 = _facets(n, 260, SEED + 41, 2.0)
    h2, e2 = _facets(n, 1500, SEED + 42, 1.6)
    h = blur(h1 + 0.35 * h2, 0.8)
    crease = np.maximum(1 - smooth(0.0, 1.6, e1), 0.6 * (1 - smooth(0.0, 1.1, e2)))
    nm = normal_map(h, 0.38)
    # a baked hint of the reflections (the engine's own do the rest): each facet its own brightness by its tilt
    tilt = (nm[..., 0] - 0.5) * 1.1 - (nm[..., 1] - 0.5) * 1.5
    gold = srgb((0.86, 0.62, 0.20))
    col = gold[None, None, :] * np.clip(0.86 + 0.45 * tilt[..., None], 0.45, 1.25)
    col = col * (1 - 0.22 * crease[..., None]) + srgb((0.97, 0.84, 0.50)) * (0.22 * crease[..., None])
    rough = 0.20 + 0.22 * crease + 0.08 * ET.periodic_noise(n, 1.0, SEED + 45)
    save_ca("T_SC_foil_C.png", col, rough)
    save("T_SC_foil_N.png", nm)


def quilt(n=1024):
    """Two by two of the kit's pillows (their seams at 0 and 0.5 both ways, where the mesh folds in), each stitched
    in diamonds; the pillow's own bulge is the mesh's, the diamonds' is here."""
    u, v = uv(n)
    N = 8
    a, b = (u + v) * N, (u - v) * N
    fa, fb = a - np.floor(a), b - np.floor(b)
    pillow = np.sqrt(np.clip(np.sin(math.pi * fa) * np.sin(math.pi * fb), 0, 1))
    ds = np.minimum(np.minimum(fa, 1 - fa), np.minimum(fb, 1 - fb))        # to the nearest stitch line, in diamonds
    groove = 1 - smooth(0.0, 0.07, ds)
    dash_a = (np.sin(b * 2 * math.pi * 7) > -0.2) & (np.minimum(fa, 1 - fa) < 0.02)
    dash_b = (np.sin(a * 2 * math.pi * 7) > -0.2) & (np.minimum(fb, 1 - fb) < 0.02)
    stitch = (dash_a | dash_b).astype(np.float32)
    # the seams between the pillows: a deeper fold, a piped edge
    su = np.minimum(np.abs(np.mod(u * 2 + 0.5, 1) - 0.5), np.abs(np.mod(v * 2 + 0.5, 1) - 0.5)) / 2
    seam = 1 - smooth(0.0, 0.012, su)
    weave = 0.5 + 0.25 * np.sin(u * 2 * math.pi * 200) + 0.25 * np.sin(v * 2 * math.pi * 200)
    wr = ET.band(n, 20, 60, SEED + 51)
    tone = ET.periodic_noise(n, 1.6, SEED + 52)
    dirt = smooth(0.55, 0.9, ET.periodic_noise(n, 1.8, SEED + 53))
    base = srgb((0.70, 0.69, 0.62))
    col = base[None, None, :] * (0.80 + 0.22 * pillow[..., None] + 0.06 * (weave[..., None] - 0.5) + 0.08 * (tone[..., None] - 0.5))
    col = col * (1 - 0.22 * dirt[..., None] * srgb((0.6, 0.8, 1.0)))
    col = col * (1 - 0.5 * stitch[..., None]) + srgb((0.80, 0.78, 0.70)) * (0.5 * stitch[..., None])
    col = col * (1 - 0.35 * seam[..., None])
    rough = 0.82 + 0.08 * (weave - 0.5) + 0.06 * dirt
    save_ca("T_SC_quilt_C.png", col, rough)
    h = 0.75 * pillow - 0.12 * groove + 0.05 * (wr - 0.5) * (1 - pillow) + 0.006 * weave + 0.02 * stitch - 0.5 * seam
    save("T_SC_quilt_N.png", normal_map(h, 0.02))


def _hex(n, nx, ny):
    """A tiling hexagon grid, nx columns by ny rows (ny even): each pixel's distance to its cell's edge (0 at the
    centre, 1 on the wall) and a cell id."""
    u, v = uv(n)
    X, Y = u * nx, v * ny
    best = np.full((n, n), 9.0, np.float32)
    ident = np.zeros((n, n), np.int64)
    for off in (0.0, 0.5):
        r = np.round(Y - off * 2) if off == 0 else np.floor(Y) + 0.0
        # rows: even rows hold lattice A (x at integers), odd rows lattice B (x at halves)
        for dr in (-1, 0, 1):
            row = np.floor(Y) + dr
            par = np.mod(row, 2)
            cx = np.round(X - 0.5 * par) + 0.5 * par
            qx, qy = np.abs(X - cx), np.abs(Y - row) * 0.8660254
            d = np.maximum(qx, 0.5 * qx + 0.8660254 * qy) * 2.0
            take = d < best
            best = np.where(take, d, best)
            ident = np.where(take, (np.mod(row, ny) * 131 + np.mod(np.round(cx * 2), nx * 2)).astype(np.int64), ident)
        _ = r
        break
    return np.clip(best, 0, 1), ident


def shield(n=512):
    d, ident = _hex(n, 14, 16)
    rng = np.random.default_rng(SEED + 61)
    table = rng.random(131 * 16 + 64)
    cell = table[ident % table.size].astype(np.float32)
    wall = smooth(0.80, 0.92, d)
    dome = np.sqrt(np.clip(1 - (d / 0.9) ** 2, 0, 1))
    char = smooth(0.35, 0.75, ET.periodic_noise(n, 1.7, SEED + 62) * 0.75 + 0.25 * cell)
    fine = ET.periodic_noise(n, 0.3, SEED + 63)
    filler = srgb((0.47, 0.34, 0.22))[None, None, :] * (0.75 + 0.35 * cell[..., None] + 0.15 * (fine[..., None] - 0.5))
    col = filler * (1 - 0.8 * char[..., None]) + srgb((0.07, 0.06, 0.055)) * (0.8 * char[..., None])
    wcol = srgb((0.50, 0.44, 0.34))[None, None, :] * (1 - 0.6 * char[..., None]) + srgb((0.13, 0.12, 0.11)) * (0.6 * char[..., None])
    col = col * (1 - wall[..., None]) + wcol * wall[..., None]
    rough = 0.88 - 0.12 * wall + 0.08 * fine
    save_ca("T_SC_shield_C.png", col, rough)
    h = 0.35 * dome * (0.6 + 0.4 * cell) + 0.5 * wall + 0.05 * fine
    save("T_SC_shield_N.png", normal_map(h, 0.03))


def rubber(n=512):
    u, v = uv(n)
    rib = 0.5 + 0.5 * np.cos(u * 2 * math.pi * 32)
    rib = smooth(0.25, 0.75, rib)
    fine = ET.periodic_noise(n, 0.4, SEED + 71)
    tone = ET.periodic_noise(n, 1.6, SEED + 72)
    dust = smooth(0.5, 0.95, ET.periodic_noise(n, 1.4, SEED + 73)) * (1 - rib)
    val = 0.055 + 0.02 * rib + 0.015 * (fine - 0.5) + 0.02 * (tone - 0.5)
    col = np.stack([val, val, val * 1.03], -1)
    col = col * (1 - 0.3 * dust[..., None]) + srgb((0.22, 0.20, 0.17)) * (0.3 * dust[..., None])
    save_ca("T_SC_rubber_C.png", col, 0.62 + 0.2 * dust + 0.1 * fine - 0.1 * rib)
    save("T_SC_rubber_N.png", normal_map(0.5 * rib + 0.04 * fine, 0.012))


def fabric(n=512):
    u, v = uv(n)
    N = 64
    wu, wv = np.sin(u * 2 * math.pi * N), np.sin(v * 2 * math.pi * N)
    cu, cv = np.floor(u * N * 2), np.floor(v * N * 2)
    over = np.mod(cu + cv, 2)                                  # which thread is on top in this cell of the weave
    warp = np.abs(np.sin(u * math.pi * N * 2)) ** 0.6          # rounded thread profiles
    weft = np.abs(np.sin(v * math.pi * N * 2)) ** 0.6
    h = np.where(over > 0.5, 0.6 + 0.4 * warp * np.abs(np.sin(v * math.pi * N * 2)), 0.6 + 0.4 * weft * np.abs(np.sin(u * math.pi * N * 2)))
    h = h * (0.55 + 0.45 * np.minimum(warp, weft) ** 0.5)
    fib = brushed(n, SEED + 81, cut=0.2)
    fibv = brushed(n, SEED + 82, cut=0.2).T
    fibres = np.where(over > 0.5, fibv, fib)
    tone = ET.periodic_noise(n, 1.6, SEED + 83)
    base = srgb((0.43, 0.44, 0.34))
    col = base[None, None, :] * (0.55 + 0.5 * h[..., None] + 0.12 * (fibres[..., None] - 0.5) + 0.1 * (tone[..., None] - 0.5))
    save_ca("T_SC_fabric_C.png", col, 0.86 + 0.06 * (fibres - 0.5))
    save("T_SC_fabric_N.png", normal_map(h + 0.05 * fibres, 0.006))
    _ = wu, wv


def chute(n=512):
    u, v = uv(n)
    orange = (u < 0.5).astype(np.float32)
    grid = 64
    gu, gv = np.abs(np.mod(u * grid + 0.5, 1) - 0.5), np.abs(np.mod(v * grid + 0.5, 1) - 0.5)
    rip = np.maximum(1 - smooth(0.02, 0.07, gu), 1 - smooth(0.02, 0.07, gv))       # the thicker ripstop threads
    weave = 0.5 + 0.25 * np.sin(u * 2 * math.pi * 256) + 0.25 * np.sin(v * 2 * math.pi * 256)
    # the seams between the gores: a folded band with two stitch rows
    ds = np.minimum(np.minimum(u, 1 - u), np.abs(u - 0.5))
    band = 1 - smooth(0.012, 0.016, ds)
    st = ((np.abs(ds - 0.008) < 0.0025) & (np.sin(v * 2 * math.pi * 48) > -0.3)).astype(np.float32)
    wr = ET.band(n, 3, 12, SEED + 91)
    tone = ET.periodic_noise(n, 1.7, SEED + 92)
    c = srgb((0.93, 0.36, 0.07))[None, None, :] * orange[..., None] + srgb((0.92, 0.91, 0.86))[None, None, :] * (1 - orange[..., None])
    col = c * (0.88 + 0.08 * (weave[..., None] - 0.5) + 0.10 * (tone[..., None] - 0.5) + 0.10 * (wr[..., None] - 0.5))
    col = col * (1 - 0.10 * rip[..., None]) * (1 - 0.12 * band[..., None])
    col = col * (1 - 0.5 * st[..., None]) + srgb((0.95, 0.94, 0.9)) * (0.5 * st[..., None])
    save_ca("T_SC_chute_C.png", col, 0.55 + 0.1 * rip + 0.1 * band)
    h = 0.06 * rip + 0.2 * band + 0.6 * (wr - 0.5) * 0.25 + 0.01 * weave + 0.05 * st
    save("T_SC_chute_N.png", normal_map(h, 0.02))


def net(n=512):
    """The stowage net, one tile = 0.25 m: twisted cord in diamonds 6 cm across, a knot at each crossing. A = the cutout."""
    u, v = uv(n)
    N = 4
    a, b = (u + v) * N, (u - v) * N
    da, db = np.abs(np.mod(a + 0.5, 1) - 0.5), np.abs(np.mod(b + 0.5, 1) - 0.5)
    w = 0.045
    cord = np.maximum(1 - smooth(w * 0.7, w, da), 1 - smooth(w * 0.7, w, db))
    knot = 1 - smooth(0.07, 0.10, np.hypot(da, db))
    mask = np.maximum(cord, knot)
    twist = 0.5 + 0.5 * np.where(da < db, np.sin((b * 14 + da * 30) * math.pi), np.sin((a * 14 + db * 30) * math.pi))
    tone = ET.periodic_noise(n, 1.5, SEED + 96)
    col = srgb((0.74, 0.71, 0.60))[None, None, :] * (0.72 + 0.22 * twist[..., None] + 0.12 * (tone[..., None] - 0.5) + 0.1 * knot[..., None])
    save_ca("T_SC_net_C.png", col, mask)


# ---------------------------------------------------------------------------------------------------------------------
# instruments


def kit():
    if os.path.isfile(KIT_JSON):
        with open(KIT_JSON) as f:
            return json.load(f)
    return {}


def tile_m():
    """The kit's tile sizes in metres ("tx_sc_x": cm), over the defaults."""
    t = dict(TILE_M)
    for k, v in (kit().get("tiles") or {}).items():
        if k.startswith("tx_sc_"):
            t[k[6:]] = round(float(v) / 100.0, 6)
    return t


def dial_specs():
    """{id: spec} as DIALS: the kit's panel.dials when kit.json is there."""
    src = (kit().get("panel") or {}).get("dials")
    if not src:
        return DIALS, "textures.py DIALS"
    out = {}
    for d in src:
        out[d["id"]] = dict(rect=d["atlas_rect"], min=d["min"], max=d["max"], a_min=d["a_min"], a_max=d["a_max"], major=d["major"],
                            minor=d["minor"], show=round(d["window_radius"] / (d["atlas_circle_radius_px"] / d["atlas_px_per_cm"]), 4))
    return out, "kit.json"


def _num(x):
    return "%g" % round(x, 3)


def draw_dial(size, spec, did):
    """One dial face, size x size px, drawn at 4x and reduced. Its circle is the square's inscribed one; the bezel
    hides what lies beyond `show` of it, so the scale sits inside that."""
    S = size * 4
    img = Image.new("RGB", (S, S), (16, 18, 18))
    d = ImageDraw.Draw(img)
    c = S / 2
    R = S / 2 * spec["show"] * 0.97
    cream = (236, 230, 205)
    lo, hi, a0, a1 = spec["min"], spec["max"], spec["a_min"], spec["a_max"]
    full = a1 - a0 >= 359.0

    def ang(val):
        return math.radians(a0 + (val - lo) / (hi - lo) * (a1 - a0))

    def pt(a, r):
        return c + math.sin(a) * r, c - math.cos(a) * r

    for z0, z1, colr in ZONES.get(did, []):
        d.arc((c - R * 0.80, c - R * 0.80, c + R * 0.80, c + R * 0.80), math.degrees(ang(z0)) - 90, math.degrees(ang(z1)) - 90,
              fill=tuple(int(v * 255) for v in colr), width=int(S * 0.02))
    minor = max(1, int(round(spec["major"] / spec["minor"])))
    n_major = int(round((hi - lo) / spec["major"]))
    font = ImageFont.truetype(FONT, int(S * (0.095 if n_major <= 7 else 0.082)))
    for i in range(n_major * minor + (0 if full else 1)):
        a = ang(lo + i * spec["major"] / minor)
        big = i % minor == 0
        mid = minor % 2 == 0 and i % minor == minor // 2
        r0 = R * (0.85 if big else (0.90 if mid else 0.93))
        d.line((*pt(a, r0), *pt(a, R * 0.99)), fill=cream, width=int(S * (0.016 if big else 0.007)))
        if big:
            x, y = pt(a, R * 0.60)
            d.text((x, y), _num(lo + (i // minor) * spec["major"]), fill=cream, font=font, anchor="mm")
    # the hub's ring; the needle and its cap are the mesh's
    d.ellipse((c - R * 0.09, c - R * 0.09, c + R * 0.09, c + R * 0.09), outline=(70, 70, 66), width=int(S * 0.006))
    if did == "alt":
        # two needles: an inner ring of ticks for the short one
        for i in range(50):
            a = math.radians(i * 7.2)
            d.line((*pt(a, R * 0.36), *pt(a, R * (0.42 if i % 5 == 0 else 0.39))), fill=(170, 166, 150), width=int(S * 0.005))
    return np.asarray(img.resize((size, size), Image.LANCZOS), np.float32) / 255.0


def dials(n=1024):
    specs, where = dial_specs()
    atlas = np.zeros((n, n, 3), np.float32) + 0.03
    for did, spec in specs.items():
        x, y, w, h = [int(round(v * n / 1024.0)) for v in spec["rect"]]
        sz = min(w, h)
        atlas[y + (h - sz) // 2:y + (h - sz) // 2 + sz, x + (w - sz) // 2:x + (w - sz) // 2 + sz] = draw_dial(sz, spec, did)
    save("T_SC_dials.png", atlas)
    _written["T_SC_dials.png"]["layout"] = where
    _written["T_SC_dials.png"]["cells"] = specs


def ball(w=512, h=256):
    """The attitude ball: pale sky above, dark earth below, the horizon, a pitch ladder every 10 degrees (numbered every
    30, in tens), meridians every 30."""
    S = 4
    img = Image.new("RGB", (w * S, h * S))
    d = ImageDraw.Draw(img)
    W_, H_ = w * S, h * S
    d.rectangle((0, 0, W_, H_ / 2), fill=(150, 185, 205))
    d.rectangle((0, H_ / 2, W_, H_), fill=(38, 30, 24))
    ink_up, ink_dn = (20, 24, 28), (225, 220, 200)
    font = ImageFont.truetype(FONT, int(H_ * 0.055))
    for lon in range(0, 360, 30):
        x = lon / 360.0 * W_
        d.line((x, H_ * 0.06, x, H_ / 2), fill=ink_up, width=S * (2 if lon % 90 == 0 else 1))
        d.line((x, H_ / 2, x, H_ * 0.94), fill=ink_dn, width=S * (2 if lon % 90 == 0 else 1))
    for p in range(-80, 90, 10):
        if p == 0:
            continue
        y = (0.5 - p / 180.0) * H_
        ink = ink_up if p > 0 else ink_dn
        bold = p % 30 == 0
        if bold:
            d.line((0, y, W_, y), fill=ink, width=S * 2)
        else:
            for k in range(24):                       # short rungs between the meridians
                x = (k + 0.5) / 24 * W_
                half = W_ / 24 * 0.22
                d.line((x - half, y, x + half, y), fill=ink, width=S)
        if bold:
            for k in range(8):
                x = (k + 0.5) / 8 * W_
                bg = (150, 185, 205) if p > 0 else (38, 30, 24)
                d.rectangle((x - H_ * 0.035, y - H_ * 0.032, x + H_ * 0.035, y + H_ * 0.032), fill=bg)
                d.text((x, y), "%d" % (abs(p) // 10), fill=ink, font=font, anchor="mm")
    d.line((0, H_ / 2, W_, H_ / 2), fill=(245, 245, 240), width=S * 3)
    save("T_SC_ball.png", np.asarray(img.resize((w, h), Image.LANCZOS), np.float32) / 255.0)


# segments a b c d e f g of each glyph (a top, b top right, c bottom right, d bottom, e bottom left, f top left, g middle)
SEGS = ["abcdef", "bc", "abged", "abgcd", "fgbc", "afgcd", "afgcde", "abc", "abcdefg", "abcdfg", "g", ""]


def digits(cw=32, ch=64, cells=16):
    S = 4
    img = Image.new("L", (cw * cells * S, ch * S), 0)
    d = ImageDraw.Draw(img)
    for k in range(cells):
        segs = SEGS[k] if k < len(SEGS) else ""
        ox = k * cw * S
        x0, x1, y0, ym, y1 = cw * S * 0.22, cw * S * 0.78, ch * S * 0.14, ch * S * 0.5, ch * S * 0.86
        t = cw * S * 0.075
        sl = 0.10                                         # the slant: x shifts with height

        def P(x, y):
            return ox + x + (ch * S * 0.5 - y) * sl, y

        def bar(xa, ya, xb, yb):
            horiz = abs(ya - yb) < 1
            g = t * 0.55
            if horiz:
                pts = [(xa + g, ya), (xa + g + t, ya - t), (xb - g - t, ya - t), (xb - g, ya), (xb - g - t, ya + t), (xa + g + t, ya + t)]
            else:
                pts = [(xa, ya + g), (xa + t, ya + g + t), (xa + t, yb - g - t), (xa, yb - g), (xa - t, yb - g - t), (xa - t, ya + g + t)]
            d.polygon([P(x, y) for x, y in pts], fill=255)

        table = {"a": (x0, y0, x1, y0), "g": (x0, ym, x1, ym), "d": (x0, y1, x1, y1), "f": (x0, y0, x0, ym), "b": (x1, y0, x1, ym),
                 "e": (x0, ym, x0, y1), "c": (x1, ym, x1, y1)}
        for s in segs:
            bar(*table[s])
    save("T_SC_digits.png", np.asarray(img.resize((cw * cells, ch), Image.LANCZOS), np.float32) / 255.0)


# ---------------------------------------------------------------------------------------------------------------------
# glass and noise


def glass(n=512):
    u, v = uv(n)
    x, y = u * 2 - 1, v * 2 - 1
    r = np.hypot(x, y)
    rng = np.random.default_rng(SEED + 101)
    inward = np.clip(1 - r, 0, 1)
    # frost: fern crystals growing in from the rim: ridged noise, finer toward the middle
    f1 = 1 - np.abs(ET.band(n, 6, 18, SEED + 102) - 0.5) * 2
    f2 = 1 - np.abs(ET.band(n, 18, 60, SEED + 103) - 0.5) * 2
    cr = norm01(0.6 * f1 ** 2 + 0.4 * f2 ** 2)
    frost = np.clip(inward * 1.15 + 0.38 * (0.5 - cr) + 0.05, 0, 1)
    # scorch: soot creeping in from the rim, streaked along the flow (down the picture)
    st = norm01(blur(ET.band(n, 8, 40, SEED + 104), 1.0))
    sx = norm01(np.real(np.fft.ifft(np.fft.fft(ET.band(n, 10, 60, SEED + 105), axis=0) * (np.abs(np.fft.fftfreq(n)) < 0.015)[:, None], axis=0)))
    scorch = np.clip(inward * 1.2 + 0.30 * (0.5 - sx) + 0.14 * (0.5 - st) + 0.04, 0, 1)
    # droplets: domes of many sizes
    drops = np.zeros((n, n), np.float32)
    yy, xx = np.mgrid[0:n, 0:n]
    for _ in range(420):
        cx, cy = rng.uniform(0, n, 2)
        rad = rng.choice([2.0, 3.0, 4.0, 6.0, 9.0, 13.0], p=[0.3, 0.25, 0.2, 0.13, 0.08, 0.04]) * rng.uniform(0.8, 1.2)
        x0, x1, y0, y1 = int(max(cx - rad - 1, 0)), int(min(cx + rad + 2, n)), int(max(cy - rad - 1, 0)), int(min(cy + rad + 2, n))
        dd = ((xx[y0:y1, x0:x1] - cx) ** 2 + ((yy[y0:y1, x0:x1] - cy) * 0.88) ** 2) / (rad * rad)
        drops[y0:y1, x0:x1] = np.maximum(drops[y0:y1, x0:x1], np.sqrt(np.clip(1 - dd, 0, 1)) * rng.uniform(0.6, 1.0))
    # rivulets: wandering vertical runs, tiling in V so they can scroll
    riv = np.zeros((n, n), np.float32)
    rows = np.arange(n)
    for k in range(26):
        x0 = rng.uniform(0, n)
        wob = 10 * np.sin(rows / n * 2 * math.pi * rng.integers(1, 4) + rng.uniform(0, 6)) + 4 * np.sin(rows / n * 2 * math.pi * rng.integers(5, 11) + rng.uniform(0, 6))
        w = rng.uniform(1.5, 3.5)
        strength = 0.5 + 0.5 * np.sin(rows / n * 2 * math.pi * rng.integers(1, 3) + rng.uniform(0, 6))
        dx = np.abs(np.mod(xx - (x0 + wob)[:, None] + n / 2, n) - n / 2)
        riv = np.maximum(riv, np.exp(-(dx / w) ** 2) * (0.35 + 0.65 * strength[:, None]))
    save("T_SC_glass.png", np.stack([frost, scorch, drops, riv], -1))


def noise(n=256):
    r = ET.periodic_noise(n, 1.6, SEED + 111)
    g = ET.band(n, 3, 14, SEED + 112)
    s = ET.band(n, 4, 40, SEED + 113)
    b = norm01(np.real(np.fft.ifft(np.fft.fft(s, axis=0) * (np.abs(np.fft.fftfreq(n)) < 0.02)[:, None], axis=0)))
    a = norm01(blur(np.random.default_rng(SEED + 114).random((n, n)), 0.6))

    def eq(x):          # histogram-equalised: the material's thresholds then mean a share of the area
        o = np.argsort(x.ravel())
        e = np.empty(x.size, np.float32)
        e[o] = np.linspace(0, 1, x.size, dtype=np.float32)
        return e.reshape(x.shape)

    save("T_SC_noise.png", np.stack([eq(r), eq(g), eq(b), eq(a)], -1))


ALL = [paint_white, paint_green, metal, foil, quilt, shield, rubber, fabric, chute, net, dials, ball, digits, glass, noise]


def gpu_bytes(name, info):
    """What the texture costs compressed on the GPU with its mips: BC5 for normals, BC1 for RGB, BC3 with alpha, BC4 grey."""
    w, h = info["size"]
    bpp = 8 if name.endswith("_N.png") else {1: 4, 3: 4, 4: 8}[info["channels"]]
    return int(w * h * bpp / 8 * 4 / 3)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    check = "--check" in sys.argv
    pins = {}
    if os.path.isfile(PINS):
        with open(PINS) as f:
            pins = json.load(f)
    for f in ALL:
        if not args or f.__name__ in args:
            f()
    tex = pins.get("textures", {})
    bad = [k for k, v in _written.items() if check and tex.get(k, {}).get("sha1") not in (None, v["sha1"])]
    for k, v in _written.items():
        v["gpu_bytes"] = gpu_bytes(k, v)
        tex[k] = v
    pins.update({"seed": SEED, "tile_m": tile_m(), "metal": METAL, "textures": dict(sorted(tex.items()))})
    pins["gpu_bytes_total"] = sum(v.get("gpu_bytes", 0) for v in pins["textures"].values())
    with open(PINS, "w") as f:
        json.dump(pins, f, indent=1, sort_keys=True)
        f.write("\n")
    print("%d textures, %.2f MB on the GPU (compressed, with mips)" % (len(pins["textures"]), pins["gpu_bytes_total"] / 1048576.0))
    if bad:
        raise SystemExit("changed against textures.json: %s" % ", ".join(bad))


if __name__ == "__main__":
    main()
