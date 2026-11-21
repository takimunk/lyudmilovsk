#!/usr/bin/env python3
"""The town's night jobs (docs/city_night.md, backlog item 29): two cleaners who mop the public rooms before they open
(the café «Тарелка» and the Rassvet's foyer from 7, the department store's ground floor from 7, the library after it
shuts at 21) and the department store's night watchman, who walks its dark sales floor with a torch from 20:30 to 6:30.

Like tools/town/town_activities.py this adds to the town generator's output and is idempotent (tools/town/town_life.py
runs it after itself; after tools/city/interiors.py moved things in these buildings, run it again):
    python3 tools/town/town_night.py            add or refresh
    python3 tools/town/town_night.py --check    exit 1 when the files are not as this script makes them

What it writes:
  Content/PeopleData/town_places.json   walked places at the end of the list (PLACE_IDS): a round of floor spots in each
                                        building, in by its street door and the way inside (the café's up its spiral
                                        stair), spots in round order
  Content/PeopleData/activities.json    mop_floor and night_watch (duties only: "free": 0, "round": 1)
  Content/PeopleData/townsfolk.json     the cleaners (occupation cleaner) and the watchman (occupation guard): their
                                        duties, and the hours they sleep (the watchman by day)
  Content/Localization/town/strings_<lang>.json   their jobs

The props (manners.json "mop" and "torch"; blender/humans/props.py mop, torch, bucket) and the torch's beam
(ANpcBody::PlaceTorchLight) are the game's side; the dressing that shows the work (the bucket and the damp floor, the
chairs up on the tables, the one lamp left on) is in the buildings (blender/city/night_kit.py).
"""
import json
import math
import os
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import town_activities as TA  # noqa: E402

OD = TA.OD
PLACE_IDS = ["P-CAFE-CLEAN", "P-CLUB2-CLEAN", "P-STORE-CLEAN", "P-LIBRARY-CLEAN", "P-STORE-WATCH"]

# one activity a duty: the sim ends a duty at the To of its person's duty of that activity and goes to that duty's place,
# so a cleaner's second building of the day is another id ("mop_floor_late"), the same in all else
ACTIVITIES_ROWS = [
    '{"id": "mop_floor_late", "places": ["clean_round"], "slots": ["mop"], "pose": "stand", "wander": 1, "round": 1, "wander_pause": [70, 160], "minutes": [60, 60], "meets": {"mind": 0.1}, "costs": {"tired": 0.06, "hunger": 0.05}, "free": 0}',
    '{"id": "mop_floor", "places": ["clean_round"], "slots": ["mop"], "pose": "stand", "wander": 1, "round": 1, "wander_pause": [70, 160], "minutes": [60, 60], "meets": {"mind": 0.1}, "costs": {"tired": 0.06, "hunger": 0.05}, "free": 0}',
    '{"id": "night_watch", "places": ["watch_round"], "slots": ["patrol"], "pose": "stand", "wander": 1, "round": 1, "wander_pause": [40, 110], "minutes": [60, 60], "meets": {"mind": 0.15}, "costs": {"tired": 0.05, "hunger": 0.04}, "free": 0}',
]
ACT_IDS = [json.loads(r)["id"] for r in ACTIVITIES_ROWS]

# the rooms, in each building's own frame (x along the street front, y into the plot, z up; blender/export/city/<type>.json)
# the café «Тарелка» (saucer_pavilion.py): the glass tunnel from the street door to the core, then the spiral stair
# (26 treads of 360/26 degrees from 290, rise 4.3/26, round the core at (0, 13)), out onto the hall floor at z 4.3
CAFE_C, CAFE_Z = (0.0, 13.0), 4.3


def _cafe_way():
    def at(r, a, z):
        return (CAFE_C[0] + r * math.cos(math.radians(a)), CAFE_C[1] + r * math.sin(math.radians(a)), z)
    way = [(0.0, 3.0, 0.0), (0.0, 9.9, 0.0)]
    n, d = 26, 360.0 / 26
    for i in range(0, n, 2):
        way.append(at(1.4, 290.0 + d * (i + 0.5), CAFE_Z * (i + 1) / n))
    way.append(at(1.4, 290.0 + d * (n - 0.5), CAFE_Z))
    way.append(at(3.3, 272.0, CAFE_Z))
    return way, at


