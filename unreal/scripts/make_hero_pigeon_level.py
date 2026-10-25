"""Builds /Game/Dev/L_HeroPigeon (not committed): the player's feral pigeon (SK_FeralPigeon_Hero) in every colour morph,
perched in a row and flying above, the old far-bird SK_FeralPigeon beside them, a bird for the first-person check, and
cameras for shoot_hero_pigeon.py (Close_<n>, Side_<n>, Fly_<n>, Row, Far). Sun, sky light, atmosphere.

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script="<this file>"
"""
import unreal

eal = unreal.EditorAssetLibrary
LEVEL = "/Game/Dev/L_HeroPigeon"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if eal.does_asset_exist(LEVEL):
    eal.delete_asset(LEVEL)
if not les.new_level(LEVEL):
    raise RuntimeError("could not create " + LEVEL)

ground = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -50))
ground.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
ground.set_actor_scale3d(unreal.Vector(400, 400, 1))
if eal.does_asset_exist("/Game/Dev/MI_BirdGround"):
    ground.static_mesh_component.set_material(0, unreal.load_asset("/Game/Dev/MI_BirdGround"))
sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 800), unreal.Rotator(0, -38, 125))
sun.light_component.set_intensity(9.0)
sun.light_component.set_light_color(unreal.LinearColor(1.0, 0.93, 0.84, 1))
sun.light_component.set_editor_property("atmosphere_sun_light", True)
sky = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300))
sky.light_component.set_editor_property("real_time_capture", True)
eas.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
eas.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
pp = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
pp.set_editor_property("unbound", True)
s = pp.get_editor_property("settings")
s.set_editor_property("override_auto_exposure_bias", True)
s.set_editor_property("auto_exposure_bias", -0.6)
s.set_editor_property("override_motion_blur_amount", True)
s.set_editor_property("motion_blur_amount", 0.0)
pp.set_editor_property("settings", s)
eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-500, 0, 100))

bird = unreal.load_class(None, "/Script/Lyudmilovsk.BirdActor")


def cam(label, loc, rot, fov=40.0):
    c = eas.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(*loc), unreal.Rotator(*rot))
    c.set_actor_label(label)
    c.camera_component.set_editor_property("field_of_view", fov)
    c.camera_component.set_editor_property("constrain_aspect_ratio", False)
    return c


def pigeon(label, loc, yaw, state, morph, hero=True, seed=1):
    a = eas.spawn_actor_from_class(bird, unreal.Vector(*loc), unreal.Rotator(0, 0, yaw))
    a.set_actor_label(label)
    a.set_editor_property("species", "FeralPigeon")
    a.set_editor_property("hero_body", hero)
    a.set_editor_property("initial_state", state)
    a.set_editor_property("morph", morph)
    a.set_editor_property("seed", seed)
    return a


N = 9
for n in range(N):
    x = n * 60.0
    pigeon("Perch_%d" % n, (x, 0, 0), 0, unreal.BirdState.PERCH, n, seed=n + 3)
    pigeon("Fly_%d" % n, (x, 0, 140), 0, unreal.BirdState.FLY, n, seed=n + 31)
    cam("Close_%d" % n, (x + 30, -40, 22), (0, -16, 127))
    cam("Side_%d" % n, (x, -85, 14), (0, -4, 90))
    cam("Fly_%d" % n, (x - 15, -95, 215), (0, -34, 80))
cam("Row", ((N - 1) * 30.0, -420, 90), (0, -10, 90), 50)
# the old far body next to the new one, at a distance (they must read alike there)
pigeon("Old", (-80, 0, 0), 0, unreal.BirdState.PERCH, 0, hero=False)
cam("Far", (-40, -900, 40), (0, -1, 92), 25)
# first person: the bird, the camera follows its eye (shoot_hero_pigeon.py)
pigeon("FPV", (0, 1500, 300), 0, unreal.BirdState.FLY, 1, seed=99)
cam("FPVCam", (0, 1500, 310), (0, 0, 0), 125)
cam("FPVOut", (-110, 1500, 330), (0, -10, 0), 60)
cam("FPVSide", (0, 1400, 300), (0, 0, 90), 50)
les.save_current_level()
unreal.log_warning("BIRDS hero pigeon level saved")
