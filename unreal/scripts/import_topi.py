"""Topi's props into the project (docs/topi.md): blender/topi/out/SM_Topi_*.glb (blender/topi/props.py) -> /Game/Topi/Props/SM_Topi_<name>,
the signs' pictures textures/localized/<lang>/topi_sign_<name>.png (tools/make_topi_signs.py) -> /Game/Topi/Localized/<lang>/T_Sign_<name>
(ULyudTopiProps shows the player's language: LyudText::LoadTexture("Topi", "T_Sign_<name>")), the cloth and wood textures/topi/*.png ->
/Game/Topi/Props/T_*, and the materials /Game/Topi/Props/M_TP_{Wood,Metal,Rag,Towel,Sign} (towel and rag two-sided; texture parameter Image) with
the instances MI_TP_{Towel_A,Towel_B,Towel_C,SignWarn,SignUral} that the mesh slots get by name (TP_Wood, TP_Metal, TP_Rag, TP_Towel, TP_SignWarn, TP_SignUral).

One step per process (Interchange imports one scene per process); unreal/scripts/import_topi.sh runs them in turn:

  TOPI_STEP=textures UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/import_topi.py      textures and materials first
  TOPI_STEP=SM_Topi_WarnSign UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/import_topi.py   then each mesh

Nanite off. Signs and the stake: their own triangles as collision; towels: none.
"""
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import academ_materials as A  # noqa: E402

DEST = "/Game/Topi"
PROPS = DEST + "/Props"
EXPORT = os.path.join(REPO, "blender", "topi", "out")
NEUTRAL = os.path.join(REPO, "textures", "topi")
LOCALIZED = os.path.join(REPO, "textures", "localized")
MESHES = ("SM_Topi_WarnSign", "SM_Topi_UralSign", "SM_Topi_Towel_A", "SM_Topi_Towel_B", "SM_Topi_Towel_C", "SM_Topi_Stake",
          "SM_Topi_Stake_A", "SM_Topi_Stake_B", "SM_Topi_Stake_C", "SM_Topi_Stake_D")
# baked texture sets from blender/topi/props.py: textures/topi/baked/<mesh>_<slot>_{A,N,ORM} (sign fronts: _{Wear,N,ORM})
BAKED = os.path.join(REPO, "textures", "topi", "baked")
BAKED_DEST = PROPS + "/Baked"
SIGNS = ("warn", "ural")
NO_COLLISION = ("SM_Topi_Towel_A", "SM_Topi_Towel_B", "SM_Topi_Towel_C")
# neutral picture file -> texture asset
TEXTURES = {"wood": "T_Wood", "rag": "T_Rag", "towel_a": "T_Towel_A", "towel_b": "T_Towel_B", "towel_c": "T_Towel_C"}
# mesh slot -> instance; the towels' slot depends on the mesh (below)
SLOT_MI = {"TP_Wood": "MI_TP_Wood", "TP_Metal": "MI_TP_Metal", "TP_Rag": "MI_TP_Rag",
           "TP_SignWarn": "MI_TP_SignWarn", "TP_SignUral": "MI_TP_SignUral"}

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty


def log(msg):
    unreal.log_warning("TOPI " + msg)


def lin(r, g, b):
    return unreal.LinearColor(r, g, b, 1.0)


def build_textured(path, tex, roughness, metallic, two_sided):
    """A material of one picture (Image) with a roughness and a metallic: wood, rag, towel, the signs' plates."""
    m = A.master(path)
    t = A.node(m, unreal.MaterialExpressionTextureSampleParameter2D, -600, -100, parameter_name="Image", texture=tex)
    mel.connect_material_property(t, "RGB", MP.MP_BASE_COLOR)
    mel.connect_material_property(A.scalar(m, "Roughness", -400, 120, roughness), "", MP.MP_ROUGHNESS)
    mel.connect_material_property(A.scalar(m, "Metallic", -400, 220, metallic), "", MP.MP_METALLIC)
    m.set_editor_property("two_sided", two_sided)
    A.finish(m)
    return m


