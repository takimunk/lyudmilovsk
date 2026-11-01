"""Europa's ground and sky into the project (docs/europa.md "The ground", "The sky from Europa"):
design/europa/game/moon_terrain.glb (tools/europa/moon_terrain.py) -> /Game/Europa/Moon/SM_EU_*, and the materials
M_EU_Terrain (the ground: triplanar ice, the bake's vertex colours), M_EU_Space (the star field round the site) and
M_EU_PlanetAir (the planet's air: a shell with a glowing limb). Run headless (unreal/scripts/import_moon.sh):

  UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/import_moon.py

The ground's tiles, the skyline and the boulders are Nanite; complex collision from the fallback mesh (its share per
level below: the near tiles fine, the far ones coarser), none on the skyline. The leads' water stays a classic mesh (the
sea's single-layer water material is put on it in the game). Set IMPORT_MOON_ONLY=materials to rebuild the materials only.
"""
import json
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
os.environ["EUROPA_AS_MODULE"] = "1"
import import_europa as E  # noqa: E402

REPO = E.REPO
GLB = os.path.join(REPO, "design", "europa", "game", "moon_terrain.glb")
MOON_JSON = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "Europa", "moon.json")
DEST = E.ROOT + "/Moon"
X = unreal
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
# the fallback mesh's share of the triangles per tile level (what complex collision is cooked from)
FALLBACK = {0: 0.35, 1: 0.5, 2: 0.5}


def log(msg):
    unreal.log_warning("MOON " + msg)


def nanite_usage(m):
    try:
        mel.set_material_usage(m, unreal.MaterialUsage.MATUSAGE_NANITE)
    except Exception:
        try:
            m.set_editor_property("used_with_nanite", True)
        except Exception as e:
            log("no Nanite usage on %s (%r)" % (m.get_name(), e))


TERRAIN_HLSL = r"""
// the site's frame in metres (every tile sits at the site's origin)
float3 p = (WP - OP) * 0.01;
float3 n = normalize(VN);
float3 w = pow(abs(n), 6.0); w /= (w.x + w.y + w.z);
float4 m = float4(VC.rgb, VA);
// two scales of the ice texture on the three planes, the far one breaking up the near one's repeats
float s1 = 1.0 / 3.5, s2 = 1.0 / 29.0;
float4 a1 = Ice.Sample(IceSampler, p.yz * s1) * w.x + Ice.Sample(IceSampler, p.xz * s1) * w.y + Ice.Sample(IceSampler, p.xy * s1) * w.z;
float4 a2 = Ice.Sample(IceSampler, p.yz * s2 + 0.37) * w.x + Ice.Sample(IceSampler, p.xz * s2 + 0.37) * w.y + Ice.Sample(IceSampler, p.xy * s2 + 0.37) * w.z;
float3 nx = Nrm.Sample(NrmSampler, p.yz * s1).xyz * 2 - 1;
float3 ny = Nrm.Sample(NrmSampler, p.xz * s1).xyz * 2 - 1;
float3 nz = Nrm.Sample(NrmSampler, p.xy * s1).xyz * 2 - 1;
float grain = dot(a1.rgb, 0.333);
float patch = dot(a2.rgb, 0.333);
// cliffs: the slope and the bake's fresh exposure; strata in the walls (layered ice), wavering
float cliff = saturate(max(smoothstep(0.8, 0.45, n.z), m.g * 0.85));
float strata = 0.5 + 0.5 * sin(p.z * 3.7 + 2.0 * sin(p.x * 0.05 + p.y * 0.04) + patch * 4.0);
strata = lerp(strata, 0.5 + 0.5 * sin(p.z * 11.0 + patch * 9.0), 0.35);
float3 frost = float3(0.80, 0.82, 0.85);
float3 plains = float3(0.70, 0.69, 0.68);
float3 salts = float3(0.46, 0.31, 0.21);
float3 young = float3(0.50, 0.64, 0.74);
float brown = saturate(m.r * (0.7 + 0.8 * patch));
float3 col = lerp(plains, frost, saturate(m.b * 1.4 + smoothstep(0.9, 1.0, n.z) * 0.2));
col = lerp(col, salts, brown);
col = lerp(col, young * lerp(0.82, 1.08, strata), cliff);
col *= lerp(0.7, 1.18, grain) * lerp(0.86, 1.1, patch);
col *= 1.0 - 0.45 * saturate(m.a);
Rough = lerp(lerp(0.78, 0.62, brown), 0.32 + 0.15 * strata, cliff);
// the detail normal on top of the ground's own (world space)
float k = Bump * lerp(1.0, 1.6, cliff);
float3 d = w.x * float3(0, nx.x, nx.y) + w.y * float3(ny.x, 0, ny.y) + w.z * float3(nz.x, nz.y, 0);
Normal = normalize(n + d * k);
return col * Albedo;
"""

