"""Offscreen screenshots of the clouds (docs/clouds.md): each cloud genus forced with lyud.Clouds.Preset, seen from the
cliff's brow over the sea at a few hours, plus sunsets and a moonlit night under clouds. No PIE, no input:

  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen -ResX=1600 -ResY=900 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py <this file>"

Prints CLOUDSHOT lines (the cloud layers per shot); screenshots land in Saved/Screenshots in the order of SHOTS
(CLOUD_SHOTS=a,b runs just those; CLOUD_EXTRA="cmd;cmd" runs before every shot).
"""
import datetime
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
import terrain as T  # noqa: E402

FACE_Y = float(os.environ.get("TERRAIN_FACE_Y", "790"))
D = datetime.datetime
BROW_Y = 40.0
BROW = (T.W.cliff_top(BROW_Y) - 4.0, BROW_Y)
AFTERNOON = D(2026, 10, 20, 13, 30)
SUNSET = D(2026, 10, 20, 16, 55)
NIGHT = D(2026, 10, 26, 0, 30)
# (name, preset, local time, pitch, yaw or "sun"/"sun-wide"/"moon"[, console commands])
SHOTS = [
    ("clear", "clear", AFTERNOON, 12, 10),
    ("cumulus", "cu", AFTERNOON, 10, 10),
    ("cumulus_up", "cu", AFTERNOON, 45, 60),
    ("congestus", "cucon", AFTERNOON, 8, 10),
    ("cumulonimbus", "cb", AFTERNOON, 6, 10),
    ("cumulonimbus_far", "cb", AFTERNOON, 4, 100),
    ("stratocumulus", "sc", AFTERNOON, 12, 10),
    ("stratus", "st", AFTERNOON, 8, 10),
    ("nimbostratus", "ns", AFTERNOON, 6, 10),
    ("altocumulus", "ac", AFTERNOON, 30, 10),
    ("altostratus_sun", "as", AFTERNOON, 0, "sun-wide"),
    ("altostratus_sun_close", "as", AFTERNOON, 0, "sun", "fov 40"),
    ("nimbostratus_sun", "ns", AFTERNOON, 0, "sun-wide"),
    ("stratocumulus_sun", "sc", AFTERNOON, 0, "sun-wide"),
    ("lenticular", "lens", AFTERNOON, 10, 10),
    ("cirrus", "ci", AFTERNOON, 30, 10),
    ("cirrocumulus", "cc", AFTERNOON, 40, 10),
    ("cirrostratus_halo", "cs", AFTERNOON, 0, "sun-wide", "fov 100"),
    ("warmfront", "warmfront", AFTERNOON, 12, 10),
    ("coldfront", "coldfront", AFTERNOON, 6, 10),
    ("showers", "showers", AFTERNOON, 6, 10),
    ("sunset_cu", "cu", SUNSET, 0, "sun-wide"),
    ("sunset_sc", "sc", SUNSET, 0, "sun-wide"),
    ("sunset_ci", "ci", SUNSET, 0, "sun-wide"),
    ("night_sc", "sc", NIGHT, 10, "moon"),
    ("weather_now", "off", AFTERNOON, 12, 10),
    ("weather_ground", "off", AFTERNOON, -25, 100),
    # the sea horizon under a closed deck: no bright strip between the deck and the sea
    ("horizon_ns", "ns", AFTERNOON, 1, 10),
    ("horizon_st", "st", AFTERNOON, 1, 10),
    ("horizon_ns_nofog", "ns", AFTERNOON, 1, 10, "r.Fog 0"),
    ("horizon_ns_fog", "ns", AFTERNOON, 1, 10, "r.Fog 1"),
    # shadows on the terrace by the club's door: crisp under a clear sky, gone under a deck
    ("terrace_shadows", "off", AFTERNOON, -30, 250, "at %.1f 33" % (T.W.DOOR_X - 2.0)),
    # from above: the layers' pattern and cover
    ("cumulus_above", "cu", AFTERNOON, -60, 10, "up 4000"),
    ("stratocumulus_above", "sc", AFTERNOON, -60, 10, "up 3000"),
    ("cumulonimbus_above", "cb", AFTERNOON, -35, 10, "up 12000"),
]
# CLOUD_PERF=1: frame time with the clouds on and off (r.VolumetricCloud) under a few skies, in this one process
PERF = bool(os.environ.get("CLOUD_PERF"))
if PERF:
    SHOTS = []
    for rnd in (1, 2):
        for p in ("clear", "cu", "sc", "cb", "ns", "lens"):
            for on in (1, 0):
                SHOTS.append(("perf%d_%s_%s" % (rnd, p, "on" if on else "off"), p, AFTERNOON, 10, 10, "r.VolumetricCloud %d" % on))
# CLOUD_TIMELAPSE=2026-10-30-06,0.5,24: the weather's own clouds every 0.5 h from that hour (no forcing), to watch
# genera change into one another
if os.environ.get("CLOUD_TIMELAPSE"):
    a = os.environ["CLOUD_TIMELAPSE"].split(",")
    y, mo, d, hh = (int(v) for v in a[0].split("-"))
    step, n = float(a[1]), int(a[2])
    t0 = D(y, mo, d, hh)
    SHOTS = [("tl_%s" % (t0 + datetime.timedelta(hours=step * k)).strftime("%m%d_%H%M"), "off", t0 + datetime.timedelta(hours=step * k),
              8, 10, "lyud.Weather.BlendSeconds 0") for k in range(n)]
