"""The street food's inventory icons (docs/food.md), cut out of one Higgsfield grid: design/food/icons_grid.png (2x2, its sidecar
icons_grid.json holds the request and the output URL; a missing grid is downloaded from it) -> textures/localized/ru/icon_<id>.jpg
(no words on them: English and Japanese fall back to ru). Then python3 tools/ui_diary_icons.py <ids> (the diary's sketches),
unreal/scripts/import_hall_assets.py (T_Icon_<id>) and unreal/scripts/import_diary_assets.py.

  python3 tools/food_icons.py
"""
import json
import os
import urllib.request

from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def main():
    side = os.path.join(ROOT, "design", "food", "icons_grid.json")
    png = os.path.join(ROOT, "design", "food", "icons_grid.png")
    spec = json.load(open(side, encoding="utf-8"))
    if not os.path.exists(png) or os.path.getsize(png) < 10000:
        urllib.request.urlretrieve(spec["output"], png)
    img = Image.open(png).convert("RGB")
    out = os.path.join(ROOT, "textures", "localized", "ru")
    w, h = img.size
    for i, name in enumerate(spec["ids"]):
        c, r = i % 2, i // 2
        cell = img.crop((c * w // 2, r * h // 2, (c + 1) * w // 2, (r + 1) * h // 2))
        m = int(cell.width * 0.02)
        cell.crop((m, m, cell.width - m, cell.height - m)).resize((512, 512), Image.LANCZOS).save(os.path.join(out, "icon_%s.jpg" % name), quality=92)
        with open(os.path.join(out, "icon_%s.json" % name), "w", encoding="utf-8") as f:
            json.dump({"id": "icon_" + name, "lang": "ru", "service": "higgsfield", "grid": "design/food/icons_grid.png", "cell": [c, r]}, f, indent=1)
    print("icons:", spec["ids"])


if __name__ == "__main__":
    main()
