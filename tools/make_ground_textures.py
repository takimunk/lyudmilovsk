"""Turns the raw ground photos (tools/fal_ground_textures.py -> textures/ground/raw/<Layer>.png) into the
landscape's textures: textures/ground/<Layer>_D.jpg (albedo, seamless, even light) and
textures/ground/<Layer>_N.jpg (tangent-space normal from the photo's fine detail). Runs in Blender for
its numpy:

  blender -b --python tools/make_ground_textures.py [-- Layer ...]
"""
import os
import sys

import bpy
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "textures", "ground", "raw")
DST = os.path.join(HERE, "..", "textures", "ground")
LAYERS = ["Grass", "Meadow", "Dirt", "Mud", "Sand", "Rock", "Forest"]
SIZE = 1024
# how strong the bumps read in the normal map, per layer (sand is nearly smooth, rock is rough)
BUMP = {"Grass": 3.0, "Meadow": 3.0, "Dirt": 4.0, "Mud": 3.5, "Sand": 1.5, "Rock": 6.0, "Forest": 3.5}


def load(path):
    im = bpy.data.images.load(path)
    if im.size[0] != SIZE:
        im.scale(SIZE, SIZE)
    a = np.array(im.pixels[:], dtype=np.float32).reshape(SIZE, SIZE, 4)[:, :, :3]
    bpy.data.images.remove(im)
    return a


def save(a, path):
    im = bpy.data.images.new("out", SIZE, SIZE, alpha=False)
    rgba = np.concatenate([np.clip(a, 0, 1), np.ones((SIZE, SIZE, 1), np.float32)], axis=2)
    im.pixels[:] = rgba.ravel()
    bpy.context.scene.view_settings.view_transform = "Standard"   # no filmic tone curve on the pixels
    settings = bpy.context.scene.render.image_settings
    settings.file_format = "JPEG"
    settings.quality = 92
    im.save_render(path, scene=bpy.context.scene)
    bpy.data.images.remove(im)


def blur(a, sigma):
    """Gaussian blur with wrap-around (FFT), per channel."""
    f = np.fft.fftfreq(SIZE)
    g = np.exp(-2 * (np.pi * sigma) ** 2 * (f[:, None] ** 2 + f[None, :] ** 2)).astype(np.float32)
    if a.ndim == 2:
        return np.real(np.fft.ifft2(np.fft.fft2(a) * g)).astype(np.float32)
    return np.stack([blur(a[:, :, c], sigma) for c in range(a.shape[2])], axis=2)


def seamless(a):
    """Cross-fades the photo with itself shifted by half, so the shifted copy covers the borders."""
    rolled = np.roll(np.roll(a, SIZE // 2, 0), SIZE // 2, 1)
    t = np.minimum(np.arange(SIZE), SIZE - 1 - np.arange(SIZE)) / float(SIZE)
    m1 = np.clip((t - 0.06) / 0.22, 0, 1)
    m1 = m1 * m1 * (3 - 2 * m1)
    m = m1[:, None] * m1[None, :]
    # a noisy edge to the fade, so it does not read as a soft square
    rng = np.random.default_rng(3)
    n = blur(rng.standard_normal((SIZE, SIZE)).astype(np.float32), 24.0)
    n /= np.abs(n).max() + 1e-6
    m = np.clip(m + 0.35 * n * m * (1 - m) * 4, 0, 1)[:, :, None]
    return a * m + rolled * (1 - m)


def even_light(a, keep=0.25):
    """Takes out the photo's large-scale light and colour drift (it repeats visibly when tiled)."""
    low = blur(a, 96.0)
    mean = a.reshape(-1, 3).mean(0)
    return a * (keep + (1 - keep) * mean / np.maximum(low, 1e-3))


def normal_map(a, strength):
    lum = a @ np.array([0.3, 0.59, 0.11], np.float32)
    h = lum - blur(lum, 10.0)                       # fine detail only
    h = blur(h, 0.8)
    gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5 * strength * 8
    gy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5 * strength * 8
    n = np.stack([-gx, gy, np.ones_like(h)], axis=2)  # rows run bottom-up, so +gy is DirectX green (Unreal's)
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    return n * 0.5 + 0.5


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    for name in argv or LAYERS:
        src = os.path.join(SRC, name + ".png")
        if not os.path.exists(src):
            print("missing", src)
            continue
        a = seamless(even_light(load(src)))
        save(a, os.path.abspath(os.path.join(DST, name + "_D.jpg")))
        save(normal_map(a, BUMP[name]), os.path.abspath(os.path.join(DST, name + "_N.jpg")))
        print("ground texture", name)


main()
