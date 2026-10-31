"""Packs the diner's game meshes (design/diner/game/SM_Diner_*.glb from building.py and tableware.py, the burger's bite stages
SM_Food_diner_burger_<k>.glb) into one .glb, one node per mesh at its own origin, so Unreal imports them in one go (Interchange
skips a second scene import in a process): unreal/scripts/import_diner.py.
  Blender -b -P blender/diner/pack.py   ->  design/diner/game/diner_kit.glb
"""
import os

import bpy

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
GAME = os.path.join(ROOT, "design", "diner", "game")
bpy.ops.wm.read_factory_settings(use_empty=True)
for o in list(bpy.data.objects):
    bpy.data.objects.remove(o)
n = 0
for f in sorted(os.listdir(GAME)):
    if not f.endswith(".glb") or not (f.startswith("SM_Diner_") or f.startswith("SM_Food_diner_")):
        continue
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.join(GAME, f))
    for o in set(bpy.data.objects) - before:
        if o.type == "MESH":
            o.name = o.data.name = f[:-4]
            n += 1
        else:
            bpy.data.objects.remove(o)
# one material per slot name: the importer made DN_Steel.001 etc. for every file after the first
for o in bpy.data.objects:
    for i, m in enumerate(o.data.materials):
        base = m.name.split(".")[0] if m else None
        if base and base != m.name and base in bpy.data.materials:
            o.data.materials[i] = bpy.data.materials[base]
bpy.ops.export_scene.gltf(filepath=os.path.join(GAME, "diner_kit.glb"), export_format="GLB", export_yup=True, export_apply=True)
print("PACK", n, "meshes")
