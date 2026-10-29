"""The cable-car stations' surface textures (docs/transport.md, "Station surfaces").

One Higgsfield grid (design/transport/textures/gen/grid_a.png, 3x3 swatches, pinned in gen/grid_a.json) is cut into
nine tiles, each made seamless (a regular pattern is cropped to whole periods, a random one is offset and blended over
its own middle), sized to 1024, and given a normal map from its brightness and a roughness map:

  design/transport/textures/T_TR_<name>_C.png   colour (sRGB)
  design/transport/textures/T_TR_<name>_N.png   normal (tangent space, +Y up in the picture = OpenGL; the importer flips green)
  design/transport/textures/T_TR_<name>_R.png   roughness (linear)

  python3 tools/transport_textures.py [--preview out.jpg]
unreal/scripts/import_transport.py imports them into /Game/Transport/Textures and makes M_TR_Surface's instances.
"""
import argparse
import json
import os

import numpy as np
from PIL import Image
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, ".."))
DIR = os.path.join(REPO, "design", "transport", "textures")
GRID = os.path.join(DIR, "gen", "grid_a.png")
SIZE = 1024

# cell (row, col) -> name, periodic axes ("" random, "y" rows repeat down, "xy" a lattice), relief strength, roughness
# base and spread, the real size one tile covers (metres; M_TR_Surface's Tile default), invert relief (dark = high)
TILES = {
    (0, 0): ("terrazzo", "", 1.0, 0.32, 0.10, 1.2, False),
    (0, 1): ("concrete", "", 1.6, 0.82, 0.10, 2.4, False),
    (0, 2): ("enamel", "", 2.2, 0.38, 0.08, 2.4, False),
    (1, 0): ("amber", "y", 3.0, 0.36, 0.06, 1.0, False),
    (1, 1): ("steel", "xy", 0.6, 0.30, 0.10, 1.0, False),
    (1, 2): ("plate", "", 2.4, 0.50, 0.15, 2.0, False),
    (2, 0): ("tread", "y", 3.0, 0.75, 0.10, 1.2, True),
    (2, 1): ("mosaic", "xy", 2.5, 0.20, 0.25, 0.8, True),
    (2, 2): ("tactile", "xy", 3.5, 0.55, 0.10, 0.6, False),
}


def cells(img):
    """The nine swatches without the black gutters (found from the dark rows and columns)."""
    a = np.asarray(img.convert("L"), dtype=np.float32)
    h, w = a.shape

    def spans(profile, n):
        dark = profile < 30
        out, start = [], None
        for i, d in enumerate(list(dark) + [True]):
            if not d and start is None:
                start = i
            elif d and start is not None:
                if i - start > n // 6:
                    out.append((start, i))
                start = None
        return out[:3]

    ys = spans(a.mean(axis=1), h)
    xs = spans(a.mean(axis=0), w)
    m = 10
    return {(r, c): img.crop((xs[c][0] + m, ys[r][0] + m, xs[c][1] - m, ys[r][1] - m)) for r in range(3) for c in range(3)}


