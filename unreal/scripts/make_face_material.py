"""Builds /Game/Characters/Humans/Materials/M_HumanFace: M_HumanSkin for the generated cast (one texture holds skin and
cloth) whose painted eyes can look about (blender/characters/face_rig.py, docs/characters.md "Face").

Inside each eye opening the texture is read shifted, so the painted iris slides the way the eye turns; the shift fades
to nothing toward the lids (60% of the way out it starts, at the rim it's gone), so the lids and lashes stay put.
Per character (MI_<Id>_body, from face_marks.json):
    EyeL_C, EyeR_C   (u, v, half width m, half height m): the opening's middle in UV and its size on the face
    EyeL_J, EyeR_J   (du/dx, du/dz, dv/dx, dv/dz): face metres -> UV round the eye
At run time (a dynamic instance, ULyudBodyAnimInstance):
    EyeL_Look, EyeR_Look  (x, z metres): where the iris moves on the face (the character's right is -x)

  UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this file> -unattended -nullrhi
"""
import unreal

MATS = "/Game/Characters/Humans/Materials"
NAME = "M_HumanFace"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty

# one eye's shift, written out twice (Metal's HLSL translation trips on arrays of the inputs)
EYE = """
{{
    float4 C = C{s}; float4 J = J{s};
    float2 d = UV - C.xy;
    float det = J.x * J.w - J.y * J.z;
    if (abs(det) > 1e-6)
    {{
        float2 p = float2(J.w * d.x - J.y * d.y, -J.z * d.x + J.x * d.y) / det;
        float r = length(p / max(C.zw, 1e-4));
        float m = 1.0 - smoothstep(0.6, 1.0, r);
        float2 L = L{s}.xy;
        o -= float2(J.x * L.x + J.y * L.y, J.z * L.x + J.w * L.y) * m;
    }}
}}
"""
SHIFT = "float2 o = UV;\n" + EYE.format(s="L") + EYE.format(s="R") + "return o;\n"


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
uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -1500, 0)
cust = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -1100, 0)
cust.set_editor_property("code", SHIFT)
cust.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT2)
cust.set_editor_property("description", "EyeShift")
ins = []
far = unreal.LinearColor(-10.0, -10.0, 0.01, 0.01)
ident = unreal.LinearColor(1.0, 0.0, 0.0, 1.0)
zero = unreal.LinearColor(0.0, 0.0, 0.0, 0.0)
sources = [("UV", uv)]
for i, (pin, pname, val) in enumerate([("CL", "EyeL_C", far), ("JL", "EyeL_J", ident), ("LL", "EyeL_Look", zero),
                                       ("CR", "EyeR_C", far), ("JR", "EyeR_J", ident), ("LR", "EyeR_Look", zero)]):
    sources.append((pin, param(m, unreal.MaterialExpressionVectorParameter, pname, val, -1500, 150 + 100 * i)))
for pin, _ in sources:
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", pin)
    ins.append(ci)
cust.set_editor_property("inputs", ins)
for pin, src in sources:                          # vector parameters' default output is RGB: the custom code wants all 4
    ok = mel.connect_material_expressions(src, "" if pin == "UV" else "RGBA", cust, pin)
    if not ok:
        log("could not connect %s" % pin)
base = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, -200)
base.set_editor_property("parameter_name", "BaseColor")
base.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
mel.connect_material_expressions(cust, "", base, "UVs")
tint = param(m, unreal.MaterialExpressionVectorParameter, "Tint", unreal.LinearColor(1, 1, 1, 1), -700, 50)
mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -400, -150)
mel.connect_material_expressions(base, "RGB", mul, "A")
mel.connect_material_expressions(tint, "", mul, "B")
mel.connect_material_property(mul, "", MP.MP_BASE_COLOR)
nor = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, 300)
nor.set_editor_property("parameter_name", "Normal")
nor.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
nor.set_editor_property("texture", unreal.load_asset("/Engine/EngineMaterials/FlatNormal"))
mel.connect_material_expressions(cust, "", nor, "UVs")
mel.connect_material_property(nor, "RGB", MP.MP_NORMAL)
mel.connect_material_property(param(m, unreal.MaterialExpressionScalarParameter, "Roughness", 0.55, -400, 100), "", MP.MP_ROUGHNESS)
mel.connect_material_property(param(m, unreal.MaterialExpressionScalarParameter, "Specular", 0.35, -400, 180), "", MP.MP_SPECULAR)
mel.recompile_material(m)
eal.save_loaded_asset(m)
log("made " + path)
