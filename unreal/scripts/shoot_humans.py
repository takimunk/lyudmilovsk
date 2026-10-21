"""Offscreen screenshots of the humans preview level (no editor window, no PIE, no input): flies the
player to a few spots of /Game/Characters/Humans/L_HumansPreview (make_humans_preview_level.py) and
takes a 1600x900 shot at each; screenshots land in Saved/Screenshots/Mac in the order of SPOTS.

  UnrealEditor <uproject> /Game/Characters/Humans/L_HumansPreview -game -RenderOffscreen -ResX=1600
      -ResY=900 -windowed -abslog=<log> "-ExecCmds=py <this file>"
HUMANSHOT_SPOTS=owner_face,owner_look runs just those.
"""
import os

import unreal

EYE = 64.0                           # camera above the pawn origin
# (name, camera x, y, z, pitch, yaw); people sit at x = 140 * i, y = 0, facing +Y (desk at +Y)
SPOTS = [
    ("owner_face", 282.0, 62.0, 128.0, -6, -92),
    ("owner_seated", 360.0, 170.0, 150.0, -14, -125),
    ("owner_look", 1402.0, 62.0, 150.0, -3, -92),
    ("owner_walk", 1600.0, 190.0, 160.0, -12, -115),
    ("owner_standup", 1740.0, 190.0, 160.0, -12, -115),
    ("row", 640.0, 330.0, 200.0, -16, -100),
]
if os.environ.get("HUMANSHOT_SPOTS"):
    SPOTS = [s for s in SPOTS if s[0] in os.environ["HUMANSHOT_SPOTS"].split(",")]
WAIT = 3.0
st = {"t": 0.0, "i": -1, "phase": 0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "L_HumansPreview" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < float(os.environ.get("HUMANSHOT_WARMUP", "90")):   # shaders compile first
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    if st["phase"] == 0:
        st["i"] += 1
        if st["i"] >= len(SPOTS):
            cmd(w, "quit")
            st["phase"] = 9
            return
        name, x, y, z, pitch, yaw = SPOTS[st["i"]]
        if pawn:
            if hasattr(pawn, "character_movement"):
                pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
            pawn.set_actor_enable_collision(False)
            pawn.set_actor_hidden_in_game(True)
            pawn.set_actor_location(unreal.Vector(x, y, z - EYE), False, True)
        pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))
        st["t0"] = st["t"]
        st["phase"] = 1
    elif st["phase"] == 1 and st["t"] - st["t0"] > WAIT:
        cam = pc.player_camera_manager
        unreal.log_warning("HUMANSHOT %s pawn=%s at %s cam %s" % (
            SPOTS[st["i"]][0], pawn.get_name() if pawn else None, pawn.get_actor_location() if pawn else None,
            cam.get_camera_location() if cam else None))
        cmd(w, "HighResShot 1600x900")
        st["t0"] = st["t"]
        st["phase"] = 2
    elif st["phase"] == 2 and st["t"] - st["t0"] > 1.5:
        st["phase"] = 0


unreal.register_slate_post_tick_callback(tick)
