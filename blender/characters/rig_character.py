"""Rigs a generated character model (one textured mesh from hi3d, T-pose) on the shared human skeleton.

    Blender -b -P blender/characters/rig_character.py -- nao [--preview]

Reads blender/characters/<id>/character.json (height, donor body, face budget, skirt) and gen/model.glb; writes
blender/characters/<id>/export/human_<id>.glb (armature root/pelvis/... exactly as SK_Human_Skeleton, one skinned mesh,
material <id>.body) for unreal/scripts/import_character.py, and --preview renders renders/characters/<id>_*.png.

How (the standard, docs/characters.md):
  1. The donor is a MakeHuman human of the cast (blender/humans/export/human_<donor>.glb, built by build_humans.py):
     its skeleton is SK_Human_Skeleton's bone for bone, and its skin weights are the house's.
  2. The donor's arms are posed into the generated T-pose by rotating the bones rigidly (frames and rolls kept, so every
     shared clip, which stores local rotations, moves the new body the same way) and that pose becomes the rest pose.
  3. The donor (bones and body) is fitted to the model: uniform scale to the height, then the arm chains stretched along
     X to the model's arm span; knees and ankles moved into the middle of the model's legs (fit_legs).
  4. The model is cleaned (one mesh, feet on the floor at the origin, facing -Y) and decimated to the face budget, then
     takes the donor's weights (nearest surface). Cloth far from the body below the hips (a skirt) is blended toward
     the pelvis so it doesn't split between the legs.
"""
import json
import math
import os
import sys

import bpy
import bmesh
import numpy as np
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
ARM_CHAINS = {s: ["upperarm_" + s, "lowerarm_" + s, "hand_" + s] for s in ("l", "r")}


def log(m):
    print("RIG " + m, flush=True)


def import_glb(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path, bone_heuristic="BLENDER")
    return [o for o in bpy.data.objects if o not in before]


def world_co(o):
    n = len(o.data.vertices)
    co = np.empty(n * 3)
    o.data.vertices.foreach_get("co", co)
    co = co.reshape(n, 3)
    m = np.array(o.matrix_world)
    return co @ m[:3, :3].T + m[:3, 3]


def set_world_co(o, co):
    m = np.array(o.matrix_world.inverted())
    o.data.vertices.foreach_set("co", (co @ m[:3, :3].T + m[:3, 3]).ravel())
    o.data.update()


# ---------------------------------------------------------------------------------------------- the model

