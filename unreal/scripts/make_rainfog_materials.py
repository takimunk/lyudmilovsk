"""The rain's material assets under /Game/Weather (small, committed):

  M_RainDrops    the streaks (ALyudRainFx's drop mesh, moved in the vertex shader), unlit translucent after motion blur
  M_RainSplash   rings where drops hit the ground and roofs
  M_WetGround    deferred decal: wet, darker, smoother ground with puddles and rain rings
  MPC_LyudWeather gets the Rain vector (the sea's rain rings; make_sea_materials.py wires them into M_LyudSea)

The HLSL is in tools/rainfog/rain_hlsl.py. Run headless:
  UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/make_rainfog_materials.py -unattended
"""
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "tools", "rainfog"))
import rain_hlsl as R  # noqa: E402
import make_sea_materials as S  # noqa: E402

X = unreal
MP = unreal.MaterialProperty
F1, F2, F3, F4 = (unreal.CustomMaterialOutputType.CMOT_FLOAT1, unreal.CustomMaterialOutputType.CMOT_FLOAT2,
                  unreal.CustomMaterialOutputType.CMOT_FLOAT3, unreal.CustomMaterialOutputType.CMOT_FLOAT4)


def _log(msg):
    unreal.log_warning("RAINFOG MATERIALS " + msg)


def _uv(g, i):
    return g.node(X.MaterialExpressionTextureCoordinate, coordinate_index=i)


def _translucent(g):
    m = g.m
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    for k, v in (("translucency_pass", unreal.MaterialTranslucencyPass.MTP_AFTER_MOTION_BLUR),
                 ("used_with_static_lighting", False), ("allow_negative_emissive_color", False)):
        try:
            m.set_editor_property(k, v)
        except Exception as e:  # noqa: BLE001
            _log("%s: %s" % (k, e))


def drops():
    g = S.Graph("M_RainDrops", dest=S.WEATHER)
    _translucent(g)
    pos = g.node(X.MaterialExpressionWorldPosition, world_position_shader_offset=S.EXCL)
    cam = g.node(X.MaterialExpressionCameraPositionWS)
    t = g.node(X.MaterialExpressionTime)
    box = g.vparam("Box", (3000.0, 2000.0, 600.0, 0.5))
    wind = g.vparam("Wind", (0.0, 0.0, 0.03, 0.12))
    rp = g.vparam("RoofP", (0.0, 0.0, 150.0, 64.0))
    roof = g.tparam("Roof")
    ins = [("C", _uv(g, 0)), ("R1", _uv(g, 1)), ("R2", _uv(g, 2)), ("Cam", cam), ("Pos", pos), ("T", t),
           ("Box", box), ("Wind", wind), ("RoofP", rp), ("Roof", roof)]
    g.out(g.custom(R.DROP_WPO, ins, out=F3, desc="RainDrop"), MP.MP_WORLD_POSITION_OFFSET)
    alpha_vs = g.custom(R.DROP_ALPHA, ins, desc="RainAlpha")
    alpha = g.node(X.MaterialExpressionVertexInterpolator)
    g.link(alpha_vs, alpha, "VS")
    shape = g.custom("float x = abs(U.x * 2.0 - 1.0); return (1.0 - x * x) * sin(3.14159 * saturate(U.y));",
                     [("U", _uv(g, 0))], desc="Streak")
    op = g.mul(g.mul(alpha, shape), g.param("Alpha", 0.4))
    g.out(g.op(X.MaterialExpressionMin, op, g.const(1.0)), MP.MP_OPACITY)
    g.out(g.mul(g.param("Light", 1.0), g.const((2.2, 2.35, 2.6))), MP.MP_EMISSIVE_COLOR)
    return g.finish()


