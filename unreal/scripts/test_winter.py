"""Offscreen winter check in Lvl_Scene01 (no PIE, no window): forces a deep, drifted snow and a frozen sea through the
lyud.Winter.* cvars (test_winter.sh sets them), waits for the snow to be read in, logs WINTERTEST lines (the
state, depths at known places, the ice's strip, residents clearing the paths) and takes HighResShots from SPOTS
(Saved/Screenshots/MacEditor, in the order of SPOTS).

  unreal/scripts/test_winter.sh                   (WINTER_SHOTS=door,beach for some only)
"""
import math
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
import terrain as T  # noqa: E402
import world_map as W  # noqa: E402

FACE_Y = float(os.environ.get("WINTER_FACE_Y", "790"))
RES = os.environ.get("WINTER_RES", "1600x900")
WAIT = float(os.environ.get("WINTER_WAIT", "5"))


def g(x, y, up):
    return (x, y, T.ground_z(x, y) + up)


def ac(x, y, up):
    mx, my = W.academ_to_map(x, y)
    return (mx, my, W.academ_z() + up)


SY = 400.0
SX = W.shore_x(SY)
TREE = W.alley_trees()[0]
# (name, camera (map x, y, z), target (map x, y, z)); map metres: x west (the sea), y north
SPOTS = [
    ("aerial", ac(150.0, -170.0, 90.0), ac(-5.0, -10.0, 0.0)),
    ("academ_square", ac(40.0, -12.0, 1.7), ac(0.0, 0.0, 4.0)),
    ("academ_yard", ac(-40.0, 10.0, 1.7), ac(0.0, 0.0, 3.0)),
    ("door", g(W.DOOR_X - 3.0, 22.0, 1.7), g(W.DOOR_X, 5.0, 1.5)),
    ("alley_tree", g(TREE[0] - 6.0, TREE[1] - 8.0, 1.7), g(TREE[0], TREE[1], 4.0)),
    ("drift_close", g(W.DOOR_X + 9.0, 30.0, 0.6), g(W.DOOR_X + 2.0, 40.0, 0.2)),
    ("beach_ice", (SX - 6.0, SY, T.ground_z(SX - 6.0, SY) + 1.7), (SX + 60.0, SY + 20.0, W.SEA_Z)),
    ("ice_strip_high", (SX - 20.0, SY - 60.0, W.SEA_Z + 30.0), (SX + 30.0, SY, W.SEA_Z)),
    ("bank_lead", (260.0, -40.0, W.SEA_Z + 14.0), (420.0, -60.0, W.SEA_Z)),
    ("sea_far", (SX - 30.0, 900.0, W.SEA_Z + 60.0), (SX + 900.0, 600.0, W.SEA_Z)),
    ("shoveller", None, None),
    ("shoveller_close", None, None),
    ("perf", ac(40.0, -12.0, 1.7), ac(0.0, 0.0, 4.0)),
]
if os.environ.get("WINTER_SHOTS"):
    SPOTS = [s for s in SPOTS if s[0] in os.environ["WINTER_SHOTS"].split(",")]
st = {"t": 0.0, "i": -1, "phase": -1, "done": False}


def log(msg):
    unreal.log_warning("WINTERTEST " + msg)


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name().startswith("Lvl_") and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def ue(p):
    return W.to_unreal(p[0], p[1], p[2], FACE_Y)


def checks(w, win):
    log(win.debug_report())
    for name, p in (("academ_square", ac(30.0, -6.0, 0.0)), ("academ_lee", ac(-12.0, 0.0, 0.0)), ("door", g(W.DOOR_X, 15.0, 0.0)),
                    ("path_alley", (0.0, 40.0, 0.0)), ("beach", (SX - 6.0, SY, 0.0))):
        u = ue(p)
        log("depth %-14s %.1f cm" % (name, win.get_snow_depth_at(unreal.Vector(*u))))
    for d in (2.0, 8.0, 14.0, 20.0, 30.0, 60.0):
        u = ue((SX + d, SY, W.SEA_Z))
        v = unreal.Vector(*u)
        log("ice %4.0f m off the high-water line: %.2f walkable %s" % (d, win.get_ice_at(v), win.is_ice_walkable(v)))


def shoveller(w):
    life = unreal.PeopleLifeSubsystem.get_people_life(w)
    if not life:
        return None
    for i in range(life.num_people()):
        info = life.get_person(i)
        if isinstance(info, tuple):
            info = info[-1]
        if info and "SHOVEL" in str(info.pose).upper():
            return info
    return None


def aim(w, pawn, pc, a, b):
    eye = 0.0
    cam_comp = pawn.get_component_by_class(unreal.CameraComponent)
    if cam_comp:
        eye = cam_comp.get_world_location().z - pawn.get_actor_location().z
    pawn.set_actor_enable_collision(False)
    pawn.set_actor_location(unreal.Vector(a[0], a[1], a[2] - eye), False, True)
    dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=math.degrees(math.atan2(dz, math.hypot(dx, dy))),
                                           yaw=math.degrees(math.atan2(dy, dx))))


def tick(dt):
    try:
        _tick(dt)
    except Exception as e:  # an exception here repeats every tick and the game never quits
        log("ERROR %s" % e)
        w = world()
        if w:
            cmd(w, "quit")
        unreal.unregister_slate_post_tick_callback(handle)


