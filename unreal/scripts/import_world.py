"""The outdoor map (tools/world_map.py) in an Unreal level: generates the placeholder geometry
(tools/world_mesh.py: slab path, the club's entrance, promenade, town, cable car)
into Saved/World/world_coast.glb, imports it, plants the poplars and maples along the path and the
Baltic trees and shrubs around the club, lays the forests (AProceduralForest) and the grass.

Run as a script it adds all that to Lvl_Scene01 (build_scene01.sh runs it after import_terrain.py,
in a process of its own: Interchange skips a second scene import in one process);
make_world_preview_level.py imports build() for a level with only the map. Nothing here is
committed content, the build makes it every time.
"""
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools"))
import terrain as T  # noqa: E402
import world_map as W  # noqa: E402
import world_mesh  # noqa: E402

DEST = "/Game/Scene01/World"
# Shrubs and the low dune pines turn into their level-0 summary sooner than trees (cm): a sea buckthorn
# is ~650k leaf triangles up close, and every leaf casts a (non-Nanite) shadow.
SUMMARY_DISTANCE = {"SeaBuckthorn": 5000.0, "Juniper": 5000.0, "DunePine": 8000.0}
# The forests (world_map.forests) stay out of the level until AProceduralForest stops crashing play
# ("Assertion failed: InstanceParams.LODs <= 8" in the HISM scene proxy, first PIE in Lvl_Scene01).
FORESTS = os.environ.get("LYUD_FORESTS") == "1"
eal = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _log(msg):
    unreal.log_warning("SCENE01 WORLD " + msg)


def to_ue(x, y, z, face_y):
    return unreal.Vector(*W.to_unreal(x, y, z, face_y))


def build(face_y, textures_dir=None, plants=True, report=False):
    """Imports the map into the open level; face_y = the portal face, Unreal Y cm."""
    textures_dir = textures_dir or os.path.join(REPO, "textures", "podyezd")
    out_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "World")
    os.makedirs(out_dir, exist_ok=True)
    glb = os.path.join(out_dir, "world_coast.glb")
    # positions in the .glb are map metres; shift them onto the portal by moving the actors after import
    tris = world_mesh.build(glb, textures_dir)
    _log("generated %s, %d triangles" % (glb, tris))

    before = set(a.get_name() for a in eas.get_all_level_actors())
    src = unreal.InterchangeManager.create_source_data(glb)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_scene(DEST, src, params)
    mgr.wait_until_all_tasks_done(True)

    for path in eal.list_assets(DEST, True):
        sm = eal.load_asset(path)
        if not isinstance(sm, unreal.StaticMesh):
            continue
        body = sm.get_editor_property("body_setup")
        if body:
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        settings = sm.get_editor_property("nanite_settings")
        settings.enabled = True
        sm.set_editor_property("nanite_settings", settings)
    eal.save_directory(DEST)                         # meshes, their materials and textures

    actors = [a for a in eas.get_all_level_actors() if a.get_name() not in before]
    for a in actors:
        if isinstance(a, unreal.StaticMeshActor):
            a.set_actor_location(unreal.Vector(0, face_y, 0), False, False)
            c = a.static_mesh_component
            # the outdoor sun lights only channel 1; channel 0 keeps Lumen and the sky lighting it
            c.set_editor_property("lighting_channels", unreal.LightingChannels(True, True, False))
            a.tags = list(a.tags) + ["NoGrass", "NoTree"]   # nothing grows on slabs, roofs or stations
        a.set_folder_path("Outside/World")
    _log("imported %d actors" % len(actors))

    if plants:
        plant_cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralPlant")
        for group, items in (("Alley", W.alley_trees()), ("Coast", W.coast_plants())):
            for i, (x, y, species, seed, age) in enumerate(items):
                p = eas.spawn_actor_from_class(plant_cls, to_ue(x, y, T.ground_z(x, y), face_y))
                p.set_actor_label("%s_%02d_%s" % (group, i, species))
                for k, v in (("species", species), ("seed", seed), ("age_at_start", age), ("demo_growth", False),
                             ("summary_distance", SUMMARY_DISTANCE.get(species, 15000.0))):
                    p.set_editor_property(k, v)
                p.set_folder_path("Outside/" + group)
                if report:
                    _log("PLANT " + p.debug_report(age, 293.67))
            _log("%s: %d plants" % (group, len(items)))
        grass_cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralGrass")
        for i, (x, y, hx, hy, species, seed, density, zmin, zmax) in enumerate(W.grass_patches()):
            g = eas.spawn_actor_from_class(grass_cls, to_ue(x, y, T.ground_z(x, y), face_y))
            g.set_actor_label("Grass_%02d_%s" % (i, species))
            for k, v in (("species", species), ("seed", seed), ("half_extent", unreal.Vector2D(hx * 100, hy * 100)),
                         ("density_scale", density), ("min_world_z", zmin * 100), ("max_world_z", zmax * 100),
                         ("trace_up", 5000.0), ("trace_down", 6000.0)):
                g.set_editor_property(k, v)
            g.set_folder_path("Outside/Grass")
        _log("grass: %d patches" % len(W.grass_patches()))
        forest_cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralForest")
        total = 0
        for i, (x, y, hx, hy, mix, count, spacing, zmin, zmax, seed) in enumerate(W.forests() if FORESTS else ()):
            f = eas.spawn_actor_from_class(forest_cls, to_ue(x, y, T.ground_z(x, y), face_y))
            f.set_actor_label("Forest_%02d_%s" % (i, mix))
            f.set_editor_property("mix", [unreal.ForestMix(species=sp, weight=w, young_age=ya, mature_age=ma)
                                          for sp, w, ya, ma in W.FOREST_MIX[mix]])
            for k, v in (("seed", seed), ("count", count), ("spacing", spacing * 100),
                         ("half_extent", unreal.Vector2D(hx * 100, hy * 100)), ("variants", 3),
                         ("min_world_z", zmin * 100), ("max_world_z", zmax * 100),
                         ("trace_up", 8000.0), ("trace_down", 10000.0), ("preview_in_editor", False)):
                f.set_editor_property(k, v)
            f.set_folder_path("Outside/Forest")
            total += count
        _log("forests: ~%d trees%s" % (total, "" if FORESTS else " (off: set LYUD_FORESTS=1)"))


def starlings_location(face_y):
    """Where the murmuration is placed: over the sea to the left of the door."""
    x, y = W.starlings()
    return to_ue(x, y, W.SEA_Z, face_y)


if __name__ == "__main__":
    import json
    with open(os.path.join(REPO, "blender", "export", "scene01_layout.json")) as f:
        layout = json.load(f)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not les.load_level("/Game/Scene01/Lvl_Scene01"):
        raise RuntimeError("no /Game/Scene01/Lvl_Scene01: run import_scene01.py first")
    build(layout["outside"]["portal_face_y"])
    les.save_current_level()
    eal.save_directory("/Game/Scene01")
    _log("done")
