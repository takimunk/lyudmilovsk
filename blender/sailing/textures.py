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
# Cloth variants (one picture each; the engine picks one per boat with the material instance, see import_sailing.py):
#   new      crisp warm white Dacron
#   old      yellowed old Dacron: creases, a grey tidemark along the foot, mildew specks, a faded emblem
#   patched  old cloth with sticky-back repair patches, one replaced (whiter) panel and a re-stitched seam
VARIANTS = ("new", "old", "patched")
CLOTH_V = {"new": (247, 245, 238), "old": (235, 228, 205), "patched": (239, 233, 214)}
# alpha = how much light the cloth lets through (the engine's subsurface / the preview's translucency)
A_CLOTH = {"new": 150, "old": 118, "patched": 126}
A_SEAM, A_TABLING, A_PATCH, A_INK, A_WINDOW = 92, 70, 52, 40, 245


def _shade(c, k):
    return tuple(max(0, min(255, int(v + k))) for v in c[:3])


def _stitch(d, pts, col, width=1, dash=5):
    """A dashed stitch line along a polyline."""
    acc = 0.0
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        L = math.hypot(x1 - x0, y1 - y0)
        n = max(1, int(L / dash))
        for k in range(n):
            if (int(acc / dash) + k) % 2 == 0:
                t0, t1 = k / n, (k + 0.7) / n
                d.line([(x0 + (x1 - x0) * t0, y0 + (y1 - y0) * t0), (x0 + (x1 - x0) * t1, y0 + (y1 - y0) * t1)], fill=col, width=width)
        acc += L


def _offset(pts, dist):
    """A polyline moved sideways (left of its direction) by dist pixels."""
    out = []
    for i, (x, y) in enumerate(pts):
        a = pts[max(0, i - 1)]
        c = pts[min(len(pts) - 1, i + 1)]
        dx, dy = c[0] - a[0], c[1] - a[1]
        L = math.hypot(dx, dy) or 1.0
        out.append((x - dy / L * dist, y + dx / L * dist))
    return out


