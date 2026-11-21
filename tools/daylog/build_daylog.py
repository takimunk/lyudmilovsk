#!/usr/bin/env python3
"""Builds the diary-writes-itself data from tools/daylog/content.py (docs/diary.md, "Days").

Writes:
  unreal/Lyudmilovsk/Content/UI/daylog.json                              rules, topics, variant counts (DayLog.cpp reads it)
  unreal/Lyudmilovsk/Content/Localization/diary/strings_<lang>.json      the words, merged by LyudText

    python3 tools/daylog/build_daylog.py           # write
    python3 tools/daylog/build_daylog.py --check   # build into memory and fail if the committed files differ
"""
import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import content as C  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
LANGS = ("ru", "en", "ja")


def fail(msg):
    sys.exit("build_daylog: " + msg)


def check_line(key, line, slots):
    if len(line) != 3 or not all(isinstance(s, str) and s.strip() for s in line):
        fail("%s: needs ru, en, ja" % key)
    for lang, s in zip(LANGS, line):
        have = sorted(set(re.findall(r"\{(\d)\}", s)))
        if have != slots:
            fail("%s (%s): placeholders %s, want %s" % (key, lang, have, slots))
        if lang == "ja" and len(s) > 60:
            fail("%s (ja): %d chars, keep it under 60" % (key, len(s)))


def build():
    tables = {l: {} for l in LANGS}

    def put(key, line, slots):
        check_line(key, line, slots)
        for i, lang in enumerate(LANGS):
            if key in tables[lang]:
                fail("duplicate key " + key)
            tables[lang][key] = line[i]

    for k, line in C.UI.items():
        put(k, line, sorted(set(re.findall(r"\{(\d)\}", line[0]))))

    topics = []
    ids = set()
    for t in C.TOPICS:
        tid = t["id"]
        if tid in ids:
            fail("duplicate topic " + tid)
        ids.add(tid)
        one, many, plain = t.get("one", []), t.get("many", []), t.get("plain", [])
        if len(plain) < 3:
            fail("%s: at least 3 plain lines (no sentence two days running)" % tid)
        if t.get("slot") and len(one) < 3:
            fail("%s: a topic with a name needs at least 3 named lines" % tid)
        if (one or many) and not t.get("slot"):
            fail("%s: named lines without a slot" % tid)
        if many and len(many) < 2:
            fail("%s: at least 2 'many' lines" % tid)
        for i, line in enumerate(one):
            put("diary.day.%s.one.%d" % (tid, i), line, ["0"])
        for i, line in enumerate(many):
            put("diary.day.%s.many.%d" % (tid, i), line, ["0", "1"])
        for i, line in enumerate(plain):
            put("diary.day.%s.plain.%d" % (tid, i), line, [])
        topics.append({"id": tid, "rank": t["rank"], "slot": t.get("slot", ""), "doodle": t.get("doodle", ""),
                       "once": bool(t.get("once")), "filler": bool(t.get("filler")),
                       "one": len(one), "many": len(many), "plain": len(plain)})

    weather = []
    for code, lines in C.WEATHER:
        if len(lines) < 3:
            fail("weather %s: at least 3 lines" % code)
        for i, line in enumerate(lines):
            put("diary.day.weather.%s.%d" % (code, i), line, [])
        weather.append({"code": code, "n": len(lines)})
    for name, lines in (("quiet", C.QUIET), ("close.busy", C.BUSY), ("close.calm", C.CALM)):
        if len(lines) < 3:
            fail(name + ": at least 3 lines")
        for i, line in enumerate(lines):
            put("diary.day.%s.%d" % (name, i), line, [])

    rules = []
    for r in C.RULES:
        event, target, topic = r[0], r[1], r[2]
        if topic not in ids:
            fail("rule %s %s: unknown topic %s" % (event, target, topic))
        rule = {"event": event, "target": target, "topic": topic}
        if len(r) > 3:
            rule["except"] = list(r[3])
        rules.append(rule)

    data = {"rules": rules, "topics": topics, "weather": weather, "quiet": len(C.QUIET), "busy": len(C.BUSY),
            "calm": len(C.CALM)}
    data.update(C.SETTINGS)
    return data, tables


def dumps(obj, indent):
    return json.dumps(obj, ensure_ascii=False, indent=indent) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    data, tables = build()
    outputs = {os.path.join(CONTENT, "UI", "daylog.json"): dumps(data, 1)}
    for lang in LANGS:
        outputs[os.path.join(CONTENT, "Localization", "diary", "strings_%s.json" % lang)] = dumps(tables[lang], 1)
    bad = []
    for path, text in outputs.items():
        if a.check:
            old = open(path, encoding="utf-8").read() if os.path.exists(path) else None
            if old != text:
                bad.append(os.path.relpath(path, ROOT))
        else:
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "w", encoding="utf-8") as f:
                f.write(text)
    if bad:
        fail("out of date (rerun build_daylog.py): " + ", ".join(bad))
    n = len(tables["ru"])
    print("daylog: %d topics, %d rules, %d lines a language" % (len(data["topics"]), len(data["rules"]), n))


if __name__ == "__main__":
    main()
