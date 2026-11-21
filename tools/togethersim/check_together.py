#!/usr/bin/env python3
"""Tonight's features together, the data side (tools/togethersim/run.sh runs it after the life-sim week):

  * one person, two places: a townsperson named by two systems (a duty, a cup's pool, the post round's postman, the
    allotment's neighbour) for the same hours of the same weekday, or asleep then;
  * the "Help wanted" boards' promise: every small job's hours have its boss on duty (townsfolk.json shifts at the job's
    plot in the job's role), so the job never answers "nobody here";
  * the hero's week: one week planned minute by minute with the travel between places (the sanatorium's whole course in its
    seven days, every small job once, the paper round on four terraces, a post round, the three cups' rounds, the
    allotment, a kite, the beach and Big Sun Day's three sights), every task placed;
  * money: nothing the hero can sell fetches more than a shop asks for it, and the week's pay covers what tonight's
    features sell;
  * every item tonight's features add has its icon pinned (localization/assets.json);
  * every event tonight's quests wait for is sent by the game's code.

    python3 tools/togethersim/check_together.py [-v]      (-v prints the planned week)
"""
import datetime
import json
import math
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
SRC = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Source", "Lyudmilovsk")
VERBOSE = "-v" in sys.argv
fails = 0
WD = ("mon", "tue", "wed", "thu", "fri", "sat", "sun")


def check(ok, text, *bad):
    global fails
    print("  %s %s" % ("ok  " if ok else "FAIL", text))
    if not ok:
        fails += 1
        for b in bad[:8]:
            print("        %s" % (b,))
        if len(bad) > 8:
            print("        ... and %d more" % (len(bad) - 8))


def load(*path):
    with open(os.path.join(CONTENT, *path), encoding="utf-8") as f:
        return json.load(f)


