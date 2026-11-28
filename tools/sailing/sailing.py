#!/usr/bin/env python3
"""The yacht school's rules and places (docs/yacht_school.md, part B): writes the game's data from the source.

Source:   tools/sailing/content.json   (places in the terrace frame, the season, limits, the timetable, lessons, races, cups)
Reads:    tools/city_plan.py, tools/world_map.py (the coast, the terraces, the pier), tools/terrain.py (only with --built)
          Content/City/city_plan.json, public_spaces.json, winter_play.json, Content/Fishing/fishing.json,
          Content/Railway/railway.json, Content/PeopleData/*.json   (what the school must stand clear of)
Writes:   unreal/Lyudmilovsk/Content/City/sailing.json            (LyudSail::FSchoolParams, Private/Sailing/SailSchoolModel.h)
          unreal/Lyudmilovsk/Content/Dev/teleports/sailing.json   (dev mode, group "sailing")
          tools/sailing/strings_needed.json                       (every string key the rules refer to, for the words' part)

  python3 tools/sailing/sailing.py           write everything
  python3 tools/sailing/sailing.py --check   write nothing; fail when a generated file is stale or a check fails
  python3 tools/sailing/sailing.py --built   heights and the bed from the built ground (Saved/World/terrain_out.bin: the Mac)

Heights. Without --built every height and the bed's profile come from the macro shape (tools/world_map.py height()), the
same on every machine, and sailing.json says "bed_source": "macro": the bars, runnels and ripples TerrainGen adds are not in
it. On the Mac, after the terrain is built, run with --built once and commit: "bed_source": "built". --check compares in the
mode the committed file was written in (and only validates when that is "built" and the built ground is not here).
The game still traces the ground under every place and asks ALyudSea for the depth; these are the people's places and the
rules' first guess.
"""
import json
import math
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOLS = os.path.join(ROOT, "tools")
HERE = os.path.join(TOOLS, "sailing")
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
OUT_DATA = os.path.join(CONTENT, "City", "sailing.json")
OUT_TELEPORTS = os.path.join(CONTENT, "Dev", "teleports", "sailing.json")
OUT_STRINGS_NEEDED = os.path.join(HERE, "strings_needed.json")
STRINGS = os.path.join(HERE, "strings.json")       # the words' part writes it; checked against strings_needed when there
PEOPLE_FILES = ("people_places.json", "town_places.json", "town_plan.json", "winter_spots.json")

sys.path.insert(0, TOOLS)
_err = sys.stderr
sys.stderr = open(os.devnull, "w")      # terrain.py warns on import when the built ground is missing
try:
    import city_plan as C  # noqa: E402
    import terrain as T  # noqa: E402
    import world_map as W  # noqa: E402
finally:
    sys.stderr.close()
    sys.stderr = _err

errors = []
BOATS = ("optimist", "cadet", "luch")
OBJECTIVES = {"event": ("event", "count"), "follow": ("seconds",), "hold": ("twa", "seconds"), "tacks": ("count",),
              "gybes": ("count",), "course": ("course", "laps"), "right": ("count", "within_s"), "astern": ("seconds",),
              "stop": ("radius_m", "max_speed", "seconds"), "pickup": ("count", "radius_m", "max_speed"),
              "land": ("radius_m", "max_speed"), "start": ("within_s",)}
COURSES = ("windward_leeward", "triangle", "kids_triangle", "figure8", "start_line")
PHASE_KINDS = ("lesson", "shore", "theory", "race", "recall", "fitout", "layup")
# the contract's numbers (docs/yacht_school.md "The school"): content.json may not drift from them
CONTRACT_WIND = {"optimist": [1.5, 8.0], "cadet": [1.5, 10.0], "luch": [1.5, 12.0]}


def err(msg):
    errors.append(msg)


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def dump(obj):
    return json.dumps(obj, ensure_ascii=False, indent=1) + "\n"


def r3(v):
    return [round(float(c), 3) for c in v]


def r2(v):
    return [round(float(c), 2) for c in v]


def strip_docs(v):
    if isinstance(v, dict):
        return {k: strip_docs(x) for k, x in v.items() if k != "_doc"}
    if isinstance(v, list):
        return [strip_docs(x) for x in v]
    return v


def built_ground_there():
    return os.path.exists(os.path.join(T.default_dir(), "terrain_out.bin"))


class Site:
    """The school's own frame: orthonormal, anchored at the gate (the slip's head), one axis to the sea along the coast's
    normal there, the other north along the coast. content.json's (s, y) are read in it: they agree with the terrace frame
    at the anchor and to half a metre anywhere on the school's ground, and rectangles stay rectangles in the world.
    Points on the pier's deck and the promenade are read in the plain terrace frame (to_map)."""

    def __init__(self, content, face_y, built):
        sl = content["school"]["slip"]
        self.face_y = face_y
        self.built = built
        self.s0, self.y0 = sl["s"][0], sl["y"]
        self.ox, self.oy = C.to_map(self.s0, self.y0)
        self.yaw = C.seaward_yaw(self.y0)
        a = math.radians(self.yaw)
        self.f = (math.cos(a), math.sin(a))          # to the sea
        self.n = (-math.sin(a), math.cos(a))         # north along the coast

    def map(self, s, y, frame="school"):
        if frame == "terrace":
            return C.to_map(s, y)
        ds, dy = self.s0 - s, y - self.y0
        return (self.ox + self.f[0] * ds + self.n[0] * dy, self.oy + self.f[1] * ds + self.n[1] * dy)

    def ground(self, x, y):
        return float(T.ground_z(x, y)) if self.built else float(W.height(x, y))

    def ue(self, x, y, z):
        return r3((x, y + self.face_y, z))

    def at(self, s, y, dz=0.0, frame="school"):
        x, yy = self.map(s, y, frame)
        return self.ue(x, yy, self.ground(x, yy) + dz)

    def face(self, f):
        if isinstance(f, (int, float)):
            return round(float(f), 2)
        return round({"sea": self.yaw, "land": self.yaw + 180.0, "north": self.yaw + 90.0, "south": self.yaw - 90.0}[f], 2)

    def rect(self, s, y):
        """A footprint: s [from, to] x y [from, to] -> the middle, the yaw its front (seaward side) faces, size
        [to the sea, along the coast], the corners (Unreal metres, anticlockwise from the seaward-south one)."""
        cs, cy = (s[0] + s[1]) / 2.0, (y[0] + y[1]) / 2.0
        cx, cyy = self.map(cs, cy)
        corners = [self.map(a, b) for a, b in ((s[0], y[0]), (s[0], y[1]), (s[1], y[1]), (s[1], y[0]))]
        return {"at": self.ue(cx, cyy, self.ground(cx, cyy)), "yaw": round(self.yaw, 2),
                "size": [round(abs(s[1] - s[0]), 2), round(abs(y[1] - y[0]), 2)],
                "corners": [r2((px, py + self.face_y)) for px, py in corners],
                "ground_z": [round(self.ground(px, py), 2) for px, py in corners],
                "s": list(s), "y": list(y)}


