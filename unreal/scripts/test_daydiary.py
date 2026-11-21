"""Offscreen look at the diary's «Дни» pages (docs/diary.md, Days): fills 14 made-up days, reports them to the log, opens the
diary on its newest entry spread and turns on through the days, a screenshot of each spread (DAYTEST lines in the log).

  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen -ResX=1600 -ResY=900 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py <absolute path>/unreal/scripts/test_daydiary.py"
Screenshots land in Saved/Screenshots (shot showui), in the order of the DAYTEST shot lines.
"""
import unreal

state = {"t": 0.0, "step": 0}

PLAN = [
    (12.0, "lyud.DayLog.Fill 14", None),
    (12.5, "lyud.DayLog.Report", None),
    (13.0, "Screen.Open diary", None),
    (16.0, "shot showui", "entries"),
    (17.0, "Screen.Turn 1", None),
    (19.5, "shot showui", "days_1"),
    (20.0, "Screen.Turn 1", None),
    (22.5, "shot showui", "days_2"),
    (23.0, "Screen.Turn 1", None),
    (25.5, "shot showui", "days_3"),
    (26.0, "lyud.Language en", None),
    (26.5, "Screen.Turn -2", None),
    (29.5, "shot showui", "days_1_en"),
    (30.0, "lyud.Language ja", None),
    (32.5, "shot showui", "days_1_ja"),
    (33.0, "lyud.Language ru", None),
    (33.5, "Screen.Open none", None),
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
        unreal.log_warning("DAYTEST shot %s" % name if name else "DAYTEST %s" % cmd)
        unreal.SystemLibrary.execute_console_command(w, cmd)
    if state["step"] >= len(PLAN) and state["t"] > PLAN[-1][0] + 2.0:
        unreal.log_warning("DAYTEST done")
        unreal.SystemLibrary.execute_console_command(w, "quit")


unreal.register_slate_post_tick_callback(tick)
