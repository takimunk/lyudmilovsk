"""Offscreen play test of the underwater world (docs/underwater.md): a camera of its own (the hero stays on the beach) goes
down a line of depths off the club's beach, to the Europa hatch's spot, an eelgrass meadow, a moraine reef with its
wrack, Furcellaria and mussels, a school of fish, a hunter, bottom fish, and watches the current lean the weed and carry
the motes. Prints UWTEST lines (the seabed's, the fish's and the current's state per spot) and screenshots each spot
(Saved/Screenshots/MacEditor, in the order of STEPS; test_underwater.sh copies them to Saved/UnderwaterTest/<name>.png).
Leaves the desktop alone (no PIE, no input); run it with test_underwater.sh. UWTEST_ONLY=a,b runs some steps.
"""
import os

import unreal

FACE_Y = 790.0          # the portal face (scene01_layout.json outside.portal_face_y)
DATUM = -4300.0
HATCH = (573.0, 100.0)  # tools/world_map.py EUROPA_HATCH
WAIT = float(os.environ.get("UWTEST_WAIT", "3.0"))


def at(x_m, y_m, z):
    return unreal.Vector(x_m * 100.0, FACE_Y + y_m * 100.0, z)


# (name, where, extra console commands): where = ("depth", m, map y) a spot that deep off the beach, looking out to sea
# 1.4 m over the bed; ("hatch",); ("seabed", kind, back m, up m); ("fish", species, back m)
STEPS = [
    ("depth_2m", ("depth", 2.0, 100.0), []),
    ("depth_5m", ("depth", 5.0, 100.0), []),
    ("depth_9m", ("depth", 9.0, 100.0), []),
    ("depth_13m", ("depth", 13.5, 100.0), []),
    ("hatch", ("hatch",), []),
    ("meadow", ("seabed", 0, 2.6, 0.9), []),
    ("meadow_low", ("seabed", 0, 1.6, 0.35), []),
    ("reef", ("seabed", 10, 4.0, 1.6), []),
    ("reef_close", ("seabed", 10, 2.0, 0.9), []),
    ("wrack", ("seabed", 1, 1.5, 0.5), []),
    ("cobbles", ("seabed", 11, 2.5, 1.2), []),
    ("school_herring", ("fish", "Herring", 2.6, 45), []),
    ("school_sprat", ("fish", "Sprat", 2.3, 35), []),
    ("sandeels", ("fish", "SandEel", 2.0, 20), []),
    ("perch", ("fish", "Perch", 2.9, 19), []),
    ("cod", ("fish", "Cod", 3.9, 32), []),
    ("trout", ("fish", "SeaTrout", 5.2, 26), []),
    ("goby", ("fish", "RoundGoby", 1.2, 32), []),
    ("sandgoby", ("fish", "SandGoby", 0.8, 20), []),
    ("flounder", ("fish", "Flounder", 2.0, 37), []),
    ("turbot", ("fish", "Turbot", 2.1, 48), []),
    ("sculpin", ("fish", "Sculpin", 0.8, 60), []),
    ("eelpout", ("fish", "Eelpout", 0.9, 60), []),
    ("pipefish", ("fish", "Pipefish", 0.8, 60), []),
    # the second wave (docs/ecosystem.md)
    ("smelt", ("fish", "Smelt", 2.2, 35), []),
    ("whitefish", ("fish", "Whitefish", 3.0, 30), []),
    ("salmon", ("fish", "Salmon", 5.5, 30), []),
    ("roach", ("fish", "Roach", 2.4, 25), []),
    ("vimba", ("fish", "Vimba", 2.6, 25), []),
    ("eel", ("fish", "Eel", 1.6, 45), []),
    ("rockgunnel", ("fish", "RockGunnel", 0.7, 45), []),
    ("fourhorn", ("fish", "FourhornSculpin", 0.9, 50), []),
    ("blackgoby", ("fish", "BlackGoby", 0.6, 35), []),
    ("ninespine", ("fish", "NinespineStickleback", 1.0, 35), []),
    ("shrimp", ("fish", "BrownShrimp", 0.35, 30), []),
    ("saduria", ("fish", "Saduria", 0.35, 30), []),
    ("mudcrab", ("fish", "MudCrab", 0.2, 30), []),
    ("gammarus", ("fish", "Gammarus", 0.15, 30), []),
    ("mysids", ("fish", "Mysid", 1.2, 50), []),
    ("seal", ("fish", "GreySeal", 7.0, 45), []),
    # the third wave: the pike in the weed, prawns, the mitten crab, the porpoise, the diving birds
    ("pike", ("fish", "Pike", 2.0, 35), []),
    ("prawn", ("fish", "Prawn", 0.3, 30), []),
    ("mittencrab", ("fish", "MittenCrab", 0.4, 30), []),
    ("porpoise", ("fish", "Porpoise", 5.5, 40), []),
    ("cormorant", ("fish", "Cormorant", 2.5, 40), []),
    ("eider", ("fish", "Eider", 2.0, 40), []),
    ("current_still", ("seabed", 0, 2.4, 0.8), ["lyud.Current.Speed 0"]),
    ("current_fast", ("seabed", 0, 2.4, 0.8), ["lyud.Current.Speed 0.6"]),
    ("current_reef", ("seabed", 1, 1.8, 0.6), ["lyud.Current.Speed 0.6"]),
    ("from_surface", ("depth", 3.0, 120.0, "surface"), ["lyud.Current.Speed -99"]),
    # the surface's underside: from 3 m down over 9 m of water, looking up at the waves, into Snell's window and
    # straight up; the same from 2.5 m over the water (the top side unchanged); then the same by night
    ("up_settle", ("up", 9.0, 100.0, -3.0, 10.0, 0.0), ["lyud.Current.Speed -99"]),   # the sea's depth map reads in
    ("up_window", ("up", 9.0, 100.0, -3.0, 50.0, 0.0), []),
    ("up_edge", ("up", 9.0, 100.0, -3.0, 25.0, 0.0), []),
    ("up_zenith", ("up", 9.0, 100.0, -3.0, 88.0, 0.0), []),
    ("up_shallow", ("up", 9.0, 100.0, -1.2, 35.0, 90.0), []),
    ("above_sea", ("up", 9.0, 100.0, 2.5, -14.0, 0.0), []),
    ("night_reef", ("fixed", 59700.0, 7200.0, -5250.0, -12.0, 30.0), ["lyud.SkyHourOffset 8"]),
    ("up_night", ("up", 9.0, 100.0, -3.0, 50.0, 0.0), []),
    ("up_night_zenith", ("up", 9.0, 100.0, -3.0, 88.0, 0.0), []),
    ("above_night", ("up", 9.0, 100.0, 2.5, -14.0, 0.0), []),
]
# frame time A/B in this one process (UWTEST_PERF=1): in a meadow by a reef with everything, without the fish, without
# any of the underwater life; then the same over the land (the door), where it must cost nothing
PERF = [
    ("perf_all", ("fixed", 56840.0, 9961.0, -5298.0, -8.0, -20.0), ["lyud.SkyHourOffset -3.5", "lyud.SeaLife 1", "lyud.Seabed 1", "lyud.SeaLife.Fish 1"]),
    ("perf_nofish", ("fixed", 56840.0, 9961.0, -5298.0, -8.0, -20.0), ["lyud.SeaLife.Fish 0"]),
    ("perf_none", ("fixed", 56840.0, 9961.0, -5298.0, -8.0, -20.0), ["lyud.SeaLife 0", "lyud.Seabed 0"]),
    ("perf_all2", ("fixed", 56840.0, 9961.0, -5298.0, -8.0, -20.0), ["lyud.SeaLife 1", "lyud.Seabed 1", "lyud.SeaLife.Fish 1"]),
    ("perf_up", ("up", 9.0, 100.0, -3.0, 50.0, 0.0), ["lyud.SeaLife 1", "lyud.Seabed 1"]),
    ("perf_sea", ("up", 9.0, 100.0, 2.5, -14.0, 0.0), []),
    ("perf_land_on", ("land",), ["lyud.SeaLife 1", "lyud.Seabed 1"]),
    ("perf_land_off", ("land",), ["lyud.SeaLife 0", "lyud.Seabed 0"]),
]
if os.environ.get("UWTEST_PERF"):
    STEPS = PERF if os.environ.get("UWTEST_PERF") == "only" else STEPS + PERF
