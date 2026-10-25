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

DEST = "/Game/Scene01/Sky"
eal = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mel = unreal.MaterialEditingLibrary
X = unreal

# The old outdoor lighting this replaces (import_scene01.py, make_world_preview_level.py).
OLD_LABELS = ("Sun", "Sun_Fill", "Outside_SkyDome", "Outside_Sea", "PreviewSea", "Sea", "Sky")
OLD_CLASSES = (unreal.SkyAtmosphere, unreal.SkyLight, unreal.VolumetricCloud)
OLD_CLASS_NAMES = ("LyudSky", "LyudSea")

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
