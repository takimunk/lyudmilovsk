"""HLSL for the clouds (unreal/scripts/import_sky.py builds M_LyudClouds and the high-cloud part of M_LyudSky from it).

The weather engine gives three cloud layers (low, middle, high), each a genus with cover, base, top and density, plus
lens clouds and shower cells. ULyudCloudSubsystem (Source/.../Sky/LyudClouds.cpp) turns each genus into a handful of
shape numbers and sets them on the materials, so the genus can change smoothly and one shader draws them all:

VOLUMETRIC (M_LyudClouds, the engine's volumetric cloud ray-marches it): the low and the middle layer, the lenses and
the rain shafts. Per layer L0 (low), L1 (middle):
  LxA = (cover 0..1, base km, top km, density 0..1)
  LxB = (cell km: the size of one cloud, sheet 0..1: a smooth deck instead of heaps, top rounding 0..1, anvil 0..1)
  LxC = (erosion 0..1: cauliflower/wispy edges, stretch >= 1 along the wind (streets, rolls), ragged base km,
         cluster 0..1: isolated towers instead of an even field)
  LxD = (offset x km, offset y km: the drift with the wind, downwind unit vector x, y)
  LxE = (rain shafts 0..1 under the base, darkness 0..1 of the base, boil speed, 0)
  Lens = (amount 0..1, base km, top km, lens size km), LensDir = (downwind x, y, 0, 0) of the flow that makes them
  Misc = (time s, extinction per metre at density 1, lightning flash 0.., 0)
  Flash = (x km, y km, altitude km, radius km) of the strike lighting the clouds from inside
Coordinates: world cm (x, y) -> km; the altitude is the volumetric cloud's own (above the planet's top = sea level).
Returns float4(density 0..~1, sky-light occlusion 0..1, flash 0..1, 0).

HIGH (in the sky dome, M_LyudSky): cirrus, cirrocumulus and cirrostratus are drawn as a 2D layer at their altitude on
the dome, behind the volumetric clouds (which the engine composites over it):
  HighA = (cover, altitude km, density, 0), HighB = (fibres 0..1 (Ci), grain 0..1 (Cc), veil 0..1 (Cs), halo 0..1)
  HighD = (offset x km, offset y km, downwind x, y)
"""

LIB = r"""
struct FLyudCloudLib
{
    float H2(float2 p) { p = frac(p * float2(0.1031, 0.1030)); p += dot(p, p.yx + 33.33); return frac((p.x + p.y) * p.x); }
    float Remap(float v, float a, float b) { return saturate((v - a) / max(b - a, 1e-4)); }
    float N2(float2 x)
    {
        float2 i = floor(x), f = frac(x);
        float2 u = f * f * (3.0 - 2.0 * f);
        return lerp(lerp(H2(i), H2(i + float2(1, 0)), u.x), lerp(H2(i + float2(0, 1)), H2(i + float2(1, 1)), u.x), u.y);
    }
    float Fbm2(float2 p, int n)
    {
        float s = 0.0, a = 0.5, w = 0.0;
        for (int o = 0; o < n; o++) { s += a * N2(p); w += a; p = mul(float2x2(1.6, 1.2, -1.2, 1.6), p) + 7.7; a *= 0.5; }
        return s / w;
    }
};
"""

