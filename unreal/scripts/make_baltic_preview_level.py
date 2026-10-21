"""Builds /Game/Dev/L_BalticPreview: the Baltic species (pine, dune pine, spruce, alder, rowan, sea buckthorn, juniper, aspen) at
several ages and seasons, and every grass on a patch. Prints each DebugReport / grass state.

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import os
import unreal

eal = unreal.EditorAssetLibrary
LEVEL = "/Game/Dev/L_BalticPreview"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
_umap = os.path.join(unreal.Paths.project_content_dir(), "Dev/L_BalticPreview.umap")
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



species = ("ScotsPine", "DunePine", "NorwaySpruce", "BlackAlder", "Rowan", "SeaBuckthorn", "Juniper", "Aspen")
for r, (day, ages) in enumerate(((DAY, (6, 15, 35, 70)), (200, (35,)), (30, (35,)))):
    for i, sp in enumerate(species):
        for j, age in enumerate(ages):
            plant(i * 2200 - 7700 + j * 450, r * 3500 if r == 0 else r * 3500, age, "%s_%d_d%d" % (sp, age, day), species=sp, seed=3 + i * 5 + j)
            p = None
for i, sp in enumerate(species):
    p = plant(i * 1500 - 5000, -3500, 40, sp + "_summary", species=sp, seed=2, summary=True)

grass_cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralGrass")
for i, g in enumerate(("RedFescue", "MeadowGrass", "MarramGrass", "SeaLymeGrass", "CommonReed", "SandSedge", "WavyHairGrass")):
    a = eas.spawn_actor_from_class(grass_cls, unreal.Vector(i * 1000 - 3000, -9000, 0))
    a.set_actor_label("Grass_" + g)
    a.set_editor_property("species", g)
    a.set_editor_property("half_extent", unreal.Vector2D(450, 450))
    a.set_editor_property("snap_to_ground", True)
    a.rebuild()
    unreal.log_warning("GRASS " + a.describe_state())
    for d in (30, 120, 200, DAY, 340):
        a.set_editor_property("editor_preview_day", d)
cam = eas.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, -4200, 900), unreal.Rotator(-2, 90, 0))
cam.set_actor_label("SeriesView")
les.save_current_level()
unreal.log_warning("BALTIC preview level saved")
