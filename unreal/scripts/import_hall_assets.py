"""Imports the bottle labels and the inventory icons, and builds the bottle materials:

  /Game/Hall/T_Label_<flavor>        textures/podyezd/label_<flavor>.jpg
  /Game/Hall/M_BottleLabel           Label (texture) x Color, glossy paper
  /Game/Hall/M_BottleGlass           translucent glass tinted by Color
  /Game/Hall/M_BottleLiquid          translucent drink tinted by Color
  /Game/Inventory/T_Icon_<item id>   textures/inventory/icon_<item id>.jpg  (UI textures, no mips)

Run with the editor closed:
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=unreal/scripts/import_hall_assets.py
"""
import glob
import os

import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))


def log(msg):
    unreal.log_warning("HALL " + msg)


def import_texture(src, folder, name, ui=False):
    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = folder
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    tools.import_asset_tasks([task])
    tex = eal.load_asset(folder + "/" + name)
    if tex is None:
        log("could not import %s" % src)
        return None
    if ui:
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tex.set_editor_property("never_stream", True)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    eal.save_loaded_asset(tex)
    return tex


for path in sorted(glob.glob(os.path.join(ROOT, "textures", "podyezd", "label_*.jpg"))):
    flavor = os.path.basename(path)[len("label_"):-4]
    import_texture(path, "/Game/Hall", "T_Label_" + flavor)
    log("label " + flavor)
for path in sorted(glob.glob(os.path.join(ROOT, "textures", "inventory", "icon_*.jpg"))):
    item = os.path.basename(path)[len("icon_"):-4]
    import_texture(path, "/Game/Inventory", "T_Icon_" + item, ui=True)
    log("icon " + item)


def new_material(path):
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    folder, name = path.rsplit("/", 1)
    return tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())


def node(m, cls, x, y, **props):
    e = mel.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def prop(m, expr, out, prop_):
    mel.connect_material_property(expr, out, prop_)


MP = unreal.MaterialProperty

# label: paper with a slight gloss
m = new_material("/Game/Hall/M_BottleLabel")
tex = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -600, 0, parameter_name="Label",
           texture=unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
tint = node(m, unreal.MaterialExpressionVectorParameter, -600, 250, parameter_name="Color", default_value=unreal.LinearColor(1, 1, 1, 1))
mul = node(m, unreal.MaterialExpressionMultiply, -300, 100)
mel.connect_material_expressions(tex, "RGB", mul, "A")
mel.connect_material_expressions(tint, "", mul, "B")
prop(m, mul, "", MP.MP_BASE_COLOR)
rough = node(m, unreal.MaterialExpressionConstant, -300, 300, r=0.5)
prop(m, rough, "", MP.MP_ROUGHNESS)
m.set_editor_property("two_sided", True)
mel.recompile_material(m)
eal.save_loaded_asset(m)

# glass and liquid: translucent, tinted by Color
for name, opacity, rough_v, spec in (("M_BottleGlass", 0.32, 0.06, 0.7), ("M_BottleLiquid", 0.78, 0.15, 0.5)):
    m = new_material("/Game/Hall/" + name)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("two_sided", True)
    col = node(m, unreal.MaterialExpressionVectorParameter, -500, 0, parameter_name="Color", default_value=unreal.LinearColor(0.1, 0.3, 0.15, 1))
    prop(m, col, "", MP.MP_BASE_COLOR)
    op = node(m, unreal.MaterialExpressionConstant, -500, 200, r=opacity)
    prop(m, op, "", MP.MP_OPACITY)
    ro = node(m, unreal.MaterialExpressionConstant, -500, 300, r=rough_v)
    prop(m, ro, "", MP.MP_ROUGHNESS)
    sp = node(m, unreal.MaterialExpressionConstant, -500, 400, r=spec)
    prop(m, sp, "", MP.MP_SPECULAR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
log("materials done")
