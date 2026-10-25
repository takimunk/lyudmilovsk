"""Frame-time spikes while moving through the forest of L_ForestPreview (offscreen -game): after the forests have baked
the pawn is carried along a line through them at WALK_SPEED cm/s (eye height) and then, high, back along a path
over the tree tops; every frame time is recorded. Prints FORESTWALK lines: median, p99, worst, and how many frames
were over 100 ms, for the walk and for the flight. Use lyud.Forest.MaxLive=0 for the no-live-plants reference.
  UnrealEditor <uproject> /Game/Dev/L_ForestPreview -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py <this file>"
"""
import statistics
import unreal

WARMUP = 1500
WALK_SPEED = 1500.0   # cm/s, a run
FLY_SPEED = 2500.0
st = {"t": 0, "phase": "warm", "x": -9000.0, "dts": [], "clock": 0.0}


def world():
    a = unreal.find_object(None, "/Game/Dev/L_ForestPreview.L_ForestPreview:PersistentLevel.ProceduralPlant_0")
    return a.get_world() if a else None


def pawn():
    w = world()
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    return pc, (pc.get_controlled_pawn() if pc else None)


def report(name):
    d = st["dts"]
    ms = sorted(x * 1000.0 for x in d)
    unreal.log_warning("FORESTWALK %s median %.1f ms, p99 %.1f ms, worst %.1f ms, %d of %d frames over 100 ms" % (
        name, statistics.median(ms), ms[int(len(ms) * 0.99)], ms[-1], sum(1 for x in ms if x > 100.0), len(ms)))
    st["dts"] = []


def tick(dt):
    st["t"] += 1
    pc, p = pawn()
    if st["phase"] == "warm":
        if st["t"] == WARMUP and p:
            st["phase"] = "walk"
            st["x"] = -9000.0
            st["dts"] = []
        return
    if not p:
        return
    speed = WALK_SPEED if st["phase"] == "walk" else FLY_SPEED
    st["x"] += speed * dt
    z = 170.0 if st["phase"] == "walk" else 600.0
    p.set_actor_location(unreal.Vector(st["x"], 0.0, z), False, False)
    pc.set_control_rotation(unreal.Rotator(0.0, 0.0 if st["phase"] == "walk" else -12.0, 0.0))
    st["dts"].append(dt)
    if st["x"] > 30000.0:
        report(st["phase"])
        if st["phase"] == "walk":
            st["phase"] = "fly"
            st["x"] = -9000.0
        else:
            unreal.SystemLibrary.execute_console_command(world(), "QUIT")
            st["phase"] = "done"


unreal.register_slate_post_tick_callback(tick)
