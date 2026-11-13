#!/usr/bin/env python3
"""Offline checks of the town plan (docs/city_plan.md) and the data the game builds from it.

  python3 tools/cityplan/check_plan.py [-v]

Reads tools/city_plan.py (rebuilt in memory), Content/City/city_plan.json, Content/City/public_spaces.json,
Content/PeopleData/town_places.json + townsfolk.json + town_plan.json and the buildings' blender/export/city/*.json.
One line per check: `ok   <check>` or `FAIL <check>: <first problems>`; exits 1 on any failure.

What the game relies on (and so what is checked):
 - ALyudCityWindows reads wall_dwellings / promenade_units windows (id, ue, size, storey) and lamps (id, head_ue);
   UPeopleLifeSubsystem lights a flat by its id: wall flats "W3-D017" go to ALyudCityWindows, building flats
   "<plot>/<flat>" to ULyudWindowLights (WindowFlats from the building json's windows[].flat).
 - PeopleSim walks the town on town_places' levels and stairs (bottom on the lower terrace, top on the upper).
 - city_mesh.py / import_city.py build the walls, stairs and buildings where the plan says.
"""
import glob
import json
import math
import os
import re
import sys
from collections import Counter, defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools"))
import city_plan as C  # noqa: E402
import world_map as W  # noqa: E402

CONTENT = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content")
VERBOSE = "-v" in sys.argv
FAILS = []


def load(rel):
    with open(os.path.join(CONTENT, rel), encoding="utf-8") as f:
        return json.load(f)


def check(name, problems):
    """problems: a list of strings (empty = ok)."""
    if problems:
        FAILS.append(name)
        shown = problems if VERBOSE else problems[:6]
        print("FAIL %s: %d problem(s): %s%s" % (name, len(problems), "; ".join(str(p) for p in shown),
                                                 "" if len(shown) == len(problems) else " ..."))
    else:
        print("ok   %s" % name)


PLAN = load("City/city_plan.json")
FACE_Y = PLAN["portal_face_y"]
TI = {t["id"]: i for i, t in enumerate(PLAN["terraces"])}
TZ = {t["id"]: t["z"] for t in PLAN["terraces"]}
TZ["B"] = W.BEACH_TOP
EPS = 0.02

# ------------------------------------------------------------------------------------------- determinism

built = json.loads(json.dumps(C.build(), ensure_ascii=False))
diff = [k for k in set(built) | set(PLAN) if built.get(k) != PLAN.get(k)]
check("plan: tools/city_plan.py rebuilds the committed city_plan.json", ["key %s differs" % k for k in sorted(diff)])
again = json.loads(json.dumps(C.build(), ensure_ascii=False))
check("plan: build() is repeatable in one process (no state leaks between calls)",
      [] if again == built else ["second build() differs"])
check("plan: portal_face_y known (blender/export/scene01_layout.json), so every ue field is written",
      [] if isinstance(FACE_Y, (int, float)) else ["portal_face_y is %r" % FACE_Y])


def ue_ok(m, u):
    return abs(u[0] - m[0] * 100) < 0.11 and abs(u[1] - (FACE_Y + m[1] * 100)) < 0.11 and abs(u[2] - m[2] * 100) < 0.11


probs = []
for grp in ("plots", "spots", "links"):
    for p in PLAN[grp]:
        if "map" in p and len(p["map"]) == 3 and not ue_ok(p["map"], p.get("ue", [1e9] * 3)):
            probs.append(p["id"])
for d in PLAN["wall_dwellings"] + PLAN["promenade_units"]:
    if not ue_ok(d["door"], d["door_ue"]):
        probs.append(d["id"] + " door")
    probs += [w["id"] for w in d["windows"] if not ue_ok(w["map"], w["ue"])]
for s in PLAN["stairs"]:
    if not (ue_ok(s["bottom"], s["bottom_ue"]) and ue_ok(s["top"], s["top_ue"])):
        probs.append(s["id"])
for lp in PLAN["lamps"]:
    if not (ue_ok(lp["foot"], lp["foot_ue"]) and ue_ok(lp["head"], lp["head_ue"])):
        probs.append(lp["id"])
check("plan: every ue point = map * 100 shifted by portal_face_y", probs)

# ------------------------------------------------------------------------------------------- ids

ID_RE = {
    "plots": r"P-[A-Z0-9]+$", "spots": r"S-[A-Z0-9-]+$", "stairs": r"ST-(SEA-\d\d|W[1-5]-\d\d|AXIS-W[1-5]|CASCADE-W[2-5])$",
    "wall_dwellings": r"W[2-5]-D\d{3}$", "lamps": r"LAMP-W[2-5]-\d\d$", "promenade_units": r"W1-U\d\d$",
    "links": r"L-[A-Z-]+$", "cascade": r"CASCADE-W[2-5]$",
}
probs = []
seen = Counter()
for grp, rx in ID_RE.items():
    for e in PLAN[grp]:
        seen[e["id"]] += 1
        if not re.match(rx, e["id"]):
            probs.append("%s %s" % (grp, e["id"]))
for wid, bays in PLAN["wall_bays"].items():
    for b in bays:
        seen[b["id"]] += 1
        if not re.match(r"%s-B\d{3}$" % wid, b["id"]) or b["wall"] != wid:
            probs.append("bay %s" % b["id"])
for d in PLAN["wall_dwellings"]:
    for k, w in enumerate(d["windows"]):
        seen[w["id"]] += 1
        if w["id"] != "%s-w%d" % (d["id"], k + 1):
            probs.append("window %s" % w["id"])
    if not d["id"].startswith(d["wall"] + "-"):
        probs.append("dwelling %s on wall %s" % (d["id"], d["wall"]))
for u in PLAN["promenade_units"]:
    for w in u["windows"]:
        seen[w["id"]] += 1
