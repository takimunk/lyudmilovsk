"""HLSL for the rare sky (docs/rare_sky.md): the aurora, noctilucent clouds, the lightning's glow and the red dwarf's
flares on the star dome (M_LyudSky, import_sky.py adds them), the halos and rainbows of M_LyudSkyOptics and the bolt
of M_LyudLightning (both on camera-centred spheres that ALyudRareSky moves). ALyudRareSky sets the parameters from
RareSkyModel (Private/Sky/RareSkyModel.h).

The optics are worked out for OUR sky, not Earth's: the light is a ~3200 K red dwarf 10 degrees across (Europa, 7
degrees, by night). Every bow and halo is the point-source pattern convolved over the limb-darkened disk, summed over
the spectrum weighted by the star's own light, so they come out broad and soft (a 2-degree spread of colours under a
10-degree blur) and warm (little blue light to split off): a pale orange-gold bow with a reddish rim, a wide halo.

Spectral constants (rainbow and halo angles per wavelength, the star's colour per wavelength in linear sRGB) are
computed here at build time.
"""

import math

# ---------------------------------------------------------------------------------------------------------------------
# the spectrum
WAVES = [410.0 + 25.0 * i for i in range(13)]          # 410..710 nm
STAR_K = 3200.0


def _g(x, mu, s1, s2):
    s = s1 if x < mu else s2
    return math.exp(-0.5 * ((x - mu) / s) ** 2)


def _xyz(l):
    """CIE 1931 colour matching (Wyman, Sloan, Shirley 2013 multi-lobe fit)."""
    x = 1.056 * _g(l, 599.8, 37.9, 31.0) + 0.362 * _g(l, 442.0, 16.0, 26.7) - 0.065 * _g(l, 501.1, 20.4, 26.2)
    y = 0.821 * _g(l, 568.8, 46.9, 40.5) + 0.286 * _g(l, 530.9, 16.3, 31.1)
    z = 1.217 * _g(l, 437.0, 11.8, 36.0) + 0.681 * _g(l, 459.0, 26.0, 13.8)
    return x, y, z


def _rgb(x, y, z):
    return (3.2406 * x - 1.5372 * y - 0.4986 * z, -0.9689 * x + 1.8758 * y + 0.0415 * z, 0.0557 * x - 0.2040 * y + 1.0570 * z)


def _planck(l, t):
    lm = l * 1e-9
    return 1.0 / (lm ** 5 * (math.exp(1.4388e-2 / (lm * t)) - 1.0))


def _weights():
    """Per wavelength: the star's light in linear sRGB, scaled so that the whole spectrum sums to luminance 1."""
    w = []
    for l in WAVES:
        p = _planck(l, STAR_K)
        w.append(tuple(c * p for c in _rgb(*_xyz(l))))
    lum = sum(0.2126 * r + 0.7152 * g + 0.0722 * b for r, g, b in w)
    return [(r / lum, g / lum, b / lum) for r, g, b in w]


def n_water(l):
    return 1.3228 + 3270.0 / (l * l)


def n_ice(l):
    return 1.30159 + 2850.0 / (l * l)


def bow_angles(n):
    """Primary and secondary rainbow radii (deg from the antisolar point) for a refractive index (Descartes)."""
    i1 = math.acos(math.sqrt((n * n - 1.0) / 3.0))
    r1 = math.asin(math.sin(i1) / n)
    p = 180.0 - math.degrees(math.pi + 2 * i1 - 4 * r1)
    i2 = math.acos(math.sqrt((n * n - 1.0) / 8.0))
    r2 = math.asin(math.sin(i2) / n)
    s = math.degrees(2 * i2 - 6 * r2 + 2 * math.pi) - 180.0
    return p, s