# --- geometry for the checks (Unreal metres) -------------------------------------------------------------------------------

def seg_dist(p, a, b):
    ax, ay = b[0] - a[0], b[1] - a[1]
    l2 = ax * ax + ay * ay
    t = 0.0 if l2 < 1e-12 else max(0.0, min(1.0, ((p[0] - a[0]) * ax + (p[1] - a[1]) * ay) / l2))
    return math.hypot(p[0] - a[0] - ax * t, p[1] - a[1] - ay * t)


def inside(p, poly):
    c = False
    n = len(poly)
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        if (a[1] > p[1]) != (b[1] > p[1]) and p[0] < (b[0] - a[0]) * (p[1] - a[1]) / (b[1] - a[1]) + a[0]:
            c = not c
    return c


def poly_point_dist(p, poly):
    """0 inside, else the distance to the outline."""
    if inside(p, poly):
        return 0.0
    return min(seg_dist(p, poly[i], poly[(i + 1) % len(poly)]) for i in range(len(poly)))


def segs_cross(a, b, c, d):
    def o(p, q, r):
        return (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])
    return o(a, b, c) * o(a, b, d) < 0 and o(c, d, a) * o(c, d, b) < 0


def poly_seg_dist(poly, a, b):
    if inside(a, poly) or inside(b, poly):
        return 0.0
    n = len(poly)
    best = 1e18
    for i in range(n):
        c, d = poly[i], poly[(i + 1) % n]
        if segs_cross(a, b, c, d):
            return 0.0
        best = min(best, seg_dist(a, c, d), seg_dist(b, c, d), seg_dist(c, a, b), seg_dist(d, a, b))
    return best


def poly_poly_dist(p, q):
    return min(min(poly_seg_dist(p, q[i], q[(i + 1) % len(q)]) for i in range(len(q))),
               min(poly_seg_dist(q, p[i], p[(i + 1) % len(p)]) for i in range(len(p))))


def is_simple(poly):
    n = len(poly)
    for i in range(n):
        for j in range(i + 2, n):
            if i == 0 and j == n - 1:
                continue
            if segs_cross(poly[i], poly[(i + 1) % n], poly[j], poly[(j + 1) % n]):
                return False
    return True


# --- the school's ground ------------------------------------------------------------------------------------------------

def expand_point(site, key, p, floor_z, deck_z, prom_z):
    """A named place -> {"pos", "yaw"} or, with n / rows, a list of slots."""
    level = p.get("level", "ground")
    frame = "terrace" if level in ("deck", "promenade") else "school"
    n, rows = int(p.get("n", 1)), int(p.get("rows", 1))
    along, step = p.get("along", "y"), float(p.get("step", 0.0))
    row_along = p.get("row_along", "y" if along == "s" else "s")
    row_step = float(p.get("row_step", 0.0))
    out = []
    for r in range(rows):
        for k in range(n):
            o = (k - (n - 1) / 2.0) * step
            s = p["s"] + (o if along == "s" else 0.0) + (r * row_step if row_along == "s" else 0.0)
            y = p["y"] + (o if along == "y" else 0.0) + (r * row_step if row_along == "y" else 0.0)
            x, yy = site.map(s, y, frame)
            z = {"ground": site.ground(x, yy), "floor": floor_z, "deck": deck_z, "promenade": prom_z, "water": W.SEA_Z}[level]
            out.append({"pos": site.ue(x, yy, z), "yaw": site.face(p.get("face", "sea"))})
    return out


