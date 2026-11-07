"""Packs the restaurant's tableware and food meshes (design/restaurant/game/tableware/*.glb from tableware.py) into one .glb,
one node per mesh at its own origin, named exactly like the mesh (as the diner's pack.py), for a single Unreal import:
  Blender -b -P blender/restaurant/pack_tableware.py   ->  design/restaurant/game/tableware_kit.glb
"""
import os

import bpy

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SRC = os.path.join(ROOT, "design", "restaurant", "game", "tableware")
OUT = os.path.join(ROOT, "design", "restaurant", "game", "tableware_kit.glb")
bpy.ops.wm.read_factory_settings(use_empty=True)
for o in list(bpy.data.objects):
    bpy.data.objects.remove(o)
n = 0
for f in sorted(os.listdir(SRC)):
    if not f.endswith(".glb") or not (f.startswith("SM_RT_") or f.startswith("SM_RF_")):
        continue
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.join(SRC, f))
    for o in set(bpy.data.objects) - before:
        if o.type == "MESH":
            o.name = o.data.name = f[:-4]
            n += 1
        else:
            bpy.data.objects.remove(o)
# one material per slot name (the importer made RS_T_Steel.001 etc. for every file after the first)
for o in bpy.data.objects:
    for i, m in enumerate(o.data.materials):
        base = m.name.split(".")[0] if m else None
        if base and base != m.name and base in bpy.data.materials:
            o.data.materials[i] = bpy.data.materials[base]
bad = [o.name for o in bpy.data.objects if o.name != o.data.name or "." in o.name]
assert not bad, bad
bpy.ops.export_scene.gltf(filepath=OUT, export_format="GLB", export_yup=True, export_apply=True)
print("PACK", n, "meshes ->", OUT, flush=True)