ONLY = [s for s in os.environ.get("UWTEST_ONLY", "").split(",") if s]
if ONLY:
    STEPS = [s for s in STEPS if s[0] in ONLY]

st = {"t": 0.0, "i": -1, "phase": 0, "cam": None}


class Cam:
    fov = 80.0

    """The free camera of lyud.Underwater.View (the hero stays dry on the beach)."""

    def __init__(self, w):
        self.w = w
        self.loc = unreal.Vector(0, 0, 0)
        self.rot = unreal.Rotator()

    def set(self, loc, rot=None):
        self.loc = loc
        if rot is not None:
            self.rot = rot
        unreal.SystemLibrary.execute_console_command(self.w, "lyud.Underwater.View %f %f %f %f %f %f" % (
            loc.x, loc.y, loc.z, self.rot.pitch, self.rot.yaw, self.fov))

    def get_actor_location(self):
        return self.loc

    def set_actor_location(self, loc, *a):
        self.set(loc)

    def set_actor_rotation(self, rot, *a):
        self.rot = rot
        self.set(self.loc)


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def log(s):
    unreal.log_warning("UWTEST " + s)


def spot_at_depth(sea, depth_m, y_m):
    """The first point out from the beach along map y where the still water at the datum is depth_m deep."""
    x = 140.0
    while x < 1250.0:
        d = sea.depth_at_level(at(x, y_m, 0.0), DATUM) * 0.01
        if d >= depth_m:
            return x
        x += 1.0
    return x


