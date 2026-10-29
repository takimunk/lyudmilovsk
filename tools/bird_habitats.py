"""Where the wild birds of the outdoor map can be: perches, feeding grounds and roosts, by habitat,
for the bird life simulation (Private/Birds, docs/bird_life.md). Reads the map (tools/world_map.py) and
the built ground (tools/terrain.py ground_z) and writes

  unreal/Lyudmilovsk/Content/BirdsData/bird_habitats.json   sites, in Unreal metres (UE cm / 100)
  unreal/Lyudmilovsk/Content/BirdsData/bird_world.bin       8 m grid: ground and obstacle-top heights

  python3 tools/bird_habitats.py [--face-y CM]

A site is a point or a segment (a roof ridge, a rope, a ledge) with a radius around it (a patch of
ground or water to walk and peck in) and a capacity (how many birds it holds). Trees found here are
only a guess from the forest tiles (flag 1) or the hero plants' ages (flag 2): in the game the bird
life replaces them with the real trees' crowns.
"""
import argparse
import json
import math
import os
import random
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
sys.path.insert(0, os.path.join(ROOT, "blender", "academgorodok"))
import world_map as W  # noqa: E402
import terrain as T  # noqa: E402
import plan as AP  # noqa: E402  (the Academgorodok's houses)
from geom import Frame  # noqa: E402
OUT_DIR = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "BirdsData")

# Order matters: LyudBirds::EHabitat (BirdSim.h) has the same order.
KINDS = ["Sea", "Beach", "Cliff", "Meadow", "Reed", "Street", "Roof", "Tower", "Wire", "Garden",
         "Broadleaf", "Conifer", "ForestFloor"]
FLAG_GUESSED_TREE = 1        # replace with the real forest's trees in the game
FLAG_HERO_PLANT = 2          # one of the hero plants (AProceduralPlant): crown height from the actor
GRID_CELL = 8.0
MARGIN = 120.0
CANOPY = {"old": 21.0, "dune": 7.0, "wet": 14.0, "town": 10.0}


def face_y_default():
    try:
        with open(os.path.join(ROOT, "blender", "export", "scene01_layout.json")) as f:
            return json.load(f)["outside"]["portal_face_y"]
    except (OSError, KeyError):
        return 790.0                   # the current layout's portal face (Unreal Y, cm)


def house_roofs():
    """(kind, a, b, radius) per building, map metres: ridges, flat roofs, the church's nave and tower."""
    out = []
    for x, y, yaw, w, d, fl, kind in W.town():
        z = min(T.ground_z(x + dx, y + dy) for dx in (-w / 2, 0, w / 2) for dy in (-d / 2, 0, d / 2))
        ca, sa = math.cos(yaw), math.sin(yaw)
        if kind == "block":
            top = z + fl * 2.8 + 0.6 + 0.3
            hw = w / 2 - 1.5
            out.append(("Roof", (x - hw * ca, y - hw * sa, top), (x + hw * ca, y + hw * sa, top), max(d / 2 - 1.5, 1.0)))
        elif kind == "kirche":
            nx, ny = x - 14 * ca, y - 14 * sa
            rh = 7.0 * math.tan(math.radians(42))
            out.append(("Roof", (nx - 11 * ca, ny - 11 * sa, z + 14 + rh), (nx + 11 * ca, ny + 11 * sa, z + 14 + rh), 0.0))
            # the tower's top ledge round the spire, and the spire's tip
            r = w * 0.5
            out.append(("Tower", (x - r * ca + r * sa, y - r * sa - r * ca, z + 30.0), (x + r * ca + r * sa, y + r * sa - r * ca, z + 30.0), 0.0))
            out.append(("Tower", (x - r * ca - r * sa, y - r * sa + r * ca, z + 30.0), (x + r * ca - r * sa, y + r * sa + r * ca, z + 30.0), 0.0))
            out.append(("Tower", (x, y, z + 46.0), (x, y, z + 46.0), 0.0))
        else:
            h = fl * 3.0 + 0.4
            ww, dd, yy = (w, d, yaw) if w >= d else (d, w, yaw + math.pi / 2)
            c2, s2 = math.cos(yy), math.sin(yy)
            rz = z + h + (dd / 2) * math.tan(math.radians(42))
            hw = ww / 2 + 0.2
            out.append(("Roof", (x - hw * c2, y - hw * s2, rz), (x + hw * c2, y + hw * s2, rz), 0.0))
    return out


