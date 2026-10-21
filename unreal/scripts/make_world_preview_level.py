"""Builds /Game/Scene01/Lvl_WorldPreview: only the outdoor map (import_terrain.py's ground, then
import_world.py on it) under a late
afternoon sun with a plain sea, for checking the map without rebuilding the club (not committed).

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import import_terrain  # noqa: E402
import import_world  # noqa: E402

LEVEL = "/Game/Scene01/Lvl_WorldPreview"
FACE_Y = 790.0
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eal = unreal.EditorAssetLibrary
if eal.does_directory_exist(import_world.DEST):
    eal.delete_directory(import_world.DEST)
if eal.does_asset_exist(LEVEL):
    eal.delete_asset(LEVEL)
if not les.new_level(LEVEL):
    raise RuntimeError("could not create " + LEVEL)

import_terrain.build(FACE_Y)
import_world.build(FACE_Y, report=True)

sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 800), unreal.Rotator(0, -18, 200))
sun.light_component.set_intensity(8.0)
sun.light_component.set_light_color(unreal.LinearColor(1.0, 0.86, 0.7, 1))
sun.light_component.set_editor_property("atmosphere_sun_light", True)
sky = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300))
sky.light_component.set_editor_property("real_time_capture", True)
eas.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
eas.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, -4300))

sea = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, FACE_Y, -4300))
sea.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
sea.set_actor_scale3d(unreal.Vector(6000, 6000, 1))
mel = unreal.MaterialEditingLibrary
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_PreviewSea", "/Game/Scene01/World", unreal.Material,
                                                               unreal.MaterialFactoryNew())
col = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -300, 0)
col.set_editor_property("constant", unreal.LinearColor(0.03, 0.08, 0.11, 1))
mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 200)
rough.set_editor_property("r", 0.08)
mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(mat)
eal.save_loaded_asset(mat)
sea.static_mesh_component.set_material(0, mat)
sea.set_actor_label("PreviewSea")

flock = unreal.load_class(None, "/Script/Lyudmilovsk.StarlingMurmuration")
if flock:
    eas.spawn_actor_from_class(flock, import_world.starlings_location(FACE_Y)).set_actor_label("Starlings")
eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-193, FACE_Y + 300, 120))
les.save_current_level()
unreal.log_warning("WORLD preview saved " + LEVEL)
