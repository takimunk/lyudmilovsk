#!/usr/bin/env python3
"""The restaurant's small localized sign by the main door (docs/restaurant.md), one picture per language (local fonts, no paid API).
The rooftop «ЯНТАРЬ» letters stay Cyrillic (architecture of a Russian town); this plaque on the menu stand reads in the player's
language. Words from strings_<lang>.json: sign.rest.name, sign.rest.plaque, sign.rest.hours.

  python3 tools/restaurant/signs.py [lang ...]     -> textures/localized/<lang>/rest_sign_plaque.png

unreal/scripts/import_restaurant.py (import kit=restaurant_kit) imports them to /Game/Restaurant/Localized/<lang>/T_Sign_plaque;
ALyudRestaurant shows the player's language.
"""
import json
import os
import sys

from PIL import Image, ImageDraw

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from make_diner_signs import STRINGS, OUT, fit, centred  # noqa: E402

AMBER, CREAM, BRONZE = (178, 92, 18), (250, 238, 214), (92, 64, 40)


def make(lang):
    with open(STRINGS % lang, encoding="utf-8") as f:
        s = json.load(f)
    W, H = 1024, 512
    im = Image.new("RGB", (W, H), AMBER)
    d = ImageDraw.Draw(im)
    d.rectangle((14, 14, W - 15, H - 15), outline=BRONZE, width=10)
    d.rectangle((34, 34, W - 35, H - 35), outline=CREAM, width=3)
    name = s["sign.rest.name"]
    f = fit(d, name, "bold", lang, 200, W - 140, 210)
    centred(d, W / 2, 200, name, f, CREAM)
    d.line((180, 318, W - 180, 318), fill=CREAM, width=4)
    sub = s["sign.rest.plaque"]
    f2 = fit(d, sub, "regular", lang, 60, W - 160, 70)
    centred(d, W / 2, 370, sub, f2, CREAM)
    hours = s["sign.rest.hours"]
    f3 = fit(d, hours, "regular", lang, 46, W - 200, 54)
    centred(d, W / 2, 440, hours, f3, CREAM)
    out = os.path.join(OUT % lang, "rest_sign_plaque.png")
    im.save(out)
    print(out)


if __name__ == "__main__":
    for lang in (sys.argv[1:] or ["ru", "en", "ja"]):
        make(lang)
