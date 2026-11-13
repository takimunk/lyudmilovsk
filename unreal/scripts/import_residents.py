"""The townspeople's own models (blender/residents, docs/residents.md) into /Game/Characters/Humans/Town:

  Town/Textures/T_Resident_{Skin,Cloth,Hair,Eyes}      the four shared atlases (blender/residents/atlases)
  Town/Materials/M_Resident_Opaque, M_Resident_Masked  the masters (the masked one for Hair)
  Town/Materials/MI_Resident_{Skin,Cloth,Hair,Eyes}    one instance per material section, shared by everybody
  Town/Import/IP_ResidentAssets                        the glTF assets pipeline bound to SK_Human_Skeleton
  Town/LOD_Resident                                    the residents' four LODs (LOD_GROUPS)
  Town/R_<key>/SK_R_<key>, SK_R_<key>_PhysicsAsset     each blender/residents/export/R_<key>.glb on SK_Human_Skeleton

Normally run by unreal/scripts/import_residents.sh (UE queue, low priority, one editor run per chunk until nothing is
left). By hand, the editor closed on this checkout:
  UnrealEditor-Cmd <uproject> -run=pythonscript -script="unreal/scripts/import_residents.py [--prepare] [--only key,key]
      [--limit N] [--chunk 60] [--force] [--retry-failed] [--prune]" -unattended -nullrhi
  python3 unreal/scripts/import_residents.py --report      (no engine: what is in, what is left, what failed and why)

A person's colours ride in UV channels 1-2, not in vertex colours (their sRGB/linear handling differs between exporters
and importers). In Blender: T = Atlas(UV0), tint = (UV1.x, UV1.y, UV2.x), amount = UV2.y (x T.a on Cloth), BaseColor =
lerp(T, T * 2 * tint, amount). Blender's glTF exporter writes every UV channel with V flipped and Interchange keeps
glTF's top-left convention, so here the tint channels read 1 - y (the masters' static switch TintFlipV, on). UV0 needs
nothing: textures are sampled top-left too. The exports agree: R_abenov_malgorzata's hair is stored (0.55, 0.57 | 0.25,
0.0), light brown (0.55, 0.43, 0.25) at amount 1 flipped; read raw, every tinted part would get amount 0 (the atlas's
untinted grey) and a green tint. So if a test import shows grey or green people, TintFlipV is the wrong way round:
switch it in the two masters; no mesh needs re-importing.

Incremental: each mesh carries its sidecar's "<hash>/<build>" as metadata tag ResidentLook, and
Saved/ImportResidents/models/<Model>.json what its last import did, so a run needn't load 898 meshes to find the few that
changed (a mesh with no record there is loaded and its tag read). --force re-imports what was imported before --since
(the wrapper passes its own start, so its runs don't redo each other's work); --retry-failed likewise for failures.
Interchange here often takes only one .glb per process (import_fish.py, import_animals.py, import_humans.py): a run stops
at the first later import that comes back empty, the next process carries on, and the rest of the session runs as many
per process as worked. RESIDENTS_WAIT_CANCEL=0 waits for Interchange without cancelling (wait_until_all_tasks_done(False);
the repo's scripts all pass True): worth one test with --limit 3 to see whether more imports then fit in a process.
"""
import argparse
import hashlib
import json
import os
import random
import shutil
import struct
import sys
import time
import traceback

try:
    import unreal
except ImportError:                    # --report runs outside the engine
    unreal = None

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
EXPORT = os.path.join(ROOT, "blender", "residents", "export")
ATLASES = os.path.join(ROOT, "blender", "residents", "atlases")
PROJECT = os.path.join(ROOT, "unreal", "Lyudmilovsk")
CONTENT = os.path.join(PROJECT, "Content")
LOOKS = os.path.join(CONTENT, "PeopleData", "town_looks.json")
STATE = os.path.join(PROJECT, "Saved", "ImportResidents")
BASE = "/Game/Characters/Humans"
SKELETON = BASE + "/SK_Human_Skeleton"
TOWN = BASE + "/Town"
LODS = TOWN + "/LOD_Resident"
# the residents' own LODs: the cast's LOD_HumanPeople (lod_people.py) caps LOD3 at 5000 triangles for its 55-70k meshes,
# which on a 24k resident would be heavier than its LOD2 (15 % = 3600)
LOD_GROUPS = ((1.0, 100.0), (0.45, 45.0), (0.2, 18.0), (0.08, 6.0))   # screen size, % triangles: 24k, 11k, 4.3k, 1.4k
# (changed later: LOD_Resident follows by itself, the meshes already in keep their LODs until imported with --force)
TEXTURES = TOWN + "/Textures"
MATS = TOWN + "/Materials"
PIPELINE = TOWN + "/Import/IP_ResidentAssets"
TMP = TOWN + "/_import"                # + "_<i>of<n>" for a shard: parallel runs clear only their own leftovers
SECTIONS = ("Skin", "Cloth", "Hair", "Eyes")
MORPHS = ("eyeBlinkLeft", "eyeBlinkRight", "jawOpen", "mouthFunnel", "mouthSmile", "browUp")
TAG = "ResidentLook"                   # on SK_R_<key>: the sidecar's "<hash>/<build>/<atlas layout>"
SPEC = "ResidentSpec"                  # on the shared assets: what they were made from
MASTER_VERSION = 1                     # bump when the master graphs change: they are rebuilt in place
TINT_FLIP_V = True                     # the masters' TintFlipV default (see above)
MAX_TRIES = 2                          # a model that fails (or takes the editor down) this often is left out
CHECKS_PER_RUN = 200                   # meshes a run loads only to read their tag
WAIT_CANCEL = os.environ.get("RESIDENTS_WAIT_CANCEL", "1") != "0"

