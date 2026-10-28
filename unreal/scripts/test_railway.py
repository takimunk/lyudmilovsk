"""Offscreen check of the sea railway (docs/railway.md) in Lvl_Scene01: the timetable clock moved so the train comes out
of the tunnel; the level crossing warns (lamps, bell, barriers down) and opens again; the train stops at the beach halt
(doors open), runs out along the bank (wake, spray, gulls: a chase camera beside it), stops at the island halt; at
night the lit car from inside and from the bank; the line to the horizon from the cliff. RAILTEST lines in the log;
screenshots in the order of the 'shot' lines (unreal/scripts/test_railway.sh copies them to Saved/RailwayTest).

  unreal/scripts/test_railway.sh            (RAILTEST_ONLY=day|night|far to run one part)
"""
import os

import unreal

ONLY = os.environ.get("RAILTEST_ONLY", "")
S = {"t": 0.0, "step": 0, "shots": 0, "phase": "plan", "chase": None, "fails": 0, "max_drops": 0, "max_gulls": 0, "max_wet": 0.0,
     "warned": False, "barrier": 0.0}
FACE_Y = 790.0      # Unreal Y of the map's Y = 0 (railway_built.json frame.portal_face_y)


def log(msg):
    unreal.log_warning("RAILTEST " + msg)


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
    cmd(w, "shot")


def rail(w):
    return unreal.LyudRailwaySubsystem.get_railway(w)


def M(x, y, z):
    """Map metres -> Unreal cm."""
    return (x * 100.0, FACE_Y + y * 100.0, z * 100.0)


def go(w, x, y, z, pitch, yaw):
    X, Y, Z = M(x, y, z)
    cmd(w, "BugItGo %.0f %.0f %.0f %.1f %.1f 0" % (X, Y, Z, pitch, yaw))


def doors_open(info):
    # Unreal's Python drops the b of a bool's name
    return getattr(info, "doors_open", None) if hasattr(info, "doors_open") else getattr(info, "b_doors_open", False)


def check(ok, what):
    log(("ok   " if ok else "FAIL ") + what)
    if not ok:
        S["fails"] += 1


def report(w):
    r = rail(w)
    log("report " + (r.report() if r else "no railway"))
    t = r.get_train(0) if r else None
    if t:
        wk = t.get_wake()
        log("train seats %d, doors %d, wake wet %.2f drops %d foam %d, gulls %d" % (
            t.num_seats(), t.num_doors(), wk.get_wetness() if wk else -1, wk.num_droplets() if wk else -1, wk.num_foam() if wk else -1,
            t.num_gulls()))
    c = r.get_crossing() if r else None
    if c:
        log("crossing warning %s barrier %.2f" % (c.is_warning(), c.get_barrier()))


def chase(w, side, up, back):
    """Puts the camera beside the train (metres: to its south, up, behind its middle), looking at it."""
    r = rail(w)
    info = r.get_train(0).get_info()
    # the train's actor stays at the line's origin while its cars move: aim at the line under its middle
    p = r.line_to_world(info.s - back * (1 if info.dir >= 0 else -1), -side, up)
    mid = r.line_to_world(info.s, 0.0, 1.5)
    d = mid - p
    import math
    yaw = math.degrees(math.atan2(d.y, d.x))
    pitch = math.degrees(math.atan2(d.z, math.hypot(d.x, d.y)))
    cmd(w, "BugItGo %.0f %.0f %.0f %.1f %.1f 0" % (p.x, p.y, p.z, pitch, yaw))


def watch(w):
    r = rail(w)
    if not r:
        return
    t = r.get_train(0)
    c = r.get_crossing()
    if t:
        wk = t.get_wake()
        if wk:
            S["max_drops"] = max(S["max_drops"], wk.num_droplets())
            S["max_wet"] = max(S["max_wet"], wk.get_wetness())
        S["max_gulls"] = max(S["max_gulls"], t.num_gulls())
    if c:
        S["warned"] = S["warned"] or c.is_warning()
        S["barrier"] = max(S["barrier"], c.get_barrier())


