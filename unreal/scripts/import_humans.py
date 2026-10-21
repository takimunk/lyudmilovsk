"""Imports the MakeHuman-based characters (blender/humans/export/human_<name>.glb) into
/Game/Characters/Humans:

  Humans/SK_Human_Skeleton                 one skeleton shared by every human (same bone layout)
  Humans/Materials/M_Human{Skin,Cloth,Hair,Eye}   master materials
  Humans/<Name>/SK_<Name>                  skeletal mesh (body + hair + clothes, one mesh)
  Humans/<Name>/SK_<Name>_PhysicsAsset, Textures/, Materials/MI_<Name>_<part>

Run headless (no editor open on this checkout):
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script="<this file> zoomer clerk"
With no names it imports every human_*.glb found. Interchange only finds the second import's assets
reliably in a fresh process, so unreal/scripts/import_humans.sh runs one process per character.

Skeleton: root, pelvis, spine_01..03, neck_01, head, jaw, eye_l, eye_r, clavicle/upperarm/lowerarm/
hand + 15 finger bones per side (thumb/index/middle/ring/pinky _01.._03), thigh/calf/foot/ball.
Morph targets: 52 ARKit face units (eyeBlinkLeft, jawOpen, ...) + 15 visemes (aa_02, p_b_m_21, ...).
Rest pose: standing A-pose, feet on the floor at the origin.
"""
import glob
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
EXPORT = os.path.join(ROOT, "blender", "humans", "export")
BASE = "/Game/Characters/Humans"
MATS = BASE + "/Materials"
SKELETON = BASE + "/SK_Human_Skeleton"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty


def log(msg):
    unreal.log_warning("HUMANS " + msg)


# ------------------------------------------------------------------------------ masters

def _new_material(name):
    path = MATS + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = tools.create_asset(name, MATS, unreal.Material, unreal.MaterialFactoryNew())
    # without these usage flags a packaged/-game run silently falls back to the default material
    m.set_editor_property("used_with_skeletal_mesh", True)
    m.set_editor_property("used_with_morph_targets", True)
    return m


def _tex(m, name, x, y, normal=False):
    t = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    t.set_editor_property("parameter_name", name)
    if normal:
        t.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        t.set_editor_property("texture", unreal.load_asset("/Engine/EngineMaterials/FlatNormal"))
    else:
        t.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
    return t


def _scalar(m, name, value, x, y):
    s = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    s.set_editor_property("parameter_name", name)
    s.set_editor_property("default_value", value)
    return s


def _vector(m, name, value, x, y):
    v = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    v.set_editor_property("parameter_name", name)
    v.set_editor_property("default_value", unreal.LinearColor(*value))
    return v


def _tinted_base(m):
    """BaseColor = texture * Tint (Tint defaults to white)."""
    t = _tex(m, "BaseColor", -700, -200)
    tint = _vector(m, "Tint", (1, 1, 1, 1), -700, 50)
    mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -400, -150)
    mel.connect_material_expressions(t, "RGB", mul, "A")
    mel.connect_material_expressions(tint, "", mul, "B")
    mel.connect_material_property(mul, "", MP.MP_BASE_COLOR)
    return t


def build_masters():
    # skin: default lit. Screen-space subsurface (profile) showed red-orange speckle noise on the
    # MakeHuman textures in the editor, so skin stays opaque lit with soft specular.
    m = _new_material("M_HumanSkin")
    _tinted_base(m)
    mel.connect_material_property(_scalar(m, "Roughness", 0.55, -400, 100), "", MP.MP_ROUGHNESS)
    mel.connect_material_property(_scalar(m, "Specular", 0.35, -400, 180), "", MP.MP_SPECULAR)
    mel.connect_material_property(_tex(m, "Normal", -700, 300, normal=True), "RGB", MP.MP_NORMAL)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)

    # cloth / shoes / glasses frames: opaque default lit
    m = _new_material("M_HumanCloth")
    _tinted_base(m)
    mel.connect_material_property(_scalar(m, "Roughness", 0.85, -400, 100), "", MP.MP_ROUGHNESS)
    mel.connect_material_property(_scalar(m, "Specular", 0.4, -400, 180), "", MP.MP_SPECULAR)
    mel.connect_material_property(_tex(m, "Normal", -700, 300, normal=True), "RGB", MP.MP_NORMAL)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)

    # hair, eyebrows, eyelashes: masked cards, two sided, alpha from the texture
    m = _new_material("M_HumanHair")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("opacity_mask_clip_value", 0.4)
    t = _tinted_base(m)
    mel.connect_material_property(t, "A", MP.MP_OPACITY_MASK)
    mel.connect_material_property(_scalar(m, "Roughness", 0.5, -400, 100), "", MP.MP_ROUGHNESS)
    mel.connect_material_property(_scalar(m, "Specular", 0.3, -400, 180), "", MP.MP_SPECULAR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)

    # eyes: glossy; the cornea shell is transparent in the texture alpha
    m = _new_material("M_HumanEye")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("opacity_mask_clip_value", 0.5)
    t = _tinted_base(m)
    mel.connect_material_property(t, "A", MP.MP_OPACITY_MASK)
    mel.connect_material_property(_scalar(m, "Roughness", 0.06, -400, 100), "", MP.MP_ROUGHNESS)
    mel.connect_material_property(_scalar(m, "Specular", 0.6, -400, 180), "", MP.MP_SPECULAR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)


