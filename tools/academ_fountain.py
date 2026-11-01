"""Where the Academgorodok's fountain stands and where its water goes (docs/fountain.md): the white cast-stone two-tier
fountain in the middle of the square's round flowerbed (blender/academgorodok/details.py fountain(), placed by
build_academgorodok.py at plan.SQUARE["fountain"]).

  python3 tools/academ_fountain.py      -> unreal/Lyudmilovsk/Content/Academgorodok/fountain.json

Read by ALyudFountain (Source/Lyudmilovsk/Private/World/LyudFountain.cpp: the water, the jets, the splashes, the sound).
Frame of the file: Unreal metres (UE cm / 100), like wires.json: X as the map, Y = portal face + map Y, Z up. The
heights are over the fountain's foot and come from the stone's lathe profiles in details.fountain(): change them
together.
"""
import argparse
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "blender", "academgorodok"))
import world_map as W  # noqa: E402
import plan as P  # noqa: E402

OUT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Academgorodok", "fountain.json")

# details.fountain(), metres over the foot
STONE = {
    # the lower basin: inside wall (0.80, 1.19) -> (0.88, 1.25), rim top 1.29; the water stands a little under the lip
    "lower_level": 1.235, "lower_r": 0.865, "lower_floor": 1.15,
    # the upper bowl: inside (0.20, 1.82) -> (0.27, 1.85), outer lip (0.325, 1.83): brim-full, it spills all round
    "upper_level": 1.846, "upper_r": 0.296, "upper_floor": 1.815,
    "lip_r": 0.333, "lip_z": 1.838,
    # the fleur-de-lis finial's top: the jet's nozzle
    "nozzle_z": 2.36,
    # the stem through the lower water, the finial's neck through the upper
    "stem_r": 0.085, "neck_r": 0.05,
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=OUT)
    a = ap.parse_args()
    fx, fy = P.SQUARE["fountain"]
    mx, my = W.academ_to_map(fx, fy)
    try:
        with open(os.path.join(ROOT, "blender", "export", "scene01_layout.json")) as f:
            face_y = json.load(f)["outside"]["portal_face_y"]
    except (OSError, KeyError):
        face_y = 790.0
    # the ensemble's ground is levelled flat at academ_z (world_map.academ_flat); the fountain stands on it
    doc = {"version": 1, "frame": "unreal_metres", "face_y_cm": face_y,
           "at": [round(mx, 3), round(my + face_y / 100.0, 3), W.academ_z()],
           # the ensemble's x axis (east) on the map, degrees from map +X (the gadroons and the fleur turn with it)
           "yaw": round(math.degrees(math.atan2(-math.cos(W.academ()[2]), math.sin(W.academ()[2]))), 2),
           "stone": STONE}
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    with open(a.out, "w") as f:
        json.dump(doc, f, indent=1)
    print("academ_fountain: at %s -> %s" % (doc["at"], a.out))


if __name__ == "__main__":
    main()
