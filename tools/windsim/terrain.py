#!/usr/bin/env python3
"""The map's relief as the wind sees it, for tools/windsim (Private/Weather/WindField.h, FWindTerrain).

Samples tools/world_map.py's macro height on a 16 m grid over the whole map with a margin (the natural detail of
tools/terrain.py is metres, not what bends a wind), and a cover grid from what is known offline:
  - the forests (world_map.forests() tiles thinned by forest_cover(), a crown height per mix);
  - the town's plots that carry a building (city_plan.json: footprint w x d, height_max). The cover's height is the
    windbreak's: a thing narrower than it is tall has a lee as long as its width lets it, so a tower counts for its
    width; lattice and open things (the lighthouse's tower, canopies, kiosks, lifeguard towers) are left out.
Not in it: the Academgorodok's blocks, the wall dwellings (they are inside the terraces' walls, which are ground),
the pier's deck, single trees. The game builds its own grid from the landscape (ULyudWindSubsystem).

Writes Saved/Wind/terrain.bin, little endian:
  "LWT1", int32 NX, int32 NY, float64 X0, Y0 (world metres of cell (0,0)'s centre), float64 Cell,
  float64 MapY (world Y = map Y + MapY), float32 SeaZ, float32 Ground[NX*NY], float32 Cover[NX*NY]  (row = Y)
World metres = Unreal cm / 100: X as the map's, Y the map's + portal_face_y / 100.

  python3 tools/windsim/terrain.py [out.bin]
"""
import json
import math
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import world_map as W  # noqa: E402

CELL = 16.0
# the height function holds between the SHORE table's ends; a wind needs ~600 m of land past where anyone stands
# (a lee is 14 obstacle heights long), and no stored sea: outside the grid FWindTerrain is open sea anyway
X0, X1 = W.X_MIN - 600.0, W.X_MAX
Y0, Y1 = W.Y_MIN - 450.0, W.Y_MAX + 650.0
OPEN_BUILDINGS = ("hyperboloid_tower", "wave_canopy", "bus_stop_kiosk", "lifeguard_tower")
CROWN = {"old": 24.0, "gallery": 16.0, "wet": 15.0, "alder": 12.0, "town": 11.0, "dune": 9.0, "stonebirch": 8.0,
         "riverbar": 6.0, "dwarfpine": 2.5, "shrub": 2.0, "rhodo": 1.5}


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "Saved", "Wind", "terrain.bin")
    plan_path = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "City", "city_plan.json")
    plan = json.load(open(plan_path)) if os.path.exists(plan_path) else {}
    map_y = float(plan.get("portal_face_y", 790.0)) / 100.0
    nx = int(round((X1 - X0) / CELL)) + 1
    ny = int(round((Y1 - Y0) / CELL)) + 1
    ground = [0.0] * (nx * ny)
    for j in range(ny):
        y = Y0 + j * CELL
        for i in range(nx):
            ground[j * nx + i] = W.height(X0 + i * CELL, y)
    cover = [0.0] * (nx * ny)

    def g(i, j):
        return ground[min(max(j, 0), ny - 1) * nx + min(max(i, 0), nx - 1)]

    # forests: a tile says which wood may stand where; forest_cover() thins it; no trees on a face steeper than 1:1
    woods = 0
    for x, y, hx, hy, mix, count, spacing, z0, z1, _seed in W.forests():
        crown = CROWN.get(mix, 12.0)
        density = min(1.0, count / max(4.0 * hx * hy / spacing ** 2 * 0.8, 1.0))
        for j in range(max(0, int(math.ceil((y - hy - Y0) / CELL))), min(ny - 1, int(math.floor((y + hy - Y0) / CELL))) + 1):
            for i in range(max(0, int(math.ceil((x - hx - X0) / CELL))), min(nx - 1, int(math.floor((x + hx - X0) / CELL))) + 1):
                z = ground[j * nx + i]
                if not z0 <= z <= z1:
                    continue
                if math.hypot(g(i + 1, j) - g(i - 1, j), g(i, j + 1) - g(i, j - 1)) / (2 * CELL) > 1.0:
                    continue
                share = W.forest_cover(X0 + i * CELL, Y0 + j * CELL)
                h = crown * (0.6 + 0.4 * density) * min(1.0, share * 1.3)
                if h > cover[j * nx + i]:
                    if cover[j * nx + i] == 0.0:
                        woods += 1
                    cover[j * nx + i] = h
    # the town's buildings: the share of each cell under the footprint (4 x 4 points a cell)
    houses = 0
    for p in plan.get("plots", []):
        h = float(p.get("height_max") or 0.0)
        if not p.get("building") or p["building"] in OPEN_BUILDINGS or h <= 0.0 or "map" not in p:
            continue
        h = min(h, max(p.get("w", 0.0), p.get("d", 0.0)))
        cx, cy = p["map"][0], p["map"][1]
        yaw = math.radians(p.get("yaw", 0.0))
        fx, fy = math.cos(yaw), math.sin(yaw)             # the way the front faces; w runs across it, d along it
        hw, hd = p.get("w", 0.0) / 2.0, p.get("d", 0.0) / 2.0
        r = math.hypot(hw, hd) + CELL
        hit = False
        for j in range(max(0, int((cy - r - Y0) / CELL)), min(ny - 1, int((cy + r - Y0) / CELL) + 1) + 1):
            for i in range(max(0, int((cx - r - X0) / CELL)), min(nx - 1, int((cx + r - X0) / CELL) + 1) + 1):
                n = 0
                for a in range(4):
                    for b in range(4):
                        dx = X0 + (i - 0.5 + (a + 0.5) / 4.0) * CELL - cx
                        dy = Y0 + (j - 0.5 + (b + 0.5) / 4.0) * CELL - cy
                        if abs(dx * fx + dy * fy) <= hd and abs(-dx * fy + dy * fx) <= hw:
                            n += 1
                if n:
                    cover[j * nx + i] = max(cover[j * nx + i], h * min(1.0, 2.0 * n / 16.0))
                    hit = True
        houses += 1 if hit else 0
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "wb") as f:
        f.write(b"LWT1")
        f.write(struct.pack("<iidddd f", nx, ny, X0, Y0 + map_y, CELL, map_y, W.SEA_Z))
        f.write(struct.pack("<%df" % (nx * ny), *ground))
        f.write(struct.pack("<%df" % (nx * ny), *cover))
    print("wind terrain: %d x %d cells of %g m, map X %g..%g Y %g..%g, ground %.1f..%.1f m, %d wooded cells, %d buildings -> %s"
          % (nx, ny, CELL, X0, X1, Y0, Y1, min(ground), max(ground), woods, houses, os.path.relpath(out, ROOT)))


if __name__ == "__main__":
    main()
