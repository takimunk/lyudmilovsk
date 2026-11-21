#!/usr/bin/env python3
"""Checks the town's generated data (tools/town/town_life.py, docs/town_life.md) before the simulation runs it:

  Content/PeopleData/town_places.json, townsfolk.json, counters.json, activities.json, people.json
  Content/Localization/town/strings_{ru,en,ja}.json

keys and place ids unique, every home a flat that exists and holds its household in its beds, every duty at a place that
exists, of a kind its activity goes to, in a role it has a slot for and with room for everybody on shift at once, a
night's sleep that no duty cuts into, friends and counter sellers who exist, names and jobs in the three languages; no
seat slot of a public space (Content/City/public_spaces.json) inside the space's own solid geometry, and the town's copy
of those slots where the spot puts them.

    python3 tools/peoplesim/check_town_data.py          (run.sh week runs it first)
"""
import collections
import json
import math
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
DATA = os.path.join(CONTENT, "PeopleData")
STRINGS = os.path.join(CONTENT, "Localization", "town")
fails = 0


def check(ok, text, *bad):
    """One line per check; the first few offenders under a failed one."""
    global fails
    print("  %s %s" % ("ok  " if ok else "FAIL", text))
    if not ok:
        fails += 1
        for b in bad[:6]:
            print("        %s" % (b,))


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def within(h, a, b):
    return (h - a) % 24 < (b - a) % 24


places_doc = load(os.path.join(DATA, "town_places.json"))
folk = load(os.path.join(DATA, "townsfolk.json"))["people"]
acts = {a["id"]: a for a in load(os.path.join(DATA, "activities.json"))["activities"]}
story = load(os.path.join(DATA, "people.json"))["people"]
story_places = load(os.path.join(DATA, "people_places.json"))
counters = load(os.path.join(DATA, "counters.json"))["counters"]
strings = {l: load(os.path.join(STRINGS, "strings_%s.json" % l)) for l in ("ru", "en", "ja")}
places = places_doc["places"]
by_id = {p["id"]: p for p in places}
print("the town's data: %d places, %d townsfolk, %d activities, %d counters" % (len(places), len(folk), len(acts), len(counters)))

# --- ids
dup = [k for k, n in collections.Counter(p["id"] for p in places).items() if n > 1]
check(not dup, "town place ids are unique (%d twice)" % len(dup), *dup)
academ_ids = {p.get("id") for p in story_places.get("places", []) + story_places.get("flats", [])}
clash = sorted(set(by_id) & academ_ids)
check(not clash, "no town place id is also an Academgorodok place's (%d)" % len(clash), *clash)
keys = [p["key"] for p in folk]
dup = [k for k, n in collections.Counter(keys).items() if n > 1]
story_keys = {p["key"] for p in story}
check(not dup and not (set(keys) & story_keys) and all(k.startswith("t_") for k in keys),
      "townsfolk keys are unique, t_-prefixed, none a story person's (%d twice, %d clashes)" % (len(dup), len(set(keys) & story_keys)),
      *(dup + sorted(set(keys) & story_keys)))
all_keys = set(keys) | story_keys

# --- homes: a flat that exists, one household each, no more people than its beds sleep
bad_home, households = [], collections.defaultdict(set)
stays = []
residents = collections.defaultdict(list)
for p in folk:
    f = by_id.get(p.get("home"))
    if not f or f["kind"] != "flat":
        bad_home.append("%s: home %s" % (p["key"], p.get("home")))
        continue
    if p.get("stay"):
        stays.append(p)     # a guest in turn (tools/town/sanatorium_guests.py): the room is theirs for the stay only, checked below
        continue
    households[p["home"]].add(p["household"])
    residents[p["home"]].append(p)
check(not bad_home, "every townsperson's home is a flat that exists (%d not)" % len(bad_home), *bad_home)
shared = ["%s: %s" % (f, sorted(h)) for f, h in households.items() if len(h) > 1]
check(not shared, "one household a flat (%d flats with two)" % len(shared), *shared)
split = collections.defaultdict(set)
for p in folk:
    split[p["household"]].add(p.get("home"))
