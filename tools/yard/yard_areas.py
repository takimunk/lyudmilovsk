#!/usr/bin/env python3
"""Children's games in the yards (docs/yard_games.md, interactives backlog item 32): where the town's children play
together. Finds the yards in front of the houses the children live in, and adds the playground, the park's lawn, the
square's north side and two stretches of the beach; for each one the games that fit it, the cover to hide behind, the
wall to count at, the chalk's square and the football field. Writes the game's data and the people's places.

Sources:  unreal/Lyudmilovsk/Content/City/city_plan.json     (plots, walls, stairs, lamps, wall flats: map metres)
          unreal/Lyudmilovsk/Content/City/public_spaces.json (props, trees, boxes, spots, waters: Unreal metres)
          unreal/Lyudmilovsk/Content/PeopleData/townsfolk.json (who lives where: the children's homes)
          unreal/Lyudmilovsk/Content/Garden/garden.json      (the allotment's fence: Unreal cm)
          tools/yard/strings.json                            (every word, {"key": {"ru", "en", "ja"}})
Writes:   unreal/Lyudmilovsk/Content/PeopleData/yard_games.json  (ULyudYardGames, Private/People/YardGames.h)
          unreal/Lyudmilovsk/Content/PeopleData/yard_spots.json  (the people's places of kind "yard", PeopleSim LoadSpots)
          unreal/Lyudmilovsk/Content/Localization/yard/strings_<lang>.json
          unreal/Lyudmilovsk/Content/Dev/teleports/yard.json

  python3 tools/yard/yard_areas.py           write everything
  python3 tools/yard/yard_areas.py --check   write nothing; fail when a generated file is stale or a check fails

Frames: everything written is in Unreal metres (world cm / 100), yaw Unreal degrees (0 = +X = toward the sea), as
public_spaces.json and the people's life; the city plan's map metres are 7.9 m off in Y (portal_face_y).
"""
import json
import math
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HERE = os.path.join(ROOT, "tools", "yard")
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
PLAN = os.path.join(CONTENT, "City", "city_plan.json")
PUBLIC = os.path.join(CONTENT, "City", "public_spaces.json")
FOLK = os.path.join(CONTENT, "PeopleData", "townsfolk.json")
GARDEN = os.path.join(CONTENT, "Garden", "garden.json")
STRINGS = os.path.join(HERE, "strings.json")
OUT_GAMES = os.path.join(CONTENT, "PeopleData", "yard_games.json")
OUT_SPOTS = os.path.join(CONTENT, "PeopleData", "yard_spots.json")
OUT_STRINGS = os.path.join(CONTENT, "Localization", "yard", "strings_%s.json")
OUT_TELEPORTS = os.path.join(CONTENT, "Dev", "teleports", "yard.json")
LANGS = ("ru", "en", "ja")

GAMES = ("hopscotch", "rope", "hide", "football", "tag")
# a yard: the clear circle round its middle (m), how far children come from to it, and the most yards
YARD_R = 10.0
YARD_CATCH = 28.0
MAX_YARDS = 9
YARD_SPACING = 45.0
# the football field: half its length (along the terrace) and half its width, clear of everything
FIELD_HALF = (12.0, 6.0)
# how far behind a thing a child crouches (m from its middle, away from the seeker's wall), by kind
COVER_SIZE = {
    "tree": 0.9, "bench_park": 1.0, "bench_promenade": 1.0, "sandpit": 1.9, "swings": 1.6, "slide": 1.5, "climber": 1.5,
    "seesaw": 1.4, "carousel": 2.3, "stall": 2.0, "flowerbed": 1.6, "changing_booth": 1.3, "corner": 1.1, "stair": 1.6,
    "kiosk": 3.6, "bush": 1.1, "lifeguard": 2.6, "planter": 1.3, "shed": 2.0, "barrel": 0.8, "board": 0.9,
}
# the things in the way (m, the radius of a circle round each)
PROP_R = {
    "lamp": 0.4, "lamp_park": 0.4, "bin": 0.4, "bench_park": 1.2, "bench_promenade": 1.2, "tree_grate": 0.9, "lounger": 1.2,
    "stall": 2.2, "flowerbed": 1.8, "changing_booth": 1.3, "umbrella": 0.5, "sandpit": 2.0, "swings": 2.2, "slide": 2.0,
    "climber": 1.8, "seesaw": 1.8, "carousel": 3.0, "wheel": 6.0, "stage": 6.0, "railing": 0.3, "railing_post": 0.3,
}


