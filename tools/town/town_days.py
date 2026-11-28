#!/usr/bin/env python3
"""The days of the town's life (docs/town_life.md "The days"): the activities the calendar and the day's luck bring, and
what a day does to the ones there already.

Like tools/town/town_activities.py this works on Content/PeopleData/activities.json as it stands and is idempotent
(tools/town/town_life.py runs it after the other add-ons):
    python3 tools/town/town_days.py            add or refresh
    python3 tools/town/town_days.py --check    exit 1 when the file is not as this script makes it

What it writes (activities.json only: no places, no people):
  - the rows of ROWS, after the last row (before the book club's, which stays the last), one a line: the Saturday market, the library's books taken back on one's own
    weekday, the pension fetched at the office, the birthday's cake bought in the morning, the clinic with a cold, and the plans' activities (a visit, the host's
    side of it, a couple's stroll, chess on a bench);
  - the keys of PATCH on rows written by hand: "today_more" (liked more by whoever has that today: paid, the pension, a
    bad night, a cold, a birthday), "own_day" (liked more on one's own weekday for it), "sick": "off" (a duty skipped
    with a cold).

The keys are read by FWorld::LoadActivities (Private/People/PeopleSim.cpp) and used by Private/People/PeopleDays.cpp.
"""
import json
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
ACTIVITIES = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "PeopleData", "activities.json")

ROWS = [
    # errands of the week
    '{"id": "market_day", "places": ["market"], "slots": ["visit"], "pose": "stand", "minutes": [10, 22], "hours": [8, 13], "days": [5], "ages": [16, 200], "meets": {"hunger": 0.8, "social": 0.5, "fun": 0.2}, "base": 0.5, "trait": "social", "once": 1}',
    '{"id": "library_return", "places": ["library"], "slots": ["visit"], "pose": "stand", "minutes": [6, 12], "hours": [10, 19], "days": [0, 1, 2, 3, 4, 5], "ages": [8, 200], "meets": {"mind": 0.5, "social": 0.2}, "base": 0.9, "trait": "studious", "once": 1, "own_day_only": 1, "regulars": 0.45}',
    '{"id": "pension_office", "places": ["office"], "slots": ["visit"], "pose": "stand", "minutes": [8, 18], "hours": [9, 13], "meets": {"social": 0.5}, "base": 1.2, "trait": "", "once": 1, "today": ["pension"]}',
    '{"id": "clinic_cold", "places": ["clinic"], "slots": ["visit"], "pose": "sit", "minutes": [20, 40], "hours": [8, 13], "days": [0, 1, 2, 3, 4], "ages": [14, 200], "meets": {"social": 0.1}, "base": 0.4, "trait": "", "once": 1, "today": ["cold"], "today_more": {"cold": 1.0}}',
    # the day's plans (FPerson::PlanAct): only as today's plan, round its hour
    '{"id": "visit_home", "places": [], "slots": ["kitchen_seat", "dining_seat", "sofa", "armchair", "desk_seat"], "pose": "sit", "minutes": [45, 90], "hours": [10, 21], "meets": {"social": 1.1, "fun": 0.4, "hunger": 0.6}, "base": 0.2, "trait": "social", "plan": 1, "visit": 1, "talk": 1, "rain": "ok"}',
    '{"id": "host_guests", "places": ["home"], "slots": ["kitchen_seat", "dining_seat", "sofa", "armchair"], "pose": "sit", "minutes": [45, 90], "hours": [10, 21.5], "meets": {"social": 1.1, "fun": 0.4, "hunger": 0.6}, "base": 0.2, "trait": "social", "plan": 1, "talk": 1}',
    '{"id": "couple_stroll", "places": ["promenade", "pier", "evening_promenade"], "pose": "walk", "minutes": [30, 55], "hours": [16.5, 21.5], "ages": [18, 200], "meets": {"nature": 0.7, "social": 0.7, "sun": 0.25}, "costs": {"hunger": 0.04, "tired": 0.02}, "base": 0.1, "trait": "nature", "plan": 1, "wander": 1, "wander_m": [10, 140], "wander_pause": [25, 90]}',
    '{"id": "chess_bench", "places": ["bench", "town_bench", "park", "pond", "fountain"], "slots": ["bench"], "pose": "sit", "minutes": [40, 80], "hours": [9.5, 18.5], "daylight": 1, "temp": [7, 40], "ages": [14, 200], "meets": {"mind": 0.8, "social": 0.8, "fun": 0.5}, "base": 0.15, "trait": "studious", "plan": 1}',
    # the birthday's cake fetched in the morning from a shop or a cafe (carried home in its box: manners.json cake_box)
    '{"id": "buy_cake", "places": ["shop", "cafe"], "slots": ["visit"], "pose": "stand", "minutes": [6, 14], "hours": [8, 12.5], "ages": [12, 200], "meets": {"hunger": 0.3, "social": 0.3}, "base": 1.4, "trait": "", "once": 1, "today": ["birthday"]}',
]
IDS = [json.loads(r)["id"] for r in ROWS]

