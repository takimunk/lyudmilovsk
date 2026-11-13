"""The North props' shared texture atlas (docs/north.md): procedural, numpy only, no paid generation.

One 2048 x 2048 base colour + tangent normal pair serves every prop (a trim sheet: faces are mapped into a tile's
window, so planks, canvas and stone repeat the same detail on purpose). Tiles are named rectangles (pixels).

    python3 blender/north/northtex.py [outdir]     writes north_atlas.png, north_atlas_n.png
"""
import os
import sys

import numpy as np
from PIL import Image

SIZE = 2048

# name -> (x, y, w, h) in pixels, y from the top of the image
TILES = {
    "wood": (0, 0, 512, 512),            # weathered grey-brown boards, grain along U
    "wood_dark": (512, 0, 512, 512),     # tarred / damp brown wood
    "bark": (1024, 0, 512, 512),         # stone birch: pale bark with dark lenticels, along U
    "stone": (1536, 0, 512, 512),        # rounded grey-brown field stone
    "canvas_khaki": (0, 512, 512, 512),
    "canvas_green": (512, 512, 512, 512),
    "nylon_orange": (1024, 512, 512, 512),
    "nylon_bluegrey": (1536, 512, 512, 512),
    "nylon_olive": (0, 1024, 512, 512),
    "nylon_yellow": (512, 1024, 512, 512),
    "tarp_blue": (1024, 1024, 512, 512),
    "metal": (1536, 1024, 512, 512),     # blackened iron with rust, a little bright wear
    "endgrain": (0, 1536, 512, 512),     # cut log end: rings, radial cracks (centre of the tile = the pith)
    "ash": (512, 1536, 256, 256),
    "rope": (768, 1536, 256, 256),
    "stripes": (512, 1792, 256, 256),    # red and white paint, 8 bands along U
    "swatch": (768, 1792, 256, 256),     # 8 x 8 flat colours of 32 px (see SWATCH)
    "char": (1024, 1536, 256, 256),      # charcoal
    "plaid": (1280, 1536, 256, 256),     # blue-white flannel
    "towel": (1536, 1536, 256, 256),     # striped towel
    "knit": (1792, 1536, 256, 256),      # grey wool sock with a red band
    "mat": (1024, 1792, 256, 256),       # foam sleeping mat
    "label": (1280, 1792, 256, 256),     # a tin's wordless label: blue and white bands
    "stone_soot": (1536, 1792, 256, 256),
    "plank_edge": (1792, 1792, 256, 256),  # fresh pale pine (cut ends, new pegs)
}

# flat colours: name -> (col, row) on the 8 x 8 grid, and the colour (sRGB 0..255)
SWATCH = {
    "dark": ((0, 0), (44, 40, 36)),            # a tent's inside
    "enamel_white": ((1, 0), (226, 224, 214)),
    "enamel_blue": ((2, 0), (36, 74, 140)),
    "tin": ((3, 0), (158, 160, 160)),
    "steel": ((4, 0), (120, 124, 128)),
    "rubber": ((5, 0), (30, 30, 32)),
    "red_paint": ((6, 0), (172, 38, 30)),
    "white_paint": ((7, 0), (222, 220, 210)),
    "sock_red": ((0, 1), (150, 44, 38)),
    "bag_orange": ((1, 1), (214, 112, 34)),
    "bag_green": ((2, 1), (62, 92, 52)),
    "rust": ((3, 1), (112, 62, 38)),
    "yellow_sign": ((4, 1), (232, 188, 28)),
    "black_paint": ((5, 1), (26, 24, 22)),
    "grey_wool": ((6, 1), (118, 116, 112)),
    "hem": ((7, 1), (60, 62, 52)),
    "carabiner": ((0, 2), (170, 60, 150)),
    "ground_dark": ((1, 2), (52, 44, 38)),
}

TINT = {
    "canvas_khaki": (0.56, 0.50, 0.33),
    "canvas_green": (0.52, 0.60, 0.46),
    "nylon_orange": (0.82, 0.42, 0.15),
    "nylon_bluegrey": (0.30, 0.38, 0.47),
    "nylon_olive": (0.30, 0.34, 0.18),
    "nylon_yellow": (0.88, 0.70, 0.10),
    "tarp_blue": (0.27, 0.42, 0.58),
}


