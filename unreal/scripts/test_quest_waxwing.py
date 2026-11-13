"""Offscreen check of the quest "A Waxwing for Sorokin" (docs/quests.md) in Lvl_Scene01, with the real people's and bird
life. It plays the quest's stages through the real systems where a script can (the pocket, the map place, the feeder and
the birds that come to it, the bird's call beside Sorokin) and by the quests' own events where it would take a hand
(the talk with Sorokin, the bird eating from the palm). QUESTWAX lines with Quest.Dump's QUESTTEST state between the
steps; "waxwing state 2" = done. Screenshots: the guide on the library's desk, the feeder in the forest, Sorokin speaking.

  sh unreal/scripts/test_quest_waxwing.sh         (MINUTES=6 of real time at slomo 8 for the birds to find the feeder)
"""
import os

import unreal

WATCH = float(os.environ.get("MINUTES", "6")) * 60.0
state = {"t": 0.0, "step": 0, "shots": 0, "wait_until": None, "next_report": 0.0}
# the issue desk's reader's ledge (quest_scenes.json library_guide: Unreal metres -241.10, 629.11, floor -35.46), seen by a
# visitor standing in front of the desk (eyes 1.65 m)
LIBRARY = "BugItGo -24224 62944 -3381 -30 -11 0"
PLAN = [
    (12.0, "Quest.Reset", None),
    (12.2, "Quest.List", None),
    (12.5, "Quest.Start waxwing", None),
    (13.0, "Quest.Dump", "started"),
    # 1. find Sorokin and talk (at 10 he watches birds from his bench in the Academgorodok square, birdwatch_yard)
    (13.5, "lyud.Birds.Time 10", None),
    (16.0, "Quest.GoToPerson sorokin_oleg 2.5", None),
    (19.0, "Quest.Scenes", None),
    (19.5, "shot showui", "sorokin_found"),
    (20.5, "Quest.Event talk Sorokin_oleg", None),
    (21.0, "Quest.Dump", "talked"),
    # 2. the guide lies on the library's desk
    (21.5, LIBRARY, None),
    (25.0, "Quest.Scenes", None),
    (25.5, "shot showui", "library_guide"),
    (26.5, "Inventory.Give bird_guide 1", None),
    (27.0, "Inventory.Give feed_rowan 5", None),
    (27.5, "Inventory.Give feeder 1", None),
    (28.0, "Quest.Dump", "prepared"),
    # 3. the forest beyond the north hills, in the morning; the feeder with rowan
    (28.5, "lyud.Birds.Time 8", None),
    (29.0, "Bird.Hero -420 1780", None),
    (33.0, "Quest.Dump", "at_grove"),
    (33.5, "lyud.Feeder.Place", None),
    (34.5, "lyud.Feeder.Fill feed_rowan 4", None),
    (35.5, "Quest.Dump", "feeder_filled"),
    (36.0, "shot showui", "feeder_in_forest"),
    # 4. the waxwings find it (as if it was filled 1.5 h ago; the game at slomo 8)
    (36.5, "lyud.Feeder.Age 1.5", None),
    (37.0, "WAIT", "waxwing_on_feeder"),
    (37.5, "shot showui", "waxwings_feeding"),
    (38.0, "Quest.Dump", "visited"),
    # 5. it eats from the palm (the befriending has its own check: test_tame.py), the hero is the waxwing
    (38.5, "Quest.Event bird_tamed Waxwing", None),
    (39.0, "Quest.Dump", "tamed"),
    # 6. to Sorokin: near him as the hero first (his body is drawn), then the call
    (39.5, "lyud.Birds.Time 10", None),
    (42.0, "Quest.GoToPerson sorokin_oleg 6", None),
    (46.0, "Quest.BirdCall Starling sorokin_oleg air", None),
    (52.0, "Quest.Dump", "called_from_the_air"),
    (52.5, "Quest.BirdCall Waxwing sorokin_oleg", None),
    (54.0, "shot showui", "sorokin_speaks"),
    (58.0, "shot showui", "sorokin_speaks_2"),
    (66.0, "Quest.Dump", "done"),
    # 7. afterwards, as a person
    (66.5, "Quest.Event talk Sorokin_oleg", None),
    (67.0, "Quest.Dump", "after"),
    # 8. a jump back (developer mode) and a save and load on a middle stage
    (67.5, "Quest.SetStage waxwing wait", None),
    (68.0, "Quest.Dump", "set_stage"),
    (68.5, "lyud.Save questwax", None),
    (71.0, "lyud.Load 0", None),
    (86.0, "Quest.Dump", "loaded"),
]


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
    if state["wait_until"] is not None:
        # the birds' hours pass: slomo 8 until the time is up (the log has the feeder_visit event when they come)
        state["wait_until"] -= dt
        state["next_report"] -= dt
        if state["next_report"] <= 0.0:
            state["next_report"] = 20.0
            unreal.SystemLibrary.execute_console_command(w, "lyud.Feeder.Report")
            unreal.SystemLibrary.execute_console_command(w, "Quest.Dump")
        if state["wait_until"] > 0.0:
            return
        state["wait_until"] = None
        unreal.SystemLibrary.execute_console_command(w, "slomo 1")
    state["t"] += dt
    while state["step"] < len(PLAN) and state["t"] > PLAN[state["step"]][0]:
        _, cmd, label = PLAN[state["step"]]
        state["step"] += 1
        unreal.log_warning("QUESTWAX %s %s" % (cmd, label or ""))
        if cmd == "WAIT":
            state["wait_until"] = WATCH
            unreal.SystemLibrary.execute_console_command(w, "slomo 8")
            return
        if cmd.startswith("shot"):
            unreal.log_warning("QUESTWAX shot %02d %s" % (state["shots"], label))
            state["shots"] += 1
        unreal.SystemLibrary.execute_console_command(w, cmd)
    if state["step"] >= len(PLAN) and state["t"] > PLAN[-1][0] + 1.0:
        unreal.log_warning("QUESTWAX finished")
        unreal.SystemLibrary.execute_console_command(w, "quit")


unreal.register_slate_post_tick_callback(tick)