# section: master, roughness, specular, subsurface tint (None: not subsurface), the atlas alpha is the tint mask
LOOK = {
    # skin subsurface like the cast's M_HumanFace: Virtual Shadow Maps give subsurface a wide penumbra, so the decimated
    # body doesn't shadow itself in hard triangles; the scatter is the skin's own colour, a little redder
    "Skin": ("Opaque", 0.55, 0.35, (1.0, 0.5, 0.4), False),
    "Cloth": ("Opaque", 0.85, 0.4, (0.0, 0.0, 0.0), True),
    "Hair": ("Masked", 0.5, 0.3, None, False),
    "Eyes": ("Opaque", 0.2, 0.5, (0.0, 0.0, 0.0), False),     # irises, teeth, tongue: wet, but not a mirror
}

if unreal:
    eal = unreal.EditorAssetLibrary
    mel = unreal.MaterialEditingLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    MP = unreal.MaterialProperty
BROKEN = []                            # material links and parameters that didn't take


def log(msg):
    if unreal:
        unreal.log_warning("RESIDENTS " + msg)
    else:
        print("RESIDENTS " + msg, flush=True)


# ------------------------------------------------------------------------------------------- files, plan

def read_json(path):
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


def write_json(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = "%s.%d.tmp" % (path, os.getpid())
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=1, sort_keys=True)
    os.replace(tmp, path)


def state_path(model):
    return os.path.join(STATE, "models", model + ".json")


def set_state(name, **kw):
    s = read_json(state_path(name)) or {}
    s.update(kw)
    write_json(state_path(name), s)
    return s


def home(model):
    """A model's folder: a townsperson's R_<key> in Town/, a named person's (looks.py CAST, named after their
    people.json body) where the game looks for it, /Game/Characters/Humans/<Body>."""
    return "%s/%s" % (TOWN if model.startswith("R_") else BASE, model)


def disk(path):
    """/Game/a/b -> <Content>/a/b (a folder; + .uasset for a package)."""
    return os.path.join(CONTENT, *path[len("/Game/"):].split("/"))


def glb_ready(glb, side):
    """A finished export: build_residents.py writes the sidecar after the glb, so a glb newer than its sidecar is
    being rebuilt; and a whole GLB (its header's length is the file's)."""
    try:
        if os.path.getmtime(glb) > os.path.getmtime(side) + 2.0:
            return False
        with open(glb, "rb") as f:
            magic, _, length = struct.unpack("<III", f.read(12))
        return magic == 0x46546C67 and length == os.path.getsize(glb)
    except (OSError, struct.error):
        return False


def town():
    """[(key, model)] from town_looks.json (the game's list), else from the sidecars there are; and whether it was the
    former."""
    looks = read_json(LOOKS)
    if looks and looks.get("people"):
        return sorted((k, v["model"].split("/")[-1]) for k, v in looks["people"].items()), True
    out = []
    for f in sorted(os.listdir(EXPORT)) if os.path.isdir(EXPORT) else []:
        if f.startswith("R_") and f.endswith(".json"):
            out.append(((read_json(os.path.join(EXPORT, f)) or {}).get("key", f[:-5]), f[:-5]))
    return out, False


def classify(c, args, since):
    """What a run does with a candidate: import, check (load the mesh, read its tag), up to date, failed (left out),
    not built (no glb or sidecar yet), not ready (being written)."""
    st = c["state"]
    if c["side"] is None:
        return "not built" if not (os.path.isfile(c["glb"]) and os.path.isfile(c["side_path"])) else "not ready"
    if not glb_ready(c["glb"], c["side_path"]) or c["side"].get("model") != c["model"]:
        return "not ready"
    have = os.path.isfile(disk(c["asset"]) + ".uasset")
    redo = args.force and st.get("at", 0) < since
    if have and st.get("status") == "imported" and st.get("tag") == c["tag"] and not redo:
        return "up to date"
    if (st.get("status") == "failed" and st.get("tag") == c["tag"] and st.get("tries", 0) >= MAX_TRIES and
            not ((args.retry_failed or args.force) and st.get("at", 0) < since)):
        return "failed"
    if have and not args.force and not (st.get("status") == "imported" and st.get("tag") != c["tag"]):
        return "check"
    return "import"


def plan(args, since):
    people, _ = town()
    if args.only:
        want = {w.strip() for w in args.only.split(",") if w.strip()}
        people = [(k, m) for k, m in people if k in want or m in want or "Town/" + m in want]
    if args.limit:
        people = people[:args.limit]
    if args.shard:
        i, n = (int(x) for x in args.shard.split("/"))
        people = people[i::n]
    out = []
    for key, model in people:
        c = dict(key=key, model=model, glb=os.path.join(EXPORT, model + ".glb"),
                 side_path=os.path.join(EXPORT, model + ".json"), asset="%s/SK_%s" % (home(model), model),
                 state=read_json(state_path(model)) or {})
        c["side"] = read_json(c["side_path"]) if os.path.isfile(c["glb"]) else None
        # the atlas layout's id too: a model built against another layout points into the wrong tiles
        c["tag"] = "%s/%s/%s" % (c["side"].get("hash"), c["side"].get("build"), c["side"].get("atlas", "-")) \
            if c["side"] else None
        c["todo"] = classify(c, args, since)
        out.append(c)
    return out


def left_of(cands):
    return sum(1 for c in cands if c["todo"] in ("import", "check"))


