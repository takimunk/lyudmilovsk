"""Turns the raw Hi3D models of the cable-car metro (design/transport/gen/<name>.glb, ~300k triangles and two 8k
textures each) into game meshes: decimated to a budget, textures cut to 1-2k, set in the game's frame and size, the
cabin's door side cut open, the pylons split into feet / shaft / head so the game can stretch only the shaft.
Writes design/transport/game/<part>.glb and design/transport/game/kit.json (sizes the game reads, copied to
Content/Transport/kit.json).

  Blender -b -P blender/transport/optimize.py [-- name ...]

Frame of the output (Blender, Z up; Unreal gets (x, -y, z)): metres; origin on the base's centre.
  cabin: long side along X, the door on +Y (Unreal: left of the way it goes, -Y), origin under the body's bottom.
  pylon_*: rope across the crossarm: line pylon's rope along Y (crossarm along X), corner pylon's along X.
  top_*: the station crown, base at 0, D_TOP across.
"""
import bmesh
import bpy
import json
import math
import os
import sys
from mathutils import Matrix, Vector

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
GEN = os.path.join(ROOT, "design", "transport", "gen")
OUT = os.path.join(ROOT, "design", "transport", "game")

CABIN_LEN = 3.9          # m along the rope (a roomy 4-seat egg: ~2 m of headroom inside)
PYLON_ARM = 10.0         # m across the line pylon's crossarm (the two ropes are 9 m apart)
CORNER_ARM = 10.0
D_TOP = 21.0             # m across a station's crown
BUDGET = {"cabin": 14000, "pylon_line": 7000, "pylon_corner": 9000, "top": 22000}
TEX = {"cabin": 2048, "pylon_line": 1024, "pylon_corner": 1024, "top": 2048}
FEET, HEAD = 0.13, 0.15  # fractions of a pylon's height kept unstretched at the bottom and the top


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def load(name):
    bpy.ops.import_scene.gltf(filepath=os.path.join(GEN, name + ".glb"))
    objs = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    if len(objs) > 1:
        bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    o.parent = None
    return o


def bounds(o):
    vs = [o.matrix_world @ v.co for v in o.data.vertices]
    lo = Vector((min(v.x for v in vs), min(v.y for v in vs), min(v.z for v in vs)))
    hi = Vector((max(v.x for v in vs), max(v.y for v in vs), max(v.z for v in vs)))
    return lo, hi


def transform(o, m):
    o.data.transform(m)
    o.data.update()


def decimate(o, tris):
    n = sum(len(p.vertices) - 2 for p in o.data.polygons)
    if n <= tris:
        return n
    mod = o.modifiers.new("dec", "DECIMATE")
    mod.decimate_type = "COLLAPSE"
    mod.ratio = tris / n
    mod.use_collapse_triangulate = True
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return sum(len(p.vertices) - 2 for p in o.data.polygons)


def shrink_textures(size):
    for img in bpy.data.images:
        if img.size[0] > size:
            img.scale(size, size)
        img.pack()


def export(objs, name):
    os.makedirs(OUT, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, name + ".glb"), use_selection=True, export_format="GLB",
                              export_image_format="JPEG", export_jpeg_quality=88, export_yup=True, export_apply=True)


def split_z(o, z0, z1, name):
    """A copy of o with only the faces whose centre is in z0..z1 (the cut faces stay whole)."""
    c = o.copy()
    c.data = o.data.copy()
    c.name = name
    bpy.context.scene.collection.objects.link(c)
    bm = bmesh.new()
    bm.from_mesh(c.data)
    kill = [f for f in bm.faces if not (z0 <= f.calc_center_median().z < z1)]
    bmesh.ops.delete(bm, geom=kill, context="FACES")
    bm.to_mesh(c.data)
    bm.free()
    return c


def tri_count(o):
    return sum(len(p.vertices) - 2 for p in o.data.polygons)


def cabin(kit):
    o = load("cabin")
    lo, hi = bounds(o)
    # long side along Y in the raw model, the door on +X: turn so the long side is X and the door +Y
    transform(o, Matrix.Rotation(math.radians(90), 4, "Z"))
    lo, hi = bounds(o)
    s = CABIN_LEN / (hi.x - lo.x)
    c = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
    transform(o, Matrix.Scale(s, 4) @ Matrix.Translation(-c))
    lo, hi = bounds(o)
    # the body: where the shell is wide (the hanger above it is thin)
    zs = sorted(set(round(v.co.z, 2) for v in o.data.vertices))
    width_at = {}
    for v in o.data.vertices:
        k = round(v.co.z, 1)
        width_at[k] = max(width_at.get(k, 0.0), abs(v.co.y))
    body_top = max(k for k, w in width_at.items() if w > (hi.y - lo.y) * 0.3)
    grip = hi.z
    # cut the door open: the side faces on +Y in the middle third, from just over the floor to under the roof
    door_half = 0.42
    floor = 0.22
    door_top = min(body_top - 0.2, floor + 2.0)
    bm = bmesh.new()
    bm.from_mesh(o.data)
    bm.normal_update()
    kill = []
    for f in bm.faces:
        p = f.calc_center_median()
        if abs(p.x) < door_half and floor + 0.05 < p.z < door_top and p.y > 0.0 and f.normal.y > 0.25:
            kill.append(f)
    bmesh.ops.delete(bm, geom=kill, context="FACES")
    bm.to_mesh(o.data)
    bm.free()
    n = decimate(o, BUDGET["cabin"])
    shrink_textures(TEX["cabin"])
    export([o], "SM_TR_Cabin")
    kit["cabin"] = {"length": round(hi.x - lo.x, 3), "width": round(hi.y - lo.y, 3), "grip": round(grip, 3),
                    "body_top": round(body_top, 3), "floor": floor, "door_half": door_half, "door_top": round(door_top, 3),
                    "tris": n, "door_faces_cut": len(kill)}


