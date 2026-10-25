"""Packs the optimized game meshes of the cable-car metro (design/transport/game/SM_TR_*.glb, optimize.py) into one
.glb, one node per mesh, so Unreal imports them in one go (Interchange skips a second scene import in a process).
  Blender -b -P blender/transport/pack.py   ->  design/transport/game/transport_kit.glb
"""
import os
import bpy

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
GAME = os.path.join(ROOT, "design", "transport", "game")
bpy.ops.wm.read_factory_settings(use_empty=True)
for f in sorted(os.listdir(GAME)):
    if not (f.startswith("SM_TR_") and f.endswith(".glb")):
        continue
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.join(GAME, f))
    for o in set(bpy.data.objects) - before:
        if o.type == "MESH":
            o.name = o.data.name = f[:-4]
            for m in o.data.materials:
                # the textured one gets the mesh's name; the glass and the room keep theirs (TR_Glass*, TR_Interior*:
                # the game puts its own materials on those slots)
                if m and not m.name.startswith(("TR_Glass", "TR_Interior")):
                    m.name = "MI_" + f[3:-4]
for img in bpy.data.images:
    img.pack()
bpy.ops.export_scene.gltf(filepath=os.path.join(GAME, "transport_kit.glb"), export_format="GLB", export_image_format="JPEG",
                          export_jpeg_quality=88, export_yup=True, export_apply=True)
print("PACK", len([o for o in bpy.data.objects if o.type == "MESH"]), "meshes")
