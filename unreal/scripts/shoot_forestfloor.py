"""Offscreen forest floor check in Lvl_Scene01 (shoot_forestfloor.sh; no PIE, no window): moves the camera into the old
forest on the upland, sets the calendar, logs the floor's state (FLOORTEST lines: instances and triangles per kind, the
mesh lint, the litter at the spots) and takes HighResShots: autumn day at eye height and close up, a birch's leaf carpet,
after rain (wet), rime, drought, then the frame time with the floor on and off (lyud.ForestFloor) in one process.
FLOOR_MODE=winter: forced snow, the same views, a walk for footprints.
"""
import math
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
import terrain as T  # noqa: E402
import world_map as W  # noqa: E402

FACE_Y = float(os.environ.get("FLOOR_FACE_Y", "790"))
RES = os.environ.get("FLOOR_RES", "1600x900")
MODE = os.environ.get("FLOOR_MODE", "autumn")
SPOT = [float(v) for v in os.environ.get("FLOOR_SPOT", "-1150,-380").split(",")]
st = {"t": 0.0, "i": -1, "phase": -1, "done": False}


def log(msg):
    unreal.log_warning("FLOORTEST " + msg)


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name().startswith("Lvl_") and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def ue(x, y, up):
    return W.to_unreal(x, y, T.ground_z(x, y) + up, FACE_Y)


def aim(pawn, pc, a, b):
    eye = 0.0
    cam = pawn.get_component_by_class(unreal.CameraComponent)
    if cam:
        eye = cam.get_world_location().z - pawn.get_actor_location().z
    pawn.set_actor_enable_collision(False)
    pawn.set_actor_location(unreal.Vector(a[0], a[1], a[2] - eye), False, True)
    dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=math.degrees(math.atan2(dz, math.hypot(dx, dy))), yaw=math.degrees(math.atan2(dy, dx))))


X, Y = SPOT
# (name, camera (map m: x, y, height over the ground), target, console commands before the shot)
VIEWS = [
    ("eye", (X, Y, 1.7), (X, Y + 14.0, 0.6), []),
    ("eye_side", (X, Y, 1.7), (X - 12.0, Y + 6.0, 0.4), []),
    ("close", (X + 0.5, Y + 3.0, 0.7), (X + 1.2, Y + 5.2, 0.0), []),
    ("low", (X - 1.0, Y + 6.0, 0.3), (X - 1.0, Y + 12.0, 0.15), []),
    ("leaves", None, None, []),
    ("leaves_close", None, None, []),
]
if MODE == "autumn":
    VIEWS += [
        ("wet", (X, Y, 1.7), (X, Y + 14.0, 0.6), ["lyud.Weather.Preset rain", "lyud.Weather.Wet 1", "lyud.ForestFloor.Wet 1"]),
        ("wet_close", (X + 0.5, Y + 3.0, 0.7), (X + 1.2, Y + 5.2, 0.0), []),
        ("rime", (X + 0.5, Y + 3.0, 0.7), (X + 1.2, Y + 5.2, 0.0),
         ["lyud.Weather.Preset clear", "lyud.Weather.Wet 0", "lyud.ForestFloor.Wet 0", "lyud.ForestFloor.Rime 1"]),
        ("drought", (X, Y, 1.7), (X, Y + 14.0, 0.6), ["lyud.ForestFloor.Rime -1", "lyud.ForestFloor.Dry 1"]),
        ("perf", (X, Y, 1.7), (X, Y + 14.0, 0.6), ["lyud.ForestFloor.Dry -1"]),
        ("off", (X, Y, 1.7), (X, Y + 14.0, 0.6), ["lyud.ForestFloor 0"]),
    ]
else:
    VIEWS += [
        ("deep", (X, Y, 1.7), (X, Y + 14.0, 0.6), ["lyud.Winter.Snow 70"]),
        ("deep_close", (X + 0.5, Y + 3.0, 0.9), (X + 1.2, Y + 5.2, 0.0), []),
        ("walk", None, None, []),
        # the same low view without the floor: what is the floor's and what the snow's own
        ("low_off", (X - 1.0, Y + 6.0, 0.3), (X - 1.0, Y + 12.0, 0.15), ["lyud.ForestFloor 0"]),
    ]
if os.environ.get("FLOOR_SHOTS"):
    VIEWS = [v for v in VIEWS if v[0] in os.environ["FLOOR_SHOTS"].split(",")]


def leafy(floor, w):
    """A spot near the start with a thick leaf carpet (the floor's own litter field says)."""
    best = None
    for dx in range(-60, 61, 6):
        for dy in range(-60, 61, 6):
            u = ue(X + dx, Y + dy, 0.0)
            lit = floor.litter_at(unreal.Vector(*u))
            if best is None or lit.leaves > best[0]:
                best = (lit.leaves, X + dx, Y + dy, lit)
    log("leafiest spot (%.0f, %.0f): leaves %.2f hue %.2f decay %.2f needles %.2f canopy %.2f" %
        (best[1], best[2], best[3].leaves, best[3].hue, best[3].decay, best[3].needles, best[3].canopy))
    return best[1], best[2]


