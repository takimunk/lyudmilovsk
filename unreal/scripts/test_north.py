"""Offscreen play test of the North (docs/north.md "Game side"): puts the hero at every place of Content/North/north.json by
day (lyud.North.Goto), then at the camp and in the Fumarole Valley at night (the fire by its schedule, then forced on),
logs the North's state each time (ULyudNorthSubsystem.debug_report: fumaroles awake and near, the fire, the steam puffs),
checks the sleep rule (nothing awake at the gate, the valley's fumaroles awake in the valley), sits the hero in the hot spring
(lyud.North.SitSpring 1: seated, eyes over the water, cushion under it) and gets him out again, and screenshots each step.
Leaves the desktop alone (no PIE, no input); run it with test_north.sh (NORTHTEST_ONLY=name,name picks steps).

Prints NORTHTEST lines; screenshots land in Saved/Screenshots/MacEditor in the order of the steps.
"""
import json
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
with open(os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "North", "north.json")) as _f:
    PLACES = [p["id"] for p in json.load(_f)["places"]]

DAY, NIGHT = 13.0, 0.75       # local hours the sky shows (the fire burns from dusk until 1:30)
# (name, place, hour, console commands before, check)
STEPS = [("day_" + p, p, DAY, [], None) for p in PLACES]
STEPS += [
    ("sleep_gate", "north_gate", DAY, [], "asleep"),
    ("wake_valley", "north_fumaroles", DAY, [], "awake"),
    ("night_camp", "north_camp", NIGHT, ["lyud.North.Fire -1"], "fire"),
    ("night_camp_forced", "north_camp", NIGHT, ["lyud.North.Fire 1"], "fire"),
    ("night_fumaroles", "north_fumaroles", NIGHT, [], "awake"),
    ("night_spring", "north_spring", NIGHT, [], None),
    ("day_camp_out", "north_camp", DAY, ["lyud.North.Fire 0"], None),
    ("wakeall_gate", "north_gate", DAY, ["lyud.North.Fire -1", "lyud.North.WakeAll 1"], "all"),
    ("hidden", "north_camp", DAY, ["lyud.North.WakeAll 0", "lyud.North 0"], "asleep"),
    ("shown_again", "north_camp", DAY, ["lyud.North 1"], None),
    # the hot spring: lyud.North.SitSpring 1 a few seconds after arriving (he wades in), then out again
    ("spring_sit", "north_spring", DAY, [], "spring_sit"),
    ("spring_out", "north_spring", DAY, [], "spring_out"),
]
if os.environ.get("NORTHTEST_ONLY"):
    STEPS = [s for s in STEPS if s[0] in os.environ["NORTHTEST_ONLY"].split(",")]
WAIT = float(os.environ.get("NORTHTEST_WAIT", "8"))
st = {"t": 0.0, "i": -1, "phase": 0, "fails": 0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def log(s):
    unreal.log_warning("NORTHTEST " + s)


def shown_hour(w):
    skies = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.LyudSky)
    if not skies:
        return None
    t = skies[0].shown_time()
    return unreal.MathLibrary.get_hour(t) + unreal.MathLibrary.get_minute(t) / 60.0


def set_hour(w, hour):
    """Moves the sky (lyud.SkyHourOffset is relative: add the difference to the current offset)."""
    now = shown_hour(w)
    if now is None:
        log("no sky: hour not set")
        return
    cur = unreal.SystemLibrary.get_console_variable_float_value("lyud.SkyHourOffset")
    d = (hour - now) % 24.0
    cmd(w, "lyud.SkyHourOffset %f" % (cur + d))


def spring(w):
    a = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.LyudNorthSpring)
    return a[0] if a else None


def check_spring(w, name, kind):
    """The hero in the hot spring: seated, his eyes over the water, the seat's cushion under it (or out and standing)."""
    s = spring(w)
    if not s:
        log("%s FAIL no spring actor (north.json has no spring?)" % name)
        st["fails"] += 1
        return
    log("%s %s" % (name, s.debug_report()))
    seated = s.is_hero_seated()
    eye = s.hero_eye_above_water()
    if kind == "spring_sit":
        ok = seated and 5.0 <= eye <= 90.0
    else:
        ok = not seated and eye > 100.0
    log("%s %s (seated %d, eyes %+.0f cm over the water at z %.0f, cushion %.0f cm under)"
        % (name, "PASS" if ok else "FAIL", 1 if seated else 0, eye, s.get_water_z(), s.water_over_seat))
    st["fails"] += 0 if ok else 1


def check(w, name, kind):
    if kind in ("spring_sit", "spring_out"):
        check_spring(w, name, kind)
        return
    n = unreal.LyudNorthSubsystem.get_north(w)
    if not n:
        log("%s FAIL no North subsystem" % name)
        st["fails"] += 1
        return
    rep = n.debug_report()
    awake, things = n.num_awake(), n.num_things()
    log("%s %s" % (name, rep))
    ok = True
    if kind == "asleep":
        ok = awake == 0
    elif kind == "awake":
        ok = awake >= 20
    elif kind == "all":
        ok = things > 0 and awake == things
    elif kind == "fire":
        ok = " lit 1 " in rep
    if kind:
        log("%s %s (%s: awake %d of %d)" % (name, "PASS" if ok else "FAIL", kind, awake, things))
        st["fails"] += 0 if ok else 1


def tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < 5.0:
        return
    if st["phase"] == 0:
        st["i"] += 1
        if st["i"] >= len(STEPS):
            n = unreal.LyudNorthSubsystem.get_north(w)
            if n:
                # the river's water (north.json "river"): samples, length, parts, awake / near / sounding now
                log("river:%s" % n.debug_report().partition(" river")[2])
            log("done: %d steps, %d failed" % (len(STEPS), st["fails"]))
            cmd(w, "quit")
            st["phase"] = 9
            return
        name, place, hour, extra, _ = STEPS[st["i"]]
        set_hour(w, hour)
        for c in extra:
            cmd(w, c)
        if STEPS[st["i"]][4] != "spring_out":
            # getting out happens where he sits (no teleport out of the seat)
            cmd(w, "lyud.North.SitSpring 0")
            cmd(w, "lyud.North.Goto " + place)
        st["t0"] = st["t"]
        st["sat"] = False
        st["phase"] = 1
    elif st["phase"] == 1:
        name, place, hour, extra, kind = STEPS[st["i"]]
        if kind in ("spring_sit", "spring_out") and not st["sat"] and st["t"] - st["t0"] > 3.0:
            # after the range check has woken the spring (it wades only while near)
            st["sat"] = True
            cmd(w, "lyud.North.SitSpring %d" % (1 if kind == "spring_sit" else 0))
        if st["t"] - st["t0"] > WAIT:
            check(w, name, kind)
            cmd(w, "HighResShot 1600x900")
            st["phase"] = 2
            st["t0"] = st["t"]
    elif st["phase"] == 2 and st["t"] - st["t0"] > 2.0:
        st["phase"] = 0


log("steps: %s" % ", ".join(s[0] for s in STEPS))
unreal.register_slate_post_tick_callback(tick)
