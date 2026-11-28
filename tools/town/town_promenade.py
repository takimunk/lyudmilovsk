#!/usr/bin/env python3
"""The warm season's evenings on the promenade and Sun Square (docs/promenade.md): the summer dances move out of the hall
«Своды» onto the square's paving in front of the stage (late May to early September, Friday and Saturday evenings), the
brass band plays on the stage on summer Sunday afternoons (the town stands and listens, the children run about), and on
warm evenings families, couples and teens walk the promenade there and back at sunset while the benches fill. No new
geometry: the stage, the square, the promenade and its benches are the public spaces' own (docs/city/public_spaces.md).

Like tools/town/town_dances.py this adds to the town generator's output and is idempotent (tools/town/town_life.py runs it
after itself; after tools/public_spaces.py moved the square's things, run it again):
    python3 tools/town/town_promenade.py            add or refresh
    python3 tools/town/town_promenade.py --check    exit 1 when the files are not as this script makes them

What it writes:
  Content/PeopleData/town_places.json   P-PROM-DANCE (kind dance_square: pairs of dance spots facing each other on the
                                        paving in front of the stage, the watchers' spots round its edge), P-PROM-BAND (kind
                                        band_square: the listeners' rows in front of the stage, the children's running
                                        spots behind them, the band's eight places on the stage), S-PROM-EVENING (kind evening_promenade: a walked round along the
                                        promenade by the sea to the north and back by the town)
  Content/PeopleData/activities.json    its rows (ACTIVITIES_ROWS), one a line, after the others' (each "when" a condition of
                                        ULyudStreetLife: prom_dances, prom_band, prom_evening)
  Content/PeopleData/townsfolk.json     the band: eight residents with a Sunday duty band_play at the stage ("band_job")
  Content/Localization/town/strings_<lang>.json   the band's role
  tools/city/extra_places.json          the hero's city things (ids P-PROM-*): the dance places and the ladies' choice
                                        pillar-lamp (P-PROM-DANCE, shown while prom_dances), the spot on the slabs before the
                                        band (P-PROM-BAND, while prom_band), the poster and the floor's look (P-PROM-BOARD)
                                        and the evening light by the sea (P-PROM-LIGHT), always
  Content/Dev/teleports/promenade.json  developer mode's points (group "promenade")

The hall's own dances (town_dances.py) carry "when": "hall_dances" (all the year but June to August), so in the summer
months the town dances here; at the season's turns (late May, early September) both floors are open.
"""
import json
import math
import os
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import town_activities as TA  # noqa: E402

OD = TA.OD
ROOT = TA.ROOT
CONTENT = TA.CONTENT
PUBLIC = os.path.join(CONTENT, "City", "public_spaces.json")
EXTRA = os.path.join(ROOT, "tools", "city", "extra_places.json")
TELEPORTS = os.path.join(CONTENT, "Dev", "teleports", "promenade.json")
PLACE_IDS = ["P-PROM-DANCE", "P-PROM-BAND", "S-PROM-EVENING"]
EXTRA_IDS = ["P-PROM-DANCE", "P-PROM-BAND", "P-PROM-BOARD", "P-PROM-LIGHT"]

