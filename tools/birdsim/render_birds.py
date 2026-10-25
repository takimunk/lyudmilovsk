"""Debug maps of the bird life from above: where every bird is and what it does, hour by hour.

  python3 tools/birdsim/render_birds.py [day.csv] [out_dir]

Reads a day written by tools/birdsim/run.sh day (or by the game, lyud.Birds.DumpDay) and draws, on the
map's ground (Content/Birds/bird_world.bin): one panel per chosen hour with a dot per bird in its
species' colour (filled = on the ground or a perch, ring = asleep, tick = flying), the club's door, and
a chart of what the birds do and where over the whole day.
"""
import csv
import math
import os
import struct
import sys
from collections import Counter, defaultdict

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
GRID = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "BirdsData", "bird_world.bin")
HOURS = [5.0, 7.75, 9.0, 12.0, 14.5, 16.5, 17.25, 21.0]
MPP = 4.0           # metres per pixel

SPECIES_COLOURS = {
    "Rook": (20, 20, 30), "HoodedCrow": (120, 120, 130), "Jackdaw": (70, 70, 110), "Magpie": (40, 90, 160),
    "FeralPigeon": (150, 150, 200), "WoodPigeon": (110, 140, 190), "HerringGull": (255, 255, 255),
    "BlackHeadedGull": (255, 200, 200), "CommonGull": (200, 255, 255), "Cormorant": (0, 80, 60),
    "GreatTit": (230, 210, 30), "BlueTit": (60, 140, 255), "Goldfinch": (230, 40, 40), "Chaffinch": (220, 120, 90),
    "Bullfinch": (255, 60, 120), "HouseSparrow": (160, 110, 60), "TreeSparrow": (200, 140, 80), "Starling": (90, 40, 140),
    "SongThrush": (180, 140, 90), "Fieldfare": (210, 100, 30), "Blackbird": (0, 0, 0), "Robin": (255, 120, 0),
    "WhiteWagtail": (240, 240, 200), "Waxwing": (200, 150, 130), "CommonSwift": (90, 60, 40),
    "BarnSwallow": (30, 50, 140), "SandMartin": (170, 150, 120), "Sparrowhawk": (255, 0, 255), "Peregrine": (0, 255, 255),
}
AIR = {"Takeoff", "Fly", "Land", "Hawk", "Soar", "Flee", "Chase", "Mob", "Approach"}


def load_grid():
    with open(GRID, "rb") as f:
        magic, nx, ny = struct.unpack("<4sii", f.read(12))
        x0, y0, cell = struct.unpack("<fff", f.read(12))
        g = struct.unpack("<%dh" % (nx * ny), f.read(nx * ny * 2))
        t = struct.unpack("<%dh" % (nx * ny), f.read(nx * ny * 2))
    return nx, ny, x0, y0, cell, g, t


def background():
    nx, ny, x0, y0, cell, g, t = load_grid()
    w, h = int(nx * cell / MPP), int(ny * cell / MPP)
    img = Image.new("RGB", (w, h))
    pix = img.load()
    for j in range(h):
        gy = int((h - 1 - j) * MPP / cell)          # north up
        for i in range(w):
            gx = int((w - 1 - i) * MPP / cell)      # Unreal +X (west, the sea) on the left
            k = min(gy, ny - 1) * nx + min(gx, nx - 1)
            z, top = g[k] / 100.0, t[k] / 100.0
            if z <= -42.9:
                c = (58, 88, 128)
            elif z < -40.5:
                c = (214, 200, 160)
            elif top - z > 9:
                c = (58, 92, 60)                     # forest
            elif top - z > 3:
                c = (104, 140, 92)                   # trees between houses, copses
            else:
                c = None
            if c is None:
                v = max(0.0, min(1.0, (z + 43) / 70.0))
                c = (int(150 + 60 * v), int(170 + 40 * v), int(120 + 50 * v))
            pix[i, j] = c
    # the houses, from the map
    sys.path.insert(0, os.path.join(ROOT, "tools"))
    try:
        import world_map as W
        import bird_habitats as BH
        fy = BH.face_y_default() / 100.0
        d = ImageDraw.Draw(img)
        for x, y, yaw, hw, hd, top in BH.house_boxes():
            ca, sa = math.cos(yaw), math.sin(yaw)
            pts = []
            for u, v in ((-hw, -hd), (hw, -hd), (hw, hd), (-hw, hd)):
                pts.append(to_px(x + u * ca - v * sa, fy + y + u * sa + v * ca, (nx, ny, x0, y0, cell), img.size))
            d.polygon(pts, fill=(150, 70, 60))
    except Exception as e:  # noqa: BLE001
        print("render_birds: no houses drawn:", e)
    return img, (nx, ny, x0, y0, cell)


