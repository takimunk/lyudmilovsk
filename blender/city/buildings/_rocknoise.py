"""Rock noise for blender/city/buildings/far_light.py: a COPY of blender/ship/seascape_kit.py's _hash, vnoise, fbm, smooth,
joints and skerry_height (that module builds the whole kit when imported, so it cannot be). The real Far Light must
have the far-view copy's shape: change both files or neither."""
import math

import numpy as np

SKIRT = -7.0


def _hash(ix, iy, iz, seed):
    h = ix.astype(np.int64) * 73856093 ^ iy.astype(np.int64) * 19349663 ^ iz.astype(np.int64) * 83492791 ^ (seed * 2654435761)
    h &= 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    h ^= h >> 16
    return (h & 0xFFFFFF) / float(0xFFFFFF)


def vnoise(x, y, z, seed=0):
    """Value noise 0..1, period 1 in each argument's unit."""
    x = np.asarray(x, np.float64); y = np.asarray(y, np.float64); z = np.asarray(z, np.float64)
    x, y, z = np.broadcast_arrays(x, y, z)
    ix = np.floor(x); iy = np.floor(y); iz = np.floor(z)
    fx = x - ix; fy = y - iy; fz = z - iz
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy); fz = fz * fz * (3 - 2 * fz)
    out = 0.0
    for dx in (0, 1):
        wx = fx if dx else 1 - fx
        for dy in (0, 1):
            wy = fy if dy else 1 - fy
            for dz in (0, 1):
                wz = fz if dz else 1 - fz
                out = out + _hash(ix + dx, iy + dy, iz + dz, seed) * wx * wy * wz
    return out


def fbm(x, y, z, scale, octaves=4, seed=0, gain=0.5):
    """Fractal noise about -1..1; scale = the first octave's feature size, m."""
    out = 0.0
    amp = 1.0
    tot = 0.0
    f = 1.0 / scale
    for o in range(octaves):
        out = out + amp * (vnoise(x * f, y * f, z * f, seed + o * 17) * 2 - 1)
        tot += amp
        amp *= gain
        f *= 2.03
    return out / tot


def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def joints(x, y, scale, width, seed):
    """Vertical cracks: 1 in a joint, 0 away (lines where a plan noise crosses its middle)."""
    n = vnoise(x / scale, y / scale, 0.0, seed) + 0.35 * vnoise(x / scale * 2.7, y / scale * 2.7, 0.0, seed + 1) - 0.175
    return 1.0 - smooth(0.0, width, np.abs(n - 0.5))


def skerry_height(X, Y, blobs, seed, rough=0.5):
    """A low wave-washed platform: smooth whalebacks (cx, cy, rx, ry, h) summed softly, striated."""
    H = np.full(X.shape, -9.0)
    for (cx, cy, rx, ry, h, rot) in blobs:
        c, s = math.cos(rot), math.sin(rot)
        u = ((X - cx) * c + (Y - cy) * s) / rx; v = (-(X - cx) * s + (Y - cy) * c) / ry
        q = u * u + v * v
        # steeper on the lee end, like ice-smoothed rock
        hh = (h + 9.0) * np.clip(1 - q, 0, None) ** 0.75 * (1 + 0.25 * np.clip(u, -1, 1)) - 9.0
        H = np.maximum(H, hh)
    live = smooth(-5, 0.5, H)
    H = H + rough * (1.6 * fbm(X, Y, 0.0, 13.0, 4, seed) + 0.9 * np.abs(fbm(X, Y, 0.0, 5.0, 3, seed + 3))) * live + 0.2 * fbm(X, Y, 0.0, 1.8, 2, seed + 1)
    # joints: clefts across, two sets, and blocks plucked out on the lee side
    H = H - (1.5 * joints(X, Y, 17.0, 0.035, seed + 2) + 0.8 * joints(X + 0.7 * Y, Y - 0.7 * X, 9.0, 0.04, seed + 6)) * live
    H = H - 1.1 * smooth(0.62, 0.75, vnoise(X / 11.0, Y / 7.0, 0.0, seed + 8)) * live
    return H
