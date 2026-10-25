"""Imports the birds built by blender/birds/build_bird.py (blender/birds/export/<Id>/<Id>.glb + textures + _rig.json).

  /Game/Birds/Materials/M_BirdPlumage, M_BirdBare, M_BirdEye   masters (masked two-sided foliage plumage with thin-film gloss)
  /Game/Birds/<Id>/SK_<Id>, SKEL_<Id>, PHYS_<Id>                skeletal mesh, sockets eye_l / eye_r on head
  /Game/Birds/<Id>/T_<Id>_Albedo|Normal|Params[_Winter]          feather atlas (RGB + alpha mask), normal, R roughness G gloss
  /Game/Birds/<Id>/MI_<Id>_Plumage|Bare|Eye[_Winter]
  /Game/Birds/<Id>/Anims/A_<Id>_<Clip>                          Idle Look Alert Hop Walk Peck Eat Preen Call Takeoff Flap Glide Bound Land Sleep
  /Game/Birds/<Id>/SM_<Id>_<Pose>                               static poses for instanced far birds (from <Id>_pose_<Pose>.glb)

Headless, no editor open on this checkout, one process per species (Interchange can't re-import in one process):
  UnrealEditor <uproject> -run=pythonscript -script="<this file> masters"
  UnrealEditor <uproject> -run=pythonscript -script="<this file> import <Id>"
(unreal/scripts/import_birds.sh does both for a list of species)
"""
import json
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
EXPORT = os.path.join(ROOT, "blender", "birds", "export")
BIRDS = "/Game/Birds"
MATS = BIRDS + "/Materials"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
CLIPS = ["Idle", "Look", "Alert", "Hop", "Walk", "Peck", "Eat", "Preen", "Call", "Takeoff", "Flap", "Glide", "Bound", "Land", "Sleep"]


def log(msg):
    unreal.log_warning("BIRDS " + msg)


def get_or_create(name, folder, cls, factory):
    path = "%s/%s" % (folder, name)
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    return tools.create_asset(name, folder, cls, factory)


def node(m, cls, x, y):
    return mel.create_material_expression(m, cls, x, y)


DEFAULTS = {}


