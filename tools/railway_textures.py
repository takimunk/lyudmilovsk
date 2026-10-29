"""The sea railway's surface textures (docs/railway.md): Higgsfield grids (design/railway/textures/gen/grid_a.png: the
line; grid_b.png: the train's inside; 3x3 swatches each, pinned in gen/grid_*.json) each cut into nine tiles, made seamless, sized to 1024, with normal and roughness
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
GRID_B = os.path.join(DIR, "gen", "grid_b.png")      # the train's inside (gen/grid_b.json)

# cell -> name, periodic axes, relief, roughness base and spread, metres per tile, invert relief
TILES_B = {
    (0, 0): ("moquette", "", 1.2, 0.95, 0.03, 0.35, False),
    (0, 1): ("decora", "", 0.08, 0.35, 0.04, 1.6, False),
    (0, 2): ("floorwood", "y", 2.0, 0.55, 0.12, 0.6, True),
    (1, 0): ("grille", "y", 3.0, 0.60, 0.10, 0.45, True),
    (1, 1): ("ceiling", "", 0.15, 0.65, 0.04, 1.8, False),
    (1, 2): ("blind", "", 0.8, 0.90, 0.04, 0.5, False),
    (2, 0): ("varnish", "y", 1.0, 0.30, 0.10, 0.8, True),
    (2, 1): ("studs", "", 2.5, 0.70, 0.08, 0.5, False),
    (2, 2): ("greenpaint", "", 1.0, 0.50, 0.12, 0.8, False),
}
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


DEGLARE = {"decora", "floorwood"}
# laminate and painted sheet read as pitted stone when their specks keep the photo's contrast: keep a hint of them
SOFT = {"decora": 0.7, "ceiling": 0.45, "varnish": 0.25}


def soften(img, k):
    from scipy import ndimage
    a = np.asarray(img.convert("RGB")).astype(np.float32)
    flat = np.stack([ndimage.gaussian_filter(a[..., i], img.size[0] / 16) for i in range(3)], axis=2)
    return Image.fromarray(np.clip(a * (1 - k) + flat * k, 0, 255).astype(np.uint8))


def deglare(img):
    """Takes the photographed sheen out of a glossy swatch (a white streak or spot would repeat on every tile and stay
    put as the camera moves): pixels much brighter than their neighbourhood go to the neighbourhood's colour."""
    from scipy import ndimage
    a = np.asarray(img.convert("RGB")).astype(np.float32)
    lum = a.mean(axis=2)
    low = ndimage.median_filter(lum, size=max(9, img.size[0] // 5))
    k = np.clip((lum - low - 3.0) / 12.0, 0.0, 1.0)
    k = ndimage.gaussian_filter(k, 2.0)
    blur = np.stack([ndimage.median_filter(a[..., i], size=max(9, img.size[0] // 5)) for i in range(3)], axis=2)
    a = a * (1 - k[..., None]) + blur * k[..., None]
    # what's left is broad soft sheen: level the brightness against its neighbourhood (along the boards and grain
    # only, so each board keeps its own tone)
    lum = a.mean(axis=2)
    soft = ndimage.gaussian_filter(lum, (3.0, img.size[0] / 10))
    a = a * (ndimage.gaussian_filter(lum, img.size[0] / 3).mean() / np.maximum(soft, 1.0))[..., None] ** 0.9
    # the sheen also bleached the colour: give each pixel the swatch's own hue at its brightness (the grain stays)
    lum = a.mean(axis=2, keepdims=True)
    tint = np.median(a.reshape(-1, 3), axis=0)
    a = 0.35 * a + 0.65 * tint * lum / max(float(tint.mean()), 1.0)
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview")
    a = ap.parse_args()
    TT.DIR = DIR
    sheet = Image.new("RGB", (6 * 512, 3 * 512)) if a.preview else None
    meta = {}
    m = 6     # equal cells (the grid has no gutters): a few pixels in from each seam
    work = []
    for gi, (path, tiles) in enumerate(((GRID, TILES), (GRID_B, TILES_B))):
        grid = Image.open(path).convert("RGB")
        w, h = grid.size
        for r in range(3):
            for c in range(3):
                work.append((gi, r, c, grid.crop((c * w // 3 + m, r * h // 3 + m, (c + 1) * w // 3 - m, (r + 1) * h // 3 - m)), tiles[(r, c)]))
    for gi, r, c, img, spec in work:
        name, axes, strength, rough, spread, tile, invert = spec
        if name in DEGLARE:
            img = deglare(img)
        if name in SOFT:
            img = soften(img, SOFT[name])
        rgb = TT.make(name, img, axes, strength, rough, spread, invert)
        for k in "CNR":
            os.replace(os.path.join(DIR, "T_TR_%s_%s.png" % (name, k)), os.path.join(DIR, "T_RL_%s_%s.png" % (name, k)))
        meta[name] = {"tile_m": tile}
        if sheet:
            sheet.paste(Image.fromarray(np.tile(rgb, (2, 2, 1))).resize((512, 512)), ((gi * 3 + c) * 512, r * 512))
        print("texture", name)
    with open(os.path.join(DIR, "textures.json"), "w") as f:
        json.dump({"_doc": "Made by tools/railway_textures.py: the metres one tile of each texture covers.", "textures": meta}, f, indent=1)
    if sheet:
        sheet.save(a.preview, quality=88)


if __name__ == "__main__":
    main()
