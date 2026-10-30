"""The Academgorodok grocery's pictures, cut out of two Higgsfield grids (docs/bird_taming.md "Feeds and the feeder"):

  design/grocery/labels_grid.png   3x2 packet labels of the bird feeds (Russian words) -> textures/localized/ru/label_<id>.jpg
  design/grocery/icons_grid.png    3x3 inventory icons (feeds, feeder, bread, kefir)    -> textures/localized/ru/icon_<id>.jpg

The grids' sidecars (design/grocery/*_grid.json) hold the request and the output URL; a missing grid is downloaded from it.
Other languages fall back to ru (LyudText::LoadTexture). Then: python3 tools/ui_diary_icons.py <ids> (the diary's sketches),
unreal/scripts/import_hall_assets.py (T_Icon_<id>, T_Label_<id>) and unreal/scripts/import_diary_assets.py.

  python3 tools/grocery_assets.py
"""
import json
import os
import urllib.request

from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT = os.path.join(ROOT, "textures", "localized", "ru")


def grid(name):
    side = os.path.join(ROOT, "design", "grocery", name + ".json")
    png = os.path.join(ROOT, "design", "grocery", name + ".png")
    with open(side, encoding="utf-8") as f:
        spec = json.load(f)
    if not os.path.exists(png) or os.path.getsize(png) < 10000:
        print("downloading", spec["output"])
        urllib.request.urlretrieve(spec["output"], png)
    return spec, Image.open(png).convert("RGB"), os.path.relpath(png, ROOT)


def cut(img, cols, rows, c, r, margin):
    w, h = img.size
    cell = img.crop((c * w // cols, r * h // rows, (c + 1) * w // cols, (r + 1) * h // rows))
    mx, my = int(cell.width * margin), int(cell.height * margin)
    return cell.crop((mx, my, cell.width - mx, cell.height - my))


def trim(cell, bg_tol=26, inset=4):
    """Crops the off-white paper round a label (the grid's gaps) to the label's printed edge, `inset` px inside it
    (the edge is soft, a line of paper would stay)."""
    px = cell.load()
    w, h = cell.size
    ref = px[2, 2]
    def far(p):
        return sum(abs(p[i] - ref[i]) for i in range(3)) > bg_tol
    rows = [y for y in range(h) if sum(far(px[x, y]) for x in range(0, w, 4)) > w // 16]
    cols = [x for x in range(w) if sum(far(px[x, y]) for y in range(0, h, 4)) > h // 16]
    if not rows or not cols:
        return cell
    return cell.crop((cols[0] + inset, rows[0] + inset, cols[-1] + 1 - inset, rows[-1] + 1 - inset))


def sidecar(path, data):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=1, ensure_ascii=False)


def main():
    os.makedirs(OUT, exist_ok=True)
    spec, img, rel = grid("labels_grid")
    for i, name in enumerate(spec["ids"]):
        c, r = i % 3, i // 3
        label = trim(cut(img, 3, 2, c, r, 0.01)).resize((512, 768), Image.LANCZOS)
        label.save(os.path.join(OUT, "label_%s.jpg" % name), quality=92)
        sidecar(os.path.join(OUT, "label_%s.json" % name), {"id": "label_" + name, "lang": "ru", "service": "higgsfield", "grid": rel, "cell": [c, r]})
    print(len(spec["ids"]), "labels")
    spec, img, rel = grid("icons_grid")
    for i, name in enumerate(spec["ids"]):
        c, r = i % 3, i // 3
        icon = cut(img, 3, 3, c, r, 0.025).resize((512, 512), Image.LANCZOS)
        icon.save(os.path.join(OUT, "icon_%s.jpg" % name), quality=90)
        sidecar(os.path.join(OUT, "icon_%s.json" % name), {"id": "icon_" + name, "lang": "ru", "service": "higgsfield", "grid": rel, "cell": [c, r]})
    print(len(spec["ids"]), "icons")


if __name__ == "__main__":
    main()
