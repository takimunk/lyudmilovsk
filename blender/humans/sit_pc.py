"""Seated at a computer: the club's loops for every person, posed on the clips' body (Guy_a) with poser.py, carried over
to A_Human_PC_* by retarget_clips.py (clips.json source "pc").

    Blender -b -P blender/humans/sit_pc.py -- [--preview]

The chair and desk are seat_anims.py's (Blender metres, the actor origin on the floor under the seat's centre, facing
-Y): seat top 0.47, desk top 0.76, the keyboard's middle 0.55 in front, the mouse 0.26 to the right of it, the screen
~0.95 in front at 1.12. The seated pelvis is the shared clips' (SitIdle/SitDown/StandUp: (0, -0.01, 0.567)), so sitting
down, the computer loops and standing up follow on without a jump.

Arms by two-bone IK with the elbows kept in by the body and a little back (not winged out), hands oriented by their
palms, part of the wrist's turn in the forearm. Each loop starts and ends on its own base pose; the game picks among
them (ULyudBodyAnimInstance, ANpcBody's Computer pose) with cross-fades, so a person types, then flicks the mouse, leans
back, stretches, scrolls and reads in an order of their own:
    PC_Type      typing in bursts, now and then a glance at the keys
    PC_Game      left hand on WASD, right on the mouse, quick flicks and clicks (a shooter)
    PC_Scroll    leaning back a little, scrolling with the index finger, slow; the left hand on the desk
    PC_Read      leaning in, the chin on the left hand, an occasional scroll
    PC_LeanBack  slumped back, hands in the lap, watching
    PC_Stretch   a stretch with both arms up, back to the keys (one-shot)
    PC_Gamepad   holding a gamepad in the lap, thumbs busy, jolts when something happens (the prop: SM_Gamepad on hand_r)
"""
import json
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import poser  # noqa: E402
from poser import X, Y, Z, FPS, ROOT, Poser  # noqa: E402

SEAT, DESK = 0.47, 0.76
PELVIS = Vector((0.0, -0.01, 0.567))
KEYS_Z = DESK + 0.035                     # the wrists over the keys
SCREEN = Vector((0.0, -0.95, 1.12))
TAU = 2 * math.pi


def log(m):
    print("SITPC " + m, flush=True)


def bump(t, at, width):
    """A smooth 0-1-0 pulse centred at `at`, `width` seconds wide (periodic over the loop handled by the caller)."""
    x = (t - at) / (width / 2)
    return max(0.0, 1.0 - x * x) ** 2


def snap(t, at, rise=0.08):
    """0 before `at`, a quick (sharp) rise to 1 over `rise` seconds."""
    return poser.ease((t - at) / rise)


def seated(p, lean=8.0, slump=0.0, legs="normal", look=(0.0, 0.0)):
    """The body on the chair: lean (deg, + forward toward the desk), slump (m, the pelvis slid forward), the feet on the
    floor, the head toward the screen plus look (yaw, pitch deg)."""
    p.reset()
    for s, sign in (("l", 1), ("r", -1)):
        p.turn("thigh_" + s, -88, X)
        p.turn("calf_" + s, 86, X)
    p.place("pelvis", PELVIS + Vector((0.0, -slump, -0.15 * slump)))
    if slump:
        p.turn("pelvis", -slump * 120.0, X)                  # slumped: the pelvis rolls back
    fwd = {"normal": 0.44, "tucked": 0.30, "out": 0.62}[legs]
    for s, sign in (("l", 1), ("r", -1)):
        ankle = Vector((sign * 0.12, -fwd, 0.085 if legs != "out" else 0.075))
        p.ik("thigh_" + s, "calf_" + s, "foot_" + s, ankle, (sign * 0.05, -1.0, 0.25))
        p.aim("foot_" + s, "ball_" + s, (sign * 0.08, -0.95, -0.42))
    p.turn("spine_01", lean * 0.40, X)
    p.turn("spine_02", lean * 0.35, X)
    p.turn("spine_03", lean * 0.25, X)
    # the head toward the screen: undo most of the lean, then the screen's pitch, then the look
    h = p.head("head")
    to = SCREEN - h
    pitch = math.degrees(math.atan2(-to.z, -to.y))           # + down
    p.turn("neck_01", -lean * 0.55 + pitch * 0.4, X)
    p.turn("head", -lean * 0.3 + pitch * 0.6 + look[1], X)
    p.turn("neck_01", look[0] * 0.4, Z)
    p.turn("head", look[0] * 0.6, Z)