def map_to_ue(p):
    """City plan map metres -> Unreal metres."""
    return (p[0], p[1] + PLAN_DATA["portal_face_y"] / 100.0)


def rnd(v, n=3):
    return round(float(v), n)


def dirv(yaw):
    a = math.radians(yaw)
    return (math.cos(a), math.sin(a))


class Rect:
    """A plot or a box seen from above: centre, facing (yaw), half depth along it, half width across."""

    def __init__(self, c, yaw, hd, hw, what):
        self.c, self.yaw, self.hd, self.hw, self.what = c, yaw, hd, hw, what
        self.f = dirv(yaw)
        self.a = (-self.f[1], self.f[0])

    def local(self, p):
        dx, dy = p[0] - self.c[0], p[1] - self.c[1]
        return dx * self.f[0] + dy * self.f[1], dx * self.a[0] + dy * self.a[1]

    def dist(self, p):
        u, v = self.local(p)
        du, dv = max(abs(u) - self.hd, 0.0), max(abs(v) - self.hw, 0.0)
        return math.hypot(du, dv)

    def corners(self):
        out = []
        for su in (-1, 1):
            for sv in (-1, 1):
                out.append(((self.c[0] + self.f[0] * su * self.hd + self.a[0] * sv * self.hw,
                             self.c[1] + self.f[1] * su * self.hd + self.a[1] * sv * self.hw), (su, sv)))
        return out

    def out_dir(self, signs):
        su, sv = signs
        x = self.f[0] * su + self.a[0] * sv
        y = self.f[1] * su + self.a[1] * sv
        n = math.hypot(x, y)
        return (x / n, y / n)


def seg_dist(p, a, b):
    ax, ay = b[0] - a[0], b[1] - a[1]
    L = ax * ax + ay * ay
    t = 0.0 if L < 1e-9 else max(0.0, min(1.0, ((p[0] - a[0]) * ax + (p[1] - a[1]) * ay) / L))
    return math.hypot(p[0] - (a[0] + ax * t), p[1] - (a[1] + ay * t))


def wall_x(wall, y_map):
    """The wall face's map X at map Y (its line is sampled every 10 m)."""
    line = wall["line"]
    if y_map <= line[0][1]:
        return line[0][0]
    for (x0, y0), (x1, y1) in zip(line, line[1:]):
        if y0 <= y_map <= y1:
            t = (y_map - y0) / (y1 - y0)
            return x0 + (x1 - x0) * t
    return line[-1][0]


def load():
    global PLAN_DATA
    with open(PLAN) as f:
        PLAN_DATA = json.load(f)
    with open(PUBLIC) as f:
        pub = json.load(f)
    with open(FOLK) as f:
        folk = json.load(f)
    with open(GARDEN) as f:
        garden = json.load(f)
    with open(STRINGS) as f:
        strings = json.load(f)
    return PLAN_DATA, pub, folk, garden, strings


