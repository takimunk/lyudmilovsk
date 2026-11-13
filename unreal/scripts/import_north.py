"""The North (volcano country, docs/north.md) in Unreal.

  materials: /Game/North/Materials: M_North_Steam (the steam pool's puffs, ALyudNorthSteam: lit translucent, the cigarette
             smoke's T_SmokePuff card, opacity = per-instance custom data 0, grey = data 2), M_North_Plume (the far volcano's
             plume, ALyudNorthPlume: unlit translucent, Color from the game), M_North_Mud (a mud pot's surface: the
             Academgorodok's flat master), M_North_Water (the hot spring: its clear-glass master), MI_NO_<slot> for every
             MAT_SPEC slot in blender/north/*.py (flat, like the restaurant's), M_North_River (the river's sheet,
             ALyudNorthRiver: lit translucent, flowing along V, clear over the gravel at the banks; build_river()).
  atlas:     the props' shared atlas: T_North_Atlas(_N) from blender/north/gen, M_North_Atlas, and M_North_Text_<id> for the
             boards' and signs' words (the GLBs carry material names only, no images); import assigns them by slot name.
  import <file.glb>: one file of blender/export/north (Interchange imports only the first glb of a process; import_north.sh
             runs one process per file) -> /Game/North/Meshes/SM_North_<Kind>[_V<n>] by blender/export/north/manifest.json,
             manifest_hut.json and far.json (far meshes: SM_North_Far_<Id>). Nanite off (glTF import turns it on by itself
             and its parent material lacks the usage), collision by the manifest ("exact": complex as simple; "box": a simple
             box; "none": none). Slots named in a MAT_SPEC get MI_NO_<slot>; the others keep the glTF material Interchange
             made (kept beside the source in /Game/North/Imported/<file>).
  place(face_y): from import_world.py (and `place` here): everything of Content/North/north.json in the open level, tagged
             "North" (lyud.North 0 hides it), folder Outside/North: the camp's things and the signs (StaticMeshActors on
             terrain.ground_z), an ALyudCampfire on the fire ring, an ALyudFumarole per fumarole (the spring is one of them),
             the spring's deck, the ranger's hut with its ALyudDoor (docs/doors.md, place_doors.py), the far volcanoes and
             land; a placeholder cube (and a log line) for every kind whose mesh is not imported yet.

Facing: north.json yaw is the Unreal yaw of a thing's front. A mesh's front in its own frame is manifest "front_yaw" (Unreal
degrees, default 90: Blender's -Y front lands on Unreal +Y, as import_city.py found), so the actor turns by yaw - front_yaw.

Headless:
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script="unreal/scripts/import_north.py materials"
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script="unreal/scripts/import_north.py import tent_dome_v0.glb"
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script="unreal/scripts/import_north.py place"
(unreal/scripts/import_north.sh runs them all.)
"""
import ast
import glob
import json
import math
import os
import shutil
import subprocess
import sys
import traceback

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "tools"))
import terrain as T  # noqa: E402
import world_map as W  # noqa: E402

EXPORT = os.path.join(REPO, "blender", "export", "north")
MANIFESTS = ("manifest.json", "manifest_hut.json", "far.json")
NORTH_JSON = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "North", "north.json")
DEST = "/Game/North"
MESHES = DEST + "/Meshes"
MATS = DEST + "/Materials"
IMPORTED = DEST + "/Imported"
TAG = "North"
FOLDER = "Outside/North"
FRONT_YAW = 90.0                 # Unreal yaw of a Blender-built mesh's front (-Y) in its own frame
NEAR_CM = 20000.0                # small camp things are not drawn farther than this
# rough sizes (m) for the placeholders of kinds not built yet
PLACEHOLDER = {"fire_ring": (1.4, 1.4, 0.3), "tent_dome": (2.4, 2.2, 1.3), "tent_ridge": (2.2, 1.6, 1.2), "tent_tunnel": (3.2, 1.8, 1.1),
               "log_bench": (2.0, 0.4, 0.45), "camp_table": (1.8, 0.8, 0.75), "tarp_shelter": (4.0, 3.0, 2.4), "woodpile": (1.6, 0.8, 0.9),
               "food_pole": (0.3, 0.3, 4.0), "clothes_line": (5.0, 0.1, 1.8), "board_camp": (1.4, 0.2, 2.0), "outhouse": (1.2, 1.2, 2.3),
               "board_reserve": (1.8, 0.2, 2.2), "barrier": (5.0, 0.2, 1.1), "sign_hot_ground": (0.6, 0.1, 1.5), "sign_post": (0.6, 0.1, 1.8),
               "spring_deck": (6.0, 6.0, 0.3), "hut": (5.0, 4.0, 3.6), "hut_interior": (4.0, 3.0, 2.0)}

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty


def log(msg):
    unreal.log_warning("NORTH " + msg)


def _cap(s):
    return s[:1].upper() + s[1:]


def north():
    with open(NORTH_JSON) as f:
        return json.load(f)


