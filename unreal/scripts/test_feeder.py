"""Offscreen check of the bird feeder and the grocery's feeds (docs/bird_taming.md "Feeds and the feeder"), in
/Game/Dev/L_FeederTest (unreal/scripts/make_feeder_test_level.py: the befriending lawn and the grocery pavilion) with the
real bird life (lyud.Birds.AnyMap=1; the lawn lies round the club door of the birds' map). The clock goes to 9:00 (lyud.Birds.Time)
that morning (birds feed in the morning; a feeder put up late in the afternoon stays empty till the next day); the hero sets a
feeder up by the garden 18 m from the club door (where tools/birdsim's check puts it), fills it with suet, walks off 9 m
and the game runs at slomo 8 (the bird life steps at most 0.25 s a frame, so it runs some 6x) while a camera beside the
feeder (lyud.Feeder.Cam) looks at the tray. FEEDER lines (lyud.Feeder.Report) every few seconds; then a feed packet's
front (lyud.Goods.Show) for each feed, the inventory with the new icons, and the pavilion outside and in.
FEEDERTEST lines, screenshots in order.

  sh unreal/scripts/test_feeder.sh        (FEED=feed_grain, SLOMO=8, MINUTES=20, AGE=0 change the run)
"""
import os

import unreal

FEED = os.environ.get("FEED", "feed_suet")
SLOMO = float(os.environ.get("SLOMO", "8"))
WATCH = float(os.environ.get("MINUTES", "20")) * 60.0    # real seconds the feeder is watched
AGE = float(os.environ.get("AGE", "0"))                  # game hours as if the feed went in that long ago
SPOT = (900.0, 2600.0)                                   # the hero, the feeder 1.2 m ahead of him (+Y)
FEEDS = ["feed_suet", "feed_niger", "feed_rowan", "feed_mealworm", "feed_grain", "feed_peanuts"]

state = {"t": 0.0, "step": 0, "shots": 0, "next_report": 0.0, "next_shot": 0.0, "watch": None, "cam": None}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "L_FeederTest" and unreal.GameplayStatics.get_player_pawn(w, 0):
            return w
    return None


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def say(msg):
    unreal.log_warning("FEEDERTEST " + msg)


def shot(w, name):
    say("shot %02d %s" % (state["shots"], name))
    state["shots"] += 1
    cmd(w, "shot showui")


def pc(w):
    return unreal.GameplayStatics.get_player_controller(w, 0)


