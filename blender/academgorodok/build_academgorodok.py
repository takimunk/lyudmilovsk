"""Builds the Academgorodok of Laboratory No. 2 (Moscow, 1945-48) for Lyudmilovsk.

Run headless from the repo root:
    /Applications/Blender.app/Contents/MacOS/Blender -b -P blender/academgorodok/build_academgorodok.py -- [--no-render] [--views a,b]

Writes blender/export/academgorodok/<part>.glb (one mesh per house, the gates, the square) and
blender/export/academgorodok/layout.json (ensemble frame), plus control renders matched to the
reference photos in renders/academgorodok/. plan.py holds the plan and facades, details.py the
detail meshes, materials.py the preview materials.
"""
import json
import math
import os
import sys

import bpy
import bmesh
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tools"))
import common  # noqa: E402
import interiors  # noqa: E402
import plan  # noqa: E402
from character_standard import MIN_DOOR_CLEAR_HEIGHT_M, MIN_DOOR_CLEAR_WIDTH_M  # noqa: E402

try:
    import details as DET
except Exception as e:          # noqa: BLE001  - details.py still being written: boxes stand in
    print("ACADEM details.py unavailable:", e)
    DET = None

try:
    import furniture as FURN
except Exception as e:          # noqa: BLE001  - furniture.py still being written: bare rooms
    print("ACADEM furniture.py unavailable:", e)
    FURN = None

# see-through: a separate translucent, shadowless mesh (glass, and the tulle that hangs over every window,
# which as an opaque sheet would shut the daylight out of the rooms)
GLASS = {"AC_Glass", "AC_GGlass", "AC_FGlass", "AC_FLace"}
EXPORT_DIR = os.path.join(ROOT, "blender", "export", "academgorodok")
RENDER_DIR = os.path.join(ROOT, "renders", "academgorodok")
TEX_DIR = os.path.join(ROOT, "textures", "academ")
ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
DO_RENDER = "--no-render" not in ARGS

HOUSES = {}
LAMP_RND = __import__("random").Random(1951)
EXTRA_MATS = {
    "AC_Room": ("#8C7A62", 0.9, "lit room behind a window"),
    "AC_Curtain": ("#D8CDB4", 0.9, "net curtain"),
    "AC_Furniture": ("#3B2C22", 0.8, "silhouette of furniture deep in a room"),
}


# ------------------------------------------------------------------------------------------------
def hex_rgb(h):
    h = h.lstrip("#")
    c = [int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c] + [1.0]


def materials():
    try:
        import materials as MAT
        MAT.build_materials(TEX_DIR)
    except Exception as e:  # noqa: BLE001
        print("ACADEM materials.py unavailable, flat colours:", e)
    extra = list(interiors.INTERIOR_MATERIALS.items())
    try:
        import gastronom
        extra += list(gastronom.GASTRONOM_MATERIALS.items())
    except Exception:  # noqa: BLE001
        pass
    if FURN is not None:
        extra += list(FURN.FURNITURE_MATERIALS.items())
    for mod in ("houseplants", "library"):
        try:
            mm = __import__(mod)
            extra += list(getattr(mm, {"houseplants": "PLANT_MATERIALS", "library": "LIBRARY_MATERIALS"}[mod]).items())
        except Exception:  # noqa: BLE001
            pass
    for name, (hx, rough, _) in list(common.MATERIALS.items()) + list(EXTRA_MATS.items()) + extra:
        mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
        if not mat.use_nodes or "Principled BSDF" not in mat.node_tree.nodes or not mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].is_linked:
            mat.use_nodes = True
            b = mat.node_tree.nodes["Principled BSDF"]
            b.inputs["Base Color"].default_value = hex_rgb(hx)
            b.inputs["Roughness"].default_value = rough
            if name == "AC_Room":
                b.inputs["Emission Color"].default_value = hex_rgb("#FFD9A0")
                b.inputs["Emission Strength"].default_value = 0.15


