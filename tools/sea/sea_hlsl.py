"""HLSL for the sea's and the wind's materials (unreal/scripts/make_sea_materials.py builds them).

These mirror the C++ in unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Weather (WindModel.cpp GustAt,
WaveModel.cpp Displace / SurfaceAt): keep both in step. The wave trains themselves are not here: ALyudSea
writes them into a 128 x 2 float texture (WaveTable), geometry trains in columns 0..63, detail trains in
64..127, row 0 = (dir x, dir y, k rad/m, amplitude m), row 1 = (omega rad/s, phase rad, wavelength m, 0),
sorted longest first, so a loop can stop at the first train too short to draw.

Parameters (ALyudSea sets them on its dynamic material):
  SeaClock  = (wave clock s wrapped to 1200, ring segments, finest mesh step m, open-sea depth m)
  SeaParams = (Gerstner choppiness, significant height m, geometry trains, detail trains)
  DepthRect = (min x m, min y m, 1/size x, 1/size y) of DepthMap (still-water depth m; land < 0)
  LandSideX = world X (m) east of which there is no sea
and from /Game/Weather/MPC_LyudWeather: Wind = (dir x, dir y, speed m/s, gustiness),
WindDrift = (drift x m, drift y m, gust clock s, day stir).
"""

# Value noise, the gust field (WindModel.cpp: Hash, ValueNoise3, GustAt) and the depth map lookup
# (ALyudSea::DepthAtM). Pasted at the head of a Custom node's code; called as Lyud.Gust(...) etc.
GUST_LIB = r"""
struct FLyudNoise
{
    float Hash(uint3 p, uint s)
    {
        uint h = p.x * 0x8da6b343u ^ p.y * 0xd8163841u ^ p.z * 0xcb1ab31fu ^ s * 0x165667b1u;
        h = (h ^ (h >> 15)) * 0x2c1b3c6du;
        h = (h ^ (h >> 12)) * 0x297a2d39u;
        h ^= h >> 15;
        return float(h >> 8) * (1.0 / 16777215.0);
    }
    float Quintic(float t) { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }
    // x, y wrap every per cells, z every 8 * per; all coordinates positive
    float Noise3(float3 x, uint s, uint per)
    {
        float3 f = floor(x);
        float3 u = x - f;
        u = float3(Quintic(u.x), Quintic(u.y), Quintic(u.z));
        uint3 i = uint3(f);
        uint pz = per * 8u;
        uint x0 = i.x % per, x1 = (i.x + 1u) % per, y0 = i.y % per, y1 = (i.y + 1u) % per, z0 = i.z % pz, z1 = (i.z + 1u) % pz;
        float c000 = Hash(uint3(x0, y0, z0), s), c100 = Hash(uint3(x1, y0, z0), s);
        float c010 = Hash(uint3(x0, y1, z0), s), c110 = Hash(uint3(x1, y1, z0), s);
        float c001 = Hash(uint3(x0, y0, z1), s), c101 = Hash(uint3(x1, y0, z1), s);
        float c011 = Hash(uint3(x0, y1, z1), s), c111 = Hash(uint3(x1, y1, z1), s);
        float a = lerp(lerp(c000, c100, u.x), lerp(c010, c110, u.x), u.y);
        float b = lerp(lerp(c001, c101, u.x), lerp(c011, c111, u.x), u.y);
        return lerp(a, b, u.z) * 2.0 - 1.0;
    }
    // where the air at world xy (m) is in the drifting gust field
    float2 Drifted(float2 xy, float4 drift) { return fmod(xy - drift.xy, 4096.0) + 4096.0; }
    // the gust factor on the mean wind (1 = mean); wind = (dir, speed, gustiness), drift = (offset, clock, day)
    float Gust(float2 xy, float4 wind, float4 drift)
    {
        float2 p = Drifted(xy, drift);
        float c = fmod(drift.z / 10.0, 1024.0);
        float n0 = Noise3(float3(p / 512.0, c * 0.25), 11u, 8u);
        float n1 = Noise3(float3(p / 128.0, c), 12u, 32u);
        float n2 = Noise3(float3(p / 32.0, c * 2.0), 13u, 128u);
        return clamp(1.0 + wind.w * (0.45 * n0 + 0.45 * n1 + 0.3 * n2) * 2.2, 0.05, 2.5);
    }
    // still-water depth at world xy (m), the same as ALyudSea::DepthAtM (bilinear between cell centres)
    float Depth(float2 xy, float4 rect, float openDepth, float landX, Texture2D dep, SamplerState depS)
    {
        if (xy.x < landX) return -10.0;
        if (rect.z <= 0.0) return openDepth;
        float2 uv = (xy - rect.xy) * rect.zw;
        if (any(uv < 0.0) || any(uv > 1.0)) return openDepth;
        return dep.SampleLevel(depS, uv, 0).r;
    }
};
FLyudNoise Lyud;
"""

