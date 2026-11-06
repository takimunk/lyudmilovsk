#!/usr/bin/env python3
"""Lyudmilovsk's public spaces (docs/city/public_spaces.md), laid out on the town plan (Content/City/city_plan.json,
tools/city_plan.py): the promenade's paving and furniture on T0, the railing on the seawall, beach furniture, Sun
Square with its fountain and stage, the seaside park with its pond and playground, the fair, the old stair up to the
club (L-CLUB-STAIR), lamps everywhere. The plan's lead builds the ground, the walls, their stairs and the pier deck;
this only furnishes them.

  python3 tools/public_spaces.py              -> unreal/Lyudmilovsk/Content/City/public_spaces.json  (ALyudPublicSpaces,
                                                 ULyudPublicSpotsSubsystem build it at play start)
                                              -> unreal/Lyudmilovsk/Content/PeopleData/town_plan.json  key "spots" only
  python3 tools/public_spaces.py --png p.png  a plan drawing

Everything is read from the plan (plots, spots, wall bays, stairs, links, terraces), so the props follow its edits:
regenerate after the plan changes. Written in Unreal metres (x = map X, y = map Y + portal face, z), yaw = Unreal
degrees (0 = +x = toward the sea, 90 = +y = north). Map frame and the terrace frame (s = metres inland, y) as the plan's.
"""
import argparse
import json
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import world_map as W  # noqa: E402
import city_plan as C  # noqa: E402

try:
    import terrain as T  # noqa: E402

    def ground(x, y):
        return T.ground_z(x, y)
except Exception:  # noqa: BLE001
    def ground(x, y):
        return W.height(x, y)

REPO = os.path.normpath(os.path.join(HERE, ".."))
CONTENT = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content")
PLAN = os.path.join(CONTENT, "City", "city_plan.json")
OUT = os.path.join(CONTENT, "City", "public_spaces.json")
TOWN_PLAN = os.path.join(CONTENT, "PeopleData", "town_plan.json")

PAVE = 0.04                       # paving's top over the terrace ground
SLAB_DEPTH = 1.4                  # paving blocks reach this far down (they hide the old ground until the terraces land)


def r3(v):
    return [round(float(c), 3) for c in v]


class Plan:
    def __init__(self):
        with open(PLAN) as f:
            self.d = json.load(f)
        self.face_y = self.d["portal_face_y"] / 100.0
        self.tz = {t["id"]: t["z"] for t in self.d["terraces"]}
        self.plots = {p["id"]: p for p in self.d["plots"]}
        self.spots = {s["id"]: s for s in self.d["spots"]}
        self.links = {l["id"]: l for l in self.d["links"]}
        self.bays = self.d.get("wall_bays", {})

    def keepout(self):
        """(y0, y1, s0, s1) rectangles on T0 nothing of ours may stand in: the sea steps, the wall stairs and the
        grand flight's foot, the kiosks, the cable car's «amber» station."""
        out = []
        for b in self.bays.get("SEA", []):
            if b["kind"] == "sea_steps":
                y = b["mid"][1]
                out.append((y - 6.0, y + 6.0, 18.0, 31.0))
        for b in self.bays.get("W1", []):
            y = b["mid"][1]
            if b["kind"] == "stair":
                out.append((y - 9.0, y + 9.0, 58.0, 80.0))
            elif b["kind"] == "grand":
                out.append((y - 7.0, y + 7.0, 48.0, 80.0))
            elif b["kind"] == "unit":
                out.append((y - 3.0, y + 3.0, 63.0, 80.0))
        for pid, p in self.plots.items():
            if p["kind"] == "kiosk":
                y = p["map"][1]
                s = W.inland(p["map"][0], y)
                out.append((y - p["w"] / 2 - 2.0, y + p["w"] / 2 + 2.0, s - p["d"] / 2 - 2.0, s + p["d"] / 2 + 2.0))
        amb = self.links.get("L-CABLE-AMBER")
        if amb:
            x, y = amb["map"][:2]
            s = W.inland(x, y)
            out.append((y - 20.0, y + 20.0, s - 20.0, s + 20.0))
        return out

    def plot_rect(self, pid):
        p = self.plots[pid]
        y = p["map"][1]
        s = W.inland(p["map"][0], y)
        return (y - p["w"] / 2, y + p["w"] / 2, s - p["d"] / 2, s + p["d"] / 2)


def at(s, y):
    return C.to_map(s, y)


def sea_yaw(y):
    return C.seaward_yaw(y)


