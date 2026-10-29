"""Maru's collar (Dima's photo, blender/animals/gen/Maru/ref/maru_collar.jpg): a thin braided cord round the neck,
pulled together under the throat by a sliding knot, a steel split ring hanging from the knot and on it a polished
brass bone-shaped tag engraved with her name.

Built on the finished body (run after build_animal.py <Breed> [--gen]): the cord follows the neck's skin, measured by
rays in a plane across the neck between neck_01 and neck_02. Two meshes, because the tag swings on its own in the game
(AAnimalActor's collar, Private/Animals/AnimalCollar.cpp):

  export/<Breed>/<Breed>_Collar.glb   the cord and the knot, in the breed's frame (moves with the neck bone)
  export/<Breed>/<Breed>_Tag.glb      the split ring, the tag's eye and the tag, round the point it hangs from
  export/<Breed>/<Breed>_collar.json  the bone, the hanging point, the neck (for the tag to rest against), in UE cm

The engraved word comes from the localization table (label.maru.tag in strings_ru.json: the tag is engraved in
Russian in every language, it's her real tag).

    blender -b -P blender/animals/collar.py -- Maru [--preview]
"""
import json
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
BREED = argv[0] if argv else "Maru"
PREVIEW = "--preview" in argv
OUT = os.path.join(ROOT, "blender", "animals", "export", BREED)
STRINGS = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Localization", "strings_ru.json")

# sizes, metres (the photo: a ~4 mm cord, a ~2 cm ring, a ~3 cm tag)
CORD_R = 0.0026
CORD_SINK = 0.026        # the cord's centre over the skin: on the fur, pressing it down a little
# a body with no fur shells (rig.json "fur" false: Maru kept as hi3d made her, the fur only painted): the cord lies on
# the skin, and hangs a little loose under the throat (2.6 cm off it left it floating, a perfect hoop, 2026-10-29)
FURLESS_SAG = 0.007
SPLIT_R, SPLIT_W = 0.0095, 0.0011
TAG_W, TAG_H, TAG_T = 0.032, 0.018, 0.0018
ENGRAVE = 0.00028


def log(*a):
    print("COLLAR", BREED, *a, flush=True)


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def to_ue(p):
    return [round(float(p[0]) * 100, 3), round(float(-p[1]) * 100, 3), round(float(p[2]) * 100, 3)]


def material(name, color, metal, rough):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (*color, 1.0)
    b.inputs["Metallic"].default_value = metal
    b.inputs["Roughness"].default_value = rough
    return m


def mesh_object(name, bm):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


# ------------------------------------------------------------------ the body
def load_body():
    bpy.ops.import_scene.gltf(filepath=os.path.join(OUT, BREED + ".glb"))
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    lods = sorted([o for o in bpy.data.objects if o.type == "MESH"], key=lambda o: o.name)
    # the finest LOD's skin (its Body faces: the fur shells have their own material)
    skin = next(o for o in lods if "LOD0" in o.name)
    for o in lods:
        if o is not skin:
            bpy.data.objects.remove(o)
    me = skin.data
    M = skin.matrix_world
    body_slot = [i for i, m in enumerate(me.materials) if m and "Fur" not in m.name]
    verts = [M @ v.co for v in me.vertices]
    polys = [tuple(p.vertices) for p in me.polygons if p.material_index in body_slot]
    # the preview shows the skin only
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.material_index not in body_slot], context="FACES")
    bm.to_mesh(me)
    bm.free()
    bvh = BVHTree.FromPolygons(verts, polys)
    bones = {b.name: (arm.matrix_world @ b.head_local, arm.matrix_world @ b.tail_local) for b in arm.data.bones}
    return arm, skin, bvh, bones


