"""Fast look at a species while tuning its shape: LOD0 only, no clips, no export (~20-40 s instead of a full build).

    Blender -b --python blender/birds/quick_look.py -- <out dir> Id [Id ...]

Exports LOD0 with a perched and a gliding pose to <out>/glb/<Id>/ and runs lineup.py on it: <out>/<Id>_side.png,
<Id>_top.png and <out>/scale.json, as from a finished build, so lineup_sheet.py composes either.
"""
import os
import shutil
import subprocess
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = open(os.path.join(HERE, "build_bird.py")).read().replace("\nmain()\n", "\n")
args = sys.argv[sys.argv.index("--") + 1:]
OUT, IDS = args[0], args[1:]
os.makedirs(OUT, exist_ok=True)


GLB = os.path.join(OUT, "glb")
for sp in IDS:
    g = {"__file__": os.path.join(HERE, "build_bird.py"), "__name__": "quick"}
    sys.argv = ["blender", "--", sp, "--no-export"]
    exec(compile(SRC, "build_bird.py", "exec"), g)
    g["C"].reset()
    g["solve_size"]()
    g["bare_cells"]()
    sb = g["build_mesh"](None, 0)
    albedo, normal, params = g["paint_atlas"]()
    mats = g["make_materials"](albedo, normal, params)
    ob = g["bake_mesh"](sb, mats, f"LOD0_SK_{sp}")
    arm = g["build_armature"]()
    ob.parent = arm
    mod = ob.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    # two one-pose clips, then through glTF like a real build (posing in this process does not deform the same way)
    g["make_action"](arm, "Idle", [(0, g["PERCH"]), (1, g["PERCH"])])
    g["make_action"](arm, "Glide", [(0, g["glide_pose"](0)), (1, g["glide_pose"](0))])
    arm.animation_data.action = None
    for pb in arm.pose.bones:
        pb.location = (0, 0, 0)
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.scale = (1, 1, 1)
    d = os.path.join(GLB, sp)
    os.makedirs(d, exist_ok=True)
    shutil.copy(os.path.join(g["OUTDIR"], f"T_{sp}_Albedo.png"), d)
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    kw = dict(filepath=os.path.join(d, f"{sp}.glb"), export_format="GLB", use_selection=True, export_yup=True, export_apply=False,
              export_normals=True, export_texcoords=True, export_materials="EXPORT", export_skins=True, export_animations=True,
              export_animation_mode="ACTIONS", export_force_sampling=True, export_vertex_color="NONE", export_def_bones=False,
              export_anim_single_armature=True, export_image_format="NONE")
    props = bpy.ops.export_scene.gltf.get_rna_type().properties
    bpy.ops.export_scene.gltf(**{k: v for k, v in kw.items() if k in props or k == "filepath"})
    print("QUICK", sp)
subprocess.run([bpy.app.binary_path, "-b", "--python", os.path.join(HERE, "lineup.py"), "--", GLB, OUT] + IDS, check=True,
               stdout=subprocess.DEVNULL)
