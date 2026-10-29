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
    # the relief near the terminator: half phases by night and by day, and a narrow lens on the night one
    ("europa_night_half", D(2026, 10, 19, 20, 0), None, BROW[0], BROW[1], 1.7, 0, "moon", "fov 25"),
    ("europa_night_half_detail", D(2026, 10, 19, 20, 0), None, BROW[0], BROW[1], 1.7, 0, "moon", "fov 9"),
    ("europa_day_half", D(2026, 11, 2, 10, 0), None, BROW[0], BROW[1], 1.7, 0, "moon", "fov 25"),
    ("sun_noon_detail", NOON, None, BROW[0], BROW[1], 1.7, 0, "sun", "fov 4;r.VolumetricCloud 0"),
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
    # high water over the flat by the railway bank (y -60): a few metres over the water south of it, toward the bank
    # and along it
    ("bank_high_water", DAY, 0.0, 330.0, -88.0, -40.0 - T.ground_z(330.0, -88.0), -14, 90),
    ("bank_high_water_along", DAY, 0.0, 300.0, -78.0, -41.2 - T.ground_z(300.0, -78.0), -7, 25),
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
# COSMIC_TUNE=1: sweeps for the polish pass (the clouds' ray-march samples and density over the setting sun, Europa's
# night brightness); COSMIC_TUNE=only runs just these
if os.environ.get("COSMIC_TUNE"):
    SS = D(2026, 10, 20, 17, 2)
    for tag, c in (("s1", "lyud.Sky.CloudSamples 1"), ("s2", "lyud.Sky.CloudSamples 2"), ("s4", "lyud.Sky.CloudSamples 4"),
                   ("s2_mode1", "lyud.Sky.CloudSamples 2;r.VolumetricRenderTarget.Mode 1"),
                   ("s2_d1.7", "lyud.Sky.CloudSamples 2;lyud.Sky.CloudDensity 1.7")):
        SHOTS.append(("tune_sunset_" + tag, SS, None, BROW[0], BROW[1], 1.7, 0, "sun", "fov 30;" + c))
    for g in ("1", "1.6"):
        SHOTS.append(("tune_europa_g" + g, D(2026, 10, 26, 0, 30), None, BROW[0], BROW[1], 1.7, 0, "moon",
                      "fov 25;lyud.Sky.MoonDiskGain " + g))
    if os.environ["COSMIC_TUNE"] == "only":
        SHOTS = [s for s in SHOTS if s[0].startswith("tune_")]
# COSMIC_BANK=1: what darkens the high water over the flat by the bank (seabed life, the bed itself dry, no sea)
if os.environ.get("COSMIC_BANK"):
    bz = -40.0 - T.ground_z(330.0, -88.0)
    for tag, tide, c in (("plain", 0.0, ""), ("noseabed", 0.0, "lyud.Seabed 0"), ("dry", -6.0, ""), ("nosea", 0.0, "lyud.Sea 0")):
        SHOTS.append(("bank_" + tag, DAY, tide, 330.0, -88.0, bz, -14, 90, c))
    SHOTS = [s for s in SHOTS if s[0].startswith("bank_") and s[0] != "bank_high_water_along"]
# COSMIC_HALO=1: the setting sun behind thick cloud now and with v3's clouds (half the density, the engine's cut-off)
if os.environ.get("COSMIC_HALO"):
    SS = D(2026, 10, 20, 17, 2)
    v3 = "lyud.Sky.CloudOpaque 0;lyud.Sky.CloudDensity 0.5"
    now = "lyud.Sky.CloudOpaque 1;lyud.Sky.CloudDensity 1"
    for tag, c in (("now", now), ("v3", v3), ("now2", now), ("v3b", v3), ("now3", now),
                   ("opaque0.5", "lyud.Sky.CloudOpaque 2;lyud.Sky.CloudDensity 1")):
        SHOTS.append(("halo_sunset_" + tag, SS, None, BROW[0], BROW[1], 1.7, 0, "sun", "fov 30;" + c))
