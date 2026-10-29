"""Renders the hero pigeon (export/FeralPigeon/FeralPigeon_Hero.glb) in the species' clips, to check folding without
Unreal: takes the actions from FeralPigeon.glb (same bone names) and plays one frame on the hero's rig.

    Blender -b --python blender/birds/hero_pigeon/pose_preview.py -- <out prefix> [Clip:frame ...] [--debug] [--fpv sweep droop]

--debug colours the feather cards by kind (red primaries, blue secondaries/tertials, green tail, yellow coverts).
"""
import math
import os
import sys

import bpy
import mathutils

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "export", "FeralPigeon")
argv = sys.argv[sys.argv.index("--") + 1:]
prefix = argv[0]
debug = "--debug" in argv
fpv = None
if "--fpv" in argv:
    i = argv.index("--fpv")
    fpv = (float(argv[i + 1]), float(argv[i + 2]))
clips = [a for a in argv[1:] if ":" in a] or ["Idle:0"]

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=os.path.join(OUT, "FeralPigeon.glb"))
actions = {a.name: a for a in bpy.data.actions}
for o in list(bpy.data.objects):
    bpy.data.objects.remove(o)
bpy.ops.import_scene.gltf(filepath=os.path.join(OUT, "FeralPigeon_Hero.glb"))
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
ob = next(o for o in bpy.data.objects if o.type == "MESH" and o.name.startswith("SK_"))
for o in list(bpy.data.objects):
    if o.type == "MESH" and o is not ob:
        bpy.data.objects.remove(o)
sc = bpy.context.scene
sc.render.engine = "BLENDER_EEVEE"
sc.render.resolution_x, sc.render.resolution_y = 1000, 700
w = bpy.data.worlds.new("w")
sc.world = w
w.color = (0.45, 0.5, 0.55)
sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
sc.collection.objects.link(sun)
sun.data.energy = 4
sun.rotation_euler = (0.7, 0.2, 0.6)
for m in ob.data.materials:
    m.surface_render_method = "DITHERED"
    m.use_backface_culling = "Feathers" not in m.name
if debug:
    groups = {g.index: g.name for g in ob.vertex_groups}
    col = ob.data.color_attributes.new("dbg", "BYTE_COLOR", "POINT")
    for v in ob.data.vertices:
        n = groups[max(v.groups, key=lambda g: g.weight).group] if len(v.groups) else ""
        b = n.split(".")[0]
        c = (0.6, 0.6, 0.6)
        if b[:1] == "p" and b[1:].isdigit():
            c = (1, 0.15, 0.1)
        elif b[:1] == "s" and b[1:].isdigit():
            c = (0.15, 0.3, 1) if int(b[1:]) <= 12 else (0.9, 0.2, 0.9)
        elif n.startswith("tf."):
            c = (0.1, 0.9, 0.2)
        col.data[v.index].color = (*c, 1)
    for m in ob.data.materials:
        nt = m.node_tree
        p = nt.nodes["Principled BSDF"]
        a = nt.nodes.new("ShaderNodeVertexColor")
        a.layer_name = "dbg"
        mix = nt.nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.blend_type = "MULTIPLY"
        mix.inputs["Factor"].default_value = 1.0
        src = p.inputs["Base Color"].links[0].from_socket if p.inputs["Base Color"].links else None
        if src:
            nt.links.new(src, mix.inputs[6])
        nt.links.new(a.outputs["Color"], mix.inputs[7])
        nt.links.new(mix.outputs[2], p.inputs["Base Color"])
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
sc.collection.objects.link(cam)
sc.camera = cam
cam.data.clip_start = 0.003
arm.animation_data_create()
for spec in clips:
    clip, frame = spec.split(":")
    act = next((a for n, a in actions.items() if n.startswith(clip)), None)
    arm.animation_data.action = act
    if act and getattr(act, "slots", None):
        arm.animation_data.action_slot = act.slots[0]
    sc.frame_set(int(frame))
    if fpv:
        bpy.context.view_layer.update()
        for side, sy in (("L", 1), ("R", -1)):
            pb = arm.pose.bones[f"humerus.{side}"]
            head = arm.matrix_world @ pb.head
            R = mathutils.Matrix.Rotation(-sy * math.radians(fpv[0]), 4, "Z") @ mathutils.Matrix.Rotation(sy * fpv[1], 4, "X")
            W = mathutils.Matrix.Translation(head) @ R @ mathutils.Matrix.Translation(-head) @ (arm.matrix_world @ pb.matrix)
            pb.matrix = arm.matrix_world.inverted() @ W
            bpy.context.view_layer.update()
    shots = {"side": ((0.0, -0.45, 0.12), (0.0, 0.0, 0.07), 50), "34": ((0.28, -0.3, 0.25), (0.0, 0.0, 0.07), 50),
             "back": ((-0.3, -0.12, 0.3), (0.0, 0.0, 0.07), 50), "top": ((0.0, 0.0, 0.6), (0.0, 0.0, 0.07), 45)}
    if fpv:
        shots = {"eye": ((0.104, 0.0, 0.096), (0.4, 0.0, 0.05), 9), "above": ((-0.2, 0.0, 0.35), (0.06, 0.0, 0.08), 30)}
    for name, (loc, tgt, lens) in shots.items():
        cam.location = loc
        cam.rotation_euler = (mathutils.Vector(tgt) - mathutils.Vector(loc)).to_track_quat("-Z", "Y").to_euler()
        cam.data.lens = lens
        sc.render.filepath = f"{prefix}_{clip}{frame}_{name}.png"
        bpy.ops.render.render(write_still=True)
print("POSE", clips, list(actions)[:20])
