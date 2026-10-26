"""Sleeping on one's side: LieDown, Sleep (a loop) and GetUp, posed on the clips' body (Guy_a, blender/humans/clips.json
target) for retarget_clips.py, which carries them to A_Human_LieDown / Sleep / GetUp.

    Blender -b -P blender/humans/sleep_clips.py -- [--preview]

The bed layout is life_anims.py's (Blender metres, character space facing -Y; UE (x, y) = 100 (x, -y)): the actor
stands at the origin beside the bed, facing away from it (-Y); the bed is behind, its near long edge at y = 0.10, the
mattress top 0.50 m, its middle at (-0.05, 0.55), the head end at +X (2 m from x = -1.05 to 0.95).

The sleeper lies on the left side facing out of the bed (-Y), the head toward +X on a pillow ~12 cm over the
mattress, knees bent (the top leg more), the lower (left) arm under the pillow, the upper (right) arm resting in front
on the mattress; slow breathing (5 s a breath). LieDown: sits down on the bed's edge, leans over onto the left
elbow, swings the legs up and settles; GetUp is LieDown played backward.

Posed by turning bones about world axes (a bone turns its children with it) from a standing pose, keyed per frame as
each bone's rotation, between key poses slerped with easing. Writes Saved/ClipSources/sleep_side.glb (actions
Side_LieDown, Side_Sleep, Side_GetUp) and with --preview Saved/SleepClips/*.png.
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
TARGET = os.path.join(ROOT, "blender", "characters", "guy_a", "export", "human_guy_a.glb")
FPS = 30
MATTRESS = 0.50
BED_Y = 0.62                     # the pelvis across the bed (the body then fills the mattress round its middle, 0.55)
HEAD_X = 0.70                    # where the head rests (the pillow at the head end, +X)
X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))


def log(m):
    print("SLEEP " + m, flush=True)


def load():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.render.fps = FPS
    bpy.ops.import_scene.gltf(filepath=TARGET, bone_heuristic="BLENDER")
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

    def reset(self):
        for b in self.pb:
            b.rotation_mode = "QUATERNION"
            b.matrix_basis = Matrix()
        bpy.context.view_layer.update()

    def head(self, name):
        bpy.context.view_layer.update()
        return (self.arm.matrix_world @ self.pb[name].matrix).translation

    def turn(self, name, deg, axis):
        """Turns a bone (and so its children) about a world axis through its joint."""
        bpy.context.view_layer.update()
        b = self.pb[name]
        mw = b.matrix.copy()
        h = mw.translation.copy()
        b.matrix = Matrix.Translation(h) @ Matrix.Rotation(math.radians(deg), 4, axis) @ Matrix.Translation(-h) @ mw
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
        m.translation += delta
        b.matrix = m
        bpy.context.view_layer.update()

    def snapshot(self):
        bpy.context.view_layer.update()
        return {b.name: (b.matrix_basis.to_translation(), b.matrix_basis.to_quaternion()) for b in self.pb}

    def apply(self, snap):
        for b in self.pb:
            t, q = snap[b.name]
            b.matrix_basis = Matrix.Translation(t) @ q.to_matrix().to_4x4()


# ---------------------------------------------------------------------------------------------- key poses

def standing(p):
    """Standing at ease: arms down by the sides, a little bent."""
    p.reset()
    p.turn("upperarm_l", 78, Y)
    p.turn("upperarm_r", -78, Y)
    p.turn("lowerarm_l", 12, X)
    p.turn("lowerarm_r", 12, X)
    return p.snapshot()


def fingers(p, side, curl):
    for f in ("index", "middle", "ring", "pinky"):
        for i in (1, 2, 3):
            nm = "%s_0%d_%s" % (f, i, side)
            if nm in p.pb:
                bpy.context.view_layer.update()
                b = p.pb[nm]
                b.rotation_mode = "QUATERNION"
                b.rotation_quaternion = Quaternion((1, 0, 0), math.radians(curl * (1.0 if i == 1 else 0.8)))


def sitting_edge(p):
    """Sitting on the bed's edge, facing out (-Y), feet on the floor, hands on the thighs."""
    standing(p)
    for s in "lr":
        p.turn("thigh_" + s, -88, X)       # thighs forward (-Y)
        p.turn("calf_" + s, 84, X)         # shins down again
        p.turn("foot_" + s, 4, X)
    p.turn("spine_01", -6, X)              # a slight slump forward
    p.turn("spine_02", -4, X)
    for s, sign in (("l", 1), ("r", -1)):
        p.turn("upperarm_" + s, -22, X)
        p.turn("lowerarm_" + s, -55, X)
    hip = p.head("thigh_l")
    p.move("pelvis", Vector((0, 0.27, MATTRESS + 0.075 - hip.z)))     # the seat on the mattress edge
    return p.snapshot()