def neck_ring(bvh, bones, n=72):
    """Points round the neck where the collar lies (a plane a little below the middle of the neck, leaning so the
    cord sits lower at the throat than at the nape), their outward directions, and the frame."""
    n1 = Vector(bones["neck_01"][0])
    n2 = Vector(bones["neck_02"][0])
    c = n1.lerp(n2, 0.42)
    axis = (n2 - n1).normalized()
    # lean: the front of the plane drops (a collar slides down the throat)
    side = Vector((0, 1, 0))
    fwd = side.cross(axis).normalized()      # perpendicular to the neck, pointing to the throat/chest side
    if fwd.z > 0:
        fwd = -fwd
    axis = (Matrix.Rotation(math.radians(-14), 3, side) @ axis).normalized()
    fwd = side.cross(axis).normalized()
    if fwd.z > 0:
        fwd = -fwd
    pts, outs, rs = [], [], []
    for k in range(n):
        t = 2 * math.pi * k / n
        d = (fwd * math.cos(t) + side * math.sin(t)).normalized()
        start = c + d * 0.4
        hit = bvh.ray_cast(start, -d, 0.45)
        r = (hit[0] - c).length if hit[0] is not None else 0.08
        rs.append(r)
        outs.append(d)
    # a light smoothing of the radii (decimation leaves facets)
    rs = np.array(rs)
    rs = np.convolve(np.r_[rs[-2:], rs, rs[:2]], np.ones(5) / 5, mode="valid")
    rig = json.load(open(os.path.join(OUT, BREED + "_rig.json")))
    for k in range(n):
        if rig.get("fur", True):
            gap = CORD_SINK
        else:
            # on the skin at the nape and the sides, sagging away from it toward the throat (the knot's weight)
            gap = CORD_R * 0.7 + FURLESS_SAG * max(0.0, outs[k].dot(fwd)) ** 2
        pts.append(c + outs[k] * (rs[k] + gap))
    return c, axis, fwd, side, pts, outs, rs


# ------------------------------------------------------------------ the cord
def tube(bm, path, r, closed, colors, sides=8, twist=0.0):
    """A braided-looking tube along a path: alternating dark red and black strands as vertex colours spiralling round."""
    col = bm.loops.layers.color.verify()
    rings = []
    N = len(path)
    for i in range(N):
        p = path[i]
        a = path[(i + 1) % N] if closed or i + 1 < N else path[i]
        b = path[i - 1] if closed or i > 0 else path[i]
        t = (a - b).normalized()
        u = t.orthogonal().normalized()
        if i > 0:
            # parallel transport of the frame
            prev_u = rings[-1][1]
            u = (prev_u - t * prev_u.dot(t)).normalized()
        w = t.cross(u)
        ring = []
        for s in range(sides):
            ang = 2 * math.pi * s / sides
            ring.append(bm.verts.new(p + (u * math.cos(ang) + w * math.sin(ang)) * r))
        rings.append((ring, u))
    bm.verts.ensure_lookup_table()
    segs = N if closed else N - 1
    for i in range(segs):
        r0, r1 = rings[i][0], rings[(i + 1) % N][0]
        for s in range(sides):
            f = bm.faces.new((r0[s], r0[(s + 1) % sides], r1[(s + 1) % sides], r1[s]))
            # a braid: diagonal strands, two colours
            strand = (s + int(i * 1.0 + twist)) % 4 < 2
            for l in f.loops:
                l[col] = colors[0] if strand else colors[1]
    if not closed:
        for ring, _ in (rings[0], rings[-1]):
            bm.faces.new(ring if ring is rings[-1][0] else list(reversed(ring)))
    return bm


