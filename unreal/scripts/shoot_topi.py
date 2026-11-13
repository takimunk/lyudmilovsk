"""Offscreen screenshots and a walk check of Topi and the river Ural (docs/topi.md); no PIE, no input, the desktop is
left alone. Modelled on shoot_terrain.py: stands the player, flying, at each spot of Lvl_Scene01, waits for Topi's
ground chunks to build and takes a 1600x900 shot. Screenshots land in Saved/Screenshots/Mac* in the order of the
TOPISHOT lines in the log.

  tools/wait_ue_queue.sh && RES=1600x900 LOG=<log> tools/play_offscreen.sh /Game/Scene01/Lvl_Scene01 unreal/scripts/shoot_topi.py
  TOPI_SHOTS=edge_day,river_night runs just those spots; TOPI_MODE=walk runs the walk check instead (pressure by day,
  lyud.Topi.TurnBack, lyud.Topi.Dissolve -> the wake at the computer); TOPI_WAIT=<s> per spot (default 8).

Map frame (Content/UI/map.json): world cm = (X*100, 790 + Y*100, Z*100) for map metres, X west, Y north.
Ground heights below come from Topi's own model (tools/topi/run.sh section x y x y 1): max(sand, map ground).
Yaw 0 looks west (the sea), 90 north, -90 south, 180 east.
"""
import os

import unreal

FACE_Y = 790.0
WAIT = float(os.environ.get("TOPI_WAIT", "11"))   # from arriving: ~3 s over the spot, then ~8 s at the eye
EYE = 1.7

# (name, map x, y, ground z, metres over the ground, pitch, yaw, time "hh:mm", night pin, extra console commands)
DAY_SPOTS = [
    ("edge", -1160.0, 905.0, -15.09, EYE, -2, 180),               # topi_edge: the sand over the ridge, the signs
    ("crest", -1310.0, 900.0, 13.57, EYE, -3, 180),               # the great dune's crest, east to the horizon
    ("dunes", -1440.0, 748.0, -12.63, EYE, -2, 180),              # topi_dunes
    ("bigdune", -1830.0, 955.0, -0.34, EYE, -3, 180),             # the crest of a 240 m dune
    ("air_east", -1150.0, 900.0, -15.09, 150.0, -15, 180),        # from 150 m: across the ridges, the join with the map
    ("air_north", -1500.0, 650.0, -10.0, 150.0, -15, 90),         # from 150 m: along the ridges
]
DAYS = [("jun10", "06-21 10:00"), ("jun18", "06-21 18:00")]
SPOTS = [("%s_%s" % (n, dn), x, y, z, up, p, yaw, when, 0, []) for dn, when in DAYS for n, x, y, z, up, p, yaw in DAY_SPOTS]
SPOTS += [
    ("air_east_jun18_5s", -1150.0, 900.0, -15.09, 150.0, -15, 180, "06-21 18:00", 0, ["@5"]),  # 5 s after air_east_jun18: the heave
    ("edge_air_jun18", -1150.0, 650.0, -11.2, 40.0, -30, 135, "06-21 18:00", 0, []),          # the join with the map's ground
    ("signs_jun18", -1120.0, 905.0, -15.09, EYE, -2, 180, "06-21 18:00", 0, []),             # the warning plates seen from the town side
    ("bank_props_jun18", -2135.0, 1030.0, -29.71, EYE, -12, 160, "06-21 18:00", 0, []),      # the Ural sign and the towels by day (no river)
    ("town_night", -330.0, 900.0, -18.09, EYE, 2, 180, "11-13 23:30", 1, []),                 # the glow over the ridge from the town centre
    ("town_night_high", -330.0, 900.0, -18.09, 30.0, 0, 180, "11-13 23:30", 1, []),
    ("path_night", -1312.0, 888.0, 13.28, EYE, -4, 180, "11-13 23:30", 1, []),               # topi_path: motes and stakes
    ("bank_night", -2150.0, 1038.0, -29.71, EYE, -4, 180, "11-13 23:30", 1, []),             # ural_bank: the river, the sign, the towels
    ("bank_back_night", -2205.0, 1050.0, -29.71, EYE, -6, -20, "11-13 23:30", 1, []),        # from the water's edge back at the sign and towels
    ("river_above_night", -2150.0, 1038.0, -29.71, 40.0, -55, 180, "11-13 23:30", 1, []),    # down at the river, the sign and the towels
    ("dissolve_night", -2150.0, 1038.0, -29.71, EYE, -4, 180, "11-13 23:30", 1, ["lyud.Topi.Dissolve"]),  # 5 s into the dive
]
if os.environ.get("TOPI_SHOTS"):
    SPOTS = [s for s in SPOTS if s[0] in os.environ["TOPI_SHOTS"].split(",")]
