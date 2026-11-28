#!/usr/bin/env python3
"""The town comes to the dances at the hall «Своды» (docs/dances.md): on Friday and Saturday evenings the young ones dance
in pairs on the dance room's tile floor and the older ones watch from the little tables and the lobby's benches; everybody
is gone before the watchman locks up at 23:00. The twelve partners the hero may dance with (tools/city/content/x_dances.json
regulars) like it more than most, so they are there.

Like tools/town/town_activities.py this adds to the town generator's output and is idempotent (tools/town/town_life.py
runs it after itself; after tools/city/interiors.py moved the hall's things, run it again):
    python3 tools/town/town_dances.py            add or refresh
    python3 tools/town/town_dances.py --check    exit 1 when the files are not as this script makes them

What it writes:
  Content/PeopleData/town_places.json   the place P-HALL-DANCE (kind dance_floor): pairs of standing slots on the floor
                                        (each pair faces each other, clear of the hero's own dance things), the seats round
                                        the little tables and the lobby's two benches; in by the main door, through the
                                        doorway in the main hall's east wall
  Content/PeopleData/activities.json    dance_evening and dance_watch (one a line)
  Content/PeopleData/townsfolk.json     the partners' like for dance_evening
"""
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import town_activities as TA  # noqa: E402

OD = TA.OD
ROOT = TA.ROOT
PLACE_IDS = ["P-HALL-DANCE"]

# Friday and Saturday (bit 0 Monday): people set off between seven and twenty past nine and stay 40-90 minutes, so the last
# leave before the watchman's eleven. Not on a holiday's day off: the dancing is on the square then (the festivals). Only
# while hall_dances holds (ULyudStreetLife: all the year but June to August, when the dances are on the square:
# tools/town/town_promenade.py, docs/promenade.md).
ACTIVITIES_ROWS = [
    '{"id": "dance_evening", "places": ["dance_floor"], "slots": ["dance_spot"], "pose": "stand", "minutes": [40, 90], "hours": [19, 21.35], "days": [4, 5], "ages": [15, 45], "meets": {"fun": 0.95, "social": 0.8}, "costs": {"tired": 0.06}, "base": 0.6, "trait": "social", "when": "hall_dances"}',
    '{"id": "dance_watch", "places": ["dance_floor"], "slots": ["watch_seat"], "pose": "sit", "minutes": [30, 80], "hours": [19, 21.6], "days": [4, 5], "ages": [55, 200], "meets": {"social": 0.75, "fun": 0.5}, "base": 0.7, "trait": "social", "when": "hall_dances"}',
]
ACT_IDS = [json.loads(r)["id"] for r in ACTIVITIES_ROWS]

# the hall's frame (blender/city/buildings/shell_chapel_hall.py, metres): in at the main door, along the main hall's lobby,
# through the doorway (y 9..12) in the wall between the main hall and the dance room (x 2.5)
WAY = [(-3.75, 4.4, 0.0), (-3.75, 6.7, 0.0), (1.75, 6.9, 0.0), (1.75, 10.5, 0.0), (3.7, 10.6, 0.0), (6.4, 11.4, 0.0)]
# the middles of the dancing pairs on the tile floor (x 4.8..9.4, y 12.3..17.2); each pair 0.55 m apart along PAIR_AXIS
PAIRS = [(5.0, 13.6), (9.2, 12.5), (9.3, 13.9), (8.0, 15.9), (5.4, 15.2), (6.6, 17.1), (8.4, 17.2), (9.3, 15.8), (7.0, 12.4), (7.9, 14.4)]
PAIR_HALF = 0.28
# the hero's own things on the floor: no pair within this of them (m)
HERO_THINGS = ["dance_1", "dance_2", "dance_3", "dance_4", "dance_floor_centre", "dance_steps"]
HERO_CLEAR = 0.75
# where the old ones sit: the export's seat spots round the little tables (the first seat of each is the hero's seat thing
# c_table_<n>, held while somebody sits there), and the lobby's benches by the door (things)
WATCH_SPOTS = ["c_seat_1_2", "c_seat_1_3", "c_seat_2_2", "c_seat_2_3", "c_seat_3_2", "c_seat_3_3", "c_seat_4_2", "c_seat_4_3"]
WATCH_THINGS = ["c_table_1", "c_table_2", "c_table_3", "c_table_4", "lobby_bench_1", "lobby_bench_2"]
LIKE = 2.2


def partners():
    d = json.load(open(os.path.join(ROOT, "tools", "city", "content", "x_dances.json"), encoding="utf-8"))
    out = []
    names = {l: json.load(open(TA.STRINGS % l, encoding="utf-8")) for l in ("ru",)}
    strings = json.load(open(os.path.join(ROOT, "tools", "city", "content", "strings", "x_dances.json"), encoding="utf-8"))
    by_name = {v: k[len("npc."):-len(".name")] for k, v in names["ru"].items() if k.startswith("npc.") and k.endswith(".name")}
    for rid, r in d["regulars"].items():
        key = by_name.get(strings[r["name"]]["ru"])
        if not key:
            sys.exit("town_dances: partner %s (%s) is not a townsperson" % (rid, strings[r["name"]]["ru"]))
        out.append(key)
    return out