def face(yaw, pitch=0.0):
    def go(w):
        pc(w).set_control_rotation(unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
    return go


# the pavilion (make_feeder_test_level.py): at PAV unrotated, its frame's x along it at 93 deg, the front at y = -7.25
PAV = (-1500.0, -2500.0, 0.0)


def pav(x, y, z):
    """A point of the pavilion's own frame (m) on the lawn, cm."""
    import math
    a = math.radians(93.0)
    bx = x * math.cos(a) - y * math.sin(a)
    by = x * math.sin(a) + y * math.cos(a)
    return (PAV[0] + bx * 100.0, PAV[1] - by * 100.0, PAV[2] + z * 100.0)


def go_look(frm, at):
    """BugItGo puts the view (eyes ~ +65 cm over the pawn's middle) at frm looking at at."""
    import math
    dx, dy, dz = at[0] - frm[0], at[1] - frm[1], at[2] - frm[2]
    yaw = math.degrees(math.atan2(dy, dx))
    pitch = math.degrees(math.atan2(dz, math.hypot(dx, dy)))
    return "BugItGo %.0f %.0f %.0f %.1f %.1f 0" % (frm[0], frm[1], frm[2] - 65.0, pitch, yaw)


def watch(on):
    def go(w):
        state["watch"] = state["t"] if on else None
        cmd(w, "slomo %g" % (SLOMO if on else 1.0))
        if not on:
            cmd(w, "lyud.Feeder.Cam off")
    return go


PLAN = [
    (1.0, "lyud.Birds.Time 9"),
    (2.0, "BugItGo %.0f %.0f 100 0 90 0" % SPOT),
    (5.0, face(90.0)),
    (5.5, "lyud.Feeder.Place"),
    (6.0, "lyud.Feeder.Fill %s 5" % FEED),
    (6.2, "lyud.Feeder.Age %g" % AGE),
    (6.4, "lyud.Feeder.Report"),
    (6.6, lambda w: shot(w, "feeder_placed")),
    # off 9 m from it (to the south), then stand still
    (7.0, "Tame.Walk %.0f %.0f 80 300 0 360 0" % (SPOT[0], SPOT[1] - 900.0)),
    (12.0, "Tame.Walk off"),
    (12.2, "lyud.Feeder.Cam"),
    (13.5, lambda w: shot(w, "feeder_cam_start")),
    (14.0, watch(True)),
]


def after_watch():
    plan = [(0.5, watch(False)), (1.0, "Tame.Walk %.0f %.0f 80 300 0 360 0" % (SPOT[0], SPOT[1] - 60.0)), (4.0, "Tame.Walk off"),
            (4.2, face(90.0, -25.0)),
            (5.0, "lyud.Feeder.Report"), (5.5, lambda w: shot(w, "feeder_from_hero"))]
    t = 6.5
    for f in FEEDS:
        plan += [(t, face(0.0, 0.0)), (t + 0.1, "lyud.Goods.Show %s 32" % f), (t + 1.5, (lambda name: lambda w: shot(w, name))("packet_" + f))]
        t += 2.0
    plan += [(t, "lyud.Goods.Show off")]
    for f in FEEDS + ["feeder", "bread_loaf", "kefir"]:
        plan.append((t + 0.1, "Inventory.Give %s %d" % (f, 1 if f in ("feeder", "bread_loaf", "kefir") else 5)))
    plan += [(t + 0.5, "Inventory.Open"), (t + 2.5, lambda w: shot(w, "inventory")), (t + 3.0, "Inventory.Open 0")]
    t += 4.0
    shots = [("pavilion_front", pav(-2.0, -18.0, 2.2), pav(4.0, -7.25, 2.2)),
             ("pavilion_sign", pav(0.0, -11.0, 2.0), pav(0.0, -7.25, 3.95)),
             ("pavilion_roof", pav(-8.0, -16.0, 7.0), pav(8.0, -2.0, 4.5)),
             ("hall_north", pav(3.0, -3.5, 1.7), pav(14.0, -2.0, 1.6)),
             ("hall_south", pav(15.0, -3.0, 1.7), pav(0.0, -2.0, 1.4)),
             ("hall_back", pav(8.0, -5.5, 1.7), pav(8.0, 1.5, 2.0))]
    for name, frm, at in shots:
        plan += [(t, go_look(frm, at)), (t + 2.5, (lambda n: lambda w: shot(w, n))(name))]
        t += 3.0
    plan.append((t + 0.5, None))
    return plan


def tick(dt):
    w = world()
    if not w:
        return
    state["t"] += min(dt, 0.25)
    t = state["t"]
    if state["watch"] is not None:
        el = t - state["watch"]
        if t >= state["next_report"]:
            state["next_report"] = t + 4.0
            say("t %.0f s (%.1f game min)" % (el, el * SLOMO / 60.0))
            cmd(w, "lyud.Feeder.Report")
        if t >= state["next_shot"]:
            state["next_shot"] = t + 20.0
            shot(w, "tray_%03dmin" % int(el * SLOMO / 60.0))
        if el > WATCH:
            state["post"] = [(t + dt_, a) for dt_, a in after_watch()]
            state["step"] = 0
            state["watch"] = None
        return
    plan = state.get("post", PLAN)
    while state["step"] < len(plan) and t > plan[state["step"]][0]:
        a = plan[state["step"]][1]
        state["step"] += 1
        if a is None:
            say("done")
            cmd(w, "quit")
            return
        if callable(a):
            a(w)
        else:
            say(a)
            cmd(w, a)


unreal.register_slate_post_tick_callback(tick)
