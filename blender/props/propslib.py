"""Shared helpers for the reusable Lyudmilovsk props (keyboards, computers, desks, chairs, small items).

Run from a generator script inside Blender:  blender -b -P blender/props/build_xxx.py
Units are metres. glTF export turns Blender Z-up into Y-up, as for the corridor.

Conventions (same as build_scene01_corridor.py):
  devices   origin on the table top under the device, screen faces local -Y (towards the chair), +X right
  desks     origin at the middle of the top surface (z = 0 is the top), floor at z = -height, chair side is -Y
  chairs    origin on the floor under the post, the sitter faces +Y, backrest at -Y
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
TEX_DIR = os.path.join(ROOT, "textures", "props")
EXPORT_DIR = os.path.join(HERE, "export")
RENDER_DIR = os.path.join(ROOT, "renders", "props")
FONT = "/Applications/Blender.app/Contents/Resources/5.3/datafiles/fonts/Inter.woff2"

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []


# ------------------------------------------------------------ scene / materials

def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scn = bpy.context.scene
    scn.unit_settings.system = "METRIC"
    return scn


def mat(name, color=(0.5, 0.5, 0.5), rough=0.5, metal=0.0, emit=None, strength=0.0, alpha=1.0, trans=0.0):
    m = bpy.data.materials.get(name)
    if m:
        return m
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    if emit is not None:
        b.inputs["Emission Color"].default_value = (*emit, 1)
        b.inputs["Emission Strength"].default_value = strength
    if trans:
        b.inputs["Transmission Weight"].default_value = trans
    if alpha < 1:
        b.inputs["Alpha"].default_value = alpha
        try:
            m.surface_render_method = "BLENDED"
        except AttributeError:
            pass
    return m


def led(name, color, strength=6.0):
    return mat("LED_" + name, (0, 0, 0), 1.0, emit=color, strength=strength)


def tex(name, file, rough=0.6, metal=0.0, size=1024, tint=None, fallback=(0.5, 0.5, 0.5), tilemult=1.0):
    """Image-textured material from textures/props/<file> (falls back to a flat colour if the file is missing)."""
    m = bpy.data.materials.get(name)
    if m:
        return m
    path = os.path.join(TEX_DIR, file)
    if not os.path.exists(path):
        print("WARN missing texture", path)
        return mat(name, fallback, rough, metal)
    m = bpy.data.materials.new(name)
    m["tile_mult"] = tilemult      # UV tile size multiplier (a bigger value = larger pattern on the object)
    m.use_nodes = True
    nt = m.node_tree
    b = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    t = nt.nodes.new("ShaderNodeTexImage")
    img = bpy.data.images.get(file)
    if img is None:
        img = bpy.data.images.load(path)
        img.name = file
        if size and img.size[0] > size:
            img.scale(size, size)
    t.image = img
    if tint:
        mix = nt.nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.blend_type = "MULTIPLY"
        mix.inputs["Factor"].default_value = 1.0
        mix.inputs["B"].default_value = (*tint, 1)
        nt.links.new(t.outputs["Color"], mix.inputs["A"])
        nt.links.new(mix.outputs["Result"], b.inputs["Base Color"])
    else:
        nt.links.new(t.outputs["Color"], b.inputs["Base Color"])
    return m


# ------------------------------------------------------------ mesh building

def _autosmooth(bm, angle=38):
    lim = math.radians(angle)
    for f in bm.faces:
        f.smooth = True
    for e in bm.edges:
        if len(e.link_faces) == 2 and e.calc_face_angle(0) > lim:
            e.smooth = False


def _uv_box(bm, tile):
    """Box-projected UVs in object space, `tile` metres = one texture repeat."""
    uv = bm.loops.layers.uv.verify()
    for f in bm.faces:
        n = f.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        a, b = [(1, 2), (0, 2), (0, 1)][ax]
        for l in f.loops:
            p = l.vert.co
            l[uv].uv = (p[a] / tile, p[b] / tile)


def _make(name, bm, material, parent, tile=0.5, smooth=38):
    if smooth is not None:
        _autosmooth(bm, smooth)
    _uv_box(bm, tile * (material.get("tile_mult", 1.0) if material is not None else 1.0))
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    if material is not None:
        me.materials.append(material)
    bpy.context.scene.collection.objects.link(obj)
    if parent:
        obj.parent = parent
    return obj


def place(obj, loc=(0, 0, 0), rot=(0, 0, 0)):
    obj.location = loc
    obj.rotation_euler = rot
    return obj


def empty(name, loc=(0, 0, 0), rot=(0, 0, 0), parent=None):
    e = bpy.data.objects.new(name, None)
    e.empty_display_size = 0.1
    e.location = loc
    e.rotation_euler = rot
    bpy.context.scene.collection.objects.link(e)
    if parent:
        e.parent = parent
    return e


def rbox(name, size, loc, material, parent=None, rot=(0, 0, 0), r=0.0, seg=3, tile=0.5, smooth=38):
    """Box with rounded edges (radius r)."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=Vector(size), verts=bm.verts)
    if r > 0:
        r = min(r, min(size) / 2 - 1e-5)
        bmesh.ops.bevel(bm, geom=list(bm.edges), offset=r, segments=seg, profile=0.5, affect="EDGES")
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return place(_make(name, bm, material, parent, tile, smooth), loc, rot)