def halo_angles(n):
    """The 22 and 46 degree halos' radii: minimum deviation through 60 and 90 degree ice prisms."""
    return (math.degrees(2 * math.asin(n * math.sin(math.radians(30)))) - 60.0,
            math.degrees(2 * math.asin(n * math.sin(math.radians(45)))) - 90.0)


def _arr(name, vals, fmt="%.5f"):
    return "static const float %s[%d] = { %s };\n" % (name, len(vals), ", ".join(fmt % v for v in vals))


def _arr3(name, vals):
    return "static const float3 %s[%d] = { %s };\n" % (name, len(vals), ", ".join("float3(%.5f, %.5f, %.5f)" % v for v in vals))


W = _weights()
SPECTRAL = (_arr3("SpecW", W) + _arr("Bow1", [bow_angles(n_water(l))[0] for l in WAVES]) +
            _arr("Bow2", [bow_angles(n_water(l))[1] for l in WAVES]) + _arr("Halo22", [halo_angles(n_ice(l))[0] for l in WAVES]) +
            _arr("Halo46", [halo_angles(n_ice(l))[1] for l in WAVES]) + _arr("NIce", [n_ice(l) for l in WAVES]) +
            "const int NW = %d;\n" % len(WAVES))

LIB = r"""
struct FRareLib
{
    float H1(float n) { return frac(sin(n * 12.9898 + 4.1414) * 43758.5453); }
    float H2(float2 p) { return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
    float N1(float x)
    {
        float i = floor(x), f = frac(x);
        float u = f * f * (3.0 - 2.0 * f);
        return lerp(H1(i), H1(i + 1.0), u);
    }
    float N2(float2 x)
    {
        float2 i = floor(x), f = frac(x);
        float2 u = f * f * (3.0 - 2.0 * f);
        return lerp(lerp(H2(i), H2(i + float2(1, 0)), u.x), lerp(H2(i + float2(0, 1)), H2(i + float2(1, 1)), u.x), u.y);
    }
    float F1(float x)
    {
        return 0.5333 * N1(x) + 0.2667 * N1(x * 2.03 + 7.1) + 0.1333 * N1(x * 4.01 + 3.3) + 0.0667 * N1(x * 8.1 + 1.7);
    }
    float F2(float2 x)
    {
        return 0.5333 * N2(x) + 0.2667 * N2(x * 2.03 + 7.1) + 0.1333 * N2(x * 4.01 + 3.3) + 0.0667 * N2(x * 8.1 + 1.7);
    }
};
"""