# the Rassvet (workers_club.py): the foyer between the main door and the buffet; its tables stay out of the way
CLUB_WAY = [(0.0, 4.7, 0.0)]
CLUB_MOP = [(-2.0, 6.1), (0.0, 6.0), (2.0, 5.9), (5.4, 5.6), (1.4, 8.7), (-1.4, 8.7), (-6.0, 5.8), (-10.5, 6.2)]
# the department store (department_store.py): the ground floor's hall, the red runner up the middle from the main door
STORE_WAY = [(0.0, 3.4, 0.0)]
STORE_MOP = [(3.0, 8.0), (5.6, 9.2), (3.0, 12.0), (-3.0, 12.0), (-3.0, 8.0), (-6.5, 9.0)]
# the watchman: up the runner, out to each side hall and back, and up to the stair's foot
STORE_WATCH = [((0.0, 7.0), None), ((-8.0, 9.0), 9.0), ((-15.5, 9.5), 9.0), ((0.0, 14.0), None), ((-8.0, 18.0), 18.0),
               ((0.0, 20.2), None), ((8.0, 18.0), 18.0), ((15.5, 9.5), 9.0), ((8.0, 9.0), 9.0)]
# the library (capsule_library.py): in front of the lending desk and the periodicals table
LIB_WAY = [(0.0, 4.8, 0.0)]
LIB_MOP = [(-8.6, 6.45), (-5.8, 6.2), (-2.8, 6.0), (2.8, 6.0), (6.4, 6.6), (-6.0, 4.6)]

CLEANER_A = [("P-CAFE-CLEAN", [7.0, 7.85]), ("P-CLUB2-CLEAN", [8.1, 9.9])]
CLEANER_B = [("P-STORE-CLEAN", [7.0, 8.85], [0, 1, 2, 3, 4, 5]), ("P-LIBRARY-CLEAN", [21.1, 22.4])]
WATCH = ("P-STORE-WATCH", [20.5, 6.5])
ALL_DAYS = [0, 1, 2, 3, 4, 5, 6]
# the hours each sleeps (check_town_data.py: 7..10.5 h, no duty starting or ending inside it); a small spread
SLEEP = {"a": (21.4, 5.6, "lark"), "b": (22.9, 6.1, "lark"), "w": (7.2, 14.8, "night")}
SPREAD = 0.25
ROLE = {"cleaner": {"ru": ("уборщик", "уборщица"), "en": ("cleaner", "cleaner"), "ja": ("清掃員", "清掃員")},
        "watch": {"ru": ("сторож", "сторож"), "en": ("night watchman", "night watchwoman"), "ja": ("夜警", "夜警")}}


def mop_slot(f, kind, xy, z=0.0, path=None):
    s = OD([("kind", kind), ("pos", f.world((xy[0], xy[1], z))), ("yaw", f.wyaw((zlib.crc32(repr(xy).encode()) % 8) * 45.0))])
    if path:
        s["path"] = [f.world(p) for p in path]
    return s


