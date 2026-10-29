"""Renders pictures of the lobby props (Workbench, flat colours, front and three-quarter): 
  Blender -b -P blender/transport/lobby_preview.py -- <outdir> [Name ...]"""
import math
import os
import sys

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import lobby_props as LP

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
out = argv[0] if argv else "/tmp/lobby_preview"
want = argv[1:]
os.makedirs(out, exist_ok=True)
for name, _f in LP.PROPS:
    if want and name not in want:
        continue
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=os.path.join(LP.OUT, "SM_TR_Lobby_%s.glb" % name))
    objs = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    for o in objs:
        for slot in o.material_slots:
            m = slot.material
            if m and m.name.split(".")[0] in LP.COL:
                c = LP.COL[m.name.split(".")[0]]
                m.diffuse_color = c + (1,)
    lo = Vector((1e9,) * 3); hi = Vector((-1e9,) * 3)
    for o in objs:
        for v in o.bound_box:
            w = o.matrix_world @ Vector(v)
            lo = Vector(map(min, lo, w)); hi = Vector(map(max, hi, w))
    c = (lo + hi) / 2
    size = max((hi - lo).length, 0.3)
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.display.shading.color_type = "MATERIAL"
    sc.display.shading.light = "STUDIO"
    sc.render.resolution_x, sc.render.resolution_y = 900, 700
    sc.world = bpy.data.worlds.new("w"); sc.world.color = (0.3, 0.3, 0.32)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam")); sc.collection.objects.link(cam); sc.camera = cam
    for k, (az, el) in enumerate(((35, 20), (-60, 15))):
        a, e = math.radians(az), math.radians(el)
        cam.location = c + Vector((math.cos(a) * math.cos(e), -math.sin(a) * math.cos(e), math.sin(e))) * size * 1.25
        d = c - cam.location
        cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
        sc.render.filepath = os.path.join(out, "%s_%d.png" % (name, k))
        bpy.ops.render.render(write_still=True)
    print("PREVIEW", name)
