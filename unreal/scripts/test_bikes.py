"""Offscreen play test of the bicycles (docs/bikes.md). Leaves the desktop alone (no PIE):

  unreal/scripts/test_bikes.sh   (UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen ... "-ExecCmds=py <this>")

Shots go to Saved/Screenshots (in order; BIKETEST lines name each): the club's bike close up from three sides, the hero
getting on it (from beside), his own view on it looking ahead and down at the bars, his hands and knees, riding and
leaning into a turn, getting off, leading it by the bars; then in the Academgorodok a resident taking their bike out of
the entrance and riding off; the bikes standing in the entrances. BIKETEST_ONLY=hero|npc|lobby limits the parts;
BIKETEST_SHOTS=0 only logs. Ends with a save and a load (the club's bike must stay where the hero left it).
"""
import os

import unreal

st = {"t": 0.0, "i": 0, "t0": 0.0, "wait": 0.0, "shot": 0}
SHOTS = os.environ.get("BIKETEST_SHOTS", "1") == "1"
ONLY = [s for s in os.environ.get("BIKETEST_ONLY", "").split(",") if s]
RES = os.environ.get("BIKETEST_RES", "1600x900")


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def log(msg):
    unreal.log_warning("BIKETEST " + msg)


def pc(w):
    return unreal.GameplayStatics.get_player_controller(w, 0)


def hero(w):
    return pc(w).get_controlled_pawn()


def shot(w, label):
    st["shot"] += 1
    log("shot %02d %s" % (st["shot"], label))
    if SHOTS:
        cmd(w, "HighResShot %s" % RES)


def bike(w, bid):
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.LyudBike):
        if str(a.get_editor_property("bike_id")) == bid:
            return a


def look(w, yaw_off, pitch):
    """The hero's own view: pitch (deg, down negative), yaw relative to his bike."""
    b = bike(w, "club")
    yaw = (b.get_actor_rotation().yaw if b else hero(w).get_actor_rotation().yaw) + yaw_off
    pc(w).set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))


STEPS = []


def step(part, wait=3.0):
    def wrap(fn):
        if not ONLY or part in ONLY or part == "all":
            STEPS.append((fn, wait))
        return fn
    return wrap


@step("all", 2.0)
def s_start(w):
    cmd(w, "lyud.Birds.Time 11 40")
    cmd(w, "lyud.Bike.Log 1")
    cmd(w, "Bike.Report")


# --- the hero and the club's bike ---
@step("hero", 6.0)
def s_bike_close(w):
    cmd(w, "Bike.Goto club")
    cmd(w, "Bike.Look club 230 0 55 45")


@step("hero", 2.5)
def s_shot_side(w):
    shot(w, "club bike, right side")
    cmd(w, "Bike.Look club 190 55 95 45")


@step("hero", 2.5)
def s_shot_front(w):
    shot(w, "club bike, three quarters from the front")
    cmd(w, "Bike.Look club 95 -15 15 40")


@step("hero", 2.5)
def s_shot_drive(w):
    shot(w, "club bike, the drivetrain close")
    cmd(w, "Bike.Look club 120 170 110 45")


@step("hero", 2.5)
def s_shot_left(w):
    shot(w, "club bike, left side from above (kickstand)")
    # the hero beside it, his head shown for the outside view
    cmd(w, "Hero.SideCam 260 40 30")
    cmd(w, "Bike.Look club 300 -35 70 50")


@step("hero", 1.0)
def s_mount(w):
    shot(w, "the hero beside the bike")
    cmd(w, "Bike.Hero mount")


@step("hero", 0.55)
def s_mount1(w):
    shot(w, "getting on: the right leg up behind")


@step("hero", 0.55)
def s_mount2(w):
    shot(w, "getting on: over the saddle")


@step("hero", 1.4)
def s_mount3(w):
    shot(w, "getting on: onto the pedals")


@step("hero", 2.0)
def s_on(w):
    shot(w, "on the bike, from outside")
    cmd(w, "Hero.DumpBones")
    cmd(w, "Hero.SideCam off")
    look(w, 0, -5)


@step("hero", 2.0)
def s_fpv_ahead(w):
    shot(w, "his own view on the bike, looking ahead")
    look(w, 0, -58)


@step("hero", 2.0)
def s_fpv_down(w):
    shot(w, "his own view looking down: hands on the bars, knees")
    look(w, 0, -5)
    cmd(w, "Bike.Hero ride 1 0 7")


@step("hero", 3.5)
def s_ride1(w):
    shot(w, "riding: his own view")
    look(w, 0, -50)


@step("hero", 1.0)
def s_ride2(w):
    shot(w, "riding: looking down, pedalling")
    look(w, 0, -8)
    cmd(w, "Bike.Hero ride 0.8 0.7 3")


@step("hero", 1.8)
def s_turn(w):
    shot(w, "riding into a turn (lean)")
    cmd(w, "Hero.SideCam 300 -60 60")
    cmd(w, "Bike.Look club 340 -20 80 50 follow")


@step("hero", 1.0)
def s_turn_out(w):
    shot(w, "riding into a turn, from outside")
    cmd(w, "Bike.Hero ride 0 0 0.1")


@step("hero", 2.5)
def s_off(w):
    cmd(w, "Bike.Report")
    cmd(w, "Bike.Hero off")


@step("hero", 0.7)
def s_off1(w):
    shot(w, "getting off")


@step("hero", 2.2)
def s_off2(w):
    shot(w, "off, the bike on its stand")
    cmd(w, "Bike.Hero lead")


@step("hero", 1.5)
def s_lead(w):
    shot(w, "leading it by the bars (outside)")
    cmd(w, "Hero.SideCam off")
    look(w, 20, -35)


@step("hero", 1.5)
def s_lead_fpv(w):
    shot(w, "leading it: his own view")
    cmd(w, "Bike.Hero letgo")
    cmd(w, "Bike.Report")


# --- the residents ---
@step("lobby", 4.0)
def s_lobby1(w):
    cmd(w, "Bike.Look Novikova1.s0 210 40 70 70")


@step("lobby", 2.5)
def s_lobby1s(w):
    shot(w, "a bike in Novikova 1's entrance")
    cmd(w, "Bike.Look Novikova3.s1 210 -40 70 70")


@step("lobby", 2.5)
def s_lobby2s(w):
    shot(w, "a bike in Novikova 3's entrance")


@step("npc", 3.0)
def s_npc(w):
    cmd(w, "Bike.Report")
    cmd(w, "lyud.Bikes.Log 1")
    cmd(w, "Bike.Look Novikova3.s0 320 60 120 55 follow")
    cmd(w, "Bike.NpcRide Novikova3.s0 sea_air")


def npc_shot(i):
    def s(w):
        shot(w, "a resident with their bike, %d" % i)
        if i % 3 == 2:
            cmd(w, "Bike.Report")
    s.__name__ = "s_npc%d" % i
    return s


for i in range(14):
    step("npc", 3.0)(npc_shot(i))


@step("hero", 2.0)
def s_save(w):
    cmd(w, "Bike.Report")
    cmd(w, "lyud.Save biketest")


@step("hero", 25.0)
def s_load(w):
    cmd(w, "lyud.Load 0")


@step("all", 2.0)
def s_end(w):
    cmd(w, "Bike.Report")
    log("end")
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
    if not w or st["t"] < 25.0:
        return
    if st["i"] == 0 and not unreal.GameplayStatics.get_all_actors_of_class(w, unreal.LyudBike):
        if st["t"] > 120.0:
            log("no bikes in the level")
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