def load_model(path, height):
    objs = [o for o in import_glb(path) if o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    if len(objs) > 1:
        bpy.ops.object.join()
    m = bpy.context.view_layer.objects.active
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    co = world_co(m)
    lo, hi = co.min(0), co.max(0)
    s = height / (hi[2] - lo[2])
    centre = np.array([(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2]])
    set_world_co(m, (co - centre) * s)
    log("model: %d verts, %d faces, raw height %.3f -> %.3f m" % (len(m.data.vertices), len(m.data.polygons), hi[2] - lo[2], height))
    return m


def decimate(m, faces):
    n = len(m.data.polygons)
    if n <= faces:
        return
    mod = m.modifiers.new("decimate", "DECIMATE")
    mod.ratio = faces / n
    mod.use_collapse_triangulate = True
    bpy.context.view_layer.objects.active = m
    bpy.ops.object.modifier_apply(modifier=mod.name)
    log("decimated %d -> %d faces" % (n, len(m.data.polygons)))


def clean(m, min_island=40):
    """Welds duplicate vertices and drops tiny loose islands (specks the generator leaves around the model)."""
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(m.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0005)
    bm.verts.ensure_lookup_table()
    seen, drop = set(), []
    for v in bm.verts:
        if v.index in seen:
            continue
        stack, island = [v], []
        seen.add(v.index)
        while stack:
            a = stack.pop()
            island.append(a)
            for e in a.link_edges:
                b = e.other_vert(a)
                if b.index not in seen:
                    seen.add(b.index)
                    stack.append(b)
        if len(island) < min_island:
            drop.extend(island)
    bmesh.ops.delete(bm, geom=drop, context="VERTS")
    bm.to_mesh(m.data)
    bm.free()
    log("cleaned: %d specks' vertices removed, %d verts left" % (len(drop), len(m.data.vertices)))


def optimize(m, spec):
    """Game budget (docs/characters.md): the generator's ~2M faces become `faces` triangles, smooth shaded; the detail
    lost is baked from a 400k copy into a `texture`-sized normal map; the base colour is scaled to the same size."""
    clean(m)
    tex = spec.get("texture", 2048)
    high = None
    if spec.get("bake_normal", True):
        decimate(m, 400000)
        high = m.copy()
        high.data = m.data.copy()
        bpy.context.scene.collection.objects.link(high)
        high.data.materials[0] = m.data.materials[0].copy()      # the bake target lives only in the low one's material
    decimate(m, spec.get("faces", 24000))
    for p in m.data.polygons:
        p.use_smooth = True
    mat = m.data.materials[0]
    nodes = mat.node_tree.nodes
    bsdf = next(n for n in nodes if n.type == "BSDF_PRINCIPLED")
    base = next((l.from_node for l in mat.node_tree.links if l.to_socket == bsdf.inputs["Base Color"]), None)
    if base is not None and base.image and base.image.size[0] > tex:
        base.image.scale(tex, tex)
        log("base colour scaled to %d" % tex)
    for l in list(mat.node_tree.links):                      # metal/roughness: the master has fixed values
        if l.to_socket in (bsdf.inputs["Metallic"], bsdf.inputs["Roughness"]):
            mat.node_tree.links.remove(l)
    bsdf.inputs["Metallic"].default_value = 0.0
    if high is not None:                                     # baked at the end, on the final face (face_rig.py
        high.hide_render = True                              # subdivides and rounds it)
        PENDING_BAKE[:] = [high, mat, bsdf, tex]


PENDING_BAKE = []
FLAT_UV = []        # texels the grafted hands use (replace_hands): flat in the normal map


def bake_pending(m):
    if PENDING_BAKE:
        high, mat, bsdf, tex = PENDING_BAKE
        high.hide_render = False
        bake_normal(m, high, mat, bsdf, tex)
        bpy.data.objects.remove(high)
        PENDING_BAKE.clear()


def bake_normal(low, high, mat, bsdf, size):
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.samples = 1
    sc.cycles.device = "CPU"
    img = bpy.data.images.new(low.name + "_normal", size, size, alpha=False, float_buffer=False)
    img.colorspace_settings.name = "Non-Color"
    nodes = mat.node_tree.nodes
    tn = nodes.new("ShaderNodeTexImage")
    tn.image = img
    nodes.active = tn
    bpy.ops.object.select_all(action="DESELECT")
    high.select_set(True)
    low.select_set(True)
    bpy.context.view_layer.objects.active = low
    bpy.ops.object.bake(type="NORMAL", use_selected_to_active=True, cage_extrusion=0.006, max_ray_distance=0.02,
                        margin=8, normal_space="TANGENT")
    if FLAT_UV:
        px = np.empty(size * size * 4, np.float32)
        img.pixels.foreach_get(px)
        px = px.reshape(size, size, 4)
        for u, v in FLAT_UV:
            x, y = int(u * size), int(v * size)
            px[max(y - 4, 0):y + 5, max(x - 4, 0):x + 5, :3] = (0.5, 0.5, 1.0)
        img.pixels.foreach_set(px.ravel())
    img.file_format = "PNG"
    nm = nodes.new("ShaderNodeNormalMap")
    mat.node_tree.links.new(tn.outputs["Color"], nm.inputs["Color"])
    mat.node_tree.links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
    img.pack()
    log("normal map baked %dx%d" % (size, size))


def silhouette(co):
    """Arm span at shoulder height, and the stance: what the fit needs from the model."""
    h = co[:, 2].max()
    arms = co[(co[:, 2] > 0.70 * h) & (co[:, 2] < 0.86 * h)]
    feet = co[co[:, 2] < 0.04 * h]
    return {"span_l": arms[:, 0].max(), "span_r": -arms[:, 0].min(), "arm_z": float(np.median(arms[np.abs(arms[:, 0]) > 0.3 * h][:, 2])),
            "foot_l": float(feet[feet[:, 0] > 0][:, 0].mean()), "foot_r": float(-feet[feet[:, 0] < 0][:, 0].mean()), "height": h}


# ---------------------------------------------------------------------------------------------- the donor

def load_donor(path):
    new = import_glb(path)
    arm = next(o for o in new if o.type == "ARMATURE")
    body = next(o for o in new if o.type == "MESH" and o.parent == arm)
    for o in new:
        if o not in (arm, body):
            bpy.data.objects.remove(o)
    if body.data.shape_keys:
        body.shape_key_clear()
    for o in (arm, body):                                    # the donor's own clips are not needed
        if o.animation_data:
            o.animation_data_clear()
    for a in list(bpy.data.actions):
        bpy.data.actions.remove(a)
    for b in arm.pose.bones:                                 # the importer leaves the first clip's frame posed
        b.matrix_basis = Matrix()
    bpy.context.view_layer.update()
    return arm, body


def apply_pose_as_rest(arm, body):
    """Bakes the current pose into the body and makes it the armature's rest pose."""
    bpy.context.view_layer.update()
    bpy.ops.object.select_all(action="DESELECT")
    bpy.context.view_layer.objects.active = body
    mod = next(md for md in body.modifiers if md.type == "ARMATURE")
    keep = mod.name
    bpy.ops.object.modifier_copy(modifier=keep)
    bpy.ops.object.modifier_apply(modifier=keep)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="POSE")
    bpy.ops.pose.select_all(action="SELECT")
    bpy.ops.pose.armature_apply(selected=False)
    bpy.ops.object.mode_set(mode="OBJECT")


