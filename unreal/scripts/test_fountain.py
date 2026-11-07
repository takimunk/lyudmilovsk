"""Offscreen test of the Academgorodok fountain (ALyudFountain, docs/fountain.md) in Lvl_Scene01: no PIE, no window.
Each shot sets the calendar (UGameCalendar.set_now), stands the camera at a spot round the fountain, waits, logs
"FOUNTAINSHOT <name>" with the frame times and the fountain's state, checks it, and takes a HighResShot. The season
checks: running on the game's start (20 October), drained after the first frost and in winter, running again in May;
the sound plays near the running fountain and is silent when it is drained or far.

  unreal/scripts/test_fountain.sh            (FOUNTAIN_SHOTS=front,close for some only)
"""
import math
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
sys.path.insert(0, os.path.join(HERE, "..", "..", "blender", "academgorodok"))
import world_map as W  # noqa: E402
import plan as P  # noqa: E402

FACE_Y = float(os.environ.get("ACADEM_FACE_Y", "790"))
RES = os.environ.get("FOUNTAIN_RES", "1600x900")
FX, FY = P.SQUARE["fountain"]
D = unreal.DateTime
OCT = D(2026, 10, 21, 12, 30, 0, 0)
# spots in the ensemble frame (x east, y north, metres over its ground) relative to the fountain
FRONT = ((FX + 6.0, FY + 0.8, 1.7), (FX, FY, 1.4))
CLOSE = ((FX + 1.9, FY + 1.3, 1.9), (FX, FY, 1.35))
JET = ((FX + 1.3, FY - 0.6, 2.2), (FX, FY, 2.1))
BASIN = ((FX + 1.2, FY + 0.2, 1.75), (FX + 0.2, FY, 1.2))
FAR = ((FX + 12.0, FY - 0.2, 1.7), (FX, FY, 1.8))
N1 = ((FX - 8.0, FY + 2.0, 1.7), (FX, FY, 1.4))          # from Novikova 1's door, the street behind
SIDE = ((FX + 0.5, FY + 6.5, 1.6), (FX, FY, 1.6))
# (name, (camera, target), date, console commands, expect running, sound: None no check, ">" plays, "0" silent)
SHOTS = [
    ("front", FRONT, OCT, [], True, ">"),
    ("close", CLOSE, OCT, [], True, ">"),
    ("jet", JET, OCT, [], True, None),
    ("basin", BASIN, OCT, [], True, None),
    ("side", SIDE, OCT, [], True, None),
    ("n1_view", N1, OCT, [], True, None),
    ("far", FAR, OCT, [], True, None),
    ("evening", FRONT, D(2026, 10, 21, 18, 30, 0, 0), [], True, None),
    ("frost_drained", FRONT, D(2026, 11, 27, 12, 0, 0, 0), [], False, "0"),
    ("frost_close", CLOSE, D(2026, 11, 27, 12, 0, 0, 0), [], False, None),
    ("winter", FRONT, D(2027, 1, 20, 12, 0, 0, 0), [], False, "0"),
    ("winter_close", CLOSE, D(2027, 1, 20, 12, 0, 0, 0), [], False, None),
    ("april", FRONT, D(2027, 4, 25, 12, 0, 0, 0), [], False, None),
    ("may", FRONT, D(2027, 5, 10, 12, 0, 0, 0), [], True, ">"),
    ("summer", CLOSE, D(2027, 7, 12, 11, 0, 0, 0), [], True, None),
    # the 'string' Dima saw from the fountain: in dry weather, from the side, low
    ("line_rain", FRONT, OCT, [], True, None),
    ("line_clear", FRONT, OCT, ["lyud.Weather.Preset clear"], True, None),
    ("line_side", ((FX + 3.0, FY + 5.0, 1.7), (FX, FY, 0.8)), OCT, [], True, None),
    ("line_low", ((FX + 4.0, FY - 2.5, 1.0), (FX, FY, 0.5)), OCT, ["lyud.Weather.Preset off"], True, None),
    ("forced_off", FRONT, OCT, ["lyud.Fountain.Force 0"], False, None),
    ("far_quiet", ((FX + 70.0, FY, 1.7), (FX, FY, 1.7)), OCT, ["lyud.Fountain.Force -1"], True, "0"),
    # frame time with and without the fountain in this one process (FOUNTAIN_SHOTS=ab_on,ab_off,ab_on2,ab_off2)
    ("ab_on", FRONT, OCT, ["lyud.Fountain.Force -1", "lyud.Academ 1"], True, None),
    ("ab_off", FRONT, OCT, ["lyud.Fountain.Force 0"], False, None),
    ("ab_on2", FRONT, OCT, ["lyud.Fountain.Force -1"], True, None),
    ("ab_off2", FRONT, OCT, ["lyud.Fountain.Force 0"], False, None),
]
if os.environ.get("FOUNTAIN_SHOTS"):
    SHOTS = [s for s in SHOTS if s[0] in os.environ["FOUNTAIN_SHOTS"].split(",")]
