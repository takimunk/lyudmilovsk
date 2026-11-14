"""HLSL for Topi's materials (unreal/scripts/make_topi_materials.py builds them; docs/topi.md).

Parameters (ALyudTopi sets them on its dynamic materials):
  TopiLook on M_TopiSand                 = (the hero's unrest 0..1, night 0..1, dissolve 0..1, heave scale)
  TopiLook on M_TopiRiver / Glow / Motes = (night 0..1, dissolve 0..1, unrest 0..1, 0)
and from /Game/Weather/MPC_LyudWeather: Wind = (dir x, dir y, speed m/s, gustiness), WindDrift = (drift x m, drift y m,
gust clock s, day stir).

Vertex colours of the sand (LyudTopi.cpp BuildChunk): r = marram density, g = the river's bed, a = how much of Topi is
here (0 on the skirt hidden under the map's own ground).
"""

# Pasted at the head of a Custom node's code; called as Topi.Heave(...) etc.
LIB = r"""
struct FLyudTopiFn
{
    float Hash(int2 p, uint s)
    {
        uint2 q = uint2(p + 1048576);
        uint h = q.x * 0x8da6b343u ^ q.y * 0xd8163841u ^ s * 0x165667b1u;
        h = (h ^ (h >> 15)) * 0x2c1b3c6du;
        h = (h ^ (h >> 12)) * 0x297a2d39u;
        h ^= h >> 15;
        return float(h >> 8) * (1.0 / 16777215.0);
    }
    // value noise 0..1
    float Noise(float2 x, uint s)
    {
        float2 f = floor(x);
        float2 u = x - f;
        u = u * u * u * (u * (u * 6.0 - 15.0) + 10.0);
        int2 i = int2(f);
        float a = Hash(i, s), b = Hash(i + int2(1, 0), s), c = Hash(i + int2(0, 1), s), d = Hash(i + int2(1, 1), s);
        return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
    }
    // One long swell of sand: p in metres, d its direction, len its length m, w its speed rad/s. xy = slope, z = height (for amplitude 1).
    float3 Swell(float2 p, float2 d, float len, float w, float t)
    {
        float k = 6.2831853 / len;
        float2 side = float2(d.y, -d.x);
        float bend = dot(p, side) * k * 0.37 + t * w * 0.31;
        float ph = dot(p, d) * k + t * w + 1.7 * sin(bend);
        float2 g = cos(ph) * (d * k + 1.7 * cos(bend) * side * k * 0.37);
        return float3(g, sin(ph));
    }
    // The far dunes' slow heave: three swells crossing, drifting down the wind. xy = slope, z = height in metres.
    float3 Heave(float2 p, float t)
    {
        float3 r = 9.0 * Swell(p, float2(-0.97, 0.24), 610.0, 0.105, t);
        r += 6.0 * Swell(p, float2(-0.86, -0.51), 380.0, -0.083, t);
        r += 3.5 * Swell(p, float2(-0.995, -0.10), 230.0, 0.151, t);
        return r;
    }
    // How much of the heave a point at dist metres from the eye gets: none near (the ground there is real: it has
    // collision), all of it far off, more with distance (dunes that seem to grow toward the horizon), and nearer
    // and stronger as the hero's mind gives way. look = TopiLook, vc = vertex colour.
    float HeaveAmount(float dist, float4 look, float4 vc)
    {
        float k = smoothstep(450.0, 2000.0, dist) * (1.0 + dist / 5000.0) * (0.45 + 0.9 * look.x);
        k += look.x * look.x * smoothstep(70.0, 450.0, dist) * 0.4;
        return k * look.w * vc.a * (1.0 - vc.g);
    }
} Topi;
"""

# World position offset of the sand. Pos, Cam: world cm; T: seconds; Look: TopiLook; VC: vertex colour.
SAND_WPO = r"""
float2 p = Pos.xy * 0.01;
float dist = length(Pos.xy - Cam.xy) * 0.01;
float k = Topi.HeaveAmount(dist, Look, VC);
return float3(0.0, 0.0, Topi.Heave(p, T).z * k * 100.0);
"""

