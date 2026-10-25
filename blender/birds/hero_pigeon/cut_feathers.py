"""Cuts the feather scan sheets (gen/feathers_*.png, Higgsfield; see gen.json) into single feathers with alpha:
gen/cut/<sheet>_<row>_<n>.png, quill at the bottom, cropped to the feather, plus cut.json (sizes in px, row, order).

    python3 blender/birds/hero_pigeon/cut_feathers.py
"""
import json
import os

import numpy as np
from PIL import Image
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
GEN = os.path.join(HERE, "gen")
OUT = os.path.join(GEN, "cut")


def cut(sheet):
    im = np.asarray(Image.open(os.path.join(GEN, f"feathers_{sheet}.png")).convert("RGB")).astype(np.float32) / 255.0
    # white background: alpha from how far a pixel is from white (soft at the barb ends)
    dist = 1.0 - im.min(axis=2)
    sat = im.max(axis=2) - im.min(axis=2)
    a = np.clip((np.maximum(dist, sat * 2.0) - 0.035) / 0.10, 0, 1)
    solid = ndimage.binary_closing(a > 0.25, iterations=3)
    solid = ndimage.binary_fill_holes(solid)
    lab, n = ndimage.label(solid)
    objs = ndimage.find_objects(lab)
    items = []
    for i, sl in enumerate(objs):
        h, w = sl[0].stop - sl[0].start, sl[1].stop - sl[1].start
        if h * w < 20000:
            continue
        items.append((sl, i + 1))
    # rows by the vertical centre, then left to right
    items.sort(key=lambda t: (t[0][0].start + t[0][0].stop) / 2)
    rows, cur, last = [], [], None
    for sl, k in items:
        cy = (sl[0].start + sl[0].stop) / 2
        if last is not None and cy - last > 250:
            rows.append(cur)
            cur = []
        cur.append((sl, k))
        last = cy
    rows.append(cur)
    meta = []
    for r, row in enumerate(rows):
        row.sort(key=lambda t: t[0][1].start)
        for j, (sl, k) in enumerate(row):
            pad = 6
            y0, y1 = max(0, sl[0].start - pad), min(im.shape[0], sl[0].stop + pad)
            x0, x1 = max(0, sl[1].start - pad), min(im.shape[1], sl[1].stop + pad)
            mask = ndimage.binary_dilation(lab[y0:y1, x0:x1] == k, iterations=4)
            rgb = im[y0:y1, x0:x1]
            alpha = a[y0:y1, x0:x1] * mask
            # un-premultiply the white fringe: the colour at the soft edges is the feather's, not the paper's
            c = np.where(alpha[..., None] > 0.02, (rgb - (1 - alpha[..., None])) / np.maximum(alpha[..., None], 0.02), rgb)
            c = np.clip(c, 0, 1)
            # fill transparent pixels with the nearest feather colour (no white halo once mipmapped)
            idx = ndimage.distance_transform_edt(alpha < 0.3, return_distances=False, return_indices=True)
            c = c[idx[0], idx[1]]
            out = np.dstack([c, alpha]) * 255
            name = f"{sheet}_{r}_{j}"
            Image.fromarray(out.astype(np.uint8), "RGBA").save(os.path.join(OUT, name + ".png"))
            meta.append({"name": name, "row": r, "index": j, "w": int(x1 - x0), "h": int(y1 - y0)})
        print(sheet, "row", r, len(row))
    return meta


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    meta = {s: cut(s) for s in ("wing", "tail")}
    json.dump(meta, open(os.path.join(OUT, "cut.json"), "w"), indent=1)
