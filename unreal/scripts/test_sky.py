"""Offscreen play test of the sky (ALyudSky): stands the player indoors and outdoors, checks that the pawn gets the
sun's lighting channel only outside, and screenshots the sky at several hours (lyud.SkyHourOffset).
Leaves the desktop alone (no PIE, no input):

  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen -ResX=1600 -ResY=900 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py <this file>"

Prints SKYTEST lines; screenshots land in Saved/Screenshots/Mac in the order of STEPS.
"""
import os

import unreal

# (name, location, pitch, yaw, hour offset); the outdoor spots are on the path out of the door
STEPS = [
    ("indoor_desk", (-17.5, -1585.0, 190.0), -5, 90, 0.0),
    ("door_sea", (-193.0, 1150.0, 190.0), -4, 0, 0.0),
    ("door_sun", (-193.0, 1150.0, 190.0), 6, -50, 0.0),
    ("door_body", (-193.0, 1150.0, 190.0), -70, 0, 0.0),
    ("sunset_1700", (-193.0, 1150.0, 190.0), 2, -60, 1.0),
    ("dusk_1740", (-193.0, 1150.0, 190.0), 2, -70, 1.67),
    ("night_2100", (-193.0, 1150.0, 190.0), 10, 90, 5.0),
    ("noon_next", (-193.0, 1150.0, 190.0), -4, 0, 20.0),
    ("indoor_after", (-17.5, -1585.0, 190.0), -5, 90, 0.0),
]
# SKYTEST_ONLY=indoor_desk,door_sea runs just those steps (e.g. for a baseline shot of a level without the sky)
if os.environ.get("SKYTEST_ONLY"):
    STEPS = [s for s in STEPS if s[0] in os.environ["SKYTEST_ONLY"].split(",")]
WAIT = 5.0
st = {"t": 0.0, "i": -1, "phase": 0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def report(w, name):
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    sky = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.LyudSky) if hasattr(unreal, "LyudSky") else []
    chans = ["%s:%d%d" % (c.get_name(), c.get_editor_property("lighting_channels").channel0,
                         c.get_editor_property("lighting_channels").channel1)
             for c in pawn.get_components_by_class(unreal.PrimitiveComponent)]
    s = sky[0] if sky else None
    if s:
        unreal.log_warning("SKYTEST " + s.get_debug_report())
    unreal.log_warning("SKYTEST %s outdoors=%s sun_alt=%.2f sun_az=%.2f pawn=%s channels=%s" % (
        name, s.is_player_outdoors() if s else None, s.sun_altitude() if s else 0, s.sun_azimuth() if s else 0,
        pawn.get_actor_location(), " ".join(chans)))


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
        name, loc, pitch, yaw, hours = STEPS[st["i"]]
        cmd(w, "lyud.SkyHourOffset %f" % hours)
        pawn.set_actor_location(unreal.Vector(*loc), False, True)
        pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))
        st["t0"] = st["t"]
        st["phase"] = 1
    elif st["phase"] == 1 and st["t"] - st["t0"] > WAIT:
        name = STEPS[st["i"]][0]
        report(w, name)
        cmd(w, "HighResShot 1600x900")
        st["phase"] = 2
        st["t0"] = st["t"]
    elif st["phase"] == 2 and st["t"] - st["t0"] > 1.0:
        st["phase"] = 0


unreal.register_slate_post_tick_callback(tick)
