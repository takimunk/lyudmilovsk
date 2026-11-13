"""Offscreen QA run of developer mode (docs/dev_mode.md "Tests"): every quest played through by its real events
(Dev.QA quests), then every teleport point visited and checked for ground under the feet (Dev.QA teleports all ... quit,
which closes the game when done).

  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen ... "-ExecCmds=py unreal/scripts/test_devqa.py [warmup s] [wait s] [group]"
unreal/scripts/test_devqa.sh runs it and checks the log (check_devqa_log.py).
"""
import sys

import unreal

WARMUP = float(sys.argv[1]) if len(sys.argv) > 1 else 45.0
WAIT = sys.argv[2] if len(sys.argv) > 2 else "2.5"
GROUP = sys.argv[3] if len(sys.argv) > 3 else "all"
state = {"t": 0.0, "step": 0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_pawn(w, 0):
            return w
    return None


PLAN = [(0.0, "lyud.Language en"), (0.5, "Dev.QA quests"), (2.0, "Dev.Tp club_wake"), (5.0, "Dev.QA teleports %s %s quit" % (GROUP, WAIT))]


def tick(dt):
    w = world()
    if not w or state["step"] >= len(PLAN):
        return
    state["t"] += min(dt, 0.25)
    now = state["t"] - WARMUP
    while state["step"] < len(PLAN) and now > PLAN[state["step"]][0]:
        c = PLAN[state["step"]][1]
        state["step"] += 1
        unreal.log_warning("DEVTEST cmd %s" % c)
        unreal.SystemLibrary.execute_console_command(w, c)


unreal.register_slate_post_tick_callback(tick)
