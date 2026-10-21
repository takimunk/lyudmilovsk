"""Imports the first-person hero (blender/hero/export/human_hero.glb: the Zoomer body in Murmuration's
clothes) onto the shared human skeleton, with Murmuration's idle and walk baked onto it.

  /Game/Characters/Hero/SK_Hero                 skeletal mesh on /Game/Characters/Humans/SK_Human_Skeleton
  /Game/Characters/Hero/Materials/MI_Hero_*     on the human masters (Humans/Materials/M_Human*)
  /Game/Characters/Hero/Anims/A_Hero_Idle, A_Hero_Walk   in place (Walk: one gait cycle)

Needs the humans content (Humans/SK_Human_Skeleton, Materials, Import/IP_HumanAssets). Run headless
with no editor open on this checkout:
  UnrealEditor <uproject> -run=pythonscript -script="<this file> [import|masters]"
(or unreal/scripts/import_hero.sh)
"""
import json
import os
import re
import struct
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
GLB = os.path.join(ROOT, "blender", "hero", "export", "human_hero.glb")
DEST = "/Game/Characters/Hero"
HUMANS = "/Game/Characters/Humans"
SKELETON = HUMANS + "/SK_Human_Skeleton"
PIPELINE = HUMANS + "/Import/IP_HumanAssets"
HAIR_WORDS = ("hair", "eyebrow", "eyelash")
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log_warning("HERO " + msg)


def ue_name(s):
    return re.sub(r"[^A-Za-z0-9_]", "_", s or "")


def kind_of(slot):
    s = slot.lower()
    if s.endswith("body") or "teeth" in s or "tongue" in s:
        return "Skin"
    if any(w in s for w in HAIR_WORDS):
        return "Hair"
    if "high-poly" in s or "high_poly" in s:
        return "Eye"
    return "Cloth"


def gltf_materials():
    with open(GLB, "rb") as f:
        f.read(12)
        n, _ = struct.unpack("<II", f.read(8))
        g = json.loads(f.read(n))
    imgs = [im.get("name", "") for im in g.get("images", [])]
    tex = [imgs[t["source"]] if "source" in t else None for t in g.get("textures", [])]
    out = {}
    for m in g.get("materials", []):
        pbr = m.get("pbrMetallicRoughness", {})
        b, nrm = pbr.get("baseColorTexture"), m.get("normalTexture")
        out[ue_name(m["name"])] = (ue_name(tex[b["index"]]) if b else None, ue_name(tex[nrm["index"]]) if nrm else None,
                                   pbr.get("baseColorFactor", [1, 1, 1, 1]), pbr.get("roughnessFactor", 0.85))
    return out


def skeletal_usage():
    """The human masters must allow skeletal meshes, or a packaged/-game run falls back to the
    default material (the editor sets the flag on first use, a cooked game can't)."""
    for kind in ("Skin", "Cloth", "Hair", "Eye"):
        m = unreal.load_asset("%s/Materials/M_Human%s" % (HUMANS, kind))
        if m and not m.get_editor_property("used_with_skeletal_mesh"):
            m.set_editor_property("used_with_skeletal_mesh", True)
            mel.recompile_material(m)
            eal.save_loaded_asset(m)
            log("M_Human%s: used with skeletal mesh" % kind)


def do_import():
    skeletal_usage()
    tmp = DEST + "/_import"
    for p in (DEST + "/SK_Hero", DEST + "/SK_Hero_PhysicsAsset"):
        if eal.does_asset_exist(p):
            eal.delete_asset(p)
    for d in (DEST + "/Materials", DEST + "/Textures", tmp):
        if eal.does_directory_exist(d):
            eal.delete_directory(d)
    src = unreal.InterchangeManager.create_source_data(GLB)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.override_pipelines.append(unreal.SoftObjectPath("/Interchange/Pipelines/DefaultGLTFPipeline.DefaultGLTFPipeline"))
    params.override_pipelines.append(unreal.SoftObjectPath(PIPELINE))
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_asset(tmp, src, params)
    mgr.wait_until_all_tasks_done(True)

    mesh, textures = None, []
    for p in eal.list_assets(tmp, True):
        a = eal.load_asset(p)
        if isinstance(a, unreal.SkeletalMesh):
            mesh = a
        elif isinstance(a, unreal.Texture2D):
            textures.append(a)
    if mesh is None:
        raise RuntimeError("no skeletal mesh in " + GLB)
    if mesh.skeleton.get_path_name() != unreal.load_asset(SKELETON).get_path_name():
        raise RuntimeError("SK_Hero did not bind to " + SKELETON)
    by_name = {}
    for t in textures:
        nm = ue_name(t.get_name())
        eal.rename_asset(t.get_path_name(), "%s/Textures/T_%s" % (DEST, nm))
        by_name[nm] = unreal.load_asset("%s/Textures/T_%s" % (DEST, nm))
    texmap = gltf_materials()
    mats = mesh.get_editor_property("materials")
    for i in range(len(mats)):
        sm = mats[i]
        slot = str(sm.material_slot_name)
        kind = kind_of(slot)
        bname, nname, factor, rough = texmap.get(ue_name(slot), (None, None, [1, 1, 1, 1], 0.85))
        mi = tools.create_asset("MI_Hero_%s" % ue_name(slot.split(".")[-1]), DEST + "/Materials",
                                unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", unreal.load_asset("%s/Materials/M_Human%s" % (HUMANS, kind)))
        if by_name.get(bname):
            mel.set_material_instance_texture_parameter_value(mi, "BaseColor", by_name[bname])
        if factor[:3] != [1, 1, 1]:
            mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(*factor))
        if kind in ("Skin", "Cloth") and by_name.get(nname):
            mel.set_material_instance_texture_parameter_value(mi, "Normal", by_name[nname])
        if kind == "Cloth":
            mel.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
        eal.save_loaded_asset(mi)
        sm.material_interface = mi
        mats[i] = sm
        log("slot %s -> %s" % (slot, kind))
    mesh.set_editor_property("materials", mats)
    eal.save_loaded_asset(mesh)
    eal.rename_asset(mesh.get_path_name(), DEST + "/SK_Hero")
    for p in eal.list_assets(tmp, True):
        a = eal.load_asset(p)
        if isinstance(a, unreal.PhysicsAsset):
            eal.rename_asset(p, DEST + "/SK_Hero_PhysicsAsset")
        elif isinstance(a, unreal.AnimSequence):
            clip = next((c for c in ("Idle", "Walk") if c in a.get_name()), None)
            if clip is None:
                eal.delete_asset(p)
                continue
            new = "%s/Anims/A_Hero_%s" % (DEST, clip)
            if eal.does_asset_exist(new):
                eal.delete_asset(new)
            eal.rename_asset(p, new)
            anim = unreal.load_asset(new)
            anim.set_editor_property("enable_root_motion", False)
            eal.save_loaded_asset(anim)
            log("anim A_Hero_%s %.2f s" % (clip, anim.get_play_length()))
        else:
            eal.delete_asset(p)
    eal.delete_directory(tmp)
    mesh = unreal.load_asset(DEST + "/SK_Hero")
    log("SK_Hero: %d materials, %d morphs, bounds %s" % (len(mesh.materials), len(mesh.morph_targets), mesh.get_bounds().box_extent))
    eal.save_directory(DEST)


step = (sys.argv[1:] or ["import"])[0]
{"import": do_import, "masters": skeletal_usage}[step]()
log("done " + step)
