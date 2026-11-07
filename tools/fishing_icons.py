"""The fishing items' inventory icons (docs/fishing.md), cut out of five Higgsfield pictures (design/fishing/icons_grid_<n>.json,
3x3, 3x3, 3x3, 2x2, a single one for the fried fish served at the diner, and a 3x2 for the better rods and the new baits;
the prompts and job ids are pinned there). No words on them: one picture serves every language.

  python3 tools/fishing_icons.py        downloads each grid's png if missing (run it on the Mac: the cloud container can't reach
                                        Higgsfield's CDN), writes textures/localized/ru/icon_<id>.jpg (512 px) + a sidecar json
Then unreal/scripts/import_hall_assets.py (icons) and python3 tools/ui_diary_icons.py <ids> (the diary's sketches).
"""
import json
import os
import urllib.request

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DESIGN = os.path.join(ROOT, "design", "fishing")


def main():
    out = os.path.join(ROOT, "textures", "localized", "ru")
    os.makedirs(out, exist_ok=True)
    for n in range(1, 7):
        name = "icons_grid_%d" % n
        with open(os.path.join(DESIGN, name + ".json"), encoding="utf-8") as f:
            spec = json.load(f)
        png = os.path.join(DESIGN, name + ".png")
        if not os.path.exists(png):
            if not spec.get("output"):
                print("no output url yet for", name)
                continue
            print("downloading", spec["output"])
            urllib.request.urlretrieve(spec["output"], png)
        img = Image.open(png).convert("RGB")
        cols = spec.get("cols", spec["grid"])
        rows = spec.get("rows", spec["grid"])
        w, h = img.size
        for i, item in enumerate(spec["ids"]):
            if item not in spec["ship"]:
                continue
            c, r = i % cols, i // cols
            cell = img.crop((c * w // cols, r * h // rows, (c + 1) * w // cols, (r + 1) * h // rows))
            m = int(cell.width * 0.02)
            cell = cell.crop((m, m, cell.width - m, cell.height - m)).resize((512, 512), Image.LANCZOS)
            cell.save(os.path.join(out, "icon_%s.jpg" % item), quality=90)
            with open(os.path.join(out, "icon_%s.json" % item), "w", encoding="utf-8") as f:
                json.dump({"id": "icon_" + item, "lang": "ru", "service": "higgsfield", "grid": "design/fishing/%s.png" % name, "cell": [c, r]}, f, indent=1)
            print("icon", item)


if __name__ == "__main__":
    main()
