"""Offscreen check of the diner by the lower station (docs/diner.md) in Lvl_Scene01: the car from outside by day and at night (open,
lit; closed, dark and locked), inside (from the door, along the counter, the nose, the kitchen), a dish served and eaten with the
fork (small and big bites, the fork at the plate and at the lips), the coffee poured at the coffee table and drunk, the burger in
the hand; the look of the room by day and in the evening (DINER_STEPS=look, evening); sitting down on a booth, a stool and the nose's
bench with E (lyud.Diner.Use, the same path as the key), a dish served on the booth's table and eaten seated, getting up to the
aisle (sit). DINERTEST lines; screenshots in the order of the 'shot' lines.        unreal/scripts/test_diner.sh
"""
import json
import math
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools"))
import world_map as W  # noqa: E402
import terrain as T  # noqa: E402

L = json.load(open(os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "Diner", "layout.json")))
FACE_Y = 790.0
S = {"shots": 0}
ONLY = [s for s in os.environ.get("DINER_STEPS", "").split(",") if s]


def log(msg):
    unreal.log_warning("DINERTEST " + msg)


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_pawn(w, 0):
            return w
    return None


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def shot(w, name):
    log("shot %02d %s" % (S["shots"], name))
    S["shots"] += 1
    cmd(w, "shot showui")


def go(w, at, look, eye=1.62, outside=False):
    """The hero to at (diner frame metres; outside: z over the ground) looking at look; BugItGo puts the camera there."""
    mx, my = W.diner_to_map(at[0], at[1])
    base = T.ground_z(mx, my) if outside else L["floor_z"]
    x, y, z = W.to_unreal(mx, my, base + eye, FACE_Y)
    tx, ty = W.diner_to_map(look[0], look[1])
    tz = (T.ground_z(tx, ty) if outside else L["floor_z"]) + look[2] if len(look) > 2 else base + eye
    gx, gy, gz = W.to_unreal(tx, ty, tz, FACE_Y)
    yaw = math.degrees(math.atan2(gy - y, gx - x))
    pitch = math.degrees(math.atan2(gz - z, math.hypot(gx - x, gy - y)))
    cmd(w, "BugItGo %.1f %.1f %.1f %.2f %.2f 0" % (x, y, z - 64.0, pitch, yaw))


def hero_where(w):
    p = unreal.GameplayStatics.get_player_pawn(w, 0)
    if p:
        v = p.get_actor_location()
        log("hero at %.0f %.0f %.0f" % (v.x, v.y, v.z))


def seat_of(kind, pick):
    seats = [s_ for s_ in L.get("hero_seats", []) if s_["kind"] == kind]
    return pick(seats) if seats else None


def aim_at_seat(w, se, back=0.75):
    """Stands in front of seat se (its facing's way, back metres off its front edge) looking at its cushion, as one does before sitting."""
    a = math.radians(se["yaw"])
    fx, fy = math.cos(a), math.sin(a)
    cx, cy, cz = se["at"][0] - fx * 0.2, se["at"][1] - fy * 0.2, se["seat_z"]
    if se["kind"] == "booth":
        go(w, (cx, -1.3), (cx, cy, cz))          # the booth's way in is from the aisle, across
    else:
        go(w, (se["at"][0] + fx * back, se["at"][1] + fy * back), (cx, cy, cz))


VIEWS = [
    ("door", (0.0, -1.7), (3.5, 1.0, 1.0)),
    ("counter", (-6.0, -0.9), (4.0, 0.6, 1.0)),
    ("booths", (6.6, -0.8), (-3.0, -1.9, 0.9)),
    ("nose", (5.5, 0.4), (9.5, 0.0, 0.9)),
    ("menu", (0.4, -0.6), (0.4, 2.6, 1.75)),
    ("booth_close", (-0.3, -1.25), (-1.8, -2.3, 0.75)),
    ("stools_close", (-4.6, -0.75), (-1.0, 0.3, 0.75)),
    ("counter_top", (-1.0, -0.35), (0.2, 0.7, 1.0)),
    ("backbar", (-2.2, 0.2), (-3.9, 2.4, 1.3)),
    ("grill", (3.6, 0.1), (5.2, 2.4, 1.3)),
    ("coffee_table", (-5.4, -1.0), (-7.0, -1.0, 1.0)),
    ("kitchen", (-3.4, 5.4), (-6.4, 3.0, 1.0)),
]


