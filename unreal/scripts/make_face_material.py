"""Builds /Game/Characters/Humans/Materials/M_HumanFace: M_HumanSkin for the generated cast (one texture holds skin and
cloth) with real eyes: the painted eye inside each opening is cut away (opacity mask) and an eyeball looks out from
behind it (blender/characters/eyes.py, ULyudBodyAnimInstance; docs/characters.md "Face").

The openings are almonds in the reference pose's mesh space (PreSkinnedPosition, cm, through a vertex interpolator: the pixel shader can't read it), so the cut stays on the same
skin whatever the bones do, and the blink morph, squeezing that skin into the lower lid line, closes it. Per character
(MI_<Id>_body, unreal/scripts/set_eyes.py from blender/characters/<id>/eyes.json):
    EyeL_Hole, EyeR_Hole   (cx, cy, cz, half width): the opening's middle on the shell, cm
    EyeL_Lid,  EyeR_Lid    (up, low, tilt, 0): top(u) = cz + up (1 - u^2)^0.75 + tilt u, bottom with -low; u across
    EyeL_Ball, EyeR_Ball   (x, y, z, yaw): the eyeball's middle (cm) and turn (deg, Blender's about z, outer end back)
    EyeL_Size, EyeR_Size   (depth, width, height, the socket's further back), cm
    IrisColor              linear
The last three are for ULyudBodyAnimInstance's eyeballs; they are fed to the cut unused so that the material keeps
them.
A character without them (glasses: Clerk, Crowd_13) keeps its painted eyes.
NormalStrength scales the baked normal map (1 = as baked; less softens what a hard light makes of it).
(Before 2026-10-27 the painted iris was slid about inside the opening instead; it smeared the hi3d paint further.)

  UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this file> -unattended -nullrhi
"""
import unreal

MATS = "/Game/Characters/Humans/Materials"
NAME = "M_HumanFace"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty

# one eye's opening, written out twice (Metal's HLSL translation trips on arrays of the inputs)
EYE = """
{{
    float u = (P.x - H{s}.x) / H{s}.w;
    if (abs(u) < 1.0 && abs(P.y - H{s}.y) < 1.5)
    {{
        float g = pow(1.0 - u * u, 0.75);
        float zc = H{s}.z + L{s}.z * u;
        if (P.z < zc + L{s}.x * g && P.z > zc - L{s}.y * g)
            o = 0.0;
    }}
}}
"""
HOLE = "float o = 1.0;\n" + EYE.format(s="L") + EYE.format(s="R") + "return o;\n"


def log(m):
    unreal.log_warning("FACEMAT " + str(m))


def param(m, cls, name, value, x, y):
    p = mel.create_material_expression(m, cls, x, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("default_value", value)
    return p


path = MATS + "/" + NAME
if eal.does_asset_exist(path):                    # rebuilt in place: the characters' instances keep their parent
    m = unreal.load_asset(path)
    mel.delete_all_material_expressions(m)
else:
    m = tools.create_asset(NAME, MATS, unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property("used_with_skeletal_mesh", True)
m.set_editor_property("used_with_morph_targets", True)
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
cust = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -700, 700)
cust.set_editor_property("code", HOLE)
cust.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
cust.set_editor_property("description", "EyeHoles")
far = unreal.LinearColor(-1.0e4, 0.0, 0.0, 1.0)
lid = unreal.LinearColor(0.0, 0.0, 0.0, 0.0)
pre = mel.create_material_expression(m, unreal.MaterialExpressionPreSkinnedPosition, -1400, 700)
vi = mel.create_material_expression(m, unreal.MaterialExpressionVertexInterpolator, -1100, 700)  # vertex shader only
mel.connect_material_expressions(pre, "", vi, "")
sources = [("P", vi)]
for i, (pin, pname, val) in enumerate([("HL", "EyeL_Hole", far), ("LL", "EyeL_Lid", lid),
                                       ("HR", "EyeR_Hole", far), ("LR", "EyeR_Lid", lid),
                                       ("IC", "IrisColor", unreal.LinearColor(0.05, 0.03, 0.02, 1.0)),
                                       ("BL", "EyeL_Ball", lid), ("SL", "EyeL_Size", lid),
                                       ("BR", "EyeR_Ball", lid), ("SR", "EyeR_Size", lid)]):
    sources.append((pin, param(m, unreal.MaterialExpressionVectorParameter, pname, val, -1100, 800 + 100 * i)))
ins = []
for pin, _ in sources:
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", pin)
    ins.append(ci)
cust.set_editor_property("inputs", ins)
for pin, src in sources:                          # vector parameters' default output is RGB: the custom code wants all 4
    if not mel.connect_material_expressions(src, "" if pin == "P" else "RGBA", cust, pin):
        log("could not connect %s" % pin)
# parts that cast no shadow (Lev Zotov's glasses frames: a hard ring of shade on the cheek): vertex colour alpha 0
# (rig_character.py "no_shadow"; alpha is 1 everywhere else, and a mesh without vertex colours reads white), clipped in
# the shadow pass only
vc = mel.create_material_expression(m, unreal.MaterialExpressionVertexColor, -700, 1000)
one = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -700, 1100)
one.set_editor_property("r", 1.0)
sps = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -450, 1000)   # the ShadowPassSwitch node
sps.set_editor_property("code", "#if SHADOW_DEPTH_SHADER\nreturn S;\n#else\nreturn D;\n#endif\n")  # (not in Python)
sps.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
sps.set_editor_property("description", "ShadowPass")
sins = []
for pin in ("D", "S"):
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", pin)
    sins.append(ci)
