"""A stand-in for the clips' body (Guy_a) where its glb isn't there (the cloud: blender/characters/*/export is local
only): SK_Human_Skeleton's 56 bone names and hierarchy, a T-pose, an adult man's proportions (1.78 m, the hips 0.885 m,
the pelvis bone 0.94 m like Guy_a's 93.7 cm), and a clay body of capsules, each part bound rigidly to its bone, so a
hand-made clip (sail_clips.py, poser.py) can be posed, looked at and checked for limbs through things before it is
baked on the real body on the Mac.

Finger bones are rolled so a turn about their local +X curls them toward the palm (poser.Poser.curl), like the cast's.
Blender character space: faces -Y, its left is +X, up +Z; metres.

    import standin_rig; arm = standin_rig.build()      (in an empty scene)
"""
import math

import bpy
import bmesh
from mathutils import Matrix, Vector

# joint positions (left side; the right is mirrored), metres
J = {
    "root": (0.0, 0.0, 0.0),
    "pelvis": (0.0, 0.0, 0.94),
    "spine_01": (0.0, 0.0, 0.99),
    "spine_02": (0.0, 0.005, 1.10),
    "spine_03": (0.0, 0.01, 1.24),
    "neck_01": (0.0, 0.015, 1.45),
    "head": (0.0, 0.0, 1.55),
    "head_end": (0.0, 0.0, 1.76),
    "jaw": (0.0, -0.03, 1.56),
    "eye_l": (0.032, -0.085, 1.635),
    "clavicle_l": (0.03, -0.0, 1.40),
    "upperarm_l": (0.175, 0.01, 1.42),
    "lowerarm_l": (0.46, 0.01, 1.42),
    "hand_l": (0.715, 0.01, 1.42),
    "thigh_l": (0.09, 0.0, 0.885),
    "calf_l": (0.095, -0.005, 0.48),
    "foot_l": (0.10, 0.03, 0.08),
    "ball_l": (0.105, -0.11, 0.022),
    "ball_end_l": (0.105, -0.17, 0.02),
}
# fingers: base offset from the wrist (x out along the hand, y toward the back (+Y), z up) and segment lengths
FING = {
    "index": ((0.092, -0.026, 0.0), (0.042, 0.026, 0.021)),
    "middle": ((0.095, -0.006, 0.002), (0.046, 0.029, 0.023)),
    "ring": ((0.091, 0.013, 0.0), (0.043, 0.027, 0.021)),
    "pinky": ((0.084, 0.03, -0.004), (0.034, 0.021, 0.018)),
}
THUMB = ((0.025, -0.03, -0.012), (0.038, 0.032, 0.026))
THUMB_DIR = Vector((0.55, -0.75, -0.3)).normalized()


def _mirror(v):
    return Vector((-v[0], v[1], v[2]))


