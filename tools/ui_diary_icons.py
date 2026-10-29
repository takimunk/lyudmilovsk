"""The diary's item pictures: every inventory icon (textures/localized/<lang>/icon_<id>.jpg, a photo-like picture on a dark
cloth) redrawn as a pencil sketch on nothing, to lie on the diary's paper: graphite outlines and hatching where the thing is
dark, a light wash of coloured pencil in its own colours. No generation: a filter, so it reruns the same for any new icon.

  python3 tools/ui_diary_icons.py [--lang ru] [id ...]   writes textures/localized/<lang>/sketch_<id>.png (RGBA, 256 px)
then unreal/scripts/import_diary_assets.py imports them as /Game/Inventory/Localized/<lang>/T_Sketch_<id>.

Needs numpy, Pillow and opencv (pip install opencv-python-headless). The cut-out: GrabCut seeded with the border as the
cloth and the middle as the thing; the cloth is a warm dark grey, so things that are themselves near black (trousers, the
lamp) keep their outline from the edges, not from the colour.
"""
import argparse
import glob
import os

import cv2
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = 256
GRAPHITE = np.array([46, 44, 52], np.float32)   # a soft 2B pencil


def cut_out(rgb):
    """0..1 mask of the thing on the cloth."""
    h, w = rgb.shape[:2]
    mask = np.full((h, w), cv2.GC_PR_BGD, np.uint8)
    b = int(w * 0.04)
    mask[:b, :] = mask[-b:, :] = cv2.GC_BGD
    mask[:, :b] = mask[:, -b:] = cv2.GC_BGD
    # what differs from the cloth's colour is probably the thing
    # the thing is where the picture has detail: the cloth and its soft light have none, so the filled outline of the
    # strong edges is the first guess (a colour test would take the lit cloth round a bottle for part of it)
    gray = cv2.cvtColor(rgb, cv2.COLOR_RGB2GRAY)
    edges = cv2.Canny(cv2.GaussianBlur(gray, (0, 0), 1.0), 40, 110)
    edges[:b, :] = edges[-b:, :] = 0
    edges[:, :b] = edges[:, -b:] = 0
    blob = cv2.morphologyEx(cv2.dilate(edges, np.ones((3, 3), np.uint8)), cv2.MORPH_CLOSE, np.ones((25, 25), np.uint8))
    cnts, _ = cv2.findContours(blob, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
    guess = np.zeros((h, w), np.uint8)
    if cnts:
        # the edges' convex hull: a smooth apple has an outline but nothing inside it; GrabCut takes the hull's cloth back out
        big = max(cv2.contourArea(c) for c in cnts)
        pts = np.concatenate([c for c in cnts if cv2.contourArea(c) > big * 0.05])
        cv2.drawContours(guess, [cv2.convexHull(pts)], -1, 1, -1)
    filled = np.zeros((h, w), np.uint8)
    cv2.drawContours(filled, cnts, -1, 1, -1)
    near = cv2.dilate(guess, np.ones((15, 15), np.uint8))
    mask[near == 0] = cv2.GC_BGD
    mask[(guess > 0) & (mask != cv2.GC_BGD)] = cv2.GC_PR_FGD
    mask[(cv2.erode(filled, np.ones((21, 21), np.uint8)) > 0) & (mask != cv2.GC_BGD)] = cv2.GC_FGD
    bgd, fgd = np.zeros((1, 65), np.float64), np.zeros((1, 65), np.float64)
    cv2.grabCut(cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR), mask, None, bgd, fgd, 6, cv2.GC_INIT_WITH_MASK)
    m = np.where((mask == cv2.GC_FGD) | (mask == cv2.GC_PR_FGD), 1, 0).astype(np.uint8)
    # the biggest pieces only (crumbs of cloth go), holes filled
    n, lab, stats, _ = cv2.connectedComponentsWithStats(m)
    if n > 1:
        big = stats[1:, cv2.CC_STAT_AREA].max()
        keep = [i for i in range(1, n) if stats[i, cv2.CC_STAT_AREA] > big * 0.08]
        m = np.isin(lab, keep).astype(np.uint8)
    m = cv2.morphologyEx(m, cv2.MORPH_CLOSE, np.ones((9, 9), np.uint8))
    cnts, _ = cv2.findContours(m, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
    filled = np.zeros_like(m)
    cv2.drawContours(filled, cnts, -1, 1, -1)
    soft = cv2.GaussianBlur(filled.astype(np.float32), (0, 0), 1.2)
    return soft, cnts


def hatch(h, w, angle, spacing, seed):
    """Parallel pencil strokes 0..1, each a little wavy and of its own pressure."""
    rng = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    a = np.deg2rad(angle)
    u = xx * np.cos(a) + yy * np.sin(a)
    v = -xx * np.sin(a) + yy * np.cos(a)
    u = u + 1.4 * np.sin(v / 37.0 + rng.uniform(0, 6))
    phase = (u / spacing) % 1.0
    line = np.clip(1.0 - np.abs(phase - 0.5) * 2 * spacing / 1.6, 0, 1)
    idx = np.floor(u / spacing).astype(np.int64)
    pressure = 0.55 + 0.45 * np.random.default_rng(seed + 1).random(idx.max() - idx.min() + 2)[idx - idx.min()]
    # strokes fade at random lengths along themselves
    breaks = 0.5 + 0.5 * np.sin(v / (23 + 9 * ((idx * 7919) % 5)) + idx * 1.7)
    return line * pressure * np.clip(breaks * 1.6, 0, 1)


def sketch(path, seed):
    src = np.array(Image.open(path).convert("RGB"))
    mask, cnts = cut_out(src)
    # crop to the thing, square, with a margin
    ys, xs = np.nonzero(mask > 0.5)
    if len(xs) == 0:
        return None
    x0, x1, y0, y1 = xs.min(), xs.max(), ys.min(), ys.max()
    c = ((x0 + x1) / 2, (y0 + y1) / 2)
    half = max(x1 - x0, y1 - y0) / 2 * 1.1 + 6
    M = np.float32([[OUT / (2 * half), 0, OUT / 2 - c[0] * OUT / (2 * half)], [0, OUT / (2 * half), OUT / 2 - c[1] * OUT / (2 * half)]])
    rgb = cv2.warpAffine(src, M, (OUT, OUT), flags=cv2.INTER_AREA, borderMode=cv2.BORDER_REPLICATE).astype(np.float32) / 255
    m = cv2.warpAffine(mask, M, (OUT, OUT), flags=cv2.INTER_LINEAR)
    lum = rgb @ np.array([0.3, 0.59, 0.11], np.float32)
    # tone: dark parts get hatched, the darker the more layers
    tone = 1.0 - np.clip((lum - 0.08) / 0.8, 0, 1)
    tone = cv2.GaussianBlur(tone, (0, 0), 2.0)
    h1 = hatch(OUT, OUT, 38, 4.2, seed)
    h2 = hatch(OUT, OUT, -52, 4.6, seed + 10)
    h3 = hatch(OUT, OUT, 8, 3.6, seed + 20)
    shade = h1 * np.clip((tone - 0.25) * 2.2, 0, 1) * 0.55 + h2 * np.clip((tone - 0.5) * 2.5, 0, 1) * 0.5 + h3 * np.clip((tone - 0.75) * 3, 0, 1) * 0.4
    # inner lines: edges of the picture itself, as a sketcher picks them
    g = (lum * 255).astype(np.uint8)
    inner = cv2.Canny(cv2.GaussianBlur(g, (0, 0), 1.3), 25, 70).astype(np.float32) / 255
    inner = cv2.GaussianBlur(inner, (0, 0), 0.6) * 0.75
    # the outline: the cut-out's edge, drawn twice with a slight wobble like a quick hand
    outline = np.zeros((OUT, OUT), np.float32)
    rng = np.random.default_rng(seed)
    mm = (m > 0.5).astype(np.uint8)
    cs, _ = cv2.findContours(mm, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
    for k in range(2):
        layer = np.zeros((OUT * 2, OUT * 2), np.uint8)
        for cnt in cs:
            p = cnt[:, 0, :].astype(np.float32)
            if len(p) < 8:
                continue
            t = np.arange(len(p), dtype=np.float32)
            wob = np.stack([np.sin(t / 19 + rng.uniform(0, 6)), np.cos(t / 23 + rng.uniform(0, 6))], 1) * (0.7 + 0.5 * k)
            q = ((p + wob) * 2).astype(np.int32)
            # a stroke now and then lifts off the paper
            step = max(len(q) // rng.integers(3, 6), 8)
            for s in range(0, len(q), step):
                seg = q[s:s + step + rng.integers(-3, 4)]
                if len(seg) > 2:
                    cv2.polylines(layer, [seg.reshape(-1, 1, 2)], False, 255, 3 if k == 0 else 2, cv2.LINE_AA)
        outline = np.maximum(outline, cv2.resize(layer, (OUT, OUT), interpolation=cv2.INTER_AREA).astype(np.float32) / 255 * (0.95 if k == 0 else 0.6))
    graphite = np.clip(np.maximum(np.maximum(outline, inner * m), shade * m), 0, 1)
    # a wash of coloured pencil in the thing's colours, paler and grainy
    grain = cv2.GaussianBlur(np.random.default_rng(seed + 5).random((OUT, OUT)).astype(np.float32), (0, 0), 0.7)
    sat = rgb - lum[..., None]
    wash_col = np.clip(lum[..., None] * 0.35 + 0.55 + sat * 1.4, 0, 1)
    wash_a = m * (0.42 + 0.3 * grain) * np.clip(0.35 + np.abs(sat).max(axis=2) * 3, 0, 1)
    # compose over nothing: colour = wash under graphite
    a_g = graphite
    col = (wash_col * wash_a[..., None] * (1 - a_g[..., None]) + GRAPHITE / 255 * a_g[..., None])
    alpha = np.clip(a_g + wash_a * (1 - a_g), 0, 1)
    col = np.where(alpha[..., None] > 1e-4, col / np.maximum(alpha[..., None], 1e-4), 0)
    out = np.dstack([np.clip(col, 0, 1) * 255, alpha * 255]).astype(np.uint8)
    return Image.fromarray(out, "RGBA")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lang", default=None, help="one language (default: every one that has icons)")
    ap.add_argument("ids", nargs="*")
    a = ap.parse_args()
    langs = [a.lang] if a.lang else sorted(os.listdir(os.path.join(ROOT, "textures", "localized")))
    for lang in langs:
        for path in sorted(glob.glob(os.path.join(ROOT, "textures", "localized", lang, "icon_*.jpg"))):
            item = os.path.basename(path)[len("icon_"):-4]
            if a.ids and item not in a.ids:
                continue
            with open(path, "rb") as f:
                if f.read(7) == b"version":   # an LFS pointer: the picture is not here
                    print("skip (not pulled)", path)
                    continue
            img = sketch(path, seed=sum(map(ord, item)))
            if img is None:
                print("nothing found in", path)
                continue
            dst = os.path.join(ROOT, "textures", "localized", lang, "sketch_%s.png" % item)
            img.save(dst, optimize=True)
            print("wrote", os.path.relpath(dst, ROOT))


if __name__ == "__main__":
    main()