class World:
    """What stands where (Unreal metres): the obstacles, the cover, the terraces' edges."""

    def __init__(self, plan, pub, garden):
        self.plan, self.pub = plan, pub
        self.rects = []      # plots and solid boxes
        self.circles = []    # (centre, r, kind)
        self.segs = []       # stairs: (a, b, half width)
        self.cover = []      # (point, size, kind, id)
        self.terrace_z = {t["id"]: t["z"] for t in plan["terraces"]}
        for p in plan["plots"]:
            if p["kind"] in ("square", "park"):
                continue    # open ground, not a building: what stands in them is in public_spaces
            c = map_to_ue(p["map"])
            r = Rect(c, p["yaw"], p["d"] / 2.0, p["w"] / 2.0, p["id"])
            self.rects.append(r)
            if p["kind"] in ("residential", "school", "clinic", "club", "library", "shop", "culture", "hall", "office", "hotel"):
                for pt, signs in r.corners():
                    o = r.out_dir(signs)
                    self.cover.append(((pt[0] + o[0] * 1.1, pt[1] + o[1] * 1.1), COVER_SIZE["corner"], "corner", p["id"]))
            elif p["kind"] == "kiosk":
                self.cover.append((c, COVER_SIZE["kiosk"], "kiosk", p["id"]))
            elif p["kind"] == "lifeguard":
                self.cover.append((c, COVER_SIZE["lifeguard"], "lifeguard", p["id"]))
        for b in pub["boxes"]:
            if b["kind"] in ("path", "playground", "promenade", "square", "square_ring", "fair", "rill_bridge"):
                continue    # paving to walk on
            self.rects.append(Rect((b["c"][0], b["c"][1]), b["yaw"], b["size"][0] / 2.0, b["size"][1] / 2.0, b["kind"]))
            if b["kind"] == "planter":
                self.cover.append(((b["c"][0], b["c"][1]), COVER_SIZE["planter"], "planter", "planter"))
        for p in pub["props"]:
            k = p["kind"]
            if k == "pond":
                continue
            self.circles.append(((p["at"][0], p["at"][1]), PROP_R.get(k, 1.0), k))
            if k in COVER_SIZE:
                self.cover.append(((p["at"][0], p["at"][1]), COVER_SIZE[k], k, k))
        for t in pub["trees"]:
            self.circles.append(((t["at"][0], t["at"][1]), 0.8, "tree"))
            self.cover.append(((t["at"][0], t["at"][1]), COVER_SIZE["tree"], "tree", t["species"]))
        for w in pub["waters"]:
            r = max(w.get("r", w.get("rect", [3, 3])))
            self.circles.append(((w["at"][0], w["at"][1]), r + 1.5, "water"))
        for f in pub.get("fountains", []):
            self.circles.append(((f["at"][0], f["at"][1]), f["stone"]["rim_r"] + 1.0, "fountain"))
        for s in plan["stairs"]:
            a, b = map_to_ue(s["bottom"]), map_to_ue(s["top"])
            self.segs.append((a, b, s["width"] / 2.0 + 0.8))
            if s["kind"] == "wall_stair":
                self.cover.append((a, COVER_SIZE["stair"], "stair", s["id"]))
        for l in plan["lamps"]:
            self.circles.append((map_to_ue(l["foot"]), 0.5, "lamp"))
        site = garden.get("site")
        if site:
            c = (site["ue"][0] / 100.0, site["ue"][1] / 100.0)
            self.rects.append(Rect(c, site["yaw"], site["size"][0] / 2.0 + 1.0, site["size"][1] / 2.0 + 1.0, "allotment"))
        for p in garden.get("props", []):
            if p["kind"] in COVER_SIZE:
                c = (p["ue"][0] / 100.0, p["ue"][1] / 100.0)
                self.cover.append((c, COVER_SIZE[p["kind"]], p["kind"], "garden"))
        self.walls = {w["id"]: w for w in plan["walls"]}

    def clearance(self, p, skip=()):
        """How far the nearest thing in the way is (m)."""
        d = 1e9
        for r in self.rects:
            if r.what not in skip:
                d = min(d, r.dist(p))
        for c, r, _ in self.circles:
            d = min(d, math.hypot(p[0] - c[0], p[1] - c[1]) - r)
        for a, b, hw in self.segs:
            d = min(d, seg_dist(p, a, b) - hw)
        return d

    def terrace_room(self, p, terrace):
        """How far p is inside its terrace (m): from the drop at the lower wall's face and from the upper wall's face; the
        beach and the promenade are not walled terraces here (a large number)."""
        y_map = p[1] - self.plan["portal_face_y"] / 100.0
        lower = next((w for w in self.walls.values() if w["above"] == terrace), None)
        upper = next((w for w in self.walls.values() if w["below"] == terrace), None)
        room = 1e9
        if lower:
            room = min(room, wall_x(lower, y_map) - p[0])     # the sea is +X: the drop is seaward of p
        if upper:
            room = min(room, p[0] - wall_x(upper, y_map))
        return room

    def cover_near(self, centre, radius, wall):
        """Hiding places within reach of the area: behind each thing, away from the counting wall; at least 2.5 m apart."""
        out = []
        for pt, size, kind, src in self.cover:
            d = math.hypot(pt[0] - centre[0], pt[1] - centre[1])
            if d > radius or d < 3.0:
                continue
            ax, ay = pt[0] - wall[0], pt[1] - wall[1]
            n = math.hypot(ax, ay)
            if n < 4.0:
                continue
            hide = (pt[0] + ax / n * size, pt[1] + ay / n * size)
            if self.clearance(hide, skip=(src,)) < -0.2 and kind not in ("corner", "kiosk", "lifeguard", "stair"):
                continue
            if any(math.hypot(hide[0] - o["at"][0], hide[1] - o["at"][1]) < 2.5 for o in out):
                continue
            out.append({"at": [rnd(hide[0]), rnd(hide[1])], "behind": [rnd(pt[0]), rnd(pt[1])], "kind": kind})
        out.sort(key=lambda o: math.hypot(o["at"][0] - centre[0], o["at"][1] - centre[1]))
        return out[:10]


