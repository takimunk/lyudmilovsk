"""HLSL for the sky dome's material (unreal/scripts/import_sky.py builds M_LyudSky from it).

The red dwarf and Europa are drawn here, per pixel of the dome, not by the atmosphere's light disks
(ALyudSky turns those off). Parameters (ALyudSky::UpdateSkyMaterial sets them):
  SunDir, MoonDir, Pole = world unit vectors toward the sun, the moon, the celestial north pole
  SunDisk  = (angular radius rad, disk-centre luminance, day number, the star's spin rad)
  SunSpots = (the spin at the day's noon rad: the face the rare sky's flares are put on, umbra brightness,
              granulation contrast, faculae strength)
  SunLook  = (aureole strength, how much of the air's dimming the disk keeps 0..1, prominence gain, corona gain)
  SunRegion0..7 = the active regions (ULyudSunActivity): latitude, star longitude (rad), strength 0..1, seed
  SunPromA/B/C0..15 = the prominences (view frame: x east, y north, z toward us, unit star): anchor xyz + height
              (radii); foot axis xyz + half length (rad); brightness, kind (0 loop, 1 hedgerow, 2 surge), lift (radii), seed
  SunFlare = the activity's flare in the rare sky's format (rare_hlsl.FLARE_HLSL draws it)
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
    // member k (0 leader, 1..4 followers and pores) of an active region's spot group (ULyudSunActivity:
    // g = latitude, star longitude, strength 0..1, seed): (centre on the unit star in the star's frame, umbra radius
    // rad; 0 = none). A stronger region has a bigger leader and more followers trailing it in longitude.
    float4 RegionSpot(float4 g, int k)
    {
        float sk = g.w * 1.31 + k * 1.618;
        bool on = g.z > 0.0 && k < 1.0 + 4.0 * g.z && (k == 0 || H1(sk + 8.0) < 0.85);
        float lat = g.x + (H1(sk + 2.0) - 0.5) * 0.03 * min(k, 1);
        float lon = g.y - k * (0.015 + 0.025 * H1(sk + 4.0)) * (0.6 + 0.6 * g.z) - (k > 0 ? 0.02 : 0.0);
        float r = k == 0 ? 0.004 + 0.014 * g.z : 0.002 + 0.004 * H1(sk + 6.0) * g.z;
        return float4(cos(lat) * sin(lon), sin(lat), cos(lat) * cos(lon), on ? r : 0.0);
    }
    // a point of a prominence (view frame, unit star): s = -1..1 along its foot, A = the foot's middle, Tt = the
    // foot's axis, w = half its length (rad), h = height at the top (radii), lift = how far it has risen, sw = its
    // sideways sway (radii) along Bn
    float3 PromPt(float3 A, float3 Tt, float3 Bn, float w, float h, float lift, float s, float sw)
    {
        float a = w * s;
        float sh = sqrt(saturate(1.0 - s * s));
        return (A * cos(a) + Tt * sin(a)) * (1.0 + h * sh) + Bn * (sw * sh) + A * lift;
    }
};
"""