# The dances 19-23 on Friday and Saturday (people set off from a quarter past seven, the last leave before eleven), the
# band 15-17 on Sunday, the promenade 18:30-22:30: the weekdays, the season and the sky are the conditions'
# (StreetLifeModel.h), so the rows carry no "days" of their own; the hours keep them inside.
ACTIVITIES_ROWS = [
    '{"id": "prom_dance", "places": ["dance_square"], "slots": ["dance_spot"], "pose": "stand", "minutes": [40, 90], "hours": [19.25, 21.4], "ages": [15, 45], "meets": {"fun": 0.95, "social": 0.8, "nature": 0.2}, "costs": {"tired": 0.06}, "base": 0.65, "trait": "social", "when": "prom_dances"}',
    '{"id": "prom_dance_watch", "places": ["dance_square"], "slots": ["watch_spot"], "pose": "look", "minutes": [20, 60], "hours": [19.25, 22.0], "ages": [46, 200], "meets": {"social": 0.7, "fun": 0.5, "nature": 0.2}, "base": 0.35, "trait": "social", "when": "prom_dances"}',
    '{"id": "band_play", "places": ["band_square"], "slots": ["staff"], "pose": "stand", "minutes": [60, 60], "meets": {"social": 0.5, "fun": 0.6}, "free": 0, "staff": 1, "when": "prom_band", "on_time": 1}',
    '{"id": "band_listen", "places": ["band_square"], "slots": ["listen_spot"], "pose": "look", "minutes": [40, 110], "hours": [15.0, 16.5], "ages": [13, 200], "meets": {"fun": 0.85, "social": 0.6, "nature": 0.2}, "base": 0.5, "trait": "social", "when": "prom_band"}',
    '{"id": "band_kids", "places": ["band_square"], "slots": ["run_spot"], "pose": "walk", "minutes": [15, 45], "hours": [15.0, 16.7], "ages": [4, 12], "meets": {"fun": 1.0, "social": 0.4, "nature": 0.2}, "costs": {"tired": 0.08, "hunger": 0.08}, "base": 0.7, "trait": "active", "when": "prom_band", "wander": 1, "wander_m": [3, 25], "wander_pause": [2, 10], "pace": 2.0}',
    '{"id": "prom_evening_walk", "places": ["evening_promenade"], "slots": ["stand"], "pose": "walk", "minutes": [30, 65], "hours": [18.6, 22.0], "ages": [6, 200], "meets": {"nature": 0.8, "social": 0.55, "sun": 0.35}, "costs": {"tired": 0.02, "hunger": 0.03}, "base": 0.45, "trait": "nature", "when": "prom_evening", "wander": 1, "round": 1, "wander_pause": [6, 40]}',
    '{"id": "prom_sunset_bench", "places": ["bench"], "slots": ["bench"], "pose": "sit", "minutes": [20, 50], "hours": [19.5, 22.2], "ages": [25, 200], "meets": {"sun": 1.0, "nature": 0.6, "social": 0.3}, "base": 0.3, "trait": "sun", "when": "prom_evening"}',
]
ACT_IDS = [json.loads(r)["id"] for r in ACTIVITIES_ROWS]

# --- where (Unreal metres; the square's paving at the stage's level, z -36.16) -----------------------------------------
# the stage stands on the square's west side facing the sea (+x); the fountain's middle is 55 m east of it
STAGE_FRONT_X = -33.0          # the stage's lip (the S-STAGE slots stand behind it)
SQUARE_Z = -36.16
AXIS_Y = 912.9
# the dance floor: the paving between the stage and the lamp row, clear of the lamps and the flowerbeds
FLOOR_X = (-28.5, -14.0)
FLOOR_Y = (902.5, 923.5)
PAIR_STEP = (2.1, 2.4)          # a pair's room (m along x, along y)
PAIR_HALF = 0.28
MAX_PAIRS = 18
# the hero's things on the floor (x, y): two places to ask a girl, the ladies' choice by the lamp on the floor's corner
HERO_DANCE = {"pev_dance_1": (-23.0, 909.0, 0.0), "pev_dance_2": (-19.0, 917.5, 180.0), "pev_white": (-15.0, 905.5, 135.0)}
HERO_CLEAR = 0.9
PROP_CLEAR = 1.2
# the band's listeners: rows facing the stage from 4 m off its lip, the hero's spot on the slabs in the front row's middle
LISTEN_ROWS = [(4.6, 9), (6.0, 11), (7.4, 13), (8.8, 13), (10.2, 11)]      # (metres from the lip, how many across)
LISTEN_STEP = 1.1
HERO_BAND = (STAGE_FRONT_X + 2.6, AXIS_Y)
# the band's eight on the stage's top (the S-STAGE slots' rows: x -37.9 and -35.9, y 909.9..915.9, z -35.12): the tuba
# and trombones behind, the trumpets and clarinets in front, all facing the sea
BAND_SPOTS = [(-37.7, y) for y in (909.6, 911.8, 914.0, 916.2)] + [(-35.7, y) for y in (909.6, 911.8, 914.0, 916.2)]
STAGE_Z = -35.12
RUN_SPOTS = [(-17.0, 898.0), (-14.0, 906.0), (-12.0, 916.0), (-14.5, 927.0), (-19.0, 931.0), (-9.0, 899.5), (-8.0, 925.5), (-21.0, 894.5)]
# the evening promenade: north by the sea from the square to the fair's end, back by the town (the stroll paths of the
# public spaces), a spot every SPACING metres
PROM_FROM, PROM_TO, SPACING = 905.0, 1190.0, 16.0
# the poster stand by the stage's south corner, the floor's look from its north edge, the evening light on the seawall
# kerb north of the pier's root (the lighthouse on the pier's head in view)
POSTER = (-31.5, 897.0, 0.0)
FLOOR_LOOK = (-21.0, 927.5, 270.0)
LIGHT_NEAR = (141.0, 927.0)

