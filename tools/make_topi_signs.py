#!/usr/bin/env python3
"""Topi's signs and cloth, drawn with PIL (local fonts, no paid API): docs/topi.md.

  python3 tools/make_topi_signs.py [lang ... | neutral]    -> textures/localized/<lang>/topi_sign_<name>.png   (warn, ural)
                                                              textures/topi/{towel_a,towel_b,towel_c,wood,rag}.png  (no words)

Words come only from unreal/Lyudmilovsk/Content/Localization/strings_<lang>.json (sign.topi.warn.title, sign.topi.warn.1, sign.topi.warn.2,
sign.topi.ural). `warn` is the old enamel warning plate (blender/topi/props.py SM_Topi_WarnSign, slot TP_SignWarn): yellowed white,
a black border, rust at the edges and the four bolt holes. `ural` is the board at the river (SM_Topi_UralSign, TP_SignUral): white hand-painted
letters on a dark weathered plank. unreal/scripts/import_topi.py imports them to /Game/Topi/Localized/<lang>/T_Sign_<name> (ULyudTopiProps shows
the player's language) and the neutral cloth and wood to /Game/Topi/Props/T_*. Fonts are the repo's fonts/ (OFL), so it runs the same on a Mac and on Linux.
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
STRINGS = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Localization", "strings_%s.json")
OUT_LOC = os.path.join(ROOT, "textures", "localized", "%s")
OUT_NEUTRAL = os.path.join(ROOT, "textures", "topi")
FONT_DIR = os.path.join(ROOT, "fonts")
FONTS = {   # kind -> font files (the repo's, then system ones as a fallback)
    "title": ["DelaGothicOne-Regular.ttf"],
    "bold": ["MPLUS1p-Bold.ttf", "/System/Library/Fonts/Supplemental/Arial Bold.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"],
    "paint": ["MPLUS1p-Bold.ttf", "/System/Library/Fonts/Supplemental/Arial Bold.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"],
    "paint_ja": ["Yomogi-Regular.ttf", "MPLUS1p-Bold.ttf", "/System/Library/Fonts/Hiragino Sans GB.ttc", "/usr/share/fonts/opentype/ipafont-gothic/ipag.ttf"],
    "title_ja": ["MPLUS1p-Bold.ttf", "/System/Library/Fonts/Hiragino Sans GB.ttc", "/usr/share/fonts/opentype/ipafont-gothic/ipag.ttf"],
}
RUST = (134, 66, 30)


def font(kind, lang, size):
    key = kind + "_ja" if lang == "ja" and kind + "_ja" in FONTS else kind
    if lang == "ja" and key == kind:
        key = "title_ja"
    for path in FONTS[key]:
        full = path if os.path.isabs(path) else os.path.join(FONT_DIR, path)
        if os.path.exists(full):
            return ImageFont.truetype(full, size)
    return ImageFont.load_default()


def fit(d, text, kind, lang, size, width, height=None):
    f = font(kind, lang, size)
    while size > 10:
        b = d.textbbox((0, 0), text, font=f)
        if b[2] - b[0] <= width and (height is None or b[3] - b[1] <= height):
            break
        size -= 2
        f = font(kind, lang, size)
    return f


def centred(d, cx, cy, text, f, fill):
    b = d.textbbox((0, 0), text, font=f)
    d.text((cx - (b[2] - b[0]) / 2 - b[0], cy - (b[3] - b[1]) / 2 - b[1]), text, font=f, fill=fill)


# ------------------------------------------------------------------------------------------------ noise

def tnoise(w, h, cx, cy, seed):
    """Seamlessly tiling value noise 0..1 of w x h pixels with cx x cy random cells."""
    r = np.random.RandomState(seed)
    cells = r.rand(cy, cx).astype(np.float32)
    big = np.tile(cells, (3, 3))
    im = Image.fromarray((big * 255).astype(np.uint8)).resize((3 * w, 3 * h), Image.BICUBIC)
    a = np.asarray(im, dtype=np.float32) / 255.0
    return a[h:2 * h, w:2 * w]


def fbm(w, h, base, seed, octaves=4, ax=1.0):
    """Tileable fractal noise 0..1; ax stretches the cells along x (grain)."""
    out = np.zeros((h, w), np.float32)
    amp, tot = 1.0, 0.0
    for o in range(octaves):
        n = base * (2 ** o)
        out += amp * tnoise(w, h, max(2, int(n * ax)), max(2, n), seed + 17 * o)
        tot += amp
        amp *= 0.5
    return out / tot


def to_img(rgb):
    return Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8), "RGB")


def blend(base, colour, mask):
    """base (h, w, 3) float, colour (3,), mask (h, w) 0..1."""
    return base * (1 - mask[..., None]) + np.array(colour, np.float32) * mask[..., None]


# ------------------------------------------------------------------------------------------------ the warning plate

def warn(s, lang):
    W, H = 1024, 732
    r = np.random.RandomState(5)
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    edge = np.minimum(np.minimum(xx, W - 1 - xx), np.minimum(yy, H - 1 - yy))
    # yellowed enamel: stained and sunburnt, darker toward the edges
    base = np.zeros((H, W, 3), np.float32)
    base[:] = (233, 225, 196)
    stain = fbm(W, H, 3, 11, 5)
    base = base * (0.88 + 0.16 * stain)[..., None]
    base = blend(base, (200, 178, 120), np.clip(fbm(W, H, 2, 21, 4) * 1.4 - 0.35, 0, 1) * 0.6)
    im = to_img(base)
    d = ImageDraw.Draw(im)
    black = (22, 20, 18)
    d.rounded_rectangle([26, 26, W - 27, H - 27], radius=26, outline=black, width=17)
    d.rounded_rectangle([54, 54, W - 55, H - 55], radius=14, outline=black, width=4)
    title = s["sign.topi.warn.title"]
    f = fit(d, title, "title", lang, 330, W - 220, 230)
    centred(d, W / 2, 232, title, f, black)
    d.rectangle([110, 388, W - 111, 396], fill=black)
    for y, key in ((476, "sign.topi.warn.1"), (580, "sign.topi.warn.2")):
        t = s[key]
        f = fit(d, t, "bold", lang, 78, W - 240, 80)
        centred(d, W / 2, y, t, f, black)
    # bolt holes where the mesh's bolts are (u = 0.5 +- 0.30 / 0.70, v = 0.5 +- 0.20 / 0.50)
    holes = [(73, 73), (W - 74, 73), (73, H - 74), (W - 74, H - 74)]
    for x, y in holes:
        d.ellipse([x - 17, y - 17, x + 17, y + 17], fill=(60, 36, 20))
        d.ellipse([x - 10, y - 10, x + 10, y + 10], fill=(24, 20, 16))
    a = np.asarray(im, dtype=np.float32).copy()
    # rust: grows from the edges and from under the bolts, runs down in streaks
    rust = np.clip(1.25 - edge / (10 + 46 * fbm(W, H, 3, 31, 4)), 0, 1)
    for x, y in holes:
        dist = np.sqrt((xx - x) ** 2 + (yy - y) ** 2)
        rust = np.maximum(rust, np.clip(1.2 - dist / (40 + 45 * fbm(W, H, 4, 41 + x, 3)), 0, 1))
        for k in range(7):                                      # a streak under the bolt
            sx = x + r.randint(-14, 15)
            L = r.randint(60, 190)
            rust[int(y):int(y) + L, sx - 1:sx + 2] = np.maximum(rust[int(y):int(y) + L, sx - 1:sx + 2], 0.65 * np.linspace(1, 0.1, min(L, H - int(y)))[:, None][:L])
    rust = np.clip(rust * (0.45 + 0.9 * fbm(W, H, 6, 51, 5)), 0, 1)
    a = blend(a, RUST, np.clip(rust * 1.15, 0, 1))
    a = blend(a, (96, 44, 20), np.clip(rust - 0.6, 0, 1) * 1.6)
    # chips of paint: bare dark metal here and there, mostly at the edges
    chip = (fbm(W, H, 14, 61, 3) > 0.80 + 0.10 * np.clip(edge / 220.0, 0, 1)).astype(np.float32)
    a = blend(a, (74, 60, 50), chip * 0.85)
    a += (r.rand(H, W, 1).astype(np.float32) - 0.5) * 8          # grain
    im = to_img(a).filter(ImageFilter.SMOOTH)
    return im


# ------------------------------------------------------------------------------------------------ the board at the river

def ural(s, lang):
    W, H = 1024, 326
    r = np.random.RandomState(7)
    # a dark weathered plank: grain along x, a knot, cracks
    grain = fbm(W, H, 4, 71, 5, ax=0.12)
    fine = tnoise(W, H, 4, 220, 72)
    base = np.zeros((H, W, 3), np.float32)
    base[:] = (62, 53, 44)
    base = base * (0.62 + 0.70 * grain)[..., None] * (0.92 + 0.16 * fine)[..., None]
    base = blend(base, (36, 31, 27), np.clip(tnoise(W, H, 3, 60, 73) - 0.55, 0, 1) * 1.6)
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    for kx, ky, kr in ((170, 250, 26), (840, 70, 20)):                      # knots
        dist = np.sqrt(((xx - kx) / 1.7) ** 2 + (yy - ky) ** 2)
        ring = np.clip(1 - np.abs(dist - kr) / 5.0, 0, 1) * 0.5 + np.clip(1 - dist / kr, 0, 1) * 0.35
        base = blend(base, (24, 20, 17), ring)
    im = to_img(base)
    title = s["sign.topi.ural"]
    # the paint: a mask of the letters, then weathered (flaked, drips, uneven)
    mask = Image.new("L", (W, H), 0)
    md = ImageDraw.Draw(mask)
    f = fit(md, title, "paint", lang, 280, W - 190, H - 80)
    # a little jitter between the letters, as a brush gives
    widths = [md.textlength(c, font=f) for c in title]
    total = sum(widths)
    x = (W - total * 1.04) / 2
    b = md.textbbox((0, 0), title, font=f)
    ytop = (H - (b[3] - b[1])) / 2 - b[1]
    for c, wch in zip(title, widths):
        tile = Image.new("L", (int(wch) + 80, f.size + 80), 0)
        ImageDraw.Draw(tile).text((40, 40 + b[1] * 0 - 0), c, font=f, fill=255)
        tile = tile.rotate(r.uniform(-4, 4), resample=Image.BICUBIC)
        mask.paste(255, (int(x) - 40, int(ytop + r.randint(-5, 6)) - 40), tile)
        x += wch * 1.04
    m = np.asarray(mask.filter(ImageFilter.GaussianBlur(1.4)), dtype=np.float32) / 255.0
    flake = np.clip(fbm(W, H, 20, 81, 3) * 1.8 - 0.55, 0, 1)
    m = m * (1 - 0.8 * flake) * (0.80 + 0.2 * tnoise(W, H, 40, 12, 82))
    # drips under the letters
    drips = np.zeros_like(m)
    cols = np.where((m > 0.9).sum(axis=0) > 0)[0]
    for _ in range(14):
        cx = int(r.choice(cols))
        ys = np.where(m[:, cx] > 0.9)[0]
        y0, L = int(ys.max()), r.randint(14, 56)
        drips[y0:y0 + L, cx:cx + 2] = 0.65
    m = np.clip(np.maximum(m, drips * (m.max() > 0)), 0, 1)
    a = blend(np.asarray(im, dtype=np.float32), (232, 228, 214), m)
    a += (r.rand(H, W, 1).astype(np.float32) - 0.5) * 7
    return to_img(a).filter(ImageFilter.SMOOTH)


# ------------------------------------------------------------------------------------------------ neutral textures

TOWELS = {   # name: (ground, stripe 1, stripe 2) faded
    "a": ((196, 205, 205), (58, 96, 140), (226, 214, 180)),
    "b": ((222, 206, 186), (176, 72, 58), (236, 232, 222)),
    "c": ((150, 170, 132), (232, 196, 96), (80, 112, 84)),
}


def towel(name):
    W, H = 512, 1024
    ground, c1, c2 = TOWELS[name]
    r = np.random.RandomState(ord(name))
    col = np.zeros((H, W, 3), np.float32)
    col[:] = ground
    y = np.arange(H, dtype=np.float32)
    # stripes across the width (along v): a wide, then two thin, at each end, mirrored
    for e in (0, 1):
        pos = (lambda t: t) if e == 0 else (lambda t: H - 1 - t)
        for y0, y1, c in ((90, 170, c1), (190, 205, c2), (222, 236, c1)):
            band = ((pos(y) >= y0) & (pos(y) < y1)).astype(np.float32)
            col = blend(col, c, np.repeat(band[:, None], W, 1))
    # hems: the cloth is tighter at both ends, a plain narrow strip
    for y0, y1 in ((0, 14), (H - 14, H)):
        col[y0:y1] = col[y0:y1] * 0.9
    # terry: loops (fine noise at two scales) lighten and darken the pile; the weft shows as faint lines
    loops = r.rand(H, W).astype(np.float32)
    loops = np.asarray(Image.fromarray((loops * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.8)), dtype=np.float32) / 255.0
    loops = (loops - loops.mean()) / (loops.std() + 1e-6)
    weft = 0.5 + 0.5 * np.sin(y * 2 * math.pi / 5.0)
    pile = 1.0 + 0.10 * loops + 0.03 * weft[:, None] + 0.10 * (fbm(W, H, 4, 90 + ord(name), 4) - 0.5)
    col = col * pile[..., None]
    # faded and dusty with sand: a pale cast and a few darker patches
    col = blend(col, (214, 200, 170), np.clip(fbm(W, H, 3, 95 + ord(name), 4) * 1.2 - 0.45, 0, 1) * 0.35)
    col = blend(col, (96, 84, 70), np.clip(fbm(W, H, 5, 99 + ord(name), 4) * 1.5 - 0.95, 0, 1) * 0.4)
    return to_img(col)


def wood():
    """512 x 512 weathered grey wood, tileable: grain runs up the picture (along v), a few cracks and darker rings."""
    W = H = 512
    grain = fbm(W, H, 4, 111, 5, ax=10.0)
    fine = tnoise(W, H, 200, 6, 112)
    blotch = fbm(W, H, 3, 113, 3)
    v = 0.55 + 0.45 * grain
    base = np.zeros((H, W, 3), np.float32)
    base[:] = (120, 112, 102)
    base = base * v[..., None] * (0.90 + 0.18 * fine)[..., None] * (0.92 + 0.14 * blotch)[..., None]
    # silvered streaks and dark cracks (periodic in x so the tile is seamless)
    r = np.random.RandomState(114)
    x = np.arange(W, dtype=np.float32)
    ycoord = np.arange(H, dtype=np.float32)
    for _ in range(9):
        cx, ln, y0 = r.randint(0, W), r.randint(60, 220), r.randint(0, H)
        prof = np.exp(-(((x - cx + W / 2) % W - W / 2) / 1.3) ** 2)
        along = np.exp(-((((ycoord - y0) + H / 2) % H - H / 2) / ln) ** 2)
        base = blend(base, (34, 30, 27), np.clip(prof[None, :] * along[:, None] * 0.9, 0, 1))
    base = blend(base, (170, 164, 152), np.clip(fbm(W, H, 5, 115, 4, ax=8.0) * 1.6 - 0.95, 0, 1) * 0.5)
    return to_img(base)


def rag():
    """A faded red cotton rag, 256 x 512: a plain weave, a faint stripe, worn thin in places."""
    W, H = 256, 512
    r = np.random.RandomState(121)
    col = np.zeros((H, W, 3), np.float32)
    col[:] = (166, 66, 54)
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    weave = 0.5 + 0.25 * np.sin(xx * 2 * math.pi / 3.0) + 0.25 * np.sin(yy * 2 * math.pi / 3.0)
    thread = r.rand(H, W).astype(np.float32)
    col = col * (0.84 + 0.20 * weave + 0.12 * (thread - 0.5))[..., None]
    col = blend(col, (210, 190, 168), np.clip(fbm(W, H, 3, 122, 4) * 1.3 - 0.55, 0, 1) * 0.5)     # sun-bleached
    band = ((yy > 380) & (yy < 410)).astype(np.float32) * 0.5
    col = blend(col, (200, 176, 150), band)
    col = blend(col, (70, 50, 40), np.clip(fbm(W, H, 6, 123, 4) * 1.6 - 1.05, 0, 1) * 0.5)         # dirt
    return to_img(col)


# ------------------------------------------------------------------------------------------------ wear layers (over every language)
#
# blender/topi/props.py bakes the signs' bodies; the fronts keep the localized picture and get these layers over it, the same
# for every language (Unreal's M_TP_SignWorn: colour = lerp(picture, wear.rgb, wear.a), normal and roughness/metal from here):
#   <name>_wear.png     RGBA: the wear's colour and how much it covers the picture
#   <name>_wear_n.png   tangent-space normal, DirectX green (Unreal)
#   <name>_wear_rm.png  R roughness, G metallic (B unused)
# Pixel rows run top-down with v = 1 at the top (Blender's UVs as props.py lays the fronts out).

class Wear:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.rgb = np.zeros((h, w, 3), np.float32)
        self.a = np.zeros((h, w), np.float32)
        self.height = np.zeros((h, w), np.float32)
        self.rough = np.zeros((h, w), np.float32)
        self.metal = np.zeros((h, w), np.float32)
        self.u, self.v = np.meshgrid((np.arange(w) + 0.5) / w, 1.0 - (np.arange(h) + 0.5) / h)

    def over(self, colour, mask, rough, metal, height=0.0):
        """A layer over what is there: colour (3,) or (h, w, 3), coverage mask 0..1."""
        m = np.clip(mask, 0, 1).astype(np.float32)
        col = np.broadcast_to(np.asarray(colour, np.float32), self.rgb.shape)
        a_new = m + self.a * (1 - m)
        self.rgb = np.where(a_new[..., None] > 1e-6, (col * m[..., None] + self.rgb * (self.a * (1 - m))[..., None]) / np.maximum(a_new, 1e-6)[..., None], 0)
        self.a = a_new
        self.rough = self.rough * (1 - m) + np.asarray(rough, np.float32) * m
        self.metal = self.metal * (1 - m) + np.asarray(metal, np.float32) * m
        self.height += m * height

    def save(self, name, base_rough, base_metal, strength):
        a = self.a
        rgba = np.dstack([self.rgb * 255, a[..., None] * 255])
        Image.fromarray(np.clip(rgba, 0, 255).astype(np.uint8), "RGBA").save(os.path.join(OUT_NEUTRAL, name + "_wear.png"))
        # roughness/metal where nothing covers the picture: the picture's own surface (enamel, paint)
        rough = base_rough * (1 - a) + self.rough
        metal = base_metal * (1 - a) + self.metal
        rm = np.dstack([rough, metal, np.zeros_like(rough)])
        Image.fromarray(np.clip(rm * 255, 0, 255).astype(np.uint8), "RGB").save(os.path.join(OUT_NEUTRAL, name + "_wear_rm.png"))
        h = self.height
        for _ in range(2):                        # soften the steps a little (two 3-tap box passes)
            h = (np.roll(h, 1, 0) + h + np.roll(h, -1, 0)) / 3
            h = (np.roll(h, 1, 1) + h + np.roll(h, -1, 1)) / 3
        gy, gx = np.gradient(h)
        # rows run down (v falls): d/dv = -d/drow
        nx, ny = -gx * strength, gy * strength
        n = np.dstack([nx, ny, np.ones_like(nx)])
        n /= np.linalg.norm(n, axis=2, keepdims=True)
        n[..., 1] = -n[..., 1]                    # DirectX green
        Image.fromarray(np.clip((n * 0.5 + 0.5) * 255, 0, 255).astype(np.uint8), "RGB").save(os.path.join(OUT_NEUTRAL, name + "_wear_n.png"))


def _streak(u, v, x0, v0, length, width0, spread, seed, w):
    """Rust run down from a point (u, v in 0..1): widens and fades downward, broken into drips."""
    r = np.random.RandomState(seed)
    below = np.clip((v0 - v) / length, 0, 1)
    on = (v < v0) & (v > v0 - length)
    width = width0 + spread * below
    drips = fbm(w.w, w.h, 3, seed, 3, ax=14.0)
    lat = (u - x0 - 0.01 * np.sin(v * 40 + r.rand() * 6)) / width
    return np.exp(-lat * lat) * (1 - below) ** 1.4 * on * (0.45 + 0.75 * drips) + np.exp(-((u - x0) ** 2 + (v - v0) ** 2) / (width0 * 0.8) ** 2) * 0.9


def warn_wear():
    """The enamel plate: paint chipped to primer and bare steel at the edges and the bent corner, rust run from the bolts and
    the top edge, the lower part sand-blasted dull with pits."""
    w = Wear(2048, 1464)
    u, v = w.u, w.v
    ex = np.minimum(u, 1 - u) * 0.70
    ey = np.minimum(v, 1 - v) * 0.50
    e = np.minimum(ex, ey)
    corner = np.clip(1 - np.hypot(1 - u, 1 - v) / 0.32, 0, 1)
    n1 = fbm(w.w, w.h, 9, 101, 5)
    n2 = fbm(w.w, w.h, 30, 102, 3)
    blast = np.clip((0.38 - v) / 0.38, 0, 1) ** 1.3
    # sand-blasted: the enamel goes dull and pale, unevenly
    w.over((0.80, 0.77, 0.68), 0.55 * blast * (0.55 + 0.45 * fbm(w.w, w.h, 6, 103, 4, ax=3.0)), 0.8, 0.0, -0.05)
    # chips: mostly round the edges and the bent corner, a few where stones hit the face
    score = n1 * 0.55 + 0.2 * n2 + 0.5 * np.exp(-e / 0.014) + 0.32 * corner + 0.12 * blast
    rs = np.random.RandomState(108)
    for _ in range(14):
        cu, cv, rad = rs.uniform(0.08, 0.92), rs.uniform(0.05, 0.95), rs.uniform(0.004, 0.012)
        d = np.hypot((u - cu) * 0.70, (v - cv) * 0.50) / rad
        score = np.maximum(score, 0.84 - 0.25 * d + 0.12 * n2)
    primer = np.clip((score - 0.70) / 0.010, 0, 1)
    bare = np.clip((score - 0.77) / 0.010, 0, 1)
    w.over((0.47, 0.30, 0.24), primer, 0.75, 0.0, -0.6)
    steel = np.dstack([0.36 + 0.06 * n2, 0.35 + 0.05 * n2, 0.33 + 0.04 * n2])
    w.over(steel, bare, 0.45, 0.85, -0.4)
    rust_on_bare = bare * np.clip((fbm(w.w, w.h, 14, 104, 4) - 0.35) / 0.25, 0, 1)
    w.over((0.36, 0.15, 0.06), rust_on_bare, 0.92, 0.15, 0.2)
    # rust run from the four bolts and down from the top edge
    rust = np.zeros_like(u)
    for k, (bu, bv) in enumerate(((0.07, 0.10), (0.07, 0.90), (0.93, 0.10), (0.93, 0.90))):
        rust = np.maximum(rust, _streak(u, v, bu, bv, 0.42 if bv > 0.5 else 0.10, 0.008, 0.025, 200 + k, w))
    top = np.exp(-np.maximum(1 - v, 0) / 0.02) * (0.35 + 0.65 * fbm(w.w, w.h, 4, 105, 3, ax=10.0))
    top_run = np.clip(1 - (1 - v) / 0.25, 0, 1) ** 3 * (fbm(w.w, w.h, 2, 106, 3, ax=18.0) > 0.58)
    rust = np.maximum(rust, np.maximum(top, top_run * 0.6))
    rust *= 0.55 + 0.45 * n2
    col = np.dstack([0.42 + 0.12 * n2, 0.17 + 0.05 * n2, 0.06 + 0.02 * n2])
    w.over(col, np.clip(rust, 0, 0.9), 0.9, 0.1, 0.1)
    # pits in the blasted part: specks of bare, rusting steel
    pits = (np.random.RandomState(107).rand(w.h, w.w) > 0.996 - 0.01 * blast) * blast
    pits = np.asarray(Image.fromarray((pits * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3)), np.float32) / 255
    w.over((0.30, 0.15, 0.07), pits, 0.9, 0.3, -0.5)
    w.save("warn", 0.30, 0.0, 6.0)


def ural_wear():
    """The painted board: the paint flaking to grey wood, cracks along the grain, grain showing through, rust run from the nails."""
    w = Wear(2048, 652)
    u, v = w.u, w.v
    grain = fbm(w.w, w.h, 8, 301, 5, ax=0.08)
    flake_n = fbm(w.w, w.h, 10, 302, 5, ax=0.35)
    edge = np.exp(-np.minimum(u, 1 - u) / 0.03) + np.exp(-np.minimum(np.abs(v - 0.488), np.minimum(v, 1 - v)) / 0.05)
    # bleached by the weather all over, more where the grain is open
    w.over((0.46, 0.44, 0.40), 0.06 + 0.16 * grain, 0.9, 0.0, 0.0)
    flake_n = 0.6 * flake_n + 0.4 * fbm(w.w, w.h, 40, 304, 3, ax=0.3)
    flakes = np.clip((flake_n + 0.22 * edge - 0.66) / 0.012, 0, 1)
    w.over((0.47, 0.45, 0.40), flakes * 0.92, 0.92, 0.0, -0.5)
    # cracks along the grain (u), a few, wavy, over part of the length
    r = np.random.RandomState(303)
    cracks = np.zeros_like(u)
    for k in range(9):
        c_v = r.uniform(0.03, 0.97)
        u0, u1 = sorted(r.uniform(0, 1, 2))
        if u1 - u0 < 0.2:
            u1 = min(1.0, u0 + 0.35)
        cv = c_v + 0.006 * np.sin(u * r.uniform(8, 20) + r.rand() * 6)
        width = 0.0018 + 0.0012 * np.sin(u * 30)
        along = np.clip((u - u0) / 0.06, 0, 1) * np.clip((u1 - u) / 0.06, 0, 1)
        cracks = np.maximum(cracks, np.exp(-((v - cv) / width) ** 2) * along)
    w.over((0.05, 0.045, 0.04), cracks * 0.95, 0.95, 0.0, -1.2)
    # nails: the board centres are v 0.244 and 0.756; two nails at each end, the lower board's left ones gone
    rust = np.zeros_like(u)
    for k, (nu, nv) in enumerate(((0.035, 0.80), (0.035, 0.71), (0.965, 0.80), (0.965, 0.71), (0.965, 0.29), (0.965, 0.20))):
        rust = np.maximum(rust, _streak(u, v, nu, nv, 0.17, 0.004, 0.01, 400 + k, w))
    holes = np.zeros_like(u)
    for nu, nv in ((0.035, 0.29), (0.035, 0.20)):
        holes = np.maximum(holes, np.exp(-((u - nu) ** 2 * 9 + (v - nv) ** 2) / 0.0035 ** 2 / 9))
    w.over((0.30, 0.13, 0.05), np.clip(rust * 0.85, 0, 1), 0.9, 0.05, 0.1)
    w.over((0.04, 0.03, 0.02), np.clip(holes * 1.5, 0, 1), 0.95, 0.0, -1.0)
    w.save("ural", 0.82, 0.0, 4.0)


def make_neutral():
    os.makedirs(OUT_NEUTRAL, exist_ok=True)
    warn_wear()
    ural_wear()
    for n in TOWELS:
        towel(n).save(os.path.join(OUT_NEUTRAL, "towel_%s.png" % n))
    wood().save(os.path.join(OUT_NEUTRAL, "wood.png"))
    rag().save(os.path.join(OUT_NEUTRAL, "rag.png"))
    print("topi textures", OUT_NEUTRAL)


def make(lang):
    with open(STRINGS % lang, encoding="utf-8") as f:
        s = json.load(f)
    out = OUT_LOC % lang
    os.makedirs(out, exist_ok=True)
    warn(s, lang).save(os.path.join(out, "topi_sign_warn.png"))
    ural(s, lang).save(os.path.join(out, "topi_sign_ural.png"))
    print("topi signs", lang)


if __name__ == "__main__":
    args = sys.argv[1:]
    if not args or "neutral" in args:
        make_neutral()
    for lang in [a for a in args if a != "neutral"] or (["ru", "en", "ja"] if not args else []):
        make(lang)
