"""Real eyes for a generated character: where each eye opening is and the eyeball behind it (system python: numpy, PIL).

    python3 blender/characters/eyes.py <id> <shots dir> [debug.png]   -> blender/characters/<id>/eyes.json

The generated models have their eyes painted on the shell, often torn or smeared, and sliding that paint to make them
look about smeared it more. So the paint inside each opening is cut away at run time (M_HumanFace's opacity mask, in
the reference pose's mesh space, so the opening closes with the blink morph) and an eyeball looks out from behind it
(ULyudBodyAnimInstance). The mesh itself stays as it is.

The openings are fitted by hand on eye_shots.py's close-ups (the hi3d paint is too ragged for an automatic fit) and kept
in character.json face.eye_open, one per eye ([0] the character's left, +x), model metres:
    top(u) = cz + up * (1 - u^2)^0.75 + tilt * u,  bottom(u) = cz - lo * (1 - u^2)^0.75 + tilt * u,  u = (x - cx) / hw
This adds the surface's depth there (y, from the shots' depth maps), the eyeball (an ellipsoid R deep and tall, about
as wide as the opening, turned about z with the face so its outer end follows the temple back, as close behind the
opening as the shell round it allows: a face's sides fall away fast at the outer corner and a straight ball pokes out
there), the dark socket behind it (socket_y) and the iris colour (face_marks.json), and draws the openings over the
close-ups for a check.
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
W, H, PX = 0.07, 0.04, 10000                       # eye_shots.py's pictures
P = 0.75


def outline(f, e):
    def px(x, z):
        return (x - (e["x"] - W / 2)) * PX, ((e["z"] + H / 2) - z) * PX
    u = np.linspace(-1, 1, 81)
    g = (1 - u ** 2) ** P
    top = [px(f["cx"] + f["hw"] * a, f["cz"] + f["up"] * b + f["tilt"] * a) for a, b in zip(u, g)]
    bot = [px(f["cx"] + f["hw"] * a, f["cz"] - f["lo"] * b + f["tilt"] * a) for a, b in zip(u, g)]
    return top + bot[::-1] + top[:1]


def front(XX, ZZ, b):
    """Where a ray from the front (+y, Blender's back, at each x, z) first meets the ball, relative to its centre's y;
    nan where it misses. The ball: an ellipsoid (width across, depth along -y, height) turned by yaw about z (its outer
    end back: + for the left eye, whose outer side is +x)."""
    c, sn = np.cos(np.radians(b["yaw"])), np.sin(np.radians(b["yaw"]))
    dx, dz = XX - b["x"], ZZ - b["z"]
    # local lateral = c dx + sn dy, local depth = -sn dx + c dy  ->  a dy^2 + 2 h dy + k = 0
    A = (sn / b["width"]) ** 2 + (c / b["depth"]) ** 2
    Bh = c * sn * dx / b["width"] ** 2 - sn * c * dx / b["depth"] ** 2
    K = (c * dx / b["width"]) ** 2 + (sn * dx / b["depth"]) ** 2 + (dz / b["height"]) ** 2 - 1
    disc = Bh * Bh - A * K
    with np.errstate(invalid="ignore"):
        return np.where(disc >= 0, (-Bh - np.sqrt(np.maximum(disc, 0))) / A, np.nan)


def place(d, e, b, f, margin=0.0006):
    """The ball's centre y (Blender: +y is back) as far forward as the shell lets it: the ellipsoid stays behind the
    shell everywhere but in the opening (a face's sides fall away fast at the outer corner). Returns (y, gap at the
    opening's middle, covers the opening). d: eye_shots' depth map round e."""
    nz, nx = d.shape
    q = PX / 4
    X = e["x"] - W / 2 + (np.arange(nx) + 0.5) / q
    Z = e["z"] + H / 2 - (np.arange(nz) + 0.5) / q
    XX, ZZ = np.meshgrid(X, Z)
    t = front(XX, ZZ, b)
    u = (XX - f["cx"]) / f["hw"]
    g = np.clip(1 - u ** 2, 0, 1) ** P
    zc = f["cz"] + f["tilt"] * u
    hole = (np.abs(u) < 1) & (ZZ < zc + f["up"] * g) & (ZZ > zc - f["lo"] * g)
    m = np.isfinite(t) & ~hole & np.isfinite(d)
    tc = front(np.array([f["cx"]]), np.array([f["cz"]]), b)[0]
    y = max(float(np.max(d[m] + margin - t[m])), f["y"] + 0.001 - tc)      # and no nearer than 1 mm behind the opening
    covered = float(np.mean(np.isfinite(t[hole])))
    return y, y + tc - f["y"], covered


def main():
    cid, shots = sys.argv[1:3]
    marks = json.load(open(os.path.join(HERE, cid, "face_marks.json")))
    spec = json.load(open(os.path.join(HERE, cid, "character.json")))
    k = marks["ipd"] / 0.063
    out, pics = [], []
    for i, (e, f) in enumerate(zip(marks["eyes"], spec["face"]["eye_open"])):
        d = np.load(os.path.join(shots, "%s_%d_depth.npy" % (cid, i)))
        q = PX / 4
        r0 = int(round(d.shape[0] / 2 + (e["z"] - f["cz"]) * q))
        c0 = int(round(d.shape[1] / 2 + (f["cx"] - e["x"]) * q))
        y = float(np.nanmin(d[r0 - 2:r0 + 3, c0 - 2:c0 + 3]))      # the shell's front there (Blender -y is forward)
        f = dict(f, y=y)
        R = 0.0118 * min(max(k, 0.9), 1.05)
        # the ball: turned (its outer end back, with the face) and as wide as fills the opening, whichever sits closest
        # behind the shell at the opening's middle
        side = 1 if i == 0 else -1
        best = None
        for yaw in range(0, 46, 3):
            for wk in (0.92, 1.0, 1.08, 1.16):
                b = {"x": f["cx"], "z": f["cz"], "depth": R, "width": max(R, f["hw"] * wk), "height": R,
                     "yaw": side * yaw}
                yb, gap, cov = place(d, e, b, f)
                score = gap + (1 - cov) * 0.05
                if best is None or score < best[0]:
                    best = (score, dict(b, y=yb), gap, cov)
        ball = best[1]
        sock = dict(ball, depth=R * 0.7, width=ball["width"] * 1.2, height=R * 1.35)
        ball["socket_y"] = max(place(d, e, sock, f)[0], ball["y"] + R * 0.3)
        print("EYE %s %d: yaw %d, width %.1f mm, gap %.1f mm, covers %.0f%%" % (cid, i, ball["yaw"], ball["width"] * 1000,
                                                                          best[2] * 1000, best[3] * 100))
        iris = [round(float(v), 3) for v in e.get("iris", [0.25, 0.2, 0.17])]
        out.append({"open": dict(f, y=round(y, 5)), "ball": {a: round(float(v), 5) for a, v in ball.items()},
                    "iris": iris})
        im = Image.open(os.path.join(shots, "%s_%d.png" % (cid, i))).convert("RGB")
        ImageDraw.Draw(im).line(outline(f, e), fill=(0, 255, 255), width=1)
        pics.append(im)
    json.dump({"_doc": "blender/characters/eyes.py: the eye openings cut at run time and the eyeballs behind them "
                       "([0] the character's left; model metres, Blender frame: -y forward)", "eyes": out},
              open(os.path.join(HERE, cid, "eyes.json"), "w"), indent=1)
    if len(sys.argv) > 3:
        s = Image.new("RGB", (pics[0].width * 2 + 10, pics[0].height), (255, 0, 255))
        s.paste(pics[1], (0, 0))
        s.paste(pics[0], (pics[0].width + 10, 0))
        s.save(sys.argv[3])
    print("EYES %s: %s" % (cid, json.dumps(out)))


main()
