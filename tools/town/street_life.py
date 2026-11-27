#!/usr/bin/env python3
"""The town's everyday street life through the warm half of the year (docs/street_life.md): the kvass barrel and the
ice-cream cart that come out on hot days, the morning street (the yard keeper sweeping, the shopkeepers putting up their
shutters, the milk round), yard life (laundry on the lines on sunny days, grandmothers on the benches, dominoes in the
evening) and the seasons' turns (autumn leaves raked into piles, the spring subbotnik).

Like tools/town/town_night.py this adds to the town generator's output and is idempotent (tools/town/town_life.py runs it
after itself; after tools/yard/yard_areas.py, tools/public_spaces.py or tools/town/counters.py changed, run it again):
    python3 tools/town/street_life.py            add or refresh
    python3 tools/town/street_life.py --check    exit 1 when the files are not as this script makes them

What it writes:
  Content/PeopleData/town_places.json   its places (PLACE_IDS), before the other add-ons' (the chain: street_life,
                                        lighthouse_keeper, south_people, town_pets, town_night, town_activities; each before
                                        the first block after it, so any order of running them leaves the same files)
  Content/PeopleData/activities.json    its rows (ACTIVITIES_ROWS), before the other add-ons' rows the same way
  Content/PeopleData/townsfolk.json     the street's people ("street_job"): the two vendors, the yard keeper, the milkwoman,
                                        a laundress in each yard, the grandmothers' and the domino players' likes, and the
                                        shopkeepers' "open_shutters" duty in front of their own shop before their shift
  Content/Localization/town/strings_<lang>.json   the new jobs
  Content/City/street_props.json        the things ULyudStreetLife shows when their condition holds: the barrel, the cart,
                                        the lines with washing, the domino tables, the leaf piles, the trunks' white bands
  tools/city/extra_places.json          the city things' places (the barrel's tap, the cart's lid, the subbotnik's bucket,
                                        the yard keeper's rake, a yard's domino table): its entries only (ids P-STREET-*)
  Content/Dev/teleports/street.json     developer mode's points (group "street")

The game switches the conditions (ULyudStreetLife from Private/People/StreetLifeModel.h: the weather and the date):
street_hot, street_sweep, laundry_hang, laundry_take, yard_grannies, yard_dominoes, leaf_raking, subbotnik. An activity
with "when" is only done while its condition is on (a duty too: the vendor stays at home on a cool day).
"""
import collections
import json
import math
import os
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import town_activities as TA  # noqa: E402

OD = collections.OrderedDict
ROOT = TA.ROOT
CONTENT = TA.CONTENT
PUBLIC = os.path.join(CONTENT, "City", "public_spaces.json")
YARDS = os.path.join(CONTENT, "PeopleData", "yard_games.json")
YARD_SPOTS = os.path.join(CONTENT, "PeopleData", "yard_spots.json")
PROPS = os.path.join(CONTENT, "City", "street_props.json")
EXTRA = os.path.join(ROOT, "tools", "city", "extra_places.json")
TELEPORTS = os.path.join(CONTENT, "Dev", "teleports", "street.json")

ALL_DAYS = [0, 1, 2, 3, 4, 5, 6]
WEEKDAYS6 = [0, 1, 2, 3, 4, 5]