def sail_picture(b, which="main", size=None, numbers=None, variant="new", face="port"):
    """RGBA picture of one sail. numbers (previews only): painted on the face's own box readable, and the other face's
    box mirrored and faint (seen through the cloth); face = 'port' or 'stbd' (the starboard face sees the picture
    mirrored, so its own number is painted mirrored)."""
    key = b if which == "main" else b + "_jib"
    marks = B.SAIL_MARKS[key]
    x0, x1, z0, z1 = B.sail_box(b, which)
    W_, H_ = x1 - x0, z1 - z0
    if size is None:
        size = (1024, 2048 if H_ > 3 else 1024)
    w, h = size
    rnd = random.Random(hash((key, variant)) & 0xFFFF)
    base = CLOTH_V[variant]
    img = weave(w, h, base, seed=hash(key) & 255).convert("RGBA")
    alpha = Image.new("L", (w, h), A_CLOTH[variant])
    d = ImageDraw.Draw(img)
    da = ImageDraw.Draw(alpha)
    thick = max(1, w // 512)

    def px(sv, hv):
        p = B.sail_point(b, sv, hv, which)
        u, v = B.sail_uv(b, p, which)
        return (u * w, (1 - v) * h)

    outline = [px(0, k / 40) for k in range(41)] + [px(k / 40, 1) for k in range(41)] + \
              [px(1, k / 40) for k in range(40, -1, -1)] + [px(k / 40, 0) for k in range(40, -1, -1)]
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).polygon(outline, fill=255)

    # ---- old cloth: yellowing toward the foot and the leech, a tidemark, creases, mildew
    if variant in ("old", "patched"):
        grad = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        gd = ImageDraw.Draw(grad)
        for y in range(0, h, 4):
            t = y / h
            gd.rectangle([0, y, w, y + 4], fill=(150, 120, 40, int(10 + 34 * t ** 2)))
        img.alpha_composite(grad)
        tide = [px(k / 30, 0.035 + 0.01 * math.sin(k * 0.9)) for k in range(31)]
        d.line(tide, fill=_shade(base, -26), width=3 * thick)
        for _ in range(9):
            sv, hv = rnd.uniform(0.1, 0.9), rnd.uniform(0.1, 0.9)
            a = rnd.uniform(-0.6, 0.6)
            L = rnd.uniform(0.08, 0.25) * w
            cx, cy = px(sv, hv)
            p0 = (cx - math.cos(a) * L, cy - math.sin(a) * L)
            p1 = (cx + math.cos(a) * L, cy + math.sin(a) * L)
            d.line([p0, p1], fill=_shade(base, -13), width=2 * thick)
            d.line([(p0[0], p0[1] + 2), (p1[0], p1[1] + 2)], fill=_shade(base, 7), width=thick)
        for _ in range(110):
            sv, hv = rnd.uniform(0.0, 1.0), rnd.uniform(0.0, 0.25) ** 1.4
            cx, cy = px(sv, hv)
            r = rnd.uniform(0.5, 1.6) * thick
            d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(118, 124, 104, 255))

    # ---- panels: cross-cut seams square to the leech (the optimist: parallel to the foot), double stitched
    n = marks["panels"]
    seam_col = _shade(base, -22)
    st_col = _shade(base, -62)
    bw = max(5, w // 150)
    lx0, ly0 = px(1.0, 0.0)
    lx1, ly1 = px(1.0, 1.0)
    ldx, ldy = lx1 - lx0, ly1 - ly0
    LL = math.hypot(ldx, ldy)
    perp = (-ldy / LL, ldx / LL)
    if perp[0] > 0:
        perp = (-perp[0], -perp[1])
    seams = []
    for k in range(1, n):
        hv = k / n
        if b == "optimist" and which == "main":
            sx, sy = px(1.0, hv * 0.92)
            dirv = (-1.0, 0.0)
        else:
            sx, sy = px(1.0, hv)
            dirv = perp
        seams.append([(sx + dirv[0] * t * w * 1.6, sy + dirv[1] * t * w * 1.6) for t in (0.0, 0.5, 1.0)])
    replaced = n // 2 if variant == "patched" else None
    layer = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    la = Image.new("L", (w, h), 0)
    lad = ImageDraw.Draw(la)
    if replaced is not None and 0 < replaced < len(seams):
        a_, c_ = seams[replaced - 1], seams[replaced]
        ld.polygon(a_ + c_[::-1], fill=CLOTH_V["new"] + (255,))
    for k, sp in enumerate(seams):
        ld.line(sp, fill=seam_col + (255,), width=bw)
        lad.line(sp, fill=A_SEAM, width=bw)
        restitch = variant == "patched" and k == 1
        for side in (-1, 1):
            o = _offset(sp, side * (bw / 2 - 1))
            if restitch:
                zz = []
                for i in range(60):
                    t = i / 59
                    x = o[0][0] + (o[-1][0] - o[0][0]) * t
                    y = o[0][1] + (o[-1][1] - o[0][1]) * t + (3 if i % 2 else -3) * thick
                    zz.append((x, y))
                ld.line(zz, fill=(70, 70, 90, 255), width=thick)
            else:
                _stitch(ld, o, st_col + (255,), width=thick, dash=4 * thick)
    img.paste(layer, (0, 0), Image.composite(layer.getchannel("A"), Image.new("L", (w, h), 0), mask))
    alpha.paste(la, (0, 0), Image.composite(la, Image.new("L", (w, h), 0), mask))

    # ---- tablings: leech, foot (and luff where there is no sleeve), a little different in tone, stitched
    tab_col = _shade(base, -9)

    def band(edge, width):
        if edge == "leech":
            a_ = [px(1.0, k / 40) for k in range(41)]
        elif edge == "foot":
            a_ = [px(k / 40, 0.0) for k in range(41)]
        else:
            a_ = [px(0.0, k / 40) for k in range(41)]
        cen = px(0.5, 0.5)
        o1 = _offset(a_, width)
        o2 = _offset(a_, -width)
        dist = lambda pts: sum(math.hypot(p[0] - cen[0], p[1] - cen[1]) for p in pts)
        inner = o1 if dist(o1) < dist(o2) else o2
        d.polygon(a_ + inner[::-1], fill=tab_col)
        da.polygon(a_ + inner[::-1], fill=A_TABLING)
        _stitch(d, _offset(inner, 0) if True else inner, st_col, width=thick, dash=4 * thick)
        mid = [((p[0] + q[0]) / 2, (p[1] + q[1]) / 2) for p, q in zip(a_, inner)]
        _stitch(d, mid, st_col, width=thick, dash=4 * thick)
    band("leech", 0.014 * w)
    band("foot", 0.012 * w)
    if not (b == "luch" and which == "main"):
        band("luff", 0.016 * w)

    # ---- luff: the luch's sleeve, the optimist's lacing eyes
    if b == "luch" and which == "main":
        pts = [px(0, hv / 40) for hv in range(41)] + [px(0.045, hv / 40) for hv in range(40, -1, -1)]
        d.polygon(pts, fill=_shade(base, -12))
        da.polygon(pts, fill=A_TABLING)
        _stitch(d, [px(0.045, hv / 40) for hv in range(41)], st_col, width=2 * thick, dash=5 * thick)
        _stitch(d, [px(0.04, hv / 40) for hv in range(41)], st_col, width=thick, dash=5 * thick)
    if b == "optimist":
        for k in range(1, 12):
            x, y = px(0.012, k / 12)
            d.ellipse([x - 7, y - 7, x + 7, y + 7], fill=(178, 178, 172), outline=(100, 100, 100), width=2)
            da.ellipse([x - 7, y - 7, x + 7, y + 7], fill=0)
        for k in range(1, 9):
            x, y = px(k / 9, 0.008)
            d.ellipse([x - 6, y - 6, x + 6, y + 6], fill=(178, 178, 172), outline=(100, 100, 100), width=2)

    # ---- corner patches: layered fans with stitched outlines (tack, clew, head; the optimist's peak and throat)
    corners = [(0.0, 0.0, 0.20, 0.09, 1, 1), (1.0, 0.0, 0.24, 0.11, -1, 1), (0.0, 1.0, 0.55, 0.07, 1, -1)]
    if b == "optimist" and which == "main":
        corners = [(0.0, 0.0, 0.16, 0.10, 1, 1), (1.0, 0.0, 0.20, 0.12, -1, 1), (0.0, 1.0, 0.18, 0.12, 1, -1),
                   (1.0, 1.0, 0.24, 0.16, -1, -1)]
    if which == "jib":
        corners = [(0.0, 0.0, 0.22, 0.10, 1, 1), (1.0, 0.0, 0.25, 0.14, -1, 1), (0.0, 1.0, 1.0, 0.09, 1, -1)]
    for (sc, hc, rs, rh, ss, hs) in corners:
        for lay in range(3):
            f = 1.0 - lay * 0.28
            pts = [px(sc, hc)]
            for i in range(13):
                a = i / 12
                sv = sc + ss * rs * f * (1 - a) ** 1.2
                hv = hc + hs * rh * f * a ** 1.2
                pts.append(px(min(1, max(0, sv)), min(1, max(0, hv))))
            d.polygon(pts, fill=_shade(PATCH if variant == "new" else base, -5 * (lay + 1)))
            da.polygon(pts, fill=max(10, A_PATCH - 12 * lay))
            _stitch(d, pts[1:], st_col, width=thick, dash=4 * thick)
        # the ring / cringle in the corner
        cx, cy = px(min(0.985, max(0.015, sc + ss * 0.02)), min(0.985, max(0.015, hc + hs * 0.012)))
        d.ellipse([cx - 9, cy - 9, cx + 9, cy + 9], fill=(190, 192, 194), outline=(90, 90, 92), width=3)
    if b == "optimist" and which == "main":
        # the sprit's peak: the sprit's eye strap running into the sail along the sprit's line
        a_, c_ = px(0.0, 0.32), px(1.0, 1.0)
        for t0 in (0.80,):
            p0 = (a_[0] + (c_[0] - a_[0]) * t0, a_[1] + (c_[1] - a_[1]) * t0)
            d.line([p0, (c_[0] - 6, c_[1] + 6)], fill=_shade(base, -20), width=10 * thick)

    # ---- batten pockets: a stitched pocket with the elastic end and the batten's shadow
    for hb_ in marks["battens"]:
        a_ = px(1.0 - marks["batten_len"], hb_)
        c_ = px(0.99, hb_)
        wid = max(9, w // 64)
        d.line([a_, c_], fill=_shade(base, -14), width=wid)
        da.line([a_, c_], fill=A_PATCH, width=wid)
        d.line([a_, c_], fill=_shade(base, -30), width=max(2, wid // 4))
        for side in (-1, 1):
            _stitch(d, _offset([a_, c_], side * wid / 2), st_col, width=thick, dash=4 * thick)
        e_ = (c_[0] + (a_[0] - c_[0]) * 0.18, c_[1] + (a_[1] - c_[1]) * 0.18)
        d.line([c_, e_], fill=_shade(base, -24), width=wid + 2)

    # ---- window (luch main; the cadet's jib): clear film with a stitched border
    win = None
    if which == "main":
        win = B.BOATS[b]["main"].get("window")
        if win:
            win = (win[0], win[1], win[2] * 0.4, win[3] * 0.4)
    elif b == "cadet":
        win = (0.30, 0.62, 0.10, 0.24)
    if win:
        s0, s1, h0, h1 = win
        q = [px(s0, h0), px(s1, h0), px(s1, h1), px(s0, h1)]
        d.polygon(q, fill=(160, 172, 176), outline=_shade(base, -40))
        da.polygon(q, fill=A_WINDOW)
        _stitch(d, q + [q[0]], st_col, width=2 * thick, dash=4 * thick)

    # ---- tell-tales (red to port, green to starboard: both seen), the class emblem
    for hv in (0.33, 0.55, 0.75):
        x, y = px(0.18, hv)
        d.line([(x, y), (x + w * 0.035, y + 6)], fill=RED if hv != 0.55 else (40, 120, 60), width=4)
    if marks.get("emblem"):
        sv, hv, sz = marks["emblem"]
        cx, cy = px(sv, hv)
        r = sz * h
        em = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        EMBLEMS[b](ImageDraw.Draw(em), cx, cy, r)
        if variant != "new":
            em.putalpha(em.getchannel("A").point(lambda v: int(v * 0.72)))
        img.alpha_composite(em)
        da.bitmap((0, 0), em.getchannel("A").point(lambda v: 255 if v > 60 else 0), fill=A_INK)

    # ---- repairs (patched): sticky-back patches with sharp edges, in a fresher tone
    if variant == "patched":
        for (sv, hv, pw, ph, ang) in ((0.62, 0.30, 0.10, 0.05, 0.25), (0.35, 0.68, 0.07, 0.07, -0.4), (0.80, 0.12, 0.06, 0.04, 0.1)):
            cx, cy = px(sv, hv)
            hw_, hh_ = pw * w, ph * h * (1024 / h)
            ca, sa = math.cos(ang), math.sin(ang)
            q = [(cx + ca * x_ - sa * y_, cy + sa * x_ + ca * y_) for x_, y_ in ((-hw_, -hh_), (hw_, -hh_), (hw_, hh_), (-hw_, hh_))]
            d.polygon(q, fill=_shade(CLOTH_V["new"], 2), outline=_shade(base, -34))
            da.polygon(q, fill=A_PATCH)

    # ---- numbers (previews; the engine draws them from T_Sail_Digits)
    if numbers is not None:
        f = font(int(marks["num_h"] * h))
        for side in ("num_stbd", "num_port"):
            own = (side == "num_port") == (face == "port")
            sv, hv = marks[side]
            cx, cy = px(sv, hv)
            t = Image.new("RGBA", (w, h), (0, 0, 0, 0))
            ImageDraw.Draw(t).text((cx, cy), str(numbers), fill=(26, 28, 34, 255 if own else 70), font=f, anchor="mm")
            if face == "stbd":
                # mirror the number in place: the starboard face sees the picture mirrored
                bb = t.getbbox()
                if bb:
                    crop = t.crop(bb).transpose(Image.FLIP_LEFT_RIGHT)
                    t = Image.new("RGBA", (w, h), (0, 0, 0, 0))
                    t.paste(crop, bb[:2])
            img.alpha_composite(t)
    img = img.filter(ImageFilter.SMOOTH)
    # outside the sail outline: plain cloth (never seen) and fully opaque
    out = Image.composite(img, Image.new("RGBA", (w, h), base + (255,)), mask)
    out.putalpha(Image.composite(alpha, Image.new("L", (w, h), A_CLOTH[variant]), mask))
    return out


def save_sails():
    nums = {"luch": 147, "optimist": 23, "cadet": 58}
    for b in B.BOATS:
        jobs = [("main", b)] + ([("jib", "cadet_jib")] if b == "cadet" else [])
        for which, stem in jobs:
            for var in VARIANTS:
                suf = "" if var == "new" else "_" + var
                sail_picture(b, which, variant=var).save(os.path.join(OUT, "sail_%s%s.png" % (stem, suf)))
                if which == "main":
                    for face, fs in (("port", ""), ("stbd", "_stbd")):
                        sail_picture(b, which, numbers=nums[b], variant=var, face=face).save(
                            os.path.join(OUT, "sail_%s%s_demo%s.png" % (stem, suf, fs)))


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
    "timetable": (0, 1536, 512, 512),
    "drawing0": (512, 1536, 256, 256), "drawing1": (768, 1536, 256, 256),
    "drawing2": (512, 1792, 256, 256), "drawing3": (768, 1792, 256, 256),
    "bay1": (1024, 1536, 128, 128), "bay2": (1152, 1536, 128, 128), "bay3": (1280, 1536, 128, 128),
    "bay4": (1408, 1536, 128, 128), "bay5": (1536, 1536, 128, 128), "bay6": (1664, 1536, 128, 128),
}


def timetable(img):
    """The painted timetable board by the door: a week grid (seven day columns as dots, the weekend in red), a row per
    class (the class emblems) with clock faces at the lesson times and boat marks on the days it sails. No words."""
    x, y, w, h = CELLS["timetable"]
    pic = Image.new("RGB", (w, h), (36, 74, 112))
    d = ImageDraw.Draw(pic)
    d.rectangle([10, 10, w - 10, h - 10], outline=(236, 232, 214), width=5)
    cols = 7
    x0, cw = 140, (w - 160) / cols
    for k in range(cols):
        cx = x0 + cw * (k + 0.5)
        d.ellipse([cx - 13, 40, cx + 13, 66], fill=(200, 60, 50) if k >= 5 else (236, 232, 214))
    rows = [("optimist", (10, 13)), ("luch", (16,)), ("cadet", (12, 17))]
    for r, (cls, hours) in enumerate(rows):
        cy = 130 + r * 125
        d.line([(24, cy - 58), (w - 24, cy - 58)], fill=(150, 170, 190), width=2)
        if cls == "luch":
            emblem_luch(d, 70, cy + 6, 38, col=(236, 120, 90))
        elif cls == "optimist":
            emblem_opti(d, 70, cy, 34)
        else:
            emblem_cadet(d, 70, cy, 36)
        rr = random.Random(r + 7)
        for k in range(cols):
            if rr.random() < 0.55 or k in (5, 6):
                cx = x0 + cw * (k + 0.5)
                hh = hours[k % len(hours)]
                d.ellipse([cx - 22, cy - 22, cx + 22, cy + 22], outline=(236, 232, 214), width=3)
                a = math.radians(hh % 12 * 30)
                d.line([(cx, cy), (cx + math.sin(a) * 12, cy - math.cos(a) * 12)], fill=(236, 232, 214), width=4)
                d.line([(cx, cy), (cx, cy - 18)], fill=(236, 232, 214), width=2)
    d.line([(24, 500 - 120), (w - 24, 500 - 120)], fill=(150, 170, 190), width=2)
    # the flag that means "sailing today" and the one for "lesson ashore", as a key
    flag_picture(d, 60, 405, 90, 60, "sail_on")
    boat_icon(d, 200, 435, 0.0, 30, (236, 232, 214))
    flag_picture(d, 290, 405, 90, 60, "shore")
    d.rectangle([420, 410, 470, 460], outline=(236, 232, 214), width=4)
    img.paste(noise(pic, 5, 11), (x, y))


def drawings(img):
    """Children's drawings of boats pinned in the classroom: crayon on paper (no words)."""
    for k in range(4):
        x, y, w, h = CELLS["drawing%d" % k]
        r = random.Random(30 + k)
        pic = Image.new("RGB", (w, h), (246, 242, 230))
        d = ImageDraw.Draw(pic)
        sea = [(40, 110, 200), (30, 140, 170), (60, 90, 180), (20, 120, 160)][k]
        for j in range(5):
            yy = 170 + j * 14
            d.line([(i * 16, yy + (6 if i % 2 else 0)) for i in range(18)], fill=sea, width=5)
        d.ellipse([w - 80, 18, w - 22, 76], fill=(250, 200, 40))
        for j in range(8):
            a = j * math.pi / 4
            d.line([(w - 51 + math.cos(a) * 34, 47 + math.sin(a) * 34), (w - 51 + math.cos(a) * 48, 47 + math.sin(a) * 48)], fill=(250, 180, 30), width=4)
        hull_c = [(220, 60, 50), (250, 200, 40), (90, 160, 220), (240, 120, 40)][k]
        bx, by = 60 + r.randint(0, 40), 150
        d.polygon([(bx, by), (bx + 120, by), (bx + 100, by + 26), (bx + 18, by + 26)], fill=hull_c, outline=(40, 40, 40))
        d.line([(bx + 60, by), (bx + 60, by - 110)], fill=(60, 60, 60), width=4)
        d.polygon([(bx + 63, by - 105), (bx + 63, by - 6), (bx + 120, by - 8)], fill=(255, 255, 255), outline=(60, 60, 60))
        if k % 2:
            d.polygon([(bx + 57, by - 90), (bx + 57, by - 8), (bx + 10, by - 8)], fill=(255, 255, 255), outline=(60, 60, 60))
        for j in range(3):
            gx, gy = r.randint(10, w - 30), r.randint(20, 90)
            d.arc([gx, gy, gx + 18, gy + 10], 200, 340, fill=(60, 60, 60), width=3)
            d.arc([gx + 16, gy, gx + 34, gy + 10], 200, 340, fill=(60, 60, 60), width=3)
        img.paste(noise(pic, 7, 40 + k), (x, y))


def bay_numbers(img):
    """Numbers painted on the boat park's slab at each bay (white road paint, a little worn)."""
    for k in range(1, 7):
        x, y, w, h = CELLS["bay%d" % k]
        pic = Image.new("RGBA", (w, h), (120, 118, 112, 255))
        d = ImageDraw.Draw(pic)
        d.text((w / 2, h / 2), str(k), fill=(236, 234, 226, 255), font=font(108), anchor="mm")
        r = random.Random(k)
        for _ in range(260):
            px_, py_ = r.randrange(w), r.randrange(h)
            d.point((px_, py_), fill=(120, 118, 112, 255))
        img.paste(pic.convert("RGB"), (x, y))


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
    timetable(img)
    drawings(img)
    bay_numbers(img)
    x, y, w, h = CELLS["paper"]
    ImageDraw.Draw(img).rectangle([x, y, x + w, y + h], fill=(238, 234, 222))
    return img


def main():
    os.makedirs(OUT, exist_ok=True)
    save_sails()
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
