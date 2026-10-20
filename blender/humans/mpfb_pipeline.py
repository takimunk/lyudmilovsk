"""Realistic rigged humans for Lyudmilovsk, built on MakeHuman's MPFB2 (Blender extension).

MPFB2 assets (base mesh, targets, rigs, skins, system clothes) are CC0; community clothes/hair are
CC0 or CC-BY (see SOURCES.md). This module wraps the MPFB services into one call per character:

    basemesh, rig = build(spec)        # spec = dict, see build_humans.py
    export_glb(basemesh, path)

Skeleton: MPFB's "game_engine" rig (UE4 mannequin bone names: root, pelvis, spine_01..03,
neck_01, head, clavicle/upperarm/lowerarm/hand, 15 finger bones per hand, thigh/calf/foot/ball)
plus face bones taken from MPFB's default rig: jaw, eye_l, eye_r (children of head).
Face: 52 ARKit face-unit shape keys + 15 Microsoft visemes on the body; eyebrows/eyelashes/
teeth/tongue get the same shape keys interpolated, so blinks and lip sync work in UE.
"""
import copy
import importlib
import json
import math
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
CACHE = os.path.join(HERE, "cache")


def _mpfb(module, symbol):
    for name in list(sys.modules):
        if name.endswith("mpfb." + module):
            return getattr(importlib.import_module(name), symbol)
    raise ImportError("MPFB2 extension is not enabled (looked for mpfb.%s); see README.md" % module)


def _services():
    s = {}
    for mod, sym in [("services.humanservice", "HumanService"), ("services.assetservice", "AssetService"),
                     ("services.exportservice", "ExportService"), ("services.objectservice", "ObjectService"),
                     ("services.locationservice", "LocationService"), ("services.targetservice", "TargetService"),
                     ("services.rigservice", "RigService"), ("services.faceservice", "FaceService"),
                     ("entities.rig", "Rig")]:
        s[sym] = _mpfb(mod, sym)
    return s


S = None


def services():
    global S
    if S is None:
        S = _services()
    return S


# ----------------------------------------------------------------------------------------- rig

FACE_BONES = {  # our name: (MPFB default-rig bone, parent in our rig)
    "jaw": ("jaw", "head"),
    "eye_l": ("eye.L", "head"),
    "eye_r": ("eye.R", "head"),
}


def _ue_rig_files():
    """Writes cache/rig.ue_face.json + weights.ue_face.json: game_engine rig + jaw/eye bones."""
    os.makedirs(CACHE, exist_ok=True)
    rigs = os.path.join(services()["LocationService"].get_mpfb_data("rigs"), "standard")
    rig = json.load(open(os.path.join(rigs, "rig.game_engine.json")))
    drig = json.load(open(os.path.join(rigs, "rig.default.json")))
    gw = json.load(open(os.path.join(rigs, "weights.game_engine.json")))
    dw = json.load(open(os.path.join(rigs, "weights.default.json")))["weights"]

    rig["root"] = rig.pop("Root")
    for b in rig.values():
        if b.get("parent") == "Root":
            b["parent"] = "root"
    for ours, (theirs, parent) in FACE_BONES.items():
        b = copy.deepcopy(drig[theirs])
        b["parent"] = parent
        b["use_connect"] = False
        rig[ours] = b

    weights = copy.deepcopy(gw)
    w = weights["weights"]
    if "Root" in w:
        w["root"] = w.pop("Root")
    head = dict(w.get("head", []))
    for ours, (theirs, _) in FACE_BONES.items():
        w[ours] = dw[theirs]
        if ours == "jaw":            # jaw takes its share from the head
            for vi, jv in dw[theirs]:
                if vi in head:
                    head[vi] = max(0.0, head[vi] - jv)
    w["head"] = [[vi, v] for vi, v in sorted(head.items()) if v > 1e-4]

    rf, wf = os.path.join(CACHE, "rig.ue_face.json"), os.path.join(CACHE, "weights.ue_face.json")
    json.dump(rig, open(rf, "w"))
    json.dump(weights, open(wf, "w"))
    return rf, wf