def build_school(content, site):
    sc = content["school"]
    bh, pk, sl = sc["boathouse"], sc["park"], sc["slip"]
    out = {"id": sc["id"], "kind": sc["kind"], "name": sc["name"], "yaw": round(site.yaw, 2)}
    bdx, bdy = site.map(bh["boat_door"]["s"], bh["y"][1])
    floor_z = round(site.ground(bdx, bdy) + bh["floor_over_door_ground"], 2)
    deck_z = W.SEA_Z + 1.5
    prom_z = C.TERRACES[0][3]
    b = site.rect(bh["s"], bh["y"])
    b.update({"plan_id": bh["plan_id"], "wall_h": bh["wall_h"], "ridge_h": bh["ridge_h"], "floor_z": floor_z,
              "classroom_s": bh["classroom_s"],
              "door": {"pos": site.ue(*site.map(bh["door"]["s"], bh["y"][1]), floor_z), "w": bh["door"]["w"], "yaw": site.face("north")},
              "boat_door": {"pos": site.ue(bdx, bdy, floor_z), "w": bh["boat_door"]["w"], "yaw": site.face("north")}})
    out["boathouse"] = b
    p = site.rect(pk["s"], pk["y"])
    p.update({"plan_id": pk["plan_id"], "fence_h": pk["fence_h"],
              "gate": {"pos": site.at(pk["s"][0], (pk["gate_y"][0] + pk["gate_y"][1]) / 2.0), "w": round(pk["gate_y"][1] - pk["gate_y"][0], 2),
                       "yaw": site.face("sea")}})
    out["park"] = p
    hw = sl["width"] / 2.0
    s_rect = site.rect(sl["s"], [sl["y"] - hw, sl["y"] + hw])
    s_rect.update({"plan_id": sl["plan_id"], "width": sl["width"], "head": site.at(sl["s"][0], sl["y"]), "foot": site.at(sl["s"][1], sl["y"])})
    out["slip"] = s_rect

    # the park's slots: every boat on its trolley, bow to the sea; the crew rigs it standing at its south side
    slots = []
    count = {}
    for row in pk["rows"]:
        y = pk["rows_y0"]
        for cls, n, pitch, ranks in row["boats"]:
            per = int(math.ceil(n / float(ranks)))
            depth = (row["s"][1] - row["s"][0]) / ranks
            for k in range(n):
                rk, col = k // per, k % per
                s = row["s"][0] + (rk + 0.5) * depth
                yy = y + (col + 0.5) * pitch
                count[cls] = count.get(cls, 0) + 1
                slots.append({"id": "%s_%d" % (cls, count[cls]), "boat": cls, "pos": site.at(s, yy), "yaw": site.face("sea"),
                              "rig": site.at(s, yy - pitch / 2.0 + 0.2)})
            y += per * pitch
        if y > pk["y"][1] - 0.2:
            err("park row at s %s runs past the fence (y %.1f > %.1f)" % (row["s"], y, pk["y"][1] - 0.2))
        if row["s"][0] < pk["s"][0] or row["s"][1] > pk["s"][1]:
            err("park row at s %s lies outside the park" % (row["s"],))
    for cls, n in content["fleet"]["boats"].items():
        if count.get(cls, 0) != n:
            err("fleet.boats says %d %s, the park's rows hold %d" % (n, cls, count.get(cls, 0)))
    order = {"luch": 0, "cadet": 1, "optimist": 2, "launch": 3}
    slots.sort(key=lambda q: (order[q["boat"]], int(q["id"].rsplit("_", 1)[1])))
    out["park_slots"] = slots
    if content["fleet"]["hero_boat"] not in [q["id"] for q in slots]:
        err("fleet.hero_boat %s is not a park slot" % content["fleet"]["hero_boat"])

    # laid up: the same boats on the hall's racks
    hl = sc["hall"]
    cells = [(t, bs, cy) for t in range(hl["tiers"]) for bs in hl["bays_s"] for cy in hl["columns_y"]]
    hall = []
    stack = [q for q in slots if q["boat"] != "launch"]
    if len(stack) > len(cells):
        err("the hall's racks hold %d boats, the fleet has %d" % (len(cells), len(stack)))
    for q, (t, bs, cy) in zip(stack, cells):
        x, yy = site.map(bs, cy)
        hall.append({"id": q["id"], "pos": site.ue(x, yy, floor_z + 0.35 + t * hl["tier_h"]), "yaw": site.face("sea"), "tier": t})
    lx, ly = site.map(*hl["launch"])
    hall.append({"id": "launch_1", "pos": site.ue(lx, ly, floor_z), "yaw": site.face("sea"), "tier": 0})
    out["hall_slots"] = hall

    points = {}
    for key, pt in sc["points"].items():
        if key == "_doc":
            continue
        got = expand_point(site, key, pt, floor_z, deck_z, prom_z)
        if key == "windsock":
            got[0]["pos"][2] = round(floor_z + bh["ridge_h"] + pt.get("z_over_ridge", 0.0), 3)
        points[key] = {"pos": got[0]["pos"] if len(got) == 1 else r3([sum(g["pos"][i] for g in got) / len(got) for i in range(3)]),
                       "yaw": got[0]["yaw"]}
        if len(got) > 1:
            points[key]["slots"] = got
    out["points"] = points
    return out


def rect_poly(rect, grow=0.0):
    """A footprint's corners grown by grow metres (Unreal metres)."""
    cx, cy = rect["at"][0], rect["at"][1]
    out = []
    for px, py in rect["corners"]:
        dx, dy = px - cx, py - cy
        hx, hy = rect["size"][0] / 2.0, rect["size"][1] / 2.0
        a = math.radians(rect["yaw"])
        fx, fy = math.cos(a), math.sin(a)
        u, v = dx * fx + dy * fy, -dx * fy + dy * fx
        u += math.copysign(grow, u) if abs(u) > hx * 0.5 else 0.0
        v += math.copysign(grow, v) if abs(v) > hy * 0.5 else 0.0
        out.append((cx + u * fx - v * fy, cy + u * fy + v * fx))
    return out


def walk_pos(obj, parent=None, path=""):
    """Every {"pos": [x, y, ..]} in a people's file, with the nearest enclosing id."""
    out = []
    if isinstance(obj, dict):
        pid = obj.get("id", parent)
        p = obj.get("pos")
        if isinstance(p, list) and len(p) >= 2 and all(isinstance(c, (int, float)) for c in p[:2]):
            out.append((pid, obj.get("kind"), p))
        for k, v in obj.items():
            out += walk_pos(v, pid, path + "/" + k)
    elif isinstance(obj, list):
        for v in obj:
            out += walk_pos(v, parent, path)
    return out