def build_cord(c, axis, fwd, side, pts, outs):
    """The loop round the neck, both sides running down into the knot under the throat."""
    n = len(pts)
    # the knot sits at the front-bottom of the loop, a little off the skin
    k_front = 0                                     # angle 0 = fwd (the throat side)
    knot = pts[k_front] + outs[k_front] * 0.008
    path = []
    for i in range(n):
        t = i / n
        # near the knot the two sides draw together into it
        d = min(t, 1 - t) * n
        p = Vector(pts[i])
        if d < 6:
            p = p.lerp(knot, (1 - d / 6) ** 2 * 0.9)
        path.append(p)
    red = (1.0, 0.85, 0.85, 1.0)        # the strands as tints of the cord material's dark maroon
    black = (0.22, 0.2, 0.2, 1.0)
    bm = bmesh.new()
    tube(bm, path, CORD_R, True, (red, black))
    # the knot: a fat little tangle of cord (a short torus knot)
    kp = []
    for i in range(48):
        t = 2 * math.pi * i / 48
        rr = 0.0042 + 0.0018 * math.cos(3 * t)
        x, y, z = rr * math.cos(2 * t), rr * math.sin(2 * t), 0.0018 * math.sin(3 * t)
        kp.append(knot + outs[k_front] * z + side * x + axis * y)
    tube(bm, kp, CORD_R * 0.95, True, (red, black), sides=6, twist=1)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    ob = mesh_object(BREED + "_Collar", bm)
    ob.data.materials.append(material("Cord", (0.12, 0.03, 0.03), 0.0, 0.85))
    for p in ob.data.polygons:
        p.use_smooth = True
    # the point the split ring hangs from: the knot's bottom
    hang = knot - axis * 0.0 + Vector((0, 0, -0.0055))
    return ob, knot, hang


# ------------------------------------------------------------------ the tag
def bone_outline(seg=28):
    """The bone tag's outline (centred, x along, y up, counter-clockwise), metres: a straight shaft and two round lobes
    at each end, the lobes of an end overlapping (a notch between them)."""
    W, H = TAG_W, TAG_H
    r = H * 0.28
    cx, cy = W / 2 - r, H / 2 - r
    sh = H * 0.2                      # half the shaft's height (below the lobes' centres)
    xa = cx - math.sqrt(r * r - (cy - sh) ** 2)
    xn = cx + math.sqrt(r * r - cy * cy)
    out = []

    def arc(ccx, ccy, a0, a1):
        for i in range(seg + 1):
            t = a0 + (a1 - a0) * i / seg
            out.append((ccx + r * math.cos(t), ccy + r * math.sin(t)))
    # the right end: from the shaft's bottom edge round the lower lobe, the notch, round the upper lobe to the top edge
    arc(cx, -cy, math.atan2(-sh + cy, xa - cx) - 2 * math.pi, math.atan2(cy, xn - cx))
    arc(cx, cy, math.atan2(-cy, xn - cx), math.atan2(sh - cy, xa - cx) + 2 * math.pi if math.atan2(sh - cy, xa - cx) < 0 else math.atan2(sh - cy, xa - cx))
    # the left end, mirrored
    right = list(out)
    left = [(-x, -y) for x, y in right]
    return right + left


def engraved_word():
    try:
        return json.load(open(STRINGS, encoding="utf-8"))["label.maru.tag"]
    except (OSError, KeyError):
        return "Мару"


