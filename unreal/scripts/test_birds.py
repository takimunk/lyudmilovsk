"""Offscreen play test of the bird life (UBirdLifeSubsystem, docs/bird_life.md). Leaves the desktop alone (no PIE):

  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen -ResX=1600 -ResY=900 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py <this file>"

(unreal/scripts/test_birds.sh runs it.) Prints BIRDTEST lines: the report at several hours of the day, the flush when
the player runs at birds, screenshots of the birds from 0.5 to 100 m and of their places (Saved/Screenshots/Mac, in
the order of the steps), then a whole simulated day to Saved/BirdSim/day.csv for tools/birdsim/render_birds.py.
"""
import os

import unreal

st = {"t": 0.0, "i": 0, "t0": 0.0, "wait": 0.0, "focus": None}
SHOTS = os.environ.get("BIRDTEST_SHOTS", "1") == "1"


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def birds(w):
    return unreal.BirdLifeSubsystem.get_bird_life(w)


def log(msg):
    unreal.log_warning("BIRDTEST " + msg)


def set_time(w, h, m=0):
    cmd(w, "lyud.Birds.Time %d %d" % (h, m))


def place(w, loc, pitch, yaw):
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    pawn.set_actor_location(unreal.Vector(*loc), False, True)
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))


def bird(w, i):
    r = birds(w).get_bird(i)
    if isinstance(r, tuple):
        return r[1] if r[0] else None
    return r if r and r.id >= 0 else None


def nearest(w, at, radius, perched=True):
    for i in birds(w).find_birds_near(unreal.Vector(*at), radius):
        b = bird(w, i)
        if b and (not perched or b.activity in (unreal.BirdActivity.PERCH, unreal.BirdActivity.PECK, unreal.BirdActivity.PREEN,
                                                unreal.BirdActivity.FORAGE, unreal.BirdActivity.SWIM)):
            return i, b
    return None, None


def look_at(w, target, dist, up=40.0, side=0.0):
    """Puts the camera (the pawn's eyes, ghosting) dist cm from target, a little above, looking at it."""
    t = target
    eye = unreal.Vector(t.x - dist, t.y + side, t.z + up)
    d = t - eye
    pitch = unreal.MathLibrary.find_look_at_rotation(eye, t).pitch
    yaw = unreal.MathLibrary.find_look_at_rotation(eye, t).yaw
    place(w, (eye.x, eye.y, eye.z - 60.0), pitch, yaw)
    st["eye"] = (eye, pitch, yaw)


def fix_eye(w):
    """Moves the pawn so the camera is exactly where look_at wanted it (the eye height differs per pawn)."""
    if not st.get("eye"):
        return
    eye, pitch, yaw = st["eye"]
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    cam = pc.player_camera_manager.get_camera_location()
    pawn = pc.get_controlled_pawn()
    pawn.set_actor_location(pawn.get_actor_location() + (eye - cam), False, True)
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))


DOOR = (-193.0, 1150.0, 190.0)
STEPS = []


def step(fn, wait=3.0):
    STEPS.append((fn, wait))
    return fn


@step
def s_start(w):
    cmd(w, "ghost")
    set_time(w, 11, 0)
    place(w, DOOR, -4, 90)
    log("start " + birds(w).report())


@step
def s_door_flock(w):
    # the pigeons that always feed on the lawn just out of the club's door
    near = []
    for i in birds(w).find_birds_near(unreal.Vector(-193.0, 2150.0, 0.0), 800.0):
        b = bird(w, i)
        if b and str(b.species_id) == "FeralPigeon":
            near.append("%d:%s" % (i, str(b.activity).split(".")[-1].split(":")[0]))
    log("door flock: %d pigeons within 8 m of the lawn: %s" % (len(near), " ".join(near)))
    place(w, (-193.0, 1350.0, 190.0), -14, 90)
    st["eye"] = None


@step
def s_door_shot(w):
    if SHOTS:
        cmd(w, "HighResShot 1600x900")


@step
def s_quick_quit(w):
    if os.environ.get("BIRDTEST_QUICK"):
        log("done")
        cmd(w, "quit")


@step
def s_spawn(w):
    cmd(w, "lyud.Birds.IgnorePlayer 1")
    # obvious birds right in front: a gull 4 m ahead on the path, a rook 7 m, a sparrow 2.5 m
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    p = pc.get_controlled_pawn().get_actor_location()
    for sp, d, side in (("HerringGull", 400, -60), ("Rook", 700, 80), ("HouseSparrow", 250, 30)):
        z = p.z - 180.0
        hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(p.x + side, p.y + d, p.z + 200), unreal.Vector(p.x + side, p.y + d, p.z - 2000),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
        if hit:
            z = hit.to_tuple()[4].z
        i = birds(w).spawn_bird(sp, unreal.Vector(p.x + side, p.y + d, z))
        st.setdefault("spawned", {})[sp] = i
        log("spawned %s %d at %.0f %.0f %.0f" % (sp, i, p.x + side, p.y + d, z))


