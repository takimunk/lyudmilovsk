#!/usr/bin/env python3
"""Sleeve fronts and record labels for the craft's eight records (tools/spacecraft/records.json), drawn, not generated.

    python3 tools/spacecraft/sleeves.py            # all three languages + the labels
    python3 tools/spacecraft/sleeves.py --lang ja  # one language (the labels too)

Writes
  textures/spacecraft/T_SC_sleeves_{ru,en,ja}.png   2048x1024: 8 sleeve fronts of 512x512, 4 columns x 2 rows
  textures/spacecraft/T_SC_record_labels.png        1024x512: 8 round centre labels of 256x256, same grid, no words
Cell n of either atlas is the record whose "cell" is n in records.json (column n % 4, row n // 4, row 0 on top).

Style: a restrained mid-century Soviet record sleeve (flat paper colour, one ink, a geometric motif, a type block), not
any real label's mark: the label name is the craft's own «Бортовая фонотека». Every word comes from records.json (ru/en/ja);
type: Dela Gothic One for the title, M PLUS 1p for the rest (both cover Latin, Cyrillic and Japanese; fonts/README.md).
Deterministic: fixed seeds, drawn at 2x and reduced with Lanczos. PIL + numpy only.
"""
import argparse
import json
import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
CATALOG = os.path.join(HERE, "records.json")
FONTS = os.path.join(REPO, "fonts")
OUT = os.path.join(REPO, "textures", "spacecraft")
F_TITLE = os.path.join(FONTS, "DelaGothicOne-Regular.ttf")
F_BOLD = os.path.join(FONTS, "MPLUS1p-Bold.ttf")
F_TEXT = os.path.join(FONTS, "MPLUS1p-Regular.ttf")
SS = 2  # supersampling
CELL = 512 * SS
LCELL = 256 * SS
LANGS = ("ru", "en", "ja")


