#!/usr/bin/env python3
"""Cuts the allotment's icon grids (localization/icon_grids_garden.json: a 3x3 and a 3x2 Higgsfield picture) into one
icon per item (tools/garden/data.py ITEMS).

  python3 tools/garden/icons.py            download each grid that isn't in design/garden/ yet (its job_id and output_url
                                           are pinned in the plan), write its sidecar, cut every grid
  python3 tools/garden/icons.py --cut      only cut the grids already in design/garden/
  python3 tools/garden/icons.py --plan localization/icon_grids_kitchen.json   the same for another plan (docs/kitchen.md)

Writes design/garden/<grid id>.png + .json (service, tool, every argument, job id, output URL, sha256) and
textures/localized/ru/icon_<item id>.jpg (a cell, a 3 % margin trimmed so no neighbour's edge shows, 512 px). The
icon_<id> entries of localization/assets.json are already written. Then on the Mac, look at every icon, and run
unreal/scripts/import_hall_assets.py and python3 tools/ui_diary_icons.py <id> + unreal/scripts/import_diary_assets.py
(the game-items skill).
"""
import hashlib
import json
import os
import sys
import urllib.request

from PIL import Image

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
GRIDS = os.path.join(ROOT, "localization", "icon_grids_garden.json")


def fetch(grid):
    dst = os.path.join(ROOT, grid["output"])
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with urllib.request.urlopen(grid["output_url"], timeout=120) as r, open(dst, "wb") as f:
        f.write(r.read())
    side = {"service": grid["service"], "tool": grid["tool"], "model": grid["model"],
            "arguments": {"aspect_ratio": grid["aspect_ratio"], "resolution": grid["resolution"],
                          "quality": grid["quality"], "prompt": grid["prompt"]},
            "job_id": grid["job_id"], "output_url": grid["output_url"], "credits": grid.get("credits", 1),
            "generated": grid.get("generated", ""), "cells_row_major": grid["ids"],
            "sha256": hashlib.sha256(open(dst, "rb").read()).hexdigest()}
    with open(dst[:-4] + ".json", "w", encoding="utf-8") as f:
        json.dump(side, f, ensure_ascii=False, indent=1)
        f.write("\n")
    print("fetched", grid["output"])


def cut(grid):
    im = Image.open(os.path.join(ROOT, grid["output"])).convert("RGB")
    cols, rows = grid["cols"], grid["rows"]
    w, h = im.size
    cw, ch = w / float(cols), h / float(rows)
    m = 0.03
    for i, iid in enumerate(grid["ids"]):
        r, c = divmod(i, cols)
        box = (int(c * cw + m * cw), int(r * ch + m * ch), int((c + 1) * cw - m * cw), int((r + 1) * ch - m * ch))
        cell = im.crop(box)
        side = min(cell.size)  # a 3:2 picture's cells come out square already; keep it square whatever the model gave
        cell = cell.crop(((cell.width - side) // 2, (cell.height - side) // 2,
                          (cell.width - side) // 2 + side, (cell.height - side) // 2 + side))
        path = os.path.join(ROOT, "textures", "localized", "ru", "icon_%s.jpg" % iid)
        cell.resize((512, 512), Image.LANCZOS).save(path, quality=90)
        print("wrote", os.path.relpath(path, ROOT))


def main():
    # --plan <file>: another grid plan of the same shape (the kitchen's dishes: localization/icon_grids_kitchen.json)
    args = sys.argv[1:]
    plan = GRIDS
    if "--plan" in args:
        plan = os.path.join(ROOT, args[args.index("--plan") + 1])
    data = json.load(open(plan, encoding="utf-8"))
    only_cut = "--cut" in args
    for grid in data["grids"]:
        have = os.path.exists(os.path.join(ROOT, grid["output"]))
        if not have and not only_cut:
            fetch(grid)
            have = True
        if have:
            cut(grid)
        else:
            print("missing", grid["output"], "(run without --cut to download it)")


if __name__ == "__main__":
    main()
