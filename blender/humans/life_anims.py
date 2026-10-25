"""Shared everyday clips for every human NPC (all MakeHuman meshes on SK_Human_Skeleton):
walk, stand idle, talk, sit down / sit idle / eat / read / stand up, lie down / sleep / get up.

Baked once on a neutral adult rig (guy_a.blend from the humans pipeline, 1.69 m) and imported as
/Game/Characters/Humans/Anims/A_Human_<Clip> by unreal/scripts/import_life_anims.py.

  Blender (bake, preview renders, glb):
    Blender -b <humans worktree>/blender/humans/guy_a.blend --python blender/humans/life_anims.py -- [--no-preview]
  -> Saved/LifeAnims/life_anims.glb, Saved/LifeAnims/frames/*.png, Saved/LifeAnims/life_anims_sheet.png,
     Saved/LifeAnims/life_anims.blend, Saved/LifeAnims/life_anims_report.json

All clips are in place (no root motion), 30 fps. Blender character space: faces -Y, its left is +X.
UE actor space: (x, y, z)_UE = 100 * (x, -y, z)_Blender, so the character faces the actor's +Y and its
left is the actor's +X.

Frames (UE, actor-local, cm):
  standing   feet on the floor around the origin, facing +Y.
  Walk       1.0 s loop = one stride (two steps), 125 cm/s: move the actor forward at 125 cm/s.
  seated     the actor origin is on the floor under the chair-seat centre, facing +Y (the table side):
             seat top 47 cm (thigh joints 47+9.2 cm up), feet 40 cm in front; the very same pelvis and
             feet as seat_anims.py's seated layout (hip_back 0, feet_fwd 0.42, feet_out 0.13), so the
             per-character A_<Name>_Sit_* loops use the same chair point.
             SitDown starts standing 30 cm in front of the chair point (feet ~33 cm in front).
  Eat        table top 75 cm, near edge 30 cm in front of the chair point, plate centre 50 cm in front.
  lying      the actor origin is the standing point beside the bed, facing +Y away from the bed.
             The bed is behind: its near long edge 10 cm behind the origin (y = -10), mattress top
             50 cm, width >= 90 cm (to y = -100); the body ends on its back along the actor X axis,
             head towards +X (the character's left at the start), pelvis at x = 0, y = -52 cm.
             Needs bed length from x = -105 to x = +95 (2.0 m) with the head end at +X.

Rig helpers copied from the humans worktree's blender/humans/seat_anims.py (wt/humans): _w, _empty,
_wave, the Rig constraint set-up (IK legs/arms with pole targets, hand orientation from forward/palm
vectors, finger curls, spine lean, look-at for neck/head/eyes), check() and the bake loop; extended
here with pelvis/foot/toe rotation helpers, leg-reach fitting, IK error and mesh clearance checks.
"""
import json
import math
import os
import subprocess
import sys

try:
    import bpy
    import numpy as np
    from mathutils import Matrix, Vector
except ImportError:                      # plain python3: only the contact sheet helper
    bpy = None

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "Saved", "LifeAnims")

FPS = 30
SEAT, TABLE, MATTRESS = 0.47, 0.75, 0.50
TABLE_EDGE, PLATE = -0.30, -0.50          # Blender y (in front = -y)
BED_EDGE, LIE_Y = 0.10, 0.52              # Blender y (behind the standing character)
WALK_V = 1.25

# name: (seconds, loops)
CLIPS = {
    "Walk": (1.0, True), "StandIdle": (8.0, True), "Talk": (6.0, True),
    "SitDown": (2.4, False), "StandUp": (2.4, False), "SitIdle": (10.0, True),
    "Eat": (6.0, True), "Read": (10.0, True),
    "LieDown": (3.0, False), "Sleep": (10.0, True), "GetUp": (3.0, False),
}
FINGERS = ["thumb", "index", "middle", "ring", "pinky"]
SIDES = (("l", 1), ("r", -1))


# --------------------------------------------------------------------------------- math helpers

def _w(arm, name):
    return arm.matrix_world @ arm.data.bones[name].head_local


def _empty(name, loc=(0, 0, 0)):
    e = bpy.data.objects.new(name, None)
    bpy.context.scene.collection.objects.link(e)
    e.location = loc
    return e


def _wave(t, period, phase=0.0):
    return math.sin(2 * math.pi * (t / period + phase))


def clamp01(x):
    return min(1.0, max(0.0, x))


def smooth(x):
    x = clamp01(x)
    return x * x * (3 - 2 * x)


def smoother(x):
    x = clamp01(x)
    return x * x * x * (x * (6 * x - 15) + 10)


def bump(t, c, w, T=None):
    """0..1..0 cosine window centred at c, half width w (periodic in T when given)."""
    d = t - c
    if T:
        d = (d + T / 2) % T - T / 2
    if abs(d) >= w:
        return 0.0
    return 0.5 * (1 + math.cos(math.pi * d / w))


def Rx(a):
    return Matrix.Rotation(a, 3, "X")


def Ry(a):
    return Matrix.Rotation(a, 3, "Y")


def Rz(a):
    return Matrix.Rotation(a, 3, "Z")


def lerp(a, b, s):
    return a + (b - a) * s


def slerp_m(a, b, s):
    return a.to_quaternion().slerp(b.to_quaternion(), s).to_matrix()


I3 = Matrix.Identity(3) if bpy else None
# lying on the back: rest up (+Z) -> +X (head towards +X), rest back (+Y) -> down, rest left (+X) -> -Y
R_LIE = Matrix(((0, 0, 1), (-1, 0, 0), (0, -1, 0))) if bpy else None


# ------------------------------------------------------------------------------------------ rig

