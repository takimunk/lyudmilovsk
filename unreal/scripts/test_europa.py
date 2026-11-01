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
    # the line waits for the island on the planet's face toward us: an hour when it is (the ephemeris says)
    (66.2, lambda w: cmd(w, "lyud.Europa.FindIsland 30 1")),
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
elif os.environ.get("EUROPATEST_ONLY") == "moon":
    # the Moon's look (docs/europa.md): the ground from eye level and from above, the planet with its town turned
    # toward us and away, the stars, a lead's water, a ladder climbed hand over hand (a free-standing one on the ice,
    # watched from the side and from the eyes). MOON_HOUR_ON / MOON_HOUR_OFF pin the hour offsets (else searched).
    M = {"on": None, "off": None, "h": -12}

    def find_hours(w):
        # step the sky's hour until the town faces Europa and until it doesn't
        if M["h"] > 12:
            return
        cmd(w, "lyud.SkyHourOffset %d" % M["h"])

    def note_hour(w):
        f = eu(w).is_island_facing()
        rep = str(eu(w).report())
        up = float(rep.split("up ")[1].split(" ")[0]) if "up " in rep else 0.0
        # the hour the town faces Europa most squarely
        if f and up > M.get("best", 0.0):
            M["best"] = up
            M["on"] = M["h"]
        if not f and M["off"] is None:
            M["off"] = M["h"]
        M["h"] += 2

    def hour(w, which):
        h = os.environ.get("MOON_HOUR_" + which.upper())
        h = int(h) if h else M[which]
        log("hour offset for island %s: %s" % (which, h))
        cmd(w, "lyud.SkyHourOffset %s" % (h if h is not None else 0))

    def look(w, yaw, pitch):
        pc(w).set_control_rotation(unreal.Rotator(0.0, pitch, yaw))

    def ground_at(w, x, y):
        s = eu(w).get_site().get_actor_location()
        a, b = unreal.Vector(s.x + x, s.y + y, s.z + 30000.0), unreal.Vector(s.x + x, s.y + y, s.z - 30000.0)
        hit = unreal.SystemLibrary.line_trace_single(w, a, b, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [pawn(w)],
                                                     unreal.DrawDebugTrace.NONE, True)
        z = hit.to_tuple()[4].z if hit else s.z
        return unreal.Vector(s.x + x, s.y + y, z + 120.0)

    def go(w, x, y):
        pawn(w).set_actor_location(ground_at(w, x, y), False, True)

    def look_dir(w, d, down=0.0):
        pc(w).set_control_rotation(unreal.Rotator(0.0, math.degrees(math.asin(max(-1, min(1, d.z)))) - down, math.degrees(math.atan2(d.y, d.x))))

    P = [(30.0, start), (30.5, lambda w: cmd(w, "lyud.Europa.Fast 0")), (31.0, lambda w: eu(w).go_europa())]
    t = 32.0
    for k in range(13):
        P.append((t, find_hours))
        P.append((t + 0.8, note_hour))
        t += 1.2
    # the town toward Europa by day (Europa over it near noon: near new moon) and by night (its lights), found from the
    # ephemeris itself (lyud.Europa.FindIsland), nothing placed by hand
    P += [(t, lambda w: cmd(w, "lyud.SkyHourOffset 0")), (t + 0.5, lambda w: cmd(w, "lyud.Europa.FindIsland 30 1")),
          (t + 2.5, look_planet), (t + 4.0, lambda w: shot(w, "earth_island_day")), (t + 4.3, report),
          (t + 4.5, lambda w: look_dir(w, eu(w).planet_direction(), 30.0)), (t + 6.0, lambda w: shot(w, "earth_island_over_ground")),
          (t + 6.5, lambda w: cmd(w, "lyud.SkyHourOffset 0")), (t + 7.0, lambda w: cmd(w, "lyud.Europa.FindIsland 30 0")),
          (t + 9.0, look_planet), (t + 10.5, lambda w: shot(w, "earth_island_night_lights")), (t + 10.8, report)]
    t += 11.5
    P += [(t, lambda w: hour(w, "off")), (t + 2.0, look_planet), (t + 4.0, lambda w: shot(w, "earth_ocean_only")), (t + 4.3, report)]
    t += 5.0
    # the stars: straight up and away from the sun
    P += [(t, lambda w: look(w, 300.0, 70.0)), (t + 2.0, lambda w: shot(w, "stars_up")), (t + 2.5, lambda w: look(w, 30.0, 25.0)),
          (t + 4.0, lambda w: shot(w, "stars_horizon"))]
    t += 5.0
    # Europa's day: its sun up (its day is a month long: step the hour by two days at a time)
    D = {"h": 0, "day": None}

    def sky_actor(w):
        return unreal.GameplayStatics.get_all_actors_of_class(w, unreal.LyudSky)[0]

    def find_day(w):
        if D["day"] is None:
            cmd(w, "lyud.SkyHourOffset %d" % D["h"])

    def note_day(w):
        if D["day"] is None:
            alt = sky_actor(w).sun_altitude()
            log("hour offset %d: Europa's sun at %.1f deg" % (D["h"], alt))
            if 18.0 < alt < 50.0:
                D["day"] = D["h"]
            D["h"] += 48

    def day(w):
        h = os.environ.get("MOON_HOUR_DAY")
        h = int(h) if h else (D["day"] if D["day"] is not None else 0)
        log("hour offset for Europa's day: %d (sun at %.1f)" % (h, sky_actor(w).sun_altitude()))
        cmd(w, "lyud.SkyHourOffset %d" % h)

    for k in range(16):
        P += [(t, find_day), (t + 0.8, note_day)]
        t += 1.2
    P += [(t, day), (t + 2.0, look_planet), (t + 4.0, lambda w: shot(w, "earth_day")), (t + 4.3, report)]
    t += 5.0
    # frame time on the ice by day (ms over ~4 s; a busy shared machine: compare only within one run)
    FT = {"n": 0, "s": 0.0, "on": False}

    def ft_on(w):
        FT.update(on=True, n=0, s=0.0)

    def ft_off(w):
        FT["on"] = False
        log("frame time on Europa by day: %.1f ms over %d frames" % (1000.0 * FT["s"] / max(FT["n"], 1), FT["n"]))
    S["ft"] = FT
    P += [(t, lambda w: look(w, 125.0, -4.0)), (t + 1.0, ft_on), (t + 5.0, ft_off)]
    t += 5.5
    # the ground at eye level all round, and a wide look from high over the hatch
    for i, yaw in enumerate((0.0, 60.0, 125.0, 190.0, 250.0, 310.0)):
        P += [(t, lambda w, yaw=yaw: look(w, yaw, -4.0)), (t + 1.5, lambda w, i=i: shot(w, "ground_eye_%d" % i))]
        t += 2.0
    for i, (x, y, yaw) in enumerate(((-12000.0, -3000.0, 160.0), (26000.0, 52000.0, 230.0), (3000.0, -30000.0, 80.0))):
        P += [(t, lambda w, x=x, y=y: go(w, x, y)), (t + 3.0, lambda w, yaw=yaw: look(w, yaw, -6.0)),
              (t + 4.5, lambda w, i=i: shot(w, "ground_far_%d" % i))]
        t += 5.0
    P += [(t, lambda w: eu(w).go_europa()), (t + 1.0, lambda w: cmd(w, "lyud.Hero.ViewBehind 9000")),
          (t + 2.0, lambda w: look(w, 300.0, -28.0)), (t + 5.0, lambda w: shot(w, "ground_wide")),
          (t + 5.5, lambda w: look(w, 125.0, -22.0)), (t + 8.0, lambda w: shot(w, "ground_wide_planet")),
          (t + 8.5, lambda w: cmd(w, "lyud.Hero.ViewBehind 0"))]
    t += 9.0
    # the near lead: from its rim, looking down into the water
    def at_lead(w, k=0, side=1.0):
        # the near lead from Content/Europa/moon.json: on its rim, looking across and down into it
        import json
        with open(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()), "Europa", "moon.json")) as f:
            lead = json.load(f)["water"][k]
        c, st = lead["centre_cm"], lead["strike"]
        n = (-st[1] * side, st[0] * side)
        off = lead["half_width_cm"] * 0.6 + 900.0
        p = ground_at(w, c[0] + n[0] * off + st[0] * 6000.0, c[1] + n[1] * off + st[1] * 6000.0)
        pawn(w).set_actor_location(p, False, True)
        M["lead"] = unreal.Vector(-n[0], -n[1], 0.0)
    for k, side in ((0, 1.0), (0, -1.0), (1, 1.0), (1, -1.0)):
        P += [(t, lambda w, k=k, side=side: at_lead(w, k, side)), (t + 3.0, lambda w: look_dir(w, M["lead"], 18.0)),
              (t + 4.5, lambda w, k=k, side=side: shot(w, "water_lead%d_%s" % (k, "a" if side > 0 else "b"))),
              (t + 5.0, lambda w: look_dir(w, M["lead"], 40.0)),
              (t + 6.5, lambda w, k=k, side=side: shot(w, "water_lead%d_%s_down" % (k, "a" if side > 0 else "b")))]
        t += 7.0
    # a ladder on the ice, climbed hand over hand
    P += [(t, lambda w: eu(w).go_europa()), (t + 1.0, lambda w: look(w, 0.0, 0.0)), (t + 1.5, lambda w: cmd(w, "lyud.Ladder.Spawn 16 70")),
          (t + 2.5, lambda w: cmd(w, "lyud.Ladder.Grab")), (t + 3.5, lambda w: cmd(w, "lyud.Ladder.AutoClimb 0.2"))]
    t += 4.0
    for i in range(6):
        P += [(t, lambda w: cmd(w, "lyud.Hero.ViewBehind 260")), (t + 0.05, lambda w: look(w, pawn(w).get_actor_rotation().yaw + 90.0, 5.0)),
              (t + 0.3, lambda w, i=i: shot(w, "ladder_side_%d" % i)), (t + 0.35, lambda w: cmd(w, "lyud.Ladder.Report"))]
        t += 0.45
    body = lambda w: pawn(w).get_actor_rotation().yaw
    P += [(t, lambda w: cmd(w, "lyud.Hero.ViewBehind 110")), (t + 0.05, lambda w: look(w, body(w) + 25.0, 30.0)),
          (t + 0.3, lambda w: shot(w, "ladder_hands_shoulder")), (t + 0.35, lambda w: cmd(w, "lyud.Hero.ViewBehind 0"))]
    t += 0.5
    def look_ladder(w, up):
        lad = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.LyudLadder)
        if lad:
            eye = pawn(w).get_actor_location() + unreal.Vector(0, 0, 70)
            d = lad[0].get_actor_location() + unreal.Vector(0, 0, eye.z - lad[0].get_actor_location().z + up) - eye
            pc(w).set_control_rotation(unreal.Rotator(0.0, math.degrees(math.atan2(d.z, math.hypot(d.x, d.y))), math.degrees(math.atan2(d.y, d.x))))
    P += [(t, lambda w: cmd(w, "lyud.Hero.ViewBehind 0")), (t + 0.05, lambda w: look_ladder(w, 25.0)), (t + 0.3, lambda w: shot(w, "ladder_hands_fpv")),
          (t + 0.6, lambda w: look_ladder(w, -70.0)), (t + 0.9, lambda w: shot(w, "ladder_feet_fpv")),
          (t + 1.0, lambda w: cmd(w, "lyud.Ladder.AutoClimb 0"))]
    t += 1.5
    P += [(t, report), (t + 0.5, lambda w: S.update(phase="done"))]
    PLAN = P
