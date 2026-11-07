"""Offscreen shots of a town building on its plot in Lvl_Scene01 (no PIE, no window): from the street in front of the
plot at eye height, by day and by night, plus lyud.City.Report after each time change. Lines "CITYSHOT ..." in the log;
shots land in Saved/Screenshots/MacEditor in order (HighResShot).

  tools/play_offscreen.sh /Game/Scene01/Lvl_Scene01 unreal/scripts/shoot_city_buildings.py
  env: CITY_PLOT (default P-CAPSULE), CITY_DIST (m, default 40), CITY_TIMES (default 13,22), CITY_RES (1600x900),
       CITY_WAIT (s per view, default 8)
"""
import json
import math
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
PLAN = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "City", "city_plan.json")
PLOT = os.environ.get("CITY_PLOT", "P-CAPSULE")
DIST = float(os.environ.get("CITY_DIST", "40"))
TIMES = [float(t) for t in os.environ.get("CITY_TIMES", "13,22").split(",")]
RES = os.environ.get("CITY_RES", "1600x900")
WAIT = float(os.environ.get("CITY_WAIT", "8"))
EYE = 170.0

with open(PLAN) as f:
    _plan = json.load(f)
FACE_Y = _plan.get("portal_face_y", 790.0)
P = [p for p in _plan["plots"] if p["id"] == PLOT][0]


def log(m):
    unreal.log_warning("CITYSHOT " + m)


def plot_frame():
    fx, fy, fz = P["front"]
    a = math.radians(P["yaw"])
    fdir = (math.cos(a), math.sin(a))
    centre = (fx - fdir[0] * P["d"] / 2.0, fy - fdir[1] * P["d"] / 2.0)
    return (fx, fy, fz), fdir, centre


def to_ue(x, y, z):
    return (x * 100.0, FACE_Y + y * 100.0, z * 100.0)


# views: (name, side offset in m to the right of someone facing the front, distance, target height in m)
VIEWS = [("front", 0.0, DIST, 14.0), ("three_quarter", -0.6 * DIST, 0.8 * DIST, 14.0)]
SHOTS = [(t, v) for t in TIMES for v in VIEWS]
st = {"t": 0.0, "i": -1, "phase": 0, "frames": [], "time": None}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name().startswith("Lvl_") and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def ground(w, x, y, z_guess):
    a = unreal.Vector(x, y, z_guess + 6000.0)
    b = unreal.Vector(x, y, z_guess - 6000.0)
    hit = unreal.SystemLibrary.line_trace_single(w, a, b, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                  unreal.DrawDebugTrace.NONE, True)
    if hit:
        t = hit.to_tuple()
        return t[4].z, t[9]
    return z_guess, None


def _tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < 8.0:
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    if st["phase"] == 0:
        st["i"] += 1
        if st["i"] >= len(SHOTS):
            cmd(w, "lyud.City.Report")
            cmd(w, "quit")
            st["phase"] = 9
            return
        hour, (name, side, dist, th) = SHOTS[st["i"]]
        cmd(w, "t.MaxFPS 0")
        wait = WAIT
        if hour != st["time"]:
            cmd(w, "lyud.Birds.Time %g" % hour)
            st["time"] = hour
            wait = WAIT + 6.0
        (fx, fy, fz), fdir, centre = plot_frame()
        right = (fdir[1], -fdir[0])
        cx = fx + fdir[0] * dist + right[0] * side
        cy = fy + fdir[1] * dist + right[1] * side
        cam = to_ue(cx, cy, fz)
        gz, actor = ground(w, cam[0], cam[1], cam[2])
        tgt = to_ue(centre[0], centre[1], fz + th)
        if hasattr(pawn, "character_movement"):
            pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
        eye = 0.0
        cam_comp = pawn.get_component_by_class(unreal.CameraComponent)
        if cam_comp:
            eye = cam_comp.get_world_location().z - pawn.get_actor_location().z
        pawn.set_actor_enable_collision(False)
        pawn.set_actor_location(unreal.Vector(cam[0], cam[1], gz + EYE - eye), False, True)
        dx, dy, dz = tgt[0] - cam[0], tgt[1] - cam[1], tgt[2] - (gz + EYE)
        pc.set_control_rotation(unreal.Rotator(roll=0, pitch=math.degrees(math.atan2(dz, math.hypot(dx, dy))),
                                               yaw=math.degrees(math.atan2(dy, dx))))
        log("%s h%g %s: camera (%.0f, %.0f, %.0f) ground %.0f on %s, target (%.0f, %.0f, %.0f)" % (
            PLOT, hour, name, cam[0], cam[1], gz + EYE, gz, actor.get_name() if actor else None, *tgt))
        st["t0"] = st["t"]
        st["wait"] = wait
        st["phase"] = 1
    elif st["phase"] == 1 and st["t"] - st["t0"] <= st["wait"]:
        if st["t"] - st["t0"] > 2.0:
            st["frames"].append(dt)
    elif st["phase"] == 1:
        f = sorted(st["frames"]) or [0.0]
        hour, (name, *_r) = SHOTS[st["i"]]
        log("shot %d %s h%g %s: %d frames, median %.1f ms" % (st["i"], PLOT, hour, name, len(f), 1000 * f[len(f) // 2]))
        st["frames"] = []
        cmd(w, "lyud.City.Report")
        cmd(w, "HighResShot " + RES)
        st["phase"] = 2
        st["t0"] = st["t"]
    elif st["phase"] == 2 and st["t"] - st["t0"] > 2.0:
        st["phase"] = 0


def tick(dt):
    # a Python error must not leave the offscreen run hanging forever: log it and quit the editor
    try:
        _tick(dt)
    except Exception:
        import traceback
        log("ERROR " + traceback.format_exc())
        try:
            unreal.SystemLibrary.execute_console_command(world(), "quit")
        except Exception:
            pass
        unreal.SystemLibrary.quit_editor()
        unreal.unregister_slate_post_tick_callback(_handle)


_handle = unreal.register_slate_post_tick_callback(tick)
