#!/usr/bin/env python3
"""Builds reading anywhere and the library's book club (docs/books.md "Reading anywhere", "The book club") from
tools/reading/content.py, tools/books/catalog.json and the town (Content/PeopleData/townsfolk.json):

  Content/Reading/reading.json                     the club (its evening, the 12 books, the members), the advice pairs
  Content/Localization/reading/strings_<lang>.json every word, the talk lines (line.town.bookclub.<work>.<n>), the hero's
                                                   lines, the titles and authors of every work (book.title.<work>)
  and, like tools/town/town_activities.py, the club's place in the town's life:
  Content/PeopleData/town_places.json              P-LIBRARY-CLUB: five seats round the library lounge's table
  Content/PeopleData/activities.json               the row "book_club" (only while the game's condition book_club is on)
  Content/PeopleData/townsfolk.json                the members' duty: Thursday 18:10-20:00 there

    python3 tools/reading/build_reading.py            write what changed
    python3 tools/reading/build_reading.py --check    exit 1 when anything is stale or a rule is broken
tools/town/town_life.py runs it after its own add-ons (a regenerated town picks the members again: the same seed, the same
people).
"""
import collections
import json
import os
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tools", "town"))
import content as C  # noqa: E402
import town_activities as TA  # noqa: E402

CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
OUT_JSON = os.path.join(CONTENT, "Reading", "reading.json")
OUT_STR = os.path.join(CONTENT, "Localization", "reading", "strings_%s.json")
CATALOG = os.path.join(ROOT, "tools", "books", "catalog.json")
LANGS = ("ru", "en", "ja")
OD = collections.OrderedDict

PLACE_ID = "P-LIBRARY-CLUB"
# a duty of the five members (Thursday, 18:10-20:00), kept only while the game has the condition book_club on (ULyudBookClub:
# not on a festival day; a public holiday skips it by "holiday": "off"); never chosen in free time
ACT_ROW = ('{"id": "book_club", "places": ["book_club"], "slots": ["club_seat"], "pose": "sit", "minutes": [100, 110], '
           '"when": "book_club", "holiday": "off", "free": 0, "meets": {"social": 0.9, "mind": 0.9, "fun": 0.4}, "talk": 1}')
DUTY_HOURS, DUTY_DAYS = [18.17, 20.0], [3]
# the library (capsule_library, its frame): in by the main door (0, 3), past the catalogue to the lounge in front of its
# table (4.0, 9.1); the sofa (4.0, 10.25) seats three, the two armchairs face each other across the table
WAY = [(0.0, 4.6, 0.0), (3.0, 7.4, 0.0), (4.0, 7.9, 0.0)]
SOFA = "lounge_sofa"
SOFA_X = (-0.55, 0.0, 0.55)
ARMCHAIRS = ("lounge_armchair_1", "lounge_armchair_2")
errors = []


def err(m):
    errors.append(m)


def pick_members(people):
    """(The talk's Russian lines are written for a woman as the elder, the student and the writer may be either, a man as the
    programmer: the picks keep to that.) One of each role, the same while the town is the same: free on Thursday evenings, up till 21:30 at least."""
    def free_thursday(p):
        for d in p.get("duties", []):
            if d.get("activity") == "book_club":
                continue
            if 3 in d.get("days", []) and d["hours"][1] > 18.0 and d["hours"][0] < 20.5:
                return False
        return p.get("bed", 23.0) >= 21.5 or p.get("bed", 23.0) < 4.0

    def best(pred):
        fit = [p for p in people if p.get("town") and pred(p) and free_thursday(p)]
        fit.sort(key=lambda p: (-round(p.get("temper", {}).get("studious", 0.0), 3), zlib.crc32(p["key"].encode())))
        return fit[0] if fit else None

    taken = set()

    def take(role, pred):
        p = best(lambda q: q["key"] not in taken and pred(q))
        if not p:
            err("nobody fits the club's %s" % role)
            return None
        taken.add(p["key"])
        return OD([("key", p["key"]), ("role", role)])

    m = [take("librarian", lambda p: p.get("occupation") == "librarian"),
         take("elder", lambda p: p.get("occupation") == "pensioner" and 62 <= p["age"] <= 84 and p.get("female")),
         take("student", lambda p: p.get("occupation") == "student" and 17 <= p["age"] <= 26),
         take("programmer", lambda p: p.get("occupation") == "programmer" and not p.get("female")),
         take("writer", lambda p: p.get("occupation") == "writer" and p.get("female"))]
    return [x for x in m if x]


