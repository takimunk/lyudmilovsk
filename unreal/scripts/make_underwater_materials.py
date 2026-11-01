"""The underwater world's material assets (docs/underwater.md), small, committed under /Game/Sea/Underwater:

  MPC_LyudUnderwater  the current and the eye under water (ULyudUnderwaterSubsystem fills it every frame)
  M_LyudSeaAlga       seaweed and eelgrass (the plant system's alga species): lean with the current, sway with the waves,
                      flutter in the eddies, tips browned in autumn
  M_LyudFish          a fish's body (the Blender-built species meshes): the swimming wave from per-instance data
  M_LyudFishFin       its fins: the same, masked by the texture's alpha, two-sided
  M_LyudSuspended     the motes and flecks drifting in the water round the eye with the current

Run headless (it imports make_sea_materials' Graph helpers):
  UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/make_underwater_materials.py -unattended
"""
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_sea_materials as SM  # noqa: E402

DEST = "/Game/Sea/Underwater"
MPC = DEST + "/MPC_LyudUnderwater"
eal = unreal.EditorAssetLibrary
X = unreal
MP = unreal.MaterialProperty
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
EXCL = unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS


def _log(msg):
    unreal.log_warning("UNDERWATER MATERIALS " + msg)


def collection():
    """Made once; later runs only add missing parameters (re-creating it would cut the materials reading it)."""
    vectors = [
        # the current's mean at the eye's depth (m/s x, y, z) and its turbulence 0..1
        ("Current", (0.0, 0.1, 0.0, 0.2)),
        # how far the water has carried things (m, wrapped at 1000), and a clock (s, wrapped at 3600)
        ("CurrentDrift", (0.0, 0.0, 0.0, 0.0)),
        # the eye: under water 0/1, its depth under the surface (m), the water's level (world cm), the light coming down 0..1
        ("Eye", (0.0, 0.0, -4300.0, 1.0)),
        # the water's colour the motes take (linear) and their brightness
        ("WaterTint", (0.16, 0.30, 0.25, 1.0)),
    ]
    if eal.does_asset_exist(MPC):
        mpc = unreal.load_asset(MPC)
    else:
        mpc = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "MPC_LyudUnderwater", DEST, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    have = {str(p.get_editor_property("parameter_name")) for p in mpc.get_editor_property("vector_parameters")}
    vs = list(mpc.get_editor_property("vector_parameters"))
    for name, v in vectors:
        if name not in have:
            p = unreal.CollectionVectorParameter()
            p.set_editor_property("parameter_name", name)
            p.set_editor_property("default_value", unreal.LinearColor(*v))
            vs.append(p)
    mpc.set_editor_property("vector_parameters", vs)
    eal.save_loaded_asset(mpc)
    return mpc


ALGA_WPO = r"""
// r: 0 at the holdfast .. 1 at the top (local height over the plant's height)
float r = saturate(LP.z / max(HRef, 1.0));
float w = r * r;
// the waves' orbits at this depth (the dominant train, as the old weed did)
float depth = max((Sea.x - P.z) * 0.01, 0.1);
float kh = clamp(SW.z * depth, 0.25, 8.0);
float A = min(SW.w / sinh(kh), 0.5);
float ph = SW.z * dot(SW.xy, P.xy * 0.01) - Om * Sea.y + Ph;
float2 orb = SW.xy * (A * cos(ph) * 0.8) * 100.0 * min(HRef / 60.0, 1.5);
// the current: a frond leans downstream, more the faster (drag against its buoyancy), and flutters in the eddies
float2 U = Cur.xy;
float sp = length(U);
float2 dir = sp > 1e-3 ? U / sp : float2(1.0, 0.0);
float lean = saturate(sp * sp * Flex * 8.0 + sp * Flex * 0.6);
float t = Drift.w;
float fl = sin(t * (1.6 + Rnd) + Rnd * 6.2831853 + P.x * 0.04 + P.y * 0.03 + r * 3.0) * (0.03 + Cur.w * (0.05 + sp));
float2 off = (orb + dir * lean * HRef * 0.75 + float2(-dir.y, dir.x) * fl * HRef) * w * Sway;
// a frond bent over is lower: keep its length roughly
return float3(off, -0.5 * length(off) * r);
"""

ALGA_COLOUR = r"""
float tip = TipBrown > 0.001 ? smoothstep(1.0 - TipBrown, 1.0 - TipBrown + 0.12, UV.y) : 0.0;
float3 c = lerp(Color.rgb, TipColour.rgb, tip);
// darker at the base and inside a clump, a little variation per plant and along the frond's width
return c * (0.55 + 0.45 * saturate(UV.y * 2.0 + 0.2)) * (0.85 + 0.3 * Rnd) * (0.9 + 0.1 * sin(UV.x * 3.14159));
"""