MODE = os.environ.get("TOPI_MODE", "shots")
st = {"t": 0.0, "i": -1, "phase": 0, "frames": [], "next_report": 0.0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name().startswith("Lvl_") and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def to_world(x, y, z):
    return unreal.Vector(x * 100.0, FACE_Y + y * 100.0, z * 100.0)


def trace_ground(w, x, y):
    """World Z (m) of whatever blocks a line down from 300 m above the map point, or None."""
    a = to_world(x, y, 300.0)
    b = to_world(x, y, -200.0)
    pawn = unreal.GameplayStatics.get_player_controller(w, 0).get_controlled_pawn()
    hit = unreal.SystemLibrary.line_trace_single(w, a, b, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [pawn],
                                                 unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    t = hit.to_tuple()
    if not t[0]:
        return None
    loc = t[4]
    return loc.z / 100.0, (t[9].get_name() if t[9] else "?")


def set_time(w, when):
    """'hh:mm' today, or 'mm-dd hh:mm' (this year) through the game calendar."""
    if "-" in when:
        md, hm = when.split()
        mo, d = (int(v) for v in md.split("-"))
        h, m = (int(v) for v in hm.split(":"))
        cal = next((c for c in unreal.ObjectIterator(unreal.GameCalendar) if c.get_outer() == w), None) \
            or next(iter(unreal.ObjectIterator(unreal.GameCalendar)), None)
        if cal is None:
            unreal.log_warning("TOPISHOT no GameCalendar: the date stays")
            cmd(w, "lyud.Birds.Time %s %s" % tuple(hm.split(":")))
            return
        now = cal.now()
        cal.set_now(unreal.DateTime(unreal.MathLibrary.get_year(now), mo, d, h, m, 0, 0))
        return
    h, m = when.split(":")
    cmd(w, "lyud.Birds.Time %d %d" % (int(h), int(m)))


def place(w, pawn, pc, x, y, z, pitch, yaw):
    if hasattr(pawn, "character_movement"):
        pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
        pawn.character_movement.velocity = unreal.Vector(0, 0, 0)
    eye = pawn.get_editor_property("base_eye_height") / 100.0 if hasattr(pawn, "base_eye_height") else 0.6
    pawn.set_actor_location(to_world(x, y, z - eye), False, True)
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))