def academ_houses():
    """The Academgorodok's three houses: (frame in map metres, length, depth, eaves z, ridge z) (blender/academgorodok/plan.py)."""
    out = []
    ox, oy, ayaw = W.academ()
    gz = W.academ_z()
    for name, (cx, cy, yaw, mirror) in AP.FRAMES.items():
        n1 = name == "Novikova1"
        L, D = (AP.N1_L, AP.N1_D) if n1 else (AP.S_L, AP.S_D)
        ez = gz + AP.eaves_z(AP.N1_FLOORS if n1 else AP.S_FLOORS)
        rz = ez + D / 2 * math.tan(AP.N1_PITCH if n1 else AP.S_PITCH)
        mx, my = W.academ_to_map(cx, cy)
        # the house's u axis on the map: the ensemble's turn (x -> (sin, -cos)) after the house's own yaw
        ux, uy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        u = (ux * math.sin(ayaw) - uy * math.cos(ayaw), -ux * math.cos(ayaw) - uy * math.sin(ayaw))
        out.append((mx, my, math.atan2(u[1], u[0]), L, D, ez, rz))
    return out


def house_boxes():
    """(x, y, yaw, half w, half d, top z) of every building, map metres, for the obstacle grid."""
    out = [(x, y, yaw, L / 2, D / 2, rz + 1.0) for x, y, yaw, L, D, ez, rz in academ_houses()]
    for x, y, yaw, w, d, fl, kind in W.town():
        z = T.ground_z(x, y)
        top = {"block": fl * 2.8 + 1.0, "kirche": 46.0}.get(kind, fl * 3.0 + 0.4 + min(w, d) / 2 * 0.9)
        out.append((x, y, yaw, w / 2 + (14 if kind == "kirche" else 0), d / 2, z + top))
    return out


def in_box(x, y, b, grow=0.0):
    bx, by, yaw, hw, hd, _ = b
    ca, sa = math.cos(yaw), math.sin(yaw)
    u = (x - bx) * ca + (y - by) * sa
    v = -(x - bx) * sa + (y - by) * ca
    return abs(u) <= hw + grow and abs(v) <= hd + grow


def forest_at(x, y, z, tiles):
    """The forest mix growing at (x, y) or None (the tile rectangles and their height band)."""
    for fx, fy, hx, hy, mix, count, spacing, z0, z1, seed in tiles:
        if abs(x - fx) <= hx and abs(y - fy) <= hy and z0 <= z <= z1:
            return mix
    return None


