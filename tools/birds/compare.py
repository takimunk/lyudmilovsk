"""Old / A / B preview comparison sheet for one species (rows = variants, columns = views), JPEG < 300 KB.

    python3 tools/birds/compare.py Rook [out.jpg]
"""
import os
import sys

from PIL import Image, ImageDraw

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sid = sys.argv[1]
out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "renders", "birdmodels", f"{sid}_old_A_B.jpg")
views = ["side", "34", "head_close", "front", "fly_top", "glide_below"]
rows = [(v, os.path.join(ROOT, "blender", "birds", "scan_raw", sid, v, "previews")) for v in ("old", "new", "B")]
rows = [(n, d) for n, d in rows if os.path.isdir(d)]
cw, ch = 400, 311
sheet = Image.new("RGB", (cw * len(views) + 60, ch * len(rows)), "white")
dr = ImageDraw.Draw(sheet)
for r, (name, d) in enumerate(rows):
    dr.text((8, r * ch + ch // 2), {"old": "OLD", "new": "A", "B": "B"}[name], fill="black")
    for c, v in enumerate(views):
        f = os.path.join(d, f"{sid}_{v}.png")
        if os.path.exists(f):
            im = Image.open(f).convert("RGB")
            im.thumbnail((cw, ch))
            sheet.paste(im, (60 + c * cw, r * ch))
os.makedirs(os.path.dirname(out), exist_ok=True)
q = 85
while True:
    sheet.save(out, quality=q)
    if os.path.getsize(out) < 300_000 or q < 40:
        break
    q -= 8
print(out, os.path.getsize(out))
