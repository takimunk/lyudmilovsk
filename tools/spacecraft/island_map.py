"""The island seen from the descent craft (docs/spacecraft.md): one small map baked from the game's real land, so that
from orbit down to ~2 km the island in the porthole IS Lyudmilovsk.

    python3 tools/spacecraft/island_map.py [--size 2048] [--check]
        -> textures/spacecraft/T_SC_island.png     RGB colour (sRGB), A = how much of it lies over the planet's own sea
                                                   (1 on the land and its shelf, fading out into the deep-sea colour)
           textures/spacecraft/T_SC_island_M.png   linear: R the night's lights, G land (no glint, no swell on it),
                                                   B shallow water (the shelf and the tidal flat: a paler glint)
           tools/spacecraft/textures.json "island" its extent in km, what map point its centre is, the town's place in
                                                   it, the latitude and longitude of its centre (the town at the planet
                                                   texture's island: tools/europa/europa_textures.py ISLAND_LAT/LON)

What is real: everything inside the walked map (tools/world_map.py: X -1300..1300, Y -900..4200 m): the coast, the
beach, the clay cliff, the tidal flat with its bars and channels, the town's terraces along the beach with every plot,
dwelling and street lamp of tools/city_plan.py, the forests (tiles and cover), the North's cones, cinder, lava and snow
(tools/north.py), the sea railway's island 2.3 km off (tools/railway.py), and the three big volcanoes that stand on the
skyline (north.far_volcanoes) where they stand. What is made up: the rest of the island's outline and its wooded
inland, grown round those (the game has no map of them: nobody walks there). North is up, west (map +X, the sea) on the
left, as on the game's map. Pure numpy + PIL + scipy; the seed and every number are here.
"""
import hashlib
import json
import math
import os
import sys
from multiprocessing import Pool

import numpy as np
from PIL import Image
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(REPO, "textures", "spacecraft")
PINS = os.path.join(HERE, "textures.json")
sys.path.insert(0, os.path.join(REPO, "tools"))
sys.path.insert(0, os.path.join(REPO, "tools", "europa"))
import world_map as W  # noqa: E402
import city_plan as C  # noqa: E402
import north as N  # noqa: E402
import railway as RW  # noqa: E402
import europa_textures as ET  # noqa: E402

SEED = 7152
EXTENT_M = 25600.0                 # the texture's side on the ground
CENTRE = (-800.0, 7000.0)          # the map point at its middle (X west, Y north)
G = 2048                           # the working grid (12.5 m); the output is reduced from it
# the made-up outline: a superellipse round the real coast and the volcanoes' feet (map metres)
BLOB_C, BLOB_A, BLOB_B, BLOB_P = (-2200.0, 5600.0), 4000.0, 8300.0, 2.6
DEEP = (0.035, 0.11, 0.24)         # the planet texture's ocean (europa_textures.planet), where the map fades to


def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def grid_xy():
    px = EXTENT_M / G
    i = (np.arange(G) + 0.5) * px
    X = CENTRE[0] + EXTENT_M / 2 - i            # west (+X) on the left
    Y = CENTRE[1] + EXTENT_M / 2 - i            # north up
    return np.meshgrid(X, Y)


_TILES = None


