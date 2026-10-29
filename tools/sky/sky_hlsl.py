"""HLSL for the sky dome's material (unreal/scripts/import_sky.py builds M_LyudSky from it).

The red dwarf and Europa are drawn here, per pixel of the dome, not by the atmosphere's light disks
(ALyudSky turns those off). Parameters (ALyudSky::UpdateSkyMaterial sets them):
  SunDir, MoonDir, Pole = world unit vectors toward the sun, the moon, the celestial north pole
  SunDisk  = (angular radius rad, disk-centre luminance, day seed, the star's spin rad)
  SunSpots = (the spin at the day's noon rad: the face the spots are put on, umbra brightness, granulation
              contrast, faculae strength)
  MoonDisk = (angular radius rad, luminance of full sunlit ice, ashen light share, night 0..1)
  MoonLook = (unused, the night halo's strength, 0, 0)
  EuropaAlbedo, EuropaRelief = Europa's baked maps (tools/sky/europa_bake.py; equirectangular, east longitude 0..360
             left to right): albedo; relief normal (east, north) and height
V = the view direction (camera to pixel), T = time s.
"""

import json
import os

LIB = r"""
struct FLyudSkyLib
{
    float H1(float n) { return frac(sin(n * 12.9898 + 4.1414) * 43758.5453); }
    float H3(float3 p)
    {
        p = frac(p * 0.3183099 + 0.1);
        p *= 17.0;
        return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
    }
    float3 H33(float3 p)
    {
        return float3(H3(p), H3(p + 19.19), H3(p + 47.77));
    }
    float Noise(float3 x)
    {
        float3 i = floor(x), f = frac(x);
        float3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
        float a = H3(i), b = H3(i + float3(1, 0, 0)), c = H3(i + float3(0, 1, 0)), d = H3(i + float3(1, 1, 0));
        float e = H3(i + float3(0, 0, 1)), f1 = H3(i + float3(1, 0, 1)), g = H3(i + float3(0, 1, 1)), h = H3(i + float3(1, 1, 1));
        return lerp(lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y), lerp(lerp(e, f1, u.x), lerp(g, h, u.x), u.y), u.z);
    }
    float Fbm(float3 p)
    {
        float n = 0.0, a = 0.5;
        for (int o = 0; o < 4; o++) { n += a * Noise(p); p = p * 2.07 + 13.1; a *= 0.5; }
        return n / 0.9375;
    }
    // granules: soft bright cells parted by thin dark lanes (Worley F2 - F1 on a warped grid, so no straight polygon
    // edges); each a gentle dome, the lane's depth varying along it; the cells' seeds wander, so the pattern boils
    float Granules(float3 p, float t)
    {
        p += float3(Noise(p * 0.7 + 3.1), Noise(p * 0.7 + 17.3), Noise(p * 0.7 + 29.9)) * 0.9 - 0.45;
        float3 i = floor(p), f = frac(p);
        float f1 = 8.0, f2 = 8.0, b = 0.5;
        for (int z = -1; z <= 1; z++)
        for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++)
        {
            float3 g = float3(x, y, z);
            float3 h = H33(i + g);
            float3 o = 0.5 + 0.4 * sin(t * (0.6 + h * 0.8) + h * 6.2831853);
            float d = length(g + o - f);
            if (d < f1) { f2 = f1; f1 = d; b = H3(i + g + 71.3); } else if (d < f2) { f2 = d; }
        }
        float e = f2 - f1;
        float lane = 1.0 - exp(-e * e * 30.0);
        float dome = 1.0 - 0.5 * smoothstep(0.0, 0.9, f1);
        return lerp(0.25, 1.0, lane) * dome * (0.85 + 0.15 * b);
    }
    // member k (0 leader, 1..4 followers) of spot group g (0..2) of a day's seed: (centre on the unit star, umbra
    // radius rad; 0 = none). One to three groups, in two belts north and south of the equator, on the face the star
    // turns to us at the day's noon (face = the spin then); a big leader, ragged smaller followers and pores trailing
    // it in longitude
    float4 Spot(float seed, int g, int k, float face)
    {
        float gk = seed * 3.17 + g * 2.71;
        float sk = gk * 1.31 + k * 1.618;
        float pg = g == 0 ? 1.0 : (g == 1 ? 0.6 : 0.3);
        float pk = k == 0 ? 1.0 : 0.9 - 0.15 * k;
        bool on = H1(gk + 7.0) < pg && H1(sk + 8.0) < pk;
        float big = g == 0 ? 1.0 : 0.55 + 0.45 * H1(gk + 9.0);
        float lat = (H1(gk) < 0.5 ? -1.0 : 1.0) * (0.14 + 0.4 * H1(gk + 1.0)) + (H1(sk + 2.0) - 0.5) * 0.02 * min(k, 1);
        float lon = face + (H1(gk + 3.0) - 0.5) * 2.2 - k * (0.02 + 0.03 * H1(sk + 4.0)) - (k > 0 ? 0.03 * big : 0.0);
        float r = k == 0 ? (0.007 + 0.009 * H1(gk + 5.0)) * big : 0.0025 + 0.004 * H1(sk + 6.0) * big;
        return float4(cos(lat) * sin(lon), sin(lat), cos(lat) * cos(lon), on ? r : 0.0);
    }
};
"""