SPACE_HLSL = r"""
// the pixel's direction in the sphere's frame = the equator of the date (the import mirrors Y)
float3 d = normalize(L);
d.y = -d.y;
float3 a = abs(d);
float2 uv; float face;
if (a.x >= a.y && a.x >= a.z) { uv = d.yz / a.x; face = d.x > 0 ? 0 : 1; }
else if (a.y >= a.z) { uv = d.xz / a.y; face = d.y > 0 ? 2 : 3; }
else { uv = d.xy / a.z; face = d.z > 0 ? 4 : 5; }
// how much sky a pixel covers (radians): a star is never drawn smaller, only fainter
float pix = max(length(fwidth(d)), 1e-5);
// the Milky Way: a band along the galactic equator, a bulge toward its centre, dark lanes
float3 gp = float3(-0.8675, -0.1981, 0.4560);
float3 gc = float3(-0.0549, -0.8734, -0.4839);
float gb = asin(clamp(dot(d, gp), -1, 1));
float lanes = 0.5 + 0.5 * sin(dot(d, float3(37.0, 23.0, 29.0)) + 3.0 * sin(dot(d, float3(11.0, -17.0, 7.0))));
float band = exp(-gb * gb / (0.16 * 0.16));
float bulge = exp(-pow(acos(clamp(dot(d, gc), -1, 1)) / 0.5, 2.0));
float dens = 1.0 + 3.0 * band + 4.0 * bulge;
float3 col = float3(0.55, 0.6, 0.75) * (band * 0.0035 + bulge * 0.006) * lerp(0.35, 1.0, lanes);
[unroll] for (int layer = 0; layer < 3; layer++)
{
    float N = layer == 0 ? 24.0 : (layer == 1 ? 90.0 : 300.0);
    float2 g = (uv * 0.5 + 0.5) * N;
    float2 c = floor(g);
    // pcg hash of the cell
    uint h = uint(c.x) + uint(c.y) * 4099u + uint(face) * 16785407u + uint(layer) * 805306457u;
    h = h * 747796405u + 2891336453u; h = ((h >> ((h >> 28u) + 4u)) ^ h) * 277803737u; h = (h >> 22u) ^ h;
    uint h2 = h * 747796405u + 2891336453u; h2 = ((h2 >> ((h2 >> 28u) + 4u)) ^ h2) * 277803737u; h2 = (h2 >> 22u) ^ h2;
    float r1 = float(h & 65535u) / 65535.0, r2 = float(h >> 16) / 65535.0;
    float r3 = float(h2 & 65535u) / 65535.0, r4 = float(h2 >> 16) / 65535.0;
    float keep = layer == 0 ? 0.5 : (layer == 1 ? 0.18 * dens : 0.06 * dens);
    if (r1 < keep)
    {
        // the star's own direction on the cube, and the true angle to it
        float2 su = ((c + 0.2 + 0.6 * float2(r2, r3)) / N) * 2.0 - 1.0;
        float3 sd = face < 0.5 ? float3(1, su.x, su.y) : face < 1.5 ? float3(-1, su.x, su.y) :
                    face < 2.5 ? float3(su.x, 1, su.y) : face < 3.5 ? float3(su.x, -1, su.y) :
                    face < 4.5 ? float3(su.x, su.y, 1) : float3(su.x, su.y, -1);
        float dist = acos(clamp(dot(d, normalize(sd)), -1.0, 1.0));
        // a few bright stars, many faint ones (flux: what the pixel gets when the star sits in it)
        float flux = layer == 0 ? 0.35 + 5.0 * pow(r4, 7.0) : (layer == 1 ? 0.12 + 0.7 * pow(r4, 6.0) : 0.05 + 0.15 * pow(r4, 4.0));
        float rad = pix * 0.55;
        float core = exp(-dist * dist / (rad * rad));
        // scintillation: each its own beat, the bright ones a breath of colour with it
        float t = T * (2.0 + 6.0 * r2) + r3 * 40.0;
        float tw = 1.0 + Look.y * (0.6 * sin(t) + 0.4 * sin(t * 2.37 + r4 * 13.0)) * (layer == 2 ? 0.6 : 1.0);
        float3 tint = r3 < 0.3 ? float3(1.0, 0.75, 0.55) : (r3 < 0.75 ? float3(1.0, 0.96, 0.92) : float3(0.78, 0.86, 1.0));
        tint = lerp(tint, tint.bgr, saturate(Look.y * 0.2 * sin(t * 1.7)) * step(0.9, r4));
        // a soft glow round the brightest
        float halo = exp(-dist * dist / (pix * pix * 9.0)) * 0.08 * pow(r4, 10.0) * (layer == 0 ? 1.0 : 0.0);
        col += tint * flux * max(tw, 0.15) * (core + halo);
    }
}
// none in front of the star's disk and its glare
float sa = acos(clamp(dot(normalize(V), normalize(Sun.xyz)), -1, 1));
col *= smoothstep(Sun.w * 1.05, Sun.w * 3.0, sa);
return col * Look.x * Gain;
"""