# ---------------------------------------------------------------------------------------------------------------------
# The aurora, on the star dome (behind every cloud). Inputs: V view dir, A = AurA (visible brightness incl. gain,
# edge km, spread km, activity), B = AurB (pulsate, red share, fringe, lower border km), C = AurC (magnetic north,
# world xy; z = the substorm's seed; w = folding scale), T time s, Tr transmittance toward V.
#
# Geometry: a curved Earth (R 6371 km) with the observer on it; distances in km. The aurora lives on field lines that
# dip 69 degrees here, so its features run up the sky along the field: a point h km up stands (h - lower) x cot(69)
# nearer the equator than its foot. The bright curtains are thin luminous sheets: each arc is a sheet whose foot runs
# east-west (magnetic) at a distance from us, folded by slow big waves and quick small curls. A view ray crosses a
# sheet where the foot equation holds (a quadratic on the curved Earth, refined twice for the folds); there its glow is
# the emission profile at that height times the path through a sheet ~1 km thick (bright where it is seen edge-on:
# the folds). Rays: brightness striped along the arc in field-aligned columns that drift and flicker with the activity.
# Behind the curtains a diffuse band (8 marched samples): the green glow, the high red and the pulsating patches.
AURORA_HLSL = LIB + r"""
FRareLib L;
float3 v = normalize(V);
if (A.x <= 0.0 || v.z < -0.03) return 0.0;
const float R = 6371.0;
const float cotI = 0.384;
float3 mn = normalize(float3(C.x, C.y, 0.0));
float3 me = cross(mn, float3(0, 0, 1));
float vz = v.z, vn = dot(v, mn), ve = dot(v, me);
float act = A.w;
float Lw = B.w;
float seed = C.z;
float3 green = float3(0.32, 1.0, 0.36);
float3 red = float3(1.0, 0.07, 0.11);
float3 pink = float3(0.95, 0.16, 0.62);
float3 col = 0.0;
float aq = cotI / (2.0 * R);
float bq = vn + vz * cotI;
float drift = T * (4.0 + 22.0 * act);              // km the rays travel along the arcs
for (int k = 0; k < 4; k++)
{
    float ks = seed * 7.13 + k * 3.71;
    float Nk = A.y + (k == 0 ? 0.0 : A.z * (0.2 + 0.25 * k + 0.2 * L.H1(ks)));
    float wk = k == 0 ? 1.0 : (0.25 + 0.45 * L.H1(ks + 1.0)) * (0.4 + 0.6 * act);
    // two crossings at most (the near and the far side of a sheet that curves over the Earth)
    float c0 = -(Lw * cotI + Nk);
    float disc = bq * bq - 4.0 * aq * c0;
    if (disc < 0.0) continue;
    float sq = sqrt(disc);
    float qq = -0.5 * (bq + (bq >= 0.0 ? sq : -sq));
    float roots[2] = { qq / aq, c0 / qq };
    for (int j = 0; j < 2; j++)
    {
        float t = roots[j];
        if (!(t > 0.0) || t > 4000.0) continue;
        float fold = 0.0, fd = 0.0, e = 0.0;
        // the folds: refine where the ray meets the folded sheet
        for (int it = 0; it < 2; it++)
        {
            e = t * ve;
            float fa = (25.0 + 70.0 * act) * C.w;
            float x1 = e / (260.0 - 90.0 * act) + seed + T * 0.0015 * (1.0 + 4.0 * act);
            float x2 = e / (30.0 - 12.0 * act) + seed * 3.0 + T * 0.01 * (1.0 + 6.0 * act);
            fold = fa * (L.F1(x1) - 0.5) * 2.0 + (6.0 + 22.0 * act) * (L.N1(x2) - 0.5) * 2.0;
            float x1b = (e + 1.0) / (260.0 - 90.0 * act) + seed + T * 0.0015 * (1.0 + 4.0 * act);
            float x2b = (e + 1.0) / (30.0 - 12.0 * act) + seed * 3.0 + T * 0.01 * (1.0 + 6.0 * act);
            fd = fa * (L.F1(x1b) - 0.5) * 2.0 + (6.0 + 22.0 * act) * (L.N1(x2b) - 0.5) * 2.0 - fold;
            float c1 = -(Lw * cotI + Nk + fold);
            float d1 = bq * bq - 4.0 * aq * c1;
            if (d1 < 0.0) break;
            float s1 = sqrt(d1);
            float q1 = -0.5 * (bq + (bq >= 0.0 ? s1 : -s1));
            t = j == 0 ? q1 / aq : c1 / q1;
        }
        if (!(t > 0.0)) continue;
        e = t * ve;
        float h = t * vz + t * t / (2.0 * R);
        float lw = Lw + 5.0 * (L.N1(e / 70.0 + ks) - 0.5) - 3.0 * act * L.N1(e / 9.0 + T * 0.2);
        float z = h - lw;
        if (z < -4.0 || z > 420.0) continue;
        // the arc comes and goes along its length; rays: field-aligned columns striped along it
        float seg = smoothstep(0.22, 0.5, L.F1(e / 480.0 + ks * 1.7 + T * 0.0008));
        float rx = (e + drift * (k % 2 == 0 ? 1.0 : -0.6)) / (2.2 + 1.5 * L.H1(ks + 2.0));
        float ray = L.N1(rx) * 0.6 + L.N1(rx * 2.7 + 5.0) * 0.4;
        float flick = 0.75 + 0.25 * L.N1(e / 6.0 + T * (1.0 + 5.0 * act));
        float rays = lerp(1.0, 0.15 + 1.9 * ray * ray * ray, 0.25 + 0.75 * act) * flick;
        float top = 40.0 + 160.0 * L.N1(rx * 0.7 + 11.0) * (0.5 + act);
        // the sheet's normal (its foot runs along the folds; it leans with the field): bright where seen edge-on
        float3 nrm = normalize(mn - me * fd + float3(0, 0, cotI));
        float path = min(1.0 / max(abs(dot(v, nrm)), 0.04), 18.0);
        float zz = max(z, 0.0);
        float bottom = smoothstep(-2.0, 1.5, z);
        float g = bottom * exp(-zz / (30.0 + 25.0 * (1.0 - act))) * (1.0 - smoothstep(top * 0.7, top, zz) * 0.85);
        float rr = smoothstep(50.0, 130.0, zz) * exp(-max(zz - 150.0, 0.0) / 90.0) * (1.0 - smoothstep(top + 40.0, top + 160.0, zz));
        float fr = smoothstep(-1.5, 0.0, z) * exp(-zz / 2.5);
        float3 c = green * g + red * rr * B.y * 0.12 + pink * fr * B.z * 1.1;
        col += c * path * rays * seg * wk * 0.06;
    }
}
// the diffuse band behind the curtains: green glow, the high red, pulsating patches (8 samples from 95 to 380 km up)
{
    float h0 = 95.0, h1 = 380.0;
    float ta = -R * vz + sqrt(R * R * vz * vz + 2.0 * R * h0 + h0 * h0);
    float tb = -R * vz + sqrt(R * R * vz * vz + 2.0 * R * h1 + h1 * h1);
    float jit = L.H2(v.xy * 1000.0 + frac(T));
    float dt = (tb - ta) / 8.0;
    for (int i = 0; i < 8; i++)
    {
        float t = ta + (i + jit) * dt;
        float h = t * vz + t * t / (2.0 * R);
        float zz = max(h - Lw, 0.0);
        float foot = t * vn + (h - Lw) * cotI;
        float e = t * ve;
        float band = smoothstep(A.y - 120.0, A.y + 20.0, foot) * (1.0 - smoothstep(A.y + A.z * 0.9, A.y + A.z * 1.4, foot));
        float patch = L.F2(float2(foot, e) / 70.0 + seed);
        float cell = L.H2(floor(float2(foot, e) / 45.0) + seed);
        float puls = pow(saturate(0.5 + 0.5 * sin(T * 6.2831853 / (5.0 + 10.0 * cell) + cell * 40.0)), 3.0);
        float pp = smoothstep(0.45, 0.7, patch) * lerp(1.0, puls * 1.6, B.x);
        float g = exp(-zz / 30.0) * smoothstep(-3.0, 3.0, h - Lw) * (0.35 + pp);
        float rr = smoothstep(60.0, 140.0, zz) * exp(-max(zz - 160.0, 0.0) / 120.0) * (0.6 + 0.4 * patch);
        col += (green * g * (0.5 + 0.5 * (1.0 - B.y)) + red * rr * B.y * 0.25) * band * dt * 0.0012;
    }
}
return col * A.x * Tr * smoothstep(-0.03, 0.01, vz);
"""

