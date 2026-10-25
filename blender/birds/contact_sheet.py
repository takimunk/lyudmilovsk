"""Tiles PNGs into one sheet (no PIL needed): Blender -b --python blender/birds/contact_sheet.py -- out.png cols cell_w a.png b.png ...
Each image is scaled to cell_w wide (nearest), cells padded to the tallest in their row."""
import sys

import bpy
import numpy as np

argv = sys.argv[sys.argv.index("--") + 1:]
out, cols, cw = argv[0], int(argv[1]), int(argv[2])
imgs = []
for p in argv[3:]:
    im = bpy.data.images.load(p)
    w, h = im.size
    a = np.array(im.pixels[:], dtype=np.float32).reshape(h, w, 4)
    sh = max(1, int(round(h * cw / w)))
    ys = (np.arange(sh) * h / sh).astype(int)
    xs = (np.arange(cw) * w / cw).astype(int)
    imgs.append(a[ys][:, xs])
rows = [imgs[i:i + cols] for i in range(0, len(imgs), cols)]
row_h = [max(i.shape[0] for i in r) for r in rows]
H, W = sum(row_h), cols * cw
sheet = np.ones((H, W, 4), np.float32) * np.array([0.12, 0.12, 0.12, 1], np.float32)
y = H
for r, rh in zip(rows, row_h):
    y -= rh
    for j, im in enumerate(r):
        sheet[y + rh - im.shape[0]:y + rh, j * cw:(j + 1) * cw] = im
o = bpy.data.images.new("sheet", W, H, alpha=True)
o.pixels.foreach_set(sheet.ravel())
o.filepath_raw = out
o.file_format = "PNG"
o.save()
print("SHEET", out, W, H)
