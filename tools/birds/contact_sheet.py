"""OLD vs NEW contact sheets for all species: per species OLD side | NEW side | OLD flight top | NEW flight top.
OLD previews from a checkout that still has the procedural birds' exports (default ~/projects/lyudmilovsk-wt/birdskel),
NEW from blender/birds/export. Writes renders/birdmodels/contact_<n>.jpg (each under 300 KB).

    python3 tools/birds/contact_sheet.py [old_checkout] [Id ...]
"""
import json
import os
import sys

from PIL import Image, ImageDraw

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
args = sys.argv[1:]
OLD = args.pop(0) if args and os.path.isdir(args[0]) else os.path.expanduser("~/projects/lyudmilovsk-wt/birdskel")
ids = args or [s["id"] for s in json.load(open(os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "BirdsData", "species.json")))["species"]]
cw, ch, lab = 300, 234, 110
per = 10
for n in range(0, len(ids), per):
    grp = ids[n:n + per]
    sheet = Image.new("RGB", (lab + 4 * cw, ch * len(grp) + 24), "white")
    dr = ImageDraw.Draw(sheet)
    for c, t in enumerate(["OLD perch", "NEW perch", "OLD flight", "NEW flight"]):
        dr.text((lab + c * cw + 8, 6), t, fill="black")
    for r, sid in enumerate(grp):
        y = 24 + r * ch
        dr.text((6, y + ch // 2), sid, fill="black")
        for c, (base, view) in enumerate([(OLD, "side"), (ROOT, "side"), (OLD, "fly_top"), (ROOT, "fly_top")]):
            f = os.path.join(base, "blender", "birds", "export", sid, "previews", f"{sid}_{view}.png")
            if os.path.exists(f):
                im = Image.open(f).convert("RGB")
                im.thumbnail((cw, ch))
                sheet.paste(im, (lab + c * cw, y))
    out = os.path.join(ROOT, "renders", "birdmodels", f"contact_{n // per + 1}.jpg")
    q = 85
    while True:
        sheet.save(out, quality=q)
        if os.path.getsize(out) < 300_000 or q < 35:
            break
        q -= 7
    print(out, os.path.getsize(out))
