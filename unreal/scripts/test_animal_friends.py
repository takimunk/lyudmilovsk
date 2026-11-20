"""Offscreen checks of the animal friends (docs/animals.md "Friends"); no window, no PIE (unreal/scripts/test_animal_friends.sh runs it).

FRIENDTEST_PART=stroke (default, /Game/Dev/L_AnimalsTest): a cat by the crouched hero: the hand held out, the sniff, the
  stroke (his hand on its head), a treat from the palm; then Maru: stroked, called along, at heel as he moves off.
FRIENDTEST_PART=door (Lvl_Scene01): Maru at the club's door becomes a friend and comes along; the hero goes into the club
  and she waits outside the door and lies down; he comes out and she is at heel; he sits on the nearest seat and she
  sits by it; a befriended cat near comes to his lap.

Prints FRIENDTEST lines (what the animals do, from the simulation) and takes screenshots (Saved/Screenshots/Mac, in step
order; a "shot <name>" line each): the hero's own view and a camera beside him (Hero.SideCam).
"""
import os

import unreal

st = {"t": 0.0, "i": 0, "t0": 0.0, "wait": 0.0, "cam": None}
PART = os.environ.get("FRIENDTEST_PART", "stroke")
LEVEL = os.environ.get("FRIENDTEST_LEVEL", "L_AnimalsTest")


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == LEVEL and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def life(w):
    return unreal.AnimalLifeSubsystem.get_animal_life(w)


def log(msg):
    unreal.log_warning("FRIENDTEST " + msg)


def pawn_of(w):
    return unreal.GameplayStatics.get_player_controller(w, 0).get_controlled_pawn()


def info(w, i):
    r = life(w).get_animal(i)
    return r[1] if isinstance(r, tuple) else r


def tell(w, what, i):
    a = info(w, i)
    p = pawn_of(w).get_actor_location()
    d = ((a.location.x - p.x) ** 2 + (a.location.y - p.y) ** 2) ** 0.5
    log("%s: %.1f m from the hero, bond %.2f friend %s | %s" % (what, d / 100.0, a.bond, a.friend, life(w).describe(i)))


def face(w, target, pitch=None):
    """Turns the hero's look to a world point (the body follows the yaw)."""
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    eye = pc.player_camera_manager.get_camera_location()
    r = unreal.MathLibrary.find_look_at_rotation(eye, target)
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=r.pitch if pitch is None else pitch, yaw=r.yaw))


def sidecam(w, eye, target):
    """The view from a camera at eye looking at target (the hero's head shown): Hero.SideCam's camera, moved."""
    cmd(w, "Hero.SideCam 200 120 30 follow")
    p = pawn_of(w)
    want = p.get_actor_location() + p.get_actor_right_vector() * 200.0 + p.get_actor_forward_vector() * 120.0 + unreal.Vector(0, 0, 30.0)
    best, best_d = None, 1e12
    for c in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.CameraActor):
        d = (c.get_actor_location() - want).length()
        if d < best_d:
            best, best_d = c, d
    if best:
        e, t = unreal.Vector(*eye), unreal.Vector(*target)
        best.set_actor_location_and_rotation(e, unreal.MathLibrary.find_look_at_rotation(e, t), False, True)
    st["cam"] = best


def beside(w, i, right=170.0, back=40.0, up=70.0):
    """The side camera placed to see both the hero and animal i."""
    a = info(w, i)
    p = pawn_of(w).get_actor_location()
    mid = unreal.Vector((a.location.x + p.x) / 2, (a.location.y + p.y) / 2, a.location.z + 35.0)
    dx, dy = a.location.x - p.x, a.location.y - p.y
    n = max((dx * dx + dy * dy) ** 0.5, 1.0)
    fx, fy = dx / n, dy / n
    eye = (mid.x - fy * right - fx * back, mid.y + fx * right - fy * back, a.location.z + up)
    sidecam(w, eye, (mid.x, mid.y, mid.z))


def own_view(w):
    cmd(w, "Hero.SideCam off")


def shot(w, name):
    cmd(w, "HighResShot 1600x900")
    log("shot %s" % name)


STEPS = []


def step(fn, wait=2.0):
    STEPS.append((fn, wait))
    return fn


@step
def s_start(w):
    cmd(w, "r.MotionBlurQuality 0")
    cmd(w, "lyud.Animals.IgnorePlayer 0")
    log("start part %s level %s" % (PART, LEVEL))


