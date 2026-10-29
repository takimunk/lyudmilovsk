"""Offscreen check of the starlings, the late swallows and the Academgorodok's wires (docs/bird_life.md). No PIE, no window:
unreal/scripts/test_starlings.sh runs it in a -game -RenderOffscreen session of Lvl_Scene01.

Prints STARTEST lines (the bird life's report at each step, which birds were shot) and takes screenshots in this order:
the wires close up and along the street at noon, starlings feeding by day (close and wide), swallows in a row on the
wires before dusk, the sunset murmuration from the beach (three moments, then the descent), starlings asleep in the
reed. test_starlings.sh copies them to Saved/shots/ with names.

The reedbed behind the beach (world_map REEDBED) comes with the level's build (import_world.py, grass patches).
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
    have = any(str(a.get_editor_property("species")) == "CommonReed" and abs(a.get_actor_location().x - REED_UE[0]) < 500
               for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralGrass")))
    log("reedbed in the level: %s (the scene build places it from world_map.grass_patches)" % have)
    log("start " + report(w))


# --- the wires at noon -----------------------------------------------------------------------------------
@step
def s_noon(w):
    set_time(w, 12.5)


step(lambda w: None, 6.0)


@step
def s_wire_close(w):
    cmd(w, "fov 70")
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
        cmd(w, "fov 50")
        look(w, (l.x - 260, l.y - 120, l.z + 70), (l.x, l.y, l.z + 8))
    else:
        log("no starling feeding")


step(lambda w: fix_eye(w), 1.0)
step(lambda w: shot(w, "starlings_feeding_close") if st.get("feed") is not None else None, 2.0)


@step
def s_feeders_wide(w):
    b = bird(w, st["feed"]) if st.get("feed") is not None else None
    if b:
        l = b.location
        cmd(w, "fov 70")
        look(w, (l.x - 2200, l.y - 1100, l.z + 700), (l.x, l.y, l.z))


step(lambda w: fix_eye(w), 1.0)
step(lambda w: shot(w, "starlings_feeding_wide") if st.get("feed") is not None else None, 2.0)


@step
def s_on_wire(w):
    i, b = find(w, (2500.0, 60000.0, -3000.0), 300000.0, "Starling", ("PERCH", "PREEN"), above=-3400.0)
    if b:
        l = b.location
        log("perched starling %d at %.0f %.0f %.0f" % (i, l.x, l.y, l.z))
        look(w, (l.x - 500, l.y - 300, l.z - 150), (l.x, l.y, l.z))
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
        # across the nearest wire from it, a little below, a narrow lens
        def dist(wr):
            a, b = wr["a"], wr["b"]
            return min(math.hypot(l.x - (a[0] + (b[0] - a[0]) * t) * 100, l.y - (a[1] + (b[1] - a[1]) * t) * 100) for t in (0.1 * k for k in range(11)))
        wr = min(WIRES["wires"], key=dist)
        dx, dy = wr["b"][0] - wr["a"][0], wr["b"][1] - wr["a"][1]
        n = math.hypot(dx, dy) or 1.0
        st["across"] = (-dy / n, dx / n)
        ax, ay = st["across"]
        cmd(w, "fov 40")
        look(w, (l.x + ax * 160, l.y + ay * 160, l.z - 35), (l.x, l.y, l.z + 6))
    else:
        log("no swallow perched")


step(lambda w: fix_eye(w), 1.0)
step(lambda w: shot(w, "swallows_on_wire_close") if st.get("swallow") is not None else None, 2.0)


@step
def s_swallows_wide(w):
    b = bird(w, st["swallow"]) if st.get("swallow") is not None else None
    if b:
        l = b.location
        ax, ay = st.get("across", (1.0, 0.0))
        cmd(w, "fov 55")
        look(w, (l.x + ax * 700, l.y + ay * 700, l.z - 250), (l.x, l.y, l.z))


step(lambda w: fix_eye(w), 1.0)
step(lambda w: shot(w, "swallows_on_wire_row") if st.get("swallow") is not None else None, 2.0)


# --- the sunset murmuration from the beach -----------------------------------------------------------------
@step
def s_murm(w):
    rise, sset = st.get("sun", sun(w))
    set_time(w, sset - float(os.environ.get("STARTEST_MURM_H", "0.2")))


step(lambda w: None, 45.0)


def murm_centre(w):
    m = re.search(r"murmuration: (\d+) birds.*?centre (-?[0-9.]+) (-?[0-9.]+) (-?[0-9.]+)", report(w))
    return (int(m.group(1)), float(m.group(2)) * 100, float(m.group(3)) * 100, float(m.group(4)) * 100) if m else None


for _k, _where in enumerate(("beach", "beach", "valley", "valley", "near", "near")):
    def _m(w, k=_k, where=_where):
        c = murm_centre(w)
        log("murmuration %d: %s" % (k, report(w)))
        if c:
            n, x, y, z = c
            if where == "near":
                # 90 m from it on the land side, low, looking up at it over the shore
                ex, ey = x - 9000.0, y + (k - 4.5) * 6000.0
                ez = ground(w, ex, ey) + 200.0
            elif where == "beach":
                # on the beach at the waterline, 200 m along the coast, looking at the flock over the shore
                ex, ey = (SHORE_X - 6.0) * 100.0, y + (20000.0 if k == 0 else -20000.0)
                ez = ground(w, ex, ey) + 170.0
            else:
                # inland, on the valley floor 250 m east, looking west at it against the sunset over the sea
                ex, ey = x - 25000.0, y + (k - 2.5) * 8000.0
                ez = ground(w, ex, ey) + 300.0
            cmd(w, "fov 60")
            look(w, (ex, ey, ez), (x, y, z))
    step(_m, 1.0)
    step(lambda w: fix_eye(w), 0.5)
    step(lambda w, k=_k, where=_where: shot(w, "murmuration_from_%s_%d" % (where, k)), 10.0)


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
        cmd(w, "fov 50")
        look(w, (l.x - 170, l.y - 120, l.z + 40), (l.x, l.y, l.z))


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
        cmd(w, "fov 70")
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


# --- popping: birds near the camera must not blink in and out (Dima, 29 Oct) ---------------------------------
# STARTEST_ONLY=pop: at the swallows' wires before dusk, by a starling flock feeding by day and under the swallows hawking
# at noon, the camera walks past them for a while with lyud.Birds.Steady 0 (the old hard cuts) and then 1, and the
# renderer's counters (pops: a bird of half a pixel or more drawn one frame and not the next, or back; body spawns) are
# compared. A few frames of each are shot.
POP = {"rows": []}


def counters(w):
    m = re.search(r"pops (\d+), body spawns (\d+) releases (\d+)", report(w))
    return tuple(int(g) for g in m.groups()) if m else (0, 0, 0)


def walker(w, a, b, secs):
    """The camera walks from a to b (cm) in secs, looking at the side of its path (+90 degrees)."""
    t0 = st["t"]
    def move(w):
        k = min(1.0, (st["t"] - t0) / secs)
        eye = tuple(a[i] + (b[i] - a[i]) * k for i in range(3))
        tgt = POP.get("look") or POP["ab"][1]
        look(w, eye, tgt)
    return move


def pop_phase(where, steady, a, b, target, secs=14.0):
    def place(w):
        # the camera jumps to the walk's start and stands there a moment: a jump pops birds, rightly, and isn't counted
        cmd(w, "lyud.Birds.Steady %d" % steady)
        POP["look"] = target
        POP["ab"] = (a(w), b(w)) if callable(a) else (a, b)
        st["move"] = walker(w, POP["ab"][0], POP["ab"][0], 1.0)
    def start(w):
        POP["c0"] = counters(w)
        POP["tt"] = st["t"]
        st["move"] = walker(w, POP["ab"][0], POP["ab"][1], secs)
    step(place, 2.0)
    def stop(w):
        st["move"] = None
        c = counters(w)
        d = [c[i] - POP["c0"][i] for i in range(3)]
        secs_ = max(0.1, st["t"] - POP["tt"])
        POP["rows"].append((where, steady, d[0], d[1], secs_))
        log("POP %s steady=%d pops %d spawns %d releases %d over %.1f s (%s)" % (where, steady, d[0], d[1], d[2], secs_, report(w)[-1200:]))
    step(start, secs * 0.5)
    step(lambda w, s_=steady, wh=where: shot(w, "pop_%s_steady%d_a" % (wh, s_)), 0.05)
    step(lambda w, s_=steady, wh=where: shot(w, "pop_%s_steady%d_b" % (wh, s_)), 0.05)
    step(lambda w, s_=steady, wh=where: shot(w, "pop_%s_steady%d_c" % (wh, s_)), secs * 0.5)
    step(stop, 0.5)


def _pop_steps():
    @step
    def p_wires_time(w):
        rise, sset = sun(w)
        st["sun"] = (rise, sset)
        set_time(w, sset - 1.0)
    step(lambda w: None, 25.0)

    @step
    def p_wires_find(w):
        log("swallows' gathering " + report(w))
        poles = WIRES["poles"]
        cx = sum(p["at"][0] for p in poles) / len(poles) * 100.0
        cy = sum(p["at"][1] for p in poles) / len(poles) * 100.0
        i, b = find(w, (cx, cy, -2800.0), 400000.0, "BarnSwallow", ("PERCH", "PREEN", "WATCH"), above=-3500.0)
        POP["sw"] = (b.location.x, b.location.y, b.location.z) if b else None
        log("pop: swallow %s at %s" % (i, POP["sw"]))
    for steady in (0, 1):
        # a walk along under the row, 8 m off it, 30 m past the swallow
        pop_phase("wires_dusk", steady, lambda w: (POP["sw"][0] - 1500, POP["sw"][1] - 800, POP["sw"][2] - 300) if POP["sw"] else (0, 0, 0),
                  lambda w: (POP["sw"][0] + 1500, POP["sw"][1] - 800, POP["sw"][2] - 300) if POP["sw"] else (0, 0, 0), None)

    @step
    def p_feed_time(w):
        set_time(w, 10.5)
    step(lambda w: None, 20.0)

    @step
    def p_feed_find(w):
        for c in ((-33000.0, 90790.0, -3700.0), (REED_UE[0], REED_UE[1], -3700.0), (-5000.0, 30000.0, 0.0)):
            i, b = find(w, c, 150000.0, "Starling", ("PECK", "FORAGE"))
            if b:
                break
        POP["st"] = (b.location.x, b.location.y, b.location.z) if b else None
        log("pop: feeding starling %s at %s" % (i, POP["st"]))
    for steady in (0, 1):
        pop_phase("flock_day", steady, lambda w: (POP["st"][0] - 2500, POP["st"][1] - 1200, POP["st"][2] + 170) if POP["st"] else (0, 0, 0),
                  lambda w: (POP["st"][0] + 2500, POP["st"][1] - 1200, POP["st"][2] + 170) if POP["st"] else (0, 0, 0), None)

    @step
    def p_noon(w):
        set_time(w, 12.5)
    step(lambda w: None, 15.0)
    for steady in (0, 1):
        p = WIRES["poles"][4]
        x, y, z = [c * 100.0 for c in p["at"]]
        pop_phase("wires_noon", steady, (x - 3000, y - 2000, z + 100), (x + 3000, y - 2000, z + 100), (x, y + 3000, z + 4000))

    @step
    def p_verdict(w):
        rows = POP["rows"]
        log("POP table: " + "; ".join("%s steady=%d pops %d spawns %d in %.0f s" % r for r in rows))
        ok = True
        for where in sorted(set(r[0] for r in rows)):
            old = [r for r in rows if r[0] == where and r[1] == 0]
            new = [r for r in rows if r[0] == where and r[1] == 1]
            if not old or not new:
                continue
            o, n = old[0], new[0]
            good = n[2] <= max(3, o[2] // 20) and n[3] <= max(o[3], 12)
            ok = ok and good
            log("POP %s: pops %d -> %d, body spawns/s %.2f -> %.2f %s" % (where, o[2], n[2], o[3] / o[4], n[3] / n[4], "ok" if good else "FAIL"))
        log("POPCHECK " + ("ok" if ok else "FAIL"))


if os.environ.get("STARTEST_ONLY") == "pop":
    STEPS[:] = STEPS[:1]
    _pop_steps()
    step(s_quit, 0.0)


if os.environ.get("STARTEST_ONLY") == "murm":
    _keep = ("s_start", "s_murm", "_m", "<lambda>", "s_quit")
    _i0 = [k for k, (fn, _) in enumerate(STEPS) if fn.__name__ == "s_murm"][0]
    _i1 = [k for k, (fn, _) in enumerate(STEPS) if fn.__name__ == "s_murm_down"][0]
    STEPS[:] = STEPS[:1] + STEPS[_i0:_i1] + [s for s in STEPS if s[0].__name__ == "s_quit"]


def _tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < 8.0:
        return
    if st.get("move"):
        st["move"](w)
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