def cafe(plan, things, by_id):
    f = TA.Frame("P-CAFE", plan, things)
    way, at = _cafe_way()
    hub = way[-1]
    busy = [s["pos"] for s in f.export["spots"] if s["kind"] in ("seat", "table", "counter", "work", "view", "stand")
            and abs(s["pos"][2] - CAFE_Z) < 0.6]

    def clear(x, y):
        if min(math.hypot(x - p[0], y - p[1]) for p in busy) < 1.05:
            return False
        for k in range(1, 10):        # the straight way from the stair's top
            t = k / 10.0
            qx, qy = hub[0] + (x - hub[0]) * t, hub[1] + (y - hub[1]) * t
            if min(math.hypot(qx - p[0], qy - p[1]) for p in busy) < 0.55:
                return False
        return True

    pts = []
    for r in (3.6, 5.0, 6.6, 7.6):
        for a in range(0, 360, 12):
            if 40 <= a <= 140 or 228 <= a <= 312 and r < 4:
                continue                # behind the bar; the stairwell's mouth
            x, y, _ = at(r, a, CAFE_Z)
            if clear(x, y) and all(math.hypot(x - q[0], y - q[1]) > 1.6 for q in pts):
                pts.append((x, y))
    pts.sort(key=lambda q: math.atan2(q[1] - CAFE_C[1], q[0] - CAFE_C[0]) % (2 * math.pi))
    if len(pts) < 4:
        sys.exit("town_night: the café has %d clear floor spots to mop" % len(pts))
    return TA.walked("P-CAFE-CLEAN", "clean_round", by_id["P-CAFE"], [f.world(p) for p in way],
                     [mop_slot(f, "mop", q, CAFE_Z) for q in pts[:8]])


def simple(pid, plot, kind, slot_kind, way, pts, plan, things, by_id, z=0.0):
    f = TA.Frame(plot, plan, things)
    slots = []
    for p in pts:
        if isinstance(p[0], tuple):
            xy, via = p
            slots.append(mop_slot(f, slot_kind, xy, z, path=[(0.0, via, z)] if via is not None else None))
        else:
            slots.append(mop_slot(f, slot_kind, p, z))
    return TA.walked(pid, kind, by_id[plot], [f.world(q) for q in way], slots)


def pick(people, taken, want_wake, want_bed, female=None, ages=(24, 62), job=None, prev=None):
    """Somebody without work (the same one while the town is the same): who already has the job first, else the nearest
    to the hours the job needs."""
    if prev is not None:
        return prev
    fit = [p for p in people if p.get("occupation") == "unemployed" and not p.get("duties") and ages[0] <= p["age"] <= ages[1]
           and p["key"] not in taken and (female is None or bool(p.get("female")) == female)]
    if not fit:
        sys.exit("town_night: nobody free for the %s job" % job)

    def off(p):
        dw = abs((p.get("wake", 7.5) - want_wake + 12) % 24 - 12)
        db = abs((p.get("bed", 23.0) - want_bed + 12) % 24 - 12)
        return round(dw + db, 1), zlib.crc32(p["key"].encode())
    return sorted(fit, key=off)[0]


def give(p, occupation, tag, duties, sleep):
    p["occupation"] = occupation
    p["night_job"] = tag
    p["duties"] = [OD([("activity", a), ("place", pl), ("hours", list(h)), ("days", list(d))]) for a, pl, h, d in duties]
    p["bed"], p["wake"], p["chronotype"] = sleep
    p["spread"] = SPREAD