def tick(dt):
    try:
        _tick(dt)
    except Exception as e:  # an exception here repeats every tick and the game never quits
        log("ERROR %s" % e)
        w = world()
        if w:
            cmd(w, "quit")
        unreal.unregister_slate_post_tick_callback(handle)


def _tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < 4.0 or st["done"]:
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    if st["phase"] == -1:
        if pawn and hasattr(pawn, "character_movement"):
            pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
        if not st.get("dated"):
            for cal in unreal.ObjectIterator(unreal.GameCalendar):
                if cal.get_world() == w:
                    cal.set_now(unreal.DateTime(2026, 10, 24, 12, 30, 0, 0) if MODE == "autumn" else unreal.DateTime(2027, 1, 20, 12, 0, 0, 0))
                    st["dated"] = True
            aim(pawn, pc, ue(*VIEWS[0][1]), ue(*VIEWS[0][2]))
            return
        floor = unreal.LyudForestFloorSubsystem.find_floor(w)
        win = unreal.LyudWinterSubsystem.get_winter(w)
        ready = floor and floor.num_trees() > 0 and floor.num_instances() > 0 and (MODE == "autumn" or (win and win.is_ready()))
        if ready and st.get("t_ready") is None:
            st["t_ready"] = st["t"]
        if ready and st["t"] - st["t_ready"] > 20.0:
            st["phase"] = 0
            st["floor"] = floor
            for line in floor.describe_state().splitlines():
                log(line)
            st["leafy"] = leafy(floor, w)
        elif st["t"] > 400.0:
            log("FAIL not ready after 400 s: %s" % (floor.describe_state() if floor else "no floor"))
            cmd(w, "quit")
            st["done"] = True
        return
    if st["phase"] == 0:
        st["i"] += 1
        if st["i"] >= len(VIEWS):
            for line in st["floor"].describe_state().splitlines()[:1]:
                log(line)
            log("done")
            cmd(w, "quit")
            st["done"] = True
            return
        name, a, b, cmds = VIEWS[st["i"]]
        for c in cmds:
            cmd(w, c)
        if name.startswith("leaves"):
            lx, ly = st["leafy"]
            a, b = ((lx - 3.0, ly - 3.0, 1.7), (lx, ly, 0.0)) if name == "leaves" else ((lx - 0.8, ly - 0.8, 0.6), (lx, ly, 0.0))
        if name == "walk":
            st["walk"] = (X + 2.0, Y + 2.0)
            a, b = (X + 2.0, Y + 2.0, 1.7), (X + 2.0, Y + 14.0, 1.0)
        aim(pawn, pc, ue(*a), ue(*b))
        st["view"] = (a, b)
        st["t0"] = st["t"]
        st["phase"] = 1
    elif st["phase"] == 1 and VIEWS[st["i"]][0] in ("perf", "off"):
        # the frame time over 10 s, the floor on ("perf") and off ("off"), same view, one process
        el = st["t"] - st["t0"]
        key = VIEWS[st["i"]][0]
        st.setdefault(key, [])
        if 4.0 < el < 14.0:
            st[key].append(dt)
        elif el >= 14.0:
            if key == "off":
                med = lambda a: 1000.0 * sorted(a)[len(a) // 2] if a else 0.0
                log("perf frame median at the forest spot: floor on %.2f ms, off %.2f ms (%d/%d frames)" %
                    (med(st["perf"]), med(st["off"]), len(st["perf"]), len(st["off"])))
                cmd(w, "lyud.ForestFloor 1")
            st["phase"] = 0
    elif st["phase"] == 1 and VIEWS[st["i"]][0] == "walk" and not st.get("walked"):
        el = st["t"] - st["t0"]
        k = min(1.0, max(0.0, (el - 1.0) / 8.0))
        x, y = st["walk"][0], st["walk"][1] + 10.0 * k
        half = pawn.capsule_component.get_scaled_capsule_half_height() if hasattr(pawn, "capsule_component") else 90.0
        u = ue(x, y, 0.0)
        pawn.set_actor_location(unreal.Vector(u[0], u[1], u[2] + half + 2.0), False, True)
        if el > 9.5:
            st["walked"] = True
            aim(pawn, pc, ue(x + 2.5, y - 16.0, 1.9), ue(x, y - 5.0, 0.0))
            st["t0"] = st["t"]
    elif st["phase"] == 1 and st["t"] - st["t0"] > (8.0 if st["i"] == 0 else 5.0):
        log("shot %s" % VIEWS[st["i"]][0])
        cmd(w, "HighResShot " + RES)
        st["phase"] = 2
        st["t0"] = st["t"]
    elif st["phase"] == 2 and st["t"] - st["t0"] > 1.5:
        st["phase"] = 0


handle = unreal.register_slate_post_tick_callback(tick)