def cylinder(name, r, h, loc, material, parent=None, rot=(0, 0, 0), seg=24, r2=None, bevel=0.0, tile=0.5, caps=True):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=caps, segments=seg, radius1=r, radius2=r if r2 is None else r2, depth=h)
    if bevel > 0:
        bmesh.ops.bevel(bm, geom=[e for e in bm.edges if len(e.link_faces) == 2 and
                                  e.calc_face_angle(0) > 0.5], offset=bevel, segments=2, affect="EDGES")
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return place(_make(name, bm, material, parent, tile, 40), loc, rot)


def blob(name, radii, loc, material, parent=None, rot=(0, 0, 0), seg=24, rings=14, tile=0.5):
    """Ellipsoid with semi-axes radii=(rx, ry, rz)."""
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=seg, v_segments=rings, radius=1.0)
    for v in bm.verts:
        v.co.x *= radii[0]
        v.co.y *= radii[1]
        v.co.z *= radii[2]
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return place(_make(name, bm, material, parent, tile, 80), loc, rot)


def lathe(name, profile, loc, material, parent=None, rot=(0, 0, 0), seg=32, tile=0.5, closed=False):
    """Surface of revolution about Z. profile = [(radius, z), ...] from bottom to top."""
    bm = bmesh.new()
    verts = [bm.verts.new((r, 0, z)) for r, z in profile]
    edges = [bm.edges.new((verts[i], verts[i + 1])) for i in range(len(verts) - 1)]
    bmesh.ops.spin(bm, geom=verts + edges, axis=(0, 0, 1), cent=(0, 0, 0), angle=2 * math.pi, steps=seg)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
    if closed:
        for end in (0, -1):
            if profile[end][0] > 1e-6:
                z = profile[end][1]
                ring = [v for v in bm.verts if abs(v.co.z - z) < 1e-6 and v.co.xy.length > 1e-6]
                if len(ring) > 2:
                    bmesh.ops.holes_fill(bm, edges=list({e for v in ring for e in v.link_edges
                                                         if all(abs(w.co.z - z) < 1e-6 for w in e.verts)}))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return place(_make(name, bm, material, parent, tile, 50), loc, rot)


def prism(name, pts, z0, z1, loc, material, parent=None, rot=(0, 0, 0), tile=0.5, smooth=38):
    """Extrude a convex-or-not 2D outline (list of (x, y)) from z0 to z1."""
    bm = bmesh.new()
    vs = [bm.verts.new((x, y, z0)) for x, y in pts]
    f = bm.faces.new(vs)
    res = bmesh.ops.extrude_face_region(bm, geom=[f])
    top = [v for v in res["geom"] if isinstance(v, bmesh.types.BMVert)]
    bmesh.ops.translate(bm, verts=top, vec=(0, 0, z1 - z0))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return place(_make(name, bm, material, parent, tile, smooth), loc, rot)