class Layout:
    def __init__(self, plan):
        self.P = plan
        self.d = {k: [] for k in ("boxes", "props", "plants", "trees", "spots", "lights", "fountains", "waters", "areas")}
        self.rnd = random.Random(1958)
        self.n = {}
        self.keep = plan.keepout()

    def free(self, s, y, pad=0.0):
        return not any(y0 - pad <= y <= y1 + pad and s0 - pad <= s <= s1 + pad for y0, y1, s0, s1 in self.keep)

    def ue(self, x, y, z):
        return r3((x, y + self.P.face_y, z))

    def uid(self, prefix):
        self.n[prefix] = self.n.get(prefix, 0) + 1
        return "%s%02d" % (prefix, self.n[prefix])

    def box(self, kind, cx, cy, yaw, lx, ly, z0, z1, skin):
        self.d["boxes"].append({"kind": kind, "c": self.ue(cx, cy, (z0 + z1) / 2), "yaw": round(yaw, 2),
                                "size": r3((lx, ly, z1 - z0)), "skin": skin})

    def prop(self, kind, x, y, z, yaw, **extra):
        p = {"kind": kind, "at": self.ue(x, y, z), "yaw": round(yaw, 2)}
        p.update(extra)
        self.d["props"].append(p)
        return p

    def spot(self, sid, kind, x, y, z, yaw, seats=1, slots=None, path=None, **extra):
        s = {"id": sid, "kind": kind, "pos": self.ue(x, y, z), "yaw": round(yaw, 2), "seats": seats}
        if slots:
            s["slots"] = slots
        if path:
            s["path"] = [self.ue(*p) for p in path]
        s.update(extra)
        self.d["spots"].append(s)
        return s

    def bench(self, sid, x, y, z, yaw, kind="bench", style="park", **extra):
        """A bench (1.8 m, three seats); yaw = the way its sitters look."""
        self.prop("bench_" + style, x, y, z, yaw)
        a = math.radians(yaw)
        fx, fy = math.cos(a), math.sin(a)
        lx, ly = -fy, fx
        slots = [{"pos": self.ue(x + lx * o + fx * 0.05, y + ly * o + fy * 0.05, z + 0.45), "yaw": round(yaw, 2),
                  "height": 0.45} for o in (-0.58, 0.0, 0.58)]
        self.spot(sid, kind, x, y, z, yaw, seats=3, slots=slots, **extra)

    def lamp(self, x, y, z, kind="lamp", area=""):
        self.prop(kind, x, y, z, 0.0)
        h = {"lamp": 4.7, "lamp_park": 3.5, "lamp_low": 1.1}[kind]
        self.d["lights"].append({"at": self.ue(x, y, z + h), "area": area, "kind": kind})

    def plant(self, species, x, y, z, age):
        self.d["plants"].append({"species": species, "at": self.ue(x, y, z), "age": round(age, 1),
                                 "seed": self.rnd.randint(1, 99999)})

    def tree(self, species, x, y, z, age):
        self.d["trees"].append({"species": species, "at": self.ue(x, y, z), "age": round(age, 1),
                                "variant": self.rnd.randint(0, 2), "yaw": round(self.rnd.uniform(0, 360), 1)})

    def pave(self, kind, s0, s1, y0, y1, z, skin, tile=8.0, depth=SLAB_DEPTH, hole=None):
        """Paving over a rectangle of the terrace frame, in tiles following the coast's slight bend.
        hole(s, y) -> True leaves that tile out (a fountain's basin, a pond)."""
        s = s0
        while s < s1 - 0.01:
            ds = min(tile, s1 - s)
            y = y0
            while y < y1 - 0.01:
                dy = min(tile, y1 - y)
                sc, yc = s + ds / 2, y + dy / 2
                if not (hole and hole(sc, yc)):
                    cx, cy = at(sc, yc)
                    self.box(kind, cx, cy, sea_yaw(yc) + 90.0, dy + 0.03, ds + 0.03, z - depth, z + PAVE, skin)
                y += dy
            s += ds


# --- T0: the promenade ------------------------------------------------------------------------------------------
def promenade(L):
    P = L.P
    z = P.tz["T0"] + PAVE
    y0, y1 = C.CITY_Y0, C.CITY_Y1
    rail_s = C.SEAWALL_S + 0.35
    w1 = lambda y: C.wall_s(0, y)  # noqa: E731
    # paving: the broad walk of pale concrete slabs by the sea, a granite band in the middle, the inner walk by W1
    y = y0
    while y < y1 - 0.01:
        dy = min(8.0, y1 - y)
        yc = y + dy / 2
        for (sa, sb, skin) in ((C.SEAWALL_S, 44.0, "deck"), (44.0, 47.0, "granite"), (47.0, w1(yc), "deck")):
            cx, cy = at((sa + sb) / 2, yc)
            L.box("promenade", cx, cy, sea_yaw(yc) + 90.0, dy + 0.03, sb - sa, z - PAVE - SLAB_DEPTH, z, skin)
        y += dy
    # the railing on the seawall's edge, a gap at each flight of sea steps
    y = y0 + 1.0
    while y < y1 - 1.0:
        if L.free(C.SEAWALL_S + 0.5, y) and abs(y - 905.0) > 7.0:
            x, yy = at(rail_s, y)
            L.prop("railing", x, yy, z, sea_yaw(y) + 90.0)
        y += 2.0
    # lamps: the sea row every 30 m by the railing, the inner row offset by half a span
    y = y0 + 10.0
    k = 0
    while y < y1 - 10.0:
        if L.free(C.SEAWALL_S + 1.2, y, 1.0) and abs(y - 905.0) > 8.0:
            L.lamp(*at(C.SEAWALL_S + 1.2, y), z, area="promenade")
        if L.free(56.0, y + 15.0, 1.0):
            L.lamp(*at(56.0, y + 15.0), z, area="promenade")
        y += 30.0
        k += 1
    # the plan's benches (S-BENCH-P*): backs to W1, facing the sea; a pair by the railing between them
    for sid, s in sorted(P.spots.items()):
        if sid.startswith("S-BENCH-P"):
            x, yy, _ = s["map"]
            L.bench(sid, x, yy, z, s["yaw"], style="promenade", terrace="T0", area="promenade",
                    name_ru=s.get("name_ru"), name_en=s.get("name_en"))
    y = y0 + 40.0
    while y < y1 - 10.0:
        sy = y + 20.0
        if L.free(C.SEAWALL_S + 2.4, sy, 1.5) and abs(sy - 905.0) > 10.0:
            L.bench(L.uid("S-BENCH-R"), *at(C.SEAWALL_S + 2.4, sy), z, sea_yaw(sy), style="promenade", terrace="T0",
                    area="promenade", sub="rail")
        y += 40.0
    # rail spots: people lean on the railing between the benches and watch the sea
    y = y0 + 30.0
    while y < y1 - 10.0:
        if L.free(C.SEAWALL_S + 0.9, y):
            x, yy = at(C.SEAWALL_S + 0.9, y)
            L.spot(L.uid("S-RAIL-"), "promenade", x, yy, z, sea_yaw(y), seats=4, sub="rail", terrace="T0")
        y += 40.0
    # trees in grates down the middle band, a maple every 20 m; bins by every other plan bench
    y = y0 + 10.0
    while y < y1 - 5.0:
        if L.free(45.5, y, 2.0) and abs(y - 905.0) > 14.0:
            x, yy = at(45.5, y)
            L.tree("Maple", x, yy, z, L.rnd.uniform(18, 32))
            L.prop("tree_grate", x, yy, z, sea_yaw(y))
        y += 20.0
    for k, (sid, s) in enumerate(sorted((i, s) for i, s in P.spots.items() if i.startswith("S-BENCH-P"))):
        if k % 2 == 0:
            x, yy, _ = s["map"]
            bx, by = at(W.inland(x, yy), yy + 1.7)
            L.prop("bin", bx, by, z, s["yaw"])
    # the stroll lines along it
    for sid, s_line in (("S-PROMENADE-SEA", C.SEAWALL_S + 5.0), ("S-PROMENADE-TOWN", 52.0)):
        pts = [(*at(s_line, yy), z) for yy in range(int(y0) + 5, int(y1) - 4, 15)]
        L.spot(sid, "promenade", *at(s_line, 905.0), z, sea_yaw(905.0) + 90.0, seats=40, path=pts, sub="stroll",
               terrace="T0")
    sp = P.spots.get("S-PROMENADE")
    if sp:
        x, yy, _ = sp["map"]
        pts = [(*at(C.SEAWALL_S + 5.0, v), z) for v in range(int(y0) + 5, int(y1) - 4, 15)]
        L.spot("S-PROMENADE", "promenade", x, yy, z, sp["yaw"], seats=sp.get("seats", 0) or 60, path=pts,
               terrace="T0", name_ru=sp.get("name_ru"), name_en=sp.get("name_en"))
    L.d["areas"].append({"id": "promenade", "name": "place.promenade", "at": L.ue(*at(45.0, 905.0), z), "r": 540.0})


