"""HLSL of the winter's materials (unreal/scripts/make_winter_materials.py builds them). The noises and the ice's
mask mirror Private/Winter/SnowModel.cpp (Hash01, ValueNoise, Fbm, FIceMask::IceAt): keep them in step, the
ice's solid tiles (C++) must lie where the ice is drawn.

Frame: world centimetres; the noises take metres. MPC_LyudWinter (ULyudWinterSubsystem::UpdateCollection):
  FieldRect  min x, min y (cm), 1/width, 1/height (cm)  of RT_SnowDepth (snow depth, m, 1 m cells)
  FieldTexel 1/w, 1/h (texels), cell cm
  CoverRect, CoverTexel  the same for RT_SnowCover (R: ground over the datum m, G: the roof over the ground m; 2 m)
  IceRect    the same for RT_SeaIce (distance from the shallows, m; 3 m)
  Snow       level depth m, freshness 0..1, melt 0..1, age days
  SnowB      snow on the branches 0..1, ready 0/1, datum m, snowfall cm/h
  Ice        open strip m, its raggedness m, cover 0..1 (0 = no ice drawn), thickness cm
  IceRail    rail line Y cm, X from, X to, half width m
  IceDrift   floes' drift x, y cm, still water z cm, seed
"""

NOISE = r"""
    float Hash01(int x, int y, uint seed)
    {
        uint h = (uint(x) * 0x8da6b343u) ^ (uint(y) * 0xd8163841u) ^ (seed * 0xcb1ab31fu);
        h ^= h >> 13;
        h *= 0x5bd1e995u;
        h ^= h >> 15;
        return float(h & 0xffffffu) / 16777216.0;
    }
    float Value(float2 p, float scale, uint seed)
    {
        float2 x = p / scale;
        float2 f0 = floor(x);
        int2 i = int2(f0);
        float2 f = x - f0;
        f = f * f * (3.0 - 2.0 * f);
        float a = Hash01(i.x, i.y, seed), b = Hash01(i.x + 1, i.y, seed);
        float c = Hash01(i.x, i.y + 1, seed), d = Hash01(i.x + 1, i.y + 1, seed);
        float ab = a + (b - a) * f.x, cd = c + (d - c) * f.x;
        return ab + (cd - ab) * f.y;
    }
    float Fbm(float2 p, float scale, uint seed)
    {
        return 0.5 * Value(p, scale, seed) + 0.3 * Value(p, scale * 0.47, seed + 1u) + 0.2 * Value(p, scale * 0.21, seed + 2u);
    }
"""

# --- the ground's lift: world position offset of the landscape by the snow's depth (vertex shader)
GROUND_WPO = r"""
float2 uv = (P.xy - Rect.xy) * Rect.zw;
if (B.y < 0.5 || any(uv <= 0.0) || any(uv >= 1.0)) return float3(0, 0, 0);
float d = max(Dep.SampleLevel(DepSampler, uv, 0).r, 0.0);
// round the player the fine snow surface (M_SnowNear) takes over: the ground's own snow sinks under it, back up
// toward the fine one's edge (N: its centre x, y, half size, fade band; cm)
float cheb = max(abs(P.x - N.x), abs(P.y - N.y));
float inner = N.z > 0.0 ? 1.0 - smoothstep(N.z - N.w, N.z - 0.25 * N.w, cheb) : 0.0;
return float3(0, 0, d * lerp(1.0, 0.1, inner) * 100.0);
"""