def build_metal(path):
    """Bare steel of the plate's back and the bolts: a colour, rough, a little metallic."""
    m = A.master(path)
    mel.connect_material_property(A.vec(m, "Color", -500, -100, (0.17, 0.15, 0.13)), "", MP.MP_BASE_COLOR)
    mel.connect_material_property(A.scalar(m, "Roughness", -400, 120, 0.6), "", MP.MP_ROUGHNESS)
    mel.connect_material_property(A.scalar(m, "Metallic", -400, 220, 0.55), "", MP.MP_METALLIC)
    A.finish(m)
    return m


def tex_asset(name):
    return eal.load_asset("%s/%s" % (PROPS, name)) if eal.does_asset_exist("%s/%s" % (PROPS, name)) else None


def import_textures():
    n = 0
    for lang in sorted(os.listdir(LOCALIZED)):
        for name in SIGNS:
            f = os.path.join(LOCALIZED, lang, "topi_sign_%s.png" % name)
            if os.path.exists(f):
                A.import_image(f, DEST + "/Localized/" + lang, "T_Sign_" + name, sub="")
                n += 1
    for src, asset in TEXTURES.items():
        f = os.path.join(NEUTRAL, src + ".png")
        if os.path.exists(f):
            A.import_image(f, PROPS, asset, sub="")
            n += 1
        else:
            log("no %s (python3 tools/make_topi_signs.py)" % f)
    log("textures: %d" % n)
    eal.save_directory(DEST)


def import_baked():
    """The baked sets: albedo sRGB (cloth keeps its alpha), normal maps, ORM masks (AO, roughness, metal), the signs' wear (sRGB + coverage)."""
    if not os.path.isdir(BAKED):
        log("no %s (blender/topi/props.py)" % BAKED)
        return 0
    n = 0
    for f in sorted(os.listdir(BAKED)):
        if not f.endswith(".png"):
            continue
        name = "T_" + f[:-4].replace("SM_Topi_", "Topi_")
        tex = A.import_image(os.path.join(BAKED, f), BAKED_DEST, name, sub="")
        kind = f[:-4].rsplit("_", 1)[1]
        if kind == "N":
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("srgb", False)
        elif kind == "ORM":
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            tex.set_editor_property("srgb", False)
        else:
            tex.set_editor_property("srgb", True)
        eal.save_loaded_asset(tex, False)
        n += 1
    return n


def _tex(m, name, x, y, sampler):
    return A.node(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y, parameter_name=name, sampler_type=sampler)


