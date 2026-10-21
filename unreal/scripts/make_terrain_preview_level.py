"""Builds /Game/Scene01/Lvl_TerrainPreview: only the outdoor ground (import_terrain.py) under a late
afternoon sun with a plain sea, for looking at the ground without rebuilding the club (not committed).
Then checks the collision: traces straight down on a 0.37 m lattice around the club and a 3.7 m one
over the whole map, and reports every spot where nothing is hit (a hole to fall through) or the hit
is off the surface terrain.ground_z() describes (the visible one) by more than 2 cm.

  unreal/scripts/make_terrain_preview.sh   (clears the old preview first)
"""
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import import_terrain  # noqa: E402

T = import_terrain.T
LEVEL = "/Game/Scene01/Lvl_TerrainPreview"
FACE_Y = 790.0
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eal = unreal.EditorAssetLibrary
if not les.new_level(LEVEL):
    raise RuntimeError("could not create " + LEVEL)

import_terrain.build(FACE_Y)

sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 800), unreal.Rotator(0, -22, 200))
sun.light_component.set_intensity(8.0)
sun.light_component.set_light_color(unreal.LinearColor(1.0, 0.88, 0.74, 1))
sun.light_component.set_editor_property("atmosphere_sun_light", True)
sky = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300))
sky.light_component.set_editor_property("real_time_capture", True)
eas.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
eas.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, -4300))
sea = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, FACE_Y, T.W.SEA_Z * 100))
sea.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
sea.set_actor_scale3d(unreal.Vector(6000, 6000, 1))
sea.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
sea.set_actor_label("PreviewSea")
eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-193, FACE_Y + 300, 200))
les.save_current_level()
unreal.log_warning("SCENE01 TERRAIN preview saved " + LEVEL)

# --- collision check -------------------------------------------------------------------------
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


landscape = [a for a in eas.get_all_level_actors() if isinstance(a, unreal.Landscape)]


def in_hole(x, y):
    return T.HOLE_X[0] <= x < T.HOLE_X[1] and T.HOLE_Y[0] <= y < T.HOLE_Y[1]


def trace(x, y):
    """World Z (m) of the first ground hit straight down at map (x, y), or None. In the portal hole the
    landscape is left out: an editor world traces its hole-less editor collision, the game cuts it."""
    ux, uy, _ = T.to_unreal(x, y, 0, FACE_Y)
    ignore = [sea] + (landscape if in_hole(x, y) else [])
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(ux, uy, 20000), unreal.Vector(ux, uy, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore,
                                                 unreal.DrawDebugTrace.NONE, True)
    if not hit:
        return None
    t = hit.to_tuple()
    return t[4].z / 100.0 if t[0] else None          # (blocking, initial overlap, time, distance, location, ...)


def check(points, label):
    holes, off, worst = [], [], 0.0
    for x, y in points:
        z = trace(x, y)
        if z is None:
            holes.append((x, y))
            continue
        d = abs(z - T.ground_z(x, y))
        worst = max(worst, d)
        if d > 0.02:
            off.append((round(x, 2), round(y, 2), round(z - T.ground_z(x, y), 2)))
    unreal.log_warning("SCENE01 TERRAIN check %s: %d traces, %d holes %s, %d off by > 2 cm %s, worst %.3f m"
                       % (label, len(points), len(holes), holes[:12], len(off), off[:12], worst))


near = [(-60 + i * 0.37, -40 + j * 0.37) for i in range(int(120 / 0.37)) for j in range(int(100 / 0.37))]
check([(x, y) for x, y in near if not (-6.3 < x < 6.3 and -1.0 < y < 0.05)], "club")   # the drop under the portal
far = [(T.W.X_MIN + 2 + i * 3.7, T.W.Y_MIN + 2 + j * 3.7) for i in range(int((T.W.X_MAX - T.W.X_MIN - 4) / 3.7))
       for j in range(int((T.W.Y_MAX - T.W.Y_MIN - 4) / 3.7))]
check(far, "map")
