"""The festivals' materials, committed under /Game/Festival (small; docs/festivals.md "Cloth in the wind", "Poplar fluff"):

  M_FestivalCloth  pennants, flags and ribbons moved by the wind in the vertex shader (FLyudCloth, FestivalParts.h):
                   a pennant's tip swings out downwind on the arc of its own length, a flag turns downwind and droops
                   in a calm, everything ripples harder the harder it blows. Reads /Game/Weather/MPC_LyudWeather
                   (Wind, WindDrift: ULyudWindSubsystem), so the whole town's bunting follows the day's real wind.
  M_PoplarFluff    the poplars' fluff (ULyudPoplarFluffSubsystem): a box of tufts that lives round the camera, drifting
                   with the wind's own drift and sinking slowly, wrapped at the box's faces; nothing moves on the CPU.

Run headless:
  UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/make_festival_materials.py -unattended
"""
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_sea_materials as S  # noqa: E402

DEST = "/Game/Festival"
X = unreal
MP = unreal.MaterialProperty
F1, F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT1, unreal.CustomMaterialOutputType.CMOT_FLOAT3

# UV = (free 0..1, phase 0..1), P1 = (hang cm, flutter cm), P2 = (arm cm, 0); Wind = (dir x, dir y, m/s, gustiness)
CLOTH_WPO = r"""
float sp = Wind.z * Blow;
float k = saturate(sp / 10.0);
float ph = UV.y * 6.2831853;
float2 d = Wind.xy;
// slow gusts riding downwind over the town
float gust = 0.5 + 0.5 * sin(dot(Pos.xy, d) * 0.004 - T * (0.6 + 0.12 * sp) + ph * 0.3);
float blow = k * (0.55 + 0.45 * gust);
// what hangs swings out downwind and up, on the arc of its own length
float th = (0.10 + 1.15 * blow) + 0.22 * (0.3 + blow) * sin(T * (3.0 + 9.0 * k) + ph * 5.0);
float3 off = float3(d * sin(th), 1.0 - cos(th)) * P1.x;
// a flag (built along +X) turns downwind and droops in a calm
float fly = saturate(sp / 5.0) * (0.6 + 0.4 * gust);
float3 dirNow = normalize(float3(d * (0.25 + 0.75 * fly), -(1.0 - fly) * 0.95));
off += (dirNow - float3(1.0, 0.0, 0.0)) * P2.x;
// the ripple: a wave running out along the cloth, across it
float3 side = P2.x > 0.0 ? float3(-d.y, d.x, 0.0) : normalize(N);
float w = sin(T * (6.0 + 14.0 * k) - UV.x * 5.0 + ph) + 0.5 * sin(T * (11.0 + 9.0 * k) + UV.x * 9.0 + ph * 1.7);
off += side * (P1.y * (0.25 + 0.75 * blow) * w * 0.6);
return off;
"""

# C = corner (-1..1), P1 = seed xy, P2 = (seed z, size cm), P3 = (phase, threshold);
# Fluff = (box xy cm, box z cm, amount 0..1, sink cm/s); WindDrift.xy = how far the air has drifted, m (wraps at 4096 m:
# the box's 64 m divides it, so the wrap never shows)
FLUFF_WPO = r"""
float3 box = float3(Fluff.x, Fluff.x, Fluff.y);
float ph = P3.x * 6.2831853;
float3 wander = float3(sin(T * 0.31 + ph) * 70.0 + sin(T * 0.93 + ph * 3.0) * 18.0,
                       cos(T * 0.27 + ph * 1.7) * 70.0 + cos(T * 1.07 + ph * 2.3) * 18.0,
                       sin(T * 0.45 + ph * 2.9) * 35.0);
float3 home = float3(P1, P2.x) * box + float3(WindDrift.xy * 100.0, -T * Fluff.w * (0.6 + 0.8 * P3.y)) + wander;
float3 rel = frac((home - Cam) / box);
float3 p = Cam + (rel - 0.5) * box;
// nothing pops at the box's faces
float3 e = saturate(min(rel, 1.0 - rel) * 7.0);
float alive = (P3.y < Fluff.z ? 1.0 : 0.0) * e.x * e.y * e.z;
float3 view = normalize(p - Cam);
float3 right = cross(view, float3(0.0, 0.0, 1.0));
right = length(right) > 1e-3 ? normalize(right) : float3(1.0, 0.0, 0.0);
float3 up = cross(right, view);
float spin = T * (0.4 + P3.x) + ph;
float2 c = float2(C.x * cos(spin) - C.y * sin(spin), C.x * sin(spin) + C.y * cos(spin));
return p + (right * c.x + up * c.y) * P2.y * alive - Pos;
"""

