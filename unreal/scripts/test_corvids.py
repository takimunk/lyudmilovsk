"""Offscreen play test of the crow family (docs/corvids.md): UCorvidLifeSubsystem in Lvl_Scene01, no window, no PIE.

  sh unreal/scripts/test_corvids.sh       (CORVIDTEST lines in the log, screenshots in Saved/Screenshots/MacEditor)

On the lawn out of the club's door it puts down finds (walnuts, acorns, a mussel, a crust, shiny things, a stick, and a
dead herring a little way off) and a party of corvids (crows, a carrion crow, magpie, jackdaws, rook, raven, jay,
nutcracker) that keep to the place, then watches: what they pick up, hide, crack, steal; a crow that likes the player
brings a present, one that holds a grudge scolds; thrown feed brings them down; the player picks a find up. Screenshots:
the finds close up, birds carrying things, the feast round the fish, the scolding, the feed.
"""
import os

import unreal

st = {"t": 0.0, "i": 0, "t0": 0.0}
SHOTS = os.environ.get("CORVIDTEST_SHOTS", "1") == "1"
DOOR = (-193.0, 1150.0, 190.0)
LAWN = (-193.0, 2400.0)


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def birds(w):
    return unreal.BirdLifeSubsystem.get_bird_life(w)


def corv(w):
    return unreal.CorvidLifeSubsystem.get_corvid_life(w)


def log(msg):
    unreal.log_warning("CORVIDTEST " + msg)


def ground(w, x, y, z0=400.0):
    hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(x, y, z0 + 2000.0), unreal.Vector(x, y, z0 - 4000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else 0.0


def place(w, loc, pitch, yaw, fov=0.0):
    cmd(w, "fov %.2f" % fov)
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pc.get_controlled_pawn().set_actor_location(unreal.Vector(*loc), False, True)
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))


def look(w, target, dist, span, yaw_off=0.0):
    """The camera at eye height dist cm off target (from the south-west, turned by yaw_off), zoomed in so that about span
    cm round the target fill the frame (the hero's eye can't go down to a nut on the ground, the lens can)."""
    import math
    a = math.radians(-120.0 + yaw_off)
    eye = unreal.Vector(target.x + math.cos(a) * dist, target.y + math.sin(a) * dist, target.z + 165.0)
    r = unreal.MathLibrary.find_look_at_rotation(eye, target)
    fov = max(4.0, min(70.0, math.degrees(2.0 * math.atan(span * 0.5 / dist))))
    place(w, (eye.x, eye.y, target.z + 95.0), r.pitch, r.yaw, fov)
    st["eye"] = (eye, target)


def fix_eye(w):
    if not st.get("eye"):
        return
    eye, target = st["eye"]
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    cam = pc.player_camera_manager.get_camera_location()
    pawn = pc.get_controlled_pawn()
    d = unreal.Vector(eye.x - cam.x, eye.y - cam.y, 0.0)      # the eye stays where the hero's height puts it
    pawn.set_actor_location(pawn.get_actor_location() + d, False, True)
    pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(cam + d, target))


def shot(w, name):
    if SHOTS:
        fix_eye(w)
        cmd(w, "HighResShot 1600x900")
        log("shot %s" % name)


def report(w):
    return corv(w).report()


def count(w, what):
    r = report(w)
    i = r.find(" " + what + "=")
    if i < 0:
        return 0
    j = i + len(what) + 2
    k = j
    while k < len(r) and r[k].isdigit():
        k += 1
    return int(r[j:k] or 0)


STEPS = []


def step(fn, wait=3.0):
    STEPS.append((fn, wait))
    return fn


@step
def s_start(w):
    cmd(w, "lyud.Birds.Time 11 0")
    place(w, DOOR, -4, 90)
    log("start " + report(w))


@step
def s_setup(w):
    x0, y0 = LAWN
    z = ground(w, x0, y0)
    st["lawn"] = unreal.Vector(x0, y0, z)
    finds = [("Walnut", 0, 0), ("Walnut", 60, 30), ("Walnut", -50, 70), ("Acorn", 30, -40), ("Acorn", -20, -60), ("Acorn", 90, -20),
             ("Hazelnut", -80, -10), ("PineCone", 120, 60), ("Mussel", -120, 40), ("Crust", 40, 110), ("BottleCap", -30, 20),
             ("Foil", 10, 50), ("Coin", 70, 80), ("Ring", -60, -30), ("Key", 100, 10), ("SeaGlass", -100, 100), ("Button", 0, -100),
             ("Stick", 150, -80)]
    for kind, dx, dy in finds:
        corv(w).spawn_item(kind, unreal.Vector(x0 + dx, y0 + dy, ground(w, x0 + dx, y0 + dy, z)))
    fx, fy = x0 + 900.0, y0 + 300.0
    st["fish"] = corv(w).spawn_item("Fish", unreal.Vector(fx, fy, ground(w, fx, fy, z)))
    st["fish_at"] = unreal.Vector(fx, fy, ground(w, fx, fy, z))
    flock = [("HoodedCrow", 600, 200), ("HoodedCrow", 700, -300), ("CarrionCrow", -600, 250), ("Magpie", -400, -350), ("Jackdaw", 300, 600),
             ("Jackdaw", 350, 650), ("Rook", -300, 650), ("Raven", 1200, 500), ("Jay", -800, -100), ("Nutcracker", -700, 400)]
    st["party"] = []
    for sp, dx, dy in flock:
        i = birds(w).spawn_bird(sp, unreal.Vector(x0 + dx, y0 + dy, ground(w, x0 + dx, y0 + dy, z)))
        st["party"].append((sp, i))
    log("party %s" % " ".join("%s:%d" % p for p in st["party"]))
    party(w)
    look(w, st["lawn"], 400.0, 320.0, -60.0)


