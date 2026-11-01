"""The smoking kit into the project (docs/smoking.md): blender/props/smoking/export/smoking_kit.glb (one mesh object per asset,
build_smoking.py), the pack's printed card per language (textures/localized/<lang>/cig_pack.png, tools/smoking_pack.py) and the
little textures (textures/smoking/*.png) -> /Game/Smoking/SM_*, M_*, T_*, /Game/Smoking/Localized/<lang>/T_CigPack.
Run headless, one step per process (Interchange imports one scene per process; unreal/scripts/import_smoking.sh runs them in turn):

  SMOKING_STEP=textures|meshes UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/import_smoking.py

Materials: M_CigPack (the card, its Atlas swapped per language by ACigarettePack), M_CigFilter, M_CigPaper, M_CigTobacco, M_CigEmber
(Glow 0..1: ash grey to a bright ember), M_LighterBody, M_LighterMetal, M_LighterFlame (unlit, additive), M_Smoke (the puffs of
ALyudSmokeFx: lit translucent, opacity from per-instance custom data 0). Nanite off, no collision (the actors set none).
"""
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
DEST = "/Game/Smoking"
TEX = DEST + "/Textures"
GLB = os.path.join(REPO, "blender", "props", "smoking", "export", "smoking_kit.glb")
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty
LANGS = ("ru", "en", "ja")


def log(msg):
    unreal.log_warning("SMOKING " + msg)


def get_or_create(name, folder, cls, factory):
    path = "%s/%s" % (folder, name)
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    return tools.create_asset(name, folder, cls, factory)


def import_texture(src, folder, name, srgb=True, alpha=False):
    t = unreal.AssetImportTask()
    t.filename = src
    t.destination_path = folder
    t.destination_name = name
    t.automated = True
    t.replace_existing = True
    t.save = False
    tools.import_asset_tasks([t])
    a = eal.load_asset("%s/%s" % (folder, name))
    if a:
        a.set_editor_property("srgb", srgb)
        if alpha:
            a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        eal.save_loaded_asset(a)
    return a


def node(m, cls, x, y):
    return mel.create_material_expression(m, cls, x, y)


def const3(m, rgb, x, y):
    c = node(m, unreal.MaterialExpressionConstant3Vector, x, y)
    c.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1))
    return c


def const(m, v, x, y):
    c = node(m, unreal.MaterialExpressionConstant, x, y)
    c.set_editor_property("r", v)
    return c


def scalar(m, name, v, x, y):
    c = node(m, unreal.MaterialExpressionScalarParameter, x, y)
    c.set_editor_property("parameter_name", name)
    c.set_editor_property("default_value", v)
    return c


def tex(m, path, x, y, param=None, sampler=None):
    t = node(m, unreal.MaterialExpressionTextureSampleParameter2D if param else unreal.MaterialExpressionTextureSample, x, y)
    if param:
        t.set_editor_property("parameter_name", param)
    t.set_editor_property("texture", eal.load_asset(path))
    if sampler:
        t.set_editor_property("sampler_type", sampler)
    return t


