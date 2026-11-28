"""Offscreen check of the flight home from Europa (docs/spacecraft.md) in Lvl_Scene01: the craft on the ice by its
ladder, the cabin (seated, the checklist in a wrong order and then in the right one, a record, a lean to the big
porthole), ПУСК, the climb, then every phase by lyud.Flight.Phase with lyud.Flight.Fast: the coast, the approach, the
entry's fire, the canopy, the swap into the real world, the splash, the capsule afloat on the real sea, the swim out,
and the craft put back on Europa. SPACEFLIGHTTEST lines in the log; screenshots in the order of the 'shot' lines
(unreal/scripts/test_spaceflight.sh copies them to Saved/SpaceflightTest). SPACEFLIGHTTEST_ONLY=pad stops before the launch.
SOUND=1 (test_spaceflight.sh) keeps the sound device: the loops and the record are checked only then.
"""
import math
import os

import unreal

S = {"t": 0.0, "step": 0, "shots": 0, "phase": "plan", "fails": 0, "real_alt": None, "max_alt": 0.0, "music": False,
     "loops": 0, "phases": []}


def log(msg):
    unreal.log_warning("SPACEFLIGHTTEST " + msg)


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


def fl(w):
    return unreal.LyudSpaceflightSubsystem.get_spaceflight(w)


def eu(w):
    return unreal.LyudEuropaSubsystem.get_europa(w)


def pawn(w):
    return unreal.GameplayStatics.get_player_pawn(w, 0)


def pc(w):
    return unreal.GameplayStatics.get_player_controller(w, 0)


def report(w):
    log(fl(w).report())


def field(w, name):
    """One number of the report line ('... name value ...')."""
    words = str(fl(w).report()).replace("|", " ").split()
    for i, x in enumerate(words[:-1]):
        if x == name:
            try:
                return float(words[i + 1])
            except ValueError:
                return None
    return None


def phase(w):
    return str(fl(w).phase_id())


def look(w, pitch, yaw_off=0.0):
    cab = fl(w).get_cabin()
    pc(w).set_control_rotation(unreal.Rotator(0.0, pitch, cab.get_actor_rotation().yaw + yaw_off))


def start(w):
    cmd(w, "DisableAllScreenMessages")
    cmd(w, "lyud.Europa.Fast 1")
    cmd(w, "lyud.Birds.Time 12 30")
    # Europa's day is a month and the game starts in its night: 96 h on puts the star 23 degrees up for the pictures
    # (SPACEFLIGHTTEST_HOURS=0 keeps the night)
    cmd(w, "lyud.SkyHourOffset %s" % os.environ.get("SPACEFLIGHTTEST_HOURS", "96"))
    f = fl(w)
    check(f is not None and f.get_cabin() is not None and f.get_stage() is not None, "the craft and its stage are spawned")
    report(w)
    check(phase(w) == "parked", "parked at the start (%s)" % phase(w))
    s = f.splash_point()
    log("splash point %.0f %.0f %.0f (UE cm)" % (s.x, s.y, s.z))


def go_craft(w):
    fl(w).go_craft()


def at_craft(w):
    f = fl(w)
    check(eu(w).is_on_europa(), "on Europa by the craft")
    d = (pawn(w).get_actor_location() - f.get_stage().get_actor_location()).length()
    check(d < 800.0, "by the ladder's foot (%.0f cm from the stage's axis)" % d)
    check(field(w, "built") == 1.0, "the craft is built")
    check(field(w, "stage") == 1.0 and field(w, "outside") == 1.0, "the stage and the cabin's shell are shown")
    report(w)


def ground_z(w, x, y, z):
    """The ground under (x, y) near height z (the site's ice is not flat round the stage), or z."""
    hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(x, y, z + 5000.0), unreal.Vector(x, y, z - 5000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [pawn(w)],
                                                 unreal.DrawDebugTrace.NONE, True)
    t = hit.to_tuple()
    return t[4].z if t[0] else z


def sun_dir(w):
    """The star's world direction from the report ('sun X=.. Y=.. Z=..'), or None."""
    r = str(fl(w).report())
    i = r.find("| sun X=")
    if i < 0:
        return None
    try:
        p = r[i + 6:].split()
        return unreal.Vector(float(p[0][2:]), float(p[1][2:]), float(p[2][2:]))
    except (ValueError, IndexError):
        return None


