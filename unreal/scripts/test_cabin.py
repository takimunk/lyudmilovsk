"""Offscreen check of the cable-car cabin as a room (docs/transport.md "The cabin"): the player waits on a platform (Akademgorodok),
the cabin comes and opens its plug door, the player WALKS in through the opening (movement input, no teleport), looks
round the inside (benches, windows, the shut door), rides to Zenith, walks out through the door; then the
cabin from outside, close. TRANSPORTTEST lines in the log; shots in Saved/TransportTest (test_transport.sh SCRIPT=...).

  SCRIPT=test_cabin.py unreal/scripts/test_transport.sh
"""
import math

import unreal

import os

# an elevated station to an elevated one (Aton's ground platform still has the old vestibule roof over its edge)
ROUTE = tuple(os.environ.get("CABIN_ROUTE", "academ zenit").split())
S = {"t": 0.0, "phase": "wait", "pt": 0.0, "shots": 0, "cabin": None, "last": 0.0, "fails": 0}


def log(msg):
    unreal.log_warning("TRANSPORTTEST " + msg)


def fail(msg):
    S["fails"] += 1
    log("FAIL " + msg)


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


def nearest_cabin(w, loc, r):
    best, bd = None, r
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.LyudCabin):
        d = (a.get_actor_location() - loc).length()
        if d < bd:
            best, bd = a, d
    return best


def local(c, p):
    """p (world) in the cabin's frame."""
    return c.get_actor_transform().inverse_transform_location(p)


def look_at(pc, pawn, target):
    d = target - (pawn.get_actor_location() + unreal.Vector(0, 0, 70))
    yaw = math.degrees(math.atan2(d.y, d.x))
    pitch = math.degrees(math.atan2(d.z, math.hypot(d.x, d.y)))
    pc.set_control_rotation(unreal.Rotator(0.0, pitch, yaw))


def walk_to(pawn, target):
    d = target - pawn.get_actor_location()
    d.z = 0
    if d.length() > 1:
        pawn.add_movement_input(d.normal(), 1.0)
    return d.length()


def probe(w, c, pawn, what):
    """Where the player is in the cabin's frame, how fast, on what, and what is straight ahead of them."""
    L = local(c, pawn.get_actor_location())
    v = pawn.get_velocity()
    mv = pawn.get_movement_component()
    base = pawn.get_movement_base() if hasattr(pawn, "get_movement_base") else None
    if not S.get("dir_logged"):
        S["dir_logged"] = True
        log("pawn %s base methods %s" % (type(pawn).__name__, [m for m in dir(pawn) if "base" in m.lower()]))
    fwd = pawn.get_control_rotation().get_forward_vector() if hasattr(pawn, "get_control_rotation") else unreal.Vector(1, 0, 0)
    a = pawn.get_actor_location()
    hit = unreal.SystemLibrary.line_trace_single(w, a, a + unreal.Vector(fwd.x, fwd.y, 0) * 150.0,
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [pawn],
                                                 unreal.DrawDebugTrace.NONE, True)
    hb = hit.to_tuple() if hit else None
    ahead = "clear"
    if hb and hb[0]:
        comp = hb[10]
        what = comp.get_name() if comp else None
        if isinstance(comp, unreal.InstancedStaticMeshComponent):
            m = comp.static_mesh
            it = comp.get_instance_transform(hb[13], True)
            what += " %s instance %d at %s scale %s" % (m.get_name() if m else None, hb[13], local(c, it.translation), it.scale3d)
        ahead = "%s/%s at %.0f" % (hb[9].get_name() if hb[9] else None, what, hb[3])
    log("%s: local (%.0f %.0f %.0f) speed %.0f mode %s base %s ahead %s" % (
        what, L.x, L.y, L.z, v.length(), mv.get_editor_property("movement_mode") if mv else "?",
        base.get_owner().get_name() + "/" + base.get_name() if base else None, ahead))


