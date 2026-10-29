"""Imports the cats and dogs built by blender/animals/build_animal.py (blender/animals/export/<Breed>/<Breed>.glb, coat and
eye textures, <Breed>_rig.json).

  /Game/Animals/Materials/M_AnimalBody, M_AnimalFur, M_AnimalEye     masters (the fur: masked two-sided shells, a strand per
                                                                     texel of the tiling strand map taller than the shell)
  /Game/Animals/<Breed>/SK_<Breed>, SKEL_<Breed>                     skeletal mesh, four LODs (shells near, none far)
  /Game/Animals/<Breed>/T_<Breed>_<coat>, T_<Breed>_Eye_<n>, T_<Breed>_Strands
  /Game/Animals/<Breed>/MI_<Breed>_<coat>_Body|Fur, MI_<Breed>_Eye_<n>
  /Game/Animals/<Breed>/Anims/A_<Breed>_<Clip>
  Content/AnimalsData/animal_bodies.json                             coats, eyes and stride speeds per breed (AAnimalActor)
  /Game/Animals/<Breed>/SM_<Breed>_Collar, SM_<Breed>_Tag            a breed with a collar (blender/animals/collar.py: Maru)

Headless, no editor open on this checkout, one process per step (Interchange can't re-import in one process):
  UnrealEditor-Cmd <uproject> -run=pythonscript -script="<this file> masters"
  UnrealEditor-Cmd <uproject> -run=pythonscript -script="<this file> textures <Breed>"
  UnrealEditor-Cmd <uproject> -run=pythonscript -script="<this file> import <Breed>"
  UnrealEditor-Cmd <uproject> -run=pythonscript -script="<this file> collar <Breed> Collar|Tag"   (when export/<Breed>/<Breed>_Collar.glb exists)
(unreal/scripts/import_animals.sh does them all for a list of breeds)
"""
import json
import os
import sys

try:
    import unreal
except ImportError:          # `python3 import_animals.py bodies` runs outside the engine
    unreal = None

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
EXPORT = os.path.join(ROOT, "blender", "animals", "export")
DATA = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "AnimalsData")
ANIMALS = "/Game/Animals"
MATS = ANIMALS + "/Materials"
if unreal:
    eal = unreal.EditorAssetLibrary
    mel = unreal.MaterialEditingLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
LOD_SCREEN = {"Cat": [1.0, 0.32, 0.12, 0.04], "dog": [1.0, 0.38, 0.15, 0.05]}


def log(msg):
    if unreal:
        unreal.log_warning("ANIMALS " + msg)
    else:
        print("ANIMALS " + msg)


def get_or_create(name, folder, cls, factory):
    path = "%s/%s" % (folder, name)
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    return tools.create_asset(name, folder, cls, factory)


def node(m, cls, x, y):
    return mel.create_material_expression(m, cls, x, y)


def png_rgba(path, w, h, rgba):
    import struct
    import zlib
    raw = b"".join(b"\x00" + bytes(rgba) * w for _ in range(h))

    def chunk(k, d):
        return struct.pack(">I", len(d)) + k + d + struct.pack(">I", zlib.crc32(k + d) & 0xffffffff)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def import_texture(path, dest, name, linear=False):
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
    if linear == "normal":
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        t.set_editor_property("srgb", False)
        t.set_editor_property("flip_green_channel", True)   # Blender (OpenGL) -> Unreal (DirectX)
    elif linear:
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        t.set_editor_property("srgb", False)
    else:
        t.set_editor_property("srgb", True)
    t.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
    eal.save_loaded_asset(t)
    return t


DEFAULTS = {}


def default_textures():
    tmp = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Saved", "animals_defaults")
    os.makedirs(tmp, exist_ok=True)
    for name, rgba, linear in (("Albedo", (128, 110, 90, 255), False), ("Strands", (200, 128, 0, 255), True),
                               ("Normal", (128, 128, 255, 255), "normal")):
        f = os.path.join(tmp, "T_AnimalDefault_%s.png" % name)
        png_rgba(f, 4, 4, rgba)
        DEFAULTS[name] = import_texture(f, MATS, "T_AnimalDefault_%s" % name, linear)