# --- activities (one a line; "when": the game's condition, ULyudStreetLife) -----------------------------------------
ACTIVITIES_ROWS = [
    '{"id": "vend_kvass", "places": ["street_stall"], "slots": ["staff"], "pose": "stand", "minutes": [60, 60], "meets": {"social": 0.4, "hunger": 0.15}, "free": 0, "staff": 1, "when": "street_hot", "rain": "ok"}',
    '{"id": "vend_icecream", "places": ["street_stall"], "slots": ["staff"], "pose": "stand", "minutes": [60, 60], "meets": {"social": 0.4, "hunger": 0.15}, "free": 0, "staff": 1, "when": "street_hot", "rain": "ok"}',
    '{"id": "buy_kvass", "places": ["P-STREET-KVASS"], "slots": ["visit"], "pose": "stand", "minutes": [3, 6], "hours": [11.5, 18.7], "ages": [6, 200], "temp": [20, 45], "when": "street_hot", "meets": {"hunger": 0.5, "fun": 0.4, "social": 0.3}, "base": -0.05, "trait": "sun"}',
    '{"id": "buy_icecream", "places": ["P-STREET-ICECART"], "slots": ["visit"], "pose": "stand", "minutes": [3, 6], "hours": [11.5, 18.7], "ages": [4, 200], "temp": [20, 45], "when": "street_hot", "meets": {"hunger": 0.4, "fun": 0.6, "social": 0.2}, "base": -0.05, "trait": "sun"}',
    '{"id": "sweep_street", "places": ["sweep_round"], "slots": ["stand"], "pose": "stand", "wander": 1, "round": 1, "wander_pause": [45, 110], "minutes": [60, 60], "meets": {"nature": 0.4, "mind": 0.1}, "costs": {"tired": 0.06, "hunger": 0.05}, "free": 0, "when": "street_sweep", "rain": "ok", "on_time": 1}',
    '{"id": "deliver_milk", "places": ["milk_round"], "slots": ["stand"], "pose": "stand", "wander": 1, "round": 1, "wander_pause": [25, 60], "minutes": [60, 60], "meets": {"social": 0.3, "nature": 0.3}, "costs": {"tired": 0.04, "hunger": 0.04}, "free": 0, "rain": "ok", "on_time": 1}',
    '{"id": "open_shutters", "places": ["shop_front"], "slots": ["stand"], "pose": "stand", "minutes": [15, 15], "meets": {"mind": 0.1}, "free": 0, "rain": "ok", "on_time": 1}',
    '{"id": "hang_laundry", "places": ["laundry_line"], "slots": ["stand"], "pose": "stand", "minutes": [25, 30], "meets": {"sun": 0.4, "nature": 0.3}, "costs": {"tired": 0.03}, "free": 0, "when": "laundry_hang", "on_time": 1}',
    '{"id": "take_laundry", "places": ["laundry_line"], "slots": ["stand"], "pose": "stand", "minutes": [20, 25], "meets": {"sun": 0.2}, "costs": {"tired": 0.03}, "free": 0, "when": "laundry_take", "rain": "ok", "on_time": 1}',
    '{"id": "granny_bench", "places": ["yard_bench"], "slots": ["bench"], "pose": "sit", "minutes": [40, 100], "hours": [9.5, 20], "ages": [58, 200], "temp": [9, 40], "when": "yard_grannies", "meets": {"social": 1.0, "nature": 0.3, "tired": 0.2}, "base": -0.45, "trait": "social"}',
    '{"id": "yard_dominoes", "places": ["domino_table"], "slots": ["domino_seat"], "pose": "sit", "minutes": [45, 110], "hours": [17.5, 21.3], "ages": [35, 200], "temp": [13, 40], "when": "yard_dominoes", "meets": {"social": 0.9, "fun": 0.8}, "base": -0.5, "trait": "social"}',
    '{"id": "rake_leaves", "places": ["leaf_round"], "slots": ["stand"], "pose": "stand", "wander": 1, "round": 1, "wander_pause": [80, 160], "minutes": [60, 60], "meets": {"nature": 0.6}, "costs": {"tired": 0.07, "hunger": 0.06}, "free": 0, "when": "leaf_raking", "on_time": 1}',
    '{"id": "subbotnik_paint", "places": ["subbotnik"], "slots": ["paint_spot"], "pose": "stand", "minutes": [40, 80], "hours": [9, 12.5], "ages": [14, 75], "when": "subbotnik", "meets": {"social": 0.8, "fun": 0.4, "nature": 0.3}, "costs": {"tired": 0.05}, "base": 0.35, "trait": "social"}',
    '{"id": "subbotnik_whitewash", "places": ["subbotnik"], "slots": ["tree_spot"], "pose": "stand", "minutes": [40, 80], "hours": [9, 12.5], "ages": [14, 75], "when": "subbotnik", "meets": {"social": 0.8, "nature": 0.5}, "costs": {"tired": 0.05}, "base": 0.35, "trait": "nature"}',
    '{"id": "subbotnik_sweep", "places": ["subbotnik"], "slots": ["sweep_spot"], "pose": "stand", "minutes": [30, 70], "hours": [9, 12.5], "ages": [10, 75], "when": "subbotnik", "meets": {"social": 0.7, "nature": 0.4}, "costs": {"tired": 0.05}, "base": 0.3, "trait": "active"}',
]
ACT_IDS = [json.loads(r)["id"] for r in ACTIVITIES_ROWS]
# the likes this script gives (a share of those who would anyway)
OWN_LIKES = ("granny_bench", "yard_dominoes", "buy_kvass", "buy_icecream")

ROLE = {
    "kvass": {"ru": ("продавец кваса", "продавщица кваса"), "en": ("kvass seller", "kvass seller"), "ja": ("クワス売り", "クワス売り")},
    "icecream": {"ru": ("мороженщик", "мороженщица"), "en": ("ice-cream man", "ice-cream lady"), "ja": ("アイス売り", "アイス売り")},
    "keeper": {"ru": ("дворник", "дворник"), "en": ("yard keeper", "yard keeper"), "ja": ("用務員", "用務員")},
    "milk": {"ru": ("молочник", "молочница"), "en": ("milkman", "milkwoman"), "ja": ("牛乳配達", "牛乳配達")},
}

# --- where -------------------------------------------------------------------------------------------------------
# the kvass barrel on Sun Square's east side by the promenade's way down, the ice-cream cart on the promenade over the beach
KVASS_NEAR = (40.0, 897.0)
ICECART_NEAR = (124.0, 990.0)
STALL_CLEAR = 2.6        # m from any prop of the public spaces and from other people's slots
QUEUE_N, SPACING = 4, 0.85
# the yard keeper's morning sweep: the promenade's kerb from south to north and back along the square
SWEEP_FROM, SWEEP_TO = 600.0, 1200.0
# the milk round: the first doors of the post round (the houses on the terraces), before the postwoman's


