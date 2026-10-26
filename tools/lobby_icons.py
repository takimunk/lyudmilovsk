"""Cuts the lobby goods' icons out of one Higgsfield grid picture (4x4, row-major, ids below) into
textures/localized/ru/icon_<id>.jpg (512 px, the same as the other icons; en and ja fall back to ru) and writes the sidecar.
  python3 tools/lobby_icons.py design/lobby/icons_grid.png
"""
import json
import os
import sys

from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
IDS = ["water", "coffee", "tea", "chips", "chocolate", "cookies", "ice_cream", "pie", "seeds", "magazine_science",
       "magazine_travel", "daily_paper", "postcard", "badge", "amber_charm", "candy"]

def main(path):
    img = Image.open(path).convert("RGB")
    w, h = img.size
    out = os.path.join(ROOT, "textures", "localized", "ru")
    for i, name in enumerate(IDS):
        c, r = i % 4, i // 4
        cell = img.crop((c * w // 4, r * h // 4, (c + 1) * w // 4, (r + 1) * h // 4))
        m = cell.width // 40                                 # a thin margin off the cell edge
        cell = cell.crop((m, m, cell.width - m, cell.height - m)).resize((512, 512), Image.LANCZOS)
        cell.save(os.path.join(out, "icon_%s.jpg" % name), quality=90)
        with open(os.path.join(out, "icon_%s.json" % name), "w", encoding="utf-8") as f:
            json.dump({"id": "icon_" + name, "lang": "ru", "service": "higgsfield", "grid": os.path.relpath(path, ROOT), "cell": [c, r]}, f, indent=1)
    print(len(IDS), "icons")

if __name__ == "__main__":
    main(os.path.abspath(sys.argv[1]))