def start(w):
    cmd(w, "DisableAllScreenMessages")
    r = rail(w)
    check(r is not None and r.is_running(), "the railway runs")
    if not r or not r.is_running():
        return
    log("stops %d: %s" % (r.num_stops(), ", ".join(str(r.get_stop_id(i)) for i in range(r.num_stops()))))
    t = r.get_train(0)
    check(t is not None and t.num_seats() > 30, "a train with seats (%d)" % (t.num_seats() if t else 0))
    # the train's doors open at the beach halt, outbound, in 20 s
    cmd(w, "lyud.Railway.Next beach 1")


def doors_beach(w):
    r = rail(w)
    info = r.get_train(0).get_info()
    check(doors_open(info) and str(r.get_stop_id(info.stop)) == "beach", "doors open at the beach halt (stop %d, s %.1f)" % (info.stop, info.s))


def doors_island(w):
    r = rail(w)
    info = r.get_train(0).get_info()
    check(doors_open(info) and str(r.get_stop_id(info.stop)) == "island", "doors open at the island halt (stop %d, s %.1f)" % (info.stop, info.s))


def summary(w):
    report(w)
    check(S["warned"] and S["barrier"] > 0.95, "the crossing warned and its barriers came down (%.2f)" % S["barrier"])
    check(S["max_wet"] > 0.3, "the wheels ran in water (wet %.2f)" % S["max_wet"])
    check(S["max_drops"] > 20, "spray from the wheels (%d droplets at most)" % S["max_drops"])
    check(S["max_gulls"] >= 3, "gulls followed the train (%d)" % S["max_gulls"])


DAY = [
    (30.0, start),
    (30.5, lambda w: go(w, 160.0, -52.0, -40.2, 2.0, 180.0)),       # on the bank off the mouth, looking into the tunnel
    (32.0, lambda w: shot(w, "tunnel_mouth")),
    (33.0, lambda w: go(w, 129.0, -73.0, -40.7, 6.0, 90.0)),        # on the path south of the crossing, facing it
    (35.0, lambda w: shot(w, "crossing_sign")),
    (36.0, lambda w: go(w, 146.0, -82.0, -40.6, 3.0, 128.0)),       # on the beach south of the crossing, looking at the mouth
    (36.5, lambda w: report(w)),
    (37.5, lambda w: shot(w, "crossing_warning")),
    (41.0, lambda w: shot(w, "train_out_of_tunnel")),
    (44.0, lambda w: report(w)),
    (46.0, lambda w: go(w, 196.0, -66.0, -39.6, -2.0, 180.0)),      # on the beach halt's deck, looking along it
    (51.5, doors_beach),
    (52.0, lambda w: shot(w, "beach_halt_doors")),
    (54.0, lambda w: go(w, 128.0, -95.0, -40.0, 4.0, 85.0)),        # the crossing from the path, barriers
    (56.0, lambda w: shot(w, "crossing_open")),
    # the run out to sea: a chase camera beside the train
    (95.0, lambda w: S.update(chase=(16.0, 4.0, -10.0))),
    (100.0, lambda w: shot(w, "bank_chase")),
    (101.0, lambda w: report(w)),
    (120.0, lambda w: S.update(chase=(9.0, 2.5, 22.0))),
    (125.0, lambda w: shot(w, "spray_close")),
    (135.0, lambda w: S.update(chase=(35.0, 14.0, -30.0))),
    (140.0, lambda w: shot(w, "gulls_over_train")),
    (141.0, lambda w: report(w)),
    (141.5, lambda w: S.update(chase=(5.0, 11.0, -26.0))),
    (144.0, lambda w: shot(w, "gulls_close")),
    (144.5, lambda w: S.update(chase=None)),
    (145.0, lambda w: cmd(w, "lyud.Railway.Next island 1")),
    (146.0, lambda w: go(w, 2420.0, -66.5, -39.3, -3.0, 180.0)),    # the island halt's deck
    (167.0, doors_island),
    (168.0, lambda w: shot(w, "island_halt")),
    (169.0, lambda w: go(w, 2290.0, -170.0, -32.0, -6.0, 50.0)),    # the island from off its south-east shore
    (172.0, lambda w: shot(w, "island")),
    (173.0, summary),
]
NIGHT = [
    (175.0, lambda w: cmd(w, "lyud.Birds.Time 21 30")),
    (180.0, lambda w: cmd(w, "lyud.Railway.Next beach -1")),
    (183.0, lambda w: go(w, 196.0, -66.0, -39.6, -2.0, 160.0)),
    (201.0, lambda w: shot(w, "night_beach_halt")),
    (203.0, lambda w: go(w, 176.0, -60.3, -40.6, -4.0, 0.0)),       # inside the second car, standing in the aisle
    (206.0, lambda w: shot(w, "night_inside")),
    (207.0, lambda w: go(w, 400.0, -110.0, -36.0, -5.0, 120.0)),
    (230.0, lambda w: shot(w, "night_from_sea")),
]
FAR = [
    (235.0, lambda w: cmd(w, "lyud.Birds.Time 16 25")),
    (240.0, lambda w: cmd(w, "lyud.Railway.Next island 1")),
    (242.0, lambda w: go(w, 72.0, -40.0, 17.0, -4.0, -2.0)),         # on the cliff top over the tunnel, looking out to sea
    (250.0, lambda w: shot(w, "line_to_island")),
    (320.0, lambda w: shot(w, "train_to_horizon")),
]
def people(w, key):
    cmd(w, "lyud.People.Report %s" % key)


