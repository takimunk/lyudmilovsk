"""Offscreen check of the starlings, the late swallows and the Academgorodok's wires (docs/bird_life.md). No PIE, no window:
unreal/scripts/test_starlings.sh runs it in a -game -RenderOffscreen session of Lvl_Scene01.

Prints STARTEST lines (the bird life's report at each step, which birds were shot) and takes screenshots in this order:
the wires close up and along the street at noon, starlings feeding by day (close and wide), swallows in a row on the
wires before dusk, the sunset murmuration from the beach (three moments, then the descent), starlings asleep in the
reed. test_starlings.sh copies them to Saved/shots/ with names.

The reedbed behind the beach (world_map REEDBED) is put up here if the level was built before it existed.
"""
import json
import math
import os
import re

import unreal

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
st = {"t": 0.0, "i": 0, "t0": 0.0, "wait": 0.0}
STEPS = []


def step(fn, wait=3.0):
    STEPS.append((fn, wait))
    return fn


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def birds(w):
    return unreal.BirdLifeSubsystem.get_bird_life(w)


def log(msg):
    unreal.log_warning("STARTEST " + msg)


def shot(w, label):
    log("SHOT " + label)
    cmd(w, "HighResShot 1600x900")


def report(w):
    return birds(w).report()


def sun(w):
    m = re.search(r"sun ([0-9.]+)-([0-9.]+)", report(w))
    return (float(m.group(1)), float(m.group(2))) if m else (7.4, 17.3)


def set_time(w, hours):
    h = int(hours) % 24
    cmd(w, "lyud.Birds.Time %d %d" % (h, int(round((hours - int(hours)) * 60)) % 60))


def bird(w, i):
    r = birds(w).get_bird(i)
    if isinstance(r, tuple):
        return r[1] if r[0] else None
    return r if r and r.id >= 0 else None


def act(b):
    return str(b.activity).split(".")[-1].split(":")[0].upper()


def find(w, at, radius, species, acts, above=None):
    for i in birds(w).find_birds_near(unreal.Vector(*at), radius):
        b = bird(w, i)
        if b and str(b.species_id) == species and act(b) in acts:
            if above is None or b.location.z > above:
                return i, b
    return None, None


def look(w, eye, target):
    """The camera (the ghost pawn's eyes) at eye, looking at target (cm)."""
    eye = unreal.Vector(*eye) if not isinstance(eye, unreal.Vector) else eye
    target = unreal.Vector(*target) if not isinstance(target, unreal.Vector) else target
    rot = unreal.MathLibrary.find_look_at_rotation(eye, target)
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    pawn.set_actor_location(eye, False, True)
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=rot.pitch, yaw=rot.yaw))
    st["eye"] = (eye, rot)


def fix_eye(w):
    if not st.get("eye"):
        return
    eye, rot = st["eye"]
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    cam = pc.player_camera_manager.get_camera_location()
    pawn = pc.get_controlled_pawn()
    pawn.set_actor_location(pawn.get_actor_location() + (eye - cam), False, True)
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=rot.pitch, yaw=rot.yaw))


def ground(w, x, y, z0=20000.0):
    hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(x, y, z0), unreal.Vector(x, y, -8000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else -3700.0


with open(os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "BirdsData", "wires.json")) as f:
    WIRES = json.load(f)
FACE_Y = WIRES["face_y_cm"]
REED = (25.0, 280.0, 60.0, 45.0)                 # tools/world_map.py REEDBED (map metres)
REED_UE = (REED[0] * 100.0, FACE_Y + REED[1] * 100.0)
SHORE_X = 137.5                                    # world_map.shore_x(280), metres