def report(cands, final):
    """The tally from the state files: what's in, failed (and why), missing materials, morph targets, LODs."""
    by = {}
    for c in cands:
        by.setdefault(c["todo"], []).append(c)
    log("%s: %d people, %d up to date, %d to import, %d to check, %d failed, %d not built yet, %d being written" % (
        "town" if final else "selection", len(cands), len(by.get("up to date", [])), len(by.get("import", [])),
        len(by.get("check", [])), len(by.get("failed", [])), len(by.get("not built", [])), len(by.get("not ready", []))))
    for c in by.get("failed", [])[:40]:
        log("  failed %s (%d tries): %s" % (c["model"], c["state"].get("tries", 0), c["state"].get("error", "?")))
    done = [c["state"] for c in by.get("up to date", [])]
    missing = ["%s %s" % (s.get("model"), ",".join(s.get("missing"))) for s in done if s.get("missing")]
    log("missing materials: %s" % (("; ".join(missing[:20]) + (" ..." if len(missing) > 20 else "")) if missing else
                                   "none (every section on its MI_Resident_*)"))
    bad = ["%s (%d)" % (s.get("model"), len(s.get("morphs", []))) for s in done
           if sorted(s.get("morphs", [])) != sorted(MORPHS)]
    log("morph targets: %d of %d meshes have the six face morphs%s" % (
        len(done) - len(bad), len(done), ("; not: " + ", ".join(bad[:20])) if bad else ""))
    lods = ["%s (%s)" % (s.get("model"), s.get("lods")) for s in done if s.get("lods") != len(LOD_GROUPS)]
    if lods:
        log("LODs: not %d on %s" % (len(LOD_GROUPS), ", ".join(lods[:20])))
    looks = read_json(LOOKS) or {}
    stale = [c for c in by.get("up to date", []) if c["state"].get("look") and c["state"]["look"] != (
        looks.get("people", {}).get(c["key"], {}).get("hash", c["state"]["look"]))]
    if stale:
        log("note: %d imported models have a newer look in town_looks.json (rebuild them: build_residents.py)" % len(stale))
    if final and looks.get("people") and os.path.isdir(EXPORT):
        listed = {v["model"].split("/")[-1] for v in looks["people"].values()}
        extra = [f for f in os.listdir(EXPORT) if f.startswith("R_") and f.endswith(".glb") and f[:-4] not in listed]
        if extra:
            log("note: %d built models aren't in town_looks.json: not imported (--prune deletes what was imported of "
                "them before)" % len(extra))


# ------------------------------------------------------------------------------------- shared assets (UE)

def sha1(path):
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()[:16]


def atlas(section, force):
    """T_Resident_<Section> with the classic texture factory (Interchange imports only one PNG per process here),
    re-imported only when the PNG changed."""
    png = os.path.join(ATLASES, "T_Resident_%s.png" % section)
    name = "T_Resident_" + section
    path = TEXTURES + "/" + name
    if not os.path.isfile(png):
        raise RuntimeError("no %s: Blender -b -P blender/residents/atlases.py" % png)
    sha = sha1(png)
    old = ""
    if eal.does_asset_exist(path):
        t = unreal.load_asset(path)
        old = (eal.get_metadata_tag(t, SPEC) or "") if t else ""
        if not force and old == sha:
            return t, False
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", png)
    task.set_editor_property("destination_path", TEXTURES)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    task.set_editor_property("factory", unreal.TextureFactory())
    tools.import_asset_tasks([task])
    t = unreal.load_asset(path)
    if t is None:
        raise RuntimeError("texture import failed: " + png)
    t.set_editor_property("srgb", True)
    # BC7 keeps the alpha (Cloth: the tint mask, Hair: the strands) at a byte a texel; one set serves the whole town, so
    # BC7's better colour over DXT1/5 is paid once
    t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
    t.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
    if section == "Hair":
        # alpha-tested strands thin out in the small mips (bald heads a street away) unless the mips keep the share of
        # texels over the clip value
        try:
            t.set_editor_property("do_scale_mips_for_alpha_coverage", True)
            t.set_editor_property("alpha_coverage_thresholds", unreal.Vector4(0.0, 0.0, 0.0, 0.5))
        except Exception as e:                                          # noqa: BLE001
            log("  %s: no alpha coverage mips (%s)" % (name, e))
    eal.set_metadata_tag(t, SPEC, sha)
    eal.save_loaded_asset(t, only_if_is_dirty=False)
    log("  %s %dx%d imported" % (name, t.blueprint_get_size_x(), t.blueprint_get_size_y()))
    return t, bool(old) and old != sha


def node(m, cls, x, y, **props):
    e = mel.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(m, src, out, dst, pins):
    """src's output into the first of dst's pins (| separated) that takes it."""
    for pin in pins.split("|"):
        if mel.connect_material_expressions(src, out, dst, pin):
            return
    BROKEN.append("%s: %s.%s -> %s.%s" % (m.get_name(), src.get_name(), out, dst.get_name(), pins))