# --- the snow decal over the whole map: returns base colour, Opacity, Normal (world), Roughness
DECAL = r"""
struct LW
{
""" + NOISE + r"""
    float Ground(float2 xy, float4 rect, Texture2D tex, SamplerState s, float datum)
    {
        float2 uv = (xy - rect.xy) * rect.zw;
        if (any(uv <= 0.0) || any(uv >= 1.0)) return -1.0e9;
        return (tex.SampleLevel(s, uv, 0).r + datum) * 100.0;
    }
    float Depth(float2 xy, float4 rect, Texture2D tex, SamplerState s)
    {
        float2 uv = (xy - rect.xy) * rect.zw;
        if (any(uv <= 0.0) || any(uv >= 1.0)) return 0.0;
        return max(tex.SampleLevel(s, uv, 0).r, 0.0);
    }
};
LW w;
Opacity = 0.0;
Normal = float3(0, 0, 1);
Roughness = 0.65;
if (B.y < 0.5 || S.x <= 0.002) return float3(0, 0, 0);
float3 P = WP;
float3 ng = normalize(cross(ddx(P), ddy(P)));
if (dot(ng, Cam - P) < 0.0) ng = -ng;
float datum = B.z;
float ground = w.Ground(P.xy, CRect, Cov, View.SharedBilinearClampedSampler, datum);
float depth = w.Depth(P.xy, FRect, Dep, View.SharedBilinearClampedSampler);
float2 m = P.xy * 0.01;
float cover = 0.0;
float pressed = 0.0;
float3 n = ng;
if (ground > -1.0e8 && P.z < ground - 60.0)
{
    // under the ground: the club, the tunnel, the cellars
    return float3(0, 0, 0);
}
float surf = ground + depth * 100.0;
if (ground > -1.0e8 && P.z < surf + 45.0 && ng.z > 0.3)
{
    // the ground's snow: by the field; thin snow lies in patches (the grass's tufts show through)
    float thr = 1.0 + 4.0 * w.Fbm(m, 1.7, 11u);
    cover = smoothstep(thr * 0.35, thr, depth * 100.0);
    // the surface's slope from the field and the ground (smooth, not the landscape's 1 m facets)
    float e = FT.z;
    float sx = (w.Ground(P.xy + float2(e, 0), CRect, Cov, View.SharedBilinearClampedSampler, datum) + 100.0 * w.Depth(P.xy + float2(e, 0), FRect, Dep, View.SharedBilinearClampedSampler))
             - (w.Ground(P.xy - float2(e, 0), CRect, Cov, View.SharedBilinearClampedSampler, datum) + 100.0 * w.Depth(P.xy - float2(e, 0), FRect, Dep, View.SharedBilinearClampedSampler));
    float sy = (w.Ground(P.xy + float2(0, e), CRect, Cov, View.SharedBilinearClampedSampler, datum) + 100.0 * w.Depth(P.xy + float2(0, e), FRect, Dep, View.SharedBilinearClampedSampler))
             - (w.Ground(P.xy - float2(0, e), CRect, Cov, View.SharedBilinearClampedSampler, datum) + 100.0 * w.Depth(P.xy - float2(0, e), FRect, Dep, View.SharedBilinearClampedSampler));
    float3 nf = normalize(float3(-sx / (2.0 * e), -sy / (2.0 * e), 1.0));
    // where the field is thin the ground's own shape shows
    n = normalize(lerp(ng, nf, saturate(depth * 12.0)));
    // round the player the fine snow (ALyudSnowNear, 10 cm cells) carries the prints: its own shape shows
    float cheb = max(abs(P.x - N.x), abs(P.y - N.y));
    float nearW = N.z > 0.0 ? 1.0 - smoothstep(N.z - N.w, N.z - 0.15 * N.w, cheb) : 0.0;
    pressed = nearW * Tt.SampleLevel(TtSampler, frac(P.xy / TR.y), 0).r;
    n = normalize(lerp(n, ng, 0.85 * nearW));
    // the wind's ripples and the snow's soft lumps (cm scale relief the 1 m field can't hold)
    float3 rip = float3(w.Value(P.xy, 90.0, 31u) - 0.5, w.Value(P.yx, 115.0, 33u) - 0.5, 0.0)
               + 0.6 * float3(w.Value(P.xy, 23.0, 34u) - 0.5, w.Value(P.yx, 29.0, 35u) - 0.5, 0.0);
    n = normalize(n + rip * 0.22 * saturate(depth * 5.0));
}
else
{
    // anything else under the open sky: roofs, benches, rails, the tops of walls; none under a roof over it
    float2 uv = (P.xy - CRect.xy) * CRect.zw;
    float roof = 0.0;
    if (all(uv > 0.0) && all(uv < 1.0))
    {
        float2 t = CT.xy;
        roof = max(max(Cov.SampleLevel(View.SharedPointClampedSampler, uv + float2(-0.5, -0.5) * t, 0).g,
                       Cov.SampleLevel(View.SharedPointClampedSampler, uv + float2(0.5, -0.5) * t, 0).g),
                   max(Cov.SampleLevel(View.SharedPointClampedSampler, uv + float2(-0.5, 0.5) * t, 0).g,
                       Cov.SampleLevel(View.SharedPointClampedSampler, uv + float2(0.5, 0.5) * t, 0).g));
    }
    float sheltered = (roof > 1.8 && ground > -1.0e8 && P.z < ground + roof * 100.0 - 60.0) ? 1.0 : 0.0;
    float up = smoothstep(0.35, 0.72, ng.z);
    float level = S.x * 100.0;
    float thr = 0.6 + 2.5 * w.Fbm(m, 0.9, 12u);
    cover = up * (1.0 - sheltered) * smoothstep(thr * 0.4, thr, level);
    n = normalize(lerp(ng, float3(0, 0, 1), 0.65));
}
if (cover <= 0.001) return float3(0, 0, 0);
// sparkle: tiny facets of the crystals
float g1 = w.Value(P.xy, 2.3, 21u) - 0.5, g2 = w.Value(P.yx, 2.9, 22u) - 0.5;
n = normalize(n + float3(g1, g2, 0) * 0.12);
// fresh snow is near white with a cool tint; old snow greys; thawing snow darkens and goes glassy
float3 fresh = float3(0.86, 0.89, 0.93);
float3 old = float3(0.70, 0.71, 0.70) * (0.92 + 0.08 * w.Fbm(m, 6.0, 23u));
float3 col = lerp(old, fresh, S.y);
col *= (1.0 - 0.25 * S.z) * (0.94 + 0.06 * w.Fbm(m, 3.0, 24u));
// pressed snow (footprints) a little greyer and glossier
col *= 1.0 - 0.12 * pressed;
Opacity = cover;
Normal = n;
Roughness = lerp(0.72, 0.42, S.z) - 0.08 * S.y - 0.15 * pressed;
return col;
"""

