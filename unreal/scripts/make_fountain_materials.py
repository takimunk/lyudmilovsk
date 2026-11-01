"""The Academgorodok fountain's materials, committed under /Game/Academgorodok/Fountain (small; docs/fountain.md):

  M_FountainWater   single-layer water of both basins: ripples running from where the water lands, a ring of foam there
  M_FountainStream  the jet, its bell and the veil: lit translucent sheets whose streaks ride with the water (the
                    meshes carry the flow time), tearing into strands as they fall
  M_FountainDrops   splashes and drips: quads moved along their parabolas and turned to the camera in the vertex shader
  M_FountainLeaves  the drained basins' floor: a damp stain and drifts of fallen leaves

ALyudFountain (Private/World/LyudFountain.cpp) builds the meshes and sets the parameters. Run headless:
  UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/make_fountain_materials.py -unattended
"""
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_sea_materials as S  # noqa: E402

DEST = "/Game/Academgorodok/Fountain"
X = unreal
MP = unreal.MaterialProperty
F1, F2, F3 = (unreal.CustomMaterialOutputType.CMOT_FLOAT1, unreal.CustomMaterialOutputType.CMOT_FLOAT2,
              unreal.CustomMaterialOutputType.CMOT_FLOAT3)

# value noise; Wrap(p, period) repeats every Period cells along x (the sheets go round: U runs 0..strands)
NOISE = r"""
struct FFountainNoise
{
    float H(float2 c) { return frac(sin(dot(c, float2(127.1, 311.7))) * 43758.5453); }
    float N(float2 p)
    {
        float2 i = floor(p), f = frac(p);
        f = f * f * (3.0 - 2.0 * f);
        return lerp(lerp(H(i), H(i + float2(1, 0)), f.x), lerp(H(i + float2(0, 1)), H(i + float2(1, 1)), f.x), f.y);
    }
    float Wrap(float2 p, float Period)
    {
        float2 i = floor(p), f = frac(p);
        f = f * f * (3.0 - 2.0 * f);
        float x0 = fmod(i.x, Period), x1 = fmod(i.x + 1.0, Period);
        return lerp(lerp(H(float2(x0, i.y)), H(float2(x1, i.y)), f.x), lerp(H(float2(x0, i.y + 1)), H(float2(x1, i.y + 1)), f.x), f.y);
    }
};
FFountainNoise Nz;
"""

# Ring = (where the water lands m, how hard 0..1, foam 0..1, basin radius m); UV = metres from the fountain's axis
WATER_NORMAL = NOISE + r"""
float2 p = UV;
float r = max(length(p), 1e-4);
float2 dir = p / r;
float d = r - Ring.x;
// capillary ripples (5 cm, ~0.3 m/s) running both ways from the landing ring, broken up, dying out over ~20 cm;
// a second train off the basin's wall
float k = 6.2831853 / 0.05;
float ph = k * abs(d) - k * 0.3 * T + 3.0 * Nz.N(p * 14.0 + T * 0.25);
float g = Ring.y * 0.05 * exp(-abs(d) * 10.0) * cos(ph) * sign(d);
float dw = Ring.w - r;
g -= Ring.y * 0.02 * exp(-dw * 18.0) * cos(k * dw - k * 0.3 * T + 1.7);
float2 grad = dir * g;
// the churn where the water pours in
float churn = exp(-d * d / (0.045 * 0.045)) * Ring.y;
grad += churn * 0.55 * (float2(Nz.N(p * 70.0 + T * 5.0), Nz.N(p * 70.0 + 19.0 - T * 5.0)) - 0.5);
// a breath of wind everywhere
grad += 0.012 * float2(sin(p.x * 70.0 + p.y * 23.0 + T * 3.1), sin(p.y * 64.0 - p.x * 17.0 - T * 2.6));
return normalize(float3(-grad, 1.0));
"""

WATER_FOAM = NOISE + r"""
float d = length(UV) - Ring.x;
// not a drawn ring: a ragged band where the strands land, wider outside (the bubbles carried out), in patches
float w = d > 0.0 ? 0.06 : 0.035;
float band = exp(-d * d / (w * w)) * (0.55 + 0.45 * Nz.N(float2(atan2(UV.y, UV.x) * 9.0, T * 0.6)));
float n = 0.6 * Nz.N(UV * 55.0 + float2(T * 1.3, -T * 0.9)) + 0.4 * Nz.N(UV * 140.0 - T * 2.3);
// bubbles drift a little outward and pop
float drift = exp(-max(d, 0.0) * 14.0) * step(0.0, d) * 0.35 * Nz.N(UV * 90.0 + T * 0.7);
return saturate((band * smoothstep(0.45, 0.85, n + band * 0.15) + drift * smoothstep(0.6, 0.85, n)) * Ring.z * 0.7);
"""

