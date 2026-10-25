#!/usr/bin/env python3
"""Leaf-card atlas for the Academgorodok house plants (procedural, PIL only, no photos).

  python3 tools/make_academ_houseplants.py      -> textures/academ/foliage_houseplants.png

A 1024 x 1024 RGBA atlas of 2 x 2 cells (alpha = the card's mask), used by blender/academgorodok/houseplants.py
(material AC_PLeafCards, CARD_CELLS):
  top left    sprengeri: an asparagus fern (A. densiflorus 'Sprengeri') spray: a wiry stem from the
              bottom centre of the cell to its top with whorls of short flat needles (cladodes) all along it;
  top right   setaceus: a lace fern (A. setaceus) frond: a triangular, flat, finely divided spray of
              bristles on side branches that shorten towards the tip.
  bottom row  benjamina_a / _b: a weeping-fig twig with alternate pointed glossy leaves (dense crowns).
The stem end of each spray is at the bottom of its cell (UV v0).
"""
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "textures", "academ", "foliage_houseplants.png")
N = 1024
SS = 2                      # supersampling
C = N // 2 * SS             # cell size while drawing


def green(rnd, light=1.0):
    g = rnd.uniform(0.75, 1.1) * light
    return (int(min(255, 62 * g)), int(min(255, 118 * g)), int(min(255, 40 * g)), 255)


def sprengeri(d, ox, oy, rnd):
    # stem: from the bottom centre, gently curving up to the top
    pts = []
    bend = rnd.uniform(-0.06, 0.06)
    for i in range(41):
        t = i / 40
        x = ox + C * (0.5 + bend * math.sin(math.pi * t) + 0.04 * math.sin(7 * t + rnd.random()))
        y = oy + C * (0.98 - 0.94 * t)
        pts.append((x, y))
    d.line(pts, fill=(70, 96, 40, 255), width=3 * SS)
    # whorls of needles all along, longer in the middle, a dense plume
    for i in range(6, 41):
        t = i / 40
        x, y = pts[i]
        L = C * (0.08 + 0.12 * math.sin(math.pi * min(1.0, t * 1.05)))
        for k in range(rnd.randint(14, 18)):
            a = rnd.uniform(0, 2 * math.pi)
            l = L * rnd.uniform(0.6, 1.0)
            # needles point outwards and a bit up (towards the tip)
            dx, dy = math.cos(a) * l, math.sin(a) * l * 0.8 - l * 0.25
            col = green(rnd, 0.85 + 0.35 * (1 - abs(math.sin(a))))
            d.line([(x, y), (x + dx, y + dy)], fill=col, width=int(2.2 * SS))
            # a few side branchlets with their own needles
            if k == 0 and rnd.random() < 0.6:
                bx, by = x + dx * 1.2, y + dy * 1.2
                for q in range(5):
                    a2 = rnd.uniform(0, 2 * math.pi)
                    l2 = L * 0.5
                    d.line([(bx, by), (bx + math.cos(a2) * l2, by + math.sin(a2) * l2)], fill=green(rnd), width=2 * SS)


def setaceus(d, ox, oy, rnd):
    # rachis
    pts = []
    for i in range(31):
        t = i / 30
        pts.append((ox + C * (0.5 + 0.03 * math.sin(4 * t)), oy + C * (0.98 - 0.95 * t)))
    d.line(pts, fill=(60, 84, 36, 255), width=2 * SS)
    for i in range(3, 31):
        t = i / 30
        x, y = pts[i]
        L = C * 0.42 * (1.0 - t) ** 0.9 + C * 0.03
        for side in (-1, 1):
            if (i + (side > 0)) % 2:
                continue
            # side branch, slanted to the tip, with fine bristles
            ang = math.radians(rnd.uniform(25, 40))
            ex, ey = x + side * L * math.cos(ang), y - L * math.sin(ang)
            d.line([(x, y), (ex, ey)], fill=(66, 94, 40, 255), width=SS)
            nb = max(4, int(L / (4 * SS)))
            for k in range(nb):
                s = (k + 0.5) / nb
                bx, by = x + (ex - x) * s, y + (ey - y) * s
                l2 = C * 0.03 * (1.0 - 0.6 * s) * rnd.uniform(0.8, 1.2)
                for q in range(3):
                    a = rnd.uniform(0, 2 * math.pi)
                    d.line([(bx, by), (bx + math.cos(a) * l2, by + math.sin(a) * l2 * 0.6)], fill=green(rnd, 1.05), width=SS + 1)


def leaf_poly(d, x, y, ln, wd, ang, col, rib):
    """An elliptic leaf with a drawn-out tip from (x, y) along ang."""
    ca, sa = math.cos(ang), math.sin(ang)
    pts = []
    n = 16
    for i in range(n + 1):
        t = i / n
        w = wd * math.sin(math.pi * t ** 0.8) ** 0.9
        pts.append((t, w))
    poly = [(x + ca * t * ln - sa * w, y + sa * t * ln + ca * w) for t, w in pts]
    poly += [(x + ca * t * ln + sa * w, y + sa * t * ln - ca * w) for t, w in reversed(pts)]
    d.polygon(poly, fill=col)
    d.line([(x, y), (x + ca * ln * 0.9, y + sa * ln * 0.9)], fill=rib, width=SS)


def benjamina(d, ox, oy, rnd):
    """A weeping-fig twig: a thin brown stem from the bottom centre with alternate glossy leaves angled to the tip."""
    pts = []
    for i in range(31):
        t = i / 30
        pts.append((ox + C * (0.5 + 0.05 * math.sin(3 * t + rnd.random() * 0.2)), oy + C * (0.99 - 0.9 * t)))
    d.line(pts, fill=(96, 80, 56, 255), width=3 * SS)
    for i in range(3, 30):
        t = i / 30
        x, y = pts[i]
        for side in (-1, 1):
            if rnd.random() < 0.35:
                continue
            ln = C * rnd.uniform(0.17, 0.24) * (1.0 - 0.35 * t)
            ang = -math.pi / 2 + side * math.radians(rnd.uniform(35, 75))
            g = rnd.uniform(0.75, 1.15)
            light = rnd.random() < 0.35
            col = (int(min(255, (70 if light else 40) * g)), int(min(255, (112 if light else 82) * g)), int(min(255, (40 if light else 34) * g)), 255)
            rib = (int(col[0] * 0.75), int(col[1] * 0.8), int(col[2] * 0.75), 255)
            leaf_poly(d, x, y, ln, ln * 0.17, ang, col, rib)


def main():
    im = Image.new("RGBA", (N * SS, N * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    for i, (fn, seed) in enumerate(((sprengeri, 1), (setaceus, 3), (benjamina, 5), (benjamina, 6))):
        ox, oy = (i % 2) * C, (i // 2) * C        # top row (image y 0) = UV v 0.5..1
        fn(d, ox, oy, random.Random(seed))
    im = im.resize((N, N), Image.LANCZOS)
    # bleed the colour into the transparent texels so mip-mapping does not darken the edges
    rgb = im.convert("RGB").filter(ImageFilter.MaxFilter(5))
    a = im.getchannel("A")
    out = Image.composite(im.convert("RGB"), rgb, a)
    out.putalpha(a)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    out.save(OUT)
    print("wrote", os.path.normpath(OUT))


if __name__ == "__main__":
    main()
