"""The run: an easy jog posed on the clips' body (Guy_a) with poser.py, carried over to A_Human_Run by retarget_clips.py
(clips.json source "run").

    Blender -b -P blender/humans/run_clip.py -- [--preview]

Why our own: UAL's Jog_Fwd_Loop has a sprinter's stride with the foot on the ground for ~0.08 s, sweeping back at ~6 m/s;
played at a jog's 2.6 m/s (ANpcBody: Speed / (260 * BodyScale)) the planted feet skated ~0.3 m every step.
Here the feet follow paths laid out for the speed: on the ground the ankle moves back at exactly SPEED (no sliding at
rate 1), a stance of DUTY of the cycle, then the heel kicks up behind, the knee drives through and the foot reaches out
and pulls back to land under the knee. The pelvis bobs (lowest at mid-stance), yaws with the forward leg and shifts over
the standing foot; the chest turns against it; the arms swing from the shoulder with the elbows bent ~85 deg, hands
loose fists crossing a little toward the middle; the head stays level and looks ahead.
Blender character space: faces -Y, its left is +X, up +Z; metres. In place (the actor moves), 30 fps, loops.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import poser  # noqa: E402
from poser import X, Y, Z, FPS, ROOT, Poser  # noqa: E402

SPEED = 2.6                  # m/s at rate 1 (ANpcBody's 260 cm/s)
FRAMES = 22                  # one cycle (two steps): 0.733 s, ~164 steps a minute
T = FRAMES / FPS
DUTY = 0.36                  # the share of a cycle each foot is on the ground
TAU = 2 * math.pi


def log(m):
    print("RUNCLIP " + m, flush=True)


def catmull(pts, u):
    """A smooth curve through (u, y, z) points (u ascending), clamped ends."""
    for i in range(len(pts) - 1):
        if pts[i][0] <= u <= pts[i + 1][0]:
            break
    p0, p1, p2, p3 = pts[max(i - 1, 0)], pts[i], pts[i + 1], pts[min(i + 2, len(pts) - 1)]
    s = (u - p1[0]) / (p2[0] - p1[0])
    out = []
    for k in (1, 2):
        a, b, c, d = p0[k], p1[k], p2[k], p3[k]
        out.append(0.5 * (2 * b + (-a + c) * s + (2 * a - 5 * b + 4 * c - d) * s * s + (-a + 3 * b - 3 * c + d) * s ** 3))
    return out


class Body:
    """Lengths and rest heights measured on the rig."""

    def __init__(self, p):
        p.reset()
        self.ankle_z = p.head("foot_l").z
        self.ball_z = p.head("ball_l").z
        self.foot_len = (p.head("ball_l") - p.head("foot_l")).length
        self.thigh = (p.head("calf_l") - p.head("thigh_l")).length
        self.shin = (p.head("foot_l") - p.head("calf_l")).length
        self.hip_below_pelvis = p.head("pelvis").z - p.head("thigh_l").z
        self.pelvis = p.head("pelvis").copy()
        self.upper = (p.head("lowerarm_l") - p.head("upperarm_l")).length
        self.fore = (p.head("hand_l") - p.head("lowerarm_l")).length
        stance = SPEED * DUTY * T
        self.land, self.off = -0.36 * stance, 0.64 * stance        # the ankle's y at touch-down (near under the body) and toe-off
        # the pelvis as high as lets the leg land with the knee a little bent
        reach = 0.985 * (self.thigh + self.shin)
        self.pelvis_z = self.ankle_z + math.sqrt(reach ** 2 - self.land ** 2) + self.hip_below_pelvis - 0.012
        log("leg %.3f+%.3f, ankle %.3f, stance %.3f m (%.3f..%.3f), pelvis %.3f (rest %.3f)" % (
            self.thigh, self.shin, self.ankle_z, stance, self.land, self.off, self.pelvis_z, self.pelvis.z))


def foot(b, ph):
    """The ankle (y, z above its rest height) and the foot's pitch (deg, + toes up) at phase ph (0: touch-down)."""
    ph %= 1.0
    if ph < DUTY:
        s = ph / DUTY
        y = b.land + (b.off - b.land) * s                       # back at exactly SPEED: no sliding
        heel = 0.05 * poser.ease((s - 0.55) / 0.45)             # the heel peels off, the ball stays down
        pitch = 6.0 * (1 - poser.ease(s / 0.2)) - 38.0 * poser.ease((s - 0.55) / 0.45)
        return y, heel, pitch
    u = (ph - DUTY) / (1 - DUTY)
    y, z = catmull([(0.0, b.off, 0.05), (0.22, b.off + 0.04, 0.27), (0.48, 0.06, 0.26), (0.76, b.land - 0.07, 0.11),
                    (1.0, b.land, 0.0)], u)
    pitch = -38.0 + 50.0 * poser.ease(u / 0.6) - 6.0 * poser.ease((u - 0.85) / 0.15)
    return y, max(z, 0.0), pitch


