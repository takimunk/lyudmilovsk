"""HLSL for the sky dome's material (unreal/scripts/import_sky.py builds M_LyudSky from it).

The red dwarf and Europa are drawn here, per pixel of the dome, not by the atmosphere's light disks
(ALyudSky turns those off). Parameters (ALyudSky::UpdateSkyMaterial sets them):
  SunDir, MoonDir, Pole = world unit vectors toward the sun, the moon, the celestial north pole
  SunDisk  = (angular radius rad, disk-centre luminance, day seed, the star's spin rad)
  SunSpots = (the spin at the day's noon rad: the face the spots are put on, umbra brightness, granulation
              contrast, faculae strength)
  MoonDisk = (angular radius rad, luminance of full sunlit ice, ashen light share, night 0..1)
  MoonLook = (contrast of the map's midtones, the night halo's strength, 0, 0)
  Europa   = Europa's surface (grey, equirectangular, east longitude 0..360 left to right)
V = the view direction (camera to pixel), T = time s.
"""

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
    // granules: bright cells with dark lanes (Worley F2 - F1); the cells' seeds wander, so the pattern boils
    float Granules(float3 p, float t)
    {
        float3 i = floor(p), f = frac(p);
        float f1 = 8.0, f2 = 8.0, b = 0.5;
        for (int z = -1; z <= 1; z++)
        for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++)
        {
            float3 g = float3(x, y, z);
            float3 h = H33(i + g);
            float3 o = 0.5 + 0.38 * sin(t * (0.6 + h * 0.8) + h * 6.2831853);
            float d = length(g + o - f);
            if (d < f1) { f2 = f1; f1 = d; b = H3(i + g + 71.3); } else if (d < f2) { f2 = d; }
        }
        // a granule: brightest at its middle, each a little brighter or dimmer than the next, a soft dark lane round it
        float lane = smoothstep(0.0, 0.35, f2 - f1);
        return lane * (0.55 + 0.45 * b) * (1.0 - 0.35 * f1);
    }
    // spot k (0..2) of group g of a day's seed: (centre on the unit star, umbra radius rad; 0 = none). Groups stand
    // in two belts north and south of the equator, on the face the star turns to us at the day's noon (face = the
    // spin then), each a big leader with smaller followers trailing it in longitude
    float4 Spot(float seed, int g, int k, float face)
    {
        float gk = seed * 3.17 + g * 2.71;
        float sk = gk * 1.31 + k * 1.618;
        bool on = (g < 3 || H1(gk + 7.0) < 0.55) && (k == 0 || H1(sk + 8.0) < (k == 1 ? 0.8 : 0.5));
        float lat = (H1(gk) < 0.5 ? -1.0 : 1.0) * (0.12 + 0.42 * H1(gk + 1.0)) + (H1(sk + 2.0) - 0.5) * 0.07 * k;
        float lon = face + (H1(gk + 3.0) - 0.5) * 2.4 - k * (0.2 + 0.16 * H1(sk + 4.0));
        float r = k == 0 ? 0.045 + 0.065 * H1(gk + 5.0) : 0.014 + 0.03 * H1(sk + 6.0);
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
    // granulation: ~70 cells across with dark lanes, slowly boiling; finer mottling on top; flatter toward the limb
    float g = L.Granules(q * 34.0, T * 0.05);
    float m = L.Fbm(q * 90.0 + T * 0.01);
    float gc = Spots.z * (0.45 + 0.55 * mu);
    float gran = 1.0 + gc * (0.55 * (g - 0.42) + 0.15 * (m - 0.5));
    // spots: a dark umbra in a striated penumbra, in groups; faculae (bright) round them, plainest near the limb
    float spot = 1.0, fac = 0.0;
    for (int gi = 0; gi < 4; gi++)
    for (int k = 0; k < 3; k++)
    {
        float4 kk = L.Spot(D.z, gi, k, Spots.x);
        if (kk.w <= 0.0) continue;
        float d = acos(clamp(dot(q, kk.xyz), -1.0, 1.0));
        float ru = kk.w, rp = kk.w * 2.2;
        if (d > rp * 3.5) continue;
        float3 t1 = normalize(cross(kk.xyz, float3(0, 1, 0))), t2 = cross(kk.xyz, t1);
        float az = atan2(dot(q, t2), dot(q, t1));
        // the penumbra's filaments run out from the umbra, its edge ragged
        float fil = 0.88 + 0.12 * sin(az * 60.0 + L.Noise(q * 45.0) * 5.0) + 0.06 * (L.Noise(q * 160.0) - 0.5);
        float rag = 1.0 + 0.18 * (L.Noise(q * 70.0 + kk.x * 9.0) - 0.5);
        float pen = 1.0 - smoothstep(rp * 0.85 * rag, rp * 1.02 * rag, d);
        float umb = 1.0 - smoothstep(ru * 0.7, ru * 1.0, d);
        spot *= lerp(1.0, lerp(0.52 * fil, Spots.y, umb), pen);
        fac = max(fac, (1.0 - smoothstep(rp, rp * 3.5, d)) * (1.0 - pen));
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

# Europa: (luminance rgb) at V: a sphere lit per pixel by the true sun (Lommel-Seeliger, a little Lambert),
# its surface the Galileo/Voyager mosaic tinted cream ice with red-brown lineae, ashen light on the night side,
# a faint halo by night. Inputs: V, M (MoonDir), S (SunDir), P (Pole), D (MoonDisk), C (MoonLook: contrast, halo),
# Tr, Europa (texture).
MOON_HLSL = r"""
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
float lon = atan2(dot(n, by), dot(n, bx)) + 1.5708;          // 90 E (270 W: Conamara, Pwyll, the lineae) faces us
float lat = asin(clamp(dot(n, bz), -1.0, 1.0));
float2 uv = float2(frac(lon / 6.2831853), 0.5 - lat / 3.14159265);   // the map runs 0..360 E
float2 dx = ddx(uv), dy = ddy(uv);
dx.x -= round(dx.x); dy.x -= round(dy.x);
if (r > 4.0 || D.y <= 0.0) return 0.0;
float3 col = 0.0;
if (r < 1.0)
{
    float g = Europa.SampleGrad(EuropaSampler, uv, dx, dy).r;
    // ice: cream where bright, the lineae and chaos terrain red-brown where dark; the map's midtones stretched
    // (Contrast) so the lineae read against the ice at a glance, not only in a close-up
    float gs = saturate((g - 0.5) * C.x + 0.5);
    float3 ice = lerp(float3(0.3, 0.12, 0.05), float3(0.97, 0.93, 0.85), smoothstep(0.25, 0.7, gs));
    ice *= 0.12 + 0.95 * gs;
    float mu0 = dot(n, normalize(S.xyz));
    float ls = mu0 > 0.0 ? mu0 / (mu0 + mu) * 2.0 : 0.0;
    float lit = lerp(saturate(mu0), ls, 0.7);
    float ashen = D.z * (0.6 + 0.4 * mu);
    col = ice * (lit + ashen);
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

# How much of the atmosphere's own luminance stays over the two disks (0.35 inside the sun, 0.5 inside Europa by
# night: by day the blue sky over the ice is what shows it at all; 1 outside, a soft rim). Inputs: V, S (SunDir), D (SunDisk), M (MoonDir), MD (MoonDisk).
DISK_MASK_HLSL = r"""
float3 v = normalize(V);
float a = acos(clamp(dot(v, normalize(S.xyz)), -1.0, 1.0));
float b = acos(clamp(dot(v, normalize(M.xyz)), -1.0, 1.0));
return lerp(0.35, 1.0, smoothstep(D.x * 0.97, D.x * 1.03, a)) * lerp(lerp(1.0, 0.5, MD.w), 1.0, smoothstep(MD.x * 0.97, MD.x * 1.03, b));
"""
