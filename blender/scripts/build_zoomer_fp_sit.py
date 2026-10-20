"""First-person "sit at the computer" for the realistic hero (SK_Zoomer, shared human skeleton).

Reuses the IK rig of blender/humans/seat_anims.py: the hero stands on the right of the chair, sidesteps,
sits, reaches out and puts his hands on keyboard and mouse, types, and looks up at the screen.
Output (local files, not committed: LFS quota):
  blender/humans_local/zoomer_fp_anim.glb   armature + body mesh + the baked action "FP_SitAtComputer"
  blender/humans_local/zoomer_eye_track.json  eye position and view direction per frame (for the camera)
The head bone is scaled to ~0 inside the action, so the hero's head and hair vanish in first person.

Needs blender/humans_local/zoomer.blend (copy of the humans-thread build) and seat_anims.py (HUMANS_DIR).

Run from the repo root:
    /Applications/Blender.app/Contents/MacOS/Blender -b -P blender/scripts/build_zoomer_fp_sit.py
"""
import json
import math
import os
import sys

import bpy
from mathutils import Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
LOCAL = os.path.join(ROOT, "blender", "humans_local")
HUMANS_DIR = os.environ.get("HUMANS_DIR") or next(
    p for p in (os.path.join(ROOT, "blender", "humans"),
                os.path.expanduser("~/projects/lyudmilovsk-wt/humans/blender/humans")) if os.path.exists(os.path.join(p, "seat_anims.py")))
sys.path.insert(0, HUMANS_DIR)
import seat_anims as sa  # noqa: E402

FPS = sa.FPS            # 30
N = 255                 # 8.5 s
ACTION = "FP_SitAtComputer"
sa.KEYS = Vector((0.0, -0.60, 0.76 + 0.015))        # our keyboard is 60 cm in front of the chair centre
DOWN = Vector((0, 0, -1))


def lerp(a, b, t):
    return a + (b - a) * t


def smooth(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3 - 2 * t)


def blend(a, b, t):
    if isinstance(a, dict):
        return {k: blend(a[k], b[k], t) for k in a}
    if isinstance(a, Vector):
        return a.lerp(b, t)
    if isinstance(a, tuple):
        return tuple(blend(x, y, t) for x, y in zip(a, b))
    if isinstance(a, (int, float)):
        return lerp(a, b, t)
    return a


class FPRig(sa.Rig):
    """seat_anims.Rig whose place() also takes a floor offset (ox, oy), a hip height and foot lifts."""

    def place(self, frame, *, hip_back=0.03, tilt=0.0, feet_fwd=0.42, feet_out=0.13, wrists=None, hands=None,
              elbows_out=0.22, look=sa.SCREEN, lean=0.1, twist=0.0, breath=0.0, curls=None,
              ox=0.0, oy=0.0, hip_z=None, lift_l=0.0, lift_r=0.0):
        self.frame = frame
        k = self.k
        thigh = sa._w(self.arm, "thigh_l")
        pel = sa._w(self.arm, "pelvis")
        hz = self.hip_z if hip_z is None else hip_z
        self.t_pelvis.location = Vector((ox, oy + hip_back + (pel.y - thigh.y), hz + (pel.z - thigh.z)))
        pb = self.pb["pelvis"]
        pb.rotation_mode = "XYZ"
        pb.rotation_euler = (tilt, 0, 0)
        pb.keyframe_insert("rotation_euler", frame=frame)
        for s, sx, lift in (("l", 1, lift_l), ("r", -1, lift_r)):
            self.t_foot[s].location = (ox + sx * feet_out * k, oy - feet_fwd * k, (0.075 + lift) * k)
            self.t_knee[s].location = (ox + sx * feet_out * k * 1.1, oy - 1.5, hz)
            self.t_wrist[s].location = wrists[s]
            self.t_elbow[s].location = (ox + sx * (0.2 + elbows_out), oy + 0.45, hz + 0.05)
            f, p = hands[s]
            self.t_hand[s].rotation_euler = self.hand_rot(s, f, p)
        self.t_look.location = look
        for e in self.helpers:
            e.keyframe_insert("location", frame=frame)
            e.keyframe_insert("rotation_euler", frame=frame)
        self.spine(lean, twist, breath)
        for s in "lr":
            for fi, fin in enumerate(sa.FINGERS):
                self.finger_curl(s, fin, (curls or {}).get((s, fin), 0.35))