def splash():
    g = S.Graph("M_RainSplash", dest=S.WEATHER)
    _translucent(g)
    pos = g.node(X.MaterialExpressionWorldPosition, world_position_shader_offset=S.EXCL)
    cam = g.node(X.MaterialExpressionCameraPositionWS)
    t = g.node(X.MaterialExpressionTime)
    box = g.vparam("Box", (2400.0, 0.5, 0.45, 1.0))
    rp = g.vparam("RoofP", (0.0, 0.0, 150.0, 64.0))
    roof = g.tparam("Roof")
    ins = [("C", _uv(g, 0)), ("R1", _uv(g, 1)), ("R2", _uv(g, 2)), ("Cam", cam), ("Pos", pos), ("T", t),
           ("Box", box), ("RoofP", rp), ("Roof", roof)]
    g.out(g.custom(R.SPLASH_WPO, ins, out=F3, desc="SplashPlace"), MP.MP_WORLD_POSITION_OFFSET)
    life_vs = g.custom(R.SPLASH_LIFE, ins, out=F2, desc="SplashLife")
    life = g.node(X.MaterialExpressionVertexInterpolator)
    g.link(life_vs, life, "VS")
    shape = g.custom(r"""
float2 d = U * 2.0 - 1.0;
float r = length(d);
float ring = exp(-pow((r - 0.75) / 0.12, 2.0));
float core = smoothstep(0.45, 0.1, r) * (1.0 - L.y) * (1.0 - L.y);
return L.x * (1.0 - L.y) * saturate(ring * 0.35 + core * 0.8);
""", [("U", _uv(g, 0)), ("L", life)], desc="SplashShape")
    g.out(g.op(X.MaterialExpressionMin, g.mul(shape, g.param("Alpha", 0.55)), g.const(1.0)), MP.MP_OPACITY)
    g.out(g.mul(g.param("Light", 1.0), g.const((1.1, 1.15, 1.2))), MP.MP_EMISSIVE_COLOR)
    return g.finish()


def wet():
    g = S.Graph("M_WetGround", dest=S.WEATHER)
    m = g.m
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("tangent_space_normal", False)
    pos = g.node(X.MaterialExpressionWorldPosition)
    view = g.node(X.MaterialExpressionCameraVectorWS)
    t = g.node(X.MaterialExpressionTime)
    wetp = g.vparam("Wet", (0.0, 0.0, 0.0, 0.0))
    rp = g.vparam("RoofP", (0.0, 0.0, 150.0, 64.0))
    roof = g.tparam("Roof")
    c = g.custom(R.WET_HLSL, [("P", pos), ("V", view), ("T", t), ("Wet", wetp), ("RoofP", rp), ("Roof", roof)],
                 desc="WetGround")
    outs = []
    for name, typ in (("Rough", F1), ("Nrm", F3)):
        o = unreal.CustomOutput()
        o.set_editor_property("output_name", name)
        o.set_editor_property("output_type", typ)
        outs.append(o)
    c.set_editor_property("additional_outputs", outs)
    g.out(c, MP.MP_OPACITY)
    g.out(c, MP.MP_ROUGHNESS, "Rough")
    g.out(c, MP.MP_NORMAL, "Nrm")
    g.out(g.const((0.0, 0.0, 0.0)), MP.MP_BASE_COLOR)
    return g.finish()


def collection():
    mpc = unreal.load_asset(S.MPC_PATH)
    vs = list(mpc.get_editor_property("vector_parameters"))
    if "Rain" not in {str(p.get_editor_property("parameter_name")) for p in vs}:
        p = unreal.CollectionVectorParameter()
        p.set_editor_property("parameter_name", "Rain")
        p.set_editor_property("default_value", unreal.LinearColor(0, 0, 0, 0))
        vs.append(p)
        mpc.set_editor_property("vector_parameters", vs)
        unreal.EditorAssetLibrary.save_loaded_asset(mpc)
        _log("MPC: Rain added")


def build():
    collection()
    drops()
    splash()
    wet()
    _log("done")


if __name__ == "__main__":
    build()
