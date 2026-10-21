"""The sky and the sea over the outdoor map: an ALyudSky (sun and moon from the game calendar, physical
atmosphere, volumetric clouds, real-time sky light, star dome) and the Baltic sea, a single-layer water
plane with waves, shoreline surf and depth colour computed in its material.

Run as a script it replaces the old fixed sun, fill light, panorama dome and sea plane of Lvl_Scene01
(build_scene01.sh runs it after import_world.py); build(face_y) does the same in any open level
(make_sky_preview_level.py). Materials go to /Game/Scene01/Sky, rebuilt every time, nothing committed.
"""
import json
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools"))
import world_map as W  # noqa: E402

DEST = "/Game/Scene01/Sky"
eal = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mel = unreal.MaterialEditingLibrary
X = unreal

# The old outdoor lighting this replaces (import_scene01.py, make_world_preview_level.py).
OLD_LABELS = ("Sun", "Sun_Fill", "Outside_SkyDome", "Outside_Sea", "PreviewSea", "Sea", "Sky")
OLD_CLASSES = (unreal.SkyAtmosphere, unreal.SkyLight, unreal.VolumetricCloud)

# The sea reaches the horizon (24 km from 44 m up): 30 km out to the west, 30 km along the coast;
# its land edge runs under the ground a little inland of the nearest water line.
SEA_OUT = 3.0e6
SEA_ALONG = 3.0e6
SEA_INLAND_X = min(x for _, x in W.SHORE) - 100.0      # map metres

# Baltic coastal water: brownish green, cloudy, a couple of metres of visibility (coefficients per metre).
ABSORPTION = (0.40, 0.10, 0.22)
SCATTERING = (0.07, 0.10, 0.08)


def _log(msg):
    unreal.log_warning("SCENE01 SKY " + msg)