def run(p, b, t):
    ph = (t / T) % 1.0
    p.reset()
    # the pelvis: bob (lowest at each mid-stance), sway over the standing foot, yaw with the forward leg, tipped forward
    bob = -0.022 * math.cos(2 * TAU * (ph - DUTY / 2))
    sway = 0.012 * math.cos(TAU * (ph - DUTY / 2))              # + toward the left (+X) while the left foot stands
    p.place("pelvis", Vector((sway, b.pelvis.y, b.pelvis_z + bob)))
    yaw = 6.0 * math.cos(TAU * (ph + 0.02))                     # the left hip forward (toward -Y) as the left leg lands
    p.turn("pelvis", -yaw, Z)
    p.turn("pelvis", 2.5 * math.sin(TAU * (ph - DUTY / 2)), Y)  # the free hip drops a little
    p.turn("pelvis", -5.0, X)                                   # tipped forward
    # legs: ankle paths, knees forward, the foot pitched by phase
    for s, sign, off in (("l", 1, 0.0), ("r", -1, 0.5)):
        y, z, pitch = foot(b, ph - off)
        ankle = Vector((sign * 0.095, y, b.ankle_z + z))
        p.ik("thigh_" + s, "calf_" + s, "foot_" + s, ankle, (sign * 0.08, -1.0, 0.1))
        a = math.radians(pitch)
        p.aim("foot_" + s, "ball_" + s, (0.0, -math.cos(a) * 0.95, math.sin(a) - 0.42 * math.cos(a)))
    # the trunk: leaning in, the chest turning against the pelvis, breathing with the steps
    for bone, w in (("spine_01", 0.35), ("spine_02", 0.35), ("spine_03", 0.3)):
        p.turn(bone, 12.0 * w, X)
        p.turn(bone, yaw * 1.7 * w, Z)
    # the head level, looking ahead: undo the lean and most of the chest's turn
    p.turn("neck_01", -9.0, X)
    p.turn("head", -6.0 - 2.0 * math.cos(2 * TAU * (ph - DUTY / 2)), X)
    p.turn("neck_01", -yaw * 0.5, Z)
    p.turn("head", -yaw * 0.3, Z)
    # arms against the legs: the right arm forward as the left foot lands
    for s, sign in (("l", 1), ("r", -1)):
        swing = 32.0 * math.cos(TAU * (ph + (0.0 if s == "r" else 0.5) + 0.03))   # deg, + forward
        elbow = 82.0 + 14.0 * max(0.0, swing) / 32.0                              # more bent in front
        p.turn("clavicle_" + s, -sign * 3.0 * swing / 32.0, Z)                     # the shoulder goes with it
        sh = p.head("upperarm_" + s)
        a, e = math.radians(swing), math.radians(swing + elbow)
        inward = 0.035 * max(0.0, swing) / 32.0                                    # across toward the middle in front
        el = sh + Vector((sign * 0.05, -math.sin(a), -math.cos(a))).normalized() * b.upper
        wr = el + Vector((-sign * (0.05 + inward * 1.5), -math.sin(e), -math.cos(e))).normalized() * b.fore
        p.ik("upperarm_" + s, "lowerarm_" + s, "hand_" + s, wr, (sign * 0.25, 1.0, -0.3))
        fd = (wr - el).normalized()
        p.hand(s, fd, (-sign, 0.0, 0.0), 0.5)                                      # palms toward the body
        p.curl(s, {"index": 55.0, "middle": 62.0, "ring": 66.0, "pinky": 70.0}, thumb=22.0)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    arm = poser.load()
    p = Poser(arm)
    b = Body(p)
    frames = []
    for i in range(FRAMES + 1):
        run(p, b, i / FPS)
        frames.append(p.snapshot())
    acts = [poser.bake(arm, p, "Run", frames, prefix="Run_")]
    p.reset()
    if "--preview" in argv:
        preview(arm, p, b)
    poser.export(arm, acts, os.path.join(ROOT, "Saved", "ClipSources", "run.glb"))


def preview(arm, p, b):
    """Side and front at 8 moments of the cycle, the ground and a mark where each foot stands: Saved/RunClip/*.png."""
    sc = bpy.context.scene
    arm.animation_data.action = None                          # posed directly, not by the baked action
    bpy.ops.mesh.primitive_plane_add(size=6, location=(0, 0, 0))
    m = bpy.data.materials.new("ground")
    m.use_nodes = True
    m.node_tree.nodes["Principled BSDF"].inputs[0].default_value = (0.35, 0.33, 0.3, 1)
    bpy.context.active_object.data.materials.append(m)
    clay = bpy.data.materials.new("clay")
    clay.use_nodes = True
    clay.node_tree.nodes["Principled BSDF"].inputs[0].default_value = (0.8, 0.55, 0.45, 1)
    for o in bpy.data.objects:
        if o.type == "MESH" and o.parent == arm:
            o.data.materials.clear()
            o.data.materials.append(clay)
    sc.render.engine = "BLENDER_EEVEE"
    sc.render.resolution_x, sc.render.resolution_y = 300, 420
    sc.view_settings.view_transform = "Standard"
    w = bpy.data.worlds.new("w")
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs[0].default_value = (0.45, 0.45, 0.48, 1)
    sc.world = w
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 3
    sun.rotation_euler = (math.radians(50), 0, math.radians(150))
    sc.collection.objects.link(sun)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    out = os.path.join(ROOT, "Saved", "RunClip")
    os.makedirs(out, exist_ok=True)
    views = {"side": ((3.2, 0.0, 0.95), (0.0, 0.0, 0.85)), "front": ((0.9, -3.0, 1.1), (0.0, 0.0, 0.85))}
    for j in range(8):
        run(p, b, T * j / 8)
        for vn, (eye, tgt) in views.items():
            cam.location = eye
            cam.rotation_euler = (Vector(tgt) - Vector(eye)).to_track_quat("-Z", "Y").to_euler()
            sc.render.filepath = os.path.join(out, "%s_%d.png" % (vn, j))
            bpy.ops.render.render(write_still=True)
    p.reset()


main()
