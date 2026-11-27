#!/usr/bin/env python3
"""The townsfolk take up the town's new things to do (docs/town_life.md "The new activities", backlog item 26): the cinema's
sessions at the Rassvet, the sanatorium's waters and its dominoes, the piano on the stage of the hall «Своды», the allotment's
beds, the morning paper on a bench, stones skipped off the beach, the postman's round.

Like tools/town/sanatorium_guests.py this adds to the town generator's output and is idempotent (tools/town/town_life.py runs
it after itself; after tools/city/interiors.py or tools/garden/build_garden.py changed a seat or a bed, run it again):
    python3 tools/town/town_activities.py            add or refresh
    python3 tools/town/town_activities.py --check    exit 1 when the files are not as this script makes them

What it writes:
  Content/PeopleData/town_places.json   walked places at the end of the list (ids in PLACE_IDS): seats at the things of
                                        Content/City/interiors.json and the spots of blender/export/city/<type>.json,
                                        the allotment's beds from Content/Garden/garden.json, the postman's doors
  Content/PeopleData/activities.json    the rows of ACTIVITIES (after the last row, one a line)
  Content/PeopleData/townsfolk.json     the bed keepers' likes, the pianists' likes, the postman's duty and job
  Content/Localization/town/strings_<lang>.json   the postman's job

A slot's "pos" is the floor under the seat (a seat thing's own point is on its top: minus its height). A slot's "path" is its
own way on from the place's "path" (an aisle, a doorway); "thing" is the city thing held while somebody sits there (the hero
gets "taken"); "owner" is the only person who uses it. A place with "open": 1 is out of doors though it is entered by its
gate (the weather counts there).
"""
import collections
import json
import math
import os
import re
import sys
import zlib

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
FOLK = os.path.join(CONTENT, "PeopleData", "townsfolk.json")
PLACES = os.path.join(CONTENT, "PeopleData", "town_places.json")
ACTIVITIES = os.path.join(CONTENT, "PeopleData", "activities.json")
INTERIORS = os.path.join(CONTENT, "City", "interiors.json")
PLAN = os.path.join(CONTENT, "City", "city_plan.json")
GARDEN = os.path.join(CONTENT, "Garden", "garden.json")
EXPORT = os.path.join(ROOT, "blender", "export", "city", "%s.json")
STRINGS = os.path.join(CONTENT, "Localization", "town", "strings_%s.json")
OD = collections.OrderedDict

PLACE_IDS = ["P-CLUB2-CINEMA", "P-SANATORIUM-LOUNGE", "P-SANATORIUM-PUMP", "P-HALL-STAGE", "P-T2H2-ALLOTMENT", "S-POST-ROUND", "S-BEACH-STONES", "S-SODA"]

