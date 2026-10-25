"""Offscreen play test of the world's sound (ULyudSoundscapeSubsystem, ULyudWeatherSubsystem): puts the player at spots
in the club, at its door, under the alley's trees, on the beach, in a few weathers and hours, records what the game
plays there to WAV (the audio mixer's own output, so the test hears exactly the mix) and prints what the soundscape
heard. Leaves the desktop alone and silent: run it with test_audio.sh (the speakers get au.LinearGainScalarForFinalOutut 0).

Prints AUDIOTEST lines; recordings land in Saved/AudioTest/<step>.wav.
"""
import os

import unreal

FACE_Y = 790.0          # the portal face (scene01_layout.json outside.portal_face_y)
SEA_Z = -4300.0
OUT = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "AudioTest"))


def at(x_m, y_m, z):
    """Map metres (X west of the door, Y out of it) to Unreal cm; z in Unreal cm."""
    return (x_m * 100.0, FACE_Y + y_m * 100.0, z)


# (name, where: start | (x, y, z) | tree, pitch, yaw, hour offset, seconds to record, console commands)
STEPS = [
    ("club_start", "start", 0, 0, 0.0, 14, ["lyud.Music.Gap 0.02", "lyud.Music.Next"]),
    ("door_afternoon", (-193.0, 1150.0, 190.0), -4, 0, 0.0, 14, ["lyud.Music.Next"]),
    ("alley_trees", "tree", 0, 0, 0.0, 12, []),
    ("alley_gale", "tree", 0, 0, 0.0, 12, ["lyud.Wind.Speed 14"]),
    ("brow_sea_view", at(60, -10, 600), -12, 0, 0.0, 12, ["lyud.Wind.Speed -1"]),
    ("beach", at(118, 5, SEA_Z + 180), -4, 0, 0.0, 16, ["lyud.Music.Next"]),
    ("beach_storm", at(118, 5, SEA_Z + 180), -4, 0, 0.0, 16, ["lyud.Wind.Speed 15"]),
    ("beach_calm_dusk", at(118, 5, SEA_Z + 180), -4, 0, 1.5, 16, ["lyud.Wind.Speed 1"]),
    ("door_rain", (-193.0, 1150.0, 190.0), -4, 0, 0.0, 14, ["lyud.Wind.Speed -1", "lyud.Weather.Rain 4"]),
    ("door_thunder", (-193.0, 1150.0, 190.0), -4, 0, 0.0, 22, ["lyud.Weather.Rain 9", "lyud.Music.Next"]),
    ("door_night", (-193.0, 1150.0, 190.0), -4, 0, 6.0, 16, ["lyud.Weather.Rain -1", "lyud.Music.Next"]),
    ("club_rain_outside", "start", 0, 0, 0.0, 12, ["lyud.Weather.Rain 6"]),
]
if os.environ.get("AUDIOTEST_ONLY"):
    STEPS = [s for s in STEPS if s[0] in os.environ["AUDIOTEST_ONLY"].split(",")]
st = {"t": 0.0, "i": -1, "phase": 0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def log(s):
    unreal.log_warning("AUDIOTEST " + s)


def tree_spot(w):
    plants = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.ProceduralPlant)
    best = None
    for p in plants:
        l = p.get_actor_location()
        # an outdoor tree near the door (the alley), not a house plant
        if l.z > -500 and abs(l.x) < 6000 and l.y > 0 and "Poplar" in str(p.get_editor_property("species")):
            if best is None or l.y < best.y:
                best = l
    if best is None and plants:
        best = plants[0].get_actor_location()
    return (best.x + 300.0, best.y, best.z + 170.0) if best else (-193.0, 1150.0, 190.0)


def tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < 6.0:
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    if "start" not in st:
        st["start"] = pawn.get_actor_location()
        os.makedirs(OUT, exist_ok=True)
    if st["phase"] == 0:
        st["i"] += 1
        if st["i"] >= len(STEPS):
            cmd(w, "quit")
            st["phase"] = 9
            return
        name, loc, pitch, yaw, hours, secs, extra = STEPS[st["i"]]
        if loc == "start":
            loc = (st["start"].x, st["start"].y, st["start"].z)
        elif loc == "tree":
            loc = tree_spot(w)
        if getattr(pawn, "character_movement", None):
            pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
            pawn.character_movement.gravity_scale = 0.0
        pawn.set_actor_enable_collision(False)
        cmd(w, "lyud.SkyHourOffset %f" % hours)
        for c in extra:
            cmd(w, c)
        st["loc"] = loc
        pawn.set_actor_location(unreal.Vector(*loc), False, True)
        pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))
        st["t0"] = st["t"]
        st["phase"] = 1
    elif st["phase"] == 1:
        pawn.set_actor_location(unreal.Vector(*st["loc"]), False, True)
        # let the place settle (indoors/outdoors easing, the surf found), then record
        if st["t"] - st["t0"] > 3.0:
            cmd(w, "lyud.Sound.Record")
            # lightning once the recording runs: a far strike, then a near one
            if STEPS[st["i"]][0] == "door_thunder":
                cmd(w, "lyud.Weather.Strike 2500")
                st["near"] = True
            st["phase"] = 2
            st["t0"] = st["t"]
    elif st["phase"] == 2:
        pawn.set_actor_location(unreal.Vector(*st["loc"]), False, True)
        name, secs = STEPS[st["i"]][0], STEPS[st["i"]][5]
        if st.get("near") and st["t"] - st["t0"] > 11.0:
            cmd(w, "lyud.Weather.Strike 400")
            st["near"] = False
        if st["t"] - st["t0"] > secs:
            cmd(w, "lyud.Sound.SaveRecording %s %s" % (name, OUT))
            s = unreal.LyudSoundscapeSubsystem.get_soundscape(w)
            wx = unreal.LyudWeatherSubsystem.get_weather(w)
            log("%s at %s" % (name, st["loc"]))
            log("  " + (s.debug_report() if s else "NO SOUNDSCAPE"))
            log("  " + (wx.debug_report() if wx else "NO WEATHER"))
            st["phase"] = 0


unreal.register_slate_post_tick_callback(tick)