# --- the beach ---------------------------------------------------------------------------------------------------
def beach(L):
    P = L.P
    sb = P.spots.get("S-BEACH")
    if sb:
        x, y, zz = sb["map"]
        L.spot("S-BEACH", "beach", x, y, ground(x, y), sb["yaw"], seats=sb["seats"], name_ru=sb.get("name_ru"),
               name_en=sb.get("name_en"))
    pier = P.plots.get("P-PIER")
    pier_y = pier["map"][1] if pier else 905.0
    bonfire = P.spots.get("S-BONFIRE")
    y = 600.0
    i = 0
    while y < 1400.0:
        for row, s in enumerate((12.0, 18.0)):
            yy = y + (7.0 if row else 0.0)
            if abs(yy - pier_y) < 14.0 or not L.free(s, yy, 2.0):
                continue
            if bonfire and math.hypot(yy - bonfire["map"][1], s - W.inland(*bonfire["map"][:2])) < 14.0:
                continue
            x, py = at(s, yy)
            gz = ground(x, py)
            if gz < W.SEA_Z + 0.9:
                continue
            L.prop("umbrella", x, py, gz, L.rnd.uniform(0, 360), variant=(i + row) % 4, season="summer")
            slots = []
            for side in (-1, 1):
                lx, ly = at(s - 0.6, yy + side * 1.3)
                lz = ground(lx, ly)
                L.prop("lounger", lx, ly, lz, sea_yaw(yy), season="summer")
                slots.append({"pos": L.ue(lx, ly, lz + 0.35), "yaw": sea_yaw(yy), "height": 0.35})
            L.spot(L.uid("S-BEACH-"), "beach", x, py, gz, sea_yaw(yy), seats=2, slots=slots, season="summer",
                   sub="loungers")
            i += 1
        y += 24.0
    # towels: free spots on the sand nearer the water (anyone with a towel, any warm day)
    for k in range(18):
        yy = 610.0 + k * 44.0 + 11.0
        if abs(yy - pier_y) < 14.0:
            continue
        x, py = at(7.0, yy)
        L.spot(L.uid("S-TOWEL-"), "beach", x, py, ground(x, py), sea_yaw(yy), seats=2, sub="towel", season="summer")
    # year round: changing booths by every other flight of sea steps
    for k, b in enumerate(b for b in P.bays.get("SEA", []) if b["kind"] == "sea_steps"):
        if k % 2 == 0 and 560.0 < b["mid"][1] < 1420.0:
            y = b["mid"][1] + 9.0
            x, py = at(21.5, y)
            L.prop("changing_booth", x, py, ground(x, py), sea_yaw(y))
    L.d["areas"].append({"id": "beach", "name": "place.beach", "at": L.ue(*at(12.0, 1000.0), W.SEA_Z + 1.2), "r": 450.0})


# --- Sun Square --------------------------------------------------------------------------------------------------
FOUNTAIN_STONE = {
    # Sun fountain: an 18 m round basin whose rim is a bench, a two-tier centrepiece; ALyudFountain's water reads the
    # levels (metres over the foot), ALyudPublicSpaces builds the stone from the same numbers
    "lower_level": 0.40, "lower_r": 8.55, "lower_floor": 0.06, "rim_r": 9.0, "rim_z": 0.46, "rim_w": 0.45,
    "upper_level": 2.42, "upper_r": 1.35, "upper_floor": 2.26, "lip_r": 1.45, "lip_z": 2.42,
    "nozzle_z": 3.45, "stem_r": 0.36, "neck_r": 0.18, "tier_r": 2.6, "tier_level": 1.02, "tier_lip_z": 1.02,
}
FLOW_SCALE = 1.8


