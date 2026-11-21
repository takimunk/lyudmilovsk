"""Ambient sound points of the outdoor map: small, fixed sources the soundscape (ULyudSoundscapeSubsystem, docs/sound.md)
plays as a loop when the player is near and the season, the hour and the weather allow. Data only for now: the
soundscape does not read this file yet, and the two sounds are specs in audio/sounds.json, not generated (docs/sound.md
"Sound points").

  python3 tools/sound_points.py     writes unreal/Lyudmilovsk/Content/Audio/sound_points.json

The points come from the land's own data (read-only):

  orchard_bees   the hives in the old orchard (tools/south.py things() `beehive`): a hum round the row on warm dry days
                 from May to September
  reed_frogs_N   the brook's runnels through the reedbed (south.brook_water, the "reed" reach): moor and common frogs
                 croaking on warm spring nights (April to June)

On a branch without tools/south.py the file has no points.

Frame: map metres (docs/world_map.md), z the built ground's (tools/terrain.py) or the water's surface; the game turns them
into Unreal cm as it does south.json's things.
"""
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import terrain as T  # noqa: E402

try:
    import south as SO  # noqa: E402  (the Aton coast; absent on older branches)
except ImportError:
    SO = None

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
OUT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Audio", "sound_points.json")
FROGS_EVERY = 25.0          # m along the reed reach between two frog points (a chorus, not one voice)


def points():
    out = []
    if SO is None:
        return out
    hives = [t for t in SO.things() if t["kind"] == "beehive"] if hasattr(SO, "things") else []
    if hives:
        x = sum(t["x"] for t in hives) / len(hives)
        y = sum(t["y"] for t in hives) / len(hives)
        half = max(math.hypot(t["x"] - x, t["y"] - y) for t in hives)
        out.append({"id": "orchard_bees", "sound": "bees_hive", "x": round(x, 2), "y": round(y, 2),
                    "z": round(T.ground_z(x, y) + 0.5, 2), "radius": round(half + 4.0, 1), "audible": 18.0, "volume": 0.45,
                    "months": [5, 6, 7, 8, 9], "hours": [9.0, 19.0], "min_temp_c": 14.0, "no_rain": True})
    if hasattr(SO, "brook_water"):
        c = SO._course()
        last, n = -1e9, 0
        for x, y, z, half in SO.brook_water():
            b = SO.brook_at(x, y)
            if b is None or c["kind"][b[1]] != "reed" or c["s"][b[1]] - last < FROGS_EVERY:
                continue
            last = c["s"][b[1]]
            n += 1
            out.append({"id": "reed_frogs_%d" % n, "sound": "frogs_reed", "x": round(x, 2), "y": round(y, 2), "z": round(z, 2),
                        "radius": 10.0, "audible": 60.0, "volume": 0.5, "months": [4, 5, 6], "night": True,
                        "min_temp_c": 10.0, "no_rain": False})
    return out


def main():
    doc = {"about": "Ambient sound points (tools/sound_points.py, docs/sound.md \"Sound points\"): a loop of `sound` "
                    "(an audio/sounds.json id) from within `radius` of the point, heard to `audible` m, in `months`, "
                    "between `hours` or at night, over `min_temp_c`, not in rain when `no_rain`.",
           "frame": "map metres (docs/world_map.md): x west, y north, z up from the club's floor",
           "points": points()}
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    pts = doc.pop("points")
    head = json.dumps(doc, indent=1, ensure_ascii=False)[:-2]
    with open(OUT, "w") as f:              # a point a line
        f.write(head + ',\n "points": [\n' + ",\n".join("  " + json.dumps(p, ensure_ascii=False) for p in pts) + "\n ]\n}\n")
    print("sound_points: %d points -> %s" % (len(pts), OUT))


if __name__ == "__main__":
    main()