# The sand's normal (world space). N: the vertex normal; Pos: world cm without offsets; Wind: MPC Wind.
SAND_NORMAL = r"""
float2 p = Pos.xy * 0.01;
float dist = length(Pos.xy - Cam.xy) * 0.01;
float3 n = normalize(N);
// the heave tilts the far ground
float k = Topi.HeaveAmount(dist, Look, VC);
float2 slope = Topi.Heave(p, T).xy * k;
// wind ripples: crests across the wind, ~11 cm apart, and broader ones ~70 cm; each fades out where it is too
// small for the pixel
float2 w = normalize(Wind.xy + float2(1e-4, 0.0));
float foot = max(length(ddx(p)), length(ddy(p)));
float along = dot(p, w);
float warp = Topi.Noise(p * 0.55, 3u) * 5.0 + Topi.Noise(p * 0.11, 4u) * 9.0;
float level = saturate((n.z - 0.80) / 0.12);                    // none on the slip faces: the sand slides there
float fine = saturate(1.0 - foot / 0.035) * level * (1.0 - VC.g);
float broad = saturate(1.0 - foot / 0.25) * level * (1.0 - VC.g);
float ph1 = along * 57.0 + warp * 6.2831853;
float ph2 = along * 9.0 + warp * 2.3;
// a ripple's lee side is steep: skew the sine
float r1 = cos(ph1 + 0.6 * sin(ph1));
float r2 = cos(ph2 + 0.5 * sin(ph2));
slope += w * (0.30 * r1 * fine + 0.16 * r2 * broad);
// grains: a little roughness of the surface up close
float grain = saturate(1.0 - foot / 0.01);
slope += (float2(Topi.Noise(p * 310.0, 5u), Topi.Noise(p * 310.0, 6u)) - 0.5) * 0.25 * grain;
return normalize(float3(n.xy - slope * n.z, n.z));
"""

# The sand's colour (rgb) and roughness (a).
SAND_COLOR = r"""
float2 p = Pos.xy * 0.01;
float3 n = normalize(N);
float foot = max(length(ddx(p)), length(ddy(p)));
float2 w = normalize(Wind.xy + float2(1e-4, 0.0));
// pale Baltic quartz sand, a little warmer and darker in broad patches (heavier minerals gather in the hollows)
float tone = Topi.Noise(p / 47.0, 7u) * 0.6 + Topi.Noise(p / 9.0, 8u) * 0.4;
float3 c = lerp(float3(0.50, 0.48, 0.42), float3(0.64, 0.62, 0.55), tone);
// the slip faces are finer, smoother sand: lighter
float slip = saturate((0.86 - n.z) / 0.10);
c = lerp(c, float3(0.68, 0.66, 0.59), 0.55 * slip);
// dark streaks of garnet and magnetite along the ripples, here and there
float streak = Topi.Noise(float2(dot(p, w) * 0.6, dot(p, float2(w.y, -w.x)) * 0.05), 9u);
// broken up so they never run on as continuous lines
float patchy = smoothstep(0.45, 0.75, Topi.Noise(p / 13.0, 14u));
c *= 1.0 - 0.12 * smoothstep(0.66, 0.88, streak) * patchy * saturate(1.0 - foot / 0.6) * (1.0 - slip);
// marram country: grey litter and roots darken the sand between the tufts
float litter = VC.r * (0.35 + 0.65 * Topi.Noise(p / 2.3, 10u));
c = lerp(c, float3(0.30, 0.29, 0.20), 0.5 * litter);
// the river's bed: by day a pale dry crust, at night wet under the light
float bed = VC.g;
c = lerp(c, lerp(float3(0.66, 0.63, 0.55), float3(0.20, 0.24, 0.22), Look.y), bed);
float rough = lerp(0.92, 0.35, bed * Look.y);
// sand streaming over the surface in a fresh wind: long pale ribbons running down the wind, snaking, fading in and out
float blow = saturate((Wind.z - 4.0) / 8.0) * (0.5 + 0.5 * Wind.w) + 0.25 * Look.x;
float2 q = float2(dot(p, w) * 0.035 - T * (0.25 + 0.03 * Wind.z), dot(p, float2(w.y, -w.x)) * 0.5 + 1.5 * Topi.Noise(p * 0.04 - T * 0.1, 11u));
float ribbon = smoothstep(0.55, 0.9, Topi.Noise(q, 12u)) * smoothstep(0.3, 0.7, Topi.Noise(p * 0.02 + T * 0.03, 13u));
float stream = ribbon * blow * saturate(1.0 - foot / 1.5) * (1.0 - bed) * saturate((n.z - 0.7) / 0.2);
c = lerp(c, float3(0.76, 0.73, 0.64), 0.6 * stream);
// seen from far off the dunes go paler and lose their warmth: they could be made of light
float dist = length(Pos.xy - Cam.xy) * 0.01;
float farK = saturate(dist / 12000.0);
c = lerp(c, float3(0.70, 0.68, 0.63), 0.5 * farK);
return float4(c, rough);
"""