STREAM_OPACITY = NOISE + r"""
// UV = (round the sheet in strands, the water's flight time s), A = (0..1 along the sheet, kind: 0 jet 1 bell 2 veil)
float strands = A.y < 0.5 ? 3.0 : (A.y < 1.5 ? 22.0 : 56.0);
float tf = UV.y - T;                               // the pattern rides with the water
float n = 0.65 * Nz.Wrap(float2(UV.x, tf * 9.0), strands) + 0.35 * Nz.Wrap(float2(UV.x * 2.0, tf * 17.0 + 3.1), strands * 2.0);
// a sheet thins and tears into strands as it falls; the jet's column stays whole
float thr = A.y < 0.5 ? 0.05 : lerp(A.y < 1.5 ? 0.2 : 0.12, A.y < 1.5 ? 0.62 : 0.55, A.x);
float strand = smoothstep(thr, thr + 0.2, n);
float facing = abs(dot(normalize(N), normalize(V)));
float edge = 1.0 - facing;
float op = strand * (0.3 + 0.55 * edge * edge);
// a glassy start at the lip, softer where it meets the water
op *= lerp(0.55, 1.0, smoothstep(0.0, 0.15, A.x)) * (1.0 - 0.4 * smoothstep(0.85, 1.0, A.x));
return saturate(op);
"""

STREAM_NORMAL = NOISE + r"""
float strands = A.y < 0.5 ? 3.0 : (A.y < 1.5 ? 22.0 : 56.0);
float tf = UV.y - T;
float e = 0.06;
float a = Nz.Wrap(float2(UV.x + e, tf * 9.0), strands), b = Nz.Wrap(float2(UV.x - e, tf * 9.0), strands);
float c = Nz.Wrap(float2(UV.x, (tf + 0.01) * 9.0), strands), d = Nz.Wrap(float2(UV.x, (tf - 0.01) * 9.0), strands);
return normalize(float3((a - b) * 2.5, (c - d) * 0.8, 1.0));
"""

STREAM_COLOR = NOISE + r"""
// clear near the nozzle and the lip, whiter as air gets into it
float tf = UV.y - T;
float n = Nz.N(float2(UV.x * 1.7, tf * 13.0));
float air = saturate(A.x * 0.9 + 0.25 * n);
return lerp(float3(0.16, 0.2, 0.21), float3(0.72, 0.75, 0.76), air);
"""

# UV0 corner, UV1 (vx, vy), UV2 (vz, seed), UV3 (period, life), UV4 (radius) - metres, seconds
DROP_WPO = r"""
float period = max(P3.x, 0.01);
float t = frac(T / period + P2.y) * period;
float3 v0 = float3(P1, P2.x);
float3 g = float3(0.0, 0.0, -9.81);
float3 off = (v0 * t + 0.5 * g * t * t) * 100.0;
float3 vel = v0 + g * t;
float alive = t < P3.y ? 1.0 : 0.0;
float3 dir = normalize(Pos + off - Cam);
float sp = length(vel);
float3 ax = sp > 1e-3 ? vel / sp : float3(0.0, 0.0, 1.0);
float3 side = cross(ax, dir);
side = length(side) > 1e-4 ? normalize(side) : float3(1.0, 0.0, 0.0);
// stretched along its path by about what it travels in a frame
float len = P4.x + sp * 0.006;
float3 q = (side * C.x * P4.x + ax * C.y * len) * 100.0;
return alive * (off + q);
"""


def _uv(g, i):
    return g.node(X.MaterialExpressionTextureCoordinate, coordinate_index=i)


def _translucent_lit(g):
    m = g.m
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    for k, v in (("used_with_static_lighting", False),):
        try:
            m.set_editor_property(k, v)
        except Exception as e:  # noqa: BLE001
            unreal.log_warning("FOUNTAIN MATERIALS %s: %s" % (k, e))


def water():
    g = S.Graph("M_FountainWater", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER, dest=DEST)
    ring = g.vparam("Ring", (0.45, 1.0, 1.0, 0.86))
    t = g.node(X.MaterialExpressionTime)
    ins = [("UV", _uv(g, 0)), ("T", t), ("Ring", ring)]
    nrm = g.custom(WATER_NORMAL, ins, out=F3, desc="FountainRipples")
    foam = g.custom(WATER_FOAM, ins, desc="FountainFoam")
    lerp = g.node(X.MaterialExpressionLinearInterpolate)
    g.link(nrm, lerp, "A")
    g.link(g.const((0.0, 0.0, 1.0)), lerp, "B")
    g.link(g.mul(foam, g.const(0.5)), lerp, "Alpha")
    g.out(lerp, MP.MP_NORMAL)
    g.out(g.add(g.const(0.03), g.mul(foam, g.const(0.6))), MP.MP_ROUGHNESS)
    g.out(g.const((0.8, 0.82, 0.82)), MP.MP_BASE_COLOR)
    g.out(g.mul(foam, g.const(0.6)), MP.MP_OPACITY)
    g.out(g.const(0.5), MP.MP_SPECULAR)
    wo = g.node(X.MaterialExpressionSingleLayerWaterMaterialOutput, x=0)
    # per metre x 0.01 (the output wants them per cm, see make_sea_materials.py): a few cm of water over white
    # stone, a touch of green from the basin's algae
    for prop, value in ((("Scattering Coefficients", "ScatteringCoefficients"), (0.0002, 0.0003, 0.00025)),
                        (("Absorption Coefficients", "AbsorptionCoefficients"), (0.014, 0.0045, 0.0055)),
                        (("Phase G", "PhaseG"), 0.3), (("Color Scale Behind Water", "ColorScaleBehindWater"), 0.85)):
        src = g.const(value)
        if not any(S.mel.connect_material_expressions(src, "", wo, pin) for pin in prop):
            raise RuntimeError("water output: no pin " + prop[0])
    try:
        g.out(g.const(1.33), MP.MP_REFRACTION)
    except RuntimeError:
        unreal.log_warning("FOUNTAIN MATERIALS no refraction pin on single-layer water")
    return g.finish()


