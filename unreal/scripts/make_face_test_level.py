"""Builds /Game/Characters/Humans/L_FaceTest: a few generated people standing (A_Human_StandIdle) in a row, a close-up
camera on each face and a player start in front of them, for unreal/scripts/test_faces.py (blinks, looks, lip-sync).

  FACE_CAST=Nao,Owner,Girl_b,Kid_boy UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this file> -unattended -nullrhi
"""
import json
import os

import unreal

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))

eal = unreal.EditorAssetLibrary
LEVEL = "/Game/Characters/Humans/L_FaceTest"
CAST = os.environ.get("FACE_CAST", "Nao,Owner,Girl_b,Kid_boy").split(",")
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if eal.does_asset_exist(LEVEL):
    les.load_level(LEVEL)
    for a in eas.get_all_level_actors():
        eas.destroy_actor(a)
elif not les.new_level(LEVEL):
    raise RuntimeError("could not create " + LEVEL)
floor = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(200, 0, -5))
floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
floor.set_actor_scale3d(unreal.Vector(12, 12, 0.1))
sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(roll=0, pitch=-35, yaw=-100))
sun.light_component.set_intensity(4.0)
eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300)).light_component.set_intensity(1.2)
eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(180, 220, 100), unreal.Rotator(0, -90, 0))
idle = unreal.load_asset("/Game/Characters/Humans/Anims/A_Human_StandIdle")
for i, who in enumerate(CAST):
    x = 120.0 * i
    act = eas.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(x, 0, 0), unreal.Rotator(0, 0, 0))
    act.set_actor_label("Face_" + who)
    comp = act.skeletal_mesh_component
    mesh = unreal.load_asset("/Game/Characters/Humans/%s/SK_%s" % (who, who))
    comp.set_skeletal_mesh_asset(mesh)
    comp.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    data = unreal.SingleAnimationPlayData()
    data.set_editor_property("anim_to_play", idle)
    data.set_editor_property("saved_looping", True)
    data.set_editor_property("saved_playing", True)
    data.set_editor_property("saved_play_rate", 1.0)
    comp.set_editor_property("animation_data", data)
    if os.environ.get("FACE_NATIVE"):                   # the body anim instance from the start (else the subsystem swaps)
        comp.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_BLUEPRINT)
        comp.set_anim_instance_class(unreal.LyudBodyAnimInstance.static_class())
    marks = os.path.join(ROOT, "blender", "characters", who.lower(), "face_marks.json")
    eye_z = 100.0 * sum(e["z"] for e in json.load(open(marks))["eyes"]) / 2 if os.path.isfile(marks) else 150.0
    cam = eas.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(x, 70.0, eye_z - 3.0),
                                     unreal.Rotator(roll=0, pitch=0, yaw=-90))
    cam.set_actor_label("FaceCam_" + who)
    cam.camera_component.set_editor_property("field_of_view", 22.0)
    cam.camera_component.set_editor_property("constrain_aspect_ratio", False)
    unreal.log_warning("FACETEST %s eye height ~%.0f cm" % (who, eye_z))
les.save_current_level()
unreal.log_warning("FACETEST level saved")
