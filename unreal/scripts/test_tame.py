"""Headless check of befriending from the hero's side (Tame.SelfTest): the vending machine sells a bag of feed, the inventory puts
it in the hand, C crouches (the eyes go down), F holds the palm out with the seeds on it and takes it back, lyud.Tame 0 frees
the hand. Run in /Game/Dev/L_TameTest (unreal/scripts/make_tame_test_level.py) without a window; prints TAMETEST lines and quits.

  UnrealEditor <uproject> /Game/Dev/L_TameTest -game -RenderOffscreen -ResX=640 -ResY=360 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py unreal/scripts/test_tame.py"
"""
import unreal

state = {"t": 0.0, "started": False}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "L_TameTest" and unreal.GameplayStatics.get_player_pawn(w, 0):
            return w
    return None


def tick(dt):
    w = world()
    if not w:
        return
    state["t"] += dt
    if not state["started"] and state["t"] > 5.0:
        state["started"] = True
        # stand at the machine, facing it
        unreal.SystemLibrary.execute_console_command(w, "Tame.Walk 255 0 80 200 0 360 0")
    if state["started"] and state["t"] > 8.0 and not state.get("run"):
        state["run"] = True
        unreal.SystemLibrary.execute_console_command(w, "Tame.Walk off")
        unreal.SystemLibrary.execute_console_command(w, "Tame.SelfTest")
    if state["t"] > 40.0:
        unreal.SystemLibrary.execute_console_command(w, "QUIT")


unreal.register_slate_post_tick_callback(tick)
