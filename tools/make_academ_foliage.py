#!/usr/bin/env python3
"""Leaf cards for the Academgorodok's square (procedural, no photos): RGBA atlases for alpha-card foliage.

  python3 tools/make_academ_foliage.py      -> textures/academ/foliage_hedge.png, foliage_flowers.png

foliage_hedge: a clipped privet/lilac hedge seen close: hundreds of small oval leaves in several greens, lit from
above, on a transparent background (the alpha is the card's mask). foliage_flowers: a clump of day-lily blades with
orange and yellow trumpets and some marigold heads, for the beds round the fountain and along the facade.
"""
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "textures", "academ")
N = 1024


def leaf(d, cx, cy, ln, wd, ang, col):
    pts = []
    for i in range(24):
        t = 2 * math.pi * i / 24
        x, y = math.cos(t) * ln / 2, math.sin(t) * wd / 2 * (1 - 0.35 * math.cos(t))
        ca, sa = math.cos(ang), math.sin(ang)
        pts.append((cx + x * ca - y * sa, cy + x * sa + y * ca))
    d.polygon(pts, fill=col)
    # midrib
    ca, sa = math.cos(ang), math.sin(ang)
    d.line([(cx - ca * ln * 0.45, cy - sa * ln * 0.45), (cx + ca * ln * 0.45, cy + sa * ln * 0.45)],
           fill=(max(col[0] - 25, 0), max(col[1] - 30, 0), max(col[2] - 20, 0), 255), width=1)


def hedge(path):
    rnd = random.Random(1947)
    im = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    for i in range(5200):
        x, y = rnd.uniform(0, N), rnd.uniform(0, N)
        # denser and lighter at the top (the sunlit clipped face), darker inside
        top = 1.0 - y / N
        if rnd.random() > 0.55 + 0.45 * top:
            continue
        g = rnd.uniform(0.55, 1.0) * (0.75 + 0.35 * top)
        col = (int(48 * g + rnd.uniform(-8, 8)), int(92 * g + rnd.uniform(-10, 10)), int(34 * g + rnd.uniform(-6, 6)), 255)
        leaf(d, x, y, rnd.uniform(22, 38), rnd.uniform(11, 17), rnd.uniform(0, math.pi), col)
    im = im.filter(ImageFilter.SMOOTH)
    im.save(path)


def flowers(path):
    rnd = random.Random(31)
    im = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    # blades from the bottom edge
    for i in range(260):
        x0 = rnd.uniform(40, N - 40)
        h = rnd.uniform(0.45, 0.85) * N
        bend = rnd.uniform(-0.35, 0.35)
        w = rnd.uniform(9, 16)
        g = rnd.uniform(0.6, 1.0)
        col = (int(60 * g), int(110 * g), int(40 * g), 255)
        pts_l, pts_r = [], []
        for k in range(12):
            t = k / 11
            x = x0 + bend * h * t * t
            y = N - h * t
            ww = w * (1 - t) + 1
            pts_l.append((x - ww / 2, y))
            pts_r.append((x + ww / 2, y))
        d.polygon(pts_l + pts_r[::-1], fill=col)
    # day lilies (orange trumpets) and marigolds (yellow-orange discs) in the upper half
    for i in range(70):
        x, y = rnd.uniform(60, N - 60), rnd.uniform(0.08, 0.5) * N
        if rnd.random() < 0.6:
            c = rnd.choice([(232, 112, 30), (240, 140, 40), (226, 90, 26)])
            for p in range(6):
                a = 2 * math.pi * p / 6 + rnd.uniform(-0.2, 0.2)
                leaf(d, x + math.cos(a) * 14, y + math.sin(a) * 14, 34, 13, a, c + (255,))
            d.ellipse([x - 6, y - 6, x + 6, y + 6], fill=(250, 200, 60, 255))
        else:
            c = rnd.choice([(250, 190, 40), (245, 150, 30)])
            r = rnd.uniform(10, 16)
            d.ellipse([x - r, y - r, x + r, y + r], fill=c + (255,))
            d.ellipse([x - r * 0.4, y - r * 0.4, x + r * 0.4, y + r * 0.4], fill=(170, 90, 20, 255))
    im = im.filter(ImageFilter.SMOOTH)
    im.save(path)


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    hedge(os.path.join(OUT, "foliage_hedge.png"))
    flowers(os.path.join(OUT, "foliage_flowers.png"))
    print("wrote", OUT)
