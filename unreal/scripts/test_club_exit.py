"""Walks the player out of the club offscreen (no PIE, no window): from the admin hall through the open
blast door (ASwingDoor "ExitDoor_Actor") out onto the terrace and back in, steering the real character
with movement input, its real capsule against the real collision; never jumps. A leg fails when the
character stops getting closer to its point. Then inspects the hall for untextured spheres and takes
shots (CLUBEXIT_SHOTS=0 skips them).

  unreal/scripts/test_club_exit.sh                 (CLUBEXIT_WINTER=1: deep forced snow outside)
Log lines: "CLUBEXIT ok|FAIL <leg> ...", "CLUBEXIT floor ..." (what the feet stand on along the way),
"CLUBEXIT sphere ..." (none expected) and a final "CLUBEXIT summary".
"""
import json
import math
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
with open(os.path.join(REPO, "blender", "export", "scene01_layout.json")) as f:
    LAYOUT = json.load(f)
DX, DY = LAYOUT["exit_door"]["center"][0], LAYOUT["exit_door"]["center"][1]
FACE = LAYOUT["outside"]["portal_face_y"]
# hall floor -> doorway -> terrace 6 m out and back (UE cm, the hall's floor is z 0)
LEGS = [("out", [(DX, DY - 300.0), (DX, DY), (DX, FACE + 600.0)]),
        ("in", [(DX, FACE + 600.0), (DX, DY), (DX, DY - 300.0)])]
REACH = 25.0
STUCK = 6.0            # s without getting 5 cm closer (long hitches while the snow first reads the world)
SHOTS = [  # (name, eye (x, y, z), look at (x, y, z)); UE cm
    ("hall_partition", (DX + 60.0, 560.0, 165.0), (60.0, 0.0, 120.0)),
    ("hall_booth", (-250.0, 300.0, 165.0), (250.0, 120.0, 100.0)),
    ("corridor_to_hall", (0.0, -250.0, 165.0), (0.0, 400.0, 120.0)),
    ("exit_door", (DX + 40.0, DY - 320.0, 160.0), (DX, FACE + 400.0, 120.0)),
]
RES = "1600x900"
st = {"t": 0.0, "leg": -1, "p": 0, "best": 1e9, "since": 0.0, "ok": 0, "fail": [], "phase": "wait", "shot": 0,
      "t0": 0.0, "minz": 1e9, "maxz": -1e9, "jumps": 0}


def log(msg):
    unreal.log_warning("CLUBEXIT " + msg)


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name().startswith("Lvl_") and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def hit_tuple(h):
    return h.to_tuple() if h else None


def floor_profile(w, pawn):
    """What the feet meet straight down every 10 cm along the door's axis, and anything that blocks a
    capsule at knee height across it."""
    prev = None
    y = DY - 200.0
    while y <= FACE + 400.0:
        a, b = unreal.Vector(DX, y, 150.0), unreal.Vector(DX, y, -300.0)
        h = unreal.SystemLibrary.line_trace_single(w, a, b, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [pawn],
                                                   unreal.DrawDebugTrace.NONE, True)
        t = hit_tuple(h)
        z = t[4].z if t and t[0] else None
        name = (t[9].get_actor_label() if t[9] else "?") + "." + (t[10].get_name() if t[10] else "?") if t and t[0] else "-"
        if prev is None or z is None or prev[0] is None or abs(z - prev[0]) > 1.5 or name != prev[1]:
            log("floor y=%.0f z=%s on %s" % (y, "%.1f" % z if z is not None else "none", name))
        prev = (z, name)
        y += 10.0
    # everything that blocks the player's capsule in the doorway
    for y in (DY - 60.0, DY - 25.0, DY, DY + 25.0, DY + 50.0, FACE - 10.0, FACE + 20.0, FACE + 60.0):
        c = unreal.Vector(DX, y, 98.0)
        hs = unreal.SystemLibrary.capsule_trace_multi(w, c, unreal.Vector(DX, y + 0.1, 98.0), 34.0, 96.0,
                                                      unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [pawn],
                                                      unreal.DrawDebugTrace.NONE, True)
        for h in hs or []:
            t = h.to_tuple()
            log("overlap capsule at y=%.0f: %s.%s at z %.1f (blocking %s)" % (
                y, t[9].get_actor_label() if t[9] else "?", t[10].get_name() if t[10] else "?", t[4].z, t[0]))


