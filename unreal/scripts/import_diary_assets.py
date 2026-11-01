"""Imports the diary's pictures and handwriting and builds its page material (docs/diary.md):

  /Game/UI/Diary/T_PaperLeft, T_PaperRight      textures/ui/diary_paper_left|right.png  (tools/ui_diary_paper.py)
  /Game/UI/Diary/T_MapInk                       textures/ui/map_ink.png                 (tools/ui_map_ink.py)
  /Game/Inventory/Localized/<lang>/T_Sketch_<id> textures/localized/<lang>/sketch_<id>.png (tools/ui_diary_icons.py)
  /Game/UI/Diary/Birds/T_Bird_<Species>[_f]     textures/ui/birds/bird_<Species>[_f].png (tools/ui_diary_birds.py)
  /Game/UI/Diary/Fonts/FF_Caveat_Regular, FF_Caveat_Bold, FF_Yomogi_Regular   fonts/*.ttf (OFL)
  /Game/UI/Diary/M_DiaryPage                    the 3D book's open pages: unlit, the screen's picture of them (Page, a render
                                                target with gamma-encoded colour) or plain paper (HasPage 0), times Bright,
                                                undoing the eye's exposure so they look as the screen draws them

Run with the editor closed:
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=unreal/scripts/import_diary_assets.py
"""
import glob
import os

import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
DIARY = "/Game/UI/Diary"


def log(msg):
    unreal.log_warning("DIARY " + msg)


def import_file(src, folder, name):
    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = folder
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    tools.import_asset_tasks([task])
    asset = eal.load_asset(folder + "/" + name)
    if asset is None:
        log("could not import %s" % src)
    return asset


def import_texture(src, folder, name, compress):
    tex = import_file(src, folder, name)
    if tex is None:
        return
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("never_stream", True)
    tex.set_editor_property("srgb", True)
    # the paper and the map are big: block compressed (BC7 keeps the ink's alpha); the sketches stay exact
    tex.set_editor_property("compression_settings",
                            unreal.TextureCompressionSettings.TC_BC7 if compress else unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    eal.save_loaded_asset(tex)
    log("%s/%s %dx%d" % (folder, name, tex.blueprint_get_size_x(), tex.blueprint_get_size_y()))


import_texture(os.path.join(ROOT, "textures", "ui", "diary_paper_left.png"), DIARY, "T_PaperLeft", True)
import_texture(os.path.join(ROOT, "textures", "ui", "diary_paper_right.png"), DIARY, "T_PaperRight", True)
import_texture(os.path.join(ROOT, "textures", "ui", "map_ink.png"), DIARY, "T_MapInk", True)
for lang_dir in sorted(glob.glob(os.path.join(ROOT, "textures", "localized", "*"))):
    lang = os.path.basename(lang_dir)
    for path in sorted(glob.glob(os.path.join(lang_dir, "sketch_*.png"))):
        item = os.path.basename(path)[len("sketch_"):-4]
        import_texture(path, "/Game/Inventory/Localized/" + lang, "T_Sketch_" + item, False)
for path in sorted(glob.glob(os.path.join(ROOT, "textures", "ui", "birds", "bird_*.png"))):
    name = os.path.basename(path)[len("bird_"):-4]
    import_texture(path, DIARY + "/Birds", "T_Bird_" + name, True)

# the handwriting: font faces, made in code (unreal.LyudFontTools: the .ttf factory asserts in a commandlet); the game
# puts them together into one composite font (LyudDiaryStyle.cpp)
for src, name in (("Caveat-Regular.ttf", "FF_Caveat_Regular"), ("Caveat-Bold.ttf", "FF_Caveat_Bold"),
                  ("Yomogi-Regular.ttf", "FF_Yomogi_Regular")):
    face = unreal.LyudFontTools.make_font_face(os.path.join(ROOT, "fonts", src), DIARY + "/Fonts/" + name)
    if face is None:
        log("could not import font %s" % src)
        continue
    eal.save_loaded_asset(face, False)
    log("font %s (%s)" % (name, face.get_class().get_name()))


# the open pages of the 3D book
def node(m, cls, x, y, **props):
    e = mel.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


path = DIARY + "/M_DiaryPage"
if eal.does_asset_exist(path):
    eal.delete_asset(path)
m = tools.create_asset("M_DiaryPage", DIARY, unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property("two_sided", False)
white = eal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
page = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -900, -100, parameter_name="Page",
            sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
if white:
    page.set_editor_property("texture", white)
gamma = node(m, unreal.MaterialExpressionConstant, -700, 60, r=2.2)
power = node(m, unreal.MaterialExpressionPower, -500, -60)
mel.connect_material_expressions(page, "RGB", power, "Base")
mel.connect_material_expressions(gamma, "", power, "Exponent")
paper = node(m, unreal.MaterialExpressionVectorParameter, -500, 120, parameter_name="Paper",
             default_value=unreal.LinearColor(0.86, 0.82, 0.72, 1.0))
has = node(m, unreal.MaterialExpressionScalarParameter, -500, 280, parameter_name="HasPage", default_value=0.0)
lerp = node(m, unreal.MaterialExpressionLinearInterpolate, -300, 0)
mel.connect_material_expressions(paper, "", lerp, "A")
mel.connect_material_expressions(power, "", lerp, "B")
mel.connect_material_expressions(has, "", lerp, "Alpha")
bright = node(m, unreal.MaterialExpressionScalarParameter, -300, 200, parameter_name="Bright", default_value=1.0)
mul = node(m, unreal.MaterialExpressionMultiply, -120, 40)
mel.connect_material_expressions(lerp, "", mul, "A")
mel.connect_material_expressions(bright, "", mul, "B")
eye = node(m, unreal.MaterialExpressionEyeAdaptationInverse, -120, 220)
mul2 = node(m, unreal.MaterialExpressionMultiply, 80, 80)
mel.connect_material_expressions(mul, "", mul2, "A")
mel.connect_material_expressions(eye, "", mul2, "B")
mel.connect_material_property(mul2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(m)
eal.save_loaded_asset(m)
log("M_DiaryPage made")
