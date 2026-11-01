"""The diary's bird drawings (docs/diary.md, Birds): cuts the Higgsfield sheets of design/birds_diary/sheets.json (3 columns x 2
rows of pencil-and-watercolour birds on white paper) into one picture per cell, the white paper made transparent so the drawing
sits on the diary's own paper like ink and paint do.

    python3 tools/ui_diary_birds.py            downloads the sheets that are missing, cuts every one
    python3 tools/ui_diary_birds.py --check    also writes design/birds_diary/gen/contact.png, all the cells on one page

Writes textures/ui/birds/bird_<Species>[_f].png (512 x 512, the bird centred with a margin), the sheets to
design/birds_diary/gen/<id>.png (not committed: the sidecar sheets.json has their URLs and sha256). Then
unreal/scripts/import_diary_assets.py imports them as /Game/UI/Diary/Birds/T_Bird_<name>.

A new species (the big birds): add a sheet to sheets.json, generate it with Higgsfield (nano_banana_2_1, the same style
text), put its run in, rerun this.
"""
import hashlib
import json
import os
import sys
import urllib.request

import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SPEC = os.path.join(ROOT, "design", "birds_diary", "sheets.json")
GEN = os.path.join(ROOT, "design", "birds_diary", "gen")
OUT = os.path.join(ROOT, "textures", "ui", "birds")
SIZE = 512
COLS, ROWS = 3, 2


def fetch(sheet):
    path = os.path.join(GEN, sheet["id"] + ".png")
    if not os.path.exists(path):
        print("downloading", sheet["id"])
        urllib.request.urlretrieve(sheet["run"]["output_url"], path)
    return path


def paper_white(rgb):
    """The paper's own white at every pixel: some sheets come on a faintly tinted paper that darkens toward the corners (the gulls),
    so one white for the whole cell leaves a grey square. Blocks that are all paper (bright and even) are the samples, a smooth
    quadratic over the cell through them per channel."""
    h, w, _ = rgb.shape
    lum = rgb.mean(axis=2)
    top = np.percentile(lum, 99)
    n = 16
    ys, xs, vals = [], [], []
    for by in range(0, h - n + 1, n):
        for bx in range(0, w - n + 1, n):
            blk = rgb[by:by + n, bx:bx + n].reshape(-1, 3)
            bl = blk.mean(axis=1)
            if bl.min() > top - 22 and bl.std() < 2.5:
                ys.append((by + n / 2) / h - 0.5)
                xs.append((bx + n / 2) / w - 0.5)
                vals.append(np.percentile(blk, 60, axis=0))
    if len(vals) < 12:
        return np.broadcast_to(np.percentile(rgb.reshape(-1, 3), 99, axis=0), rgb.shape)
    ys, xs, vals = np.array(ys), np.array(xs), np.array(vals)
    basis = lambda x, y: np.stack([np.ones_like(x), x, y, x * x, x * y, y * y], axis=-1)
    # (numpy on Accelerate warns about overflow in any large matmul; the numbers are fine)
    with np.errstate(all="ignore"):
        coef, *_ = np.linalg.lstsq(basis(xs, ys), vals, rcond=None)
        gy, gx = np.mgrid[0:h, 0:w]
        paper = basis(gx / w - 0.5, gy / h - 0.5) @ coef
    return np.clip(paper, 1.0, 255.0)


def paper_to_alpha(im):
    """White paper -> transparent: what is darker than the paper is pigment, its strength the alpha (like multiplying the
    drawing onto another paper), the colour un-mixed from the white."""
    rgb = np.asarray(im.convert("RGB"), dtype=np.float64)
    white = paper_white(rgb)
    # how far below the paper, the most in any channel
    a = np.clip((1.0 - rgb / white).max(axis=2), 0.0, None)
    # the paper's own grain stays paper: a soft step instead of a hard cut (no speckle, no grey haze)
    a = np.clip((a - 0.03) / 0.97 * 1.15, 0.0, 1.0)
    a = np.where(a < 0.02, 0.0, a)
    # c = paper * (1 - a) + pigment * a  (on white paper scaled to 255)
    am = np.maximum(a, 1e-3)[..., None]
    col = (rgb / white * 255.0 - 255.0 * (1.0 - am)) / am
    col = np.clip(col, 0, 255)
    out = np.dstack([col, a[..., None] * 255.0]).round().astype(np.uint8)
    out[a == 0] = 0
    return Image.fromarray(out)


