"""Offscreen look at the Big Sun day (docs/festivals.md): no window, no PIE.

  unreal/scripts/shoot_sun_day.sh   (runs this in Lvl_Scene01 -game -RenderOffscreen through the UE queue)

The zenith on Sun Square (crowd, sun discs, flags, the bigger sun), a lane at the foot of W2 with the pennant strings,
the promenade arcade's swags, then the seaside phase with the kites over the beach, then poplar fluff by the park
avenue and a ground check of the town's planted poplars. Prints SUNDAYSHOT lines; screenshots are named sunday_*.
SUNDAY_ONLY=zenith,seaside,fluff limits the parts; SUNDAY_CMDS="a;b" adds console commands at the start.
"""
import json
import math
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
CONTENT = os.path.join(HERE, "..", "Lyudmilovsk", "Content")
RES = os.environ.get("SUNDAY_RES", "1600x900")
ONLY = [s for s in os.environ.get("SUNDAY_ONLY", "zenith,seaside,fluff").split(",") if s]

PLAN = json.load(open(os.path.join(CONTENT, "City", "city_plan.json")))
POPLARS = json.load(open(os.path.join(CONTENT, "City", "poplars.json")))["trees"]


def spot(i):
    for s in PLAN["spots"]:
        if s["id"] == i:
            return s["ue"]


def lamp(i):
    for l in PLAN["lamps"]:
        if l["id"] == i:
            return l


SQUARE = spot("S-SQUARE")
BEACH = spot("S-BEACH")
LAMP = lamp("LAMP-W2-10")
LAMP2 = lamp("LAMP-W2-11")

st = {"t": 0.0, "i": 0, "t0": 0.0, "wait": 0.0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def log(msg):
    unreal.log_warning("SUNDAYSHOT " + msg)


def place(w, eye, target):
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    if hasattr(pawn, "character_movement"):
        pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
    pawn.set_actor_enable_collision(False)
    off = 0.0
    cam = pawn.get_component_by_class(unreal.CameraComponent)
    if cam:
        off = cam.get_world_location().z - pawn.get_actor_location().z
    pawn.set_actor_location(unreal.Vector(eye[0], eye[1], eye[2] - off), False, True)
    dx, dy, dz = target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=math.degrees(math.atan2(dz, math.hypot(dx, dy))),
                                           yaw=math.degrees(math.atan2(dy, dx))))


def shot(w, name):
    cmd(w, "HighResShot %s filename=sunday_%s" % (RES, name))
    log("shot %s" % name)


def sun_dir(w):
    """Towards the sun: the brightest directional light of the level (ALyudSky's sun, not its moon)."""
    best = None
    for c in unreal.ObjectIterator(unreal.DirectionalLightComponent):
        try:
            if c.get_world() != w or c.get_name().startswith("Default__"):
                continue
            i = c.get_editor_property("intensity")
        except Exception:  # noqa: BLE001
            continue
        if best is None or i > best[0]:
            best = (i, c)
    if best:
        f = best[1].get_forward_vector()
        return (-f.x, -f.y, -f.z)
    return (0.0, -0.6, 0.8)


def kite_points(w):
    pts = []
    cls = getattr(unreal, "LyudKites", None)
    if not cls:
        return pts
    for a in unreal.ObjectIterator(cls):
        if a.get_world() != w or a.get_name().startswith("Default__"):
            continue
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            if c.is_visible() and "Kite" in str(c.static_mesh.get_name() if c.static_mesh else ""):
                l = c.get_world_location()
                pts.append((l.x, l.y, l.z))
    return pts


def ground(w, x, y, z):
    """What is under (x, y) from 30 m above z: (hit z, actor name) or None."""
    hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(x, y, z + 3000.0), unreal.Vector(x, y, z - 3000.0),
                                                  unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                  unreal.DrawDebugTrace.NONE, True)
    if not hit:
        return None
    t = hit.to_tuple()
    blocking, loc, actor = t[0], t[4], t[9]
    if not blocking:
        return None
    return (loc.z, actor.get_name() if actor else "?")


STEPS = []


def step(fn, wait=1.0):
    STEPS.append((fn, wait))


