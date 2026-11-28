"""The words of the flight home from Europa (docs/spacecraft.md), from one source to the three string tables.

  input   tools/spacecraft/strings.json   every word, {"key": {"ru", "en", "ja"}}; Russian is the source
  output  unreal/Lyudmilovsk/Content/Localization/spacecraft/strings_<lang>.json
          (a sub-folder table: LyudText merges every Content/Localization/<sub>/strings_<lang>.json at load and
          tools/check_localization.py checks it, so nothing has to be registered in C++)

    python3 tools/spacecraft/strings.py            write the tables
    python3 tools/spacecraft/strings.py --check    verify the source and that the tables are up to date (CI); exit 1 on a problem
    python3 tools/spacecraft/strings.py --list     the keys by group with their Russian text

The recorded announcer's callouts are flight.auto.<id> (the subtitle). The same text is also written as
line.flightauto.<id>.1, because tools/gen_voice.py voices only line.<speaker>.<scene>.<n> keys: speaker `flightauto`
in localization/voice.json. Never edit the alias by hand.

Checks: every key in ru, en and ja, no empty text, the same {n} placeholders, the same "[E] " prefix in every language,
no key that another table already owns, and the lengths below (panel captions must fit their plates; a subtitle must fit
two lines). Length is counted in cells: a full-width character (kana, kanji, full-width punctuation) is 2, anything else 1.
"""
import json
import os
import re
import sys
import unicodedata

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
LOC = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Localization")
SRC = os.path.join(HERE, "strings.json")
OUT = os.path.join(LOC, "spacecraft", "strings_%s.json")
LANGS = ("ru", "en", "ja")

# the announcer's callouts are voiced through an alias key (see the top of the file)
ALIAS = (("flight.auto.", "line.flightauto.%s.1"),)

# key prefix -> the most cells a text may take (first match wins)
LIMITS = (
    ("flight.lamp.", 10),       # annunciator cell, one line
    ("flight.inst.", 12),       # instrument name plate
    ("flight.sw.", 10),         # switch caption
    ("flight.mark.", 12),       # ON / OFF / OPEN / SHUT / PARACHUTE
    ("flight.unit.", 10),
    ("flight.phase.", 12),      # the CRT's phase line
    ("flight.plate.name", 10),
    ("flight.plate.", 30),      # one line of the maker's plate
    ("flight.auto.", 52),       # one subtitle line
    ("line.flightauto.", 52),
    ("line.hero.flight_", 80),  # two subtitle lines
    ("flight.book.", 112),
    ("ui.flight.toast.", 64),
    ("ui.flight.no.", 64),
    ("ui.flight.", 48),         # an [E] prompt
    ("dev.", 48),
    ("place.", 28),
)


def cells(text):
    return sum(2 if unicodedata.east_asian_width(c) in ("W", "F") else 1 for c in text)


def limit_of(key):
    for prefix, n in LIMITS:
        if key.startswith(prefix):
            return n
    return None


def load():
    with open(SRC, encoding="utf-8") as f:
        return json.load(f)


def tables(strings):
    out = {l: {} for l in LANGS}
    for key, v in strings.items():
        for l in LANGS:
            out[l][key] = v.get(l, "")
        for prefix, alias in ALIAS:
            if key.startswith(prefix):
                for l in LANGS:
                    out[l][alias % key[len(prefix):]] = v.get(l, "")
    return out


def dump(table):
    return json.dumps(table, ensure_ascii=False, indent=1) + "\n"


def problems(strings):
    bad = []
    for key, v in strings.items():
        if not re.match(r"^[a-z0-9_]+(\.[a-z0-9_]+)+$", key):
            bad.append("%s: not a lowercase dotted key" % key)
        for l in LANGS:
            if not v.get(l, "").strip():
                bad.append("%s: no %s text" % (key, l))
        if set(v) - set(LANGS):
            bad.append("%s: unknown language %s" % (key, sorted(set(v) - set(LANGS))))
        ph = sorted(re.findall(r"\{\d+\}", v.get("ru", "")))
        for l in LANGS:
            t = v.get(l, "")
            if sorted(re.findall(r"\{\d+\}", t)) != ph:
                bad.append("%s: %s placeholders differ from ru" % (key, l))
            if t != t.strip() or "  " in t:
                bad.append("%s: %s has stray spaces" % (key, l))
            if t.startswith("[E] ") != v.get("ru", "").startswith("[E] "):
                bad.append("%s: %s and ru differ in the [E] prefix" % (key, l))
            n = limit_of(key)
            if n is not None and cells(t) > n:
                bad.append("%s: %s is %d cells, the limit is %d: %s" % (key, l, cells(t), n, t))
        if key.startswith(("flight.lamp.", "flight.inst.", "flight.sw.", "flight.mark.", "flight.phase.")):
            for l in ("ru", "en"):
                if v.get(l, "") != v.get(l, "").upper():
                    bad.append("%s: %s panel caption is not upper case" % (key, l))
    # nobody else's key: the hand-written tables win over sub-folder ones, and two sub-folders must not disagree
    mine = set(tables(strings)["ru"])
    for l in LANGS:
        main = os.path.join(LOC, "strings_%s.json" % l)
        if os.path.isfile(main):
            with open(main, encoding="utf-8") as f:
                for k in mine & set(json.load(f)):
                    bad.append("%s is already in the hand-written strings_%s.json" % (k, l))
    for sub in sorted(os.listdir(LOC)):
        f = os.path.join(LOC, sub, "strings_ru.json")
        if sub != "spacecraft" and os.path.isfile(f):
            with open(f, encoding="utf-8") as fh:
                for k in mine & set(json.load(fh)):
                    bad.append("%s is already in %s/strings_ru.json" % (k, sub))
    return bad


def main():
    strings = load()
    if "--list" in sys.argv:
        group = None
        for key, v in strings.items():
            g = ".".join(key.split(".")[:2])
            if g != group:
                group = g
                print("\n# " + g)
            print("%-40s %s" % (key, v["ru"]))
        return 0
    bad = problems(strings)
    out = tables(strings)
    if "--check" in sys.argv:
        for l in LANGS:
            path = OUT % l
            if not os.path.isfile(path):
                bad.append("%s is missing: run tools/spacecraft/strings.py" % os.path.relpath(path, ROOT))
            else:
                with open(path, encoding="utf-8") as f:
                    if f.read() != dump(out[l]):
                        bad.append("%s is stale: run tools/spacecraft/strings.py" % os.path.relpath(path, ROOT))
    for b in bad:
        print("spacecraft strings: " + b)
    if not bad and "--check" not in sys.argv:
        os.makedirs(os.path.dirname(OUT), exist_ok=True)
        for l in LANGS:
            with open(OUT % l, "w", encoding="utf-8") as f:
                f.write(dump(out[l]))
    print("spacecraft strings: %d source keys, %d keys per table, %d languages, %d problems" %
          (len(strings), len(out["ru"]), len(LANGS), len(bad)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