BAND_N = 8
BAND_ROLE = {"ru": ("музыкант духового оркестра", "музыкантша духового оркестра"), "en": ("brass band player", "brass band player"),
             "ja": ("吹奏楽団員", "吹奏楽団員")}
BAND_DAYS = [6]
BAND_HOURS = [15.0, 17.0]


def r2(v):
    return [round(v[0], 2), round(v[1], 2), round(v[2], 2)]


def props_xy():
    pub = TA.load(PUBLIC)
    pts = [p["at"] for p in pub["props"] if p["kind"] not in ("railing", "tree_grate", "stage")]
    for f in pub.get("fountains", []):
        c = f["at"]
        r = f.get("stone", {}).get("rim_r", 9.0) + 0.6
        pts += [[c[0] + r * math.cos(a), c[1] + r * math.sin(a)] for a in [k * math.pi / 12 for k in range(24)]]
    return pts, pub


def clear_of(x, y, pts, d):
    return min(math.hypot(x - q[0], y - q[1]) for q in pts) >= d


def dance_square(pts):
    hero = [(v[0], v[1]) for v in HERO_DANCE.values()]
    slots = []
    nx = int((FLOOR_X[1] - FLOOR_X[0]) / PAIR_STEP[0]) + 1
    ny = int((FLOOR_Y[1] - FLOOR_Y[0]) / PAIR_STEP[1]) + 1
    pairs = []
    for i in range(nx):
        for j in range(ny):
            cx = FLOOR_X[0] + i * PAIR_STEP[0] + (0.6 if j % 2 else 0.0)
            cy = FLOOR_Y[0] + j * PAIR_STEP[1]
            if cx > FLOOR_X[1]:
                continue
            ok = all(clear_of(cx + s * PAIR_HALF, cy, pts, PROP_CLEAR) and clear_of(cx + s * PAIR_HALF, cy, hero, HERO_CLEAR) for s in (-1, 1))
            if ok:
                pairs.append((cx, cy))
    if len(pairs) < 12:
        sys.exit("town_promenade: only %d pairs fit on the square's dance floor" % len(pairs))
    # a summer evening's floor holds more than the hall's room (ten pairs) but not every square metre of it: MAX_PAIRS,
    # spread evenly over the floor
    if len(pairs) > MAX_PAIRS:
        step = len(pairs) / float(MAX_PAIRS)
        pairs = [pairs[int(k * step)] for k in range(MAX_PAIRS)]
    for cx, cy in pairs:
        # each pair faces each other across x (the one on the low side faces +x, the other -x)
        slots.append(OD([("kind", "dance_spot"), ("pos", r2([cx - PAIR_HALF, cy, SQUARE_Z])), ("yaw", 0.0)]))
        slots.append(OD([("kind", "dance_spot"), ("pos", r2([cx + PAIR_HALF, cy, SQUARE_Z])), ("yaw", 180.0)]))
    # the watchers round the floor's long sides and its sea end, facing in
    watch = []
    for k in range(8):
        x = FLOOR_X[0] + 1.0 + k * 1.9
        watch.append((x, FLOOR_Y[0] - 2.2, 90.0))
        watch.append((x, FLOOR_Y[1] + 2.2, 270.0))
    for k in range(5):
        watch.append((FLOOR_X[1] + 2.4, FLOOR_Y[0] + 2.0 + k * 4.2, 180.0))
    n = 0
    for x, y, yaw in watch:
        if clear_of(x, y, pts, 1.0) and clear_of(x, y, hero, HERO_CLEAR):
            slots.append(OD([("kind", "watch_spot"), ("pos", r2([x, y, SQUARE_Z])), ("yaw", yaw)]))
            n += 1
    if n < 10:
        sys.exit("town_promenade: only %d watchers' spots round the dance floor" % n)
    return OD([("id", "P-PROM-DANCE"), ("kind", "dance_square"), ("pos", r2([sum(FLOOR_X) / 2, AXIS_Y, SQUARE_Z])), ("yaw", 0.0),
               ("island", 0), ("slots", slots)]), len(pairs)


