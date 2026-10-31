"""The bicycle's parts into the project (docs/bikes.md): design/bikes/game/bike_kit.glb (blender/bikes/build_bike.py)
-> /Game/Bikes/Meshes/SM_Bike_* (and SM_BikeRack), their materials as instances of one base material /Game/Bikes/M_Bike
(colour, metal, roughness; the frame's paint is recoloured per bike in the game), the chain's scrolling links
/Game/Bikes/M_BikeChain. Run headless (unreal/scripts/import_bikes.sh):

  UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/import_bikes.py

Nanite off (glTF's Interchange parent lacks the usage flag: garbled fallbacks otherwise), no collision (ALyudBike has its
own box for the view ray), LODs generated here (the parts are thin: spokes and the chain go first).
"""
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
KIT = os.path.join(REPO, "design", "bikes", "game", "bike_kit.glb")
ROOTDIR = "/Game/Bikes"
MESHES = ROOTDIR + "/Meshes"
DEST = MESHES + "/bike_kit"
MATS = ROOTDIR + "/Materials"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log_warning("BIKES " + msg)


def base_material():
    """M_Bike: BaseColor (vector), Metallic, Roughness (scalars); opaque, lit, used with static meshes."""
    path = ROOTDIR + "/M_Bike"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = tools.create_asset("M_Bike", ROOTDIR, unreal.Material, unreal.MaterialFactoryNew())
    col = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -400, -100)
    col.set_editor_property("parameter_name", "BaseColor")
    col.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
    met = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -400, 100)
    met.set_editor_property("parameter_name", "Metallic")
    rough = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -400, 250)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.5)
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(met, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def chain_material():
    """M_BikeChain: the chain's links drawn on its two bands (U = link pitches along the loop, V across a band), running
    round as the scalar Scroll grows (ALyudBike sets it from the cranks); masked between the plates."""
    path = ROOTDIR + "/M_BikeChain"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = tools.create_asset("M_BikeChain", ROOTDIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("two_sided", True)
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    scroll = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, 200)
    scroll.set_editor_property("parameter_name", "Scroll")

    def custom(code, y, out_type):
        c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -600, y)
        c.set_editor_property("code", code)
        c.set_editor_property("output_type", out_type)
        ins = []
        for n in ("UV", "Scroll"):
            ci = unreal.CustomInput()
            ci.set_editor_property("input_name", n)
            ins.append(ci)
        c.set_editor_property("inputs", ins)
        mel.connect_material_expressions(uv, "", c, "UV")
        mel.connect_material_expressions(scroll, "", c, "Scroll")
        return c
    # a link: two round ends (the rollers) and a waisted plate between them
    mask = custom("float f = frac(UV.x + Scroll); float v = abs(UV.y - 0.5);\n"
                  "float w = 0.5 - 0.16 * pow(sin(3.14159 * f), 2.0);\n"
                  "return v < w ? 1.0 : 0.0;", 0, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    shade = custom("float f = frac(UV.x + Scroll); float r = 1.0 - saturate(min(f, 1.0 - f) / 0.2);\n"
                   "float v = abs(UV.y - 0.5) * 2.0;\n"
                   "return lerp(float3(0.07, 0.07, 0.075), float3(0.42, 0.41, 0.4), r) * (1.0 - 0.35 * v * v);", 250,
                   unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    mel.connect_material_property(mask, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    mel.connect_material_property(shade, "", unreal.MaterialProperty.MP_BASE_COLOR)
    met = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 400)
    met.set_editor_property("r", 1.0)
    rough = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 500)
    rough.set_editor_property("r", 0.42)
    mel.connect_material_property(met, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def gltf_values(mi):
    """Base colour, metallic, roughness of an Interchange glTF material instance (its parameter names vary a little)."""
    col, met, rough = unreal.LinearColor(0.5, 0.5, 0.5, 1.0), 0.0, 0.5
    for p in mel.get_vector_parameter_names(mi):
        if "olor" in str(p) and "missive" not in str(p):
            col = mel.get_material_instance_vector_parameter_value(mi, p)
    for p in mel.get_scalar_parameter_names(mi):
        n = str(p)
        if "etal" in n:
            met = mel.get_material_instance_scalar_parameter_value(mi, p)
        elif "ough" in n:
            rough = mel.get_material_instance_scalar_parameter_value(mi, p)
    return col, met, rough


def main():
    if not os.path.isfile(KIT):
        log("no %s (blender/bikes/build_bike.py): nothing to import" % KIT)
        return
    base = base_material()
    chain = chain_material()
    for d in (DEST, MATS):
        if eal.does_directory_exist(d):
            eal.delete_directory(d)
    for p in eal.list_assets(MESHES, False):
        eal.delete_asset(p.split(".")[0])
    src = unreal.InterchangeManager.create_source_data(KIT)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_scene(MESHES, src, params)
    mgr.wait_until_all_tasks_done(True)
    made = {}
    meshes = []
    for path in eal.list_assets(DEST, True):
        a = eal.load_asset(path)
        if isinstance(a, unreal.StaticMesh):
            meshes.append(a)
    log("imported %d meshes" % len(meshes))
    for a in meshes:
        name = a.get_name()
        s = a.get_editor_property("nanite_settings")
        s.enabled = False
        a.set_editor_property("nanite_settings", s)
        # the materials: one instance of M_Bike per glTF material (the chain's own scrolling one)
        for i, sm in enumerate(a.get_editor_property("static_materials")):
            slot = str(sm.get_editor_property("material_slot_name"))
            mi = sm.get_editor_property("material_interface")
            key = slot.split(".")[0]
            if key not in made:
                if "Chain" in key:
                    made[key] = chain
                else:
                    col, met, rough = gltf_values(mi) if mi else (unreal.LinearColor(0.5, 0.5, 0.5, 1.0), 0.0, 0.5)
                    inst = tools.create_asset("MI_" + key, MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
                    mel.set_material_instance_parent(inst, base)
                    mel.set_material_instance_vector_parameter_value(inst, "BaseColor", col)
                    mel.set_material_instance_scalar_parameter_value(inst, "Metallic", met)
                    mel.set_material_instance_scalar_parameter_value(inst, "Roughness", rough)
                    mel.update_material_instance(inst)
                    eal.save_loaded_asset(inst)
                    made[key] = inst
                    log("material %s: colour %.2f %.2f %.2f metal %.2f rough %.2f" % (key, col.r, col.g, col.b, met, rough))
            a.set_material(i, made[key])
        # lower LODs for the distance (the whole bike is ~2 m: small on screen soon)
        opts = unreal.EditorScriptingMeshReductionOptions()
        opts.set_editor_property("auto_compute_lod_screen_size", False)
        opts.set_editor_property("reduction_settings", [
            unreal.EditorScriptingMeshReductionSettings(percent_triangles=1.0, screen_size=1.0),
            unreal.EditorScriptingMeshReductionSettings(percent_triangles=0.4, screen_size=0.25),
            unreal.EditorScriptingMeshReductionSettings(percent_triangles=0.15, screen_size=0.1),
            unreal.EditorScriptingMeshReductionSettings(percent_triangles=0.05, screen_size=0.04)])
        n = unreal.EditorStaticMeshLibrary.set_lods(a, opts) if name != "SM_Bike_Chain" else 1
        tris = [a.get_num_triangles(l) for l in range(a.get_num_lods())]
        eal.save_loaded_asset(a)
        target = MESHES + "/" + name
        if a.get_path_name().split(".")[0] != target:
            eal.rename_asset(a.get_path_name().split(".")[0], target)
        log("mesh %s: lods %d, triangles %s" % (name, n, tris))
    eal.save_directory(ROOTDIR)
    # Interchange's own material instances are no longer used
    if eal.does_directory_exist(DEST):
        for p in eal.list_assets(DEST, True):
            eal.delete_asset(p.split(".")[0])
        eal.delete_directory(DEST)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for act in eas.get_all_level_actors():
        if act.get_name().startswith("SM_Bike") or "bike_kit" in str(act.get_folder_path()):
            eas.destroy_actor(act)
    log("done: %s" % ", ".join(sorted(m.get_name() for m in meshes)))


main()
