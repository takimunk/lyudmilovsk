"""Offscreen test of the town's public spaces (ALyudPublicSpaces, ULyudPublicSpotsSubsystem; docs/city/public_spaces.md)
in Lvl_Scene01: no PIE, no window. Each shot sets the calendar, stands the camera at a place (terrace frame: s metres
inland, y along the coast, z over the terrace), waits, logs "CITYSHOT <name>" with the frame times and the spaces' state,
checks it and takes a HighResShot. Then the spots API: find, reserve, seat, release.

  unreal/scripts/test_public_spaces.sh          (CITY_SHOTS=square,fountain for some only; CITY_AB=1 adds the A/B rounds)
"""
import math
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
import city_plan as C  # noqa: E402
import world_map as W  # noqa: E402

FACE_Y = 790.0
RES = os.environ.get("CITY_RES", "1600x900")
D = unreal.DateTime
DAY = D(2026, 10, 21, 13, 0, 0, 0)
DUSK = D(2026, 10, 21, 18, 40, 0, 0)
NIGHT = D(2026, 10, 21, 22, 30, 0, 0)
JULY = D(2027, 7, 14, 13, 0, 0, 0)
T0, T1 = -39.5, -36.2
FS, FY = 150.0, 905.0                      # the fountain, terrace frame


def p(s, y, z):
    x, yy = C.to_map(s, y)
    return (x, yy, z)


