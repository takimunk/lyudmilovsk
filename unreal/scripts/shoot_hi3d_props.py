"""Offscreen shots of the hi3d town props where tools/city/hi3d_props.py placed them (Content/City/hi3d_props.json),
in Lvl_Scene01: no PIE, no window. Per shot the camera stands in front of the prop (its yaw is the way its front looks)
at a distance from its height (blender/props_hi3d/game/<id>.json), looks at its middle and takes a HighResShot; a far
shot of some checks LOD pops. Per prop it also logs (HI3DTEST): the ground under its footprint against its base
(floating / sunk), whether a trace into it hits its collision, and for chandeliers the ceiling over the top.

  unreal/scripts/shoot_hi3d_props.sh            (HI3D_SHOTS=sundial_square,girl_dove_pond for some only)
"""
import json
import math
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
DOC = json.load(open(os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "City", "hi3d_props.json")))
GAME = os.path.join(ROOT, "blender", "props_hi3d", "game")
RES = os.environ.get("HI3D_RES", "1600x900")
WAIT = float(os.environ.get("HI3D_WAIT", "5"))
DAY = unreal.DateTime(2026, 10, 21, 13, 0, 0, 0)


def size(prop):
    p = os.path.join(GAME, prop + ".json")
    if os.path.exists(p):
        g = json.load(open(p))
        return g["max"][2] - g["min"][2], g.get("radius", 0.6)
    return 2.0, 0.6


def shots():
    out, seen = [], set()
    for it in DOC["items"]:
        prop = it["prop"]
        if not os.path.exists(os.path.join(GAME, prop + ".glb")):
            continue                                         # not modelled (or on hold): nothing to see
        key = prop if prop != "chandelier" else it["id"].rsplit("_", 1)[0]
        if key in seen and prop not in ("chandelier",):
            continue
        seen.add(key)
        h, r = size(prop)
        x, y, z = it["at"]
        yaw = math.radians(it["yaw"])
        fx, fy = math.cos(yaw), math.sin(yaw)
        if prop == "chandelier":
            # 3.5 m under its top, a few metres off: in the hall it hangs in (an upper storey's hall too; from the ground
            # floor's eye height a store's chandeliers were behind the slab)
            cam = (x + 5.0 * fx, y + 5.0 * fy, z - 3.5)
            tgt = (x, y, z - 0.6 * h)
        else:
            d = max(3.2, 1.5 * h + r)
            cam = (x + d * fx, y + d * fy, z + 1.7)
            tgt = (x, y, z + 0.5 * h)
        out.append((it["id"], cam, tgt, it, h, r))
        if prop in ("sundial", "girl_dove", "gull_stele"):          # LOD pops: the same from far
            d = 30.0
            out.append((it["id"] + "_far", (x + d * fx, y + d * fy, z + 2.0), (x, y, z + 0.5 * h), it, h, r))
    return out


SHOTS = shots()
if os.environ.get("HI3D_SHOTS"):
    want = os.environ["HI3D_SHOTS"].split(",")
    SHOTS = [s for s in SHOTS if s[0] in want or s[0].rsplit("_far", 1)[0] in want]
st = {"t": 0.0, "i": -1, "phase": 0, "frames": [], "probed": False}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name().startswith("Lvl_") and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def cm(q):
    return unreal.Vector(q[0] * 100.0, q[1] * 100.0, q[2] * 100.0)


def trace(w, a, b, ignore):
    """First visibility hit a -> b (cm): (component name, owner name, z cm) or None."""
    try:
        hit = unreal.SystemLibrary.line_trace_single(w, a, b, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore,
                                                     unreal.DrawDebugTrace.NONE, True)
    except Exception as e:  # noqa: BLE001
        unreal.log_warning("HI3DTEST trace failed: %s" % e)
        return None
    if isinstance(hit, tuple):
        hit = hit[0]
    if hit is None:
        return None
    t = hit.to_tuple()
    # HitResult tuple: (blocking_hit, initial_overlap, time, distance, location, impact_point, normal, impact_normal,
    #                   phys_mat, hit_actor, hit_component, hit_bone_name, bone_name, hit_item, element_index,
    #                   face_index, trace_start, trace_end)
    if not t[0]:
        return None
    comp, actor = t[10], t[9]
    return (comp.get_name() if comp else "?", actor.get_name() if actor else "?", t[5].z)