# The red dwarf: (luminance rgb) at view direction V, plus the corona and prominences around it.
# Inputs: V, S (SunDir), P (Pole), D (SunDisk), Spots (SunSpots), T (time s), Tr (atmosphere transmittance rgb toward the sun).
SUN_HLSL = r"""
FLyudSkyLib L;
float3 v = normalize(V);
float3 s = normalize(S.xyz);
float R = D.x;
float c = dot(v, s);
// disk frame: north from the celestial pole, east across
float3 nn = normalize(P.xyz - s * dot(P.xyz, s));
float3 ee = cross(s, nn);
float3 w = v - s * c;
float2 u = float2(dot(w, ee), dot(w, nn)) / sin(R);
float r = c > 0.0 ? length(u) : 100.0;
float fw = max(fwidth(r), 1e-4);
if (r > 3.5 || D.y <= 0.0) return 0.0;
// a ~3000 K star, deep red-orange, darker and redder toward the limb (deeper than the light: the tonemapper pales it)
const float3 Hot = float3(1.0, 0.31, 0.075);
const float3 Limb = float3(0.55, 0.07, 0.015);
float3 col = 0.0;
if (r < 1.0)
{
    float mu = sqrt(1.0 - r * r);
    // a point on the star in its own frame (y = its north, z = toward us), turned with its spin
    float3 p = float3(u.x, u.y, mu);
    float sp = D.w;
    float3 q = float3(p.x * cos(sp) + p.z * sin(sp), p.y, -p.x * sin(sp) + p.z * cos(sp));
    // strong limb darkening (a cool convective star): ~0.15 of the centre at the very edge
    float limb = 0.15 + 0.85 * pow(mu, 0.85);
    float3 base = lerp(Limb, Hot, pow(mu, 0.6)) * limb;
    // granulation at its true scale: a granule is ~1/1000 of the diameter, so ~1000 across the disk, at best a
    // couple of pixels each full-screen: a fine, barely resolved mottling, faded to its mean where a pixel covers
    // several (else it shimmers); mesogranulation and supergranulation as faint broad patches; slowly boiling
    float aa = 1.0 - smoothstep(0.0008, 0.0025, fw);
    float g1 = aa > 0.0 ? L.Granules(q * 500.0, T * 0.15) : 0.62;
    float meso = L.Fbm(q * 45.0 + T * 0.003);
    float sup = L.Noise(q * 14.0 + 7.0);
    float gc = Spots.z * (0.4 + 0.6 * mu);
    float gran = 1.0 + gc * (aa * 0.14 * (g1 - 0.62) + 0.035 * (meso - 0.5) + 0.03 * (sup - 0.5));
    // spots: small and subtle (Dima: barely visible dark points, a little darker than the photosphere): a few pores
    // and one or two small spots with a slight, soft penumbra, a ragged edge
    float spot = 1.0, fac = 0.0;
    for (int gi = 0; gi < 3; gi++)
    for (int k = 0; k < 5; k++)
    {
        float4 kk = L.Spot(D.z, gi, k, Spots.x);
        if (kk.w <= 0.0) continue;
        float ru = kk.w;
        float3 dq = q - kk.xyz;
        if (dot(dq, dq) > ru * ru * 36.0) continue;
        float hs = L.H1(D.z * 1.7 + gi * 5.3 + k * 2.9);
        float3 t1 = normalize(cross(float3(0, 1, 0), kk.xyz)), t2 = cross(kk.xyz, t1);
        float2 rq = float2(dot(dq, t1), dot(dq, t2));
        float el = 1.0 + 0.5 * frac(hs * 7.31);
        rq *= float2(1.0 / sqrt(el), sqrt(el));
        float d = length(rq);
        float2 cs = rq / max(d, 1e-6);
        float3 sd = float3(hs * 37.0, k * 11.0, gi * 7.0);
        float ue = ru * (0.8 + 0.4 * L.Noise(float3(cs * 2.2, 0.0) + sd));
        bool pore = k > 0 || ru < 0.006;
        float pe = pore ? ue * 1.6 : ue * (1.9 + 0.4 * L.Noise(float3(cs * 1.8, 2.0) + sd));
        float fil = 0.9 + 0.1 * L.Noise(float3(cs * 16.0, d / ru * 0.6) + sd);
        float x = saturate((d - ue) / max(pe - ue, 1e-5));
        float pen = 1.0 - smoothstep(0.0, 1.0, x);
        float umb = 1.0 - smoothstep(0.7, 1.0, d / ue);
        float dark = lerp(lerp(1.0, 0.86 * fil, pen * (pore ? 0.5 : 1.0)), Spots.y, umb);
        spot *= dark;
        fac = max(fac, (1.0 - smoothstep(pe, pe * 4.0, d)) * (1.0 - pen));
    }
    // faculae: bright patches round the groups and a network near the limb, where they show best
    float facNoise = smoothstep(0.42, 0.75, L.Fbm(q * 20.0 + 5.0));
    float net = smoothstep(0.55, 0.85, L.Fbm(q * 13.0 + 11.0)) * 0.6;
    float faculae = 1.0 + Spots.w * (fac * facNoise + net) * pow(1.0 - mu, 0.6);
    col = base * gran * spot * faculae;
    // a soft edge, a pixel or so wide
    col *= saturate((1.0 - r) / fw);
}
// the corona: a soft orange glow off the limb
float h = max(r - 1.0, 0.0);
float corona = 0.05 * exp(-h * 10.0) + 0.012 * exp(-h * 2.5);
// prominences: now and then an arc of glowing gas standing on the limb (new ones each day)
float prom = 0.0;
if (r > 0.97 && r < 1.35)
{
    float phi = atan2(u.y, u.x);
    for (int j = 0; j < 3; j++)
    {
        float pk = D.z * 5.13 + j * 3.7;
        if (L.H1(pk) < 0.4) continue;
        float at = L.H1(pk + 1.0) * 6.2831853;
        float dphi = atan2(sin(phi - at), cos(phi - at));
        float a = 0.05 + 0.09 * L.H1(pk + 2.0);
        float x = dphi, y = r - 0.985;
        float arc = abs(sqrt(x * x + y * y * 1.3) - a);
        float n = L.Fbm(float3(phi * 30.0, r * 40.0, T * 0.02 + pk));
        prom += exp(-arc * arc / (0.0004 + 0.0006 * n)) * smoothstep(0.0, 0.02, y) * (0.5 + n);
    }
}
float3 glow = (float3(1.0, 0.36, 0.12) * corona + float3(1.0, 0.25, 0.15) * prom * 0.5) * smoothstep(0.985, 1.0, r);
return (col + glow) * D.y * Tr;
"""

