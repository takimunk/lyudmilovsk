#!/usr/bin/env python3
"""Builds the town's post (docs/letters.md) from tools/letters/content.py.

Writes:
  unreal/Lyudmilovsk/Content/Letters/letters.json                    the structure (keys only), read by ULyudLetters
  unreal/Lyudmilovsk/Content/Localization/letters/strings_<lang>.json the words, merged by LyudText

Inputs besides content.py: Content/City/interiors.json (the kiosks' postcard racks the postboxes stand by, the open homes'
shelves the hero's cards stand on), Content/City/city_plan.json (the terraces, plots and wall dwellings: postal
addresses), Content/UI/quests.json and Content/PeopleData/festivals.json (checked: every quest and holiday named exists).

    python3 tools/letters/build_letters.py           # build
    python3 tools/letters/build_letters.py --check   # build into memory and fail if the committed files differ
"""
import argparse
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import content as C  # noqa: E402

LANGS = ("ru", "en", "ja")

# the postboxes: a kiosk's postcard rack (or a spot of the plan) and where to try to stand the box from it, in the
# thing's frame (cm forward along its yaw, cm to its left); the game tries the first offsets that fit (ULyudLetters)
POSTBOXES = [
    ("pb_kiosk1", "thing", "P-KIOSK1/postcards"),
    ("pb_kiosk2", "thing", "P-KIOSK2/postcards"),
    ("pb_kiosk3", "thing", "P-KIOSK3/postcards"),
    ("pb_kiosk4", "thing", "P-KIOSK4/postcards"),
    ("pb_kiosk5", "thing", "P-KIOSK5/postcards"),
    ("pb_kiosk6", "thing", "P-KIOSK6/postcards"),
    ("pb_square", "spot", "S-SQUARE"),
    ("pb_sanatorium", "thing", "P-SANATORIUM/brochure_rack"),
    ("pb_hall", "thing", "P-HALL/foyer_postcards"),
]
# the club's own mailbox: on the club's portal face beside the door (map metres, docs/world_map.md)
MAILBOX = {"map": [1.6, 0.0], "yaw": 90.0, "door": [-1.9, 6.0]}

POST = {"collect": 17, "deliver_town": 11, "deliver_hero": 10, "pen_travel": 3, "pen_start_day": 2, "pen_start_late": 5,
        "pen_nudge_days": 10, "invite_after": 2, "festival_notice": 2, "thanks_window": 4}


def load(rel):
    with open(os.path.join(CONTENT, rel), encoding="utf-8") as f:
        return json.load(f)


def homes_from_plan(plan):
    """Postal addresses: each terrace is a street; its plots numbered 1.. along it; a wall dwelling is the wall's own street
    with its door number. A building's flats take the building's entry plus the flat (LettersModel's AddressOf)."""
    homes = {}
    by_terrace = {}
    for p in plan["plots"]:
        by_terrace.setdefault(p.get("terrace", ""), []).append(p)
    for terrace, plots in by_terrace.items():
        plots.sort(key=lambda p: (p.get("s", 0.0), p["id"]))
        for n, p in enumerate(plots, 1):
            homes[p["id"]] = [terrace, str(n)]
    for d in plan["wall_dwellings"]:
        wall, _, num = d["id"].partition("-D")
        homes[d["id"]] = [wall, str(int(num)) if num.isdigit() else num]
    for u in plan.get("promenade_units", []):
        wall, _, num = u["id"].partition("-U")
        homes.setdefault(u["id"], [wall, str(int(num)) if num.isdigit() else num])
    return homes


