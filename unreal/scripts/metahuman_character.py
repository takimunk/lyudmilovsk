"""A MetaHuman for a generated character: the hi3d model is only the reference (Dima, 2026-10-28).

    UnrealEditor-Cmd <uproject> -run=pythonscript -script="unreal/scripts/metahuman_character.py nao" -unattended
    MH_STEPS=import,conform,rig,textures,hair,build (default: all, in that order)

Inputs from blender/characters/mh_source.py (blender/characters/<id>/export/mh/): head.glb (the model's head and neck
below the hair), portrait.png + portrait.json (a front picture and its camera, for MetaHuman's face landmark tracker).
Output: /Game/MetaHumans/<Id>/MHC_<Id> (the editable character) and its built assets under /Game/MetaHumans/<Id>/Build.
rig and textures are MetaHuman cloud services: they need the editor's Epic account login.
"""
import json
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
eal = unreal.EditorAssetLibrary
mhs = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)


def log(m):
    unreal.log_warning("MHC " + str(m))


TARGET = os.environ.get("MH_TARGET", "head")              # head: the head below the hair; model: the whole model


def import_head(cid, dest):
    glb = os.path.join(ROOT, "blender", "characters", cid, "export", "mh", TARGET + ".glb")
    task = unreal.AssetImportTask()
    task.filename = glb
    task.destination_path = dest
    task.destination_name = "SM_%s_%s_ref" % (cid.capitalize(), TARGET)
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    meshes = [p for p in task.imported_object_paths if "SM_" in p or "Mesh" in p]
    log("imported %s -> %s" % (glb, task.imported_object_paths))
    for p in task.imported_object_paths:
        a = unreal.load_asset(p)
        if isinstance(a, (unreal.StaticMesh, unreal.SkeletalMesh)):
            return a
    raise RuntimeError("no mesh imported from " + glb)


def import_template(cid, dest):
    """blender/characters/mh_wrap.py's sections (MetaHuman's own face mesh wrapped onto the model) as static meshes."""
    src = os.path.join(ROOT, "blender", "characters", cid, "export", "mh")
    tasks = []
    for part in ("head", "eyeLeft", "eyeRight", "teeth"):
        t = unreal.AssetImportTask()
        t.filename = os.path.join(src, "template_%s.fbx" % part)
        t.destination_path = dest + "/Template"
        t.destination_name = "SM_%s_template_%s" % (cid.capitalize(), part)
        t.automated, t.replace_existing, t.save = True, True, True
        tasks.append(t)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    out = {}
    for part, t in zip(("head", "eyeLeft", "eyeRight", "teeth"), tasks):
        out[part] = next((unreal.load_asset(p) for p in t.imported_object_paths
                          if isinstance(unreal.load_asset(p), unreal.StaticMesh)), None)
        log("template %s -> %s" % (part, out[part].get_path_name() if out[part] else None))
    return out


def from_template(ch, parts):
    p = unreal.ImportFromTemplateParams()
    p.match_vertices_by_u_vs = True
    p.use_eye_meshes = True
    p.use_teeth_mesh = True
    p.alignment_options = getattr(unreal.MetaHumanAlignmentOptions, os.environ.get("MH_ALIGN", "ROTATION_TRANSLATION"),
                                  unreal.MetaHumanAlignmentOptions.SCALING_ROTATION_TRANSLATION)
    r = mhs.import_from_template(ch, parts["head"], parts["eyeLeft"], parts["eyeRight"], parts["teeth"], p)
    log("import_from_template: %s (align %s)" % (r, p.alignment_options))


def character_asset(cid, dest):
    name = os.environ.get("MH_NAME", "MHC_" + cid.capitalize())
    path = "%s/%s" % (dest, name)
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    if os.environ.get("MH_FROM"):                              # start from another of ours
        return eal.duplicate_asset(os.environ["MH_FROM"], path)
    preset = os.environ.get("MH_PRESET")                      # start from one of MetaHuman's own faces
    if preset:
        return eal.duplicate_asset("/MetaHumanCharacter/Optional/Presets/" + preset, path)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name="MHC_" + cid.capitalize(), package_path=dest, asset_class=unreal.MetaHumanCharacter,
        factory=unreal.new_object(type=unreal.MetaHumanCharacterFactoryNew))


