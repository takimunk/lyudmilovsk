#!/usr/bin/env python3
"""The cable car's stations living by the town's day (docs/transport.md "Station life"; backlog item 41).

Reads
  unreal/Lyudmilovsk/Content/PeopleData/townsfolk.json     the townsfolk (tools/town/town_life.py): homes, jobs, tempers
  unreal/Lyudmilovsk/Content/PeopleData/town_places.json   where their homes, the school, the market, the beach are
  unreal/Lyudmilovsk/Content/Transport/network.json        the stations and lines
Writes
  unreal/Lyudmilovsk/Content/Transport/station_life.json   the halls' staff rota and the riders' trips (ALyudStationHall,
                                                           ULyudStationLife; tools/transportsim/run.sh town)
  and puts the hall staff and the Akademgorodok's researchers into the town's data (apply(), also called by town_life.py
  so a regenerated town keeps them): their job's duty stays work_away (out of the town's sight while at work, so the
  station halls can show them) with "station"/"role" or "commute" added, the hours and their sleep fitted to the shift;
  occupation station_attendant / station_barista / researcher, their role in Content/Localization/town/strings_<lang>.json.

Who rides (every trip only when riding is quicker than walking: from home to the nearest good station, the ride, on foot
from the other station):
  staff        to their own station for the shift, back after it (the far stations' staff ride to work)
  researchers  home -> Akademgorodok, out 7:45..8:45 on weekdays, back 17:30..19:00
  away         the other out-of-town workers: to Aton (the club, the train) at their own hours
  pupils       to the school's stop and back, school days
  pensioners   to the market, Saturday mornings
  beach        to Amber Shore on warm days (game: 20 C and dry)
  cinema       home from the club's last show (22:45), a few of them each evening
  festival     home from the festival square when a festival's evening ends (game: ULyudFestivalSubsystem OnPhaseEnd)

Everything is seeded by the people's keys: the same town gives the same rota and the same faces at the same hours.

    python3 tools/station_life.py [--check] [--summary]
"""
import argparse
import hashlib
import json
import math
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
FOLK = os.path.join(CONTENT, "PeopleData", "townsfolk.json")
PLACES = os.path.join(CONTENT, "PeopleData", "town_places.json")
NETWORK = os.path.join(CONTENT, "Transport", "network.json")
BUILT = os.path.join(CONTENT, "Transport", "network_built.json")    # per checkout (tools/transport.py): the stations' ground
OUT = os.path.join(CONTENT, "Transport", "station_life.json")
STRINGS = os.path.join(CONTENT, "Localization", "town", "strings_%s.json")
FACE_Y = 7.9                    # UE metres of map y = 0 (city_plan.json portal_face_y / 100)

OPEN, CLOSE = 5.75, 24.25       # the halls: the attendant opens at 5:45, locks up at 0:15
LAST_RIDE = 23.85               # no trip starts later (everyone is home before the hall shuts)
SHIFTS = {
    # role: [(shift, hours, wake, bed)]  (bed < wake: after midnight)
    "attendant": [("early", (5.5, 14.75), 4.6, 21.4), ("late", (14.5, 24.25), 9.2, 1.2)],
    "barista": [("early", (6.25, 14.75), 5.2, 21.9), ("late", (14.5, 23.75), 9.0, 0.9)],
}
# two people a shift: one works Monday to Friday, the other the weekend and three weekdays, so every day is covered
ROTA = [[0, 1, 2, 3, 4], [5, 6, 0, 1, 2]]
WALK_FACTOR = 1.3               # PeopleSim WaySeconds: the way on foot is ~1.3 x the straight line
CABIN = 6.0                     # m/s with people aboard (CablewaySim FParams::Speed)
WAIT = 150.0                    # s: the lift, the platform, a cabin on call
DWELL = 14.0                    # s at each stop on the way
CHANGE = 200.0                  # s: changing lines at Zenith
UP, DOWN = 10.0, 3.0            # s a metre of the terraces' stairs up / down (the town is on six terraces)
ROLES = {
    "station_attendant": ("дежурный по станции", "station attendant", "駅員"),
    "station_attendant_f": ("дежурная по станции", "station attendant", "駅員"),
    "station_barista": ("буфетчик на станции", "station buffet keeper", "駅の売店係"),
    "station_barista_f": ("буфетчица на станции", "station buffet keeper", "駅の売店係"),
    "researcher": ("научный сотрудник", "researcher", "研究員"),
    "researcher_f": ("научная сотрудница", "researcher", "研究員"),
}


