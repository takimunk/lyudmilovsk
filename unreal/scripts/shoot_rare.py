"""Offscreen screenshots of the rare sky (docs/rare_sky.md): the aurora at a few Kp and substorm phases, halos round
the red dwarf and Europa, rainbows, noctilucent clouds, a flare and lightning. Stands the player, flying, at a spot of
Lvl_Scene01 at a local date and hour (lyud.SkyHourOffset from the calendar's now), forces the phenomenon with the
lyud.Rare.* console variables, aims (at the sun, the moon, the antisolar point, the magnetic pole or a yaw) and takes
a 1600x900 shot. No PIE, no input, the desktop is left alone:

  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen -ResX=1600 -ResY=900 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py <this file>"

Prints RARE lines (the sky and the rare sky per shot); screenshots land in Saved/Screenshots/Mac in the order of SHOTS
(RARE_SHOTS=a,b runs the shots whose names start so). tools/sky/rare/shoot.sh wraps it with a watchdog.
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
TERRACE = (W.DOOR_X - 2.0, 33.0)
NOON, DAY = D(2026, 10, 20, 12, 30), D(2026, 10, 21, 13, 0)
NEWMOON, FULLMOON = D(2026, 11, 10, 23, 30), D(2026, 10, 26, 0, 30)
POLE_BEARING = 344.0
# (name, local time, (map x, y), pitch, aim: yaw deg or "sun" / "moon" / "antisun" / "pole" / "north", commands;
#  "STRIKE d cg yaw" makes a strike d m off at yaw degrees from the view, "RARE_DISK k" sets OpticsDiskScale)
SHOTS = [
    # the aurora at a Kp 5 night (low toward the magnetic pole, NNW), a Kp 7 storm (up to the zenith), a Kp 9 one (a
    # corona overhead); the moon is new on 2026-11-10; phases staged by lyud.Rare.AuroraOnset (minutes since breakup)
    ("aurora_kp5_growth", NEWMOON, BROW, 12, "pole", "lyud.Rare.Kp 5;lyud.Rare.AuroraOnset -25"),
    ("aurora_kp5_breakup", NEWMOON, BROW, 12, "pole", "lyud.Rare.Kp 5;lyud.Rare.AuroraOnset 4"),
    ("aurora_kp5_recovery", NEWMOON, BROW, 12, "pole", "lyud.Rare.Kp 5;lyud.Rare.AuroraOnset 35"),
    ("aurora_kp7_breakup", NEWMOON, BROW, 30, "pole", "lyud.Rare.Kp 7;lyud.Rare.AuroraOnset 4"),
    ("aurora_kp7_breakup_wide", NEWMOON, BROW, 20, "pole", "lyud.Rare.Kp 7;lyud.Rare.AuroraOnset 6;fov 110"),
    ("aurora_kp7_recovery", NEWMOON, BROW, 30, "pole", "lyud.Rare.Kp 7;lyud.Rare.AuroraOnset 40"),
    ("aurora_kp9_corona", NEWMOON, BROW, 80, "pole", "lyud.Rare.Kp 9;lyud.Rare.AuroraOnset 5;fov 100"),
    ("aurora_kp7_town", NEWMOON, TERRACE, 18, "pole", "lyud.Rare.Kp 7;lyud.Rare.AuroraOnset 5"),
    ("aurora_kp7_fullmoon", FULLMOON, BROW, 25, "pole", "lyud.Rare.Kp 7;lyud.Rare.AuroraOnset 4"),
    # halos round the red dwarf and Europa
    ("halo_noon", NOON, BROW, 0, "sun", "lyud.Rare.Halo 1;fov 100"),
    ("halo_low_sun", D(2026, 10, 20, 16, 30), BROW, 0, "sun", "lyud.Rare.Halo 1;fov 100"),
    ("halo_sharp_noon", NOON, BROW, 0, "sun", "lyud.Rare.Halo 1;fov 100;RARE_DISK 0.1"),
    ("halo_moon", FULLMOON, BROW, 0, "moon", "lyud.Rare.Halo 1;fov 100"),
    # rainbows opposite a low sun
    ("rainbow_afternoon", D(2026, 10, 20, 15, 45), BROW, 0, "antisun", "lyud.Rare.Rainbow 1;fov 100"),
    ("rainbow_sharp", D(2026, 10, 20, 15, 45), BROW, 0, "antisun", "lyud.Rare.Rainbow 1;fov 100;RARE_DISK 0.1"),
    # noctilucent clouds: a June night, the sun ~11 degrees down in the north
    ("noctilucent", D(2027, 6, 24, 0, 40), BROW, 8, "north", "lyud.Rare.Noctilucent 1;fov 90"),
    # a flare on the star
    ("flare_close", NOON, BROW, 0, "sun", "lyud.Rare.Flare 1;fov 30"),
    ("flare_detail", NOON, BROW, 0, "sun", "lyud.Rare.Flare 0.7;fov 12;r.VolumetricCloud 0"),
    ("flare_detail_late", NOON, BROW, 0, "sun", "lyud.Rare.Flare 0.25;fov 12;r.VolumetricCloud 0"),
    # lightning: a stroke held at its peak, by night and by day, near and far, a flash in the cloud, the leader
    ("strike_night_near", NEWMOON, BROW, 8, 70, "lyud.Rare.StrikeHold 0.04;lyud.Weather.Cloud 0.9;STRIKE 1800 1 0"),
    ("strike_night_far", NEWMOON, BROW, 4, 70, "lyud.Rare.StrikeHold 0.04;lyud.Weather.Cloud 0.9;STRIKE 6000 1 10"),
    ("strike_night_cloud", NEWMOON, BROW, 8, 70, "lyud.Rare.StrikeHold 0.02;lyud.Weather.Cloud 0.9;STRIKE 3000 0 0"),
    ("strike_day", DAY, BROW, 6, 70, "lyud.Rare.StrikeHold 0.04;lyud.Weather.Cloud 0.9;STRIKE 2500 1 0"),
    ("strike_leader", NEWMOON, BROW, 8, 70, "lyud.Rare.StrikeHold 0.02;lyud.Weather.Cloud 0.9;STRIKE 1800 1 0"),
]
if os.environ.get("RARE_SHOTS"):
    SHOTS = [s for s in SHOTS if any(s[0].startswith(p) for p in os.environ["RARE_SHOTS"].split(","))]
# console state every shot starts from
RESET = ["lyud.Rare.Kp -1", "lyud.Rare.Aurora -1", "lyud.Rare.AuroraOnset -1e9", "lyud.Rare.Halo -1", "lyud.Rare.Rainbow -1",
         "lyud.Rare.Noctilucent -1", "lyud.Rare.Flare -1", "lyud.Rare.OpticsSharp 0", "lyud.Rare.StrikeHold -1", "lyud.Weather.Cloud -1", "fov 0",
         "r.VolumetricCloud 1"]
WAIT = float(os.environ.get("RARE_WAIT", "7"))
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


def compass_yaw(bearing):
    """World yaw of a compass bearing (north = +Y, east = -X: ALyudSky, NorthYawDeg 0)."""
    b = math.radians(bearing)
    return math.degrees(math.atan2(math.cos(b), -math.sin(b)))


def aim(sky, what, pitch):
    """(yaw, pitch) for an aim; pitch None keeps the shot's own."""
    if what == "pole":
        return compass_yaw(POLE_BEARING), pitch
    if what == "north":
        return compass_yaw(0.0), pitch
    if what == "antisun":
        return compass_yaw(sky.sun_azimuth() + 180.0), 20.0 - sky.sun_altitude()
    if what == "sun":
        return compass_yaw(sky.sun_azimuth()), sky.sun_altitude() + pitch
    rep = sky.get_debug_report()
    part = rep.split("moon alt ")[1].split()
    return compass_yaw(float(part[2])), float(part[0]) + pitch


