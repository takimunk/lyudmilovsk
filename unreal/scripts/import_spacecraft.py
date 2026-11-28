"""The descent craft «Ласточка» into the project (docs/spacecraft.md): textures/spacecraft/*.png
(tools/spacecraft/textures.py, island_map.py, labels.py, sleeves.py) -> /Game/Spacecraft/Textures (the localized ones
-> /Game/Spacecraft/Localized/<lang>/), the materials -> /Game/Spacecraft/Materials, and the kit
design/spacecraft/game/spacecraft_kit.glb (blender/spacecraft/spacecraft_kit.py) -> /Game/Spacecraft/Meshes/SM_SC_*,
its material slots filled by name. Run headless (unreal/scripts/import_spacecraft.sh):

  UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/import_spacecraft.py

Every material is built here from HLSL custom nodes (tools/spacecraft/MATERIAL_NOTES.md has each parameter: units, who
sets it). The HLSL is written in a small branch-free subset (declarations with a value, assignments, [unroll] loops with
constant bounds, one return at the end; no if, no ?:, no comparisons: step() and lerp() instead), for two reasons: it
is what a phone's shader compiler likes, and tools/spacecraft/material_preview.py can then run the very same text in
numpy (the previews in design/spacecraft/preview/ are these shaders, not a repaint of them).

Outside Unreal, `python3 unreal/scripts/import_spacecraft.py` lints every node (balanced brackets, every input used,
every name declared, the subset kept, the constants that mirror other tools still equal to them) and, when numpy is
there, executes each one once (material_preview.smoke).

IMPORT_SPACECRAFT_ONLY=materials rebuilds the textures and materials only and puts them back on the imported meshes.
Nanite off on every mesh (small, moving, seen from inside); simple collision as authored.
"""
import json
import math
import os
import re
import sys

try:
    import unreal
    if not hasattr(unreal, "EditorAssetLibrary"):      # the repo's own unreal/ folder, seen from its root
        unreal = None
except ImportError:          # outside the editor: the lint and the previews only
    unreal = None

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
KIT = os.path.join(REPO, "design", "spacecraft", "game", "spacecraft_kit.glb")
KIT_JSON = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "Spacecraft", "kit.json")
TEX_SRC = os.path.join(REPO, "textures", "spacecraft")
PINS = os.path.join(REPO, "tools", "spacecraft", "textures.json")
RECORDS_SRC = os.path.join(REPO, "audio", "music", "spacecraft")
ROOT = "/Game/Spacecraft"
MESHES = ROOT + "/Meshes"
TEX = ROOT + "/Textures"
MAT = ROOT + "/Materials"
LOCAL = ROOT + "/Localized"
EU_TEX = "/Game/Europa/Textures"
LANGS = ("ru", "en", "ja")
# one tile of each tx_sc_* surface in metres (the kit's UVs are metres; Content/Spacecraft/kit.json "tiles" in cm wins
# when it is there), its metalness (tools/spacecraft/textures.py TILE_M / METAL: the lint compares them with
# textures.json)
TILE_M = {"paint_white": 2.0, "paint_green": 2.0, "metal": 1.0, "foil": 1.0, "quilt": 0.57923, "shield": 1.0,
          "rubber": 0.5, "fabric": 0.5, "net": 0.25}
METAL = {"metal": 0.9, "foil": 1.0}
# surfaces with no normal map of their own, cut out by their colour's alpha
CUTOUT = ("net",)
# the canopies: the cloth's tile is two gores round (an orange and a white) -> TileU = gores / 2 (kit.json "chute")
CHUTES = {"MI_SC_chute": ("main", 24, 3.0), "MI_SC_chute_drogue": ("drogue", 8, 1.0)}
# the instrument layout the shaders' defaults mirror (kit.json "panel"; the lint compares)
DIGIT_COUNT = 17
DIGIT_WINDOWS = (("clock", 0, 6, 2, 0), ("range", 6, 6, 0, 0), ("speed", 12, 5, 0, 0))     # id, first, digits, colon every, point after
DIGIT_ASPECT = 1.7 / 2.8
LAMP_GRID = (6, 4)
RECORD_R, LABEL_R = 12.5, 4.5
# the CRT's drawn path (FlightModel.h LyudFlight::CrtPath and the Crt* instruments; the lint reads that header)
CRT_PATH = dict(ox=0.045, oy=0.02, r0=0.30, r1=0.165, a0=205.0, a1=20.0, bend=600.0 / 900.0, bump=0.05)
CRT_BODIES = ((0.14, 0.26, 0.05), (0.68, 0.56, 0.16))
CRT_ASPECT = 20.8 / 15.8
FLIGHT_MODEL = os.path.join(REPO, "unreal", "Lyudmilovsk", "Source", "Lyudmilovsk", "Private", "Spacecraft", "FlightModel.h")
# the analytic sphere of the planet inside its 128 x 64 mesh (whose flat faces lie up to 0.06 % inside the true sphere)
PLANET_FIT = 0.9985
# where the planet texture's own island is (tools/europa/europa_textures.py ISLAND_LAT / ISLAND_LON / ISLAND_SIZE), as
# a unit vector in the texture's frame (x to longitude 0, z north); the lint recomputes them
EU_ISLAND = (54.96, 20.40, 9.0)
if unreal:
    eal = unreal.EditorAssetLibrary
    mel = unreal.MaterialEditingLibrary
    X = unreal


def log(msg):
    if unreal:
        unreal.log_warning("SPACECRAFT " + msg)
    else:
        print("SPACECRAFT " + msg)


# =====================================================================================================================
# HLSL. Inputs are named in each material's spec below; a texture input T comes with its sampler TSampler.

# --- the planet -------------------------------------------------------------------------------------------------------

PLANET_HIT_HLSL = r"""
// the exact sphere inside the mesh: where this pixel's ray meets it. Returns its outward normal (world), or 0 when the
// ray passes outside (the mesh's corners): the opacity mask drops those pixels and the air behind shows
float R = ObjR * PLANET_FIT;
float3 o = (Cam - C) / R;
float3 v = normalize(P - Cam);
float b = dot(o, v);
float3 cr = cross(o, v);
float disc = 1.0 - dot(cr, cr);
float t = -b - sqrt(max(disc, 0.0));
float3 n = normalize(o + v * t);
return n * step(0.0, disc) * step(0.0, t);
""".replace("PLANET_FIT", "%.4f" % PLANET_FIT)

PLANET_MASK_HLSL = r"""
return step(0.5, dot(Nw, Nw));
"""

PLANET_HLSL = r"""
// Nw: the sphere's normal in the world (0 = no planet here). L, Ls: the same normal and the sun in the mesh's frame
// (Blender's with Y mirrored by the import): x to longitude 0, z north
float hit = step(0.5, dot(Nw, Nw));
float3 nw = normalize(Nw + float3(0.0, 0.0, 1.0e-6));
float3 d = normalize(L + float3(0.0, 0.0, 1.0e-6));
d = float3(d.x, -d.y, d.z);
float3 ls = normalize(Ls + float3(0.0, 0.0, 1.0e-6));
ls = float3(ls.x, -ls.y, ls.z);
float3 l = normalize(SunDir.xyz);
float3 v = normalize(V);
float ndl = dot(nw, l);
float mu = saturate(dot(nw, v));
// the planet textures' own place: equirectangular, longitude 0 at the middle, north at the top
float coslat = sqrt(max(1.0 - d.z * d.z, 1.0e-4));
float2 uv = float2(atan2(d.y, d.x) * 0.15915494 + 0.5, 0.5 - asin(clamp(d.z, -1.0, 1.0)) * 0.31830989);
// how much of the planet one pixel covers (radians): picks the mips, and lets the fine layers in only when they resolve
float foot = clamp(max(length(ddx(d)), length(ddy(d))), 1.0e-7, 0.05);
float lod = clamp(log2(foot * 326.0), 0.0, 8.0);
float3 alb = Albedo.SampleLevel(AlbedoSampler, uv, lod).rgb;
// the texture's own island is a nine-degree blot drawn to be seen from Europa: open sea goes over it, and the real
// island (the baked map) is laid on at IslandLat / IslandLon, IslandSizeKm across on a planet of PlanetRadiusKm
float a0 = acos(clamp(dot(d, float3(EU_IX, EU_IY, EU_IZ)), -1.0, 1.0));
float3 sea0 = lerp(float3(0.0027, 0.0116, 0.0467), float3(0.0086, 0.0732, 0.1195), 0.6 * exp(-a0 * a0 / 0.1194));
alb = lerp(alb, sea0, 1.0 - smoothstep(0.125, 0.155, a0));
float la = radians(IslandLat);
float lo = radians(IslandLon);
float3 iu = float3(cos(la) * cos(lo), cos(la) * sin(lo), sin(la));
float3 ie = float3(-sin(lo), cos(lo), 0.0);
float3 inn = cross(iu, ie);
float cu = dot(d, iu);
float2 ien = float2(dot(d, ie), dot(d, inn));
float sizeRad = IslandSizeKm / PlanetRadiusKm;
float2 q = float2(ien.x, -ien.y) / (max(cu, 0.2) * sizeRad);
float inside = step(abs(q.x), 0.5) * step(abs(q.y), 0.5) * step(0.2, cu);
float ilod = clamp(log2(foot / sizeRad * 1024.0), 0.0, 10.0);
float4 isl = Island.SampleLevel(IslandSampler, q + 0.5, ilod) * inside;
float3 im = IslandM.SampleLevel(IslandMSampler, q + 0.5, ilod).rgb * inside;
// fine layers from one small noise tile, on a conformal map centred on the island (no pole, no seam on the island's
// half of the planet; faded out toward its far side). Each layer's mip follows the pixel, so it comes in by itself
// as the planet nears; Detail is the master
float2 sp = ien / (1.05 + cu);
float2 sps = float2(dot(ls, ie), dot(ls, inn)) * 0.0015;
float dfade = saturate((cu + 0.7) * 2.0) * Detail;
float nl = log2(foot * 128.0);
float near = 1.0 - saturate((log2(foot) + 14.0) / 6.0);
float2 drift = float2(CloudShift * 8.0, 0.0);
// (each layer turned its own way and scaled off the others, so the tile's repeat never lines up into a weave)
float2 sp1 = float2(sp.x * 0.8 + sp.y * 0.6, sp.y * 0.8 - sp.x * 0.6) * 7.0 + drift;
float2 sp2 = float2(sp.x * 0.28 - sp.y * 0.96, sp.y * 0.28 + sp.x * 0.96) * 37.0 + drift * 4.0;
float2 sp3 = float2(sp.x * 0.91 + sp.y * 0.41, sp.y * 0.91 - sp.x * 0.41) * 331.0;
float2 sp4 = float2(sp.x * 0.5 - sp.y * 0.87, sp.y * 0.5 + sp.x * 0.87) * 1870.0;
float n1 = Noise.SampleLevel(NoiseSampler, sp1, nl + 2.8).r;
float n2 = Noise.SampleLevel(NoiseSampler, sp2, nl + 5.2).r;
float n2s = Noise.SampleLevel(NoiseSampler, sp2 + float2(sps.x * 0.28 - sps.y * 0.96, sps.y * 0.28 + sps.x * 0.96) * 37.0, nl + 5.2).r;
float w1 = Noise.SampleLevel(NoiseSampler, sp3, nl + 8.4).b;
float w2 = Noise.SampleLevel(NoiseSampler, sp4 + float2(0.0, T * 0.002), nl + 10.9).g;
// clouds: the planet's cover, drifting; billows grow on it close up. A second look a little toward the sun gives the
// billows their shaded side and the sea its cloud shadows
float3 e3 = float3(-d.y, d.x, 0.0) / coslat;
float3 n3 = cross(d, e3);
float2 duv = float2(dot(ls, e3) / coslat * 0.15915494, -dot(ls, n3) * 0.31830989) * 0.003;
float2 cuv = uv + float2(CloudShift, 0.0);
float c0 = Clouds.SampleLevel(CloudsSampler, cuv, lod).r;
float cs0 = Clouds.SampleLevel(CloudsSampler, cuv + duv, lod).r;
float bil = (n1 - 0.5) * 0.7 + (n2 - 0.5) * 0.6;
float bils = (n1 - 0.5) * 0.7 + (n2s - 0.5) * 0.6;
float c = saturate(c0 + bil * dfade * smoothstep(0.0, 0.3, c0));
float cs = saturate(cs0 + bils * dfade * smoothstep(0.0, 0.3, cs0));
c = lerp(c, smoothstep(0.12, 0.72, c), dfade * near * 0.8);
cs = lerp(cs, smoothstep(0.12, 0.72, cs), dfade * near * 0.8);
float shade = saturate((cs - c) * 2.5);
float3 cloudCol = float3(0.93, 0.94, 0.96) * (1.0 - 0.5 * shade) * (1.0 + 0.12 * (n2 - 0.5) * dfade);
// the sea: wind streaks and slicks close up, the cloud's shadow, the island over it
alb = alb * (1.0 + ((w1 - 0.5) * 0.22 + (n2 - 0.5) * 0.15) * dfade);
float3 surf = lerp(alb, isl.rgb * (1.0 + (w2 - 0.5) * 0.5 * im.g * dfade), isl.a) * (1.0 - 0.5 * cs * (1.0 - c));
float3 sunTint = lerp(float3(1.0, 0.42, 0.16), float3(1.0, 1.0, 1.0), smoothstep(0.0, 0.3, ndl));
float3 col = lerp(surf, cloudCol, c) * SunLight.rgb * saturate(ndl) * sunTint;
// the sun's glint: one lobe on the sphere, narrower the calmer the sea (Rough), broken into glitter by the swell
float3 h = normalize(l + v);
float shin = 2.0 / max(Rough * Rough * Rough * Rough, 1.0e-4);
float ndh = saturate(dot(nw, h) + ((w1 - 0.5) * 0.004 + (w2 - 0.5) * 0.003) * dfade);
float glint = pow(ndh, shin) * (shin / 160.0) * 0.35 * Glint * (1.0 + (w2 - 0.5) * 1.5 * dfade);
col += SunLight.rgb * sunTint * glint * (1.0 - c) * (1.0 - im.g) * step(0.0, ndl);
// the air over the day side (the shell M_SC_PlanetAir draws only the limb beyond the disc): Rayleigh blue, thicker
// toward the limb, orange where the sun is low
float fres = pow(1.0 - mu, 3.0);
float3 hazeCol = lerp(float3(0.12, 0.06, 0.04), float3(0.05, 0.11, 0.26), smoothstep(-0.08, 0.22, ndl));
// (looked at from close and steeply, the same air is a thinner veil over what is resolved under it)
float thick = lerp(1.0, 0.3, near);
col += hazeCol * SunLight.rgb * (saturate(ndl + 0.12) * ((0.6 + 0.8 * (1.0 - mu)) * thick + 1.0 * fres) * Haze);
// the town's lights on the night side, under clear sky
float night = 1.0 - smoothstep(-0.12, 0.04, ndl);
col += float3(1.0, 0.62, 0.3) * im.r * night * (1.0 - c) * Lights;
return col * Gain * hit;
"""