def square(L):
    P = L.P
    y0, y1, s0, s1 = P.plot_rect("P-SQUARE")
    z = P.tz["T1"] + PAVE
    sf = P.spots["S-FOUNTAIN"]
    fx, fy, _ = sf["map"]
    fs = W.inland(fx, fy)
    st = FOUNTAIN_STONE
    s1c = max(s0 + 10.0, min(s1, C.wall_s(1, (y0 + y1) / 2) - 2.0))

    def in_basin(s, y):
        return math.hypot(s - fs, y - fy) < st["rim_r"] - 3.0

    # granite setts round the fountain (a 30 m disc pattern on 4 m tiles), pale slabs beyond
    L.pave("square", max(s0, 72.0), s1c, y0, y1, z - PAVE, "square", tile=6.0,
           hole=lambda s, y: math.hypot(s - fs, y - fy) < st["rim_r"] + 2.0)
    for k in range(24):
        a = 2 * math.pi * (k + 0.5) / 24
        for r, w in ((st["rim_r"] + 1.25, 2.5),):
            cx, cy = fx + math.cos(a) * r, fy + math.sin(a) * r
            L.box("square_ring", cx, cy, math.degrees(a), w + 0.05, 2 * math.pi * r / 24 + 0.12, z - PAVE - SLAB_DEPTH,
                  z, "granite")
    L.d["fountains"].append({"id": "sun", "at": L.ue(fx, fy, z), "yaw": sf["yaw"], "stone": st,
                             "flow_scale": FLOW_SCALE, "build_stone": True})
    L.d["areas"].append({"id": "square", "name": "place.square", "at": L.ue(fx, fy, z), "r": 60.0})
    # the rim is a bench: 24 seats looking out (and at the town); facing in for those who watch the water
    slots = []
    for j in range(24):
        a = 2 * math.pi * j / 24
        px, py = fx + math.cos(a) * (st["rim_r"] + 0.05), fy + math.sin(a) * (st["rim_r"] + 0.05)
        slots.append({"pos": L.ue(px, py, z + st["rim_z"]), "yaw": round(math.degrees(a), 2), "height": st["rim_z"]})
    L.spot("S-FOUNTAIN", "fountain", fx, fy, z, sf["yaw"], seats=24, slots=slots, name_ru=sf.get("name_ru"),
           name_en=sf.get("name_en"))
    # a ring of 12 benches facing the fountain at 18 m, 8 lamps between, 4 round flowerbeds on the diagonals at 25 m
    for j in range(12):
        a = 2 * math.pi * (j + 0.5) / 12
        L.bench(L.uid("S-BENCH-SQ"), fx + math.cos(a) * 17.0, fy + math.sin(a) * 17.0, z,
                math.degrees(a) + 180.0, area="square", terrace="T1")
    for j in range(8):
        a = 2 * math.pi * j / 8
        L.lamp(fx + math.cos(a) * 15.0, fy + math.sin(a) * 15.0, z, area="square")
    for j in range(4):
        a = 2 * math.pi * (j + 0.5) / 4
        flowerbed(L, fx + math.cos(a) * 26.0, fy + math.sin(a) * 26.0, z, 3.2, j)
    for k in range(12):
        a = 2 * math.pi * (k + 0.5) / 12 + 0.02
        L.lamp(fx + math.cos(a) * 34.0, fy + math.sin(a) * 34.0, z, area="square") if k % 3 == 0 else None
    # the plan's grannies' benches on the square
    for sid in ("S-BENCH-01", "S-BENCH-02"):
        s = P.spots.get(sid)
        if s:
            x, yy, _ = s["map"]
            ss = W.inland(x, yy)
            # back to the square's inland edge, facing the fountain
            L.bench(sid, x, yy, z, math.degrees(math.atan2(fy - yy, fx - x)), area="square", terrace="T1",
                    name_ru=s.get("name_ru"), name_en=s.get("name_en"))
    # maples along both long sides, in grates, with benches between them under the crowns
    for side, ye in ((-1, y0 + 4.0), (1, y1 - 4.0)):
        for k in range(8):
            s = s0 + 10.0 + k * 13.0
            if s > s1c - 6.0 or s < 72.0:
                continue
            x, yy = at(s, ye)
            L.tree("Maple", x, yy, z, L.rnd.uniform(25, 40))
            L.prop("tree_grate", x, yy, z, 0.0)
            if k < 7 and s + 6.5 < s1c - 6.0:
                bx, by = at(s + 6.5, ye)
                L.bench(L.uid("S-BENCH-SQ"), bx, by, z, sea_yaw(ye) + 90.0 * side * -1, area="square", terrace="T1")
    for k in range(4):
        a = 2 * math.pi * (k + 0.25) / 4
        L.prop("bin", fx + math.cos(a) * 19.2, fy + math.sin(a) * 19.2, z, math.degrees(a))
    # the stage on the square's east side, facing the sea
    sg = P.spots.get("S-STAGE")
    if sg:
        gx, gy, _ = sg["map"]
        L.prop("stage", gx, gy, z, sg["yaw"])
        L.d["lights"].append({"at": L.ue(gx + 2.0, gy, z + 5.0), "area": "stage", "kind": "stage"})
        L.spot("S-STAGE", "stage", gx, gy, z + 1.1, sg["yaw"], seats=0, name_ru=sg.get("name_ru"),
               name_en=sg.get("name_en"), audience=L.ue(gx + 18.0, gy, z))
    ss = P.spots.get("S-SQUARE")
    pts = [(fx + math.cos(2 * math.pi * j / 24) * 12.0, fy + math.sin(2 * math.pi * j / 24) * 12.0, z) for j in range(25)]
    L.spot("S-SQUARE", "square", fx, fy, z, sf["yaw"], seats=(ss or {}).get("seats", 40) or 40, path=pts, sub="crowd",
           name_ru=(ss or {}).get("name_ru"), name_en=(ss or {}).get("name_en"))
    L.spot("S-SQUARE-AUDIENCE", "square", (gx + 20.0) if sg else fx, gy if sg else fy, z, 180.0, seats=300,
           sub="audience")