def period(a, axis):
    """The pattern's period along an axis (pixels), from the autocorrelation of the mean profile."""
    p = a.mean(axis=1 - axis)
    p = p - p.mean()
    ac = np.correlate(p, p, "full")[len(p) - 1:]
    ac /= ac[0] + 1e-6
    lo = 8
    peaks = [i for i in range(lo, len(ac) // 2) if ac[i] > ac[i - 1] and ac[i] >= ac[i + 1] and ac[i] > 0.2]
    return peaks[0] if peaks else None


def crop_periods(img, axes):
    a = np.asarray(img.convert("L"), dtype=np.float32)
    h, w = a.shape
    box = [0, 0, w, h]
    for axis in (0, 1):
        if ("y" if axis == 0 else "x") not in axes:
            continue
        p = period(a, axis)
        if not p:
            continue
        n = (h if axis == 0 else w) // p
        if axis == 0:
            box[3] = n * p
        else:
            box[2] = n * p
    return img.crop(tuple(box))


def seamless(arr, axes):
    """Offset by half and blend the original back over the middle, on the axes that are not periodic already."""
    out = arr.astype(np.float32)
    for axis in (0, 1):
        if ("y" if axis == 0 else "x") in axes:
            continue
        n = out.shape[axis]
        rolled = np.roll(out, n // 2, axis=axis)
        t = np.abs(np.linspace(-1.0, 1.0, n))
        wgt = np.clip((1.0 - t) / 0.35, 0.0, 1.0)            # 1 in the middle, 0 at the edges
        wgt = wgt * wgt * (3 - 2 * wgt)
        shape = [1, 1, 1]
        shape[axis] = n
        wgt = wgt.reshape(shape)
        out = out * wgt + rolled * (1 - wgt)
    return out


def normal_map(height, strength):
    gx = ndimage.sobel(height, axis=1, mode="wrap")
    gy = ndimage.sobel(height, axis=0, mode="wrap")
    nx, ny, nz = -gx * strength, gy * strength, np.ones_like(height)
    ln = np.sqrt(nx * nx + ny * ny + nz * nz)
    return np.stack([nx / ln, ny / ln, nz / ln], axis=-1) * 0.5 + 0.5


def brushed(img):
    """Brushed steel made here (the scan's broad sheen would band when tiled): wrapped noise smeared along X, the scan's mean tone."""
    rng = np.random.default_rng(7)
    n = rng.random((SIZE, SIZE)).astype(np.float32)
    n = ndimage.gaussian_filter1d(n, 40, axis=1, mode="wrap") * 0.7 + ndimage.gaussian_filter1d(n, 3, axis=1, mode="wrap") * 0.3
    n = (n - n.mean()) / (n.std() + 1e-6)
    base = np.asarray(img.convert("RGB"), dtype=np.float32).reshape(-1, 3).mean(axis=0)
    rgb = np.clip(base[None, None, :] * (1.0 + 0.06 * n[..., None]), 0, 255)
    return Image.fromarray(rgb.astype(np.uint8))


def make(name, img, axes, strength, rough, spread, invert):
    img = brushed(img) if name == "steel" else crop_periods(img, axes)
    img = img.resize((SIZE, SIZE), Image.LANCZOS)
    rgb = seamless(np.asarray(img.convert("RGB")), axes)
    # flatten the light: the swatch's own large-scale shading (a gradient across a scan) would repeat as a pattern
    lum = rgb.mean(axis=2)
    low = ndimage.gaussian_filter(lum, SIZE / 6, mode="wrap")
    rgb = np.clip(rgb * (lum.mean() / np.maximum(low, 1.0))[..., None], 0, 255)
    lum = rgb.mean(axis=2) / 255.0
    h = ndimage.gaussian_filter(lum, 1.2, mode="wrap") - ndimage.gaussian_filter(lum, 24, mode="wrap")
    if invert:
        h = -h
    nrm = normal_map(h, strength * 6.0)
    detail = lum - ndimage.gaussian_filter(lum, 8, mode="wrap")
    r = np.clip(rough + spread * (-detail / (np.abs(detail).max() + 1e-6)), 0.04, 1.0)
    Image.fromarray(rgb.astype(np.uint8)).save(os.path.join(DIR, "T_TR_%s_C.png" % name))
    Image.fromarray((nrm * 255).astype(np.uint8)).save(os.path.join(DIR, "T_TR_%s_N.png" % name))
    Image.fromarray((r * 255).astype(np.uint8)).save(os.path.join(DIR, "T_TR_%s_R.png" % name))
    return rgb.astype(np.uint8)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview", help="a sheet of each texture tiled 2x2")
    a = ap.parse_args()
    grid = Image.open(GRID).convert("RGB")
    sheet = Image.new("RGB", (3 * 512, 3 * 512)) if a.preview else None
    meta = {}
    for (r, c), img in cells(grid).items():
        name, axes, strength, rough, spread, tile, invert = TILES[(r, c)]
        rgb = make(name, img, axes, strength, rough, spread, invert)
        meta[name] = {"tile_m": tile}
        if sheet:
            t = Image.fromarray(np.tile(rgb, (2, 2, 1))).resize((512, 512))
            sheet.paste(t, (c * 512, r * 512))
        print("texture", name)
    with open(os.path.join(DIR, "textures.json"), "w") as f:
        json.dump({"_doc": "Made by tools/transport_textures.py: the metres one tile of each texture covers.", "textures": meta}, f, indent=1)
    if sheet:
        sheet.save(a.preview, quality=88)


if __name__ == "__main__":
    main()
