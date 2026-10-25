"""Offscreen screenshots of the shared life clips on three humans: flies the player over
/Game/Characters/Humans/L_LifeAnimsPreview (make_life_preview_level.py) and takes a 1600x900 shot at
each spot; screenshots land in Saved/Screenshots/Mac in the order of SPOTS.
(Adapted from the humans worktree's shoot_humans.py.)

  UnrealEditor <uproject> /Game/Characters/Humans/L_LifeAnimsPreview -game -RenderOffscreen -ResX=1600
      -ResY=900 -windowed -abslog=<log> "-ExecCmds=py <this file>"
"""
import os

import unreal

EYE = 64.0                           # camera above the pawn origin
# (name, camera x, y, z, pitch, yaw); band i (Guy_a, Kid_boy, Girl_b) at y = -800 i, clips at x = 260 j
SPOTS = []          # must match make_life_preview_level.py: camera at (x + 120, y + 265)
for _i, _who in enumerate(os.environ.get("LIFE_CAST", "Guy_a,Kid_boy,Girl_b").split(",")):
    for _j, _clip in enumerate(os.environ.get("LIFE_CLIPS", "Walk,Talk,SitIdle,Eat,Read,Sleep,LieDown,SitDown").split(",")):
        SPOTS.append(("%s_%s" % (_who, _clip), 260.0 * _j + 120.0, -800.0 * _i + 245.0, 0, 0, 0))
if os.environ.get("LIFESHOT_SPOTS"):
    SPOTS = [s for s in SPOTS if s[0] in os.environ["LIFESHOT_SPOTS"].split(",")]
WAIT = 1.5
st = {"t": 0.0, "i": -1, "phase": 0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "L_LifeAnimsPreview" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < float(os.environ.get("LIFESHOT_WARMUP", "90")):   # shaders compile first
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    if st["phase"] == 0:
        st["i"] += 1
        if st["i"] >= len(SPOTS):
            cmd(w, "quit")
            st["phase"] = 9
            return
        name, x, y, z, pitch, yaw = SPOTS[st["i"]]          # the level's camera nearest the spot
        cams = sorted(unreal.GameplayStatics.get_all_actors_of_class(w, unreal.CameraActor),
                      key=lambda a: (a.get_actor_location() - unreal.Vector(x, y + 20.0, a.get_actor_location().z)).length())
        if pawn:
            pawn.set_actor_hidden_in_game(True)
        pc.set_view_target_with_blend(cams[0], 0.0)
        st["cam"] = cams[0].get_actor_location()
        st["t0"] = st["t"]
        st["phase"] = 1
    elif st["phase"] == 1 and st["t"] - st["t0"] > WAIT:
        cam = pc.player_camera_manager
        unreal.log_warning("LIFESHOT %s cam %s pawn=%s at %s cam %s" % (
            SPOTS[st["i"]][0], st.get("cam"), pawn.get_name() if pawn else None, pawn.get_actor_location() if pawn else None,
            cam.get_camera_location() if cam else None))
        cmd(w, "HighResShot 1600x900")
        st["t0"] = st["t"]
        st["phase"] = 2
    elif st["phase"] == 2 and st["t"] - st["t0"] > 1.5:
        st["phase"] = 0


unreal.register_slate_post_tick_callback(tick)