class Rig:
    def __init__(self, arm):
        self.arm = arm
        self.pb = arm.pose.bones
        mw = arm.matrix_world
        bones = arm.data.bones
        self.rest = {b.name: mw @ b.matrix_local for b in bones}
        self.head = {b.name: mw @ b.head_local for b in bones}
        self.tail = {b.name: mw @ b.tail_local for b in bones}
        self.height = self.head["head"].z + 0.12
        self.k = self.height / 1.75                         # body scale vs an adult man (seat_anims)
        self.hip_z = SEAT + 0.095 * self.k
        self.pel0 = self.head["pelvis"]
        self.hc0 = (self.head["thigh_l"] + self.head["thigh_r"]) / 2
        self.az = (self.head["foot_l"].z + self.head["foot_r"].z) / 2          # ankle height
        self.leg = {s: (self.tail["thigh_" + s] - self.head["thigh_" + s]).length +
                    (self.tail["calf_" + s] - self.head["calf_" + s]).length for s in "lr"}
        self.helpers, self.cons = [], []

    # ----------------------------------------------------------------- helpers / constraints
    def helper(self, name, loc=(0, 0, 0), rot=False):
        e = _empty(self.arm.name + "_" + name, loc)
        if rot:
            e.rotation_mode = "QUATERNION"
        e["rot"] = rot
        self.helpers.append(e)
        return e

    def con(self, bone, kind, **kw):
        c = self.pb[bone].constraints.new(kind)
        for k, v in kw.items():
            setattr(c, k, v)
        self.cons.append((bone, c))
        return c

    def setup(self):
        self.t_pelvis = self.helper("pelvis")
        self.t_prot = self.helper("pelvis_rot", rot=True)
        self.con("pelvis", "COPY_LOCATION", target=self.t_pelvis)
        self.con("pelvis", "COPY_ROTATION", target=self.t_prot)
        self.t_foot, self.t_knee, self.t_frot, self.t_brot = {}, {}, {}, {}
        for s in "lr":
            self.t_foot[s] = self.helper("foot_" + s)
            self.t_knee[s] = self.helper("knee_" + s)
            self.t_frot[s] = self.helper("foot_rot_" + s, rot=True)
            self.t_brot[s] = self.helper("ball_rot_" + s, rot=True)
            # seat_anims uses -90 deg for both legs, which turns both knees ~30 deg inwards (they
            # touch when seated); measured on guy_a: -121 / -59 put the knee in the pole plane
            self.con("calf_" + s, "IK", target=self.t_foot[s], pole_target=self.t_knee[s],
                     pole_angle=math.radians(-121 if s == "l" else -59), chain_count=2)
            self.con("foot_" + s, "COPY_ROTATION", target=self.t_frot[s])
            self.con("ball_" + s, "COPY_ROTATION", target=self.t_brot[s])
        self.t_wrist, self.t_elbow, self.t_hand = {}, {}, {}
        for s in "lr":
            self.t_wrist[s] = self.helper("wrist_" + s)
            self.t_elbow[s] = self.helper("elbow_" + s)
            self.t_hand[s] = self.helper("hand_" + s, rot=True)
            # seat_anims uses 0 here; a sweep on guy_a puts the elbow nearest its pole at -90 deg
            self.con("lowerarm_" + s, "IK", target=self.t_wrist[s], pole_target=self.t_elbow[s],
                     pole_angle=math.radians(-90), chain_count=2)
            self.con("hand_" + s, "COPY_ROTATION", target=self.t_hand[s])
        self.t_look = self.helper("look")
        self.con("neck_01", "DAMPED_TRACK", target=self.t_look, track_axis="TRACK_Z", influence=0.35)
        self.con("head", "DAMPED_TRACK", target=self.t_look, track_axis="TRACK_Z", influence=0.8)
        for s in "lr":
            self.con("eye_" + s, "DAMPED_TRACK", target=self.t_look, track_axis="TRACK_Y")

    def teardown(self):
        for bone, c in self.cons:
            self.pb[bone].constraints.remove(c)
        for e in self.helpers:
            bpy.data.objects.remove(e)
        self.helpers, self.cons = [], []

    # ------------------------------------------------------------------------- orientation
    def hand_m(self, s, forward, palm):
        """World rotation for hand_<s>: bone along `forward`, palm facing `palm` (seat_anims.hand_rot)."""
        r = self.rest["hand_" + s].to_3x3().normalized()
        y0 = r.col[1]
        inward = Vector((-1 if s == "l" else 1, 0, 0))
        cand = [r.col[0], -r.col[0], r.col[2], -r.col[2]]
        p0 = max(cand, key=lambda v: v.dot(inward))
        p0 = (p0 - y0 * p0.dot(y0)).normalized()
        f = Vector(forward).normalized()
        p = Vector(palm)
        p = (p - f * p.dot(f)).normalized()
        f0 = Matrix((y0, p0, y0.cross(p0))).transposed()
        f1 = Matrix((f, p, f.cross(p))).transposed()
        return f1 @ f0.transposed() @ r

    def foot_m(self, s, pitch=0.0, yaw=0.0, R=None, bone="foot"):
        """World rotation of foot/ball: rest turned by pitch (+ = toes down) about X, yaw about Z, then R."""
        rest = self.rest[bone + "_" + s].to_3x3().normalized()
        return (R or I3) @ Rz(yaw) @ Rx(pitch) @ rest

    def thigh_w(self, s, P, prot):
        return P + prot @ (self.head["thigh_" + s] - self.pel0)

    def pelvis_for_hips(self, hc, prot):
        """Pelvis head position that puts the hip-joint centre at hc."""
        return hc - prot @ (self.hc0 - self.pel0)

    def fit_legs(self, P, prot, feet, margin=0.012):
        """Lowers P so that no leg is over-stretched (IK would leave the foot short)."""
        P = P.copy()
        for s in "lr":
            th = self.thigh_w(s, P, prot)
            d = th - feet[s]
            lmax = self.leg[s] - margin
            h2 = d.x * d.x + d.y * d.y
            if d.length > lmax and h2 < lmax * lmax:
                P.z -= d.z - math.sqrt(lmax * lmax - h2)
        return P

    # --------------------------------------------------------------------------- keying
    def finger_curl(self, s, finger, amount):
        for i, w in ((1, 0.6), (2, 1.0), (3, 0.8)):
            b = self.pb["%s_0%d_%s" % (finger, i, s)]
            b.rotation_mode = "XYZ"
            ang = amount * w
            if finger == "thumb":
                b.rotation_euler = (0, 0, ang * (0.5 if s == "l" else -0.5))
            else:
                b.rotation_euler = (ang, 0, 0)
            b.keyframe_insert("rotation_euler", frame=self.frame)

    def spine(self, lean, twist=0.0, side=0.0, breath=0.0):
        for name, w in (("spine_01", 0.35), ("spine_02", 0.35), ("spine_03", 0.3)):
            b = self.pb[name]
            b.rotation_mode = "XYZ"
            b.rotation_euler = (lean * w + (breath if name == "spine_03" else 0.0), twist * w, side * w)
            b.keyframe_insert("rotation_euler", frame=self.frame)

    def pose(self, f, *, P, prot=None, feet, frot=None, brot=None, knees, wrists, elbows, hands, look,
             lean=0.0, twist=0.0, side=0.0, breath=0.0, curls=None, curl=0.4, jaw=0.0):
        self.frame = f
        prot = prot or I3
        self.t_pelvis.location = P
        self.t_prot.rotation_quaternion = (prot @ self.rest["pelvis"].to_3x3().normalized()).to_quaternion()
        for s in "lr":
            self.t_foot[s].location = feet[s]
            self.t_knee[s].location = knees[s]
            fr = (frot or {}).get(s) or self.foot_m(s)
            self.t_frot[s].rotation_quaternion = fr.to_quaternion()
            br = (brot or {}).get(s) or (fr @ self.rest["foot_" + s].to_3x3().normalized().inverted()
                                          @ self.rest["ball_" + s].to_3x3().normalized())
            self.t_brot[s].rotation_quaternion = br.to_quaternion()
            self.t_wrist[s].location = wrists[s]
            self.t_elbow[s].location = elbows[s]
            self.t_hand[s].rotation_quaternion = hands[s].to_quaternion()
        self.t_look.location = look
        for e in self.helpers:
            e.keyframe_insert("location", frame=f)
            if e["rot"]:
                e.keyframe_insert("rotation_quaternion", frame=f)
        self.spine(lean, twist, side, breath)
        jb = self.pb["jaw"]
        jb.rotation_mode = "XYZ"
        jb.rotation_euler = (jaw, 0, 0)
        jb.keyframe_insert("rotation_euler", frame=f)
        for s in "lr":
            for fin in FINGERS:
                self.finger_curl(s, fin, (curls or {}).get((s, fin), curl))

    def posed(self, bone, tail=False):
        pb = self.pb[bone]
        return self.arm.matrix_world @ (pb.tail if tail else pb.head)

    def mouth(self):
        """Mouth point of the current pose (lips: 4.5 cm above and 1 cm in front of the chin)."""
        rest_mouth = self.tail["jaw"] + Vector((0, -0.012, 0.045)) * self.k
        hm = self.rest["head"]
        local = hm.inverted() @ rest_mouth
        return self.arm.matrix_world @ self.pb["head"].matrix @ local


# ------------------------------------------------------------------------------- shared poses

def standing_feet(r, y=0.0, wide=0.11, yaw=0.13, dy=(0.0, 0.0)):
    feet = {s: Vector((sx * wide * r.k, y + dy[0 if s == "l" else 1], r.az)) for s, sx in SIDES}
    frot = {s: r.foot_m(s, 0.0, sx * yaw) for s, sx in SIDES}
    return feet, frot


def stand_knees(r, P):
    return {s: Vector((sx * 0.14 * r.k, P.y - 1.5, 0.5 * r.k)) for s, sx in SIDES}


