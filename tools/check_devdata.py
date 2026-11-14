#!/usr/bin/env python3
"""Static check of the quests and the developer-mode data, no engine needed (docs/dev_mode.md, docs/ui.md).

  python3 tools/check_devdata.py [--strict]

Content/UI/quests.json: every objective's event is one the game sends (grepped from the C++), its target exists (a map.json
place with a radius for reach, an item for get/have/use/wear, a person for talk), every word key is in all three string tables,
rewards and dev blocks name real items, quests and teleport points. Content/Dev/teleports/*.json: parses, ids unique across
files, places exist, name keys in all tables. The C++ dev actions (LyudDev::FAutoAction / FAutoTeleport): ids unique, name keys
present. Exit 1 on a problem; --strict also fails on warnings (a group with no dev.group key, a festival event target that
cannot be checked here).
"""
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UE = os.path.join(ROOT, "unreal", "Lyudmilovsk")
CONTENT = os.path.join(UE, "Content")
SOURCE = os.path.join(UE, "Source")
LANGS = ["ru", "en", "ja"]

problems, warnings = [], []


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


# the main tables and every feature's own (Localization/<feature>/strings_<lang>.json), as LyudText merges them
strings = {}
for l in LANGS:
    strings[l] = {}
    for path in sorted(glob.glob(os.path.join(CONTENT, "Localization", "**", "strings_%s.json" % l), recursive=True)):
        strings[l].update(load(path))


def need_key(key, where):
    missing = [l for l in LANGS if key not in strings[l]]
    if missing:
        problems.append("%s: string key %s missing in %s" % (where, key, ",".join(missing)))


# --- what exists ---------------------------------------------------------------------------------------------------------
places = {p["id"]: p for p in load(os.path.join(CONTENT, "UI", "map.json"))["places"]}
items = {m.group(1) for k in strings["ru"] for m in [re.match(r"item\.(\w+)\.name$", k)] if m}
people = set()
for name in ["people.json", "townsfolk.json"]:
    path = os.path.join(CONTENT, "PeopleData", name)
    if os.path.exists(path):
        for person in load(path).get("people", []):
            people |= {person.get("key", ""), str(person.get("body", "")).split("/")[-1]}
people |= {m.group(1) for k in strings["ru"] for m in [re.match(r"npc\.(\w+)\.name$", k)] if m}
# talk reports the dialogue's section name ([Owner] in Content/Dialogue/*.txt); FName and FString compare without case
for path in glob.glob(os.path.join(CONTENT, "Dialogue", "*.txt")):
    with open(path, encoding="utf-8", errors="replace") as f:
        people |= set(re.findall(r"^\[([^\]]+)\]", f.read(), re.M))
people = {p.lower() for p in people if p}

sources = []
for path in glob.glob(os.path.join(SOURCE, "**", "*.cpp"), recursive=True) + glob.glob(os.path.join(SOURCE, "**", "*.h"), recursive=True):
    with open(path, encoding="utf-8", errors="replace") as f:
        sources.append((os.path.relpath(path, ROOT), f.read()))
code = "\n".join(s for _, s in sources)
# events the game reports: Notify(.., TEXT("x")) and the festivals' HeroEvent(.., TEXT("x"), ..), plus reach (UpdatePosition)
events = set(re.findall(r'Notify\([^,()]+(?:\(\))?, TEXT\("([a-z_]+)"\)', code)) | set(re.findall(r'HeroEvent\([^,]+, TEXT\("([a-z_]+)"\)', code))
# ... and any lowercase word in quotes on a line that reports or names a quest event (ternaries, HandleEvent, the city things'
# "do" / "visit"), and the events documented in LyudQuests.h ("   <event> <target>   what" lines)
for line in code.splitlines():
    if re.search(r"Notify\(|HandleEvent\(|HeroEvent\(|\bEvent\b", line):
        events |= set(re.findall(r'"([a-z][a-z_]*)"', line))
header = next((t for p, t in sources if p.endswith(os.path.join("UI", "LyudQuests.h"))), "")
events |= set(re.findall(r"^ \*   ([a-z_]+)[ <]", header, re.M))
events |= {"reach", "have", "bring"}

teleports = {}
for path in sorted(glob.glob(os.path.join(CONTENT, "Dev", "teleports", "*.json"))):
    name = os.path.basename(path)
    try:
        data = load(path)
    except ValueError as e:
        problems.append("%s: does not parse: %s" % (name, e))
        continue
    for t in data.get("teleports", []):
        tid = t.get("id")
        if not tid:
            problems.append("%s: a point without an id" % name)
            continue
        if tid in teleports:
            problems.append("%s: teleport id %s also in %s" % (name, tid, teleports[tid][0]))
        teleports[tid] = (name, t)
        if "place" in t and t["place"] not in places:
            problems.append("%s: %s: place %s is not in map.json" % (name, tid, t["place"]))
        if "place" not in t and not ("x" in t and "y" in t):
            problems.append("%s: %s: neither place nor x, y" % (name, tid))
        if t.get("name"):
            need_key(t["name"], "%s: %s" % (name, tid))
        else:
            warnings.append("%s: %s has no name key (the panel shows the id)" % (name, tid))
        if "time" in t and not re.match(r"^\d{1,2}:\d{2}$", t["time"]):
            problems.append("%s: %s: time %r is not HH:MM" % (name, tid, t["time"]))
        group = t.get("group", os.path.splitext(name)[0])
        if "dev.group.%s" % group not in strings["en"]:
            warnings.append("%s: group %s has no dev.group.%s key" % (name, group, group))
# the city plan's points and the code's moving points also answer to an id
city = os.path.join(CONTENT, "City", "city_plan.json")
plan_ids = set()
if os.path.exists(city):
    plan = load(city)
    for field in ["plots", "spots", "promenade_units"]:
        plan_ids |= {p["id"] for p in plan.get(field, [])}
