"""Offscreen memory probe for Lvl_Scene01: plays the level, writes memreport -full at a few moments
(Saved/Profiling/MemReports) and prints MEMPROBE lines; an outside sampler reads the process footprint.

  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py <this file>"

Extra console commands to run before the first report: env MEMPROBE_CMDS="cmd1;cmd2".
"""
import os
import unreal

REPORTS = [(90.0, "a"), (240.0, "b")]
state = {"t": 0.0, "next": 0, "frames": 0}


def world():
    return unreal.UnrealEditorSubsystem().get_game_world() if hasattr(unreal, "UnrealEditorSubsystem") else None


def cmd(c):
    unreal.SystemLibrary.execute_console_command(None, c)


def tick(dt):
    state["t"] += dt
    state["frames"] += 1
    if state["frames"] == 5:
        for c in filter(None, os.environ.get("MEMPROBE_CMDS", "").split(";")):
            unreal.log_warning("MEMPROBE cmd %s" % c)
            cmd(c)
    if state["next"] < len(REPORTS) and state["t"] >= REPORTS[state["next"]][0]:
        name = REPORTS[state["next"]][1]
        unreal.log_warning("MEMPROBE report %s at %.0f s, frame %d" % (name, state["t"], state["frames"]))
        cmd("memreport -full -name=probe_%s" % name)
        state["next"] += 1
        if state["next"] == len(REPORTS):
            unreal.log_warning("MEMPROBE done")
            cmd("QUIT")


unreal.register_slate_post_tick_callback(tick)
unreal.log_warning("MEMPROBE start")
