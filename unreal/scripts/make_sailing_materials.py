"""The sailing school's master materials under /Game/Sailing/Materials (docs/yacht_school.md "Meshes (part C)").
Built by script like make_sea_materials.py / make_festival_materials.py; import_sailing.py calls build() before it makes
the instances. Run alone:
  UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/make_sailing_materials.py -unattended

CUSTOM PRIMITIVE DATA (set per boat part by ALyudDinghy with SetCustomPrimitiveDataFloat; every part of one boat gets
the same block so one material instance serves the whole fleet):
  [0] camber   -1..1   the sail's belly: sign = side (+ bellies to the part's local +Y = port), size = depth (1 = 10 %
                       of the chord at 40 % aft of the luff, flatter at the foot and the head)
  [1] luff      0..1   luffing flutter: a wave running from the luff to the leech (0 = drawing, 1 = flogging)
  [2] number    0..999 the sail number (drawn from T_Sail_Digits in the class's number boxes, both sides)
  [3..5] tint   linear RGB of the hull where COLOR_0.R = 1 (M_Sail_Hull); 0,0,0 = keep the slot's own colour

Mesh channels the materials read (blender/sailing/boats.py): sail UV0 = the class's sail picture (Blender v up; glTF
flips it, so the material uses 1 - v for the number boxes), UV1 = (s from the luff 0..1, h from the foot 0..1),
UV2 = (chord m at that height, distance from the luff m), COLOR_0.B = 1 starboard face / 0 port face / 0.5 edges;
hull COLOR_0.R = tint mask, COLOR_0.G = cavity (1 open). Flags and the windsock: UV1.x = 0 at the hoist / mouth .. 1 at
the fly / tail.

  M_Sail_Cloth     sail: picture + numbers, WPO belly + flutter (two-sided faces are real geometry: one-sided material)
  M_Sail_Hull      gelcoat + tint mask + cavity, roughness / metallic parameters
  M_Sail_Flag      signal flags and the pin flag: picture (Image), WPO ripple growing with the wind (MPC Wind.z), the
                   engine yaws the flag actor downwind
  M_Sail_Windsock  bands by slot colour (Color), WPO droop in a calm and flutter in wind; the engine yaws it
"""
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_sea_materials as S  # noqa: E402

DEST = "/Game/Sailing/Materials"
X = unreal
MP = unreal.MaterialProperty
F1, F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT1, unreal.CustomMaterialOutputType.CMOT_FLOAT3

SAIL_WPO = r"""
float s = saturate(UV1.x);
float h = saturate(UV1.y);
float chord = max(UV2.x, 0.05) * 100.0;              // cm
float shape = 4.0 * s * (1.0 - s) * (1.0 - 0.35 * s); // deepest ~40 % aft
float hfac = saturate(h * 6.0) * (1.0 - 0.35 * h);    // flat at the boom, flatter toward the head
float depth = Camber * 0.10 * chord * shape * hfac;
float w = sin(T * 9.0 - s * 7.0 + h * 3.0) + 0.5 * sin(T * 15.3 - s * 11.0 + h * 5.0);
float flut = Luff * chord * 0.035 * w * sqrt(s);
return normalize(Side) * (depth + flut);
"""

SAIL_COLOUR = r"""
float3 col = Pic;
float side = VC.b;
if (side < 0.25 || side > 0.75)
{
    float4 box = side > 0.75 ? NumStbd : NumPort;     // Blender UV (v up): u0 v0 u1 v1
    float2 uv = float2(UV0.x, 1.0 - UV0.y);
    float lx = (uv.x - box.x) / max(box.z - box.x, 1e-4);
    float ly = (uv.y - box.y) / max(box.w - box.y, 1e-4);
    if (lx >= 0.0 && lx <= 1.0 && ly >= 0.0 && ly <= 1.0)
    {
        if (side > 0.75) lx = 1.0 - lx;               // the starboard face sees the picture mirrored
        float n = clamp(round(Number), 0.0, 999.0);
        float count = n >= 100.0 ? 3.0 : (n >= 10.0 ? 2.0 : 1.0);
        float cellw = 1.0 / 3.0;
        float x0 = 0.5 - count * cellw * 0.5;          // centred in the box
        float k = floor((lx - x0) / cellw);
        if (k >= 0.0 && k < count)
        {
            float p = count - 1.0 - k;                 // power of ten of this digit
            float d = fmod(floor(n / pow(10.0, p)), 10.0);
            float fx = (lx - x0) / cellw - k;
            float2 duv = float2((d + fx) / 10.0, 1.0 - ly);
            float a = Digits.SampleLevel(DigitsSampler, duv, 0).a;
            col = lerp(col, Ink, a);
        }
    }
}
return col;
"""

HULL_COLOUR = r"""
float3 tint = (Tint.r + Tint.g + Tint.b) > 0.0 ? Tint : Base;
return lerp(Base, tint, VC.r) * lerp(0.55, 1.0, VC.g);
"""

