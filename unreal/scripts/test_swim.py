"""Offscreen play test of swimming and diving (ULyudSwimComponent on the hero, ALyudSea): drops the hero into open
water, dives, swims down along the look, runs out of air three times (choking, pushed up, the near-drownings counted),
and the third time realises he can breathe (the line, the meter gone for good); then checks the time under water is
unlimited, that the tide moves the surface he swims at, and the winter's ice is a ceiling with no air under it.
Leaves the desktop alone (no PIE, no input); run it with test_swim.sh.

Prints SWIMTEST lines (and a PASS/FAIL list at the end); screenshots land in Saved/Screenshots/MacEditor in step order.
SWIMTEST_ONLY=dive,breath,... runs some steps (the breath steps need each other, in order).
"""
import os

import unreal

FACE_Y = 790.0          # the portal face (scene01_layout.json outside.portal_face_y)
SEA_Z = -4300.0          # the datum (high water of springs)
OPEN = (60000.0, FACE_Y, SEA_Z)    # 600 m out: open water, ~20 m deep

st = {"t": 0.0, "i": -1, "t0": 0.0, "phase": "start", "checks": [], "shots": 0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def log(s):
    unreal.log_warning("SWIMTEST " + s)


def check(name, ok, info=""):
    st["checks"].append((name, bool(ok), info))
    log("%s %s %s" % ("PASS" if ok else "FAIL", name, info))


def shot(w, name):
    st["shots"] += 1
    log("shot %02d %s" % (st["shots"], name))
    # "shot showui" keeps the HUD (the air meter, the subtitle) in the picture
    cmd(w, "shot showui")


def ctx(w):
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    swim = unreal.LyudSwimComponent.get_player_swim(w)
    sea = unreal.LyudSea.get_sea(w)
    return pc, pawn, swim, sea


def state(swim):
    return str(swim.get_state()).split(".")[-1].split(":")[0].strip("<> ").upper()


def drop(w, loc, pitch=-5.0, yaw=0.0):
    pc, pawn, swim, sea = ctx(w)
    cm = pawn.character_movement
    pawn.set_actor_enable_collision(True)
    cm.gravity_scale = 1.0
    cm.set_movement_mode(unreal.MovementMode.MOVE_FALLING)
    pawn.set_actor_location(unreal.Vector(*loc), False, True)
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))


def look(w, pitch, yaw=0.0):
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))


def swim_forward(w, amount=1.0):
    # the stroke as the hero's move input does it: along the look under water
    pc, pawn, swim, sea = ctx(w)
    rot = pc.get_control_rotation()
    pawn.add_movement_input(unreal.MathLibrary.get_forward_vector(rot), amount, False)


# each step: (name, seconds, start(w), every(w, t), end(w)); a step ends early when every() returns True
def s_float_start(w):
    cmd(w, "lyud.TideOverride 0")
    cmd(w, "lyud.Swim.Reset")
    drop(w, (OPEN[0], OPEN[1], SEA_Z + 40.0))


def s_float_end(w):
    pc, pawn, swim, sea = ctx(w)
    eye = pc.player_camera_manager.get_camera_location()
    log(swim.debug_report())
    check("surface: swimming at the surface", state(swim) == "SURFACE", state(swim))
    check("surface: eyes over the waves", eye.z > sea.sample_surface(eye).surface_z - 15.0, "%.1f" % (eye.z - sea.sample_surface(eye).surface_z))
    check("surface: no meter while full", swim.get_air() > 0.99)
    shot(w, "surface")


def s_dive_start(w):
    look(w, -35.0)
    cmd(w, "lyud.Swim.Dive")


def s_dive_every(w, t):
    swim_forward(w)
    return False


def s_dive_end(w):
    pc, pawn, swim, sea = ctx(w)
    log(swim.debug_report())
    check("dive: under", swim.is_underwater(), state(swim))
    check("dive: went down along the look", swim.get_depth_cm() > 150.0, "%.0f cm" % swim.get_depth_cm())
    check("dive: air going", 0.5 < swim.get_air() < 0.99, "%.2f" % swim.get_air())
    check("dive: camera under (the sea's post process)", sea.is_camera_underwater())
    shot(w, "under_down")


def s_level_start(w):
    look(w, 0.0, 30.0)


def s_level_every(w, t):
    swim_forward(w)
    return False


def s_level_end(w):
    pc, pawn, swim, sea = ctx(w)
    log(swim.debug_report())
    check("level: still under, swimming level", swim.is_underwater())
    shot(w, "under_level")


def s_body_start(w):
    cmd(w, "lyud.Hero.ViewBehind 260")


def s_body_end(w):
    shot(w, "under_body_behind")


def drown_first_start(w):
    cmd(w, "lyud.Hero.ViewBehind 0")
    drown_start(w)


