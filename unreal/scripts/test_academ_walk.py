"""Walks the player through the Academgorodok offscreen (no PIE, no window): every route of
blender/export/academgorodok/layout.json "walks" (up each stair from the lobby to the top landing;
from each landing through a flat's door into each of its rooms and back), steering the real
character with movement input, its real capsule against the real collision. A route fails when
the character stops making progress towards its next point.

  unreal/scripts/test_academ_walk.sh       (ACADEM_WALKS=stair to run only routes containing it)
Log lines: "ACADEMWALK ok|FAIL <house> <route> ..." and a final "ACADEMWALK summary".
"""
import json
import math
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools"))
import world_map as W  # noqa: E402

FACE_Y = float(os.environ.get("ACADEM_FACE_Y", "790"))
with open(os.path.join(REPO, "blender", "export", "academgorodok", "layout.json")) as f:
    WALKS = json.load(f)["walks"]
if os.environ.get("ACADEM_WALKS"):
    WALKS = [w for w in WALKS if os.environ["ACADEM_WALKS"] in w["label"]]
REACH = 20.0           # cm: close enough to a point
STUCK = 2.5            # s without getting 5 cm closer
st = {"t": 0.0, "w": -1, "p": 0, "best": 1e9, "since": 0.0, "ok": 0, "fail": [], "started": False}


def log(msg):
    unreal.log_warning("ACADEMWALK " + msg)


def ue(p):
    x, y = W.academ_to_map(p[0], p[1])
    return unreal.Vector(*W.to_unreal(x, y, W.academ_z() + p[2], FACE_Y))


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name().startswith("Lvl_") and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def start(pawn, w):
    route = WALKS[st["w"]]
    a = ue(route["points"][0])
    pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_WALKING)
    pawn.set_actor_location(unreal.Vector(a.x, a.y, a.z + 100.0), False, True)
    st["p"] = 1
    st["best"] = 1e9
    st["since"] = st["t"]


def tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < 5.0:
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    if not st["started"]:
        unreal.SystemLibrary.execute_console_command(w, "t.MaxFPS 0")
        pawn.character_movement.set_editor_property("max_walk_speed", 900.0)
        st["started"] = True
        st["w"] = 0
        if not WALKS:
            log("summary: no routes")
            unreal.SystemLibrary.execute_console_command(w, "quit")
            return
        start(pawn, w)
        return
    if st["w"] >= len(WALKS):
        return
    route = WALKS[st["w"]]
    pts = route["points"]
    target = ue(pts[st["p"]])
    here = pawn.get_actor_location()
    feet = here.z - 96.0
    d = math.hypot(target.x - here.x, target.y - here.y)
    if d < REACH and abs(feet - target.z) < 120.0:
        st["p"] += 1
        st["best"] = 1e9
        st["since"] = st["t"]
        if st["p"] >= len(pts):
            st["ok"] += 1
            log("ok %s %s (%d points)" % (route["house"], route["label"], len(pts)))
            nxt(pawn, w)
        return
    if d < st["best"] - 5.0:
        st["best"] = d
        st["since"] = st["t"]
    elif st["t"] - st["since"] > STUCK:
        st["fail"].append((route["house"], route["label"], st["p"]))
        d2 = max(d, 1.0)
        ahead = unreal.Vector(here.x + (target.x - here.x) / d2 * 40.0, here.y + (target.y - here.y) / d2 * 40.0, here.z)
        hit = unreal.SystemLibrary.capsule_trace_single(w, here, ahead, 33.0, 94.0, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                                                        False, [pawn], unreal.DrawDebugTrace.NONE, True)
        if hit:
            t = hit.to_tuple()
            log("  blocked by %s at %s normal %s (feet %+.0f cm)" % (t[9].get_name() if t[9] else "?", [round(v) for v in (t[4].x, t[4].y, t[4].z)],
                                                                     [round(v, 2) for v in (t[6].x, t[6].y, t[6].z)], t[4].z - feet))
        log("FAIL %s %s stuck before point %d of %d at %s (target %s, %.0f cm away)" % (
            route["house"], route["label"], st["p"], len(pts) - 1,
            [round(v) for v in (here.x, here.y, feet)], [round(v) for v in (target.x, target.y, target.z)], d))
        nxt(pawn, w)
        return
    # follow the segment from the previous point (not just head for the target): doorways leave the
    # capsule ~20 cm a side, so steer for a point 50 cm ahead on the line, pulled back onto it
    prev = ue(pts[st["p"] - 1])
    sx, sy = target.x - prev.x, target.y - prev.y
    sl = math.hypot(sx, sy) or 1.0
    ux, uy = sx / sl, sy / sl
    along = (here.x - prev.x) * ux + (here.y - prev.y) * uy
    ahead = min(sl, max(0.0, along) + 50.0)
    aim_x, aim_y = prev.x + ux * ahead, prev.y + uy * ahead
    ax, ay = aim_x - here.x, aim_y - here.y
    al = math.hypot(ax, ay) or 1.0
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=0, yaw=math.degrees(math.atan2(uy, ux))))
    pawn.add_movement_input(unreal.Vector(ax / al, ay / al, 0), 1.0, True)


def nxt(pawn, w):
    st["w"] += 1
    if st["w"] >= len(WALKS):
        log("summary: %d ok, %d failed of %d routes" % (st["ok"], len(st["fail"]), len(WALKS)))
        unreal.SystemLibrary.execute_console_command(w, "quit")
        return
    start(pawn, w)


unreal.register_slate_post_tick_callback(tick)