def flowerbed(L, x, y, z, r, seed):
    """A round raised bed: a granite kerb (prop), flowers inside (procedural plants of the season)."""
    L.prop("flowerbed", x, y, z, 0.0, r=r)
    rnd = random.Random(seed * 7 + 3)
    zz = z + 0.32
    for j in range(10):
        a = 2 * math.pi * j / 10 + rnd.uniform(-0.1, 0.1)
        L.plant("Marigold" if j % 2 else "Aster", x + math.cos(a) * r * 0.74, y + math.sin(a) * r * 0.74, zz, 1.0)
    for j in range(5):
        a = 2 * math.pi * j / 5 + 0.4
        L.plant("Cosmos", x + math.cos(a) * r * 0.4, y + math.sin(a) * r * 0.4, zz, 1.0)
    L.plant("DayLily", x, y, zz, 2.0)


# --- the seaside park ----------------------------------------------------------------------------------------------
def park(L):
    P = L.P
    y0, y1, s0, s1 = P.plot_rect("P-PARK")
    z = P.tz["T1"]
    club = P.plot_rect("P-CLUB2") if "P-CLUB2" in P.plots else None

    def blocked(s, y, pad=3.0):
        return club and club[0] - pad < y < club[1] + pad and club[2] - pad < s < club[3] + pad

    def path(a, b, w=3.2, skin="gravel"):
        n = max(1, int(math.hypot(b[0] - a[0], b[1] - a[1]) / 8.0))
        for k in range(n):
            u0, u1 = k / n, (k + 1) / n
            sc, yc = a[0] + (b[0] - a[0]) * (u0 + u1) / 2, a[1] + (b[1] - a[1]) * (u0 + u1) / 2
            if blocked(sc, yc, 0.0):
                continue
            (ax, ay), (bx, by) = at(a[0] + (b[0] - a[0]) * u0, a[1] + (b[1] - a[1]) * u0), \
                at(a[0] + (b[0] - a[0]) * u1, a[1] + (b[1] - a[1]) * u1)
            L.box("path", (ax + bx) / 2, (ay + by) / 2, math.degrees(math.atan2(by - ay, bx - ax)),
                  math.hypot(bx - ax, by - ay) + w * 0.6, w, z - 0.3, z + 0.03, skin)

    sm = (s0 + s1) / 2
    pond = P.spots.get("S-POND")
    play = P.spots.get("S-PLAYGROUND")
    ps, py_ = (W.inland(*pond["map"][:2]), pond["map"][1]) if pond else (sm + 15, y1 - 110)
    gs, gy = (W.inland(*play["map"][:2]), play["map"][1]) if play else (sm - 25, y0 + 80)
    # the loop round the park, the long avenue down its middle, cross paths from the fair and to the club
    loop = [(s0 + 8, y0 + 8), (s1 - 8, y0 + 8), (s1 - 8, y1 - 8), (s0 + 8, y1 - 8), (s0 + 8, y0 + 8)]
    for a, b in zip(loop, loop[1:]):
        path(a, b)
    path((sm, y0 - 2.0), (sm, y1 + 2.0), 5.0, "square")
    for yy in (y0 + 75.0, y0 + 225.0):
        path((s0 - 2.0, yy), (s1 + 2.0, yy), 3.6)
    L.d["areas"].append({"id": "park", "name": "place.park", "at": L.ue(*at(sm, (y0 + y1) / 2), z), "r": 160.0})
    # the pond: an oval of still water in a stone kerb, ducks; ice in winter
    rx, ry = 14.0, 22.0
    px, py = at(ps, py_)
    L.d["waters"].append({"id": "pond", "at": L.ue(px, py, z + 0.22), "r": [rx, ry], "yaw": sea_yaw(py_),
                          "kerb": 0.35, "ice": True})
    L.prop("pond", px, py, z, sea_yaw(py_), r=[rx, ry])
    psp = P.spots["S-POND"] if pond else {}
    slots = []
    for j in range(10):
        a = 2 * math.pi * j / 10
        qx, qy = px + math.cos(a) * (rx + 0.25), py + math.sin(a) * (ry + 0.25)
        slots.append({"pos": L.ue(qx, qy, z + 0.4), "yaw": round(math.degrees(math.atan2(math.sin(a), math.cos(a))), 2),
                      "height": 0.4})
    L.spot("S-POND", "pond", px, py, z, sea_yaw(py_), seats=10, slots=slots,
           path=[(px + math.cos(2 * math.pi * j / 20) * (rx + 3.5), py + math.sin(2 * math.pi * j / 20) * (ry + 3.5), z)
                 for j in range(21)], name_ru=psp.get("name_ru"), name_en=psp.get("name_en"))
    for j in range(20):
        a = 2 * math.pi * (j + 0.5) / 20
        L.box("path", px + math.cos(a) * (rx + 3.5), py + math.sin(a) * (ry + 3.5),
              math.degrees(math.atan2(math.cos(a) * ry, -math.sin(a) * rx)), 8.2, 3.0, z - 0.3, z + 0.03, "gravel")
    for j in range(6):
        a = 2 * math.pi * (j + 0.25) / 6
        L.bench(L.uid("S-BENCH-PK"), px + math.cos(a) * (rx + 6.2), py + math.sin(a) * (ry + 6.2), z,
                math.degrees(a) + 180.0, area="park", terrace="T1")
    for j in range(4):
        a = 2 * math.pi * j / 4 + 0.3
        L.lamp(px + math.cos(a) * (rx + 6.0), py + math.sin(a) * (ry + 6.0), z, kind="lamp_park", area="park")
    for k, sp in enumerate(("WeepingBirch", "WeepingBirch", "BlackAlder", "WeepingBirch")):
        a = 2 * math.pi * k / 4 + 1.0
        L.tree(sp, px + math.cos(a) * (rx + 10.0), py + math.sin(a) * (ry + 9.0), z, L.rnd.uniform(18, 30))
    # the playground: a sand pit, swings, a slide, a climbing frame and a seesaw on a rubber floor
    qx, qy = at(gs, gy)
    L.box("playground", qx, qy, sea_yaw(gy) + 90.0, 24.0, 18.0, z - 0.3, z + 0.04, "rubber")
    for kind, ds, dy in (("sandpit", -4.0, -6.0), ("swings", 4.0, -6.0), ("slide", 4.0, 5.0), ("climber", -4.0, 5.0),
                         ("seesaw", 0.0, 0.0)):
        x, yy = at(gs + ds, gy + dy)
        L.prop(kind, x, yy, z + 0.04, sea_yaw(gy))
    plp = P.spots.get("S-PLAYGROUND", {})
    L.spot("S-PLAYGROUND", "playground", qx, qy, z, sea_yaw(gy), seats=8, name_ru=plp.get("name_ru"),
           name_en=plp.get("name_en"))
    for k, (ds, dy) in enumerate(((-11.0, -6.0), (-11.0, 0.0), (-11.0, 6.0))):
        bx, by = at(gs + ds, gy + dy)
        L.bench(L.uid("S-BENCH-PK"), bx, by, z, sea_yaw(gy) + 180.0, area="playground", terrace="T1",
                sub="parents")
    # benches and lamps along the loop and the avenue
    for a, b in zip(loop, loop[1:]):
        n = int(math.hypot(b[0] - a[0], b[1] - a[1]) / 12.0)
        for k in range(1, n):
            u = k / n
            s, y = a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u
            if blocked(s, y) or math.hypot(s - ps, y - py_) < ry + 6 or math.hypot(s - gs, y - gy) < 16.0:
                continue
            ds, dy = b[0] - a[0], b[1] - a[1]
            ln = math.hypot(ds, dy)
            ns, ny = -dy / ln, ds / ln                    # left of the path, toward the park's middle for this loop
            if k % 2 == 0:
                bx, by = at(s + ns * 2.6, y + ny * 2.6)
                fx_, fy_ = at(s, y)
                L.bench(L.uid("S-BENCH-PK"), bx, by, z, math.degrees(math.atan2(fy_ - by, fx_ - bx)), area="park",
                        terrace="T1")
            else:
                L.lamp(*at(s - ns * 2.2, y - ny * 2.2), z, kind="lamp_park", area="park")
    for k in range(1, int((y1 - y0) / 20.0)):
        y = y0 + k * 20.0
        if math.hypot(sm - ps, y - py_) < ry + 4:
            continue
        L.lamp(*at(sm + (3.4 if k % 2 else -3.4), y), z, kind="lamp_park", area="park")
    for sid in ("S-BENCH-03", "S-BENCH-04"):
        s = P.spots.get(sid)
        if s:
            x, yy, _ = s["map"]
            L.bench(sid, x, yy, z, s["yaw"], area="park", terrace="T1", name_ru=s.get("name_ru"),
                    name_en=s.get("name_en"))
    # the stroll: the loop
    pts = []
    for a, b in zip(loop, loop[1:]):
        for k in range(6):
            u = k / 6
            pts.append((*at(a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u), z + 0.03))
    pk = P.spots.get("S-PARK", {})
    L.spot("S-PARK", "park", *at(sm, (y0 + y1) / 2), z, sea_yaw((y0 + y1) / 2), seats=pk.get("seats", 30), path=pts + [pts[0]],
           sub="stroll", name_ru=pk.get("name_ru"), name_en=pk.get("name_en"))
    # trees: the avenue of maples, rowan and birch groups on the lawns, apples in an orchard corner, poplars along the
    # inland edge; shrubs and a privet hedge on the fair's side
    for k in range(int((y1 - y0) / 12.0)):
        y = y0 + 6.0 + k * 12.0
        if math.hypot(sm - ps, y - py_) < ry + 10:
            continue
        for side in (-1, 1):
            L.tree("Maple", *at(sm + side * 5.0, y), z, L.rnd.uniform(20, 38))
    groups = [((s0 + 28, y0 + 40), "Rowan", 5), ((s1 - 25, y0 + 45), "SilverBirch", 6), ((s0 + 30, y0 + 150), "Rowan", 4),
              ((s1 - 26, y0 + 150), "WeepingBirch", 4), ((s0 + 28, y1 - 40), "Apple", 8), ((s1 - 28, y1 - 30), "SilverBirch", 5)]
    for (gs_, gy_), sp, n in groups:
        for k in range(n):
            a = L.rnd.uniform(0, 2 * math.pi)
            rr = L.rnd.uniform(2.5, 10.0)
            s, y = gs_ + math.cos(a) * rr, gy_ + math.sin(a) * rr
            if blocked(s, y) or math.hypot(s - ps, y - py_) < ry + 4 or math.hypot(s - gs, y - gy) < 15:
                continue
            L.tree(sp, *at(s, y), z, L.rnd.uniform(12, 30))
    for k in range(int((y1 - y0) / 25.0)):
        y = y0 + 12.0 + k * 25.0
        if not blocked(s1 - 2.5, y):
            L.tree("PyramidalPoplar", *at(s1 - 2.5, y), z, L.rnd.uniform(25, 35))
    for k in range(int((y1 - y0) / 4.0)):
        y = y0 + 2.0 + k * 4.0
        if abs(y - (y0 + 75.0)) < 4 or abs(y - (y0 + 225.0)) < 4 or blocked(s0 + 2.0, y):
            continue
        L.plant("Privet", *at(s0 + 2.0, y), z, 6.0)
    for (gs_, gy_) in ((s0 + 20, y0 + 20), (s1 - 20, y0 + 20), (s0 + 20, y1 - 20), (s1 - 20, y1 - 20)):
        for k in range(3):
            L.plant("Spiraea", *at(gs_ + k * 1.6, gy_ + (k % 2) * 1.4), z, 5.0)
    for side in (-1, 1):
        flowerbed(L, *at(sm + side * 14.0, y0 + 75.0), z, 2.6, 10 + side)