# activities.json rows, one a line. The cinema has two sessions a day (the Rassvet's programme: the day's film from 15:00,
# the evening's from 19:00): "hours" is when people set off for one. "temp": [min, max] C: chosen only in such weather.
ACTIVITIES_ROWS = [
    '{"id": "cinema_day", "places": ["cinema_hall"], "slots": ["cinema_seat"], "pose": "sit", "minutes": [88, 96], "hours": [14.6, 15.2], "ages": [6, 200], "meets": {"fun": 0.9, "mind": 0.3, "social": 0.25}, "base": -0.32, "trait": "social"}',
    '{"id": "cinema_evening", "places": ["cinema_hall"], "slots": ["cinema_seat"], "pose": "sit", "minutes": [88, 96], "hours": [18.6, 19.2], "ages": [12, 200], "meets": {"fun": 0.9, "mind": 0.3, "social": 0.3}, "base": -0.05, "trait": "social"}',
    '{"id": "san_cure", "places": ["sanatorium_pump"], "slots": ["cure_seat"], "pose": "sit", "minutes": [20, 40], "hours": [9, 12.5], "days": [0, 1, 2, 3, 4, 5], "ages": [60, 200], "meets": {"tired": 0.2, "social": 0.35, "hunger": 0.3}, "base": -0.28, "trait": ""}',
    '{"id": "dominoes", "places": ["sanatorium_lounge"], "slots": ["domino_seat"], "pose": "sit", "minutes": [40, 90], "hours": [15, 20], "ages": [58, 200], "meets": {"social": 0.9, "fun": 0.7, "mind": 0.2}, "base": -0.3, "trait": "social"}',
    '{"id": "play_piano", "places": ["hall_stage"], "slots": ["piano_seat"], "pose": "sit", "minutes": [15, 35], "hours": [18, 20.6], "ages": [10, 200], "meets": {"fun": 0.6, "mind": 0.5}, "base": -1.7, "trait": "studious"}',
    '{"id": "tend_allotment", "places": ["allotment"], "slots": ["bed_side"], "pose": "stand", "minutes": [30, 70], "hours": [6.5, 10.5], "daylight": 1, "temp": [6, 40], "meets": {"nature": 0.9, "sun": 0.4, "fun": 0.2}, "costs": {"tired": 0.08, "hunger": 0.08}, "base": -1.6, "trait": "nature"}',
    '{"id": "water_allotment", "places": ["allotment"], "slots": ["bed_side"], "pose": "stand", "minutes": [15, 30], "hours": [14.5, 20.5], "daylight": 1, "temp": [5, 40], "meets": {"nature": 0.8, "fun": 0.2}, "costs": {"tired": 0.04}, "base": -1.6, "trait": "nature"}',
    '{"id": "read_paper", "places": ["park", "pond", "fountain", "promenade", "promenade_bench", "town_bench", "bench"], "slots": ["bench"], "pose": "sit", "minutes": [20, 45], "hours": [7, 11.5], "daylight": 1, "temp": [4, 40], "ages": [30, 200], "meets": {"mind": 0.55, "nature": 0.3, "sun": 0.2}, "base": -0.2, "trait": "studious"}',
    '{"id": "skip_stones", "places": ["stone_shore"], "slots": ["stand"], "pose": "look", "minutes": [10, 25], "hours": [9, 20], "ages": [6, 70], "daylight": 1, "wind": [0, 7.5], "temp": [3, 40], "meets": {"fun": 0.8, "nature": 0.6, "sun": 0.2}, "costs": {"tired": 0.03}, "base": -0.25, "trait": "active"}',
    '{"id": "post_round", "places": ["post_round"], "slots": ["stand"], "pose": "stand", "wander": 1, "round": 1, "wander_pause": [50, 130], "minutes": [60, 60], "meets": {"social": 0.4, "nature": 0.5, "sun": 0.3}, "costs": {"hunger": 0.05, "tired": 0.03}, "free": 0, "rain": "ok"}',
    # the soda-water automats of the plan's street_machines (docs/soda_machines.md): a glass on a hot day, in their season
    # (May-October: "temp" keeps them to warm days; the machines' own months and broken days are the city things', not the walkers')
    '{"id": "soda_drink", "places": ["soda_machine"], "slots": ["stand"], "pose": "stand", "minutes": [3, 6], "hours": [9, 21], "daylight": 1, "temp": [22, 40], "ages": [6, 200], "meets": {"hunger": 0.2, "fun": 0.3, "sun": 0.1}, "base": -0.45, "trait": ""}',
]
ACT_IDS = [json.loads(r)["id"] for r in ACTIVITIES_ROWS]
OWN_LIKES = ("tend_allotment", "water_allotment", "play_piano")     # the likes this script gives (the guests' script gives its own)

# the Rassvet's hall (blender/city/buildings/workers_club.py, the building's frame): in by the main door, along the front
# wall of the foyer to the east door of the hall, across the flat floor in front of row 1 to an aisle, up it to the row
CINEMA_WAY = [(0.0, 4.7, 0.0), (13.0, 5.3, 0.0), (13.0, 11.9, 0.0), (13.0, 13.5, 0.0)]
CINEMA_AISLES = (-14.9, -8.7, -2.9, 2.9, 8.7, 14.9)
CINEMA_FRONT_Y = 13.6
# a few of the seats the hero can take are the townsfolk's too (held while they sit: "taken")
CINEMA_SHARED = ("seat_r5_b2", "seat_r5_b4", "seat_r8_b1", "seat_r8_b5", "seat_r2_b4", "seat_r10_b3")
# the sanatorium's ground hall (round_sanatorium.py): the entrance in the lobby; the door to the pump room in the
# partition at -70.7 degrees, 15 m from the ring's middle (0, 26)
SAN_INSIDE = (0.0, 9.6, 0.0)
SAN_LOUNGE = ("lobby_lounge_a", "lobby_lounge_b", "lobby_sofa", "lobby_sofa_b")
SAN_PUMP_WAY = [(4.26, 11.62, 0.0), (5.45, 12.1, 0.0)]
SAN_PUMP = (("pump_bench", (5.25, 13.2, 0.0)), ("pump_stool_w", (5.85, 12.75, 0.0)))
SAN_WAIT = ("lobby_bench_e", "lobby_bench_w")
# the hall «Своды» (shell_chapel_hall.py): up the main hall's east side past the chairs, the stage's east steps
# (y 16.5 .. 18.6, up to z 1.0), across the stage in front of the grand piano to its stool
HALL_WAY = [(-3.75, 4.4, 0.0), (-3.75, 6.7, 0.0), (1.75, 6.9, 0.0), (1.75, 16.4, 0.0), (1.75, 18.75, 1.0), (-2.2, 19.7, 1.0)]
HALL_STOOL = "stage_piano_stool"
ALLOTMENT_SKIP = ("hero", "maarja")     # the hero's bed; Maarja is shown by the garden itself (ULyudGarden::TickNeighbour)
PIANISTS_OF = ("musician", "choir_leader", "teacher", "dance_teacher")
POSTMAN_ROLE = {"ru": ("почтальон", "почтальон"), "en": ("postman", "postwoman"), "ja": ("郵便配達員", "郵便配達員")}
POST_HOURS, POST_DAYS = [8.5, 12.0], [0, 1, 2, 3, 4, 5]


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f, object_pairs_hook=OD)


