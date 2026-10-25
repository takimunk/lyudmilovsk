"""Builds /Game/Dev/L_BirdSpecies: every imported bird species (/Game/Birds/<Id>/SK_<Id>) standing in a row on a lawn,
playing its Idle, and a flying copy above playing Flap, with cameras per species for shoot_bird_species.py
(Side_<Id>, Q34_<Id>, Close_<Id>, Fly_<Id>). Sun, sky light, atmosphere.

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script="<this file> [Id Id ...]"
"""
import json
import os
import sys

import unreal

eal = unreal.EditorAssetLibrary
LEVEL = "/Game/Dev/L_BirdSpecies"
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
TABLE = {s["id"]: s for s in json.load(open(os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "BirdsData", "species.json")))["species"]}
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
grass = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
ground.static_mesh_component.set_material(0, grass)
sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 800), unreal.Rotator(0, -38, 125))
sun.set_actor_label("Sun")
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
s.set_editor_property("override_auto_exposure_method", True)
s.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
s.set_editor_property("override_auto_exposure_bias", True)
s.set_editor_property("auto_exposure_bias", 10.5)
pp.set_editor_property("settings", s)
eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-500, 0, 100))

bird_cls = unreal.load_class(None, "/Script/Lyudmilovsk.BirdActor")
ids = sys.argv[1:] or [d for d in TABLE if eal.does_asset_exist("/Game/Birds/%s/SK_%s" % (d, d))]
x = 0.0
for sid in ids:
    mesh = unreal.load_asset("/Game/Birds/%s/SK_%s" % (sid, sid))
    if mesh is None:
        continue
    L = TABLE[sid]["lengthCm"]
    span = TABLE[sid]["wingspanCm"]
    for kind, z, state in (("Perched", 0.0, unreal.BirdState.PERCH), ("Flying", 60.0 + span, unreal.BirdState.FLY)):
        a = eas.spawn_actor_from_class(bird_cls, unreal.Vector(x, 0, z), unreal.Rotator(0, 0, 0))
        a.set_actor_label("%s_%s" % (kind, sid))
        a.set_editor_property("species", sid)
        a.set_editor_property("initial_state", state)
        a.set_editor_property("seed", len(sid) * 7 + int(z))
        a.get_editor_property("body").set_skinned_asset_and_update(mesh)
    d = max(L, 12) * 2.6
    # bird faces +X, its left is -Y: side camera on its left
    for label, loc, rot in (("Side", (x, -d, L * 0.35), (0, -4, 90)),
                            ("Q34", (x + d * 0.75, -d * 0.75, L * 0.6), (0, -12, 135)),
                            ("Close", (x + L * 0.55, -L * 0.75, L * 0.5), (0, -12, 125)),
                            ("Fly", (x - span * 0.2, -span * 1.6, 60.0 + span * 1.9), (0, -29, 90))):
        cam = eas.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(*loc), unreal.Rotator(*rot))
        cam.set_actor_label("%s_%s" % (label, sid))
        cam.camera_component.set_editor_property("field_of_view", 40.0)
    x += max(span, 60) * 1.6
les.save_current_level()
unreal.log_warning("BIRDS level saved with %d species" % len(ids))
