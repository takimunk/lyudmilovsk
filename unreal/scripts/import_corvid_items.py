"""Imports the corvid props built by blender/props/corvid_items.py (blender/props/export/corvid_items/) and their inventory icons.

  /Game/Birds/Items/M_CorvidItem                  one master: Albedo (alpha = metal mask when UseMask), Roughness, RoughnessMetal, Metal, UseMask; opaque,
                                                  two-sided (fins), used with instanced static meshes
  /Game/Birds/Items/T_CorvidItem_<Name>           albedo (256, Fish 512)
  /Game/Birds/Items/MI_CorvidItem_<Name>          instance with the item's roughness / metal
  /Game/Birds/Items/SM_CorvidItem_<Name>          static mesh, LOD0 + LOD1, nanite off, no collision (1 uu = 1 cm, origin at the centre of mass)
  /Game/Inventory/Localized/<lang>/T_Icon_find_<id>   textures/localized/<lang>/icon_find_<id>.jpg (UI textures, no mips) for every language folder

Everything goes through the legacy FBX / texture factories (Interchange imports only once per process here), so one process does it all:
  tools/wait_ue_queue.sh
  UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this file>            (all steps)
  ... -script="<this file> meshes"   or  "<this file> icons"
"""
import glob
import json
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
SRC = os.path.join(ROOT, "blender", "props", "export", "corvid_items")
ITEMS = "/Game/Birds/Items"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
LOD1_SCREEN = 0.06      # a few centimetres: LOD1 from ~1.5 m on


def log(msg):
    unreal.log_warning("CORVIDITEMS " + msg)


def import_texture(path, dest, name, ui=False):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    task.set_editor_property("factory", unreal.TextureFactory())
    tools.import_asset_tasks([task])
    tex = unreal.load_asset("%s/%s" % (dest, name))
    if tex is None:
        raise RuntimeError("texture import failed: " + path)
    if ui:
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tex.set_editor_property("never_stream", True)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    else:
        tex.set_editor_property("srgb", True)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    eal.save_loaded_asset(tex)
    return tex


