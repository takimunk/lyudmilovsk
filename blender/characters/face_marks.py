"""Finds a generated character's eyes and mouth on a front, unlit picture of its face (numpy + scipy, no Blender).

    python3 blender/characters/face_marks.py <face.png> <centre_z> <scale_m> [debug.png]

The picture is an orthographic front view (the model faces the camera, -Y in Blender), `scale_m` metres wide,
centred on x = 0 and z = centre_z (rig_character.py renders it). Prints JSON, in model metres (x right of the
picture = the character's right... see below), z up:

    eyes:  [{"x", "z", "w", "h"}, {...}]   iris centres and the eye opening's width and height; [0] is the
                                           character's left (+X in Blender, the picture's right)
    mouth: {"x", "z", "w"}                 the lip line's middle and the mouth's width
    ipd:   distance between the irises

Eyes: the two darkest iris-sized blobs on lighter surroundings (a difference of Gaussians, so a dark fringe of hair
doesn't count) left and right of the middle, in the face's upper half. The opening: along the iris's row and column,
how far the colour stays away from the skin's (the cheeks' median). Mouth: the darkest row of the face's middle strip
0.8-1.45 IPD under the eyes (between the lips), its width where that row stays darker than the skin.
"""
import json
import sys

import numpy as np
from PIL import Image
from scipy import ndimage as nd


