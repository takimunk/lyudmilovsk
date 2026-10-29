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
float d = Dep.SampleLevel(DepSampler, uv, 0).r;
return float3(0, 0, max(d, 0.0) * 100.0);
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
Opacity = cover;
Normal = n;
Roughness = lerp(0.72, 0.42, S.z) - 0.08 * S.y;
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
Roughness = lerp(lerp(0.08, 0.25, cr), 0.7, snow);
return col;
"""

# --- snow on the branches and needles (M_PlantLeaf, M_PlantNeedle): how much of the leaf's colour turns white
LEAF_SNOW = r"""
float up = saturate((N.z - 0.15) * 2.2);
float n = frac(sin(dot(floor(P.xy / 7.0), float2(12.9898, 78.233))) * 43758.5453);
return saturate(up * B.x * (0.7 + 0.5 * n));
"""
