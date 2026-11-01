"""The street food kit into the project (docs/food.md): blender/food/export/food_kit.glb (the foods' bite stages, SM_Food_<id>_<k>)
and stall_kit.glb (SM_Stall_*), both on one atlas T_FoodKit_{C,N,R}.png -> /Game/Food/SM_Food_* and /Game/Food/Stalls/SM_Stall_*
with the material instance MI_FoodKit of M_FoodKit (base colour, normal, roughness). Run headless, one step per process
(Interchange imports one scene per process; unreal/scripts/import_food.sh runs them in turn):

  FOOD_STEP=textures|food|stalls UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/import_food.py

Nanite off. Foods: no collision (they ride on the hand). Stalls: their own triangles as collision (people walk round them).
"""
import glob
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
DEST = "/Game/Food"
TEX = DEST + "/Textures"
EXPORT = os.path.join(REPO, "blender", "food", "export")
KITS = {
    "food": (EXPORT, "food_kit", "FoodKit", DEST),
    "stalls": (EXPORT, "stall_kit", "FoodKit", DEST + "/Stalls"),
}
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log_warning("FOOD " + msg)


def get_or_create(name, folder, cls, factory):
    path = "%s/%s" % (folder, name)
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    return tools.create_asset(name, folder, cls, factory)


def import_textures():
    n = 0
    for folder, kit in ((EXPORT, "FoodKit"),):
        for suffix in ("C", "N", "R"):
            src = os.path.join(folder, "T_%s_%s.png" % (kit, suffix))
            if not os.path.isfile(src):
                log("no %s" % src)
                continue
            t = unreal.AssetImportTask()
            t.filename = src
            t.destination_path = TEX
            t.destination_name = "T_%s_%s" % (kit, suffix)
            t.automated = True
            t.replace_existing = True
            t.save = False
            tools.import_asset_tasks([t])
            a = eal.load_asset("%s/T_%s_%s" % (TEX, kit, suffix))
            if not a:
                continue
            if suffix == "N":
                a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
                a.set_editor_property("srgb", False)
                a.set_editor_property("flip_green_channel", True)     # OpenGL normals from Blender
            elif suffix == "R":
                a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
                a.set_editor_property("srgb", False)
            eal.save_loaded_asset(a)
            n += 1
    log("textures: %d" % n)
    master()


def node(m, cls, x, y):
    return mel.create_material_expression(m, cls, x, y)


def master():
    """M_FoodKit: opaque, one-sided (cloth is two layers), base colour x Tint, normal, roughness."""
    m = get_or_create("M_FoodKit", DEST, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(m)
    m.set_editor_property("two_sided", False)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    m.set_editor_property("used_with_static_lighting", False)
    defaults = {"C": "T_FoodKit_C", "N": "T_FoodKit_N", "R": "T_FoodKit_R"}
    samplers = {"C": unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, "N": unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
                "R": unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE}
    names = {"C": "BaseColor", "N": "Normal", "R": "Roughness"}
    params = {}
    for i, k in enumerate(("C", "N", "R")):
        t = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, -200 + 300 * i)
        t.set_editor_property("parameter_name", names[k])
        t.set_editor_property("sampler_type", samplers[k])
        tex = eal.load_asset("%s/%s" % (TEX, defaults[k]))
        if tex:
            t.set_editor_property("texture", tex)
        params[k] = t
    tint = node(m, unreal.MaterialExpressionVectorParameter, -700, -450)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    mul = node(m, unreal.MaterialExpressionMultiply, -350, -250)
    mel.connect_material_expressions(params["C"], "RGB", mul, "A")
    mel.connect_material_expressions(tint, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(params["N"], "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(params["R"], "R", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    for kit in ("FoodKit",):
        mi = get_or_create("MI_%s" % kit, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, m)
        for k in ("C", "N", "R"):
            tex = eal.load_asset("%s/T_%s_%s" % (TEX, kit, k))
            if tex:
                mel.set_material_instance_texture_parameter_value(mi, names[k], tex)
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
    log("material M_FoodKit + MI_FoodKit")


def import_signs():
    """The stalls' signs, one picture per language: /Game/Food/Localized/<lang>/T_FoodSign_<name> and M_FoodSign (Image),
    the material of every FoodSign_<name> slot; ULyudStallSubsystem sets the picture of the game's language."""
    n = 0
    for lang in ("ru", "en", "ja"):
        for src in sorted(glob.glob(os.path.join(EXPORT, "signs", lang, "T_FoodSign_*.png"))):
            t = unreal.AssetImportTask()
            t.filename = src
            t.destination_path = "%s/Localized/%s" % (DEST, lang)
            t.destination_name = os.path.basename(src)[:-4]
            t.automated = True
            t.replace_existing = True
            t.save = True
            tools.import_asset_tasks([t])
            n += 1
    m = get_or_create("M_FoodSign", DEST, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(m)
    t = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -500, 0)
    t.set_editor_property("parameter_name", "Image")
    tex = eal.load_asset("%s/Localized/ru/T_FoodSign_cheese" % DEST)
    if tex:
        t.set_editor_property("texture", tex)
    mel.connect_material_property(t, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    r = node(m, unreal.MaterialExpressionConstant, -300, 200)
    r.set_editor_property("r", 0.85)
    mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log("signs: %d pictures, M_FoodSign" % n)
    return m


def import_kit(step):
    folder, name, kit, dest = KITS[step]
    glb = os.path.join(folder, name + ".glb")
    if not os.path.isfile(glb):
        log("no %s" % glb)
        return
    tmp = "%s/_import_%s" % (DEST, name)
    if eal.does_directory_exist(tmp):
        eal.delete_directory(tmp)
    src = unreal.InterchangeManager.create_source_data(glb)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_scene(tmp, src, params)
    mgr.wait_until_all_tasks_done(True)
    mi = eal.load_asset("%s/MI_%s" % (DEST, kit))
    sign_mat = import_signs() if step == "stalls" else None
    n = 0
    for path in eal.list_assets(tmp, True):
        a = eal.load_asset(path)
        if not isinstance(a, unreal.StaticMesh):
            continue
        mesh = a.get_name()
        target = "%s/%s" % (dest, mesh)
        if eal.does_asset_exist(target):
            eal.delete_asset(target)
        if not eal.rename_asset(path, target):
            log("could not move %s" % path)
            continue
        a = eal.load_asset(target)
        s = a.get_editor_property("nanite_settings")
        s.enabled = False
        a.set_editor_property("nanite_settings", s)
        if mi:
            for slot, sm in enumerate(a.get_editor_property("static_materials")):
                is_sign = str(sm.get_editor_property("material_slot_name")).startswith("FoodSign_")
                a.set_material(slot, sign_mat if is_sign and sign_mat else mi)
        body = a.get_editor_property("body_setup")
        if body and step == "stalls":
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        elif body:
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
            body.set_editor_property("agg_geom", unreal.KAggregateGeom())
        eal.save_loaded_asset(a)
        n += 1
    eal.delete_directory(tmp)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for act in eas.get_all_level_actors():
        if act.get_name().startswith(("SM_Food", "SM_Stall")) or name in str(act.get_folder_path()):
            eas.destroy_actor(act)
    log("%s: %d meshes into %s" % (name, n, dest))


STEP = os.environ.get("FOOD_STEP", "textures")
if STEP == "textures":
    import_textures()
else:
    import_kit(STEP)