def stream():
    g = S.Graph("M_FountainStream", dest=DEST)
    _translucent_lit(g)
    t = g.node(X.MaterialExpressionTime)
    n = g.node(X.MaterialExpressionVertexNormalWS)
    v = g.node(X.MaterialExpressionCameraVectorWS)
    ins = [("UV", _uv(g, 0)), ("A", _uv(g, 1)), ("T", t)]
    g.out(g.custom(STREAM_OPACITY, ins + [("N", n), ("V", v)], desc="StreamOpacity"), MP.MP_OPACITY)
    g.out(g.custom(STREAM_NORMAL, ins, out=F3, desc="StreamNormal"), MP.MP_NORMAL)
    g.out(g.custom(STREAM_COLOR, ins, out=F3, desc="StreamColor"), MP.MP_BASE_COLOR)
    g.out(g.const(0.06), MP.MP_ROUGHNESS)
    g.out(g.const(0.7), MP.MP_SPECULAR)
    return g.finish()


def drops():
    g = S.Graph("M_FountainDrops", dest=DEST)
    _translucent_lit(g)
    pos = g.node(X.MaterialExpressionWorldPosition, world_position_shader_offset=S.EXCL)
    cam = g.node(X.MaterialExpressionCameraPositionWS)
    t = g.node(X.MaterialExpressionTime)
    ins = [("C", _uv(g, 0)), ("P1", _uv(g, 1)), ("P2", _uv(g, 2)), ("P3", _uv(g, 3)), ("P4", _uv(g, 4)),
           ("Pos", pos), ("Cam", cam), ("T", t)]
    g.out(g.custom(DROP_WPO, ins, out=F3, desc="DropFlight"), MP.MP_WORLD_POSITION_OFFSET)
    op = g.custom("float r2 = dot(C, C); return saturate(1.0 - r2) * 0.8;", [("C", _uv(g, 0))], desc="DropShape")
    g.out(op, MP.MP_OPACITY)
    g.out(g.const((0.75, 0.78, 0.8)), MP.MP_BASE_COLOR)
    g.out(g.const(0.1), MP.MP_ROUGHNESS)
    g.out(g.const(0.8), MP.MP_SPECULAR)
    return g.finish()


LEAVES = NOISE + r"""
float2 p = UV + Seed * 0.013;
float n = 0.55 * Nz.N(p * 6.0) + 0.3 * Nz.N(p * 17.0) + 0.15 * Nz.N(p * 41.0);
// leaves drift against the wall and round the stem
float r = length(UV);
float lean = smoothstep(Edge.x, Edge.y, r) * 0.25 + (1.0 - smoothstep(Edge.z, Edge.z + 0.05, r)) * 0.15;
return smoothstep(0.5, 0.62, n + lean);
"""


def leaves():
    g = S.Graph("M_FountainLeaves", dest=DEST)
    seed = g.param("Seed", 0.0)
    edge = g.vparam("Edge", (0.35, 0.6, 0.1, 0.0))
    uv = _uv(g, 0)
    amt = g.custom(LEAVES, [("UV", uv), ("Seed", seed), ("Edge", edge)], desc="LeafDrift")
    tex = g.node(X.MaterialExpressionTextureSample, texture=unreal.load_asset("/Game/ForestFloor/T_Leaves_D"))
    g.link(g.mul(uv, g.const(2.2)), tex, "UVs")
    stain = g.custom(NOISE + "float n = Nz.N(UV * 9.0) * 0.5 + Nz.N(UV * 31.0) * 0.5;"
                     " return lerp(float3(0.36, 0.34, 0.3), float3(0.25, 0.24, 0.2), n);",
                     [("UV", uv)], out=F3, desc="Stain")
    col = g.node(X.MaterialExpressionLinearInterpolate)
    g.link(stain, col, "A")
    g.link(g.mul(tex, g.const((0.62, 0.5, 0.4))), col, "B")
    g.link(amt, col, "Alpha")
    g.out(col, MP.MP_BASE_COLOR)
    g.out(g.const(0.8), MP.MP_ROUGHNESS)
    return g.finish()


def build():
    if not S.eal.does_directory_exist(DEST):
        S.eal.make_directory(DEST)
    for f in (water, stream, drops, leaves):
        f()
    unreal.log_warning("FOUNTAIN MATERIALS done")


build()
