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
    (231.0, lambda w: cmd(w, "lyud.Railway.Next beach 1")),         # out again: the lit cars on the dark bank
    (290.0, lambda w: S.update(chase=(45.0, 5.0, -10.0))),
    (300.0, lambda w: shot(w, "night_bank")),
    (301.0, lambda w: S.update(chase=(14.0, 2.0, 4.0))),
    (306.0, lambda w: shot(w, "night_bank_close")),
    (307.0, lambda w: S.update(chase=None)),
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


def state(w, key):
    """A resident's doing (waiting, riding, ...) from the life's own description."""
    life = unreal.PeopleLifeSubsystem.get_people_life(w)
    d = str(life.describe(life.find_person(key))) if life else ""
    parts = d.split(") ", 1)
    return parts[1].split(" ", 1)[0] if len(parts) > 1 else ""


def later(dt, fn):
    PLAN_EXTRA.append((S["t"] + dt, fn))
    PLAN_EXTRA.sort(key=lambda e: e[0])


RIDERS_KEYS = ("sorokin_oleg", "nishimura_aya")


def riders_watch(w):
    """The riders' shots follow what they do (the game's seconds and the test's drift apart under load): as soon as
    both wait on the island halt's deck, from the track; as the doors open, along the deck; once both ride, inside the
    car and through its windows."""
    st = [state(w, k) for k in RIDERS_KEYS]
    if S.get("rw") == "walk" and all(x == "waiting" for x in st):
        S["rw"] = "wait"
        go(w, 2410.0, -55.0, -39.6, -8.0, -160.0)
        later(2.0, lambda w: shot(w, "island_waiting"))
        later(2.5, lambda w: [people(w, k) for k in RIDERS_KEYS])
    info = rail(w).get_train(0).get_info()
    if S.get("rw") == "wait" and doors_open(info) and str(rail(w).get_stop_id(info.stop)) == "island":
        S["rw"] = "doors"
        go(w, 2372.0, -63.4, -39.2, -3.0, 2.0)
        later(1.5, lambda w: shot(w, "island_boarding"))
    if S.get("rw") == "doors" and all(x == "riding" for x in st):
        S["rw"] = "ride"
        check(True, "both residents got in at the island halt")
        later(8.0, lambda w: S.update(chase=(0.0, 1.6, -7.0)))
        later(14.0, lambda w: shot(w, "riders_inside"))
        later(15.0, lambda w: S.update(chase=(9.0, 2.0, 0.0)))
        later(21.0, lambda w: shot(w, "riders_window"))
        later(22.0, lambda w: S.update(chase=None, rw="done"))


def riders_end(w):
    check(S.get("rw") == "done", "the riders waited, boarded and were seen riding (%s)" % S.get("rw"))
    for k in RIDERS_KEYS:
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
    (35.0, lambda w: S.update(rw="walk")),
    (600.0, riders_end),
]
def near_plant(w, dist):
    """The camera dist metres south of the island's first plant, looking at it."""
    best = None
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.ProceduralPlant):
        p = a.get_actor_location()
        if p.x > 200000:
            best = a
            break
    if not best:
        log("no island plant")
        return
    p = best.get_actor_location()
    log("plant %s %s at %.0f %.0f %.0f" % (best.get_name(), best.get_editor_property("species"), p.x, p.y, p.z))
    cmd(w, "BugItGo %.0f %.0f %.0f -8 90 0" % (p.x, p.y - dist * 100.0, p.z + 400.0))


def frames(w, n, first=0, step=1.0):
    for i in range(n):
        PLAN_EXTRA.append((S["t"] + step * i, lambda w, i=i: sunset_frame(w, first + i)))


def sunset_frame(w, i):
    info = rail(w).get_train(0).get_info()
    log("sunset %03d train s %.0f dir %d" % (i, info.s, info.dir))
    shot(w, "sunset_%03d" % i)