@step
def s_spawn_shot(w):
    log("spawned shot " + birds(w).report())
    if SHOTS:
        cmd(w, "HighResShot 1600x900")


@step
def s_report_noon(w):
    log("noon " + birds(w).report())
    if SHOTS:
        cmd(w, "HighResShot 1600x900")


@step
def s_close(w):
    cmd(w, "lyud.Birds.IgnorePlayer 1")
    st["focus"] = st.get("spawned", {}).get("HouseSparrow")
    st["focus2"] = st.get("spawned", {}).get("HerringGull")


for _who in ("focus", "focus2"):
    for _d in (50.0, 200.0, 500.0, 1500.0, 4000.0, 10000.0):
        def _shot(w, d=_d, who=_who):
            b = bird(w, st[who]) if st.get(who) is not None else None
            if b:
                look_at(w, b.location, d, up=max(15.0, min(d * 0.3, 400.0)))
                log("lod %d cm: bird %d %s %s" % (d, st[who], b.species_id, b.activity))
        step(_shot, 1.5)

        def _snap(w, d=_d, who=_who):
            if SHOTS and st.get(who) is not None:
                fix_eye(w)
                cmd(w, "HighResShot 1600x900")
        step(_snap, 1.5)


@step
def s_close_end(w):
    cmd(w, "lyud.Birds.IgnorePlayer 0")


@step
def s_run(w):
    # the player runs (scripted, 5.5 m/s) from 25 m straight at the nearest bird on the ground near the door
    i, b = None, None
    for j in birds(w).find_birds_near(unreal.Vector(*DOOR), 30000.0):
        c = bird(w, j)
        if c and c.activity in (unreal.BirdActivity.PECK, unreal.BirdActivity.FORAGE) and c.alertness < 0.1:
            i, b = j, c
            break
    if b:
        st["run"] = i
        loc = b.location
        st["run_to"] = unreal.Vector(loc.x, loc.y, loc.z)
        cmd(w, "lyud.Birds.TestRun %.0f %.0f %.0f %.0f %.0f 550 30" % (loc.x - 2500.0, loc.y, loc.z, loc.x, loc.y))
        log("run at bird %d %s from 25 m" % (i, b.species_id))


@step
def s_run2(w):
    b = bird(w, st["run"]) if st.get("run") is not None else None
    if b:
        log("3 s after the run started: bird %d act %s alert %.2f" % (st["run"], b.activity, b.alertness))


def _run_check(w, label):
    b = bird(w, st["run"]) if st.get("run") is not None else None
    if b:
        d = (b.location - st["run_to"]).length() / 100.0
        log("%s: bird %d act %s alert %.2f, %.1f m from where the player stands" % (label, st["run"], b.activity, b.alertness, d))


step(lambda w: _run_check(w, "8 s after"), 12.0)
step(lambda w: _run_check(w, "20 s after"), 10.0)
step(lambda w: _run_check(w, "30 s after"), 3.0)


@step
def s_scare(w):
    log("after scare " + birds(w).report())
    cmd(w, "ghost")


for _h, _view in ((7, "dawn"), (13, "midday"), (17, "dusk"), (22, "night")):
    def _hour(w, h=_h):
        set_time(w, h, 30 if h == 17 else 0)
    step(_hour, 20.0)

    def _rep(w, h=_h, name=_view):
        log("%s %02d " % (name, h) + birds(w).report())
        place(w, (-30000.0, 120000.0, 3500.0), -25, 160)   # over the valley, toward the town and the beach
        if SHOTS:
            cmd(w, "HighResShot 1600x900")
    step(_rep, 3.0)


@step
def s_day(w):
    set_time(w, 4, 0)
    birds(w)  # warm
    cmd(w, "lyud.Birds.DumpDay 4 24")


@step
def s_quit(w):
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
        if st["t"] > 60.0 and st["i"] == 0:
            log("bird life did not start")
            cmd(w, "quit")
        return
    if st["t"] - st["t0"] < st["wait"]:
        return
    if st["i"] >= len(STEPS):
        return
    fn, wait = STEPS[st["i"]]
    st["i"] += 1
    try:
        fn(w)
    except Exception as e:  # noqa: BLE001
        log("step %s failed: %s" % (fn.__name__, e))
    st["t0"] = st["t"]
    st["wait"] = wait


unreal.register_slate_post_tick_callback(tick)