# Noctilucent clouds, 83 km up, lit by the sun from below the horizon. Inputs: V, S (SunDir), N = NlcA (brightness incl.
# gain, seed, 0, 0), T, Tr. Where the Earth's shadow (plus ~15 km of the ozone's screening) passes above 83 km they go
# out; their blue-white of Earth's sky would be the sunlight's colour through the ozone, here the dwarf's warm light:
# pale gold-pearl. Fine billows and a herringbone of waves inside broad patches, brightest toward the sun's side.
NLC_HLSL = LIB + r"""
FRareLib L;
float3 v = normalize(V);
if (N.x <= 0.0 || v.z < -0.01) return 0.0;
const float R = 6371.0, H = 83.0;
float t = -R * v.z + sqrt(R * R * v.z * v.z + 2.0 * R * H + H * H);
float2 q = v.xy * t;
float3 s = normalize(S.xyz);
float3 upp = normalize(float3(q / R, 1.0));
float dep = max(-asin(clamp(dot(s, upp), -1.0, 1.0)), 0.0);
float hs = R * (1.0 / cos(dep) - 1.0) + 15.0;
float lit = smoothstep(hs - 6.0, hs + 6.0, H);
float2 d1 = float2(0.8, 0.6), d2 = float2(-0.6, 0.8);
float w1 = 0.5 + 0.5 * sin(dot(q, d1) * 0.85 + L.N2(q * 0.02) * 7.0);
float w2 = 0.5 + 0.5 * sin(dot(q, d2) * 0.33 + L.N2(q * 0.05 + 3.0) * 5.0);
float bands = pow(w1, 3.0) * (0.45 + 0.55 * w2);
float patch = smoothstep(0.38, 0.72, L.F2(q * 0.0035 + N.y + T * 0.00002));
float tex = patch * (0.3 + 0.7 * bands);
float3 sh = normalize(float3(s.xy, 1e-4));
float side = saturate(dot(normalize(float3(v.xy, 1e-4)), sh) * 0.5 + 0.5);
float ph = 0.35 + 1.65 * side * side * side;
float3 col = float3(0.95, 0.9, 0.78);
return col * tex * lit * ph * N.x * Tr * smoothstep(-0.01, 0.03, v.z);
"""

