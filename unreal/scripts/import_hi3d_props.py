"""The hi3d town props into the project (docs/city/hi3d_props.md): blender/props_hi3d/game/<id>.glb and
T_Hi3d_<id>_{C,N,R}.png (blender/props_hi3d/optimize.py) -> /Game/City/Hi3D/SM_Hi3d_<id> with MI_Hi3d_<id> of
M_Hi3dProp (base colour, normal, roughness textures; scalar Metallic from props.json). Headless, one step per process
(Interchange imports one scene per process; unreal/scripts/import_hi3d_props.sh runs them in turn):

  HI3D_STEP=materials UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/import_hi3d_props.py
  HI3D_STEP=mesh HI3D_ID=<id> UnrealEditor-Cmd ...

Nanite off; LOD1/LOD2 at 50 % / 25 %; a 26-sided simple collision for the ones props.json marks "collide", none for
the rest (chandeliers).
"""
import json
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
DEST = "/Game/City/Hi3D"
TEX = DEST + "/Textures"
GAME = os.path.join(REPO, "blender", "props_hi3d", "game")
SPEC = json.load(open(os.path.join(REPO, "blender", "props_hi3d", "props.json")))["props"]
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
NAMES = {"C": "BaseColor", "N": "Normal", "R": "Roughness"}


def log(msg):
    unreal.log_warning("HI3DPROPS " + msg)


def get_or_create(name, folder, cls, factory):
    path = "%s/%s" % (folder, name)
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    return tools.create_asset(name, folder, cls, factory)


def ready():
    return [p for p in sorted(SPEC) if os.path.isfile(os.path.join(GAME, p + ".glb"))]


def import_textures(pid):
    for k in ("C", "N", "R"):
        src = os.path.join(GAME, "T_Hi3d_%s_%s.png" % (pid, k))
        if not os.path.isfile(src):
            log("no %s" % src)
            continue
        t = unreal.AssetImportTask()
        t.filename = src
        t.destination_path = TEX
        t.destination_name = "T_Hi3d_%s_%s" % (pid, k)
        t.automated = True
        t.replace_existing = True
        t.save = False
        tools.import_asset_tasks([t])
        a = eal.load_asset("%s/T_Hi3d_%s_%s" % (TEX, pid, k))
        if not a:
            continue
        if k == "N":
            a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            a.set_editor_property("srgb", False)
            a.set_editor_property("flip_green_channel", True)     # OpenGL normals from Blender
        elif k == "R":
            a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
            a.set_editor_property("srgb", False)
        eal.save_loaded_asset(a)


def node(m, cls, x, y):
    return mel.create_material_expression(m, cls, x, y)


def master():
    """M_Hi3dProp: opaque; base colour, normal, roughness maps; Metallic scalar."""
    m = get_or_create("M_Hi3dProp", DEST, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(m)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    samplers = {"C": unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, "N": unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
                "R": unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE}
    p = {}
    for i, k in enumerate(("C", "N", "R")):
        t = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, -200 + 300 * i)
        t.set_editor_property("parameter_name", NAMES[k])
        t.set_editor_property("sampler_type", samplers[k])
        p[k] = t
    met = node(m, unreal.MaterialExpressionScalarParameter, -400, 500)
    met.set_editor_property("parameter_name", "Metallic")
    met.set_editor_property("default_value", 0.0)
    mel.connect_material_property(p["C"], "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(p["N"], "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(p["R"], "R", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(met, "", unreal.MaterialProperty.MP_METALLIC)
    # the defaults must be set before the material compiles its samplers (a sampler without a texture fails)
    first = ready()[0] if ready() else None
    for k in ("C", "N", "R"):
        tex = eal.load_asset("%s/T_Hi3d_%s_%s" % (TEX, first, k)) if first else None
        if tex:
            p[k].set_editor_property("texture", tex)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def materials():
    ids = ready()
    for pid in ids:
        import_textures(pid)
    m = master()
    for pid in ids:
        mi = get_or_create("MI_Hi3d_%s" % pid, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, m)
        for k in ("C", "N", "R"):
            tex = eal.load_asset("%s/T_Hi3d_%s_%s" % (TEX, pid, k))
            if tex:
                mel.set_material_instance_texture_parameter_value(mi, NAMES[k], tex)
        mel.set_material_instance_scalar_parameter_value(mi, "Metallic", float(SPEC[pid].get("metallic", 0.0)))
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
    log("materials: M_Hi3dProp + %d instances (%s)" % (len(ids), ", ".join(ids)))


def mesh(pid):
    glb = os.path.join(GAME, pid + ".glb")
    if not os.path.isfile(glb):
        log("no %s" % glb)
        return
    tmp = "%s/_import_%s" % (DEST, pid)
    if eal.does_directory_exist(tmp):
        eal.delete_directory(tmp)
    src = unreal.InterchangeManager.create_source_data(glb)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_scene(tmp, src, params)
    mgr.wait_until_all_tasks_done(True)
    target = "%s/SM_Hi3d_%s" % (DEST, pid)
    done = False
    for path in eal.list_assets(tmp, True):
        a = eal.load_asset(path)
        if not isinstance(a, unreal.StaticMesh) or done:
            continue
        if eal.does_asset_exist(target):
            eal.delete_asset(target)
        if not eal.rename_asset(path, target):
            log("could not move %s" % path)
            continue
        a = eal.load_asset(target)
        s = a.get_editor_property("nanite_settings")
        s.enabled = False
        a.set_editor_property("nanite_settings", s)
        mi = eal.load_asset("%s/MI_Hi3d_%s" % (DEST, pid))
        if mi:
            for slot in range(len(a.get_editor_property("static_materials"))):
                a.set_material(slot, mi)
        sme = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        opts = unreal.StaticMeshReductionOptions()
        opts.set_editor_property("auto_compute_lod_screen_size", False)
        lods = []
        for pct, screen in ((1.0, 1.0), (0.5, 0.35), (0.25, 0.12)):
            r = unreal.StaticMeshReductionSettings()
            r.set_editor_property("percent_triangles", pct)
            r.set_editor_property("screen_size", screen)
            lods.append(r)
        opts.set_editor_property("reduction_settings", lods)
        sme.set_lods_with_notification(a, opts, False)
        body = a.get_editor_property("body_setup")
        if body:
            body.set_editor_property("agg_geom", unreal.KAggregateGeom())
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
        if SPEC[pid].get("collide"):
            sme.add_simple_collisions_with_notification(a, unreal.ScriptingCollisionShapeType.NDOP26, False)
        eal.save_loaded_asset(a)
        done = True
    eal.delete_directory(tmp)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for act in eas.get_all_level_actors():
        if pid in act.get_name() or pid in str(act.get_folder_path()):
            eas.destroy_actor(act)
    log("%s: %s" % (pid, target if done else "NO MESH"))


STEP = os.environ.get("HI3D_STEP", "materials")
if STEP == "materials":
    materials()
elif STEP == "list":
    print("HI3DPROPS READY " + " ".join(ready()))
else:
    mesh(os.environ["HI3D_ID"])
