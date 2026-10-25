"""Builds the player bird's materials (BirdSelfBody):
  /Game/Birds/Flight/M_BirdFeather  one feather painted from per-instance custom data 0..12 (BirdLook::FPaint):
                                    base, accent at the tip / root / fringe / bars, a pale spot, an iridescent sheen;
                                    barbs and the shaft drawn from the UV, the vane's edges frayed by the mask
  /Game/Birds/Flight/M_BirdBody     Top/Bottom colours by Mode: the shells round the body and head, the beak root to tip, the legs

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import unreal

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


ACROSS = """
float s = uv.y < 0.42 ? (uv.y - 0.42) / 0.42 : (uv.y - 0.42) / 0.58;
"""

COLOR_HLSL = ACROSS + """
float u = uv.x;
float a = abs(s);
float3 base = float3(c0, c1, c2);
float3 acc = float3(c3, c4, c5);
// barbs: fine ridges slanting from the shaft toward the tip
float barb = sin(u * 140.0 - a * 55.0);
float3 col = base * (0.9 + 0.08 * barb);
// the outline: both edges of the vane and the rounded tip
float outline = min(1.0 - a, (1.0 - u) * 4.0);
float m = 0.0;
if (c6 > 0.001) m = max(m, smoothstep(1.0 - c6 - 0.02, 1.0 - c6 + 0.02, u));
if (c7 > 0.001) m = max(m, 1.0 - smoothstep(c7 - 0.02, c7 + 0.02, u));
if (c8 > 0.001) m = max(m, 1.0 - smoothstep(c8 * 0.75, c8, outline));
if (c9 > 0.5) m = max(m, smoothstep(0.3, 0.6, sin(u * c9 * 6.2832 + s * 0.6)) * step(0.12, u));
col = lerp(col, acc * (0.9 + 0.08 * barb), m);
if (c10 > 0.001)
{
    float2 d = float2((u - (1.0 - c10 - c6 * 0.6 - 0.08)) * 2.2, s * 1.1);
    col = lerp(col, float3(0.9, 0.88, 0.82), 1.0 - smoothstep(c10 * 0.55, c10 * 0.55 + 0.05, length(d)));
}
// the shaft
float shaft = (1.0 - smoothstep(0.015, 0.05, a)) * step(0.04, u) * (1.0 - u * 0.6);
col = lerp(col, col * 0.55 + 0.12, shaft * 0.7);
// gloss: a sheen whose hue turns with the angle to the eye (starlings, crows, swallows)
float3 sheen = lerp(float3(0.05, 0.35, 0.18), float3(0.32, 0.08, 0.45), saturate(c12 + (1.0 - nv) * 0.6 - 0.3));
col += sheen * c11 * (pow(1.0 - nv, 2.0) * 0.35 + 0.04);
return col * (0.9 + 0.2 * r);
"""

MASK_HLSL = ACROSS + """
float u = uv.x;
float a = abs(s);
// the barbs part at the edges: a ragged outline instead of a cut card
float n = frac(sin(dot(float2(floor(u * 90.0), s > 0 ? 1.0 : -1.0), float2(12.9898, 78.233))) * 43758.5453);
return (1.0 - a) > 0.03 + 0.09 * n * a ? 1.0 : 0.0;
"""

m = new_material("/Game/Birds/Flight/M_BirdFeather")
m.set_editor_property("two_sided", True)
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
m.set_editor_property("opacity_mask_clip_value", 0.5)
uv = node(m, unreal.MaterialExpressionTextureCoordinate, -1400, -400)
rnd = node(m, unreal.MaterialExpressionPerInstanceRandom, -1400, -300)
# |N . V|: how squarely the feather faces the eye
nrm = node(m, unreal.MaterialExpressionVertexNormalWS, -1600, -200)
cam = node(m, unreal.MaterialExpressionCameraVectorWS, -1600, -100)
dot = node(m, unreal.MaterialExpressionDotProduct, -1450, -150)
link(nrm, "", dot, "A")
link(cam, "", dot, "B")
nv = node(m, unreal.MaterialExpressionAbs, -1350, -150)
link(dot, "", nv, "")
names = ["c%d" % i for i in range(13)]
color = node(m, unreal.MaterialExpressionCustom, -700, -200, code=COLOR_HLSL, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
             description="Feather", inputs=[custom_input(n) for n in ["uv", "r", "nv"] + names])
link(uv, "", color, "uv")
link(rnd, "", color, "r")
link(nv, "", color, "nv")
data = []
for i, n in enumerate(names):
    d = node(m, unreal.MaterialExpressionPerInstanceCustomData, -1100, 0 + i * 60, data_index=i)
    link(d, "", color, n)
    data.append(d)
mel.connect_material_property(color, "", MP.MP_BASE_COLOR)
mask = node(m, unreal.MaterialExpressionCustom, -700, 300, code=MASK_HLSL, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1,
            description="Vane", inputs=[custom_input("uv")])
link(uv, "", mask, "uv")
mel.connect_material_property(mask, "", MP.MP_OPACITY_MASK)
# glossy feathers are smoother
gl = node(m, unreal.MaterialExpressionMultiply, -500, 500, const_b=-0.3)
link(data[11], "", gl, "A")
rough = node(m, unreal.MaterialExpressionAdd, -350, 500, const_b=0.66)
link(gl, "", rough, "A")
mel.connect_material_property(rough, "", MP.MP_ROUGHNESS)
spec = node(m, unreal.MaterialExpressionConstant, -350, 600, r=0.35)
mel.connect_material_property(spec, "", MP.MP_SPECULAR)
m.set_editor_property("used_with_instanced_static_meshes", True)
mel.recompile_material(m)
eal.save_asset(m.get_path_name())

BODY_HLSL = """
// 0: the body and head shells, Top above and Bottom below (the mesh's U goes round it)
// 1: the beak, Top at the root and Bottom at the tip (U along it); 2: one colour (legs)
float k = Mode < 0.5 ? saturate(sin(uv.x * 6.2832) * 2.0 + 0.3) : Mode < 1.5 ? 1.0 - smoothstep(0.15, 0.7, uv.x) : 1.0;
return lerp(Bottom, Top, k);
"""
b = new_material("/Game/Birds/Flight/M_BirdBody")
buv = node(b, unreal.MaterialExpressionTextureCoordinate, -900, 0)
top = node(b, unreal.MaterialExpressionVectorParameter, -900, 100, parameter_name="Top", default_value=unreal.LinearColor(0.2, 0.15, 0.1, 1))
bot = node(b, unreal.MaterialExpressionVectorParameter, -900, 300, parameter_name="Bottom", default_value=unreal.LinearColor(0.5, 0.45, 0.4, 1))
mode = node(b, unreal.MaterialExpressionScalarParameter, -900, 500, parameter_name="Mode", default_value=0.0)
bc = node(b, unreal.MaterialExpressionCustom, -500, 100, code=BODY_HLSL, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
          description="Body", inputs=[custom_input(n) for n in ("uv", "Top", "Bottom", "Mode")])
link(buv, "", bc, "uv")
link(top, "", bc, "Top")
link(bot, "", bc, "Bottom")
link(mode, "", bc, "Mode")
mel.connect_material_property(bc, "", MP.MP_BASE_COLOR)
r = node(b, unreal.MaterialExpressionScalarParameter, -300, 300, parameter_name="Roughness", default_value=0.6)
mel.connect_material_property(r, "", MP.MP_ROUGHNESS)
# the shells under the feathers must not shine: a dark sphere with plain specular glows against a low sun
sp = node(b, unreal.MaterialExpressionScalarParameter, -300, 400, parameter_name="Specular", default_value=0.5)
mel.connect_material_property(sp, "", MP.MP_SPECULAR)
mel.recompile_material(b)
eal.save_asset(b.get_path_name())
unreal.log_warning("BIRD FLIGHT materials saved")
