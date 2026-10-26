"""Imports Saved/ClipSources/gamepad.glb (blender/humans/gamepad.py) as /Game/Characters/Humans/Props/SM_Gamepad,
the prop ULyudBodyAnimInstance shows between a console player's hands (PC_Gamepad).

  UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this file> -unattended -nullrhi
"""
import os

import unreal

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
GLB = os.path.join(ROOT, "Saved", "ClipSources", "gamepad.glb")
DEST = "/Game/Characters/Humans/Props"
eal = unreal.EditorAssetLibrary
if eal.does_directory_exist(DEST + "/_import"):
    eal.delete_directory(DEST + "/_import")
params = unreal.ImportAssetParameters()
params.is_automated = True
mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
mgr.import_asset(DEST + "/_import", unreal.InterchangeManager.create_source_data(GLB), params)
mgr.wait_until_all_tasks_done(True)
for p in eal.list_assets(DEST + "/_import", True):
    a = eal.load_asset(p)
    name = p.split(".")[-1]
    if isinstance(a, unreal.StaticMesh):
        new = DEST + "/SM_Gamepad"
    elif isinstance(a, unreal.MaterialInterface):
        new = DEST + "/Materials/" + name.replace(".", "_")
    else:
        continue
    if eal.does_asset_exist(new):
        eal.delete_asset(new)
    eal.rename_asset(p, new)
    unreal.log_warning("GAMEPAD %s -> %s" % (name, new))
for p in eal.list_assets(DEST + "/_import", True):
    eal.delete_asset(p)
eal.delete_directory(DEST + "/_import")
eal.save_directory(DEST)
unreal.log_warning("GAMEPAD done")