def mesh_object(name, m, coll):
    """geom.Mesh -> bpy object with one material slot per used material name."""
    mats = sorted(set(f[0] for f in m.faces))
    me = bpy.data.meshes.new(name)
    me.from_pydata(m.verts, [], [f[1] for f in m.faces])
    uv = me.uv_layers.new(name="UVMap")
    li = 0
    for poly, (mat, idx, uvs) in zip(me.polygons, m.faces):
        poly.material_index = mats.index(mat)
        for k, loop in enumerate(poly.loop_indices):
            uv.data[loop].uv = uvs[k]
    for mn in mats:
        me.materials.append(bpy.data.materials.get(mn) or bpy.data.materials.new(mn))   # e.g. the AC_Sign_* boards
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0005)
    bm.to_mesh(me)
    bm.free()
    # soft and turned furniture is smooth-shaded with its creases kept (furniture.SMOOTH_MATERIALS)
    smooth = set(getattr(FURN, "SMOOTH_MATERIALS", set()) if FURN is not None else set())
    for mod in ("houseplants", "library"):
        try:
            smooth |= set(__import__(mod).SMOOTH_MATERIALS)
        except Exception:  # noqa: BLE001
            pass
    if smooth and any(mt.name in smooth for mt in me.materials):
        for p in me.polygons:
            p.use_smooth = me.materials[p.material_index].name in smooth
        if hasattr(me, "set_sharp_from_angle"):
            me.set_sharp_from_angle(angle=math.radians(getattr(FURN, "SMOOTH_ANGLE", 50.0)))
    return ob


# --- details ---------------------------------------------------------------------------------------
_cache = {}


def _key(name, kw):
    return name + "|" + json.dumps(kw, sort_keys=True, default=str)


NO_MIRROR = {"cartouche"}      # carries writing: placed, never mirrored


def detail_object(name, kw, mirrored=False):
    """One template object per (name, kwargs, mirrored); placed copies share its mesh."""
    k = _key(name, kw) + ("|m" if mirrored else "")
    if k in _cache:
        return _cache[k]
    if mirrored:
        base = detail_object(name, kw)
        ob = base.copy()
        ob.data = base.data.copy()
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        bmesh.ops.scale(bm, vec=(-1, 1, 1), verts=bm.verts[:])
        bmesh.ops.reverse_faces(bm, faces=bm.faces[:])
        bm.to_mesh(ob.data)
        bm.free()
        ob.name = "T_" + name + "_m"
        _cache[k] = ob
        return ob
    ob = None
    if DET is not None and hasattr(DET, "make"):
        ob = DET.make(name, **kw)
    if ob is None:
        ob = _placeholder(name, kw)
    ob.name = "T_" + name
    for c in ob.users_collection:
        c.objects.unlink(ob)
    _cache[k] = ob
    return ob


