"""Offscreen comparison of the day's sunsets (docs/sun.md, "Sunsets"; ULyudSunset): the same view from the club's cliff
over the Baltic toward the sun, each kind (lyud.Sunset.Profile 0..6), a few days left to their own character and the
look without it (lyud.Sunset.Intensity 0), in five weathers (clean clear air, broken altocumulus, overcast, after rain,
haze) with the sun ~2 degrees up, in broken cloud just after sunset (-1) and in clear air and broken cloud ~4 degrees down; then the town across the light
and a few sunrises. No PIE, no input, the desktop is left alone:

  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py <this file>"

Prints a SUNSETSHOT line per shot (name, then the sky's report with the SUNSET profile); the screenshots land in
Saved/Screenshots/Mac in the order of the shots. tools/sky/sunset/sheet.py builds the sheet from the log and them.
SUNSET_ROWS=pink,off and SUNSET_COLS=clear,broken pick rows and columns; SUNSET_WAIT seconds per shot (8).
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
DAY = D(2026, 11, 1)

# the weathers: (column, sun altitude, console commands)
WEATHERS = {
    "clear": "lyud.Weather.Preset clear;lyud.Sunset.Clean 0;lyud.Sunset.Haze 0",
    "broken": "lyud.Weather.Preset clear;lyud.Clouds.Preset ac;lyud.Sunset.Clean 0;lyud.Sunset.Haze 0",
    "overcast": "lyud.Weather.Preset overcast",
    "afterrain": "lyud.Weather.Preset fair;lyud.Sunset.Clean 1;lyud.Sunset.Haze 0",
    "haze": "lyud.Weather.Preset clear;lyud.Weather.Fog 0.15;lyud.Sunset.Clean 0;lyud.Sunset.Haze 0.85",
}
COLS = [("clear", 2.0), ("broken", 2.0), ("overcast", 2.0), ("afterrain", 2.0), ("haze", 2.0), ("broken", -1.0), ("broken", -4.0), ("clear", -4.0)]
# the rows: (row, the day, console commands)
ROWS = [("golden", DAY, "lyud.Sunset.Profile 0"), ("orange", DAY, "lyud.Sunset.Profile 1"), ("pink", DAY, "lyud.Sunset.Profile 2"),
        ("red", DAY, "lyud.Sunset.Profile 3"), ("crimson", DAY, "lyud.Sunset.Profile 4"), ("grey", DAY, "lyud.Sunset.Profile 5"),
        ("spectacular", DAY, "lyud.Sunset.Profile 6"), ("nov03", D(2026, 11, 3), ""), ("nov08", D(2026, 11, 8), ""),
        ("off", DAY, "lyud.Sunset.Intensity 0")]
if os.environ.get("SUNSET_ROWS"):
    ROWS = [r for r in ROWS if r[0] in os.environ["SUNSET_ROWS"].split(",")]
if os.environ.get("SUNSET_COLS"):
    COLS = [c for c in COLS if c[0] in os.environ["SUNSET_COLS"].split(",")]

# (name, day, sunrise?, sun altitude, map x, y, view, console commands)
SHOTS = []
for r, day, rc in ROWS:
    for c, alt in COLS:
        SHOTS.append(("%s__%s%+d" % (r, c, alt), day, False, alt, BROW[0], BROW[1], "sea", rc + ";" + WEATHERS[c]))
if not os.environ.get("SUNSET_ONLY_SEA"):
    for r, day, rc in ROWS:
        if r in ("golden", "pink", "crimson", "grey", "off"):
            SHOTS.append(("town_%s__broken+2" % r, day, False, 2.0, TERRACE[0], TERRACE[1] + 25.0, "town", rc + ";" + WEATHERS["broken"]))
    for r, day, rc in [("auto", DAY, ""), ("auto", D(2026, 11, 3), ""), ("off", DAY, "lyud.Sunset.Intensity 0")]:
        SHOTS.append(("sunrise_%s_%s__broken+2" % (r, day.strftime("%b%d").lower()), day, True, 2.0, BROW[0], BROW[1], "sea", rc + ";" + WEATHERS["broken"]))
WAIT = float(os.environ.get("SUNSET_WAIT", "8"))
RESET = ["lyud.Weather.Preset off", "lyud.Clouds.Preset off", "lyud.Weather.Fog -1", "lyud.Weather.Cloud -1", "lyud.Weather.Rain -1",
         "lyud.Sunset.Profile -1", "lyud.Sunset.Intensity 1", "lyud.Sunset.Clean -1", "lyud.Sunset.Haze -1", "lyud.Sunset.Seed 0",
         "lyud.Weather.BlendSeconds 0", "fov 0", "t.MaxFPS 0"]
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


def hours_for(w, sky, day, sunrise, alt):
    """The hour offset at which the sun stands at alt on that day's evening (or morning): bisection on the ephemeris."""
    def alt_at(h):
        cmd(w, "lyud.SkyHourOffset %f" % ((day - st["base"]).total_seconds() / 3600.0 + h))
        sky.update_sky()
        return sky.sun_altitude()
    lo, hi = (4.0, 12.0) if sunrise else (12.0, 21.0)   # rising: alt grows with h; setting: falls
    for _ in range(40):
        mid = 0.5 * (lo + hi)
        a = alt_at(mid)
        if (a < alt) == sunrise:
            lo = mid
        else:
            hi = mid
    alt_at(0.5 * (lo + hi))
    return 0.5 * (lo + hi)


