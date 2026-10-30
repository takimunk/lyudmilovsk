"""L_BarkPreview: every tree species of PlantSpeciesLibrary at a young and an old age in two rows, a plant each, to look
at the bark close up (bench_forest_preview.py with FOREST_VIEWS and LEVEL=/Game/Dev/L_BarkPreview).
"""
import unreal

SPECIES = ["PyramidalPoplar", "BlackPoplar", "Apple", "Maple", "SwampWhiteOak", "SilverBirch", "DownyBirch", "WeepingBirch",
           "DwarfBirch", "StoneBirch", "ScotsPine", "DunePine", "NorwaySpruce", "BlackAlder", "Rowan", "SeaBuckthorn", "Juniper", "Aspen"]
AGES = (10, 35)
eal = unreal.EditorAssetLibrary
LEVEL = "/Game/Dev/L_BarkPreview"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
import os
_umap = os.path.join(unreal.Paths.project_content_dir(), "Dev/L_BarkPreview.umap")
if os.path.exists(_umap):
    os.remove(_umap)
if not les.new_level(LEVEL):
    raise RuntimeError("could not create " + LEVEL)
ground = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -50))
ground.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
ground.set_actor_scale3d(unreal.Vector(600, 200, 1))
ground.static_mesh_component.set_material(0, unreal.load_asset("/Engine/EngineMaterials/DefaultMaterial"))
sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 800), unreal.Rotator(0, -32, 60))
sun.light_component.set_intensity(8.0)
sun.light_component.set_light_color(unreal.LinearColor(1.0, 0.9, 0.78, 1))
sun.light_component.set_editor_property("atmosphere_sun_light", True)
sky = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300))
sky.light_component.set_editor_property("real_time_capture", True)
eas.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
eas.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralPlant")
for r, age in enumerate(AGES):
    for i, sp in enumerate(SPECIES):
        p = eas.spawn_actor_from_class(cls, unreal.Vector(i * 600, r * 1200, 0))
        p.set_actor_label("%s_%d" % (sp, age))
        p.set_editor_property("species", sp)
        p.set_editor_property("seed", 3 + i)
        p.set_editor_property("age_at_start", age)
        p.set_editor_property("demo_growth", False)
        p.set_editor_property("bsnap_to_ground" if False else "snap_to_ground", False)
        p.set_editor_property("editor_preview_age", age)
        p.set_editor_property("editor_preview_day", 200.0)
pp = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, -600, 300))
les.save_current_level()
unreal.log_warning("BARK level saved")
