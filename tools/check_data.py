#!/usr/bin/env python3
"""Cross-data consistency checks of the game's data files (docs/testing.md): what one file names, another must have.

    python3 tools/check_data.py [-v]        exits 1 when there is a problem (notes alone do not fail it)

Checked, offline, on the committed files (no engine, no LFS):
  ids        no duplicate ids (people, places, flats, activities, festivals, quests and their stages/objectives,
             teleports, map places, dishes, fishing spots, plan plots/spots, counters), no duplicate keys in the string
             tables (json.load would silently keep the last one)
  people     friends, households, duties (activity, place), likes, start: known activities, people and places; every
             person has npc.<key>.name; a story person's model (mesh, and body once made) is a committed SK_<name>;
             a townsperson's body is Town/R_<key without t_> as town_looks.json says; wake/bed and duty hours in
             0..24, duties of one day do not overlap
  voice      every speaker with line.<speaker>.* has an entry in localization/voice.json (or is skipped there)
  items      the item list is LyudInventory.cpp's (and FishingModel.cpp's fish): every item has item.<id>.name/.desc and
             an icon (T_Icon_<id> in Content/Inventory/Localized/ru, the game's LyudText::LoadTexture path); every item
             a shop, stall, menu, food or quest names exists; prices >= 0, counts >= 1
  places     activity place kinds exist, festival venues are a place kind/spot of the plan, quest reach targets and
             teleport places are map.json ids, fishing spots and kitchens name plan plots/spots
  bounds     coordinates inside the map (map.json frame + a margin), heights of the ground sane, seat heights 0.2..1.3 m
  strings    every title/name key that data names exists in the tables (with the generated sub-tables)
A note (not a failure) is printed for what is odd but allowed (a voice entry nobody speaks with, ...).
"""
import glob
import json
import os
import re
import subprocess
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
SRC = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Source", "Lyudmilovsk", "Private")
VERBOSE = "-v" in sys.argv

problems = []
notes = []


def bad(where, msg):
    problems.append("%s: %s" % (where, msg))


def note(where, msg):
    notes.append("%s: %s" % (where, msg))


def _no_dups(pairs, where):
    seen = {}
    for k, v in pairs:
        if k in seen:
            bad(where, "duplicate key %r in one object" % k)
        seen[k] = v
    return seen


def load(rel):
    path = os.path.join(CONTENT, rel)
    with open(path, encoding="utf-8") as f:
        return json.load(f, object_pairs_hook=lambda p: _no_dups(p, rel))


def dups(where, ids):
    seen = set()
    for i in ids:
        if i in seen:
            bad(where, "duplicate id %r" % (i,))
        seen.add(i)
    return seen


def git_files(prefix):
    try:
        out = subprocess.check_output(["git", "-C", ROOT, "ls-files", prefix], stderr=subprocess.DEVNULL)
        return set(out.decode("utf-8").split("\n"))
    except (OSError, subprocess.CalledProcessError):
        return set()


# --- the tables ---------------------------------------------------------------------------------------------------
LOC = os.path.join(CONTENT, "Localization")
strings = {}
for lang in ("ru", "en", "ja"):
    t = {}
    for path in [os.path.join(LOC, "strings_%s.json" % lang)] + sorted(glob.glob(os.path.join(LOC, "*", "strings_%s.json" % lang))):
        rel = os.path.relpath(path, CONTENT)
        with open(path, encoding="utf-8") as f:
            for k, v in json.load(f, object_pairs_hook=lambda p, r=rel: _no_dups(p, r)).items():
                t.setdefault(k, v)
    strings[lang] = t
S = strings["ru"]


def need_key(where, key):
    for lang, t in strings.items():
        if key not in t:
            bad(where, "string key %s missing in %s" % (key, lang))
            return False
    return True


# --- items (C++ is the source: LyudInventory.cpp MakeDefs, FishingModel.cpp GFish) --------------------------------
with open(os.path.join(SRC, "LyudInventory.cpp"), encoding="utf-8") as f:
    inv = f.read()