def h01(*parts):
    """A number 0..1 from the parts (stable across runs and machines)."""
    d = hashlib.sha1("|".join(str(p) for p in parts).encode("utf-8")).digest()
    return int.from_bytes(d[:6], "big") / float(1 << 48)


def read(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def write(path, data):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=1)
        f.write("\n")


class Net:
    """The stations and the ride times between them (two lines meeting at Zenith)."""

    def __init__(self, network):
        built = {}
        if os.path.exists(BUILT):
            built = {s["id"]: s.get("ground", 0.0) for s in read(BUILT)["stations"]}
        self.at = {s["id"]: (s["at"][0], s["at"][1], built.get(s["id"], -30.0)) for s in network["stations"]}
        self.lines = {l["id"]: l["stops"] for l in network["lines"]}
        self.ids = [s["id"] for s in network["stations"]]

    def _on_line(self, a, b):
        for lid, stops in self.lines.items():
            if a in stops and b in stops:
                i, j = stops.index(a), stops.index(b)
                lo, hi = min(i, j), max(i, j)
                d = sum(math.dist(self.at[stops[k]][:2], self.at[stops[k + 1]][:2]) * 1.15 for k in range(lo, hi))
                return d / CABIN + DWELL * (hi - lo - 1)
        return None

    def ride(self, a, b):
        """Seconds from walking into station a's hall to walking out of b's (the wait included); None: no way."""
        if a == b:
            return None
        t = self._on_line(a, b)
        if t is not None:
            return WAIT + t
        best = None
        for c in self.ids:
            t1, t2 = self._on_line(a, c), self._on_line(c, b)
            if t1 is not None and t2 is not None and c not in (a, b):
                t = WAIT + t1 + CHANGE + t2
                best = t if best is None or t < best else best
        return best

    @staticmethod
    def walk(o, d, speed):
        """Seconds on foot between map points (x, y, z): the way and the terraces' stairs."""
        dz = d[2] - o[2]
        return WALK_FACTOR * math.dist(o[:2], d[:2]) / speed + (dz * UP if dz > 0 else -dz * DOWN)

    def plan(self, o, d, speed, taste=0.0):
        """The best stations to ride between for a way from map point o to d (x, y, z) on foot at speed (m/s), or None
        when walking suits them better. taste: how much they like the ride (0.2: they take it when it is up to 20 % slower
        than walking; the old, the children and the tired like it). Returns (a, b, seconds on foot to a, ride seconds,
        seconds on foot from b, walk-only seconds)."""
        direct = self.walk(o, d, speed)
        if direct < 420.0:
            return None         # a short way: everyone walks
        best = None
        for a in self.ids:
            wa = self.walk(o, self.at[a], speed)
            if wa > direct * 0.7:
                continue
            for b in self.ids:
                r = self.ride(a, b)
                if r is None:
                    continue
                wb = self.walk(self.at[b], d, speed)
                t = wa + r + wb
                if best is None or t < best[0]:
                    best = (t, a, b, wa, r, wb)
        if best is None or best[0] > direct * (1.0 + taste):
            return None
        return best[1], best[2], best[3], best[4], best[5], direct


def place_xy(p):
    q = p.get("pos") or p.get("door")
    return (q[0], q[1] - FACE_Y, q[2])


def taste(p):
    """How much they like riding over walking (-0.1..0.35): seeded, more for the old and the children."""
    t = -0.1 + 0.3 * h01(p["key"], "taste")
    if p["age"] >= 60:
        t += 0.15
    if p["age"] < 16:
        t += 0.08
    return t


def work_away_duty(p):
    ds = [d for d in p.get("duties", []) if d["activity"] == "work_away"]
    return ds[0] if len(ds) == 1 and len(p.get("duties", [])) == 1 else None


def role_key(role, female):
    return role + "_f" if female and role + "_f" in ROLES else role


