"""Offscreen check of the things to do inside the town's buildings (docs/city_interiors.md, ULyudCityThings) in Lvl_Scene01:
lists them (lyud.City.Things), then runs a few of each kind with lyud.City.Do from wherever the hero stands (a shelf, a board,
a counter, something to take twice, a seat, a bed), puts the hero in front of a shelf and takes a screenshot of the prompt
and of the page, sits and lies on the nearest seat and bed (a save and a load while seated, a sleep to 7:00), reads a text of
the reading view to its end and lists what was read, opens the staff talk at a counter and asks a topic (the list, the reset),
logs the room tone, takes a lift (lyud.City.Go), saves and runs the shelf again. CITYTEST lines; the CityThings lines of LogLyudCity say what each did.
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
            if kind == "counter":
                add(0.5, lambda w: cmd(w, "Inventory.Open 0"))    # a shop left open would hide every later step
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
    # second pass: a daily board twice (the same notice), the telescope by day and by night, a show's subtitles, the
    # 32.85 m bench of the bridge house (sat at its real height), the weekday command, open-all-hours
    acts = json.load(open(DATA, encoding="utf-8")).get("acts", {}) if os.path.exists(DATA) else {}
    daily = [t for t in things if acts.get(t.get("act"), {}).get("rotate") == "daily"][:1]
    for th in daily:
        add(1.0, lambda w, i=th["id"]: cmd(w, "lyud.City.Do " + i))
        add(1.0, lambda w, i=th["id"]: cmd(w, "lyud.City.Do " + i))
    scope = [t for t in things if t.get("act") == "telescope"][:1]
    for th in scope:
        add(1.0, lambda w: cmd(w, "lyud.Birds.Time 12"))
        add(1.0, lambda w, i=th["id"]: cmd(w, "lyud.City.Do " + i))
        add(1.0, lambda w: cmd(w, "lyud.Birds.Time 23"))
        add(1.0, lambda w, i=th["id"]: cmd(w, "lyud.City.Do " + i))
        add(1.0, lambda w: cmd(w, "lyud.Birds.Time 16"))
    add(1.0, lambda w: cmd(w, "lyud.City.Weekday wed"))
    show = [t for t in things if acts.get(t.get("act"), {}).get("screen") and "height" in t.get("props", {})][:1]
    for th in show:
        add(1.0, lambda w: cmd(w, "Dev.Do interiors.open_all_hours"))
        add(3.0, lambda w, x=th: look_at(w, x))
        add(3.0, lambda w, i=th["id"]: cmd(w, "lyud.City.Do " + i))
        add(1.0, lambda w: shot(w, "screen_show"))
        add(6.0, lambda w: shot(w, "screen_show_later"))
        add(1.0, lambda w: cmd(w, "Dev.Do interiors.open_all_hours"))
    bench = [t for t in things if t["id"] == "P-BRIDGEHOUSE/gallery_bench_e1"]
    for th in bench:
        add(3.0, lambda w, x=th: look_at(w, x))
        add(2.0, lambda w, i=th["id"]: cmd(w, "lyud.City.Do " + i))
        add(1.0, lambda w: shot(w, "bridge_bench"))
    # third pass: a stamp (a library topic read), the collections' page, a shut café's chair (its closed act at 23:00)
    stamped = [t for t in things if acts.get(t.get("act"), {}).get("collect", "").startswith("shelves/")][:1]
    for th in stamped:
        add(1.5, lambda w, i=th["id"]: cmd(w, "lyud.City.Do " + i))
    add(1.5, lambda w: cmd(w, "lyud.City.Collections"))
    add(1.0, lambda w: shot(w, "collections"))
    add(1.0, lambda w: cmd(w, "lyud.City.Collections grant kiosks"))
    add(1.0, lambda w: cmd(w, "lyud.City.Collections clear kiosks"))
    shut = [t for t in things if t.get("closed") and t["kind"] == "seat"][:1]
    for th in shut:
        add(1.0, lambda w: cmd(w, "lyud.Birds.Time 23"))
        add(1.5, lambda w, i=th["id"]: cmd(w, "lyud.City.Do " + i))
        add(1.0, lambda w: cmd(w, "lyud.Birds.Time 16"))
    # fourth pass: one mini-game of each type (docs "Mini-games"): started from here, a few presses, won at once; the log says
    # "game aim|rally|duel on ...", "game beat ..." and "game over on ...: win"
    for gtype in ("aim", "rally", "duel"):
        game = [t for t in things if ((acts.get(t.get("act"), {}).get("game") or {}).get("type")) == gtype][:1]
        for th in game:
            add(1.5, lambda w, i=th["id"]: cmd(w, "lyud.City.Game " + i))
            add(1.0, lambda w, g=gtype: shot(w, "game_" + g))
            for n in range(3):
                add(0.7, lambda w, g=gtype, k=n: cmd(w, "lyud.City.Game press" + (" %d" % (k + 1) if g == "duel" else "")))
            add(2.5, lambda w: cmd(w, "lyud.City.Game win"))
            add(1.0, lambda w, g=gtype: shot(w, "game_%s_over" % g))
    add(1.0, lambda w: cmd(w, "lyud.City.Game regulars"))
    # fifth pass (docs "Sitting and lying", "The reading page"): the nearest seat (twice: the second stands him up first), a
    # save and a load while seated (he must stand, never stuck), the nearest bed and a sleep to 7:00, a text opened by id and
    # leafed to its end (the stamp, "read ... to the end"), the read list, every text read and cleared again
    add(2.0, lambda w: cmd(w, "lyud.City.Sit"))
    add(1.0, lambda w: shot(w, "sit_nearest"))
    add(1.5, lambda w: cmd(w, "lyud.City.Sit"))
    add(2.0, lambda w: cmd(w, "lyud.Save citytest_seated"))
    add(3.0, lambda w: cmd(w, "lyud.Load 0"))
    add(1.0, lambda w: shot(w, "after_load_seated"))
    add(2.5, lambda w: cmd(w, "lyud.City.Lie"))
    add(1.0, lambda w: shot(w, "lie_nearest"))
    add(1.0, lambda w: cmd(w, "lyud.City.Sleep 7"))
    add(6.0, lambda w: shot(w, "after_sleep"))
    add(1.5, lambda w: cmd(w, "lyud.City.Read birds_starlings"))
    add(1.0, lambda w: shot(w, "book_page1"))
    for _ in range(4):
        add(1.0, lambda w: cmd(w, "lyud.City.Read next"))
    add(1.0, lambda w: cmd(w, "lyud.City.Read paper_today"))
    add(1.0, lambda w: shot(w, "newspaper"))
    add(1.0, lambda w: cmd(w, "lyud.City.Read prev"))
    add(1.0, lambda w: cmd(w, "lyud.City.Read close"))
    add(1.0, lambda w: cmd(w, "lyud.City.Read list"))
    add(1.0, lambda w: shot(w, "read_list"))
    add(1.0, lambda w: cmd(w, "lyud.City.Read all"))
    add(1.0, lambda w: cmd(w, "lyud.City.Read clear"))
    add(1.0, lambda w: cmd(w, "lyud.City.Read no_such_text"))
    readers = [t for t in things if acts.get(t.get("act"), {}).get("read")][:1]
    for th in readers:
        add(1.5, lambda w, i=th["id"]: cmd(w, "lyud.City.Do " + i))
        add(1.0, lambda w: cmd(w, "lyud.City.Read close"))
    # the staff talk: the chooser at a counter with topics (wherever the hero is), one topic asked, the list, the reset;
    # the room tone where the hero stands
    talkers = [t for t in things if t.get("topics") or acts.get(t.get("act"), {}).get("topics")][:1]
    for th in talkers:
        add(1.5, lambda w, i=th["id"]: cmd(w, "lyud.City.Ask " + i))
        add(1.0, lambda w: shot(w, "talk"))
        topic = (th.get("topics") or acts.get(th.get("act"), {}).get("topics"))[0]
        add(1.5, lambda w, i=th["id"], tp=topic: cmd(w, "lyud.City.Ask %s %s" % (i, tp)))
        add(1.0, lambda w: shot(w, "talk_answer"))
    add(1.0, lambda w: cmd(w, "lyud.City.Ask list"))
    add(1.0, lambda w: cmd(w, "lyud.City.Ask reset"))
    add(1.0, lambda w: cmd(w, "lyud.City.Ask no/such_thing"))
    add(1.0, lambda w: cmd(w, "lyud.City.Tone"))
    # a storey link: to a lift door if the data has one (else any door), as a lift would put him there
    lifts = [t for t in things if t.get("floors")] or [t for t in things if t.get("kind") == "door"]
    for th in lifts[:1]:
        add(2.5, lambda w, i=(th["floors"][0]["to"] if th.get("floors") else th["id"]): cmd(w, "lyud.City.Go " + i))
        add(1.0, lambda w: shot(w, "lift_arrived"))
    add(1.0, lambda w: cmd(w, "lyud.City.Go no/such_thing"))
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
