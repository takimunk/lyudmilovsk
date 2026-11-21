#!/usr/bin/env python3
"""The sanatorium «Корона»'s four guests as townsfolk (docs/town_life.md "Pinned guests"): Pyotr Ilyich Gromov and Sonya at
the window table, Boris Tamm and Nikolai Sazonov at the chess table, the people of the table stories
(tools/city/content/strings/x_p_sanatorium_course.json, city.san.tg.* / tf.*). Each lives in a room of the sanatorium and
three times a day goes to the dining room and eats at his or her own chair (the chairs of the acts san_seat_<name> in
interiors.json), the rest of the day free like anybody in town.

The town generator (tools/town/town_life.py) does not make them: this script adds them to its output and is idempotent, so
it runs again after every regeneration:
    python3 tools/town/sanatorium_guests.py            add or refresh them
    python3 tools/town/sanatorium_guests.py --check    exit 1 when the files are not as this script makes them

What it writes: Content/PeopleData/townsfolk.json (the four people), town_places.json (the walked dining room
P-SANATORIUM-DINING: in by the sanatorium's door, one place a guest at his or her chair; the inhalation room
P-SANATORIUM-INHALE: two of its three chairs, the first left for the hero), activities.json (eat_sanatorium_<meal>, the meals
as duties; cure_sanatorium_inhale, the morning's inhalation as a duty: docs/sanatorium_procedures.md, the guest at the same
procedure talks to the hero), Content/Localization/town/strings_<lang>.json (names and "guest").
"""
import collections
import json
import math
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
FOLK = os.path.join(CONTENT, "PeopleData", "townsfolk.json")
PLACES = os.path.join(CONTENT, "PeopleData", "town_places.json")
ACTIVITIES = os.path.join(CONTENT, "PeopleData", "activities.json")
INTERIORS = os.path.join(CONTENT, "City", "interiors.json")
STRINGS = os.path.join(CONTENT, "Localization", "town", "strings_%s.json")
OD = collections.OrderedDict

PLACE_ID = "P-SANATORIUM-DINING"
PLACE_KIND = "sanatorium_dining"
# the regime's meals (city.san.regime.*): breakfast, lunch, supper; they set off 20 minutes early (FWorld::DutyNow)
MEALS = (("breakfast", [8.5, 9.25]), ("lunch", [13.5, 14.25]), ("supper", [18.5, 19.25]))
EVERY_DAY = [0, 1, 2, 3, 4, 5, 6]
# a sitting meal rests a little ("tired"), so a tired old guest finishes it before the nap instead of leaving mid-meal
# (FWorld::Interrupt); one activity a meal: the life finds a person's duty by its activity (FWorld::DutyFor), so three duties of one activity
# would all read as the first (breakfast) and the guests would sit on past its end
ACTIVITY_LINES = ['    {"id": "eat_sanatorium_%s", "places": ["%s"], "slots": ["staff"], "pose": "eat", "minutes": [45, 45], '
                  '"meets": {"hunger": 2.6, "social": 0.4, "tired": 0.15}, "free": 0, "staff": 1},' % (m, PLACE_KIND) for m, _h in MEALS]

# the inhalation room of the hydrotherapy wing (docs/sanatorium_procedures.md): the guests take turns on two of its chairs (the
# first, by the door, stays free for the hero), half an hour each, Monday to Saturday; the n-th guest at INHALE_HOURS[n // 2]
INHALE_PLACE = "P-SANATORIUM-INHALE"
INHALE_KIND = "sanatorium_inhale"
INHALE_CHAIRS = ("P-SANATORIUM/inhale_2", "P-SANATORIUM/inhale_3")
INHALE_HOURS = (10.0, 11.0, 12.0, 15.0, 16.0, 17.0)
INHALE_DAYS = [0, 1, 2, 3, 4, 5]
# the way in, in the building's ring (export frame: the ring's centre at (0, 26)): along the ground hall from the lobby to the
# treatment room, through the wing's door at -45 degrees, into the inhalation room (angle in degrees, radius in metres)
INHALE_WAY = ((-80.0, 15.0), (-62.0, 15.0), (-50.0, 15.8), (-45.0, 16.95), (-41.0, 16.4))
INHALE_LINE = ('    {"id": "cure_sanatorium_inhale", "places": ["%s"], "slots": ["staff"], "pose": "sit", "minutes": [30, 30], '
               '"meets": {"tired": 0.1, "social": 0.25}, "free": 0, "staff": 1},' % INHALE_KIND)

