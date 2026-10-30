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
  Misc = (time s, the tracing's max distance km (the clouds fade out before it), lightning flash 0.., 0)
  Cam = the camera, world cm
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
    // the engine's cloud noise volumes hold one Perlin-Worley channel packed into a narrow band: spread it to 0..1
    float S(float v) { return saturate((v - 0.80) / 0.17); }
    float Er(float v) { return saturate((v - 0.55) / 0.33); }
    float N2(float2 x)
    {
        float2 i = floor(x), f = frac(x);
        float2 u = f * f * (3.0 - 2.0 * f);
        return lerp(lerp(H2(i), H2(i + float2(1, 0)), u.x), lerp(H2(i + float2(0, 1)), H2(i + float2(1, 1)), u.x), u.y);
    }
    // thunderstorm cells: 1 at a cell's axis .. 0 at its edge (negative outside), tb = height in the cloud 0..1
    float Cells(float2 q, float cell, float cover, float anvil, float tb)
    {
        float G = cell * 4.5;
        float2 gi = floor(q / G);
        float best = -1.0;
        for (int gy = -1; gy <= 1; gy++)
        for (int gx = -1; gx <= 1; gx++)
        {
            float2 g = gi + float2(gx, gy);
            if (H2(g + 0.71) > saturate(cover * 0.9)) continue;
            float2 cpos = (g + 0.5 + (float2(H2(g + 2.3), H2(g + 8.9)) - 0.5) * 0.5) * G;
            float size = 0.7 + 0.6 * H2(g + 4.4);
            float R = cell * 0.45 * size * (1.0 - 0.2 * tb) + anvil * cell * 0.9 * size * smoothstep(0.72, 0.95, tb);
            best = max(best, 1.0 - length(q - cpos) / max(R, 0.05));
        }
        return best;
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
// how far the sample is: fine detail and thin fringes that a far pixel can't resolve go (they read as bright noisy
// specks against a low sun: "confetti")
float far = saturate(length(P - Cam) * 1e-5 / 18.0);
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
    float spread = B.w * smoothstep(0.7, 0.93, tn);
    q -= w * (tn * tn * th * 0.2 + spread * th * 0.45);
    float cell = max(B.x, 0.05);
    float2 qa = float2(dot(q, w) / max(C.y, 1.0), dot(q, cw));    // stretched along the wind: streets, rolls

    // the field of where the clouds are: one large-scale slice (clusters) and the cloud-scale shape
    float lf = K.S(Shape.SampleLevel(ShapeSampler, float3(qa / (cell * 14.0), 0.31 + t * E.z * 2e-5), 0).r);
    float3 p3 = float3(qa / (cell * 3.0), h / (cell * 3.5) + t * E.z * 1e-4);
    // the cloud-scale shape and one finer octave of the same noise
    float n3c = K.S(Shape.SampleLevel(ShapeSampler, p3, 0).r);
    float n3 = far < 0.99 ? lerp(n3c * 0.75 + K.S(Shape.SampleLevel(ShapeSampler, p3 * 2.7 + 0.37, 0).r) * 0.25, n3c, far) : n3c;
    float field = lerp(n3, lf * 0.55 + n3 * 0.45, 0.35 + 0.55 * C.w);
    // isolated cells: only the peaks of the large-scale field carry clouds
    field *= lerp(1.0, K.Remap(lf, 0.35, 0.75) * 1.3, C.w);

    // the ragged base: the bottom wanders by up to C.z km
    float rag = C.z * (1.0 - K.S(Shape.SampleLevel(ShapeSampler, float3(qa / (cell * 5.0), 0.73), 0).r));
    float b2 = base - rag;
    float tb = saturate((h - b2) / max(top - b2, 0.05));

    if (h >= b2)
    {
        // the vertical profile: a flat base, rounded tops (the cover shrinks upward), the anvil widening again
        float shrink = lerp(1.0, 1.0 - tb * tb * 0.85, B.z) + spread * 1.4;
        // a thin sky of heaps is fewer clouds, not smaller ones: below CMIN the clouds keep the shape of a CMIN sky and
        // a large-scale mask thins them out (tiny peaks of the noise were noisy, translucent, bright specks against the
        // sun); decks are not affected
        const float CMIN = 0.3;
        float thin = (1.0 - B.y) * saturate(1.0 - cover / CMIN);
        float c = saturate(lerp(cover, CMIN, thin) * shrink);
        float d = K.Remap(field, 1.0 - c, 1.0 - c + max(c, 0.08) * 0.9);
        if (thin > 0.0)
        {
            float m = K.S(Shape.SampleLevel(ShapeSampler, float3(qa / (cell * 9.0), 0.83), 0).r);
            float keep = saturate(cover / CMIN);
            d *= lerp(1.0, K.Remap(m, 1.0 - keep - 0.06, 1.0 - keep + 0.06), thin);
        }
        if (B.w > 0.001)
        {
            // thunderstorms: separate giant cells on a fixed grid, each a tower narrowing a little upward under an
            // anvil that flares out downwind at the top (the anvil's shift is already in q); blended in as the anvil
            // grows, so congestus turns into cumulonimbus without a jump
            float best = K.Cells(q, 6.0, cover, B.w, tb);
            float dc = K.Remap(best + (n3 - 0.5) * 0.5, 0.0, 0.3) * smoothstep(1.0, 0.9, tb);
            d = lerp(d, dc, saturate(B.w * 2.0));
        }
        d *= smoothstep(0.0, lerp(0.04, 0.25, B.y), tb) * smoothstep(1.0, lerp(0.8, 0.55, B.y), tb);
        // a deck: an even layer with only a soft texture
        float sheet = saturate(cover * 1.25) * (0.72 + 0.28 * n3) * smoothstep(0.0, 0.2, tb) * smoothstep(1.0, 0.7, tb);
        sheet *= K.Remap(field + cover, 0.25, 0.65);
        d = lerp(d, sheet, B.y);
        // erosion by fine detail: billows on the tops, wisps at the base
        if (d > 0.0 && C.x > 0.0 && far < 0.99)
        {
            float3 pe = p3 * 4.1 + float3(0, 0, t * E.z * 3e-4);
            float ef = K.Er(Erosion.SampleLevel(ErosionSampler, pe, 0).r) * 0.65 + K.Er(Erosion.SampleLevel(ErosionSampler, pe * 2.3 + 0.5, 0).r) * 0.35;
            ef = lerp(1.0 - ef, ef, saturate(tb * 5.0));
            float k = ef * C.x * 0.45 * (1.0 - far);
            d = saturate((d - k) / max(1.0 - k, 0.05));
        }
        // no faint translucent fringe: a cloud's edge goes from clear to its body over a short band, so the few
        // ray-march samples a small far cloud gets don't each catch a different wisp
        d = smoothstep(0.04, 0.25, d) * d;
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
        float above = K.S(Shape.SampleLevel(ShapeSampler, float3(qsa / (cell * 3.0), b2 / (cell * 3.5)), 0).r);
        float lfs = K.S(Shape.SampleLevel(ShapeSampler, float3(qsa / (cell * 14.0), 0.31), 0).r);
        float f2 = lerp(above, lfs * 0.55 + above * 0.45, 0.35 + 0.55 * C.w) * lerp(1.0, K.Remap(lfs, 0.35, 0.75) * 1.3, C.w);
        float core = lerp(K.Remap(f2, 1.0 - cover * 0.8, 1.0), B.w > 0.001 ? K.Remap(K.Cells(qs, 6.0, cover, 0.0, 0.0), 0.1, 0.6) : 0.0, saturate(B.w * 2.0));
        float streak = K.S(Shape.SampleLevel(ShapeSampler, float3(qsa / (cell * 1.5), h * 0.03 + t * 0.0005), 0).r);
        float d = E.x * 0.02 * core * (0.6 + 0.4 * streak) * smoothstep(0.0, 0.4, h) * smoothstep(b2, b2 - 0.3, h - 0.3);
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
            float thick = (Lens.z - Lens.y) * 0.42 * sqrt(saturate(1.0 - rr * rr));
            float dz = abs(h - hc);
            float plates = 0.8 + 0.2 * cos((h - hc) * 6.2831853 / 0.3);
            float edge = K.S(Shape.SampleLevel(ShapeSampler, float3(f / (Lens.w * 0.6), hc), 0).r);
            float d = smoothstep(thick, thick * 0.4, dz) * plates * smoothstep(1.0, 0.7 - 0.15 * edge, rr) * (0.75 + 0.25 * edge);
            dens += d * 0.45;
            ao = min(ao, lerp(0.85, 1.0, saturate((h - hc + thick) / max(2.0 * thick, 0.01))));
        }
    }
}

