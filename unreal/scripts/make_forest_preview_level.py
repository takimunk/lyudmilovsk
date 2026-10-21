"""Builds /Game/Dev/L_ForestPreview: the Baltic species (pine, dune pine, spruce, alder, rowan, sea buckthorn, juniper, aspen) at
several ages and seasons, and every grass on a patch. Prints each DebugReport / grass state.

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import os
import unreal

eal = unreal.EditorAssetLibrary
LEVEL = "/Game/Dev/L_ForestPreview"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
_umap = os.path.join(unreal.Paths.project_content_dir(), "Dev/L_ForestPreview.umap")
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
ground.set_actor_scale3d(unreal.Vector(400, 400, 1))
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



forest_cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralForest")
f = eas.spawn_actor_from_class(forest_cls, unreal.Vector(0, 0, 0))
f.set_actor_label("Forest")
f.set_editor_property("half_extent", unreal.Vector2D(6000, 6000))
f.set_editor_property("count", 700)
f.set_editor_property("spacing", 450)
f.set_editor_property("snap_to_ground", True)
f.set_editor_property("preview_in_editor", True)
mix = []
for sp, w, y, m in (("ScotsPine", 3, 12, 45), ("NorwaySpruce", 2, 12, 40), ("SilverBirch", 1.5, 8, 30), ("BlackAlder", 1, 10, 35), ("Rowan", 0.5, 8, 25)):
    e = unreal.ForestMix()
    e.set_editor_property("species", sp); e.set_editor_property("weight", w)
    e.set_editor_property("young_age", y); e.set_editor_property("mature_age", m)
    mix.append(e)
f.set_editor_property("mix", mix)
f.rebuild()
f.bake_all()
unreal.log_warning("FOREST " + f.describe_state())
cam = eas.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(-7000, 0, 600), unreal.Rotator(-3, 0, 0))
cam.set_actor_label("SeriesView")
les.save_current_level()
unreal.log_warning("FOREST level saved")
