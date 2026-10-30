"""Builds /Game/ForestFloor (docs/forest_floor.md): what AForestFloor draws with.

  MPC_LyudWeather gets LitterRect (the litter field's window), FloorWeather (wet, rime, drought, rain), FloorSnow (snow, wind)
  RT_Litter       the litter field (R needles, G leaves, B leaf hue, A decay), written by AForestFloor
  T_Leaves_*, T_Moss_*  leaf litter and feather moss photos (textures/ground)
  M_FloorPlant    the herbs (bilberry, ferns ...) and fallen leaves: Color x LeafDetail, wind sway by height from
                  MPC_LyudWeather.Wind, loose leaves stirred by gusts, wet/rime/drought/snow
  M_FloorGrass    mosses, lichen and forest grass tufts (as M_PlantGrass, plus the weather)
  M_FloorDebris   wood (the species' bark textures, bare broken ends), stones (granite, gneiss), cones, moss cushions;
                  moss on old wood, wet/rime/drought/snow
  M_FloorFalling  leaves falling from the deciduous crowns, moved entirely in the vertex shader

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
  FLOOR_GROUND_ONLY=1 also (or only, FLOOR_GROUND_ONLY=2) puts the litter into the existing M_Ground (forestfloor_ground.py).
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import forestfloor_ground as FG  # noqa: E402

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty
FLOAT1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
FLOAT3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
EXCL = unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS
node, link, custom = FG.node, FG.link, FG.custom


def new_material(name, foliage):
    path = FG.FLOOR + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = tools.create_asset(name, FG.FLOOR, unreal.Material, unreal.MaterialFactoryNew())
    if foliage:
        m.set_editor_property("two_sided", True)
        m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    return m


def finish(m):
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    FG._log("saved " + m.get_name())


def scalar(m, name, x, y, v):
    return node(m, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=v)


def vec(m, name, x, y, col):
    return node(m, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(*col, 1))


def cdata(m, i, x, y):
    c = node(m, unreal.MaterialExpressionPerInstanceCustomData, x, y, data_index=i)
    link(node(m, unreal.MaterialExpressionConstant, x - 150, y, r=0.0), "", c, "DefaultValue")
    return c


def common(m, x, y):
    """World position, object position, time, the floor's weather and the wind."""
    mpc = FG.collection()
    wind = unreal.load_asset("/Game/Weather/MPC_LyudWeather")
    C = unreal.MaterialExpressionCollectionParameter
    return {
        "P": node(m, unreal.MaterialExpressionWorldPosition, x, y, world_position_shader_offset=EXCL),
        "O": node(m, unreal.MaterialExpressionObjectPositionWS, x, y + 100),
        "T": node(m, unreal.MaterialExpressionTime, x, y + 200),
        "Wx": node(m, C, x, y + 300, collection=mpc, parameter_name="FloorWeather"),
        "Sn": node(m, C, x, y + 400, collection=mpc, parameter_name="FloorSnow"),
        "Wd": node(m, C, x, y + 500, collection=wind, parameter_name="Wind"),
        "Nw": node(m, unreal.MaterialExpressionVertexNormalWS, x, y + 600),
        "Fade": node(m, unreal.MaterialExpressionPerInstanceFadeAmount, x, y + 700),
    }


# --- herbs and fallen leaves
PLANT_PS = """
float3 col = Color.rgb * Detail.rgb;
if (Loose > 0.5)
{
    // a fallen leaf: the species' autumn colours, then brown and black as it decays (custom data 0)
    col = lerp(Color.rgb, ColorB.rgb, saturate(D0 * 1.3)) * Detail.rgb;
    col = lerp(col, float3(0.07, 0.05, 0.035) * (0.6 + dot(Detail.rgb, float3(0.33, 0.33, 0.33))), saturate((D0 - 0.35) * 1.6));
}
col *= 0.8 + 0.4 * Rnd;
float up = saturate(Nw.z);
col *= lerp(1.0, 0.62, Wx.x);
float r = lerp(0.6, 0.25, Wx.x);
col = lerp(col, dot(col, float3(0.33, 0.34, 0.33)) * float3(1.15, 1.08, 0.95) + 0.015, Wx.z * 0.4);
col = lerp(col, float3(0.84, 0.87, 0.9), Wx.y * saturate(up * 1.4) * 0.85);
col = lerp(col, float3(0.85, 0.88, 0.92), saturate(Sn.x * 1.2) * saturate((up - 0.25) * 2.5));
Rough = r;
return col;
"""