VOLUME_HLSL = LIB + r"""
FLyudCloudLib K;
float3 pk = P * 1e-5;                                  // world km
float h = MaterialExpressionCloudSampleAltitude(Parameters) * 1e-5;
float t = Misc.x;
float dens = 0.0, ao = 1.0, ext = 0.0;

for (int li = 0; li < 2; li++)
{
    float4 A = li == 0 ? L0A : L1A;
    float4 B = li == 0 ? L0B : L1B;
    float4 C = li == 0 ? L0C : L1C;
    float4 D = li == 0 ? L0D : L1D;
    float4 E = li == 0 ? L0E : L1E;
    float cover = A.x, base = A.y, top = A.z;
    if (cover < 0.005) continue;
    float th = max(top - base, 0.05);
    float lowest = base - C.z - (E.x > 0.0 ? base : 0.0);   // ragged base, or the rain shafts down to the ground
    if (h < lowest || h > top) continue;

    float2 w = D.zw, cw = float2(-w.y, w.x);
    float2 q = pk.xy + D.xy;
    float tn = saturate((h - base) / th);
    // towers lean downwind a little; an anvil spreads far downwind at the top
    float spread = B.w * smoothstep(0.62, 0.92, tn);
    q -= w * (tn * tn * th * 0.25 + spread * th * 1.2);
    float cell = max(B.x, 0.05);
    float2 qa = float2(dot(q, w) / max(C.y, 1.0), dot(q, cw));    // stretched along the wind: streets, rolls

    // the field of where the clouds are: one large-scale slice (clusters) and the cloud-scale shape
    float lf = Shape.SampleLevel(ShapeSampler, float3(qa / (cell * 7.0), 0.31 + t * E.z * 2e-5), 0).r;
    float3 p3 = float3(qa / cell, h / (cell * 1.25) + t * E.z * 1e-4);
    float4 s = Shape.SampleLevel(ShapeSampler, p3, 0);
    float n3 = s.r * 0.62 + dot(s.gba, float3(0.2, 0.12, 0.06));
    float field = lerp(n3, lf * 0.55 + n3 * 0.45, 0.35 + 0.55 * C.w);
    // isolated cells: only the peaks of the large-scale field carry clouds
    field *= lerp(1.0, K.Remap(lf, 0.35, 0.75) * 1.3, C.w);

    // the ragged base: the bottom wanders by up to C.z km
    float rag = C.z * (1.0 - Shape.SampleLevel(ShapeSampler, float3(qa / (cell * 2.0), 0.73), 0).g);
    float b2 = base - rag;
    float tb = saturate((h - b2) / max(top - b2, 0.05));

    if (h >= b2)
    {
        // the vertical profile: a flat base, rounded tops (the cover shrinks upward), the anvil widening again
        float shrink = lerp(1.0, 1.0 - tb * tb * 0.85, B.z) + spread * 1.4;
        float c = saturate(cover * shrink);
        float d = K.Remap(field, 1.0 - c, 1.0 - c + max(c, 0.08) * 0.9);
        d *= smoothstep(0.0, lerp(0.04, 0.25, B.y), tb) * smoothstep(1.0, lerp(0.8, 0.55, B.y), tb);
        // a deck: an even layer with only a soft texture
        float sheet = saturate(cover * 1.25) * (0.72 + 0.28 * n3) * smoothstep(0.0, 0.2, tb) * smoothstep(1.0, 0.7, tb);
        sheet *= K.Remap(field + cover, 0.25, 0.65);
        d = lerp(d, sheet, B.y);
        // erosion by fine detail: billows on the tops, wisps at the base
        if (d > 0.0 && C.x > 0.0)
        {
            float3 e = Erosion.SampleLevel(ErosionSampler, p3 * 5.3 + float3(0, 0, t * E.z * 3e-4), 0).rgb;
            float ef = dot(e, float3(0.625, 0.25, 0.125));
            ef = lerp(1.0 - ef, ef, saturate(tb * 5.0));
            float k = ef * C.x * 0.45;
            d = saturate((d - k) / max(1.0 - k, 0.05));
        }
        d *= A.w;
        if (d > 0.0)
        {
            dens += d;
            // the sky light reaches the tops; bases and the insides of thick clouds see less of it
            ao = min(ao, lerp(1.0 - 0.75 * E.y, 1.0, tb));
        }
    }
    else if (E.x > 0.0)
    {
        // rain or snow shafts under the base: grey curtains where the cloud above is thickest, streaked down the wind
        float2 qs = q + w * (b2 - h) * 0.35;
        float2 qsa = float2(dot(qs, w) / max(C.y, 1.0), dot(qs, cw));
        float above = Shape.SampleLevel(ShapeSampler, float3(qsa / cell, b2 / (cell * 1.25)), 0).r;
        float lfs = Shape.SampleLevel(ShapeSampler, float3(qsa / (cell * 7.0), 0.31), 0).r;
        float f2 = lerp(above, lfs * 0.55 + above * 0.45, 0.35 + 0.55 * C.w) * lerp(1.0, K.Remap(lfs, 0.35, 0.75) * 1.3, C.w);
        float core = K.Remap(f2, 1.0 - cover * 0.8, 1.0);
        float streak = Erosion.SampleLevel(ErosionSampler, float3(qsa / (cell * 0.35), h * 0.08 + t * 0.002), 0).r;
        float d = E.x * 0.07 * core * (0.55 + 0.45 * streak) * smoothstep(0.0, 0.4, h) * smoothstep(b2, b2 - 0.3, h - 0.3);
        dens += d * A.w;
        ao = min(ao, 0.5);
    }
}

// lens clouds: smooth stacked plates fixed over the ground (standing waves in strong stable flow), long across the wind
if (Lens.x > 0.0 && h > Lens.y - 0.2 && h < Lens.z + 0.2)
{
    float2 lw = LensDir.xy, lc = float2(-lw.y, lw.x);
    float2 r = float2(dot(pk.xy, lw), dot(pk.xy, lc));
    float G = Lens.w * 3.5;
    float2 ci = floor(r / G);
    if (K.H2(ci + 0.37) < Lens.x)
    {
        float2 f = r - (ci + 0.5) * G - (float2(K.H2(ci + 1.7), K.H2(ci + 5.3)) - 0.5) * G * 0.3;
        float L = Lens.w * (0.55 + 0.6 * K.H2(ci + 9.1));
        float rr = length(float2(f.x / (L * 0.38), f.y / L));
        if (rr < 1.0)
        {
            float hc = lerp(Lens.y, Lens.z, 0.3 + 0.4 * K.H2(ci + 3.3));
            float thick = (Lens.z - Lens.y) * 0.3 * (1.0 - rr * rr);
            float dz = abs(h - hc);
            float plates = 0.6 + 0.4 * cos((h - hc) / max(thick, 0.01) * 6.2831853 * 1.25);
            float edge = Shape.SampleLevel(ShapeSampler, float3(f / (Lens.w * 0.6), hc), 0).g;
            float d = smoothstep(thick, thick * 0.55, dz) * plates * smoothstep(1.0, 0.75 - 0.1 * edge, rr);
            dens += d * 0.9;
            ao = min(ao, lerp(0.7, 1.0, saturate((h - hc + thick) / max(2.0 * thick, 0.01))));
        }
    }
}

// a lightning strike lights the cloud round it from inside
float fl = 0.0;
if (Misc.z > 0.0 && dens > 0.0)
{
    float3 dp = float3(pk.xy, h) - Flash.xyz;
    fl = Misc.z * exp(-dot(dp, dp) / max(Flash.w * Flash.w, 0.01));
}
return float4(dens, ao, fl, 0.0);
"""

