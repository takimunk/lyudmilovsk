"""Offscreen shots of the Academgorodok in Lvl_Scene01 (no PIE, no window): the camera stands where
the reference photos were taken (blender/academgorodok/build_academgorodok.py VIEWS) and at 5, 30,
100 and 300 m, flies the pawn there and takes a HighResShot. Shots land in Saved/Screenshots/Mac in
the order of SPOTS; the log has an "ACADEMSHOT" line per spot with the frame times.

  unreal/scripts/shoot_academgorodok.sh            (ACADEM_SHOTS=n1_front,s_gate for some only)
"""
import math
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
import world_map as W  # noqa: E402

FACE_Y = float(os.environ.get("ACADEM_FACE_Y", "790"))
RES = os.environ.get("ACADEM_RES", "1600x900")
# (name, camera (x, y, z over the ensemble's ground), target (x, y, z), console commands first);
# ensemble frame: x east (the square), y north, metres
SPOTS = [
    ("n1_front", (32.75, 1.3, 1.6), (0.0, 1.3, 7.5)),            # commons_Marshal_Novikov_str._1_2_.jpg
    ("n1_panorama", (62.0, 1.3, 1.6), (0.0, 1.3, 8.0)),          # commons_Marshal_Novikov_str._1_1_.jpg
    ("s_gate", (24.0, -40.0, 1.6), (4.0, -38.5, 5.0)),           # commons_Marshal_Novikov_str._1_gates.jpg
    ("m8_corner", (33.2, -55.1, 1.6), (22.3, -60.1, 6.5)),       # commons_Maksimova_str._8_1_.jpg
    ("n3_street", (-45.0, 140.0, 1.6), (-20.0, 95.0, 5.0)),      # 2025 street photo, heading 157
    ("aerial", (130.0, -150.0, 85.0), (-5.0, -10.0, 0.0)),       # pastvu_1983687.jpg
    ("d5_portal", (12.5, 0.0, 1.6), (7.25, 0.0, 3.0)),           # 5 m from the central portal
    ("d30", (40.0, -12.0, 1.6), (0.0, 0.0, 6.0)),
    ("d100_station", (96.0, 16.0, 1.7), (0.0, 0.0, 7.0)),        # beside the lower station, the square ahead
    ("d300", (300.0, 90.0, 6.0), (0.0, 0.0, 8.0)),
    ("cable_car", (330.0, 170.0, 30.0), (0.0, 0.0, 6.0)),        # from the cabin, coming down
    ("yard", (-40.0, 10.0, 1.6), (0.0, 0.0, 8.0)),
    ("gastronom", (1.0, -5.5, 2.3), (12.0, -1.0, 1.5)),          # inside, through the open portal
]
if os.environ.get("ACADEM_SHOTS"):
    SPOTS = [s for s in SPOTS if s[0] in os.environ["ACADEM_SHOTS"].split(",")]
WAIT = float(os.environ.get("ACADEM_WAIT", "6"))
st = {"t": 0.0, "i": -1, "phase": 0, "frames": []}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name().startswith("Lvl_") and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def ue(p):
    x, y = W.academ_to_map(p[0], p[1])
    return W.to_unreal(x, y, W.academ_z() + p[2], FACE_Y)


def tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < 5.0:
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    if st["phase"] == 0:
        st["i"] += 1
        if st["i"] >= len(SPOTS):
            cmd(w, "quit")
            st["phase"] = 9
            return
        name, cam, tgt = SPOTS[st["i"]][:3]
        for c in ["t.MaxFPS 0"] + list(SPOTS[st["i"]][3] if len(SPOTS[st["i"]]) > 3 else []):
            cmd(w, c)
        if hasattr(pawn, "character_movement"):
            pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
        a, b = ue(cam), ue(tgt)
        eye = 0.0
        cam_comp = pawn.get_component_by_class(unreal.CameraComponent)
        if cam_comp:
            eye = cam_comp.get_world_location().z - pawn.get_actor_location().z
        pawn.set_actor_enable_collision(False)
        pawn.set_actor_location(unreal.Vector(a[0], a[1], a[2] - eye), False, True)
        dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
        pc.set_control_rotation(unreal.Rotator(roll=0, pitch=math.degrees(math.atan2(dz, math.hypot(dx, dy))),
                                               yaw=math.degrees(math.atan2(dy, dx))))
        st["t0"] = st["t"]
        st["phase"] = 1
    elif st["phase"] == 1 and st["t"] - st["t0"] <= WAIT:
        if st["t"] - st["t0"] > 2.0:
            st["frames"].append(dt)
    elif st["phase"] == 1:
        f = sorted(st["frames"]) or [0.0]
        unreal.log_warning("ACADEMSHOT %s at %s: %d frames, median %.1f ms, worst %.1f ms" % (
            SPOTS[st["i"]][0], pawn.get_actor_location(), len(f), 1000 * f[len(f) // 2], 1000 * f[-1]))
        st["frames"] = []
        cmd(w, "HighResShot " + RES)
        st["phase"] = 2
        st["t0"] = st["t"]
    elif st["phase"] == 2 and st["t"] - st["t0"] > 1.5:
        st["phase"] = 0


unreal.register_slate_post_tick_callback(tick)
