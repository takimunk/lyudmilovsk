"""Imports the first-person sit animation baked by blender/scripts/build_zoomer_fp_sit.py
(blender/humans_local/zoomer_fp_anim.glb, armature + one action) onto the shared human skeleton as
/Game/Characters/Zoomer/A_Zoomer_FP_SitAtComputer.

Run headless (no editor open on this checkout), after the humans assets are in Content/Characters/Humans:
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import os
import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
GLB = os.path.normpath(os.path.join(HERE, "..", "..", "blender", "humans_local", "zoomer_fp_anim.glb"))
SKELETON = "/Game/Characters/Humans/SK_Human_Skeleton"
PIPELINE = "/Game/Characters/Humans/Import/IP_HumanAssets"
DEST = "/Game/Characters/Zoomer"
TMP = DEST + "/_fp_import"
FINAL = DEST + "/A_Zoomer_FP_SitAtComputer"
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log_warning("ZOOMERFP " + msg)


for d in (TMP,):
    if eal.does_directory_exist(d):
        eal.delete_directory(d)
if eal.does_asset_exist(FINAL):
    eal.delete_asset(FINAL)

src = unreal.InterchangeManager.create_source_data(GLB)
params = unreal.ImportAssetParameters()
params.is_automated = True
params.override_pipelines.append(unreal.SoftObjectPath("/Interchange/Pipelines/DefaultGLTFPipeline.DefaultGLTFPipeline"))
params.override_pipelines.append(unreal.SoftObjectPath(PIPELINE))
mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
mgr.import_asset(TMP, src, params)
mgr.wait_until_all_tasks_done(True)

anim = None
for p in eal.list_assets(TMP, True):
    a = eal.load_asset(p)
    log("%s %s" % (a.get_class().get_name(), p))
    if isinstance(a, unreal.AnimSequence):
        anim = (p, a)
if not anim:
    raise RuntimeError("no AnimSequence imported from " + GLB)
eal.rename_asset(anim[0], FINAL)
a = unreal.load_asset(FINAL)
a.set_editor_property("enable_root_motion", False)
eal.save_loaded_asset(a)
log("skeleton %s, length %.2f s" % (a.get_editor_property("skeleton").get_path_name(), a.get_editor_property("sequence_length")))
for p in eal.list_assets(TMP, True):
    eal.delete_asset(p)
eal.delete_directory(TMP)
log("done")
