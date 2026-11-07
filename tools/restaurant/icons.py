"""The restaurant's menu icons, cut out of one Higgsfield grid (docs/restaurant.md):

  design/restaurant/icons_grid.png   4x4 icons of the menu (no words: one picture for every language) -> textures/localized/ru/icon_<id>.jpg

The grid's sidecar (design/restaurant/icons_grid.json) holds the request and the output URL; a missing grid is downloaded from it.
Then: python3 tools/ui_diary_icons.py <ids> (the diary's sketches) and unreal/scripts/import_hall_assets.py (T_Icon_<id>).

  python3 tools/restaurant/icons.py
"""
import json
import os
import sys

from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from grocery_assets import cut, sidecar  # noqa: E402


def main():
    import glob
    n = 0
    for side in sorted(glob.glob(os.path.join(ROOT, "design", "restaurant", "icons_grid*.json"))):
        n += grid(side, side[:-5] + ".png")
    print("restaurant icons", n)


def grid(side, png):
    with open(side, encoding="utf-8") as f:
        spec = json.load(f)
    if not os.path.exists(png) or os.path.getsize(png) < 10000:
        import urllib.request
        urllib.request.urlretrieve(spec["output"], png)
    img = Image.open(png).convert("RGB")
    out = os.path.join(ROOT, "textures", "localized", "ru")
    n = 0
    cols, rows = spec.get("cols", 4), spec.get("rows", 4)
    for i, name in enumerate(spec["ids"]):
        if not name:
            continue
        c, r = i % cols, i // cols
        cell = cut(img, cols, rows, c, r, 0.02)
        side_px = min(cell.size)
        x0, y0 = (cell.width - side_px) // 2, (cell.height - side_px) // 2
        cell.crop((x0, y0, x0 + side_px, y0 + side_px)).resize((512, 512), Image.LANCZOS).save(os.path.join(out, "icon_%s.jpg" % name), quality=92)
        sidecar(os.path.join(out, "icon_%s.json" % name), {"id": "icon_" + name, "lang": "ru", "service": "higgsfield",
                                                          "grid": os.path.relpath(png, ROOT), "cell": [c, r]})
        n += 1
    return n


if __name__ == "__main__":
    main()
