"""The world map as the hero drew it in his diary (the map spread, docs/diary.md): pen and coloured pencil on nothing, so the
diary's own paper shows through. Same frame and scale as tools/ui_map.py (the places and the game's map view use one frame),
no words in the picture (the game writes the names in the current language).

  python3 tools/ui_map_ink.py      writes textures/ui/map_ink.png (RGBA)
then unreal/scripts/import_diary_assets.py imports it as /Game/UI/Diary/T_MapInk.

Drawn: the shore as a firm ink line with the old cartographers' ripple lines along it in blue pencil, sand stippled, brown
pencil contours every 5 m (pressed harder every 25), hachures down the cliffs, the forest as little scalloped crowns in green
pencil over a light wash, the path dashed, the alley's trees as dots, the cable car with its towers, the houses hatched.
"""
import math
import os
import random
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import ui_map as U  # noqa: E402
import world_map as W  # noqa: E402

SS = 2                     # drawn twice as big, then halved: smooth strokes
INK = (34, 38, 64)         # a blue-black fountain pen
GRAPHITE = (70, 66, 70)
BROWN = (126, 84, 52)
BLUE = (64, 108, 160)
GREEN = (64, 112, 60)
OCHRE = (176, 132, 64)
RED = (172, 52, 40)


def cached_heights(w, h):
    path = os.path.join(ROOT, "textures", "ui", ".map_heights_%dx%d.npy" % (w, h))
    if os.path.exists(path) and os.path.getmtime(path) > os.path.getmtime(os.path.join(HERE, "world_map.py")):
        return np.load(path)
    H = np.array(U.heights(w, h), np.float32)
    try:
        np.save(path, H)
    except OSError:
        pass
    return H


def distance_out(land, limit):
    """Rough distance (pixels, 8-neighbour rings) from the land, up to limit; numpy only."""
    dist = np.full(land.shape, limit + 1, np.float32)
    grown = land.copy()
    dist[grown] = 0
    for r in range(1, limit + 1):
        g = grown.copy()
        g[1:] |= grown[:-1]
        g[:-1] |= grown[1:]
        g[:, 1:] |= grown[:, :-1]
        g[:, :-1] |= grown[:, 1:]
        if r % 3 == 0:     # every third ring also diagonally: rounder rings
            g[1:, 1:] |= grown[:-1, :-1]
            g[:-1, :-1] |= grown[1:, 1:]
            g[1:, :-1] |= grown[:-1, 1:]
            g[:-1, 1:] |= grown[1:, :-1]
        dist[g & ~grown] = r
        grown = g
    return dist