def t_pose(arm, body, arm_z=None):
    """Rotates each arm chain rigidly so it points straight out along +-X (bone frames and rolls ride along)."""
    pb = arm.pose.bones
    for side, chain in ARM_CHAINS.items():
        out = Vector((1 if side == "l" else -1, 0, 0))
        for name in chain:
            bpy.context.view_layer.update()
            b = pb[name]
            mw = arm.matrix_world @ b.matrix
            head = mw.translation
            child = [c for c in b.children if c.name in chain]
            tail = (arm.matrix_world @ child[0].matrix).translation if child else (arm.matrix_world @ b.tail)
            d = (tail - head).normalized()
            q = d.rotation_difference(out)
            rot = Matrix.Translation(head) @ q.to_matrix().to_4x4() @ Matrix.Translation(-head)
            b.matrix = arm.matrix_world.inverted() @ rot @ mw
    apply_pose_as_rest(arm, body)


def bone_points(arm):
    return {b.name: (arm.matrix_world @ b.head_local, arm.matrix_world @ b.tail_local) for b in arm.data.bones}


def warp_donor(arm, body, fn):
    """Moves the donor's bone joints and body vertices by the same point map fn (Nx3 -> Nx3).
    Bones are moved in edit mode by their heads/tails; only lengths along each arm change, so the frames keep their rolls."""
    set_world_co(body, fn(world_co(body)))
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    inv = arm.matrix_world.inverted()
    mw = arm.matrix_world
    for e in arm.data.edit_bones:
        roll = e.roll
        h = fn(np.array([tuple(mw @ e.head)]))[0]
        t = fn(np.array([tuple(mw @ e.tail)]))[0]
        e.head = inv @ Vector(h)
        e.tail = inv @ Vector(t)
        e.roll = roll
    bpy.ops.object.mode_set(mode="OBJECT")


def fit_donor(arm, body, target):
    co = world_co(body)
    h = co[:, 2].max()
    s = target["height"] * target.get("donor_height_share", 0.985) / h      # the model's hair stands a little higher
    warp_donor(arm, body, lambda p: p * s)
    src = silhouette(world_co(body))
    pts = bone_points(arm)
    for side, sign in (("l", 1), ("r", -1)):
        sx = abs(pts["upperarm_" + side][0].x)
        k = (target["span_" + side] - sx) / (src["span_" + side] - sx)
        dz = target["arm_z"] - src["arm_z"]

        def f(p, sx=sx, k=k, sign=sign, dz=dz):
            q = p.copy()
            m = (q[:, 0] * sign) > sx
            q[m, 0] = sign * (sx + (q[m, 0] * sign - sx) * k)
            q[m, 2] += dz * np.clip((q[m, 0] * sign - sx) / 0.15, 0, 1)
            return q
        warp_donor(arm, body, f)
        log("arm %s: span %.3f -> %.3f (x%.3f), height %+.3f" % (side, src["span_" + side], target["span_" + side], k, dz))


def leg_centre(co, z, sign, h):
    """The middle of one leg's cross-section at height z (bounding box centre, x, y), or None where it isn't a
    clean leg (a skirt or a coat joins both legs there)."""
    s = co[(np.abs(co[:, 2] - z) < 0.012) & (co[:, 0] * sign > 0.012)]
    if len(s) < 12:
        return None
    w = s[:, 0].max() - s[:, 0].min()
    if w > 0.13 * h or s[:, 0].min() * sign < 0.0:
        return None
    return np.array([(s[:, 0].max() + s[:, 0].min()) / 2, (s[:, 1].max() + s[:, 1].min()) / 2])


def centre_body(arm, body, mco):
    """The generated model is centred on its bounding box (hair, a bag, the arms' T), the donor on its own feet: shift
    the donor so its hips' cross-section has the same middle as the model's (front/back and sideways)."""
    z = bone_points(arm)["thigh_l"][0].z + 0.03
    dco = world_co(body)
    ms, ds = (c[np.abs(c[:, 2] - z) < 0.015] for c in (mco, dco))
    ms = ms[np.abs(ms[:, 0]) < 0.3]
    d = np.array([(ms[:, i].max() + ms[:, i].min() - ds[:, i].max() - ds[:, i].min()) / 2 for i in (0, 1)] + [0.0])
    warp_donor(arm, body, lambda p: p + d)
    log("body centred on the model's hips: %+.3f %+.3f m" % (d[0], d[1]))