# ------------------------------------------------------------------------------------------------ manifests
def manifests():
    """Every mesh the builders listed: dicts with kind, variant, file, collision, size, front_yaw, ... and `asset` (the
    Unreal name it gets). Missing manifests are skipped (the builders run in parallel)."""
    out = []
    for name in MANIFESTS:
        path = os.path.join(EXPORT, name)
        if not os.path.exists(path):
            log("no %s yet: its meshes are skipped" % os.path.relpath(path, REPO))
            continue
        with open(path) as f:
            data = json.load(f)
        rows = data if isinstance(data, list) else data.get("meshes", [])
        for r in rows:
            r = dict(r)
            r["_manifest"] = name
            r["_top"] = data if isinstance(data, dict) else {}
            out.append(r)
    # SM_North_<Kind>, with _V<n> where a kind comes in several variants; the far meshes by id
    counts = {}
    for r in out:
        counts[r.get("kind")] = counts.get(r.get("kind"), 0) + 1
    for r in out:
        kind = r.get("kind", "")
        if kind.startswith("far_"):
            r["asset"] = "SM_North_Far_" + _cap(str(r.get("id") or kind[4:]))
        elif counts.get(kind, 0) > 1 or int(r.get("variant", 0) or 0) > 0:
            r["asset"] = "SM_North_%s_V%d" % (_cap(kind), int(r.get("variant", 0) or 0))
        else:
            r["asset"] = "SM_North_" + _cap(kind)
    return out


def mesh_for(kind, variant=0):
    """The imported mesh for a kind (and variant), or None."""
    for name in ("SM_North_%s_V%d" % (_cap(kind), variant), "SM_North_" + _cap(kind), "SM_North_%s_V0" % _cap(kind)):
        path = "%s/%s" % (MESHES, name)
        if eal.does_asset_exist(path):
            return unreal.load_asset(path)
    return None


def row_for(kind, variant=0):
    rows = [r for r in manifests_cached() if r.get("kind") == kind]
    return next((r for r in rows if int(r.get("variant", 0) or 0) == variant), rows[0] if rows else {})


_CACHE = {}


def manifests_cached():
    if "m" not in _CACHE:
        _CACHE["m"] = manifests()
    return _CACHE["m"]


# ------------------------------------------------------------------------------------------------ materials
def _specs():
    """Every MAT_SPEC dict literal in blender/north/*.py (hex colour, roughness, metallic, emissive), without bpy."""
    out = {}
    for path in sorted(glob.glob(os.path.join(REPO, "blender", "north", "*.py"))):
        try:
            tree = ast.parse(open(path).read())
        except SyntaxError:
            continue
        for n in tree.body:
            if isinstance(n, ast.Assign) and getattr(n.targets[0], "id", "").endswith("MAT_SPEC"):
                try:
                    out.update(ast.literal_eval(n.value))
                except ValueError:
                    log("MAT_SPEC in %s is not a literal" % os.path.basename(path))
    return out


