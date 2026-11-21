#!/usr/bin/env python3
"""Animals that live with the town's people (docs/animals.md "With people", backlog item 46): which households keep a dog
and who walks it, which flats in the terrace walls keep a cat and on which window sill it sits, the lady who carries the
bowls to the yard cats' feeders, and where the hedgehogs come out at dusk.

Like tools/town/town_activities.py this adds to the town generator's output and is idempotent (tools/town/town_life.py runs
it after itself):
    python3 tools/town/town_pets.py            add or refresh
    python3 tools/town/town_pets.py --check    exit 1 when the files are not as this script makes them

What it writes:
  Content/AnimalsData/pets.json         the dogs, the cats, the lady, the feeders of her round, the hedgehogs (Unreal metres,
                                        like animal_places.json); read by ULyudPets (Source/.../Animals/LyudPets.*)
  Content/PeopleData/activities.json    walk_dog, walk_dog_late, feed_cats, feed_cats_late (duties only: "free": 0)
  Content/PeopleData/town_places.json   S-CAT-FEEDERS: where the lady stands at each feeder
  Content/PeopleData/townsfolk.json     the dog owners' two walks a day (duties)
  Content/PeopleData/people.json        the lady's two rounds a day (duties of LADY)
  Content/Localization/pets/strings_<lang>.json   the pets' names (animal.pet.<id>.name)

Who gets a dog: a grown-up of the town who is up before seven and free at seven and at nineteen, one a household, picked by
a hash of the key, so the same people while the town is the same. A cat: a household of a flat in the terrace walls
(W2..W5), whose ground-floor window gives the sill. Nobody keeps both.
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
PEOPLE = os.path.join(CONTENT, "PeopleData", "people.json")
PLACES = os.path.join(CONTENT, "PeopleData", "town_places.json")
ACTIVITIES = os.path.join(CONTENT, "PeopleData", "activities.json")
PLAN = os.path.join(CONTENT, "City", "city_plan.json")
ANIMAL_PLACES = os.path.join(CONTENT, "AnimalsData", "animal_places.json")
PETS = os.path.join(CONTENT, "AnimalsData", "pets.json")
STRINGS = os.path.join(CONTENT, "Localization", "pets", "strings_%s.json")    # a table of its own (the town's holds only people)
OD = collections.OrderedDict

DOGS = 10
CATS = 14
CAT_FRIENDS = 12                            # residents a yard cat sees home
STALL_FRONT = 1.9                           # m from the fishmonger to where a cat sits, on the buyers' side of the counter
# whose work sends a cat out of the way when it comes within a broom's reach (activities; who has them as duties)
SHOO_ACTS = ["mop_floor", "mop_floor_late", "night_watch"]
WALKS = ((7.0, 7.55), (19.0, 19.6))         # the dog's morning and evening walk, hours
ROUNDS = ((7.9, 8.35), (18.4, 18.85))       # the lady's rounds: the bowls are down at 8:00 and 18:30
LADY = "korneeva_nina"                      # Nina Korneeva, 71, of Novikova 1 (people.json): "the lady from the second stair"
FEED_PLACE = "S-CAT-FEEDERS"
ANCHOR_PLACE = "P-CAFE-CLEAN"               # town_night.py's first place and first activity: ours go just before them (that
ANCHOR_ACTIVITY = "mop_floor_late"          # script puts its own before town_activities.py's, which keeps its own last)
# The evening's walk and round are activities of their own: the people's life lets a duty last till the end of the LAST
# duty of the same activity (FWorld::Think), so two duties of one activity a day would run the morning's into the evening.
ACT_IDS = ["walk_dog", "walk_dog_late", "feed_cats", "feed_cats_late"]
ACTIVITIES_ROWS = [
    '{"id": "walk_dog", "places": ["promenade", "park", "pond", "fountain", "square"], "pose": "walk", "minutes": [30, 36], "meets": {"nature": 0.8, "sun": 0.2, "social": 0.1}, "costs": {"tired": 0.02}, "free": 0, "wander": 1, "wander_m": [20, 160], "wander_pause": [15, 50], "rain": "ok", "on_time": 1}',
    '{"id": "walk_dog_late", "places": ["promenade", "park", "pond", "fountain", "square"], "pose": "walk", "minutes": [30, 36], "meets": {"nature": 0.8, "sun": 0.2, "social": 0.1}, "costs": {"tired": 0.02}, "free": 0, "wander": 1, "wander_m": [20, 160], "wander_pause": [15, 50], "rain": "ok", "on_time": 1}',
    '{"id": "feed_cats", "places": ["cat_feeder"], "slots": ["stand"], "pose": "stand", "wander": 1, "round": 1, "wander_pause": [80, 140], "minutes": [25, 27], "meets": {"nature": 0.4, "social": 0.2}, "free": 0, "rain": "ok"}',
    '{"id": "feed_cats_late", "places": ["cat_feeder"], "slots": ["stand"], "pose": "stand", "wander": 1, "round": 1, "wander_pause": [80, 140], "minutes": [25, 27], "meets": {"nature": 0.4, "social": 0.2}, "free": 0, "rain": "ok"}',
]
SILL_OUT = 0.14         # m the cat sits out from the glass, on the window's ledge
DOOR_OUT = 0.9          # m in front of the flat's door a cat waits

# the pets' names: ru, en, ja (a dog's by its owner's hash, a cat's by its flat's). The owners' lines say "he" of the dog
# (tools/barks/lines/16_pets.txt), so the households' dogs are all dogs, with dogs' names.
DOG_NAMES = [("Тузик", "Tuzik", "トゥージク"), ("Барон", "Baron", "バロン"), ("Пират", "Pirate", "ピラート"), ("Буран", "Buran", "ブラン"),
             ("Гром", "Grom", "グロム"), ("Шарик", "Sharik", "シャーリク"), ("Дружок", "Druzhok", "ドルジョーク"), ("Рекс", "Rex", "レックス"),
             ("Полкан", "Polkan", "ポルカン"), ("Мухтар", "Mukhtar", "ムフタール"), ("Бим", "Bim", "ビム"), ("Джек", "Jack", "ジェック")]
CAT_NAMES = [("Мурка", "Murka", "ムルカ"), ("Барсик", "Barsik", "バルシク"), ("Васька", "Vaska", "ワーシカ"), ("Дымок", "Dymok", "ドゥイモク"),
             ("Маркиза", "Markiza", "マルキーザ"), ("Рыжик", "Ryzhik", "ルィージク"), ("Соня", "Sonya", "ソーニャ"), ("Пушок", "Pushok", "プショーク"),
             ("Клякса", "Klyaksa", "クリャクサ"), ("Филя", "Filya", "フィーリャ"), ("Муся", "Musya", "ムーシャ"), ("Кузя", "Kuzya", "クージャ"),
             ("Симка", "Simka", "シームカ"), ("Тишка", "Tishka", "ティーシカ"), ("Боцман", "Bosun", "ボーツマン"), ("Ночка", "Nochka", "ノーチカ")]

# hedgehogs: (id, offset from a place of town_places.json or an absolute point, range m)
HEDGEHOGS = [("park_orchard", "S-PARK", (14.0, 9.0), 18.0), ("park_lawn", "S-PARK", (-16.0, -6.0), 16.0), ("pond_bank", "S-POND", (-12.0, 7.0), 14.0),
             ("playground_hedge", "S-PLAYGROUND", (9.0, -8.0), 12.0), ("academ_yard", None, (-246.0, 616.0), 16.0),
             ("academ_birches", None, (-262.0, 640.0), 14.0)]


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f, object_pairs_hook=OD)


def dump(d):
    return json.dumps(d, ensure_ascii=False, indent=1) + "\n"


def r2(v):
    return [round(float(x), 2) for x in v]


def crc(s):
    return zlib.crc32(s.encode())


def busy(p, a, b):
    """True if a duty of p's covers any of the hours a..b."""
    for d in p.get("duties", []):
        if d.get("activity") in ACT_IDS:
            continue
        h0, h1 = d["hours"]
        spans = [(h0, h1)] if h0 <= h1 else [(h0, 24.0), (0.0, h1)]
        if any(s0 < b + 0.4 and a - 0.4 < s1 for s0, s1 in spans):
            return True
    return False