def club_place(plan, things, by_id, members):
    f = TA.Frame("P-LIBRARY", plan, things)
    base = by_id["P-LIBRARY"]
    sofa = TA.need(things, "P-LIBRARY/" + SOFA)
    sl = f.spot(SOFA) if any(s["id"] == SOFA for s in f.export["spots"]) else None
    sx, sy = (sl["pos"][0], sl["pos"][1]) if sl else (4.0, 10.25)
    slots = []
    # the armchairs first (the librarian and the elder), then the sofa: each member at a seat of their own
    for i, tid in enumerate(ARMCHAIRS):
        slots.append(TA.seat_slot("club_seat", TA.need(things, "P-LIBRARY/" + tid), owner=members[i]["key"] if i < len(members) else ""))
    for j, dx in enumerate(SOFA_X):
        i = len(ARMCHAIRS) + j
        h = float((sofa.get("props") or {}).get("height", 0.45))
        s = OD([("kind", "club_seat"), ("pos", f.world((sx + dx, sy, 0.0))), ("yaw", round(sofa["yaw"] % 360.0, 1)), ("height", h),
                ("thing", sofa["id"])])
        if i < len(members):
            s["owner"] = members[i]["key"]
        slots.append(s)
    for s in slots:
        if not s.get("owner"):
            s.pop("owner", None)
    return TA.walked(PLACE_ID, "book_club", base, [f.world(p) for p in WAY], slots)


def bubble_len(s):
    return len(s)