def probe(w, pawn):
    """Ground, collision and ceiling per placed prop (once)."""
    ign = [pawn]
    for it in DOC["items"]:
        prop = it["prop"]
        if not os.path.exists(os.path.join(GAME, prop + ".glb")):
            continue
        h, r = size(prop)
        x, y, z = it["at"]
        if prop == "chandelier":
            up = trace(w, cm((x, y, z - 0.05)), cm((x, y, z + 3.0)), ign)
            unreal.log_warning("HI3DTEST %s ceiling over the top: %s" % (
                it["id"], "%.2f m above (%s)" % (up[2] / 100.0 - z, up[1]) if up else "NONE within 3 m"))
            continue
        # ground just outside the footprint, four sides: how far the base is above (+) / below (-) it
        gaps = []
        for ang in (0, 90, 180, 270):
            a = math.radians(ang)
            gx, gy = x + (r + 0.25) * math.cos(a), y + (r + 0.25) * math.sin(a)
            g = trace(w, cm((gx, gy, z + 1.5)), cm((gx, gy, z - 3.0)), ign)
            gaps.append(None if not g else z - g[2] / 100.0)
        ok = [g for g in gaps if g is not None]
        # a horizontal trace into its middle from 3 m: its collision (Hi3d_<prop> component) should block
        yaw = math.radians(it["yaw"])
        a = (x + (r + 3.0) * math.cos(yaw), y + (r + 3.0) * math.sin(yaw), z + min(1.0, 0.4 * h))
        hit = trace(w, cm(a), cm((x, y, a[2])), ign)
        unreal.log_warning("HI3DTEST %s base vs ground %s m; trace in hits %s" % (
            it["id"], ", ".join("%.2f" % g if g is not None else "none" for g in gaps), hit[0] if hit else "NOTHING"))
        if ok and (max(ok) > 0.12 or min(ok) < -0.25):
            unreal.log_warning("HI3DTEST FAIL %s floats/sinks (%s)" % (it["id"], ok))
        if prop_collides(prop) and not (hit and "Hi3d" in hit[0]):
            unreal.log_warning("HI3DTEST FAIL %s: no collision hit (%s)" % (it["id"], hit))


def prop_collides(prop):
    return DOC["meshes"].get(prop, {}).get("collide", False)


def tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < 5.0:
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    if not st["probed"] and st["t"] > 25.0:
        st["probed"] = True
        try:
            probe(w, pawn)
        except Exception as e:  # noqa: BLE001
            unreal.log_warning("HI3DTEST FAIL probe raised %s" % e)
    if st["phase"] == 0:
        if not st["probed"]:
            return
        st["i"] += 1
        if st["i"] >= len(SHOTS):
            unreal.log_warning("HI3DTEST done: %d shots" % len(SHOTS))
            cmd(w, "quit")
            st["phase"] = 9
            return
        name, cam, tgt, it, h, r = SHOTS[st["i"]]
        if st["i"] == 0:
            for c in ["t.MaxFPS 0", "au.LinearGainScalarForFinalOutut 0", "lyud.Weather.Preset clear"]:
                cmd(w, c)
        for cal in unreal.ObjectIterator(unreal.GameCalendar):
            if cal.get_world() == w:
                cal.set_now(DAY)
        if hasattr(pawn, "character_movement"):
            pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
        eye = 0.0
        cam_comp = pawn.get_component_by_class(unreal.CameraComponent)
        if cam_comp:
            eye = cam_comp.get_world_location().z - pawn.get_actor_location().z
        pawn.set_actor_enable_collision(False)
        a, b = cm(cam), cm(tgt)
        pawn.set_actor_location(unreal.Vector(a.x, a.y, a.z - eye), False, True)
        dx, dy, dz = b.x - a.x, b.y - a.y, b.z - a.z
        pc.set_control_rotation(unreal.Rotator(roll=0, pitch=math.degrees(math.atan2(dz, math.hypot(dx, dy))),
                                               yaw=math.degrees(math.atan2(dy, dx))))
        st["t0"] = st["t"]
        st["phase"] = 1
    elif st["phase"] == 1 and st["t"] - st["t0"] <= WAIT + (25.0 if st["i"] == 0 else 0.0):
        if st["t"] - st["t0"] > 2.0:
            st["frames"].append(dt)
    elif st["phase"] == 1:
        name = SHOTS[st["i"]][0]
        f = sorted(st["frames"]) or [0.0]
        unreal.log_warning("HI3DSHOT %s at %s: %d frames, median %.1f ms" % (name, pawn.get_actor_location(), len(f),
                                                                            1000 * f[len(f) // 2]))
        st["frames"] = []
        cmd(w, "HighResShot " + RES)
        st["phase"] = 2
        st["t0"] = st["t"]
    elif st["phase"] == 2 and st["t"] - st["t0"] > 1.5:
        st["phase"] = 0


unreal.register_slate_post_tick_callback(tick)