def arms_hang(r, P, prot, swing=None, bend=0.0, out=0.0):
    """Wrists hanging by the thighs; swing[s] in -1..1 (+ = forward), bend lifts the forearm."""
    wr, el, hd = {}, {}, {}
    for s, sx in SIDES:
        sw = (swing or {}).get(s, 0.0)
        sh = r.head["upperarm_" + s]
        local = Vector((sx * (0.225 + out) * r.k, sh.y + 0.03 - 0.17 * r.k * sw,
                        sh.z - 0.485 * r.k + 0.03 * sw * sw + 0.03 * max(0.0, sw) + bend))
        wr[s] = P + prot @ (local - r.pel0)
        el[s] = P + prot @ (Vector((sx * 0.42, sh.y + 0.45, sh.z - 0.15)) - r.pel0)
        hd[s] = r.hand_m(s, prot @ Vector((0, -0.12 - 0.25 * max(0.0, sw), -1)), prot @ Vector((-sx, 0.05, 0)))
    return wr, el, hd


def ahead(r, dist=3.0, dz=0.0, dx=0.0):
    return Vector((dx, -dist, r.head["head"].z + 0.08 * r.k + dz))


def seated_layout(r, tilt=-0.10):
    """Pelvis and feet of seat_anims' seated pose (hip_back 0, feet_fwd 0.42, feet_out 0.13)."""
    thigh, pel = r.head["thigh_l"], r.pel0
    P = Vector((0, 0.0 + (pel.y - thigh.y), r.hip_z + (pel.z - thigh.z)))
    feet = {s: Vector((sx * 0.13 * r.k, -0.42 * r.k, 0.075 * r.k)) for s, sx in SIDES}
    knees = {s: Vector((sx * 0.13 * r.k * 1.1, -1.5, r.hip_z)) for s, sx in SIDES}
    frot = {s: r.foot_m(s, 0.0, sx * 0.08) for s, sx in SIDES}
    return P, Rx(tilt), feet, knees, frot


def seat_elbows(r, out=0.22):
    return {s: Vector((sx * (0.2 + out), 0.45, r.hip_z + 0.05)) for s, sx in SIDES}


def thigh_hands(r, P, prot):
    """Hands resting on the thighs, palms down, fingers towards the knees."""
    wr, hd = {}, {}
    for s, sx in SIDES:
        wr[s] = Vector((sx * 0.135 * r.k, -0.20 * r.k, r.hip_z + 0.115 * r.k))
        hd[s] = r.hand_m(s, Vector((-sx * 0.12, -1, -0.32)), Vector((0, 0, -1)))
    return wr, hd


# --------------------------------------------------------------------------------- the walk

def walk_foot(r, u, T):
    """Ankle position, foot pitch and toe pitch at phase u (0 = heel strike) of one foot.
    Stance 0..0.6: heel rocker, foot flat, heel off about the ball; the ground slides back at WALK_V."""
    k = r.k
    L = WALK_V * T
    heel_l = Vector((0, 0.05 * k, -r.az))                     # heel contact, from the ankle
    ball_l = Vector((0, -0.125 * k, -r.az + 0.008))           # ball contact, from the ankle
    Y0 = -0.29                                                # heel contact at heel strike
    duty = 0.6

    def stance(u):
        g = Y0 + L * u                                        # heel ground point (moves back)
        if u < 0.12:
            p = lerp(-0.26, 0.0, smooth(u / 0.12))
            return Vector((0, g, 0)) - Rx(p) @ heel_l, p, 0.0
        if u < 0.40:
            return Vector((0, g, 0)) - heel_l, 0.0, 0.0
        p = 0.62 * smooth((u - 0.40) / (duty - 0.40)) ** 1.2
        b = Vector((0, g + heel_l.y - ball_l.y, 0))
        b.y = g - (heel_l.y - ball_l.y)
        return b - Rx(p) @ ball_l, p, -p                      # toes stay flat on the ground

    if u < duty:
        a, p, tp = stance(u)
        return a, p, tp
    s = (u - duty) / (1 - duty)
    a0, p0, _ = stance(duty - 1e-6)
    a1, p1, _ = stance(0.0)
    Tsw = (1 - duty) * T
    m0, m1 = 0.25 * WALK_V * Tsw, 0.55 * WALK_V * Tsw          # backward speed at toe-off / heel strike
    h00, h10, h01, h11 = 2 * s ** 3 - 3 * s ** 2 + 1, s ** 3 - 2 * s ** 2 + s, -2 * s ** 3 + 3 * s ** 2, s ** 3 - s ** 2
    y = h00 * a0.y + h10 * m0 + h01 * a1.y + h11 * m1
    z = lerp(a0.z, a1.z, smooth(s)) + 0.055 * k * math.sin(math.pi * s) ** 1.5
    p = lerp(p0, 0.05, smooth(s / 0.45)) if s < 0.45 else lerp(0.05, p1, smooth((s - 0.45) / 0.55))
    tp = lerp(-p0, 0.0, smooth(s / 0.3))
    return Vector((0, y, z)), p, tp


def walk(r, t, T):
    k = r.k
    ph = t / T
    feet, frot, brot = {}, {}, {}
    for s, sx, off in (("l", 1, 0.0), ("r", -1, 0.5)):
        a, p, tp = walk_foot(r, (ph + off) % 1.0, T)
        a.x = sx * 0.095 * k
        feet[s] = a
        frot[s] = r.foot_m(s, p, sx * 0.07)
        brot[s] = r.foot_m(s, p + tp, sx * 0.07, bone="ball")
    c = math.cos(2 * math.pi * ph)
    yaw = -0.075 * c                        # left hip forward at the left heel strike
    roll = -0.05 * math.sin(2 * math.pi * ph)
    prot = Rz(yaw) @ Ry(roll) @ Rx(0.06)
    hz = r.hc0.z - 0.032 * k + 0.018 * k * math.cos(4 * math.pi * (ph - 0.3))
    hc = Vector((0.018 * math.sin(2 * math.pi * ph), 0.02, hz))
    P = r.pelvis_for_hips(hc, prot)
    P0 = P.copy()
    P = r.fit_legs(P, prot, feet, margin=0.006)
    r.clamped = getattr(r, "clamped", 0) + (1 if P.z < P0.z - 1e-4 else 0)
    swing = {"l": -c, "r": c}               # arms opposite to the legs
    wr, el, hd = arms_hang(r, P, Rz(yaw * 0.3), swing, out=0.01)
    look = ahead(r, 4.0)
    r.pose(r.f, P=P, prot=prot, feet=feet, frot=frot, brot=brot, knees=stand_knees(r, P), wrists=wr,
           elbows=el, hands=hd, look=look, lean=0.05, twist=-1.5 * yaw, side=-0.6 * roll, curl=0.38)


# --------------------------------------------------------------------------- standing loops

def stand_body(r, t, w, look, lean=0.02, breath=0.0, extra=None):
    """Standing with the weight shifted by w (-1 right .. +1 left)."""
    k = r.k
    feet, frot = standing_feet(r, 0.0, 0.11, 0.13, dy=(-0.01, 0.02))
    roll = -0.04 * w
    prot = Rz(0.03 * w) @ Ry(roll)
    hc = Vector((0.03 * w * k, 0.005, r.hc0.z))
    P = r.fit_legs(r.pelvis_for_hips(hc, prot), prot, feet, margin=0.012)
    return P, prot, feet, frot, roll


def stand_idle(r, t, T):
    k = r.k
    w = math.sin(2 * math.pi * t / T) * 0.9 + 0.1 * math.sin(2 * math.pi * 3 * t / T)
    br = math.sin(2 * math.pi * 2 * t / T)
    P, prot, feet, frot, roll = stand_body(r, t, w, None)
    yaw = 0.22 * math.sin(2 * math.pi * (t / T + 0.15)) + 0.45 * bump(t, 5.6, 1.1, T) - 0.25 * bump(t, 2.0, 0.8, T)
    look = Vector((math.sin(yaw) * 3.0, -math.cos(yaw) * 3.0, ahead(r).z + 0.05 * math.sin(2 * math.pi * 3 * t / T)))
    P.z += 0.003 * br
    wr, el, hd = arms_hang(r, P, prot, {"l": 0.06 * br, "r": 0.06 * br})
    r.pose(r.f, P=P, prot=prot, feet=feet, frot=frot, knees=stand_knees(r, P), wrists=wr, elbows=el,
           hands=hd, look=look, lean=0.03, side=0.6 * roll, breath=0.025 * br, curl=0.45)


