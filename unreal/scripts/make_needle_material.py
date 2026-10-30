"""Builds /Game/Plants/M_PlantNeedle: M_PlantLeaf, masked by the alpha of LeafDetail (a card of painted needles).

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import winter_snow  # noqa: E402

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty


def new_material(path):
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    folder, name = path.rsplit("/", 1)
    return tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())


def node(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError("could not connect %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def custom_input(name):
    c = unreal.CustomInput()
    c.set_editor_property("input_name", name)
    return c


import tree_wind  # noqa: E402


def wind(m, weight, x, y):
    """The tree wind (tree_wind.py) as a world position offset node, scaled by the expression weight."""
    return tree_wind.wind(m, weight, x, y, node, link, scalar_param)


def scalar_param(m, name, x, y, v):
    return node(m, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=v)


# --- leaves
m = new_material("/Game/Plants/M_PlantNeedle")
m.set_editor_property("two_sided", True)
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
m.set_editor_property("opacity_mask_clip_value", 0.5)
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
color = node(m, unreal.MaterialExpressionVectorParameter, -900, -300, parameter_name="Color", default_value=unreal.LinearColor(0.1, 0.2, 0.04, 1))
detail = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -900, 0, parameter_name="LeafDetail",
              texture=unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
leaf = node(m, unreal.MaterialExpressionMultiply, -600, -200)
link(color, "", leaf, "A")
link(detail, "RGB", leaf, "B")
# each leaf cluster a little lighter or darker: 0.8 + 0.4 * PerInstanceRandom
rnd = node(m, unreal.MaterialExpressionPerInstanceRandom, -900, 300)
shade = node(m, unreal.MaterialExpressionMultiply, -700, 300, const_b=0.4)
link(rnd, "", shade, "A")
shade1 = node(m, unreal.MaterialExpressionAdd, -550, 300, const_b=0.8)
link(shade, "", shade1, "A")
base = node(m, unreal.MaterialExpressionMultiply, -350, -100)
link(leaf, "", base, "A")
link(shade1, "", base, "B")
# snow on the branches (MPC_LyudWinter, ULyudWinterSubsystem: fresh snow sticks, wind and thaw shake it off)
mel.connect_material_property(winter_snow.leaf_snow(m, base), "", MP.MP_BASE_COLOR)
# the alpha is anti-aliased coverage: a blue-noise dither (the temporal AA resolves it) turns it into a soft edge
# instead of a hard stepped cut: mask = alpha - noise + 0.5, clipped at 0.5
noise = node(m, unreal.MaterialExpressionScalarBlueNoise, -700, 520)
diff = node(m, unreal.MaterialExpressionSubtract, -520, 500)
link(detail, "A", diff, "A")
link(noise, "", diff, "B")
dmask = node(m, unreal.MaterialExpressionAdd, -380, 500, const_b=0.5)
link(diff, "", dmask, "A")
mel.connect_material_property(dmask, "", MP.MP_OPACITY_MASK)
# light through the blade: the leaf's own colour, a little deeper
sss = node(m, unreal.MaterialExpressionMultiply, -350, 150, const_b=0.7)
link(leaf, "", sss, "A")
mel.connect_material_property(sss, "", MP.MP_SUBSURFACE_COLOR)
rough = node(m, unreal.MaterialExpressionConstant, -300, 400, r=0.55)
mel.connect_material_property(rough, "", MP.MP_ROUGHNESS)
# swinging with the twig (per-instance data 0, set by the tree) and a little flutter of its own
leaf_amp = node(m, unreal.MaterialExpressionScalarParameter, -1500, 800, parameter_name="SwayAmp", default_value=0.0)
leaf_w = node(m, unreal.MaterialExpressionPerInstanceCustomData, -1300, 700, data_index=0)
zero = node(m, unreal.MaterialExpressionConstant, -1500, 700, r=0.0)
link(zero, "", leaf_w, "DefaultValue")
# a baked tree has no per-instance sway weight: WeightMin gives all its leaves one
leaf_w2 = node(m, unreal.MaterialExpressionMax, -1200, 700)
link(leaf_w, "", leaf_w2, "A")
link(node(m, unreal.MaterialExpressionScalarParameter, -1400, 600, parameter_name="WeightMin", default_value=0.0), "", leaf_w2, "B")
leaf_sw = node(m, unreal.MaterialExpressionMultiply, -1100, 750)
link(leaf_w2, "", leaf_sw, "A")
link(leaf_amp, "", leaf_sw, "B")
swing = wind(m, leaf_sw, -800, 750)
FLUTTER_HLSL = tree_wind.FLUTTER_HLSL
flut = node(m, unreal.MaterialExpressionCustom, -800, 1000, code=FLUTTER_HLSL, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
            description="Flutter", inputs=[custom_input(n) for n in ("T", "R", "F", "Wd")])
link(node(m, unreal.MaterialExpressionTime, -1100, 1000), "", flut, "T")
link(node(m, unreal.MaterialExpressionPerInstanceRandom, -1100, 1100), "", flut, "R")
link(node(m, unreal.MaterialExpressionScalarParameter, -1100, 1200, parameter_name="Flutter", default_value=0.0), "", flut, "F")
link(node(m, unreal.MaterialExpressionCollectionParameter, -1100, 1300, collection=unreal.load_asset("/Game/Weather/MPC_LyudWeather"), parameter_name="Wind"), "", flut, "Wd")
wpo = node(m, unreal.MaterialExpressionAdd, -500, 850)
link(swing, "", wpo, "A")
link(flut, "", wpo, "B")
mel.connect_material_property(wpo, "", MP.MP_WORLD_POSITION_OFFSET)
m.set_editor_property("used_with_instanced_static_meshes", True)
mel.recompile_material(m)
eal.save_asset(m.get_path_name())
unreal.log_warning("NEEDLE material saved")