# ------------------------------------------------------------------------------- import

HAIR_WORDS = ("eyebrow", "eyelash", "hair", "beard", "moustache", "short0", "long0", "bob0", "ponytail", "braid", "afro")


def kind_of(slot_name):
    s = slot_name.lower()
    if s.endswith("body") or "teeth" in s or "tongue" in s:
        return "Skin"
    if any(w in s for w in HAIR_WORDS):
        return "Hair"
    if "high-poly" in s or "low-poly" in s or "high_poly" in s:
        return "Eye"
    return "Cloth"


PIPELINE = BASE + "/Import/IP_HumanAssets"


def shared_skeleton_pipeline():
    """A copy of the glTF assets pipeline that binds imported meshes to SK_Human_Skeleton."""
    if eal.does_asset_exist(PIPELINE):
        eal.delete_asset(PIPELINE)
    src = unreal.load_asset("/Interchange/Pipelines/DefaultGLTFAssetsPipeline")
    pipe = tools.duplicate_asset(PIPELINE.split("/")[-1], PIPELINE.rsplit("/", 1)[0], src)
    common = pipe.get_editor_property("common_skeletal_meshes_and_animations_properties")
    common.set_editor_property("skeleton", unreal.load_asset(SKELETON))
    eal.save_loaded_asset(pipe)
    return pipe.get_path_name()


def gltf_material_textures(glb):
    """{material name: (base colour image name, normal image name)} read from the .glb itself, so
    the mapping does not depend on what the Interchange material pipeline made of it."""
    import json
    import struct
    with open(glb, "rb") as f:
        f.read(12)
        n, _ = struct.unpack("<II", f.read(8))
        g = json.loads(f.read(n))
    imgs = [im.get("name", "") for im in g.get("images", [])]
    tex = [imgs[t["source"]] if "source" in t else None for t in g.get("textures", [])]
    out = {}
    for m in g.get("materials", []):
        b = m.get("pbrMetallicRoughness", {}).get("baseColorTexture")
        nrm = m.get("normalTexture")
        pbr = m.get("pbrMetallicRoughness", {})
        out[_ue_name(m["name"])] = (_ue_name(tex[b["index"]]) if b else None,
                                    _ue_name(tex[nrm["index"]]) if nrm else None,
                                    pbr.get("baseColorFactor", [1, 1, 1, 1]), pbr.get("roughnessFactor", 0.85))
    return out


def gltf_animation_names(glb):
    import json
    import struct
    with open(glb, "rb") as f:
        f.read(12)
        n, _ = struct.unpack("<II", f.read(8))
        g = json.loads(f.read(n))
    return [a.get("name", "") for a in g.get("animations", [])]


def _ue_name(s):
    import re
    return re.sub(r"[^A-Za-z0-9_]", "_", s or "")