def marks(png, centre_z, scale, debug=None):
    img = np.asarray(Image.open(png).convert("RGB")).astype(np.float32) / 255.0
    n = img.shape[0]
    px = scale / n                                         # metres a pixel
    lum = img @ np.array([0.299, 0.587, 0.114], np.float32)

    def to_m(r, c):
        return (c - n / 2) * px, centre_z + (n / 2 - r) * px

    def to_px(x, z):
        return n / 2 - (z - centre_z) / px, n / 2 + x / px

    s1, s2 = 0.0022 / px, 0.007 / px                       # iris-sized dark blob against the eye's surroundings
    dog = nd.gaussian_filter(lum, s2) - nd.gaussian_filter(lum, s1)
    rows = slice(int(n * 0.15), int(n * 0.72))              # round the donor's eye line (the picture's middle)
    eyes = []
    for side in (1, -1):                                   # +1: the picture's right = the character's left
        cols = slice(int(n / 2 + 0.016 / px), int(n / 2 + 0.06 / px)) if side > 0 else slice(int(n / 2 - 0.06 / px), int(n / 2 - 0.016 / px))
        win = dog[rows, cols]
        r, c = np.unravel_index(np.argmax(win), win.shape)
        eyes.append([r + rows.start, c + cols.start])
    # both irises on one level: a tilted head is rare, a wrong blob (a nostril, an earring) is not
    if abs(eyes[0][0] - eyes[1][0]) > 0.012 / px:
        better = 0 if dog[tuple(eyes[0])] > dog[tuple(eyes[1])] else 1
        r0 = eyes[better][0]
        o = 1 - better
        side = 1 if o == 0 else -1
        cols = slice(int(n / 2 + 0.016 / px), int(n / 2 + 0.06 / px)) if side > 0 else slice(int(n / 2 - 0.06 / px), int(n / 2 - 0.016 / px))
        rr = slice(int(r0 - 0.006 / px), int(r0 + 0.006 / px))
        win = dog[rr, cols]
        r, c = np.unravel_index(np.argmax(win), win.shape)
        eyes[o] = [r + rr.start, c + cols.start]
    ipd = abs(eyes[0][1] - eyes[1][1]) * px
    eye_r = (eyes[0][0] + eyes[1][0]) / 2
    # the skin: the cheeks under the eyes
    cr = slice(int(eye_r + 0.35 * ipd / px), int(eye_r + 0.55 * ipd / px))
    cheeks = np.concatenate([img[cr, int(e[1] - 0.1 * ipd / px):int(e[1] + 0.1 * ipd / px)].reshape(-1, 3) for e in eyes])
    skin = np.median(cheeks, 0)
    far = np.linalg.norm(nd.gaussian_filter(img, (1.2, 1.2, 0)) - skin, axis=2)
    thr = 0.5 * np.median(np.linalg.norm(cheeks - skin, axis=1)) + 0.09
    out = {"ipd": round(float(ipd), 4), "eyes": []}
    for r, c in eyes:
        def run(dr, dc, limit):
            k = 0
            while k < limit and far[int(r + dr * k), int(c + dc * k)] > thr:
                k += 1
            return k
        lim = int(0.35 * ipd / px)
        w = run(0, 1, lim) + run(0, -1, lim)
        h = run(1, 0, lim // 2) + run(-1, 0, lim // 2)
        x, z = to_m(r, c)
        rad = int(0.0035 / px)                             # the iris's colour: the darker part of a 3.5 mm disc
        yy, xx = np.mgrid[-rad:rad + 1, -rad:rad + 1]
        disc = img[int(r) - rad:int(r) + rad + 1, int(c) - rad:int(c) + rad + 1][(yy * yy + xx * xx) <= rad * rad]
        dl = disc @ np.array([0.299, 0.587, 0.114])
        iris = np.median(disc[dl <= np.percentile(dl, 60)], 0)
        out["eyes"].append({"x": round(float(x), 4), "z": round(float(z), 4),
                            "iris": [round(float(v), 3) for v in iris],
                            "w": round(float(np.clip(w * px, 0.30 * ipd, 0.60 * ipd)), 4),
                            "h": round(float(np.clip(h * px, 0.12 * ipd, 0.30 * ipd)), 4)})
    # the mouth: the darkest row of the middle strip under the nose
    mid = int(n / 2 + (out["eyes"][0]["x"] + out["eyes"][1]["x"]) / 2 / px)
    half = int(0.12 * ipd / px)
    rr = np.arange(min(n - 3, int(eye_r + 0.8 * ipd / px)), min(n - 2, int(eye_r + 1.45 * ipd / px)))
    prof = nd.gaussian_filter1d(lum[:, mid - half:mid + half].mean(1), 1.5)[rr]
    r = int(rr[np.argmin(prof)])
    row = nd.gaussian_filter1d(lum[r - 1:r + 2].mean(0), 2.0)
    sk = float(skin @ np.array([0.299, 0.587, 0.114]))
    lo = hi = mid
    while lo > max(1, mid - 0.7 * ipd / px) and row[lo] < sk - 0.06:
        lo -= 1
    while hi < min(n - 2, mid + 0.7 * ipd / px) and row[hi] < sk - 0.06:
        hi += 1
    x, z = to_m(r, (lo + hi) / 2)
    out["mouth"] = {"x": round(float(x), 4), "z": round(float(z), 4),
                    "w": round(float(np.clip((hi - lo) * px, 0.55 * ipd, 1.0 * ipd)), 4)}
    if debug:
        from PIL import ImageDraw
        im = Image.open(png).convert("RGB")
        d = ImageDraw.Draw(im)
        for e in out["eyes"]:
            r, c = to_px(e["x"], e["z"])
            a, b = e["w"] / px / 2, e["h"] / px / 2
            d.ellipse([c - a, r - b, c + a, r + b], outline=(0, 255, 0), width=2)
            d.ellipse([c - 4, r - 4, c + 4, r + 4], fill=(255, 0, 0))
        m = out["mouth"]
        r, c = to_px(m["x"], m["z"])
        d.line([c - m["w"] / px / 2, r, c + m["w"] / px / 2, r], fill=(0, 128, 255), width=3)
        im.save(debug)
    return out


if __name__ == "__main__":
    a = sys.argv[1:]
    print(json.dumps(marks(a[0], float(a[1]), float(a[2]), a[3] if len(a) > 3 else None)))
