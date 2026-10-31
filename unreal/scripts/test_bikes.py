"""Offscreen play test of the bicycles (docs/bikes.md). Leaves the desktop alone (no PIE):

  unreal/scripts/test_bikes.sh   (UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen ... "-ExecCmds=py <this>")

Shots go to Saved/Screenshots (in order; "BIKETEST shot NN" lines name each): the club's bike close up from three sides
and from above, the hero getting on it (from beside), his own view on it looking ahead and down at the bars, his hands
and knees, riding and leaning into a turn, getting off, leading it by the bars; the bikes standing in the entrances; a
resident of the Academgorodok taking their bike out of the entrance and riding off. BIKETEST_ONLY=hero,lobby,npc limits
the parts; BIKETEST_SHOTS=0 only logs. The hero part ends with a save and a load (the club's bike must stay where the
hero left it).

A shot is a step of its own: HighResShot takes the frame after the commands of its step, so nothing moves the view in
the step that shoots.
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


def bike(w, bid):
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.LyudBike):
        if str(a.get_editor_property("bike_id")) == bid:
            return a


def look(yaw_off, pitch):
    """The hero's own view: pitch (deg, down negative), yaw relative to the club's bike."""
    def f(w):
        b = bike(w, "club")
        yaw = b.get_actor_rotation().yaw + yaw_off
        pc(w).set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))
    return f


STEPS = []


def part(name, items):
    if ONLY and name not in ONLY:
        return
    for it in items:
        STEPS.append(it)


def C(*cmds, wait=2.0):
    """Console commands (or callables taking the world), then wait seconds."""
    def f(w):
        for c in cmds:
            c(w) if callable(c) else cmd(w, c)
    return (f, wait)


def SHOT(label, wait=1.5):
    def f(w):
        st["shot"] += 1
        log("shot %02d %s" % (st["shot"], label))
        if SHOTS:
            cmd(w, "HighResShot %s" % RES)
    return (f, wait)


part("all", [C("lyud.Birds.Time 12 30", "lyud.SkyHourOffset 0", "lyud.Bike.Log 1", "lyud.Bikes.Log 1", "Bike.Report", wait=4.0)])

part("hero", [
    C("Bike.Goto club", "Bike.Look club 240 0 60 45", wait=6.0), SHOT("club bike, right side"),
    C("Bike.Look club 190 55 95 45", wait=2.0), SHOT("club bike, three quarters from the front"),
    C("Bike.Look club 95 -15 15 40", wait=2.0), SHOT("club bike, the drivetrain close"),
    C("Bike.Look club 160 180 220 50", wait=2.0), SHOT("club bike from above, its left side and stand"),
    C("Bike.Look club 900 20 900 50", wait=2.5), SHOT("where the club's bike stands (from high up)"),
    # the hero beside it, his head shown for the outside view
    C("Hero.SideCam 260 40 30", "Bike.Look club 320 -35 70 50", wait=2.0), SHOT("the hero beside the bike", wait=0.5),
    C("Bike.Hero mount", wait=0.5), SHOT("getting on: the right leg up behind", wait=0.2),
    C(wait=0.3), SHOT("getting on: over the saddle", wait=0.2),
    C(wait=0.5), SHOT("getting on: onto the pedals", wait=0.2),
    C(wait=1.5), SHOT("on the bike, from outside"),
    C("Hero.DumpBones", "Hero.SideCam off", look(0, -5), wait=2.0), SHOT("his own view on the bike, looking ahead"),
    C(look(0, -58), wait=2.0), SHOT("his own view looking down: hands on the bars, knees"),
    C(look(0, -5), "Bike.Hero ride 1 0 8", wait=3.5), SHOT("riding: his own view", wait=0.3),
    C(look(0, -50), wait=0.7), SHOT("riding: looking down, pedalling", wait=0.3),
    C(look(0, -8), "Bike.Hero ride 0.8 0.7 3", wait=1.5), SHOT("riding into a turn: his view (horizon level, the body leans)", wait=0.3),
    C("Hero.SideCam 300 -60 60", "Bike.Look club 380 -20 90 50 follow", wait=0.6), SHOT("riding into a turn, from outside", wait=0.3),
    C("Bike.Hero ride 0 0 0.1", wait=2.5),
    C("Bike.Report", "Bike.Hero off", wait=0.6), SHOT("getting off", wait=0.3),
    C(wait=1.8), SHOT("off, the bike on its stand"),
    C("Bike.Hero lead", wait=1.5), SHOT("leading it by the bars (outside)"),
    C("Hero.SideCam off", look(20, -35), wait=1.5), SHOT("leading it: his own view"),
    C("Bike.Hero letgo", "Bike.Report", "lyud.Save biketest", wait=4.0),
    C("lyud.Load 0", wait=30.0),
    C("Bike.Report", wait=1.0),
])

part("lobby", [
    C("Bike.Look Novikova1.s0 210 40 70 70", wait=5.0), SHOT("a bike in Novikova 1's entrance"),
    C("Bike.Look Novikova3.s1 210 -40 70 70", wait=3.0), SHOT("a bike in Novikova 3's entrance"),
    C("Bike.Look academ 260 30 90 50", wait=3.0), SHOT("the bike by the lower cable-car station"),
])

npc = [C("lyud.Birds.Time 7 05", "Bike.Look Novikova3.s0 330 60 130 55", wait=8.0), C("Bike.Report", "Bike.NpcRide any sea_air", "lyud.SkyHourOffset 6", wait=3.0)]
for i in range(16):
    npc.append(SHOT("a resident with their bike, %d" % i, wait=2.7))
    if i % 4 == 3:
        npc.append(C("Bike.Report", wait=0.3))
part("npc", npc)

part("all", [C("Bike.Report", wait=1.0), C(lambda w: log("end"), "quit", wait=5.0)])


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
        log("step %d failed: %s" % (st["i"], e))
    st["t0"] = st["t"]
    st["wait"] = wait


unreal.register_slate_post_tick_callback(tick)