# CLOUD_DIAG=1: what washes the scene out (the other weather systems switched off one by one)
if os.environ.get("CLOUD_DIAG"):
    SHOTS = [("diag_" + tag, "cu", AFTERNOON, 10, 10, c) for tag, c in (
        ("plain", ""), ("nograde", "lyud.Grade 0"), ("nofog", "lyud.Fog.Fx 0"), ("norain", "lyud.Rain.Fx 0"),
        ("nofog_scale", "lyud.Fog.Scale 0"), ("noclouds", "r.VolumetricCloud 0"), ("nofog_r", "r.Fog 0"),
        ("none", "lyud.Grade 0;lyud.Fog.Fx 0;lyud.Rain.Fx 0;r.Fog 0"))]
if os.environ.get("CLOUD_SHOTS"):
    SHOTS = [s for s in SHOTS if s[0] in os.environ["CLOUD_SHOTS"].split(",")]
EXTRA = [c for c in os.environ.get("CLOUD_EXTRA", "").split(";") if c]
WAIT = float(os.environ.get("CLOUD_WAIT", "6"))
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
        part = sky.get_debug_report().split("moon alt ")[1].split()
        alt, az = float(part[0]), float(part[2])
    a, z = math.radians(alt), math.radians(az)
    x, y = -math.sin(z) * math.cos(a), math.cos(z) * math.cos(a)
    yaw = math.degrees(math.atan2(y, x))
    return yaw, (alt if not what.endswith("wide") else max(alt - 6.0, 2.0))


def tick(dt):
    try:
        step(dt)
    except Exception as e:                            # never hang offscreen on a script error
        unreal.log_error("CLOUDSHOT script error: %r" % e)
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
    if st["base"] is None:
        cmd(w, "lyud.SkyHourOffset 0")
        b = sky.shown_time()
        M = unreal.MathLibrary
        st["base"] = D(M.get_year(b), M.get_month(b), M.get_day(b), M.get_hour(b), M.get_minute(b), M.get_second(b))
        return
    if st["phase"] == 0 and st["i"] < 0 and st["t"] < float(os.environ.get("CLOUD_WARMUP", "300")):
        # wait for the cloud material's shaders (the clouds stay off until they are ready)
        cmd(w, "lyud.Clouds.Preset cu")
        if "ready 1" not in unreal.LyudCloudSubsystem.get_clouds(w).debug_report():
            return
        unreal.log_warning("CLOUDSHOT warm after %.0f s" % st["t"])
    if st["phase"] == 0:
        st["i"] += 1
        if st["i"] >= len(SHOTS):
            cmd(w, "quit")
            st["phase"] = 9
            return
        name, preset, when, pitch, yaw = SHOTS[st["i"]][:5]
        up = 1.7
        at = BROW
        for c in ["fov 0", "t.MaxFPS 0", "lyud.Clouds.Preset " + preset] + EXTRA + (SHOTS[st["i"]][5:] or [""])[0].split(";"):
            if c.startswith("up "):                   # test hook: metres over the brow
                up = float(c.split()[1])
            elif c.startswith("at "):                 # test hook: stand at map x, y (metres) instead of the brow
                at = (float(c.split()[1]), float(c.split()[2]))
            elif c:
                cmd(w, c)
        cmd(w, "lyud.SkyHourOffset %f" % ((when - st["base"]).total_seconds() / 3600.0))
        if hasattr(pawn, "character_movement"):
            pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
        ux, uy, uz = T.to_unreal(at[0], at[1], T.ground_z(*at) + up, FACE_Y)
        pawn.set_actor_location(unreal.Vector(ux, uy, uz), False, True)
        if not isinstance(yaw, str):
            pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))
        st["t0"] = st["t"]
        st["phase"] = 1
    elif st["phase"] == 1 and st["t"] - st["t0"] > 1.0:
        yaw = SHOTS[st["i"]][4]
        if isinstance(yaw, str):
            y, p = aim(sky, yaw)
            pc.set_control_rotation(unreal.Rotator(roll=0, pitch=p, yaw=y))
        st["phase"] = 2
    elif st["phase"] == 2 and PERF and st["t"] - st["t0"] > 3.0:
        st.setdefault("dts", []).append(dt)
        if st["t"] - st["t0"] > 8.0:
            d = sorted(st.pop("dts"))
            unreal.log_warning("CLOUDPERF %s median %.2f ms p90 %.2f ms n %d" % (SHOTS[st["i"]][0], d[len(d) // 2] * 1000, d[int(len(d) * 0.9)] * 1000, len(d)))
            st["phase"] = 0
    elif st["phase"] == 2 and st["t"] - st["t0"] > WAIT:
        clouds = unreal.LyudCloudSubsystem.get_clouds(w)
        rep = clouds.debug_report() if clouds else "no cloud subsystem"
        unreal.log_warning("CLOUDSHOT %s | %s | sun alt %.1f az %.1f" % (SHOTS[st["i"]][0], rep, sky.sun_altitude(), sky.sun_azimuth()))
        cmd(w, "HighResShot 1600x900")
        st["phase"] = 3
        st["t0"] = st["t"]
    elif st["phase"] == 3 and st["t"] - st["t0"] > 1.5:
        st["phase"] = 0


handle = unreal.register_slate_post_tick_callback(tick)