if PART == "stroke":
    @step
    def s_cat(w):
        cmd(w, "lyud.Birds.Time 12 0")
        p = pawn_of(w)
        p.set_actor_location(unreal.Vector(0.0, 0.0, 120.0), False, True)
        unreal.GameplayStatics.get_player_controller(w, 0).set_control_rotation(unreal.Rotator(roll=0, pitch=0, yaw=0))
        cmd(w, "lyud.Animals.SpawnNear Cat 1 170 25 2")
        cmd(w, "Dev.Do friends.treats")
    step(lambda w: log("spawned: " + life(w).report()), 3.0)

    @step
    def s_cat_find(w):
        ids = list(life(w).find_animals_near(unreal.Vector(170.0, 25.0, 0.0), 1500.0))
        st["cat"] = ids[0] if ids else 0
        tell(w, "the cat", st["cat"])
        # (trust comes from calm company in the game; here at once, the bond stays its own)
        cmd(w, "lyud.Friends.Befriend %d" % st["cat"])
        cmd(w, "lyud.Friends.Do crouch")
        a = info(w, st["cat"])
        face(w, unreal.Vector(a.location.x, a.location.y, a.location.z + 15.0))
    step(lambda w: (tell(w, "crouched", st["cat"]), cmd(w, "lyud.Friends.Do hand %d" % st["cat"])), 2.5)
    for k in range(6):
        step(lambda w, k=k: (tell(w, "hand out t%d" % k, st["cat"]), face(w, info(w, st["cat"]).location)), 1.0)
    step(lambda w: shot(w, "cat_hand_own_view"), 1.0)
    step(lambda w: (cmd(w, "lyud.Friends.Do stroke %d" % st["cat"]), tell(w, "stroke asked", st["cat"])), 0.2)
    step(lambda w: (tell(w, "stroked t1", st["cat"]), shot(w, "cat_stroke_own_view")), 1.2)
    step(lambda w: beside(w, st["cat"]), 0.6)
    step(lambda w: (tell(w, "stroked t2", st["cat"]), shot(w, "cat_stroke_side")), 0.5)
    step(lambda w: shot(w, "cat_stroke_side_2"), 0.6)
    step(lambda w: (cmd(w, "lyud.Friends.Do stroke %d" % st["cat"]), beside(w, st["cat"], right=-150.0, back=20.0, up=55.0)), 2.5)
    step(lambda w: (tell(w, "stroked again", st["cat"]), shot(w, "cat_stroke_other_side")), 1.5)
    step(lambda w: (cmd(w, "lyud.Friends.Do feed %d" % st["cat"]), tell(w, "treat held out", st["cat"])), 4.0)
    for k in range(4):
        step(lambda w, k=k: tell(w, "treat t%d" % k, st["cat"]), 1.0)
    step(lambda w: (beside(w, st["cat"]), None), 0.2)
    step(lambda w: shot(w, "cat_treat_side"), 0.6)
    step(lambda w: (own_view(w), cmd(w, "lyud.Friends.Report")), 2.0)

    # Maru: stroked, called along, at heel
    @step
    def s_maru(w):
        cmd(w, "lyud.Friends.Do stand")
        p = pawn_of(w)
        p.set_actor_location(unreal.Vector(3000.0, 0.0, 120.0), False, True)
        cmd(w, "lyud.Animals.SpawnNear Maru 0 3200 40 2")
    step(lambda w: None, 3.0)

    @step
    def s_maru_find(w):
        ids = list(life(w).find_animals_near(unreal.Vector(3200.0, 40.0, 0.0), 1500.0))
        st["maru"] = ids[0] if ids else 0
        tell(w, "maru", st["maru"])
        cmd(w, "lyud.Friends.Do crouch")
        face(w, info(w, st["maru"]).location)
        cmd(w, "lyud.Friends.Do hand %d" % st["maru"])
    for k in range(6):
        step(lambda w, k=k: (tell(w, "maru hand t%d" % k, st["maru"]), face(w, info(w, st["maru"]).location)), 1.0)
    for k in range(5):
        step(lambda w, k=k: (cmd(w, "lyud.Friends.Do stroke %d" % st["maru"]), tell(w, "maru stroke %d" % k, st["maru"])), 1.0)
        if k == 1:
            step(lambda w: beside(w, st["maru"], right=210.0, back=60.0, up=85.0), 0.8)
            step(lambda w: shot(w, "maru_stroke_side"), 0.6)
        step(lambda w: None, 3.2)
    step(lambda w: (own_view(w), cmd(w, "lyud.Friends.Do stand"), tell(w, "maru after strokes", st["maru"])), 1.5)
    step(lambda w: (cmd(w, "lyud.Friends.Do walk %d" % st["maru"]), tell(w, "walk asked", st["maru"])), 1.0)
    for k in range(8):
        def _walk(w, k=k):
            p = pawn_of(w)
            p.set_actor_location(p.get_actor_location() + unreal.Vector(0.0, 130.0, 0.0), True, False)
            unreal.GameplayStatics.get_player_controller(w, 0).set_control_rotation(unreal.Rotator(roll=0, pitch=-5, yaw=90))
            tell(w, "walk t%d" % k, st["maru"])
        step(_walk, 1.0)
    step(lambda w: beside(w, st["maru"], right=260.0, back=-120.0, up=90.0), 1.5)
    step(lambda w: (tell(w, "at heel, he stands", st["maru"]), shot(w, "maru_at_heel_side")), 1.0)
    step(lambda w: (own_view(w), cmd(w, "lyud.Friends.Report")), 1.0)

