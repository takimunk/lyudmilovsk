"""Offscreen check that "Awakening" never sticks (docs/ui.md): the normal way (a few steps, the owner, the door, the cable car),
a teleport straight to the cable car from a fresh start, and a load of a save made on the first step followed by a teleport
out of the door. QUESTFLOW lines with Quest.Dump's QUESTTEST state; "awakening state 2" = done.

  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed -nosound
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py unreal/scripts/test_quest_flow.py"
"""
import unreal

DOOR = "BugItGo -193 1500 150 0 90 0"
CABLE = "BugItGo -1500 14090 200 0 120 0"
state = {"t": 0.0, "step": 0, "walk": 0.0}
PLAN = [
    # 1. the normal way: walk (movement input), talk to the owner, out of the door, up the alley (teleport in steps)
    (10.0, "Quest.Dump", "start"),
    (10.1, "WALK", None),
    (13.0, "Quest.Dump", "after_walk"),
    (13.5, "Quest.Event talk owner", None),
    (14.0, DOOR, None),
    (16.0, "Quest.Dump", "at_door"),
    (16.5, CABLE, None),
    (19.0, "Quest.Dump", "normal_end"),
    # 2. a fresh start, then straight to the cable car (QA's teleport)
    (20.0, "Quest.Reset", None),
    (20.2, "BugItGo -193 -1500 150 0 90 0", None),
    (22.0, CABLE, None),
    (25.0, "Quest.Dump", "teleport_end"),
    # 3. a save on the first step, loaded, then out of the door
    (25.5, "BugItGo -193 -1500 150 0 90 0", None),
    (26.0, "Quest.Reset", None),
    (26.5, "Quest.Dump", "before_save"),
    (27.0, "lyud.Save questflow", None),
    (29.0, "lyud.Load 0", None),
    (44.0, "Quest.Dump", "loaded"),
    (44.5, DOOR, None),
    (47.0, "Quest.Dump", "load_end"),
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
    dt = min(dt, 0.25)
    state["t"] += dt
    if state["walk"] > 0.0:
        state["walk"] -= dt
        pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
        pawn.add_movement_input(pawn.get_actor_forward_vector(), 1.0)
    while state["step"] < len(PLAN) and state["t"] > PLAN[state["step"]][0]:
        _, cmd, label = PLAN[state["step"]]
        state["step"] += 1
        unreal.log_warning("QUESTFLOW %s %s" % (cmd, label or ""))
        if cmd == "WALK":
            state["walk"] = 2.5
            continue
        unreal.SystemLibrary.execute_console_command(w, cmd)
    if state["step"] >= len(PLAN) and state["t"] > PLAN[-1][0] + 1.0:
        unreal.log_warning("QUESTFLOW done")
        unreal.SystemLibrary.execute_console_command(w, "quit")


unreal.register_slate_post_tick_callback(tick)
