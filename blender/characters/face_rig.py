"""Eyes that blink and look about, and a mouth that opens, on a generated character (rig_character.py calls build()).

The generated models (hi3d) are one textured shell: the eyes and the lips are painted on it. Cutting the painted eye
out for a real eyeball was tried and looked like a ping-pong ball in a hole (the openings the pictures paint are big,
the shell has no lids). So the paint stays and moves:

  1. Landmarks: the face is rendered front-on, unlit, and face_marks.py (numpy/scipy, run by the system python) finds
     the irises, the eye openings and the lip line (blender/characters/<id>/face_marks.json, debug picture
     renders/characters/<id>_face.png). character.json "face": {"eyes": false} leaves the eyes alone (glasses),
     "marks": {...} overrides what was found.
  2. The face round the eyes and the mouth is subdivided twice (the 24k-triangle budget leaves an eye ~10 vertices).
  3. Shape keys (morph targets in UE, driven by ULyudBodyAnimInstance):
       eyeBlinkLeft/Right  the painted eye is squeezed down into the lower lid line and the upper lid's skin
                           follows it down, so the lashes close over it
       jawOpen             the lower lip and chin turn about a hinge under the ears; the face parts along the lip
                           line and the painted line between the lips stretches into the open mouth
       mouthFunnel (o, u), mouthSmile (e, i, a smile), browUp
  4. Looking about: face_marks.json "uv" holds each eye's opening in texture space (its middle and the 2x2 map
     between face metres and UV); M_HumanSkin shifts the texture inside the opening (EyeL/EyeR parameters, the
     shift faded to nothing at the lids), so the painted iris slides the way the eye turns.
"""
import json
import math
import os
import subprocess

import bpy
import bmesh
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree
from mathutils.interpolate import poly_3d_calc

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
SHOT = 0.2                                        # metres across the face picture


def log(m):
    print("FACE " + m, flush=True)


# ---------------------------------------------------------------------------------------------- landmarks

def face_shot(model, arm, path):
    """Front orthographic picture of the face, base colour only; returns the picture's centre height."""
    eye = (arm.matrix_world @ arm.data.bones["eye_l"].head_local + arm.matrix_world @ arm.data.bones["eye_r"].head_local) / 2
    sc = bpy.context.scene
    keep = (sc.render.engine, sc.render.resolution_x, sc.render.resolution_y, sc.camera, sc.view_settings.view_transform,
            sc.render.filepath)
    hidden = [o for o in bpy.data.objects if o.type == "MESH" and o != model and not o.hide_render]
    for o in hidden:
        o.hide_render = True
    sc.render.engine = "BLENDER_EEVEE"
    sc.render.resolution_x = sc.render.resolution_y = 1024
    sc.view_settings.view_transform = "Standard"
    nt = model.material_slots[0].material.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    base = next(l.from_node for l in nt.links if l.to_socket == bsdf.inputs["Base Color"])
    outn = next(n for n in nt.nodes if n.type == "OUTPUT_MATERIAL")
    was = next(l.from_socket for l in nt.links if l.to_socket == outn.inputs["Surface"])
    em = nt.nodes.new("ShaderNodeEmission")
    nt.links.new(base.outputs["Color"], em.inputs["Color"])
    nt.links.new(em.outputs["Emission"], outn.inputs["Surface"])
    cam = bpy.data.objects.new("facecam", bpy.data.cameras.new("facecam"))
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = SHOT
    sc.collection.objects.link(cam)
    sc.camera = cam
    cz = eye.z + 0.005
    cam.location = (0.0, -1.0, cz)
    cam.rotation_euler = (math.radians(90), 0, 0)
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    nt.links.new(was, outn.inputs["Surface"])
    nt.nodes.remove(em)
    bpy.data.objects.remove(cam)
    for o in hidden:
        o.hide_render = False
    sc.render.engine, sc.render.resolution_x, sc.render.resolution_y, sc.camera, sc.view_settings.view_transform, \
        sc.render.filepath = keep
    return cz