AIR_HLSL = r"""
// the planet's air seen from outside or inside its shell (M_EU_PlanetAir's single scattering in six steps): the
// glowing limb, the twilight ring past the terminator. Nothing where the ray meets the sea (M_SC_PlanetNear draws its
// own haze there). The shell is two-sided: from outside both faces draw the same ray, so each gives half
float Ra = Air.x / PLANET_FIT;
float Rp = ObjR / Air.x * PLANET_FIT;
float3 o = (Cam - C) / Rp;
float3 v = normalize(WP - Cam);
float b = dot(o, v);
float oo = dot(o, o);
float3 cr = cross(o, v);
float p2 = dot(cr, cr);
float disc = Ra * Ra - p2;
float sq = sqrt(max(disc, 0.0));
float t0 = max(-b - sq, 0.0);
float t1 = -b + sq;
float sea = step(p2, 1.0) * step(b, 0.0);
float on = step(0.0, disc) * step(t0, t1) * (1.0 - sea);
float H = (Ra - 1.0) * 0.22;
float3 Lsun = normalize(SunDir.xyz);
float3 beta = float3(0.175, 0.41, 1.0);
float K = 60.0 * Air.y;
float ds = max(t1 - t0, 0.0) / 6.0;
float3 acc = float3(0.0, 0.0, 0.0);
float tauV = 0.0;
[unroll] for (int i = 0; i < 6; i++)
{
    float t = t0 + ds * (float(i) + 0.5);
    float3 p = o + v * t;
    float r = max(length(p), 1.0e-3);
    float rho = exp(-max(r - 1.0, 0.0) / H);
    float m = dot(p, Lsun) / r;
    float z = degrees(acos(clamp(m, -1.0, 1.0)));
    float ch = 1.0 / max(m + 0.15 * pow(max(93.885 - z, 0.01), -1.253), 0.03);
    float edge = -sqrt(max(1.0 - 1.0 / (r * r), 0.0));
    float lit = smoothstep(edge - 0.005, edge + 0.03, m) * smoothstep(-0.25, 0.05, m);
    acc += exp(-beta * (K * H * (tauV + rho * ch))) * (rho * lit * ds);
    tauV += rho * ds / H;
}
float cosT = dot(v, Lsun);
float phaseR = 0.75 * (1.0 + cosT * cosT);
float phaseM = (1.0 - 0.5184) / pow(1.5184 - 1.44 * cosT, 1.5) * 0.06;
float outside = step(Ra * Ra, oo);
return acc * (beta * phaseR + phaseM) * SunLight.rgb * (K * 0.12 * on * (1.0 - 0.5 * outside) * Gain);
""".replace("PLANET_FIT", "%.4f" % PLANET_FIT)

# --- the fire -----------------------------------------------------------------------------------------------------

PLASMA_HLSL = r"""
// the wake seen through a porthole: U round the shell (whole turns: every U scale is an integer, so it closes), V
// along the flow. A glowing sheath in lanes, long streamers tearing past, fine filaments, sparks; colour by Heat,
// strength by Plasma. One 256 noise tile, seven lookups
float t = T * Flow;
float2 p = UV;
float lane = Noise.SampleLevel(NoiseSampler, float2(p.x * 1.0 + t * 0.02, p.y * 0.05 - t * 0.13), 0.0).r;
float a = Noise.SampleLevel(NoiseSampler, float2(p.x * 2.0 + lane * 0.2, p.y * 0.22 - t * 0.7), 0.0).r;
float b = Noise.SampleLevel(NoiseSampler, float2(p.x * 5.0 + a * 0.10, p.y * 0.11 - t * 1.9), 0.0).g;
float f = Noise.SampleLevel(NoiseSampler, float2(p.x * 6.0 + a * 0.06, p.y * 0.45 - t * 3.3), 0.0).b;
float k = Noise.SampleLevel(NoiseSampler, float2(p.x * 9.0 + b * 0.2, p.y * 1.3 - t * 2.6), 0.0).r;
float g = Noise.SampleLevel(NoiseSampler, float2(p.x * 5.0, p.y * 0.07 - t * 0.9), 0.0).a;
float g2 = Noise.SampleLevel(NoiseSampler, float2(p.x * 3.0 + 0.37, p.y * 2.3 - t * 6.1), 0.0).r;
// thick where a lane runs, thin and dark between; the streamers' edges torn by the fast small noise
float sheath = smoothstep(0.25, 0.95, a * 0.65 + lane * 0.35);
float stream = smoothstep(0.52, 0.95, b + (k - 0.5) * 0.25) * (0.35 + 0.65 * sheath);
float fil = pow(f, 4.0) * (0.3 + 0.7 * sheath);
float e = saturate(sheath * sheath * 0.45 + stream * 0.75 + fil * 0.6);
float flick = 0.86 + 0.09 * sin(T * 37.0) + 0.07 * sin(T * 23.3 + p.x * 12.566) + 0.05 * sin(T * 61.0 + p.y * 9.0);
// deep red, orange, white-pink where it burns hardest; a violet fringe where a hot flow thins out
float hh = saturate(Heat * (0.35 + 0.75 * e));
float3 col = lerp(float3(0.50, 0.015, 0.0), float3(1.0, 0.27, 0.03), smoothstep(0.05, 0.50, hh));
col = lerp(col, float3(1.0, 0.78, 0.80), smoothstep(0.62, 1.0, hh));
float fringe = smoothstep(0.02, 0.10, e) * smoothstep(0.34, 0.12, e) * smoothstep(0.55, 1.0, Heat);
col = lerp(col, float3(0.40, 0.18, 1.0), fringe * 0.6);
float ends = smoothstep(0.0, 0.08, p.y) * smoothstep(1.0, 0.85, p.y);
float body = (0.03 + 0.05 * Heat + fringe * 0.12 + e * e * 1.6 + e * 0.2) * flick;
// sparks: a few bright grains streaking down the flow, each alive for a moment
float spark = smoothstep(0.985, 1.0, g) * smoothstep(0.45, 0.9, g2) * smoothstep(0.15, 0.5, Heat);
float3 outc = col * body + float3(1.0, 0.85, 0.6) * (spark * 6.0);
return outc * (Plasma * Plasma * ends * Gain);
"""

# --- the porthole glass ----------------------------------------------------------------------------------------------

GLASS_BODY = r"""
// the pane, UV 0..1 across it (the rim at radius 1 of UV * 2 - 1). Mask: R frost and G scorch arrival (0 at the rim,
// 1 in the middle), B droplets, A rivulets (tiling in V). Unlit: Light is what the cabin's lamps give the glass
float4 m = Mask.SampleLevel(MaskSampler, UV, 0.0);
float riv = Mask.SampleLevel(MaskSampler, float2(UV.x, UV.y - T * 0.11), 0.0).a;
float riv2 = Mask.SampleLevel(MaskSampler, float2(UV.x * 1.7 + 0.31, UV.y * 0.8 - T * 0.19), 0.0).a;
float2 qc = UV * 2.0 - 1.0;
float fr = (1.0 - smoothstep(Frost * 1.15 - 0.14, Frost * 1.15, m.r)) * saturate(Frost * 30.0);
float sc = (1.0 - smoothstep(Scorch * 1.15 - 0.30, Scorch * 1.15, m.g)) * saturate(Scorch * 30.0);
float grazing = pow(1.0 - saturate(abs(dot(normalize(N), normalize(V)))), 3.0);
// a faint reflection of the lit cabin: stronger at a glancing look, a soft blot of the lamp near the top
float lamp = exp(-((qc.x + 0.25) * (qc.x + 0.25) + (qc.y + 0.45) * (qc.y + 0.45)) * 9.0);
float refl = (0.02 + 0.25 * grazing + 0.05 * lamp) * Reflect;
float3 col = Light.rgb * refl;
float op = 0.03 + refl * 0.5;
// soot browns the glass from the rim in: it takes light away and gives a little dull brown back
col = lerp(col, float3(0.050, 0.028, 0.012) * Light.rgb, sc * 0.9);
op = max(op, sc * (0.55 + 0.4 * Scorch));
// frost: white crystals, lit by the cabin
col = lerp(col, (float3(0.75, 0.82, 0.90) * (0.6 + 0.4 * m.r)) * Light.rgb, fr * 0.85);
op = max(op, fr * 0.8);
// water: beads standing on the pane, runs crawling down it; each a small lens (a bright rim low on the bead)
float bead = saturate((m.b + Wet - 1.0) * 6.0) * step(0.01, m.b);
float runs = saturate(riv * 1.2 + riv2 * 0.7 - 0.25) * Wet * Wet;
float water = saturate(bead + runs);
col += Light.rgb * water * (0.10 + 0.35 * smoothstep(0.35, 0.9, m.b) * bead);
op = max(op, water * 0.30);
// the whiteout
col = lerp(col, FogColor.rgb, Fog);
op = lerp(op, 1.0, Fog * Fog * (3.0 - 2.0 * Fog));
op = saturate(op);
"""
GLASS_HLSL = GLASS_BODY + "return col;\n"
GLASS_OP_HLSL = GLASS_BODY + "return op;\n"

