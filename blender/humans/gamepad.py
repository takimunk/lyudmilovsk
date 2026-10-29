"""A gamepad for the club's console players (the prop ULyudBodyAnimInstance holds between the hands in PC_Gamepad).

    Blender -b -P blender/humans/gamepad.py   -> Saved/ClipSources/gamepad.glb (unreal/scripts/import_gamepad.py)

Built from primitives: a bevelled body 15 x 6 cm, two grips angled down and back, two thumbsticks, a d-pad, four face
buttons, shoulder buttons; matte dark plastic with lighter sticks and buttons. Origin in the middle of the body; its
length along X, the top (sticks) +Z, the far edge (shoulder buttons) +Y. ~1.5k triangles.
"""
import math
import os

import bpy
import bmesh

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))


def mat(name, rgb, rough):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = rgb + (1,)
    b.inputs["Roughness"].default_value = rough
    return m


def add(prim, mat_, **kw):
    getattr(bpy.ops.mesh, "primitive_%s_add" % prim)(**kw)
    o = bpy.context.active_object
    o.data.materials.append(mat_)
    return o


bpy.ops.wm.read_factory_settings(use_empty=True)
body_m = mat("gamepad.body", (0.035, 0.037, 0.04), 0.55)
part_m = mat("gamepad.parts", (0.12, 0.12, 0.13), 0.45)
parts = []
b = add("cube", body_m, size=1.0)
b.scale = (0.15, 0.06, 0.028)
bpy.ops.object.transform_apply(scale=True)
bev = b.modifiers.new("bevel", "BEVEL")
bev.width, bev.segments = 0.012, 3
parts.append(b)
for sign in (1, -1):
    g = add("cylinder", body_m, radius=0.021, depth=0.07, vertices=16, location=(sign * 0.058, -0.03, -0.006))
    g.rotation_euler = (math.radians(70), math.radians(sign * 18), 0)
    parts.append(g)
    st = add("cylinder", part_m, radius=0.009, depth=0.012, vertices=14, location=(sign * 0.032, -0.012, 0.018))
    parts.append(st)
    sh = add("cube", part_m, size=1.0, location=(sign * 0.05, 0.03, 0.006))
    sh.scale = (0.035, 0.008, 0.012)
    parts.append(sh)
for dx, dy in ((0.0, 0.009), (0.0, -0.009), (0.009, 0.0), (-0.009, 0.0)):
    parts.append(add("cylinder", part_m, radius=0.0042, depth=0.006, vertices=10, location=(-0.052 + dx, 0.008 + dy, 0.015)))
    d = add("cube", part_m, size=1.0, location=(0.052 + dx * 0.6, 0.008 + dy * 0.6, 0.015))
    d.scale = (0.006, 0.006, 0.005)
    parts.append(d)
bpy.ops.object.select_all(action="DESELECT")
for o in parts:
    o.select_set(True)
bpy.context.view_layer.objects.active = b
for o in parts:
    for m in o.modifiers:
        bpy.context.view_layer.objects.active = o
        bpy.ops.object.modifier_apply(modifier=m.name)
bpy.context.view_layer.objects.active = b
bpy.ops.object.join()
for p in b.data.polygons:
    p.use_smooth = True
b.name = "SM_Gamepad"
out = os.path.join(ROOT, "Saved", "ClipSources", "gamepad.glb")
bpy.ops.export_scene.gltf(filepath=out, use_selection=True, export_format="GLB", export_yup=True)
print("GAMEPAD %d triangles -> %s" % (sum(len(p.vertices) - 2 for p in b.data.polygons), out))