# --- the fair ------------------------------------------------------------------------------------------------------
def fair(L):
    P = L.P
    y0, y1, s0, s1 = P.plot_rect("P-FAIR")
    z = P.tz["T1"] + PAVE
    L.pave("fair", s0, s1, y0, y1, z - PAVE, "square", tile=8.0)
    yc = (y0 + y1) / 2
    L.d["areas"].append({"id": "fair", "name": "place.fair", "at": L.ue(*at((s0 + s1) / 2, yc), z), "r": 80.0})
    goods = ["honey", "smoked_fish", "pastry", "toys", "pickles", "flowers", "amber", "tea", "berries", "knits",
             "pottery", "corn"]
    n = 0
    lane = s0 + 30.0
    for (s, face) in ((lane - 5.5, 1), (lane + 5.5, -1), (s1 - 6.0, -1)):
        y = y0 + 8.0
        while y < y1 - 8.0:
            if s == s1 - 6.0 and abs(y - yc) < 22.0:
                y += 7.0
                continue
            x, py = at(s, y)
            yaw = sea_yaw(y) + (0.0 if face > 0 else 180.0)
            sid = "S-FAIR-STALL%02d" % (n + 1)
            g = goods[n % len(goods)]
            L.prop("stall", x, py, z, yaw, variant=n % 4, goods=g, id=sid)
            a = math.radians(yaw)
            fx, fy = math.cos(a), math.sin(a)
            L.spot(sid, "fair", x + fx * 1.5, py + fy * 1.5, z, yaw + 180.0, seats=3, sub="customers",
                   seller={"pos": L.ue(x - fx * 0.7, py - fy * 0.7, z), "yaw": round(yaw, 2)}, goods=g)
            n += 1
            y += 7.0
    # a carousel in the open middle of the far row, lamps along the lane, benches at its ends
    cx, cy = at(s1 - 12.0, yc)
    L.prop("carousel", cx, cy, z, 0.0)
    L.d["lights"].append({"at": L.ue(cx, cy, z + 5.5), "area": "fair", "kind": "carousel"})
    for k in range(7):
        L.lamp(*at(lane, y0 + 10.0 + k * 22.0), z, area="fair")
    for yy, sgn in ((y0 + 3.0, 1), (y1 - 3.0, -1)):
        for k in range(2):
            L.bench(L.uid("S-BENCH-FR"), *at(lane - 4.0 + k * 8.0, yy), z, sea_yaw(yy) + 90.0 * sgn, area="fair",
                    terrace="T1")
        L.prop("bin", *at(lane, yy), z, 0.0)
    fp = P.spots.get("S-FAIR", {})
    pts = [(*at(lane, y0 + 4.0), z), (*at(lane, y1 - 4.0), z)]
    L.spot("S-FAIR", "fair", *at(lane, yc), z, sea_yaw(yc), seats=fp.get("seats", 30) or 30, path=pts, sub="crowd",
           name_ru=fp.get("name_ru"), name_en=fp.get("name_en"))


