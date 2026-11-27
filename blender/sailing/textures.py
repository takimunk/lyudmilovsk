#!/usr/bin/env python3
"""Procedural (PIL, no paid generation, no words) pictures of the sailing school -> blender/sailing/out/textures/:

  sail_<class>.png        the class's sail: cloth weave, cross-cut panel seams, corner patches, batten pockets, the luff
                          sleeve (luch) or lacing eyes (optimist), the window (luch), the class emblem (original designs:
                          «Луч» = a red sun with rays; Optimist = a blue wave in a circle; Cadet = a blue pennant star).
                          No numbers: the material draws them (T_Sail_Digits, custom primitive data 2).
  sail_<class>_demo.png   the same with the numbers painted in (Blender previews only).
  sail_digits.png         0..9 in a row (white on transparent; alpha = ink): the engine's sail numbers.
  sail_atlas.png          pictures of the boathouse (cells in atlas_cells.json): the blackboard's points-of-sail
                          diagram, the flag chart, the knot board, photos, the clock / barometer / anemometer / radio faces,
                          the gable mosaic emblem, the pennants, the boat-park plate.
  sail_flags.png          the signal mast's flags (ids in atlas_cells.json "flags").

  python3 blender/sailing/textures.py           (build_all.py runs it first)
"""
import json
import math
import os
import random
import sys

from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import boatspec as B  # noqa: E402

OUT = os.path.join(HERE, "out", "textures")
CELLS_JSON = os.path.join(HERE, "atlas_cells.json")

CLOTH = (244, 243, 236)
SEAM = (214, 211, 200)
PATCH = (232, 229, 218)
RED = (196, 38, 32)
BLUE = (34, 78, 150)
NAVY = (24, 36, 70)
YELLOW = (240, 190, 40)
GREEN = (40, 128, 64)
FONT_DIRS = ["/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
             "/System/Library/Fonts/Supplemental/Arial Bold.ttf", "/Library/Fonts/Arial Bold.ttf"]


def font(size):
    from PIL import ImageFont
    for p in FONT_DIRS:
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


def noise(img, amp=6, seed=1):
    r = random.Random(seed)
    px = img.load()
    w, h = img.size
    for _ in range(w * h // 3):
        x, y = r.randrange(w), r.randrange(h)
        c = px[x, y]
        d = r.randint(-amp, amp)
        px[x, y] = tuple(max(0, min(255, v + d)) for v in c[:3]) + tuple(c[3:])
    return img


def weave(w, h, base, seed=3):
    img = Image.new("RGB", (w, h), base)
    d = ImageDraw.Draw(img)
    for y in range(0, h, 3):
        d.line([(0, y), (w, y)], fill=tuple(v - 4 for v in base))
    for x in range(0, w, 3):
        d.line([(x, 0), (x, h)], fill=tuple(v - 3 for v in base))
    return noise(img, 5, seed)


# ---------------------------------------------------------------------------------------------- emblems
def emblem_luch(d, cx, cy, r, col=RED):
    """«Луч»: a sun half-risen over two wave lines, with long rays: an original sign of the class."""
    for k in range(9):
        a = math.pi * (k + 0.5) / 9
        ca, sa = math.cos(a), math.sin(a)
        w = 0.07
        p = [(cx + math.cos(a - w) * r * 0.62, cy - math.sin(a - w) * r * 0.62),
             (cx + ca * r * 1.0, cy - sa * r * 1.0),
             (cx + math.cos(a + w) * r * 0.62, cy - math.sin(a + w) * r * 0.62)]
        d.polygon(p, fill=col)
    d.pieslice([cx - r * 0.5, cy - r * 0.5, cx + r * 0.5, cy + r * 0.5], 180, 360, fill=col)
    for k, yy in enumerate((0.12, 0.30)):
        pts = []
        for i in range(41):
            t = i / 40
            x = cx - r + 2 * r * t
            pts.append((x, cy + r * yy + math.sin(t * math.pi * 4) * r * 0.05))
        d.line(pts, fill=col, width=max(2, int(r * 0.09)))


def emblem_opti(d, cx, cy, r):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=BLUE, width=max(3, int(r * 0.12)))
    for k in range(2):
        pts = [(cx - r * 0.7 + 1.4 * r * i / 30, cy + r * (0.05 + 0.3 * k) + math.sin(i / 30 * math.pi * 2) * r * 0.12) for i in range(31)]
        d.line(pts, fill=BLUE, width=max(3, int(r * 0.12)))
    d.polygon([(cx - r * 0.1, cy - r * 0.7), (cx - r * 0.1, cy - r * 0.05), (cx - r * 0.55, cy - r * 0.05)], fill=BLUE)
    d.polygon([(cx + r * 0.0, cy - r * 0.6), (cx + r * 0.0, cy - r * 0.05), (cx + r * 0.35, cy - r * 0.05)], fill=BLUE)


