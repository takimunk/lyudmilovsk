"""The wind of the trees' materials (M_PlantBark, M_PlantLeaf, M_PlantNeedle): the world's wind
(MPC_LyudWeather: Wind = dir x, dir y, speed m/s, gustiness; WindDrift = the gust field's drift and clock, see
tools/sea/sea_hlsl.py) bends a tree and shakes its twigs and leaves in proportion to its strength.

  local   = (speed / 7 m/s)^1.5 x the gust at this place, at most 4.5: 1 is the breezy afternoon the species'
            SwayAmp is given for, 3 a gale
  lean    the whole tree leans downwind, Bend x height^2 x local^2 x 0.55 cm, breathing with the gusts, at most ~16 degrees (the top of a 12 m
            tree with Bend 0.13 at 7 m/s: 10 cm; at 20 m/s: 2.5 m, a gale)
  sway    twigs and leaf clusters swing downwind and a little across it, W (= SwayAmp x the vertex's weight) x local
            cm, faster in a stronger wind, in a wave that travels with the wind across the crown
  flutter the leaves' own shiver grows with the wind (a rustle), see FLUTTER_HLSL
Past ~90 m from the camera the motion fades out (it cannot be seen and costs the vertex shader).

Material parameters: Bend (species), FootFromActor (1 when the mesh instances are a tree's leaf clusters, so the tree's
foot is the actor's place; 0 when the instance is the tree itself), SwayAmp, Flutter.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "sea"))
import sea_hlsl  # noqa: E402

mel = unreal.MaterialEditingLibrary

WIND_BODY = """
float fade = 1.0 - saturate((length(P - Cam) - 4000.0) / 5000.0);
if (fade <= 0.0) return float3(0, 0, 0);
float spd = Wd.z;
float2 dir = dot(Wd.xy, Wd.xy) > 1e-4 ? normalize(Wd.xy) : float2(1, 0);
float2 side = float2(-dir.y, dir.x);
// the gust field drifts with the wind (WindDrift = x, y m, clock s): two cheap waves instead of the sea's 3-octave noise
float2 q = (P.xy * 0.01 - Dr.xy);
float c = Dr.z * 0.1;
float gust = clamp(1.0 + Wd.w * 1.4 * (0.7 * sin(q.x * 0.011 + q.y * 0.0083 + c * 0.9) + 0.5 * sin(q.x * 0.043 - q.y * 0.037 + c * 2.1)), 0.15, 2.4);
float x = max(spd, 0.0) * (1.0 / 7.0);
float local = min(x * sqrt(x) * gust, 4.5);   // ~ x^1.5: close enough to the 1.15 power the doc says, no pow()
float3 foot = FootFromActor > 0.5 ? Actor : O;
float h = max(P.z - foot.z, 0.0) * 0.01;
// the lean breathes with the gusts (0.75 .. 1.25 of its mean)
float lean = Bend * h * h * local * local * 0.55 * (1.0 + 0.25 * sin(T * 0.7 + dot(P.xy, dir) * 0.004));
lean = min(lean, h * 28.0);   // never more than ~16 degrees off the vertical at any height
float ph = T * (1.0 + 0.45 * local) + dot(P.xy, dir) * 0.011;
float osc = 0.65 * sin(ph) + 0.35 * sin(ph * 1.73 + 1.3);
float sway = W * local * (0.6 + 0.8 * osc);
float across = 0.4 * W * local * sin(ph * 1.31 + P.x * 0.02);
float3 wpo = float3(dir * (lean + sway) + side * across, -0.1 * abs(sway) - lean * lean * 0.002);
return wpo * fade;
"""
WIND_HLSL = WIND_BODY

FLUTTER_HLSL = """
float k = 0.25 + 0.9 * saturate(Wd.z / 9.0);
float f = 1.0 + 0.8 * saturate(Wd.z / 12.0);
return float3(0.4 * sin(T * 5.3 * f + R * 17.0), 0.3 * sin(T * 6.1 * f + R * 29.0), 0.6 * sin(T * 7.0 * f + R * 40.0)) * F * k;
"""


def _in(n):
    c = unreal.CustomInput()
    c.set_editor_property("input_name", n)
    return c


def wind(m, weight, x, y, node, link, scalar):
    """World position offset node: the tree wind at this vertex, W = the expression `weight` (SwayAmp already in)."""
    coll = unreal.load_asset("/Game/Weather/MPC_LyudWeather")
    C = unreal.MaterialExpressionCollectionParameter
    pos = node(m, unreal.MaterialExpressionWorldPosition, x - 700, y,
               world_position_shader_offset=unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    obj = node(m, unreal.MaterialExpressionObjectPositionWS, x - 700, y + 100)
    act = node(m, unreal.MaterialExpressionActorPositionWS, x - 700, y + 200)
    time = node(m, unreal.MaterialExpressionTime, x - 700, y + 300)
    wd = node(m, C, x - 700, y + 400, collection=coll, parameter_name="Wind")
    dr = node(m, C, x - 700, y + 500, collection=coll, parameter_name="WindDrift")
    cam = node(m, unreal.MaterialExpressionCameraPositionWS, x - 700, y + 600)
    names = ("P", "O", "Actor", "T", "Wd", "Dr", "Cam", "W", "Bend", "FootFromActor")
    c = node(m, unreal.MaterialExpressionCustom, x, y, code=WIND_HLSL, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
             description="TreeWind", inputs=[_in(n) for n in names])
    link(pos, "", c, "P")
    link(obj, "", c, "O")
    link(act, "", c, "Actor")
    link(time, "", c, "T")
    link(wd, "", c, "Wd")
    link(dr, "", c, "Dr")
    link(cam, "", c, "Cam")
    link(weight, "", c, "W")
    link(scalar(m, "Bend", x - 700, y + 700, 0.1), "", c, "Bend")
    link(scalar(m, "FootFromActor", x - 700, y + 800, 0.0), "", c, "FootFromActor")
    return c
