"""The game's world map (the M page of the game screen): a painted picture of tools/world_map.py with no words on it,
and the places with their positions (the names are keys, place.<id>.name, drawn by the game in the current language).

  python3 tools/ui_map.py            writes textures/ui/map_world.jpg, textures/ui/binoculars_mask.png,
                                     unreal/Lyudmilovsk/Content/UI/map.json
then unreal/scripts/import_ui_assets.py imports the pictures as /Game/UI/T_WorldMap and /Game/UI/T_Binoculars.

Style: an old Soviet tourist map: cream paper, brown contours every 5 m (thicker every 25), green forest with tree dots,
blue sea with depth bands, a dotted path, the cable car as a line with towers, buildings as dark blocks.
"""
import json
import math
import os
import random
import sys
from multiprocessing import Pool

from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import world_map as W  # noqa: E402

PX = 1.25          # metres per pixel
MIX_CODE = {"old": 1, "dune": 2, "wet": 3}
CONTOUR = 5.0      # metres between contour lines
LAYOUT = os.path.join(ROOT, "blender", "export", "scene01_layout.json")


def _row(j):
    y = W.Y_MAX - (j + 0.5) * PX
    w = int((W.X_MAX - W.X_MIN) / PX)
    return [W.height(W.X_MAX - (i + 0.5) * PX, y) for i in range(w)]


def heights(w, h):
    with Pool() as pool:
        return pool.map(_row, range(h), chunksize=16)


def _beach_row(j):
    y = W.Y_MAX - (j + 0.5) * PX
    w = int((W.X_MAX - W.X_MIN) / PX)
    return [W.is_beach(W.X_MAX - (i + 0.5) * PX, y) for i in range(w)]


def _hash(ix, iy):
    n = (ix * 374761393 + iy * 668265263) & 0xFFFFFFFF
    n = ((n ^ (n >> 13)) * 1274126177) & 0xFFFFFFFF
    return (n & 0xFFFF) / 65535.0


def noise(x, y, cell):
    """Smooth value noise 0..1 with features about cell metres across."""
    fx, fy = x / cell, y / cell
    ix, iy = math.floor(fx), math.floor(fy)
    tx, ty = fx - ix, fy - iy
    tx, ty = tx * tx * (3 - 2 * tx), ty * ty * (3 - 2 * ty)
    a = _hash(ix, iy) + (_hash(ix + 1, iy) - _hash(ix, iy)) * tx
    b = _hash(ix, iy + 1) + (_hash(ix + 1, iy + 1) - _hash(ix, iy + 1)) * tx
    return a + (b - a) * ty


def lerp(a, b, t):
    return tuple(int(a[k] + (b[k] - a[k]) * t) for k in range(3))


def academ_footprints():
    """The three houses of the ensemble as map polygons (blender/academgorodok/plan.py frames and sizes)."""
    sys.path.insert(0, os.path.join(ROOT, "blender", "academgorodok"))
    try:
        import plan  # noqa: E402
    except Exception as e:  # the Blender build's modules may need bpy one day: then no houses on the map
        print("no academgorodok plan:", e)
        return []
    out = []
    for name, (cx, cy, yaw_deg, _mirror) in plan.FRAMES.items():
        L, D = (plan.N1_L, plan.N1_D) if name == "Novikova1" else (plan.S_L, plan.S_D)
        c, s = math.cos(math.radians(yaw_deg)), math.sin(math.radians(yaw_deg))
        pts = []
        for u, v in ((-L / 2, -D / 2), (L / 2, -D / 2), (L / 2, D / 2), (-L / 2, D / 2)):
            ex, ny = cx + u * c - v * s, cy + u * s + v * c      # ensemble x east, y north
            pts.append(W.academ_to_map(ex, ny))
        out.append(pts)
    return out


