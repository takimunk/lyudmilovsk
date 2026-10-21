"""Offscreen screenshots of the ground (no PIE, no input, the desktop is left alone): stands the
player, flying, at a few spots of Lvl_TerrainPreview (make_terrain_preview.sh) or Lvl_Scene01 and
takes a 1600x900 shot at each. Screenshots land in Saved/Screenshots/Mac in the order of SPOTS.

  UnrealEditor <uproject> /Game/Scene01/Lvl_TerrainPreview -game -RenderOffscreen -ResX=1600 -ResY=900 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py <this file>"
  (TERRAIN_FACE_Y=<portal face, Unreal Y cm> for Lvl_Scene01; the preview level's is 790)
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
import terrain as T  # noqa: E402

FACE_Y = float(os.environ.get("TERRAIN_FACE_Y", "790"))
# (name, map x, y, metres above the ground, pitch, yaw); yaw 0 looks west to the sea, 90 north
SPOTS = [
    ("door_north", T.W.DOOR_X, 5.0, 1.0, -6, 90),
    ("portal", 0.0, 16.0, 1.0, -4, -90),
    ("portal_side", 14.0, 9.0, 2.0, -15, -140),
    ("hill_meadow_sea", -60.0, 80.0, 1.0, -8, 10),
    ("hill_meadow_inland", -60.0, 80.0, 1.0, -10, 200),
    ("track", -12.0, 140.0, 1.0, -12, 100),
    ("forest_edge", -100.0, -20.0, 1.0, -6, 180),
    ("flank_beach", 95.0, 20.0, 1.0, -10, 100),
    ("overview", 30.0, -250.0, 90.0, -18, 100),
    ("ground_close", -40.0, 60.0, 1.0, -55, 130),
    ("bluff_from_beach", 125.0, -60.0, 1.0, 12, 160),
    ("bluff_along", 118.0, 80.0, 1.0, 8, -100),
    ("bluff_top_edge", 70.0, -150.0, 1.0, -25, 20),
    ("escarpment", 60.0, 230.0, 2.0, 0, 120),
]
# TERRAIN_SHOTS=bluff_along,portal runs just those spots
if os.environ.get("TERRAIN_SHOTS"):
    SPOTS = [s for s in SPOTS if s[0] in os.environ["TERRAIN_SHOTS"].split(",")]
WAIT = 4.0
st = {"t": 0.0, "i": -1, "phase": 0, "frames": []}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name().startswith("Lvl_") and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < 4.0:
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    if st["phase"] == 0:
        st["i"] += 1
        if st["i"] >= len(SPOTS):
            cmd(w, "quit")
            st["phase"] = 9
            return
        name, x, y, up, pitch, yaw = SPOTS[st["i"]]
        cmd(w, "t.MaxFPS 0")
        if hasattr(pawn, "character_movement"):
            pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
        ux, uy, uz = T.to_unreal(x, y, T.ground_z(x, y) + up, FACE_Y)
        pawn.set_actor_location(unreal.Vector(ux, uy, uz), False, True)
        pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))
        st["t0"] = st["t"]
        st["phase"] = 1
    elif st["phase"] == 1 and st["t"] - st["t0"] <= WAIT:
        if st["t"] - st["t0"] > 1.5:
            st["frames"].append(dt)                   # frame times once the spot has streamed in
    elif st["phase"] == 1:
        f = sorted(st["frames"]) or [0.0]
        unreal.log_warning("TERRAINSHOT %s at %s: %d frames, median %.1f ms, worst %.1f ms" % (
            SPOTS[st["i"]][0], pawn.get_actor_location(), len(f), 1000 * f[len(f) // 2], 1000 * f[-1]))
        st["frames"] = []
        cmd(w, "HighResShot 1600x900")
        st["phase"] = 2
        st["t0"] = st["t"]
    elif st["phase"] == 2 and st["t"] - st["t0"] > 1.0:
        st["phase"] = 0


unreal.register_slate_post_tick_callback(tick)
