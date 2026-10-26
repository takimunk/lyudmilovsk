"""Offscreen checks of the cats and dogs (docs/animals.md), in /Game/Dev/L_AnimalsTest or Lvl_Scene01; no window, no PIE:

  UnrealEditor <uproject> /Game/Dev/L_AnimalsTest -game -RenderOffscreen -ResX=1600 -ResY=900 -windowed
      -dpcvars=lyud.MainMenuOnStart=0,lyud.Animals.AnyMap=1,lyud.Birds.AnyMap=1 -abslog=<log> "-ExecCmds=py <this file>"

(unreal/scripts/test_animals.sh runs it.) Prints ANIMTEST lines and takes screenshots (Saved/Screenshots/Mac, in step
order): every breed and coat in a row, close-ups of the faces, the clips on a cat and a laika, then the simulation: a
laika and a cat meeting (chase, hiss or a tree), a cat and a sparrow (stalk, pounce, the bird flies), a friendly dog and
the player standing still.
ANIMTEST_PART=lineup|poses|life picks a part (default all); door, town and maru run only when asked.
maru: Maru up close (the model, the collar, the tag), her name over her, E (lyud.Animals.Hail) and her glad bark, the
tag swinging as she turns and trots (ANIMTEST lines with the collar's swing and clinks, frames in Saved/Screenshots).
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


if PART == "maru":
    M = (6000.0, 0.0, 0.0)

    def collar_of(w, actor):
        c = actor.get_component_by_class(unreal.AnimalCollarComponent) if actor else None
        return c

    def maru(w):
        l = life(w)
        r = l.get_animal(st["maru"])
        a = r[1] if isinstance(r, tuple) else r
        return a

    @step
    def s_maru(w):
        # a still body for the model shots, and the living one (the animal life) 4 m off for the rest
        cmd(w, "Animal.Spawn Maru 0 Stand %.0f %.0f 0 0" % (M[0], M[1]))
        cmd(w, "lyud.Animals.SpawnNear Maru 0 %.0f %.0f 2" % (M[0], M[1] + 900.0))
        cmd(w, "lyud.Animals.IgnorePlayer 0")
        log("maru spawned: " + life(w).report())
    @step
    def s_parts(w):
        for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.AnimalActor):
            body = a.get_editor_property("body")
            for c in a.get_components_by_class(unreal.StaticMeshComponent):
                log("part %s %s at %s visible %s mesh %s" % (a.get_name(), c.get_name(), c.get_world_location(), c.is_visible(),
                                                          c.static_mesh.get_name() if c.static_mesh else None))
            log("neck %s at %s" % (a.get_name(), body.get_socket_location("neck_01")))
            c = collar_of(w, a)
            log("clinks so far %s: %s" % (a.get_name(), c.get_editor_property("jingles") if c else "-"))
    step(lambda w: look(w, (M[0] + 160.0, M[1] - 190.0, 95.0), (M[0], M[1], 40.0)), 5.0)
    step(lambda w: shot(w, "maru_34"), 2.0)
    step(lambda w: look(w, (M[0] - 10.0, M[1] - 260.0, 60.0), (M[0], M[1], 45.0)), 2.0)
    step(lambda w: shot(w, "maru_side"), 2.0)
    step(lambda w: look(w, (M[0] + 150.0, M[1] - 15.0, 62.0), (M[0] + 30.0, M[1], 52.0)), 2.0)
    step(lambda w: shot(w, "maru_front"), 2.0)
    step(lambda w: look(w, (M[0] + 85.0, M[1] - 30.0, 52.0), (M[0] + 38.0, M[1], 50.0)), 2.0)
    step(lambda w: shot(w, "maru_collar"), 2.0)
    step(lambda w: look(w, (M[0] + 58.0, M[1] - 6.0, 50.0), (M[0] + 40.0, M[1], 49.0)), 2.0)
    step(lambda w: shot(w, "maru_tag"), 2.0)

    @step
    def s_find(w):
        ids = list(life(w).find_animals_near(unreal.Vector(M[0], M[1] + 900.0, 0.0), 2000.0))
        st["maru"] = ids[0] if ids else 0
        a = maru(w)
        log("living maru %d name '%s' key %s breed %s" % (st["maru"], a.name, a.name_key, a.breed))
        t = a.location
        # the player 3 m away, looking at her: her label and the [E] offer
        look(w, (t.x - 300.0, t.y - 60.0, t.z + 165.0), (t.x, t.y, t.z + 50.0))
    step(lambda w: (cmd(w, "shot showui"), log("shot maru_label")), 2.0)

    @step
    def s_rest(w):
        a = maru(w)
        c = collar_of(w, a.actor)
        st["jr"] = c.get_editor_property("jingles") if c else -1
    step(lambda w: log("at rest 5 s: %s more clinks" % (collar_of(w, maru(w).actor).get_editor_property("jingles") - st["jr"])), 5.0)

    @step
    def s_target(w):
        tags = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.AnimalNameTags)
        tg = tags[0].get_target() if tags else -2
        log("name tags: %d labels, target %d (maru %d)" % (tags[0].num_labels() if tags else -1, tg, st["maru"]))
        a = maru(w)
        c = collar_of(w, a.actor)
        st["j0"] = c.get_editor_property("jingles") if c else -1
        cmd(w, "lyud.Animals.Hail %d" % st["maru"])
        log("hailed: " + life(w).describe(st["maru"]))
    for k in range(10):
        def _bark(w, k=k):
            a = maru(w)
            c = collar_of(w, a.actor)
            log("bark t%d act %s swing %.1f jingles %s | %s" % (k, a.activity, c.get_editor_property("swing_deg") if c else -1,
                                                               c.get_editor_property("jingles") if c else "-", life(w).describe(st["maru"])))
            cmd(w, "shot showui")
        step(_bark, 0.35)

    @step
    def s_trot_still(w):
        # a body trotting on the spot: the neck bobs, the tag bounces (a burst of frames)
        cmd(w, "Animal.Spawn Maru 0 Trot %.0f %.0f 0 0" % (M[0] + 2000.0, M[1]))
    step(lambda w: look(w, (M[0] + 2120.0, M[1] - 110.0, 70.0), (M[0] + 2035.0, M[1], 48.0)), 3.0)
    for k in range(8):
        def _burst(w, k=k):
            for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.AnimalActor):
                c = collar_of(w, a)
                if c and a.get_actor_location().x > M[0] + 1500.0:
                    log("burst t%d swing %.1f jingles %d" % (k, c.get_editor_property("swing_deg"), c.get_editor_property("jingles")))
            shot(w, "maru_trot_burst_%d" % k)
        step(_burst, 0.25)

    @step
    def s_run(w):
        # she trots after the player who walks off: the tag bounces and clinks
        a = maru(w)
        t = a.location
        place(w, (t.x - 900.0, t.y - 300.0, t.z + 170.0), -8.0, 20.0)
        log("walked off")
    for k in range(12):
        def _trot(w, k=k):
            a = maru(w)
            c = collar_of(w, a.actor)
            log("trot t%d act %s v %.0f swing %.1f jingles %s" % (k, a.activity, a.velocity.length(), c.get_editor_property("swing_deg") if c else -1,
                                                                   c.get_editor_property("jingles") if c else "-"))
            t = a.location
            look(w, (t.x + 200.0, t.y - 180.0, t.z + 90.0), (t.x, t.y, t.z + 45.0))
            if k % 2 == 0:
                shot(w, "maru_trot_%02d" % k)
        step(_trot, 0.8)


if PART == "clubmaru":
    # Maru at the computer club's door in the real scene: her label, E, her bark
    def named(w):
        l = life(w)
        for i in list(l.find_animals_near(unreal.Vector(-193.0, 1150.0, 0.0), 4000.0)):
            r = l.get_animal(i)
            a = r[1] if isinstance(r, tuple) else r
            if a.name_key:
                return i, a
        return -1, None

    @step
    def s_club(w):
        place(w, (-193.0, 1150.0, 190.0), -8.0, 90.0)
        cmd(w, "lyud.Birds.Time 13 0")
    step(lambda w: log("club " + life(w).report()), 8.0)

    @step
    def s_club_find(w):
        i, a = named(w)
        st["maru"] = i
        log("maru at the club: %d %s %s" % (i, a.name if a else "-", life(w).describe(i) if i >= 0 else ""))
        if a:
            t = a.location
            look(w, (t.x - 260.0, t.y - 120.0, t.z + 165.0), (t.x, t.y, t.z + 45.0))
    step(lambda w: (cmd(w, "shot showui"), log("shot club_maru_label")), 2.0)
    step(lambda w: (cmd(w, "lyud.Animals.Hail %d" % st["maru"]), log("hail " + life(w).describe(st["maru"]))), 0.3)
    for k in range(5):
        step(lambda w, k=k: (cmd(w, "shot showui"), log("club bark t%d %s" % (k, life(w).describe(st["maru"])))), 0.5)
    @step
    def s_club_close(w):
        r = life(w).get_animal(st["maru"])
        a = r[1] if isinstance(r, tuple) else r
        t = a.location
        look(w, (t.x - 120.0, t.y - 70.0, t.z + 80.0), (t.x, t.y, t.z + 45.0))
    step(lambda w: shot(w, "club_maru_close"), 2.0)


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