AIR_HLSL = r"""
// the planet's air: single scattering through a thin shell (units of the planet's radius), its glowing limb,
// the day side's blue haze, the twilight ring past the terminator, nothing in the planet's shadow
float Ra = Air.x;
float Rp = ObjR / Ra;
float3 o = (Cam - C) / Rp;
float3 v = normalize(WP - Cam);
float b = dot(o, v);
float cc = dot(o, o) - Ra * Ra;
float disc = b * b - cc;
if (disc <= 0) return 0;
float sq = sqrt(disc);
float t0 = max(-b - sq, 0), t1 = -b + sq;
float dp = b * b - (dot(o, o) - 1.0);
if (dp > 0) { float tp = -b - sqrt(dp); if (tp > 0) t1 = min(t1, tp); }
if (t1 <= t0) return 0;
float H = (Ra - 1.0) * 0.26;
float3 Ls = normalize(SunDir.xyz);
float3 beta = float3(0.175, 0.41, 1.0);
float K = 60.0 * Air.y;
float ds = (t1 - t0) / 14.0;
float3 acc = 0;
float tauV = 0;
[unroll] for (int i = 0; i < 14; i++)
{
    float t = t0 + ds * (i + 0.5);
    float3 p = o + v * t;
    float r = length(p);
    float rho = exp(-(r - 1.0) / H);
    float mu = dot(p / r, Ls);
    float z = degrees(acos(clamp(mu, -1, 1)));
    float ch = 1.0 / max(mu + 0.15 * pow(max(93.885 - z, 0.01), -1.253), 0.03);
    float edge = -sqrt(max(1.0 - 1.0 / (r * r), 0.0));
    float lit = smoothstep(edge - 0.015, edge + 0.02, mu);
    float3 T = exp(-beta * K * H * (tauV + rho * ch));
    acc += rho * T * lit * ds;
    tauV += rho * ds / H;
}
float cosT = dot(v, Ls);
float phaseR = 0.75 * (1.0 + cosT * cosT);
float g = 0.72;
float phaseM = (1.0 - g * g) / pow(1.0 + g * g - 2.0 * g * cosT, 1.5) * 0.06;
return acc * K * (beta * phaseR + phaseM) * SunLight.rgb * 0.12;
"""


