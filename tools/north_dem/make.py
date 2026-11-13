#!/usr/bin/env python3
"""The North's relief comes from a real place: the cinder cones of the Northern Breakthrough of the Great Tolbachik
Fissure Eruption (1975, Kamchatka, 55.68 N 160.24 E) and the lava plain west of them.

This fetches the public elevation tiles (Mapzen / AWS Terrain Tiles, "terrarium" PNGs; over Kamchatka the data is
NASA SRTM, public domain), mends the radar's voids, takes the window the map uses, scales it (SCALE real metres per
map metre, heights by the same factor so slopes stay true), takes out part of the regional tilt and writes

    tools/north_dem/dem.i16    little-endian int16 decimetres over the map's zero, rows south to north, west... see dem.json
    tools/north_dem/dem.json   the grid's frame and where it came from

tools/north.py reads those two with the standard library only. Needs numpy and Pillow (a tool, not a build step):

    python3 tools/north_dem/make.py [preview.png]
"""
import json
import math
import os
import subprocess
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
CACHE = os.path.join(HERE, ".tiles")
ZOOM = 13
LAT, LON = 55.675, 160.245          # the middle of the fetched area
HALF_KM = 5.5
SCALE = 2.0                         # real metres per map metre
# the window in map metres and where the fetched area's middle falls in it
X_RANGE = (-1500.0, 1300.0)         # 200 m past the map's east edge, for the far land's start
Y_RANGE = (2100.0, 4400.0)
MID_AT = (-650.0, 2750.0)           # map (X, Y) of LAT, LON
MEND_TOL = 7.0                      # real metres off the local median that mark a void
# cones cut by the window's south edge are taken off the stage: (map X, Y, radius)
ERASE = [(-500.0, 2190.0, 330.0)]
STEP = 10.0                         # map metres per cell
TILT = (-0.086, 0.018)              # the region's own tilt taken out (m per map m along X and Y): Tolbachik's flank
                                    # climbs 12 % inland, the map keeps 2 %
BASE = (0.0, 3000.0, 42.0)        # the height the land has at this map point after everything


def tile_xy(lat, lon, z):
    n = 2 ** z
    return (lon + 180) / 360 * n, (1 - math.asinh(math.tan(math.radians(lat))) / math.pi) / 2 * n


def tile(z, x, y):
    os.makedirs(CACHE, exist_ok=True)
    p = os.path.join(CACHE, "%d_%d_%d.png" % (z, x, y))
    if not os.path.exists(p):
        subprocess.run(["curl", "-sS", "-f", "-o", p,
                        "https://s3.amazonaws.com/elevation-tiles-prod/terrarium/%d/%d/%d.png" % (z, x, y)], check=True)
    a = np.asarray(Image.open(p).convert("RGB")).astype(np.float64)
    return a[..., 0] * 256 + a[..., 1] + a[..., 2] / 256 - 32768