ELBOW = {"l": (0.10, 0.45, -1.0), "r": (-0.10, 0.45, -1.0)}   # elbows down by the body, a little back, barely out


def arm(p, side, wrist, fingers_dir, palm=(0, 0, -1), pole=None, forearm_share=0.5):
    p.ik("upperarm_" + side, "lowerarm_" + side, "hand_" + side, wrist, pole or ELBOW[side])
    p.hand(side, fingers_dir, palm, forearm_share)


def keys_hands(p, t, typing=True, look_down=0.0):
    """Both hands on the keyboard; fingers tapping in bursts."""
    for s, sign in (("l", 1), ("r", -1)):
        w = Vector((sign * 0.10, -0.47, KEYS_Z)) + Vector((sign * 0.008 * math.sin(t * 1.3 + sign), 0.006 * math.sin(t * 0.9), 0))
        arm(p, s, w, (-sign * 0.25, -1.0, -0.35))
        taps = {}
        for i, f in enumerate(("index", "middle", "ring", "pinky")):
            tap = 0.0
            if typing:
                ph = (t * (7.0 + i * 1.3) + i * 1.7 + (0 if s == "l" else 0.9)) % 1.0
                tap = 14.0 * max(0.0, math.sin(ph * TAU)) ** 6
            taps[f] = 28.0 + tap
        p.curl(s, taps, thumb=10.0)


def burst(t, period, on):
    """1 while typing, 0 in the pauses: `on` of every `period` seconds, edges softened."""
    ph = (t % period) / period
    return poser.ease(ph / 0.05) * poser.ease((on - ph) / 0.05)


# ---------------------------------------------------------------------------------------------- the loops

def pc_type(p, t, T):
    glance = bump(t % 8.0, 5.2, 1.4)
    seated(p, lean=9.0 + 2.0 * math.sin(t * TAU / T), look=(2.0 * math.sin(t * TAU * 2 / T), 14.0 * glance))
    keys_hands(p, t, typing=burst(t, 4.0, 0.7) > 0.5)


