"""Builds /Game/Dev/L_DoorTest (not committed): a floor and one wall along Y at x = 0 with five ALyudDoor in it, the
hero starting on the -X side (the doors swing to +X). For the offscreen door checks (unreal/scripts/test_doors.sh).

  y -600 standard, hinge left      y -200 wide, hinge right      y 300 double      y 800 standard, locked
  y 1200 fitted to a 110 x 220 hole (below the standard: flagged)

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import os
import sys
import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
import character_standard as cs  # noqa: E402

eal = unreal.EditorAssetLibrary
LEVEL = "/Game/Dev/L_DoorTest"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
_umap = os.path.join(unreal.Paths.project_content_dir(), "Dev/L_DoorTest.umap")
if eal.does_asset_exist(LEVEL):
    eal.delete_asset(LEVEL)
if os.path.exists(_umap):
    os.remove(_umap)
if not les.new_level(LEVEL):
    raise RuntimeError("could not create " + LEVEL)
cube = unreal.load_asset("/Engine/BasicShapes/Cube")
base = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
if eal.does_directory_exist("/Game/Dev/DoorMats"):
    eal.delete_directory("/Game/Dev/DoorMats")
mats = {}


def mat(name, color):
    if name not in mats:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset("MI_Door_" + name, "/Game/Dev/DoorMats",
                                                                     unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(mi, base)
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*color))
        mats[name] = mi
    return mats[name]


def box(x0, x1, y0, y1, z0, z1, label, m):
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2))
    a.static_mesh_component.set_static_mesh(cube)
    a.set_actor_scale3d(unreal.Vector((x1 - x0) / 100, (y1 - y0) / 100, (z1 - z0) / 100))
    a.set_actor_label(label)
    a.static_mesh_component.set_material(0, m)
    return a


WALL_T, WALL_H = 20.0, 300.0
DOORS = [  # (tag, y, size, hinge, extra props, hole)
    ("DoorTest_Std", -600.0, unreal.LyudDoorSize.STANDARD, unreal.LyudDoorHinge.LEFT, {}, cs.door_hole_for("Standard")),
    ("DoorTest_Wide", -200.0, unreal.LyudDoorSize.WIDE, unreal.LyudDoorHinge.RIGHT, {}, cs.door_hole_for("Wide")),
    ("DoorTest_Double", 300.0, unreal.LyudDoorSize.DOUBLE, unreal.LyudDoorHinge.LEFT, {}, cs.door_hole_for("Double")),
    ("DoorTest_Locked", 800.0, unreal.LyudDoorSize.STANDARD, unreal.LyudDoorHinge.RIGHT, {"locked": True}, cs.door_hole_for("Standard")),
    ("DoorTest_SmallHole", 1200.0, unreal.LyudDoorSize.FIT_HOLE, unreal.LyudDoorHinge.LEFT,
     {"hole_width": 110.0, "hole_height": 220.0}, (110.0, 220.0)),
]
box(-1500, 1500, -1500, 2000, -20, 0, "Floor", mat("Floor", (0.25, 0.22, 0.2, 1)))
wall = mat("Wall", (0.6, 0.58, 0.52, 1))
y = -1500.0
for tag, yc, size, hinge, extra, (hw, hh) in DOORS:
    box(-WALL_T / 2, WALL_T / 2, y, yc - hw / 2, 0, WALL_H, "Wall_" + tag, wall)
    box(-WALL_T / 2, WALL_T / 2, yc - hw / 2, yc + hw / 2, hh, WALL_H, "Head_" + tag, wall)
    y = yc + hw / 2
    d = eas.spawn_actor_from_class(unreal.load_class(None, "/Script/Lyudmilovsk.LyudDoor"), unreal.Vector(0, yc, 0), unreal.Rotator(0, 0, 0))
    d.set_actor_label(tag)
    d.set_editor_property("tags", [unreal.Name(tag)])
    d.set_editor_property("size", size)
    d.set_editor_property("hinge", hinge)
    d.set_editor_property("wall_thickness", WALL_T)
    for k, v in extra.items():
        d.set_editor_property(k, v)
    d.rebuild()
    unreal.log_warning("DOORLEVEL %s hole %.0f x %.0f (door says %.0f x %.0f) clear %.0f x %.0f" % (
        tag, hw, hh, d.get_editor_property("hole_width"), d.get_editor_property("hole_height"),
        d.get_editor_property("actual_clear_width"), d.get_editor_property("actual_clear_height")))
box(-WALL_T / 2, WALL_T / 2, y, 2000, 0, WALL_H, "Wall_End", wall)

sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(roll=0, pitch=-40, yaw=-140))
sun.light_component.set_intensity(6.0)
sun.light_component.set_editor_property("atmosphere_sun_light", True)
eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300)).light_component.set_editor_property("real_time_capture", True)
eas.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-400, -600, 100), unreal.Rotator(0, 0, 0))
les.save_current_level()
unreal.log_warning("DOOR test level saved")
