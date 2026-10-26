"""Turns the raw Hi3D models of the cable-car metro (design/transport/gen/<name>.glb, ~300k triangles and two 8k
textures each) into game meshes: decimated to a budget, textures cut to 1-2k, set in the game's frame and size, the
cabin made a room (cabin.py), the pylons split into feet / shaft / head so the game can stretch only the shaft.
Writes design/transport/game/<part>.glb and design/transport/game/kit.json (sizes the game reads, copied to
Content/Transport/kit.json).

  Blender -b -P blender/transport/optimize.py [-- name ...]

Frame of the output (Blender, Z up; Unreal gets (x, -y, z)): metres; origin on the base's centre.
  cabin: long side along X, the door on +Y (Unreal: left of the way it goes, -Y), origin on the floor under the grip
  (cabin.py: shell, inside, hanger, two door leaves).
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
        if img.type != "IMAGE":
            continue
        if img.size[0] > size:
            img.scale(size, size)
        img.pack()


GLASS_DARK = 0.32        # a texel this dark (and not a saturated colour) is a window
GLASS_SAT = 0.45


def _texels(o):
    """The base colour texture of o's first material as (w, h, flat RGBA list)."""
    for m in o.data.materials:
        if not m or not m.use_nodes:
            continue
        for n in m.node_tree.nodes:
            if n.type == "BSDF_PRINCIPLED":
                link = n.inputs["Base Color"].links
                if link and link[0].from_node.type == "TEX_IMAGE" and link[0].from_node.image:
                    img = link[0].from_node.image
                    return img.size[0], img.size[1], list(img.pixels[:])
    return 0, 0, None


def glass_and_rooms(o, floor_z=None, interior=(0.78, 0.74, 0.66), amber=False, dark=GLASS_DARK, dull=GLASS_SAT, zmax=1e9):
    """Windows out of the texture: faces whose colour is dark and dull become a second material TR_Glass (the game makes
    it real glass); then a room inside following the model: a copy of the shell, its faces turned inward and drawn a
    little in, without the windows, in plain plaster (TR_Interior), and a floor at floor_z. Returns (glass faces, room faces)."""
    w, h, px = _texels(o)
    if not px:
        return 0, 0
    me = o.data
    uv = me.uv_layers.active.data
    glass = bpy.data.materials.new("TR_Glass")
    glass.diffuse_color = (0.3, 0.4, 0.45, 0.25)
    room = bpy.data.materials.new("TR_Interior")
    room.diffuse_color = interior + (1.0,)
    me.materials.append(glass)
    me.materials.append(room)
    gi, ri = len(me.materials) - 2, len(me.materials) - 1
    n = 0
    for poly in me.polygons:
        u = sum(uv[l].uv[0] for l in poly.loop_indices) / poly.loop_total
        v = sum(uv[l].uv[1] for l in poly.loop_indices) / poly.loop_total
        x, y = min(w - 1, max(0, int(u % 1.0 * w))), min(h - 1, max(0, int(v % 1.0 * h)))
        k = (y * w + x) * 4
        r, g, b = px[k], px[k + 1], px[k + 2]
        hi, lo = max(r, g, b), min(r, g, b)
        sat = (hi - lo) / hi if hi > 1e-4 else 0.0
        # amber glass (the lantern station): a bright, strongly orange-yellow texel
        if poly.center.z > zmax:
            continue
        if (hi < dark and sat < dull) or (amber and sat > 0.55 and hi > 0.55 and r >= g > b):
            poly.material_index = gi
            n += 1
    # the room: the shell without its windows, inward
    bm = bmesh.new()
    bm.from_mesh(me)
    keep = [f for f in bm.faces if f.material_index != gi]
    inner = bmesh.ops.duplicate(bm, geom=keep)
    faces = [e for e in inner["geom"] if isinstance(e, bmesh.types.BMFace)]
    verts = list({v for f in faces for v in f.verts})
    c = sum((v.co for v in verts), Vector()) / max(1, len(verts))
    for v in verts:
        d = v.co - c
        v.co = c + Vector((d.x * 0.985, d.y * 0.985, d.z * 0.985 if v.co.z > c.z else d.z))
    bmesh.ops.reverse_faces(bm, faces=faces)
    for f in faces:
        f.material_index = ri
    if floor_z is not None:
        lo, hi = bounds(o)
        r0 = min(hi.x - lo.x, hi.y - lo.y) * 0.45
        ring = [bm.verts.new((c.x + r0 * math.cos(a), c.y + r0 * math.sin(a), floor_z)) for a in
                [2 * math.pi * i / 24 for i in range(24)]]
        fl = bm.faces.new(ring)
        fl.material_index = ri
        fl.normal_update()
        if fl.normal.z < 0:
            fl.normal_flip()
        faces.append(fl)
    bm.to_mesh(me)
    bm.free()
    return n, len(faces)


def debug_render(o, name):
    """TR_DEBUG=<dir>: a workbench picture of o with the windows red and the room blue (to check the glass split)."""
    out = os.environ.get("TR_DEBUG")
    if not out:
        return
    for m in o.data.materials:
        if m and m.name.startswith("TR_Glass"):
            m.diffuse_color = (1, 0, 0, 1)
        elif m and m.name.startswith("TR_Interior"):
            m.diffuse_color = (0, 0.3, 1, 1)
        elif m:
            m.diffuse_color = (0.8, 0.8, 0.8, 1)
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.display.shading.color_type = "MATERIAL"
    sc.render.resolution_x = sc.render.resolution_y = 600
    cam = bpy.data.objects.new("dbgcam", bpy.data.cameras.new("dbgcam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    lo, hi = bounds(o)
    c = (lo + hi) / 2
    d = max(hi - lo)
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = d * 1.2
    for k, v in (("a", Vector((1, -1, 0.6))), ("b", Vector((-1, 1, 0.6)))):
        cam.location = c + v.normalized() * d * 3
        cam.rotation_euler = (-v).to_track_quat("-Z", "Y").to_euler()
        sc.render.filepath = os.path.join(out, "%s_%s.png" % (name, k))
        bpy.ops.render.render(write_still=True)
    for m in o.data.materials:
        if m:
            m.diffuse_color = (0.8, 0.8, 0.8, 1)


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
    """The cabin as a room people ride in: blender/transport/cabin.py."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import cabin as cab
    o = load("cabin")
    parts, entry = cab.build(o, {"transform": transform, "bounds": bounds, "decimate": decimate, "texels": _texels})
    shell = parts["SM_TR_Cabin"]
    debug_render(shell, "cabin")
    shrink_textures(TEX["cabin"])
    for name, ob in parts.items():
        export([ob], name)
    if os.environ.get("TR_DEBUG"):
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(os.environ["TR_DEBUG"], "cabin.blend"))
    kit["cabin"] = entry


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
    g, r = glass_and_rooms(o, floor_z=0.6, amber=name == "top_lantern")
    debug_render(o, name)
    shrink_textures(TEX["top"])
    style = name[4:]
    export([o], "SM_TR_Top_" + style.capitalize())
    kit.setdefault("tops", {})[style] = {"size": [round(hi.x - lo.x, 3), round(hi.y - lo.y, 3), round(hi.z, 3)], "tris": n,
                                         "glass_faces": g, "room_faces": r}


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