def pc_game(p, t, T):
    # flicks: the mouse jumps to a new spot quickly and holds; the head follows a little
    spots = [(0.0, 0.0), (0.035, 0.01), (-0.02, -0.012), (0.05, 0.004), (-0.03, 0.015), (0.0, 0.0)]
    n = len(spots) - 1
    seg = T / n
    k = min(int(t // seg), n - 1)
    a, b = spots[k], spots[k + 1]
    s = snap(t - k * seg, seg * 0.55, 0.09)
    off = (a[0] + (b[0] - a[0]) * s, a[1] + (b[1] - a[1]) * s)
    seated(p, lean=13.0, look=(-off[0] * 120.0, 0.0))
    # left hand on WASD, fingers pressing
    w = Vector((0.16, -0.45, KEYS_Z))
    arm(p, "l", w, (-0.35, -1.0, -0.35))
    press = {f: 26.0 + 16.0 * max(0.0, math.sin((t * r + ph) * TAU)) ** 8
             for f, r, ph in (("index", 2.1, 0.1), ("middle", 3.3, 0.4), ("ring", 1.7, 0.7), ("pinky", 0.9, 0.2))}
    p.curl("l", press, thumb=20.0)
    # right hand on the mouse
    m = Vector((-0.27 + off[0], -0.43 + off[1], DESK + 0.045))
    arm(p, "r", m, (0.10, -1.0, -0.30), forearm_share=0.6)
    click = 12.0 * max(0.0, math.sin((t * 1.9) * TAU)) ** 16
    p.curl("r", {"index": 18.0 + click, "middle": 22.0, "ring": 38.0, "pinky": 45.0}, thumb=15.0)


def pc_scroll(p, t, T):
    seated(p, lean=3.0, look=(0.0, 3.0 + 2.0 * math.sin(t * TAU / T)))
    m = Vector((-0.27, -0.42, DESK + 0.045))
    arm(p, "r", m, (0.08, -1.0, -0.30), forearm_share=0.6)
    scroll = 18.0 * (0.5 - 0.5 * math.cos(t * TAU * 1.4)) * burst(t, 3.0, 0.45)
    p.curl("r", {"index": 15.0 + scroll, "middle": 24.0, "ring": 40.0, "pinky": 48.0}, thumb=15.0)
    # the left hand resting on the desk to the side, relaxed
    arm(p, "l", Vector((0.22, -0.36, DESK + 0.03)), (-0.55, -1.0, -0.2))
    p.curl("l", 22.0, thumb=8.0)


def pc_read(p, t, T):
    seated(p, lean=16.0, look=(4.0 * math.sin(t * TAU / T), 6.0))
    # the chin on the left hand, the elbow on the desk
    chin = p.head("head") + Vector((0.0, -0.085, -0.105))
    arm(p, "l", chin + Vector((0.035, 0.0, -0.03)), (-0.35, -0.2, 1.0), palm=(-0.3, 1.0, 0.0), pole=(0.25, -0.35, -1.0))
    p.curl("l", 35.0, thumb=15.0)
    m = Vector((-0.27, -0.42, DESK + 0.045))
    arm(p, "r", m, (0.08, -1.0, -0.30), forearm_share=0.6)
    scroll = 18.0 * bump(t % T, T * 0.6, 1.2)
    p.curl("r", {"index": 15.0 + scroll, "middle": 24.0, "ring": 40.0, "pinky": 48.0}, thumb=15.0)


def pc_lean_back(p, t, T):
    seated(p, lean=-12.0, slump=0.05, legs="out", look=(3.0 * math.sin(t * TAU / T), -2.0))
    for s, sign in (("l", 1), ("r", -1)):
        lap = Vector((sign * 0.10, -0.24 - 0.02 * math.sin(t * TAU / T + sign), SEAT + 0.17))
        arm(p, s, lap, (-sign * 0.6, -0.8, -0.3), palm=(0, 0, -1), pole=(sign * 0.35, 0.3, -1.0))
        p.curl(s, 30.0, thumb=10.0)


def pc_stretch(p, t, T):
    up = bump(t, T * 0.45, T * 0.7)
    seated(p, lean=9.0 - 20.0 * up, look=(0.0, -15.0 * up))
    keys = {}
    for s, sign in (("l", 1), ("r", -1)):
        kw = Vector((sign * 0.10, -0.47, KEYS_Z))
        hw = Vector((sign * 0.12, 0.02, 1.78))
        w = kw.lerp(hw, poser.ease(up * 1.15))
        arm(p, s, w, Vector((-sign * 0.25, -1.0, -0.35)).lerp(Vector((-sign * 0.6, 0.0, 1.0)), up),
            palm=Vector((0, 0, -1)).lerp(Vector((0, -1, 0)), up), pole=Vector(ELBOW[s]).lerp(Vector((sign, 0.2, 0.0)), up))
        p.curl(s, 28.0 * (1 - up) + 5.0 * up, thumb=10.0)


def pc_gamepad(p, t, T):
    jolt = bump(t % 9.0, 6.0, 0.5)
    seated(p, lean=12.0 + 6.0 * jolt, look=(0.0, -4.0 * jolt))
    for s, sign in (("l", 1), ("r", -1)):
        w = Vector((sign * 0.075, -0.30 - 0.04 * jolt, 0.72 + 0.05 * jolt))
        arm(p, s, w, (-sign * 0.55, -0.80, 0.20), palm=(-sign * 0.75, 0.0, -0.65), pole=(sign * 0.15, 0.4, -1.0))
        p.curl(s, {"index": 40.0, "middle": 70.0, "ring": 75.0, "pinky": 78.0}, thumb=None)
    # the thumbs on the sticks and buttons
    for s, sign in (("l", 1), ("r", -1)):
        for i, a in ((2, 18.0 + 10.0 * math.sin(t * (5.0 if s == "l" else 7.3))), (3, 22.0)):
            nm = "thumb_0%d_%s" % (i, s)
            if nm in p.pb:
                from mathutils import Quaternion
                p.pb[nm].rotation_mode = "QUATERNION"
                p.pb[nm].rotation_quaternion = Quaternion((1, 0, 0), math.radians(a))
    bpy.context.view_layer.update()


LOOPS = {"PC_Type": (pc_type, 8.0), "PC_Game": (pc_game, 6.0), "PC_Scroll": (pc_scroll, 9.0), "PC_Read": (pc_read, 10.0),
         "PC_LeanBack": (pc_lean_back, 10.0), "PC_Stretch": (pc_stretch, 4.0), "PC_Gamepad": (pc_gamepad, 9.0)}


def gamepad_grip(p):
    """Where the gamepad sits in PC_Gamepad's base pose, against hand_r (for the game's prop: SM_Gamepad attached to
    hand_r with this offset, UE cm and degrees)."""
    pc_gamepad(p, 0.0, 9.0)
    hr, hl = p.head("hand_r"), p.head("hand_l")
    mid = (hr + hl) / 2 + Vector((0.0, -0.035, 0.015))
    m = p.arm.matrix_world @ p.pb["hand_r"].matrix
    local = m.inverted() @ mid
    return {"bone": "hand_r", "blender_local_m": [round(v, 4) for v in local], "between_hands_m": round((hr - hl).length, 4)}


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    only = argv[argv.index("--only") + 1].split(",") if "--only" in argv else None
    arm = poser.load()
    p = Poser(arm)
    acts = []
    for name, (fn, T) in LOOPS.items():
        if only and name not in only:
            continue
        frames = []
        for i in range(int(round(T * FPS)) + 1):
            fn(p, i / FPS, T)
            frames.append(p.snapshot())
        acts.append(poser.bake(arm, p, name, frames, prefix="Pc_"))
        p.reset()
        fn(p, 0.0, T)
        log("%s: hands %s %s" % (name, tuple(round(v, 3) for v in p.head("hand_l")), tuple(round(v, 3) for v in p.head("hand_r"))))
    grip = gamepad_grip(p)
    json.dump(grip, open(os.path.join(ROOT, "Saved", "ClipSources", "gamepad_grip.json"), "w"), indent=1)
    log("gamepad grip %s" % grip)
    p.reset()
    if "--preview" in argv:
        preview(arm, acts)
    poser.export(arm, acts, os.path.join(ROOT, "Saved", "ClipSources", "sit_pc.glb"))


def preview(arm, acts):
    def box(name, x0, x1, y0, y1, z0, z1):
        bpy.ops.mesh.primitive_cube_add(location=((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2))
        o = bpy.context.active_object
        o.scale = ((x1 - x0) / 2, (y1 - y0) / 2, (z1 - z0) / 2)
        o.name = name
    box("seat", -0.22, 0.22, -0.22, 0.22, SEAT - 0.04, SEAT)
    box("back", -0.22, 0.22, 0.22, 0.25, SEAT, SEAT + 0.45)
    box("desk", -0.7, 0.7, -1.05, -0.35, DESK - 0.03, DESK)
    box("keys", -0.22, 0.22, -0.62, -0.47, DESK, DESK + 0.02)
    box("mouse", -0.30, -0.24, -0.47, -0.37, DESK, DESK + 0.03)
    box("screen", -0.3, 0.3, -0.98, -0.95, 0.95, 1.30)
    sc = bpy.context.scene
    clay = bpy.data.materials.new("clay")                     # the pose reads better in plain clay
    clay.diffuse_color = (0.75, 0.62, 0.55, 1)
    for o in bpy.data.objects:
        if o.type == "MESH" and o.parent == arm:
            o.data.materials.clear()
            o.data.materials.append(clay)
    sc.render.engine = "BLENDER_EEVEE"
    sc.render.resolution_x, sc.render.resolution_y = 360, 360
    w = bpy.data.worlds.new("w")
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs[0].default_value = (0.6, 0.6, 0.62, 1)
    sc.world = w
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 3
    sun.rotation_euler = (math.radians(35), 0, math.radians(160))     # from the front-left, onto the face and hands
    sc.collection.objects.link(sun)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    out = os.path.join(ROOT, "Saved", "SitPc")
    os.makedirs(out, exist_ok=True)
    views = {"side": ((1.35, -0.35, 1.0), (0.0, -0.28, 0.85)), "back": ((0.55, 0.9, 1.55), (0.0, -0.3, 0.85)),
             "front": ((-0.75, -1.05, 1.25), (0.0, -0.25, 0.9))}
    for act in acts:
        arm.animation_data.action = act
        n = int(act.frame_range[1])
        for j in range(4):
            sc.frame_set(1 + int((n - 1) * j / 4))
            for vn, (eye, tgt) in views.items():
                cam.location = eye
                cam.rotation_euler = (Vector(tgt) - Vector(eye)).to_track_quat("-Z", "Y").to_euler()
                sc.render.filepath = os.path.join(out, "%s_%s_%d.png" % (act.name, vn, j))
                bpy.ops.render.render(write_still=True)
    arm.animation_data.action = None


main()
