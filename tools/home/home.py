#!/usr/bin/env python3
"""The hero's own room in the club (docs/hero_room.md): writes Content/Home/home.json for ULyudHeroRoom.

Reads the room's export (blender/export/city/hero_room.json, built by blender/city/build_city.py from
blender/city/buildings/hero_room.py) and where it stands (tools/city/extra_places.json, plot P-HOME), and writes: the
meshes to spawn and their origin, the hole to cut in the hall's left wall, the lamps, the places for the hero's things
(the export's things with a `place`), the trophies that are not items, the house plants the pot takes.

    python3 tools/home/home.py            write
    python3 tools/home/home.py --check    fail if the file is stale or the data is wrong
Run tools/city/interiors.py too: it gives the same things their acts (tools/city/content/x_home.json).
"""
import json
import math
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
EXPORT = os.path.join(ROOT, "blender", "export", "city", "hero_room.json")
PLACES = os.path.join(ROOT, "tools", "city", "extra_places.json")
OUT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Home", "home.json")
TOURNAMENTS = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "City", "tournaments.json")
STRINGS = os.path.join(ROOT, "tools", "city", "content", "strings", "x_home.json")
PLANT_CPP = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Source", "Lyudmilovsk", "Private", "Plants", "PlantSpeciesHerbs.cpp")
PLOT = "P-HOME"

# The hall's left wall as blender/scripts/build_scene01_corridor.py builds it (wall_piece "Hall_Wall_L": x -3.5..-3.0,
# y -7.5..0, oil paint to 1.5 m, whitewash to the ceiling at 3.6 m), in Unreal cm (X = x, Y = -y).
WALL = {"x": [-350.0, -300.0], "y": [0.0, 750.0], "z1": 360.0, "paint_z": 150.0, "mesh_prefix": "Hall_Wall_L"}
# House plants of the procedural plant system the pot takes (PlantSpeciesHerbs.cpp, Indoor()): small ones that suit a 32 cm pot
PLANTS = ["Geranium", "SpiderPlant", "Aloe", "Cactus", "Begonia", "Avocado"]
CUP_TINTS = {"cog_chess": 0, "san_dominoes": 1, "tt_cup": 2}


def local_to_world(front, yaw, local):
    """The same as tools/city/interiors.py: building frame -> Unreal metres."""
    a = math.radians(yaw)
    x, y, z = local
    return [front[0] + x * math.sin(a) - y * math.cos(a), front[1] - x * math.cos(a) - y * math.sin(a), front[2] + z]


def world_yaw(plot_yaw, local_yaw):
    w = (plot_yaw - local_yaw) % 360.0
    return round(w - 360.0 if w > 180.0 else w, 1)


def cm(p):
    return [round(v * 100.0, 1) for v in p]


