"""Builds /Game/Characters/Humans/Materials/M_HumanEyeball: the eyeballs ULyudBodyAnimInstance puts behind the generated
cast's eye openings (blender/characters/eyes.py, M_HumanFace's cut; docs/characters.md "Face").

The ball is the engine's sphere (radius 50 local units) stretched into an ellipsoid that fills the opening; it never
turns (an ellipsoid turning would push its long ends through the lids): the iris is drawn where the gaze leaves it,
so the eye looks about by moving the iris over the white. Parameters (a dynamic instance per eye):
    Size      (depth, width, height, socket) cm; local X forward, Y the character's right, Z up; socket 1 = the
              dark flesh behind the ball (a second, flatter body that fills the corners)
    Gaze      the look direction in the ball's frame (X forward)
    IrisColor, ScleraColor linear; IrisR, PupilR cm
Also the cats' and dogs' runtime eyeballs (AAnimalActor, a dark white, a big iris).
The white is greyed toward the corners and under the upper lid (the lids' shade on a real eye), the iris has fibres,
a darker rim and a glossy cornea, so it doesn't read as a ping-pong ball.

  UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this file> -unattended -nullrhi
"""
import unreal

MATS = "/Game/Characters/Humans/Materials"
NAME = "M_HumanEyeball"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty

COMMON = """
float3 p = LP / 50.0 * S.xyz;
float3 g = normalize(G.xyz);
float along = dot(p, g);
float d = length(p - along * g);
float t = d / max(IR, 0.01);
bool iris = along > 0.0 && t < 1.0;
"""
COLOUR = COMMON + """
if (S.w > 0.5)
    return float3(0.09, 0.035, 0.03);
float cy = saturate(abs(p.y) / S.y);
float3 white = SC.rgb;
white = lerp(white, float3(0.42, 0.2, 0.18), saturate(cy * cy * cy) * 0.7);
white *= lerp(1.0, 0.45, saturate(cy * cy));
white *= lerp(1.0, 0.6, saturate(p.z / S.z + 0.15));
if (!iris)
    return white;
float3 b1 = normalize(cross(g, float3(0, 0, 1)));
float3 b2 = cross(g, b1);
float a = atan2(dot(p, b1), dot(p, b2));
float fib = 0.82 + 0.18 * sin(a * 37.0 + t * 7.0) * sin(a * 11.0 + 1.7) + 0.08 * sin(a * 71.0);
float3 col = IC.rgb * lerp(1.45, 0.75, t) * fib;
col = lerp(col, IC.rgb * 0.25, smoothstep(0.8, 1.0, t));
col *= lerp(0.65, 1.0, saturate(-p.z / S.z + 0.6));
float pr = PR / max(IR, 0.01);
return t < pr ? float3(0.006, 0.005, 0.005) : col;
"""
ROUGH = COMMON + """
if (S.w > 0.5)
    return 0.6;
return iris ? 0.06 : 0.22;
"""


def log(m):
    unreal.log_warning("EYEMAT " + str(m))


def param(m, cls, name, value, x, y):
    p = mel.create_material_expression(m, cls, x, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("default_value", value)
    return p


def custom(m, code, out, desc, y, sources):
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -600, y)
    c.set_editor_property("code", code)
    c.set_editor_property("output_type", out)
    c.set_editor_property("description", desc)
    ins = []
    for pin, _, _ in sources:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", pin)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    for pin, src, out_pin in sources:
        if not mel.connect_material_expressions(src, out_pin, c, pin):
            log("could not connect %s" % pin)
    return c


path = MATS + "/" + NAME
if eal.does_asset_exist(path):
    m = unreal.load_asset(path)
    mel.delete_all_material_expressions(m)
else:
    m = tools.create_asset(NAME, MATS, unreal.Material, unreal.MaterialFactoryNew())
lp = mel.create_material_expression(m, unreal.MaterialExpressionLocalPosition, -1100, 0)
src = [("LP", lp, ""),
       ("S", param(m, unreal.MaterialExpressionVectorParameter, "Size", unreal.LinearColor(1.2, 1.2, 1.2, 0), -1100, 100), "RGBA"),
       ("G", param(m, unreal.MaterialExpressionVectorParameter, "Gaze", unreal.LinearColor(1, 0, 0, 0), -1100, 200), "RGBA"),
       ("IR", param(m, unreal.MaterialExpressionScalarParameter, "IrisR", 0.58, -1100, 300), ""),
       ("PR", param(m, unreal.MaterialExpressionScalarParameter, "PupilR", 0.21, -1100, 380), ""),
       ("IC", param(m, unreal.MaterialExpressionVectorParameter, "IrisColor", unreal.LinearColor(0.05, 0.03, 0.02, 1), -1100, 460), "RGBA"),
       ("SC", param(m, unreal.MaterialExpressionVectorParameter, "ScleraColor", unreal.LinearColor(0.56, 0.52, 0.48, 1), -1100, 540), "RGBA")]
col = custom(m, COLOUR, unreal.CustomMaterialOutputType.CMOT_FLOAT3, "EyeColour", 0, src)
rough = custom(m, ROUGH, unreal.CustomMaterialOutputType.CMOT_FLOAT1, "EyeRough", 300, src[:4])
mel.connect_material_property(col, "", MP.MP_BASE_COLOR)
# a fill: the ball sits in a socket the face shades (soft skin shadows shade it all), and without the light a real eye
# gets back off the lids it went pitch black, a hole in the face (2026-10-28). The fill is set on screen (the camera's
# exposure undone), so it reads alike in daylight and in the dark club; Fill is how bright on screen
inv = mel.create_material_expression(m, unreal.MaterialExpressionEyeAdaptationInverse, -600, 550)
fill = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -350, 500)
mel.connect_material_expressions(col, "", fill, "A")
mel.connect_material_expressions(inv, "", fill, "B")
fill2 = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -200, 550)
mel.connect_material_expressions(fill, "", fill2, "A")
mel.connect_material_expressions(param(m, unreal.MaterialExpressionScalarParameter, "Fill", 0.22, -400, 650), "", fill2, "B")
mel.connect_material_property(fill2, "", MP.MP_EMISSIVE_COLOR)
mel.connect_material_property(rough, "", MP.MP_ROUGHNESS)
spec = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 400)
spec.set_editor_property("r", 0.5)
mel.connect_material_property(spec, "", MP.MP_SPECULAR)
mel.recompile_material(m)
eal.save_loaded_asset(m)
log("made " + path)
