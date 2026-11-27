"""Cuts inventory icons out of Higgsfield grid pictures, from their committed sidecars (the game-assets rule: always a grid).

  python3 tools/icons_from_sheets.py [--force] [--margin 0.02] [sidecar.json ...]

With no sidecars it takes design/icons_1127/sheet_*.json and design/fishing/icons_grid_*.json. For each sidecar:
  1. the grid picture sits next to it as <sidecar>.png; a missing one is downloaded from its output URL (run this on the Mac:
     the cloud container can't reach Higgsfield's CDN),
  2. its sha256 is checked against the sidecar's, or written into the sidecar when it has none yet,
  3. the picture is cut into equal cells (row by row, left to right), each resized to 512 x 512 and saved as
     textures/localized/ru/icon_<id>.jpg (JPEG q90; no words on them, en and ja fall back to ru) with its own sidecar
     icon_<id>.json (model, prompt, source sheet + cell, sha256), the same as the icons_city sheets.
An id that already has a picture is skipped (--force cuts it again). At the end it prints the import and diary commands for
exactly the ids it wrote.

Sidecar keys it reads (both house formats): output_url | output; cells_row_major | ids; ship (optional subset to cut);
grid [cols, rows] | grid n | cols + rows; arguments.prompt | prompt; arguments.resolution/quality | resolution/quality.
Needs only Pillow (pip install pillow).
"""
import argparse
import glob
import hashlib
import json
import os
import sys
import urllib.request

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "textures", "localized", "ru")
SIZE = 512
DEFAULT = ["design/icons_1127/sheet_*.json", "design/fishing/icons_grid_*.json"]
COLS = {1: "left", 2: "middle", 3: "right"}


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def is_picture(path):
    """True for a real picture; False for a missing file or a git LFS pointer (the picture exists upstream, not here)."""
    if not os.path.exists(path):
        return False
    with open(path, "rb") as f:
        return f.read(7) != b"version"


def grid_of(spec):
    g = spec.get("grid")
    if isinstance(g, list):
        return int(g[0]), int(g[1])
    n = int(g) if g else 3
    return int(spec.get("cols", n)), int(spec.get("rows", n))


def cell_name(i, cols, rows):
    c, r = i % cols, i // cols
    if cols == 3:
        where = COLS[c + 1]
    elif cols == 1:
        where = "single"
    else:
        where = "column %d" % (c + 1)
    return "cell %d (row %d, %s)" % (i + 1, r + 1, where)


def load_assets():
    try:
        with open(os.path.join(ROOT, "localization", "assets.json"), encoding="utf-8") as f:
            return json.load(f)["assets"]
    except (OSError, KeyError, ValueError):
        return {}


def cut_sheet(side, assets, force, margin, written):
    rel_side = os.path.relpath(side, ROOT)
    with open(side, encoding="utf-8") as f:
        spec = json.load(f)
    args = spec.get("arguments", spec)
    ids = spec.get("cells_row_major") or spec.get("ids") or []
    ship = set(spec.get("ship") or ids)
    url = spec.get("output_url") or spec.get("output")
    png = os.path.splitext(side)[0] + ".png"
    # an existing icon (even a git LFS pointer: its picture is upstream) is kept unless --force
    todo = [i for i in ids if i in ship and (force or not os.path.exists(os.path.join(OUT, "icon_%s.jpg" % i)))]
    if not todo:
        print("%s: every icon already has a picture" % rel_side)
        return
    if not is_picture(png):
        if not url or not url.startswith("https://"):
            print("%s: no picture and no output URL, skipped" % rel_side)
            return
        print("downloading", url)
        tmp = png + ".part"
        urllib.request.urlretrieve(url, tmp)
        os.replace(tmp, png)
    digest = sha256(png)
    if spec.get("sha256") and spec["sha256"] != digest:
        sys.exit("%s: sha256 %s does not match the sidecar's %s (a different picture?)" % (os.path.relpath(png, ROOT), digest, spec["sha256"]))
    if not spec.get("sha256"):
        spec["sha256"] = digest
        with open(side, "w", encoding="utf-8") as f:
            json.dump(spec, f, ensure_ascii=False, indent=1)
            f.write("\n")
    img = Image.open(png).convert("RGB")
    cols, rows = grid_of(spec)
    w, h = img.size
    model = "higgsfield %s (%s, %s)" % (spec.get("model", "?"), args.get("resolution", "?"), args.get("quality", "?"))
    for i, item in enumerate(ids):
        if item not in todo:
            continue
        c, r = i % cols, i // cols
        cell = img.crop((c * w // cols, r * h // rows, (c + 1) * w // cols, (r + 1) * h // rows))
        m = int(round(min(cell.size) * margin))                      # a thin margin off the cell edge
        if m:
            cell = cell.crop((m, m, cell.width - m, cell.height - m))
        jpg = os.path.join(OUT, "icon_%s.jpg" % item)
        cell.resize((SIZE, SIZE), Image.LANCZOS).save(jpg, "JPEG", quality=90)
        prompt = (assets.get("icon_" + item) or {}).get("prompt", "")
        if not prompt or prompt.startswith("See "):
            prompt = "See %s (%s)." % (rel_side, cell_name(i, cols, rows))
        meta = {
            "id": "icon_" + item,
            "lang": "ru",
            "model": model,
            "aspect": "1:1",
            "prompt": prompt,
            "source": "%s (sidecar %s), %s, cut into %dx%d equal cells, resized to %dx%d"
                      % (os.path.relpath(png, ROOT), rel_side, cell_name(i, cols, rows), cols, rows, SIZE, SIZE),
            "sha256": sha256(jpg),
            "strings": {},
        }
        with open(os.path.join(OUT, "icon_%s.json" % item), "w", encoding="utf-8") as f:
            json.dump(meta, f, ensure_ascii=False, indent=1)
        written.append(item)
        print("icon", item)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("sidecars", nargs="*")
    ap.add_argument("--force", action="store_true", help="cut again even where a picture exists")
    ap.add_argument("--margin", type=float, default=0.02, help="share of the cell trimmed off each edge (default 0.02)")
    a = ap.parse_args()
    sides = [os.path.abspath(s) for s in a.sidecars] or sorted(p for g in DEFAULT for p in glob.glob(os.path.join(ROOT, g)))
    os.makedirs(OUT, exist_ok=True)
    assets = load_assets()
    written = []
    for side in sides:
        cut_sheet(side, assets, a.force, a.margin, written)
    if not written:
        print("nothing written")
        return
    ids = ",".join("icon_" + i for i in written)
    print("\n%d icons written. Look at them, then:" % len(written))
    print("  python3 tools/ui_diary_icons.py " + " ".join(written))
    print("  ICONS=%s UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=unreal/scripts/import_hall_assets.py" % ids)
    print("  ICONS=%s UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=unreal/scripts/import_diary_assets.py" % ids)


if __name__ == "__main__":
    main()
