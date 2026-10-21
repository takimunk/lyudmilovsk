"""Builds /Game/Dev/L_PlantsPreview: AProceduralPlant poplars on a grassy plane in afternoon sun,
one per age (a growth series), plus a black poplar and a far-away level-0 summary. Prints each
plant's DebugReport. For checking the plant system (the level is not committed).

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import os
import unreal

eal = unreal.EditorAssetLibrary
LEVEL = "/Game/Dev/L_PlantsPreview"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
_umap = os.path.join(unreal.Paths.project_content_dir(), "Dev/L_PlantsPreview.umap")
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


# a growth series of one tree (same seed): sprout to 30 years
for i, age in enumerate((1, 2, 4, 7, 11, 16, 22, 30)):
    plant(i * 900 - 3150, 0, age, "Poplar_age%02d" % age)
# the same 30-year tree through the year
for i, day in enumerate((100, 125, 180, 280, 293.67, 305, 320)):
    p = plant(i * 900 - 2700, 2500, 30, "Poplar_day%03d" % int(day))
    p.set_editor_property("editor_preview_day", day)
    p.show_at(30, day, False)
    unreal.log_warning("PLANT day %.0f: %s" % (day, p.debug_report(30, day)))
plant(-3600, 5200, 30, "BlackPoplar_30", species="BlackPoplar", seed=3)
plant(-600, 5200, 30, "Poplar_seed2", seed=2)
plant(2400, 5200, 30, "Poplar_summary", summary=True)

cam = eas.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, -4200, 900), unreal.Rotator(-2, 90, 0))
cam.set_actor_label("SeriesView")
les.save_current_level()
unreal.log_warning("PLANTS preview level saved")
