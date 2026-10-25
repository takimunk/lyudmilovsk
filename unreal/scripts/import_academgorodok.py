"""The Academgorodok of Laboratory No. 2 (Moscow, 1945-48) in Unreal.

  import: blender/export/academgorodok/academgorodok.glb (built by
          blender/academgorodok/build_academgorodok.py) -> /Game/Academgorodok/Meshes/SM_<part>,
          one static mesh per house, the gates and the square; Nanite, exact collision, the
          materials of unreal/scripts/academ_materials.py by slot name (AC_*).
  place:  place(face_y) puts them on the map (tools/world_map.py academ()), tagged "Academ"
          (console lyud.Academ 0/1 hides them), with the square's trees.

Headless, one process for the import (Interchange), then the placement into Lvl_Scene01:
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script="unreal/scripts/import_academgorodok.py import"
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script="unreal/scripts/import_academgorodok.py place"
(unreal/scripts/import_academgorodok.sh runs both.) import_world.py calls place() too, so a full
build_scene01.sh keeps the ensemble.
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

EXPORT = os.path.join(REPO, "blender", "export", "academgorodok")
GLB = os.path.join(EXPORT, "academgorodok.glb")
LAYOUT = os.path.join(EXPORT, "layout.json")
DEST = "/Game/Academgorodok"
MESHES = DEST + "/Meshes"
TAG = "Academ"
# trees of the square: the real ones are small-leaved limes, spruces and birches; the plant library
# has no lime yet, Norway maple stands in (same round crown at this age)
SUMMARY_DISTANCE = 6000.0
NEAR_CM = 6000.0       # insides and clear glass are drawn up to this far

eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log_warning("ACADEM " + msg)


SIGNS = ("library", "reading_room", "issue_desk", "catalogue", "silence", "hours", "cloakroom", "periodicals")


def _pictures():
    """{material slot: image}: the paintings (furniture.PAINTINGS) and the signs (their Russian picture;
    AcademToggle.cpp swaps in the player's language from /Game/Academgorodok/Localized/<lang>)."""
    out = {"AC_Sign_" + n: os.path.join(REPO, "textures", "localized", "ru", "academ_sign_%s.png" % n) for n in SIGNS}
    try:
        import furniture
        out.update({k: os.path.join(REPO, v) for k, v in getattr(furniture, "PAINTINGS", {}).items()})
    except Exception as e:  # noqa: BLE001
        log("no paintings: %s" % e)
    return out


def _import_signs():
    import academ_materials
    loc = os.path.join(REPO, "textures", "localized")
    n = 0
    for lang in sorted(os.listdir(loc)):
        for name in SIGNS:
            f = os.path.join(loc, lang, "academ_sign_%s.png" % name)
            if os.path.exists(f):
                academ_materials.import_image(f, DEST + "/Localized/" + lang, "T_Sign_" + name, sub="")
                n += 1
    log("signs: %d pictures" % n)


def _extra_materials():
    """Flat slots beyond common.MATERIALS: (hex, roughness, emissive)."""
    sys.path.insert(0, os.path.join(REPO, "blender", "academgorodok"))
    # rooms nobody enters (behind a few windows) and the lamps' glass stay unlit in daylight
    out = {"AC_Room": ("#8C7A62", 0.9, 0.0), "AC_Curtain": ("#D8CDB4", 0.9, 0.0), "AC_Furniture": ("#3B2C22", 0.8, 0.0)}
    import interiors
    grain = {"AC_IWall": "plaster_white", "AC_IDado": "plaster_white", "AC_ICeil": "plaster_white",
             "AC_IStep": "stone_cast", "AC_ITread": "stone_cast", "AC_ITile": "concrete"}
    for k, (hx, rough, _) in interiors.INTERIOR_MATERIALS.items():
        out[k] = (hx, rough, 0.6 if "Opal" in k else 0.0, grain.get(k))
    try:
        import furniture
        for k, (hx, rough, _) in furniture.FURNITURE_MATERIALS.items():
            if k not in getattr(furniture, "PAINTINGS", {}):          # pictures get their image (_pictures)
                out[k] = (hx, rough, 0.6 if "Opal" in k else 0.0)
    except Exception as e:  # noqa: BLE001
        log("no furniture materials: %s" % e)
    try:
        import gastronom
        for k, (hx, rough, _) in gastronom.GASTRONOM_MATERIALS.items():
            out[k] = (hx, rough, 6.0 if "Opal" in k else 0.0)
    except Exception as e:  # noqa: BLE001
        log("no gastronom materials: %s" % e)
    for mod, table in (("houseplants", "PLANT_MATERIALS"), ("library", "LIBRARY_MATERIALS")):
        try:
            mm = __import__(mod)
            for k, (hx, rough, _) in getattr(mm, table).items():
                if k != "AC_PLeafCards":                       # masked cards: academ_materials (foliage)
                    out[k] = (hx, rough, 0.6 if "Opal" in k else 0.0)
        except Exception as e:  # noqa: BLE001
            log("no %s materials: %s" % (mod, e))
    return out


def import_meshes():
    if eal.does_directory_exist(MESHES):
        eal.delete_directory(MESHES)
    tmp = DEST + "/_import"
    src = unreal.InterchangeManager.create_source_data(GLB)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_asset(tmp, src, params)
    mgr.wait_until_all_tasks_done(True)
    try:
        import academ_materials
        mats = academ_materials.make_materials(DEST + "/Materials", os.path.join(REPO, "textures", "academ"),
                                               extra=_extra_materials(), pictures=_pictures())
    except Exception as e:  # noqa: BLE001
        log("materials from the glTF (academ_materials failed: %s)" % e)
        mats = {}
    count = 0
    for path in eal.list_assets(tmp, True):
        sm = eal.load_asset(path)
        if not isinstance(sm, unreal.StaticMesh):
            continue
        name = sm.get_name()
        glass = name.endswith("_Glass") or name.endswith("_InsideGlass")      # translucent, near only
        far_glass = name.endswith("_GlassFar")                               # opaque dark panes, far only
        for i, sl in enumerate(sm.get_editor_property("static_materials")):
            if glass:
                sm.set_material(i, mats["LACE"] if str(sl.material_slot_name).startswith("AC_FLace") else mats["GLASS_CLEAR"])
                continue
            if far_glass:
                sm.set_material(i, mats["AC_Glass"])
                continue
            key = str(sl.material_slot_name)
            mi = mats.get(key)
            if mi is None:
                # Interchange names the slot after the glTF material, sometimes with a suffix
                mi = next((v for k, v in mats.items() if key.startswith(k)), None)
            if mi is not None:
                sm.set_material(i, mi)
            else:
                log("no material for slot %s on %s" % (key, name))
        body = sm.get_editor_property("body_setup")
        if body:
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        settings = sm.get_editor_property("nanite_settings")
        settings.enabled = not glass                    # translucent glass stays a classic mesh
        # the fallback mesh is what complex collision uses: keep it full resolution, or steps turn
        # into ramps and doorways close up (the walking test, test_academ_walk.py, failed on them)
        settings.fallback_target = unreal.NaniteFallbackTarget.PERCENT_TRIANGLES
        settings.fallback_percent_triangles = 1.0
        sm.set_editor_property("nanite_settings", settings)
        target = MESHES + "/SM_" + name.replace("SM_", "")
        eal.save_loaded_asset(sm)
        eal.rename_asset(sm.get_path_name(), target)
        count += 1
    if not mats:
        raise RuntimeError("no materials: the meshes would keep the glTF placeholders")
    _import_signs()
    eal.delete_directory(tmp)                     # the glTF placeholder materials
    eal.save_directory(DEST)
    log("imported %d meshes" % count)


def _ue_yaw():
    """Unreal yaw (degrees) of the ensemble: Blender x (east) is Unreal +X in the mesh, y (north) -Y;
    world_map.academ() turns east onto the map's (sin a, -cos a)."""
    return math.degrees(W.academ()[2]) - 90.0


def place(face_y, report=False):
    """Spawns the ensemble in the open level (face_y: the portal face, Unreal Y cm)."""
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in eas.get_all_level_actors():
        if TAG in [str(t) for t in a.tags]:
            eas.destroy_actor(a)
    with open(LAYOUT) as f:
        layout = json.load(f)
    z0 = W.academ_z()
    yaw = _ue_yaw()
    n = 0
    for name, part in layout["parts"].items():
        sm = unreal.load_asset(MESHES + "/SM_" + name)
        if sm is None:
            log("missing mesh SM_%s" % name)
            continue
        lx, ly, lz = part["location"]
        x, y = W.academ_to_map(lx, ly)
        loc = unreal.Vector(*W.to_unreal(x, y, z0 + lz, face_y))
        a = eas.spawn_actor_from_class(unreal.StaticMeshActor, loc, unreal.Rotator(0.0, 0.0, yaw))
        a.static_mesh_component.set_static_mesh(sm)
        a.set_actor_label("Academ_" + name)
        house = name.split("_")[0] in ("Novikova1", "Novikova3", "Maksimova8")
        a.tags = [TAG, "NoGrass", "NoTree"] + (["Roof"] if house and not name.endswith(("Glass", "GlassFar")) else [])
        a.set_folder_path("Outside/Academgorodok")
        c = a.static_mesh_component
        c.set_editor_property("lighting_channels", unreal.LightingChannels(True, True, False))
        if name.endswith(("_Glass", "_InsideGlass", "_GlassFar")):
            c.set_editor_property("cast_shadow", False)
        # Near only: the insides and the clear glass (60 m); from 55 m on the opaque panes close the windows
        # (QA's A/B: the furnished flats seen through clear glass cost ~1.7 ms on views of the town)
        if name.endswith(("_Inside", "_InsideGlass", "_Glass")):
            c.set_editor_property("ld_max_draw_distance", NEAR_CM)      # "Desired Max Draw Distance"
        if name.endswith("_GlassFar"):
            c.set_editor_property("min_draw_distance", NEAR_CM - 500.0)
        n += 1
    plant_cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralPlant")
    for i, (lx, ly, species, seed, age) in enumerate(layout.get("trees", [])):
        x, y = W.academ_to_map(lx, ly)
        p = eas.spawn_actor_from_class(plant_cls, unreal.Vector(*W.to_unreal(x, y, z0, face_y)))
        p.set_actor_label("Academ_Tree_%02d_%s" % (i, species))
        for k, v in (("species", species), ("seed", seed), ("age_at_start", age), ("demo_growth", False),
                     ("summary_distance", SUMMARY_DISTANCE)):
            p.set_editor_property(k, v)
        p.tags = [TAG]
        p.set_folder_path("Outside/Academgorodok")
    gardens = layout.get("garden") or []
    for gi, g in enumerate(gardens if isinstance(gardens, list) else [gardens]):
        # the square's garden: old maples (for the limes), birches and spruces either side of the axis;
        # the trees keep off the paths, hedges and beds (the Square mesh is tagged NoTree)
        forest_cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralForest")
        x, y = W.academ_to_map(*g["centre"])
        f = eas.spawn_actor_from_class(forest_cls, unreal.Vector(*W.to_unreal(x, y, z0, face_y)))
        f.set_actor_label("Academ_Garden_%d" % gi)
        f.set_editor_property("mix", [unreal.ForestMix(species=sp, weight=wt, young_age=ya, mature_age=ma)
                                      for sp, wt, ya, ma in g["mix"]])
        hx, hy = g["half"]
        for k, v in (("seed", 1947 + gi), ("count", g["count"]), ("spacing", 700.0),
                     # the ensemble's x runs along the map's -Y: swap the half extents
                     ("half_extent", unreal.Vector2D(hy * 100, hx * 100)), ("variants", 2),
                     ("min_world_z", (z0 - 3) * 100), ("max_world_z", (z0 + 3) * 100),
                     ("trace_up", 3000.0), ("trace_down", 3000.0), ("preview_in_editor", False)):
            f.set_editor_property(k, v)
        f.tags = [TAG]
        f.set_folder_path("Outside/Academgorodok")
    for i, l in enumerate(layout.get("lights", [])):      # the Gastronom's pendants and a lamp in each room
        lx, ly, lz = l["at"]
        x, y = W.academ_to_map(lx, ly)
        a = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(*W.to_unreal(x, y, z0 + lz - 0.25, face_y)))
        a.set_actor_label("Academ_Lamp_%02d" % i)
        c = a.point_light_component
        c.set_editor_property("intensity_units", unreal.LightUnits.LUMENS)
        c.set_editor_property("intensity", float(l["lumens"]))
        hx = l["colour"].lstrip("#")
        c.set_editor_property("light_color", unreal.Color(int(hx[4:6], 16), int(hx[2:4], 16), int(hx[0:2], 16), 255))
        c.set_editor_property("attenuation_radius", 650.0)
        c.set_editor_property("cast_shadows", False)
        c.set_editor_property("max_draw_distance", 4000.0)      # ~300 lamps in the flats: only the near ones
        c.set_editor_property("max_distance_fade_range", 500.0)
        # the flats' and the landings' lamps burn from dusk (AcademToggle.cpp); the shop's are always on
        kind = l.get("kind", "shop")
        a.tags = [TAG] + (["AcademLamp"] if kind != "shop" else []) + (["AcademLit"] if kind in ("room_lit", "stair") else [])
        if kind != "shop":
            c.set_editor_property("visible", False)
            c.set_editor_property("attenuation_radius", 450.0)
            c.set_editor_property("cast_shadows", kind != "stair")     # no light through the partitions
        a.set_folder_path("Outside/Academgorodok")
    # real doors in the doorways the builder lists (ALyudDoor fitted to each hole; docs/doors.md): flat entrances shut,
    # room doors open, as the builder sets "open"
    if layout.get("doors"):
        sys.path.insert(0, HERE)
        import place_doors

        def to_ue(lx, ly, lz):
            x, y = W.academ_to_map(lx, ly)
            return W.to_unreal(x, y, z0 + lz, face_y)
        place_doors.spawn_doors(layout["doors"], to_ue, tags=[TAG], folder="Outside/Academgorodok/Doors",
                                label_prefix="Academ_Door_", leaf_material=unreal.load_asset(DEST + "/Materials/MI_AC_ILeaf"),
                                log=log)
    log("placed %d parts, %d trees at map %s, yaw %.1f" % (n, len(layout.get("trees", [])), W.academ()[:2], yaw))


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