# --- instruments ------------------------------------------------------------------------------------------------------

DIALS_E_HLSL = r"""
// the face's own print, glowing through from behind
return A.rgb * BackColor.rgb * (Backlight * Gain);
"""

NEEDLE_E_HLSL = r"""
return Color.rgb * (Backlight * Gain);
"""

DIGITS_HLSL = r"""
// every seven-segment cell of the panel: one quad per cell, UV 0..1 over it (V down), the second UV's U its number
// ((index + 0.5) / Count). Three windows, each a row of cells: WinN = (first cell, digits, a colon after every n-th
// place from the right or 0, a point after that place or 0), ValueN the whole number shown (the engine scales it),
// OptN = (1 keeps leading zeros, 1 blanks the window, 1 holds the colon steady). Segments a..g are bits 1, 2 .. 64
static const float SEGM[10] = { 63.0, 6.0, 91.0, 79.0, 102.0, 109.0, 125.0, 7.0, 127.0, 111.0 };
static const float4 SEGP[7] = { float4(0.0, 0.0, 1.0, 0.0), float4(1.0, 0.0, 1.0, 0.5), float4(1.0, 0.5, 1.0, 1.0), float4(0.0, 1.0, 1.0, 1.0),
                                float4(0.0, 0.5, 0.0, 1.0), float4(0.0, 0.0, 0.0, 0.5), float4(0.0, 0.5, 1.0, 0.5) };
float idx = floor(saturate(UV1.x) * Count * 0.9999);
float in1 = step(Win1.x - 0.5, idx);
float in2 = step(Win2.x - 0.5, idx);
float4 win = lerp(lerp(Win0, Win1, in1), Win2, in2);
float4 opt = lerp(lerp(Opt0, Opt1, in1), Opt2, in2);
float val = floor(abs(lerp(lerp(Value0, Value1, in1), Value2, in2)) + 0.5);
float n = max(floor(win.y + 0.5), 1.0);
float place = n - 1.0 - (idx - floor(win.x + 0.5));
float dv = floor(val / round(pow(10.0, max(place, 0.0))) + 1.0e-4);
float digit = dv - 10.0 * floor(dv / 10.0 + 1.0e-4);
float shown = saturate(step(0.5, dv) + step(place, 0.5) + step(0.5, opt.x) + step(place, win.w + 0.5) * step(0.5, win.w));
float mask = SEGM[int(digit + 0.5)];
// the digit's box leans a little; pp is in units of the cell's height so the strokes are even
float2 c = saturate(UV);
float lean = (0.5 - c.y) * 0.10;
float2 pp = float2((c.x - lean) * Aspect, c.y);
float2 b0 = float2(0.16 * Aspect, 0.14);
float2 b1 = float2(0.70 * Aspect, 0.86);
float th = 0.052;
float aa = max(fwidth(pp.y) * 1.2, 0.004);
float lit = 0.0;
float ghost = 0.0;
[unroll] for (int i = 0; i < 7; i++)
{
    float4 s = SEGP[i];
    float2 pa = lerp(b0, b1, s.xy);
    float2 pb = lerp(b0, b1, s.zw);
    float2 ab = pb - pa;
    float seglen = length(ab);
    float2 dir = ab / seglen;
    float along = dot(pp - pa, dir);
    float2 off = pp - pa - dir * clamp(along, th * 1.15, seglen - th * 1.15);
    float shape = 1.0 - smoothstep(th - aa, th + aa, abs(off.x) + abs(off.y) * 0.999);
    float bit = floor(mask / exp2(float(i)) + 1.0e-4);
    bit = bit - 2.0 * floor(bit * 0.5 + 1.0e-4);
    lit += shape * bit;
    ghost += shape;
}
// the point after place win.w, the colon after every win.z-th place: at the cell's right edge, by the gap
float2 dp = pp - float2(0.90 * Aspect, 0.86);
float dot1 = (1.0 - smoothstep(th - aa, th + aa, length(dp))) * step(abs(place - win.w), 0.25) * step(0.5, win.w);
float cm = place / max(win.z, 1.0);
float colon_here = step(0.5, win.z) * step(0.5, place) * step(abs(cm - floor(cm + 0.5)), 0.01);
float2 c1 = pp - float2(0.91 * Aspect, 0.34);
float2 c2 = pp - float2(0.89 * Aspect, 0.66);
float col2 = (1.0 - smoothstep(th * 0.9 - aa, th * 0.9 + aa, min(length(c1), length(c2)))) * colon_here * (0.25 + 0.75 * step(frac(T), 0.5 + opt.z));
float on = saturate(lit * shown + dot1 + col2) * (1.0 - saturate(opt.y));
return Color.rgb * (on * Gain + saturate(ghost) * 0.035 * Gain * Ghost);
"""

BAR_HLSL = r"""
// a column of lamps filling from the bottom (V runs down the picture; Flip 1 fills from the top of the UVs):
// green, amber from Warn, red from Hot (shares of the height)
float y = lerp(1.0 - UV.y, UV.y, step(0.5, Flip));
float n = max(Segments, 1.0);
float k = (floor(y * n * 0.9999) + 0.5) / n;
float gap = step(0.12, frac(y * n)) * step(frac(y * n), 0.88) * step(0.08, UV.x) * step(UV.x, 0.92);
float on = step(k, saturate(Level) + 0.5 / n - 1.0e-3) * step(1.0e-3, Level);
float3 ramp = lerp(float3(0.15, 1.0, 0.25), float3(1.0, 0.62, 0.08), step(Warn, k));
ramp = lerp(ramp, float3(1.0, 0.10, 0.05), step(Hot, k));
float blink = lerp(1.0, 0.55 + 0.45 * step(0.5, frac(T * 3.0)), step(Hot, saturate(Level)));
return ramp * gap * (on * blink * Gain + 0.03 * Gain);
"""

LAMPS_HLSL = r"""
// the annunciator board: 24 lamps, each a quad whose UV0 is its caption in the label atlas and whose second UV is the
// middle of its cell of a 6 x 4 grid (row 0 at V = 0): lamp = row * 6 + col. LampsA..F carry four lamps each (A.x =
// lamp 0 .. F.w = lamp 23): value = mode + 4 * class; mode 0 off, 1 on, 2 slow blink, 3 fast blink; class 0 green,
// 1 amber, 2 red, 3 white
float idx = floor(saturate(UV1.y) * 3.999) * 6.0 + floor(saturate(UV1.x) * 5.999);
float grp = floor(idx * 0.25 + 1.0e-3);
float4 sel = LampsA * (1.0 - step(0.5, grp)) + LampsB * (step(0.5, grp) - step(1.5, grp)) + LampsC * (step(1.5, grp) - step(2.5, grp))
           + LampsD * (step(2.5, grp) - step(3.5, grp)) + LampsE * (step(3.5, grp) - step(4.5, grp)) + LampsF * step(4.5, grp);
float k = idx - grp * 4.0;
float s = dot(sel, float4(1.0 - step(0.5, k), step(0.5, k) - step(1.5, k), step(1.5, k) - step(2.5, k), step(2.5, k)));
s = floor(s + 0.5);
float cls = floor(s * 0.25 + 1.0e-3);
float mode = s - cls * 4.0;
float slow = step(frac(T * 1.1), 0.5);
float fast = step(frac(T * 3.6), 0.5);
float on = step(0.5, mode) * lerp(1.0, lerp(slow, fast, step(2.5, mode)), step(1.5, mode));
float3 tint = lerp(float3(0.20, 1.0, 0.30), float3(1.0, 0.60, 0.08), step(0.5, cls));
tint = lerp(tint, float3(1.0, 0.10, 0.06), step(1.5, cls));
tint = lerp(tint, float3(0.95, 0.97, 1.0), step(2.5, cls));
// a lit lamp: its face glows, the caption stands dark on it; unlit: a dull face, pale words
float text = Cap.r;
float3 lit = tint * (1.0 - 0.88 * text) * Gain;
float3 dark = tint * 0.012 * Gain + float3(0.5, 0.5, 0.46) * text * 0.02 * Gain;
return lerp(dark, lit, on);
"""

LABELS_BASE_HLSL = r"""
return lerp(Plate.rgb, Ink.rgb, A.r);
"""
LABELS_E_HLSL = r"""
return GlowColor.rgb * (A.r * Backlight * Gain);
"""

