#!/usr/bin/env python3
"""Texture atlases of the residents: every skin, garment, hairstyle, eye and accessory texture in four shared maps.

    python3 blender/residents/atlases.py [--tile 512] [--out blender/residents/atlases]
    (needs the scan: wardrobe_scan.py; accessories' tiles come from accessories.py, which wants bpy importable:
    run it with Blender's python or pip bpy)

All residents share four materials, one per atlas: Skin, Cloth, Hair (alpha-tested strands, beards, string bags) and
Eyes (irises, teeth, tongue). Each source texture gets a tile (tile px square, pad px of its own colour around it); a
mesh's UVs are squeezed into its tile (layout.json: tile rect u0 v0 u1 v1, UV origin bottom left).

What the game shader does with a texel T (docs/residents.md): colour = lerp(T, T * 2 * tint, amount * mask) with tint
and amount from the mesh's UV channels 1-2 and mask = T.a on Cloth (1 elsewhere). So:
- tinted cloth (wardrobe.py slots with a palette): grey detail, linear luminance with mean 0.5 per slot, alpha 255;
  logos/prints painted out first where the garment says `clean`; texels of the `keep_texels` family keep their colour
  with alpha 0;
- kept cloth (slots "keep*", shoes as painted): the original colour, alpha 0;
- hair, eyebrows, eyelashes, beards: grey detail, alpha = the strands;
- skin, eyes, teeth, tongue: the original colour (skin's tone comes from the tint).
Texels no face uses are filled from their neighbours, so mip maps don't bleed the sources' magenta/black backgrounds in.
"""
import argparse
import glob
import json
import math
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import wardrobe as W  # noqa: E402

SCAN = os.path.join(HERE, "cache", "wardrobe")
SECTIONS = ("Skin", "Cloth", "Hair", "Eyes")


# ---------------------------------------------------------------------------------------------- colour

def to_linear(c):
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def to_srgb(c):
    c = np.clip(c, 0.0, 1.0)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(c, 1 / 2.4) - 0.055)


def luminance(lin):
    return lin[..., 0] * 0.2126 + lin[..., 1] * 0.7152 + lin[..., 2] * 0.0722


def hsv(rgb):
    mx = rgb.max(-1)
    mn = rgb.min(-1)
    d = mx - mn
    s = np.where(mx > 1e-6, d / np.maximum(mx, 1e-6), 0.0)
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    dd = np.maximum(d, 1e-6)
    h = np.where(mx == r, (60 * (g - b) / dd) % 360, np.where(mx == g, 60 * (b - r) / dd + 120, 60 * (r - g) / dd + 240))
    return h, s, mx


def grey_detail(rgb, mask):
    """sRGB texels -> sRGB grey whose linear value has mean 0.5 over mask (relative contrast kept, tamed when dark)."""
    L = luminance(to_linear(rgb))
    m = float(L[mask].mean()) if mask.any() else 0.5
    d = 0.5 * L / max(m, 1e-4)
    if m < 0.03:                                   # a near-black source: its noise would turn into stripes
        d = 0.5 + (d - 0.5) * (m / 0.03)
    d = np.clip(d, 0.12, 1.0)
    g = to_srgb(d)
    return np.stack([g, g, g], -1)


# --------------------------------------------------------------------------------------------- filters

def box_blur(a, r):
    """Box blur of radius r along both axes (edges clamped); a is HxW or HxWxC."""
    if r <= 0:
        return a
    out = a
    for axis in (0, 1):
        pad = [(0, 0)] * out.ndim
        pad[axis] = (r + 1, r)
        p = np.pad(out, pad, mode="edge")
        c = np.cumsum(p, axis=axis)
        n = out.shape[axis]
        hi = np.take(c, np.arange(2 * r + 1, 2 * r + 1 + n), axis=axis)
        lo = np.take(c, np.arange(0, n), axis=axis)
        out = (hi - lo) / (2 * r + 1)
    return out


def soft_blur(a, r):
    for _ in range(3):
        a = box_blur(a, r)
    return a


def norm_blur(rgb, good, r):
    """Blur of rgb over the good texels only (normalised convolution)."""
    w = good.astype(np.float32)
    num = soft_blur(rgb * w[..., None], r)
    den = soft_blur(w, r)
    return num / np.maximum(den[..., None], 1e-6), den


def grow(mask, px):
    m = mask.copy()
    for _ in range(px):
        m = m | np.roll(m, 1, 0) | np.roll(m, -1, 0) | np.roll(m, 1, 1) | np.roll(m, -1, 1)
    return m