# Europa's baked relief (tools/sky/europa_bake.py): B = height * HEIGHT_GAIN + 0.5, height unit = relief_scale of
# Europa's radius
_BAKE = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "europa_bake.json")))
_RELIEF = "const float HG = %g, RS = %g;\nconst float2 MAPSZ = float2(%d.0, %d.0);\n" % (
    0.4, _BAKE["relief_scale"], _BAKE["width"], _BAKE["height"])

# Europa: (luminance rgb) at V: a sphere lit per pixel by the true sun, its surface our procedural Europa baked into
# two maps (albedo; relief normal + height): Lommel-Seeliger over the sphere, the relief's normals shading its ridges,
# bands and chaos blocks, and their shadows (a short march toward the sun over the height map) long near the
# terminator; ashen light on the night side, a faint halo by night. Inputs: V, M (MoonDir), S (SunDir), P (Pole),
# D (MoonDisk), C (MoonLook: halo in y, phase law in z), Tr, Europa (albedo), Relief.
MOON_HLSL = _RELIEF + r"""
float3 v = normalize(V);
float3 m = normalize(M.xyz);
float R = D.x;
float c = dot(v, m);
float3 nn = normalize(P.xyz - m * dot(P.xyz, m));
float3 ee = cross(m, nn);
float3 w = v - m * c;
float2 u = float2(dot(w, ee), dot(w, nn)) / sin(R);
float r = c > 0.0 ? length(u) : 100.0;
float fw = max(fwidth(r), 1e-4);
// the surface's normal (world) where the view meets the sphere; the map's coordinates there (derivatives
// taken before any branch: Metal wants them in uniform flow)
float mu = sqrt(saturate(1.0 - r * r));
float2 uc = u / max(r, 1.0);
float3 n = ee * uc.x + nn * uc.y - m * mu;
// the body frame: x toward us (the face it always turns to us), z north
float3 bx = -m, bz = nn, by = cross(bz, bx);
float lon = atan2(dot(n, by), dot(n, bx)) + 4.9742;          // 285 E faces us
float lat = asin(clamp(dot(n, bz), -1.0, 1.0));
float2 uv = float2(frac(lon / 6.2831853), 0.5 - lat / 3.14159265);   // the maps run 0..360 E
float2 dx = ddx(uv), dy = ddy(uv);
dx.x -= round(dx.x); dy.x -= round(dy.x);
float mip = max(log2(max(length(dx * MAPSZ), length(dy * MAPSZ))), 0.0);
if (r > 4.0 || D.y <= 0.0) return 0.0;
float3 col = 0.0;
if (r < 1.0)
{
    float3 alb = Europa.SampleGrad(EuropaSampler, uv, dx, dy).rgb;
    float4 rl = Relief.SampleGrad(ReliefSampler, uv, dx, dy);
    // the relief's normal in the surface's frame (east, north, up)
    float3 E = cross(bz, n);
    E = dot(E, E) > 1e-8 ? normalize(E) : ee;
    float3 N = cross(n, E);
    float2 sl = rl.xy * 2.0 - 1.0;
    float3 rn = normalize(E * sl.x + N * sl.y + n * sqrt(saturate(1.0 - dot(sl, sl))));
    float3 s = normalize(S.xyz);
    float mg = dot(n, s), mr = dot(rn, s);
    // Lommel-Seeliger over the sphere (an icy moon's flat-looking full disk), the relief's own slopes on top
    float ls = mg > 0.0 ? mg / (mg + mu) * 2.0 : 0.0;
    // C.z: Europa's phase law over Lommel-Seeliger (opposition surge, rough-surface shadows), the same factor
    // ALyudSky gives its light
    float lit = ls * clamp(saturate(mr) / max(mg, 0.08), 0.0, 1.8) * smoothstep(-0.03, 0.04, mg) * (C.z > 0.0 ? C.z : 1.0);
    // the relief's shadows: march from here toward the sun over the height map; they lengthen near the terminator
    float sh = 1.0;
    if (mg > -0.03 && mg < 0.45)
    {
        float2 sd = float2(dot(s, E), dot(s, N));
        float sdl = max(length(sd), 1e-4);
        float tanE = mg / sdl;
        float2 duv = sd / sdl * float2(1.0 / (6.2831853 * max(cos(lat), 0.05)), -1.0 / 3.14159265);
        float h0 = (rl.z - 0.5) / HG * RS;
        for (int i = 1; i <= 12; i++)
        {
            float t = 0.0015 * i * i * 0.25 + 0.0008 * i;              // radians along the ground, up to ~0.03
            float hz = (Relief.SampleLevel(ReliefSampler, uv + duv * t, mip).z - 0.5) / HG * RS;
            float ray = h0 + t * tanE;
            sh = min(sh, saturate(1.0 + (ray - hz) / (RS * 0.12)));
        }
    }
    float ashen = D.z * (0.6 + 0.4 * mu);
    col = alb * (lit * sh + ashen);
    col *= saturate((1.0 - r) / fw);
}
// a faint halo by night (moonlight in the haze)
float h = max(r - 1.0, 0.0);
float3 halo = float3(0.55, 0.65, 0.85) * C.y * exp(-h * 4.0) * D.w;
return (col + halo) * D.y * Tr;
"""