def build():
    out = {}
    plan = TA.load(TA.PLAN)
    things = {t["id"]: t for t in TA.load(TA.INTERIORS)["things"]}
    pd = TA.load(TA.PLACES)
    places = [p for p in pd["places"] if p["id"] not in PLACE_IDS]
    by_id = {p["id"]: p for p in places}
    new = [cafe(plan, things, by_id),
           simple("P-CLUB2-CLEAN", "P-CLUB2", "clean_round", "mop", CLUB_WAY, CLUB_MOP, plan, things, by_id),
           simple("P-STORE-CLEAN", "P-STORE", "clean_round", "mop", STORE_WAY, STORE_MOP, plan, things, by_id),
           simple("P-LIBRARY-CLEAN", "P-LIBRARY", "clean_round", "mop", LIB_WAY, LIB_MOP, plan, things, by_id),
           simple("P-STORE-WATCH", "P-STORE", "watch_round", "patrol", STORE_WAY, STORE_WATCH, plan, things, by_id)]
    assert [p["id"] for p in new] == PLACE_IDS
    # before town_activities.py's places (it keeps its own at the very end): either script may run last
    cut = next((i for i, p in enumerate(places) if p["id"] in TA.PLACE_IDS), len(places))
    pd["places"] = places[:cut] + new + places[cut:]
    out[TA.PLACES] = TA.dump(pd)

    fd = TA.load(TA.FOLK)
    people = fd["people"]
    # who has the jobs already ("night_job"): they keep them
    had = {p["night_job"]: p for p in people if p.get("night_job")}
    taken = set()
    a = pick(people, taken, SLEEP["a"][1], SLEEP["a"][0], female=True, job="cleaner", prev=had.get("cleaner_a"))
    taken.add(a["key"])
    b = pick(people, taken, SLEEP["b"][1], SLEEP["b"][0], job="cleaner", prev=had.get("cleaner_b"))
    taken.add(b["key"])
    w = pick(people, taken, SLEEP["w"][1], SLEEP["w"][0], female=False, ages=(35, 66), job="watchman", prev=had.get("watch"))
    for p in had.values():
        if p not in (a, b, w):          # a job that went (never, as the list stands): back to no work
            p.pop("night_job", None)
            p["occupation"] = "unemployed"
            p["duties"] = [d for d in p.get("duties", []) if d.get("activity") not in ACT_IDS]
    acts = ("mop_floor", "mop_floor_late")
    give(a, "cleaner", "cleaner_a", [(acts[i], pl, h, ALL_DAYS) for i, (pl, h) in enumerate(CLEANER_A)], SLEEP["a"])
    give(b, "cleaner", "cleaner_b", [(acts[i], e[0], e[1], e[2] if len(e) > 2 else ALL_DAYS) for i, e in enumerate(CLEANER_B)],
         SLEEP["b"])
    give(w, "guard", "watch", [("night_watch", WATCH[0], WATCH[1], ALL_DAYS)], SLEEP["w"])
    out[TA.FOLK] = TA.dump(fd)
    for lang in ("ru", "en", "ja"):
        sd = TA.load(TA.STRINGS % lang)
        for p, job in ((a, "cleaner"), (b, "cleaner"), (w, "watch")):
            sd["npc.%s.role" % p["key"]] = ROLE[job][lang][1 if p.get("female") else 0]
        out[TA.STRINGS % lang] = TA.dump(sd)

    # activities.json is written by hand, one activity a line: ours just before town_activities.py's rows (it keeps its
    # own after the last row), or after the last row when it has none
    text = open(TA.ACTIVITIES, encoding="utf-8").read()
    lines = [l for l in text.split("\n") if not any('"id": "%s"' % x in l for x in ACT_IDS)]
    theirs = [i for i, l in enumerate(lines) if any('"id": "%s"' % x in l for x in TA.ACT_IDS)]
    rows = ["    " + r + "," for r in ACTIVITIES_ROWS]
    if theirs:
        lines[theirs[0]:theirs[0]] = rows
    else:
        end = max(i for i, l in enumerate(lines) if l.strip() == "]")
        last = max(i for i in range(end) if lines[i].strip())
        if not lines[last].rstrip().endswith(","):
            lines[last] = lines[last].rstrip() + ","
        rows[-1] = rows[-1][:-1]
        lines[last + 1:end] = rows
    out[TA.ACTIVITIES] = "\n".join(lines)
    json.loads(out[TA.ACTIVITIES])
    return out, (a, b, w), new


def main():
    out, (a, b, w), new = build()
    stale = [p for p, t in out.items() if open(p, encoding="utf-8").read() != t]
    if "--check" in sys.argv:
        for p in stale:
            print("town_night: %s is not up to date (python3 tools/town/town_night.py)" % os.path.relpath(p, TA.ROOT))
        sys.exit(1 if stale else 0)
    for p in stale:
        with open(p, "w", encoding="utf-8") as f:
            f.write(out[p])
        print("wrote", os.path.relpath(p, TA.ROOT))
    print("town night: %s; cleaners %s, %s; the watchman %s" % (", ".join("%s %d" % (p["id"], len(p["slots"])) for p in new),
                                                                a["key"], b["key"], w["key"]))


if __name__ == "__main__":
    main()