def tint_graph(m):
    """BaseColor = lerp(T.rgb, T.rgb * 2 * tint, amount); returns the atlas sample and the base colour."""
    X = unreal
    uv0 = node(m, X.MaterialExpressionTextureCoordinate, -2200, -400, coordinate_index=0)
    t = node(m, X.MaterialExpressionTextureSampleParameter2D, -1950, -400, parameter_name="Atlas",
             texture=unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
    link(m, uv0, "", t, "UVs")
    # (r, g, b, amount) as the mesh carries them: UV1 = (r, g), UV2 = (b, amount), each V written flipped by glTF
    uv1 = node(m, X.MaterialExpressionTextureCoordinate, -2200, 100, coordinate_index=1)
    uv2 = node(m, X.MaterialExpressionTextureCoordinate, -2200, 220, coordinate_index=2)
    raw = node(m, X.MaterialExpressionAppendVector, -1950, 150)
    link(m, uv1, "", raw, "A")
    link(m, uv2, "", raw, "B")
    # (r, 1 - g, b, 1 - amount)
    neg = node(m, X.MaterialExpressionMultiply, -1750, 250)
    link(m, raw, "", neg, "A")
    link(m, node(m, X.MaterialExpressionConstant4Vector, -1950, 300, constant=unreal.LinearColor(1.0, -1.0, 1.0, -1.0)),
         "", neg, "B")
    unflip = node(m, X.MaterialExpressionAdd, -1550, 250)
    link(m, neg, "", unflip, "A")
    link(m, node(m, X.MaterialExpressionConstant4Vector, -1750, 400, constant=unreal.LinearColor(0.0, 1.0, 0.0, 1.0)),
         "", unflip, "B")
    flip = node(m, X.MaterialExpressionStaticSwitchParameter, -1350, 150, parameter_name="TintFlipV",
                default_value=TINT_FLIP_V)
    link(m, unflip, "", flip, "True|A")
    link(m, raw, "", flip, "False|B")
    tint = node(m, X.MaterialExpressionComponentMask, -1150, 50, r=True, g=True, b=True, a=False)
    link(m, flip, "", tint, "")
    amount = node(m, X.MaterialExpressionComponentMask, -1150, 250, r=False, g=False, b=False, a=True)
    link(m, flip, "", amount, "")
    # Cloth: texels painted to keep their colour (shoes, a white shirt under a suit) have alpha 0
    masked = node(m, X.MaterialExpressionMultiply, -950, 350)
    link(m, amount, "", masked, "A")
    link(m, t, "A", masked, "B")
    am = node(m, X.MaterialExpressionStaticSwitchParameter, -750, 250, parameter_name="AlphaIsTintMask",
              default_value=False)
    link(m, masked, "", am, "True|A")
    link(m, amount, "", am, "False|B")
    # tinted parts are grey detail with a linear mean of 0.5: T * 2 * tint has the tint's mean
    tt = node(m, X.MaterialExpressionMultiply, -950, -100)
    link(m, t, "RGB", tt, "A")
    link(m, tint, "", tt, "B")
    t2 = node(m, X.MaterialExpressionMultiply, -750, -100, const_b=2.0)
    link(m, tt, "", t2, "A")
    base = node(m, X.MaterialExpressionLinearInterpolate, -500, -250)
    link(m, t, "RGB", base, "A")
    link(m, t2, "", base, "B")
    link(m, am, "", base, "Alpha")
    mel.connect_material_property(base, "", MP.MP_BASE_COLOR)
    return t, base


def master(name, masked, force):
    path = MATS + "/" + name
    spec = "v%d flipv=%d" % (MASTER_VERSION, int(TINT_FLIP_V))
    if eal.does_asset_exist(path):
        m = unreal.load_asset(path)
        if not force and eal.get_metadata_tag(m, SPEC) == spec:
            return m
        mel.delete_all_material_expressions(m)          # rebuilt in place: the instances keep their parent
    else:
        m = tools.create_asset(name, MATS, unreal.Material, unreal.MaterialFactoryNew())
    # without these usage flags a packaged/-game run silently falls back to the default material
    m.set_editor_property("used_with_skeletal_mesh", True)
    m.set_editor_property("used_with_morph_targets", True)
    # two-sided like the glb's materials and the cast's M_HumanFace: a skirt's or coat's hem folded past a leg shows its
    # inside, so do sleeves and collars
    m.set_editor_property("two_sided", True)
    if masked:
        m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        m.set_editor_property("opacity_mask_clip_value", 0.5)
        m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    else:
        m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
        m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SUBSURFACE)
    t, base = tint_graph(m)
    rough = node(m, unreal.MaterialExpressionScalarParameter, -400, 100, parameter_name="Roughness", default_value=0.6)
    mel.connect_material_property(rough, "", MP.MP_ROUGHNESS)
    specular = node(m, unreal.MaterialExpressionScalarParameter, -400, 180, parameter_name="Specular", default_value=0.4)
    mel.connect_material_property(specular, "", MP.MP_SPECULAR)
    if masked:
        mel.connect_material_property(t, "A", MP.MP_OPACITY_MASK)
    else:
        # the scatter is the section's own colour times SubsurfaceTint; black (Cloth, Eyes) lights them as default lit,
        # keeping only subsurface's softer shadow edge, as the cast's cloth has it
        sss = node(m, unreal.MaterialExpressionMultiply, -250, 400)
        link(m, base, "", sss, "A")
        link(m, node(m, unreal.MaterialExpressionVectorParameter, -500, 450, parameter_name="SubsurfaceTint",
                     default_value=unreal.LinearColor(0.0, 0.0, 0.0, 1.0)), "", sss, "B")
        mel.connect_material_property(sss, "", MP.MP_SUBSURFACE_COLOR)
        scatter = node(m, unreal.MaterialExpressionScalarParameter, -400, 550, parameter_name="Scatter", default_value=1.0)
        mel.connect_material_property(scatter, "", MP.MP_OPACITY)
    mel.recompile_material(m)
    eal.set_metadata_tag(m, SPEC, spec)
    eal.save_loaded_asset(m, only_if_is_dirty=False)
    log("  %s built (%s, two-sided)" % (name, "masked" if masked else "opaque, subsurface"))
    return m


