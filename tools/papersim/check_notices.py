#!/usr/bin/env python3
"""The notice boards' texts (tools/city/content/x_notices.json, docs/town_paper.md "Notices") through two years of days:
on every day each board shows its own line, at most one notice per page (dated notices never collide), every dated notice
shows on some day, and no board stays without a dated notice for more than MAX_GAP days (it must look alive)."""
import datetime
import json
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
NOTICES = os.path.join(ROOT, "tools", "city", "content", "x_notices.json")
START = datetime.date(2026, 10, 20)
MAX_GAP = {"lost": 60, "club": 40, "health": 60, "school": 70}


def holds(line, day):
    if isinstance(line, str):
        return True
    since = (day - START).days
    if "after" in line and since < line["after"]:
        return False
    if "before" in line and since >= line["before"]:
        return False
    if "date" in line:
        lo, hi = line["date"]
        md = day.strftime("%m-%d")
        if lo <= hi and not (lo <= md <= hi):
            return False
        if lo > hi and not (md >= lo or md <= hi):
            return False
    if "festival" in line:
        return False    # festival pages are checked by citysim/the game; treat as not holding here
    return True


def main():
    texts = json.load(open(NOTICES, encoding="utf-8"))["texts"]
    fails = 0
    seen = set()
    for tid, t in sorted(texts.items()):
        board = tid.split("_", 1)[1]
        gap = longest = 0
        for n in range(730):
            day = START + datetime.timedelta(days=n)
            shown = 0
            dated = False
            for page in t["pages"]:
                hold = [e for e in page if holds(e, day)]
                if len(hold) > 1 and all(isinstance(e, dict) and ("date" in e or "after" in e) for e in hold):
                    print("FAIL %s %s: %d notices on one page" % (tid, day, len(hold)))
                    fails += 1
                if hold:
                    shown += 1
                for e in hold:
                    if isinstance(e, dict) and ("date" in e or "after" in e):
                        dated = True
                        seen.add(e["k"])
            if shown == 0:
                print("FAIL %s %s: the board shows nothing" % (tid, day))
                fails += 1
            gap = 0 if dated else gap + 1
            longest = max(longest, gap)
        ok = longest <= MAX_GAP.get(board, 60)
        print("%s %s: longest run without a dated notice %d days (limit %d)" % ("ok  " if ok else "FAIL", tid, longest, MAX_GAP.get(board, 60)))
        fails += 0 if ok else 1
    allk = {e["k"] for t in texts.values() for p in t["pages"] for e in p if isinstance(e, dict) and ("date" in e or "after" in e)}
    missing = allk - seen
    print("%s every dated notice shows on some day (%d of %d)" % ("ok  " if not missing else "FAIL", len(allk) - len(missing), len(allk)))
    fails += 1 if missing else 0
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