def tube(name, pts, r, material, parent=None, seg=8, smooth_pts=True, tile=0.5, cap=True):
    """Round tube along a polyline (Catmull-Rom smoothed). Points are in the parent's local frame."""
    path = [Vector(p) for p in pts]
    if smooth_pts and len(path) > 2:
        sm = []
        for i in range(len(path) - 1):
            p0 = path[max(i - 1, 0)]
            p1, p2 = path[i], path[i + 1]
            p3 = path[min(i + 2, len(path) - 1)]
            for s in range(6):
                t = s / 6
                sm.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t +
                                 (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3))
        sm.append(path[-1])
        path = sm
    bm = bmesh.new()
    rings = []
    prev_n = None
    for i, p in enumerate(path):
        d = (path[min(i + 1, len(path) - 1)] - path[max(i - 1, 0)]).normalized()
        up = Vector((0, 0, 1)) if abs(d.z) < 0.95 else Vector((1, 0, 0))
        a = d.cross(up).normalized()
        b = d.cross(a).normalized()
        rings.append([bm.verts.new(p + (a * math.cos(2 * math.pi * k / seg) + b * math.sin(2 * math.pi * k / seg)) * r)
                      for k in range(seg)])
    for i in range(len(rings) - 1):
        for k in range(seg):
            bm.faces.new((rings[i][k], rings[i][(k + 1) % seg], rings[i + 1][(k + 1) % seg], rings[i + 1][k]))
    if cap:
        bm.faces.new(rings[0][::-1])
        bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return _make(name, bm, material, parent, tile, 60)


def quad_screen(name, w, h, loc, material, parent=None, rot=(0, 0, 0)):
    """Flat screen quad facing local -Y, UV 0..1 (so a screen texture can be dropped in)."""
    bm = bmesh.new()
    hw, hh = w / 2, h / 2
    vs = [bm.verts.new(p) for p in [(-hw, 0, -hh), (hw, 0, -hh), (hw, 0, hh), (-hw, 0, hh)]]
    f = bm.faces.new(vs[::-1])      # normal -Y
    uv = bm.loops.layers.uv.verify()
    for l, c in zip(f.loops, [(0, 1), (1, 1), (1, 0), (0, 0)]):
        l[uv].uv = c
    me = bpy.data.meshes.new(name)
    bm.normal_update()
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    me.materials.append(material)
    bpy.context.scene.collection.objects.link(obj)
    if parent:
        obj.parent = parent
    return place(obj, loc, rot)


def text_mesh(name, label, size, loc, material, parent=None, rot=(0, 0, 0), extrude=0.0002, align="CENTER", font=None):
    """Flat text as real geometry (for keycap legends, labels)."""
    cu = bpy.data.curves.new(name, "FONT")
    cu.body = label
    cu.size = size
    cu.extrude = extrude
    cu.align_x = align
    cu.align_y = "CENTER"
    cu.resolution_u = 2
    f = bpy.data.fonts.get("legend")
    if f is None and os.path.exists(FONT):
        f = bpy.data.fonts.load(FONT)
        f.name = "legend"
    cu.font = f or cu.font
    ob = bpy.data.objects.new(name, cu)
    bpy.context.scene.collection.objects.link(ob)
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    bpy.data.objects.remove(ob)
    bpy.data.curves.remove(cu)
    out = bpy.data.objects.new(name, me)
    me.materials.append(material)
    bpy.context.scene.collection.objects.link(out)
    if parent:
        out.parent = parent
    return place(out, loc, rot)


# ------------------------------------------------------------ finishing

