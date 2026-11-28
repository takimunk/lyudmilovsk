#!/usr/bin/env python3
"""A talk with a townsperson (docs/town_talk.md "A talk"): builds the game's files from tools/townchat/content.py and the
given names of tools/town/names.json:
  unreal/Lyudmilovsk/Content/PeopleData/town_chat.json                 (rules, groups, questions: LyudTownChat::FData)
  unreal/Lyudmilovsk/Content/Localization/townchat/strings_<lang>.json (the hero's questions and answers, the page's words,
                                                                        the given names' case forms: townchat.fn.<id>.<form>)
and checks them against the game's data: activities, occupations and likes that exist, every on = tc_* of the town talk's
lines (tools/barks/lines/18_townchat.txt) known and with a line for anybody, the names in a line the same in ru, en and
ja, and a quest's own lines for a resident (line.<resident key>.<quest>.<stage>.<n> in Localization/quests) naming a
resident, a quest and a stage that exist, numbered from 1 without a gap.

    python3 tools/townchat/build_town_chat.py            # writes the files
    python3 tools/townchat/build_town_chat.py --check    # fails if they are stale or anything is wrong
Their own lines are built by tools/barks/build_barks.py; run tools/townchatsim/run.sh after both.
"""
import collections
import glob
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import content  # noqa: E402
import names_ru  # noqa: E402

D = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content")
LANGS = ["ru", "en", "ja"]
CHECK = "--check" in sys.argv
problems = []


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


# ---- the given names' forms
given = load(os.path.join(REPO, "tools", "town", "names.json"))["given"]
given = given + [{"id": i, "sex": sex, "ru": ru, "en": en, "ja": ja} for i, sex, ru, en, ja in content.EXTRA_NAMES]
strings = collections.OrderedDict()     # key -> (ru, en, ja)
for key, words in content.UI.items():
    strings[key] = words
for topic, (grown, young) in content.TOPICS.items():
    strings["townchat.topic." + topic] = grown
    strings["townchat.topic." + topic + ".young"] = young
for ask in content.ASKS:
    for n, words in enumerate(ask["answers"]):
        strings["townchat.ask.%s.a%d" % (ask["id"], n + 1)] = words
name_ids = []
for g in given:
    forms = names_ru.decline(g["ru"], g["sex"])
    name_ids.append(g["id"])
    for form, ru in zip(content.FORMS, forms):
        # English and Japanese have no cases: the line's own words carry them (with {6}, {6}さんと)
        strings["townchat.fn.%s.%s" % (g["id"], form)] = (ru, g["en"], g["ja"])
if len(set(name_ids)) != len(name_ids):
    problems.append("names.json: a given name's id twice")

# ---- what the game knows
acts = {a["id"] for a in load(os.path.join(D, "PeopleData", "activities.json"))["activities"]}
townsfolk = load(os.path.join(D, "PeopleData", "townsfolk.json"))["people"]
jobs = {p.get("occupation") for p in townsfolk}
likes = {k for p in townsfolk for k in p.get("likes", {})}
keys = {p["key"] for p in townsfolk}
did = collections.OrderedDict()
for group, ids in content.DID.items():
    for a in ids:
        if a not in acts:
            problems.append("DID %s: no activity %s in activities.json" % (group, a))
        if a in did:
            problems.append("DID: activity %s in two groups" % a)
        did[a] = group
for job in content.JOBS:
    if job not in jobs:
        problems.append("JOBS: nobody is a %s" % job)
for hobby in content.HOBBIES:
    if hobby not in likes:
        problems.append("HOBBIES: nobody likes %s" % hobby)
for p in townsfolk:
    if p["key"].rsplit("_", 1)[-1] not in name_ids:
        problems.append("%s: the key's last part is not a given name of names.json" % p["key"])
for topic in ["work." + v for v in set(content.JOBS.values()) | {"work"}] + ["kin." + k for k in ("kids", "parents", "sibling", "spouse", "mate")] \
        + ["day", "day.morning", "day.evening", "festival", "rumour", "weather", "hobby", "friend", "town"]:
    if topic not in content.TOPICS:
        problems.append("TOPICS: no title for %s" % topic)
for key, words in strings.items():
    if len(words) != 3 or any(not w.strip() for w in words):
        problems.append("%s needs ru, en and ja" % key)
    elif len({tuple(sorted(set(re.findall(r"\{\d+\}", w)))) for w in words}) != 1:
        problems.append("%s: the placeholders differ between the languages" % key)