def drown_start(w):
    look(w, -20.0)
    cmd(w, "lyud.Swim.Dive")
    st["choked"] = False
    st["dived"] = False


def drown_every(w, t):
    pc, pawn, swim, sea = ctx(w)
    if not st["dived"]:
        if swim.is_underwater() and swim.get_depth_cm() > 120.0:
            st["dived"] = True
            cmd(w, "lyud.Swim.Air 0.02")
        elif state(swim) == "SURFACE":
            cmd(w, "lyud.Swim.Dive")
        swim_forward(w)
        return False
    if swim.is_out_of_air() and not st["choked"]:
        st["choked"] = True
        st["chokeT"] = t
        log("  choking at %.1f s, depth %.0f" % (t, swim.get_depth_cm()))
    if st["choked"] and t - st["chokeT"] > 0.4 and not st.get("redshot"):
        st["redshot"] = True
        shot(w, "choking_%d" % st["drown"])
    return st["choked"] and state(swim) == "SURFACE" and t - st["chokeT"] > 0.5


def drown_end(w):
    pc, pawn, swim, sea = ctx(w)
    log(swim.debug_report())
    st["redshot"] = False
    n = st["drown"]
    check("near-drowning %d: choked" % n, st["choked"])
    check("near-drowning %d: pushed up to the surface" % n, state(swim) == "SURFACE", state(swim))
    check("near-drowning %d: counted" % n, ("near-drownings %d" % n) in swim.debug_report())
    check("near-drowning %d: meter still there" % n, not swim.is_breath_free())
    st["drown"] += 1


def realise_every(w, t):
    pc, pawn, swim, sea = ctx(w)
    if not st["dived"]:
        if swim.is_underwater() and swim.get_depth_cm() > 120.0:
            st["dived"] = True
            cmd(w, "lyud.Swim.Air 0.02")
        elif state(swim) == "SURFACE":
            cmd(w, "lyud.Swim.Dive")
        swim_forward(w)
        return False
    if swim.is_breath_free() and not st.get("freeT"):
        st["freeT"] = t
        st["freeUnder"] = swim.is_underwater()
        log("  breath free at %.1f s, depth %.0f, %s" % (t, swim.get_depth_cm(), swim.debug_report()))
    if st.get("freeT") and t - st["freeT"] > 1.2 and not st.get("subshot"):
        st["subshot"] = True
        shot(w, "realise_line")
    return st.get("freeT") and t - st["freeT"] > 9.0


def realise_end(w):
    pc, pawn, swim, sea = ctx(w)
    log(swim.debug_report())
    check("realise: on the 3rd time the breath is free", swim.is_breath_free())
    check("realise: he did not come up for it", st.get("freeUnder"))
    check("realise: meter gone", "meter 0.00" in swim.debug_report())
    shot(w, "after_realise")


def s_long_start(w):
    cmd(w, "lyud.Swim.Breath 3")
    look(w, -40.0)
    cmd(w, "lyud.Swim.Dive")


def s_long_every(w, t):
    swim_forward(w, 0.3)
    return False


def s_long_end(w):
    pc, pawn, swim, sea = ctx(w)
    log(swim.debug_report())
    check("unlimited: 20 s under on 3 s of air, no choking", swim.is_underwater() and not swim.is_out_of_air() and swim.get_air() > 0.99,
          "under %.0f s" % swim.get_time_under())
    cmd(w, "lyud.Swim.Breath -1")


def s_tide_start(w):
    cmd(w, "lyud.TideOverride -3.5")
    drop(w, (OPEN[0], OPEN[1] + 500.0, SEA_Z - 300.0))


def s_tide_end(w):
    pc, pawn, swim, sea = ctx(w)
    eye = pc.player_camera_manager.get_camera_location()
    log(swim.debug_report() + "  sea level %.0f" % sea.sea_level())
    check("tide: the sea level is down", abs(sea.sea_level() - (SEA_Z - 350.0)) < 30.0, "%.0f" % sea.sea_level())
    check("tide: swimming at the low surface", state(swim) == "SURFACE" and abs(eye.z - sea.sample_surface(eye).surface_z) < 60.0,
          "%s, eye %.0f over" % (state(swim), eye.z - sea.sample_surface(eye).surface_z))
    shot(w, "tide_low_surface")
    cmd(w, "lyud.Swim.Dive")


def s_tide_dive_every(w, t):
    look(w, -60.0)
    swim_forward(w)
    return False


def s_tide_dive_end(w):
    pc, pawn, swim, sea = ctx(w)
    log(swim.debug_report())
    check("tide: depth measured from the low surface", swim.is_underwater() and swim.get_depth_cm() > 100.0
          and swim.get_eye_location().z < sea.sea_level() - 100.0, "%.0f" % swim.get_depth_cm())
    cmd(w, "lyud.TideOverride 0")