# World position offset of the sea surface (WaveModel.cpp Displace), cm. Trains shorter than ~6 mesh
# cells at this distance fade out of the geometry (their slopes stay in the pixel normals).
# Inputs: Pos (absolute world position, cm), Cam (camera position, cm), Tab, Dep, Clock, SP, Rect, LandX.
VERTEX_HLSL = r"""
float3 p = Pos * 0.01;
float2 xy = p.xy;
float spacing = max(length(p - Cam * 0.01) * 6.2831853 / Clock.y, Clock.z);
float depth = Lyud.Depth(xy, Rect, Clock.w, LandX, Dep, DepSampler);
// no sea on land: tuck the surface away under the ground
if (depth < -0.3) return float3(0.0, 0.0, -3000.0);
float cap = saturate(0.4 * max(depth, 0.0) / max(0.5 * SP.y, 1e-3));
float3 d = 0;
int n = (int)SP.z;
for (int i = 0; i < n; i++)
{
    float4 a = Tab.Load(int3(i, 0, 0));
    float4 b = Tab.Load(int3(i, 1, 0));
    float fade = saturate((b.z / spacing - 3.0) / 3.0);
    if (fade <= 0.0) break;
    float A = a.w * sqrt(saturate(a.z * depth * 0.5)) * cap * fade;
    float s, c;
    sincos(a.z * dot(a.xy, xy) - b.x * Clock.x + b.y, s, c);
    d.xy -= SP.x * A * a.xy * s;
    d.z += A * c;
}
return d * 100.0;
"""

# Per pixel: the surface normal of every train at the particle's rest point XY (m, from the vertex
# shader), detail trains scaled by the local gust (cat's paws; glassy where the air is still), trains
# finer than the pixel fading into roughness; whitecaps where the Gerstner surface pinches (Jacobian)
# in a fresh wind. Returns (normal x, normal y, roughness, whitecap); normal z = sqrt(1 - x^2 - y^2).
# Inputs: XY, V (camera vector), Tab, Dep, Clock, SP, Rect, LandX, Wind, Drift.
PIXEL_HLSL = r"""
float2 xy = XY;
float foot = max(length(ddx(xy)), length(ddy(xy))) + 1e-5;
float depth = Lyud.Depth(xy, Rect, Clock.w, LandX, Dep, DepSampler);
float cap = saturate(0.4 * max(depth, 0.0) / max(0.5 * SP.y, 1e-3));
float t = Clock.x;
float Txx = 1.0, Txy = 0.0, Tyy = 1.0, Txz = 0.0, Tyz = 0.0, lost = 0.0;
int ng = (int)SP.z;
for (int i = 0; i < ng; i++)
{
    float4 a = Tab.Load(int3(i, 0, 0));
    float A = a.w * sqrt(saturate(a.z * depth * 0.5)) * cap;
    float fade = saturate(2.0 - a.z * foot * 1.5);
    lost += (a.z * A) * (a.z * A) * (1.0 - fade * fade);
    if (fade <= 0.0) continue;
    float4 b = Tab.Load(int3(i, 1, 0));
    A *= fade;
    float s, c;
    sincos(a.z * dot(a.xy, xy) - b.x * t + b.y, s, c);
    float KA = a.z * A;
    Txx -= SP.x * KA * a.x * a.x * c;
    Txy -= SP.x * KA * a.x * a.y * c;
    Tyy -= SP.x * KA * a.y * a.y * c;
    Txz -= KA * a.x * s;
    Tyz -= KA * a.y * s;
}
float J = Txx * Tyy - Txy * Txy;
float g = Lyud.Gust(xy, Wind, Drift);
float U = Wind.z * g;
float rip = saturate((U - 0.7) / 1.8) * g * saturate(depth * 4.0);
int nd = (int)SP.w;
for (int k = 0; k < nd; k++)
{
    float4 a = Tab.Load(int3(64 + k, 0, 0));
    float A = a.w * rip;
    float fade = saturate(2.0 - a.z * foot * 1.5);
    lost += (a.z * A) * (a.z * A) * (1.0 - fade * fade);
    if (fade <= 0.0) break;
    float4 b = Tab.Load(int3(64 + k, 1, 0));
    float s, c;
    sincos(a.z * dot(a.xy, xy) - b.x * t + b.y, s, c);
    float KA = a.z * A * fade;
    Txz -= KA * a.x * s;
    Tyz -= KA * a.y * s;
}
float3 n = normalize(float3(Txy * Tyz - Txz * Tyy, Txz * Txy - Txx * Tyz, Txx * Tyy - Txy * Txy));
// at grazing views a steep facet would mirror the water below the horizon (a black hole in the
// reflection): tilt such facets back toward flat until they mirror the sky
float3 v = normalize(V);
for (int j = 0; j < 4; j++)
{
    float3 r = 2.0 * dot(n, v) * n - v;
    if (r.z > 0.03) break;
    n = normalize(lerp(n, float3(0, 0, 1), 0.5));
}
float rough = saturate(0.02 + sqrt(lost) * 0.9);
// whitecaps: where the crest pinches, in a wind over ~4 m/s, broken up by a drifting noise
float2 p = Lyud.Drifted(xy, Drift);
float fn = 0.5 + 0.5 * Lyud.Noise3(float3(p / 2.0, fmod(Drift.z / 10.0, 1024.0) * 4.0), 15u, 512u);
// (the fresher the wind, the less a crest has to pinch to break: ~1 % of the sea white at 7 m/s, ~6 % at 14)
float thr = lerp(0.62, 0.95, saturate((U - 4.0) / 10.0));
float wc = smoothstep(thr, thr - 0.3, J) * saturate((U - 3.0) / 3.0);
wc = saturate(wc * (0.4 + 1.4 * fn) - 0.08);
// in a strong wind the old foam is drawn out into streaks along it
float2 wd = normalize(Wind.xy + 1e-4);
float2 sq = float2(dot(p, float2(-wd.y, wd.x)) * 0.45, dot(p, wd) * 0.12);
float st = Lyud.Noise3(float3(sq + 4096.0, 1.0), 16u, 4096u);
float brk = Lyud.Noise3(float3(p / 6.0, 2.0), 17u, 512u) * 0.5 + 0.5;
wc += saturate(st * 2.5 - 1.4) * saturate((U - 9.0) / 6.0) * 0.25 * fn * brk;
return float4(n.x, n.y, rough, saturate(wc));
"""