PLANT_VS = """
float3 lp = P - O;
float h = saturate(lp.z / max(HRef, 1.0));
float spd = Wd.z;
float2 dir = dot(Wd.xy, Wd.xy) > 1e-4 ? normalize(Wd.xy) : float2(1, 0);
float gust = (0.55 + 0.45 * sin(T * 1.9 + P.x * 0.013 + P.y * 0.009)) * (0.6 + 0.6 * Wd.w);
// under the canopy the wind is a third of the open's
float a = Amp * h * h * saturate(0.15 + spd / 9.0) * 0.6;
float3 sway = float3(dir * (gust + 0.3 * sin(T * 4.7 + P.x * 0.05 + P.y * 0.03)), -0.1 * gust) * a;
// loose fallen leaves: stirred and skittered in gusts
float l = Loose * D1;
float stir = l * saturate((spd - 3.0) / 6.0) * (0.5 + 0.5 * sin(T * 2.3 + D1 * 40.0));
sway += float3(dir * stir * 6.0, abs(sin(T * 7.0 + D1 * 90.0)) * stir * 1.5);
return sway + (Fade - 1.0) * lp;
"""


def plant_material():
    m = new_material("M_FloorPlant", True)
    c = common(m, -1400, 600)
    ps = custom(m, PLANT_PS, [
        ("Color", (vec(m, "Color", -1100, -400, (0.1, 0.2, 0.04)), "RGBA")),
        ("ColorB", (vec(m, "ColorB", -1100, -300, (0.25, 0.12, 0.04)), "RGBA")),
        ("Detail", (node(m, unreal.MaterialExpressionTextureSampleParameter2D, -1100, -200, parameter_name="LeafDetail",
                         texture=unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture")), "RGB")),
        ("Loose", scalar(m, "Loose", -1100, 0, 0.0)), ("D0", cdata(m, 0, -1100, 100)),
        ("Rnd", node(m, unreal.MaterialExpressionPerInstanceRandom, -1100, 200)),
        ("Nw", c["Nw"]), ("Wx", c["Wx"]), ("Sn", c["Sn"])], -500, -200, "FloorPlant", extra=[("Rough", FLOAT1)])
    mel.connect_material_property(ps, "", MP.MP_BASE_COLOR)
    sss = node(m, unreal.MaterialExpressionMultiply, -250, 0, const_b=0.6)
    link(ps, "", sss, "A")
    mel.connect_material_property(sss, "", MP.MP_SUBSURFACE_COLOR)
    mel.connect_material_property(ps, "Rough", MP.MP_ROUGHNESS)
    vs = custom(m, PLANT_VS, [("P", c["P"]), ("O", c["O"]), ("T", c["T"]), ("Wd", c["Wd"]), ("Fade", c["Fade"]),
                              ("HRef", scalar(m, "HeightRef", -1100, 900, 30.0)), ("Amp", scalar(m, "SwayAmp", -1100, 1000, 3.0)),
                              ("Loose", scalar(m, "Loose", -1100, 0, 0.0)), ("D1", cdata(m, 1, -1100, 1100))], -500, 700, "FloorPlantWind")
    mel.connect_material_property(vs, "", MP.MP_WORLD_POSITION_OFFSET)
    finish(m)


# --- moss, lichen and grass tufts
GRASS_PS = """
float3 green = lerp(GB.rgb, GT.rgb, VC.r);
float3 dry = lerp(DB.rgb, DT.rgb, VC.r);
float d = saturate(Dryness + (D0 - 0.5) * 0.35);
float3 col = lerp(green, dry, d) * (0.8 + 0.4 * VC.g);
col *= lerp(1.0, 0.6, Wx.x);
col = lerp(col, lerp(col, dry * 0.9, Moss) , Wx.z * 0.6);
col = lerp(col, float3(0.84, 0.87, 0.9), Wx.y * (0.4 + 0.6 * VC.r) * 0.85);
col = lerp(col, float3(0.8, 0.84, 0.9), Snow * 0.9 * (0.4 + 0.6 * VC.r));
Rough = lerp(0.65, 0.3, Wx.x);
return col;
"""

GRASS_VS = """
float spd = Wd.z;
float2 dir = dot(Wd.xy, Wd.xy) > 1e-4 ? normalize(Wd.xy) : float2(1, 0);
float a = sin(T * 1.3 + P.x * 0.011 + P.y * 0.007);
float b = sin(T * 0.83 + P.y * 0.013 - P.x * 0.005);
float g = (0.55 + 0.45 * sin(T * 0.31 + (P.x + P.y) * 0.002)) * saturate(0.2 + spd / 8.0);
float w = Amp * H * H;
float3 sway = float3(dir * (0.5 + 0.7 * a) + float2(-dir.y, dir.x) * 0.5 * b, -0.15 * abs(a)) * g * w;
float k = Scale * Fade;
return sway + float3(0, 0, (k - 1.0) * (P.z - O.z));
"""


def grass_material():
    m = new_material("M_FloorGrass", True)
    c = common(m, -1400, 600)
    vc = node(m, unreal.MaterialExpressionVertexColor, -1100, -600)
    ps = custom(m, GRASS_PS, [
        ("GB", (vec(m, "GreenBase", -1100, -450, (0.02, 0.06, 0.012)), "RGBA")), ("GT", (vec(m, "GreenTip", -1100, -350, (0.08, 0.19, 0.035)), "RGBA")),
        ("DB", (vec(m, "DryBase", -1100, -250, (0.1, 0.075, 0.03)), "RGBA")), ("DT", (vec(m, "DryTip", -1100, -150, (0.34, 0.25, 0.09)), "RGBA")),
        ("VC", vc), ("Dryness", scalar(m, "Dryness", -1100, -50, 0.0)), ("D0", cdata(m, 0, -1100, 50)),
        ("Moss", scalar(m, "Moss", -1100, 150, 1.0)), ("Snow", scalar(m, "Snow", -1100, 250, 0.0)),
        ("Wx", c["Wx"])], -500, -200, "FloorGrass", extra=[("Rough", FLOAT1)])
    mel.connect_material_property(ps, "", MP.MP_BASE_COLOR)
    sss = node(m, unreal.MaterialExpressionMultiply, -250, 0, const_b=0.6)
    link(ps, "", sss, "A")
    mel.connect_material_property(sss, "", MP.MP_SUBSURFACE_COLOR)
    mel.connect_material_property(ps, "Rough", MP.MP_ROUGHNESS)
    vs = custom(m, GRASS_VS, [("P", c["P"]), ("O", c["O"]), ("T", c["T"]), ("Wd", c["Wd"]), ("Fade", c["Fade"]), ("H", (vc, "R")),
                              ("Amp", scalar(m, "SwayAmp", -1100, 900, 0.5)), ("Scale", scalar(m, "HeightScale", -1100, 1000, 1.0))],
                -500, 700, "FloorGrassWind")
    mel.connect_material_property(vs, "", MP.MP_WORLD_POSITION_OFFSET)
    finish(m)


# --- wood, stones, cones, moss cushions
DEBRIS_PS = """
float3 col; float r; float3 nT = float3(0, 0, 1);
float rnd = D0;
float up = saturate(Nw.z);
float3 lp = P - O;
if (Pattern < 0.5)
{
    // wood: the species' bark (UVs in metres), bare weathered wood at the broken ends with its rings
    float2 buv = UV * Tile.xy;
    float4 b = Texture2DSample(BarkA, View.MaterialTextureBilinearWrapedSampler, buv);
    nT = Texture2DSample(BarkN, View.MaterialTextureBilinearWrapedSampler, buv).rgb * 2.0 - 1.0;
    col = b.rgb * (0.75 + 0.35 * rnd);
    float rings = 0.85 + 0.15 * sin(length(UV) * 2400.0);
    float3 bare = float3(0.36, 0.29, 0.21) * rings * (0.8 + 0.3 * rnd);
    col = lerp(col, bare, VC.r);
    nT = normalize(lerp(nT, float3(0, 0, 1), VC.r));
    col = lerp(col, dot(col, float3(0.33, 0.33, 0.33)) * float3(0.9, 0.9, 0.92), 0.25 * rnd);
    r = 0.86;
}
else if (Pattern < 1.5)
{
    // granite and gneiss: crystals, dark mica, quartz, banding; grey-green lichen crusts
    float h1 = frac(sin(dot(floor(lp * 0.9), float3(12.9898, 78.233, 37.719))) * 43758.5453);
    float h2 = frac(sin(dot(floor(lp * 2.7), float3(39.3468, 11.135, 83.155))) * 24634.6345);
    float band = 0.5 + 0.5 * sin(lp.z * 0.35 + sin(lp.x * 0.11) * 3.0);
    col = lerp(TintA.rgb, TintB.rgb, saturate(h1 * 0.7 + band * 0.3 + (rnd - 0.5) * 0.3));
    col = lerp(col, float3(0.03, 0.03, 0.035), step(0.9, h2));
    col = lerp(col, float3(0.62, 0.6, 0.56), step(0.94, h1) * 0.6);
    float lich = saturate((sin(lp.x * 0.07 + lp.y * 0.05) * 0.5 + 0.5) * 1.4 - 0.7) * (0.3 + 0.7 * up);
    col = lerp(col, float3(0.42, 0.45, 0.36), lich * 0.6);
    r = 0.72;
}
else if (Pattern < 2.5)
{
    // a cone's scales in their spirals, the edges lighter
    float s = frac(UV.x * 90.0 + UV.y * 55.0), t = frac(UV.y * 85.0 - UV.x * 45.0);
    float e = smoothstep(0.0, 0.35, min(s, t)) * smoothstep(1.0, 0.75, max(s, t));
    col = lerp(TintA.rgb * 0.6, TintB.rgb, e) * (0.85 + 0.3 * rnd);
    nT = normalize(float3((s - 0.5) * 0.8, (t - 0.5) * 0.8, 1.0));
    r = 0.8;
}
else
{
    float3 mt = Texture2DSample(MossT, View.MaterialTextureBilinearWrapedSampler, lp.xy / 45.0).rgb;
    col = mt * lerp(TintA.rgb, TintB.rgb, rnd) * 4.0;
    r = 0.95;
}
// moss and lichen on what faces up, more on old wood (custom data 1)
float moss = saturate(MossAmt * VC.b * (0.3 + 0.9 * D1) * (0.6 + 0.4 * sin(lp.x * 0.08 + lp.y * 0.05)));
float3 mc = Texture2DSample(MossT, View.MaterialTextureBilinearWrapedSampler, lp.xy / 45.0).rgb * 0.75;
col = lerp(col, mc, moss);
r = lerp(r, 0.95, moss);
nT = normalize(lerp(nT, float3(0, 0, 1), moss));
// the weather: wet stone shines most
bool stone = Pattern > 0.5 && Pattern < 1.5;
col *= lerp(1.0, stone ? 0.55 : 0.62, Wx.x);
r = lerp(r, stone ? 0.12 : 0.3, Wx.x);
col = lerp(col, dot(col, float3(0.33, 0.34, 0.33)) * float3(1.15, 1.08, 0.98) + 0.02, Wx.z * 0.35);
col = lerp(col, float3(0.84, 0.87, 0.9), Wx.y * saturate(up * 1.4) * 0.85);
col = lerp(col, float3(0.86, 0.88, 0.92), saturate(Sn.x * 1.3) * saturate((up - 0.35) * 3.0));
Rough = r;
Nrm = nT;
return col;
"""


def debris_material(moss_tex):
    m = new_material("M_FloorDebris", False)
    c = common(m, -1400, 600)
    T = unreal.MaterialExpressionTextureObjectParameter
    ps = custom(m, DEBRIS_PS, [
        ("Pattern", scalar(m, "Pattern", -1100, -600, 0.0)),
        ("TintA", (vec(m, "TintA", -1100, -500, (0.3, 0.27, 0.25)), "RGBA")), ("TintB", (vec(m, "TintB", -1100, -400, (0.42, 0.33, 0.3)), "RGBA")),
        ("BarkA", node(m, T, -1100, -300, parameter_name="BarkAlbedo", texture=unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))),
        ("BarkN", node(m, T, -1100, -200, parameter_name="BarkNormal", texture=unreal.load_asset("/Engine/EngineMaterials/DefaultNormal"),
                       sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)),
        ("Tile", (vec(m, "BarkTile", -1100, -100, (3.0, 1.25, 0.0)), "RGBA")),
        ("MossT", node(m, unreal.MaterialExpressionTextureObject, -1100, 0, texture=moss_tex)),
        ("MossAmt", scalar(m, "MossAmount", -1100, 100, 0.0)),
        ("VC", node(m, unreal.MaterialExpressionVertexColor, -1100, 200)),
        ("UV", node(m, unreal.MaterialExpressionTextureCoordinate, -1100, 300)),
        ("D0", cdata(m, 0, -1100, 400)), ("D1", cdata(m, 1, -1100, 500)),
        ("P", c["P"]), ("O", c["O"]), ("Nw", c["Nw"]), ("Wx", c["Wx"]), ("Sn", c["Sn"])],
        -500, -200, "FloorDebris", extra=[("Rough", FLOAT1), ("Nrm", FLOAT3)])
    mel.connect_material_property(ps, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(ps, "Rough", MP.MP_ROUGHNESS)
    mel.connect_material_property(ps, "Nrm", MP.MP_NORMAL)
    finish(m)


# --- falling leaves
FALL_VS = """
float H = max(D0, 200.0);
float ph = D1;
float spd = Wd.z;
float2 dir = dot(Wd.xy, Wd.xy) > 1e-4 ? normalize(Wd.xy) : float2(1, 0);
float v = 80.0 + 15.0 * spd;
float t = frac(T * v / H + ph);
float3 lp = P - O;
float a = T * (3.0 + ph * 4.0) + ph * 30.0;
float c1 = cos(a), s1 = sin(a);
float3 rp = float3(lp.x * c1 - lp.z * s1, lp.y, lp.x * s1 + lp.z * c1);
float secs = t * H / v;
float3 drift = float3(dir * secs * spd * 35.0 + float2(sin(T * 1.3 + ph * 20.0), cos(T * 1.1 + ph * 13.0)) * 40.0, H * (1.0 - t));
float vis = smoothstep(0.0, 0.04, t) * smoothstep(1.0, 0.94, t) * Fade;
return rp * vis - lp + drift;
"""


def falling_material():
    m = new_material("M_FloorFalling", True)
    c = common(m, -1400, 600)
    col = node(m, unreal.MaterialExpressionVectorParameter, -1100, -400, parameter_name="Color", default_value=unreal.LinearColor(0.5, 0.35, 0.05, 1))
    det = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -1100, -250, parameter_name="LeafDetail",
               texture=unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
    mul = node(m, unreal.MaterialExpressionMultiply, -800, -300)
    link(col, "", mul, "A")
    link(det, "RGB", mul, "B")
    mel.connect_material_property(mul, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(mul, "", MP.MP_SUBSURFACE_COLOR)
    mel.connect_material_property(node(m, unreal.MaterialExpressionConstant, -800, 0, r=0.6), "", MP.MP_ROUGHNESS)
    vs = custom(m, FALL_VS, [("P", c["P"]), ("O", c["O"]), ("T", c["T"]), ("Wd", c["Wd"]), ("Fade", c["Fade"]),
                             ("D0", cdata(m, 0, -1100, 900)), ("D1", cdata(m, 1, -1100, 1000))], -500, 700, "FloorFalling")
    mel.connect_material_property(vs, "", MP.MP_WORLD_POSITION_OFFSET)
    finish(m)


ONLY = os.environ.get("FLOOR_GROUND_ONLY", "")
if ONLY != "2":
    FG.collection()
    FG.litter_target()
    tex = FG.textures()
    plant_material()
    grass_material()
    debris_material(tex["MossD"])
    falling_material()
if ONLY:
    FG.patch_ground()
FG._log("done")