def s_start(w):
    for c in ["lyud.Prefetch 0", "ghost", "r.MotionBlurQuality 0", "t.MaxFPS 0"] + \
            [c for c in os.environ.get("SUNDAY_CMDS", "").split(";") if c]:
        cmd(w, c)
    place(w, (SQUARE[0] + 3500.0, SQUARE[1] - 2500.0, SQUARE[2] + 1200.0), (SQUARE[0], SQUARE[1], SQUARE[2] + 200.0))
    log("start")


step(s_start, 4.0)

if "zenith" in ONLY:
    step(lambda w: cmd(w, "lyud.Festival.Go sun_day zenith"), 40.0)
    step(lambda w: (cmd(w, "lyud.Festival.Report"), log("sun dir %s" % (sun_dir(w),))), 1.0)

    def s_sq_wide(w):
        place(w, (SQUARE[0] + 3500.0, SQUARE[1] - 2500.0, SQUARE[2] + 1200.0), (SQUARE[0], SQUARE[1], SQUARE[2] + 200.0))
    step(s_sq_wide, 3.0)
    step(lambda w: shot(w, "square_wide"), 2.0)

    def s_sq_crowd(w):
        # behind the crowd at eye height, looking the way they look (at the sun), a little up
        d = sun_dir(w)
        n = math.hypot(d[0], d[1]) or 1.0
        hx, hy = d[0] / n, d[1] / n
        e = (SQUARE[0] - hx * 1500.0, SQUARE[1] - hy * 1500.0, SQUARE[2] + 170.0)
        place(w, e, (e[0] + hx * 1000.0, e[1] + hy * 1000.0, e[2] + 420.0))
    step(s_sq_crowd, 3.0)
    step(lambda w: shot(w, "square_crowd"), 2.0)

    def s_sq_faces(w):
        # from the sun's side, the faces turned up
        d = sun_dir(w)
        n = math.hypot(d[0], d[1]) or 1.0
        hx, hy = d[0] / n, d[1] / n
        e = (SQUARE[0] + hx * 1800.0 + 350.0, SQUARE[1] + hy * 1800.0 + 250.0, SQUARE[2] + 300.0)
        place(w, e, (SQUARE[0], SQUARE[1], SQUARE[2] + 150.0))
    step(s_sq_faces, 3.0)
    step(lambda w: shot(w, "square_faces"), 2.0)

    def s_sky(w):
        # off the square's centre (its fountain carries a sun of its own), towards the sea
        d = sun_dir(w)
        e = (SQUARE[0] + 2200.0, SQUARE[1] + 600.0, SQUARE[2] + 200.0)
        place(w, e, (e[0] + d[0] * 1000.0, e[1] + d[1] * 1000.0, e[2] + d[2] * 1000.0 - 150.0))
    step(s_sky, 3.0)
    step(lambda w: shot(w, "sky_sun"), 2.0)

    def s_lane(w):
        a, b = LAMP["head_ue"], LAMP2["head_ue"]
        # down the lane from before lamp 10, at eye height on the terrace below W2
        e = (a[0] + 250.0, a[1] - 2200.0, LAMP["foot_ue"][2] + 170.0)
        place(w, e, ((a[0] + b[0]) * 0.5, (a[1] + b[1]) * 0.5, a[2] - 150.0))
    step(s_lane, 4.0)
    step(lambda w: shot(w, "lane_w2"), 2.0)

    def s_lane_side(w):
        a, b = LAMP["head_ue"], LAMP2["head_ue"]
        m = ((a[0] + b[0]) * 0.5, (a[1] + b[1]) * 0.5, a[2])
        e = (m[0] + 1600.0, m[1] - 300.0, LAMP["foot_ue"][2] + 200.0)
        place(w, e, (m[0] - 300.0, m[1], m[2] - 100.0))
    step(s_lane_side, 3.0)
    step(lambda w: shot(w, "lane_w2_side"), 2.0)

    def s_arcade(w):
        # the promenade below W1 near the square: swags along the arcade's rail
        e = (12500.0, 86000.0, -3950.0 + 170.0)
        place(w, e, (9300.0, 90800.0, -3550.0))
    step(s_arcade, 3.0)
    step(lambda w: shot(w, "arcade"), 2.0)

def bunting_hidden(w, hide):
    n = 0
    cls = getattr(unreal, "LyudStreetBunting", None)
    for a in (unreal.ObjectIterator(cls) if cls else []):
        if a.get_world() == w and not a.get_name().startswith("Default__"):
            a.set_actor_hidden_in_game(hide)
            n += 1
    return n


