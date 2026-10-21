"""Plays L_ForestPreview twice in a row without a window (offscreen -game): the forests bake and swap trees for live
plants near the camera while the pawn is moved through them, then the level is opened again (a second world after
the first is torn down) and the same again. Prints PLAYTEST lines; the process must still be alive at the end.

  UnrealEditor <uproject> /Game/Dev/L_ForestPreview -game -RenderOffscreen -ResX=800 -ResY=450 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py <this file>"
"""
import unreal

state = {"tick": 0, "round": 0}
ROUNDS = 2
FRAMES = 700


def world():
    a = unreal.find_object(None, "/Game/Dev/L_ForestPreview.L_ForestPreview:PersistentLevel.ProceduralPlant_0")
    return a.get_world() if a else None


def tick(dt):
    state["tick"] += 1
    t = state["tick"]
    w = unreal.GameplayStatics
    if t % 100 == 0:
        unreal.log_warning("PLAYTEST round %d frame %d" % (state["round"], t))
    if t == 30:
        unreal.log_warning("PLAYTEST world = %s" % world())
    if t == FRAMES:
        state["round"] += 1
        state["tick"] = 0
        if state["round"] >= ROUNDS:
            unreal.log_warning("PLAYTEST done")
            unreal.SystemLibrary.execute_console_command(world(), "QUIT")
        else:
            unreal.log_warning("PLAYTEST reopening the level")
            unreal.SystemLibrary.execute_console_command(world(), "open /Game/Dev/L_ForestPreview")


unreal.register_slate_post_tick_callback(tick)
unreal.log_warning("PLAYTEST start")