def tick(dt):
    try:
        step(dt)
    except Exception as e:                            # never hang offscreen on a script error
        unreal.log_error("RARE script error: %r" % e)
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
    rare = actor(w, unreal.LyudRareSky)
    if st["base"] is None:
        cmd(w, "lyud.SkyHourOffset 0")
        b = sky.shown_time()
        M = unreal.MathLibrary
        st["base"] = D(M.get_year(b), M.get_month(b), M.get_day(b), M.get_hour(b), M.get_minute(b), M.get_second(b))
        unreal.log_warning("RARE calendar now %s, rare sky %s" % (st["base"], "found" if rare else "MISSING"))
        return
    shot = SHOTS[st["i"]] if 0 <= st["i"] < len(SHOTS) else None
    if st["phase"] == 0:
        st["i"] += 1
        if st["i"] >= len(SHOTS):
            cmd(w, "quit")
            st["phase"] = 9
            return
        name, when, (x, y), pitch, what, extra = SHOTS[st["i"]]
        rare.set_editor_property("optics_disk_scale", 1.0)
        for c in RESET + extra.split(";"):
            if c.startswith("RARE_DISK "):
                rare.set_editor_property("optics_disk_scale", float(c.split()[1]))
            elif c and not c.startswith("STRIKE "):
                cmd(w, c)
        cmd(w, "lyud.SkyHourOffset %f" % ((when - st["base"]).total_seconds() / 3600.0))
        cmd(w, "t.MaxFPS 0")
        if hasattr(pawn, "character_movement"):
            pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
        ux, uy, uz = T.to_unreal(x, y, T.ground_z(x, y) + 1.7, FACE_Y)
        pawn.set_actor_location(unreal.Vector(ux, uy, uz), False, True)
        if not isinstance(what, str):
            pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=what))
        st["t0"] = st["t"]
        st["phase"] = 1
    elif st["phase"] == 1 and st["t"] - st["t0"] > 1.5:
        name, when, xy, pitch, what, extra = shot
        if isinstance(what, str):
            yaw, p = aim(sky, what, pitch)
            pc.set_control_rotation(unreal.Rotator(roll=0, pitch=p if p is not None else pitch, yaw=yaw))
        st["phase"] = 2
    elif st["phase"] == 2 and st["t"] - st["t0"] > WAIT:
        name, when, xy, pitch, what, extra = shot
        for c in extra.split(";"):
            if c.startswith("STRIKE "):
                cmd(w, "lyud.Rare.Strike " + c[len("STRIKE "):])
        st["phase"] = 3
        st["t0"] = st["t"]
    elif st["phase"] == 3 and st["t"] - st["t0"] > 0.5:
        unreal.log_warning("RARE %s | %s | %s" % (shot[0], sky.get_debug_report(), rare.debug_report() if rare else "no rare sky"))
        cmd(w, "HighResShot 1600x900")
        st["phase"] = 4
        st["t0"] = st["t"]
    elif st["phase"] == 4 and st["t"] - st["t0"] > 1.5:
        st["phase"] = 0


handle = unreal.register_slate_post_tick_callback(tick)