def landmarks(model, arm, cid, spec):
    face = spec.get("face", {})
    os.makedirs(os.path.join(ROOT, "renders", "characters"), exist_ok=True)
    shot = os.path.join(ROOT, "Saved", "FaceShots", cid + ".png")
    os.makedirs(os.path.dirname(shot), exist_ok=True)
    cz = face_shot(model, arm, shot)
    dbg = os.path.join(ROOT, "renders", "characters", cid + "_face.png")
    out = subprocess.run(["python3", os.path.join(HERE, "face_marks.py"), shot, "%.5f" % cz, str(SHOT), dbg],
                         capture_output=True, text=True)
    if out.returncode != 0:
        raise RuntimeError("face_marks failed: " + out.stderr[-800:])
    marks = json.loads(out.stdout.strip().splitlines()[-1])
    marks.update(face.get("marks", {}))
    json.dump(marks, open(os.path.join(HERE, cid, "face_marks.json"), "w"), indent=1)
    log("marks %s" % marks)
    return marks


# ---------------------------------------------------------------------------------------------- mesh helpers

def front_hit(tree, x, z):
    """Where a ray from the front (Blender -Y, the face's side) meets the model at x, z."""
    loc, nor, idx, d = tree.ray_cast(Vector((x, -2.0, z)), Vector((0, 1, 0)))
    return loc, idx


def uv_at(me, bm_faces_uv, tree, x, z):
    loc, idx = front_hit(tree, x, z)
    if loc is None:
        return None
    co, uvs = bm_faces_uv[idx]
    w = poly_3d_calc(co, loc)
    return sum((u * wi for u, wi in zip(uvs, w)), Vector((0, 0)))


def faces_uv(me):
    uv = me.uv_layers.active.data
    out = []
    for p in me.polygons:
        out.append(([me.vertices[v].co.copy() for v in p.vertices], [uv[l].uv.copy() for l in p.loop_indices]))
    return out


def subdivide_near(model, centres, front_y, levels=2, smooth=0.0):
    """Subdivides (`levels` times) the front faces whose centre is near any of `centres` [(x, z, rx, rz)]; smooth > 0
    rounds the new vertices onto the surface's curve (the face's low-poly facets)."""
    for _ in range(levels):
        bm = bmesh.new()
        bm.from_mesh(model.data)
        sel = set()
        for f in bm.faces:
            c = f.calc_center_median()
            if c.y > front_y(c.x, c.z):
                continue
            for (x, z, rx, rz) in centres:
                if ((c.x - x) / rx) ** 2 + ((c.z - z) / rz) ** 2 < 1.0:
                    for e in f.edges:
                        sel.add(e)
                    break
        bmesh.ops.subdivide_edges(bm, edges=list(sel), cuts=1, use_grid_fill=True, use_single_edge=False, smooth=smooth)
        bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4])
        bm.to_mesh(model.data)
        bm.free()
    model.data.update()


# ---------------------------------------------------------------------------------------------- eyes

def eye_uv(me, fuv, tree, e):
    """The eye opening in texture space: UV at the iris and d(UV)/d(face metres) by finite differences."""
    h = 0.003
    c = uv_at(me, fuv, tree, e["x"], e["z"])
    px = uv_at(me, fuv, tree, e["x"] + h, e["z"])
    pz = uv_at(me, fuv, tree, e["x"], e["z"] + h)
    if c is None or px is None or pz is None:
        return None
    j = [[(px.x - c.x) / h, (pz.x - c.x) / h], [(px.y - c.y) / h, (pz.y - c.y) / h]]
    return {"centre": [round(c.x, 5), round(c.y, 5)], "j": [[round(v, 4) for v in r] for r in j]}


def build(model, arm, cid, spec):
    face = spec.get("face", {})
    marks = landmarks(model, arm, cid, spec)
    ipd = marks["ipd"]
    k = ipd / 0.063
    with_eyes = face.get("eyes", True)
    me = model.data
    tree0 = BVHTree.FromObject(model, bpy.context.evaluated_depsgraph_get(), deform=False)

    def surf_y(x, z):
        loc, _ = front_hit(tree0, x, z)
        return loc.y if loc is not None else 0.0
    eyes = marks["eyes"]
    m = marks["mouth"]
    for e in eyes:
        e["surface_y"] = round(surf_y(e["x"], e["z"]), 4)
    regions = [(m["x"], m["z"], m["w"] * 0.7, 0.016 * k)]
    if with_eyes:
        regions += [(e["x"], e["z"], e["w"] * 0.8, e["h"] * 1.5 + 0.004) for e in eyes]
    face_y = surf_y(m["x"], (eyes[0]["z"] + m["z"]) / 2) + 0.05
    # the whole face once (24k triangles leave facets a key light shows; rounding them with smooth > 0 puffed up the
    # lids), then the eyes and the mouth once more; the normal map is baked afterwards, on this final face
    mid_z = (eyes[0]["z"] + m["z"]) / 2
    subdivide_near(model, [(m["x"], mid_z, 0.075 * k, 0.095 * k)], lambda x, z: face_y, levels=1)
    subdivide_near(model, regions, lambda x, z: face_y, levels=1)
    if with_eyes:
        smooth_eyes(model, eyes, iterations=2)
    lower = part_lips(model, m, k, face_y)
    mouth_bag(model, m, k, surf_y(m["x"], m["z"]))
    tree = BVHTree.FromObject(model, bpy.context.evaluated_depsgraph_get(), deform=False)
    if with_eyes:
        fuv = faces_uv(me)
        for e in eyes:
            e["uv"] = eye_uv(me, fuv, tree, e)
            e["surface_y"] = round(surf_y(e["x"], e["z"]), 4)
        json.dump(marks, open(os.path.join(HERE, cid, "face_marks.json"), "w"), indent=1)
    if with_eyes:
        eye_mask(model, eyes)
    shape_keys(model, marks, k, tree0, with_eyes, lower)
    return marks


