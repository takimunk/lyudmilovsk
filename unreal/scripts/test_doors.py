"""Offscreen check of the doors in /Game/Dev/L_DoorTest (make_door_test_level.py): runs Door.SelfTest (E, fit,
NPCs, locks), then saves, flips every door, loads the save and runs Door.CheckSaved. Prints DOORTEST lines and quits.

  UnrealEditor <uproject> /Game/Dev/L_DoorTest -game -RenderOffscreen -dpcvars=lyud.MainMenuOnStart=0
      -abslog=<log> "-ExecCmds=py unreal/scripts/test_doors.py"     (unreal/scripts/test_doors.sh does it)
"""
import unreal

state = {"t": 0.0, "phase": 0, "at": 0.0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "L_DoorTest" and unreal.GameplayStatics.get_player_pawn(w, 0):
            return w
    return None


def run(w, cmd):
    unreal.log_warning("DOORTEST run " + cmd)
    unreal.SystemLibrary.execute_console_command(w, cmd)


# phase: (seconds after the previous one, command)
PLAN = [
    (6.0, "Door.SelfTest"),
    (50.0, "lyud.Save doortest"),
    (2.0, "lyud.Door.Lock DoorTest_Locked 0"),
    (0.5, "lyud.Door.Toggle DoorTest"),
    (3.0, "lyud.Door.List"),
    (0.5, "lyud.Load 0"),
    (12.0, "Door.CheckSaved"),
    (1.0, "lyud.Door.List"),
    (1.0, "QUIT"),
]


def tick(dt):
    state["t"] += dt
    w = world()
    if not w:
        return
    if state["phase"] < len(PLAN) and state["t"] - state["at"] >= PLAN[state["phase"]][0]:
        state["at"] = state["t"]
        run(w, PLAN[state["phase"]][1])
        state["phase"] += 1


unreal.register_slate_post_tick_callback(tick)
