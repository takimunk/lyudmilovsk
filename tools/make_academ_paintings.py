#!/usr/bin/env python3
"""Procedural framed pictures for the Academgorodok flats (furniture.py PAINTINGS / AC_FPaint_<name>).

Pure Python + Pillow, no third-party image input, deterministic (one seed per picture):
  python3 tools/make_academ_paintings.py [name ...]

Each picture is first drawn as a simple flat scene (gradients, polygons, clusters of dabs), then
oil paintings are re-painted with oriented brush strokes coarse to fine (strokes follow the edges of
the scene, sampled from blurred copies of it), and get a canvas weave, old yellowed varnish, a soft
vignette and fine craquelure. The photograph is a blurred, grainy sepia print of a generic faceless
sitter; the engraving is cross-hatching laid over a tone map on foxed paper.

Output: textures/academ/paintings/<name>.jpg, 1024 px on the long side, JPEG quality 90. The mean
sRGB colour of each is printed (furniture.py uses it as the flat stand-in colour of the material).
Provenance: docs/academgorodok/SOURCES.md ("Paintings").
"""
import math
import random
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageOps

OUT = Path(__file__).resolve().parent.parent / "textures" / "academ" / "paintings"
SIZES = {"4:3": (1024, 768), "3:2": (1024, 683), "4:5": (819, 1024)}


# --- helpers ---------------------------------------------------------------------------------------
def clamp(v):
    return max(0, min(255, int(round(v))))


def jit(c, rnd, a):
    return tuple(clamp(v + rnd.uniform(-a, a)) for v in c[:3])


def mix(a, b, t):
    return tuple(clamp(a[i] + (b[i] - a[i]) * t) for i in range(3))


def vgrad(w, h, stops):
    """Vertical gradient; stops [(t 0..1, rgb)] sorted by t."""
    col = Image.new("RGB", (1, h))
    for y in range(h):
        t = y / max(1, h - 1)
        for (t0, c0), (t1, c1) in zip(stops, stops[1:]):
            if t <= t1 or (t1 == stops[-1][0]):
                col.putpixel((0, y), mix(c0, c1, 0 if t1 == t0 else max(0.0, min(1.0, (t - t0) / (t1 - t0)))))
                break
    return col.resize((w, h))


def blob(d, rnd, cx, cy, r, cols, n, alpha=255, rr=(0.25, 0.6), sy=1.0):
    """A cluster of n round dabs inside an ellipse (r, r * sy) around (cx, cy)."""
    for _ in range(n):
        a = rnd.uniform(0, 2 * math.pi)
        q = math.sqrt(rnd.random())
        x, y = cx + math.cos(a) * r * q, cy + math.sin(a) * r * q * sy
        s = r * rnd.uniform(*rr) * 0.35
        c = jit(rnd.choice(cols), rnd, 10)
        d.ellipse([x - s, y - s * rnd.uniform(0.6, 1.0), x + s, y + s * rnd.uniform(0.6, 1.0)], fill=c + (alpha,))


def stroke(d, x, y, ang, L, wdt, col):
    dx, dy = math.cos(ang) * L / 2, math.sin(ang) * L / 2
    d.line([(x - dx, y - dy), (x + dx, y + dy)], fill=col, width=max(1, int(wdt)))


