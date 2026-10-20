"""Seated loops for the club humans, baked per character on the shared UE skeleton.

Character space (Blender): origin on the floor under the chair-seat centre, facing -Y; desk in front.
Seat top 0.47 m, desk top 0.76 m, keyboard centre 0.55 m in front, mouse 0.25 m to the right of it,
monitor centre ~1.10 m high, 0.95 m in front. In UE the meshes face +Y, so an actor faces the desk
along +Y of its local frame (rotate the actor so its +Y points at the screen).

Each loop is posed with IK targets (hands, feet) + look-at, animated with periodic functions so it
loops seamlessly, baked to FK keys on every deforming bone, then the helpers are removed.

  Sit_KeyboardMouse  playing on keyboard + mouse: WASD fingers, mouse flicks and clicks, leaning in
  Sit_Gamepad        playing with a gamepad held in front of the chest, thumbs on the sticks
  Sit_Browse         surfing: right hand scrolls/clicks the mouse, left forearm on the desk
  Sit_Idle           the owner in his booth: leaning back, hands on the desk, looking around
"""
import math

import bpy
from mathutils import Matrix, Vector

FPS = 30
SEAT, DESK = 0.47, 0.76
KEYS = Vector((0.0, -0.55, DESK + 0.015))
MOUSE = Vector((-0.26, -0.52, DESK + 0.02))   # right hand: the character's right is -X
SCREEN = Vector((0.0, -0.95, 1.12))

LOOPS = {"Sit_KeyboardMouse": 4.0, "Sit_Gamepad": 6.0, "Sit_Browse": 8.0, "Sit_Idle": 12.0}
FINGERS = ["thumb", "index", "middle", "ring", "pinky"]


def _w(arm, name):
    return arm.matrix_world @ arm.data.bones[name].head_local


def _empty(name, loc=(0, 0, 0)):
    e = bpy.data.objects.new(name, None)
    bpy.context.scene.collection.objects.link(e)
    e.location = loc
    return e


def _wave(t, period, phase=0.0):
    return math.sin(2 * math.pi * (t / period + phase))


