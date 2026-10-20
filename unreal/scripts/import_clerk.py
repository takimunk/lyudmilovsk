"""Imports the Station 09 clerk's own assets into /Game/Characters/Clerk:

  A_Clerk_Sit_Excel   8 s Excel-painting loop for /Game/Characters/Humans/Clerk/SK_Clerk (shared
                      human skeleton), from blender/export/clerk_excel_anim.glb
  Desk/clerk_desk/...  his ball mouse (moving with his hand) + the animated Excel plane on the CRT
                      (skeletal mesh clerk_desk, anim clerk_desk_Anim), from blender/export/clerk_desk.glb
  M_ClerkExcel        the picture filling in cell by cell (textures/clerk/excel_*.png)

Run headless (no editor open on this checkout; the humans must be imported already):
  unreal/scripts/import_clerk.sh

Placing (both actors at layout "clerk".chair, yaw = chair_yaw - 90, animation mode single node,
looping, started together): SkeletalMeshActor SK_Clerk playing A_Clerk_Sit_Excel, and
SkeletalMeshActor clerk_desk playing clerk_desk_Anim.
"""
import os
import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
ANIM_GLB = os.path.join(ROOT, "blender", "export", "clerk_excel_anim.glb")
DESK_GLB = os.path.join(ROOT, "blender", "export", "clerk_desk.glb")
HUMANS_PIPELINE = "/Game/Characters/Humans/Import/IP_HumanAssets"
TEX = os.path.join(ROOT, "textures", "clerk")
DEST = "/Game/Characters/Clerk"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

# Must match build_clerk.py: grid of GRID_COLS x GRID_ROWS cells of the 1024x768 Excel texture.
GRID_COLS, GRID_ROWS = 52, 36
GRID_UV0 = (22 / 1024, 129 / 768)
GRID_UVS = (52 * 18 / 1024, 36 * 16 / 768)
CYCLE, PAINT = 150.0, 130.0      # seconds: he paints the whole picture in PAINT, admires it, starts over


def log(msg):
    unreal.log_warning("CLERK " + msg)


def interchange(glb, dest, pipelines=()):
    src = unreal.InterchangeManager.create_source_data(glb)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    for p in pipelines:
        params.override_pipelines.append(unreal.SoftObjectPath(p))
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_asset(dest, src, params)
    mgr.wait_until_all_tasks_done(True)
    return [eal.load_asset(p) for p in eal.list_assets(dest, True)]


def import_anim():
    """Sit_Excel onto the shared human skeleton; everything else the import made is dropped."""
    tmp = DEST + "/_anim"
    got = interchange(ANIM_GLB, tmp, ["/Interchange/Pipelines/DefaultGLTFPipeline.DefaultGLTFPipeline",
                                      HUMANS_PIPELINE + "." + HUMANS_PIPELINE.rsplit("/", 1)[1]])
    anim = next(a for a in got if isinstance(a, unreal.AnimSequence))
    log("anim skeleton %s" % anim.get_editor_property("skeleton").get_path_name())
    eal.rename_asset(anim.get_path_name(), DEST + "/A_Clerk_Sit_Excel")
    anim = unreal.load_asset(DEST + "/A_Clerk_Sit_Excel")
    anim.set_editor_property("enable_root_motion", False)
    eal.save_loaded_asset(anim)
    eal.delete_directory(tmp)
    return anim


def import_desk():
    """Imported in place (renaming the skeleton afterwards breaks the animation's reference)."""
    got = interchange(DESK_GLB, DEST + "/Desk")
    for a in got:
        log("desk %s %s" % (a.get_class().get_name(), a.get_path_name()))
    return next(a for a in got if isinstance(a, unreal.SkeletalMesh))


def import_texture(name, pixel_art):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(TEX, name + ".png")
    task.destination_path = DEST + "/Textures"
    task.destination_name = "T_Clerk_" + name
    task.automated = True
    task.replace_existing = True
    task.factory = unreal.TextureFactory()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    paths = list(task.get_editor_property("imported_object_paths"))
    t = unreal.load_asset(paths[0]) if paths else None
    log("texture %s -> %s" % (name, paths))
    t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    if pixel_art:      # cells are looked up exactly; the window image keeps mips against moire
        t.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        t.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
    eal.save_loaded_asset(t)
    return t


