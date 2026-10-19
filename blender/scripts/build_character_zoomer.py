"""Gen Z hero: oversized hoodie, wide jeans, chunky sneakers. Low-poly, textured, rigged, animated.

Builds blender/character_zoomer.blend and exports blender/export/character_zoomer.glb
(skinned mesh + two animations: Zoomer_SitDown and Zoomer_TypeIdle).
Textures come from tools/fal_textures.py (textures/character/*.png).

Character space: metres, origin on the floor under the chair centre, faces -Y (like the hero's
chair in Scene 01). Put the actor at the chair, same orientation.

Run from the repo root:
    /Applications/Blender.app/Contents/MacOS/Blender -b -P blender/scripts/build_character_zoomer.py
"""
import math
import os
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
TEX = os.path.join(ROOT, "textures", "character")
BLEND = os.path.join(ROOT, "blender", "character_zoomer.blend")
GLB = os.path.join(ROOT, "blender", "export", "character_zoomer.glb")

FPS = 24
# Seat geometry of the hero's chair and desk (see build_scene01_corridor.py), relative to the chair centre.
HIP_Z_SIT = 0.605         # hip joint height when sitting on the seat (seat top is ~0.51)
KEYS_Y = -0.66            # keyboard centre
KEYS_Z = 0.81             # key tops

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.fps = FPS


# ------------------------------------------------------------------ materials
def image_mat(name, png, rough=0.9):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = rough
    if png:
        tex = m.node_tree.nodes.new("ShaderNodeTexImage")
        tex.image = bpy.data.images.load(os.path.join(TEX, png))
        m.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    return m


def flat_mat(name, color, rough=0.4):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = rough
    return m


MATS = {
    "skin": image_mat("Zoomer_Skin", "face.png", 0.7),
    "hoodie": image_mat("Zoomer_Hoodie", "hoodie.png", 0.95),
    "denim": image_mat("Zoomer_Denim", "denim.png", 0.85),
    "shoes": image_mat("Zoomer_Sneakers", "sneakers.png", 0.55),
    "hair": image_mat("Zoomer_Hair", "hair.png", 0.6),
    "plastic": flat_mat("Zoomer_Earbuds", (0.9, 0.9, 0.92), 0.3),
}


# ------------------------------------------------------------------ geometry
def loft(name, rings, mat, seg=20, tile=0.45, axis="z", caps=(True, True)):
    """Lofts ellipse rings. ring = (pos along axis, radius a, radius b, centre a, centre b).
    axis z: a=x, b=y. axis y: a=x, b=z."""
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new("UVMap")

    def pt(r, ang):
        t, ra, rb, ca, cb = r
        a, b = ca + ra * math.cos(ang), cb + rb * math.sin(ang)
        return Vector((a, b, t)) if axis == "z" else Vector((a, t, b))

    grid = [[bm.verts.new(pt(r, 2 * math.pi * i / seg)) for i in range(seg)] for r in rings]
    arc = [0.0]
    for k in range(1, len(rings)):
        dt = rings[k][0] - rings[k - 1][0]
        dr = (rings[k][1] + rings[k][2] - rings[k - 1][1] - rings[k - 1][2]) / 2
        arc.append(arc[-1] + math.hypot(dt, dr))
    circ = [2 * math.pi * math.sqrt((r[1] ** 2 + r[2] ** 2) / 2) for r in rings]
    for k in range(len(rings) - 1):
        for i in range(seg):
            f = bm.faces.new([grid[k][i], grid[k][(i + 1) % seg], grid[k + 1][(i + 1) % seg], grid[k + 1][i]])
            uvs = [(i * circ[k] / seg, arc[k]), ((i + 1) * circ[k] / seg, arc[k]),
                   ((i + 1) * circ[k + 1] / seg, arc[k + 1]), (i * circ[k + 1] / seg, arc[k + 1])]
            for lp, (u, v) in zip(f.loops, uvs):
                lp[uvl].uv = (u / tile, v / tile)
    for end, on in ((0, caps[0]), (-1, caps[1])):
        if not on:
            continue
        r = rings[end]
        c = bm.verts.new(pt((r[0], 0, 0, r[3], r[4]), 0))
        row = grid[end]
        for i in range(seg):
            f = bm.faces.new([c, row[(i + 1) % seg], row[i]])
            for lp in f.loops:
                lp[uvl].uv = (lp.vert.co.x / tile, lp.vert.co.y / tile)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for f in bm.faces:
        f.smooth = True
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    me.materials.append(MATS[mat])
    ob = bpy.data.objects.new(name, me)
    scene.collection.objects.link(ob)
    return ob