def talk(r, t, T):
    k = r.k
    w = 0.6 * math.sin(2 * math.pi * t / T)
    br = math.sin(2 * math.pi * 2 * t / T)
    P, prot, feet, frot, roll = stand_body(r, t, w, None)
    g1 = bump(t, 1.2, 0.75, T)             # right hand: open palm up, forward
    g2 = bump(t, 3.0, 0.85, T)             # both hands open outwards
    g3 = bump(t, 4.4, 0.6, T)              # left hand points
    zc = P.z + 0.07 * k
    wr, hd, el = {}, {}, {}
    for s, sx in SIDES:
        base = Vector((sx * 0.16 * k, -0.23 * k, zc))
        g = g1 if s == "r" else g3
        up = Vector((sx * 0.13 * k, -0.37 * k, zc + 0.12 * k))
        out = Vector((sx * 0.32 * k, -0.30 * k, zc + 0.08 * k))
        beat = 0.015 * math.sin(2 * math.pi * 12 * t / T) * (g + g2)
        wr[s] = base.lerp(up, g).lerp(out, g2) + Vector((0, 0, beat))
        wr[s] = P + prot @ (wr[s] - Vector((0, 0, P.z))) - P + Vector((P.x, P.y, P.z)) if False else wr[s] + Vector((P.x, 0, 0))
        f_base = Vector((-sx * 0.55, -1, -0.15))
        p_base = Vector((-sx * 0.6, 0, -1))
        f_g = Vector((-sx * 0.15, -1, 0.15))
        p_g = Vector((sx * 0.2, -0.2, 1))
        a = max(g, g2)
        hd[s] = r.hand_m(s, f_base.lerp(f_g, a), p_base.lerp(p_g, a))
        el[s] = Vector((sx * 0.5, 0.35, P.z + 0.15))
    curls = {(s, f): 0.45 - 0.3 * max(g1 if s == "r" else g3, g2) for s in "lr" for f in FINGERS}
    curls[("l", "index")] = 0.45 - 0.42 * max(g3, g2)
    env = 1.0 - bump(t, 5.45, 0.55, T)     # a short pause to listen
    syl = 0.5 + 0.5 * math.sin(2 * math.pi * 27 * t / T)
    jaw = env * 0.07 * syl * (0.65 + 0.35 * math.sin(2 * math.pi * 7 * t / T))
    nod = 0.04 * (g1 + g2 * 0.6) - 0.05 * bump(t, 5.45, 0.5, T)
    look = Vector((0.15 * math.sin(2 * math.pi * t / T), -1.6, ahead(r).z - 0.05 - nod))
    r.pose(r.f, P=P, prot=prot, feet=feet, frot=frot, knees=stand_knees(r, P), wrists=wr, elbows=el,
           hands=hd, look=look, lean=0.04, twist=0.06 * (g1 - g3), side=0.6 * roll, breath=0.02 * br,
           curls=curls, jaw=jaw)


# ---------------------------------------------------------------------------------- seated

SIT_TILT = -0.10


def sit_idle(r, t, T):
    k = r.k
    P, prot, feet, knees, frot = seated_layout(r, SIT_TILT)
    br = math.sin(2 * math.pi * 3 * t / T)
    wr, hd = thigh_hands(r, P, prot)
    sh = 0.012 * bump(t, 6.5, 1.5, T)                         # right hand slides along the thigh
    wr["r"] = wr["r"] + Vector((0, -0.06 * bump(t, 6.5, 1.5, T), sh))
    yaw = 0.35 * math.sin(2 * math.pi * t / T) + 0.15 * math.sin(2 * math.pi * 2 * t / T + 0.5)
    look = Vector((math.sin(yaw) * 2.5, -math.cos(yaw) * 2.5, 1.1 + 0.06 * math.sin(2 * math.pi * 3 * t / T)))
    r.pose(r.f, P=P, prot=prot, feet=feet, frot=frot, knees=knees, wrists=wr, elbows=seat_elbows(r, 0.2),
           hands=hd, look=look, lean=0.02 + 0.04 * bump(t, 6.5, 2.0, T), twist=0.25 * yaw * 0.4,
           breath=0.025 * br, curl=0.35)


STAND_HC_Y = -0.30          # hip centre when standing in front of the chair


def sit_down(r, t, T, reverse=False):
    k = r.k
    u = t / T
    if reverse:
        u = 1 - u
    Ps, ps, feet_s, knees_s, frot_s = seated_layout(r, SIT_TILT)
    feet_st = {s: Vector((sx * 0.115 * k, STAND_HC_Y - 0.03, r.az)) for s, sx in SIDES}
    frot_st = {s: r.foot_m(s, 0.0, sx * 0.1) for s, sx in SIDES}
    c = smoother((u - 0.62) / 0.33)                            # feet slide forward once seated
    feet = {s: feet_st[s].lerp(feet_s[s], c) for s in "lr"}
    frot = {s: slerp_m(frot_st[s], frot_s[s], c) for s in "lr"}
    a = smoother((u - 0.06) / 0.62)                            # descent
    ay = 1 - (1 - a) ** 2                                      # hips go back first, then down
    hc_s = Ps + ps @ (r.hc0 - r.pel0)
    hc_st = Vector((0, STAND_HC_Y, r.hc0.z))
    hc = Vector((0, lerp(hc_st.y, hc_s.y, ay), lerp(hc_st.z, hc_s.z, smoother(a))))
    tilt = lerp(0.0, SIT_TILT, a) + 0.30 * math.sin(math.pi * a)
    prot = Rx(tilt)
    P = r.fit_legs(r.pelvis_for_hips(hc, prot), prot, feet, margin=0.012)
    lean = 0.62 * math.sin(math.pi * min(1.0, a * 1.08)) ** 1.3 + 0.02 * a
    # arms: hanging -> reach forward a little for balance -> hands onto the thighs
    wr_h, el_h, hd_h = arms_hang(r, P, prot)
    wr_t, hd_t = thigh_hands(r, Ps, ps)
    b = smoother((u - 0.35) / 0.45)
    fwd = 0.10 * k * math.sin(math.pi * clamp01((u - 0.1) / 0.6))
    wr = {s: wr_h[s].lerp(wr_t[s], b) + Vector((0, -fwd, 0.02 * math.sin(math.pi * b))) for s in "lr"}
    hd = {s: slerp_m(hd_h[s], hd_t[s], b) for s in "lr"}
    el = {s: el_h[s].lerp(seat_elbows(r, 0.2)[s], b) for s in "lr"}
    knees = {s: stand_knees(r, P)[s].lerp(knees_s[s], a) for s in "lr"}
    look_st = Vector((0, -2.5, ahead(r).z - 0.1))
    look_s = Vector((math.sin(0.0) * 2.5, -2.5, 1.1))
    look = look_st.lerp(look_s, smooth(u)) + Vector((0, 0, -0.35 * math.sin(math.pi * a)))
    r.pose(r.f, P=P, prot=prot, feet=feet, frot=frot, knees=knees, wrists=wr, elbows=el, hands=hd,
           look=look, lean=lean, breath=0.0, curl=lerp(0.4, 0.35, b))


def stand_up(r, t, T):
    sit_down(r, t, T, reverse=True)


EAT_TILT = -0.02