def fixed(w, s, side, up, look_s, look_side=0.0, look_up=2.0):
    """A still camera beside the line (metres along it, to its north, over the rail head), looking at a point by it."""
    import math
    r = rail(w)
    p = r.line_to_world(s, side, up)
    q = r.line_to_world(look_s, look_side, look_up)
    d = q - p
    cmd(w, "BugItGo %.0f %.0f %.0f %.1f %.1f 0" % (p.x, p.y, p.z, math.degrees(math.atan2(d.z, math.hypot(d.x, d.y))),
                                                   math.degrees(math.atan2(d.y, d.x))))


def follow(w, train_s):
    """The long lens: from the still camera, aimed at the train's middle, zoomed so it fills a third of the frame until
    the lens is at its longest (6 degrees: longer, the sea's waves alias into stripes), after which it shrinks into the
    haze. From south of the line and 10 m up, so the masts (north of the rails) stand behind it, not before it."""
    import math
    r = rail(w)
    p = r.line_to_world(2850.0, -20.0, 10.0)
    q = r.line_to_world(train_s - 16.0, 0.0, 2.0)
    d = q - p
    dist = math.hypot(d.x, d.y) / 100.0
    cmd(w, "fov %.2f" % max(6.0, min(40.0, math.degrees(2.0 * math.atan(48.0 / max(dist, 1.0))))))
    cmd(w, "BugItGo %.0f %.0f %.0f %.3f %.3f 0" % (p.x, p.y, p.z, math.degrees(math.atan2(d.z, math.hypot(d.x, d.y))),
                                                   math.degrees(math.atan2(d.y, d.x))))


PLAN_EXTRA = []
# the train leaves the island for the horizon at sunset: a still camera 6 m over the water a little north of the bank,
# 290 m past the island halt, looking out along the line with the low sun on the left; the train pulls out of the halt
# behind it, runs by on the bank throwing its spray and foam and goes on; then a longer lens on it from south of the line
# (follow) and the game at 8x watch it go into the haze
SUNSET = [
    (21.0, lambda w: cmd(w, "lyud.Birds.Time 16 50")),
    (22.0, lambda w: cmd(w, "DisableAllScreenMessages")),
    (23.0, lambda w: cmd(w, "lyud.Railway.Next island 1")),
    (26.0, lambda w: fixed(w, 2850.0, 14.0, 6.0, 6000.0, -700.0, 0.0)),
    (66.0, lambda w: frames(w, 90, 0, 1.0)),            # it pulls out, comes by and runs off at its own pace
    (156.5, lambda w: cmd(w, "slomo 8")),
    (156.6, lambda w: S.update(follow=True)),
    (157.0, lambda w: frames(w, 90, 90, 1.0)),          # then eight times faster, into the haze
    (248.0, lambda w: cmd(w, "slomo 1")),
    (248.1, lambda w: S.update(follow=False)),
]

TREES = [
    (22.0, lambda w: near_plant(w, 12.0)),
    (30.0, lambda w: shot(w, "pine_near_early")),
    (31.0, lambda w: near_plant(w, 120.0)),
    (36.0, lambda w: shot(w, "pine_far_early")),
    (150.0, lambda w: near_plant(w, 12.0)),
    (156.0, lambda w: shot(w, "pine_near_late")),
    (157.0, lambda w: near_plant(w, 120.0)),
    (163.0, lambda w: shot(w, "pine_far_late")),
]
def ride_board(w):
    """The player into car 2's aisle by its middle door, while the doors stand open at the beach halt."""
    t = rail(w).get_train(0)
    p = t.get_door_point(2)
    S["ride_seat"] = 0
    # from the door point (outside the car) 1.45 m in, toward the line's centre, standing on the floor
    mid = rail(w).line_to_world(t.get_info().s, 0.0, 0.0)
    d = unreal.Vector(mid.x - p.x, mid.y - p.y, 0.0)
    n = (d.x * d.x + d.y * d.y) ** 0.5 or 1.0
    q = unreal.Vector(p.x + d.x / n * 145.0, p.y + d.y / n * 145.0, p.z + 20.0)
    # not BugItGo: it ghosts the pawn (no collision, flying); the pawn is put there as it is, walking, and falls onto
    # the car's floor like a player stepping in
    pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    pawn.set_actor_location(unreal.Vector(q.x, q.y, q.z + 80.0), False, True)
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pc.set_control_rotation(unreal.Rotator(0.0, 0.0, 90.0))
    S["ride_log"] = True