LEAD_HLSL = r"""
// the leads' water (ALyudEuropaSite): the sea's own absorption (make_sea_materials.py ABSORPTION, per metre) seen
// through to the bed where it is thin and black-teal where deep, a skin of slow ripples (0.13 g), its Fresnel mirror of
// the planet's disk and glow and the star's glitter, a little light scattered inside, and a rim of new ice where it
// thins against the shore. Translucent: the colour is what is added over the scene behind, as much as the water hides it.
float3 V = normalize(Cam - WP);
float2 p = (WP.xy - OP.xy) * 0.01;
float2 g = 0;
g += float2(0.8, 0.6) * cos(dot(p, float2(0.8, 0.6)) * 4.1 - T * 1.1) * 0.035;
g += float2(-0.5, 0.86) * cos(dot(p, float2(-0.5, 0.86)) * 7.3 - T * 1.6 + 1.3) * 0.025;
g += float2(0.2, -0.98) * cos(dot(p, float2(0.2, -0.98)) * 12.7 - T * 2.3 + 2.1) * 0.016;
g += float2(0.97, 0.24) * cos(dot(p, float2(0.97, 0.24)) * 21.0 - T * 3.0 + 0.4) * 0.011;
g += float2(-0.7, -0.7) * cos(dot(p, float2(-0.7, -0.7)) * 37.0 - T * 4.1 + 2.7) * 0.007;
float3 n = normalize(float3(-g, 1.0));
float ndv = saturate(dot(n, V));
float F = 0.02 + 0.98 * pow(1.0 - ndv, 5.0);
float3 R = reflect(-V, n);
float3 pd = normalize(PD.xyz);
float disk = smoothstep(PD.w - 0.004, PD.w + 0.004, dot(R, pd));
float glow = exp(-acos(clamp(dot(R, pd), -1, 1)) / 0.35) * 0.04;
float3 sd = normalize(SD.xyz);
float glint = smoothstep(SD.w - 0.003, SD.w + 0.003, dot(R, sd));
float3 refl = PL.rgb * (disk + glow) + SL.rgb * glint + 0.002;
// the path through the water to what lies under it, metres
float path = max(SceneD - PixD, 0.0) * 0.01;
float3 Tw = exp(-float3(0.42, 0.085, 0.13) * path * 1.6);
float tavg = dot(Tw, float3(0.2, 0.5, 0.3));
float3 lightNow = PL.rgb * 0.03 + SL.rgb * 0.05 * Lit + 0.002;
float3 body = float3(0.05, 0.22, 0.24) * lightNow * (1.0 - tavg);
float rim = exp(-path / 0.25) * (0.6 + 0.4 * frac(sin(dot(floor(p * 3.0), float2(12.9898, 78.233))) * 43758.5453));
float3 ice = float3(0.75, 0.82, 0.86) * lightNow * 8.0;
float op = saturate(1.0 - (1.0 - F) * tavg * (1.0 - rim));
float3 add = F * refl + (1.0 - F) * body + rim * ice;
return float4(add / max(op, 0.02), op);
"""