def rect_uv(name, inset=6):
    """(u0, v0, u1, v1) in glTF-like image space with v up (Blender): the tile's window, inset from its edge."""
    x, y, w, h = TILES[name]
    return ((x + inset) / SIZE, 1.0 - (y + h - inset) / SIZE, (x + w - inset) / SIZE, 1.0 - (y + inset) / SIZE)


def swatch_uv(name):
    """The centre of a flat colour's cell, a (u, v) pair."""
    (c, r), _ = SWATCH[name]
    x, y, w, h = TILES["swatch"]
    return ((x + c * 32 + 16) / SIZE, 1.0 - (y + r * 32 + 16) / SIZE)


def pnoise(rng, w, h, fx, fy, power=1.0):
    """Periodic noise 0..1, a gaussian band-pass in frequency: fx, fy = cycles across the tile along each axis."""
    n = rng.standard_normal((h, w))
    F = np.fft.fft2(n)
    kx = np.fft.fftfreq(w) * w
    ky = np.fft.fftfreq(h) * h
    KX, KY = np.meshgrid(kx, ky)
    amp = np.exp(-(KX / fx) ** 2 - (KY / fy) ** 2)
    out = np.real(np.fft.ifft2(F * amp))
    out = (out - out.min()) / (out.max() - out.min() + 1e-9)
    return out ** power


def fbm(rng, w, h, base, octaves=4, aniso=1.0):
    t = np.zeros((h, w))
    a, s = 1.0, 0.0
    for o in range(octaves):
        f = base * (2 ** o)
        t += a * pnoise(rng, w, h, f / aniso, f)
        s += a
        a *= 0.5
    t /= s
    return (t - t.min()) / (t.max() - t.min() + 1e-9)


def mix(a, b, t):
    t = np.clip(t, 0, 1)[..., None]
    return a * (1 - t) + b * t


def col(c):
    return np.array(c, dtype=float)


def tile_wood(rng, w, h, base, dark, planks=False):
    # grain runs along x (U)
    g = fbm(rng, w, h, 3.0, 4, aniso=14.0)
    ring = 0.5 + 0.5 * np.sin((g * 24.0 + np.linspace(0, 1, h)[:, None] * 2.0) * np.pi)
    streak = pnoise(rng, w, h, 2.0, 90.0)
    crack = np.clip((pnoise(rng, w, h, 3.0, 140.0) - 0.74) * 6.0, 0, 1)
    c = mix(col(base), col(dark), 0.55 * g + 0.25 * ring + 0.25 * streak)
    c = mix(c, col(base) * 0.55, crack)
    # silver weathering and faint dark knots
    sil = pnoise(rng, w, h, 5.0, 14.0)
    c = mix(c, c * 0.75 + np.array([0.09, 0.09, 0.085]), 0.35 * sil)
    for _ in range(3 if planks else 2):
        kx, ky = rng.integers(0, w), rng.integers(0, h)
        yy, xx = np.mgrid[0:h, 0:w]
        dx = np.minimum(abs(xx - kx), w - abs(xx - kx)) / 16.0
        dy = np.minimum(abs(yy - ky), h - abs(yy - ky)) / 7.0
        k = np.exp(-(dx * dx + dy * dy))
        c = mix(c, col(dark) * 0.45, 0.8 * k)
    hgt = 0.6 * g + 0.4 * ring - 0.8 * crack
    return c, hgt


def tile_bark(rng, w, h):
    # stone birch: pale creamy-grey with dark horizontal lenticels
    g = fbm(rng, w, h, 4.0, 4, aniso=0.25)
    base = mix(col((0.78, 0.74, 0.66)), col((0.68, 0.64, 0.57)), g)
    len_ = pnoise(rng, w, h, 130.0, 24.0)
    dash = np.clip((len_ - 0.62) * 7.0, 0, 1)
    c = mix(base, col((0.12, 0.10, 0.09)), dash * 0.85)
    peel = pnoise(rng, w, h, 20.0, 3.0)
    c = mix(c, col((0.86, 0.80, 0.70)), np.clip((peel - 0.7) * 4.0, 0, 1) * 0.5)
    return c, 0.5 * g - 0.55 * dash


