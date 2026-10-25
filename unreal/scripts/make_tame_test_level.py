"""Builds /Game/Dev/L_TameTest (not committed): a lawn by the sea with the vending machine, a low wall and a bench, daylight,
the hero starting 4 m from the machine. For the offscreen befriending recordings (unreal/scripts/record_tame.py):
buy feed, take it in the hand, crouch, creep up on a bird with the palm out.

  rm -rf Content/Dev/L_TameTest* Content/Dev/TameMats   (a level that exists cannot be replaced in-process)
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import os
import unreal

eal = unreal.EditorAssetLibrary
LEVEL = "/Game/Dev/L_TameTest"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
_umap = os.path.join(unreal.Paths.project_content_dir(), "Dev/L_TameTest.umap")
if eal.does_asset_exist(LEVEL):
    eal.delete_asset(LEVEL)
if os.path.exists(_umap):
    os.remove(_umap)
if not les.new_level(LEVEL):
    raise RuntimeError("could not create " + LEVEL)
cube = unreal.load_asset("/Engine/BasicShapes/Cube")
base = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")


def box(loc, size, label, color, rot=(0, 0, 0)):
    """rot: (pitch, yaw, roll); unreal.Rotator itself takes (roll, pitch, yaw)."""
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))
    a.static_mesh_component.set_static_mesh(cube)
    a.set_actor_scale3d(unreal.Vector(size[0] / 100, size[1] / 100, size[2] / 100))
    a.set_actor_label(label)
    mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset("MI_Tame_" + label, "/Game/Dev/TameMats",
                                                                 unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(mi, base)
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*color))
    a.static_mesh_component.set_material(0, mi)
    return a


if eal.does_directory_exist("/Game/Dev/TameMats"):
    eal.delete_directory("/Game/Dev/TameMats")
box((0, 0, -50), (6000, 6000, 100), "Lawn", (0.07, 0.12, 0.04, 1))
box((0, 3400, -60), (60000, 800, 100), "Sand", (0.45, 0.4, 0.3, 1))
box((0, 30000, -120), (60000, 50000, 100), "Sea", (0.02, 0.05, 0.08, 1))
box((900, -500, 30), (40, 600, 60), "Wall", (0.35, 0.33, 0.3, 1))
box((700, 500, 22), (50, 180, 8), "Bench", (0.25, 0.15, 0.08, 1))
for y in (-80, 80):
    box((700, 500 + y, 9), (40, 8, 18), "BenchLeg%d" % (y > 0), (0.1, 0.1, 0.1, 1))
sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(roll=0, pitch=-40, yaw=140))
sun.light_component.set_intensity(7.0)
sun.light_component.set_editor_property("atmosphere_sun_light", True)
eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300)).light_component.set_editor_property("real_time_capture", True)
eas.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
eas.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
# the machine 4 m ahead of the start, its front (+X) toward the hero
vm = eas.spawn_actor_from_class(unreal.load_class(None, "/Script/Lyudmilovsk.VendingMachine"), unreal.Vector(400, 0, 0), unreal.Rotator(roll=0, pitch=0, yaw=180))
vm.set_actor_label("Vending")
# the machine's body is a mesh in Scene01; here a dark box of its size behind the shelves (its front 20 cm out, the
# bottles stand 24 cm out, as behind the real one's glass)
box((408.75, 0, 95), (57, 90, 190), "VendingBody", (0.05, 0.12, 0.08, 1), (0, 180, 0))
eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, 0, 100), unreal.Rotator(0, 0, 0))
les.save_current_level()
unreal.log_warning("TAME test level saved")