body = inv[inv.index("MakeDefs()"):inv.index("const TArray<FLyudItemDef>& ULyudInventory::AllDefs()")]
items = set(re.findall(r'\b(?:Drink|Good|FeedGood|Wear|Bait)\(TEXT\("(\w+)"\)', body))
items |= set(re.findall(r'\.Id = TEXT\("(\w+)"\)', body))
items |= set(re.findall(r'TEXT\("(find_\w+)"\)', body))
item_price = {m.group(1): int(m.group(2)) for m in re.finditer(r'\bGood\(TEXT\("(\w+)"\),[^;]*?,\s*(-?\d+),\s*[\d.]+f(?:,[^)]*)?\);', body)}
with open(os.path.join(SRC, "Fishing", "FishingModel.cpp"), encoding="utf-8") as f:
    fm = f.read()
gfish = fm[fm.index("GFish[] = {"):]
gfish = gfish[:gfish.index("};")]
fish = re.findall(r'\{\s*"(\w+)",', gfish)
items |= {"fish_" + x.lower() for x in fish}
for i, p in item_price.items():
    if p < 0:
        bad("LyudInventory.cpp", "item %s has a negative price %d" % (i, p))
icons = git_files("unreal/Lyudmilovsk/Content/Inventory/Localized/ru")
no_icon = []
for i in sorted(items):
    need_key("item %s" % i, "item.%s.name" % i)
    need_key("item %s" % i, "item.%s.desc" % i)
    if icons and "unreal/Lyudmilovsk/Content/Inventory/Localized/ru/T_Icon_%s.uasset" % i not in icons:
        no_icon.append(i)
if no_icon:
    bad("icons", "%d items have no /Game/Inventory/Localized/ru/T_Icon_<id> (the pocket shows a grey square; "
        "tools/gen_localized.py icon_<id>, then unreal/scripts import): %s" % (len(no_icon), " ".join(no_icon)))
for k in S:
    m = re.match(r"item\.(\w+)\.name$", k)
    if m and m.group(1) not in items:
        note("strings", "item.%s.name names no item of LyudInventory.cpp" % m.group(1))


def need_item(where, i):
    if i not in items:
        bad(where, "unknown item %r" % (i,))


def need_goods(where, goods):
    for g in goods:
        need_item(where, g.get("item"))
        if g.get("price", 0) < 0:
            bad(where, "negative price for %s" % g.get("item"))
        if g.get("count", 1) < 1:
            bad(where, "count < 1 for %s" % g.get("item"))


# --- map frame and bounds -----------------------------------------------------------------------------------------
umap = load("UI/map.json")
fr = umap["frame"]
FACE = fr["portal_face_y"] / 100.0                # UE metres of map y = 0
MARGIN = 50.0
X0, X1, Y0, Y1 = fr["x_min"] - MARGIN, fr["x_max"] + MARGIN, fr["y_min"] - MARGIN, fr["y_max"] + MARGIN
Z0, Z1 = -80.0, 250.0                             # deepest sea bed .. highest hill, metres


def in_map(where, x, y, z=None):
    if not (X0 <= x <= X1 and Y0 <= y <= Y1):
        bad(where, "(%.1f, %.1f) is off the map" % (x, y))
    elif z is not None and not (Z0 <= z <= Z1):
        bad(where, "height %.1f m is not on this map's ground (%g..%g)" % (z, Z0, Z1))


def in_map_ue_m(where, p):
    """p: Unreal metres (UE cm / 100)."""
    in_map(where, p[0], p[1] - FACE, p[2] if len(p) > 2 else None)


GROUND_SLOTS = {"stand", "staff", "visit", "sit_ground", "board", "counter", "grill", "vending", "booth"}


def seat_height(where, h, kind=None):
    """A slot's height: 0 for what is done on one's feet or on the ground, else a seat 0.2..1.3 m over its floor."""
    if kind in GROUND_SLOTS:
        if not 0.0 <= h <= 1.3:
            bad(where, "height %.2f m" % h)
    elif not (0.2 <= h <= 1.3):
        bad(where, "seat height %.2f m (a seat is 0.2..1.3 m over its floor)" % h)


map_ids = dups("UI/map.json places", [p["id"] for p in umap["places"]])
for p in umap["places"]:
    in_map("UI/map.json %s" % p["id"], p["x"], p["y"])
    need_key("UI/map.json %s" % p["id"], "place.%s.name" % p["id"])

for ar in load("City/public_spaces.json").get("areas", []):
    need_key("public_spaces area %s" % ar["id"], ar["name"])
    in_map_ue_m("public_spaces area %s" % ar["id"], ar["at"])