def load(p):
    return TA.load(p)


def dump(d):
    return TA.dump(d)


def r2(v):
    return [round(v[0], 2), round(v[1], 2), round(v[2], 2)]


def rot(yaw, x, y):
    a = math.radians(yaw)
    return x * math.cos(a) - y * math.sin(a), x * math.sin(a) + y * math.cos(a)


def at_local(origin, yaw, x, y, dz=0.0):
    dx, dy = rot(yaw, x, y)
    return r2([origin[0] + dx, origin[1] + dy, origin[2] + dz])


def clear_point(near, z, props, avoid, yaw_to):
    """The nearest point to `near` (a spiral of half-metre steps) clear of every prop and slot; the stall faces yaw_to."""
    best = None
    for ring in range(0, 30):
        r = ring * 0.5
        steps = max(1, int(2 * math.pi * r / 0.5))
        for k in range(steps):
            a = 2 * math.pi * k / steps
            x, y = near[0] + r * math.cos(a), near[1] + r * math.sin(a)
            # the stall and its line: the barrel's length (2.6 m) behind and the queue (4 m) in front
            ok = True
            for dx in (-1.8, -0.9, 0.0, 1.2, 2.4, 3.6, 4.6):
                px, py = x + dx * math.cos(math.radians(yaw_to)), y + dx * math.sin(math.radians(yaw_to))
                if min(math.hypot(px - q[0], py - q[1]) for q in props) < (STALL_CLEAR if dx <= 0.0 else 1.4):
                    ok = False
                    break
                if avoid and min(math.hypot(px - q[0], py - q[1]) for q in avoid) < 1.6:
                    ok = False
                    break
            if ok:
                best = (round(x, 2), round(y, 2), z)
                break
        if best:
            return best
    sys.exit("street_life: no clear ground near %s" % (near,))


def stall(pid, kind, origin, yaw, role):
    """A stall: the vendor at its front end (local +X 1.45 m from the middle), the counter point 0.45 m further (the
    mugs' table / the cart's lid), the line going out from there, away from it, 0.85 m apart."""
    slots = [OD([("kind", "staff"), ("tag", role), ("pos", at_local(origin, yaw, 1.45, 0.0)), ("yaw", round(yaw % 360, 1))])]
    for q in range(QUEUE_N):
        slots.append(OD([("kind", "visit"), ("pos", at_local(origin, yaw, 2.35 + q * SPACING, 0.0)), ("yaw", round((yaw + 180) % 360, 1)),
                         ("queue", pid), ("q", q)]))
    return OD([("id", pid), ("kind", kind), ("pos", r2(origin)), ("yaw", round(yaw, 1)), ("island", 0), ("roles", [role]), ("slots", slots)])


def walked_round(pid, kind, pts, yaws=None):
    slots = []
    for i, p in enumerate(pts):
        slots.append(OD([("kind", "stand"), ("pos", r2(p)), ("yaw", round((yaws[i] if yaws else 0.0) % 360, 1))]))
    return OD([("id", pid), ("kind", kind), ("pos", r2(pts[0])), ("yaw", 0.0), ("island", 0), ("slots", slots)])


def yard_frame(a):
    """A yard's middle and its turn (the yard's own yaw), and a point in its frame (x along the yaw, y to its left)."""
    c, yaw = a["c"], a.get("yaw", 0.0)
    return lambda x, y: at_local(c, yaw, x, y)


def yard_trees(a):
    return [k for k in a.get("kit", []) if k["kind"] == "tree"]


def yard_benches(a):
    return [k for k in a.get("kit", []) if k["kind"] == "bench_park"]


def yard_layout(a):
    """Where a yard's things go (its clear disc of 10 m: the games use the middle): the washing line across the far side
    from the benches (two posts 5 m apart, 6.5 m out), the domino table near the benches' end, toward the middle."""
    f = yard_frame(a)
    benches = yard_benches(a)
    # the side the benches are on: their mean in the yard's frame
    c, yaw = a["c"], a.get("yaw", 0.0)
    if benches:
        bx = sum(b["at"][0] for b in benches) / len(benches) - c[0]
        by = sum(b["at"][1] for b in benches) / len(benches) - c[1]
        side = math.degrees(math.atan2(by, bx))
    else:
        side = yaw + 180.0
    # the line opposite the benches, across: the posts at +-2.5 m along it
    lx, ly = math.cos(math.radians(side + 180)), math.sin(math.radians(side + 180))
    mid = [c[0] + lx * 6.5, c[1] + ly * 6.5, c[2]]
    line_yaw = (side + 90.0) % 360
    # the table: toward the benches, 5 m out and 3 m to the side
    tx, ty = math.cos(math.radians(side)), math.sin(math.radians(side))
    table = [c[0] + tx * 5.0 - ty * 3.0, c[1] + ty * 5.0 + tx * 3.0, c[2]]
    return r2(mid), round(line_yaw, 1), r2(table), round(side, 1)