fest_path = os.path.join(CONTENT, "PeopleData", "festivals.json")
festivals = {f["id"] for f in load(fest_path).get("festivals", [])} if os.path.exists(fest_path) else set()
code_teleports = set(re.findall(r'FAutoTeleport \w+\(TEXT\("([^"]+)"\)', code))
known_points = set(teleports) | set(places) | plan_ids | code_teleports

# --- the quests ----------------------------------------------------------------------------------------------------------
quests = load(os.path.join(CONTENT, "UI", "quests.json"))["quests"]
quest_ids = {q["id"] for q in quests}


def check_target(event, target, where):
    if event == "reach":
        p = places.get(target)
        if not p:
            problems.append("%s: reach %s: no such place in map.json" % (where, target))
        elif not p.get("r"):
            problems.append("%s: reach %s: the place has radius 0, it can never be reached" % (where, target))
    elif event in ("get", "have", "use", "wear"):
        if target.endswith("*"):
            if not any(i.startswith(target[:-1]) for i in items):
                problems.append("%s: %s %s matches no item" % (where, event, target))
        elif target and target not in items:
            problems.append("%s: %s %s: no such item (item.%s.name)" % (where, event, target, target))
    elif event == "talk":
        if target and target.lower() not in people:
            problems.append("%s: talk %s: nobody of that key or dialogue section" % (where, target))
    elif event in ("festival", "festival_visit", "festival_watch"):
        # <festival id>[.<part or show>]: the festival must exist (the show names live in the festival code)
        fest = target.split(".")[0]
        if fest and fest not in festivals:
            problems.append("%s: %s %s: no festival %s in festivals.json" % (where, event, target, fest))


for q in quests:
    qid = q["id"]
    need_key("quest.%s.title" % qid, qid)
    need_key("quest.%s.desc" % qid, qid)
    need_key("quest.%s.done" % qid, qid)
    start = q.get("start_on")
    if start:
        if start["event"] not in events:
            problems.append("%s: starts on %s, an event the game never sends" % (qid, start["event"]))
        else:
            check_target(start["event"], start.get("target", ""), qid + " start")
    elif not q.get("auto_start") and not any(qid in s.get("reward", {}).get("start", []) for o in quests for s in o["stages"]):
        warnings.append("%s: nothing starts it (no auto_start, start_on or reward start)" % qid)
    stage_ids = set()
    for s in q["stages"]:
        where = "%s/%s" % (qid, s["id"])
        if s["id"] in stage_ids:
            problems.append("%s: stage id twice" % where)
        stage_ids.add(s["id"])
        need_key("quest.%s.%s.log" % (qid, s["id"]), where)
        if not s.get("objectives"):
            problems.append("%s: no objectives" % where)
        for o in s.get("objectives", []):
            need_key("quest.%s.%s.obj" % (qid, o["id"]), where)
            for ev in o["event"].split("|"):
                if ev not in events:
                    problems.append("%s/%s: event %s is never sent by the game" % (where, o["id"], ev))
                else:
                    check_target(ev, o.get("target", ""), "%s/%s" % (where, o["id"]))
            if "place" in o and o["place"] not in places:
                problems.append("%s/%s: map marker place %s not in map.json" % (where, o["id"], o["place"]))
        reward = s.get("reward", {})
        for it in reward.get("items", {}):
            if it not in items:
                problems.append("%s: reward item %s does not exist" % (where, it))
        for st in reward.get("start", []):
            if st not in quest_ids:
                problems.append("%s: reward starts unknown quest %s" % (where, st))
        dev = s.get("dev")
        if dev:
            if dev.get("tp") and dev["tp"] not in known_points:
                problems.append("%s: dev tp %s is not a teleport point" % (where, dev["tp"]))
            for it in dev.get("give", {}):
                if it not in items:
                    problems.append("%s: dev give %s does not exist" % (where, it))
            if "time" in dev and not re.match(r"^\d{1,2}:\d{2}$", dev["time"]):
                problems.append("%s: dev time %r is not HH:MM" % (where, dev["time"]))

# --- the C++ registrations -----------------------------------------------------------------------------------------------
seen = {}
for path, text in sources:
    if not path.endswith(".cpp"):
        continue  # the header's examples
    for kind, tid, group, key in re.findall(r'(FAutoAction|FAutoTeleport) \w+\(TEXT\("([^"]+)"\), TEXT\("([^"]+)"\), TEXT\("([^"]+)"\)', text):
        if (kind, tid) in seen:
            problems.append("%s: %s %s also in %s" % (path, kind, tid, seen[(kind, tid)]))
        seen[(kind, tid)] = path
        need_key(key, "%s %s" % (path, tid))
        if "dev.group.%s" % group not in strings["en"]:
            warnings.append("%s: %s group %s has no dev.group.%s key" % (path, tid, group, group))
    for quest, stage in re.findall(r'FAutoQuestSetup \w+\(TEXT\("([^"]+)"\), TEXT\("([^"]+)"\)', text):
        q = next((x for x in quests if x["id"] == quest), None)
        if not q or stage not in [s["id"] for s in q["stages"]]:
            problems.append("%s: quest setup for %s/%s, no such stage" % (path, quest, stage))

print("DEVDATA %d quests, %d teleport points in files, %d code registrations, %d problems, %d warnings"
      % (len(quests), len(teleports), len(seen), len(problems), len(warnings)))
for p in problems:
    print("  PROBLEM " + p)
for w in sorted(set(warnings)):
    print("  warning " + w)
sys.exit(1 if problems or ("--strict" in sys.argv and warnings) else 0)