def back_off(w):
    """A dozen metres from the stage on its sunlit side (else from behind it), standing on the ice, for a picture of
    the whole craft."""
    st = fl(w).get_stage()
    fwd = st.get_actor_forward_vector()
    right = st.get_actor_right_vector()
    sun = sun_dir(w)
    if sun is not None and math.hypot(sun.x, sun.y) > 0.1:
        k = 1300.0 / math.hypot(sun.x, sun.y)
        # a little round from the star, so the light rakes across the craft instead of flattening it
        at = st.get_actor_location() + unreal.Vector(sun.x * k, sun.y * k, 0.0) + right * 450.0
    else:
        # (before the cabin's sky is up the report has no star: the planet's side, where it stands by day)
        at = st.get_actor_location() + fwd * 1300.0 + right * 450.0
    at = unreal.Vector(at.x, at.y, ground_z(w, at.x, at.y, st.get_actor_location().z) + 170.0)
    pawn(w).set_actor_location(at, False, True)
    d = st.get_actor_location() + unreal.Vector(0, 0, 330) - at
    pc(w).set_control_rotation(unreal.Rotator(0.0, math.degrees(math.atan2(d.z, math.hypot(d.x, d.y))), math.degrees(math.atan2(d.y, d.x))))


def go_cabin(w):
    fl(w).go_cabin()


def in_cabin(w):
    f = fl(w)
    check(f.is_hero_aboard() and f.is_hero_seated(), "seated in the cabin")
    check(phase(w) == "prelaunch", "prelaunch once seated (%s)" % phase(w))
    check(field(w, "inside") == 1.0, "the cabin's inside is shown")
    check(field(w, "rig") == 1.0, "the cabin's own sky is up")
    report(w)


def wrong_order(w):
    fl(w).do("sw_gyro")


def wrong_refused(w):
    r = str(fl(w).report())
    check("switches 00000" in r, "the gyros before the power: refused")


def switch(w, name):
    fl(w).do("sw_" + name)


def checklist_done(w):
    r = str(fl(w).report())
    check("switches 11111" in r, "the five switches are on")
    check(field(w, "power") == 1.0, "the panel is alive")
    if os.environ.get("SOUND") == "1":
        check((field(w, "loops") or 0) >= 2, "the cabin's beds are heard (%s loops)" % field(w, "loops"))


def record(w):
    fl(w).do("player")


def record_on(w):
    check(fl(w).is_record_playing(), "a record is turning")
    if os.environ.get("SOUND") == "1":
        check(field(w, "music") == 1.0, "its music plays")


def launch(w):
    f = fl(w)
    f.do("cover")
    f.do("launch")


def launched(w):
    f = fl(w)
    check(f.is_flying(), "launched")
    check(phase(w) == "ascent", "ascent (%s)" % phase(w))
    report(w)


def jump(w, name):
    fl(w).jump_to_phase(name)


def in_phase(w, name):
    check(phase(w) == name, "phase %s (%s at %.1f s)" % (name, phase(w), fl(w).flight_seconds()))
    check(fl(w).is_hero_seated(), "still on the couch in %s" % name)
    report(w)


def fast(w, x):
    cmd(w, "lyud.Flight.Fast %s" % x)


def real_world(w):
    f = fl(w)
    check(f.is_in_real_world(), "in the real world under the canopy")
    c = f.get_cabin().get_actor_location()
    s = f.splash_point()
    check(math.hypot(c.x - s.x, c.y - s.y) < 40000.0, "over the splash point (%.0f m off)" % (math.hypot(c.x - s.x, c.y - s.y) / 100.0))
    check(not eu(w).is_on_europa(), "Europa is left")
    report(w)


def afloat(w):
    f = fl(w)
    check(f.is_capsule_afloat(), "the capsule is afloat (%s)" % phase(w))
    check(f.flights_flown() >= 1, "one flight flown")
    c = f.get_cabin().get_actor_location()
    sea = unreal.LyudSea.get_sea(w)
    lvl = sea.sea_level() if sea else c.z
    check(abs(c.z - lvl) < 150.0, "on the water (%.0f cm over the still sea)" % (c.z - lvl))
    if sea:
        check(sea.depth_at(c) > 250.0, "deep enough under it (%.0f cm)" % sea.depth_at(c))
    check(f.is_hero_seated(), "the hero rode it down on his couch")
    report(w)