def _placeholder(name, kw):
    """Simple stand-ins until details.py covers a name (also the fallback for unknown names)."""
    from geom import Mesh, box
    m = Mesh()
    w, h = kw.get("w", 1.0), kw.get("h", 1.0)
    if name in ("window",):
        unit = kw.get("unit")
        box(m, "AC_Frame", -w / 2, w / 2, -0.16, -0.08, 0, 0.07)
        box(m, "AC_Frame", -w / 2, w / 2, -0.16, -0.08, h - 0.07, h)
        box(m, "AC_Frame", -w / 2, -w / 2 + 0.07, -0.16, -0.08, 0, h)
        box(m, "AC_Frame", w / 2 - 0.07, w / 2, -0.16, -0.08, 0, h)
        box(m, "AC_Frame", -0.04, 0.04, -0.16, -0.08, 0, h)
        box(m, "AC_Frame", -w / 2, w / 2, -0.16, -0.08, h * 0.72, h * 0.72 + 0.06)
        m.face("AC_Glass", [(-w / 2, -0.12, 0), (w / 2, -0.12, 0), (w / 2, -0.12, h), (-w / 2, -0.12, h)])
        box(m, "AC_Zinc", -w / 2 - 0.05, w / 2 + 0.05, -0.1, 0.06, -0.03, 0.0)
    elif name == "door":
        box(m, "AC_Door", -w / 2, w / 2, -0.3, -0.22, 0, h)
    elif name == "surround":
        if kw.get("style") in ("band", "ears", "hood", "hood_ears", "arch"):
            t = 0.16
            box(m, "AC_Trim", -w / 2 - t, w / 2 + t, 0, 0.04, h, h + t)
            box(m, "AC_Trim", -w / 2 - t, -w / 2, 0, 0.04, 0, h)
            box(m, "AC_Trim", w / 2, w / 2 + t, 0, 0.04, 0, h)
            if "hood" in kw.get("style"):
                box(m, "AC_Trim", -w / 2 - 0.3, w / 2 + 0.3, 0, 0.18, h + t, h + t + 0.14)
    elif name == "steps":
        n, rise, tread = kw["n"], kw["rise"], kw["tread"]
        for i in range(n):
            box(m, "AC_Concrete", -w / 2, w / 2, 0, tread * (n - i), rise * i - 0.3, rise * (i + 1))
    elif name in ("portal_dark", "portal_side"):
        mat = "AC_RusticDark" if name == "portal_dark" else "AC_Quoin"
        box(m, mat, -w / 2 - 0.55, -w / 2, 0, 0.12, 0, h + 0.5)
        box(m, mat, w / 2, w / 2 + 0.55, 0, 0.12, 0, h + 0.5)
        box(m, mat, -w / 2, w / 2, 0, 0.12, h, h + 0.5)
    elif name == "console_big":
        box(m, "AC_Trim", -0.2, 0.2, 0, 0.9, -h, 0)
    elif name == "slab":
        box(m, "AC_Trim", -w / 2, w / 2, 0, kw["depth"], 0, kw["t"])
    elif name == "pilaster":
        box(m, "AC_Trim", -w / 2, w / 2, -0.1, 0.1, 0, h)
    elif name == "column":
        d = kw["d"]
        box(m, "AC_Trim", -d / 2, d / 2, -d / 2, d / 2, 0, h)
    elif name == "parapet":
        box(m, "AC_Trim", -kw["length"] / 2, kw["length"] / 2, -0.06, 0.06, 0, h)
    elif name in ("railing", "railing_side"):
        L = kw["length"]
        if name == "railing_side":
            box(m, "AC_Iron", -0.02, 0.02, 0, L, h - 0.04, h)
        else:
            box(m, "AC_Iron", -L / 2, L / 2, -0.02, 0.02, h - 0.04, h)
            for i in range(int(L / 0.12)):
                x = -L / 2 + 0.06 + i * 0.12
                box(m, "AC_Iron", x - 0.008, x + 0.008, -0.008, 0.008, 0, h)
    elif name == "balcony":
        L, D = kw["length"], kw["depth"]
        box(m, "AC_Concrete", -L / 2, L / 2, 0, D, -0.2, 0)
        box(m, "AC_Iron", -L / 2, L / 2, D - 0.02, D, 0.96, 1.0)
        for i in range(int(L / 0.12)):
            x = -L / 2 + 0.06 + i * 0.12
            box(m, "AC_Iron", x - 0.008, x + 0.008, D - 0.02, D - 0.004, 0, 1.0)
    elif name == "oculus":
        d = kw["d"]
        box(m, "AC_Trim", -d / 2, d / 2, 0, 0.08, -d / 2, d / 2)
    elif name == "swag":
        box(m, "AC_Trim", -kw["length"] / 2, kw["length"] / 2, 0, 0.06, -0.15, 0.1)
    elif name == "modillion":
        box(m, "AC_Trim", -0.06, 0.06, 0, kw["depth"], -0.22, 0)
    elif name == "rafter":
        box(m, "AC_Timber", -0.07, 0.07, 0, kw["length"], -0.18, 0)
    elif name == "chimney":
        box(m, "AC_Brick", -0.45, 0.45, -0.3, 0.3, 0, h)
        box(m, "AC_Zinc", -0.55, 0.55, -0.4, 0.4, h + 0.3, h + 0.35)
    elif name == "downpipe":
        box(m, "AC_Zinc", -0.06, 0.06, -0.06, 0.06, 0.0, h)
    elif name == "bay_window":
        D = kw["depth"]
        box(m, "AC_Wall", -w / 2, w / 2, 0, D, 0, 0.9)
        box(m, "AC_Glass", -w / 2 + 0.1, w / 2 - 0.1, D - 0.1, D - 0.05, 0.9, h - 0.4)
        box(m, "AC_Trim", -w / 2 - 0.1, w / 2 + 0.1, 0, D + 0.1, h - 0.4, h)
    elif name == "vent":
        s = kw["s"]
        box(m, "AC_Iron", -s / 2, s / 2, -0.05, 0.01, 0, s)
    elif name == "quoin":
        for i in range(kw["courses"]):
            l = 0.9 if i % 2 == 0 else 0.6
            z0 = i * kw["course_h"]
            box(m, "AC_Quoin", -0.05, l, -0.05, 0.06, z0, z0 + kw["course_h"] - 0.04)
    elif name == "cartouche":
        box(m, "AC_Trim", -0.5, 0.5, -0.1, 0.25, 0, 0.35)
        box(m, "AC_Trim", -0.3, 0.3, 0.0, 0.12, 0.35, 1.2)
    elif name == "wicket_block":
        box(m, "AC_Wall", -1.9, 1.9, -0.4, 0.4, 0, 4.5)
    elif name == "scroll_pylon":
        box(m, "AC_Pylon", -0.6, 0.6, -0.25, 0.25, 0, 2.8)
    else:
        box(m, "AC_Trim", -0.2, 0.2, 0, 0.2, 0, 0.2)
    coll = bpy.context.scene.collection
    return mesh_object("P_" + name, m, coll)