# (name, camera, target, date, commands, checks)
SHOTS = [
    ("square", p(110, 880, T1 + 1.7), p(FS, FY, T1 + 1.5), DAY, [], "fountain"),
    ("fountain", p(140, 897, T1 + 1.7), p(FS, FY, T1 + 2.4), DAY, [], "fountain"),
    ("fountain_rim", p(143, 912, T1 + 1.1), p(FS, FY, T1 + 0.6), DAY, [], ""),
    ("square_air", p(85, 860, T1 + 28.0), p(FS, FY, T1), DAY, [], ""),
    ("square_dusk", p(110, 880, T1 + 1.7), p(FS, FY, T1 + 1.5), DUSK, [], "night"),
    ("square_night", p(118, 925, T1 + 1.7), p(FS, FY, T1 + 1.5), NIGHT, [], "night"),
    ("stage", p(170, 905, T1 + 1.7), p(206, 905, T1 + 3.0), DAY, [], ""),
    ("promenade", p(40, 840, T0 + 1.7), p(40, 960, T0 + 1.2), DAY, [], ""),
    ("promenade_sea", p(55, 1000, T0 + 1.7), p(10, 1010, T0 + 0.0), DAY, [], ""),
    ("promenade_air", p(70, 820, T0 + 25.0), p(30, 960, T0), DAY, [], ""),
    ("promenade_night", p(40, 840, T0 + 1.7), p(40, 960, T0 + 1.2), NIGHT, [], "night"),
    ("beach_summer", p(6, 950, -41.0), p(30, 990, -39.0), JULY, ["lyud.City.Summer -1"], "summer"),
    ("beach_october", p(6, 950, -41.0), p(30, 990, -39.0), DAY, [], "nosummer"),
    ("pier", p(28, 905, T0 + 1.7), p(-150, 905, T0 + 1.0), DAY, [], ""),
    ("pier_night", p(28, 905, T0 + 1.7), p(-150, 905, T0 + 1.0), NIGHT, [], "night"),
    ("park", p(200, 1120, T1 + 1.7), p(255, 1200, T1 + 1.0), DAY, [], ""),
    ("pond", p(250, 1225, T1 + 1.7), p(270, 1260, T1 + 0.3), DAY, [], ""),
    ("pond_ice", p(250, 1225, T1 + 1.7), p(270, 1260, T1 + 0.3), D(2027, 1, 20, 13, 0, 0, 0), [], "ice"),
    ("playground", p(214, 1140, T1 + 1.7), p(230, 1150, T1 + 0.5), DAY, [], ""),
    ("park_night", p(200, 1120, T1 + 1.7), p(255, 1200, T1 + 1.0), NIGHT, [], "night"),
    ("fair", p(95, 1170, T1 + 1.7), p(140, 1210, T1 + 1.2), DAY, [], ""),
    ("fair_lane", p(135, 1140, T1 + 1.7), p(135, 1260, T1 + 1.5), DAY, [], ""),
    ("fair_night", p(135, 1140, T1 + 1.7), p(135, 1260, T1 + 1.5), NIGHT, [], "night"),
    ("club_stair", (21.0, 308.0, T1 + 1.7), (13.0, 215.0, -20.0), DAY, [], ""),
    ("club_stair_top", (9.0, 172.0, 1.7), (14.0, 215.0, -25.0), DAY, [], ""),
    ("cascade", (-268.28, 739.29, -34.5), (-280.41, 756.28, -32.7), JULY, ["lyud.Fountain.Force 1"], ""),
    ("cascade_close", (-273.87, 747.95, -34.6), (-279.12, 752.5, -35.4), JULY, [], ""),
    ("cascade_air", (-577.82, 719.01, 8.0), (-606.86, 760.93, -22.0), JULY, [], ""),
    ("cascade_night", (-268.28, 739.29, -34.5), (-280.41, 756.28, -32.7), D(2027, 7, 14, 23, 30, 0, 0), [], "night"),
    # close-ups of the kit (the 3D glitch rule: seams, z-fighting, inside-out faces, stretched patterns)
    ("kit_bench_square", p(FS + 15.2, FY + 2.0, T1 + 1.3), p(FS + 17.0, FY + 0.6, T1 + 0.4), DAY, [], ""),
    ("kit_bench_promenade", p(58.0, 418.0, T0 + 1.2), p(62.0, 420.0, T0 + 0.5), DAY, [], ""),
    ("kit_lamp", p(FS - 12.0, FY + 1.0, T1 + 4.0), p(FS - 15.0, FY, T1 + 4.6), DAY, [], ""),
    ("kit_stall", p(126.0, 1160.0, T1 + 1.6), p(129.5, 1157.0, T1 + 1.2), DAY, [], ""),
    ("kit_carousel", p(150.0, 1210.0, T1 + 2.5), p(163.0, 1210.0, T1 + 2.0), DAY, [], ""),
    ("kit_stage", p(190.0, 896.0, T1 + 2.0), p(205.0, 905.0, T1 + 2.5), DAY, [], ""),
    ("kit_beach", p(16.0, 645.0, -40.6), p(12.0, 648.0, -41.2), JULY, [], ""),
    ("kit_pier_rail", p(5.0, 908.0, T0 + 1.5), p(-20.0, 910.5, T0 + 0.6), DAY, [], ""),
    ("kit_fountain_sun", p(FS - 5.5, FY + 1.0, T1 + 3.0), p(FS, FY, T1 + 2.9), DAY, [], ""),
]
if os.environ.get("CITY_AB"):
    SHOTS += [("ab_on", p(110, 880, T1 + 1.7), p(FS, FY, T1 + 1.5), DAY, ["lyud.City.Show 1"], ""),
              ("ab_off", p(110, 880, T1 + 1.7), p(FS, FY, T1 + 1.5), DAY, ["lyud.City.Show 0"], ""),
              ("ab_on2", p(110, 880, T1 + 1.7), p(FS, FY, T1 + 1.5), DAY, ["lyud.City.Show 1"], ""),
              ("ab_off2", p(110, 880, T1 + 1.7), p(FS, FY, T1 + 1.5), DAY, ["lyud.City.Show 0"], ""),
              ("abn_on", p(40, 840, T0 + 1.7), p(40, 960, T0 + 1.2), NIGHT, ["lyud.City.Show 1"], ""),
              ("abn_off", p(40, 840, T0 + 1.7), p(40, 960, T0 + 1.2), NIGHT, ["lyud.City.Show 0"], ""),
              ("abn_on2", p(40, 840, T0 + 1.7), p(40, 960, T0 + 1.2), NIGHT, ["lyud.City.Show 1"], ""),
              ("abn_off2", p(40, 840, T0 + 1.7), p(40, 960, T0 + 1.2), NIGHT, ["lyud.City.Show 0"], ""),
              # the street lights alone at night (the pool against glow only), on the square and the promenade
              ("abl_on", p(110, 880, T1 + 1.7), p(FS, FY, T1 + 1.5), NIGHT, ["lyud.City.Show 1", "lyud.City.StreetLights 12"], ""),
              ("abl_off", p(110, 880, T1 + 1.7), p(FS, FY, T1 + 1.5), NIGHT, ["lyud.City.StreetLights 0"], ""),
              ("abl_on2", p(110, 880, T1 + 1.7), p(FS, FY, T1 + 1.5), NIGHT, ["lyud.City.StreetLights 12"], ""),
              ("abl_off2", p(110, 880, T1 + 1.7), p(FS, FY, T1 + 1.5), NIGHT, ["lyud.City.StreetLights 0"], ""),
              ("abl8", p(110, 880, T1 + 1.7), p(FS, FY, T1 + 1.5), NIGHT, ["lyud.City.StreetLights 8"], "")]