def eat(r, t, T):
    """Two spoonfuls per loop: scoop, lift to the mouth, chew while the hand goes back."""
    k = r.k
    P, prot, feet, knees, frot = seated_layout(r, EAT_TILT)
    tau = t % (T / 2)
    lift = smoother((tau - 0.45) / 0.75) * (1 - smoother((tau - 1.55) / 0.7))
    lean = 0.20 + 0.10 * lift
    br = math.sin(2 * math.pi * 2 * t / T)
    plate = Vector((0, PLATE, TABLE + 0.015))
    scoop = Vector((-0.07, PLATE + 0.03 + 0.04 * math.sin(math.pi * clamp01(tau / 0.45)), TABLE + 0.07))
    left = Vector((0.21 * k, -0.40, TABLE + 0.035))
    look_plate = plate + Vector((0, 0.05, 0))
    look_far = Vector((0.0, -2.0, 1.15))
    lk = smooth((tau - 1.6) / 0.4) * (1 - smooth((tau - 2.7) / 0.3))
    look = look_plate.lerp(look_far, lk) + Vector((0, 0, 0.12 * lift))
    hands = {"l": r.hand_m("l", Vector((-0.45, -1, -0.12)), Vector((0, 0, -1))),
             "r": r.hand_m("r", Vector((0.55, -1, -0.45)), Vector((0.8, 0, -0.6)))}
    el = {"l": Vector((0.55, 0.15, 0.65)), "r": Vector((-0.5, 0.3, 0.55))}
    curls = {("l", f): 0.3 for f in FINGERS}
    curls.update({("r", "thumb"): 0.45, ("r", "index"): 0.55, ("r", "middle"): 0.7, ("r", "ring"): 0.95,
                  ("r", "pinky"): 1.0})
    chew = 0.035 * (0.5 - 0.5 * math.cos(2 * math.pi * 3 * clamp01((tau - 1.55) / 1.35))) \
        * (1 - smooth((tau - 2.75) / 0.15))
    jaw = 0.13 * bump(tau, 1.2, 0.28) + chew
    wr = {"l": left, "r": scoop}
    r.pose(r.f, P=P, prot=prot, feet=feet, frot=frot, knees=knees, wrists=wr, elbows=el, hands=hands,
           look=look, lean=lean, breath=0.02 * br, curls=curls, jaw=jaw)
    if lift > 0:
        bpy.context.view_layer.update()
        m = r.mouth()
        at_mouth = m + Vector((-0.05, -0.15, -0.12)) * k
        r.t_wrist["r"].location = scoop.lerp(at_mouth, lift)
        r.t_wrist["r"].keyframe_insert("location", frame=r.f)
        f_m = (m - at_mouth).normalized() + Vector((0, 0, 0.3))
        hm = slerp_m(hands["r"], r.hand_m("r", f_m, Vector((0.9, 0.1, -0.5))), lift)
        r.t_hand["r"].rotation_quaternion = hm.to_quaternion()
        r.t_hand["r"].keyframe_insert("rotation_quaternion", frame=r.f)


def read(r, t, T):
    """Holding an open book at chest height in both hands; a page turn at 6.0-7.4 s."""
    k = r.k
    P, prot, feet, knees, frot = seated_layout(r, -0.06)
    br = math.sin(2 * math.pi * 3 * t / T)
    book = Vector((0, -0.33 * k, r.hip_z + 0.33 * k))
    wr, hd = {}, {}
    for s, sx in SIDES:
        wr[s] = book + Vector((sx * 0.19 * k, 0.05, -0.07 * k))
        hd[s] = r.hand_m(s, Vector((-sx * 0.3, -0.45, 1)), Vector((-sx * 0.45, 0.8, 0.25)))
    turn = clamp01((t - 6.0) / 1.4)
    reach = math.sin(math.pi * smooth(turn)) if 0 < turn < 1 else 0.0
    sweep = smooth(turn / 0.45) if turn < 0.45 else 1 - smooth((turn - 0.45) / 0.55)
    wr["r"] = wr["r"] + Vector((0.15 * k * sweep, -0.03 * reach, 0.07 * reach))
    hd["r"] = slerp_m(hd["r"], r.hand_m("r", Vector((0.5, -0.8, 0.4)), Vector((0.4, 0.3, -1))), reach)
    # eyes go along the lines and down the page; back up on the page turn
    pg = ((t - 7.4) % T) / (T - 1.4)
    line = (t * 0.55) % 1.0
    sc = line / 0.85 if line < 0.85 else 1 - (line - 0.85) / 0.15
    look = book + Vector(((sc - 0.5) * 0.12, 0.02, 0.06 - 0.12 * clamp01(pg)))
    look = look.lerp(book + Vector((0, 0, 0.04)), reach)
    curls = {(s, f): 0.32 for s in "lr" for f in FINGERS}
    curls.update({(s, "thumb"): 0.08 for s in "lr"})
    curls[("r", "index")] = 0.32 - 0.25 * reach
    r.pose(r.f, P=P, prot=prot, feet=feet, frot=frot, knees=knees, wrists=wr,
           elbows={s: Vector((sx * 0.38, 0.25, r.hip_z + 0.05)) for s, sx in SIDES}, hands=hd, look=look,
           lean=0.14, breath=0.022 * br, curls=curls)


# ------------------------------------------------------------------------------------ lying

LIE = {"dz": 0.0, "dheel": 0.0, "dhand": 0.0}      # calibrated against the mesh in calibrate_lying()


def lie_P(r):
    return Vector((0.0, LIE_Y, MATTRESS + 0.12 + LIE["dz"]))


def lying_targets(r, t, T=10.0, R=None, P=None):
    """Lying on the back (body frame R, pelvis P); t drives the sleep breathing."""
    R = R if R is not None else R_LIE
    P = P if P is not None else lie_P(r)
    k = r.k

    def W(v):                       # rest-space point -> world
        return P + R @ (v - r.pel0)
    feet, frot, knees, wr, el, hd = {}, {}, {}, {}, {}, {}
    for s, sx in SIDES:
        a = r.head["foot_" + s]
        feet[s] = W(Vector((sx * 0.115 * k, a.y + 0.02 * k + LIE["dheel"], a.z + 0.01)))
        frot[s] = r.foot_m(s, 0.55, sx * 0.3, R)
        knees[s] = W(Vector((sx * 0.2, -1.5, 0.5 * k)))
        sh = r.head["upperarm_" + s]
        wr[s] = W(Vector((sx * 0.30 * k, sh.y + 0.03 + LIE["dhand"], sh.z - 0.47 * k)))
        el[s] = W(Vector((sx * 0.55, sh.y + 0.2, sh.z - 0.1)))
        hd[s] = r.hand_m(s, R @ Vector((sx * 0.1, -0.1, -1)), R @ Vector((-sx * 0.35, 1, 0)))
    br = math.sin(2 * math.pi * 3 * t / T)
    roll = 0.25 + 0.04 * math.sin(2 * math.pi * t / T)
    look = W(r.head["head"] + Vector((-math.sin(roll) * 2.0, -math.cos(roll) * 2.0, 0.25)))
    return dict(P=P, prot=R, feet=feet, frot=frot, knees=knees, wrists=wr, elbows=el, hands=hd, look=look,
                lean=-0.02, breath=0.03 * br, curl=0.3)


def sleep(r, t, T):
    d = lying_targets(r, t, T)
    r.pose(r.f, **d)


