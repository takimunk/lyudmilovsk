"""HLSL of the rain and fog effects' materials (unreal/scripts/make_rainfog_materials.py builds them; the sea's rain
rings go into M_LyudSea through make_sea_materials.py). ALyudRainFx (Private/Weather/LyudRainFog.cpp) feeds the
parameters; keep the names in step with it.

Parameters (vectors x, y, z, w):
  drops   Box    = box side cm, box height cm, fall speed cm/s, share of streaks shown
          Wind   = wind x, y cm/s, streak exposure s, drop width cm
          RoofP  = roof map origin x, y cm, cell cm, cells per side;  Roof = the map (r top height cm, g 1 on the sea)
  splash  Box    = box side cm, share shown, period s, size cm
  wet     Wet    = wetness 0..1, puddles 0..1, ripples 0..1, -
"""

# the roof map: the top height the rain meets over a world point (r) and whether it is the sea (g)
ROOF_LIB = r"""
struct FRoof
{
    float2 At(float2 xy, float4 P, Texture2D Tex, SamplerState S, out float inside)
    {
        float2 uv = (xy - P.xy) / (P.z * P.w);
        inside = (uv.x > 0.0 && uv.y > 0.0 && uv.x < 1.0 && uv.y < 1.0) ? 1.0 : 0.0;
        return Tex.SampleLevel(S, uv, 0).rg;
    }
};
FRoof RoofMap;
"""

# one streak: where it is (wrapped in its box around the camera, so it stays put in the world as the camera moves),
# slanted along its velocity, a camera-facing ribbon at least a pixel wide (fainter when it is thinner than that)
DROP_BODY = ROOF_LIB + r"""
float3 B = float3(Box.x, Box.x, Box.y);
float size = R2.y;
float fall = Box.z * (0.75 + 0.5 * size);
float keep = frac(R1.x * 13.37 + R1.y * 7.71 + R2.x * 3.13);
float3 vel = float3(Wind.x, Wind.y, -fall) * (0.85 + 0.3 * frac(size * 7.7));
float3 p0 = float3(R1.x, R1.y, R2.x) * B + vel * fmod(T, 3600.0);
float3 rel = (frac((p0 - Cam) / B + 0.5) - 0.5) * B;
float3 w = Cam + rel;
float a = keep < Box.w ? 1.0 : 0.0;
float inside;
float2 roof = RoofMap.At(w.xy, RoofP, Roof, RoofSampler, inside);
if (inside > 0.5 && w.z < roof.r) a = 0.0;
float dist = length(rel);
a *= saturate((dist - 35.0) / 60.0);
float3 e = abs(rel) / (0.5 * B);
a *= saturate((1.0 - max(e.x, max(e.y, e.z))) * 5.0);
float3 ax = normalize(vel);
float3 side = normalize(cross(ax, rel / max(dist, 1.0)) + float3(1e-4, 0.0, 0.0));
float thick = Wind.w * (0.6 + 0.8 * size);
float width = max(thick, dist * 0.0014);
a *= thick / width;
float len = length(vel) * Wind.z;
float3 off = side * (C.x - 0.5) * width + ax * (C.y - 0.5) * len;
"""
DROP_WPO = DROP_BODY + "return (a > 0.001 ? w + off : w) - Pos;"
DROP_ALPHA = DROP_BODY + "return a;"

# a splash where a drop hits: a ring spreading on whatever the roof map says the rain meets (not the sea, whose
# material rings itself), born again somewhere else every period
SPLASH_BODY = ROOF_LIB + r"""
float per = Box.z * (0.7 + 0.6 * R2.y);
float t = fmod(T, 3600.0) / per + R2.x * 17.0;
float cyc = floor(t);
float life = t - cyc;
float2 j = frac(sin(float2(cyc * 12.9898 + R1.x * 78.233, cyc * 39.346 + R1.y * 11.135)) * 43758.5453);
float2 q = frac(R1 + j) * Box.x;
float2 rel = (frac((q - Cam.xy) / Box.x + 0.5) - 0.5) * Box.x;
float2 xy = Cam.xy + rel;
float a = frac(R1.x * 5.31 + R1.y * 9.17 + j.x) < Box.y ? 1.0 : 0.0;
float inside;
float2 roof = RoofMap.At(xy, RoofP, Roof, RoofSampler, inside);
if (inside < 0.5 || roof.r < -1.0e6 || roof.g > 0.5) a = 0.0;
float d = length(float3(rel, roof.r - Cam.z));
a *= saturate(1.0 - d / (0.5 * Box.x)) * saturate((d - 30.0) / 50.0);
float r = Box.w * lerp(1.5, 7.0, sqrt(life)) * (0.6 + 0.8 * R2.y);
float3 target = float3(xy + (C - 0.5) * 2.0 * r, roof.r + 1.5);
"""
SPLASH_WPO = SPLASH_BODY + "return (a > 0.001 ? target : float3(xy, roof.r)) - Pos;"
SPLASH_LIFE = SPLASH_BODY + "return float2(a, life);"