def aim(sky, view):
    alt, az = sky.sun_altitude(), sky.sun_azimuth()
    a, z = math.radians(alt), math.radians(az)
    x, y = -math.sin(z) * math.cos(a), math.cos(z) * math.cos(a)   # north = +Y, east = -X
    yaw = math.degrees(math.atan2(y, x))
    if view == "town":                                   # across the light: the lit town and the sky away from the sun
        return yaw + 120.0, 12.0
    return yaw, 7.0                                      # the sun low in the frame, the clouds over it, the sea below


def tick(dt):
    try:
        step(dt)
    except Exception as e:                               # never hang offscreen on a script error
        unreal.log_error("SUNSETSHOT script error: %r" % e)
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
        unreal.log_warning("SUNSETSHOT calendar now %s, %d shots" % (st["base"], len(SHOTS)))
        st["t0"] = st["t"]
        st["phase"] = 8                                   # shaders and clouds warm up first
        return
    if st["phase"] == 8:
        if st["t"] - st["t0"] > 20.0:
            st["phase"] = 0
        return
    if st["phase"] == 0:
        st["i"] += 1
        if st["i"] >= len(SHOTS):
            cmd(w, "quit")
            st["phase"] = 9
            return
        name, day, sunrise, alt, x, y, view, cmds = SHOTS[st["i"]]
        for c in RESET + cmds.split(";"):
            if c:
                cmd(w, c)
        if hasattr(pawn, "character_movement"):
            pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
        ux, uy, uz = T.to_unreal(x, y, T.ground_z(x, y) + 1.7, FACE_Y)
        pawn.set_actor_location(unreal.Vector(ux, uy, uz), False, True)
        hours_for(w, sky, day, sunrise, alt)
        clouds = unreal.LyudCloudSubsystem.get_clouds(w)
        if clouds:
            clouds.snap()                                 # the new weather's clouds at once, not eased in
        yaw, pitch = aim(sky, view)
        pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))
        cmd(w, "fov 80")
        st["t0"] = st["t"]
        st["phase"] = 2
    elif st["phase"] == 2 and st["t"] - st["t0"] > WAIT:
        name = SHOTS[st["i"]][0]
        unreal.log_warning("SUNSETSHOT %s | %s" % (name, sky.get_debug_report()))
        cmd(w, "HighResShot 1280x720")
        st["phase"] = 3
        st["t0"] = st["t"]
    elif st["phase"] == 3 and st["t"] - st["t0"] > 1.5:
        st["phase"] = 0


handle = unreal.register_slate_post_tick_callback(tick)
