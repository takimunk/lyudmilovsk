"""The diner by the lower station in Unreal (docs/diner.md).

  import: design/diner/game/diner_kit.glb (blender/diner/building.py, tableware.py, pack.py) -> /Game/Diner/Meshes/SM_Diner_<part>
          (the car's shell, its insides and glass, the china, the food, the fork and spoon, the loads) and the burger's bite
          stages /Game/Food/SM_Food_diner_burger_<k>; flat materials MI_DN_<slot> from blender/diner/diner_common.py MAT_SPEC,
          clear glass, the signs' pictures (textures/localized/<lang>/diner_sign_<name>.png, tools/make_diner_signs.py) with a
          glow the game turns up at night.
  place:  place(face_y) puts the car on the map (tools/world_map.py diner(), its floor at layout.json floor_z): the meshes
          tagged "Diner" (the shell and insides also "Roof": indoors for the sky), ALyudDiner (lamps, opening hours, serving,
          the coffee) and the three ALyudDoor (street, kitchen, back; the outer two tagged "DinerOuter": locked at night).

Headless:
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script="unreal/scripts/import_diner.py import"
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script="unreal/scripts/import_diner.py place"
import_world.py calls place() too, so a full build_scene01.sh keeps the diner.
"""
import json
import math
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "tools"))
import world_map as W  # noqa: E402

KIT = os.path.join(REPO, "design", "diner", "game", "diner_kit.glb")
LAYOUT = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "Diner", "layout.json")
DEST = "/Game/Diner"
MESHES = DEST + "/Meshes"
MATS = DEST + "/Materials"
TAG = "Diner"
NEAR_CM = 6000.0
SMALL_CM = 2500.0          # the small things (the counter's dispensers, lamps' fittings, the kitchen's pots): drawn to 25 m
TEX_DIR = os.path.join(REPO, "textures", "diner")
SIGNS = ("name", "band", "roof", "menu", "coffee")

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty


def log(msg):
    unreal.log_warning("DINER " + msg)


def _literal(name):
    """A dict literal of blender/diner/diner_common.py (MAT_SPEC, MAT_TEX) without bpy: read from the file."""
    import ast
    src = open(os.path.join(REPO, "blender", "diner", "diner_common.py")).read()
    tree = ast.parse(src)
    for n in tree.body:
        if isinstance(n, ast.Assign) and getattr(n.targets[0], "id", "") == name:
            return ast.literal_eval(n.value)
    raise RuntimeError("no " + name)


def _spec():
    return _literal("MAT_SPEC")


