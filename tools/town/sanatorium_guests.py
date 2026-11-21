#!/usr/bin/env python3
"""The sanatorium «Корона»'s guests as townsfolk (docs/town_life.md "The sanatorium's guests"): a dozen people who come
for a fortnight and go, two by two. Every Saturday the pair of one dining table leaves by the morning train (they wait on
the platform with their suitcases) and the next pair of that table comes by the next one and walks to the sanatorium, so
four are there at any time. The first two pairs are the people of the hero's table stories
(tools/city/content/strings/x_p_sanatorium_course.json, city.san.tg.* / tf.*): Pyotr Ilyich Gromov and Sonya at the window
table, Boris Tamm and Nikolai Sazonov at the chess table. Each lives in a room of the sanatorium, three times a day eats at
his or her table's chair (the chairs of the acts san_seat_<name> in interiors.json), takes the waters, and after lunch
goes out by the regime: the promenade and the pier, the kiosk for postcards.

Who is here when is the person's "stay" (PeopleSim FPerson::StayCycle): a cycle of six weeks, the week they come in, the
hours of the trains. The week is the calendar's (FWorld::Week; Monday 1 January 2001 began week 0), and WEEK_OF is set so
that on the game's first day (Tuesday 20 October 2026) Tamm and Sazonov are in their second week and Gromov and Sonya in
their first.

The town generator (tools/town/town_life.py) does not make them: this script adds them to its output and is idempotent, so
it runs again after every regeneration:
    python3 tools/town/sanatorium_guests.py            add or refresh them
    python3 tools/town/sanatorium_guests.py --check    exit 1 when the files are not as this script makes them

What it writes: Content/PeopleData/townsfolk.json (the twelve people), town_places.json (the walked dining room
P-SANATORIUM-DINING: in by the sanatorium's door, a place a chair; the guests' end of the railway platform S-RAIL-GUESTS; the
inhalation room P-SANATORIUM-INHALE: two of its three chairs, the first left for the hero), activities.json
(eat_sanatorium_<meal>, san_arrive, san_depart, stay_gone, san_promenade, san_postcards; cure_sanatorium_inhale, the morning's
inhalation as a duty: docs/sanatorium_procedures.md, the guest at the same procedure talks to the hero),
Content/Localization/town/strings_<lang>.json (names and "guest").
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

# the extra activities: the day they come (at the reception with the suitcase), the day they go (on the platform), gone
# (away, hidden: FWorld::GoneAct), and the regime's outings after the waters and after lunch, which only the guests like
# enough to do (base -2)
EXTRA_LINES = [
    '    {"id": "san_arrive", "places": ["hotel"], "slots": ["visit"], "pose": "stand", "minutes": [20, 20], "meets": {"social": 0.3}, "free": 0},',
    '    {"id": "san_depart", "places": ["guest_platform"], "slots": ["stand"], "pose": "stand", "minutes": [30, 30], "meets": {"social": 0.2}, "free": 0, "rain": "ok"},',
    '    {"id": "stay_gone", "places": ["guest_platform"], "slots": ["stand"], "pose": "hidden", "minutes": [60, 60], "free": 0, "away": 1},',
    '    {"id": "san_promenade", "places": ["promenade", "pier"], "pose": "walk", "minutes": [35, 70], "hours": [14.4, 16.6], "daylight": 1, "temp": [-5, 40], '
    '"meets": {"nature": 0.9, "sun": 0.5, "social": 0.2}, "costs": {"hunger": 0.04, "tired": 0.03}, "base": -2.0, "trait": "nature", "wander": 1, "wander_m": [10, 120], "wander_pause": [40, 120]},',
    '    {"id": "san_postcards", "places": ["kiosk"], "slots": ["visit"], "pose": "stand", "minutes": [6, 12], "hours": [12.2, 13.1], "days": [1, 4, 6], '
    '"meets": {"social": 0.5, "fun": 0.3}, "base": -2.0, "trait": "social"},',
]
EXTRA_IDS = ("san_arrive", "san_depart", "stay_gone", "san_promenade", "san_postcards")

# the stay: the Saturday train in, a fortnight, the Saturday train out
CYCLE_WEEKS = 6
STAY_DAYS = 14
ARRIVE = 11.0           # off the train (the hour they are back in the world, on the platform)
LEAVE = 10.75           # the train out: gone from the platform
ARRIVE_DUTY = [11.0, 12.4]      # the walk to «Корона» and the reception
DEPART_DUTY = [9.45, 10.75]     # after breakfast: the walk to the halt, the wait on the platform
# the calendar week (mod CYCLE_WEEKS) a pair comes in: the game begins in week 2 of the cycle
PAIR_WEEK = {"tamm": 0, "gromov": 1, "krylova": 2, "levin": 3, "beketov": 4, "kovalchuk": 5}
# the two tables: their chairs (a, b) and their rooms
TABLES = {
    1: {"chairs": ("P-SANATORIUM/dining_table_1", "P-SANATORIUM/dining_1_c"), "rooms": ("P-SANATORIUM/1-01", "P-SANATORIUM/1-02")},
    2: {"chairs": ("P-SANATORIUM/dining_table_2", "P-SANATORIUM/dining_2_c"), "rooms": ("P-SANATORIUM/1-03", "P-SANATORIUM/1-04")},
}
PLATFORM_ID = "S-RAIL-GUESTS"
PLATFORM_KIND = "guest_platform"
PEOPLE_PLACES = os.path.join(CONTENT, "PeopleData", "people_places.json")

OUT = {"san_promenade": 4.2, "san_postcards": 3.4}      # the regime's outings: every guest


def G(key, pair, seat, table, mesh, age, female, wake, bed, speed, friend, temper, likes, name, town):
    t = dict(zip(("social", "active", "studious", "gamer", "nature", "sun"), temper))
    likes = dict(likes)
    likes.update(OUT)
    return {"key": key, "pair": pair, "seat": seat, "table": table, "mesh": mesh, "age": age, "female": female, "wake": wake, "bed": bed,
            "walk_speed": speed, "friends": [friend], "temper": t, "likes": likes, "name": name, "town": town}


# key, the pair, the chair (0 a, 1 b), the table, body, age, woman, wake, bed, walk speed, the table mate, temper, likes,
# name (ru, en, ja), where they are from (for the lines: tools/barks/lines/16_sanatorium_guests.txt, job = guest_<name>)
GUESTS = [
    G("t_gromov_pyotr", "gromov", 0, 1, "Korneev_ivan", 78, 0, 6.5, 22.25, 0.95, "t_mironova_sonya", (0.55, 0.4, 0.7, 0.3, 0.6, 0.5),
      {"sit_bench": 0.9, "sea_air": 1.0, "watch_sunrise": 0.8, "read_library": 0.5, "san_cure": 0.9, "read_paper": 0.6},
      ("Пётр Громов", "Pyotr Gromov", "ピョートル・グロモフ"), "Звёздный"),
    G("t_mironova_sonya", "gromov", 1, 1, "Frolova_masha", 16, 1, 7.5, 22.75, 1.3, "t_gromov_pyotr", (0.45, 0.6, 0.8, 0.2, 0.5, 0.6),
      {"stroll": 0.9, "read_library": 1.0, "coffee": 0.5, "swim_look": 0.6, "cinema_evening": 0.8},
      ("Соня Миронова", "Sonya Mironova", "ソーニャ・ミロノワ"), "Тверь"),
    G("t_tamm_boris", "tamm", 0, 2, "Sorokin_oleg", 81, 0, 6.75, 22.5, 0.9, "t_sazonov_nikolai", (0.7, 0.3, 0.5, 0.9, 0.5, 0.4),
      {"play_club": 1.0, "sit_bench": 0.8, "fish_shore": 0.7, "chat": 0.6, "dominoes": 1.1, "san_cure": 0.6},
      ("Борис Тамм", "Boris Tamm", "ボリス・タム"), "Таллин"),
    G("t_sazonov_nikolai", "tamm", 1, 2, "Zotov_lev", 80, 0, 7.0, 22.5, 0.9, "t_tamm_boris", (0.6, 0.35, 0.9, 0.85, 0.4, 0.4),
      {"play_club": 1.0, "read_library": 0.8, "sit_bench": 0.6, "chat": 0.5, "dominoes": 1.1, "san_cure": 0.6},
      ("Николай Сазонов", "Nikolai Sazonov", "ニコライ・サゾーノフ"), "Псков"),
    G("t_krylova_zinaida", "krylova", 0, 2, "Guseva_valentina", 71, 1, 6.5, 22.0, 0.95, "t_abdullina_raisa", (0.8, 0.35, 0.5, 0.3, 0.6, 0.6),
      {"chat": 0.9, "sit_bench": 0.8, "san_cure": 1.0, "dominoes": 0.6, "cinema_day": 0.6, "sea_air": 0.6},
      ("Зинаида Крылова", "Zinaida Krylova", "ジナイーダ・クルィロワ"), "Вологда"),
    G("t_abdullina_raisa", "krylova", 1, 2, "Zotova_galina", 68, 1, 6.75, 22.25, 1.0, "t_krylova_zinaida", (0.6, 0.45, 0.6, 0.2, 0.7, 0.6),
      {"stroll": 0.8, "shop_town": 0.7, "san_cure": 0.9, "sit_bench": 0.6, "read_paper": 0.5, "cinema_evening": 0.5},
      ("Раиса Абдуллина", "Raisa Abdullina", "ライーサ・アブドゥリナ"), "Казань"),
    G("t_levin_arkady", "levin", 0, 1, "Owner", 74, 0, 6.0, 22.0, 0.95, "t_levina_tamara", (0.5, 0.5, 0.6, 0.4, 0.8, 0.6),
      {"sea_air": 1.1, "watch_sunrise": 1.0, "fish_shore": 0.6, "san_cure": 0.8, "sit_bench": 0.7, "read_paper": 0.7},
      ("Аркадий Левин", "Arkady Levin", "アルカージー・レーヴィン"), "Мурманск"),
    G("t_levina_tamara", "levin", 1, 1, "Korneeva_nina", 70, 1, 6.75, 22.25, 0.95, "t_levin_arkady", (0.65, 0.4, 0.8, 0.2, 0.6, 0.5),
      {"read_library": 0.8, "culture_evening": 0.9, "play_piano": 1.9, "san_cure": 0.8, "stroll": 0.7, "cinema_evening": 0.6},
      ("Тамара Левина", "Tamara Levina", "タマーラ・レーヴィナ"), "Мурманск"),
    G("t_beketov_yuri", "beketov", 0, 2, "Zotov_lev", 66, 0, 6.25, 22.5, 1.05, "t_matsumoto_haruo", (0.45, 0.6, 0.7, 0.5, 0.7, 0.4),
      {"walk": 0.8, "lookout_view": 0.9, "san_cure": 0.7, "play_club": 0.6, "read_paper": 0.8, "sea_air": 0.7},
      ("Юрий Бекетов", "Yuri Beketov", "ユーリー・ベケトフ"), "Норильск"),
    G("t_matsumoto_haruo", "beketov", 1, 2, "Sorokin_oleg", 69, 0, 6.0, 21.75, 0.95, "t_beketov_yuri", (0.4, 0.4, 0.8, 0.5, 0.7, 0.6),
      {"watch_sunrise": 1.0, "sit_bench": 0.8, "san_cure": 0.9, "read_library": 0.6, "play_club": 0.6, "sea_air": 0.8},
      ("Харуо Мацумото", "Haruo Matsumoto", "松本 春雄"), "Отару"),
    G("t_kovalchuk_lidia", "kovalchuk", 0, 1, "Orlova_tanya", 60, 1, 6.25, 22.25, 1.1, "t_kovalchuk_asya", (0.75, 0.55, 0.4, 0.3, 0.6, 0.6),
      {"chat": 0.9, "coffee": 0.7, "san_cure": 0.9, "sit_bench": 0.6, "shop_town": 0.6, "cinema_day": 0.7},
      ("Лидия Ковальчук", "Lidia Kovalchuk", "リディヤ・コワリチュク"), "Иркутск"),
    G("t_kovalchuk_asya", "kovalchuk", 1, 1, "Kid_girl", 12, 1, 7.5, 21.5, 1.35, "t_kovalchuk_lidia", (0.6, 0.8, 0.5, 0.5, 0.7, 0.7),
      {"skip_stones": 1.2, "fly_kite": 0.9, "playground": 0.7, "swim_look": 0.7, "cinema_day": 0.9, "read_library": 0.4},
      ("Ася Ковальчук", "Asya Kovalchuk", "アーシャ・コワリチュク"), "Иркутск"),
]
ROLE = {"ru": ("отдыхающий в санатории", "отдыхающая в санатории"), "en": ("sanatorium guest", "sanatorium guest"),
        "ja": ("療養所の滞在客", "療養所の滞在客")}


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f, object_pairs_hook=OD)


def dump(d):
    return json.dumps(d, ensure_ascii=False, indent=1) + "\n"


def short(g):
    return g["key"][2:].split("_")[0]


def seat_tag(g):
    return "guest_seat_%d%s" % (g["table"], "ab"[g["seat"]])


def person(g):
    home = TABLES[g["table"]]["rooms"][g["seat"]]
    me = "guest_" + short(g)        # their own job for the lines (job = guest_gromov)
    duties = [OD([("activity", "san_depart"), ("place", PLATFORM_ID), ("role", me), ("hours", list(DEPART_DUTY)), ("days", EVERY_DAY),
                  ("stay_day", STAY_DAYS)]),
              OD([("activity", "san_arrive"), ("place", "P-SANATORIUM"), ("role", me), ("hours", list(ARRIVE_DUTY)), ("days", EVERY_DAY),
                  ("stay_day", 0)])]
    duties += [OD([("activity", "eat_sanatorium_" + m), ("place", PLACE_ID), ("role", seat_tag(g)), ("hours", list(h)), ("days", EVERY_DAY)])
               for m, h in MEALS]
    return OD([
        ("key", g["key"]), ("town", 1), ("mesh", g["mesh"]), ("age", g["age"]), ("female", g["female"]),
        ("household", "t_guest_" + short(g) + ("" if g["seat"] == 0 else "_b")), ("home", home), ("chronotype", "lark"),
        ("wake", g["wake"]), ("bed", g["bed"]), ("spread", 0.25),
        ("stay", OD([("cycle", CYCLE_WEEKS * 7), ("from", PAIR_WEEK[g["pair"]] * 7 + 5), ("days", STAY_DAYS), ("arrive", ARRIVE), ("leave", LEAVE)])),
        ("duties", duties + [inhale_duty(GUESTS.index(g))]),
        ("friends", list(g["friends"])), ("temper", OD(g["temper"])), ("walk_speed", g["walk_speed"]),
        ("occupation", "guest"), ("likes", OD(g["likes"])),
    ])


def platform_place():
    """The guests' end of the beach halt's platform: up the halt's own way (people_places.json rail_beach), then along the
    deck, a step back from the boarding points. Under the sky ("open")."""
    halt = next(p for p in load(PEOPLE_PLACES)["places"] if p["id"] == "rail_beach")
    boards = [sl["pos"] for sl in halt["slots"] if sl["kind"] == "board"]
    way = [list(w) for w in halt["way"]]
    back = 0.9 if way[-1][1] < boards[0][1] else -0.9      # away from the track: the side the way comes up on
    slots = []
    for i in range(6):
        b = boards[min(i // 2, len(boards) - 1)]
        pos = [round(b[0] + (1.3 if i % 2 else -1.3), 2), round(b[1] - back, 2), b[2]]
        slots.append(OD([("kind", "stand"), ("pos", pos), ("yaw", halt.get("yaw", 90.0)), ("height", 0.0), ("approach", list(pos))]))
    return OD([("id", PLATFORM_ID), ("kind", PLATFORM_KIND), ("door", list(halt["pos"])), ("yaw", halt.get("yaw", 90.0)), ("island", 0),
               ("open", 1), ("slots", slots), ("roles", []), ("path", way)])


def dining_place(things, door, yaw):
    """The dining room: in by the sanatorium's door, 1.6 m into the lobby (one open hall with the tables), each guest's
    place at his or her chair (the city thing's position and yaw; the seat 0.45 m over the floor). "thing": while the guest
    sits there the chair is held (UPeopleLifeSubsystem::UpdateThingHolds), so the hero can't sit on him."""
    slots = []
    for table in sorted(TABLES):
        for seat, thing in enumerate(TABLES[table]["chairs"]):
            t = things[thing]
            # the thing's own point is on the seat's top; the slot is the floor under it (+ height), as everywhere in the life
            h = float((t.get("props") or {}).get("height", 0.45))
            x, y, z = (round(v / 100.0, 2) for v in t["ue"])
            slots.append(OD([("kind", "staff"), ("tag", "guest_seat_%d%s" % (table, "ab"[seat])), ("pos", [x, y, round(z - h, 2)]),
                             ("yaw", round(t["yaw"] % 360.0, 1)), ("height", h), ("thing", thing)]))
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
    for thing in [c for t in TABLES.values() for c in t["chairs"]] + list(INHALE_CHAIRS):
        if thing not in things:
            sys.exit("sanatorium_guests: %s is not a thing in interiors.json (python3 tools/city/interiors.py)" % thing)
    # the places: the dining room and the inhalation room after the sanatorium itself
    pd = load(PLACES)
    places = [p for p in pd["places"] if p["id"] not in (PLACE_ID, PLATFORM_ID, INHALE_PLACE)]
    at = next(i for i, p in enumerate(places) if p["id"] == "P-SANATORIUM")
    san = places[at]
    places.insert(at + 1, dining_place(things, san["door"], san["yaw"]))
    places.insert(at + 2, inhale_place(things, san["door"], san["yaw"]))
    places.insert(at + 3, platform_place())
    rooms = [r for t in TABLES.values() for r in t["rooms"]]
    for room in rooms:
        flat = next((p for p in places if p["id"] == room), None)
        if not flat or flat["kind"] != "flat":
            sys.exit("sanatorium_guests: home %s is not a flat in town_places.json" % room)
    pd["places"] = places
    out[PLACES] = dump(pd)
    # the people: at the end, after the generated town
    fd = load(FOLK)
    keys = {g["key"] for g in GUESTS}
    taken = {p["home"] for p in fd["people"] if p["key"] not in keys}
    for room in rooms:
        if room in taken:
            sys.exit("sanatorium_guests: %s already has a household (pick a free room)" % room)
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
    lines = [l for l in text.split("\n") if '"id": "eat_sanatorium' not in l and '"id": "cure_sanatorium' not in l
             and not any('"id": "%s"' % e in l for e in EXTRA_IDS)]
    at = next(i for i, l in enumerate(lines) if '"id": "work_town"' in l)
    lines[at + 1:at + 1] = ACTIVITY_LINES + EXTRA_LINES + [INHALE_LINE]
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
    print("sanatorium guests: %d people in turn (a fortnight each, two tables), %d meals a day at %s, inhalations at %s"
          % (len(GUESTS), len(MEALS), PLACE_ID, INHALE_PLACE))


if __name__ == "__main__":
    main()
