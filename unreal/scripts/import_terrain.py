"""The outdoor ground in an Unreal level: a Landscape over the whole map (tools/terrain.py samples the
map's macro shape, ULyudTerrainLibrary::BuildTerrain adds the natural detail, erosion, dirt tracks and
surface layers), its layered material from textures/ground/, the grass mask
(/Game/Scene01/Terrain/DA_TerrainMask, ULyudTerrainMask) and the small mesh patch that fills the
landscape's hole at the club's portal.

Run as a script it adds the ground to Lvl_Scene01 (build_scene01.sh runs it before import_world.py, in
a process of its own); make_terrain_preview_level.py imports build() for a level with only the ground.
Nothing here is committed content but the textures; the build makes the rest every time. Afterwards
tools/terrain.py ground_z() reads the built heights (Saved/World/terrain_out.bin) for placing things.
"""
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools"))
import terrain as T  # noqa: E402
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import winter_snow  # noqa: E402
import forestfloor_ground  # noqa: E402

DEST = "/Game/Scene01/Terrain"
TIDE_MPC = "/Game/Weather/MPC_LyudWeather"      # its "Tide" (make_sea_materials.py) wets the sand under the tide
TEX_DIR = os.path.join(REPO, "textures", "ground")
eal = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty
LIB = unreal.LyudTerrainLibrary

# Per layer: tint (multiplies the photo), roughness, and how strongly its texture's bright bits win a
# blend with its neighbours (stones and grass tips poke through, mud and sand settle into the gaps).
LOOK = {
    "Grass": ((0.66, 0.78, 0.56), 0.92, 0.6),
    "Meadow": ((0.74, 0.7, 0.6), 0.9, 0.6),
    "Dirt": ((0.6, 0.55, 0.52), 0.88, 0.5),
    "Mud": ((0.6, 0.58, 0.56), 0.62, 0.2),
    "Sand": ((0.74, 0.69, 0.6), 0.82, 0.3),
    "Rock": ((0.56, 0.56, 0.57), 0.8, 0.9),
    "Forest": ((0.72, 0.72, 0.68), 0.9, 0.5),
}
NEAR_TILE = 2.6       # metres per texture repeat up close
FAR_TILE = 11.0       # and the coarser copy that takes over in the distance and breaks the repeats


def _log(msg):
    unreal.log_warning("SCENE01 TERRAIN " + msg)