FLAG_WPO = r"""
float u = saturate(UV1.x);
float sp = Wind.z * Blow;
float k = saturate(sp / 10.0);
float w = sin(T * (5.0 + 12.0 * k) - u * 7.0) + 0.4 * sin(T * (9.0 + 9.0 * k) - u * 13.0);
float droop = (1.0 - saturate(sp / 4.0)) * u * u * Size * 0.6;
return normalize(Side) * (Size * 0.08 * (0.3 + k) * w * u) - float3(0.0, 0.0, droop);
"""


def _uv(g, i):
    return g.node(X.MaterialExpressionTextureCoordinate, coordinate_index=i)


def _cpd(g, name, index, default=0.0):
    p = g.node(X.MaterialExpressionScalarParameter, -900, parameter_name=name, default_value=float(default))
    p.set_editor_property("use_custom_primitive_data", True)
    p.set_editor_property("primitive_data_index", index)
    return p


def _cpd_vec(g, name, index):
    p = g.node(X.MaterialExpressionVectorParameter, -900, parameter_name=name, default_value=unreal.LinearColor(0, 0, 0, 1))
    p.set_editor_property("use_custom_primitive_data", True)
    p.set_editor_property("primitive_data_index", index)
    return p


def _side(g):
    t = g.node(X.MaterialExpressionTransform, transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL,
               transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    g.link(g.const((0.0, 1.0, 0.0)), t)
    return t


def cloth():
    g = S.Graph("M_Sail_Cloth", dest=DEST)
    vc = g.node(X.MaterialExpressionVertexColor)
    pic = g.node(X.MaterialExpressionTextureSampleParameter2D, parameter_name="Image")
    digits = g.tparam("Digits")
    col = g.custom(SAIL_COLOUR, [("Pic", (pic, "RGB")), ("VC", vc), ("UV0", _uv(g, 0)), ("Number", _cpd(g, "Number", 2)),
                                 ("NumStbd", g.vparam("NumStbd", (0.0, 0.0, 0.0, 0.0))), ("NumPort", g.vparam("NumPort", (0.0, 0.0, 0.0, 0.0))),
                                 ("Ink", g.vparam("Ink", (0.02, 0.02, 0.025, 1.0))), ("Digits", digits)], out=F3, desc="SailColour")
    g.out(col, MP.MP_BASE_COLOR)
    g.out(g.param("Roughness", 0.7), MP.MP_ROUGHNESS)
    g.out(g.param("Specular", 0.3), MP.MP_SPECULAR)
    wpo = g.custom(SAIL_WPO, [("UV1", _uv(g, 1)), ("UV2", _uv(g, 2)), ("Camber", _cpd(g, "Camber", 0)), ("Luff", _cpd(g, "Luff", 1)),
                              ("T", g.node(X.MaterialExpressionTime)), ("Side", _side(g))], out=F3, desc="SailBelly")
    g.out(wpo, MP.MP_WORLD_POSITION_OFFSET)
    # a little light through the cloth
    try:
        g.m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
        g.out(g.const((0.25, 0.24, 0.22)), MP.MP_SUBSURFACE_COLOR)
    except Exception as e:      # noqa: BLE001
        unreal.log_warning("SAILING materials: no two-sided foliage: %s" % e)
    return g.finish()


def hull():
    g = S.Graph("M_Sail_Hull", dest=DEST)
    vc = g.node(X.MaterialExpressionVertexColor)
    tint = _cpd_vec(g, "Tint", 3)
    col = g.custom(HULL_COLOUR, [("Base", g.vparam("Color", (0.85, 0.85, 0.82, 1.0))), ("Tint", tint), ("VC", vc)], out=F3, desc="HullTint")
    g.out(col, MP.MP_BASE_COLOR)
    g.out(g.param("Roughness", 0.3), MP.MP_ROUGHNESS)
    g.out(g.param("Metallic", 0.0), MP.MP_METALLIC)
    return g.finish()


def flag(coll, name="M_Sail_Flag", picture=True):
    g = S.Graph(name, dest=DEST)
    if picture:
        pic = g.node(X.MaterialExpressionTextureSampleParameter2D, parameter_name="Image")
        g.out(pic, MP.MP_BASE_COLOR, "RGB")
    else:
        g.out(g.vparam("Color", (0.9, 0.3, 0.1, 1.0)), MP.MP_BASE_COLOR, "RGB")
    g.out(g.param("Roughness", 0.85), MP.MP_ROUGHNESS)
    wpo = g.custom(FLAG_WPO, [("UV1", _uv(g, 1)), ("T", g.node(X.MaterialExpressionTime)), ("Side", _side(g)),
                              ("Wind", g.mpc(coll, "Wind")), ("Blow", g.param("Blow", 1.0)), ("Size", g.param("Size", 90.0))],
                   out=F3, desc="FlagInWind")
    g.out(wpo, MP.MP_WORLD_POSITION_OFFSET)
    return g.finish()


def build():
    if not S.eal.does_directory_exist(DEST):
        S.eal.make_directory(DEST)
    coll = S.collection()
    out = {"cloth": cloth(), "hull": hull(), "flag": flag(coll), "windsock": flag(coll, "M_Sail_Windsock", picture=False)}
    unreal.log_warning("SAILING materials: masters built")
    return out


if __name__ == "__main__":
    build()
