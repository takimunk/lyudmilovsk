"""Imports the cable-car metro's meshes and makes its few materials (docs/transport.md).

  design/transport/game/transport_kit.glb (blender/transport/optimize.py + pack.py from the Hi3D models)
      -> /Game/Transport/Meshes/SM_TR_* (no Nanite: Interchange's glTF parent lacks the usage flag)
  M_TR_Paint (Color, Roughness, Metallic), M_TR_Glass (translucent Color, Opacity), M_TR_Glow (emissive Color)
      -> /Game/Transport/Materials

Nothing is placed in a level: ULyudTransportSubsystem builds the network at play from Content/Transport/*.json.
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=unreal/scripts/import_transport.py
"""
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
KIT = os.path.join(REPO, "design", "transport", "game", "transport_kit.glb")
MESHES = "/Game/Transport/Meshes"
MATS = "/Game/Transport/Materials"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log_warning("TRANSPORT " + msg)


def material(name, blend, build):
    path = MATS + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = tools.create_asset(name, MATS, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", blend)
    build(m)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def vparam(m, name, rgba, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", unreal.LinearColor(*rgba))
    return e


def sparam(m, name, v, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", v)
    return e


def paint(m):
    mel.connect_material_property(vparam(m, "Color", (0.8, 0.76, 0.66, 1), -500, 0), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(sparam(m, "Roughness", 0.55, -500, 200), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(sparam(m, "Metallic", 0.0, -500, 300), "", unreal.MaterialProperty.MP_METALLIC)
    m.set_editor_property("used_with_instanced_static_meshes", True)


def glass(m):
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    mel.connect_material_property(vparam(m, "Color", (0.55, 0.66, 0.68, 1), -500, 0), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(sparam(m, "Opacity", 0.22, -500, 200), "", unreal.MaterialProperty.MP_OPACITY)
    mel.connect_material_property(sparam(m, "Roughness", 0.04, -500, 300), "", unreal.MaterialProperty.MP_ROUGHNESS)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    m.set_editor_property("two_sided", True)


def glow(m):
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    c = vparam(m, "Color", (1.0, 0.7, 0.35, 1), -600, 0)
    k = sparam(m, "Intensity", 6.0, -600, 200)
    mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 100)
    mel.connect_material_expressions(c, "", mul, "A")
    mel.connect_material_expressions(k, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    m.set_editor_property("used_with_instanced_static_meshes", True)


TEXTURES = os.path.join(REPO, "design", "transport", "textures")
TEX_DIR = "/Game/Transport/Textures"


def surface(m):
    """M_TR_Surface: the stations' textured surfaces (tools/transport_textures.py). The meshes' UVs are metres; the
    texture repeats every Tile metres. BaseTex * Tint, NormalTex, RoughTex * RoughMul, Metallic."""
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -1200, 0)
    tile = sparam(m, "Tile", 1.0, -1200, 150)
    div = mel.create_material_expression(m, unreal.MaterialExpressionDivide, -1000, 50)
    mel.connect_material_expressions(uv, "", div, "A")
    mel.connect_material_expressions(tile, "", div, "B")

    def tex(name, y, sampler):
        t = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -800, y)
        t.set_editor_property("parameter_name", name)
        # a default of the sampler's kind (the engine's default texture is a colour one: the material would not compile)
        kind = {"BaseTex": "C", "NormalTex": "N", "RoughTex": "R"}[name]
        default = eal.load_asset(TEX_DIR + "/T_TR_concrete_" + kind)
        if default:
            t.set_editor_property("texture", default)
        t.set_editor_property("sampler_type", sampler)
        mel.connect_material_expressions(div, "", t, "UVs")
        return t
    base = tex("BaseTex", -200, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    nrm = tex("NormalTex", 100, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    rgh = tex("RoughTex", 400, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    tint = vparam(m, "Tint", (1, 1, 1, 1), -800, -400)
    mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -400, -300)
    mel.connect_material_expressions(base, "RGB", mul, "A")
    mel.connect_material_expressions(tint, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    rm = sparam(m, "RoughMul", 1.0, -600, 600)
    rmul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -400, 450)
    mel.connect_material_expressions(rgh, "R", rmul, "A")
    mel.connect_material_expressions(rm, "", rmul, "B")
    mel.connect_material_property(rmul, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(sparam(m, "Metallic", 0.0, -400, 650), "", unreal.MaterialProperty.MP_METALLIC)
    m.set_editor_property("used_with_instanced_static_meshes", True)


def import_textures():
    """design/transport/textures/T_TR_*_{C,N,R}.png -> /Game/Transport/Textures (normal maps as normal maps, green
    flipped: the generator writes OpenGL's +Y), then M_TR_Surface over them."""
    if not os.path.isdir(TEXTURES):
        log("no %s: keeping the committed textures" % TEXTURES)
        return 0
    tasks = []
    names = sorted(f for f in os.listdir(TEXTURES) if f.startswith("T_TR_") and f.endswith(".png"))
    for f in names:
        t = unreal.AssetImportTask()
        t.filename = os.path.join(TEXTURES, f)
        t.destination_path = TEX_DIR
        t.automated = True
        t.replace_existing = True
        t.save = False
        tasks.append(t)
    tools.import_asset_tasks(tasks)
    for f in names:
        a = eal.load_asset(TEX_DIR + "/" + f[:-4])
        if not a:
            continue
        kind = f[-5]
        if kind == "N":
            a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            a.set_editor_property("srgb", False)
            a.set_editor_property("flip_green_channel", True)
        elif kind == "R":
            a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
            a.set_editor_property("srgb", False)
        eal.save_loaded_asset(a)
    material("M_TR_Surface", unreal.BlendMode.BLEND_OPAQUE, surface)
    return len(names)


def import_kit():
    if not os.path.isfile(KIT):
        # the meshes are committed (LFS); the .glb is only where they were made (blender/transport/optimize.py, pack.py)
        log("no %s: keeping the committed meshes" % KIT)
        return 0
    if eal.does_directory_exist(MESHES):
        eal.delete_directory(MESHES)
    src = unreal.InterchangeManager.create_source_data(KIT)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_scene(MESHES, src, params)
    mgr.wait_until_all_tasks_done(True)
    n = 0
    for path in eal.list_assets(MESHES, True):
        a = eal.load_asset(path)
        if isinstance(a, unreal.StaticMesh):
            s = a.get_editor_property("nanite_settings")
            s.enabled = False
            a.set_editor_property("nanite_settings", s)
            body = a.get_editor_property("body_setup")
            if body:
                # the vestibules and their towers are walked into and on: their own triangles collide (doors, rooms, decks, railings)
                body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
                                         if a.get_name().startswith(("SM_TR_Vestibule_", "SM_TR_Tower_")) else unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
            log("mesh %s: %d triangles" % (a.get_name(), a.get_num_triangles(0)))
            n += 1
    # Hi3D's PBR maps read as metal on these painted surfaces (everything came out near black): paint, not metal
    for path in eal.list_assets(MESHES, True):
        a = eal.load_asset(path)
        if isinstance(a, unreal.MaterialInstanceConstant):
            done = []
            for pname in mel.get_scalar_parameter_names(a):
                pname = str(pname)
                if "etal" in pname:
                    mel.set_material_instance_scalar_parameter_value(a, pname, 0.0)
                    done.append(pname + "=0")
                elif "oughness" in pname and "Texture" not in pname:
                    mel.set_material_instance_scalar_parameter_value(a, pname, 0.6)
                    done.append(pname + "=0.6")
            mel.update_material_instance(a)
            log("material %s (%s): %s" % (a.get_name(), a.get_editor_property("parent").get_name() if a.get_editor_property("parent") else "-",
                                          ", ".join(done) or "no metal / roughness parameters"))
    eal.save_directory(MESHES)
    # the import also spawned actors in the editor's scratch level: never saved
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for act in eas.get_all_level_actors():
        if act.get_name().startswith("SM_TR_") or act.get_folder_path() and "transport_kit" in str(act.get_folder_path()):
            eas.destroy_actor(act)
    return n


def main():
    if not os.path.isfile(KIT) and eal.does_directory_exist(MATS):
        # a checkout without the generated kit: the committed meshes and materials stay as they are (the textures are
        # committed sources, so they and M_TR_Surface are made anyway)
        log("no %s: the committed meshes and materials stay" % KIT)
        log("textures: %d" % import_textures())
        return
    for name, blend, build in (("M_TR_Paint", unreal.BlendMode.BLEND_OPAQUE, paint),
                               ("M_TR_Glass", unreal.BlendMode.BLEND_TRANSLUCENT, glass),
                               ("M_TR_Glow", unreal.BlendMode.BLEND_OPAQUE, glow)):
        material(name, blend, build)
        log("material " + name)
    log("textures: %d" % import_textures())
    log("meshes: %d" % import_kit())


if __name__ == "__main__":
    main()
    log("done")
