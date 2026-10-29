"""Blender: bounding box, triangle count and three workbench views of a .glb (to see how a generated model sits).
  Blender -b -P blender/transport/inspect_glb.py -- in.glb out_prefix
"""
import sys, math
import bpy
from mathutils import Vector
argv = sys.argv[sys.argv.index("--") + 1:]
src, out = argv[0], argv[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
objs = [o for o in bpy.context.scene.objects if o.type == "MESH"]
lo = Vector((1e9,) * 3); hi = Vector((-1e9,) * 3); tris = 0
for o in objs:
    for c in o.bound_box:
        w = o.matrix_world @ Vector(c)
        lo = Vector(map(min, lo, w)); hi = Vector(map(max, hi, w))
    tris += sum(len(p.vertices) - 2 for p in o.data.polygons)
print("INSPECT", src, "objects", len(objs), "tris", tris, "min", tuple(round(v, 3) for v in lo), "max", tuple(round(v, 3) for v in hi))
for o in objs:
    print("INSPECT  obj", o.name, [m.name for m in o.data.materials], [ (n.image.name, n.image.size[:]) for m in o.data.materials if m and m.use_nodes for n in m.node_tree.nodes if n.type == 'TEX_IMAGE' and n.image])
sc = bpy.context.scene
sc.render.engine = "BLENDER_WORKBENCH"
sc.display.shading.light = "STUDIO"; sc.display.shading.color_type = "TEXTURE"
sc.render.resolution_x = sc.render.resolution_y = 512
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam")); sc.collection.objects.link(cam); sc.camera = cam
cam.data.type = "ORTHO"
c = (lo + hi) / 2; size = max(hi - lo) * 1.2
cam.data.ortho_scale = size
for name, d in (("front_-y", Vector((0, -1, 0))), ("side_+x", Vector((1, 0, 0))), ("top", Vector((0, 0, 1)))):
    cam.location = c + d * size * 3
    cam.rotation_euler = (d * -1).to_track_quat("-Z", "Y" if name != "top" else "Y").to_euler()
    sc.render.filepath = "%s_%s.png" % (out, name)
    bpy.ops.render.render(write_still=True)