check("ids: every id matches its pattern (P-, S-, ST-, W#-D###, W#-D###-w#, LAMP-, W#-B###)", probs)
check("ids: no id used twice across the whole plan", ["%s x%d" % (k, n) for k, n in seen.items() if n > 1])
nums = defaultdict(list)
for d in PLAN["wall_dwellings"]:
    nums[d["wall"]].append(int(d["id"][-3:]))
check("ids: wall flats numbered 1..n per wall without gaps",
      ["%s: %s" % (w, n[:5]) for w, n in nums.items() if sorted(n) != list(range(1, len(n) + 1))])
check("plan: every plot's building is a catalogued building type",
      ["%s: %s" % (p["id"], p["building"]) for p in PLAN["plots"] if p["building"] and p["building"] not in PLAN["building_types"]])

# ------------------------------------------------------------------------------------------- terraces, walls

probs = []
zs = [t["z"] for t in PLAN["terraces"]]
if zs != sorted(zs):
    probs.append("terraces not rising: %s" % zs)
for i, w in enumerate(PLAN["walls"]):
    if abs(w["height"] - (zs[i + 1] - zs[i])) > 1e-6:
        probs.append("%s height %s" % (w["id"], w["height"]))
for y in range(int(C.CITY_Y0), int(C.CITY_Y1) + 1, 5):
    ss = [C.SEAWALL_S] + [C.wall_s(i, y) for i in range(len(C.WALLS))]
    for a, b in zip(ss, ss[1:]):
        if b - a < C.WALL_DEPTH + C.LANE_W + 10.0:
            probs.append("y %d: walls %.1f and %.1f too close for a block, a lane and a terrace" % (y, a, b))
            break
check("walls: terraces rise, wall heights match, walls never crowd each other (block + lane + 10 m)", probs)

probs = []
for i in range(len(C.WALLS)):
    for y in range(int(C.CITY_Y0), int(C.CITY_Y1) + 1, 7):
        ws = C.wall_s(i, y)
        x_front, _ = C.to_map(ws - 0.5, y)
        x_top, _ = C.to_map(ws + C.RAMP_START + C.WALL_RAMP + 0.5, y)
        zf, zt = C.terrace_z(x_front, y), C.terrace_z(x_top, y)
        if abs(zf - zs[i]) > EPS or abs(zt - zs[i + 1]) > EPS:
            probs.append("%s y %d: %.2f / %.2f" % (C.WALLS[i][0], y, zf, zt))
        x_in, _ = C.to_map(ws + C.WALL_DEPTH - 0.3, y)
        if abs(C.terrace_z(x_in, y) - zs[i + 1]) > EPS:
            probs.append("%s y %d: ramp not done inside the block" % (C.WALLS[i][0], y))
check("terrain: lower terrace right in front of each wall, upper terrace on top of its block (ramp hidden)", probs)

probs = []
for y in range(int(C.CITY_Y0), int(C.CITY_Y1) + 1, 10):
    for s in range(30, 1200, 3):
        x, _ = C.to_map(float(s), float(y))
        z = W.height(x, y)
        t = C.terrace_at(x, y)
        if not (zs[0] - 0.01 <= z <= 80.0) or not math.isfinite(z):
            probs.append("(%d, %d) z %.2f" % (s, y, z))
            break
        if t < 0:
            probs.append("(%d, %d) not on a terrace" % (s, y))
            break
check("terrain: ground inside the town finite and between T0 and the ridge", probs)

# ------------------------------------------------------------------------------------------- plots

OPEN = {"square", "park"}             # no building: people, paving and furniture may be on them


def plot_box(p):
    return (p["s"] - p["d"] / 2, p["s"] + p["d"] / 2, p["y_along"] - p["w"] / 2, p["y_along"] + p["w"] / 2)


def in_box(bx, s, y, m=0.0):
    return bx[0] - m < s < bx[1] + m and bx[2] - m < y < bx[3] + m


def boxes_overlap(a, b, m=0.0):
    return a[0] < b[1] - m and b[0] < a[1] - m and a[2] < b[3] - m and b[2] < a[3] - m


_LINES = {}


def wall_dist(i, pt):
    """Distance (m) of a map point from wall i's face (perpendicular, to its dense polyline): + in front (seaward)."""
    if i not in _LINES:
        _LINES[i] = C._wall_line(i)
    pts = _LINES[i]
    best = 1e9
    for (x0, y0, _), (x1, y1, _) in zip(pts, pts[1:]):
        if abs(y0 - pt[1]) > 60.0:
            continue
        dx, dy = x1 - x0, y1 - y0
        t = max(0.0, min(1.0, ((pt[0] - x0) * dx + (pt[1] - y0) * dy) / (dx * dx + dy * dy)))
        best = min(best, math.hypot(pt[0] - x0 - dx * t, pt[1] - y0 - dy * t))
    s = W.inland(pt[0], pt[1])
    ws = C.SEAWALL_S if i < 0 else C.wall_s(i, pt[1])
    return best if s < ws else -best


def sy(pt):
    """(s, y) of a map point."""
    return W.inland(pt[0], pt[1]), pt[1]


PL = PLAN["plots"]
PB = {p["id"]: plot_box(p) for p in PL}
probs = []
for p in PL:
    x, y = C.to_map(p["s"], p["y_along"])
    if abs(x - p["map"][0]) > 0.01 or abs(y - p["map"][1]) > 0.01:
        probs.append("%s map is not (s, y_along)" % p["id"])
    a = math.radians(p["yaw"])
    fx, fy = p["map"][0] + math.cos(a) * p["d"] / 2, p["map"][1] + math.sin(a) * p["d"] / 2
    if abs(fx - p["front"][0]) > 0.02 or abs(fy - p["front"][1]) > 0.02:
        probs.append("%s front is not d/2 along yaw" % p["id"])
    if p["front"][2] != p["map"][2] or abs(p["map"][2] - TZ[p["terrace"]]) > 1e-6:
        probs.append("%s z %.2f not its terrace's %.2f" % (p["id"], p["map"][2], TZ[p["terrace"]]))
    if p["building"] and not p.get("height_max") and p["kind"] not in ("pool", "residential"):
        probs.append("%s has no height_max" % p["id"])