# The river: particles of light in a smooth fast stream. UV = (metres along, -1..1 across); returns rgb = light, a = opacity.
RIVER = r"""
float night = Look.x, dissolve = Look.y;
float across = UV.y;
float edge = smoothstep(1.0, 0.72, abs(across));
// the stream is fastest in the middle (TopiModel RiverFlow)
float speed = 2.4 * (1.0 - 0.75 * across * across);
float3 light = 0.0;
float cover = 0.0;
// the body: a soft band of teal, with long threads of brighter light drawn out by the flow
float thread = Topi.Noise(float2(UV.x * 0.06 - T * speed * 0.06, across * 3.5), 21u);
float thread2 = Topi.Noise(float2(UV.x * 0.21 - T * speed * 0.21, across * 9.0 + 3.0), 22u);
// low contrast: at a grazing angle long bright threads smear into scan lines
float body = 0.45 + 0.25 * smoothstep(0.35, 0.8, thread) + 0.15 * smoothstep(0.5, 0.9, thread2);
// how steeply the eye looks at the water (0 grazing .. 1 straight down): fine sparks squash into lines when grazing
float graze = abs(V.z);
light += float3(0.035, 0.30, 0.33) * body;   // a dim sea-green body (the sparks carry the colour, not a turquoise slab)
cover += 0.5 * body;
// the particles: three sizes, each cell of a grid moving with the stream holds one spark or none
for (int layer = 0; layer < 3; ++layer)
{
    float cells = layer == 0 ? 0.9 : (layer == 1 ? 2.6 : 6.5);          // cells per metre
    float2 g = float2(UV.x - T * speed * (1.0 - 0.12 * layer), across * 21.5) * cells;
    int2 id = int2(floor(g));
    float2 f = g - floor(g);
    uint s = 30u + uint(layer) * 7u;
    float has = Topi.Hash(id, s);
    float2 at = 0.25 + 0.5 * float2(Topi.Hash(id, s + 1u), Topi.Hash(id, s + 2u));
    float twinkle = 0.55 + 0.45 * sin(T * (1.5 + 4.0 * Topi.Hash(id, s + 3u)) + 6.2831853 * Topi.Hash(id, s + 4u));
    float2 d = (f - at) * float2(1.0, 1.0);
    float r = lerp(0.10, 0.22, Topi.Hash(id, s + 5u)) * (layer == 0 ? lerp(1.5, 1.0, smoothstep(0.05, 0.3, graze)) : 1.0);
    float spark = exp(-dot(d, d) / (r * r)) * step(0.45, has) * twinkle;
    // each spark has its own colour between teal, aquamarine, pale violet and a rare gold: they shift as they go
    float hue = Topi.Hash(id, s + 6u) + 0.08 * sin(T * 0.7 + UV.x * 0.05);
    float3 col = lerp(float3(0.10, 1.00, 0.85), float3(0.35, 0.75, 1.00), smoothstep(0.2, 0.6, hue));
    col = lerp(col, float3(0.80, 0.60, 1.00), smoothstep(0.6, 0.85, hue));
    col = lerp(col, float3(1.00, 0.90, 0.55), smoothstep(0.92, 1.0, hue));
    float fade = layer == 2 ? saturate(1.0 - Foot / 0.25) * smoothstep(0.08, 0.3, graze)
               : (layer == 1 ? saturate(1.0 - Foot / 0.8) * smoothstep(0.03, 0.15, graze) : 1.0);
    light += col * spark * (layer == 0 ? 2.0 : (layer == 1 ? 3.0 : 4.0)) * fade;
    cover += spark * fade;
}
float k = night * VC.a * edge;
light *= k * Bright * (1.0 + 7.0 * dissolve);
return float4(light, saturate(cover * k * (0.75 + dissolve)));
"""

