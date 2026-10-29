"""Posing the clips' body (Guy_a, blender/humans/clips.json target) by hand in world space, for the hand-made clips
(sit_pc.py; sleep_clips.py has its own copy of the basics): turn and aim bones about world axes (a bone turns its
children with it), two-bone IK for the arms and legs with a pole, hands oriented by their palm, fingers curled; poses
snapshotted as each bone's rotation, keyed per frame, exported as a glb of NLA tracks for retarget_clips.py.

Blender character space: faces -Y, its left is +X, up +Z; metres.
"""
import math
import os

import bpy
from mathutils import Matrix, Quaternion, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
TARGET = os.path.join(ROOT, "blender", "characters", "guy_a", "export", "human_guy_a.glb")
FPS = 30
X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
FINGERS = ("index", "middle", "ring", "pinky")


def load(path=TARGET):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.render.fps = FPS
    bpy.ops.import_scene.gltf(filepath=path, bone_heuristic="BLENDER")
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    if arm.animation_data:
        arm.animation_data_clear()
    for a in list(bpy.data.actions):
        bpy.data.actions.remove(a)
    return arm


class Poser:
    def __init__(self, arm):
        self.arm = arm
        self.pb = arm.pose.bones
        self.rest_local = {}
        for b in arm.data.bones:
            self.rest_local[b.name] = (b.parent.matrix_local.inverted() @ b.matrix_local) if b.parent else b.matrix_local.copy()

    def reset(self):
        for b in self.pb:
            b.rotation_mode = "QUATERNION"
            b.matrix_basis = Matrix()
        bpy.context.view_layer.update()

    def head(self, name):
        bpy.context.view_layer.update()
        return (self.arm.matrix_world @ self.pb[name].matrix).translation.copy()

    def turn(self, name, deg, axis):
        """Turns a bone (and so its children) about a world axis through its joint."""
        if abs(deg) < 1e-6:
            return
        bpy.context.view_layer.update()
        b = self.pb[name]
        mw = b.matrix.copy()
        h = mw.translation.copy()
        b.matrix = Matrix.Translation(h) @ Matrix.Rotation(math.radians(deg), 4, Vector(axis).normalized()) @ Matrix.Translation(-h) @ mw
        bpy.context.view_layer.update()

    def aim(self, name, child, direction):
        """Turns a bone the shortest way so that its joint-to-child line points along a world direction."""
        d = (self.head(child) - self.head(name)).normalized()
        q = d.rotation_difference(Vector(direction).normalized())
        axis, ang = q.to_axis_angle()
        if ang > 1e-5:
            self.turn(name, math.degrees(ang), axis)

    def move(self, name, delta):
        bpy.context.view_layer.update()
        b = self.pb[name]
        m = b.matrix.copy()
        m.translation += Vector(delta)
        b.matrix = m
        bpy.context.view_layer.update()

    def place(self, name, where):
        self.move(name, Vector(where) - self.head(name))

    def ik(self, upper, lower, end, target, pole):
        """Two-bone IK: the end joint to `target`, the middle joint bending toward `pole` (a world direction)."""
        s, e0, w0 = self.head(upper), self.head(lower), self.head(end)
        l1, l2 = (e0 - s).length, (w0 - e0).length
        d = Vector(target) - s
        dist = min(max(d.length, abs(l1 - l2) + 1e-4), l1 + l2 - 1e-4)
        dn = d.normalized()
        a = (l1 * l1 - l2 * l2 + dist * dist) / (2 * dist)
        h = math.sqrt(max(l1 * l1 - a * a, 0.0))
        pv = Vector(pole) - dn * Vector(pole).dot(dn)
        if pv.length < 1e-6:
            pv = Vector((0, 0, -1)) - dn * dn.z
        pv.normalize()
        elbow = s + dn * a + pv * h
        self.aim(upper, lower, elbow - s)
        self.aim(lower, end, (s + dn * dist) - self.head(lower))

    def palm_normal(self, side):
        sgn = -1.0 if side == "l" else 1.0
        i, p = self.head("index_01_" + side), self.head("pinky_01_" + side)
        f = self.head("middle_01_" + side) - self.head("hand_" + side)
        return ((i - p).cross(f) * sgn).normalized()

    def hand(self, side, fingers_dir, palm, forearm_share=0.5):
        """Points the hand's fingers along fingers_dir and turns it about that axis until the palm faces `palm`; part
        of that turn goes into the forearm (no twist bones: the wrist alone would wring)."""
        self.aim("hand_" + side, "middle_01_" + side, fingers_dir)
        ax = (self.head("middle_01_" + side) - self.head("hand_" + side)).normalized()
        n = self.palm_normal(side)
        want = Vector(palm) - ax * Vector(palm).dot(ax)
        n = n - ax * n.dot(ax)
        if want.length < 1e-6 or n.length < 1e-6:
            return
        ang = math.degrees(n.normalized().angle(want.normalized()))
        if n.cross(want).dot(ax) < 0:
            ang = -ang
        fa = (self.head("hand_" + side) - self.head("lowerarm_" + side)).normalized()
        self.turn("lowerarm_" + side, ang * forearm_share, fa)
        # the forearm's turn moved the hand: aim it again and finish the turn in the wrist
        self.aim("hand_" + side, "middle_01_" + side, fingers_dir)
        ax = (self.head("middle_01_" + side) - self.head("hand_" + side)).normalized()
        n = self.palm_normal(side)
        n = n - ax * n.dot(ax)
        want = Vector(palm) - ax * Vector(palm).dot(ax)
        ang = math.degrees(n.normalized().angle(want.normalized()))
        if n.cross(want).dot(ax) < 0:
            ang = -ang
        self.turn("hand_" + side, ang, ax)

    def curl(self, side, amounts, thumb=None):
        """Finger curl (deg at the first joint, a little less at the others) about each finger's own bend axis;
        amounts: one number for all four or a dict per finger."""
        for f in FINGERS:
            c = amounts.get(f, 0.0) if isinstance(amounts, dict) else amounts
            for i, k in ((1, 1.0), (2, 0.9), (3, 0.7)):
                nm = "%s_0%d_%s" % (f, i, side)
                if nm in self.pb:
                    b = self.pb[nm]
                    b.rotation_mode = "QUATERNION"
                    b.rotation_quaternion = Quaternion((1, 0, 0), math.radians(c * k))
        if thumb is not None:
            for i in (2, 3):
                nm = "thumb_0%d_%s" % (i, side)
                if nm in self.pb:
                    self.pb[nm].rotation_mode = "QUATERNION"
                    self.pb[nm].rotation_quaternion = Quaternion((1, 0, 0), math.radians(thumb))
        bpy.context.view_layer.update()

    def snapshot(self):
        bpy.context.view_layer.update()
        return {b.name: (b.matrix_basis.to_translation(), b.matrix_basis.to_quaternion()) for b in self.pb}

    def apply(self, snap):
        for b in self.pb:
            t, q = snap[b.name]
            b.matrix_basis = Matrix.Translation(t) @ q.to_matrix().to_4x4()
        bpy.context.view_layer.update()