# --- the old stair up to the club ---------------------------------------------------------------------------------
def club_stair(L):
    """L-CLUB-STAIR: from the town's south end up the spur's north slope to the club's alley. Steps of 15 cm follow
    the slope where it is steep, landings where it is gentle; a railing each side, low lamps every ~12 m."""
    lk = L.P.links.get("L-CLUB-STAIR")
    if not lk:
        return
    (bx, by, bz), (tx, ty, tz) = lk["bottom"], lk["top"]
    ln = math.hypot(tx - bx, ty - by)
    ux, uy = (tx - bx) / ln, (ty - by) / ln
    yaw = math.degrees(math.atan2(uy, ux))
    width = 2.6
    # the walking line: the ground smoothed over 5 m, never falling on the way up, from bz to tz
    N = int(ln / 0.25)
    gz = [ground(bx + ux * ln * i / N, by + uy * ln * i / N) for i in range(N + 1)]
    sm = []
    for i in range(N + 1):
        lo, hi = max(0, i - 10), min(N, i + 10)
        sm.append(sum(gz[lo:hi + 1]) / (hi - lo + 1))
    sm[0], sm[-1] = bz, tz
    for i in range(1, N + 1):
        sm[i] = max(sm[i], sm[i - 1])
    rise = 0.15
    i = 0
    level = bz
    k = 0
    steps = 0
    while i < N:
        # how far this tread runs: until the line climbs a riser over it (at least 0.34 m, at most a landing of 3 m)
        j = i + 1
        while j < N and sm[j] < level + rise * 0.5 and (j - i) * 0.25 < 3.0:
            j += 1
        j = max(j, i + 2) if j - i < 2 and j < N else j
        d0, d1 = ln * i / N, ln * min(j, N) / N
        cx, cy = bx + ux * (d0 + d1) / 2, by + uy * (d0 + d1) / 2
        low = min(gz[i:j + 1]) - 0.4
        L.box("club_stair", cx, cy, yaw, d1 - d0 + 0.02, width, min(low, level - 0.3), level, "granite")
        steps += 1
        if k % 40 == 20:
            for side in (-1, 1):
                L.lamp(cx - uy * side * (width / 2 + 0.3), cy + ux * side * (width / 2 + 0.3), level, kind="lamp_park",
                       area="club_stair")
        if k % 3 == 0:
            for side in (-1, 1):
                L.prop("railing_post", cx - uy * side * (width / 2 - 0.05), cy + ux * side * (width / 2 - 0.05), level,
                       yaw)
        k += 1
        i = j
        level = min(tz, level + rise) if i < N else tz
    L.d["club_stair"] = {"steps": steps, "bottom": L.ue(bx, by, bz), "top": L.ue(tx, ty, tz)}