def on_elbow(p):
    """Half way: leaning over onto the left elbow toward the head end, the legs still down."""
    sitting_edge(p)
    p.turn("pelvis", 45, Y)
    p.turn("spine_02", -10, Z)
    p.aim("upperarm_l", "lowerarm_l", (0.25, 0.15, -0.95))      # propped on the left forearm toward the head end
    p.aim("lowerarm_l", "hand_l", (0.85, 0.20, -0.45))
    p.aim("upperarm_r", "lowerarm_r", (0.15, -0.45, -0.88))     # the right hand across, on the mattress in front
    p.aim("lowerarm_r", "hand_r", (0.55, -0.35, -0.75))
    hip = p.head("pelvis")
    p.move("pelvis", Vector((0.02, 0.08, MATTRESS + 0.13 - hip.z)))
    return p.snapshot()


def lying(p, breath=0.0):
    """On the left side, facing out of the bed, head toward +X on the pillow; breath 0..1 lifts the chest."""
    standing(p)
    p.turn("pelvis", 90, Y)                       # the whole body down onto its left side: up -> +X
    # a gentle curl: forward is -Y now, the body's left -Z
    p.turn("spine_01", -6 - 1.2 * breath, Z)
    p.turn("spine_02", -5 - 1.0 * breath, Z)
    p.turn("spine_03", -3 + 1.8 * breath, Z)
    for b, a in (("spine_01", -3), ("spine_02", -3), ("spine_03", -2)):  # the broad shoulders hold the chest up: the
        p.turn(b, a, Y)                                                        # trunk slopes up from the waist
    p.turn("neck_01", -8, Z)
    p.turn("neck_01", 20, Y)                      # the head down onto the pillow (~12 cm)
    p.turn("head", -6 - 0.8 * breath, Z)
    p.turn("head", 6, Y)                          # the cheek into the pillow
    # legs: hips and knees bent, the top (right) leg further forward and resting over the lower one
    p.turn("thigh_l", 38, Z)
    p.turn("calf_l", -55, Z)
    p.turn("thigh_r", 55, Z)
    p.turn("thigh_r", 10, X)
    p.turn("calf_r", -70, Z)
    for s in "lr":
        p.turn("foot_" + s, -18, Z)
    # the lower (left) arm forward and up toward the head, the forearm back under the pillow; the upper (right) arm
    # down across the chest, the hand resting on the mattress in front
    p.aim("upperarm_l", "lowerarm_l", (0.55, -0.62, 0.12))
    p.aim("lowerarm_l", "hand_l", (0.30, 0.90, -0.10))
    p.aim("hand_l", "middle_01_l", (0.15, 0.95, -0.15))
    p.turn("clavicle_r", 4 + 2 * breath, Y)
    p.aim("upperarm_r", "lowerarm_r", (-0.05, -0.50, -0.86))
    p.aim("lowerarm_r", "hand_r", (0.40, -0.60, -0.70))
    p.aim("hand_r", "middle_01_r", (0.50, -0.60, -0.60))
    fingers(p, "l", 25)
    fingers(p, "r", 35)
    # onto the bed: the head over the pillow, the hip and shoulder on the mattress
    head, hip = p.head("head"), p.head("thigh_l")
    dz = MATTRESS + 0.085 - hip.z                 # the lower hip joint ~8 cm over the mattress
    p.move("pelvis", Vector((HEAD_X - head.x, BED_Y - p.head("pelvis").y, dz)))
    return p.snapshot()


# ---------------------------------------------------------------------------------------------- clips

def ease(t):
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


def bake(arm, p, name, frames):
    """frames: a list of snapshots, one a frame."""
    act = bpy.data.actions.new("Side_" + name)
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
    log("%s: %d frames" % (name, len(frames)))
    return act


