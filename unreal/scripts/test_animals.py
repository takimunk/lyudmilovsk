"""Offscreen checks of the cats and dogs (docs/animals.md), in /Game/Dev/L_AnimalsTest or Lvl_Scene01; no window, no PIE:

  UnrealEditor <uproject> /Game/Dev/L_AnimalsTest -game -RenderOffscreen -ResX=1600 -ResY=900 -windowed
      -dpcvars=lyud.MainMenuOnStart=0,lyud.Animals.AnyMap=1,lyud.Birds.AnyMap=1 -abslog=<log> "-ExecCmds=py <this file>"

(unreal/scripts/test_animals.sh runs it.) Prints ANIMTEST lines and takes screenshots (Saved/Screenshots/Mac, in step
order): every breed and coat in a row, close-ups of the faces, the clips on a cat and a laika, then the simulation: a
laika and a cat meeting (chase, hiss or a tree), a cat and a sparrow (stalk, pounce, the bird flies), a friendly dog and
the player standing still.
ANIMTEST_PART=lineup|poses|life picks a part (default all).
"""
import os

import unreal

st = {"t": 0.0, "i": 0, "t0": 0.0, "wait": 0.0}
PART = os.environ.get("ANIMTEST_PART", "all")
LEVEL = os.environ.get("ANIMTEST_LEVEL", "L_AnimalsTest")


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == LEVEL and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def life(w):
    return unreal.AnimalLifeSubsystem.get_animal_life(w)


def log(msg):
    unreal.log_warning("ANIMTEST " + msg)


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


STEPS = []


def step(fn, wait=2.0):
    STEPS.append((fn, wait))
    return fn


BREEDS = [("Cat", 7, 110.0), ("Laika", 2, 170.0), ("Shepherd", 2, 190.0), ("Mutt", 2, 130.0)]
CLIPS_CAT = ["Stand", "Walk", "Trot", "Run", "Stalk", "Sit", "Rest", "Sleep", "Groom", "Crouch", "Hiss", "Perch"]
CLIPS_DOG = ["Stand", "Walk", "Trot", "Run", "Sit", "Rest", "Sleep", "Groom", "Bark", "PlayBow", "Play", "Beg"]


@step
def s_start(w):
    cmd(w, "ghost")
    cmd(w, "lyud.Birds.Time 12 0")
    cmd(w, "r.MotionBlurQuality 0")
    log("start")


if PART in ("all", "lineup"):
    @step
    def s_lineup(w):
        # every breed and coat in two rows, 3/4 to the camera
        y = 0.0
        for row, (breed, n, gap) in enumerate(BREEDS):
            for c in range(n):
                x = 400.0 + row * 260.0
                cmd(w, "Animal.Spawn %s %d Stand %.0f %.0f 0 %.0f" % (breed, c, x, y + c * gap - (n - 1) * gap / 2, 200.0))
        log("lineup spawned")
    step(lambda w: look(w, (-450.0, -900.0, 350.0), (650.0, 0.0, 30.0)), 6.0)
    step(lambda w: shot(w, "lineup"), 2.0)
    step(lambda w: look(w, (150.0, -350.0, 120.0), (400.0, 0.0, 25.0)), 2.0)
    step(lambda w: shot(w, "cats_close"), 2.0)
    # faces: the tabby cat (coat 0 at the row's left end) and the laikas
    step(lambda w: look(w, (470.0, -385.0, 32.0), (400.0, -330.0, 27.0)), 2.0)
    step(lambda w: shot(w, "cat_face"), 2.0)
    step(lambda w: look(w, (790.0, -170.0, 75.0), (660.0, -85.0, 60.0)), 2.0)
    step(lambda w: shot(w, "laika_face"), 2.0)

if PART in ("all", "close"):
    @step
    def s_close(w):
        cmd(w, "Animal.Spawn Cat 0 Sit 3000 0 0 180")
        cmd(w, "Animal.Spawn Laika 0 Stand 3000 300 0 180")
        cmd(w, "Animal.Spawn Cat 1 Rest 3000 -250 0 150")
    step(lambda w: look(w, (2930.0, -15.0, 32.0), (3000.0, 0.0, 26.0)), 4.0)
    step(lambda w: shot(w, "cat_face_close"), 2.0)
    step(lambda w: look(w, (2880.0, -60.0, 60.0), (3000.0, 0.0, 18.0)), 2.0)
    step(lambda w: shot(w, "cat_sit_1m"), 2.0)
    step(lambda w: look(w, (2850.0, 230.0, 75.0), (3000.0, 300.0, 50.0)), 2.0)
    step(lambda w: shot(w, "laika_1m"), 2.0)
    step(lambda w: look(w, (2900.0, -330.0, 45.0), (3000.0, -250.0, 15.0)), 2.0)
    step(lambda w: shot(w, "ginger_rest"), 2.0)

