"""Builds the two materials AProceduralPlant draws with (their textures are generated in code per
species at runtime, so these are the only plant assets):

  /Game/Plants/M_PlantBark  BarkAlbedo (colour) x Tint, BarkNormal (normal map), snow on upward faces by Snow
  /Game/Plants/M_PlantLeaf  two-sided foliage: Color x LeafDetail (veins) with a per-instance shade
  /Game/Plants/M_PlantFruit a fruit's skin from five numbers per fruit (per-instance custom data on the
                            tree, scalar parameters Fruit0..4 on a fallen one): ground colour by Ripeness,
                            a blush on one side, stripes, lenticels from FruitDetail

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


# --- bark
m = new_material("/Game/Plants/M_PlantBark")
albedo = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -900, -200, parameter_name="BarkAlbedo",
              texture=unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
normal = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -900, 200, parameter_name="BarkNormal",
              texture=unreal.load_asset("/Engine/EngineMaterials/DefaultNormal"), sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
tint = node(m, unreal.MaterialExpressionVectorParameter, -900, -450, parameter_name="Tint", default_value=unreal.LinearColor(1, 1, 1, 1))
snow = node(m, unreal.MaterialExpressionScalarParameter, -900, 500, parameter_name="Snow", default_value=0.0)
wood = node(m, unreal.MaterialExpressionMultiply, -600, -300)
link(albedo, "RGB", wood, "A")
link(tint, "", wood, "B")
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
link(snow, "", mask, "B")
white = node(m, unreal.MaterialExpressionConstant3Vector, -400, -50, constant=unreal.LinearColor(0.85, 0.88, 0.92, 1))
lerp = node(m, unreal.MaterialExpressionLinearInterpolate, -150, -200)
link(wood, "", lerp, "A")
link(white, "", lerp, "B")
link(mask, "", lerp, "Alpha")
mel.connect_material_property(lerp, "", MP.MP_BASE_COLOR)
mel.connect_material_property(normal, "RGB", MP.MP_NORMAL)
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
mel.connect_material_property(base, "", MP.MP_BASE_COLOR)
# light through the blade: the leaf's own colour, a little deeper
sss = node(m, unreal.MaterialExpressionMultiply, -350, 150, const_b=0.7)
link(leaf, "", sss, "A")
mel.connect_material_property(sss, "", MP.MP_SUBSURFACE_COLOR)
rough = node(m, unreal.MaterialExpressionConstant, -300, 400, r=0.55)
mel.connect_material_property(rough, "", MP.MP_ROUGHNESS)
m.set_editor_property("used_with_instanced_static_meshes", True)
mel.recompile_material(m)
eal.save_asset(m.get_path_name())
# --- fruit
def custom_input(name):
    c = unreal.CustomInput()
    c.set_editor_property("input_name", name)
    return c



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