def to_px(x, y, frame, size):
    nx, ny, x0, y0, cell = frame
    w, h = size
    return (w - 1 - (x - x0) / MPP, h - 1 - (y - y0) / MPP)


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "Saved", "BirdSim", "day.csv")
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.dirname(src)
    rows = defaultdict(list)
    with open(src) as f:
        for r in csv.DictReader(f):
            rows[round(float(r["hour"]) * 4) / 4].append(r)
    bg, frame = background()
    font = ImageFont.load_default()
    panels = []
    for hour in HOURS:
        snap = rows.get(round(hour * 4) / 4)
        if not snap:
            continue
        img = bg.copy()
        d = ImageDraw.Draw(img)
        acts = Counter()
        for r in snap:
            x, y = float(r["x"]), float(r["y"])
            px, py = to_px(x, y, frame, img.size)
            col = SPECIES_COLOURS.get(r["species"], (255, 0, 0))
            act = r["act"]
            acts[act] += 1
            if act == "Sleep":
                d.ellipse((px - 2.5, py - 2.5, px + 2.5, py + 2.5), outline=col)
            elif act in AIR:
                d.line((px - 3, py + 2, px, py - 1, px + 3, py + 2), fill=col, width=1)
            else:
                d.ellipse((px - 2, py - 2, px + 2, py + 2), fill=col, outline=(0, 0, 0) if sum(col) > 500 else None)
        dx, dy = to_px(-1.93, 11.9, frame, img.size)
        d.rectangle((dx - 4, dy - 4, dx + 4, dy + 4), outline=(255, 0, 0), width=2)
        hh = int(hour)
        d.rectangle((0, 0, img.size[0], 34), fill=(0, 0, 0))
        d.text((6, 4), "%02d:%02d   %d birds" % (hh, int((hour - hh) * 60), len(snap)), fill=(255, 255, 255), font=font)
        d.text((6, 18), "  ".join("%s %d" % kv for kv in acts.most_common(7)), fill=(220, 220, 220), font=font)
        panels.append(img)
    if not panels:
        print("render_birds: no snapshots at the chosen hours in", src)
        return
    pw, ph = panels[0].size
    cols = 4
    rows_n = (len(panels) + cols - 1) // cols
    legend_h = 120
    sheet = Image.new("RGB", (pw * cols // 2, ph * rows_n // 2 + legend_h), (30, 30, 30))
    for k, p in enumerate(panels):
        sheet.paste(p.resize((pw // 2, ph // 2)), ((k % cols) * pw // 2, (k // cols) * ph // 2))
    d = ImageDraw.Draw(sheet)
    y0 = ph * rows_n // 2 + 6
    for k, (sp, col) in enumerate(SPECIES_COLOURS.items()):
        x = 8 + (k % 8) * (sheet.size[0] // 8)
        y = y0 + (k // 8) * 18
        d.ellipse((x, y + 3, x + 8, y + 11), fill=col, outline=(128, 128, 128))
        d.text((x + 12, y), sp, fill=(230, 230, 230), font=font)
    d.text((8, y0 + 84), "dot = on the ground or a perch, ring = asleep, tick = flying; red square = the club's door; west (the sea) on the left, north up",
           fill=(200, 200, 200), font=font)
    path = os.path.join(out, "birds_day_map.png")
    sheet.save(path)
    for k, p in enumerate(panels):
        p.save(os.path.join(out, "birds_%02d.png" % k))

    # what they do and where, every quarter hour
    hours = sorted(rows)
    groups = ["Sleep", "Perch", "Preen", "Forage", "Peck", "Swim", "Watch", "air"]
    cols_ = {"Sleep": (60, 60, 120), "Perch": (120, 160, 120), "Preen": (90, 130, 90), "Forage": (200, 170, 90),
             "Peck": (230, 200, 60), "Swim": (80, 140, 220), "Watch": (240, 80, 80), "air": (230, 230, 230)}
    W_, H_ = 1200, 360
    chart = Image.new("RGB", (W_, H_ + 40), (24, 24, 24))
    d = ImageDraw.Draw(chart)
    bw = W_ / max(len(hours), 1)
    for k, hr in enumerate(hours):
        c = Counter("air" if r["act"] in AIR else r["act"] for r in rows[hr])
        n = sum(c.values()) or 1
        y = H_
        for g_ in groups:
            v = c.get(g_, 0) / n * H_
            d.rectangle((k * bw, y - v, (k + 1) * bw, y), fill=cols_[g_])
            y -= v
        if abs(hr - round(hr)) < 0.01 and int(round(hr)) % 2 == 0:
            d.text((k * bw, H_ + 4), "%02d" % int(round(hr)), fill=(220, 220, 220), font=font)
    for k, g_ in enumerate(groups):
        d.rectangle((10 + k * 140, H_ + 22, 22 + k * 140, H_ + 34), fill=cols_[g_])
        d.text((26 + k * 140, H_ + 22), g_, fill=(220, 220, 220), font=font)
    chart.save(os.path.join(out, "birds_day_acts.png"))
    print("render_birds:", path, "and birds_day_acts.png,", len(panels), "panels")


if __name__ == "__main__":
    main()
