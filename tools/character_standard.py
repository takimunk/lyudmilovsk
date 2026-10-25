"""Character size standard for scripts: reads the numbers from the C++ header that Unreal uses
(unreal/Lyudmilovsk/Source/Lyudmilovsk/Public/CharacterStandard.h), so code and scripts never drift apart.

All values are Unreal centimetres; the *_m helpers give metres for the Blender build.
"""
import os
import re

HEADER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "unreal", "Lyudmilovsk", "Source",
                      "Lyudmilovsk", "Public", "CharacterStandard.h")


def _read():
    text = open(HEADER).read()
    v = {k: float(x) for k, x in re.findall(r"constexpr float (\w+) = ([\d.]+)f;", text)}
    v["MinDoorClearWidth"] = 2 * v["PlayerCapsuleRadius"] + 2 * v["PassMargin"]
    v["MinDoorClearHeight"] = max(2 * v["PlayerCapsuleHalfHeight"], v["MaxNpcHeight"]) + 15.0
    return v


STD = _read()
PLAYER_RADIUS_CM = STD["PlayerCapsuleRadius"]
PLAYER_HALF_HEIGHT_CM = STD["PlayerCapsuleHalfHeight"]
MAX_NPC_HEIGHT_CM = STD["MaxNpcHeight"]
MIN_DOOR_CLEAR_WIDTH_M = STD["MinDoorClearWidth"] / 100.0
MIN_DOOR_CLEAR_HEIGHT_M = STD["MinDoorClearHeight"] / 100.0

# ALyudDoor (Doors/LyudDoor.h): the hole a door needs for a clear size and back, cm. Lining both sides, the open
# leaf's thickness across the opening per leaf; the head lining over the clear height.
DOOR_LEAF_CM = STD["DoorLeafThickness"]
DOOR_LINING_CM = STD["DoorLiningThickness"]
DOOR_SIZES_CM = {  # size name -> (clear width, clear height, leaves), as ELyudDoorSize
    "Standard": (STD["StandardDoorClearWidth"], STD["StandardDoorClearHeight"], 1),
    "Wide": (STD["WideDoorClearWidth"], STD["StandardDoorClearHeight"], 1),
    "Double": (STD["DoubleDoorClearWidth"], STD["DoubleDoorClearHeight"], 2),
}


def door_hole(clear_w, clear_h, leaves=1, frame=True):
    """The wall hole (w, h) in cm for an ALyudDoor with this clear size."""
    lining = DOOR_LINING_CM if frame else 0.0
    return clear_w + 2 * lining + leaves * DOOR_LEAF_CM, clear_h + lining


def door_clear(hole_w, hole_h, leaves=1, frame=True):
    """The clear size (w, h) in cm an ALyudDoor fitted to this hole (Size FitHole) gives."""
    lining = DOOR_LINING_CM if frame else 0.0
    return hole_w - 2 * lining - leaves * DOOR_LEAF_CM, hole_h - lining


def door_hole_for(size):
    """The hole (w, h) in cm for a standard size name: Standard, Wide, Double."""
    w, h, n = DOOR_SIZES_CM[size]
    return door_hole(w, h, n)
