"""Offscreen screenshots of the red-dwarf sun, Europa, moonlit and moonless nights and the tide (docs/tides.md):
stands the player, flying, at a few spots of Lvl_Scene01 at given local dates and hours (lyud.SkyHourOffset from the
calendar's now) and tide levels (lyud.TideOverride), aims at the sun or the moon where asked, and takes a 1600x900
shot at each. No PIE, no input, the desktop is left alone:

  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen -ResX=1600 -ResY=900 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py <this file>"

Prints COSMIC lines (sky and sea state per shot); screenshots land in Saved/Screenshots/Mac in the order of SHOTS
(COSMIC_SHOTS=a,b runs just those).
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
NOON, DAY = D(2026, 10, 20, 12, 30), D(2026, 10, 21, 13, 0)
# (name, local time, tide m or None, map x, y, metres over the ground, pitch, yaw or "sun"/"moon"[, console commands])
SHOTS = [
    ("sun_noon", NOON, None, BROW[0], BROW[1], 1.7, 0, "sun-wide"),
    ("sun_noon_close", NOON, None, BROW[0], BROW[1], 1.7, 0, "sun", "fov 30"),
    ("sun_noon_fullscreen_noclouds", NOON, None, BROW[0], BROW[1], 1.7, 0, "sun", "fov 11;r.VolumetricCloud 0"),
    ("sun_afternoon", D(2026, 10, 20, 15, 30), None, BROW[0], BROW[1], 1.7, 0, "sun-wide"),
    ("sun_sunset", D(2026, 10, 20, 17, 2), None, BROW[0], BROW[1], 1.7, 0, "sun-wide"),
    ("sun_sunset_close", D(2026, 10, 20, 17, 2), None, BROW[0], BROW[1], 1.7, 0, "sun", "fov 30"),
    ("sun_dusk", D(2026, 10, 20, 17, 35), None, BROW[0], BROW[1], 1.7, 2, 0),
    ("sun_from_terrace", NOON, None, TERRACE[0], TERRACE[1] + 25.0, 1.7, 0, "sun-wide"),
    ("europa_day", D(2026, 10, 30, 9, 30), None, BROW[0], BROW[1], 1.7, 0, "moon-wide"),
    ("europa_day_close", D(2026, 10, 30, 9, 30), None, BROW[0], BROW[1], 1.7, 0, "moon", "fov 25"),
    ("europa_night_gibbous", D(2026, 10, 22, 22, 0), None, BROW[0], BROW[1], 1.7, 0, "moon", "fov 25"),
    ("europa_night_full", D(2026, 10, 26, 0, 30), None, BROW[0], BROW[1], 1.7, 0, "moon-wide"),
    ("europa_night_full_close", D(2026, 10, 26, 0, 30), None, BROW[0], BROW[1], 1.7, 0, "moon", "fov 25"),
    ("europa_crescent_dawn", D(2026, 11, 6, 6, 15), None, BROW[0], BROW[1], 1.7, 0, "moon", "fov 25"),
    ("night_full_town", D(2026, 10, 26, 0, 30), None, TERRACE[0], TERRACE[1] + 25.0, 1.7, -6, 90),
    ("night_full_beach", D(2026, 10, 26, 0, 30), None, BROW[0], BROW[1], 1.7, -14, 20),
    ("night_new_town", D(2026, 11, 10, 0, 30), None, TERRACE[0], TERRACE[1] + 25.0, 1.7, -6, 90),
    ("night_new_beach", D(2026, 11, 10, 0, 30), None, BROW[0], BROW[1], 1.7, -14, 20),
    ("tide_high_brow", DAY, 0.0, BROW[0], BROW[1], 1.7, -16, 15),
    ("tide_mid_brow", DAY, -2.0, BROW[0], BROW[1], 1.7, -16, 15),
    ("tide_low_brow", DAY, -4.5, BROW[0], BROW[1], 1.7, -16, 15),
    ("tide_low_high_view", DAY, -4.5, BROW[0], BROW[1] - 60.0, 45.0, -22, 25),
    ("tide_high_beach", DAY, 0.0, BEACH[0], BEACH[1], 1.7, -3, 10),
    ("tide_mid_beach", DAY, -2.0, BEACH[0], BEACH[1], 1.7, -3, 10),
    ("tide_low_beach", DAY, -4.5, BEACH[0], BEACH[1], 1.7, -3, 10),
    ("tide_low_pools", DAY, -4.5, BEACH[0] + 60.0, BEACH[1], 1.7, -12, 30),
]
# tuning: COSMIC_GAINS=0.5,1,2 adds sun_noon / europa shots at those disk gains
for g in [v for v in os.environ.get("COSMIC_GAINS", "").split(",") if v]:
    SHOTS.append(("sun_noon_gain%s" % g, NOON, None, BROW[0], BROW[1], 1.7, 0, "sun", "fov 30;lyud.Sky.SunDiskGain " + g))
    SHOTS.append(("europa_night_gain%s" % g, D(2026, 10, 26, 0, 30), None, BROW[0], BROW[1], 1.7, 0, "moon",
                  "fov 25;lyud.Sky.MoonDiskGain " + g))
# COSMIC_DIAG=1: what makes the glow round the disks (bloom, clouds) and whether the white sunset disk is ours
if os.environ.get("COSMIC_DIAG"):
    FULL = D(2026, 10, 26, 0, 30)
    for tag, c in (("plain", ""), ("nobloom", "r.BloomQuality 0"), ("noclouds", "r.VolumetricCloud 0"),
                   ("neither", "r.BloomQuality 0;r.VolumetricCloud 0"), ("nodisk", "lyud.Sky.SunDiskGain 0")):
        SHOTS.append(("diag_noon_" + tag, NOON, None, BROW[0], BROW[1], 1.7, 0, "sun", "fov 30;" + c))
        SHOTS.append(("diag_sunset_" + tag, D(2026, 10, 20, 17, 2), None, BROW[0], BROW[1], 1.7, 0, "sun", "fov 30;" + c))
        SHOTS.append(("diag_moon_" + tag, FULL, None, BROW[0], BROW[1], 1.7, 0, "moon", "fov 25;" + c.replace("SunDisk", "MoonDisk")))
    for k in ("1", "3", "8"):
        SHOTS.append(("diag_newmoon_glow" + k, D(2026, 11, 10, 0, 30), None, TERRACE[0], TERRACE[1] + 25.0, 1.7, -6, 90,
                      "lyud.Sky.NightLight " + k))
    SHOTS.append(("diag_low_pools", DAY, -4.5, BEACH[0] + 60.0, BEACH[1], 1.7, -12, 30))
    SHOTS.append(("diag_low_brow", DAY, -4.5, BROW[0], BROW[1], 1.7, -16, 15))
    SHOTS.append(("diag_high_beach", DAY, 0.0, BEACH[0], BEACH[1], 1.7, -3, 10))
    S2 = D(2026, 10, 20, 17, 2)
    for tag, c in (("angle1", "sunangle 1"), ("noatmo", "r.SkyAtmosphere 0"), ("nolight", "sunvis 0"), ("nodisk2", "lyud.Sky.SunDiskGain 0")):
        SHOTS.append(("diag2_sunset_" + tag, S2, None, BROW[0], BROW[1], 1.7, 0, "sun", "fov 30;" + c))
    for g in ("0.5", "1", "2"):
        SHOTS.append(("diag3_noon_g" + g, NOON, None, BROW[0], BROW[1], 1.7, 0, "sun", "fov 30;lyud.Sky.SunDiskGain " + g))
        SHOTS.append(("diag3_sunset_g" + g, D(2026, 10, 20, 16, 50), None, BROW[0], BROW[1], 1.7, 0, "sun", "fov 30;lyud.Sky.SunDiskGain " + g))
    for g in ("0.5", "1", "2"):
        SHOTS.append(("diag3_moon_g" + g, D(2026, 10, 26, 0, 30), None, BROW[0], BROW[1], 1.7, 0, "moon", "fov 25;lyud.Sky.MoonDiskGain " + g))
        SHOTS.append(("diag3_daymoon_g" + g, D(2026, 10, 30, 9, 30), None, BROW[0], BROW[1], 1.7, 0, "moon", "fov 25;lyud.Sky.MoonDiskGain " + g))
    if os.environ["COSMIC_DIAG"] == "only3":
        SHOTS = [s for s in SHOTS if s[0].startswith("diag3_")]
    if os.environ["COSMIC_DIAG"] == "only2":
        SHOTS = [s for s in SHOTS if s[0].startswith("diag2_")]
    if os.environ["COSMIC_DIAG"] == "only":
        SHOTS = [s for s in SHOTS if s[0].startswith("diag_")]
# COSMIC_TUNE=1: sweeps for the polish pass (cloud density over the setting sun, the sun's specular and the capillary
# ripples on the glitter path, the night haze round Europa); COSMIC_TUNE=only runs just these
if os.environ.get("COSMIC_TUNE"):
    SS = D(2026, 10, 20, 17, 2)
    for cap in ("0.08", "0.2"):
        SHOTS.append(("tune_sunpath_cap" + cap, SS, None, BROW[0], BROW[1], 1.7, 0, "sun-wide", "lyud.Sea.Capillary " + cap))
        SHOTS.append(("tune_sunpath_close_cap" + cap, SS, None, BROW[0], BROW[1], 1.7, 0, "sun", "fov 30;lyud.Sea.Capillary " + cap))
    SHOTS.append(("tune_noon_sea_cap0.2", NOON, None, BROW[0], BROW[1], 1.7, -10, 20, "lyud.Sea.Capillary 0.2"))
    if os.environ["COSMIC_TUNE"] == "only":
        SHOTS = [s for s in SHOTS if s[0].startswith("tune_")]
if os.environ.get("COSMIC_SHOTS"):
    SHOTS = [s for s in SHOTS if s[0] in os.environ["COSMIC_SHOTS"].split(",")]
WAIT = float(os.environ.get("COSMIC_WAIT", "7"))
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
    pitch = alt if not what.endswith("wide") else max(alt - 14.0, -8.0)
    return yaw, pitch


def tick(dt):
    try:
        step(dt)
    except Exception as e:                            # never hang offscreen on a script error
        unreal.log_error("COSMIC script error: %r" % e)
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
        unreal.log_warning("COSMIC calendar now %s" % st["base"])
        return
    if st["phase"] == 0:
        st["i"] += 1
        if st["i"] >= len(SHOTS):
            cmd(w, "quit")
            st["phase"] = 9
            return
        name, when, tide, x, y, up, pitch, yaw = SHOTS[st["i"]][:8]
        for c in ["lyud.Sky.SunDiskGain 1", "lyud.Sky.MoonDiskGain 1", "lyud.Sky.NightLight 1", "lyud.Sky.SunSpecular 1",
                  "lyud.Sky.CloudDensity 1", "lyud.Sky.NightMie -1", "lyud.Sea.Capillary 0.18", "fov 0", "r.BloomQuality 5",
                  "r.VolumetricCloud 1", "r.SkyAtmosphere 1", "r.Lumen.Reflections.Allow 1", "r.SkyLight.RealTimeReflectionCapture 1", "sunvis 1", "sunangle %g" % sky.get_editor_property("sun_angular_diameter_deg")] + (SHOTS[st["i"]][8:] or [""])[0].split(";"):
            if c.startswith("sunangle "):             # test hook: the sun light's source angle
                sky.get_editor_property("sun_light").set_light_source_angle(float(c.split()[1]))
            elif c.startswith("sunvis "):
                sky.get_editor_property("sun_light").set_visibility(c.split()[1] == "1")
            elif c:
                cmd(w, c)
        hours = (when - st["base"]).total_seconds() / 3600.0
        cmd(w, "lyud.SkyHourOffset %f" % hours)
        cmd(w, "lyud.TideOverride %f" % (tide if tide is not None else -999.0))
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
        yaw = SHOTS[st["i"]][7]
        if isinstance(yaw, str):
            y, p = aim(sky, yaw)
            pc.set_control_rotation(unreal.Rotator(roll=0, pitch=p, yaw=y))
        st["phase"] = 2
    elif st["phase"] == 2 and st["t"] - st["t0"] > WAIT:
        name = SHOTS[st["i"]][0]
        sun = sky.get_editor_property("sun_light")
        unreal.log_warning("COSMIC %s | %s | %s | sun disk scale %s angle %.1f" % (
            name, sky.get_debug_report(), sea.get_debug_report() if sea else "no sea",
            sun.get_editor_property("atmosphere_sun_disk_color_scale"), sun.get_editor_property("light_source_angle")))
        cmd(w, "HighResShot 1600x900")
        st["phase"] = 3
        st["t0"] = st["t"]
    elif st["phase"] == 3 and st["t"] - st["t0"] > 1.5:
        st["phase"] = 0


handle = unreal.register_slate_post_tick_callback(tick)