# --- places -------------------------------------------------------------------------------------------------------
plan = load("City/city_plan.json")
plan_ids = dups("City/city_plan.json plots+spots", [p["id"] for p in plan["plots"]] + [s["id"] for s in plan.get("spots", [])])
plan_kinds = {p.get("kind") for p in plan["plots"]} | {s.get("kind") for s in plan.get("spots", [])}
tplan = load("PeopleData/town_plan.json")
tplan_ids = {s["id"] for s in tplan.get("spots", [])}
plan_kinds |= {s.get("kind") for s in tplan.get("spots", [])}

tpl = load("PeopleData/town_places.json")
town_place_ids = dups("PeopleData/town_places.json", [p["id"] for p in tpl["places"]])
town_kinds = {p["kind"] for p in tpl["places"]}
for p in tpl["places"]:
    w = "town_places %s" % p["id"]
    if "pos" in p or "door" in p:
        in_map_ue_m(w, p.get("pos", p.get("door")))
    for s in p.get("slots", []):
        if "pos" in s:
            in_map_ue_m(w + " slot", s["pos"])
        if "height" in s:
            seat_height(w + " slot " + s.get("kind", "?"), s["height"], s.get("kind"))

pp = load("PeopleData/people_places.json")
pp_ids = dups("PeopleData/people_places.json places", [p["id"] for p in pp["places"]])
flat_ids = dups("PeopleData/people_places.json flats", [f["id"] for f in pp["flats"]])
dups("PeopleData/people_places.json stairs", [s["id"] for s in pp["stairs"]])
pp_kinds = {p["kind"] for p in pp["places"]}
for p in pp["places"]:
    if "door" in p:
        in_map_ue_m("people_places %s" % p["id"], p["door"])
    for s in p.get("slots", []):
        if "height" in s:
            seat_height("people_places %s slot %s" % (p["id"], s.get("kind")), s["height"], s.get("kind"))
for fl in pp["flats"]:
    in_map_ue_m("people_places flat %s" % fl["id"], fl["door"])
    for s in fl.get("slots", []):
        if "height" in s:
            seat_height("people_places flat %s slot %s" % (fl["id"], s.get("kind")), s["height"], s.get("kind"))
for s in pp["club"].get("seats", []):
    seat_height("people_places club seat %s" % s.get("index"), s["height"])

seats = load("Restaurant/seats.json")
for i, s in enumerate(seats["seats"]):
    seat_height("Restaurant/seats.json seat %d" % i, s["height"])
    in_map_ue_m("Restaurant/seats.json seat %d" % i, s["pos"])

all_place_ids = town_place_ids | pp_ids | flat_ids | plan_ids | tplan_ids
# "snow": the winter's path patches, made at run time (PeopleLife.cpp, PeopleSim.cpp KindNames)
all_kinds = town_kinds | pp_kinds | plan_kinds | {"home", "club", "flat", "snow"}

acts = load("PeopleData/activities.json")
act_ids = dups("PeopleData/activities.json", [a["id"] for a in acts["activities"]])
for a in acts["activities"]:
    for k in a.get("places", []):
        if k not in all_kinds and k not in all_place_ids:
            bad("activity %s" % a["id"], "place %r is no place kind or id of town_places/people_places/the plan" % k)
    if "hours" in a and not all(0 <= h <= 48 for h in a["hours"]):
        bad("activity %s" % a["id"], "hours %s" % a["hours"])
    if "minutes" in a and (a["minutes"][0] < 0 or a["minutes"][0] > a["minutes"][-1]):
        bad("activity %s" % a["id"], "minutes %s" % a["minutes"])

# --- people -------------------------------------------------------------------------------------------------------
story = load("PeopleData/people.json")["people"]
town = load("PeopleData/townsfolk.json")["people"]
people = story + town
person_ids = dups("people.json + townsfolk.json", [p["key"] for p in people])
humans = git_files("unreal/Lyudmilovsk/Content/Characters/Humans")
looks = load("PeopleData/town_looks.json")["people"]


def overlaps(spans):
    spans = sorted(spans)
    return any(b0 < a1 for (a0, a1), (b0, b1) in zip(spans, spans[1:]))