def fill_empty(rgb, filled, iters=48):
    """Pushes colours of filled texels out into the empty ones (then the mean)."""
    rgb = rgb.copy()
    f = filled.copy()
    for _ in range(iters):
        if f.all():
            break
        acc = np.zeros_like(rgb)
        cnt = np.zeros(f.shape, np.float32)
        ff = f.astype(np.float32)
        for sh in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            acc += np.roll(rgb * ff[..., None], sh, (0, 1))
            cnt += np.roll(ff, sh, (0, 1))
        new = (cnt > 0) & ~f
        rgb[new] = acc[new] / cnt[new][:, None]
        f = f | new
    if (~f).any() and f.any():
        rgb[~f] = rgb[f].mean(0)
    return rgb


def clean_prints(rgb, mask):
    """Paints logos and prints out of a slot: texels far from the slot's usual colour get the colour around them."""
    if not mask.any():
        return rgb
    med = np.median(rgb[mask], axis=0)
    dev = np.abs(rgb - med).max(-1)
    out = grow(mask & (dev > 0.16), 3) & mask
    good = mask & ~out
    if not out.any() or not good.any():
        return rgb
    fill, _ = norm_blur(rgb, good, 6)
    far, den = norm_blur(rgb, good, 24)
    fill = np.where((soft_blur(good.astype(np.float32), 6) > 0.05)[..., None], fill, far)
    rgb = rgb.copy()
    rgb[out] = fill[out]
    return rgb


def keep_mask(rgb, rule):
    h, s, v = hsv(rgb)
    if rule == "white":
        m = (s < 0.25) & (v > 0.55)
    elif rule == "maroon":
        m = ((h < 25) | (h > 315)) & (s > 0.25) & (v > 0.08)
    else:
        raise ValueError("keep_texels rule %s" % rule)
    # whole pieces, not speckles: close the gaps between stripes, drop lone texels
    m = soft_blur(m.astype(np.float32), 4) > 0.3
    return m


# ---------------------------------------------------------------------------------------------- tiles

def load(path, size=None):
    im = Image.open(path).convert("RGBA")
    if size and im.size != (size, size):
        rgb = im.convert("RGB").resize((size, size), Image.LANCZOS)
        a = im.getchannel("A").resize((size, size), Image.LANCZOS)
        rgb.putalpha(a)
        im = rgb
    return np.asarray(im, dtype=np.float32) / 255.0


def load_mask(path, size):
    im = Image.open(path).convert("L")
    if im.size != (size, size):
        im = im.resize((size, size), Image.BILINEAR)
    return np.asarray(im) > 96


def garment_tile(info, size):
    g = W.GARMENTS[info["name"]]
    src = load(info["texture_abs"], size)
    rgb = src[..., :3].copy()
    out_rgb = rgb.copy()
    alpha = np.zeros(rgb.shape[:2], np.float32)
    used = np.zeros(rgb.shape[:2], bool)
    pal = g.get("palettes", {})
    clean = g.get("clean", {})
    for slot, mp in sorted(info.get("masks", {}).items()):
        m = grow(load_mask(os.path.join(SCAN, mp), size), 2) & ~used
        used |= m
        if slot.startswith("keep") or slot not in pal:
            continue                                      # as painted, alpha 0
        work = clean_prints(rgb, m) if clean.get(slot) else rgb
        keep = keep_mask(work, g["keep_texels"]) & m if g.get("keep_texels") else np.zeros_like(m)
        tint = m & ~keep
        d = grey_detail(work, tint)
        out_rgb[tint] = d[tint]
        alpha[tint] = 1.0
    out_rgb = fill_empty(out_rgb, used)
    alpha = np.where(used, alpha, fill_empty(alpha[..., None].repeat(3, -1), used)[..., 0])
    return np.concatenate([out_rgb, alpha[..., None]], -1)


def strands_tile(path, size):
    src = load(path, size)
    a = src[..., 3]
    m = a > 0.05
    d = grey_detail(src[..., :3], m & (a > 0.5)) if m.any() else src[..., :3]
    rgb = fill_empty(np.where(m[..., None], d, 0.0), m)
    return np.concatenate([rgb, a[..., None]], -1)


def plain_tile(path, size, alpha=1.0):
    src = load(path, size)
    src[..., 3] = alpha
    return src


def accessory_tiles(size, tmp):
    """{tile: (rgba, section, tintable)} from accessories.py (bpy must import)."""
    try:
        import accessories as A
    except Exception as e:                                  # noqa: BLE001
        print("atlases: accessories.py not importable (%s): no accessory tiles" % e)
        return {}
    paths = A.make_tiles(tmp)
    out = {}
    for name, p in sorted(paths.items()):
        tintable = A.TILE_TINTABLE.get(name, True)
        src = load(p, size)
        has_alpha = float(src[..., 3].min()) < 0.99
        section = "Hair" if has_alpha else "Cloth"
        if tintable:
            m = src[..., 3] > 0.05
            rgb = grey_detail(src[..., :3], m)
            rgb = fill_empty(np.where(m[..., None], rgb, 0.0), m) if has_alpha else rgb
            src = np.concatenate([rgb, src[..., 3:4] if has_alpha else np.ones_like(src[..., 3:4])], -1)
        elif section == "Cloth":
            src[..., 3] = 0.0
        out[name] = (src, section, tintable)
    return out


