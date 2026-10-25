"""Imports a generated character (blender/characters/<id>/export/human_<id>.glb from rig_character.py) as a human:

  /Game/Characters/Humans/<Id>/SK_<Id>                    on the shared SK_Human_Skeleton
  /Game/Characters/Humans/<Id>/Textures/T_<Id>_*          the model's own textures (base colour, normal, ORM if any)
  /Game/Characters/Humans/<Id>/Materials/MI_<Id>_body     M_HumanSkin instance (the generated texture holds skin and cloth)
  /Game/Characters/Humans/<Id>/SK_<Id>_PhysicsAsset

Run headless, the editor closed on this checkout (docs/characters.md):
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script="unreal/scripts/import_character.py nao" -unattended -nullrhi
The masters M_Human* must exist (they do on main; unreal/scripts/import_humans.py builds them) and carry
used_with_skeletal_mesh + used_with_morph_targets.
"""
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
BASE = "/Game/Characters/Humans"
SKELETON = BASE + "/SK_Human_Skeleton"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log_warning("CHARACTER " + msg)


def pipeline():
    path = BASE + "/Import/IP_HumanAssets"
    if not eal.does_asset_exist(path):
        src = unreal.load_asset("/Interchange/Pipelines/DefaultGLTFAssetsPipeline")
        pipe = tools.duplicate_asset(path.split("/")[-1], path.rsplit("/", 1)[0], src)
        common = pipe.get_editor_property("common_skeletal_meshes_and_animations_properties")
        common.set_editor_property("skeleton", unreal.load_asset(SKELETON))
        eal.save_loaded_asset(pipe)
    return path


def gltf_texture_roles(glb):
    """{image name: BaseColor | Normal | MetalRough} from the .glb's first material."""
    import json
    import struct
    with open(glb, "rb") as f:
        f.read(12)
        n, _ = struct.unpack("<II", f.read(8))
        g = json.loads(f.read(n))
    imgs = [im.get("name", "Image_%d" % i) for i, im in enumerate(g.get("images", []))]
    tex = [imgs[t["source"]] for t in g.get("textures", [])]
    m = g["materials"][0]
    pbr = m.get("pbrMetallicRoughness", {})
    out = {}
    for role, ref in (("BaseColor", pbr.get("baseColorTexture")), ("MetalRough", pbr.get("metallicRoughnessTexture")),
                      ("Normal", m.get("normalTexture"))):
        if ref:
            out[tex[ref["index"]]] = role
    return out


FACE_CURVES = ["eyeBlinkLeft", "eyeBlinkRight", "jawOpen", "mouthFunnel", "mouthSmile", "browUp"]


def face_curves():
    """face_rig.py's morph targets as morph target curves on SK_Human_Skeleton: ULyudBodyAnimInstance drives them."""
    skel = unreal.load_asset(SKELETON)
    AL = unreal.AnimationLibrary
    have = [str(n) for n in AL.get_curve_meta_data_names(skel)] if hasattr(AL, "get_curve_meta_data_names") else []
    for c in FACE_CURVES:
        if c not in have:
            AL.add_curve_meta_data(skel, c)
        AL.set_curve_meta_data_morph_target(skel, c, True)
    eal.save_loaded_asset(skel)


def eye_params(mi, cid):
    """EyeL_C/J, EyeR_C/J of M_HumanFace from blender/characters/<id>/face_marks.json (face_rig.py)."""
    import json
    path = os.path.join(ROOT, "blender", "characters", cid, "face_marks.json")
    if not os.path.isfile(path):
        return
    for side, e in zip("LR", json.load(open(path))["eyes"]):
        uv = e.get("uv")
        if not uv:
            continue
        (a, b), (c, d) = uv["j"]
        mel.set_material_instance_vector_parameter_value(
            mi, "Eye%s_C" % side, unreal.LinearColor(uv["centre"][0], uv["centre"][1], e["w"] / 2, e["h"] / 2))
        mel.set_material_instance_vector_parameter_value(mi, "Eye%s_J" % side, unreal.LinearColor(a, b, c, d))
    log("  eyes: opening parameters from face_marks.json")