def pylon(kit, name, arm, rope_axis):
    o = load(name)
    lo, hi = bounds(o)
    # crossarm (or sheave) width across the rope sets the scale
    across = (hi.x - lo.x) if rope_axis == "Y" else (hi.y - lo.y)
    s = arm / across
    c = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
    transform(o, Matrix.Scale(s, 4) @ Matrix.Translation(-c))
    lo, hi = bounds(o)
    h = hi.z
    # the rope saddle: the height where the model is widest across the rope, in the head
    best, saddle = 0.0, h * 0.9
    for v in o.data.vertices:
        if v.co.z > h * (1 - HEAD - 0.05):
            w = abs(v.co.x) if rope_axis == "Y" else abs(v.co.y)
            if w > best:
                best, saddle = w, v.co.z
    n = decimate(o, BUDGET[name])
    shrink_textures(TEX[name])
    fz, hz = h * FEET, h * (1 - HEAD)
    feet = split_z(o, -1.0, fz, "feet")
    shaft = split_z(o, fz, hz, "shaft")
    head = split_z(o, hz, h + 1.0, "head")
    # the shaft to 1 m high from 0 (the game stretches it), the head from 0
    transform(shaft, Matrix.Scale(1.0 / (hz - fz), 4, Vector((0, 0, 1))) @ Matrix.Translation((0, 0, -fz)))
    transform(head, Matrix.Translation((0, 0, -hz)))
    for part, ob in (("Feet", feet), ("Shaft", shaft), ("Head", head)):
        bpy.ops.object.select_all(action="DESELECT")
        export([ob], "SM_TR_%s_%s" % ("PylonLine" if name == "pylon_line" else "PylonCorner", part))
    kit[name] = {"height": round(h, 3), "feet": round(fz, 3), "shaft": round(hz - fz, 3), "head": round(h - hz, 3),
                 "saddle": round(saddle, 3), "saddle_over_head_base": round(saddle - hz, 3), "rope_axis": rope_axis,
                 "tris": n, "parts_tris": [tri_count(feet), tri_count(shaft), tri_count(head)]}


def corner(kit):
    """The corner pylon is a lattice A-frame: stretching it would bend its braces, so the game scales it whole (its
    height is the corner's rope height); the rope runs along X through the curved sheave at SADDLE of its height."""
    o = load("pylon_corner")
    lo, hi = bounds(o)
    s = 1.0 / (hi.z - lo.z)
    c = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
    transform(o, Matrix.Scale(s, 4) @ Matrix.Translation(-c))
    n = decimate(o, BUDGET["pylon_corner"])
    shrink_textures(TEX["pylon_corner"])
    export([o], "SM_TR_PylonCorner")
    lo, hi = bounds(o)
    kit["pylon_corner"] = {"height": 1.0, "saddle": 0.8, "size": [round(hi.x - lo.x, 3), round(hi.y - lo.y, 3)],
                           "rope_axis": "X", "tris": n}


def top(kit, name):
    o = load(name)
    lo, hi = bounds(o)
    s = D_TOP / max(hi.x - lo.x, hi.y - lo.y)
    c = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
    transform(o, Matrix.Scale(s, 4) @ Matrix.Translation(-c))
    lo, hi = bounds(o)
    n = decimate(o, BUDGET["top"])
    shrink_textures(TEX["top"])
    style = name[4:]
    export([o], "SM_TR_Top_" + style.capitalize())
    kit.setdefault("tops", {})[style] = {"size": [round(hi.x - lo.x, 3), round(hi.y - lo.y, 3), round(hi.z, 3)], "tris": n}


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    kit_path = os.path.join(OUT, "kit.json")
    kit = json.load(open(kit_path)) if os.path.isfile(kit_path) else {}
    names = argv or sorted(f[:-4] for f in os.listdir(GEN) if f.endswith(".glb"))
    for name in names:
        reset()
        if name == "cabin":
            cabin(kit)
        elif name == "pylon_line":
            pylon(kit, name, PYLON_ARM, "Y")
        elif name == "pylon_corner":
            corner(kit)
        elif name.startswith("top_"):
            top(kit, name)
        print("OPTIMIZE", name, json.dumps(kit.get(name) or kit.get("tops", {}).get(name[4:])))
    os.makedirs(OUT, exist_ok=True)
    kit["_doc"] = "Made by blender/transport/optimize.py: sizes of the game meshes (metres, Blender frame). Copied to Content/Transport/kit.json."
    json.dump(kit, open(kit_path, "w"), indent=1, sort_keys=True)


main()
