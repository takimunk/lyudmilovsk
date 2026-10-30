"""Where the town's cats and dogs can walk and what they go to, for the animal life simulation
(Private/Animals, docs/animals.md). Reads the map (tools/world_map.py), the built ground (tools/terrain.py
ground_z) and the Academgorodok's layout (blender/export/academgorodok/layout.json), and writes

  unreal/Lyudmilovsk/Content/AnimalsData/animal_world.bin    2 m grid over the lived-in part of the map:
                                                             ground height (cm) and what the ground is
  unreal/Lyudmilovsk/Content/AnimalsData/animal_places.json  places: doorsteps, the shop's back door, bins,
                                                             warm hatches, benches, sunny walls, dens...

  python3 tools/animal_map.py [--face-y CM]

Frame: Unreal metres (UE cm / 100), like the birds' habitats: X = map X, Y = portal face + map Y.

Ground kinds (the low 3 bits of a cell; bit 0x08 = cover: bushes, a hedge, under trees, round a house's base):
  0 blocked (a house, water, a wall, ground too steep), 1 grass and open ground, 2 paved (streets, the
  square, paths, the promenade), 3 yard (round the houses), 4 sand (beach), 5 forest floor.
Ground steeper than STEEP is blocked; between ROUGH and STEEP it is walkable but costs more.
"""
import argparse
import json
import math
import os
import random
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import world_map as W  # noqa: E402
import terrain as T  # noqa: E402

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
OUT_DIR = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "AnimalsData")
ACADEM_LAYOUT = os.path.join(ROOT, "blender", "export", "academgorodok", "layout.json")

CELL = 2.0
# the lived-in part: the town and the Academgorodok in the valley, its beach and promenade, the spur
# in front of the club's door (an island for animals: its sides are too steep, the cable car links it)
X0, X1 = -960.0, 290.0
Y0, Y1 = -40.0, 1900.0
STEEP = 0.80        # rise over run: ~39 degrees
ROUGH = 0.45

BLOCKED, GRASS, PAVED, YARD, SAND, FOREST = 0, 1, 2, 3, 4, 5
COVER = 0x08

# place kinds; LyudAnimals::EPlace (AnimalSim.h) has the same order
PLACES = ["Doorstep", "ShopDoor", "Bins", "WarmHatch", "Bench", "SunnyWall", "Den", "Square", "Beach",
          "ClubDoor", "Station", "Feeder", "Basement", "Lookout", "Refuge"]

# Academgorodok houses (plan.FRAMES and sizes): centre (ensemble x, y), u-axis yaw (deg), length, depth
ACADEM_HOUSES = {
    "Novikova1": ((0.0, 0.0), 93.0, 60.7, 14.5),
    "Novikova3": ((-12.83 - 2.05, 76.14 - 5.08), -34.6, 63.3, 19.8),
    "Maksimova8": ((-5.86 - 2.05, -68.74 - 5.08), -138.2, 63.3, 19.8),
    "Gastronom": ((-42.99, -22.1), 93.0, 21.45, 9.25),      # the grocery pavilion (blender/academgorodok/grocery.py footprint())
}


def face_y_default():
    try:
        with open(os.path.join(ROOT, "blender", "export", "scene01_layout.json")) as f:
            return json.load(f)["outside"]["portal_face_y"]
    except (OSError, KeyError):
        return 790.0


def town_boxes():
    """(x, y, yaw, half w, half d, kind, floors) of every town building, map metres."""
    out = []
    for x, y, yaw, w, d, fl, kind in W.town():
        out.append((x, y, yaw, w / 2 + (14 if kind == "kirche" else 0), d / 2, kind, fl))
    return out


def academ_boxes():
    out = []
    for name, ((cx, cy), yaw_deg, L, D) in ACADEM_HOUSES.items():
        mx, my = W.academ_to_map(cx, cy)
        # the ensemble's x axis (east) -> map (sin a, -cos a): a direction's map angle is atan2 of that
        a = math.radians(yaw_deg)
        ex, ey = math.cos(a), math.sin(a)               # u axis in the ensemble frame
        ux, uy = W.academ_to_map(cx + ex, cy + ey)
        out.append((mx, my, math.atan2(uy - my, ux - mx), L / 2, D / 2, "academ:" + name, 4))
    return out


def in_box(x, y, b, grow=0.0):
    bx, by, yaw, hw, hd = b[:5]
    ca, sa = math.cos(yaw), math.sin(yaw)
    u = (x - bx) * ca + (y - by) * sa
    v = -(x - bx) * sa + (y - by) * ca
    return abs(u) <= hw + grow and abs(v) <= hd + grow


