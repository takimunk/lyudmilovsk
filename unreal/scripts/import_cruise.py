"""The cruise's islands into the project (docs/ship.md): design/ship/game/seascape_kit.glb (blender/ship/seascape_kit.py)
-> /Game/Cruise/Meshes/seascape_kit/... (Interchange puts the scene in a folder named after the file). Run headless:

  UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/import_cruise.py   (unreal/scripts/import_cruise.sh)

Nanite off (the pieces are movable and seen from kilometres: their own far levels are in the kit), no collision at all
(nobody lands), no distance fields. The rock and the land get the kit's own materials made here: the mesh's vertex
colour (weathering, guano, weed, grass: seascape_kit.py's rock_colour) times a tiling grey rock detail,
design/ship/textures/T_SS_rock_{C,N}.png, 8 m a tile (the kit's UVs are metres / 8). The painted slots keep the glTF
materials; the game swaps glow_lamp and glow_window for its own (ALyudCruiseSeascape).
"""
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
KIT = os.path.join(REPO, "design", "ship", "game", "seascape_kit.glb")
TEXTURES = os.path.join(REPO, "design", "ship", "textures")
MESHES = "/Game/Cruise/Meshes"
DEST = MESHES + "/seascape_kit"
TEX_DIR = "/Game/Cruise/Textures"
MAT_DIR = "/Game/Cruise/Materials"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log_warning("CRUISE " + msg)


def import_textures():
    names = sorted(f for f in os.listdir(TEXTURES) if f.startswith("T_SS_") and f.endswith(".png")) if os.path.isdir(TEXTURES) else []
    tasks = []
    for f in names:
        t = unreal.AssetImportTask()
        t.filename = os.path.join(TEXTURES, f)
        t.destination_path = TEX_DIR
        t.automated = True
        t.replace_existing = True
        t.save = False
        tasks.append(t)
    if tasks:
        tools.import_asset_tasks(tasks)
    for f in names:
        a = eal.load_asset(TEX_DIR + "/" + f[:-4])
        if not a:
            continue
        if f[-5] == "N":
            a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            a.set_editor_property("srgb", False)
            a.set_editor_property("flip_green_channel", True)
        eal.save_loaded_asset(a)
    return len(names)


def node(m, cls, x, y):
    return mel.create_material_expression(m, cls, x, y)


def natural_material(name, textured):
    """M_SS_Rock / M_SS_Land: vertex colour (x the rock detail x 2 and its normal map when textured), rough, no metal."""
    path = MAT_DIR + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = tools.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    vc = node(m, unreal.MaterialExpressionVertexColor, -900, 0)
    rough = node(m, unreal.MaterialExpressionConstant, -300, 300)
    rough.set_editor_property("r", 0.93)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    col = eal.load_asset(TEX_DIR + "/T_SS_rock_C") if textured else None
    nrm = eal.load_asset(TEX_DIR + "/T_SS_rock_N") if textured else None
    if col:
        tex = node(m, unreal.MaterialExpressionTextureSample, -900, 200)
        tex.set_editor_property("texture", col)
        mul = node(m, unreal.MaterialExpressionMultiply, -600, 0)
        mel.connect_material_expressions(vc, "", mul, "A")
        mel.connect_material_expressions(tex, "RGB", mul, "B")
        gain = node(m, unreal.MaterialExpressionMultiply, -400, 0)
        gain.set_editor_property("const_b", 2.0)
        mel.connect_material_expressions(mul, "", gain, "A")
        mel.connect_material_property(gain, "", unreal.MaterialProperty.MP_BASE_COLOR)
    else:
        mel.connect_material_property(vc, "", unreal.MaterialProperty.MP_BASE_COLOR)
    if nrm:
        tn = node(m, unreal.MaterialExpressionTextureSample, -900, 500)
        tn.set_editor_property("texture", nrm)
        tn.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        mel.connect_material_property(tn, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def main():
    log("textures: %d" % import_textures())
    if not os.path.isfile(KIT):
        log("no %s: run python3 blender/ship/seascape_kit.py (the export is not in git)" % KIT)
        return
    mats = {"ss_rock": natural_material("M_SS_Rock", True), "ss_land": natural_material("M_SS_Land", False)}
    if eal.does_directory_exist(DEST):
        eal.delete_directory(DEST)
    src = unreal.InterchangeManager.create_source_data(KIT)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_scene(MESHES, src, params)
    mgr.wait_until_all_tasks_done(True)
    n = 0
    total = 0
    for path in eal.list_assets(DEST, True):
        a = eal.load_asset(path)
        if isinstance(a, unreal.StaticMesh):
            s = a.get_editor_property("nanite_settings")
            s.enabled = False
            a.set_editor_property("nanite_settings", s)
            a.set_editor_property("generate_mesh_distance_field", False)
            body = a.get_editor_property("body_setup")
            if body:
                body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
            for i, sl in enumerate(a.get_editor_property("static_materials")):
                slot = str(sl.material_slot_name)
                for key, mat in mats.items():
                    if slot.startswith(key):
                        a.set_material(i, mat)
            tris = a.get_num_triangles(0)
            total += tris
            log("mesh %s: %d triangles" % (a.get_name(), tris))
            n += 1
    for path in eal.list_assets(DEST, True):
        a = eal.load_asset(path)
        if isinstance(a, unreal.MaterialInstanceConstant):
            for pname in mel.get_scalar_parameter_names(a):
                if "etal" in str(pname):
                    mel.set_material_instance_scalar_parameter_value(a, str(pname), 0.0)
            mel.update_material_instance(a)
    eal.save_directory(DEST)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for act in eas.get_all_level_actors():
        if act.get_name().startswith("SS_") or "seascape_kit" in str(act.get_folder_path()):
            eas.destroy_actor(act)
    log("kit: %d meshes, %d triangles into %s" % (n, total, DEST))


main()