def band_square(pts):
    slots = []
    for d, n in LISTEN_ROWS:
        x = STAGE_FRONT_X + d
        for k in range(n):
            y = AXIS_Y + (k - (n - 1) / 2.0) * LISTEN_STEP
            if math.hypot(x - HERO_BAND[0], y - HERO_BAND[1]) < 0.9 or not clear_of(x, y, pts, 0.9):
                continue
            # turned a little toward the stage's middle
            yaw = math.degrees(math.atan2(AXIS_Y - y, (STAGE_FRONT_X - 3.0) - x)) % 360.0
            slots.append(OD([("kind", "listen_spot"), ("pos", r2([x, y, SQUARE_Z])), ("yaw", round(yaw, 1))]))
    for x, y in BAND_SPOTS:
        slots.append(OD([("kind", "staff"), ("pos", r2([x, y, STAGE_Z])), ("yaw", 0.0)]))
    for x, y in RUN_SPOTS:
        if clear_of(x, y, pts, 1.0):
            slots.append(OD([("kind", "run_spot"), ("pos", r2([x, y, SQUARE_Z])), ("yaw", round(zlib.crc32(("%s%s" % (x, y)).encode()) % 360, 1))]))
    return OD([("id", "P-PROM-BAND"), ("kind", "band_square"), ("pos", r2([STAGE_FRONT_X + 7.0, AXIS_Y, SQUARE_Z])), ("yaw", 180.0),
               ("island", 0), ("slots", slots)])


def path_at(path, y):
    """The point of a stroll polyline (running south to north) at the given y."""
    for a, b in zip(path, path[1:]):
        if a[1] <= y <= b[1]:
            t = (y - a[1]) / ((b[1] - a[1]) or 1.0)
            return [a[0] + (b[0] - a[0]) * t, y, a[2]]
    sys.exit("town_promenade: the stroll path does not reach y %.1f" % y)


def evening_round(places):
    sea = next(p for p in places if p["id"] == "S-PROMENADE-SEA")
    town = next(p for p in places if p["id"] == "S-PROMENADE-TOWN")
    spots = TA.load(os.path.join(CONTENT, "PeopleData", "town_plan.json"))["spots"]
    path = {s["id"]: s["path"] for s in spots if s["id"] in ("S-PROMENADE-SEA", "S-PROMENADE-TOWN")}
    n = int((PROM_TO - PROM_FROM) / SPACING)
    slots = []
    for k in range(n + 1):
        p = path_at(path["S-PROMENADE-SEA"], PROM_FROM + k * SPACING)
        slots.append(OD([("kind", "stand"), ("pos", r2(p)), ("yaw", 90.0), ("capacity", 4)]))
    for k in range(n, -1, -1):
        p = path_at(path["S-PROMENADE-TOWN"], PROM_FROM + k * SPACING)
        slots.append(OD([("kind", "stand"), ("pos", r2(p)), ("yaw", 270.0), ("capacity", 4)]))
    del sea, town
    return OD([("id", "S-PROM-EVENING"), ("kind", "evening_promenade"), ("pos", slots[0]["pos"]), ("yaw", 0.0), ("island", 0), ("slots", slots)])


# --- the hero's things (tools/city/extra_places.json) -------------------------------------------------------------

def to_local(front, yaw, w):
    """Unreal metres -> the place's frame (interiors.py local_to_world, which is its own inverse)."""
    a = math.radians(yaw)
    dx, dy = w[0] - front[0], w[1] - front[1]
    return [round(dx * math.sin(a) - dy * math.cos(a), 3), round(-dx * math.cos(a) - dy * math.sin(a), 3)]


def extra(pid, building, front, name_ru, name_en, things):
    """An extra place at front (yaw 0) with its things: (id, kind, world x, y, rise over the paving, world yaw, props)."""
    out = []
    for tid, kind, x, y, z, wyaw, props in things:
        lx, ly = to_local(front, 0.0, (x, y))
        out.append(OD([("id", tid), ("kind", kind), ("pos", [lx, ly, z]), ("yaw", round((0.0 - wyaw) % 360.0, 1)), ("props", props)]))
    # the footprint: round the things with 1.5 m to spare (the square's paving is open ground; this only bounds the place)
    xs = [t["pos"][0] for t in out] + [0.0]
    ys = [t["pos"][1] for t in out] + [0.0]
    x0, x1, y0, y1 = min(xs) - 1.5, max(xs) + 1.5, min(ys) - 1.5, max(ys) + 1.5
    fp = [[round(x0, 2), round(y0, 2)], [round(x1, 2), round(y0, 2)], [round(x1, 2), round(y1, 2)], [round(x0, 2), round(y1, 2)]]
    return OD([("id", pid), ("kind", "yard"), ("building", building), ("front", r2(front)), ("yaw", 0.0), ("w", 3.0), ("d", 3.0),
               ("name_ru", name_ru), ("name_en", name_en),
               ("export", OD([("footprint", fp), ("entrances", [OD([("id", "front"), ("pos", [0.0, -1.2, 0.0]), ("kind", "street")])]),
                              ("things", out)]))])