def check_footprints(content, site, school):
    """The boathouse, the park and the slip against everything already on the beach. Hard things (plots, props, the
    banya, stairs, the seawall, the pier) are errors; the people's free slots on the sand inside are listed as displaced
    (the people's part moves or drops them: nobody edits the town's generated files from here)."""
    sc = content["school"]
    clear = sc["clearance_m"]
    hard = sc["hard"]
    fy = site.face_y
    feet = {k: school[k] for k in ("boathouse", "park", "slip")}
    polys = {k: rect_poly(v) for k, v in feet.items()}

    def hit(p, r):
        for k, poly in polys.items():
            if poly_point_dist(p, poly) < r + clear:
                return k
        return None

    # the seawall behind and the pier beside
    for k in ("boathouse", "park"):
        for (px, py) in feet[k]["corners"]:
            s = W.shore_x(py - fy) - px
            if s > C.SEAWALL_S - 1.0:
                err("%s reaches s %.2f: under 1 m from the seawall (s %.0f)" % (k, s, C.SEAWALL_S))
            if py - fy > hard["pier_y"][0] - 1.2:
                err("%s reaches y %.2f: under 1.2 m from the pier's deck (y %.0f)" % (k, py - fy, hard["pier_y"][0]))
    if poly_poly_dist(polys["boathouse"], polys["park"]) > 0.3 and poly_poly_dist(rect_poly(feet["boathouse"], -0.2), rect_poly(feet["park"], -0.2)) <= 0.0:
        err("the boathouse and the park overlap")

    plan = load(os.path.join(CONTENT, "City", "city_plan.json"))
    for pl in plan["plots"]:
        if pl["id"] == "P-PIER":
            continue
        a = math.radians(pl["yaw"])
        fx, fyy = math.cos(a), math.sin(a)
        cx, cy = pl["map"][0], pl["map"][1] + fy
        hw, hd = pl["w"] / 2.0, pl["d"] / 2.0
        poly = [(cx + fx * u - fyy * v, cy + fyy * u + fx * v) for u, v in ((hd, -hw), (hd, hw), (-hd, hw), (-hd, -hw))]
        for k, mine in polys.items():
            if poly_poly_dist(mine, poly) < clear:
                err("%s overlaps the plan's plot %s" % (k, pl["id"]))
    for st in plan["stairs"]:
        a, b = (st["bottom"][0], st["bottom"][1] + fy), (st["top"][0], st["top"][1] + fy)
        for k, mine in polys.items():
            if poly_seg_dist(mine, a, b) < st.get("width", 2.0) / 2.0 + clear:
                err("%s blocks the stair %s" % (k, st["id"]))
    for sp in plan["spots"]:
        k = hit((sp["map"][0], sp["map"][1] + fy), 1.0)
        if k:
            err("%s stands on the plan's spot %s" % (k, sp["id"]))
    public = load(os.path.join(CONTENT, "City", "public_spaces.json"))
    for pr in public["props"]:
        r = hard["prop_r"].get(pr["kind"], hard["prop_r"]["default"])
        k = hit(pr["at"], r)
        if k:
            err("%s overlaps the %s at %s (%s)" % (k, pr["kind"], pr["at"][:2], pr.get("season", "all year")))
    for bx in public["boxes"]:
        k = hit(bx["c"], max(bx["size"][0], bx["size"][1]) / 2.0)
        if k:
            err("%s overlaps a %s box at %s" % (k, bx["kind"], bx["c"][:2]))
    winter = load(os.path.join(CONTENT, "City", "winter_play.json"))
    k = hit(winter["banya"]["at"], hard["banya_r"])
    if k:
        err("%s overlaps the banya" % k)

    displaced = []
    seen = set()
    files = [("City/public_spaces.json", public["spots"])]
    for name in PEOPLE_FILES:
        path = os.path.join(CONTENT, "PeopleData", name)
        if os.path.exists(path):
            files.append(("PeopleData/" + name, load(path)))
    for name, data in files:
        for pid, kind, pos in walk_pos(data):
            k = hit(pos, 0.0)
            if not k:
                continue
            key = (pid, round(pos[0], 1), round(pos[1], 1))
            if key in seen:
                continue
            seen.add(key)
            displaced.append({"by": k, "file": name, "id": pid, "kind": kind, "pos": r2(pos[:2])})
    displaced.sort(key=lambda d: (d["pos"][1], d["pos"][0], d["id"] or ""))
    return displaced


# --- the water ----------------------------------------------------------------------------------------------------------

def bed_under_datum(site, x, y):
    return site.ground(x, y) - W.SEA_Z