def choose(folk, homes, net):
    """Who works in the halls and who works at the Akademgorodok: (staff [(person, station, role, shift, rota)], researchers).
    Decided from what apply() never changes (keys, homes, ages, tempers, having one out-of-town job), so it is the same
    before and after apply()."""
    pool = [p for p in folk if work_away_duty(p) and 20 <= p["age"] <= 62 and p["home"] in homes]
    pool.sort(key=lambda p: h01(p["key"], "staff"))
    used = set()
    staff = []
    for st in net.ids:
        for role in ("attendant", "barista"):
            for shift_i, shift in enumerate(SHIFTS[role]):
                for rota_i in range(len(ROTA)):
                    # the nearest free candidate to the station (people work near home), the seeded order breaking ties
                    best = None
                    for p in pool:
                        if p["key"] in used:
                            continue
                        d = math.dist(homes[p["home"]][:2], net.at[st][:2])
                        if best is None or d < best[0] - 1e-6:
                            best = (d, p)
                    if best is None:
                        continue
                    used.add(best[1]["key"])
                    staff.append((best[1], st, role, shift[0], rota_i))
    researchers = [p for p in folk if work_away_duty(p) and p["key"] not in used and p["temper"].get("studious", 0) >= 0.5
                   and p["age"] >= 22]
    return staff, researchers


def apply(folk, places, network, strings=None):
    """Puts the hall staff and the researchers into the townsfolk (in place); strings: {lang: table} to fix their roles in.
    Returns (staff, researchers) as choose() gives them."""
    net = Net(network)
    homes = {p["id"]: place_xy(p) for p in places if "pos" in p or "door" in p}
    staff, researchers = choose(folk, homes, net)
    changed = []
    for p, st, role, shift, rota in staff:
        sh = next(s for s in SHIFTS[role] if s[0] == shift)
        _, hours, wake, bed = sh
        j = (h01(p["key"], "wake") - 0.5) * 0.3
        p["duties"] = [{"activity": "work_away", "station": st, "role": role, "shift": shift,
                        "hours": [hours[0], hours[1] if hours[1] <= 24.0 else round(hours[1] - 24.0, 2)],
                        "days": ROTA[rota]}]
        p["wake"] = round(wake + j, 2)
        p["bed"] = round((bed + j) % 24, 2)
        p["chronotype"] = "lark" if shift == "early" else "owl"
        p["occupation"] = "station_" + role
        changed.append(p)
    for p in researchers:
        d = work_away_duty(p)
        start = round(7.6 + h01(p["key"], "out") * 1.0, 2)
        end = round(17.4 + h01(p["key"], "back") * 1.4, 2)
        d.clear()
        d.update({"activity": "work_away", "commute": "academ", "hours": [start, end], "days": [0, 1, 2, 3, 4]})
        p["wake"] = round(min(p["wake"], start - 0.9), 2)
        night = (p["wake"] - p["bed"]) % 24
        if night < 7.2 or night > 10.4:
            p["bed"] = round((p["wake"] - 8.3) % 24, 2)
        p["occupation"] = "researcher"
        changed.append(p)
    if strings:
        for i, lang in enumerate(("ru", "en", "ja")):
            t = strings.get(lang)
            if t is None:
                continue
            for p in changed:
                t["npc.%s.role" % p["key"]] = ROLES[role_key(p["occupation"], bool(p["female"]))][i]
    return staff, researchers


