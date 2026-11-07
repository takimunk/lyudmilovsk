"""Offscreen check of the restaurant «Янтарь» (docs/restaurant.md) in Lvl_Scene01: the pavilion from the promenade and the sea by day,
inside; the hero sits at a table (the seated eye height), opens the menu card, orders; the waiter walks from the kitchen pass with the
tray, sets the dishes down; eating seated with a spoon, chopsticks, the fingers, a fork and knife, a cup; the empty plates cleared; the
bill; the pavilion at night, open and lit, then closed. RESTTEST lines; screenshots in the order of the 'shot' lines.
REST_STEPS=outside,sit,menu,order,waiter_walk,serve,eat,clear,bill,night (any of them), REST_LANG=ru|en|ja.
                                                                                        unreal/scripts/test_restaurant.sh
"""
import json
import math
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
CONTENT = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content")
L = json.load(open(os.path.join(CONTENT, "Restaurant", "layout.json")))
CFG = json.load(open(os.path.join(CONTENT, "Restaurant", "restaurant.json")))
CITY = json.load(open(os.path.join(CONTENT, "City", "city_plan.json")))
PLOT = next(p for p in CITY["plots"] if p["id"] == CFG.get("plot", "P-RESTAURANT"))
UX, UY, GROUND = PLOT["ue"][0], PLOT["ue"][1], PLOT["ue"][2]
FLOOR = GROUND + 100.0 * CFG.get("podium", 0.6)
YAW = PLOT["yaw"]
S = {"shots": 0}
ONLY = [s for s in os.environ.get("REST_STEPS", "").split(",") if s]
WAIT = float(os.environ.get("REST_WAIT", "0.12"))     # the cooking waits scaled down for the test


def log(msg):
    unreal.log_warning("RESTTEST " + msg)


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


def ue(p, base=None):
    """The building's frame (m; Blender: +x the sea, y across, z over the floor) -> Unreal cm."""
    c, s = math.cos(math.radians(YAW)), math.sin(math.radians(YAW))
    x, y = p[0] * 100.0, -p[1] * 100.0
    z = (FLOOR if base is None else base) + (p[2] if len(p) > 2 else 0.0) * 100.0
    return UX + x * c - y * s, UY + x * s + y * c, z


def go(w, at, look, eye=1.62, outside=False):
    """The hero to at (frame metres; outside: eye over the ground under the podium) looking at look (frame m, z over the floor)."""
    base = GROUND if outside else FLOOR
    x, y, z = ue((at[0], at[1], eye), base)
    gx, gy, gz = ue(look)
    yaw = math.degrees(math.atan2(gy - y, gx - x))
    pitch = math.degrees(math.atan2(gz - z, math.hypot(gx - x, gy - y)))
    cmd(w, "BugItGo %.1f %.1f %.1f %.2f %.2f 0" % (x, y, z - 64.0, pitch, yaw))


def hero_seat():
    """The first seat of the first hero table with a sea view (the largest x), its index among all seats."""
    k = 0
    best = None
    for t in L["tables"]:
        for i, s in enumerate(t["seats"]):
            if t.get("hero") and (best is None or t["at"][0] > best[1]["at"][0]):
                if i == 0:
                    best = (k, t, s)
            k += 1
    return best