def hm(h):
    m = int(round(h * 60))
    return "%02d:%02d" % ((m // 60) % 24, m % 60)


def days_of(spec):
    """'wed-sun' / 'mon,sat' / 'sat' -> {2,3,4,5,6}."""
    out = set()
    for part in str(spec or "mon-sun").split(","):
        a, _, b = part.strip().partition("-")
        i = WD.index(a)
        j = WD.index(b) if b else i
        k = i
        while True:
            out.add(k)
            if k == j:
                break
            k = (k + 1) % 7
    return out


def spans(a, b):
    """[a, b) in hours of one day, b < a past midnight: the pieces inside [0, 24)."""
    a, b = a % 24, b % 24 if b != 24 else 24
    return [(a, b)] if b > a else [(a, 24.0), (0.0, b)]


def overlap(p, q):
    return max(0.0, min(p[1], q[1]) - max(p[0], q[0]))


folk = {p["key"]: p for p in load("PeopleData", "townsfolk.json")["people"]}
inter = load("City", "interiors.json")
things = {t["id"]: t for t in inter["things"]}
acts = inter["acts"]
cups = load("City", "tournaments.json")["tournaments"]
rounds = load("City", "rounds.json")
garden = load("Garden", "garden.json")
print("tonight's features together, the data: %d townsfolk, %d things, %d cups" % (len(folk), len(things), len(cups)))


# --- who is where, by the week's plan of every system ------------------------------------------------------------------
def bookings(key):
    """(weekday, from, to, what) for every system that puts this person somewhere."""
    p = folk[key]
    out = []
    for d in p["duties"]:
        for wd in d["days"]:
            for s in spans(*d["hours"]):
                out.append((wd, s[0], s[1], "%s %s" % (d["activity"], d.get("place", ""))))
    return out


def asleep(key, wd, a, b):
    """Is the person in bed in part of [a, b) (their usual wake and bed hours)?"""
    p = folk[key]
    wake, bed = p["wake"], p["bed"]
    night = spans(bed, wake)
    return any(overlap(n, (a, b)) > 0.01 for n in night)


roles = {}   # key -> [(weekday, from, to, what)]
regulars = inter["regulars"]
for cid, c in sorted(cups.items()):
    span = (c["start"], c["start"] + 3 * c["step"])
    for rid, r in sorted(regulars.items()):
        name = r["name"]
        if c["game"] in r["plays"] and name.startswith("npc.") and name[4:-5] in folk:
            roles.setdefault(name[4:-5], []).append((c["weekday"], span[0], span[1], "the cup %s (%s)" % (cid, rid)))
post = rounds["post"]
for wd in post["days"]:
    roles.setdefault(post["postman"], []).append((wd, post["from"], post["until"], "the post round's postman"))
nb = garden["neighbour"]
for d in nb["days"]:
    roles.setdefault(nb["person"], []).append((WD.index(d), nb["hours"][0], nb["hours"][1], "the allotment's neighbour"))

clash = []
for key, rs in sorted(roles.items()):
    if key not in folk:
        clash.append("%s is named by %s but is no townsperson" % (key, rs[0][3]))
        continue
    duties = bookings(key)
    for i, (wd, a, b, what) in enumerate(rs):
        for (wd2, a2, b2, what2) in duties + rs[i + 1:]:
            if wd2 == wd and overlap((a, b), (a2, b2)) > 0.01:
                clash.append("%s %s: %s %s-%s and %s %s-%s" % (WD[wd], key, what, hm(a), hm(b), what2, hm(a2), hm(b2)))
        if asleep(key, wd, a, b):
            clash.append("%s %s: %s %s-%s but asleep (wakes %s, bed %s)" % (WD[wd], key, what, hm(a), hm(b), hm(folk[key]["wake"]),
                                                                             hm(folk[key]["bed"])))
check(not clash, "nobody is put in two places at once by tonight's systems (%d people with a role, %d clashes)" % (len(roles), len(clash)), *clash)


# --- the boards' promise: a boss on duty through every job's hours ---------------------------------------------------
def staffed(plot, role, wd, h):
    for p in folk.values():
        for d in p["duties"]:
            if d.get("place") == plot and d.get("role") == role and any(s[0] <= h < s[1] for s in spans(*d["hours"])):
                # a shift past midnight belongs to the day it began
                if (wd in d["days"]) if (d["hours"][1] > d["hours"][0] or h >= d["hours"][0]) else ((wd - 1) % 7 in d["days"]):
                    return True
    return False


jobs = {tid: t for tid, t in things.items() if t.get("act", "").startswith("job_") and acts.get(t["act"], {}).get("needs_staff")}


def own_hours(tid):
    """Does the job keep hours of its own (a rule's: the kiosk's papers 8-10, the night log 18-7), or only its counter's (the
    café's ice cream in the café's open hours)? A counter's hours are kept by its staff in turns (town_life.py's shifts:
    two days off each), and its job, like the counter, answers while somebody is on: the board says so."""
    t = things[tid]
    same = {}
    for x in things.values():
        if x["plot"] == t["plot"]:
            k = (tuple(x.get("hours") or (0, 24)), x.get("days") or "mon-sun")
            same[k] = same.get(k, 0) + 1
    usual = max(same.items(), key=lambda kv: kv[1])[0]
    return (tuple(t.get("hours") or (0, 24)), t.get("days") or "mon-sun") != usual


unmanned = []
job_windows = {}   # thing -> [(weekday, from, to)] with a boss there
for tid, t in sorted(jobs.items()):
    a = acts[t["act"]]
    hours = t.get("hours") or [0, 24]
    wins = []
    for wd in sorted(days_of(t.get("days"))):
        for s in spans(*hours):
            h, start, gap = s[0], None, None
            while h < s[1] - 1e-6:
                ok = staffed(t["plot"], a["needs_staff"], wd, h)
                if ok and start is None:
                    start = h
                if not ok and start is not None:
                    wins.append((wd, start, h))
                    start = None
                if not ok and gap is None:
                    gap = h
                if ok and gap is not None and own_hours(tid):
                    unmanned.append("%s %s (%s): offered %s-%s with no %s on duty" % (WD[wd], tid, t["act"], hm(gap), hm(h), a["needs_staff"]))
                    gap = None
                h += 0.25
            if start is not None:
                wins.append((wd, start, s[1]))
            if gap is not None and own_hours(tid):
                unmanned.append("%s %s (%s): offered %s-%s with no %s on duty" % (WD[wd], tid, t["act"], hm(gap), hm(s[1]), a["needs_staff"]))
    job_windows[tid] = wins
check(not unmanned, "every small job with hours of its own has its boss on duty through them (%d jobs, %d of them with their own hours, "
      "%d stretches without)" % (len(jobs), sum(own_hours(t) for t in jobs), len(unmanned)), *unmanned)


# --- the hero's week ---------------------------------------------------------------------------------------------------
def at(thing_id):
    t = things[thing_id]
    return (t["ue"][0] / 100.0, t["ue"][1] / 100.0)


spots = {s["id"]: s for s in load("City", "public_spaces.json")["spots"]}
places = {p["id"]: p for p in load("PeopleData", "town_places.json")["places"]}


def venue(kind):
    """A festival venue (a spot kind) as a point: the first public spot of that kind."""
    for s in spots.values():
        if s.get("kind") == kind or s["id"] == "S-" + kind.upper():
            return (s["pos"][0], s["pos"][1])
    for p in places.values():
        if p.get("kind") == kind and p.get("pos"):
            return (p["pos"][0], p["pos"][1])
    return None


SPEED = 3.0          # m/s, door to door: walking, a bicycle, the cable car's waits
DAY_FROM, DAY_TO = 6.0, 23.0


def travel(p, q):
    return 0.0 if p is None or q is None else (math.hypot(p[0] - q[0], p[1] - q[1]) / SPEED + 120.0) / 3600.0


class Plan:
    def __init__(self):
        self.days = {d: [] for d in range(7)}   # (from, to, where, name)

    def fits(self, d, a, b, where):
        if a < DAY_FROM - 1e-6 or b > DAY_TO + 1e-6:
            return False
        for (x, y, w, _) in self.days[d]:
            if not (b + travel(where, w) <= x + 1e-6 or y + travel(w, where) <= a + 1e-6):
                return False
        return True

    def place(self, name, where, windows, minutes, step=5):
        """The first moment in windows [(day, from, to)] the task fits; False if none."""
        dur = minutes / 60.0
        for d, a, b in sorted(windows):
            h = a
            while h + dur <= b + 1e-6:
                if self.fits(d, h, h + dur, where):
                    self.days[d].append((h, h + dur, where, name))
                    self.days[d].sort()
                    return (d, h)
                h += step / 60.0
        return None


plan = Plan()
unplaced = []
pay = {}
week = range(7)


def every(days, a, b):
    return [(d, a, b) for d in week if d in days]


pending = []


def task(name, where, windows, minutes, money=0):
    """A task for the week: placed later, the least flexible first (the cups' rounds, then the narrow windows)."""
    if where is None:
        unplaced.append("%s: no place found for it" % name)
        return
    pending.append((sum(max(0.0, b - a - minutes / 60.0) for _, a, b in windows), len(pending), name, where, windows, minutes, money))


def place_all():
    for _, _, name, where, windows, minutes, money in sorted(pending, key=lambda t: (t[0], t[1])):
        if plan.place(name, where, windows, minutes) is None:
            unplaced.append(name)
        elif money:
            pay[name] = money


# the three cups (signed up for all; three rounds each, a game is 45 minutes), Big Sun Day's sights
for cid, c in sorted(cups.items()):
    t = next((tid for tid, x in things.items() if x["plot"] == c["plot"] and acts.get(x.get("act", ""), {}).get("game", {}).get("vs") == c["game"]), None)
    for r in range(3):
        h = c["start"] + r * c["step"]
        task("cup %s round %d" % (cid, r + 1), at(t) if t else None, [(c["weekday"], h, h + 0.75)], 45)
fest = next(f for f in (lambda d: d["festivals"] if isinstance(d, dict) else d)(load("PeopleData", "festivals.json")) if f["id"] == "sun_day")
for ph in fest["program"]:
    if ph["id"] in ("zenith", "seaside", "sunset"):
        task("Big Sun Day: %s" % ph["id"], venue(ph["venue"]), [(5, ph["hours"][0], ph["hours"][1])], 20)
# the sanatorium's course: the voucher on Monday, its nine stamps in the stay's seven days, the discharge
SAN = "P-SANATORIUM/"
task("san: the voucher and its extension", at(SAN + "cloakroom"), [(0, 8, 21)], 10)
course = [("san: the morning exercises", "lobby_exercise", (7, 9), "mon-sun", 15),
          ("san: the spring at 8", "pump_room", (7.5, 9.5), "mon-sun", 5),
          ("san: the spring at 13", "pump_room", (12.5, 14), "mon-sun", 5),
          ("san: the spring at 18", "pump_room", (17.5, 19), "mon-sun", 5),
          ("san: quartz", "treat_quartz", None, None, 10),
          ("san: the oxygen cocktail", "oxygen_bar", None, None, 10),
          ("san: the health path", "treat_chart", None, None, 45),
          ("san: the quiet hour", "couch_a", (14, 16), "mon-sun", 60),
          ("san: the evening in the lobby", "lobby_dance", (19, 23), "mon-sun", 20)]
for name, tid, hours, days, mins in course:
    t = things[SAN + tid]
    hours = hours or tuple(t.get("hours") or (0, 24))
    task(name, at(SAN + tid), every(days_of(days or t.get("days")), hours[0], hours[1]), mins)
task("san: the discharge (all nine stamps)", at(SAN + "treat_desk"), [(6, 9, 17), (5, 9, 17)], 15)
# every small job once, while its boss is there (the quest job_book)
for tid in sorted(jobs):
    a = acts[things[tid]["act"]]
    lines = a.get("lines") or []
    game = a.get("game") or {}
    best = max([l.get("pay", 0) for l in lines if isinstance(l, dict)] + [w.get("pay", 0) for w in game.get("win", [])] + [0])
    best += game.get("pay", 0) * game.get("need", 0)
    task("job %s" % things[tid]["act"], at(tid), job_windows.get(tid, []), a.get("minutes") or 30, best)
# the rounds: the paper round on the day's terrace (in turn from the game's first day), four terraces; one post round
since_monday = (datetime.date(2027, 6, 7) - datetime.date(2026, 10, 20)).days
paper = rounds["paper"]
kiosk = (paper["bundle"][0] / 100.0, paper["bundle"][1] / 100.0)
terraces = {}
for d in week:
    terraces.setdefault("T%d" % ((since_monday + d) % 4 + 1), []).append(d)
for t, ds in sorted(terraces.items()):
    task("paper round %s" % t, kiosk, [(d, paper["from"], paper["deadline"]) for d in ds], 90, 12 * 4 + 15)
door = (post["door"]["at"][0] / 100.0, post["door"]["at"][1] / 100.0)
task("post round", door, [(d, post["from"], post["deadline"]) for d in week if d in post["days"]], 120, post["parcels"] * 10 + 10)
# the allotment: rent the bed at the board, dig and sow; a kite bought at the toy shop and flown; the beach
bed = garden["beds"][0]["ue"]
task("allotment: rent, dig and sow", (bed[0] / 100.0, bed[1] / 100.0), every(range(7), 7, 20), 40)
toy = next((tid for tid, x in things.items() if x.get("act") in acts and acts[x["act"]].get("shop") == "toy_shop"), None)
task("buy a kite at the toy shop", at(toy) if toy else None, every(range(7), 10, 19), 10)
task("fly the kite on the beach", venue("beach"), every(range(7), 10, 20), 30)
task("the beach: stones and a castle", venue("beach"), every(range(7), 8, 21), 30)
place_all()
if VERBOSE:
    for d in week:
        print("     %s" % WD[d])
        for (a, b, _, n) in plan.days[d]:
            print("        %s-%s %s" % (hm(a), hm(b), n))
busy = sum(b - a for d in week for (a, b, _, _) in plan.days[d])
check(not unplaced, "the hero's week holds all of tonight's features (%d tasks, %.1f hours busy)" % (sum(len(v) for v in plan.days.values()), busy),
      *unplaced)


# --- money ---------------------------------------------------------------------------------------------------------------
shop_price = {}
for s in inter["shops"].values():
    for g in s["goods"]:
        shop_price[g["item"]] = min(shop_price.get(g["item"], 1 << 30), g["price"])
for stall in load("City", "fair_goods.json")["stalls"].values():
    for g in stall.get("goods", []):
        shop_price[g["item"]] = min(shop_price.get(g["item"], 1 << 30), g["price"])
arb = ["%s: sold for up to %d, bought for %d" % (i, max(p), shop_price[i]) for i, p in garden["sell"].items() if i in shop_price and max(p) >= shop_price[i]]
check(not arb, "nothing the hero sells fetches as much as a shop asks for it (%d sellable)" % len(garden["sell"]), *arb)
earned = sum(pay.values())
want = {"kite_diamond": None, "san_voucher": 90, "san_extend": 100, "spade": None, "watering_can": None, "seeds_radish": None}
cost = garden["rules"]["rent"] + sum(v if v is not None else shop_price.get(k, 0) for k, v in want.items())
check(earned >= cost, "the week's pay (%d roubles from %d paid tasks) covers what tonight's features sell the hero (%d: a kite, the course, "
      "the allotment's kit and rent)" % (earned, len(pay), cost))


# --- icons ---------------------------------------------------------------------------------------------------------------
assets = json.load(open(os.path.join(ROOT, "localization", "assets.json"), encoding="utf-8"))["assets"]
inv = open(os.path.join(SRC, "Private", "LyudInventory.cpp"), encoding="utf-8").read()
items = set(re.findall(r'Good\(TEXT\("([a-z0-9_]+)"\)', inv))
tonight = {"kite_diamond", "kite_box", "seashell", "amber_piece", "san_diploma", "spade", "watering_can", "garden_compost", "seeds_radish",
           "seeds_dill", "seeds_sunflower", "seed_potatoes", "seedling_tomato", "seedling_strawberry", "radish", "dill", "potatoes", "tomatoes",
           "strawberries", "jam_strawberry", "garden_film", "postcard"}
missing = sorted(i for i in tonight & items if "icon_" + i not in assets)
check(not missing, "every item of tonight's features has its icon pinned (%d of them in the game)" % len(tonight & items), *missing)
pictures = sorted(i for i in tonight & items if "icon_" + i in assets
                  and not os.path.exists(os.path.join(ROOT, "textures", "localized", "ru", "icon_%s.jpg" % i)))
if pictures:
    print("       (pinned, picture still to cut on the Mac: %s)" % ", ".join(pictures))


# --- quests --------------------------------------------------------------------------------------------------------------
quests = load("UI", "quests.json")
quests = quests["quests"] if isinstance(quests, dict) else quests
code = ""
for base, _, files in os.walk(SRC):
    for f in files:
        if f.endswith((".cpp", ".h")):
            code += open(os.path.join(base, f), encoding="utf-8", errors="ignore").read()
ours = ("job_book", "rounds", "amber_shore", "sanatorium_course", "fest_sun_day")
silent = []
for q in quests:
    if q["id"] not in ours:
        continue
    for st in q["stages"]:
        for o in st["objectives"]:
            for ev in o["event"].split("|"):
                if ev not in ("do", "ask", "visit", "talk", "have") and 'TEXT("%s")' % ev not in code:
                    silent.append("%s/%s waits for %s, which no code sends" % (q["id"], o["id"], ev))
here = [i for i in ours if any(q["id"] == i for q in quests)]
check(not silent and len(here) >= 4, "tonight's quests wait only for events the game sends (%s%s)" % (
    ", ".join(here), "; not merged here: " + ", ".join(i for i in ours if i not in here) if len(here) < len(ours) else ""), *silent)

print("all ok" if not fails else "%d FAILED" % fails)
sys.exit(1 if fails else 0)
