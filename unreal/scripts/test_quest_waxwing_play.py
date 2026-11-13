"""Offscreen check of "A Waxwing for Sorokin" (docs/quests.md) played closer to the hand than test_quest_waxwing.py:
  1. Sorokin hour by hour 8..19 (his birdwatch_yard duty: the bench in the Academgorodok's square), shots at 10 and 16
  2. the guide, rowan and a feeder already in the pocket, then the real talk with him (Npc.Talk, every line with voice):
     find -> prepare -> journey at once
  3. the grove by day: the feeder filled with rowan, the hero standing by it, NO lyud.Feeder.Age and no slomo; the real
     minutes until a waxwing comes down (feeder_visit Waxwing) are logged (QUESTPLAY waited <s>)
  4. the bird's return: a call on the wing near Sorokin, then sitting beside him a second later: the three waxwing_seen lines
QUESTPLAY lines with Quest.Dump's QUESTTEST state between the steps.

  sh unreal/scripts/test_quest_waxwing_play.sh       (MINUTES=10: the longest real wait for the waxwing)
"""
import os

import unreal

WATCH = float(os.environ.get("MINUTES", "10")) * 60.0
LOG = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "test_quest_waxwing_play.log")
state = {"t": 0.0, "step": 0, "shots": 0, "wait": None, "waited": 0.0, "report": 0.0}
PLAN = [
    (12.0, "Quest.Reset", None),
]
t = 12.5
# 1. Sorokin through the day
for h in range(8, 20):
    PLAN += [(t, "lyud.Birds.Time %d" % h, None), (t + 2.5, "Quest.GoToPerson sorokin_oleg 3", None),
             (t + 6.0, "Quest.GoToPerson sorokin_oleg 3", None), (t + 7.0, "Quest.Scenes", "h%d" % h)]
    if h in (10, 16):
        PLAN.append((t + 7.5, "shot showui", "sorokin_bench_%dh" % h))
    t += 8.5
PLAN += [
    # 2. the pocket ready, then the real talk at 10
    (t, "Quest.Reset", None),
    (t + 0.5, "Inventory.Give bird_guide 1", None),
    (t + 0.8, "Inventory.Give feed_rowan 5", None),
    (t + 1.1, "Inventory.Give feeder 1", None),
    (t + 1.5, "lyud.Birds.Time 10", None),
    (t + 4.0, "Quest.GoToPerson sorokin_oleg 1.3", None),
    (t + 7.0, "Quest.GoToPerson sorokin_oleg 1.3", None),
    (t + 9.0, "Npc.Where", None),
    (t + 9.5, "Npc.Talk", None),
    (t + 10.5, "shot showui", "sorokin_talk"),
]
t += 11.0
for i in range(10):
    PLAN.append((t + i * 1.2, "Npc.Talk", None))
t += 13.0
PLAN += [
    (t, "Quest.Dump", "after_first_talk"),
    # 3. the grove by day, no hurrying by hand
    (t + 0.5, "lyud.Birds.Time 9", None),
    (t + 1.0, "Bird.Hero -420 1780", None),
    (t + 5.0, "Quest.Dump", "at_grove"),
    (t + 5.5, "lyud.Feeder.Place", None),
    (t + 6.5, "lyud.Feeder.Fill feed_rowan 4", None),
    (t + 7.5, "Quest.Dump", "filled"),
    (t + 8.0, "WAIT", "waxwing"),
    (t + 8.5, "shot showui", "grove_after_wait"),
    # where the birds are over the next day from here (hour, id, species, act, x, y, z, site): Saved/QuestWaxwingPlay/day.csv
    (t + 8.7, "lyud.Birds.DumpDay 9 8 " + os.path.join(unreal.Paths.project_saved_dir(), "QuestWaxwingPlay", "day.csv"), None),
    (t + 9.0, "Quest.Dump", "visited"),
    # 4. tamed; to Sorokin on his bench; a call on the wing, then sitting beside him
    (t + 9.5, "Quest.Event bird_tamed Waxwing", None),
    (t + 10.0, "lyud.Birds.Time 16", None),
    (t + 12.5, "Quest.GoToPerson sorokin_oleg 6", None),
    (t + 16.0, "Quest.GoToPerson sorokin_oleg 6", None),
    (t + 18.0, "Quest.BirdCall Waxwing sorokin_oleg air", None),
    (t + 19.5, "Quest.BirdCall Waxwing sorokin_oleg", None),
    (t + 21.0, "shot showui", "sorokin_seen_1"),
    (t + 25.0, "shot showui", "sorokin_seen_2"),
    (t + 33.0, "Quest.Dump", "done"),
]


def visited():
    try:
        with open(LOG, encoding="utf-8", errors="replace") as f:
            return any("event feeder_visit Waxwing" in l for l in f)
    except OSError:
        return False


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_pawn(w, 0):
            return w
    return None


def tick(dt):
    w = world()
    if not w:
        return
    dt = min(dt, 0.25)
    if state["wait"] is not None:
        state["waited"] += dt
        state["report"] -= dt
        if state["report"] <= 0.0:
            state["report"] = 30.0
            unreal.SystemLibrary.execute_console_command(w, "lyud.Feeder.Report")
            if visited():
                state["wait"] = None
                unreal.log_warning("QUESTPLAY waited %.0f s: a waxwing came" % state["waited"])
        if state["wait"] is not None and state["waited"] < state["wait"]:
            return
        if state["wait"] is not None:
            unreal.log_warning("QUESTPLAY waited %.0f s: no waxwing" % state["waited"])
            state["wait"] = None
    state["t"] += dt
    while state["step"] < len(PLAN) and state["t"] > PLAN[state["step"]][0]:
        _, cmd, label = PLAN[state["step"]]
        state["step"] += 1
        unreal.log_warning("QUESTPLAY %s %s" % (cmd, label or ""))
        if cmd == "WAIT":
            state["wait"], state["waited"], state["report"] = WATCH, 0.0, 0.0
            return
        if cmd.startswith("shot"):
            unreal.log_warning("QUESTPLAY shot %02d %s" % (state["shots"], label))
            state["shots"] += 1
        unreal.SystemLibrary.execute_console_command(w, cmd)
    if state["step"] >= len(PLAN) and state["t"] > PLAN[-1][0] + 1.0:
        unreal.log_warning("QUESTPLAY finished")
        unreal.SystemLibrary.execute_console_command(w, "quit")


unreal.register_slate_post_tick_callback(tick)