def _lin(hx):
    hx = hx.lstrip("#")
    c = [int(hx[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c]


def build_steam(path):
    """The steam pool's puffs: M_Smoke's graph (import_smoking.py) with a whiter body and a grey for smoke (custom data 2)."""
    import academ_materials as A
    m = A.master(path)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    tex = A.node(m, unreal.MaterialExpressionTextureSample, -900, 0)
    tex.set_editor_property("texture", eal.load_asset("/Game/Smoking/Textures/T_SmokePuff"))
    cd0 = A.node(m, unreal.MaterialExpressionPerInstanceCustomData, -900, 250, data_index=0)
    a = A.node(m, unreal.MaterialExpressionMultiply, -600, 100)
    A.link(tex, "A", a, "A")
    A.link(cd0, "", a, "B")
    fade = A.node(m, unreal.MaterialExpressionDepthFade, -400, 250)
    fade.set_editor_property("fade_distance_default", 30.0)
    A.link(a, "", fade, "Opacity")
    mel.connect_material_property(fade, "", MP.MP_OPACITY)
    cd2 = A.node(m, unreal.MaterialExpressionPerInstanceCustomData, -900, -250, data_index=2)
    col = A.node(m, unreal.MaterialExpressionLinearInterpolate, -400, -200)
    A.link(A.vec(m, "SteamColor", -700, -350, (0.9, 0.91, 0.93)), "", col, "A")
    A.link(A.vec(m, "SmokeColor", -700, -150, (0.32, 0.31, 0.3)), "", col, "B")
    A.link(cd2, "", col, "Alpha")
    mel.connect_material_property(col, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(A.scalar(m, "Roughness", -400, 450, 1.0), "", MP.MP_ROUGHNESS)
    mel.layout_material_expressions(m)
    mel.recompile_material(m)
    eal.save_loaded_asset(m, False)
    return m


def build_plume(path):
    """The far plume: unlit (the translucency lighting volume does not reach 9 km), Color set by ALyudNorthPlume by the sun."""
    import academ_materials as A
    m = A.master(path)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    tex = A.node(m, unreal.MaterialExpressionTextureSample, -900, 0)
    tex.set_editor_property("texture", eal.load_asset("/Game/Smoking/Textures/T_SmokePuff"))
    cd0 = A.node(m, unreal.MaterialExpressionPerInstanceCustomData, -900, 250, data_index=0)
    a = A.node(m, unreal.MaterialExpressionMultiply, -600, 100)
    A.link(tex, "A", a, "A")
    A.link(cd0, "", a, "B")
    mel.connect_material_property(a, "", MP.MP_OPACITY)
    e = A.node(m, unreal.MaterialExpressionMultiply, -400, -150)
    A.link(A.vec(m, "Color", -700, -250, (1.0, 0.98, 0.95)), "", e, "A")
    A.link(tex, "RGB", e, "B")
    mel.connect_material_property(e, "", MP.MP_EMISSIVE_COLOR)
    mel.layout_material_expressions(m)
    mel.recompile_material(m)
    eal.save_loaded_asset(m, False)
    return m


def build_atlas_materials():
    """The props' shared atlas (blender/north/gen, blender/north/northtex.py) imported once as /Game/North/Textures/T_North_Atlas
    (sRGB) and T_North_Atlas_N (normal map), one material M_North_Atlas from them; and M_North_Text_<id> for every board and
    sign text (textures/localized/ru/north_<id>.png: the Russian ones; LyudText::LoadTexture("North", "north_<id>") swaps the
    language). The GLBs carry only these material names, no images; import_file() assigns them by slot name."""
    import academ_materials as A
    gen = os.path.join(REPO, "blender", "north", "gen")
    out = {}
    try:
        d = A.import_image(os.path.join(gen, "north_atlas.png"), DEST, "T_North_Atlas")
        n = A.import_image(os.path.join(gen, "north_atlas_n.png"), DEST, "T_North_Atlas_N")
        n.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        n.set_editor_property("srgb", False)
        eal.save_loaded_asset(d, False)
        eal.save_loaded_asset(n, False)
        m = A.master(MATS + "/M_North_Atlas")
        td = A.node(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, -200, parameter_name="BaseColor", texture=d,
                    sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        tn = A.node(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, 200, parameter_name="Normal", texture=n,
                    sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        mel.connect_material_property(td, "RGB", MP.MP_BASE_COLOR)
        mel.connect_material_property(tn, "RGB", MP.MP_NORMAL)
        mel.connect_material_property(A.scalar(m, "Roughness", -400, 450, 0.85), "", MP.MP_ROUGHNESS)
        A.finish(m)
        out["M_North_Atlas"] = m
    except Exception:       # noqa: BLE001
        log("atlas material failed:\n" + traceback.format_exc())
    texdir = os.path.join(REPO, "textures", "localized", "ru")
    for path in sorted(glob.glob(os.path.join(texdir, "north_*.png"))):
        tid = os.path.splitext(os.path.basename(path))[0][len("north_"):]
        try:
            tex = A.import_image(path, DEST, "T_North_Text_" + tid)
            eal.save_loaded_asset(tex, False)
            out["M_North_Text_" + tid] = A.build_picture("%s/M_North_Text_%s" % (MATS, tid), tex)
        except Exception:   # noqa: BLE001
            log("text material %s failed:\n%s" % (tid, traceback.format_exc()))
    log("atlas + %d text materials" % max(0, len(out) - 1))
    return out


# ------------------------------------------------------------------------------------------------ the river's water
# ALyudNorthRiver's sheet (docs/north.md "River water (game side)"): UV0 = (U across 0..1, V along: metres over the local
# flow speed factor, restarting at multiples of RIVER_REPEAT in every mesh part), vertex colour R = how fast 0..1,
# G = 1 at the sheet's two ends. Every noise below repeats along V every RIVER_REPEAT units (its cells per V unit are
# whole numbers of a 64th), so the parts join without a seam; the pattern rides downstream at Flow V units a second.
RIVER_REPEAT = 64.0
RIVER_LIB = r"""
const float R = %(rep).1f;
struct FRiverNoise
{
    float H(float2 c) { return frac(sin(dot(c, float2(127.1, 311.7))) * 43758.5453); }
    // value noise repeating every Py cells along y
    float N(float2 p, float Py)
    {
        float2 i = floor(p), f = frac(p);
        f = f * f * (3.0 - 2.0 * f);
        float y0 = i.y - Py * floor(i.y / Py);
        float y1 = y0 + 1.0 >= Py ? 0.0 : y0 + 1.0;
        return lerp(lerp(H(float2(i.x, y0)), H(float2(i.x + 1.0, y0)), f.x),
                    lerp(H(float2(i.x, y1)), H(float2(i.x + 1.0, y1)), f.x), f.y);
    }
    // the surface: slow lumps, a cross-running layer and fine ripples (more of them where it is fast), all downstream
    float Hgt(float2 uv, float T, float Flow, float Fast, float Rep)
    {
        float v1 = uv.y - fmod(T * Flow, Rep);
        float v2 = uv.y - fmod(T * Flow * 0.75, Rep);
        float v3 = uv.y - fmod(T * Flow * 1.25, Rep);
        return 0.55 * N(float2(uv.x * 5.0, v1 * 0.5), Rep * 0.5)
             + 0.30 * N(float2(uv.x * 11.0 + 7.0, v2 * 1.5), Rep * 1.5)
             + (0.15 + 0.3 * Fast) * N(float2(uv.x * 26.0 + 3.0, v3 * 4.0), Rep * 4.0);
    }
};
FRiverNoise Rv;
""" % {"rep": RIVER_REPEAT}

RIVER_NORMAL = RIVER_LIB + r"""
float e = 0.02;
float hu = Rv.Hgt(UV + float2(e, 0.0), T, Flow, Fast, R) - Rv.Hgt(UV - float2(e, 0.0), T, Flow, Fast, R);
float hv = Rv.Hgt(UV + float2(0.0, 0.08), T, Flow, Fast, R) - Rv.Hgt(UV - float2(0.0, 0.08), T, Flow, Fast, R);
// fast water is rougher (the glints break up and sparkle), slow reaches nearly glassy
float s = 0.3 + 0.9 * Fast;
return normalize(float3(-hu * 5.0 * s, -hv * 2.0 * s, 1.0));
"""

RIVER_FOAM = RIVER_LIB + r"""
float v = UV.y - fmod(T * Flow * 1.1, R);
float n = 0.65 * Rv.N(float2(UV.x * 18.0, v * 3.0), R * 3.0) + 0.35 * Rv.N(float2(UV.x * 40.0 + 5.0, v * 8.0), R * 8.0);
// a broken line of foam along the banks and streaks in mid-stream, only where the water is fast
float edge = 1.0 - smoothstep(0.0, 0.12, min(UV.x, 1.0 - UV.x));
return saturate(smoothstep(0.62, 0.9, n + Fast * 0.3 + edge * 0.25) * Fast);
"""

RIVER_OPACITY = r"""
// Thick: centimetres of water along the view (the scene behind minus this pixel)
float th = max(Thick, 0.0);
float op = lerp(0.1, 0.72, saturate(th / 150.0));      // clear over the gravel, darker where deep
op = max(op, Foam * 0.9);
op *= saturate(th / 12.0);                             // soft where it meets the bank
op *= smoothstep(0.0, 0.03, UV.x) * smoothstep(0.0, 0.03, 1.0 - UV.x);
op *= 1.0 - Ends;                                      // the first point and the mouth fade
return saturate(op);
"""

RIVER_COLOR = r"""
float m = saturate(max(Thick, 0.0) / 150.0);
float3 c = lerp(Shallow.rgb, Deep.rgb, m);
return lerp(c, float3(0.82, 0.85, 0.85), Foam);
"""


def build_river(path=None):
    """M_North_River: lit translucent (forward shaded for real glints), two-sided (seen from under it too)."""
    import make_sea_materials as S
    F1, F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT1, unreal.CustomMaterialOutputType.CMOT_FLOAT3
    if not eal.does_directory_exist(MATS):
        eal.make_directory(MATS)
    g = S.Graph((path or MATS + "/M_North_River").rsplit("/", 1)[1], dest=MATS)
    m = g.m
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("two_sided", True)
    try:
        m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    except Exception as e:  # noqa: BLE001
        log("river: forward-shaded translucency unavailable (%s), per-pixel lighting" % e)
        m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0)
    t = g.node(unreal.MaterialExpressionTime)
    vc = g.node(unreal.MaterialExpressionVertexColor)
    flow = g.param("Flow", 0.9)
    thick = g.op(unreal.MaterialExpressionSubtract, g.node(unreal.MaterialExpressionSceneDepth),
                 g.node(unreal.MaterialExpressionPixelDepth))
    moving = [("UV", uv), ("T", t), ("Flow", flow), ("Fast", (vc, "R"))]
    foam = g.custom(RIVER_FOAM, moving, desc="RiverFoam")
    g.out(g.custom(RIVER_NORMAL, moving, out=F3, desc="RiverNormal"), MP.MP_NORMAL)
    g.out(g.custom(RIVER_OPACITY, [("UV", uv), ("Thick", thick), ("Foam", foam), ("Ends", (vc, "G"))], desc="RiverOpacity"),
          MP.MP_OPACITY)
    g.out(g.custom(RIVER_COLOR, [("Thick", thick), ("Foam", foam), ("Shallow", g.vparam("Shallow", (0.16, 0.21, 0.18, 1.0))),
                                 ("Deep", g.vparam("Deep", (0.02, 0.06, 0.07, 1.0)))], out=F3, desc="RiverColor"), MP.MP_BASE_COLOR)
    g.out(g.add(g.const(0.03), g.mul(foam, g.const(0.6))), MP.MP_ROUGHNESS)
    g.out(g.const(0.6), MP.MP_SPECULAR)
    mel.layout_material_expressions(m)
    return g.finish()


def build_river_safe():
    """The river's material never stops the other North materials."""
    try:
        build_river()
    except Exception:       # noqa: BLE001
        log("river material failed:\n" + traceback.format_exc())


def make_materials():
    import academ_materials as A
    if not eal.does_asset_exist("/Game/Smoking/Textures/T_SmokePuff"):
        log("no /Game/Smoking/Textures/T_SmokePuff (unreal/scripts/import_smoking.sh): the steam materials need it")
    else:
        build_steam(MATS + "/M_North_Steam")
        build_plume(MATS + "/M_North_Plume")
    A.build_flat(MATS + "/M_North_Mud")
    A.build_glass_clear(MATS + "/M_North_Water")
    build_river_safe()
    out = {}
    specs = _specs()
    if specs:
        flat = A.build_flat(MATS + "/M_NO_Flat")
        for slot, spec in specs.items():
            hx, rough, metal, emissive = (list(spec) + [0.0, 0.0, 0.0])[:4]
            mi = A.instance("%s/MI_NO_%s" % (MATS, slot), flat)
            c = _lin(hx)
            A._set(mi, "v", "Color", unreal.LinearColor(c[0], c[1], c[2], 1))
            A._set(mi, "s", "Roughness", rough)
            A._set(mi, "s", "Metallic", metal)
            A._set(mi, "s", "Emissive", emissive)
            mel.update_material_instance(mi)
            eal.save_loaded_asset(mi, False)
            out[slot] = mi
    build_atlas_materials()
    eal.save_directory(DEST)
    eal.save_directory(MATS)
    log("materials: steam, plume, mud, water, river, %d slot instances" % len(out))
    return out


def slot_materials():
    """MI_NO_<slot> already built (the import step does not rebuild them)."""
    out = {}
    for slot in _specs():
        p = "%s/MI_NO_%s" % (MATS, slot)
        if eal.does_asset_exist(p):
            out[slot] = unreal.load_asset(p)
    # the props' atlas and the boards' text materials (the GLBs carry names only)
    for p in eal.list_assets(MATS, False):
        nm = p.split(".")[0].rsplit("/", 1)[-1]
        if nm == "M_North_Atlas" or nm.startswith("M_North_Text_"):
            out[nm] = unreal.load_asset(p)
    return out


# ------------------------------------------------------------------------------------------------ import
def ensure_far():
    """The far backdrop meshes come from tools/north_far.py (numpy + PIL): run it with the system python3 if missing."""
    if os.path.exists(os.path.join(EXPORT, "far.json")):
        return
    tool = os.path.join(REPO, "tools", "north_far.py")
    py = shutil.which("python3")
    if not os.path.exists(tool) or not py:
        log("no far.json and no %s to make it: the far volcanoes are skipped" % os.path.relpath(tool, REPO))
        return
    try:
        r = subprocess.run([py, tool], cwd=REPO, capture_output=True, text=True, timeout=900)
        log("tools/north_far.py: exit %d %s" % (r.returncode, (r.stdout or "").strip().splitlines()[-1:] or ""))
        if r.returncode:
            log((r.stderr or "")[-800:])
    except Exception as e:      # noqa: BLE001
        log("tools/north_far.py failed: %s" % e)
    _CACHE.clear()


def _manifest_boxes(sm, body, boxes, asset):
    """The manifest's collision_boxes (Blender mesh space, metres: center, size, rot_y_deg = right-handed about Blender +Y,
    positive tipping the +X end down) as the mesh's simple collision. Blender -> glTF (x, z, -y) -> Unreal (glTF x, z, y)
    (Interchange GLTFCore ConversionUtilities.h ConvertVec3), so Blender (x, y, z) m = Unreal (x, -y, z) * 100 cm; the tip
    of +X down is a negative Unreal pitch. A footbridge walks on its deck and ramps, the trestles have none."""
    try:
        elems = []
        for b in boxes:
            c, sz = b["center"], b["size"]
            e = unreal.KBoxElem()
            e.set_editor_property("center", unreal.Vector(c[0] * 100.0, -c[1] * 100.0, c[2] * 100.0))
            e.set_editor_property("rotation", unreal.Rotator(roll=0.0, pitch=-float(b.get("rot_y_deg", 0.0)), yaw=0.0))
            e.set_editor_property("x", sz[0] * 100.0)
            e.set_editor_property("y", sz[1] * 100.0)
            e.set_editor_property("z", sz[2] * 100.0)
            elems.append(e)
        geom = unreal.KAggregateGeom()
        geom.set_editor_property("box_elems", elems)
        body.set_editor_property("agg_geom", geom)
        bb = sm.get_bounding_box()
        ext = bb.max - bb.min
        log("%s: %d collision boxes from the manifest; mesh extent %.0f x %.0f x %.0f cm (X Y Z)" %
            (asset, len(elems), ext.x, ext.y, ext.z))
    except Exception:       # noqa: BLE001
        log("%s: collision boxes failed, simple box instead:\n%s" % (asset, traceback.format_exc()))
        unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).add_simple_collisions(sm, unreal.ScriptCollisionShapeType.BOX)


def import_file(fname):
    """Imports one glb of blender/export/north into the asset its manifest row names."""
    path = os.path.join(EXPORT, fname)
    rows = [r for r in manifests_cached() if os.path.basename(r.get("file", "")) == fname]
    if not os.path.exists(path):
        log("missing %s (built by blender/north or tools/north_far.py): skipped" % os.path.relpath(path, REPO))
        return 0
    if not rows:
        log("%s is in no manifest: skipped" % fname)
        return 0
    row = rows[0]
    asset = row["asset"]
    stem = os.path.splitext(fname)[0]
    tmp = "%s/%s" % (IMPORTED, stem)
    if eal.does_directory_exist(tmp):
        eal.delete_directory(tmp)
    src = unreal.InterchangeManager.create_source_data(path)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_asset(tmp, src, params)
    mgr.wait_until_all_tasks_done(True)
    mats = slot_materials()
    meshes = [p for p in eal.list_assets(tmp, True) if isinstance(eal.load_asset(p), unreal.StaticMesh)]
    if not meshes:
        log("%s: no static mesh came out of the import" % fname)
        return 0
    if len(meshes) > 1:
        log("%s: %d meshes, the first is %s (the builders export one object per file)" % (fname, len(meshes), asset))
    sm = eal.load_asset(meshes[0])
    far = row.get("kind", "").startswith("far_")
    for i, sl in enumerate(sm.get_editor_property("static_materials")):
        key = str(sl.material_slot_name).split(".")[0]
        mi = mats.get(key) or next((v for k, v in sorted(mats.items(), key=lambda kv: -len(kv[0])) if key.startswith(k)), None)
        if mi is not None:
            sm.set_material(i, mi)
    settings = sm.get_editor_property("nanite_settings")
    settings.enabled = False
    sm.set_editor_property("nanite_settings", settings)
    collision = "none" if far else row.get("collision", "box")
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    body = sm.get_editor_property("body_setup")
    if collision == "exact":
        if body:
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    else:
        try:
            sub.remove_collisions(sm)
        except Exception as e:      # noqa: BLE001
            log("%s: remove_collisions: %s" % (asset, e))
        if collision == "box" and row.get("collision_boxes") and body:
            _manifest_boxes(sm, body, row["collision_boxes"], asset)
        elif collision == "box":
            try:
                sub.add_simple_collisions(sm, unreal.ScriptCollisionShapeType.BOX)
            except Exception as e:  # noqa: BLE001
                log("%s: add_simple_collisions: %s" % (asset, e))
        if body:
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    target = "%s/%s" % (MESHES, asset)
    if eal.does_asset_exist(target):
        eal.delete_asset(target)
    eal.save_loaded_asset(sm)
    eal.rename_asset(sm.get_path_name(), target)
    # the glTF's own materials and textures stay in /Game/North/Imported/<file> (the mesh points at them)
    eal.save_directory(DEST)
    log("imported %s -> %s (collision %s)" % (fname, target, collision))
    return 1


def import_all(files=None):
    ensure_far()
    if files is None:
        files = sorted({os.path.basename(r.get("file", "")) for r in manifests_cached() if r.get("file")})
    n = 0
    for f in files:
        n += import_file(f)
    log("imported %d of %d files" % (n, len(files)))


# ------------------------------------------------------------------------------------------------ place
def _apply_common(a, label, sub, tags=("NoGrass", "NoTree")):
    a.set_actor_label(label)
    a.tags = [unreal.Name(t) for t in [TAG] + list(tags)]
    a.set_folder_path(FOLDER + ("/" + sub if sub else ""))


def _ground(x, y):
    return T.ground_z(x, y)


def _rot(yaw):
    return unreal.Rotator(0.0, 0.0, yaw)


def spawn_mesh(kind, variant, x, y, yaw, label, sub, z=None, near=True):
    """A StaticMeshActor of kind at map (x, y) on the ground with its front at Unreal yaw; a placeholder cube if the mesh is
    missing. Returns the actor."""
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    face_y = _CACHE["face_y"]
    gz = _ground(x, y) if z is None else z
    loc = unreal.Vector(*W.to_unreal(x, y, gz, face_y))
    sm = mesh_for(kind, variant)
    row = row_for(kind, variant)
    if sm is None:
        size = row.get("size") or PLACEHOLDER.get(kind, (1.0, 1.0, 1.0))
        a = eas.spawn_actor_from_class(unreal.StaticMeshActor, loc + unreal.Vector(0, 0, size[2] * 50.0), _rot(yaw))
        a.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube.Cube"))
        a.set_actor_scale3d(unreal.Vector(size[0], size[1], size[2]))
        _apply_common(a, "North_Missing_" + label, sub, ("NoGrass", "NoTree", "NorthPlaceholder"))
        _CACHE.setdefault("missing", set()).add(kind)
        return a
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor, loc, _rot(yaw - float(row.get("front_yaw", FRONT_YAW))))
    c = a.static_mesh_component
    c.set_static_mesh(sm)
    c.set_editor_property("lighting_channels", unreal.LightingChannels(True, True, False))
    if near:
        c.set_editor_property("ld_max_draw_distance", NEAR_CM)
    if row.get("collision") == "none":
        c.set_collision_profile_name("NoCollision")
    _apply_common(a, "North_" + label, sub)
    return a


def place_far(face_y):
    """The far backdrop: never walked on, never culled, no shadows, out of Lumen and the distance fields."""
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    vols = {v["id"]: v for v in north().get("far_volcanoes", [])}
    n = 0
    for r in manifests_cached():
        kind = r.get("kind", "")
        if not kind.startswith("far_"):
            continue
        path = "%s/%s" % (MESHES, r["asset"])
        if not eal.does_asset_exist(path):
            log("far mesh %s not imported: skipped" % r["asset"])
            continue
        if kind == "far_volcano":
            v = vols.get(r.get("id"))
            if v is None:
                log("far volcano %s is not in north.json: skipped" % r.get("id"))
                continue
            loc = W.to_unreal(v["x"], v["y"], v["base"], face_y)
        else:
            loc = W.to_unreal(0.0, 0.0, 0.0, face_y)
        a = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc), _rot(0.0))
        c = a.static_mesh_component
        c.set_static_mesh(unreal.load_asset(path))
        c.set_collision_profile_name("NoCollision")
        c.set_editor_property("lighting_channels", unreal.LightingChannels(True, True, False))
        for prop, val in (("cast_shadow", False), ("affect_distance_field_lighting", False), ("affect_dynamic_indirect_lighting", False),
                          ("never_distance_cull", True), ("ld_max_draw_distance", 0.0), ("visible_in_ray_tracing", False)):
            try:
                c.set_editor_property(prop, val)
            except Exception as e:      # noqa: BLE001
                log("far %s: %s: %s" % (r["asset"], prop, e))
        _apply_common(a, "North_Far_" + str(r.get("id", kind)), "Far", ())
        n += 1
    log("far: %d backdrop meshes" % n)