def tick_shots(w, pc, pawn, dt):
    """Per spot: stand 60 m over the model's ground, wait for the chunks under it (and their collision), trace down
    to the real ground, stand at the eye height over it, wait, shoot. Extra commands run TOPI_LEAD (5) seconds before
    the shot ("@n" adds n seconds to the wait)."""
    if st["phase"] == 0:
        st["i"] += 1
        if st["i"] >= len(SPOTS):
            cmd(w, "quit")
            st["phase"] = 9
            return
        name, x, y, gz, up, pitch, yaw, hhmm, night, extra = SPOTS[st["i"]]
        cmd(w, "t.MaxFPS 0")
        cmd(w, "lyud.Topi.Calm 1")
        set_time(w, hhmm)
        cmd(w, "lyud.Topi.Night %d" % night)
        place(w, pawn, pc, x, y, gz + 60.0, pitch, yaw)
        st["wait"] = WAIT + sum(float(e[1:]) for e in extra if e.startswith("@"))
        st["ground"] = None
        st["fired"] = False
        st["t0"] = st["t"]
        st["phase"] = 1
    elif st["phase"] == 1:
        name, x, y, gz, up, pitch, yaw, hhmm, night, extra = SPOTS[st["i"]]
        el = st["t"] - st["t0"]
        if st["ground"] is None:
            place(w, pawn, pc, x, y, gz + 60.0, pitch, yaw)
            if el > 3.0:
                g = trace_ground(w, x, y)
                st["ground"] = g[0] if g else gz
                unreal.log_warning("TOPISHOT %s ground: model %.2f m, trace %s" % (name, gz, g))
            return
        if not st["fired"] or not any(not e.startswith("@") for e in extra):
            place(w, pawn, pc, x, y, st["ground"] + up, pitch, yaw)    # Topi's hold / the mind must not move the camera
        if not st["fired"] and el > st["wait"] - float(os.environ.get("TOPI_LEAD", "5")):
            for e in extra:
                if not e.startswith("@"):
                    cmd(w, e)
            st["fired"] = True
        if el > 5.0:
            st["frames"].append(dt)
        if el > st["wait"]:
            f = sorted(st["frames"]) or [0.0]
            unreal.log_warning("TOPISHOT %s at %s: %d frames, median %.1f ms, worst %.1f ms; ground %.2f m" % (
                name, pawn.get_actor_location(), len(f), 1000 * f[len(f) // 2], 1000 * f[-1], st["ground"]))
            cmd(w, "lyud.Topi.Report")
            st["frames"] = []
            cmd(w, "HighResShot 1600x900")
            st["phase"] = 2
            st["t0"] = st["t"]
    elif st["phase"] == 2 and st["t"] - st["t0"] > 1.5:
        st["phase"] = 0


# walk check: (seconds from start, action)
WALK = [
    (0.0, "day"), (1.0, "stand"), (12.0, "report"), (16.0, "report"), (20.0, "report"),
    (22.0, "lyud.Topi.TurnBack"), (24.0, "report"), (28.0, "report"),
    (30.0, "night"), (31.0, "river"), (40.0, "report"), (41.0, "lyud.Topi.Dissolve"),
    (45.0, "report"), (52.0, "report"), (60.0, "report"), (70.0, "report"), (72.0, "wakeup"), (74.0, "quit"),
]


def tick_walk(w, pc, pawn, dt):
    t = st["t"] - st.setdefault("w0", st["t"])
    while st["i"] + 1 < len(WALK) and WALK[st["i"] + 1][0] <= t:
        st["i"] += 1
        a = WALK[st["i"]][1]
        pawn = pc.get_controlled_pawn()
        if a == "day":
            set_time(w, "14:00")
            cmd(w, "lyud.Topi.Night -1")
            cmd(w, "lyud.Topi.Calm 0")
        elif a == "stand":
            if hasattr(pawn, "character_movement"):
                pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_WALKING)
            pawn.set_actor_location(to_world(-1440.0, 748.0, -12.63 + 1.2), False, True)
            pc.set_control_rotation(unreal.Rotator(roll=0, pitch=0, yaw=180))
        elif a == "night":
            set_time(w, "23:30")
        elif a == "river":
            pawn.set_actor_location(to_world(-2150.0, 1038.0, -29.71 + 1.2), False, True)
            pc.set_control_rotation(unreal.Rotator(roll=0, pitch=0, yaw=180))
        elif a == "report":
            loc = pawn.get_actor_location() if pawn else None
            unreal.log_warning("TOPIWALK t=%.0f pawn %s at %s" % (t, pawn.get_name() if pawn else None, loc))
            cmd(w, "lyud.Topi.Report")
        elif a == "wakeup":
            starts = [s for s in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerStart)
                      if "WakeUp" in s.get_name() or "WakeUp" in str(s.get_editor_property("player_start_tag"))]
            for s in starts:
                d = (pawn.get_actor_location() - s.get_actor_location()).length()
                unreal.log_warning("TOPIWALK wake: %s at %s, pawn %.0f cm from it" % (s.get_name(), s.get_actor_location(), d))
            if not starts:
                unreal.log_warning("TOPIWALK wake: no PlayerStart_WakeUp found")
        elif a == "quit":
            cmd(w, "quit")
        else:
            unreal.log_warning("TOPIWALK t=%.0f %s" % (t, a))
            cmd(w, a)


def tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < 6.0 or st["phase"] == 9:
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    if not pawn:
        return
    if MODE == "walk":
        tick_walk(w, pc, pawn, dt)
    else:
        tick_shots(w, pc, pawn, dt)


unreal.register_slate_post_tick_callback(tick)