# Atmospheric transmittance toward a direction (altitude from its z): an analytic extinction for a damp
# Baltic air (Kasten & Young air mass), so both disks dim and redden near the horizon.
TRANS_HLSL = r"""
float alt = degrees(asin(clamp(normalize(Dir.xyz).z, -1.0, 1.0)));
float h = max(alt, 0.0);
float am = 1.0 / (sin(radians(h)) + 0.50572 * pow(h + 6.07995, -1.6364));
float3 tau = float3(0.07, 0.14, 0.30);
// 1 at the zenith; under the horizon the disk sinks into the ground's haze
return exp(-tau * (am - 1.0)) * smoothstep(-6.0, 0.5, alt);
"""

# Stars fade toward the horizon and near the bright disks. Inputs: V, M (MoonDir), S (SunDir), MD (MoonDisk), SD (SunDisk).
STAR_MASK_HLSL = r"""
float3 v = normalize(V);
float horizon = saturate(v.z * 3.0 - 0.05);
float am = acos(clamp(dot(v, normalize(M.xyz)), -1.0, 1.0));
float as = acos(clamp(dot(v, normalize(S.xyz)), -1.0, 1.0));
return horizon * smoothstep(MD.x, MD.x * 3.5, am) * smoothstep(SD.x, SD.x * 2.0, as);
"""

# How much of the atmosphere's own luminance stays over the two disks (0.35 inside the sun, 0.2 inside Europa by
# night, so the blue night sky does not tint the ice lavender; by day the blue sky over the ice is what shows it at
# all; 1 outside, a soft rim). Inputs: V, S (SunDir), D (SunDisk), M (MoonDir), MD (MoonDisk).
DISK_MASK_HLSL = r"""
float3 v = normalize(V);
float a = acos(clamp(dot(v, normalize(S.xyz)), -1.0, 1.0));
float b = acos(clamp(dot(v, normalize(M.xyz)), -1.0, 1.0));
return lerp(0.35, 1.0, smoothstep(D.x * 0.97, D.x * 1.03, a)) * lerp(lerp(1.0, 0.2, MD.w), 1.0, smoothstep(MD.x * 0.97, MD.x * 1.03, b));
"""