def fit_legs(arm, body, mco, h):
    """Moves the donor's knees and ankles into the middle of the model's legs (the generated stance is wider than
    the MakeHuman donor's): rotations then swing the model's legs about their own joints, so they don't cross.
    The hip joint stays; below it each leg is shifted sideways and front/back, growing to the knee's and the ankle's
    offsets, faded out toward the body's midline."""
    pts = bone_points(arm)
    dco = world_co(body)
    for side, sign in (("l", 1), ("r", -1)):
        hip, knee, ankle = (pts[b + "_" + side][0].z for b in ("thigh", "calf", "foot"))
        off = {}
        for nm, z in (("knee", knee), ("ankle", ankle + 0.02)):
            m, d = leg_centre(mco, z, sign, h), leg_centre(dco, z, sign, h)
            off[nm] = None if m is None or d is None else m - d
        if off["ankle"] is None:
            log("legs %s: no clean ankle section, kept" % side)
            continue
        if off["knee"] is None:                                  # under a skirt: halfway, as a straight leg would be
            off["knee"] = off["ankle"] * (hip - knee) / (hip - ankle)
        dk, da = off["knee"], off["ankle"]

        def f(p, hip=hip, knee=knee, ankle=ankle, dk=dk, da=da, sign=sign):
            q = p.copy()
            z = q[:, 2]
            t_k = np.clip((hip - z) / (hip - knee), 0, 1)
            t_a = np.clip((knee - z) / (knee - ankle), 0, 1)
            d = dk[None, :] * t_k[:, None] + (da - dk)[None, :] * t_a[:, None]
            fade = np.clip(q[:, 0] * sign / 0.05, 0, 1) * (z < hip)
            q[:, 0] += d[:, 0] * fade
            q[:, 1] += d[:, 1] * fade
            return q
        warp_donor(arm, body, f)
        log("legs %s: knee %+.3f %+.3f, ankle %+.3f %+.3f m" % (side, dk[0], dk[1], da[0], da[1]))


# ---------------------------------------------------------------------------------------------- weights

def select_beyond(o, cut, sign, mat_name=None):
    """Vertex indices of `o` past x = cut on the arm's side (sign*x > sign*cut), optionally only `mat_name`'s faces."""
    co = world_co(o)
    far = sign * co[:, 0] > sign * cut
    if mat_name is not None:
        slot = next(i for i, s in enumerate(o.material_slots) if s.material and s.material.name == mat_name)
        keep = np.zeros(len(co), bool)
        for p in o.data.polygons:
            if p.material_index == slot:
                keep[list(p.vertices)] = True
        far &= keep
    return far


def delete_verts(o, mask):
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(o.data)
    bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[bm.verts[i] for i in np.nonzero(mask)[0]], context="VERTS")
    bm.to_mesh(o.data)
    bm.free()


def base_image(m):
    mat = m.data.materials[0]
    bsdf = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    return next(l.from_node for l in mat.node_tree.links if l.to_socket == bsdf.inputs["Base Color"]).image


def vertex_colours(m, img, px):
    """Each vertex's base colour (the texel under its first loop's UV)."""
    W, H = img.size
    uv = np.empty(len(m.data.loops) * 2)
    m.data.uv_layers[0].data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    li = np.empty(len(m.data.loops), int)
    m.data.loops.foreach_get("vertex_index", li)
    vuv = np.zeros((len(m.data.vertices), 2))
    vuv[li] = uv
    x = np.clip((vuv[:, 0] % 1) * W, 0, W - 1).astype(int)
    y = np.clip((vuv[:, 1] % 1) * H, 0, H - 1).astype(int)
    return px[y, x, :3]


def find_cuff(m, sign, px, img, step=0.005):
    """Where the sleeve ends: walking in from the fingertips, the first slice where under half the vertices are
    skin-coloured (warm: red over blue; the jacket's grey and the jumper's green cuff are not, the painted finger gaps
    are only darker)."""
    co = world_co(m)
    col = vertex_colours(m, img, px)
    a = sign * co[:, 0]
    tip = a.max()
    like = (col[:, 0] - col[:, 2] > 0.07) & (col[:, 0] > 0.3)
    skin = np.median(col[(a > tip - 0.06) & like], 0)
    x = tip - 0.03
    while x > tip - 0.3:
        s = (a > x - step) & (a <= x)
        if s.sum() > 10 and like[s].mean() < 0.5:
            return sign * x, skin
        x -= step
    raise RuntimeError("no cuff found")


