"""Offscreen play test of the birds' nests (docs/bird_nests.md). Leaves the desktop alone (no PIE):

  unreal/scripts/test_nests.sh   (UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen ... "-ExecCmds=py <this>")

For each case it moves the calendar to a day this year a species' nearest nest is in a stage (lyud.Birds.Nest), waits for
the birds to settle, puts the camera close to the nest and takes shots (Saved/Screenshots, in order; NESTTEST lines say
which is which): the nest a metre or so off, a grazing look over its rim, and a wider one with the parents about. Prints
each nest's state, where it was put (on a branch, a wall, the ground) and the bird life's report.
NESTTEST_ONLY=Rook,Blackbird limits the species; NESTTEST_SHOTS=0 only logs.
"""
import os

import unreal

st = {"t": 0.0, "i": 0, "t0": 0.0, "wait": 0.0}
SHOTS = os.environ.get("NESTTEST_SHOTS", "1") == "1"
ONLY = [s for s in os.environ.get("NESTTEST_ONLY", "").split(",") if s]

# species, stage, how far through it, camera distance (cm)
CASES = [
    ("Rook", "Build", 0.6, 250), ("Rook", "Eggs", 0.5, 250), ("Magpie", "Build", 0.9, 220), ("HoodedCrow", "Chicks", 0.6, 220),
    ("Blackbird", "Eggs", 0.6, 110), ("Blackbird", "Chicks", 0.8, 110), ("SongThrush", "Eggs", 0.5, 100),
    ("Chaffinch", "Chicks", 0.3, 90), ("Goldfinch", "Build", 0.7, 80), ("Robin", "Chicks", 0.6, 90),
    ("GreatTit", "Chicks", 0.9, 120), ("Starling", "Chicks", 0.9, 150), ("HouseSparrow", "Chicks", 0.8, 200),
    ("BarnSwallow", "Chicks", 0.8, 120), ("SandMartin", "Build", 0.8, 250), ("FeralPigeon", "Eggs", 0.5, 120),
    ("WoodPigeon", "Eggs", 0.5, 120), ("HerringGull", "Eggs", 0.5, 200), ("HerringGull", "Fledged", 0.05, 300),
    ("MuteSwan", "Eggs", 0.5, 500), ("MuteSwan", "Fledged", 0.05, 600), ("GreyHeron", "Chicks", 0.6, 400),
    ("Cormorant", "Chicks", 0.6, 400), ("WhiteTailedEagle", "Chicks", 0.6, 600), ("CommonBuzzard", "Eggs", 0.5, 350),
]
if ONLY:
    CASES = [c for c in CASES if c[0] in ONLY]


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def life(w):
    return unreal.BirdLifeSubsystem.get_bird_life(w)


def log(msg):
    unreal.log_warning("NESTTEST " + msg)


def nest(w, i):
    r = life(w).get_nest(i)
    if isinstance(r, tuple):
        return r[1] if r[0] else None
    return r


def place_camera(w, target, yaw, dist, up):
    """A camera dist cm from target along yaw, up cm above it, looking at it (a CameraActor as the view target: the
    hero's own camera sits on its head and its body is in the way)."""
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    fwd = unreal.Rotator(roll=0, pitch=0, yaw=yaw).get_forward_vector()
    eye = unreal.Vector(target.x + fwd.x * dist, target.y + fwd.y * dist, target.z + up)
    rot = unreal.MathLibrary.find_look_at_rotation(eye, target)
    cmd(w, "lyud.Birds.ViewFrom %.1f %.1f %.1f %.2f %.2f 60" % (eye.x, eye.y, eye.z, rot.pitch, rot.yaw))
    pc.get_controlled_pawn().set_actor_location(eye + unreal.Vector(0, 0, 400), False, True)


STEPS = []


def step(fn, wait=3.0):
    STEPS.append((fn, wait))
    return fn


@step
def s_start(w):
    cmd(w, "ghost")
    cmd(w, "lyud.Birds.IgnorePlayer 1")
    near = life(w).get_nests_near(unreal.Vector(0, 0, 0), 1e9)
    kinds = {}
    for n in near:
        kinds[str(n.stage)] = kinds.get(str(n.stage), 0) + 1
    log("start: %d nests %s; %s" % (len(near), kinds, life(w).report()))


HIDDEN_KINDS = ("Cavity", "Eave", "Burrow", "MudCup")


def trees(w, hidden):
    """Hides the forests and the live trees (the nest stays where it was put: on their wood or floating, which shows)."""
    for cls in (unreal.ProceduralForest, unreal.ProceduralPlant):
        for a in unreal.GameplayStatics.get_all_actors_of_class(w, cls):
            a.set_actor_hidden_in_game(hidden)


def make_case(sp, stage, frac, dist):
    def s_go(w):
        cmd(w, "lyud.Birds.Nest %s %s %.2f" % (sp, stage, frac))

    def s_look(w):
        pc = unreal.GameplayStatics.get_player_controller(w, 0)
        i = life(w).find_nest(sp, pc.get_controlled_pawn().get_actor_location())
        n = nest(w, i) if i >= 0 else None
        if not n or n.id < 0:
            log("%s %s: no nest (find %d)" % (sp, stage, i))
            st["n"] = None
            return
        st["n"] = n
        log("%s %s: nest %d %s stage %s eggs %d chicks %d age %.1f progress %.2f placed %s at %.0f %.0f %.0f yaw %.0f" % (
            sp, stage, n.id, n.kind, n.stage, n.eggs, n.chicks, n.chick_age, n.progress, n.placed, n.location.x, n.location.y,
            n.location.z, n.yaw))
        if str(n.kind) in HIDDEN_KINDS:
            # from in front of the hole or the box, a little above
            place_camera(w, n.location, n.yaw, dist, dist * 0.15)
        else:
            # from above and to one side: into the cup
            place_camera(w, n.location, n.yaw + 30.0, dist * 0.75, dist * 0.8)

    def s_shot1(w):
        if st.get("n") and SHOTS:
            rep = life(w).report()
            log("shot %s %s scene: %d birds within 15 m; %s" % (sp, stage, len(life(w).find_birds_near(st["n"].location, 1500)),
                                                                  rep[rep.find("nearest nest"):][:400]))
            cmd(w, "HighResShot 1600x900")

    def s_bare(w):
        if st.get("n"):
            trees(w, True)

    def s_shot2(w):
        if st.get("n") and SHOTS:
            log("shot %s %s bare (trees hidden)" % (sp, stage))
            cmd(w, "HighResShot 1600x900")

    def s_back(w):
        trees(w, False)

    for fn, wait in ((s_go, 6.0), (s_look, 4.0), (s_shot1, 3.0), (s_bare, 1.5), (s_shot2, 3.0), (s_back, 0.5)):
        STEPS.append((fn, wait))


for c in CASES:
    make_case(*c)


@step
def s_end(w):
    log("end: " + life(w).report())
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
    if not life(w) or not life(w).is_running():
        if st["t"] > 90.0 and st["i"] == 0:
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
