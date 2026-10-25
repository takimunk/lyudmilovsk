"""Offscreen check of the cable-car metro (docs/transport.md) in Lvl_Scene01: the board by the club's exit, Aton's
platform, the route diagram, a ride from Aton to the Akademgorodok in a cabin (the player put on the platform, a
cabin called, the player stepped in, out at the other end), the stations and pylons from outside, the map tab.
TRANSPORTTEST lines in the log; screenshots (shot) in Saved/Screenshots in the order of the 'shot' lines.

  unreal/scripts/test_transport.sh
"""
import unreal

S = {"t": 0.0, "step": 0, "phase": "plan", "ride_t": 0.0, "shots": 0, "cabin": None, "last": 0.0}


def log(msg):
    unreal.log_warning("TRANSPORTTEST " + msg)


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


def transport(w):
    return unreal.LyudTransportSubsystem.get_transport(w)


def report(w):
    t = transport(w)
    log("report\n" + (t.report() if t else "no transport"))


def cabins_near(w, loc, r):
    best, bd = None, r
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.LyudCabin):
        d = (a.get_actor_location() - loc).length()
        if d < bd:
            best, bd = a, d
    return best


# fixed views (Unreal cm; BugItGo x y z pitch yaw roll)
PLAN = [
    (84.0, lambda w: report(w)),
    (85.0, lambda w: cmd(w, "BugItGo -193 1200 170 -2 90 0")),
    (88.0, lambda w: shot(w, "door_out")),
    (89.0, lambda w: cmd(w, "BugItGo 330 2250 170 -8 -90 0")),
    (92.0, lambda w: shot(w, "club_board")),
    (93.0, lambda w: cmd(w, "BugItGo 1200 11500 1400 -18 125 0")),
    (97.0, lambda w: shot(w, "aton_from_air")),
    (98.0, lambda w: cmd(w, "Transport.Diagram aton")),
    (100.0, lambda w: shot(w, "diagram")),
    (101.0, lambda w: cmd(w, "Transport.Diagram close")),
    (102.0, lambda w: cmd(w, "Screen.Open map")),
    (104.0, lambda w: shot(w, "map_tab")),
    (105.0, lambda w: cmd(w, "Screen.Open none")),
    (106.0, lambda w: cmd(w, "BugItGo -23000 47000 1500 -12 -60 0")),
    (110.0, lambda w: shot(w, "academ_station")),
    (111.0, lambda w: cmd(w, "BugItGo -42000 86000 2500 -10 -40 0")),
    (115.0, lambda w: shot(w, "zenit_station")),
    (116.0, lambda w: cmd(w, "Transport.Goto academ outside")),
    (119.0, lambda w: shot(w, "academ_vestibule_out")),
    (120.0, lambda w: cmd(w, "Transport.Goto academ vestibule")),
    (123.0, lambda w: shot(w, "academ_vestibule_in")),
    (124.0, lambda w: cmd(w, "Transport.Goto zenit platform")),
    (128.0, lambda w: shot(w, "zenit_platform")),
    (130.0, lambda w: cmd(w, "lyud.Language en")),
    # a person from the club to the library in the Akademgorodok: down the alley to Aton and into a cabin
    (130.5, lambda w: cmd(w, "lyud.People.Order streamer read_library")),
    (131.0, lambda w: cmd(w, "Transport.Ride aton academ")),
    (133.0, lambda w: shot(w, "aton_platform")),
    (133.5, lambda w: start_ride(w)),
]


def start_ride(w):
    S["phase"] = "board"
    S["ride_t"] = 0.0
    report(w)


def ride(w, dt):
    t = transport(w)
    pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    S["ride_t"] += dt
    if S["phase"] == "board":
        c = cabins_near(w, pawn.get_actor_location(), 2500.0)
        if c and 2.0 < S["ride_t"] < 2.3:
            # the cabin from the platform, a few metres off its door
            at = c.get_actor_location() - c.get_actor_right_vector() * 650.0 + unreal.Vector(0, 0, 160)
            pawn.set_actor_location(at, False, True)
            yaw = c.get_actor_rotation().yaw + 90.0
            pc.set_control_rotation(unreal.Rotator(0.0, -8.0, yaw))
        elif c and 3.5 < S["ride_t"] < 3.8 and not S.get("cabin_shot"):
            S["cabin_shot"] = True
            shot(w, "cabin_at_aton")
        if c and S["ride_t"] > 5.0:
            # step in through the door (the cabin's left side)
            loc = c.get_actor_location() + unreal.Vector(0, 0, 100)
            pawn.set_actor_location(loc, False, True)
            S["cabin"] = c
            S["phase"] = "riding"
            S["ride_t"] = 0.0
            log("stepped into %s at %s" % (c.get_name(), loc))
        elif S["ride_t"] > 120.0:
            log("FAIL no cabin came to Aton in 2 min")
            S["phase"] = "outside"
    elif S["phase"] == "riding":
        c = S["cabin"]
        # keep looking ahead out of the window
        r = c.get_actor_rotation()
        pc.set_control_rotation(unreal.Rotator(0.0, -5.0, r.yaw))
        idx = t.player_cabin()
        cabs = t.get_cabins()
        st = str(cabs[idx].state) if 0 <= idx < len(cabs) else "?"
        if S["ride_t"] - S["last"] > 12.0:
            S["last"] = S["ride_t"]
            log("riding: cabin %s player cabin %d %s" % (c.get_actor_location(), idx, st))
            shot(w, "ride_%03d" % int(S["ride_t"]))
        if S["ride_t"] > 20.0 and st == "dwelling":
            log("arrived after %.0f s" % S["ride_t"])
            report(w)
            S["phase"] = "arrived"
            S["ride_t"] = 0.0
        elif S["ride_t"] > 400.0:
            log("FAIL the ride did not end in 400 s")
            report(w)
            S["phase"] = "outside"
    elif S["phase"] == "arrived":
        if S["ride_t"] > 3.0 and S["cabin"]:
            c = S["cabin"]
            out = c.get_actor_location() - c.get_actor_right_vector() * 250.0 + unreal.Vector(0, 0, 100)
            pawn.set_actor_location(out, False, True)
            pc.set_control_rotation(unreal.Rotator(0.0, -5.0, c.get_actor_rotation().yaw + 180.0))
            S["cabin"] = None
        if S["ride_t"] > 6.0:
            shot(w, "academ_platform")
            S["phase"] = "outside"
            S["ride_t"] = 0.0
    elif S["phase"] == "outside":
        # on the Akademgorodok's platform: the streamer from the club comes by cabin too
        if S["ride_t"] - S.get("last_out", -100.0) > 20.0:
            S["last_out"] = S["ride_t"]
            log("player cabin after leaving: %d" % t.player_cabin())
            report(w)
            cmd(w, "lyud.People.Report streamer")
            shot(w, "academ_wait_%03d" % int(S["ride_t"]))
        if S["ride_t"] > float(240):
            log("done")
            cmd(w, "quit")
            S["phase"] = "quit"


def tick(dt):
    w = world()
    if not w:
        return
    dt = min(dt, 0.25)
    S["t"] += dt
    while S["step"] < len(PLAN) and S["t"] > PLAN[S["step"]][0]:
        PLAN[S["step"]][1](w)
        S["step"] += 1
    if S["phase"] not in ("plan", "quit"):
        ride(w, dt)
    if S["t"] > 900.0 and S["phase"] != "quit":
        log("FAIL timeout")
        cmd(w, "quit")
        S["phase"] = "quit"


unreal.register_slate_post_tick_callback(tick)
