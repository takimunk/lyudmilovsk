"""Imports the player's feral pigeon built by blender/birds/hero_pigeon/build_hero_pigeon.py (+ make_morphs.py) onto the
species' own skeleton:

  /Game/Birds/Materials/M_BirdHeroBody                    opaque, one-sided (seen from inside the head it is not there)
  /Game/Birds/FeralPigeon/SK_FeralPigeon_Hero             slots Plumage, Skull (hidden in first person), Feathers, Bare
  /Game/Birds/FeralPigeon/T_FeralPigeon_Hero_*            body albedo / normal / params, + Albedo_<Morph>
  /Game/Birds/FeralPigeon/T_FeralPigeon_HeroFeathers_*    the scanned feather cards' atlas, + Albedo_<Morph>
  /Game/Birds/FeralPigeon/MI_FeralPigeon_{Plumage,Feathers}_Hero[_<Morph>], MI_FeralPigeon_Bare_Hero

One Unreal process per step (Interchange imports once per process here), no editor open on this checkout:
  unreal/scripts/import_hero_pigeon.sh
"""
import json
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
_argv, sys.argv = sys.argv, [sys.argv[0], "none"]      # import_birds runs its step list on import: give it none
import import_birds as B  # noqa: E402
sys.argv = _argv

SID = "FeralPigeon"
SRC = os.path.join(B.EXPORT, SID)
DEST = "%s/%s" % (B.BIRDS, SID)
MORPHS = [m["name"] for m in json.load(open(os.path.join(B.ROOT, "blender", "birds", "hero_pigeon", "morphs.json")))["morphs"] if m["name"]]
eal = B.eal


def do_textures():
    for stem in ("T_FeralPigeon_Hero", "T_FeralPigeon_HeroFeathers"):
        for kind, suffix in (("albedo", "Albedo"), ("normal", "Normal"), ("params", "Params")):
            B.import_texture(os.path.join(SRC, "%s_%s.png" % (stem, suffix)), DEST, "%s_%s" % (stem, suffix), kind)
        for m in MORPHS:
            f = os.path.join(SRC, "%s_Albedo_%s.png" % (stem, m))
            if os.path.exists(f):
                B.import_texture(f, DEST, "%s_Albedo_%s" % (stem, m), "albedo")
    # the old hero atlas' winter / female albedos belong to the old body's UVs: gone with it
    for v in B.VARIANTS:
        for p in ("%s/T_%s_Hero_Albedo_%s" % (DEST, SID, v), "%s/MI_%s_Plumage_Hero_%s" % (DEST, SID, v)):
            if eal.does_asset_exist(p):
                eal.delete_asset(p)
    eal.save_directory(DEST)
    B.log("hero pigeon textures, morphs: " + " ".join(MORPHS))


def body_master():
    B.default_textures()
    m = B.make_master("M_BirdHeroBody", False)
    m.set_editor_property("two_sided", False)
    B.mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def tex(stem, albedo_suffix=""):
    t = lambda s: unreal.load_asset("%s/%s_%s" % (DEST, stem, s))  # noqa: E731
    return {"albedo": t("Albedo" + albedo_suffix), "normal": t("Normal"), "params": t("Params")}