def instance(section, masters, tex, force):
    kind, rough, specular, sss, alpha_mask = LOOK[section]
    name = "MI_Resident_" + section
    path = MATS + "/" + name
    parent = masters[kind]
    want = json.dumps([parent.get_path_name(), eal.get_metadata_tag(parent, SPEC), tex.get_path_name(),
                       eal.get_metadata_tag(tex, SPEC), rough, specular, sss, alpha_mask])
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
        if not force and eal.get_metadata_tag(mi, SPEC) == want:
            return mi
    else:
        mi = tools.create_asset(name, MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", parent)
    ok = [mel.set_material_instance_texture_parameter_value(mi, "Atlas", tex),
          mel.set_material_instance_scalar_parameter_value(mi, "Roughness", rough),
          mel.set_material_instance_scalar_parameter_value(mi, "Specular", specular)]
    if sss is not None:
        ok.append(mel.set_material_instance_vector_parameter_value(mi, "SubsurfaceTint",
                                                                   unreal.LinearColor(sss[0], sss[1], sss[2], 1.0)))
    if alpha_mask:                     # Cloth only: an overridden switch compiles the instance shaders of its own
        try:
            ok.append(mel.set_material_instance_static_switch_parameter_value(mi, "AlphaIsTintMask", True))
        except Exception as e:                                          # noqa: BLE001
            log("  %s: %s" % (name, e))
            ok.append(False)
    mel.update_material_instance(mi)
    # UE 5.8's setters return False even when the value took: judge by reading the values back
    ok = [mel.get_material_instance_texture_parameter_value(mi, "Atlas") == tex,
          abs(mel.get_material_instance_scalar_parameter_value(mi, "Roughness") - rough) < 1e-3,
          abs(mel.get_material_instance_scalar_parameter_value(mi, "Specular") - specular) < 1e-3]
    if sss is not None:
        v = mel.get_material_instance_vector_parameter_value(mi, "SubsurfaceTint")
        ok.append(max(abs(v.r - sss[0]), abs(v.g - sss[1]), abs(v.b - sss[2])) < 1e-3)
    if alpha_mask:
        ok.append(bool(mel.get_material_instance_static_switch_parameter_value(mi, "AlphaIsTintMask")))
    if not all(ok):
        BROKEN.append("%s: parameters Atlas, Roughness, Specular, SubsurfaceTint, AlphaIsTintMask took %s" % (name, ok))
    eal.set_metadata_tag(mi, SPEC, want)
    eal.save_loaded_asset(mi, only_if_is_dirty=False)
    log("  %s on %s: roughness %.2f%s%s" % (name, parent.get_name(), rough,
                                            (", subsurface %s" % (sss,)) if sss and any(sss) else "",
                                            ", AlphaIsTintMask" if alpha_mask else ""))
    return mi


def pipeline():
    """A copy of the glTF assets pipeline that binds the meshes to SK_Human_Skeleton, made once (import_character.py's;
    import_model checks every mesh's skeleton)."""
    if eal.does_asset_exist(PIPELINE):
        return
    src = unreal.load_asset("/Interchange/Pipelines/DefaultGLTFAssetsPipeline")
    pipe = tools.duplicate_asset(PIPELINE.split("/")[-1], PIPELINE.rsplit("/", 1)[0], src)
    common = pipe.get_editor_property("common_skeletal_meshes_and_animations_properties")
    common.set_editor_property("skeleton", unreal.load_asset(SKELETON))
    eal.save_loaded_asset(pipe, only_if_is_dirty=False)


def lod_settings():
    """Town/LOD_Resident (SkeletalMeshLODSettings) from LOD_GROUPS, made again when they change (tag ResidentSpec)."""
    spec = "lods " + json.dumps(LOD_GROUPS)
    if eal.does_asset_exist(LODS):
        s = unreal.load_asset(LODS)
        if eal.get_metadata_tag(s, SPEC) == spec:
            return s
    else:
        s = unreal.AssetToolsHelpers.get_asset_tools().create_asset(LODS.split("/")[-1], TOWN,
                                                                     unreal.SkeletalMeshLODSettings, None)
    groups = []
    for screen, pct in LOD_GROUPS:
        g = unreal.SkeletalMeshLODGroupSettings()
        ps = unreal.PerPlatformFloat()
        ps.set_editor_property("default", screen)
        g.set_editor_property("screen_size", ps)
        r = unreal.SkeletalMeshOptimizationSettings()
        r.set_editor_property("num_of_triangles_percentage", pct / 100.0)
        g.set_editor_property("reduction_settings", r)
        groups.append(g)
    s.set_editor_property("lod_groups", groups)
    eal.set_metadata_tag(s, SPEC, spec)
    eal.save_loaded_asset(s)
    log("LOD_Resident: %s" % ", ".join("%g %% from screen size %g" % (p, sc) for sc, p in LOD_GROUPS))
    return s


def unprepared():
    need = [TEXTURES + "/T_Resident_" + s for s in SECTIONS] + [MATS + "/MI_Resident_" + s for s in SECTIONS] + [
        MATS + "/M_Resident_Opaque", MATS + "/M_Resident_Masked", PIPELINE, LODS]
    return [p for p in need if not eal.does_asset_exist(p)]


def face_curves():
    """ULyudBodyAnimInstance drives the six face morphs through morph target curves on SK_Human_Skeleton, which
    import_character.py adds (its face_curves()); this script never saves the skeleton, so it only says."""
    AL = unreal.AnimationLibrary
    if not hasattr(AL, "get_curve_meta_data_names"):
        return
    have = {str(n) for n in AL.get_curve_meta_data_names(unreal.load_asset(SKELETON))}
    lack = [c for c in MORPHS if c not in have]
    if lack:
        log("note: SK_Human_Skeleton has no curve %s: the residents won't blink or talk until a cast import "
            "(import_character.py) adds them" % ",".join(lack))


def prepare(force):
    tex, changed = {}, []
    for s in SECTIONS:
        tex[s], new = atlas(s, force)
        if new:
            changed.append(s)
    masters = {"Opaque": master("M_Resident_Opaque", False, force), "Masked": master("M_Resident_Masked", True, force)}
    for s in SECTIONS:
        instance(s, masters, tex[s], force)
    pipeline()
    lod_settings()
    face_curves()
    if changed and any(os.path.isfile(disk("%s/SK_%s" % (home(m), m)) + ".uasset") for _, m in town()[0]):
        log("note: atlases %s changed: if their layout did, build_residents.py rebuilds every model (the sidecars carry "
            "the layout's id) and the following runs re-import them" % ",".join(changed))
    for b in BROKEN:
        log("  MISSING " + b)
    log("prepared: atlases, masters, MI_Resident_*, %s, LOD_Resident%s" % (
        PIPELINE.split("/")[-1], (" -- %d material links or parameters didn't take (MISSING above): the people import "
                                  "anyway; fix master() and bump MASTER_VERSION, the next run rebuilds it in place"
                                  % len(BROKEN)) if BROKEN else ""))


def prune():
    people, listed = town()
    if not listed:
        log("prune: no town_looks.json, nothing deleted")
        return
    keep = {m for _, m in people}
    root = disk(TOWN)
    gone = [n for n in sorted(os.listdir(root)) if n.startswith("R_") and n not in keep and
            os.path.isdir(os.path.join(root, n))] if os.path.isdir(root) else []
    for n in gone:
        clear("%s/%s" % (TOWN, n))
        if os.path.isfile(state_path(n)):
            os.remove(state_path(n))
    log("prune: %d models no longer in town_looks.json deleted%s" % (len(gone), (": " + " ".join(gone[:20])) if gone else ""))


# ------------------------------------------------------------------------------------------ a model (UE)

def clear(folder):
    """Every asset of a /Game folder gone, and the files the editor leaves behind on disk (import_character.py)."""
    if eal.does_directory_exist(folder):
        for p in eal.list_assets(folder, True):
            eal.delete_asset(p)
        eal.delete_directory(folder)
    if os.path.isdir(disk(folder)):
        shutil.rmtree(disk(folder), ignore_errors=True)


def interchange(glb, tmp):
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.override_pipelines.append(unreal.SoftObjectPath("/Interchange/Pipelines/DefaultGLTFPipeline.DefaultGLTFPipeline"))
    params.override_pipelines.append(unreal.SoftObjectPath(PIPELINE + "." + PIPELINE.split("/")[-1]))
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_asset(tmp, unreal.InterchangeManager.create_source_data(glb), params)
    mgr.wait_until_all_tasks_done(WAIT_CANCEL)
    mesh, phys = None, None
    for p in eal.list_assets(tmp, True):
        a = eal.load_asset(p)
        if isinstance(a, unreal.SkeletalMesh):
            mesh = a
        elif isinstance(a, unreal.PhysicsAsset):
            phys = a
    return mesh, phys


def section_of(slot):
    """Interchange names a slot after its glTF material (Skin, Cloth, Hair, Eyes), sometimes with a suffix."""
    s = slot.lower()
    return next((k for k in SECTIONS if s == k.lower()), None) or next(
        (k for k in SECTIONS if s.startswith(k.lower()) or k.lower() in s), None)


def nanite_off(mesh):
    """No Nanite on the people (the masters carry no Nanite usage: garbled fallbacks otherwise)."""
    try:
        ns = mesh.get_editor_property("nanite_settings")
    except Exception:                                                   # noqa: BLE001
        return "n/a"
    if ns.get_editor_property("enabled"):
        ns.set_editor_property("enabled", False)
        mesh.set_editor_property("nanite_settings", ns)
        return "turned off"
    return "off"


def import_model(c, nth, mis, lods, tmp_root):
    """One person; nth: the Interchange imports this process made before. -> (result, info, imports made), result
    "ok" (info: stats), "failed" or "miss" (a later import came back empty: this process is done importing)."""
    model = c["model"]
    dest = home(model)
    final = "%s/SK_%s" % (dest, model)
    tmp = "%s/%s" % (tmp_root, model)
    t0 = time.time()
    clear(tmp)
    mesh, phys = interchange(c["glb"], tmp)
    calls = 1
    if mesh is None and nth > 0:
        log("  %s: nothing came in; once more" % model)
        clear(tmp)
        mesh, phys = interchange(c["glb"], tmp)
        calls = 2
        if mesh is None:
            clear(tmp)
            return "miss", "import %d of this process came back empty, twice" % (nth + 1), calls
    if mesh is None:
        clear(tmp)
        return "failed", "no skeletal mesh came out of the glb", calls
    skel = unreal.load_asset(SKELETON)
    if mesh.skeleton is None or mesh.skeleton.get_path_name() != skel.get_path_name():
        got = mesh.skeleton.get_path_name() if mesh.skeleton else "none"
        mesh = phys = None
        clear(tmp)                     # the stray skeleton goes with it: never a second skeleton
        return ("miss" if nth > 0 else "failed"), "bound to %s, not SK_Human_Skeleton" % got, calls
    # the old model goes only now (a miss keeps it), while the new one still holds Interchange's material instances:
    # deleting runs the garbage collector, which would take unreferenced packages out of the import folder
    clear(dest)
    mats = mesh.get_editor_property("materials")
    slots, missing = {}, []
    for i in range(len(mats)):
        sm = mats[i]
        slot = str(sm.material_slot_name)
        mi = mis.get(section_of(slot))
        if mi is None:
            missing.append(slot)
            mi = mis["Cloth"]          # opaque and tinted: a stray section still draws
        sm.material_interface = mi
        mats[i] = sm
        slots[slot] = mi.get_name()
    mesh.set_editor_property("materials", mats)
    if not eal.rename_asset(mesh.get_path_name(), final):
        mesh = phys = None
        clear(tmp)
        return "failed", "could not move the mesh to " + final, calls
    if phys is not None and not eal.rename_asset(phys.get_path_name(), final + "_PhysicsAsset"):
        log("  %s: could not move its physics asset" % model)
    phys = None
    clear(tmp)                         # Interchange's own material instances
    mesh = unreal.load_asset(final)
    if eal.does_asset_exist(final + "_PhysicsAsset"):
        mesh.set_editor_property("physics_asset", unreal.load_asset(final + "_PhysicsAsset"))
    nanite = nanite_off(mesh)
    mesh.set_editor_property("lod_settings", lods)
    unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem).regenerate_lod(mesh, len(LOD_GROUPS), True, False)
    eal.set_metadata_tag(mesh, TAG, c["tag"])
    eal.save_loaded_asset(mesh, only_if_is_dirty=False)
    eal.save_directory(dest)
    sme = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    n = sme.get_lod_count(mesh)
    b = mesh.get_bounds().box_extent
    return "ok", dict(lods=n, lod_verts=[sme.get_num_verts(mesh, i) for i in range(n)],
                      morphs=sorted(t.get_name() for t in mesh.morph_targets), slots=slots, missing=missing,
                      physics=eal.does_asset_exist(final + "_PhysicsAsset"), nanite=nanite,
                      height_cm=round(b.z * 2, 1), seconds=round(time.time() - t0, 1)), calls


