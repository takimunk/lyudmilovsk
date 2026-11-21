#!/usr/bin/env python3
"""What the hero can do on the Aton coast besides sitting and looking (docs/south.md "Things to do", backlog item 39):
drink from the spring and the alley's drinking fountain with his hands, fill an empty bottle with spring water, read the
notice boards, the stele and the belvedere's view as pages, sit on the boats lying keel up at the brook's mouth.

    python3 tools/south_uses.py            write Content/South/south_uses.json
    python3 tools/south_uses.py --check    exit 1 when it is not as this script makes it

Reads Content/South/south.json (things, in map metres: x west, y north) and the meshes' manifests for sizes. The game
(ULyudSouthUses, Public/South/LyudSouthUses.h) spawns the spots at runtime when the level has the coast (any
ALyudSouthSpot placed by import_south.py), so nothing has to be imported in the editor. Heights are the built ground's
(tools/terrain.py ground_z, the same the things were placed on). Every spot's "yaw" is the Unreal yaw it faces (the
viewer's or the sitter's X); the page words are in Content/Localization/south_uses/strings_<lang>.json (and the boards'
own south.board.* keys, tools/south_signs.py).
"""
import json
import math
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import terrain as T     # noqa: E402

CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
SOUTH = os.path.join(CONTENT, "South", "south.json")
OUT = os.path.join(CONTENT, "South", "south_uses.json")
NORTH_MANIFEST = os.path.join(ROOT, "blender", "export", "north", "manifest.json")

# the boards' pages: the first page is the board's own words (what is painted on it), the rest are this feature's
BOARDS = {
    "info_board_1": {"page": "trail", "title": "south.board.trail.title",
                     "pages": [["south.board.trail.%d" % i for i in range(1, 6)], ["southuse.trail.p2"]]},
    "info_board_2": {"page": "reeds", "title": "south.board.reeds.title",
                     "pages": [["south.board.reeds.%d" % i for i in range(1, 5)], ["southuse.reeds.p2"], ["southuse.reeds.p3"]]},
    "town_stele_1": {"page": "stele", "title": "southuse.stele.title", "pages": [["southuse.stele.p1"], ["southuse.stele.p2"]]},
}
VIEW_PAGE = {"page": "view", "title": "southuse.view.title", "pages": [["southuse.view.p1"], ["southuse.view.p2"], ["southuse.view.p3"]]}
READ_AHEAD = 1.3        # where the reader stands in front of a board, m
DRINK_AHEAD = 0.9       # in front of the trough / the fountain
BOAT_SIDE = 0.70        # a boat's seat: on the edge of its upturned bottom, this far out from the keel line (m)
BOAT_SEAT_X = (-0.7, 0.7)
BOAT_SEAT_CM = 62.0     # the cushion (the hull's edge) over the ground there


def r2(v):
    return round(v, 2)


def ahead(t, d, side=0.0):
    a = math.radians(float(t.get("yaw", 0.0)))
    return (t["x"] + d * math.cos(a) - side * math.sin(a), t["y"] + d * math.sin(a) + side * math.cos(a))


def spot(sid, kind, xy, yaw, **more):
    x, y = xy
    s = {"id": sid, "kind": kind, "x": r2(x), "y": r2(y), "z": r2(T.ground_z(x, y)), "yaw": round(yaw % 360.0, 1)}
    s.update(more)
    return s


def build():
    data = json.load(open(SOUTH, encoding="utf-8"))
    things = data["things"]
    by_id = {t["id"]: t for t in things}
    uses = []
    # the spring and the drinking fountain on the alley: drink with the hands; the spring fills a bottle too
    for t in things:
        if t["kind"] == "spring_trough":
            uses.append(spot("spring_%s" % t["id"].split("_")[-1], "drink", ahead(t, DRINK_AHEAD), float(t["yaw"]) + 180.0,
                             water="spring", fill=1, reach=130))
        elif t["kind"] == "drinking_fountain":
            uses.append(spot("fountain_%s" % t["id"].split("_")[-1], "drink", ahead(t, DRINK_AHEAD), float(t["yaw"]) + 180.0,
                             water="fountain", fill=0, reach=110))
    # the boards and the stele: read as pages
    for tid, b in BOARDS.items():
        t = by_id.get(tid)
        if t is None:
            sys.exit("south_uses: %s is not in south.json" % tid)
        uses.append(spot("read_%s" % b["page"], "read", ahead(t, READ_AHEAD), float(t["yaw"]) + 180.0,
                         title=b["title"], pages=b["pages"], reach=170))
    # the belvedere: its view as a page, read standing at the rail beside the coin viewer
    bel = next(t for t in things if t["kind"] == "belvedere")
    uses.append(spot("read_view", "read", ahead(bel, 1.6, 1.2), float(bel["yaw"]), title=VIEW_PAGE["title"], pages=VIEW_PAGE["pages"],
                     reach=150, z_floor=0.25))
    # the boats keel up: two seats on the edge of each upturned bottom, facing the sea (+X) side
    rows = {r["kind"]: r for r in json.load(open(NORTH_MANIFEST, encoding="utf-8"))["meshes"]}
    beam = rows["rowboat"]["size"][1] / 2.0
    seats = []
    for t in things:
        if t["kind"] != "rowboat":
            continue
        # the mesh's X (its length) lies along the thing's yaw - 90 (import_north.spawn_mesh FRONT_YAW 90)
        along = math.radians(float(t["yaw"]) - 90.0)
        ax, ay = math.cos(along), math.sin(along)
        nx, ny = -ay, ax                     # across the boat; pick the side toward the sea
        if nx < 0:
            nx, ny = -nx, -ny
        face = math.degrees(math.atan2(ny, nx))
        for k, sx in enumerate(BOAT_SEAT_X):
            x = t["x"] + ax * sx + nx * min(BOAT_SIDE, beam - 0.05)
            y = t["y"] + ay * sx + ny * min(BOAT_SIDE, beam - 0.05)
            # the frame stands on the ground under the cushion's front edge (ALyudSouthSpot), the cushion BOAT_SEAT_CM up
            gz = T.ground_z(t["x"], t["y"])
            seats.append({"id": "boat_%s_%d" % (t["id"].split("_")[-1], k + 1), "x": r2(x + nx * 0.1), "y": r2(y + ny * 0.1),
                          "z": r2(gz), "yaw": round(face % 360.0, 1), "seat_cm": BOAT_SEAT_CM})
    return {"_doc": "Written by tools/south_uses.py (docs/south.md \"Things to do\"): spots the game spawns on the Aton coast. "
                    "Map metres (x west, y north, z up from the club's floor); yaw: the Unreal yaw the user faces.",
            "uses": uses, "seats": seats}


def main():
    text = json.dumps(build(), ensure_ascii=False, indent=1) + "\n"
    old = open(OUT, encoding="utf-8").read() if os.path.exists(OUT) else ""
    if "--check" in sys.argv:
        if old != text:
            print("south_uses: %s is not up to date (python3 tools/south_uses.py)" % os.path.relpath(OUT, ROOT))
            sys.exit(1)
        return
    if old != text:
        open(OUT, "w", encoding="utf-8").write(text)
        print("wrote", os.path.relpath(OUT, ROOT))
    d = json.loads(text)
    print("south uses: %d spots (%s), %d boat seats" % (len(d["uses"]), ", ".join(u["id"] for u in d["uses"]), len(d["seats"])))


if __name__ == "__main__":
    main()