def pick_dogs(people, homes, cat_households):
    fit = [p for p in people if 18 <= p["age"] <= 78 and p.get("occupation") not in ("away", "pupil", "postman")
           and p.get("wake", 9.0) <= 6.8 and (p.get("bed", 23.0) >= 20.6 or p.get("bed", 23.0) < 5.0)
           and not any(busy(p, a, b) for a, b in WALKS) and p.get("home") in homes and p["household"] not in cat_households]
    fit.sort(key=lambda p: crc("dog:" + p["key"]))
    out, seen, buildings = [], set(), collections.Counter()
    for p in fit:
        b = homes[p["home"]].get("building", p["home"])
        if p["household"] in seen or buildings[b] >= 2:
            continue
        seen.add(p["household"])
        buildings[b] += 1
        out.append(p)
        if len(out) == DOGS:
            break
    return out


def pick_friends(people, homes, taken):
    """Residents a yard cat likes and sees home: older people of households without a pet, one a building."""
    fit = [p for p in people if p["age"] >= 55 and p.get("occupation") not in ("away",) and p.get("home") in homes
           and p["household"] not in taken]
    fit.sort(key=lambda p: crc("catfriend:" + p["key"]))
    out, buildings = [], set()
    for p in fit:
        b = homes[p["home"]].get("building", p["home"])
        if b in buildings:
            continue
        buildings.add(b)
        out.append(p)
        if len(out) == CAT_FRIENDS:
            break
    return out


