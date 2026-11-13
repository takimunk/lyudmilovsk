"""Draws the North from above without Unreal: shaded relief of the macro shape (or the built ground when
Saved/World/terrain_out.bin exists and --built is given), the masks (snow white, scoria dark red, lava black, steaming
ground yellow), the trail, and the places. python3 tools/north_preview.py out.png [px metres] [--built]"""
import math
import sys

from PIL import Image, ImageDraw

import north as N
import world_map as W


def render(out, px=6.0, built=False, y0=1900.0):
    import terrain as T
    hfn = T.ground_z if built else W.height
    w, h = int((W.X_MAX - W.X_MIN) / px), int((W.Y_MAX + 60 - y0) / px)
    zs = [[hfn(W.X_MAX - (i + 0.5) * px, W.Y_MAX + 60 - (j + 0.5) * px) for i in range(w + 1)] for j in range(h + 1)]
    img = Image.new("RGB", (w, h))
    pix = img.load()
    for j in range(h):
        y = W.Y_MAX + 60 - (j + 0.5) * px
        for i in range(w):
            x = W.X_MAX - (i + 0.5) * px
            z = zs[j][i]
            gx, gy = (zs[j][i + 1] - z) / px, (zs[j + 1][i] - z) / px
            shade = max(0.25, min(1.3, 0.85 + 1.6 * (gx - gy)))          # lit from the south-west (image left-down)
            if W.inland(x, y) <= 0:
                c = (70, 110, 150)
            elif W.is_beach(x, y):
                c = (205, 190, 150)
            else:
                t = max(0.0, min(1.0, (z + 40) / 150.0))
                c = (int(95 + 70 * t), int(140 - 10 * t), int(80 + 20 * t))
                sn, sc, lv, su = N.snow(x, y), N.scoria(x, y), N.lava(x, y), N.sulfur(x, y)
                mix = lambda a, b, k: tuple(int(a[q] + (b[q] - a[q]) * k) for q in range(3))
                c = mix(c, (92, 62, 58), sc)
                c = mix(c, (45, 42, 44), lv)
                c = mix(c, (215, 200, 120), su)
                c = mix(c, (245, 248, 252), min(1.0, sn * 2))
                c = tuple(int(v * shade) for v in c)
            pix[i, j] = tuple(max(0, min(255, v)) for v in c)
    d = ImageDraw.Draw(img)
    to = lambda x, y: ((W.X_MAX - x) / px, (W.Y_MAX + 60 - y) / px)
    for t in T.tracks():
        pts = [to(*p) for p in t if p[1] > y0]
        if len(pts) > 1:
            d.line(pts, fill=(120, 80, 40), width=1)
    for p in N.places():
        u, v = to(p["x"], p["y"])
        d.ellipse((u - 3, v - 3, u + 3, v + 3), outline=(255, 0, 0))
        d.text((u + 5, v - 5), p["id"].replace("north_", ""), fill=(0, 0, 0))
    for f in N.fumaroles():
        u, v = to(f["x"], f["y"])
        d.point((u, v), fill=(255, 255, 255))
    for c in N.camp():
        u, v = to(c["x"], c["y"])
        d.point((u, v), fill=(255, 120, 0))
    img.save(out)
    return w, h


if __name__ == "__main__":
    a = [x for x in sys.argv[1:] if not x.startswith("--")]
    print(render(a[0], float(a[1]) if len(a) > 1 else 6.0, "--built" in sys.argv))