# Surf over the shallows: a band of foam lines running in to the beach and a broken white edge where the
# water meets the sand. Depth = metres of water under the surface from the scene (SceneDepthWithoutWater),
# Bed = still-water depth from the depth map, Pos in cm, Wind for how big the surf is.
SURF_HLSL = r"""
float2 p = Pos.xy * 0.01;
float d = max(min(Depth * 0.01, Bed + 0.2), 0.0);
float hs = SP.y;
float2 q = p * 0.9 + float2(Time * 0.05, Time * 0.02);
float n = 0, a = 0.5;
for (int o = 0; o < 3; o++)
{
    n += a * (Lyud.Noise3(float3(fmod(q, 2048.0) + 2048.0, 3.0), 21u + o, 2048u) * 0.5 + 0.5);
    q = q * 2.03 + 17.1;
    a *= 0.5;
}
n /= 0.875;
// breakers: lines along the depth contours rolling in to the beach where the water is shallower than the
// waves are tall; a sharp white front and foam trailing behind it, broken up by noise
float zone = saturate(1.0 - d / max(hs * 1.5 + 0.2, 0.25));
float ph = d * 2.2 / max(hs, 0.25) + Time * 0.16 + n * 0.35;
float f = frac(ph);
float front = smoothstep(0.72, 0.95, f) * (1.0 - smoothstep(0.96, 1.0, f));
float trail = exp(-f * 2.2) * 0.8;
float breakers = saturate((front + trail) * zone * (0.6 + n) - 0.2) * saturate(hs * 3.0);
// the wash: a broken white edge where the water runs up the sand
float edge = saturate(1.0 - d / 0.25) * smoothstep(0.3, 0.6, n);
return saturate(breakers * 1.4 + edge);
"""

# Weed on the bed swaying with the passing waves: the dominant train's orbital motion at this depth
# (horizontal excursion a / sinh(k h)), plus a flutter; R of the vertex colour = how far up the plant.
# Inputs: P (world position cm), R (vertex colour R), Rnd (per-instance random), Sea, SW, Om, Ph, Sway.
WEED_HLSL = r"""
float depth = max((Sea.x - P.z) * 0.01, 0.1);
float kh = clamp(SW.z * depth, 0.25, 8.0);
float A = min(SW.w / sinh(kh), 0.5);
float ph = SW.z * dot(SW.xy, P.xy * 0.01) - Om * Sea.y + Ph;
float w = R * R * Sway;
float2 across = float2(-SW.y, SW.x);
float fl = sin(Sea.y * 2.1 + Rnd * 6.2831853 + P.x * 0.03 + R * 2.0);
float2 off = (SW.xy * A * cos(ph) * 0.8 + across * 0.035 * fl + SW.xy * 0.03) * 100.0 * w;
return float3(off, -0.4 * length(off) * R);
"""

# A swimming fish's body wave, cm, across its side (Side = the instance's local Y in world space).
FISH_HLSL = r"""
float w = R * R * Wiggle;
return Side * sin(T * 13.0 + Rnd * 6.2831853 - R * 5.0) * 1.1 * w;
"""