def place(w, step):
    name, where, extra = step
    sea = unreal.LyudSea.get_sea(w)
    uw = unreal.LyudUnderwaterSubsystem.get_underwater(w)
    cam = st["cam"]
    eye = cam.get_actor_location()
    look = None
    kind = where[0]
    cam.fov = where[3] if kind == "fish" else 80.0
    if kind == "depth":
        x = spot_at_depth(sea, where[1], where[2])
        ok_bed = sea.depth_at(at(x, where[2], 0.0))
        level = sea.sea_level()
        bed_z = level - ok_bed
        if len(where) > 3:
            eye = at(x - 4.0, where[2], level + 120.0)
            look = at(x + 3.0, where[2], bed_z)
        else:
            eye = at(x, where[2], bed_z + 140.0)
            look = at(x + 10.0, where[2], bed_z + 40.0)
    elif kind == "up":
        # ("up", spot depth m, map y, eye m over the still level, pitch, yaw)
        x = spot_at_depth(sea, where[1], where[2])
        eye = at(x, where[2], sea.sea_level() + where[3] * 100.0)
        cam.set_actor_location(eye)
        cam.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=where[4], yaw=where[5]))
        st["look"] = None
        return
    elif kind == "fixed":
        eye = unreal.Vector(where[1], where[2], where[3])
        cam.set_actor_location(eye)
        cam.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=where[4], yaw=where[5]))
        st["look"] = None
        return
    elif kind == "land":
        eye = unreal.Vector(-193.0, 1150.0, 190.0)
        look = unreal.Vector(-193.0, 3150.0, 150.0)
    elif kind == "hatch":
        level = sea.sea_level()
        bed_z = level - sea.depth_at(at(HATCH[0], HATCH[1], 0.0))
        eye = at(HATCH[0] - 5.0, HATCH[1] - 2.0, bed_z + 220.0)
        look = at(HATCH[0], HATCH[1], bed_z)
    elif kind == "seabed":
        p = sea.seabed.find_nearest(where[1], eye)
        if p.x == eye.x and p.y == eye.y:
            log("%s: none of kind %d near" % (name, where[1]))
        eye = unreal.Vector(p.x - where[2] * 100.0, p.y - where[2] * 40.0, p.z + where[3] * 100.0)
        look = unreal.Vector(p.x, p.y, p.z + 30.0)
    elif kind == "fish":
        p = uw.find_nearest_fish(where[1], eye)
        if p.x == eye.x and p.y == eye.y and p.z == eye.z:
            log("%s: no %s round the eye (%d)" % (name, where[1], uw.get_fish_count(where[1])))
            look = None
        else:
            eye = unreal.Vector(p.x - where[2] * 70.0, p.y + where[2] * 70.0, p.z + where[2] * 20.0)
            look = p
    cam.set_actor_location(eye, False, True)
    if look is not None:
        d = look - eye
        cam.set_actor_rotation(unreal.MathLibrary.make_rot_from_x(d), True)
    st["look"] = look