def yard_places(a):
    """S-YARD-xx-LAUNDRY (two stand slots at the line), S-YARD-xx-DOMINO (four seats round the table), S-YARD-xx-WORK (the
    subbotnik's spots: in front of each bench to paint it, by each tree to whitewash it, three to sweep)."""
    mid, line_yaw, table, side = yard_layout(a)
    yid = a["id"]
    laundry = OD([("id", yid + "-LAUNDRY"), ("kind", "laundry_line"), ("pos", mid), ("yaw", line_yaw), ("island", 0), ("slots", [
        OD([("kind", "stand"), ("pos", at_local(mid, line_yaw, d, -0.55)), ("yaw", round((line_yaw + 90) % 360, 1))]) for d in (-1.2, 1.2)])])
    seats = []
    for k in range(4):
        ang = side + 45 + 90 * k
        p = [table[0] + 0.75 * math.cos(math.radians(ang)), table[1] + 0.75 * math.sin(math.radians(ang)), table[2] + 0.45]
        seats.append(OD([("kind", "domino_seat"), ("pos", r2(p)), ("yaw", round((ang + 180) % 360, 1)), ("height", 0.45)]))
    domino = OD([("id", yid + "-DOMINO"), ("kind", "domino_table"), ("pos", table), ("yaw", round(side, 1)), ("island", 0), ("slots", seats)])
    work = []
    for b in yard_benches(a):
        y = b.get("yaw", 0.0)
        dx, dy = rot(y, 0.9, 0.0)       # in front of the bench (a park bench's front is +X)
        work.append(OD([("kind", "paint_spot"), ("pos", r2([b["at"][0] + dx, b["at"][1] + dy, a["c"][2]])), ("yaw", round((y + 180) % 360, 1))]))
    for t in yard_trees(a):
        to = math.degrees(math.atan2(t["at"][1] - a["c"][1], t["at"][0] - a["c"][0]))
        p = [t["at"][0] - 0.6 * math.cos(math.radians(to)), t["at"][1] - 0.6 * math.sin(math.radians(to)), a["c"][2]]
        work.append(OD([("kind", "tree_spot"), ("pos", r2(p)), ("yaw", round(to % 360, 1))]))
    f = yard_frame(a)
    for x, y in ((-3.0, 2.0), (2.5, -3.0), (3.0, 3.5)):
        work.append(OD([("kind", "sweep_spot"), ("pos", f(x, y)), ("yaw", round((a.get("yaw", 0.0) + 45 * (x > 0)) % 360, 1))]))
    sub = OD([("id", yid + "-WORK"), ("kind", "subbotnik"), ("pos", r2(a["c"])), ("yaw", round(a.get("yaw", 0.0), 1)), ("island", 0), ("slots", work)])
    return [laundry, domino, sub]


def leaf_round(yards):
    pts, yaws = [], []
    for a in yards:
        f = yard_frame(a)
        for x, y in ((-2.0, 4.5), (3.5, -2.5)):
            pts.append(f(x, y))
            yaws.append(a.get("yaw", 0.0) + (90 if x < 0 else 270))
    return walked_round("S-LEAF-ROUND", "leaf_round", pts, yaws)


def sweep_round(places):
    """Every second promenade kerb spot from the south end of the stretch to the north, then back along Sun Square."""
    kerbs = sorted([p for p in places if p["id"].startswith("S-KERB-") and SWEEP_FROM <= p["pos"][1] <= SWEEP_TO], key=lambda p: p["pos"][1])
    pts = [[p["pos"][0] - 1.6, p["pos"][1], p["pos"][2]] for p in kerbs[::2]]
    sq = next(p for p in places if p["id"] == "S-SQUARE")
    for dy in (12.0, 0.0, -12.0):
        pts.append([sq["pos"][0] + 14.0, sq["pos"][1] + dy, sq["pos"][2]])
    yaws = [90.0] * (len(pts) - 3) + [270.0] * 3
    return walked_round("S-SWEEP-ROUND", "sweep_round", pts, yaws)


def milk_round(places):
    post = next(p for p in places if p["id"] == "S-POST-ROUND")
    pts = [s["pos"] for s in post["slots"][:14]]
    return walked_round("S-MILK-ROUND", "milk_round", pts, [s["yaw"] for s in post["slots"][:14]])


def shop_fronts(places):
    """One stand spot outside the street door of every promenade shop and kiosk with a counter (its staff slot outdoors,
    a unit's door), 1.3 m out from the door: the shopkeeper's shutters."""
    out = []
    for p in places:
        if p["kind"] not in ("shop", "kiosk") or p.get("shell") or not p.get("door"):
            continue
        staff = [s for s in p["slots"] if s["kind"] == "staff" and s.get("pos")]
        if not staff:
            continue
        d = p["door"]
        s = staff[0]["pos"]
        vx, vy = d[0] - s[0], d[1] - s[1]
        n = math.hypot(vx, vy) or 1.0
        pos = [d[0] + vx / n * 1.3, d[1] + vy / n * 1.3, d[2]]
        out.append(OD([("id", "S-FRONT-" + p["id"]), ("kind", "shop_front"), ("pos", r2(pos)), ("yaw", round(math.degrees(math.atan2(-vy, -vx)) % 360, 1)),
                       ("island", 0), ("of", p["id"]), ("slots", [OD([("kind", "stand"), ("pos", r2(pos)), ("yaw", round(math.degrees(math.atan2(-vy, -vx)) % 360, 1))])])]))
    return out