check("plots: centre, front and z consistent with s, y_along, yaw and the terrace", probs)

probs = []
for i, a in enumerate(PL):
    for b in PL[i + 1:]:
        if boxes_overlap(PB[a["id"]], PB[b["id"]]):
            probs.append("%s x %s" % (a["id"], b["id"]))
check("plots: no two plots overlap", probs)

# a plot stands on its own terrace: between the block of the wall below (or the seawall) and the lane in front of the
# wall above. By design: the risers straddle a wall (P-RISER1/2), the pier stands out in the sea, the beach plots.
STRADDLE = {"P-RISER1": 1, "P-RISER2": 3}
probs = []
for p in PL:
    t = p["terrace"]
    if t == "B" or p["id"] in ("P-PIER",):
        continue
    i = TI[t]
    s0, s1, y0, y1 = PB[p["id"]]
    for y in (y0 + 0.01, (y0 + y1) / 2, y1 - 0.01):
        lo = C.SEAWALL_S if i == 0 else C.wall_s(i - 1, y) + C.WALL_DEPTH
        hi = C.wall_s(i, y) - (C.LANE_W if 1 <= i <= 4 else 0.0) if i < 5 else 1e9
        if p["id"] in STRADDLE:
            ws = C.wall_s(STRADDLE[p["id"]], y)
            if not (s0 < ws < s1):
                probs.append("%s does not straddle its wall at y %.0f" % (p["id"], y))
            continue
        if s0 < lo - 0.01 or s1 > hi + 0.01:
            probs.append("%s y %.0f: s %.1f..%.1f outside %.1f..%.1f" % (p["id"], y, s0, s1, lo, hi))
            break
    if p["id"] in STRADDLE:
        continue
    for s in (s0 + 0.3, p["s"], s1 - 0.3):
        for y in (y0 + 0.3, p["y_along"], y1 - 0.3):
            x, _ = C.to_map(s, y)
            z = W.height(x, y)
            if abs(z - p["map"][2]) > 0.05:
                probs.append("%s ground %.2f at (s %.0f, y %.0f), plot z %.2f" % (p["id"], z, s, y, p["map"][2]))
                break
        else:
            continue
        break
check("plots: each on its own terrace, off the walls and lanes, on level ground (risers straddle their wall)", probs)

probs = []
for p in PL:
    t = p["terrace"]
    if p["building"] and p.get("height_max") and t not in ("B",) and p["id"] not in STRADDLE:
        i = TI[t]
        # a building may rise over the terrace above (the town climbs), but never over the ridge: top under 120 m
        top = p["map"][2] + p["height_max"]
        if top > 80.0:
            probs.append("%s top %.1f" % (p["id"], top))
check("plots: building tops (z + height_max) stay under the ridge", probs)

# ------------------------------------------------------------------------------------------- the walls' bays

probs = []
for wid, bays in PLAN["wall_bays"].items():
    prev = None
    for b in bays:
        L = math.dist(b["p0"], b["p1"])
        want = C.BAY.get(wid, C.DWELLING_W)
        if abs(L - want) > 0.1:            # a chord of the arc: a little short in the bends
            probs.append("%s length %.2f" % (b["id"], L))
        if prev and math.dist(prev["p1"], b["p0"]) > 0.01:
            probs.append("%s gap to %s" % (prev["id"], b["id"]))
        prev = b
        if wid != "SEA" and b["mask"] >= 0.999:
            i = [w[0] for w in C.WALLS].index(wid)
            off = wall_dist(i, b["mid"])
            # a chord of the curved face: in the Cascade's sharp bends up to ~0.5 m off the true curve. The terrain's
            # ramp starts RAMP_START behind the true face, so the chord must stay well within that
            if abs(off) > C.RAMP_START - 0.4:
                probs.append("%s mid %.2f off the face" % (b["id"], off))
check("bays: each wall cut into equal, gapless bays lying on the face", probs)

probs = []
KINDS = {"SEA": {"blank", "sea_steps"}, "W1": {"blank", "unit", "stair", "grand"}}
for wid, bays in PLAN["wall_bays"].items():
    ok = KINDS.get(wid, {"dwelling", "stair", "grand", "plot"})
    for b in bays:
        if b["mask"] >= 0.999 and b["kind"] not in ok:
            probs.append("%s kind %s" % (b["id"], b["kind"]))
        if b["mask"] < 0.999 and b["kind"] != "blank":
            probs.append("%s (fading) kind %s" % (b["id"], b["kind"]))
    for k, b in enumerate(bays):
        if wid not in ("SEA", "W1") and b["kind"] == "stair":
            part = b.get("stair_part")
            other = bays[k + 1] if part == 0 else bays[k - 1]
            if other["kind"] != "stair" or other.get("stair_part") != 1 - part:
                probs.append("%s stair without its other half" % b["id"])
check("bays: kinds valid per wall; wall stairs on W2..W5 come in pairs", probs)