def lie_down(r, t, T, reverse=False):
    k = r.k
    u = t / T
    if reverse:
        u = 1 - u
    fin = lying_targets(r, 0.0)
    # standing beside the bed, back to it
    feet_st, frot_st = standing_feet(r, 0.0, 0.11, 0.12)
    # sitting on the edge: hip joints 14 cm inside the edge, mattress sinks 3 cm
    s1 = smoother((u - 0.04) / 0.30)
    hc_st = Vector((0, 0.0, r.hc0.z))
    hc_e = Vector((0, BED_EDGE + 0.14, MATTRESS - 0.03 + 0.138 * k))   # buttocks 13.8 cm below the hips
    ay = 1 - (1 - s1) ** 2
    hc1 = Vector((0, lerp(hc_st.y, hc_e.y, ay), lerp(hc_st.z, hc_e.z, smoother(s1))))
    tilt1 = -0.08 * s1 + 0.25 * math.sin(math.pi * s1)
    lean1 = 0.5 * math.sin(math.pi * s1) ** 1.3 + 0.12 * s1
    # lying back and turning onto the bed
    s2 = smoother((u - 0.30) / 0.55)
    R1 = Rx(tilt1)
    R = slerp_m(R1, fin["prot"], s2)
    P1 = r.pelvis_for_hips(hc1, R1)
    P = P1.lerp(fin["P"], s2) + Vector((0, 0, 0.09 * math.sin(math.pi * s2)))
    lean = lerp(lean1, fin["lean"], s2)
    # legs: planted, then knees up and swung onto the bed
    s3 = smoother((u - 0.36) / 0.50)
    feet, frot = {}, {}
    for s, sx in SIDES:
        a = feet_st[s]
        mid = Vector((sx * 0.10 - 0.45, -0.12, MATTRESS + 0.42))
        c = fin["feet"][s]
        v = smooth(s3 if s == "r" else s3 * 1.05 - 0.05)
        feet[s] = (1 - v) ** 2 * a + 2 * (1 - v) * v * mid + v * v * c
        frot[s] = slerp_m(frot_st[s], fin["frot"][s], v)
    if s3 <= 0:
        P = r.fit_legs(P, R, feet, margin=0.012)
    knees = {s: (stand_knees(r, P)[s]).lerp(fin["knees"][s], s2) if s2 > 0 else stand_knees(r, P)[s] for s in "lr"}
    for s in "lr":                     # while sitting the knees point forward
        if s2 <= 0:
            knees[s] = Vector((knees[s].x, -1.5, 0.5))
        else:
            knees[s] = Vector((knees[s].x, -1.5, 0.5)).lerp(fin["knees"][s], s2)
    # arms: hanging -> pushing on the mattress beside the hips -> by the sides lying
    wr_h, el_h, hd_h = arms_hang(r, P, R1)
    push = {s: Vector((sx * 0.30 * k, BED_EDGE + 0.20, MATTRESS + 0.07)) for s, sx in SIDES}
    hd_p = {s: r.hand_m(s, Vector((sx * 0.25, -1, -0.12)), Vector((0, 0, -1))) for s, sx in SIDES}
    s4 = {"r": smoother((u - 0.30) / 0.30), "l": smoother((u - 0.50) / 0.35)}
    cur = lying_targets(r, 0.0, R=R, P=P)          # arms by the sides in the current body frame
    wr, hd, el = {}, {}, {}
    for s, sx in SIDES:
        a = smoother((u - 0.10) / 0.22)
        w0 = wr_h[s].lerp(push[s], a)
        h0 = slerp_m(hd_h[s], hd_p[s], a)
        lift = Vector((0, 0, 0.12 * math.sin(math.pi * s4[s])))
        wr[s] = w0.lerp(cur["wrists"][s], s4[s]) + lift
        hd[s] = slerp_m(h0, cur["hands"][s], s4[s])
        el[s] = (el_h[s].lerp(Vector((sx * 0.6, BED_EDGE, 1.0)), a)).lerp(cur["elbows"][s], s4[s])
    look0 = Vector((0, -2.5, ahead(r).z - 0.2 * math.sin(math.pi * s1)))
    look = look0.lerp(fin["look"], smooth((u - 0.35) / 0.5))
    r.pose(r.f, P=P, prot=R, feet=feet, frot=frot, knees=knees, wrists=wr, elbows=el, hands=hd, look=look,
           lean=lean, breath=0.0, curl=lerp(0.4, fin["curl"], s2))


def get_up(r, t, T):
    lie_down(r, t, T, reverse=True)


MAKERS = {"Walk": walk, "StandIdle": stand_idle, "Talk": talk, "SitDown": sit_down, "StandUp": stand_up,
          "SitIdle": sit_idle, "Eat": eat, "Read": read, "LieDown": lie_down, "Sleep": sleep, "GetUp": get_up}


# ------------------------------------------------------------------------------- mesh checks

REGIONS = {"torso": ("pelvis", "spine_"), "thigh": ("thigh_",), "calf": ("calf_",), "foot": ("foot_", "ball_"),
           "head": ("head", "neck_", "jaw", "eye_"), "arm": ("upperarm_", "lowerarm_", "clavicle_"),
           "hand": ("hand_", "thumb_", "index_", "middle_", "ring_", "pinky_")}


class Meshes:
    """Evaluated vertex positions of the character's meshes, labelled by the dominant bone region."""

    def __init__(self, arm):
        self.objs = [o for o in bpy.data.objects if o.type == "MESH" and o.parent == arm and not o.name.startswith("life_")]
        self.labels = {}
        names = list(REGIONS)
        bones = set(arm.data.bones.keys())
        for o in self.objs:
            gname = {g.index: g.name for g in o.vertex_groups}
            helper = {g.index for g in o.vertex_groups if g.name in ("HelperGeometry", "JointCubes")}
            lab = np.full(len(o.data.vertices), -1, dtype=np.int16)
            for v in o.data.vertices:
                best, bw = None, 0.0
                if any(g.group in helper and g.weight > 0.5 for g in v.groups):
                    continue                         # MakeHuman helper geometry (deleted on export)
                for g in v.groups:
                    nm = gname.get(g.group)
                    if nm in bones and g.weight > bw:
                        best, bw = nm, g.weight
                if best:
                    for i, nm in enumerate(names):
                        if best.startswith(REGIONS[nm]):
                            lab[v.index] = i
                            break
            self.labels[o.name] = lab

    def masks(self, on):
        for o in self.objs:
            for m in o.modifiers:
                if m.type == "MASK":
                    m.show_viewport = on

    def points(self):
        dg = bpy.context.evaluated_depsgraph_get()
        out = {nm: [] for nm in REGIONS}
        names = list(REGIONS)
        for o in self.objs:
            ev = o.evaluated_get(dg)
            me = ev.data
            n = len(me.vertices)
            lab = self.labels[o.name]
            if n != len(lab):
                continue
            co = np.empty(n * 3)
            me.vertices.foreach_get("co", co)
            co = co.reshape(-1, 3)
            mw = np.array(o.matrix_world)
            co = co @ mw[:3, :3].T + mw[:3, 3]
            for i, nm in enumerate(names):
                out[nm].append(co[lab == i])
        return {nm: (np.concatenate(v) if v else np.zeros((0, 3))) for nm, v in out.items()}


def inside(p, x0, x1, y0, y1):
    return p[(p[:, 0] > x0) & (p[:, 0] < x1) & (p[:, 1] > y0) & (p[:, 1] < y1)]


def zmin(p):
    return float(p[:, 2].min()) if len(p) else float("nan")


def calibrate_lying(r, meshes):
    """Puts the back on the mattress (1 cm sink), heels and hands resting on it."""
    for it in range(3):
        r.setup()
        r.f = 1
        r.pose(1, **lying_targets(r, 0.0))
        bpy.context.scene.frame_set(1)
        bpy.context.view_layer.update()
        pts = meshes.points()
        torso = zmin(np.concatenate([pts["torso"], pts["thigh"]]))
        heel = zmin(pts["foot"])
        hand = zmin(pts["hand"])
        head = zmin(pts["head"])
        print("LIFE calibrate %d: torso/thigh min %.3f heel %.3f hand %.3f head %.3f" % (it, torso, heel, hand, head))
        if any(v != v for v in (torso, heel, hand, head)):
            raise RuntimeError("lying calibration: a body region has no vertices")
        LIE["dz"] += (MATTRESS - 0.01) - torso
        LIE["dheel"] += heel - (MATTRESS - 0.005)
        LIE["dhand"] += hand - (MATTRESS - 0.003)
        r.teardown()
        clear_anim(r.arm)
    LIE["head_gap"] = head - MATTRESS
    print("LIFE lying calibration", LIE)


