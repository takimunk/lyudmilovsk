"""Contact sheets and inventory icons from the renders of blender/props/corvid_items.py (system python3 with PIL; Blender has none).

    python3 blender/props/corvid_items_sheet.py <export/corvid_items dir> <repo root>

  <dir>/contact_sheet.png, contact_sheet_lod1.png, contact_sheet_top.png   every item close up (3/4 view, LOD1, from above)
  textures/localized/<lang>/icon_find_<id>.jpg                               512 x 512 inventory icons (en, ru, ja), the style of the other icons:
                                                                             warm dark vignette, object ~80% of the frame, soft shadow
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

OUT, ROOT = sys.argv[1], sys.argv[2]
RENDERS = os.path.join(OUT, "renders")
LANGS = ("en", "ru", "ja")
ORDER = ["Acorn", "Hazelnut", "Walnut", "PineCone", "Mussel", "Crust", "Fish", "FeedPile",
         "BottleCap", "Foil", "Ring", "Button", "Coin", "Key", "SeaGlass", "Stick"]


def backdrop(w, h, c_in, c_out):
    y, x = np.mgrid[0:h, 0:w]
    d = np.hypot((x - w / 2) / (w / 2), (y - h * 0.45) / (h / 2)) / 1.25
    t = np.clip(d, 0, 1)[..., None] ** 1.5
    a = np.array(c_in, float) * (1 - t) + np.array(c_out, float) * t
    return Image.fromarray(a.astype(np.uint8), "RGB").convert("RGBA")


def font(sz):
    for p in ("/System/Library/Fonts/Helvetica.ttc", "/System/Library/Fonts/Supplemental/Arial.ttf"):
        if os.path.exists(p):
            return ImageFont.truetype(p, sz)
    return ImageFont.load_default()


def sheet(suffix, out_name, meta, cols=4, cell=440):
    names = [n for n in ORDER if os.path.exists(os.path.join(RENDERS, n + suffix + ".png"))]
    if not names:
        return
    rows = (len(names) + cols - 1) // cols
    sh = Image.new("RGB", (cols * cell, rows * cell), (40, 40, 44))
    d = ImageDraw.Draw(sh)
    f = font(17)
    for i, n in enumerate(names):
        x, y = (i % cols) * cell, (i // cols) * cell
        bg = backdrop(cell, cell, (118, 118, 124), (70, 70, 76))
        im = Image.open(os.path.join(RENDERS, n + suffix + ".png")).convert("RGBA").resize((cell, cell), Image.LANCZOS)
        bg.alpha_composite(im)
        sh.paste(bg.convert("RGB"), (x, y))
        mm = meta.get(n, {})
        tri = mm.get("lod1_tris" if "lod1" in out_name else "tris", "?")
        sz = mm.get("size_cm", [0, 0, 0])
        d.text((x + 10, y + 8), "%s  %s tris  %.1f cm" % (n, tri, max(sz)), fill=(240, 240, 240), font=f)
        d.rectangle([x, y, x + cell - 1, y + cell - 1], outline=(30, 30, 34))
    path = os.path.join(OUT, out_name)
    sh.save(path)
    print("SHEET", path)


def icon(name, item_id):
    src = os.path.join(RENDERS, "icon_%s.png" % name)
    if not os.path.exists(src):
        return
    S = 512
    bg = backdrop(S, S, (63, 54, 47), (30, 25, 22))
    im = Image.open(src).convert("RGBA").resize((S, S), Image.LANCZOS)
    bg.alpha_composite(im)
    rgb = bg.convert("RGB")
    for lang in LANGS:
        folder = os.path.join(ROOT, "textures", "localized", lang)
        os.makedirs(folder, exist_ok=True)
        rgb.save(os.path.join(folder, "icon_find_%s.jpg" % item_id), quality=92)
    return rgb


def main():
    meta = json.load(open(os.path.join(OUT, "meta.json")))
    sheet("", "contact_sheet.png", meta)
    sheet("_LOD1", "contact_sheet_lod1.png", meta)
    sheet("_top", "contact_sheet_top.png", meta)
    ids = {"Acorn": "acorn", "Hazelnut": "hazelnut", "Walnut": "walnut", "PineCone": "pinecone", "Mussel": "mussel", "BottleCap": "bottlecap",
           "Foil": "foil", "Ring": "ring", "Button": "button", "Coin": "coin", "Key": "key", "SeaGlass": "seaglass"}
    made = []
    for n, i in ids.items():
        r = icon(n, i)
        if r is not None:
            made.append(r)
    if made:
        sh = Image.new("RGB", (4 * 256, ((len(made) + 3) // 4) * 256))
        for j, r in enumerate(made):
            sh.paste(r.resize((256, 256), Image.LANCZOS), ((j % 4) * 256, (j // 4) * 256))
        sh.save(os.path.join(OUT, "icons_sheet.png"))
        print("SHEET", os.path.join(OUT, "icons_sheet.png"))


main()
