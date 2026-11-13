"""Topi's material assets under /Game/Topi (small, committed):

  M_TopiSand    the dunes: wind ripples, sand streaming in the wind, the far dunes' slow heave (ALyudTopi's ground)
  M_TopiRiver   the river Ural: particles of light in a fast smooth stream (shows only at night)
  M_TopiGlow    the curtain of glow over the river, seen from far off
  M_TopiMotes   motes of light over the river and along the path to it

The HLSL is in tools/topi/topi_hlsl.py. Run headless (after make_sea_materials.py, which makes MPC_LyudWeather):
  UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/make_topi_materials.py -unattended
"""
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "tools", "topi"))
import topi_hlsl as T  # noqa: E402
import make_sea_materials as S  # noqa: E402

TOPI = "/Game/Topi"
X = unreal
MP = unreal.MaterialProperty
F1, F2, F3, F4 = (unreal.CustomMaterialOutputType.CMOT_FLOAT1, unreal.CustomMaterialOutputType.CMOT_FLOAT2,
                  unreal.CustomMaterialOutputType.CMOT_FLOAT3, unreal.CustomMaterialOutputType.CMOT_FLOAT4)


def _log(msg):
    unreal.log_warning("TOPI MATERIALS " + msg)


def _mask(g, src, r=False, gg=False, b=False, a=False):
    e = g.node(X.MaterialExpressionComponentMask, r=r, g=gg, b=b, a=a)
    g.link(src, e)
    return e


def _vc(g):
    """The vertex colour as one float4."""
    vc = g.node(X.MaterialExpressionVertexColor)
    e = g.node(X.MaterialExpressionAppendVector)
    g.link(vc, e, "A", "")
    g.link(vc, e, "B", "A")
    return e


def _light(g, blend):
    m = g.m
    m.set_editor_property("blend_mode", blend)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    for k, v in (("used_with_static_lighting", False), ("allow_negative_emissive_color", False)):
        try:
            m.set_editor_property(k, v)
        except Exception as e:  # noqa: BLE001
            _log("%s: %s" % (k, e))


def sand(coll):
    g = S.Graph("M_TopiSand", dest=TOPI)
    g.m.set_editor_property("tangent_space_normal", False)
    vpos = g.node(X.MaterialExpressionWorldPosition, world_position_shader_offset=S.EXCL)
    cam = g.node(X.MaterialExpressionCameraPositionWS)
    t = g.node(X.MaterialExpressionTime)
    look = (g.vparam("TopiLook", (0.0, 0.0, 0.0, 1.0)), "RGBA")
    wind = g.mpc(coll, "Wind")
    vc = _vc(g)
    n = g.node(X.MaterialExpressionVertexNormalWS)
    ins = [("Pos", vpos), ("Cam", cam), ("T", t), ("Look", look), ("VC", vc), ("N", n), ("Wind", wind)]
    g.out(g.custom(T.LIB + T.SAND_WPO, ins[:5], out=F3, desc="TopiHeave"), MP.MP_WORLD_POSITION_OFFSET)
    g.out(g.custom(T.LIB + T.SAND_NORMAL, ins, out=F3, desc="TopiSandNormal"), MP.MP_NORMAL)
    col = g.custom(T.LIB + T.SAND_COLOR, ins, out=F4, desc="TopiSandColour")
    g.out(_mask(g, col, r=True, gg=True, b=True), MP.MP_BASE_COLOR)
    g.out(_mask(g, col, a=True), MP.MP_ROUGHNESS)
    g.out(g.const(0.35), MP.MP_SPECULAR)
    return g.finish()


def river():
    g = S.Graph("M_TopiRiver", dest=TOPI)
    _light(g, unreal.BlendMode.BLEND_TRANSLUCENT)
    uv = g.node(X.MaterialExpressionTextureCoordinate, coordinate_index=0)
    t = g.node(X.MaterialExpressionTime)
    look = (g.vparam("TopiLook", (1.0, 0.0, 0.0, 0.0)), "RGBA")
    foot = g.custom("return max(length(ddx(UV)), length(ddy(UV)));", [("UV", uv)], desc="Footprint")
    out = g.custom(T.LIB + T.RIVER, [("UV", uv), ("T", t), ("Look", look), ("VC", _vc(g)), ("Foot", foot), ("Bright", g.param("Bright", 6.0))],
                   out=F4, desc="TopiRiver")
    g.out(_mask(g, out, r=True, gg=True, b=True), MP.MP_EMISSIVE_COLOR)
    g.out(_mask(g, out, a=True), MP.MP_OPACITY)
    return g.finish()


def glow():
    g = S.Graph("M_TopiGlow", dest=TOPI)
    _light(g, unreal.BlendMode.BLEND_ADDITIVE)
    uv = g.node(X.MaterialExpressionTextureCoordinate, coordinate_index=0)
    t = g.node(X.MaterialExpressionTime)
    look = (g.vparam("TopiLook", (1.0, 0.0, 0.0, 0.0)), "RGBA")
    dist = g.node(X.MaterialExpressionPixelDepth)
    out = g.custom(T.LIB + T.GLOW, [("UV", uv), ("T", t), ("Look", look), ("VC", _vc(g)), ("Dist", dist), ("Bright", g.param("Bright", 0.9))],
                   out=F3, desc="TopiGlow")
    g.out(out, MP.MP_EMISSIVE_COLOR)
    return g.finish()


def motes(coll):
    g = S.Graph("M_TopiMotes", dest=TOPI)
    _light(g, unreal.BlendMode.BLEND_ADDITIVE)
    uv = g.node(X.MaterialExpressionTextureCoordinate, coordinate_index=0)
    t = g.node(X.MaterialExpressionTime)
    look = (g.vparam("TopiLook", (1.0, 0.0, 0.0, 0.0)), "RGBA")
    vc = _vc(g)
    wind = g.mpc(coll, "Wind")

    def axis(v):
        """A camera axis as a world direction."""
        e = g.node(X.MaterialExpressionTransform, transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_VIEW,
                   transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
        g.link(g.const(v), e)
        return e

    ins = [("UV", uv), ("T", t), ("Look", look), ("VC", vc), ("Wind", wind), ("Right", axis((1.0, 0.0, 0.0))), ("Up", axis((0.0, 1.0, 0.0)))]
    g.out(g.custom(T.LIB + T.MOTE_WPO, ins, out=F3, desc="TopiMote"), MP.MP_WORLD_POSITION_OFFSET)
    out = g.custom(T.LIB + T.MOTE_LIGHT, [("UV", uv), ("T", t), ("Look", look), ("VC", vc), ("Bright", g.param("Bright", 40.0))],
                   out=F3, desc="TopiMoteLight")
    g.out(out, MP.MP_EMISSIVE_COLOR)
    return g.finish()


def build():
    if not unreal.EditorAssetLibrary.does_directory_exist(TOPI):
        unreal.EditorAssetLibrary.make_directory(TOPI)
    coll = S.collection()
    sand(coll)
    river()
    glow()
    motes(coll)
    _log("done")


if __name__ == "__main__":
    build()
