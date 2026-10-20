"""Builds /Game/Characters/Humans/L_HumansPreview: every human seated on a simple chair at a desk,
playing one of its seated loops, under a warm light. For checking the characters in the editor.

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import unreal

eal = unreal.EditorAssetLibrary
LEVEL = "/Game/Characters/Humans/L_HumansPreview"
CAST = [("Zoomer", "Sit_KeyboardMouse"), ("Clerk", "Sit_KeyboardMouse"), ("Owner", "Sit_Idle"),
        ("Streamer", "Sit_Stream"), ("Kid_boy", "Sit_Gamepad"), ("Kid_girl", "Sit_ChinRest"),
        ("Guy_a", "Sit_KeyboardMouse"), ("Guy_b", "Sit_LeanBack"), ("Girl_a", "Sit_KeyboardMouse"),
        ("Girl_b", "Sit_Browse")]
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if eal.does_asset_exist(LEVEL):
    eal.delete_asset(LEVEL)
if not les.new_level(LEVEL):
    raise RuntimeError("could not create " + LEVEL)
cube = unreal.load_asset("/Engine/BasicShapes/Cube")


def box(loc, size, label):
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc))
    a.static_mesh_component.set_static_mesh(cube)
    a.set_actor_scale3d(unreal.Vector(size[0] / 100, size[1] / 100, size[2] / 100))
    a.set_actor_label(label)


box((0, 0, -5), (3000, 3000, 10), "Floor")
light = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(-40, 0, 60))
light.light_component.set_intensity(2.0)
eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300)).light_component.set_intensity(1.0)
for i, (name, anim) in enumerate(CAST):
    x = i * 140.0
    # chair at (x, 0), desk toward +Y (the meshes face +Y, so yaw 0)
    box((x, 0, 45), (45, 45, 4), "Seat_" + name)
    box((x, 60, 74), (120, 60, 4), "Desk_" + name)
    box((x, 95, 112), (55, 3, 33), "Monitor_" + name)
    act = eas.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(x, 0, 0), unreal.Rotator(0, 0, 0))
    act.set_actor_label("Human_" + name)
    comp = act.skeletal_mesh_component
    comp.set_skeletal_mesh_asset(unreal.load_asset("/Game/Characters/Humans/%s/SK_%s" % (name, name)))
    comp.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    data = unreal.SingleAnimationPlayData()
    data.set_editor_property("anim_to_play", unreal.load_asset("/Game/Characters/Humans/%s/Anims/A_%s_%s" % (name, name, anim)))
    data.set_editor_property("saved_looping", True)
    data.set_editor_property("saved_playing", True)
    data.set_editor_property("saved_position", 0.7)
    comp.set_editor_property("animation_data", data)
les.save_current_level()
unreal.log_warning("HUMANS preview level saved")
