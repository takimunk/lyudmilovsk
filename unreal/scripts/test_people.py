"""Offscreen checks of the people's life (docs/people.md) in Lvl_Scene01; no window, no PIE:

  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen -ResX=1600 -ResY=900 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py <this file>"

(unreal/scripts/test_people.sh runs it.) Prints PEOPLETEST lines and takes screenshots (Saved/Screenshots/Mac, in step
order): the club at the start of a new game (everybody at their station, the owner in his booth), a boy getting up to the
vending machine, people at the sunset on the spur, a night in the Academgorodok (asleep in their beds), a morning
(breakfast, the way to school and work), the library.
PEOPLETEST_PART=club|town|all picks a part (default all).
"""
import os

import unreal

st = {"t": 0.0, "i": 0, "t0": 0.0, "wait": 0.0}
PART = os.environ.get("PEOPLETEST_PART", "all")


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def life(w):
    return unreal.PeopleLifeSubsystem.get_people_life(w)


def log(msg):
    unreal.log_warning("PEOPLETEST " + msg)


def person(w, key):
    l = life(w)
    r = l.get_person(l.find_person(key))
    if isinstance(r, tuple):
        return r[1] if r[0] else None
    return r


def place(w, loc, pitch, yaw):
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    pawn.set_actor_location(unreal.Vector(*loc), False, True)
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))
    st["eye"] = (unreal.Vector(*loc), pitch, yaw)


def look(w, eye, target):
    e, t = unreal.Vector(*eye), unreal.Vector(*target)
    r = unreal.MathLibrary.find_look_at_rotation(e, t)
    place(w, eye, r.pitch, r.yaw)


def fix_eye(w):
    if not st.get("eye"):
        return
    eye, pitch, yaw = st["eye"]
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    cam = pc.player_camera_manager.get_camera_location()
    pawn = pc.get_controlled_pawn()
    pawn.set_actor_location(pawn.get_actor_location() + (eye - cam), False, True)
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))


def shot(w, name):
    fix_eye(w)
    cmd(w, "HighResShot 1600x900")
    log("shot %s" % name)


def present(w, keys, activities=None):
    """The first of keys who is in the world now (not away, not in the cable car), doing one of activities if given."""
    for k in keys:
        p = person(w, k)
        if p and p.pose != unreal.PersonPose.HIDDEN and p.state != "riding" and (not activities or p.activity in activities):
            return k
    return None


def look_at_present(w, keys, tag, back=300.0, up=140.0, side=0.6, activities=None):
    k = present(w, keys, activities)
    st["who"] = k
    log("%s: %s" % (tag, k))
    if k:
        look_at_person(w, k, back, up, side)


def look_at_person(w, key, back=260.0, up=120.0, side=0.6):
    p = person(w, key)
    if not p:
        log("no %s" % key)
        return
    t = p.location
    import math
    yaw = math.radians(p.yaw)
    # in front of them, a little to the side
    ex = t.x + math.cos(yaw) * back + math.sin(yaw) * back * side
    ey = t.y + math.sin(yaw) * back - math.cos(yaw) * back * side
    look(w, (ex, ey, t.z + up), (t.x, t.y, t.z + 90.0))
    log("%s: %s at %s pose %s | %s" % (key, p.activity, p.place, p.pose, life(w).describe(p.id)))


STEPS = []
KEYS = ["streamer", "girl_b", "crowd_11", "crowd_12", "crowd_13", "kid_girl", "kid_boy", "guy_a", "guy_b", "girl_a", "clerk", "owner"]


def step(fn, wait=2.0):
    STEPS.append((fn, wait))
    return fn


def report(tag):
    def _r(w):
        log("%s %s" % (tag, life(w).report()))
    return _r


@step
def s_start(w):
    cmd(w, "ghost")
    cmd(w, "r.MotionBlurQuality 0")
    log("start running=%s people=%d" % (life(w).is_running(), life(w).num_people()))
    log("flats " + ", ".join("%s: %s" % (k, v) for k, v in life(w).flats().items() if v))