# The lightning's flash on the dome: the sky glows toward the strike (seen between the clouds), a little everywhere.
# Inputs: V, F = Flash (world dir to the strike, intensity).
FLASH_HLSL = r"""
float3 v = normalize(V);
if (F.w <= 0.0) return 0.0;
float a = acos(clamp(dot(v, normalize(F.xyz)), -1.0, 1.0));
return float3(0.78, 0.84, 1.0) * F.w * (0.03 + exp(-a * a / 0.04) + 0.15 * exp(-a * a / 0.5)) * smoothstep(-0.05, 0.02, v.z);
"""

# A flare on the red dwarf: two white-hot ribbons and a kernel on its face, spreading apart as it fades. The same disk
# frame as the sun (sky_hlsl.SUN_HLSL). Inputs: V, S (SunDir), P (Pole), D (SunDisk: radius, luminance, day seed,
# spin), F = Flare (lat, lon from the face turned to us at noon, strength now, peak), Spots (x = the noon spin), Tr.
FLARE_HLSL = r"""
float3 v = normalize(V);
if (F.z <= 0.0 || D.y <= 0.0) return 0.0;
float3 s = normalize(S.xyz);
float c = dot(v, s);
float3 nn = normalize(P.xyz - s * dot(P.xyz, s));
float3 ee = cross(s, nn);
float3 w = v - s * c;
float2 u = float2(dot(w, ee), dot(w, nn)) / sin(D.x);
float r = c > 0.0 ? length(u) : 100.0;
if (r > 1.6) return 0.0;
float mu = sqrt(saturate(1.0 - r * r));
float3 p = float3(u.x, u.y, mu);
float sp = D.w;
float3 q = float3(p.x * cos(sp) + p.z * sin(sp), p.y, -p.x * sin(sp) + p.z * cos(sp));
float lon = Spots.x + F.y;
float3 fc = float3(cos(F.x) * sin(lon), sin(F.x), cos(F.x) * cos(lon));
float3 t1 = normalize(cross(float3(0, 1, 0), fc)), t2 = cross(fc, t1);
float3 dq = q - fc;
float2 x = float2(dot(dq, t1), dot(dq, t2));
float fade = saturate(F.z / max(F.w, 1e-3));
float sep = 0.015 + 0.04 * (1.0 - fade);
float len = 0.07 + 0.06 * F.w;
float along = exp(-x.x * x.x / (len * len));
float bend = 0.01 * sin(x.x / len * 2.5);
float rib = exp(-pow((x.y - sep - bend) / 0.006, 2.0)) + exp(-pow((x.y + sep + bend * 0.7) / 0.006, 2.0));
float kern = exp(-dot(x, x) / (0.014 * 0.014)) * fade * fade;
float glow = exp(-dot(x, x) / (0.08 * 0.08)) * 0.2;
float face = smoothstep(0.0, 0.15, dot(fc, float3(sin(sp), 0, cos(sp))));
float onDisk = saturate((1.0 - r) / max(fwidth(r), 1e-4));
float3 hot = float3(1.0, 0.92, 0.95);
return hot * (rib * along * 3.0 + kern * 5.0 + glow) * F.z * face * onDisk * D.y * Tr;
"""

