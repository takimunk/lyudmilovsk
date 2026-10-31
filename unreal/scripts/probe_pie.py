"""Editor + PIE memory probe without a window: the editor opens Lvl_Scene01 offscreen, idles, starts Play In
Editor, plays, writes memreports (editor idle, PIE) and quits. An outside sampler (probe_memory.sh pie) reads
the process footprint.
  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -RenderOffscreen -unattended -nosplash
      -abslog=<log> "-ExecCmds=py <this file>"
"""
import unreal

STEPS = [(60.0, "idle"), (75.0, "play"), (225.0, "pie"), (235.0, "stop"), (265.0, "after"), (270.0, "quit")]
state = {"t": 0.0, "i": 0}


def cmd(c):
    unreal.SystemLibrary.execute_console_command(None, c)


def tick(dt):
    state["t"] += min(dt, 0.5)
    if state["i"] >= len(STEPS) or state["t"] < STEPS[state["i"]][0]:
        return
    step = STEPS[state["i"]][1]
    state["i"] += 1
    unreal.log_warning("PIEPROBE %s at %.0f s" % (step, state["t"]))
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if step == "play":
        les.editor_request_begin_play()
    elif step == "stop":
        les.editor_request_end_play()
    elif step == "quit":
        cmd("QUIT_EDITOR")
    else:
        cmd("memreport -full -name=pie_%s" % step)


unreal.register_slate_post_tick_callback(tick)
unreal.log_warning("PIEPROBE start")