def hex_rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def mix(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


_font_cache = {}


def font(path, size):
    key = (path, size)
    if key not in _font_cache:
        _font_cache[key] = ImageFont.truetype(path, size)
    return _font_cache[key]


# ---------------------------------------------------------------- type

def split_units(text, lang):
    """Breakable units: words for ru/en, characters (keeping punctuation attached) for ja."""
    if lang != "ja":
        return text.split(" "), " "
    units, closers = [], "、。）」』・）"
    for ch in text:
        if units and (ch in closers or units[-1] in "（「『"):
            units[-1] += ch
        else:
            units.append(ch)
    return units, ""


def wrap(text, lang, f, max_w):
    units, sep = split_units(text, lang)
    lines, cur = [], ""
    for u in units:
        trial = u if not cur else cur + sep + u
        if f.getlength(trial) <= max_w or not cur:
            cur = trial
        else:
            lines.append(cur)
            cur = u
    if cur:
        lines.append(cur)
    return lines


def fit(text, lang, path, max_w, max_lines, size, min_size):
    """Largest size from `size` down at which `text` wraps into at most max_lines lines no wider than max_w."""
    for s in range(size, min_size - 1, -SS):
        f = font(path, s)
        lines = wrap(text, lang, f, max_w)
        if len(lines) <= max_lines and all(f.getlength(l) <= max_w for l in lines):
            return f, lines
    f = font(path, min_size)
    return f, wrap(text, lang, f, max_w)[:max_lines]


def draw_lines(d, lines, f, x, y, fill, lead=1.18, anchor="la", max_w=None):
    asc, desc = f.getmetrics()
    step = int((asc + desc) * lead)
    for i, line in enumerate(lines):
        if anchor == "ma":
            d.text((x, y + i * step), line, font=f, fill=fill, anchor="ma")
        else:
            d.text((x, y + i * step), line, font=f, fill=fill)
    return y + len(lines) * step


# ---------------------------------------------------------------- motifs (drawn in a box, ink on paper)

def motif(d, kind, box, ink, soft, rng):
    x0, y0, x1, y1 = box
    w, h = x1 - x0, y1 - y0
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    lw = max(1, min(3 * SS, int(min(w, h) / 90)))
    if kind == "lattice":  # the aria's ground bass: a quiet woven grid with one bright square
        n = 8
        for i in range(n + 1):
            t = i / n
            d.line([(x0 + t * w, y0), (x0 + t * w, y1)], fill=soft, width=lw)
            d.line([(x0, y0 + t * h), (x1, y0 + t * h)], fill=soft, width=lw)
        for i in range(n):
            for j in range(n):
                if (i * 3 + j * 5) % 7 == 0:
                    d.rectangle([x0 + i * w / n + lw * 2, y0 + j * h / n + lw * 2,
                                 x0 + (i + 1) * w / n - lw * 2, y0 + (j + 1) * h / n - lw * 2], fill=ink)
    elif kind == "moon":  # a moon and its path on the water
        r = w * 0.2
        my = y0 + h * 0.32
        d.ellipse([cx - r, my - r, cx + r, my + r], fill=ink)
        for k in range(9):
            yy = y0 + h * 0.62 + k * h * 0.042
            half = r * (1.1 - k * 0.09) * (1 + 0.15 * math.sin(k * 1.7))
            d.line([(cx - half, yy), (cx + half, yy)], fill=ink, width=lw)
        d.line([(x0, y0 + h * 0.6), (x1, y0 + h * 0.6)], fill=soft, width=lw)
    elif kind == "columns":  # three slow columns, unequal
        for k, (fx, fh) in enumerate([(0.22, 0.72), (0.5, 0.9), (0.78, 0.6)]):
            cw = w * 0.12
            top = y1 - h * fh
            d.rectangle([x0 + fx * w - cw / 2, top, x0 + fx * w + cw / 2, y1], fill=ink if k == 1 else soft)
            d.rectangle([x0 + fx * w - cw * 0.75, top - h * 0.04, x0 + fx * w + cw * 0.75, top], fill=ink)
        d.line([(x0, y1), (x1, y1)], fill=ink, width=lw * 2)
    elif kind == "birch":  # birch trunks with their dashes
        for k, fx in enumerate([0.18, 0.4, 0.66, 0.86]):
            tw = w * (0.07 if k % 2 else 0.055)
            xx = x0 + fx * w
            d.rectangle([xx - tw / 2, y0, xx + tw / 2, y1], fill=ink)
            for m in range(10):
                yy = y0 + (m + 0.3 + rng.random() * 0.4) * h / 10
                ln = tw * (0.4 + rng.random() * 0.5)
                side = -1 if (m + k) % 2 else 1
                d.rectangle([xx + side * tw / 2 - (ln if side > 0 else 0), yy,
                             xx + side * tw / 2 + (0 if side > 0 else ln), yy + h * 0.012], fill=soft)
    elif kind == "waves":  # three-four time on the sea
        for k in range(7):
            yy = y0 + h * (0.18 + k * 0.11)
            pts = []
            for i in range(61):
                t = i / 60
                pts.append((x0 + t * w, yy + math.sin(t * math.pi * 3 + k * 0.9) * h * 0.035))
            d.line(pts, fill=ink if k % 3 == 1 else soft, width=lw * (2 if k % 3 == 1 else 1), joint="curve")
    elif kind == "horn":  # a phonograph horn, flat
        d.polygon([(cx - w * 0.04, cy + h * 0.1), (x1 - w * 0.06, y0 + h * 0.08), (x1 - w * 0.06, y0 + h * 0.52)],
                  fill=ink)
        d.ellipse([x1 - w * 0.14, y0 + h * 0.06, x1 - w * 0.0, y0 + h * 0.54], outline=soft, width=lw * 2)
        d.rectangle([x0 + w * 0.12, y1 - h * 0.24, x0 + w * 0.62, y1 - h * 0.04], fill=ink)
        d.line([(cx - w * 0.04, cy + h * 0.1), (x0 + w * 0.37, y1 - h * 0.24)], fill=ink, width=lw * 2)
        d.ellipse([x0 + w * 0.17, y1 - h * 0.30, x0 + w * 0.57, y1 - h * 0.22], fill=soft)
    elif kind == "lantern":  # a paper lantern and a crescent
        lw2, lh = w * 0.34, h * 0.5
        top = cy - lh / 2 + h * 0.06
        d.line([(cx, y0), (cx, top)], fill=ink, width=lw)
        d.ellipse([cx - lw2 / 2, top, cx + lw2 / 2, top + lh], fill=ink)
        for k in range(1, 5):
            yy = top + lh * k / 5
            half = lw2 / 2 * math.sqrt(max(0.0, 1 - ((yy - top - lh / 2) / (lh / 2)) ** 2))
            d.line([(cx - half, yy), (cx + half, yy)], fill=soft, width=lw)
        d.rectangle([cx - lw2 * 0.2, top - h * 0.03, cx + lw2 * 0.2, top + h * 0.01], fill=ink)
        d.rectangle([cx - lw2 * 0.2, top + lh - h * 0.01, cx + lw2 * 0.2, top + lh + h * 0.03], fill=ink)
        r = w * 0.09
        mx, my = x0 + w * 0.2, y0 + h * 0.2
        d.ellipse([mx - r, my - r, mx + r, my + r], fill=ink)
        d.ellipse([mx - r + r * 0.45, my - r - r * 0.15, mx + r + r * 0.45, my + r - r * 0.15], fill=None)
    elif kind == "clouds":  # flat cloud banks seen from above
        for k in range(5):
            yy = y0 + h * (0.2 + k * 0.17)
            off = (k % 2) * w * 0.12
            for m in range(4):
                rr = h * (0.07 + 0.02 * ((m + k) % 3))
                xx = x0 + off + m * w * 0.26
                d.ellipse([xx - rr * 1.6, yy - rr, xx + rr * 1.6, yy + rr], fill=ink if (k + m) % 4 == 0 else soft)
    else:
        d.ellipse([cx - w * 0.3, cy - w * 0.3, cx + w * 0.3, cy + w * 0.3], outline=ink, width=lw * 2)


# ---------------------------------------------------------------- paper

def paper(size, color, seed, amount=7.0):
    rng = np.random.default_rng(seed)
    n = rng.normal(0, amount, (size[1] // 4 + 1, size[0] // 4 + 1)).astype(np.float32)
    n = np.kron(n, np.ones((4, 4), np.float32))[:size[1], :size[0]]
    fine = rng.normal(0, amount * 0.5, (size[1], size[0])).astype(np.float32)
    base = np.array(color, np.float32)[None, None, :] + (n * 0.6 + fine)[:, :, None]
    return Image.fromarray(np.clip(base, 0, 255).astype(np.uint8), "RGB")


# ---------------------------------------------------------------- one sleeve

def sleeve(cat, rec, lang):
    bg, ink = hex_rgb(rec["sleeve"]["bg"]), hex_rgb(rec["sleeve"]["ink"])
    soft = mix(bg, ink, 0.45)
    img = paper((CELL, CELL), bg, 1000 + rec["cell"])
    d = ImageDraw.Draw(img)
    rng = np.random.default_rng(77 + rec["cell"])
    m = 34 * SS  # margin
    inner = CELL - 2 * m
    t = rec["text"]

    # top rule: the label's name and the record's number
    f_small = font(F_TEXT, 15 * SS)
    d.text((m, m), cat["label_name"][lang], font=f_small, fill=ink)
    d.text((CELL - m, m), "№ %02d · 33⅓" % (rec["cell"] + 1), font=f_small, fill=ink, anchor="ra")
    d.line([(m, m + 26 * SS), (CELL - m, m + 26 * SS)], fill=ink, width=2 * SS)

    # the motif panel
    top = m + 40 * SS
    ph = int(inner * 0.50)
    box = (m, top, CELL - m, top + ph)
    d.rectangle(box, fill=mix(bg, (255, 255, 255), 0.18))
    pad = 18 * SS
    before = img.copy()
    motif(d, rec["sleeve"]["motif"], (box[0] + pad, box[1] + pad, box[2] - pad, box[3] - pad), ink, soft, rng)
    # clip the motif to its panel: whatever spilled past the frame is painted back from before
    clip = Image.new("L", img.size, 255)
    ImageDraw.Draw(clip).rectangle(box, fill=0)
    img.paste(before, (0, 0), clip)
    d = ImageDraw.Draw(img)
    d.rectangle(box, outline=ink, width=2 * SS)

    # the type block
    y = top + ph + 16 * SS
    composer = t["composer"][lang]
    if lang != "ja":
        composer = composer.upper()
    f_c, l_c = fit(composer, lang, F_BOLD, inner, 1, 24 * SS, 14 * SS)
    y = draw_lines(d, l_c, f_c, m, y, ink, lead=1.1)
    bottom = CELL - m
    # performer: at the foot, at most 2 lines
    f_p, l_p = fit(t["performer"][lang], lang, F_TEXT, inner, 2, 17 * SS, 11 * SS)
    asc, desc = f_p.getmetrics()
    p_h = int((asc + desc) * 1.15) * len(l_p)
    title_room = bottom - p_h - 10 * SS - (y + 4 * SS)
    f_t, l_t = fit(t["title"][lang], lang, F_TITLE, inner, 2, 54 * SS, 20 * SS)
    while True:
        a2, d2 = f_t.getmetrics()
        if int((a2 + d2) * 1.08) * len(l_t) <= title_room or f_t.size <= 20 * SS:
            break
        f_t, l_t = fit(t["title"][lang], lang, F_TITLE, inner, 2, f_t.size - SS, 20 * SS)
    draw_lines(d, l_t, f_t, m, y + 4 * SS, ink, lead=1.08)
    draw_lines(d, l_p, f_p, m, bottom - p_h, mix(ink, bg, 0.15), lead=1.15)
    return img, {"composer": f_c.size // SS, "title": f_t.size // SS, "performer": f_p.size // SS,
                 "lines": [len(l_t), len(l_p)]}


# ---------------------------------------------------------------- one label (no words)

def label(rec):
    bg, ink = hex_rgb(rec["sleeve"]["bg"]), hex_rgb(rec["sleeve"]["ink"])
    img = Image.new("RGBA", (LCELL, LCELL), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = LCELL / 2
    r = LCELL / 2 - 4 * SS
    d.ellipse([c - r, c - r, c + r, c + r], fill=ink + (255,))
    d.ellipse([c - r * 0.93, c - r * 0.93, c + r * 0.93, c + r * 0.93], outline=bg + (255,), width=2 * SS)
    # a small motif in the upper half, the number below, the spindle hole in the middle
    rng = np.random.default_rng(500 + rec["cell"])
    soft = mix(ink, bg, 0.5)
    motif(d, rec["sleeve"]["motif"], (c - r * 0.36, c - r * 0.62, c + r * 0.36, c - r * 0.2), bg, soft, rng)
    f = font(F_TITLE, 30 * SS)
    d.text((c, c + r * 0.42), "%02d" % (rec["cell"] + 1), font=f, fill=bg + (255,), anchor="mm")
    f2 = font(F_BOLD, 13 * SS)
    d.text((c, c + r * 0.70), "33⅓", font=f2, fill=bg + (255,), anchor="mm")
    hole = r * 0.075
    d.ellipse([c - hole, c - hole, c + hole, c + hole], fill=(0, 0, 0, 0))
    return img


# ---------------------------------------------------------------- atlases

def cell_xy(n, size):
    return (n % 4) * size, (n // 4) * size


def build(cat, langs):
    os.makedirs(OUT, exist_ok=True)
    recs = sorted(cat["records"], key=lambda r: r["cell"])
    assert [r["cell"] for r in recs] == list(range(8)), "cells must be 0..7"
    report = {}
    for lang in langs:
        atlas = Image.new("RGB", (CELL * 4, CELL * 2))
        for rec in recs:
            img, info = sleeve(cat, rec, lang)
            atlas.paste(img, cell_xy(rec["cell"], CELL))
            report.setdefault(rec["id"], {})[lang] = info
        path = os.path.join(OUT, "T_SC_sleeves_%s.png" % lang)
        atlas.resize((2048, 1024), Image.LANCZOS).save(path, optimize=True)
        print("sleeves: wrote", os.path.relpath(path, REPO))
    atlas = Image.new("RGBA", (LCELL * 4, LCELL * 2), (0, 0, 0, 0))
    for rec in recs:
        atlas.paste(label(rec), cell_xy(rec["cell"], LCELL))
    path = os.path.join(OUT, "T_SC_record_labels.png")
    atlas.resize((1024, 512), Image.LANCZOS).save(path, optimize=True)
    print("sleeves: wrote", os.path.relpath(path, REPO))
    for rid, by in report.items():
        print("  %-32s %s" % (rid, "  ".join("%s t%d/%dl p%d/%dl" % (l, v["title"], v["lines"][0], v["performer"],
                                                                     v["lines"][1]) for l, v in by.items())))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--lang", choices=LANGS, action="append")
    a = ap.parse_args()
    with open(CATALOG, encoding="utf-8") as f:
        cat = json.load(f)
    build(cat, a.lang or LANGS)


if __name__ == "__main__":
    main()
