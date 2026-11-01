"""Imports the fish built by blender/fish/build_fish.py (blender/fish/export/<Id>.glb and <Id>_D.png) into /Game/Sea/Fish:

  SM_<Id>     the static mesh (slots Body and Fins, vertex colours for the swimming wave), LOD1 reduced from it
  T_<Id>_D    its texture (RGB colour, A the fins' membranes)

ULyudUnderwaterSubsystem gives each species M_LyudFish / M_LyudFishFin instances at play. Headless, no editor open on this
checkout; Interchange imports one .glb per process, so import_fish.sh runs one process per species:
  UnrealEditor <uproject> -run=pythonscript -script="unreal/scripts/import_fish.py textures Herring Cod ..."
  UnrealEditor <uproject> -run=pythonscript -script="unreal/scripts/import_fish.py mesh Herring"
"""
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
EXPORT = os.path.normpath(os.path.join(HERE, "..", "..", "blender", "fish", "export"))
DEST = "/Game/Sea/Fish"
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(s):
    unreal.log_warning("FISHIMPORT " + s)


def do_textures(ids):
    """The textures with the classic factory (any number in one process, unlike Interchange)."""
    if not eal.does_directory_exist(DEST):
        eal.make_directory(DEST)
    for sid in ids:
        f = os.path.join(EXPORT, "%s_D.png" % sid)
        if not os.path.exists(f):
            log("%s: no texture %s" % (sid, f))
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", f)
        task.set_editor_property("destination_path", DEST)
        task.set_editor_property("destination_name", "T_%s_D" % sid)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        task.set_editor_property("factory", unreal.TextureFactory())
        tools.import_asset_tasks([task])
        t = unreal.load_asset("%s/T_%s_D" % (DEST, sid))
        if t is None:
            log("%s: texture import failed" % sid)
            continue
        # a small fish never needs more than 1024 wide; keep the alpha (the fins' mask) uncompressed enough
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        t.set_editor_property("srgb", True)
        t.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
        eal.save_loaded_asset(t)
        log("%s: texture %s" % (sid, t.get_path_name()))


def do_mesh(sid):
    src_file = os.path.join(EXPORT, sid + ".glb")
    if not os.path.exists(src_file):
        log("%s: no %s" % (sid, src_file))
        return
    tmp = "%s/_import_%s" % (DEST, sid)
    if eal.does_directory_exist(tmp):
        eal.delete_directory(tmp)
    src = unreal.InterchangeManager.create_source_data(src_file)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.override_pipelines.append(unreal.SoftObjectPath("/Interchange/Pipelines/DefaultGLTFPipeline.DefaultGLTFPipeline"))
    params.override_pipelines.append(unreal.SoftObjectPath("/Interchange/Pipelines/DefaultGLTFAssetsPipeline.DefaultGLTFAssetsPipeline"))
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_asset(tmp, src, params)
    mgr.wait_until_all_tasks_done(True)
    mesh = None
    for p in eal.list_assets(tmp, True):
        a = eal.load_asset(p)
        if isinstance(a, unreal.StaticMesh):
            mesh = a
    if mesh is None:
        log("%s: no static mesh in the .glb" % sid)
        return
    dest = "%s/SM_%s" % (DEST, sid)
    if eal.does_asset_exist(dest):
        eal.delete_asset(dest)
    eal.rename_asset(mesh.get_path_name(), dest)
    mesh = unreal.load_asset(dest)
    # the materials come from the game (M_LyudFish instances per slot): drop the glTF's
    mats = mesh.get_editor_property("static_materials")
    for i in range(len(mats)):
        m = mats[i]
        m.material_interface = None
        mats[i] = m
    mesh.set_editor_property("static_materials", mats)
    # no collision, no nav, a reduced LOD1 for the fish a few metres off (the murk hides the rest)
    mesh.set_editor_property("light_map_resolution", 4)
    # the editor subsystem is not there in a commandlet: the library has the same calls
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.EditorStaticMeshLibrary
    opts = unreal.StaticMeshReductionOptions()
    opts.set_editor_property("auto_compute_lod_screen_size", False)
    s0 = unreal.StaticMeshReductionSettings()
    s0.set_editor_property("percent_triangles", 1.0)
    s0.set_editor_property("screen_size", 1.0)
    s1 = unreal.StaticMeshReductionSettings()
    s1.set_editor_property("percent_triangles", 0.3)
    s1.set_editor_property("screen_size", 0.05)
    opts.set_editor_property("reduction_settings", [s0, s1])
    sub.set_lods(mesh, opts)
    sub.remove_collisions(mesh)
    eal.save_loaded_asset(mesh)
    eal.delete_directory(tmp)
    b = mesh.get_bounds()
    log("%s: %s, %d LODs, %d slots %s, half size %.1f x %.1f x %.1f cm" % (
        sid, dest, sub.get_lod_count(mesh), len(mats), [str(m.material_slot_name) for m in mats], b.box_extent.x, b.box_extent.y, b.box_extent.z))


if __name__ == "__main__":
    args = sys.argv[1:]
    if args and args[0] == "textures":
        do_textures(args[1:])
    elif len(args) == 2 and args[0] == "mesh":
        do_mesh(args[1])
    else:
        log("usage: import_fish.py textures <Id>... | mesh <Id>")
