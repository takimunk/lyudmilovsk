"""Offscreen shots of the town's terraces (docs/city_plan.md): a camera at a map point looking at another, at a given
calendar time, 1920x1080. No PIE, no input, no window. Screenshots land in Saved/Screenshots/Mac in the order of SHOTS.

  tools/play_offscreen.sh /Game/Scene01/Lvl_Scene01 unreal/scripts/shoot_city.py   (RES=1920x1080 GREP=CITYSHOT)
  CITY_SHOTS=pier,high runs just those; CITY_FACE_Y overrides the portal face (Unreal Y cm, default 790)
"""
import datetime
import math
import os

import unreal

FACE_Y = float(os.environ.get("CITY_FACE_Y", "790"))
DAY = datetime.datetime(2026, 10, 20, 14, 0, 0)
NIGHT = datetime.datetime(2026, 10, 20, 23, 0, 0)
# (name, eye map x y z, target map x y z, when)
SHOTS = [
    ("a_pier", (300, 905, -37), (-400, 905, -20), DAY),
    ("b_w3", (-200, 1000, -27.3), (-310, 1010, -24), DAY),
    ("c_high", (450, 600, 40), (-300, 1000, -30), DAY),
    ("e_cascade", (-100, 700, -34), (-330, 800, -25), DAY),
    ("d_pier_night", (300, 905, -37), (-400, 905, -20), NIGHT),
    ("d_high_night", (450, 600, 40), (-300, 1000, -30), NIGHT),
]
if os.environ.get("CITY_SHOTS"):
    SHOTS = [s for s in SHOTS if s[0] in os.environ["CITY_SHOTS"].split(",")]
WAIT = float(os.environ.get("CITY_WAIT", "8"))
st = {"t": 0.0, "i": -1, "phase": 0, "frames": [], "cam": None}


def ue(p):
    return unreal.Vector(p[0] * 100.0, FACE_Y + p[1] * 100.0, p[2] * 100.0)


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name().startswith("Lvl_") and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def tick(dt):
    try:
        step(dt)
    except Exception as e:  # never hang offscreen on a script error
        unreal.log_error("CITYSHOT script error: %r" % e)
        w = world()
        if w:
            cmd(w, "quit")
        unreal.unregister_slate_post_tick_callback(handle)


def step(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < 6.0:
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    if st["phase"] == 0:
        st["i"] += 1
        if st["i"] >= len(SHOTS):
            cmd(w, "quit")
            st["phase"] = 9
            return
        name, eye, target, when = SHOTS[st["i"]]
        cal = unreal.SubsystemBlueprintLibrary.get_world_subsystem(w, unreal.GameCalendar)
        cal.set_time_scale(0.0)
        cal.set_now(unreal.DateTime(when.year, when.month, when.day, when.hour, when.minute, 0, 0))
        cmd(w, "lyud.SkyHourOffset 0")
        cmd(w, "t.MaxFPS 0")
        e, t = ue(eye), ue(target)
        d = t - e
        yaw = math.degrees(math.atan2(d.y, d.x))
        pitch = math.degrees(math.atan2(d.z, math.hypot(d.x, d.y)))
        if pawn:
            if hasattr(pawn, "character_movement"):
                pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
            pawn.set_actor_location(e, False, True)
            pawn.set_actor_hidden_in_game(True)
        if not st["cam"]:
            st["cam"] = unreal.GameplayStatics.begin_deferred_actor_spawn_from_class(
                w, unreal.CameraActor, unreal.Transform(e, unreal.Rotator(0, 0, 0)))
            unreal.GameplayStatics.finish_spawning_actor(st["cam"], unreal.Transform(e, unreal.Rotator(0, 0, 0)))
            st["cam"].camera_component.set_editor_property("field_of_view", 75.0)
            st["cam"].camera_component.set_editor_property("constrain_aspect_ratio", False)
        st["cam"].set_actor_location_and_rotation(e, unreal.Rotator(roll=0, pitch=pitch, yaw=yaw), False, True)
        pc.set_view_target_with_blend(st["cam"], 0.0)
        pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))
        st["t0"] = st["t"]
        st["phase"] = 1
    elif st["phase"] == 1 and st["t"] - st["t0"] <= WAIT:
        if st["t"] - st["t0"] > 2.0:
            st["frames"].append(dt)
    elif st["phase"] == 1:
        f = sorted(st["frames"]) or [0.0]
        win = unreal.SubsystemBlueprintLibrary.get_world_subsystem(w, unreal.LyudCityWindowsSubsystem)
        name = SHOTS[st["i"]][0]
        lit = []
        if win:
            for wid in ("W3-D017-w1", "W1-U01-w1", "W2-D001-w1"):
                lit.append("%s=%s" % (wid, win.is_window_lit(wid)))
        unreal.log_warning("CITYSHOT %s: %d frames, median %.1f ms, worst %.1f ms, windows %s" % (
            name, len(f), 1000 * f[len(f) // 2], 1000 * f[-1], " ".join(lit) or "no subsystem"))
        st["frames"] = []
        cmd(w, "HighResShot 1920x1080")
        st["phase"] = 2
        st["t0"] = st["t"]
    elif st["phase"] == 2 and st["t"] - st["t0"] > 1.5:
        st["phase"] = 0


handle = unreal.register_slate_post_tick_callback(tick)