def check_tag(c):
    """A mesh with no record in STATE (copied in, or STATE lost): its own tag says whether it is current."""
    mesh = unreal.load_asset(c["asset"])
    tag = eal.get_metadata_tag(mesh, TAG) if mesh else ""
    if not tag or tag != c["tag"]:
        return False
    sme = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    set_state(c["model"], status="imported", tag=tag, look=c["side"].get("hash"), key=c["key"], model=c["model"],
              at=time.time(), tries=0, error="", lods=sme.get_lod_count(mesh),
              morphs=sorted(t.get_name() for t in mesh.morph_targets),
              missing=[str(m.material_slot_name) for m in mesh.materials if not m.material_interface or
                       not m.material_interface.get_name().startswith("MI_Resident_")])
    return True


def spot_check(cands, n):
    done = [c for c in cands if c["todo"] == "up to date"]
    skel = unreal.load_asset(SKELETON).get_path_name()
    sme = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    for c in random.sample(done, min(n, len(done))):
        mesh = unreal.load_asset(c["asset"])
        if mesh is None:
            log("  spot %s: does not load" % c["model"])
            continue
        morphs = [t.get_name() for t in mesh.morph_targets]
        log("  spot %s: %d morph targets%s, %d LODs, materials %s, skeleton %s, physics %s, tag %s" % (
            c["model"], len(morphs), "" if sorted(morphs) == sorted(MORPHS) else " %s" % morphs, sme.get_lod_count(mesh),
            "/".join(m.material_interface.get_name() if m.material_interface else "NONE" for m in mesh.materials),
            "shared" if mesh.skeleton and mesh.skeleton.get_path_name() == skel else "WRONG",
            "yes" if mesh.get_editor_property("physics_asset") else "NONE", eal.get_metadata_tag(mesh, TAG) or "NONE"))