def place_details(details, mirror, coll):
    """Copies of the detail templates (house frame). A mirrored house (Maksimova 8) gets mirrored
    copies at mirrored places, except pieces with writing, which are only moved and turned."""
    copies = []
    for name, kw, pos, yaw in details:
        x, y, z = pos
        corner = math.radians(-45) if name == "cartouche" else 0.0      # faces the corner's diagonal
        if mirror:
            x, yaw, corner = -x, -yaw, -corner            # a direction at angle a goes to pi - a
        t = detail_object(name, kw, mirrored=mirror and name not in NO_MIRROR)
        ob = t.copy()
        coll.objects.link(ob)
        ob.matrix_world = Matrix.Translation((x, y, z)) @ Matrix.Rotation(yaw + corner, 4, "Z")
        copies.append(ob)
    return copies


def join(target, obs):
    if not obs:
        return target
    with bpy.context.temp_override(active_object=target, selected_editable_objects=[target] + obs,
                                   selected_objects=[target] + obs):
        bpy.ops.object.join()
    return target


def split_glass(ob, coll):
    """Moves the glass faces of ob into their own object <name>_Glass (same transform)."""
    idx = [i for i, mt in enumerate(ob.data.materials) if mt and mt.name in GLASS]
    if not idx:
        return None
    g = ob.copy()
    g.data = ob.data.copy()
    g.name = ob.name + "_Glass"
    coll.objects.link(g)
    for o, keep in ((ob, False), (g, True)):
        bm = bmesh.new()
        bm.from_mesh(o.data)
        kill = [f for f in bm.faces if (f.material_index in idx) != keep]
        bmesh.ops.delete(bm, geom=kill, context="FACES")
        bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
        bm.to_mesh(o.data)
        bm.free()
    return g


def apply_notches(ob, notches):
    """Cuts the roof's overhang away in front of gabled bays (house frame boxes, x0 x1 y0 y1)."""
    if not notches:
        return
    for x0, x1, y0, y1 in notches:
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
        # cut by two vertical planes and the wall plane, then delete faces inside the box above eaves
        for co, no in (((x0, 0, 0), (1, 0, 0)), ((x1, 0, 0), (1, 0, 0)), ((0, y0, 0), (0, 1, 0)), ((0, y1, 0), (0, 1, 0))):
            res = bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=co, plane_no=no)
        kill = []
        for f in bm.faces:
            c = f.calc_center_median()
            mat = ob.data.materials[f.material_index].name if f.material_index < len(ob.data.materials) else ""
            if x0 < c.x < x1 and y0 < c.y < y1 and mat in ("AC_Roof", "AC_RoofBrown", "AC_Trim", "AC_Timber") and c.z > plan.eaves_z(3) - 0.2:
                kill.append(f)
        bmesh.ops.delete(bm, geom=kill, context="FACES")
        bm.to_mesh(ob.data)
        bm.free()