def light_point(places, pts):
    kerbs = [p for p in places if p["id"].startswith("S-KERB-")]
    k = min(kerbs, key=lambda p: math.hypot(p["pos"][0] - LIGHT_NEAR[0], p["pos"][1] - LIGHT_NEAR[1]))
    # a step in from the kerb (west), clear of the lamps by the sea
    for dy in (0.0, 2.0, -2.0, 4.0, -4.0, 6.0):
        x, y = k["pos"][0] - 1.0, LIGHT_NEAR[1] + dy
        if clear_of(x, y, pts, 1.4):
            return [round(x, 2), round(y, 2), k["pos"][2]]
    sys.exit("town_promenade: no clear point for the evening light near %s" % (LIGHT_NEAR,))


def extras(places, pts):
    z = SQUARE_Z
    dance = extra("P-PROM-DANCE", "prom_dance", [sum(FLOOR_X) / 2, AXIS_Y, z], "Танцы на площади", "The dances on the square", [
        (tid, "sport", x, y, 1.0, yaw, OD([("what", "dance")])) for tid, (x, y, yaw) in HERO_DANCE.items()])
    band = extra("P-PROM-BAND", "prom_band", [HERO_BAND[0], HERO_BAND[1], z], "Духовой оркестр", "The brass band", [
        ("pev_band_spot", "seat", HERO_BAND[0], HERO_BAND[1], 0.1, 180.0, OD([("what", "listen"), ("height", 0.1)]))])
    board = extra("P-PROM-BOARD", "prom_board", [POSTER[0], POSTER[1], z], "Афиша у эстрады", "The poster by the stage", [
        ("pev_poster", "board", POSTER[0], POSTER[1], 1.5, POSTER[2], OD([("topic", "timetable")])),
        ("pev_floor", "board", FLOOR_LOOK[0], FLOOR_LOOK[1], 1.5, FLOOR_LOOK[2], OD([("topic", "view")]))])
    lp = light_point(places, pts)
    light = extra("P-PROM-LIGHT", "prom_light", lp, "Вечерний свет у моря", "The evening light by the sea", [
        ("pev_light", "board", lp[0], lp[1], 1.5, 180.0, OD([("topic", "view")]))])
    return [dance, band, board, light], lp


# --- the band --------------------------------------------------------------------------------------------------------

FREE_JOBS = ("unemployed", "homemaker", "pensioner")


def band_people(people):
    """Eight of the town who have no work and no duty of any add-on (pensioners mostly, as a town's brass band is), 18 to 78,
    up by noon; picked by a die of their key, so the same eight every run."""
    fit = [p for p in people if p.get("occupation") in FREE_JOBS and not p.get("duties") and not p.get("night_job") and p.get("stay") is None
           and p.get("town", 1) and 18 <= p["age"] <= 78 and p.get("wake", 7.5) <= 11.0 and not p.get("street_job")]
    if len(fit) < BAND_N:
        sys.exit("town_promenade: only %d free for the band" % len(fit))
    return sorted(fit, key=lambda p: zlib.crc32(("brass" + p["key"]).encode()))[:BAND_N]


def put_rows(text):
    lines = text.split("\n")
    have = {a: next((i for i, l in enumerate(lines) if '"id": "%s"' % a in l), -1) for a in ACT_IDS}
    if all(i >= 0 for i in have.values()):
        for a, r in zip(ACT_IDS, ACTIVITIES_ROWS):
            comma = "," if lines[have[a]].rstrip().endswith(",") else ""
            lines[have[a]] = "    " + r + comma
    else:
        lines = [l for l in lines if not any('"id": "%s"' % x in l for x in ACT_IDS)]
        rows = ["    " + r + "," for r in ACTIVITIES_ROWS]
        end = max(i for i, l in enumerate(lines) if l.strip() == "]")
        last = max(i for i in range(end) if lines[i].strip())
        if not lines[last].rstrip().endswith(","):
            lines[last] = lines[last].rstrip() + ","
        rows[-1] = rows[-1][:-1]
        lines[last + 1:end] = rows
    out = "\n".join(lines)
    json.loads(out)
    return out


