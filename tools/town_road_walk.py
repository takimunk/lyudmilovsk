#!/usr/bin/env python3
"""The first walk to the town, played offline (backlog item 38, docs/south.md "The first walk"): the hero walks the Aton
coast's trails metre by metre through a copy of ULyudQuests' state machine (tools/questsim/engine.py, its rules read from
LyudQuests.cpp) with every map.json place's radius tested at each step, as ULyudQuests::UpdatePosition does, and the
walked-trail ink (map.json "trails", ULyudQuests::Walked) mirrored.

    python3 tools/town_road_walk.py         exit 1 on a failed check

Checks:
  - the plain walk: the awakening, then the lookout, back to the alley, down the town trail, over the bridge, past the stele,
    onto the first terrace, across the town to Sun Square, bread bought: every stage of town_road begun in order, done;
  - straight down the trail without the lookout: the lookout's stage is skipped by the descent and the quest still ends;
  - by cabin first: the square and bread in town before the walk do not finish it (the terrace stage holds, no_skip), the walk
    afterwards does;
  - the trail ink: after the plain walk the town and lookout trails are inked end to end and the spring path only where it leaves the town trail;
  - the words: the quest's "five minutes" is what the post at the alley's end says (tools/south.py sign_planks).
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "questsim"))
import engine  # noqa: E402
import south as S  # noqa: E402

CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
QUEST = "town_road"
TRAIL_REACH = 6.0          # ULyudQuests::TrailReach


def load(rel):
    with open(os.path.join(CONTENT, rel), encoding="utf-8") as f:
        return json.load(f)


class Walker:
    def __init__(self):
        cpp = open(os.path.join(ROOT, "unreal", "Lyudmilovsk", "Source", "Lyudmilovsk", "Private", "UI", "LyudQuests.cpp"),
                   encoding="utf-8").read()
        m = load(os.path.join("UI", "map.json"))
        self.places = [p for p in m["places"] if p.get("r", 0) > 0]
        self.trails = m.get("trails", [])
        self.sim = engine.Sim(engine.load_defs(load(os.path.join("UI", "quests.json"))), engine.Rules.detect(cpp))
        self.inside = set()
        self.walked = {t["id"]: [False] * len(t["pts"]) for t in self.trails}
        self.stages = []           # town_road's stage ids in the order they began
        self.at = None

    def note(self):
        p = self.sim.find_progress(QUEST)
        d = self.sim.find_def(QUEST)
        if p and p.state == engine.ACTIVE:
            sid = d.stages[p.stage].id
            if not self.stages or self.stages[-1] != sid:
                self.stages.append(sid)
        elif p and p.state == engine.DONE and (not self.stages or self.stages[-1] != "done"):
            self.stages.append("done")

    def stand(self, x, y):
        """UpdatePosition at (x, y) map metres: the trails' ink, then the places in by the radius, out at 1.15x."""
        self.at = (x, y)
        for t in self.trails:
            w = self.walked[t["id"]]
            for i, (px, py) in enumerate(t["pts"]):
                if not w[i] and (px - x) ** 2 + (py - y) ** 2 < TRAIL_REACH ** 2:
                    w[i] = True
        for p in self.places:
            d2 = (p["x"] - x) ** 2 + (p["y"] - y) ** 2
            pid = p["id"].lower()
            if d2 > (p["r"] * 1.15) ** 2:
                self.inside.discard(pid)
                continue
            if d2 > p["r"] ** 2 or pid in self.inside:
                continue
            self.inside.add(pid)
            self.sim.handle_event("reach", p["id"], 1)
            self.note()

    def walk(self, pts, step=1.0):
        """Along a polyline, a step at a time (from where he stands if he stands somewhere)."""
        pts = list(pts)
        if self.at is not None:
            pts.insert(0, self.at)
        for (ax, ay), (bx, by) in zip(pts, pts[1:]):
            n = max(1, int(math.hypot(bx - ax, by - ay) / step))
            for k in range(1, n + 1):
                self.stand(ax + (bx - ax) * k / n, ay + (by - ay) * k / n)

    def event(self, ev, target="", n=1):
        self.sim.handle_event(ev, target, n)
        self.note()

    def buy(self, item):
        # ULyudInventory::Add: the pocket grows, then "get" (and "have" counts the pocket)
        self.sim.inv_add(item, 1)
        self.note()

    def awaken(self):
        """The awakening as played: up from the computer, the owner, out of the door, along the alley to the station."""
        self.event("leave_computer")
        self.event("talk", "owner")
        self.stand(*self.place("club_door"))
        self.walk([self.place("alley"), self.place("cable_upper")])

    def place(self, pid):
        p = next(p for p in load(os.path.join("UI", "map.json"))["places"] if p["id"] == pid)
        return p["x"], p["y"]

    def state(self):
        return self.sim.state(QUEST)