if PART in ("all", "poses"):
    @step
    def s_poses(w):
        for i, c in enumerate(CLIPS_CAT):
            cmd(w, "Animal.Spawn Cat %d %s %.0f %.0f 0 -60" % (i % 7, c, -2000.0 + (i % 6) * 140.0, 1200.0 + (i // 6) * 180.0))
        for i, c in enumerate(CLIPS_DOG):
            cmd(w, "Animal.Spawn Laika %d %s %.0f %.0f 0 -60" % (i % 2, c, -2000.0 + (i % 6) * 220.0, 1700.0 + (i // 6) * 260.0))
        log("poses spawned")
    step(lambda w: look(w, (-1650.0, 700.0, 260.0), (-1650.0, 1300.0, 20.0)), 5.0)
    step(lambda w: shot(w, "cat_poses"), 2.0)
    step(lambda w: look(w, (-1450.0, 1050.0, 380.0), (-1450.0, 1850.0, 30.0)), 2.0)
    step(lambda w: shot(w, "dog_poses"), 2.0)

if PART in ("all", "life"):
    @step
    def s_life(w):
        # the simulation: a cat on the lawn, a laika 9 m off, a sparrow by the cat; the player standing still 6 m away
        cmd(w, "lyud.Animals.SpawnNear Cat 0 0 -2000 2")
        cmd(w, "lyud.Animals.SpawnNear Laika 0 -900 -2000 2")
        cmd(w, "lyud.Birds.SpawnNear HouseSparrow 250 -1900 2")
        look(w, (-300.0, -2600.0, 300.0), (-300.0, -2000.0, 0.0))
        log("life spawned: " + life(w).report())
    for k in range(16):
        def _watch(w, k=k):
            l = life(w)
            log("t%02d %s | %s | %s" % (k, l.report(), l.describe(0), l.describe(1)))
            if k % 3 == 0:
                shot(w, "life_%02d" % k)
        step(_watch, 2.5)


if PART == "door":
    DOOR = (-193.0, 1150.0, 190.0)

    @step
    def s_door(w):
        place(w, DOOR, -8.0, 90.0)
        log("door " + life(w).report())
    for k in range(3):
        def _door(w, k=k):
            ids = list(life(w).find_animals_near(unreal.Vector(DOOR[0], DOOR[1], 0.0), 2500.0))
            log("door t%d near: %s" % (k, " | ".join(life(w).describe(i) for i in ids)))
            if k == 2 and ids:
                r = life(w).get_animal(ids[0])
                a = r[1] if isinstance(r, tuple) else r
                t = a.location
                look(w, (t.x - 250.0, t.y - 250.0, t.z + 140.0), (t.x, t.y, t.z + 20.0))
        step(_door, 6.0)
    step(lambda w: shot(w, "door_animal"), 2.0)
    step(lambda w: (look(w, (-193.0, 1100.0, 200.0), (-193.0, 2200.0, 0.0))), 2.0)
    step(lambda w: shot(w, "door_view"), 2.0)

if PART == "town":
    SQ = (-25159.0, 61341.0, -3616.0)

    def animal(w, i):
        r = life(w).get_animal(i)
        if isinstance(r, tuple):
            return r[1] if r[0] else None
        return r

    @step
    def s_town(w):
        cmd(w, "lyud.Birds.Time %s" % os.environ.get("ANIMTEST_TIME", "15 40"))
        place(w, (SQ[0], SQ[1], SQ[2] + 170.0), -10.0, 0.0)
        log("town " + life(w).report())
    step(lambda w: log("town later " + life(w).report()), 20.0)
    for k in range(4):
        def _near(w, k=k):
            ids = list(life(w).find_animals_near(unreal.Vector(SQ[0], SQ[1], SQ[2]), 9000.0))
            if k < len(ids):
                a = animal(w, ids[k])
                log("near %d: %s" % (ids[k], life(w).describe(ids[k])))
                t = a.location
                look(w, (t.x - 260.0, t.y - 160.0, t.z + 110.0), (t.x, t.y, t.z + 20.0))
                st["shoot"] = True
            else:
                st["shoot"] = False
        step(_near, 2.0)
        step(lambda w, k=k: shot(w, "town_%d" % k) if st.get("shoot") else None, 2.0)

    @step
    def s_perf_on(w):
        place(w, (SQ[0] - 1500.0, SQ[1] - 800.0, SQ[2] + 400.0), -12.0, 30.0)
        cmd(w, "csvprofile start")
        log("perf on " + life(w).report())
    STEPS[-1] = (STEPS[-1][0], 14.0)
    step(lambda w: (cmd(w, "csvprofile stop"), log("perf on stop")), 2.0)

    @step
    def s_perf_off(w):
        cmd(w, "lyud.Animals 0")
        cmd(w, "csvprofile start")
        log("perf off")
    STEPS[-1] = (STEPS[-1][0], 14.0)
    step(lambda w: (cmd(w, "csvprofile stop"), log("perf off stop")), 2.0)
    step(lambda w: cmd(w, "lyud.Animals 1"), 2.0)


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