def add_ue_rig(basemesh):
    s = services()
    rf, wf = _ue_rig_files()
    rig = s["Rig"].from_json_file_and_basemesh(rf, basemesh)
    arm = rig.create_armature_and_fit_to_basemesh()
    arm.name = arm.data.name = basemesh.name + ".rig"
    s["RigService"].normalize_rotation_mode(arm)
    basemesh.parent = arm
    arm.location = basemesh.location
    basemesh.location = (0, 0, 0)
    s["RigService"].load_weights(arm, basemesh, wf)
    s["RigService"].ensure_armature_modifier(basemesh, arm)
    return arm


# ------------------------------------------------------------------------------------- assets

def asset_path(name, subdir):
    p = services()["AssetService"].find_asset_absolute_path(name, asset_subdir=subdir)
    if p is None:
        raise FileNotFoundError("%s/%s not installed (see README.md: asset packs)" % (subdir, name))
    return p


def find_mhclo(folder, subdir="clothes"):
    """Asset by its folder name (folder names are unique; .mhclo file names are not always)."""
    root = services()["LocationService"].get_user_data(subdir)
    d = os.path.join(root, folder)
    files = [f for f in os.listdir(d) if f.endswith(".mhclo")] if os.path.isdir(d) else []
    if not files:
        raise FileNotFoundError("%s/%s not installed (see README.md: asset packs)" % (subdir, folder))
    return os.path.join(d, files[0])


def find_skin(folder):
    root = services()["LocationService"].get_user_data("skins")
    d = os.path.join(root, folder)
    files = [f for f in os.listdir(d) if f.endswith(".mhmat")]
    return os.path.join(d, files[0])


def add_asset(basemesh, folder, subdir, atype):
    s = services()
    before = set(bpy.data.objects)
    s["HumanService"].add_mhclo_asset(find_mhclo(folder, subdir), basemesh, asset_type=atype,
                                      material_type="GAMEENGINE", subdiv_levels=0)
    new = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
    return new[0] if new else None


# -------------------------------------------------------------------------------------- build

def build(spec):
    """spec keys: name, macro{gender,age,muscle,weight,proportions,height,race{asian,caucasian,african}},
    targets{relative target path: weight}, skin, eyebrows, eyelashes, hair, clothes[list of folders],
    eyes (material name, optional). Returns (basemesh, armature, {folder: object})."""
    s = services()
    HS = s["HumanService"]
    macro = dict(s["TargetService"].get_default_macro_info_dict())
    macro.update({k: v for k, v in spec.get("macro", {}).items() if k != "race"})
    if "race" in spec.get("macro", {}):
        macro["race"] = spec["macro"]["race"]
    basemesh = HS.create_human(macro_detail_dict=macro)
    basemesh.name = spec["name"]

    troot = s["LocationService"].get_mpfb_data("targets")
    for rel, wgt in spec.get("targets", {}).items():
        s["TargetService"].load_target(basemesh, os.path.join(troot, rel + ".target.gz"), weight=wgt)

    # face shape keys before rig/assets, so proxies can get them interpolated
    s["FaceService"].load_targets(basemesh, load_microsoft_visemes=True, load_arkit_faceunits=True)

    arm = add_ue_rig(basemesh)
    HS.set_character_skin(find_skin(spec["skin"]), basemesh, skin_type="GAMEENGINE")

    objs = {}
    objs["eyes"] = add_asset(basemesh, spec.get("eyes", "high-poly"), "eyes", "Eyes")
    objs["eyebrows"] = add_asset(basemesh, spec.get("eyebrows", "eyebrow001"), "eyebrows", "Eyebrows")
    objs["eyelashes"] = add_asset(basemesh, spec.get("eyelashes", "eyelashes01"), "eyelashes", "Eyelashes")
    objs["teeth"] = add_asset(basemesh, "teeth_base", "teeth", "Teeth")
    objs["tongue"] = add_asset(basemesh, "tongue01", "tongue", "Tongue")
    if spec.get("hair"):
        objs["hair"] = add_asset(basemesh, spec["hair"], "hair", "Hair")
    for c in spec.get("clothes", []):
        objs[c] = add_asset(basemesh, c, "clothes", "Clothes")
    s["FaceService"].interpolate_targets(basemesh)
    return basemesh, arm, objs