def node(m, cls, x, y, **props):
    e = mel.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def make_master():
    path = ITEMS + "/M_CorvidItem"
    if eal.does_asset_exist(path):
        m = unreal.load_asset(path)
    else:
        m = tools.create_asset("M_CorvidItem", ITEMS, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(m)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    m.set_editor_property("used_with_static_lighting", False)
    tex = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -900, 0, parameter_name="Albedo",
               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
               texture=unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
    metal = node(m, unreal.MaterialExpressionScalarParameter, -900, 300, parameter_name="Metal", default_value=0.0)
    use_mask = node(m, unreal.MaterialExpressionScalarParameter, -900, 400, parameter_name="UseMask", default_value=0.0)
    rough = node(m, unreal.MaterialExpressionScalarParameter, -900, 520, parameter_name="Roughness", default_value=0.6)
    rough_m = node(m, unreal.MaterialExpressionScalarParameter, -900, 620, parameter_name="RoughnessMetal", default_value=0.4)
    mf = node(m, unreal.MaterialExpressionLinearInterpolate, -550, 330)          # metal factor: Metal, or the texture alpha when UseMask
    mel.connect_material_expressions(metal, "", mf, "A")
    mel.connect_material_expressions(tex, "A", mf, "B")
    mel.connect_material_expressions(use_mask, "", mf, "Alpha")
    rf = node(m, unreal.MaterialExpressionLinearInterpolate, -550, 560)
    mel.connect_material_expressions(rough, "", rf, "A")
    mel.connect_material_expressions(rough_m, "", rf, "B")
    mel.connect_material_expressions(mf, "", rf, "Alpha")
    mel.connect_material_property(tex, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(mf, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(rf, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log("master " + path)
    return m


def make_instance(name, master, tex, meta):
    path = "%s/MI_CorvidItem_%s" % (ITEMS, name)
    mi = unreal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(
        "MI_CorvidItem_%s" % name, ITEMS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", master)
    mel.set_material_instance_texture_parameter_value(mi, "Albedo", tex)
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", meta["roughness"])
    mel.set_material_instance_scalar_parameter_value(mi, "RoughnessMetal", meta["roughness_metal"])
    mel.set_material_instance_scalar_parameter_value(mi, "Metal", meta["metal"])
    mel.set_material_instance_scalar_parameter_value(mi, "UseMask", 1.0 if meta["mixed_metal_mask"] else 0.0)
    eal.save_loaded_asset(mi)
    return mi


def import_fbx(path, dest, name):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("automated_import_should_detect_type", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    d = ui.static_mesh_import_data
    d.set_editor_property("combine_meshes", True)
    d.set_editor_property("auto_generate_collision", False)
    d.set_editor_property("generate_lightmap_u_vs", False)
    d.set_editor_property("build_nanite", False)
    d.set_editor_property("remove_degenerates", False)
    d.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    d.set_editor_property("import_uniform_scale", 1.0)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", False)
    task.set_editor_property("factory", unreal.FbxFactory())
    task.set_editor_property("options", ui)
    tools.import_asset_tasks([task])
    sm = unreal.load_asset("%s/%s" % (dest, name))
    if not isinstance(sm, unreal.StaticMesh):
        raise RuntimeError("no static mesh from " + path)
    return sm


def probe_once(sub):
    names = [n for n in dir(sub) if "lod" in n.lower() or "collision" in n.lower()]
    log("StaticMeshEditorSubsystem: " + ", ".join(sorted(names)))


def do_meshes():
    meta = json.load(open(os.path.join(SRC, "meta.json")))
    if not eal.does_directory_exist(ITEMS):
        eal.make_directory(ITEMS)
    master = make_master()
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if sub is None:   # not available in the commandlet: the (deprecated) library has the same calls
        sub = unreal.EditorStaticMeshLibrary
    probe_once(sub)
    for name, mt in sorted(meta.items()):
        tex = import_texture(os.path.join(SRC, "T_%s.png" % name), ITEMS, "T_CorvidItem_%s" % name)
        mi = make_instance(name, master, tex, mt)
        mi1 = None
        if mt.get("lod1_texture"):   # thin revolved shapes (Ring, Button, Key) have a coarser LOD1 with its own small texture
            tex1 = import_texture(os.path.join(SRC, mt["lod1_texture"]), ITEMS, "T_CorvidItem_%s_LOD1" % name)
            mi1 = make_instance(name + "_LOD1", master, tex1, mt)
        sm = import_fbx(os.path.join(SRC, name + ".fbx"), ITEMS, "SM_CorvidItem_%s" % name)
        # LOD1: the second FBX as a throwaway mesh, copied into LOD1 (import_lod fails on a freshly imported mesh here)
        has_lod1_tex = bool(mt.get("lod1_texture"))   # Ring, Button, Key: LOD1 has other UVs than the LOD0 texture; one slot per mesh here, so they ship LOD0 only
        tmp = None if has_lod1_tex else import_fbx(os.path.join(SRC, name + "_LOD1.fbx"), ITEMS + "/_tmp", "SM_tmp_%s" % name)
        lod_ok = False
        try:
            if has_lod1_tex:
                sm.build_from_static_mesh_descriptions([sm.get_static_mesh_description(0)], False)   # drops a LOD1 left from an earlier import
                raise RuntimeError('LOD0 only (own LOD1 texture not supported through one material slot)')
            # the editor subsystems are not available in the commandlet: rebuild the mesh from the two descriptions
            d0 = sm.get_static_mesh_description(0)
            d1 = tmp.get_static_mesh_description(0)
            sm.build_from_static_mesh_descriptions([d0, d1], False)
            lod_ok = sm.get_num_lods() == 2
        except Exception as e:
            log("%s: LOD1 build failed: %s" % (name, e))
        if tmp is not None:
            eal.delete_asset(tmp.get_path_name())
        # material, nanite, collision
        mats = sm.get_editor_property("static_materials")
        for i in range(len(mats)):
            m = mats[i]
            m.material_interface = mi
            mats[i] = m
        sm.set_editor_property("static_materials", mats)
        if mi1 is not None:
            log("%s: LOD1 has its own texture; material slots %d" % (name, len(mats)))
            try:
                # the LOD1 FBX brought the material M_<Name>_LOD1 as a second slot; give it the LOD1 instance
                if len(mats) > 1:
                    sm.set_material(1, mi1)
                    sm.set_material(0, mi)
                else:
                    log("%s: no second slot for the LOD1 texture: LOD1 will use the LOD0 texture" % name)
            except Exception as e:
                log("%s: set_material: %s" % (name, e))
        ns = sm.get_editor_property("nanite_settings")
        ns.set_editor_property("enabled", False)
        sm.set_editor_property("nanite_settings", ns)
        try:
            sub.remove_collisions(sm)
        except Exception as e:
            log("%s: remove_collisions: %s" % (name, e))
        try:
            bs = sm.get_editor_property("body_setup")
            if bs:
                bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
        except Exception as e:
            log("%s: collision profile: %s" % (name, e))
        # LOD0 full size up close, LOD1 from ~1.5 m
        try:
            if hasattr(sub, "set_lod_screen_sizes"):
                sub.set_lod_screen_sizes(sm, [1.0, LOD1_SCREEN])
        except Exception as e:
            log("%s: set_lod_screen_sizes: %s" % (name, e))
        try:
            sms = sm.get_editor_property("source_models")
            if len(sms) > 1:
                for idx, val in ((0, 1.0), (1, LOD1_SCREEN)):
                    sd = sms[idx].get_editor_property("screen_size")
                    sd.set_editor_property("default", val)
                    sms[idx].set_editor_property("screen_size", sd)
                sm.set_editor_property("source_models", sms)
        except Exception as e:
            log("%s: screen sizes: %s" % (name, e))
        eal.save_loaded_asset(sm)
        b = sm.get_bounds()
        log("%s: LODs %d, verts LOD0 %d, bounds %.2f x %.2f x %.2f cm, nanite %s, material %s, lod import %s" % (
            name, sm.get_num_lods(), sm.get_num_triangles(0) if hasattr(sm, 'get_num_triangles') else -1, b.box_extent.x * 2, b.box_extent.y * 2, b.box_extent.z * 2,
            sm.get_editor_property("nanite_settings").get_editor_property("enabled"), sm.get_editor_property("static_materials")[0].material_interface.get_name(),
            lod_ok))


def do_icons():
    n = 0
    for lang_dir in sorted(glob.glob(os.path.join(ROOT, "textures", "localized", "*"))):
        lang = os.path.basename(lang_dir)
        for path in sorted(glob.glob(os.path.join(lang_dir, "icon_find_*.jpg"))):
            item = os.path.basename(path)[len("icon_"):-4]
            import_texture(path, "/Game/Inventory/Localized/" + lang, "T_Icon_" + item, ui=True)
            n += 1
            log("icon %s %s" % (lang, item))
    log("%d icons" % n)


steps = sys.argv[1:] or ["meshes", "icons"]
# the pythonscript commandlet passes the whole "<file> args" string as the script; arguments arrive in sys.argv[1:] here
if "meshes" in steps:
    do_meshes()
if "icons" in steps:
    do_icons()
log("done")