class Rig:
    def __init__(self, arm):
        self.arm = arm
        self.pb = arm.pose.bones
        self.height = _w(arm, "head").z + 0.12
        self.k = self.height / 1.75                         # body scale vs an adult man
        self.hip_z = SEAT + 0.095 * self.k
        self.helpers = []
        self.cons = []

    # ---------------------------------------------------------------- helpers / constraints
    def helper(self, name, loc=(0, 0, 0)):
        e = _empty(self.arm.name + "_" + name, loc)
        self.helpers.append(e)
        return e

    def con(self, bone, kind, **kw):
        c = self.pb[bone].constraints.new(kind)
        for k, v in kw.items():
            setattr(c, k, v)
        self.cons.append((bone, c))
        return c

    def setup(self):
        a = self.arm
        # pelvis: a helper carrying the seated hip position/tilt
        self.t_pelvis = self.helper("pelvis")
        self.con("pelvis", "COPY_LOCATION", target=self.t_pelvis)
        # legs: IK to foot targets, knees pointing forward
        self.t_foot, self.t_knee = {}, {}
        for s in "lr":
            self.t_foot[s] = self.helper("foot_" + s)
            self.t_knee[s] = self.helper("knee_" + s)
            self.con("calf_" + s, "IK", target=self.t_foot[s], pole_target=self.t_knee[s],
                     pole_angle=math.radians(-90), chain_count=2)
        # arms: IK to wrist targets, elbows out/back; hand orientation from a helper
        self.t_wrist, self.t_elbow, self.t_hand = {}, {}, {}
        for s in "lr":
            self.t_wrist[s] = self.helper("wrist_" + s)
            self.t_elbow[s] = self.helper("elbow_" + s)
            self.t_hand[s] = self.helper("hand_" + s)
            self.con("lowerarm_" + s, "IK", target=self.t_wrist[s], pole_target=self.t_elbow[s],
                     pole_angle=0.0, chain_count=2)
            self.con("hand_" + s, "COPY_ROTATION", target=self.t_hand[s])
        # head + neck look at a target
        self.t_look = self.helper("look")
        self.con("neck_01", "DAMPED_TRACK", target=self.t_look, track_axis="TRACK_Z", influence=0.35)
        self.con("head", "DAMPED_TRACK", target=self.t_look, track_axis="TRACK_Z", influence=0.8)
        # eyes follow too
        for s in "lr":
            self.con("eye_" + s, "DAMPED_TRACK", target=self.t_look, track_axis="TRACK_Y")
        self.rest = {b.name: (a.matrix_world @ b.matrix_local) for b in a.data.bones}

    def teardown(self):
        for bone, c in self.cons:
            self.pb[bone].constraints.remove(c)
        for e in self.helpers:
            bpy.data.objects.remove(e)

    # ------------------------------------------------------------------------- poses
    def hand_rot(self, s, forward, palm):
        """World rotation for hand_<s> so the bone points along `forward` and the palm faces `palm`."""
        r = self.rest["hand_" + s].to_3x3().normalized()
        y0 = r.col[1]
        # palm normal at rest: the local axis pointing most toward the body centre-line
        inward = Vector((-1 if s == "l" else 1, 0, 0))
        cand = [r.col[0], -r.col[0], r.col[2], -r.col[2]]
        p0 = max(cand, key=lambda v: v.dot(inward))
        p0 = (p0 - y0 * p0.dot(y0)).normalized()
        f = forward.normalized()
        p = (palm - f * palm.dot(f)).normalized()
        f0 = Matrix((y0, p0, y0.cross(p0))).transposed()
        f1 = Matrix((f, p, f.cross(p))).transposed()
        return (f1 @ f0.transposed() @ r).to_euler()

    def finger_curl(self, s, finger, amount):
        """Curls one finger by bending its three bones about their local X axes (radians)."""
        for i, w in ((1, 0.6), (2, 1.0), (3, 0.8)):
            b = self.pb["%s_0%d_%s" % (finger, i, s)]
            b.rotation_mode = "XYZ"
            ang = amount * w
            if finger == "thumb":
                b.rotation_euler = (0, 0, ang * (0.5 if s == "l" else -0.5))
            else:
                b.rotation_euler = (ang, 0, 0)
            b.keyframe_insert("rotation_euler", frame=self.frame)

    def spine(self, lean, twist=0.0, breath=0.0):
        """Forward lean (radians, + = toward the desk) spread over spine_01..03."""
        for name, w in (("spine_01", 0.35), ("spine_02", 0.35), ("spine_03", 0.3)):
            b = self.pb[name]
            b.rotation_mode = "XYZ"
            b.rotation_euler = (lean * w + breath * (w if name == "spine_03" else 0), twist * w, 0)
            b.keyframe_insert("rotation_euler", frame=self.frame)

    def place(self, frame, *, hip_back=0.03, tilt=0.0, feet_fwd=0.42, feet_out=0.13,
              wrists=None, hands=None, elbows_out=0.22, look=SCREEN, lean=0.1, twist=0.0, breath=0.0,
              curls=None):
        self.frame = frame
        k = self.k
        thigh = _w(self.arm, "thigh_l")
        pel = _w(self.arm, "pelvis")
        self.t_pelvis.location = Vector((0, hip_back + (pel.y - thigh.y), self.hip_z + (pel.z - thigh.z)))
        pb = self.pb["pelvis"]
        pb.rotation_mode = "XYZ"
        pb.rotation_euler = (tilt, 0, 0)
        pb.keyframe_insert("rotation_euler", frame=frame)
        for s, sx in (("l", 1), ("r", -1)):
            self.t_foot[s].location = (sx * feet_out * k, -feet_fwd * k, 0.075 * k)
            self.t_knee[s].location = (sx * feet_out * k * 1.1, -1.5, self.hip_z)
            self.t_wrist[s].location = wrists[s]
            self.t_elbow[s].location = (sx * (0.2 + elbows_out), 0.45, self.hip_z + 0.05)
            f, p = hands[s]
            self.t_hand[s].rotation_euler = self.hand_rot(s, f, p)
        self.t_look.location = look
        for e in self.helpers:
            e.keyframe_insert("location", frame=frame)
            e.keyframe_insert("rotation_euler", frame=frame)
        self.spine(lean, twist, breath)
        for s in "lr":
            for fi, fin in enumerate(FINGERS):
                self.finger_curl(s, fin, (curls or {}).get((s, fin), 0.35))


# ------------------------------------------------------------------------------- loops

DOWN, FWD = Vector((0, 0, -1)), Vector((0, -1, 0))


def keyboard_mouse(r, t, T):
    k = r.k
    aim = Vector((0.015 * _wave(t, T / 2) + 0.01 * _wave(t, T / 4, 0.3), 0.008 * _wave(t, T / 2, 0.25), 0))
    flick = 0.02 * max(0, _wave(t, T, 0.6)) ** 6
    wr = {"l": KEYS + Vector((0.11, 0.095, 0.035)),
          "r": MOUSE + Vector((0.0, 0.1, 0.03)) + aim + Vector((flick, 0, 0))}
    hands = {"l": (FWD + Vector((-0.3, 0, -0.15)), DOWN), "r": (FWD + Vector((0.1, 0, -0.15)), DOWN)}
    curls = {}
    for i, fin in enumerate(FINGERS):            # WASD: fingers press in a rhythm
        curls[("l", fin)] = 0.35 + 0.25 * max(0, _wave(t, T / 4, i * 0.21)) ** 3
    click = max(0, _wave(t, T / 2, 0.1)) ** 12
    curls.update({("r", "index"): 0.3 + 0.25 * click, ("r", "middle"): 0.35, ("r", "thumb"): 0.2,
                  ("r", "ring"): 0.6, ("r", "pinky"): 0.7})
    look = SCREEN + Vector((0.05 * _wave(t, T, 0.1), 0, 0.02 * _wave(t, T / 2)))
    r.place(r.f, wrists=wr, hands=hands, lean=0.18, tilt=0.04, look=look, curls=curls, breath=0.02 * _wave(t, T / 2))