# COSMIC_EDGE=1: the grain at the sunset clouds' edges against the opaque cut-off, the density and the samples
if os.environ.get("COSMIC_EDGE"):
    SS = D(2026, 10, 20, 17, 2)
    def edge(samples, cut, dens):
        return "lyud.Sky.CloudSamples %s;lyud.Sky.CloudOpaque %s;lyud.Sky.CloudDensity %s" % (samples, cut, dens)
    for tag, c in (("now", edge(1, 1, 1)), ("cut0", edge(1, 0, 1)), ("cut0.25", edge(1, 0.25, 1)), ("s2", edge(2, 1, 1)),
                   ("s2_cut0.25", edge(2, 0.25, 1)), ("s3_cut0", edge(3, 0, 1)), ("d0.5_cut0", edge(1, 0, 0.5))):
        SHOTS.append(("edge_sunset_" + tag, SS, None, BROW[0], BROW[1], 1.7, 0, "sun", "fov 30;" + c))
    SHOTS = [s for s in SHOTS if s[0].startswith("edge_")]
# COSMIC_PHASE=1: the same night views (the town path from the terrace, the beach from the cliff) at Europa's
# phases, for the moonlight, the night sky light and the exposure following the phase
PHASES = (("new", D(2026, 11, 10, 0, 30)), ("crescent", D(2026, 11, 5, 5, 30)), ("quarter", D(2026, 11, 2, 3, 30)),
          ("gibbous", D(2026, 10, 22, 22, 0)), ("full", D(2026, 10, 26, 0, 30)))
if os.environ.get("COSMIC_PHASE"):
    CC = os.environ.get("COSMIC_PHASE_CLOUD", "0.3")
    for tag, when in PHASES:
        # the same cloud cover every night (the calendar's weather differs from night to night, and clouds over the
        # moon now shade its light), the disk from 30 m up, clear of the cliff and the poles
        SHOTS.append(("phase_%s_town" % tag, when, None, TERRACE[0], TERRACE[1] + 25.0, 1.7, -6, 90, "lyud.Weather.Cloud " + CC))
        SHOTS.append(("phase_%s_beach" % tag, when, None, BROW[0], BROW[1], 1.7, -14, 20, "lyud.Weather.Cloud " + CC))
        SHOTS.append(("phase_%s_moon" % tag, when, None, BROW[0], BROW[1], 30.0, 0, "moon", "fov 25;lyud.Weather.Cloud " + CC))
    if os.environ["COSMIC_PHASE"] == "only":
        SHOTS = [s for s in SHOTS if s[0].startswith("phase_")]
# COSMIC_FOAMDIAG=1: what draws the white strokes on the sand under the high water (the foam glowing, no ripples, no sea)
if os.environ.get("COSMIC_FOAMDIAG"):
    for tag, c in (("glow", "lyud.Sea.DebugFoam 4"), ("nocap", "lyud.Sea.Capillary 0"), ("nosea", "lyud.Sea 0")):
        SHOTS.append(("foamdiag_" + tag, DAY, 0.0, BEACH[0], BEACH[1], 1.7, -3, 10, c))
    SHOTS = [s for s in SHOTS if s[0].startswith(("foamdiag_", "phase_"))]
# COSMIC_SCALE=1: how big Europa looks at the player's own field of view from the terrace (the town and trees for
# scale), by night and by day, and the same frames with the real Moon's size (0.52 deg, a test override only)
if os.environ.get("COSMIC_SCALE"):
    for tag, when in (("night", D(2026, 10, 25, 19, 30)), ("day", D(2026, 10, 30, 10, 30))):
        for size, c in (("", ""), ("_old", "moonsize 0.52")):
            SHOTS.append(("scale_%s%s" % (tag, size), when, None, TERRACE[0], TERRACE[1] + 25.0, 1.7, 0, "moon-wide",
                          "lyud.Weather.Cloud 0;" + c))
    if os.environ["COSMIC_SCALE"] == "only":
        SHOTS = [s for s in SHOTS if s[0].startswith("scale_")]
# COSMIC_SEALOOK=1: the sea's colour with the weather and the light from the cliff: sunny, overcast, storm, sunset,
# night (full moon), and a winter noon; the look eases in at once (lyud.Sea.LookSeconds 0)
# COSMIC_MOONCLOUD=1: Europa by day behind the clouds (the clouds must cover the whole disk, lit and unlit side) at
# the default size and at 20 deg; with diagnostics (no disk, no moonlight) when it is "diag"
if os.environ.get("COSMIC_MOONCLOUD"):
    MC = [("", ""), ("_20", "moonsize 20")]
    if os.environ["COSMIC_MOONCLOUD"] == "diag":
        MC += [("_nodisk", "lyud.Sky.MoonDiskGain 0"), ("_nolight", "lyud.Sky.NightLight 0")]
    for tag, c in MC:
        for t, when in (("day", D(2026, 10, 30, 10, 30)), ("dayhalf", D(2026, 11, 2, 10, 0))):
            SHOTS.append(("mooncloud_%s%s" % (t, tag), when, None, TERRACE[0], TERRACE[1] + 25.0, 1.7, 0, "moon", c))
    SHOTS = [s for s in SHOTS if s[0].startswith("mooncloud_")]