WAIT = float(os.environ.get("FOUNTAIN_WAIT", "10"))
st = {"t": 0.0, "i": -1, "phase": 0, "frames": [], "fail": 0, "levels": []}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name().startswith("Lvl_") and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def ue(p):
    x, y = W.academ_to_map(p[0], p[1])
    return W.to_unreal(x, y, W.academ_z() + p[2], FACE_Y)


def fountain(w):
    for f in unreal.ObjectIterator(unreal.LyudFountain):
        if f.get_world() == w:
            return f


def check(ok, what):
    if not ok:
        st["fail"] += 1
    unreal.log_warning("FOUNTAINTEST %s %s" % ("ok" if ok else "FAIL", what))


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
            unreal.log_warning("FOUNTAINTEST done: %d failed" % st["fail"])
            cmd(w, "quit")
            st["phase"] = 9
            return
        name, (cam, tgt), when, cmds, _, _ = SHOTS[st["i"]]
        first = ["t.MaxFPS 0", "au.LinearGainScalarForFinalOutut 0"] if st["i"] == 0 else []
        first += [c for c in os.environ.get("FOUNTAIN_CMDS", "").split(";") if c] if st["i"] == 0 else []
        for c in first + cmds:
            cmd(w, c)
        for cal in unreal.ObjectIterator(unreal.GameCalendar):
            if cal.get_world() == w:
                cal.set_now(when)
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
        st["levels"] = []
        st["phase"] = 1
    elif st["phase"] == 1 and st["t"] - st["t0"] <= WAIT + (25.0 if st["i"] == 0 else 0.0):
        if st["t"] - st["t0"] > 2.0:
            st["frames"].append(dt)
            f = fountain(w)
            if f and st["t"] - st["t0"] > 8.0:      # after a season jump: the frost search (a slice per tick) and the sound's 1.5 s fade
                st["levels"].append(f.get_sound_level())
    elif st["phase"] == 1:
        name, _, _, _, want_run, want_sound = SHOTS[st["i"]]
        f = sorted(st["frames"]) or [0.0]
        unreal.log_warning("FOUNTAINSHOT %s at %s: %d frames, median %.1f ms, worst %.1f ms" % (
            name, pawn.get_actor_location(), len(f), 1000 * f[len(f) // 2], 1000 * f[-1]))
        fo = fountain(w)
        if not fo:
            check(False, "%s: no fountain in the world" % name)
        else:
            lv = st["levels"]
            mean = sum(lv) / len(lv) if lv else 0.0
            unreal.log_warning("FOUNTAINSTATE %s: %s; sound level %.4f" % (name, fo.debug_report(), mean))
            check(fo.is_running() == want_run, "%s: running %s, expected %s" % (name, fo.is_running(), want_run))
            if want_sound == ">":
                check(mean > 0.002, "%s: the sound plays (level %.4f)" % (name, mean))
            elif want_sound == "0":
                check(mean < 1e-4, "%s: the sound is silent (level %.4f)" % (name, mean))
        st["frames"] = []
        cmd(w, "HighResShot " + RES)
        st["phase"] = 2
        st["t0"] = st["t"]
    elif st["phase"] == 2 and st["t"] - st["t0"] > 1.5:
        st["phase"] = 0


unreal.register_slate_post_tick_callback(tick)