def spheres(w):
    """Primitives round the hall with a sphere mesh or an engine default material."""
    box = unreal.Box(unreal.Vector(-700.0, -600.0, -300.0), unreal.Vector(700.0, 900.0, 500.0))
    n = 0
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor):
        for c in a.get_components_by_class(unreal.PrimitiveComponent):
            try:
                if not c.is_visible():
                    continue
                o, e, _r = unreal.SystemLibrary.get_component_bounds(c)
            except Exception:
                continue
            if not (box.min.x <= o.x <= box.max.x and box.min.y <= o.y <= box.max.y and box.min.z <= o.z <= box.max.z):
                continue
            mesh = ""
            if isinstance(c, unreal.StaticMeshComponent) and c.static_mesh:
                mesh = c.static_mesh.get_path_name()
            mats = []
            try:
                for i in range(c.get_num_materials()):
                    m = c.get_material(i)
                    mats.append(m.get_path_name() if m else "None")
            except Exception:
                pass
            low = (mesh + " " + " ".join(mats)).lower()
            # an engine ball bigger than an eye or an apple, or the engine's default/checker material
            big_ball = "basicshapes/sphere" in low and max(e.x, e.y, e.z) >= 15.0
            if big_ball or "defaultmaterial" in low or "worldgrid" in low or (mesh and "None" in mats):
                n += 1
                log("sphere %s %s (%s) at %s ext %s mesh %s mats %s" % (
                    a.get_class().get_name(), a.get_actor_label(), c.get_name(), [round(v) for v in (o.x, o.y, o.z)],
                    [round(v) for v in (e.x, e.y, e.z)], mesh, mats[:4]))
    log("spheres: %d untextured or engine balls round the hall" % n)
    st["spheres"] = n


def aim(pawn, pc, a, b):
    eye = 0.0
    cam = pawn.get_component_by_class(unreal.CameraComponent)
    if cam:
        eye = cam.get_world_location().z - pawn.get_actor_location().z
    pawn.set_actor_enable_collision(False)
    pawn.set_actor_location(unreal.Vector(a[0], a[1], a[2] - eye), False, True)
    dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=math.degrees(math.atan2(dz, math.hypot(dx, dy))),
                                           yaw=math.degrees(math.atan2(dy, dx))))


def start_leg(pawn):
    pts = LEGS[st["leg"]][1]
    a = pts[0]
    pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_WALKING)
    pawn.set_actor_location(unreal.Vector(a[0], a[1], 200.0 if a[1] < FACE else 400.0), False, True)
    st["p"], st["best"], st["since"] = 1, 1e9, st["t"]


