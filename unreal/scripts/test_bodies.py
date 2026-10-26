"""Offscreen frames of /Game/Characters/Humans/L_BodyTest (make_body_test_level.py): for each camera a burst of
frames (people at their computers working through their loops; people on the ramp and the step with their feet on
the ground). FRAMES each, STEP seconds apart; BODYSHOT lines in the log name them (tools/face_gif.py makes GIFs).

  UnrealEditor <uproject> /Game/Characters/Humans/L_BodyTest -game -RenderOffscreen -ResX=960 -ResY=640 -windowed
      -nosound -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py <this file>"
"""
import os

import unreal

CAMS = os.environ.get("BODY_CAMS", "Kid_boy,Guy_a,Girl_b,Owner,Kid_boy_side,Guy_a_side,Ramp,Step,Flat").split(",")
FRAMES = int(os.environ.get("BODY_FRAMES", "20"))
STEP = float(os.environ.get("BODY_STEP", "0.7"))
st = {"t": 0.0, "i": -1, "phase": 0, "n": 0, "t0": 0.0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "L_BodyTest" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < float(os.environ.get("BODY_WARMUP", "60")):
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    if st["phase"] == 0:
        st["i"] += 1
        if st["i"] >= len(CAMS):
            cmd(w, "quit")
            st["phase"] = 9
            return
        if st["i"] == 0:
            cmd(w, "lyud.Body.OnFeet 1")
            cmd(w, "lyud.Face.Log 1")
        cam = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.CameraActor)
                   if a.get_actor_label() == "BodyCam_" + CAMS[st["i"]])
        pc.set_view_target_with_blend(cam, 0.0)
        cmd(w, "fov 50")
        st["phase"], st["t0"], st["n"] = 1, st["t"], 0
    elif st["phase"] == 1 and st["t"] - st["t0"] >= STEP:
        unreal.log_warning("BODYSHOT %s view %03d" % (CAMS[st["i"]], st["n"]))
        cmd(w, "HighResShot 960x640")
        st["n"] += 1
        st["t0"] = st["t"]
        if st["n"] >= FRAMES:
            st["phase"] = 0


unreal.register_slate_post_tick_callback(tick)
