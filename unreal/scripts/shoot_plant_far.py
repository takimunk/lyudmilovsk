"""Shots of the trees in L_PlantFar from 30, 80, 200 and 400 m, first in full detail (SummaryDistance out of reach),
then as they really switch (their far view from SummaryDistance on); then the crown tops close up from the Top_*
cameras, live (top_live_*) and as their bake (top_bake_*, the same mesh forests draw). Offscreen -game; FAR_SHOTS is the folder.
  UnrealEditor <uproject> /Game/Dev/L_PlantFar -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 "-ExecCmds=py <this file>"
"""
import os
import unreal

SHOTS = os.environ.get("FAR_SHOTS", "/tmp/shots")
DISTS = (30, 80, 200, 400)
PITCH = {30: 14.0, 80: 6.0, 200: 2.0, 400: 1.0}
MODES = (("full", 1.0e9), ("far", 15000.0))
WARMUP = 400
HOLD = 240
state = {"t": 0, "i": -1}
plant_cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralPlant")


def world():
    a = unreal.find_object(None, "/Game/Dev/L_PlantFar.L_PlantFar:PersistentLevel.ProceduralPlant_0")
    return a.get_world() if a else None


def setup(i):
    w = world()
    mode, dist_cut = MODES[i // len(DISTS)]
    d = DISTS[i % len(DISTS)]
    for p in unreal.GameplayStatics.get_all_actors_of_class(w, plant_cls):
        p.set_editor_property("summary_distance", dist_cut)
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn() if pc else None
    if pawn:
        pawn.set_actor_location(unreal.Vector(0, -d * 100, 100), False, True)
    if pc:
        pc.set_control_rotation(unreal.Rotator(0.0, PITCH[d], 90.0))
    return mode, d


def tick(dt):
    state["t"] += 1
    t = state["t"]
    if state["i"] < 0:
        if t >= WARMUP:
            state["i"] = 0
            state["t"] = 0
            setup(0)
        return
    total = len(MODES) * len(DISTS)
    i = state["i"]
    if i >= total:
        tops(i - total, t)
        return
    if t % 20 == 1:
        setup(i)
    if t == HOLD:
        mode, d = setup(i)
        unreal.SystemLibrary.execute_console_command(world(), "HighResShot 1280x720 filename=%s/%s_%03dm.png" % (SHOTS, mode, d))
        unreal.log_warning("PLANTFAR shot %s %d m" % (mode, d))
    if t == HOLD + 30:
        state["i"] += 1
        state["t"] = 0



def tops(k, t):
    """k: 0..7, live then bake, one camera each."""
    w = world()
    cams = sorted([c for c in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.CameraActor) if c.get_actor_label().startswith("Top_")],
                  key=lambda c: c.get_actor_label())
    if k >= 2 * len(cams):
        unreal.SystemLibrary.execute_console_command(w, "QUIT")
        return
    bake = k >= len(cams)
    cam = cams[k % len(cams)]
    if t == 1:
        for p in unreal.GameplayStatics.get_all_actors_of_class(w, plant_cls):
            p.set_editor_property("summary_distance", 0.0 if bake else 1.0e9)
        pc = unreal.GameplayStatics.get_player_controller(w, 0)
        pc.set_view_target_with_blend(cam, 0.0)
    if t == HOLD:
        name = "%s_%s" % ("top_bake" if bake else "top_live", cam.get_actor_label()[4:])
        unreal.SystemLibrary.execute_console_command(w, "HighResShot 1280x720 filename=%s/%s.png" % (SHOTS, name))
        unreal.log_warning("PLANTFAR shot " + name)
    if t == HOLD + 30:
        state["i"] += 1
        state["t"] = 0


unreal.register_slate_post_tick_callback(tick)