def next_leg(w, pawn):
    st["leg"] += 1
    if st["leg"] >= len(LEGS):
        log("summary walk: %d ok, %d failed of %d legs, feet z %.1f..%.1f" % (st["ok"], len(st["fail"]), len(LEGS),
                                                                             st["minz"], st["maxz"]))
        st["phase"] = "shots"
        st["t0"] = st["t"]
        return
    start_leg(pawn)


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
    if not w or st["t"] < 6.0:
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    if st["phase"] == "wait":
        cmd(w, "t.MaxFPS 0")
        doors = [d for d in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.load_class(None, "/Script/Lyudmilovsk.LyudDoorBase"))]
        for d in doors:
            d.open(None)
        log("pawn %s, capsule r %.0f hh %.0f, step %.0f; opened %d doors" % (
            pawn.get_class().get_name(), pawn.capsule_component.get_scaled_capsule_radius(),
            pawn.capsule_component.get_scaled_capsule_half_height(), pawn.character_movement.max_step_height, len(doors)))
        st["phase"] = "settle"
        st["t0"] = st["t"]
        return
    if st["phase"] == "settle":
        if st["t"] - st["t0"] > 6.0:   # the door swings open (55 deg/s), the snow settles
            floor_profile(w, pawn)
            spheres(w)
            st["phase"] = "walk"
            st["leg"] = 0
            start_leg(pawn)
        return
    if st["phase"] == "walk":
        pts = LEGS[st["leg"]][1]
        target = pts[st["p"]]
        here = pawn.get_actor_location()
        feet = here.z - pawn.capsule_component.get_scaled_capsule_half_height()
        if pawn.character_movement.is_falling() is False and abs(here.y - DY) < 150.0:
            st["minz"], st["maxz"] = min(st["minz"], feet), max(st["maxz"], feet)
        d = math.hypot(target[0] - here.x, target[1] - here.y)
        if os.environ.get("CLUBEXIT_TRACE") and st["t"] - st.get("traced", 0.0) > 0.5:
            st["traced"] = st["t"]
            v = pawn.get_velocity()
            log("trace %s p%d at %s v %s max %.0f mode %s" % (LEGS[st["leg"]][0], st["p"], [round(c) for c in (here.x, here.y, feet)],
                                                       [round(c) for c in (v.x, v.y, v.z)], pawn.character_movement.max_walk_speed,
                                                       pawn.character_movement.movement_mode))
        if d < REACH:
            st["p"] += 1
            st["best"], st["since"] = 1e9, st["t"]
            if st["p"] >= len(pts):
                st["ok"] += 1
                log("ok %s (feet at the end %.1f)" % (LEGS[st["leg"]][0], feet))
                next_leg(w, pawn)
            return
        if d < st["best"] - 5.0:
            st["best"], st["since"] = d, st["t"]
        elif st["t"] - st["since"] > STUCK:
            st["fail"].append(LEGS[st["leg"]][0])
            d2 = max(d, 1.0)
            ahead = unreal.Vector(here.x + (target[0] - here.x) / d2 * 40.0, here.y + (target[1] - here.y) / d2 * 40.0, here.z)
            h = unreal.SystemLibrary.capsule_trace_single(w, here, ahead, 33.0, 94.0, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                                                          False, [pawn], unreal.DrawDebugTrace.NONE, True)
            t = hit_tuple(h)
            if t and t[0]:
                log("  blocked by %s.%s at %s normal %s (%+.0f cm over the feet)" % (
                    t[9].get_actor_label() if t[9] else "?", t[10].get_name() if t[10] else "?",
                    [round(v) for v in (t[4].x, t[4].y, t[4].z)], [round(v, 2) for v in (t[6].x, t[6].y, t[6].z)], t[4].z - feet))
            log("FAIL %s stuck before point %d at %s (%.0f cm away)" % (LEGS[st["leg"]][0], st["p"],
                                                                       [round(v) for v in (here.x, here.y, feet)], d))
            next_leg(w, pawn)
            return
        prev = pts[st["p"] - 1]
        sx, sy = target[0] - prev[0], target[1] - prev[1]
        sl = math.hypot(sx, sy) or 1.0
        ux, uy = sx / sl, sy / sl
        along = (here.x - prev[0]) * ux + (here.y - prev[1]) * uy
        ahead = min(sl, max(0.0, along) + 50.0)
        ax, ay = prev[0] + ux * ahead - here.x, prev[1] + uy * ahead - here.y
        al = math.hypot(ax, ay) or 1.0
        pc.set_control_rotation(unreal.Rotator(roll=0, pitch=0, yaw=math.degrees(math.atan2(uy, ux))))
        pawn.add_movement_input(unreal.Vector(ax / al, ay / al, 0), 1.0, True)
        return
    if st["phase"] == "shots":
        if os.environ.get("CLUBEXIT_SHOTS", "1") == "0" or st["shot"] >= len(SHOTS):
            log("summary: %d ok, %d failed (%s), %d spheres" % (st["ok"], len(st["fail"]), ", ".join(st["fail"]) or "-", st.get("spheres", -1)))
            cmd(w, "quit")
            st["phase"] = "done"
            return
        el = st["t"] - st["t0"]
        name, a, b = SHOTS[st["shot"]]
        if el < 0.1:
            aim(pawn, pc, a, b)
        elif el > 4.0 and not st.get("fired"):
            log("shot %s" % name)
            cmd(w, "HighResShot " + RES)
            st["fired"] = True
        elif el > 6.0:
            st["fired"] = False
            st["shot"] += 1
            st["t0"] = st["t"]


handle = unreal.register_slate_post_tick_callback(tick)
