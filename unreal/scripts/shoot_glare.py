"""Offscreen screenshots of the sun's glare (docs/sun.md, "Glare", ULyudSunGlare): the blinding noon star, the squint, a
blink and the "ай" when staring, the adapted star, the afterimages after looking away (bright, then the dark blot fading),
the star blinding again after a couple of seconds away, sunsets fresh and adapted, the glare off (lyud.Sun.Glare 0) and
on Europa. No PIE, no input, the desktop is left alone:

  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen -ResX=1600 -ResY=900 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py <this file>"

Screenshots are taken with the UI ("shot showui": the glare is a Slate overlay) and land in Saved/Screenshots/MacEditor in
the order of the GLARE lines. GLARE_SHOTS=a,b runs just those scenes. GLARE_FILM=1 instead takes a frame every tick of
one stare (look at the sun, adapt, look away, look back) at lyud.Sun.Glare film speed (slomo GLARE_SLOMO, 0.2) for a
video.
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
TERRACE = (W.DOOR_X - 2.0, 8.0)
NOON = D(2026, 11, 1, 12, 30)
SET1, SET2 = D(2026, 11, 1, 16, 15), D(2026, 11, 1, 16, 38)
CLEAR = "lyud.Weather.Preset off;lyud.Weather.Cloud 0.15;lyud.Clouds.Preset off"

# a scene: (name, time, place, steps); a step is (seconds to wait, aim, commands, shot name or None). aim: "sun" (the
# star in the middle), "sun-wide" (the star high in a wide view), "away" (turned 50 degrees off it, down to the land),
# "keep" (as it is). A wait of "lids" waits until the hero's eyelids are shut, then freezes the eye for the shot.
SCENES = [
    ("noon", NOON, BROW, [
        (2.0, "sun-wide", "lyud.Sun.Glare.Reset", "noon_bright_wide"),
        (0.5, "sun", "lyud.Sun.Glare.Reset", None),
        (1.6, "keep", "", "noon_stare_squint"),
        ("lids", "keep", "", "noon_blink"),
        (3.0, "keep", "", "noon_adapting"),
        (6.0, "keep", "", "noon_adapted"),
        (0.6, "away", "", "after_0s6"),
        (1.8, "keep", "", "after_2s4"),
        (3.0, "keep", "", "after_5s4"),
        (0.1, "sun", "", "back_at_once"),
        (0.1, "away", "", None),
        (3.0, "keep", "", "after_8s"),
        (0.2, "sun", "", "back_after_3s_away"),
    ]),
    ("noon_off", NOON, BROW, [
        (2.0, "sun", "lyud.Sun.Glare 0", "noon_glare_off"),
        (0.5, "sun", "lyud.Sun.Glare 1;lyud.Sun.Glare.Reset", None),
    ]),
    ("sunset", SET1, BROW, [
        (2.0, "sun-wide", "lyud.Sun.Glare.Reset", "sunset_bright_wide"),
        (1.0, "sun", "lyud.Sun.Glare.Reset", "sunset_bright"),
        (0.5, "sun", "lyud.Sun.Glare.Adapt 1", "sunset_adapted"),
        (0.5, "sun", "lyud.Sun.Glare.Adapt -1;lyud.Sun.Glare.Reset", None),
    ]),
    ("sunset_low", SET2, BROW, [
        (2.0, "sun-wide", "lyud.Sun.Glare.Reset", "sunset_low_bright_wide"),
        (1.0, "sun", "lyud.Sun.Glare.Reset", "sunset_low_bright"),
        (0.5, "sun", "lyud.Sun.Glare.Adapt 1", "sunset_low_adapted"),
        (0.5, "sun", "lyud.Sun.Glare.Adapt -1;lyud.Sun.Glare.Reset", None),
    ]),
    ("terrace", SET1, (TERRACE[0], TERRACE[1] + 25.0), [
        (2.0, "sun-wide", "lyud.Sun.Glare.Reset", "terrace_sunset_bright"),
    ]),
    ("flash", NOON, BROW, [
        (1.0, "away", "lyud.Sun.Glare.Reset;lyud.Sun.Glare.FlashBlind 1", "flash_start"),
        ("lids", "keep", "", "flash_blink"),
        (1.0, "keep", "", "flash_after_1_blink"),
        (2.2, "keep", "", "flash_after_3_blinks"),
        (3.0, "keep", "", "flash_gone"),
    ]),
    ("europa", NOON, None, [
        (8.0, "sun-wide", "lyud.Europa.GoEuropa;lyud.Sun.Glare.Reset", "europa_bright_wide"),
        (1.0, "sun", "lyud.Sun.Glare.Reset", "europa_bright"),
        (2.0, "keep", "", None),
        ("lids", "keep", "", "europa_blink"),
        (7.0, "keep", "", "europa_adapted"),
        (1.5, "away", "", "europa_after"),
    ]),
]
# GLARE_TUNE=1: the low sun's colour, adapted and fresh, against the sky's own horizon settings (tuning)
if os.environ.get("GLARE_TUNE"):
    SCENES = [("tune_" + t, SET1 if t.startswith("s1") else SET2, BROW, [
        (2.0, "sun", c + ";lyud.Sun.Glare.Reset;lyud.Sun.Glare.Adapt 1", "tune_%s_adapted" % t),
        (1.0, "sun", "lyud.Sun.Glare.Adapt -1;lyud.Sun.Glare.Reset", "tune_%s_fresh" % t),
        (0.2, "sun", "lyud.Sky.SunHorizonDimming -1;lyud.Sky.SunDiskGain 1;lyud.Sky.LowSunDiskGain -1", None)])
        for t, c in (("s1_base", ""), ("s1_dim07", "lyud.Sky.SunHorizonDimming 0.7"), ("s1_dim1", "lyud.Sky.SunHorizonDimming 1"),
                     ("s1_gain05", "lyud.Sky.SunDiskGain 0.5"), ("s1_lowgain0", "lyud.Sky.LowSunDiskGain 0;lyud.Sky.SunHorizonDimming 0.7"),
                     ("s2_base", ""), ("s2_dim07", "lyud.Sky.SunHorizonDimming 0.7"), ("s2_dim1", "lyud.Sky.SunHorizonDimming 1"))]
if os.environ.get("GLARE_SHOTS"):
    SCENES = [s for s in SCENES if s[0] in os.environ["GLARE_SHOTS"].split(",")]
FILM = os.environ.get("GLARE_FILM", "") == "1"
SLOMO = float(os.environ.get("GLARE_SLOMO", "0.2"))
if FILM:
    # one stare in game seconds: (from, to, aim): at the sea first, onto the sun, held, away, back
    FILM_PLAN = [(0.0, 1.5, "sea"), (1.5, 2.3, "pan_in"), (2.3, 13.0, "sun"), (13.0, 13.4, "pan_out"), (13.4, 19.0, "away"), (19.0, 19.4, "pan_back"), (19.4, 22.0, "sun")]
    FILM_END = 22.0
st = {"t": 0.0, "scene": -1, "step": 0, "phase": "start", "base": None}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name().startswith("Lvl_") and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def actor(w, cls):
    a = unreal.GameplayStatics.get_all_actors_of_class(w, cls)
    return a[0] if a else None


def sun_aim(sky):
    alt, az = sky.sun_altitude(), sky.sun_azimuth()
    a, z = math.radians(alt), math.radians(az)
    x, y = -math.sin(z) * math.cos(a), math.cos(z) * math.cos(a)
    return math.degrees(math.atan2(y, x)), alt


def aim(pc, sky, what):
    yaw, pitch = sun_aim(sky)
    if what == "keep":
        return
    if what == "sun-wide":
        pitch = max(pitch - 14.0, -8.0)
    elif what == "away":
        yaw += 50.0
        pitch = -10.0
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))


def glare_of(w):
    for s in unreal.ObjectIterator(unreal.LyudSunGlare):
        if s.get_world() == w:
            return s


def log(w, name):
    g = glare_of(w)
    sky = actor(w, unreal.LyudSky)
    unreal.log_warning("GLARE %s | %s | %s" % (name, g.get_debug_line() if g else "no glare", sky.get_debug_report() if sky else ""))


def tick(dt):
    try:
        step(dt)
    except Exception as e:                            # never hang offscreen on a script error
        unreal.log_error("GLARE script error: %r" % e)
        w = world()
        if w:
            cmd(w, "quit")
        unreal.unregister_slate_post_tick_callback(handle)


def place(w, pc, pawn, when, where, sky):
    hours = (when - st["base"]).total_seconds() / 3600.0
    cmd(w, "lyud.SkyHourOffset %f" % hours)
    if where is None:
        return
    if hasattr(pawn, "character_movement"):
        pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
    x, y = where
    ux, uy, uz = T.to_unreal(x, y, T.ground_z(x, y) + 1.7, FACE_Y)
    pawn.set_actor_location(unreal.Vector(ux, uy, uz), False, True)


def film(w, pc, sky):
    """GLARE_FILM: a frame every tick through one stare, the view following FILM_PLAN in game seconds."""
    now = unreal.GameplayStatics.get_time_seconds(w) - st["film0"]
    yaw, pitch = sun_aim(sky)
    sea = (yaw + 35.0, -6.0)
    off = (yaw + 50.0, -10.0)
    for a, b, what in FILM_PLAN:
        if a <= now < b:
            k = (now - a) / max(b - a, 1e-3)
            k = k * k * (3 - 2 * k)
            if what == "sea":
                y, p = sea
            elif what == "pan_in":
                y, p = sea[0] + (yaw - sea[0]) * k, sea[1] + (pitch - sea[1]) * k
            elif what == "sun" or what == "pan_back" and k >= 1:
                y, p = yaw, pitch
            elif what == "pan_out":
                y, p = yaw + (off[0] - yaw) * k, pitch + (off[1] - pitch) * k
            elif what == "pan_back":
                y, p = off[0] + (yaw - off[0]) * k, off[1] + (pitch - off[1]) * k
            else:
                y, p = off
            pc.set_control_rotation(unreal.Rotator(roll=0, pitch=p, yaw=y))
    if now >= FILM_END:
        cmd(w, "slomo 1")
        cmd(w, "quit")
        st["phase"] = "done"
        return
    st["frame"] = st.get("frame", 0) + 1
    g = glare_of(w)
    unreal.log_warning("GLARE film%04d t %.3f | %s" % (st["frame"], now, g.get_debug_line() if g else ""))
    cmd(w, "shot showui")


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
        for c in CLEAR.split(";") + ["lyud.Weather.BlendSeconds 0", "t.MaxFPS 0", "lyud.Sun.Glare 1", "lyud.Sun.Glare.Adapt -1", "lyud.Hud 0"]:
            cmd(w, c)
        unreal.log_warning("GLARE calendar now %s" % st["base"])
        st["t0"] = st["t"]
        return
    if FILM:
        if st["phase"] == "start":
            place(w, pc, pawn, NOON, BROW, sky)
            st["phase"] = "settle"
            st["t0"] = st["t"]
        elif st["phase"] == "settle" and st["t"] - st["t0"] > 12.0:
            yaw, pitch = sun_aim(sky)
            pc.set_control_rotation(unreal.Rotator(roll=0, pitch=-6.0, yaw=yaw + 35.0))
            cmd(w, "lyud.Sun.Glare.Reset")
            cmd(w, "slomo %g" % SLOMO)
            st["film0"] = unreal.GameplayStatics.get_time_seconds(w)
            st["phase"] = "film"
        elif st["phase"] == "film":
            film(w, pc, sky)
        return
    if st["phase"] == "start":
        st["scene"] += 1
        if st["scene"] >= len(SCENES):
            cmd(w, "quit")
            st["phase"] = "done"
            return
        name, when, where, steps = SCENES[st["scene"]]
        place(w, pc, pawn, when, where, sky)
        aim(pc, sky, "sun-wide")
        st["step"] = 0
        st["phase"] = "settle"
        st["t0"] = st["t"]
        st["g0"] = unreal.GameplayStatics.get_time_seconds(w)
    elif st["phase"] == "settle" and st["t"] - st["t0"] > 6.0:
        st["phase"] = "step"
        st["begin"] = True
    elif st["phase"] == "step":
        steps = SCENES[st["scene"]][3]
        if st["step"] >= len(steps):
            st["phase"] = "start"
            return
        wait, what, cmds, shot = steps[st["step"]]
        if st.pop("begin", False):
            if wait == "lids":
                cmds += ";lyud.Sun.Glare.FreezeAtShut 1"
            for c in cmds.split(";"):
                if c:
                    cmd(w, c)
            aim(pc, sky, what)
            st["g0"] = unreal.GameplayStatics.get_time_seconds(w)
            return
        g = glare_of(w)
        elapsed = unreal.GameplayStatics.get_time_seconds(w) - st["g0"]
        if wait == "lids":
            # the C++ freezes the eye the frame a blink shuts it (lyud.Sun.Glare.FreezeAtShut)
            if not g or g.get_lids() < 0.99:
                if elapsed < 12.0:
                    return
            cmd(w, "lyud.Sun.Glare.Freeze 1")
            cmd(w, "lyud.Sun.Glare.FreezeAtShut 0")
        elif elapsed < wait:
            return
        if shot:
            log(w, shot)
            cmd(w, "shot showui")
            st["phase"] = "shot"
            st["t0"] = st["t"]
        else:
            st["step"] += 1
            st["begin"] = True
    elif st["phase"] == "shot" and st["t"] - st["t0"] > 0.4:
        cmd(w, "lyud.Sun.Glare.Freeze 0")
        st["step"] += 1
        st["phase"] = "step"
        st["begin"] = True


handle = unreal.register_slate_post_tick_callback(tick)