HLSL = """
float3 base = Texture2DSample(Base, BaseSampler, UV).rgb;
float2 c = (UV - G0) / GS;
if (c.x < 0 || c.y < 0 || c.x >= 1 || c.y >= 1) return base;
float2 n = float2(%(cols)d, %(rows)d);
float2 cell = floor(c * n);
float4 art = Texture2DSampleLevel(Art, ArtSampler, (cell + 0.5) / n, 0);
float prog = saturate(fmod(T, %(cycle).1f) / %(paint).1f);
float2 f = frac(c * n);
float edge = 1.0 - 0.15 * saturate((max(f.x - 0.9, f.y - 0.9)) * 10.0);
if (art.a < prog) return art.rgb * edge;
if (art.a < prog + 0.012) return lerp(base, float3(0.32, 0.42, 0.85), 0.5);
return base;
""" % {"cols": GRID_COLS, "rows": GRID_ROWS, "cycle": CYCLE, "paint": PAINT}


def excel_material(base, art):
    path = DEST + "/M_ClerkExcel"
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_ClerkExcel", DEST, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("used_with_skeletal_mesh", True)
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    t = mel.create_material_expression(m, unreal.MaterialExpressionTime, -900, 120)
    tb = mel.create_material_expression(m, unreal.MaterialExpressionTextureObject, -900, 240)
    tb.set_editor_property("texture", base)
    ta = mel.create_material_expression(m, unreal.MaterialExpressionTextureObject, -900, 400)
    ta.set_editor_property("texture", art)
    g0 = mel.create_material_expression(m, unreal.MaterialExpressionConstant2Vector, -900, 560)
    g0.set_editor_property("r", GRID_UV0[0])
    g0.set_editor_property("g", GRID_UV0[1])
    gs = mel.create_material_expression(m, unreal.MaterialExpressionConstant2Vector, -900, 660)
    gs.set_editor_property("r", GRID_UVS[0])
    gs.set_editor_property("g", GRID_UVS[1])
    cu = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -500, 200)
    cu.set_editor_property("code", HLSL)
    cu.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for nm in ("UV", "T", "Base", "Art", "G0", "GS"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", nm)
        ins.append(ci)
    cu.set_editor_property("inputs", ins)
    for src, nm in ((uv, "UV"), (t, "T"), (tb, "Base"), (ta, "Art"), (g0, "G0"), (gs, "GS")):
        mel.connect_material_expressions(src, "", cu, nm)
    boost = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -200, 300)
    boost.set_editor_property("const_b", 3.0)
    mel.connect_material_expressions(cu, "", boost, "A")
    mel.connect_material_property(boost, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(cu, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -200, 450)
    rough.set_editor_property("r", 0.15)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def find(cls):
    for p in eal.list_assets(DEST, True):
        a = eal.load_asset(p)
        if isinstance(a, cls):
            yield a


def main():
    """Two passes, one process each (Interchange only finds a second import's assets reliably in a
    fresh process): "anim", then "desk" (see unreal/scripts/import_clerk.sh)."""
    import sys
    if "anim" in sys.argv[1:]:
        if eal.does_directory_exist(DEST):
            eal.delete_directory(DEST)
        anim = import_anim()
        log("anim %s %.2fs" % (anim.get_name(), anim.get_play_length()))
    if "desk" in sys.argv[1:]:
        for sub in ("/Desk", "/Textures"):
            if eal.does_directory_exist(DEST + sub):
                eal.delete_directory(DEST + sub)
        if eal.does_asset_exist(DEST + "/M_ClerkExcel"):
            eal.delete_asset(DEST + "/M_ClerkExcel")
        desk = import_desk()
        base, art = import_texture("excel_base", False), import_texture("excel_art", True)
        excel = excel_material(base, art)
        mats = desk.get_editor_property("materials")
        for i, sm in enumerate(mats):
            if "ExcelScreen" in str(sm.get_editor_property("material_slot_name")):
                sm.set_editor_property("material_interface", excel)
                mats[i] = sm
        desk.set_editor_property("materials", mats)
        eal.save_loaded_asset(desk)
        anim = next(eal.load_asset(p) for p in eal.list_assets(DEST + "/Desk", True)
                    if isinstance(eal.load_asset(p), unreal.AnimSequence))
        log("desk anim %s %.2fs, skeleton %s" % (anim.get_path_name(), anim.get_play_length(),
                                                 anim.get_editor_property("skeleton").get_path_name()))
    eal.save_directory(DEST)
    log("done")

main()
