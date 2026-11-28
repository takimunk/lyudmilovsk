"""Comparison sheet for one species: OLD bird / RAW hi3d scan / NEW (scan rig) from the same preview cameras.
JPEG under 300 KB in renders/birdmodels/<Id>_old_raw_new.jpg.

    python3 tools/birds/compare.py Rook [out.jpg]
"""
import os
import sys

from PIL import Image, ImageDraw

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sid = sys.argv[1]
out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "renders", "birdmodels", f"{sid}_old_raw_new.jpg")
base = os.path.join(ROOT, "blender", "birds", "scan_raw", sid)
views = ["side", "34", "front", "back", "fly_top", "glide_below", "head_close"]
rows = [("OLD", os.path.join(base, "old", "previews"), "{s}_{v}"),
        ("RAW hi3d", os.path.join(base, "S", "previews"), "{s}_raw_{v}"),
        ("NEW", os.path.join(base, "S", "previews"), "{s}_{v}")]
cw, ch = 360, 280
sheet = Image.new("RGB", (cw * len(views) + 90, ch * len(rows)), "white")
dr = ImageDraw.Draw(sheet)
for r, (name, d, pat) in enumerate(rows):
    dr.text((8, r * ch + ch // 2), name, fill="black")
    for c, v in enumerate(views):
        f = os.path.join(d, pat.format(s=sid, v=v) + ".png")
        if os.path.exists(f):
            im = Image.open(f).convert("RGB")
            im.thumbnail((cw, ch))
            sheet.paste(im, (90 + c * cw, r * ch))
os.makedirs(os.path.dirname(out), exist_ok=True)
q = 88
while True:
    sheet.save(out, quality=q)
    if os.path.getsize(out) < 300_000 or q < 40:
        break
    q -= 8
print(out, os.path.getsize(out))