def _hut_door(hut, face_y, hut_yaw):
    """The hut's door: manifest_hut.json's door entry (place_doors.py's format, hut-local Blender metres), else the
    standard hole in the middle of the front wall."""
    import place_doors
    row = row_for("hut")
    top = row.get("_top", {}) if row else {}
    door = row.get("door") or top.get("door")
    size = row.get("size") or PLACEHOLDER["hut"]
    if not isinstance(door, dict) or "at" not in door:
        log("hut: no door entry in manifest_hut.json: the standard door in the middle of the front wall")
        door = {"at": [0.0, -size[1] * 0.5, 0.0], "out": [0.0, 1.0], "hole": [1.2, 2.18], "wall": 0.2}
    entry = {"name": "north_hut", "at": door["at"], "out": door.get("out", [0.0, 1.0]), "hole": door.get("hole", [1.2, 2.18]),
             "wall": door.get("wall", 0.2), "leaves": door.get("leaves", 1), "hinge": door.get("hinge", "left") if isinstance(door.get("hinge"), str) else "left",
             "frame": bool(door.get("frame", False)), "open": False, "locked": False}
    ox, oy, oz = W.to_unreal(hut["x"], hut["y"], _ground(hut["x"], hut["y"]), face_y)
    rot = math.radians(hut_yaw - float(row.get("front_yaw", FRONT_YAW)) if row else hut_yaw - FRONT_YAW)
    c, s = math.cos(rot), math.sin(rot)

    def to_ue(lx, ly, lz):
        # Blender (x, y, z) m -> the mesh's (x, -y, z) cm, turned with the hut
        x, y = lx * 100.0, -ly * 100.0
        return (ox + x * c - y * s, oy + x * s + y * c, oz + lz * 100.0)
    leaf = mesh_for("hut_door")
    doors, _small = place_doors.spawn_doors([entry], to_ue, tags=[TAG], folder=FOLDER + "/Hut", label_prefix="North_Door_", log=log)
    for d in doors:
        if leaf is not None:
            d.set_editor_property("leaf_mesh", leaf)
            d.rebuild()