def perf_at(name, view):
    """Mean frame time at a view with the street bunting shown and hidden, A/B/A/B, 6 s each after 3 s to settle."""
    for k, hide in enumerate([False, True, False, True, False, True]):
        def _set(w, hide=hide, k=k):
            view(w)
            n = bunting_hidden(w, hide)
            st["perf"] = None
            if k == 0:
                log("perf %s: %d bunting actors" % (name, n))
        step(_set, 3.0)

        def _go(w):
            st["perf"] = [0.0, 0]
        step(_go, 6.0)

        def _read(w, hide=hide):
            s, n = st["perf"] or (0.0, 0)
            st["perf"] = None
            st.setdefault("perfres", {}).setdefault((name, hide), []).append(s / max(n, 1) * 1000.0)
            log("perf %s bunting %s: %.2f ms over %d frames" % (name, "hidden" if hide else "shown", s / max(n, 1) * 1000.0, n))
        step(_read, 0.0)

    def _sum(w):
        a = st["perfres"].get((name, False), [])
        b = st["perfres"].get((name, True), [])
        if a and b:
            log("perf %s: shown %.2f ms, hidden %.2f ms, bunting costs %+.2f ms (runs %s / %s)" % (
                name, sum(a) / len(a), sum(b) / len(b), sum(a) / len(a) - sum(b) / len(b),
                " ".join("%.2f" % x for x in a), " ".join("%.2f" % x for x in b)))
        bunting_hidden(w, False)
    step(_sum, 0.0)


if "perf" in ONLY:
    if "zenith" not in ONLY:
        step(lambda w: cmd(w, "lyud.Festival.Go sun_day zenith"), 40.0)
    perf_at("square", lambda w: s_sq_crowd(w) if "s_sq_crowd" in globals() else place(
        w, (SQUARE[0] - 1500.0, SQUARE[1], SQUARE[2] + 170.0), (SQUARE[0], SQUARE[1], SQUARE[2] + 300.0)))
    perf_at("lane_w2", lambda w: s_lane(w) if "s_lane" in globals() else place(
        w, (LAMP["head_ue"][0] + 250.0, LAMP["head_ue"][1] - 2200.0, LAMP["foot_ue"][2] + 170.0),
        (LAMP["head_ue"][0], LAMP["head_ue"][1] + 1400.0, LAMP["head_ue"][2] - 150.0)))

if "zenith" in ONLY:
    # the crowd a few minutes into the zenith (people walk there from all over town)
    def s_late(w):
        cmd(w, "lyud.Festival.Report")
        s_sq_wide(w)
    step(s_late, 3.0)
    step(lambda w: shot(w, "square_wide_late"), 2.0)
    step(lambda w: s_sq_crowd(w), 3.0)
    step(lambda w: shot(w, "square_crowd_late"), 2.0)

if "crowd" in ONLY:
    # the square some minutes into the zenith, once the town has walked there
    def s_crowd_go(w):
        cmd(w, "lyud.Festival.Go sun_day zenith")
        place(w, (SQUARE[0] + 3500.0, SQUARE[1] - 2500.0, SQUARE[2] + 1200.0), (SQUARE[0], SQUARE[1], SQUARE[2] + 200.0))
    step(s_crowd_go, float(os.environ.get("SUNDAY_CROWD_WAIT", "240")))
    step(lambda w: cmd(w, "lyud.Festival.Report"), 1.0)
    step(lambda w: shot(w, "crowd_wide"), 2.0)

    def s_crowd_low(w):
        e = (SQUARE[0] + 2600.0, SQUARE[1] + 900.0, SQUARE[2] + 260.0)
        place(w, e, (SQUARE[0], SQUARE[1], SQUARE[2] + 250.0))
    step(s_crowd_low, 3.0)
    step(lambda w: shot(w, "crowd_low"), 2.0)