def _lin(hx):
    hx = hx.lstrip("#")
    c = [int(hx[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c]


def build_sign_master(path):
    """A sign's picture (Image) lit by the scene, and glowing (Glow x the picture) when ALyudDiner turns it up at night."""
    import academ_materials as A
    m = A.master(path)
    t = A.node(m, unreal.MaterialExpressionTextureSampleParameter2D, -600, -100, parameter_name="Image")
    mel.connect_material_property(t, "RGB", MP.MP_BASE_COLOR)
    g = A.node(m, unreal.MaterialExpressionMultiply, -300, 50)
    A.link(t, "RGB", g, "A")
    A.link(A.scalar(m, "Glow", -600, 150, 0.0), "", g, "B")
    mel.connect_material_property(g, "", MP.MP_EMISSIVE_COLOR)
    mel.connect_material_property(A.scalar(m, "Roughness", -400, 250, 0.35), "", MP.MP_ROUGHNESS)
    A.finish(m)
    return m


def build_tex_master(path, d, n, r):
    """The textured master: BaseColor x Tint, Normal (NormalStrength from flat), Mask R roughness x RoughnessScale, Mask G ambient
    occlusion (AOStrength), Metallic, all on UV 0 (metres) x Tiling."""
    import academ_materials as A
    m = A.master(path)
    uv0 = A.node(m, unreal.MaterialExpressionTextureCoordinate, -1900, 0, coordinate_index=0)
    uv = A.node(m, unreal.MaterialExpressionMultiply, -1700, 40)
    A.link(uv0, "", uv, "A")
    A.link(A.scalar(m, "Tiling", -1900, 120, 1.0), "", uv, "B")
    td = A.node(m, unreal.MaterialExpressionTextureSampleParameter2D, -1450, -350, parameter_name="BaseColor", texture=d,
                sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    tn = A.node(m, unreal.MaterialExpressionTextureSampleParameter2D, -1450, 350, parameter_name="Normal", texture=n,
                sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    tr = A.node(m, unreal.MaterialExpressionTextureSampleParameter2D, -1450, 0, parameter_name="Mask", texture=r,
                sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    for t in (td, tn, tr):
        A.link(uv, "", t, "UVs")
    col = A.node(m, unreal.MaterialExpressionMultiply, -1100, -400)
    A.link(td, "RGB", col, "A")
    A.link(A.vec(m, "Tint", -1450, -550, (1, 1, 1)), "", col, "B")
    mel.connect_material_property(col, "", MP.MP_BASE_COLOR)
    rough = A.node(m, unreal.MaterialExpressionMultiply, -1100, 0)
    A.link(tr, "R", rough, "A")
    A.link(A.scalar(m, "RoughnessScale", -1300, 120, 1.0), "", rough, "B")
    rsat = A.node(m, unreal.MaterialExpressionSaturate, -950, 0)
    A.link(rough, "", rsat, "")
    mel.connect_material_property(rsat, "", MP.MP_ROUGHNESS)
    one = A.node(m, unreal.MaterialExpressionConstant, -1100, 180, r=1.0)
    ao = A.node(m, unreal.MaterialExpressionLinearInterpolate, -950, 200)
    A.link(one, "", ao, "A")
    A.link(tr, "G", ao, "B")
    A.link(A.scalar(m, "AOStrength", -1100, 280, 1.0), "", ao, "Alpha")
    mel.connect_material_property(ao, "", MP.MP_AMBIENT_OCCLUSION)
    flatn = A.node(m, unreal.MaterialExpressionConstant3Vector, -1100, 420, constant=unreal.LinearColor(0, 0, 1, 1))
    nl = A.node(m, unreal.MaterialExpressionLinearInterpolate, -950, 420)
    A.link(flatn, "", nl, "A")
    A.link(tn, "RGB", nl, "B")
    A.link(A.scalar(m, "NormalStrength", -1100, 520, 1.0), "", nl, "Alpha")
    mel.connect_material_property(nl, "", MP.MP_NORMAL)
    mel.connect_material_property(A.scalar(m, "Metallic", -700, 100, 0.0), "", MP.MP_METALLIC)
    mel.connect_material_property(A.scalar(m, "Specular", -700, 200, 0.5), "", MP.MP_SPECULAR)
    A.finish(m)
    return m


def import_textures(sets):
    """textures/diner/<set>_D.jpg, _N.png, _R.jpg -> /Game/Diner/Textures/T_DN_<set>_{D,N,R}; {(set, suffix): Texture2D}."""
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    files = []
    for name in sets:
        for suf in ("D", "N", "R"):
            f = next((os.path.join(TEX_DIR, "%s_%s.%s" % (name, suf, e)) for e in ("jpg", "png")
                      if os.path.exists(os.path.join(TEX_DIR, "%s_%s.%s" % (name, suf, e)))), None)
            if f is None:
                raise RuntimeError("missing texture %s_%s (run tools/diner_textures.py)" % (name, suf))
            files.append((name, suf, f, "T_DN_%s_%s" % (name, suf)))
    out = {}
    for name, suf, f, asset in files:
        path = "%s/Textures/%s" % (DEST, asset)
        tex = None
        for _ in range(2):           # the first texture of a fresh Interchange process sometimes goes missing: once more
            t = unreal.AssetImportTask()
            t.filename = f
            t.destination_path = DEST + "/Textures"
            t.destination_name = asset
            t.automated = True
            t.replace_existing = True
            t.save = False
            tools.import_asset_tasks([t])
            tex = eal.load_asset(path) if eal.does_asset_exist(path) else None
            if tex is not None:
                break
        if tex is None:
            raise RuntimeError("import failed: " + f)
        if suf == "D":
            tex.set_editor_property("srgb", True)
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
        elif suf == "N":
            tex.set_editor_property("srgb", False)
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("flip_green_channel", False)       # written DirectX (green down) by diner_textures.py
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
        else:
            tex.set_editor_property("srgb", False)
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_SPECULAR)
        eal.save_loaded_asset(tex, False)
        out[(name, suf)] = tex
    return out


def make_materials():
    import academ_materials as A
    flat = A.build_flat(MATS + "/M_DN_Flat")
    textured = _literal("MAT_TEX")
    tex = import_textures(sorted({v[0] for v in textured.values()}))
    first = sorted(tex)[0][0]
    m_tex = build_tex_master(MATS + "/M_DN_Tex", tex[(first, "D")], tex[(first, "N")], tex[(first, "R")])
    clear = A.build_glass_clear(MATS + "/M_DN_GlassClear")
    sign = build_sign_master(MATS + "/M_DN_Sign")
    out = {}
    for slot, (hx, rough, metal, emissive) in _spec().items():
        if slot == "DN_Glass":
            mi = A.instance("%s/MI_%s" % (MATS, slot), clear)
            A._set(mi, "v", "Color", unreal.LinearColor(0.05, 0.07, 0.07, 1))
            A._set(mi, "s", "Opacity", 0.12)
            A._set(mi, "s", "OpacityGrazing", 0.6)
        elif slot in textured:
            set_, tile_m, nstrength = textured[slot]
            mi = A.instance("%s/MI_%s" % (MATS, slot), m_tex)
            A._set(mi, "t", "BaseColor", tex[(set_, "D")])
            A._set(mi, "t", "Normal", tex[(set_, "N")])
            A._set(mi, "t", "Mask", tex[(set_, "R")])
            A._set(mi, "s", "Tiling", 1.0 / tile_m)
            A._set(mi, "s", "NormalStrength", nstrength)
            A._set(mi, "s", "Metallic", metal)
        else:
            mi = A.instance("%s/MI_%s" % (MATS, slot), flat)
            c = _lin(hx)
            A._set(mi, "v", "Color", unreal.LinearColor(c[0], c[1], c[2], 1))
            A._set(mi, "s", "Roughness", rough)
            A._set(mi, "s", "Metallic", metal)
            A._set(mi, "s", "Emissive", emissive)
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi, False)
        out[slot] = mi
    # the signs: the Russian picture by default; ALyudDiner sets the player's language's (/Game/Diner/Localized/<lang>)
    loc = os.path.join(REPO, "textures", "localized")
    for lang in sorted(os.listdir(loc)):
        for name in SIGNS:
            f = os.path.join(loc, lang, "diner_sign_%s.png" % name)
            if os.path.exists(f):
                tex = A.import_image(f, DEST + "/Localized/" + lang, "T_Sign_" + name, sub="")
                if lang == "ru":
                    mi = A.instance("%s/MI_DN_Sign_%s" % (MATS, name), sign)
                    A._set(mi, "t", "Image", tex)
                    mel.update_material_instance(mi)
                    eal.save_loaded_asset(mi, False)
                    out["DN_Sign_" + name] = mi
    return out


def import_meshes():
    for d in (MESHES,):
        if eal.does_directory_exist(d):
            eal.delete_directory(d)
    mats = make_materials()
    tmp = DEST + "/_import"
    src = unreal.InterchangeManager.create_source_data(KIT)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_asset(tmp, src, params)
    mgr.wait_until_all_tasks_done(True)
    count = 0
    for path in eal.list_assets(tmp, True):
        sm = eal.load_asset(path)
        if not isinstance(sm, unreal.StaticMesh):
            continue
        name = sm.get_name()
        if not name.startswith("SM_"):
            name = "SM_" + name
        glass = name.endswith("_Glass")
        building = name in BUILDING
        for i, sl in enumerate(sm.get_editor_property("static_materials")):
            key = str(sl.material_slot_name)
            mi = mats.get(key) or next((v for k, v in sorted(mats.items(), key=lambda kv: -len(kv[0])) if key.startswith(k)), None)
            if mi is not None:
                sm.set_material(i, mi)
            else:
                log("no material for slot %s on %s" % (key, name))
        body = sm.get_editor_property("body_setup")
        if body:
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        settings = sm.get_editor_property("nanite_settings")
        # no Nanite (glTF imports here are classic meshes; the house rule): LODs for the car's furniture and small things
        settings.enabled = False
        sm.set_editor_property("nanite_settings", settings)
        if building and not glass and name != "SM_Diner_Shell":
            _lods(sm, name)
        target = ("/Game/Food/" if name.startswith("SM_Food_") else MESHES + "/") + name
        if eal.does_asset_exist(target):
            eal.delete_asset(target)
        eal.save_loaded_asset(sm)
        eal.rename_asset(sm.get_path_name(), target)
        count += 1
    eal.delete_directory(tmp)
    eal.save_directory(DEST)
    eal.save_directory("/Game/Food")
    log("imported %d meshes" % count)


BUILDING = ("SM_Diner_Shell", "SM_Diner_Inside", "SM_Diner_Glass", "SM_Diner_Inside_Glass", "SM_Diner_Small", "SM_Diner_Small_Glass",
            "SM_Diner_Stools")
# LOD0 full; the car's furniture is seen from across the street (Inside), the small things only from the room (Small, Stools)
LODS = {"SM_Diner_Inside": ((1.0, 1.0), (0.5, 0.45), (0.2, 0.15)),
        "SM_Diner_Small": ((1.0, 1.0), (0.4, 0.5), (0.12, 0.2)),
        "SM_Diner_Stools": ((1.0, 1.0), (0.4, 0.5), (0.15, 0.2))}


def _lods(sm, name):
    steps = LODS.get(name)
    if not steps:
        return
    opts = unreal.EditorScriptingMeshReductionOptions()
    opts.set_editor_property("auto_compute_lod_screen_size", False)
    opts.set_editor_property("reduction_settings", [unreal.EditorScriptingMeshReductionSettings(percent_triangles=p, screen_size=z)
                                                    for p, z in steps])
    sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    n = sms.set_lods(sm, opts)
    # collision stays on the full mesh (the benches' and the counter's every triangle)
    sm.set_editor_property("lod_for_collision", 0)
    log("%s: %d LODs, triangles %s" % (name, n, [sm.get_num_triangles(i) for i in range(sm.get_num_lods())]))


def _ue_yaw():
    """Unreal yaw of the car: its frame is the ensemble's (world_map.diner()), glTF x -> Unreal X, y -> -Y."""
    return math.degrees(W.diner()[2]) - 90.0


def place(face_y, report=False):
    """Spawns the diner in the open level (face_y: the portal face, Unreal Y cm)."""
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in eas.get_all_level_actors():
        if TAG in [str(t) for t in a.tags]:
            eas.destroy_actor(a)
    with open(LAYOUT) as f:
        layout = json.load(f)
    z0 = layout["floor_z"]
    yaw = _ue_yaw()

    def to_ue(lx, ly, lz):
        x, y = W.diner_to_map(lx, ly)
        return W.to_unreal(x, y, z0 + lz, face_y)
    origin = unreal.Vector(*to_ue(0.0, 0.0, 0.0))
    n = 0
    for name in layout["parts"]:
        sm = unreal.load_asset(MESHES + "/" + name)
        if sm is None:
            log("missing mesh %s" % name)
            continue
        a = eas.spawn_actor_from_class(unreal.StaticMeshActor, origin, unreal.Rotator(0.0, 0.0, yaw))
        a.static_mesh_component.set_static_mesh(sm)
        a.set_actor_label("Diner_" + name[9:])
        glass = name.endswith("_Glass")
        a.tags = [TAG, "NoGrass", "NoTree"] + ([] if glass else ["Roof"])
        a.set_folder_path("Outside/Diner")
        c = a.static_mesh_component
        c.set_editor_property("lighting_channels", unreal.LightingChannels(True, True, False))
        if glass:
            c.set_editor_property("cast_shadow", False)
        if name in ("SM_Diner_Inside", "SM_Diner_Inside_Glass"):
            c.set_editor_property("ld_max_draw_distance", NEAR_CM)
        if name == "SM_Diner_Stools":
            # the hero stands at the counter where a stool is (people keep off them: people_map.build_diner blocks them)
            c.set_collision_profile_name("NoCollision")
            c.set_editor_property("ld_max_draw_distance", NEAR_CM)
        if name in ("SM_Diner_Small", "SM_Diner_Small_Glass"):
            # the things on tops, the lamps' fittings, the pots: nothing to walk into, nothing seen from the street
            c.set_collision_profile_name("NoCollision")
            c.set_editor_property("ld_max_draw_distance", SMALL_CM)
            c.set_editor_property("affect_distance_field_lighting", False)
        n += 1
    cls = unreal.load_class(None, "/Script/Lyudmilovsk.LyudDiner")
    d = eas.spawn_actor_from_class(cls, origin, unreal.Rotator(0.0, 0.0, yaw))
    d.set_actor_label("Diner")
    d.tags = [TAG]
    d.set_folder_path("Outside/Diner")
    import place_doors
    for outside in (True, False):
        place_doors.spawn_doors([e for e in layout["doors"] if bool(e.get("outside")) == outside], to_ue,
                                tags=[TAG] + (["DinerOuter"] if outside else []), folder="Outside/Diner/Doors",
                                label_prefix="Diner_Door_", leaf_material=unreal.load_asset(MATS + "/MI_DN_EnamelRed" if outside else MATS + "/MI_DN_Mahogany"),
                                frame_material=unreal.load_asset(MATS + "/MI_DN_Steel"), log=log)
    log("placed %d parts at map %s, floor %.2f, yaw %.1f" % (n, [round(v, 1) for v in W.diner()[:2]], z0, yaw))


if __name__ == "__main__":
    if "import" in sys.argv[1:]:
        import_meshes()
    if "place" in sys.argv[1:]:
        with open(os.path.join(REPO, "blender", "export", "scene01_layout.json")) as f:
            face_y = json.load(f).get("outside", {}).get("portal_face_y", 790.0)
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if not les.load_level("/Game/Scene01/Lvl_Scene01"):
            raise RuntimeError("no /Game/Scene01/Lvl_Scene01")
        place(face_y)
        les.save_current_level()
        log("done")