# keys put on rows written by hand (never on another add-on's rows: it would write them back)
PATCH = {
    "shop_town": {"today_more": {"pension": 0.3, "payday": 0.25, "birthday": 0.5}},
    "coffee": {"today_more": {"payday": 0.5, "sleepy": 0.35, "pension": 0.2}},
    "eat_out": {"today_more": {"payday": 0.9}},
    "nap": {"today_more": {"sleepy": 0.5, "cold": 0.5}},
    "rest_home": {"today_more": {"cold": 0.4}},
    "culture_evening": {"own_day": 0.35, "today_more": {"payday": 0.2}},
    "read_library": {"own_day": 0.35},
    "sport": {"own_day": 0.4},
    "clinic_visit": {"own_day": 0.5},
    "school_town": {"sick": "off"},
    "school": {"sick": "off"},
    "work_away": {"sick": "off"},
}


def build(text):
    lines = text.split("\n")
    have = {i: next((n for n, l in enumerate(lines) if '"id": "%s"' % i in l), -1) for i in IDS}
    if all(n >= 0 for n in have.values()):
        # each row stays where it stands (other add-ons put their rows after ours), with its comma as it is
        for i, r in zip(IDS, ROWS):
            lines[have[i]] = "    " + r + ("," if lines[have[i]].rstrip().endswith(",") else "")
    else:
        # after the last row; the book club's row stays the last (tools/reading/build_reading.py puts it there)
        lines = [l for l in lines if not any('"id": "%s"' % i in l for i in IDS)]
        rows = [n for n, l in enumerate(lines) if l.lstrip().startswith('{"id": ')]
        last = rows[-1]
        if '"id": "book_club"' in lines[last]:
            lines[last:last] = ["    " + r + "," for r in ROWS]
        else:
            if not lines[last].rstrip().endswith(","):
                lines[last] = lines[last].rstrip() + ","
            new = ["    " + r + "," for r in ROWS]
            new[-1] = new[-1][:-1]
            lines[last + 1:last + 1] = new
    for n, l in enumerate(lines):
        s = l.strip()
        if not s.startswith('{"id": '):
            continue
        comma = s.endswith(",")
        row = json.loads(s[:-1] if comma else s)
        patch = PATCH.get(row["id"])
        if patch:
            row.update(patch)
            lines[n] = "    " + json.dumps(row, ensure_ascii=False) + ("," if comma else "")
    return "\n".join(lines)


def main():
    text = open(ACTIVITIES, encoding="utf-8").read()
    out = build(text)
    json.loads(out)
    if "--check" in sys.argv:
        if out != text:
            print("town_days: activities.json is not up to date (python3 tools/town/town_days.py)")
            sys.exit(1)
        print("town_days: activities.json is up to date")
        return
    if out != text:
        with open(ACTIVITIES, "w", encoding="utf-8") as f:
            f.write(out)
    print("town days: %d rows (%s), %d rows patched" % (len(ROWS), ", ".join(IDS), len(PATCH)))


if __name__ == "__main__":
    main()