def ease(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3 - 2 * t)


def blend(a, b, t):
    out = {}
    for k in a:
        ta, qa = a[k]
        tb, qb = b[k]
        if qa.dot(qb) < 0:
            qb = -qb
        out[k] = (ta.lerp(tb, t), qa.slerp(qb, t))
    return out


def bake(arm, p, name, frames, prefix="Hand_"):
    act = bpy.data.actions.new(prefix + name)
    arm.animation_data_create()
    arm.animation_data.action = act
    prev = {}
    for i, snap in enumerate(frames):
        for b in p.pb:
            t, q = snap[b.name]
            if b.name in prev and prev[b.name].dot(q) < 0:
                q = -q
            prev[b.name] = q
            b.rotation_mode = "QUATERNION"
            b.rotation_quaternion = q
            b.keyframe_insert("rotation_quaternion", frame=i + 1, group=b.name)
            if b.name in ("root", "pelvis"):
                b.location = t
                b.keyframe_insert("location", frame=i + 1, group=b.name)
    act.use_fake_user = True
    print("POSER %s: %d frames" % (name, len(frames)), flush=True)
    return act


def export(arm, acts, path):
    arm.animation_data.action = None
    for a in acts:
        tr = arm.animation_data.nla_tracks.new()
        tr.name = a.name
        tr.strips.new(a.name, 1, a)
        tr.mute = True
    body = max((o for o in bpy.data.objects if o.type == "MESH" and o.parent == arm), key=lambda o: len(o.data.vertices))
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    body.select_set(True)
    bpy.context.view_layer.objects.active = arm
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=path, use_selection=True, export_format="GLB", export_skins=True,
                              export_animations=True, export_animation_mode="NLA_TRACKS", export_force_sampling=True,
                              export_def_bones=False, export_yup=True, export_morph=False, export_materials="NONE")
    print("POSER wrote %s" % os.path.relpath(path, ROOT), flush=True)