split = ["%s: %s" % (h, sorted(f)) for h, f in split.items() if len(f) > 1]
check(not split, "a household lives in one flat (%d split)" % len(split), *split)
over = []
for fid, rs in residents.items():
    beds = sum(s.get("capacity", 1) for s in by_id[fid].get("slots", []) if s.get("kind") == "bed")
    sofas = sum(s.get("capacity", 1) for s in by_id[fid].get("slots", []) if s.get("kind") == "sofa")
    if len(rs) > beds + sofas:
        over.append("%s: %d people, %d beds + %d sofas" % (fid, len(rs), beds, sofas))
check(not over, "no flat holds more than its beds and sofas sleep (%d over)" % len(over), *over)
# the guests in turn: nobody else's flat, no room and no chair two stays at once, four of them here on any day
def stay_days(p):
    st = p["stay"]
    return {(st["from"] + i) % st["cycle"] for i in range(st["days"] + 1)}       # the day they come to the day they go
clash = []
for i, p in enumerate(stays):
    if p["home"] in residents:
        clash.append("%s: %s is somebody's flat" % (p["key"], p["home"]))
    for q in stays[i + 1:]:
        both = stay_days(p) & stay_days(q)
        # the day one goes and the other comes is shared: out by the morning train, in by the next
        both = {d for d in both if not ((d == (p["stay"]["from"] + p["stay"]["days"]) % p["stay"]["cycle"] and d == q["stay"]["from"] and p["stay"]["leave"] <= q["stay"]["arrive"])
                                        or (d == (q["stay"]["from"] + q["stay"]["days"]) % q["stay"]["cycle"] and d == p["stay"]["from"] and q["stay"]["leave"] <= p["stay"]["arrive"]))}
        if not both:
            continue
        if p["home"] == q["home"]:
            clash.append("%s and %s: both in %s on cycle days %s" % (p["key"], q["key"], p["home"], sorted(both)[:3]))
        roles = {d.get("role") for d in p["duties"] if d.get("role", "").startswith("guest_seat")} & {d.get("role") for d in q["duties"]}
        if roles:
            clash.append("%s and %s: both at %s on cycle days %s" % (p["key"], q["key"], sorted(roles), sorted(both)[:3]))
if stays:
    cycle = stays[0]["stay"]["cycle"]
    here = [sum(1 for p in stays if d in {(p["stay"]["from"] + i) % cycle for i in range(1, p["stay"]["days"])}) for d in range(cycle)]
    check(not clash and min(here) >= 2 and max(here) <= 4, "the guests in turn: no room or chair two stays at once (%d clashes), %d-%d of them between trains on any day"
          % (len(clash), min(here), max(here)), *clash)

