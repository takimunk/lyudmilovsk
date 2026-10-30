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

from PIL import Image, ImageFilter

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


def paper_to_alpha(im):
    """White paper -> transparent: what is darker than the paper is pigment, its strength the alpha (like multiplying the
    drawing onto another paper), the colour un-mixed from the white."""
    im = im.convert("RGB")
    px = im.load()
    w, h = im.size
    # the paper's own white: the brightest tenth of the border
    border = [px[x, y] for x in range(0, w, 7) for y in (0, h - 1)] + [px[x, y] for y in range(0, h, 7) for x in (0, w - 1)]
    border.sort(key=sum)
    top = border[len(border) * 9 // 10:]
    white = tuple(sum(c[i] for c in top) / len(top) for i in range(3))
    out = Image.new("RGBA", (w, h))
    op = out.load()
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            # how far below the paper, the most in any channel
            a = max(1.0 - r / max(white[0], 1), 1.0 - g / max(white[1], 1), 1.0 - b / max(white[2], 1), 0.0)
            a = min(1.0, a * 1.15)
            if a < 0.035:
                op[x, y] = (0, 0, 0, 0)
                continue
            # c = paper * (1 - a) + pigment * a  (on white paper scaled to 255)
            col = []
            for c, wch in ((r, white[0]), (g, white[1]), (b, white[2])):
                v = (c / max(wch, 1) * 255.0 - 255.0 * (1.0 - a)) / a
                col.append(int(max(0, min(255, v))))
            op[x, y] = (col[0], col[1], col[2], int(a * 255))
    return out


def cut(path, sheet):
    im = Image.open(path).convert("RGB")
    w, h = im.size
    cw, ch = w // COLS, h // ROWS
    names = sheet["cells"]
    done = []
    for i, name in enumerate(names):
        cx, cy = i % COLS, i // COLS
        # a little in from the cell's edges: the next cell's twig may just reach over
        inset = int(min(cw, ch) * 0.015)
        cell = im.crop((cx * cw + inset, cy * ch + inset, (cx + 1) * cw - inset, (cy + 1) * ch - inset))
        # the drawing's box: what is clearly darker than the paper
        grey = cell.convert("L").filter(ImageFilter.MinFilter(5))
        mask = grey.point(lambda v: 255 if v < 215 else 0)
        box = mask.getbbox() or (0, 0, cell.size[0], cell.size[1])
        bw, bh = box[2] - box[0], box[3] - box[1]
        side = int(max(bw, bh) * 1.12)
        mx, my = (box[0] + box[2]) // 2, (box[1] + box[3]) // 2
        sq = Image.new("RGB", (side, side), (255, 255, 255))
        sq.paste(cell.crop((mx - side // 2, my - side // 2, mx - side // 2 + side, my - side // 2 + side)), (0, 0))
        # anything of the crop that fell outside the cell is white paper (the crop fills it with black)
        region = Image.new("L", cell.size, 255)
        rmask = Image.new("L", (side, side), 0)
        rmask.paste(region, (side // 2 - mx, side // 2 - my))
        sq = Image.composite(sq, Image.new("RGB", (side, side), (255, 255, 255)), rmask)
        sq = sq.resize((SIZE, SIZE), Image.LANCZOS)
        rgba = paper_to_alpha(sq)
        dst = os.path.join(OUT, "bird_%s.png" % name)
        rgba.save(dst, optimize=True)
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