def paint_uv_tris(px, uv_tris, colour, grow=4):
    """Fills the texels under UV triangles (n x 3 x 2, 0..1) with one colour, `grow` texels wider."""
    H, W = px.shape[:2]
    mask = np.zeros((H, W), bool)
    for t in uv_tris:
        p = t * (W, H)
        x0, y0 = np.floor(p.min(0)).astype(int) - grow
        x1, y1 = np.ceil(p.max(0)).astype(int) + grow
        x0, y0, x1, y1 = max(x0, 0), max(y0, 0), min(x1, W - 1), min(y1, H - 1)
        if x1 < x0 or y1 < y0:
            continue
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        (ax, ay), (bx, by), (cx, cy) = p
        d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(d) < 1e-9:
            continue
        l1 = ((by - cy) * (gx - cx) + (cx - bx) * (gy - cy)) / d
        l2 = ((cy - ay) * (gx - cx) + (ax - cx) * (gy - cy)) / d
        l3 = 1 - l1 - l2
        e = grow / max(abs(d) ** 0.5, 1.0)
        mask[y0:y1 + 1, x0:x1 + 1] |= (l1 > -e) & (l2 > -e) & (l3 > -e)
    px[mask, :3] = colour
    return mask


def replace_hands(model, body, arm, spec):
    """The generator fuses the fingers into mittens (hi3d, every adult so far): each hand is cut off at the wrist and
    the donor's own hand (MakeHuman skin, five fingers, its 15 finger bones) is set in its place, a few centimetres of
    its wrist reaching back into the sleeve. Its colour comes from the cut-off hand (UVs by nearest surface onto the
    model's own texture), its weights from the donor like the rest (transfer_weights). The high copy for the normal
    bake loses the mitten too, so the fingers don't take its folds."""
    skin = next(s.material.name for s in body.material_slots if s.material and s.material.name.endswith(".body"))
    cutoff, overlap = spec.get("cut", 0.005), spec.get("overlap", 0.04)
    high = PENDING_BAKE[0] if PENDING_BAKE else None
    img = base_image(model)
    W, H = img.size
    px = np.empty(W * H * 4, np.float32)
    img.pixels.foreach_get(px)
    px = px.reshape(H, W, 4)
    for side, sign in (("l", 1), ("r", -1)):
        cuff, tone = find_cuff(model, sign, px, img)
        cut = cuff + sign * cutoff
        # the old hand: kept apart as the colour source, then dropped from the model and the bake copy
        old = model.copy()
        old.data = model.data.copy()
        bpy.context.scene.collection.objects.link(old)
        delete_verts(old, ~select_beyond(old, cut, sign))
        # the mitten's texels become one even skin tone: the new fingers take UVs from it by nearest surface, across
        # its islands' seams, which would bring the seams' padding (jacket grey) and the painted finger gaps along
        uv = np.empty(len(old.data.loops) * 2)
        old.data.uv_layers[0].data.foreach_get("uv", uv)
        uv = uv.reshape(-1, 2)
        tris = [uv[list(p.loop_indices)] for p in old.data.polygons if p.loop_total == 3]
        mask = paint_uv_tris(px, np.array(tris) % 1.0, tone)
        # the texel deepest inside the painted area: every new hand corner points at it
        inner = mask.copy()
        for _ in range(64):
            nxt = inner.copy()
            nxt[1:] &= inner[:-1]
            nxt[:-1] &= inner[1:]
            nxt[:, 1:] &= inner[:, :-1]
            nxt[:, :-1] &= inner[:, 1:]
            if not nxt.any():
                break
            inner = nxt
        ys, xs = np.nonzero(inner)
        spot = ((xs[0] + 0.5) / W, (ys[0] + 0.5) / H)
        FLAT_UV.append(spot)
        delete_verts(model, select_beyond(model, cut, sign))
        if high is not None:
            delete_verts(high, select_beyond(high, cut, sign))
        # the donor's hand (skin faces only), joined into the model
        hand = body.copy()
        hand.data = body.data.copy()
        bpy.context.scene.collection.objects.link(hand)
        for md in list(hand.modifiers):
            hand.modifiers.remove(md)
        hand.parent = None
        hand.matrix_world = body.matrix_world.copy()
        delete_verts(hand, ~select_beyond(hand, cut - sign * overlap, sign, skin))
        n_hand = len(hand.data.vertices)
        for p in hand.data.polygons:
            p.material_index = 0
            p.use_smooth = True
        hand.data.materials.clear()
        hand.data.materials.append(model.data.materials[0])
        for g in list(hand.vertex_groups):
            hand.vertex_groups.remove(g)
        hand.vertex_groups.new(name="graft").add(list(range(n_hand)), 1.0, "REPLACE")
        while len(hand.data.uv_layers) > 1:
            hand.data.uv_layers.remove(hand.data.uv_layers[-1])
        hand.data.uv_layers[0].name = model.data.uv_layers[0].name
        bpy.ops.object.select_all(action="DESELECT")
        hand.select_set(True)
        model.select_set(True)
        bpy.context.view_layer.objects.active = model
        bpy.ops.object.join()
        gi = model.vertex_groups["graft"].index
        graft = np.array([any(g.group == gi for g in v.groups) for v in model.data.vertices])
        li = np.empty(len(model.data.loops), int)
        model.data.loops.foreach_get("vertex_index", li)
        uv = np.empty(len(model.data.loops) * 2)
        lay = model.data.uv_layers[0]
        lay.data.foreach_get("uv", uv)
        uv = uv.reshape(-1, 2)
        uv[graft[li]] = spot
        lay.data.foreach_set("uv", uv.ravel())
        model.vertex_groups.remove(model.vertex_groups["graft"])
        bpy.data.objects.remove(old)
        log("hand %s: sleeve ends at x=%.3f, cut at %.3f, donor hand set in (%d verts), %d texels of skin %s"
            % (side, cuff, cut, n_hand, mask.sum(), np.round(tone, 3).tolist()))
    img.pixels.foreach_set(px.ravel())
    img.update()


