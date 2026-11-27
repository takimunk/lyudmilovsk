#!/usr/bin/env python3
"""Builds the friends among the residents (docs/friends.md) from tools/friends/content.py.

Writes:
  unreal/Lyudmilovsk/Content/PeopleData/friends.json                  the structure (keys only), read by ULyudFriends
  unreal/Lyudmilovsk/Content/Localization/friends/strings_<lang>.json  the words, merged by LyudText

Inputs besides content.py: Content/City/interiors.json (the hosts' chairs at their tables), Content/PeopleData/townsfolk.json
(the hosts who are townsfolk), Content/UI/quests.json (the hosts' home quests), Content/Letters/letters.json (their letters'
addressees). The town-talk lines of friends are tools/barks/lines/13_friends.txt (built by tools/barks/build_barks.py).

    python3 tools/friends/build_friends.py           # build
    python3 tools/friends/build_friends.py --check   # build into memory and fail if the committed files differ
"""
import argparse
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import content as C  # noqa: E402

LANGS = ("ru", "en", "ja")

RULES = {"levels": [0, 10, 30, 70, 130], "greet": 2, "heard": 1, "heard_cap": 3, "visit": 4, "quest": 20,
         "gift_loved": 12, "gift_liked": 6, "gift_plain": 2, "card": 8, "bench": 6, "tea": 12, "outing": 12, "festival": 8, "festival_level": 3, "chat": 1, "chat_answer": 1, "chat_cap": 2,
         "tea_level": 2, "tea_every": 7, "tea_ahead": 2, "outing_level": 4, "outing_every": 21, "bench_level": 3,
         "wave_level": 2, "nick_level": 3}

# the ten open homes (docs/city_interiors.md "Stories"): where the hero sits for tea (a seat whose act is a plain "sit",
# so E seats him), where the host sits if the town's life has them (their body is drawn there), the hour
PLACES = {
    "holm":    dict(person="", plot="P-T1H2", chair="P-T1H2/flat_armchair", host_chair="", hour=17),
    "okada":   dict(person="", plot="P-T4H1", chair="P-T4H1/flat_armchair", host_chair="", hour=17),
    "singh":   dict(person="", plot="P-CAPSULE", chair="P-CAPSULE/capsule_desk_chair", host_chair="", hour=18),
    "kravets": dict(person="", plot="P-T5H1", chair="P-T5H1/capsule_desk_chair", host_chair="", hour=18),
    "okafor":  dict(person="", plot="P-T1H4", chair="P-T1H4/studio_guest_chair", host_chair="", hour=17),
    "tadesse": dict(person="t_tadesse_valentina", plot="P-T1H3", chair="P-T1H3/studio_kitchen_chair",
                    host_chair="P-T1H3/studio_stool", hour=16),
    "okonkwo": dict(person="t_okonkwo_yasmin", plot="P-T3H1", chair="P-T3H1/studio_kitchen_chair",
                    host_chair="P-T3H1/studio_stool", hour=18),
    "santos":  dict(person="", plot="P-T1H5", chair="P-T1H5/home_dining_chair", host_chair="P-T1H5/home_dining_chair_head", hour=18),
    "girma":   dict(person="", plot="P-RISER1", chair="P-RISER1/home_dining_chair", host_chair="", hour=17),
    "aoki":    dict(person="", plot="P-RISER2", chair="P-RISER2/home_dining_chair", host_chair="", hour=17),
}
OUTING_ACTS = ["sea_air", "stroll", "fair_visit", "feed_birds", "eat_out", "coffee"]
# the gift reactions are said over the friend's head by town talk (ULyudTownTalkSubsystem::SayLine): line.* keys
LINE_KEYS = re.compile(r"^friends\.gift\.(loved|liked|plain|again)\.\d+$")


def load(rel):
    with open(os.path.join(CONTENT, rel), encoding="utf-8") as f:
        return json.load(f)


