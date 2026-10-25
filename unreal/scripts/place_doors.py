"""Spawns ALyudDoor (Doors/LyudDoor.h) into the open level from a builder's list of doorways (docs/doors.md).

A builder (Blender, a layout.json) writes one entry per doorway, in its own frame and metres:

    {"name": "N1_flat_2_1", "at": [x, y, z],     # floor point in the middle of the hole, in the middle of the wall
     "out": [dx, dy],                            # horizontal direction the leaves swing to (into the room / corridor)
     "hole": [w, h], "wall": t,                  # the hole in the wall and the wall's thickness
     "leaves": 1, "hinge": "left",               # left/right as seen standing on the "out" side, facing the doorway
     "frame": false,                             # true: the door brings its own lining and casing (the builder doesn't)
     "open": false, "locked": false}

and the importer calls  spawn_doors(entries, to_unreal)  with to_unreal(x, y, z) -> Unreal cm (x, y, z) of that frame.
Door yaw is taken from the same mapping (two points), so mirrored or rotated frames come out right. Every door is fitted
to its hole (Size FitHole); one whose hole is below the character standard is reported (and still placed).
"""
import math
import unreal

DOOR_CLASS = "/Script/Lyudmilovsk.LyudDoor"


def _yaw(to_unreal, at, out):
    a = to_unreal(at[0], at[1], at[2])
    b = to_unreal(at[0] + out[0], at[1] + out[1], at[2])
    return math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))


def spawn_doors(entries, to_unreal, tags=(), folder="Doors", label_prefix="Door_", leaf_material=None, frame_material=None,
                log=unreal.log_warning):
    """Returns (spawned doors, names of doors below the standard)."""
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    cls = unreal.load_class(None, DOOR_CLASS)
    doors, small = [], []
    for e in entries:
        x, y, z = e["at"]
        loc = unreal.Vector(*to_unreal(x, y, z))
        yaw = _yaw(to_unreal, e["at"], e["out"])
        d = eas.spawn_actor_from_class(cls, loc, unreal.Rotator(0.0, 0.0, yaw))
        d.set_actor_label(label_prefix + e["name"])
        d.tags = [unreal.Name(t) for t in tags]
        d.set_folder_path(folder)
        d.set_editor_property("size", unreal.LyudDoorSize.FIT_HOLE)
        d.set_editor_property("hole_width", e["hole"][0] * 100.0)
        d.set_editor_property("hole_height", e["hole"][1] * 100.0)
        d.set_editor_property("wall_thickness", e.get("wall", 0.2) * 100.0)
        d.set_editor_property("double_leaf", e.get("leaves", 1) == 2)
        d.set_editor_property("hinge", unreal.LyudDoorHinge.RIGHT if e.get("hinge") == "right" else unreal.LyudDoorHinge.LEFT)
        d.set_editor_property("build_frame", bool(e.get("frame", False)))
        d.set_editor_property("start_open", bool(e.get("open", False)))
        d.set_editor_property("locked", bool(e.get("locked", False)))
        if leaf_material:
            d.set_editor_property("leaf_material", leaf_material)
        if frame_material:
            d.set_editor_property("frame_material", frame_material)
        d.rebuild()
        if d.get_editor_property("below_standard"):
            small.append(e["name"])
        doors.append(d)
    log("doors: %d placed, %d below the character standard%s" % (len(doors), len(small), (": " + ", ".join(small[:12])) if small else ""))
    return doors, small