# --- the sea's ice: returns base colour, Mask (opacity mask), Roughness
ICE = r"""
struct LW
{
""" + NOISE + r"""
    // pancakes: round cakes ~1-2 m in the band along the edge (bigger and closer toward the solid ice)
    float Pancakes(float2 m, float k, uint seed, out float rim)
    {
        float2 c = floor(m / 1.6);
        float best = 1.0e3;
        float bestR = 1.0;
        for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++)
        {
            int2 q = int2(c) + int2(i, j);
            float2 o = float2(Hash01(q.x, q.y, seed), Hash01(q.x, q.y, seed + 5u));
            float2 ctr = (float2(q) + 0.2 + 0.6 * o) * 1.6;
            float r = (0.35 + 0.45 * Hash01(q.x, q.y, seed + 9u)) * (0.55 + 0.6 * k);
            float d = length(m - ctr) / r;
            if (d < best) { best = d; bestR = r; }
        }
        rim = smoothstep(0.7, 0.98, best);
        return best < 1.0 ? 1.0 : 0.0;
    }
};
LW w;
Mask = 0.0;
Roughness = 0.2;
Normal = float3(0, 0, 1);
if (I.z <= 0.0 || I.w < 1.0) return float3(0, 0, 0);
float2 xy = P.xy;
float2 uv = (xy - R.xy) * R.zw;
float d = 1.0e4;
if (all(uv > 0.0) && all(uv < 1.0)) d = Tex.SampleLevel(TexSampler, uv, 0).r;
if (xy.x >= Rail.y && xy.x <= Rail.z) d = min(d, max(abs(xy.y - Rail.x) * 0.01 - Rail.w, 0.0));
float2 m = xy * 0.01;
uint seed = uint(Dr.w);
float edge = I.x + I.y * (2.0 * w.Fbm(m, 9.0, seed) - 1.0);
float floe = 1.0;
if (I.z < 0.97)
{
    float n = w.Fbm(m - Dr.xy * 0.01, 60.0, seed + 7u);
    floe = saturate((0.2 + 0.6 * I.z - n) / 0.03);
}
float into = d - edge;                         // m past the edge, into the ice
float solid = saturate((into - 4.0) / 1.5);    // the same as FIceMask::IceAt
float rim = 0.0;
float cakes = (into > 0.0 && into < 5.5) ? w.Pancakes(m, saturate(into / 4.0), seed + 31u, rim) : 0.0;
Mask = max(solid, cakes) * floe;
if (Mask <= 0.0) return float3(0, 0, 0);
// thin dark ice near the edge and when it is young, grey-blue bare ice, cracks, snow on top
float thick = saturate(I.w / 25.0) * saturate(into / 12.0 + 0.2);
float3 bare = lerp(float3(0.11, 0.14, 0.16), float3(0.42, 0.50, 0.55), thick);
float cr = 1.0 - smoothstep(0.0, 0.012, abs(w.Value(m, 23.0, seed + 41u) - 0.5));
cr = max(cr, 1.0 - smoothstep(0.0, 0.008, abs(w.Value(m + 37.0, 9.0, seed + 43u) - 0.5)) * 0.6);
bare *= 1.0 - 0.45 * cr;
float level = S.x * 100.0;
float drift = w.Fbm(float2(m.x * 0.35, m.y), 12.0, seed + 51u);
float snow = saturate((level * (0.4 + 1.2 * drift) - 1.0) / 4.0) * saturate(into / 3.0);
float3 col = lerp(bare, lerp(float3(0.70, 0.71, 0.70), float3(0.86, 0.89, 0.93), S.y), snow);
// the pancakes' raised white rims
col = lerp(col, float3(0.78, 0.81, 0.84), rim * (1.0 - solid) * 0.8);
// pressure ridges: rubble walls where floes were pushed together, a few hundred metres apart, white and lumpy
float rv = w.Value(m, 260.0, seed + 61u) + 0.35 * w.Value(m, 70.0, seed + 62u);
float ridge = (1.0 - smoothstep(0.0, 0.018, abs(rv - 0.68))) * saturate(into / 40.0);
col = lerp(col, float3(0.82, 0.85, 0.9), ridge * 0.9);
// relief: wind-carved snow (sastrugi, long along the wind), the ridges' blocks, bare ice's ripples
float e = 0.35;
float2 q = float2(m.x * 0.45, m.y);
float h0 = w.Fbm(q, 2.2, seed + 71u), hx = w.Fbm(q + float2(e * 0.45, 0), 2.2, seed + 71u), hy = w.Fbm(q + float2(0, e), 2.2, seed + 71u);
float k = 0.45 * snow + 0.2 * (1.0 - snow) + 2.0 * ridge;
Normal = normalize(float3(-(hx - h0) / e * k, -(hy - h0) / e * k, 1.0));
Roughness = lerp(lerp(0.08, 0.25, cr), 0.7, max(snow, ridge));
return col;
"""