def emblem_cadet(d, cx, cy, r):
    pts = []
    for k in range(10):
        a = -math.pi / 2 + k * math.pi / 5
        rr = r if k % 2 == 0 else r * 0.42
        pts.append((cx + math.cos(a) * rr, cy + math.sin(a) * rr))
    d.polygon(pts, fill=BLUE)
    d.ellipse([cx - r * 0.18, cy - r * 0.18, cx + r * 0.18, cy + r * 0.18], fill=CLOTH)


EMBLEMS = {"luch": emblem_luch, "optimist": emblem_opti, "cadet": emblem_cadet}


# ---------------------------------------------------------------------------------------------- sails
def sail_picture(b, which="main", size=None, numbers=None):
    key = b if which == "main" else b + "_jib"
    marks = B.SAIL_MARKS[key]
    x0, x1, z0, z1 = B.sail_box(b, which)
    W_, H_ = x1 - x0, z1 - z0
    if size is None:
        h = 2048 if H_ > 3 else 1024
        w = 1024
        size = (w, h)
    w, h = size
    img = weave(w, h, CLOTH, seed=hash(key) & 255)
    d = ImageDraw.Draw(img)

    def px(sv, hv):
        p = B.sail_point(b, sv, hv, which)
        u, v = B.sail_uv(b, p, which)
        return (u * w, (1 - v) * h)

    # outside the sail: neutral cloth (never seen); inside: seams across, perpendicular-ish to the leech
    n = marks["panels"]
    for k in range(1, n):
        hv = k / n
        pts = [px(sv / 20, hv * (1 - 0.25 * sv / 20) + 0.25 * hv * sv / 20 - 0.03 * (sv / 20) * (1 - hv)) for sv in range(21)]
        d.line(pts, fill=SEAM, width=max(2, w // 300))
        d.line([(x, y + 4) for x, y in pts], fill=(228, 226, 216), width=1)
    # corner patches (tack, clew, head) as layered fans
    for (sc, hc, rs, rh) in ((0.0, 0.0, 0.22, 0.10), (1.0, 0.0, 0.25, 0.12), (0.0, 1.0, 1.0, 0.10)):
        for lay in range(3):
            f = 1.0 - lay * 0.3
            pts = [px(sc, hc)]
            for i in range(9):
                a = i / 8
                sv = sc + (rs * f) * (1 - a) * (1 if sc == 0 else -1)
                hv = hc + (rh * f) * a * (1 if hc == 0 else -1)
                pts.append(px(min(1, max(0, sv)), min(1, max(0, hv))))
            d.polygon(pts, fill=tuple(v - 6 * (lay + 1) for v in PATCH))
    # luff: sleeve (luch) or a tabling with lacing eyes
    if b == "luch" and which == "main":
        pts = [px(0, hv / 40) for hv in range(41)] + [px(0.045, hv / 40) for hv in range(40, -1, -1)]
        d.polygon(pts, fill=(226, 224, 214))
        d.line([px(0.045, hv / 40) for hv in range(41)], fill=SEAM, width=3)
    else:
        d.line([px(0.02, hv / 40) for hv in range(41)], fill=SEAM, width=3)
        if b == "optimist":
            for k in range(1, 12):
                x, y = px(0.012, k / 12)
                d.ellipse([x - 6, y - 6, x + 6, y + 6], fill=(170, 170, 165), outline=(110, 110, 110), width=2)
    # leech tabling and batten pockets
    d.line([px(0.985, hv / 40) for hv in range(41)], fill=SEAM, width=4)
    for hb in marks["battens"]:
        a = px(1.0 - marks["batten_len"], hb)
        c = px(0.995, hb)
        d.line([a, c], fill=(205, 203, 192), width=max(8, w // 70))
        d.line([a, c], fill=(222, 220, 210), width=max(4, w // 160))
    # window (luch): a clear panel low on the sail (reads as a greyish see-through rectangle)
    win = B.BOATS[b].get(which, {}).get("window") if which == "main" else None
    if win:
        s0, s1, h0, h1 = win[0], win[1], win[2] * 0.4, win[3] * 0.4
        q = [px(s0, h0), px(s1, h0), px(s1, h1), px(s0, h1)]
        d.polygon(q, fill=(176, 186, 190), outline=(150, 150, 146))
    # tell-tales
    for hv in (0.33, 0.55, 0.75):
        x, y = px(0.18, hv)
        d.line([(x, y), (x + w * 0.035, y + 6)], fill=RED if hv != 0.55 else (40, 120, 60), width=4)
    # emblem
    if marks.get("emblem"):
        sv, hv, sz = marks["emblem"]
        cx, cy = px(sv, hv)
        r = sz * h
        EMBLEMS[b](d, cx, cy, r) if b != "luch" else emblem_luch(d, cx, cy, r)
    if numbers is not None:
        f = font(int(marks["num_h"] * h))
        for side in ("num_stbd", "num_port"):
            sv, hv = marks[side]
            cx, cy = px(sv, hv)
            d.text((cx, cy), str(numbers), fill=(30, 30, 34), font=f, anchor="mm")
    return img.filter(ImageFilter.SMOOTH)


def digits():
    w, h = 1280, 192
    img = Image.new("RGBA", (w, h), (255, 255, 255, 0))
    d = ImageDraw.Draw(img)
    f = font(176)
    for k in range(10):
        d.text((k * 128 + 64, h / 2), str(k), fill=(255, 255, 255, 255), font=f, anchor="mm")
    return img


# ---------------------------------------------------------------------------------------------- atlas
CELLS = {      # name: (x, y, w, h) in a 2048 x 2048 picture (pixels, y down)
    "blackboard": (0, 0, 1024, 512),
    "flagchart": (1024, 0, 512, 512),
    "knotboard": (1536, 0, 512, 512),
    "photo0": (0, 512, 256, 192), "photo1": (256, 512, 256, 192), "photo2": (0, 704, 256, 192), "photo3": (256, 704, 256, 192),
    "clock": (512, 512, 256, 256), "barometer": (768, 512, 256, 256),
    "anemometer": (512, 768, 256, 128), "radio": (768, 768, 256, 128),
    "mosaic": (1024, 512, 512, 512),
    "pennant0": (1536, 512, 256, 256), "pennant1": (1792, 512, 256, 256),
    "pennant2": (1536, 768, 256, 256), "pennant3": (1792, 768, 256, 256),
    "toolboard": (0, 1024, 512, 512),
    "parkplate": (512, 1024, 512, 256),
    "lifejacket": (512, 1280, 256, 256),
    "chart": (1024, 1024, 1024, 512),
    "paper": (768, 1280, 256, 256),
}
FLAGS = ["sail_on", "shore", "races", "storm", "closed", "class_optimist", "class_luch", "class_cadet", "prep", "recall"]


def chalk_line(d, pts, width=5, col=(236, 236, 228)):
    d.line(pts, fill=col, width=width, joint="curve")


def boat_icon(d, cx, cy, ang, size, col, sail_side=1, sail_ang=0.4):
    """A dinghy seen from above pointing at ang (radians, 0 = up the picture), with its sail as a line."""
    ca, sa = math.cos(ang), math.sin(ang)

    def P(x, y):
        return (cx + x * ca - y * sa, cy + x * sa + y * ca)
    hull = [P(0, -size), P(size * 0.35, -size * 0.2), P(size * 0.3, size * 0.7), P(-size * 0.3, size * 0.7), P(-size * 0.35, -size * 0.2)]
    d.line(hull + [hull[0]], fill=col, width=4)
    m = P(0, -size * 0.35)
    tip = P(sail_side * math.sin(sail_ang) * size * 0.9, -size * 0.35 + math.cos(sail_ang) * size * 0.9)
    d.line([m, tip], fill=col, width=5)


def blackboard(img, x, y, w, h):
    d = ImageDraw.Draw(img)
    d.rectangle([x, y, x + w, y + h], fill=(34, 52, 44))
    r = random.Random(5)
    for _ in range(1600):
        px_, py_ = x + r.random() * w, y + r.random() * h
        d.point((px_, py_), fill=(60, 76, 68))
    cx, cy = x + w * 0.5, y + h * 0.56
    R = h * 0.36
    # wind arrow from the top
    chalk_line(d, [(cx, y + 18), (cx, cy - R - 40)], 6)
    d.polygon([(cx - 16, cy - R - 60), (cx + 16, cy - R - 60), (cx, cy - R - 28)], fill=(236, 236, 228))
    for k in (-1, 1):
        chalk_line(d, [(cx + k * 30, y + 26), (cx + k * 30, cy - R - 70)], 3)
    # no-go sector (shaded) +-45 deg
    for a in range(-44, 45, 4):
        t = math.radians(a)
        chalk_line(d, [(cx, cy), (cx + math.sin(t) * R * 0.9, cy - math.cos(t) * R * 0.9)], 1, (120, 140, 130))
    d.arc([cx - R, cy - R, cx + R, cy + R], 0, 360, fill=(220, 220, 210), width=3)
    # boats round the circle on the points of sail (close-hauled, close reach, beam reach, broad reach, run)
    for deg, sail in ((50, 0.25), (70, 0.5), (90, 0.8), (130, 1.2), (175, 1.45)):
        for side in (1, -1):
            t = math.radians(deg) * side
            bx_, by_ = cx + math.sin(t) * R, cy - math.cos(t) * R
            boat_icon(d, bx_, by_, t, 26, (240, 240, 230), sail_side=-side, sail_ang=sail)
            chalk_line(d, [(cx + math.sin(t) * R * 0.2, cy - math.cos(t) * R * 0.2), (cx + math.sin(t) * R * 0.78, cy - math.cos(t) * R * 0.78)], 2, (200, 200, 190))
    # a beating zig-zag on the left, a knot on the right (chalk sketch)
    zz = [(x + 60, y + h - 40), (x + 120, y + h - 120), (x + 70, y + h - 200), (x + 130, y + h - 280), (x + 90, y + h - 350)]
    chalk_line(d, zz, 3)
    for i in range(40):
        t = i / 39 * math.pi * 3
        pass
    d.ellipse([x + w - 170, y + h - 210, x + w - 70, y + h - 110], outline=(236, 236, 228), width=4)
    chalk_line(d, [(x + w - 120, y + h - 110), (x + w - 120, y + h - 30)], 4)
    chalk_line(d, [(x + w - 150, y + h - 160), (x + w - 60, y + h - 60)], 4)
    # frame (wood) is geometry; a chalk ledge shadow
    d.rectangle([x, y + h - 6, x + w, y + h], fill=(28, 40, 34))


def flag_picture(d, x, y, w, h, fid):
    """One signal flag filling the box."""
    def R(a, b, c, e, col):
        d.rectangle([x + a * w, y + b * h, x + c * w, y + e * h], fill=col)
    W_ = (246, 246, 240)
    BLK = (24, 24, 26)
    if fid == "sail_on":
        R(0, 0, 1, 1, GREEN)
        R(0, 0.4, 1, 0.6, W_)
    elif fid == "shore":        # yellow and black quarters ("L"-like): a lesson ashore
        R(0, 0, 0.5, 0.5, YELLOW); R(0.5, 0, 1, 0.5, BLK); R(0, 0.5, 0.5, 1, BLK); R(0.5, 0.5, 1, 1, YELLOW)
    elif fid == "races":        # red and white diagonals
        R(0, 0, 1, 1, W_)
        d.polygon([(x, y), (x + w, y), (x, y + h)], fill=RED)
    elif fid == "storm":        # red with a black square
        R(0, 0, 1, 1, RED); R(0.3, 0.3, 0.7, 0.7, BLK)
    elif fid == "closed":       # blue and white chequers ("N"-like: no)
        for i in range(4):
            for j in range(4):
                R(i / 4, j / 4, (i + 1) / 4, (j + 1) / 4, BLUE if (i + j) % 2 == 0 else W_)
    elif fid.startswith("class_"):
        R(0, 0, 1, 1, W_)
        b = fid[6:]
        if b == "luch":
            emblem_luch(d, x + w * 0.5, y + h * 0.62, h * 0.42)
        elif b == "optimist":
            emblem_opti(d, x + w * 0.5, y + h * 0.5, h * 0.36)
        else:
            emblem_cadet(d, x + w * 0.5, y + h * 0.5, h * 0.38)
    elif fid == "prep":         # "P"-like: blue with a white square
        R(0, 0, 1, 1, BLUE); R(0.3, 0.3, 0.7, 0.7, W_)
    elif fid == "recall":       # "X"-like: white with a blue cross
        R(0, 0, 1, 1, W_); R(0.4, 0, 0.6, 1, BLUE); R(0, 0.38, 1, 0.62, BLUE)
    d.rectangle([x, y, x + w - 1, y + h - 1], outline=(200, 200, 196), width=2)


def flags_sheet():
    img = Image.new("RGB", (1280, 512), (240, 240, 236))
    d = ImageDraw.Draw(img)
    cells = {}
    for k, fid in enumerate(FLAGS):
        cx, cy = (k % 5) * 256, (k // 5) * 256
        flag_picture(d, cx + 8, cy + 40, 240, 176, fid)
        cells[fid] = [(cx + 8) / 1280, (cy + 40) / 512, 240 / 1280, 176 / 512]
    return noise(img, 4, 9), cells


def knotboard(img, x, y, w, h):
    d = ImageDraw.Draw(img)
    d.rectangle([x, y, x + w, y + h], fill=(120, 80, 46))
    for k in range(0, h, 18):
        d.line([(x, y + k), (x + w, y + k + 6)], fill=(110, 72, 40), width=2)
    d.rectangle([x + 20, y + 20, x + w - 20, y + h - 20], fill=(28, 52, 92))
    rope = (226, 214, 180)
    r = random.Random(2)
    for k in range(9):
        cx = x + 80 + (k % 3) * 176
        cy = y + 90 + (k // 3) * 160
        # a different loop figure per knot
        pts = []
        lobes = 1 + k % 3
        for i in range(80):
            t = i / 79 * 2 * math.pi
            rr = 40 + 14 * math.sin(lobes * t + k)
            pts.append((cx + math.cos(t) * rr * (1 + 0.3 * (k % 2)), cy + math.sin(t) * rr * 0.8))
        d.line(pts, fill=(160, 150, 120), width=14, joint="curve")
        d.line(pts, fill=rope, width=10, joint="curve")
        d.line([(cx - 60, cy + 55), (cx - 20, cy + 30)], fill=rope, width=10)
        d.line([(cx + 60, cy + 55), (cx + 20, cy + 30)], fill=rope, width=10)
        d.rectangle([cx - 40, cy + 62, cx + 40, cy + 76], fill=(232, 222, 190))   # a blank brass label
    return d


def flagchart(img, x, y, w, h):
    d = ImageDraw.Draw(img)
    d.rectangle([x, y, x + w, y + h], fill=(238, 232, 214))
    allf = FLAGS + ["sail_on", "storm"]
    for k in range(12):
        cx, cy = x + 24 + (k % 3) * 160, y + 30 + (k // 3) * 118
        flag_picture(d, cx, cy, 120, 80, allf[k])
        d.line([(cx, cy + 94), (cx + 120, cy + 94)], fill=(150, 140, 120), width=3)


def photos(img):
    r = random.Random(11)
    for k in range(4):
        x, y, w, h = CELLS["photo%d" % k]
        d = ImageDraw.Draw(img)
        sky = (150 + r.randint(-10, 10), 160, 150)
        d.rectangle([x, y, x + w, y + h], fill=(230, 225, 210))
        d.rectangle([x + 12, y + 12, x + w - 12, y + h - 12], fill=sky)
        d.rectangle([x + 12, y + h * 0.6, x + w - 12, y + h - 12], fill=(90, 100, 100))
        for i in range(2 + k):
            bx_ = x + 30 + r.random() * (w - 70)
            by_ = y + h * 0.6
            d.polygon([(bx_, by_), (bx_, by_ - 50 - r.random() * 20), (bx_ + 28, by_)], fill=(232, 230, 220))
            d.line([(bx_ - 6, by_ + 3), (bx_ + 34, by_ + 3)], fill=(50, 50, 50), width=4)
        img.paste(img.crop((x, y, x + w, y + h)).convert("L").convert("RGB"), (x, y))


def dial(img, cell, kind):
    x, y, w, h = CELLS[cell]
    d = ImageDraw.Draw(img)
    if kind in ("clock", "barometer"):
        d.rectangle([x, y, x + w, y + h], fill=(40, 40, 40))
        cx, cy, R = x + w / 2, y + h / 2, w * 0.46
        d.ellipse([cx - R, cy - R, cx + R, cy + R], fill=(240, 236, 224))
        for k in range(60 if kind == "clock" else 40):
            a = 2 * math.pi * k / (60 if kind == "clock" else 40) - (0 if kind == "clock" else math.pi * 0.75)
            if kind == "barometer" and k > 30:
                continue
            ln = 0.14 if k % 5 == 0 else 0.06
            d.line([(cx + math.sin(a) * R * 0.92, cy - math.cos(a) * R * 0.92),
                    (cx + math.sin(a) * R * (0.92 - ln), cy - math.cos(a) * R * (0.92 - ln))], fill=(30, 30, 30), width=4 if k % 5 == 0 else 2)
        if kind == "clock":
            f = font(26)
            for k in range(1, 13):
                a = 2 * math.pi * k / 12
                d.text((cx + math.sin(a) * R * 0.66, cy - math.cos(a) * R * 0.66), str(k), fill=(30, 30, 30), font=f, anchor="mm")
        else:
            d.arc([cx - R * 0.5, cy - R * 0.5, cx + R * 0.5, cy + R * 0.5], 150, 390, fill=(160, 40, 30), width=6)
            d.line([(cx, cy), (cx + R * 0.6, cy - R * 0.4)], fill=(20, 20, 20), width=5)
    elif kind == "anemometer":
        d.rectangle([x, y, x + w, y + h], fill=(60, 64, 60))
        d.rectangle([x + 16, y + 16, x + w - 16, y + h - 16], fill=(222, 220, 196))
        for k in range(21):
            xx = x + 30 + k * (w - 60) / 20
            d.line([(xx, y + h - 30), (xx, y + h - (50 if k % 5 == 0 else 40))], fill=(20, 20, 20), width=3)
        d.line([(x + w * 0.5, y + h - 28), (x + w * 0.35, y + 30)], fill=(170, 30, 20), width=5)
    elif kind == "radio":
        d.rectangle([x, y, x + w, y + h], fill=(70, 60, 50))
        d.rectangle([x + 12, y + 14, x + w * 0.6, y + h - 14], fill=(210, 190, 140))
        for k in range(12):
            xx = x + 22 + k * (w * 0.6 - 30) / 11
            d.line([(xx, y + 30), (xx, y + 50)], fill=(60, 50, 40), width=2)
        d.line([(x + 70, y + 24), (x + 70, y + h - 24)], fill=(180, 30, 20), width=3)
        for k in range(6):
            d.line([(x + w * 0.66, y + 20 + k * 16), (x + w - 14, y + 20 + k * 16)], fill=(40, 34, 28), width=6)


def mosaic(img):
    """The gable mosaic: the «Луч» sun over the sea and a sail, in small tesserae."""
    x, y, w, h = CELLS["mosaic"]
    pic = Image.new("RGB", (w, h), (232, 226, 210))
    d = ImageDraw.Draw(pic)
    d.rectangle([0, h * 0.62, w, h], fill=(40, 96, 150))
    for k in range(5):
        d.line([(0, h * (0.68 + 0.06 * k)), (w, h * (0.66 + 0.06 * k))], fill=(70, 130, 180), width=8)
    emblem_luch(d, w * 0.36, h * 0.60, h * 0.40)
    d.polygon([(w * 0.70, h * 0.66), (w * 0.70, h * 0.18), (w * 0.88, h * 0.66)], fill=(250, 250, 246))
    d.polygon([(w * 0.68, h * 0.66), (w * 0.68, h * 0.30), (w * 0.56, h * 0.66)], fill=(250, 250, 246))
    d.polygon([(w * 0.55, h * 0.68), (w * 0.90, h * 0.68), (w * 0.86, h * 0.72), (w * 0.59, h * 0.72)], fill=(196, 38, 32))
    # tesserae: average per 16 px tile with grout lines
    t = 16
    small = pic.resize((w // t, h // t), Image.BILINEAR)
    r = random.Random(4)
    md = ImageDraw.Draw(pic)
    md.rectangle([0, 0, w, h], fill=(150, 146, 136))
    for i in range(w // t):
        for j in range(h // t):
            c = small.getpixel((i, j))
            dv = r.randint(-12, 12)
            c = tuple(max(0, min(255, v + dv)) for v in c)
            md.rectangle([i * t + 1, j * t + 1, i * t + t - 2, j * t + t - 2], fill=c)
    img.paste(pic, (x, y))


def pennants(img):
    cols = [(150, 30, 30), (30, 60, 130), (30, 110, 60), (200, 150, 30)]
    for k in range(4):
        x, y, w, h = CELLS["pennant%d" % k]
        d = ImageDraw.Draw(img)
        d.rectangle([x, y, x + w, y + h], fill=cols[k])
        d.rectangle([x + 6, y + 6, x + w - 6, y + h - 6], outline=(220, 180, 70), width=6)
        emblem_luch(d, x + w / 2, y + h * 0.48, w * 0.28, col=(230, 190, 80)) if k % 2 == 0 else \
            emblem_cadet(d, x + w / 2, y + h * 0.4, w * 0.22)
        for i in range(8):
            d.line([(x + 10 + i * 30, y + h - 30), (x + 20 + i * 30, y + h - 6)], fill=(220, 180, 70), width=4)


def toolboard(img):
    x, y, w, h = CELLS["toolboard"]
    d = ImageDraw.Draw(img)
    d.rectangle([x, y, x + w, y + h], fill=(196, 176, 140))
    for i in range(0, w, 32):
        for j in range(0, h, 32):
            d.ellipse([x + i + 14, y + j + 14, x + i + 18, y + j + 18], fill=(120, 100, 80))
    # tool silhouettes (painted outlines: where each tool hangs)
    sil = (60, 50, 40)
    d.line([(x + 60, y + 60), (x + 60, y + 300)], fill=sil, width=10)
    d.rectangle([x + 40, y + 40, x + 80, y + 80], outline=sil, width=6)
    d.line([(x + 150, y + 60), (x + 150, y + 260)], fill=sil, width=8)
    d.polygon([(x + 130, y + 260), (x + 170, y + 260), (x + 150, y + 330)], outline=sil)
    d.line([(x + 240, y + 60), (x + 300, y + 300)], fill=sil, width=10)
    d.arc([x + 330, y + 50, x + 470, y + 190], 0, 360, fill=sil, width=8)
    d.line([(x + 100, y + 400), (x + 420, y + 400)], fill=sil, width=14)


def parkplate(img):
    x, y, w, h = CELLS["parkplate"]
    d = ImageDraw.Draw(img)
    d.rectangle([x, y, x + w, y + h], fill=(236, 232, 220))
    d.rectangle([x + 8, y + 8, x + w - 8, y + h - 8], outline=(30, 60, 120), width=8)
    emblem_luch(d, x + w * 0.5, y + h * 0.62, h * 0.34)


def lifejacket(img):
    x, y, w, h = CELLS["lifejacket"]
    d = ImageDraw.Draw(img)
    d.rectangle([x, y, x + w, y + h], fill=(206, 60, 36))
    for k in range(0, h, 32):
        d.line([(x, y + k), (x + w, y + k)], fill=(176, 46, 28), width=4)
    d.rectangle([x, y + h * 0.45, x + w, y + h * 0.55], fill=(220, 220, 214))


def chart(img):
    x, y, w, h = CELLS["chart"]
    d = ImageDraw.Draw(img)
    d.rectangle([x, y, x + w, y + h], fill=(214, 226, 222))
    r = random.Random(8)
    land = [(x, y), (x + w * 0.35, y), (x + w * 0.30, y + h * 0.3), (x + w * 0.38, y + h * 0.55), (x + w * 0.28, y + h), (x, y + h)]
    d.polygon(land, fill=(226, 214, 176))
    for k in range(1, 6):
        pts = [(px_ + k * 22, py_) for px_, py_ in land[1:5]]
        d.line(pts, fill=(150, 180, 190), width=2)
    for _ in range(60):
        d.text((x + w * 0.45 + r.random() * w * 0.5, y + r.random() * h), str(r.randint(2, 30)), fill=(90, 110, 120), font=font(16))
    # a race course: triangle of marks
    pts = [(x + w * 0.55, y + h * 0.8), (x + w * 0.8, y + h * 0.2), (x + w * 0.9, y + h * 0.65)]
    d.line(pts + [pts[0]], fill=(196, 38, 32), width=4)
    for p in pts:
        d.ellipse([p[0] - 10, p[1] - 10, p[0] + 10, p[1] + 10], fill=(240, 90, 26))


def atlas():
    img = Image.new("RGB", (2048, 2048), (128, 128, 128))
    blackboard(img, *CELLS["blackboard"])
    flagchart(img, *CELLS["flagchart"])
    knotboard(img, *CELLS["knotboard"])
    photos(img)
    for c in ("clock", "barometer", "anemometer", "radio"):
        dial(img, c, c)
    mosaic(img)
    pennants(img)
    toolboard(img)
    parkplate(img)
    lifejacket(img)
    chart(img)
    x, y, w, h = CELLS["paper"]
    ImageDraw.Draw(img).rectangle([x, y, x + w, y + h], fill=(238, 234, 222))
    return img


def main():
    os.makedirs(OUT, exist_ok=True)
    for b in B.BOATS:
        sail_picture(b).save(os.path.join(OUT, "sail_%s.png" % b))
        sail_picture(b, numbers={"luch": 147, "optimist": 23, "cadet": 58}[b]).save(os.path.join(OUT, "sail_%s_demo.png" % b))
    sail_picture("cadet", "jib").save(os.path.join(OUT, "sail_cadet_jib.png"))
    digits().save(os.path.join(OUT, "sail_digits.png"))
    atlas().save(os.path.join(OUT, "sail_atlas.png"))
    fl, fcells = flags_sheet()
    fl.save(os.path.join(OUT, "sail_flags.png"))
    cells = {"_doc": "UV cells of the sailing pictures: [u0, v0(top), w, h] as fractions of the picture, y DOWN from the top "
                     "(Blender v = 1 - y). sail_atlas.png 2048x2048; sail_flags.png 1280x512.",
             "atlas": {k: [v[0] / 2048, v[1] / 2048, v[2] / 2048, v[3] / 2048] for k, v in CELLS.items()},
             "flags": fcells, "flag_ids": FLAGS}
    with open(CELLS_JSON, "w") as f:
        f.write(B.compact_json(cells) + "\n")
    print("SAILING textures ->", os.path.relpath(OUT, os.path.dirname(os.path.dirname(HERE))))


if __name__ == "__main__":
    main()
