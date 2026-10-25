"""Headless check of the club owner's trade screen: opens it, buys a bag of feed (12 rub, 5 handfuls), buys again, closes it.
Run in /Game/Dev/L_TameTest (unreal/scripts/make_tame_test_level.py) without a window; prints SHOPTEST lines.

  UnrealEditor <uproject> /Game/Dev/L_TameTest -game -RenderOffscreen -ResX=640 -ResY=360 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py unreal/scripts/test_shop.py"
"""
import unreal

state = {"t": 0.0, "step": 0}


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
    plan = [(4.0, "Shop.Open"), (5.0, "Shop.Open 0"), (6.0, "Shop.Open 0"), (7.0, "Inventory.Open")]
    if state["step"] < len(plan) and state["t"] > plan[state["step"]][0]:
        unreal.SystemLibrary.execute_console_command(w, plan[state["step"]][1])
        state["step"] += 1
    if state["t"] > 9.0:
        unreal.log_warning("SHOPTEST done")
        unreal.SystemLibrary.execute_console_command(w, "quit")


unreal.register_slate_post_tick_callback(tick)