# grand stairs: one group of bays per (wall, grand_y), contiguous, no other stair inside it
probs = []
for wid, bays in PLAN["wall_bays"].items():
    if wid == "SEA":
        continue
    by = defaultdict(list)
    for k, b in enumerate(bays):
        if b["kind"] == "grand":
            by[b.get("grand_y", 905.0)].append(k)
    for gy, ks in by.items():
        if ks != list(range(ks[0], ks[-1] + 1)):
            probs.append("%s grand %g: bays %s interrupted by %s" % (wid, gy, [bays[k]["id"] for k in (ks[0], ks[-1])],
                                                                    [bays[k]["id"] + " " + bays[k]["kind"] for k in range(ks[0], ks[-1]) if k not in ks]))
check("grand stairs: each one's bays are contiguous (no wall stair or flat inside the flight)", probs)

# ------------------------------------------------------------------------------------------- stairs


def grand_centre(wid, gy):
    grp = [b for b in PLAN["wall_bays"][wid] if b["kind"] == "grand" and b.get("grand_y", 905.0) == gy and b["mask"] >= 0.999]
    mid, _, _ = C._grand_centre(grp)
    return mid


probs = []
WALL_I = {w[0]: i for i, w in enumerate(C.WALLS)}
for st in PLAN["stairs"]:
    b, t = st["bottom"], st["top"]
    wid = st["wall"]
    if wid == "SEA":
        lo, hi = W.BEACH_TOP, zs[0]
        sb, yb = sy(b)
        st_ = sy(t)[0]
        if not (sb < C.SEAWALL_S < st_):
            probs.append("%s does not cross the seawall" % st["id"])
    else:
        i = WALL_I[wid]
        lo, hi = zs[i], zs[i + 1]
        sb, yb = sy(b)
        stp, yt = sy(t)
        if not sb < C.wall_s(i, yb) - 0.3:
            probs.append("%s bottom not in front of %s" % (st["id"], wid))
        elif abs(C.terrace_z(*b[:2]) - lo) > EPS:
            probs.append("%s bottom on ground %.2f, not %.2f" % (st["id"], C.terrace_z(*b[:2]), lo))
        if not C.wall_s(i, yt) + 0.3 < stp < C.wall_s(i, yt) + C.WALL_DEPTH:
            probs.append("%s top not on %s's block" % (st["id"], wid))
    if abs(b[2] - lo) > 1e-6 or abs(t[2] - hi) > 1e-6:
        probs.append("%s z %.2f -> %.2f, levels %.2f -> %.2f" % (st["id"], b[2], t[2], lo, hi))
    run = math.dist(b[:2], t[:2])
    rise = hi - lo
    if run < rise * 0.9:
        probs.append("%s too steep: %.1f m up over %.1f m" % (st["id"], rise, run))
check("stairs: bottom on the lower level in front of the wall, top on the upper (its block), walkable pitch", probs)

probs = []
for st in PLAN["stairs"]:
    if st["kind"] != "grand_stair":
        continue
    gy = 905.0 if "AXIS" in st["id"] else C.CASCADE_Y
    c = grand_centre(st["wall"], gy)
    # the mesh (city_mesh.grand_stairs) is centred on the group's middle: the walking line must run up its middle too
    ax =[t - b for t, b in zip(st["top"][:2], st["bottom"][:2])]
    L = math.hypot(*ax)
    # distance of the mesh's centre from the stair's walking line
    dist = abs((c[0] - st["bottom"][0]) * ax[1] - (c[1] - st["bottom"][1]) * ax[0]) / L
    if dist > 0.25:
        probs.append("%s walks %.2f m off the flight's centre line (width %.1f)" % (st["id"], dist, st["width"]))
check("grand stairs: the people's line up each grand stair runs on the built flight's centre", probs)

probs = []
for st in PLAN["stairs"]:
    if st["kind"] not in ("wall_stair", "grand_stair"):
        continue
    for o in PLAN["stairs"]:
        if o is st or o["wall"] != st["wall"] or o["kind"] not in ("wall_stair", "grand_stair"):
            continue
        if set(st.get("bays", [])) & set(o.get("bays", [])):
            probs.append("%s and %s share bays" % (st["id"], o["id"]))
    if st["kind"] == "grand_stair":
        mid = [(b + t) / 2 for b, t in zip(st["bottom"][:2], st["top"][:2])]
        for o in PLAN["stairs"]:
            if o["wall"] == st["wall"] and o["kind"] == "wall_stair":
                for q in (o["bottom"], o["top"]):
                    if math.dist(q[:2], st["top"][:2]) < st["width"] / 2 + 1.0 and abs(q[2] - st["top"][2]) < 0.5 or \
                            math.dist(q[:2], st["bottom"][:2]) < st["width"] / 2 + 1.0 and abs(q[2] - st["bottom"][2]) < 0.5:
                        probs.append("%s ends on the grand stair %s" % (o["id"], st["id"]))
                        break
check("stairs: no two stairs share a bay or land on each other", sorted(set(probs)))

# the axis: the town's main axis is Y 905 (docs/city_plan.md); every axis stair is centred on it within a bay
probs = []
for st in PLAN["stairs"]:
    if "AXIS" in st["id"]:
        c = grand_centre(st["wall"], 905.0)
        if abs(c[1] - 905.0) > C.BAY.get(st["wall"], C.DWELLING_W) / 2 + 0.5:
            probs.append("%s centred at y %.1f" % (st["id"], c[1]))
check("grand stairs: the axis stairs stand on the main axis Y 905", probs)

# stair footprints keep off the plots
probs = []
for st in PLAN["stairs"]:
    if st["wall"] == "SEA":
        continue
    for q in (st["bottom"], [(a + b) / 2 for a, b in zip(st["bottom"], st["top"])]):
        s, y = sy(q)
        for p in PL:
            if p["kind"] in OPEN or not p["building"]:
                continue
            if in_box(PB[p["id"]], s, y):
                probs.append("%s inside %s" % (st["id"], p["id"]))
check("stairs: no stair runs into a building plot", probs)

# ------------------------------------------------------------------------------------------- dwellings, units, lamps

