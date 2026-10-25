#!/usr/bin/env python3
"""The Academgorodok library's signs, one picture per language (local fonts, no paid API).

  python3 tools/make_academ_signs.py [lang ...]     -> textures/localized/<lang>/academ_sign_<name>.png

Words come only from unreal/Lyudmilovsk/Content/Localization/strings_<lang>.json (keys sign.academ.library*);
the outside sign copies the enamel plates of the street (white enamel, dark blue letters, a thin blue rule,
like the old «ХЛЕБ» plate on docs/academgorodok/refs/dima/shop.jpg); the boards inside are maroon glass
with gilt letters, like the museum's. unreal/scripts/import_academgorodok.py imports them to
/Game/Academgorodok/Localized/<lang>/T_Sign_<name>; AcademToggle.cpp shows the player's language.
"""
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
STRINGS = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Localization", "strings_%s.json")
OUT = os.path.join(ROOT, "textures", "localized", "%s")
FONTS = {
    "bold": ["/System/Library/Fonts/Supplemental/Arial Bold.ttf", "/System/Library/Fonts/Supplemental/Arial Unicode.ttf"],
    "regular": ["/System/Library/Fonts/Supplemental/Arial.ttf", "/System/Library/Fonts/Supplemental/Arial Unicode.ttf"],
    "ja": ["/System/Library/Fonts/Hiragino Sans GB.ttc", "/System/Library/Fonts/Supplemental/Arial Unicode.ttf"],
}
BLUE = (28, 42, 82)
INSIDE = {   # board name -> key
    "reading_room": "sign.academ.library.reading_room",
    "issue_desk": "sign.academ.library.issue_desk",
    "catalogue": "sign.academ.library.catalogue",
    "silence": "sign.academ.library.silence",
    "hours": "sign.academ.library.hours",
    "cloakroom": "sign.academ.library.cloakroom",
    "periodicals": "sign.academ.library.periodicals",
}


def font(kind, lang, size):
    for path in (FONTS["ja"] if lang == "ja" else FONTS[kind]):
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def fit(draw, text, kind, lang, size, width):
    f = font(kind, lang, size)
    while draw.textlength(text, font=f) > width and size > 10:
        size -= 2
        f = font(kind, lang, size)
    return f


def centred(draw, y, text, f, fill, W):
    w = draw.textlength(text, font=f)
    draw.text(((W - w) / 2, y), text, font=f, fill=fill)


def outside(s, lang, path):
    W, H = 1024, 720
    im = Image.new("RGB", (W, H), (236, 236, 230))
    d = ImageDraw.Draw(im)
    d.rectangle([14, 14, W - 15, H - 15], outline=BLUE, width=10)
    f = fit(d, s["sign.academ.library.sub"].upper(), "regular", lang, 54, W - 140)
    centred(d, 70, s["sign.academ.library.sub"].upper(), f, BLUE, W)
    f = fit(d, s["sign.academ.library"], "bold", lang, 210, W - 120)
    centred(d, 175, s["sign.academ.library"], f, BLUE, W)
    d.rectangle([60, 470, W - 60, 478], fill=BLUE)
    f = fit(d, s["sign.academ.library.hours"], "bold", lang, 64, W - 140)
    centred(d, 520, s["sign.academ.library.hours"], f, BLUE, W)
    # enamel: a soft sheen and a few chips at the edges
    im = im.filter(ImageFilter.SMOOTH)
    d = ImageDraw.Draw(im)
    for x, y, r in ((40, 690, 9), (990, 40, 7), (600, 700, 6), (20, 300, 5)):
        d.ellipse([x - r, y - r, x + r, y + r], fill=(60, 52, 44))
    im.save(path)


def board(text, lang, path, portrait=False):
    if portrait:
        return board_tall(text, lang, path)
    W, H = 1024, 300
    im = Image.new("RGB", (W, H), (88, 18, 26))
    d = ImageDraw.Draw(im)
    d.rectangle([10, 10, W - 11, H - 11], outline=(196, 160, 86), width=6)
    f = fit(d, text, "bold", lang, 120, W - 120)
    bbox = d.textbbox((0, 0), text, font=f)
    centred(d, (H - (bbox[3] - bbox[1])) / 2 - bbox[1], text, f, (222, 190, 112), W)
    im.save(path)


def board_tall(text, lang, path):
    """A portrait board (the opening hours by the arch): the words on as many lines as they need."""
    W, H = 600, 820
    im = Image.new("RGB", (W, H), (88, 18, 26))
    d = ImageDraw.Draw(im)
    d.rectangle([10, 10, W - 11, H - 11], outline=(196, 160, 86), width=6)
    words = text.split(" ") if lang != "ja" else [text[i:i + 4] for i in range(0, len(text), 4)]
    f = font("bold", lang, 92)
    lines, cur = [], ""
    for w_ in words:
        t = (cur + (" " if cur and lang != "ja" else "") + w_)
        if d.textlength(t, font=f) > W - 90 and cur:
            lines.append(cur)
            cur = w_
        else:
            cur = t
    lines.append(cur)
    lh = 120
    y = (H - lh * len(lines)) / 2
    for ln in lines:
        centred(d, y, ln, f, (222, 190, 112), W)
        y += lh
    im.save(path)


TALL = {"hours"}


def main(langs):
    for lang in langs:
        s = json.load(open(STRINGS % lang))
        out = OUT % lang
        os.makedirs(out, exist_ok=True)
        outside(s, lang, os.path.join(out, "academ_sign_library.png"))
        for name, key in INSIDE.items():
            board(s[key], lang, os.path.join(out, "academ_sign_%s.png" % name), portrait=name in TALL)
        print("wrote", out)


if __name__ == "__main__":
    main(sys.argv[1:] or ["ru", "en", "ja"])
