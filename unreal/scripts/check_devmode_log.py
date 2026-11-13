"""Reads test_devmode's log: actions and points listed, every jump landed, the first quest played to its end, the fly toggle
went on and off, every tab button was found and reached, the test finished."""
import re
import sys

lines = [l for l in open(sys.argv[1], encoding="utf-8", errors="replace")
         if re.search(r"DEV(TEST|PANEL|TP|ACTION)? |DEVTP|DEVACTION|QUESTTEST", l)]
bad = []
actions = [l for l in lines if "DEVACTION " in l]
points = [l for l in lines if "DEVTP " in l]
if len(actions) < 20:
    bad.append("only %d action lines" % len(actions))
if len(points) < 20:
    bad.append("only %d teleport points" % len(points))
for l in lines:
    if ("DEV tp " in l and re.search(r"FAILED|no such point|not there now", l)) or ("DEV do " in l and "no such action" in l):
        bad.append(l.strip())
    if "DEVPANEL no " in l or ("DEVPANEL click" in l and "target ok" not in l):
        bad.append(l.strip())
landed = [l for l in lines if re.search(r"DEV tp \S+ ok", l)]
if len(landed) < 5:
    bad.append("%d of 5 jumps landed" % len(landed))
if not any("DEV play awakening done" in l for l in lines):
    bad.append("awakening did not play to its end: %s" % [l.strip() for l in lines if "DEV play" in l][-3:])
fly = [l for l in lines if "DEVACTION move.fly" in l]
# listed at the start (off), after switching it on, and after switching it off again
if len(fly) < 3 or not fly[1].rstrip().endswith(" on") or not fly[2].rstrip().endswith(" off"):
    bad.append("fly toggle: %s" % [l.strip() for l in fly])
tabs = [l for l in lines if re.search(r"DEVPANEL click ui\.dev\.tab\.", l)]
if len(tabs) < 7:
    bad.append("%d of 7 tabs clicked" % len(tabs))
if not any("DEVTEST done" in l for l in lines):
    bad.append("the test did not finish")
print("DEVMODE check: %d actions, %d points, %d jumps, %d tabs, %d problems" % (len(actions) // 3, len(points), len(landed), len(tabs), len(bad)))
for b in bad:
    print("  " + b)
sys.exit(1 if bad else 0)