def gc():
    try:
        unreal.SystemLibrary.collect_garbage()
    except Exception:                                                   # noqa: BLE001
        pass


def run(args):
    t0 = time.time()
    since = args.since or t0
    if not eal.does_asset_exist(SKELETON):
        log("STOP no %s: import the cast first (import_humans.py)" % SKELETON)
        return
    if args.prune:
        prune()
    todo = unprepared()
    if args.prepare or todo:
        if todo and not args.prepare:
            log("shared assets missing (%s): this run makes them, imports start with the next run" % ", ".join(
                p.split("/")[-1] for p in todo))
        prepare(args.force and args.prepare)
        log("left %d" % left_of(plan(args, since)))
        return
    for s in SECTIONS:
        png = os.path.join(ATLASES, "T_Resident_%s.png" % s)
        if os.path.isfile(png) and eal.get_metadata_tag(unreal.load_asset(TEXTURES + "/T_Resident_" + s), SPEC) != sha1(png):
            log("note: %s changed since its import: run with --prepare (the wrapper does)" % os.path.basename(png))
    mis = {s: unreal.load_asset(MATS + "/MI_Resident_" + s) for s in SECTIONS}
    lods = lod_settings()
    tmp_root = TMP + (("_" + args.shard.replace("/", "of")) if args.shard else "")
    clear(tmp_root)                    # what a killed run left behind
    cands = plan(args, since)
    for c in cands:                    # still "trying": the editor died or hung on it
        st = c["state"]
        if st.get("status") == "trying":
            if st.get("tries", 0) >= MAX_TRIES:
                st = set_state(c["model"], status="failed", at=time.time(),
                               error="the editor died or hung importing it (%d tries)" % st.get("tries", 0))
                log("FAILED %s: %s" % (c["model"], st["error"]))
            else:
                st = set_state(c["model"], status="pending")
            c["state"] = st
            c["todo"] = classify(c, args, since)
    # how many imports a process takes, learnt in this session (per shard: parallel runs keep their own)
    memo_path = os.path.join(STATE, "interchange%s.json" % (("_" + args.shard.replace("/", "of")) if args.shard else ""))
    memo = read_json(memo_path) or {}
    if memo.get("session") != "%d" % since or memo.get("wait_cancel") != WAIT_CANCEL:
        memo = dict(session="%d" % since, wait_cancel=WAIT_CANCEL)
    if memo.get("attempt", 0) >= 2 and memo.get("attempt", 0) > memo.get("finished", 0):
        # the last process died in a later import: once may be the person (their tries count), twice is the process
        deaths = memo.get("deaths", [])[-1:] + [memo["attempt"]]
        memo["deaths"] = deaths
        if len(deaths) >= 2:
            memo["limit"] = max(1, min(deaths) - 1)
    chunk = min(args.chunk, memo.get("limit") or args.chunk)
    if chunk < args.chunk:
        log("Interchange took %d .glb per process in this session: %d per run" % (chunk, chunk))
    memo.update(attempt=0, finished=0)
    write_json(memo_path, memo)
    done, failed, checked, nth, fails_in_row, loads, stop = [], [], 0, 0, 0, 0, ""
    for c in cands:
        if c["todo"] == "check":
            if checked >= CHECKS_PER_RUN:
                continue
            checked += 1
            loads += 1
            current = check_tag(c)
            if loads % 10 == 0:            # (a run with no records loads up to CHECKS_PER_RUN meshes only for this)
                gc()
            if current:
                c["state"] = read_json(state_path(c["model"])) or {}
                c["todo"] = "up to date"
                continue
            c["todo"] = "import"
        if c["todo"] != "import":
            continue
        if nth >= chunk:
            break
        prev = read_json(state_path(c["model"]))
        was = prev or {}
        # asked to try again (--retry-failed, --force): the tries of earlier sessions don't count
        fresh = (args.force or args.retry_failed) and was.get("at", 0) < since
        tries = (0 if fresh or was.get("tag") != c["tag"] else was.get("tries", 0)) + 1
        set_state(c["model"], status="trying", tag=c["tag"], tries=tries, at=time.time(), key=c["key"], model=c["model"])
        memo["attempt"] = nth + 1
        write_json(memo_path, memo)
        try:
            res, info, calls = import_model(c, nth, mis, lods, tmp_root)
        except Exception as e:                                          # noqa: BLE001
            res, info, calls = "failed", "%s: %s" % (type(e).__name__, e), 1
            log("  " + " | ".join(l.strip() for l in traceback.format_exc().strip().splitlines()[-5:]
                                  if l.strip().strip("~^")))
            try:
                clear("%s/%s" % (tmp_root, c["model"]))
            except Exception:                                           # noqa: BLE001
                pass
        nth += calls
        memo["finished"] = memo["attempt"]
        if res == "miss":
            # nothing wrong with the model: its record as it was, and this process imports no more
            if prev is None:
                os.remove(state_path(c["model"]))
            else:
                write_json(state_path(c["model"]), prev)
            memo["limit"] = max(1, memo["attempt"] - 1)
            write_json(memo_path, memo)
            stop = "%s: %s; the next run goes on" % (c["model"], info)
            break
        write_json(memo_path, memo)
        if res == "ok":
            st = set_state(c["model"], status="imported", tag=c["tag"], look=c["side"].get("hash"), tries=0,
                           at=time.time(), error="", **info)
            c["state"], c["todo"] = st, "up to date"
            done.append(c)
            fails_in_row = 0
            log("imported %s: %d LODs %s, %d morphs, %s, nanite %s, %.0f cm, %.0f s" % (
                c["model"], info["lods"], info["lod_verts"], len(info["morphs"]),
                " ".join("%s>%s" % (k, v.replace("MI_Resident_", "")) for k, v in sorted(info["slots"].items())),
                info["nanite"], info["height_cm"], info["seconds"]))
        else:
            st = set_state(c["model"], status="failed", error=info, at=time.time())
            c["state"] = st
            c["todo"] = "failed" if st.get("tries", 0) >= MAX_TRIES else "import"
            failed.append(c)
            fails_in_row += 1
            log("FAILED %s (try %d): %s" % (c["model"], st.get("tries", 0), info))
            if fails_in_row >= 2 and nth > 1:
                stop = "two failures in a row; the next run starts afresh"
                break
        loads += 1
        if loads % 10 == 0:
            gc()
    if stop:
        log("this run stops importing: " + stop)
    left = left_of(cands)
    log("run: %d imported, %d failed, %d meshes checked, %d per run, %.0f s" % (len(done), len(failed), checked, chunk,
                                                                              time.time() - t0))
    missing = ["%s %s" % (c["model"], ",".join(c["state"].get("missing"))) for c in done if c["state"].get("missing")]
    if missing:
        log("missing materials: %s" % "; ".join(missing))
    bad = [c["model"] for c in done if sorted(c["state"].get("morphs", [])) != sorted(MORPHS)]
    if done:
        log("morph targets: %d of %d imported have the six face morphs%s" % (
            len(done) - len(bad), len(done), ("; not: " + ", ".join(bad)) if bad else ""))
    if left == 0:
        if not args.shard:
            report(plan(args, since), final=not (args.only or args.limit))
        spot_check(cands, args.spot)
    log("left %d" % left)


