"""Offscreen play test of birds coming to the feed (docs/bird_taming.md "Birds come to the feed"), flying round the player,
perching on real things and being drawn far off (docs/bird_life.md). Leaves the desktop alone (no PIE):

  unreal/scripts/test_birdfeed.sh        (BIRDFEED lines in the log; frames in Saved/BirdFeed/<n>_<name>.png)

The hero stands in the Academgorodok's street at mid-morning with a box of mealworms (starlings' favourite) in the hand:
the birds round about notice it and come down round them; the palm then brings one in. In the same run, A/B in one
process: flying birds with and without the swerve round the player (lyud.Birds.Avoid), perches with and without the check
against the real branches and roofs (lyud.Birds.PerchCheck, lyud.Birds.PerchAudit), and the far birds' pops with the old
draw range and the new one (lyud.Birds.DrawDistance / FullDetail).
"""
import os

import unreal

st = {"t": 0.0, "i": 0, "t0": 0.0, "wait": 0.0, "shot": 0}
OUT = os.path.join(unreal.Paths.project_saved_dir(), "BirdFeed")
# the Academgorodok's street (a bird site of bird_habitats.json), Unreal cm
ACADEM = (-24930.0, 60280.0)
DOOR = (-193.0, 1150.0, 190.0)
FEED = os.environ.get("BIRDFEED_FEED", "feed_mealworm")


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def birds(w):
    return unreal.BirdLifeSubsystem.get_bird_life(w)


def log(msg):
    unreal.log_warning("BIRDFEED " + msg)


def ground(w, x, y):
    hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(x, y, 20000.0), unreal.Vector(x, y, -20000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else -3600.0


def pawn(w):
    return unreal.GameplayStatics.get_player_controller(w, 0).get_controlled_pawn()


def place(w, x, y, pitch, yaw):
    z = ground(w, x, y)
    pawn(w).set_actor_location(unreal.Vector(x, y, z + 100.0), False, True)
    unreal.GameplayStatics.get_player_controller(w, 0).set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))
    st["feet"] = (x, y, z)


def shot(w, name):
    st["shot"] += 1
    path = os.path.join(OUT, "%02d_%s.png" % (st["shot"], name))
    cmd(w, "HighResShot 1280x720 filename=%s" % path)


def report(w):
    r = birds(w).report()
    keep = [p for p in r.split("; ") if p.startswith(("BirdLife", "at the hand", "perches", "through", "pops", "drawn"))]
    return " | ".join(keep)[:600]


def view_from_side(w, dist=750.0, up=280.0, side_yaw=200.0):
    """A free camera dist cm off the hero, looking at their feet (the ring of birds round them)."""
    x, y, z = st["feet"]
    import math
    a = math.radians(side_yaw)
    cx, cy, cz = x + math.cos(a) * dist, y + math.sin(a) * dist, z + up
    look = unreal.MathLibrary.find_look_at_rotation(unreal.Vector(cx, cy, cz), unreal.Vector(x, y, z + 40.0))
    cmd(w, "lyud.Birds.ViewFrom %.0f %.0f %.0f %.1f %.1f 70" % (cx, cy, cz, look.pitch, look.yaw))


STEPS = []


def step(fn, wait=3.0):
    STEPS.append((fn, wait))
    return fn


@step
def s_start(w):
    os.makedirs(OUT, exist_ok=True)
    cmd(w, "lyud.Birds.Time 10 0")
    cmd(w, "Inventory.Equip none")
    place(w, ACADEM[0], ACADEM[1], -10, 0)
    log("start " + report(w))


# --- perches: one minute with the check off, one with it on, an audit after each (near the club's garden and here) ---
@step
def s_perch_off(w):
    cmd(w, "lyud.Birds.PerchCheck 0")
    cmd(w, "lyud.Birds.Avoid 1")
    place(w, DOOR[0], DOOR[1] + 900.0, -5, 90)


step(lambda w: None, 75.0)


@step
def s_perch_off_audit(w):
    cmd(w, "lyud.Birds.PerchAudit 60")
    log("perch check off: see the BIRDPERCH line above")


@step
def s_perch_on(w):
    cmd(w, "lyud.Birds.Time 10 30")       # (a jump: everybody resettles, so both rounds start alike)
    cmd(w, "lyud.Birds.PerchCheck 1")


step(lambda w: None, 75.0)


@step
def s_perch_on_audit(w):
    cmd(w, "lyud.Birds.PerchAudit 60")
    log("perch check on: see the BIRDPERCH line above; " + report(w))