BAYS = {b["id"]: b for bays in PLAN["wall_bays"].values() for b in bays}


def on_bay(b, pt):
    """(u along the bay from its middle, metres in front of the face) of a map point."""
    ax, ay = b["p1"][0] - b["p0"][0], b["p1"][1] - b["p0"][1]
    L = math.hypot(ax, ay)
    ax, ay = ax / L, ay / L
    nx, ny = ay, -ax
    dx, dy = pt[0] - b["mid"][0], pt[1] - b["mid"][1]
    return dx * ax + dy * ay, dx * nx + dy * ny, L


probs = []
for d in PLAN["wall_dwellings"]:
    b = BAYS.get(d["bay"])
    if not b or b["kind"] != "dwelling" or b["wall"] != d["wall"]:
        probs.append("%s: bay %s is not a dwelling bay" % (d["id"], d["bay"]))
        continue
    i = WALL_I[d["wall"]]
    if d["terrace"] != PLAN["terraces"][i]["id"] or abs(d["yaw"] - b["yaw"]) > 1e-6:
        probs.append("%s terrace/yaw" % d["id"])
    u, n, L = on_bay(b, d["door"])
    if abs(n - 0.6) > 0.01 or abs(u) + 0.55 > L / 2:
        probs.append("%s door (u %.2f, %.2f in front)" % (d["id"], u, n))
    s, y = sy(d["door"])
    if not s < C.wall_s(i, y) or abs(C.terrace_z(*d["door"][:2]) - d["door"][2]) > EPS or abs(d["door"][2] - zs[i]) > 1e-6:
        probs.append("%s door not on the lane at the foot of %s" % (d["id"], d["wall"]))
    if s < C.wall_s(i, y) - C.LANE_W:
        probs.append("%s door off the lane" % d["id"])
    for p in PL:
        if p["building"] and in_box(PB[p["id"]], s, y, 0.5):
            probs.append("%s door inside %s" % (d["id"], p["id"]))
    rects = [(u - 0.55, u + 0.55, 0.0, 2.2)]
    for w in d["windows"]:
        wu, wn, _ = on_bay(b, w["map"])
        ww, wh = w["size"]
        zc = w["map"][2] - b["lower"]
        if abs(wn + 0.27) > 0.01:
            probs.append("%s not in its reveal (%.2f)" % (w["id"], wn))
        if abs(wu) + ww / 2 > L / 2 - 0.2 or zc - wh / 2 < 0.3 or zc + wh / 2 > b["upper"] - b["lower"] - 0.3:
            probs.append("%s off the bay's face" % w["id"])
        if w["storey"] != (0 if zc < 3.0 else 1):
            probs.append("%s storey %d at %.1f m" % (w["id"], w["storey"], zc))
        r = (wu - ww / 2, wu + ww / 2, zc - wh / 2, zc + wh / 2)
        for o in rects:
            if r[0] < o[1] and o[0] < r[1] and r[2] < o[3] and o[2] < r[3]:
                probs.append("%s overlaps another opening" % w["id"])
        rects.append(r)
check("wall flats: in a dwelling bay, door 0.6 m out on the lane at the lower level, windows on the face", probs)

probs = []
for d in PLAN["wall_dwellings"]:
    if len(d["windows"]) != 3 or [w["storey"] for w in d["windows"]] != [0, 1, 1]:
        probs.append(d["id"])
    for w in d["windows"]:
        if not (len(w.get("ue", [])) == 3 and len(w.get("size", [])) == 2 and all(math.isfinite(v) for v in w["ue"] + w["size"])):
            probs.append(w["id"] + " fields")
for u in PLAN["promenade_units"]:
    for w in u["windows"]:
        if not (len(w.get("ue", [])) == 3 and len(w.get("size", [])) == 2):
            probs.append(w["id"] + " fields")
check("windows: every wall/unit window carries id, ue, size, storey (what ALyudCityWindows reads)", probs)

probs = []
units = [b for b in PLAN["wall_bays"]["W1"] if b["kind"] == "unit"]
if len(units) != len(PLAN["promenade_units"]):
    probs.append("%d unit bays, %d units" % (len(units), len(PLAN["promenade_units"])))
for u in PLAN["promenade_units"]:
    b = BAYS[u["bay"]]
    du, dn, L = on_bay(b, u["door"])
    s, y = sy(u["door"])
    if b["kind"] != "unit" or abs(dn - 0.6) > 0.01 or abs(du) + 0.6 > L / 2 or not (C.SEAWALL_S < s < C.wall_s(0, y)):
        probs.append(u["id"])
    if abs(u["door"][2] - zs[0]) > 1e-6:
        probs.append(u["id"] + " z")
check("promenade units: one per unit bay of W1, door on the promenade", probs)

probs = []
for lp in PLAN["lamps"]:
    b = BAYS[lp["bay"]]
    s, y = sy(lp["foot"])
    i = WALL_I[lp["wall"]]
    if b["kind"] in ("grand", "plot") or b["mask"] < 0.999:
        probs.append("%s on a %s bay" % (lp["id"], b["kind"]))
    if not (C.LANE_W < wall_dist(i, lp["foot"]) < C.LANE_W + 1.0):
        probs.append("%s foot not at the lane's kerb" % lp["id"])
    if abs(lp["foot"][2] - zs[i]) > 1e-6 or abs(C.terrace_z(*lp["foot"][:2]) - zs[i]) > EPS:
        probs.append("%s foot z" % lp["id"])
    for p in PL:
        if p["building"] and in_box(PB[p["id"]], s, y, 0.5):
            probs.append("%s inside %s" % (lp["id"], p["id"]))
check("lamps: on the lane's kerb of a flat or stair bay, at the lower level, off the plots", probs)

# ------------------------------------------------------------------------------------------- spots