FLUFF_MASK = r"""
// a ragged tuft: a soft core with wisps
float r = length(C);
float a = atan2(C.y, C.x);
float wisp = 0.72 + 0.28 * sin(a * 5.0 + P3.x * 40.0) * sin(a * 3.0 - P3.x * 17.0);
return saturate((wisp - r) * 4.0);
"""


def _uv(g, i):
    return g.node(X.MaterialExpressionTextureCoordinate, coordinate_index=i)


def _props(g, **props):
    for k, v in props.items():
        try:
            g.m.set_editor_property(k, v)
        except Exception as e:  # noqa: BLE001
            unreal.log_warning("FESTIVAL MATERIALS %s: %s" % (k, e))


def cloth(coll):
    g = S.Graph("M_FestivalCloth", dest=DEST)
    _props(g, two_sided=True)
    pos = g.node(X.MaterialExpressionWorldPosition, world_position_shader_offset=S.EXCL)
    ins = [("UV", _uv(g, 0)), ("P1", _uv(g, 1)), ("P2", _uv(g, 2)), ("Pos", pos),
           ("N", g.node(X.MaterialExpressionVertexNormalWS)), ("T", g.node(X.MaterialExpressionTime)),
           ("Wind", g.mpc(coll, "Wind")), ("Blow", g.param("Blow", 1.0))]
    g.out(g.custom(CLOTH_WPO, ins, out=F3, desc="ClothInWind"), MP.MP_WORLD_POSITION_OFFSET)
    col = g.custom("return float3(RG, B.x);", [("RG", _uv(g, 3)), ("B", _uv(g, 4))], out=F3, desc="ClothColour")
    g.out(col, MP.MP_BASE_COLOR)
    g.out(g.const(0.85), MP.MP_ROUGHNESS)
    g.out(g.const(0.2), MP.MP_SPECULAR)
    return g.finish()


def fluff(coll):
    g = S.Graph("M_PoplarFluff", dest=DEST)
    _props(g, two_sided=True, blend_mode=unreal.BlendMode.BLEND_MASKED, opacity_mask_clip_value=0.35)
    pos = g.node(X.MaterialExpressionWorldPosition, world_position_shader_offset=S.EXCL)
    cam = g.node(X.MaterialExpressionCameraPositionWS)
    t = g.node(X.MaterialExpressionTime)
    ins = [("C", _uv(g, 0)), ("P1", _uv(g, 1)), ("P2", _uv(g, 2)), ("P3", _uv(g, 3)), ("Pos", pos), ("Cam", cam), ("T", t),
           ("Fluff", g.vparam("Fluff", (6400.0, 2400.0, 0.0, 9.0))), ("WindDrift", g.mpc(coll, "WindDrift"))]
    g.out(g.custom(FLUFF_WPO, ins, out=F3, desc="FluffDrift"), MP.MP_WORLD_POSITION_OFFSET)
    g.out(g.custom(FLUFF_MASK, [("C", _uv(g, 0)), ("P3", _uv(g, 3))], desc="FluffTuft"), MP.MP_OPACITY_MASK)
    g.out(g.const((0.9, 0.9, 0.86)), MP.MP_BASE_COLOR)
    g.out(g.const(0.95), MP.MP_ROUGHNESS)
    g.out(g.const(0.1), MP.MP_SPECULAR)
    return g.finish()


def build():
    if not S.eal.does_directory_exist(DEST):
        S.eal.make_directory(DEST)
    coll = S.collection()
    cloth(coll)
    fluff(coll)
    unreal.log_warning("FESTIVAL MATERIALS done")


build()
