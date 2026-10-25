"""Offscreen check of the game screen and the HUD (docs/ui.md): runs quest events, opens every page in three languages, wears the
head lamp and the binoculars, walks out of the door, and takes a screenshot of each step (UITEST lines in the log).

  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen -ResX=1600 -ResY=900 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py unreal/scripts/test_ui.py"
Screenshots land in Saved/Screenshots (shot showui).
"""
import os

import unreal

state = {"t": 0.0, "step": 0}
DOOR = "BugItGo -193 1500 150 0 90 0"
CABLE = "BugItGo -1500 14090 200 0 120 0"

PLAN = [
    (12.0, "Quest.Dump", None),
    (12.5, "shot showui", "hud_start"),
    (14.0, "Quest.Event leave_computer", None),
    (14.5, "Quest.Event talk owner", None),
    (15.0, "Quest.Dump", None),
    (16.0, "Screen.Open inventory", None),
    (18.0, "shot showui", "inventory"),
    (19.0, "Inventory.Wear head_lamp", None),
    (19.2, "Inventory.Wear binoculars", None),
    (21.0, "shot showui", "inventory_worn"),
    (22.0, "Screen.Open journal", None),
    (24.0, "shot showui", "journal"),
    (25.0, "Screen.Open diary", None),
    (27.0, "shot showui", "diary"),
    (28.0, "Screen.Open map", None),
    (30.0, "shot showui", "map_here"),
    (31.0, "Screen.Open map academ", None),
    (33.0, "shot showui", "map_academ"),
    (34.0, "Screen.Open none", None),
    (34.5, DOOR, None),
    (37.0, "Quest.Dump", None),
    (37.5, "shot showui", "door_hud"),
    (39.0, "lyud.BinocularsHold 1", None),
    (42.0, "shot showui", "binoculars"),
    (43.0, "lyud.BinocularsHold 0", None),
    (44.0, CABLE, None),
    (47.0, "Quest.Dump", None),
    (47.5, "shot showui", "cable_hud"),
    (48.0, "lyud.Language en", None),
    (48.5, "Screen.Open journal", None),
    (50.5, "shot showui", "journal_en"),
    (51.0, "Screen.Open map", None),
    (53.0, "shot showui", "map_en"),
    (54.0, "lyud.Language ja", None),
    (54.5, "Screen.Open inventory", None),
    (56.5, "shot showui", "inventory_ja"),
    (57.0, "Screen.Open journal", None),
    (59.0, "shot showui", "journal_ja"),
    (60.0, "Screen.Open none", None),
    (60.5, "lyud.Language ru", None),
    (61.0, "Shop.Open", None),
    (62.0, "Shop.Open 1", None),
    (63.0, "shot showui", "shop"),
    (64.0, "Shop.Open 2", None),
    (65.0, "Inventory.Open", None),
    (66.0, "Quest.Dump", None),
    (66.5, "Shop.Open close", None),
    (67.0, "lyud.SkyHourOffset 6", None),
    (67.5, DOOR, None),
    (71.0, "shot showui", "lamp_night"),
    (72.0, "Inventory.Wear -1", None),
    (74.0, "shot showui", "no_lamp_night"),
    # the quests survive a save and a load
    (75.0, "Quest.Dump", None),
    (76.0, "lyud.Save uitest", None),
    (78.0, "lyud.Load 0", None),
    (92.0, "Quest.Dump", None),
    (93.0, "Screen.Open journal", None),
    (95.0, "shot showui", "journal_loaded"),
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
            unreal.log_warning("UITEST shot %s" % name)
        else:
            unreal.log_warning("UITEST %s" % cmd)
        unreal.SystemLibrary.execute_console_command(w, cmd)
    if state["step"] >= len(PLAN) and state["t"] > PLAN[-1][0] + 2.0:
        unreal.log_warning("UITEST done")
        unreal.SystemLibrary.execute_console_command(w, "quit")


unreal.register_slate_post_tick_callback(tick)