ON_EDGE = {"S-SUNSET", "S-PIER-HEAD", "S-CASCADE"}   # by design on a wall's edge, the pier, the cascade
probs = []
for sp in PLAN["spots"]:
    t = sp["terrace"]
    if abs(sp["map"][2] - (TZ[t] if t != "B" else W.BEACH_TOP - 0.5)) > 1e-6:
        probs.append("%s z" % sp["id"])
    if sp["id"] in ON_EDGE or t == "B":
        continue
    s, y = sy(sp["map"])
    ti = C.terrace_at(*sp["map"][:2])
    if ti != TI[t] or abs(C.terrace_z(*sp["map"][:2]) - TZ[t]) > EPS:
        probs.append("%s on terrace %s ground %.2f, says %s" % (sp["id"], ti, C.terrace_z(*sp["map"][:2]), t))
    for p in PL:
        if p["building"] and p["kind"] not in ("fair",) and in_box(PB[p["id"]], s, y):
            probs.append("%s inside %s" % (sp["id"], p["id"]))
check("spots: on their terrace's level ground and not inside a building plot", probs)

# ------------------------------------------------------------------------------------------- cascade

probs = []
for c in PLAN["cascade"]:
    gs = [s for s in PLAN["stairs"] if s["id"] == "ST-CASCADE-" + c["wall"]]
    if not gs:
        probs.append("%s has no stair" % c["id"])
        continue
    st = gs[0]
    i = WALL_I[c["wall"]]
    if c["bottom"][2] != zs[i] or abs(c["top"][2] - (zs[i + 1] - C.CASCADE_BED)) > 1e-6:
        probs.append("%s levels" % c["id"])
    if abs(c["rise"] * c["steps"] - (zs[i + 1] - zs[i])) > 1e-3 or not 0.12 < c["rise"] < 0.2:
        probs.append("%s steps" % c["id"])
    # the channel lies beside the stair (not on it) and inside the grand bays
    grp = [b for b in PLAN["wall_bays"][c["wall"]] if b["kind"] == "grand" and b.get("grand_y") == C.CASCADE_Y]
    centre = grand_centre(c["wall"], C.CASCADE_Y)
    b0 = grp[0]
    ax, ay = c["along"]
    u = (c["top"][0] - centre[0]) * ax + (c["top"][1] - centre[1]) * ay
    lo, hi = u - c["width"] / 2 - 0.4, u + c["width"] / 2 + 0.4
    span0 = (grp[0]["p0"][0] - centre[0]) * ax + (grp[0]["p0"][1] - centre[1]) * ay
    span1 = (grp[-1]["p1"][0] - centre[0]) * ax + (grp[-1]["p1"][1] - centre[1]) * ay
    if lo < st["width"] / 2 - 0.01:
        probs.append("%s channel overlaps the stair (%.2f < %.2f)" % (c["id"], lo, st["width"] / 2))
    if hi > span1 + 0.01 or lo < span0:
        probs.append("%s channel %.2f..%.2f outside its grand bays %.2f..%.2f" % (c["id"], lo, hi, span0, span1))
    run = math.dist(c["bottom"][:2], c["top"][:2])
    if abs(run - c["steps"] * c["tread"]) > 0.01:
        probs.append("%s run %.2f" % (c["id"], run))
check("cascade: each channel beside its stair, inside the grand bays, steps add up to the wall", probs)

# ------------------------------------------------------------------------------------------- public spaces

PS = load("City/public_spaces.json")
FY = PS["face_y"]
BUILT = [p for p in PL if p["building"] and p["kind"] not in ("fair", "pier", "pool", "kiosk", "lifeguard")]


def ps_sy(at):
    return W.inland(at[0], at[1] - FY), at[1] - FY


probs = []
if abs(FY - FACE_Y / 100.0) > 1e-6:
    probs.append("face_y %.3f vs plan %.3f" % (FY, FACE_Y / 100.0))
for b in PS["boxes"]:
    s, y = ps_sy(b["c"])
    half = max(b["size"][0], b["size"][1]) / 2
    for p in BUILT:
        bx = PB[p["id"]]
        # the block's centre well inside a building's plot (its paving may run up to the plot's edge)
        if in_box(bx, s, y, -min(2.0, half)) and bx[0] + 2 < s < bx[1] - 2:
            probs.append("box %s %s inside %s" % (b["skin"], [round(v, 1) for v in b["c"]], p["id"]))
            break
check("public spaces: no paving block inside a building's plot", probs)

probs = []
for pr in PS["props"]:
    s, y = ps_sy(pr["at"])
    for p in BUILT:
        if in_box(PB[p["id"]], s, y, -0.5):
            probs.append("%s at %s inside %s" % (pr["kind"], [round(v, 1) for v in pr["at"]], p["id"]))
            break
for tr in PS["trees"] + PS["plants"]:
    s, y = ps_sy(tr["at"])
    for p in BUILT:
        if in_box(PB[p["id"]], s, y, -0.5):
            probs.append("%s at %s inside %s" % (tr["species"], [round(v, 1) for v in tr["at"]], p["id"]))
            break
check("public spaces: no prop, tree or plant inside a building's plot", probs)

probs = []
for pr in PS["props"] + PS["trees"]:
    s, y = ps_sy(pr["at"])
    for i in range(len(C.WALLS)):
        ws = C.wall_s(i, y)
        if ws - 0.3 < s < ws + C.WALL_DEPTH - 0.3 and C.city_mask(y) > 0.99:
            probs.append("%s at %s in %s's block" % (pr.get("kind", pr.get("species")), [round(v, 1) for v in pr["at"]], C.WALLS[i][0]))
check("public spaces: no prop or tree stands inside a terrace wall's block", probs)