def conform(ch, head, cid):
    src = os.path.join(ROOT, "blender", "characters", cid, "export", "mh")
    cam = json.load(open(os.path.join(src, "portrait.json")))
    size, pixels = unreal.PromotedFrameUtils.get_promoted_frame_as_pixel_array_from_disk(os.path.join(src, "portrait.png"))
    res = mhs.track_face_landmarks_from_image(pixels, size.x, size.y)
    if isinstance(res, tuple) and len(res) == 1:
        res = res[0]
    if not res:
        raise RuntimeError("no face landmarks found on the portrait")
    log("landmarks: %d curves" % len(res))
    verts, idx, *_ = mhs.get_mesh_data_for_conforming(head)
    log("head target: %d verts, %d tris" % (len(verts), len(idx) // 3))
    p = unreal.ConformTargetParams()
    combined = TARGET == "model"
    p.conform_target_mesh.target_parts_type = unreal.TargetPartsType.COMBINED if combined else unreal.TargetPartsType.HEAD_ONLY
    p.estimate_body_joints_from_mesh = combined
    for name, val in ((("body_vertices", verts), ("body_vertex_indices", idx)) if combined else
                      (("head_vertices", verts), ("head_vertex_indices", idx))):
        if hasattr(p.conform_target_mesh, name):
            setattr(p.conform_target_mesh, name, val)
    if not hasattr(p.conform_target_mesh, "head_vertices"):
        log("conform_target_mesh fields: %s" % [n for n in dir(p.conform_target_mesh) if not n.startswith("_")])
    p.auto_solve = True
    p.body_conform_solve_settings.pipeline_name = "combined" if combined else "head_only"   # IdentityTemplate/pipeline_presets.json
    v = unreal.MinimalViewInfo()
    v.location = unreal.Vector(*cam["location_cm"])
    v.rotation = unreal.Rotator(pitch=cam["pitch"], yaw=cam["yaw"], roll=0.0)
    v.fov = cam["fov"]
    v.aspect_ratio = float(size.x) / float(size.y)
    v.projection_mode = unreal.CameraProjectionMode.PERSPECTIVE
    p.curve_tracking_points = res
    p.camera_view_info = v
    p.image_size = size
    key = unreal.MetaHumanCharacterTargetMeshKey()
    if combined:
        key.combined_mesh = head
    else:
        key.head_mesh = head
    if not mhs.conform_to_target_meshes(ch, key, p):
        raise RuntimeError("conform_to_target_meshes failed")
    if combined:
        mhs.commit_posed_state_as_a_pose(ch, key)
    log("conformed to %s" % head.get_path_name())


# per character: grooms from /MetaHumanCharacter/Optional/Grooms/Bindings/<slot>/WI_*, hair melanin, skin tone (u, v)
LOOK = {
    "nao": {"Hair": "WI_Hair_M_BobBangs", "Eyebrows": "WI_Eyebrows_M_Natural", "Eyelashes": "WI_Eyelashes_L_SlightCurl",
            "melanin": 1.0, "skin_uv": (0.18, 0.35), "clear": ("Beard", "Mustache"), "height": 160.0},
}


def set_look(ch, cid):
    look = LOOK.get(cid, {})
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    col = mhs.get_preview_collection(ch)
    keys = {}
    for slot in ("Hair", "Eyebrows", "Eyelashes"):
        if slot not in look:
            continue
        pkg = "/MetaHumanCharacter/Optional/Grooms/Bindings/%s/%s" % (slot, look[slot])
        data = reg.get_assets_by_package_name(pkg)
        if not data:
            log("no wardrobe item " + pkg)
            continue
        key = col.try_add_item_from_wardrobe_item(slot_name=slot, wardrobe_item=data[0].get_asset())
        col.default_instance.set_single_slot_selection(slot_name=slot, item_key=key)     # replaces the preset's
        keys[slot] = key
    for slot in look.get("clear", ()):
        col.default_instance.set_single_slot_selection(slot_name=slot, item_key=unreal.MetaHumanPaletteItemKey())
    mhs.on_edit_preview_collection(ch)
    mhs.assemble_for_preview(character=ch)
    for slot, key in keys.items():
        for prm in col.default_instance.get_instance_parameters(item_path=unreal.MetaHumanPaletteItemPath(item_key=key)):
            if prm.name == "Melanin" and "melanin" in look:
                prm.set_float(value=look["melanin"])
    mhs.on_edit_preview_collection(ch)
    if "skin_uv" in look and not os.environ.get("MH_PRESET"):
        sk = unreal.MetaHumanCharacterSkinProperties()
        sk.u, sk.v = look["skin_uv"]
        st = unreal.MetaHumanCharacterSkinSettings()
        st.skin = sk
        mhs.commit_skin_settings(ch, st)
    log("look: %s" % {k: v for k, v in look.items()})


def shape(ch, cid):
    """Likeness without losing the start face: its face model coefficients blended toward the conformed one
    (MH_TOWARD, weight MH_BLEND), body height and eye colour from LOOK."""
    look = LOOK.get(cid, {})
    toward = os.environ.get("MH_TOWARD")
    if toward:
        other = unreal.load_asset(toward)
        mhs.try_add_object_to_edit(other)
        a, b = list(mhs.get_face_model_coefficients(ch)), list(mhs.get_face_model_coefficients(other))
        mhs.remove_object_to_edit(other)
        w = float(os.environ.get("MH_BLEND", "0.35"))
        mix = a[:2] + [(1 - w) * x + w * y for x, y in zip(a[2:], b[2:])]
        mhs.set_face_model_coefficients(ch, mix)
        log("face blended %.2f toward %s" % (w, toward))
    if "height" in look:
        cons = mhs.get_body_constraints(ch)
        for c in cons:
            if str(c.name) == "Height":
                c.target_measurement = look["height"]
                c.is_active = True
        mhs.set_body_constraints(ch, cons)
        mhs.commit_body_state(ch, mhs.get_body_state(ch)) if hasattr(mhs, "get_body_state") else None
        log("height %.0f cm" % look["height"])
    e = ch.eyes_settings
    log("eye settings %s" % str(e.eye_left)[:900])
    if "iris" in look:
        log("eye fields %s" % [n for n in dir(e.eye_left) if not n.startswith("_")][:30])
        for side in (e.eye_left, e.eye_right):
            for path in look["iris"]:
                obj = side
                *head, last = path[0].split(".")
                for h in head:
                    obj = getattr(obj, h)
                setattr(obj, last, path[1])
        mhs.commit_eyes_settings(ch, e)


def main():
    cid = os.environ.get("MH_ID") or (sys.argv[-1] if len(sys.argv) > 1 else "nao")
    steps = os.environ.get("MH_STEPS", "import,conform,rig,textures,hair,build").split(",")
    dest = "/Game/MetaHumans/" + cid.capitalize()
    head = unreal.load_asset("%s/%s/StaticMeshes/SM_%s_%s_ref" % (dest, TARGET, cid.capitalize(), TARGET))
    if "import" in steps or (head is None and "conform" in steps):
        head = import_head(cid, dest)
    ch = character_asset(cid, dest)
    if not mhs.try_add_object_to_edit(ch):
        raise RuntimeError("cannot edit " + ch.get_path_name())
    try:
        if "template" in steps:
            before = list(mhs.get_face_model_coefficients(ch))
            from_template(ch, import_template(cid, dest))
            after = list(mhs.get_face_model_coefficients(ch))
            log("template moved the face model by %.2f" % sum((a - b) ** 2 for a, b in zip(before[2:], after[2:])) ** 0.5)
        if "conform" in steps:
            before = list(mhs.get_face_model_coefficients(ch))
            conform(ch, head, cid)
            after = list(mhs.get_face_model_coefficients(ch))
            log("conform moved the face model by %.2f" % sum((a - b) ** 2 for a, b in zip(before[2:], after[2:])) ** 0.5)
        if "shape" in steps:
            shape(ch, cid)
            eal.save_loaded_asset(ch)
        if "rig" in steps:
            r = unreal.MetaHumanCharacterAutoRiggingRequestParams()
            r.blocking, r.report_progress = True, False
            r.rig_type = unreal.MetaHumanRigType.JOINTS_AND_BLEND_SHAPES
            mhs.request_auto_rigging(ch, r)
            eal.save_loaded_asset(ch)
            log("auto-rigged, saved")
        if "textures" in steps:
            t = unreal.MetaHumanCharacterTextureRequestParams()
            t.blocking, t.report_progress = True, False
            mhs.request_texture_sources(ch, t)
            eal.save_loaded_asset(ch)
            log("textures: high res %s, saved" % ch.has_high_resolution_textures)
        if "hair" in steps:
            set_look(ch, cid)
            eal.save_loaded_asset(ch)
        if "build" in steps:
            log("can build: %s" % mhs.can_build_meta_human(character=ch))
            if mhs.can_build_meta_human(character=ch):
                b = unreal.MetaHumanCharacterEditorBuildParameters()
                b.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
                b.pipeline_quality = unreal.MetaHumanQualityLevel.MEDIUM
                b.absolute_build_path = dest + "/Build"
                b.common_folder_path = "/Game/MetaHumans/Common"
                try:
                    mhs.build_meta_human(character=ch, params=b)
                except RuntimeError as e:                   # the Common control rigs log link warnings as errors
                    log("build reported: %s" % str(e)[:300])
                unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
                log("built under %s/Build, saved" % dest)
    finally:
        eal.save_loaded_asset(ch)
        if mhs.is_object_added_for_editing(ch):
            mhs.remove_object_to_edit(ch)


try:
    main()
finally:
    if os.environ.get("MH_QUIT"):                    # run inside a rendering editor (-RenderOffscreen): the build's
        unreal.SystemLibrary.quit_editor()      # texture graph needs the GPU and Slate, a commandlet has neither
