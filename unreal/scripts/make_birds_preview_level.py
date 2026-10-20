"""Builds /Game/Dev/L_BirdsPreview: a hill above a flat sea with an AStarlingMurmuration over the
water, simulating in the editor viewport. For checking the murmuration (not committed).

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import os
import unreal

eal = unreal.EditorAssetLibrary
LEVEL = "/Game/Dev/L_BirdsPreview"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
_umap = os.path.join(unreal.Paths.project_content_dir(), "Dev/L_BirdsPreview.umap")
if eal.does_asset_exist(LEVEL):
    eal.delete_asset(LEVEL)
if os.path.exists(_umap):
    os.remove(_umap)
if not les.new_level(LEVEL):
    raise RuntimeError("could not create " + LEVEL)
cube = unreal.load_asset("/Engine/BasicShapes/Cube")


def box(loc, size, label):
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc))
    a.static_mesh_component.set_static_mesh(cube)
    a.set_actor_scale3d(unreal.Vector(size[0] / 100, size[1] / 100, size[2] / 100))
    a.set_actor_label(label)


# as in Scene 01: the club's door on a hilltop 45 m above the sea, the shore ~120 m out (UE +Y)
box((0, 0, -4350), (400000, 400000, 100), "Sea")
box((0, -6000, -2150), (80000, 18000, 4300), "Hill")
box((0, 7500, -3300), (80000, 9000, 2100), "Slope")
eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(-20, 0, 60)).light_component.set_intensity(6.0)
eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300)).light_component.set_intensity(1.0)
eas.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
birds = eas.spawn_actor_from_class(unreal.load_class(None, "/Script/Lyudmilovsk.StarlingMurmuration"), unreal.Vector(0, 30800, -4300))
birds.set_actor_label("Murmuration")
birds.set_editor_property("simulate_in_editor", True)
cam = eas.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 1500, 170), unreal.Rotator(5, 90, 0))
cam.set_actor_label("HilltopView")
les.save_current_level()
unreal.log_warning("BIRDS preview level saved")
