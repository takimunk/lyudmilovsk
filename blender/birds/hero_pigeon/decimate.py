"""gen/hi3d_pigeon.glb (2M triangles, 8k textures) -> gen/hi3d_dec.blend: the mesh at ~160k triangles, its frame
turned to the bird's (X forward... not yet: only reduced), textures kept. First step of build_hero_pigeon.py.

    Blender -b --python blender/birds/hero_pigeon/decimate.py
"""
import os
import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=os.path.join(HERE, "gen", "hi3d_pigeon.glb"))
ob = next(o for o in bpy.data.objects if o.type == "MESH")
ob.data.transform(ob.matrix_world)
ob.parent = None
ob.matrix_world.identity()
for o in list(bpy.data.objects):
    if o is not ob:
        bpy.data.objects.remove(o)
bpy.context.view_layer.objects.active = ob
m = ob.modifiers.new("dec", "DECIMATE")
m.ratio = 0.08
m.use_collapse_triangulate = True
bpy.ops.object.modifier_apply(modifier="dec")
print("TRIS", len(ob.data.polygons))
for img in bpy.data.images:
    img.pack()
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, "gen", "hi3d_dec.blend"), compress=True)
