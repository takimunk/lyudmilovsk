"""Offscreen check of the hatch to Europa (docs/europa.md) in Lvl_Scene01, the whole way there and back with E
(lyud.Europa.Use) at 5x speed (lyud.Europa.Fast 1): the hatch on the sea bed and its map marker; E opens the lid, E climbs
down (the lid shuts over the head); the full airlock; the computer drains it (the water goes down, the pump plays); the
inner ladder up, the jump between the shafts, out onto Europa: space view, 0.13 g, the planet overhead, the line and the
music on the first look; a high jump; back: E opens Europa's lid, E climbs down, the computer fills the lock, the sea
ladder up and out under the sea (the swimming takes over). EUROPATEST lines in the log; screenshots in the order of the
'shot' lines (unreal/scripts/test_europa.sh copies them to Saved/EuropaTest). EUROPATEST_ONLY=europa|hatch runs a short part.
"""
import math

import unreal

S = {"t": 0.0, "step": 0, "shots": 0, "phase": "plan", "fails": 0, "jump0": None, "jump_max": 0.0, "drain_seen": False,
     "music": False}
SEA_SHAFT = (0.0, 0.0)
EUROPA_SHAFT = (-180.0, 0.0)
FLOOR = -580.0


def log(msg):
    unreal.log_warning("EUROPATEST " + msg)


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_pawn(w, 0):
            return w
    return None


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def shot(w, name, ui=False):
    log("shot %02d %s" % (S["shots"], name))
    S["shots"] += 1
    cmd(w, "shot showui" if ui else "shot")


def check(ok, what):
    log(("ok   " if ok else "FAIL ") + what)
    if not ok:
        S["fails"] += 1


def eu(w):
    return unreal.LyudEuropaSubsystem.get_europa(w)


def pawn(w):
    return unreal.GameplayStatics.get_player_pawn(w, 0)


def pc(w):
    return unreal.GameplayStatics.get_player_controller(w, 0)


def report(w):
    log(eu(w).report())


def look_at(w, target):
    p = pawn(w)
    eye = p.get_actor_location() + unreal.Vector(0, 0, 70)
    d = target - eye
    pc(w).set_control_rotation(unreal.Rotator(0.0, math.degrees(math.atan2(d.z, math.hypot(d.x, d.y))), math.degrees(math.atan2(d.y, d.x))))


def kit(w, x, y, z):
    return eu(w).get_airlock().kit_to_world(unreal.Vector(x, y, z))


def stand(w, at, look):
    """The pawn put there (walking: it falls onto the floor; in the full lock, diving), looking at look."""
    p = pawn(w)
    p.set_actor_location(at + unreal.Vector(0, 0, 100), False, True)
    look_at(w, look)
    if state(w) == 0:
        unreal.LyudSwimComponent.get_player_swim(w).enter_under()


def action(w, want):
    a = str(eu(w).current_action())
    check(a == want, "E would %s (%s)" % (want, a))
    return a == want


def use(w, want):
    if action(w, want):
        eu(w).use()


def state(w):
    """0 full, 1 draining, 2 dry, 3 filling."""
    name = str(eu(w).get_airlock().get_state()).upper()
    return next((i for i, n in enumerate(("FLOODED", "DRAINING", "DRY", "FILLING")) if n in name), -1)


def start(w):
    cmd(w, "DisableAllScreenMessages")
    cmd(w, "lyud.Europa.Fast 1")
    cmd(w, "lyud.Birds.Time 12 30")
    e = eu(w)
    check(e is not None and e.get_airlock() is not None and e.get_site() is not None, "the hatch and Europa are spawned")
    report(w)
    a = e.get_airlock().get_actor_location()
    log("hatch at %.0f %.0f %.0f (UE cm)" % (a.x, a.y, a.z))


def at_hatch(w):
    eu(w).go_hatch()


def look_lid(w):
    look_at(w, kit(w, 0, 0, 0))