def mirror_x(name, ob):
    """Copy of ob mirrored across x=0 (bm flips normals back)."""
    me = ob.data.copy()
    bm = bmesh.new()
    bm.from_mesh(me)
    for v in bm.verts:
        v.co.x = -v.co.x
    bmesh.ops.reverse_faces(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    cp = bpy.data.objects.new(name, me)
    scene.collection.objects.link(cp)
    return cp


def set_uv(ob, fn):
    """fn(vertex co) -> (u, v), applied per loop."""
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    uvl = bm.loops.layers.uv.active
    for f in bm.faces:
        for lp in f.loops:
            lp[uvl].uv = fn(lp.vert.co)
    bm.to_mesh(ob.data)
    bm.free()


SKIN_PLAIN_UV = (0.04, 0.5)


def smooth(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3 - 2 * t)


def below(z, joint, h):
    """0 above the joint window, 1 below it."""
    return smooth((joint + h - z) / (2 * h))


def weights(ob, fn):
    for v in ob.data.vertices:
        ws = fn(v.co)
        total = sum(ws.values()) or 1.0
        for bone, w in ws.items():
            w /= total
            if w > 1e-4:
                g = ob.vertex_groups.get(bone) or ob.vertex_groups.new(name=bone)
                g.add([v.index], w, "REPLACE")


parts = []


def build_body():
    # ---- torso (hoodie body) and the hood hanging at the back
    torso = loft("Hoodie_Body", [
        (0.84, 0.275, 0.185, 0, 0.0), (0.88, 0.268, 0.178, 0, 0.0), (1.00, 0.255, 0.165, 0, 0.0),
        (1.12, 0.26, 0.165, 0, 0.0), (1.26, 0.265, 0.165, 0, 0.0), (1.36, 0.255, 0.155, 0, 0.0),
        (1.42, 0.20, 0.13, 0, 0.0), (1.455, 0.115, 0.095, 0, 0.0)], "hoodie", seg=24)
    weights(torso, lambda c: {
        "pelvis": below(c.z, 1.08, 0.06),
        "spine": (1 - below(c.z, 1.08, 0.06)) * below(c.z, 1.24, 0.06),
        "chest": (1 - below(c.z, 1.24, 0.06)) * (1 - smooth((c.z - 1.40) / 0.06)),
        "neck": (1 - below(c.z, 1.24, 0.06)) * smooth((c.z - 1.40) / 0.06)})
    hood = loft("Hoodie_Hood", [
        (1.28, 0.17, 0.06, 0, 0.145), (1.36, 0.19, 0.075, 0, 0.14), (1.44, 0.17, 0.09, 0, 0.115),
        (1.51, 0.13, 0.095, 0, 0.075), (1.545, 0.09, 0.07, 0, 0.045)], "hoodie", seg=20)
    weights(hood, lambda c: {"chest": 1 - smooth((c.z - 1.40) / 0.1), "neck": smooth((c.z - 1.40) / 0.1)})
    cuff = loft("Hoodie_Collar", [(1.42, 0.115, 0.095, 0, 0.0), (1.47, 0.092, 0.078, 0, 0.0), (1.50, 0.085, 0.072, 0, -0.005)],
                "hoodie", seg=20, caps=(False, False))
    weights(cuff, lambda c: {"neck": 1.0})
    parts.extend([torso, hood, cuff])

    # ---- sleeves: one loft per arm, bent by the weights around elbow and wrist
    def arm_rings():
        return [(1.44, 0.075, 0.08, 0.245, 0.0), (1.40, 0.092, 0.092, 0.285, 0.0), (1.30, 0.092, 0.092, 0.31, 0.0),
                (1.16, 0.085, 0.085, 0.31, 0.0), (1.10, 0.088, 0.088, 0.31, 0.0), (1.04, 0.086, 0.086, 0.31, 0.0),
                (0.92, 0.088, 0.088, 0.31, 0.0), (0.84, 0.092, 0.09, 0.31, 0.0), (0.80, 0.078, 0.076, 0.31, 0.0)]
    arm = loft("Sleeve.L", arm_rings(), "hoodie", seg=16, tile=0.45)
    weights(arm, lambda c: {
        "upper_arm.L": 1 - below(c.z, 1.10, 0.07),
        "forearm.L": below(c.z, 1.10, 0.07)})
    # hand: mitten, thin in x because the palm faces the thigh while the arm hangs
    hand = loft("Hand.L", [(0.87, 0.022, 0.040, 0.31, 0.0), (0.80, 0.025, 0.046, 0.31, 0.0), (0.72, 0.021, 0.048, 0.31, 0.0),
                           (0.64, 0.015, 0.043, 0.31, 0.0), (0.615, 0.007, 0.03, 0.31, 0.0)], "skin", seg=12)
    weights(hand, lambda c: {"hand.L": 1.0})
    set_uv(hand, lambda c: SKIN_PLAIN_UV)
    # the loft ring a is x, b is y: hand thickness 'a' (x) is thin, width 'b' (y) wide
    parts.extend([arm, hand])
    parts.append(mirror_x("Sleeve.R", arm))
    hand_r = mirror_x("Hand.R", hand)
    parts.append(hand_r)
    for ob in (parts[-2], parts[-1]):
        for g in list(ob.vertex_groups):
            ob.vertex_groups.remove(g)
    weights(parts[-2], lambda c: {
        "upper_arm.R": 1 - below(c.z, 1.10, 0.07),
        "forearm.R": below(c.z, 1.10, 0.07)})
    weights(parts[-1], lambda c: {"hand.R": 1.0})
    # ---- jeans legs
    def leg_rings():
        return [(1.00, 0.135, 0.125, 0.105, 0.0), (0.92, 0.14, 0.13, 0.105, 0.0), (0.72, 0.135, 0.125, 0.105, 0.0),
                (0.56, 0.13, 0.12, 0.105, 0.0), (0.50, 0.132, 0.122, 0.105, 0.0), (0.44, 0.13, 0.12, 0.105, 0.0),
                (0.28, 0.128, 0.122, 0.105, 0.0), (0.12, 0.13, 0.13, 0.108, 0.0), (0.05, 0.138, 0.14, 0.11, -0.01)]
    leg = loft("Jeans.L", leg_rings(), "denim", seg=18, tile=0.5, caps=(True, False))
    weights(leg, lambda c: {
        "pelvis": smooth((c.z - 0.95) / 0.08),
        "thigh.L": (1 - smooth((c.z - 0.95) / 0.08)) * (1 - below(c.z, 0.50, 0.08)),
        "shin.L": below(c.z, 0.50, 0.08) * (1 - below(c.z, 0.10, 0.03)),
        "foot.L": below(c.z, 0.10, 0.03) * 0.35})
    parts.append(leg)
    leg_r = mirror_x("Jeans.R", leg)
    for g in list(leg_r.vertex_groups):
        leg_r.vertex_groups.remove(g)
    weights(leg_r, lambda c: {
        "pelvis": smooth((c.z - 0.95) / 0.08),
        "thigh.R": (1 - smooth((c.z - 0.95) / 0.08)) * (1 - below(c.z, 0.50, 0.08)),
        "shin.R": below(c.z, 0.50, 0.08) * (1 - below(c.z, 0.10, 0.03)),
        "foot.R": below(c.z, 0.10, 0.03) * 0.35})
    parts.append(leg_r)
    # ---- sneakers: chunky, toe towards -Y
    shoe = loft("Sneaker.L", [(0.095, 0.042, 0.048, 0.105, 0.055), (0.07, 0.054, 0.056, 0.105, 0.058),
                              (0.0, 0.062, 0.058, 0.105, 0.06), (-0.10, 0.062, 0.052, 0.105, 0.055),
                              (-0.18, 0.052, 0.04, 0.105, 0.045), (-0.225, 0.03, 0.028, 0.105, 0.036)],
                "shoes", seg=16, tile=0.14, axis="y")
    weights(shoe, lambda c: {"foot.L": 1.0})
    parts.append(shoe)
    shoe_r = mirror_x("Sneaker.R", shoe)
    for g in list(shoe_r.vertex_groups):
        shoe_r.vertex_groups.remove(g)
    weights(shoe_r, lambda c: {"foot.R": 1.0})
    parts.append(shoe_r)
    # ---- neck and head
    neck = loft("Neck", [(1.40, 0.05, 0.05, 0, 0.0), (1.47, 0.045, 0.047, 0, 0.0), (1.53, 0.045, 0.05, 0, -0.005)], "skin", seg=12)
    set_uv(neck, lambda c: SKIN_PLAIN_UV)
    weights(neck, lambda c: {"neck": 1 - smooth((c.z - 1.46) / 0.05), "head": smooth((c.z - 1.46) / 0.05)})
    head_rings = [(1.500, 0.040, 0.050, 0, -0.012), (1.525, 0.072, 0.074, 0, -0.008), (1.570, 0.094, 0.098, 0, 0.0),
                  (1.625, 0.104, 0.108, 0, 0.003), (1.680, 0.100, 0.106, 0, 0.005), (1.725, 0.080, 0.088, 0, 0.005),
                  (1.752, 0.045, 0.055, 0, 0.005), (1.762, 0.01, 0.015, 0, 0.005)]
    head = loft("Head", head_rings, "skin", seg=28)

    def face_uv(c):
        if c.y > 0.0:
            return SKIN_PLAIN_UV
        return (0.5 + c.x / 0.263, 0.55 + (c.z - 1.625) / 0.263)
    set_uv(head, face_uv)
    weights(head, lambda c: {"head": 1.0})
    nose = loft("Nose", [(1.585, 0.002, 0.002, 0, -0.108), (1.598, 0.011, 0.012, 0, -0.105), (1.612, 0.012, 0.016, 0, -0.103),
                         (1.626, 0.006, 0.01, 0, -0.1)], "skin", seg=10)
    set_uv(nose, lambda c: SKIN_PLAIN_UV)
    weights(nose, lambda c: {"head": 1.0})
    ear = loft("Ear.L", [(1.60, 0.004, 0.012, 0.104, 0.005), (1.62, 0.009, 0.018, 0.106, 0.005), (1.64, 0.004, 0.012, 0.104, 0.005)],
               "skin", seg=8)
    set_uv(ear, lambda c: SKIN_PLAIN_UV)
    weights(ear, lambda c: {"head": 1.0})
    bud = loft("Earbud.L", [(1.615, 0.005, 0.005, 0.112, 0.0), (1.622, 0.009, 0.009, 0.114, -0.002),
                            (1.63, 0.004, 0.004, 0.113, -0.002), (1.575, 0.003, 0.003, 0.113, -0.003)], "plastic", seg=8)
    weights(bud, lambda c: {"head": 1.0})
    for ob in (ear, bud):
        parts.append(ob)
        m = mirror_x(ob.name.replace(".L", ".R"), ob)
        for g in list(m.vertex_groups):
            m.vertex_groups.remove(g)
        weights(m, lambda c: {"head": 1.0})
        parts.append(m)
    parts.extend([neck, head, nose])
    # ---- hair: a messy shell over the back and top of the head, forehead stays open
    rnd = random.Random(7)
    shell = loft("Hair", [(r[0] + (0.004 if i else -0.008), r[1] + 0.013, r[2] + 0.015, r[3], r[4]) for i, r in enumerate(head_rings)],
                 "hair", seg=28, tile=0.4)
    bm = bmesh.new()
    bm.from_mesh(shell.data)
    kill = [f for f in bm.faces if (f.calc_center_median().y < -0.03 and f.calc_center_median().z < 1.678)
            or f.calc_center_median().z < 1.54]
    bmesh.ops.delete(bm, geom=kill, context="FACES")
    for v in bm.verts:
        if v.co.z > 1.55:
            n = Vector((v.co.x, v.co.y, 0)).normalized()
            v.co += n * rnd.uniform(-0.004, 0.012) + Vector((0, 0, rnd.uniform(-0.004, 0.004)))
    bm.to_mesh(shell.data)
    bm.free()
    set_uv(shell, lambda c: (math.atan2(c.y, c.x) / math.pi * 1.2, c.z * 3.0))
    weights(shell, lambda c: {"head": 1.0})
    parts.append(shell)


# ------------------------------------------------------------------ armature
BONES = [  # name, parent, head, tail, connected
    ("root", None, (0, 0, 0), (0, 0, 0.12), False),
    ("pelvis", "root", (0, 0, 0.95), (0, 0, 1.08), False),
    ("spine", "pelvis", (0, 0, 1.08), (0, 0, 1.24), True),
    ("chest", "spine", (0, 0, 1.24), (0, 0, 1.40), True),
    ("neck", "chest", (0, 0, 1.40), (0, 0, 1.48), True),
    ("head", "neck", (0, 0, 1.48), (0, 0, 1.76), True),
]
for s, sx in ((".L", 1), (".R", -1)):
    BONES += [
        ("upper_arm" + s, "chest", (0.31 * sx, 0, 1.40), (0.31 * sx, 0, 1.10), False),
        ("forearm" + s, "upper_arm" + s, (0.31 * sx, 0, 1.10), (0.31 * sx, 0, 0.84), True),
        ("hand" + s, "forearm" + s, (0.31 * sx, 0, 0.84), (0.31 * sx, 0, 0.62), True),
        ("thigh" + s, "pelvis", (0.105 * sx, 0, 0.95), (0.105 * sx, 0, 0.50), False),
        ("shin" + s, "thigh" + s, (0.105 * sx, 0, 0.50), (0.105 * sx, 0, 0.10), True),
        ("foot" + s, "shin" + s, (0.105 * sx, 0, 0.10), (0.105 * sx, -0.18, 0.045), True),
    ]


def build_armature():
    data = bpy.data.armatures.new("Zoomer")
    arm = bpy.data.objects.new("Zoomer_Armature", data)
    scene.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    for name, parent, head, tail, conn in BONES:
        b = data.edit_bones.new(name)
        b.head, b.tail = Vector(head), Vector(tail)
        if parent:
            b.parent = data.edit_bones[parent]
            b.use_connect = conn
    bpy.ops.object.mode_set(mode="OBJECT")
    return arm


# ------------------------------------------------------------------ posing
def pitch(deg):
    a = math.radians(deg)
    return Vector((0, -math.sin(a), math.cos(a)))


def solve2(root, target, l1, l2, pole):
    """Two-bone IK: returns the middle joint position."""
    d = target - root
    dist = min(max(d.length, abs(l1 - l2) + 1e-4), l1 + l2 - 1e-4)
    dirn = d.normalized()
    a = (l1 * l1 - l2 * l2 + dist * dist) / (2 * dist)
    h = math.sqrt(max(l1 * l1 - a * a, 0))
    side = (pole - dirn * pole.dot(dirn))
    side = side.normalized() if side.length > 1e-6 else Vector((0, 0, 1))
    return root + dirn * a + side * h


class Rig:
    def __init__(self, arm):
        self.arm = arm
        self.pb = arm.pose.bones
        self.prev_q = {}

    def update(self):
        bpy.context.view_layer.update()

    def head_of(self, pb):
        if pb.parent is None:
            return pb.bone.head_local.copy()
        rel = pb.parent.bone.matrix_local.inverted() @ pb.bone.head_local
        return pb.parent.matrix @ rel

    def aim(self, name, d, twist=0.0, head=None, normal=None, target_normal=None):
        """Points bone `name` along world direction d. Optionally rolls it so the bone-local
        axis `normal` (rest frame) faces `target_normal`."""
        pb = self.pb[name]
        rest = pb.bone.matrix_local.to_3x3()
        d = d.normalized()
        y0 = rest @ Vector((0, 1, 0))
        R = y0.rotation_difference(d).to_matrix() @ rest
        if normal is not None:
            n = R @ normal
            tn = target_normal - d * target_normal.dot(d)
            n = n - d * n.dot(d)
            ang = math.atan2(d.dot(n.cross(tn)), n.dot(tn))
            R = Matrix.Rotation(ang, 3, d) @ R
        if twist:
            R = Matrix.Rotation(twist, 3, d) @ R
        pb.matrix = Matrix.Translation(head if head is not None else self.head_of(pb)) @ R.to_4x4()
        self.update()

    def tail_of(self, name):
        pb = self.pb[name]
        return pb.matrix @ Vector((0, pb.bone.length, 0))

    def key(self, frame):
        for pb in self.pb:
            q = pb.rotation_quaternion
            p = self.prev_q.get(pb.name)
            if p is not None and q.dot(p) < 0:
                q.negate()
            self.prev_q[pb.name] = q.copy()
            pb.keyframe_insert("rotation_quaternion", frame=frame)
            if pb.name in ("pelvis", "root"):
                pb.keyframe_insert("location", frame=frame)


def lerp(a, b, t):
    return a + (b - a) * t


def pose_character(rig, p):
    """p: dict of pose parameters, all in character space."""
    pelvis_pos = p["pelvis"]
    rig.aim("root", Vector((0, 0, 1)))
    rig.aim("pelvis", pitch(p["pelvis_pitch"]), head=pelvis_pos)
    rig.aim("spine", pitch(p["spine_pitch"]))
    rig.aim("chest", pitch(p["chest_pitch"]))
    rig.aim("neck", pitch(p["neck_pitch"]))
    rig.aim("head", pitch(p["head_pitch"]), twist=p.get("head_yaw", 0.0))
    for s, sx in ((".L", 1), (".R", -1)):
        # arms
        sh = rig.head_of(rig.pb["upper_arm" + s])
        wrist = Vector(p["wrist" + s])
        elbow = solve2(sh, wrist, 0.30, 0.26, Vector((0.5 * sx, 0.9, -0.3)))
        rig.aim("upper_arm" + s, elbow - sh)
        rig.aim("forearm" + s, wrist - elbow)
        hd = Vector(p["hand_dir" + s])
        # palm normal: rest hand is thin in x, palm faces inward (-sx)
        rig.aim("hand" + s, hd, normal=rig.pb["hand" + s].bone.matrix_local.to_3x3().inverted() @ Vector((-sx, 0, 0)),
                target_normal=Vector(p["palm" + s]))
        # legs
        hip = rig.head_of(rig.pb["thigh" + s])
        ankle = Vector(p["ankle" + s])
        knee = solve2(hip, ankle, 0.45, 0.40, Vector((0, -1, 0.15)))
        rig.aim("thigh" + s, knee - hip)
        rig.aim("shin" + s, ankle - knee)
        rig.aim("foot" + s, Vector(p["toe_dir" + s]))


def stand_pose(px=0.0, py=0.0):
    return dict(
        pelvis=Vector((px, py, 0.95)), pelvis_pitch=0, spine_pitch=0, chest_pitch=0, neck_pitch=0, head_pitch=0,
        **{"wrist.L": (px + 0.315, py - 0.01, 0.855), "wrist.R": (px - 0.315, py - 0.01, 0.855),
           "hand_dir.L": Vector((0, -0.05, -1)), "hand_dir.R": Vector((0, -0.05, -1)),
           "palm.L": (-1, 0, 0), "palm.R": (1, 0, 0),
           "ankle.L": (px + 0.105, py, 0.10), "ankle.R": (px - 0.105, py, 0.10),
           "toe_dir.L": Vector((0, -0.97, -0.25)), "toe_dir.R": Vector((0, -0.97, -0.25))})


def sit_pose(t=0.0):
    """Seated, hunched towards the keyboard, hands on the keys. t is a small typing phase."""
    bob = math.sin(t * 2 * math.pi)
    bob2 = math.sin(t * 2 * math.pi + 2.2)
    return dict(
        pelvis=Vector((0, 0.09, HIP_Z_SIT + 0.004 * math.sin(t * 2 * math.pi))), pelvis_pitch=-4, spine_pitch=12, chest_pitch=20,
        neck_pitch=30, head_pitch=14 + 1.0 * bob2,
        **{"wrist.L": (0.115, KEYS_Y + 0.14, KEYS_Z + 0.045 + 0.007 * max(bob, 0)),
           "wrist.R": (-0.115, KEYS_Y + 0.14, KEYS_Z + 0.045 + 0.007 * max(bob2, 0)),
           "hand_dir.L": Vector((0.05, -1, -0.30 - 0.1 * bob)), "hand_dir.R": Vector((-0.05, -1, -0.30 - 0.1 * bob2)),
           "palm.L": (0, 0, -1), "palm.R": (0, 0, -1),
           "ankle.L": (0.115, -0.50, 0.10), "ankle.R": (-0.115, -0.50, 0.10),
           "toe_dir.L": Vector((0, -0.97, -0.25)), "toe_dir.R": Vector((0, -0.97, -0.25))})


def blend(a, b, t):
    out = {}
    for k in a:
        va, vb = a[k], b[k]
        out[k] = va.lerp(vb, t) if isinstance(va, Vector) else (
            tuple(lerp(x, y, t) for x, y in zip(va, vb)) if isinstance(va, tuple) else lerp(va, vb, t))
    return out


def new_action(rig, name):
    ad = rig.arm.animation_data_create()
    act = bpy.data.actions.new(name)
    ad.action = act
    rig.prev_q.clear()
    for pb in rig.pb:
        pb.rotation_mode = "QUATERNION"
    return act


def lift(p, side, amount):
    """Raise the foot of `side` ('.L'/'.R') by amount metres."""
    a = list(p["ankle" + side])
    a[2] += amount
    p["ankle" + side] = tuple(a)


def anim_sit_down(rig):
    act = new_action(rig, "Zoomer_SitDown")
    # Key poses. The character starts standing just beside the chair, sidesteps over it, lowers and reaches out.
    stand = stand_pose(-0.55, 0.0)   # starts on the sitter's right (-X), where the player stands
    over = stand_pose(0.0, 0.09)
    # hunch slightly while going down
    drop = sit_pose(0.0)
    drop["wrist.L"] = (0.20, -0.20, 0.80)
    drop["wrist.R"] = (-0.20, -0.20, 0.80)
    drop["hand_dir.L"] = Vector((0, -0.3, -1))
    drop["hand_dir.R"] = Vector((0, -0.3, -1))
    drop["palm.L"] = (-1, 0, 0)
    drop["palm.R"] = (1, 0, 0)
    reach = sit_pose(0.0)
    reach["wrist.L"] = (0.14, KEYS_Y + 0.17, KEYS_Z + 0.12)
    reach["wrist.R"] = (-0.14, KEYS_Y + 0.17, KEYS_Z + 0.12)
    reach["palm.L"] = (-0.6, 0, -0.8)
    reach["palm.R"] = (0.6, 0, -0.8)
    seated = sit_pose(0.0)
    T = [(0, stand), (8, stand), (30, over), (54, drop), (72, reach), (92, seated), (110, seated)]
    rig.arm.animation_data.action = act
    for f in range(0, 111, 2):
        # pick segment
        for (f0, p0), (f1, p1) in zip(T, T[1:]):
            if f0 <= f <= f1:
                t = smooth((f - f0) / max(f1 - f0, 1))
                p = blend(p0, p1, t)
                break
        # footwork: lift the foot on the way in, the trailing foot follows
        if 8 <= f <= 30:
            k = math.sin((f - 8) / 22 * math.pi)
            lift(p, ".R", 0.10 * k)
        if 54 <= f <= 90:
            pass
        pose_character(rig, p)
        rig.key(f)
    act.use_fake_user = True
    return act


def anim_type_idle(rig):
    act = new_action(rig, "Zoomer_TypeIdle")
    n = 48
    for f in range(0, n + 1, 2):
        pose_character(rig, sit_pose((f % n) / n))
        rig.key(f)
    act.use_fake_user = True
    return act


# ------------------------------------------------------------------ main
def main():
    build_body()
    arm = build_armature()
    bpy.ops.object.select_all(action="DESELECT")
    for ob in parts:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    body = bpy.context.view_layer.objects.active
    body.name = "Zoomer_Mesh"
    # neck/hand skin UV loops and tiling stay as built; add the armature
    body.parent = arm
    mod = body.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    for m in body.data.polygons:
        m.use_smooth = True
    # unused groups (bones that never got verts) are fine

    rig = Rig(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="POSE")
    sit = anim_sit_down(rig)
    idle = anim_type_idle(rig)
    bpy.ops.object.mode_set(mode="OBJECT")
    arm.animation_data.action = idle
    os.makedirs(os.path.dirname(GLB), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=BLEND)
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.export_scene.gltf(filepath=GLB, export_format="GLB", use_selection=True, export_animations=True,
                              export_animation_mode="ACTIONS", export_skins=True, export_apply=False,
                              export_yup=True, export_image_format="AUTO")
    print("done:", GLB)


main()
