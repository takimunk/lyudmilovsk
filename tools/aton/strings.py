"""Builds the game's tables of the ATON OS lab programs' words (docs/aton_lab.md) from tools/aton/strings/*.json
({"key": {"ru", "en", "ja"}}): Content/Localization/aton/strings_<lang>.json, which LyudText merges in at load.

    python3 tools/aton/strings.py           write the tables
    python3 tools/aton/strings.py --check   fail if they are stale, a language is missing, placeholders differ,
                                            a key is the hand-written table's, or the code asks for a key nobody wrote
"""
import glob
import json
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(ROOT, "tools", "aton", "strings")
LOC = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Localization")
OUT = os.path.join(LOC, "aton")
CODE = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Source", "Lyudmilovsk")
LANGS = ("ru", "en", "ja")


def main():
    check = "--check" in sys.argv
    bad = 0
    words = {}
    for path in sorted(glob.glob(os.path.join(SRC, "*.json"))):
        for key, by_lang in json.load(open(path, encoding="utf-8")).items():
            if key in words:
                print("%s: %s is written twice" % (os.path.basename(path), key)); bad += 1
            words[key] = by_lang
    for key, by_lang in words.items():
        for lang in LANGS:
            if not by_lang.get(lang, "").strip():
                print("%s: no %s" % (key, lang)); bad += 1
            elif sorted(re.findall(r"\{\d+\}", by_lang[lang])) != sorted(re.findall(r"\{\d+\}", by_lang.get("ru", ""))):
                print("%s: placeholders differ in %s" % (key, lang)); bad += 1
    hand = json.load(open(os.path.join(LOC, "strings_ru.json"), encoding="utf-8"))
    for key in words:
        if key in hand:
            print("%s is already in the hand-written table" % key); bad += 1
    # every whole key the lab's code names must exist somewhere
    for rel in ("Private/AtonLab.cpp", "Private/AtonOS/AtonLabApps.cpp"):
        for key in set(re.findall(r'TEXT\("((?:ui|dev)\.[a-z0-9_.]*[a-z0-9_])"\)', open(os.path.join(CODE, rel), encoding="utf-8").read())):
            if key not in words and key not in hand:
                print("%s asks for %s, which nobody wrote" % (rel, key)); bad += 1
    for lang in LANGS:
        text = json.dumps({k: words[k].get(lang, "") for k in sorted(words)}, ensure_ascii=False, indent=1) + "\n"
        path = os.path.join(OUT, "strings_%s.json" % lang)
        if check:
            if not os.path.isfile(path) or open(path, encoding="utf-8").read() != text:
                print("%s is stale: run python3 tools/aton/strings.py" % os.path.relpath(path, ROOT)); bad += 1
        else:
            os.makedirs(OUT, exist_ok=True)
            open(path, "w", encoding="utf-8").write(text)
    print("aton strings: %d keys, %d problems" % (len(words), bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