for p in people:
    k = p["key"]
    w = "person %s" % k
    if "npc.%s.name" % k not in S:
        bad(w, "no npc.%s.name in the strings" % k)
    for fr_ in p.get("friends", []):
        if fr_ not in person_ids:
            bad(w, "friend %r is nobody" % fr_)
        if fr_ == k:
            bad(w, "is their own friend")
    for a in p.get("likes", {}):
        if a not in act_ids:
            bad(w, "likes unknown activity %r" % a)
    if "start" in p and p["start"].get("activity") not in act_ids:
        bad(w, "start activity %r unknown" % p["start"].get("activity"))
    for h in ("wake", "bed"):
        if h in p and not 0 <= p[h] < 24:
            bad(w, "%s %.2f is not an hour of the day" % (h, p[h]))
    days = {d: [] for d in range(7)}
    for du in p.get("duties", []):
        if du.get("activity") not in act_ids:
            bad(w, "duty activity %r unknown" % du.get("activity"))
        if du.get("place") and du["place"] not in all_place_ids:
            bad(w, "duty place %r is no place" % du["place"])
        a, b = du["hours"]
        if not (0 <= a <= 24 and 0 <= b <= 24) or a == b:
            bad(w, "duty hours %s" % du["hours"])
            continue
        spans = [(a, b)] if a < b else [(a, 24), (0, b)]
        for d in du.get("days", range(7)):
            if not 0 <= d <= 6:
                bad(w, "duty day %r" % d)
                continue
            days[d] += spans
    for d, spans in days.items():
        if overlaps(spans):
            bad(w, "two duties overlap on day %d: %s" % (d, sorted(spans)))
            break
    if p.get("town"):
        if p.get("home") and p["home"] not in town_place_ids:
            bad(w, "home %r is no place of town_places.json" % p["home"])
        want = "Town/R_" + k[2:] if k.startswith("t_") else None
        if want and p.get("body") != want:
            bad(w, "body %r, the residents generator names it %r" % (p.get("body"), want))
        if k not in looks:
            bad(w, "no entry in town_looks.json")
        elif looks[k].get("model") != p.get("body"):
            bad(w, "town_looks.json model %r differs from body %r" % (looks[k].get("model"), p.get("body")))
    else:
        if not p.get("mesh"):
            bad(w, "no mesh (model) name")
        for f in ("mesh", "body"):
            m = p.get(f)
            if m and humans and "unreal/Lyudmilovsk/Content/Characters/Humans/%s/SK_%s.uasset" % (m, m.split("/")[-1]) not in humans:
                (bad if f == "mesh" else note)(w, "%s %s: no /Game/Characters/Humans/%s/SK_%s" % (f, m, m, m.split("/")[-1]))
for k in looks:
    if k not in person_ids:
        bad("town_looks.json", "%s is nobody of townsfolk.json" % k)
households = {}
for p in people:
    households.setdefault(p.get("household"), []).append(p["key"])

# voices: every speaker of line.<speaker>.* has a voice.json entry; a story person without lines needs none
with open(os.path.join(ROOT, "localization", "voice.json"), encoding="utf-8") as f:
    voice = json.load(f, object_pairs_hook=lambda p: _no_dups(p, "localization/voice.json"))
speakers = voice["speakers"]
with open(os.path.join(LOC, "strings_ru.json"), encoding="utf-8") as f:
    line_speakers = {k.split(".")[1] for k in json.load(f) if k.startswith("line.")}     # tools/gen_voice.py voices these
for s in sorted(line_speakers - set(speakers)):
    bad("voice", "speaker %s has lines but no entry in localization/voice.json" % s)
for s in sorted(set(speakers) - line_speakers - {"default"}):
    note("voice", "localization/voice.json speaker %s has no line.%s.* (stale?)" % (s, s))
for p in story:
    if p["key"] not in speakers and p["key"] in line_speakers:
        bad("voice", "%s speaks but has no voice" % p["key"])

# --- trade --------------------------------------------------------------------------------------------------------
counters = load("PeopleData/counters.json")["counters"]
dups("PeopleData/counters.json (person, activity)", [(c["person"], c["activity"]) for c in counters])
for c in counters:
    w = "counter %s/%s" % (c["person"], c["activity"])
    if c["person"] not in person_ids:
        bad(w, "seller is nobody")
    if c["activity"] not in act_ids:
        bad(w, "unknown activity")
    else:
        pe = next((p for p in people if p["key"] == c["person"]), None)
        if pe and not any(d.get("activity") == c["activity"] for d in pe.get("duties", [])) and pe.get("start", {}).get("activity") != c["activity"]:
            bad(w, "the seller has no duty %s, so never sells" % c["activity"])
    need_key(w, c["title"])
    need_goods(w, c["goods"])
    if c.get("cooks_fish", 0) < 0:
        bad(w, "negative cooks_fish price")