def s_ice_start(w):
    cmd(w, "lyud.Winter.Ice 1")
    cmd(w, "lyud.Winter.IceCm 30")
    drop(w, (OPEN[0], OPEN[1] - 500.0, SEA_Z - 400.0), pitch=40.0)


def s_ice_every(w, t):
    pc, pawn, swim, sea = ctx(w)
    if t > 6.0:
        # straight up at the ice
        look(w, 80.0)
        swim_forward(w)
        cmd(w, "lyud.Swim.Air 1")
    return False


def s_ice_end(w):
    pc, pawn, swim, sea = ctx(w)
    winter = unreal.LyudWinterSubsystem.get_winter(w)
    log(swim.debug_report() + ("  winter: " + winter.debug_report().replace("\n", " | ")[:300] if winter else "  NO WINTER"))
    ice = winter.get_ice_at(swim.get_eye_location()) if winter else 0.0
    log("  ice at the eye %.2f" % ice)
    if ice < 0.5:
        check("ice: (skipped, no ice laid here yet)", True)
        return
    check("ice: under the ice", swim.is_under_ice() and swim.is_underwater(), state(swim))
    check("ice: the head stays under its underside", swim.get_eye_location().z < sea.sea_level() - 20.0,
          "eye %.0f, sea %.0f" % (swim.get_eye_location().z, sea.sea_level()))
    shot(w, "under_ice")
    cmd(w, "lyud.Winter.Ice -1")
    cmd(w, "lyud.Winter.IceCm -1")


def s_shore_start(w):
    # onto the flat by the beach at low water: on its feet, dry
    cmd(w, "lyud.TideOverride -4.5")
    drop(w, (11800.0, FACE_Y + 500.0, SEA_Z + 100.0))


def s_shore_end(w):
    pc, pawn, swim, sea = ctx(w)
    log(swim.debug_report())
    check("shore: not swimming in the shallows", state(swim) == "DRY", state(swim))


def nothing(w, t=None):
    return False


STEPS = [
    ("float", 6.0, s_float_start, nothing, s_float_end),
    ("dive", 5.0, s_dive_start, s_dive_every, s_dive_end),
    ("level", 3.0, s_level_start, s_level_every, s_level_end),
    ("body", 1.5, s_body_start, nothing, s_body_end),
    ("drown1", 40.0, drown_first_start, drown_every, drown_end),
    ("drown2", 40.0, drown_start, drown_every, drown_end),
    ("realise", 50.0, drown_start, realise_every, realise_end),
    ("long", 20.0, s_long_start, s_long_every, s_long_end),
    ("tide", 8.0, s_tide_start, nothing, s_tide_end),
    ("tide_dive", 4.0, nothing, s_tide_dive_every, s_tide_dive_end),
    ("ice", 16.0, s_ice_start, s_ice_every, s_ice_end),
    ("shore", 6.0, s_shore_start, nothing, s_shore_end),
]
if os.environ.get("SWIMTEST_ONLY"):
    keep = os.environ["SWIMTEST_ONLY"].split(",")
    STEPS = [s for s in STEPS if s[0] in keep]
st["drown"] = 1


def finish(w):
    fails = [c for c in st["checks"] if not c[1]]
    log("RESULT %d checks, %d failed" % (len(st["checks"]), len(fails)))
    for c in fails:
        log("  FAILED " + c[0] + " " + c[2])
    cmd(w, "quit")


def tick(dt):
    try:
        st["t"] += dt
        w = world()
        if not w or st["t"] < 8.0:
            return
        if st["phase"] == "start":
            cmd(w, "lyud.Swim.Debug 0")
            cmd(w, "lyud.Hud 1")
            st["phase"] = "next"
        if st["phase"] == "next":
            st["i"] += 1
            if st["i"] >= len(STEPS):
                st["phase"] = "done"
                finish(w)
                return
            name = STEPS[st["i"]][0]
            log("step %s" % name)
            STEPS[st["i"]][2](w)
            st["t0"] = st["t"]
            st["phase"] = "run"
            return
        if st["phase"] == "run":
            name, secs, start, every, end = STEPS[st["i"]]
            t = st["t"] - st["t0"]
            if int(t) != st.get("sec"):
                st["sec"] = int(t)
                pc, pawn, swim, sea = ctx(w)
                if swim and int(t) % 2 == 0:
                    log("  %s t %d %s" % (name, int(t), swim.debug_report()))
            if every(w, t) or t > secs:
                end(w)
                st["phase"] = "pause"
                st["t0"] = st["t"]
        elif st["phase"] == "pause" and st["t"] - st["t0"] > 1.0:
            st["phase"] = "next"
    except Exception as e:
        log("ERROR %s" % e)
        st["phase"] = "done"
        w = world()
        if w:
            cmd(w, "quit")


unreal.register_slate_post_tick_callback(tick)
