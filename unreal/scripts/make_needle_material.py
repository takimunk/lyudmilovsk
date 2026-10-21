"""Builds /Game/Plants/M_PlantNeedle: M_PlantLeaf, masked by the alpha of LeafDetail (a card of painted needles).

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


# Wind for swinging twigs and their leaves: a smooth field over the world, so a twig and the leaves on
# it move together; W is how far this point swings (cm at full gust).
WIND_HLSL = """
float a = sin(T * 1.3 + P.x * 0.011 + P.y * 0.007);
float b = sin(T * 0.83 + P.y * 0.013 - P.x * 0.005);
float g = 0.55 + 0.45 * sin(T * 0.31 + (P.x + P.y) * 0.002);
return float3(0.5 + 0.7 * a, 0.6 * b, -0.15 * abs(a)) * g * W;
"""


def wind(m, weight, x, y):
    """World position offset node: WIND_HLSL at this vertex, scaled by the expression weight."""
    pos = node(m, unreal.MaterialExpressionWorldPosition, x - 300, y,
               world_position_shader_offset=unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    time = node(m, unreal.MaterialExpressionTime, x - 300, y + 120)
    c = node(m, unreal.MaterialExpressionCustom, x, y, code=WIND_HLSL, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
             description="Wind", inputs=[custom_input(n) for n in ("P", "T", "W")])
    link(pos, "", c, "P")
    link(time, "", c, "T")
    link(weight, "", c, "W")
    return c


# --- leaves
m = new_material("/Game/Plants/M_PlantNeedle")
m.set_editor_property("two_sided", True)
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
m.set_editor_property("opacity_mask_clip_value", 0.15)
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
mel.connect_material_property(base, "", MP.MP_BASE_COLOR)
mel.connect_material_property(detail, "A", MP.MP_OPACITY_MASK)
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
leaf_sw = node(m, unreal.MaterialExpressionMultiply, -1100, 750)
link(leaf_w, "", leaf_sw, "A")
link(leaf_amp, "", leaf_sw, "B")
swing = wind(m, leaf_sw, -800, 750)
FLUTTER_HLSL = """
return float3(0.4 * sin(T * 5.3 + R * 17.0), 0.3 * sin(T * 6.1 + R * 29.0), 0.6 * sin(T * 7.0 + R * 40.0)) * F;
"""
flut = node(m, unreal.MaterialExpressionCustom, -800, 1000, code=FLUTTER_HLSL, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
            description="Flutter", inputs=[custom_input(n) for n in ("T", "R", "F")])
link(node(m, unreal.MaterialExpressionTime, -1100, 1000), "", flut, "T")
link(node(m, unreal.MaterialExpressionPerInstanceRandom, -1100, 1100), "", flut, "R")
link(node(m, unreal.MaterialExpressionScalarParameter, -1100, 1200, parameter_name="Flutter", default_value=0.0), "", flut, "F")
wpo = node(m, unreal.MaterialExpressionAdd, -500, 850)
link(swing, "", wpo, "A")
link(flut, "", wpo, "B")
mel.connect_material_property(wpo, "", MP.MP_WORLD_POSITION_OFFSET)
m.set_editor_property("used_with_instanced_static_meshes", True)
mel.recompile_material(m)
eal.save_asset(m.get_path_name())
unreal.log_warning("NEEDLE material saved")
