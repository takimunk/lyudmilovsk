#!/usr/bin/env python3
"""The Mayak lighthouse's game data (docs/lighthouse.md): where its light, its ways up and down and its weather log are, and
its words in three languages.

    python3 tools/lighthouse/lighthouse.py           write them
    python3 tools/lighthouse/lighthouse.py --check   exit 1 when a written file differs from what this script makes

Reads Content/South/south.json (the lighthouse and the meteo screen, tools/south.py) and their manifests
(blender/export/south/manifest_*.json: the tower's lamp, door_stand, hatch, the screen's read_at; blender/south/props_*.py),
writes
  Content/South/lighthouse.json                 the light's lamp, the stairs up/down, the log (Unreal cm and yaw)
  Content/Localization/lighthouse/strings_<lang>.json   tools/lighthouse/strings_src.py

Frames: south.json is map metres; the world is Unreal cm with y moved by the club's portal face (city_plan.json
portal_face_y). A manifest point {"x","y","z","yaw"} is in the mesh's Blender frame: (x, y) -> the actor's local (x, -y),
the actor turned by the thing's yaw - front_yaw (import_north.py spawn_mesh), and it faces the thing's yaw - yaw.
"""
import collections
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tools", "town"))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import south_people as SP     # noqa: E402  (the coast's manifests and frames)
import strings_src as W       # noqa: E402

CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
OUT = os.path.join(CONTENT, "South", "lighthouse.json")
STRINGS = os.path.join(CONTENT, "Localization", "lighthouse", "strings_%s.json")
OD = collections.OrderedDict
QUEST = "lighthouse_paraffin"
# where the hero lands a little off the spot he left from, so the other way's prompt is not under his feet at once, m
STEP_OFF = 0.5


def cm(c, x, y, z):
    p = c.at(x, y, z)
    return [round(p[0] * 100.0, 1), round(p[1] * 100.0, 1), round(p[2] * 100.0, 1)]


def point(c, t, key):
    r = c.row(t)
    p = r.get(key)
    if not p:
        sys.exit("lighthouse: the %s manifest has no %s (python3 blender/south/build_south.py)" % (t["kind"], key))
    x, y, z = c.local(t, p["x"], p["y"], p.get("z", 0.0))
    return x, y, z, (float(t.get("yaw", 0.0)) - float(p.get("yaw", 0.0))) % 360.0


def ahead(x, y, yaw, d):
    a = math.radians(yaw)
    return x + d * math.cos(a), y + d * math.sin(a)


def data():
    c = SP.Coast()
    towers = c.things("lighthouse")
    screens = c.things("meteo_screen")
    if not towers or not screens:
        sys.exit("lighthouse: south.json needs the lighthouse and its meteo_screen (python3 tools/south.py)")
    t, s = towers[0], screens[0]
    r = c.row(t)
    lamp = c.local(t, *r["lamp"])
    door = point(c, t, "door_stand")
    hatch = point(c, t, "hatch")
    read = point(c, s, "read_at")
    view = point(c, t, "gallery_view")
    # up: from the door step to the gallery, landing a step out from the hatch toward the rail; down: the reverse, landing a
    # step back from the door, facing out to the sea's side of the yard (away from the door)
    ux, uy = ahead(hatch[0], hatch[1], hatch[3], STEP_OFF)
    dx, dy = ahead(door[0], door[1], door[3], -STEP_OFF)
    return OD([
        ("_doc", "Written by tools/lighthouse/lighthouse.py from south.json and the manifests: Unreal cm and yaw (docs/lighthouse.md)."),
        ("lamp", cm(c, *lamp)),
        ("tower_yaw", round(float(t.get("yaw", 0.0)), 2)),
        ("stairs", [OD([("id", "up"), ("at", cm(c, *door[:3])), ("yaw", round(door[3], 1)), ("to", cm(c, ux, uy, hatch[2])),
                        ("to_yaw", round(hatch[3], 1)), ("reach", 110)]),
                    OD([("id", "down"), ("at", cm(c, *hatch[:3])), ("yaw", round(hatch[3], 1)), ("to", cm(c, dx, dy, door[2])),
                        ("to_yaw", round((door[3] + 180.0) % 360.0, 1)), ("reach", 90)])]),
        ("log", OD([("at", cm(c, *read[:3])), ("yaw", round(read[3], 1))])),
        # E on the gallery's sea side holds the binoculars' view (an ALyudSouthSpot "view" without a stand: the deck is too
        # narrow for the coin viewer's pedestal)
        ("view", OD([("at", cm(c, *view[:3])), ("yaw", round(view[3], 1))])),
        ("quest", QUEST),
        ("open_hours", [8, 21]),
    ])


def strings():
    out = {}
    for i, lang in enumerate(("ru", "en", "ja")):
        out[STRINGS % lang] = json.dumps(OD((k, v[i]) for k, v in W.S.items()), ensure_ascii=False, indent=1) + "\n"
    return out


def main():
    out = strings()
    out[OUT] = json.dumps(data(), ensure_ascii=False, indent=1) + "\n"
    stale = [p for p, txt in out.items() if not os.path.exists(p) or open(p, encoding="utf-8").read() != txt]
    if "--check" in sys.argv:
        for p in stale:
            print("lighthouse: %s is not up to date (python3 tools/lighthouse/lighthouse.py)" % os.path.relpath(p, ROOT))
        sys.exit(1 if stale else 0)
    for p in stale:
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "w", encoding="utf-8") as f:
            f.write(out[p])
        print("wrote", os.path.relpath(p, ROOT))
    print("lighthouse: %d words a language, lamp at %s" % (len(W.S), json.loads(out[OUT])["lamp"]))


if __name__ == "__main__":
    main()
