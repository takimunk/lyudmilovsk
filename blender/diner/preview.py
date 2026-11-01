"""Renders the diner's meshes (design/diner/game/SM_Diner_*.glb) from a few cameras to design/diner/previews/ (Eevee, a sun and
the sky): Blender -b -P blender/diner/preview.py [-- view ...]"""
import math
import os
import sys

import bpy
from mathutils import Vector

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
GAME = os.path.join(ROOT, "design", "diner", "game")
OUT = os.path.join(ROOT, "design", "diner", "previews")
# name: (eye, target, lens mm); diner frame (Blender: x, y, z)
VIEWS = {
    "front": ((-9.0, -14.0, 2.2), (0.5, 0.0, 1.6), 24),
    "nose": ((16.0, -6.0, 1.9), (6.0, 0.0, 1.6), 26),
    "back": ((-12.0, 13.0, 3.5), (-2.0, 2.0, 1.5), 24),
    "door_in": ((0.0, -2.2, 1.65), (2.0, 1.5, 1.2), 16),
    "counter": ((-6.6, -1.0, 1.65), (4.0, 0.8, 1.0), 18),
    "booths": ((6.8, -0.6, 1.6), (-3.0, -2.0, 0.9), 18),
    "coffee": ((-5.6, -1.0, 1.65), (-6.9, -1.0, 1.05), 22),
    "kitchen": ((-2.6, 5.6, 1.7), (-6.4, 3.0, 1.0), 16),
    "nose_in": ((3.0, 0.5, 1.6), (9.5, 0.0, 1.0), 18),
}


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for f in sorted(os.listdir(GAME)):
        if f.startswith("SM_Diner_") and f.endswith(".glb") and ("Shell" in f or "Inside" in f or f == "SM_Diner_Glass.glb"):
            bpy.ops.import_scene.gltf(filepath=os.path.join(GAME, f))
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import diner_common as C
    sign = (0.95, 0.85, 0.3)
    for m in bpy.data.materials:
        key = m.name.split(".")[0]
        b = m.node_tree.nodes.get("Principled BSDF") if m.node_tree else None
        if not b:
            continue
        if key in C.MAT_SPEC:
            hx, rough, metal, _ = C.MAT_SPEC[key]
            b.inputs["Base Color"].default_value = C._lin(hx) + (1,)
            b.inputs["Roughness"].default_value = rough
            b.inputs["Metallic"].default_value = metal
        elif key.startswith("DN_Sign_"):
            b.inputs["Base Color"].default_value = sign + (1,)
    for o in bpy.context.scene.objects:
        if o.type == "MESH" and "Glass" in o.name:
            for m in o.data.materials:
                if m and m.node_tree:
                    b = m.node_tree.nodes.get("Principled BSDF")
                    if b:
                        b.inputs["Alpha"].default_value = 0.15
                        b.inputs["Roughness"].default_value = 0.05
                    m.surface_render_method = "BLENDED" if hasattr(m, "surface_render_method") else None
        if o.type == "MESH":
            for m in o.data.materials:
                if m and m.node_tree and m.name.startswith("DN_Opal"):
                    b = m.node_tree.nodes.get("Principled BSDF")
                    b.inputs["Emission Color"].default_value = (1, 0.9, 0.75, 1)
                    b.inputs["Emission Strength"].default_value = 4.0
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE" if "BLENDER_EEVEE" in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE_NEXT"
    sc.render.resolution_x, sc.render.resolution_y = 1280, 800
    w = bpy.data.worlds.new("w")
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs[0].default_value = (0.55, 0.65, 0.8, 1)
    w.node_tree.nodes["Background"].inputs[1].default_value = 0.8
    sc.world = w
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 3.0
    sun.rotation_euler = (math.radians(50), 0, math.radians(-35))
    sc.collection.objects.link(sun)
    for i, x in enumerate((-6.0, -3.6, -1.2, 1.2, 3.6, 6.0, 8.6, -5.8, -3.5)):
        y = 0.0 if i < 7 else (3.9 if i == 7 else 4.9)
        L = bpy.data.objects.new("l%d" % i, bpy.data.lights.new("l%d" % i, "POINT"))
        L.data.energy = 120
        L.location = (x, y, 2.4 if i < 7 else 2.0)
        sc.collection.objects.link(L)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    os.makedirs(OUT, exist_ok=True)
    for name, (eye, tgt, lens) in VIEWS.items():
        if args and name not in args:
            continue
        cam.location = eye
        cam.rotation_euler = (Vector(tgt) - Vector(eye)).to_track_quat("-Z", "Y").to_euler()
        cam.data.lens = lens
        cam.data.clip_start = 0.05
        sc.render.filepath = os.path.join(OUT, "building_%s.png" % name)
        bpy.ops.render.render(write_still=True)
        print("PREVIEW", sc.render.filepath, flush=True)


main()
