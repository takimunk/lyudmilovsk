"""The sea's and the wind's material assets, committed under /Game/Sea and /Game/Weather (small):

  /Game/Weather/MPC_LyudWeather  the wind and the sea for any material (ULyudWindSubsystem, ALyudSea fill it)
  /Game/Sea/M_LyudSea            single-layer water with the waves (ALyudSea)
  /Game/Sea/PP_LyudUnderwater    the water's fog under the surface
  /Game/Sea/M_SeaWeed, M_SeaRock, M_SeaJelly, M_SeaFish   the seabed's life (ULyudSeabedComponent)

The HLSL is in tools/sea/sea_hlsl.py, mirroring the C++ wave and wind models. Run headless:
  UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/make_sea_materials.py -unattended
"""
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools", "sea"))
import sea_hlsl as H  # noqa: E402

SEA = "/Game/Sea"
WEATHER = "/Game/Weather"
MPC_PATH = WEATHER + "/MPC_LyudWeather"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
X = unreal
MP = unreal.MaterialProperty
EXCL = unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS

# Baltic coastal water: brownish green, cloudy, a few metres of visibility (coefficients per metre).
# Blue goes first in the Baltic (yellow substance from the rivers), red next (the water itself).
ABSORPTION = (0.45, 0.14, 0.38)
SCATTERING = (0.03, 0.04, 0.032)


def _log(msg):
    unreal.log_warning("SEA MATERIALS " + msg)


def collection():
    """The parameter collection: created once, then only its parameter list is kept up to date
    (re-creating it would cut every material that reads it)."""
    vectors = [("Wind", (0.0, 1.0, 6.0, 0.25)), ("WindDrift", (0.0, 0.0, 0.0, 0.0)),
               ("Sea", (-4300.0, 0.0, 0.5, 20.0)), ("SeaWave", (1.0, 0.0, 0.3, 0.2))]
    scalars = [("SeaWaveOmega", 1.7), ("SeaWavePhase", 0.0)]
    if eal.does_asset_exist(MPC_PATH):
        mpc = unreal.load_asset(MPC_PATH)
    else:
        if not eal.does_directory_exist(WEATHER):
            eal.make_directory(WEATHER)
        mpc = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "MPC_LyudWeather", WEATHER, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    have_v = {str(p.get_editor_property("parameter_name")) for p in mpc.get_editor_property("vector_parameters")}
    have_s = {str(p.get_editor_property("parameter_name")) for p in mpc.get_editor_property("scalar_parameters")}
    vs = list(mpc.get_editor_property("vector_parameters"))
    for name, v in vectors:
        if name not in have_v:
            p = unreal.CollectionVectorParameter()
            p.set_editor_property("parameter_name", name)
            p.set_editor_property("default_value", unreal.LinearColor(*v))
            vs.append(p)
    ss = list(mpc.get_editor_property("scalar_parameters"))
    for name, v in scalars:
        if name not in have_s:
            p = unreal.CollectionScalarParameter()
            p.set_editor_property("parameter_name", name)
            p.set_editor_property("default_value", v)
            ss.append(p)
    mpc.set_editor_property("vector_parameters", vs)
    mpc.set_editor_property("scalar_parameters", ss)
    eal.save_loaded_asset(mpc)
    return mpc