def plan():
    t = 80.0
    out = []

    def add(dt, f):
        nonlocal t
        out.append((t, f))
        t += dt

    def step(name):
        return not ONLY or name in ONLY
    add(0.5, lambda w: cmd(w, "lyud.Diner.Open 1"))
    add(1.0, lambda w: cmd(w, "Inventory.Earn 300"))
    if step("outside"):
        add(3.0, lambda w: go(w, (-9.0, -15.0), (0.5, 0.0, 1.3), outside=True))
        add(2.0, lambda w: shot(w, "front_day"))
        add(3.0, lambda w: go(w, (16.5, -7.0), (6.0, 0.0, 1.4), outside=True))
        add(2.0, lambda w: shot(w, "nose_day"))
        add(3.0, lambda w: go(w, (-10.0, 12.0), (-3.0, 3.0, 1.2), outside=True))
        add(2.0, lambda w: shot(w, "back_day"))
        add(3.0, lambda w: go(w, (0.0, -6.0), (0.0, -2.7, 1.4), outside=True))
        add(2.0, lambda w: shot(w, "door_day"))
    if step("inside"):
        add(3.0, lambda w: go(w, (0.0, -1.7), (3.5, 1.0, 1.0)))
        add(2.0, lambda w: shot(w, "in_door"))
        add(3.0, lambda w: go(w, (-6.0, -0.9), (4.0, 0.6, 1.0)))
        add(2.0, lambda w: shot(w, "in_counter"))
        add(3.0, lambda w: go(w, (6.6, -0.8), (-3.0, -1.9, 0.9)))
        add(2.0, lambda w: shot(w, "in_booths"))
        add(3.0, lambda w: go(w, (5.5, 0.4), (9.5, 0.0, 0.9)))
        add(2.0, lambda w: shot(w, "in_nose"))
        add(3.0, lambda w: go(w, (0.4, -0.6), (0.4, 2.6, 1.75)))
        add(2.0, lambda w: shot(w, "in_menu"))
        add(3.0, lambda w: go(w, (-3.4, 5.4), (-6.4, 3.0, 1.0)))
        add(2.0, lambda w: shot(w, "in_kitchen"))
    for when, clock in (("look", "12 30"), ("evening", "17 50")):
        if step(when):
            add(1.0, lambda w, c=clock: cmd(w, "lyud.Birds.Time " + c))
            for name, at, look in VIEWS:
                add(3.0, lambda w, at=at, look=look: go(w, at, look))
                add(2.0, lambda w, n=name, wh=when: shot(w, "%s_%s" % (wh, n)))
    if step("sit"):
        add(1.0, lambda w: cmd(w, "lyud.Birds.Time 12 30"))
        add(1.0, lambda w: cmd(w, "lyud.Diner.Seats"))
        booth = seat_of("booth", lambda ss: max((x for x in ss if x["at"][1] > -1.5 and 0.9 < x["at"][0] < 2.7), key=lambda x: x["at"][0], default=ss[0]))
        stool = seat_of("stool", lambda ss: ss[len(ss) // 2])
        bench = seat_of("bench", lambda ss: ss[len(ss) // 2])
        if booth:
            add(3.0, lambda w: aim_at_seat(w, booth))
            add(1.5, lambda w: shot(w, "sit_booth_before"))
            add(0.5, lambda w: cmd(w, "lyud.Diner.Use"))
            add(3.0, lambda w: shot(w, "sit_booth_seated"))
            add(0.5, lambda w: hero_where(w))
            add(1.0, lambda w: cmd(w, "lyud.Diner.Serve diner_pancakes"))
            add(2.5, lambda w: shot(w, "sit_booth_served"))
            add(0.5, lambda w: cmd(w, "slomo 0.2"))
            add(0.5, lambda w: cmd(w, "lyud.Diner.Eat"))
            add(3.5, lambda w: shot(w, "sit_booth_fork"))
            add(4.0, lambda w: shot(w, "sit_booth_mouth"))
            add(2.0, lambda w: cmd(w, "slomo 1"))
            add(4.0, lambda w: cmd(w, "lyud.Bite.Stop"))
            add(1.0, lambda w: cmd(w, "lyud.Diner.Use"))
            add(1.5, lambda w: hero_where(w))
            add(1.0, lambda w: shot(w, "sit_booth_stood"))
        if stool:
            add(3.0, lambda w: aim_at_seat(w, stool))
            add(0.5, lambda w: cmd(w, "lyud.Diner.Use"))
            add(3.0, lambda w: shot(w, "sit_stool_seated"))
            add(1.0, lambda w: cmd(w, "lyud.Diner.Use"))
            add(1.5, lambda w: hero_where(w))
        if bench:
            add(3.0, lambda w: aim_at_seat(w, bench))
            add(0.5, lambda w: cmd(w, "lyud.Diner.Use"))
            add(3.0, lambda w: shot(w, "sit_bench_seated"))
            add(1.0, lambda w: cmd(w, "lyud.Diner.Use"))
            add(1.5, lambda w: hero_where(w))
    if step("eat"):
        sv = L["counter"]["serving"][5]
        stand = sv["stand"]
        plate = sv["plate"]
        # looking at the plate the way one does at a counter: down at it from the stool, not straight down
        add(3.0, lambda w: go(w, (stand[0], stand[1] - 0.05), (plate[0], plate[1] + 1.2, plate[2] - 0.1)))
        add(1.0, lambda w: cmd(w, "lyud.Diner.Serve diner_spaghetti"))
        add(2.0, lambda w: shot(w, "served"))
        # slowed down five times so the shots land in the right phase whatever the offscreen frame rate
        add(0.5, lambda w: cmd(w, "slomo 0.2"))
        add(0.5, lambda w: cmd(w, "lyud.Diner.Eat"))
        add(3.0, lambda w: shot(w, "fork_plate_small"))
        add(3.7, lambda w: shot(w, "fork_mouth_small"))
        add(4.0, lambda w: cmd(w, "lyud.Bite.Take big"))
        add(3.6, lambda w: shot(w, "fork_plate_big"))
        add(4.3, lambda w: shot(w, "fork_mouth_big"))
        add(3.0, lambda w: cmd(w, "slomo 1"))
        for _ in range(3):
            add(2.6, lambda w: cmd(w, "lyud.Bite.Take big"))
        add(3.0, lambda w: shot(w, "after_bites"))
        add(1.0, lambda w: cmd(w, "lyud.Bite.Stop"))
    if step("coffee"):
        st = L["coffee"]["stand"]
        tap = L["coffee"]["tap"]
        add(3.0, lambda w: go(w, (st[0], st[1]), (tap[0], tap[1], tap[2])))
        add(1.5, lambda w: shot(w, "coffee_prompt"))
        add(1.25, lambda w: cmd(w, "lyud.Diner.Coffee"))
        add(1.5, lambda w: shot(w, "coffee_carry"))
        add(1.2, lambda w: shot(w, "coffee_pour"))
        add(3.5, lambda w: shot(w, "coffee_poured"))
        add(0.5, lambda w: cmd(w, "slomo 0.2"))
        add(0.5, lambda w: cmd(w, "lyud.Diner.Eat"))
        add(0.1, lambda w: cmd(w, "lyud.Bite.Take big"))
        add(6.0, lambda w: shot(w, "coffee_sip"))
        add(3.0, lambda w: cmd(w, "slomo 1"))
        add(3.0, lambda w: cmd(w, "lyud.Bite.Stop"))
    if step("burger"):
        sv = L["counter"]["serving"][7]
        add(3.0, lambda w: go(w, (sv["stand"][0], sv["stand"][1] - 0.05), tuple(sv["plate"])))
        add(1.0, lambda w: cmd(w, "lyud.Diner.Serve diner_burger"))
        add(2.0, lambda w: shot(w, "burger_served"))
        add(0.5, lambda w: cmd(w, "lyud.Diner.Eat"))
        add(2.0, lambda w: shot(w, "burger_hand"))
        add(0.5, lambda w: cmd(w, "slomo 0.2"))
        add(0.5, lambda w: cmd(w, "lyud.Hand.Press 1"))
        add(4.0, lambda w: shot(w, "burger_mouth"))
        add(0.5, lambda w: cmd(w, "slomo 1"))
        add(1.5, lambda w: cmd(w, "lyud.Hand.Press 0"))
        add(2.0, lambda w: shot(w, "burger_bitten"))
        add(0.5, lambda w: cmd(w, "lyud.Hand.PutAway"))
    if step("night"):
        add(0.5, lambda w: cmd(w, "lyud.Birds.Time 21 30"))
        add(4.0, lambda w: go(w, (-9.0, -15.0), (0.5, 0.0, 1.3), outside=True))
        add(3.0, lambda w: shot(w, "front_night_open"))
        add(0.5, lambda w: cmd(w, "lyud.Diner.Open 0"))
        add(3.0, lambda w: cmd(w, "lyud.Diner.State"))
        add(1.0, lambda w: shot(w, "front_night_closed"))
        add(0.5, lambda w: cmd(w, "lyud.Door.List"))
    add(1.0, lambda w: cmd(w, "lyud.Diner.State"))
    add(1.0, lambda w: log("done"))
    add(1.0, lambda w: cmd(w, "quit"))
    return out


STATE = {"t": 0.0, "i": 0, "plan": []}


def tick(dt):
    w = world()
    if not w:
        return
    STATE["t"] += min(dt, 0.25)
    if not STATE["plan"]:
        STATE["plan"] = plan()
        cmd(w, "lyud.Language %s" % os.environ.get("DINER_LANG", "ru"))
        cmd(w, "lyud.Birds.Time 12 30")
    P = STATE["plan"]
    while STATE["i"] < len(P) and STATE["t"] > P[STATE["i"]][0]:
        P[STATE["i"]][1](w)
        STATE["i"] += 1


unreal.register_slate_post_tick_callback(tick)
