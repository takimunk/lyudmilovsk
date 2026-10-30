"""The forest floor in the ground's material (docs/forest_floor.md): M_Ground's forest layer takes the litter field that
AForestFloor writes round the camera (/Game/ForestFloor/RT_Litter, MPC_LyudWeather.LitterRect): fallen leaves tinted by the
trees about (birch yellow, maple and rowan orange-red, oak and alder brown), darkened and matted as they decay, feather
moss where there are few leaves, warmer needle litter under the pines; drought pales it, rime frosts it, rain mats it.
Its parameters (LitterRect, FloorWeather, FloorSnow) are added to MPC_LyudWeather (only added, never re-created).

  ground_litter(m, blend, forest_w)   a Custom node between the ground's blend and its wet sand (same output names)
  patch_ground()                      puts it into an existing M_Ground in place (no ground rebuild): FLOOR_GROUND_ONLY=1
                                      with make_forestfloor_materials.py

Textures: textures/ground/Leaves_*.jpg and Moss_*.jpg (tools/fal_ground_textures.py, make_ground_textures.py).
"""
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
TEX_DIR = os.path.join(REPO, "textures", "ground")
FLOOR = "/Game/ForestFloor"
# the floor's parameters live in the weather's collection: a landscape material may read only two (M_Ground reads it and the winter's)
MPC_PATH = "/Game/Weather/MPC_LyudWeather"
RT_PATH = FLOOR + "/RT_Litter"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
EXCL = unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS
MPC_VECTORS = [("LitterRect", (0, 0, 0, 0)), ("FloorWeather", (0, 0, 0, 0)), ("FloorSnow", (0, 0.3, 0, 0))]


def _log(msg):
    unreal.log_warning("FORESTFLOOR " + msg)


