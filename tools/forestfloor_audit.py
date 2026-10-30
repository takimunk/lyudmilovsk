"""Checks a forest floor audit (AForestFloor::AuditMap, unreal/scripts/audit_forestfloor.sh) against the map's own
zones in tools/world_map.py, independently of the walk grid the floor is gated on: no piece may lie in the
Academgorodok or within 15 m of its ground, in the town's circle, on or within 15 m of a street, the path from the
club's door, the promenade, a cable-car station (its vestibule) or pylon, or outside the old and wet forest tiles.

    python3 tools/forestfloor_audit.py Saved/ForestFloorAudit/after.ffa [more.ffa ...]

Prints one line per file and the worst offenders; exits 1 if any piece breaks a rule.
"""
import json
import math
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import animal_map as AM  # noqa: E402
import world_map as W  # noqa: E402

KEEP = 15.0
PLACES = os.path.join(HERE, "..", "unreal", "Lyudmilovsk", "Content", "AnimalsData", "animal_places.json")


def load(path):
    with open(path, "rb") as f:
        data = f.read()
    assert data[:4] == b"FFA1", path
    nx, ny = struct.unpack_from("<ii", data, 4)
    x0, y0, cell = struct.unpack_from("<fff", data, 12)
    occ = data[24:24 + nx * ny]
    return nx, ny, x0, y0, cell, occ


def rules():
    """(name, test(x, y) -> True where a piece must not lie), map metres."""
    path = W.path_points(1.0)
    cc = W.cable_car()
    # what is built of a station: its tower or vestibule's podium (docs/transport.md), 7 m round its point without one
    stations = [(x, y, 7.0) for x, y in cc["stations"]] + [(v[0], v[1], v[2]) for v in cc.get("vestibules", [])]
    ring = [(r * math.cos(a * math.pi / 8), r * math.sin(a * math.pi / 8)) for r in (5.0, 10.0, KEEP - 0.5) for a in range(16)]
    towers = cc["towers"]
    boxes = AM.academ_boxes() + AM.town_boxes()
    tiles = [t for t in W.forests() if t[4] in ("old", "wet")]
    x0, x1, y0, y1 = W.ACADEM_RECT

    def academ(x, y):
        lx, ly = W.academ_local(x, y)
        return max(x0 - lx, lx - x1, y0 - ly, ly - y1) < KEEP

    def town(x, y):
        return math.hypot(x - W.TOWN_CENTER[0], y - W.TOWN_CENTER[1]) < W.TOWN_R + 30.0

    def street(x, y):
        return AM.on_street(x, y) or any(AM.on_street(x + dx, y + dy) for dx, dy in ring)

    def club_path(x, y):
        return -20.0 < y < W.PATH_LEN + KEEP + 2 and any(math.hypot(x - px, y - py) < KEEP + 1.8 for px, py in path)

    def forecourt(x, y):
        return abs(x - W.DOOR_X) < 6.0 + KEEP and -2.0 - KEEP < y < 7.0 + KEEP

    def promenade(x, y):
        return W.PROMENADE_Y[0] - KEEP < y < W.PROMENADE_Y[1] + KEEP and abs(W.inland(x, y) - (W.BEACH_W - 5.0)) < 3.0 + KEEP

    def station(x, y):
        return any(math.hypot(x - sx, y - sy) < r + KEEP for sx, sy, r in stations)

    def pylon(x, y):
        return any(math.hypot(x - tx, y - ty) < 2.0 + KEEP for tx, ty in towers)

    def building(x, y):
        return any(AM.in_box(x, y, b, KEEP) for b in boxes)

    def outside_woods(x, y):
        # the floor may run onto the steep edge of its woods (the walk grid's blocked slope cells up to 6 m off them)
        return not any(abs(x - tx) <= hx + 8.0 and abs(y - ty) <= hy + 8.0 for tx, ty, hx, hy, *_ in tiles)

    def beach(x, y):
        return W.inland(x, y) < W.BEACH_W

    return [("academgorodok", academ), ("town", town), ("street", street), ("club path", club_path),
            ("forecourt", forecourt), ("promenade", promenade), ("station", station), ("pylon", pylon),
            ("building", building), ("beach", beach), ("outside the woods", outside_woods)]


def main():
    face_y = json.load(open(PLACES))["face_y_cm"] / 100.0
    tests = rules()
    bad_total = 0
    for path in sys.argv[1:]:
        nx, ny, x0, y0, cell, occ = load(path)
        counts = {name: [0, 0] for name, _ in tests}
        worst = []
        cells = big = 0
        for j in range(ny):
            row = occ[j * nx:(j + 1) * nx]
            if not any(row):
                continue
            for i in range(nx):
                o = row[i]
                if not o:
                    continue
                cells += 1
                big += o == 2
                x, y = x0 + i * cell, y0 + j * cell - face_y
                for name, test in tests:
                    if test(x, y):
                        counts[name][0] += 1
                        counts[name][1] += o == 2
                        if len(worst) < 12:
                            worst.append((name, round(x, 1), round(y, 1), "big" if o == 2 else "small"))
        bad = sum(c[0] for c in counts.values())
        bad_total += bad
        print("%s: %d occupied 2 m cells (%d with a big piece); breaking a rule: %d %s" % (
            os.path.basename(path), cells, big, bad,
            ", ".join("%s %d (big %d)" % (n, c[0], c[1]) for n, c in counts.items() if c[0])))
        for w in worst:
            print("   %s at map (%.1f, %.1f) %s" % w)
    sys.exit(1 if bad_total else 0)


if __name__ == "__main__":
    main()