// fade out before the ray march stops (a hard arc in the sky otherwise)
float dcam = length(pk - Cam * 1e-5);
dens *= smoothstep(Misc.y, Misc.y * 0.9, dcam);

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
float band = K.Fbm2(pa * float2(0.03, 0.06) + 3.1, 3);
float fib = K.Fbm2(pa * float2(0.35, 1.4) + float2(K.Fbm2(pa * 0.08, 2) * 5.0, K.Fbm2(pa * 0.11 + 5.0, 2) * 2.0), foot < 0.5 ? 4 : 2);
float hook = K.Fbm2(pa * float2(0.35, 0.9) + 11.0, 2);
float ci = K.Remap(band * 0.75 + fib * 0.45 + hook * 0.1, 0.95 - cover * 0.6, 1.15) * HighB.x;
// cirrocumulus: tiny grains in ripples
float rip = 0.5 + 0.5 * sin(pa.x * 4.0 + K.Fbm2(pa * 0.3, 2) * 6.0);
float gr = K.N2(pa * float2(5.0, 3.5)) * (0.6 + 0.4 * rip);
gr = lerp(gr, 0.5, saturate(foot * 4.0));
float cc = K.Remap(band * 0.6 + gr * 0.6, 0.95 - cover * 0.6, 1.05) * HighB.y;
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
float3 lit = SunL.rgb * ph * 0.12 + MoonL.rgb * phm * 0.12;
float3 amb = Amb.rgb * 1.3;
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
