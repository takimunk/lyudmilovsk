#!/usr/bin/env python3
"""The diner's signs, one picture per language (local fonts, no paid API): docs/diner.md.

  python3 tools/make_diner_signs.py [lang ...]     -> textures/localized/<lang>/diner_sign_<name>.png

Words only from unreal/Lyudmilovsk/Content/Localization/strings_<lang>.json (sign.diner.*, the menu's item names and the
prices of PeopleData/counters.json). The 1940s diner's lettering: the name in red script-like italics on the cream enamel band,
the band's words in stainless-steel capitals, the roof sign red on cream with a steel rim (lit at night), the menu board white
letters on black felt with a red title, the coffee card on cream. unreal/scripts/import_diner.py imports them to
/Game/Diner/Localized/<lang>/T_Sign_<name>; ALyudDiner shows the player's language.
"""
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
STRINGS = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Localization", "strings_%s.json")
COUNTERS = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "PeopleData", "counters.json")
OUT = os.path.join(ROOT, "textures", "localized", "%s")
FONTS = {
    "script": ["/System/Library/Fonts/Supplemental/Arial Bold Italic.ttf", "/System/Library/Fonts/Supplemental/Arial Unicode.ttf"],
    "bold": ["/System/Library/Fonts/Supplemental/Arial Bold.ttf", "/System/Library/Fonts/Supplemental/Arial Unicode.ttf"],
    "regular": ["/System/Library/Fonts/Supplemental/Arial.ttf", "/System/Library/Fonts/Supplemental/Arial Unicode.ttf"],
    "ja": ["/System/Library/Fonts/ヒラギノ角ゴシック W6.ttc", "/System/Library/Fonts/Hiragino Sans GB.ttc", "/System/Library/Fonts/Supplemental/Arial Unicode.ttf"],
}
CREAM, RED, STEEL, INK = (239, 226, 194), (156, 34, 40), (150, 154, 158), (40, 30, 26)
# the picture sizes follow the quads' proportions in blender/diner/building.py signs()
SIZES = {"name": (2048, 124), "band": (2048, 124), "roof": (1536, 240), "menu": (1600, 960), "coffee": (800, 400)}


def font(kind, lang, size):
    for path in (FONTS["ja"] if lang == "ja" else FONTS[kind]):
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
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


def band(text, lang, size, bg, fg, kind, rim=None):
    W, H = size
    im = Image.new("RGB", (W, H), bg)
    d = ImageDraw.Draw(im)
    if rim:
        d.rectangle([0, 0, W - 1, H - 1], outline=rim, width=max(4, H // 16))
    f = fit(d, text, kind, lang, int(H * 0.8), W - min(H, 160), int(H * 0.72))
    if kind == "script" and lang != "ja":
        # a thin shadow under the red letters, as the enamel's fired lettering has
        centred(d, W / 2 + 3, H / 2 + 3, text, f, tuple(int(c * 0.75) for c in bg))
    centred(d, W / 2, H / 2, text, f, fg)
    return im.filter(ImageFilter.SMOOTH)


def menu(s, lang):
    W, H = SIZES["menu"]
    im = Image.new("RGB", (W, H), (24, 22, 22))
    d = ImageDraw.Draw(im)
    d.rectangle([10, 10, W - 11, H - 11], outline=STEEL, width=8)
    centred(d, W / 2, 95, s["sign.diner.menu.title"], fit(d, s["sign.diner.menu.title"], "script", lang, 110, W - 300), (206, 52, 52))
    with open(COUNTERS) as f:
        rows = [r for r in json.load(f)["counters"] if r["activity"] == "work_diner"][0]["goods"]
    rows = rows + [{"item": "diner_coffee", "price": 8}]
    top, step = 190, (H - 300) / len(rows)
    for i, g in enumerate(rows):
        y = top + i * step
        name = s["item.%s.name" % g["item"]]
        price = s["sign.diner.menu.price"].replace("{0}", str(g["price"]))
        fn = fit(d, name, "regular", lang, int(step * 0.62), W - 520)
        d.text((90, y), name, font=fn, fill=(236, 232, 222))
        # Arial has no rouble sign: the price in Helvetica Neue Bold
        fp = font("bold", lang, int(step * 0.62)) if lang == "ja" else ImageFont.truetype("/System/Library/Fonts/HelveticaNeue.ttc", int(step * 0.62), index=1)
        pw = d.textlength(price, font=fp)
        d.text((W - 90 - pw, y), price, font=fp, fill=(236, 232, 222))
        # dot leaders between the name and the price
        x0 = 90 + d.textlength(name, font=fn) + 20
        for x in range(int(x0), int(W - 110 - pw), 22):
            d.ellipse([x, y + step * 0.45, x + 5, y + step * 0.45 + 5], fill=(120, 116, 110))
    hours = s["sign.diner.menu.hours"]
    centred(d, W / 2, H - 60, hours, fit(d, hours, "bold", lang, 52, W - 200), (206, 190, 150))
    return im


def make(lang):
    with open(STRINGS % lang, encoding="utf-8") as f:
        s = json.load(f)
    out = OUT % lang
    os.makedirs(out, exist_ok=True)
    pics = {
        "name": band(s["sign.diner.name"], lang, SIZES["name"], CREAM, RED, "script"),
        "band": band(s["sign.diner.band"].upper() if lang != "ja" else s["sign.diner.band"], lang, SIZES["band"], CREAM, (110, 114, 118), "bold"),
        "roof": band(s["sign.diner.roof"], lang, SIZES["roof"], CREAM, RED, "script", rim=RED),
        "coffee": band(s["sign.diner.coffee"], lang, SIZES["coffee"], CREAM, INK, "bold", rim=(90, 43, 27)),
        "menu": menu(s, lang),
    }
    for name, im in pics.items():
        im.save(os.path.join(out, "diner_sign_%s.png" % name))
    print("diner signs", lang, sorted(pics))


if __name__ == "__main__":
    for lang in sys.argv[1:] or ["ru", "en", "ja"]:
        make(lang)