def main(out_png):
    w = int((W.X_MAX - W.X_MIN) / U.PX)
    h = int((W.Y_MAX - W.Y_MIN) / U.PX)
    print("ink map %d x %d px" % (w, h))
    H = cached_heights(w, h)
    sea = H < W.SEA_Z
    from multiprocessing import Pool
    with Pool() as pool:
        beach = np.array(pool.map(U._beach_row, range(h), chunksize=16), bool)
    gy, gx = np.gradient(H, U.PX)
    slope = np.hypot(gx, gy)

    W2, H2 = w * SS, h * SS
    img = Image.new("RGBA", (W2, H2), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    rnd = random.Random(11)

    def px(x, y):
        return (W.X_MAX - x) / U.PX * SS, (W.Y_MAX - y) / U.PX * SS

    def wash(mask, color, alpha, blur=3):
        """A light coloured-pencil wash over a mask: grainy, darker at its edge."""
        m = Image.fromarray((mask * 255).astype(np.uint8)).resize((W2, H2), Image.BILINEAR).filter(ImageFilter.GaussianBlur(blur * SS))
        a = np.asarray(m, np.float32) / 255
        grain = np.random.default_rng(3).random((H2, W2)).astype(np.float32)
        edge = np.clip(1 - np.abs(a - 0.5) * 2, 0, 1)
        k = np.clip(a * (0.65 + 0.35 * grain) + edge * 0.5, 0, 1) * alpha
        layer = np.zeros((H2, W2, 4), np.uint8)
        layer[..., :3] = color
        layer[..., 3] = (k * 255).astype(np.uint8)
        img.alpha_composite(Image.fromarray(layer, "RGBA"))

    # --- washes
    wash(beach & ~sea, OCHRE, 0.18, 2)
    forest_mask = np.zeros((h, w), bool)
    cell = 40.0
    tiles = W.forests()
    yy = W.Y_MAX - (np.arange(h) + 0.5) * U.PX
    xx = W.X_MAX - (np.arange(w) + 0.5) * U.PX
    X, Y = np.meshgrid(xx, yy)
    for t in tiles:
        fx, fy, hx, hy, mix, _c, _s, z0, z1, _sd = t
        if mix == "town":
            continue
        i0, i1 = int((W.X_MAX - (fx + hx)) / U.PX), int((W.X_MAX - (fx - hx)) / U.PX) + 1
        j0, j1 = int((W.Y_MAX - (fy + hy)) / U.PX), int((W.Y_MAX - (fy - hy)) / U.PX) + 1
        i0, j0 = max(i0, 0), max(j0, 0)
        sub = H[j0:j1, i0:i1]
        forest_mask[j0:j1, i0:i1] |= (sub >= z0) & (sub <= z1) & (slope[j0:j1, i0:i1] < 0.8)
    # a ragged edge, as in the painted map
    n = np.vectorize(lambda x, y: U.noise(x, y, 22.0))(X[::4, ::4], Y[::4, ::4])
    n = np.asarray(Image.fromarray((n * 255).astype(np.uint8)).resize((w, h), Image.BILINEAR), np.float32) / 255
    soft = np.asarray(Image.fromarray(forest_mask.astype(np.uint8) * 255).filter(ImageFilter.GaussianBlur(9)), np.float32) / 255
    forest = (soft + (n - 0.5) * 0.6) > 0.55
    wash(forest, GREEN, 0.14, 3)

    # --- contours: brown pencil
    k = np.floor(H / U.CONTOUR)
    edge = ((k != np.roll(k, 1, 1)) | (k != np.roll(k, 1, 0))) & ~sea
    thick = edge & (k % 5 == 0)
    js, is_ = np.nonzero(edge)
    for j, i in zip(js, is_):
        a = 150 if thick[j, i] else 80
        d.ellipse((i * SS - 1.2, j * SS - 1.2, i * SS + 1.2, j * SS + 1.2), fill=BROWN + (a,))

    # --- the cliffs: hachures down the slope
    js, is_ = np.nonzero((slope > 0.9) & ~sea)
    for j, i in zip(js, is_):
        if rnd.random() > 0.12:
            continue
        g = np.array([-gx[j, i], gy[j, i]])        # the image's x runs west, y south
        g = g / (np.linalg.norm(g) + 1e-6)
        L = 5 + 6 * min(1.0, (slope[j, i] - 0.9))
        a, b = i * SS, j * SS
        # downhill in the image: heights fall toward -grad (x: map x falls to the right)
        dx, dy = g[0], g[1]
        d.line((a, b, a - dx * L * SS * 0.5, b - dy * L * SS * 0.5), fill=GRAPHITE + (190,), width=3)

    # --- the shore: ripple lines in blue pencil, offset out to sea, then the coast in ink
    dist = distance_out(~sea, 48)   # pixels from the land, at sea
    # a blue wash strongest along the shore, fading out to sea
    near = np.clip(1 - dist / 48.0, 0, 1) * sea
    m = np.zeros((h, w, 4), np.uint8)
    m[..., :3] = BLUE
    m[..., 3] = (np.clip(0.1 + 0.25 * near, 0, 1) * sea * 255).astype(np.uint8)
    img.alpha_composite(Image.fromarray(m, "RGBA").resize((W2, H2), Image.BILINEAR))
    for r in (4, 9, 15, 23, 33, 45):
        ring = (dist >= r) & (dist < r + 1) & sea
        js, is_ = np.nonzero(ring)
        fade = max(0.3, 1 - r / 56)
        for j, i in zip(js, is_):
            if rnd.random() < 0.9:
                d.ellipse((i * SS - 1.3, j * SS - 1.3, i * SS + 1.3, j * SS + 1.3), fill=BLUE + (int(200 * fade),))
    # the open sea: short horizontal wave strokes
    for j in range(6, h, 14):
        i = rnd.randint(0, 20)
        while i < w:
            L = rnd.randint(10, 26)
            if sea[j, min(i + L, w - 1)] and sea[j, i] and dist[j, i] > 47 and rnd.random() < 0.5:
                a, b = i * SS, j * SS
                pts = [(a + t * SS, b + math.sin(t * 0.5) * 2.2) for t in range(0, L, 2)]
                d.line(pts, fill=BLUE + (150,), width=2)
            i += L + rnd.randint(14, 40)
    coast = sea & ~(np.roll(sea, 1, 0) & np.roll(sea, -1, 0) & np.roll(sea, 1, 1) & np.roll(sea, -1, 1))
    js, is_ = np.nonzero(coast)
    for j, i in zip(js, is_):
        d.ellipse((i * SS - 2, j * SS - 2, i * SS + 2, j * SS + 2), fill=INK + (235,))
    # sand: stipple
    js, is_ = np.nonzero(beach & ~sea)
    for j, i in zip(js, is_):
        if rnd.random() < 0.12:
            a, b = i * SS + rnd.uniform(-1, 1), j * SS + rnd.uniform(-1, 1)
            d.ellipse((a - 0.9, b - 0.9, a + 0.9, b + 0.9), fill=OCHRE + (190,))

    # --- the forest: scalloped crowns on a loose grid, jittered
    step = 15
    for j in range(0, h, step):
        for i in range(0, w, step):
            jj, ii = j + rnd.randint(0, step - 1), i + rnd.randint(0, step - 1)
            if jj >= h or ii >= w or not forest[jj, ii] or rnd.random() < 0.3:
                continue
            a, b = ii * SS, jj * SS
            r = rnd.uniform(7.0, 10.0) * SS / 2
            # three bumps over a short trunk
            for k2 in range(3):
                ang = math.pi * (0.15 + 0.35 * k2)
                cx, cy = a + math.cos(ang) * r * 0.9 - r * 0.45, b - math.sin(ang) * r * 0.6
                d.arc((cx - r * 0.7, cy - r * 0.7, cx + r * 0.7, cy + r * 0.7), 180, 360, fill=GREEN + (225,), width=3)
            d.line((a, b + r * 0.2, a, b + r * 0.9), fill=GREEN + (170,), width=2)
    for x, y, *_ in W.alley_trees():
        a, b = px(x, y)
        d.ellipse((a - 3.5, b - 3.5, a + 3.5, b + 3.5), outline=GREEN + (230,), width=2)

    # --- the promenade (a double line), the path (dashes), the club
    pts = [px(W.shore_x(y) - W.BEACH_W + 5.0, y) for y in range(int(W.PROMENADE_Y[0]), int(W.PROMENADE_Y[1]) + 1, 10)]
    for off in (-3, 3):
        d.line([(p[0] + off, p[1]) for p in pts], fill=GRAPHITE + (200,), width=2)
    path = [px(x, y) for x, y in W.path_points(2.0)]
    for k2 in range(0, len(path) - 1, 3):
        d.line((path[k2], path[min(k2 + 1, len(path) - 1)]), fill=INK + (220,), width=3)
    a, b = px(W.DOOR_X, 0.0)
    d.rectangle((a - 12, b - 4, a + 12, b + 8), outline=INK + (240,), width=3)
    for yv in range(0, int(W.CORRIDOR_END_Y), -14):
        d.line((px(W.DOOR_X, yv), px(W.DOOR_X, yv - 7)), fill=GRAPHITE + (170,), width=3)

    # --- the cable car: a firm line with tower ticks; the stations as boxes filled in red pencil
    ends = [px(*W.STATION_UP), px(*W.STATION_DOWN)]
    d.line(ends, fill=INK + (230,), width=3)
    ux, uy = ends[1][0] - ends[0][0], ends[1][1] - ends[0][1]
    L = math.hypot(ux, uy) or 1
    nx, ny = -uy / L, ux / L
    for tx, ty, *_ in W.cable_car()["towers"]:
        a, b = px(tx, ty)
        d.line((a - nx * 8, b - ny * 8, a + nx * 8, b + ny * 8), fill=INK + (230,), width=3)
    for sx, sy in (W.STATION_UP, W.STATION_DOWN):
        a, b = px(sx, sy)
        d.rectangle((a - 13, b - 9, a + 13, b + 9), fill=RED + (120,), outline=INK + (240,), width=3)

    # --- houses: outlined and hatched
    for poly in U.academ_footprints():
        P = [px(x, y) for x, y in poly]
        d.polygon(P, outline=INK + (240,), width=3)
        xs, ys = [p[0] for p in P], [p[1] for p in P]
        mask = Image.new("L", (W2, H2), 0)
        ImageDraw.Draw(mask).polygon(P, fill=255)
        hatch = Image.new("RGBA", (W2, H2), (0, 0, 0, 0))
        hd = ImageDraw.Draw(hatch)
        for c in range(int(min(xs) - (max(ys) - min(ys))), int(max(xs)), 7):
            hd.line((c, max(ys), c + (max(ys) - min(ys)), min(ys)), fill=BROWN + (200,), width=2)
        img.paste(hatch, (0, 0), Image.composite(hatch, Image.new("RGBA", (W2, H2)), mask))

    out = img.resize((w, h), Image.LANCZOS)
    os.makedirs(os.path.dirname(out_png), exist_ok=True)
    out.save(out_png, optimize=True)
    print("wrote", out_png)


if __name__ == "__main__":
    main(os.path.join(ROOT, "textures", "ui", "map_ink.png"))