def build_tag():
    """The split ring (its top at the origin), a little eye and the tag below it, the tag's face toward +X (away from
    the chest when she stands: the game turns it), engraved."""
    gold = material("Gold", (1.0, 0.72, 0.32), 1.0, 0.22)
    engraved = material("Engraved", (0.42, 0.27, 0.10), 1.0, 0.55)
    steel = material("Steel", (0.78, 0.78, 0.8), 1.0, 0.28)
    # the tag body: outline in the y-z plane (its face looks along +x), extruded through x
    outline = bone_outline()
    top = SPLIT_R * 2 + 0.0035                 # from the ring's top down to the tag's top edge
    zc = -(top + TAG_H / 2)
    bm = bmesh.new()
    front = [bm.verts.new((TAG_T / 2, u, zc + v)) for u, v in outline]
    back = [bm.verts.new((-TAG_T / 2, u, zc + v)) for u, v in outline]
    ff = bm.faces.new(front)
    bf = bm.faces.new(list(reversed(back)))
    n = len(outline)
    for i in range(n):
        bm.faces.new((front[i], back[i], back[(i + 1) % n], front[(i + 1) % n]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    # a little bevel round the edges so it catches the light like a pressed tag
    edges = [e for e in bm.edges if len(e.link_faces) == 2 and abs(e.link_faces[0].normal.x) > 0.9 or
             len(e.link_faces) == 2 and abs(e.link_faces[1].normal.x) > 0.9]
    bmesh.ops.bevel(bm, geom=edges, offset=TAG_T * 0.3, segments=2, affect="EDGES", profile=0.5)
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4])
    tag = mesh_object("Tag", bm)
    tag.data.materials.append(gold)
    tag.data.materials.append(engraved)
    # the name: text cut into the face
    bpy.ops.object.text_add(location=(0, 0, 0))
    txt = bpy.context.object
    txt.data.body = engraved_word()
    try:
        # Blender's own Inter (SIL OFL) has Cyrillic; its built-in font has not
        txt.data.font = bpy.data.fonts.load(os.path.join(os.path.dirname(bpy.app.binary_path), "..", "Resources",
                                                         "%d.%d" % bpy.app.version[:2], "datafiles", "fonts", "Inter.woff2"))
    except RuntimeError as e:
        log("no Inter font", e)
    txt.data.align_x = "CENTER"
    txt.data.align_y = "CENTER"
    txt.data.size = TAG_H * 0.46
    txt.data.extrude = ENGRAVE * 2
    txt.data.resolution_u = 4
    txt.rotation_euler = (math.pi / 2, 0, math.pi / 2)       # lying in the y-z plane, read from +x
    txt.location = (TAG_T / 2, 0, zc)
    bpy.ops.object.convert(target="MESH")
    txt = bpy.context.object
    # the word must fit inside the shaft and lobes
    dims = txt.dimensions
    # the word must lie on the shaft (it may reach a little into the lobes)
    fit = min(1.0, (TAG_W * 0.6) / max(dims.x, dims.y, 1e-6), (TAG_H * 0.33) / max(min(dims.y, dims.z) if dims.y < dims.x else dims.z, 1e-6))
    txt.scale = (1.0, fit, fit) if dims.y > dims.x else (fit, fit, 1.0)
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    mod = tag.modifiers.new("Engrave", "BOOLEAN")
    mod.operation = "DIFFERENCE"
    mod.solver = "EXACT"
    mod.use_self = True               # a variable font's glyphs are drawn with overlapping strokes (the М)
    mod.use_hole_tolerant = True
    mod.object = txt
    bpy.context.view_layer.objects.active = tag
    bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.data.objects.remove(txt)
    # faces of the cut (inside the face plane) get the engraved material
    for p in tag.data.polygons:
        if p.center.x < TAG_T / 2 - ENGRAVE * 0.3 and abs(p.normal.x) < 0.98 or (p.normal.x > 0.9 and p.center.x < TAG_T / 2 - ENGRAVE * 0.5):
            p.material_index = 1
    # the split ring: two turns of flat wire, hanging in the y-z plane (its top at the origin)
    bm = bmesh.new()
    pts = []
    for i in range(96):
        t = 2 * math.pi * i / 48
        off = (i / 96.0 - 0.5) * SPLIT_W * 1.8
        pts.append(Vector((off, SPLIT_R * math.sin(t), -SPLIT_R + SPLIT_R * math.cos(t))))
    tube(bm, pts, SPLIT_W * 0.6, False, ((1, 1, 1, 1), (1, 1, 1, 1)), sides=6)
    # the tag's eye: a small ring through the hole at the top of the tag, hanging in the split ring
    eye_c = Vector((0, 0, -SPLIT_R * 2 + 0.0004 - 0.0018))
    epts = [eye_c + Vector((0.0018 * math.cos(2 * math.pi * i / 24), 0, 0.0018 * math.sin(2 * math.pi * i / 24) - 0.0006)) for i in range(24)]
    tube(bm, epts, 0.0004, True, ((1, 1, 1, 1), (1, 1, 1, 1)), sides=5)
    ring = mesh_object("Ring", bm)
    ring.data.materials.append(steel)
    # one object, three slots: Steel, Gold, Engraved
    bpy.ops.object.select_all(action="DESELECT")
    ring.select_set(True)
    tag.select_set(True)
    bpy.context.view_layer.objects.active = ring
    bpy.ops.object.join()
    ob = bpy.context.object
    ob.name = BREED + "_Tag"
    for p in ob.data.polygons:
        p.use_smooth = p.material_index == 0      # the wire round, the stamped tag flat-faced
    # the tag's centre of mass below the hanging point (for the pendulum)
    com = zc * 0.85
    return ob, -com