def step(w, dt):
    t = unreal.LyudTransportSubsystem.get_transport(w)
    pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    S["pt"] += dt
    ph = S["phase"]
    if ph == "wait" and S["t"] > 80.0:
        cmd(w, "Transport.Ride %s %s" % ROUTE)
        S["phase"], S["pt"] = "call", 0.0
    elif ph == "call":
        c = nearest_cabin(w, pawn.get_actor_location(), 2500.0)
        if c and c.door > 0.95:
            S["cabin"] = c
            # stand on the platform 2.2 m out from the door, facing it
            door = c.door_point()
            dl = local(c, door)
            # straight out from the door, past the platform's edge line, clear of the benches further in
            out = c.get_actor_transform().transform_location(unreal.Vector(dl.x, dl.y - 130.0, 100.0))
            pawn.set_actor_location(out, False, True)
            look_at(pc, pawn, door + unreal.Vector(0, 0, 110))
            S["phase"], S["pt"] = "look_door", 0.0
            S["lp"] = -1
            log("cabin %s open at the first station, door %s floor z %.0f player %s" % (c.get_name(), door, c.get_actor_location().z, out))
        elif S["pt"] > 150.0:
            fail("no cabin opened its door in 150 s")
            S["phase"] = "done"
    elif ph == "look_door":
        if S["pt"] > 1.5 and not S.get("s1"):
            S["s1"] = True
            shot(w, "door_open_from_platform")
        if S["pt"] > 2.5:
            S["phase"], S["pt"] = "walk_in", 0.0
    elif ph == "walk_in":
        c = S["cabin"]
        inside = c.get_actor_transform().transform_location(unreal.Vector(0, 0, 100))
        walk_to(pawn, inside)
        L = local(c, pawn.get_actor_location())
        if S["pt"] - S.get("lp", -1) > 0.5:
            S["lp"] = S["pt"]
            probe(w, c, pawn, "walk_in")
        if abs(L.y) < 40 and abs(L.x) < 120:
            log("walked in through the door in %.1f s (local %s), player cabin %d" % (S["pt"], L, t.player_cabin()))
            S["phase"], S["pt"] = "inside", 0.0
        elif S["pt"] > 8.0:
            fail("could not walk into the cabin (stuck at local %s)" % L)
            S["phase"], S["pt"] = "inside", 0.0
            pawn.set_actor_location(inside, False, True)
    elif ph == "inside":
        c = S["cabin"]
        tr = c.get_actor_transform()
        views = [(1.0, "inside_bench_front", unreal.Vector(160, 0, 60)), (2.5, "inside_bench_back", unreal.Vector(-160, 0, 60)),
                 (4.0, "inside_window_side", unreal.Vector(0, 200, 110)), (5.5, "inside_door", unreal.Vector(47, -200, 100))]
        for at, name, tgt in views:
            if S["pt"] > at - 0.8 and not S.get(name + "_aim"):
                S[name + "_aim"] = True
                look_at(pc, pawn, tr.transform_location(tgt))
            if S["pt"] > at and not S.get(name):
                S[name] = True
                shot(w, name)
        # the route-diagram posters on the inside wall
        for k, poster in enumerate(c.posters):
            at = 6.5 + k * 1.5
            if S["pt"] > at - 0.8 and not S.get("poster%d_aim" % k):
                S["poster%d_aim" % k] = True
                look_at(pc, pawn, poster.get_world_location())
            if S["pt"] > at and not S.get("poster%d" % k):
                S["poster%d" % k] = True
                shot(w, "inside_poster_%d" % k)
        if S["pt"] > 6.5 + 1.5 * len(c.posters):
            log("posters inside: %d" % len(c.posters))
            S["phase"], S["pt"], S["last"], S["lp"] = "riding", 0.0, 0.0, -1
            if os.environ.get("CABIN_QUICK"):
                S["phase"] = "done"
    elif ph == "riding":
        c = S["cabin"]
        idx = t.player_cabin()
        cabs = t.get_cabins()
        st = str(cabs[idx].state) if 0 <= idx < len(cabs) else "?"
        look_at(pc, pawn, c.get_actor_transform().transform_location(unreal.Vector(400, 120, 60)))
        L = local(c, pawn.get_actor_location())
        if S["pt"] - S.get("lp", -1) > (0.25 if S["pt"] < 6.0 else 3.0):
            S["lp"] = S["pt"]
            probe(w, c, pawn, "riding %.2f cabin %s %s" % (S["pt"], c.get_actor_location(), st))
        if S["pt"] - S["last"] > 15.0:
            S["last"] = S["pt"]
            log("riding: player cabin %d %s, player local %s" % (idx, st, L))
            shot(w, "ride_%03d" % int(S["pt"]))
        if abs(L.x) > 250 or abs(L.y) > 140 or L.z < -60:
            fail("the player left the moving cabin (local %s)" % L)
            S["phase"] = "done"
        if S["pt"] > 20.0 and st == "dwelling" and c.door > 0.95:
            log("arrived after %.0f s" % S["pt"])
            S["phase"], S["pt"] = "walk_out", 0.0
        elif S["pt"] > 400.0:
            fail("the ride did not end in 400 s")
            S["phase"] = "done"
    elif ph == "walk_out":
        c = S["cabin"]
        door = c.door_point()
        out = door + (door - c.get_actor_location()).normal() * 200.0
        if S["pt"] < 0.5:
            look_at(pc, pawn, out + unreal.Vector(0, 0, 100))
        walk_to(pawn, out)
        L = local(c, pawn.get_actor_location())
        if -L.y > 160:
            log("walked out in %.1f s, player cabin %d" % (S["pt"], t.player_cabin()))
            S["phase"], S["pt"] = "outside", 0.0
        elif S["pt"] > 8.0:
            fail("could not walk out of the cabin (stuck at local %s)" % L)
            S["phase"], S["pt"] = "outside", 0.0
    elif ph == "outside":
        c = S["cabin"]
        if S["pt"] > 0.5 and not S.get("o1"):
            S["o1"] = True
            look_at(pc, pawn, c.get_actor_location() + unreal.Vector(0, 0, 150))
        if S["pt"] > 1.5 and not S.get("o2"):
            S["o2"] = True
            shot(w, "cabin_from_the_platform")
            log("player cabin after leaving: %d" % t.player_cabin())
        if S["pt"] > 2.0 and not S.get("o3"):
            # the cabin close from the air, three-quarter view of its door side
            S["o3"] = True
            p = c.get_actor_transform().transform_location(unreal.Vector(520, -620, 380))
            cmd(w, "ghost")
            pawn.set_actor_location(p, False, False)
            look_at(pc, pawn, c.get_actor_location() + unreal.Vector(0, 0, 150))
        if S["pt"] > 3.5 and not S.get("o4"):
            S["o4"] = True
            shot(w, "cabin_close_outside")
        if S["pt"] > 4.0:
            S["phase"] = "done"
    elif ph == "done":
        log("done, %d fails" % S["fails"])
        cmd(w, "quit")
        S["phase"] = "quit"


def tick(dt):
    w = world()
    if not w or S["phase"] == "quit":
        return
    dt = min(dt, 0.25)
    S["t"] += dt
    step(w, dt)
    if S["t"] > 700.0 and S["phase"] != "quit":
        fail("timeout in %s" % S["phase"])
        cmd(w, "quit")
        S["phase"] = "quit"


unreal.register_slate_post_tick_callback(tick)
