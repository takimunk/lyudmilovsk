"""Renders the restaurant's meshes (design/restaurant/game/SM_Rest_*.glb) from a few cameras to design/restaurant/previews/
(Eevee, a sun and the sky, a sand ground plane at the ground level, point lights from layout.json):
  Blender -b -P blender/restaurant/preview.py [-- view ...]
The view "dusk" renders with the lamps and the sign lit."""
import json
import math
import os
import sys

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import restaurant_common as C  # noqa: E402

GAME = os.path.join(C.ROOT, "design", "restaurant", "game")
OUT = os.path.join(C.ROOT, "design", "restaurant", "previews")
LAYOUT = os.path.join(C.ROOT, "unreal", "Lyudmilovsk", "Content", "Restaurant", "layout.json")
# name: (eye, target, lens mm, dusk)
VIEWS = {
    "promenade": ((-17.0, -31.0, 2.4), (0.0, -4.0, 3.0), 26, False),
    "sea": ((34.0, 9.0, 2.2), (0.0, 0.0, 3.0), 30, False),
    "aerial": ((24.0, -27.0, 24.0), (-1.5, 0.0, 0.0), 24, False),
    "back": ((-28.0, 14.0, 4.0), (-10.0, 0.0, 2.0), 24, False),
    "door_in": ((0.0, -9.3, 1.65), (0.0, 3.0, 1.4), 15, False),
    "pass": ((1.0, -1.6, 1.65), (-8.3, 0.0, 1.3), 18, False),
    "table": ((6.0, 0.9, 1.75), (7.2, 0.0, 0.6), 20, False),
    "kitchen": ((-9.0, -4.2, 1.7), (-13.5, 2.0, 1.0), 14, False),
    "overhang": ((6.0, -12.6, 1.6), (-2.0, -9.0, 4.0), 16, False),
    "dusk": ((-17.0, -31.0, 2.4), (0.0, -4.0, 3.0), 26, True),
    "door_out": ((1.5, -15.2, 1.7), (0.0, -10.0, 1.8), 22, False),
    "junction": ((-5.2, 10.6, 1.6), (-8.6, 5.4, 3.0), 18, False),
    "sign": ((3.0, -21.0, 6.5), (0.0, -13.8, 5.3), 40, False),
    "column": ((9.0, -14.0, 1.6), (3.2, -12.0, 3.6), 24, False),
    "oculus": ((3.0, -2.0, 1.6), (0.0, 0.0, 4.5), 16, False),
}


def set_mats(dusk):
    for m in bpy.data.materials:
        key = m.name.split(".")[0]
        b = m.node_tree.nodes.get("Principled BSDF") if m.node_tree else None
        if not b or key not in C.MAT_SPEC:
            continue
        hx, rough, metal, emis = C.MAT_SPEC[key]
        b.inputs["Base Color"].default_value = C._lin(hx) + (1,)
        b.inputs["Roughness"].default_value = rough
        b.inputs["Metallic"].default_value = metal
        glow = 0.0
        if key == "RS_Opal":
            glow = 4.0 if dusk else 1.5
        elif key == "RS_HeatLamp":
            glow = 3.0
        elif key == "RS_SignGlow" and dusk:
            glow = 6.0
        b.inputs["Emission Color"].default_value = C._lin(hx) + (1,)
        b.inputs["Emission Strength"].default_value = glow
        if key == "RS_Glass":
            b.inputs["Alpha"].default_value = 0.18
            b.inputs["Roughness"].default_value = 0.03
            if hasattr(m, "surface_render_method"):
                m.surface_render_method = "BLENDED"


def ground():
    me = bpy.data.meshes.new("ground")
    s = 120.0
    me.from_pydata([(-s, -s, -0.6), (s, -s, -0.6), (s, s, -0.6), (-s, s, -0.6)], [], [(0, 1, 2, 3)])
    o = bpy.data.objects.new("ground", me)
    mat = bpy.data.materials.new("pv_sand")
    mat.diffuse_color = C._lin("#C9B994") + (1,)
    mat.use_nodes = True
    mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = C._lin("#C9B994") + (1,)
    mat.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.95
    me.materials.append(mat)
    bpy.context.scene.collection.objects.link(o)


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for f in sorted(os.listdir(GAME)):
        if f.startswith("SM_Rest_") and f.endswith(".glb"):
            bpy.ops.import_scene.gltf(filepath=os.path.join(GAME, f))
    ground()
    sc = bpy.context.scene
    names = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    sc.render.engine = "BLENDER_EEVEE" if "BLENDER_EEVEE" in names else "BLENDER_EEVEE_NEXT"
    sc.render.resolution_x, sc.render.resolution_y = 1280, 800
    try:
        sc.eevee.taa_render_samples = 32
    except Exception:  # noqa: BLE001
        pass
    w = bpy.data.worlds.new("w")
    w.use_nodes = True
    sc.world = w
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sc.collection.objects.link(sun)
    with open(LAYOUT, encoding="utf-8") as f:
        lay = json.load(f)
    pts = []
    for L in lay["lights"]:
        o = bpy.data.objects.new("pl", bpy.data.lights.new("pl", "POINT"))
        o.data.color = tuple(C._lin(L["colour"]))
        o.location = L["at"]
        o.data.shadow_soft_size = 0.15
        o.data.use_shadow = False
        sc.collection.objects.link(o)
        pts.append((o, L["lumens"]))
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    os.makedirs(OUT, exist_ok=True)
    for name, (eye, tgt, lens, dusk) in VIEWS.items():
        if args and name not in args:
            continue
        set_mats(dusk)
        bg = w.node_tree.nodes["Background"]
        bg.inputs[0].default_value = (0.03, 0.04, 0.09, 1) if dusk else (0.55, 0.65, 0.8, 1)
        bg.inputs[1].default_value = 0.6 if dusk else 0.9
        sun.data.energy = 0.3 if dusk else 3.2
        sun.data.color = (1.0, 0.55, 0.35) if dusk else (1.0, 0.97, 0.92)
        sun.rotation_euler = (math.radians(82 if dusk else 50), 0, math.radians(-40))
        for o, lm in pts:
            o.data.energy = lm / (8.0 if dusk else 14.0)
        cam.location = eye
        cam.rotation_euler = (Vector(tgt) - Vector(eye)).to_track_quat("-Z", "Y").to_euler()
        cam.data.lens = lens
        cam.data.clip_start = 0.05
        sc.render.filepath = os.path.join(OUT, "rest_%s.png" % name)
        bpy.ops.render.render(write_still=True)
        print("PREVIEW", sc.render.filepath, flush=True)


main()
