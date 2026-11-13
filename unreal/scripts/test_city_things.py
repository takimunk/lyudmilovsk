"""Offscreen check of the things to do inside the town's buildings (docs/city_interiors.md, ULyudCityThings) in Lvl_Scene01:
lists them (lyud.City.Things), then runs a few of each kind with lyud.City.Do from wherever the hero stands (a shelf, a board,
a counter, something to take twice, a seat, a bed), puts the hero in front of a shelf and takes a screenshot of the prompt
and of the page, saves and runs the shelf again. CITYTEST lines; the CityThings lines of LogLyudCity say what each did.
unreal/scripts/test_city_things.sh checks them.
"""
import json
import math
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
DATA = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "City", "interiors.json")
S = {"t": 0.0, "step": 0, "shots": 0}


def log(msg):
    unreal.log_warning("CITYTEST " + msg)


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_pawn(w, 0):
            return w
    return None


def cmd(w, c):
    log("cmd " + c)
    unreal.SystemLibrary.execute_console_command(w, c)


def shot(w, name):
    log("shot %02d %s" % (S["shots"], name))
    S["shots"] += 1
    unreal.SystemLibrary.execute_console_command(w, "shot showui")


def first_of(things, kind, n=1):
    return [t for t in things if t.get("kind") == kind][:n]


def look_at(w, t):
    """The hero 1.6 m in front of thing t (along its yaw), eyes 40 cm over its point, looking at it."""
    x, y, z = t["ue"]
    r = math.radians(t.get("yaw", 0.0))
    ex, ey, ez = x + math.cos(r) * 160.0, y + math.sin(r) * 160.0, z + 40.0
    yaw = math.degrees(math.atan2(y - ey, x - ex))
    pitch = math.degrees(math.atan2(z - ez, math.hypot(x - ex, y - ey)))
    cmd(w, "BugItGo %.1f %.1f %.1f %.2f %.2f 0" % (ex, ey, ez - 64.0, pitch, yaw))


def plan():
    things = []
    if os.path.exists(DATA):
        things = json.load(open(DATA, encoding="utf-8")).get("things", [])
    log("data %d things" % len(things))
    t = 20.0
    out = []

    def add(dt, f):
        nonlocal t
        out.append((t, f))
        t += dt

    add(1.0, lambda w: cmd(w, "lyud.City.Things"))
    add(1.0, lambda w: cmd(w, "Inventory.Earn 500"))
    for kind, n in (("shelf", 2), ("board", 2), ("counter", 2), ("machine", 1), ("play", 1), ("screen", 1)):
        for th in first_of(things, kind, n):
            add(1.5, lambda w, i=th["id"]: cmd(w, "lyud.City.Do " + i))
    for th in first_of(things, "take"):
        add(1.0, lambda w, i=th["id"]: cmd(w, "lyud.City.Do " + i))
        add(1.0, lambda w, i=th["id"]: cmd(w, "lyud.City.Do " + i))      # the second time today: daily_done
    shelves = first_of(things, "shelf")
    if shelves:
        sh = shelves[0]
        add(3.0, lambda w: look_at(w, sh))
        add(1.0, lambda w: cmd(w, "lyud.City.Things.Show 1"))
        add(1.5, lambda w: shot(w, "shelf_prompt"))
        add(1.5, lambda w: cmd(w, "lyud.City.Do " + sh["id"]))
        add(1.5, lambda w: shot(w, "shelf_page"))
        add(1.0, lambda w: cmd(w, "lyud.City.Things.Show 0"))
    for kind in ("seat", "bed"):
        for th in first_of(things, kind):
            add(3.0, lambda w, x=th: look_at(w, x))
            add(4.0, lambda w, i=th["id"]: cmd(w, "lyud.City.Do " + i))
            add(1.0, lambda w, k=kind: shot(w, k))
    add(2.0, lambda w: cmd(w, "lyud.Save citytest"))
    if shelves:
        add(1.5, lambda w: cmd(w, "lyud.City.Do " + shelves[0]["id"]))
    add(1.0, lambda w: cmd(w, "lyud.City.Do no/such_thing"))
    add(2.0, lambda w: log("done"))
    add(1.0, lambda w: cmd(w, "quit"))
    return out


PLAN = []


def tick(dt):
    w = world()
    if not w:
        return
    if not PLAN:
        PLAN.extend(plan())
    S["t"] += dt
    while S["step"] < len(PLAN) and S["t"] > PLAN[S["step"]][0]:
        PLAN[S["step"]][1](w)
        S["step"] += 1


unreal.register_slate_post_tick_callback(tick)