def riders_start(w):
    cmd(w, "DisableAllScreenMessages")
    r = rail(w)
    check(r is not None and r.is_running(), "the railway runs")
    log("report " + r.report())
    for k in ("nishimura_aya", "sorokin_oleg", "sato_mila"):
        people(w, k)


# residents riding: on Tuesday at 10:41 two of them are on the island; ordered home they walk to the island halt and
# take the 10:47 back (the railway's clock untouched: their wait is fixed when they reach the platform)
RIDERS = [
    (21.0, lambda w: cmd(w, "lyud.Birds.Time 10 41")),
    (22.0, riders_start),
    (24.0, lambda w: go(w, 2328.0, -94.0, -40.6, -6.0, -135.0)),     # by the painter on the island's south shore
    (30.0, lambda w: cmd(w, "lyud.People.Order sorokin_oleg read_home")),
    (30.5, lambda w: cmd(w, "lyud.People.Order nishimura_aya read_home")),
    (34.0, lambda w: shot(w, "island_painter")),
    (35.0, lambda w: go(w, 2425.0, -66.5, -39.2, -5.0, 180.0)),      # the island halt's deck, looking along it
    (200.0, lambda w: people(w, "sorokin_oleg")),
    (201.0, lambda w: people(w, "nishimura_aya")),
    (360.0, lambda w: shot(w, "island_waiting")),
    (361.0, lambda w: report(w)),
    (400.0, lambda w: shot(w, "island_boarding")),
    (401.0, lambda w: people(w, "sorokin_oleg")),
    (430.0, lambda w: S.update(chase=(0.0, 1.6, -7.0))),
    (438.0, lambda w: shot(w, "riders_inside")),
    (439.0, lambda w: people(w, "sorokin_oleg")),
    (439.5, lambda w: people(w, "nishimura_aya")),
    (440.0, lambda w: S.update(chase=(9.0, 2.0, 0.0))),
    (446.0, lambda w: shot(w, "riders_window")),
    (447.0, lambda w: S.update(chase=None)),
]
PLAN = []
for name, part in (("riders", RIDERS),) if ONLY == "riders" else ():
    PLAN += part
for name, part in (("day", DAY), ("night", NIGHT), ("far", FAR)):
    if (not ONLY or name in ONLY.split(",")) and ONLY != "riders":
        PLAN += part
if ONLY and "day" not in ONLY.split(",") and ONLY != "riders":
    PLAN.insert(0, (30.0, start))
PLAN.append((PLAN[-1][0] + 4.0, lambda w: S.update(phase="done")))


def tick(dt):
    w = world()
    if not w:
        return
    dt = min(dt, 0.25)
    S["t"] += dt
    if S["phase"] == "quit":
        return
    while S["step"] < len(PLAN) and S["t"] > PLAN[S["step"]][0]:
        PLAN[S["step"]][1](w)
        S["step"] += 1
    if S["chase"]:
        chase(w, *S["chase"])
    watch(w)
    if S["phase"] == "done" or S["t"] > 800.0:
        log("done: %d failed" % S["fails"] if S["phase"] == "done" else "FAIL timeout")
        cmd(w, "quit")
        S["phase"] = "quit"


unreal.register_slate_post_tick_callback(tick)
