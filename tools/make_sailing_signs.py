#!/usr/bin/env python3
"""The sailing school's signs, one picture per language (local fonts, no paid API): docs/yacht_school.md "Meshes (part C)".

  python3 tools/make_sailing_signs.py [lang ...]     -> textures/localized/<lang>/sail_sign_<name>.png

Words only from the strings tables: unreal/Lyudmilovsk/Content/Localization/sailing/strings_<lang>.json (part G's table,
keys sign.sail.*), then the main table strings_<lang>.json; a key missing from both falls back to
blender/sailing/sign_strings.json, the proposal part C hands to part G (move the keys into the table and delete it).

  name  the gable band «ДЮСШ парусного спорта «Луч»» (6.0 x 0.58 m): white enamel, dark blue capitals, the sun emblem
  door  the plate by the people's door (0.5 x 0.4 m): the school's name in two lines and the class emblem

The boathouse's slots SLP_Sign_Name / SLP_Sign_Door take these (unreal/scripts/import_sailing.py imports them to
/Game/Sailing/Localized/<lang>/T_Sail_Sign_<name>; the engine shows the player's language with LyudText::LoadTexture).
"""
import json
import os
import sys

from PIL import Image, ImageDraw

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
sys.path.insert(0, os.path.join(ROOT, "blender", "sailing"))
import textures as TX  # noqa: E402

TABLES = [os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Localization", "sailing", "strings_%s.json"),
          os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Localization", "strings_%s.json")]
PROPOSAL = os.path.join(ROOT, "blender", "sailing", "sign_strings.json")
OUT = os.path.join(ROOT, "textures", "localized", "%s")
FONTS = {
    "bold": ["/System/Library/Fonts/Supplemental/Arial Bold.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
             "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf"],
    "ja": ["/System/Library/Fonts/ヒラギノ角ゴシック W6.ttc", "/System/Library/Fonts/Hiragino Sans GB.ttc",
           "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc", "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
           "/System/Library/Fonts/Supplemental/Arial Unicode.ttf"],
}
NAVY, ENAMEL = (26, 44, 92), (242, 240, 232)
SIZES = {"name": (2048, 198), "door": (640, 512)}


def font(lang, size):
    from PIL import ImageFont
    for p in (FONTS["ja"] if lang == "ja" else FONTS["bold"]):
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return None


def strings(lang):
    out = {}
    for t in reversed(TABLES):
        p = t % lang
        if os.path.exists(p):
            with open(p) as f:
                out.update(json.load(f))
    if os.path.exists(PROPOSAL):
        with open(PROPOSAL) as f:
            prop = json.load(f).get(lang, {})
        for k, v in prop.items():
            out.setdefault(k, v)
    return out


def fit(d, text, lang, size, width, height):
    s = size
    while s > 8:
        f = font(lang, s)
        if f is None:
            return None
        bb = d.textbbox((0, 0), text, font=f)
        if bb[2] - bb[0] <= width and bb[3] - bb[1] <= height:
            return f
        s -= 2
    return font(lang, 8)


def sign_name(lang, S):
    w, h = SIZES["name"]
    img = Image.new("RGB", (w, h), ENAMEL)
    d = ImageDraw.Draw(img)
    d.rectangle([6, 6, w - 7, h - 7], outline=NAVY, width=8)
    TX.emblem_luch(d, 130, h * 0.62, h * 0.42)
    TX.emblem_luch(d, w - 130, h * 0.62, h * 0.42)
    text = S.get("sign.sail.school_name")
    if text:
        f = fit(d, text, lang, 130, w - 580, h - 50)       # a clear gap to the emblems at both ends
        if f:
            d.text((w / 2, h / 2), text, fill=NAVY, font=f, anchor="mm")
    return img


def sign_door(lang, S):
    w, h = SIZES["door"]
    img = Image.new("RGB", (w, h), ENAMEL)
    d = ImageDraw.Draw(img)
    d.rectangle([5, 5, w - 6, h - 6], outline=NAVY, width=6)
    TX.emblem_luch(d, w / 2, h * 0.26, h * 0.17)
    for k, key in enumerate(("sign.sail.door_line1", "sign.sail.door_line2")):
        text = S.get(key)
        if text:
            f = fit(d, text, lang, 64, w - 80, 70)
            if f:
                d.text((w / 2, h * (0.61 + 0.19 * k)), text, fill=NAVY, font=f, anchor="mm")
    return img


def main(langs):
    for lang in langs:
        S = strings(lang)
        os.makedirs(OUT % lang, exist_ok=True)
        for name, fn in (("name", sign_name), ("door", sign_door)):
            fn(lang, S).save(os.path.join(OUT % lang, "sail_sign_%s.png" % name))
        print("SAILING signs", lang)


if __name__ == "__main__":
    main(sys.argv[1:] or ["ru", "en", "ja"])
