"""The sea railway's surface textures (docs/railway.md): one Higgsfield grid (design/railway/textures/gen/grid_a.png,
3x3 swatches, pinned in gen/grid_a.json) cut into nine tiles, made seamless, sized to 1024, with normal and roughness
maps, by the cable car's own texture code (tools/transport_textures.py):

  design/railway/textures/T_RL_<name>_{C,N,R}.png    python3 tools/railway_textures.py [--preview out.jpg]

unreal/scripts/import_railway.py imports them into /Game/Railway/Textures; the line's materials are M_TR_Surface's
instances over them (Private/Railway/RailMaterials.cpp, names "tx_<name>").
"""
import argparse
import json
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import transport_textures as TT  # noqa: E402

REPO = os.path.normpath(os.path.join(HERE, ".."))
DIR = os.path.join(REPO, "design", "railway", "textures")
GRID = os.path.join(DIR, "gen", "grid_a.png")

# cell -> name, periodic axes, relief, roughness base and spread, metres per tile, invert relief
TILES = {
    (0, 0): ("ballast", "", 3.0, 0.85, 0.10, 0.8, False),
    (0, 1): ("armour", "", 3.5, 0.75, 0.15, 2.4, False),
    (0, 2): ("planks", "y", 2.0, 0.80, 0.10, 1.6, True),
    (1, 0): ("planks_dark", "y", 2.0, 0.85, 0.08, 1.6, True),
    (1, 1): ("rubber", "y", 2.5, 0.70, 0.10, 1.2, False),
    (1, 2): ("tunnel", "", 1.5, 0.90, 0.08, 3.0, False),
    (2, 0): ("wetsand", "", 1.2, 0.55, 0.15, 1.5, False),
    (2, 1): ("turf", "", 1.8, 0.90, 0.05, 1.8, False),
    (2, 2): ("lichen", "", 2.2, 0.85, 0.10, 2.0, False),
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview")
    a = ap.parse_args()
    TT.DIR = DIR
    grid = Image.open(GRID).convert("RGB")
    sheet = Image.new("RGB", (3 * 512, 3 * 512)) if a.preview else None
    meta = {}
    w, h = grid.size
    m = 6     # equal cells (the grid has no gutters): a few pixels in from each seam
    cells = {(r, c): grid.crop((c * w // 3 + m, r * h // 3 + m, (c + 1) * w // 3 - m, (r + 1) * h // 3 - m)) for r in range(3) for c in range(3)}
    for (r, c), img in cells.items():
        name, axes, strength, rough, spread, tile, invert = TILES[(r, c)]
        rgb = TT.make(name, img, axes, strength, rough, spread, invert)
        for k in "CNR":
            os.replace(os.path.join(DIR, "T_TR_%s_%s.png" % (name, k)), os.path.join(DIR, "T_RL_%s_%s.png" % (name, k)))
        meta[name] = {"tile_m": tile}
        if sheet:
            sheet.paste(Image.fromarray(np.tile(rgb, (2, 2, 1))).resize((512, 512)), (c * 512, r * 512))
        print("texture", name)
    with open(os.path.join(DIR, "textures.json"), "w") as f:
        json.dump({"_doc": "Made by tools/railway_textures.py: the metres one tile of each texture covers.", "textures": meta}, f, indent=1)
    if sheet:
        sheet.save(a.preview, quality=88)


if __name__ == "__main__":
    main()