def tile_stone(rng, w, h, base, dark, soot=False):
    g = fbm(rng, w, h, 3.0, 5)
    sp = pnoise(rng, w, h, 60.0, 60.0)
    c = mix(col(base), col(dark), g)
    c = mix(c, c * 1.35, np.clip((sp - 0.62) * 4.0, 0, 1) * 0.7)
    li = pnoise(rng, w, h, 9.0, 9.0)
    if not soot:
        c = mix(c, col((0.62, 0.64, 0.46)), np.clip((li - 0.74) * 5.0, 0, 1) * 0.45)   # a little lichen
    else:
        c = c * 0.8
        c = mix(c, col((0.40, 0.38, 0.35)), np.clip((li - 0.7) * 4.0, 0, 1) * 0.4)
    return c, 0.5 * g + 0.4 * sp


def tile_cloth(rng, w, h, tint, ripstop=False, canvas=False, fade=0.35):
    xx, yy = np.meshgrid(np.arange(w), np.arange(h))
    weave = 0.5 + 0.25 * np.cos(xx * 2 * np.pi / 4.0) + 0.25 * np.cos(yy * 2 * np.pi / 4.0)
    fd = fbm(rng, w, h, 2.0, 4)
    stain = pnoise(rng, w, h, 7.0, 7.0)
    c = col(tint)
    # sun fading and dirt: lighter, greyer patches and darker splashes
    grey = np.array([c.mean()] * 3) * 1.1
    c = mix(c, grey, fade * fd)
    c = mix(c, c * 0.62, np.clip((stain - 0.62) * 3.0, 0, 1) * (0.4 if canvas else 0.3))
    img = c * (0.96 + 0.04 * weave[..., None]) if canvas else c * (0.995 + 0.005 * weave[..., None])
    # hem seams: faint stitched lines each ~1/4 of the tile
    seam = ((xx % 256) < 2)
    img = np.where(seam[..., None], img * 0.93, img)
    hgt = 0.015 * weave + 0.5 * fd + 0.0 * seam
    return img, hgt


def tile_metal(rng, w, h):
    g = fbm(rng, w, h, 4.0, 4)
    rust = pnoise(rng, w, h, 8.0, 8.0)
    c = mix(col((0.10, 0.10, 0.10)), col((0.22, 0.2, 0.19)), g)
    c = mix(c, col((0.42, 0.22, 0.10)), np.clip((rust - 0.55) * 3.0, 0, 1) * 0.8)
    wear = np.clip((pnoise(rng, w, h, 30.0, 30.0) - 0.7) * 5.0, 0, 1)
    c = mix(c, col((0.5, 0.5, 0.5)), wear * 0.5)
    return c, 0.4 * g + 0.5 * rust


def tile_endgrain(rng, w, h):
    yy, xx = np.mgrid[0:h, 0:w]
    dx, dy = (xx - w / 2) / (w / 2), (yy - h / 2) / (h / 2)
    r = np.sqrt(dx * dx + dy * dy)
    warp = fbm(rng, w, h, 2.0, 3)
    rr = r + 0.06 * (warp - 0.5)
    rings = 0.5 + 0.5 * np.sin(rr * 48.0)
    base = mix(col((0.78, 0.62, 0.42)), col((0.55, 0.40, 0.24)), rings * 0.7 + 0.2 * warp)
    ang = np.arctan2(dy, dx)
    cr = np.clip((pnoise(rng, w, h, 40.0, 40.0) - 0.7) * 5.0, 0, 1) * (0.5 + 0.5 * np.cos(ang * 7.0)) ** 6 * (r > 0.1)
    c = mix(base, col((0.25, 0.17, 0.10)), cr * 0.9)
    # grey bark margin beyond r > 0.9 and a dark rim line
    c = mix(c, col((0.30, 0.27, 0.23)), np.clip((r - 0.92) * 20.0, 0, 1))
    return c, 0.5 * rings - cr


def tile_rope(rng, w, h):
    xx, yy = np.meshgrid(np.arange(w), np.arange(h))
    twist = 0.5 + 0.5 * np.sin((xx * 0.5 + yy * 1.0) * 2 * np.pi / 16.0 * 1.0)
    fib = pnoise(rng, w, h, 40.0, 5.0)
    c = mix(col((0.50, 0.40, 0.26)), col((0.74, 0.64, 0.46)), twist * 0.6 + 0.3 * fib)
    return c, twist