def tex_param(m, name, x, y, sampler):
    t = node(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    t.set_editor_property("parameter_name", name)
    t.set_editor_property("sampler_type", sampler)
    t.set_editor_property("texture", DEFAULTS[name])
    return t


def scalar(m, name, value, x, y):
    s = node(m, unreal.MaterialExpressionScalarParameter, x, y)
    s.set_editor_property("parameter_name", name)
    s.set_editor_property("default_value", value)
    return s


def const(m, v, x, y):
    c = node(m, unreal.MaterialExpressionConstant, x, y)
    c.set_editor_property("r", v)
    return c


FUR_HLSL = """
// a fur shell: this texel shows if its strand reaches the shell's height (TexCoord 1 x, 0 at the skin .. 1 at the tips);
// the strand's random (G) tints it; roots are shaded by the hairs over them, tips catch the light
float h = saturate(H);
float reach = S.r;
// the lower shells are nearly full (the undercoat), the strands thin out toward the tips
float mask = saturate((reach + 0.18 * (1.0 - h) * (1.0 - h) - h) * 14.0 + 0.5);
float3 col = Albedo * lerp(RootDark, 1.06, smoothstep(0.0, 1.0, h)) * lerp(0.86, 1.14, S.g) * Tint;
Mask = mask;
return col;
"""


def make_masters():
    default_textures()
    # the body under the fur: opaque, matte, a little darker than the coat (seen between the hairs)
    m = get_or_create("M_AnimalBody", MATS, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(m)
    m.set_editor_property("used_with_skeletal_mesh", True)
    m.set_editor_property("two_sided", False)
    alb = tex_param(m, "Albedo", -700, 0, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    mul = node(m, unreal.MaterialExpressionMultiply, -400, 0)
    mel.connect_material_expressions(alb, "RGB", mul, "A")
    mel.connect_material_expressions(scalar(m, "SkinDark", 0.72, -700, 250), "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    nrm = tex_param(m, "Normal", -700, 450, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_property(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(const(m, 0.9, -400, 200), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(const(m, 0.25, -400, 300), "", unreal.MaterialProperty.MP_SPECULAR)
    # subsurface, as the people's skin: Virtual Shadow Maps give it a wide penumbra, else the generated heads' coarse
    # facets shadow themselves in hard jagged triangles (Maru's face, 2026-10-28)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SUBSURFACE)
    ssc = node(m, unreal.MaterialExpressionMultiply, -250, 400)
    mel.connect_material_expressions(mul, "", ssc, "A")
    tint_s = node(m, unreal.MaterialExpressionConstant3Vector, -400, 450)
    tint_s.set_editor_property("constant", unreal.LinearColor(0.75, 0.68, 0.64, 1.0))   # near neutral: a skin red turned her bare white lids pink
    mel.connect_material_expressions(tint_s, "", ssc, "B")
    mel.connect_material_property(ssc, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    mel.connect_material_property(const(m, 1.0, -400, 550), "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    # the fur shells
    f = get_or_create("M_AnimalFur", MATS, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(f)
    f.set_editor_property("used_with_skeletal_mesh", True)
    f.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    f.set_editor_property("two_sided", True)
    f.set_editor_property("opacity_mask_clip_value", 0.5)
    f.set_editor_property("dithered_lod_transition", True)
    alb = tex_param(f, "Albedo", -900, -200, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    uv = node(f, unreal.MaterialExpressionTextureCoordinate, -1200, 150)
    uvs = node(f, unreal.MaterialExpressionMultiply, -1050, 150)
    mel.connect_material_expressions(uv, "", uvs, "A")
    mel.connect_material_expressions(scalar(f, "StrandScale", 8.0, -1200, 300), "", uvs, "B")
    st = tex_param(f, "Strands", -900, 150, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    mel.connect_material_expressions(uvs, "", st, "UVs")
    vc = node(f, unreal.MaterialExpressionTextureCoordinate, -900, 450)    # TexCoord 1 .x = the shell's height
    vc.set_editor_property("coordinate_index", 1)
    tint = node(f, unreal.MaterialExpressionVectorParameter, -900, 600)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    cu = node(f, unreal.MaterialExpressionCustom, -500, 0)
    cu.set_editor_property("code", FUR_HLSL)
    cu.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for nm in ("Albedo", "S", "H", "RootDark", "Tint"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", nm)
        ins.append(ci)
    cu.set_editor_property("inputs", ins)
    out = unreal.CustomOutput()
    out.set_editor_property("output_name", "Mask")
    out.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    cu.set_editor_property("additional_outputs", [out])
    mel.connect_material_expressions(alb, "RGB", cu, "Albedo")
    mel.connect_material_expressions(st, "RGB", cu, "S")
    hm = node(f, unreal.MaterialExpressionComponentMask, -700, 450)
    hm.set_editor_property("r", True)
    hm.set_editor_property("g", False)
    hm.set_editor_property("b", False)
    hm.set_editor_property("a", False)
    mel.connect_material_expressions(vc, "", hm, "")
    mel.connect_material_expressions(hm, "", cu, "H")
    mel.connect_material_expressions(scalar(f, "RootDark", 0.74, -900, 750), "", cu, "RootDark")
    mel.connect_material_expressions(tint, "", cu, "Tint")
    mel.connect_material_property(cu, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(cu, "Mask", unreal.MaterialProperty.MP_OPACITY_MASK)
    fn = tex_param(f, "Normal", -900, 900, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_property(fn, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(const(f, 0.82, -500, 300), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(const(f, 0.22, -500, 400), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(f)
    eal.save_loaded_asset(f)
    # the eye: glossy, wet
    e = get_or_create("M_AnimalEye", MATS, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(e)
    e.set_editor_property("used_with_skeletal_mesh", True)
    alb = tex_param(e, "Albedo", -600, 0, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    mel.connect_material_property(alb, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    # wet but not a mirror: a full specular turned brown eyes sky-blue in daylight (Maru's, 2026-10-27)
    mel.connect_material_property(const(e, 0.12, -400, 200), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(const(e, 0.45, -400, 300), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(e)
    eal.save_loaded_asset(e)
    log("masters")


def rig_of(breed):
    return json.load(open(os.path.join(EXPORT, breed, breed + "_rig.json")))


def do_textures(breed):
    src = os.path.join(EXPORT, breed)
    dest = "%s/%s" % (ANIMALS, breed)
    if eal.does_directory_exist(dest):
        eal.delete_directory(dest)
    rig = rig_of(breed)
    for coat in rig["coats"]:
        import_texture(os.path.join(src, "T_%s_%s.png" % (breed, coat)), dest, "T_%s_%s" % (breed, coat))
    for i in range(rig["eyes"]):
        import_texture(os.path.join(src, "T_%s_Eye_%d.png" % (breed, i)), dest, "T_%s_Eye_%d" % (breed, i))
    import_texture(os.path.join(src, "T_%s_Strands.png" % breed), dest, "T_%s_Strands" % breed, linear=True)
    nf = os.path.join(src, "T_%s_Normal.png" % breed)
    if os.path.exists(nf):
        import_texture(nf, dest, "T_%s_Normal" % breed, linear="normal")
    eal.save_directory(dest)
    log("%s textures: %d coats, %d eyes" % (breed, len(rig["coats"]), rig["eyes"]))


def mi(name, folder, parent, textures, scalars=None):
    m = get_or_create(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    m.set_editor_property("parent", parent)
    for k, v in textures.items():
        mel.set_material_instance_texture_parameter_value(m, k, v)
    for k, v in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(m, k, v)
    eal.save_loaded_asset(m)
    return m


def do_import(breed):
    src = os.path.join(EXPORT, breed)
    rig = rig_of(breed)
    dest = "%s/%s" % (ANIMALS, breed)
    tmp = dest + "/_import"
    source = unreal.InterchangeManager.create_source_data(os.path.join(src, breed + ".glb"))
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.override_pipelines.append(unreal.SoftObjectPath("/Interchange/Pipelines/DefaultGLTFPipeline.DefaultGLTFPipeline"))
    params.override_pipelines.append(unreal.SoftObjectPath("/Interchange/Pipelines/DefaultGLTFAssetsPipeline.DefaultGLTFAssetsPipeline"))
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_asset(tmp, source, params)
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
        raise RuntimeError("no skeletal mesh in %s.glb" % breed)
    body_m = unreal.load_asset(MATS + "/M_AnimalBody")
    fur_m = unreal.load_asset(MATS + "/M_AnimalFur")
    eye_m = unreal.load_asset(MATS + "/M_AnimalEye")
    strands = unreal.load_asset("%s/T_%s_Strands" % (dest, breed))
    first = {}
    nrm = unreal.load_asset("%s/T_%s_Normal" % (dest, breed)) if eal.does_asset_exist("%s/T_%s_Normal" % (dest, breed)) else None
    extra = {"Normal": nrm} if nrm else {}
    for coat in rig["coats"]:
        alb = unreal.load_asset("%s/T_%s_%s" % (dest, breed, coat))
        b = mi("MI_%s_%s_Body" % (breed, coat), dest, body_m, dict({"Albedo": alb}, **extra))
        f = mi("MI_%s_%s_Fur" % (breed, coat), dest, fur_m, dict({"Albedo": alb, "Strands": strands}, **extra), {"StrandScale": float(rig["strand_scale"]) * 1.6})
        first.setdefault("Body", b)
        first.setdefault("Fur", f)
    for i in range(rig["eyes"]):
        e = mi("MI_%s_Eye_%d" % (breed, i), dest, eye_m, {"Albedo": unreal.load_asset("%s/T_%s_Eye_%d" % (dest, breed, i))})
        first.setdefault("Eye", e)
    mats = mesh.get_editor_property("materials")
    for i in range(len(mats)):
        sm = mats[i]
        slot = str(sm.material_slot_name)
        kind = "Fur" if "Fur" in slot else ("Eye" if "Eye" in slot else "Body")
        sm.material_interface = first[kind]
        mats[i] = sm
    mesh.set_editor_property("materials", mats)
    moves = [(mesh, "SK_%s" % breed, dest)]
    if skel:
        moves.append((skel, "SKEL_%s" % breed, dest))
    if phys:
        moves.append((phys, "PHYS_%s" % breed, dest))
    clips = sorted(rig["clips"], key=len, reverse=True)
    for a in anims:
        nm = a.get_name()
        clip = next((c for c in clips if nm == c or nm.endswith("_" + c) or nm.endswith(c)), None)
        if clip is None:
            continue
        a.set_editor_property("enable_root_motion", False)
        moves.append((a, "A_%s_%s" % (breed, clip), dest + "/Anims"))
    tools.rename_assets([unreal.AssetRenameData(o, path, name) for o, name, path in moves])
    mesh = unreal.load_asset("%s/SK_%s" % (dest, breed))
    if phys:
        mesh.set_editor_property("physics_asset", unreal.load_asset("%s/PHYS_%s" % (dest, breed)))
    for o, name, path in moves:
        eal.save_asset("%s/%s" % (path, name), only_if_is_dirty=False)
    reds = [eal.load_asset(p) for p in eal.list_assets(tmp, True)]
    reds = [r for r in reds if r and r.get_class().get_name() == "ObjectRedirector"]
    if reds:
        tools.fixup_referencers(reds)
    sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    n = sub.get_lod_count(mesh)
    unreal.BirdSpeciesLibrary.set_lod_screen_sizes(mesh, LOD_SCREEN["Cat" if breed == "Cat" else "dog"][:n])
    eal.save_loaded_asset(mesh)
    for p in eal.list_assets(tmp, True):
        eal.delete_asset(p)
    if eal.does_directory_exist(tmp):
        eal.delete_directory(tmp)
    eal.save_directory(dest)
    b = mesh.get_bounds().box_extent
    log("%s: %d materials, %d LODs, %d clips, bounds %.1f x %.1f x %.1f cm" % (breed, len(mats), n, len(eal.list_assets(dest + "/Anims")), b.x * 2, b.y * 2, b.z * 2))


def collar_materials():
    """The cord (its braid in the vertex colours) and the metal of the ring and the tag (one master, three instances)."""
    c = get_or_create("M_AnimalCollarCord", MATS, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(c)
    # dark red and black braid: the strands' colours ride in the vertex colours (white when the importer drops them,
    # so they only tint a dark maroon base)
    vc = node(c, unreal.MaterialExpressionVertexColor, -700, 0)
    base = node(c, unreal.MaterialExpressionConstant3Vector, -700, 200)
    base.set_editor_property("constant", unreal.LinearColor(0.11, 0.022, 0.025, 1))
    mul = node(c, unreal.MaterialExpressionMultiply, -450, 0)
    mel.connect_material_expressions(vc, "", mul, "A")
    mel.connect_material_expressions(base, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(const(c, 0.85, -400, 200), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(const(c, 0.3, -400, 300), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(c)
    eal.save_loaded_asset(c)
    m = get_or_create("M_AnimalCollarMetal", MATS, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(m)
    col = node(m, unreal.MaterialExpressionVectorParameter, -500, 0)
    col.set_editor_property("parameter_name", "Color")
    col.set_editor_property("default_value", unreal.LinearColor(1.0, 0.72, 0.3, 1))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(const(m, 1.0, -400, 200), "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(scalar(m, "Roughness", 0.2, -500, 300), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    out = {"Cord": c}
    for name, rgb, rough in (("Gold", (1.0, 0.6, 0.16), 0.22), ("Engraved", (0.42, 0.24, 0.06), 0.5), ("Steel", (0.82, 0.82, 0.84), 0.25)):
        i = get_or_create("MI_Collar_" + name, MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        i.set_editor_property("parent", m)
        mel.set_material_instance_vector_parameter_value(i, "Color", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1))
        mel.set_material_instance_scalar_parameter_value(i, "Roughness", rough)
        eal.save_loaded_asset(i)
        out[name] = i
    return out


def do_collar(breed, parts=("Collar", "Tag")):
    """The collar's two static meshes (the cord round the neck, the swinging ring and tag) with their materials.
    Interchange misses a second import in the same process: import_animals.sh runs one part per process."""
    src = os.path.join(EXPORT, breed)
    dest = "%s/%s" % (ANIMALS, breed)
    mats = collar_materials()
    for part in parts:
        tmp = "%s/_import_%s" % (dest, part)
        source = unreal.InterchangeManager.create_source_data(os.path.join(src, "%s_%s.glb" % (breed, part)))
        params = unreal.ImportAssetParameters()
        params.is_automated = True
        params.override_pipelines.append(unreal.SoftObjectPath("/Interchange/Pipelines/DefaultGLTFPipeline.DefaultGLTFPipeline"))
        params.override_pipelines.append(unreal.SoftObjectPath("/Interchange/Pipelines/DefaultGLTFAssetsPipeline.DefaultGLTFAssetsPipeline"))
        mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
        mgr.import_asset(tmp, source, params)
        mgr.wait_until_all_tasks_done(True)
        mesh = next((eal.load_asset(p) for p in eal.list_assets(tmp, True) if isinstance(eal.load_asset(p), unreal.StaticMesh)), None)
        if mesh is None:
            raise RuntimeError("no static mesh in %s_%s.glb" % (breed, part))
        sms = mesh.get_editor_property("static_materials")
        for i in range(len(sms)):
            sm = sms[i]
            slot = str(sm.material_slot_name)
            key = next((k for k in mats if k in slot), "Gold")
            sm.material_interface = mats[key]
            sms[i] = sm
        mesh.set_editor_property("static_materials", sms)
        # a few thousand triangles: no Nanite (its materials would need the Nanite usage)
        ns = mesh.get_editor_property("nanite_settings")
        ns.set_editor_property("enabled", False)
        mesh.set_editor_property("nanite_settings", ns)
        name = "SM_%s_%s" % (breed, part)
        if eal.does_asset_exist("%s/%s" % (dest, name)):
            eal.delete_asset("%s/%s" % (dest, name))
        tools.rename_assets([unreal.AssetRenameData(mesh, dest, name)])
        m = unreal.load_asset("%s/%s" % (dest, name))
        eal.save_asset("%s/%s" % (dest, name), only_if_is_dirty=False)
        for p in eal.list_assets(tmp, True):
            eal.delete_asset(p)
        if eal.does_directory_exist(tmp):
            eal.delete_directory(tmp)
        b = m.get_bounds().box_extent
        log("%s: %d tris, %d materials, %.1f x %.1f x %.1f cm" % (name, m.get_num_triangles(0), len(sms), b.x * 2, b.y * 2, b.z * 2))
    eal.save_directory(dest)


def do_bodies():
    """Content/AnimalsData/animal_bodies.json from every breed's rig facts (what AAnimalActor reads)."""
    # breeds not built in this checkout (blender/animals/export is local) keep their entries
    path = os.path.join(DATA, "animal_bodies.json")
    out = json.load(open(path)) if os.path.exists(path) else {}
    for breed in sorted(os.listdir(EXPORT)):
        p = os.path.join(EXPORT, breed, breed + "_rig.json")
        if not os.path.exists(p):
            continue
        r = json.load(open(p))
        out[breed] = {"kind": r["kind"], "coats": r["coats"], "eyes": r["eyes"], "speeds_cms": r["speeds_cms"],
                      "height_cm": r["height_cm"], "length_cm": r["length_cm"], "clips": sorted(r["clips"])}
        ey = os.path.join(ROOT, "blender", "animals", "gen", breed, "eyes.json")     # runtime eyeballs, set by hand
        if os.path.exists(ey):
            out[breed]["eyeballs"] = {k: v for k, v in json.load(open(ey)).items() if not k.startswith("_")}
        c = os.path.join(EXPORT, breed, breed + "_collar.json")
        if os.path.exists(c):
            # the collar: the bone it rides, the point the tag hangs from, the chest it rests on (the breed's frame, cm)
            out[breed]["collar"] = {k: v for k, v in json.load(open(c)).items() if k != "breed"}
    os.makedirs(DATA, exist_ok=True)
    with open(os.path.join(DATA, "animal_bodies.json"), "w") as f:
        json.dump(out, f, indent=1)
    out = dict(sorted(out.items()))
    log("bodies: " + ", ".join(out))


step = sys.argv[1:] or ["masters"]
if step[0] == "masters":
    make_masters()
elif step[0] == "textures":
    for b in step[1:]:
        do_textures(b)
elif step[0] == "bodies":
    do_bodies()
elif step[0] == "collar":
    do_collar(step[1], step[2:] or ("Collar", "Tag"))
else:
    for b in step[1:]:
        do_import(b)
log("done " + " ".join(step))
