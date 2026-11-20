#!/usr/bin/env python3
"""The Rassvet's newsreel data (docs/cinema.md), from tools/films/catalog.py:

  unreal/Lyudmilovsk/Content/City/cinema.json               films (length range, lecturer cues, laughs), the programme by
                                                            weekday + festival, the cinemas (screen, booth, hall in world cm)
  unreal/Lyudmilovsk/Content/Localization/films/strings_<lang>.json   every word: titles, makers, the lecturer's lines,
                                                            the day's programme (film.programme.<mon..sun|fest>), city.film.*

Read by ULyudCinema (Private/City/LyudCinema.cpp, model Private/City/CinemaModel.h) and by tools/paper/paper.py (the
paper prints the day's programme). Run after changing the catalogue or moving the Rassvet:

    python3 tools/films/films.py            # write
    python3 tools/films/films.py --check    # only check (exit 1 on a problem)
"""
import collections
import json
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import catalog  # noqa: E402

CONTENT = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content")
PLAN = os.path.join(CONTENT, "City", "city_plan.json")
OUT = os.path.join(CONTENT, "City", "cinema.json")
OUT_STRINGS = os.path.join(CONTENT, "Localization", "films")
LANGS = ("ru", "en", "ja")
DAYS = ("mon", "tue", "wed", "thu", "fri", "sat", "sun")
ERRORS = []


def err(msg):
    ERRORS.append(msg)


# the building frame -> Unreal metres (the same as tools/city/interiors.py local_to_world / ue_m / world_yaw)
def ue_m(plan, p):
    return [p[0], p[1] + plan["portal_face_y"] / 100.0, p[2]]


def local_to_world(front, yaw, local):
    a = math.radians(yaw)
    x, y, z = local
    return [front[0] + x * math.sin(a) - y * math.cos(a), front[1] - x * math.cos(a) - y * math.sin(a), front[2] + z]


def world_yaw(plot_yaw, local_yaw):
    w = (plot_yaw - local_yaw) % 360.0
    return round(w - 360.0 if w > 180.0 else w, 2)


def cm(p):
    return [round(v * 100.0, 1) for v in p]


def title_list(ids, lang):
    """The day's films for the paper and the poster, in the language's own way of listing titles."""
    by = {f["id"]: f for f in catalog.FILMS}
    li = {"ru": 0, "en": 1, "ja": 2}[lang]
    names = [by[i]["title"][li] for i in ids]
    years = [by[i]["year"] for i in ids]
    span = str(years[0]) if min(years) == max(years) else "%d–%d" % (min(years), max(years))
    if lang == "ru":
        body = " и ".join("«%s»" % n for n in names)
        return "%s (%s)" % (body, span)
    if lang == "en":
        body = " and ".join("“%s”" % n for n in names)
        return "%s (%s)" % (body, span)
    body = "と".join("『%s』" % n for n in names)
    return "%s（%s年）" % (body, span)


def strings():
    t = collections.OrderedDict()

    def put(key, ru, en, ja):
        if key in t:
            err("string %s twice" % key)
        t[key] = {"ru": ru, "en": en, "ja": ja}

    for key, ru, en, ja in catalog.S:
        put(key, ru, en, ja)
    for f in catalog.FILMS:
        put("film.%s.title" % f["id"], *f["title"])
        put("film.%s.maker" % f["id"], *f["maker"])
        for n, (_, words) in enumerate(f["lector"]):
            put("film.%s.l%d" % (f["id"], n + 1), *words)
    for d in range(7):
        put("film.programme.%s" % DAYS[d], *(title_list(catalog.PROGRAMME[d], l) for l in LANGS))
    put("film.programme.fest", *(title_list(catalog.FESTIVAL, l) for l in LANGS))
    for k, v in t.items():
        ph = sorted(re.findall(r"\{\d+\}", v["ru"]))
        for l in LANGS:
            if not v[l].strip():
                err("string %s: no %s" % (k, l))
            if sorted(re.findall(r"\{\d+\}", v[l])) != ph:
                err("string %s: placeholders differ in %s" % (k, l))
    return t