# ---------------------------------------------------------------------------------------------------------------------
# M_LyudSkyOptics: halos and rainbows, on a sphere ~3 km round the camera (in front of the far clouds and rain,
# behind the near land). Inputs: V (camera to pixel), Ld = light (world dir to the sun or Europa, xyz; w = its angular
# radius rad), Lc = light colour and luminance (rgb incl. transmittance and gain), Hs = halo (22 ring, sun dogs, pillar,
# 46 ring), Rb = rainbow (strength, rain share, seed, 0).
OPTICS_HLSL = SPECTRAL + LIB + r"""
FRareLib Lb;
float3 v = normalize(V);
float3 l = normalize(Ld.xyz);
float a = degrees(Ld.w);                 // the light's angular radius, deg
float3 col = 0.0;
// the light's limb-darkened disk as a radial kernel: 7 taps across it
const int NT = 7;
float tx[7], tw[7];
float ws = 0.0;
// each tap's pattern is blurred by the taps' spacing, so the sum is smooth (no stripes)
float sp = a / 3.5;
float sp2 = sp * sp;
for (int i = 0; i < NT; i++)
{
    float x = (i - 3) / 3.5;
    tx[i] = x * a;
    float m = sqrt(saturate(1.0 - x * x));
    tw[i] = m * (0.15 + 0.85 * pow(m, 0.85));
    ws += tw[i];
}
float th = degrees(acos(clamp(dot(v, l), -1.0, 1.0)));        // from the light
float ta = 180.0 - th;                                         // from the antisolar point
if (Rb.x > 0.0)
{
    float3 bow = 0.0;
    for (int k = 0; k < NW; k++)
    {
        float p1 = 0.0, p2 = 0.0;
        for (int i = 0; i < NT; i++)
        {
            float d1 = Bow1[k] - (ta + tx[i]);                   // inside the primary > 0
            p1 += tw[i] * (d1 < 0.0 ? exp(-d1 * d1 / (0.12 + sp2)) : (0.2 + 0.8 / sqrt(1.0 + d1 / (0.35 + sp))) * (1.0 - smoothstep(10.0, 40.0, d1)));
            float d2 = (ta + tx[i]) - Bow2[k];                   // outside the secondary > 0
            p2 += tw[i] * (d2 < 0.0 ? exp(-d2 * d2 / (0.16 + sp2)) : (0.15 + 0.85 / sqrt(1.0 + d2 / (0.4 + sp))) * (1.0 - smoothstep(8.0, 30.0, d2)));
        }
        bow += SpecW[k] * (p1 + 0.42 * p2);
    }
    bow /= ws;
    // where the rain is: the bow breaks into pieces along its arc
    float3 an = -l;
    float3 bx = normalize(cross(float3(0, 0, 1), an));
    float3 by = cross(an, bx);
    float ang = atan2(dot(v, by), dot(v, bx));
    float rain = smoothstep(0.25, 0.6, Lb.F1(ang * 2.2 + Rb.z) * 0.6 + Rb.y * 0.6);
    col += max(bow, 0.0) * Rb.x * rain * 0.35;
}
if (Hs.x > 0.0 || Hs.y > 0.0 || Hs.z > 0.0)
{
    float3 ring = 0.0, ring46 = 0.0;
    for (int k = 0; k < NW; k++)
    {
        float p = 0.0, p46 = 0.0;
        for (int i = 0; i < NT; i++)
        {
            float e = (th + tx[i]) - Halo22[k];
            p += tw[i] * smoothstep(-0.4 - sp, 0.15 + sp, e) * (0.7 * exp(-max(e, 0.0) / (1.4 + sp)) + 0.3 * exp(-max(e, 0.0) / 7.0));
            float e46 = (th + tx[i]) - Halo46[k];
            p46 += tw[i] * smoothstep(-0.5 - sp, 0.2 + sp, e46) * exp(-max(e46, 0.0) / 3.0);
        }
        ring += SpecW[k] * p;
        ring46 += SpecW[k] * p46;
    }
    col += max(ring, 0.0) / ws * Hs.x * 0.25 + max(ring46, 0.0) / ws * Hs.w * 0.08;
    // sun dogs: at the light's elevation, D' (the oblique minimum deviation of plate crystals) to either side;
    // red toward the light, a long white tail outward; the disk blurs them into broad patches
    float hl = asin(clamp(l.z, -1.0, 1.0));
    float hv = asin(clamp(v.z, -1.0, 1.0));
    float2 lh = normalize(l.xy + 1e-6), vh = normalize(v.xy + 1e-6);
    float daz = degrees(acos(clamp(dot(lh, vh), -1.0, 1.0)));
    float del = degrees(hv - hl);
    if (Hs.y > 0.0)
    {
        float3 dogs = 0.0;
        float sv = sqrt(0.36 + a * a * 0.25);
        float vert = exp(-del * del / (2.0 * sv * sv));
        float ch = cos(hl), sh2 = sin(hl) * sin(hl);
        for (int k = 0; k < NW; k++)
        {
            float np = sqrt(max(NIce[k] * NIce[k] - sh2, 1.0)) / max(ch, 0.2);
            float Dp = degrees(2.0 * asin(min(np * 0.5, 1.0))) - 60.0;
            float p = 0.0;
            for (int i = 0; i < NT; i++)
            {
                float e = (daz * ch + tx[i]) - Dp * ch;
                p += tw[i] * smoothstep(-0.4 - sp, 0.15 + sp, e) * (0.75 * exp(-max(e, 0.0) / (1.0 + sp)) + 0.25 * exp(-max(e, 0.0) / 9.0));
            }
            dogs += SpecW[k] * p;
        }
        col += max(dogs, 0.0) / ws * vert * Hs.y * 0.9;
    }
    if (Hs.z > 0.0)
    {
        // the pillar: plates tilting a little reflect the light up into a column as wide as the disk
        float xw = daz * cos(hv);
        float col1 = exp(-xw * xw / (2.0 * pow(a * 0.55, 2.0)));
        float up = del > 0.0 ? exp(-del / 9.0) : exp(del / 3.0) * 0.6;
        col += float3(1.0, 0.62, 0.36) * col1 * up * Hs.z * 0.5;
    }
}
return col * Lc.rgb * smoothstep(-0.06, 0.0, v.z);
"""