def build_water(content, site):
    wt = content["water"]
    fy = site.face_y
    out = {"swim_line_m": wt["swim_line_m"], "group_area": wt["group_area"], "class_area": wt["class_area"]}
    y0, y1, st = wt["shore_y"]
    shore = []
    y = y0
    while y <= y1 + 1e-6:
        shore.append([round(y + fy, 2), round(W.shore_x(y), 2)])
        y += st
    out["shore"] = shore
    areas = {}
    for key, a in wt["areas"].items():
        if key == "_doc":
            continue
        d0, d1 = a["d"]
        ya, yb = a["y"]
        n = max(1, int(math.ceil((yb - ya) / a["step_m"])))
        ys = [ya + (yb - ya) * k / n for k in range(n + 1)]
        poly = [(W.shore_x(yy) + d0, yy + fy) for yy in ys] + [(W.shore_x(yy) + d1, yy + fy) for yy in reversed(ys)]
        # the bed over the area on a 10 m grid: how deep it is, how much of it is wet at low water
        beds = []
        gy = ya + 5.0
        while gy < yb:
            gd = d0 + 5.0
            while gd < d1:
                beds.append(bed_under_datum(site, W.shore_x(gy) + gd, gy))
                gd += 10.0
            gy += 10.0
        areas[key] = {"min_depth": a["min_depth"], "d": a["d"], "poly": [r2(p) for p in poly],
                      "bed": [round(min(beds), 2), round(max(beds), 2)]}
        if not is_simple(poly):
            err("water area %s: its outline crosses itself" % key)
    out["areas"] = areas

    # the launch line: from the slip's head out along the school's seaward axis; the bed along it (under the datum)
    sl = content["school"]["slip"]
    pr = wt["profile"]
    hx, hy = site.map(sl["s"][0], sl["y"])
    bed = []
    d = 0.0
    while d <= pr["to_m"] + 1e-6:
        x, yy = hx + site.f[0] * (sl["s"][0] + d), hy + site.f[1] * (sl["s"][0] + d)
        bed.append(round(bed_under_datum(site, x, yy), 3))
        d += pr["step_m"]
    zx, zy = hx + site.f[0] * sl["s"][0], hy + site.f[1] * sl["s"][0]
    out["launch"] = {"origin": r2((zx, zy + fy)), "dir": [round(site.f[0], 5), round(site.f[1], 5)], "step_m": pr["step_m"], "bed": bed,
                     "slip_foot_d": round(-sl["s"][1], 2)}

    # what the water keeps clear of
    pier = content["school"]["hard"]
    py0, py1 = pier["pier_y"]
    px0, px1 = W.shore_x(905.0) - pier["pier_s"][0], W.shore_x(905.0) - pier["pier_s"][1]
    fish = load(os.path.join(CONTENT, "Fishing", "fishing.json"))["boat"]
    winter = load(os.path.join(CONTENT, "City", "winter_play.json"))
    rail = load(os.path.join(CONTENT, "Railway", "railway.json"))
    ln, isl = rail["line"], rail["island"]
    keep = {
        "pier": {"a": r2((px0, (py0 + py1) / 2.0 + fy)), "b": r2((px1, (py0 + py1) / 2.0 + fy)), "half_w": (py1 - py0) / 2.0,
                 "clear_m": wt["keep_clear"]["pier_m"], "head_clear_m": wt["keep_clear"]["pier_head_m"]},
        # the rowing boat lies along the pier's south side (fishing.json: its first place; the game moves it out along the
        # pier to where it floats at low water): the whole line, from the beach to the pier's head
        "mooring": {"a": r2(((fish["ue"][0] + fish["mooring"][0]) / 100.0, (fish["ue"][1] + fish["mooring"][1]) / 100.0)),
                    "b": r2((px1, (fish["ue"][1] + fish["mooring"][1]) / 100.0)), "clear_m": wt["keep_clear"]["mooring_m"]},
        "ice_hole": {"at": r2(winter["ice_hole"]["at"][:2]), "clear_m": wt["keep_clear"]["ice_hole_m"]},
        "bank": {"a": r2((ln["portal_x"], ln["y"] + fy)), "b": r2((isl["x"] + isl["rx"], ln["y"] + fy)), "clear_m": wt["keep_clear"]["bank_m"]},
        "island": {"at": r2((isl["x"], isl["y"] + fy)), "r": max(isl["rx"], isl["ry"]), "clear_m": wt["keep_clear"]["island_m"]},
    }
    out["keep_clear"] = keep
    polys = {k: [tuple(p) for p in a["poly"]] for k, a in areas.items()}
    for k, poly in polys.items():
        d = poly_seg_dist(poly, keep["pier"]["a"], keep["pier"]["b"]) - keep["pier"]["half_w"]
        if d < keep["pier"]["clear_m"]:
            err("water area %s comes within %.1f m of the pier (%.0f wanted)" % (k, d, keep["pier"]["clear_m"]))
        if poly_point_dist(keep["pier"]["b"], poly) < keep["pier"]["head_clear_m"]:
            err("water area %s comes within %.0f m of the pier's head (the anglers)" % (k, keep["pier"]["head_clear_m"]))
        d = poly_seg_dist(poly, keep["mooring"]["a"], keep["mooring"]["b"])
        if d < keep["mooring"]["clear_m"]:
            err("water area %s comes within %.1f m of the rowing boat's mooring line" % (k, d))
        if poly_point_dist(keep["ice_hole"]["at"], poly) < keep["ice_hole"]["clear_m"]:
            err("water area %s is near the ice hole" % k)
        if poly_seg_dist(poly, keep["bank"]["a"], keep["bank"]["b"]) < keep["bank"]["clear_m"]:
            err("water area %s is near the railway's bank" % k)
        if poly_point_dist(keep["island"]["at"], poly) < keep["island"]["r"] + keep["island"]["clear_m"]:
            err("water area %s is near the railway's island" % k)
        if k != "corridor" and min(p[0] - W.shore_x(p[1] - fy) for p in poly) < wt["swim_line_m"]:
            err("water area %s comes inside the bathers' strip (%.0f m)" % (k, wt["swim_line_m"]))
    if poly_poly_dist(polys["corridor"], polys["kids"]) > 0.0 or poly_poly_dist(polys["kids"], polys["race"]) > 0.0:
        err("the water areas do not join up (corridor - kids - race)")
    if not inside((zx + site.f[0] * 5.0, zy + fy + site.f[1] * 5.0), polys["corridor"]):
        err("the launch line does not start inside the corridor")
    for g, lst in list(wt["group_area"].items()) + list(wt["class_area"].items()):
        for a in lst:
            if a not in areas:
                err("water: %s names an unknown area %s" % (g, a))
    return out


# --- the rules' cross-checks ----------------------------------------------------------------------------------------------