def alga_material(coll, wcoll):
    g = SM.Graph("M_LyudSeaAlga", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE, dest=DEST)
    g.m.set_editor_property("two_sided", True)
    rnd = g.node(X.MaterialExpressionPerInstanceRandom)
    uv = g.node(X.MaterialExpressionTextureCoordinate)
    col = g.custom(ALGA_COLOUR, [("Color", g.vparam("Color", (0.07, 0.16, 0.035, 1.0))), ("TipColour", g.vparam("TipColour", (0.16, 0.12, 0.04, 1.0))),
                                 ("TipBrown", g.param("TipBrown", 0.0)), ("UV", uv), ("Rnd", rnd)], out=F3, desc="AlgaColour")
    g.out(col, MP.MP_BASE_COLOR)
    g.out(g.mul(col, g.param("Translucent", 0.5)), MP.MP_SUBSURFACE_COLOR)
    g.out(g.param("Roughness", 0.45), MP.MP_ROUGHNESS)
    g.out(g.param("Specular", 0.35), MP.MP_SPECULAR)
    pos = g.node(X.MaterialExpressionWorldPosition, world_position_shader_offset=EXCL)
    lp = g.node(X.MaterialExpressionLocalPosition)
    wpo = g.custom(ALGA_WPO, [("LP", lp), ("P", pos), ("Rnd", rnd), ("HRef", g.param("HeightRef", 60.0)), ("Sway", g.param("Sway", 1.0)),
                              ("Flex", g.param("Flex", 1.0)), ("Sea", g.mpc(wcoll, "Sea")), ("SW", g.mpc(wcoll, "SeaWave")),
                              ("Om", g.mpc(wcoll, "SeaWaveOmega")), ("Ph", g.mpc(wcoll, "SeaWavePhase")),
                              ("Cur", g.mpc(coll, "Current")), ("Drift", g.mpc(coll, "CurrentDrift"))], out=F3, desc="AlgaWater")
    g.out(wpo, MP.MP_WORLD_POSITION_OFFSET)
    g.m.set_editor_property("used_with_instanced_static_meshes", True)
    return g.finish()


FISH_WPO = r"""
// Col: R along the body (0 nose .. 1 tail tip), G flexibility, B paired fins; per instance: phase, beat 0..1, bend -1..1
float u = Col.r;
float env = lerp(0.03 + 0.97 * u * u, 0.3 + 0.7 * u, Eel);
float lat = Beat * Amp * Len * env * sin(6.2831853 * Waves * u - Phase);
float s = u - 0.3;
float turn = Bend * Len * 0.1 * s * abs(s);
// the pectoral and pelvic fins scull, the fin edges ripple
float fin = Col.b * sin(Phase * 0.5 + u * 2.0) * 0.035 * Len * (0.4 + Beat) + Col.g * (1.0 - Col.b) * sin(Phase * 1.3 + u * 9.0) * 0.006 * Len;
if (Flat > 0.5)
{
    // a flatfish lies on its blind side: its fins' margins ripple up and down, the body barely bends
    return float3(0.0, 0.0, Col.g * Len * 0.035 * Beat * sin(6.2831853 * 2.0 * u - Phase) + lat * 0.15);
}
return float3(0.0, lat + turn, fin);
"""


