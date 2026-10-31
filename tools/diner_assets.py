"""The diner's inventory icons, cut out of one Higgsfield grid (docs/diner.md):

  design/diner/icons_grid.png    3x3 icons of the menu (no words: one picture for every language) -> textures/localized/ru/icon_<id>.jpg

The grid's sidecar (design/diner/icons_grid.json) holds the request and the output URL; a missing grid is downloaded from it.
Then: python3 tools/ui_diary_icons.py <ids> (the diary's sketches), unreal/scripts/import_hall_assets.py (T_Icon_<id>) and
unreal/scripts/import_diary_assets.py.

  python3 tools/diner_assets.py
"""
import json
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from grocery_assets import cut, sidecar  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def main():
    side = os.path.join(ROOT, "design", "diner", "icons_grid.json")
    png = os.path.join(ROOT, "design", "diner", "icons_grid.png")
    with open(side, encoding="utf-8") as f:
        spec = json.load(f)
    if not os.path.exists(png) or os.path.getsize(png) < 10000:
        import urllib.request
        urllib.request.urlretrieve(spec["output"], png)
    img = Image.open(png).convert("RGB")
    out = os.path.join(ROOT, "textures", "localized", "ru")
    for i, name in enumerate(spec["ids"]):
        c, r = i % 3, i // 3
        cut(img, 3, 3, c, r, 0.025).resize((512, 512), Image.LANCZOS).save(os.path.join(out, "icon_%s.jpg" % name), quality=92)
        sidecar(os.path.join(out, "icon_%s.json" % name), {"id": "icon_" + name, "lang": "ru", "service": "higgsfield",
                                                          "grid": os.path.relpath(png, ROOT), "cell": [c, r]})
    print("diner icons", len(spec["ids"]))


if __name__ == "__main__":
    main()
