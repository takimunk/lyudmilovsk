"""Offscreen check of the people's faces (docs/characters.md "Face") on /Game/Characters/Humans/L_FaceTest
(make_face_test_level.py): for each person a close-up, a burst of frames while they say a line (lyud.Face.Speak, the
lip-sync without sound), then while they chatter, so blinks, looks and the mouth show. Frames -> Saved/Screenshots,
FACESHOT lines in the log tell which is which (tools/face_gif.py makes GIFs).

  FACE_CAST=... FACE_LINES=line.nao.first.1,... UnrealEditor <uproject> /Game/Characters/Humans/L_FaceTest -game
      -RenderOffscreen -ResX=640 -ResY=640 -windowed -nosound -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log>
      "-ExecCmds=py <this file>"   (without MainMenuOnStart=0 the world starts paused under the menu)
"""
import os

import unreal

CAST = os.environ.get("FACE_CAST", "Nao,Owner,Girl_b,Kid_boy").split(",")
LINES = os.environ.get("FACE_LINES", "line.nao.first.1,line.owner.first.1,line.girl_b.first.1,line.kid_boy.first.1").split(",")
FRAMES = int(os.environ.get("FACE_FRAMES", "36"))
STEP = 0.12
RES = os.environ.get("FACE_RES", "640")
st = {"t": 0.0, "i": -1, "phase": 0, "n": 0, "t0": 0.0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "L_FaceTest" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < float(os.environ.get("FACE_WARMUP", "60")):
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    if st["phase"] == 0:
        st["i"] += 1
        if st["i"] >= len(CAST):
            cmd(w, "quit")
            st["phase"] = 9
            return
        who = CAST[st["i"]]
        cam = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.CameraActor)
                   if a.get_actor_label() == "FaceCam_" + who)
        pc.set_view_target_with_blend(cam, 0.0)
        cmd(w, "lyud.Face.Log 1")
        if os.environ.get("FACE_SUN_ANGLE"):            # a softer sun: the level's own is the engine default 0.54 deg
            for l in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DirectionalLight):
                l.light_component.set_editor_property("light_source_angle", float(os.environ["FACE_SUN_ANGLE"]))
                l.light_component.set_visibility(False)
                l.light_component.set_visibility(True)          # recreates its render state
        cmd(w, "fov %s" % os.environ.get("FACE_FOV", "22"))                                  # the camera manager keeps its own 90 otherwise
        st["phase"], st["t0"], st["n"] = 1, st["t"], 0
    elif st["phase"] == 1 and st["t"] - st["t0"] > 1.0:
        who = CAST[st["i"]]
        cmd(w, "lyud.Face.Speak SK_%s %s" % (who, LINES[st["i"]] if st["i"] < len(LINES) else LINES[0]))
        st["phase"], st["t0"] = 2, st["t"]
    elif st["phase"] == 2 and st["t"] - st["t0"] >= STEP:
        unreal.log_warning("FACESHOT %s speak %03d" % (CAST[st["i"]], st["n"]))
        cmd(w, "HighResShot %sx%s" % (RES, RES))
        st["n"] += 1
        st["t0"] = st["t"]
        if st["n"] >= FRAMES:
            cmd(w, "lyud.Face.Chatter 1")
            st["phase"], st["n"] = 3, 0
    elif st["phase"] == 3 and st["t"] - st["t0"] >= STEP:
        unreal.log_warning("FACESHOT %s chatter %03d" % (CAST[st["i"]], st["n"]))
        cmd(w, "HighResShot %sx%s" % (RES, RES))
        st["n"] += 1
        st["t0"] = st["t"]
        if st["n"] >= FRAMES:
            cmd(w, "lyud.Face.Chatter 0")
            st["phase"] = 0


unreal.register_slate_post_tick_callback(tick)