# --------------------------------------------------------------------------------------------- packing

def grid(n):
    cols = 1
    while cols * cols < n:
        cols *= 2
    rows = 1
    while cols * rows < n:
        rows *= 2
    return cols, rows


def pack(tiles, size, pad):
    """tiles: [(key, rgba)] -> (atlas rgba, {key: [u0, v0, u1, v1]})."""
    cols, rows = grid(len(tiles))
    W_, H_ = cols * size, rows * size
    atlas = np.zeros((H_, W_, 4), np.float32)
    rects = {}
    inner = size - 2 * pad
    for i, (key, rgba) in enumerate(tiles):
        c, r = i % cols, i // cols
        u8 = (np.clip(rgba, 0, 1) * 255 + 0.5).astype(np.uint8)
        # colour and alpha apart: Pillow premultiplies RGBA when resampling (kept texels have alpha 0)
        rgb = Image.fromarray(u8[..., :3], "RGB").resize((inner, inner), Image.LANCZOS)
        a = Image.fromarray(u8[..., 3], "L").resize((inner, inner), Image.LANCZOS)
        small = np.concatenate([np.asarray(rgb, dtype=np.float32), np.asarray(a, dtype=np.float32)[..., None]], -1) / 255.0
        t = np.pad(small, ((pad, pad), (pad, pad), (0, 0)), mode="edge")
        atlas[r * size:(r + 1) * size, c * size:(c + 1) * size] = t
        x0, y0 = c * size + pad, r * size + pad
        rects[key] = [round(x0 / W_, 6), round(1 - (y0 + inner) / H_, 6), round((x0 + inner) / W_, 6), round(1 - y0 / H_, 6)]
    return atlas, rects


def scans(subdir):
    out = {}
    for p in sorted(glob.glob(os.path.join(SCAN, "%s__*.json" % subdir))):
        info = json.load(open(p, encoding="utf-8"))
        out[info["name"]] = info
    return out


def build(size, pad, out_dir):
    import tempfile
    os.makedirs(out_dir, exist_ok=True)
    sections = {s: [] for s in SECTIONS}
    meta = {}
    for name, info in scans("skins").items():
        sections["Skin"].append(("skins/" + name, plain_tile(info["texture_abs"], size)))
    for name, info in scans("clothes").items():
        if name not in W.GARMENTS:
            continue
        sections["Cloth"].append(("clothes/" + name, garment_tile(info, size)))
    for sub in ("hair", "eyebrows", "eyelashes"):
        for name, info in scans(sub).items():
            sections["Hair"].append(("%s/%s" % (sub, name), strands_tile(info["texture_abs"], size)))
    eyes = scans("eyes").get("high-poly") or scans("eyes").get("low-poly")
    if eyes:
        tex_dir = os.path.dirname(eyes["texture_abs"])
        for iris in W.IRISES:
            p = os.path.join(tex_dir, "%s_eye.png" % iris)
            if os.path.isfile(p):
                sections["Eyes"].append(("eyes/" + iris, plain_tile(p, size)))
    for sub, name in (("teeth", "teeth_base"), ("tongue", "tongue01")):
        info = scans(sub).get(name)
        if info:
            sections["Eyes"].append(("%s/%s" % (sub, name), plain_tile(info["texture_abs"], size)))
    with tempfile.TemporaryDirectory() as tmp:
        for name, (rgba, section, tintable) in accessory_tiles(size, tmp).items():
            sections[section].append(("acc/" + name, rgba))
            meta["acc/" + name] = dict(tintable=tintable)
    layout = {"tile": size, "pad": pad, "sections": {}}
    for s in SECTIONS:
        atlas, rects = pack(sections[s], size, pad)
        fn = "T_Resident_%s.png" % s
        Image.fromarray((np.clip(atlas, 0, 1) * 255 + 0.5).astype(np.uint8), "RGBA").save(os.path.join(out_dir, fn), optimize=False)
        layout["sections"][s] = dict(file=fn, size=[atlas.shape[1], atlas.shape[0]], tiles=rects)
        print("atlas %-5s %d tiles -> %s %dx%d" % (s, len(rects), fn, atlas.shape[1], atlas.shape[0]))
    layout["meta"] = meta
    with open(os.path.join(out_dir, "layout.json"), "w", encoding="utf-8") as f:
        json.dump(layout, f, indent=1, sort_keys=True)
    return layout


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    ap = argparse.ArgumentParser()
    ap.add_argument("--tile", type=int, default=512)
    ap.add_argument("--pad", type=int, default=0, help="default: tile / 32")
    ap.add_argument("--out", default=os.path.join(HERE, "atlases"))
    a = ap.parse_args(argv)
    build(a.tile, a.pad or max(4, a.tile // 32), a.out)


if __name__ == "__main__":
    main()