# --- snow on the branches and needles (M_PlantLeaf, M_PlantNeedle): how much of the leaf's colour turns white
LEAF_SNOW = r"""
float up = saturate((N.z - 0.15) * 2.2);
float n = frac(sin(dot(floor(P.xy / 7.0), float2(12.9898, 78.233))) * 43758.5453);
return saturate(up * B.x * (0.7 + 0.5 * n));
"""

# --- the fine snow round the player (M_SnowNear on ALyudSnowNear's 16 cm grid): the ground (RT_NearGround, 1 m,
# exact landscape heights) + the snow field's depth pressed down by the trail (RT_SnowTrail: footprints, trenches;
# a toroidal window, wrap addressing). NG: x0, y0 (cm, texel 0's centre), cell cm, texels a side; TR: window cm.
NEAR_COMMON = r"""
struct LWN
{
""" + NOISE + r"""
    float Ground(float2 xy, float4 ng, Texture2D tex)
    {
        float2 t = (xy - ng.xy) / ng.z;
        float2 i0 = clamp(floor(t), 0.0, ng.w - 2.0);
        float2 f = saturate(t - i0);
        int2 i = int2(i0);
        float a = tex.Load(int3(i, 0)).r, b = tex.Load(int3(i + int2(1, 0), 0)).r;
        float c = tex.Load(int3(i + int2(0, 1), 0)).r, d = tex.Load(int3(i + int2(1, 1), 0)).r;
        return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
    }
    float Depth(float2 xy, float4 rect, Texture2D tex, SamplerState s)
    {
        float2 uv = (xy - rect.xy) * rect.zw;
        if (any(uv <= 0.0) || any(uv >= 1.0)) return 0.0;
        return max(tex.SampleLevel(s, uv, 0).r, 0.0);
    }
    float Trail(float2 xy, float4 tr, Texture2D tex, SamplerState s)
    {
        return tex.SampleLevel(s, frac(xy / tr.y), 0).r;
    }
    // the snow's surface: pressed snow keeps ~15% of its depth
    float Height(float2 xy, float4 ng, Texture2D gt, float4 rect, Texture2D dt, SamplerState ds, float4 tr, Texture2D tt, SamplerState ts)
    {
        return Ground(xy, ng, gt) + 100.0 * Depth(xy, rect, dt, ds) * (1.0 - 0.85 * Trail(xy, tr, tt, ts));
    }
};
LWN w;
"""