CRT_HLSL = r"""
// a green vector screen, UV 0..1 (V down). The pictures live in the square in the screen's middle, y up (q; Aspect =
// the screen's width / height), which is the flight model's own "0..1 square" (FlightModel.h FInstruments Crt*).
// Mode 0: the trajectory: Europa and the planet as circles (PosEuropa / PosPlanet: xy, z radius), the model's path
// CrtPath between them (the same formula: 5 chords to the bend, 13 round the planet), flown solid and ahead dashed, the craft a blinking dot at
// Craft01 of it. Mode 1: the horizon (Att: x roll in radians, y pitch as a share of the screen). Mode 2: the load, a
// bar 0..6 g with its peak mark (GLoad.x now, GLoad.y the peak). Phase 0..8 lights one of nine pips along the top
// (a wordless phase name), -1 none
float2 p = UV;
float2 q = float2((p.x - 0.5) * Aspect + 0.5, 1.0 - p.y);
float px = max(fwidth(p.y), 0.0015);
float w = px * 1.2 + 0.0025;
float m0 = 1.0 - step(0.5, Mode);
float m1 = step(0.5, Mode) - step(1.5, Mode);
float m2 = step(1.5, Mode);
// the grid, everywhere
float2 gq = abs(frac(q * 8.0 + 0.5) - 0.5) / 8.0;
float lines = (1.0 - smoothstep(0.0, w, min(gq.x, gq.y))) * 0.10;
// page 0
float2 e = PosEuropa.xy;
float2 pl = PosPlanet.xy;
float de = abs(length(q - e) - PosEuropa.z);
float dpl = abs(length(q - pl) - PosPlanet.z);
float inpl = 1.0 - smoothstep(PosPlanet.z - w, PosPlanet.z, length(q - pl));
float circ = (1.0 - smoothstep(0.0, w, de)) + (1.0 - smoothstep(0.0, w, dpl)) + 0.25 * exp(-de * de / (w * w * 30.0)) + 0.25 * exp(-dpl * dpl / (w * w * 30.0));
// the planet hatched, so the two circles are never mistaken
circ += inpl * (1.0 - smoothstep(0.0, w, abs(frac((q.x + q.y) * 22.0) - 0.5) / 22.0)) * 0.35;
float2 e0 = e + float2(CRT_OX, CRT_OY);
float2 s0 = pl + CRT_R0 * float2(cos(CRT_A0), sin(CRT_A0));
float dpath = 9.0;
float sdone = 0.0;
float2 prev = e0;
float prevt = 0.0;
[unroll] for (int i = 1; i < 19; i++)
{
    float tt = min(float(i) / 5.0, 1.0) * CRT_BEND + max(float(i) - 5.0, 0.0) / 13.0 * (1.0 - CRT_BEND);
    float u1 = saturate(tt / CRT_BEND);
    float2 pa = lerp(e0, s0, u1) + float2(0.0, CRT_BUMP * sin(u1 * 3.14159265) * (1.0 - u1));
    float u2 = saturate((tt - CRT_BEND) / (1.0 - CRT_BEND));
    float an = lerp(CRT_A0, CRT_A1, u2);
    float2 pb = pl + lerp(CRT_R0, CRT_R1, u2 * u2) * float2(cos(an), sin(an));
    float2 cur = lerp(pa, pb, step(CRT_BEND, tt));
    float2 ab = cur - prev;
    float hh = saturate(dot(q - prev, ab) / max(dot(ab, ab), 1.0e-6));
    float dd = length(q - prev - ab * hh);
    float nearer = step(dd, dpath);
    sdone = lerp(sdone, lerp(prevt, tt, hh), nearer);
    dpath = min(dpath, dd);
    prev = cur;
    prevt = tt;
}
// flown: a solid line with its glow; ahead: dashes; none inside the two bodies
float flown = step(sdone, Craft01);
float dash = lerp(step(0.5, frac(sdone * 36.0)), 1.0, flown);
float clear = step(PosEuropa.z, length(q - e)) * (1.0 - inpl);
float path = ((1.0 - smoothstep(0.0, w, dpath)) * (0.45 * dash + 0.55 * flown) + 0.2 * flown * exp(-dpath * dpath / (w * w * 30.0))) * clear;
float ct = saturate(Craft01);
float c1 = saturate(ct / CRT_BEND);
float2 ca = lerp(e0, s0, c1) + float2(0.0, CRT_BUMP * sin(c1 * 3.14159265) * (1.0 - c1));
float c2 = saturate((ct - CRT_BEND) / (1.0 - CRT_BEND));
float can = lerp(CRT_A0, CRT_A1, c2);
float2 cb = pl + lerp(CRT_R0, CRT_R1, c2 * c2) * float2(cos(can), sin(can));
float2 cp = lerp(ca, cb, step(CRT_BEND, ct));
float dc = length(q - cp);
float blink = 0.35 + 0.65 * step(frac(T * 1.6), 0.6);
float craft = ((1.0 - smoothstep(w * 1.5, w * 3.0, dc)) * 1.6 + exp(-dc * dc / (w * w * 60.0)) * 0.7) * blink;
float page0 = circ + path + craft;
// page 1: the horizon line, tilted and shifted, a fixed little aircraft, pitch rungs
float cr = cos(Att.x);
float sr = sin(Att.x);
float2 hc = float2(q.x - 0.5, 0.5 - q.y);
float2 hr = float2(hc.x * cr + hc.y * sr, -hc.x * sr + hc.y * cr);
float hy = hr.y - Att.y;
float hor = (1.0 - smoothstep(0.0, w, abs(hy))) + 0.25 * exp(-hy * hy / (w * w * 30.0));
float rung = (1.0 - smoothstep(0.0, w, abs(frac(hy * 5.0 + 0.5) - 0.5) / 5.0)) * step(abs(hr.x), 0.10) * step(abs(hy), 0.45) * 0.5;
float wing = (1.0 - smoothstep(0.0, w, abs(hc.y))) * step(0.08, abs(hc.x)) * step(abs(hc.x), 0.22) + (1.0 - smoothstep(w, w * 2.5, length(hc)));
float ring = (1.0 - smoothstep(0.0, w, abs(length(hc) - 0.40))) * 0.5;
float page1 = (hor + rung) * step(length(hc), 0.40) + wing * 0.9 + ring;
// page 2: the load
float gx = (p.x - 0.12) / 0.76;
float inbar = step(0.0, gx) * step(gx, 1.0);
float fill = step(gx, saturate(GLoad.x / 6.0)) * step(abs(p.y - 0.5), 0.10) * inbar * (0.55 + 0.25 * step(0.5, frac(p.x * 60.0)));
float frame = (1.0 - smoothstep(0.0, w, abs(abs(p.y - 0.5) - 0.12))) * inbar;
float tick = (1.0 - smoothstep(0.0, w, abs(frac(gx * 6.0 + 0.5) - 0.5) / 6.0 * 0.76)) * step(0.12, abs(p.y - 0.5)) * step(abs(p.y - 0.5), 0.17) * step(-0.01, gx) * step(gx, 1.01);
float peak = (1.0 - smoothstep(0.0, w * 1.5, abs(gx - saturate(GLoad.y / 6.0)) * 0.76)) * step(abs(p.y - 0.5), 0.20) * inbar;
float page2 = fill + frame + tick + peak;
// the phase pips along the top: nine small boxes, the current one full, the flown ones outlined brighter
float kx = (p.x - 0.14) / 0.72 * 9.0;
float kc = floor(kx);
float2 kq = float2(abs(frac(kx) - 0.5), abs(p.y - 0.065) * 9.0 * 0.72 * Aspect);
float kbox = max(kq.x, kq.y);
float kin = step(0.0, kx) * step(kx, 9.0) * step(-0.5, Phase);
float kedge = (1.0 - smoothstep(0.0, w * 9.0, abs(kbox - 0.30))) * (0.25 + 0.35 * step(kc, Phase + 0.5));
float kfill = step(kbox, 0.30) * step(abs(kc - Phase), 0.5);
float pips = (kedge + kfill * 0.9) * kin;
// an optional caption from the label atlas along the bottom (CapRect: u0, v0, u1, v1 in the atlas; all 0 = none)
float2 cq = (p - float2(0.08, 0.86)) / float2(0.84, 0.10);
float incap = step(0.0, cq.x) * step(cq.x, 1.0) * step(0.0, cq.y) * step(cq.y, 1.0) * step(1.0e-4, CapRect.z - CapRect.x);
float cap = Labels.SampleLevel(LabelsSampler, lerp(CapRect.xy, CapRect.zw, saturate(cq)), 0.0).r * incap;
float v = lines + page0 * m0 + page1 * m1 + page2 * m2 + pips + cap * 0.9;
// the tube: scanlines, a slow roll bar, a breath of flicker, dark corners, the phosphor's own colour
float scan = 0.82 + 0.18 * sin(p.y * 6.2832 * 120.0);
float roll = 1.0 + 0.10 * smoothstep(0.92, 1.0, frac(p.y * 0.7 - T * 0.23));
float flick = 0.96 + 0.04 * sin(T * 53.0);
float2 vq = (p - 0.5) * 2.0;
float vig = saturate(1.15 - dot(vq * vq, vq * vq) * 0.6);
float3 phos = lerp(float3(0.10, 1.0, 0.35), float3(0.75, 1.0, 0.80), saturate(v - 1.0));
return (phos * v * scan * roll * flick * Power + float3(0.004, 0.022, 0.010) * Power) * (vig * Gain);
"""
for _k, _v in (("CRT_OX", CRT_PATH["ox"]), ("CRT_OY", CRT_PATH["oy"]), ("CRT_R0", CRT_PATH["r0"]), ("CRT_R1", CRT_PATH["r1"]),
               ("CRT_A0", math.radians(CRT_PATH["a0"])), ("CRT_A1", math.radians(CRT_PATH["a1"])), ("CRT_BEND", CRT_PATH["bend"]),
               ("CRT_BUMP", CRT_PATH["bump"])):
    CRT_HLSL = CRT_HLSL.replace(_k, "%.6f" % _v)

GLOBE_HLSL = r"""
// the little globe: the planet's own map on the sphere's UVs, a graticule every 30 degrees, a red point at the island,
// a pale cross-hair ring over the point under the craft (CraftLat / CraftLon degrees; Cross 0 hides it)
float3 c = Map.rgb * Tint.rgb;
float2 g = abs(frac(UV * float2(12.0, 6.0) + 0.5) - 0.5) / float2(12.0, 6.0);
float grat = 1.0 - smoothstep(0.0012, 0.0030, min(g.x * 2.0 * cos((UV.y - 0.5) * 3.14159), g.y));
c = lerp(c, float3(0.75, 0.72, 0.60), grat * 0.35);
float2 iq = (UV - float2(IslandLon / 360.0 + 0.5, 0.5 - IslandLat / 180.0)) * float2(2.0, 1.0);
c = lerp(c, float3(0.8, 0.08, 0.05), (1.0 - smoothstep(0.004, 0.006, length(iq))) * Mark);
float du = UV.x - (CraftLon / 360.0 + 0.5);
float2 kq = float2((du - floor(du + 0.5)) * 2.0 * cos(radians(CraftLat)), UV.y - (0.5 - CraftLat / 180.0));
float kr = length(kq);
float ring = 1.0 - smoothstep(0.0015, 0.0035, abs(kr - 0.022));
float arms = (1.0 - smoothstep(0.0010, 0.0030, min(abs(kq.x), abs(kq.y)))) * step(0.008, kr) * step(kr, 0.040);
c = lerp(c, float3(0.95, 0.92, 0.80), saturate(ring + arms) * Cross);
return c;
"""

RECORD_OFF_HLSL = r"""
return WP - C;
"""

RECORD_BODY = r"""
// the vinyl (the label is its own slot: M_SC_RecordLabel). No UVs: O is this pixel from the record's pivot in the
// world, Ol the same in the record's own frame (the disc in its XY plane, RecordR cm), so the pressing's wobble turns
// with the record while the sheen's wedges stay where the eye and the lamp (LightDir: toward it, world) put them
float r = length(Ol.xy) / RecordR;
float ang = atan2(Ol.y, Ol.x + 1.0e-5);
float lr = LabelR / RecordR;
// grooves: fine rings, a wobble from the pressing, smooth bands between the tracks and at the run-out
float wob = 0.004 * sin(ang) + 0.002 * sin(ang * 2.0 + 1.3);
float band = smoothstep(lr + 0.07, lr + 0.09, r) * (1.0 - smoothstep(0.955, 0.965, r));
float gapk = frac((r + wob) * 3.1 + 0.37);
float gap = 1.0 - smoothstep(0.03, 0.05, abs(gapk - 0.5));
float track = band * (1.0 - 0.85 * gap);
float fine = 0.5 + 0.5 * sin((r + wob) * 900.0);
float fade = saturate(1.0 - fwidth(r) * 300.0);
// the grooves throw the light back along the line from the centre toward the half vector's foot on the disc
float3 n = normalize(N);
float3 h = normalize(normalize(V) + normalize(LightDir.xyz));
float3 hp = h - n * dot(h, n) + float3(1.0e-4, 0.0, 0.0);
float3 op = O - n * dot(O, n) + float3(0.0, 1.0e-4, 0.0);
float3 hd = normalize(hp);
float3 od = normalize(op);
float ca = dot(od, hd);
float cb = dot(od, normalize(hd * 0.45 + cross(n, hd) * 0.89));
float wedge = pow(abs(ca), 26.0) + 0.35 * pow(abs(cb), 60.0);
float sheen = wedge * track * (0.65 + 0.35 * fine * fade);
float3 base = float3(0.012, 0.012, 0.014);
float rough = lerp(0.22, 0.38 + 0.12 * fine * fade, track);
float3 emis = Sheen.rgb * sheen;
"""
RECORD_BASE_HLSL = RECORD_BODY + "return base;\n"
RECORD_ROUGH_HLSL = RECORD_BODY + "return rough;\n"
RECORD_E_HLSL = RECORD_BODY + "return emis;\n"

RECORD_LABEL_HLSL = r"""
// the paper label: the atlas' round print over the paper, the spindle hole dark
float r = length(UV * 2.0 - 1.0);
float3 c = lerp(Paper.rgb, A.rgb, A.a);
return lerp(float3(0.004, 0.004, 0.004), c, smoothstep(HoleR - 0.01, HoleR + 0.01, r));
"""

SLEEVE_UV_HLSL = r"""
// cell Cell of a 4 x 2 atlas (row 0 on top: the sleeves, the record labels); the UV 0..1 is that cell's picture
float cx = fmod(floor(Cell + 0.5), 4.0);
float cy = floor((Cell + 0.5) / 4.0);
return (float2(cx, cy) + saturate(UV)) * float2(0.25, 0.5);
"""
SLEEVE_BASE_HLSL = r"""
// the printed card, rubbed pale along its edges, a ring worn into it by the record inside
float2 e = min(UV, 1.0 - UV);
float wear = (1.0 - smoothstep(0.0, 0.035, min(e.x, e.y))) * 0.35;
float ring = (1.0 - smoothstep(0.0, 0.012, abs(length(UV - 0.5) - 0.44))) * 0.10;
return lerp(A.rgb * Tint.rgb, float3(0.78, 0.76, 0.70), saturate(wear + ring) * Wear);
"""

CHUTE_UV_HLSL = r"""
// U round the canopy (TileU = gores / 2: the tile is an orange gore and a white one), V from the skirt to the vent
return UV * float2(TileU, TileV);
"""