# The red dwarf: (luminance rgb) at view direction V: the disk (limb darkening, granulation, the active regions' spots
# and faculae), the corona with streamers over the active regions, the prominences on the limb (dark filaments where
# they lie in front of the disk) and the warm aureole round it all in the haze.
# Inputs: V, S (SunDir), P (Pole), D (SunDisk), Spots (SunSpots), T (time s), Tr (atmosphere transmittance rgb toward
# V), Look (SunLook: aureole, horizon dimming kept 0..1, prominence gain, corona gain), Look2 (SunLook2: the disk's
# detail contrast, 0, 0, 0), R0..R7 (SunRegion*: latitude,
# star longitude, strength, seed), PA/PB/PC0..15 (SunPromA/B/C*: anchor xyz + height; foot axis xyz + half length;
# brightness, kind 0 loop 1 hedgerow 2 surge, lift, seed), all from ULyudSunActivity.
SUN_REGIONS = 8
SUN_PROMS = 16
SUN_HLSL = r"""
FLyudSkyLib L;
float4 RG[8] = { R0, R1, R2, R3, R4, R5, R6, R7 };
float4 PA[16] = { PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7, PA8, PA9, PA10, PA11, PA12, PA13, PA14, PA15 };
float4 PB[16] = { PB0, PB1, PB2, PB3, PB4, PB5, PB6, PB7, PB8, PB9, PB10, PB11, PB12, PB13, PB14, PB15 };
float4 PC[16] = { PC0, PC1, PC2, PC3, PC4, PC5, PC6, PC7, PC8, PC9, PC10, PC11, PC12, PC13, PC14, PC15 };

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
if (D.y <= 0.0) return 0.0;
// the air toward the star's centre, without the sinking below the horizon: the disk keeps Look.y of its dimming (the
// eye adapts to a low sun, and a real one stays blinding); the colour still follows the air, pixel by pixel
float salt = degrees(asin(clamp(s.z, -1.0, 1.0)));
float sh0 = max(salt, 0.0);
float sam = 1.0 / (sin(radians(sh0)) + 0.50572 * pow(sh0 + 6.07995, -1.6364));
float3 trs = exp(-float3(0.07, 0.14, 0.30) * (sam - 1.0));
float trsY = max(dot(trs, float3(0.2126, 0.7152, 0.0722)), 1e-3);
float3 TrB = Tr * lerp(1.0, min(pow(trsY, Look.y - 1.0), 30.0), smoothstep(30.0, 3.0, salt));
// the aureole: the haze's forward glow round the star, warm, wider and stronger the more air the light crosses
float3 aur = 0.0;
if (r > 0.98 && r < 14.0)
{
    float x = max(r - 1.0, 0.0);
    float haze = 0.25 + 1.6 * (1.0 - trsY);
    float lobe = 0.03 * exp(-x / 0.7) + 0.012 / (1.0 + x * x / 9.0) + 0.0035 / (1.0 + x * x / 60.0);
    aur = float3(1.0, 0.5, 0.26) * (Look.x * haze * lobe) * smoothstep(0.98, 1.02, r);
}
if (r > 4.5) return aur * D.y * TrB;
// a ~3000 K star, deep red-orange, darker and redder toward the limb (deeper than the light: the tonemapper pales it)
const float3 Hot = float3(1.0, 0.31, 0.075);
const float3 Limb = float3(0.55, 0.07, 0.015);
float sp = D.w;
float csp = cos(sp), ssp = sin(sp);
float3 col = 0.0;
// prominences: their emission off the limb, their dark filaments where they lie in front of the disk
float prom = 0.0, core = 0.0, filament = 1.0;
float phi0 = atan2(u.y, u.x);
if (r > 0.55 && Look.z > 0.0)
{
    float wdMin = max(fw * 1.2, 0.002);
    for (int i = 0; i < SUN_PROMS; i++)
    {
        float4 pa = PA[i], pb = PB[i], pc = PC[i];
        if (pc.x <= 0.0) continue;
        float3 A = pa.xyz;
        float hh = pa.w, ww = pb.w, lift = pc.z;
        float ext = ww * 1.1 + hh + 0.06;
        float2 d0 = u - A.xy * (1.0 + lift);
        if (dot(d0, d0) > ext * ext) continue;
        float3 Tt = pb.xyz;
        float3 Bn = cross(A, Tt);
        float sd = pc.w;
        int kind = (int)(pc.y + 0.5);
        // the sway: slow, and a quicker ripple along it (the field lines shaken from below)
        float swA = hh * (0.07 * sin(T * 0.21 + sd) + 0.03 * sin(T * 0.53 + sd * 1.7));
        // the arch as a polyline; for a hedgerow, each vertex also stands a thread down to the surface
        float best = 1e9, bs = 0.0, bz = 0.0, br = 1.0, thr = 0.0, thz = 1.0;
        float wdT = max(0.007, wdMin);
        float3 prev = L.PromPt(A, Tt, Bn, ww, hh, lift, -1.0, swA * sin(sd));
        const int NSEG = 10;
        for (int k = 1; k <= NSEG; k++)
        {
            float sk = -1.0 + 2.0 * k / NSEG;
            float ph = swA * sin(sd + sk * 2.3 + T * 0.4);
            float hk = hh * (kind == 1 ? 0.65 + 0.55 * L.H1(sd + k * 3.1) : 1.0);
            float3 cur = L.PromPt(A, Tt, Bn, ww, hk, lift, sk, ph);
            float2 e = cur.xy - prev.xy, f = u - prev.xy;
            float t = saturate(dot(f, e) / max(dot(e, e), 1e-9));
            float2 g = f - e * t;
            float dd = dot(g, g);
            if (dd < best) { best = dd; bs = sk - (2.0 / NSEG) * (1.0 - t); bz = lerp(prev.z, cur.z, t); br = length(lerp(prev.xy, cur.xy, t)); }
            if (kind == 1)
            {
                // a vertical thread from the foot to the arch, swaying with it
                float3 base = L.PromPt(A, Tt, Bn, ww, 0.0, lift, sk - 1.0 / NSEG, 0.0);
                float3 top = L.PromPt(A, Tt, Bn, ww, hk, lift, sk - 1.0 / NSEG, ph);
                float2 e2 = top.xy - base.xy, f2 = u - base.xy;
                float t2 = saturate(dot(f2, e2) / max(dot(e2, e2), 1e-9));
                float2 g2 = f2 - e2 * t2;
                float q = exp(-dot(g2, g2) / (wdT * wdT)) * (0.35 + 0.65 * t2) * (wdT > 0.007 ? 0.007 / wdT : 1.0);
                if (q > thr) { thr = q; thz = lerp(base.z, top.z, t2); }
            }
            prev = cur;
        }
        float dist = sqrt(best);
        float wd0 = (kind == 1 ? 0.028 : (kind == 2 ? 0.014 : 0.02)) * (1.0 + 0.8 * lift);
        float wd = max(wd0, wdMin);
        // under the arch: the body of the prominence, a faint fibrous sheet down to the surface
        float inside = r > 1.0 ? smoothstep(0.0, 0.04, br - r) * smoothstep(1.0, 0.85, abs(bs)) * smoothstep(1.0, 1.015, r) : 0.0;
        if (dist > wd * 8.0 && thr < 0.01 && inside <= 0.0) continue;
        // fine structure: strands across, knots of plasma draining down the legs, flickering threads
        // strands along it: noise stretched along the arch, fine across it
        float n1 = L.Noise(float3(bs * 7.0, dist / wd * 3.5, sd)) * 0.6 + L.Noise(float3(bs * 23.0, dist / wd * 7.0, sd + 5.0)) * 0.4;
        float flow = L.Noise(float3(bs * 9.0 - T * 0.12 * sign(bs + 1e-4), sd * 0.37, 0.0));
        float band = exp(-dist * dist / (wd * wd)) * (wd0 / wd);
        // brighter toward the top of the legs, fading into the feet
        float legs = 0.55 + 0.45 * smoothstep(1.0, 0.4, abs(bs));
        float e1 = band * (0.25 + 1.2 * n1 * n1) * (0.5 + 0.8 * flow) * legs;
        float glow = exp(-dist / (wd * 1.5)) * 0.25;
        float thread = thr * (0.5 + 0.8 * L.Noise(float3(bs * 25.0, T * 0.05 + sd, 3.0)));
        float fib = L.Noise(float3(phi0 * 160.0 + sd, r * 25.0 - T * 0.02, sd));
        float body = inside * (kind == 1 ? 0.4 : 0.3) * (0.3 + 1.0 * fib * fib);
        float em = (kind == 1 ? 0.5 * e1 + 0.9 * thread : e1) + glow + body;
        // an erupting one leaves a faint expanding front ahead of it
        em += lift > 0.2 ? 0.05 * exp(-abs(dist - 0.25 * lift) / (0.03 + 0.02 * lift)) : 0.0;
        float zAt = kind == 1 && thr > band ? thz : bz;
        if (r < 1.0)
        {
            // in front of the disk: a dark filament (cool gas against the hot photosphere); behind it: nothing
            if (zAt > 0.0 && kind == 1) filament *= 1.0 - saturate(0.06 * exp(-dist / 0.035) * (0.6 + 0.6 * n1) * pc.x);
        }
        else
        {
            prom += em * pc.x;
            core += band * pc.x;
        }
    }
}
if (r < 1.0)
{
    float mu = sqrt(1.0 - r * r);
    // a point on the star in its own frame (y = its north, z = toward us), turned with its spin
    float3 p = float3(u.x, u.y, mu);
    float3 q = float3(p.x * csp + p.z * ssp, p.y, -p.x * ssp + p.z * csp);
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
    // the active regions' spots: small and subtle (Dima: barely visible dark points, a little darker than the
    // photosphere): pores and small spots with a slight, soft penumbra, a ragged edge; bright plage round them
    float spot = 1.0, fac = 0.0, plage = 0.0;
    for (int gi = 0; gi < SUN_REGIONS; gi++)
    {
        float4 rg = RG[gi];
        if (rg.z <= 0.0) continue;
        float3 rc = float3(cos(rg.x) * sin(rg.y), sin(rg.x), cos(rg.x) * cos(rg.y));
        float cr = dot(q, rc);
        if (cr < 0.95) continue;
        plage = max(plage, rg.z * exp(-(1.0 - cr) / (0.0025 * (0.5 + rg.z))));
        for (int k = 0; k < 5; k++)
        {
            float4 kk = L.RegionSpot(rg, k);
            if (kk.w <= 0.0) continue;
            float ru = kk.w;
            float3 dq = q - kk.xyz;
            if (dot(dq, dq) > ru * ru * 36.0) continue;
            float hs = L.H1(rg.w * 1.7 + gi * 5.3 + k * 2.9);
            float3 t1 = normalize(cross(float3(0, 1, 0), kk.xyz)), t2 = cross(kk.xyz, t1);
            float2 rq = float2(dot(dq, t1), dot(dq, t2));
            float el = 1.0 + 0.5 * frac(hs * 7.31);
            rq *= float2(1.0 / sqrt(el), sqrt(el));
            float d = length(rq);
            float2 cs = rq / max(d, 1e-6);
            float3 sdd = float3(hs * 37.0, k * 11.0, gi * 7.0);
            float ue = ru * (0.8 + 0.4 * L.Noise(float3(cs * 2.2, 0.0) + sdd));
            bool pore = k > 0 || ru < 0.006;
            float pe = pore ? ue * 1.6 : ue * (1.9 + 0.4 * L.Noise(float3(cs * 1.8, 2.0) + sdd));
            float fil = 0.9 + 0.1 * L.Noise(float3(cs * 16.0, d / ru * 0.6) + sdd);
            float x = saturate((d - ue) / max(pe - ue, 1e-5));
            float pen = 1.0 - smoothstep(0.0, 1.0, x);
            float umb = 1.0 - smoothstep(0.7, 1.0, d / ue);
            float dark = lerp(lerp(1.0, 0.86 * fil, pen * (pore ? 0.5 : 1.0)), Spots.y, umb);
            spot *= dark;
            fac = max(fac, (1.0 - smoothstep(pe, pe * 4.0, d)) * (1.0 - pen));
        }
    }
    // faculae: bright patches round the groups and a network near the limb, where they show best
    float facNoise = smoothstep(0.42, 0.75, L.Fbm(q * 20.0 + 5.0));
    float net = smoothstep(0.55, 0.85, L.Fbm(q * 13.0 + 11.0)) * 0.6;
    float faculae = 1.0 + Spots.w * ((fac + plage) * facNoise + net) * pow(1.0 - mu, 0.6);
    // the disk is drawn bright (the tonemapper's shoulder squeezes it): its detail is steepened by Look2.x so the
    // granulation, the spots and the faculae still read
    col = base * pow(max(gran * spot * filament, 1e-3), Look2.x) * faculae;
    // a soft edge, a pixel or so wide
    col *= saturate((1.0 - r) / fw);
}
// the corona: a soft orange glow off the limb, brighter and reaching farther over the active regions near the limb
// (helmet streamers)
float h = max(r - 1.0, 0.0);
float phi = atan2(u.y, u.x);
float streamer = 0.0;
for (int gj = 0; gj < SUN_REGIONS; gj++)
{
    float4 rg = RG[gj];
    if (rg.z <= 0.0) continue;
    float3 rq = float3(cos(rg.x) * sin(rg.y), sin(rg.x), cos(rg.x) * cos(rg.y));
    float3 rp = float3(rq.x * csp - rq.z * ssp, rq.y, rq.x * ssp + rq.z * csp);
    float dphi = atan2(sin(phi - atan2(rp.y, rp.x)), cos(phi - atan2(rp.y, rp.x)));
    streamer += rg.z * exp(-rp.z * rp.z / 0.12) * exp(-dphi * dphi / (0.09 * (1.0 + 2.0 * h)));
}
float corona = (0.02 * exp(-h * 10.0) + 0.005 * exp(-h * 2.5)) * (1.0 + 1.5 * streamer) + 0.009 * streamer * exp(-h * 1.3);
// the chromosphere: a thin ragged red rim of spicules just off the limb
float chrom = exp(-h / 0.006) * (0.5 + 0.7 * L.Noise(float3(phi * 260.0, h * 300.0, T * 0.1))) * smoothstep(0.0, 0.001, h);
float off = smoothstep(0.985, 1.0, r);
float3 glow = (float3(1.0, 0.36, 0.12) * corona * Look.w + (float3(1.0, 0.17, 0.08) * (prom + 0.35 * chrom) + float3(1.0, 0.3, 0.15) * core * 0.15) * 0.35 * Look.z) * off;
return (col + glow + aur) * D.y * TrB;
"""

SUN_HLSL = SUN_HLSL.replace("SUN_PROMS", str(SUN_PROMS)).replace("SUN_REGIONS", str(SUN_REGIONS))

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

# Europa in front of the sun: 0 inside the moon's disk, 1 outside (a pixel-wide soft edge). At a new moon the two big
# disks overlap by day (14 + 10 degrees across, a few degrees apart): the sun, its limb and corona go behind the dark
# moon instead of drawing through it. Inputs: V, M (MoonDir), MD (MoonDisk).
MOON_COVER_HLSL = r"""
float a = acos(clamp(dot(normalize(V), normalize(M.xyz)), -1.0, 1.0));
float fw = max(fwidth(a), 1e-5);
return MD.y > 0.0 ? saturate((a - MD.x) / fw + 0.5) : 1.0;
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