def do_mesh():
    B.log("mesh step: old exists %s" % eal.does_asset_exist("%s/SK_%s_Hero" % (DEST, SID)))
    base = unreal.load_asset("%s/SK_%s" % (DEST, SID))
    if base is None:
        raise RuntimeError("import FeralPigeon first")
    body = body_master()
    plum = unreal.load_asset(B.MATS + "/M_BirdPlumage")
    bare = unreal.load_asset(B.MATS + "/M_BirdBare")
    hue = json.load(open(os.path.join(SRC, "%s_rig.json" % SID))).get("gloss_hue", 0.0)
    mi_body = B.material_instance("MI_%s_Plumage_Hero" % SID, DEST, body, tex("T_FeralPigeon_Hero"), hue)
    mi_feat = B.material_instance("MI_%s_Feathers_Hero" % SID, DEST, plum, tex("T_FeralPigeon_HeroFeathers"), hue)
    old_bare = unreal.load_asset("%s/MI_%s_Bare" % (DEST, SID))      # the species' own legs and toes paint
    mi_bare = B.get_or_create("MI_%s_Bare_Hero" % SID, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi_bare.set_editor_property("parent", old_bare)
    # a feral pigeon's legs and feet are coral red (the species' paint is pale here)
    B.mel.set_material_instance_vector_parameter_value(mi_bare, "Tint", unreal.LinearColor(0.95, 0.34, 0.30, 1))
    eal.save_loaded_asset(mi_bare)
    # the scanned feathers are lit as foliage (light comes through them): a little darker than the scans so the wing
    # sits with the body, not whiter than it
    feathers = [mi_feat]
    for m in MORPHS:
        # ABirdActor::ApplyMorph swaps every slot whose material name has "Plumage" or "Feathers" for <name>_<Morph>
        B.material_instance("MI_%s_Plumage_Hero_%s" % (SID, m), DEST, body, tex("T_FeralPigeon_Hero", "_" + m), hue)
        feathers.append(B.material_instance("MI_%s_Feathers_Hero_%s" % (SID, m), DEST, plum, tex("T_FeralPigeon_HeroFeathers", "_" + m), hue))
    for mi in feathers:
        B.mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(0.74, 0.74, 0.76, 1))
        eal.save_loaded_asset(mi)
    path = DEST + "/_pipe/IP_BirdHero"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    pipe = B.tools.duplicate_asset("IP_BirdHero", DEST + "/_pipe", unreal.load_asset("/Interchange/Pipelines/DefaultGLTFAssetsPipeline"))
    pipe.get_editor_property("common_skeletal_meshes_and_animations_properties").set_editor_property("skeleton", base.skeleton)
    eal.save_loaded_asset(pipe)
    tmp = DEST + "/_hero"
    src = unreal.InterchangeManager.create_source_data(os.path.join(SRC, SID + "_Hero.glb"))
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.override_pipelines.append(unreal.SoftObjectPath("/Interchange/Pipelines/DefaultGLTFPipeline.DefaultGLTFPipeline"))
    params.override_pipelines.append(unreal.SoftObjectPath(pipe.get_path_name()))
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_asset(tmp, src, params)
    mgr.wait_until_all_tasks_done(True)
    B.log("after import: old exists %s, tmp has %s" % (eal.does_asset_exist("%s/SK_%s_Hero" % (DEST, SID)), eal.list_assets(tmp, True)))
    sk = next((eal.load_asset(p) for p in eal.list_assets(tmp, True) if isinstance(eal.load_asset(p), unreal.SkeletalMesh)), None)
    if sk is None:
        raise RuntimeError("no skeletal mesh in FeralPigeon_Hero.glb")
    by_glb = {"Plumage": ("Plumage", mi_body), "Skull": ("Skull", mi_body), "Feathers": ("Feathers", mi_feat), "Bare": ("Bare", mi_bare)}
    mats = sk.get_editor_property("materials")
    for i in range(len(mats)):
        sm = mats[i]
        key = next(k for k in by_glb if k in str(sm.material_slot_name))
        sm.material_slot_name = by_glb[key][0]          # "Skull" is what ABirdActor::SetFirstPerson looks for
        sm.material_interface = by_glb[key][1]
        mats[i] = sm
    sk.set_editor_property("materials", mats)
    sk.set_editor_property("physics_asset", base.get_editor_property("physics_asset"))
    if eal.does_asset_exist("%s/SK_%s_Hero" % (DEST, SID)):
        eal.delete_asset("%s/SK_%s_Hero" % (DEST, SID))
    B.tools.rename_assets([unreal.AssetRenameData(sk, DEST, "SK_%s_Hero" % SID)])
    eal.save_asset("%s/SK_%s_Hero" % (DEST, SID), only_if_is_dirty=False)
    for p in eal.list_assets(tmp, True):
        eal.delete_asset(p)
    for d in (tmp, DEST + "/_pipe"):
        if eal.does_directory_exist(d):
            eal.delete_directory(d)
    # the old hero body's paint (its 4096 card atlas) is not used any more
    for p in ("MI_%s_Eye_Hero" % SID, "MI_%s_Bare_Hero_Winter" % SID, "MI_%s_Eye_Hero_Winter" % SID):
        if eal.does_asset_exist("%s/%s" % (DEST, p)):
            eal.delete_asset("%s/%s" % (DEST, p))
    eal.save_directory(DEST)
    sk = unreal.load_asset("%s/SK_%s_Hero" % (DEST, SID))
    b = sk.get_bounds().box_extent
    B.log("%s hero: bounds %.1f x %.1f x %.1f cm, slots %s" % (SID, b.x * 2, b.y * 2, b.z * 2,
          [str(m.material_slot_name) for m in sk.get_editor_property("materials")]))


def do_clear():
    """The old SK_<Id>_Hero goes in its own process: deleting it in the one that has just loaded it leaves it in memory
    and the new mesh can't take its name."""
    p = "%s/SK_%s_Hero" % (DEST, SID)
    if eal.does_asset_exist(p):
        ok = eal.delete_asset(p)
        B.log("old hero body deleted: %s" % ok)


step = sys.argv[1] if len(sys.argv) > 1 else ""
if step == "clear":
    do_clear()
elif step == "textures":
    do_textures()
elif step == "mesh":
    do_mesh()
else:
    raise SystemExit("import_hero_pigeon.py textures|mesh")