def plan():
    t = 80.0
    out = []

    def add(dt, f):
        nonlocal t
        out.append((t, f))
        t += dt

    def step(name):
        return not ONLY or name in ONLY
    seat_ix, table, seat = hero_seat()
    pass_at = L["pass"]["waiter"]
    add(0.5, lambda w: cmd(w, "lyud.Restaurant.Open 1"))
    add(0.5, lambda w: cmd(w, "lyud.Restaurant.WaitScale %.3f" % WAIT))
    add(1.0, lambda w: cmd(w, "Inventory.Earn 600"))
    add(1.0, lambda w: cmd(w, "lyud.Restaurant.State"))
    if step("outside"):
        add(3.0, lambda w: go(w, (6.0, -30.0), (0.0, 0.0, 2.5), outside=True))
        add(2.0, lambda w: shot(w, "promenade_day"))
        add(3.0, lambda w: go(w, (32.0, 8.0), (0.0, 0.0, 2.5), outside=True))
        add(2.0, lambda w: shot(w, "sea_side_day"))
        add(3.0, lambda w: go(w, (0.0, -15.5), (0.0, -10.0, 1.6), outside=True))
        add(2.0, lambda w: shot(w, "door_day"))
        add(3.0, lambda w: go(w, (0.0, -8.6), (3.0, 4.0, 1.0)))
        add(2.0, lambda w: shot(w, "in_door"))
        add(3.0, lambda w: go(w, (6.0, 3.0), (pass_at[0], pass_at[1], 1.4)))
        add(2.0, lambda w: shot(w, "in_pass"))
    if step("sit"):
        sa = seat["at"]
        add(3.0, lambda w: go(w, (sa[0] - 0.9 * math.cos(math.radians(seat["yaw"])), sa[1] - 0.9 * math.sin(math.radians(seat["yaw"]))), table["at"]))
        add(2.0, lambda w: shot(w, "chair_prompt"))
        add(1.0, lambda w: cmd(w, "lyud.Restaurant.Sit %d" % seat_ix))
        add(3.0, lambda w: shot(w, "seated"))
        add(0.5, lambda w: cmd(w, "lyud.Hero.ViewBehind 170"))
        add(2.0, lambda w: shot(w, "seated_behind"))
        add(0.5, lambda w: cmd(w, "lyud.Hero.ViewBehind 0"))
        add(1.0, lambda w: cmd(w, "lyud.Restaurant.State"))
    if step("menu"):
        add(1.0, lambda w: cmd(w, "lyud.Restaurant.Menu"))
        add(2.5, lambda w: shot(w, "menu"))
        add(1.0, lambda w: cmd(w, "lyud.Restaurant.Menu 0"))
    if step("order"):
        add(1.0, lambda w: cmd(w, "lyud.Restaurant.Order rs_borscht rs_ramen rs_khachapuri rs_steak rs_coffee"))
        add(1.0, lambda w: cmd(w, "lyud.Restaurant.State"))
    if step("waiter_walk"):
        # the bread and water come first; then the dishes off the pass
        add(0.5, lambda w: cmd(w, "lyud.Restaurant.Look %.2f %.2f 1.1" % (pass_at[0], pass_at[1])))
        add(6.0, lambda w: shot(w, "waiter_pass"))
        add(0.5, lambda w: cmd(w, "lyud.Hero.ViewBehind 260"))
        add(3.0, lambda w: shot(w, "waiter_walk"))
        add(3.0, lambda w: shot(w, "waiter_near"))
        add(0.5, lambda w: cmd(w, "lyud.Hero.ViewBehind 0"))
    if step("serve"):
        add(0.5, lambda w: cmd(w, "lyud.Restaurant.Look %.2f %.2f %.2f" % (table["at"][0], table["at"][1], table["at"][2])))
        add(16.0, lambda w: shot(w, "served_table"))
        add(14.0, lambda w: cmd(w, "lyud.Restaurant.State"))
        add(1.0, lambda w: shot(w, "all_served"))
    if step("eat"):
        add(0.5, lambda w: cmd(w, "slomo 0.25"))
        for item, label in (("rs_borscht", "spoon"), ("rs_ramen", "chopsticks"), ("rs_khachapuri", "fingers"), ("rs_steak", "knife_fork"), ("rs_coffee", "cup")):
            add(0.5, lambda w, i=item: cmd(w, "lyud.Restaurant.Eat %s" % i))
            add(0.1, lambda w: cmd(w, "lyud.Bite.Take big"))
            add(2.4, lambda w, n=label: shot(w, "eat_%s_dish" % n))
            add(2.6, lambda w, n=label: shot(w, "eat_%s_mouth" % n))
            add(4.0, lambda w: cmd(w, "lyud.Bite.Take big"))
            add(6.0, lambda w: cmd(w, "lyud.Bite.Stop"))
        add(0.5, lambda w: cmd(w, "slomo 1"))
        add(1.0, lambda w: shot(w, "after_bites"))
        add(1.0, lambda w: cmd(w, "lyud.Restaurant.State"))
    if step("clear"):
        for _ in range(4):
            add(0.3, lambda w: cmd(w, "lyud.Restaurant.Finish"))
        add(0.5, lambda w: cmd(w, "lyud.Restaurant.Finish rs_steak"))
        add(14.0, lambda w: shot(w, "clearing"))
        add(10.0, lambda w: cmd(w, "lyud.Restaurant.State"))
        add(1.0, lambda w: shot(w, "cleared"))
    if step("bill"):
        add(1.0, lambda w: cmd(w, "lyud.Restaurant.Bill"))
        add(18.0, lambda w: shot(w, "bill"))
        add(1.0, lambda w: cmd(w, "lyud.Restaurant.State"))
        add(1.0, lambda w: cmd(w, "lyud.Restaurant.Stand"))
        add(2.0, lambda w: shot(w, "stood_up"))
    if step("night"):
        add(0.5, lambda w: cmd(w, "lyud.Restaurant.Stand"))
        add(0.5, lambda w: cmd(w, "lyud.Birds.Time 21 40"))
        add(4.0, lambda w: go(w, (6.0, -30.0), (0.0, 0.0, 2.5), outside=True))
        add(3.0, lambda w: shot(w, "promenade_night_open"))
        add(3.0, lambda w: go(w, (34.0, 10.0), (0.0, 0.0, 2.5), outside=True))
        add(3.0, lambda w: shot(w, "sea_side_night_open"))
        add(0.5, lambda w: cmd(w, "lyud.Restaurant.Open 0"))
        add(3.0, lambda w: cmd(w, "lyud.Restaurant.State"))
        add(1.0, lambda w: shot(w, "night_closed"))
    add(1.0, lambda w: cmd(w, "lyud.Restaurant.State"))
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
        cmd(w, "lyud.Language %s" % os.environ.get("REST_LANG", "ru"))
        cmd(w, "lyud.Birds.Time 13 30")
    P = STATE["plan"]
    while STATE["i"] < len(P) and STATE["t"] > P[STATE["i"]][0]:
        P[STATE["i"]][1](w)
        STATE["i"] += 1


unreal.register_slate_post_tick_callback(tick)