def trips_for(folk, places, network, staff, researchers):
    """Every rider's trips: [{key, group, mesh, trips: [{from, to, at, days, if, chance}]}]."""
    net = Net(network)
    by_id = {p["id"]: p for p in places}
    homes = {p["id"]: place_xy(p) for p in places if "pos" in p or "door" in p}
    staff_keys = {p["key"] for p, *_ in staff}
    research_keys = {p["key"] for p in researchers}
    school = place_xy(by_id["P-SCHOOL"]) if "P-SCHOOL" in by_id else None
    market = place_xy(by_id["P-MARKET"]) if "P-MARKET" in by_id else None
    beach = place_xy(by_id["S-BEACH"]) if "S-BEACH" in by_id else None
    cinema = place_xy(by_id["P-CLUB2"]) if "P-CLUB2" in by_id else None
    square = place_xy(by_id["S-SQUARE"]) if "S-SQUARE" in by_id else None
    riders = []

    def add(p, group, trips):
        trips = [t for t in trips if t]
        if trips:
            riders.append({"key": p["key"], "group": group, "mesh": p.get("mesh", ""), "trips": trips})

    def trip(a, b, at, days, cond="", chance=1.0):
        at = round(at, 3)
        if at < OPEN + 0.1 or at > LAST_RIDE:
            return None
        return {"from": a, "to": b, "at": at, "days": days, "if": cond, "chance": chance}

    for p in folk:
        home = homes.get(p["home"])
        if home is None:
            continue
        v = max(0.8, p.get("walk_speed", 1.2))
        key = p["key"]
        if key in staff_keys:
            d = p["duties"][0]
            st = d["station"]
            go = net.plan(home, net.at[st], v, 0.1)
            if not go:
                continue
            a, b, wa, r, wb, _ = go
            h0, h1 = d["hours"]
            h1 = h1 + 24.0 if h1 < h0 else h1
            # out: to be at work when the shift starts; home: from their own station after it (the late shift: none,
            # the last cabins are gone; they walk)
            out = trip(a, st, h0 - (r + wb) / 3600.0 - 0.05, d["days"])
            back = trip(st, b if b != st else a, h1 + 0.1, d["days"]) if h1 < LAST_RIDE else None
            add(p, "staff", [out, back])
            continue
        if key in research_keys:
            d = work_away_duty(p)
            dest = net.at["academ"]
            go = net.plan(home, dest, v, taste(p) + 0.1)
            if not go:
                continue
            a, b, wa, r, wb, _ = go
            h0, h1 = d["hours"]
            add(p, "researchers", [trip(a, b, h0 + wa / 3600.0, d["days"]), trip(b, a, h1 - (r + wa) / 3600.0, d["days"])])
            continue
        d = work_away_duty(p)
        if d:
            go = net.plan(home, net.at["aton"], v, taste(p))
            if go:
                a, b, wa, r, wb, _ = go
                h0, h1 = d["hours"]
                add(p, "away", [trip(a, b, h0 + wa / 3600.0, d["days"]), trip(b, a, h1 - (r + wa) / 3600.0, d["days"])])
            continue
        if school and any(x["activity"] == "school_town" for x in p.get("duties", [])):
            go = net.plan(home, school, v, taste(p))
            if go:
                a, b, wa, r, wb, _ = go
                x = next(x for x in p["duties"] if x["activity"] == "school_town")
                h0, h1 = x["hours"]
                add(p, "pupils", [trip(a, b, h0 - 0.3 + wa / 3600.0, x["days"]), trip(b, a, h1 + wb / 3600.0, x["days"])])
            continue
        if market and p.get("occupation") in ("pensioner",) and p["temper"].get("social", 0) >= 0.45:
            go = net.plan(home, market, v, taste(p))
            if go:
                a, b, wa, r, wb, _ = go
                out = 8.6 + h01(key, "market") * 1.6
                add(p, "pensioners", [trip(a, b, out, [5]), trip(b, a, out + 1.4 + h01(key, "market2") * 1.2, [5])])
            continue
        free_days = [i for i in range(7) if not any(i in x.get("days", []) for x in p.get("duties", []))]
        if beach and 14 <= p["age"] <= 65 and p["temper"].get("sun", 0) >= 0.62 and free_days:
            go = net.plan(home, beach, v, taste(p))
            if go:
                a, b, wa, r, wb, _ = go
                out = 10.3 + h01(key, "beach") * 2.5
                add(p, "beach", [trip(a, b, out, free_days, "warm", 0.6), trip(b, a, out + 3.5 + h01(key, "beach2") * 3.0, free_days, "warm", 0.6)])
                continue
        if cinema and 16 <= p["age"] <= 70 and p["temper"].get("social", 0) >= 0.6:
            go = net.plan(cinema, home, v, taste(p))
            if go:
                a, b, wa, r, wb, _ = go
                add(p, "cinema", [trip(a, b, 22.75 + wa / 3600.0 + h01(key, "kino") * 0.15, list(range(7)), "", 0.12)])
                continue
        if square and 16 <= p["age"] <= 75 and p["temper"].get("social", 0) >= 0.5:
            go = net.plan(square, home, v, taste(p))
            if go:
                a, b, wa, r, wb, _ = go
                # "at" for a festival trip: minutes after the evening's end (the game adds the end's hour)
                riders.append({"key": key, "group": "festival", "mesh": p.get("mesh", ""), "trips": [
                    {"from": a, "to": b, "at": round(wa / 3600.0 + h01(key, "fest") * 0.4, 3), "days": list(range(7)),
                     "if": "festival_end", "chance": 0.5}]})
    return riders


