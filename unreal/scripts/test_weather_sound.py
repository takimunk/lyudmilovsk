"""Offscreen play test of the weather's sound (ULyudSoundscapeSubsystem's weather voices, docs/sound.md "Weather"):
plays weather presets at the club's door, under the alley's trees and on the beach, walks the player for the
footsteps, records what the game plays to WAV (the audio mixer's own output) and prints what the soundscape heard.
Leaves the desktop alone and silent: run it with test_weather_sound.sh.

Prints WSOUNDTEST lines; recordings land in Saved/WeatherSound/<step>.wav.
"""
import os

import unreal

FACE_Y = 790.0          # the portal face (scene01_layout.json outside.portal_face_y)
SEA_Z = -4300.0
OUT = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "WeatherSound"))
DOOR = (-193.0, 1150.0, 190.0)


def at(x_m, y_m, z):
    """Map metres (X west of the door, Y out of it) to Unreal cm; z in Unreal cm."""
    return (x_m * 100.0, FACE_Y + y_m * 100.0, z)


BEACH = at(118, 5, SEA_Z + 180)
# (name, where: door | tree | beach | (x, y, z), walk, hour offset, seconds, console commands)
STEPS = [
    ("beach_drizzle", "beach", False, 0.0, 14, ["lyud.Weather.Preset drizzle"]),
    ("alley_rain", "tree", False, 0.0, 12, ["lyud.Weather.Preset rain"]),
    ("alley_rain_umbrella", "tree", False, 0.0, 12, ["lyud.Sound.Umbrella 1"]),
    ("door_downpour_hail", "door", False, 0.0, 14, ["lyud.Sound.Umbrella -1", "lyud.Weather.Preset downpour", "lyud.Sound.Force Hail=0.8"]),
    ("door_storm", "door", False, 0.0, 30, ["lyud.Sound.Force", "lyud.Weather.Preset thunder"]),
    ("alley_after_rain", "tree", False, 0.0, 12, ["lyud.Weather.Preset overcast", "lyud.Sound.Force Drip=1"]),
    ("door_walk_puddles", "door", True, 0.0, 12, ["lyud.Sound.Force Puddles=1"]),
    ("beach_seafog", "beach", False, 0.0, 70, ["lyud.Sound.Force", "lyud.Weather.Preset seafog"]),
    ("door_blizzard", "door", False, 0.0, 14, ["lyud.Weather.Preset blizzard"]),
    ("door_walk_snow", "door", True, 0.0, 12, ["lyud.Weather.Preset snow"]),
    ("door_walk_frost", "door", True, 3.0, 12, ["lyud.Weather.Preset frost"]),
    ("beach_sea_ice_night", "beach", False, 6.0, 40, ["lyud.Sound.Force IceRate=-1.5"]),
    ("door_freezing_rain_twigs", "tree", False, 0.0, 14, ["lyud.Weather.Preset freezingrain", "lyud.Sound.Force Glaze=1", "lyud.Wind.Speed 8"]),
    ("door_thaw", "door", False, 0.0, 14, ["lyud.Weather.Preset off", "lyud.Wind.Speed -1", "lyud.Weather.Temp 3", "lyud.Weather.SnowDepth 15", "lyud.Sound.Force Melt=1"]),
]
if os.environ.get("WSOUNDTEST_ONLY"):
    STEPS = [s for s in STEPS if s[0] in os.environ["WSOUNDTEST_ONLY"].split(",")]
st = {"t": 0.0, "i": -1, "phase": 0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def log(s):
    unreal.log_warning("WSOUNDTEST " + s)


def tree_spot(w):
    plants = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.ProceduralPlant)
    best = None
    for p in plants:
        l = p.get_actor_location()
        if l.z > -500 and abs(l.x) < 6000 and l.y > 0 and "Poplar" in str(p.get_editor_property("species")):
            if best is None or l.y < best.y:
                best = l
    if best is None and plants:
        best = plants[0].get_actor_location()
    return (best.x + 300.0, best.y, best.z + 170.0) if best else DOOR


def tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < 6.0:
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    move = getattr(pawn, "character_movement", None)
    if "start" not in st:
        st["start"] = pawn.get_actor_location()
        os.makedirs(OUT, exist_ok=True)
        # the weather alone, no music cues over it
        cmd(w, "lyud.Music 0")
    walk = st["i"] >= 0 and STEPS[st["i"]][2]
    if st["phase"] == 0:
        st["i"] += 1
        if st["i"] >= len(STEPS):
            cmd(w, "quit")
            st["phase"] = 9
            return
        name, loc, walk, hours, secs, extra = STEPS[st["i"]]
        loc = {"door": DOOR, "beach": BEACH}.get(loc, loc)
        if loc == "tree":
            loc = tree_spot(w)
        if move:
            if walk:
                move.set_movement_mode(unreal.MovementMode.MOVE_WALKING)
                move.gravity_scale = 1.0
            else:
                move.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
                move.gravity_scale = 0.0
        pawn.set_actor_enable_collision(walk)
        cmd(w, "lyud.SkyHourOffset %f" % hours)
        for c in extra:
            cmd(w, c)
        st["loc"] = loc
        pawn.set_actor_location(unreal.Vector(*loc), False, True)
        pc.set_control_rotation(unreal.Rotator(roll=0, pitch=-4, yaw=90))
        st["t0"] = st["t"]
        st["phase"] = 1
    elif st["phase"] == 1:
        if not walk:
            pawn.set_actor_location(unreal.Vector(*st["loc"]), False, True)
        # let the place and the weather settle (indoors/outdoors, the surf, the smoothed voices), then record
        if st["t"] - st["t0"] > 5.0:
            cmd(w, "lyud.Sound.Record")
            st["phase"] = 2
            st["t0"] = st["t"]
            st["walked"] = 0.0
            st["from"] = pawn.get_actor_location()
    elif st["phase"] == 2:
        name, secs = STEPS[st["i"]][0], STEPS[st["i"]][4]
        if name == "door_storm":
            # besides the storm's own strikes: one far off, then one close
            for when, dist in ((1.0, 7000), (14.0, 700)):
                if st["t"] - st["t0"] > when and not st.get("struck%d" % dist):
                    st["struck%d" % dist] = True
                    cmd(w, "lyud.Weather.Strike %d" % dist)
        if walk:
            # out of the door and back: two seconds each way
            back = int((st["t"] - st["t0"]) / 3.0) % 2 == 1
            pawn.add_movement_input(unreal.Vector(0, -1 if back else 1, 0), 1.0, False)
        else:
            pawn.set_actor_location(unreal.Vector(*st["loc"]), False, True)
        if st["t"] - st["t0"] > secs:
            cmd(w, "lyud.Sound.SaveRecording %s %s" % (name, OUT))
            s = unreal.LyudSoundscapeSubsystem.get_soundscape(w)
            wx = unreal.LyudWeatherSubsystem.get_weather(w)
            log("%s at %s walk %s moved %.0f cm" % (name, st["loc"], walk, (pawn.get_actor_location() - st["from"]).length()))
            log("  " + (s.debug_report() if s else "NO SOUNDSCAPE"))
            winter = unreal.LyudWinterSubsystem.get_winter(w)
            if winter:
                feet = pawn.get_actor_location()
                log("  underfoot snow %.1f cm ice walkable %s on ground %s" % (winter.get_snow_depth_at(feet), winter.is_ice_walkable(feet),
                                                                          move.is_moving_on_ground() if move else None))
            log("  " + (wx.debug_report(False) if wx else "NO WEATHER"))
            st["phase"] = 0


unreal.register_slate_post_tick_callback(tick)