def smooth_eyes(model, eyes, iterations=4, factor=0.5, grow=1.5):
    """Irons the relief out of the eye openings and the lids (hi3d sculpts ragged ridges there that a key light turns
    into torn lids and hard rings); the painted eye carries the look. Also run on the bake's high copy, so the normal
    map doesn't bring the ridges back."""
    bm = bmesh.new()
    bm.from_mesh(model.data)
    verts = []
    for v in bm.verts:
        for e in eyes:
            sy = e.get("surface_y")
            if ((v.co.x - e["x"]) / (e["w"] / 2 * grow)) ** 2 + ((v.co.z - e["z"]) / (e["h"] / 2 * grow + 0.004)) ** 2 < 1.0 \
                    and (sy is None or abs(v.co.y - sy) < 0.015):
                verts.append(v)
                break
    for _ in range(iterations):
        bmesh.ops.smooth_vert(bm, verts=verts, factor=factor, use_axis_x=True, use_axis_y=True, use_axis_z=True)
    bm.to_mesh(model.data)
    bm.free()
    model.data.update()
    log("eyes: %d vertices smoothed (%s)" % (len(verts), model.name))


def eye_mask(model, eyes):
    """Vertex colour EyeMask: red = the left eye's opening, green = the right's (1 inside, fading out to the lids), for
    M_HumanFace's iris slide. A mask in texture space caught the neighbouring UV islands too (a crease over the nose)."""
    me = model.data
    attr = me.color_attributes.new("EyeMask", "BYTE_COLOR", "POINT")
    co = np.array([v.co[:] for v in me.vertices])
    col = np.zeros((len(co), 4), np.float32)
    col[:, 3] = 1.0
    for ch, e in enumerate(eyes):
        r = np.sqrt(((co[:, 0] - e["x"]) / (e["w"] / 2)) ** 2 + ((co[:, 2] - e["z"]) / (e["h"] / 2)) ** 2)
        front = np.abs(co[:, 1] - e["surface_y"]) < 0.01
        col[:, ch] = np.where(front, 1.0 - smooth((r - 0.55) / 0.4), 0.0)
    attr.data.foreach_set("color", col.ravel())
    me.color_attributes.active_color = attr
    me.color_attributes.render_color_index = me.color_attributes.find("EyeMask")
    log("eye mask: %d / %d vertices" % (int((col[:, 0] > 0.01).sum()), int((col[:, 1] > 0.01).sum())))


def part_lips(model, m, k, face_y):
    """Cuts the face along the lip line (a horizontal plane through it, across the mouth's width) and splits it, so
    jawOpen opens a real gap. Returns a mask of the vertices on the lower side of the cut."""
    bm = bmesh.new()
    bm.from_mesh(model.data)
    mw = m["w"] / 2
    faces = [f for f in bm.faces if f.calc_center_median().y < face_y
             and abs(f.calc_center_median().x - m["x"]) < mw * 1.05 and abs(f.calc_center_median().z - m["z"]) < 0.008 * k]
    geom = list({v for f in faces for v in f.verts}) + list({e for f in faces for e in f.edges}) + faces
    res = bmesh.ops.bisect_plane(bm, geom=geom, plane_co=(0, 0, m["z"]), plane_no=(0, 0, 1), dist=0.0002)
    cut = [e for e in res["geom_cut"] if isinstance(e, bmesh.types.BMEdge)
           and all(abs(v.co.x - m["x"]) < mw * 0.92 and abs(v.co.z - m["z"]) < 0.0004 for v in e.verts)
           and all(f.calc_center_median().y < face_y for f in e.link_faces)]
    bmesh.ops.split_edges(bm, edges=cut)
    bm.verts.ensure_lookup_table()
    lower = np.zeros(len(bm.verts), bool)
    for v in bm.verts:
        if abs(v.co.z - m["z"]) < 0.0004 and abs(v.co.x - m["x"]) < mw and v.link_faces:
            lower[v.index] = np.mean([f.calc_center_median().z for f in v.link_faces]) < m["z"]
    # the two copies a hair apart: an importer welding duplicate positions would join the lips again
    for v in bm.verts:
        if lower[v.index]:
            v.co.z -= 0.0004
    bm.to_mesh(model.data)
    bm.free()
    model.data.update()
    log("lips parted along %d edges" % len(cut))
    return lower


