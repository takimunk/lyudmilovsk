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