def lead_water_material():
    g = E.Graph("M_EU_LeadWater", shading=unreal.MaterialShadingModel.MSM_UNLIT, blend=unreal.BlendMode.BLEND_TRANSLUCENT)
    vec = lambda n, v: (g.vector(n, v), "RGBA")
    c = g.custom(LEAD_HLSL, [("WP", g.node(X.MaterialExpressionWorldPosition)), ("OP", g.node(X.MaterialExpressionObjectPositionWS)),
                             ("Cam", g.node(X.MaterialExpressionCameraPositionWS)), ("T", g.node(X.MaterialExpressionTime)),
                             ("SceneD", g.node(X.MaterialExpressionSceneDepth)), ("PixD", g.node(X.MaterialExpressionPixelDepth)),
                             ("PD", vec("PlanetDir", (0, 0, 1, 0.974))), ("PL", vec("PlanetLight", (5, 6, 8, 0))),
                             ("SD", vec("SunDir", (0, 0, 1, 0.996))), ("SL", vec("SunLight", (18, 15, 12, 0))),
                             ("Lit", g.scalar("SunLit", 1.0))], out=unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    rgb = g.node(X.MaterialExpressionComponentMask, r=True, g=True, b=True, a=False)
    g.link(c, rgb)
    a = g.node(X.MaterialExpressionComponentMask, r=False, g=False, b=False, a=True)
    g.link(c, a)
    g.out(rgb, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(a, unreal.MaterialProperty.MP_OPACITY)
    return g.finish()


def terrain_material():
    g = E.Graph("M_EU_Terrain")
    g.m.set_editor_property("tangent_space_normal", False)
    nanite_usage(g.m)
    wp = g.node(X.MaterialExpressionWorldPosition)
    op = g.node(X.MaterialExpressionObjectPositionWS)
    vn = g.node(X.MaterialExpressionVertexNormalWS)
    vc = g.node(X.MaterialExpressionVertexColor)
    ice = g.node(X.MaterialExpressionTextureObjectParameter, parameter_name="Ice", texture=E.tex("T_EU_ice_C"))
    nrm = g.node(X.MaterialExpressionTextureObjectParameter, parameter_name="IceNormal", texture=E.tex("T_EU_ice_N"))
    c = g.node(X.MaterialExpressionCustom, -450, code=TERRAIN_HLSL, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for name in ("WP", "OP", "VN", "VC", "VA", "Ice", "Nrm", "Bump", "Albedo"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    outs = []
    for name, typ in (("Normal", unreal.CustomMaterialOutputType.CMOT_FLOAT3), ("Rough", unreal.CustomMaterialOutputType.CMOT_FLOAT1)):
        co = unreal.CustomOutput()
        co.set_editor_property("output_name", name)
        co.set_editor_property("output_type", typ)
        outs.append(co)
    c.set_editor_property("additional_outputs", outs)
    g.link(wp, c, "WP")
    g.link(op, c, "OP")
    g.link(vn, c, "VN")
    g.link(vc, c, "VC", "")
    g.link(vc, c, "VA", "A")
    g.link(ice, c, "Ice")
    g.link(nrm, c, "Nrm")
    g.link(g.scalar("Bump", 1.1), c, "Bump")
    g.link(g.scalar("Albedo", 0.9), c, "Albedo")
    g.out(c, unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(c, unreal.MaterialProperty.MP_NORMAL, "Normal")
    g.out(c, unreal.MaterialProperty.MP_ROUGHNESS, "Rough")
    g.out(g.scalar("Specular", 0.5), unreal.MaterialProperty.MP_SPECULAR)
    return g.finish()


def space_material():
    g = E.Graph("M_EU_Space", shading=unreal.MaterialShadingModel.MSM_UNLIT, blend=unreal.BlendMode.BLEND_ADDITIVE, two_sided=True)
    cam = g.node(X.MaterialExpressionCameraVectorWS)
    view = g.custom("return -C;", [("C", cam)])
    loc = g.node(X.MaterialExpressionTransform, transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,
                 transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
    g.link(view, loc)
    c = g.custom(SPACE_HLSL, [("L", loc), ("V", view), ("T", g.node(X.MaterialExpressionTime)),
                              ("Sun", (g.vector("SunDir", (0, 0, 1, 0.087)), "RGBA")),
                              ("Look", (g.vector("Look", (1, 0.35, 0, 0)), "RGBA")), ("Gain", g.scalar("Gain", 1.0))])
    g.out(c, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return g.finish()


def air_material():
    g = E.Graph("M_EU_PlanetAir", shading=unreal.MaterialShadingModel.MSM_UNLIT, blend=unreal.BlendMode.BLEND_ADDITIVE)
    c = g.custom(AIR_HLSL, [("WP", g.node(X.MaterialExpressionWorldPosition)), ("C", g.node(X.MaterialExpressionObjectPositionWS)),
                            ("Cam", g.node(X.MaterialExpressionCameraPositionWS)), ("ObjR", g.node(X.MaterialExpressionObjectRadius)),
                            ("SunDir", (g.vector("SunDir", (0.3, 0.2, 0.9, 0)), "RGBA")),
                            ("SunLight", (g.vector("SunLight", (12.7, 10.5, 8.6, 0)), "RGBA")),
                            ("Air", (g.vector("Air", (1.045, 1.0, 0, 0)), "RGBA"))])
    g.out(g.op(X.MaterialExpressionMultiply, c, g.scalar("Gain", 2.2)), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return g.finish()


def import_meshes(terrain, boulder_mat):
    if not os.path.isfile(GLB):
        raise RuntimeError("no %s: run tools/europa/moon_terrain.py" % GLB)
    with open(MOON_JSON) as f:
        moon = json.load(f)
    level = {t["name"]: t["level"] for t in moon["tiles"]}
    water = {w["name"] for w in moon["water"]}
    tmp = DEST + "/moon_terrain"
    if eal.does_directory_exist(DEST):
        eal.delete_directory(DEST)
    src = unreal.InterchangeManager.create_source_data(GLB)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_scene(DEST, src, params)
    mgr.wait_until_all_tasks_done(True)
    n = tris = 0
    for path in eal.list_assets(DEST, True):
        a = eal.load_asset(path)
        if not isinstance(a, unreal.StaticMesh):
            continue
        name = a.get_name()
        dest = DEST + "/" + name
        if path.split(".")[0] != dest:
            if eal.does_asset_exist(dest):
                eal.delete_asset(dest)
            eal.rename_asset(path, dest)
            a = eal.load_asset(dest)
        s = a.get_editor_property("nanite_settings")
        s.enabled = name not in water
        if name in level:
            s.fallback_target = unreal.NaniteFallbackTarget.PERCENT_TRIANGLES
            s.fallback_percent_triangles = FALLBACK.get(level[name], 0.5)
        elif name == "SM_EU_Boulders":
            s.fallback_target = unreal.NaniteFallbackTarget.PERCENT_TRIANGLES
            s.fallback_percent_triangles = 0.5
        a.set_editor_property("nanite_settings", s)
        body = a.get_editor_property("body_setup")
        if body:
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        a.set_material(0, boulder_mat if name == "SM_EU_Boulders" else terrain)
        eal.save_loaded_asset(a)
        tris += a.get_num_triangles(0)
        n += 1
    eal.save_directory(DEST)
    if eal.does_directory_exist(tmp):
        eal.delete_directory(tmp)
    # what Interchange left in the level
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for act in eas.get_all_level_actors():
        if act.get_name().startswith("SM_EU_") or "moon_terrain" in str(act.get_folder_path()):
            eas.destroy_actor(act)
    log("meshes: %d into %s (%d fallback triangles; the json says %d)" % (n, DEST, tris, moon["triangles"]))


def main():
    terrain = terrain_material()
    boulder = E.instance("MI_EU_Boulder", terrain, {"Albedo": 0.95, "Bump": 0.8})
    nanite_usage(boulder)
    space_material()
    air_material()
    lead_water_material()
    # the planet's textures (the island where the town is, its lights) and its material, back on its mesh
    E.import_textures()
    planet = E.planet()
    sm = eal.load_asset(E.MESHES + "/SM_EU_Planet")
    if sm:
        sm.set_material(0, planet)
        eal.save_loaded_asset(sm)
    log("materials built")
    if os.environ.get("IMPORT_MOON_ONLY") == "materials":
        # the rebuilt materials back on the meshes (a deleted and re-made material drops off them)
        for path in eal.list_assets(DEST, False):
            a = eal.load_asset(path)
            if isinstance(a, unreal.StaticMesh) and "Water" not in a.get_name():
                a.set_material(0, boulder if a.get_name() == "SM_EU_Boulders" else terrain)
                eal.save_loaded_asset(a)
        return
    import_meshes(terrain, boulder)


main()