def paint(out_jpg):
    w = int((W.X_MAX - W.X_MIN) / PX)
    h = int((W.Y_MAX - W.Y_MIN) / PX)
    print("map %d x %d px at %.2f m/px" % (w, h, PX))
    H = heights(w, h)
    with Pool() as pool:
        B = pool.map(_beach_row, range(h), chunksize=16)

    def px(x, y):
        return (W.X_MAX - x) / PX, (W.Y_MAX - y) / PX

    # forest: the tiles, only where the ground is in their height band and not too steep
    tiles = W.forests()
    cell = 40.0
    grid = {}
    for t in tiles:
        x, y, hx, hy, mix, _count, _sp, z0, z1, _seed = t
        for gx in range(int((x - hx) // cell), int((x + hx) // cell) + 1):
            for gy in range(int((y - hy) // cell), int((y + hy) // cell) + 1):
                grid.setdefault((gx, gy), []).append(t)

    paper_low = (226, 228, 196)     # the valley: a greener cream
    paper_high = (236, 224, 192)    # the upland
    sea_shallow = (176, 204, 210)
    sea_deep = (118, 156, 180)
    beach = (238, 220, 168)
    forest = {"old": (160, 190, 132), "dune": (186, 204, 150), "wet": (170, 198, 150), "town": (196, 210, 160)}
    img = Image.new("RGB", (w, h))
    pix = img.load()
    cover_img = Image.new("L", (w, h), 0)
    cover = cover_img.load()
    mixes_img = Image.new("L", (w, h), 0)
    mixes = mixes_img.load()
    shades = {}
    trees = []
    rnd = random.Random(7)
    for j in range(h):
        y = W.Y_MAX - (j + 0.5) * PX
        row = H[j]
        up = H[j - 1] if j > 0 else row
        dn = H[j + 1] if j + 1 < h else row
        for i in range(w):
            z = row[i]
            if z < W.SEA_Z:
                d = W.SEA_Z - z
                c = lerp(sea_shallow, sea_deep, min(1.0, d / 10.0))
                if int(d / 3.0) != int((W.SEA_Z - row[i - 1 if i else i]) / 3.0):    # depth bands
                    c = lerp(c, sea_deep, 0.35)
                pix[i, j] = c
                continue
            if B[j][i]:
                pix[i, j] = beach
                continue
            # hillshade from the light in the north-west (upper left of the picture)
            zl, zr = row[i - 1] if i else z, row[i + 1] if i + 1 < w else z
            gx, gy = (zr - zl) / (2 * PX), (dn[i] - up[i]) / (2 * PX)
            slope = math.hypot(gx, gy)
            shade = max(0.78, min(1.08, 1.0 - 0.9 * (gx + gy) / math.sqrt(2)))
            t = max(0.0, min(1.0, (z - W.LOW_Z) / (W.TOP_Z - W.LOW_Z)))
            c = lerp(paper_low, paper_high, t)
            x = W.X_MAX - (i + 0.5) * PX
            for f in grid.get((int(x // cell), int(y // cell)), ()):
                fx, fy, hx, hy, mix, _c, _s, z0, z1, _sd = f
                if abs(x - fx) <= hx and abs(y - fy) <= hy and z0 <= z <= z1 and slope < 0.8:
                    if mix == "town":       # trees standing apart between the lots: rings on the paper only
                        if rnd.random() < 0.0006:
                            trees.append((i, j, mix))
                        break
                    cover[i, j] = 255
                    mixes[i, j] = MIX_CODE[mix]
                    break
            if slope > 0.9:          # the cliffs: brown hatching
                c = lerp(c, (170, 120, 80), min(0.6, (slope - 0.9) * 0.6))
            pix[i, j] = tuple(min(255, int(v * shade)) for v in c)
            shades[i, j] = shade

    # the forest: the tiles' coverage softened and broken by noise into a ragged edge and a blotchy canopy
    soft = cover_img.filter(ImageFilter.GaussianBlur(9)).load()
    names = {v: k for k, v in MIX_CODE.items()}
    for j in range(h):
        y = W.Y_MAX - (j + 0.5) * PX
        for i in range(w):
            code = mixes[i, j]
            if not code:
                continue
            x = W.X_MAX - (i + 0.5) * PX
            mix = names[code]
            n = noise(x, y, 22.0) * 0.65 + noise(x, y, 7.0) * 0.35
            k = soft[i, j] / 255.0 + (n - 0.5) * 0.6
            if mix != "old":
                k -= 0.25 * (1.0 - n)      # copses and dune scrub are patchy
            if k < 0.55:
                continue
            c = lerp(forest[mix], (128, 164, 104), max(0.0, n - 0.5) * 1.2)
            sh = shades.get((i, j), 1.0)
            pix[i, j] = tuple(min(255, int(v * sh)) for v in c)
            if rnd.random() < (0.004 if mix == "old" else 0.0025):
                trees.append((i, j, mix))

    # contours
    ov = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    op = ov.load()
    for j in range(1, h):
        row, up = H[j], H[j - 1]
        for i in range(1, w):
            z = row[i]
            if z < W.SEA_Z:
                continue
            k = math.floor(z / CONTOUR)
            if k != math.floor(row[i - 1] / CONTOUR) or k != math.floor(up[i] / CONTOUR):
                thick = k % 5 == 0
                op[i, j] = (140, 96, 58, 200 if thick else 110)
    img.paste(ov, (0, 0), ov)

    d = ImageDraw.Draw(img)
    # the shore line
    for j in range(h):
        row = H[j]
        for i in range(1, w):
            if (row[i] < W.SEA_Z) != (row[i - 1] < W.SEA_Z):
                d.point((i, j), fill=(70, 110, 140))
    # trees as small rings
    for i, j, mix in trees:
        col = (90, 128, 80) if mix == "old" else (110, 140, 90)
        d.ellipse((i - 3, j - 3, i + 3, j + 3), outline=col, width=1)
    # the promenade: a concrete strip along the valley beach
    pts = []
    for yy in range(int(W.PROMENADE_Y[0]), int(W.PROMENADE_Y[1]) + 1, 10):
        pts.append(px(W.shore_x(yy) - W.BEACH_W + 5.0, yy))
    d.line(pts, fill=(160, 150, 136), width=5)
    # the path from the door with its alley
    path = [px(x, y) for x, y in W.path_points(2.0)]
    for k in range(0, len(path) - 1, 2):
        d.line((path[k], path[k + 1]), fill=(120, 96, 70), width=3)
    for x, y, *_ in W.alley_trees():
        a, b = px(x, y)
        d.ellipse((a - 2.5, b - 2.5, a + 2.5, b + 2.5), fill=(84, 120, 70))
    # the cable car
    cc = W.cable_car()
    ends = [px(*W.STATION_UP), px(*W.STATION_DOWN)]
    d.line(ends, fill=(60, 50, 50), width=2)
    for tx, ty, *_ in cc["towers"]:
        a, b = px(tx, ty)
        d.rectangle((a - 3, b - 3, a + 3, b + 3), fill=(60, 50, 50))
    for sx, sy in (W.STATION_UP, W.STATION_DOWN):
        a, b = px(sx, sy)
        d.rectangle((a - 7, b - 5, a + 7, b + 5), fill=(150, 46, 36), outline=(60, 30, 20))
    # the club: the forecourt and the corridor under the upland, dashed
    a, b = px(W.DOOR_X, 0.0)
    d.rectangle((a - 6, b - 2, a + 6, b + 4), fill=(70, 60, 55))
    for yy in range(0, int(W.CORRIDOR_END_Y), -12):
        d.line((px(W.DOOR_X, yy), px(W.DOOR_X, yy - 6)), fill=(110, 90, 75), width=3)
    # the Academgorodok
    for poly in academ_footprints():
        d.polygon([px(x, y) for x, y in poly], fill=(176, 120, 70), outline=(70, 46, 30))
    img = img.filter(ImageFilter.SMOOTH)
    os.makedirs(os.path.dirname(out_jpg), exist_ok=True)
    img.save(out_jpg, quality=90)
    print("wrote", out_jpg)


def binoculars_mask(out_png, w=1920, h=1080):
    """Black around two round lenses side by side, soft edges, the lenses clear."""
    big = 2
    m = Image.new("L", (w // big, h // big), 255)
    d = ImageDraw.Draw(m)
    r = int(h / big * 0.47)
    cy = h // big // 2
    for cx in (w // big // 2 - int(r * 0.72), w // big // 2 + int(r * 0.72)):
        d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=0)
    m = m.filter(ImageFilter.GaussianBlur(10)).resize((w, h), Image.BILINEAR)
    img = Image.new("RGBA", (w, h), (0, 0, 0, 255))
    img.putalpha(m.point(lambda a: min(255, int(a * 1.0))))
    os.makedirs(os.path.dirname(out_png), exist_ok=True)
    img.save(out_png)
    print("wrote", out_png)


def places(out_json):
    """Where the names go. r: found by coming that close (0: never by walking); known: on the map from the start."""
    portal_face_y = 790.0
    if os.path.exists(LAYOUT):
        with open(LAYOUT) as f:
            portal_face_y = json.load(f).get("outside", {}).get("portal_face_y", portal_face_y)
    beach_x = next(x for x in range(0, 400) if W.is_beach(x, 60.0)) + W.BEACH_W / 2
    ax, ay, _ = W.academ()
    path = W.path_points(3.0)
    mid = path[len(path) // 2]
    P = [
        # id, x, y, r, kind, known
        ("club", W.DOOR_X, -110.0, 0.0, "building", True),
        ("club_door", W.DOOR_X, 6.0, 7.0, "building", True),
        ("alley", mid[0], mid[1], 18.0, "nature", False),
        ("cable_upper", W.STATION_UP[0], W.STATION_UP[1], 16.0, "transport", False),
        ("cable_lower", W.STATION_DOWN[0], W.STATION_DOWN[1], 20.0, "transport", False),
        ("academ", ax, ay, 70.0, "building", False),
        ("town", W.TOWN_CENTER[0], W.TOWN_CENTER[1], 260.0, "town", False),
        ("promenade", W.shore_x(850.0) - W.BEACH_W + 5.0, 850.0, 40.0, "nature", False),
        ("beach", beach_x, 60.0, 25.0, "nature", False),
        ("starlings", W.starlings()[0], W.starlings()[1], 150.0, "nature", False),
        ("valley", -620.0, 560.0, 0.0, "land", True),
        ("upland", -300.0, -450.0, 0.0, "land", True),
        ("north_hills", -400.0, 1850.0, 0.0, "land", True),
        ("sea", 900.0, 1200.0, 0.0, "water", True),
    ]
    data = {
        "comment": "Written by tools/ui_map.py from tools/world_map.py: do not edit by hand. Metres in the map frame (docs/world_map.md).",
        "frame": {"x_min": W.X_MIN, "x_max": W.X_MAX, "y_min": W.Y_MIN, "y_max": W.Y_MAX, "portal_face_y": portal_face_y,
                  "metres_per_pixel": PX},
        "places": [{"id": i, "x": round(x, 1), "y": round(y, 1), "r": r, "kind": k, "known": kn} for i, x, y, r, k, kn in P],
    }
    with open(out_json, "w") as f:
        json.dump(data, f, indent=1, ensure_ascii=False)
        f.write("\n")
    print("wrote", out_json)


if __name__ == "__main__":
    places(os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "UI", "map.json"))
    binoculars_mask(os.path.join(ROOT, "textures", "ui", "binoculars_mask.png"))
    if "--places" not in sys.argv:
        paint(os.path.join(ROOT, "textures", "ui", "map_world.jpg"))