def clear_anim(arm):
    if arm.animation_data:
        a = arm.animation_data.action
        arm.animation_data.action = None
        if a and a.users == 0:
            bpy.data.actions.remove(a)
    for pb in arm.pose.bones:
        pb.location = (0, 0, 0)
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.rotation_euler = (0, 0, 0)
        pb.scale = (1, 1, 1)


# ------------------------------------------------------------------------------- bake & check

def check(arm, action, frames):
    """seat_anims.check: forearms/hands through each other or into the torso."""
    arm.animation_data.action = action
    bad = []

    def seg(b):
        pb = arm.pose.bones[b]
        return arm.matrix_world @ pb.head, arm.matrix_world @ pb.tail

    def dist(s1, s2):
        best = 1e9
        for i in range(11):
            p = s1[0].lerp(s1[1], i / 10)
            for j in range(11):
                best = min(best, (p - s2[0].lerp(s2[1], j / 10)).length)
        return best
    for f in frames:
        bpy.context.scene.frame_set(f)
        fa = dist(seg("lowerarm_l"), seg("lowerarm_r"))
        if fa < 0.07:
            bad.append("f%d forearms %.3f m apart" % (f, fa))
        for s in "lr":
            sp = (seg("spine_01")[0], seg("spine_03")[1])
            d = dist(seg("lowerarm_" + s), sp)
            if d < 0.1:
                bad.append("f%d lowerarm_%s %.3f m from the spine" % (f, s, d))
            d = dist(seg("hand_" + s), sp)
            if d < 0.09:
                bad.append("f%d hand_%s %.3f m from the spine" % (f, s, d))
        d = dist(seg("thigh_l"), seg("thigh_r"))
        if d < 0.12:
            bad.append("f%d thighs %.3f m apart" % (f, d))
    return bad


def bake(arm, name, meshes):
    sc = bpy.context.scene
    T, loops = CLIPS[name]
    n = int(round(T * FPS))
    r = Rig(arm)
    r.setup()
    arm.animation_data_create()
    arm.animation_data.action = bpy.data.actions.new("Life_%s_keys" % name)
    ik_err, ik_worst = 0.0, ""
    for f in range(n + 1):
        r.f = f + 1
        MAKERS[name](r, f / FPS, T)
        sc.frame_set(r.f)
        for s in "lr":
            for b, tg in (("calf_" + s, r.t_foot[s]), ("lowerarm_" + s, r.t_wrist[s])):
                e = (r.posed(b, True) - tg.matrix_world.translation).length
                if e > ik_err:
                    ik_err, ik_worst = e, "%s f%d" % (b, r.f)
    bpy.context.view_layer.objects.active = arm
    for o in bpy.context.selected_objects:
        o.select_set(False)
    arm.select_set(True)
    bpy.ops.object.mode_set(mode="POSE")
    bpy.ops.nla.bake(frame_start=1, frame_end=n + 1, only_selected=False, visual_keying=True,
                     clear_constraints=False, use_current_action=False, bake_types={"POSE"})
    baked = arm.animation_data.action
    baked.name = "Life_" + name
    baked.use_fake_user = True
    # frames 1..n+1, exported slid to t = 0: UE length = T exactly, a loop's last key equals its first
    bpy.ops.object.mode_set(mode="OBJECT")
    keys = bpy.data.actions.get("Life_%s_keys" % name)
    if keys:
        bpy.data.actions.remove(keys)
    r.teardown()
    for h in list(bpy.data.actions):
        if h.name.startswith(arm.name + "_") and h.users == 0:
            bpy.data.actions.remove(h)
    info = {"seconds": T, "frames": n + 1, "loop": loops, "ik_error_m": round(ik_err, 4), "ik_worst": ik_worst}
    if name == "Walk":
        info["clamped_frames"] = getattr(r, "clamped", 0)
    probs = check(arm, baked, range(1, n + 2, 3))
    info["clearance"] = probs[:6] or "clean"
    info.update(mesh_report(arm, baked, name, n, meshes))
    arm.animation_data.action = None
    clear_anim(arm)
    print("LIFE %s %s" % (name, json.dumps(info)))
    return baked, info