def import_character(cid):
    glb = os.path.join(ROOT, "blender", "characters", cid, "export", "human_%s.glb" % cid)
    title = cid.capitalize()
    dest = "%s/%s" % (BASE, title)
    tmp = dest + "/_import"
    # replacing a person keeps their own clips (<Id>/Anims, e.g. the seated loops); everything else is made anew at
    # the same paths, so the level's actors and the clips' retarget sources find the new SK_<Id>
    for p in eal.list_assets(dest, True) if eal.does_directory_exist(dest) else []:
        if "/Anims/" not in p:
            eal.delete_asset(p)
    disk = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Characters", "Humans", title)
    if os.path.isdir(disk):                       # delete_asset/delete_directory leave renamed files behind on disk
        import shutil
        for f in os.listdir(disk):
            if f != "Anims":
                q = os.path.join(disk, f)
                shutil.rmtree(q) if os.path.isdir(q) else os.remove(q)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.override_pipelines.append(unreal.SoftObjectPath("/Interchange/Pipelines/DefaultGLTFPipeline.DefaultGLTFPipeline"))
    params.override_pipelines.append(unreal.SoftObjectPath(pipeline()))
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_asset(tmp, unreal.InterchangeManager.create_source_data(glb), params)
    mgr.wait_until_all_tasks_done(True)

    mesh, textures = None, []
    for p in eal.list_assets(tmp, True):
        a = eal.load_asset(p)
        if isinstance(a, unreal.SkeletalMesh):
            mesh = a
        elif isinstance(a, unreal.Texture2D):
            textures.append(a)
    if mesh is None:
        raise RuntimeError("no skeletal mesh in " + glb)
    if mesh.skeleton.get_path_name() != unreal.load_asset(SKELETON).get_path_name():
        raise RuntimeError("%s did not bind to SK_Human_Skeleton (%s)" % (title, mesh.skeleton.get_path_name()))

    roles = gltf_texture_roles(glb)
    tex = {}
    for t in textures:
        role = roles.get(t.get_name(), "Extra%d" % len(tex))
        new = "%s/Textures/T_%s_%s" % (dest, title, role)
        eal.rename_asset(t.get_path_name(), new)
        tex[role] = unreal.load_asset(new)
    log("  textures %s" % sorted(tex))

    mats = mesh.get_editor_property("materials")
    for i in range(len(mats)):
        sm = mats[i]
        mi = tools.create_asset("MI_%s_body%s" % (title, "" if i == 0 else i), dest + "/Materials",
                                unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        face = unreal.load_asset(BASE + "/Materials/M_HumanFace")             # M_HumanSkin + eyes that look about
        mi.set_editor_property("parent", face or unreal.load_asset(BASE + "/Materials/M_HumanSkin"))
        if face and i == 0:
            eye_params(mi, cid)
        if "BaseColor" in tex:
            mel.set_material_instance_texture_parameter_value(mi, "BaseColor", tex["BaseColor"])
        if "Normal" in tex:
            mel.set_material_instance_texture_parameter_value(mi, "Normal", tex["Normal"])
        mel.set_material_instance_scalar_parameter_value(mi, "Roughness", 0.75)     # mostly cloth; skin is a small part
        mel.set_material_instance_scalar_parameter_value(mi, "Specular", 0.35)
        eal.save_loaded_asset(mi)
        sm.material_interface = mi
        mats[i] = sm
    mesh.set_editor_property("materials", mats)
    eal.save_loaded_asset(mesh)
    eal.rename_asset(mesh.get_path_name(), "%s/SK_%s" % (dest, title))
    for p in eal.list_assets(tmp, True):
        a = eal.load_asset(p)
        if isinstance(a, unreal.PhysicsAsset):
            eal.rename_asset(p, "%s/SK_%s_PhysicsAsset" % (dest, title))
    for p in eal.list_assets(tmp, True):
        eal.delete_asset(p)
    eal.delete_directory(tmp)
    mesh = unreal.load_asset("%s/SK_%s" % (dest, title))
    # LOD1/LOD2 at 50 % and 25 % of the triangles (the game-assets budget)
    sme = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    sme.regenerate_lod(mesh, 3, True, False)
    eal.save_loaded_asset(mesh)
    for p in eal.list_assets(dest + "/Anims", True) if eal.does_directory_exist(dest + "/Anims") else []:
        a = eal.load_asset(p)
        if isinstance(a, unreal.AnimSequence):
            a.set_retarget_source_asset(mesh)
            eal.save_loaded_asset(a)
            log("  own clip %s -> SK_%s" % (a.get_name(), title))
    log("  LODs %d" % sme.get_lod_count(mesh))
    log("%s: %d materials, extent %s, skeleton %s" % (title, len(mesh.materials), mesh.get_bounds().box_extent,
                                                     mesh.skeleton.get_path_name()))
    eal.save_directory(dest)


def main():
    for cid in [a for a in sys.argv[1:] if not a.startswith("-")]:
        import_character(cid)
    face_curves()
    log("done")


main()