def import_glb(name):
    glb = os.path.join(EXPORT, "human_%s.glb" % name)
    title = name.capitalize()
    dest = "%s/%s" % (BASE, title)
    tmp = dest + "/_import"
    if eal.does_directory_exist(dest):
        eal.delete_directory(dest)
    src = unreal.InterchangeManager.create_source_data(glb)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    if eal.does_asset_exist(SKELETON):        # import straight onto the shared skeleton
        params.override_pipelines.append(unreal.SoftObjectPath("/Interchange/Pipelines/DefaultGLTFPipeline.DefaultGLTFPipeline"))
        params.override_pipelines.append(unreal.SoftObjectPath(shared_skeleton_pipeline()))
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_asset(tmp, src, params)
    mgr.wait_until_all_tasks_done(True)

    mesh = None
    textures = []
    for p in eal.list_assets(tmp, True):
        a = eal.load_asset(p)
        if isinstance(a, unreal.SkeletalMesh):
            mesh = a
        elif isinstance(a, unreal.Texture2D):
            textures.append(a)
    if mesh is None:
        for p in eal.list_assets(dest, True):
            log("  found %s %s" % (eal.load_asset(p).get_class().get_name(), p))
        raise RuntimeError("no skeletal mesh in " + glb)

    texmap = gltf_material_textures(glb)
    gltf_anims = gltf_animation_names(glb)
    by_name = {}
    for t in textures:
        nm = _ue_name(t.get_name())
        eal.rename_asset(t.get_path_name(), "%s/Textures/T_%s" % (dest, nm))
        by_name[nm] = unreal.load_asset("%s/Textures/T_%s" % (dest, nm))

    # materials: our masters, with the imported texture of each slot
    mats = mesh.get_editor_property("materials")
    for i in range(len(mats)):
        sm = mats[i]
        old = sm.material_interface
        slot = str(sm.material_slot_name)
        kind = kind_of(slot)
        bname, nname, factor, rough = texmap.get(_ue_name(slot), (None, None, [1, 1, 1, 1], 0.85))
        base_tex = by_name.get(bname)
        nrm_tex = by_name.get(nname)
        mi_name = "MI_%s_%s" % (title, slot.split(".")[-1].replace("-", "_"))
        mi = tools.create_asset(mi_name, dest + "/Materials", unreal.MaterialInstanceConstant,
                                unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", unreal.load_asset("%s/M_Human%s" % (MATS, kind)))
        if base_tex:
            mel.set_material_instance_texture_parameter_value(mi, "BaseColor", base_tex)
        elif bname:
            log("  WARNING texture %s of slot %s not found" % (bname, slot))
        if factor[:3] != [1, 1, 1]:
            mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(*factor))
        if kind in ("Skin", "Cloth") and nrm_tex:
            mel.set_material_instance_texture_parameter_value(mi, "Normal", nrm_tex)
        if kind == "Cloth":
            mel.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
        eal.save_loaded_asset(mi)
        sm.material_interface = mi
        mats[i] = sm
        log("  slot %s -> %s (%s)" % (slot, mi_name, kind))
    mesh.set_editor_property("materials", mats)

    # shared skeleton: the first human imported provides it, the rest point at it
    own_skel = mesh.skeleton
    if not eal.does_asset_exist(SKELETON):
        eal.rename_asset(own_skel.get_path_name(), SKELETON)
    elif own_skel.get_path_name() != unreal.load_asset(SKELETON).get_path_name():
        log("  WARNING %s did not bind to the shared skeleton (%s)" % (title, own_skel.get_path_name()))
    eal.save_loaded_asset(mesh)

    eal.rename_asset(mesh.get_path_name(), "%s/SK_%s" % (dest, title))
    anims = []
    for p in eal.list_assets(tmp, True):
        a = eal.load_asset(p)
        if isinstance(a, unreal.PhysicsAsset):
            eal.rename_asset(p, "%s/SK_%s_PhysicsAsset" % (dest, title))
        elif isinstance(a, unreal.AnimSequence):
            nm = a.get_name()
            nm = nm.replace("human_" + name, "").lstrip("_")     # Interchange prefixes the file name
            if "Sit_" in nm:
                nm = nm[nm.find("Sit_"):]
            elif len(gltf_anims) == 1:              # a lone animation is named after the file
                nm = gltf_anims[0]
            new = "%s/Anims/A_%s_%s" % (dest, title, nm)
            eal.rename_asset(p, new)
            anim = unreal.load_asset(new)
            anim.set_editor_property("enable_root_motion", False)
            eal.save_loaded_asset(anim)
            anims.append(new.split("/")[-1])
    for p in eal.list_assets(tmp, True):
        eal.delete_asset(p)
    eal.delete_directory(tmp)
    mesh = unreal.load_asset("%s/SK_%s" % (dest, title))
    log("%s: %d morphs, %d materials, skeleton %s, extent %s, anims %s" % (
        title, len(mesh.morph_targets), len(mesh.materials), mesh.skeleton.get_path_name(),
        mesh.get_bounds().box_extent, anims))
    eal.save_directory(dest)
    return mesh


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("-")]
    names = args or [os.path.basename(p)[6:-4] for p in sorted(glob.glob(os.path.join(EXPORT, "human_*.glb")))]
    if "--keep-masters" not in sys.argv or not eal.does_asset_exist(MATS + "/M_HumanSkin"):
        build_masters()
    for n in ([] if "--masters-only" in sys.argv else names):
        import_glb(n)
    eal.save_directory(BASE)
    log("done")


main()
