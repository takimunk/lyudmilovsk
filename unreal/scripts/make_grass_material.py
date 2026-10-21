"""Builds /Game/Plants/M_PlantGrass, the material AProceduralGrass draws its tufts with (the tuft mesh is
made in code per grass; vertex colour R = how far up the blade, G = a random per blade):

  colour   GreenBase -> GreenTip up the blade, DryBase -> DryTip when dry; Dryness (0 green .. 1 straw) plus a
           per-tuft offset (instance custom data 0); Snow whitens it
  wind     blades swing by SwayAmp cm at the tip, weight R^2, in the same wind field as the trees
  height   HeightScale squashes the tuft toward its root (winter, new spring growth) and the instance cull
           fade (PerInstanceFadeAmount) shrinks it into the ground at the end of its ring

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty
PATH = "/Game/Plants/M_PlantGrass"


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


def vec(m, name, x, y, col):
    return node(m, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(*col, 1))


def scalar(m, name, x, y, v):
    return node(m, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=v)


if eal.does_asset_exist(PATH):
    eal.delete_asset(PATH)
m = tools.create_asset("M_PlantGrass", "/Game/Plants", unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property("two_sided", True)
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)

vc = node(m, unreal.MaterialExpressionVertexColor, -1500, 0)
# --- colour
g_base = vec(m, "GreenBase", -1500, -400, (0.02, 0.06, 0.012))
g_tip = vec(m, "GreenTip", -1500, -250, (0.08, 0.19, 0.035))
d_base = vec(m, "DryBase", -1500, 250, (0.1, 0.075, 0.03))
d_tip = vec(m, "DryTip", -1500, 400, (0.34, 0.25, 0.09))
green = node(m, unreal.MaterialExpressionLinearInterpolate, -1200, -300)
link(g_base, "", green, "A"); link(g_tip, "", green, "B"); link(vc, "R", green, "Alpha")
dry = node(m, unreal.MaterialExpressionLinearInterpolate, -1200, 300)
link(d_base, "", dry, "A"); link(d_tip, "", dry, "B"); link(vc, "R", dry, "Alpha")
# dryness: global + (per-tuft custom data 0 - 0.5) * 0.35, saturated
dryness = scalar(m, "Dryness", -1500, 600, 0.0)
tuft = node(m, unreal.MaterialExpressionPerInstanceCustomData, -1500, 700, data_index=0)
link(node(m, unreal.MaterialExpressionConstant, -1700, 700, r=0.5), "", tuft, "DefaultValue")
off = node(m, unreal.MaterialExpressionSubtract, -1300, 700, const_b=0.5)
link(tuft, "", off, "A")
off2 = node(m, unreal.MaterialExpressionMultiply, -1150, 700, const_b=0.35)
link(off, "", off2, "A")
dsum = node(m, unreal.MaterialExpressionAdd, -1000, 650)
link(dryness, "", dsum, "A"); link(off2, "", dsum, "B")
dsat = node(m, unreal.MaterialExpressionSaturate, -850, 650)
link(dsum, "", dsat, "")
col = node(m, unreal.MaterialExpressionLinearInterpolate, -700, 0)
link(green, "", col, "A"); link(dry, "", col, "B"); link(dsat, "", col, "Alpha")
# each blade a little lighter or darker: 0.8 + 0.4 * G
shade = node(m, unreal.MaterialExpressionMultiply, -800, -100, const_b=0.4)
link(vc, "G", shade, "A")
shade1 = node(m, unreal.MaterialExpressionAdd, -650, -100, const_b=0.8)
link(shade, "", shade1, "A")
shaded = node(m, unreal.MaterialExpressionMultiply, -500, 0)
link(col, "", shaded, "A"); link(shade1, "", shaded, "B")
# snow whitens it, more toward the tip
snow = scalar(m, "Snow", -700, 250, 0.0)
snow_t = node(m, unreal.MaterialExpressionMultiply, -520, 250, const_b=0.9)
link(snow, "", snow_t, "A")
white = node(m, unreal.MaterialExpressionConstant3Vector, -520, 150, constant=unreal.LinearColor(0.8, 0.84, 0.9, 1))
final = node(m, unreal.MaterialExpressionLinearInterpolate, -300, 0)
link(shaded, "", final, "A"); link(white, "", final, "B"); link(snow_t, "", final, "Alpha")
mel.connect_material_property(final, "", MP.MP_BASE_COLOR)
sss = node(m, unreal.MaterialExpressionMultiply, -300, 200, const_b=0.6)
link(final, "", sss, "A")
mel.connect_material_property(sss, "", MP.MP_SUBSURFACE_COLOR)
mel.connect_material_property(node(m, unreal.MaterialExpressionConstant, -300, 350, r=0.6), "", MP.MP_ROUGHNESS)

# --- wind and height
WPO_HLSL = """
float a = sin(T * 1.3 + P.x * 0.011 + P.y * 0.007);
float b = sin(T * 0.83 + P.y * 0.013 - P.x * 0.005);
float g = 0.55 + 0.45 * sin(T * 0.31 + (P.x + P.y) * 0.002);
float w = Amp * H * H;
float3 sway = float3(0.5 + 0.7 * a, 0.6 * b, -0.15 * abs(a)) * g * w;
float k = Scale * Fade;
return sway + float3(0, 0, (k - 1.0) * (P.z - O.z));
"""
wpo = node(m, unreal.MaterialExpressionCustom, -300, 900, code=WPO_HLSL, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
           description="GrassWind", inputs=[custom_input(n) for n in ("P", "O", "T", "H", "Amp", "Scale", "Fade")])
link(node(m, unreal.MaterialExpressionWorldPosition, -700, 900,
          world_position_shader_offset=unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS), "", wpo, "P")
link(node(m, unreal.MaterialExpressionObjectPositionWS, -700, 1000), "", wpo, "O")
link(node(m, unreal.MaterialExpressionTime, -700, 1100), "", wpo, "T")
link(vc, "R", wpo, "H")
link(scalar(m, "SwayAmp", -700, 1200, 6.0), "", wpo, "Amp")
link(scalar(m, "HeightScale", -700, 1300, 1.0), "", wpo, "Scale")
link(node(m, unreal.MaterialExpressionPerInstanceFadeAmount, -700, 1400), "", wpo, "Fade")
mel.connect_material_property(wpo, "", MP.MP_WORLD_POSITION_OFFSET)
m.set_editor_property("used_with_instanced_static_meshes", True)
mel.recompile_material(m)
eal.save_asset(m.get_path_name())
unreal.log_warning("GRASS material saved")