def gamepad(r, t, T):
    k = r.k
    c = Vector((0, -0.2 * k, r.hip_z + 0.2 * k + 0.01 * _wave(t, T / 3)))
    wr = {"l": c + Vector((0.075, 0.07, 0)), "r": c + Vector((-0.075, 0.07, 0))}
    hands = {"l": (Vector((-0.5, -1, 0.1)), Vector((-1, 0, 0.6))),
             "r": (Vector((0.5, -1, 0.1)), Vector((1, 0, 0.6)))}
    curls = {(s, f): 0.9 for s in "lr" for f in ("middle", "ring", "pinky")}
    for s, ph in (("l", 0.0), ("r", 0.4)):
        curls[(s, "index")] = 0.5 + 0.2 * max(0, _wave(t, T / 3, ph)) ** 4
        curls[(s, "thumb")] = 0.25 + 0.2 * _wave(t, T / 5, ph)
    look = SCREEN + Vector((0.06 * _wave(t, T / 2), 0, 0.02 * _wave(t, T, 0.3)))
    r.place(r.f, wrists=wr, hands=hands, lean=-0.04, tilt=-0.12, hip_back=0.06, look=look, curls=curls,
            elbows_out=0.12, breath=0.02 * _wave(t, T / 3))


def browse(r, t, T):
    scroll = max(0, _wave(t, T / 4)) ** 4
    wr = {"l": Vector((0.2, -0.36, DESK + 0.06)),
          "r": MOUSE + Vector((0.0, 0.1, 0.03)) + Vector((0.01 * _wave(t, T, 0.2), 0.01 * _wave(t, T / 2), 0))}
    hands = {"l": (Vector((-0.6, -1, -0.1)), DOWN), "r": (FWD + Vector((0.1, 0, -0.15)), DOWN)}
    curls = {("r", "index"): 0.3 + 0.3 * scroll, ("r", "middle"): 0.35, ("r", "thumb"): 0.2,
             ("r", "ring"): 0.6, ("r", "pinky"): 0.7}
    curls.update({("l", f): 0.5 for f in FINGERS})
    look = SCREEN + Vector((0.04 * _wave(t, T, 0.5), 0, -0.04 + 0.05 * _wave(t, T / 2)))
    r.place(r.f, wrists=wr, hands=hands, lean=0.06, tilt=-0.08, hip_back=-0.02, look=look, curls=curls,
            twist=0.05, breath=0.02 * _wave(t, T / 2))


def idle(r, t, T):
    wr = {"l": Vector((0.17, -0.42, DESK + 0.04)), "r": Vector((-0.2, -0.44, DESK + 0.04))}
    hands = {"l": (Vector((-0.4, -1, -0.1)), DOWN), "r": (Vector((0.4, -1, -0.1)), DOWN)}
    curls = {(s, f): 0.45 for s in "lr" for f in FINGERS}
    yaw = 0.5 * _wave(t, T) + 0.15 * _wave(t, T / 3, 0.2)
    look = Vector((math.sin(yaw) * 1.2, -math.cos(yaw) * 1.2, 1.05 + 0.08 * _wave(t, T / 2, 0.1)))
    r.place(r.f, wrists=wr, hands=hands, lean=-0.04, tilt=-0.14, hip_back=0.0, look=look, curls=curls,
            elbows_out=0.18, breath=0.025 * _wave(t, T / 3))


MAKERS = {"Sit_KeyboardMouse": keyboard_mouse, "Sit_Gamepad": gamepad, "Sit_Browse": browse, "Sit_Idle": idle}


def bake_all(arm, names=None):
    """Creates one baked action per loop on `arm`; returns the actions."""
    sc = bpy.context.scene
    sc.render.fps = FPS
    out = []
    for name in names or list(MAKERS):
        T = LOOPS[name]
        n = int(T * FPS)
        r = Rig(arm)
        r.setup()
        arm.animation_data_create()
        arm.animation_data.action = bpy.data.actions.new(name + "_keys")
        for h in r.helpers:
            h.animation_data_clear()
        for f in range(n + 1):
            r.f = f + 1
            MAKERS[name](r, f / FPS, T)
        bpy.context.view_layer.objects.active = arm
        for o in bpy.context.selected_objects:
            o.select_set(False)
        arm.select_set(True)
        bpy.ops.object.mode_set(mode="POSE")
        bpy.ops.nla.bake(frame_start=1, frame_end=n + 1, only_selected=False, visual_keying=True,
                         clear_constraints=False, use_current_action=False, bake_types={"POSE"})
        baked = arm.animation_data.action
        baked.name = name
        baked.use_fake_user = True
        bpy.ops.object.mode_set(mode="OBJECT")
        keys = bpy.data.actions.get(name + "_keys")
        if keys:
            bpy.data.actions.remove(keys)
        r.teardown()
        for h in list(bpy.data.actions):
            if h.name.startswith(arm.name + "_") and h.users == 0:
                bpy.data.actions.remove(h)
        out.append(baked)
    arm.animation_data.action = None
    for pb in arm.pose.bones:
        pb.location = (0, 0, 0)
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.rotation_euler = (0, 0, 0)
    return out