def main():
    ap = argparse.ArgumentParser(description="the residents' models into Unreal (docs/residents.md)")
    ap.add_argument("--prepare", action="store_true", help="only the atlases, masters, instances and pipeline")
    ap.add_argument("--only", default="", help="keys (t_...) or models (R_...), comma separated")
    ap.add_argument("--limit", type=int, default=0, help="the first N people of the town (by key)")
    ap.add_argument("--chunk", type=int, default=60, help="Interchange imports per editor run at most")
    ap.add_argument("--force", action="store_true", help="re-import what was imported before --since")
    ap.add_argument("--retry-failed", action="store_true", help="retry what failed before --since")
    ap.add_argument("--prune", action="store_true", help="delete Town/R_* folders no longer in town_looks.json")
    ap.add_argument("--since", type=float, default=0.0, help="the session's start, unix time (default: now)")
    ap.add_argument("--shard", default="", help="i/n: every n-th person from the i-th (parallel runs)")
    ap.add_argument("--spot", type=int, default=6, help="meshes the last run loads again to check")
    ap.add_argument("--report", action="store_true", help="the tally from the state files (no engine needed)")
    args, _ = ap.parse_known_args(sys.argv[1:])
    if args.report or unreal is None:
        cands = plan(args, args.since or time.time())
        report(cands, final=not (args.only or args.limit or args.shard))
        log("left %d" % left_of(cands))
        return
    run(args)


if __name__ != "import_residents":    # run as a script (UE's -script= or python3), not when imported
    main()