def paint(base, rnd, radii=(14, 7, 3), thresh=26, jitter=9, flat_angle=0.0):
    """Re-paints `base` with oriented strokes, coarse to fine (after Hertzmann 1998)."""
    w, h = base.size
    canvas = base.filter(ImageFilter.GaussianBlur(radii[0]))
    d = ImageDraw.Draw(canvas)
    cp = canvas.load()
    for li, R in enumerate(radii):
        ref = base.filter(ImageFilter.GaussianBlur(max(1, R * 0.5)))
        lum = ref.convert("L")
        rp, lp = ref.load(), lum.load()
        o = max(1, R // 2)
        pts = [(x + rnd.uniform(-R / 2, R / 2), y + rnd.uniform(-R / 2, R / 2)) for y in range(0, h, R) for x in range(0, w, R)]
        rnd.shuffle(pts)
        for x, y in pts:
            xi, yi = int(min(w - 1 - o, max(o, x))), int(min(h - 1 - o, max(o, y)))
            c = rp[xi, yi]
            if li:
                cc = cp[xi, yi]
                if abs(c[0] - cc[0]) + abs(c[1] - cc[1]) + abs(c[2] - cc[2]) < thresh:
                    continue
            gx = lp[xi + o, yi] - lp[xi - o, yi]
            gy = lp[xi, yi + o] - lp[xi, yi - o]
            if gx * gx + gy * gy < 9:
                ang = flat_angle + rnd.uniform(-0.35, 0.35)
            else:
                ang = math.atan2(gy, gx) + math.pi / 2 + rnd.uniform(-0.15, 0.15)
            stroke(d, x, y, ang, R * rnd.uniform(1.4, 3.0), R * rnd.uniform(0.6, 1.0), jit(c, rnd, jitter))
    return canvas


def soft_light(img, tex, k):
    t = Image.merge("RGB", [tex] * 3)
    return Image.blend(img, ImageChops.soft_light(img, t), k)


def vignette(img, k):
    w, h = img.size
    g = Image.radial_gradient("L").resize((w, h), Image.BILINEAR)
    m = g.point(lambda v: clamp(255 - k * max(0, v - 90) * 1.6))
    return ImageChops.multiply(img, Image.merge("RGB", [m] * 3))


def oil_finish(img, rnd, varnish=(255, 238, 205), cracks=70):
    w, h = img.size
    weave = Image.new("L", (w, h), 128)
    dw = ImageDraw.Draw(weave)
    for x in range(0, w, 3):
        dw.line([(x, 0), (x, h)], fill=112)
    for y in range(0, h, 3):
        dw.line([(0, y), (w, y)], fill=116)
    noise = Image.effect_noise((w, h), 30)
    tex = Image.blend(weave, noise, 0.5)
    img = soft_light(img, tex, 0.45)
    img = ImageChops.multiply(img, Image.new("RGB", (w, h), varnish))
    img = vignette(img, 0.35)
    over = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    for _ in range(cracks):                                        # craquelure
        x, y = rnd.uniform(0, w), rnd.uniform(0, h)
        a = rnd.uniform(0, 2 * math.pi)
        pts = [(x, y)]
        for _ in range(rnd.randint(4, 12)):
            a += rnd.uniform(-0.9, 0.9)
            x, y = x + math.cos(a) * rnd.uniform(6, 22), y + math.sin(a) * rnd.uniform(6, 22)
            pts.append((x, y))
        d.line(pts, fill=(30, 22, 12, 55), width=1)
    img = Image.alpha_composite(img.convert("RGBA"), over).convert("RGB")
    return img.filter(ImageFilter.UnsharpMask(2, 40, 2))


def mean_hex(img):
    r, g, b = img.convert("RGB").resize((1, 1), Image.BOX).getpixel((0, 0))
    return "#%02X%02X%02X" % (r, g, b)


# --- the pictures ------------------------------------------------------------------------------------
def birch_grove(rnd):
    """Spring birch grove in the manner of Levitan: white trunks, thin green leaves, blue shadows."""
    w, h = SIZES["4:3"]
    hz = int(h * 0.56)
    img = vgrad(w, h, [(0.0, (142, 168, 196)), (0.35, (196, 208, 214)), (0.56, (226, 224, 206)), (0.57, (150, 160, 96)),
                       (1.0, (112, 128, 62))])
    d = ImageDraw.Draw(img, "RGBA")
    for _ in range(40):                                           # clouds
        blob(d, rnd, rnd.uniform(0, w), rnd.uniform(0, h * 0.3), rnd.uniform(40, 110), [(236, 234, 226), (214, 216, 220)], 14, 120, sy=0.35)
    for x in range(-20, w + 20, 14):                              # far woods
        blob(d, rnd, x, hz - rnd.uniform(0, 30), rnd.uniform(18, 40), [(112, 132, 118), (128, 146, 122), (98, 118, 112)], 6, 200, sy=0.8)
    for _ in range(500):                                          # grass, ochre patches
        x, y = rnd.uniform(0, w), rnd.uniform(hz, h)
        stroke(d, x, y, rnd.uniform(-0.3, 0.3), rnd.uniform(10, 40), rnd.uniform(3, 8),
               jit(rnd.choice([(168, 176, 96), (140, 156, 76), (186, 170, 104), (120, 140, 70)]), rnd, 10) + (200,))
    trunks = sorted([(rnd.uniform(0.02, 0.98) * w, rnd.uniform(0, 1)) for _ in range(11)], key=lambda t: t[1])
    for x, depth in trunks:                                       # far to near
        base = hz + 10 + depth * (h - hz - 30)
        tw = 6 + depth * 30
        lean = rnd.uniform(-0.05, 0.05)
        # long blue shadow across the grass
        d.polygon([(x - tw / 2, base), (x + tw / 2, base), (x - 260 - depth * 200, base + 30 + depth * 60),
                   (x - 270 - depth * 220, base + 22 + depth * 50)], fill=(108, 124, 150, 120))
        top = -20
        poly = [(x - tw / 2, base), (x + tw / 2, base), (x + tw * 0.3 + lean * (base - top), top), (x - tw * 0.3 + lean * (base - top), top)]
        d.polygon(poly, fill=(234, 232, 222))
        d.polygon([(x + tw * 0.15, base), (x + tw / 2, base), (x + tw * 0.3 + lean * (base - top), top),
                   (x + tw * 0.1 + lean * (base - top), top)], fill=(178, 178, 170))
        for _ in range(int(10 + depth * 18)):                     # black marks
            t = rnd.uniform(0.0, 1.0)
            yy = base - t * (base - top)
            xx = x + lean * (base - yy) + rnd.uniform(-tw * 0.25, tw * 0.25)
            mw = tw * (0.55 - 0.25 * t) * rnd.uniform(0.3, 0.9)
            d.ellipse([xx - mw, yy - 2 - depth * 3, xx + mw, yy + 2 + depth * 3], fill=(38, 34, 30, 230))
        for _ in range(4):                                        # thin branches
            t = rnd.uniform(0.45, 0.95)
            yy = base - t * (base - top)
            xx = x + lean * (base - yy)
            s = rnd.choice([-1, 1])
            d.line([(xx, yy), (xx + s * rnd.uniform(40, 120), yy - rnd.uniform(30, 90))], fill=(70, 60, 50, 200), width=max(1, int(tw * 0.12)))
    for _ in range(170):                                          # pale young leaves
        x, y = rnd.uniform(-20, w + 20), rnd.uniform(-20, hz * 0.95) ** 1.0
        blob(d, rnd, x, y, rnd.uniform(18, 50), [(170, 196, 110), (192, 210, 128), (140, 170, 92), (206, 214, 150)], 10, 170)
    return oil_finish(paint(img, rnd, (12, 6, 3)), rnd)


def winter_village(rnd):
    """Winter village at dusk in the manner of Shishkin/Yuon: log houses, smoke, pines, snow."""
    w, h = SIZES["3:2"]
    hz = int(h * 0.5)
    img = vgrad(w, h, [(0.0, (108, 128, 168)), (0.3, (176, 170, 186)), (0.5, (222, 184, 172)), (0.51, (206, 210, 226)),
                       (1.0, (232, 234, 240))])
    d = ImageDraw.Draw(img, "RGBA")
    for _ in range(25):
        blob(d, rnd, rnd.uniform(0, w), rnd.uniform(0, h * 0.3), rnd.uniform(50, 120), [(150, 150, 180), (196, 170, 186)], 12, 90, sy=0.3)
    pts = [(0, hz)]                                               # far forest
    x = 0
    while x < w:
        x += rnd.uniform(6, 16)
        pts.append((x, hz - rnd.uniform(18, 60)))
    pts += [(w, hz + 6), (0, hz + 6)]
    d.polygon(pts, fill=(64, 78, 86))
    for _ in range(300):                                          # snow strokes, blue shadows
        y = rnd.uniform(hz, h)
        stroke(d, rnd.uniform(0, w), y, rnd.uniform(-0.15, 0.15), rnd.uniform(20, 70), rnd.uniform(3, 9),
               jit(rnd.choice([(236, 236, 242), (196, 204, 226), (218, 214, 226)]), rnd, 6) + (170,))
    houses = [(0.18, 0.62, 1.0), (0.45, 0.57, 0.7), (0.63, 0.55, 0.55), (0.80, 0.6, 0.85)]
    warm = rnd.randrange(len(houses))
    for i, (fx, fy, s) in enumerate(houses):
        cx, by = fx * w, fy * h
        hw, hh = 95 * s, 70 * s
        d.rectangle([cx - hw, by - hh, cx + hw, by], fill=(104, 72, 50))
        for k in range(1, 7):                                     # log courses
            yy = by - hh * k / 7
            d.line([(cx - hw, yy), (cx + hw, yy)], fill=(80, 55, 40), width=max(1, int(2 * s)))
        d.rectangle([cx + hw * 0.35, by - hh, cx + hw, by], fill=(82, 58, 44, 160))
        rh = 60 * s
        d.polygon([(cx - hw - 14 * s, by - hh), (cx + hw + 14 * s, by - hh), (cx + hw * 0.6, by - hh - rh), (cx - hw * 0.6, by - hh - rh)], fill=(238, 238, 244))
        d.polygon([(cx - hw - 14 * s, by - hh), (cx + hw + 14 * s, by - hh), (cx + hw + 10 * s, by - hh + 8 * s), (cx - hw - 10 * s, by - hh + 8 * s)], fill=(150, 156, 180))
        for k in (-1, 1):
            wx = cx + k * hw * 0.45
            col = (238, 186, 100) if (i == warm or (k > 0 and i == 0)) else (58, 50, 46)
            d.rectangle([wx - 12 * s, by - hh * 0.62, wx + 12 * s, by - hh * 0.28], fill=col)
            d.rectangle([wx - 15 * s, by - hh * 0.66, wx + 15 * s, by - hh * 0.6], fill=(226, 226, 232))
        chx = cx + hw * 0.3
        d.rectangle([chx - 7 * s, by - hh - rh * 0.9, chx + 7 * s, by - hh - rh * 0.4], fill=(120, 90, 80))
        sx, sy = chx, by - hh - rh * 0.95
        for k in range(14):                                       # smoke
            sx += rnd.uniform(-4, 10)
            sy -= rnd.uniform(8, 16)
            blob(d, rnd, sx, sy, 10 + k * 2.5 * s, [(200, 198, 206), (180, 178, 192)], 4, 70)
        d.rectangle([cx - hw - 60 * s, by - 18 * s, cx - hw, by], fill=(90, 70, 56, 200))     # fence
    for fx, sc in ((0.03, 1.4), (0.97, 1.2), (0.34, 0.6), (0.92, 0.7)):     # pines
        cx, by = fx * w, h * (0.75 if sc > 1 else 0.58)
        d.line([(cx, by), (cx, by - 300 * sc)], fill=(56, 40, 32), width=int(8 * sc))
        for k in range(9):
            yy = by - 40 * sc - k * 30 * sc
            r = (80 - k * 7) * sc
            d.polygon([(cx - r, yy), (cx + r, yy), (cx, yy - 55 * sc)], fill=jit((40, 62, 48), rnd, 8))
            d.line([(cx - r * 0.9, yy - 2), (cx + r * 0.6, yy - 6)], fill=(226, 230, 240, 160), width=max(1, int(3 * sc)))
    for k in (-1, 1):                                             # sledge track
        d.line([(w * 0.55 + k * 10 + t * 10, h - t * (h - hz - 40)) for t in [i / 10 for i in range(11)]],
               fill=(160, 170, 200, 160), width=4)
    return oil_finish(paint(img, rnd, (12, 6, 3)), rnd)


def lilac_still_life(rnd):
    """Soviet still life of the 1950s: lilac in a glass jar, white cloth, two apples, dark ground."""
    w, h = SIZES["4:5"]
    img = vgrad(w, h, [(0.0, (62, 46, 34)), (0.62, (86, 64, 44)), (0.63, (214, 208, 192)), (1.0, (186, 180, 166))])
    d = ImageDraw.Draw(img, "RGBA")
    for _ in range(260):
        stroke(d, rnd.uniform(0, w), rnd.uniform(0, h * 0.62), rnd.uniform(-1.2, 1.2), rnd.uniform(30, 90), rnd.uniform(8, 20),
               jit(rnd.choice([(70, 52, 36), (54, 40, 30), (92, 70, 46)]), rnd, 8) + (110,))
    for _ in range(40):                                           # cloth folds
        x = rnd.uniform(0, w)
        stroke(d, x, rnd.uniform(h * 0.66, h), rnd.uniform(-0.25, 0.25), rnd.uniform(60, 200), rnd.uniform(4, 12),
               jit(rnd.choice([(176, 172, 162), (232, 228, 214)]), rnd, 6) + (90,))
    jx0, jx1, jy0, jy1 = w * 0.40, w * 0.60, h * 0.50, h * 0.74   # glass jar
    d.rounded_rectangle([jx0, jy0, jx1, jy1], 18, fill=(120, 124, 108, 200))
    for k in range(9):                                            # stems in the water
        x = rnd.uniform(jx0 + 10, jx1 - 10)
        d.line([(x, jy1 - 10), (x + rnd.uniform(-20, 20), jy0 - 20)], fill=(70, 96, 50, 220), width=4)
    d.line([(jx0 + 14, jy0 + 10), (jx0 + 14, jy1 - 14)], fill=(236, 240, 232, 220), width=6)
    d.line([(jx1 - 18, jy0 + 30), (jx1 - 18, jy1 - 30)], fill=(214, 220, 210, 160), width=3)
    d.ellipse([jx0 - 6, jy1 - 10, jx1 + 6, jy1 + 10], fill=(150, 146, 128, 160))
    for _ in range(40):                                           # leaves
        x, y = w * 0.5 + rnd.uniform(-0.36, 0.36) * w, h * 0.42 + rnd.uniform(-0.12, 0.14) * h
        a = rnd.uniform(0, 2 * math.pi)
        L = rnd.uniform(40, 70)
        d.polygon([(x, y), (x + math.cos(a - 0.5) * L * 0.6, y + math.sin(a - 0.5) * L * 0.6), (x + math.cos(a) * L, y + math.sin(a) * L),
                   (x + math.cos(a + 0.5) * L * 0.6, y + math.sin(a + 0.5) * L * 0.6)], fill=jit((60, 92, 46), rnd, 14))
    purple = [(150, 100, 172), (172, 126, 194), (122, 80, 150), (190, 150, 210)]
    white = [(232, 226, 236), (212, 206, 224), (244, 240, 244), (196, 190, 210)]
    for k in range(40):                                           # panicles, back to front
        a = rnd.uniform(0, 2 * math.pi)
        q = rnd.random() ** 0.8
        cx, cy = w * 0.5 + math.cos(a) * q * w * 0.32, h * 0.33 + math.sin(a) * q * h * 0.2
        cols = white if rnd.random() < 0.35 else purple
        ang = math.atan2(cy - h * 0.62, cx - w * 0.5)
        L = rnd.uniform(70, 130)
        for j in range(170):
            t = rnd.random()
            px = cx + math.cos(ang) * L * (t - 0.5) + rnd.gauss(0, 1) * (1 - t) * 22
            py = cy + math.sin(ang) * L * (t - 0.5) + rnd.gauss(0, 1) * (1 - t) * 22
            s = rnd.uniform(3, 7)
            c = jit(rnd.choice(cols), rnd, 10)
            if rnd.random() < 0.3:
                c = mix(c, (60, 40, 60), 0.35)
            d.ellipse([px - s, py - s, px + s, py + s], fill=c + (235,))
    for ax, ay, r in ((w * 0.22, h * 0.78, 52), (w * 0.76, h * 0.8, 46)):     # apples
        d.ellipse([ax - r, ay - r, ax + r, ay + r], fill=(176, 58, 40))
        d.ellipse([ax - r * 0.2, ay - r * 0.8, ax + r * 0.9, ay + r * 0.6], fill=(206, 160, 66, 170))
        d.ellipse([ax - r * 0.55, ay - r * 0.6, ax - r * 0.2, ay - r * 0.3], fill=(246, 230, 200, 200))
        d.ellipse([ax - r * 0.9, ay + r * 0.75, ax + r * 0.9, ay + r * 1.1], fill=(120, 110, 96, 120))
    return oil_finish(paint(img, rnd, (10, 5, 2), thresh=30), rnd)


def harbour_1950s(rnd):
    """Socialist-realist northern harbour, early 1950s: white steamship, cranes, timber, gulls."""
    w, h = SIZES["3:2"]
    hz = int(h * 0.46)
    img = vgrad(w, h, [(0.0, (96, 138, 192)), (0.45, (196, 212, 226)), (0.46, (88, 112, 130)), (1.0, (62, 92, 112))])
    cl = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    dc = ImageDraw.Draw(cl)
    for _ in range(9):                                            # big white cumulus clouds
        cx, cy = rnd.uniform(0, w), rnd.uniform(h * 0.08, h * 0.3)
        r = rnd.uniform(70, 150)
        blob(dc, rnd, cx, cy + r * 0.3, r * 1.2, [(168, 176, 194)], 40, 200, rr=(0.4, 0.8), sy=0.25)
        for j in range(6):
            blob(dc, rnd, cx + rnd.uniform(-r, r), cy - rnd.uniform(0, r * 0.4), r * 0.55, [(250, 250, 246), (232, 234, 238), (214, 218, 228)], 30, 230, rr=(0.4, 0.9))
    img = Image.alpha_composite(img.convert("RGBA"), cl.filter(ImageFilter.GaussianBlur(5))).convert("RGB")
    d = ImageDraw.Draw(img, "RGBA")
    pts = [(0, hz)] + [(x, hz - 14 - 10 * math.sin(x / 70.0) - rnd.uniform(0, 6)) for x in range(0, w + 40, 40)] + [(w, hz + 4), (0, hz + 4)]
    d.polygon(pts, fill=(120, 138, 150))                          # far shore
    for _ in range(420):                                          # waves
        y = rnd.uniform(hz + 4, h * 0.8)
        stroke(d, rnd.uniform(0, w), y, rnd.uniform(-0.06, 0.06), rnd.uniform(10, 40), rnd.uniform(2, 5),
               jit(rnd.choice([(140, 168, 184), (52, 80, 100), (186, 204, 214)]), rnd, 8) + (170,))
    # steamship
    sx0, sx1, wl = w * 0.12, w * 0.62, h * 0.62
    d.polygon([(sx0, wl - 48), (sx1 + 30, wl - 56), (sx1, wl), (sx0 + 22, wl)], fill=(234, 234, 228))
    d.polygon([(sx0 + 18, wl - 10), (sx1 + 4, wl - 10), (sx1, wl), (sx0 + 22, wl)], fill=(150, 44, 36))
    d.line([(sx0 + 4, wl - 40), (sx1 + 24, wl - 47)], fill=(40, 40, 46), width=4)
    d.rectangle([sx0 + 120, wl - 92, sx1 - 70, wl - 50], fill=(240, 238, 232))
    d.rectangle([sx0 + 170, wl - 120, sx1 - 130, wl - 92], fill=(236, 234, 228))
    for k in range(14):
        x = sx0 + 130 + k * (sx1 - sx0 - 210) / 14
        d.ellipse([x, wl - 80, x + 7, wl - 73], fill=(50, 56, 64))
    fx = (sx0 + sx1) / 2 + 10
    d.polygon([(fx - 22, wl - 120), (fx + 22, wl - 120), (fx + 30, wl - 190), (fx - 14, wl - 190)], fill=(36, 36, 38))
    d.polygon([(fx - 18, wl - 160), (fx + 25, wl - 160), (fx + 27, wl - 175), (fx - 16, wl - 175)], fill=(176, 40, 34))
    for k in range(10):                                           # funnel smoke
        blob(d, rnd, fx + 30 + k * 26, wl - 200 - k * 8, 14 + k * 3, [(150, 150, 154), (190, 190, 194)], 4, 90)
    for mx in (sx0 + 80, sx1 - 30):
        d.line([(mx, wl - 50), (mx + 6, wl - 230)], fill=(60, 54, 50), width=4)
    d.line([(sx0 + 80, wl - 225), (sx1 - 24, wl - 225)], fill=(60, 54, 50, 180), width=1)
    d.rectangle([0, wl + 6, w, wl + 12], fill=(40, 60, 76, 160))   # reflection shadow
    # quay
    qy = h * 0.72
    d.polygon([(0, qy), (w, qy - 20), (w, h), (0, h)], fill=(138, 132, 122))
    d.polygon([(0, qy), (w, qy - 20), (w, qy - 8), (0, qy + 12)], fill=(96, 90, 84))
    for _ in range(160):
        stroke(d, rnd.uniform(0, w), rnd.uniform(qy, h), rnd.uniform(-0.2, 0.2), rnd.uniform(20, 60), rnd.uniform(4, 10),
               jit(rnd.choice([(150, 144, 132), (120, 114, 106), (164, 154, 136)]), rnd, 8) + (140,))
    for k in range(5):                                            # timber stacks
        tx, ty = w * (0.55 + k * 0.09), qy + 30 + (k % 2) * 25
        tw_, th_ = 90, 46
        d.rectangle([tx, ty, tx + tw_, ty + th_], fill=(196, 156, 94))
        for j in range(1, 5):
            d.line([(tx, ty + th_ * j / 5), (tx + tw_, ty + th_ * j / 5)], fill=(150, 112, 64), width=2)
        d.polygon([(tx + tw_, ty), (tx + tw_ + 18, ty - 10), (tx + tw_ + 18, ty + th_ - 10), (tx + tw_, ty + th_)], fill=(170, 130, 76))
    for cx in (w * 0.7, w * 0.88):                                # portal cranes
        base = qy - 4
        top = h * 0.2
        for s in (-1, 1):
            d.line([(cx + s * 26, base), (cx + s * 10, top + 60)], fill=(116, 58, 40), width=6)
        for j in range(8):
            y0 = base - j * (base - top - 60) / 8
            y1 = base - (j + 1) * (base - top - 60) / 8
            d.line([(cx - 24 + j * 1.8, y0), (cx + 22 - j * 1.8, y1)], fill=(116, 58, 40), width=2)
        d.rectangle([cx - 22, top + 30, cx + 26, top + 62], fill=(126, 64, 44))
        d.line([(cx, top + 40), (cx - 190, top - 30)], fill=(116, 58, 40), width=5)
        d.line([(cx - 190, top - 30), (cx - 186, wl - 70)], fill=(40, 40, 40), width=1)
        d.rectangle([cx - 204, wl - 76, cx - 168, wl - 60], fill=(190, 150, 90))
    for _ in range(14):                                           # dock workers
        x, y = rnd.uniform(w * 0.05, w * 0.95), rnd.uniform(qy + 10, h * 0.95)
        s = 0.6 + (y - qy) / (h - qy) * 0.9
        c = rnd.choice([(46, 52, 70), (70, 60, 50), (60, 70, 80)])
        d.rectangle([x - 5 * s, y - 26 * s, x + 5 * s, y], fill=c)
        d.ellipse([x - 4 * s, y - 35 * s, x + 4 * s, y - 26 * s], fill=(196, 150, 120))
    for _ in range(12):                                           # gulls
        x, y = rnd.uniform(0, w), rnd.uniform(h * 0.05, h * 0.4)
        s = rnd.uniform(6, 14)
        d.line([(x - s, y - s * 0.4), (x, y), (x + s, y - s * 0.4)], fill=(250, 250, 250, 230), width=2)
    return oil_finish(paint(img, rnd, (12, 6, 3)), rnd)


def family_portrait(rnd):
    """1940s studio photograph, sepia: a soft, generic, faceless sitter in a suit and round glasses."""
    w, h = SIZES["4:5"]
    g = Image.radial_gradient("L").resize((w, h)).point(lambda v: clamp(186 - v * 0.38))
    d = ImageDraw.Draw(g)
    cx = w * 0.5
    d.ellipse([cx - w * 0.5, h * 0.66, cx + w * 0.5, h * 1.35], fill=46)            # shoulders, dark suit
    d.polygon([(cx - 70, h * 0.66), (cx + 70, h * 0.66), (cx, h * 0.86)], fill=214)  # shirt
    d.polygon([(cx - 14, h * 0.68), (cx + 14, h * 0.68), (cx + 20, h * 0.86), (cx, h * 0.9), (cx - 20, h * 0.86)], fill=70)  # tie
    d.polygon([(cx - 70, h * 0.66), (cx - 6, h * 0.86), (cx - 120, h * 0.95)], fill=36)  # lapels
    d.polygon([(cx + 70, h * 0.66), (cx + 6, h * 0.86), (cx + 120, h * 0.95)], fill=36)
    d.rectangle([cx - 55, h * 0.56, cx + 55, h * 0.68], fill=140)                    # neck
    hx, hy, rx, ry = cx, h * 0.42, w * 0.17, h * 0.17
    d.ellipse([hx - rx, hy - ry, hx + rx, hy + ry], fill=168)                         # head
    d.ellipse([hx + rx * 0.35, hy - ry * 0.8, hx + rx * 1.0, hy + ry * 0.95], fill=150)  # shadow side
    d.chord([hx - rx * 1.03, hy - ry * 1.1, hx + rx * 1.03, hy + ry * 0.0], 180, 360, fill=74)  # hair
    d.ellipse([hx - rx * 0.9, hy - ry * 0.75, hx + rx * 0.8, hy - ry * 0.35], fill=168)          # forehead
    d.line([(hx - rx * 0.35, hy - ry * 1.05), (hx - rx * 0.3, hy - ry * 0.7)], fill=110, width=4)   # parting
    for s in (-1, 1):
        d.ellipse([hx + s * rx * 0.98 - 14, hy - 10, hx + s * rx * 0.98 + 14, hy + 40], fill=150)   # ears
        ex = hx + s * rx * 0.42
        d.ellipse([ex - 34, hy - 6 - 26, ex + 34, hy - 6 + 26], outline=40, width=5)               # glasses
        d.ellipse([ex - 10, hy - 14, ex + 10, hy], fill=96)
    d.line([(hx - rx * 0.1, hy - 6), (hx + rx * 0.1, hy - 6)], fill=40, width=4)
    d.polygon([(hx, hy), (hx + 14, hy + 42), (hx - 6, hy + 44)], fill=140)            # nose shade
    d.line([(hx - 28, hy + 72), (hx + 26, hy + 70)], fill=110, width=5)               # mouth
    g = g.filter(ImageFilter.GaussianBlur(7))                                          # soft focus
    g = Image.blend(g, Image.effect_noise((w, h), 60), 0.12)                           # film grain
    d = ImageDraw.Draw(g)
    for _ in range(5):                                                                 # scratches, spots
        x, y = rnd.uniform(0, w), rnd.uniform(0, h)
        d.line([(x, y), (x + rnd.uniform(-60, 60), y + rnd.uniform(-20, 40))], fill=clamp(rnd.uniform(200, 240)), width=1)
    for _ in range(30):
        x, y, r = rnd.uniform(0, w), rnd.uniform(0, h), rnd.uniform(1, 3.5)
        d.ellipse([x - r, y - r, x + r, y + r], fill=rnd.choice([30, 230]))
    img = ImageOps.colorize(g, black=(34, 22, 14), white=(238, 224, 194), mid=(146, 112, 78))
    return vignette(img, 0.45)


def engraving_monastery(rnd):
    """19th-century steel engraving: a monastery with onion domes over a river, cross-hatched."""
    w, h = SIZES["4:3"]
    t = vgrad(w, h, [(0.0, (206, 206, 206)), (0.5, (240, 240, 240)), (0.6, (240, 240, 240)), (1.0, (200, 200, 200))]).convert("L")
    d = ImageDraw.Draw(t)
    for _ in range(8):                                            # cloud bands
        y = rnd.uniform(h * 0.05, h * 0.3)
        x = rnd.uniform(-100, w)
        d.ellipse([x, y, x + rnd.uniform(200, 500), y + rnd.uniform(14, 40)], fill=222)
    hz = h * 0.58
    d.polygon([(0, hz - 30), (w * 0.3, hz - 70), (w * 0.7, hz - 60), (w, hz - 20), (w, hz), (0, hz)], fill=176)   # far hill
    mx = w * 0.56
    d.rectangle([mx - 260, hz - 70, mx + 240, hz - 10], fill=190)                     # wall
    for k in range(11):
        x = mx - 250 + k * 46
        d.rectangle([x, hz - 82, x + 20, hz - 70], fill=190)
    d.rectangle([mx - 260, hz - 70, mx - 220, hz - 10], fill=120)                     # tower shadow
    d.rectangle([mx - 120, hz - 200, mx + 40, hz - 70], fill=200)                     # cathedral
    d.polygon([(mx - 130, hz - 200), (mx + 50, hz - 200), (mx - 40, hz - 240)], fill=150)
    d.rectangle([mx + 10, hz - 200, mx + 40, hz - 70], fill=128)
    for dx, r, hh in ((-80, 26, 230), (-40, 34, 260), (0, 26, 230)):                  # drums and onion domes
        x = mx + dx
        d.rectangle([x - r * 0.7, hz - hh, x + r * 0.7, hz - 200], fill=186)
        d.ellipse([x - r, hz - hh - r * 1.6, x + r, hz - hh + r * 0.2], fill=80)
        d.polygon([(x - r * 0.5, hz - hh - r * 1.4), (x + r * 0.5, hz - hh - r * 1.4), (x, hz - hh - r * 2.4)], fill=80)
        d.line([(x, hz - hh - r * 2.4), (x, hz - hh - r * 3.2)], fill=40, width=3)
        d.line([(x - 8, hz - hh - r * 3.0), (x + 8, hz - hh - r * 3.0)], fill=40, width=3)
        for k in range(3):
            d.rectangle([x - 6 + (k - 1) * 14, hz - hh + 10, x + 2 + (k - 1) * 14, hz - hh + 26], fill=60)
    bx = mx + 160                                                 # bell tower
    for k, (bw_, bh_) in enumerate(((60, 100), (46, 70), (34, 60))):
        y0 = hz - 70 - sum(v[1] for v in ((60, 100), (46, 70), (34, 60))[:k + 1])
        d.rectangle([bx - bw_ / 2, y0, bx + bw_ / 2, y0 + bh_], fill=196)
        d.rectangle([bx + bw_ / 6, y0, bx + bw_ / 2, y0 + bh_], fill=130)
        d.rectangle([bx - 8, y0 + 14, bx + 8, y0 + bh_ - 14], fill=50)
    d.polygon([(bx - 18, hz - 300), (bx + 18, hz - 300), (bx, hz - 380)], fill=90)
    d.line([(bx, hz - 380), (bx, hz - 410)], fill=40, width=3)
    d.rectangle([0, hz, w, h * 0.8], fill=196)                    # river
    for _ in range(40):
        x = rnd.uniform(mx - 260, mx + 240)
        d.line([(x, hz + 4), (x, hz + rnd.uniform(20, 80))], fill=150, width=rnd.randint(4, 14))
    for bxx in (w * 0.3, w * 0.75):                               # boats
        d.polygon([(bxx - 50, hz + 60), (bxx + 50, hz + 60), (bxx + 36, hz + 76), (bxx - 36, hz + 76)], fill=60)
        d.line([(bxx - 10, hz + 60), (bxx - 4, hz + 20)], fill=50, width=2)
    d.polygon([(0, h * 0.78), (w * 0.4, h * 0.82), (w, h * 0.86), (w, h), (0, h)], fill=110)   # near bank
    for _ in range(26):                                           # trees on the left
        tx, ty, tr = rnd.uniform(0, w * 0.22), rnd.uniform(h * 0.2, h * 0.75), rnd.uniform(40, 90)
        for _ in range(8):
            a, q = rnd.uniform(0, 2 * math.pi), rnd.uniform(0, tr * 0.7)
            r = tr * rnd.uniform(0.25, 0.5)
            d.ellipse([tx + math.cos(a) * q - r, ty + math.sin(a) * q - r, tx + math.cos(a) * q + r, ty + math.sin(a) * q + r],
                      fill=rnd.choice([44, 60, 80]))
    d.line([(w * 0.11, h), (w * 0.12, h * 0.4)], fill=40, width=14)
    t = t.filter(ImageFilter.GaussianBlur(1))
    ink = Image.new("L", (w, h), 255)
    for thr, ang, sp in ((214, 0.0, 7), (176, 0.8, 6), (140, -0.8, 5), (104, 1.57, 5), (70, 0.3, 4), (44, -0.3, 3)):
        lines = Image.new("L", (w, h), 255)
        dl = ImageDraw.Draw(lines)
        c, s = math.cos(ang), math.sin(ang)
        diag = math.hypot(w, h)
        k = -diag
        while k < diag:
            pts = []
            for j in range(0, 41):
                u = -diag + 2 * diag * j / 40
                off = k + math.sin(u * 0.02 + k * 0.1) * 1.2
                pts.append((w / 2 + c * u - s * off, h / 2 + s * u + c * off))
            dl.line(pts, fill=0, width=1)
            k += sp
        mask = t.point(lambda v, thr=thr: 255 if v < thr else 0)
        ink = ImageChops.darker(ink, ImageChops.lighter(lines, ImageChops.invert(mask)))
    edges = t.filter(ImageFilter.FIND_EDGES).point(lambda v: 0 if v > 6 else 255)
    ink = ImageChops.darker(ink, edges)
    paper = Image.new("RGB", (w, h), (236, 226, 198))
    pd = ImageDraw.Draw(paper, "RGBA")
    for _ in range(60):                                           # foxing
        x, y, r = rnd.uniform(0, w), rnd.uniform(0, h), rnd.uniform(2, 14)
        pd.ellipse([x - r, y - r, x + r, y + r], fill=(170, 120, 70, rnd.randint(20, 60)))
    paper = soft_light(paper, Image.effect_noise((w, h), 30), 0.4)
    img = Image.composite(paper, Image.new("RGB", (w, h), (36, 30, 28)), ink.filter(ImageFilter.GaussianBlur(0.5)))
    return vignette(img, 0.2)


PICTURES = {"birch_grove": birch_grove, "winter_village": winter_village, "lilac_still_life": lilac_still_life,
            "harbour_1950s": harbour_1950s, "family_portrait": family_portrait, "engraving_monastery": engraving_monastery}


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    for i, name in enumerate(sys.argv[1:] or PICTURES):
        img = PICTURES[name](random.Random(1000 + list(PICTURES).index(name)))
        dest = OUT / (name + ".jpg")
        img.save(dest, "JPEG", quality=90, optimize=True)
        print("%-20s %s %s %s" % (name, img.size, mean_hex(img), dest))
