"""Builds /Game/Dev/L_BirdFlight: a quick level for looking at the player bird (not committed): a meadow, a 40 m
cliff over a sea plane to the west (+X), a low wall and a post to land on, low warm sun, a player start.

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
Then: unreal/scripts/record_bird_flight.sh pose:Starling pose_starling "" /Game/Dev/L_BirdFlight
"""
import os
import unreal

eal = unreal.EditorAssetLibrary
LEVEL = "/Game/Dev/L_BirdFlight"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
_umap = os.path.join(unreal.Paths.project_content_dir(), "Dev/L_BirdFlight.umap")
if eal.does_asset_exist(LEVEL):
    eal.delete_asset(LEVEL)
if os.path.exists(_umap):
    os.remove(_umap)
if not les.new_level(LEVEL):
    raise RuntimeError("could not create " + LEVEL)
cube = unreal.load_asset("/Engine/BasicShapes/Cube")
grass = unreal.load_asset("/Engine/EngineMaterials/DefaultMaterial")


def box(loc, size, label):
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc))
    a.static_mesh_component.set_static_mesh(cube)
    a.set_actor_scale3d(unreal.Vector(size[0] / 100, size[1] / 100, size[2] / 100))
    a.set_actor_label(label)
    return a


box((-20000, 0, -50), (60000, 80000, 100), "Meadow")        # top at 0, the land east of the cliff edge (X < 10000)
box((60000, 0, -4350), (100000, 80000, 100), "Sea")         # sea at -43 m (FBirdAir::SeaZ)
box((10200, 0, -2150), (400, 80000, 4300), "CliffFace")
box((3000, 1200, 60), (40, 600, 120), "Wall")
box((2500, -800, 100), (20, 20, 200), "Post")
sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(-25, 0, 200))
sun.light_component.set_intensity(8.0)
sun.light_component.set_editor_property("atmosphere_sun_light", True)
eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300)).light_component.set_intensity(1.0)
eas.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
eas.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
pp = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
pp.set_editor_property("unbound", True)
eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, 0, 100), unreal.Rotator(0, 0, 0))
les.save_current_level()
unreal.log_warning("BIRD FLIGHT preview level saved")