def transfer_weights(model, body, arm, skirt):
    for g in body.vertex_groups:
        if g.name not in model.vertex_groups:
            model.vertex_groups.new(name=g.name)
    mod = model.modifiers.new("weights", "DATA_TRANSFER")
    mod.object = body
    mod.use_vert_data = True
    mod.data_types_verts = {"VGROUP_WEIGHTS"}
    mod.vert_mapping = "POLYINTERP_NEAREST"
    mod.layers_vgroup_select_src = "ALL"
    mod.layers_vgroup_select_dst = "NAME"
    bpy.context.view_layer.objects.active = model
    bpy.ops.object.modifier_apply(modifier=mod.name)
    if skirt:
        relax_skirt(model, body, arm, skirt)
    relax_hem(model, body, arm)
    smooth_shoulders(model, arm)
    lock_head(model, arm)
    bpy.ops.object.vertex_group_limit_total(group_select_mode="ALL", limit=4)
    bpy.ops.object.vertex_group_normalize_all(group_select_mode="ALL", lock_active=False)
    model.parent = arm
    md = model.modifiers.new("Armature", "ARMATURE")
    md.object = arm


def lock_head(model, arm):
    """The generated head is one shell (hair, bangs, face): it must turn with the head bone as a whole. Nearest-surface
    weights give the bangs and the bob some neck/spine (from the donor's own hair) and the eyelids the donor's eye bones,
    so a nod slid the bangs down over the eyes. Above the jaw everything goes to `head` (eyes merged in), with a 6 cm
    blend down to the neck."""
    pts = bone_points(arm)
    top = pts["head"][0].z                      # the head joint (skull base)
    names = [g.name for g in model.vertex_groups]
    head = model.vertex_groups["head"]
    co = world_co(model)
    n = 0
    for v, p in zip(model.data.vertices, co):
        t = min(1.0, max(0.0, (p[2] - (top - 0.06)) / 0.06))
        w = {names[g.group]: g.weight for g in v.groups}
        eyes = w.pop("eye_l", 0.0) + w.pop("eye_r", 0.0)
        if t <= 0 and not eyes:
            continue
        keep = 1.0 - t
        new = {k: x * keep for k, x in w.items()}
        new["head"] = new.get("head", 0.0) + eyes * keep + t * (sum(w.values()) + eyes)
        for k in ("eye_l", "eye_r"):
            if k in model.vertex_groups:
                model.vertex_groups[k].remove([v.index])
        for k, x in new.items():
            if x > 1e-4:
                model.vertex_groups[k].add([v.index], x, "REPLACE")
            elif k in w:
                model.vertex_groups[k].remove([v.index])
        n += 1
    log("head locked: %d vertices follow the head bone" % n)


def smooth_shoulders(model, arm, radius=0.13):
    """Arms come down 60-80 degrees from the T-pose in every clip; the donor's shoulder weights, copied onto a thicker
    clothed shell, pinch it into a ridge there. Smoothing the weights round each shoulder joint spreads the turn over
    the deltoid and the shirt's yoke."""
    pts = bone_points(arm)
    co = world_co(model)
    sel = np.zeros(len(co), bool)
    for s in "lr":
        sel |= np.linalg.norm(co - np.array(pts["upperarm_" + s][0]), axis=1) < radius
    model.data.vertices.foreach_set("select", sel)
    bpy.context.view_layer.objects.active = model
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_mode(type="VERT")
    bpy.ops.object.mode_set(mode="OBJECT")
    model.data.vertices.foreach_set("select", sel)
    bpy.ops.object.mode_set(mode="WEIGHT_PAINT")
    model.data.use_paint_mask_vertex = True
    bpy.ops.object.vertex_group_smooth(group_select_mode="ALL", factor=0.5, repeat=6, expand=0.0)
    model.data.use_paint_mask_vertex = False
    bpy.ops.object.mode_set(mode="OBJECT")
    log("shoulders: weights of %d vertices smoothed" % int(sel.sum()))


