"""The library book's inventory icon (docs/books.md), cut out of design/books/icon_grid.json (a 2x2 Higgsfield picture of
four covers; the prompt and job id are pinned there). No words on it: one picture serves every language.

  python3 tools/books/book_icon.py     downloads the grid's png if missing (on the Mac: the cloud can't reach the CDN),
                                       writes textures/localized/ru/icon_library_book.jpg (512 px) + a sidecar json
Then unreal/scripts/import_hall_assets.py (icons) and python3 tools/ui_diary_icons.py library_book (the diary's sketch).
"""
import json
import os
import urllib.request

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DESIGN = os.path.join(ROOT, "design", "books")


def main():
    out = os.path.join(ROOT, "textures", "localized", "ru")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(DESIGN, "icon_grid.json"), encoding="utf-8") as f:
        spec = json.load(f)
    png = os.path.join(DESIGN, "icon_grid.png")
    if not os.path.exists(png):
        print("downloading", spec["output"])
        urllib.request.urlretrieve(spec["output"], png)
    img = Image.open(png).convert("RGB")
    n = spec["grid"]
    w, h = img.size
    for i, item in enumerate(spec["ids"]):
        if item not in spec["ship"]:
            continue
        c, r = i % n, i // n
        cell = img.crop((c * w // n, r * h // n, (c + 1) * w // n, (r + 1) * h // n))
        m = int(cell.width * 0.02)
        cell = cell.crop((m, m, cell.width - m, cell.height - m)).resize((512, 512), Image.LANCZOS)
        cell.save(os.path.join(out, "icon_%s.jpg" % item), quality=90)
        with open(os.path.join(out, "icon_%s.json" % item), "w", encoding="utf-8") as f:
            json.dump({"id": "icon_" + item, "lang": "ru", "service": "higgsfield", "grid": "design/books/icon_grid.png", "cell": [c, r]}, f, indent=1)
        print("icon", item)


if __name__ == "__main__":
    main()