# rain rings on still water: cells of RingCell, each now and then hit, a ring running out and fading;
# returns the normal's xy tilt
RINGS_LIB = r"""
struct FRings
{
    float2 Tilt(float2 p, float t, float amount)
    {
        float2 cell = floor(p);
        float2 f = p - cell;
        float2 n = 0.0;
        for (int i = -1; i <= 1; i++)
        for (int k = -1; k <= 1; k++)
        {
            float2 c = cell + float2(i, k);
            float3 h = frac(sin(float3(dot(c, float2(127.1, 311.7)), dot(c, float2(269.5, 183.3)), dot(c, float2(419.2, 371.9)))) * 43758.5453);
            if (h.z > amount) continue;
            float ph = frac(t * (0.9 + 0.5 * h.x) + h.z * 7.0);
            float2 d = f - (float2(i, k) + h.xy);
            float r = length(d);
            float wv = r - ph * 0.9;
            float amp = (1.0 - ph) * (1.0 - ph) * exp(-wv * wv * 140.0);
            n += d / max(r, 1e-3) * sin(wv * 45.0) * amp;
        }
        return n;
    }
};
FRings Rings;
"""

# the wet ground decal: darker and smoother where the sky rains on it (the roof map), puddles on the flat with rings
WET_HLSL = ROOF_LIB + RINGS_LIB + r"""
float3 dx = ddx(P), dy = ddy(P);
float3 n = normalize(cross(dx, dy));
if (dot(n, V) < 0.0) n = -n;
float flat = smoothstep(0.86, 0.97, n.z);
float inside;
float2 roof = RoofMap.At(P.xy, RoofP, Roof, RoofSampler, inside);
float exposed = inside * step(roof.r - 30.0, P.z);
float2 uvm = (P.xy - RoofP.xy) / (RoofP.z * RoofP.w);
float2 edge = min(uvm, 1.0 - uvm);
exposed *= saturate(min(edge.x, edge.y) * 12.0);
// puddles: two octaves of value noise over the ground, more of them the wetter it is
float2 g = P.xy / 260.0;
float2 gi = floor(g), gf = g - gi;
gf = gf * gf * (3.0 - 2.0 * gf);
float h00 = frac(sin(dot(gi, float2(127.1, 311.7))) * 43758.5453);
float h10 = frac(sin(dot(gi + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
float h01 = frac(sin(dot(gi + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);
float h11 = frac(sin(dot(gi + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);
float nz = lerp(lerp(h00, h10, gf.x), lerp(h01, h11, gf.x), gf.y);
float2 g2 = P.xy / 70.0;
float2 g2i = floor(g2), g2f = g2 - g2i;
g2f = g2f * g2f * (3.0 - 2.0 * g2f);
float k00 = frac(sin(dot(g2i, float2(269.5, 183.3))) * 43758.5453);
float k10 = frac(sin(dot(g2i + float2(1, 0), float2(269.5, 183.3))) * 43758.5453);
float k01 = frac(sin(dot(g2i + float2(0, 1), float2(269.5, 183.3))) * 43758.5453);
float k11 = frac(sin(dot(g2i + float2(1, 1), float2(269.5, 183.3))) * 43758.5453);
nz = nz * 0.7 + 0.3 * lerp(lerp(k00, k10, g2f.x), lerp(k01, k11, g2f.x), g2f.y);
float th = 1.0 - 0.42 * Wet.y;
float puddle = smoothstep(th, th + 0.05, nz) * flat * step(0.01, Wet.y);
float2 tilt = Rings.Tilt(P.xy / 22.0, T, Wet.z) * 0.5 * Wet.z * puddle;
float3 nn = normalize(n + float3(tilt, 0.0));
float op = lerp(Wet.x * lerp(0.32, 0.55, flat), 0.85, puddle) * exposed;
Rough = lerp(0.16, 0.02, puddle);
Nrm = nn;
return op;
"""

# the sea's rings: added to the waves' normal in M_LyudSea where it rains (MPC_LyudWeather.Rain: x amount, y drizzle)
SEA_RINGS = RINGS_LIB + r"""
float2 a = N.xy;
if (Rain.x > 0.001)
{
    a += Rings.Tilt(XY / lerp(0.3, 0.12, Rain.y), T, Rain.x) * 0.35 * Rain.x;
}
return float3(a, sqrt(saturate(1.0 - dot(a, a))));
"""