def relax_hem(model, body, arm, gap=0.015, blend=0.04):
    """Loose cloth round the hips (a shirt's or a jacket's hem standing off the body) follows the pelvis, not the
    thighs: nearest-surface weights gave the hem the buttocks' thigh share, so sitting dragged the shirt's back out
    like a bag. Only cloth farther than `gap` from the donor's skin, from 12 cm under the hip joints up."""
    from mathutils.bvhtree import BVHTree
    dg = bpy.context.evaluated_depsgraph_get()
    tree = BVHTree.FromObject(body, dg)
    co = world_co(model)
    hip = bone_points(arm)["thigh_l"][0].z
    names = [g.name for g in model.vertex_groups]
    legs = [n for n in names if n.split("_")[0] in ("thigh", "calf", "foot", "ball")]
    n = 0
    for v, p in zip(model.data.vertices, co):
        if not (hip - 0.12 < p[2] < hip + 0.2):
            continue
        w = {names[g.group]: g.weight for g in v.groups}
        leg = sum(w.get(k, 0) for k in legs)
        if leg <= 0:
            continue
        hit = tree.find_nearest(Vector(p))
        if hit[0] is None:
            continue
        t = min(1.0, max(0.0, (hit[3] - gap) / blend))
        if t <= 0:
            continue
        for k in legs:
            if k in w:
                x = w[k] * (1 - t)
                if x > 1e-4:
                    model.vertex_groups[k].add([v.index], x, "REPLACE")
                else:
                    model.vertex_groups[k].remove([v.index])
        model.vertex_groups["pelvis"].add([v.index], w.get("pelvis", 0) + leg * t, "REPLACE")
        n += 1
    log("hem: %d loose cloth vertices moved from the thighs to the pelvis" % n)


def relax_skirt(model, body, arm, skirt):
    """Skirt cloth (below the hips, away from the legs) follows the pelvis more than either thigh."""
    from mathutils.bvhtree import BVHTree
    dg = bpy.context.evaluated_depsgraph_get()
    tree = BVHTree.FromObject(body, dg)
    co = world_co(model)
    pts = bone_points(arm)
    hip = pts["thigh_l"][0].z
    hem = skirt["hem_z"]
    names = [g.name for g in model.vertex_groups]
    idx = {n: i for i, n in enumerate(names)}
    legs = [n for n in names if n.split("_")[0] in ("thigh", "calf", "foot", "ball")]
    n = 0
    for v, p in zip(model.data.vertices, co):
        if not (hem - 0.02 < p[2] < hip):
            continue
        hit = tree.find_nearest(Vector(p))
        if hit[0] is None:
            continue
        t = min(1.0, max(0.0, (hit[3] - skirt["gap"]) / skirt["blend"])) * min(1.0, (hip - p[2]) / 0.12)
        if t <= 0:
            continue
        w = {names[g.group]: g.weight for g in v.groups}
        leg = sum(w.get(k, 0) for k in legs)
        keep = 1 - t
        new = {k: x * keep for k, x in w.items()}
        new["pelvis"] = new.get("pelvis", 0) + t * (1 - leg) + t * leg * skirt["pelvis"]
        for s in ("l", "r"):
            new["thigh_" + s] = new.get("thigh_" + s, 0) + t * leg * (1 - skirt["pelvis"]) / 2
        for k, x in new.items():
            if x > 1e-4:
                model.vertex_groups[idx[k]].add([v.index], x, "REPLACE")
            elif k in w:
                model.vertex_groups[idx[k]].remove([v.index])
        n += 1
    log("skirt: %d vertices relaxed toward the pelvis" % n)


# ---------------------------------------------------------------------------------------------- export

def finish(model, arm, cid):
    model.name = "SK_" + cid.capitalize()
    for i, slot in enumerate(model.material_slots):
        if slot.material:
            slot.material.name = "%s.body%s" % (cid, "" if i == 0 else str(i))
    bpy.data.objects.remove(bpy.data.objects[BODY_NAME])
    arm.name = "Armature_" + cid
    out = os.path.join(HERE, cid, "export", "human_%s.glb" % cid)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    model.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.gltf(filepath=out, use_selection=True, export_format="GLB", export_skins=True,
                              export_morph=True, export_morph_normal=True, export_animations=False,
                              export_vertex_color="ACTIVE", export_all_vertex_colors=False, export_def_bones=False, export_yup=True,
                              export_apply=False, export_image_format="AUTO", export_jpeg_quality=90)
    log("wrote %s (%d verts, %d faces, %d groups)" % (os.path.relpath(out, ROOT), len(model.data.vertices),
                                                        len(model.data.polygons), len(model.vertex_groups)))
    return out