def build():
    out = {}
    pts, pub = props_xy()
    pd = TA.load(TA.PLACES)
    places = [p for p in pd["places"] if p["id"] not in PLACE_IDS]
    dance, npairs = dance_square(pts)
    new = [dance, band_square(pts), evening_round(places)]
    fresh = {p["id"]: p for p in new}
    had = [p["id"] for p in pd["places"]]
    if all(i in had for i in PLACE_IDS):
        pd["places"] = [fresh.get(p["id"], p) for p in pd["places"]]
    else:
        pd["places"] = [p for p in pd["places"] if p["id"] not in PLACE_IDS] + new
    out[TA.PLACES] = TA.dump(pd)
    out[TA.ACTIVITIES] = put_rows(open(TA.ACTIVITIES, encoding="utf-8").read())

    fd = TA.load(TA.FOLK)
    people = fd["people"]
    for p in people:
        if p.get("band_job"):
            p.pop("band_job", None)
            p["duties"] = [d for d in p.get("duties", []) if d.get("activity") not in ACT_IDS]
            if not p["duties"]:
                p.pop("duties")
    band = band_people(people)
    for p in band:
        p["band_job"] = "brass"
        p["duties"] = [OD([("activity", "band_play"), ("place", "P-PROM-BAND"), ("hours", list(BAND_HOURS)), ("days", list(BAND_DAYS))])]
    out[TA.FOLK] = TA.dump(fd)
    keys = {p["key"]: p for p in band}
    for lang in ("ru", "en", "ja"):
        sd = TA.load(TA.STRINGS % lang)
        for k, p in keys.items():
            sd["npc.%s.role" % k] = BAND_ROLE[lang][1 if p.get("female") else 0]
        out[TA.STRINGS % lang] = TA.dump(sd)

    ex = TA.load(EXTRA)
    mine, lp = extras(pd["places"], pts)
    rest = [e for e in ex["places"] if e["id"] not in EXTRA_IDS]
    # before street_life.py's block (it keeps the others where they stand and puts its own at the end), so either may run last
    at = next((i for i, e in enumerate(rest) if e["id"].startswith("P-STREET-")), len(rest))
    ex["places"] = rest[:at] + mine + rest[at:]
    out[EXTRA] = json.dumps(ex, ensure_ascii=False, indent=1) + "\n"

    def tp(i, x, y, yaw, time=None):
        t = OD([("id", "promenade." + i), ("group", "promenade"), ("name", "dev.tp.promenade." + i), ("x", round(x, 1)), ("y", round(y, 1)),
                ("yaw", round(yaw % 360, 1))])
        if time:
            t["time"] = time
        return t
    out[TELEPORTS] = TA.dump(OD([("teleports", [
        tp("dances", FLOOR_X[1] + 4.0, AXIS_Y, 180.0, "20:30"),
        tp("band", STAGE_FRONT_X + 12.0, AXIS_Y, 180.0, "15:10"),
        tp("poster", POSTER[0] + 1.5, POSTER[1], 180.0),
        tp("sunset", lp[0] - 1.5, lp[1], 0.0, "20:40"),
        tp("evening", path_at(next(s["path"] for s in TA.load(os.path.join(CONTENT, "PeopleData", "town_plan.json"))["spots"]
                                   if s["id"] == "S-PROMENADE"), 1000.0)[0], 1000.0, 90.0, "20:00"),
    ])]))
    return out, new, band, npairs


def main():
    out, new, band, npairs = build()
    stale = [p for p, t in out.items() if not os.path.exists(p) or open(p, encoding="utf-8").read() != t]
    if "--check" in sys.argv:
        for p in stale:
            print("town_promenade: %s is not up to date (python3 tools/town/town_promenade.py)" % os.path.relpath(p, ROOT))
        sys.exit(1 if stale else 0)
    for p in stale:
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "w", encoding="utf-8") as f:
            f.write(out[p])
        print("wrote", os.path.relpath(p, ROOT))
    print("town_promenade: %s; %d dancing pairs; the band %s" % (", ".join("%s %d slots" % (p["id"], len(p["slots"])) for p in new), npairs,
                                                                  ", ".join(p["key"] for p in band)))


if __name__ == "__main__":
    main()