def build():
    problems = []
    interiors = load("City/interiors.json")
    things = {t["id"]: t for t in interiors["things"]}
    plan = load("City/city_plan.json")
    spots = {s["id"]: s for s in plan["spots"]}
    qdata = load("UI/quests.json")
    quests = {q["id"] for q in (qdata["quests"] if isinstance(qdata, dict) else qdata)}
    festivals = {f["id"] for f in load("PeopleData/festivals.json")["festivals"]}
    townsfolk = {p["key"] for p in load("PeopleData/townsfolk.json")["people"]}

    postboxes = []
    for pid, kind, ref in POSTBOXES:
        src = things.get(ref) if kind == "thing" else spots.get(ref)
        if not src:
            problems.append("postbox %s: no %s %s" % (pid, kind, ref))
            continue
        postboxes.append({"id": pid, "ue": [round(v, 1) for v in src["ue"]], "yaw": round(float(src.get("yaw", 0.0)), 1),
                          "near": ref})

    correspondents = []
    for c in C.CORRESPONDENTS:
        t = things.get(c["shelf"])
        if not t:
            problems.append("correspondent %s: no thing %s" % (c["id"], c["shelf"]))
            continue
        if c["person"] and c["person"] not in townsfolk:
            problems.append("correspondent %s: no townsperson %s" % (c["id"], c["person"]))
        e = {"id": c["id"], "name": c["name"], "home": c["home"], "person": c["person"],
             "near": [round(v, 1) for v in t["ue"]], "near_radius": 700.0,
             "shelf": [round(v, 1) for v in t["ue"]], "shelf_yaw": round(float(t.get("yaw", 0.0)), 1),
             "replies": c["replies"], "invite": c["invite"], "shelf_note": c["shelf_note"]}
        correspondents.append(e)

    thanks = []
    for quest, who, body, delay in C.THANKS:
        if quest not in quests:
            problems.append("thanks: no quest %s" % quest)
        thanks.append({"quest": quest, "from": who, "body": body, "delay": delay})
    for fid in C.INVITE_FESTIVALS:
        if fid not in festivals:
            problems.append("invite: no festival %s" % fid)

    data = {
        "_doc": "Written by tools/letters/build_letters.py from tools/letters/content.py: do not edit by hand. The town's post: "
                "docs/letters.md.",
        "post": POST,
        "pictures": [p[0] for p in C.PICTURES],
        "openings": [p[0] for p in C.OPENINGS],
        "wishes": [p[0] for p in C.WISHES],
        "replies": C.REPLIES,
        "thanks_lines": C.THANKS_LINES,
        "postboxes": postboxes,
        "mailbox": MAILBOX,
        "correspondents": correspondents,
        "thanks": thanks,
        "invites": {"generic": C.INVITE_GENERIC, "festivals": C.INVITE_FESTIVALS},
        "pen": {"who": "letters.pen.who", "place": "letters.pen.place", "nudge": C.PEN_NUDGE,
                "answer_none": C.PEN_ANSWERS["none"],
                "letters": [{"body": p["body"], "diary": p["diary"], "ask": p["ask"]} for p in C.PEN],
                "acks": C.PEN_ACKS, "answers": C.PEN_ANSWERS},
        "homes": homes_from_plan(plan),
    }

    # every key the structure names has its words; every placeholder the same in the three languages
    def keys_in(v):
        if isinstance(v, str):
            if v.startswith("letters.") or v.startswith("line.letters."):
                yield v
        elif isinstance(v, list):
            for x in v:
                yield from keys_in(x)
        elif isinstance(v, dict):
            for x in v.values():
                yield from keys_in(x)
    for k in keys_in(data):
        if k not in C.STRINGS:
            problems.append("no words for %s" % k)
    import re
    for k, tr in C.STRINGS.items():
        marks = {lang: sorted(set(re.findall(r"\{\d\}", tr[lang]))) for lang in LANGS}
        if len({tuple(m) for m in marks.values()}) != 1:
            problems.append("placeholders differ in %s: %s" % (k, marks))
        for lang in LANGS:
            if not tr[lang].strip():
                problems.append("empty %s %s" % (lang, k))
    tables = {lang: {k: tr[lang] for k, tr in C.STRINGS.items()} for lang in LANGS}
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
    outputs = {os.path.join(CONTENT, "Letters", "letters.json"): dumps(data, 1)}
    for lang, table in tables.items():
        outputs[os.path.join(CONTENT, "Localization", "letters", "strings_%s.json" % lang)] = dumps(table, 1)
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
        print("letters: %d keys, %d correspondents, %d pen letters, %d postboxes; %d problems, %d stale" % (
            len(tables["ru"]), len(data["correspondents"]), len(data["pen"]["letters"]), len(data["postboxes"]), len(problems), len(stale)))
        return 1 if problems or stale else 0
    for path, text in outputs.items():
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            f.write(text)
    print("letters: %d keys, %d correspondents, %d pen letters, %d postboxes, %d homes; %d problems" % (
        len(tables["ru"]), len(data["correspondents"]), len(data["pen"]["letters"]), len(data["postboxes"]), len(data["homes"]), len(problems)))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