def dump(d):
    return json.dumps(d, ensure_ascii=False, indent=1) + "\n"


def r2(v):
    return [round(v[0], 2), round(v[1], 2), round(v[2], 2)]


class Frame:
    """A building's frame in the world (x along the front, y into the plot, z up; metres): found from the things both the
    building's export and interiors.json have, so it is the frame the game's things really stand in."""

    def __init__(self, plot, plan, things):
        p = next(q for q in plan["plots"] if q["id"] == plot)
        self.plot, self.yaw = plot, p["yaw"]
        self.export = load(EXPORT % p["building"])
        a = math.radians(self.yaw)
        self.s, self.c = math.sin(a), math.cos(a)
        os_ = []
        for t in self.export.get("things", []):
            w = things.get("%s/%s" % (plot, t["id"]))
            if w:
                x, y, z = t["pos"]
                os_.append((w["ue"][0] / 100.0 - (x * self.s - y * self.c), w["ue"][1] / 100.0 - (-x * self.c - y * self.s), w["ue"][2] / 100.0 - z))
        if len(os_) < 3:
            sys.exit("town_activities: %s has no things in interiors.json to find its frame by" % plot)
        self.o = [sorted(v[i] for v in os_)[len(os_) // 2] for i in range(3)]
        off = max(max(abs(v[i] - self.o[i]) for i in range(3)) for v in os_)
        if off > 0.08:
            sys.exit("town_activities: %s: the export and interiors.json disagree by %.2f m (rerun tools/city/interiors.py)" % (plot, off))

    def world(self, local):
        x, y, z = local
        return r2([self.o[0] + x * self.s - y * self.c, self.o[1] - x * self.c - y * self.s, self.o[2] + z])

    def wyaw(self, local_yaw):
        return round((self.yaw - local_yaw) % 360.0, 1)

    def spot(self, sid):
        return next(s for s in self.export["spots"] if s["id"] == sid)


def seat_slot(kind, thing, **more):
    """A slot at a seat thing of interiors.json: the floor under it, its height, the thing held."""
    h = float((thing.get("props") or {}).get("height", 0.45))
    x, y, z = (v / 100.0 for v in thing["ue"])
    s = OD([("kind", kind), ("pos", r2([x, y, z - h])), ("yaw", round(thing["yaw"] % 360.0, 1)), ("height", h), ("thing", thing["id"])])
    s.update(more)
    return s


def need(things, tid):
    if tid not in things:
        sys.exit("town_activities: %s is not a thing in interiors.json (python3 tools/city/interiors.py)" % tid)
    return things[tid]


def walked(pid, kind, base, way, slots):
    return OD([("id", pid), ("kind", kind), ("door", list(base["door"])), ("yaw", base["yaw"]), ("island", 0), ("slots", slots),
               ("roles", []), ("building", base["building"]), ("path", way)])


def cinema(plan, things, by_id):
    f = Frame("P-CLUB2", plan, things)
    hero = [t["pos"] for t in f.export["things"] if t["id"].startswith("seat_r") or t["id"] == "reel_seat"]
    slots = []
    for s in f.export["spots"]:
        m = re.match(r"seat(\d+)$", s["id"])
        if not m:
            continue
        x, y, z = s["pos"]
        if any(abs(x - h[0]) < 0.3 and abs(y - h[1]) < 0.3 for h in hero):
            continue                    # the hero's own seats (the block's middle seat in rows 1, 2, 5, 8, 10)
        if zlib.crc32(s["id"].encode()) % 7 >= 3:
            continue                    # three seats in seven: room for the eye, and the slots stay few
        ax = min(CINEMA_AISLES, key=lambda a: abs(a - x))
        slots.append(OD([("kind", "cinema_seat"), ("pos", f.world((x, y, z))), ("yaw", f.wyaw(s.get("yaw", 0.0))), ("height", 0.45),
                         ("approach", f.world((ax, y, z))), ("path", [f.world((ax, CINEMA_FRONT_Y, 0.0))])]))
    for tid in CINEMA_SHARED:
        t = need(things, "P-CLUB2/" + tid)
        loc = next(q["pos"] for q in f.export["things"] if q["id"] == tid)
        ax = min(CINEMA_AISLES, key=lambda a: abs(a - loc[0]))
        slots.append(seat_slot("cinema_seat", t, approach=f.world((ax, loc[1], loc[2] - 0.45)), path=[f.world((ax, CINEMA_FRONT_Y, 0.0))]))
    return walked("P-CLUB2-CINEMA", "cinema_hall", by_id["P-CLUB2"], [f.world(p) for p in CINEMA_WAY], slots)


def sanatorium(plan, things, by_id):
    f = Frame("P-SANATORIUM", plan, things)
    base = by_id["P-SANATORIUM"]
    inside = f.world(SAN_INSIDE)
    lounge = [seat_slot("domino_seat", need(things, "P-SANATORIUM/" + t)) for t in SAN_LOUNGE]
    pump_way = [f.world(p) for p in SAN_PUMP_WAY]
    cure = [seat_slot("cure_seat", need(things, "P-SANATORIUM/" + t), approach=f.world(ap), path=pump_way) for t, ap in SAN_PUMP]
    cure += [seat_slot("cure_seat", need(things, "P-SANATORIUM/" + t)) for t in SAN_WAIT]     # waiting their turn in the lobby
    return [walked("P-SANATORIUM-LOUNGE", "sanatorium_lounge", base, [inside], lounge),
            walked("P-SANATORIUM-PUMP", "sanatorium_pump", base, [inside], cure)]


def hall(plan, things, by_id):
    f = Frame("P-HALL", plan, things)
    stool = seat_slot("piano_seat", need(things, "P-HALL/" + HALL_STOOL))
    return walked("P-HALL-STAGE", "hall_stage", by_id["P-HALL"], [f.world(p) for p in HALL_WAY], [stool])


def allotment(garden, folk_keys):
    """The commune house's yard (docs/allotment.md): in by the gate, up the lane between the two rows of beds; two places a
    bed on its lane side, its keeper's only."""
    site, gate = garden["site"], [v / 100.0 for v in garden["site"]["gate"]]
    cx = site["ue"][0] / 100.0
    slots = []
    for b in garden["beds"]:
        if b["id"] in ALLOTMENT_SKIP:
            continue
        if b["keeper"] not in folk_keys:
            sys.exit("town_activities: the bed keeper %s is not in townsfolk.json" % b["keeper"])
        bx, by, bz = (v / 100.0 for v in b["ue"])
        side = 1.0 if bx < cx else -1.0         # the lane is toward the yard's middle
        for dy in (-0.9, 0.9):
            slots.append(OD([("kind", "bed_side"), ("pos", r2([bx + side * (garden["bed_size"][0] / 2 + 0.45), by + dy, bz])),
                             ("yaw", 180.0 if side > 0 else 0.0),
                             # a standing place is stood at its approach point: the bed's side itself, reached along the lane
                             ("approach", r2([bx + side * (garden["bed_size"][0] / 2 + 0.45), by + dy, bz])),
                             ("path", [r2([cx, by + dy, bz])]), ("owner", b["keeper"])]))
    return OD([("id", "P-T2H2-ALLOTMENT"), ("kind", "allotment"), ("door", r2([gate[0], gate[1] - 1.0, gate[2]])), ("yaw", site["yaw"]),
               ("island", 0), ("open", 1), ("slots", slots), ("roles", []),
               ("path", [r2([gate[0] + 0.2, gate[1] + 2.6, gate[2]]), r2([cx, gate[1] + 6.0, gate[2]])])])


def stone_shore():
    """Where flat stones are sent over the water: spots along the town's beach on the wet sand's edge, 6.5 m up from the
    mean water line (the ground there is a hand over the springs' high water: tools/world_map.py), facing the sea."""
    sys.path.insert(0, os.path.join(ROOT, "tools"))
    import world_map as wm
    beach = next(p for p in load(PLACES)["places"] if p["id"] == "S-BEACH")
    ys = [sl["pos"][1] for sl in beach["slots"]]
    slots = []
    y = min(ys) + 12.0
    while y < max(ys):
        x = wm.shore_x(y) - 6.5
        pos = r2([x, y, wm.height(x, y)])
        slots.append(OD([("kind", "stand"), ("pos", pos), ("yaw", 0.0), ("height", 0.0), ("approach", list(pos))]))
        y += 45.0
    mid = slots[len(slots) // 2]["pos"]
    return OD([("id", "S-BEACH-STONES"), ("kind", "stone_shore"), ("pos", list(mid)), ("yaw", 0.0), ("island", 0), ("slots", slots), ("roles", [])])


def post_round(places):
    """The postman's doors: every house's street door once, a wall's dwellings every fortieth; from the sea front up, each
    terrace along."""
    doors = OD()
    wall_n = collections.Counter()
    for p in places:
        if p["kind"] != "flat" or not p.get("building"):
            continue
        b = p["building"]
        if b.startswith("W"):
            wall_n[b] += 1
            if wall_n[b] % 40 != 1:
                continue
            doors[p["id"]] = (p["door"], p["yaw"])
        elif b not in doors:
            doors[b] = (p["door"], p["yaw"])
    stops = sorted(doors.values(), key=lambda d: (round(d[0][2]), d[0][1]))
    # every other terrace the other way along: a round, not a comb
    rows = collections.OrderedDict()
    for d in stops:
        rows.setdefault(round(d[0][2]), []).append(d)
    slots = []
    for i, row in enumerate(rows.values()):
        for door, yaw in (row if i % 2 == 0 else row[::-1]):
            slots.append(OD([("kind", "stand"), ("pos", list(door)), ("yaw", round((yaw + 180.0) % 360.0, 1))]))
    return OD([("id", "S-POST-ROUND"), ("kind", "post_round"), ("pos", list(slots[0]["pos"])), ("yaw", 0.0), ("island", 0), ("slots", slots)])


def soda_machines(plan):
    """The soda-water automats (tools/city_plan.py STREET_MACHINES): one standing place a machine, where the hero drinks too
    (the building's "drink" spot: 0.85 m in front of the cabinet's middle), facing the machine."""
    slots = []
    for m in plan.get("street_machines", []):
        if m.get("building") != "soda_machine":
            continue
        c, f = [v / 100.0 for v in m["ue"]], [v / 100.0 for v in m["front_ue"]]
        dx, dy = f[0] - c[0], f[1] - c[1]
        n = math.hypot(dx, dy) or 1.0
        pos = r2([c[0] + dx / n * 0.85, c[1] + dy / n * 0.85, f[2]])
        slots.append(OD([("kind", "stand"), ("pos", pos), ("yaw", round(math.degrees(math.atan2(-dy, -dx)) % 360.0, 1)), ("height", 0.0),
                         ("approach", list(pos)), ("tag", m["id"])]))
    if not slots:
        sys.exit("town_activities: no soda machines in city_plan.json's street_machines (python3 tools/city_plan.py)")
    return OD([("id", "S-SODA"), ("kind", "soda_machine"), ("pos", list(slots[0]["pos"])), ("yaw", 0.0), ("island", 0), ("slots", slots), ("roles", [])])


def pick_postman(people):
    """Somebody without work, up early, of an age for the bag (the same one while the town is the same)."""
    was = [p for p in people if p.get("occupation") == "postman"]
    if was:
        return was[0]
    fit = [p for p in people if p.get("occupation") == "unemployed" and 24 <= p["age"] <= 58 and not p.get("duties")
           and p.get("wake", 9.0) <= 7.6 and 21.0 <= p.get("bed", 23.0) <= 24.0]
    if not fit:
        sys.exit("town_activities: nobody to carry the post (no unemployed early riser of 24-58)")
    return sorted(fit, key=lambda p: zlib.crc32(p["key"].encode()))[0]


def set_like(p, act, more):
    likes = p.setdefault("likes", OD())
    likes[act] = more


def build():
    out = {}
    plan = load(PLAN)
    things = {t["id"]: t for t in load(INTERIORS)["things"]}
    garden = load(GARDEN)
    pd = load(PLACES)
    places = [p for p in pd["places"] if p["id"] not in PLACE_IDS]
    was = [p["id"] for p in pd["places"]]
    by_id = {p["id"]: p for p in places}
    fd = load(FOLK)
    people = fd["people"]
    keys = {p["key"] for p in people}
    new = [cinema(plan, things, by_id)] + sanatorium(plan, things, by_id) + [hall(plan, things, by_id), allotment(garden, keys), post_round(places), stone_shore(), soda_machines(plan)]
    assert [p["id"] for p in new] == PLACE_IDS
    # each stays where it stands in the file (other add-ons put their places after ours); a new one goes to the end
    fresh = {p["id"]: p for p in new}
    pd["places"] = [fresh.get(p["id"], p) for p in pd["places"]] + [p for p in new if p["id"] not in was]
    out[PLACES] = dump(pd)

    # the people: the beds' keepers, those who play, the postman
    # (the sanatorium's guests are sanatorium_guests.py's own: their likes are left as it wrote them)
    people = [p for p in people if p.get("occupation") != "guest"]
    for p in people:
        for a in OWN_LIKES:
            (p.get("likes") or {}).pop(a, None)
    keepers = {b["keeper"] for b in garden["beds"] if b["id"] not in ALLOTMENT_SKIP}
    for p in people:
        if p["key"] in keepers:
            set_like(p, "tend_allotment", 2.0)
            set_like(p, "water_allotment", 2.7)
        # the piano: the town's musicians and teachers now and then, and one in thirty of the rest who learned as a child
        h = zlib.crc32(("piano:" + p["key"]).encode())
        if p["age"] >= 10 and (p.get("occupation") in PIANISTS_OF and h % 3 == 0 or h % 30 == 0):
            set_like(p, "play_piano", 1.75)
    postman = pick_postman(people)
    for p in people:
        if p is not postman and p.get("occupation") == "postman":
            p["occupation"] = "unemployed"
            p["duties"] = [d for d in p.get("duties", []) if d.get("activity") != "post_round"]
    postman["occupation"] = "postman"
    postman["duties"] = [d for d in postman.get("duties", []) if d.get("activity") != "post_round"] + [
        OD([("activity", "post_round"), ("place", "S-POST-ROUND"), ("hours", list(POST_HOURS)), ("days", list(POST_DAYS))])]
    out[FOLK] = dump(fd)
    for lang in ("ru", "en", "ja"):
        sd = load(STRINGS % lang)
        sd["npc.%s.role" % postman["key"]] = POSTMAN_ROLE[lang][1 if postman.get("female") else 0]
        out[STRINGS % lang] = dump(sd)

    # activities.json is written by hand, one activity a line: ours after the last row
    text = open(ACTIVITIES, encoding="utf-8").read()
    lines = text.split("\n")
    have = {a: next((i for i, l in enumerate(lines) if '"id": "%s"' % a in l), -1) for a in ACT_IDS}
    if all(i >= 0 for i in have.values()):
        # each row stays where it stands in the file (other add-ons put their rows after ours), with its comma as it is
        for a, r in zip(ACT_IDS, ACTIVITIES_ROWS):
            comma = "," if lines[have[a]].rstrip().endswith(",") else ""
            lines[have[a]] = "    " + r + comma
    else:
        lines = [l for l in lines if not any('"id": "%s"' % a in l for a in ACT_IDS)]
        end = max(i for i, l in enumerate(lines) if l.strip() == "]")
        last = max(i for i in range(end) if lines[i].strip())
        if not lines[last].rstrip().endswith(","):
            lines[last] = lines[last].rstrip() + ","
        rows = ["    " + r + "," for r in ACTIVITIES_ROWS]
        rows[-1] = rows[-1][:-1]
        lines[last + 1:end] = rows
    out[ACTIVITIES] = "\n".join(lines)
    json.loads(out[ACTIVITIES])
    return out, postman, new


def main():
    out, postman, new = build()
    stale = [p for p, t in out.items() if open(p, encoding="utf-8").read() != t]
    if "--check" in sys.argv:
        for p in stale:
            print("town_activities: %s is not up to date (python3 tools/town/town_activities.py)" % os.path.relpath(p, ROOT))
        sys.exit(1 if stale else 0)
    for p in stale:
        with open(p, "w", encoding="utf-8") as f:
            f.write(out[p])
        print("wrote", os.path.relpath(p, ROOT))
    print("town activities: %s; the postman is %s" % (", ".join("%s %d" % (p["id"], len(p["slots"])) for p in new), postman["key"]))


if __name__ == "__main__":
    main()