fair = load("City/fair_goods.json")["stalls"]
for sid, st in fair.items():
    w = "fair stall %s" % sid
    need_key(w, st["title_key"])
    need_goods(w, st["goods"])
    if sid not in town_place_ids and sid not in tplan_ids:
        bad(w, "no such stall in town_places/town_plan")

menu = load("Restaurant/menu.json")
dups("Restaurant/menu.json dishes", [d["id"] for d in menu["dishes"]])
for d in menu["dishes"]:
    w = "dish %s" % d["id"]
    need_item(w, d["id"])
    if d.get("section") not in menu["sections"]:
        bad(w, "section %r is not one of the menu's" % d.get("section"))
    if d.get("price", 0) < 0:
        bad(w, "negative price")
    if d["id"] in item_price and item_price[d["id"]] != d.get("price"):
        note(w, "menu price %s, LyudInventory.cpp price %s" % (d.get("price"), item_price[d["id"]]))

food = load("Food/food.json")
for k in food.get("foods", {}):
    need_item("Food/food.json", k)
for k, v in food.items():
    if isinstance(v, dict) and k not in ("foods",):
        for i in v:
            if not i.startswith("_"):
                need_item("Food/food.json %s" % k, i)
stalls = load("Food/stalls.json")["stalls"]
dups("Food/stalls.json", [s["id"] for s in stalls])
for s in stalls:
    if s["id"] not in pp_ids:
        bad("food stall %s" % s["id"], "no place of people_places.json")

fishing = load("Fishing/fishing.json")
dups("Fishing/fishing.json spots", [s["id"] for s in fishing["spots"]])
for s in fishing["spots"] + fishing.get("kitchens", []):
    w = "fishing %s" % s["id"]
    if s.get("plan") and s["plan"] not in plan_ids:
        bad(w, "plan %r is no plot/spot of city_plan.json" % s["plan"])
    in_map_ue_m(w, [c / 100.0 for c in s["ue"]])
for k in fishing.get("kitchens", []):
    if not (0 <= k["open"] < k["close"] <= 24):
        bad("fishing kitchen %s" % k["id"], "hours %s..%s" % (k["open"], k["close"]))
if fishing.get("boat", {}).get("fee", 0) < 0:
    bad("fishing boat", "negative fee")
if fishing.get("boat", {}).get("plan") and fishing["boat"]["plan"] not in plan_ids:
    bad("fishing boat", "plan %r unknown" % fishing["boat"]["plan"])

# --- festivals ----------------------------------------------------------------------------------------------------
fest = load("PeopleData/festivals.json")["festivals"]
fest_ids = dups("PeopleData/festivals.json", [f["id"] for f in fest])
venue_ok = plan_kinds | town_kinds | plan_ids | tplan_ids | town_place_ids
for f in fest:
    w = "festival %s" % f["id"]
    dups(w + " phases", [ph["id"] for ph in f["program"]])
    for ph in f["program"]:
        v = ph.get("venue")
        if v and v not in venue_ok:
            bad(w, "phase %s: venue %r is no spot kind or id of the plan" % (ph["id"], v))
        a, b = ph["hours"]
        if not (0 <= a < b <= 48):
            bad(w, "phase %s hours %s" % (ph["id"], ph["hours"]))
        if not 0 <= ph.get("pull", 1) <= 2:
            bad(w, "phase %s pull %s" % (ph["id"], ph.get("pull")))
    for v in f.get("decor", {}).get("venues", []):
        if v not in venue_ok:
            bad(w, "decor venue %r unknown" % v)
    need_key(w, "festival.%s.name" % f["id"])
    # one venue holds one phase at a time (phases at different venues may overlap: fireworks on the pier, lamps on the promenade)
    ph = f["program"]
    for i, a in enumerate(ph):
        for b in ph[i + 1:]:
            if a.get("day", 0) == b.get("day", 0) and a.get("venue") == b.get("venue") \
                    and a["hours"][0] < b["hours"][1] and b["hours"][0] < a["hours"][1]:
                bad(w, "phases %s and %s overlap at %s" % (a["id"], b["id"], a.get("venue")))

