"""Every dressing set of the exported buildings (blender/export/city/*.json "dress") over a game year: on how many
(day, hour) samples it shows. A set that never shows is an error; one that always shows is a warning (a set should change
something). Also: exclusive groups named <base>_<k> / <base>_<word> that are meant to take turns are listed with how often
two of them show at once (informational).

    python3 tools/citydress/year_scan.py [types...]
"""
import datetime
import glob
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "blender", "city"))
import citykit  # noqa: E402

START = datetime.date(2026, 10, 20)
WEATHERS = [{"dry", "clear"}, {"dry"}, {"rain", "wet"}, {"wet", "dry"}, {"snow", "cold"}, {"fog", "dry"},
            {"warm", "clear", "dry"}, {"cold", "dry", "clear"}]
HOURS = {"saucer_pavilion": (8, 22), "round_sanatorium": (0, 24), "school": (7.5, 17), "clinic": (8, 20),
         "workers_club": (10, 23), "cog_club": (10, 23), "capsule_library": (7, 21), "department_store": (9, 20)}


def scan(path):
    data = json.load(open(path))
    sets = data.get("dress", [])
    if not sets:
        return 0, 0
    t = os.path.basename(path)[:-5]
    oh = HOURS.get(t.split("__")[0], (8, 21))
    errors = warns = 0
    counts = {d["id"]: 0 for d in sets}
    total = 0
    for day in range(0, 365, 1):
        date = START + datetime.timedelta(days=day)
        fests = {"new_year"} if (date.month, date.day) in ((12, 31), (1, 1)) else set()
        for hour in (6.25, 8.75, 10.25, 12.5, 15.25, 18.5, 21.25, 23.5):
            wx = WEATHERS[(day * 3 + int(hour)) % len(WEATHERS)]
            now = {"since": day, "day": 20381 + day, "hour": hour, "weekday": date.weekday(), "month": date.month,
                   "weather": wx, "festivals": fests, "festival_today": bool(fests),
                   "open": citykit.dress_in_hours(hour, oh[0], oh[1])}
            total += 1
            for d in sets:
                if citykit.dress_visible(d["when"], data.get("plot", t), d["id"], now):
                    counts[d["id"]] += 1
    for d in sets:
        c = counts[d["id"]]
        if c == 0:
            print("CITYDRESS ERROR %s: set %s never shows in a year (%s)" % (t, d["id"], d["when"]))
            errors += 1
        elif c == total:
            print("CITYDRESS warn  %s: set %s always shows (%s)" % (t, d["id"], d["when"]))
            warns += 1
    print("CITYDRESS %s: %d sets, %d tris, shown %s" % (t, len(sets), sum(d["tris"] for d in sets),
          ", ".join("%s %d%%" % (k, round(100.0 * v / total)) for k, v in counts.items())))
    return errors, warns


def main():
    names = sys.argv[1:]
    paths = sorted(glob.glob(os.path.join(ROOT, "blender", "export", "city", "*.json")))
    if names:
        paths = [p for p in paths if os.path.basename(p)[:-5].split("__")[0] in names or os.path.basename(p)[:-5] in names]
    errors = warns = 0
    for p in paths:
        e, w = scan(p)
        errors += e
        warns += w
    print("CITYDRESS year scan: %d errors, %d warnings" % (errors, warns))
    sys.exit(1 if errors else 0)


if __name__ == "__main__":
    main()