# --- people ------------------------------------------------------------------------------------------------------

FREE_JOBS = ("unemployed", "homemaker", "pensioner")


def free(p, taken, ages, female=None, wake=None):
    """Somebody with no work and no duty of any add-on, up in time for the job (wake: the latest hour they may get up;
    their night is never moved, so the other add-ons' choices that go by people's hours stay as they were)."""
    return ((wake is None or (p.get("wake", 7.5) <= wake and 19.0 <= p.get("bed", 23.0) <= 23.9)) and p.get("occupation") in FREE_JOBS and not p.get("duties") and not p.get("night_job") and p.get("stay") is None
            and ages[0] <= p["age"] <= ages[1] and p["key"] not in taken and (female is None or bool(p.get("female")) == female))


def pick(people, taken, ages, female, job, prev=None, near=None, homes=None, wake=None):
    if prev is not None:
        return prev
    fit = [p for p in people if free(p, taken, ages, female, wake)]
    if not fit:
        sys.exit("street_life: nobody free for the %s job" % job)
    if near is not None and homes is not None:
        def dist(p):
            h = homes.get(p["home"])
            return (round(math.hypot(h[0] - near[0], h[1] - near[1]), 1) if h else 9e9, zlib.crc32(p["key"].encode()))
        return sorted(fit, key=dist)[0]
    return sorted(fit, key=lambda p: zlib.crc32((job + p["key"]).encode()))[0]


def duty(act, place, hours, days, role=None):
    d = OD([("activity", act), ("place", place)])
    if role:
        d["role"] = role
    d["hours"] = list(hours)
    d["days"] = list(days)
    return d


def strip_own(p):
    p["duties"] = [d for d in p.get("duties", []) if d.get("activity") not in ACT_IDS]
    for k in OWN_LIKES:
        p.get("likes", {}).pop(k, None)


# --- the props the game shows (Content/City/street_props.json) ----------------------------------------------------

def props_doc(kvass, icecart, yards, piles):
    items = []

    def add(kind, at, yaw, when, **extra):
        it = OD([("kind", kind), ("at", r2(at)), ("yaw", round(yaw % 360, 1)), ("when", when)])
        it.update(extra)
        items.append(it)
    add("kvass_barrel", kvass[0], kvass[1], "street_hot")
    add("icecream_cart", icecart[0], icecart[1], "street_hot")
    for a in yards:
        mid, line_yaw, table, side = yard_layout(a)
        add("laundry_line_posts", mid, line_yaw, "")
        add("laundry_washing", mid, line_yaw, "laundry_out", variant=zlib.crc32(a["id"].encode()) % 3)
        add("domino_table", table, side, "")
        for t in yard_trees(a):
            add("trunk_white", [t["at"][0], t["at"][1], a["c"][2]], 0.0, "trunks_white")
    for p in piles:
        add("leaf_pile", p, zlib.crc32(repr(p).encode()) % 360, "leaf_piles")
    return OD([("_doc", "Written by tools/town/street_life.py (docs/street_life.md): do not edit by hand. The street's things "
                        "ULyudStreetLife builds (CityKit meshes, one instanced set per kind and condition) and shows while "
                        "their condition holds (\"\": always). at: Unreal metres (z the ground), yaw: Unreal degrees."),
               ("version", 1), ("items", items)])


# --- the city things' places (tools/city/extra_places.json) -----------------------------------------------------

def stall_extra(pid, building, origin, yaw, thing, kind, role, name_ru, name_en, props):
    """An extra place whose export is inline: one thing at the stall's counter point, its staff the vendor."""
    front = at_local(origin, yaw, 1.9, 0.0)
    # the place's front faces the customers (local +X of the stall)
    fp = [[-1.6, -1.6], [1.6, -1.6], [1.6, 1.6], [-1.6, 1.6]]
    return OD([("id", pid), ("kind", "stall"), ("building", building), ("front", front), ("yaw", round(yaw % 360, 1)), ("w", 3.2), ("d", 3.2),
               ("name_ru", name_ru), ("name_en", name_en), ("staff", [[role, 1, [10, 19], "mon-sun"]]),
               ("export", OD([("footprint", fp), ("entrances", [OD([("id", "front"), ("pos", [0.0, -1.2, 0.0]), ("kind", "street")])]),
                              ("things", [OD([("id", thing), ("kind", kind), ("pos", [0.0, 0.0, 0.95]), ("yaw", 0.0), ("props", props)])])]))])


