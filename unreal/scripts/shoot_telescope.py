"""Offscreen screenshots through a telescope (docs/telescopes.md): Jupiter with its moons, the moon with its phase,
Saturn's ring, the sea by day; and the same place on foot. No PIE, no input, the desktop is left alone:

  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen -ResX=1600 -ResY=900 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py <this file>"

(unreal/scripts/shoot_telescope.sh does that through the UE queue.) Prints SCOPESHOT lines (the telescope's state per
shot); the screenshots ("shot showui", so the eyepiece's Slate mask is in them) land in Saved/Screenshots in the order of
SHOTS. SCOPE_SHOTS=a,b runs just those.
"""
import datetime
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
import terrain as T  # noqa: E402

W = T.W
FACE_Y = float(os.environ.get("TERRAIN_FACE_Y", "790"))
BROW_Y = 40.0
BROW = (W.cliff_top(BROW_Y) - 4.0, BROW_Y)
CLEAR = "lyud.Weather.Cloud 0;lyud.Weather.Preset clear"
# (name, local hour (next such hour after the game's start; "moon" = an hour of the night with the moon well up),
#  aim (body or "alt az"), eyepiece, focus knob, extra console commands)
SHOTS = [
    ("foot_night", 4.0, None, 0, 0.85, ""),
    ("jupiter_p0", 4.0, "jupiter", 0, 0.85, ""),
    ("jupiter_p1", 4.0, "jupiter", 1, 0.85, ""),
    ("jupiter_p2", 4.0, "jupiter", 2, 0.85, ""),
    ("jupiter_p2_blur", 4.0, "jupiter", 2, 0.55, ""),
    ("moon_p1", "moon", "moon", 1, 0.85, ""),
    ("moon_p2", "moon", "moon", 2, 0.85, ""),
    ("saturn_p2", 21.0, "saturn", 2, 0.85, ""),
    ("saturn_p3", 21.0, "saturn", 3, 0.85, ""),
    ("foot_day", 13.0, None, 0, 0.85, ""),
    ("sea_day_p0", 13.0, "0.4 sea", 0, 0.85, ""),
    ("sea_day_p1", 13.0, "0.4 sea", 1, 0.85, ""),
    ("sea_day_p1_near", 13.0, "-2 sea", 1, 0.5, ""),
]
# SCOPE_BENCH=<seconds>: no shots; the frame time at the brow at 04:00 on foot (facing Jupiter) and looking at x3 and x60,
# in rounds A B C A B C ... (one process: the Mac's load drifts more between processes than the difference)
BENCH = float(os.environ.get("SCOPE_BENCH", "0"))
if BENCH:
    ROUNDS = int(os.environ.get("SCOPE_BENCH_ROUNDS", "3"))
    SHOTS = [s for _ in range(ROUNDS) for s in (
        ("bench_foot", 4.0, None, 0, 0.85, ""), ("bench_p0", 4.0, "jupiter", 0, 0.85, ""), ("bench_p2", 4.0, "jupiter", 2, 0.85, ""))]
    WAIT_BENCH = BENCH
if os.environ.get("SCOPE_DIAG"):
    SHOTS = [("diag_foot90", 4.0, "foot 26 102 90", 0, 0.85, ""), ("diag_foot28", 4.0, "foot 26 102 28", 0, 0.85, ""),
             ("diag_foot7", 4.0, "foot 26 102 7", 0, 0.85, ""), ("diag_p0", 4.0, "jupiter", 0, 0.85, ""),
             ("diag_p1", 4.0, "jupiter", 1, 0.85, ""), ("diag_p2", 4.0, "jupiter", 2, 0.85, ""),
             ("diag_p0_late", 4.0, "jupiter", 0, 0.85, ";lyud.Scope.Power 2;lyud.Scope.Power 0")]
if os.environ.get("SCOPE_SHOTS"):
    SHOTS = [s for s in SHOTS if s[0] in os.environ["SCOPE_SHOTS"].split(",")]
WAIT = float(os.environ.get("SCOPE_WAIT", "6"))
st = {"t": 0.0, "i": -1, "phase": 0, "base": None}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name().startswith("Lvl_") and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def actor(w, cls):
    a = unreal.GameplayStatics.get_all_actors_of_class(w, cls)
    return a[0] if a else None


def moon_alt(sky):
    try:
        return float(sky.get_debug_report().split("moon alt ")[1].split()[0])
    except Exception:
        return -90.0


def offset_for(hour):
    """Hours from the game's start to the next local `hour` (at least 0.5 h on)."""
    b = st["base"]
    h = b.hour + b.minute / 60.0
    d = (hour - h) % 24.0
    return d if d > 0.5 else d + 24.0


def tick(dt):
    try:
        step(dt)
    except Exception as e:                            # never hang offscreen on a script error
        unreal.log_error("SCOPESHOT script error: %r" % e)
        w = world()
        if w:
            cmd(w, "quit")
        unreal.unregister_slate_post_tick_callback(handle)


