"""Where the hatch to Europa is (docs/europa.md): picks nothing by itself, it resolves the authored spot against the
ground and writes what the game reads.

    python3 tools/europa/europa_site.py          -> unreal/Lyudmilovsk/Content/Europa/europa.json (+ a report)

The spot: map (573, 100) m, some 440 m out from the high-water line in front of the beach under the club, past the
tidal flat (kept for it by the underwater world: plain sand, no reefs within 30 m), 160 m north of the sea railway's bank
and clear of the tidal channels; several metres of water even at spring low water (the tide falls to -4.5 m). The bed is read from tools/terrain.py's ground_z round the
collar (the landscape gets a 2 x 2 m hole under the shaft: terrain_holes, read back by terrain.py). Europa itself is
far off the map: 80 km south and 5 km up, beyond the sea mesh (30 km) and under the sky dome (200 km).
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools"))
import terrain as T  # noqa: E402
import world_map as W  # noqa: E402

OUT = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "Europa", "europa.json")
HATCH = (573.0, 100.0)          # map metres (X west / out to sea, Y north)
SITE_Z = -0.35                  # the bed under the lid seat (blender/europa/europa_kit.py SITE_Z)


def portal_face_y():
    with open(os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "UI", "map.json")) as f:
        return json.load(f)["frame"]["portal_face_y"]


def main():
    x, y = HATCH
    ring = [T.ground_z(x + 2.2 * math.cos(a), y + 2.2 * math.sin(a)) for a in [k * math.pi / 3 for k in range(6)]]
    bed = sum(ring) / len(ring)
    seat = bed - SITE_Z
    hw = W.SEA_Z - bed
    data = {
        "_doc": "Written by tools/europa/europa_site.py: do not edit by hand. The hatch to Europa (docs/europa.md); UE cm.",
        "hatch_map_m": [x, y],
        "hatch_cm": [round(x * 100, 1), round(portal_face_y() + y * 100, 1), round(seat * 100, 1)],
        "bed_m": round(bed, 2),
        "depth_high_water_m": round(hw, 2),
        "depth_spring_low_m": round(hw + W.TIDE_SPRING_LOW, 2),
        "terrain_holes": [[i, j] for i in (int(x) - 1, int(x)) for j in (int(y) - 1, int(y))],
        "europa_origin_cm": [0.0, -8.0e6, 5.0e5],
        "planet": {"elevation_deg": 35.0, "sun_offset_deg": 15.0, "distance_cm": 6.0e6, "diameter_deg": 26.0},
        "gravity": 0.13,
        "space_radius_cm": 6.0e5,
        "walk_radius_cm": 3.5e5,
    }
    with open(OUT, "w") as f:
        json.dump(data, f, indent=1)
        f.write("\n")
    print("europa_site: hatch map (%.0f, %.0f) m, bed %.2f m (water %.1f m at high tide, %.1f m at spring low), UE %s"
          % (x, y, bed, hw, hw + W.TIDE_SPRING_LOW, data["hatch_cm"]))


if __name__ == "__main__":
    main()