def _tiles():
    global _TILES
    if _TILES is None:
        grid = {}
        for t in W.forests():
            x, y, hx, hy = t[0], t[1], t[2], t[3]
            for gx in range(int((x - hx) // 80.0), int((x + hx) // 80.0) + 1):
                for gy in range(int((y - hy) // 80.0), int((y + hy) // 80.0) + 1):
                    grid.setdefault((gx, gy), []).append(t)
        _TILES = grid
    return _TILES


WOOD_CODE = {"old": 1.0, "dune": 0.55, "wet": 0.7, "town": 0.35, "stonebirch": 0.6, "dwarfpine": 0.8, "alder": 0.65,
             "shrub": 0.4, "rhodo": 0.3, "gallery": 0.6, "riverbar": 0.35}


def _real_row(args):
    """One row of the walked map: (z, cliff, wood density, lava, snow, cinder, terrace index, town mask) per sample."""
    y, xs = args
    out = np.zeros((len(xs), 8), np.float32)
    tiles = _tiles()
    cm = C.city_mask(y)
    for k, x in enumerate(xs):
        z = W.height(x, y)
        out[k, 0] = z
        if z < W.SEA_Z:
            continue
        out[k, 1] = W.cliff(x, y)
        wood = 0.0
        for t in tiles.get((int(x // 80.0), int(y // 80.0)), ()):
            if abs(x - t[0]) <= t[2] and abs(y - t[1]) <= t[3] and t[7] <= z <= t[8]:
                wood = max(wood, WOOD_CODE.get(t[4], 0.5))
        if wood > 0.0:
            wood *= W.forest_cover(x, y)
        out[k, 2] = wood
        if y > 2600.0:
            out[k, 3] = N.lava(x, y)
            out[k, 4] = N.snow(x, y)
            out[k, 5] = N.scoria(x, y)
        ti = C.terrace_at(x, y)
        out[k, 6] = ti
        out[k, 7] = cm if (ti >= 0 and W.inland(x, y) < C.INLAND_END) else 0.0
    return out


def real_fields(X, Y):
    """The walked map on the grid: a dict of arrays and the mask of where they hold."""
    cols = np.where((X[0] >= W.X_MIN) & (X[0] <= 2600.0))[0]
    rows = np.where((Y[:, 0] >= W.Y_MIN) & (Y[:, 0] <= W.Y_MAX))[0]
    xs = X[0, cols]
    with Pool() as pool:
        res = pool.map(_real_row, [(float(Y[r, 0]), [float(v) for v in xs]) for r in rows], chunksize=8)
    F = np.zeros((G, G, 8), np.float32)
    F[..., 0] = -1.0e3
    F[np.ix_(rows, cols)] = np.stack(res, 0)
    inside = np.zeros((G, G), bool)
    inside[np.ix_(rows, cols)] = True
    return F, inside


def fbm(n, beta, seed):
    return ET.periodic_noise(n, beta, seed)


def bake():
    X, Y = grid_xy()
    px = EXTENT_M / G
    F, inside = real_fields(X, Y)
    rng = np.random.default_rng(SEED)
    n1 = fbm(G, 1.8, SEED + 1)             # big shapes
    n2 = fbm(G, 1.3, SEED + 2)             # hills
    n3 = fbm(G, 0.8, SEED + 3)             # grain
    vols = N.far_volcanoes()

    # --- the outline ---------------------------------------------------------------------------------------------
    tx, ty = (X - BLOB_C[0]) / BLOB_A, (Y - BLOB_C[1]) / BLOB_B
    r = (np.abs(tx) ** BLOB_P + np.abs(ty) ** BLOB_P) ** (1.0 / BLOB_P)
    east = smooth(600.0, -1500.0, X)                               # ragged only away from the real coast
    s = (1.0 - r) * 3200.0 + (n1 - 0.5) * 2600.0 * east + (n2 - 0.5) * 500.0 * east
    for v in vols:
        d = np.hypot(X - v["x"], Y - v["y"])
        foot = v["radius"] * (0.92 if v["base"] > 0 else 0.55)     # the one that stands in the sea is its own islet
        sv = foot - d + (n1 - 0.5) * 900.0 + (n2 - 0.5) * 300.0
        k = 400.0
        s = np.where(np.abs(s - sv) < 4 * k, k * np.log(np.exp(np.clip(s / k, -50, 50)) + np.exp(np.clip(sv / k, -50, 50))), np.maximum(s, sv))
    # the real west coast wherever the map knows it (and a little beyond its ends, easing off to the made-up one)
    yc = np.clip(Y[:, 0], W.Y_MIN, W.Y_MAX)
    shore = np.array([W.shore_x(float(v)) for v in yc], np.float32)
    past = np.maximum(W.Y_MIN - Y[:, 0], Y[:, 0] - W.Y_MAX)
    shore = shore + np.clip(past, 0, None) ** 1.5 * 0.05
    sw = shore[:, None] - X
    k = 150.0
    s = -k * np.log(np.exp(np.clip(-s / k, -60, 60)) + np.exp(np.clip(-sw / k, -60, 60)))
    land = s > 0.0
    # inside the walked map the heights say what is land (the river's mouth, the brook, the pier's water)
    real_land = F[..., 0] >= W.SEA_Z
    land = np.where(inside, real_land, land)
    # the sea railway's island
    isl = RW.load()["island"]
    ia = np.arctan2((Y - isl["y"]) / isl["ry"], (X - isl["x"]) / isl["rx"])
    kk = isl.get("seed", 1958)
    rr = 1.0 + 0.10 * np.sin(3 * ia + kk * 0.37) + 0.06 * np.sin(5 * ia + kk * 0.11) + 0.04 * np.sin(9 * ia + 1.3)
    it = np.hypot((X - isl["x"]) / isl["rx"], (Y - isl["y"]) / isl["ry"]) / rr
    land = land | (it < 1.0)

    d_in = ndimage.distance_transform_edt(land) * px               # metres inland
    d_out = ndimage.distance_transform_edt(~land) * px             # metres out to sea

    # --- the made-up land: heights, then cover -------------------------------------------------------------------
    z = np.minimum(d_in * 0.12, 70.0) * (0.5 + n1) + 140.0 * smooth(300.0, 2500.0, d_in) * (n2 - 0.35) * (0.6 + n1)
    z = np.maximum(z, 1.0)
    snowline = np.zeros((G, G), np.float32) + 1.0e6
    crater = np.zeros((G, G), np.float32)
    gully = np.zeros((G, G), np.float32)
    ang1 = ET.periodic_noise(64, 1.2, SEED + 9)[0]                 # ravines round a cone: a 1D noise by bearing
    ang1 = np.interp(np.linspace(0, 64, 4096, endpoint=False), np.arange(65), np.append(ang1, ang1[0]))
    for vi, v in enumerate(vols):
        d = np.hypot(X - v["x"], Y - v["y"])
        cone = v["height"] * np.clip(1.0 - d / v["radius"], 0, 1) ** 1.7
        cone = np.minimum(cone, v["height"] * (0.93 + 0.5 * np.clip(d / v["crater"], 0, 1) ** 2 * 0.14))   # the crater
        bearing = np.arctan2(Y - v["y"], X - v["x"]) / (2 * math.pi) + 0.5 + 0.035 * (n2 - 0.5) + 0.01 * (n3 - 0.5)
        rav = ang1[(bearing * 4096 * 3 + vi * 977).astype(np.int64) % 4096] * 0.6 + ang1[(bearing * 4096 * 11 + vi * 131).astype(np.int64) % 4096] * 0.4
        rav = (rav - 0.5) * smooth(0.0, 0.35, d / v["radius"]) * smooth(1.0, 0.75, d / v["radius"])
        here = d < v["radius"]
        z = np.maximum(z, v["base"] + cone * (1.0 + 0.16 * rav))
        gully = np.where(here, rav, gully)
        snowline = np.where(here, np.minimum(snowline, v["snow"]), snowline)
        crater = np.maximum(crater, smooth(v["crater"] * 1.6, v["crater"] * 0.5, d) * (0.5 + 0.5 * v["plume"]))
    zs = z - W.SEA_Z * 0                                            # (the made-up heights are over the sea already)
    slope = np.hypot(*np.gradient(ndimage.gaussian_filter(zs, 1.0), px))
    forest = np.array((0.085, 0.17, 0.075), np.float32)
    birch = np.array((0.17, 0.27, 0.10), np.float32)
    meadow = np.array((0.27, 0.36, 0.15), np.float32)
    tundra = np.array((0.30, 0.29, 0.19), np.float32)
    rock = np.array((0.23, 0.20, 0.18), np.float32)
    cinder = np.array((0.16, 0.12, 0.11), np.float32)
    lava_c = np.array((0.07, 0.065, 0.065), np.float32)
    snow_c = np.array((0.90, 0.92, 0.95), np.float32)
    sand = np.array((0.78, 0.70, 0.50), np.float32)
    clay = np.array((0.60, 0.47, 0.33), np.float32)

    def mix(a, b, t):
        return a * (1 - t[..., None]) + b * t[..., None]

    glade = smooth(0.50, 0.66, n2 * 0.55 + n3 * 0.45) * smooth(3500.0, 600.0, d_in + 2500.0 * (n1 - 0.5))
    col = np.zeros((G, G, 3), np.float32) + forest
    col = col * (0.82 + 0.36 * n3[..., None])
    col = mix(col, meadow * (0.8 + 0.2 * n3[..., None]), glade * 0.85)
    col = mix(col, birch, smooth(250.0, 450.0, zs) * 0.8)
    col = mix(col, tundra, smooth(480.0, 700.0, zs + 150.0 * (n2 - 0.5)))
    col = mix(col, rock * (0.8 + 0.4 * n3[..., None]), smooth(750.0, 1000.0, zs + 200.0 * (n2 - 0.5)))
    col = mix(col, cinder, crater)
    snowy = smooth(-150.0, 250.0, zs - snowline - 350.0 + 700.0 * gully + 500.0 * (n2 - 0.5) + 300.0 * (n3 - 0.5))
    col = mix(col, snow_c, snowy)
    col = mix(col, sand, smooth(45.0, 20.0, d_in) * (1 - smooth(0.2, 0.5, slope)))          # a beach all the way round
    col = col * (1.0 - 0.25 * smooth(0.25, 0.9, slope)[..., None] * (1 - snowy[..., None]))  # steep ground a little darker

    # --- the walked map over it ----------------------------------------------------------------------------------
    rz = F[..., 0]
    up = np.clip((rz - W.SEA_Z) / 80.0, 0, 1)
    rcol = mix(np.zeros((G, G, 3), np.float32) + meadow * 0.86, np.array((0.27, 0.32, 0.15), np.float32), up)
    rcol = rcol * (0.85 + 0.25 * n3[..., None])
    wood = ndimage.gaussian_filter(F[..., 2], 1.2)
    wood = np.clip(wood + 0.5 * smooth(-20.0, 10.0, rz) * (1 - F[..., 7]) * smooth(0.35, 0.6, n3) * (Y < 2600.0), 0, 1)     # the upland's old forest
    rcol = mix(rcol, forest * (0.85 + 0.3 * n3[..., None]), np.clip(wood * 1.15, 0, 1))
    rcol = mix(rcol, cinder * (0.9 + 0.5 * n3[..., None]), F[..., 5] * 0.9)
    rcol = mix(rcol, lava_c, F[..., 3])
    rcol = mix(rcol, snow_c, F[..., 4])
    # the town: pale paved terraces stepping up from the embankment, a darker line at each wall, roofs on the plots
    town = np.clip(ndimage.gaussian_filter(F[..., 7], 2.0) * (0.7 + 0.6 * n3), 0, 1) * (F[..., 0] >= W.SEA_Z)
    terr = F[..., 6]
    paving = np.array((0.47, 0.45, 0.40), np.float32)
    tcol = paving[None, None, :] * (0.86 + 0.05 * terr[..., None]) * (0.92 + 0.16 * n3[..., None])
    tcol = mix(tcol, np.array((0.20, 0.30, 0.13), np.float32), 0.55 * smooth(0.35, 0.6, n3))             # street trees, gardens
    wall_line = (ndimage.maximum_filter(terr, 3) != ndimage.minimum_filter(terr, 3)) & (terr >= 0)
    tcol = mix(tcol, np.array((0.36, 0.34, 0.31), np.float32), wall_line.astype(np.float32) * 0.8)
    rcol = mix(rcol, tcol, town)
    rcol = mix(rcol, clay * (0.85 + 0.3 * n3[..., None]), F[..., 1])
    rs = np.array([W.shore_x(float(v)) for v in np.clip(Y[:, 0], -1400, 4900)], np.float32)[:, None] - X      # inland, m
    rcol = mix(rcol, sand, ((rs <= W.BEACH_W + 4.0) & (rz >= W.SEA_Z)).astype(np.float32))
    # roofs and yards: the plots, the Academgorodok, the stations, the hut
    roofs = np.zeros((G, G), np.float32)

    def stamp(arr, x, y, v, r=0.0):
        i, j = (CENTRE[0] + EXTENT_M / 2 - x) / px, (CENTRE[1] + EXTENT_M / 2 - y) / px
        rad = max(1, int(math.ceil(r / px)))
        for dj in range(-rad, rad + 1):
            for di in range(-rad, rad + 1):
                jj, ii = int(j) + dj, int(i) + di
                if 0 <= jj < G and 0 <= ii < G and math.hypot((ii + 0.5 - i), (jj + 0.5 - j)) * px <= max(r, px * 0.71):
                    arr[jj, ii] = max(arr[jj, ii], v)

    for p in C.plots():
        stamp(roofs, p["map"][0], p["map"][1], 1.0, min(p["w"], p["d"]) * 0.3)
    ax, ay, _ = W.academ()
    for dx, dy in ((0, 0), (40, 30), (-40, 30), (0, -45)):
        stamp(roofs, ax + dx, ay + dy, 0.9, 22.0)
    for sx, sy in W.cable_car()["stations"]:
        stamp(roofs, sx, sy, 0.7, 8.0)
    stamp(roofs, N.HUT[0], N.HUT[1], 0.6, 5.0)
    roofs = ndimage.gaussian_filter(roofs, 0.5)
    rcol = mix(rcol, np.array((0.50, 0.27, 0.20), np.float32), np.clip(roofs * 1.2, 0, 1) * 0.7)
    # blend the walked map into the made-up land over its inland and far edges
    soft = 450.0 + 500.0 * (n2 - 0.5)
    edge = np.minimum.reduce([smooth(W.X_MIN, W.X_MIN + soft, X), smooth(W.Y_MIN, W.Y_MIN + soft, Y), smooth(W.Y_MAX, W.Y_MAX - soft, Y)])
    wreal = edge * inside * real_land
    col = mix(col, rcol, wreal)
    # the relief, lit a little from the south-west so the cones and the valley read from above (the planet's own sun
    # only knows the sphere): the heights of both parts together
    hh = ndimage.gaussian_filter(zs * (1 - wreal) + np.clip(rz - W.SEA_Z, 0, None) * wreal, 1.0)
    gy, gx = np.gradient(hh, px)
    shade = np.clip(1.0 + 0.9 * (-gx * 0.6 + gy * 0.6), 0.55, 1.35)             # columns run east, rows run south
    col = col * (1 + (shade[..., None] - 1) * (0.75 * (1 - wreal[..., None] * town[..., None])))
    # the railway island: sand and turf, a rocky west end
    col = mix(col, mix(np.zeros((G, G, 3), np.float32) + sand, meadow, smooth(0.9, 0.5, it)), (it < 1.0).astype(np.float32))

    # --- the sea: the real bed where the map has it, a shelf made the same way elsewhere --------------------------
    depth_syn = 0.3 + np.minimum(d_out, 350.0) / 80.0 + np.clip(d_out - 350.0, 0, None) * 0.03
    depth_syn = np.where(depth_syn > 9.3, 9.3 + (d_out - 500.0).clip(0) * 0.012, depth_syn)
    depth_real = np.clip(W.SEA_Z - rz, 0.0, None)
    wr = (inside & ~real_land).astype(np.float32) * np.minimum(smooth(W.Y_MIN, W.Y_MIN + 260.0, Y), smooth(W.Y_MAX, W.Y_MAX - 260.0, Y)) \
        * smooth(2600.0, 2200.0, X)
    depth = depth_syn * (1 - wr) + np.minimum(depth_real, 60.0) * wr
    depth = depth * (0.9 + 0.2 * n3)
    # sand under clear water: pale at the edge, turquoise over the flat, teal on the shelf, then the open sea's blue
    wet_sand = np.array((0.62, 0.60, 0.44), np.float32)
    turq = np.array((0.13, 0.46, 0.47), np.float32)
    teal = np.array((0.06, 0.25, 0.36), np.float32)
    deep = np.array(DEEP, np.float32)
    sea = mix(np.zeros((G, G, 3), np.float32) + wet_sand, turq, 1 - np.exp(-depth / 1.6))
    sea = mix(sea, teal, 1 - np.exp(-np.clip(depth - 3.0, 0, None) / 6.0))
    sea = mix(sea, deep, smooth(9.0, 45.0, depth))
    # a line of surf on the open coast
    surf = np.exp(-(d_out / 18.0) ** 2) * smooth(0.35, 0.7, n3)
    sea = mix(sea, np.array((0.85, 0.88, 0.88), np.float32), surf * 0.5)
    landf = ndimage.gaussian_filter(land.astype(np.float32), 0.6)
    col = mix(sea, col, landf)

    alpha = smooth(3200.0, 1100.0, d_out)
    bx = np.minimum(np.minimum(np.arange(G), G - 1 - np.arange(G))[None, :], np.minimum(np.arange(G), G - 1 - np.arange(G))[:, None]) * px
    alpha = np.maximum(alpha, landf) * smooth(0.0, 500.0, bx)
    if (alpha[0].max() > 0) or (alpha[:, 0].max() > 0) or (landf * (bx < 400.0)).max() > 0.01:
        raise SystemExit("the island touches the texture's edge: move CENTRE or grow EXTENT_M")

    # --- the night's lights --------------------------------------------------------------------------------------
    L = np.zeros((G, G), np.float32)

    def light(x, y, v):
        i, j = int((CENTRE[0] + EXTENT_M / 2 - x) / px), int((CENTRE[1] + EXTENT_M / 2 - y) / px)
        if 0 <= i < G and 0 <= j < G:
            L[j, i] += v

    for lp in C.lamps():
        light(lp["foot"][0], lp["foot"][1], 0.5)
    for p in C.plots():
        light(p["map"][0], p["map"][1], 1.2)
    for dw in C.wall_dwellings():
        light(dw["door"][0], dw["door"][1], 0.12 + 0.2 * rng.random())
    for yy in np.arange(C.CITY_Y0 if hasattr(C, "CITY_Y0") else 450.0, 1250.0, 30.0):       # the promenade's lamps
        light(W.shore_x(float(yy)) - W.BEACH_W - 4.0, float(yy), 0.45)
    for dx, dy in ((0, 0), (40, 30), (-40, 30), (0, -45), (60, -20)):
        light(ax + dx, ay + dy, 1.0)
    for sx, sy in W.cable_car()["stations"]:
        light(sx, sy, 0.8)
    light(0.0, 0.0, 0.5)                               # the club's door
    light(N.HUT[0], N.HUT[1], 0.35)
    light(N.CAMP[0], N.CAMP[1], 0.3)
    for st in RW.load()["stops"]:
        light(st["x"], isl["y"] if st["id"] == "island" else W.shore_x(isl["y"]) * 0 + isl["y"], 0.5)
    pier = next((p for p in C.plots() if p["kind"] == "pier"), None)
    if pier:
        light(pier["map"][0], pier["map"][1], 3.0)     # the lighthouse
    lights = ndimage.gaussian_filter(L, 0.9) * 5.0 + ndimage.gaussian_filter(L, 3.5) * 14.0   # the lamps and their glow
    lights = np.clip(lights, 0, 1)
    shallow = (1 - landf) * smooth(16.0, 2.0, depth)
    return col, alpha, lights, landf, shallow


def reduce(a, size):
    f = a.shape[0] // size
    if f == 1:
        return a
    sh = (size, f, size, f) + a.shape[2:]
    return a.reshape(sh).mean(axis=(1, 3))


def write(name, arr):
    img = (np.clip(arr, 0, 1) * 255 + 0.5).astype(np.uint8)
    Image.fromarray(img).save(os.path.join(OUT, name))
    print("wrote", name, img.shape)
    return {"sha1": hashlib.sha1(img.tobytes()).hexdigest(), "size": [img.shape[1], img.shape[0]], "channels": int(img.shape[2])}


def main():
    size = int(sys.argv[sys.argv.index("--size") + 1]) if "--size" in sys.argv else 1024
    os.makedirs(OUT, exist_ok=True)
    col, alpha, lights, landf, shallow = bake()
    # the lights keep their sparkle when reduced: the brightest of each block, not the mean
    f = G // size
    lmax = lights.reshape(size, f, size, f).max(axis=(1, 3)) * 0.6 + reduce(lights, size) * 0.4
    a = write("T_SC_island.png", np.concatenate([reduce(col, size), reduce(alpha, size)[..., None]], -1))
    m = write("T_SC_island_M.png", np.stack([lmax, reduce(landf, size), reduce(shallow, size)], -1))
    a["gpu_bytes"] = int(size * size * 4 / 3)
    m["gpu_bytes"] = int(size * size * 0.5 * 4 / 3)
    pins = {}
    if os.path.isfile(PINS):
        with open(PINS) as fh:
            pins = json.load(fh)
    tex = pins.setdefault("textures", {})
    if "--check" in sys.argv:
        bad = [k for k, v in (("T_SC_island.png", a), ("T_SC_island_M.png", m)) if tex.get(k, {}).get("sha1") not in (None, v["sha1"])]
        if bad:
            raise SystemExit("changed against textures.json: %s" % ", ".join(bad))
    tex["T_SC_island.png"], tex["T_SC_island_M.png"] = a, m
    tx, ty = W.TOWN_CENTER
    lat0 = ET.ISLAND_LAT + (CENTRE[1] - ty) / 111320.0
    lon0 = ET.ISLAND_LON + (tx - CENTRE[0]) / (111320.0 * math.cos(math.radians(ET.ISLAND_LAT)))
    pins["island"] = {
        "seed": SEED, "extent_km": EXTENT_M / 1000.0, "metres_per_texel": EXTENT_M / size,
        "centre_map_m": list(CENTRE), "north_up": True, "west_left": True,
        "town_map_m": [tx, ty],
        "town_uv": [round((CENTRE[0] + EXTENT_M / 2 - tx) / EXTENT_M, 5), round((CENTRE[1] + EXTENT_M / 2 - ty) / EXTENT_M, 5)],
        "club_uv": [round((CENTRE[0] + EXTENT_M / 2) / EXTENT_M, 5), round((CENTRE[1] + EXTENT_M / 2) / EXTENT_M, 5)],
        "town_lat_lon": [ET.ISLAND_LAT, ET.ISLAND_LON],
        "centre_lat_lon_true_scale": [round(lat0, 5), round(lon0, 5)],
        "planet_texture_island": {"lat": ET.ISLAND_LAT, "lon": ET.ISLAND_LON, "size_deg": ET.ISLAND_SIZE},
        "real_part": {"x": [W.X_MIN, W.X_MAX], "y": [W.Y_MIN, W.Y_MAX]},
        "about": "uv = 0.5 + (east, -north) / extent from the centre; map X is west, so east = -(X - centre X). "
                 "M_SC_PlanetNear puts the texture's centre at IslandLat/IslandLon and scales it by IslandSizeKm "
                 "against PlanetRadiusKm; see tools/spacecraft/MATERIAL_NOTES.md.",
    }
    pins["gpu_bytes_total"] = sum(v.get("gpu_bytes", 0) for v in tex.values())
    pins["textures"] = dict(sorted(tex.items()))
    with open(PINS, "w") as fh:
        json.dump(pins, fh, indent=1, sort_keys=True)
        fh.write("\n")
    print("island: %.1f km, %.1f m a texel; %.2f MB of textures in all" % (EXTENT_M / 1000.0, EXTENT_M / size, pins["gpu_bytes_total"] / 1048576.0))


if __name__ == "__main__":
    main()