if PART in ("all", "club"):
    step(report("club 16:00"), 2.0)
    # the stations from the walkway, then the hall and the booth
    step(lambda w: look(w, (-110.0, 60.0, 175.0), (60.0, -700.0, 80.0)), 3.0)
    step(lambda w: shot(w, "club_stations"), 2.0)
    step(lambda w: look(w, (-120.0, 280.0, 175.0), (170.0, 640.0, 90.0)), 2.0)
    step(lambda w: shot(w, "club_booth"), 2.0)

    @step
    def s_snack(w):
        ok = life(w).order(life(w).find_person("kid_boy"), "snack_club", "")
        log("order kid_boy snack_club: %s" % ok)
    step(lambda w: look_at_person(w, "kid_boy", 300.0, 60.0, 0.3), 2.5)
    step(lambda w: shot(w, "kid_boy_standing_up"), 3.0)
    step(lambda w: look_at_person(w, "kid_boy", 300.0, 60.0, 0.3), 1.0)
    step(lambda w: shot(w, "kid_boy_walking"), 6.0)
    # the hall from by the booth, toward the vending machines (lit by the hall's lamps)
    step(lambda w: look(w, (60.0, 420.0, 175.0), (-185.0, 300.0, 110.0)), 1.0)
    step(lambda w: shot(w, "kid_boy_at_vending"), 2.0)

if PART in ("all", "town"):
    # the sunset: who comes out to see the sun go down (the sky's sunset on 20 October is ~17:45)
    step(lambda w: cmd(w, "lyud.Birds.Time 17 30"), 4.0)
    step(report("sunset 17:30"), 1.0)
    step(lambda w: look_at_present(w, KEYS, "sunset", 420.0, 120.0, 0.8, ("sun_watch",))
         or (not st.get("who") and look_at_present(w, KEYS, "out", 420.0, 120.0, 0.8,
                                                   ("walk", "sit_bench", "chat"))), 2.0)
    step(lambda w: shot(w, "sunset_person") if st.get("who") else None, 2.0)

    # a night: asleep at home
    step(lambda w: cmd(w, "lyud.Birds.Time 23 50"), 4.0)
    step(report("night 23:50"), 1.0)
    step(lambda w: look_at_person(w, "kid_girl", 200.0, 190.0, 1.0), 2.0)
    step(lambda w: shot(w, "kid_girl_asleep"), 2.0)
    step(lambda w: look_at_person(w, "girl_a", 200.0, 190.0, 1.0), 2.0)
    step(lambda w: shot(w, "vera_asleep"), 2.0)

    # a morning: breakfast, off to school and work
    step(lambda w: cmd(w, "lyud.Birds.Time 7 40"), 4.0)
    step(report("morning 07:40"), 1.0)
    step(lambda w: look_at_person(w, "kid_girl", 260.0, 120.0, 0.6), 2.0)
    step(lambda w: shot(w, "kid_girl_morning"), 6.0)
    step(lambda w: look_at_present(w, ["clerk", "girl_a", "kid_boy", "crowd_12", "guy_b"], "morning", 300.0, 140.0, 0.6), 2.0)
    step(lambda w: shot(w, "morning_person") if st.get("who") else None, 2.0)

    # the library at midday (Olya at her desk on Wednesday)
    step(lambda w: cmd(w, "lyud.Birds.Time 12 30"), 4.0)
    step(report("midday 12:30"), 1.0)

    @step
    def s_library(w):
        ok = life(w).order(life(w).find_person("crowd_12"), "read_library", "library")
        log("order crowd_12 read_library: %s" % ok)
    # wait (up to ~3 min) till she sits down in the reading hall
    for k in range(40):
        def _wait(w, k=k):
            p = person(w, "crowd_12")
            if p.state == "doing" and not st.get("reading"):
                look_at_person(w, "crowd_12", 260.0, 150.0, 0.4)
                st["reading"] = True
                log("crowd_12 reading after %d checks" % k)
        step(_wait, 10.0)
    step(lambda w: look_at_person(w, "crowd_12", 260.0, 150.0, 0.4), 2.0)
    step(lambda w: shot(w, "library_reader"), 2.0)

    # an afternoon walk on the square
    step(lambda w: cmd(w, "lyud.Birds.Time 14 30"), 4.0)

    @step
    def s_walk(w):
        ok = life(w).order(life(w).find_person("streamer"), "walk", "academ_square")
        log("order streamer walk: %s" % ok)
    step(lambda w: look_at_person(w, "streamer", 450.0, 200.0, 0.6), 25.0)
    step(lambda w: look_at_person(w, "streamer", 450.0, 200.0, 0.6), 1.0)
    step(lambda w: shot(w, "streamer_walking"), 2.0)
    step(report("later"), 1.0)


@step
def s_end(w):
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