def spot_extra(pid, building, at, yaw, thing, kind, name_ru, name_en, props, z=0.9):
    fp = [[-1.5, -1.5], [1.5, -1.5], [1.5, 1.5], [-1.5, 1.5]]
    return OD([("id", pid), ("kind", "yard"), ("building", building), ("front", r2(at)), ("yaw", round(yaw % 360, 1)), ("w", 3.0), ("d", 3.0),
               ("name_ru", name_ru), ("name_en", name_en),
               ("export", OD([("footprint", fp), ("entrances", [OD([("id", "front"), ("pos", [0.0, -1.2, 0.0]), ("kind", "street")])]),
                              ("things", [OD([("id", thing), ("kind", kind), ("pos", [0.0, 0.0, z]), ("yaw", 0.0), ("props", props)])])]))])


def insert_before_chain(lines_or_places, own_pred, later_pred, new):
    """Our block, before the first block of the add-ons after us (or at the end): the chain's rule."""
    rest = [x for x in lines_or_places if not own_pred(x)]
    at = next((i for i, x in enumerate(rest) if later_pred(x)), None)
    if at is None:
        return None, rest
    return rest[:at] + new + rest[at:], rest


def later_ids():
    import lighthouse_keeper as LH
    import south_people as SP
    import town_pets as PETS
    import town_night as TN
    places = set(LH.PLACE_IDS) | set(SP.PLACE_IDS) | {PETS.FEED_PLACE} | set(TN.PLACE_IDS) | set(TA.PLACE_IDS)
    acts = list(LH.ACT_IDS) + list(SP.ACT_IDS) + list(PETS.ACT_IDS) + list(TN.ACT_IDS) + list(TA.ACT_IDS)
    return places, acts


