"""Builds /Game/Characters/Humans/L_LifeAnimsPreview: three different humans (Guy_a, Kid_boy, Girl_b)
each playing the shared A_Human_* clips with simple furniture, laid out in the clips' conventions.

  UnrealEditor <uproject> -run=pythonscript -script=<this file> -unattended -nullrhi
Then shoot it offscreen with unreal/scripts/shoot_life_anims.py.

Band i (character i) at y = -800 i; clip j at x = 260 j. Actors face +Y (yaw 0).
  chair: seat centre at the actor origin, top 47 cm; table top 75 cm from y = +30 to +100
  bed:   mattress top 50 cm, from y = -10 to -100 and x = -105 to +95 around the actor origin
"""
import os

import unreal

eal = unreal.EditorAssetLibrary
LEVEL = "/Game/Characters/Humans/L_LifeAnimsPreview"
CAST = os.environ.get("LIFE_CAST", "Guy_a,Kid_boy,Girl_b").split(",")     # LIFE_CAST=Nao,Guy_a for a new character
ALL_CLIPS = [("Seat", "chair", 1.0), ("Walk", None, 0.3), ("Talk", None, 1.2), ("SitIdle", "chair", 0.5), ("Eat", "table", 1.25),
         ("Read", "chair", 1.0), ("Sleep", "bed", 0.5), ("LieDown", "bed", 1.5), ("SitDown", "chair", 1.0)]
# Seat = the person's own seated clip at a computer (people.json computer_clip); LIFE_CLIPS=Seat,Walk picks some
CLIPS = [c for c in ALL_CLIPS if c[0] in os.environ.get("LIFE_CLIPS", ",".join(c[0] for c in ALL_CLIPS[1:])).split(",")]


def seat_clip(who):
    import json
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Lyudmilovsk", "Content", "PeopleData", "people.json")
    clip = next((p.get("computer_clip") for p in json.load(open(root))["people"] if p["mesh"] == who), None) or "Sit_KeyboardMouse"
    if clip == "Excel":
        return "/Game/Characters/Clerk/A_Clerk_Sit_Excel"
    owner, clip = clip.split("/") if "/" in clip else (who, clip)
    return "/Game/Characters/Humans/%s/Anims/A_%s_%s" % (owner, owner, clip)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if eal.does_asset_exist(LEVEL):                 # rebuild in place (re-creating it fails while registered)
    les.load_level(LEVEL)
    for a in eas.get_all_level_actors():
        eas.destroy_actor(a)
elif not les.new_level(LEVEL):
    raise RuntimeError("could not create " + LEVEL)
cube = unreal.load_asset("/Engine/BasicShapes/Cube")


def box(x0, x1, y0, y1, z0, z1, label):
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor,
                                   unreal.Vector((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2))
    a.static_mesh_component.set_static_mesh(cube)
    a.set_actor_scale3d(unreal.Vector((x1 - x0) / 100, (y1 - y0) / 100, (z1 - z0) / 100))
    a.set_actor_label(label)


box(-500, 2500, -2600, 600, -10, 0, "Floor")
light = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500),
                                   unreal.Rotator(roll=0, pitch=-50, yaw=-115))    # from the camera side
light.light_component.set_intensity(5.0)
eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300)).light_component.set_intensity(1.0)
eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(700, 400, 120))
for i, who in enumerate(CAST):
    y = -800.0 * i
    mesh = unreal.load_asset("/Game/Characters/Humans/%s/SK_%s" % (who, who))
    for j, (clip, furn, at) in enumerate(CLIPS):
        x = 260.0 * j
        tag = "%s_%s" % (who, clip)
        if furn in ("chair", "table"):
            box(x - 22, x + 22, y - 22, y + 22, 43, 47, "Seat_" + tag)
            box(x - 22, x + 22, y - 25, y - 22, 47, 92, "Back_" + tag)
        if furn == "table":
            box(x - 55, x + 55, y + 30, y + 100, 72, 75, "Table_" + tag)
        if furn == "bed":
            box(x - 105, x + 95, y - 100, y - 10, 30, 50, "Mattress_" + tag)
            box(x - 108, x + 98, y - 103, y - 13, 0, 30, "BedFrame_" + tag)
        act = eas.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(x, y, 0), unreal.Rotator(0, 0, 0))
        act.set_actor_label("Life_" + tag)
        comp = act.skeletal_mesh_component
        comp.set_skeletal_mesh_asset(mesh)
        comp.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_SINGLE_NODE)
        data = unreal.SingleAnimationPlayData()
        anim = unreal.load_asset(seat_clip(who) if clip == "Seat" else "/Game/Characters/Humans/Anims/A_Human_" + clip)
        data.set_editor_property("anim_to_play", anim)
        data.set_editor_property("saved_looping", True)
        data.set_editor_property("saved_playing", False)          # frozen at `at` seconds
        data.set_editor_property("saved_position", at)
        comp.set_editor_property("animation_data", data)
        unreal.log_warning("LIFEPREVIEW %s skeleton ok=%s mesh skeleton=%s anim skeleton=%s" % (
            tag, mesh.skeleton == anim.get_skeleton(), mesh.skeleton.get_name(), anim.get_skeleton().get_name()))
# cameras for shoot_life_anims.py: one per person, 2.9 m in front-right, looking at the body
for i, who in enumerate(CAST):
    for j, (clip, furn, at) in enumerate(CLIPS):
        x, y = 260.0 * j, -800.0 * i
        tz = 45.0 if furn == "bed" else 85.0
        eye = unreal.Vector(x + 120.0, y + 265.0, 175.0 if furn == "bed" else 150.0)
        tgt = unreal.Vector(x, y - (45.0 if furn == "bed" else 0.0), tz)
        rot = unreal.MathLibrary.find_look_at_rotation(eye, tgt)
        cam = eas.spawn_actor_from_class(unreal.CameraActor, eye, rot)
        cam.set_actor_label("LifeCam_%s_%s" % (who, clip))
        cam.camera_component.set_editor_property("field_of_view", 50.0)
les.save_current_level()
unreal.log_warning("LIFEPREVIEW level saved")