def mouth_bag(model, m, k, sy):
    """A dark curved sheet behind the lips, seen when the mouth opens; textured with the darkest bit of the model's
    own texture (no new material), skinned to the head."""
    me = model.data
    mat = me.materials[0]
    img = next(n.image for n in mat.node_tree.nodes if n.type == "TEX_IMAGE" and n.image and "normal" not in n.image.name.lower())
    w, h = img.size
    px = np.empty(w * h * 4, np.float32)
    img.pixels.foreach_get(px)
    lum = px.reshape(h, w, 4)[:, :, :3].mean(2)
    # the darkest 9x9 block (a single dark texel would bleed with mips)
    s = 9
    blk = lum[: h - h % s, : w - w % s].reshape(h // s, s, w // s, s).max((1, 3))
    j, i = np.unravel_index(np.argmin(blk), blk.shape)
    uv = Vector(((i * s + s / 2) / w, (j * s + s / 2) / h))
    mw = m["w"] / 2
    tree = BVHTree.FromObject(model, bpy.context.evaluated_depsgraph_get(), deform=False)
    bm = bmesh.new()
    nx, nz = 8, 4
    grid = {}
    for a in range(nx + 1):
        for b in range(nz + 1):
            x = m["x"] + (a / nx * 2 - 1) * mw * 0.95
            z = m["z"] - 0.013 * k + b / nz * 0.017 * k
            y = sy + 0.009 * k + 0.012 * k * ((x - m["x"]) / mw) ** 2
            hit, _ = front_hit(tree, x, z)                 # always behind the skin (a beard, a fuller lip)
            if hit is not None:
                y = max(y, hit.y + 0.004 * k)
            grid[a, b] = bm.verts.new((x, y, z))
    for a in range(nx):
        for b in range(nz):
            bm.faces.new((grid[a, b], grid[a + 1, b], grid[a + 1, b + 1], grid[a, b + 1]))
    uvl = bm.loops.layers.uv.new(me.uv_layers.active.name)
    for f in bm.faces:
        for l in f.loops:
            l[uvl].uv = uv
    bag = bpy.data.meshes.new("mouth_bag")
    bm.to_mesh(bag)
    bm.free()
    o = bpy.data.objects.new("mouth_bag", bag)
    bpy.context.scene.collection.objects.link(o)
    bag.materials.append(mat)
    o.vertex_groups.new(name="head").add(list(range(len(bag.vertices))), 1.0, "REPLACE")
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True)
    model.select_set(True)
    bpy.context.view_layer.objects.active = model
    bpy.ops.object.join()
    log("mouth: a dark sheet behind the lips (texel %.0f%% luminance)" % (100 * blk.min()))


# ---------------------------------------------------------------------------------------------- shape keys

def smooth(x):
    x = np.clip(x, 0.0, 1.0)
    return x * x * (3 - 2 * x)