def build(write_town=True):
    folk_doc = read(FOLK)
    folk = folk_doc["people"]
    places = read(PLACES)["places"]
    network = read(NETWORK)
    strings = {lang: read(STRINGS % lang) for lang in ("ru", "en", "ja")}
    staff, researchers = apply(folk, places, network, strings)
    if write_town:
        write(FOLK, folk_doc)
        for lang, t in strings.items():
            write(STRINGS % lang, t)
    riders = trips_for(folk, places, network, staff, researchers)
    regular = sorted(r["key"] for r in riders if sum(len(t["days"]) for t in r["trips"] if not t["if"]) >= 8)
    out = {
        "_doc": "The stations' day (tools/station_life.py, docs/transport.md 'Station life'): the halls' staff rota (staff: "
                "townsfolk whose out-of-town job is the hall; hours as in townsfolk.json) and the riders' trips (at: the hour "
                "they walk into the 'from' hall; days 0 Monday; if: warm | festival_end (at = hours after the evening's end) | "
                "''; chance: the share of those days). Generated: rerun after the town or the network change.",
        "open": [OPEN, CLOSE],
        "staff": [{"key": p["key"], "station": st, "role": role, "shift": shift, "hours": p["duties"][0]["hours"],
                   "days": p["duties"][0]["days"], "mesh": p.get("mesh", "")}
                  for p, st, role, shift, rota in staff],
        "riders": riders,
        "regulars": regular,
    }
    write(OUT, out)
    return out


def check(out):
    """The rota covers every hall every open hour; nobody's trip starts after the last ride or before opening; the
    staff ride to their own station; nobody is in two places."""
    fails = []
    for st in sorted({s["station"] for s in out["staff"]}):
        for role in ("attendant", "barista"):
            for day in range(7):
                first, last = (6.0, 23.5) if role == "attendant" else (6.5, 23.5)
                for half in range(int(first * 2), int(last * 2)):
                    h = half / 2.0
                    on = [s for s in out["staff"] if s["station"] == st and s["role"] == role and day in s["days"]
                          and s["hours"][0] <= h < (s["hours"][1] if s["hours"][1] > s["hours"][0] else s["hours"][1] + 24)]
                    if not on:
                        fails.append("%s %s: nobody on day %d at %.1f" % (st, role, day, h))
                        break
    for r in out["riders"]:
        for t in r["trips"]:
            if t["if"] != "festival_end" and not (OPEN <= t["at"] <= LAST_RIDE):
                fails.append("%s: a trip at %.2f" % (r["key"], t["at"]))
            if t["from"] == t["to"]:
                fails.append("%s: a trip from %s to itself" % (r["key"], t["from"]))
    keys = [s["key"] for s in out["staff"]]
    if len(keys) != len(set(keys)):
        fails.append("somebody works two posts")
    for f in fails[:30]:
        print("FAIL", f)
    return not fails


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="only build station_life.json and check it (the town's files untouched)")
    ap.add_argument("--summary", action="store_true")
    args = ap.parse_args()
    out = build(write_town=not args.check)
    groups = {}
    for r in out["riders"]:
        groups[r["group"]] = groups.get(r["group"], 0) + 1
    print("station_life: %d staff, %d riders %s, %d regulars" % (len(out["staff"]), len(out["riders"]), groups, len(out["regulars"])))
    if args.summary:
        for s in out["staff"]:
            print("  %-28s %-8s %-9s %-5s %s %s" % (s["key"], s["station"], s["role"], s["shift"], s["hours"], s["days"]))
        hist = {}
        for r in out["riders"]:
            for t in r["trips"]:
                if t["if"]:
                    continue
                for d in t["days"]:
                    if d == 0:
                        k = (t["from"], int(t["at"]))
                        hist[k] = hist.get(k, 0) + 1
        for k in sorted(hist):
            print("  Monday %-8s %02d:00  %d" % (k[0], k[1], hist[k]))
    ok = check(out)
    print("station_life:", "all ok" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