GUESTS = [
    {"key": "t_gromov_pyotr", "thing": "P-SANATORIUM/dining_table_1", "home": "P-SANATORIUM/1-01", "mesh": "Korneev_ivan",
     "age": 78, "female": 0, "wake": 6.5, "bed": 22.25, "walk_speed": 0.95, "friends": ["t_mironova_sonya"],
     "temper": {"social": 0.55, "active": 0.4, "studious": 0.7, "gamer": 0.3, "nature": 0.6, "sun": 0.5},
     "likes": {"sit_bench": 0.9, "sea_air": 1.0, "watch_sunrise": 0.8, "read_library": 0.5, "san_cure": 0.9, "read_paper": 0.6},
     "name": ("Пётр Громов", "Pyotr Gromov", "ピョートル・グロモフ")},
    {"key": "t_mironova_sonya", "thing": "P-SANATORIUM/dining_1_c", "home": "P-SANATORIUM/1-02", "mesh": "Frolova_masha",
     "age": 16, "female": 1, "wake": 7.5, "bed": 22.75, "walk_speed": 1.3, "friends": ["t_gromov_pyotr"],
     "temper": {"social": 0.45, "active": 0.6, "studious": 0.8, "gamer": 0.2, "nature": 0.5, "sun": 0.6},
     "likes": {"stroll": 0.9, "read_library": 1.0, "coffee": 0.5, "swim_look": 0.6, "cinema_evening": 0.8},
     "name": ("Соня Миронова", "Sonya Mironova", "ソーニャ・ミロノワ")},
    {"key": "t_tamm_boris", "thing": "P-SANATORIUM/dining_table_2", "home": "P-SANATORIUM/1-03", "mesh": "Sorokin_oleg",
     "age": 81, "female": 0, "wake": 6.75, "bed": 22.5, "walk_speed": 0.9, "friends": ["t_sazonov_nikolai"],
     "temper": {"social": 0.7, "active": 0.3, "studious": 0.5, "gamer": 0.9, "nature": 0.5, "sun": 0.4},
     "likes": {"play_club": 1.0, "sit_bench": 0.8, "fish_shore": 0.7, "chat": 0.6, "dominoes": 1.1, "san_cure": 0.6},
     "name": ("Борис Тамм", "Boris Tamm", "ボリス・タム")},
    {"key": "t_sazonov_nikolai", "thing": "P-SANATORIUM/dining_2_c", "home": "P-SANATORIUM/1-04", "mesh": "Zotov_lev", "age": 80, "female": 0, "wake": 7.0, "bed": 22.5, "walk_speed": 0.9,
     "friends": ["t_tamm_boris"],
     "temper": {"social": 0.6, "active": 0.35, "studious": 0.9, "gamer": 0.85, "nature": 0.4, "sun": 0.4},
     "likes": {"play_club": 1.0, "read_library": 0.8, "sit_bench": 0.6, "chat": 0.5, "dominoes": 1.1, "san_cure": 0.6},
     "name": ("Николай Сазонов", "Nikolai Sazonov", "ニコライ・サゾーノフ")},
]
ROLE = {"ru": ("отдыхающий в санатории", "отдыхающая в санатории"), "en": ("sanatorium guest", "sanatorium guest"),
        "ja": ("療養所の滞在客", "療養所の滞在客")}


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f, object_pairs_hook=OD)


def dump(d):
    return json.dumps(d, ensure_ascii=False, indent=1) + "\n"


