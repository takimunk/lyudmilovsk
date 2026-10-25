"""Shoots every camera of /Game/Dev/L_BirdSpecies (make_bird_species_level.py), offscreen -game. BIRD_SHOTS is the folder,
BIRD_CAMS an optional comma-free filter (camera label prefix, e.g. "Side_").
  UnrealEditor <uproject> /Game/Dev/L_BirdSpecies -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 "-ExecCmds=py <this file>"
"""
import os

import unreal

SHOTS = os.environ.get("BIRD_SHOTS", "/tmp/bird_shots")
FILTER = os.environ.get("BIRD_CAMS", "")
WARMUP = int(os.environ.get("BIRD_WARMUP", "500"))
HOLD = 90
state = {"t": 0, "i": 0, "cams": None}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "L_BirdSpecies" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w
    return None


def tick(dt):
    state["t"] += 1
    w = world()
    if w is None:
        return
    if state["cams"] is None:
        if state["t"] < WARMUP:
            return
        cams = [c for c in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.CameraActor) if c.get_actor_label().startswith(FILTER)]
        state["cams"] = sorted(cams, key=lambda c: c.get_actor_label())
        state["t"] = 0
        os.makedirs(SHOTS, exist_ok=True)
        unreal.log_warning("BIRDSHOT %d cameras" % len(state["cams"]))
    cams = state["cams"]
    i = state["i"]
    if i >= len(cams):
        unreal.SystemLibrary.execute_console_command(w, "QUIT")
        return
    t = state["t"]
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    if t == 1:
        pc.set_view_target_with_blend(cams[i], 0.0)
    if t == HOLD:
        unreal.SystemLibrary.execute_console_command(w, "HighResShot 1280x720 filename=%s/%s.png" % (SHOTS, cams[i].get_actor_label()))
    if t == HOLD + 20:
        state["i"] += 1
        state["t"] = 0


unreal.register_slate_post_tick_callback(tick)
