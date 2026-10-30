"""Reads test_devpanel's log: every click reached its widget, each preset and weather type changed the weather,
each phenomenon check box turned its force on and then off, Auto left nothing forced, F10 opened and closed the menu."""
import re
import sys

lines = [l for l in open(sys.argv[1], encoding="utf-8", errors="replace") if "DEVPANEL" in l or "DEVTEST" in l]
bad = []
clicks = 0
pending = None
before = None
for l in lines:
    if "DEVPANEL no " in l:
        bad.append(l.strip())
    m = re.search(r"DEVPANEL click (.+?) at ", l)
    if m:
        clicks += 1
        if "target ok" not in l:
            bad.append(l.strip())
        pending = m.group(1)
    # each click logs the weather before it, then the click, then the weather after
    m = re.search(r"DEVPANEL (before|after) weather (.*)", l)
    if m and m.group(1) == "before":
        before = m.group(2)
    elif m and pending:
        if True:
            after = m.group(2)
            changes = before != after
            # a strike and a star flare leave the weather as it was; Auto right after Auto too
            if not changes and not pending.startswith(("ui.dev.wx.ph.lightning", "ui.dev.wx.ph.sunflare", "SSlider 8", "ui.dev.wx.auto", "ui.dev.world")):
                bad.append("no change: %s | %s" % (pending, after))
            pending = None
logs = [re.search(r"DEVPANEL log weather (.*)", l).group(1) for l in lines if "DEVPANEL log weather" in l]
if len(logs) < 3:
    bad.append("missing log lines")
else:
    on = logs[0].split("| on:")[1].split()
    want = ["thunder", "lenticular", "fog", "aurora", "halo", "rainbow", "nlc", "flare", "activestar", "snowcover", "drifts", "canopy",
            "seaice", "lowwater"]
    if [w for w in want if w not in on]:
        bad.append("phenomena not all on: %s" % logs[0])
    if logs[1].split("| on:")[1].strip():
        bad.append("phenomena left on: %s" % logs[1])
    if logs[-1].split("| on:")[1].strip():
        bad.append("forced after Auto: %s" % logs[-1])
keys = [l for l in lines if "DEVPANEL key F10" in l]
if len(keys) != 2 or any("handled 1" not in k for k in keys):
    bad.append("F10: %s" % keys)
print("DEVPANEL check: %d clicks, %d problems" % (clicks, len(bad)))
for b in bad:
    print("  " + b)
sys.exit(1 if bad else 0)
