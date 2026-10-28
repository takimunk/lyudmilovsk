"""Offscreen shots of a built MetaHuman on /Game/MetaHumans/L_MHShot (make_mh_shot_level.py), through each MHCam_*
camera: Saved/Screenshots/MacEditor, MHSHOT lines in the log say which is which.

    UnrealEditor <uproject> /Game/MetaHumans/L_MHShot -game -RenderOffscreen -ResX=1024 -ResY=1024 -windowed -nosound
        -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py unreal/scripts/shoot_mh.py"
"""
import os

import unreal

CAMS = os.environ.get("MH_CAMS", "MHCam_Face,MHCam_Face34,MHCam_Full").split(",")
st = {"t": 0.0, "i": 0, "shot": -1.0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "L_MHShot" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < float(os.environ.get("MH_WARMUP", "40")):
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    if st["i"] >= len(CAMS):
        if st["t"] - st["shot"] > 4:
            unreal.SystemLibrary.execute_console_command(w, "quit")
        return
    if st["shot"] < 0 or st["t"] - st["shot"] > 4:
        if st["shot"] >= 0:
            st["i"] += 1
            if st["i"] >= len(CAMS):
                return
        cams = unreal.GameplayStatics.get_all_actors_with_tag(w, CAMS[st["i"]])
        if cams:
            pc.set_view_target_with_blend(cams[0], 0.0)
        st["shot"] = st["t"]
        st["pending"] = True
        return
    if st.get("pending") and st["t"] - st["shot"] > 2:
        st["pending"] = False
        unreal.log_warning("MHSHOT %s" % CAMS[st["i"]])
        unreal.SystemLibrary.execute_console_command(w, "HighResShot 1")


unreal.register_slate_post_tick_callback(tick)
