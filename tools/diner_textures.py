#!/usr/bin/env python3
"""Procedural tileable PBR textures for the 1940s diner (numpy + PIL, deterministic, no network).

    nice -n 19 python3 tools/diner_textures.py [set ...]

Writes textures/diner/<set>_D.jpg (base colour sRGB), <set>_N.png (tangent normal, DirectX: green = down, Unreal's),
<set>_R.jpg (mask: R roughness, G ambient occlusion, B 0) and design/diner/previews/textures_sheet.jpg (contact sheet).
Every set is periodic (noise from FFT-filtered white noise on the tile, cell patterns on wrapped lattices), and is drawn at real
scale: SETS[name] = (tile metres, pixels). UVs of the meshes are in metres (blender/diner/diner_common.py MAT_TEX).
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
OUT = os.path.join(ROOT, "textures", "diner")
SHEET = os.path.join(ROOT, "design", "diner", "previews", "textures_sheet.jpg")


# ---------------------------------------------------------------- helpers
def hexc(h):
    h = h.lstrip("#")
    return np.array([int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)])


def sstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def mix(a, b, t):
    return a + (b - a) * t[..., None]


def _freq(n):
    k = np.fft.fftfreq(n) * n
    return k[None, :], k[:, None]          # KX (cycles per tile along u), KY


def noise(n, seed, fc=8.0, s=3.0, ax=1.0, ay=1.0, flo=0.0):
    """Periodic noise, std 1. fc = cutoff in cycles per tile; ax/ay stretch (ax large = features long along u);
    flo = highpass cycles per tile."""
    kx, ky = _freq(n)
    f = np.hypot(kx * ax, ky * ay)
    amp = 1.0 / (1.0 + (f / fc) ** s)
    if flo > 0:
        amp = amp * (1 - np.exp(-(f / flo) ** 2))
    amp[0, 0] = 0
    w = np.random.default_rng(seed).standard_normal((n, n))
    a = np.fft.ifft2(np.fft.fft2(w) * amp).real
    return a / (a.std() + 1e-12)


def blur(a, sx, sy=None):
    """Periodic gaussian blur, sigma in pixels."""
    sy = sx if sy is None else sy
    n = a.shape[0]
    fx = np.fft.fftfreq(n)[None, :]
    fy = np.fft.fftfreq(n)[:, None]
    g = np.exp(-2 * np.pi ** 2 * ((sx * fx) ** 2 + (sy * fy) ** 2))
    return np.fft.ifft2(np.fft.fft2(a) * g).real


def dots(n, seed, count, sigma, sy=None, amp_lo=0.5):
    """Sparse soft dots/streaks (peak <= 1), `count` per tile."""
    rng = np.random.default_rng(seed)
    a = np.zeros((n, n))
    ys = rng.integers(0, n, count)
    xs = rng.integers(0, n, count)
    a[ys, xs] = rng.uniform(amp_lo, 1.0, count)
    sy = sigma if sy is None else sy
    return blur(a, sigma, sy) * (2 * np.pi * sigma * sy)


def worley(n, g, seed):
    """Periodic cellular noise with g x g cells: F1, F2 (cell units) and the id of the nearest cell."""
    rng = np.random.default_rng(seed)
    jx = rng.random((g, g))
    jy = rng.random((g, g))
    pos = (np.arange(n) + 0.5) / n * g
    cell = np.floor(pos).astype(int)
    f1 = np.full((n, n), 9.0)
    f2 = np.full((n, n), 9.0)
    cid = np.zeros((n, n), int)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            ry = ((cell + dy) % g)[:, None]
            rx = ((cell + dx) % g)[None, :]
            px = (cell + dx)[None, :] + jx[ry, rx] - pos[None, :]
            py = (cell + dy)[:, None] + jy[ry, rx] - pos[:, None]
            d = np.hypot(px, py)
            new1 = d < f1
            new2 = (~new1) & (d < f2)
            f2 = np.where(new1, f1, np.where(new2, d, f2))
            cid = np.where(new1, ry * g + rx, cid)
            f1 = np.where(new1, d, f1)
    return f1, f2, cid


def tile_grid(n, tile_m, cols, rows, bond=False):
    """Rectangular tiles/bricks on the tile: distance to the nearest edge d (m), tile ids, local 0..1 coords."""
    px = tile_m / n
    c = (np.arange(n) + 0.5) * px
    X = np.broadcast_to(c[None, :], (n, n))
    Y = np.broadcast_to(c[:, None], (n, n))
    pw, ph = tile_m / cols, tile_m / rows
    r = np.floor(Y / ph).astype(int)
    xs = (X + (r % 2) * pw / 2) % tile_m if bond else X
    ix = np.floor(xs / pw).astype(int) % cols
    lx = xs - np.floor(xs / pw) * pw
    ly = Y - r * ph
    d = np.minimum(np.minimum(lx, pw - lx), np.minimum(ly, ph - ly))
    return d, r % rows * cols + ix, ix, r % rows, lx / pw, ly / ph


def height_ao(h, tile_m, radius_mm=3.0, k=1.0):
    n = h.shape[0]
    rpx = max(radius_mm * 1e-3 / (tile_m / n), 0.7)
    cav = np.clip(blur(h, rpx) - h, 0, None) / (radius_mm * 1e-3)
    return np.clip(1 - k * cav, 0.2, 1)


def normal_map(h, tile_m, k=1.0):
    """DirectX tangent normal from a height map in metres (rows run down = -v): n = (-dh/dx, -dh/drow, 1)."""
    n = h.shape[0]
    px = tile_m / n
    gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) / (2 * px) * k
    gy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) / (2 * px) * k
    nx, ny, nz = -gx, -gy, np.ones_like(h)
    L = np.sqrt(nx * nx + ny * ny + nz * nz)
    return np.stack([nx / L, ny / L, nz / L], -1) * 0.5 + 0.5


def cov_mask(d, half, px):
    """1 on the tile surface, 0 in the joint (half = joint half-width, m), antialiased."""
    return np.clip((d - half) / px + 0.5, 0, 1)


# ---------------------------------------------------------------- sets
def floor_checker(n, S):
    tile = 1.0
    px = tile / n
    d, tid, ix, iy, lu, lv = tile_grid(n, tile, 4, 4)
    black = ((ix + iy) % 2 == 0)
    rng = np.random.default_rng(S)
    tv = rng.random(16)
    tvar = (tv[tid] - 0.5) * 2
    jw = 0.0025
    cov = cov_mask(d, jw / 2, px)
    cb, cc = hexc("#1b1b1b"), hexc("#e9e4d6")
    base = np.where(black[..., None], cb, cc)
    v = 1 + 0.05 * tvar + 0.008 * noise(n, S + 1, 30) + 0.008 * noise(n, S + 2, 90, flo=20)
    # marbled vinyl chips on the cream tiles, faint grey flecks in the black ones
    fleck = noise(n, S + 3, 160, flo=60)
    v = v + np.where(black, 0.0, 0.012 * fleck) + np.where(black, 0.05 * np.clip(fleck - 1.8, 0, None), 0)
    col = base * v[..., None]
    sx = np.clip(noise(n, S + 4, 220, ax=14.0) - 2.0, 0, 3)
    sy = np.clip(noise(n, S + 5, 220, ay=14.0) - 2.0, 0, 3)
    scuff = np.clip((sx + sy) * 0.6, 0, 1.5)
    col = col + np.where(black, 0.045, -0.06)[..., None] * scuff[..., None]
    dirt = np.exp(-np.clip(d - jw / 2, 0, None) / 0.004)
    col = col * (1 - np.where(black, 0.0, 0.10) * dirt)[..., None]
    wear = np.exp(-np.clip(d - jw / 2, 0, None) / 0.0012) * black
    col = col + 0.045 * wear[..., None] + 0.06 * (dirt * noise(n, S + 6, 120))[..., None] * 0.3
    gcol = hexc("#8a8884") * (1 + 0.07 * noise(n, S + 7, 200))[..., None]
    col = mix(gcol, col, cov)
    h = np.where(d < jw / 2, -0.0012, -0.0012 * (1 - sstep(jw / 2, jw / 2 + 0.0016, d)))
    h = h - 0.00004 * scuff + 0.00002 * noise(n, S + 8, 100)
    rough = np.where(black, 0.36, 0.42) + 0.07 * noise(n, S + 9, 20) + 0.14 * scuff
    rough = rough * cov + 0.85 * (1 - cov)
    ao = np.minimum(1 - 0.35 * (1 - cov) - 0.15 * dirt, 1) * height_ao(h, tile, 3, 0.6) ** 0.5
    return col, h, rough, ao, 1.0


def floor_border(n, S):
    tile = 0.5
    px = tile / n
    d, tid, ix, iy, lu, lv = tile_grid(n, tile, 4, 4)
    rng = np.random.default_rng(S)
    tvar = (rng.random(16)[tid] - 0.5) * 2
    jw = 0.002
    cov = cov_mask(d, jw / 2, px)
    base = hexc("#2c1916") * (1 + 0.12 * tvar)[..., None]
    # terrazzo chips: three populations of small stones
    chips = np.zeros((n, n, 3))
    for i, (c, th, f) in enumerate((("#6b3a30", 1.7, 260), ("#8c7a6a", 2.25, 300), ("#4a2822", 1.3, 200))):
        z = noise(n, S + 10 + i, f, s=4, flo=f * 0.35)
        m = sstep(th, th + 0.25, z)
        chips = chips * (1 - m[..., None]) + hexc(c) * m[..., None]
        base = base * (1 - m[..., None]) + hexc(c)[None, None] * (0.85 + 0.3 * rng.random()) * m[..., None]
    col = base * (1 + 0.025 * noise(n, S + 20, 40))[..., None]
    gcol = hexc("#6a6159") * (1 + 0.08 * noise(n, S + 21, 150))[..., None]
    col = mix(gcol, col, cov)
    h = np.where(d < jw / 2, -0.001, -0.001 * (1 - sstep(jw / 2, jw / 2 + 0.0012, d))) + 0.00004 * noise(n, S + 22, 150)
    rough = 0.42 + 0.06 * noise(n, S + 23, 30)
    rough = rough * cov + 0.85 * (1 - cov)
    ao = (1 - 0.3 * (1 - cov)) * height_ao(h, tile, 2.5, 0.5)
    return col, h, rough, ao, 1.0


def formica(n, S):
    tile = 0.6
    base = hexc("#e5dabf")
    patch = 0.035 * noise(n, S + 1, 7) + 0.02 * noise(n, S + 2, 18) + 0.012 * noise(n, S + 3, 60)
    f1, f2, _ = worley(n, 7, S + 4)
    crack = 1 - sstep(0.0, 0.05, f2 - f1)
    f1b, f2b, _ = worley(n, 15, S + 5)
    crack2 = 1 - sstep(0.0, 0.035, f2b - f1b)
    v = 1 + patch - 0.045 * crack * (0.5 + 0.5 * sstep(-1, 1, noise(n, S + 6, 5))) - 0.025 * crack2
    col = base * v[..., None]
    gold = np.clip(dots(n, S + 7, 700, 1.1), 0, 1)
    grey = np.clip(dots(n, S + 8, 500, 1.0), 0, 1)
    brown = np.clip(dots(n, S + 9, 90, 3.0, 1.0) * 0.8, 0, 1)         # a few small boomerang-ish streaks
    brown2 = np.clip(dots(n, S + 10, 90, 1.0, 3.5) * 0.8, 0, 1)
    col = mix(col, np.broadcast_to(hexc("#c3a05a"), col.shape), gold * 0.7)
    col = mix(col, np.broadcast_to(hexc("#9c978a"), col.shape), grey * 0.6)
    col = mix(col, np.broadcast_to(hexc("#cfc3a4"), col.shape), np.maximum(brown, brown2) * 0.5)
    h = 0.000015 * noise(n, S + 11, 80)
    rough = 0.3 + 0.03 * noise(n, S + 12, 12) + 0.05 * crack
    ao = np.ones((n, n)) * (1 - 0.04 * crack)
    return col, h, rough, ao, 1.0


def mahogany(n, S):
    tile = 1.2
    Yc = (np.arange(n) + 0.5) / n
    Yc = Yc[:, None]
    warp = noise(n, S + 1, 60, ax=22.0) * 0.5 + 0.7 * noise(n, S + 2, 14, ax=9.0)
    st1 = np.sin(2 * np.pi * (70 * Yc + 0.9 * warp))
    st2 = np.sin(2 * np.pi * (23 * Yc + 0.6 * warp + 0.4 * noise(n, S + 3, 10, ax=6.0)))
    fine = noise(n, S + 4, 500, ax=45.0, flo=60)
    fig = noise(n, S + 5, 8, ax=3.0)
    t = 0.5 + 0.11 * st1 + 0.09 * st2 + 0.09 * fine + 0.07 * fig
    pore = np.clip(blur(dots(n, S + 6, 2600, 1.0), 4.0, 0.7) * 5.5, 0, 1)
    pore = np.clip(pore + 0.5 * np.clip(blur(dots(n, S + 7, 3500, 1.0), 2.5, 0.6) * 4, 0, 1), 0, 1)
    t = np.clip(t, 0, 1)
    dark, light = hexc("#3a1a10"), hexc("#7d3e28")
    col = dark + (light - dark) * t[..., None]
    col = col * (1 - 0.22 * pore)[..., None]
    h = 0.00006 * (0.5 * fine + 0.3 * st1) - 0.00022 * pore
    rough = 0.35 + 0.035 * noise(n, S + 8, 12) + 0.2 * pore - 0.03 * fig
    ao = 1 - 0.55 * pore
    return col, h, rough, ao, 1.0


def vinyl_red(n, S):
    tile = 0.25
    f1, f2, cid = worley(n, 100, S)
    edge = f2 - f1
    bump = sstep(0.0, 0.4, edge)
    cell_r = np.random.default_rng(S + 1).random(100 * 100)[cid]
    f1b, f2b, _ = worley(n, 210, S + 2)
    fine = sstep(0.0, 0.3, f2b - f1b)
    h = 0.00035 * bump + 0.00012 * fine + 0.00003 * noise(n, S + 3, 200)
    sheen = noise(n, S + 4, 6)
    v = 0.84 + 0.16 * bump + 0.03 * (cell_r - 0.5) + 0.02 * sheen + 0.015 * noise(n, S + 5, 120)
    col = hexc("#a01e26") * v[..., None]
    rough = 0.44 + 0.07 * sheen + 0.14 * (1 - bump) * 1.0 - 0.03 * fine
    ao = 0.55 + 0.45 * bump
    return col, h, rough, ao, 2.2


def steel_brushed(n, S):
    tile = 0.5
    s1 = noise(n, S + 1, 420, ax=60.0, flo=0)       # long, fine streaks
    s2 = noise(n, S + 2, 180, ax=24.0)
    s3 = noise(n, S + 3, 40, ax=6.0)
    smudge = noise(n, S + 4, 5) * 0.6 + noise(n, S + 5, 14) * 0.4
    fp = np.clip(blur(dots(n, S + 6, 6, 3.0), 2.0, 1.0) * 3, 0, 1)      # a couple of fingerprints-ish blots
    v = 1 + 0.028 * s1 + 0.016 * s2 + 0.012 * s3 + 0.035 * smudge - 0.04 * fp
    col = hexc("#b7babd") * v[..., None]
    h = 0.000012 * s1 + 0.000008 * s2
    rough = np.clip(0.31 + 0.045 * s1 + 0.025 * s2 + 0.03 * smudge + 0.04 * fp, 0.24, 0.42)
    ao = np.ones((n, n))
    return col, h, rough, ao, 3.0


def aluminum_ribbed(n, S):
    tile = 0.05
    Y = (np.arange(n) + 0.5) / n
    p = (Y * 25.0) % 1.0                               # 25 ribs, 2 mm each
    q = np.clip(1 - ((p - 0.5) / 0.42) ** 2, 0, 1)
    rib = np.sqrt(q)[:, None] * np.ones((1, n))
    streak = noise(n, S + 1, 90, ax=18.0)
    h = 0.00045 * rib + 0.000004 * streak
    v = 1 + 0.03 * streak + 0.02 * noise(n, S + 2, 6) - 0.10 * (1 - rib)
    col = hexc("#c9cdd1") * v[..., None]
    rough = 0.28 + 0.04 * streak + 0.15 * (1 - rib)
    ao = 0.35 + 0.65 * rib ** 0.6
    return col, h, rough, ao, 1.0


def enamel(n, S, hexcol, name):
    tile = 1.0
    peel = noise(n, S + 1, 150, s=4, flo=25)
    peel2 = noise(n, S + 2, 40, s=3)
    h = 0.000025 * peel + 0.00003 * peel2
    v = 1 + 0.018 * noise(n, S + 3, 4) + 0.012 * noise(n, S + 4, 14) + 0.004 * peel
    col = hexc(hexcol) * v[..., None]
    chip = np.clip(dots(n, S + 5, 14, 1.4), 0, 1)
    speck = np.clip(dots(n, S + 6, 60, 0.8), 0, 1)
    rim = np.clip(blur(chip, 1.5) * 1.6 - chip * 0.4, 0, 1)
    col = mix(col, np.broadcast_to(hexc("#3a3836"), col.shape), np.clip(chip * 1.6, 0, 1) * 0.85)
    col = mix(col, np.broadcast_to(np.minimum(hexc(hexcol) * 1.12 + 0.03, 1), col.shape), rim * 0.4)
    col = col * (1 - 0.18 * speck)[..., None]
    h = h - 0.00035 * chip
    rough = 0.16 + 0.025 * noise(n, S + 7, 6) + 0.012 * peel + 0.45 * chip
    ao = 1 - 0.5 * chip
    return col, h, rough, ao, 1.2


def wall_tile(n, S):
    tile = 0.4
    px = tile / n
    d, tid, ix, iy, lu, lv = tile_grid(n, tile, 4, 4)
    rng = np.random.default_rng(S)
    tvar = (rng.random(16)[tid] - 0.5) * 2
    tint = (rng.random(16)[tid] - 0.5) * 2
    jw = 0.003
    cov = cov_mask(d, jw / 2, px)
    pillow = sstep(jw / 2, jw / 2 + 0.009, d)
    dome = 4 * lu * (1 - lu) * 4 * lv * (1 - lv)
    col = hexc("#eadfc6") * (1 + 0.025 * tvar + 0.012 * noise(n, S + 1, 24))[..., None]
    col = col * (1 + 0.012 * tint[..., None] * np.array([1.0, 0.0, -1.5]))
    glazeedge = 1 - 0.035 * (1 - pillow)
    col = col * glazeedge[..., None]
    crazing = np.clip(worley(n, 22, S + 2)[1] - worley(n, 22, S + 2)[0], 0, 1)
    craze = (1 - sstep(0.0, 0.03, crazing)) * sstep(0.6, 1.2, noise(n, S + 3, 12))
    col = col * (1 - 0.05 * craze)[..., None]
    gcol = hexc("#b7b5ad") * (1 + 0.08 * noise(n, S + 4, 220) + 0.05 * noise(n, S + 5, 40))[..., None]
    gcol = gcol * (1 - 0.1 * (1 - cov))[..., None]
    col = mix(gcol, col, cov)
    h = -0.0018 * (1 - pillow) * cov - 0.0018 * (1 - cov) + 0.0007 * dome * cov + 0.00003 * noise(n, S + 6, 100) * (1 - cov)
    rough = 0.15 + 0.02 * noise(n, S + 7, 10) + 0.04 * craze
    rough = rough * cov + (0.88 + 0.05 * noise(n, S + 8, 100)) * (1 - cov)
    ao = (1 - 0.4 * (1 - cov)) * (1 - 0.12 * (1 - pillow) * cov)
    return col, h, rough, ao, 1.0


def ceiling(n, S):
    tile = 1.22
    px = tile / n
    x = (np.arange(n) + 0.5) * px
    xm = x % 0.61
    dj = np.minimum(xm, 0.61 - xm)                      # distance to the joint (m)
    panel = (x // 0.61).astype(int)
    # profile along u only
    groove = np.exp(-(dj / 0.0012) ** 2)
    lip = np.exp(-(((dj - 0.006) / 0.003) ** 2))
    prof_h = -0.0009 * groove + 0.0004 * lip - 0.0006 * sstep(0.0, 0.5, dj / 0.5) * 0   # keep flat
    bow = 0.0012 * (1 - (2 * xm / 0.61 - 1) ** 2)       # panels bow slightly between joints
    tone = np.where(panel == 0, 1.0, 0.992) * (1 - 0.05 * groove - 0.015 * np.exp(-(dj / 0.01) ** 2) + 0.012 * lip)
    prof_h = prof_h + bow * 0.0
    col = hexc("#f1e7d2")[None, None] * tone[None, :, None] * np.ones((n, 1, 1))
    lown = noise(n, S + 1, 16, s=3)
    col = col * (1 + 0.004 * lown)[..., None]
    h = np.ones((n, 1)) * prof_h[None, :] + 0.00002 * lown
    # rivets: 10 per joint line, both sides, at 6 mm from the joint
    rv = np.zeros((n, n))
    yy = (np.arange(n) + 0.5) * px
    for jx in (0.0, 0.61):
        for side in (-1, 1):
            cx = (jx + side * 0.011)
            for k in range(10):
                cy = (k + 0.5) * tile / 10
                dx = (x[None, :] - cx + tile / 2) % tile - tile / 2
                dy = (yy[:, None] - cy + tile / 2) % tile - tile / 2
                rv = np.maximum(rv, np.exp(-(dx * dx + dy * dy) / (2 * 0.0014 ** 2)))
    col = col * (1 - 0.1 * rv)[..., None] + 0.03 * np.clip(blur(rv, 1.2) - rv * 0.3, 0, 1)[..., None]
    h = h + 0.0006 * rv
    rough = 0.6 + 0.02 * lown - 0.12 * rv + 0.1 * groove[None, :]
    ao = (1 - 0.5 * groove[None, :]) * np.ones((n, 1)) * (1 - 0.1 * rv)
    return col, h, rough, ao, 1.0


def brick(n, S):
    tile = 1.0
    px = tile / n
    cols, rows = 4, 14
    d, tid, ix, iy, lu, lv = tile_grid(n, tile, cols, rows, bond=True)
    rng = np.random.default_rng(S)
    # a brick id must be the same for the wrapped half of a shifted row: tid is by (row, col after the shift): fine
    tv = rng.random(rows * cols)
    tv2 = rng.random(rows * cols)
    tvar = (tv[tid] - 0.5) * 2
    tvar2 = (tv2[tid] - 0.5) * 2
    jw = 0.010
    cov = cov_mask(d, jw / 2, px)
    inner = sstep(jw / 2, jw / 2 + 0.004, d)
    base = hexc("#7b3b2d")
    warm = np.array([1.0, 0.0, -0.2])
    col = base * (1 + 0.14 * tvar)[..., None] * (1 + 0.06 * tvar2[..., None] * np.array([0.3, 1.0, 1.4]))
    surf = noise(n, S + 1, 150, s=3) * 0.5 + noise(n, S + 2, 40) * 0.5
    col = col * (1 + 0.08 * surf + 0.05 * noise(n, S + 3, 6))[..., None]
    pit = np.clip(dots(n, S + 4, 400, 1.0), 0, 1)
    col = col * (1 - 0.3 * pit)[..., None] * (1 - 0.06 * (1 - inner))[..., None]
    mort = hexc("#a39e92") * (1 + 0.1 * noise(n, S + 5, 300) + 0.06 * noise(n, S + 6, 60))[..., None]
    mort = mort * (1 - 0.25 * np.exp(-np.clip(d, 0, None) / 0.002))[..., None]
    col = mix(mort, col, cov)
    h = -0.003 * (1 - cov) - 0.003 * (1 - inner) * cov + 0.0006 * surf * cov + 0.0004 * noise(n, S + 7, 200) * (1 - cov) - 0.0005 * pit
    rough = 0.85 + 0.05 * surf
    rough = rough * cov + 0.93 * (1 - cov)
    ao = (1 - 0.55 * (1 - cov)) * (1 - 0.15 * (1 - inner) * cov)
    return col, h, rough, ao, 1.0


def concrete(n, S):
    tile = 1.0
    base = hexc("#8f8b83") * 1.05
    big = noise(n, S + 1, 5) * 0.5 + noise(n, S + 2, 14) * 0.5
    mid = noise(n, S + 3, 60)
    fine = noise(n, S + 4, 300, s=2)
    stain = np.clip(noise(n, S + 5, 4, ay=0.6) * 0.8 + noise(n, S + 6, 12), -1, 3)
    streak = noise(n, S + 7, 40, ay=0.12, ax=1.0)           # rain streaks running down
    agg = np.clip(dots(n, S + 8, 6000, 1.1), 0, 1)
    agg2 = np.clip(dots(n, S + 9, 4500, 1.0), 0, 1)
    pit = np.clip(dots(n, S + 10, 700, 1.3), 0, 1)
    v = 1 + 0.05 * big + 0.04 * mid + 0.05 * fine - 0.07 * np.clip(stain, 0, 2) - 0.03 * streak
    col = base * v[..., None]
    col = mix(col, np.broadcast_to(hexc("#b8b4aa"), col.shape), agg * 0.45)
    col = mix(col, np.broadcast_to(hexc("#6b665e"), col.shape), agg2 * 0.4)
    col = col * (1 - 0.45 * pit)[..., None]
    h = 0.0008 * mid + 0.0005 * fine + 0.0004 * (agg + agg2) - 0.0015 * pit + 0.0012 * big
    rough = 0.86 + 0.04 * fine - 0.03 * agg
    ao = 1 - 0.65 * pit - 0.04 * np.clip(stain, 0, 2)
    return col, h, rough, ao, 1.0


def plaster(n, S):
    tile = 1.0
    sw = noise(n, S + 1, 30, ax=0.7) * 0.6 + noise(n, S + 2, 10, ay=0.5) * 0.4
    fine = noise(n, S + 3, 160, s=2.5)
    patch = noise(n, S + 4, 5)
    col = hexc("#e6e1d6") * (1 + 0.012 * sw + 0.008 * fine + 0.012 * patch)[..., None]
    pit = np.clip(dots(n, S + 5, 160, 0.9), 0, 1)
    col = col * (1 - 0.1 * pit)[..., None]
    h = 0.0005 * sw + 0.00025 * fine - 0.0004 * pit
    rough = 0.86 + 0.03 * fine
    ao = 1 - 0.3 * pit
    return col, h, rough, ao, 1.0


def kitchen_tile(n, S):
    tile = 0.6
    px = tile / n
    d, tid, ix, iy, lu, lv = tile_grid(n, tile, 4, 4)
    rng = np.random.default_rng(S)
    tvar = (rng.random(16)[tid] - 0.5) * 2
    jw = 0.004
    cov = cov_mask(d, jw / 2, px)
    pillow = sstep(jw / 2, jw / 2 + 0.003, d)
    base = hexc("#a9b3a4")
    col = base * (1 + 0.05 * tvar + 0.02 * noise(n, S + 1, 30))[..., None]
    col = col * (1 + 0.02 * tvar[..., None] * np.array([-0.5, 0.5, -0.6]))
    spk = np.clip(dots(n, S + 2, 3500, 0.9), 0, 1)
    spk2 = np.clip(dots(n, S + 3, 3000, 0.9), 0, 1)
    col = mix(col, np.broadcast_to(hexc("#6d786b"), col.shape), spk * 0.5)
    col = mix(col, np.broadcast_to(hexc("#cfd5c8"), col.shape), spk2 * 0.45)
    wear = noise(n, S + 4, 50) * 0.5 + noise(n, S + 5, 10) * 0.5
    col = col * (1 + 0.03 * wear)[..., None] * (1 - 0.06 * (1 - pillow))[..., None]
    gcol = hexc("#76786f") * (1 + 0.1 * noise(n, S + 6, 250))[..., None]
    col = mix(gcol, col, cov)
    h = -0.0018 * (1 - cov) - 0.0008 * (1 - pillow) * cov + 0.00008 * noise(n, S + 7, 150) * cov + 0.0001 * noise(n, S + 8, 300) * (1 - cov)
    rough = 0.4 + 0.07 * wear + 0.1 * (spk + spk2) * 0.3
    rough = rough * cov + 0.9 * (1 - cov)
    ao = (1 - 0.4 * (1 - cov)) * (1 - 0.1 * (1 - pillow) * cov)
    return col, h, rough, ao, 1.0


def roof(n, S):
    tile = 1.0
    px = tile / n
    x = (np.arange(n) + 0.5) * px
    xm = (x + 0.25) % 0.5 - 0.25                         # seams at x = 0 and 0.5
    a = np.abs(xm)
    # standing seam: flat pan, a raised trapezoid rib 22 mm wide, 18 mm tall (clamped to be visible in the normal)
    rib = (1 - sstep(0.009, 0.016, a)) * 0.018
    cap = np.exp(-(((a - 0.011) / 0.0025) ** 2))
    prof = rib
    pan = 0.0006 * np.sin(2 * np.pi * (x / 0.5))        # slight pan oil-canning, periodic
    h = np.ones((n, 1)) * (prof + pan)[None, :]
    rain = noise(n, S + 1, 50, ay=0.08, ax=1.0)          # rain streaks down the slope (v)
    weather = noise(n, S + 2, 6) * 0.6 + noise(n, S + 3, 20) * 0.4
    fine = noise(n, S + 4, 130, s=2.5)
    rust = np.clip(dots(n, S + 5, 120, 1.2), 0, 1)
    col = hexc("#a9adad") * (1 + 0.025 * rain + 0.02 * weather + 0.012 * fine)[..., None]
    seamv = np.ones((n, 1)) * (1 + 0.05 * cap - 0.07 * (np.exp(-(((a - 0.019) / 0.003) ** 2))))[None, :]
    col = col * seamv[..., None]
    col = mix(col, np.broadcast_to(hexc("#7a5a45"), col.shape), rust * 0.5)
    h = h + 0.00008 * fine - 0.0003 * rust
    rough = 0.42 + 0.06 * rain + 0.05 * weather + 0.25 * rust
    ao = np.ones((n, 1)) * (1 - 0.35 * np.exp(-(((a - 0.019) / 0.004) ** 2)))[None, :] * (1 - 0.3 * rust)
    return col, h, rough, ao, 1.0


SETS = {
    "floor_checker": (1.0, 1024, floor_checker),
    "floor_border": (0.5, 512, floor_border),
    "formica": (0.6, 1024, formica),
    "mahogany": (1.2, 1024, mahogany),
    "vinyl_red": (0.25, 1024, vinyl_red),
    "steel_brushed": (0.5, 1024, steel_brushed),
    "aluminum_ribbed": (0.05, 256, aluminum_ribbed),
    "enamel_red": (1.0, 1024, lambda n, S: enamel(n, S, "#9c2228", "red")),
    "enamel_cream": (1.0, 1024, lambda n, S: enamel(n, S, "#efe2c2", "cream")),
    "wall_tile": (0.4, 1024, wall_tile),
    "ceiling": (1.22, 1024, ceiling),
    "brick": (1.0, 1024, brick),
    "concrete": (1.0, 1024, concrete),
    "plaster": (1.0, 512, plaster),
    "kitchen_tile": (0.6, 1024, kitchen_tile),
    "roof": (1.0, 512, roof),
}


# ---------------------------------------------------------------- output
def to8(a):
    return (np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8)


def build(name):
    tile, n, fn = SETS[name]
    S = 1000 + 37 * list(SETS).index(name)
    col, h, rough, ao, k = fn(n, S)
    nm = normal_map(h, tile, k)
    D = to8(col)
    N = to8(nm)
    R = np.stack([to8(rough), to8(ao), np.zeros((n, n), np.uint8)], -1)
    Image.fromarray(D).save(os.path.join(OUT, name + "_D.jpg"), quality=92, subsampling=0)
    Image.fromarray(N).save(os.path.join(OUT, name + "_N.png"), optimize=True)
    Image.fromarray(R).save(os.path.join(OUT, name + "_R.jpg"), quality=95, subsampling=0)
    return D, N, R


def sheet(imgs):
    try:
        font = ImageFont.load_default(size=13)
    except TypeError:
        font = ImageFont.load_default()
    T, TT = 160, 128
    cw, ch = 3 * T + 2 * TT + 5 * 6, 2 * TT + 24
    ncol = 2
    names = list(imgs)
    nrow = (len(names) + ncol - 1) // ncol
    W, H = ncol * cw + 6, nrow * ch + 6
    im = Image.new("RGB", (W, H), (40, 40, 44))
    dr = ImageDraw.Draw(im)
    for i, nm in enumerate(names):
        D, N, R = imgs[nm]
        x0, y0 = 6 + (i % ncol) * cw, 6 + (i // ncol) * ch
        tile, n, _ = SETS[nm]
        dr.text((x0, y0), "%s  %.2f m  %dpx  (D, N, R=rough/ao, D 2x2)" % (nm, tile, n), fill=(235, 235, 235), font=font)
        y = y0 + 20
        for j, a in enumerate((D, N, R)):
            im.paste(Image.fromarray(a).resize((T, T), Image.LANCZOS), (x0 + j * (T + 6), y))
        t = Image.fromarray(D).resize((TT, TT), Image.LANCZOS)
        for a in range(2):
            for b in range(2):
                im.paste(t, (x0 + 3 * (T + 6) + b * TT, y + a * TT))
    os.makedirs(os.path.dirname(SHEET), exist_ok=True)
    im.save(SHEET, quality=90)


def main():
    os.makedirs(OUT, exist_ok=True)
    want = sys.argv[1:] or list(SETS)
    imgs = {}
    for nm in SETS:
        if nm in want:
            imgs[nm] = build(nm)
            print("made", nm, flush=True)
        else:                                               # keep the sheet whole when only some sets are rebuilt
            imgs[nm] = tuple(np.array(Image.open(os.path.join(OUT, nm + s))) for s in ("_D.jpg", "_N.png", "_R.jpg"))
    sheet(imgs)
    tot = sum(os.path.getsize(os.path.join(OUT, f)) for f in os.listdir(OUT))
    print("textures: %.1f MB; sheet %s" % (tot / 1e6, SHEET))


if __name__ == "__main__":
    main()