def new_material(name):
    m = get_or_create(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(m)
    m.set_editor_property("used_with_static_lighting", False)
    return m


def done(m):
    mel.recompile_material(m)
    eal.save_loaded_asset(m)


def solid(name, rgb, rough, metal=0.0, spec=0.5, texture=None):
    m = new_material(name)
    if texture:
        t = tex(m, texture, -500, 0)
        mel.connect_material_property(t, "RGB", MP.MP_BASE_COLOR)
    else:
        mel.connect_material_property(const3(m, rgb, -400, 0), "", MP.MP_BASE_COLOR)
    mel.connect_material_property(const(m, rough, -400, 200), "", MP.MP_ROUGHNESS)
    mel.connect_material_property(const(m, metal, -400, 300), "", MP.MP_METALLIC)
    mel.connect_material_property(const(m, spec, -400, 400), "", MP.MP_SPECULAR)
    done(m)
    return m


def materials():
    # the card, its picture per language (ACigarettePack sets Atlas)
    m = new_material("M_CigPack")
    t = tex(m, DEST + "/Localized/ru/T_CigPack", -500, 0, "Atlas")
    mel.connect_material_property(t, "RGB", MP.MP_BASE_COLOR)
    mel.connect_material_property(const(m, 0.62, -400, 250), "", MP.MP_ROUGHNESS)
    mel.connect_material_property(const(m, 0.35, -400, 350), "", MP.MP_SPECULAR)
    done(m)
    solid("M_CigFilter", None, 0.7, texture=TEX + "/T_CigFilter")
    solid("M_CigPaper", (0.86, 0.85, 0.82), 0.55, spec=0.4)
    solid("M_CigTobacco", (0.22, 0.12, 0.05), 0.9, spec=0.2)
    solid("M_LighterBody", (0.92, 0.32, 0.02), 0.22, spec=0.6)
    solid("M_LighterMetal", (0.72, 0.72, 0.74), 0.32, metal=1.0)

    # the ember: ash grey charring to black where it burns, the cracks glowing with Glow
    m = new_material("M_CigEmber")
    t = tex(m, TEX + "/T_CigEmber", -900, 0)
    glow = scalar(m, "Glow", 0.0, -900, 300)
    strength = scalar(m, "EmberStrength", 60.0, -900, 400)
    ash, char = const3(m, (0.42, 0.40, 0.38), -600, -200), const3(m, (0.05, 0.045, 0.04), -600, -120)
    lerp = node(m, unreal.MaterialExpressionLinearInterpolate, -350, -150)
    mel.connect_material_expressions(ash, "", lerp, "A")
    mel.connect_material_expressions(char, "", lerp, "B")
    # unlit it is all ash; lit, the char shows under it
    charmask = node(m, unreal.MaterialExpressionMultiply, -600, 0)
    mel.connect_material_expressions(t, "R", charmask, "A")
    mel.connect_material_expressions(glow, "", charmask, "B")
    mel.connect_material_expressions(charmask, "", lerp, "Alpha")
    mel.connect_material_property(lerp, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(const(m, 0.95, -350, 100), "", MP.MP_ROUGHNESS)
    em = node(m, unreal.MaterialExpressionMultiply, -350, 250)
    hot = const3(m, (1.0, 0.22, 0.03), -600, 200)
    g2 = node(m, unreal.MaterialExpressionMultiply, -600, 320)
    mel.connect_material_expressions(t, "G", g2, "A")
    mel.connect_material_expressions(glow, "", g2, "B")
    g3 = node(m, unreal.MaterialExpressionMultiply, -480, 350)
    mel.connect_material_expressions(g2, "", g3, "A")
    mel.connect_material_expressions(glow, "", g3, "B")
    mel.connect_material_expressions(hot, "", em, "A")
    s = node(m, unreal.MaterialExpressionMultiply, -350, 400)
    mel.connect_material_expressions(g3, "", s, "A")
    mel.connect_material_expressions(strength, "", s, "B")
    mel.connect_material_expressions(s, "", em, "B")
    mel.connect_material_property(em, "", MP.MP_EMISSIVE_COLOR)
    done(m)

    # the flame: unlit additive, blue at its foot, yellow-white in the middle, fading at the top (V up its height)
    m = new_material("M_LighterFlame")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    uv = node(m, unreal.MaterialExpressionTextureCoordinate, -1000, 0)
    v = node(m, unreal.MaterialExpressionComponentMask, -850, 0)
    v.set_editor_property("g", True)
    v.set_editor_property("r", False)
    mel.connect_material_expressions(uv, "", v, "")
    hlsl = node(m, unreal.MaterialExpressionCustom, -600, 0)
    hlsl.set_editor_property("code", "float k = smoothstep(0.06, 0.35, V);\n"
                                     "float fade = 1 - smoothstep(0.55, 1.0, V);\n"
                                     "float3 c = lerp(float3(0.15, 0.35, 1.0) * 3, float3(1.0, 0.62, 0.22) * 14, k);\n"
                                     "return c * fade;")
    hlsl.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", "V")
    hlsl.set_editor_property("inputs", [ci])
    mel.connect_material_expressions(v, "", hlsl, "V")
    mel.connect_material_property(hlsl, "", MP.MP_EMISSIVE_COLOR)
    done(m)

    # smoke: a soft puff card, lit by the scene, opacity per puff (custom data 0) and soft where it meets things
    m = new_material("M_Smoke")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    t = tex(m, TEX + "/T_SmokePuff", -800, 0)
    cd = node(m, unreal.MaterialExpressionPerInstanceCustomData, -800, 250)
    cd.set_editor_property("data_index", 0)
    a = node(m, unreal.MaterialExpressionMultiply, -500, 100)
    mel.connect_material_expressions(t, "A", a, "A")
    mel.connect_material_expressions(cd, "", a, "B")
    fade = node(m, unreal.MaterialExpressionDepthFade, -500, 300)
    fade.set_editor_property("fade_distance_default", 6.0)
    mel.connect_material_expressions(a, "", fade, "Opacity")
    mel.connect_material_property(fade, "", MP.MP_OPACITY)
    mel.connect_material_property(const3(m, (0.72, 0.73, 0.75), -500, -150), "", MP.MP_BASE_COLOR)
    mel.connect_material_property(const(m, 1.0, -500, 450), "", MP.MP_ROUGHNESS)
    done(m)
    log("materials")


def import_textures():
    n = 0
    for lang in LANGS:
        src = os.path.join(REPO, "textures", "localized", lang, "cig_pack.png")
        if os.path.isfile(src) and import_texture(src, DEST + "/Localized/" + lang, "T_CigPack"):
            n += 1
    for f, name, srgb, alpha in (("cig_filter.png", "T_CigFilter", True, False), ("cig_ember.png", "T_CigEmber", False, False),
                                 ("smoke_puff.png", "T_SmokePuff", True, True)):
        if import_texture(os.path.join(REPO, "textures", "smoking", f), TEX, name, srgb, alpha):
            n += 1
    log("textures: %d" % n)
    materials()


SLOTS = {"Pack": "M_CigPack", "CigFilter": "M_CigFilter", "CigPaper": "M_CigPaper", "CigTobacco": "M_CigTobacco",
         "CigEmber": "M_CigEmber", "LighterBody": "M_LighterBody", "LighterMetal": "M_LighterMetal", "Flame": "M_LighterFlame"}


def import_meshes():
    if not os.path.isfile(GLB):
        log("no %s (run blender/props/smoking/build_smoking.py)" % GLB)
        return
    tmp = DEST + "/_import"
    if eal.does_directory_exist(tmp):
        eal.delete_directory(tmp)
    src = unreal.InterchangeManager.create_source_data(GLB)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_scene(tmp, src, params)
    mgr.wait_until_all_tasks_done(True)
    n = 0
    for path in eal.list_assets(tmp, True):
        a = eal.load_asset(path)
        if not isinstance(a, unreal.StaticMesh):
            continue
        name = a.get_name()
        target = "%s/%s" % (DEST, name)
        if eal.does_asset_exist(target):
            eal.delete_asset(target)
        if not eal.rename_asset(path, target):
            log("could not move %s" % path)
            continue
        a = eal.load_asset(target)
        s = a.get_editor_property("nanite_settings")
        s.enabled = False
        a.set_editor_property("nanite_settings", s)
        slots = a.get_editor_property("static_materials")
        for i, sm in enumerate(slots):
            slot = str(sm.get_editor_property("material_slot_name"))
            key = next((k for k in SLOTS if slot == k or slot.startswith(k)), None)
            mat = eal.load_asset("%s/%s" % (DEST, SLOTS[key])) if key else None
            if mat:
                a.set_material(i, mat)
            else:
                log("%s: no material for slot %s" % (name, slot))
        body = a.get_editor_property("body_setup")
        if body:
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
            body.set_editor_property("agg_geom", unreal.KAggregateGeom())
        eal.save_loaded_asset(a)
        b = a.get_bounding_box()
        log("%s %d slots, bounds %s .. %s" % (name, len(slots), b.min, b.max))
        n += 1
    eal.delete_directory(tmp)
    # Interchange also leaves its materials and any scene actors behind: only ours stay
    for path in eal.list_assets(DEST, False):
        a = eal.find_asset_data(path).get_asset()
        if isinstance(a, (unreal.MaterialInstanceConstant,)) and a.get_name().startswith(tuple(SLOTS)):
            eal.delete_asset(path)
    log("meshes: %d into %s" % (n, DEST))


STEP = os.environ.get("SMOKING_STEP", "textures")
if STEP == "textures":
    import_textures()
else:
    import_meshes()