# --- main --------------------------------------------------------------------------------------------
def build():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    materials()
    scene = bpy.context.scene
    coll = bpy.data.collections.new("Academgorodok")
    scene.collection.children.link(coll)
    work = bpy.data.collections.new("Work")
    scene.collection.children.link(work)

    houses = plan.ensemble()
    parts = {}
    for name, h in houses.items():
        f = h.frame
        m = h.mesh
        mi = h.inside
        if h.interiors:
            m_in, rooms = interiors.build(h)
            mi.extend(m_in)
            h.rooms = rooms
            if FURN is not None:
                for r in rooms:
                    if r["kind"] != "stair":
                        try:
                            parts_ = FURN.furnish_parts(r)
                            for key in ("finish", "furniture", "curtains"):
                                mi.extend(parts_[key])
                            # one lamp per lived-in room (the ceiling fitting): the rooms are dark at night
                            # at night a lamp burns in about a quarter of the rooms (AcademToggle.cpp lights
                            # them from dusk); the others stay dark
                            # by day only the room the player is in; the rest stay dark from outside
                            if r["kind"] in ("living", "study", "bedroom", "kitchen", "corridor") and parts_["lights"]:
                                x, y, z, col, lm = parts_["lights"][0]
                                lit = r["kind"] != "corridor" and LAMP_RND.random() < 0.27
                                h.lights = getattr(h, "lights", []) + [(x, y, z, col, min(lm, 1000), "room_lit" if lit else "room")]
                        except Exception as e:  # noqa: BLE001
                            print("ACADEM furnish failed for %s: %s" % (r["kind"], e))
            print("ACADEM %s: %d rooms inside" % (name, len(rooms)))
        if f.mirror:
            m = m.transformed(lambda p: (-p[0], p[1], p[2]))
            m.faces = [(mt, list(reversed(i)), list(reversed(uv))) for mt, i, uv in m.faces]
            mi = mi.transformed(lambda p: (-p[0], p[1], p[2]))
            mi.faces = [(mt, list(reversed(i)), list(reversed(uv))) for mt, i, uv in mi.faces]
        if mi.faces:
            # the insides (flats, stairs, the shop, their furniture) are drawn only near by (Unreal:
            # import_academgorodok.py); far away dark opaque glass closes the windows instead
            obi = mesh_object(name + "_Inside", mi, coll)
            obi.matrix_world = Matrix.Translation((f.ox, f.oy, f.oz)) @ Matrix.Rotation(f.yaw, 4, "Z")
            parts[name + "_Inside"] = obi
            gi = split_glass(obi, coll)
            if gi is not None:
                gi.name = name + "_InsideGlass"
                parts[name + "_InsideGlass"] = gi
        ob = mesh_object(name, m, coll)
        notches = [(-b, -a, c, d) if f.mirror else (a, b, c, d) for a, b, c, d in getattr(h, "notches", [])]
        apply_notches(ob, notches)
        join(ob, place_details(h.details, f.mirror, coll))
        ob.matrix_world = Matrix.Translation((f.ox, f.oy, f.oz)) @ Matrix.Rotation(f.yaw, 4, "Z")
        parts[name] = ob
        glass = split_glass(ob, coll)
        if glass is not None:
            parts[name + "_Glass"] = glass
            far = glass.copy()                       # same panes, opaque dark glass, from 55 m on
            far.data = glass.data.copy()
            far.name = name + "_GlassFar"
            coll.objects.link(far)
            parts[name + "_GlassFar"] = far

    gate_dets, lines, gate_mesh = plan.gates(houses)
    gate_ob = mesh_object("Gates", gate_mesh, coll)
    copies = []
    for name, kw, pos, yaw, d in gate_dets:
        t = detail_object(name, kw)
        c = t.copy()
        coll.objects.link(c)
        c.matrix_world = Matrix.Translation(Vector(pos)) @ Matrix.Rotation(yaw, 4, "Z")
        copies.append(c)
    join(gate_ob, copies)
    parts["Gates"] = gate_ob

    sq_mesh = plan.square_mesh()
    sq_mesh.extend(plan.benches(houses))
    sq = mesh_object("Square", sq_mesh, coll)
    fx, fy = plan.SQUARE["fountain"]
    copies = []
    for name, kw in (("fountain", {}), ("flowerbed_railing", {"radius": plan.SQUARE["bed_r"]})):
        t = detail_object(name, kw)
        c = t.copy()
        coll.objects.link(c)
        c.matrix_world = Matrix.Translation((fx, fy, 0.0))
        copies.append(c)
    join(sq, copies)
    parts["Square"] = sq

    # apply transforms (mirror included) and fix normals after the mirror
    for name, ob in parts.items():
        with bpy.context.temp_override(active_object=ob, selected_editable_objects=[ob], selected_objects=[ob]):
            bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    return parts, houses, lines