def build():
    arm_data = bpy.data.armatures.new("SK_Standin")
    arm = bpy.data.objects.new("Standin", arm_data)
    bpy.context.scene.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    eb = arm_data.edit_bones

    def bone(name, head, tail, parent=None, roll_z=None):
        b = eb.new(name)
        b.head, b.tail = Vector(head), Vector(tail)
        if roll_z is not None:
            b.align_roll(Vector(roll_z))
        if parent:
            b.parent = eb[parent]
            b.use_connect = False
        return b

    up = Vector((0, 0, 0.08))
    bone("root", J["root"], Vector(J["root"]) + Vector((0, 0.1, 0)))
    bone("pelvis", J["pelvis"], J["spine_01"], "root", roll_z=(0, -1, 0))
    bone("spine_01", J["spine_01"], J["spine_02"], "pelvis", roll_z=(0, -1, 0))
    bone("spine_02", J["spine_02"], J["spine_03"], "spine_01", roll_z=(0, -1, 0))
    bone("spine_03", J["spine_03"], J["neck_01"], "spine_02", roll_z=(0, -1, 0))
    bone("neck_01", J["neck_01"], J["head"], "spine_03", roll_z=(0, -1, 0))
    bone("head", J["head"], J["head_end"], "neck_01", roll_z=(0, -1, 0))
    bone("jaw", J["jaw"], Vector(J["jaw"]) + Vector((0, -0.08, -0.03)), "head")
    for s, mir in (("l", lambda v: Vector(v)), ("r", _mirror)):
        side = 1.0 if s == "l" else -1.0
        bone("eye_" + s, mir(J["eye_l"]), mir(J["eye_l"]) + Vector((0, -0.02, 0)), "head")
        bone("clavicle_" + s, mir(J["clavicle_l"]), mir(J["upperarm_l"]), "spine_03", roll_z=(0, -1, 0))
        bone("upperarm_" + s, mir(J["upperarm_l"]), mir(J["lowerarm_l"]), "clavicle_" + s, roll_z=(0, -1, 0))
        bone("lowerarm_" + s, mir(J["lowerarm_l"]), mir(J["hand_l"]), "upperarm_" + s, roll_z=(0, -1, 0))
        wrist = mir(J["hand_l"])
        bone("hand_" + s, wrist, wrist + Vector((side * 0.095, -0.006, 0.002)), "lowerarm_" + s, roll_z=(0, 0, -1))
        for f, (base, segs) in FING.items():
            p = wrist + Vector((side * base[0], base[1], base[2]))
            parent = "hand_" + s
            d = Vector((side, 0.0, 0.0))
            for i, ln in enumerate(segs):
                # local X = d x palm(-Z): a turn about +X curls toward the palm; Z = X x Y
                x = d.cross(Vector((0, 0, -1))).normalized()
                z = x.cross(d)
                nm = "%s_0%d_%s" % (f, i + 1, s)
                bone(nm, p, p + d * ln, parent, roll_z=z)
                parent = nm
                p = p + d * ln
        p = wrist + Vector((side * THUMB[0][0], THUMB[0][1], THUMB[0][2]))
        d = Vector((side * THUMB_DIR.x, THUMB_DIR.y, THUMB_DIR.z))
        bend = Vector((side * 0.15, 0.55, -0.8)).normalized()          # toward the palm's middle, under the hand
        parent = "hand_" + s
        for i, ln in enumerate(THUMB[1]):
            x = d.cross(bend).normalized()
            z = x.cross(d)
            nm = "thumb_0%d_%s" % (i + 1, s)
            bone(nm, p, p + d * ln, parent, roll_z=z)
            parent = nm
            p = p + d * ln
        bone("thigh_" + s, mir(J["thigh_l"]), mir(J["calf_l"]), "pelvis", roll_z=(0, -1, 0))
        bone("calf_" + s, mir(J["calf_l"]), mir(J["foot_l"]), "thigh_" + s, roll_z=(0, -1, 0))
        bone("foot_" + s, mir(J["foot_l"]), mir(J["ball_l"]), "calf_" + s, roll_z=(0, 0, 1))
        bone("ball_" + s, mir(J["ball_l"]), mir(J["ball_end_l"]), "foot_" + s, roll_z=(0, 0, 1))
    bpy.ops.object.mode_set(mode="OBJECT")
    _body(arm)
    return arm


