"""The sea railway's kit into the project (docs/railway.md): design/railway/game/rail_kit.glb (blender/railway/rail_kit.py)
-> /Game/Railway/Meshes/rail_kit/... (Interchange puts the scene in a folder named after the file). Run headless:

  UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/import_railway.py   (unreal/scripts/import_railway.sh)

Nanite off (Interchange's glTF parent lacks the usage flag: garbled fallbacks otherwise), simple collision only where the
game walks on a piece's own triangles (the island, the shelter's floor is the platform's), the glTF materials kept
(rail_kit.py's palette: base colour, roughness, metal, emission); the game swaps the glass and the lamps for its own.
"""
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
KIT = os.path.join(REPO, "design", "railway", "game", "rail_kit.glb")
MESHES = "/Game/Railway/Meshes"
DEST = MESHES + "/rail_kit"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log_warning("RAILWAY " + msg)


def main():
    if not os.path.isfile(KIT):
        log("no %s: keeping the committed meshes" % KIT)
        return
    if eal.does_directory_exist(DEST):
        eal.delete_directory(DEST)
    src = unreal.InterchangeManager.create_source_data(KIT)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_scene(MESHES, src, params)
    mgr.wait_until_all_tasks_done(True)
    n = 0
    for path in eal.list_assets(DEST, True):
        a = eal.load_asset(path)
        if isinstance(a, unreal.StaticMesh):
            s = a.get_editor_property("nanite_settings")
            s.enabled = False
            a.set_editor_property("nanite_settings", s)
            body = a.get_editor_property("body_setup")
            if body:
                walk = a.get_name() in ("SM_RL_Island", "SM_RL_Shelter", "SM_RL_XingGateBox", "SM_RL_XingPost", "SM_RL_NameBoard")
                body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE if walk
                                         else unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
            log("mesh %s: %d triangles" % (a.get_name(), a.get_num_triangles(0)))
            n += 1
    for path in eal.list_assets(DEST, True):
        a = eal.load_asset(path)
        if isinstance(a, unreal.MaterialInstanceConstant):
            name = a.get_name()
            for pname in mel.get_scalar_parameter_names(a):
                pname = str(pname)
                # glTF's metalness is right only on the chrome and steel
                if "etal" in pname and "chrome" not in name.lower() and "steel" not in name.lower():
                    mel.set_material_instance_scalar_parameter_value(a, pname, 0.0)
            mel.update_material_instance(a)
    eal.save_directory(DEST)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for act in eas.get_all_level_actors():
        if act.get_name().startswith("SM_RL_") or "rail_kit" in str(act.get_folder_path()):
            eas.destroy_actor(act)
    log("kit: %d meshes into %s" % (n, DEST))


main()