def export(parts):
    """One .glb with every part at the origin (Interchange reliably imports one file per process);
    layout.json says where each goes in the ensemble frame."""
    os.makedirs(EXPORT_DIR, exist_ok=True)
    layout = {"frame": "ensemble metres: x east, y north, z up; origin = Novikova 1 footprint centre at ground",
              "parts": {}, "trees": plan.square_trees(), "lights": [], "garden": plan.SQUARE_GARDEN}
    layout["doors"] = []
    for name, h in HOUSES.items():
        for i, d in enumerate(getattr(h, "doors", [])):
            f = h.frame
            p0 = f(d["at"])
            p1 = f((d["at"][0] + d["out"][0], d["at"][1] + d["out"][1], d["at"][2]))
            layout["doors"].append({"name": "%s_Door_%03d" % (name, i), "at": list(p0),
                                    "out": [p1[0] - p0[0], p1[1] - p0[1]], "hole": d["hole"], "wall": d["wall"],
                                    "leaves": d["leaves"], "hinge": "left", "frame": False,
                                    "open": d["open"], "locked": d["locked"]})
    # every plant is an AProceduralPlant: the square's beds and hedges, the library's pots
    layout["plants"] = [dict(pl) for pl in plan.SQUARE_PLANTS]
    layout["benches"] = [dict(b) for b in plan.BENCHES]
    for name, h in HOUSES.items():
        for pl in getattr(h, "plants", []):
            p0 = h.frame(tuple(pl["at"]))
            a = math.radians(pl["yaw"])
            p1 = h.frame((pl["at"][0] + math.cos(a), pl["at"][1] + math.sin(a), pl["at"][2]))
            layout["plants"].append(dict(pl, at=list(p0), yaw=math.degrees(math.atan2(p1[1] - p0[1], p1[0] - p0[0])),
                                         indoor=True))
    layout["walks"] = []
    for name, h in HOUSES.items():
        for label, pts in getattr(h, "walks", []):
            f = h.frame
            layout["walks"].append({"house": name, "label": label, "points": [list(f(p)) for p in pts]})
        for l in getattr(h, "lights", []):
            x, y, z, col, lm = l[:5]
            kind = l[5] if len(l) > 5 else "shop"
            p = h.frame((x, y, z))
            layout["lights"].append({"at": list(p), "colour": col, "lumens": lm, "kind": kind})
    bpy.ops.object.select_all(action="DESELECT")
    for name, ob in parts.items():
        loc = ob.matrix_world.translation.copy()
        ob.location = (0, 0, 0)
        ob.select_set(True)
        tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
        layout["parts"][name] = {"location": list(loc), "tris": tris}
        print("ACADEM part %s: %d tris" % (name, tris))
    path = os.path.join(EXPORT_DIR, "academgorodok.glb")
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True, export_apply=True,
                              export_cameras=False, export_lights=False, export_materials="EXPORT",
                              export_image_format="NONE")      # slot names only: Unreal swaps in its own materials
    for name, ob in parts.items():
        ob.location = layout["parts"][name]["location"]
    with open(os.path.join(EXPORT_DIR, "layout.json"), "w") as f:
        json.dump(layout, f, indent=1)
    print("ACADEM exported", path)