def films():
    out, seen = [], set()
    for f in catalog.FILMS:
        fid = f["id"]
        if not re.match(r"^[a-z][a-z0-9_]*$", fid) or fid in seen:
            err("film id %r: bad or twice" % fid)
        seen.add(fid)
        if f["year"] >= 1929:
            err("%s: published %d, not before 1929 (not public domain in the USA)" % (fid, f["year"]))
        if max(f["died"]) > 1955:
            err("%s: a maker died %d, after 1955 (life + 70 not over)" % (fid, max(f["died"])))
        lo, hi = f["seconds"]
        if not 10 <= lo < hi <= 1500:
            err("%s: seconds %r" % (fid, f["seconds"]))
        cues = [a for a, _ in f["lector"]]
        if cues != sorted(cues) or any(not 0.0 <= a < 1.0 for a in cues):
            err("%s: lecturer cues must rise inside 0..1: %r" % (fid, cues))
        if any(not 0.0 < a < 1.0 for a in f["laughs"]) or f["laughs"] != sorted(f["laughs"]):
            err("%s: laughs must rise inside 0..1" % fid)
        out.append(collections.OrderedDict([
            ("id", fid), ("year", f["year"]), ("title", "film.%s.title" % fid), ("maker", "film.%s.maker" % fid),
            ("seconds", [lo, hi]),
            ("lector", [[a, "film.%s.l%d" % (fid, n + 1)] for n, (a, _) in enumerate(f["lector"])]),
            ("laughs", list(f["laughs"])),
        ]))
    return out, seen


def cinemas(plan):
    plots = {p["id"]: p for p in plan["plots"]}
    out = collections.OrderedDict()
    for cid, c in catalog.CINEMAS.items():
        p = plots.get(c["plot"])
        if not p:
            err("cinema %s: no plot %s" % (cid, c["plot"]))
            continue
        front, yaw = ue_m(plan, p["front"]), p["yaw"]
        s = c["screen"]
        # the picture faces the hall (+y in the building frame: kit yaw 0 faces -y, the street, so the hall side is kit yaw 180)
        out[cid] = collections.OrderedDict([
            ("plot", c["plot"]),
            ("acts", list(c["acts"])),
            ("screen", collections.OrderedDict([("ue", cm(local_to_world(front, yaw, s["pos"]))), ("yaw", world_yaw(yaw, 180.0)),
                                                ("w", round(s["w"] * 100.0, 1)), ("h", round(s["h"] * 100.0, 1))])),
            ("booth", cm(local_to_world(front, yaw, c["booth"]))),
            ("hall", collections.OrderedDict([("ue", cm(local_to_world(front, yaw, c["hall"]["pos"]))),
                                              ("radius", round(c["hall"]["radius"] * 100.0, 1))])),
        ])
    return out


def main():
    check = "--check" in sys.argv
    with open(PLAN, encoding="utf-8") as f:
        plan = json.load(f)
    table = strings()
    fl, ids = films()
    for d, day in list(catalog.PROGRAMME.items()) + [("fest", catalog.FESTIVAL)]:
        if not day:
            err("programme %s is empty" % d)
        for i in day:
            if i not in ids:
                err("programme %s: unknown film %s" % (d, i))
    if sorted(catalog.PROGRAMME) != list(range(7)):
        err("programme needs the seven weekdays 0..6")
    data = collections.OrderedDict([
        ("about", "Written by tools/films/films.py from tools/films/catalog.py (docs/cinema.md); never edit by hand. The films "
                  "themselves are fetched by tools/films/fetch_films.py into Content/Films/<id>.mp4 (not in git)."),
        ("films", fl),
        ("programme", [list(catalog.PROGRAMME[d]) for d in range(7)]),
        ("festival", list(catalog.FESTIVAL)),
        ("cinemas", cinemas(plan)),
    ])
    if ERRORS:
        for e in ERRORS:
            print("films.py: " + e, file=sys.stderr)
        return 1
    outs = [(OUT, json.dumps(data, ensure_ascii=False, indent=1) + "\n")]
    for l in LANGS:
        flat = collections.OrderedDict((k, v[l]) for k, v in table.items())
        outs.append((os.path.join(OUT_STRINGS, "strings_%s.json" % l), json.dumps(flat, ensure_ascii=False, indent=1) + "\n"))
    stale = []
    for path, text in outs:
        old = open(path, encoding="utf-8").read() if os.path.exists(path) else None
        if old != text:
            stale.append(os.path.relpath(path, REPO))
            if not check:
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, "w", encoding="utf-8") as f:
                    f.write(text)
    if check and stale:
        print("films.py: out of date: %s (run tools/films/films.py)" % ", ".join(stale), file=sys.stderr)
        return 1
    print("films.py: %d films, %d strings, %d cinemas%s" % (len(fl), len(table), len(data["cinemas"]),
                                                         "" if check else (", wrote %d files" % len(stale))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
