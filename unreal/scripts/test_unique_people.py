"""Offscreen check that everybody in the town exists once (docs/people.md, 'One person, one body'), in Lvl_Scene01:

  unreal/scripts/test_unique_people.sh

At 03:00, 10:00, 15:00 and 21:00 (and once more after a save and a load) the camera visits the club and the places of a
few people far apart; at each spot lyud.People.Unique counts every visible human model in the world by whose model it is
(the life's bodies, the club's placed actors, the booth's owner, anything else with a person's model). Anybody seen
twice fails the test. Prints UNIQUETEST lines: where each named person is at each hour (state, activity, place, the model
that shows them), who is awake at 03:00, and a shot of Nika (the streamer) at her place at 15:00.
"""
import math

import unreal

st = {"t": 0.0, "i": 0, "t0": 0.0, "wait": 0.0, "dupes": 0, "checks": 0, "awake": []}
HOURS = [(3, 0), (10, 0), (15, 0), (21, 0)]
# people whose places are far apart (the club, the Academgorodok, the diner, the cheese tent, the lab, the sea)
SPOTS = ["streamer", "frolova_masha", "orlova_tanya", "zhukova_lena", "khuazheva_susanna", "zotov_lev", "lange_pyotr"]
CLUB = ((-110.0, 60.0, 175.0), (60.0, -700.0, 80.0))


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def life(w):
    return unreal.PeopleLifeSubsystem.get_people_life(w)


def log(msg):
    unreal.log_warning("UNIQUETEST " + msg)


def person(w, key):
    l = life(w)
    r = l.get_person(l.find_person(key))
    if isinstance(r, tuple):
        return r[1] if r[0] else None
    return r


def everybody(w):
    l = life(w)
    out = []
    for i in range(l.num_people()):
        r = l.get_person(i)
        p = r[1] if isinstance(r, tuple) and r[0] else (None if isinstance(r, tuple) else r)
        if p:
            out.append(p)
    return out


def look(w, eye, target):
    e, t = unreal.Vector(*eye), unreal.Vector(*target)
    r = unreal.MathLibrary.find_look_at_rotation(e, t)
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    pawn.set_actor_location(e, False, True)
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=r.pitch, yaw=r.yaw))
    st["eye"] = e


def fix_eye(w):
    """The camera exactly at the eye asked for (a frame after the move: the camera manager lags a frame)."""
    if not st.get("eye"):
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    cam = pc.player_camera_manager.get_camera_location()
    pawn.set_actor_location(pawn.get_actor_location() + (st["eye"] - cam), False, True)


def look_at_person(w, key, back=260.0, up=120.0, side=0.5):
    p = person(w, key)
    if not p:
        return None
    t = p.location
    yaw = math.radians(p.yaw)
    ex = t.x + math.cos(yaw) * back + math.sin(yaw) * back * side
    ey = t.y + math.sin(yaw) * back - math.cos(yaw) * back * side
    look(w, (ex, ey, t.z + up), (t.x, t.y, t.z + 90.0))
    return p


def check(w, tag):
    cam = unreal.GameplayStatics.get_player_controller(w, 0).player_camera_manager.get_camera_location()
    r = life(w).instance_report()
    text, twice = (r[0], r[1]) if isinstance(r, tuple) else (r, 0)
    st["checks"] += 1
    st["dupes"] += twice
    log("%s %s (camera %.0f %.0f %.0f) %s" % ("FAIL" if twice else "ok", tag, cam.x, cam.y, cam.z, text))


def where(w, tag):
    for p in everybody(w):
        log("where %s %s: %s %s at %s pose %s model %s home %s actor %s" % (
            tag, p.key, p.state, p.activity, p.place, str(p.pose).split(".")[-1], p.mesh, p.home,
            p.actor.get_name() if p.actor else "-"))


STEPS = []


def step(fn, wait=2.0):
    STEPS.append((fn, wait))
    return fn


@step
def s_start(w):
    cmd(w, "ghost")
    cmd(w, "r.MotionBlurQuality 0")
    # everybody outdoors gets a body at once, wherever the camera is: the strictest count
    cmd(w, "lyud.People.DrawDistance 100000")
    l = life(w)
    log("start running=%s people=%d" % (l.is_running(), l.num_people()))
    for p in everybody(w):
        log("model %s: %s (owner of it: %s)" % (p.key, l.shown_model(p.id) or "NONE (lives unseen)",
                                               l.model_owner(l.shown_model(p.id))))


def visit_all(tag):
    step(lambda w: look(w, *CLUB), 0.5)
    step(fix_eye, 4.0)
    step(lambda w: check(w, "%s club" % tag), 0.5)
    for k in SPOTS:
        step(lambda w, k=k: look_at_person(w, k, 600.0, 250.0, 0.6), 0.5)
        step(fix_eye, 4.0)
        step(lambda w, k=k: check(w, "%s near %s" % (tag, k)), 0.5)


for h, m in HOURS:
    tag = "%02d:%02d" % (h, m)
    step(lambda w, h=h, m=m: cmd(w, "lyud.Birds.Time %d %d" % (h, m)), 6.0)
    step(lambda w, tag=tag: log("report %s %s" % (tag, life(w).report())), 0.5)
    step(lambda w, tag=tag: where(w, tag), 0.5)
    visit_all(tag)
    if h == 3:
        @step
        def s_night(w):
            awake = ["%s (%s %s)" % (p.key, p.activity, p.place) for p in everybody(w)
                     if p.home and p.pose != unreal.PersonPose.SLEEP]
            st["awake"] = awake
            log("night 03:00 awake: %d %s" % (len(awake), ", ".join(awake)))
    if h == 15:
        step(lambda w: look_at_person(w, "streamer", 230.0, 110.0, 0.5), 0.5)
        step(fix_eye, 5.0)
        step(lambda w: look_at_person(w, "streamer", 230.0, 110.0, 0.5), 0.5)
        step(fix_eye, 2.0)

        @step
        def s_nika(w):
            p = person(w, "streamer")
            log("nika at 15:00: %s %s at %s pose %s actor %s" % (p.state, p.activity, p.place, p.pose,
                                                                   p.actor.get_name() if p.actor else "-"))
            cmd(w, "HighResShot 1600x900")
        STEPS[-1] = (STEPS[-1][0], 4.0)

# a save and a load (the level opens again): still one of everybody
step(lambda w: cmd(w, "lyud.Save unique_people_test"), 4.0)
step(lambda w: cmd(w, "lyud.Load 0"), 25.0)
step(lambda w: cmd(w, "lyud.People.DrawDistance 100000") or log("after load people=%d" % life(w).num_people()), 6.0)
visit_all("after-load")


@step
def s_end(w):
    ok = st["dupes"] == 0 and st["checks"] > 0 and not st.get("errors")
    log("result %s: %d checks, %d people seen twice, %d failed steps, %d awake at 03:00" % (
        "PASS" if ok else "FAIL", st["checks"], st["dupes"], st.get("errors", 0), len(st["awake"])))
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
    if not w or st["t"] < 10.0:
        return
    if st["t"] - st["t0"] < st["wait"] or st["i"] >= len(STEPS):
        return
    fn, wait = STEPS[st["i"]]
    st["i"] += 1
    try:
        fn(w)
    except Exception as e:  # noqa: BLE001
        log("step %d failed: %s" % (st["i"], e))
        st["errors"] = st.get("errors", 0) + 1
    st["t0"] = st["t"]
    st["wait"] = wait


unreal.register_slate_post_tick_callback(tick)