def _body(arm):
    """Clay capsules on the bones, one mesh, each part weighted wholly to its bone."""
    bm = bmesh.new()
    dl = bm.verts.layers.deform.verify()
    names = [b.name for b in arm.data.bones]
    heads = {b.name: b.head_local.copy() for b in arm.data.bones}
    tails = {b.name: b.tail_local.copy() for b in arm.data.bones}

    def add(geom_verts, bone):
        gi = names.index(bone)
        for v in geom_verts:
            v[dl][gi] = 1.0

    def capsule(a, b, r0, r1, bone, flat=1.0, seg=12):
        a, b = Vector(a), Vector(b)
        d = b - a
        g = bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=r0, radius2=r1, depth=max(d.length, 1e-3))
        q = d.to_track_quat("Z", "Y")
        m = Matrix.Translation((a + b) * 0.5) @ q.to_matrix().to_4x4() @ Matrix.Diagonal((1.0, flat, 1.0, 1.0))
        bmesh.ops.transform(bm, matrix=m, verts=g["verts"])
        add(g["verts"], bone)
        for c, r in ((a, r0), (b, r1)):
            s = bmesh.ops.create_uvsphere(bm, u_segments=seg, v_segments=seg // 2, radius=r)
            bmesh.ops.transform(bm, matrix=Matrix.Translation(c) @ q.to_matrix().to_4x4() @ Matrix.Diagonal((1.0, flat, 1.0, 1.0)), verts=s["verts"])
            add(s["verts"], bone)

    def blob(c, size, bone, seg=14):
        s = bmesh.ops.create_uvsphere(bm, u_segments=seg, v_segments=seg // 2, radius=1.0)
        bmesh.ops.transform(bm, matrix=Matrix.Translation(Vector(c)) @ Matrix.Diagonal((size[0], size[1], size[2], 1.0)), verts=s["verts"])
        add(s["verts"], bone)

    blob((0, 0.01, 0.92), (0.165, 0.11, 0.11), "pelvis")
    blob((0, 0.0, 1.05), (0.15, 0.10, 0.10), "spine_01")
    blob((0, 0.0, 1.19), (0.16, 0.105, 0.12), "spine_02")
    blob((0, 0.01, 1.33), (0.175, 0.11, 0.12), "spine_03")
    capsule((0, 0.015, 1.43), (0, 0.01, 1.55), 0.055, 0.05, "neck_01")
    blob((0, -0.005, 1.65), (0.085, 0.105, 0.115), "head")
    blob((0, -0.105, 1.63), (0.016, 0.03, 0.02), "head")                  # the nose: which way the face looks
    for s in "lr":
        sign = 1.0 if s == "l" else -1.0
        capsule(heads["clavicle_" + s] + Vector((0, 0.01, 0)), heads["upperarm_" + s], 0.05, 0.055, "clavicle_" + s)
        capsule(heads["upperarm_" + s], heads["lowerarm_" + s], 0.055, 0.042, "upperarm_" + s)
        capsule(heads["lowerarm_" + s], heads["hand_" + s], 0.04, 0.03, "lowerarm_" + s, flat=0.85)
        w = heads["hand_" + s]
        blob(w + Vector((sign * 0.05, -0.004, 0.0)), (0.05, 0.042, 0.016), "hand_" + s)
        for f in ("index", "middle", "ring", "pinky", "thumb"):
            for i in (1, 2, 3):
                nm = "%s_0%d_%s" % (f, i, s)
                r = 0.0095 if f != "pinky" else 0.008
                if f == "thumb":
                    r = 0.011
                capsule(heads[nm], tails[nm], r, r * 0.92, nm, seg=8)
        capsule(heads["thigh_" + s], heads["calf_" + s], 0.08, 0.055, "thigh_" + s)
        capsule(heads["calf_" + s], heads["foot_" + s], 0.05, 0.035, "calf_" + s)
        capsule(heads["foot_" + s] + Vector((0, 0.03, -0.03)), heads["ball_" + s] + Vector((0, 0, -0.0)), 0.04, 0.03, "foot_" + s, flat=0.7)
        capsule(heads["ball_" + s], tails["ball_" + s], 0.03, 0.025, "ball_" + s, flat=0.7)
    me = bpy.data.meshes.new("StandinBody")
    o = bpy.data.objects.new("StandinBody", me)
    for n in names:
        o.vertex_groups.new(name=n)
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = True
    bpy.context.scene.collection.objects.link(o)
    o.parent = arm
    o.modifiers.new("Armature", "ARMATURE").object = arm
    return o
