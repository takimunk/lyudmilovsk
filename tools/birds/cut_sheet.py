"""Cuts a bird reference sheet (columns = species, rows = front / left side / back on white) into hi3d input views.

    python3 tools/birds/cut_sheet.py <sheet.png> Rook HoodedCrow CarrionCrow Raven

Writes blender/birds/scan_raw/<Id>/views/{front,back,left,right}.png (right = the left profile mirrored: birds are
bilaterally symmetric, and a 4th view steadies hi3d's reconstruction). Each view is cropped to the bird's bounding box
(non-white pixels) with a margin, padded to a square on white, at most 2048 px.
"""
import os
import sys

import numpy as np
from scipy import ndimage
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
ROWS = ["front", "left", "back"]


def crop(cell):
    a = np.asarray(cell.convert("RGB")).astype(int)
    ink = (255 * 3 - a.sum(2)) > 60
    # keep the bird only: the biggest blob (grown a little so feathers and legs stay joined), dropping a neighbour's
    # wing tip that crossed into this cell
    lab, n = ndimage.label(ndimage.binary_dilation(ink, iterations=6))
    if n > 1:
        big = 1 + int(np.argmax(ndimage.sum(ink, lab, range(1, n + 1))))
        ink &= lab == big
        cell = Image.fromarray(np.where(ink[..., None], a, 255).astype(np.uint8))
    ys, xs = np.nonzero(ink)
    if len(xs) == 0:
        raise SystemExit("empty cell")
    x0, x1, y0, y1 = xs.min(), xs.max(), ys.min(), ys.max()
    edge = ink[0].any() or ink[-1].any() or ink[:, 0].any() or ink[:, -1].any()
    side = int(max(x1 - x0, y1 - y0) * 1.12)
    cx, cy = (x0 + x1) // 2, (y0 + y1) // 2
    out = Image.new("RGB", (side, side), (255, 255, 255))
    out.paste(cell.convert("RGB").crop((x0, y0, x1 + 1, y1 + 1)), (side // 2 - (cx - x0), side // 2 - (cy - y0)))
    if side > 2048:
        out = out.resize((2048, 2048), Image.LANCZOS)
    return out, edge


def main():
    sheet, ids = Image.open(sys.argv[1]), sys.argv[2:]
    W, H = sheet.size
    cw, ch = W / len(ids), H / len(ROWS)
    for c, sid in enumerate(ids):
        d = os.path.join(ROOT, "blender", "birds", "scan_raw", sid, "views")
        os.makedirs(d, exist_ok=True)
        for r, view in enumerate(ROWS):
            cell = sheet.crop((int(c * cw), int(r * ch), int((c + 1) * cw), int((r + 1) * ch)))
            im, edge = crop(cell)
            im.save(os.path.join(d, view + ".png"))
            if view == "left":
                im.transpose(Image.FLIP_LEFT_RIGHT).save(os.path.join(d, "right.png"))
            print(sid, view, im.size, "TOUCHES CELL EDGE" if edge else "")


if __name__ == "__main__":
    main()
