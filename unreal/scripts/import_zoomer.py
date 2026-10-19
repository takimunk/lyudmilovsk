"""Imports blender/export/character_zoomer.glb into /Game/Characters/Zoomer
(skeletal mesh, skeleton, materials, textures, Zoomer_SitDown and Zoomer_TypeIdle animations).

Run headless (editor closed):
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import os
import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
GLB = os.path.normpath(os.path.join(HERE, "..", "..", "blender", "export", "character_zoomer.glb"))
DEST = "/Game/Characters/Zoomer"
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log_warning("ZOOMER " + msg)


if eal.does_directory_exist(DEST):
    eal.delete_directory(DEST)

src = unreal.InterchangeManager.create_source_data(GLB)
params = unreal.ImportAssetParameters()
params.is_automated = True
mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
mgr.import_asset(DEST, src, params)
mgr.wait_until_all_tasks_done(True)

for p in eal.list_assets(DEST, True):
    a = eal.load_asset(p)
    log("%s  %s" % (a.get_class().get_name(), p))
eal.save_directory(DEST)
log("done")