def summary(w):
    report(w)
    check(S["drain_seen"], "the water went down while draining")
    check(S["music"], "the Europa music played")
    check(S["jump_max"] > 400.0, "a high jump on Europa (%.0f cm)" % S["jump_max"])


def watch(w):
    e = eu(w)
    if not e or not e.get_airlock():
        return
    if state(w) == 1 and 0.2 < e.get_airlock().get_water01() < 0.8:
        S["drain_seen"] = True
    if e.is_music_playing():
        S["music"] = True
    if S["jump0"] is not None:
        S["jump_max"] = max(S["jump_max"], pawn(w).get_actor_location().z - S["jump0"])


def jump(w):
    S["jump0"] = pawn(w).get_actor_location().z
    pawn(w).jump()


def on_europa(w):
    e = eu(w)
    check(e.is_on_europa(), "on Europa")
    g = pawn(w).get_editor_property("character_movement").get_editor_property("gravity_scale")
    check(abs(g - 0.13) < 0.01, "gravity 0.13 g (%.2f)" % g)
    sky = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.LyudSky)
    check(sky and sky[0].is_space_view(), "the sky's space view")
    report(w)


def look_planet(w):
    d = eu(w).planet_direction()
    pc(w).set_control_rotation(unreal.Rotator(0.0, math.degrees(math.asin(d.z)) - 6.0, math.degrees(math.atan2(d.y, d.x))))


def planet_seen(w):
    check(eu(w).has_seen_planet(), "the planet was seen (the line said)")


def in_room(w, dry):
    e = eu(w)
    check(not e.is_climbing(), "the climb is over")
    p = pawn(w).get_actor_location()
    l = kit(w, 0, 0, 0)
    check(p.z < l.z - 300 and p.z > l.z + FLOOR - 50, "in the airlock (%.0f under the hatch)" % (l.z - p.z))
    check(not e.is_on_europa(), "not on Europa")
    check(state(w) == (2 if dry else 0), "the lock is %s (%d)" % ("dry" if dry else "full", state(w)))
    report(w)


def out_in_sea(w):
    p = pawn(w).get_actor_location()
    l = kit(w, 0, 0, 0)
    check(p.z > l.z - 50, "out of the hatch onto the bed (%.0f over the lid seat)" % (p.z - l.z))
    sw = unreal.LyudSwimComponent.get_player_swim(w)
    check(sw is not None and sw.is_swimming(), "swimming in the sea again (%s)" % (sw.get_state() if sw else None))
    report(w)


