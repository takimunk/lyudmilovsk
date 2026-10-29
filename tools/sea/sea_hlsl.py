"""HLSL for the sea's and the wind's materials (unreal/scripts/make_sea_materials.py builds them).

These mirror the C++ in unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Weather (WindModel.cpp GustAt,
WaveModel.cpp Displace / SurfaceAt): keep both in step. The wave trains themselves are not here: ALyudSea
writes them into a 128 x 2 float texture (WaveTable), geometry trains in columns 0..63, detail trains in
64..127, row 0 = (dir x, dir y, k rad/m, amplitude m), row 1 = (omega rad/s, phase rad, wavelength m, 0),
sorted longest first, so a loop can stop at the first train too short to draw.

Parameters (ALyudSea sets them on its dynamic material):
  SeaClock  = (wave clock s wrapped to 1200, ring segments, finest mesh step m, open-sea depth m)
  SeaParams = (Gerstner choppiness, significant height m, geometry trains, detail trains)
  DepthRect = (min x m, min y m, 1/size x, 1/size y) of DepthMap (r = depth under the datum m, land < 0;
              g = water a hollow on the tidal flat keeps when the ebb leaves it, m: the ground draws those pools)
  LandSideX = world X (m) east of which there is no sea
  Tide      = the water over the datum now, m (0 at spring high water, negative below; the depth map is under the datum)
  Glitter   = (the capillary ripples' slope per train, the water's least roughness, 0, 0)
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
    // (depth now m, calm 0..1, lift m) at world xy: the depth map's depth under the datum (r, bilinear between
    // cell centres) plus the tide. The pools the ebb leaves in the flat's hollows (g) are drawn by the ground
    // (M_Ground), so the sea stays calm = 1, lift = 0 here; the slots are kept for the call sites.
    float3 Water(float2 xy, float4 rect, float openDepth, float landX, float tide, Texture2D dep, SamplerState depS)
    {
        if (xy.x < landX) return float3(-10.0, 1.0, 0.0);
        if (rect.z <= 0.0) return float3(openDepth + tide, 1.0, 0.0);
        float2 uv = (xy - rect.xy) * rect.zw;
        if (any(uv < 0.0) || any(uv > 1.0)) return float3(openDepth + tide, 1.0, 0.0);
        return float3(dep.SampleLevel(depS, uv, 0).r + tide, 1.0, 0.0);
    }
    // still-water depth at world xy now (m), the same as ALyudSea::WaterDepthAtM: the depth map's depth under
    // the datum (r, bilinear between cell centres) plus the tide
    float Depth(float2 xy, float4 rect, float openDepth, float landX, float tide, Texture2D dep, SamplerState depS)
    {
        return Water(xy, rect, openDepth, landX, tide, dep, depS).x;
    }
    // the surf (WaveModel.cpp SurfZone, SurfHeight): 0..1 into the shallows, and the breakers' height there (m)
    float SurfZone(float depth, float hs) { return depth <= 0.0 ? 0.0 : saturate(1.0 - depth / max(hs * 1.5 + 0.2, 0.25)); }
    float SurfPhase(float depth, float hs, float t) { return frac(depth * 2.2 / max(hs, 0.25) + t * 0.16); }
    float SurfHeight(float depth, float hs, float t)
    {
        float a = SurfZone(depth, hs) * min(0.3 * depth, 0.35 * hs) * saturate(hs * 3.0);
        float f = SurfPhase(depth, hs, t);
        return a * (f * f * f - 0.25) * 2.0;
    }
    float BreakCap(float depth, float hs) { return saturate(0.4 * max(depth, 0.0) / max(0.5 * hs, 1e-3)) * (1.0 - 0.6 * SurfZone(depth, hs)); }
    // cellular noise: (F1, F2), the distances to the nearest and second nearest of points jittered one per unit
    // cell, each circling slowly (bubbles shifting); x positive
    float2 Cells(float2 x, uint s, float t)
    {
        float2 f = floor(x), u = x - f;
        float d1 = 8.0, d2 = 8.0;
        for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++)
        {
            uint3 c = uint3(uint(f.x + i) % 4096u, uint(f.y + j) % 4096u, 0u);
            float hx = Hash(c, s), hy = Hash(c, s + 7u), hp = Hash(c, s + 13u);
            float2 o = float2(i, j) + 0.15 + 0.7 * float2(hx, hy) + 0.12 * float2(cos(t * 0.7 + hp * 6.28), sin(t * 0.6 + hp * 6.28)) - u;
            float d = dot(o, o);
            if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) { d2 = d; }
        }
        return sqrt(float2(d1, d2));
    }
    // foam lace at p (m) for an amount of foam 0..1, foot = metres per pixel: a net of foam threads round open holes
    // (cell borders, F2 - F1 small), with finer bubbles in them. A little foam is thin broken threads and faint;
    // more thickens the threads until the holes close. Where a cell is smaller than a few pixels it turns into its
    // mean cover, a soft haze, never shimmer. Edges are smooth over at least a pixel, so nothing is hard-edged.
    float Lace(float2 p, float t, float amt, float foot)
    {
        if (amt <= 0.002) return 0.0;
        float2 q = fmod(p + float2(t * 0.05, t * 0.02), 2048.0) + 2048.0;
        // the cells warped at two sizes so the net is never a honeycomb, and a slow noise that varies the threads'
        // width and breaks them into pieces
        float2 wq = float2(Noise3(float3(q / 1.3, t * 0.03), 43u, 2048u), Noise3(float3(q / 1.3 + 57.0, t * 0.03), 44u, 2048u));
        float2 wr = float2(Noise3(float3(q / 0.4, t * 0.05), 45u, 2048u), Noise3(float3(q / 0.4 + 91.0, t * 0.05), 46u, 2048u));
        float br = Noise3(float3(q / 0.9, t * 0.04), 47u, 2048u) * 0.5 + 0.5;
        float2 c1 = Cells(q / 0.7 + wq * 0.55 + wr * 0.15, 41u, t);
        float2 c2 = Cells(q / 0.18 + 31.7 + wr * 0.6, 42u, t * 1.4);
        float a1 = foot / 0.7 * 1.5, a2 = foot / 0.18 * 1.5;
        float w1 = (0.02 + 0.45 * amt * amt) * (0.4 + 1.2 * br);  // thread half-width, in cells
        float w2 = 0.05 + 0.45 * amt;
        float s1 = 0.06 + a1, s2 = 0.06 + a2;
        float l1 = 1.0 - smoothstep(w1 - s1, w1 + s1, c1.y - c1.x);
        float l2 = 1.0 - smoothstep(w2 - s2, w2 + s2, c2.y - c2.x);
        // the threads fray: bubbles along them, gaps where the slow noise is low (less foam, more gaps)
        l1 *= (0.35 + 0.65 * l2) * smoothstep(0.55 - 0.5 * amt, 0.9 - 0.5 * amt, br + 0.25 * (c1.x - 0.5));
        float far1 = saturate(a1 * 1.5 - 0.3), far2 = saturate(a2 * 1.5 - 0.3);
        l1 = lerp(l1, saturate(w1 * 2.6 + 0.15) * smoothstep(0.0, 0.7, amt), far1);
        l2 = lerp(l2, saturate(w2 * 2.0), far2);
        float net = l1 + smoothstep(0.5, 1.0, amt) * l2 * 0.4 * (1.0 - far1 * 0.5);
        return saturate(net) * smoothstep(0.0, 0.4, amt) * (0.75 + 0.25 * amt);
    }
};
FLyudNoise Lyud;
"""