def merge_by_material(root, keep=("Screen_",), name=None):
    """Joins all mesh children of `root` into one mesh per material (and keeps parts whose name starts with `keep`).
    World transforms are baked into the vertices relative to `root`."""
    name = name or root.name
    kids = []

    def walk(o):
        for c in o.children:
            kids.append(c)
            walk(c)
    walk(root)
    groups = {}
    keepers = []
    for o in kids:
        if o.type != "MESH":
            continue
        if any(o.name.startswith(k) for k in keep):
            keepers.append(o)
            continue
        key = o.data.materials[0].name if o.data.materials else "none"
        groups.setdefault(key, []).append(o)
    inv = root.matrix_world.inverted()
    bpy.context.view_layer.update()
    made = []
    for key, objs in groups.items():
        bm = bmesh.new()
        uvs = bm.loops.layers.uv.verify()
        for o in objs:
            m = o.data.copy()
            m.transform(inv @ o.matrix_world)
            if (inv @ o.matrix_world).determinant() < 0:
                m.flip_normals()
            bm.from_mesh(m)
            bpy.data.meshes.remove(m)
        me = bpy.data.meshes.new(name + "_" + key)
        bm.to_mesh(me)
        bm.free()
        me.materials.append(objs[0].data.materials[0])
        for p in me.polygons:
            pass
        ob = bpy.data.objects.new(name + "_" + key, me)
        bpy.context.scene.collection.objects.link(ob)
        ob.parent = root
        made.append(ob)
    keep_names = {o.name for o in keepers}
    kid_names = [(o.name, o.type) for o in kids]
    inv_keep = {o.name: inv @ o.matrix_world for o in keepers}
    for nm, tp in kid_names:
        if tp == "MESH" and nm not in keep_names and nm in bpy.data.objects:
            bpy.data.objects.remove(bpy.data.objects[nm])
    for nm, mw in inv_keep.items():     # re-bake the parent chain to root
        o = bpy.data.objects[nm]
        o.parent = root
        o.matrix_parent_inverse = Matrix.Identity(4)
        o.matrix_basis = mw
    for nm, tp in kid_names:
        if tp == "EMPTY" and nm in bpy.data.objects:
            bpy.data.objects.remove(bpy.data.objects[nm])
    return made


def stats(root):
    tris = 0
    n = 0
    for o in bpy.data.objects:
        if o.type == "MESH":
            n += 1
            tris += sum(len(p.vertices) - 2 for p in o.data.polygons)
    return n, tris


def setup_studio(scn, res=(900, 700)):
    scn.render.engine = "CYCLES"
    scn.cycles.samples = 24
    scn.cycles.use_denoising = True
    scn.render.resolution_x, scn.render.resolution_y = res
    w = bpy.data.worlds.new("W")
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs["Color"].default_value = (0.42, 0.42, 0.44, 1)
    w.node_tree.nodes["Background"].inputs["Strength"].default_value = 1.0
    scn.world = w
    ground = rbox("_ground", (6, 6, 0.01), (0, 0, -0.005), mat("_g", (0.35, 0.35, 0.35), 0.9))
    for loc, e in (((1.5, -2.5, 2.5), 250), ((-2.5, -1.0, 1.5), 90)):
        ld = bpy.data.lights.new("L", "AREA")
        ld.energy = e
        ld.size = 1.5
        lo = bpy.data.objects.new("L", ld)
        bpy.context.scene.collection.objects.link(lo)
        lo.location = loc
        d = Vector((0, 0, 0.2)) - Vector(loc)
        lo.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    return ground


def render(scn, path, target=(0, 0, 0.15), loc=(0.8, -1.2, 0.7), lens=60):
    cam = bpy.data.cameras.new("C")
    cam.lens = lens
    co = bpy.data.objects.new("C", cam)
    bpy.context.scene.collection.objects.link(co)
    co.location = loc
    co.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    scn.camera = co
    os.makedirs(os.path.dirname(path), exist_ok=True)
    scn.render.filepath = path
    bpy.ops.render.render(write_still=True)


def export(root, name, blend=True):
    """Saves blender/props/<name>.blend and exports blender/props/export/<name>.glb for the root and its children."""
    os.makedirs(EXPORT_DIR, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    sel = [root] + [o for o in bpy.data.objects if o.parent == root or (o.parent and o.parent.parent == root)]
    for o in sel:
        o.select_set(True)
    bpy.context.view_layer.objects.active = root
    bpy.ops.export_scene.gltf(filepath=os.path.join(EXPORT_DIR, name + ".glb"), export_format="GLB",
                              use_selection=True, export_apply=True, export_image_format="JPEG",
                              export_jpeg_quality=88, export_cameras=False, export_lights=False)
    if blend:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, name + ".blend"), compress=True)