def check_rules(content, school):
    points = set(school["points"].keys())
    groups, coaches, fleet = content["groups"], content["coaches"], content["fleet"]
    lim, tt = content["limits"], content["timetable"]
    for cls in BOATS:
        if lim["wind"].get(cls) != CONTRACT_WIND[cls]:
            err("limits.wind.%s %s is not the contract's %s" % (cls, lim["wind"].get(cls), CONTRACT_WIND[cls]))
    if lim["launch_depth_m"] != 0.5 or lim["board_full_depth_m"] != 1.0:
        err("limits: the contract says 0.5 m of water to sail, the board half up under 1.0 m")
    se = content["season"]
    if se["opening"] != [4, 25] or se["opening_weekday"] != 5 or se["closing_month"] != 11 or se["closing_weekday"] != 6:
        err("season: the contract says the first Saturday on or after April 25 to the first Sunday of November")
    for g in ("kids", "juniors", "adults", "race"):
        if g not in groups:
            err("groups: the contract's group %s is missing" % g)
    for gid, g in groups.items():
        if gid == "_doc":
            continue
        if g["coach"] not in coaches:
            err("group %s: unknown coach %s" % (gid, g["coach"]))
        for b in g["boats"]:
            if b not in BOATS:
                err("group %s: unknown boat %s" % (gid, b))
        for d in g["drills"]:
            if d not in content["drills"]:
                err("group %s: unknown drill %s" % (gid, d))
    slots = set()
    for s in fleet["sailors"]:
        if s["slot"] in slots:
            err("fleet: sailor slot %s twice" % s["slot"])
        slots.add(s["slot"])
        if s["group"] not in groups or s["boat"] not in groups[s["group"]]["boats"]:
            err("fleet: sailor %s: group %s does not sail a %s" % (s["slot"], s["group"], s["boat"]))
    for cls in BOATS:
        n = sum(1 for s in fleet["sailors"] if s["boat"] == cls) + (1 if cls == content["hero"]["boat"] else 0)
        if n > fleet["boats"][cls]:
            err("fleet: %d sail a %s (with the hero), there are %d boats" % (n, cls, fleet["boats"][cls]))
    for kind, phases in tt["phases"].items():
        if kind == "_doc":
            continue
        if kind not in PHASE_KINDS:
            err("timetable.phases: unknown kind %s" % kind)
        if sum(1 for p in phases if p[1] == 0) != 1:
            err("timetable.phases.%s: exactly one phase takes what is left (0 minutes)" % kind)
        for p in phases:
            if p[2] != "water" and p[2] not in points:
                err("timetable.phases.%s: place %s is not one of school.points" % (kind, p[2]))
    fixed = {k: sum(p[1] for p in v) / 60.0 for k, v in tt["phases"].items() if k != "_doc"}
    events = content["races"]["events"]
    for pat in ("term", "summer"):
        busy = []
        for s in tt[pat]:
            if s["group"] not in groups:
                err("timetable.%s: unknown group %s" % (pat, s["group"]))
                continue
            if s["len"] - fixed["lesson"] < lim["min_water_h"]:
                err("timetable.%s: %s at %.2f leaves %.2f h on the water" % (pat, s["group"], s["from"], s["len"] - fixed["lesson"]))
            for d in s["days"]:
                busy.append((d, s["from"], s["from"] + s["len"], s["group"], groups[s["group"]]["coach"], set(groups[s["group"]]["boats"])))
        for e in events:
            if e["rule"] == "weekly":
                busy.append((e["weekday"], e["from"], e["from"] + e["len"], "race:" + e["id"], groups["race"]["coach"], set(e["classes"])))
        for i in range(len(busy)):
            for j in range(i + 1, len(busy)):
                a, b = busy[i], busy[j]
                if a[0] == b[0] and a[1] < b[2] and b[1] < a[2]:
                    # one launch: nothing overlaps on a day (the coach and the boats follow from that)
                    err("timetable.%s: %s and %s overlap on weekday %d (one launch, coach %s / %s)" % (pat, a[3], b[3], a[0], a[4], b[4]))
    for e in events:
        if e["rule"] not in ("opening", "closing", "date", "weekend_on_or_after", "weekly"):
            err("races.events.%s: unknown rule %s" % (e["id"], e["rule"]))
        if e["course"] not in COURSES:
            err("races.events.%s: unknown course %s" % (e["id"], e["course"]))
        for c in e["classes"]:
            if c not in BOATS:
                err("races.events.%s: unknown class %s" % (e["id"], c))
        if e.get("stamp") and e["stamp"] not in content["stamps"]["pieces"]:
            err("races.events.%s: stamp %s is not a piece" % (e["id"], e["stamp"]))
        if e["len"] - fixed["race"] < e["races"] * 0.5:
            err("races.events.%s: %d races do not fit %.2f h on the water" % (e["id"], e["races"], e["len"] - fixed["race"]))
    for _, piece in content["miles"]["stamps"]:
        if piece not in content["stamps"]["pieces"]:
            err("miles: stamp %s is not a piece" % piece)
    ids = []
    for le in content["lessons"]:
        if le["id"] in ids:
            err("lessons: %s twice" % le["id"])
        ids.append(le["id"])
        if le.get("area") and le["area"] not in content["water"]["areas"]:
            err("lesson %s: unknown area %s" % (le["id"], le["area"]))
        for k, ob in enumerate(le["objectives"]):
            need = OBJECTIVES.get(ob["kind"])
            if need is None:
                err("lesson %s objective %d: unknown kind %s" % (le["id"], k + 1, ob["kind"]))
                continue
            for f in need:
                if f not in ob:
                    err("lesson %s objective %d (%s): no %s" % (le["id"], k + 1, ob["kind"], f))
            if ob["kind"] == "course" and ob["course"] not in COURSES:
                err("lesson %s objective %d: unknown course %s" % (le["id"], k + 1, ob["course"]))
    rank_ids = [r["id"] for r in content["hero"]["ranks"]]
    for r in content["hero"]["ranks"]:
        for le in r.get("lessons", []):
            if le not in ids:
                err("rank %s: unknown lesson %s" % (r["id"], le))
    for le in content["lessons"]:
        if le.get("needs_rank") and le["needs_rank"] not in rank_ids:
            err("lesson %s: unknown rank %s" % (le["id"], le["needs_rank"]))
    if sum(1 for r in content["hero"]["ranks"] if r.get("certificate")) != 1:
        err("hero.ranks: exactly one rank carries the certificate")
    for state, sg in content["signals"].items():
        if state != "_doc" and state not in ("closed", "fitout", "shore", "wait", "sail", "race", "recall", "storm"):
            err("signals: unknown state %s" % state)
    for did, d in content["drills"].items():
        if did != "_doc" and d.get("course") and d["course"] not in COURSES:
            err("drill %s: unknown course %s" % (did, d["course"]))


