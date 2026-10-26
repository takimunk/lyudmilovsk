"""Builds /Game/Dev/L_PlantBloom: every flowering tree and shrub species of the plant system, mature, in a row on a plain
(AProceduralPlant, seed 3), with cameras Cam_<Species>_Tree (the whole plant), _Twigs (the outer twigs close up),
_Mid (60 m) and _Far (120 m away) and _Bake (12 m, drawn as its bake, the kind forests use). For shoot_plant_bloom.py.

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import unreal

eal = unreal.EditorAssetLibrary
LEVEL = "/Game/Dev/L_PlantBloom"
SPECIES = [("PyramidalPoplar", 30), ("BlackPoplar", 30), ("Aspen", 25), ("BlackAlder", 25), ("SilverBirch", 25), ("DownyBirch", 25),
           ("WeepingBirch", 25), ("StoneBirch", 20), ("DwarfBirch", 8), ("SwampWhiteOak", 60), ("Apple", 15), ("Rowan", 18),
           ("Maple", 40), ("SeaBuckthorn", 8), ("ScotsPine", 40), ("DunePine", 30), ("NorwaySpruce", 40), ("Juniper", 15)]
SPACING = 2500
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if eal.does_asset_exist(LEVEL):
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
sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 800), unreal.Rotator(0, -42, 60))
sun.light_component.set_intensity(8.0)
sun.light_component.set_light_color(unreal.LinearColor(1.0, 0.92, 0.8, 1))
sun.light_component.set_editor_property("atmosphere_sun_light", True)
sky = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300))
sky.light_component.set_editor_property("real_time_capture", True)
eas.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-3000, -3000, 100))

plant_cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralPlant")


def cam(label, loc, pitch):
    c = eas.spawn_actor_from_class(unreal.CameraActor, loc, unreal.Rotator(0, pitch, 90))
    c.set_actor_label(label)


for i, (species, age) in enumerate(SPECIES):
    x = i * SPACING - len(SPECIES) * SPACING // 2
    p = eas.spawn_actor_from_class(plant_cls, unreal.Vector(x, 0, 0))
    p.set_actor_label("Bloom_" + species)
    p.set_editor_property("species", species)
    p.set_editor_property("seed", 3)
    p.set_editor_property("age_at_start", age)
    p.set_editor_property("editor_preview_age", age)
    p.set_editor_property("demo_growth", False)
    s = p.get_summary()
    h, r, base = s.height * 100.0, s.crown_radius * 100.0, s.crown_base_height * 100.0
    unreal.log_warning("PLANTBLOOM %s age %d height %.1f m crown radius %.1f m base %.1f m" % (species, age, h / 100, r / 100, base / 100))
    cam("Cam_%s_Tree" % species, unreal.Vector(x, -max(2.2 * h, 4.5 * r, 500.0), 0.5 * h), 0)
    cam("Cam_%s_Twigs" % species, unreal.Vector(x, -(0.5 * r + 130.0), base + 0.6 * (h - base)), -4)
    cam("Cam_%s_Mid" % species, unreal.Vector(x, -6000, 0.5 * h + 200), 0)
    cam("Cam_%s_Far" % species, unreal.Vector(x, -12000, 0.5 * h + 300), 0)
    cam("Cam_%s_Bake" % species, unreal.Vector(x, -max(1.4 * h, 3.5 * r, 700.0), 0.45 * h), 0)
les.save_current_level()
unreal.log_warning("PLANTBLOOM level saved")