probs = []
for pr in PS["props"]:
    s, y = ps_sy(pr["at"])
    for st in PLAN["stairs"]:
        if st["kind"] != "grand_stair":
            continue
        a, b = sy(st["bottom"]), sy(st["top"])
        # the flight's rectangle in the terrace frame (perpendicular to the wall): across = y
        if min(a[0], b[0]) - 0.3 < s < max(a[0], b[0]) and abs(y - (a[1] + b[1]) / 2) < st["width"] / 2 - 0.2:
            probs.append("%s at %s on %s" % (pr["kind"], [round(v, 1) for v in pr["at"]], st["id"]))
check("public spaces: no prop on a grand stair's flight", probs)

probs = []
for pr in PS["props"]:
    if not all(math.isfinite(v) for v in pr["at"]) or not math.isfinite(pr.get("yaw", 0.0)):
        probs.append(pr["kind"])
check("public spaces: props have finite positions", probs)

# ------------------------------------------------------------------------------------------- people's data

TP = load("PeopleData/town_places.json")
FOLK = load("PeopleData/townsfolk.json")["people"]
TPLAN = load("PeopleData/town_plan.json")
places = {p["id"]: p for p in TP["places"]}
flats = {p["id"]: p for p in TP["places"] if p["kind"] == "flat"}
wall_flats = {d["id"]: d for d in PLAN["wall_dwellings"]}

check("people: town_places ids unique", ["%s x%d" % (k, n) for k, n in Counter(p["id"] for p in TP["places"]).items() if n > 1])
probs = []
tw = {k for k in flats if re.match(r"W\d-D\d{3}$", k)}
probs += ["%s in town_places, not in the plan" % k for k in sorted(tw - set(wall_flats))]
probs += ["%s in the plan, not in town_places" % k for k in sorted(set(wall_flats) - tw)]
for k in tw & set(wall_flats):
    if flats[k]["windows"] != [w["id"] for w in wall_flats[k]["windows"]]:
        probs.append("%s windows differ" % k)
    door = [wall_flats[k]["door_ue"][0] / 100, wall_flats[k]["door_ue"][1] / 100, wall_flats[k]["door_ue"][2] / 100]
    if math.dist(door, flats[k]["door"]) > 0.02:
        probs.append("%s door moved" % k)
check("people: wall flats in town_places = the plan's wall_dwellings (ids, windows, doors)", probs)

probs = []
plan_stairs = {s["id"]: s for s in PLAN["stairs"]}
tps = {s["id"]: s for s in TP["town"]["stairs"]}
if set(plan_stairs) != set(tps):
    probs.append("ids differ: %s" % sorted(set(plan_stairs) ^ set(tps))[:6])
for k in set(plan_stairs) & set(tps):
    for end in ("bottom", "top"):
        u = [v / 100 for v in plan_stairs[k][end + "_ue"]]
        if math.dist(u, tps[k][end]) > 0.02:
            probs.append("%s %s moved" % (k, end))
lv = [l["z"] for l in TP["town"]["levels"]]
if lv[1:] != zs:
    probs.append("levels %s" % lv)
check("people: town_places stairs and levels = the plan's", probs)

# building flats: town_places "<plot>/<flat>" <-> the building json; windows the life lights exist in that json
EXPORT = os.path.join(REPO, "blender", "export", "city")


def bjson(btype, pid):
    for name in ("%s__%s.json" % (btype, pid.lower().replace("-", "_")), "%s.json" % btype):
        path = os.path.join(EXPORT, name)
        if os.path.exists(path):
            with open(path, encoding="utf-8") as f:
                return json.load(f)
    return None


probs, probs_w, probs_d = [], [], []
by_plot = defaultdict(list)
for k, f in flats.items():
    if "/" in k:
        by_plot[k.split("/")[0]].append(f)
for p in PL:
    data = bjson(p["building"], p["id"]) if p["building"] else None
    have = by_plot.get(p["id"], [])
    if p["kind"] == "residential" and not have:
        probs.append("%s (residential) has no flats in town_places" % p["id"])
    if not data:
        if have and not all(f["windows"] == [] for f in have):
            probs.append("%s has windows but no building json" % p["id"])
        continue
    jflats = {"%s/%s" % (p["id"], f["flat"]) for f in data.get("flats", [])}
    tflats = {f["id"] for f in have}
    if have and jflats != tflats:
        probs.append("%s: %d flats in json, %d in town_places (%s)" % (p["id"], len(jflats), len(tflats), sorted(jflats ^ tflats)[:3]))
    wins = {"%s/%s" % (p["id"], w["id"]): w for w in data.get("windows", [])}
    flat_of = defaultdict(list)
    for w in data.get("windows", []):
        if w.get("flat"):
            flat_of["%s/%s" % (p["id"], w["flat"])].append(w)
    slots = [w["slot"] for w in data.get("windows", [])]
    if len(set(slots)) != len(slots) or (slots and (min(slots) < 0 or max(slots) >= data.get("light_tex", 64) ** 2)):
        probs_w.append("%s window slots not unique / outside the light texture" % p["id"])
    for f in have:
        for w in f["windows"]:
            if w not in wins:
                probs_w.append("%s window %s not in the building" % (f["id"], w))
            elif "%s/%s" % (p["id"], wins[w].get("flat")) != f["id"]:
                probs_w.append("%s window %s belongs to flat %s" % (f["id"], w, wins[w].get("flat")))
        if not flat_of.get(f["id"]):
            probs_w.append("%s has no window for SetFlatLit" % f["id"])
    for fid in flat_of:
        if fid not in jflats:
            probs_w.append("window flat %s not a flat" % fid)
    # entrances (the flats' doors) on the plot's ground, near the plot
    front = p["front"]
    a = math.radians(p["yaw"])
    for e in data.get("entrances", []):
        lx, ly, lz = e["pos"]
        x = front[0] + lx * math.sin(a) - ly * math.cos(a)
        y = front[1] - lx * math.cos(a) - ly * math.sin(a)
        s, yy = W.inland(x, y), y
        if not in_box(PB[p["id"]], s, yy, 3.0) and p["id"] not in STRADDLE:
            probs_d.append("%s entrance %s %.1f m out of the plot" % (p["id"], e["id"], 0))
        if e.get("kind") == "street" and abs(lz) < 0.3 and p["id"] not in STRADDLE and p["terrace"] != "B" and p["id"] != "P-PIER":
            g = W.height(x, y)
            if abs(g - (front[2] + lz)) > 0.35:
                probs_d.append("%s street entrance %s at z %.2f, ground %.2f" % (p["id"], e["id"], front[2] + lz, g))
