#!/usr/bin/env python3
"""Cuts the winter items' icon grid (tools/winter_play/icon_grids.json: one 3x2 Higgsfield picture) into one icon per item.

  python3 tools/winter_play/icons.py <grid id> <job id> <output url>    download the grid, write its sidecar, cut it
  python3 tools/winter_play/icons.py --cut                              cut the grids already in design/winter_play/

Writes design/winter_play/<grid id>.png + .json (service, tool, every argument, job id, output URL, sha256),
textures/localized/ru/icon_<record id>.jpg (a cell, a 3 % margin trimmed so no neighbour's edge shows, 512 px), and the
icon_<record id> entries of localization/assets.json. Then on the Mac: unreal/scripts/import_hall_assets.py and
python3 tools/ui_diary_icons.py <id> + unreal/scripts/import_diary_assets.py (the game-items skill).
"""
import hashlib
import json
import os
import sys
import urllib.request

from PIL import Image

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
GRIDS = os.path.join(ROOT, "tools", "winter_play", "icon_grids.json")


def cut(grid):
    src = os.path.join(ROOT, grid["output"])
    im = Image.open(src).convert("RGB")
    w, h = im.size
    cols, rows = grid.get("cols", 3), grid.get("rows", 3)
    cw, ch = w / float(cols), h / float(rows)
    m = 0.03
    out = []
    for i, rid in enumerate(grid["ids"]):
        r, c = divmod(i, cols)
        box = (int(c * cw + m * cw), int(r * ch + m * ch), int((c + 1) * cw - m * cw), int((r + 1) * ch - m * ch))
        cell = im.crop(box).resize((512, 512), Image.LANCZOS)
        path = os.path.join(ROOT, "textures", "localized", "ru", "icon_%s.jpg" % rid)
        cell.save(path, quality=90)
        out.append(rid)
        print("wrote", os.path.relpath(path, ROOT))
    return out


def register(grid, ids):
    path = os.path.join(ROOT, "localization", "assets.json")
    with open(path, encoding="utf-8") as f:
        man = json.load(f)
    strings = json.load(open(os.path.join(ROOT, "tools", "winter_play", "strings.json"), encoding="utf-8"))
    pos = ["row 1, left", "row 1, middle", "row 1, right", "row 2, left", "row 2, middle", "row 2, right"]
    for i, rid in enumerate(ids):
        desc = strings.get("item.%s.desc" % rid, {}).get("en", "")
        man["assets"]["icon_" + rid] = {
            "kind": "icon", "style": "icon", "aspect": "1:1",
            "prompt": desc,
            "source": "tools/winter_play/icon_grids.json: %s, cell %d (%s), one 3x2 Higgsfield grid (the game-assets rule)" % (grid["id"], i + 1, pos[i]),
        }
    with open(path, "w", encoding="utf-8") as f:
        json.dump(man, f, ensure_ascii=False, indent=1)
        f.write("\n")


def main():
    data = json.load(open(GRIDS, encoding="utf-8"))
    if len(sys.argv) == 4:
        gid, job, url = sys.argv[1:4]
        grid = next(g for g in data["grids"] if g["id"] == gid)
        dst = os.path.join(ROOT, grid["output"])
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with urllib.request.urlopen(url, timeout=120) as r, open(dst, "wb") as f:
            f.write(r.read())
        side = {"service": grid["service"], "tool": grid["tool"], "model": grid["model"],
                "arguments": {"aspect_ratio": grid["aspect_ratio"], "resolution": grid["resolution"], "quality": grid["quality"], "prompt": grid["prompt"]},
                "job_id": job, "output_url": url, "credits": 1, "cells_row_major": grid["ids"],
                "sha256": hashlib.sha256(open(dst, "rb").read()).hexdigest()}
        with open(dst[:-4] + ".json", "w", encoding="utf-8") as f:
            json.dump(side, f, ensure_ascii=False, indent=1)
            f.write("\n")
        register(grid, cut(grid))
        return
    for grid in data["grids"]:
        if os.path.exists(os.path.join(ROOT, grid["output"])):
            register(grid, cut(grid))


if __name__ == "__main__":
    main()