def build():
    problems = []
    things = {t["id"]: t for t in load("City/interiors.json")["things"]}
    townsfolk = {p["key"]: p for p in load("PeopleData/townsfolk.json")["people"]}
    qdata = load("UI/quests.json")
    quests = {q["id"] for q in (qdata["quests"] if isinstance(qdata, dict) else qdata)}
    letters = load("Letters/letters.json")
    correspondents = {c["id"]: c for c in letters["correspondents"]}
    acts = {a["id"] for a in load("PeopleData/activities.json")["activities"]}
    from_pocket = set()
    try:
        inv = open(os.path.join(ROOT, "unreal", "Lyudmilovsk", "Source", "Lyudmilovsk", "Private", "LyudInventory.cpp"), encoding="utf-8").read()
        from_pocket = set(re.findall(r'TEXT\("([a-z0-9_]+)"\)', inv))
    except OSError:
        problems.append("no LyudInventory.cpp to check the gifts against")

    hosts = []
    for h in C.HOSTS:
        hid = h["id"]
        pl = PLACES.get(hid)
        if not pl:
            problems.append("host %s: no place in PLACES" % hid)
            continue
        e = {"id": hid, "name": h.get("name", "friends.host.%s.name" % hid), "person": pl["person"], "plot": pl["plot"],
             "quest": "home_" + hid if "home_" + hid in quests else "", "correspondent": "h_" + hid if "h_" + hid in correspondents else "",
             "hour": pl["hour"], "topics": h.get("topics", 3)}
        if pl["person"]:
            if pl["person"] not in townsfolk:
                problems.append("host %s: no townsperson %s" % (hid, pl["person"]))
            elif not townsfolk[pl["person"]]["home"].startswith(pl["plot"] + "/"):
                problems.append("host %s: %s doesn't live in %s" % (hid, pl["person"], pl["plot"]))
        for field in ("chair", "host_chair"):
            tid = pl[field]
            if not tid:
                e[field] = ""
                continue
            t = things.get(tid)
            if not t or t.get("kind") != "seat":
                problems.append("host %s: %s %s is not a seat of interiors.json" % (hid, field, tid))
                continue
            e[field] = tid
            e[field + "_ue"] = [round(v, 1) for v in t["ue"]]
            e[field + "_yaw"] = round(float(t.get("yaw", 0.0)), 1)
            e[field + "_height"] = float(t.get("props", {}).get("height", 0.45))
        e["table"] = ""
        hosts.append(e)
    if len(hosts) != 10:
        problems.append("%d hosts, not the ten open homes" % len(hosts))

    gifts = []
    for item, hints in C.GIFTS:
        if from_pocket and item not in from_pocket:
            problems.append("gift %s: no such item in LyudInventory.cpp" % item)
        gifts.append({"item": item, "hints": list(hints)})
    for a in OUTING_ACTS:
        if a not in acts:
            problems.append("outing %s: no such activity" % a)
    jobs = {}
    for p in townsfolk.values():
        j = p.get("occupation", "")
        if j and j not in jobs:
            jobs[j] = "friends.job." + j

    data = {
        "_doc": "Written by tools/friends/build_friends.py from tools/friends/content.py: do not edit by hand. Friends among the "
                "residents: docs/friends.md.",
        "rules": RULES,
        "nicknames": [{"id": n, "hint": hint} for n, hint in C.NICKNAMES],
        "gifts": gifts,
        "hosts": hosts,
        "outing_acts": OUTING_ACTS,
        "jobs": dict(sorted(jobs.items())),
    }

    # every key the structure and the game name has its words; the same placeholders in the three languages
    needed = set(jobs.values())
    needed |= {"friends.nick." + n for n, _ in C.NICKNAMES}
    for h in hosts:
        needed |= {h["name"], "friends.tea.%s.welcome" % h["id"], "friends.tea.%s.bye" % h["id"], "friends.tea.%s.invite" % h["id"]}
        for i in range(1, h["topics"] + 1):
            needed |= {"friends.tea.%s.q%d" % (h["id"], i), "friends.tea.%s.a%d" % (h["id"], i)}
    needed |= {"friends.outing.invite." + a for a in OUTING_ACTS}
    needed |= {"friends.day.in.%d" % d for d in range(7)}
    needed |= {"friends.level.%d" % n for n in range(1, 5)}
    needed |= {"friends.diary.title", "friends.diary.empty", "friends.fact.home", "friends.fact.job", "friends.fact.nick",
               "friends.fact.likes", "friends.fact.cards", "friends.fact.teas", "friends.fact.outing", "friends.fact.met",
               "friends.fact.family", "friends.tea.prompt", "friends.tea.title", "friends.tea.keys", "friends.tea.not_now",
               "friends.tea.diary", "friends.outing.diary", "friends.outing.toast", "friends.festival.toast", "friends.festival.diary", "friends.gift.prompt", "friends.gift.title",
               "friends.gift.keys", "friends.gift.none", "friends.gift.diary", "friends.bench.toast", "friends.know.toast",
               "friends.gift.loved.1", "friends.gift.liked.1", "friends.gift.plain.1", "friends.gift.again.1",
               "friends.tea.served.1", "dev.group.friends"}
    for k in sorted(needed):
        if k not in C.STRINGS:
            problems.append("no words for %s" % k)
    for k, tr in C.STRINGS.items():
        marks = {lang: sorted(set(re.findall(r"\{\d\}", tr[lang]))) for lang in LANGS}
        if len({tuple(m) for m in marks.values()}) != 1:
            problems.append("placeholders differ in %s: %s" % (k, marks))
        for lang in LANGS:
            if not tr[lang].strip():
                problems.append("empty %s %s" % (lang, k))
    tables = {}
    for lang in LANGS:
        t = {}
        for k, tr in C.STRINGS.items():
            t[("line." + k) if LINE_KEYS.match(k) else k] = tr[lang]
        tables[lang] = t
    return data, tables, problems


def dumps(obj, indent):
    return json.dumps(obj, ensure_ascii=False, indent=indent) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    data, tables, problems = build()
    for p in problems:
        print("PROBLEM", p, file=sys.stderr)
    outputs = {os.path.join(CONTENT, "PeopleData", "friends.json"): dumps(data, 1)}
    for lang, table in tables.items():
        outputs[os.path.join(CONTENT, "Localization", "friends", "strings_%s.json" % lang)] = dumps(table, 1)
    summary = "friends: %d keys, %d hosts, %d gifts, %d nicknames" % (len(tables["ru"]), len(data["hosts"]), len(data["gifts"]),
                                                                       len(data["nicknames"]))
    if args.check:
        stale = []
        for path, text in outputs.items():
            try:
                with open(path, encoding="utf-8") as f:
                    if f.read() != text:
                        stale.append(path)
            except OSError:
                stale.append(path)
        for s in stale:
            print("STALE", os.path.relpath(s, ROOT), file=sys.stderr)
        print("%s; %d problems, %d stale" % (summary, len(problems), len(stale)))
        return 1 if problems or stale else 0
    for path, text in outputs.items():
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            f.write(text)
    print("%s; %d problems" % (summary, len(problems)))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