CHUTE_SSS_HLSL = r"""
// the sun through the cloth
return A.rgb * Tint.rgb * Through;
"""

# =====================================================================================================================
# The materials as data: what the editor builds and what the lint and the previews read.
#   inputs: (pin, source). Sources: "uv", "uv1", "time", "wp", "objpos", "objr", "campos", "camvec", "vnormal",
#   ("scalar", Name, default), ("vector", Name, (r, g, b, a)), ("tex", Name, texture, sampler) a texture object for
#   SampleLevel, ("sample", Name, texture, sampler[, uv node]) a sampled RGBA, ("node", key) an earlier node's output,
#   ("local", key) the same turned from the world into the mesh's frame.
#   nodes: (key, hlsl, type, inputs) in order; outputs: material property -> key (or a ("scalar"|"vector", ...) source).

F1, F2, F3 = "float1", "float2", "float3"
S_COLOR, S_LINEAR, S_MASKS, S_GREY, S_NORMAL = "color", "linear_color", "masks", "linear_grayscale", "normal"


def _planet_inputs():
    return [("Nw", ("node", "hit")), ("L", ("local", "hit")), ("Ls", ("local", "sun")), ("V", "camvec"),
            ("SunDir", ("vector", "SunDir", (0.3, 0.2, 0.9, 0))), ("SunLight", ("vector", "SunLight", (12.7, 10.5, 8.6, 0))),
            ("T", "time"), ("Gain", ("scalar", "Gain", 2.2)), ("Lights", ("scalar", "Lights", 20.0)),
            ("CloudShift", ("scalar", "CloudShift", 0.0)), ("Detail", ("scalar", "Detail", 1.0)),
            ("Rough", ("scalar", "Rough", 0.33)), ("Glint", ("scalar", "Glint", 1.0)), ("Haze", ("scalar", "Haze", 1.0)),
            ("IslandLat", ("scalar", "IslandLat", EU_ISLAND[0])), ("IslandLon", ("scalar", "IslandLon", EU_ISLAND[1])),
            ("IslandSizeKm", ("scalar", "IslandSizeKm", 1000.0)), ("PlanetRadiusKm", ("scalar", "PlanetRadiusKm", 6371.0)),
            ("Albedo", ("tex", "Albedo", EU_TEX + "/T_EU_planet_C", S_COLOR)), ("Clouds", ("tex", "Clouds", EU_TEX + "/T_EU_clouds", S_LINEAR)),
            ("Island", ("tex", "Island", "T_SC_island", S_COLOR)), ("IslandM", ("tex", "IslandM", "T_SC_island_M", S_MASKS)),
            ("Noise", ("tex", "Noise", "T_SC_noise", S_MASKS))]


def _eu_const(code):
    la, lo = math.radians(EU_ISLAND[0]), math.radians(EU_ISLAND[1])
    return (code.replace("EU_IX", "%.6f" % (math.cos(la) * math.cos(lo))).replace("EU_IY", "%.6f" % (math.cos(la) * math.sin(lo)))
            .replace("EU_IZ", "%.6f" % math.sin(la)))


PLANET_HLSL = _eu_const(PLANET_HLSL)
_LAMPS = [("Lamps" + c, ("vector", "Lamps" + c, (0, 0, 0, 0))) for c in "ABCDEF"]

MATERIALS = {
    "M_SC_PlanetNear": dict(
        shading="unlit", blend="masked", two_sided=True,
        nodes=[("hit", PLANET_HIT_HLSL, F3, [("P", "wp"), ("C", "objpos"), ("Cam", "campos"), ("ObjR", "objr")]),
               ("sun", "return SunDir.xyz;\n", F3, [("SunDir", ("vector", "SunDir", (0.3, 0.2, 0.9, 0)))]),
               ("mask", PLANET_MASK_HLSL, F1, [("Nw", ("node", "hit"))]),
               ("col", PLANET_HLSL, F3, _planet_inputs())],
        outputs={"emissive": "col", "opacity_mask": "mask"}),
    "M_SC_PlanetAir": dict(
        shading="unlit", blend="additive", two_sided=True,
        nodes=[("col", AIR_HLSL, F3, [("WP", "wp"), ("C", "objpos"), ("Cam", "campos"), ("ObjR", "objr"),
                                      ("SunDir", ("vector", "SunDir", (0.3, 0.2, 0.9, 0))),
                                      ("SunLight", ("vector", "SunLight", (12.7, 10.5, 8.6, 0))),
                                      ("Air", ("vector", "Air", (1.045, 1.0, 0, 0))), ("Gain", ("scalar", "Gain", 2.2))])],
        outputs={"emissive": "col"}),
    "M_SC_Plasma": dict(
        shading="unlit", blend="additive", two_sided=False,     # (SM_SC_FireShell has both faces)
        nodes=[("col", PLASMA_HLSL, F3, [("UV", "uv"), ("T", "time"), ("Heat", ("scalar", "Heat", 0.6)), ("Plasma", ("scalar", "Plasma", 0.0)),
                                         ("Flow", ("scalar", "Flow", 1.0)), ("Gain", ("scalar", "Gain", 40.0)),
                                         ("Noise", ("tex", "Noise", "T_SC_noise", S_MASKS))])],
        outputs={"emissive": "col"}),
    "M_SC_Glass": dict(
        shading="unlit", blend="translucent", two_sided=False,  # (the panes and the covers have a face each way)
        nodes=[(k, code, t, [("UV", "uv"), ("T", "time"), ("N", "vnormal"), ("V", "camvec"),
                             ("Frost", ("scalar", "Frost", 0.0)), ("Scorch", ("scalar", "Scorch", 0.0)), ("Wet", ("scalar", "Wet", 0.0)),
                             ("Fog", ("scalar", "Fog", 0.0)), ("FogColor", ("vector", "FogColor", (6.0, 6.2, 6.5, 0))),
                             ("Reflect", ("scalar", "Reflect", 1.0)), ("Light", ("vector", "Light", (1.2, 1.0, 0.8, 0))),
                             ("Mask", ("tex", "Mask", "T_SC_glass", S_MASKS))])
               for k, code, t in (("col", GLASS_HLSL, F3), ("op", GLASS_OP_HLSL, F1))],
        outputs={"emissive": "col", "opacity": "op"}),
    "M_SC_Dials": dict(
        shading="lit",
        nodes=[("e", DIALS_E_HLSL, F3, [("A", ("sample", "Atlas", "T_SC_dials", S_COLOR)), ("BackColor", ("vector", "BackColor", (1.0, 0.78, 0.45, 0))),
                                        ("Backlight", ("scalar", "Backlight", 0.0)), ("Gain", ("scalar", "Gain", 4.0))])],
        outputs={"base_color": ("sample", "Atlas", "T_SC_dials", S_COLOR), "emissive": "e", "roughness": ("scalar", "Roughness", 0.45)}),
    "M_SC_Needle": dict(
        shading="lit",
        nodes=[("e", NEEDLE_E_HLSL, F3, [("Color", ("vector", "Color", (0.95, 0.42, 0.10, 0))), ("Backlight", ("scalar", "Backlight", 0.0)),
                                         ("Gain", ("scalar", "Gain", 2.0))])],
        outputs={"base_color": ("vector", "Color", (0.95, 0.42, 0.10, 0)), "emissive": "e", "roughness": ("scalar", "Roughness", 0.5)}),
    "M_SC_Digits": dict(
        shading="unlit",
        nodes=[("col", DIGITS_HLSL, F3,
                [("UV", "uv"), ("UV1", "uv1"), ("T", "time"), ("Count", ("scalar", "Count", float(DIGIT_COUNT)))]
                + [("Value%d" % i, ("scalar", "Value%d" % i, 0.0)) for i in range(3)]
                + [("Win%d" % i, ("vector", "Win%d" % i, tuple(float(x) for x in w[1:]))) for i, w in enumerate(DIGIT_WINDOWS)]
                + [("Opt%d" % i, ("vector", "Opt%d" % i, (1.0 if w[0] == "clock" else 0.0, 0.0, 0.0, 0.0))) for i, w in enumerate(DIGIT_WINDOWS)]
                + [("Aspect", ("scalar", "Aspect", round(DIGIT_ASPECT, 4))), ("Ghost", ("scalar", "Ghost", 1.0)),
                   ("Color", ("vector", "Color", (1.0, 0.30, 0.04, 0))), ("Gain", ("scalar", "Gain", 8.0))])],
        outputs={"emissive": "col"}),
    "M_SC_Bar": dict(
        shading="unlit",
        nodes=[("col", BAR_HLSL, F3, [("UV", "uv"), ("T", "time"), ("Level", ("scalar", "Level", 0.0)), ("Segments", ("scalar", "Segments", 20.0)),
                                      ("Warn", ("scalar", "Warn", 0.6)), ("Hot", ("scalar", "Hot", 0.85)), ("Flip", ("scalar", "Flip", 0.0)),
                                      ("Gain", ("scalar", "Gain", 6.0))])],
        outputs={"emissive": "col"}),
    "M_SC_Lamps": dict(
        shading="unlit",
        nodes=[("col", LAMPS_HLSL, F3, [("UV1", "uv1"), ("T", "time"), ("Cap", ("sample", "Atlas", "T_SC_labels", S_GREY)),
                                        ("Gain", ("scalar", "Gain", 8.0))] + _LAMPS)],
        outputs={"emissive": "col"}),
    "M_SC_Labels": dict(
        shading="lit",
        nodes=[("base", LABELS_BASE_HLSL, F3, [("A", ("sample", "Atlas", "T_SC_labels", S_GREY)), ("Plate", ("vector", "Plate", (0.035, 0.04, 0.04, 0))),
                                               ("Ink", ("vector", "Ink", (0.80, 0.78, 0.68, 0)))]),
               ("e", LABELS_E_HLSL, F3, [("A", ("sample", "Atlas", "T_SC_labels", S_GREY)), ("GlowColor", ("vector", "GlowColor", (1.0, 0.78, 0.45, 0))),
                                         ("Backlight", ("scalar", "Backlight", 0.0)), ("Gain", ("scalar", "Gain", 1.5))])],
        outputs={"base_color": "base", "emissive": "e", "roughness": ("scalar", "Roughness", 0.55)}),
    "M_SC_CRT": dict(
        shading="unlit",
        nodes=[("col", CRT_HLSL, F3, [("UV", "uv"), ("T", "time"), ("Mode", ("scalar", "Mode", 0.0)),
                                      ("PosEuropa", ("vector", "PosEuropa", CRT_BODIES[0] + (0,))),
                                      ("PosPlanet", ("vector", "PosPlanet", CRT_BODIES[1] + (0,))),
                                      ("Craft01", ("scalar", "Craft01", 0.0)), ("Phase", ("scalar", "Phase", -1.0)),
                                      ("Att", ("vector", "Att", (0, 0, 0, 0))), ("GLoad", ("vector", "GLoad", (0, 0, 0, 0))),
                                      ("CapRect", ("vector", "CapRect", (0, 0, 0, 0))), ("Aspect", ("scalar", "Aspect", round(CRT_ASPECT, 4))),
                                      ("Power", ("scalar", "Power", 1.0)), ("Gain", ("scalar", "Gain", 5.0)),
                                      ("Labels", ("tex", "Labels", "T_SC_labels", S_GREY))])],
        outputs={"emissive": "col"}),
    "M_SC_Globe": dict(
        shading="lit",
        nodes=[("base", GLOBE_HLSL, F3, [("UV", "uv"), ("Map", ("sample", "Map", EU_TEX + "/T_EU_planet_C", S_COLOR)),
                                         ("Tint", ("vector", "Tint", (1.6, 1.6, 1.6, 0))), ("Mark", ("scalar", "Mark", 1.0)),
                                         ("IslandLat", ("scalar", "IslandLat", EU_ISLAND[0])), ("IslandLon", ("scalar", "IslandLon", EU_ISLAND[1])),
                                         ("CraftLat", ("scalar", "CraftLat", 0.0)), ("CraftLon", ("scalar", "CraftLon", 0.0)),
                                         ("Cross", ("scalar", "Cross", 0.0))])],
        outputs={"base_color": "base", "roughness": ("scalar", "Roughness", 0.3)}),
    "M_SC_Ball": dict(
        shading="lit",
        nodes=[("e", DIALS_E_HLSL, F3, [("A", ("sample", "Atlas", "T_SC_ball", S_COLOR)), ("BackColor", ("vector", "BackColor", (1.0, 0.78, 0.45, 0))),
                                        ("Backlight", ("scalar", "Backlight", 0.0)), ("Gain", ("scalar", "Gain", 1.0))])],
        outputs={"base_color": ("sample", "Atlas", "T_SC_ball", S_COLOR), "emissive": "e", "roughness": ("scalar", "Roughness", 0.35)}),
    "M_SC_Record": dict(
        shading="lit",
        nodes=[("off", RECORD_OFF_HLSL, F3, [("WP", "wp"), ("C", "objpos")])]
        + [(k, code, t, [("O", ("node", "off")), ("Ol", ("local", "off")), ("N", "vnormal"), ("V", "camvec"),
                         ("RecordR", ("scalar", "RecordR", RECORD_R)), ("LabelR", ("scalar", "LabelR", LABEL_R)),
                         ("LightDir", ("vector", "LightDir", (0.0, 0.0, 1.0, 0))),
                         ("Sheen", ("vector", "Sheen", (0.25, 0.22, 0.18, 0)))])
           for k, code, t in (("base", RECORD_BASE_HLSL, F3), ("rough", RECORD_ROUGH_HLSL, F1), ("e", RECORD_E_HLSL, F3))],
        outputs={"base_color": "base", "roughness": "rough", "emissive": "e"}),
    "M_SC_RecordLabel": dict(
        shading="lit",
        nodes=[("luv", SLEEVE_UV_HLSL, F2, [("UV", "uv"), ("Cell", ("scalar", "Cell", 0.0))]),
               ("base", RECORD_LABEL_HLSL, F3, [("UV", "uv"), ("A", ("sample", "Atlas", "T_SC_record_labels", S_COLOR, "luv")),
                                                ("Paper", ("vector", "Paper", (0.62, 0.58, 0.48, 0))),
                                                ("HoleR", ("scalar", "HoleR", 0.08))])],
        outputs={"base_color": "base", "roughness": ("scalar", "Roughness", 0.65)}),
    "M_SC_Sleeve": dict(
        shading="lit",
        nodes=[("suv", SLEEVE_UV_HLSL, F2, [("UV", "uv"), ("Cell", ("scalar", "Cell", 0.0))]),
               ("base", SLEEVE_BASE_HLSL, F3, [("UV", "uv"), ("A", ("sample", "Atlas", "T_SC_sleeves", S_COLOR, "suv")),
                                               ("Tint", ("vector", "Tint", (1, 1, 1, 0))), ("Wear", ("scalar", "Wear", 1.0))])],
        outputs={"base_color": "base", "roughness": ("scalar", "Roughness", 0.7)}),
}
# sources that need a fallback when another thread's texture is not there yet: (wanted, stand-in of the same kind)
FALLBACK = {"T_SC_labels": "T_SC_digits", "T_SC_record_labels": "T_SC_island", "T_SC_sleeves": "T_SC_dials"}