def _node(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def _link(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError("could not connect %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def _custom_input(name):
    c = unreal.CustomInput()
    c.set_editor_property("input_name", name)
    return c


def _custom_output(name, typ):
    c = unreal.CustomOutput()
    c.set_editor_property("output_name", name)
    c.set_editor_property("output_type", typ)
    return c


def import_textures(layers):
    """textures/ground/<Layer>_D.jpg, _N.jpg -> DEST/Textures/T_<Layer>_D, _N."""
    tasks = []
    for name in layers:
        for suffix in ("D", "N"):
            t = unreal.AssetImportTask()
            t.filename = os.path.join(TEX_DIR, "%s_%s.jpg" % (name, suffix))
            t.destination_path = DEST + "/Textures"
            t.destination_name = "T_%s_%s" % (name, suffix)
            t.automated = True
            t.replace_existing = True
            t.save = False
            tasks.append(t)
    tools.import_asset_tasks(tasks)
    out = {}
    for name in layers:
        d = eal.load_asset("%s/Textures/T_%s_D" % (DEST, name))
        n = eal.load_asset("%s/Textures/T_%s_N" % (DEST, name))
        n.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        n.set_editor_property("srgb", False)
        for tex in (d, n):
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
        out[name] = (d, n)
    return out


def _blend_hlsl(layers):
    """Height-aware blend of the layers: each layer's photo is mixed from the near and far tiling, its
    brightness lets it win a little against its neighbours, a slow macro variation breaks repeats."""
    n = len(layers)
    tints = ", ".join("float3(%g, %g, %g)" % LOOK[l][0] for l in layers)
    rough = ", ".join("%g" % LOOK[l][1] for l in layers)
    poke = ", ".join("%g" % LOOK[l][2] for l in layers)
    lines = ["float W[%d] = {%s};" % (n, ", ".join("W%d" % i for i in range(n))),
             "float3 A[%d] = {%s};" % (n, ", ".join("A%d" % i for i in range(n))),
             "float3 F[%d] = {%s};" % (n, ", ".join("F%d" % i for i in range(n))),
             "float3 N[%d] = {%s};" % (n, ", ".join("N%d" % i for i in range(n))),
             "const float3 Tint[%d] = {%s};" % (n, tints),
             "const float Rough[%d] = {%s};" % (n, rough),
             "const float Poke[%d] = {%s};" % (n, poke)]
    lines.append("""
float nearK = 1.0 - saturate((Dist - 600.0) / 5000.0);
float farMix = lerp(0.65, 0.25, nearK);
float b[%(n)d];
float top = -10.0;
for (int i = 0; i < %(n)d; i++)
{
    float h = dot(A[i], float3(0.33, 0.34, 0.33));
    b[i] = W[i] > 0.002 ? W[i] + h * Poke[i] * 0.6 : -10.0;
    top = max(top, b[i]);
}
float total = 0.0;
for (int i = 0; i < %(n)d; i++)
{
    b[i] = max(b[i] - top + 0.22, 0.0);
    total += b[i];
}
float3 col = 0;
float3 nrm = 0;
float r = 0;
for (int i = 0; i < %(n)d; i++)
{
    float k = b[i] / max(total, 1e-4);
    float3 c = lerp(A[i], F[i], farMix);
    col += k * c * Tint[i];
    nrm += k * N[i];
    r += k * Rough[i];
}
col *= 0.7 + 0.75 * Macro;                      // Macro averages ~0.4
Normal = normalize(lerp(float3(0, 0, 1), nrm, nearK * 0.4 + 0.1));
Roughness = r;
return col;
""" % {"n": n})
    return "\n".join(lines)


# Layers that cover steep faces (the bluff) are projected from the sides as well as from above, so
# their photo is not stretched down a cliff: metres per repeat.
TRIPLANAR = {"Rock": 6.0}

TRIPLANAR_UV_HLSL = """
float s = 1.0 / (Tile * 100.0);
UVX = float2(P.y, -P.z) * s;
UVY = float2(P.x, -P.z) * s;
return float2(P.x, P.y) * s;
"""

TRIPLANAR_MIX_HLSL = """
// the facing from the pixel's own slope (a landscape's vertex normal is not reliable in its pixel shader)
float3 Nw = normalize(cross(ddx(P), ddy(P)));
float3 w = pow(abs(Nw), 4.0);
w /= (w.x + w.y + w.z);
// the side views' tangent space is only roughly the landscape's: their bumps at half strength
Nrm = normalize(NZ * w.z + float3(NX.xy * 0.5, 1) * w.x + float3(NY.xy * 0.5, 1) * w.y);
float3 c = CX * w.x + CY * w.y + CZ * w.z;
// strata on the side views: wavy bands of pale sand and dark silt through the clay, a metre or two apart
float side = saturate(1.0 - w.z * 2.0);
float wav = sin(P.x * 0.0007 + P.y * 0.0005) * 140.0 + sin(P.x * 0.0031 - P.y * 0.0023) * 35.0;
float sand = smoothstep(0.55, 0.95, sin((P.z + wav) * 0.0042));
float silt = smoothstep(0.75, 0.98, sin((P.z + wav * 0.7) * 0.0097 + 1.3));
c = lerp(c, c * float3(1.35, 1.22, 0.92), sand * 0.7 * side);
c = lerp(c, c * 0.55, silt * 0.6 * side);
return c;
"""


def _triplanar(m, d, nm, tile, x, y, shared):
    """Colour and normal of a photo projected along world X, Y and Z, mixed by the surface's facing."""
    pos = _node(m, unreal.MaterialExpressionWorldPosition, x - 700, y)
    uv = _node(m, unreal.MaterialExpressionCustom, x - 450, y, code=TRIPLANAR_UV_HLSL,
               output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT2, description="TriplanarUV",
               inputs=[_custom_input("P"), _custom_input("Tile")],
               additional_outputs=[_custom_output("UVX", unreal.CustomMaterialOutputType.CMOT_FLOAT2),
                                   _custom_output("UVY", unreal.CustomMaterialOutputType.CMOT_FLOAT2)])
    tile_c = _node(m, unreal.MaterialExpressionConstant, x - 700, y + 300, r=tile)
    _link(pos, "", uv, "P")
    _link(tile_c, "", uv, "Tile")
    mix = _node(m, unreal.MaterialExpressionCustom, x + 300, y, code=TRIPLANAR_MIX_HLSL,
                output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description="TriplanarMix",
                inputs=[_custom_input(n) for n in ("CX", "CY", "CZ", "NX", "NY", "NZ", "P")],
                additional_outputs=[_custom_output("Nrm", unreal.CustomMaterialOutputType.CMOT_FLOAT3)])
    for k, out in enumerate(("UVX", "UVY", "")):
        axis = "XYZ"[k]
        c = _node(m, unreal.MaterialExpressionTextureSample, x, y + k * 90, texture=d, sampler_source=shared)
        t = _node(m, unreal.MaterialExpressionTextureSample, x, y + 270 + k * 90, texture=nm, sampler_source=shared,
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        _link(uv, out, c, "UVs")
        _link(uv, out, t, "UVs")
        _link(c, "RGB", mix, "C" + axis)
        _link(t, "RGB", mix, "N" + axis)
    _link(pos, "", mix, "P")
    return mix                                        # outputs: "" colour, "Nrm" normal


# Wet sand and pools under the tide: from the water's edge up to where the water stood within the last hour the sand
# is dark and glistening (a film of water over it: smooth, its bumps flattened, wet grains catching the sun), a damp
# band above that dries out over the hour as the ebb leaves it; over the high-water mark it stays dry.
# Tide = MPC_LyudWeather "Tide" (ALyudSea): (water now, datum, wet line, springs), world cm; T = time s.
# The pools are water: nearly black through their depth (the sand under them seen darkly), smooth and mirroring the
# sky, ruffled by small drifting ripples, with a sharp edge and a band of soaked sand round it.
WET_HLSL = """
float z = P.z;
float now = Tide.x;
float top = max(Tide.z, now) + 10.0;
float wet = (1.0 - smoothstep(top - 5.0, top + 25.0, z)) * (1.0 - smoothstep(Tide.y + 15.0, Tide.y + 45.0, z));
float sheen = wet * (1.0 - saturate((z - now) / max(top - now, 40.0)));
// pools: the hollows the ebb cannot drain keep water up to their rim (the sea's depth map: r = bed under the datum,
// g = the water a hollow keeps, both m; ALyudSea hands it over as TidePools / PoolRect)
struct FWetLib
{
    float H(float2 c) { return frac(sin(dot(c, float2(127.1, 311.7))) * 43758.5453); }
    float N(float2 x)
    {
        float2 i = floor(x), f = frac(x);
        f = f * f * (3.0 - 2.0 * f);
        return lerp(lerp(H(i), H(i + float2(1, 0)), f.x), lerp(H(i + float2(0, 1)), H(i + float2(1, 1)), f.x), f.y);
    }
};
FWetLib WL;
float2 xm = P.xy * 0.01;
// the pools' outline wanders: the lookup is warped by a couple of metres and the water level against the sand
// shifted by a centimetre or two (on a flat that moves the edge by metres), so no straight edges from the map's cells
float2 warp = (float2(WL.N(xm / 7.0), WL.N(xm / 7.0 + 31.7)) - 0.5) * 3.0
             + (float2(WL.N(xm / 31.0 + 3.3), WL.N(xm / 31.0 + 17.9)) - 0.5) * 14.0;
float wob = (WL.N(xm / 3.1 + 5.3) - 0.5) * 2.4 + (WL.N(xm / 0.9 + 11.1) - 0.5) * 0.8;
// the runnels behind the bars keep long strips along the shore: the water in them drains down to a level that
// wanders by up to ~12 cm over tens of metres, so a strip breaks into separate lobed pools with sand spits between
float drain = saturate((WL.N(xm / 23.0 + 7.7) * 0.65 + WL.N(xm / 9.0 + 2.9) * 0.35 - 0.38) * 1.6) * 12.0;
float pool = 0.0, rim = 0.0, deep = 0.0;
if (Rect.z > 0.0)
{
    float2 uv = (xm + warp - Rect.xy) * Rect.zw;
    float2 bw = Pools.SampleLevel(PoolsSampler, uv, 0).rg;
    float surf = Tide.y - (bw.x - bw.y) * 100.0 - 2.0 + wob - drain;   // the pool's water level, world cm
    float keep = step(Tide.x + 3.0, surf) * smoothstep(0.004, 0.02, bw.y) * step(1.0, bw.y * 100.0 - drain + 2.0);
    pool = smoothstep(0.0, 0.35, surf - z) * keep;
    deep = smoothstep(0.3, 10.0, surf - z) * keep;
    // the wet edge: soaked sand a little above the water, darkest right at it
    rim = (1.0 - smoothstep(0.0, 2.5, z - surf)) * keep * (1.0 - pool);
}
float2 xy = P.xy * 0.01;
// fine sparkle on the soaked sand: grain-sized facets tilted at random catch the sun and the sky; faded out where a
// pixel covers many grains (else it shimmers)
float foot = max(length(ddx(xy)), length(ddy(xy)));
float2 cell = floor(xy * 60.0);
float h1 = frac(sin(dot(cell, float2(127.1, 311.7))) * 43758.5453);
float h2 = frac(sin(dot(cell, float2(269.5, 183.3))) * 43758.5453);
float spark = step(0.9, h1) * (1.0 - smoothstep(0.004, 0.02, foot)) * max(sheen, rim);
float3 sn = normalize(float3((h2 - 0.5) * 0.6, (frac(h2 * 7.13) - 0.5) * 0.6, 1.0));
// the pools' ripples: a few small trains drifting with time (a breath of wind over still water)
float2 rg = 0.0;
rg += float2(0.8, 0.6) * cos(dot(xy, float2(0.8, 0.6)) * 9.0 - T * 2.1);
rg += float2(-0.5, 0.86) * cos(dot(xy, float2(-0.5, 0.86)) * 13.0 - T * 2.7 + 1.3);
rg += float2(0.97, -0.26) * cos(dot(xy, float2(0.97, -0.26)) * 21.0 - T * 3.3 + 4.1);
rg += float2(-0.3, -0.95) * cos(dot(xy, float2(-0.3, -0.95)) * 31.0 - T * 4.0 + 2.2) * 0.6;
rg += float2(WL.N(xy * 1.7 + T * 0.3) - 0.5, WL.N(xy * 1.7 + 9.0 - T * 0.25) - 0.5) * 0.8;
float3 pn = normalize(float3(rg * 0.03, 1.0));
float3 n = normalize(lerp(N, float3(0, 0, 1), 0.65 * max(sheen, rim)));
n = normalize(lerp(n, sn, spark));
Nrm = normalize(lerp(n, pn, pool));
Rough = lerp(lerp(lerp(lerp(R, 0.42, wet), 0.12, sheen), 0.1, rim), 0.05, spark * (1.0 - pool));
Rough = lerp(Rough, 0.02, pool);
Spec = lerp(lerp(0.5, 0.6, max(sheen, rim)), 0.15, pool);
// the sky in the pools: the sky light's cubemap (clouds and all) along the mirror direction, by water's Fresnel (the
// landscape's own reflections stay dim on still water), RDir / RAmt for the SkyLightEnvMapSample node
float3 v = normalize(-V);
float3 wn = normalize(float3(Nrm.xy, max(Nrm.z, 0.2)));
RDir = reflect(-v, wn);
float cosv = saturate(dot(v, wn));
// a pool is shallow: mostly the sand through it, the sky by water's Fresnel only toward grazing and never a full
// mirror of the pale horizon (that made paper-white strips from afar); the mirrored ray is lifted a little off the
// horizon so it picks up blue and cloud
RDir.z = max(RDir.z, 0.3);
// Schlick's Fresnel for water (0.02 head-on), held under a full mirror; the sand under it is dimmed by what's reflected
float fr = 0.02 + 0.98 * pow(1.0 - cosv, 5.0);
RAmt = pool * min(fr, 0.75);
float3 c = C * lerp(float3(1, 1, 1), float3(0.42, 0.42, 0.44), max(wet * 0.6, rim));
// the wet edge darkest right at the water: a darker ring outlines each pool
c *= lerp(1.0, 0.72, rim * rim);
c *= 1.0 - min(fr, 0.75) * pool;
// through the water: the sand darker and a little greener, more so where the pool is deeper
return lerp(c, c * lerp(float3(0.6, 0.6, 0.55), float3(0.3, 0.34, 0.33), deep), pool);
"""


def _wet_node(m, blend):
    """The wet sand and pools (WET_HLSL) on top of the ground's blend; wires base colour, normal, roughness, specular."""
    pos = _node(m, unreal.MaterialExpressionWorldPosition, -300, -400)
    tide = _node(m, unreal.MaterialExpressionCollectionParameter, -300, -300,
                 collection=unreal.load_asset(TIDE_MPC), parameter_name="Tide")
    pools = _node(m, unreal.MaterialExpressionTextureObjectParameter, -300, -200, parameter_name="TidePools",
                  texture=unreal.load_asset("/Engine/EngineResources/Black"))
    rect = _node(m, unreal.MaterialExpressionVectorParameter, -300, -100, parameter_name="PoolRect",
                 default_value=unreal.LinearColor(0, 0, 0, 0))
    time = _node(m, unreal.MaterialExpressionTime, -300, -500)
    wet = _node(m, unreal.MaterialExpressionCustom, 100, 0, code=WET_HLSL,
                output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description="WetSand",
                inputs=[_custom_input(n) for n in ("C", "N", "R", "P", "Tide", "Pools", "Rect", "T", "V")],
                additional_outputs=[_custom_output("Nrm", unreal.CustomMaterialOutputType.CMOT_FLOAT3),
                                    _custom_output("Rough", unreal.CustomMaterialOutputType.CMOT_FLOAT1),
                                    _custom_output("Spec", unreal.CustomMaterialOutputType.CMOT_FLOAT1),
                                    _custom_output("RDir", unreal.CustomMaterialOutputType.CMOT_FLOAT3),
                                    _custom_output("RAmt", unreal.CustomMaterialOutputType.CMOT_FLOAT1)])
    _link(blend, "", wet, "C")
    _link(blend, "Normal", wet, "N")
    _link(blend, "Roughness", wet, "R")
    _link(pos, "", wet, "P")
    _link(tide, "", wet, "Tide")
    _link(pools, "", wet, "Pools")
    _link(rect, "RGBA", wet, "Rect")
    _link(time, "", wet, "T")
    view = _node(m, unreal.MaterialExpressionCameraVectorWS, -300, -100 - 500)
    _link(view, "", wet, "V")
    env = _node(m, unreal.MaterialExpressionSkyLightEnvMapSample, 400, -200)
    _link(wet, "RDir", env, "Direction")
    rough = _node(m, unreal.MaterialExpressionConstant, 300, -100, r=0.04)
    _link(rough, "", env, "Roughness")
    refl = _node(m, unreal.MaterialExpressionMultiply, 600, -200)
    _link(env, "", refl, "A")
    _link(wet, "RAmt", refl, "B")
    mel.connect_material_property(refl, "", MP.MP_EMISSIVE_COLOR)
    mel.connect_material_property(wet, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(wet, "Nrm", MP.MP_NORMAL)
    mel.connect_material_property(wet, "Rough", MP.MP_ROUGHNESS)
    mel.connect_material_property(wet, "Spec", MP.MP_SPECULAR)
    return wet


def patch_wet():
    """Rebuilds only M_Ground's wet sand and pools in place (the landscape keeps its material), for tuning WET_HLSL
    without rebuilding the ground: TERRAIN_WET_ONLY=1."""
    m = unreal.load_asset(DEST + "/M_Ground")
    blend = None
    for e in [e for e in unreal.ObjectIterator(unreal.MaterialExpression) if e.get_outer() == m]:
        if isinstance(e, unreal.MaterialExpressionCustom) and e.get_editor_property("description") == "WetSand":
            mel.delete_material_expression(m, e)
        elif isinstance(e, unreal.MaterialExpressionCustom) and e.get_editor_property("description") == "GroundBlend":
            blend = e
        elif e.get_editor_property("material_expression_editor_x") == -300 and \
                e.get_editor_property("material_expression_editor_y") in (-600, -500, -400, -300, -200, -100) or \
                (e.get_editor_property("material_expression_editor_x") in (300, 400, 600) and
                 e.get_editor_property("material_expression_editor_y") in (-200, -100)):
            mel.delete_material_expression(m, e)     # the wet node's own inputs (_wet_node puts them there)
    if blend is None:
        raise RuntimeError("M_Ground has no GroundBlend node: rebuild the ground")
    litter = [e for e in unreal.ObjectIterator(unreal.MaterialExpressionCustom)
              if e.get_outer() == m and e.get_editor_property("description") == "ForestLitter"]
    _wet_node(m, litter[0] if litter else blend)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    _log("M_Ground wet sand patched")


def make_material(layers, textures):
    path = DEST + "/M_Ground"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = tools.create_asset("M_Ground", DEST, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    shared = unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS
    near = _node(m, unreal.MaterialExpressionLandscapeLayerCoords, -2400, 0, mapping_scale=NEAR_TILE)
    far = _node(m, unreal.MaterialExpressionLandscapeLayerCoords, -2400, 200, mapping_scale=FAR_TILE)
    macro1 = _node(m, unreal.MaterialExpressionLandscapeLayerCoords, -2400, 400, mapping_scale=47.0)
    macro2 = _node(m, unreal.MaterialExpressionLandscapeLayerCoords, -2400, 600, mapping_scale=131.0)
    n = len(layers)
    blend = _node(m, unreal.MaterialExpressionCustom, -300, 0, code=_blend_hlsl(layers),
                  output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description="GroundBlend",
                  inputs=[_custom_input(p + str(i)) for p in "WAFN" for i in range(n)] +
                  [_custom_input("Macro"), _custom_input("Dist")],
                  additional_outputs=[_custom_output("Normal", unreal.CustomMaterialOutputType.CMOT_FLOAT3),
                                      _custom_output("Roughness", unreal.CustomMaterialOutputType.CMOT_FLOAT1)])
    forest_w = None
    for i, name in enumerate(layers):
        d, nm = textures[name]
        y = i * 520
        w = _node(m, unreal.MaterialExpressionLandscapeLayerSample, -1200, y, parameter_name=name,
                  preview_weight=1.0 if i == 0 else 0.0)
        if name == "Forest":
            forest_w = w
        if name in TRIPLANAR:
            mix = _triplanar(m, d, nm, TRIPLANAR[name], -1800, y, shared)
            _link(w, "", blend, "W%d" % i)
            _link(mix, "", blend, "A%d" % i)
            _link(mix, "", blend, "F%d" % i)
            _link(mix, "Nrm", blend, "N%d" % i)
            continue
        a = _node(m, unreal.MaterialExpressionTextureSample, -1800, y + 100, texture=d, sampler_source=shared)
        f = _node(m, unreal.MaterialExpressionTextureSample, -1800, y + 250, texture=d, sampler_source=shared)
        t = _node(m, unreal.MaterialExpressionTextureSample, -1800, y + 400, texture=nm, sampler_source=shared,
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        _link(near, "", a, "UVs")
        _link(far, "", f, "UVs")
        _link(near, "", t, "UVs")
        _link(w, "", blend, "W%d" % i)
        _link(a, "RGB", blend, "A%d" % i)
        _link(f, "RGB", blend, "F%d" % i)
        _link(t, "RGB", blend, "N%d" % i)
    # slow brightness drift from the grass photo seen from very far, two scales
    grass_d = textures[layers[0]][0]
    m1 = _node(m, unreal.MaterialExpressionTextureSample, -1800, -500, texture=grass_d, sampler_source=shared)
    m2 = _node(m, unreal.MaterialExpressionTextureSample, -1800, -300, texture=grass_d, sampler_source=shared)
    _link(macro1, "", m1, "UVs")
    _link(macro2, "", m2, "UVs")
    mix = _node(m, unreal.MaterialExpressionLinearInterpolate, -1400, -400, const_alpha=0.5)
    _link(m1, "G", mix, "A")
    _link(m2, "R", mix, "B")
    _link(mix, "", blend, "Macro")
    depth = _node(m, unreal.MaterialExpressionPixelDepth, -1400, -200)
    _link(depth, "", blend, "Dist")
    # the forest floor's litter (AForestFloor's field) over the forest layer, under the wet sand and pools
    _wet_node(m, forestfloor_ground.ground_litter(m, blend, forest_w) if forest_w else blend)
    hole = _node(m, unreal.MaterialExpressionLandscapeVisibilityMask, -300, 400)
    mel.connect_material_property(hole, "", MP.MP_OPACITY_MASK)
    winter_snow.ground_wpo(m)   # the snow lifts the ground (ULyudWinterSubsystem)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def make_patch_material(textures):
    """The portal patch's ground: the grass photo, world-aligned at the landscape's near tiling."""
    path = DEST + "/M_GroundPatch"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = tools.create_asset("M_GroundPatch", DEST, unreal.Material, unreal.MaterialFactoryNew())
    d, nm = textures["Grass"]
    pos = _node(m, unreal.MaterialExpressionWorldPosition, -900, 0)
    xy = _node(m, unreal.MaterialExpressionComponentMask, -700, 0, r=True, g=True, b=False, a=False)
    scale = _node(m, unreal.MaterialExpressionDivide, -500, 0, const_b=NEAR_TILE * 100.0)
    _link(pos, "", xy, "")
    _link(xy, "", scale, "A")
    a = _node(m, unreal.MaterialExpressionTextureSample, -300, 0, texture=d)
    t = _node(m, unreal.MaterialExpressionTextureSample, -300, 250, texture=nm,
              sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    _link(scale, "", a, "UVs")
    _link(scale, "", t, "UVs")
    tint = _node(m, unreal.MaterialExpressionMultiply, -100, 0, const_b=0.95)
    _link(a, "RGB", tint, "A")
    mel.connect_material_property(tint, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(t, "RGB", MP.MP_NORMAL)
    rough = _node(m, unreal.MaterialExpressionConstant, -100, 300, r=LOOK["Grass"][1])
    mel.connect_material_property(rough, "", MP.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def make_patch(face_y, material):
    """terrain.portal_patch() as a static mesh with exact (complex-as-simple) collision, in the
    landscape's hole at the portal."""
    verts, tris = T.portal_patch()
    path = DEST + "/SM_PortalPatch"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    sm = tools.create_asset("SM_PortalPatch", DEST, unreal.StaticMesh, None)
    desc = sm.create_static_mesh_description()
    group = desc.create_polygon_group()
    vids = []
    for x, y, z in verts:
        v = desc.create_vertex()
        desc.set_vertex_position(v, unreal.Vector(x * 100.0, y * 100.0, z * 100.0))
        vids.append(v)
    for a, b, c in tris:
        inst = []
        # Unreal's front face winds clockwise seen from above (its Y runs the other way to glTF's)
        for k in (a, c, b):
            vi = desc.create_vertex_instance(vids[k])
            desc.set_vertex_instance_uv(vi, unreal.Vector2D(verts[k][0] / NEAR_TILE, verts[k][1] / NEAR_TILE), 0)
            inst.append(vi)
        desc.create_triangle(group, inst)
    desc.compute_polygon_triangle_normals_and_tangents() if hasattr(desc, "compute_polygon_triangle_normals_and_tangents") else None
    sm.set_editor_property("allow_cpu_access", True)     # the collision is cooked from this mesh's own triangles
    sm.build_from_static_mesh_descriptions([desc])
    sm.set_material(0, material)
    body = sm.get_editor_property("body_setup")
    if body:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    eal.save_loaded_asset(sm)
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, face_y, 0))
    a.static_mesh_component.set_static_mesh(sm)
    a.static_mesh_component.set_editor_property("lighting_channels", unreal.LightingChannels(True, True, False))
    a.set_actor_label("Ground_PortalPatch")
    a.set_folder_path("Outside/Ground")
    return a


def build(face_y):
    """Adds the ground to the open level; face_y = the portal face, Unreal Y cm."""
    out_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "World")
    meta = T.write_inputs(out_dir, log=_log)
    _log("inputs written to %s (%d x %d)" % (out_dir, meta["nx"], meta["ny"]))
    layers = [str(n) for n in LIB.get_layer_names()]
    textures = import_textures(layers)
    material = make_material(layers, textures)
    infos = []
    for name in layers:
        path = "%s/Layers/LI_%s" % (DEST, name)
        if eal.does_asset_exist(path):
            eal.delete_asset(path)
        info = LIB.create_layer_info(path, name)
        eal.save_loaded_asset(info)
        infos.append(info)
    mask_path = DEST + "/DA_TerrainMask"
    if eal.does_asset_exist(mask_path):
        eal.delete_asset(mask_path)
    mask = LIB.create_mask(mask_path)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    origin = unreal.Vector(T.GRID_X0 * 100.0, face_y + T.GRID_Y0 * 100.0, 0.0)
    landscape, report = LIB.build_terrain(world, out_dir, origin, material, infos, mask)
    for line in str(report).splitlines():
        _log(line)
    if not landscape:
        raise RuntimeError("terrain build failed: %s" % report)
    landscape.set_folder_path("Outside/Ground")
    # the sea hands the ground its tide pools at runtime (ALyudSea::UploadDepthMap): per-component dynamic instances
    landscape.set_editor_property("use_dynamic_material_instance", True)
    eal.save_loaded_asset(mask)
    T.load(out_dir)                                   # ground_z now answers with the built heights
    make_patch(face_y, make_patch_material(textures))
    eal.save_directory(DEST)
    return landscape


if __name__ == "__main__" and os.environ.get("TERRAIN_WET_ONLY"):
    patch_wet()
elif __name__ == "__main__":
    import json
    with open(os.path.join(REPO, "blender", "export", "scene01_layout.json")) as f:
        layout = json.load(f)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not les.load_level("/Game/Scene01/Lvl_Scene01"):
        raise RuntimeError("no /Game/Scene01/Lvl_Scene01: run import_scene01.py first")
    build(layout["outside"]["portal_face_y"])
    les.save_current_level()
    _log("done")
