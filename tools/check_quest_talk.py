#!/usr/bin/env python3
"""Checks the quests' spoken side: every `quest.` line of Content/Dialogue/npc_dialogue.txt and every entry of
Content/UI/quest_scenes.json (docs/quests.md). Offline, no Unreal.

  python3 tools/check_quest_talk.py        # prints the problems, exit code 1 if any

What it looks at: the quest and its stages exist (`done`, `<stage>_empty` for a stage with a `bring`); the conditions of a
variant (`.if.<cond>+<cond>`) are known words, items and quests; a variant has a plain line of the same quest to stand in
for; a variant that holds for ever (`done:`) has its own `.again` when the plain talk has some; a `done` line has no `.again`;
every line key has words in ru, en and ja; and the people, species, quests, stages and lines of `listeners`, `pickups` and
`remarks`.
"""
import glob
import json
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "unreal", "Lyudmilovsk", "Content")
SUN = {"night", "low", "day", "cloudy"}


def load_strings(lang):
    out = {}
    for path in glob.glob(os.path.join(ROOT, "Localization", "**", "strings_%s.json" % lang), recursive=True):
        with open(path, encoding="utf-8") as f:
            data = json.load(f)
        out.update(data.get("strings", data) if isinstance(data, dict) else {})
    return out


def main():
    problems = []
    with open(os.path.join(ROOT, "UI", "quests.json"), encoding="utf-8") as f:
        quests = {q["id"]: q for q in json.load(f)["quests"]}
    strings = {lang: load_strings(lang) for lang in ("ru", "en", "ja")}
    with open(os.path.join(ROOT, "PeopleData", "people.json"), encoding="utf-8") as f:
        people_data = json.load(f)
    people = set()
    for p in people_data.get("people", people_data if isinstance(people_data, list) else []):
        people.add((p.get("key") or p.get("id") or "").lower())

    def stages_of(quest, with_empty):
        out = {"done"}
        for s in quests[quest]["stages"]:
            out.add(s["id"])
            if with_empty and any(o["event"] == "bring" for o in s["objectives"]):
                out.add(s["id"] + "_empty")
        return out

    def check_keys(where, keys):
        for key in keys:
            full = key if key.startswith("line.") else "line." + key
            for lang, table in strings.items():
                if not table.get(full):
                    problems.append("%s: %s has no words in %s" % (where, full, lang))

    def check_cond(where, cond):
        if ":" not in cond:
            if cond not in SUN:
                problems.append("%s: condition '%s' is not one of %s" % (where, cond, ", ".join(sorted(SUN))))
            return
        kind, arg = cond.split(":", 1)
        if kind in ("have", "no"):
            if not strings["ru"].get("item.%s.name" % arg):
                problems.append("%s: condition %s: no item '%s'" % (where, cond, arg))
        elif kind in ("done", "on"):
            if arg not in quests:
                problems.append("%s: condition %s: no quest '%s'" % (where, cond, arg))
        elif kind != "flag":
            problems.append("%s: condition '%s' is not known (have, no, done, on, flag)" % (where, cond))

    # ---- npc_dialogue.txt
    section = None
    plain = set()      # (section, quest, stage) with a plain line
    variants = []      # (where, section, quest, stages)
    plain_again = set()
    variant_keys = {}
    variant_again = set()
    path = os.path.join(ROOT, "Dialogue", "npc_dialogue.txt")
    with open(path, encoding="utf-8") as f:
        for n, raw in enumerate(f, 1):
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            m = re.match(r"^\[(.+)\]$", line)
            if m:
                section = m.group(1).strip()
                continue
            if "=" not in line:
                continue
            key, value = [x.strip() for x in line.split("=", 1)]
            if not key.startswith("quest.") and not key.startswith("aside."):
                continue
            where = "npc_dialogue.txt:%d [%s] %s" % (n, section, key)
            parts = key.split(".")
            if len(parts) < 3:
                problems.append("%s: needs quest.<quest>.<stage>" % where)
                continue
            quest, stage_list, tail = parts[1], [s.strip() for s in parts[2].split(",")], parts[3:]
            if quest not in quests:
                problems.append("%s: no quest '%s'" % (where, quest))
                continue
            for stage in stage_list:
                if stage not in stages_of(quest, True):
                    problems.append("%s: quest %s has no stage '%s'" % (where, quest, stage))
            conds = []
            if tail and tail[0] == "if":
                if len(tail) < 2:
                    problems.append("%s: .if needs a condition" % where)
                    continue
                # a condition may hold dots itself (flag:remark.<id>): everything up to a last ".again"
                again = tail[-1:] == ["again"]
                conds = ".".join(tail[1:-1] if again else tail[1:]).split("+")
                tail = ["again"] if again else []
                for cond in conds:
                    check_cond(where, cond)
            if tail and tail != ["again"]:
                problems.append("%s: '%s' after the stages is not .again or .if.<cond>" % (where, ".".join(tail)))
            if "done" in stage_list and tail == ["again"]:
                problems.append("%s: a done line with .again is said for ever in front of the everyday talks" % where)
            if conds and not tail:
                variant_keys[(section + key[:5], quest, tuple(stage_list), tuple(conds))] = where
            if conds and tail:
                variant_again.add((section + key[:5], quest, tuple(stage_list), tuple(conds)))
            if not conds and tail:
                for stage in stage_list:
                    plain_again.add((section + key[:5], quest, stage))
            if conds:
                variants.append((where, section + key[:5], quest, stage_list))
            elif not tail:
                for stage in stage_list:
                    plain.add((section + key[:5], quest, stage))
            keys = [k.strip() for k in value.split("|") if k.strip()]
            if not keys:
                problems.append("%s: no lines" % where)
            check_keys(where, keys)
    variant_info = [(w, k[0], k[1], list(k[2]), list(k[3]), k in variant_again) for k, w in variant_keys.items()]
    for where, sec, quest, stage_list, conds, has_again in variant_info:
        if conds and all(c.startswith("done:") for c in conds) and not has_again and "done" not in stage_list:
            if any((sec, quest, st) in plain_again for st in stage_list):
                problems.append("%s: a variant that holds for ever (%s) hides the plain talk's .again lines: give it its own .again" % (where, "+".join(conds)))
    for where, sec, quest, stage_list in variants:
        for stage in stage_list:
            if (sec, quest, stage) not in plain:
                problems.append("%s: no plain line of %s.%s in [%s] for the variant to stand in for" % (where, quest, stage, sec))

    # ---- quest_scenes.json
    with open(os.path.join(ROOT, "UI", "quest_scenes.json"), encoding="utf-8") as f:
        scenes = json.load(f)

    def check_stage(where, quest, stages):
        if quest not in quests:
            problems.append("%s: no quest '%s'" % (where, quest))
            return
        for stage in stages:
            if stage not in stages_of(quest, False):
                problems.append("%s: quest %s has no stage '%s'" % (where, quest, stage))

    species = set()
    try:
        with open(os.path.join(ROOT, "BirdsData", "species.json"), encoding="utf-8") as f:
            sd = json.load(f)
        for sp in (sd.get("species", sd) if isinstance(sd, dict) else sd):
            species.add(sp.get("id") if isinstance(sp, dict) else sp)
    except OSError:
        pass
    for p in scenes.get("pickups", []):
        where = "quest_scenes.json pickup %s" % p.get("id")
        check_stage(where, p.get("quest"), p.get("stages", []))
        if not strings["ru"].get("item.%s.name" % p.get("item")):
            problems.append("%s: no item '%s'" % (where, p.get("item")))
    for listener in scenes.get("listeners", []):
        where = "quest_scenes.json listener %s" % listener.get("person")
        if people and listener.get("person") not in people:
            problems.append("%s: nobody of that key in people.json" % where)
        for r in listener.get("reactions", []):
            if r.get("species") and species and r["species"] not in species:
                problems.append("%s: no species '%s' in BirdsData/species.json" % (where, r["species"]))
            for key in r.get("lines", []):
                if key.replace("line.", "", 1).split(".")[0] != listener.get("person"):
                    problems.append("%s: %s is not their own line" % (where, key))
            if r.get("quest"):
                check_stage(where, r["quest"], [r.get("stage")])
            check_keys(where, r.get("lines", []))
    def check_note(where, r):
        note = r.get("note")
        if note:
            for lang, table in strings.items():
                if not table.get(note):
                    problems.append("%s: note %s has no words in %s" % (where, note, lang))

    for listener in scenes.get("listeners", []):
        for r in listener.get("reactions", []):
            check_note("quest_scenes.json listener %s" % listener.get("person"), r)
    seen = set()
    for r in scenes.get("remarks", []):
        check_note("quest_scenes.json remark %s" % r.get("id"), r)
        where = "quest_scenes.json remark %s" % r.get("id")
        if not r.get("id") or r["id"] in seen:
            problems.append("%s: needs an id of its own" % where)
        seen.add(r.get("id"))
        if people and r.get("person") not in people:
            problems.append("%s: nobody '%s' in people.json" % (where, r.get("person")))
        check_stage(where, r.get("quest"), r.get("stages", []))
        if not r.get("lines"):
            problems.append("%s: no lines" % where)
        for key in r.get("lines", []):
            speaker = key.replace("line.", "", 1).split(".")[0]
            if speaker != r.get("person"):
                problems.append("%s: %s is not %s's own line" % (where, key, r.get("person")))
        check_keys(where, r.get("lines", []))

    for p in problems:
        print(p)
    print("check_quest_talk: %d quests, %d problems" % (len(quests), len(problems)))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
