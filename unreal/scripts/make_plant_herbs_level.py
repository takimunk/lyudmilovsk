"""Builds /Game/Dev/L_PlantHerbs: a flower bed (day lily, marigold, cosmos, aster), a hedge row (privet, spiraea), every
house plant in its pot in a row, and a "room" of 51 potted plants for the performance check; cameras Cam_* for
shoot_plant_herbs.py.

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import unreal

eal = unreal.EditorAssetLibrary
LEVEL = "/Game/Dev/L_PlantHerbs"
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
ground.set_actor_scale3d(unreal.Vector(400, 400, 1))
ground.static_mesh_component.set_material(0, unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial"))
sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 800), unreal.Rotator(0, -38, 130))
sun.light_component.set_intensity(8.0)
sun.light_component.set_light_color(unreal.LinearColor(1.0, 0.9, 0.78, 1))
sun.light_component.set_editor_property("atmosphere_sun_light", True)
sky = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300))
sky.light_component.set_editor_property("real_time_capture", True)
eas.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, -3000, 100))

plant_cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralPlant")


def plant(species, x, y, age=5, seed=1, label=None, yaw=None):
    p = eas.spawn_actor_from_class(plant_cls, unreal.Vector(x, y, 0), unreal.Rotator(0, 0, (seed * 47) % 360 if yaw is None else yaw))
    p.set_actor_label(label or ("%s_%d" % (species, seed)))
    p.set_editor_property("species", species)
    p.set_editor_property("seed", seed)
    p.set_editor_property("age_at_start", age)
    p.set_editor_property("editor_preview_age", age)
    p.set_editor_property("demo_growth", False)
    return p


def cam(label, loc, pitch, yaw):
    c = eas.spawn_actor_from_class(unreal.CameraActor, loc, unreal.Rotator(0, pitch, yaw))
    c.set_actor_label(label)


# the bed: day lilies at the back, asters, cosmos, a front row of marigolds
seed = 1
for i in range(3):
    plant("DayLily", -150 + i * 150, 260, 4, seed); seed += 1
for i in range(3):
    plant("Aster", -180 + i * 130, 160, 3, seed); seed += 1
for i in range(3):
    plant("Cosmos", -120 + i * 120, 80, 1, seed); seed += 1
for i in range(7):
    plant("Marigold", -240 + i * 80, 0, 1, seed); seed += 1
cam("Cam_Bed", unreal.Vector(0, -330, 150), -20, 90)
cam("Cam_BedClose", unreal.Vector(-60, -110, 70), -22, 90)
cam("Cam_AsterClose", unreal.Vector(-50, 40, 90), -20, 90)

# the hedge: privet 60 cm apart along X (its CrownBox clips it to a 0.9 x 0.65 x 1.3 m wall), then two spiraeas
for i in range(10):
    plant("Privet", -300 + i * 60, 1200, 6, 1 + i % 3, yaw=0)
plant("Spiraea", 600, 1200, 6, 1)
plant("Spiraea", 780, 1250, 6, 2)
cam("Cam_Hedge", unreal.Vector(250, 650, 160), -8, 90)
cam("Cam_HedgeClose", unreal.Vector(-50, 1000, 110), -5, 90)

# every house plant in a row, each with its own camera
INDOOR = ["RubberPlant", "WeepingFig", "Lemon", "Dracaena", "Monstera", "SnakePlant", "Aspidistra", "SpiderPlant", "Tradescantia",
          "Geranium", "Begonia", "AfricanViolet", "KentiaPalm", "Aloe", "Cactus", "Dieffenbachia", "AsparagusFern", "Amaryllis", "Avocado"]
for i, s in enumerate(INDOOR):
    x = -2400 + i * 300
    # small ones stand on a "sill" (a box 80 cm high) like on a windowsill
    small = s in ("Tradescantia", "Geranium", "Begonia", "AfricanViolet", "Aloe", "Cactus", "AsparagusFern", "SpiderPlant")
    z = 0
    if small:
        box = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, -1500, 0))
        box.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
        box.set_actor_scale3d(unreal.Vector(0.8, 0.5, 0.8))
        box.set_actor_location(unreal.Vector(x, -1500, 40), False, False)
        z = 80
    p = plant(s, x, -1500, 5, 1, "In_" + s)
    p.set_actor_location(unreal.Vector(x, -1500, z + 5), False, False)
    h = {"RubberPlant": 150, "WeepingFig": 170, "Lemon": 130, "Dracaena": 150, "Monstera": 100, "KentiaPalm": 160, "Dieffenbachia": 90,
         "SnakePlant": 80, "Aspidistra": 70, "Avocado": 110, "Amaryllis": 60}.get(s, 50)
    d = max(70, h * 1.25)
    cam("Cam_" + s, unreal.Vector(x, -1500 - d, z + h * 0.65 + 25), -18, 90)
cam("Cam_IndoorRow", unreal.Vector(0, -3300, 260), -8, 90)

# size bounds: every house plant old, several seeds, far from the row (checked by shoot_plant_herbs.py, PLANTBOUND)
for k, s in enumerate(INDOOR):
    for sd in range(1, 5):
        plant(s, -2400 + k * 300 + sd * 40, 4000, 60, sd * 17, "Bound_%s_%d" % (s, sd))

# the room for the performance check: 51 pots
for k in range(51):
    s = INDOOR[k % len(INDOOR)]
    plant(s, 6000 + (k % 9) * 120, -1000 + (k // 9) * 140, 4, 10 + k, "Room_%02d" % k)
cam("Cam_Room", unreal.Vector(6480, -1500, 300), -25, 90)
cam("Cam_Empty", unreal.Vector(6480, -1500, 300), 30, -90)
les.save_current_level()
unreal.log_warning("PLANTHERBS level saved")