def kid_homes(plan, folk):
    """The children's homes (5-13): Unreal metres -> how many children, with their terrace."""
    plots = {p["id"]: p for p in plan["plots"]}
    dwell = {d["id"]: d for d in plan["wall_dwellings"]}
    homes = {}
    for p in folk["people"]:
        if not 5 <= p["age"] <= 13:
            continue
        h = p["home"]
        pid = h.split("/")[0]
        if pid in plots:
            key, at, ter = pid, map_to_ue(plots[pid]["map"]), plots[pid]["terrace"]
        elif h in dwell:
            d = dwell[h]
            key, at, ter = d["wall"], map_to_ue(d["door"]), d["terrace"]
            key = h
        else:
            continue
        e = homes.setdefault(key, {"at": at, "terrace": ter, "kids": 0, "plot": plots.get(pid)})
        e["kids"] += 1
    return homes


def find_yards(world, homes):
    """In front of each house with children (and among the wall flats): the clear spot nearest the most children."""
    cands = []
    for key, h in homes.items():
        plot = h["plot"]
        pts = []
        if plot is not None:
            r = Rect(map_to_ue(plot["map"]), plot["yaw"], plot["d"] / 2.0, plot["w"] / 2.0, plot["id"])
            for depth in (9.0, 12.0, 15.0, 18.0):
                for k in range(-6, 7):
                    along = k * 12.0
                    if abs(along) > r.hw + 6.0:
                        continue
                    pts.append((r.c[0] + r.f[0] * (r.hd + depth) + r.a[0] * along, r.c[1] + r.f[1] * (r.hd + depth) + r.a[1] * along))
            for s in (-1, 1):
                for depth in (12.0, 16.0):
                    for off in (-6.0, 0.0, 6.0):
                        pts.append((r.c[0] + r.a[0] * s * (r.hw + depth) + r.f[0] * off, r.c[1] + r.a[1] * s * (r.hw + depth) + r.f[1] * off))
        else:
            f = dirv(-3.0)
            for depth in (10.0, 13.0, 16.0):
                for along in (-15.0, 0.0, 15.0):
                    pts.append((h["at"][0] + f[0] * depth - f[1] * along, h["at"][1] + f[1] * depth + f[0] * along))
        for p in pts:
            if world.clearance(p) < YARD_R or world.terrace_room(p, h["terrace"]) < YARD_R + 3.0:
                continue
            near = sum(o["kids"] for o in homes.values() if o["terrace"] == h["terrace"]
                       and math.hypot(o["at"][0] - p[0], o["at"][1] - p[1]) < 110.0)
            d_home = math.hypot(h["at"][0] - p[0], h["at"][1] - p[1])
            cands.append((near - d_home * 0.01, p, h["terrace"], key, near))
    cands.sort(key=lambda c: -c[0])
    yards = []
    for score, p, ter, key, near in cands:
        if near < 2:
            continue
        if any(math.hypot(p[0] - y["c"][0], p[1] - y["c"][1]) < YARD_SPACING for y in yards):
            continue
        yards.append({"c": p, "terrace": ter, "home": key, "kids": near})
        if len(yards) >= MAX_YARDS:
            break
    return yards