def build_baked(path, cloth):
    """Albedo + Normal + ORM. cloth: two-sided, masked by the albedo's alpha (frayed rags, towel edges)."""
    m = A.master(path)
    S = unreal.MaterialSamplerType
    alb = _tex(m, "Albedo", -700, -200, S.SAMPLERTYPE_COLOR)
    nrm = _tex(m, "Normal", -700, 100, S.SAMPLERTYPE_NORMAL)
    orm = _tex(m, "ORM", -700, 400, S.SAMPLERTYPE_MASKS)
    mel.connect_material_property(alb, "RGB", MP.MP_BASE_COLOR)
    mel.connect_material_property(nrm, "RGB", MP.MP_NORMAL)
    mel.connect_material_property(orm, "R", MP.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(orm, "G", MP.MP_ROUGHNESS)
    mel.connect_material_property(orm, "B", MP.MP_METALLIC)
    if cloth:
        mel.connect_material_property(alb, "A", MP.MP_OPACITY_MASK)
    A.finish(m)
    if cloth:
        m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        m.set_editor_property("two_sided", True)
        mel.recompile_material(m)
        eal.save_loaded_asset(m, False)
    return m


def build_sign_worn(path):
    """A sign's front: the localized picture (Image, ULyudTopiProps sets the player's language) under the wear layer
    (Wear: rgb colour, a coverage), with the wear's normal and ORM."""
    m = A.master(path)
    S = unreal.MaterialSamplerType
    pic = _tex(m, "Image", -900, -300, S.SAMPLERTYPE_COLOR)
    wear = _tex(m, "Wear", -900, -50, S.SAMPLERTYPE_COLOR)
    nrm = _tex(m, "Normal", -700, 200, S.SAMPLERTYPE_NORMAL)
    orm = _tex(m, "ORM", -700, 450, S.SAMPLERTYPE_MASKS)
    lerp = A.node(m, unreal.MaterialExpressionLinearInterpolate, -400, -200)
    A.link(pic, "RGB", lerp, "A")
    A.link(wear, "RGB", lerp, "B")
    A.link(wear, "A", lerp, "Alpha")
    mel.connect_material_property(lerp, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(nrm, "RGB", MP.MP_NORMAL)
    mel.connect_material_property(orm, "R", MP.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(orm, "G", MP.MP_ROUGHNESS)
    mel.connect_material_property(orm, "B", MP.MP_METALLIC)
    A.finish(m)
    return m


def baked_tex(name):
    path = "%s/%s" % (BAKED_DEST, name)
    return eal.load_asset(path) if eal.does_asset_exist(path) else None


def make_baked_instances():
    """One instance per mesh slot that has a baked set: MI_<mesh>_<slot> (mesh without SM_)."""
    baked = build_baked(PROPS + "/M_TP_Baked", False)
    cloth = build_baked(PROPS + "/M_TP_BakedCloth", True)
    worn = build_sign_worn(PROPS + "/M_TP_SignWorn")
    sign_ru = {n: eal.load_asset("%s/Localized/ru/T_Sign_%s" % (DEST, n)) if eal.does_asset_exist("%s/Localized/ru/T_Sign_%s" % (DEST, n)) else None
               for n in SIGNS}
    n = 0
    for mesh in MESHES:
        short = mesh.replace("SM_Topi_", "Topi_")
        for slot in ("TP_Metal", "TP_SignWarn", "TP_Wood", "TP_SignUral", "TP_Rag", "TP_Towel_A", "TP_Towel_B", "TP_Towel_C"):
            nrm = baked_tex("T_%s_%s_N" % (short, slot))
            if nrm is None:
                continue
            front = slot.startswith("TP_Sign")
            parent = worn if front else (cloth if slot.startswith("TP_Towel") or slot == "TP_Rag" else baked)
            mi = A.instance("%s/MI_%s_%s" % (PROPS, short, slot), parent)
            A._set(mi, "t", "Normal", nrm)
            A._set(mi, "t", "ORM", baked_tex("T_%s_%s_ORM" % (short, slot)))
            if front:
                A._set(mi, "t", "Wear", baked_tex("T_%s_%s_Wear" % (short, slot)))
                pic = sign_ru["warn" if slot == "TP_SignWarn" else "ural"]
                if pic is not None:
                    A._set(mi, "t", "Image", pic)
            else:
                A._set(mi, "t", "Albedo", baked_tex("T_%s_%s_A" % (short, slot)))
            mel.update_material_instance(mi)
            eal.save_loaded_asset(mi, False)
            n += 1
    log("baked materials: M_TP_Baked, M_TP_BakedCloth, M_TP_SignWorn and %d instances" % n)
    return n


def make_materials():
    wood = build_textured(PROPS + "/M_TP_Wood", tex_asset("T_Wood"), 0.92, 0.0, False)
    metal = build_metal(PROPS + "/M_TP_Metal")
    rag = build_textured(PROPS + "/M_TP_Rag", tex_asset("T_Rag"), 0.95, 0.0, True)
    towel = build_textured(PROPS + "/M_TP_Towel", tex_asset("T_Towel_A"), 0.95, 0.0, True)
    sign_ru = lambda n: eal.load_asset("%s/Localized/ru/T_Sign_%s" % (DEST, n)) if eal.does_asset_exist("%s/Localized/ru/T_Sign_%s" % (DEST, n)) else None  # noqa: E731
    sign = build_textured(PROPS + "/M_TP_Sign", sign_ru("warn"), 0.55, 0.0, False)
    plan = [("MI_TP_Wood", wood, None), ("MI_TP_Metal", metal, None), ("MI_TP_Rag", rag, tex_asset("T_Rag")),
            ("MI_TP_Towel_A", towel, tex_asset("T_Towel_A")), ("MI_TP_Towel_B", towel, tex_asset("T_Towel_B")),
            ("MI_TP_Towel_C", towel, tex_asset("T_Towel_C")), ("MI_TP_SignWarn", sign, sign_ru("warn")),
            ("MI_TP_SignUral", sign, sign_ru("ural"))]
    for name, parent, tex in plan:
        mi = A.instance("%s/%s" % (PROPS, name), parent)
        if tex is not None:
            A._set(mi, "t", "Image", tex)
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi, False)
    # the wood instance shows wood.png (the master's default); the sign master's default is the Russian plate
    log("materials: M_TP_{Wood,Metal,Rag,Towel,Sign} and %d instances" % len(plan))
    eal.save_directory(PROPS)


def import_mesh(name):
    glb = os.path.join(EXPORT, name + ".glb")
    if not os.path.isfile(glb):
        log("no %s (python3 blender/topi/props.py)" % glb)
        return
    tmp = "%s/_import_%s" % (DEST, name)
    if eal.does_directory_exist(tmp):
        eal.delete_directory(tmp)
    src = unreal.InterchangeManager.create_source_data(glb)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_asset(tmp, src, params)
    mgr.wait_until_all_tasks_done(True)
    found = [p for p in eal.list_assets(tmp, True) if isinstance(eal.load_asset(p), unreal.StaticMesh)]
    if not found:
        log("nothing imported from %s" % glb)
        return
    sm = eal.load_asset(found[0])
    short = name.replace("SM_Topi_", "Topi_")
    for i, sl in enumerate(sm.get_editor_property("static_materials")):
        key = str(sl.material_slot_name)
        baked_mi = "%s/MI_%s_%s" % (PROPS, short, key)
        if eal.does_asset_exist(baked_mi):
            sm.set_material(i, eal.load_asset(baked_mi))
            continue
        slot = next((k for k in list(SLOT_MI) + ["TP_Towel"] if key == k or key.startswith(k)), None)
        mi_name = None
        if slot == "TP_Towel":
            mi_name = "MI_TP_Towel_" + name[-1]
        elif slot:
            mi_name = SLOT_MI[slot]
        mi = eal.load_asset("%s/%s" % (PROPS, mi_name)) if mi_name and eal.does_asset_exist("%s/%s" % (PROPS, mi_name)) else None
        if mi is not None:
            sm.set_material(i, mi)
        else:
            log("no material for slot %s on %s (run the textures step first)" % (key, name))
    settings = sm.get_editor_property("nanite_settings")
    settings.enabled = False
    sm.set_editor_property("nanite_settings", settings)
    body = sm.get_editor_property("body_setup")
    if body and name in NO_COLLISION:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
        body.set_editor_property("agg_geom", unreal.KAggregateGeom())
    elif body:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    target = "%s/%s" % (PROPS, name)
    if eal.does_asset_exist(target):
        eal.delete_asset(target)
    eal.save_loaded_asset(sm)
    eal.rename_asset(sm.get_path_name(), target)
    eal.delete_directory(tmp)
    log("%s imported to %s" % (name, target))


STEP = os.environ.get("TOPI_STEP", "textures")
if STEP == "textures":
    import_textures()
    make_materials()
    log("baked textures: %d" % import_baked())
    eal.save_directory(PROPS)
    make_baked_instances()
    eal.save_directory(PROPS)
elif STEP in MESHES:
    import_mesh(STEP)
else:
    log("unknown TOPI_STEP %s (textures or one of %s)" % (STEP, ", ".join(MESHES)))