def mesh_report(arm, action, name, n, meshes):
    """Floor / seat / table / mattress clearance measured on the skinned meshes."""
    arm.animation_data.action = action
    sc = bpy.context.scene
    out = {}
    frames = sorted(set(list(range(1, n + 2, max(1, n // 12))) + [n + 1]))
    floor, seat, table, bed, edge = [], [], [], [], []
    for f in frames:
        sc.frame_set(f)
        pts = meshes.points()
        allp = np.concatenate([v for v in pts.values() if len(v)])
        floor.append(zmin(allp))
        if name in ("SitIdle", "Eat", "Read", "SitDown", "StandUp"):
            body = np.concatenate([pts["torso"], pts["thigh"]])
            seat.append(zmin(inside(body, -0.22, 0.22, -0.22, 0.22)))
        if name == "Eat":
            ah = np.concatenate([pts["arm"], pts["hand"]])
            table.append(zmin(inside(ah, -0.6, 0.6, -1.0, TABLE_EDGE)))
            leg = np.concatenate([pts["thigh"], pts["calf"]])
            under = inside(leg, -0.6, 0.6, -1.0, TABLE_EDGE)
            out.setdefault("thigh_top_under_table", [])
            out["thigh_top_under_table"].append(round(float(under[:, 2].max()), 3) if len(under) else None)
        if name in ("LieDown", "Sleep", "GetUp"):
            bed.append(zmin(inside(allp, -1.05, 0.95, BED_EDGE, 1.0)))
    out["min_z_floor"] = round(min(floor), 3)
    if seat:
        out["seat_contact_min_z"] = [round(v, 3) for v in seat if v == v]
    if table:
        out["arms_over_table_min_z"] = round(min(table), 3)
    if bed:
        out["bed_footprint_min_z"] = [round(v, 3) for v in bed]
    if name in ("LieDown", "Sleep", "GetUp", "SitIdle", "Walk", "StandIdle"):
        f_end = n + 1 if name == "LieDown" else 1
        sc.frame_set(f_end)
        pb = arm.pose.bones

        def ue(v):
            return [round(v.x * 100, 1), round(-v.y * 100, 1), round(v.z * 100, 1)]
        out["ue_cm_frame%d" % f_end] = {"pelvis": ue(arm.matrix_world @ pb["pelvis"].head),
                                        "head_top": ue(arm.matrix_world @ pb["head"].tail),
                                        "ankle_l": ue(arm.matrix_world @ pb["foot_l"].head),
                                        "ankle_r": ue(arm.matrix_world @ pb["foot_r"].head)}
    if name == "Walk":
        # foot slip: the flat stance foot's ankle must move back at WALK_V
        vs = []
        for f in range(1, n + 1):
            sc.frame_set(f)
            a0 = arm.matrix_world @ arm.pose.bones["foot_l"].head
            sc.frame_set(f + 1)
            a1 = arm.matrix_world @ arm.pose.bones["foot_l"].head
            u = (f - 1) / n
            if 0.14 < u < 0.38:
                vs.append((a1.y - a0.y) * FPS)
        out["stance_ankle_speed_cm_s"] = round(100 * sum(vs) / len(vs), 1)
    arm.animation_data.action = None
    return out


# ------------------------------------------------------------------------------- preview

def props():
    """Simple furniture boxes for the preview renders."""
    def box(name, x0, x1, y0, y1, z0, z1, col):
        bpy.ops.mesh.primitive_cube_add(size=1, location=((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2))
        o = bpy.context.active_object
        o.name = "life_" + name
        o.scale = (x1 - x0, y1 - y0, z1 - z0)
        o.color = col
        return o
    p = {}
    p["chair"] = [box("seat", -0.22, 0.22, -0.22, 0.22, SEAT - 0.04, SEAT, (0.45, 0.3, 0.2, 1)),
                  box("back", -0.22, 0.22, 0.22, 0.25, SEAT, SEAT + 0.45, (0.45, 0.3, 0.2, 1))]
    for i, (x, y) in enumerate(((-0.2, -0.2), (0.2, -0.2), (-0.2, 0.2), (0.2, 0.2))):
        p["chair"].append(box("leg%d" % i, x - 0.015, x + 0.015, y - 0.015, y + 0.015, 0, SEAT - 0.04, (0.45, 0.3, 0.2, 1)))
    p["table"] = [box("table", -0.55, 0.55, -1.0, TABLE_EDGE, TABLE - 0.03, TABLE, (0.6, 0.55, 0.45, 1)),
                  box("plate", -0.11, 0.11, PLATE - 0.11, PLATE + 0.11, TABLE, TABLE + 0.015, (0.9, 0.9, 0.9, 1))]
    p["bed"] = [box("mattress", -1.05, 0.95, BED_EDGE, BED_EDGE + 0.9, 0.30, MATTRESS, (0.75, 0.78, 0.85, 1)),
                box("bedframe", -1.08, 0.98, BED_EDGE + 0.03, BED_EDGE + 0.93, 0.0, 0.30, (0.35, 0.25, 0.2, 1))]
    p["floor"] = [box("floor", -3, 3, -3, 3, -0.02, 0.0, (0.5, 0.5, 0.5, 1))]
    return p


SHOTS = {   # clip: (props, eye, target, lens)
    "Walk": ([], (3.4, -0.6, 1.0), (0, 0, 0.85), 45),
    "StandIdle": ([], (1.6, -3.2, 1.2), (0, 0, 0.9), 45),
    "Talk": ([], (1.4, -3.0, 1.25), (0, 0, 1.0), 45),
    "SitDown": (["chair"], (3.0, -1.6, 1.0), (0, -0.15, 0.75), 40),
    "StandUp": (["chair"], (3.0, -1.6, 1.0), (0, -0.15, 0.75), 40),
    "SitIdle": (["chair"], (1.8, -2.4, 1.1), (0, -0.1, 0.75), 45),
    "Eat": (["chair", "table"], (2.0, -1.4, 1.4), (0, -0.25, 0.9), 40),
    "Read": (["chair"], (1.9, -1.9, 1.2), (0, -0.15, 0.85), 45),
    "LieDown": (["bed"], (1.2, -3.0, 2.0), (0, 0.35, 0.65), 32),
    "Sleep": (["bed"], (0.8, -2.2, 2.3), (0, 0.5, 0.55), 35),
    "GetUp": (["bed"], (1.2, -3.0, 2.0), (0, 0.35, 0.65), 32),
}


def render_previews(arm, actions):
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.display.shading.light = "STUDIO"
    sc.display.shading.color_type = "TEXTURE"
    sc.display.shading.show_shadows = True
    sc.render.resolution_x, sc.render.resolution_y = 420, 520
    sc.render.film_transparent = False
    pr = props()
    for o in [o for v in pr.values() for o in v]:
        o.data.materials.clear()
    sc.display.shading.color_type = "TEXTURE"
    cam = bpy.data.objects.new("life_cam", bpy.data.cameras.new("life_cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    frames_dir = os.path.join(OUT, "frames")
    os.makedirs(frames_dir, exist_ok=True)
    listing = {}
    for name, act in actions.items():
        use, eye, tgt, lens = SHOTS[name]
        for key, objs in pr.items():
            for o in objs:
                o.hide_render = not (key in use or key == "floor")
        cam.location = eye
        cam.data.lens = lens
        cam.rotation_euler = (Vector(tgt) - Vector(eye)).to_track_quat("-Z", "Y").to_euler()
        arm.animation_data.action = act
        n = CLIPS[name][0] * FPS
        picks = [1 + round(n * i / 5) for i in range(6)]
        listing[name] = []
        for f in picks:
            sc.frame_set(f)
            path = os.path.join(frames_dir, "%s_%03d.png" % (name, f))
            sc.render.filepath = path
            bpy.ops.render.render(write_still=True)
            listing[name].append([path, round((f - 1) / FPS, 2)])
    arm.animation_data.action = None
    for objs in pr.values():
        for o in objs:
            bpy.data.objects.remove(o)
    bpy.data.objects.remove(cam)
    with open(os.path.join(OUT, "frames.json"), "w") as fh:
        json.dump(listing, fh, indent=1)
    subprocess.run(["/usr/bin/python3", os.path.abspath(__file__), "--sheet", OUT], check=False)


def make_sheet(out_dir):
    from PIL import Image, ImageDraw
    listing = json.load(open(os.path.join(out_dir, "frames.json")))
    rows = list(listing.items())
    w, h, lab = 210, 260, 120
    sheet = Image.new("RGB", (lab + w * 6, h * len(rows)), (24, 24, 24))
    d = ImageDraw.Draw(sheet)
    for i, (name, frames) in enumerate(rows):
        d.text((8, i * h + h // 2 - 6), name, fill=(240, 240, 240))
        for j, (path, sec) in enumerate(frames):
            im = Image.open(path).convert("RGB").resize((w, h))
            sheet.paste(im, (lab + j * w, i * h))
            d.text((lab + j * w + 4, i * h + 4), "%.2fs" % sec, fill=(255, 255, 0))
    p = os.path.join(out_dir, "life_anims_sheet.png")
    sheet.save(p)
    print("LIFE sheet", p)


# ------------------------------------------------------------------------------------ export

def export_glb(arm, actions, path):
    """Armature + a one-triangle proxy mesh (Interchange needs a skinned mesh) with only our clips."""
    me = bpy.data.meshes.new("life_proxy")
    me.from_pydata([(0, 0, 0), (0.01, 0, 0), (0, 0, 0.01)], [], [(0, 1, 2)])
    proxy = bpy.data.objects.new("life_proxy", me)
    bpy.context.scene.collection.objects.link(proxy)
    proxy.parent = arm
    vg = proxy.vertex_groups.new(name="root")
    vg.add([0, 1, 2], 1.0, "REPLACE")
    proxy.modifiers.new("Armature", "ARMATURE").object = arm
    keep = set(a.name for a in actions.values())
    for a in list(bpy.data.actions):
        if a.name not in keep:
            bpy.data.actions.remove(a)
    clear_anim(arm)
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    proxy.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.gltf(filepath=path, use_selection=True, export_format="GLB",
                              export_skins=True, export_morph=False, export_materials="NONE",
                              export_animations=True, export_animation_mode="ACTIONS", export_anim_slide_to_zero=True,
                              export_force_sampling=True, export_optimize_animation_size=False,
                              export_anim_single_armature=True, export_reset_pose_bones=True,
                              export_def_bones=False, export_yup=True, export_apply=False)
    print("LIFE exported", path)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    os.makedirs(OUT, exist_ok=True)
    sc = bpy.context.scene
    sc.render.fps = FPS
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    clear_anim(arm)
    meshes = Meshes(arm)
    meshes.masks(False)            # measure the whole skin (masks only hide it under the clothes)
    r = Rig(arm)
    print("LIFE rig %s height %.3f k %.3f leg %.3f hip_z %.3f" % (arm.name, r.height, r.k, r.leg["l"], r.hip_z))
    calibrate_lying(r, meshes)
    names = [a for a in argv if not a.startswith("--")] or list(CLIPS)
    actions, report = {}, {"rig": arm.name, "height_m": round(r.height, 3), "lying_calibration": dict(LIE)}
    for name in names:
        actions[name], report[name] = bake(arm, name, meshes)
    meshes.masks(True)
    with open(os.path.join(OUT, "life_anims_report.json"), "w") as fh:
        json.dump(report, fh, indent=1)
    if "--no-preview" not in argv:
        render_previews(arm, actions)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "life_anims.blend"))
    if "--no-export" not in argv:
        export_glb(arm, actions, os.path.join(OUT, "life_anims.glb"))


if __name__ == "__main__":
    if bpy is None:
        if "--sheet" in sys.argv:
            make_sheet(sys.argv[sys.argv.index("--sheet") + 1])
    else:
        main()