check("buildings: every residential plot has flats; town_places flats = the building json's", probs)
check("buildings: every window the life lights exists, belongs to its flat, every flat has one (ULyudWindowLights)", probs_w)
check("buildings: street entrances in their plot at the ground's level", probs_d)

probs = []
homes = Counter(p["home"] for p in FOLK)
for h in homes:
    if h not in flats:
        probs.append("home %s not a flat" % h)
cap = {}
for k, f in flats.items():
    cap[k] = sum(s.get("capacity", 1) for s in f["slots"] if s["kind"] == "bed")
for h, n in homes.items():
    if h in cap and n > cap[h]:
        probs.append("%s: %d residents, %d beds" % (h, n, cap[h]))
hh = defaultdict(set)
for p in FOLK:
    hh[p["household"]].add(p["home"])
probs += ["household %s in %d homes" % (k, len(v)) for k, v in hh.items() if len(v) != 1]
for p in FOLK:
    for d in p["duties"]:
        pl = d.get("place")
        if pl and pl not in places and not any(q["kind"] == pl for q in TP["places"]):
            probs.append("%s works at %s" % (p["key"], pl))
check("people: every resident's home is a flat with a bed for them, households share one home, workplaces exist", probs)

probs = []
keys = Counter(p["key"] for p in FOLK)
probs += ["key %s x%d" % (k, n) for k, n in keys.items() if n > 1]
n_wall = sum(1 for h in homes if h in wall_flats)
if n_wall == 0 or len(homes) < 100:
    probs.append("only %d homes (%d in the walls)" % (len(homes), n_wall))
check("people: %d residents in %d homes (%d wall flats lived in, of %d)" % (len(FOLK), len(homes), n_wall, len(wall_flats)), probs)

probs = []
plan_spot_ids = {s["id"] for s in PLAN["spots"]}
tp_ids = {s["id"] for s in TPLAN["spots"]}
ps_ids = {s["id"] for s in PS["spots"]}
if tp_ids != ps_ids:
    probs.append("town_plan.json spots != public_spaces.json spots: %s" % sorted(tp_ids ^ ps_ids)[:5])
probs += ["plan spot %s missing in public spaces / town_places" % s for s in sorted(plan_spot_ids) if s not in ps_ids and s not in places]
for s in TPLAN["spots"]:
    if s["id"] not in places and s.get("season") != "summer" and s.get("sub") != "ride":
        probs.append("spot %s not in town_places" % s["id"])
check("people: every plan spot is furnished or passed on; town_plan spots = public_spaces spots, all in town_places", probs)

probs = []
for k, f in flats.items():
    wall = re.match(r"W[2-5]-D\d{3}$", k) is not None
    # UPeopleLifeSubsystem::SendFlatLight routes by the slash: none = the walls' ALyudCityWindows, one = ULyudWindowLights
    if wall == ("/" in k) or (not wall and (k.count("/") != 1 or k.split("/")[0] not in PB)):
        probs.append(k)
check("people: flat ids route to the right light owner (wall flats no '/', building flats '<plot>/<flat>')", probs)

# ------------------------------------------------------------------------------------------- the restaurant

import io  # noqa: E402
sys.path.insert(0, os.path.join(REPO, "tools", "restaurant"))
import seats as SEATS  # noqa: E402

captured = {}


def _fake_open(path, mode="r", *a, **k):
    if "w" in mode:
        buf = io.StringIO()
        buf.close = lambda: captured.__setitem__(path, buf.getvalue())
        return buf
    return open(path, mode, *a, **k)


SEATS.open = _fake_open
SEATS.print = lambda *a, **k: None
SEATS.main()
regen = json.loads(next(iter(captured.values()))) if captured else None
committed = load("Restaurant/seats.json")
check("restaurant: tools/restaurant/seats.py rebuilds the committed seats.json from the plan's P-RESTAURANT",
      [] if regen == committed else ["seats.json differs from a rerun"])
probs = []
rp = next(p for p in PL if p["id"] == "P-RESTAURANT")
for sname, pt in [("seat %s" % s["table"], s["pos"]) for s in committed["seats"]] + \
        [("entrance " + k, v) for k, v in committed["entrance"].items()]:
    s, y = W.inland(pt[0], pt[1] - FACE_Y / 100.0), pt[1] - FACE_Y / 100.0
    if not in_box(PB["P-RESTAURANT"], s, y, 1.0):
        probs.append("%s outside the plot" % sname)
for k in ("outside", "back_outside"):
    pt = committed["entrance"][k]
    g = C.terrace_z(pt[0], pt[1] - FACE_Y / 100.0)
    if abs(pt[2] - g) > 0.05:
        probs.append("entrance %s at z %.2f, ground %.2f" % (k, pt[2], g))
check("restaurant: seats and doors on its plot, the outside doors on the terrace's ground", probs)

print("all ok" if not FAILS else "%d check(s) failed" % len(FAILS))
sys.exit(1 if FAILS else 0)
