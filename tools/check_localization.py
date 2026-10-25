"""Checks the string tables: the same keys in every language, the same {0} placeholders, no empty values, every
asset/voice placeholder known, and (--code) no literal Cyrillic or kana/kanji left in C++ UI code.

    python3 tools/check_localization.py [--code]
"""
import json
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DIR = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Localization")
langs = sorted(f[len("strings_"):-5] for f in os.listdir(DIR) if f.startswith("strings_"))
tables = {l: json.load(open(os.path.join(DIR, "strings_%s.json" % l), encoding="utf-8")) for l in langs}
bad = 0
master = tables["ru"]
for l in langs:
    for k in master.keys() - tables[l].keys():
        print("%s: missing %s" % (l, k)); bad += 1
    for k in tables[l].keys() - master.keys():
        print("%s: extra %s (not in ru)" % (l, k)); bad += 1
    for k, v in tables[l].items():
        if not v.strip():
            print("%s: empty %s" % (l, k)); bad += 1
        if k in master and sorted(re.findall(r"\{\d+\}", v)) != sorted(re.findall(r"\{\d+\}", master[k])):
            print("%s: placeholders differ in %s" % (l, k)); bad += 1
man = json.load(open(os.path.join(ROOT, "localization", "assets.json"), encoding="utf-8"))
used = set()
for a in man["assets"].values():
    used |= set(re.findall(r"<<([^>]+)>>", a["prompt"]))
for st in man["styles"].values():
    used |= set(re.findall(r"<<([^>]+)>>", st))
for k in used - master.keys():
    print("assets.json uses unknown key %s" % k); bad += 1
if "--code" in sys.argv:
    cjk = re.compile(r"[Ѐ-ӿ぀-ヿ一-鿿]")
    src = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Source")
    for d, _, fs in os.walk(src):
        for f in fs:
            if f.endswith((".cpp", ".h")):
                for n, line in enumerate(open(os.path.join(d, f), encoding="utf-8"), 1):
                    if cjk.search(line) and not line.lstrip().startswith("//") and "/*" not in line and "*" != line.lstrip()[:1]:
                        print("literal text: %s:%d" % (os.path.relpath(os.path.join(d, f), ROOT), n)); bad += 1
print("%d languages, %d keys, %d problems" % (len(langs), len(master), bad))
sys.exit(1 if bad else 0)
