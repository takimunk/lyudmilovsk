"""The sky and the sea over the outdoor map: an ALyudSky (sun and moon from the game calendar, physical
atmosphere, volumetric clouds, real-time sky light, star dome) and the Baltic sea (ALyudSea: wind-driven
waves, single-layer water, seabed life; its materials come from make_sea_materials.py).

Run as a script it replaces the old fixed sun, fill light, panorama dome and sea plane of Lvl_Scene01
(build_scene01.sh runs it after import_world.py); build(face_y) does the same in any open level
(make_sky_preview_level.py). The sky material goes to /Game/Scene01/Sky, rebuilt every time, nothing committed.
"""
import json
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools"))
import world_map as W  # noqa: E402
sys.path.insert(0, os.path.join(REPO, "tools", "sky"))
import sky_hlsl as SH  # noqa: E402
import cloud_hlsl as CH  # noqa: E402
import rare_hlsl as RH  # noqa: E402

DEST = "/Game/Scene01/Sky"
eal = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mel = unreal.MaterialEditingLibrary
X = unreal

# The old outdoor lighting this replaces (import_scene01.py, make_world_preview_level.py).
OLD_LABELS = ("Sun", "Sun_Fill", "Outside_SkyDome", "Outside_Sea", "PreviewSea", "Sea", "Sky")
OLD_CLASSES = (unreal.SkyAtmosphere, unreal.SkyLight, unreal.VolumetricCloud)
OLD_CLASS_NAMES = ("LyudSky", "LyudSea")

# Europa's surface: our own, procedural, baked by tools/sky/europa_bake.py (seed and parameters in its .json):
# albedo (sRGB) and relief (linear: normal east/north, height)
EUROPA_ALBEDO = os.path.join(REPO, "textures", "sky", "europa_albedo.png")
EUROPA_RELIEF = os.path.join(REPO, "textures", "sky", "europa_relief.png")

# The sea's land edge runs under the ground a little inland of the nearest water line.
SEA_INLAND_X = min(x for _, x in W.SHORE) - 100.0      # map metres



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


def europa_texture(path, name, srgb):
    """One of Europa's baked maps as a texture under DEST (imported every build from the committed PNG)."""
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.load_asset(DEST + "/" + name)
    tex.set_editor_property("srgb", srgb)
    tex.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
    tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    # the relief stays uncompressed: block compression would smear the normals and the heights the shadows march over
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT if srgb else
                            unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    tex.set_editor_property("never_stream", True)
    eal.save_loaded_asset(tex)
    return tex


