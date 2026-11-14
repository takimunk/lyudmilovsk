"""Imports Saved/ClipSources/props/<name>.glb (blender/humans/props.py) as /Game/Characters/Humans/Props/SM_Prop_<name>:
the things the townsfolk's manners put in their hands (docs/town_manners.md), and the painter's easel.

  unreal/scripts/import_props.sh   (one UnrealEditor-Cmd per prop: Interchange imports only the first glb in a process)
  PROPS=phone UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this file> -unattended -nullrhi

Moved every frame in somebody's hands: no collision, no distance field, no Nanite.
"""
import os

import unreal

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SRC = os.path.join(ROOT, "Saved", "ClipSources", "props")
DEST = "/Game/Characters/Humans/Props"
NAMES = ["phone", "cigarette", "camera", "umbrella", "rod", "brush", "binoculars", "newspaper", "book", "knitting",
         "ice_cream", "cup", "bag", "easel"]
if os.environ.get("PROPS"):
    NAMES = [n for n in os.environ["PROPS"].split(",") if n]
eal = unreal.EditorAssetLibrary
mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
done = 0
for name in NAMES:
    glb = os.path.join(SRC, name + ".glb")
    if not os.path.exists(glb):
        unreal.log_error("PROPS no %s (run blender/humans/props.py)" % glb)
        continue
    tmp = DEST + "/_import_" + name
    if eal.does_directory_exist(tmp):
        eal.delete_directory(tmp)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    mgr.import_asset(tmp, unreal.InterchangeManager.create_source_data(glb), params)
    mgr.wait_until_all_tasks_done(True)
    if not eal.list_assets(tmp, True):
        unreal.log_error("PROPS %s: nothing imported (a second Interchange import in one process?)" % name)
        continue
    for p in eal.list_assets(tmp, True):
        a = eal.load_asset(p)
        asset = p.split(".")[-1]
        if isinstance(a, unreal.StaticMesh):
            new = DEST + "/SM_Prop_" + name
            a.set_editor_property("generate_mesh_distance_field", False)
            try:
                ns = a.get_editor_property("nanite_settings")
                ns.enabled = False
                a.set_editor_property("nanite_settings", ns)
            except Exception as e:  # noqa: BLE001
                unreal.log_warning("PROPS %s: nanite settings: %s" % (name, e))
            body = a.get_editor_property("body_setup")
            if body:
                body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
            unreal.EditorStaticMeshLibrary.remove_collisions(a)
        elif isinstance(a, unreal.MaterialInterface):
            new = DEST + "/Materials/" + asset.replace(".", "_")
        else:
            continue
        if eal.does_asset_exist(new):
            eal.delete_asset(new)
        eal.rename_asset(p, new)
        unreal.log_warning("PROPS %s -> %s" % (asset, new))
    for p in eal.list_assets(tmp, True):
        eal.delete_asset(p)
    eal.delete_directory(tmp)
    done += 1
eal.save_directory(DEST)
unreal.log_warning("PROPS done: %d of %d" % (done, len(NAMES)))