def keyed(keys, seconds):
    """keys: [(time 0..1, snapshot)], eased between; seconds long."""
    n = int(round(seconds * FPS)) + 1
    out = []
    for i in range(n):
        t = i / (n - 1)
        for (t0, a), (t1, b) in zip(keys, keys[1:]):
            if t0 <= t <= t1:
                out.append(blend(a, b, ease((t - t0) / max(t1 - t0, 1e-6))))
                break
    return out


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    arm = load()
    p = Poser(arm)
    stand, sit, elbow, lie = standing(p), sitting_edge(p), on_elbow(p), lying(p)
    log("lying: " + ", ".join("%s %s" % (b, tuple(round(v, 3) for v in p.head(b)))
                              for b in ("head", "pelvis", "thigh_l", "upperarm_l", "hand_l", "hand_r", "foot_l", "foot_r", "calf_r")))
    report_mesh(arm, "lying")
    down = keyed([(0.0, stand), (0.35, sit), (0.65, elbow), (1.0, lie)], 3.0)
    breaths = []
    for i in range(int(10 * FPS) + 1):
        b = 0.5 - 0.5 * math.cos(2 * math.pi * i / (5 * FPS))       # 5 s a breath, two in the loop
        breaths.append(lying(p, b))
    acts = [bake(arm, p, "LieDown", down), bake(arm, p, "Sleep", breaths), bake(arm, p, "GetUp", list(reversed(down)))]
    arm.animation_data.action = None
    for a in acts:
        tr = arm.animation_data.nla_tracks.new()
        tr.name = a.name
        tr.strips.new(a.name, 1, a)
        tr.mute = True
    p.reset()
    if "--preview" in argv:
        preview(arm, acts)
    out = os.path.join(ROOT, "Saved", "ClipSources", "sleep_side.glb")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    body = max((o for o in bpy.data.objects if o.type == "MESH" and o.parent == arm), key=lambda o: len(o.data.vertices))
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    body.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.gltf(filepath=out, use_selection=True, export_format="GLB", export_skins=True,
                              export_animations=True, export_animation_mode="NLA_TRACKS", export_force_sampling=True,
                              export_def_bones=False, export_yup=True, export_morph=False, export_materials="NONE")
    log("wrote %s" % os.path.relpath(out, ROOT))


def report_mesh(arm, label):
    """Where the posed body is: its bounding box and how much of it sinks into the mattress (more than 2 cm)."""
    import numpy as np
    body = max((o for o in bpy.data.objects if o.type == "MESH" and o.parent == arm), key=lambda o: len(o.data.vertices))
    dg = bpy.context.evaluated_depsgraph_get()
    me = body.evaluated_get(dg).to_mesh()
    co = np.array([body.matrix_world @ v.co for v in me.vertices])
    body.evaluated_get(dg).to_mesh_clear()
    on = (co[:, 0] > -1.05) & (co[:, 0] < 0.95) & (co[:, 1] > 0.10) & (co[:, 1] < 1.0)
    sunk = on & (co[:, 2] < MATTRESS - 0.02)
    log("%s: x %.2f..%.2f, y %.2f..%.2f, z %.2f..%.2f; off the mattress %d%%, sunk %d%%" % (
        label, co[:, 0].min(), co[:, 0].max(), co[:, 1].min(), co[:, 1].max(), co[:, 2].min(), co[:, 2].max(),
        100 * (~on).mean(), 100 * sunk.mean()))


def preview(arm, acts):
    """The bed as boxes and a few frames of each clip from the room and from the head end."""
    def box(name, x0, x1, y0, y1, z0, z1):
        bpy.ops.mesh.primitive_cube_add(location=((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2))
        o = bpy.context.active_object
        o.scale = ((x1 - x0) / 2, (y1 - y0) / 2, (z1 - z0) / 2)
        o.name = name
    box("mattress", -1.05, 0.95, 0.10, 1.0, 0.30, MATTRESS)
    box("pillow", 0.55, 0.90, 0.25, 0.85, MATTRESS, MATTRESS + 0.12)
    box("floor", -2, 2, -2, 2, -0.02, 0.0)
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    sc.render.resolution_x, sc.render.resolution_y = 480, 360
    w = bpy.data.worlds.new("w")
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs[0].default_value = (0.6, 0.6, 0.62, 1)
    sc.world = w
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 3
    sun.rotation_euler = (math.radians(40), 0, math.radians(-30))
    sc.collection.objects.link(sun)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    out = os.path.join(ROOT, "Saved", "SleepClips")
    os.makedirs(out, exist_ok=True)
    views = {"room": ((0.0, -2.8, 1.4), (0.0, 0.5, 0.55)), "top": ((0.0, 0.3, 3.6), None)}
    for act in acts:
        arm.animation_data.action = act
        n = int(act.frame_range[1])
        for j in range(5):
            f = 1 + int((n - 1) * j / 4)
            sc.frame_set(f)
            for vn, (eye, tgt) in views.items():
                cam.location = eye
                if tgt is None:                                    # straight down, +X to the right
                    cam.rotation_euler = (0, 0, 0)
                else:
                    cam.rotation_euler = (Vector(tgt) - Vector(eye)).to_track_quat("-Z", "Y").to_euler()
                sc.render.filepath = os.path.join(out, "%s_%s_%d.png" % (act.name, vn, j))
                bpy.ops.render.render(write_still=True)
    arm.animation_data.action = None


main()
