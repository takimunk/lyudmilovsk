"""The clouds' own noise volumes (import_sky.py makes VolumeTextures of them for M_LyudClouds; tools/sky/cloud_hlsl.py).

The engine's cloud noise (T_VolumeNoiseShape128 / Erosion32) keeps one channel in a narrow 8-bit band (0.80..0.97 and
0.55..0.90): spread to 0..1 in the shader it had ~40 levels plus block-compression error, which showed in the clouds as
a coarse mottled grain ("confetti", worst against a low sun). These are full-range 16-bit and uncompressed:

  textures/sky/cloud_shape128.png   128^3 Perlin-Worley (billowy heaps), 16384 x 128: slice z at x = 128 z
  textures/sky/cloud_erosion32.png  32^3 Worley fbm (the fine billows and wisps), 1024 x 32

Both tile in all three axes and are histogram-matched to what the shader saw from the engine's after its remap (the
QUANTILES below), so every threshold tuned on the old noise still holds. Same seed, same files:

  python3 tools/sky/cloud_noise.py
"""
import os

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "..", "textures", "sky"))
SEED = 2660

# 5 % steps of the engine's noise after the shader's old remap (K.S / K.Er), measured from its source textures
SHAPE_Q = [0.0, 0.023, 0.161, 0.231, 0.3, 0.346, 0.392, 0.438, 0.484, 0.507, 0.554, 0.577, 0.623, 0.646, 0.669, 0.715,
           0.761, 0.784, 0.83, 0.9, 1.0]
EROSION_Q = [0.0, 0.0, 0.056, 0.14, 0.211, 0.27, 0.318, 0.365, 0.401, 0.449, 0.484, 0.52, 0.556, 0.603, 0.639, 0.674,
             0.722, 0.769, 0.829, 0.9, 1.0]


def worley(n, cells, rng):
    """Tileable 1 - F1 Worley noise on an n^3 grid with cells^3 feature points (one per cell)."""
    pts = rng.random((cells, cells, cells, 3))
    c = np.arange(n) * cells / n
    gx, gy, gz = np.meshgrid(c, c, c, indexing="ij")
    ix, iy, iz = gx.astype(int), gy.astype(int), gz.astype(int)
    best = np.full((n, n, n), 9.0)
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for dz in (-1, 0, 1):
                cx, cy, cz = (ix + dx) % cells, (iy + dy) % cells, (iz + dz) % cells
                p = pts[cx, cy, cz]
                d2 = ((ix + dx + p[..., 0] - gx) ** 2 + (iy + dy + p[..., 1] - gy) ** 2 + (iz + dz + p[..., 2] - gz) ** 2)
                best = np.minimum(best, d2)
    return 1.0 - np.clip(np.sqrt(best), 0.0, 1.0)


def smooth_noise(n, freq, rng):
    """Tileable band-limited value noise (white noise filtered round freq cycles per tile in Fourier space)."""
    w = rng.standard_normal((n, n, n))
    k = np.fft.fftfreq(n) * n
    kx, ky, kz = np.meshgrid(k, k, k, indexing="ij")
    r = np.sqrt(kx ** 2 + ky ** 2 + kz ** 2)
    f = np.exp(-((r - freq) / (0.6 * freq)) ** 2) * (r > 0)
    v = np.real(np.fft.ifftn(np.fft.fftn(w) * f))
    return (v - v.mean()) / (v.std() + 1e-9)


def match(v, q):
    """Histogram-match v to the quantiles q (5 % steps)."""
    order = np.argsort(v, axis=None)
    ranks = np.empty(v.size)
    ranks[order] = np.linspace(0.0, 1.0, v.size)
    return np.interp(ranks, np.linspace(0.0, 1.0, len(q)), q).reshape(v.shape)


def strip(vol):
    """n^3 -> an n x n^2 strip of z slices, as the VolumeTexture factory reads it (16-bit)."""
    n = vol.shape[0]
    img = np.concatenate([vol[:, :, z].T for z in range(n)], axis=1)
    return Image.fromarray((np.clip(img, 0.0, 1.0) * 65535.0 + 0.5).astype(np.uint16))


def main():
    rng = np.random.default_rng(SEED)
    n = 128
    perlin = sum(a * smooth_noise(n, f, rng) for a, f in ((1.0, 4.0), (0.5, 8.0), (0.25, 16.0)))
    perlin = (perlin - perlin.min()) / (perlin.max() - perlin.min())
    wfbm = 0.625 * worley(n, 4, rng) + 0.25 * worley(n, 8, rng) + 0.125 * worley(n, 16, rng)
    wfbm = (wfbm - wfbm.min()) / (wfbm.max() - wfbm.min())
    # Perlin-Worley: the Perlin noise remapped by the Worley fbm (billows with soft gaps)
    pw = np.clip((perlin - (wfbm - 1.0)) / (1.0 - (wfbm - 1.0)), 0.0, 1.0)
    shape = match(pw, SHAPE_Q)
    m = 32
    ero = 0.625 * worley(m, 2, rng) + 0.25 * worley(m, 4, rng) + 0.125 * worley(m, 8, rng)
    ero = match(ero, EROSION_Q)
    os.makedirs(OUT, exist_ok=True)
    strip(shape).save(os.path.join(OUT, "cloud_shape128.png"))
    strip(ero).save(os.path.join(OUT, "cloud_erosion32.png"))
    print("cloud_noise: shape", shape.shape, "erosion", ero.shape, "->", OUT)


if __name__ == "__main__":
    main()