def swim_out(w):
    fl(w).do("dev_swim_out")


def swum(w):
    f = fl(w)
    check(not f.is_hero_aboard(), "out of the capsule")
    sw = unreal.LyudSwimComponent.get_player_swim(w)
    check(sw is not None and sw.is_swimming(), "swimming")
    report(w)


def see_capsule(w):
    c = fl(w).get_cabin().get_actor_location()
    p = pawn(w).get_actor_location() + unreal.Vector(0, 0, 60)
    d = c + unreal.Vector(0, 0, 80) - p
    pc(w).set_control_rotation(unreal.Rotator(0.0, math.degrees(math.atan2(d.z, math.hypot(d.x, d.y))), math.degrees(math.atan2(d.y, d.x))))


def reset(w):
    fl(w).reset_flight()


def after_reset(w):
    check(phase(w) == "parked", "the craft is back on Europa, parked (%s)" % phase(w))
    check(not fl(w).is_in_real_world(), "its cabin left the sea")
    report(w)


def summary(w):
    report(w)
    seen = ",".join(S["phases"])
    check(all(p in S["phases"] for p in ("prelaunch", "ascent", "coast", "approach", "entry", "chute", "splash", "afloat")), "every phase was seen (%s)" % seen)
    check(S["max_alt"] > 2000.0, "the cabin climbed over the real ice (%.0f m)" % S["max_alt"])
    check(S["real_alt"] is not None and 500.0 < S["real_alt"] < 1000.0, "the real world began a few hundred metres up (%s m)" % S["real_alt"])
    if os.environ.get("SOUND") == "1":
        check(S["loops"] >= 4, "the flight's loops were heard (%d at once)" % S["loops"])
        check(S["music"], "the record's music played")
    else:
        log("skip the sound checks (no sound device; SOUND=1 to run them)")


def watch(w):
    f = fl(w)
    if not f or not f.get_cabin():
        return
    p = phase(w)
    if p not in S["phases"]:
        S["phases"].append(p)
    if p == "ascent" and f.get_stage():
        S["max_alt"] = max(S["max_alt"], (f.get_cabin().get_actor_location().z - f.get_stage().get_actor_location().z) / 100.0)
    if f.is_in_real_world() and S["real_alt"] is None and f.is_flying():
        sea = unreal.LyudSea.get_sea(w)
        S["real_alt"] = round((f.get_cabin().get_actor_location().z - (sea.sea_level() if sea else 0.0)) / 100.0, 1)
    n = field(w, "loops")
    if n:
        S["loops"] = max(S["loops"], int(n))
    if field(w, "music") == 1.0:
        S["music"] = True


PLAN = [
    (30.0, start),
    (31.0, go_craft), (34.0, at_craft), (34.5, back_off), (36.5, lambda w: shot(w, "craft_on_the_ice")),
    (37.0, go_craft), (38.0, lambda w: look(w, 35.0)), (39.0, lambda w: shot(w, "ladder_and_hatch", True)),
    (40.0, go_cabin), (42.0, in_cabin), (42.5, lambda w: look(w, -18.0, 24.0)), (44.0, lambda w: shot(w, "panel_cold")),
    (44.5, wrong_order), (45.0, wrong_refused), (45.5, lambda w: shot(w, "wrong_order", True)),
    (46.0, lambda w: switch(w, "power")), (48.0, lambda w: switch(w, "air")), (49.0, lambda w: switch(w, "gyro")),
    (54.0, lambda w: switch(w, "seal")), (55.0, lambda w: switch(w, "arm")), (56.0, checklist_done),
    (56.5, lambda w: shot(w, "panel_ready")),
    (57.0, record), (58.5, record_on), (59.0, lambda w: look(w, -5.0, -60.0)), (60.0, lambda w: shot(w, "record_player", True)),
    (60.5, lambda w: fl(w).look_through("big")), (62.5, lambda w: shot(w, "porthole_on_the_pad")),
    (63.0, lambda w: fl(w).look_through("back")),
]
if os.environ.get("SPACEFLIGHTTEST_ONLY") == "pad":
    PLAN += [(64.0, report), (64.5, lambda w: S.update(phase="done"))]