def strings_needed(content, school):
    """Every string key the rules and the data refer to, with a note for whoever writes the words (part G)."""
    need = {}

    def add(key, note):
        need[key] = note
    add(content["school"]["name"], "the school's name: ДЮСШ парусного спорта «Луч»")
    for cid, c in content["coaches"].items():
        if cid != "_doc":
            add(c["name"], "staff title: " + c["role"])
    for gid in content["groups"]:
        if gid != "_doc":
            add("sail.group.%s" % gid, "the group's name on the timetable")
    for cls in BOATS + ("launch",):
        add("sail.boat.%s" % cls, "the boat's name")
    for r in content["hero"]["ranks"]:
        if r.get("name"):
            add(r["name"], "rank: " + r["id"])
    for le in content["lessons"]:
        base = "sail.lesson.%s" % le["id"]
        add(base + ".title", "the lesson's name in the membership book")
        add(base + ".intro", "the tutor before the lesson: what we do today")
        add(base + ".done", "the tutor when every objective is done")
        add(base + ".fail", "the tutor when the lesson is broken off (recall, dark, the hero left)")
        for k, ob in enumerate(le["objectives"]):
            args = ", ".join("%s=%s" % (a, ob[a]) for a in OBJECTIVES[ob["kind"]] if a in ob)
            add("%s.obj%d" % (base, k + 1), "objective on the HUD ({0} done, {1} wanted): %s %s" % (ob["kind"], args))
            add("%s.hint%d" % (base, k + 1), "the tutor's shout while the hero works at it: %s" % ob["kind"])
    for t in ("irons", "capsize", "righted", "recall", "calm", "gust", "touch", "wrong_side", "missed_mark", "ocs", "good_start",
              "shallow", "outside", "reef", "board_up", "well_done", "penalty_done"):
        add("sail.tutor.%s" % t, "the coach's shout from the launch: " + t)
    for did, d in content["drills"].items():
        if did != "_doc":
            add("sail.drill.%s" % did, "drill: " + d["what"])
    for kind, phases in content["timetable"]["phases"].items():
        if kind != "_doc":
            for p in phases:
                add("sail.phase.%s" % p[0], "a session's part, for the timetable board")
    for t in content["timetable"]["shore_topics"]:
        add("sail.shore.%s" % t, "the shore lesson's topic")
    for state in content["signals"]:
        if state != "_doc":
            add("sail.signal.%s" % state, "what the signal mast says (the hint when the hero looks at it)")
    for rs in ("season", "ice", "dark", "calm", "wind", "gusts", "thunder", "fog", "low_water", "day_off"):
        add("sail.reason.%s" % rs, "why there is no sailing ({0}: the wind, m/s)")
    for e in content["races"]["events"]:
        add("sail.race.%s.name" % e["id"], "the regatta's / the series' name")
    for st in ("dnf", "dns", "dsq", "ocs"):
        add("sail.status.%s" % st, "a result sheet's code")
    for st in ("warning", "prep", "go", "ocs", "general_recall", "finish", "abandon", "shorten", "time_limit"):
        add("sail.race.start.%s" % st, "the race officer's hail / the HUD line at this signal")
    for k, key in content["radio"]["forecast"].items():
        add(key, "radio, the sailors' forecast: %s ({0} wind m/s, {1} gusts, {2} from where, {3} the group)" % k)
    for k, key in content["radio"]["news"].items():
        add(key, "paper / radio, yesterday's racing: %s ({0} the regatta, {1} the class, {2} the winner, {3} second, {4} third, {5} the hero's place)" % k)
    for k in ("offer", "fee", "done", "no_money", "already"):
        add("sail.join.%s" % k, "joining the school with the head coach ({0}: the fee)")
    for k in ("offer", "no_cert", "closed", "taken", "done", "returned"):
        add("sail.hire.%s" % k, "hiring a boat from the bosun ({0}: the fee)")
    add("city.coll.%s" % content["stamps"]["set"], "the stamp collection's title")
    for p in content["stamps"]["pieces"]:
        add("city.coll.%s.%s" % (content["stamps"]["set"], p), "stamp")
    for s in content["fleet"]["sailors"]:
        add("sail.sailor.%s" % s["slot"], "a member's name until the people's part binds the slot to a townsperson")
    for t in content["teleports"]:
        add(t["name"], "dev mode teleport")
    for f in sorted({e["festival"] for e in content["races"]["events"] if e.get("festival", "").startswith("sail_")}):
        add("festival.%s.name" % f, "the festival's name")
        add("festival.%s.desc" % f, "the festival's line in the calendar")
    for k in ("obj_done", "lesson_done", "rank_up", "certificate", "course_next_mark", "penalty", "place", "launch_far", "book.title",
              "book.lessons", "book.races", "book.miles", "book.capsizes", "book.table"):
        add("ui.sail.%s" % k, "HUD / the membership book")
    flags = set()
    for state, sg in content["signals"].items():
        if state == "_doc":
            continue
        flags.update(sg["flags"])
        flags.update(sg.get("class_flags", {}).values())
    flags.update(content["races"]["start"]["flags"].values())
    for f in sorted(flags):
        add("ui.sail.flag.%s" % f, "a flag's meaning (the hint when the hero looks at the mast or the launch)")
    return dict(sorted(need.items()))