@step
def s_finds_shot(w):
    shot(w, "finds on the lawn")


@step
def s_finds_close(w):
    look(w, st["lawn"], 300.0, 70.0, -60.0)


@step
def s_finds_close_shot(w):
    shot(w, "finds close up")
    # the player steps back to watch from the door
    place(w, (LAWN[0] - 1400.0, LAWN[1] - 1400.0, st["lawn"].z + 180.0), -10, 45)
    st["eye"] = None


step(lambda w: log("after 20 s: " + report(w)), 20.0)
step(lambda w: log("after 40 s: " + report(w)), 20.0)


@step
def s_carrier(w):
    c = corv(w).get_carriers(st["lawn"], 6000.0)
    st["carrier"] = c[0] if len(c) else None
    if st["carrier"] is not None:
        b = birds(w).get_bird(st["carrier"])
        b = b[1] if isinstance(b, tuple) else b
        st["carrier_at"] = b.location
        log("carrier %d %s: %s" % (st["carrier"], b.species_id, corv(w).describe_corvid(st["carrier"])))
        look(w, b.location, 700.0, 120.0)
    else:
        log("nobody carries anything now")


@step
def s_carrier_shot(w):
    if st.get("carrier") is not None:
        b = birds(w).get_bird(st["carrier"])
        b = b[1] if isinstance(b, tuple) else b
        look(w, b.location, 700.0, 120.0)
        shot(w, "a corvid with something in its bill")


@step
def s_fish(w):
    look(w, st["fish_at"], 1200.0, 700.0)


@step
def s_fish_shot(w):
    shot(w, "round the dead fish")
    log("fish: " + report(w))


@step
def s_gift_setup(w):
    # the player stands still at the lawn's edge; the corvids round here like them now
    place(w, (LAWN[0] - 500.0, LAWN[1] - 300.0, st["lawn"].z + 180.0), -12, 60)
    st["eye"] = None
    st["gifts0"] = count(w, "Gift")
    cmd(w, "lyud.Corvids.Feel 0.9 0 120")


def party(w):
    for sp, i in st["party"]:
        log("  %s %d: %s" % (sp, i, corv(w).describe_corvid(i)))


def _gift_wait(w):
    log("waiting for a present: %d (since start)" % count(w, "Gift"))
    party(w)


for _ in range(6):
    step(_gift_wait, 15.0)


@step
def s_gift_check(w):
    n = count(w, "Gift") - st["gifts0"]
    log("presents brought: %d %s" % (n, "PASS" if n > 0 else "FAIL"))
    shot(w, "after the presents")


@step
def s_take(w):
    # the player picks up whatever lies in reach in front of them (a present, or one of the finds walked up to)
    cmd(w, "lyud.Corvids.Take")
    x0, y0 = LAWN
    place(w, (x0 - 30.0, y0 - 110.0, st["lawn"].z + 180.0), -55, 80)


@step
def s_take2(w):
    cmd(w, "lyud.Corvids.Take")


step(lambda w: log("after the take: " + report(w)), 3.0)


@step
def s_grudge(w):
    place(w, (LAWN[0] - 400.0, LAWN[1] + 200.0, st["lawn"].z + 180.0), 10, 60)
    st["eye"] = None
    st["scold0"] = count(w, "Scold")
    cmd(w, "lyud.Corvids.Feel 0 0.9 120")


step(lambda w: (log("scolding after 10 s: %d" % (count(w, "Scold") - st["scold0"])), party(w)), 10.0)
step(lambda w: shot(w, "scolded"), 2.0)
step(lambda w: shot(w, "scolded 2"), 4.0)


@step
def s_grudge_check(w):
    n = count(w, "Scold") - st["scold0"]
    log("scold calls: %d %s" % (n, "PASS" if n >= 3 else "FAIL"))
    cmd(w, "lyud.Corvids.Feel 0.3 0 120")


@step
def s_throw(w):
    place(w, (LAWN[0] - 300.0, LAWN[1] - 500.0, st["lawn"].z + 180.0), -20, 80)
    st["eye"] = None
    cmd(w, "lyud.Corvids.Throw")
    log("feed thrown")


step(lambda w: log("feed after 20 s: " + report(w)), 20.0)
step(lambda w: shot(w, "at the thrown feed"), 10.0)


@step
def s_items_ab(w):
    # the items' drawing cost, A/B in this process
    cmd(w, "stat unit")
    log("items drawn: " + report(w).split("items drawn")[-1])


@step
def s_end(w):
    log("end " + report(w))
    log("birds " + birds(w).report()[:300])
    log("done")
    cmd(w, "quit")


def tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < 20.0:       # (shaders, streaming)
        return
    if st["i"] >= len(STEPS):
        return
    fn, wait = STEPS[st["i"]]
    if st["t"] - st["t0"] < wait:
        return
    st["t0"] = st["t"]
    st["i"] += 1
    try:
        fn(w)
    except Exception as e:      # keep going: the log says what broke
        log("step %s failed: %r" % (getattr(fn, "__name__", "?"), e))


unreal.register_slate_post_tick_callback(tick)
