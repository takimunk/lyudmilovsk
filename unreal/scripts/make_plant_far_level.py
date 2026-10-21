"""Builds /Game/Dev/L_PlantFar: a row of live AProceduralPlant trees (poplar, maple, oak, silver birch, apple) on a
plain, for comparing the far view (the tree's bake, from SummaryDistance) with full detail: shoot_plant_far.py.

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import os
import unreal

eal = unreal.EditorAssetLibrary
LEVEL = "/Game/Dev/L_PlantFar"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if eal.does_asset_exist(LEVEL):
    # reuse it, emptied (deleting and recreating a level in one session fails: the registry still lists it)
    les.load_level(LEVEL)
    for a in eas.get_all_level_actors():
        if not isinstance(a, unreal.WorldSettings):
            eas.destroy_actor(a)
elif not les.new_level(LEVEL):
    raise RuntimeError("could not create " + LEVEL)

ground = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -50))
ground.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
ground.set_actor_scale3d(unreal.Vector(2000, 2000, 1))
ground.static_mesh_component.set_material(0, unreal.load_asset("/Engine/EngineMaterials/DefaultMaterial"))
sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 800), unreal.Rotator(0, -32, 60))
sun.light_component.set_intensity(8.0)
sun.light_component.set_light_color(unreal.LinearColor(1.0, 0.86, 0.7, 1))
sun.light_component.set_editor_property("atmosphere_sun_light", True)
sky = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300))
sky.light_component.set_editor_property("real_time_capture", True)
eas.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
eas.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, -3000, 100))

plant_cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralPlant")
for i, (species, age) in enumerate((("PyramidalPoplar", 30), ("Maple", 30), ("SwampWhiteOak", 80), ("SilverBirch", 45), ("Apple", 25))):
    p = eas.spawn_actor_from_class(plant_cls, unreal.Vector(i * 1500 - 3000, 0, 0))
    p.set_actor_label("Far_" + species)
    p.set_editor_property("species", species)
    p.set_editor_property("seed", 3)
    p.set_editor_property("age_at_start", age)
    p.set_editor_property("editor_preview_age", age)
    p.set_editor_property("demo_growth", False)
# close-ups of the crown tops (shoot_plant_far.py: tops)
for i, (species, top) in enumerate((("PyramidalPoplar", 2350), ("Maple", 1500), ("SwampWhiteOak", 1800), ("SilverBirch", 1900))):
    cam = eas.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(i * 1500 - 3000, -900, top), unreal.Rotator(0, 8, 90))
    cam.set_actor_label("Top_" + species)
les.save_current_level()
unreal.log_warning("PLANTFAR level saved")
