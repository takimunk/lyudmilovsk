"""The craft's records as words for the game (docs/spacecraft.md, tools/spacecraft/RECORDS_NOTES.md).

    python3 tools/spacecraft/records_strings.py            write the tables
    python3 tools/spacecraft/records_strings.py --check    they are up to date and complete (CI)

  input   tools/spacecraft/records.json   each record's "text" (name, title, composer, performer, note in ru / en / ja)
  output  unreal/Lyudmilovsk/Content/Localization/spacecraft_records/strings_<lang>.json
          (a sub-folder table: LyudText merges every Content/Localization/<sub>/strings_<lang>.json by itself)
          unreal/Lyudmilovsk/Content/Spacecraft/records.json   what the game's record player reads: id, cell, mood,
          seconds (the pinned length, else the middle of seconds_expected), the fallback cues by mood

Keys, <rec> = the record's id without "record_sc_" (the eight are the craft's own rack: they are NOT inventory items,
so there are no item.* keys): flight.rec.<rec>.name (the sleeve's name, the toast "Now playing: {0}" takes .title),
.title, .composer, .performer, .note (the sleeve note shown when the record goes on), and flight.rec.credit.<rec>
(the attribution line the licence asks for, the same in every language: for the credits screen).
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
SRC = os.path.join(HERE, "records.json")
LOC = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Localization", "spacecraft_records", "strings_%s.json")
GAME = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Spacecraft", "records.json")
LANGS = ("ru", "en", "ja")
FIELDS = ("name", "title", "composer", "performer", "note")

# RECORDS_NOTES.md "Fallback when the files are not fetched": the game's own cue for a sleeve without a disc, by the
# record's first mood that is listed here (the cue's file is /Game/Audio/Music/MU_<cue>)
FALLBACK = (
    ("piano", ["dawn_2", "day_2", "rain_1"]),
    ("night", ["night_1", "night_2"]), ("moon", ["night_1", "night_2"]), ("still", ["night_1", "night_2"]),
    ("space", ["europa_1", "ural_1"]), ("weightless", ["europa_1", "ural_1"]), ("ambient", ["europa_1", "ural_1"]),
    ("orchestra", ["dusk_1"]), ("waltz", ["dusk_1"]),
    ("retro", ["dusk_2"]), ("jazz", ["dusk_2"]),
)
HOUSE_CUE = "europa_1"


def fallback(moods):
    for mood in moods:
        for key, cues in FALLBACK:
            if key == mood:
                return cues
    return [HOUSE_CUE]


def build():
    with open(SRC, encoding="utf-8") as f:
        src = json.load(f)
    tables = {lang: {} for lang in LANGS}
    game = []
    problems = []
    for n, rec in enumerate(sorted(src["records"], key=lambda r: r["cell"])):
        rid = rec["id"]
        if not rid.startswith("record_sc_"):
            problems.append("%s: the id must start with record_sc_" % rid)
            continue
        short = rid[len("record_sc_"):]
        for field in FIELDS:
            for lang in LANGS:
                text = rec.get("text", {}).get(field, {}).get(lang, "")
                if not text:
                    problems.append("%s: no %s in %s" % (rid, field, lang))
                tables[lang]["flight.rec.%s.%s" % (short, field)] = text
        for lang in LANGS:
            tables[lang]["flight.rec.credit.%s" % short] = rec.get("attribution", "")
        seconds = rec.get("seconds")
        if not seconds:
            lo, hi = rec.get("seconds_expected", [180, 240])
            seconds = 0.5 * (lo + hi)
        if rec["cell"] != n:
            problems.append("%s: cell %s, expected %d (cells must run 0..n-1)" % (rid, rec["cell"], n))
        game.append({"id": rid, "short": short, "cell": rec["cell"], "seconds": round(float(seconds), 1),
                     "pinned": bool(rec.get("seconds")), "mood": rec.get("mood", []), "fallback": fallback(rec.get("mood", [])),
                     "asset": "/Game/Spacecraft/Music/%s" % rid})
    data = {"_doc": "Written by tools/spacecraft/records_strings.py from tools/spacecraft/records.json: do not edit by hand.",
            "house_cue": HOUSE_CUE, "records": game}
    return tables, data, problems


def dump(obj):
    return json.dumps(obj, ensure_ascii=False, indent=1) + "\n"


def main():
    tables, data, problems = build()
    outs = [(LOC % lang, dump(tables[lang])) for lang in LANGS] + [(GAME, dump(data))]
    if "--check" in sys.argv:
        for path, text in outs:
            try:
                with open(path, encoding="utf-8") as f:
                    if f.read() != text:
                        problems.append("%s is not up to date" % os.path.relpath(path, ROOT))
            except OSError:
                problems.append("%s is missing" % os.path.relpath(path, ROOT))
        for p in problems:
            print("FAIL " + p)
        print("records_strings: %d records, %d keys a table, %s" % (len(data["records"]), len(tables["ru"]), "%d problems" % len(problems) if problems else "ok"))
        sys.exit(1 if problems else 0)
    if problems:
        for p in problems:
            print("FAIL " + p)
        sys.exit(1)
    for path, text in outs:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            f.write(text)
        print("wrote " + os.path.relpath(path, ROOT))


if __name__ == "__main__":
    main()