NEAR_WPO = NEAR_COMMON + r"""
if (N.z <= 0.0) return float3(0, 0, -100000.0);
float h = w.Height(P.xy, NG, Gt, Rect, Dep, DepSampler, TR, Tt, TtSampler);
// toward its edge it sinks under the ground's own snow (which comes up there)
float cheb = max(abs(P.x - N.x), abs(P.y - N.y));
float sink = 8.0 * smoothstep(N.z - N.w, N.z, cheb);
return float3(0, 0, h - sink - P.z);
"""

NEAR_PS = NEAR_COMMON + r"""
Mask = 0.0;
Normal = float3(0, 0, 1);
Roughness = 0.65;
float d = w.Depth(P.xy, Rect, Dep, DepSampler);
float tr = w.Trail(P.xy, TR, Tt, TtSampler);
float left = d * (1.0 - 0.85 * tr);
float cheb = max(abs(P.x - N.x), abs(P.y - N.y));
if (B.y < 0.5 || d < 0.02 || cheb > N.z) return float3(0, 0, 0);
// toward its edge it dissolves (a per-pixel dither the temporal AA smooths) into the ground's own snow: no ring
float fade = 1.0 - smoothstep(N.z - N.w, N.z - 0.15 * N.w, cheb);
float2 sp = Parameters.SvPosition.xy + float(View.StateFrameIndexMod8) * float2(5.588, 3.471);
float ign = frac(52.9829189 * frac(dot(sp, float2(0.06711056, 0.00583715))));
Mask = ign < fade ? 1.0 : 0.0;
float e = 6.0;
float hx = w.Height(P.xy + float2(e, 0), NG, Gt, Rect, Dep, DepSampler, TR, Tt, TtSampler) - w.Height(P.xy - float2(e, 0), NG, Gt, Rect, Dep, DepSampler, TR, Tt, TtSampler);
float hy = w.Height(P.xy + float2(0, e), NG, Gt, Rect, Dep, DepSampler, TR, Tt, TtSampler) - w.Height(P.xy - float2(0, e), NG, Gt, Rect, Dep, DepSampler, TR, Tt, TtSampler);
float3 n = normalize(float3(-hx / (2.0 * e), -hy / (2.0 * e), 1.0));
// the same relief and sparkle as the ground's snow decal (M_SnowDecal), so the two can't be told apart
float3 rip = float3(w.Value(P.xy, 90.0, 31u) - 0.5, w.Value(P.yx, 115.0, 33u) - 0.5, 0.0)
           + 0.6 * float3(w.Value(P.xy, 23.0, 34u) - 0.5, w.Value(P.yx, 29.0, 35u) - 0.5, 0.0);
n = normalize(n + rip * 0.22 * saturate(d * 5.0));
float g1 = w.Value(P.xy, 2.3, 21u) - 0.5, g2 = w.Value(P.yx, 2.9, 22u) - 0.5;
Normal = normalize(n + float3(g1, g2, 0) * 0.12);
float2 m = P.xy * 0.01;
float3 fresh = float3(0.86, 0.89, 0.93);
float3 old = float3(0.70, 0.71, 0.70) * (0.92 + 0.08 * w.Fbm(m, 6.0, 23u));
float3 col = lerp(old, fresh, S.y) * (1.0 - 0.25 * S.z) * (0.94 + 0.06 * w.Fbm(m, 3.0, 24u));
// pressed snow is a little greyer and glossier
col *= 1.0 - 0.12 * tr;
Roughness = lerp(0.72, 0.42, S.z) - 0.08 * S.y - 0.15 * tr;
return col;
"""