def bake_for_export(basemesh):
    """Copies the character, bakes delete-masks (hidden body under clothes) and removes helpers."""
    s = services()
    root = s["ExportService"].create_character_copy(basemesh, name_suffix="_export")
    bm = s["ObjectService"].find_object_of_type_amongst_nearest_relatives(root, "Basemesh")
    s["ExportService"].bake_modifiers_remove_helpers(bm, bake_masks=True, bake_subdiv=False,
                                                     remove_helpers=True, also_proxy=True)
    bm = tidy_export_copy(root)
    return root, bm


def is_face_key(name):
    face = set(_mpfb("services.faceservice", "ARKIT_FACEUNITS")) | set(_mpfb("services.faceservice", "MICROSOFT_VISEMES"))
    return name in face


def bake_body_targets(obj):
    """Folds every modelling shape key (age, gender, nose...) into the basis and keeps only the face
    keys (ARKit units + visemes), each still relative to the new basis."""
    sk = obj.data.shape_keys
    if not sk:
        return
    import numpy as np
    blocks = sk.key_blocks
    basis = blocks[0]
    n = len(basis.data)
    b = np.empty(n * 3)
    basis.data.foreach_get("co", b)
    total = np.zeros(n * 3)
    model = [k for k in blocks[1:] if not is_face_key(k.name)]
    for k in model:
        c = np.empty(n * 3)
        k.data.foreach_get("co", c)
        total += (c - b) * k.value
    for k in blocks:
        if k in model:
            continue
        c = np.empty(n * 3)
        k.data.foreach_get("co", c)
        k.data.foreach_set("co", c + total)
    for k in model:
        obj.shape_key_remove(k)
    for k in obj.data.shape_keys.key_blocks:
        k.value = 0.0
    me = obj.data
    me.vertices.foreach_set("co", (b + total))
    me.update()


def tidy_export_copy(root):
    """Drops helper objects (bone-shape spheres etc.) and bakes modelling keys on every mesh."""
    for c in list(root.children_recursive):
        if c.type == "MESH" and not c.data.materials:
            bpy.data.objects.remove(c)
    meshes = [c for c in root.children_recursive if c.type == "MESH"]
    for c in meshes:
        bake_body_targets(c)
    # one skeletal mesh per character in UE: join body, hair, garments (materials stay sections,
    # shape keys are merged by name)
    body = [m for m in meshes if m.name.endswith("_export") and "." not in m.name.replace("_export", "")]
    body = body[0] if body else meshes[0]
    bpy.ops.object.select_all(action="DESELECT")
    for m in meshes:
        m.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.join()
    name = root.name.replace(".rig_export", "")
    body.name = body.data.name = "SK_" + name.capitalize()
    root.name = "Armature_" + name
    return body


def export_glb(root, path, actions=None):
    """Exports armature + child meshes (skinned, with shape keys) as glTF binary."""
    bpy.ops.object.select_all(action="DESELECT")
    root.select_set(True)
    for c in root.children_recursive:
        c.select_set(True)
    bpy.context.view_layer.objects.active = root
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=path, use_selection=True, export_format="GLB",
                              export_skins=True, export_morph=True, export_morph_normal=False,
                              export_animations=bool(actions), export_animation_mode="ACTIONS",
                              export_force_sampling=True, export_optimize_animation_size=False,
                              export_anim_single_armature=True, export_reset_pose_bones=True,
                              export_def_bones=False, export_yup=True, export_apply=False,
                              export_image_format="AUTO", export_jpeg_quality=90)