def high_clouds(g, em, sky, view, sun_dir, moon_dir, vparam):
    """Cirrus, cirrocumulus and cirrostratus as a layer on the dome at their altitude (tools/sky/cloud_hlsl.py
    HIGH_HLSL; ULyudCloudSubsystem sets HighA/B/D), lit by the sun and Europa through the atmosphere."""
    rgba = lambda n: (n, "RGBA")
    high_a = vparam("HighA", (0.0, 9.0, 0.0, 0.0))
    high_b = vparam("HighB", (0.0, 0.0, 0.0, 0.0))
    high_d = vparam("HighD", (0.0, 0.0, 1.0, 0.0))
    cam = g.node(X.MaterialExpressionCameraPositionWS)
    # where the view ray meets the layer (9 km nominal; the light hardly changes over the layer's spread)
    at = g.custom("float3 v = normalize(V); return C + v * (A.y * 1e5 / max(v.z, 0.05));",
                  [("V", view), ("C", cam), ("A", rgba(high_a))], out=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    sun_l = g.node(X.MaterialExpressionSkyAtmosphereLightIlluminance, light_index=0)
    pin = "WorldPosition" if mel.connect_material_expressions(at, "", sun_l, "WorldPosition") else "World Position"
    g.link(at, sun_l, pin)
    moon_l = g.node(X.MaterialExpressionSkyAtmosphereLightIlluminance, light_index=1)
    g.link(at, moon_l, pin)
    return g.custom(CH.HIGH_HLSL, [("Sky", em), ("V", view), ("Cam", cam), ("SunL", sun_l), ("MoonL", moon_l), ("Amb", sky),
                                   ("SunDir", rgba(sun_dir)), ("MoonDir", rgba(moon_dir)), ("HighA", rgba(high_a)),
                                   ("HighB", rgba(high_b)), ("HighD", rgba(high_d))],
                    out=unreal.CustomMaterialOutputType.CMOT_FLOAT3)


def cloud_material():
    """The volumetric clouds: the low and middle layers, lens clouds and rain shafts (tools/sky/cloud_hlsl.py
    VOLUME_HLSL; ULyudCloudSubsystem sets the layer parameters), on the engine's cloud noise volumes."""
    g = Graph("M_LyudClouds")
    g.m.set_editor_property("material_domain", unreal.MaterialDomain.MD_VOLUME)
    g.m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    g.m.set_editor_property("used_with_volumetric_cloud", True)

    def vparam(name, v):
        return g.node(X.MaterialExpressionVectorParameter, -900, parameter_name=name,
                      default_value=unreal.LinearColor(v[0], v[1], v[2], v[3]))
    rgba = lambda n: (n, "RGBA")
    ins = [("P", g.node(X.MaterialExpressionWorldPosition)), ("Cam", g.node(X.MaterialExpressionCameraPositionWS))]
    ins.append(("Shape", g.node(X.MaterialExpressionTextureObject, -900,
                                texture=unreal.load_asset("/Engine/EngineSky/VolumetricClouds/T_VolumeNoiseShape128"))))
    ins.append(("Erosion", g.node(X.MaterialExpressionTextureObject, -900,
                                  texture=unreal.load_asset("/Engine/EngineSky/VolumetricClouds/T_VolumeNoiseErosion32"))))
    defaults = {"A": (0.0, 1.0, 2.0, 1.0), "B": (1.5, 0.0, 0.6, 0.0), "C": (0.5, 1.3, 0.05, 0.0), "D": (0.0, 0.0, 1.0, 0.0),
                "E": (0.0, 0.3, 0.6, 0.0)}
    for li in (0, 1):
        for k, v in defaults.items():
            n = "L%d%s" % (li, k)
            ins.append((n, rgba(vparam(n, v))))
    for n, v in (("Lens", (0.0, 3.5, 4.5, 6.0)), ("LensDir", (1.0, 0.0, 0.0, 0.0)), ("Misc", (0.0, 60.0, 0.0, 0.0)),
                 ("Flash", (0.0, 0.0, 2.0, 3.0))):
        ins.append((n, rgba(vparam(n, v))))
    d = g.custom(CH.VOLUME_HLSL, ins, out=unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    # extinction per metre: density x Sigma x the sky's tuning multiplier (ALyudSky sets Cloud_GlobalDensity)
    sigma = g.param("Sigma", 0.04)
    glob = g.param("Cloud_GlobalDensity", 1.0)
    ext = g.custom("return D.x * S * K;", [("D", d), ("S", sigma), ("K", glob)],
                   out=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    g.out(ext, unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    albedo = vparam("Albedo", (0.93, 0.93, 0.95, 0.0))
    g.out(albedo, unreal.MaterialProperty.MP_BASE_COLOR)
    ao = g.custom("return D.y;", [("D", d)])
    g.out(ao, unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    flash = vparam("FlashColor", (0.75, 0.8, 1.0, 0.0))
    em = g.custom("return D.z * D.x * F.rgb;", [("D", d), ("F", rgba(flash))], out=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    g.out(em, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # two-lobe phase (a bright silver lining toward the sun, a softer back glow) and the multiple-scattering octaves
    # that make thick clouds white instead of grey
    adv = g.node(X.MaterialExpressionVolumetricAdvancedMaterialOutput)
    for k, v in (("const_phase_g", 0.6), ("const_phase_g2", -0.25), ("const_phase_blend", 0.3),
                 ("multi_scattering_approximation_octave_count", 2), ("const_multi_scattering_contribution", 0.55),
                 ("const_multi_scattering_occlusion", 0.45), ("const_multi_scattering_eccentricity", 0.6),
                 ("gray_scale_material", True), ("ground_contribution", False)):
        adv.set_editor_property(k, v)
    return g.finish()


def sky_material():
    """The star dome: the atmosphere's own luminance, the red dwarf and Europa (tools/sky/sky_hlsl.py), the
    night sky's airglow, and stars that the day sky and the two disks outshine."""
    g = Graph("M_LyudSky", unreal.MaterialShadingModel.MSM_UNLIT)
    sky = g.node(X.MaterialExpressionSkyAtmosphereViewLuminance)
    # ALyudSky keeps a low sun's light up but not the sky it would brighten with it (SkyLumScale)
    sky = g.op(X.MaterialExpressionMultiply, sky, g.param("SkyLumScale", 1.0))
    cam = g.node(X.MaterialExpressionCameraVectorWS)
    view = g.custom("return -C;", [("C", cam)], out=unreal.CustomMaterialOutputType.CMOT_FLOAT3)

    def vparam(name, v):
        return g.node(X.MaterialExpressionVectorParameter, -900, parameter_name=name,
                      default_value=unreal.LinearColor(v[0], v[1], v[2], v[3]))
    sun_dir = vparam("SunDir", (0.0, 0.0, 1.0, 0.0))
    moon_dir = vparam("MoonDir", (0.0, 0.7, 0.7, 0.0))
    pole = vparam("Pole", (0.0, 0.57, 0.82, 0.0))
    sun_disk = vparam("SunDisk", (0.0873, 3.6, 0.0, 0.0))
    moon_disk = vparam("MoonDisk", (0.061, 0.14, 0.012, 0.0))
    glow = vparam("NightGlow", (0.0, 0.0, 0.0, 0.0))
    spots = vparam("SunSpots", (0.0, 0.08, 1.0, 0.8))
    moon_look = vparam("MoonLook", (1.6, 0.004, 0.0, 0.0))
    t = g.node(X.MaterialExpressionTime)
    europa = g.node(X.MaterialExpressionTextureObjectParameter, -900, parameter_name="EuropaAlbedo",
                    texture=europa_texture(EUROPA_ALBEDO, "T_EuropaAlbedo", True))
    relief = g.node(X.MaterialExpressionTextureObjectParameter, -900, parameter_name="EuropaRelief",
                    texture=europa_texture(EUROPA_RELIEF, "T_EuropaRelief", False))
    rgba = lambda n: (n, "RGBA")
    tr = g.custom(SH.TRANS_HLSL, [("Dir", view)], out=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    # the star's activity (ULyudSunActivity sets these): active regions, prominences, the flare
    sun_ins = [("V", view), ("S", rgba(sun_dir)), ("P", rgba(pole)), ("D", rgba(sun_disk)), ("Spots", rgba(spots)), ("T", t),
               ("Tr", tr), ("Look", rgba(vparam("SunLook", (1.0, 0.25, 1.0, 1.0)))),
               ("Look2", rgba(vparam("SunLook2", (2.2, 0.0, 0.0, 0.0))))]
    sun_ins += [("R%d" % i, rgba(vparam("SunRegion%d" % i, (0.0, 0.0, 0.0, 0.0)))) for i in range(SH.SUN_REGIONS)]
    for k in "ABC":
        sun_ins += [("P%s%d" % (k, i), rgba(vparam("SunProm%s%d" % (k, i), (0.0, 0.0, 1.0, 0.0)))) for i in range(SH.SUN_PROMS)]
    sun = g.custom(SH.LIB + SH.SUN_HLSL, sun_ins, out=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    moon = g.custom(SH.MOON_HLSL, [("V", view), ("M", rgba(moon_dir)), ("S", rgba(sun_dir)), ("P", rgba(pole)),
                                   ("D", rgba(moon_disk)), ("C", rgba(moon_look)), ("Tr", tr), ("Europa", europa), ("Relief", relief)],
                    out=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    # Europa passes in front of the sun at new moons (both disks are big): the sun goes behind it
    cover = g.custom(SH.MOON_COVER_HLSL, [("V", view), ("M", rgba(moon_dir)), ("MD", rgba(moon_disk))])
    sun = g.op(X.MaterialExpressionMultiply, sun, cover)
    uv = g.node(X.MaterialExpressionTextureCoordinate, u_tiling=12.0, v_tiling=6.0)
    stars = g.node(X.MaterialExpressionTextureSample, texture=unreal.load_asset("/Engine/EngineSky/T_Sky_Stars"))
    g.link(uv, stars, "UVs")
    # stars thin out toward the horizon, where the air is thickest, and near the two disks
    fade = g.custom(SH.STAR_MASK_HLSL, [("V", view), ("M", rgba(moon_dir)), ("S", rgba(sun_dir)),
                                        ("MD", rgba(moon_disk)), ("SD", rgba(sun_disk))])
    bright = g.param("StarBrightness", 1.0)
    k = g.op(X.MaterialExpressionMultiply, g.op(X.MaterialExpressionMultiply, fade, bright), g.const(0.15))
    st = g.op(X.MaterialExpressionMultiply, stars, k, a_pin="RGB")
    # the airglow: brighter toward the horizon, where the line of sight runs through more of the glowing layer
    ag = g.custom("float z = saturate(normalize(V).z); return G.rgb * (0.55 + 0.9 * exp(-z * 5.0)) * step(-0.2, normalize(V).z);",
                  [("V", view), ("G", rgba(glow))], out=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    # over the two disks most of the sky's own glow is taken off: the haze in front of a far brighter real sun would
    # not show, and added to the capped disk it pales the deep red-orange to pink
    over = g.custom(SH.DISK_MASK_HLSL, [("V", view), ("S", rgba(sun_dir)), ("D", rgba(sun_disk)), ("M", rgba(moon_dir)),
                                        ("MD", rgba(moon_disk))])
    sky = g.op(X.MaterialExpressionMultiply, sky, over)
    em = g.op(X.MaterialExpressionAdd, g.op(X.MaterialExpressionAdd, sky, sun),
              g.op(X.MaterialExpressionAdd, g.op(X.MaterialExpressionAdd, moon, st), ag))
    em = high_clouds(g, em, sky, view, sun_dir, moon_dir, vparam)
    # the rare sky (tools/sky/rare_hlsl.py, ALyudRareSky sets these): the aurora, noctilucent clouds, the lightning's
    # glow and the red dwarf's flares
    f3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
    aur = g.custom(RH.AURORA_HLSL, [("V", view), ("A", rgba(vparam("AurA", (0.0, 2000.0, 300.0, 0.0)))),
                                    ("B", rgba(vparam("AurB", (0.0, 0.4, 0.0, 105.0)))),
                                    ("C", rgba(vparam("AurC", (-0.27, 0.96, 0.0, 1.0)))), ("T", t), ("Tr", tr)], out=f3)
    nlc = g.custom(RH.NLC_HLSL, [("V", view), ("S", rgba(sun_dir)), ("N", rgba(vparam("NlcA", (0.0, 0.0, 0.0, 0.0)))),
                                 ("T", t), ("Tr", tr)], out=f3)
    flash = g.custom(RH.FLASH_HLSL, [("V", view), ("F", rgba(vparam("Flash", (0.0, 1.0, 0.2, 0.0))))], out=f3)
    flare = g.custom(RH.FLARE_HLSL, [("V", view), ("S", rgba(sun_dir)), ("P", rgba(pole)), ("D", rgba(sun_disk)),
                                     ("F", rgba(vparam("Flare", (0.3, 0.0, 0.0, 0.0)))), ("Spots", rgba(spots)), ("Tr", tr)], out=f3)
    # the activity's everyday flares, drawn the same way (ULyudSunActivity sets SunFlare)
    flare2 = g.custom(RH.FLARE_HLSL, [("V", view), ("S", rgba(sun_dir)), ("P", rgba(pole)), ("D", rgba(sun_disk)),
                                      ("F", rgba(vparam("SunFlare", (0.3, 0.0, 0.0, 0.0)))), ("Spots", rgba(spots)), ("Tr", tr)], out=f3)
    flare = g.op(X.MaterialExpressionAdd, flare, flare2)
    rare = g.op(X.MaterialExpressionAdd, g.op(X.MaterialExpressionAdd, aur, nlc), g.op(X.MaterialExpressionAdd, flash, flare))
    em = g.op(X.MaterialExpressionAdd, em, rare)
    g.out(em, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.m.set_editor_property("is_sky", True)
    g.m.set_editor_property("two_sided", True)
    return g.finish()


def _translucent_dome(name, code, inputs):
    """An unlit additive material for a camera-centred sphere (ALyudRareSky): fogged by the clouds in front of it."""
    g = Graph(name, unreal.MaterialShadingModel.MSM_UNLIT)
    g.m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    g.m.set_editor_property("two_sided", True)
    for prop in ("apply_cloud_fogging", "compute_fog_per_pixel"):
        try:
            g.m.set_editor_property(prop, True)
        except Exception as e:                       # property names move between engine versions
            _log("%s: no %s (%r)" % (name, prop, e))
    cam = g.node(X.MaterialExpressionCameraVectorWS)
    view = g.custom("return -C;", [("C", cam)], out=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = [("V", view)]
    for pin, src in inputs:
        if isinstance(src, tuple) and src[0] == "vec":
            n = g.node(X.MaterialExpressionVectorParameter, -900, parameter_name=src[1],
                       default_value=unreal.LinearColor(*src[2]))
            ins.append((pin, (n, "RGBA")))
        else:
            ins.append((pin, src(g)))
    em = g.custom(code, ins, out=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    g.out(em, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return g.finish()


def optics_material():
    """Halos, sun dogs, pillars and rainbows (rare_hlsl.OPTICS_HLSL) on a sphere ~3 km round the camera."""
    return _translucent_dome("M_LyudSkyOptics", RH.OPTICS_HLSL, [
        ("Ld", ("vec", "Light", (0.0, -0.7, 0.3, 0.087))), ("Lc", ("vec", "LightColor", (0.0, 0.0, 0.0, 0.0))),
        ("Hs", ("vec", "Halo", (0.0, 0.0, 0.0, 0.0))), ("Rb", ("vec", "Rainbow", (0.0, 0.0, 0.0, 0.0)))])


def lightning_material():
    """A cloud-to-ground bolt (rare_hlsl.BOLT_HLSL) on a sphere just short of the strike."""
    return _translucent_dome("M_LyudLightning", RH.BOLT_HLSL, [
        ("Cam", lambda g: g.node(X.MaterialExpressionCameraPositionWS)),
        ("Bt", ("vec", "Bolt", (0.0, 0.0, 0.0, 0.0))), ("Bb", ("vec", "BoltLook", (150000.0, 0.0, 1.0, 1.0)))])


def build(face_y, sea_z=W.SEA_Z * 100.0):
    """Puts the sky and the sea into the open level; face_y = the portal face, Unreal Y cm."""
    if not eal.does_directory_exist(DEST):
        eal.make_directory(DEST)
    for a in eas.get_all_level_actors():
        if a.get_actor_label() in OLD_LABELS or isinstance(a, OLD_CLASSES) or a.get_class().get_name() in OLD_CLASS_NAMES:
            eas.destroy_actor(a)

    sky_cls = unreal.load_class(None, "/Script/Lyudmilovsk.LyudSky")
    if not sky_cls:
        raise RuntimeError("LyudSky class missing: build the C++ module")
    sky = eas.spawn_actor_from_class(sky_cls, unreal.Vector(0, face_y, sea_z))
    sky.set_actor_label("Sky")
    sky.set_folder_path("Outside")
    sky.set_editor_property("sky_material", sky_material())
    sky.set_editor_property("cloud_material", cloud_material())
    # the rare sky's own materials: ALyudRareSky (spawned by its subsystem at play) loads them from DEST
    optics_material()
    lightning_material()

    # the sea: ALyudSea (waves from the wind, depth from the ground, seabed life; its materials are
    # committed assets from make_sea_materials.py), with no sea east of the old plane's land edge
    sea_cls = unreal.load_class(None, "/Script/Lyudmilovsk.LyudSea")
    sea = eas.spawn_actor_from_class(sea_cls, unreal.Vector(0, face_y, sea_z))
    sea.set_actor_label("Sea")
    sea.set_folder_path("Outside")
    sea.set_editor_property("land_side_x", SEA_INLAND_X * 100.0)
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