def shape_keys(model, marks, k, tree0, with_eyes, lower):
    me = model.data
    if not me.shape_keys:
        model.shape_key_add(name="Basis", from_mix=False)
    co = np.array([v.co[:] for v in me.vertices])
    body = np.ones(len(co), bool)

    def add(name, delta):
        sk = model.shape_key_add(name=name, from_mix=False)
        sk.data.foreach_set("co", (co + delta).ravel())
        log("key %s: %d vertices move (max %.1f mm)" % (name, int((np.abs(delta).sum(1) > 1e-6).sum()), 1000 * np.abs(delta).max()))

    for side, e in zip("lr", marks["eyes"]):
        if not with_eyes:
            add("eyeBlinkLeft" if side == "l" else "eyeBlinkRight", np.zeros_like(co))
            continue
        a, b = e["w"] / 2, e["h"] / 2
        u = (co[:, 0] - e["x"]) / a
        t = (co[:, 2] - e["z"]) / b
        sy = e["surface_y"]
        near = (np.abs(u) < 1.3) & (np.abs(co[:, 1] - sy) < 0.012) & (t > -1.6) & (t < 3.0) & (co[:, 2] < e["z"] + 0.022)
        rim = 0.25 + 0.75 * np.sqrt(np.clip(1 - u * u, 0.0, 1.0))
        g = smooth((1.3 - np.abs(u)) / 0.3)
        t2 = t.copy()
        inside = near & (np.abs(t) <= rim)
        t2[inside] = -0.85 * rim[inside] + (t[inside] + rim[inside]) * 0.075     # the eye squeezed into a line
        lid = near & (t > rim)
        f = np.clip(1 - (t - rim) / 1.3, 0, 1)
        t2[lid] = t[lid] - 1.7 * rim[lid] * f[lid]                               # the upper lid's skin follows
        low = near & (t < -rim)
        f = np.clip(1 - (-t - rim) / 0.6, 0, 1)
        t2[low] = t[low] + 0.1 * rim[low] * f[low]                               # the lower lid lifts a little
        d = np.zeros_like(co)
        d[:, 2] = (t2 - t) * b * g
        d[:, 1] = -0.0008 * k * g * near * (np.abs(t2 - t) > 1e-6)                # the lid is a touch in front
        add("eyeBlinkLeft" if side == "l" else "eyeBlinkRight", d)
    m = marks["mouth"]
    mw = m["w"] / 2
    front_m, _ = front_hit(tree0, m["x"], m["z"])
    fy = front_m.y if front_m is not None else co[:, 1].min()
    dx = co[:, 0] - m["x"]
    dz = co[:, 2] - m["z"]
    facing = co[:, 1] < fy + 0.045 * k
    # jawOpen: under the lip line, the chin and the lower lip turn about a hinge under the ears
    side_fade = smooth((mw * 1.9 + 0.012 * k - np.abs(dx)) / (0.025 * k))
    cut = np.abs(dx) < mw * 0.92                         # across the parted lips the lower side moves whole
    lw = np.zeros(len(co))
    lw[: len(lower)] = lower
    on_line = np.abs(dz) < 0.0004                        # the cut's two copies: the lower one goes
    below = np.where(cut, np.where(on_line, lw, (dz < 0).astype(float)), smooth((-dz) / 0.0025))
    # an oval opening: the lower lip drops most in the middle, the corners hardly (deeper down the chin moves whole)
    taper = np.sqrt(np.clip(1 - (dx / (mw * 0.98)) ** 2, 0, 1))
    deep = smooth((-dz) / (0.012 * k))
    below = below * (taper + (1 - taper) * deep)
    chin = smooth((dz + 0.085 * k) / (0.02 * k))
    w = facing * side_fade * below * chin
    hinge = np.array([m["x"], fy + 0.085 * k, m["z"] + 0.022 * k])
    ang = math.radians(14.0) * w
    y, z = co[:, 1] - hinge[1], co[:, 2] - hinge[2]
    d = np.zeros_like(co)
    d[:, 1] = (y * np.cos(ang) - z * np.sin(ang)) - y
    d[:, 2] = (y * np.sin(ang) + z * np.cos(ang)) - z
    add("jawOpen", d)
    lips = facing * smooth(1 - np.sqrt((dx / (mw * 1.25)) ** 2 + (dz / (0.014 * k)) ** 2))
    d = np.zeros_like(co)
    d[:, 0] = -0.32 * dx * lips
    d[:, 1] = -0.003 * k * lips
    add("mouthFunnel", d)
    corner = lips * np.clip(np.abs(dx) / mw, 0, 1.2)
    d = np.zeros_like(co)
    d[:, 0] = np.sign(dx) * 0.0022 * k * corner
    d[:, 2] = 0.003 * k * corner
    d[:, 1] = 0.0008 * k * corner
    add("mouthSmile", d)
    d = np.zeros_like(co)
    for e in marks["eyes"]:
        bz = e["z"] + e["h"] * 0.5 + 0.008 * k
        br = facing * smooth(1 - np.abs(co[:, 0] - e["x"]) / (e["w"] * 0.85)) * smooth(1 - np.abs(co[:, 2] - bz - 0.006 * k) / (0.014 * k))
        d[:, 2] += 0.0035 * k * br
    add("browUp", d)