def to_square(w):
    # across the town from the trail's end to the square (the routed track, roughly: north along the first terrace)
    gx, gy = S.GATE
    sx, sy = w.place("square")
    w.walk([(gx + 20.0, gy + 120.0), (sx - 40.0, sy - 200.0), (sx, sy)], step=2.0)


def main():
    bad = []
    tr = S.trails()
    order = [s["id"] for s in next(q for q in load(os.path.join("UI", "quests.json"))["quests"] if q["id"] == QUEST)["stages"]]

    # 1. the plain walk with the lookout
    w = Walker()
    w.awaken()
    if w.state() != engine.ACTIVE:
        bad.append("town_road did not begin when the awakening ended at the station (state %s)" % w.state())
    w.walk(tr["belvedere"])
    w.walk(list(reversed(tr["belvedere"])))
    w.walk(tr["town"])
    to_square(w)
    w.buy("bread_loaf")
    if w.stages != order + ["done"]:
        bad.append("plain walk: stages %s, expected %s" % (w.stages, order + ["done"]))
    for name in ("town", "belvedere"):
        miss = w.walked[name].count(False)
        if miss:
            bad.append("plain walk: %d points of the %s trail are not inked" % (miss, name))
    # the spring path leaves the town trail at the shoulder's foot: only its first few metres are passed
    if sum(w.walked.get("spring", [])) > 2:
        bad.append("plain walk: %d points of the spring path are inked though he never took it" % sum(w.walked["spring"]))
    print("plain walk: %s" % " > ".join(w.stages))

    # 2. straight down, no lookout
    w = Walker()
    w.awaken()
    w.walk(tr["town"])
    to_square(w)
    w.buy("bread_rye")
    if w.state() != engine.DONE or "lookout" in w.stages[1:]:
        bad.append("no lookout: stages %s, state %s" % (w.stages, w.state()))
    print("straight down: %s" % " > ".join(w.stages))

    # 3. by cabin first: the square and bread before the walk
    w = Walker()
    w.awaken()
    w.at = None
    w.stand(*w.place("cable_lower"))
    w.walk([w.place("square")], step=2.0)
    w.buy("bread_loaf")
    early = list(w.stages)
    if w.state() == engine.DONE or w.sim.find_def(QUEST).stages[w.sim.find_progress(QUEST).stage].id in ("square", "bread"):
        bad.append("by cabin: the town before the walk moved the quest past the terrace (%s)" % early)
    w.at = None
    w.stand(*S.ALLEY_END)
    w.walk(tr["town"])
    to_square(w)
    w.buy("bread_loaf")
    if w.state() != engine.DONE:
        bad.append("by cabin: the walk afterwards did not finish it (stages %s)" % w.stages)
    print("by cabin first: %s | then the walk: %s" % (" > ".join(early), " > ".join(w.stages[len(early):])))

    # 4. the words
    town = next(d for name, d, _ in S.sign_planks()[0] if name == "town")
    if not town.endswith("_5min"):
        bad.append("the post at the alley's end says %s for the town, the quest says five minutes" % town)

    for b in bad:
        print("FAIL", b)
    print("town_road_walk: %s" % ("all ok" if not bad else "%d problems" % len(bad)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
