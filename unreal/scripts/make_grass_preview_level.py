"""Builds /Game/Dev/L_GrassPreview: the Baltic species (pine, dune pine, spruce, alder, rowan, sea buckthorn, juniper, aspen) at
several ages and seasons, and every grass on a patch. Prints each DebugReport / grass state.

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import os
import unreal

eal = unreal.EditorAssetLibrary
LEVEL = "/Game/Dev/L_GrassPreview"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
_umap = os.path.join(unreal.Paths.project_content_dir(), "Dev/L_GrassPreview.umap")
if eal.does_asset_exist(LEVEL):
    eal.delete_asset(LEVEL)
if os.path.exists(_umap):
    os.remove(_umap)
if not les.new_level(LEVEL):
    raise RuntimeError("could not create " + LEVEL)

DAY = 293.67  # 20 October, 16:00
plant_cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralPlant")

ground = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -50))
ground.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
ground.set_actor_scale3d(unreal.Vector(1500, 1500, 1))
ground.set_actor_label("Ground")
ground.static_mesh_component.set_material(0, unreal.load_asset("/Engine/EngineMaterials/DefaultMaterial"))

sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 800), unreal.Rotator(0, -32, 60))
sun.light_component.set_intensity(8.0)
sun.light_component.set_light_color(unreal.LinearColor(1.0, 0.86, 0.7, 1))
sun.light_component.set_editor_property("atmosphere_sun_light", True)
sky = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300))
sky.light_component.set_editor_property("real_time_capture", True)
sky.light_component.set_intensity(1.0)
eas.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
eas.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))


def plant(x, y, age, label, species="PyramidalPoplar", seed=1, summary=False):
    p = eas.spawn_actor_from_class(plant_cls, unreal.Vector(x, y, 0))
    p.set_actor_label(label)
    p.set_editor_property("species", species)
    p.set_editor_property("seed", seed)
    p.set_editor_property("editor_preview_age", age)
    p.set_editor_property("editor_preview_day", DAY)
    p.set_editor_property("force_summary", summary)
    p.show_at(age, DAY, summary)
    unreal.log_warning("PLANT " + p.debug_report(age, DAY))
    return p



import os
grass_cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralGrass")
TURF = os.environ.get("GRASS_TURF", "0") == "1"
DENSITY = float(os.environ.get("GRASS_DENSITY", "1.0"))
n = 0
for gx in range(-1, 1):
    for gy in range(-1, 1):
        species = ("RedFescue", "MeadowGrass", "WavyHairGrass", "RedFescue")[n % 4]
        for sp in ((species, "Turf") if TURF else (species,)):
            g = eas.spawn_actor_from_class(grass_cls, unreal.Vector(gx * 6000 + 3000, gy * 6000 + 3000, 0))
            g.set_editor_property("species", sp)
            g.set_editor_property("seed", 100 + n)
            g.set_editor_property("half_extent", unreal.Vector2D(3000, 3000))
            g.set_editor_property("density_scale", DENSITY)
            g.set_editor_property("use_terrain_mask", False)
        n += 1
pp = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, 0, 300))
les.save_current_level()
unreal.log_warning("GRASS level saved")