def slope(x, y):
    e = 2.0
    hx = (T.ground_z(x + e, y) - T.ground_z(x - e, y)) / (2 * e)
    hy = (T.ground_z(x, y + e) - T.ground_z(x, y - e)) / (2 * e)
    return math.hypot(hx, hy)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--face-y", type=float, default=None, help="portal face, Unreal Y cm")
    ap.add_argument("--out", default=OUT_DIR)
    a = ap.parse_args()
    face_y = a.face_y if a.face_y is not None else face_y_default()
    fy = face_y / 100.0
    rnd = random.Random(2026)
    tiles = W.forests()
    houses = house_boxes()
    sites = []

    def add(kind, p, q=None, radius=0.0, cap=8, flags=0):
        q = q or p
        # map metres -> Unreal metres (X stays, Y from the portal face)
        sites.append([KINDS.index(kind), round(p[0], 2), round(fy + p[1], 2), round(p[2], 2),
                      round(q[0], 2), round(fy + q[1], 2), round(q[2], 2), round(radius, 2), cap, flags])

    def near_house(x, y, grow):
        return any(in_box(x, y, b, grow) for b in houses if abs(b[0] - x) < 40 and abs(b[1] - y) < 40)

    # the sea off the coast, the wet sand at the water line, the clay cliff's face
    for y in range(int(W.Y_MIN), int(W.Y_MAX) + 1, 70):
        for off in (35.0, 100.0, 190.0, 320.0):
            x = W.shore_x(y) + off + rnd.uniform(-10, 10)
            if x < W.X_MAX + MARGIN:
                add("Sea", (x, y + rnd.uniform(-15, 15), W.SEA_Z), radius=20.0, cap=30)
    for y in range(int(W.Y_MIN), int(W.Y_MAX) + 1, 30):
        x = W.shore_x(y) - 6.0
        add("Beach", (x, y, T.ground_z(x, y) + 0.02), radius=6.0, cap=16)
    for y in range(int(W.Y_MIN), int(W.Y_MAX) + 1, 25):
        top = W._edge_top(y)
        if top - W.BEACH_TOP < 20.0:
            continue
        s = W.BEACH_W + W.cliff_width(top) * 0.5
        p = []
        for dy in (-8.0, 8.0):
            x = W.shore_x(y + dy) - s
            p.append((x, y + dy, T.ground_z(x, y + dy)))
        add("Cliff", p[0], p[1], 0.0, 10)

    # open ground: meadows and fields, the cliff top, the spur; reed and sedge on the wet valley floor
    for gy in range(int(W.Y_MIN), int(W.Y_MAX) + 1, 45):
        for gx in range(int(W.X_MIN), int(W.X_MAX) + 1, 45):
            x, y = gx + rnd.uniform(-12, 12), gy + rnd.uniform(-12, 12)
            if W.inland(x, y) < W.BEACH_W + 6.0:
                continue
            z = T.ground_z(x, y)
            if slope(x, y) > 0.35 or forest_at(x, y, z, tiles) in ("old", "wet", "dune"):
                continue
            if math.hypot(x - W.TOWN_CENTER[0], y - W.TOWN_CENTER[1]) < W.TOWN_R - 40 or near_house(x, y, 6.0):
                continue
            add("Meadow", (x, y, z + 0.02), radius=10.0, cap=24)
    for p in W.grass_patches():
        if p[4] in ("CommonReed", "SandSedge"):
            # over the whole patch, every 15 m
            nx_, ny_ = max(1, int(p[2] * 2 / 15.0)), max(1, int(p[3] * 2 / 15.0))
            for i in range(nx_):
                for j in range(ny_):
                    x, y = p[0] - p[2] + (i + 0.5) * 2 * p[2] / nx_, p[1] - p[3] + (j + 0.5) * 2 * p[3] / ny_
                    add("Reed", (x, y, T.ground_z(x, y) + 0.6), radius=6.0, cap=20)

    # the town: streets and yards, the square, the promenade; roofs, the church tower
    cx, cy = W.TOWN_CENTER
    for gy in range(int(cy - W.TOWN_R), int(cy + W.TOWN_R) + 1, 32):
        for gx in range(int(cx - W.TOWN_R), int(cx + W.TOWN_R) + 1, 32):
            x, y = gx + rnd.uniform(-6, 6), gy + rnd.uniform(-6, 6)
            if math.hypot(x - cx, y - cy) > W.TOWN_R or W.inland(x, y) < W.BEACH_W + 6 or W.raised(x, y) > 1.0:
                continue
            if near_house(x, y, 3.0):
                continue
            add("Street", (x, y, T.ground_z(x, y) + 0.02), radius=5.0, cap=14)
    for dx, dy in ((-20, -15), (15, -20), (-15, 22), (25, 18), (0, 0)):
        add("Street", (cx + dx, cy + dy, T.ground_z(cx + dx, cy + dy) + 0.02), radius=8.0, cap=40)
    for y in range(int(W.PROMENADE_Y[0]), int(W.PROMENADE_Y[1]) + 1, 40):
        x = W.shore_x(y) - (W.BEACH_W - 5.0)
        add("Street", (x, y, T.ground_z(x, y) + 0.02), radius=3.0, cap=10)
    for kind, p, q, r in house_roofs():
        add(kind, p, q, r, 10 if kind == "Roof" else 16)
    # the Academgorodok: ridges and both eaves of each house (jackdaws, pigeons, starlings, wagtails sit on them)
    for x, y, yaw, L, D, ez, rz in academ_houses():
        ca, sa = math.cos(yaw), math.sin(yaw)
        hw = L / 2 - 1.0
        for off, z in ((0.0, rz), (-D / 2 - 0.3, ez), (D / 2 + 0.3, ez)):
            nx_, ny_ = -sa * off, ca * off
            add("Roof", (x + nx_ - hw * ca, y + ny_ - hw * sa, z + 0.1), (x + nx_ + hw * ca, y + ny_ + hw * sa, z + 0.1), 0.0, 24)
    # the Academgorodok's power lines (tools/academ_wires.py): every conductor, sagging, in pieces of up to 4 m
    wires_path = os.path.join(OUT_DIR, "wires.json")
    if os.path.exists(wires_path):
        with open(wires_path) as f:
            wdoc = json.load(f)
        wfy = wdoc["face_y_cm"] / 100.0
        for wr in wdoc["wires"]:
            wa, wb, sg = wr["a"], wr["b"], wr["sag"]
            n = max(1, int(math.ceil(math.dist(wa, wb) / 4.0)))
            pts = []
            for k in range(n + 1):
                t = min(max(k / n, 0.04), 0.96)          # keep off the insulators
                pts.append((wa[0] + (wb[0] - wa[0]) * t, wa[1] - wfy + (wb[1] - wa[1]) * t,
                            wa[2] + (wb[2] - wa[2]) * t - 4 * sg * t * (1 - t)))
            for p, q in zip(pts, pts[1:]):
                add("Wire", p, q, 0.0, max(2, int(math.dist(p, q) / 0.15)))

    # the cable car's two ropes, between their supports, sagging (world_mesh.cable_car)
    cc = W.cable_car()
    (x0, y0), (x1, y1) = cc["upper"], cc["lower"]
    yaw = math.atan2(y1 - y0, x1 - x0)
    nx, ny = -math.sin(yaw), math.cos(yaw)
    for side in (-3.0, 3.0):
        for (ax, ay, az), (bx, by, bz) in zip(cc["supports"], cc["supports"][1:]):
            span = math.hypot(bx - ax, by - ay)
            n = max(1, int(span / 20))
            pts = []
            for k in range(n + 1):
                t = 0.15 + 0.7 * k / n                    # keep off the towers' heads and the stations
                sag = 4 * t * (1 - t) * span * 0.012
                pts.append((ax + (bx - ax) * t + side * nx, ay + (by - ay) * t + side * ny, az + (bz - az) * t - sag + 0.03))
            for p, q in zip(pts, pts[1:]):
                add("Wire", p, q, 0.0, 8)

    # trees: guessed from the forest tiles (the game puts the real crowns in), the hero plants
    for fx, fyy, hx, hy, mix, count, spacing, z0, z1, seed in tiles:
        step = 28.0 if mix != "town" else 40.0
        ny_ = max(1, int(2 * hy / step))
        nx_ = max(1, int(2 * hx / step))
        for j in range(ny_):
            for i in range(nx_):
                x = fx - hx + (i + 0.5) * 2 * hx / nx_ + rnd.uniform(-6, 6)
                y = fyy - hy + (j + 0.5) * 2 * hy / ny_ + rnd.uniform(-6, 6)
                z = T.ground_z(x, y)
                if not (z0 <= z <= z1) or slope(x, y) > 0.6 or W.inland(x, y) < W.BEACH_W + 3:
                    continue
                if mix == "town" and (near_house(x, y, 3.0) or rnd.random() > 0.5):
                    continue
                kind = {"old": "Conifer" if rnd.random() < 0.75 else "Broadleaf", "dune": "Conifer",
                        "wet": "Broadleaf", "town": "Garden"}[mix]
                h = CANOPY[mix] * rnd.uniform(0.8, 1.1)
                add(kind, (x, y, z + h * 0.7), radius=2.5, cap=6, flags=FLAG_GUESSED_TREE)
                if mix in ("old", "wet") and rnd.random() < 0.5:
                    add("ForestFloor", (x + 4, y + 4, T.ground_z(x + 4, y + 4) + 0.02), radius=6.0, cap=10)
    for x, y, sp, seed, age in W.alley_trees() + W.coast_plants():
        kind = "Conifer" if sp in ("ScotsPine", "DunePine", "Juniper") else "Broadleaf"
        h = min(age * 0.8, 18.0) if kind == "Broadleaf" else min(age * 0.35, 16.0)
        add(kind, (x, y, T.ground_z(x, y) + h * 0.75), radius=1.5, cap=4, flags=FLAG_HERO_PLANT)
    for p in W.path_points(15.0):
        x, y = p[0] + 2.2, p[1]
        add("Meadow", (x, y, T.ground_z(x, y) + 0.02), radius=2.0, cap=6)

    # the ground and the tallest thing on it (roofs, crowns) on an 8 m grid, for flight clearance
    gx0, gy0 = W.X_MIN - MARGIN, W.Y_MIN - MARGIN
    nxg = int((W.X_MAX - W.X_MIN + 2 * MARGIN) / GRID_CELL) + 1
    nyg = int((W.Y_MAX - W.Y_MIN + 2 * MARGIN) / GRID_CELL) + 1
    ground, top = [], []
    for j in range(nyg):
        y = gy0 + j * GRID_CELL
        for i in range(nxg):
            x = gx0 + i * GRID_CELL
            g = T.ground_z(x, y) if W.X_MIN <= x <= W.X_MAX and W.Y_MIN <= y <= W.Y_MAX else W.height(x, y)
            g = max(g, W.SEA_Z)
            t = g
            mix = forest_at(x, y, g, tiles)
            if mix and W.inland(x, y) > W.BEACH_W + 3:
                t = g + CANOPY[mix] * (1.15 if mix != "town" else 0.6)
            ground.append(g)
            top.append(t)
    for b in houses:
        bx, by, yaw_, hw, hd, tz = b
        r = math.hypot(hw, hd)
        for j in range(max(0, int((by - r - gy0) / GRID_CELL)), min(nyg, int((by + r - gy0) / GRID_CELL) + 2)):
            for i in range(max(0, int((bx - r - gx0) / GRID_CELL)), min(nxg, int((bx + r - gx0) / GRID_CELL) + 2)):
                if in_box(gx0 + i * GRID_CELL, gy0 + j * GRID_CELL, b, GRID_CELL * 0.75):
                    k = j * nxg + i
                    top[k] = max(top[k], tz)
    os.makedirs(a.out, exist_ok=True)
    with open(os.path.join(a.out, "bird_world.bin"), "wb") as f:
        f.write(struct.pack("<4sii", b"BWG1", nxg, nyg))
        f.write(struct.pack("<fff", gx0, fy + gy0, GRID_CELL))
        f.write(struct.pack("<%dh" % len(ground), *[int(round(v * 100)) for v in ground]))
        f.write(struct.pack("<%dh" % len(top), *[int(round(v * 100)) for v in top]))
    doc = {"version": 1, "frame": "unreal_metres", "face_y_cm": face_y, "sea_z": W.SEA_Z,
           "kinds": KINDS, "fields": ["kind", "ax", "ay", "az", "bx", "by", "bz", "radius", "capacity", "flags"],
           "grid": "bird_world.bin", "sites": sites}
    with open(os.path.join(a.out, "bird_habitats.json"), "w") as f:
        json.dump(doc, f, separators=(",", ":"))
    counts = {}
    for s in sites:
        counts[KINDS[s[0]]] = counts.get(KINDS[s[0]], 0) + 1
    print("bird_habitats: %d sites %s, grid %dx%d -> %s" % (len(sites), counts, nxg, nyg, a.out))


if __name__ == "__main__":
    main()