else:
    PLAN += [
        (64.0, lambda w: look(w, -18.0, 24.0)), (65.0, launch), (66.0, launched), (69.0, lambda w: shot(w, "ascent_panel")),
        (69.5, lambda w: fl(w).look_through("big")), (72.0, lambda w: shot(w, "ascent_porthole")),
        # the swap from the real ice to the impostors is near 20 s of the flight: before and after it
        (84.0, lambda w: shot(w, "ascent_before_swap")), (92.0, lambda w: shot(w, "ascent_after_swap")), (92.5, report),
        (93.0, lambda w: jump(w, "coast")), (95.0, lambda w: in_phase(w, "coast")), (96.0, lambda w: jump(w, "150")),
        (98.0, lambda w: fl(w).look_through("big")), (100.0, lambda w: shot(w, "coast_europa_behind")),
        (100.5, lambda w: jump(w, "400")), (102.0, lambda w: fl(w).look_through("big")), (103.5, lambda w: shot(w, "coast_planet_ahead")),
        (103.5, lambda w: fl(w).look_through("left")), (105.5, lambda w: shot(w, "coast_left_porthole")),
        (106.0, lambda w: fl(w).look_through("back")), (107.0, lambda w: look(w, -18.0, 24.0)), (108.0, lambda w: shot(w, "coast_panel")),
        (108.5, lambda w: fl(w).do("nap")), (110.0, lambda w: check(field(w, "nap") == 1.0, "asleep in the coast")),
        (110.5, lambda w: shot(w, "nap")), (111.0, lambda w: fl(w).do("wake")), (112.0, lambda w: check(field(w, "nap") == 0.0, "awake again")),
        (113.0, lambda w: jump(w, "approach")), (115.0, lambda w: in_phase(w, "approach")), (115.5, lambda w: jump(w, "690")),
        # every jump seats the hero again (the view back on the panel): the porthole is looked through after each one
        (117.0, lambda w: fl(w).look_through("big")), (118.5, lambda w: shot(w, "approach_island")),
        (119.0, lambda w: jump(w, "712")), (120.5, lambda w: fl(w).look_through("big")), (122.0, lambda w: shot(w, "approach_low")),
        (122.5, lambda w: jump(w, "entry")), (124.5, lambda w: in_phase(w, "entry")), (125.0, lambda w: jump(w, "750")),
        (126.5, lambda w: fl(w).look_through("big")), (128.0, lambda w: shot(w, "entry_fire")),
        (128.5, lambda w: fl(w).look_through("back")), (130.0, lambda w: shot(w, "entry_cabin")),
        (130.5, lambda w: jump(w, "chute")), (132.5, lambda w: in_phase(w, "chute")), (133.0, lambda w: jump(w, "825.5")),
        (134.5, lambda w: fl(w).look_through("big")), (136.0, lambda w: shot(w, "chute_before_whiteout")),
        (136.5, lambda w: jump(w, "832")), (138.0, lambda w: fl(w).look_through("big")), (140.0, real_world),
        (140.5, lambda w: shot(w, "real_world_whiteout")),
        # out of the cloud (the whiteout is gone at 842 s): the real sea and the island under the canopy
        (152.0, lambda w: shot(w, "real_world_porthole")),
        (152.5, lambda w: fl(w).look_through("left")), (154.5, lambda w: shot(w, "real_world_left")),
        (155.0, lambda w: fl(w).look_through("right")), (157.0, lambda w: shot(w, "real_world_right")),
        (157.5, lambda w: fl(w).look_through("big")), (162.0, lambda w: shot(w, "real_world_low")),
        (162.5, lambda w: fl(w).look_through("back")),
        # the rest of the descent at its own speed would take 25 s more: four times faster
        (163.0, lambda w: fast(w, 4)), (175.0, lambda w: fast(w, 1)), (177.0, afloat), (178.0, lambda w: shot(w, "afloat_cabin")),
        (178.5, lambda w: fl(w).look_through("big")), (180.5, lambda w: shot(w, "afloat_porthole")),
        (181.0, lambda w: fl(w).look_through("back")),
        (182.0, swim_out), (186.0, swum), (186.5, see_capsule), (188.0, lambda w: shot(w, "capsule_on_the_sea")),
        (189.0, reset), (191.0, after_reset), (192.0, summary), (192.5, lambda w: S.update(phase="done")),
    ]


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
