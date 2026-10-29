"""Builds /Game/Characters/Humans/L_BodyTest for unreal/scripts/test_bodies.py (docs/characters.md "Clips"):
  desks: four people at club desks (seat_anims' chair and desk: seat 47 cm, desk 76, keyboard 55 in front), each placed
         with their own seated clip so ULyudFaceSubsystem puts them on the computer loops (Kid_boy's gamepad, Guy_a a
         gamer, Girl_b browsing, the owner idling);
  ground: three people walking in place on a 12-degree ramp, on a step and on flat ground (lyud.Body.OnFeet 1 turns the
         feet onto the ground for placed people).
A camera per spot (BodyCam_<name>).

  UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this file> -unattended -nullrhi
"""
import unreal

eal = unreal.EditorAssetLibrary
LEVEL = "/Game/Characters/Humans/L_BodyTest"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if eal.does_asset_exist(LEVEL):
    les.load_level(LEVEL)
    for a in eas.get_all_level_actors():
        eas.destroy_actor(a)
elif not les.new_level(LEVEL):
    raise RuntimeError("could not create " + LEVEL)
cube = unreal.load_asset("/Engine/BasicShapes/Cube")


def box(c, size, rot=None, label=""):
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*c), rot or unreal.Rotator(0, 0, 0))
    a.static_mesh_component.set_static_mesh(cube)
    a.set_actor_scale3d(unreal.Vector(size[0] / 100, size[1] / 100, size[2] / 100))
    a.set_actor_label(label)
    return a


def person(who, clip, at, yaw=0.0):
    act = eas.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(*at), unreal.Rotator(0, 0, yaw))
    act.set_actor_label("Body_" + who)
    comp = act.skeletal_mesh_component
    comp.set_skeletal_mesh_asset(unreal.load_asset("/Game/Characters/Humans/%s/SK_%s" % (who, who)))
    comp.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    data = unreal.SingleAnimationPlayData()
    data.set_editor_property("anim_to_play", unreal.load_asset(clip))
    data.set_editor_property("saved_looping", True)
    data.set_editor_property("saved_playing", True)
    data.set_editor_property("saved_play_rate", 1.0)
    comp.set_editor_property("animation_data", data)


def cam(name, at, look):
    rot = unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*at), unreal.Vector(*look))
    c = eas.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(*at), rot)
    c.set_actor_label("BodyCam_" + name)
    c.camera_component.set_editor_property("field_of_view", 50.0)


box((600, 0, -5), (2600, 2000, 10), label="Floor")
sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(roll=0, pitch=-45, yaw=-120))
sun.light_component.set_intensity(4.0)
eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300)).light_component.set_intensity(1.3)
eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(600, 800, 100))
H = "/Game/Characters/Humans/"
desks = [("Kid_boy", H + "Kid_boy/Anims/A_Kid_boy_Sit_Gamepad"), ("Guy_a", H + "Guy_a/Anims/A_Guy_a_Sit_KeyboardMouse"),
         ("Girl_b", H + "Girl_b/Anims/A_Girl_b_Sit_Browse"), ("Owner", H + "Owner/Anims/A_Owner_Sit_Idle")]
for i, (who, clip) in enumerate(desks):
    x = 200.0 * i
    box((x, 0, 45), (44, 44, 4), label="Seat_" + who)
    box((x, -23.5, 69), (44, 3, 45), label="Back_" + who)
    box((x, 70, 74.5), (140, 70, 3), label="Desk_" + who)
    box((x, 55, 77), (44, 15, 1.5), label="Keys_" + who)
    box((x, 140, 112), (60, 3, 35), label="Screen_" + who)
    person(who, clip, (x, 0, 0))
    cam(who, (x - 130, 150, 135), (x, 20, 85))
    cam(who + "_side", (x + 160, 30, 110), (x, 20, 80))
# the ground test: a ramp, a step, flat
walk = H + "Anims/A_Human_Walk"
idle = H + "Anims/A_Human_StandIdle"
box((900, -600, 0), (300, 200, 10), unreal.Rotator(roll=0, pitch=12, yaw=0), label="Ramp")
box((1300, -600, 7.5), (100, 200, 15), label="Step")
box((1300, -480, 30), (100, 10, 60), label="StepMark")       # a post at the step's near corner, to see where it is
person("Nao", idle, (900, -600, 0))
person("Guy_b", idle, (1300 - 50, -600, 0))
person("Girl_a", walk, (1650, -600, 0))
for nm, x in (("Ramp", 900), ("Step", 1300), ("Flat", 1650)):
    cam(nm, (x + 170, -240, 170), (x, -600, 45))
les.save_current_level()
unreal.log_warning("BODYTEST level saved")