# the kit's material slots -> materials, by the slot's exact name in lower case (blender/spacecraft/spacecraft_kit.py's
# MATS). tx_sc_<surface> goes to MI_SC_<surface>. The flat ones not named here (SC_dark, SC_red, SC_yellow, SC_lamp,
# SC_case, SC_paper, SC_book, SC_blanket, SC_cap, SC_cord) keep the glb's own material. A (mesh, slot) pair wins over
# the slot alone; Content/Spacecraft/kit.json may add or override with "materials": {slot or "Mesh:slot": material}
SLOTS = {
    "sc_dials": "M_SC_Dials", "sc_needle": "M_SC_Needle", "sc_digits": "M_SC_Digits", "sc_bar": "M_SC_Bar", "sc_crt": "M_SC_CRT",
    "sc_lamps": "M_SC_Lamps", "sc_labels": "MI_SC_Labels_ru", "sc_glass": "MI_SC_GlassCover", "sc_porthole": "M_SC_Glass",
    "sc_fire": "M_SC_Plasma", "sc_globe": "M_SC_Globe", "sc_ball": "M_SC_Ball", "sc_record": "M_SC_Record",
    "sc_record_label": "M_SC_RecordLabel", "sc_sleeve": "M_SC_Sleeve", "sc_chute": "MI_SC_chute",
    "sc_planet": "M_SC_PlanetNear", "sc_air": "M_SC_PlanetAir",
    "sm_sc_drogue:sc_chute": "MI_SC_chute_drogue",
}
FLAT_SLOTS = ("sc_dark", "sc_red", "sc_yellow", "sc_lamp", "sc_case", "sc_paper", "sc_book", "sc_blanket", "sc_cap", "sc_cord")


# =====================================================================================================================
# The editor side