if "seaside" in ONLY:
    def s_sea(w):
        cmd(w, "lyud.Festival.Go sun_day seaside")
        place(w, (BEACH[0] - 2500.0, BEACH[1] - 1500.0, BEACH[2] + 600.0), (BEACH[0] + 2000.0, BEACH[1], BEACH[2] + 1500.0))
    step(s_sea, 35.0)

    def s_kites(w):
        cmd(w, "lyud.Festival.Report")
        k = kite_points(w)
        log("kites up %d: %s" % (len(k), ", ".join("(%.0f %.0f %.0f)" % p for p in k[:12])))
        if k:
            cx = sum(p[0] for p in k) / len(k)
            cy = sum(p[1] for p in k) / len(k)
            cz = sum(p[2] for p in k) / len(k)
            # from behind the fliers, the beach in front and the kites over it
            dx, dy = cx - BEACH[0], cy - BEACH[1]
            n = math.hypot(dx, dy) or 1.0
            e = (BEACH[0] - dx / n * 2500.0, BEACH[1] - dy / n * 2500.0, BEACH[2] + 350.0)
            st["kc"] = (cx, cy, cz)
            place(w, e, (cx, cy, (cz + BEACH[2]) * 0.5))
    step(s_kites, 3.0)
    step(lambda w: shot(w, "beach_kites"), 2.0)

    def s_kite_close(w):
        k = kite_points(w)
        if k:
            p = min(k, key=lambda q: q[2])
            place(w, (p[0] - 900.0, p[1] - 900.0, p[2] - 200.0), p)
    step(s_kite_close, 2.0)
    step(lambda w: shot(w, "kite_close"), 2.0)

if "fluff" in ONLY:
    def s_fluff(w):
        cmd(w, "lyud.Fluff.Amount 1")
        t = POPLARS[0]["at"]
        place(w, (t[0] + 1800.0, t[1] - 1500.0, t[2] + 170.0), (t[0], t[1] + 800.0, t[2] + 400.0))
    step(s_fluff, 15.0)
    step(lambda w: cmd(w, "lyud.Fluff.Report"), 1.0)
    step(lambda w: shot(w, "fluff_avenue"), 2.0)

    def s_fluff_close(w):
        t = POPLARS[0]["at"]
        place(w, (t[0] + 600.0, t[1] - 600.0, t[2] + 160.0), (t[0] + 900.0, t[1] + 200.0, t[2] + 220.0))
    step(s_fluff_close, 3.0)
    step(lambda w: shot(w, "fluff_close"), 2.0)

    def s_poplars(w):
        bad = 0
        for i, t in enumerate(POPLARS):
            a = t["at"]
            g = ground(w, a[0], a[1], a[2])
            if not g:
                log("poplar %d at (%.0f %.0f %.0f): nothing under it" % (i, a[0], a[1], a[2]))
                bad += 1
                continue
            dz = g[0] - a[2]
            ok = abs(dz) < 40.0
            if not ok:
                bad += 1
            log("poplar %d at (%.0f %.0f %.0f): ground %.0f (%+.0f) on %s%s" % (i, a[0], a[1], a[2], g[0], dz, g[1],
                                                                               "" if ok else "  <-- OFF"))
        log("poplars checked %d, off %d" % (len(POPLARS), bad))
        # a look at the planted row from the side
        xs = [t["at"][0] for t in POPLARS]
        ys = [t["at"][1] for t in POPLARS]
        c = (sum(xs) / len(xs), sum(ys) / len(ys), POPLARS[0]["at"][2])
        place(w, (c[0] + 4500.0, c[1] - 3000.0, c[2] + 1500.0), (c[0], c[1], c[2] + 600.0))
    step(s_poplars, 6.0)
    step(lambda w: shot(w, "poplars_row"), 2.0)
    step(lambda w: cmd(w, "lyud.Fluff.Report"), 1.0)


def s_end(w):
    log("done")
    cmd(w, "quit")


step(s_end, 1.0)


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
    if st.get("perf") is not None:
        st["perf"][0] += dt
        st["perf"][1] += 1
    w = world()
    if not w or st["t"] < 8.0:
        return
    if st["t"] - st["t0"] < st["wait"] or st["i"] >= len(STEPS):
        return
    fn, wait = STEPS[st["i"]]
    st["i"] += 1
    try:
        fn(w)
    except Exception as e:  # noqa: BLE001
        log("step %d failed: %s" % (st["i"], e))
    st["t0"] = st["t"]
    st["wait"] = wait


unreal.register_slate_post_tick_callback(tick)