PLAN = [
    (30.0, start),
    (31.0, lambda w: cmd(w, "Screen.Open map europa_hatch")),
    (33.0, lambda w: shot(w, "map_marker", ui=True)),
    (34.0, lambda w: cmd(w, "Screen.Open none")),
    (34.5, at_hatch),
    (34.7, look_lid),
    (35.6, lambda w: shot(w, "hatch_on_seabed")),
    (35.8, look_lid),
    (36.0, lambda w: use(w, "open_sea")),
    (36.5, look_lid),
    (36.9, lambda w: shot(w, "hatch_open")),
    (37.0, look_lid),
    (37.2, lambda w: use(w, "enter_sea")),
    (38.0, lambda w: shot(w, "climbing_down")),
    (46.0, lambda w: in_room(w, False)),
    (46.5, lambda w: stand(w, kit(w, -90, 70, FLOOR), kit(w, -90, 160, FLOOR + 95))),
    (48.5, lambda w: shot(w, "airlock_flooded")),
    (49.0, lambda w: use(w, "console")),
    (50.6, lambda w: stand(w, kit(w, -90, -60, FLOOR), kit(w, -90, 120, FLOOR + 40))),
    (51.0, lambda w: shot(w, "airlock_draining")),
    (51.8, lambda w: report(w)),
    (55.0, lambda w: check(state(w) == 2, "drained (%d)" % state(w))),
    (55.5, lambda w: stand(w, kit(w, -60, -40, FLOOR), kit(w, -250, 40, FLOOR + 140))),
    (57.5, lambda w: shot(w, "airlock_dry")),
    (58.0, lambda w: stand(w, kit(w, EUROPA_SHAFT[0] - 10, 0, FLOOR), kit(w, EUROPA_SHAFT[0] + 100, 0, FLOOR + 150))),
    (59.0, lambda w: use(w, "climb_europa")),
    (59.6, lambda w: shot(w, "inner_hatch_open")),
    (66.0, on_europa),
    (66.5, lambda w: look_at(w, eu(w).get_site().get_actor_location())),
    (68.0, lambda w: shot(w, "europa_hatch")),
    (68.5, look_planet),
    (70.0, lambda w: shot(w, "europa_planet")),
    (71.0, planet_seen),
    (73.5, lambda w: shot(w, "europa_line", ui=True)),
    (75.0, lambda w: pc(w).set_control_rotation(unreal.Rotator(0.0, 4.0, 200.0))),
    (76.0, lambda w: shot(w, "europa_horizon")),
    (76.5, jump),
    (78.5, lambda w: look_planet(w)),
    (79.0, lambda w: shot(w, "europa_jump")),
    (86.0, lambda w: S.update(jump0=None)),
    (87.0, lambda w: eu(w).go_europa()),
    (88.0, lambda w: look_at(w, eu(w).get_site().get_actor_location())),
    (89.0, lambda w: use(w, "open_europa")),
    (90.5, lambda w: use(w, "enter_europa")),
    (97.0, lambda w: in_room(w, True)),
    (97.5, lambda w: stand(w, kit(w, -90, 70, FLOOR), kit(w, -90, 160, FLOOR + 95))),
    (99.0, lambda w: use(w, "console")),
    (100.2, lambda w: shot(w, "airlock_filling")),
    (105.0, lambda w: check(state(w) == 0, "filled (%d)" % state(w))),
    (105.5, lambda w: stand(w, kit(w, SEA_SHAFT[0] - 10, 0, FLOOR), kit(w, SEA_SHAFT[0] + 100, 0, FLOOR + 150))),
    (107.0, lambda w: use(w, "climb_sea")),
    (115.0, out_in_sea),
    (115.5, look_lid),
    (116.5, lambda w: shot(w, "back_in_sea")),
    (117.0, summary),
    (118.0, lambda w: S.update(phase="done")),
]


import os
if os.environ.get("EUROPATEST_ONLY") == "europa":
    PLAN = [(30.0, start), (31.0, lambda w: eu(w).go_europa()), (34.0, look_planet), (36.0, lambda w: shot(w, "europa_planet")),
            (36.5, lambda w: report(w)), (37.0, lambda w: S.update(phase="done"))]
elif os.environ.get("EUROPATEST_ONLY") == "hatch":
    PLAN = [(30.0, start), (31.0, at_hatch), (31.2, lambda w: report(w)), (31.5, look_lid), (32.0, lambda w: report(w)),
            (33.0, lambda w: report(w)), (34.0, lambda w: action(w, "open_sea")), (34.5, lambda w: S.update(phase="done"))]


def tick(dt):
    w = world()
    if not w:
        return
    try:
        dt = min(dt, 0.25)
        S["t"] += dt
        if S["phase"] == "quit":
            return
        while S["step"] < len(PLAN) and S["t"] > PLAN[S["step"]][0]:
            PLAN[S["step"]][1](w)
            S["step"] += 1
        watch(w)
        if S["phase"] == "done" or S["t"] > 400.0:
            log("done: %d failed" % S["fails"] if S["phase"] == "done" else "FAIL timeout")
            cmd(w, "quit")
            S["phase"] = "quit"
    except Exception as ex:
        # an exception in a slate tick repeats forever: report it and leave
        log("FAIL exception %r at step %d" % (ex, S["step"]))
        cmd(w, "quit")
        S["phase"] = "quit"


unreal.register_slate_post_tick_callback(tick)
