"""Offscreen screenshots of the red dwarf as the light source and its activity (docs/sun.md): sunsets from the terrace
and the beach (the disk, its aureole, the path on the sea, the light and the shadows), the noon disk close up, the
limb's prominences, a flare, Europa's night for comparison, and a time-lapse of the limb. No PIE, no input, the
desktop is left alone:

  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen -ResX=1600 -ResY=900 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py <this file>"

Prints SUN lines (sky state per shot); screenshots land in Saved/Screenshots/Mac in the order of SHOTS
(SUN_SHOTS=a,b runs just those). SUN_TIMELAPSE=<count>,<seconds> then stays on the last shot's view and takes
<count> more shots <seconds> apart while the game clock runs (the star's activity clock x lyud.Sun.TimeScale).
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
D = datetime.datetime
BROW_Y = 40.0
BROW = (W.cliff_top(BROW_Y) - 4.0, BROW_Y)
BEACH = (W.shore_x(BROW_Y) - 12.0, BROW_Y)
TERRACE = (W.DOOR_X - 2.0, 8.0)
NOON = D(2026, 10, 20, 12, 30)
SET1, SET2 = D(2026, 10, 20, 16, 45), D(2026, 10, 20, 17, 8)
NIGHT = D(2026, 10, 26, 0, 30)
CLEAR = "lyud.Weather.Cloud 0.15"
OLD = "lyud.Sky.LowSunLight 0;lyud.Sky.SunHorizonDimming 1;lyud.Sky.SunShafts 0;lyud.Sky.SunAureole 0"
LIMB = "fov 16;r.VolumetricCloud 0;lyud.Weather.Cloud 0"
WHOLE = LIMB.replace("fov 16", "fov 22")                 # the whole disk in the frame (flares anywhere on it)
# (name, local time, map x, y, metres over the ground, pitch, yaw or "sun"/"sun-wide"/"sun-path"/"moon"[, console commands])
SHOTS = [
    ("sunset_terrace", SET1, TERRACE[0], TERRACE[1] + 25.0, 1.7, 0, "sun-wide", CLEAR),
    ("sunset_terrace_low", SET2, TERRACE[0], TERRACE[1] + 25.0, 1.7, 0, "sun-wide", CLEAR),
    ("sunset_terrace_old", SET1, TERRACE[0], TERRACE[1] + 25.0, 1.7, 0, "sun-wide", CLEAR + ";" + OLD),
    ("sunset_terrace_back", SET1, TERRACE[0], TERRACE[1] + 25.0, 1.7, -8, "sun-back", CLEAR),
    ("sunset_beach", SET1, BEACH[0], BEACH[1], 1.7, 0, "sun-path", CLEAR),
    ("sunset_beach_low", SET2, BEACH[0], BEACH[1], 1.7, 0, "sun-path", CLEAR),
    ("sunset_beach_old", SET1, BEACH[0], BEACH[1], 1.7, 0, "sun-path", CLEAR + ";" + OLD),
    ("sunset_brow", SET2, BROW[0], BROW[1], 1.7, 0, "sun-path", CLEAR),
    ("sunset_close", SET1, BROW[0], BROW[1], 1.7, 0, "sun", "fov 30;" + CLEAR),
    ("sunset_close_low", SET2, BROW[0], BROW[1], 1.7, 0, "sun", "fov 30;" + CLEAR),
    ("sunset_clouds", SET1, BROW[0], BROW[1], 1.7, 0, "sun-wide", "lyud.Weather.Cloud 0.6"),
    ("noon_wide", NOON, BROW[0], BROW[1], 1.7, 0, "sun-wide", CLEAR),
    ("noon_close", NOON, BROW[0], BROW[1], 1.7, 0, "sun", "fov 30;" + CLEAR),
    ("noon_limb", NOON, BROW[0], BROW[1], 1.7, 0, "sun", LIMB),
    ("noon_detail", NOON, BROW[0], BROW[1], 1.7, 0, "sun", "fov 5;r.VolumetricCloud 0;lyud.Weather.Cloud 0"),
    ("flare_limb", NOON, BROW[0], BROW[1], 1.7, 0, "sun", WHOLE + ";lyud.Sun.ForceFlare 0;lyud.Sun.ForceFlare 0.9"),
    ("flare_late", NOON, BROW[0], BROW[1], 1.7, 0, "sun", WHOLE + ";lyud.Sun.ForceFlare 0;lyud.Sun.ForceFlare 0.88;wait 30"),
    ("flare_arcade", NOON, BROW[0], BROW[1], 1.7, 0, "sun", WHOLE + ";lyud.Sun.ForceFlareLimb 1;lyud.Sun.ForceFlare 0;lyud.Sun.ForceFlare 0.86;wait 40"),
    ("busy_limb", NOON, BROW[0], BROW[1], 1.7, 0, "sun", LIMB + ";lyud.Sun.Activity 2.5"),
    ("europa_night", NIGHT, BROW[0], BROW[1], 1.7, 0, "moon-wide", CLEAR),
    ("sunset_terrace_town", SET1, TERRACE[0], TERRACE[1] + 25.0, 1.7, -4, "sun-side", CLEAR),
]
# SUN_DIAG=1: what veils the low disk, and the noon disk's brightness against its detail
if os.environ.get("SUN_DIAG"):
    SC = "fov 30;" + CLEAR
    SHOTS = [("diag_set_" + t, SET1, BROW[0], BROW[1], 1.7, 0, "sun", SC + ";" + c) for t, c in (
        ("plain", ""), ("nofog", "r.Fog 0"), ("noclouds", "r.VolumetricCloud 0"), ("noaureole", "lyud.Sky.SunAureole 0"),
        ("nolow", "lyud.Sky.LowSunLight 0"), ("nobloom", "r.BloomQuality 0"), ("noshafts", "lyud.Sky.SunShafts 0"),
        ("noatmo", "r.SkyAtmosphere 0"))]
    for k in ("0", "1", "2"):
        SHOTS.append(("diag_shafts%s_terrace" % k, SET1, TERRACE[0], TERRACE[1] + 25.0, 1.7, 0, "sun-wide", CLEAR + ";lyud.Sky.SunShafts " + k))
        SHOTS.append(("diag_shafts%s_close" % k, SET1, BROW[0], BROW[1], 1.7, 0, "sun", SC + ";lyud.Sky.SunShafts " + k))
if os.environ.get("SUN_SHOTS"):
    SHOTS = [s for s in SHOTS if s[0] in os.environ["SUN_SHOTS"].split(",")]
# SUN_VARY="tag=cmd;cmd|tag2=..." (tuning sweeps): each chosen shot again per variant, its commands added
if os.environ.get("SUN_VARY"):
    VARY = [v.split("=", 1) for v in os.environ["SUN_VARY"].split("|")]
    SHOTS = [(s[0] + "_" + t,) + s[1:7] + ((s[7] + ";" if len(s) > 7 else "") + c,) for s in SHOTS for t, c in VARY]
WAIT = float(os.environ.get("SUN_WAIT", "7"))
LAPSE = [float(v) for v in os.environ.get("SUN_TIMELAPSE", "0,60").split(",")]
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


def aim(sky, what):
    if what.startswith("sun"):
        alt, az = sky.sun_altitude(), sky.sun_azimuth()
    else:
        rep = sky.get_debug_report()
        part = rep.split("moon alt ")[1].split()
        alt, az = float(part[0]), float(part[2])
    a, z = math.radians(alt), math.radians(az)
    # compass to world: north = +Y, east = -X (ALyudSky, NorthYawDeg 0)
    x, y = -math.sin(z) * math.cos(a), math.cos(z) * math.cos(a)
    yaw = math.degrees(math.atan2(y, x))
    pitch = alt
    if what.endswith("wide"):
        pitch = max(alt - 14.0, -8.0)
    elif what.endswith("path"):                      # the sun near the top: the sea's glitter path below it
        pitch = alt - 16.0
    elif what.endswith("back"):                      # back to the sun: the lit land and the long shadows
        yaw += 180.0
        pitch = -8.0
    elif what.endswith("side"):                      # across the light: lit faces, shadows running sideways
        yaw += 75.0
        pitch = -4.0
    return yaw, pitch


def tick(dt):
    try:
        step(dt)
    except Exception as e:                            # never hang offscreen on a script error
        unreal.log_error("SUN script error: %r" % e)
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
    sky = actor(w, unreal.LyudSky)
    sea = actor(w, unreal.LyudSea)
    if st["base"] is None:
        cmd(w, "lyud.SkyHourOffset 0")
        b = sky.shown_time()
        M = unreal.MathLibrary
        st["base"] = D(M.get_year(b), M.get_month(b), M.get_day(b), M.get_hour(b), M.get_minute(b), M.get_second(b))
        st["moon_d"] = sky.get_editor_property("moon_size_deg")
        unreal.log_warning("SUN calendar now %s" % st["base"])
        return
    if st["phase"] == 0:
        st["i"] += 1
        if st["i"] >= len(SHOTS) and st.get("lapse", 0) < LAPSE[0]:
            st["i"] = len(SHOTS) - 1
            st["lapse"] = st.get("lapse", 0) + 1
            st["phase"] = 5
            st["t0"] = st["t"]
            return
        if st["i"] >= len(SHOTS):
            cmd(w, "quit")
            st["phase"] = 9
            return
        name, when, x, y, up, pitch, yaw = SHOTS[st["i"]][:7]
        for c in ["lyud.Sky.SunDiskGain 1", "lyud.Sky.MoonDiskGain 1", "lyud.Sky.NightLight 1", "lyud.Sky.SunSpecular 1",
                  "lyud.Sky.CloudDensity 1", "lyud.Sky.CloudOpaque 1", "lyud.Sky.CloudSamples 1", "r.VolumetricRenderTarget.Mode 0", "lyud.Seabed 1", "lyud.Sea 1", "lyud.Sky.NightMie -1", "lyud.Sea.Capillary 0.18", "lyud.Sea.DebugFoam 0", "lyud.Weather.Cloud -1", "lyud.Weather.Rain -1", "lyud.Wind.Speed -1", "lyud.Sea.LookSeconds 120", "fov 0", "r.BloomQuality 5",
                  "r.VolumetricCloud 1", "r.SkyAtmosphere 1", "r.Lumen.Reflections.Allow 1", "r.SkyLight.RealTimeReflectionCapture 1", "sunvis 1", "lyud.Weather.BlendSeconds 0", "sunangle %g" % sky.get_editor_property("sun_angular_diameter_deg"), "moonsize %g" % st["moon_d"],
                  "lyud.Sky.LowSunLight 1", "lyud.Sky.SunHorizonDimming -1", "lyud.Sky.SunShafts 1", "lyud.Sky.SunAureole 1",
                  "lyud.Sun.Activity 1", "lyud.Sun.Debug 0", "lyud.Sky.SunDetailContrast -1", "r.Fog 1", "lyud.Sky.LowSunEV 0.7", "lyud.Sky.LowSunDiskGain -1",
                  "lyud.Sky.LowSunCloud -1", "lyud.Sky.SunRimClear -1", "lyud.Sky.SunLimb -1", "lyud.Sky.LowSunMieG -1", "lyud.Sun.ForceFlareLimb 0"] + (SHOTS[st["i"]][7:] or [""])[0].split(";"):
            if c.startswith("sunangle "):             # test hook: the sun light's source angle
                sky.get_editor_property("sun_light").set_light_source_angle(float(c.split()[1]))
            elif c.startswith("moonsize "):           # test hook: Europa's apparent size
                cmd(w, "lyud.Sky.MoonSizeDeg %s" % c.split()[1])
                sky.update_sky()
            elif c.startswith("moonvis "):
                sky.get_editor_property("moon_light").set_visibility(c.split()[1] == "1")
            elif c.startswith("wait "):               # this shot waits longer (a flare running on)
                st["extra"] = float(c.split()[1])
            elif c.startswith("sunvis "):
                sky.get_editor_property("sun_light").set_visibility(c.split()[1] == "1")
            elif c:
                cmd(w, c)
        hours = (when - st["base"]).total_seconds() / 3600.0
        cmd(w, "lyud.SkyHourOffset %f" % hours)
        cmd(w, "t.MaxFPS 0")
        if hasattr(pawn, "character_movement"):
            pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
        ux, uy, uz = T.to_unreal(x, y, T.ground_z(x, y) + up, FACE_Y)
        pawn.set_actor_location(unreal.Vector(ux, uy, uz), False, True)
        if not isinstance(yaw, str):
            pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))
        st["t0"] = st["t"]
        st["phase"] = 1
    elif st["phase"] == 1 and st["t"] - st["t0"] > 1.5:
        yaw = SHOTS[st["i"]][6]
        if isinstance(yaw, str):
            y, p = aim(sky, yaw)
            pc.set_control_rotation(unreal.Rotator(roll=0, pitch=p, yaw=y))
        st["phase"] = 2
    elif st["phase"] == 2 and st["t"] - st["t0"] > WAIT + st.get("extra", 0.0):
        st["extra"] = 0.0
        name = st.pop("lapse_name", SHOTS[st["i"]][0])
        sun = sky.get_editor_property("sun_light")
        unreal.log_warning("SUN %s | %s | %s | sun disk scale %s angle %.1f" % (
            name, sky.get_debug_report(), "activity",
            sun.get_editor_property("atmosphere_sun_disk_color_scale"), sun.get_editor_property("light_source_angle")))
        cmd(w, "HighResShot 1600x900")
        st["phase"] = 3
        st["t0"] = st["t"]
    elif st["phase"] == 3 and st["t"] - st["t0"] > 1.5:
        st["phase"] = 0
    elif st["phase"] == 5 and st["t"] - st["t0"] > LAPSE[1] - WAIT - 3.0:
        # a time-lapse frame: the same view, the clock running; re-aim at the drifting sun
        y, p = aim(sky, "sun")
        pc.set_control_rotation(unreal.Rotator(roll=0, pitch=p, yaw=y))
        st["t0"] = st["t"] - 1.6
        st["phase"] = 2
        st["lapse_name"] = "lapse%02d" % st["lapse"]


handle = unreal.register_slate_post_tick_callback(tick)