# The high clouds on the dome: in front of the sky (and the sun and Europa, which they veil), behind the volumetric ones.
# In: Sky = the dome's colour so far (atmosphere, disks, stars), V = view direction, Cam = camera world cm,
# SunL/MoonL = the light reaching the layer from the sun / the moon (SkyAtmosphereLightIlluminance at the layer),
# Amb = the atmosphere's own luminance there (the sky light the cloud scatters), SunDir, MoonDir, HighA, HighB, HighD.
HIGH_HLSL = LIB + r"""
FLyudCloudLib K;
float3 v = normalize(V);
float cover = HighA.x;
if (cover < 0.005 || v.z < 0.0) return Sky;
float alt = HighA.y;
// the flat layer seen from the camera, with the earth's curvature: far off it sinks toward the horizon
float R = 6360.0;
float z = max(v.z, 0.0);
float tk = -R * z + sqrt(R * R * z * z + 2.0 * R * alt + alt * alt);   // km along the ray to the layer's sphere
float2 p = Cam.xy * 1e-5 + v.xy * tk + HighD.xy;
float2 w = HighD.zw, cw = float2(-w.y, w.x);
float2 pa = float2(dot(p, w), dot(p, cw));
// a pixel's footprint on the layer: fine detail fades out toward the horizon instead of shimmering
float foot = tk * 0.0015;

// cirrus: fibres combed along the upper wind, hooked, in bands
float band = K.Fbm2(pa * float2(0.018, 0.05) + 3.1, 3);
float fib = K.Fbm2(pa * float2(0.12, 1.6) + float2(K.Fbm2(pa * 0.06, 2) * 4.0, 0.0), foot < 0.5 ? 4 : 2);
float hook = K.Fbm2(pa * float2(0.35, 0.9) + 11.0, 2);
float ci = K.Remap(band * 0.6 + fib * 0.55 + hook * 0.1, 1.0 - cover * 0.95, 1.05) * HighB.x;
// cirrocumulus: tiny grains in ripples
float rip = 0.5 + 0.5 * sin(pa.x * 9.0 + K.Fbm2(pa * 0.4, 2) * 6.0);
float gr = K.N2(pa * float2(14.0, 9.0)) * rip;
float cc = K.Remap(band * 0.5 + gr * lerp(0.6, 0.1, saturate(foot * 3.0)), 1.0 - cover * 0.9, 1.0) * HighB.y;
// cirrostratus: a thin even veil with faint fibres in it
float cs = saturate(cover * 1.1) * (0.75 + 0.25 * fib) * HighB.z;
float a = saturate(max(max(ci, cc), cs) * HighA.z);
// far off the layer thins into the haze
a *= smoothstep(0.0, 0.06, z);
if (a <= 0.0) return Sky;

// light: forward-scattering ice crystals (bright toward the sun), the sky's own light from all round
float3 sd = normalize(SunDir.xyz), md = normalize(MoonDir.xyz);
float cs1 = dot(v, sd), cm1 = dot(v, md);
float ph = 0.08 + 0.55 * pow(saturate(cs1 * 0.5 + 0.5), 8.0) + 0.12 * pow(saturate(cs1 * 0.5 + 0.5), 2.0);
float phm = 0.08 + 0.55 * pow(saturate(cm1 * 0.5 + 0.5), 8.0);
float3 lit = SunL.rgb * ph * 0.05 + MoonL.rgb * phm * 0.05;
float3 amb = Amb.rgb * 0.9;
float3 col = lit + amb;
// the 22-degree halo in a cirrostratus veil round the sun (and Europa): a ring, reddish inside
float ang = acos(clamp(cs1, -1.0, 1.0));
float ring = exp(-pow((ang - 0.384) / 0.02, 2.0)) * HighB.w * cs;
float3 halo = SunL.rgb * 0.004 * ring * float3(0.9 + 0.3 * saturate((ang - 0.384) / 0.03), 1.0, 1.0 + 0.2 * saturate((ang - 0.384) / 0.03));
float angm = acos(clamp(cm1, -1.0, 1.0));
halo += MoonL.rgb * 0.004 * exp(-pow((angm - 0.384) / 0.02, 2.0)) * HighB.w * cs;
// the disks and stars behind are veiled; the sky's own glow mostly stays (the cloud is lit by the same sky)
return Sky * (1.0 - a * 0.65) + col * a + halo;
"""