def _enum(cls, *names):
    for n in names:
        if hasattr(cls, n):
            return getattr(cls, n)
    raise AttributeError("%s has none of %s" % (cls, names))


def place(face_y=None):
    """Spawns the North into the open level (removing what an earlier run placed)."""
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if face_y is None:
        with open(os.path.join(REPO, "blender", "export", "scene01_layout.json")) as f:
            face_y = json.load(f).get("outside", {}).get("portal_face_y", 790.0)
    _CACHE.clear()
    _CACHE["face_y"] = face_y
    removed = 0
    for a in eas.get_all_level_actors():
        if TAG in [str(t) for t in a.tags]:
            eas.destroy_actor(a)
            removed += 1
    data = north()
    n = 0
    # the camp and the signs
    for it in data.get("camp", []):
        spawn_mesh(it["kind"], int(it.get("variant", 0)), it["x"], it["y"], it["yaw"], it["id"], "Camp")
        n += 1
    for it in data.get("signs", []):
        a = spawn_mesh(it["kind"], int(it.get("variant", 0)), it["x"], it["y"], it["yaw"], it["id"], "Signs")
        if it.get("text"):
            a.tags = list(a.tags) + [unreal.Name("Text:" + it["text"])]
        n += 1
    # other things of the land (north.json "things": log bridges over the river's channels, cairns, ...): like the camp's,
    # but a thing with "z" stands at that absolute map height (a bridge's deck over the water), not on the ground
    nthings = 0
    for it in data.get("things", []):
        z = it.get("z")
        if mesh_for(it["kind"], int(it.get("variant", 0))) is None:
            # no placeholder for these (a cube at a bridge's deck height over the water would mislead): skipped until imported
            log("thing %s: no mesh for %s v%d yet, skipped" % (it["id"], it["kind"], int(it.get("variant", 0))))
            continue
        spawn_mesh(it["kind"], int(it.get("variant", 0)), it["x"], it["y"], float(it.get("yaw", 0.0)), it["id"], "Things",
                   z=None if z is None else float(z), near="bridge" not in it["kind"])
        nthings += 1
    # the camp's fire on its ring
    fire = next((it for it in data.get("camp", []) if it["kind"] == "fire_ring"), None)
    if fire:
        cls = unreal.load_class(None, "/Script/Lyudmilovsk.LyudCampfire")
        a = eas.spawn_actor_from_class(cls, unreal.Vector(*W.to_unreal(fire["x"], fire["y"], _ground(fire["x"], fire["y"]), face_y)), _rot(0.0))
        _apply_common(a, "North_Campfire", "Camp")
    # the fumaroles (the spring among them)
    fcls = unreal.load_class(None, "/Script/Lyudmilovsk.LyudFumarole")
    kinds = {"vent": _enum(unreal.LyudFumaroleKind, "VENT"), "steam": _enum(unreal.LyudFumaroleKind, "STEAM"),
             "mudpot": _enum(unreal.LyudFumaroleKind, "MUD_POT", "MUDPOT"), "spring": _enum(unreal.LyudFumaroleKind, "SPRING")}
    for f in data.get("fumaroles", []):
        a = eas.spawn_actor_from_class(fcls, unreal.Vector(*W.to_unreal(f["x"], f["y"], _ground(f["x"], f["y"]), face_y)), _rot(0.0))
        a.set_editor_property("fumarole_id", unreal.Name(f["id"]))
        a.set_editor_property("kind", kinds.get(f["kind"], kinds["vent"]))
        a.set_editor_property("size", float(f["size"]))
        _apply_common(a, "North_Fumarole_%s_%s" % (f["id"], f["kind"]), "Fumaroles",
                      () if f["kind"] == "steam" else ("NoGrass", "NoTree"))
    # the spring's deck
    sp = data.get("spring")
    if sp:
        spawn_mesh("spring_deck", 0, sp["x"], sp["y"], float(sp.get("yaw", 0.0)), "spring_deck", "Spring")
    # the ranger's hut: shell (exact collision), furniture, door
    hut = data.get("hut")
    if hut:
        for kind in ("hut", "hut_interior"):
            a = spawn_mesh(kind, 0, hut["x"], hut["y"], hut["yaw"], kind, "Hut", near=(kind != "hut"))
            if kind == "hut" and mesh_for("hut") is not None:
                a.tags = list(a.tags) + [unreal.Name("Roof")]
        _hut_door(hut, face_y, hut["yaw"])
    place_far(face_y)
    missing = sorted(_CACHE.get("missing", ()))
    if missing:
        log("placeholders for kinds not imported yet: %s" % ", ".join(missing))
    log("placed %d camp things and signs, %d other things, %d fumaroles, campfire %d, hut %d (removed %d old actors)" %
        (n, nthings, len(data.get("fumaroles", [])), 1 if fire else 0, 1 if hut else 0, removed))


def place_safe(face_y):
    """import_world.py's hook: a broken or half-built North never stops the world's build."""
    try:
        place(face_y)
    except Exception:       # noqa: BLE001
        log("place failed:\n" + traceback.format_exc())


if __name__ == "__main__":
    args = sys.argv[1:]
    if "materials" in args:
        make_materials()
    elif "river" in args:
        build_river()
    if "import" in args:
        rest = [a for a in args if a.endswith(".glb")]
        import_all(rest or None)
    if "far" in args:
        ensure_far()
    if "place" in args:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if not les.load_level("/Game/Scene01/Lvl_Scene01"):
            raise RuntimeError("no /Game/Scene01/Lvl_Scene01")
        place()
        les.save_current_level()
        log("done")