# ---- their lines (tools/barks/lines/18_townchat.txt): the events, a line for anybody, the names
events = collections.defaultdict(list)     # on -> [(set name, [conditions], [(key, ru, en, ja)])]
src = os.path.join(REPO, "tools", "barks", "lines", "18_townchat.txt")
if os.path.exists(src):
    cur = None
    for raw in open(src, encoding="utf-8"):
        line = raw.rstrip("\n")
        if line.startswith("[") and line.endswith("]"):
            cur = {"name": line[1:-1], "conds": [], "lines": [], "on": ""}
        elif cur is not None and line.startswith("say "):
            cur["lines"].append([x.strip() for x in line[4:].split(" | ")])
        elif cur is not None and "=" in line and not line.startswith("##"):
            k, v = [x.strip() for x in line.split("=", 1)]
            if k == "on":
                cur["on"] = v
                events[v].append(cur)
            elif k != "weight":
                cur["conds"].append(k)
    for on, sets in events.items():
        if on not in content.EVENTS:
            problems.append("18_townchat.txt: on = %s is not an event of content.py" % on)
    for on in content.EVENTS:
        if not any(s["lines"] for s in events.get(on, [])):
            problems.append("18_townchat.txt: no lines with on = %s" % on)
    for on in content.ALWAYS:
        if events.get(on) and not any(not s["conds"] and s["lines"] for s in events[on]):
            problems.append("18_townchat.txt: on = %s has no set without conditions (a line for anybody, any time)" % on)
    named = {"tc_friend"} | {"tc_kin_" + k for k in content.KIN}
    for on, sets in events.items():
        for s in sets:
            for parts in s["lines"]:
                if len(parts) != 4:
                    continue
                key, ru, en, ja = parts
                used = [set(re.findall(r"\{(\d)\}", t)) for t in (ru, en, ja)]
                if any(u != used[0] for u in used):
                    problems.append("%s: the placeholders differ between ru, en and ja" % key)
                forms = {int(x) for x in used[0]} - {0, 1}
                if forms and on not in named:
                    problems.append("%s: a name ({2}..{7}) in a line that names nobody (on = %s)" % (key, on))
                if any(f > 7 for f in forms):
                    problems.append("%s: no such name form" % key)
                if "{1}" in ru and on != "tc_rumour":
                    problems.append("%s: {1} (a rumour's detail) outside on = tc_rumour" % key)
else:
    problems.append("no tools/barks/lines/18_townchat.txt")

# ---- a quest's own lines for a resident: line.<resident key>.<quest>.<stage>.<n> (Localization/quests)
quests = {q["id"]: {s["id"] for s in q["stages"]} for q in load(os.path.join(D, "UI", "quests.json"))["quests"]}
quest_lines = collections.defaultdict(dict)
for lang in LANGS:
    for path in glob.glob(os.path.join(D, "Localization", "**", "strings_%s.json" % lang), recursive=True):
        for k in load(path):
            m = re.match(r"^line\.(t_[a-z0-9]+_[a-z0-9]+)\.([a-z0-9_]+)\.([a-z0-9_]+)\.(\d+)$", k)
            if m:
                quest_lines[m.group(1, 2, 3)].setdefault(lang, set()).add(int(m.group(4)))
for (who, quest, stage), by_lang in sorted(quest_lines.items()):
    where = "line.%s.%s.%s" % (who, quest, stage)
    if who not in keys:
        problems.append("%s: no resident %s in townsfolk.json" % (where, who))
    if quest not in quests:
        problems.append("%s: no quest %s" % (where, quest))
    # "done": said once after the quest is finished (LyudFriendsTalk.cpp, the after-quest talk)
    elif stage not in quests[quest] | {s + "_empty" for s in quests[quest]} | {"done"}:
        problems.append("%s: quest %s has no stage %s" % (where, quest, stage))
    for lang in LANGS:
        ns = by_lang.get(lang, set())
        if ns != set(range(1, len(ns) + 1)) or ns != by_lang.get("ru", set()):
            problems.append("%s: the %s lines are not 1..n as in ru" % (where, lang))

# ---- write (or compare)
data = collections.OrderedDict()
data["_doc"] = "A talk with a townsperson (docs/town_talk.md). Built by tools/townchat/build_town_chat.py from content.py: do not edit."
data["rules"] = content.RULES
data["weights"] = content.WEIGHTS
data["did"] = did
data["jobs"] = content.JOBS
data["hobbies"] = content.HOBBIES
data["asks"] = [{k: v for k, v in (("id", a["id"]), ("answers", len(a["answers"])), ("min_age", a.get("min_age", 0)), ("max_age", a.get("max_age", 200)))}
                for a in content.ASKS]
data["names"] = sorted(name_ids)
outputs = {os.path.join(D, "PeopleData", "town_chat.json"): json.dumps(data, ensure_ascii=False, indent=1) + "\n"}
for i, lang in enumerate(LANGS):
    table = collections.OrderedDict((k, v[i]) for k, v in strings.items())
    outputs[os.path.join(D, "Localization", "townchat", "strings_%s.json" % lang)] = json.dumps(table, ensure_ascii=False, indent=1) + "\n"
stale = 0
for path, text in outputs.items():
    old = open(path, encoding="utf-8").read() if os.path.exists(path) else None
    if old != text:
        if CHECK:
            problems.append("%s is stale: run tools/townchat/build_town_chat.py" % os.path.relpath(path, REPO))
            stale += 1
        else:
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "w", encoding="utf-8") as f:
                f.write(text)

n_lines = sum(len(s["lines"]) for sets in events.values() for s in sets)
print("town chat: %d strings (%d names x %d forms), %d topics' titles, %d questions back; %d events, %d sets, %d lines of theirs; %d quest talks"
      % (len(strings), len(name_ids), len(content.FORMS), len(content.TOPICS) * 2, len(content.ASKS), len(events),
         sum(len(s) for s in events.values()), n_lines, len(quest_lines)))
for p in problems:
    print("PROBLEM", p)
sys.exit(1 if problems else 0)