def export(ob, path):
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True, export_yup=True, export_apply=True,
                              export_normals=True, export_materials="EXPORT", export_vertex_color="ACTIVE")
    tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
    log("exported", os.path.basename(path), tris, "tris")
    return tris


def preview(collar, tag_ob, hang, skin):
    """A close render of the neck with the collar and the tag hung in place (export/<Breed>/collar_preview.png)."""
    t = tag_ob.copy()
    t.data = tag_ob.data.copy()
    bpy.context.scene.collection.objects.link(t)
    t.location = hang
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE"
    sc.render.resolution_x, sc.render.resolution_y = 900, 900
    world = bpy.data.worlds.new("W")
    world.color = (0.6, 0.6, 0.62)
    sc.world = world
    skin.data.materials.clear()
    skin.data.materials.append(material("Fur", (0.85, 0.78, 0.68), 0.0, 0.9))
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    target = Vector(hang) + Vector((-0.03, 0, 0.04))
    cam.location = target + Vector((0.42, -0.36, 0.06))
    cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = 60
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 4
    sun.rotation_euler = (math.radians(50), 0, math.radians(30))
    sc.collection.objects.link(sun)
    sc.render.filepath = os.path.join(OUT, "collar_preview.png")
    bpy.ops.render.render(write_still=True)
    log("preview", sc.render.filepath)
    # the tag face on, close (its face looks along +x)
    tc = t.location + Vector((0, 0, -(SPLIT_R * 2 + 0.0035 + TAG_H / 2)))
    cam.location = tc + Vector((0.09, 0, 0))
    cam.rotation_euler = (tc - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = 85
    cam.data.clip_start = 0.005
    skin.hide_render = True
    collar.hide_render = True
    sc.render.filepath = os.path.join(OUT, "tag_preview.png")
    bpy.ops.render.render(write_still=True)
    skin.hide_render = False
    collar.hide_render = False
    log("preview", sc.render.filepath)


def main():
    reset()
    arm, skin, bvh, bones = load_body()
    c, axis, fwd, side, pts, outs, rs = neck_ring(bvh, bones)
    log("neck radius %.1f..%.1f cm" % (min(rs) * 100, max(rs) * 100))
    collar, knot, hang = build_cord(c, axis, fwd, side, pts, outs)
    tag, com = build_tag()
    if PREVIEW:
        preview(collar, tag, hang, skin)
    tris = export(collar, os.path.join(OUT, BREED + "_Collar.glb"))
    tris += export(tag, os.path.join(OUT, BREED + "_Tag.glb"))
    # the neck as the tag sees it: a sphere it rests against (the chest below the throat)
    chest_c = c - fwd * float(np.mean(rs)) * 0.25 - axis * 0.05
    chest_r = float((Vector(hang) - chest_c).length) - 0.004
    info = {"breed": BREED, "bone": "neck_01", "hang": to_ue(hang), "com_cm": round(com * 100, 3),
            "chest_center": to_ue(chest_c), "chest_radius_cm": round(chest_r * 100, 2),
            "forward": to_ue(fwd * 0.01), "tris": tris}
    json.dump(info, open(os.path.join(OUT, BREED + "_collar.json"), "w"), indent=1)
    log("info", info)


main()