def _tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < 4.0 or st["done"]:
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    win = unreal.LyudWinterSubsystem.get_winter(w)
    if st["phase"] == -1:
        # wait for the snow to be read in (the traces run a few hundred rows a frame)
        if pawn and hasattr(pawn, "character_movement"):
            pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
        if win and win.is_ready():
            st["phase"] = -2
            st["t0"] = st["t"]
            log("ready after %.1f s" % st["t"])
            # residents out with their pushers, near the camera's first spots
            aim(w, pawn, pc, ue(ac(40.0, -12.0, 1.7)), ue(ac(0.0, 0.0, 4.0)))
            cmd(w, "lyud.Winter.SnowOnPaths 12")
        elif st["t"] > 240.0:
            log("FAIL not ready after 240 s: " + (win.debug_report() if win else "no winter subsystem"))
            cmd(w, "quit")
            st["done"] = True
        return
    if st["phase"] == -2:
        if st["t"] - st["t0"] < 25.0:
            if st["t"] - st["t0"] > 12.0 and not st.get("ordered"):
                # residents who are about (not at work or in town) out with their pushers
                st["ordered"] = True
                life = unreal.PeopleLifeSubsystem.get_people_life(w)
                sq = ue(ac(30.0, -6.0, 0.0))
                cands = []
                for i in range(life.num_people() if life else 0):
                    info = life.get_person(i)
                    info = info[-1] if isinstance(info, tuple) else info
                    if info.age >= 15 and info.state not in ("away", "riding", "waiting"):
                        cands.append((math.hypot(info.location.x - sq[0], info.location.y - sq[1]), i, info))
                for d, i, info in sorted(cands, key=lambda c: c[0])[:4]:
                    log("ordered %s (%s, %.0f m off) to shovel: %s" % (info.key, info.state, d / 100.0, life.order(i, "shovel")))
                # the game's clock 10x so they get out and along their stretches within the test
                cmd(w, "slomo 6")
            return
        checks(w, win)
        st["phase"] = 0
    if st["phase"] == 0 and st["i"] + 1 < len(SPOTS) and not SPOTS[st["i"] + 1][0].startswith("shoveller"):
        cmd(w, "slomo 1")
    if st["phase"] == 0:
        st["i"] += 1
        if st["i"] >= len(SPOTS):
            log(win.debug_report())
            log("done")
            cmd(w, "quit")
            st["done"] = True
            return
        name, cam, tgt = SPOTS[st["i"]]
        cmd(w, "t.MaxFPS 0")
        if name.startswith("shoveller"):
            info = shoveller(w)
            if not info:
                # they have to come out of their flats first: wait up to 3 minutes for one at work
                st.setdefault("wait_shovel", st["t"])
                if st["t"] - st["wait_shovel"] < 180.0:
                    st["i"] -= 1
                    return
                log("no shoveller at work (skipping that shot)")
                life = unreal.PeopleLifeSubsystem.get_people_life(w)
                for k in ("korneev_ivan", "belov_misha", "valiev_rustam"):
                    if life and life.find_person(k) >= 0:
                        log(life.describe(life.find_person(k)))
                return
            log("shoveller %s at %s doing %s: %s" % (info.key, info.location, info.activity, unreal.LyudWinterSubsystem.get_winter(w).debug_report()))
            loc = info.location
            yaw = math.radians(info.yaw)
            k = 1.0 if name == "shoveller" else 0.45
            a = (loc.x + (math.cos(yaw) * 450.0 + math.sin(yaw) * 250.0) * k, loc.y + (math.sin(yaw) * 450.0 - math.cos(yaw) * 250.0) * k, loc.z + 170.0 * (0.6 + 0.4 * k))
            b = (loc.x, loc.y, loc.z + 70.0)
            aim(w, pawn, pc, a, b)
        else:
            aim(w, pawn, pc, ue(cam), ue(tgt))
        st["t0"] = st["t"]
        st["phase"] = 1
    elif st["phase"] == 1 and SPOTS[st["i"]][0] == "perf":
        # frame time with the winter and without it (lyud.Winter 0), the same view, one process
        st.setdefault("pf", {"on": [], "off": []})
        el = st["t"] - st["t0"]
        if 3.0 < el < 13.0:
            st["pf"]["on"].append(dt)
        elif el >= 13.0 and not st.get("pf_off"):
            st["pf_off"] = True
            cmd(w, "lyud.Winter 0")
        elif 19.0 < el < 29.0:
            st["pf"]["off"].append(dt)
        elif el >= 29.0:
            med = lambda a: 1000.0 * sorted(a)[len(a) // 2] if a else 0.0
            log("perf frame median: winter on %.1f ms, off %.1f ms (%d/%d frames)" % (med(st["pf"]["on"]), med(st["pf"]["off"]), len(st["pf"]["on"]), len(st["pf"]["off"])))
            cmd(w, "lyud.Winter 1")
            st["phase"] = 0
    elif st["phase"] == 1 and st["t"] - st["t0"] > WAIT:
        log("shot %s" % SPOTS[st["i"]][0])
        cmd(w, "HighResShot " + RES)
        st["phase"] = 2
        st["t0"] = st["t"]
    elif st["phase"] == 2 and st["t"] - st["t0"] > 1.5:
        st["phase"] = 0


handle = unreal.register_slate_post_tick_callback(tick)
