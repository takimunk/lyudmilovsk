"""Builds the two materials AProceduralPlant draws with (their textures are generated in code per
species at runtime, so these are the only plant assets):

  /Game/Plants/M_PlantBark  BarkAlbedo (colour) x Tint, BarkNormal (normal map), snow on upward faces by Snow;
                            vertex colour G turns thin twigs TwigColour, R the old trunk base rough and dark
                            (BaseFurrow..BaseRidge by the albedo's alpha)
  /Game/Plants/M_PlantLeaf  two-sided foliage: Color x LeafDetail (veins) with a per-instance shade
  /Game/Plants/M_PlantFruit a fruit's skin from five numbers per fruit (per-instance custom data on the
                            tree, scalar parameters Fruit0..4 on a fallen one): ground colour by Ripeness,
                            a blush on one side, stripes, lenticels from FruitDetail

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


# --- bark
m = new_material("/Game/Plants/M_PlantBark")
albedo = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -900, -200, parameter_name="BarkAlbedo",
              texture=unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
normal = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -900, 200, parameter_name="BarkNormal",
              texture=unreal.load_asset("/Engine/EngineMaterials/DefaultNormal"), sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
# --- against tiling: the texture is sampled twice, the second time at another scale and offset, and the two are blended
# patch by patch by a noise of the world position (a different pattern on every tree, none repeating along a trunk);
# a slower noise lightens and darkens whole stretches of bark
albedo_b = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -900, -700, parameter_name="BarkAlbedo",
                texture=unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
normal_b = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -900, 700, parameter_name="BarkNormal",
                texture=unreal.load_asset("/Engine/EngineMaterials/DefaultNormal"), sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
uv_b = node(m, unreal.MaterialExpressionTextureCoordinate, -1200, -800, coordinate_index=0, u_tiling=0.61, v_tiling=0.37)
uv_off = node(m, unreal.MaterialExpressionAdd, -1050, -800, const_b=0.37)
link(uv_b, "", uv_off, "A")
link(uv_off, "", albedo_b, "UVs")
link(uv_off, "", normal_b, "UVs")
MACRO_HLSL = """
float3 q = P * Scale;
float3 i = floor(q);
float3 f = frac(q);
f = f * f * (3.0 - 2.0 * f);
float h000 = frac(sin(dot(i, float3(127.1, 311.7, 74.7))) * 43758.5453);
float h100 = frac(sin(dot(i + float3(1, 0, 0), float3(127.1, 311.7, 74.7))) * 43758.5453);
float h010 = frac(sin(dot(i + float3(0, 1, 0), float3(127.1, 311.7, 74.7))) * 43758.5453);
float h110 = frac(sin(dot(i + float3(1, 1, 0), float3(127.1, 311.7, 74.7))) * 43758.5453);
float h001 = frac(sin(dot(i + float3(0, 0, 1), float3(127.1, 311.7, 74.7))) * 43758.5453);
float h101 = frac(sin(dot(i + float3(1, 0, 1), float3(127.1, 311.7, 74.7))) * 43758.5453);
float h011 = frac(sin(dot(i + float3(0, 1, 1), float3(127.1, 311.7, 74.7))) * 43758.5453);
float h111 = frac(sin(dot(i + float3(1, 1, 1), float3(127.1, 311.7, 74.7))) * 43758.5453);
return lerp(lerp(lerp(h000, h100, f.x), lerp(h010, h110, f.x), f.y), lerp(lerp(h001, h101, f.x), lerp(h011, h111, f.x), f.y), f.z);
"""
wpos = node(m, unreal.MaterialExpressionWorldPosition, -1500, -300,
            world_position_shader_offset=unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
patch_n = node(m, unreal.MaterialExpressionCustom, -1300, -300, code=MACRO_HLSL, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1,
               description="BarkPatches", inputs=[custom_input("P"), custom_input("Scale")])
link(wpos, "", patch_n, "P")
link(node(m, unreal.MaterialExpressionConstant, -1500, -150, r=0.045), "", patch_n, "Scale")
tone_n = node(m, unreal.MaterialExpressionCustom, -1300, -100, code=MACRO_HLSL, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1,
              description="BarkTone", inputs=[custom_input("P"), custom_input("Scale")])
link(wpos, "", tone_n, "P")
link(node(m, unreal.MaterialExpressionConstant, -1500, 0, r=0.0065), "", tone_n, "Scale")
patch_mask = node(m, unreal.MaterialExpressionSmoothStep, -1100, -300)
link(node(m, unreal.MaterialExpressionConstant, -1250, -420, r=0.38), "", patch_mask, "Min")
link(node(m, unreal.MaterialExpressionConstant, -1250, -370, r=0.62), "", patch_mask, "Max")
link(patch_n, "", patch_mask, "Value")
tone = node(m, unreal.MaterialExpressionMultiply, -1100, -100, const_b=0.36)
link(tone_n, "", tone, "A")
tone1 = node(m, unreal.MaterialExpressionAdd, -950, -100, const_b=0.82)
link(tone, "", tone1, "A")
alb_mix = node(m, unreal.MaterialExpressionLinearInterpolate, -800, -250)
link(albedo, "RGB", alb_mix, "A")
link(albedo_b, "RGB", alb_mix, "B")
link(patch_mask, "", alb_mix, "Alpha")
alb_toned = node(m, unreal.MaterialExpressionMultiply, -650, -250)
link(alb_mix, "", alb_toned, "A")
link(tone1, "", alb_toned, "B")
alpha_mix = node(m, unreal.MaterialExpressionLinearInterpolate, -800, -50)
link(albedo, "A", alpha_mix, "A")
link(albedo_b, "A", alpha_mix, "B")
link(patch_mask, "", alpha_mix, "Alpha")
nrm_mix = node(m, unreal.MaterialExpressionLinearInterpolate, -800, 350)
link(normal, "RGB", nrm_mix, "A")
link(normal_b, "RGB", nrm_mix, "B")
link(patch_mask, "", nrm_mix, "Alpha")
tint = node(m, unreal.MaterialExpressionVectorParameter, -900, -450, parameter_name="Tint", default_value=unreal.LinearColor(1, 1, 1, 1))
snow = node(m, unreal.MaterialExpressionScalarParameter, -900, 500, parameter_name="Snow", default_value=0.0)
vc = node(m, unreal.MaterialExpressionVertexColor, -1300, -700)
twig_col = node(m, unreal.MaterialExpressionVectorParameter, -1300, -900, parameter_name="TwigColour", default_value=unreal.LinearColor(0.08, 0.035, 0.02, 1))
twig = node(m, unreal.MaterialExpressionLinearInterpolate, -1000, -600)
link(alb_toned, "", twig, "A")
link(twig_col, "", twig, "B")
link(vc, "G", twig, "Alpha")
base_r = node(m, unreal.MaterialExpressionVectorParameter, -1300, -1100, parameter_name="BaseRidge", default_value=unreal.LinearColor(0.06, 0.06, 0.055, 1))
base_f = node(m, unreal.MaterialExpressionVectorParameter, -1300, -1250, parameter_name="BaseFurrow", default_value=unreal.LinearColor(0.01, 0.01, 0.01, 1))
rough_base = node(m, unreal.MaterialExpressionLinearInterpolate, -1000, -1100)
link(base_f, "", rough_base, "A")
link(base_r, "", rough_base, "B")
link(alpha_mix, "", rough_base, "Alpha")
bark = node(m, unreal.MaterialExpressionLinearInterpolate, -800, -600)
link(twig, "", bark, "A")
link(rough_base, "", bark, "B")
link(vc, "R", bark, "Alpha")
# the tree's own birch marks (chevrons, shed-branch eyes, lenticels) from its mask, mapped by UV 1
uv1 = node(m, unreal.MaterialExpressionTextureCoordinate, -1300, -1500, coordinate_index=1)
scars = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -1000, -1500, parameter_name="ScarMask",
             texture=unreal.load_asset("/Engine/EngineResources/Black"))
link(uv1, "", scars, "UVs")
scar_col = node(m, unreal.MaterialExpressionVectorParameter, -1000, -1700, parameter_name="ScarColour", default_value=unreal.LinearColor(0.012, 0.011, 0.01, 1))
marked = node(m, unreal.MaterialExpressionLinearInterpolate, -700, -800)
link(bark, "", marked, "A")
link(scar_col, "", marked, "B")
link(scars, "R", marked, "Alpha")
wood = node(m, unreal.MaterialExpressionMultiply, -600, -300)
link(marked, "", wood, "A")
link(tint, "", wood, "B")
# thin twigs swing (vertex colour B: 0 at a twig's base .. 1 at its tip) by SwayAmp cm
sway_amp = node(m, unreal.MaterialExpressionScalarParameter, -1300, 900, parameter_name="SwayAmp", default_value=0.0)
sway_w = node(m, unreal.MaterialExpressionMultiply, -1000, 900)
link(vc, "B", sway_w, "A")
link(sway_amp, "", sway_w, "B")
mel.connect_material_property(wind(m, sway_w, -700, 900), "", MP.MP_WORLD_POSITION_OFFSET)
# snow lies on faces that look up: saturate((N.z - 0.3) * 4) * Snow
nrm = node(m, unreal.MaterialExpressionVertexNormalWS, -900, 650)
nz = node(m, unreal.MaterialExpressionComponentMask, -700, 650, r=False, g=False, b=True, a=False)
link(nrm, "", nz, "")
sub = node(m, unreal.MaterialExpressionSubtract, -550, 650, const_b=0.3)
link(nz, "", sub, "A")
mul = node(m, unreal.MaterialExpressionMultiply, -420, 650, const_b=4.0)
link(sub, "", mul, "A")
sat = node(m, unreal.MaterialExpressionSaturate, -300, 650)
link(mul, "", sat, "")
mask = node(m, unreal.MaterialExpressionMultiply, -180, 600)
link(sat, "", mask, "A")
link(winter_snow.branch_snow(m, snow), "", mask, "B")
white = node(m, unreal.MaterialExpressionConstant3Vector, -400, -50, constant=unreal.LinearColor(0.85, 0.88, 0.92, 1))
lerp = node(m, unreal.MaterialExpressionLinearInterpolate, -150, -200)
link(wood, "", lerp, "A")
link(white, "", lerp, "B")
link(mask, "", lerp, "Alpha")
mel.connect_material_property(lerp, "", MP.MP_BASE_COLOR)
mel.connect_material_property(nrm_mix, "", MP.MP_NORMAL)
rough = node(m, unreal.MaterialExpressionConstant, -300, 300, r=0.92)
mel.connect_material_property(rough, "", MP.MP_ROUGHNESS)
m.set_editor_property("used_with_instanced_static_meshes", True)
mel.recompile_material(m)
eal.save_asset(m.get_path_name())

# --- leaves
m = new_material("/Game/Plants/M_PlantLeaf")
m.set_editor_property("two_sided", True)
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
# --- fruit

FRUIT_HLSL = """
if (UV.y < 0) return float3(0.05, 0.035, 0.018);
float r = saturate(Ripeness + D4);
float3 ground = lerp(GroundUnripe, GroundRipe, r) * lerp(0.88, 1.12, D3);
float side = cos(6.2831853 * (UV.x - D1)) * 0.5 + 0.5;
float lat = saturate(1.0 - abs(UV.y - 0.45) * 1.6);
float n = Detail.r;
float stripe = 0.5 + 0.5 * sin(6.2831853 * (UV.x * 26.0 + (n - 0.5) * 2.0));
float m = saturate((side * lat + (n - 0.5) * 0.6) * 1.8 - (1.0 - D0) * 1.4);
m *= lerp(1.0, saturate(0.35 + 1.1 * stripe), D2);
m *= smoothstep(0.35, 0.95, r);
float3 c = lerp(ground, BlushColour * lerp(0.7, 1.15, n), m);
return lerp(c, ground * 1.3 + 0.03, Detail.g * 0.5);
"""
m = new_material("/Game/Plants/M_PlantFruit")
custom = node(m, unreal.MaterialExpressionCustom, -300, 0, code=FRUIT_HLSL,
              output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description="FruitSkin",
              inputs=[custom_input(n) for n in
                      ("UV", "Detail", "D0", "D1", "D2", "D3", "D4", "Ripeness", "GroundUnripe", "GroundRipe", "BlushColour")])
uv = node(m, unreal.MaterialExpressionTextureCoordinate, -900, -400)
link(uv, "", custom, "UV")
det = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -900, -250, parameter_name="FruitDetail",
           texture=unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
link(det, "RGB", custom, "Detail")
for i in range(5):
    p = node(m, unreal.MaterialExpressionScalarParameter, -1200, -50 + i * 120, parameter_name="Fruit%d" % i, default_value=0.5 if i in (1, 3) else 0.0)
    d = node(m, unreal.MaterialExpressionPerInstanceCustomData, -900, -50 + i * 120, data_index=i)
    link(p, "", d, "DefaultValue")
    link(d, "", custom, "D%d" % i)
for i, (name, col) in enumerate((("GroundUnripe", (0.15, 0.3, 0.04)), ("GroundRipe", (0.45, 0.42, 0.06)), ("BlushColour", (0.5, 0.04, 0.02)))):
    v = node(m, unreal.MaterialExpressionVectorParameter, -900, 600 + i * 150, parameter_name=name, default_value=unreal.LinearColor(col[0], col[1], col[2], 1))
    link(v, "", custom, name)
ripe = node(m, unreal.MaterialExpressionScalarParameter, -900, 1050, parameter_name="Ripeness", default_value=1.0)
link(ripe, "", custom, "Ripeness")
mel.connect_material_property(custom, "", MP.MP_BASE_COLOR)
rough = node(m, unreal.MaterialExpressionConstant, -300, 300, r=0.35)
mel.connect_material_property(rough, "", MP.MP_ROUGHNESS)
m.set_editor_property("used_with_instanced_static_meshes", True)
mel.recompile_material(m)
eal.save_asset(m.get_path_name())
unreal.log_warning("PLANT materials saved")
