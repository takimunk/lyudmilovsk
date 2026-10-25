"""Offscreen play test of the wind and the sea (ULyudWindSubsystem, ALyudSea, ULyudSeabedComponent): flies the
camera to spots over the coast, the beach, the open water and under it, in a few winds and hours, prints the
wind, the sea state, the seabed's life and a floating crate's ride, and screenshots each spot.
Leaves the desktop alone (no PIE, no input); run it with test_sea.sh.

Prints SEATEST lines; screenshots land in Saved/Screenshots/MacEditor in the order of STEPS.
"""
import os

import unreal

FACE_Y = 790.0          # the portal face (scene01_layout.json outside.portal_face_y)
SEA_Z = -4300.0


def at(x_m, y_m, z):
    """Map metres (X west of the door, Y out of it) to Unreal cm; z in Unreal cm."""
    return (x_m * 100.0, FACE_Y + y_m * 100.0, z)


# (name, location, pitch, yaw, hour offset, wind m/s or -1 for the calendar's, extra console commands)
STEPS = [
    ("door_calendar", (-193.0, 1150.0, 190.0), -4, 0, 0.0, -1, []),
    ("brow_calendar", at(60, -10, 600), -12, 0, 0.0, -1, []),
    ("beach_calendar", at(118, 5, SEA_Z + 180), -4, 0, 0.0, -1, ["lyud.Sea.SpawnFloat 15500 1290 -4150"]),
    ("beach_crate", at(118, 5, SEA_Z + 180), -6, 0, 0.0, -1, []),
    ("water_low", at(190, 0, SEA_Z + 300), -12, 10, 0.0, -1, []),
    ("water_storm", at(190, 0, SEA_Z + 300), -8, 10, 0.0, 14, []),
    ("brow_storm", at(60, -10, 600), -12, 0, 0.0, 14, []),
    ("water_calm_dusk", at(190, 0, SEA_Z + 300), -6, 10, 1.6, 1, []),
    ("under_bed", at(185, 10, SEA_Z - 250), -25, 20, 0.0, -1, []),
    ("under_level", at(185, 10, SEA_Z - 200), 0, 60, 0.0, -1, []),
    ("under_up", at(185, 10, SEA_Z - 250), 50, 0, 0.0, -1, []),
    ("under_boulder", "near:3", -15, 0, 0.0, -1, []),
    ("under_meadow", "near:0", -20, 0, 0.0, -1, []),
    ("under_fish", "near:8", 0, 0, 0.0, -1, []),
    ("open_calendar", at(1500, 0, SEA_Z + 400), -10, 10, 0.0, -1, []),
    ("open_storm", at(1500, 0, SEA_Z + 400), -10, 10, 0.0, 14, []),
    ("open_storm_foam", at(1500, 0, SEA_Z + 400), -10, 10, 0.0, 14, ["lyud.Sea.DebugFoam 5"]),
    ("beach_foam", at(118, 5, SEA_Z + 180), -4, 0, 0.0, -1, ["lyud.Sea.DebugFoam 5"]),
    ("swim_sea", at(190, 0, SEA_Z - 100), -10, 0, 0.0, -1, []),
    ("swim_beach", at(118, 5, SEA_Z + 250), -10, 0, 0.0, -1, []),
    ("phone_quality", at(190, 0, SEA_Z + 300), -12, 10, 0.0, -1, ["lyud.Sea.Quality 0"]),
]
if os.environ.get("SEATEST_ONLY"):
    STEPS = [s for s in STEPS if s[0] in os.environ["SEATEST_ONLY"].split(",")]
WAIT = float(os.environ.get("SEATEST_WAIT", "6"))
st = {"t": 0.0, "i": -1, "phase": 0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def log(s):
    unreal.log_warning("SEATEST " + s)


def report(w, name):
    wind = unreal.LyudWindSubsystem.get_wind(w)
    sea = unreal.LyudSea.get_sea(w)
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    eye = pc.player_camera_manager.get_camera_location()
    log("%s eye %s" % (name, eye))
    pawn = pc.get_controlled_pawn()
    cm = getattr(pawn, "character_movement", None)
    sea = unreal.LyudSea.get_sea(w)
    log("  pawn at %s, movement %s, swimmable %s" % (pawn.get_actor_location(), cm.movement_mode if cm else None,
        sea.is_swimmable(pawn.get_actor_location()) if sea else None))
    if wind:
        log("  " + wind.debug_report())
        s = wind.get_wind_at(eye, 2.0, True)
        log("  wind at the eye (2 m): %.2f m/s gust %.2f" % (s.speed, s.gust))
    if not sea:
        log("  NO SEA")
        return
    log("  " + sea.get_debug_report())
    log("  " + sea.seabed.get_debug_report())
    smp = sea.sample_surface(eye)
    log("  surface over the eye z %.1f (immersion %.1f), depth %.0f cm, sea %s, normal %s, water velocity %s" % (
        smp.surface_z, smp.immersion, smp.depth, smp.is_sea, smp.normal, smp.velocity))
    for a in unreal.GameplayStatics.get_all_actors_with_tag(w, "LyudFloat"):
        loc = a.get_actor_location()
        s2 = sea.sample_surface(loc)
        log("  crate at %s, surface there %.1f, crate centre %+.1f cm from it, rot %s" % (
            loc, s2.surface_z, loc.z - s2.surface_z, a.get_actor_rotation()))


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
        name, loc, pitch, yaw, hours, wind, extra = STEPS[st["i"]]
        cm = getattr(pawn, "character_movement", None)
        if name.startswith("swim"):
            # a body dropped where it stands: the sea's water volume should make it swim, the beach not
            pawn.set_actor_enable_collision(True)
            if cm:
                cm.gravity_scale = 1.0
                cm.set_movement_mode(unreal.MovementMode.MOVE_FALLING)
        else:
            if cm:
                cm.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
                cm.gravity_scale = 0.0
            pawn.set_actor_enable_collision(False)
        cmd(w, "lyud.SkyHourOffset %f" % hours)
        cmd(w, "lyud.Wind.Speed %f" % wind)
        cmd(w, "lyud.Sea.DebugFoam 0")
        for c in extra:
            cmd(w, c)
        if isinstance(loc, str):
            # stand 2.5 m off the nearest seabed thing of a kind, at its height + 1 m, looking at it
            sea = unreal.LyudSea.get_sea(w)
            eye = pawn.get_actor_location()
            p = sea.seabed.find_nearest(int(loc.split(":")[1]), eye)
            loc = (p.x - 250.0, p.y, p.z + 100.0)
            st["loc"] = loc
        st["loc"] = loc
        pawn.set_actor_location(unreal.Vector(*loc), False, True)
        pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))
        st["t0"] = st["t"]
        st["phase"] = 1
    elif st["phase"] == 1:
        # keep it still against any drift
        if not STEPS[st["i"]][0].startswith("swim"):
            pawn.set_actor_location(unreal.Vector(*st["loc"]), False, True)
        if st["t"] - st["t0"] > WAIT:
            report(w, STEPS[st["i"]][0])
            cmd(w, "HighResShot 1600x900")
            st["phase"] = 2
            st["t0"] = st["t"]
    elif st["phase"] == 2 and st["t"] - st["t0"] > 1.5:
        st["phase"] = 0


unreal.register_slate_post_tick_callback(tick)
