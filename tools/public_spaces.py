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

import terrain as T  # noqa: E402

_TERRAIN_OUT = os.path.join(T.default_dir(), "terrain_out.bin")
if not os.path.exists(_TERRAIN_OUT):
    sys.stderr.write("\n*** public_spaces.py: WARNING: no %s - heights come from the macro shape, not the built ground.\n"
                     "*** Copy or build the terrain first (the beach, the club stair and the cascade's rills follow it);\n"
                     "*** --strict refuses to write without it.\n\n" % _TERRAIN_OUT)


def ground(x, y):
    return T.ground_z(x, y)

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


def stand(L, x, y, z, yaw, kind="stand"):
    return {"pos": L.ue(x, y, z), "yaw": round(yaw, 2), "height": 0.0, "kind": kind}


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
        for (sa, sb, skin) in ((C.SEAWALL_S + 0.03, 44.0, "deck"), (44.0, 47.0, "granite"), (47.0, w1(yc), "deck")):
            cx, cy = at((sa + sb) / 2, yc)
            L.box("promenade", cx, cy, sea_yaw(yc) + 90.0, dy + 0.03, sb - sa, z - PAVE - SLAB_DEPTH, z, skin)
        y += dy
    # no railing on the seawall: its 0.45 m kerb (the plan lead's city_mesh.py, s 25.0..25.45) is the parapet and a
    # seat; people sit on it facing the sea, three to every 2.5 m
    kz = P.tz["T0"] + 0.45
    y = y0 + 4.0
    while y < y1 - 4.0:
        run = [v for v in (y + k * 0.8 for k in range(12)) if L.free(C.SEAWALL_S + 0.2, v, 1.0) and abs(v - 905.0) > 7.5
               and C.CITY_Y0 + 1.0 < v < C.CITY_Y1 - 1.0]
        if len(run) >= 4:
            slots = [{"pos": L.ue(*at(C.SEAWALL_S + 0.22, v), kz), "yaw": sea_yaw(v), "height": 0.45} for v in run]
            x, yy = at(C.SEAWALL_S + 0.22, run[len(run) // 2])
            L.spot(L.uid("S-KERB-"), "promenade", x, yy, P.tz["T0"], sea_yaw(yy), seats=len(slots), slots=slots,
                   sub="kerb", terrace="T0")
        y += 20.0
    # lamps: the sea row every 30 m by the railing, the inner row offset by half a span
    y = y0 + 10.0
    k = 0
    while y < y1 - 10.0:
        if L.free(C.SEAWALL_S + 1.2, y, 1.0) and abs(y - 905.0) > 8.0:
            L.lamp(*at(C.SEAWALL_S + 1.2, y), z, area="promenade")
        if L.free(52.0, y + 15.0, 1.0):
            L.lamp(*at(52.0, y + 15.0), z, area="promenade")
        y += 30.0
        k += 1
    # the plan's benches (S-BENCH-P*): backs to W1, facing the sea; a pair by the railing between them
    for sid, s in sorted(P.spots.items()):
        if sid.startswith("S-BENCH-P"):
            x, yy, _ = s["map"]
            L.bench(sid, x, yy, z, s["yaw"], style="promenade", terrace="T0", area="promenade",
                    name_ru=s.get("name_ru"), name_en=s.get("name_en"))
    # more by W1, halfway between the plan's (the grannies fill these first: backs to the shops, the sea in front)
    for sid, sp_ in sorted(P.spots.items()):
        if sid.startswith("S-BENCH-P") and sid[9:].isdigit():
            bx0, by0, _ = sp_["map"]
            yy = by0 + 20.0
            if yy < y1 - 6.0 and L.free(62.0, yy, 1.5) and abs(yy - 905.0) > 36.0:
                L.bench(L.uid("S-BENCH-PM"), *at(62.0, yy), z, sea_yaw(yy), style="promenade", terrace="T0", area="promenade")
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
            slots = [{"pos": L.ue(*at(C.SEAWALL_S + 0.8, y + o), z), "yaw": sea_yaw(y), "height": 0.0, "kind": "stand"}
                     for o in (-1.8, -0.6, 0.6, 1.8)]
            L.spot(L.uid("S-RAIL-"), "promenade", x, yy, z, sea_yaw(y), seats=4, slots=slots, sub="rail", terrace="T0")
        y += 40.0
    # trees in grates down the middle band, a maple every 20 m; bins by every other plan bench
    y = y0 + 10.0
    while y < y1 - 5.0:
        if L.free(45.5, y, 2.0) and abs(y - 905.0) > 14.0:
            x, yy = at(45.5, y)
            L.tree("Maple", x, yy, z, L.rnd.uniform(18, 32))
            L.prop("tree_grate", x, yy, z, sea_yaw(y))
        y += 20.0
    # small round beds in the granite band, every 120 m halfway between two maples
    y = y0 + 70.0
    k = 0
    while y < y1 - 20.0:
        if L.free(45.5, y, 2.0) and abs(y - 905.0) > 16.0:
            flowerbed(L, *at(45.5, y), z, 1.25, 20 + k, outer=5, inner=0)
            k += 1
        y += 120.0
    for k, (sid, s) in enumerate(sorted((i, s) for i, s in P.spots.items() if i.startswith("S-BENCH-P"))):
        if k % 2 == 0:
            x, yy, _ = s["map"]
            bx, by = at(W.inland(x, yy), yy + 1.7)
            L.prop("bin", bx, by, z, s["yaw"])
    # the stroll lines along it: by the sea, down the middle (the plan's S-PROMENADE), by the town (clear of the kiosks
    # and the W1 stairs' feet); each with stand slots every ~10 m
    sp = P.spots.get("S-PROMENADE", {})
    for sid, s_line in (("S-PROMENADE-SEA", C.SEAWALL_S + 5.0), ("S-PROMENADE", 40.0), ("S-PROMENADE-TOWN", 57.0)):
        pts = [(*at(s_line, yy), z) for yy in range(int(y0) + 5, int(y1) - 4, 15) if L.free(s_line, yy, 0.5)]
        slots = [{"pos": L.ue(*at(s_line, yy), z), "yaw": round(sea_yaw(yy) + 90.0, 2), "height": 0.0, "kind": "stand"}
                 for yy in range(int(y0) + 10, int(y1) - 4, 10) if L.free(s_line, yy, 1.0)]
        extra = {"name_ru": sp.get("name_ru"), "name_en": sp.get("name_en")} if sid == "S-PROMENADE" else {}
        L.spot(sid, "promenade", *at(s_line, 905.0), z, sea_yaw(905.0) + 90.0, seats=len(slots), path=pts, slots=slots,
               sub="stroll", terrace="T0", **extra)
    L.d["areas"].append({"id": "promenade", "name": "place.promenade.name", "at": L.ue(*at(45.0, 905.0), z), "r": 540.0})


# --- the beach ---------------------------------------------------------------------------------------------------
def beach(L):
    P = L.P
    sb = P.spots.get("S-BEACH")
    if sb:
        x, y, zz = sb["map"]
        slots = []
        for k in range(sb["seats"]):
            yy = 610.0 + (k * 13.3) % 780.0
            ss = 8.0 + (k * 7) % 4 * 1.2
            if abs(yy - 905.0) < 14.0:
                yy += 28.0
            qx, qy = at(ss, yy)
            slots.append({"pos": L.ue(qx, qy, ground(qx, qy) + 0.02), "yaw": sea_yaw(yy), "height": 0.0, "kind": "sit_ground"})
        L.spot("S-BEACH", "beach", x, y, ground(x, y), sb["yaw"], seats=len(slots), slots=slots, name_ru=sb.get("name_ru"),
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
        x, py = at(9.0, yy)
        gz = ground(x, py)
        slots = [{"pos": L.ue(*at(9.0, yy + o), ground(*at(9.0, yy + o)) + 0.02), "yaw": sea_yaw(yy), "height": 0.0,
                  "kind": "lie"} for o in (-0.6, 0.6)]
        L.spot(L.uid("S-TOWEL-"), "beach", x, py, gz, sea_yaw(yy), seats=2, slots=slots, sub="towel", season="summer")
    # year round: changing booths by every other flight of sea steps
    for k, b in enumerate(b for b in P.bays.get("SEA", []) if b["kind"] == "sea_steps"):
        if k % 2 == 0 and 560.0 < b["mid"][1] < 1420.0:
            y = b["mid"][1] + 9.0
            x, py = at(21.5, y)
            L.prop("changing_booth", x, py, ground(x, py), sea_yaw(y))
    L.d["areas"].append({"id": "beach", "name": "place.beach.name", "at": L.ue(*at(12.0, 1000.0), W.SEA_Z + 1.2), "r": 450.0})


# --- planters along the top of W1 (the lower town's edge over the promenade) ---------------------------------------
def w1_planters(L):
    """Long low planters on T1 just behind W1's parapet, every 45 m where no stair, grand flight or plot is: spiraea and
    day lilies over the promenade. Clear of the plan lead's street-tree strips (those stand in front of W2..W5)."""
    P = L.P
    z = P.tz["T1"]
    y = C.CITY_Y0 + 20.0
    k = 0
    rects = []
    for pid, p in P.plots.items():
        yy = p["map"][1]
        sp = W.inland(p["map"][0], yy)
        rects.append((yy - p["w"] / 2 - 3, yy + p["w"] / 2 + 3, sp - p["d"] / 2 - 3, sp + p["d"] / 2 + 3))
    near_stair = [b["mid"][1] for b in P.bays.get("W1", []) if b["kind"] in ("stair", "grand")]
    while y < C.CITY_Y1 - 20.0:
        s = C.wall_s(0, y) + 2.4
        if not any(abs(y - sy) < 12.0 for sy in near_stair) and not any(a < y < b and c < s < d for a, b, c, d in rects):
            x, yy = at(s, y)
            yaw = sea_yaw(y) + 90.0
            L.box("planter", x, yy, yaw, 6.0, 1.1, z - 0.2, z + 0.45, "granite")
            a = math.radians(yaw)
            for j in range(3):
                o = (j - 1) * 1.9
                L.plant("Spiraea" if j != 1 else ("DayLily" if k % 2 else "Aster"), x + math.cos(a) * o, yy + math.sin(a) * o, z + 0.45,
                        4.0 if j != 1 else 2.0)
            k += 1
        y += 45.0


# --- Sun Square --------------------------------------------------------------------------------------------------
FOUNTAIN_STONE = {
    # Sun fountain: an 18 m round basin whose rim is a bench, a two-tier centrepiece; ALyudFountain's water reads the
    # levels (metres over the foot), ALyudPublicSpaces builds the stone from the same numbers
    "lower_level": 0.40, "lower_r": 8.55, "lower_floor": 0.06, "rim_r": 9.0, "rim_z": 0.46, "rim_w": 0.45,
    "upper_level": 2.75, "upper_r": 1.9, "upper_floor": 2.58, "lip_r": 2.0, "lip_z": 2.75,
    "nozzle_z": 4.0, "stem_r": 0.48, "neck_r": 0.24,
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
    L.d["areas"].append({"id": "square", "name": "place.square.name", "at": L.ue(fx, fy, z), "r": 60.0})
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
        perf = [stand(L, *at(W.inland(gx, gy) + ds, gy + dy), z + 1.04, sea_yaw(gy)) for ds in (-1.0, -3.0)
                for dy in (-3.0, -1.0, 1.0, 3.0)]
        L.spot("S-STAGE", "stage", gx, gy, z + 1.04, sg["yaw"], seats=len(perf), slots=perf, name_ru=sg.get("name_ru"),
               name_en=sg.get("name_en"), audience=L.ue(gx + 18.0, gy, z))
    ss = P.spots.get("S-SQUARE")
    pts = [(fx + math.cos(2 * math.pi * j / 24) * 12.0, fy + math.sin(2 * math.pi * j / 24) * 12.0, z) for j in range(25)]
    crowd = []
    for j in range(40):
        a = 2 * math.pi * j / 40
        r = 11.0 + (j % 3) * 1.2
        crowd.append(stand(L, fx + math.cos(a) * r, fy + math.sin(a) * r, z, math.degrees(a) + 180.0))
    L.spot("S-SQUARE", "square", fx, fy, z, sf["yaw"], seats=len(crowd), path=pts, slots=crowd, sub="crowd",
           name_ru=(ss or {}).get("name_ru"), name_en=(ss or {}).get("name_en"))
    if sg:
        gs = W.inland(gx, gy)
        obst = [(q["at"][0], q["at"][1] - L.P.face_y) for q in L.d["props"] if q["kind"] not in ("tree_grate",)]
        aud = []
        for row in range(26):
            srow = gs - 8.0 - row * 1.3
            for col in range(-18, 19):
                yy = gy + col * 1.15
                qx, qy = at(srow, yy)
                if math.hypot(qx - fx, qy - fy) < st["rim_r"] + 1.5:
                    continue
                if any(abs(qx - ox) < 1.0 and abs(qy - oy) < 1.0 for ox, oy in obst):
                    continue
                if any(math.hypot(qx - fx - math.cos(2 * math.pi * (j + 0.5) / 4) * 26.0,
                                  qy - fy - math.sin(2 * math.pi * (j + 0.5) / 4) * 26.0) < 4.0 for j in range(4)):
                    continue
                aud.append(stand(L, qx, qy, z, math.degrees(math.atan2(gy - qy, gx - qx))))
                if len(aud) >= 300:
                    break
            if len(aud) >= 300:
                break
        L.spot("S-SQUARE-AUDIENCE", "square", gx + 20.0, gy, z, 180.0, seats=len(aud), slots=aud, sub="audience")


# flowerbed mixes (outer ring, inner ring, middle): each bed takes one by its seed, so the town's beds differ
BED_MIXES = [
    (("Marigold", "Aster"), ("Cosmos",), "DayLily"),
    (("Cosmos", "Marigold"), ("Aster",), "Spiraea"),
    (("Aster", "Cosmos"), ("DayLily",), "DayLily"),
    (("Heather", "Marigold"), ("Cosmos",), "Spiraea"),
]


def flowerbed(L, x, y, z, r, seed, outer=10, inner=5):
    """A round raised bed: a granite kerb (prop), flowers inside (procedural plants of the season)."""
    L.prop("flowerbed", x, y, z, 0.0, r=r)
    rnd = random.Random(seed * 7 + 3)
    zz = z + 0.32
    o, i, mid = BED_MIXES[seed % len(BED_MIXES)]
    for j in range(outer):
        a = 2 * math.pi * j / outer + rnd.uniform(-0.1, 0.1)
        L.plant(o[j % len(o)], x + math.cos(a) * r * 0.74, y + math.sin(a) * r * 0.74, zz, 1.0)
    for j in range(inner):
        a = 2 * math.pi * j / max(inner, 1) + 0.4
        L.plant(i[j % len(i)], x + math.cos(a) * r * 0.4, y + math.sin(a) * r * 0.4, zz, 1.0)
    L.plant(mid, x, y, zz, 2.0 if mid != "Spiraea" else 4.0)


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
    L.d["areas"].append({"id": "park", "name": "place.park.name", "at": L.ue(*at(sm, (y0 + y1) / 2), z), "r": 160.0})
    # the pond: an oval of still water in a stone kerb, ducks; ice in winter
    rx, ry = 14.0, 22.0
    px, py = at(ps, py_)
    # (laid square to the world's axes: the coast is only ~6 degrees off, and the kerb's seats, the ring path and the
    # benches round it are placed in the same frame)
    L.d["waters"].append({"id": "pond", "at": L.ue(px, py, z + 0.22), "r": [rx, ry], "yaw": 0.0, "kerb": 0.35, "ice": True})
    L.prop("pond", px, py, z, 0.0, r=[rx, ry])
    psp = P.spots["S-POND"] if pond else {}
    slots = []
    for j in range(10):
        a = 2 * math.pi * j / 10
        qx, qy = px + math.cos(a) * (rx + 0.25), py + math.sin(a) * (ry + 0.25)
        slots.append({"pos": L.ue(qx, qy, z + 0.36), "yaw": round(math.degrees(math.atan2(math.sin(a), math.cos(a))), 2),
                      "height": 0.36})
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
    kids = [stand(L, *at(gs + ds, gy + dy), z + 0.04, sea_yaw(gy)) for ds, dy in
            ((-4.0, -3.5), (-2.0, -6.0), (2.0, -4.0), (6.0, -6.0), (2.0, 5.0), (-2.0, 3.0), (0.0, 1.5), (-6.0, 5.0))]
    L.spot("S-PLAYGROUND", "playground", qx, qy, z, sea_yaw(gy), seats=len(kids), slots=kids, name_ru=plp.get("name_ru"),
           name_en=plp.get("name_en"))
    for k, (ds, dy) in enumerate(((-11.0, -6.0), (-11.0, 0.0), (-11.0, 6.0), (11.0, -6.0), (11.0, 0.0), (11.0, 6.0))):
        bx, by = at(gs + ds, gy + dy)
        L.bench(L.uid("S-BENCH-PK"), bx, by, z, sea_yaw(gy) + (180.0 if ds < 0 else 0.0), area="playground", terrace="T1",
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
    # the south entrance: a pair facing each other across the avenue's mouth (the grannies' first stop)
    for side in (-1, 1):
        bx, by = at(sm + side * 4.6, y0 + 3.0)
        L.bench(L.uid("S-BENCH-PK"), bx, by, z, sea_yaw(y0) + (180.0 if side > 0 else 0.0), area="park_entrance", terrace="T1")
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
    walkers = [stand(L, q[0], q[1], z + 0.03, 0.0) for q in pts]
    L.spot("S-PARK", "park", *at(sm, (y0 + y1) / 2), z, sea_yaw((y0 + y1) / 2), seats=len(walkers), slots=walkers,
           path=pts + [pts[0]],
           sub="stroll", name_ru=pk.get("name_ru"), name_en=pk.get("name_en"))
    # trees: the avenue of maples, rowan and birch groups on the lawns, apples in an orchard corner, poplars along the
    # inland edge; shrubs and a privet hedge on the fair's side
    for k in range(int((y1 - y0) / 12.0)):
        y = y0 + 6.0 + k * 12.0
        if math.hypot(sm - ps, y - py_) < ry + 10:
            continue
        for side in (-1, 1):
            if not blocked(sm + side * 5.0, y):
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
    for k in range(int((y1 - y0) / 5.0)):
        y = y0 + 2.5 + k * 5.0
        if abs(y - (y0 + 75.0)) < 4 or abs(y - (y0 + 225.0)) < 4 or blocked(s0 + 2.0, y):
            continue
        L.plant("Privet", *at(s0 + 2.0, y), z, 6.0)
    for (gs_, gy_) in ((s0 + 20, y0 + 20), (s1 - 20, y0 + 20), (s0 + 20, y1 - 20), (s1 - 20, y1 - 20)):
        for k in range(3):
            L.plant("Spiraea", *at(gs_ + k * 1.6, gy_ + (k % 2) * 1.4), z, 5.0)
    for side in (-1, 1):
        flowerbed(L, *at(sm + side * 14.0, y0 + 75.0), z, 2.6, 10 + side)
        flowerbed(L, *at(sm + side * 14.0, y0 + 225.0), z, 2.2, 12 + side)
    # a conifer corner (pines and spruce over lingonberry and bilberry), an aspen copse, ferns under the birches,
    # heather and juniper on the pond's inland side
    for k, (sp, n) in enumerate((("ScotsPine", 4), ("NorwaySpruce", 3))):
        for j in range(n):
            a = L.rnd.uniform(0, 2 * math.pi)
            ss, yy = s1 - 22 + math.cos(a) * L.rnd.uniform(2, 9), y1 - 70 + math.sin(a) * L.rnd.uniform(2, 9)
            if not blocked(ss, yy) and math.hypot(ss - ps, yy - py_) > ry + 6:
                L.tree(sp, *at(ss, yy), z, L.rnd.uniform(25, 45))
    for j in range(8):
        ss, yy = s1 - 22 + L.rnd.uniform(-8, 8), y1 - 70 + L.rnd.uniform(-8, 8)
        if not blocked(ss, yy) and math.hypot(ss - ps, yy - py_) > ry + 4:
            L.plant("Lingonberry" if j % 2 else "Bilberry", *at(ss, yy), z, 3.0)
    for j in range(5):
        a = L.rnd.uniform(0, 2 * math.pi)
        ss, yy = s0 + 26 + math.cos(a) * L.rnd.uniform(2, 7), y0 + 110 + math.sin(a) * L.rnd.uniform(2, 7)
        if not blocked(ss, yy) and math.hypot(ss - gs, yy - gy) > 16:
            L.tree("Aspen", *at(ss, yy), z, L.rnd.uniform(14, 28))
    for (gs_, gy_) in ((s1 - 25, y0 + 45), (s1 - 26, y0 + 150), (s1 - 28, y1 - 30)):
        for j in range(4):
            L.plant("Bracken" if j % 2 else "BucklerFern", *at(gs_ + L.rnd.uniform(-5, 5), gy_ + L.rnd.uniform(-5, 5)), z, 3.0)
    for j in range(7):
        a = math.pi * 0.15 + math.pi * 0.7 * j / 6
        L.plant("Heather" if j % 3 else "Juniper", px - math.cos(a) * (rx + 2.0), py + math.sin(a) * (ry + 2.0) * 0.0 + (j - 3) * 3.2, z,
                4.0 if j % 3 else 8.0)


# the fair's stall themes and their goods (items of LyudInventory.cpp: icons and names exist): item, price, count
FAIR_THEMES = {
    "pastry": [("pie", 15, 6), ("cookies", 18, 4), ("bread_loaf", 11, 3)],
    "sweets": [("candy", 7, 10), ("chocolate", 22, 4), ("ice_cream", 28, 6)],
    "cheese": [("cheese_braid", 24, 4), ("cheese_round_small", 38, 2), ("cheese_round_big", 100, 1)],
    "dairy": [("kefir", 13, 6), ("bread_loaf", 12, 3)],
    "corn": [("corn_cob", 14, 8), ("seeds", 9, 6)],
    "tea": [("tea", 12, 8), ("coffee", 25, 6)],
    "amber": [("amber_charm", 85, 3), ("postcard", 4, 10), ("badge", 11, 5)],
    "fruit": [("apple", 6, 12)],
    "drinks": [("drink_baikal", 18, 6), ("drink_buratino", 18, 6), ("drink_dyushes", 18, 6), ("drink_tarkhun", 18, 6),
               ("water", 15, 6)],
    "news": [("daily_paper", 5, 8), ("magazine_science", 38, 3), ("magazine_travel", 38, 3)],
    "birds": [("bird_feed", 9, 6), ("feed_grain", 14, 4), ("feed_peanuts", 28, 4)],
    "snacks": [("chips", 32, 6), ("seeds", 9, 6)],
}


# --- the fair ------------------------------------------------------------------------------------------------------
def fair(L):
    P = L.P
    y0, y1, s0, s1 = P.plot_rect("P-FAIR")
    z = P.tz["T1"] + PAVE
    L.pave("fair", s0, s1, y0, y1, z - PAVE, "square", tile=8.0)
    yc = (y0 + y1) / 2
    L.d["areas"].append({"id": "fair", "name": "place.fair.name", "at": L.ue(*at((s0 + s1) / 2, yc), z), "r": 80.0})
    goods = list(FAIR_THEMES)
    n = 0
    n_staffed = 0
    lane = s0 + 30.0
    for (s, face) in ((lane - 5.5, 1), (lane + 5.5, -1), (s1 - 6.0, -1)):
        y = y0 + 8.0
        while y < y1 - 8.0:
            if s == s1 - 6.0 and (abs(y - yc) < 22.0 or y > y1 - 34.0):
                y += 7.0
                continue
            x, py = at(s, y)
            yaw = sea_yaw(y) + (180.0 if face > 0 else 0.0)    # the lane's rows face it, the far row the promenade
            sid = "S-FAIR-STALL%02d" % (n + 1)
            staffed = s != s1 - 6.0 and abs(y - yc) < 22.0          # the twelve stalls in the lane's middle have sellers
            if staffed:
                g = goods[n_staffed % len(goods)]                   # one of each theme among the staffed
                n_staffed += 1
            else:
                g = goods[(n * 5 + 3) % len(goods)]
            L.prop("stall", x, py, z, yaw, variant=n % 4, goods=g, id=sid)
            L.d.setdefault("fair_goods", {})[sid] = {"theme": g, "title_key": "ui.shop.title_fair_" + g, "staffed": staffed,
                                                     "goods": [{"item": i, "price": pr, "count": c} for i, pr, c in FAIR_THEMES[g]]}
            a = math.radians(yaw)
            fx, fy = math.cos(a), math.sin(a)
            cust = [stand(L, x + fx * 1.5 - fy * o, py + fy * 1.5 + fx * o, z, yaw + 180.0) for o in (-0.8, 0.0, 0.8)]
            L.spot(sid, "fair", x + fx * 1.5, py + fy * 1.5, z, yaw + 180.0, seats=3, slots=cust, sub="customers", staffed=staffed,
                   seller={"pos": L.ue(x - fx * 0.7, py - fy * 0.7, z), "yaw": round(yaw, 2)}, goods=g)
            n += 1
            y += 7.0
    # a carousel in the open middle of the far row, lamps along the lane, benches at its ends
    cx, cy = at(s1 - 12.0, yc)
    L.prop("carousel", cx, cy, z, 0.0)
    L.d.setdefault("rides", []).append({"kind": "carousel", "at": L.ue(cx, cy, z), "yaw": 0.0})
    L.d["lights"].append({"at": L.ue(cx, cy, z + 5.5), "area": "fair", "kind": "carousel"})
    seats = []
    for k in range(8):
        a = 2 * math.pi * k / 8
        seats.append({"pos": L.ue(cx + math.cos(a) * 3.2, cy + math.sin(a) * 3.2, z + 1.18), "yaw": round(math.degrees(a) + 90.0, 2),
                      "height": 0.83, "kind": "ride"})
    L.spot("S-FAIR-CAROUSEL", "fair", cx, cy, z, 0.0, seats=8, slots=seats, sub="ride",
           queue=[stand(L, *at(s1 - 12.0 - 5.6, yc + dy), z, sea_yaw(yc) + 180.0) for dy in (-1.5, -0.5, 0.5, 1.5)],
           notes="the seats at the rest angle: the carousel stands still for boarding 90 s of every 240 s (ALyudFairRides)")
    # the Ferris wheel at the far corner, its plane along the coast
    wx, wy = at(s1 - 14.0, y1 - 24.0)
    wyaw = sea_yaw(y1 - 24.0) + 90.0
    L.prop("wheel", wx, wy, z, wyaw)
    L.d["rides"].append({"kind": "wheel", "at": L.ue(wx, wy, z), "yaw": wyaw})
    L.d["lights"].append({"at": L.ue(wx, wy, z + 9.2), "area": "fair", "kind": "carousel"})
    a = math.radians(sea_yaw(y1 - 24.0))
    L.spot("S-FAIR-WHEEL", "fair", wx, wy, z, sea_yaw(y1 - 24.0), seats=4, sub="ride",
           slots=[{"pos": L.ue(wx + math.cos(a) * 0.6 * sd, wy + math.sin(a) * 0.6 * sd, z + 0.44 + 0.37), "yaw": wyaw + (0 if sd > 0 else 180),
                   "height": 0.37, "kind": "ride"} for sd in (-1, -1, 1, 1)],
           queue=[stand(L, wx + math.cos(a) * (2.4 + k * 0.8), wy + math.sin(a) * (2.4 + k * 0.8), z, sea_yaw(y1 - 24.0) + 180.0)
                  for k in range(4)],
           notes="the lowest gondola at the platform while the wheel stands (90 s of every 240 s)")
    for k in range(7):
        L.lamp(*at(lane, y0 + 10.0 + k * 22.0), z, area="fair")
    for yy, sgn in ((y0 + 3.0, 1), (y1 - 3.0, -1)):
        for k in range(2):
            L.bench(L.uid("S-BENCH-FR"), *at(lane - 4.0 + k * 8.0, yy), z, sea_yaw(yy) + 90.0 * sgn, area="fair",
                    terrace="T1")
        L.prop("bin", *at(lane, yy), z, 0.0)
    fp = P.spots.get("S-FAIR", {})
    pts = [(*at(lane, yy), z) for yy in range(int(y0) + 4, int(y1) - 3, 8)]
    crowd = [stand(L, *at(lane + (1.6 if k % 2 else -1.6), y0 + 6.0 + k * 4.6), z, sea_yaw(yc) + (180.0 if k % 2 else 0.0))
             for k in range(int((y1 - y0 - 12.0) / 4.6))]
    L.spot("S-FAIR", "fair", *at(lane, yc), z, sea_yaw(yc), seats=len(crowd), path=pts, slots=crowd, sub="crowd",
           name_ru=fp.get("name_ru"), name_en=fp.get("name_en"))


# --- the old stair up to the club ---------------------------------------------------------------------------------
def club_stair(L):
    """L-CLUB-STAIR: from the town's south end up the spur's north slope to the club's alley. On the gentle foot the steps
    follow the ground straight towards the top; up the steep face (to ~38 degrees) the stair zigzags in flights of 8
    risers (16.5 x 30 cm) set across the slope so each flight climbs at a walkable ~29 degrees on the ground, with a
    landing at each turn. Posts and a handrail each side, low lamps at the turns."""
    lk = L.P.links.get("L-CLUB-STAIR")
    if not lk:
        return
    (bx, by, bz), (tx, ty, tz) = lk["bottom"], lk["top"]
    width, rise, tread, grade = 2.4, 0.165, 0.30, 0.55

    def grad(x, y):
        e = 0.75
        return ((ground(x + e, y) - ground(x - e, y)) / (2 * e), (ground(x, y + e) - ground(x, y - e)) / (2 * e))

    def tread_box(x, y, z, dx, dy, ln):
        foot = min(ground(x + dx * k * ln / 2 + sx * width / 2, y + dy * k * ln / 2 + sy * width / 2)
                   for k in (-1, 1) for sx, sy in ((-dy, dx), (dy, -dx)))
        L.box("club_stair", x, y, math.degrees(math.atan2(dy, dx)), ln + 0.02, width, min(foot - 0.3, z - 0.3), z, "granite")

    ln = math.hypot(tx - bx, ty - by)
    ux, uy = (tx - bx) / ln, (ty - by) / ln            # along the line, up
    vx, vy = -uy, ux                                   # across it

    def P(t, l):
        return bx + ux * t + vx * l, by + uy * t + vy * l

    t, l = 0.0, 0.0
    z = max(bz, ground(bx, by))
    side, n, flights = 1, 0, 0
    NR = 8                                             # risers in a flight
    Lf = NR * tread                                    # a flight's horizontal run
    while t < ln - 0.5 and n < 1200 and z < tz - 0.05:
        g_here = ground(*P(t, l))
        steep = (ground(*P(t + 2.0, l)) - g_here) / 2.0 > grade * 0.8
        if not steep:
            # the gentle foot: straight on, a step wherever the ground has risen a riser
            t += tread
            x, y = P(t, l)
            if ground(x, y) > z + rise * 0.6:
                z = min(z + rise, tz)
            tread_box(x, y, z, ux, uy, tread)
            n += 1
            if n % 12 == 0:
                for sd in (-1, 1):
                    L.prop("railing_post", x + vx * sd * (width / 2 - 0.05), y + vy * sd * (width / 2 - 0.05), z,
                           math.degrees(math.atan2(uy, ux)))
            continue
        # a flight of 12 risers: how far along the line the ground rises 12 risers decides its angle to the line
        want = min(tz, z + NR * rise)

        def lat(f):
            return Lf * math.sqrt(max(0.0, 1.0 - (f / Lf) ** 2))

        if abs(l + side * lat(0.6 * Lf)) > 6.0:
            side = -side
        # the flight's end must land on the ground: search its forward run with the sideways run it implies, and
        # keep every tread within reach of the ground on the way (no step buried in the slope)
        def cost(f):
            under = max(ground(*P(t + f * k / NR, l + side * lat(f) * k / NR)) - (z + rise * k) for k in range(1, NR + 1))
            ex, ey = P(t + f, l + side * lat(f))
            return max(0.0, under - 0.2) * 4.0 + abs(ground(ex, ey) - want)

        f = min((0.6 + 0.05 * i for i in range(int((Lf - 0.6) / 0.05) + 1)), key=cost)
        sin_a = lat(f) / Lf
        dx, dy = ux * (f / Lf) + vx * side * sin_a, uy * (f / Lf) + vy * side * sin_a
        yaw = math.degrees(math.atan2(dy, dx))
        for k in range(NR):
            t += f / NR
            l += side * Lf * sin_a / NR
            z = min(z + rise, tz)
            x, y = P(t, l)
            # an old stair on a hillside: where the slope bulges over the flight the step sits on it (a taller riser)
            hump = max(ground(x + dx * a + dy * b, y + dy * a - dx * b) for a in (-0.15, 0.15) for b in (-1.1, 0.0, 1.1))
            z = min(max(z, min(hump + 0.03, z + rise * 0.85)), tz)
            tread_box(x, y, z, dx, dy, tread)
            if k % 3 == 1:
                for sd in (-1, 1):
                    L.prop("railing_post", x - dy * sd * (width / 2 - 0.05), y + dx * sd * (width / 2 - 0.05), z, yaw)
            n += 1
            if z >= tz:
                break
        # the landing: a square of the stair's width at the turn, a step on along the line
        t += 0.4
        x, y = P(t, l)
        tread_box(x, y, z, ux, uy, width)
        if flights % 2 == 0:
            L.lamp(*P(t, l - side * (width / 2 + 0.3)), z, kind="lamp_park", area="club_stair")
        flights += 1
        side = -side
    x, y = P(t, l)
    # the last run to the link's top: straight, risers spread evenly over it
    d = math.hypot(tx - x, ty - y)
    if d > 0.4 or z < tz - 0.01:
        k = max(1, int(math.ceil(max(tz - z, 0.0) / rise)), int(d / 0.9))
        z0 = z
        for j in range(1, k + 1):
            qx, qy = x + (tx - x) * j / k, y + (ty - y) * j / k
            tread_box(qx, qy, z0 + (tz - z0) * j / k, (tx - x) / max(d, 1e-3), (ty - y) / max(d, 1e-3), d / k)
            n += 1
        x, y, z = tx, ty, tz
    L.d["club_stair"] = {"steps": n, "flights": flights, "end": L.ue(x, y, z), "top": L.ue(tx, ty, tz),
                         "bottom": L.ue(bx, by, bz)}


# --- the Cascade's water (its stepped channels are the plan lead's: city_plan.json "cascade") -------------------------
def cascade(L):
    """Water down the stepped channels beside the Cascade's grand stairs (W5 at the top to W2), a basin at each foot and
    a rill across each terrace from that basin to the next channel's head; a spring basin over the top one. The game
    draws the sheets with the fountains' stream material (static, the water moves in the material)."""
    cs = sorted(L.P.d.get("cascade", []), key=lambda c: -c["bottom"][2])      # top (W5) first
    if not cs:
        return
    L.d["cascades"] = []
    L.d["rills"] = []
    for i, c in enumerate(cs):
        fx, fy, fz = c["bottom"]
        dx, dy = c["seaward"]                       # down the channel, out over the lower terrace
        ax, ay = c["along"]                         # across it
        w = c["width"]
        L.d["cascades"].append({"id": c["id"], "foot": L.ue(fx, fy, fz), "down": r3((dx, dy)), "across": r3((ax, ay)),
                                "width": w, "steps": c["steps"], "rise": c["rise"], "tread": c["tread"],
                                "bed_below": c["bed_below_stair"]})
        # the basin at the foot: 4.5 m out, the channel's width and its cheeks, a granite kerb 0.35 high
        bl, bw = 4.5, w + 0.8
        bx, by = fx + dx * bl / 2, fy + dy * bl / 2
        yaw = math.degrees(math.atan2(dy, dx))
        L.box("basin_floor", bx, by, yaw, bl, bw, fz - 0.4, fz + 0.03, "basin")
        for side in (-1, 1):
            L.box("basin_kerb", bx + ax * side * (bw / 2 + 0.15), by + ay * side * (bw / 2 + 0.15), yaw, bl + 0.6, 0.3,
                  fz - 0.3, fz + 0.35, "granite")
        L.box("basin_kerb", fx + dx * (bl + 0.15), fy + dy * (bl + 0.15), yaw, 0.3, bw, fz - 0.3, fz + 0.35, "granite")
        L.d["waters"].append({"id": "cascade_" + c["wall"], "at": L.ue(bx, by, fz + 0.27), "rect": [bl, bw], "yaw": yaw,
                              "ring": L.ue(fx + dx * 0.2, fy + dy * 0.2, fz + 0.27), "kind": "basin"})
        sid = "S-CASCADE" if i == len(cs) - 1 else "S-CASCADE-" + c["wall"]
        slots = [{"pos": L.ue(bx + ax * side * (bw / 2 + 0.15) + dx * o, by + ay * side * (bw / 2 + 0.15) + dy * o, fz + 0.35),
                  "yaw": round(math.degrees(math.atan2(ay * side, ax * side)), 2), "height": 0.35}
                 for side in (-1, 1) for o in (-1.4, 0.0, 1.4)]
        sp = L.P.spots.get("S-CASCADE", {}) if sid == "S-CASCADE" else {}
        L.spot(sid, "fountain", bx, by, fz, yaw, seats=len(slots), slots=slots, sub="cascade",
               name_ru=sp.get("name_ru"), name_en=sp.get("name_en"))
        # the rill to the next channel's head, across this foot's terrace: out of the basin's far end, round behind
        # the next head (so it meets the channel from inland, not across the next grand stair's landing)
        if i + 1 < len(cs):
            n = cs[i + 1]
            hx, hy, hz = n["top"]
            ndx, ndy = n["seaward"]
            ex, ey = fx + dx * (bl + 0.3), fy + dy * (bl + 0.3)
            px, py = hx - ndx * 6.0, hy - ndy * 6.0
            z = fz + 0.06
            L.d["rills"].append({"id": "rill_" + c["wall"], "points": [L.ue(ex, ey, z), L.ue(px, py, z), L.ue(hx, hy, hz + 0.02)],
                                 "width": 0.7})
            for (ax0, ay0), (ax1, ay1) in (((ex, ey), (px, py)), ((px, py), (hx, hy))):
                ln = math.hypot(ax1 - ax0, ay1 - ay0)
                ux, uy = (ax1 - ax0) / ln, (ay1 - ay0) / ln
                ya = math.degrees(math.atan2(uy, ux))
                k = int(math.ceil(ln / 6.0))
                for j in range(k):
                    cx, cy = ax0 + ux * ln * (j + 0.5) / k, ay0 + uy * ln * (j + 0.5) / k
                    L.box("rill_bed", cx, cy, ya, ln / k + 0.05, 0.7, fz - 0.3, fz + 0.02, "basin")
                    for side in (-1, 1):
                        L.box("rill_kerb", cx - uy * side * 0.45, cy + ux * side * 0.45, ya, ln / k + 0.05, 0.2,
                              fz - 0.3, fz + 0.12, "granite")
                # a footbridge where the rill passes the next grand stair's landing: a slab every 25 m
                for j in range(1, int(ln / 25.0) + 1):
                    cx, cy = ax0 + ux * j * 25.0, ay0 + uy * j * 25.0
                    L.box("rill_bridge", cx, cy, ya, 1.6, 1.4, fz + 0.12, fz + 0.2, "granite")
    # the spring over the top channel: a square basin on the upper terrace behind its head, water welling up
    t = cs[0]
    hx, hy, hz = t["top"]
    ddx, ddy = t["seaward"]
    sx, sy = hx - ddx * 3.0, hy - ddy * 3.0
    yaw = math.degrees(math.atan2(ddy, ddx))
    L.box("basin_floor", sx, sy, yaw, 4.0, t["width"] + 0.8, hz - 0.3, hz + 0.03, "basin")
    for side in (-1, 1):
        L.box("basin_kerb", sx + t["along"][0] * side * (t["width"] / 2 + 0.55), sy + t["along"][1] * side * (t["width"] / 2 + 0.55),
              yaw, 4.6, 0.3, hz - 0.3, hz + 0.4, "granite")
    L.box("basin_kerb", sx - ddx * 2.15, sy - ddy * 2.15, yaw, 0.3, t["width"] + 1.4, hz - 0.3, hz + 0.4, "granite")
    L.d["waters"].append({"id": "cascade_spring", "at": L.ue(sx, sy, hz + 0.2), "rect": [4.0, t["width"] + 0.8], "yaw": yaw,
                          "ring": L.ue(sx, sy, hz + 0.2), "kind": "spring"})


# --- the pier (its deck is the plan lead's: tools/city_mesh.py pier()) -----------------------------------------------
PIER_Y, PIER_W, PIER_OUT, PIER_HEAD = 905.0, 12.0, -140.0, 26.0


def pier(L):
    """Railings on the deck's edge kerbs and round the head, lamps and benches down the deck; the head's middle is
    the lighthouse's (the buildings thread), its rim the sunset/fishing/fireworks spot."""
    z = L.P.tz["T0"]                                   # the head's top
    zd = z + 0.015                                     # the deck's (city_mesh.py: 1.5 cm over, no shared plane)
    x0, y0 = at(C.SEAWALL_S, PIER_Y)
    x1, y1 = at(PIER_OUT, PIER_Y)
    ln = math.hypot(x1 - x0, y1 - y0)
    nx, ny = (x1 - x0) / ln, (y1 - y0) / ln            # out to sea
    ux, uy = -ny, nx                                   # across
    yaw_out = math.degrees(math.atan2(ny, nx))
    hc = ln + PIER_HEAD / 2 - 2.0
    head0 = hc - PIER_HEAD / 2

    def P(u, d):
        return x0 + ux * u + nx * d, y0 + uy * u + ny * d

    for side in (-1, 1):
        d = 2.0
        while d < head0 - 1.0:
            L.prop("railing", *P(side * (PIER_W / 2 - 0.15), d), z + 0.2, yaw_out)
            d += 2.0
    # round the head: three sides, the landward side open to the deck in its middle
    h = PIER_HEAD / 2
    for k in range(13):
        o = -h + 1.0 + k * 2.0
        L.prop("railing", *P(o, hc + h - 0.1), z, yaw_out + 90.0)
        for side in (-1, 1):
            L.prop("railing", *P(side * (h - 0.1), hc + o), z, yaw_out)
        if abs(o) > PIER_W / 2 + 0.5:
            L.prop("railing", *P(o, head0 + 0.1), z, yaw_out + 90.0)
    d = 12.0
    k = 0
    while d < head0 - 4.0:
        for side in (-1, 1):
            if k % 2 == 0:
                L.lamp(*P(side * (PIER_W / 2 - 0.8), d), zd, area="pier")
            else:
                bx, by = P(side * (PIER_W / 2 - 1.1), d)
                # back to the railing, facing across the deck
                L.bench(L.uid("S-BENCH-PR"), bx, by, zd, math.degrees(math.atan2(-uy * side, -ux * side)),
                        style="promenade", area="pier", terrace="T0")
        d += 14.0
        k += 1
    for cu, cd in ((-h + 1.5, hc - h + 1.5), (h - 1.5, hc - h + 1.5), (-h + 1.5, hc + h - 1.5), (h - 1.5, hc + h - 1.5)):
        L.lamp(*P(cu, cd), z, area="pier")
    for side in (-1, 1):
        for o in (-6.0, 0.0, 6.0):
            bx, by = P(side * (h - 1.6), hc + o)
            L.bench(L.uid("S-BENCH-PR"), bx, by, z, math.degrees(math.atan2(uy * side, ux * side)), style="promenade",
                    area="pier_head", terrace="T0", sub="sea")
    sp = L.P.spots.get("S-PIER-HEAD", {})
    slots = [{"pos": L.ue(*P(o, hc + h - 0.6), z), "yaw": round(yaw_out, 2), "height": 0.0} for o in range(-10, 11, 2)]
    L.spot("S-PIER-HEAD", "pier", *P(0.0, hc + h - 1.5), z, yaw_out, seats=len(slots), slots=slots, sub="rail",
           name_ru=sp.get("name_ru"), name_en=sp.get("name_en"), notes=sp.get("notes", ""),
           path=[(*P(0.0, d), z) for d in range(2, int(head0), 10)] + [(*P(0.0, hc + h - 1.5), z)])
    L.spot("S-PIER-SUNRISE", "sunrise_point", *P(0.0, head0 + 1.0), z, 180.0, seats=6, faces="sunrise",
           slots=[stand(L, *P(o, head0 + 1.0), z, 180.0) for o in (-5.0, -3.0, -1.0, 1.0, 3.0, 5.0)])
    L.d["areas"].append({"id": "pier", "name": "place.pier.name", "at": L.ue(*P(0.0, hc), z), "r": 20.0})


def validate(doc, plan):
    """Every seat and standing place on dry, open, walkable ground: not inside a building's plot, not in the sea below
    high water, not in the fountain's basin or the pond. Returns the problems."""
    bad = []
    rects = []
    for pid, p in plan.plots.items():
        if p.get("building") and p["kind"] not in ("fair",):
            y = p["map"][1]
            sp = W.inland(p["map"][0], y)
            rects.append((pid, y - p["w"] / 2, y + p["w"] / 2, sp - p["d"] / 2, sp + p["d"] / 2))
    f = doc["fountains"][0] if doc["fountains"] else None
    pond = next((w for w in doc["waters"] if w["id"] == "pond"), None)
    for sp in doc["spots"]:
        for sl in sp.get("slots", []):
            x, y, z = sl["pos"][0], sl["pos"][1] - plan.face_y, sl["pos"][2]
            s_ = W.inland(x, y)
            for pid, y0, y1, s0, s1 in rects:
                if y0 < y < y1 and s0 < s_ < s1:
                    bad.append("%s in plot %s" % (sp["id"], pid))
            if s_ < C.SEAWALL_S and z < W.SEA_Z + 0.25:
                bad.append("%s below high water (z %.2f)" % (sp["id"], z))
            if f and math.hypot(x - f["at"][0], y + plan.face_y - f["at"][1]) < f["stone"]["rim_r"] - 0.6 and sp["kind"] != "fountain":
                bad.append("%s in the fountain's basin" % sp["id"])
            if pond:
                a = math.radians(-pond["yaw"])
                dx, dy = x - pond["at"][0], y + plan.face_y - pond["at"][1]
                u, v = dx * math.cos(a) - dy * math.sin(a), dx * math.sin(a) + dy * math.cos(a)
                if (u / pond["r"][0]) ** 2 + (v / pond["r"][1]) ** 2 < 0.95:
                    bad.append("%s in the pond" % sp["id"])
        if not sp.get("slots"):
            bad.append("%s has no slots" % sp["id"])
    return bad


def build():
    plan = Plan()
    L = Layout(plan)
    promenade(L)
    beach(L)
    square(L)
    park(L)
    fair(L)
    w1_planters(L)
    pier(L)
    cascade(L)
    club_stair(L)
    # the plan's other spots we don't furnish (cascade, lookouts, bonfire, pier head, the upper benches): passed on
    have = {s["id"] for s in L.d["spots"]}
    for sid, s in plan.spots.items():
        if sid in have:
            continue
        x, y, z = s["map"]
        n = max(1, s.get("seats", 0))
        ring = [stand(L, x + math.cos(2.4 * k) * (0.6 + 0.35 * math.sqrt(k)), y + math.sin(2.4 * k) * (0.6 + 0.35 * math.sqrt(k)),
                      z, s["yaw"]) for k in range(n)]
        L.spot(sid, s["kind"], x, y, z, s["yaw"], seats=n, slots=ring, name_ru=s.get("name_ru"),
               name_en=s.get("name_en"), notes=s.get("notes", ""), furnished=False)
    doc = {"_doc": "Built by tools/public_spaces.py from Content/City/city_plan.json (docs/city/public_spaces.md); "
                   "do not edit by hand. Unreal metres, yaw Unreal degrees (0 = +x = the sea). boxes: solid blocks "
                   "(c = centre, size = along yaw, across, height); props: instanced kit meshes; plants: single "
                   "procedural plants; trees: one AProceduralForest; lights: lamp heads that take a light from the "
                   "pool at night; waters: still water (the pond); spots: ULyudPublicSpotsSubsystem, also written to "
                   "PeopleData/town_plan.json.",
           "version": 2, "frame": "unreal_metres", "face_y": plan.face_y}
    doc.update(L.d)
    doc["problems"] = validate(doc, plan)
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
    fg = {"_doc": "The fair's stalls (tools/public_spaces.py, docs/city/public_spaces.md): theme, shop title key, goods "
                  "[{item, price, count}] (LyudInventory items). staffed: town_life.py gives it a seller (place "
                  "S-FAIR-STALLnn-SELLER at the spot's seller pos, activity work_fair, Sat-Sun 10-19) and the counters.json "
                  "entry (ULyudCounterTradeSubsystem: T to trade).",
          "stalls": doc.get("fair_goods", {})}
    with open(os.path.join(os.path.dirname(OUT), "fair_goods.json"), "w") as f:
        json.dump(fg, f, ensure_ascii=False, indent=1)
        f.write("\n")
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
    ap.add_argument("--strict", action="store_true", help="refuse without the built terrain or with any spot problem")
    a = ap.parse_args()
    if a.strict and not os.path.exists(_TERRAIN_OUT):
        sys.exit("no %s" % _TERRAIN_OUT)
    doc = build()
    probs = doc.pop("problems")
    for pr in probs[:40]:
        print("PROBLEM", pr)
    print("%d spot problems" % len(probs))
    if a.strict and probs:
        sys.exit(1)
    if not a.dry:
        write(doc)
    kinds = {}
    for p in doc["props"]:
        kinds[p["kind"]] = kinds.get(p["kind"], 0) + 1
    print("public spaces: %d boxes, %d props %s, %d plants, %d trees, %d lights, %d spots, club stair %s" % (
        len(doc["boxes"]), len(doc["props"]), kinds, len(doc["plants"]), len(doc["trees"]), len(doc["lights"]),
        len(doc["spots"]), doc.get("club_stair")))
    if a.png:
        png(doc, a.png)


if __name__ == "__main__":
    main()