def ride_local(w):
    import math
    pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    r = rail(w).get_train(0).get_seat(S.get("ride_seat", 0))
    seat = r[-1] if isinstance(r, tuple) else r
    if not pawn or seat is None or (isinstance(r, tuple) and not r[0]):
        return None
    return seat.inverse_transform_location(pawn.get_actor_location())


def ride_watch(w):
    l = ride_local(w)
    if l is None:
        return
    info = rail(w).get_train(0).get_info()
    if "ride_first" not in S and abs(info.speed) < 0.01:
        S["ride_ref"] = l
    if abs(info.speed) > 0.05:
        S["ride_first"] = True
        r = S.get("ride_ref", l)
        S["ride_drift"] = max(S.get("ride_drift", 0.0), ((l.x - r.x) ** 2 + (l.y - r.y) ** 2) ** 0.5)


def ride_report(w):
    l = ride_local(w)
    info = rail(w).get_train(0).get_info()
    pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    mv = pawn.get_movement_base() if pawn and hasattr(pawn, "get_movement_base") else None
    mode = pawn.get_editor_property("character_movement").movement_mode if pawn else None
    log("ride s %.1f v %.2f local %.0f %.0f %.0f base %s mode %s drift %.1f cm" % (
        info.s, info.speed, l.x, l.y, l.z, mv.get_owner().get_name() if mv else None, mode, S.get("ride_drift", 0.0)))


def ride_end(w):
    ride_report(w)
    pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    mv = pawn.get_movement_base() if pawn and hasattr(pawn, "get_movement_base") else None
    mode = pawn.get_editor_property("character_movement").movement_mode if pawn else None
    check(mode == unreal.MovementMode.MOVE_WALKING, "the player walks on the car's floor (%s, base %s)" % (mode, mv.get_name() if mv else None))
    check(S.get("ride_first", False) and S.get("ride_drift", 1e9) < 15.0,
          "a player standing in the car stays put as it pulls away and runs (drift %.1f cm)" % S.get("ride_drift", -1.0))


def look(w, a, b):
    """The camera at a, looking at b; both (metres along the line from the second car's middle, to the north, over
    the rail head)."""
    import math
    r = rail(w)
    m = r.get_train(0).get_info().s - 8.45
    p = r.line_to_world(m + a[0], a[1], a[2])
    q = r.line_to_world(m + b[0], b[1], b[2])
    d = q - p
    # BugItGo places the pawn: its eyes are ~64 cm over that
    cmd(w, "BugItGo %.0f %.0f %.0f %.2f %.2f 0" % (p.x, p.y, p.z - 64.0, math.degrees(math.atan2(d.z, math.hypot(d.x, d.y))),
                                                   math.degrees(math.atan2(d.y, d.x))))


