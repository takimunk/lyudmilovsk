"""Offscreen shots of the climbing vines (AProceduralVine, planted by UVinePlantings on Novikova 1's
courtyard face) in Lvl_Scene01: each shot puts the camera at a spot, holds every vine at an age and day
(Vine.ShowAt), waits for the growth to be simulated and drawn, logs "VINESHOT <name>" with frame times
and the vines' state (Vine.Report), and takes a HighResShot.

  unreal/scripts/shoot_vine.sh       (VINE_SHOTS=s_oct,s_winter for some only)
"""
import math
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
import world_map as W  # noqa: E402

FACE_Y = float(os.environ.get("ACADEM_FACE_Y", "790"))
RES = os.environ.get("VINE_RES", "1600x900")
# ensemble frame (x east, y north, metres over the ensemble's ground); the courtyard face is at x ~ -7
S_WIDE = ((-24.0, -21.0, 1.7), (-7.0, -26.5, 7.0))
N_WIDE = ((-24.0, 21.0, 1.7), (-8.5, 26.5, 7.0))
S_CLOSE = ((-11.5, -25.0, 1.6), (-6.4, -27.5, 2.6))
S_CORNER = ((-17.0, -40.0, 2.0), (-6.0, -30.0, 6.0))
YARD = ((-48.0, 0.0, 3.0), (-7.0, 0.0, 8.0))
FAR = ((-140.0, -5.0, 8.0), (-7.0, 0.0, 8.0))
# (name, (camera, target), console commands): the first holds the oldest age, so the growth is simulated once
SHOTS = [
    ("s_oct", S_WIDE, ["Vine.ShowAt"]),                       # the calendar: late October
    ("s_close_oct", S_CLOSE, ["Vine.ShowAt"]),
    ("s_corner_oct", S_CORNER, ["Vine.ShowAt"]),
    ("n_oct", N_WIDE, ["Vine.ShowAt"]),
    ("yard_oct", YARD, ["Vine.ShowAt"]),
    ("far_oct", FAR, ["Vine.ShowAt"]),
    ("s_summer", S_WIDE, ["Vine.ShowAt 15.6 200"]),
    ("s_close_summer", S_CLOSE, ["Vine.ShowAt 15.6 200"]),
    ("s_sept", S_WIDE, ["Vine.ShowAt 15.7 272"]),
    ("s_winter", S_WIDE, ["Vine.ShowAt 16.1 30"]),
    ("s_close_winter", S_CLOSE, ["Vine.ShowAt 16.1 30"]),
    ("s_spring", S_WIDE, ["Vine.ShowAt 15.4 150"]),
    ("s_age1", S_WIDE, ["Vine.ShowAt 1 230"]),
    ("s_age3", S_WIDE, ["Vine.ShowAt 3 230"]),
    ("s_age6", S_WIDE, ["Vine.ShowAt 6 230"]),
    ("s_age10", S_WIDE, ["Vine.ShowAt 10 230"]),
    # frame time with and without the vines, in this one process (VINE_SHOTS=ab_on,ab_off,...)
    ("ab_on", S_WIDE, ["Vine.ShowAt", "lyud.Vines 1"]),
    ("ab_off", S_WIDE, ["lyud.Vines 0"]),
    ("ab_yard_on", YARD, ["lyud.Vines 1"]),
    ("ab_yard_off", YARD, ["lyud.Vines 0"]),
    ("ab_close_on", S_CLOSE, ["lyud.Vines 1"]),
    ("ab_close_off", S_CLOSE, ["lyud.Vines 0"]),
    ("ab_on2", S_WIDE, ["lyud.Vines 1"]),
    ("ab_off2", S_WIDE, ["lyud.Vines 0"]),
]
if os.environ.get("VINE_SHOTS"):
    SHOTS = [s for s in SHOTS if s[0] in os.environ["VINE_SHOTS"].split(",")]
SHOTS = SHOTS * int(os.environ.get("VINE_ROUNDS", "1"))   # repeated rounds for A/B frame times
WAIT = float(os.environ.get("VINE_WAIT", "6"))
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
        if st["i"] >= len(SHOTS):
            cmd(w, "quit")
            st["phase"] = 9
            return
        name, (cam, tgt), cmds = SHOTS[st["i"]]
        first = [c for c in os.environ.get("VINE_CMDS", "").split(";") if c] if st["i"] == 0 else []
        for c in ["t.MaxFPS 0"] + first + cmds:
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
    elif st["phase"] == 1 and st["t"] - st["t0"] <= WAIT + (20.0 if st["i"] == 0 else 0.0):
        if st["t"] - st["t0"] > 2.0:
            st["frames"].append(dt)
    elif st["phase"] == 1:
        f = sorted(st["frames"]) or [0.0]
        unreal.log_warning("VINESHOT %s at %s: %d frames, median %.1f ms, worst %.1f ms" % (
            SHOTS[st["i"]][0], pawn.get_actor_location(), len(f), 1000 * f[len(f) // 2], 1000 * f[-1]))
        cmd(w, "Vine.Report")
        st["frames"] = []
        cmd(w, "HighResShot " + RES)
        st["phase"] = 2
        st["t0"] = st["t"]
    elif st["phase"] == 2 and st["t"] - st["t0"] > 1.5:
        st["phase"] = 0


unreal.register_slate_post_tick_callback(tick)
