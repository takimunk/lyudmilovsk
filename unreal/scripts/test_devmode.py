"""Offscreen check of developer mode (docs/dev_mode.md): the Dev.* commands and every tab of the F7 panel.
Lists the actions and the teleport points, jumps to a few points (each must land), plays the first quest to its end by its
real events, fills the pocket, flies and lands, opens every tab (a shot each, in English) and the debug line.

  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen -ResX=1920 -ResY=1080 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py unreal/scripts/test_devmode.py [warmup s]"
unreal/scripts/test_devmode.sh runs it and checks the log (check_devmode_log.py). Shots land in Saved/Screenshots.
"""
import sys

import unreal

WARMUP = float(sys.argv[1]) if len(sys.argv) > 1 else 45.0
state = {"t": 0.0, "step": 0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_pawn(w, 0):
            return w
    return None


def cmd(w, c):
    unreal.log_warning("DEVTEST cmd %s" % c)
    unreal.SystemLibrary.execute_console_command(w, c)


def shot(name):
    def go(w):
        unreal.log_warning("DEVTEST shot %s" % name)
        unreal.SystemLibrary.execute_console_command(w, "shot showui")
    return go


PLAN = [(0.0, "lyud.Language en"), (0.5, "Dev.List"), (1.0, "Dev.TpList")]
t = 2.0
for point in ["beach_under_club", "valley_meadow", "P-SQUARE", "S-FOUNTAIN", "club_wake"]:
    PLAN += [(t, "Dev.Tp " + point), (t + 3.0, "Dev.Where"), (t + 3.5, shot("tp_" + point))]
    t += 5.0
PLAN += [(t, "Dev.Quest awakening forget"), (t + 0.5, "Dev.Quest awakening owner"), (t + 1.0, "Quest.Dump"),
         (t + 1.5, "Dev.Quest awakening play"), (t + 2.0, "Quest.Dump")]
t += 3.0
# the side quests (docs/quests.md): each must play to its end too
for q in ["waxwing", "waxwing_after", "evening_paper", "crow_gift", "fisherman_cheese", "window_feeder"]:
    PLAN += [(t, "Dev.Quest %s forget" % q), (t + 0.5, "Dev.Quest %s play" % q)]
    t += 3.0
PLAN += [(t, "Dev.Do pocket.money"), (t + 0.5, "Dev.Do pocket.all"), (t + 1.0, "Inventory.Open"), (t + 2.5, shot("pocket_all")),
         (t + 3.0, "Inventory.Open 0")]
t += 4.0
PLAN += [(t, "Dev.Do move.fly"), (t + 2.0, "Dev.List"), (t + 2.5, "Dev.Do move.fly"), (t + 3.0, "Dev.List")]
t += 4.0
PLAN += [(t, "Dev.Do game.overlay"), (t + 1.5, shot("overlay")), (t + 2.0, "Dev.Panel open")]
t += 3.0
for tab in ["world", "teleport", "quests", "town", "pocket", "birds", "actions"]:
    PLAN += [(t, "Dev.Panel clickkey ui.dev.tab." + tab), (t + 1.5, shot("tab_" + tab))]
    t += 2.5
PLAN += [(t, "Dev.Panel close"), (t + 0.5, "Dev.Do game.overlay"), (t + 1.0, "Dev.List")]


def tick(dt):
    w = world()
    if not w:
        return
    state["t"] += min(dt, 0.25)
    now = state["t"] - WARMUP
    while state["step"] < len(PLAN) and now > PLAN[state["step"]][0]:
        _, action = PLAN[state["step"]]
        state["step"] += 1
        if callable(action):
            action(w)
        else:
            cmd(w, action)
    if state["step"] >= len(PLAN) and now > PLAN[-1][0] + 2.0:
        unreal.log_warning("DEVTEST done")
        unreal.SystemLibrary.execute_console_command(w, "quit")
        state["step"] += 1000


unreal.register_slate_post_tick_callback(tick)
