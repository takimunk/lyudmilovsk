"""Offscreen check that the forest floor keeps out of the built world (audit_forestfloor.sh; no PIE, no window), in winter
(forced snow, where a log reads as a white pipe): the Academgorodok's square and yard shot with the old placement
(lyud.ForestFloor.Guard -1: the old placement on a ground without the terrain mask's forest channel, as the
winter thread's checkout had it, "before") and the guarded one ("after"), a forest view to show the woods keep their floor,
then AForestFloor::AuditMap over the Academgorodok (the old placement) and over the whole map (guard 1): FLOORAUDIT lines, the
occupancy files in Saved/ForestFloorAudit/ for tools/forestfloor_audit.py.
"""
import math
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
import terrain as T  # noqa: E402
import world_map as W  # noqa: E402

FACE_Y = float(os.environ.get("FLOOR_FACE_Y", "790"))
RES = os.environ.get("FLOOR_RES", "1600x900")
OUT = os.path.join(HERE, "..", "Lyudmilovsk", "Saved", "ForestFloorAudit")
BEFORE = int(os.environ.get("FLOOR_BEFORE", "-1"))
st = {"t": 0.0, "step": 0, "t0": 0.0, "done": False}


def log(msg):
    unreal.log_warning("FLOORAUDIT " + msg)


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name().startswith("Lvl_") and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def ac(x, y, up):
    mx, my = W.academ_to_map(x, y)
    return W.to_unreal(mx, my, W.academ_z() + up, FACE_Y)


def g(x, y, up):
    return W.to_unreal(x, y, T.ground_z(x, y) + up, FACE_Y)


def aim(pawn, pc, a, b):
    eye = 0.0
    cam = pawn.get_component_by_class(unreal.CameraComponent)
    if cam:
        eye = cam.get_world_location().z - pawn.get_actor_location().z
    pawn.set_actor_enable_collision(False)
    pawn.set_actor_location(unreal.Vector(a[0], a[1], a[2] - eye), False, True)
    dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=math.degrees(math.atan2(dz, math.hypot(dx, dy))), yaw=math.degrees(math.atan2(dy, dx))))


SX, SY = -1150.0, -380.0
VIEWS = {
    "square": (ac(40.0, -12.0, 1.7), ac(0.0, 0.0, 4.0)),
    "yard": (ac(-40.0, 10.0, 1.7), ac(0.0, 0.0, 3.0)),
    "garden": (ac(60.0, 30.0, 6.0), ac(20.0, 0.0, 0.0)),
    "forest": (g(SX, SY, 1.7), g(SX, SY + 14.0, 0.6)),
}
# (view, guard, seconds to settle, shot name)
PLAN = [("square", BEFORE, 40.0, "before_square"), ("yard", BEFORE, 15.0, "before_yard"), ("garden", BEFORE, 15.0, "before_garden"),
        ("audit_before", BEFORE, 0.0, None),
        ("square", 1, 30.0, "after_square"), ("yard", 1, 15.0, "after_yard"), ("garden", 1, 15.0, "after_garden"),
        ("forest", 1, 35.0, "after_forest"), ("audit_after", 1, 0.0, None)]


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
    if not st.get("dated"):
        if pawn and hasattr(pawn, "character_movement"):
            pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
        for cal in unreal.ObjectIterator(unreal.GameCalendar):
            if cal.get_world() == w:
                cal.set_now(unreal.DateTime(2027, 1, 20, 12, 0, 0, 0))
                st["dated"] = True
        cmd(w, "lyud.ForestFloor.Guard %d" % BEFORE)
        aim(pawn, pc, *VIEWS["square"])
        return
    floor = unreal.LyudForestFloorSubsystem.find_floor(w)
    if not st.get("ready"):
        win = unreal.LyudWinterSubsystem.get_winter(w)
        if floor and floor.num_trees() > 0 and win and win.is_ready():
            st["ready"] = True
            st["t0"] = st["t"]
            log("ready at %.0f s: %s" % (st["t"], floor.describe_state().splitlines()[0]))
            ox, oy, _ = W.academ()
            for name, (x, y) in (("academ origin", (ox, oy)), ("forest spot", (SX, SY)), ("door", (W.DOOR_X, 30.0))):
                log("site at %s: %.0f" % (name, floor.site_at(unreal.Vector(*W.to_unreal(x, y, 0.0, FACE_Y)))))
        elif st["t"] > 500.0:
            log("FAIL not ready after 500 s")
            cmd(w, "quit")
            st["done"] = True
        return
    if st["step"] >= len(PLAN):
        log("done")
        cmd(w, "quit")
        st["done"] = True
        return
    view, guard, settle, shot = PLAN[st["step"]]
    if not st.get("set"):
        cmd(w, "lyud.ForestFloor.Guard %d" % guard)
        if view in VIEWS:
            aim(pawn, pc, *VIEWS[view])
        st["set"] = True
        st["t0"] = st["t"]
        return
    if view.startswith("audit"):
        os.makedirs(OUT, exist_ok=True)
        if view == "audit_before":
            ox, oy, _ = W.academ()
            centre, radius = W.to_unreal(ox, oy, 0.0, FACE_Y), 30000.0
        else:
            centre, radius = W.to_unreal(0.0, 650.0, 0.0, FACE_Y), 240000.0
        path = os.path.abspath(os.path.join(OUT, view.split("_")[1] + ".ffa"))
        log(floor.audit_map(path, unreal.Vector(*centre), radius))
        log("wrote %s" % path)
    elif st["t"] - st["t0"] > settle:
        if not st.get("shot"):
            for line in floor.describe_state().splitlines()[:1]:
                log("%s: %s" % (shot, line))
            big = [l.strip() for l in floor.describe_state().splitlines() if l.strip().startswith(("Log_", "Stump_", "Boulder_"))]
            log("%s big: %s" % (shot, "; ".join(l.split(" inst")[0] for l in big)))
            log("shot %s" % shot)
            cmd(w, "HighResShot " + RES)
            st["shot"] = st["t"]
            return
        if st["t"] - st["shot"] < 2.0:
            return
    else:
        return
    st["step"] += 1
    st["set"] = False
    st["shot"] = None


handle = unreal.register_slate_post_tick_callback(tick)