def build():
    out = OD()
    cat = TA.load(CATALOG)["works"]
    # ---- rules on the words
    if len(C.BOOKS) != 12 or len(set(C.BOOKS)) != 12:
        err("the club needs 12 different books")
    for w in C.BOOKS:
        if w not in cat:
            err("club book %s is not in the catalogue" % w)
            continue
        have = cat[w].get("texts", {})
        if "ru" not in have or "en" not in have:
            err("club book %s needs a Russian and an English text" % w)
        if len(C.TALK.get(w, [])) != len(C.CLUB["order"]):
            err("club book %s: %d talk lines, want %d" % (w, len(C.TALK.get(w, [])), len(C.CLUB["order"])))
        if len(C.HERO.get(w, [])) != C.CLUB["hero_lines"]:
            err("club book %s: the hero's lines" % w)
        for ln in C.TALK.get(w, []) + C.HERO.get(w, []):
            if len(ln) != 3 or not all(ln):
                err("club book %s: a line without all three languages" % w)
            elif bubble_len(ln[2]) > 48:
                err("club book %s: Japanese bubble over 48: %s" % (w, ln[2]))
    for a, b in C.PAIRS:
        for x in (a, b):
            if x not in cat or not cat[x].get("texts"):
                err("advice pair %s -> %s: %s has no text in the catalogue" % (a, b, x))
    for x in C.STARTERS:
        if x not in cat or not cat[x].get("texts"):
            err("starter %s has no text" % x)
    for k, v in C.S.items():
        if len(v) != 3 or not all(v):
            err("string %s lacks a language" % k)
        else:
            import re
            ph = [sorted(set(re.findall(r"\{\d\}", t))) for t in v]
            if ph[0] != ph[1] or ph[0] != ph[2]:
                err("string %s: placeholders differ %s" % (k, ph))
    for n in range(1, C.FRIENDS["opinions"] + 1):
        if "reading.friend.opinion.%d" % n not in C.S:
            err("missing reading.friend.opinion.%d" % n)

    # ---- the town: members, the place, the activity
    folk = TA.load(TA.FOLK)
    people = folk["people"]
    members = pick_members(people)
    plan = TA.load(TA.PLAN)
    things = {t["id"]: t for t in TA.load(TA.INTERIORS)["things"]}
    pd = TA.load(TA.PLACES)
    places = [p for p in pd["places"] if p["id"] != PLACE_ID]
    by_id = {p["id"]: p for p in places}
    if len(members) == 5:
        # the place and the duties stay where they were when there already (a rerun after the later add-ons must not
        # move them to the end: the committed files would change for nothing)
        club = club_place(plan, things, by_id, members)
        at = next((i for i, p in enumerate(pd["places"]) if p["id"] == PLACE_ID), None)
        pd["places"] = places[:at] + [club] + places[at:] if at is not None else places + [club]
        out[TA.PLACES] = TA.dump(pd)
        mk = {m["key"] for m in members}
        for p in people:
            duties = p.get("duties", [])
            at = next((i for i, d in enumerate(duties) if d.get("activity") == "book_club"), len(duties))
            if "duties" in p:
                p["duties"] = [d for d in duties if d.get("activity") != "book_club"]
            if p["key"] in mk:
                p.setdefault("duties", []).insert(at, OD([("activity", "book_club"), ("place", PLACE_ID), ("hours", list(DUTY_HOURS)),
                                                          ("days", list(DUTY_DAYS))]))
        out[TA.FOLK] = TA.dump(folk)
        text = open(TA.ACTIVITIES, encoding="utf-8").read()
        lines = [l for l in text.split("\n") if '"id": "book_club"' not in l]
        end = max(i for i, l in enumerate(lines) if l.strip() == "]")
        last = max(i for i in range(end) if lines[i].strip())
        if not lines[last].rstrip().endswith(","):
            lines[last] = lines[last].rstrip() + ","
        lines[last + 1:end] = ["    " + ACT_ROW]
        out[TA.ACTIVITIES] = "\n".join(lines)
        json.loads(out[TA.ACTIVITIES])

    # ---- reading.json
    club = OD([("weekday", C.CLUB["weekday"]), ("from", C.CLUB["from"]), ("to", C.CLUB["to"]), ("place", C.CLUB["place"]),
               ("order", C.CLUB["order"]), ("hero_lines", C.CLUB["hero_lines"]), ("books", C.BOOKS), ("members", members)])
    doc = OD([("_doc", "Generated by tools/reading/build_reading.py from tools/reading/content.py; docs/books.md. Do not edit."),
              ("club", club), ("pairs", [list(p) for p in C.PAIRS]), ("starters", C.STARTERS), ("friends", C.FRIENDS)])
    out[OUT_JSON] = json.dumps(doc, ensure_ascii=False, indent=1) + "\n"

    # ---- strings
    for li, lang in enumerate(LANGS):
        s = OD()
        s["_doc"] = "Generated by tools/reading/build_reading.py (tools/reading/content.py). Do not edit."
        for k, v in C.S.items():
            s[k] = v[li]
        for w in C.BOOKS:
            for n, ln in enumerate(C.TALK.get(w, []), 1):
                s["line.town.bookclub.%s.%d" % (w, n)] = ln[li]
            for n, ln in enumerate(C.HERO.get(w, []), 1):
                s["reading.club.hero.%s.%d" % (w, n)] = ln[li]
        for role, names in C.ROLES.items():
            s["reading.role.%s" % role] = names[lang]
        for w in sorted(cat):
            can = cat[w].get("canon", {})
            t, a = can.get("title", {}), can.get("author", {})
            s["book.title.%s" % w] = t.get(lang) or t.get("en") or w
            s["book.author.%s" % w] = a.get(lang) or a.get("en") or ""
        out[OUT_STR % lang] = json.dumps(s, ensure_ascii=False, indent=1) + "\n"
    return out, members


def main():
    out, members = build()
    if errors:
        for e in errors:
            print("build_reading: " + e)
        sys.exit(1)
    stale = [p for p, t in out.items() if not os.path.exists(p) or open(p, encoding="utf-8").read() != t]
    if "--check" in sys.argv:
        for p in stale:
            print("build_reading: %s is not up to date (python3 tools/reading/build_reading.py)" % os.path.relpath(p, ROOT))
        sys.exit(1 if stale else 0)
    for p in stale:
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "w", encoding="utf-8") as f:
            f.write(out[p])
        print("wrote", os.path.relpath(p, ROOT))
    print("reading: club members %s" % ", ".join("%s (%s)" % (m["key"], m["role"]) for m in members))


if __name__ == "__main__":
    main()