def dance_floor(plan, things, by_id):
    f = TA.Frame("P-HALL", plan, things)
    loc = {t["id"]: t["pos"] for t in f.export["things"]}
    slots = []
    for cx, cy in PAIRS:
        for side in (-1, 1):
            x, y = cx + side * PAIR_HALF, cy
            near = min(math.hypot(x - loc[h][0], y - loc[h][1]) for h in HERO_THINGS)
            if near < HERO_CLEAR:
                sys.exit("town_dances: the pair at %.1f, %.1f stands on the hero's dance things (%.2f m)" % (cx, cy, near))
            # the export's yaws (as the seats facing each other across a little table: 90 on the low x side, 270 on the
            # high), turned into the world's by the frame: the two face each other
            slots.append(OD([("kind", "dance_spot"), ("pos", f.world((x, y, 0.0))), ("yaw", f.wyaw(270.0 if side > 0 else 90.0))]))
    for sid in WATCH_SPOTS:
        s = f.spot(sid)
        x, y, z = s["pos"]
        slots.append(OD([("kind", "watch_seat"), ("pos", f.world((x, y, z))), ("yaw", f.wyaw(s.get("yaw", 0.0))), ("height", 0.45)]))
    for tid in WATCH_THINGS:
        slots.append(TA.seat_slot("watch_seat", TA.need(things, "P-HALL/" + tid)))
    return TA.walked("P-HALL-DANCE", "dance_floor", by_id["P-HALL"], [f.world(p) for p in WAY], slots)


def build():
    out = {}
    plan = TA.load(TA.PLAN)
    things = {t["id"]: t for t in TA.load(TA.INTERIORS)["things"]}
    pd = TA.load(TA.PLACES)
    by_id = {p["id"]: p for p in pd["places"] if p["id"] not in PLACE_IDS}
    new = [dance_floor(plan, things, by_id)]
    fresh = {p["id"]: p for p in new}
    had = [p["id"] for p in pd["places"]]
    if all(i in had for i in PLACE_IDS):
        pd["places"] = [fresh.get(p["id"], p) for p in pd["places"]]
    else:
        # at the very end, after town_activities.py's places: the other add-ons put theirs before those and keep them where
        # they stand, so any of them may run last
        pd["places"] = [p for p in pd["places"] if p["id"] not in PLACE_IDS] + new
    out[TA.PLACES] = TA.dump(pd)

    fd = TA.load(TA.FOLK)
    keys = set(partners())
    for p in fd["people"]:
        likes = p.get("likes")
        if likes is not None:
            likes.pop("dance_evening", None)
        if p["key"] in keys:
            # first among the likes: town_activities.py takes its own out and puts them back at the end, so either may run last
            p["likes"] = OD([("dance_evening", LIKE)] + list((likes or {}).items()))
    out[TA.FOLK] = TA.dump(fd)

    text = open(TA.ACTIVITIES, encoding="utf-8").read()
    lines = text.split("\n")
    have = {a: next((i for i, l in enumerate(lines) if '"id": "%s"' % a in l), -1) for a in ACT_IDS}
    if all(i >= 0 for i in have.values()):
        for a, r in zip(ACT_IDS, ACTIVITIES_ROWS):
            comma = "," if lines[have[a]].rstrip().endswith(",") else ""
            lines[have[a]] = "    " + r + comma
    else:
        # after the last row (the other add-ons keep theirs where they stand)
        lines = [l for l in lines if not any('"id": "%s"' % x in l for x in ACT_IDS)]
        rows = ["    " + r + "," for r in ACTIVITIES_ROWS]
        end = max(i for i, l in enumerate(lines) if l.strip() == "]")
        last = max(i for i in range(end) if lines[i].strip())
        if not lines[last].rstrip().endswith(","):
            lines[last] = lines[last].rstrip() + ","
        rows[-1] = rows[-1][:-1]
        lines[last + 1:end] = rows
    out[TA.ACTIVITIES] = "\n".join(lines)
    json.loads(out[TA.ACTIVITIES])
    return out, new, keys


def main():
    out, new, keys = build()
    stale = [p for p, t in out.items() if open(p, encoding="utf-8").read() != t]
    if "--check" in sys.argv:
        for p in stale:
            print("town_dances: %s is not up to date (python3 tools/town/town_dances.py)" % os.path.relpath(p, ROOT))
        sys.exit(1 if stale else 0)
    for p in stale:
        with open(p, "w", encoding="utf-8") as f:
            f.write(out[p])
        print("wrote", os.path.relpath(p, ROOT))
    print("town_dances: %s; %d partners like dancing" % (", ".join("%s %d slots" % (p["id"], len(p["slots"])) for p in new), len(keys)))


if __name__ == "__main__":
    main()