# World position offset of the sea surface (WaveModel.cpp Displace), cm. Trains shorter than ~6 mesh
# cells at this distance fade out of the geometry (their slopes stay in the pixel normals).
# Inputs: Pos (absolute world position, cm), Cam (camera position, cm), Tab, Dep, Clock, SP, Rect, LandX, Tide.
VERTEX_HLSL = r"""
float3 p = Pos * 0.01;
float2 xy = p.xy;
float spacing = max(length(p - Cam * 0.01) * 6.2831853 / Clock.y, Clock.z);
float3 w = Lyud.Water(xy, Rect, Clock.w, LandX, Tide, Dep, DepSampler);
float depth = w.x;
// no sea on land: tuck the surface away under the ground
if (depth < -0.3) return float3(0.0, 0.0, -3000.0);
// a pool left on the flat stands still at its own level
float cap = Lyud.BreakCap(depth, SP.y) * w.y;
float3 d = float3(0.0, 0.0, Lyud.SurfHeight(depth, SP.y, Clock.x) * w.y + w.z);
int n = (int)SP.z;
for (int i = 0; i < n; i++)
{
    float4 a = Tab.Load(int3(i, 0, 0));
    float4 b = Tab.Load(int3(i, 1, 0));
    float fade = saturate((b.z / spacing - 3.0) / 3.0);
    if (fade <= 0.0) break;
    float A = a.w * sqrt(saturate(a.z * depth * 4.0)) * cap * fade;
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
# Inputs: XY, V (camera vector), Tab, Dep, Clock, SP, Rect, LandX, Tide, Wind, Drift, Glit (capillary slope, least roughness).
PIXEL_HLSL = r"""
float2 xy = XY;
float foot = max(length(ddx(xy)), length(ddy(xy))) + 1e-5;
float3 w = Lyud.Water(xy, Rect, Clock.w, LandX, Tide, Dep, DepSampler);
float depth = w.x;
float cap = Lyud.BreakCap(depth, SP.y) * w.y;
float t = Clock.x;
float Txx = 1.0, Txy = 0.0, Tyy = 1.0, Txz = 0.0, Tyz = 0.0, lost = 0.0, eta = 0.0;
int ng = (int)SP.z;
for (int i = 0; i < ng; i++)
{
    float4 a = Tab.Load(int3(i, 0, 0));
    float A = a.w * sqrt(saturate(a.z * depth * 4.0)) * cap;
    float fade = saturate(2.0 - a.z * foot * 1.5);
    lost += (a.z * A) * (a.z * A) * (1.0 - fade * fade);
    if (fade <= 0.0) continue;
    float4 b = Tab.Load(int3(i, 1, 0));
    A *= fade;
    float s, c;
    sincos(a.z * dot(a.xy, xy) - b.x * t + b.y, s, c);
    float KA = a.z * A;
    eta += A * c;
    Txx -= SP.x * KA * a.x * a.x * c;
    Txy -= SP.x * KA * a.x * a.y * c;
    Tyy -= SP.x * KA * a.y * a.y * c;
    Txz -= KA * a.x * s;
    Tyz -= KA * a.y * s;
}
float J = Txx * Tyy - Txy * Txy;
float g = Lyud.Gust(xy, Wind, Drift);
float U = Wind.z * g;
float rip = saturate((U - 0.7) / 1.8) * g * saturate(depth * 4.0) * (0.25 + 0.75 * w.y);
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
// capillary ripples: the sea is never a mirror; even in a calm a light air ruffles it into wavelets a few cm to two
// metres long (deep-water dispersion with surface tension), Glit.x = their slope. Near the camera they break the
// sun's and the sky's mirror images into glitter; far off they go into the roughness and the sun's mirror image
// stretches into a path
float2 wdir = normalize(Wind.xy + float2(1e-4, 0.0));
float wang = atan2(wdir.y, wdir.x);
float cap0 = Glit.x * saturate(depth * 4.0) * (0.6 + 0.4 * saturate(g));
for (int q = 0; q < 6; q++)
{
    float ang = wang + (q - 2.5) * 0.55 + sin(q * 2.4) * 0.3;
    float kq = 6.2831853 / (0.12 * pow(1.75, q));
    float KA = cap0 * (0.8 + 0.08 * q);
    float fade = saturate(2.0 - kq * foot * 1.5);
    lost += KA * KA * (1.0 - fade * fade);
    if (fade <= 0.0) continue;
    float2 dq = float2(cos(ang), sin(ang));
    float om = sqrt(9.81 * kq + 7.4e-5 * kq * kq * kq);
    float s, c;
    sincos(kq * dot(dq, xy) - om * t + q * 1.7, s, c);
    Txz -= KA * fade * dq.x * s;
    Tyz -= KA * fade * dq.y * s;
}
// the breakers' slope, from how their height changes across the pixel's neighbours
float sh = Lyud.SurfHeight(depth, SP.y, t) * w.y;
float2 dxy = ddx(xy), dyy = ddy(xy);
float2 dsh = float2(ddx(sh), ddy(sh));
float det = dxy.x * dyy.y - dxy.y * dyy.x;
float2 sg = abs(det) > 1e-8 ? float2(dsh.x * dyy.y - dsh.y * dxy.y, dsh.y * dxy.x - dsh.x * dyy.x) / det : 0.0;
Txz += sg.x;
Tyz += sg.y;
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
float rough = saturate(Glit.y + sqrt(lost) * 0.9);
// whitecaps: crests that stand high (eta in sigmas of the sea) or pinch (Jacobian) break; the fresher the
// wind the lower the bar (a few % of the sea at 7 m/s, a white-flecked sea at gale force), broken up by noise
float2 p = Lyud.Drifted(xy, Drift);
float fn = 0.5 + 0.5 * Lyud.Noise3(float3(p / 2.0, fmod(Drift.z / 10.0, 1024.0) * 4.0), 15u, 512u);
float sigma = max(SP.y * 0.25, 0.02);
float score = eta / sigma + (1.0 - J) * 4.0;
float bar = lerp(3.0, 0.8, saturate((U - 4.0) / 10.0));
float wind = saturate((U - 3.0) / 3.0);
// broken into tufts a metre or so across, not whole crests; a soft amount of foam (the lace draws it), wide ramps so
// a pinching crest near the camera doesn't cut a hard edge
float fine = Lyud.Noise3(float3(p / 0.8, fmod(Drift.z / 10.0, 1024.0) * 8.0), 18u, 1024u) * 0.5 + 0.5;
float wc = smoothstep(bar - 0.6, bar + 1.8, score) * wind * saturate(0.25 + 1.1 * fn) * smoothstep(0.1, 0.85, fine) * w.y;
// old foam left behind the breakers, grey and drawn out along the wind in a gale
float2 wd = normalize(Wind.xy + 1e-4);
float2 sq = float2(dot(p, float2(-wd.y, wd.x)) * 0.35, dot(p, wd) * 0.09);
float st = Lyud.Noise3(float3(sq + 4096.0, 1.0), 16u, 4096u) * 0.5 + 0.5;
float patchy = Lyud.Noise3(float3(p / 9.0, 2.0), 17u, 512u) * 0.5 + 0.5;
float old = smoothstep(bar - 0.9, bar + 0.3, score) * st * patchy * w.y;
wc = max(wc, old * saturate((U - 8.0) / 6.0) * 0.4);
return float4(n.x, n.y, rough, saturate(wc));
"""

# Surf over the shallows, as an amount of foam 0..1 (the Foam node draws it as lace, Lyud.Lace): a soft band on each
# spilling breaker's front that thins out behind it, and a band where the water runs up the sand; both broken into
# patches. Widths are held in metres, not in depth: over a flat (1:200) a band of depth or phase is tens of metres
# wide, which drew wide white bands. Depth = metres of water under the surface from the scene (SceneDepthWithoutWater),
# Bed = (still-water depth now from the depth map, calm: 0 in a pool the ebb left), Pos in cm, Wind for how big the
# surf is.
SURF_HLSL = r"""
float2 p = Pos.xy * 0.01;
float d = max(min(Depth * 0.01, Bed.x + 0.2), 0.0);
float hs = SP.y;
float t = Clock.x;
float foot = max(max(length(ddx(p)), length(ddy(p))), 1e-4);     // metres per pixel
float2 q = p * 0.35 + float2(t * 0.03, t * 0.012);
float n = 0, a = 0.5;
for (int o = 0; o < 3; o++)
{
    n += a * (Lyud.Noise3(float3(fmod(q, 2048.0) + 2048.0, 3.0 + t * 0.02), 21u + o, 2048u) * 0.5 + 0.5);
    q = q * 2.03 + 17.1;
    a *= 0.5;
}
n /= 0.875;
float patchy = smoothstep(0.25, 0.75, n);
// spilling breakers: the same bores the surface rises in (Lyud.SurfHeight); on the front a soft band ~0.6 m wide
// whatever the slope, thinning out behind it as the foam dissolves
float zone = Lyud.SurfZone(Bed.x, hs);
float raw = Bed.x * 2.2 / max(hs, 0.25);
float perM = length(float2(ddx(raw), ddy(raw))) / foot;          // phase per metre
float f = frac(Lyud.SurfPhase(Bed.x, hs, t) + (n - 0.5) * 0.1);
float lineW = max(0.6 * perM, fwidth(raw));
float df = f - 0.93;
float front = exp(-df * df / (lineW * lineW));
float behind = df < 0.0 ? f + 0.07 : df;                         // phase since the front passed
float trail = exp(-behind * 5.0) * (1.0 - front);
float breakers = (front * 0.85 + trail * 0.5) * (0.3 + 0.7 * patchy) * sqrt(zone) * saturate(hs * 3.0);
// the wash: a band ~0.3-2.5 m off the water's edge, measured on the smooth depth map (the mesh's own edge, where it
// tucks under the sand, is jagged: foam drawn along the scene's thickness traced its teeth), none in the last few cm
// of water so the jagged edge itself stays clear
float perD = max(length(float2(ddx(Bed.x), ddy(Bed.x))) / foot, 0.002);  // depth per metre (the smooth depth map's)
float off = max(Bed.x, 0.0) / perD;                                       // metres from the still-water edge
float wash = smoothstep(0.1, 0.6, off) * (1.0 - smoothstep(0.8, 2.5, off)) * (0.15 + 0.45 * patchy) * saturate(hs * 4.0 + 0.3);
return saturate(max(breakers, wash)) * smoothstep(0.005, 0.04, d) * Bed.y;
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