def node(m, cls, x, y, **props):
    e = mel.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError("could not connect %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def custom(m, code, inputs, x, y, desc, out=unreal.CustomMaterialOutputType.CMOT_FLOAT3, extra=()):
    """A Custom HLSL node; inputs: [(name, node or (node, pin))], extra: [(name, CustomMaterialOutputType)]."""
    c = node(m, unreal.MaterialExpressionCustom, x, y, code=code, output_type=out, description=desc)
    ins = []
    for name, _ in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    outs = []
    for name, typ in extra:
        o = unreal.CustomOutput()
        o.set_editor_property("output_name", name)
        o.set_editor_property("output_type", typ)
        outs.append(o)
    if outs:
        c.set_editor_property("additional_outputs", outs)
    for name, src in inputs:
        s, p = src if isinstance(src, tuple) else (src, "")
        link(s, p, c, name)
    return c


def collection():
    if not eal.does_directory_exist(FLOOR):
        eal.make_directory(FLOOR)
    if eal.does_asset_exist(FLOOR + "/MPC_LyudFloor"):
        eal.delete_asset(FLOOR + "/MPC_LyudFloor")        # an early build's own collection
    mpc = unreal.load_asset(MPC_PATH)
    if not mpc:
        raise RuntimeError("no %s: run make_sea_materials.py first" % MPC_PATH)
    have = {str(p.get_editor_property("parameter_name")) for p in mpc.get_editor_property("vector_parameters")}
    vs = list(mpc.get_editor_property("vector_parameters"))
    for name, v in MPC_VECTORS:
        if name not in have:
            p = unreal.CollectionVectorParameter()
            p.set_editor_property("parameter_name", name)
            p.set_editor_property("default_value", unreal.LinearColor(*[float(c) for c in v]))
            vs.append(p)
    mpc.set_editor_property("vector_parameters", vs)
    eal.save_loaded_asset(mpc)
    return mpc


def litter_target():
    if eal.does_asset_exist(RT_PATH):
        rt = unreal.load_asset(RT_PATH)
    else:
        rt = tools.create_asset("RT_Litter", FLOOR, unreal.TextureRenderTarget2D, unreal.TextureRenderTargetFactoryNew())
    rt.set_editor_property("render_target_format", unreal.TextureRenderTargetFormat.RTF_RGBA16F)
    rt.set_editor_property("size_x", 128)
    rt.set_editor_property("size_y", 128)
    rt.set_editor_property("clear_color", unreal.LinearColor(0, 0, 0, 0))
    rt.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    rt.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    rt.set_editor_property("auto_generate_mips", False)
    rt.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
    eal.save_loaded_asset(rt)
    return rt


def textures():
    """textures/ground/{Leaves,Moss}_{D,N}.jpg -> /Game/ForestFloor/T_*."""
    tasks = []
    for name in ("Leaves", "Moss"):
        for suffix in ("D", "N"):
            t = unreal.AssetImportTask()
            t.filename = os.path.join(TEX_DIR, "%s_%s.jpg" % (name, suffix))
            t.destination_path = FLOOR
            t.destination_name = "T_%s_%s" % (name, suffix)
            t.automated = True
            t.replace_existing = True
            t.save = False
            tasks.append(t)
    tools.import_asset_tasks(tasks)
    out = {}
    for name in ("Leaves", "Moss"):
        for suffix in ("D", "N"):
            tex = eal.load_asset("%s/T_%s_%s" % (FLOOR, name, suffix))
            if suffix == "N":
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
                tex.set_editor_property("srgb", False)
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
            eal.save_loaded_asset(tex)
            out[name + suffix] = tex
    return out


UV_HLSL = """
UVLeaf = P.xy / 75.0;
UVMoss = P.xy / 120.0;
UVLeafFar = P.xy / 730.0;
UVMossFar = P.xy / 1900.0;
return saturate((P.xy - Rect.xy) * Rect.z);
"""

LITTER_HLSL = """
float3 c = Col; float3 n = N; float r = R;
float2 uv = (P.xy - Rect.xy) * Rect.z;
float inside = Rect.w * step(0.0, uv.x) * step(uv.x, 1.0) * step(0.0, uv.y) * step(uv.y, 1.0);
float edge = saturate(min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y)) * 12.0);
float w = saturate(WF) * inside * edge;
if (w <= 0.001) { Normal = n; Roughness = r; return c; }
float near = 1.0 - saturate((Dist - 1500.0) / 6000.0);
float3 leafA = LeafD.rgb;
float3 leafN = LeafN.rgb;
float3 mossA = MossD.rgb;
float3 mossN = MossN.rgb;
// the leaves' colour from the trees about: birch yellow -> maple and rowan orange-red -> oak and alder brown; the photo keeps the shapes
float lum = dot(leafA, float3(0.3, 0.59, 0.11));
float3 hueC = L.b < 0.5 ? lerp(float3(0.62, 0.44, 0.08), float3(0.56, 0.18, 0.05), L.b * 2.0)
                        : lerp(float3(0.56, 0.18, 0.05), float3(0.27, 0.16, 0.08), L.b * 2.0 - 1.0);
float3 leafC = lerp(leafA * 0.8, hueC * lum * 2.0, 0.55);
// not every leaf the same: some already brown, some still pale, in patches of a metre
float vary = MossFar.r * 2.0 - 0.6 + LeafFar.b - 0.4;
leafC = lerp(leafC, leafC * float3(0.55, 0.42, 0.3), saturate(vary));
// decay: dark, matted, brown-black by spring
leafC = lerp(leafC, float3(0.085, 0.06, 0.04) * (0.55 + lum), saturate(L.a * 1.1));
// patchy where thin: a slow breakup from a far tiling of the same photo
// never a wall-to-wall carpet: the needles and the ground's own photo show through in streaks and round the trunks
float leafCov = saturate((L.g - 0.2 + (LeafFar.g - 0.45) * 1.1) * 2.0) * w * 0.88;
// feather moss where few leaves lie, in carpets
float mossCov = saturate((MossFar.g - 0.32) * 3.5) * (1.0 - L.g) * w * 0.85;
c = lerp(c, mossA * float3(0.72, 0.8, 0.62), mossCov);
n = normalize(lerp(n, float3(mossN.xy * near, 1.0), mossCov));
c = lerp(c, leafC, leafCov);
n = normalize(lerp(n, float3(leafN.xy * near * (1.0 - L.a * 0.6), 1.0), leafCov));
r = lerp(r, lerp(0.8, 0.92, mossCov), max(leafCov, mossCov));
// pine needles under the pines: warmer, rustier
c = lerp(c, c * float3(1.1, 0.94, 0.78), saturate(L.r) * w * (1.0 - leafCov) * 0.45);
// drought pales and crisps it; rime frosts the tops; rain mats the leaves flat (the wet decal darkens them)
float dry = Wx.z * w;
c = lerp(c, dot(c, float3(0.33, 0.34, 0.33)) * float3(1.18, 1.1, 0.96) + 0.02, dry * 0.45);
r = lerp(r, 0.96, dry);
float rime = Wx.y * w;
float rn = frac(sin(dot(floor(P.xy * 0.9), float2(12.9898, 78.233))) * 43758.5453);
c = lerp(c, float3(0.82, 0.86, 0.9), rime * (0.55 + 0.4 * rn) * saturate(n.z * 1.2));
r = lerp(r, 0.5, rime * 0.6);
n = normalize(lerp(n, float3(0, 0, 1), Wx.x * w * 0.45));
Normal = n;
Roughness = r;
return c;
"""


def ground_litter(m, blend, forest_w, x=-150, y=700):
    """blend: the ground's blend node ("" colour, "Normal", "Roughness"); forest_w: the Forest layer's weight.
    The textures are sampled by plain sample nodes on the ground's shared samplers (a landscape material is near
    Metal's 16-sampler limit; texture objects in HLSL would each take one)."""
    mpc = collection()
    rt = litter_target()
    tex = textures()
    MPCN = unreal.MaterialExpressionCollectionParameter
    X = x - 700
    pos = node(m, unreal.MaterialExpressionWorldPosition, X - 300, y, world_position_shader_offset=EXCL)
    rect = node(m, MPCN, X - 300, y + 100, collection=mpc, parameter_name="LitterRect")
    uvs = custom(m, UV_HLSL, [("P", pos), ("Rect", rect)], X, y, "ForestLitterUV", out=unreal.CustomMaterialOutputType.CMOT_FLOAT2,
                 extra=[(n, unreal.CustomMaterialOutputType.CMOT_FLOAT2) for n in ("UVLeaf", "UVMoss", "UVLeafFar", "UVMossFar")])
    wrap = unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS
    clamp = unreal.SamplerSourceMode.SSM_CLAMP_WORLD_GROUP_SETTINGS

    def sample(texture, pin, yy, normal=False, src=wrap):
        props = dict(texture=texture, sampler_source=src)
        if normal:
            props["sampler_type"] = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
        elif texture == rt:
            props["sampler_type"] = unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
        s_ = node(m, unreal.MaterialExpressionTextureSample, X + 300, y + yy, **props)
        link(uvs, pin, s_, "UVs")
        return (s_, "RGBA")

    c = custom(m, LITTER_HLSL, [
        ("Col", (blend, "")), ("N", (blend, "Normal")), ("R", (blend, "Roughness")), ("WF", forest_w),
        ("P", pos), ("Rect", rect),
        ("L", sample(rt, "", 100, src=clamp)),
        ("Wx", node(m, MPCN, X + 300, y + 200, collection=mpc, parameter_name="FloorWeather")),
        ("LeafD", sample(tex["LeavesD"], "UVLeaf", 300)),
        ("LeafN", sample(tex["LeavesN"], "UVLeaf", 400, normal=True)),
        ("MossD", sample(tex["MossD"], "UVMoss", 500)),
        ("MossN", sample(tex["MossN"], "UVMoss", 600, normal=True)),
        ("LeafFar", sample(tex["LeavesD"], "UVLeafFar", 700)),
        ("MossFar", sample(tex["MossD"], "UVMossFar", 800)),
        ("Dist", node(m, unreal.MaterialExpressionPixelDepth, X + 300, y + 900)),
    ], x, y, "ForestLitter",
        extra=[("Normal", unreal.CustomMaterialOutputType.CMOT_FLOAT3), ("Roughness", unreal.CustomMaterialOutputType.CMOT_FLOAT1)])
    return c


def patch_ground(path="/Game/Scene01/Terrain/M_Ground"):
    """Puts the litter node between GroundBlend and WetSand of an existing M_Ground (replacing an older one)."""
    m = unreal.load_asset(path)
    if not m:
        raise RuntimeError("no " + path)
    blend = wet = None
    for e in [e for e in unreal.ObjectIterator(unreal.MaterialExpression) if e.get_outer() == m]:
        d = e.get_editor_property("description") if isinstance(e, unreal.MaterialExpressionCustom) else ""
        if d == "ForestLitter":
            mel.delete_material_expression(m, e)
        elif d == "GroundBlend":
            blend = e
        elif d == "WetSand":
            wet = e
        elif d == "ForestLitterUV" or (e.get_editor_property("material_expression_editor_x") in (-1150, -850, -550) and
                                       e.get_editor_property("material_expression_editor_y") >= 700):
            mel.delete_material_expression(m, e)      # the litter node's own inputs
    if blend is None or wet is None:
        raise RuntimeError("M_Ground has no GroundBlend / WetSand: rebuild the ground")
    fw = node(m, unreal.MaterialExpressionLandscapeLayerSample, -550, 1600, parameter_name="Forest", preview_weight=0.0)
    lit = ground_litter(m, blend, fw)
    link(lit, "", wet, "C")
    link(lit, "Normal", wet, "N")
    link(lit, "Roughness", wet, "R")
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    _log("M_Ground: forest litter patched in")