# --- quests -------------------------------------------------------------------------------------------------------
quests = load("UI/quests.json")["quests"]
dups("UI/quests.json", [q["id"] for q in quests])
fest_phases = {"%s.%s" % (f["id"], ph["id"]) for f in fest for ph in f["program"]}
# LyudFestival.cpp sends festival_watch with the show and with <festival>.<show>, festival_visit with both the id and id.phase
fest_shows = {sh for f in fest for ph in f["program"] for sh in ph.get("show", [])}
fest_shows |= {"%s.%s" % (f["id"], sh) for f in fest for ph in f["program"] for sh in ph.get("show", [])}
for q in quests:
    w = "quest %s" % q["id"]
    need_key(w, "quest.%s.title" % q["id"])
    need_key(w, "quest.%s.desc" % q["id"])
    dups(w + " stages", [s["id"] for s in q["stages"]])
    dups(w + " objectives", [o["id"] for s in q["stages"] for o in s["objectives"]])
    for s in q["stages"]:
        need_key(w, "quest.%s.%s.log" % (q["id"], s["id"]))
        for o in s["objectives"]:
            need_key(w, "quest.%s.%s.obj" % (q["id"], o["id"]))
            ev, t = o["event"], o.get("target")
            if ev == "reach" and t not in map_ids:
                bad(w, "reach target %r is no map.json place" % t)
            elif ev == "talk" and t not in person_ids:
                bad(w, "talk target %r is nobody" % t)
            elif ev in ("get", "have", "use", "wear") and t:
                if t.endswith("*"):
                    if not any(i.startswith(t[:-1]) for i in items):
                        bad(w, "%s target %r matches no item" % (ev, t))
                else:
                    need_item(w, t)
            elif ev == "festival_visit" and t not in fest_phases and t not in fest_ids:
                bad(w, "festival_visit target %r is no festival or festival.phase" % t)
            elif ev == "festival_watch" and t not in fest_shows:
                bad(w, "festival_watch target %r is no show or festival.show of festivals.json" % t)
            elif ev == "festival_wish" and t not in fest_ids:
                bad(w, "festival_wish target %r is no festival" % t)
            if o.get("place") and o["place"] not in map_ids and o["place"] not in all_place_ids and o["place"] != "club":
                bad(w, "objective place %r unknown" % o["place"])
        for nq in s.get("reward", {}).get("start", []):
            if nq not in {x["id"] for x in quests}:
                bad(w, "reward starts unknown quest %r" % nq)

# --- developer teleports ------------------------------------------------------------------------------------------
tps = []
for path in sorted(glob.glob(os.path.join(CONTENT, "Dev", "teleports", "*.json"))):
    rel = os.path.relpath(path, CONTENT)
    for t in load(rel)["teleports"]:
        tps.append(t)
        w = "teleport %s (%s)" % (t["id"], rel)
        need_key(w, t["name"])
        if "place" in t:
            if t["place"] not in map_ids:
                bad(w, "place %r is no map.json id" % t["place"])
        elif "x" in t and "y" in t:
            in_map(w, t["x"], t["y"], t["z"] / 100.0 if "z" in t else None)
        else:
            bad(w, "neither x, y nor place")
        if "time" in t and not re.match(r"^([01]\d|2[0-3]):[0-5]\d$", t["time"]):
            bad(w, "time %r is not HH:MM" % t["time"])
dups("Dev/teleports ids", [t["id"] for t in tps])

# --- report -------------------------------------------------------------------------------------------------------
for n in notes if VERBOSE else notes[:20]:
    print("note    " + n)
if not VERBOSE and len(notes) > 20:
    print("note    ... %d more (-v)" % (len(notes) - 20))
for p in problems:
    print("PROBLEM " + p)
print("check_data: %d items, %d people, %d places, %d activities, %d festivals, %d quests, %d teleports: %d problems, %d notes"
      % (len(items), len(people), len(all_place_ids), len(act_ids), len(fest), len(quests), len(tps), len(problems), len(notes)))
sys.exit(1 if problems else 0)