def pick_cats(people, homes, dwellings):
    by_house = OD()
    for p in people:
        by_house.setdefault(p["household"], []).append(p)
    fit = []
    for house, folk in by_house.items():
        home = folk[0].get("home")
        d = dwellings.get(home)
        if not d or home not in homes:
            continue
        low = [w for w in d["windows"] if w.get("storey") == 0]
        grown = [p for p in folk if p["age"] >= 16]
        if low and grown:
            fit.append((house, home, d, low[0], max(grown, key=lambda p: (p["age"], p["key"]))))
    fit.sort(key=lambda t: crc("cat:" + t[0]))
    out, walls = [], collections.Counter()
    for t in fit:
        if walls[t[2]["wall"]] >= 5:
            continue
        walls[t[2]["wall"]] += 1
        out.append(t)
        if len(out) == CATS:
            break
    return out


def build():
    out = {}
    plan = load(PLAN)
    pd = load(PLACES)
    places = [p for p in pd["places"] if p["id"] != FEED_PLACE]
    homes = {p["id"]: p for p in places if p["kind"] == "flat" and p.get("door")}
    by_id = {p["id"]: p for p in places}
    dwellings = {d["id"]: d for d in plan["wall_dwellings"]}
    fd = load(FOLK)
    people = fd["people"]
    for p in people:
        p["duties"] = [d for d in p.get("duties", []) if d.get("activity") not in ACT_IDS]

    cats = pick_cats(people, homes, dwellings)
    dogs = pick_dogs(people, homes, {c[0] for c in cats})
    names = {"ru": OD(), "en": OD(), "ja": OD()}
    pets = OD([("_doc", "Animals that live with people (tools/town/town_pets.py, docs/animals.md \"With people\"). Unreal metres, yaw "
                        "radians (the animals' frame). dogs: owner = the resident who walks it (townsfolk key) while their activity is "
                        "walk; cats: by day on the sill, at feeding time at the door; lady: who fills the feeders on her round "
                        "(activity feed); hedgehogs: where they come out at dusk from April to October; friends: residents a yard cat "
                        "sees home to their door; stalls: where cats beg while a fishmonger stands there; shoo: activities at which the "
                        "shooers send a cat out of their way."),
               ("walk", ["walk_dog", "walk_dog_late"]), ("feed", ["feed_cats", "feed_cats_late"]), ("lady", LADY)])
    rows = []
    used = set()
    for n, p in enumerate(dogs):
        h = crc("dog:" + p["key"])
        k = h % len(DOG_NAMES)
        while k in used:
            k = (k + 1) % len(DOG_NAMES)
        used.add(k)
        pid = "dog_%02d" % (n + 1)
        for i, lang in enumerate(("ru", "en", "ja")):
            names[lang]["animal.pet.%s.name" % pid] = DOG_NAMES[k][i]
        rows.append(OD([("id", pid), ("owner", p["key"]), ("household", p["household"]), ("home", p["home"]), ("breed", (h >> 8) % 3),
                        ("coat", (h >> 12) % 3), ("female", 0), ("name", "animal.pet.%s.name" % pid), ("door", r2(homes[p["home"]]["door"]))]))
        p["duties"] = p["duties"] + [OD([("activity", act), ("hours", [a, b])]) for act, (a, b) in zip(("walk_dog", "walk_dog_late"), WALKS)]
    pets["dogs"] = rows
    rows = []
    used = set()
    for n, (house, home, d, win, who) in enumerate(cats):
        h = crc("cat:" + house)
        k = h % len(CAT_NAMES)
        while k in used:
            k = (k + 1) % len(CAT_NAMES)
        used.add(k)
        pid = "cat_%02d" % (n + 1)
        for i, lang in enumerate(("ru", "en", "ja")):
            names[lang]["animal.pet.%s.name" % pid] = CAT_NAMES[k][i]
        yaw = math.radians(d["yaw"])
        nx, ny = math.cos(yaw), math.sin(yaw)
        ue = win["ue"]
        sill = [ue[0] / 100.0 + nx * SILL_OUT, ue[1] / 100.0 + ny * SILL_OUT, ue[2] / 100.0 - win["size"][1] / 2.0]
        door = [d["door_ue"][0] / 100.0 + nx * DOOR_OUT, d["door_ue"][1] / 100.0 + ny * DOOR_OUT, d["door_ue"][2] / 100.0]
        rows.append(OD([("id", pid), ("owner", who["key"]), ("household", house), ("home", home), ("window", win["id"]), ("coat", (h >> 8) % 6),
                        ("female", (h >> 16) % 2), ("name", "animal.pet.%s.name" % pid), ("sill", r2(sill)), ("yaw", round(yaw, 3)), ("door", r2(door))]))
    pets["cats"] = rows

    # the lady's round: every feeder of the animals' map, nearest her yard first
    ap = load(ANIMAL_PLACES)
    feeder = ap["places_kinds"].index("Feeder")
    feeders = sorted([p for p in ap["places"] if p[0] == feeder], key=lambda p: (p[1] + 240.0) ** 2 + (p[2] - 620.0) ** 2)
    if not feeders:
        sys.exit("town_pets: the animals' map has no feeder (tools/animal_map.py)")
    pets["feeders"] = [r2(p[1:4]) for p in feeders]
    slots = []
    for p in feeders:
        x, y, z = p[1], p[2], p[3]
        slots.append(OD([("kind", "stand"), ("pos", r2((x + 0.9, y, z))), ("yaw", 180.0), ("tag", "bowls")]))      # the bowls go down
        slots.append(OD([("kind", "stand"), ("pos", r2((x + 2.6, y + 1.4, z))), ("yaw", -152.0), ("tag", "watch")]))  # she watches them eat
    place = OD([("id", FEED_PLACE), ("kind", "cat_feeder"), ("pos", list(slots[0]["pos"])), ("yaw", 0.0), ("island", 0), ("slots", slots)])
    # before the other scripts' places (so every script finds the file as it makes it)
    import town_activities as TA
    import town_night as TN
    later_places = set(TN.PLACE_IDS) | set(TA.PLACE_IDS)
    first = next((i for i, p in enumerate(places) if p["id"] in later_places), len(places))
    pd["places"] = places[:first] + [place] + places[first:]
    out[PLACES] = dump(pd)

    hogs = []
    for hid, at, off, rng in HEDGEHOGS:
        if at:
            base = by_id[at].get("pos") or by_id[at]["door"]
            pos = [base[0] + off[0], base[1] + off[1], base[2]]
        else:
            pos = [off[0], off[1], feeders[0][3]]
        hogs.append(OD([("id", hid), ("pos", r2(pos)), ("range", rng)]))
    pets["hedgehogs"] = hogs

    # the yard cats and the townsfolk (docs/animals.md "Town cats"): who is seen home, the fish stalls, the brooms
    taken = {p["household"] for p in dogs} | {c[0] for c in cats}
    pets["friends"] = [OD([("person", p["key"]), ("home", p["home"]), ("door", r2(homes[p["home"]]["door"]))])
                       for p in pick_friends(people, homes, taken)]
    stalls, mongers = [], []
    markets = [pl for pl in places if pl["kind"] == "market"]       # the open market only: no cat walks into a shop
    for pl in markets:
        for sl in pl.get("slots", []):
            if sl.get("kind") == "staff" and sl.get("tag") == "fishmonger":
                stalls.append(OD([("place", pl["id"]), ("pos", r2(sl["pos"])), ("yaw", round(math.radians(sl.get("yaw", 0.0)), 3)),
                                  ("front", STALL_FRONT)]))
    for p in people:
        if any(d.get("role") == "fishmonger" and d.get("place") in {pl["id"] for pl in markets} for d in p.get("duties", [])):
            mongers.append(p["key"])
    pets["stalls"] = stalls
    pets["fishmongers"] = mongers
    pets["shoo"] = SHOO_ACTS
    pets["shooers"] = [p["key"] for p in people if any(d.get("activity") in SHOO_ACTS for d in p.get("duties", []))]
    out[PETS] = dump(pets)
    out[FOLK] = dump(fd)

    # the lady: people.json is written by hand: her duties in her own lines
    text = open(PEOPLE, encoding="utf-8").read()
    m = re.search(r'\{"key": "%s".*?\n    \{"key"' % LADY, text, re.S)
    if not m:
        sys.exit("town_pets: %s is not in people.json" % LADY)
    block = m.group(0)
    at = block.index('"duties": ') + len('"duties": ')
    was, used_to = json.JSONDecoder(object_pairs_hook=OD).raw_decode(block, at)
    was = [d for d in was if d.get("activity") not in ACT_IDS]
    now = was + [OD([("activity", act), ("place", FEED_PLACE), ("hours", [a, b])]) for act, (a, b) in zip(("feed_cats", "feed_cats_late"), ROUNDS)]
    block2 = block[:at] + json.dumps(now, ensure_ascii=False) + block[used_to:]
    out[PEOPLE] = text[:m.start()] + block2 + text[m.end():]
    json.loads(out[PEOPLE])

    for lang in ("ru", "en", "ja"):
        out[STRINGS % lang] = dump(names[lang])

    text = open(ACTIVITIES, encoding="utf-8").read()
    lines = [l for l in text.split("\n") if not any('"id": "%s"' % a in l for a in ACT_IDS)]
    # before town_night.py's and town_activities.py's rows (the post-steps' fixed order: south_people, town_pets, town_night,
    # town_activities; each before the first block after it, so any order of running them leaves the same file)
    import town_activities as TA
    import town_night as TN
    later = list(TN.ACT_IDS) + list(TA.ACT_IDS)
    at = next(i for i, l in enumerate(lines) if any('"id": "%s"' % a in l for a in later))
    lines[at:at] = ["    " + r + "," for r in ACTIVITIES_ROWS]
    out[ACTIVITIES] = "\n".join(lines)
    json.loads(out[ACTIVITIES])
    return out, pets


def main():
    out, pets = build()
    stale = [p for p, t in out.items() if not os.path.exists(p) or open(p, encoding="utf-8").read() != t]
    if "--check" in sys.argv:
        for p in stale:
            print("town_pets: %s is not up to date (python3 tools/town/town_pets.py)" % os.path.relpath(p, ROOT))
        sys.exit(1 if stale else 0)
    for p in stale:
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "w", encoding="utf-8") as f:
            f.write(out[p])
        print("wrote", os.path.relpath(p, ROOT))
    print("town_pets: %d dogs, %d cats, the lady %s with %d feeders, %d hedgehogs" % (
        len(pets["dogs"]), len(pets["cats"]), pets["lady"], len(pets["feeders"]), len(pets["hedgehogs"])))


if __name__ == "__main__":
    main()