def step(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < 8.0:
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    sky = actor(w, unreal.LyudSky)
    if st["base"] is None:
        cmd(w, "lyud.SkyHourOffset 0")
        b = sky.shown_time()
        M = unreal.MathLibrary
        st["base"] = datetime.datetime(M.get_year(b), M.get_month(b), M.get_day(b), M.get_hour(b), M.get_minute(b))
        unreal.log_warning("SCOPESHOT calendar now %s" % st["base"])
        for v in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PostProcessVolume):
            ps = v.settings
            unreal.log_warning("SCOPESHOT ppv %s unbound=%s prio=%s method=%s(%s) bias=%s(%s) min=%s max=%s(%s) speed=%s/%s" % (
                v.get_name(), v.unbound, v.priority, ps.auto_exposure_method, ps.override_auto_exposure_method,
                ps.auto_exposure_bias, ps.override_auto_exposure_bias, ps.auto_exposure_min_brightness,
                ps.auto_exposure_max_brightness, ps.override_auto_exposure_max_brightness,
                ps.auto_exposure_speed_up, ps.auto_exposure_speed_down))
        cmd(w, "lyud.Scope.Debug 1")
        # the moon's hour: scan the coming night (18:00 .. 07:00) a frame per hour for the highest moon
        st["scan"] = [offset_for(18.0) + k for k in range(14)]
        st["best"] = (None, -90.0)
        st["phase"] = 7
        return
    if st["phase"] == 7:
        if st.get("scanning") is not None:
            alt = moon_alt(sky)
            if alt > st["best"][1]:
                st["best"] = (st["scanning"], alt)
        if st["scan"]:
            st["scanning"] = st["scan"].pop(0)
            cmd(w, "lyud.SkyHourOffset %f" % st["scanning"])
            sky.update_sky()
            return
        unreal.log_warning("SCOPESHOT moon highest at +%s h, alt %.1f" % st["best"])
        st["phase"] = 0
        return
    if st["phase"] == 0:
        st["i"] += 1
        if st["i"] >= len(SHOTS):
            cmd(w, "lyud.Scope.End")
            cmd(w, "quit")
            st["phase"] = 9
            return
        name, hour, aim, power, knob, extra = SHOTS[st["i"]]
        cmd(w, "lyud.Scope.End")
        hours = st["best"][0] if hour == "moon" else offset_for(hour)
        if hours is None:
            hours = offset_for(2.0)
        cmd(w, "lyud.SkyHourOffset %f" % hours)
        for c in (CLEAR + ";t.MaxFPS 0;" + extra).split(";"):
            if c:
                cmd(w, c)
        if hasattr(pawn, "character_movement"):
            pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
        if "start" not in st:
            st["start"] = pawn.get_actor_location()
        p0 = st["start"]
        ux, uy, uz = p0.x, p0.y, p0.z + 100.0 * float(os.environ.get("SCOPE_UP", "25"))
        pawn.set_actor_location(unreal.Vector(ux, uy, uz), False, True)
        unreal.log_warning("SCOPESHOT pawn at %s" % pawn.get_actor_location())
        # face the sea on foot (toward the shore below the brow)
        st["sea_yaw"] = float(os.environ.get("SCOPE_SEA_AZ", "270")) + 90.0
        pc.set_control_rotation(unreal.Rotator(roll=0, pitch=0, yaw=st["sea_yaw"]))
        fa = SHOTS[st["i"]][2]
        if fa and fa.startswith("foot "):              # on foot looking at alt az with a fov: "foot <alt> <az> <fov>"
            _, a, z, f = fa.split()
            pc.set_control_rotation(unreal.Rotator(roll=0, pitch=float(a), yaw=float(z) + 90.0))
            cmd(w, "fov " + f)
        st["t0"] = st["t"]
        st["phase"] = 1
    elif st["phase"] == 1 and st["t"] - st["t0"] > 1.5:
        name, hour, aim, power, knob, extra = SHOTS[st["i"]]
        if aim and aim.startswith("foot "):
            pass
        elif aim:
            cmd(w, "fov 0")
            cmd(w, "lyud.Scope.Look")
            # world yaw to compass azimuth: north = +Y, east = -X, so yaw = az + 90
            cmd(w, "lyud.Scope.Aim %s" % aim.replace("sea", "%.1f" % ((st["sea_yaw"] - 90.0) % 360.0)))
            cmd(w, "lyud.Scope.Power %d" % power)
            cmd(w, "lyud.Scope.Focus %g" % knob)
        elif BENCH:
            cmd(w, "lyud.Scope.Look")
            cmd(w, "lyud.Scope.Aim jupiter")
            st["foot_pending"] = True
        st["phase"] = 2
        st["frames"] = []
    elif st["phase"] == 2 and BENCH:
        if st.get("foot_pending") and st["t"] - st["t0"] > 3.5:
            st["foot_pending"] = False
            # on foot, looking the way the tube looks: take the camera's rotation, step back from the telescope
            rot = pc.player_camera_manager.get_camera_rotation()
            cmd(w, "lyud.Scope.End")
            pc.set_control_rotation(rot)
        if st["t"] - st["t0"] > 4.0 + 1.5:
            st["frames"].append(dt * 1000.0)
        if st["t"] - st["t0"] > 4.0 + 1.5 + BENCH:
            f = sorted(st["frames"])
            n = len(f)
            unreal.log_warning("SCOPEBENCH %s frames %d mean %.2f ms median %.2f p90 %.2f" % (
                SHOTS[st["i"]][0], n, sum(f) / max(n, 1), f[n // 2] if n else 0.0, f[int(n * 0.9)] if n else 0.0))
            st["phase"] = 0
    elif st["phase"] == 2 and st["t"] - st["t0"] > WAIT:
        name, hour, aim, power, knob, extra = SHOTS[st["i"]]
        if aim and not aim.startswith("foot") and not ("0" <= aim[0] <= "9" or aim[0] == "-"):
            cmd(w, "lyud.Scope.Aim %s" % aim)          # logs the state (and false when it is under the horizon)
        unreal.log_warning("SCOPESHOT %s | %s | moon %.1f" % (name, sky.get_debug_report(), moon_alt(sky)))
        cmd(w, "shot showui")
        st["phase"] = 3
        st["t0"] = st["t"]
    elif st["phase"] == 3 and st["t"] - st["t0"] > 1.5:
        st["phase"] = 0


handle = unreal.register_slate_post_tick_callback(tick)
