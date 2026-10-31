"""Lineup sheets from lineup.py renders (plain python3 + PIL, no Blender).

    python3 blender/birds/lineup_sheet.py <out dir> <label>=<render dir> [<label>=<render dir>]

For each render dir: <out>/lineup_<label>.jpg (every species perched at one true scale, rows by size, 10 cm grid) and
<out>/flight_<label>.jpg (gliding, from above, at one true scale). With two dirs also <out>/side_compare.jpg (each species
fitted to its cell, the first dir on the left, the second on the right) and <out>/lineup_compare.jpg (the two lineups
stacked).
"""
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFont

OUT = sys.argv[1]
SETS = [a.split("=", 1) for a in sys.argv[2:]]
os.makedirs(OUT, exist_ok=True)
SPECIES = json.load(open(os.path.join(os.path.dirname(__file__), "..", "..", "unreal", "Lyudmilovsk", "Content", "BirdsData",
                                      "species.json")))["species"]
LEN = {s["id"]: s["lengthCm"] for s in SPECIES}
SPAN = {s["id"]: s["wingspanCm"] for s in SPECIES}
BG = (160, 170, 184)
GRID = (148, 158, 172)
INK = (24, 26, 30)


def font(px):
    for f in ("/System/Library/Fonts/Helvetica.ttc", "/System/Library/Fonts/SFNS.ttf"):
        try:
            return ImageFont.truetype(f, px)
        except OSError:
            pass
    return ImageFont.load_default()


def crop(im):
    bb = im.getchannel("A").point(lambda a: 255 if a > 8 else 0).getbbox()
    return (im.crop(bb), bb) if bb else (im, (0, 0) + im.size)


def load(d, i, view):
    p = os.path.join(d, f"{i}_{view}.png")
    return Image.open(p).convert("RGBA") if os.path.exists(p) else None


def lineup(d, label, view, pxcm, width):
    """Every species at pxcm pixels per cm, sorted by size, wrapped into rows; feet (side) or centres (top) on a line."""
    sc = json.load(open(os.path.join(d, "scale.json")))
    key = LEN if view == "side" else SPAN
    ids = sorted((i for i in sc if view in sc[i]), key=lambda i: key.get(i, 0))
    items = []
    for i in ids:
        im = load(d, i, view)
        if im is None:
            continue
        k = sc[i][view]["mpp"] * 100 * pxcm
        im, _ = crop(im)
        im = im.resize((max(1, round(im.width * k)), max(1, round(im.height * k))), Image.LANCZOS)
        items.append((i, im))
    gap, lab = 24, 34
    rows, row, x = [], [], 0
    for it in items:
        if row and x + it[1].width > width - 2 * gap:
            rows.append(row)
            row, x = [], 0
        row.append(it)
        x += it[1].width + gap
    rows.append(row)
    heights = [max(im.height for _, im in r) + lab + gap for r in rows]
    head = 70
    H = head + sum(heights) + gap
    sheet = Image.new("RGB", (width, H), BG)
    dr = ImageDraw.Draw(sheet)
    step = 10 * pxcm
    for gx in range(0, width, max(1, round(step))):
        dr.line([(gx, head), (gx, H)], fill=GRID)
    f, fb = font(20), font(30)
    what = "perched, side" if view == "side" else "gliding, from above"
    dr.text((gap, 18), f"{label}: {what}, true scale (grid 10 cm)", fill=INK, font=fb)
    y = head
    for r, h in zip(rows, heights):
        base = y + h - lab - gap // 2
        x = gap
        for i, im in r:
            yy = base - im.height if view == "side" else base - (h - lab - gap) // 2 - im.height // 2
            sheet.paste(im, (x, yy), im)
            t = f"{i} {key.get(i, '?')}"
            tw = dr.textlength(t, font=f)
            dr.text((x + max(0, (im.width - tw) / 2), base + 6), t, fill=INK, font=f)
            x += max(im.width, int(tw)) + gap
        if view == "side":
            dr.line([(gap, base), (width - gap, base)], fill=(110, 116, 126), width=2)
        y += h
    sheet.save(os.path.join(OUT, f"{'lineup' if view == 'side' else 'flight'}_{label}.jpg"), quality=88)
    return sheet


def compare(cols=6, cw=300, ch=240):
    (la, da), (lb, db) = SETS[:2]
    ids = sorted(set(json.load(open(os.path.join(da, "scale.json")))) | set(json.load(open(os.path.join(db, "scale.json")))),
                 key=lambda i: LEN.get(i, 0))
    rows = (len(ids) + cols - 1) // cols
    W, Hc = cols * (2 * cw + 20), ch + 36
    sheet = Image.new("RGB", (W, rows * Hc + 60), BG)
    dr = ImageDraw.Draw(sheet)
    f, fb = font(18), font(28)
    dr.text((14, 14), f"side views, each fitted to its cell: {la} (left) / {lb} (right)", fill=INK, font=fb)
    for n, i in enumerate(ids):
        cx, cy = n % cols * (2 * cw + 20), 60 + n // cols * Hc
        for k, d in enumerate((da, db)):
            im = load(d, i, "side")
            if im is None:
                continue
            im, _ = crop(im)
            s = min((cw - 12) / im.width, (ch - 12) / im.height)
            im = im.resize((max(1, round(im.width * s)), max(1, round(im.height * s))), Image.LANCZOS)
            x0 = cx + k * cw + (cw - im.width) // 2
            sheet.paste(im, (x0, cy + ch - im.height - 4), im)
        dr.line([(cx + cw, cy + 8), (cx + cw, cy + ch - 8)], fill=GRID, width=2)
        dr.text((cx + 8, cy + ch + 4), f"{i} ({LEN.get(i, '?')} cm)", fill=INK, font=f)
    sheet.save(os.path.join(OUT, "side_compare.jpg"), quality=88)


sheets = []
for label, d in SETS:
    sheets.append(lineup(d, label, "side", 4.0, 3200))
    lineup(d, label, "top", 2.0, 3200)
if len(SETS) >= 2:
    compare()
    a, b = sheets[:2]
    st = Image.new("RGB", (max(a.width, b.width), a.height + b.height), BG)
    st.paste(a, (0, 0))
    st.paste(b, (0, a.height))
    st.save(os.path.join(OUT, "lineup_compare.jpg"), quality=88)
print("SHEETS", OUT)
