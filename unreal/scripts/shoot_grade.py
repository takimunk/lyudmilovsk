"""Offscreen A/B of the seasonal/weather colour grade (ULyudGradeSubsystem): the same spot and moment with lyud.Grade 1
and 0 — a winter dusk and night at the club's door (lamps), a summer day and golden hour over the sea, the scene's
autumn start, a grey rain, and inside the club (the grade must be off there). Run with shoot_grade.sh.
Prints GRADETEST lines; screenshots in Saved/Screenshots/MacEditor in STEPS order (on, off for each)."""
import os

import unreal

FACE_Y = 790.0
DOOR = (-193.0, 1150.0, 190.0)


def at(x_m, y_m, z):
    return (x_m * 100.0, FACE_Y + y_m * 100.0, z)


# (name, where, pitch, yaw, date y m d h mi, forced: cloud, rain)
STEPS = [
    ("winter_dusk_door", DOOR, -4, 0, (2027, 1, 18, 16, 40), (0.35, 0.0)),
    ("winter_night_door", DOOR, -4, 0, (2027, 1, 18, 19, 30), (0.35, 0.0)),
    ("winter_night_left", DOOR, -4, -90, (2027, 1, 18, 19, 30), (0.35, 0.0)),
    ("winter_night_right", DOOR, -4, 90, (2027, 1, 18, 19, 30), (0.35, 0.0)),
    ("winter_night_back", DOOR, -4, 180, (2027, 1, 18, 19, 30), (0.35, 0.0)),
    ("summer_day_brow", at(60, -10, 600), -10, 0, (2027, 7, 10, 13, 0), (0.3, 0.0)),
    ("summer_golden_brow", at(60, -10, 600), -6, 0, (2027, 7, 10, 20, 40), (0.2, 0.0)),
    ("autumn_start_door", DOOR, -4, 0, (2026, 10, 20, 16, 0), (-1, -1)),
    ("autumn_rain_door", DOOR, -4, 0, (2026, 10, 22, 12, 0), (1.0, 3.0)),
    ("club_inside_winter", (-17.5, -1585.0, 190.0), -5, 90, (2027, 1, 18, 16, 40), (0.35, 0.0)),
]
if os.environ.get("GRADE_ONLY"):
    STEPS = [s for s in STEPS if s[0] in os.environ["GRADE_ONLY"].split(",")]
WAIT = float(os.environ.get("GRADE_WAIT", "7"))
st = {"t": 0.0, "i": -1, "ab": 0, "phase": 0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def log(s):
    unreal.log_warning("GRADETEST " + s)


def tick(dt):
    try:
        st["t"] += dt
        w = world()
        if not w or st["t"] < 5.0:
            return
        pc = unreal.GameplayStatics.get_player_controller(w, 0)
        pawn = pc.get_controlled_pawn()
        if st["phase"] == 0:
            if st["ab"] == 0:
                st["i"] += 1
            if st["i"] >= len(STEPS):
                cmd(w, "quit")
                st["phase"] = 9
                unreal.unregister_slate_post_tick_callback(handle)
                return
            name, loc, pitch, yaw, (y, mo, d, h, mi), (cloud, rain) = STEPS[st["i"]]
            cm = getattr(pawn, "character_movement", None)
            if cm:
                cm.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
                cm.gravity_scale = 0.0
            pawn.set_actor_enable_collision(False)
            cmd(w, "lyud.Weather.BlendSeconds 0")
            cmd(w, "lyud.Weather.Preset off")
            for cal in unreal.ObjectIterator(unreal.GameCalendar):
                if cal.get_world() == w:
                    cal.set_now(unreal.DateTime(y, mo, d, h, mi, 0, 0))
            cmd(w, "lyud.Weather.Cloud %f" % cloud)
            cmd(w, "lyud.Weather.Rain %f" % rain)
            cmd(w, "lyud.Grade %d" % (1 - st["ab"]))
            st["loc"] = loc
            pawn.set_actor_location(unreal.Vector(*loc), False, True)
            pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))
            st["t0"] = st["t"]
            st["phase"] = 1
        elif st["phase"] == 1:
            pawn.set_actor_location(unreal.Vector(*st["loc"]), False, True)
            # the grade eases with the outdoors vote: give it its seconds (the "off" shot needs less)
            if st["t"] - st["t0"] > (WAIT if st["ab"] == 0 else 4.0):
                name = STEPS[st["i"]][0]
                g = unreal.LyudGradeSubsystem
                gs = None
                for s in unreal.ObjectIterator(g):
                    if s.get_world() == w:
                        gs = s
                log("%s %s | %s" % (name, "on" if st["ab"] == 0 else "off", gs.debug_report() if gs else "NO GRADE"))
                log("  " + unreal.LyudWeatherSubsystem.get_weather(w).debug_report(False))
                cmd(w, "HighResShot 1600x900")
                st["phase"] = 2
                st["t0"] = st["t"]
        elif st["phase"] == 2 and st["t"] - st["t0"] > 2.0:
            st["ab"] = 1 - st["ab"]
            st["phase"] = 0
    except Exception as e:
        log("FAIL exception %s" % e)
        unreal.SystemLibrary.execute_console_command(None, "quit")
        unreal.unregister_slate_post_tick_callback(handle)


handle = unreal.register_slate_post_tick_callback(tick)