class Graph:
    """A new material and small helpers for wiring its nodes."""

    def __init__(self, name, shading=None, dest=SEA):
        if eal.does_asset_exist(dest + "/" + name):
            eal.delete_asset(dest + "/" + name)
        self.m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, dest, unreal.Material, unreal.MaterialFactoryNew())
        if shading is not None:
            self.m.set_editor_property("shading_model", shading)
        self.y = 0

    def node(self, cls, x=-600, y=None, **props):
        if y is None:
            self.y += 120
            y = self.y
        e = mel.create_material_expression(self.m, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(self, src, dst, pin="", src_pin=""):
        if not mel.connect_material_expressions(src, src_pin, dst, pin):
            raise RuntimeError("%s: no pin %r on %s" % (self.m.get_name(), pin, dst.get_name()))

    def op(self, cls, a, b, x=-300, a_pin="", b_pin=""):
        e = self.node(cls, x)
        self.link(a, e, "A", a_pin)
        self.link(b, e, "B", b_pin)
        return e

    def mul(self, a, b, a_pin="", b_pin=""):
        return self.op(X.MaterialExpressionMultiply, a, b, a_pin=a_pin, b_pin=b_pin)

    def add(self, a, b, a_pin="", b_pin=""):
        return self.op(X.MaterialExpressionAdd, a, b, a_pin=a_pin, b_pin=b_pin)

    def const(self, v):
        if isinstance(v, (int, float)):
            return self.node(X.MaterialExpressionConstant, -900, r=float(v))
        if len(v) == 2:
            return self.node(X.MaterialExpressionConstant2Vector, -900, r=v[0], g=v[1])
        return self.node(X.MaterialExpressionConstant3Vector, -900, constant=unreal.LinearColor(v[0], v[1], v[2], 1))

    def param(self, name, v):
        return self.node(X.MaterialExpressionScalarParameter, -900, parameter_name=name, default_value=float(v))

    def vparam(self, name, v):
        return self.node(X.MaterialExpressionVectorParameter, -900, parameter_name=name, default_value=unreal.LinearColor(*v))

    def tparam(self, name, path="/Engine/EngineResources/Black"):
        return self.node(X.MaterialExpressionTextureObjectParameter, -900, parameter_name=name, texture=unreal.load_asset(path))

    def mpc(self, coll, name):
        e = self.node(X.MaterialExpressionCollectionParameter, -900, collection=coll)
        e.set_editor_property("parameter_name", name)
        return e

    def custom(self, code, inputs, out=unreal.CustomMaterialOutputType.CMOT_FLOAT1, x=-450, desc="Custom"):
        c = self.node(X.MaterialExpressionCustom, x, code=code, output_type=out, description=desc)
        ins = []
        for name, _ in inputs:
            ci = unreal.CustomInput()
            ci.set_editor_property("input_name", name)
            ins.append(ci)
        c.set_editor_property("inputs", ins)
        for name, src in inputs:
            s, p = src if isinstance(src, tuple) else (src, "")
            if not p and isinstance(s, X.MaterialExpressionVectorParameter):
                p = "RGBA"   # all four channels (the default pin is RGB only)
            self.link(s, c, name, p)
        return c

    def out(self, src, prop, src_pin=""):
        if not mel.connect_material_property(src, src_pin, prop):
            raise RuntimeError("%s: could not connect %s" % (self.m.get_name(), prop))

    def finish(self):
        mel.recompile_material(self.m)
        eal.save_loaded_asset(self.m)
        _log("%s built" % self.m.get_name())
        return self.m


def sea_material(coll):
    g = Graph("M_LyudSea", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    g.m.set_editor_property("tangent_space_normal", False)
    g.m.set_editor_property("two_sided", True)

    vpos = g.node(X.MaterialExpressionWorldPosition, world_position_shader_offset=EXCL)
    cam = g.node(X.MaterialExpressionCameraPositionWS)
    tab = g.tparam("WaveTable")
    dep = g.tparam("DepthMap")
    clock = g.vparam("SeaClock", (0.0, 128.0, 0.25, 25.0))
    sp = g.vparam("SeaParams", (0.8, 0.0, 0.0, 0.0))
    rect = g.vparam("DepthRect", (0.0, 0.0, 0.0, 0.0))
    land = g.param("LandSideX", -1.0e7)
    wind = g.mpc(coll, "Wind")
    drift = g.mpc(coll, "WindDrift")

    # the rest point of the water under this vertex, world metres, handed to the pixels
    xy = g.node(X.MaterialExpressionComponentMask, r=True, g=True, b=False, a=False)
    g.link(vpos, xy)
    rest = g.node(X.MaterialExpressionVertexInterpolator)
    g.link(g.mul(xy, g.const(0.01)), rest, "VS")

    wpo = g.custom(H.GUST_LIB + H.VERTEX_HLSL,
                   [("Pos", vpos), ("Cam", cam), ("Tab", tab), ("Dep", dep), ("Clock", clock), ("SP", sp), ("Rect", rect), ("LandX", land)],
                   out=unreal.CustomMaterialOutputType.CMOT_FLOAT3, desc="SeaWaves")
    g.out(wpo, MP.MP_WORLD_POSITION_OFFSET)

    uv = rest
    view = g.node(X.MaterialExpressionCameraVectorWS)
    pix = g.custom(H.GUST_LIB + H.PIXEL_HLSL,
                   [("XY", uv), ("V", view), ("Tab", tab), ("Dep", dep), ("Clock", clock), ("SP", sp), ("Rect", rect), ("LandX", land),
                    ("Wind", wind), ("Drift", drift)],
                   out=unreal.CustomMaterialOutputType.CMOT_FLOAT4, desc="SeaNormal")

    # surf over the shallows, from the scene's own depth under the water and the depth map
    ppos = g.node(X.MaterialExpressionWorldPosition)
    t = g.node(X.MaterialExpressionTime)
    below = g.node(X.MaterialExpressionSceneDepthWithoutWater, fallback_depth=1.0e6)
    here = g.node(X.MaterialExpressionPixelDepth)
    thick = g.custom("return (Below - Here) * abs(V.z);", [("Below", below), ("Here", here), ("V", view)])
    bed = g.custom(H.GUST_LIB + "return Lyud.Depth(XY, Rect, Clock.w, LandX, Dep, DepSampler);",
                   [("XY", uv), ("Rect", rect), ("Clock", clock), ("LandX", land), ("Dep", dep)], desc="Bed")
    surf = g.custom(H.GUST_LIB + H.SURF_HLSL,
                    [("Pos", ppos), ("Time", t), ("Depth", thick), ("Bed", bed), ("SP", sp)],
                    desc="Surf")
    wc = g.node(X.MaterialExpressionComponentMask, r=False, g=False, b=False, a=True)
    g.link(pix, wc)
    # foam values come out soft; lift them so a breaking crest is properly white
    foam = g.custom("return saturate(max(S, W) * 2.5);", [("S", surf), ("W", wc)], desc="Foam")

    nrm = g.custom("float2 a = N.xy; return float3(a, sqrt(saturate(1.0 - dot(a, a))));", [("N", pix)],
                   out=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    lerp = g.node(X.MaterialExpressionLinearInterpolate)
    g.link(nrm, lerp, "A")
    g.link(g.const((0.0, 0.0, 1.0)), lerp, "B")
    g.link(g.mul(foam, g.const(0.6)), lerp, "Alpha")
    g.out(lerp, MP.MP_NORMAL)

    rough = g.node(X.MaterialExpressionComponentMask, r=False, g=False, b=True, a=False)
    g.link(pix, rough)
    g.out(g.add(rough, g.mul(foam, g.const(0.7))), MP.MP_ROUGHNESS)
    g.out(g.mul(foam, g.const((0.9, 0.92, 0.9))), MP.MP_BASE_COLOR)
    # lyud.Sea.DebugFoam: the foam glows, to see where it is
    g.out(g.mul(foam, g.param("DebugFoam", 0.0)), MP.MP_EMISSIVE_COLOR)
    # single-layer water shows its base colour (the foam) as much as its opacity says
    g.out(g.op(X.MaterialExpressionMin, g.mul(foam, g.const(1.6)), g.const(1.0)), MP.MP_OPACITY)
    g.out(g.const(0.255), MP.MP_SPECULAR)

    wo = g.node(X.MaterialExpressionSingleLayerWaterMaterialOutput, x=0)
    for prop, value in ((("Scattering Coefficients", "ScatteringCoefficients"), SCATTERING),
                        (("Absorption Coefficients", "AbsorptionCoefficients"), ABSORPTION),
                        (("Phase G", "PhaseG"), 0.3), (("Color Scale Behind Water", "ColorScaleBehindWater"), 1.0)):
        src = g.const(value)
        if not any(mel.connect_material_expressions(src, "", wo, pin) for pin in prop):
            raise RuntimeError("water output: no pin " + prop[0])
    try:
        g.out(g.const(1.33), MP.MP_REFRACTION)
    except RuntimeError:
        _log("no refraction pin on single-layer water")
    return g.finish()


def underwater_material(coll):
    """Under the surface: the scene fades into the water's own colour by distance, and the light in
    that colour dims the deeper the eye is."""
    g = Graph("PP_LyudUnderwater", None)
    g.m.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    g.m.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_BEFORE_DOF)
    scene = g.node(X.MaterialExpressionSceneTexture, scene_texture_id=unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    depth = g.node(X.MaterialExpressionSceneDepth)
    cam = g.node(X.MaterialExpressionCameraPositionWS)
    sea = g.mpc(coll, "Sea")
    code = r"""
float d = Depth * 0.01;
float camDepth = max((Sea.x - Cam.z) * 0.01, 0.0);
float3 absorb = float3(%f, %f, %f);
float3 sigma = absorb + float3(%f, %f, %f);
float3 T = exp(-sigma * d);
float3 light = exp(-absorb * (camDepth + 0.5));
return Scene.rgb * T + WaterColor * Bright * light * (1.0 - T);
""" % (ABSORPTION + SCATTERING)
    em = g.custom(code, [("Scene", (scene, "Color")), ("Depth", depth), ("Cam", cam), ("Sea", sea),
                         ("WaterColor", g.vparam("WaterColor", (0.09, 0.19, 0.15, 1.0))), ("Bright", g.param("Brightness", 1.0))],
                  out=unreal.CustomMaterialOutputType.CMOT_FLOAT3, desc="Underwater")
    g.out(em, MP.MP_EMISSIVE_COLOR)
    return g.finish()


def weed_material(coll):
    g = Graph("M_SeaWeed", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    g.m.set_editor_property("two_sided", True)
    vc = g.node(X.MaterialExpressionVertexColor)
    rnd = g.node(X.MaterialExpressionPerInstanceRandom)
    tint = g.vparam("Tint", (0.12, 0.2, 0.05, 1.0))
    shade = g.custom("return Tint.rgb * (0.6 + 0.8 * G) * (0.8 + 0.4 * Rnd) * (1.0 + 1.6 * B * float3(1.0, 0.9, 0.4));",
                     [("Tint", tint), ("G", (vc, "G")), ("B", (vc, "B")), ("Rnd", rnd)], out=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    g.out(shade, MP.MP_BASE_COLOR)
    g.out(g.mul(shade, g.const(0.6)), MP.MP_SUBSURFACE_COLOR)
    g.out(g.param("Roughness", 0.5), MP.MP_ROUGHNESS)
    pos = g.node(X.MaterialExpressionWorldPosition, world_position_shader_offset=EXCL)
    wpo = g.custom(H.WEED_HLSL, [("P", pos), ("R", (vc, "R")), ("Rnd", rnd), ("Sea", g.mpc(coll, "Sea")), ("SW", g.mpc(coll, "SeaWave")),
                                 ("Om", g.mpc(coll, "SeaWaveOmega")), ("Ph", g.mpc(coll, "SeaWavePhase")), ("Sway", g.param("Sway", 1.0))],
                   out=unreal.CustomMaterialOutputType.CMOT_FLOAT3, desc="WeedSway")
    g.out(wpo, MP.MP_WORLD_POSITION_OFFSET)
    g.m.set_editor_property("used_with_instanced_static_meshes", True)
    return g.finish()


def rock_material():
    g = Graph("M_SeaRock", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    vc = g.node(X.MaterialExpressionVertexColor)
    rnd = g.node(X.MaterialExpressionPerInstanceRandom)
    pos = g.node(X.MaterialExpressionWorldPosition)
    nz = g.node(X.MaterialExpressionVertexNormalWS)
    code = r"""
// granite speckle and a film of growth on the upper faces
float3 p = P * 0.08;
float s = frac(sin(dot(floor(p), float3(12.9898, 78.233, 37.719))) * 43758.5453);
float3 base = Tint.rgb * (0.6 + 0.8 * G) * (0.75 + 0.5 * s) * (0.85 + 0.3 * Rnd);
float film = saturate(N.z) * 0.5 * Film;
return lerp(base, float3(0.09, 0.11, 0.04), film);
"""
    col = g.custom(code, [("P", pos), ("Tint", g.vparam("Tint", (0.3, 0.3, 0.28, 1.0))), ("G", (vc, "G")), ("Rnd", rnd), ("N", nz),
                          ("Film", g.param("Film", 1.0))], out=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    g.out(col, MP.MP_BASE_COLOR)
    g.out(g.param("Roughness", 0.8), MP.MP_ROUGHNESS)
    g.m.set_editor_property("used_with_instanced_static_meshes", True)
    return g.finish()


def jelly_material():
    g = Graph("M_SeaJelly", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    g.m.set_editor_property("two_sided", True)
    g.m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    vc = g.node(X.MaterialExpressionVertexColor)
    tint = g.vparam("Tint", (0.75, 0.8, 0.9, 1.0))
    col = g.custom("return lerp(Tint.rgb, float3(0.78, 0.5, 0.85), B);", [("Tint", tint), ("B", (vc, "B"))],
                   out=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    g.out(col, MP.MP_BASE_COLOR)
    g.out(g.mul(col, g.const(0.04)), MP.MP_EMISSIVE_COLOR)
    g.out(g.param("Roughness", 0.2), MP.MP_ROUGHNESS)
    # a clear bell drawn as a fine dither (it keeps depth, so the water's fog treats it right), solid rings
    fres = g.node(X.MaterialExpressionFresnel)
    dither = g.custom("""
float a = max(B, 0.22 + 0.5 * F);
// interleaved gradient noise, shifted every frame so the temporal upscaler blends it into a veil
float2 p = Parameters.SvPosition.xy + float2(47.0, 17.0) * float(View.StateFrameIndexMod8);
float r = frac(52.9829189 * frac(dot(p, float2(0.06711056, 0.00583715))));
return a > r ? 1.0 : 0.0;
""", [("B", (vc, "B")), ("F", fres)], desc="Dither")
    g.out(dither, MP.MP_OPACITY_MASK)
    g.m.set_editor_property("used_with_instanced_static_meshes", True)
    return g.finish()


def fish_material():
    g = Graph("M_SeaFish", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    vc = g.node(X.MaterialExpressionVertexColor)
    rnd = g.node(X.MaterialExpressionPerInstanceRandom)
    pos = g.node(X.MaterialExpressionWorldPosition)
    code = r"""
// dark back to silver belly (G); a flatfish gets a sandy mottle instead
float2 p = P.xy * 0.35;
float m = frac(sin(dot(floor(p), float2(12.9898, 78.233))) * 43758.5453);
float3 c = Tint.rgb * (0.35 + 0.9 * G) * lerp(1.0, 0.7 + 0.6 * m, Mottle);
return c * (0.9 + 0.2 * Rnd);
"""
    col = g.custom(code, [("P", pos), ("Tint", g.vparam("Tint", (0.65, 0.7, 0.75, 1.0))), ("G", (vc, "G")), ("Rnd", rnd),
                          ("Mottle", g.param("Mottle", 0.0))], out=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    g.out(col, MP.MP_BASE_COLOR)
    g.out(g.param("Metallic", 0.0), MP.MP_METALLIC)
    g.out(g.param("Roughness", 0.3), MP.MP_ROUGHNESS)
    side = g.node(X.MaterialExpressionTransform, transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL,
                  transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    g.link(g.const((0.0, 1.0, 0.0)), side)
    wpo = g.custom(H.FISH_HLSL, [("Side", side), ("R", (vc, "R")), ("Rnd", rnd), ("T", g.node(X.MaterialExpressionTime)),
                                 ("Wiggle", g.param("Wiggle", 0.0))], out=unreal.CustomMaterialOutputType.CMOT_FLOAT3, desc="Wiggle")
    g.out(wpo, MP.MP_WORLD_POSITION_OFFSET)
    g.m.set_editor_property("used_with_instanced_static_meshes", True)
    return g.finish()


def build():
    for d in (SEA, WEATHER):
        if not eal.does_directory_exist(d):
            eal.make_directory(d)
    coll = collection()
    sea_material(coll)
    underwater_material(coll)
    weed_material(coll)
    rock_material()
    jelly_material()
    fish_material()
    eal.save_directory(SEA)
    eal.save_directory(WEATHER)
    _log("done")


if __name__ == "__main__":
    build()