def build():
    problems = []
    ex = json.load(open(EXPORT, encoding="utf-8"))
    place = next(p for p in json.load(open(PLACES, encoding="utf-8"))["places"] if p["id"] == PLOT)
    front, yaw = place["front"], place["yaw"]
    strings = json.load(open(STRINGS, encoding="utf-8"))
    slots = []
    for t in ex["things"]:
        kind = (t.get("props") or {}).get("place")
        if not kind:
            continue
        slots.append({"id": t["id"], "type": kind, "ue": cm(local_to_world(front, yaw, t["pos"])), "yaw": world_yaw(yaw, t["yaw"])})
    if not any(s["type"] == "pot" for s in slots):
        problems.append("the room has no pot")
    lamps = [{"ue": cm(local_to_world(front, yaw, l["pos"])), "lumens": l["lumens"], "colour": l["colour"], "radius": round(l["radius"] * 100.0, 1)}
             for l in ex["lamps"]]
    # the room's box from its footprint, a little inside the walls' outer faces
    fp = [local_to_world(front, yaw, (x, y, 0.0)) for x, y in ex["footprint"]]
    box = {"min": [round(min(p[0] for p in fp) * 100, 1), round(min(p[1] for p in fp) * 100, 1), round(front[2] * 100 - 20, 1)],
           "max": [round(max(p[0] for p in fp) * 100, 1), round(max(p[1] for p in fp) * 100, 1), round((front[2] + ex["height"]) * 100, 1)]}
    # the door: the hole the room's module declares in its note (x -hw..hw, z 0..h) on the hall's face of the wall
    note = next(n for n in ex["notes"] if n.startswith("door hole"))
    hw = float(note.split("x -")[1].split("..")[0])
    dh = float(note.split("z 0..")[1])
    a = local_to_world(front, yaw, (-hw, 0.0, 0.0))
    b = local_to_world(front, yaw, (hw, 0.0, 0.0))
    wall = dict(WALL, door_y=sorted([round(a[1] * 100, 1), round(b[1] * 100, 1)]), door_z=round(dh * 100, 1))
    if abs(a[0] * 100 - WALL["x"][1]) > 0.6 or abs(yaw) > 0.01:
        problems.append("the room's front is not on the hall wall's face (X %.1f, yaw %.1f)" % (a[0] * 100, yaw))
    if not (WALL["y"][0] + 20 <= wall["door_y"][0] and wall["door_y"][1] <= WALL["y"][1] - 20):
        problems.append("the door is not inside the wall")
    trophies = []
    cups = json.load(open(TOURNAMENTS, encoding="utf-8")).get("tournaments", {}) if os.path.exists(TOURNAMENTS) else {}
    for cid in sorted(cups):
        trophies.append({"id": "cup:" + cid, "stamp": cups[cid].get("collect", "cups/" + cid), "name": "city.myroom.trophy.cup." + cid,
                         "shape": "cup", "tint": CUP_TINTS.get(cid, 0)})
    trophies.append({"id": "book:birds", "stamp": "", "name": "city.myroom.trophy.bird_book", "shape": "book", "tint": 0})
    for t in trophies:
        if t["name"] not in strings:
            problems.append("no string %s" % t["name"])
    src = open(PLANT_CPP, encoding="utf-8").read()
    for sp in PLANTS:
        if 'Indoor(TEXT("%s")' % sp not in src:
            problems.append("plant %s is not a house plant of PlantSpeciesHerbs.cpp" % sp)
        if "city.myroom.plant.%s" % sp not in strings:
            problems.append("no string city.myroom.plant.%s" % sp)
    data = {
        "about": "Written by tools/home/home.py from blender/export/city/hero_room.json and tools/city/extra_places.json: do not edit by hand (docs/hero_room.md).",
        "plot": PLOT, "origin": cm(front), "yaw": yaw,
        "meshes": ["/Game/City/Buildings/hero_room/SM_hero_room", "/Game/City/Buildings/hero_room/SM_hero_room_Detail"],
        "box": box, "wall": wall, "lamps": lamps, "slots": slots, "trophies": trophies, "plants": PLANTS,
    }
    return data, problems


def dump(data):
    lines = ["{"]
    keys = list(data)
    for k in keys:
        v = data[k]
        tail = "," if k != keys[-1] else ""
        if k in ("slots", "lamps", "trophies"):
            lines.append(' "%s": [' % k)
            lines += ["  " + json.dumps(x, ensure_ascii=False) + ("," if i < len(v) - 1 else "") for i, x in enumerate(v)]
            lines.append(" ]" + tail)
        else:
            lines.append(' "%s": %s%s' % (k, json.dumps(v, ensure_ascii=False), tail))
    lines.append("}")
    return "\n".join(lines) + "\n"


def main():
    data, problems = build()
    text = dump(data)
    if "--check" in sys.argv:
        old = open(OUT, encoding="utf-8").read() if os.path.exists(OUT) else ""
        if old != text:
            problems.append("%s is stale: run tools/home/home.py" % os.path.relpath(OUT, ROOT))
    else:
        os.makedirs(os.path.dirname(OUT), exist_ok=True)
        with open(OUT, "w", encoding="utf-8") as f:
            f.write(text)
    for p in problems:
        print("ERROR " + p)
    by = {}
    for s in data["slots"]:
        by[s["type"]] = by.get(s["type"], 0) + 1
    print("home: %d places (%s), %d lamps, %d trophies, %d plants%s" % (
        len(data["slots"]), ", ".join("%s %d" % kv for kv in sorted(by.items())), len(data["lamps"]), len(data["trophies"]),
        len(data["plants"]), "" if "--check" in sys.argv else ", wrote " + os.path.relpath(OUT, ROOT)))
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
