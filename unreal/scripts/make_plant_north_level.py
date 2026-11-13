"""Builds /Game/Dev/L_PlantNorth: the North's plants (PlantSpeciesNorth.cpp, docs/north.md "Plants") on a plain, for
shoot_plant_north.py.

  Ages      a column per species, rows young / middle / old (AProceduralPlant, live), Cam_Ages over them all and
            Cam_<Species> close on the middle one
  Seasons   the same species mature, a row per season: Season_<day>_<Species> (the shot script holds each at its day:
            spring 140, early summer 175, high summer 210, autumn 262, winter 20), Cam_Seasons
  Forest    an AProceduralForest of the belt's mixes (dwarf pine, alder, rhododendron) as the map draws it (bakes,
            LOD distances and cull as import_world sets them), Cam_Forest at 25 m and Cam_ForestFar at 90 m
  Patches   AProceduralGrass herb patches of fireweed, cotton grass and mountain avens (no terrain mask here: the whole
            patch grows), Cam_Patches

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import unreal

eal = unreal.EditorAssetLibrary
LEVEL = "/Game/Dev/L_PlantNorth"
# (species, young, middle, old age in years, camera height cm)
SPECIES = [("SiberianDwarfPine", 12, 30, 60, 220), ("DwarfAlder", 6, 15, 30, 250), ("GoldenRhododendron", 5, 12, 30, 80),
           ("Fireweed", 1, 3, 8, 150), ("MountainAvens", 2, 6, 15, 40), ("CottonGrass", 1, 3, 8, 50)]
SEASONS = (140, 175, 210, 262, 20)
COL = 500          # cm between species
ROW = 600          # cm between age rows
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if eal.does_asset_exist(LEVEL):
    les.load_level(LEVEL)
    for a in eas.get_all_level_actors():
        if not isinstance(a, unreal.WorldSettings):
            eas.destroy_actor(a)
elif not les.new_level(LEVEL):
    raise RuntimeError("could not create " + LEVEL)

ground = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -50))
ground.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
ground.set_actor_scale3d(unreal.Vector(600, 600, 1))
ground.static_mesh_component.set_material(0, unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial"))
sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 800), unreal.Rotator(0, -38, 130))
sun.light_component.set_intensity(8.0)
sun.light_component.set_light_color(unreal.LinearColor(1.0, 0.92, 0.82, 1))
sun.light_component.set_editor_property("atmosphere_sun_light", True)
sky = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300))
sky.light_component.set_editor_property("real_time_capture", True)
eas.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, -3000, 100))

plant_cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralPlant")
grass_cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralGrass")
forest_cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralForest")


def plant(species, x, y, age, seed, label):
    p = eas.spawn_actor_from_class(plant_cls, unreal.Vector(x, y, 0), unreal.Rotator(0, 0, (seed * 47) % 360))
    p.set_actor_label(label)
    for k, v in (("species", species), ("seed", seed), ("age_at_start", float(age)), ("editor_preview_age", float(age)),
                 ("demo_growth", False), ("summary_distance", 1.0e9)):
        p.set_editor_property(k, v)
    return p


def cam(label, loc, pitch, yaw):
    c = eas.spawn_actor_from_class(unreal.CameraActor, loc, unreal.Rotator(0, pitch, yaw))
    c.set_actor_label(label)


x0 = -(len(SPECIES) - 1) * COL / 2
# ages: rows along +Y (young nearest the camera)
for i, (sp, young, mid, old, h) in enumerate(SPECIES):
    x = x0 + i * COL
    for r, age in enumerate((young, mid, old)):
        plant(sp, x, r * ROW, age, 3 + r, "Age_%d_%s" % (r, sp))
    cam("Cam_" + sp, unreal.Vector(x, ROW - max(180, h * 1.6), h * 0.7 + 30), -15, 90)
cam("Cam_Ages", unreal.Vector(0, -1900, 650), -17, 90)

# seasons: mature, one row per day, behind the ages
y0 = 3 * ROW + 900
for r, day in enumerate(SEASONS):
    for i, (sp, young, mid, old, h) in enumerate(SPECIES):
        plant(sp, x0 + i * COL, y0 + r * 400, mid, 7, "Season_%03d_%s" % (day, sp))
cam("Cam_Seasons", unreal.Vector(0, y0 - 1700, 900), -24, 90)

# the forest as the map draws it: the belt's species, baked, at the map's spacing
fx, fy = 0, 9000
forest = eas.spawn_actor_from_class(forest_cls, unreal.Vector(fx, fy, 0))
forest.set_actor_label("Forest_North")
forest.set_editor_property("mix", [unreal.ForestMix(species=s, weight=w, young_age=ya, mature_age=ma) for s, w, ya, ma in
                                   (("SiberianDwarfPine", 0.45, 15, 60), ("DwarfAlder", 0.3, 8, 30),
                                    ("GoldenRhododendron", 0.25, 8, 30))])
for k, v in (("seed", 5), ("count", 160), ("spacing", 350.0), ("half_extent", unreal.Vector2D(2500, 2500)), ("variants", 2),
             ("cull_distance", 35000.0), ("preview_in_editor", False)):
    forest.set_editor_property(k, v)
cam("Cam_Forest", unreal.Vector(fx, fy - 2500 - 2500, 400), -8, 90)
cam("Cam_ForestFar", unreal.Vector(fx, fy - 2500 - 9000, 1200), -7, 90)

# herb patches (the map's settings: tools/north.py HERB_PATCH_SETTINGS)
for i, (sp, density, habitat) in enumerate((("Fireweed", 0.7, unreal.GrassHabitat.MEADOW),
                                             ("CottonGrass", 3.0, unreal.GrassHabitat.WET),
                                             ("MountainAvens", 2.0, unreal.GrassHabitat.MEADOW))):
    g = eas.spawn_actor_from_class(grass_cls, unreal.Vector(-6000 + i * 1200, 2000, 0))
    g.set_actor_label("Patch_" + sp)
    for k, v in (("species", sp), ("seed", 11 + i), ("half_extent", unreal.Vector2D(500, 500)), ("herb_density", density),
                 ("herb_habitat", habitat), ("use_terrain_mask", False), ("trace_up", 500.0), ("trace_down", 1000.0)):
        g.set_editor_property(k, v)
cam("Cam_Patches", unreal.Vector(-4800, 600, 350), -14, 90)
les.save_current_level()
unreal.log_warning("PLANTNORTH level saved")