# COSMIC_OPAQUE=1:1,0.12:2 (cut-off multiplier:density multiplier pairs): the clouds' opaque cut-off
# (lyud.Sky.CloudOpaque) and density (lyud.Sky.CloudDensity) over Europa by day, the setting sun and a noon sky; the
# game is slowed to a crawl (slomo) so the same clouds are in every frame
if os.environ.get("COSMIC_OPAQUE"):
    OS = []
    for kd in os.environ["COSMIC_OPAQUE"].split(","):
        k, d = (kd.split(":") + ["1"])[:2]
        c0 = "slomo 0.0001;lyud.Sky.CloudOpaque %s;lyud.Sky.CloudDensity %s" % (k, d)
        tag = "o%s_d%s" % (k, d)
        OS += [(tag + "_moon", D(2026, 10, 30, 10, 30), None, TERRACE[0], TERRACE[1] + 25.0, 1.7, 0, "moon", c0 + ";moonsize " + os.environ.get("COSMIC_OPAQUE_MOON", "20")),
               (tag + "_sunset", D(2026, 10, 20, 17, 2), None, BROW[0], BROW[1], 1.7, 0, "sun", c0 + ";fov 30"),
               (tag + "_sky", NOON, None, BROW[0], BROW[1], 1.7, 25, 60, c0)]
    OS.sort(key=lambda s: s[0].split("_")[-1])
    SHOTS = OS
if os.environ.get("COSMIC_SEALOOK"):
    SL = "lyud.Sea.LookSeconds 0;"
    for tag, when, c in (("sunny", DAY, "lyud.Weather.Cloud 0;lyud.Wind.Speed 5"),
                         ("overcast", DAY, "lyud.Weather.Cloud 1;lyud.Wind.Speed 7"),
                         ("storm", DAY, "lyud.Weather.Cloud 1;lyud.Weather.Rain 3;lyud.Wind.Speed 18"),
                         ("sunset", D(2026, 10, 20, 17, 0), "lyud.Weather.Cloud 0.3;lyud.Wind.Speed 5"),
                         ("night", D(2026, 10, 26, 0, 30), "lyud.Weather.Cloud 0.2;lyud.Wind.Speed 5"),
                         ("winter", D(2027, 2, 10, 12, 30), "lyud.Weather.Cloud 0.3;lyud.Wind.Speed 7")):
        SHOTS.append(("sealook_" + tag, when, -1.0, BROW[0], BROW[1], 1.7, -10, 15, SL + c))
    if os.environ["COSMIC_SEALOOK"] == "only":
        SHOTS = [s for s in SHOTS if s[0].startswith("sealook_")]
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
        st["moon_d"] = sky.get_editor_property("moon_size_deg")
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
                  "lyud.Sky.CloudDensity 1", "lyud.Sky.CloudOpaque 1", "lyud.Sky.CloudSamples 1", "r.VolumetricRenderTarget.Mode 0", "lyud.Seabed 1", "lyud.Sea 1", "lyud.Sky.NightMie -1", "lyud.Sea.Capillary 0.18", "lyud.Sea.DebugFoam 0", "lyud.Weather.Cloud -1", "lyud.Weather.Rain -1", "lyud.Wind.Speed -1", "lyud.Sea.LookSeconds 120", "fov 0", "r.BloomQuality 5",
                  "r.VolumetricCloud 1", "r.SkyAtmosphere 1", "r.Lumen.Reflections.Allow 1", "r.SkyLight.RealTimeReflectionCapture 1", "sunvis 1", "lyud.Weather.BlendSeconds 0", "sunangle %g" % sky.get_editor_property("sun_angular_diameter_deg"), "moonsize %g" % st["moon_d"]] + (SHOTS[st["i"]][8:] or [""])[0].split(";"):
            if c.startswith("sunangle "):             # test hook: the sun light's source angle
                sky.get_editor_property("sun_light").set_light_source_angle(float(c.split()[1]))
            elif c.startswith("moonsize "):           # test hook: Europa's apparent size
                cmd(w, "lyud.Sky.MoonSizeDeg %s" % c.split()[1])
                sky.update_sky()
            elif c.startswith("moonvis "):
                sky.get_editor_property("moon_light").set_visibility(c.split()[1] == "1")
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
