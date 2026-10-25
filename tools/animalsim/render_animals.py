"""Debug maps of the animal life from above.

  python3 tools/animalsim/render_animals.py [day.csv] [out_dir]

Draws the walk grid (Content/AnimalsData/animal_world.bin: grey blocked, green grass, light paved, tan yard,
sand, dark green forest), the places (animal_places.json) and, with a day CSV from tools/animalsim/run.sh day
(or lyud.Animals.DumpDay), a panel per chosen hour with a dot per animal (cats orange rings, dogs brown
squares; filled = awake) and a chart of what they do over the day.
"""
import csv
import json
import os
import struct
import sys
from collections import Counter, defaultdict

from PIL import Image, ImageDraw

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
DATA = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "AnimalsData")
COL = {0: (90, 90, 95), 1: (120, 160, 90), 2: (200, 200, 190), 3: (185, 170, 130), 4: (230, 215, 160), 5: (55, 95, 60)}
PCOL = {"Doorstep": (120, 60, 200), "ShopDoor": (255, 0, 0), "Bins": (160, 40, 40), "WarmHatch": (255, 120, 0),
        "Bench": (60, 60, 200), "SunnyWall": (255, 230, 0), "Den": (100, 50, 20), "Square": (0, 0, 0),
        "Beach": (0, 150, 220), "ClubDoor": (255, 0, 255), "Station": (0, 0, 0), "Feeder": (255, 60, 160),
        "Basement": (80, 80, 80), "Lookout": (0, 200, 200)}


def load_grid():
    with open(os.path.join(DATA, "animal_world.bin"), "rb") as f:
        tag, nx, ny = struct.unpack("<4sii", f.read(12))
        x0, y0, cell = struct.unpack("<fff", f.read(12))
        f.read(nx * ny * 2)
        kind = f.read(nx * ny)
    return nx, ny, x0, y0, cell, kind


def base_image(scale=1):
    nx, ny, x0, y0, cell, kind = load_grid()
    im = Image.new("RGB", (nx, ny))
    px = im.load()
    for j in range(ny):
        for i in range(nx):
            c = kind[j * nx + i]
            r, g, b = COL.get(c & 7, (255, 0, 0))
            if c & 8:
                r, g, b = int(r * 0.8), int(g * 0.8), int(b * 0.8)
            px[nx - 1 - i, ny - 1 - j] = (r, g, b)      # map X is west: east on the right, north up
    if scale != 1:
        im = im.resize((nx * scale, ny * scale), Image.NEAREST)

    def to_px(x, y):
        return ((nx - 1 - (x - x0) / cell) * scale, (ny - 1 - (y - y0) / cell) * scale)
    return im, to_px


def draw_places(im, to_px, r=2):
    d = ImageDraw.Draw(im)
    with open(os.path.join(DATA, "animal_places.json")) as f:
        doc = json.load(f)
    kinds = doc["places_kinds"]
    for p in doc["places"]:
        x, y = to_px(p[1], p[2])
        d.ellipse([x - r, y - r, x + r, y + r], fill=PCOL.get(kinds[p[0]], (255, 255, 255)))


def main():
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "Saved", "AnimalSim")
    os.makedirs(out, exist_ok=True)
    im, to_px = base_image(1)
    draw_places(im, to_px)
    im.save(os.path.join(out, "animal_map.png"))
    csv_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(out, "day.csv")
    if not os.path.exists(csv_path):
        print("map ->", os.path.join(out, "animal_map.png"))
        return
    rows = defaultdict(list)
    acts = defaultdict(Counter)
    with open(csv_path) as f:
        for r in csv.DictReader(f):
            h = float(r["hour"])
            rows[round(h, 2)].append(r)
            acts[int(h)][r["kind"] + ":" + r["act"]] += 1
    hours = sorted(rows)
    pick = [h for h in hours if abs(h - round(h)) < 1e-3 and int(round(h)) % 3 == 0][:8]
    panels = []
    for h in pick:
        im2 = im.copy()
        d = ImageDraw.Draw(im2)
        for r in rows[h]:
            x, y = to_px(float(r["x"]), float(r["y"]))
            asleep = r["act"] in ("Sleep", "Rest")
            if r["kind"] == "Cat":
                c = (255, 140, 0)
                d.ellipse([x - 3, y - 3, x + 3, y + 3], outline=c, fill=None if asleep else c)
            else:
                c = (110, 60, 20)
                d.rectangle([x - 3, y - 3, x + 3, y + 3], outline=c, fill=None if asleep else c)
        d.text((8, 8), "%05.2f h" % h, fill=(0, 0, 0))
        panels.append(im2)
    if panels:
        w, hgt = panels[0].size
        sheet = Image.new("RGB", (w * 4, hgt * ((len(panels) + 3) // 4)), (255, 255, 255))
        for k, p in enumerate(panels):
            sheet.paste(p, ((k % 4) * w, (k // 4) * hgt))
        sheet.save(os.path.join(out, "animals_day_map.png"))
    # what they do, hour by hour
    keys = sorted({k for c in acts.values() for k in c})
    with open(os.path.join(out, "animals_day_acts.txt"), "w") as f:
        for hh in sorted(acts):
            tot = sum(acts[hh].values())
            f.write("%02d " % hh + " ".join("%s=%d%%" % (k, 100 * acts[hh][k] // tot) for k in keys if acts[hh][k]) + "\n")
    print("->", out)


if __name__ == "__main__":
    main()