def tex_param(m, name, x, y, sampler):
    t = node(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    t.set_editor_property("parameter_name", name)
    t.set_editor_property("sampler_type", sampler)
    t.set_editor_property("texture", DEFAULTS[name])
    return t


def png_rgba(path, w, h, rgba):
    """A flat PNG without PIL."""
    import struct
    import zlib
    raw = b"".join(b"\x00" + bytes(rgba) * w for _ in range(h))

    def chunk(k, d):
        return struct.pack(">I", len(d)) + k + d + struct.pack(">I", zlib.crc32(k + d) & 0xffffffff)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def default_textures():
    tmpdir = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Saved", "birds_defaults")
    os.makedirs(tmpdir, exist_ok=True)
    for name, rgba, kind in (("Albedo", (128, 128, 128, 255), "albedo"), ("Normal", (128, 128, 255, 255), "normal"), ("Params", (140, 0, 255, 255), "params")):
        f = os.path.join(tmpdir, "T_BirdDefault_%s.png" % name)
        png_rgba(f, 4, 4, rgba)
        DEFAULTS[name] = import_texture(f, MATS, "T_BirdDefault_%s" % name, kind)


GLOSS_HLSL = """
// structural gloss: a thin-film tint that shows mostly at grazing angles, over the (dark) feather colour.
// Hue picks the species' sheen (0.0 purple-green starling, 0.35 magpie blue-green, 0.62 rook / swallow blue-purple).
float f = saturate(1 - abs(dot(normalize(N), normalize(V))));
float ph = Hue + f * 0.45;
float3 film = 0.5 + 0.5 * cos(6.2831853 * (ph + float3(0.0, 0.33, 0.67)));
return Base + film * saturate(Gloss) * (0.004 + 0.07 * f * f * f);
"""


DITHER_HLSL = """
// interleaved-gradient noise, moved every frame: soft alpha (strand tips) becomes a dither that temporal AA smooths
float2 p = Parameters.SvPosition.xy + (float)View.StateFrameIndexMod8 * float2(5.588238, 5.588238);
float n = frac(52.9829189 * frac(dot(p, float2(0.06711056, 0.00583715))));
return Alpha + (n - 0.5) * 0.98 * step(0.001, Alpha) * step(Alpha, 0.999);
"""


def make_master(name, plumage, eye=False):
    m = get_or_create(name, MATS, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(m)
    m.set_editor_property("used_with_skeletal_mesh", True)
    m.set_editor_property("used_with_static_lighting", False)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    if plumage:
        m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        m.set_editor_property("two_sided", True)
        m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
        m.set_editor_property("opacity_mask_clip_value", 0.5)
        m.set_editor_property("dithered_lod_transition", True)
    else:
        m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
        m.set_editor_property("two_sided", True)   # bill halves and toe webs are open shells; the eyeball faces either way
        m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    alb = tex_param(m, "Albedo", -900, -200, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    nrm = tex_param(m, "Normal", -900, 150, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    par = tex_param(m, "Params", -900, 450, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    tint = node(m, unreal.MaterialExpressionVectorParameter, -900, -450)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    mul = node(m, unreal.MaterialExpressionMultiply, -600, -250)
    mel.connect_material_expressions(alb, "RGB", mul, "A")
    mel.connect_material_expressions(tint, "", mul, "B")
    base = mul
    if plumage:
        hue = node(m, unreal.MaterialExpressionScalarParameter, -900, 700)
        hue.set_editor_property("parameter_name", "GlossHue")
        hue.set_editor_property("default_value", 0.0)
        vn = node(m, unreal.MaterialExpressionPixelNormalWS, -600, 650)
        cam = node(m, unreal.MaterialExpressionCameraVectorWS, -600, 750)
        cu = node(m, unreal.MaterialExpressionCustom, -350, -100)
        cu.set_editor_property("code", GLOSS_HLSL)
        cu.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        ins = []
        for nm in ("Base", "Gloss", "Hue", "N", "V"):
            ci = unreal.CustomInput()
            ci.set_editor_property("input_name", nm)
            ins.append(ci)
        cu.set_editor_property("inputs", ins)
        mel.connect_material_expressions(mul, "", cu, "Base")
        mel.connect_material_expressions(par, "G", cu, "Gloss")
        mel.connect_material_expressions(hue, "", cu, "Hue")
        mel.connect_material_expressions(vn, "", cu, "N")
        mel.connect_material_expressions(cam, "", cu, "V")
        base = cu
        # soft strand tips: the alpha is dithered (temporal AA resolves it to soft, fluffy edges)
        dith = node(m, unreal.MaterialExpressionCustom, -350, 120)
        dith.set_editor_property("code", DITHER_HLSL)
        dith.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", "Alpha")
        dith.set_editor_property("inputs", [ci])
        mel.connect_material_expressions(alb, "A", dith, "Alpha")
        mel.connect_material_property(dith, "", unreal.MaterialProperty.MP_OPACITY_MASK)
        # feathers let light through: backlit wings glow warm
        sss = node(m, unreal.MaterialExpressionMultiply, -350, 250)
        mel.connect_material_expressions(mul, "", sss, "A")
        k = node(m, unreal.MaterialExpressionConstant, -500, 330)
        k.set_editor_property("r", 0.35)
        mel.connect_material_expressions(k, "", sss, "B")
        mel.connect_material_property(sss, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
        spec = node(m, unreal.MaterialExpressionConstant, -350, 400)
        spec.set_editor_property("r", 0.35)
        mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    if eye:
        spec = node(m, unreal.MaterialExpressionConstant, -350, 400)
        spec.set_editor_property("r", 0.9)
        mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    if plumage or eye:
        mel.connect_material_property(par, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    else:   # bill, legs, bare skin: horn and scales are matte, whatever the texture says
        mx = node(m, unreal.MaterialExpressionMax, -350, 500)
        mel.connect_material_expressions(par, "R", mx, "A")
        lo = node(m, unreal.MaterialExpressionConstant, -500, 560)
        lo.set_editor_property("r", 0.55)
        mel.connect_material_expressions(lo, "", mx, "B")
        mel.connect_material_property(mx, "", unreal.MaterialProperty.MP_ROUGHNESS)
        sp = node(m, unreal.MaterialExpressionConstant, -350, 620)
        sp.set_editor_property("r", 0.3)
        mel.connect_material_property(sp, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log("master " + name)
    return m


def do_masters():
    default_textures()
    make_master("M_BirdPlumage", True)
    make_master("M_BirdBare", False)
    make_master("M_BirdEye", False, eye=True)


def import_texture(path, dest, name, kind):
    """With the classic texture factory (Interchange imports only one PNG per process here)."""
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    task.set_editor_property("factory", unreal.TextureFactory())
    tools.import_asset_tasks([task])
    t = unreal.load_asset("%s/%s" % (dest, name))
    if t is None:
        raise RuntimeError("texture import failed: " + path)
    if kind == "normal":
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        t.set_editor_property("srgb", False)
        t.set_editor_property("flip_green_channel", True)   # Blender (OpenGL) -> Unreal (DirectX)
    elif kind == "params":
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        t.set_editor_property("srgb", False)
    else:
        t.set_editor_property("srgb", True)
    t.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
    eal.save_loaded_asset(t)
    return t


def material_instance(name, folder, parent, tex, hue):
    mi = get_or_create(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", parent)
    mel.set_material_instance_texture_parameter_value(mi, "Albedo", tex["albedo"])
    mel.set_material_instance_texture_parameter_value(mi, "Normal", tex["normal"])
    mel.set_material_instance_texture_parameter_value(mi, "Params", tex["params"])
    if "Plumage" in name:
        mel.set_material_instance_scalar_parameter_value(mi, "GlossHue", hue)
    eal.save_loaded_asset(mi)
    return mi


def do_hues(ids):
    """GlossHue on every species' plumage instances, from blender/birds/species_art.py (morph gloss_hue)."""
    sys.path.insert(0, os.path.join(ROOT, "blender", "birds"))
    import species_art as art
    table = json.load(open(os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "BirdsData", "species.json")))["species"]
    for sid in ids or [t["id"] for t in table]:
        hue = art.get(sid)["morph"].get("gloss_hue", 0.0) if sid in art.SPECIES else 0.0
        for suffix in ("", "_Winter"):
            p = "%s/%s/MI_%s_Plumage%s" % (BIRDS, sid, sid, suffix)
            if eal.does_asset_exist(p):
                mi = unreal.load_asset(p)
                mel.set_material_instance_scalar_parameter_value(mi, "GlossHue", hue)
                eal.save_loaded_asset(mi)
        log("hue %s %.2f" % (sid, hue))


TEX = (("albedo", "Albedo"), ("normal", "Normal"), ("params", "Params"), ("winter", "Albedo_Winter"))


def do_textures(sid):
    """Step 1 (own process): a clean folder and the textures (Interchange won't import them after the .glb in one process)."""
    src_dir = os.path.join(EXPORT, sid)
    dest = "%s/%s" % (BIRDS, sid)
    if eal.does_directory_exist(dest):
        eal.delete_directory(dest)
    for kind, suffix in TEX:
        f = os.path.join(src_dir, "T_%s_%s.png" % (sid, suffix))
        if os.path.exists(f):
            import_texture(f, dest, "T_%s_%s" % (sid, suffix), "albedo" if kind == "winter" else kind)
    eal.save_directory(dest)


def do_import(sid):
    src_dir = os.path.join(EXPORT, sid)
    rig = json.load(open(os.path.join(src_dir, "%s_rig.json" % sid)))
    dest = "%s/%s" % (BIRDS, sid)
    tmp = dest + "/_import"
    src = unreal.InterchangeManager.create_source_data(os.path.join(src_dir, sid + ".glb"))
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.override_pipelines.append(unreal.SoftObjectPath("/Interchange/Pipelines/DefaultGLTFPipeline.DefaultGLTFPipeline"))
    params.override_pipelines.append(unreal.SoftObjectPath("/Interchange/Pipelines/DefaultGLTFAssetsPipeline.DefaultGLTFAssetsPipeline"))
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_asset(tmp, src, params)
    mgr.wait_until_all_tasks_done(True)
    mesh, skel, phys, anims = None, None, None, []
    for p in eal.list_assets(tmp, True):
        a = eal.load_asset(p)
        if isinstance(a, unreal.SkeletalMesh):
            mesh = a
        elif isinstance(a, unreal.Skeleton):
            skel = a
        elif isinstance(a, unreal.PhysicsAsset):
            phys = a
        elif isinstance(a, unreal.AnimSequence):
            anims.append(a)
    if mesh is None:
        raise RuntimeError("no skeletal mesh in %s.glb" % sid)
    tex = {kind: unreal.load_asset("%s/T_%s_%s" % (dest, sid, suffix)) for kind, suffix in TEX}
    hue = rig.get("gloss_hue", 0.0)
    masters = {"Plumage": unreal.load_asset(MATS + "/M_BirdPlumage"), "Bare": unreal.load_asset(MATS + "/M_BirdBare"),
               "Eye": unreal.load_asset(MATS + "/M_BirdEye")}
    mis = {k: material_instance("MI_%s_%s" % (sid, k), dest, v, tex, hue) for k, v in masters.items()}
    if tex["winter"]:
        wt = dict(tex, albedo=tex["winter"])
        for k, v in masters.items():
            material_instance("MI_%s_%s_Winter" % (sid, k), dest, v, wt, hue)
    mats = mesh.get_editor_property("materials")
    for i in range(len(mats)):
        sm = mats[i]
        slot = str(sm.material_slot_name)
        kind = next((k for k in mis if k in slot), "Plumage")
        sm.material_interface = mis[kind]
        mats[i] = sm
    mesh.set_editor_property("materials", mats)
    # move everything out of the import folder in one go (references follow), then fix up and drop the redirectors
    moves = [(mesh, "SK_%s" % sid, dest)]
    if skel:
        moves.append((skel, "SKEL_%s" % sid, dest))
    if phys:
        moves.append((phys, "PHYS_%s" % sid, dest))
    for a in anims:
        nm = a.get_name()
        clip = next((c for c in CLIPS if nm.endswith(c) or ("_%s" % c) in nm or nm == c), None)
        if clip is None:
            continue
        a.set_editor_property("enable_root_motion", False)
        moves.append((a, "A_%s_%s" % (sid, clip), dest + "/Anims"))
    tools.rename_assets([unreal.AssetRenameData(o, path, name) for o, name, path in moves])
    mesh = unreal.load_asset("%s/SK_%s" % (dest, sid))
    if phys:
        mesh.set_editor_property("physics_asset", unreal.load_asset("%s/PHYS_%s" % (dest, sid)))
    for o, name, path in moves:
        eal.save_asset("%s/%s" % (path, name), only_if_is_dirty=False)
    reds = [eal.load_asset(p) for p in eal.list_assets(tmp, True)]
    reds = [r for r in reds if r and r.get_class().get_name() == "ObjectRedirector"]
    if reds:
        tools.fixup_referencers(reds)
    eal.save_loaded_asset(mesh)
    lods = import_lods(mesh, src_dir, sid, mis)
    poses = 0
    for f in sorted(os.listdir(src_dir)):
        if f.startswith(sid + "_pose_") and f.endswith(".glb"):
            pose = f[len(sid) + 6:-4]
            poses += import_pose(os.path.join(src_dir, f), dest, sid, pose, mis)
    for p in eal.list_assets(tmp, True):
        eal.delete_asset(p)
    if eal.does_directory_exist(tmp):
        eal.delete_directory(tmp)
    eal.save_directory(dest)
    b = mesh.get_bounds().box_extent
    log("%s: %d materials, %d LODs, %d clips, %d poses, bounds %.1f x %.1f x %.1f cm" % (sid, len(mats), lods + 1, len(eal.list_assets(dest + "/Anims")), poses, b.x * 2, b.y * 2, b.z * 2))


LOD_SCREEN = [1.0, 0.16, 0.06, 0.02]   # a bird is small: full feathers only up close (0.16 ~ a 20 cm bird at ~4 m)


def import_lods(mesh, src_dir, sid, mis):
    """The LODs came in with the .glb (meshes named LODn_SK_<Id>); give them the bird screen sizes."""
    sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    n = sub.get_lod_count(mesh)
    unreal.BirdSpeciesLibrary.set_lod_screen_sizes(mesh, LOD_SCREEN[:n])
    # the LOD sections carry the glTF materials; point every section at our instances by slot name
    mats = mesh.get_editor_property("materials")
    for i in range(len(mats)):
        sm = mats[i]
        kind = next((k for k in mis if k in str(sm.material_slot_name)), "Plumage")
        sm.material_interface = mis[kind]
        mats[i] = sm
    mesh.set_editor_property("materials", mats)
    eal.save_loaded_asset(mesh)
    return n - 1


def import_pose(path, dest, sid, pose, mis):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", dest + "/_pose")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    tools.import_asset_tasks([task])
    sm = None
    for p in eal.list_assets(dest + "/_pose", True):
        a = eal.load_asset(p)
        if isinstance(a, unreal.StaticMesh) and sm is None:
            sm = a
        elif not isinstance(a, unreal.StaticMesh):
            eal.delete_asset(p)
    if sm is None and not getattr(import_pose, "retried", False):
        import_pose.retried = True
        return import_pose(path, dest, sid, pose, mis)
    if sm is None:
        log("pose %s: no static mesh" % pose)
        return 0
    mats = sm.get_editor_property("static_materials")
    for i in range(len(mats)):
        m = mats[i]
        kind = next((k for k in mis if k in str(m.material_slot_name)), "Plumage")
        m.material_interface = mis[kind]
        mats[i] = m
    sm.set_editor_property("static_materials", mats)
    eal.rename_asset(sm.get_path_name(), "%s/SM_%s_%s" % (dest, sid, pose))
    eal.delete_directory(dest + "/_pose")
    return 1


step = (sys.argv[1:] or ["masters"])
if step[0] == "masters":
    do_masters()
elif step[0] == "hues":
    do_hues(step[1:])
elif step[0] == "textures":
    for sid in step[1:]:
        do_textures(sid)
else:
    for sid in step[1:]:
        do_import(sid)
log("done " + " ".join(step))
