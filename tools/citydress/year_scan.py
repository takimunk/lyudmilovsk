"""Every dressing set of the exported buildings (blender/export/city/*.json "dress") over a game year: on how many
(day, hour) samples it shows. A set that never shows is an error; one that always shows is a warning (a set should change
something). Also: exclusive groups named <base>_<k> / <base>_<word> that are meant to take turns are listed with how often
two of them show at once (informational).
Sets with a visit condition (visit=: a trace a townsperson leaves) are scanned with a stand-in for the people's life: the
listed activities end at their usual hours (VISIT_ENDS) on most days (a stable die), so a set that could never show
(a keep shorter than the gap to its hours, a closed-hours clash) is still caught.

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
# when the visits of an activity usually end (h), a stand-in for the people's life (tools/peoplesim has the real one)
VISIT_ENDS = {"eat_home": (8.0, 13.5, 19.5), "coffee": (9.5, 11.0, 15.5, 18.0), "eat_out": (13.5, 19.5),
              "read_library": (11.5, 16.0, 19.0), "book_club": (20.5,), "dominoes": (12.0, 17.5), "san_cure": (10.0, 16.0),
              "culture_evening": (21.5,), "cinema_evening": (21.0,), "cinema_day": (15.0,), "work_office": (13.5, 16.0),
              "school_town": (13.5,), "homework": (17.0, 19.5), "stove_tea": (9.5, 16.5), "radio_hour": (19.0,),
              "read_home": (14.0, 21.0), "play_piano": (19.5,), "eat_club": (13.0, 19.0)}


def visit_ago(when, plot, did, day, hour):
    """Hours since the stand-in's last visit for a set, None if none in the last two days."""
    best = None
    for back in (0, 1, 2):
        if citykit.dress_hash01(plot, did + "#visit", day - back) >= 0.85:
            continue
        for a in when["visit"]:
            for e in VISIT_ENDS.get(a, (10.0, 14.0, 18.0)):
                ago = hour + 24.0 * back - e
                if ago >= 0 and (best is None or ago < best):
                    best = ago
    return best


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
        for hour in (6.25, 7.5, 8.75, 10.25, 12.5, 15.25, 18.5, 21.25, 23.5):     # 7.5: the cleaners before opening
            wx = WEATHERS[(day * 3 + int(hour)) % len(WEATHERS)]
            now = {"since": day, "day": 20381 + day, "hour": hour, "weekday": date.weekday(), "month": date.month,
                   "weather": wx, "festivals": fests, "festival_today": bool(fests),
                   "open": citykit.dress_in_hours(hour, oh[0], oh[1])}
            total += 1
            vis = {}
            for d in sets:
                if "visit" in d["when"]:
                    ago = visit_ago(d["when"], data.get("plot", t), d["id"], now["day"], hour)
                    if ago is not None:
                        vis[d["id"]] = ago
            now["visit_ago"] = vis
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