# --- the feed in the hand at the Academgorodok ---
@step
def s_feed(w):
    cmd(w, "lyud.Birds.Time 9 30")
    place(w, ACADEM[0], ACADEM[1], -12, 0)
    cmd(w, "Inventory.Give %s 5" % FEED)
    cmd(w, "Inventory.Equip %s" % FEED)
    st["feed_t"] = st["t"]
    log("holding %s" % FEED)


for _k in range(12):
    def _watch(w, k=_k):
        log("feed +%2ds: at the hand %d %s" % ((k + 1) * 5, birds(w).num_at_feed(False), report(w)))
        if k in (3, 7, 11):
            view_from_side(w)
    step(_watch, 2.0)

    def _snap(w, k=_k):
        if k in (3, 7, 11):
            shot(w, "feed_%02ds_side" % ((k + 1) * 5))
    step(_snap, 3.0)


@step
def s_feed_eyes(w):
    cmd(w, "lyud.Birds.ViewFrom")
    unreal.GameplayStatics.get_player_controller(w, 0).set_control_rotation(unreal.Rotator(roll=0, pitch=-28, yaw=180))


@step
def s_feed_eyes_shot(w):
    shot(w, "feed_eyes")


# --- flying round the player: the ring of birds keeps coming and going; through-the-body count with the swerve off, then on ---
@step
def s_avoid_off(w):
    st["intr0"] = intrusions(w)
    cmd(w, "lyud.Birds.Avoid 0")


step(lambda w: None, 40.0)


@step
def s_avoid_on(w):
    st["intr1"] = intrusions(w)
    cmd(w, "lyud.Birds.Avoid 1")


step(lambda w: None, 40.0)


@step
def s_avoid_report(w):
    i2 = intrusions(w)
    log("through the player in 40 s: swerve off %d, on %d" % (st["intr1"] - st["intr0"], i2 - st["intr1"]))


def intrusions(w):
    r = birds(w).report()
    i = r.find("through the player ")
    return int(r[i + len("through the player "):].split(";")[0].split(" ")[0]) if i >= 0 else -1


# --- the palm: one of the waiting birds comes to it ---
@step
def s_offer(w):
    view_from_side(w, 420.0, 160.0, 160.0)
    cmd(w, "Tame.Offer 1")
    st["offer_t"] = st["t"]


for _k in range(8):
    def _offer(w, k=_k):
        log("palm +%2ds: at the hand %d %s" % ((k + 1) * 4, birds(w).num_at_feed(False), report(w)))
        shot(w, "palm_%02ds" % ((k + 1) * 4))
    step(_offer, 4.0)


@step
def s_offer_end(w):
    cmd(w, "Tame.Offer 0")
    cmd(w, "lyud.Birds.ViewFrom")
    cmd(w, "Inventory.Equip none")


step(lambda w: log("feed put away: at the hand %d" % birds(w).num_at_feed(False)), 30.0)
step(lambda w: log("30 s after: at the hand %d %s" % (birds(w).num_at_feed(False), report(w))), 1.0)


# --- far birds: pops with the old range (700 lengths, no far silhouettes), then the new one, looking out over the town ---
def pops(w):
    r = birds(w).report()
    i = r.find("pops ")
    return int(r[i + 5:].split(",")[0].split(" ")[0]) if i >= 0 else -1


@step
def s_far_old(w):
    cmd(w, "lyud.Birds.Time 17 0")       # the starlings gather and fly to the roost: the sky is full of them
    place(w, DOOR[0], DOOR[1] + 900.0, 8, 60)
    cmd(w, "lyud.Birds.DrawDistance 700")
    cmd(w, "lyud.Birds.FullDetail 100000")
    st["pops0"] = pops(w)


step(lambda w: shot(w, "far_old"), 50.0)


@step
def s_far_new(w):
    st["pops1"] = pops(w)
    cmd(w, "lyud.Birds.DrawDistance 1200")
    cmd(w, "lyud.Birds.FullDetail 350")


step(lambda w: shot(w, "far_new"), 50.0)


@step
def s_far_report(w):
    p2 = pops(w)
    log("pops in 50 s: old range %d, new range %d; %s" % (st["pops1"] - st["pops0"], p2 - st["pops1"], report(w)))
    log("last pops: " + birds(w).report().split("(last pops: ")[-1][:500])


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
        log("step %s failed: %s" % (getattr(fn, "__name__", "?"), e))
    st["t0"] = st["t"]
    st["wait"] = wait


unreal.register_slate_post_tick_callback(tick)