def report(w, name):
    sea = unreal.LyudSea.get_sea(w)
    uw = unreal.LyudUnderwaterSubsystem.get_underwater(w)
    eye = st["cam"].get_actor_location()
    smp = sea.sample_surface(eye)
    log("%s eye %s, %.1f m under the surface, still depth %.1f m, camera under %s" % (
        name, eye, smp.immersion * 0.01, smp.depth * 0.01, sea.is_camera_underwater()))
    log("  " + sea.seabed.get_debug_report())
    log("  " + uw.get_debug_report())
    c = uw.current_at(eye)
    log("  current at the eye %.3f %.3f %.3f m/s" % (c.x * 0.01, c.y * 0.01, c.z * 0.01))


def tick(dt):
    st["t"] += dt
    if st.get("phase") == 2 and STEPS[st["i"]][0].startswith("perf") and st["t"] - st["t0"] > 2.0:
        st.setdefault("dts", []).append(dt)
    w = world()
    if not w or st["t"] < 8.0:
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    if st["cam"] is None:
        # our own camera: the hero stays dry on the beach (the sea would hold a swimmer's eyes over the waves)
        pawn = pc.get_controlled_pawn()
        cm = getattr(pawn, "character_movement", None)
        if cm:
            cm.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
            cm.gravity_scale = 0.0
        pawn.set_actor_enable_collision(False)
        pawn.set_actor_location(at(100.0, 0.0, -4000.0), False, True)
        st["cam"] = Cam(w)
        st["cam"].set(at(200, 100, -4400), unreal.Rotator())
        cmd(w, "lyud.SkyHourOffset -3.5")
        cmd(w, "lyud.TideOverride -0.8")
        cmd(w, "lyud.Wind.Speed 5")
        log("camera ready")
        return
    if st["phase"] == 0:
        st["i"] += 1
        if st["i"] >= len(STEPS):
            cmd(w, "quit")
            st["phase"] = 9
            return
        for c in STEPS[st["i"]][2]:
            cmd(w, c)
        # go there once to let the seabed and the fish settle round it, then place again on what is there
        place(w, STEPS[st["i"]])
        st["t0"] = st["t"]
        st["phase"] = 1
    elif st["phase"] == 1 and st["t"] - st["t0"] > 2.0:
        place(w, STEPS[st["i"]])
        st["t0"] = st["t"]
        st["phase"] = 2
    elif st["phase"] == 2:
        # a fish moves: keep looking at it
        step = STEPS[st["i"]]
        if step[1][0] == "fish" and st["t"] - st["t0"] < WAIT - 0.3:
            uw = unreal.LyudUnderwaterSubsystem.get_underwater(w)
            eye = st["cam"].get_actor_location()
            p = uw.find_nearest_fish(step[1][1], eye)
            if (p - eye).length() > 1.0:
                st["cam"].set_actor_rotation(unreal.MathLibrary.make_rot_from_x(p - eye), True)
        if st["t"] - st["t0"] > (WAIT if not step[0].startswith("perf") else 10.0):
            if step[0].startswith("perf"):
                d = sorted(st.pop("dts", [0.0]))
                log("%s frame mean %.2f ms, median %.2f, p95 %.2f over %d frames" % (
                    step[0], 1000.0 * sum(d) / len(d), 1000.0 * d[len(d) // 2], 1000.0 * d[int(len(d) * 0.95)], len(d)))
            report(w, STEPS[st["i"]][0])
            cmd(w, "HighResShot 1600x900")
            st["phase"] = 3
            st["t0"] = st["t"]
    elif st["phase"] == 3 and st["t"] - st["t0"] > 1.5:
        st["phase"] = 0


unreal.register_slate_post_tick_callback(tick)