elif os.environ.get("EUROPATEST_ONLY") == "lead":
    # the leads' water by Europa's day, from above and from the rim, and the hands on the rungs from the eyes
    import json

    def lead(k):
        with open(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()), "Europa", "moon.json")) as f:
            return json.load(f)["water"][k]

    def over(w, k, back, up, pitch):
        L = lead(k)
        s = eu(w).get_site().get_actor_location()
        c = unreal.Vector(s.x + L["centre_cm"][0], s.y + L["centre_cm"][1], s.z + L["z_cm"])
        n = unreal.Vector(-L["strike"][1], L["strike"][0], 0.0)
        cam = c + n * back + unreal.Vector(0, 0, up)
        pawn(w).set_actor_location(cam, False, True)
        pawn(w).get_editor_property("character_movement").set_movement_mode(unreal.MovementMode.MOVE_FLYING)
        d = c - cam
        pc(w).set_control_rotation(unreal.Rotator(0.0, math.degrees(math.atan2(d.z, math.hypot(d.x, d.y))) + pitch, math.degrees(math.atan2(d.y, d.x))))

    P = [(30.0, start), (30.5, lambda w: cmd(w, "lyud.Europa.Fast 0")), (31.0, lambda w: eu(w).go_europa()),
         (32.0, lambda w: cmd(w, "lyud.SkyHourOffset 96"))]
    t = 34.0
    for k in (0, 1):
        for i, (back, up, pitch) in enumerate(((0.0, 9000.0, 0.0), (3500.0, 900.0, 0.0), (-3500.0, 900.0, 0.0), (1500.0, 300.0, -5.0))):
            P += [(t, lambda w, k=k, b=back, u=up, p=pitch: over(w, k, b, u, p)),
                  (t + 2.5, lambda w, k=k, i=i: shot(w, "lead%d_view%d" % (k, i)))]
            t += 3.0
    P += [(t, lambda w: pawn(w).get_editor_property("character_movement").set_movement_mode(unreal.MovementMode.MOVE_FALLING)),
          (t + 0.5, lambda w: eu(w).go_europa()), (t + 1.5, lambda w: pc(w).set_control_rotation(unreal.Rotator(0.0, 0.0, 0.0))),
          (t + 2.0, lambda w: cmd(w, "lyud.Ladder.Spawn 16 70")), (t + 3.0, lambda w: cmd(w, "lyud.Ladder.Grab")),
          (t + 4.0, lambda w: cmd(w, "lyud.Ladder.AutoClimb 0.2")), (t + 7.0, lambda w: cmd(w, "lyud.Ladder.AutoClimb 0"))]
    t += 7.5
    for i, pitch in enumerate((-20.0, -35.0, 10.0)):
        # toward the ladder: its -X is the way the climber faces
        face = lambda w: unreal.GameplayStatics.get_all_actors_of_class(w, unreal.LyudLadder)[0].get_actor_rotation().yaw + 180.0
        P += [(t, lambda w, p=pitch: pc(w).set_control_rotation(unreal.Rotator(0.0, p, face(w)))),
              (t + 0.4, lambda w, i=i: shot(w, "ladder_eyes_%d" % i))]
        t += 0.6
    P += [(t, report), (t + 0.5, lambda w: S.update(phase="done"))]
    PLAN = P
elif os.environ.get("EUROPATEST_ONLY") == "hatch":
    PLAN = [(30.0, start), (31.0, at_hatch), (31.2, lambda w: report(w)), (31.5, look_lid), (32.0, lambda w: report(w)),
            (33.0, lambda w: report(w)), (34.0, lambda w: action(w, "open_sea")), (34.5, lambda w: S.update(phase="done"))]


def tick(dt):
    w = world()
    if not w:
        return
    try:
        if S.get("ft") and S["ft"]["on"]:
            S["ft"]["n"] += 1
            S["ft"]["s"] += dt
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