# M_LyudLightning: a cloud-to-ground bolt, worked out per pixel from its seed, on a sphere just short of the strike
# (nearer land hides it, the clouds in front fog it). Inputs: V, Cam (camera position, world cm), Bt = (the strike's
# ground point world cm, seed), Bb = (the cloud base's height over it cm, brightness, branches 0..1, the leader's
# progress 0..1: the stepped leader creeps down before the return stroke lights the whole channel).
BOLT_HLSL = LIB + r"""
FRareLib L;
if (Bb.y <= 0.0) return 0.0;
float3 v = normalize(V);
float3 o = Cam.xyz;
float3 g = Bt.xyz;
float seed = Bt.w;
float hb = Bb.x;
float3 top = g + float3((L.H1(seed + 1.0) - 0.5) * hb * 0.5, (L.H1(seed + 2.0) - 0.5) * hb * 0.5, hb);
float pix = max(length(fwidth(v)), 1e-5);
const int NS = 22;
float best = 0.0;
// the main channel: a Brownian bridge from the cloud to the ground point, zigzags of ~5% of the height
float3 wn = 0.0;
for (int i = 1; i <= NS; i++)
    wn += float3(L.H1(seed * 3.1 + i * 1.37) - 0.5, L.H1(seed * 5.3 + i * 2.11) - 0.5, 0.0);
float3 prev = top, acc = 0.0;
float glow = 0.0;
for (int i = 1; i <= NS; i++)
{
    float f = i / (float)NS;
    acc += float3(L.H1(seed * 3.1 + i * 1.37) - 0.5, L.H1(seed * 5.3 + i * 2.11) - 0.5, 0.0);
    float3 cur = lerp(top, g, f) + (acc - wn * f) * hb * 0.09;
    if (f > Bb.w + 0.02) break;
    // the closest approach of the view ray to the segment prev..cur
    float3 d = cur - prev;
    float3 w0 = o - prev;
    float aa = dot(v, v), bb = dot(v, d), cc = dot(d, d), dd = dot(v, w0), ee = dot(d, w0);
    float den = max(aa * cc - bb * bb, 1e-6);
    float sc = max((bb * ee - cc * dd) / den, 0.0);
    float tc = saturate((aa * ee - bb * dd) / den);
    float3 pr = o + v * sc, ps = prev + d * tc;
    float dist = length(pr - ps);
    float wd = max(60.0, sc * pix * 1.3);
    best = max(best, exp(-dist * dist / (wd * wd)));
    glow += exp(-dist / (wd * 10.0 + 3000.0)) * 0.025;
    // branches: forks that leave the channel downward and peter out
    if (Bb.z > 0.0 && i < NS * 0.7 && L.H1(seed * 7.7 + i) < 0.22 * Bb.z)
    {
        float3 bp = cur, bacc = 0.0;
        float3 bdir = normalize(float3(L.H1(seed + i * 9.1) - 0.5, L.H1(seed + i * 4.3) - 0.5, -0.9));
        for (int j = 1; j <= 6; j++)
        {
            bacc += float3(L.H1(seed * 1.9 + i * 13.0 + j) - 0.5, L.H1(seed * 2.3 + i * 17.0 + j) - 0.5, L.H1(seed * 2.9 + j + i) - 0.5);
            float3 bc = cur + bdir * hb * 0.045 * j + bacc * hb * 0.02;
            float3 bd = bc - bp;
            float3 bw = o - bp;
            float b2 = dot(v, bd), c2 = dot(bd, bd), d2 = dot(v, bw), e2 = dot(bd, bw);
            float dn2 = max(c2 - b2 * b2, 1e-6);
            float s2 = max((b2 * e2 - c2 * d2) / dn2, 0.0);
            float t2 = saturate((e2 - b2 * d2) / dn2);
            float dist2 = length(o + v * s2 - (bp + bd * t2));
            float wd2 = max(40.0, s2 * pix * 1.1);
            best = max(best, exp(-dist2 * dist2 / (wd2 * wd2)) * (0.55 - 0.07 * j));
            bp = bc;
        }
    }
    prev = cur;
}
return float3(0.82, 0.86, 1.0) * (best * 6.0 + min(glow, 0.6)) * Bb.y;
"""