# --- falling snow (ALyudSnowFx): every flake a quad at the camera; P the actor (the camera), C its corner (UV0),
# R1, R2 its randoms (UV1, UV2), T the time; Box (x, y, z sizes cm, density 0..1), Wind (cm/s)
FLAKES_WPO = r"""
struct LWF
{
    float Ground(float2 xy, float4 rect, Texture2D tex, SamplerState s, float datum)
    {
        float2 uv = (xy - rect.xy) * rect.zw;
        if (any(uv <= 0.0) || any(uv >= 1.0)) return -1.0e9;
        return (tex.SampleLevel(s, uv, 0).r + datum) * 100.0;
    }
};
LWF w;
float id = frac(R1.x * 37.13 + R2.x * 91.71 + R1.y * 13.37);
if (id > Box.w) return float3(0, 0, 0);    // not falling now: a quad of no size at the camera
// Style: x flake size, y fall speed, z sway, w the share swept along near the ground (blowing snow)
float speed = lerp(65.0, 135.0, R2.y) * Style.y;
float3 base = float3(R1.x, R1.y, R2.x) * Box.xyz;
bool low = frac(id * 7.31) < Style.w;
float gust = low ? 1.35 : 0.9;
float3 pos = base + float3(Wind.x * T * gust, Wind.y * T * gust, low ? 0.0 : -speed * T);
float ph = R1.x * 61.0 + R2.y * 17.0;
pos.xy += float2(sin(T * 1.3 + ph), cos(T * 1.1 + ph * 1.7)) * 18.0 * Style.z;
float3 rel = (frac((pos - P) / Box.xyz + 0.5) - 0.5) * Box.xyz;
float3 wp = P + rel;
// none under the snow's surface, none under a roof
float ground = w.Ground(wp.xy, CRect, Cov, View.SharedBilinearClampedSampler, B.z);
if (ground > -1.0e8)
{
    float2 fuv = (wp.xy - FRect.xy) * FRect.zw;
    float d = (all(fuv > 0.0) && all(fuv < 1.0)) ? max(Dep.SampleLevel(View.SharedBilinearClampedSampler, fuv, 0).r, 0.0) : 0.0;
    float surf = ground + d * 100.0;
    // blowing snow: a sheet streaming along a metre or two over the snow, thickest at the bottom
    if (low) wp.z = surf + 5.0 + 160.0 * R2.x * R2.x + 6.0 * sin(T * 3.1 + ph);
    if (wp.z < surf) return float3(0, 0, 0);
    float2 cuv = (wp.xy - CRect.xy) * CRect.zw;
    float roof = Cov.SampleLevel(View.SharedPointClampedSampler, cuv, 0).g;
    if (roof > 1.8 && wp.z < ground + roof * 100.0) return float3(0, 0, 0);
}
float3 Rv = ResolvedView.ViewToTranslatedWorld[0].xyz;
float3 Uv = ResolvedView.ViewToTranslatedWorld[1].xyz;
float dist = length(rel);
// ~2 cm flakes (clumps); far ones kept at least ~a pixel and fainter (the pixel shader)
float size = max(1.1, dist * 0.0022) * lerp(0.6, 1.4, R1.y) * Style.x;
wp += (Rv * C.x + Uv * C.y) * size;
return wp - P;
"""

FLAKES_PS = r"""
float r = length(C);
float dist = length(WP - Cam);
float a = (1.0 - smoothstep(0.25, 1.0, r)) * saturate(1.1 / max(dist * 0.0022, 1.1));
// nearest ones soft (out of focus), faint up close so they don't blot the view
a *= smoothstep(30.0, 160.0, dist);
return a * 0.85;
"""