class Graph:
    """A new material and small helpers for wiring its nodes."""

    def __init__(self, name, shading=None):
        if eal.does_asset_exist(DEST + "/" + name):
            eal.delete_asset(DEST + "/" + name)
        self.m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, DEST, unreal.Material, unreal.MaterialFactoryNew())
        if shading is not None:
            self.m.set_editor_property("shading_model", shading)
        self.y = 0

    def node(self, cls, x=-600, y=None, **props):
        if y is None:
            self.y += 120
            y = self.y
        e = mel.create_material_expression(self.m, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(self, src, dst, pin="", src_pin=""):
        if not mel.connect_material_expressions(src, src_pin, dst, pin):
            raise RuntimeError("%s: no pin %r on %s" % (self.m.get_name(), pin, dst.get_name()))

    def op(self, cls, a, b, x=-300, a_pin="", b_pin=""):
        e = self.node(cls, x)
        self.link(a, e, "A", a_pin)
        self.link(b, e, "B", b_pin)
        return e

    def const(self, v):
        if isinstance(v, (int, float)):
            return self.node(X.MaterialExpressionConstant, -900, r=float(v))
        return self.node(X.MaterialExpressionConstant3Vector, -900, constant=unreal.LinearColor(v[0], v[1], v[2], 1))

    def param(self, name, v):
        return self.node(X.MaterialExpressionScalarParameter, -900, parameter_name=name, default_value=float(v))

    def custom(self, code, inputs, out=unreal.CustomMaterialOutputType.CMOT_FLOAT1, x=-450):
        c = self.node(X.MaterialExpressionCustom, x, code=code, output_type=out)
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


def sky_material():
    """The star dome: the atmosphere's own luminance, the sun and moon disks, and stars that the day sky outshines."""
    g = Graph("M_LyudSky", unreal.MaterialShadingModel.MSM_UNLIT)
    sky = g.node(X.MaterialExpressionSkyAtmosphereViewLuminance)
    sun = g.node(X.MaterialExpressionSkyAtmosphereLightDiskLuminance, light_index=0)
    moon = g.node(X.MaterialExpressionSkyAtmosphereLightDiskLuminance, light_index=1)
    uv = g.node(X.MaterialExpressionTextureCoordinate, u_tiling=12.0, v_tiling=6.0)
    stars = g.node(X.MaterialExpressionTextureSample, texture=unreal.load_asset("/Engine/EngineSky/T_Sky_Stars"))
    g.link(uv, stars, "UVs")
    cam = g.node(X.MaterialExpressionCameraVectorWS)
    # stars thin out toward the horizon, where the air is thickest
    fade = g.custom("return saturate(-V.z * 3.0 - 0.05);", [("V", cam)])
    bright = g.param("StarBrightness", 1.0)
    k = g.op(X.MaterialExpressionMultiply, g.op(X.MaterialExpressionMultiply, fade, bright), g.const(0.15))
    st = g.op(X.MaterialExpressionMultiply, stars, k, a_pin="RGB")
    em = g.op(X.MaterialExpressionAdd, g.op(X.MaterialExpressionAdd, sky, sun), g.op(X.MaterialExpressionAdd, moon, st))
    g.out(em, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.m.set_editor_property("is_sky", True)
    g.m.set_editor_property("two_sided", True)
    return g.finish()


# Waves: a spectrum of 24 directional wave trains around the wind, from 18 m swell down to ripples, their
# slopes summed per pixel; trains shorter than a few pixels fade out and turn into roughness instead, so
# the far sea goes from glitter to a soft sheen without aliasing. Output: world normal (xyz), roughness (w).
WAVES_HLSL = r"""
float2 p = Pos.xy * 0.01;
float foot = max(length(ddx(p)), length(ddy(p))) + 1e-5;
float2 wd = normalize(float2(WindX, WindY));
// gusts: slow patches of rougher and calmer water drifting downwind
float gust = 0.55 + 0.22 * (sin(dot(p, float2(0.011, 0.006)) - Time * 0.04) + sin(dot(p, float2(-0.004, 0.017)) + Time * 0.03))
           + 0.12 * sin(dot(p, float2(0.031, -0.023)) - Time * 0.09);
float2 g = 0;
float lost = 0;
float L = 18.0;
for (int i = 0; i < 24; i++)
{
    float h = frac(i * 0.6180339 + 0.37);
    float h2 = frac(sin(i * 12.9898 + 1.3) * 43758.5453);
    float ang = (h - 0.5) * 2.6 * (0.6 + 0.4 * h2);
    float ca = cos(ang), sa = sin(ang);
    float2 d = float2(wd.x * ca - wd.y * sa, wd.x * sa + wd.y * ca);
    float k = 6.2831853 / (L * (0.85 + 0.3 * h2));
    float w = sqrt(9.81 * k);
    float s = Steep * gust * (0.5 + 0.8 * frac(h2 * 7.31)) * (1.0 - 0.5 * abs(h - 0.5));
    float ph = k * dot(d, p) - w * Time + h2 * 6.2831853;
    float fade = saturate(2.0 - k * foot * 1.5);
    g += d * (s * cos(ph)) * fade;
    lost += s * s * (1.0 - fade);
    L *= 0.84;
}
float3 n = normalize(float3(-g.x, -g.y, 1.0));
// at grazing views a steep facet would mirror the water below the horizon (a black hole in the
// reflection): tilt such facets back toward flat until they mirror the sky
float3 v = normalize(V);
for (int j = 0; j < 4; j++)
{
    float3 r = 2.0 * dot(n, v) * n - v;
    if (r.z > 0.03) break;
    n = normalize(lerp(n, float3(0, 0, 1), 0.5));
}
return float4(n, saturate(0.03 + sqrt(lost) * 0.9));
"""

# Foam: a strip of surf over the shallows, lines of it running in to the beach, and a broken white edge
# where the water meets the sand. Depth = metres of water under the surface along the vertical.
FOAM_HLSL = r"""
float2 p = Pos.xy * 0.01;
float d = max(Depth, 0.0) * 0.01;
float2 q = p * 1.3 + float2(Time * 0.07, Time * 0.03);
float n = 0, a = 0.5;
for (int o = 0; o < 3; o++)
{
    float2 i = floor(q), f = frac(q);
    f = f * f * (3.0 - 2.0 * f);
    float4 hv;
    float3 p3;
    p3 = frac(float3(i.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); hv.x = frac((p3.x + p3.y) * p3.z);
    p3 = frac(float3((i + float2(1, 0)).xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); hv.y = frac((p3.x + p3.y) * p3.z);
    p3 = frac(float3((i + float2(0, 1)).xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); hv.z = frac((p3.x + p3.y) * p3.z);
    p3 = frac(float3((i + float2(1, 1)).xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); hv.w = frac((p3.x + p3.y) * p3.z);
    n += a * lerp(lerp(hv.x, hv.y, f.x), lerp(hv.z, hv.w, f.x), f.y);
    q = q * 2.03 + 17.1;
    a *= 0.5;
}
n /= 0.875;
float band = saturate(1.0 - d / FoamDepth);
band = sqrt(band);
float surf = sin(d * 5.0 + Time * 1.3 + n * 2.5) * 0.5 + 0.5;
float edge = saturate(1.0 - d / 0.15);
float foam = saturate((surf * band * band - 0.35 + n * 0.5) * 2.5) * band + edge * smoothstep(0.3, 0.6, n);
return saturate(foam);
"""


def sea_material():
    g = Graph("M_BalticSea", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    g.m.set_editor_property("tangent_space_normal", False)
    pos = g.node(X.MaterialExpressionWorldPosition)
    t = g.node(X.MaterialExpressionTime)
    cam = g.node(X.MaterialExpressionCameraVectorWS)
    waves = g.custom(WAVES_HLSL, [("Pos", pos), ("Time", t), ("V", cam), ("Steep", g.param("WaveSteepness", 0.06)),
                                  ("WindX", g.param("WindX", -1.0)), ("WindY", g.param("WindY", 0.35))],
                     out=unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    below = g.node(X.MaterialExpressionSceneDepthWithoutWater, fallback_depth=1.0e6)
    here = g.node(X.MaterialExpressionPixelDepth)
    depth = g.custom("return (Below - Here) * abs(V.z);", [("Below", below), ("Here", here), ("V", cam)])
    foam = g.custom(FOAM_HLSL, [("Pos", pos), ("Time", t), ("Depth", depth), ("FoamDepth", g.param("FoamDepth", 1.6))])

    n = g.node(X.MaterialExpressionComponentMask, r=True, g=True, b=True, a=False)
    g.link(waves, n)
    flat = g.const((0.0, 0.0, 1.0))
    lerp = g.node(X.MaterialExpressionLinearInterpolate)
    g.link(n, lerp, "A")
    g.link(flat, lerp, "B")
    g.link(g.op(X.MaterialExpressionMultiply, foam, g.const(0.6)), lerp, "Alpha")
    g.out(lerp, unreal.MaterialProperty.MP_NORMAL)

    rough = g.node(X.MaterialExpressionComponentMask, r=False, g=False, b=False, a=True)
    g.link(waves, rough)
    g.out(g.op(X.MaterialExpressionAdd, rough, g.op(X.MaterialExpressionMultiply, foam, g.const(0.7))),
          unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(g.op(X.MaterialExpressionMultiply, foam, g.const((0.82, 0.85, 0.82))), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.const(0.255), unreal.MaterialProperty.MP_SPECULAR)

    wo = g.node(X.MaterialExpressionSingleLayerWaterMaterialOutput, x=0)
    for prop, value in ((("Scattering Coefficients", "ScatteringCoefficients"), SCATTERING),
                        (("Absorption Coefficients", "AbsorptionCoefficients"), ABSORPTION),
                        (("Phase G", "PhaseG"), 0.3), (("Color Scale Behind Water", "ColorScaleBehindWater"), 1.0)):
        src = g.const(value)
        if not any(mel.connect_material_expressions(src, "", wo, pin) for pin in prop):
            raise RuntimeError("water output: no pin " + prop[0])
    return g.finish()


def build(face_y, sea_z=W.SEA_Z * 100.0):
    """Puts the sky and the sea into the open level; face_y = the portal face, Unreal Y cm."""
    if not eal.does_directory_exist(DEST):
        eal.make_directory(DEST)
    for a in eas.get_all_level_actors():
        if a.get_actor_label() in OLD_LABELS or isinstance(a, OLD_CLASSES) or a.get_class().get_name() == "LyudSky":
            eas.destroy_actor(a)

    sky_cls = unreal.load_class(None, "/Script/Lyudmilovsk.LyudSky")
    if not sky_cls:
        raise RuntimeError("LyudSky class missing: build the C++ module")
    sky = eas.spawn_actor_from_class(sky_cls, unreal.Vector(0, face_y, sea_z))
    sky.set_actor_label("Sky")
    sky.set_folder_path("Outside")
    sky.set_editor_property("sky_material", sky_material())

    sea = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
    sea.set_actor_label("Sea")
    sea.set_folder_path("Outside")
    c = sea.static_mesh_component
    c.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
    c.set_material(0, sea_material())
    c.set_editor_property("lighting_channels", unreal.LightingChannels(True, True, False))
    c.set_editor_property("cast_shadow", False)
    c.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
    east = SEA_INLAND_X * 100.0
    sea.set_actor_location(unreal.Vector((east + SEA_OUT) * 0.5, face_y, sea_z), False, False)
    sea.set_actor_scale3d(unreal.Vector((SEA_OUT - east) / 100.0, SEA_ALONG * 2.0 / 100.0, 1.0))
    _log("sky + sea at face_y %.0f, sea z %.0f" % (face_y, sea_z))
    return sky, sea


if __name__ == "__main__":
    with open(os.path.join(REPO, "blender", "export", "scene01_layout.json")) as f:
        layout = json.load(f)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not les.load_level("/Game/Scene01/Lvl_Scene01"):
        raise RuntimeError("no /Game/Scene01/Lvl_Scene01: run import_scene01.py first")
    out = layout.get("outside", {})
    build(out.get("portal_face_y", 790.0), out.get("sea_z", W.SEA_Z * 100.0))
    les.save_current_level()
    eal.save_directory(DEST)
    _log("done")