class Graph:
    """A new material and small helpers for wiring its nodes (import_europa.py's, in this folder)."""

    def __init__(self, name, shading=None, blend=None, two_sided=False):
        if eal.does_asset_exist(MAT + "/" + name):
            eal.delete_asset(MAT + "/" + name)
        self.m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, MAT, unreal.Material, unreal.MaterialFactoryNew())
        if shading is not None:
            self.m.set_editor_property("shading_model", shading)
        if blend is not None:
            self.m.set_editor_property("blend_mode", blend)
        self.m.set_editor_property("two_sided", two_sided)
        self.y = 0
        self.params = {}

    def node(self, cls, x=-600, **props):
        self.y += 120
        e = mel.create_material_expression(self.m, cls, x, self.y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(self, src, dst, pin="", src_pin=""):
        if not mel.connect_material_expressions(src, src_pin, dst, pin):
            raise RuntimeError("%s: no pin %r on %s" % (self.m.get_name(), pin, dst.get_name()))

    def op(self, cls, a, b, a_pin="", b_pin=""):
        e = self.node(cls, -300)
        self.link(a, e, "A", a_pin)
        self.link(b, e, "B", b_pin)
        return e

    def scalar(self, name, v):
        if ("s", name) not in self.params:
            self.params[("s", name)] = self.node(X.MaterialExpressionScalarParameter, -900, parameter_name=name, default_value=float(v))
        return self.params[("s", name)]

    def vector(self, name, v):
        if ("v", name) not in self.params:
            self.params[("v", name)] = self.node(X.MaterialExpressionVectorParameter, -900, parameter_name=name,
                                                 default_value=unreal.LinearColor(*v))
        return self.params[("v", name)]

    def custom(self, code, inputs, out=None):
        c = self.node(X.MaterialExpressionCustom, -450, code=code,
                      output_type=out if out is not None else unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        ins = []
        for name, _ in inputs:
            ci = unreal.CustomInput()
            ci.set_editor_property("input_name", name)
            ins.append(ci)
        c.set_editor_property("inputs", ins)
        for name, src in inputs:
            s, p = src if isinstance(src, tuple) else (src, "")
            self.link(s, c, name, p)
        return c

    def out(self, src, prop, src_pin=""):
        if not mel.connect_material_property(src, src_pin, prop):
            raise RuntimeError("%s: could not connect %s" % (self.m.get_name(), prop))

    def finish(self):
        mel.recompile_material(self.m)
        eal.save_loaded_asset(self.m)
        return self.m


def tex(name):
    """A texture by its name here, by a full /Game path, or its stand-in when another thread's is not imported yet."""
    if name.startswith("/"):
        return eal.load_asset(name)
    for path in (TEX + "/" + name, LOCAL + "/ru/" + name):
        if eal.does_asset_exist(path):
            return eal.load_asset(path)
    if name in FALLBACK:
        log("no texture %s yet: %s stands in" % (name, FALLBACK[name]))
        return eal.load_asset(TEX + "/" + FALLBACK[name])
    raise RuntimeError("no texture %s" % name)


def _sampler(kind):
    st = unreal.MaterialSamplerType
    return {S_COLOR: st.SAMPLERTYPE_COLOR, S_LINEAR: st.SAMPLERTYPE_LINEAR_COLOR, S_MASKS: st.SAMPLERTYPE_MASKS,
            S_GREY: st.SAMPLERTYPE_LINEAR_GRAYSCALE, S_NORMAL: st.SAMPLERTYPE_NORMAL}[kind]


def import_textures():
    """Every T_SC_*.png of textures/spacecraft; T_SC_<name>_<lang>.png goes to Localized/<lang>/T_SC_<name>
    (LyudText::LoadTexture's place)."""
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    names = sorted(f for f in os.listdir(TEX_SRC) if f.startswith("T_SC_") and f.endswith(".png"))
    tasks, where = [], {}
    for f in names:
        base = f[:-4]
        lang = next((g for g in LANGS if base.endswith("_" + g)), None)
        dest, asset = (LOCAL + "/" + lang, base[:-len(lang) - 1]) if lang else (TEX, base)
        t = unreal.AssetImportTask()
        t.filename = os.path.join(TEX_SRC, f)
        t.destination_path = dest
        t.destination_name = asset
        t.automated = True
        t.replace_existing = True
        t.save = False
        tasks.append(t)
        where[f] = dest + "/" + asset
    tools.import_asset_tasks(tasks)
    tc = unreal.TextureCompressionSettings
    for f in names:
        a = eal.load_asset(where[f])
        if a is None:
            log("texture %s did not import" % f)
            continue
        base = f[:-4]
        if base.endswith("_N"):
            a.set_editor_property("compression_settings", tc.TC_NORMALMAP)
            a.set_editor_property("srgb", False)
        elif base in ("T_SC_glass", "T_SC_noise", "T_SC_island_M"):
            # channels that are numbers, not colours
            a.set_editor_property("compression_settings", tc.TC_MASKS)
            a.set_editor_property("srgb", False)
        elif base == "T_SC_digits" or base.startswith("T_SC_labels"):
            a.set_editor_property("compression_settings", tc.TC_GRAYSCALE)
            a.set_editor_property("srgb", False)
        if base in ("T_SC_island", "T_SC_island_M", "T_SC_dials", "T_SC_digits", "T_SC_record_labels") or base.startswith(("T_SC_labels", "T_SC_sleeves")):
            a.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
            a.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
        if base in ("T_SC_ball",):
            a.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
        eal.save_loaded_asset(a)
    return len(names)


def _source(g, built, src):
    """A spec source -> (expression, output pin)."""
    if isinstance(src, str):
        cls = {"uv": X.MaterialExpressionTextureCoordinate, "uv1": X.MaterialExpressionTextureCoordinate, "time": X.MaterialExpressionTime,
               "wp": X.MaterialExpressionWorldPosition, "objpos": X.MaterialExpressionObjectPositionWS,
               "objr": X.MaterialExpressionObjectRadius, "campos": X.MaterialExpressionCameraPositionWS,
               "camvec": X.MaterialExpressionCameraVectorWS, "vnormal": X.MaterialExpressionVertexNormalWS}[src]
        if ("n", src) not in g.params:
            g.params[("n", src)] = g.node(cls, -900, coordinate_index=1) if src == "uv1" else g.node(cls, -900)
        return g.params[("n", src)], ""
    kind = src[0]
    if kind == "scalar":
        return g.scalar(src[1], src[2]), ""
    if kind == "vector":
        return g.vector(src[1], src[2]), "RGBA"
    if kind == "tex":
        key = ("t", src[1])
        if key not in g.params:
            g.params[key] = g.node(X.MaterialExpressionTextureObjectParameter, -900, parameter_name=src[1], texture=tex(src[2]),
                                   sampler_type=_sampler(src[3]))
        return g.params[key], ""
    if kind == "sample":
        key = ("p", src[1])
        if key not in g.params:
            e = g.node(X.MaterialExpressionTextureSampleParameter2D, -700, parameter_name=src[1], texture=tex(src[2]),
                       sampler_type=_sampler(src[3]))
            if len(src) > 4:
                g.link(built[src[4]], e, "UVs")
            g.params[key] = e
        return g.params[key], "RGBA"
    if kind == "node":
        return built[src[1]], ""
    if kind == "local":
        key = ("l", src[1])
        if key not in g.params:
            e = g.node(X.MaterialExpressionTransform, -300,
                       transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,
                       transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
            g.link(built[src[1]], e)
            g.params[key] = e
        return g.params[key], ""
    raise RuntimeError("unknown source %r" % (src,))


def build(name, spec):
    sm, bm, mp, ot = unreal.MaterialShadingModel, unreal.BlendMode, unreal.MaterialProperty, unreal.CustomMaterialOutputType
    g = Graph(name, shading={"unlit": sm.MSM_UNLIT, "lit": sm.MSM_DEFAULT_LIT}[spec["shading"]],
              blend={"masked": bm.BLEND_MASKED, "additive": bm.BLEND_ADDITIVE, "translucent": bm.BLEND_TRANSLUCENT}.get(spec.get("blend")),
              two_sided=spec.get("two_sided", False))
    built = {}
    for key, code, typ, inputs in spec["nodes"]:
        ins = []
        for pin, src in inputs:
            e, p = _source(g, built, src)
            ins.append((pin, (e, p)))
        built[key] = g.custom(code, ins, out={F1: ot.CMOT_FLOAT1, F2: ot.CMOT_FLOAT2, F3: ot.CMOT_FLOAT3}[typ])
    props = {"emissive": mp.MP_EMISSIVE_COLOR, "base_color": mp.MP_BASE_COLOR, "roughness": mp.MP_ROUGHNESS, "opacity": mp.MP_OPACITY,
             "opacity_mask": mp.MP_OPACITY_MASK, "metallic": mp.MP_METALLIC, "normal": mp.MP_NORMAL}
    for prop, src in spec["outputs"].items():
        if isinstance(src, str):
            g.out(built[src], props[prop])
        else:
            e, p = _source(g, built, src)
            g.out(e, props[prop], "RGB" if p == "RGBA" else p)
    return g.finish()


def surface_master():
    """The kit's tiling surfaces (M_EU_Surface with a normal map): colour + roughness in one texture, metre UVs / tile."""
    g = Graph("M_SC_Surface")
    uv = g.node(X.MaterialExpressionTextureCoordinate)
    uvs = g.op(X.MaterialExpressionMultiply, uv, g.scalar("TileInv", 0.5))
    t = g.node(X.MaterialExpressionTextureSampleParameter2D, parameter_name="BaseTex", texture=tex("T_SC_paint_white_C"))
    g.link(uvs, t, "UVs")
    n = g.node(X.MaterialExpressionTextureSampleParameter2D, parameter_name="NormalTex", texture=tex("T_SC_paint_white_N"),
               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    g.link(uvs, n, "UVs")
    tint = g.vector("Tint", (1, 1, 1, 1))
    g.out(g.op(X.MaterialExpressionMultiply, t, tint, "RGB", ""), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(t, unreal.MaterialProperty.MP_ROUGHNESS, "A")
    g.out(n, unreal.MaterialProperty.MP_NORMAL, "RGB")
    g.out(g.scalar("Metallic", 0.0), unreal.MaterialProperty.MP_METALLIC)
    return g.finish()


def net_master():
    """A cutout surface without a normal map (the stowage net): colour, its alpha the mask, both sides."""
    g = Graph("M_SC_Net", blend=unreal.BlendMode.BLEND_MASKED, two_sided=True)
    uv = g.node(X.MaterialExpressionTextureCoordinate)
    uvs = g.op(X.MaterialExpressionMultiply, uv, g.scalar("TileInv", 4.0))
    t = g.node(X.MaterialExpressionTextureSampleParameter2D, parameter_name="BaseTex", texture=tex("T_SC_net_C"))
    g.link(uvs, t, "UVs")
    tint = g.vector("Tint", (1, 1, 1, 1))
    g.out(g.op(X.MaterialExpressionMultiply, t, tint, "RGB", ""), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(t, unreal.MaterialProperty.MP_OPACITY_MASK, "A")
    g.out(g.scalar("Roughness", 0.8), unreal.MaterialProperty.MP_ROUGHNESS)
    return g.finish()


def chute_master():
    """The canopy: the ripstop cloth, two-sided, the sun glowing through it (two-sided foliage shading)."""
    g = Graph("M_SC_Chute", shading=unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE, two_sided=True)
    uv = g.node(X.MaterialExpressionTextureCoordinate)
    uvs = g.custom(CHUTE_UV_HLSL, [("UV", uv), ("TileU", g.scalar("TileU", 12.0)), ("TileV", g.scalar("TileV", 3.0))],
                   out=unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    t = g.node(X.MaterialExpressionTextureSampleParameter2D, parameter_name="BaseTex", texture=tex("T_SC_chute_C"))
    g.link(uvs, t, "UVs")
    n = g.node(X.MaterialExpressionTextureSampleParameter2D, parameter_name="NormalTex", texture=tex("T_SC_chute_N"),
               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    g.link(uvs, n, "UVs")
    tint = g.vector("Tint", (1, 1, 1, 1))
    g.out(g.op(X.MaterialExpressionMultiply, t, tint, "RGB", ""), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(t, unreal.MaterialProperty.MP_ROUGHNESS, "A")
    g.out(n, unreal.MaterialProperty.MP_NORMAL, "RGB")
    sss = g.custom(CHUTE_SSS_HLSL, [("A", (t, "RGBA")), ("Tint", (tint, "RGBA")), ("Through", g.scalar("Through", 0.6))])
    g.out(sss, unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    return g.finish()


def instance(name, parent, scalars=None, textures=None, vectors=None):
    path = MAT + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, MAT, unreal.MaterialInstanceConstant,
                                                                unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for k, v in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    for k, v in (textures or {}).items():
        mel.set_material_instance_texture_parameter_value(mi, k, v)
    for k, v in (vectors or {}).items():
        mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*v))
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    return mi


def kit_json():
    if not os.path.isfile(KIT_JSON):
        return {}
    with open(KIT_JSON) as f:
        return json.load(f)


def tiles_m(kit=None):
    """TILE_M with the kit's own sizes over it (kit.json "tiles": {"tx_sc_x": cm})."""
    out = dict(TILE_M)
    for k, v in ((kit if kit is not None else kit_json()).get("tiles") or {}).items():
        if k.startswith("tx_sc_") and k[6:] in out:
            out[k[6:]] = float(v) / 100.0
    return out


def build_materials():
    """All of them: {name: asset}."""
    kit = kit_json()
    mats = {}
    surf = surface_master()
    mats["M_SC_Surface"] = surf
    mats["M_SC_Net"] = net_master()
    mats["M_SC_Chute"] = chute_master()
    for name, tile in tiles_m(kit).items():
        if name in CUTOUT:
            mats["MI_SC_" + name] = instance("MI_SC_" + name, mats["M_SC_Net"], {"TileInv": 1.0 / tile}, {"BaseTex": tex("T_SC_%s_C" % name)})
        else:
            mats["MI_SC_" + name] = instance("MI_SC_" + name, surf, {"TileInv": 1.0 / tile, "Metallic": METAL.get(name, 0.0)},
                                             {"BaseTex": tex("T_SC_%s_C" % name), "NormalTex": tex("T_SC_%s_N" % name)})
    for name, (key, gores, tile_v) in CHUTES.items():
        gores = float((kit.get("chute") or {}).get(key, {}).get("gores", gores))
        mats[name] = instance(name, mats["M_SC_Chute"], {"TileU": gores / 2.0, "TileV": tile_v})
    for name, spec in MATERIALS.items():
        mats[name] = build(name, spec)
        log("material %s" % name)
    # the instruments' cover glasses: the same pane, never frosted or wet, a stronger reflection of the cabin
    mats["MI_SC_GlassCover"] = instance("MI_SC_GlassCover", mats["M_SC_Glass"], {"Reflect": 1.6})
    # the printed plates in each language (the game picks by LyudText::Language(); the lamp board and the CRT take the
    # same atlas as their Atlas / Labels texture parameter on the dynamic instance)
    for lang in LANGS:
        path = LOCAL + "/" + lang + "/T_SC_labels"
        if eal.does_asset_exist(path):
            mats["MI_SC_Labels_" + lang] = instance("MI_SC_Labels_" + lang, mats["M_SC_Labels"], textures={"Atlas": eal.load_asset(path)})
    mats.setdefault("MI_SC_Labels_ru", mats["M_SC_Labels"])
    return mats


def slot_name(slot, mesh=""):
    """The material's name for a slot of a mesh, or None to leave the glb's own. Pure: the lint runs it over kit.json."""
    s = slot.lower()
    s = re.sub(r"\.\d+$", "", s)                       # Blender's .001
    both = mesh.lower() + ":" + s
    if both in SLOTS:
        return SLOTS[both]
    if s in SLOTS:
        return SLOTS[s]
    if s.startswith("tx_sc_"):
        return "MI_SC_" + s[6:] if s[6:] in TILE_M else None
    return None


def slot_material(slot, mats, extra, mesh=""):
    for k in (mesh + ":" + slot, slot):
        for a, b in extra.items():
            if a.lower() == k.lower() and b in mats:
                return mats[b]
    name = slot_name(slot, mesh)
    return mats.get(name) if name else None


def kit_overrides():
    k = kit_json()
    over = k.get("materials") or k.get("slot_materials") or {}
    return {a: b for a, b in over.items() if isinstance(b, str)} if isinstance(over, dict) else {}


def assign(a, mats, extra):
    """The materials onto one mesh's slots by their names; returns the report line and the slots left alone."""
    left = []
    for i, m in enumerate(a.get_editor_property("static_materials")):
        slot = str(m.get_editor_property("material_slot_name"))
        mat = slot_material(slot, mats, extra, a.get_name())
        if mat:
            # (setting the struct in the array copy doesn't stick: set_material does)
            a.set_material(i, mat)
        else:
            left.append(slot)
            mi = m.get_editor_property("material_interface")
            # glTF's metalness is right only on bare metal: the flat small parts stay dielectric
            if isinstance(mi, unreal.MaterialInstanceConstant) and "metal" not in slot.lower():
                for pname in mel.get_scalar_parameter_names(mi):
                    if "etal" in str(pname):
                        mel.set_material_instance_scalar_parameter_value(mi, str(pname), 0.0)
                mel.update_material_instance(mi)
                eal.save_loaded_asset(mi)
    return left


def import_kit(mats):
    extra = kit_overrides()
    if os.environ.get("IMPORT_SPACECRAFT_ONLY") == "materials" or not os.path.isfile(KIT):
        if not os.path.isfile(KIT):
            log("no %s: keeping the imported meshes" % KIT)
        # the rebuilt materials back on the meshes (a deleted and re-made material drops off them)
        n = 0
        for path in (eal.list_assets(MESHES, False) if eal.does_directory_exist(MESHES) else []):
            a = eal.load_asset(path)
            if isinstance(a, unreal.StaticMesh):
                assign(a, mats, extra)
                eal.save_loaded_asset(a)
                n += 1
        log("materials back on %d meshes" % n)
        return
    tmp = MESHES + "/spacecraft_kit"
    if eal.does_directory_exist(tmp):
        eal.delete_directory(tmp)
    src = unreal.InterchangeManager.create_source_data(KIT)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_scene(MESHES, src, params)
    mgr.wait_until_all_tasks_done(True)
    n = tris = 0
    unassigned = set()
    for path in eal.list_assets(tmp, True):
        a = eal.load_asset(path)
        if not isinstance(a, unreal.StaticMesh):
            continue
        name = a.get_name()
        dest = MESHES + "/" + name
        if eal.does_asset_exist(dest):
            eal.delete_asset(dest)
        eal.rename_asset(path, dest)
        a = eal.load_asset(dest)
        s = a.get_editor_property("nanite_settings")
        s.enabled = False
        a.set_editor_property("nanite_settings", s)
        left = assign(a, mats, extra)
        unassigned.update(left)
        eal.save_loaded_asset(a)
        tris += a.get_num_triangles(0)
        log("mesh %s: %d triangles, slots %s" % (name, a.get_num_triangles(0), ["%s=%s" % (m.get_editor_property("material_slot_name"), m.get_editor_property("material_interface").get_name()) for m in a.get_editor_property("static_materials")]))
        n += 1
    eal.save_directory(ROOT)
    # what Interchange left in the level (the scene's actors)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for act in eas.get_all_level_actors():
        if act.get_name().startswith("SM_SC_") or "spacecraft_kit" in str(act.get_folder_path()):
            eas.destroy_actor(act)
    log("kit: %d meshes, %d triangles into %s" % (n, tris, MESHES))
    if unassigned:
        log("slots left with the glb's own flat materials: %s" % ", ".join(sorted(unassigned)))


def record_asset(filename):
    """audio/music/spacecraft/<id>.wav -> its asset's name: MU_SC_<id without record_sc_ / sc_>."""
    base = os.path.splitext(os.path.basename(filename))[0]
    for pre in ("record_sc_", "sc_"):
        if base.startswith(pre):
            base = base[len(pre):]
            break
    return "MU_SC_" + base


def import_records():
    """The fetched records (tools/spacecraft/fetch_records.py -> audio/music/spacecraft/*.wav; import_audio.py does not
    look into that folder) -> /Game/Audio/Music/MU_SC_<id>, streamed from disk. None fetched: none imported (the game
    falls back to its own cues, RECORDS_NOTES.md)."""
    files = sorted(f for f in (os.listdir(RECORDS_SRC) if os.path.isdir(RECORDS_SRC) else []) if f.endswith(".wav"))
    tasks = []
    for f in files:
        t = unreal.AssetImportTask()
        t.filename = os.path.join(RECORDS_SRC, f)
        t.destination_path = "/Game/Audio/Music"
        t.destination_name = record_asset(f)
        t.automated = True
        t.replace_existing = True
        t.save = True
        tasks.append(t)
    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for f in files:
        path = "/Game/Audio/Music/" + record_asset(f)
        w = eal.load_asset(path)
        if isinstance(w, unreal.SoundWave):
            try:
                w.set_editor_property("loading_behavior", unreal.SoundWaveLoadingBehavior.LOAD_ON_DEMAND)
                eal.save_asset(path)
            except Exception as e:  # noqa: BLE001 (the wave is in, only not streamed)
                log("could not set streaming on %s: %s" % (path, e))
            log("record %s" % path)
        else:
            log("record %s did not import" % f)
    return len(files)


def main():
    log("textures: %d" % import_textures())
    mats = build_materials()
    log("materials: %d" % len(mats))
    import_kit(mats)
    log("records: %d" % import_records())
    eal.save_directory(ROOT)
    log("done")


# =====================================================================================================================
# The lint (outside Unreal)

HLSL_WORDS = set("""float float2 float3 float4 int static const return for unroll abs acos asin atan2 clamp cos cross ddx ddy degrees dot
exp exp2 floor fmod frac fwidth length lerp log2 max min normalize pow radians round saturate sin smoothstep sqrt step
SampleLevel""".split())


def strip_comments(code):
    return re.sub(r"//[^\n]*", "", code)


def lint_node(mat, key, code, typ, inputs):
    """Problems of one custom node, as strings."""
    bad = []
    src = strip_comments(code)
    for a, b in ("()", "{}", "[]"):
        if src.count(a) != src.count(b):
            bad.append("unbalanced %s%s" % (a, b))
    if re.search(r"\bif\b|\belse\b|\?|\bwhile\b|[<>!=]=|<|>|&&|\|\|", re.sub(r"for \(int \w+ = \d+; \w+ < \d+; \w+\+\+\)", "", src)):
        bad.append("outside the subset (if / ?: / comparison)")
    if len(re.findall(r"\breturn\b", src)) != 1 or not src.strip().endswith(";"):
        bad.append("wants exactly one return, last")
    names = [n for n, _ in inputs]
    if len(set(names)) != len(names):
        bad.append("an input twice")
    declared = set(names)
    for n, s in inputs:
        if isinstance(s, tuple) and s[0] == "tex":
            declared.add(n + "Sampler")
    for m in re.finditer(r"\b(?:float[234]?|int)\s+(\w+)\s*(?:\[\d+\])?\s*=", src):
        if m.group(1) in declared:
            bad.append("%s declared twice" % m.group(1))
        declared.add(m.group(1))
    body = re.sub(r"\.[A-Za-z_]\w*", "", src)                       # swizzles and methods
    body = re.sub(r"\b\d+\.?\d*(?:e[-+]?\d+)?\b", " ", body)
    used = set(re.findall(r"[A-Za-z_]\w*", body))
    for n in sorted(used - declared - HLSL_WORDS):
        bad.append("undeclared %s" % n)
    whole = set(re.findall(r"[A-Za-z_]\w*", src))
    for n in names:
        if n not in whole:
            bad.append("input %s unused" % n)
    for n, s in inputs:
        if isinstance(s, tuple) and s[0] == "tex" and (n + "Sampler") not in whole:
            bad.append("texture %s never sampled" % n)
    if re.search(r"(?<![\w.])\d+(?![\w.])", re.sub(r"\[\d+\]|int \w+ = \d+; \w+ < \d+|\d+\.\d*e[-+]?\d+", "", src)):
        bad.append("a bare integer literal (write 1.0)")
    return ["%s.%s: %s" % (mat, key, b) for b in bad]


def lint():
    bad = []
    count = 0
    for mat, spec in MATERIALS.items():
        keys = set()
        params = {}
        for key, code, typ, inputs in spec["nodes"]:
            count += 1
            bad += lint_node(mat, key, code, typ, inputs)
            for n, s in inputs:
                if isinstance(s, tuple) and s[0] in ("node", "local") and s[1] not in keys:
                    bad.append("%s.%s: %s reads node %s before it exists" % (mat, key, n, s[1]))
                if isinstance(s, tuple) and s[0] in ("scalar", "vector", "tex", "sample"):
                    if params.setdefault(s[1], s[:4]) != s[:4]:
                        bad.append("%s: parameter %s declared two ways" % (mat, s[1]))
            keys.add(key)
        for prop, src in spec["outputs"].items():
            if isinstance(src, str) and src not in keys:
                bad.append("%s: output %s reads a missing node" % (mat, prop))
    bad += lint_node("M_SC_Chute", "sss", CHUTE_SSS_HLSL, F3, [("A", 0), ("Tint", 0), ("Through", 0)])
    bad += lint_node("M_SC_Chute", "uv", CHUTE_UV_HLSL, F2, [("UV", 0), ("TileU", 0), ("TileV", 0)])
    # constants that mirror other tools
    if os.path.isfile(PINS):
        with open(PINS) as f:
            pins = json.load(f)
        want = dict(pins.get("tile_m") or {})
        want.pop("chute", None)
        if want != tiles_m() or pins.get("metal") != METAL:
            bad.append("the tiles / METAL differ from tools/spacecraft/textures.json: %s / %s" % (want, tiles_m()))
        isl = pins.get("island", {}).get("planet_texture_island")
        if isl and (isl["lat"], isl["lon"], isl["size_deg"]) != EU_ISLAND:
            bad.append("EU_ISLAND differs from the planet texture's island (textures.json)")
        for spec in MATERIALS.values():
            for _, _, _, inputs in spec["nodes"]:
                for _, s in inputs:
                    if isinstance(s, tuple) and s[0] in ("tex", "sample") and not s[2].startswith("/"):
                        have = [k for k in os.listdir(TEX_SRC) if k.startswith(s[2])] if os.path.isdir(TEX_SRC) else []
                        if not have and s[2] not in FALLBACK:
                            bad.append("texture %s is not in textures/spacecraft" % s[2])
    bad += lint_kit()
    return count, bad


def lint_kit():
    """The kit's own word (Content/Spacecraft/kit.json) against what the shaders and the slot table assume."""
    kit = kit_json()
    if not kit:
        return []
    bad = []
    names = set(MATERIALS) | {"MI_SC_" + n for n in TILE_M} | set(CHUTES) | {"MI_SC_GlassCover", "MI_SC_Labels_ru"}
    for v in SLOTS.values():
        if v not in names:
            bad.append("slot table names %s, which nothing builds" % v)
    for mesh, piece in (kit.get("pieces") or {}).items():
        for slot in piece.get("slots", []):
            name = slot_name(slot, mesh)
            if name is None and slot.lower() not in FLAT_SLOTS:
                bad.append("kit: %s's slot %s has no material here" % (mesh, slot))
    panel = kit.get("panel") or {}
    dg = panel.get("digits") or {}
    if dg:
        wins = tuple((w["id"], w["first_index"], w["digits"]) for w in dg.get("windows", []))
        if dg.get("count") != DIGIT_COUNT or wins != tuple(w[:3] for w in DIGIT_WINDOWS):
            bad.append("kit: the digit windows moved: %s" % (wins,))
        cw, ch = dg.get("cell_size", (1.7, 2.8))
        if abs(cw / ch - DIGIT_ASPECT) > 0.01:
            bad.append("kit: a digit cell is %.3f wide for its height, the shader draws %.3f" % (cw / ch, DIGIT_ASPECT))
    lm = panel.get("lamps") or {}
    if lm:
        if (lm.get("cols"), lm.get("rows")) != LAMP_GRID:
            bad.append("kit: the lamp board is %s x %s" % (lm.get("cols"), lm.get("rows")))
        for c in lm.get("cells", []):
            u, v = c["data_uv"]
            if int(v * LAMP_GRID[1]) * LAMP_GRID[0] + int(u * LAMP_GRID[0]) != c["index"]:
                bad.append("kit: lamp %s's data UV does not give its index" % c["id"])
    pl = kit.get("player") or {}
    if pl and (pl.get("record_radius"), pl.get("label_radius")) != (RECORD_R, LABEL_R):
        bad.append("kit: the record is %s / %s cm" % (pl.get("record_radius"), pl.get("label_radius")))
    crt = panel.get("crt") or {}
    if crt and abs(crt["size"][0] / crt["size"][1] - CRT_ASPECT) > 0.01:
        bad.append("kit: the CRT is %s cm" % (crt["size"],))
    if os.path.isfile(FLIGHT_MODEL):
        with open(FLIGHT_MODEL) as f:
            fm = f.read()
        for want in ("const double EX = 0.14, EY = 0.26, PX = 0.68, PY = 0.56, Bend = 600.0 / 900.0;",
                     "const double R0 = 0.30, R1 = 0.165, A0 = Rad(205.0), A1 = Rad(20.0);", "X = Lerp(EX + 0.045, SX, U);",
                     "Y = Lerp(EY + 0.02, SY, U) + 0.05 * std::sin(U * Pi) * (1.0 - U);", "I.CrtEuropaR = 0.05;", "I.CrtPlanetR = 0.16;"):
            if want not in fm:
                bad.append("FlightModel.h's CrtPath changed (no longer has: %s): CRT_PATH / CRT_BODIES mirror it" % want)
    for name, (key, gores, _) in CHUTES.items():
        if (kit.get("chute") or {}).get(key, {}).get("gores", gores) % 2:
            bad.append("kit: %s has an odd number of gores: the two-gore tile will not close" % key)
    return bad


if __name__ == "__main__":
    if unreal:
        main()
    else:
        n, problems = lint()
        for p in problems:
            print("LINT", p)
        try:
            sys.path.insert(0, os.path.join(REPO, "tools", "spacecraft"))
            import material_preview
            problems += material_preview.smoke(sys.modules[__name__])
        except ImportError as e:
            print("(not executed: %s)" % e)
        print("SPACECRAFT lint: %d nodes in %d materials, %d problems" % (n, len(MATERIALS), len(problems)))
        sys.exit(1 if problems else 0)