def tile_stripes(rng, w, h):
    xx, yy = np.meshgrid(np.arange(w), np.arange(h))
    band = (xx * 8 // w) % 2
    base = np.where(band[..., None] == 0, col((0.72, 0.13, 0.10)), col((0.86, 0.85, 0.80)))
    chip = np.clip((pnoise(rng, w, h, 30.0, 30.0) - 0.68) * 5.0, 0, 1)
    c = mix(base, col((0.35, 0.33, 0.30)), chip * 0.6)
    c = mix(c, c * 0.7, fbm(rng, w, h, 2.0, 3) * 0.4)
    return c, 0.3 * chip


def tile_plaid(rng, w, h):
    xx, yy = np.meshgrid(np.arange(w), np.arange(h))
    a = ((xx // 32) % 2) * 0.5
    b = ((yy // 32) % 2) * 0.5
    t = a + b
    c = np.where(t[..., None] == 0, col((0.14, 0.22, 0.45)), np.where(t[..., None] == 0.5, col((0.35, 0.45, 0.72)), col((0.82, 0.84, 0.88))))
    weave = 0.9 + 0.1 * np.cos(xx * np.pi / 2.0) * np.cos(yy * np.pi / 2.0)
    return c * weave[..., None], 0.5 * weave


def tile_towel(rng, w, h):
    xx, yy = np.meshgrid(np.arange(w), np.arange(h))
    band = ((yy // 24) % 4 == 1) | ((yy // 24) % 4 == 2)
    base = np.where(band[..., None], col((0.20, 0.34, 0.62)), col((0.84, 0.84, 0.80)))
    loops = pnoise(rng, w, h, 70.0, 70.0)
    return base * (0.85 + 0.25 * loops[..., None]), loops


def tile_knit(rng, w, h):
    xx, yy = np.meshgrid(np.arange(w), np.arange(h))
    v = 0.5 + 0.5 * np.sin(xx * 2 * np.pi / 6.0 + (yy % 12) * 0.4)
    base = col((0.48, 0.47, 0.44))
    c = base * (0.82 + 0.28 * v[..., None])
    band = (yy > 40) & (yy < 70)
    c = np.where(band[..., None], col((0.62, 0.18, 0.15)) * (0.8 + 0.3 * v[..., None]), c)
    return c, v


def tile_mat(rng, w, h):
    xx, yy = np.meshgrid(np.arange(w), np.arange(h))
    ridge = 0.5 + 0.5 * np.cos(yy * 2 * np.pi / 16.0)
    c = col((0.80, 0.50, 0.14)) * (0.85 + 0.15 * ridge[..., None])
    c = mix(c, c * 0.65, np.clip((pnoise(rng, w, h, 6.0, 6.0) - 0.55) * 2.0, 0, 1) * 0.5)
    return c, ridge


def tile_label(rng, w, h):
    xx, yy = np.meshgrid(np.arange(w), np.arange(h))
    c = np.where((xx // 64 % 2 == 0)[..., None], col((0.14, 0.32, 0.62)), col((0.88, 0.88, 0.84)))
    c = np.where(((xx > 88) & (xx < 168))[..., None], col((0.95, 0.94, 0.9)), c)
    return c * (0.9 + 0.1 * pnoise(rng, w, h, 4.0, 4.0)[..., None]), 0 * xx


def tile_ash(rng, w, h):
    g = fbm(rng, w, h, 4.0, 4)
    c = mix(col((0.45, 0.44, 0.42)), col((0.74, 0.73, 0.70)), g)
    bits = np.clip((pnoise(rng, w, h, 30.0, 30.0) - 0.7) * 6.0, 0, 1)
    c = mix(c, col((0.07, 0.06, 0.06)), bits * 0.8)
    return c, 0.5 * g


def tile_char(rng, w, h):
    g = fbm(rng, w, h, 5.0, 4)
    sq = np.clip((pnoise(rng, w, h, 22.0, 22.0) - 0.5) * 5.0, 0, 1)
    c = mix(col((0.04, 0.035, 0.03)), col((0.20, 0.17, 0.15)), g * 0.6)
    c = mix(c, col((0.3, 0.27, 0.25)), sq * 0.2)
    return c, 0.6 * sq


def tile_pine(rng, w, h):
    return tile_wood(rng, w, h, (0.82, 0.68, 0.46), (0.62, 0.46, 0.28))


def build(seed=7):
    rng = np.random.default_rng(seed)
    atlas = np.zeros((SIZE, SIZE, 3))
    height = np.full((SIZE, SIZE), 0.5)
    nstr = {"wood": 4.0, "wood_dark": 4.0, "bark": 5.0, "stone": 5.0, "endgrain": 3.0, "rope": 4.0, "metal": 2.5}

    def put(name, c, hg):
        x, y, w, h = TILES[name]
        atlas[y:y + h, x:x + w] = c
        height[y:y + h, x:x + w] = hg

    for name, (x, y, w, h) in TILES.items():
        if name == "wood":
            c, hg = tile_wood(rng, w, h, (0.52, 0.44, 0.34), (0.30, 0.24, 0.18), planks=True)
        elif name == "wood_dark":
            c, hg = tile_wood(rng, w, h, (0.36, 0.27, 0.19), (0.18, 0.13, 0.09))
        elif name == "plank_edge":
            c, hg = tile_pine(rng, w, h)
        elif name == "bark":
            c, hg = tile_bark(rng, w, h)
        elif name == "stone":
            c, hg = tile_stone(rng, w, h, (0.50, 0.49, 0.45), (0.30, 0.30, 0.29))
        elif name == "stone_soot":
            c, hg = tile_stone(rng, w, h, (0.36, 0.35, 0.33), (0.18, 0.18, 0.18), soot=True)
        elif name.startswith("canvas"):
            c, hg = tile_cloth(rng, w, h, TINT[name], canvas=True, fade=0.45)
        elif name.startswith("nylon") or name == "tarp_blue":
            c, hg = tile_cloth(rng, w, h, TINT[name], ripstop=name != "tarp_blue", fade=0.3)
        elif name == "metal":
            c, hg = tile_metal(rng, w, h)
        elif name == "endgrain":
            c, hg = tile_endgrain(rng, w, h)
        elif name == "ash":
            c, hg = tile_ash(rng, w, h)
        elif name == "char":
            c, hg = tile_char(rng, w, h)
        elif name == "rope":
            c, hg = tile_rope(rng, w, h)
        elif name == "stripes":
            c, hg = tile_stripes(rng, w, h)
        elif name == "plaid":
            c, hg = tile_plaid(rng, w, h)
        elif name == "towel":
            c, hg = tile_towel(rng, w, h)
        elif name == "knit":
            c, hg = tile_knit(rng, w, h)
        elif name == "mat":
            c, hg = tile_mat(rng, w, h)
        elif name == "label":
            c, hg = tile_label(rng, w, h)
        elif name == "swatch":
            c = np.zeros((h, w, 3))
            hg = np.full((h, w), 0.5)
            for _, ((cc, rr), rgb) in SWATCH.items():
                c[rr * 32:(rr + 1) * 32, cc * 32:(cc + 1) * 32] = np.array(rgb) / 255.0
            c[...] = np.where(c.sum(-1, keepdims=True) == 0, 0.5, c)
        else:
            continue
        put(name, c, hg)

    # tiles need a gutter: edge pixels repeat outward a few pixels (mip bleeding of neighbours stays inside the tile)
    img = (np.clip(atlas, 0, 1) ** (1 / 1.0) * 255).astype(np.uint8)
    strength = np.ones((SIZE, SIZE))
    for name, (x, y, w, h) in TILES.items():
        s = nstr.get(name, 3.0)
        strength[y:y + h, x:x + w] = s
        if name in TINT or name.startswith("nylon"):
            strength[y:y + h, x:x + w] = 1.2
    gy, gx = np.gradient(height)
    nx, ny = -gx * strength * 8.0, gy * strength * 8.0       # OpenGL: +Y up in the image's v, so flip y below
    nz = np.ones_like(nx)
    ln = np.sqrt(nx * nx + ny * ny + nz * nz)
    normal = np.stack([nx / ln, ny / ln, nz / ln], -1) * 0.5 + 0.5
    nimg = (normal * 255).astype(np.uint8)
    # flat colours and the flat tiles have no relief
    x, y, w, h = TILES["swatch"]
    nimg[y:y + h, x:x + w] = (128, 128, 255)
    return img, nimg


def write(outdir):
    os.makedirs(outdir, exist_ok=True)
    img, nimg = build()
    Image.fromarray(img).save(os.path.join(outdir, "north_atlas.png"), optimize=True)
    Image.fromarray(nimg).save(os.path.join(outdir, "north_atlas_n.png"), optimize=True)


if __name__ == "__main__":
    d = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "gen")
    write(d)
    print("atlas ->", d)