def person(g):
    return OD([
        ("key", g["key"]), ("town", 1), ("mesh", g["mesh"]), ("age", g["age"]), ("female", g["female"]),
        ("household", "t_" + g["home"].lower().replace("/", "_")), ("home", g["home"]), ("chronotype", "lark"),
        ("wake", g["wake"]), ("bed", g["bed"]), ("spread", 0.25),
        ("duties", [OD([("activity", "eat_sanatorium_" + m), ("place", PLACE_ID), ("role", "guest_" + g["key"][2:]),
                        ("hours", list(h)), ("days", EVERY_DAY)]) for m, h in MEALS] + [inhale_duty(GUESTS.index(g))]),
        ("friends", list(g["friends"])), ("temper", OD(g["temper"])), ("walk_speed", g["walk_speed"]),
        ("occupation", "guest"), ("likes", OD(g["likes"])),
    ])


def dining_place(things, door, yaw):
    """The dining room: in by the sanatorium's door, 1.6 m into the lobby (one open hall with the tables), each guest's
    place at his or her chair (the city thing's position and yaw; the seat 0.45 m over the floor). "thing": while the guest
    sits there the chair is held (UPeopleLifeSubsystem::UpdateThingHolds), so the hero can't sit on him."""
    slots = []
    for g in GUESTS:
        t = things[g["thing"]]
        # the thing's own point is on the seat's top; the slot is the floor under it (+ height), as everywhere in the life
        h = float((t.get("props") or {}).get("height", 0.45))
        x, y, z = (round(v / 100.0, 2) for v in t["ue"])
        slots.append(OD([("kind", "staff"), ("tag", "guest_" + g["key"][2:]), ("pos", [x, y, round(z - h, 2)]),
                         ("yaw", round(t["yaw"] % 360.0, 1)), ("height", h), ("thing", g["thing"])]))
    floor = slots[0]["pos"][2]
    inside = [round(door[0] - 1.6, 2), round(door[1] + 0.6, 2), floor]
    return OD([("id", PLACE_ID), ("kind", PLACE_KIND), ("door", list(door)), ("yaw", yaw), ("island", 0), ("slots", slots),
               ("roles", []), ("building", "P-SANATORIUM"), ("path", [inside])])