# a yard's things (docs/yard_games.md "The yards"): (kind, angle from the house side, ring radius m, how many, extra)
YARD_KIT = (
    ("bench_park", 0.0, 12.0, 1, {}), ("bench_park", 22.0, 12.0, 1, {}), ("bin", -16.0, 12.0, 1, {}),
    ("sandpit", 40.0, 14.0, 1, {}), ("flowerbed", -40.0, 14.0, 1, {"r": 1.4}),
    ("tree", 125.0, 16.0, 1, {"species": "Maple"}), ("tree", -125.0, 16.0, 1, {"species": "SilverBirch"}),
    ("tree", 98.0, 17.5, 1, {"species": "Rowan"}), ("tree", -98.0, 17.5, 1, {"species": "Apple"}),
    ("bush", 150.0, 13.0, 1, {"species": "Privet"}), ("bush", -150.0, 13.0, 1, {"species": "Privet"}),
    ("bush", 170.0, 13.5, 1, {"species": "Spiraea"}), ("bush", 45.0, 15.5, 1, {"species": "Spiraea"}),
)
KIT_R = {"bench_park": 1.2, "bin": 0.4, "sandpit": 2.0, "flowerbed": 1.6, "tree": 1.0, "bush": 1.0}


def furnish(world, c, yaw, terrace, seed):
    """Benches by the house, a sandpit, a flower bed, trees at the ends and bushes on the drop's side: the yard's own
    things, which the game builds (ULyudYardGames) and the children hide behind."""
    out = []
    house = yaw + 180.0
    for n, (kind, ang, rad, _count, extra) in enumerate(YARD_KIT):
        placed = None
        for tries in range(12):
            a = house + ang + (tries % 4 - 1.5) * 7.0 * (1 if tries < 4 else 2)
            r = rad + (tries // 4) * 1.5
            d = dirv(a)
            p = (c[0] + d[0] * r, c[1] + d[1] * r)
            need = KIT_R[kind] + 0.4
            if world.clearance(p) < need or world.terrace_room(p, terrace) < need + 1.0:
                continue
            if any(math.hypot(p[0] - o["at"][0], p[1] - o["at"][1]) < KIT_R[kind] + KIT_R[o["kind"]] + 0.6 for o in out):
                continue
            placed = (p, a)
            break
        if not placed:
            continue
        p, a = placed
        face = a + 180.0 if kind == "bench_park" else (seed * 37 + n * 71) % 360
        o = {"kind": kind, "at": [rnd(p[0]), rnd(p[1])], "yaw": rnd(face % 360.0, 1)}
        o.update(extra)
        if kind == "tree":
            o["age"] = 18 + (seed * 7 + n * 5) % 14
            o["variant"] = (seed + n) % 3
        elif kind == "bush":
            o["age"] = 6 + (seed + n) % 4
            o["seed"] = 3200 + seed * 31 + n
        out.append(o)
    for o in out:
        world.circles.append(((o["at"][0], o["at"][1]), KIT_R[o["kind"]], o["kind"]))
        size = COVER_SIZE.get(o["kind"], {"bush": 1.1, "bin": 0.0}.get(o["kind"], 0.0))
        if size > 0.0:
            world.cover.append(((o["at"][0], o["at"][1]), size, o["kind"], "yard"))
    return out


def field_fits(world, c, yaw, half, terrace=None):
    """A football field (half length along yaw's across, half width) clear round c: samples its edge and inside."""
    f = dirv(yaw)
    a = (-f[1], f[0])
    for u in (-1.0, -0.5, 0.0, 0.5, 1.0):
        for v in (-1.0, 0.0, 1.0):
            p = (c[0] + a[0] * u * half[0] + f[0] * v * half[1], c[1] + a[1] * u * half[0] + f[1] * v * half[1])
            if world.clearance(p) < 0.8:
                return False
            if terrace and world.terrace_room(p, terrace) < 2.5:
                return False
    return True


def nearest_wall(world, c, yaw_hint):
    """Where the seeker counts: the nearest building face, retaining wall or tree within 25 m (a point 0.6 m off it,
    and the yaw facing it)."""
    best = None
    for r in world.rects:
        if r.what in ("planter", "allotment") or r.dist(c) > 25.0:
            continue
        u, v = r.local(c)
        cu, cv = max(-r.hd, min(r.hd, u)), max(-r.hw, min(r.hw, v))
        pt = (r.c[0] + r.f[0] * cu + r.a[0] * cv, r.c[1] + r.f[1] * cu + r.a[1] * cv)
        d = math.hypot(pt[0] - c[0], pt[1] - c[1])
        if best is None or d < best[0]:
            best = (d, pt)
    for p, size, kind, src in world.cover:
        if kind == "tree":
            d = math.hypot(p[0] - c[0], p[1] - c[1])
            if 4.0 < d < 25.0 and (best is None or d + 3.0 < best[0]):
                best = (d, p)
    if best is None:
        f = dirv(yaw_hint + 180.0)
        pt = (c[0] + f[0] * 9.0, c[1] + f[1] * 9.0)
    else:
        pt = best[1]
    dx, dy = pt[0] - c[0], pt[1] - c[1]
    n = math.hypot(dx, dy) or 1.0
    stand = (pt[0] - dx / n * 0.6, pt[1] - dy / n * 0.6)
    face = math.degrees(math.atan2(dy, dx))
    return stand, face


def make_area(world, aid, kind, c, z, radius, catch, yaw, games, terrace=None, places=()):
    """One play area's record: the games that fit it, its hiding places, the counting wall, chalk and field."""
    wall, face = nearest_wall(world, c, yaw)
    cover = world.cover_near(c, radius + 14.0, wall)
    ok = []
    for g in games:
        if g == "hide" and len(cover) < 4:
            continue
        if g == "football" and not field_fits(world, c, yaw, FIELD_HALF, terrace):
            continue
        ok.append(g)
    f = dirv(yaw)
    a = (-f[1], f[0])
    # the chalk's squares: a little to one side of the middle (the rope turns on the other), running along the terrace
    chalk = (c[0] + a[0] * -4.0 + f[0] * -2.0, c[1] + a[1] * -4.0 + f[1] * -2.0)
    rope = (c[0] + a[0] * 4.0, c[1] + a[1] * 4.0)
    area = {
        "id": aid, "kind": kind, "c": [rnd(c[0]), rnd(c[1]), rnd(z)], "r": radius, "catch": catch, "yaw": rnd(yaw, 1),
        "games": ok,
        "wall": [rnd(wall[0]), rnd(wall[1])], "wall_yaw": rnd(face, 1),
        "chalk": [rnd(chalk[0]), rnd(chalk[1])], "chalk_yaw": rnd(yaw + 90.0, 1),
        "rope": [rnd(rope[0]), rnd(rope[1])], "rope_yaw": rnd(yaw, 1),
        "field": {"half": list(FIELD_HALF), "yaw": rnd(yaw + 90.0, 1)},
        "cover": cover,
        "places": list(places),
    }
    return area


def build(plan, pub, folk, garden):
    world = World(plan, pub, garden)
    homes = kid_homes(plan, folk)
    yards = find_yards(world, homes)
    areas, spots = [], []
    plots = {p["id"]: p for p in plan["plots"]}
    for i, y in enumerate(yards, 1):
        sid = "S-YARD-%02d" % i
        z = world.terrace_z[y["terrace"]]
        home_plot = plots.get(y["home"].split("/")[0])
        yaw = home_plot["yaw"] if home_plot else -3.0
        kit = furnish(world, y["c"], yaw, y["terrace"], i)
        area = make_area(world, sid, "yard", y["c"], z, YARD_R, YARD_CATCH, yaw, GAMES, y["terrace"], places=[sid])
        area["home"] = y["home"]
        area["kit"] = kit
        area["kids_near"] = y["kids"]
        areas.append(area)
        # the people's place: a ring of standing points round the middle (three to a point), facing in
        slots = []
        for k in range(8):
            ang = k * 45.0
            d = dirv(ang)
            rr = 4.5 if k % 2 == 0 else 6.5
            slots.append({"pos": [rnd(y["c"][0] + d[0] * rr), rnd(y["c"][1] + d[1] * rr), rnd(z)], "yaw": rnd(ang + 180.0, 1),
                          "height": 0.0, "kind": "stand", "capacity": 3})
        # the benches by the house: the grown-ups' seats (a word on the bench, a grandmother minding the children)
        seats = []
        for b in kit:
            if b["kind"] != "bench_park":
                continue
            f = dirv(b["yaw"])
            for off in (-0.45, 0.45):
                seats.append({"pos": [rnd(b["at"][0] - f[1] * off), rnd(b["at"][1] + f[0] * off), rnd(z + 0.45)], "yaw": b["yaw"],
                              "height": 0.45, "kind": "bench"})
        if seats:
            spots.append({"id": sid + "-BENCH", "kind": "yard_bench", "pos": [rnd(seats[0]["pos"][0]), rnd(seats[0]["pos"][1]), rnd(z)],
                          "yaw": seats[0]["yaw"], "seats": len(seats), "slots": seats})
        spots.append({"id": sid, "kind": "yard", "pos": [rnd(y["c"][0]), rnd(y["c"][1]), rnd(z)], "yaw": rnd(yaw, 1),
                      "seats": 24, "slots": slots, "home": y["home"]})
    sp = {s["id"]: s for s in pub["spots"]}
    # the playground: everything round the sandpit and the slide to hide behind; tag and the rope on its soft floor
    pg = sp["S-PLAYGROUND"]
    areas.append(make_area(world, "S-PLAYGROUND", "playground", pg["pos"], pg["pos"][2], 9.0, 30.0, pg["yaw"],
                           ("rope", "hide", "tag"), places=["S-PLAYGROUND"]))
    # the park: the lawn east of the pond (its trees and benches are the cover)
    park = sp["S-PARK"]
    lawn = best_open(world, park["pos"], 70.0, 12.0)
    areas.append(make_area(world, "S-PARK-LAWN", "park", lawn, park["pos"][2], 12.0, 60.0, park["yaw"],
                           ("hide", "football", "tag"), places=["S-PARK", "S-POND"]))
    # the square: its north side, clear of the fountain and the audience
    sq = sp["S-SQUARE"]
    north = best_open(world, (sq["pos"][0], sq["pos"][1] + 26.0), 14.0, 9.0)
    areas.append(make_area(world, "S-SQUARE-NORTH", "square", north, sq["pos"][2], 9.0, 45.0, sq["yaw"],
                           ("hopscotch", "rope", "tag"), places=["S-SQUARE", "S-FOUNTAIN"]))
    # the beach: two stretches of sand between the loungers and the water, by the town's middle and by the fair
    for aid, bid in (("S-BEACH-PLAY1", "S-BEACH-13"), ("S-BEACH-PLAY2", "S-BEACH-19")):
        b = sp[bid]
        sea = best_open(world, (b["pos"][0] + 9.0, b["pos"][1]), 10.0, 7.0)
        areas.append(make_area(world, aid, "beach", sea, b["pos"][2], 9.0, 40.0, 0.0, ("football", "tag"),
                               places=[bid, "S-BEACH"]))
    return areas, spots, yards, homes, world


def best_open(world, near, search, need):
    """The most open point within search m of near (on a 2 m grid), preferring the nearer one."""
    best, bestv = (near[0], near[1]), -1e9
    steps = int(search / 2.0)
    for i in range(-steps, steps + 1):
        for j in range(-steps, steps + 1):
            p = (near[0] + i * 2.0, near[1] + j * 2.0)
            d = math.hypot(i * 2.0, j * 2.0)
            if d > search:
                continue
            cl = min(world.clearance(p), need + 3.0)
            v = cl - d * 0.05
            if v > bestv:
                best, bestv = p, v
    return best


def checks(areas, spots, strings):
    problems = []
    if len([a for a in areas if a["kind"] == "yard"]) < 4:
        problems.append("fewer than 4 yards found")
    groups = 0
    for a in areas:
        if not a["games"]:
            problems.append("%s: no game fits" % a["id"])
        if "hide" in a["games"] and len(a["cover"]) < 4:
            problems.append("%s: hide-and-seek with %d hiding places" % (a["id"], len(a["cover"])))
        groups += 1
    for g in GAMES:
        if not any(g in a["games"] for a in areas):
            problems.append("no area for %s" % g)
    need = ["yard.game.%s" % g for g in GAMES] + ["yard.toast.found_you", "yard.toast.you_found", "yard.prompt.join"]
    for k in need:
        if k not in strings:
            problems.append("missing string %s" % k)
    for k, v in strings.items():
        for lang in LANGS:
            if not v.get(lang):
                problems.append("string %s has no %s" % (k, lang))
    return problems


def outputs(areas, spots, strings):
    games = {
        "_doc": "Built by tools/yard/yard_areas.py (docs/yard_games.md); do not edit by hand. Unreal metres, yaw Unreal degrees "
                "(0 = +X = the sea). Each area: c centre (z the ground), r its clear radius, catch how far children come from, "
                "games that fit it, cover = hiding places (at: where a child crouches, behind: the thing), wall: where the seeker "
                "counts (wall_yaw facing it), chalk/rope: the hopscotch's first square and the rope's middle, field: football "
                "(half length, half width; yaw = along its length), places: the people's places whose children play here.",
        "version": 1,
        "areas": areas,
    }
    spots_out = {
        "_doc": "Built by tools/yard/yard_areas.py (docs/yard_games.md): the yards' people's places (kind yard), the same format as "
                "town_plan.json's spots. Do not edit by hand.",
        "spots": spots,
    }
    tps = {"about": "Written by tools/yard/yard_areas.py: x/y map metres (docs/world_map.md), yaw Unreal degrees; outdoors, dropped onto the ground.",
           "teleports": []}
    py = PLAN_DATA["portal_face_y"] / 100.0
    for a in areas:
        name = "dev.tp.yard.%s" % a["id"].lower()
        f = dirv(a["yaw"])
        tps["teleports"].append({"id": "yard.%s" % a["id"].lower(), "group": "yard", "name": name,
                                 "x": rnd(a["c"][0] + f[0] * (a["r"] + 4.0), 2), "y": rnd(a["c"][1] - py + f[1] * (a["r"] + 4.0), 2),
                                 "yaw": rnd(a["yaw"] + 180.0, 1), "time": "16:30"})
    files = {
        OUT_GAMES: json.dumps(games, ensure_ascii=False, indent=1) + "\n",
        OUT_SPOTS: json.dumps(spots_out, ensure_ascii=False, indent=1) + "\n",
        OUT_TELEPORTS: json.dumps(tps, ensure_ascii=False, indent=1) + "\n",
    }
    for lang in LANGS:
        table = {}
        for k, v in sorted(strings.items()):
            if k.startswith("line."):
                # a child's shout: a boy's and a girl's (ULyudYardGames::SayLine adds .m / .f)
                table[k + ".m"] = v[lang]
                table[k + ".f"] = v.get(lang + "_f", v[lang])
            else:
                table[k] = v[lang]
        for a in areas:
            key = "dev.tp.yard.%s" % a["id"].lower()
            base = strings.get("yard.area.%s" % a["kind"], {}).get(lang, a["id"])
            table[key] = base if a["kind"] != "yard" else base + " " + a["id"][-2:]
        files[OUT_STRINGS % lang] = json.dumps(dict(sorted(table.items())), ensure_ascii=False, indent=1) + "\n"
    return files


def main():
    check = "--check" in sys.argv
    plan, pub, folk, garden, strings = load()
    areas, spots, yards, homes, world = build(plan, pub, folk, garden)
    problems = checks(areas, spots, strings)
    files = outputs(areas, spots, strings)
    stale = []
    for path, text in files.items():
        old = open(path).read() if os.path.exists(path) else None
        if old != text:
            stale.append(os.path.relpath(path, ROOT))
            if not check:
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, "w") as f:
                    f.write(text)
    kids = sum(h["kids"] for h in homes.values())
    print("yard games: %d children at %d homes; %d areas (%d yards)" % (kids, len(homes), len(areas), len(yards)))
    for a in areas:
        print("  %-15s %-10s games %-34s cover %2d  %s" % (a["id"], a["kind"], ",".join(a["games"]), len(a["cover"]),
                                                          ("home %s, %d children near" % (a.get("home"), a.get("kids_near", 0))) if a["kind"] == "yard" else ""))
    for p in problems:
        print("PROBLEM:", p)
    if check and stale:
        for s in stale:
            print("STALE:", s)
    elif stale:
        print("wrote", ", ".join(stale))
    return 1 if problems or (check and stale) else 0


if __name__ == "__main__":
    sys.exit(main())