if PART == "door":
    def named(w):
        l = life(w)
        for i in list(l.find_animals_near(unreal.Vector(-193.0, 1150.0, 0.0), 6000.0)):
            if info(w, i).name_key:
                return i
        return -1

    @step
    def s_club(w):
        cmd(w, "lyud.Birds.Time 13 0")
        cmd(w, "Dev.Tp friends_club_door")
    step(lambda w: log("club: " + life(w).report()), 10.0)

    @step
    def s_find(w):
        st["maru"] = named(w)
        tell(w, "maru at the club", st["maru"])
        cmd(w, "lyud.Friends.Befriend maru")
        face(w, info(w, st["maru"]).location)
    step(lambda w: (cmd(w, "lyud.Friends.Do walk %d" % st["maru"]), tell(w, "walk asked", st["maru"])), 2.0)
    for k in range(4):
        step(lambda w, k=k: tell(w, "outside t%d" % k, st["maru"]), 1.5)
    step(lambda w: beside(w, st["maru"], right=260.0, back=-100.0, up=95.0), 0.5)
    step(lambda w: shot(w, "maru_heel_at_club"), 0.8)

    @step
    def s_in(w):
        st["door"] = pawn_of(w).get_actor_location()
        own_view(w)
        cmd(w, "Dev.Tp club_wake")
    for k in range(9):
        def _wait(w, k=k):
            tell(w, "he is inside t%d" % k, st["maru"])
            if k in (3, 8):
                a = info(w, st["maru"])
                d = st["door"]
                # a camera outside, a few metres from her, the door behind her
                dx, dy = a.location.x - d.x, a.location.y - d.y
                sidecam(w, (a.location.x - dx * 0.2 + 330.0, a.location.y - dy * 0.2 + 260.0, a.location.z + 110.0), (a.location.x, a.location.y, a.location.z + 30.0))
        step(_wait, 3.0)
        if k in (3, 8):
            step(lambda w, k=k: shot(w, "maru_waits_at_door_%s" % ("sits" if k == 3 else "lies")), 1.0)
            step(lambda w, k=k: (sidecam(w, (info(w, st["maru"]).location.x - 300.0, info(w, st["maru"]).location.y - 280.0, info(w, st["maru"]).location.z + 120.0),
                                         (info(w, st["maru"]).location.x, info(w, st["maru"]).location.y, info(w, st["maru"]).location.z + 30.0)), None), 0.4)
            step(lambda w, k=k: shot(w, "maru_waits_at_door_%s_b" % ("sits" if k == 3 else "lies")), 1.0)

    @step
    def s_out(w):
        own_view(w)
        cmd(w, "Dev.Tp friends_club_door")
    for k in range(4):
        step(lambda w, k=k: tell(w, "he is out again t%d" % k, st["maru"]), 1.5)
    step(lambda w: beside(w, st["maru"], right=260.0, back=-100.0, up=95.0), 0.5)
    step(lambda w: shot(w, "maru_back_at_heel"), 0.8)

    # a seat: she sits by it; a befriended cat near comes to the lap
    @step
    def s_sit(w):
        own_view(w)
        cmd(w, "lyud.City.Sit")
    for k in range(6):
        step(lambda w, k=k: tell(w, "he sits t%d (walker %d, lap cat %d)" % (k, life(w).walker(), life(w).lap_cat()), st["maru"]), 2.5)
    step(lambda w: beside(w, st["maru"], right=300.0, back=-60.0, up=110.0), 0.5)
    step(lambda w: shot(w, "maru_by_the_seat"), 0.8)

    @step
    def s_lapcat(w):
        p = pawn_of(w).get_actor_location()
        l = life(w)
        cats = [i for i in list(l.find_animals_near(p, 2500.0)) if info(w, i).kind == unreal.AnimalKind.CAT]
        log("cats within 25 m of the seat: %d" % len(cats))
        for i in cats[:3]:
            cmd(w, "lyud.Friends.Befriend %d" % i)
        st["cats"] = cats[:3]
    for k in range(12):
        def _lap(w, k=k):
            log("lap t%d: lap cat %d" % (k, life(w).lap_cat()))
            if life(w).lap_cat() >= 0:
                tell(w, "the lap cat", life(w).lap_cat())
        step(_lap, 3.0)

    @step
    def s_lapshot(w):
        i = life(w).lap_cat()
        if i >= 0:
            own_view(w)
            face(w, info(w, i).location)
            cmd(w, "lyud.Friends.Do stroke %d" % i)
    step(lambda w: shot(w, "lap_cat_own_view"), 1.2)
    step(lambda w: (beside(w, life(w).lap_cat() if life(w).lap_cat() >= 0 else st["maru"], right=230.0, back=0.0, up=110.0), None), 0.5)
    step(lambda w: shot(w, "seat_side"), 0.8)
    step(lambda w: (own_view(w), cmd(w, "lyud.Friends.Report")), 1.0)


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
    if not w or st["t"] < 6.0:
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