# The glow standing over the river, seen from far off. UV = (metres along, 0 foot .. 1 top); returns the light.
GLOW = r"""
float night = Look.x, dissolve = Look.y;
float up = UV.y;
// a calm curtain: broad, slow, low-contrast folds rather than streaks
float rays = 0.78 + 0.22 * Topi.Noise(float2(UV.x / 240.0 - T * 0.006, up * 1.2 + T * 0.008), 41u);
float shimmer = 0.9 + 0.1 * Topi.Noise(float2(UV.x / 45.0 + T * 0.03, up * 3.0 - T * 0.05), 42u);
float fall = pow(1.0 - up, 2.6) * smoothstep(0.0, 0.03, up) * lerp(1.0, 0.5, up);
// strongest where the path comes out (UV.x = 0): ~30 degrees of the horizon seen from the town, fainter along the rest
float focus = 0.3 + 0.7 * exp(-(UV.x * UV.x) / (520.0 * 520.0));
// near the river the curtain goes: there the river itself is the light
float nearK = smoothstep(120.0, 700.0, Dist * 0.01);
float3 col = lerp(float3(0.12, 0.55, 0.52), float3(0.30, 0.45, 0.85), up * 1.4);
return col * fall * rays * shimmer * focus * nearK * night * VC.a * Bright * (1.0 + 2.0 * dissolve);
"""

# A mote of light: the quad's corner offset. Pos: the mote's place (world cm, all four corners); UV: its corner
# (-1..1); VC: (speed, phase, size, kind 0 wind / 1 riser); Right, Up: the camera's axes in the world.
MOTE_WPO = r"""
float night = Look.x, dissolve = Look.y;
float rate = 0.035 + 0.05 * VC.r;
float life = frac(T * rate + VC.g);
float2 w = normalize(Wind.xy + float2(1e-4, 0.0));
// kind (VC.a): 0 blown down the wind from the river (the path leads up the wind's trail), 1 rising from the water,
// 0.5 a spark streaming on the water with the current (Flow: the stream's direction, in the vertex normal)
float riser = step(0.75, VC.a);
float streamer = step(0.25, VC.a) * (1.0 - riser);
float3 wind = float3(w * (life - 0.5) * (500.0 + 90.0 * Wind.z), 35.0 * sin(life * 6.2831853 + VC.b * 20.0));
float3 rise = float3(260.0 * (life - 0.5) + 30.0 * sin(life * 9.0 + VC.g * 30.0), 25.0 * sin(life * 7.0 + VC.r * 40.0), 170.0 * life);
// 240 cm/s: the stream's speed (TopiModel RiverFlow)
float3 flow = float3(normalize(Flow.xy + float2(1e-4, 0.0)) * (life - 0.5) * 240.0 / rate, 4.0 * sin(life * 40.0 + VC.g * 30.0));
float3 drift = riser > 0.5 ? rise : (streamer > 0.5 ? flow : wind);
// mostly sparks of 3..6 cm, one in ten a larger soft one; the streaming ones tiny
float big = step(0.9, VC.b) * (1.0 - streamer);
float size = lerp(lerp(1.5 + 1.5 * VC.b / 0.9, 6.0, big), 0.8 + 1.4 * VC.b, streamer) * sin(3.14159 * life) * night * (1.0 + 2.5 * dissolve);
return drift + (UV.x * Right + UV.y * Up) * size;
"""

MOTE_LIGHT = r"""
float night = Look.x, dissolve = Look.y;
float d = saturate(1.0 - dot(UV, UV));
float riser = step(0.75, VC.a);
float streamer = step(0.25, VC.a) * (1.0 - riser);
float big = step(0.9, VC.b) * (1.0 - streamer);
d = lerp(d * d * d, d * d * 0.35, big);       // sparks: a sharp point; the big ones soft and dim
float tw = 0.6 + 0.4 * sin(T * (2.0 + 5.0 * VC.r) + VC.g * 40.0);
float3 col = lerp(float3(0.15, 1.0, 0.85), float3(0.85, 0.95, 1.0), VC.b * VC.b);
// the streaming sparks take the river's colours: teal, aquamarine, pale violet, a rare gold
float hue = frac(VC.r * 7.13 + VC.g * 3.71);
float3 sc = lerp(float3(0.10, 1.00, 0.85), float3(0.35, 0.75, 1.00), smoothstep(0.2, 0.6, hue));
sc = lerp(sc, float3(0.80, 0.60, 1.00), smoothstep(0.6, 0.85, hue));
sc = lerp(sc, float3(1.00, 0.90, 0.55), smoothstep(0.95, 1.0, hue));
col = lerp(col, sc * 1.6, streamer);
return col * d * tw * night * Bright * (1.0 + 3.0 * dissolve);
"""
