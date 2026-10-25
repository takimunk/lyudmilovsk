"""Builds /Game/Dev/L_AnimalsTest (not committed): a lawn with a low wall and a tree trunk, daylight, a player start. For
the offscreen cat and dog checks (unreal/scripts/test_animals.py): the breeds and coats in a row, their poses, a dog
and a cat meeting, a cat and a sparrow.

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import os
import unreal

eal = unreal.EditorAssetLibrary
LEVEL = "/Game/Dev/L_AnimalsTest"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
_umap = os.path.join(unreal.Paths.project_content_dir(), "Dev/L_AnimalsTest.umap")
if eal.does_asset_exist(LEVEL):
    eal.delete_asset(LEVEL)
if os.path.exists(_umap):
    os.remove(_umap)
if not les.new_level(LEVEL):
    raise RuntimeError("could not create " + LEVEL)
cube = unreal.load_asset("/Engine/BasicShapes/Cube")
cyl = unreal.load_asset("/Engine/BasicShapes/Cylinder")
base = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
if eal.does_directory_exist("/Game/Dev/AnimalsMats"):
    eal.delete_directory("/Game/Dev/AnimalsMats")


def shape(mesh, loc, size, label, color):
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc), unreal.Rotator(0, 0, 0))
    a.static_mesh_component.set_static_mesh(mesh)
    a.set_actor_scale3d(unreal.Vector(size[0] / 100, size[1] / 100, size[2] / 100))
    a.set_actor_label(label)
    mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset("MI_Animals_" + label, "/Game/Dev/AnimalsMats",
                                                                 unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(mi, base)
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*color))
    a.static_mesh_component.set_material(0, mi)
    return a


shape(cube, (0, 0, -50), (8000, 8000, 100), "Lawn", (0.09, 0.13, 0.05, 1))
shape(cube, (0, 2500, -49), (8000, 400, 100), "Path", (0.32, 0.31, 0.29, 1))
shape(cube, (1200, -900, 40), (30, 900, 80), "Wall", (0.36, 0.33, 0.3, 1))
shape(cyl, (-900, -400, 300), (40, 40, 600), "Trunk", (0.12, 0.09, 0.07, 1))
sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(roll=0, pitch=-35, yaw=135))
sun.light_component.set_intensity(7.0)
sun.light_component.set_editor_property("atmosphere_sun_light", True)
sun.light_component.set_editor_property("lighting_channels", unreal.LightingChannels(True, True, False))
eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300)).light_component.set_editor_property("real_time_capture", True)
eas.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
eas.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-600, 0, 100), unreal.Rotator(0, 0, 0))
les.save_current_level()
unreal.log_warning("ANIMALS test level saved")
