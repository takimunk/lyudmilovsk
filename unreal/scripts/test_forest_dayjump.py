"""Moves the calendar from late October to 18 January while the forests of L_ForestPreview are drawn, and times how long
the far (baked) trees take to follow: prints DAYJUMP lines with the number of finished bakes every half second, and
shots at +1 s and +10 s. The camera hangs over the forest looking down.
  UnrealEditor <uproject> /Game/Dev/L_ForestPreview -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py <this file>"
"""
import unreal

st = {"t": 0, "clock": 0.0, "phase": "warm", "done0": 0, "next": 0.0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def forest_stats(w):
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.ProceduralForest):
        return a.describe_state()
    return ""


def done_count(w):
    s = forest_stats(w)
    i = s.find("; ")
    # "... ; N kinds baked (M queued or running); ..."
    try:
        part = s.split("; ")[1]
        return int(part.split(" kinds baked")[0]), int(part.split("(")[1].split(" ")[0])
    except Exception:
        return -1, -1


def place(w):
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    pawn.set_actor_location(unreal.Vector(-9000.0, 0.0, 2500.0), False, False)
    pc.set_control_rotation(unreal.Rotator(0.0, -9.0, 0.0))


def tick(dt):
    try:
        tick2(dt)
    except Exception as e:
        unreal.log_warning("DAYJUMP error %s" % e)
        unreal.SystemLibrary.execute_console_command(world(), "QUIT")


def tick2(dt):
    st["t"] += 1
    w = world()
    if not w:
        return
    if st["phase"] == "warm":
        if st["t"] == 1200:
            place(w)
        if st["t"] == 1500:
            done, pend = done_count(w)
            st["done0"] = done
            cal = [c for c in unreal.ObjectIterator(unreal.GameCalendar) if c.get_world() == w][0]
            unreal.log_warning("DAYJUMP before: %d kinds baked, date %s" % (done, cal.now()))
            cal.set_now(unreal.DateTime(2027, 1, 18, 12, 0, 0))
            unreal.SystemLibrary.execute_console_command(w, "HighResShot 1280x720 filename=/tmp/shots/jump_before.png")
            st["phase"] = "jump"
            st["clock"] = 0.0
            st["next"] = 0.5
    elif st["phase"] == "jump":
        st["clock"] += dt
        if st["clock"] >= st["next"]:
            done, pend = done_count(w)
            unreal.log_warning("DAYJUMP +%.1f s: %d baked in total (%d new), %d pending" % (st["clock"], done, done - st["done0"], pend))
            st["next"] += 0.5
            if abs(st["clock"] - 1.0) < 0.3:
                unreal.SystemLibrary.execute_console_command(w, "HighResShot 1280x720 filename=/tmp/shots/jump_1s.png")
        if st["clock"] > 14.0:
            unreal.SystemLibrary.execute_console_command(w, "HighResShot 1280x720 filename=/tmp/shots/jump_14s.png")
            st["phase"] = "end"
            st["t"] = 0
    elif st["phase"] == "end" and st["t"] > 60:
        unreal.SystemLibrary.execute_console_command(w, "QUIT")
        st["phase"] = "done"


unreal.register_slate_post_tick_callback(tick)