def build():
    out = {}
    plan_places = load(TA.PLACES)
    places = plan_places["places"]
    yards_doc = load(YARDS)
    yards = [a for a in yards_doc["areas"] if a["kind"] == "yard"]
    pub = load(PUBLIC)
    props_xy = [p["at"] for p in pub["props"] if p["kind"] not in ("railing",)]
    for f in pub.get("fountains", []):
        c = f.get("at") or f.get("c") or f.get("pos")
        if c:
            props_xy += [[c[0] + 7 * math.cos(a), c[1] + 7 * math.sin(a)] for a in [k * math.pi / 6 for k in range(12)]]
    own_ids = set()

    # the stalls: the barrel faces west onto the square, the cart east toward the sea and the beach
    others = [s["pos"] for p in places if not p["id"].startswith(("P-STREET-",)) for s in p.get("slots", []) if s.get("pos")]
    sq = next(p for p in places if p["id"] == "S-SQUARE")
    kv = clear_point(KVASS_NEAR, sq["pos"][2], props_xy, others, 180.0)
    prom = next(p for p in places if p["id"] == "S-KERB-30")
    ic = clear_point(ICECART_NEAR, prom["pos"][2], props_xy, others, 0.0)
    kv_place = stall("P-STREET-KVASS", "street_stall", kv, 180.0, "vendor")
    ic_place = stall("P-STREET-ICECART", "street_stall", ic, 0.0, "vendor")
    sweep = sweep_round(places)
    milk = milk_round(places)
    fronts = shop_fronts(places)
    ypl = []
    for a in yards:
        ypl += yard_places(a)
    leaves = leaf_round(yards)
    new = [kv_place, ic_place, sweep, milk] + fronts + ypl + [leaves]
    PLACE_IDS = [p["id"] for p in new]
    own_ids = set(PLACE_IDS)
    later_p, later_a = later_ids()
    merged, rest = insert_before_chain(places, lambda p: p["id"] in own_ids or p["id"].startswith(("S-FRONT-", "P-STREET-")),
                                       lambda p: p["id"] in later_p, new)
    plan_places["places"] = merged if merged is not None else rest + new
    out[TA.PLACES] = dump(plan_places)

    # activities.json: our rows before the chain's
    text = open(TA.ACTIVITIES, encoding="utf-8").read()
    lines = text.split("\n")
    rows = ["    " + r + "," for r in ACTIVITIES_ROWS]
    merged, rest = insert_before_chain(lines, lambda l: any('"id": "%s"' % a in l for a in ACT_IDS),
                                       lambda l: any('"id": "%s"' % a in l for a in later_a), rows)
    if merged is None:
        sys.exit("street_life: run the other add-ons first (their rows are the anchor)")
    out[TA.ACTIVITIES] = "\n".join(merged)
    json.loads(out[TA.ACTIVITIES])

    # the people
    fd = load(TA.FOLK)
    people = fd["people"]
    by_key = {p["key"]: p for p in people}
    had = {}
    for p in people:
        if p.get("street_job"):
            had.setdefault(p["street_job"], p)
    for p in people:
        strip_own(p)
        if "open_shutters" in [d.get("activity") for d in p.get("duties", [])]:
            pass
    homes = {}
    for p in places:
        if p["kind"] == "flat" and p.get("door"):
            homes[p["id"]] = p["door"]
    taken = set()
    jobs = {}
    jobs["kvass"] = pick(people, taken, (42, 68), True, "kvass", had.get("kvass"), wake=9.3)
    taken.add(jobs["kvass"]["key"])
    jobs["icecream"] = pick(people, taken, (24, 64), None, "icecream", had.get("icecream"), wake=9.3)
    taken.add(jobs["icecream"]["key"])
    jobs["keeper"] = pick(people, taken, (38, 74), False, "keeper", had.get("keeper"), wake=5.1)
    taken.add(jobs["keeper"]["key"])
    jobs["milk"] = pick(people, taken, (30, 74), True, "milk", had.get("milk"), wake=5.5)
    taken.add(jobs["milk"]["key"])
    laundress = {}
    for a in yards:
        tag = "laundry:" + a["id"]
        q = pick(people, taken, (26, 82), True, tag, had.get(tag), near=a["c"], homes=homes, wake=8.6)
        taken.add(q["key"])
        laundress[a["id"]] = q
    # whoever had a street job that went: back to what they were
    for tag, p in had.items():
        if p["key"] not in taken:
            p.pop("street_job", None)
    # the jobs
    for tag in ("kvass", "icecream", "keeper", "milk"):
        p = jobs[tag]
        p["street_job"] = tag
        if p.get("occupation") == "unemployed" or tag in ("kvass", "icecream", "keeper", "milk"):
            p.setdefault("street_was", p.get("occupation", "unemployed"))
            p["occupation"] = {"kvass": "vendor", "icecream": "vendor", "keeper": "janitor", "milk": "milk_round"}[tag]
    jobs["kvass"]["duties"] = [duty("vend_kvass", "P-STREET-KVASS", [10.0, 19.0], ALL_DAYS, "vendor")] + jobs["kvass"].get("duties", [])
    jobs["icecream"]["duties"] = [duty("vend_icecream", "P-STREET-ICECART", [10.0, 19.0], ALL_DAYS, "vendor")] + jobs["icecream"].get("duties", [])
    jobs["keeper"]["duties"] = [duty("sweep_street", "S-SWEEP-ROUND", [5.5, 7.4], ALL_DAYS),
                                duty("rake_leaves", "S-LEAF-ROUND", [8.0, 11.0], WEEKDAYS6)] + jobs["keeper"].get("duties", [])
    jobs["milk"]["duties"] = [duty("deliver_milk", "S-MILK-ROUND", [6.0, 7.5], WEEKDAYS6)] + jobs["milk"].get("duties", [])
    # (nobody's hours of sleep are changed: the keeper and the milkwoman are picked from the early risers, see pick)
    for yid, p in laundress.items():
        p["street_job"] = "laundry:" + yid
        p["duties"] = [duty("hang_laundry", yid + "-LAUNDRY", [9.0, 9.6], ALL_DAYS), duty("take_laundry", yid + "-LAUNDRY", [9.6, 19.5], ALL_DAYS)] + p.get("duties", [])
    # the shopkeepers: on every day, whoever is first on shift at a front puts the shutters up a quarter of an hour before
    # it (those up in time: nobody's night is cut short for it)
    fronts_of = {f["of"]: f for f in fronts}
    for pid, f in sorted(fronts_of.items()):
        days_of = OD()
        for wd in ALL_DAYS:
            first = None
            for p in people:
                for d in p.get("duties", []):
                    if d.get("place") == pid and d.get("activity", "").startswith("work") and wd in d.get("days", ALL_DAYS):
                        k = (d["hours"][0], zlib.crc32(p["key"].encode()))
                        if first is None or k < first[0]:
                            first = (k, p, d)
            if (first and first[0][0] - 0.75 >= first[1].get("wake", 7.0) and first[0][0] >= 6.0
                    and not any(d.get("activity") in later_a for d in first[1].get("duties", []))):
                days_of.setdefault((first[1]["key"], first[0][0]), []).append(wd)
        for (key, h0), days in days_of.items():
            p = by_key[key]
            p["duties"] = [duty("open_shutters", f["id"], [round(h0 - 0.25, 2), h0], days)] + p["duties"]
    later_likes = set(later_a) | set(TA.OWN_LIKES)
    # likes: grandmothers to their benches (most women of 62 and over, a few of the men), domino players (men of 45 and
    # over, a third), and a fondness for the barrel and the cart (those who like the sun)
    for p in people:
        if p.get("stay") is not None or not p.get("town", 1):
            continue
        h = zlib.crc32(("street" + p["key"]).encode()) % 100
        likes = p.setdefault("likes", OD())
        if p["age"] >= 62 and (p.get("female") and h < 70 or not p.get("female") and h < 20):
            likes["granny_bench"] = round(0.9 + (h % 7) * 0.05, 2)
        if p["age"] >= 45 and not p.get("female") and h % 3 == 0:
            likes["yard_dominoes"] = round(0.8 + (h % 5) * 0.06, 2)
        if p["temper"].get("sun", 0.5) > 0.6 and h % 4 == 0:
            likes["buy_kvass" if h % 8 == 0 else "buy_icecream"] = 0.4
        if not likes:
            p.pop("likes", None)
        else:
            # ours before the later add-ons' likes (they put theirs back at the end when they run again)
            later = [k for k in likes if k in later_likes]
            p["likes"] = OD([(k, v) for k, v in likes.items() if k not in later] + [(k, likes[k]) for k in later])
    out[TA.FOLK] = dump(fd)
    for lang in ("ru", "en", "ja"):
        sd = load(TA.STRINGS % lang)
        for tag in ("kvass", "icecream", "keeper", "milk"):
            p = jobs[tag]
            sd["npc.%s.role" % p["key"]] = ROLE[tag][lang][1 if p.get("female") else 0]
        out[TA.STRINGS % lang] = dump(sd)

    # the leaf piles: two in each yard where the keeper rakes (the round's spots, a step on)
    piles = []
    for s in leaves["slots"]:
        dx, dy = rot(s["yaw"], 1.4, 0.0)
        piles.append([s["pos"][0] + dx, s["pos"][1] + dy, s["pos"][2]])
    out[PROPS] = dump(props_doc((kv, 180.0), (ic, 0.0), yards, piles))

    # the city things
    ex = load(EXTRA)
    keep = [e for e in ex["places"] if not e["id"].startswith("P-STREET-")]
    y1 = yards[0]
    mid, line_yaw, table, side = yard_layout(y1)
    work = next(p for p in ypl if p["id"] == y1["id"] + "-WORK")
    bucket = next(s for s in work["slots"] if s["kind"] == "tree_spot")
    rake_at = piles[0]
    ex["places"] = keep + [
        stall_extra("P-STREET-KVASS", "street_kvass", kv, 180.0, "kvass_tap", "counter", "vendor", "Бочка с квасом", "The kvass barrel",
                    OD([("shop", "street_kvass"), ("role", "vendor")])),
        stall_extra("P-STREET-ICECART", "street_icecart", ic, 0.0, "cart_lid", "counter", "vendor", "Тележка с мороженым", "The ice-cream cart",
                    OD([("shop", "street_icecream"), ("role", "vendor")])),
        spot_extra("P-STREET-SUBBOTNIK", "street_subbotnik", bucket["pos"], bucket["yaw"], "whitewash_bucket", "take",
                   "Субботник во дворе", "The subbotnik in the yard", OD([("what", "whitewash")]), z=0.3),
        spot_extra("P-STREET-LEAVES", "street_leaves", rake_at, 0.0, "rake", "take", "Листья во дворе", "Leaves in the yard",
                   OD([("what", "rake")]), z=0.6),
        spot_extra("P-STREET-DOMINO", "street_domino", table, side, "domino_table", "seat", "Доминошный стол", "The domino table",
                   OD([("what", "dominoes"), ("height", 0.45)]), z=0.45),
    ]
    out[EXTRA] = json.dumps(ex, ensure_ascii=False, indent=1) + "\n"

    # developer mode's points (map metres = Unreal metres here: x, y; outdoors, dropped onto the ground)
    def tp(i, at, yaw, time=None):
        t = OD([("id", "street." + i), ("group", "street"), ("name", "dev.tp.street." + i), ("x", round(at[0], 1)), ("y", round(at[1], 1)),
                ("yaw", round(yaw % 360, 1))])
        if time:
            t["time"] = time
        return t
    kq = kv_place["slots"][-1]["pos"]
    iq = ic_place["slots"][-1]["pos"]
    out[TELEPORTS] = dump(OD([("teleports", [
        tp("kvass", at_local(kq, 0.0, 1.5, 0.0), 180.0, "14:00"),
        tp("icecart", at_local(iq, 0.0, -1.5, 0.0), 0.0, "14:00"),
        tp("sweep", sweep["slots"][0]["pos"], 90.0, "05:45"),
        tp("yard", at_local(y1["c"], y1.get("yaw", 0.0), 0.0, -6.0), y1.get("yaw", 0.0) + 90, "10:30"),
        tp("dominoes", at_local(table, side, -3.0, 0.0), side, "19:00"),
    ])]))
    return out, jobs, laundress, new


def check_town_places_tp(path):
    return path


def main():
    out, jobs, laundress, new = build()
    stale = [p for p, t in out.items() if not os.path.exists(p) or open(p, encoding="utf-8").read() != t]
    if "--check" in sys.argv:
        for p in stale:
            print("street_life: %s is not up to date (python3 tools/town/street_life.py)" % os.path.relpath(p, ROOT))
        sys.exit(1 if stale else 0)
    for p in stale:
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "w", encoding="utf-8") as f:
            f.write(out[p])
        print("wrote", os.path.relpath(p, ROOT))
    print("street life: %d places; kvass %s, ice cream %s, yard keeper %s, milk %s; %d laundresses" % (
        len(new), jobs["kvass"]["key"], jobs["icecream"]["key"], jobs["keeper"]["key"], jobs["milk"]["key"], len(laundress)))


if __name__ == "__main__":
    main()