@step
def s_start(w):
    cmd(w, "ghost")
    cmd(w, "lyud.Birds.IgnorePlayer 1")
    # the reedbed, if this level was built before it (import_world.py places it from world_map.grass_patches)
    have = any(str(a.get_editor_property("species")) == "CommonReed" and abs(a.get_actor_location().x - REED_UE[0]) < 500
               for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralGrass")))
    if not have:
        cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralGrass")
        xf = unreal.Transform(unreal.Vector(REED_UE[0], REED_UE[1], ground(w, *REED_UE)), unreal.Rotator(0, 0, 0), unreal.Vector(1, 1, 1))
        g = unreal.GameplayStatics.begin_deferred_actor_spawn_from_class(w, cls, xf)
        for k, v in (("species", "CommonReed"), ("seed", 731), ("half_extent", unreal.Vector2D(REED[2] * 100, REED[3] * 100)),
                     ("density_scale", 2.5), ("min_world_z", -10000.0), ("max_world_z", -3500.0), ("trace_up", 5000.0), ("trace_down", 6000.0)):
            g.set_editor_property(k, v)
        unreal.GameplayStatics.finish_spawning_actor(g, xf)
        log("reedbed put up at %.0f %.0f" % REED_UE)
    log("start " + report(w))


# --- the wires at noon -----------------------------------------------------------------------------------
@step
def s_noon(w):
    set_time(w, 12.5)


step(lambda w: None, 6.0)


@step
def s_wire_close(w):
    p = WIRES["poles"][3]
    x, y, z = [c * 100.0 for c in p["at"]]
    a = math.radians(p["along"])
    # 5 m off the line, at the crossarm's height minus 1.5 m, looking up at the insulators
    nx, ny = -math.sin(a), math.cos(a)
    look(w, (x + nx * 450 + math.cos(a) * 150, y + ny * 450 + math.sin(a) * 150, z + 700), (x, y, z + 830))


step(lambda w: fix_eye(w), 1.5)
step(lambda w: shot(w, "wires_insulators_close"), 2.0)


@step
def s_wire_street(w):
    p, q = WIRES["poles"][1], WIRES["poles"][5]
    x, y, z = [c * 100.0 for c in p["at"]]
    qx, qy, qz = [c * 100.0 for c in q["at"]]
    a = math.radians(p["along"])
    nx, ny = -math.sin(a), math.cos(a)
    look(w, (x + nx * 900 - (qx - x) * 0.15, y + ny * 900 - (qy - y) * 0.15, z + 180), (qx, qy, qz + 600))


step(lambda w: fix_eye(w), 1.5)
step(lambda w: shot(w, "wires_along_street"), 2.0)


@step
def s_wire_side(w):
    p = WIRES["poles"][9]
    x, y, z = [c * 100.0 for c in p["at"]]
    look(w, (x - 2500, y - 1800, z + 900), (x, y, z + 500))


step(lambda w: fix_eye(w), 1.5)
step(lambda w: shot(w, "wires_side_houses"), 2.0)


# --- starlings feeding by day ------------------------------------------------------------------------------
@step
def s_day(w):
    set_time(w, 10.5)


step(lambda w: None, 20.0)


@step
def s_feeders(w):
    log("10:30 " + report(w))
    i, b = None, None
    for c in ((-33000.0, 90790.0, -3700.0), (REED_UE[0], REED_UE[1], -3700.0), (-5000.0, 30000.0, 0.0)):
        i, b = find(w, c, 150000.0, "Starling", ("PECK", "FORAGE"))
        if b:
            break
    st["feed"] = i
    if b:
        l = b.location
        log("feeding starling %d at %.0f %.0f %.0f" % (i, l.x, l.y, l.z))
        look(w, (l.x - 700, l.y - 300, l.z + 160), (l.x, l.y, l.z + 10))
    else:
        log("no starling feeding")


step(lambda w: fix_eye(w), 1.0)
step(lambda w: shot(w, "starlings_feeding_close") if st.get("feed") is not None else None, 2.0)


@step
def s_feeders_wide(w):
    b = bird(w, st["feed"]) if st.get("feed") is not None else None
    if b:
        l = b.location
        look(w, (l.x - 3000, l.y - 1500, l.z + 1200), (l.x, l.y, l.z))


step(lambda w: fix_eye(w), 1.0)
step(lambda w: shot(w, "starlings_feeding_wide") if st.get("feed") is not None else None, 2.0)


@step
def s_on_wire(w):
    i, b = find(w, (2500.0, 60000.0, -3000.0), 300000.0, "Starling", ("PERCH", "PREEN"), above=-3400.0)
    if b:
        l = b.location
        log("perched starling %d at %.0f %.0f %.0f" % (i, l.x, l.y, l.z))
        look(w, (l.x - 600, l.y - 400, l.z - 150), (l.x, l.y, l.z))
        st["perch"] = i


step(lambda w: fix_eye(w), 1.0)
step(lambda w: shot(w, "starlings_perched_day") if st.get("perch") is not None else None, 2.0)


# --- swallows on the wires before dusk ---------------------------------------------------------------------
@step
def s_swallows(w):
    rise, sset = sun(w)
    st["sun"] = (rise, sset)
    set_time(w, sset - 1.0)


step(lambda w: None, 25.0)


@step
def s_swallows_find(w):
    log("swallows' gathering " + report(w))
    poles = WIRES["poles"]
    cx = sum(p["at"][0] for p in poles) / len(poles) * 100.0
    cy = sum(p["at"][1] for p in poles) / len(poles) * 100.0
    i, b = find(w, (cx, cy, -2800.0), 400000.0, "BarnSwallow", ("PERCH", "PREEN", "WATCH"), above=-3500.0)
    st["swallow"] = i
    if b:
        l = b.location
        log("swallow %d at %.0f %.0f %.0f" % (i, l.x, l.y, l.z))
        look(w, (l.x - 220, l.y - 180, l.z - 40), (l.x, l.y, l.z + 3))
    else:
        log("no swallow perched")


step(lambda w: fix_eye(w), 1.0)
step(lambda w: shot(w, "swallows_on_wire_close") if st.get("swallow") is not None else None, 2.0)


@step
def s_swallows_wide(w):
    b = bird(w, st["swallow"]) if st.get("swallow") is not None else None
    if b:
        l = b.location
        look(w, (l.x - 900, l.y - 700, l.z - 300), (l.x, l.y, l.z))


step(lambda w: fix_eye(w), 1.0)
step(lambda w: shot(w, "swallows_on_wire_row") if st.get("swallow") is not None else None, 2.0)


# --- the sunset murmuration from the beach -----------------------------------------------------------------
@step
def s_murm(w):
    rise, sset = st.get("sun", sun(w))
    set_time(w, sset - 0.3)


step(lambda w: None, 45.0)


def murm_centre(w):
    m = re.search(r"murmuration: (\d+) birds.*?centre (-?[0-9.]+) (-?[0-9.]+) (-?[0-9.]+)", report(w))
    return (int(m.group(1)), float(m.group(2)) * 100, float(m.group(3)) * 100, float(m.group(4)) * 100) if m else None


for _k in range(3):
    def _m(w, k=_k):
        c = murm_centre(w)
        log("murmuration %d: %s" % (k, report(w)))
        if c:
            n, x, y, z = c
            # on the beach at the waterline, 1.7 m up, looking inland at the flock
            ex, ey = (SHORE_X - 8.0) * 100.0, y + (k - 1) * 4000.0
            look(w, (ex, ey, ground(w, ex, ey) + 170.0), (x, y, z))
    step(_m, 1.0)
    step(lambda w: fix_eye(w), 0.5)
    step(lambda w, k=_k: shot(w, "murmuration_from_beach_%d" % k), 12.0)


@step
def s_murm_down(w):
    rise, sset = st.get("sun", sun(w))
    set_time(w, sset + 0.15)


step(lambda w: None, 30.0)


@step
def s_murm_down_shot(w):
    c = murm_centre(w)
    log("descent " + report(w))
    tx, ty = REED_UE
    ex, ey = (SHORE_X - 8.0) * 100.0, ty
    look(w, (ex, ey, ground(w, ex, ey) + 170.0), (c[1], c[2], c[3]) if c else (tx, ty, -2000.0))


step(lambda w: fix_eye(w), 0.5)
step(lambda w: shot(w, "murmuration_descent"), 2.0)


# --- asleep in the reed -------------------------------------------------------------------------------------
@step
def s_sleep(w):
    rise, sset = st.get("sun", sun(w))
    set_time(w, sset + 0.8)


step(lambda w: None, 15.0)


@step
def s_sleep_find(w):
    log("night " + report(w))
    i, b = find(w, (REED_UE[0], REED_UE[1], -3600.0), 12000.0, "Starling", ("SLEEP",))
    st["sleeper"] = i
    if b:
        l = b.location
        log("asleep: starling %d at %.0f %.0f %.0f" % (i, l.x, l.y, l.z))
        look(w, (l.x - 260, l.y - 200, l.z + 60), (l.x, l.y, l.z))


step(lambda w: fix_eye(w), 1.0)
step(lambda w: shot(w, "starlings_asleep_in_reed_dusk") if st.get("sleeper") is not None else None, 2.0)


@step
def s_sleep_lit(w):
    # the same place with the sky moved to day, to see them (the birds keep their night)
    cmd(w, "lyud.SkyHourOffset -10")


step(lambda w: fix_eye(w), 4.0)
step(lambda w: shot(w, "starlings_asleep_in_reed_lit") if st.get("sleeper") is not None else None, 2.0)


@step
def s_sleep_wide(w):
    b = bird(w, st["sleeper"]) if st.get("sleeper") is not None else None
    if b:
        l = b.location
        look(w, (l.x - 1400, l.y - 900, l.z + 500), (l.x, l.y, l.z))


step(lambda w: fix_eye(w), 1.0)
step(lambda w: shot(w, "reedbed_roost_lit"), 2.0)


@step
def s_quit(w):
    cmd(w, "lyud.SkyHourOffset 0")
    log("done")
    cmd(w, "quit")


def tick(dt):
    try:
        _tick(dt)
    except Exception as e:  # noqa: BLE001
        if not st.get("err"):
            st["err"] = 1
            log("tick failed: %s" % e)
            cmd(world(), "quit")


def _tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < 8.0:
        return
    if not birds(w) or not birds(w).is_running():
        if st["t"] > 90.0 and st["i"] == 0:
            log("bird life did not start")
            cmd(w, "quit")
        return
    if st["t"] - st["t0"] < st["wait"] or st["i"] >= len(STEPS):
        return
    fn, wait = STEPS[st["i"]]
    st["i"] += 1
    try:
        fn(w)
    except Exception as e:  # noqa: BLE001
        log("step %s failed: %s" % (getattr(fn, "__name__", "?"), e))
    st["t0"] = st["t"]
    st["wait"] = wait


unreal.register_slate_post_tick_callback(tick)
