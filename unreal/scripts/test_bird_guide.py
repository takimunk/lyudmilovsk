"""Offscreen check of the bird guide (docs/bird_guide.md): the library desk's shop, the guide open at a few places in three
languages, the diary's map with the bird layer on and off. BIRDGUIDE lines in the log, screenshots in Saved/Screenshots.

  sh unreal/scripts/test_bird_guide.sh
"""
import unreal

state = {"t": 0.0, "step": 0, "shots": 0}
DOOR = "BugItGo -193 1500 150 0 90 0"
CABLE = "BugItGo -1500 14090 200 0 120 0"

PLAN = [
    (10.0, "lyud.Trade.Open girl_b", None),
    (11.5, "shot showui", "library_shop"),
    (12.0, "Shop.Open close", None),
    (13.0, DOOR, None),
    (20.0, "lyud.BirdGuide.Report", None),
    (20.5, "lyud.BirdGuide.Open", None),
    (22.0, "shot showui", "guide_door"),
    (25.0, "shot showui", "guide_door_later"),
    (25.5, "lyud.BirdGuide.Open close", None),
    (26.0, "Screen.Open map", None),
    (28.5, "shot showui", "map_layer"),
    (29.0, "lyud.BirdGuide.MapLayer 0", None),
    (30.0, "shot showui", "map_no_layer"),
    (30.5, "lyud.BirdGuide.MapLayer 1", None),
    (31.0, "Screen.Open none", None),
    (32.0, CABLE, None),
    (40.0, "lyud.BirdGuide.Report", None),
    (40.5, "lyud.BirdGuide.Open", None),
    (42.5, "shot showui", "guide_cable"),
    (43.0, "lyud.BirdGuide.Open close", None),
    (43.5, "lyud.Language en", None),
    (44.0, "lyud.BirdGuide.Open", None),
    (46.0, "shot showui", "guide_en"),
    (46.5, "lyud.BirdGuide.Open close", None),
    (47.0, "lyud.Language ja", None),
    (47.5, "lyud.BirdGuide.Open", None),
    (49.5, "shot showui", "guide_ja"),
    (50.0, "lyud.BirdGuide.Open close", None),
    (50.5, "lyud.Language ru", None),
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
    state["t"] += min(dt, 0.25)
    while state["step"] < len(PLAN) and state["t"] > PLAN[state["step"]][0]:
        t, cmd, name = PLAN[state["step"]]
        state["step"] += 1
        if name:
            unreal.log_warning("BIRDGUIDE shot %02d %s" % (state["shots"], name))
            state["shots"] += 1
        else:
            unreal.log_warning("BIRDGUIDE %s" % cmd)
        unreal.SystemLibrary.execute_console_command(w, cmd)
    if state["step"] >= len(PLAN) and state["t"] > PLAN[-1][0] + 2.0:
        unreal.log_warning("BIRDGUIDE done")
        unreal.SystemLibrary.execute_console_command(w, "quit")


unreal.register_slate_post_tick_callback(tick)
