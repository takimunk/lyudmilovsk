"""A small lit level to look at a built MetaHuman offscreen (metahuman_character.py builds them):
/Game/MetaHumans/L_MHShot with sun, sky light, sky atmosphere, a grey floor, the character at the origin facing +Y
and camera actors MHCam_Face / MHCam_Face34 / MHCam_Full (shoot_mh.py shoots through them).

    MH_ID=nao UnrealEditor-Cmd <uproject> -run=pythonscript -script="unreal/scripts/make_mh_shot_level.py" -unattended
"""
import os

import unreal

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
LEVEL = "/Game/MetaHumans/L_MHShot"


def spawn(cls, loc=(0, 0, 0), rot=(0, 0, 0), label=None):
    a = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(*rot))
    if label:
        a.set_actor_label(label)
    return a


def main():
    cid = os.environ.get("MH_ID", "nao")
    title = cid.capitalize()
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
        unreal.EditorAssetLibrary.delete_asset(LEVEL)
    les.new_level(LEVEL)
    sun = spawn(unreal.DirectionalLight, rot=(0, -40, -120), label="Sun")   # roll, pitch, yaw
    sun.light_component.set_editor_property("intensity", 6.0)
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    spawn(unreal.SkyAtmosphere)
    sky = spawn(unreal.SkyLight, label="Sky")
    sky.light_component.set_editor_property("real_time_capture", True)
    spawn(unreal.ExponentialHeightFog)
    floor = spawn(unreal.StaticMeshActor, loc=(0, 0, -1), label="Floor")
    floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
    floor.set_actor_scale3d(unreal.Vector(20, 20, 1))
    path = "/Game/MetaHumans/%s/Build/MHC_%s/BP_MHC_%s" % (title, title, title)
    bp = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if bp is None:
        raise RuntimeError("no blueprint class at " + path)
    ch = eas.spawn_actor_from_class(bp, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    ch.set_actor_label("MH_" + title)
    # MetaHumans face +Y: cameras on the +Y side looking back (yaw -90)
    for name, loc, rot, fov in (("MHCam_Face", (0, 80, 150), (0, -2, -90), 30),
                                ("MHCam_Face34", (55, 60, 150), (0, -2, -132), 30),
                                ("MHCam_Full", (0, 420, 95), (0, -2, -90), 30)):
        cam = spawn(unreal.CameraActor, loc=loc, rot=rot, label=name)
        cam.camera_component.set_editor_property("field_of_view", fov)
        cam.camera_component.set_editor_property("constrain_aspect_ratio", False)
        cam.tags = [name]
    les.save_current_level()
    unreal.log_warning("MHSHOT level %s with %s" % (LEVEL, bp.get_path_name() if bp else None))


main()