# The people's part binds the slots to townsfolk (tools/sailing/sailing_people.py writes this file and then runs sailing.py again;
# docs/yacht_school.md "People (part F)"): each sailor's and coach's "person" is the townsperson's key ("" while unbound).
MEMBERS = os.path.join(CONTENT, "PeopleData", "sailing_members.json")


def bind_people(data):
    if not os.path.exists(MEMBERS):
        return
    by_slot = {m["slot"]: m["person"] for m in load(MEMBERS)["members"] if m.get("slot")}
    for s in data["fleet"]["sailors"]:
        s["person"] = by_slot.get(s["slot"], "")
        if s.get("crew"):
            s["crew_person"] = by_slot.get(s["crew"], "")
    for c_id, c in data["coaches"].items():
        c["person"] = by_slot.get(c_id, "")


def main():
    check = "--check" in sys.argv
    content = load(os.path.join(HERE, "content.json"))
    public = load(os.path.join(CONTENT, "City", "public_spaces.json"))
    old = load(OUT_DATA) if os.path.exists(OUT_DATA) else None
    built = "--built" in sys.argv
    skip_compare = False
    if check and old is not None and old.get("bed_source") == "built":
        built = built_ground_there()
        skip_compare = not built
    if built and not built_ground_there():
        print("ERROR: --built needs Saved/World/terrain_out.bin (build the terrain first)")
        sys.exit(1)
    site = Site(content, public["face_y"], built)
    data = {"_doc": "Built by tools/sailing/sailing.py from tools/sailing/content.json (docs/yacht_school.md, part B). Do not edit "
                    "by hand. Unreal metres (x = map X to the sea, y = map Y + face_y, z), yaw Unreal degrees (0 = +x = the "
                    "sea). bed_source macro: heights and the bed are the macro shape's (rerun with --built on the Mac); the "
                    "game traces the ground and asks the sea for the depth.",
            "version": 1, "bed_source": "built" if built else "macro", "face_y": public["face_y"], "sea_z": W.SEA_Z,
            "tide": {"neap_low": W.TIDE_NEAP_LOW, "spring_low": W.TIDE_SPRING_LOW}}
    school = build_school(content, site)
    data["school"] = school
    school["displaced"] = check_footprints(content, site, school)
    data["water"] = build_water(content, site)
    check_rules(content, school)
    for k in ("fleet", "groups", "coaches", "season", "limits", "signals", "timetable", "drills", "course", "races", "hero", "lessons",
              "stamps", "quest_events", "miles", "radio"):
        data[k] = strip_docs(content[k])
    bind_people(data)
    need = strings_needed(content, school)
    if os.path.exists(STRINGS):
        # the words' part (G): tools/sailing/strings*.json and, for the city.* keys, the city's theme file; the festivals'
        # names are the town part's, in the main tables with every other festival's
        have = {}
        for name in sorted(os.listdir(HERE)):
            if name.startswith("strings") and name.endswith(".json") and name != "strings_needed.json":
                have.update(load(os.path.join(HERE, name)))
        city = os.path.join(ROOT, "tools", "city", "content", "strings", "x_sailing.json")
        if os.path.exists(city):
            have.update(load(city))
        for k in need:
            if k not in have and not k.startswith("festival."):
                err("the words (tools/sailing/strings*.json, tools/city/content/strings/x_sailing.json) have no %s (%s)" % (k, need[k]))
    tps = []
    for t in content["teleports"]:
        pt = school["points"].get(t["point"])
        if not pt:
            err("teleport %s: no point %s" % (t["id"], t["point"]))
            continue
        tps.append({"id": t["id"], "group": "sailing", "name": t["name"], "x": round(pt["pos"][0], 2),
                    "y": round(pt["pos"][1] - site.face_y, 2), "yaw": round(pt["yaw"] % 360.0, 1)})
    outs = {
        OUT_DATA: dump(data),
        OUT_TELEPORTS: dump({"about": "Written by tools/sailing/sailing.py: x/y map metres (docs/world_map.md), yaw Unreal degrees; "
                                      "outdoors, dropped onto the ground.", "teleports": tps}),
        OUT_STRINGS_NEEDED: dump({"_doc": "Generated by tools/sailing/sailing.py: every string key the yacht school's rules and data "
                                          "refer to, with a note. The words' part writes them (ru/en/ja) into tools/sailing/strings.json; "
                                          "once that file exists sailing.py --check fails on a missing key. Do not edit.", "keys": need}),
    }
    if errors:
        for e in errors:
            print("ERROR:", e)
        sys.exit(1)
    stale = []
    for path, text in outs.items():
        was = open(path, encoding="utf-8").read() if os.path.exists(path) else None
        if was != text:
            if check and skip_compare and path == OUT_DATA:
                continue
            stale.append(os.path.relpath(path, ROOT))
            if not check:
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, "w", encoding="utf-8") as f:
                    f.write(text)
    if check and stale:
        print("stale:", ", ".join(stale))
        sys.exit(1)
    w = data["water"]
    print("sailing: boathouse %s, park %s, slip foot %s; water %s; bed %s%s; %d people's slots displaced; %d string keys needed%s" % (
        school["boathouse"]["at"][:2], school["park"]["at"][:2], school["slip"]["foot"][:2],
        ", ".join("%s %d pts" % (k, len(a["poly"])) for k, a in w["areas"].items()), data["bed_source"],
        " (compared nothing: the committed file is from the built ground, which is not here)" if skip_compare else "",
        len(school["displaced"]), len(need), ("; wrote " + ", ".join(stale)) if stale and not check else ""))
    for d in school["displaced"]:
        print("  displaced by the %s: %s %s (%s) at %s" % (d["by"], d["file"], d["id"], d["kind"], d["pos"]))
    print("all ok")


if __name__ == "__main__":
    main()