def fetch():
    """(heights, metres per pixel), north up, the middle at LAT, LON."""
    cx, cy = tile_xy(LAT, LON, ZOOM)
    mpp = 40075016.686 * math.cos(math.radians(LAT)) / 2 ** ZOOM / 256
    hp = HALF_KM * 1000 / mpp
    x0, x1 = int((cx * 256 - hp) // 256), int((cx * 256 + hp) // 256)
    y0, y1 = int((cy * 256 - hp) // 256), int((cy * 256 + hp) // 256)
    big = np.vstack([np.hstack([tile(ZOOM, x, y) for x in range(x0, x1 + 1)]) for y in range(y0, y1 + 1)])
    px, py, h = int(cx * 256 - x0 * 256), int(cy * 256 - y0 * 256), int(hp)
    return big[py - h:py + h, px - h:px + h], mpp


def median(a, k):
    p = np.pad(a, k, mode="edge")
    w = np.lib.stride_tricks.sliding_window_view(p, (2 * k + 1, 2 * k + 1))
    return np.median(w, axis=(2, 3))


def blur(a, k):
    for _ in range(3):
        p = np.pad(a, k, mode="edge")
        c = np.cumsum(np.cumsum(p, 0), 1)
        c = np.pad(c, ((1, 0), (1, 0)))
        n = 2 * k + 1
        a = (c[n:, n:] - c[:-n, n:] - c[n:, :-n] + c[:-n, :-n]) / (n * n)
    return a


def fill(d, hole, rounds=400):
    """The holes take their surroundings' shape (the heat equation run to rest, coarse first)."""
    d = np.where(hole, blur(np.where(hole, median(d, 6), d), 6), d)
    for _ in range(rounds):
        p = np.pad(d, 1, mode="edge")
        d = np.where(hole, (p[:-2, 1:-1] + p[2:, 1:-1] + p[1:-1, :-2] + p[1:-1, 2:]) / 4, d)
    return d


def grow(m, n):
    for _ in range(n):
        p = np.pad(m, 1)
        m = p[:-2, 1:-1] | p[2:, 1:-1] | p[1:-1, :-2] | p[1:-1, 2:] | m
    return m


def mend(d):
    """The radar's voids and the spikes round them: whatever is far off its neighbourhood's median is cut out with a
    margin and filled from its surroundings."""
    bad = np.abs(d - median(d, 4)) > MEND_TOL
    return fill(d, grow(bad, 3)), bad


def sample(d, fx, fy):
    """Bicubic (Catmull-Rom) at float pixel coords."""
    x0, y0 = np.floor(fx).astype(int), np.floor(fy).astype(int)
    tx, ty = fx - x0, fy - y0

    def w(t):
        return [((-t + 2) * t - 1) * t / 2, ((3 * t - 5) * t * t + 2) / 2, ((-3 * t + 4) * t + 1) * t / 2, (t - 1) * t * t / 2]
    wx, wy = w(tx), w(ty)
    out = 0
    for j in range(4):
        yy = np.clip(y0 + j - 1, 0, d.shape[0] - 1)
        row = 0
        for i in range(4):
            row = row + wx[i] * d[yy, np.clip(x0 + i - 1, 0, d.shape[1] - 1)]
        out = out + wy[j] * row
    return out


def build():
    raw, mpp = fetch()
    d, bad = mend(raw)
    nx = int(round((X_RANGE[1] - X_RANGE[0]) / STEP)) + 1
    ny = int(round((Y_RANGE[1] - Y_RANGE[0]) / STEP)) + 1
    X = X_RANGE[0] + STEP * np.arange(nx)[None, :]
    Y = Y_RANGE[0] + STEP * np.arange(ny)[:, None]
    # map +X is west, +Y north; the picture's columns run east, rows south
    c = raw.shape[0] / 2
    fx = c - (X - MID_AT[0]) * SCALE / mpp + 0 * Y
    fy = c - (Y - MID_AT[1]) * SCALE / mpp + 0 * X
    assert fx.min() > 2 and fy.min() > 2 and fx.max() < raw.shape[1] - 3 and fy.max() < raw.shape[0] - 3, "window off the data"
    z = sample(d, fx, fy) / SCALE
    for ex, ey, er in ERASE:
        z = fill(z, np.hypot(X - ex, Y - ey) < er, 3000)
    z = z - TILT[0] * (X - BASE[0]) - TILT[1] * (Y - BASE[1])
    i, j = int(round((BASE[0] - X_RANGE[0]) / STEP)), int(round((BASE[1] - Y_RANGE[0]) / STEP))
    z = z - z[j, i] + BASE[2]
    return z, {"mpp": mpp, "voids": int(bad.sum())}


def write(z, info):
    q = np.round(z * 10).astype("<i2")
    q.tofile(os.path.join(HERE, "dem.i16"))
    meta = {
        "source": "Northern Breakthrough cones, Tolbachik, Kamchatka (%.3f N %.3f E); NASA SRTM via Mapzen/AWS Terrain "
                  "Tiles zoom %d" % (LAT, LON, ZOOM),
        "scale": SCALE, "x0": X_RANGE[0], "y0": Y_RANGE[0], "step": STEP, "nx": int(z.shape[1]), "ny": int(z.shape[0]),
        "unit": 0.1, "order": "rows from y0 north, each from x0 toward +X (west)",
        "tilt_removed": list(TILT), "mid_at": list(MID_AT),
    }
    with open(os.path.join(HERE, "dem.json"), "w") as f:
        json.dump(meta, f, indent=1)
    return meta


def preview(z, path, K=4, G=200):
    gy, gx = np.gradient(z, STEP)
    # light from the north-west of the picture (map +X is the picture's left)
    s = np.clip(0.64 + 1.1 * (gx - gy) / np.sqrt(1 + gx * gx + gy * gy) * 0.7, 0, 1)
    t = np.clip((z - z.min()) / (z.max() - z.min()), 0, 1)
    rgb = np.stack([s * (0.45 + 0.5 * t), s * (0.75 - 0.3 * t), s * (0.4 - 0.1 * t)], -1)
    im = Image.fromarray((rgb[::-1, ::-1] * 255).astype(np.uint8)).resize((z.shape[1] * K, z.shape[0] * K), Image.LANCZOS)
    from PIL import ImageDraw
    dr = ImageDraw.Draw(im)
    for x in range(-1400, 1300, G):
        px = (X_RANGE[1] - x) / STEP * K
        dr.line([(px, 0), (px, im.size[1])], fill=(255, 255, 255, 60)); dr.text((px + 2, 2), str(x), fill=(255, 255, 0))
    for y in range(2200, 4400, G):
        py = (Y_RANGE[1] - y) / STEP * K
        dr.line([(0, py), (im.size[0], py)], fill=(255, 255, 255)); dr.text((2, py + 2), str(y), fill=(255, 255, 0))
        for x in range(-1400, 1300, G):
            v = z[int((y - Y_RANGE[0]) / STEP), int((x - X_RANGE[0]) / STEP)]
            dr.text(((X_RANGE[1] - x) / STEP * K + 2, py + 12), "%.0f" % v, fill=(255, 255, 255))
    im.save(path)


if __name__ == "__main__":
    z, info = build()
    meta = write(z, info)
    print("north dem: %d x %d cells, z %.1f..%.1f m, %d void cells mended" % (meta["nx"], meta["ny"], z.min(), z.max(), info["voids"]))
    if len(sys.argv) > 1:
        preview(z, sys.argv[1])