sps.set_editor_property("inputs", sins)
mel.connect_material_expressions(one, "", sps, "D")
mel.connect_material_expressions(vc, "A", sps, "S")
cut = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -250, 800)
mel.connect_material_expressions(cust, "", cut, "A")
mel.connect_material_expressions(sps, "", cut, "B")
mel.connect_material_property(cut, "", MP.MP_OPACITY_MASK)
base = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, -200)
base.set_editor_property("parameter_name", "BaseColor")
base.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
tint = param(m, unreal.MaterialExpressionVectorParameter, "Tint", unreal.LinearColor(1, 1, 1, 1), -700, 50)
mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -400, -150)
mel.connect_material_expressions(base, "RGB", mul, "A")
mel.connect_material_expressions(tint, "", mul, "B")
mel.connect_material_property(mul, "", MP.MP_BASE_COLOR)
nor = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, 300)
nor.set_editor_property("parameter_name", "Normal")
nor.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
nor.set_editor_property("texture", unreal.load_asset("/Engine/EngineMaterials/FlatNormal"))
flat = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -500, 450)
flat.set_editor_property("constant", unreal.LinearColor(0, 0, 1, 1))
soft = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -300, 350)
mel.connect_material_expressions(flat, "", soft, "A")
mel.connect_material_expressions(nor, "RGB", soft, "B")
mel.connect_material_expressions(param(m, unreal.MaterialExpressionScalarParameter, "NormalStrength", 0.75, -500, 550), "", soft, "Alpha")
mel.connect_material_property(soft, "", MP.MP_NORMAL)
mel.connect_material_property(param(m, unreal.MaterialExpressionScalarParameter, "Roughness", 0.55, -400, 100), "", MP.MP_ROUGHNESS)
mel.connect_material_property(param(m, unreal.MaterialExpressionScalarParameter, "Specular", 0.35, -400, 180), "", MP.MP_SPECULAR)
# skin: subsurface. Besides the softer light, Virtual Shadow Maps give a subsurface material a wide penumbra
# (r.Shadow.Virtual.SubsurfaceShadowMinSourceAngle): the 24k-triangle shells otherwise shadow themselves in hard
# jagged triangles round the nose, the eyes and the mouth (the "scary" faces, 2026-10-28)
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SUBSURFACE)
# the scatter takes the texture's own colour, a little redder (one texture holds skin, hair and cloth: a fixed skin red
# turned black hair pink), and only a little of it
sss = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -250, 650)
mel.connect_material_expressions(mul, "", sss, "A")
mel.connect_material_expressions(param(m, unreal.MaterialExpressionVectorParameter, "SubsurfaceTint",
                                       unreal.LinearColor(1.0, 0.5, 0.4, 1.0), -400, 700), "", sss, "B")
mel.connect_material_property(sss, "", MP.MP_SUBSURFACE_COLOR)
mel.connect_material_property(param(m, unreal.MaterialExpressionScalarParameter, "Scatter", 1.0, -400, 800), "", MP.MP_OPACITY)
mel.recompile_material(m)
eal.save_loaded_asset(m)
log("made " + path)