def inhale_duty(n):
    """The n-th guest's turn in the inhalation room: a chair (by n), an hour (by n // 2)."""
    h = INHALE_HOURS[(n // len(INHALE_CHAIRS)) % len(INHALE_HOURS)]
    return OD([("activity", "cure_sanatorium_inhale"), ("place", INHALE_PLACE), ("role", "inhale_%d" % (n % len(INHALE_CHAIRS) + 1)),
               ("hours", [h, h + 0.5]), ("days", INHALE_DAYS)])


def inhale_place(things, door, yaw):
    """The inhalation room: in by the sanatorium's door, along the hall and through the wing's door (the building's frame from
    its export and interiors.json, as tools/town/town_activities.py finds it), a slot at each of the guests' chairs."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import town_activities
    frame = town_activities.Frame("P-SANATORIUM", load(town_activities.PLAN), things)
    slots = []
    for i, tid in enumerate(INHALE_CHAIRS):
        t = things[tid]
        h = float((t.get("props") or {}).get("height", 0.45))
        x, y, z = (round(v / 100.0, 2) for v in t["ue"])
        slots.append(OD([("kind", "staff"), ("tag", "inhale_%d" % (i + 1)), ("pos", [x, y, round(z - h, 2)]),
                         ("yaw", round(t["yaw"] % 360.0, 1)), ("height", h), ("thing", tid)]))
    floor = slots[0]["pos"][2]
    way = []
    for ang, r in INHALE_WAY:
        a = math.radians(ang)
        w = frame.world((r * math.cos(a), 26.0 + r * math.sin(a), 0.0))
        way.append([round(w[0], 2), round(w[1], 2), floor])
    return OD([("id", INHALE_PLACE), ("kind", INHALE_KIND), ("door", list(door)), ("yaw", yaw), ("island", 0), ("slots", slots),
               ("roles", []), ("building", "P-SANATORIUM"), ("path", way)])


def build():
    """{path: new text} for every file this script owns a part of."""
    out = {}
    things = {t["id"]: t for t in load(INTERIORS)["things"] if t["plot"] == "P-SANATORIUM"}
    for tid in [g["thing"] for g in GUESTS] + list(INHALE_CHAIRS):
        if tid not in things:
            sys.exit("sanatorium_guests: %s is not a thing in interiors.json (python3 tools/city/interiors.py)" % tid)
    for g in GUESTS:
        if g["thing"] not in things:
            sys.exit("sanatorium_guests: %s is not a thing in interiors.json (python3 tools/city/interiors.py)" % g["thing"])
    # the places: the dining room after the sanatorium itself
    pd = load(PLACES)
    places = [p for p in pd["places"] if p["id"] not in (PLACE_ID, INHALE_PLACE)]
    at = next(i for i, p in enumerate(places) if p["id"] == "P-SANATORIUM")
    san = places[at]
    places.insert(at + 1, dining_place(things, san["door"], san["yaw"]))
    places.insert(at + 2, inhale_place(things, san["door"], san["yaw"]))
    for g in GUESTS:
        flat = next((p for p in places if p["id"] == g["home"]), None)
        if not flat or flat["kind"] != "flat":
            sys.exit("sanatorium_guests: home %s is not a flat in town_places.json" % g["home"])
    pd["places"] = places
    out[PLACES] = dump(pd)
    # the people: at the end, after the generated town
    fd = load(FOLK)
    keys = {g["key"] for g in GUESTS}
    taken = {p["home"] for p in fd["people"] if p["key"] not in keys}
    for g in GUESTS:
        if g["home"] in taken:
            sys.exit("sanatorium_guests: %s already has a household (pick a free room)" % g["home"])
    fd["people"] = [p for p in fd["people"] if p["key"] not in keys] + [person(g) for g in GUESTS]
    out[FOLK] = dump(fd)
    # names and the job in three languages, after the generated town's
    for i, lang in enumerate(("ru", "en", "ja")):
        sd = load(STRINGS % lang)
        for k in [k for k in sd if re.match(r"npc\.(%s)\.(name|role)$" % "|".join(keys), k)]:
            del sd[k]
        for g in GUESTS:
            sd["npc.%s.name" % g["key"]] = g["name"][i]
            sd["npc.%s.role" % g["key"]] = ROLE[lang][g["female"]]
        out[STRINGS % lang] = dump(sd)
    # the meal as a duty (activities.json is written by hand, one activity a line: after work_town)
    text = open(ACTIVITIES, encoding="utf-8").read()
    lines = [l for l in text.split("\n") if '"id": "eat_sanatorium' not in l and '"id": "cure_sanatorium' not in l]
    at = next(i for i, l in enumerate(lines) if '"id": "work_town"' in l)
    lines[at + 1:at + 1] = ACTIVITY_LINES + [INHALE_LINE]
    out[ACTIVITIES] = "\n".join(lines)
    json.loads(out[ACTIVITIES])
    return out


def main():
    out = build()
    stale = [p for p, t in out.items() if open(p, encoding="utf-8").read() != t]
    if "--check" in sys.argv:
        for p in stale:
            print("sanatorium_guests: %s is not up to date (python3 tools/town/sanatorium_guests.py)" % os.path.relpath(p, ROOT))
        sys.exit(1 if stale else 0)
    for p in stale:
        with open(p, "w", encoding="utf-8") as f:
            f.write(out[p])
        print("wrote", os.path.relpath(p, ROOT))
    print("sanatorium guests: %d people, %d meals a day at %s, inhalations at %s" % (len(GUESTS), len(MEALS), PLACE_ID, INHALE_PLACE))


if __name__ == "__main__":
    main()