def build():
    plan = Plan()
    L = Layout(plan)
    promenade(L)
    beach(L)
    square(L)
    park(L)
    fair(L)
    club_stair(L)
    # the plan's other spots we don't furnish (cascade, lookouts, bonfire, pier head, the upper benches): passed on
    have = {s["id"] for s in L.d["spots"]}
    for sid, s in plan.spots.items():
        if sid in have:
            continue
        x, y, z = s["map"]
        L.spot(sid, s["kind"], x, y, z, s["yaw"], seats=s.get("seats", 0), name_ru=s.get("name_ru"),
               name_en=s.get("name_en"), notes=s.get("notes", ""), furnished=False)
    doc = {"_doc": "Built by tools/public_spaces.py from Content/City/city_plan.json (docs/city/public_spaces.md); "
                   "do not edit by hand. Unreal metres, yaw Unreal degrees (0 = +x = the sea). boxes: solid blocks "
                   "(c = centre, size = along yaw, across, height); props: instanced kit meshes; plants: single "
                   "procedural plants; trees: one AProceduralForest; lights: lamp heads that take a light from the "
                   "pool at night; waters: still water (the pond); spots: ULyudPublicSpotsSubsystem, also written to "
                   "PeopleData/town_plan.json.",
           "version": 2, "frame": "unreal_metres", "face_y": plan.face_y}
    doc.update(L.d)
    return doc


def write(doc):
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(doc, f, ensure_ascii=False, separators=(",", ":"))
        f.write("\n")
    plan = {}
    if os.path.exists(TOWN_PLAN):
        with open(TOWN_PLAN) as f:
            plan = json.load(f)
    plan["_doc_spots"] = ("spots: written by tools/public_spaces.py (the public spaces' layout: benches, the fountain's "
                          "rim, the promenade rail, the beach, the fair's stalls, crowd places, the plan's own S-* "
                          "spots). id, kind, pos [x,y,z] Unreal metres, yaw Unreal degrees, seats; slots = seat "
                          "points (height = seat over the ground); path = a stroll polyline; furnished false = only "
                          "passed on from city_plan.json.")
    plan["spots"] = doc["spots"]
    with open(TOWN_PLAN, "w") as f:
        json.dump(plan, f, ensure_ascii=False, indent=1)
        f.write("\n")


def png(doc, path):
    from PIL import Image, ImageDraw
    pts = [b["c"] for b in doc["boxes"] if b["kind"] != "club_stair"] + [p["at"] for p in doc["props"]]
    x0, x1 = min(p[0] for p in pts) - 10, max(p[0] for p in pts) + 10
    y0, y1 = min(p[1] for p in pts) - 10, max(p[1] for p in pts) + 10
    k = 1.5
    im = Image.new("RGB", (int((y1 - y0) * k), int((x1 - x0) * k)), (222, 214, 190))
    dr = ImageDraw.Draw(im)

    def px(x, y):          # north to the right, the sea at the top
        return ((y - y0) * k, (x1 - x) * k)

    col = {"deck": (205, 205, 200), "square": (215, 205, 190), "gravel": (190, 170, 140), "granite": (120, 120, 125),
           "rubber": (170, 70, 60)}
    for b in doc["boxes"]:
        cx, cy = b["c"][0], b["c"][1]
        a = math.radians(b["yaw"])
        hx, hy = b["size"][0] / 2, b["size"][1] / 2
        q = [(cx + math.cos(a) * u - math.sin(a) * v, cy + math.sin(a) * u + math.cos(a) * v)
             for u, v in ((-hx, -hy), (hx, -hy), (hx, hy), (-hx, hy))]
        dr.polygon([px(*p) for p in q], fill=col.get(b["skin"], (180, 180, 180)))
    pc = {"lamp": (240, 180, 0), "lamp_park": (240, 180, 0), "railing": (40, 40, 40), "umbrella": (220, 40, 40),
          "stall": (200, 80, 160), "bench_park": (30, 110, 40), "bench_promenade": (30, 110, 40)}
    for p in doc["props"]:
        x, y = px(p["at"][0], p["at"][1])
        dr.ellipse((x - 1.5, y - 1.5, x + 1.5, y + 1.5), fill=pc.get(p["kind"], (90, 90, 90)))
    for t in doc["trees"]:
        x, y = px(t["at"][0], t["at"][1])
        dr.ellipse((x - 3, y - 3, x + 3, y + 3), outline=(20, 110, 20))
    for f in doc["fountains"]:
        x, y = px(f["at"][0], f["at"][1])
        r = f["stone"]["rim_r"] * k
        dr.ellipse((x - r, y - r, x + r, y + r), outline=(40, 90, 200), width=2)
    for w in doc["waters"]:
        x, y = px(w["at"][0], w["at"][1])
        dr.ellipse((x - w["r"][1] * k, y - w["r"][0] * k, x + w["r"][1] * k, y + w["r"][0] * k), fill=(90, 140, 200))
    im.save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--png")
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    doc = build()
    if not a.dry:
        write(doc)
    kinds = {}
    for p in doc["props"]:
        kinds[p["kind"]] = kinds.get(p["kind"], 0) + 1
    print("public spaces: %d boxes, %d props %s, %d plants, %d trees, %d lights, %d spots, club stair %s" % (
        len(doc["boxes"]), len(doc["props"]), kinds, len(doc["plants"]), len(doc["trees"]), len(doc["lights"]),
        len(doc["spots"]), doc.get("club_stair", {}).get("steps")))
    if a.png:
        png(doc, a.png)


if __name__ == "__main__":
    main()