def box_point(b, u, v):
    bx, by, yaw = b[:3]
    ca, sa = math.cos(yaw), math.sin(yaw)
    return bx + u * ca - v * sa, by + u * sa + v * ca


def on_street(x, y):
    """The town's street grid (12 m streets between 90 x 75 m blocks) and its square."""
    cx, cy = W.TOWN_CENTER
    ca, sa = math.cos(W.TOWN_ROT), math.sin(W.TOWN_ROT)
    u = (x - cx) * ca + (y - cy) * sa
    v = -(x - cx) * sa + (y - cy) * ca
    if math.hypot(x - cx, y - cy) > W.TOWN_R + 40:
        return False
    if -90 <= u <= 90 and -75 <= v <= 75:
        return True
    du = (u + 6.0) % 90.0
    dv = (v + 6.0) % 75.0
    return du < 12.0 or dv < 12.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--face-y", type=float, default=None, help="portal face, Unreal Y cm")
    ap.add_argument("--out", default=OUT_DIR)
    a = ap.parse_args()
    face_y = a.face_y if a.face_y is not None else face_y_default()
    fy = face_y / 100.0
    rnd = random.Random(1961)
    boxes = town_boxes() + academ_boxes()
    tiles = W.forests()

    nx = int((X1 - X0) / CELL) + 1
    ny = int((Y1 - Y0) / CELL) + 1
    z = [0.0] * (nx * ny)
    for j in range(ny):
        y = Y0 + j * CELL
        for i in range(nx):
            z[j * nx + i] = T.ground_z(X0 + i * CELL, y)
    kind = bytearray(nx * ny)
    path = W.path_points(1.0)
    cc = W.cable_car()
    for j in range(ny):
        y = Y0 + j * CELL
        for i in range(nx):
            x = X0 + i * CELL
            k = j * nx + i
            g = z[k]
            if g < W.SEA_Z + 0.15 or W.inland(x, y) < 1.0:
                continue                                  # the sea
            i0, i1 = max(i - 1, 0), min(i + 1, nx - 1)
            j0, j1 = max(j - 1, 0), min(j + 1, ny - 1)
            sx = (z[j * nx + i1] - z[j * nx + i0]) / ((i1 - i0) * CELL)
            sy = (z[j1 * nx + i] - z[j0 * nx + i]) / ((j1 - j0) * CELL)
            s = math.hypot(sx, sy)
            if s > STEEP:
                continue
            c = GRASS
            if W.is_beach(x, y):
                c = SAND
            in_town = math.hypot(x - W.TOWN_CENTER[0], y - W.TOWN_CENTER[1]) < W.TOWN_R + 30 or W.academ_flat(x, y) > 0.0
            for tx, ty, hx, hy, mix, *_ in () if in_town else tiles:
                if abs(x - tx) <= hx and abs(y - ty) <= hy and mix in ("old", "wet") and W.inland(x, y) > W.BEACH_W + 3:
                    c = FOREST | COVER
                    break
            if on_street(x, y):
                c = PAVED
            if W.PROMENADE_Y[0] < y < W.PROMENADE_Y[1] and abs(W.inland(x, y) - (W.BEACH_W - 5.0)) < 3.0:
                c = PAVED
            if 0.0 < y < W.PATH_LEN + 2:
                px = min(path, key=lambda p: abs(p[1] - y))[0]
                if abs(x - px) < 1.8:
                    c = PAVED
            if -2.0 < y < 7.0 and abs(x - W.DOOR_X) < 6.0:
                c = PAVED                                 # the forecourt
            if W.academ_flat(x, y) >= 1.0:
                lx, ly = W.academ_local(x, y)
                c = PAVED if lx > 18 and (abs(ly) < 4 or abs(ly - 25.5) > 13 or lx > 78) else YARD
            kind[k] = c
    # buildings: blocked, with a yard round them (the base of a wall is cover for a cat)
    for b in boxes:
        bx, by = b[0], b[1]
        r = math.hypot(b[3], b[4]) + 12
        for j in range(max(0, int((by - r - Y0) / CELL)), min(ny, int((by + r - Y0) / CELL) + 2)):
            for i in range(max(0, int((bx - r - X0) / CELL)), min(nx, int((bx + r - X0) / CELL) + 2)):
                x, y = X0 + i * CELL, Y0 + j * CELL
                k = j * nx + i
                if in_box(x, y, b, 0.6):
                    kind[k] = BLOCKED
                elif kind[k] != BLOCKED and in_box(x, y, b, 1.6):
                    kind[k] = (YARD if (kind[k] & 7) not in (PAVED,) else PAVED) | COVER
                elif (kind[k] & 7) == GRASS and in_box(x, y, b, 9.0):
                    kind[k] = YARD
    # the cable car's stations and towers
    for (sx, sy) in (W.STATION_UP, W.STATION_DOWN):
        for j in range(int((sy - 7 - Y0) / CELL), int((sy + 7 - Y0) / CELL) + 1):
            for i in range(int((sx - 5 - X0) / CELL), int((sx + 5 - X0) / CELL) + 1):
                if 0 <= i < nx and 0 <= j < ny:
                    kind[j * nx + i] = BLOCKED
    for tx, ty in cc["towers"]:
        i, j = int(round((tx - X0) / CELL)), int(round((ty - Y0) / CELL))
        if 0 <= i < nx and 0 <= j < ny:
            kind[j * nx + i] = BLOCKED

    def walk(x, y):
        i, j = int(round((x - X0) / CELL)), int(round((y - Y0) / CELL))
        return 0 <= i < nx and 0 <= j < ny and (kind[j * nx + i] & 7) != BLOCKED

    def gz(x, y):
        return T.ground_z(x, y)

    places = []

    def add(kindname, x, y, yaw=0.0, cap=2, sun=0.0, warm=0.0, food=0.0, tag="", height=0.0, need_walk=True):
        if need_walk and not walk(x, y):
            return False
        places.append([PLACES.index(kindname), round(x, 2), round(fy + y, 2), round(gz(x, y), 2), round(yaw, 3),
                       cap, round(sun, 2), round(warm, 2), round(food, 2), round(height, 2), tag])
        return True

    # doorsteps: the street side of every house (one door a house, two on the long blocks), facing out
    for b in boxes:
        if b[5] == "kirche":
            continue
        best = None
        for (u, v, n) in ((0, b[4] + 1.4, (0, 1)), (0, -b[4] - 1.4, (0, -1)), (b[3] + 1.4, 0, (1, 0)), (-b[3] - 1.4, 0, (-1, 0))):
            x, y = box_point(b, u, v)
            if not walk(x, y):
                continue
            ox, oy = box_point(b, u * 1.8 + n[0] * 6, v * 1.8 + n[1] * 6)
            score = (2.0 if on_street(ox, oy) else 0.0) + (1.0 if abs(v) > 0 else 0.0) + rnd.random() * 0.3
            if best is None or score > best[0]:
                yaw = b[2] + math.atan2(n[1], n[0])
                best = (score, x, y, yaw, u, v)
        if best is None:
            continue
        _, x, y, yaw, u, v = best
        # southern walls get the sun (map Y is north): sun 0..1 by how much the door faces south
        sun = max(0.0, -math.sin(yaw)) * 0.8 + 0.2
        if b[5] == "block":
            for t in (-0.5, 0.5):
                x2, y2 = box_point(b, b[3] * t if v != 0 else u, v)
                add("Doorstep", x2, y2, yaw, 3, sun, 0.2, 0.0, b[5])
            # flats over a heated basement: a cellar window cats go in and out of
            bx2, by2 = box_point(b, b[3] * 0.8, -v if v != 0 else b[4] + 1.2)
            add("Basement", bx2, by2, yaw + math.pi, 4, 0.0, 0.8, 0.0, "block")
        else:
            add("Doorstep", x, y, yaw, 2, sun, 0.1, 0.0, b[5])
    # the Academgorodok: a doorstep at every stair, the Gastronom's back door, benches on the square
    try:
        with open(ACADEM_LAYOUT) as f:
            lay = json.load(f)
    except OSError:
        lay = {"walks": []}
    ab = {b[5].split(":")[1]: b for b in boxes if b[5].startswith("academ:")}
    for wk in lay.get("walks", []):
        if not wk["label"].startswith("stair"):
            continue
        b = ab.get(wk["house"])
        if not b:
            continue
        p = W.academ_to_map(wk["points"][0][0], wk["points"][0][1])
        # out through the nearest facade, 1.3 m clear of it
        ca, sa = math.cos(b[2]), math.sin(b[2])
        u = (p[0] - b[0]) * ca + (p[1] - b[1]) * sa
        v = -(p[0] - b[0]) * sa + (p[1] - b[1]) * ca
        if b[3] - abs(u) < b[4] - abs(v):
            u = math.copysign(b[3] + 1.3, u)
            n = (math.copysign(1, u), 0)
        else:
            v = math.copysign(b[4] + 1.3, v)
            n = (0, math.copysign(1, v))
        x, y = box_point(b, u, v)
        yaw = b[2] + math.atan2(n[1], n[0])
        add("Doorstep", x, y, yaw, 3, max(0.0, -math.sin(yaw)) * 0.8 + 0.2, 0.3, 0.0, "academ")
    n1 = ab.get("Novikova1")
    gs = ab.get("Gastronom")
    if gs:
        # the grocery pavilion (grocery.py): its doors look east at Novikova 1's back (house -y, the box's +v: the map
        # is mirrored); people go in by the hall door (house x 4.35, u -3.4 from the box centre); the bins behind it
        x, y = box_point(gs, -3.4, gs[4] + 1.5)
        add("ShopDoor", x, y, gs[2] + math.pi / 2, 4, 0.2, 0.2, 1.0, "gastronom")
        x, y = box_point(gs, 4.0, -(gs[4] + 2.5))
        add("Bins", x, y, 0.0, 4, 0.0, 0.0, 0.7, "gastronom")
    if n1:
        x, y = box_point(n1, -12.0, n1[4] + 3.0)
        add("Feeder", x, y, 0.0, 6, 0.4, 0.0, 1.0, "academ")       # the lady from the second stair feeds the cats
        x, y = box_point(n1, -26.0, n1[4] + 2.0)
        add("WarmHatch", x, y, 0.0, 4, 0.2, 1.0, 0.0, "academ")
    for ly in (-6.0, 6.0, 19.0, -19.0):
        x, y = W.academ_to_map(30.0, ly)
        add("Bench", x, y, 0.0, 2, 0.6, 0.0, 0.0, "academ")
    x, y = W.academ_to_map(22.9 - 2.05, 6.6 - 5.08 + 7.0)
    add("Square", x, y, 0.0, 12, 0.7, 0.0, 0.0, "academ")
    # the town: its square, shops and bins on the streets, heating hatches, benches, feeders
    cx, cy = W.TOWN_CENTER
    add("Square", cx - 30, cy - 25, 0.0, 16, 0.7, 0.0, 0.0, "town")
    add("Square", cx + 35, cy + 30, 0.0, 16, 0.7, 0.0, 0.0, "town")
    ca, sa = math.cos(W.TOWN_ROT), math.sin(W.TOWN_ROT)

    def tw(u, v):
        return cx + u * ca - v * sa, cy + u * sa + v * ca
    for bu in range(-6, 6):
        for bv in range(-5, 6):
            # street crossings: bins at some, a heating hatch at others (the heating main runs under the streets)
            x, y = tw(bu * 90.0, bv * 75.0)
            if math.hypot(x - cx, y - cy) > W.TOWN_R - 20 or not walk(x, y):
                continue
            r = rnd.random()
            if r < 0.22:
                add("Bins", x + 4, y + 4, 0.0, 3, 0.0, 0.0, 0.6, "town")
            elif r < 0.40:
                add("WarmHatch", x - 3, y + 3, 0.0, 4, 0.2, 1.0, 0.0, "town")
            elif r < 0.50:
                add("Bench", x + 5, y - 5, 0.0, 2, 0.6, 0.0, 0.0, "town")
    feeders = 0
    for p in list(places):
        if PLACES[p[0]] == "Doorstep" and p[10] == "house" and rnd.random() < 0.08 and feeders < 14:
            add("Feeder", p[1] + math.cos(p[4]) * 1.5, p[2] - fy + math.sin(p[4]) * 1.5, p[4], 4, p[6], 0.0, 1.0, "town")
            feeders += 1
    # empty lots and the edges of the copses: dens for the dogs
    for gx in range(int(X0) + 40, int(X1) - 40, 70):
        for gy in range(int(Y0) + 300, int(Y1) - 60, 70):
            x, y = gx + rnd.uniform(-20, 20), gy + rnd.uniform(-20, 20)
            i, j = int(round((x - X0) / CELL)), int(round((y - Y0) / CELL))
            if not walk(x, y):
                continue
            c = kind[j * nx + i] & 7
            if c in (FOREST, GRASS) and rnd.random() < 0.35:
                add("Den", x, y, rnd.uniform(-math.pi, math.pi), 6, 0.3, 0.3, 0.0, "edge")
    # the promenade and the beach, the club's door, the stations
    for y in range(int(W.PROMENADE_Y[0]), int(W.PROMENADE_Y[1]), 60):
        x = W.shore_x(y) - 10.0
        add("Beach", x, y, 0.0, 10, 1.0, 0.0, 0.2, "beach")
        xb = W.shore_x(y) - (W.BEACH_W - 5.0)
        if rnd.random() < 0.4:
            add("Bench", xb - 1.5, y, 0.0, 2, 0.8, 0.0, 0.0, "promenade")
    add("ClubDoor", W.DOOR_X, 5.5, math.pi / 2, 3, 0.5, 0.3, 0.0, "club")
    add("Lookout", W.DOOR_X - 8.0, 60.0, 0.0, 3, 0.8, 0.0, 0.0, "spur")
    add("Den", W.DOOR_X + 9.0, 12.0, 0.0, 2, 0.3, 0.2, 0.0, "spur")
    add("Station", W.STATION_UP[0], W.STATION_UP[1] - 9.0, 0.0, 3, 0.5, 0.0, 0.0, "upper")
    add("Station", W.STATION_DOWN[0] + 9.0, W.STATION_DOWN[1], 0.0, 3, 0.5, 0.0, 0.0, "lower")
    # sunny walls: the south faces of the houses (cats bask there in the morning)
    for b in boxes:
        if rnd.random() < 0.35:
            for (u, v) in ((0, -b[4] - 1.2), (0, b[4] + 1.2)):
                x, y = box_point(b, u, v)
                yaw = b[2] + (math.pi / 2 if v > 0 else -math.pi / 2)
                if -math.sin(yaw) > 0.5:
                    add("SunnyWall", x, y, yaw, 2, 1.0, 0.0, 0.0, b[5].split(":")[0])

    # refuges a cat goes up when a dog is after it: the Academgorodok's square trees and gardens; guessed yard
    # trees in the town (tag "guess": the game puts the real trees of the town's forests in instead)
    for tx, ty, sp, seed, age in lay.get("trees", []):
        x, y = W.academ_to_map(tx, ty)
        add("Refuge", x, y, 0.0, 2, 0.0, 0.0, 0.0, "academ", 2.6, need_walk=False)
    for g in lay.get("garden", []):
        (gx, gy), (hx, hy) = g["centre"], g["half"]
        for _ in range(g.get("count", 8) // 2):
            x, y = W.academ_to_map(gx + rnd.uniform(-hx, hx), gy + rnd.uniform(-hy, hy))
            add("Refuge", x, y, 0.0, 2, 0.0, 0.0, 0.0, "guess", 2.4, need_walk=False)
    for b in boxes:
        if b[5] == "house" and rnd.random() < 0.3:
            for _ in range(4):
                u, v = rnd.uniform(-b[3], b[3]), rnd.choice((-1, 1)) * (b[4] + rnd.uniform(3.0, 7.0))
                x, y = box_point(b, u, v)
                if walk(x, y):
                    add("Refuge", x, y, 0.0, 2, 0.0, 0.0, 0.0, "guess", 2.2)
                    break
    for x, y, sp, seed, age in W.alley_trees():
        add("Refuge", x, y, 0.0, 1, 0.0, 0.0, 0.0, "club", 2.4, need_walk=False)

    os.makedirs(a.out, exist_ok=True)
    with open(os.path.join(a.out, "animal_world.bin"), "wb") as f:
        f.write(struct.pack("<4sii", b"AWG1", nx, ny))
        f.write(struct.pack("<fff", X0, fy + Y0, CELL))
        f.write(struct.pack("<%dh" % len(z), *[int(round(max(v, W.SEA_Z) * 100)) for v in z]))
        f.write(bytes(kind))
    doc = {"version": 1, "frame": "unreal_metres", "face_y_cm": face_y, "places_kinds": PLACES,
           "fields": ["kind", "x", "y", "z", "yaw", "capacity", "sun", "warm", "food", "height", "tag"],
           "grid": "animal_world.bin", "places": places}
    with open(os.path.join(a.out, "animal_places.json"), "w") as f:
        json.dump(doc, f, separators=(",", ":"))
    counts = {}
    for p in places:
        counts[PLACES[p[0]]] = counts.get(PLACES[p[0]], 0) + 1
    kc = [0] * 6
    for c in kind:
        kc[c & 7] += 1
    print("animal_map: grid %dx%d (%s), %d places %s -> %s" % (nx, ny, dict(zip(["blocked", "grass", "paved", "yard", "sand", "forest"], kc)), len(places), counts, a.out))


if __name__ == "__main__":
    main()