# --- duties: the place exists, its kind is one the activity goes to, a slot for the role, room for everybody on shift
bad_duty, no_slot = [], []
on_shift = collections.defaultdict(lambda: collections.Counter())      # (place, role) -> (weekday, half-hour) -> people
for p in folk:
    for d in p["duties"]:
        a = acts.get(d["activity"])
        if not a:
            bad_duty.append("%s: activity %s unknown" % (p["key"], d["activity"]))
            continue
        if a.get("free", 1) != 0:
            bad_duty.append("%s: %s is a free-time activity, not a duty" % (p["key"], d["activity"]))
        if not d.get("place"):
            continue
        pl = by_id.get(d["place"])
        if not pl:
            bad_duty.append("%s: place %s missing" % (p["key"], d["place"]))
            continue
        if pl["kind"] not in a.get("places", []):
            bad_duty.append("%s: %s at a %s (%s goes to %s)" % (p["key"], d["place"], pl["kind"], d["activity"], a.get("places")))
        if a.get("round") or a.get("when"):
            continue        # a round's workplace is its doors, one after another; a meeting (the book club) has its members' own seats
        if "stay_day" in d:
            continue        # a guest's day of coming or going: the reception, the platform, no job
        staff = [s for s in pl.get("slots", []) if s.get("kind") == "staff"]
        tagged = [s for s in staff if s.get("tag") == d.get("role")]
        if not (tagged or staff):
            no_slot.append("%s: no staff slot at %s for %s" % (p["key"], d["place"], d.get("role")))
        h0, h1 = d["hours"]
        span = (h1 - h0) % 24 or 24
        for day in d["days"]:
            for i in range(int(span * 2)):
                t = day * 24 + h0 + i * 0.5
                if not p.get("stay"):       # guests in turn share a chair one after another (checked with the stays)
                    on_shift[(d["place"], d.get("role"))][(int(t // 24) % 7, round(t % 24 * 2))] += 1
check(not bad_duty, "every duty is a known duty activity at a place that exists, of a kind it goes to (%d not)" % len(bad_duty), *bad_duty)
check(not no_slot, "every job has a staff slot at its workplace (%d not)" % len(no_slot), *no_slot)
crowded = []
for (pid, role), slots in on_shift.items():
    pl = by_id.get(pid)
    if not pl:
        continue
    staff = [s for s in pl.get("slots", []) if s.get("kind") == "staff"]
    tagged = [s for s in staff if s.get("tag") == role] or staff
    cap = sum(s.get("capacity", 1) for s in tagged)
    peak = max(slots.values())
    if peak > cap:
        crowded.append("%s %s: %d on shift at once, %d staff places" % (pid, role, peak, cap))
check(not crowded, "every workplace has a staff place for everybody on shift at once (%d short)" % len(crowded), *crowded)
unstaffed = []
for pl in places:
    for role in pl.get("roles", []):
        if (pl["id"], role) not in on_shift:
            unstaffed.append("%s %s" % (pl["id"], role))
check(not unstaffed, "every job a workplace lists has somebody doing it (%d without)" % len(unstaffed), *unstaffed)

# --- the day: a night's sleep (7..10.5 h for grown-ups), no duty starting or ending inside it
short, cut = [], []
for p in folk:
    night = (p["wake"] - p["bed"]) % 24
    if night < 6.9 or (p["age"] >= 16 and night > 10.6):
        short.append("%s: bed %.2f wake %.2f (%.1f h)" % (p["key"], p["bed"], p["wake"], night))
    for d in p["duties"]:
        h0, h1 = d["hours"][0] % 24, d["hours"][1] % 24
        if within(h0, p["bed"], p["wake"]) or (h1 != p["bed"] and within(h1, p["bed"], p["wake"]) and h1 != h0):
            cut.append("%s: %s %s, asleep %.2f-%.2f" % (p["key"], d["activity"], d["hours"], p["bed"], p["wake"]))
check(not short, "everybody's night is 7 to 10.5 hours (a child's more) (%d not)" % len(short), *short)
check(not cut, "no duty starts or ends while they sleep (%d do)" % len(cut), *cut)
kids = [p["key"] for p in folk if p["age"] < 16 and not p.get("stay") and any(d["activity"] not in ("school_town", "school") for d in p["duties"])]
check(not kids, "children under 16 have no job (%d do)" % len(kids), *kids)
pupils = [p for p in folk if any(d.get("role") == "pupil" for d in p["duties"])]
odd = ["%s %d" % (p["key"], p["age"]) for p in pupils if not 7 <= p["age"] <= 18]
check(pupils and not odd, "pupils are of school age (%d pupils, %d not)" % (len(pupils), len(odd)), *odd)

# --- friends and counters
ghosts = ["%s -> %s" % (p["key"], f) for p in folk for f in p.get("friends", []) if f not in all_keys or f == p["key"]]
check(not ghosts, "friends are people who exist, not themselves (%d not)" % len(ghosts), *ghosts)
duty_acts = {p["key"]: {d["activity"] for d in p.get("duties", [])} for p in folk + story}
bad_counter = []
for c in counters:
    if c["person"] not in all_keys:
        bad_counter.append("%s: nobody of that key" % c["person"])
    elif c.get("activity") and c["activity"] not in acts:
        bad_counter.append("%s: activity %s unknown" % (c["person"], c["activity"]))
    elif c.get("town") and c.get("activity") not in duty_acts.get(c["person"], set()):
        bad_counter.append("%s: never does %s (no such duty)" % (c["person"], c.get("activity")))
check(not bad_counter, "every counter's seller exists and works there (%d not)" % len(bad_counter), *bad_counter)

# --- names and jobs in ru, en, ja
missing, stale = [], []
for lang, t in strings.items():
    for k in keys:
        for f in ("name", "role"):
            if not str(t.get("npc.%s.%s" % (k, f), "")).strip():
                missing.append("%s: npc.%s.%s" % (lang, k, f))
    for k in t:
        m = re.match(r"npc\.(.+)\.(name|role)$", k)
        if not m or m.group(1) not in set(keys):
            stale.append("%s: %s" % (lang, k))
check(not missing, "every townsperson has a name and a job in ru, en and ja (%d missing)" % len(missing), *missing)
check(not stale, "the town's tables hold nobody else (%d stale keys)" % len(stale), *stale)
script = []
for k in keys:
    ru, en, ja = (strings[l].get("npc.%s.name" % k, "") for l in ("ru", "en", "ja"))
    if not re.search(r"[Ѐ-ӿ]", ru) or not re.search(r"[぀-ヿ一-鿿]", ja) or re.search(r"[Ѐ-ӿ぀-ヿ一-鿿]", en):
        script.append("%s: %s / %s / %s" % (k, ru, en, ja))
check(not script, "names are written in each language's script (Cyrillic, Latin, kana) (%d not)" % len(script), *script)
dupname = [n for n, c in collections.Counter(strings["en"].get("npc.%s.name" % k) for k in keys).items() if c > 1]
check(not dupname, "no two townsfolk share a full name (%d do)" % len(dupname), *dupname)

# --- seats in the public spaces (tools/public_spaces.py): a slot inside a solid box (a basin's kerb, a parapet, a wall)
# puts the sitter inside its collision; the game's ground trace then stands them on top and they never sit
spaces = load(os.path.join(CONTENT, "City", "public_spaces.json"))
solid = [b for b in spaces["boxes"] if b.get("size")]


def in_box(b, x, y, inset=0.02):
    a = math.radians(b.get("yaw", 0.0))
    dx, dy = x - b["c"][0], y - b["c"][1]
    u, v = dx * math.cos(a) + dy * math.sin(a), -dx * math.sin(a) + dy * math.cos(a)
    return abs(u) < b["size"][0] / 2 - inset and abs(v) < b["size"][1] / 2 - inset


buried, nslots = [], 0
for sp in spaces["spots"]:
    for i, sl in enumerate(sp.get("slots", [])):
        nslots += 1
        x, y, z = sl["pos"]
        feet = z - sl.get("height", 0.0)
        for b in solid:
            if b["c"][2] + b["size"][2] / 2 > feet + 0.1 and b["c"][2] - b["size"][2] / 2 < z + 1.0 and in_box(b, x, y):
                buried.append("%s slot %d at (%.2f, %.2f) inside a %s" % (sp["id"], i, x, y, b["kind"]))
                break
check(not buried, "no public-space seat slot is inside solid geometry (%d slots, %d inside)" % (nslots, len(buried)), *buried)
spot_xy = {sp["id"]: [tuple(sl["pos"][:2]) for sl in sp.get("slots", [])] for sp in spaces["spots"]}
moved = []
for p in places:
    want = spot_xy.get(p["id"])
    if want is None:
        continue
    got = [tuple(sl["pos"][:2]) for sl in p.get("slots", [])]
    if len(got) != len(want) or any(math.hypot(a[0] - b[0], a[1] - b[1]) > 0.01 for a, b in zip(got, want)):
        moved.append(p["id"])
check(not moved, "the town's places keep their public spots' seat slots (%d differ)" % len(moved), *moved)

print("all ok" if not fails else "%d FAILED" % fails)
sys.exit(1 if fails else 0)
