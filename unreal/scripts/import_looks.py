"""Imports blender/hero/export/looks/<name>.glb (blender/hero/build_looks.py) as /Game/Characters/Hero/Look/<name>: the
barber's haircuts and the cap that ULyudLook puts on the head of the hero's shadow (docs/look.md).

  unreal/scripts/import_looks.sh   (one UnrealEditor-Cmd per mesh: Interchange imports only the first glb in a process)
  LOOKS=SM_HeroCap UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this file> -unattended -nullrhi

They only cast a shadow and move with the head every frame: no collision, no distance field, no Nanite. Local only, like
the hero's own content (Content/Characters/Hero is not in git).
"""
import os

import unreal

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SRC = os.path.join(ROOT, "blender", "hero", "export", "looks")
DEST = "/Game/Characters/Hero/Look"
NAMES = ["SM_HeroHair_crop", "SM_HeroHair_short", "SM_HeroHair_parted", "SM_HeroCap"]
if os.environ.get("LOOKS"):
    NAMES = [n for n in os.environ["LOOKS"].split(",") if n]
eal = unreal.EditorAssetLibrary
mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
done = 0
for name in NAMES:
    glb = os.path.join(SRC, name + ".glb")
    if not os.path.exists(glb):
        unreal.log_error("LOOKS no %s (run blender/hero/build_looks.py)" % glb)
        continue
    tmp = DEST + "/_import_" + name
    if eal.does_directory_exist(tmp):
        eal.delete_directory(tmp)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    mgr.import_asset(tmp, unreal.InterchangeManager.create_source_data(glb), params)
    mgr.wait_until_all_tasks_done(True)
    if not eal.list_assets(tmp, True):
        unreal.log_error("LOOKS %s: nothing imported (a second Interchange import in one process?)" % name)
        continue
    for p in eal.list_assets(tmp, True):
        a = eal.load_asset(p)
        asset = p.split(".")[-1]
        if isinstance(a, unreal.StaticMesh):
            new = DEST + "/" + name
            a.set_editor_property("generate_mesh_distance_field", False)
            try:
                ns = a.get_editor_property("nanite_settings")
                ns.enabled = False
                a.set_editor_property("nanite_settings", ns)
            except Exception as e:  # noqa: BLE001
                unreal.log_warning("LOOKS %s: nanite settings: %s" % (name, e))
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
        unreal.log_warning("LOOKS %s -> %s" % (asset, new))
    for p in eal.list_assets(tmp, True):
        eal.delete_asset(p)
    eal.delete_directory(tmp)
    done += 1
eal.save_directory(DEST)
unreal.log_warning("LOOKS done: %d of %d" % (done, len(NAMES)))
