"""Frame cost of the forest in L_ForestPreview, offscreen -game, the camera standing inside it: after the forests have
baked, the median frame time of 300 frames at each of a few viewpoints. Prints FORESTBENCH lines. Run with
tools-style command line:
  UnrealEditor <uproject> /Game/Dev/L_ForestPreview -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py <this file>"
"""
import os
import statistics
import unreal

SHOTS = os.environ.get("FOREST_SHOTS", "/tmp/shots")
SHOT_PREFIX = os.environ.get("FOREST_SHOT_PREFIX", "shot")

WARMUP = int(os.environ.get("FOREST_WARMUP", "1500"))
SAMPLE = int(os.environ.get("FOREST_SAMPLE", "300"))
# the forest's western edge is at x = -6000 cm; the camera stands D metres out from it, eye height, looking east into it.
# FOREST_VIEWS="name:x:y:z:pitch;..." (cm, degrees; yaw 0) replaces them
VIEWS = [("d%03dm" % d, (-6000 - d * 100, 0, 170), (0, 0, 0)) for d in (10, 30, 80, 200)] + [("inside", (-1200, 0, 170), (0, 0, 0)), ("end", (-1200, 0, 170), (0, 0, 0))]
if os.environ.get("FOREST_VIEWS"):
    VIEWS = []
    for v in os.environ["FOREST_VIEWS"].split(";"):
        f = v.split(":")
        n, x, y, z, pitch = f[:5]
        VIEWS.append((n, (float(x), float(y), float(z)), (float(pitch), float(f[5]) if len(f) > 5 else 0.0, 0)))
    VIEWS.append(("end",) + VIEWS[-1][1:])
state = {"t": 0, "view": -1, "dts": [], "phase": "warm"}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def place(i):
    w = world()
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn() if pc else None
    _, loc, rot = VIEWS[i]
    if pawn:
        pawn.set_actor_location(unreal.Vector(*loc), False, False)
    if pc:
        pc.set_control_rotation(unreal.Rotator(rot[2], rot[0], rot[1]))


def tick(dt):
    state["t"] += 1
    t = state["t"]
    if state["view"] < 0:
        if t == WARMUP:
            state["view"] = 0
            state["t"] = 0
            place(0)
            state["dts"] = []
        return
    if t == 1 or t % 100 == 1:
        place(state["view"])
    if t > 60:
        state["dts"].append(dt * 1000.0)
    if t == SAMPLE + 60:
        d = state["dts"]
        unreal.log_warning("FORESTBENCH %s median %.2f ms (p90 %.2f)" % (VIEWS[state["view"]][0], statistics.median(d), sorted(d)[int(len(d) * 0.9)]))
        unreal.SystemLibrary.execute_console_command(world(), "HighResShot 1280x720 filename=%s/%s_%s.png" % (SHOTS, SHOT_PREFIX, VIEWS[state["view"]][0]))
        state["view"] += 1
        state["t"] = -60
        state["dts"] = []
        if state["view"] >= len(VIEWS):
            unreal.SystemLibrary.execute_console_command(world(), "QUIT")


unreal.register_slate_post_tick_callback(tick)