def keyposes(r):
    """(frame, place() kwargs). Frames are 1-based."""
    k = r.k
    stand_hip = sa._w(r.arm, "thigh_l").z * 0.99
    ox0 = -0.50 * k
    hang = lambda ox: {"l": Vector((ox + 0.27 * k, 0.0, 0.86 * k)), "r": Vector((ox - 0.27 * k, 0.0, 0.86 * k))}
    hang_hands = {"l": (DOWN, Vector((-1, 0, 0))), "r": (DOWN, Vector((1, 0, 0)))}
    stand = dict(ox=ox0, oy=0.0, hip_z=stand_hip, hip_back=0.0, feet_fwd=0.0, feet_out=0.11, wrists=hang(ox0),
                 hands=hang_hands, look=Vector((ox0, -1.2, 1.5 * k)), lean=0.0, tilt=0.0, elbows_out=0.1)
    over = dict(stand, ox=0.0, oy=0.05, wrists=hang(0.0), look=Vector((0, -1.2, 1.35 * k)), lean=0.03)
    over_hands = over["hands"]
    sit_low = dict(over, hip_z=r.hip_z + 0.12 * k, hip_back=0.04, feet_fwd=0.38, feet_out=0.13, lean=0.14, tilt=0.03,
                   wrists=hang(0.0), look=Vector((0, -0.6, 0.9)), elbows_out=0.2)
    seated = dict(sit_low, hip_z=r.hip_z, feet_fwd=0.42, lean=0.16, look=Vector((0, -0.6, 0.85)))
    reach_w = {"l": sa.KEYS + Vector((0.15, 0.22, 0.14)), "r": sa.MOUSE + Vector((0.0, 0.22, 0.14))}
    reach_h = {"l": (sa.FWD + Vector((-0.3, 0, -0.35)), DOWN), "r": (sa.FWD + Vector((0.1, 0, -0.35)), DOWN)}
    reach = dict(seated, wrists=reach_w, hands=reach_h, lean=0.2, look=sa.KEYS + Vector((0, 0, 0.0)))
    curls = {(s, f): 0.2 for s in "lr" for f in sa.FINGERS}
    reach["curls"] = curls
    for d in (stand, over, sit_low, seated):
        d["curls"] = {(s, f): 0.25 for s in "lr" for f in sa.FINGERS}
    return [(1, stand), (14, stand), (44, over), (74, sit_low), (96, seated), (118, reach)], over_hands