if os.environ.get("CITY_SHOTS"):
    SHOTS = [s for s in SHOTS if s[0] in os.environ["CITY_SHOTS"].split(",")]
WAIT = float(os.environ.get("CITY_WAIT", "6"))
st = {"t": 0.0, "i": -1, "phase": 0, "frames": [], "fail": 0, "api": False}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name().startswith("Lvl_") and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def ue(q):
    return W.to_unreal(q[0], q[1], q[2], FACE_Y)


def first(cls, w):
    for o in unreal.ObjectIterator(cls):
        if o.get_world() == w:
            return o


def check(ok, what):
    if not ok:
        st["fail"] += 1
    unreal.log_warning("CITYTEST %s %s" % ("ok" if ok else "FAIL", what))


def api_test(w):
    sub = first(unreal.LyudPublicSpotsSubsystem, w)
    check(sub is not None, "spots subsystem exists")
    if not sub:
        return
    n = sub.num_spots()
    check(n > 200, "%d spots loaded" % n)
    sp = sub.get_spaces()
    check(sp is not None, "public spaces built")
    if sp:
        tags = [str(t) for t in sp.tags]
        check("PlanStage" in tags and "PlanStalls" in tags, "the festivals' tags PlanStage/PlanStalls set (%s)" % tags)
        for kind in ("bench_park", "bench_promenade", "lamp", "railing", "stall", "wheel", "carousel"):
            check(sp.count_props(kind) > 0, "%s: %d" % (kind, sp.count_props(kind)))
    near = ue(p(FS + 17.0, FY, T1))
    bench = sub.find_nearest_free("bench", unreal.Vector(*near), 3000.0, "None")
    check(str(bench) != "None", "a free bench by the fountain: %s" % bench)
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    who = [pc, pc.get_controlled_pawn(), sub, w]          # any four objects stand for four people
    seats = [sub.reserve(bench, x) for x in who]
    check(seats[:3] == [0, 1, 2] and seats[3] == -1, "three seats then full: %s" % seats)
    check(sub.reserve(bench, who[1]) == 1, "the same sitter keeps its seat")
    seat = sub.get_seat(bench, 1)
    unreal.log_warning("CITYTEST seat 1 of %s at %s" % (bench, seat.translation))
    sub.release(bench, who[1])
    check(sub.free_places(bench) == 1, "release gives the seat back")
    sub.release_all(who[0])
    sub.release_all(who[2])
    rim = sub.find_nearest_free("fountain", unreal.Vector(*near), 3000.0, "None")
    check(str(rim) == "S-FOUNTAIN", "the fountain's rim is a spot: %s" % rim)
    res = sub.get_spot("S-FOUNTAIN")
    spot = res[1] if isinstance(res, tuple) else res       # Python gets the out struct (and maybe the bool)
    if spot is not None:
        check(len(spot.seats) == 24, "the rim seats 24 (%d)" % len(spot.seats))
    fs = first(unreal.LyudFountainSubsystem, w)
    if fs:
        fo = fs.get_fountains()
        check(len(fo) >= 2, "%d fountains (the Academgorodok's and Sun Square's)" % len(fo))


def tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < 5.0:
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    if not st["api"]:
        st["api"] = True
        try:
            api_test(w)
        except Exception as e:  # noqa: BLE001
            check(False, "the spots API test raised %s" % e)
    if st["phase"] == 0:
        st["i"] += 1
        if st["i"] >= len(SHOTS):
            unreal.log_warning("CITYTEST done: %d failed" % st["fail"])
            cmd(w, "quit")
            st["phase"] = 9
            return
        name, cam, tgt, when, cmds, _ = SHOTS[st["i"]]
        pre = ["t.MaxFPS 0", "au.LinearGainScalarForFinalOutut 0", "lyud.Weather.Preset clear"] if st["i"] == 0 else []
        pre += [c for c in os.environ.get("CITY_CMDS", "").split(";") if c] if st["i"] == 0 else []
        for c in pre + cmds:
            cmd(w, c)
        for cal in unreal.ObjectIterator(unreal.GameCalendar):
            if cal.get_world() == w:
                cal.set_now(when)
        if hasattr(pawn, "character_movement"):
            pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
        a, b = ue(cam), ue(tgt)
        eye = 0.0
        cam_comp = pawn.get_component_by_class(unreal.CameraComponent)
        if cam_comp:
            eye = cam_comp.get_world_location().z - pawn.get_actor_location().z
        pawn.set_actor_enable_collision(False)
        pawn.set_actor_location(unreal.Vector(a[0], a[1], a[2] - eye), False, True)
        dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
        pc.set_control_rotation(unreal.Rotator(roll=0, pitch=math.degrees(math.atan2(dz, math.hypot(dx, dy))),
                                               yaw=math.degrees(math.atan2(dy, dx))))
        st["t0"] = st["t"]
        st["phase"] = 1
    elif st["phase"] == 1 and st["t"] - st["t0"] <= WAIT + (30.0 if st["i"] == 0 else 0.0):
        if st["t"] - st["t0"] > 2.0:
            st["frames"].append(dt)
    elif st["phase"] == 1:
        name, _, _, _, _, want = SHOTS[st["i"]]
        f = sorted(st["frames"]) or [0.0]
        sub = first(unreal.LyudPublicSpotsSubsystem, w)
        sp = sub.get_spaces() if sub else None
        rep = sp.debug_report() if sp else "no spaces"
        unreal.log_warning("CITYSHOT %s at %s: %d frames, median %.1f ms, worst %.1f ms; %s" % (
            name, pawn.get_actor_location(), len(f), 1000 * f[len(f) // 2], 1000 * f[-1], rep))
        if sp and want == "night":
            check(sp.is_night() and "pooled lights lit of" in rep and not rep.split("glow ")[1].startswith("0.00"),
                  "%s: lamps lit (%s)" % (name, rep))
        if sp and want == "summer":
            check(sp.is_summer(), "%s: beach furniture out" % name)
        if sp and want == "nosummer":
            check(not sp.is_summer(), "%s: beach furniture away" % name)
        if sp and want == "ice":
            check("ice 1" in rep, "%s: the pond frozen" % name)
        if want == "fountain":
            fs = first(unreal.LyudFountainSubsystem, w)
            running = [f.is_running() for f in fs.get_fountains()] if fs else []
            check(len(running) >= 2 and all(running), "%s: fountains running %s" % (name, running))
        st["frames"] = []
        if not os.environ.get("CITY_NULLRHI"):
            cmd(w, "HighResShot " + RES)
        st["phase"] = 2
        st["t0"] = st["t"]
    elif st["phase"] == 2 and st["t"] - st["t0"] > 1.5:
        st["phase"] = 0


unreal.register_slate_post_tick_callback(tick)
