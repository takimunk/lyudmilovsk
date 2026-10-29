"""Offscreen play test of the rain and fog effects (ALyudRainFx, ULyudRainFogSubsystem): puts the camera at the
club's door, the brow over the sea, the beach and inside the club under forced weather (drizzle, rain, a downpour,
soaked ground, sea fog, ground fog, haze, a night rain), prints the effects' state and screenshots each spot; the
perf_* pairs time the same downpour with the effects on and off in this one process (CSV profiles).
Leaves the desktop alone (no PIE, no input); run it with test_rainfog.sh (RAINTEST_ONLY=name,name to pick steps).

Prints RAINTEST lines; screenshots land in Saved/Screenshots/MacEditor in the order of STEPS.
"""
import os

import unreal

FACE_Y = 790.0          # the portal face (scene01_layout.json outside.portal_face_y)
SEA_Z = -4300.0
DOOR = (-193.0, 1150.0, 190.0)


def at(x_m, y_m, z):
    """Map metres (X west of the door, Y out of it) to Unreal cm; z in Unreal cm."""
    return (x_m * 100.0, FACE_Y + y_m * 100.0, z)


# (name, location, pitch, yaw, hour offset, weather: rain mm/h, fog 0..1, wet 0..1 (-1 the rain's), fog top m, wind m/s; extra)
STEPS = [
    ("door_dry", DOOR, -4, 0, 0.0, (0.0, 0.0, 0.0, -1, 3), []),
    ("door_drizzle", DOOR, -4, 0, 0.0, (0.3, 0.35, 0.5, -1, 2), []),
    ("door_rain", DOOR, -4, 0, 0.0, (3.0, 0.0, 0.8, -1, 5), []),
    ("door_downpour", DOOR, -10, 0, 0.0, (14.0, 0.0, 1.0, -1, 9), []),
    ("door_ground", DOOR, -35, 0, 0.0, (6.0, 0.0, 1.0, -1, 4), []),
    ("door_soaked_after", DOOR, -30, 0, 0.0, (0.0, 0.0, 1.0, -1, 2), []),
    ("club_inside", (-17.5, -1585.0, 190.0), -5, 90, 0.0, (6.0, 0.4, 1.0, -1, 4), []),
    ("brow_rain", at(60, -10, 600), -12, 0, 0.0, (4.0, 0.0, 0.8, -1, 6), []),
    ("beach_rings", at(118, 5, SEA_Z + 180), -25, 0, 0.0, (4.0, 0.0, 0.8, -1, 1), []),
    ("brow_seafog", at(60, -10, 600), -8, 0, 0.0, (0.0, 0.85, 0.2, 45, 2), []),
    ("door_seafog", DOOR, -4, 0, 0.0, (0.0, 0.85, 0.2, 60, 2), []),
    ("brow_groundfog_morning", at(60, -10, 600), -10, -90, 16.3, (0.0, 0.7, 0.3, 15, 0.5), []),
    ("door_haze", DOOR, -2, 0, 0.0, (0.0, 0.2, 0.0, 200, 4), []),
    ("door_night_rain", DOOR, -6, 0, 5.0, (4.0, 0.1, 1.0, -1, 4), []),
    ("preset_drizzle", DOOR, -6, 0, 0.0, (-1, -1, -1, -1, -1), ["lyud.Weather.Preset drizzle"]),
    ("preset_downpour", at(60, -10, 600), -10, 0, 0.0, (-1, -1, -1, -1, -1), ["lyud.Weather.Preset downpour"]),
    ("preset_seafog", at(60, -10, 600), -8, 0, 0.0, (-1, -1, -1, -1, -1), ["lyud.Weather.Preset seafog"]),
    ("preset_fog", DOOR, -4, 0, 0.0, (-1, -1, -1, -1, -1), ["lyud.Weather.Preset fog"]),
    ("perf_on", DOOR, -6, 0, 0.0, (14.0, 0.0, 1.0, -1, 9), ["lyud.Rain.Fx 1", "csvprofile start"]),
    ("perf_off", DOOR, -6, 0, 0.0, (14.0, 0.0, 1.0, -1, 9), ["lyud.Rain.Fx 0", "csvprofile start"]),
    ("perf_on2", DOOR, -6, 0, 0.0, (14.0, 0.0, 1.0, -1, 9), ["lyud.Rain.Fx 1", "csvprofile start"]),
]
if os.environ.get("RAINTEST_ONLY"):
    STEPS = [s for s in STEPS if s[0] in os.environ["RAINTEST_ONLY"].split(",")]
WAIT = float(os.environ.get("RAINTEST_WAIT", "6"))
st = {"t": 0.0, "i": -1, "phase": 0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def log(s):
    unreal.log_warning("RAINTEST " + s)


def report(w, name):
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    log("%s eye %s" % (name, pc.player_camera_manager.get_camera_location()))
    rf = unreal.LyudRainFogSubsystem.get_rain_fog(w)
    log("  " + (rf.debug_report() if rf else "NO RAINFOG SUBSYSTEM"))
    wx = unreal.LyudWeatherSubsystem.get_weather(w)
    if wx:
        log("  " + wx.debug_report())


def tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < 4.0:
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    if st["phase"] == 0:
        st["i"] += 1
        if st["i"] >= len(STEPS):
            cmd(w, "quit")
            st["phase"] = 9
            return
        name, loc, pitch, yaw, hours, (rain, fog, wet, top, wind), extra = STEPS[st["i"]]
        cm = getattr(pawn, "character_movement", None)
        if cm:
            cm.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
            cm.gravity_scale = 0.0
        pawn.set_actor_enable_collision(False)
        cmd(w, "lyud.Weather.Preset off")   # clears the forced values too: first
        cmd(w, "lyud.SkyHourOffset %f" % hours)
        cmd(w, "lyud.Wind.Speed %f" % wind)
        cmd(w, "lyud.Weather.Rain %f" % rain)
        cmd(w, "lyud.Weather.Fog %f" % fog)
        cmd(w, "lyud.Weather.Wet %f" % wet)
        cmd(w, "lyud.Weather.FogTop %f" % top)
        cmd(w, "lyud.Weather.Cloud %f" % (-1 if rain < 0 else 0.95 if rain > 0 or fog > 0.5 else 0.5))
        for c in extra:
            cmd(w, c)
        st["loc"] = loc
        pawn.set_actor_location(unreal.Vector(*loc), False, True)
        pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))
        st["t0"] = st["t"]
        st["phase"] = 1
    elif st["phase"] == 1:
        pawn.set_actor_location(unreal.Vector(*st["loc"]), False, True)
        name = STEPS[st["i"]][0]
        if st["t"] - st["t0"] > (WAIT * 2 if name.startswith("perf") else WAIT):
            report(w, name)
            if name.startswith("perf"):
                cmd(w, "csvprofile stop")
                log("  csv %s" % name)
            else:
                cmd(w, "HighResShot 1600x900")
            st["phase"] = 2
            st["t0"] = st["t"]
    elif st["phase"] == 2 and st["t"] - st["t0"] > 2.0:
        st["phase"] = 0


unreal.register_slate_post_tick_callback(tick)
