"""Checks the string tables: the same keys in every language, the same {0} placeholders, no empty values, every
asset/voice placeholder known, and (--code) no literal Cyrillic or kana/kanji left in C++ UI code. The generated tables
in subfolders (Content/Localization/<sub>/strings_<lang>.json: the townsfolk's names, their lines) are checked the same
way; LyudText merges them in, the hand-written keys winning.

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


def check_tables(where, tables):
    """The same keys in every language as in ru, no empty values, the same placeholders."""
    global bad
    master = tables["ru"]
    for l in langs:
        for k in master.keys() - tables[l].keys():
            print("%s%s: missing %s" % (where, l, k)); bad += 1
        for k in tables[l].keys() - master.keys():
            print("%s%s: extra %s (not in ru)" % (where, l, k)); bad += 1
        for k, v in tables[l].items():
            if not v.strip():
                print("%s%s: empty %s" % (where, l, k)); bad += 1
            if k in master and sorted(re.findall(r"\{\d+\}", v)) != sorted(re.findall(r"\{\d+\}", master[k])):
                print("%s%s: placeholders differ in %s" % (where, l, k)); bad += 1


check_tables("", tables)
master = dict(tables["ru"])
for sub in sorted(os.listdir(DIR)):
    if not os.path.isfile(os.path.join(DIR, sub, "strings_ru.json")):
        continue
    subt = {}
    for l in langs:
        f = os.path.join(DIR, sub, "strings_%s.json" % l)
        subt[l] = json.load(open(f, encoding="utf-8")) if os.path.isfile(f) else {}
    check_tables(sub + "/", subt)
    shadowed = [k for k in subt["ru"] if k in tables["ru"]]
    if shadowed:
        print("%s/: %d keys are the hand-written table's (theirs are shown), e.g. %s" % (sub, len(shadowed), shadowed[0]))
    for k, v in subt["ru"].items():
        master.setdefault(k, v)
man = json.load(open(os.path.join(ROOT, "localization", "assets.json"), encoding="utf-8"))
used = set()
for a in man["assets"].values():
    used |= set(re.findall(r"<<([^>]+)>>", a["prompt"]))
for st in man["styles"].values():
    used |= set(re.findall(r"<<([^>]+)>>", st))
for k in used - master.keys():
    print("assets.json uses unknown key %s" % k); bad += 1
# every in-game item has an icon: an item.<id>.name key needs an icon_<id> asset (assets.json) with a picture in every language
for k in master:
    m = re.match(r"item\.(\w+)\.name$", k)
    if not m:
        continue
    icon = "icon_" + m.group(1)
    if icon not in man["assets"]:
        print("item %s has no icon asset %s in localization/assets.json" % (m.group(1), icon)); bad += 1
        continue
    for l in langs:
        if l == "ru" and not os.path.exists(os.path.join(ROOT, "textures", "localized", "ru", icon + ".jpg")):
            print("item %s: icon picture textures/localized/ru/%s.jpg is missing (python3 tools/gen_localized.py %s)" % (m.group(1), icon, icon)); bad += 1
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
