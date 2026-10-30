"""Builds /Game/Dev/L_FeederTest (not committed) for unreal/scripts/test_feeder.py: a copy of the befriending lawn
(/Game/Dev/L_TameTest, make it first with make_tame_test_level.py) with the grocery pavilion's meshes
(/Game/Academgorodok/Meshes/SM_Gastronom*, unreal/scripts/import_academgorodok.py) set down on it at PAV, unrotated
(its street front looks toward +X, the hall runs toward -Y), and three lamps in the hall.

  rm -rf Content/Dev/L_FeederTest*
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import unreal

eal = unreal.EditorAssetLibrary
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
SRC, LEVEL = "/Game/Dev/L_TameTest", "/Game/Dev/L_FeederTest"
PAV = (-1500.0, -2500.0, 0.0)

if eal.does_asset_exist(LEVEL):
    eal.delete_asset(LEVEL)
if not eal.duplicate_asset(SRC, LEVEL):
    raise RuntimeError("no " + SRC + ": run make_tame_test_level.py first")
les.load_level(LEVEL)
for part in ("SM_Gastronom", "SM_Gastronom_Inside", "SM_Gastronom_Glass", "SM_Gastronom_InsideGlass"):
    sm = unreal.load_asset("/Game/Academgorodok/Meshes/" + part)
    if not sm:
        raise RuntimeError("no mesh " + part)
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*PAV), unreal.Rotator(0.0, 0.0, 0.0))
    a.static_mesh_component.set_static_mesh(sm)
    a.set_actor_label("Grocery_" + part)
# lamps along the hall (the pavilion's frame: x along it at 93 deg, the hall between y = -6.75 and 1.5)
for x in (3.0, 9.0, 15.0):
    import math
    yaw = math.radians(93.0)
    bx = x * math.cos(yaw) + 2.5 * math.sin(yaw)
    by = x * math.sin(yaw) - 2.5 * math.cos(yaw)
    l = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(PAV[0] + bx * 100.0, PAV[1] - by * 100.0, 360.0))
    l.light_component.set_intensity(2500.0)
    l.light_component.set_attenuation_radius(1500.0)
les.save_current_level()
unreal.log_warning("FEEDERLEVEL saved " + LEVEL)