def fish_material(coll, fins):
    name = "M_LyudFishFin" if fins else "M_LyudFish"
    g = SM.Graph(name, unreal.MaterialShadingModel.MSM_DEFAULT_LIT, dest=DEST)
    if fins:
        g.m.set_editor_property("two_sided", True)
        g.m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    tex = g.node(X.MaterialExpressionTextureSampleParameter2D, parameter_name="Skin", texture=unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
    tint = g.vparam("Tint", (1.0, 1.0, 1.0, 1.0))
    col = g.mul(tex, tint, a_pin="RGB")
    g.out(col, MP.MP_BASE_COLOR)
    # silver flanks: the pale parts of a silvery fish shine like metal
    met = g.custom("return saturate((dot(C, float3(0.33, 0.33, 0.33)) - 0.35) * 2.0) * Silver;", [("C", (tex, "RGB")), ("Silver", g.param("Silver", 0.0))], desc="Silver")
    g.out(met, MP.MP_METALLIC)
    g.out(g.param("Roughness", 0.32), MP.MP_ROUGHNESS)
    g.out(g.param("Specular", 0.6), MP.MP_SPECULAR)
    if fins:
        g.out(tex, MP.MP_OPACITY_MASK, src_pin="A")
    vc = g.node(X.MaterialExpressionVertexColor)
    cd = [g.node(X.MaterialExpressionPerInstanceCustomData, data_index=i) for i in range(3)]
    local = g.custom(FISH_WPO, [("Col", vc), ("Phase", cd[0]), ("Beat", cd[1]), ("Bend", cd[2]), ("Len", g.param("BodyLength", 20.0)),
                                ("Amp", g.param("Amp", 0.1)), ("Waves", g.param("Waves", 0.9)), ("Eel", g.param("Eel", 0.0)),
                                ("Flat", g.param("Flat", 0.0))], out=F3, desc="Swim")
    world = g.node(X.MaterialExpressionTransform, transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL,
                   transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    g.link(local, world)
    g.out(world, MP.MP_WORLD_POSITION_OFFSET)
    g.m.set_editor_property("used_with_instanced_static_meshes", True)
    return g.finish()


MOTE_WPO = r"""
// each quad is one mote: its corners all sit on its seed point (LP, in a Box cm cube); the water carries it (Drift, m)
// and it wraps round the eye, so the motes fill a cube round the camera and stream through it with the current
float3 seed = LP;
float T = Drift.w;
float3 w = float3(sin(T * 0.37 + Rnd.x * 40.0), cos(T * 0.29 + Rnd.x * 23.0), sin(T * 0.21 + Rnd.x * 17.0)) * 3.0;
float3 rel = frac((seed + Drift.xyz * 100.0 + w - Cam) / Box) - 0.5;
float3 pos = Cam + rel * Box;
float size = Size * (0.4 + 1.2 * Rnd.x * Rnd.x);
float3 right = ResolvedView.ViewToTranslatedWorld[0].xyz;
float3 up = ResolvedView.ViewToTranslatedWorld[1].xyz;
pos += (right * Corner.x + up * Corner.y) * size;
return pos - P;
"""

MOTE_COLOUR = r"""
float d = length(P - Cam) / (Box * 0.5);
float fade = (1.0 - smoothstep(0.55, 1.0, d)) * smoothstep(0.02, 0.08, d);
float dot2 = saturate(1.0 - dot(Corner, Corner));
return Tint.rgb * Tint.a * Eye.w * Eye.x * fade * dot2 * dot2 * (0.4 + 1.2 * Rnd.x);
"""


def mote_material(coll):
    g = SM.Graph("M_LyudSuspended", unreal.MaterialShadingModel.MSM_UNLIT, dest=DEST)
    g.m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    g.m.set_editor_property("translucency_pass", unreal.MaterialTranslucencyPass.MTP_AFTER_DOF)
    g.m.set_editor_property("two_sided", True)
    corner = g.node(X.MaterialExpressionTextureCoordinate, coordinate_index=0)
    rnd = g.node(X.MaterialExpressionTextureCoordinate, coordinate_index=1)
    cam = g.node(X.MaterialExpressionCameraPositionWS)
    lp = g.node(X.MaterialExpressionLocalPosition)
    pos = g.node(X.MaterialExpressionWorldPosition, world_position_shader_offset=EXCL)
    box = g.param("Box", 1400.0)
    wpo = g.custom(MOTE_WPO, [("LP", lp), ("P", pos), ("Cam", cam), ("Corner", corner), ("Rnd", rnd), ("Box", box),
                              ("Size", g.param("Size", 0.35)), ("Drift", g.mpc(coll, "CurrentDrift"))],
                   out=F3, desc="Motes")
    g.out(wpo, MP.MP_WORLD_POSITION_OFFSET)
    pos2 = g.node(X.MaterialExpressionWorldPosition)
    em = g.custom(MOTE_COLOUR, [("P", pos2), ("Cam", cam), ("Corner", corner), ("Rnd", rnd), ("Box", box), ("Tint", g.mpc(coll, "WaterTint")),
                                ("Eye", g.mpc(coll, "Eye"))], out=F3, desc="MoteColour")
    g.out(em, MP.MP_EMISSIVE_COLOR)
    return g.finish()


def build():
    if not eal.does_directory_exist(DEST):
        eal.make_directory(DEST)
    coll = collection()
    wcoll = unreal.load_asset(SM.MPC_PATH)
    alga_material(coll, wcoll)
    fish_material(coll, False)
    fish_material(coll, True)
    mote_material(coll)
    eal.save_directory(DEST)
    _log("done")


if __name__ == "__main__":
    build()