def preview(model, arm, cid):
    """Rest pose front/side and a test pose (arms down, a step) rendered with Eevee."""
    out = os.path.join(ROOT, "renders", "characters")
    os.makedirs(out, exist_ok=True)
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    sc.render.resolution_x, sc.render.resolution_y = 900, 1200
    w = bpy.data.worlds.new("w")
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs[0].default_value = (0.6, 0.6, 0.62, 1)
    w.node_tree.nodes["Background"].inputs[1].default_value = 1.0
    sc.world = w
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 3.0
    sun.rotation_euler = (math.radians(50), 0, math.radians(30))
    sc.collection.objects.link(sun)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 2.0
    sc.collection.objects.link(cam)
    sc.camera = cam

    def shot(name, yaw):
        a = math.radians(yaw)
        cam.location = (4 * math.sin(a), -4 * math.cos(a), 0.85)
        cam.rotation_euler = (math.radians(90), 0, a)
        sc.render.filepath = os.path.join(out, "%s_%s.png" % (cid, name))
        bpy.ops.render.render(write_still=True)
    shot("front", 0)
    shot("side", 90)
    pb = arm.pose.bones
    # the hands close up (replace_hands): from the front and from above, at rest
    cam.data.ortho_scale = 0.32
    for side in ("l", "r"):
        h = arm.matrix_world @ arm.data.bones["hand_" + side].head_local
        c = h + Vector((0.08 if side == "l" else -0.08, 0, 0))
        cam.location, cam.rotation_euler = (c.x, c.y - 2, c.z), (math.radians(90), 0, 0)
        sc.render.filepath = os.path.join(out, "%s_hand_%s.png" % (cid, side))
        bpy.ops.render.render(write_still=True)
        cam.location, cam.rotation_euler = (c.x, c.y, c.z + 2), (0, 0, 0)
        sc.render.filepath = os.path.join(out, "%s_hand_%s_top.png" % (cid, side))
        bpy.ops.render.render(write_still=True)
    cam.data.ortho_scale = 2.0

    def turn(name, deg, axis):
        """Rotates a pose bone about an armature-space axis through its head."""
        bpy.context.view_layer.update()
        b = pb[name]
        mw = b.matrix.copy()
        head = mw.translation.copy()
        b.matrix = Matrix.Translation(head) @ Matrix.Rotation(math.radians(deg), 4, axis) @ Matrix.Translation(-head) @ mw
    turn("upperarm_l", 70, "Y")          # arms down
    turn("upperarm_r", -70, "Y")
    turn("lowerarm_l", -30, "Z")         # forearms forward a little
    turn("lowerarm_r", 30, "Z")
    turn("thigh_l", -30, "X")            # a step: the left leg forward, the knee bent
    turn("calf_l", 40, "X")
    turn("thigh_r", 12, "X")
    turn("spine_02", 8, "Z")
    turn("head", -15, "Z")
    turn("head", 20, "X")                # a nod: the bangs must stay on the forehead
    shot("posed", 30)
    shot("posed_side", 90)


BODY_NAME = None


def main():
    global BODY_NAME
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    cid = [a for a in argv if not a.startswith("--")][0]
    spec = json.load(open(os.path.join(HERE, cid, "character.json")))
    bpy.ops.wm.read_factory_settings(use_empty=True)
    model = load_model(os.path.join(HERE, cid, "gen", spec.get("model", "model") + ".glb"), spec["height"])
    optimize(model, spec)
    target = silhouette(world_co(model))
    target["donor_height_share"] = spec.get("donor_height_share", 0.985)
    log("model silhouette %s" % {k: round(v, 3) for k, v in target.items()})
    arm, body = load_donor(os.path.join(ROOT, spec["donor"]))
    BODY_NAME = body.name
    t_pose(arm, body)
    fit_donor(arm, body, target)
    centre_body(arm, body, world_co(model))
    fit_legs(arm, body, world_co(model), target["height"])
    if "hands" in spec:
        replace_hands(model, body, arm, spec["hands"])
    transfer_weights(model, body, arm, spec.get("skirt"))
    if spec.get("face", {}).get("rig", True):
        import face_rig
        marks = face_rig.build(model, arm, cid, spec)
        if PENDING_BAKE and spec.get("face", {}).get("eyes", True):
            face_rig.smooth_eyes(PENDING_BAKE[0], marks["eyes"], iterations=8)
    bake_pending(model)
    if "--preview" in argv:
        body.hide_render = True
        preview(model, arm, cid)
        for b in arm.pose.bones:
            b.matrix_basis = Matrix()
    os.makedirs(os.path.join(HERE, cid, "export"), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, cid, "export", cid + ".blend"))
    finish(model, arm, cid)


if __name__ == "__main__":
    main()