def preview(arm, track):
    sc = bpy.context.scene
    out = os.path.join(LOCAL, "preview")
    os.makedirs(out, exist_ok=True)

    def box(name, size, loc, col):
        bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
        o = bpy.context.object
        o.scale = size
        m = bpy.data.materials.new(name)
        m.diffuse_color = col
        o.data.materials.append(m)
    box("seat", (0.48, 0.46, 0.06), (0, 0.0, 0.44), (.2, .2, .22, 1))
    box("back", (0.46, 0.07, 0.5), (0, 0.26, 0.75), (.2, .2, .22, 1))
    box("desk", (1.6, 0.8, 0.05), (0, -1.0, 0.735), (.5, .3, .15, 1))
    box("kb", (0.44, 0.15, 0.025), (0, -0.60, 0.775), (.8, .75, .6, 1))
    box("mon", (0.46, 0.05, 0.38), (0, -1.0, 1.0), (.02, .02, .02, 1))
    box("floor", (6, 6, 0.02), (0, 0, -0.01), (.4, .4, .4, 1))
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.display.shading.light = "STUDIO"
    sc.display.shading.color_type = "TEXTURE"
    sc.render.resolution_x, sc.render.resolution_y = 960, 540
    cam = bpy.data.objects.new("c", bpy.data.cameras.new("c"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    for f in (1, 30, 50, 80, 120, 160, 250):
        sc.frame_set(f)
        cam.data.lens = 40
        cam.location = Vector((3.0, -0.4, 1.0))
        cam.rotation_euler = (Vector((0, -0.3, 0.9)) - cam.location).to_track_quat('-Z', 'Y').to_euler()
        sc.render.filepath = os.path.join(out, "side_%d.png" % f)
        bpy.ops.render.render(write_still=True)
    for f in (30, 60, 90, 120, 160, 220):
        sc.frame_set(f)
        e = track[f - 1]
        cam.data.lens = 20
        cam.location = Vector(e["pos"])
        cam.rotation_euler = Vector(e["fwd"]).to_track_quat('-Z', 'Y').to_euler()
        sc.render.filepath = os.path.join(out, "eye_%d.png" % f)
        bpy.ops.render.render(write_still=True)


def main():
    bpy.ops.wm.open_mainfile(filepath=os.path.join(LOCAL, "zoomer.blend"))
    scene = bpy.context.scene
    scene.render.fps = FPS
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    for a in list(bpy.data.actions):
        a.use_fake_user = False
    r = FPRig(arm)
    r.setup()
    arm.animation_data_create()
    arm.animation_data.action = bpy.data.actions.new(ACTION + "_keys")
    for h in r.helpers:
        h.animation_data_clear()
    kp, _ = keyposes(r)
    typing_from = 118
    for f in range(1, N + 1):
        r.f = f
        if f < typing_from:
            for (f0, p0), (f1, p1) in zip(kp, kp[1:]):
                if f0 <= f <= f1:
                    kw = blend(p0, p1, smooth((f - f0) / (f1 - f0)))
                    break
            else:
                kw = kp[0][1]
            if 14 <= f <= 44:                               # the right foot steps over, then the left follows
                kw = dict(kw, lift_r=0.11 * math.sin((f - 14) / 30 * math.pi))
            if 40 <= f <= 74:
                kw = dict(kw, lift_l=0.08 * math.sin((f - 40) / 34 * math.pi))
            r.place(f, **kw)
        else:
            t = (f - typing_from) / FPS
            # same hand play as the club's keyboard/mouse loop, looking up at the screen after a few seconds
            sa.keyboard_mouse(r, t, 4.0)
            u = smooth((f - 170) / 45)          # watches the hands on the keys, then looks up at the screen
            r.t_look.location = Vector((sa.KEYS.x, sa.KEYS.y - 0.02, sa.KEYS.z)).lerp(sa.SCREEN, u)
            r.t_look.keyframe_insert("location", frame=f)
    bpy.context.view_layer.objects.active = arm
    for o in bpy.context.selected_objects:
        o.select_set(False)
    arm.select_set(True)
    bpy.ops.object.mode_set(mode="POSE")
    bpy.ops.nla.bake(frame_start=1, frame_end=N, only_selected=False, visual_keying=True,
                     clear_constraints=False, use_current_action=False, bake_types={"POSE"})
    baked = arm.animation_data.action
    baked.name = ACTION
    bpy.ops.object.mode_set(mode="OBJECT")
    keys = bpy.data.actions.get(ACTION + "_keys")
    if keys:
        bpy.data.actions.remove(keys)
    r.teardown()

    # eye track from the baked pose (before the head is scaled away)
    arm.animation_data.action = baked
    track = {}
    for f in range(1, N + 1):
        scene.frame_set(f)
        el, er = arm.pose.bones["eye_l"], arm.pose.bones["eye_r"]
        pos = (arm.matrix_world @ el.head + arm.matrix_world @ er.head) / 2
        fwd = ((arm.matrix_world.to_3x3() @ el.matrix.to_3x3().col[1]) +
               (arm.matrix_world.to_3x3() @ er.matrix.to_3x3().col[1])).normalized()
        track[f - 1] = {"pos": list(pos), "fwd": list(fwd)}
    with open(os.path.join(LOCAL, "zoomer_eye_track.json"), "w") as fh:
        json.dump({"fps": FPS, "frames": track}, fh)

    # hide the head: scale the head bone to ~0 on every frame
    head = arm.pose.bones["head"]
    head.rotation_mode = head.rotation_mode
    for f in range(1, N + 1):
        head.scale = (0.001, 0.001, 0.001)
        head.keyframe_insert("scale", frame=f)
    head.scale = (0.001, 0.001, 0.001)
    head.keyframe_insert("scale", frame=N)
    for fc in baked.fcurves if hasattr(baked, "fcurves") else []:
        pass
    scene.frame_start, scene.frame_end = 1, N
    for pb in arm.pose.bones:       # leave the pose neutral
        pass

    if "--preview" in sys.argv:
        preview(arm, track)

    # export: armature + the baked action only
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    for c in arm.children_recursive:        # Interchange only imports animations together with a skinned mesh
        c.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.gltf(filepath=os.path.join(LOCAL, "zoomer_fp_anim.glb"), use_selection=True, export_format="GLB",
                              export_skins=True, export_animations=True, export_animation_mode="ACTIVE_ACTIONS",
                              export_force_sampling=True, export_optimize_animation_size=False,
                              export_anim_single_armature=True, export_def_bones=False, export_yup=True,
                              export_apply=False, export_morph=False)
    print("done: FP anim", N, "frames at", FPS, "fps; eye frames", len(track))


main()
