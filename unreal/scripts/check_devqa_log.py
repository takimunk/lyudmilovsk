"""Reads test_devqa's log: every quest played through (none STUCK), the teleport walk finished and every point landed.
  python3 check_devqa_log.py <log> [--allow-teleport-problems]   (the second form only reports bad points)"""
import re
import sys

lines = [l.rstrip() for l in open(sys.argv[1], encoding="utf-8", errors="replace") if "DEVQA " in l]
bad, notes = [], []
quests = [l for l in lines if "DEVQA quests done" in l]
if not quests:
    bad.append("the quest run did not report (Dev.QA quests)")
bad += [l[l.index("DEVQA"):] for l in lines if re.search(r"DEVQA quest \S+ STUCK", l)]
tps = [l for l in lines if "DEVQA teleports done" in l]
if not tps:
    bad.append("the teleport walk did not finish (timeout or crash?)")
tp_bad = [l[l.index("DEVQA"):] for l in lines if "DEVQA teleports problem" in l]
(notes if "--allow-teleport-problems" in sys.argv else bad).extend(tp_bad)
summary = [l[l.index("DEVQA"):] for l in quests + tps]
print("DEVQA check: %s; %d problems" % ("; ".join(summary) or "no summary", len(bad)))
for b in bad:
    print("  " + b)
for n in notes:
    print("  (allowed) " + n)
sys.exit(1 if bad else 0)