# the car's inside, standing at the beach halt (the doors open on the platform's side, south), by day then at night:
# the aisle, the benches and their grilles, the cards over the racks at a grazing angle (flicker would show there),
# a hanging poster, the shut doors' stickers, both end walls, a window's blind, the aisle at night
VIEWS = [
    ("aisle", (-6.2, 0.0, 2.75), (8.0, 0.0, 2.45)),
    ("benches", (-1.0, 0.25, 2.25), (0.6, -1.2, 1.55)),
    ("cards_grazing", (-3.2, -0.85, 3.05), (1.0, -1.32, 3.2)),
    ("poster", (-3.4, 0.35, 3.0), (-2.0, 0.0, 3.45)),
    ("door_stickers", (-4.55, -0.2, 2.35), (-4.55, 1.37, 2.05)),
    ("end_cab", (-4.6, 0.0, 2.8), (-6.6, 0.0, 3.2)),
    ("end_gangway", (5.0, 0.0, 2.6), (8.0, 0.0, 3.0)),
    ("blind", (1.0, -0.3, 2.35), (1.0, 1.4, 2.55)),
]
INSIDE = [
    (21.0, lambda w: cmd(w, "lyud.Birds.Time 11 20")),
    (22.0, lambda w: cmd(w, "DisableAllScreenMessages")),
    (23.0, lambda w: cmd(w, "lyud.Railway.Next beach 1")),
]
for i, (name, a_, b_) in enumerate(VIEWS):
    INSIDE += [(45.0 + 4.0 * i, lambda w, a_=a_, b_=b_: look(w, a_, b_)), (47.5 + 4.0 * i, lambda w, n=name: shot(w, "in_" + n))]
INSIDE += [
    (90.0, lambda w: cmd(w, "lyud.Birds.Time 21 40")),
    (91.0, lambda w: cmd(w, "lyud.Railway.Next beach 1")),
    (114.0, lambda w: look(w, VIEWS[0][1], VIEWS[0][2])),
    (118.0, lambda w: shot(w, "in_aisle_night")),
    (119.0, lambda w: look(w, VIEWS[2][1], VIEWS[2][2])),
    (122.0, lambda w: shot(w, "in_cards_night")),
]

# riding: the player stands in the second car at the beach halt; it leaves and runs out onto the bank; standing
# still, the player must stay where they stand in the car (the car's floor carries them)
RIDE = [
    (22.0, lambda w: cmd(w, "DisableAllScreenMessages")),
    (23.0, lambda w: cmd(w, "lyud.Railway.Next beach 1")),
    (46.0, ride_board),
    (52.0, ride_report),
    (54.0, lambda w: shot(w, "ride_standing")),
] + [(60.0 + 5.0 * i, ride_report) for i in range(16)] + [
    (100.0, lambda w: shot(w, "ride_running")),
    (142.0, ride_end),
]

PLAN = []
for name, part in (("inside", INSIDE),) if ONLY == "inside" else ():
    PLAN += part
for name, part in (("ride", RIDE),) if ONLY == "ride" else ():
    PLAN += part
for name, part in (("sunset", SUNSET),) if ONLY == "sunset" else ():
    PLAN += part
for name, part in (("trees", TREES),) if ONLY == "trees" else ():
    PLAN += part
for name, part in (("riders", RIDERS),) if ONLY == "riders" else ():
    PLAN += part
for name, part in (("day", DAY), ("night", NIGHT), ("far", FAR)):
    if (not ONLY or name in ONLY.split(",")) and ONLY not in ("riders", "trees", "sunset", "ride", "inside"):
        PLAN += part
if ONLY and "day" not in ONLY.split(",") and ONLY not in ("riders", "trees", "sunset", "ride", "inside"):
    PLAN.insert(0, (30.0, start))
PLAN.append((max(t for t, _ in PLAN) + 4.0, lambda w: S.update(phase="done")))


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
    while PLAN_EXTRA and S["t"] > PLAN_EXTRA[0][0]:
        PLAN_EXTRA.pop(0)[1](w)
    if S["chase"]:
        chase(w, *S["chase"])
    if S.get("rw") in ("walk", "wait", "doors"):
        riders_watch(w)
    if S.get("ride_log"):
        ride_watch(w)
    if S.get("follow"):
        follow(w, rail(w).get_train(0).get_info().s)
    watch(w)
    if S["phase"] == "done" or S["t"] > 800.0:
        log("done: %d failed" % S["fails"] if S["phase"] == "done" else "FAIL timeout")
        cmd(w, "quit")
        S["phase"] = "quit"


unreal.register_slate_post_tick_callback(tick)