# --- control renders ---------------------------------------------------------------------------------
# camera (ensemble frame), target, lens (35 mm equivalent focal length), matched to the photos
VIEWS = {
    # commons_Marshal_Novikov_str._1_2_.jpg: ~25 m east of Novikova 1 on its axis, eye level
    "n1_front": ((7.25 + 25.5, 1.3, 1.6), (0.0, 1.3, 7.5), 26.0, (1500, 2000)),
    # pastvu_1983687.jpg: bird's-eye from the south-east
    "aerial": ((130.0, -150.0, 85.0), (-5.0, -10.0, 0.0), 45.0, (2000, 1207)),
    # commons_Maksimova_str._8_1_.jpg: ~12 m off the quoin corner of Maksimova 8
    "m8_corner": (None, None, 24.0, (2000, 1500)),
    # commons_Marshal_Novikov_str._1_gates.jpg: street side of the south gate, looking west
    "s_gate": ((24.0, -40.0, 1.6), (4.0, -38.5, 5.0), 24.0, (2000, 1500)),
    # commons_Moscow_Marshal_Novikov_street_3_2025-02_437.jpg: GPS 55.80181 37.46904, heading 157
    "n3_street": ((-41.0 - plan.N1_OSM[0], 145.0 - plan.N1_OSM[1], 1.6), None, 21.0, (2000, 1339)),
}


def camera_for(name, houses):
    cam_pos, target, lens35, res = VIEWS[name]
    if name == "m8_corner":
        h = houses["Maksimova8"]
        c = h.frame((plan.S_L / 2, plan.S_D / 2, 0.0))
        centre = h.frame((0.0, 0.0, 0.0))
        d = Vector((c[0] - centre[0], c[1] - centre[1], 0)).normalized()
        cam_pos = (c[0] + d.x * 12.0, c[1] + d.y * 12.0, 1.6)
        target = (c[0], c[1], 6.5)
    if target is None:   # a heading (degrees from north, clockwise)
        hd = math.radians(157.0)
        target = (cam_pos[0] + math.sin(hd) * 50, cam_pos[1] + math.cos(hd) * 50, 4.0)
    return cam_pos, target, lens35, res


def render_views(houses, names):
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE"
    world = bpy.data.worlds.new("Sky")
    scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    bg.inputs["Color"].default_value = (0.55, 0.7, 0.95, 1)
    bg.inputs["Strength"].default_value = 1.0
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 4.0
    sun.rotation_euler = (math.radians(50), 0, math.radians(200))
    scene.collection.objects.link(sun)
    ground = bpy.data.meshes.new("Ground")
    ground.from_pydata([(-400, -400, 0), (400, -400, 0), (400, 400, 0), (-400, 400, 0)], [], [(0, 1, 2, 3)])
    g = bpy.data.objects.new("Ground", ground)
    gm = bpy.data.materials.new("GroundMat")
    gm.use_nodes = True
    gm.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.12, 0.16, 0.08, 1)
    ground.materials.append(gm)
    scene.collection.objects.link(g)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    os.makedirs(RENDER_DIR, exist_ok=True)
    for name in names:
        pos, target, lens35, res = camera_for(name, houses)
        scene.render.resolution_x, scene.render.resolution_y = res
        cam.data.sensor_fit = "HORIZONTAL" if res[0] >= res[1] else "VERTICAL"
        cam.data.sensor_width = 36.0 if res[0] >= res[1] else 24.0
        cam.data.sensor_height = 24.0 if res[0] >= res[1] else 36.0
        cam.data.lens = lens35
        cam.location = pos
        d = Vector(target) - Vector(pos)
        cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = os.path.join(RENDER_DIR, "view_%s.png" % name)
        bpy.ops.render.render(write_still=True)
        print("ACADEM render", scene.render.filepath)


def check_doors(houses):
    """Every door opening must pass the character standard (tools/character_standard.py)."""
    bad = []
    for name, h in houses.items():
        for dn, kw, pos, yaw in h.details:
            if dn == "door" and (kw["w"] < MIN_DOOR_CLEAR_WIDTH_M or kw["h"] < MIN_DOOR_CLEAR_HEIGHT_M):
                bad.append((name, kw))
    print("ACADEM doors checked; too small:", bad)
    return bad


if __name__ == "__main__":
    parts, houses, lines = build()
    HOUSES = houses
    check_doors(houses)
    export(parts)
    if DO_RENDER:
        names = list(VIEWS)
        for a in ARGS:
            if a.startswith("--views="):
                names = a.split("=", 1)[1].split(",")
        render_views(houses, names)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(EXPORT_DIR, "academgorodok_preview.blend"))