def drawings(rgb, cw, ch):
    """Which pixels belong to which cell's drawing. A bird may reach over its cell's edge (a tail, a wing tip, a bill), so
    cells are not cut on the grid: the pencil and paint are grouped into blots (strokes closer than ~1% of the sheet joined),
    each blot goes to the cell its middle is in."""
    h, w, _ = rgb.shape
    ink = ndimage.minimum_filter(rgb.mean(axis=2), size=5) < 215
    reach = max(3, int(min(cw, ch) * 0.012))
    blots, n = ndimage.label(ndimage.binary_dilation(ink, iterations=reach))
    owner = np.full(n + 1, -1)
    for i, sl in enumerate(ndimage.find_objects(blots), start=1):
        size = np.count_nonzero(blots[sl] == i)
        if size < (reach * 4) ** 2:
            continue  # a speck of dust
        cy, cx = ndimage.center_of_mass(blots[sl] == i)
        cy, cx = cy + sl[0].start, cx + sl[1].start
        owner[i] = min(int(cy // ch), ROWS - 1) * COLS + min(int(cx // cw), COLS - 1)
    return ink, owner[blots]


def cut(path, sheet):
    im = Image.open(path).convert("RGB")
    w, h = im.size
    cw, ch = w // COLS, h // ROWS
    rgb = np.asarray(im)
    ink, cell_of = drawings(rgb, cw, ch)
    # room around the sheet for a bird near its edge: the paper continued
    pad = max(cw, ch) // 2
    padded = np.pad(rgb, ((pad, pad), (pad, pad), (0, 0)), mode="reflect")
    names = sheet["cells"]
    done = []
    for i, name in enumerate(names):
        mine = cell_of == i
        ys, xs = np.nonzero(mine & ink)
        if not len(ys):
            print("  WARNING %s: nothing drawn in its cell" % name)
            continue
        box = (xs.min(), ys.min(), xs.max() + 1, ys.max() + 1)
        bw, bh = box[2] - box[0], box[3] - box[1]
        side = int(max(bw, bh) * 1.12)
        mx, my = (box[0] + box[2]) // 2, (box[1] + box[3]) // 2
        x0, y0 = mx - side // 2 + pad, my - side // 2 + pad
        sq = Image.fromarray(padded[y0:y0 + side, x0:x0 + side])
        # only this cell's drawing: a neighbour's twig inside the square is paper here
        own = np.zeros((side, side), dtype=bool)
        ox0, oy0 = max(0, mx - side // 2), max(0, my - side // 2)
        ox1, oy1 = min(w, mx - side // 2 + side), min(h, my - side // 2 + side)
        own[oy0 - (my - side // 2):oy1 - (my - side // 2), ox0 - (mx - side // 2):ox1 - (mx - side // 2)] = \
            mine[oy0:oy1, ox0:ox1]
        keep = Image.fromarray((own * 255).astype(np.uint8)).resize((SIZE, SIZE), Image.BILINEAR)
        rgba = paper_to_alpha(sq.resize((SIZE, SIZE), Image.LANCZOS))
        out = np.asarray(rgba).copy()
        out[..., 3] = (out[..., 3].astype(np.float64) * np.asarray(keep) / 255.0).round().astype(np.uint8)
        out[out[..., 3] == 0] = 0
        dst = os.path.join(OUT, "bird_%s.png" % name)
        Image.fromarray(out).save(dst, optimize=True)
        done.append((name, dst, (bw, bh)))
        print("  %-16s box %4dx%-4d -> %s" % (name, bw, bh, os.path.relpath(dst, ROOT)))
    return done


def main():
    spec = json.load(open(SPEC))
    os.makedirs(GEN, exist_ok=True)
    os.makedirs(OUT, exist_ok=True)
    cells = []
    for sheet in spec["sheets"]:
        if "run" not in sheet:
            print("no run for", sheet["id"], "- generate it first")
            continue
        path = fetch(sheet)
        sha = hashlib.sha256(open(path, "rb").read()).hexdigest()
        if sheet["run"].get("sha256") not in (None, sha):
            print("WARNING %s: sha256 differs from the sidecar" % sheet["id"])
        sheet["run"]["sha256"] = sha
        sheet["run"]["size"] = list(Image.open(path).size)
        print(sheet["id"])
        cells += cut(path, sheet)
    json.dump(spec, open(SPEC, "w"), ensure_ascii=False, indent=1)
    open(SPEC, "a").write("\n")
    if "--check" in sys.argv:
        n = len(cells)
        cols = 6
        rows = (n + cols - 1) // cols
        page = Image.new("RGB", (cols * 260, rows * 290), (243, 237, 222))
        from PIL import ImageDraw
        d = ImageDraw.Draw(page)
        for i, (name, dst, _) in enumerate(cells):
            im = Image.open(dst).resize((250, 250))
            page.paste(im, ((i % cols) * 260 + 5, (i // cols) * 290 + 5), im)
            d.text(((i % cols) * 260 + 8, (i // cols) * 290 + 262), name, fill=(40, 40, 60))
        page.save(os.path.join(GEN, "contact.png"))
        print("contact sheet:", os.path.relpath(os.path.join(GEN, "contact.png"), ROOT))


if __name__ == "__main__":
    main()
