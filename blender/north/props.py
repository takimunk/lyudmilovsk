"""The North's props, one function per kind: fn(variant) -> (Builder, extra) where extra = {seats, notes}.
KINDS at the bottom lists them (collision class and variants).  Everything: metres, Z up, -Y is the front, origin on the
ground under the middle.  A hiker's camp in Kamchatka-like country: slightly worn, 1990s-2000s gear, no brand logos.
"""
import math
import os
import random

from mathutils import Matrix, Vector

import northkit as K
from northkit import Builder, V

TWO_PI = 2 * math.pi


def rz(a):
    return Matrix.Rotation(a, 3, "Z")


def rx(a):
    return Matrix.Rotation(a, 3, "X")


def ry(a):
    return Matrix.Rotation(a, 3, "Y")


def lerp(a, b, t):
    return a + (b - a) * t


def smooth(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3 - 2 * t)


def guy(b, p0, p1, r=0.003, peg=True, sag=0.0, n=2):
    """A taut (or slightly sagging) line from p0 (on the object) to p1 (on the ground) with a peg at p1."""
    p0, p1 = Vector(p0), Vector(p1)
    if sag > 0:
        pts = [p0.lerp(p1, i / (n - 1)) - Vector((0, 0, sag * 4 * (i / (n - 1)) * (1 - i / (n - 1)))) for i in range(n)]
    else:
        pts = [p0, p1]
    b.tube(pts, r, seg=4, tile="rope", caps=False)
    if peg:
        d = (p0 - p1)
        d.z = 0
        d = d.normalized() if d.length > 1e-6 else Vector((0, 0, 1))
        # the peg leans away from the line's pull
        b.peg(p1 + Vector((0, 0, 0.1)) + d * 0.045, direction=(-d.x * 0.45, -d.y * 0.45, -1.0), length=0.21)


def catenary(p0, p1, sag, n):
    p0, p1 = Vector(p0), Vector(p1)
    return [p0.lerp(p1, i / (n - 1)) - Vector((0, 0, sag * 4 * (i / (n - 1)) * (1 - i / (n - 1))))
            for i in range(n)]


# =========================================================================================================
# log_bench
# =========================================================================================================

def log_bench(variant):
    b = Builder("log_bench_v%d" % variant)
    L = (1.8, 1.68)[variant]
    W = (0.34, 0.38)[variant]
    zt = 0.44
    depth = 0.16
    arc = []
    n = 9
    for i in range(n + 1):
        t = math.pi + math.pi * i / n
        arc.append(((W / 2) * math.cos(t) * -1, zt + depth * math.sin(t) * 1.0))
    # outline: top-left -> arc along the bottom -> top-right (a, z)
    outline = [(-W / 2, zt)] + [(W / 2 * math.cos(math.pi + math.pi * i / n) * -1 * -1, 0) for i in range(0)]
    outline = []
    for i in range(n + 1):
        t = math.pi * i / n
        outline.append((-W / 2 * math.cos(t), zt - depth * math.sin(t)))
    # arc from (-W/2, zt) through the bottom to (W/2, zt); close with the flat top edge (last -> first)
    wob = [(a * (1 + 0.03 * math.sin(i * 1.7 + variant)), z) for i, (a, z) in enumerate(outline)]
    side = lambda i: "wood" if i == len(wob) - 1 else "bark"
    b.prism(wob, V(0, 0, 0), (V(0, 1, 0), V(0, 0, 1), V(1, 0, 0)), L, tile="bark", side_tile=side, cap_tile="endgrain")
    # stumps
    for sx in (-1, 1):
        x = sx * (L / 2 - 0.34 + 0.04 * variant)
        h = zt - depth + 0.01
        b.cyl(V(x, 0.01 * sx, 0), V(x, 0.01 * sx, 0.07), 0.215, 0.18, seg=10, tile="bark", cap_tile="bark", cap1=False, wobble=0.06)
        b.cyl(V(x, 0.01 * sx, 0.07), V(x, 0.01 * sx, h), 0.18, 0.155, seg=10, tile="bark", cap_tile="endgrain", cap0=False, wobble=0.05)
        # a chopped notch of fresh pine at the stump's foot
    # a branch stub on the log's side (variant 1: two)
    for i in range(1 + variant):
        x = (-0.3 + 0.55 * i + 0.12 * variant)
        b.cyl(V(x, W / 2 * 0.7, zt - 0.1), V(x + 0.03, W / 2 * 0.7 + 0.1, zt - 0.14), 0.028, 0.02, seg=6, tile="bark", cap_tile="endgrain")
    seats = [[-0.45, 0.0, zt, 0.0], [0.45, 0.0, zt, 0.0]]
    return b, {"seats": seats, "notes": "split birch log on two stumps; seat top z %.2f; seats face the front (-Y)" % zt}


# =========================================================================================================
# fire_ring
# =========================================================================================================

def fire_ring(variant):
    b = Builder("fire_ring")
    r = random.Random(11)
    n = 14
    for i in range(n):
        a = TWO_PI * i / n + r.uniform(-0.05, 0.05)
        R = 0.6 + r.uniform(-0.03, 0.04)
        sx, sy, sz = r.uniform(0.24, 0.34), r.uniform(0.2, 0.28), r.uniform(0.17, 0.26)
        b.stone(V(math.cos(a) * R, math.sin(a) * R, sz * 0.15 + 0.005), (sx, sy, sz), tile="stone_soot", seed=i, flatten=1.0,
                rot=rz(a + r.uniform(-0.4, 0.4)), rough=0.14)
    # ash bed: a low mound with a ragged rim
    rings = [0.0, 0.2, 0.38, 0.5]
    zr = [0.045, 0.04, 0.025, 0.006]
    seg = 14
    pts = {}
    for j, rr in enumerate(rings):
        for i in range(seg):
            a = TWO_PI * i / seg
            k = 1.0 + (0.08 * math.sin(a * 3 + 1.0) if j else 0.0)
            pts[(j, i)] = V(math.cos(a) * rr * k, math.sin(a) * rr * k, zr[j] + 0.006 * math.sin(a * 5 + j))
    uvc = lambda p: b.mapuv("ash", p.x + 0.5, p.y + 0.5, 1.0, 1.0, (0.0, 0.0))
    for i in range(seg):
        c = pts[(0, 0)]
        i2 = (i + 1) % seg
        b.face([c, pts[(1, i)], pts[(1, i2)]], [uvc(c), uvc(pts[(1, i)]), uvc(pts[(1, i2)])])
        for j in (1, 2):
            q = [pts[(j, i)], pts[(j + 1, i)], pts[(j + 1, i2)], pts[(j, i2)]]
            b.face(q, [uvc(p) for p in q])
    # charred log ends lying on the ash, burnt thin toward the middle
    for a_deg, ln in ((15, 0.62), (112, 0.58), (205, 0.66), (290, 0.55)):
        a = math.radians(a_deg)
        d = V(math.cos(a), math.sin(a), 0)
        p0 = d * 0.10 + V(0, 0, 0.065)
        p1 = d * (0.10 + ln * 0.8) + V(0, 0, 0.085)
        b.cyl(p0, p1, 0.028, 0.055, seg=7, tile="char", cap_tile="char", wobble=0.12)
    for a_deg, off in ((60, 0.3), (160, 0.28), (250, 0.33), (330, 0.25)):
        a = math.radians(a_deg)
        b.stone(V(math.cos(a) * off, math.sin(a) * off, 0.035), (0.09, 0.05, 0.04), tile="char", seed=a_deg, rough=0.3)
    # tripod: three poles crossing at the top, tied, a chain and a blackened kettle
    top = V(0, 0, 1.74)
    for a_deg in (90, 210, 330):
        a = math.radians(a_deg)
        foot = V(math.cos(a) * 1.05, math.sin(a) * 1.05, 0.0)
        tip = V(-math.cos(a) * 0.05, -math.sin(a) * 0.05, 1.86)
        b.cyl(foot, tip, 0.032, 0.022, seg=7, tile="wood_dark", cap_tile="wood_dark", wobble=0.05)
    b.cyl(V(0, 0, 1.62), V(0, 0, 1.72), 0.058, 0.058, seg=10, tile="rope", caps=False)
    chain = [V(0.0, 0.0, 1.70 - i * 0.12) for i in range(8)] + [V(0, 0, 0.8)]
    b.tube([V(0, 0, 1.70), V(0, 0, 0.83)], 0.0045, seg=4, tile="metal", caps=False)
    for i in range(7):
        z = 1.62 - i * 0.115
        b.cyl(V(0, 0, z), V(0, 0, z + 0.04), 0.009, 0.009, seg=6, tile="metal", cap0=False, cap1=False, smooth=False)
    # kettle (lathe) with lid, spout and bail
    prof = [(0.0, 0.5), (0.105, 0.5), (0.126, 0.54), (0.133, 0.62), (0.121, 0.69), (0.088, 0.725), (0.072, 0.738), (0.078, 0.744),
            (0.045, 0.762), (0.012, 0.775), (0.0, 0.777)]
    b.lathe(prof, seg=14, tile="metal")
    bail = [V(math.cos(t) * 0.128, 0, 0.665 + math.sin(t) * 0.17) for t in [math.pi * i / 8 for i in range(9)]]
    b.tube(bail, 0.004, seg=4, tile="metal", caps=False)
    b.tube([V(0.12, 0, 0.56), V(0.17, 0, 0.62), V(0.235, 0, 0.71)], 0.021, seg=7, tile="metal", rscale=lambda i, n: (1.0, 0.8, 0.55)[i])
    b.cyl(V(0, 0, 0.82), V(0, 0, 0.84), 0.016, 0.016, seg=6, tile="metal")
    return b, {"notes": "no flame: the game adds fire; ring r ~0.62 m; tripod feet at r 1.05 m; kettle hangs z 0.5..0.78"}


# =========================================================================================================
# woodpile
# =========================================================================================================

def _split_log(b, x, z, ylen, y0, R, kind, rot, rnd):
    """One piece of firewood lying along Y at (x, z): a quarter / half / whole round, bark on the arc, cut ends."""
    if kind == "quarter":
        a0 = rot
        pts = [(0, 0)] + [(R * math.cos(a0 + math.pi / 2 * i / 4), R * math.sin(a0 + math.pi / 2 * i / 4)) for i in range(5)]
        cx, cz = x - R * 0.35 * math.cos(a0 + math.pi / 4), z - R * 0.35 * math.sin(a0 + math.pi / 4)
        side = lambda i: "wood" if i in (0, 5) else "bark"
    elif kind == "half":
        a0 = rot
        pts = [(R * math.cos(a0 + math.pi * i / 6), R * math.sin(a0 + math.pi * i / 6)) for i in range(7)]
        cx, cz = x, z
        side = lambda i: "wood" if i == 6 else "bark"
    else:
        pts = [(R * math.cos(TWO_PI * i / 9), R * math.sin(TWO_PI * i / 9)) for i in range(9)]
        cx, cz = x, z
        side = lambda i: "bark"
    pts = [(p[0] * (1 + rnd.uniform(-0.05, 0.05)), p[1] * (1 + rnd.uniform(-0.05, 0.05))) for p in pts]
    b.prism(pts, V(cx, y0 + ylen / 2, cz), (V(1, 0, 0), V(0, 0, 1), V(0, 1, 0)), ylen, tile="bark", side_tile=side, cap_tile="endgrain")


def woodpile(variant):
    b = Builder("woodpile")
    r = random.Random(5)
    R = 0.072
    rows = 7
    z = 0.0
    for j in range(rows):
        per = 10 - (j // 3)
        x0 = -(per - 1) * R * 1.02 + (R * 0.9 if j % 2 else 0)
        zc = R * 0.95 + j * R * 1.72
        for i in range(per):
            x = x0 + i * 2.05 * R * 1.0
            if abs(x) > 0.82:
                continue
            kind = r.choice(["quarter", "quarter", "half", "round", "quarter"])
            ylen = 0.5 + r.uniform(-0.03, 0.02)
            _split_log(b, x, zc + r.uniform(-0.004, 0.004), ylen, -0.25 + r.uniform(-0.02, 0.025), R * r.uniform(0.9, 1.1), kind, r.uniform(0, TWO_PI), r)
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.cyl(V(sx * 0.86, sy * 0.3, -0.1), V(sx * 0.86 + sx * 0.01, sy * 0.3, 1.0), 0.04, 0.034, seg=7, tile="bark", cap_tile="endgrain", wobble=0.06)
    # chopping block with an axe stuck in its top
    bx = 1.4
    b.cyl(V(bx, 0, 0), V(bx, 0, 0.44), 0.215, 0.195, seg=12, tile="bark", cap_tile="endgrain", wobble=0.05, cap0=False)
    for i in range(3):          # axe cuts on the block's top: shallow notches (little dark boxes sunk into it)
        a = 0.7 * i
        b.box(V(bx + 0.06 * math.cos(a * 3), 0.07 * math.sin(a * 3), 0.436), (0.16, 0.012, 0.012), "wood_dark", rot=rz(a * 1.4 + 0.5))
    phi = math.radians(62)
    h = V(math.cos(phi), 0, math.sin(phi))
    bb = V(math.sin(phi), 0, -math.cos(phi))
    eye = V(bx - 0.10, 0.0, 0.46)
    b.cyl(eye - h * 0.04, eye + h * 0.78, 0.019, 0.026, seg=6, tile="plank_edge", cap_tile="plank_edge", aspect=0.8)
    head = [(-0.03, -0.04), (0.03, -0.04), (0.03, 0.04), (0.07, 0.14), (-0.07, 0.14), (-0.03, 0.04)]
    head = [(p[0], p[1]) for p in head]
    b.prism(head, eye, (h, bb, V(0, 1, 0)), 0.036, tile="metal", cap_tile="metal")
    # chips on the ground
    for i in range(10):
        a = r.uniform(0, TWO_PI)
        d = r.uniform(0.28, 0.6)
        b.box(V(bx + math.cos(a) * d, math.sin(a) * d, 0.006), (r.uniform(0.04, 0.09), r.uniform(0.02, 0.04), 0.012), "plank_edge", rot=rz(r.uniform(0, 3)))
    b.shift(-0.5, 0)
    return b, {"notes": "firewood stack ~1.65 m wide along X, chopping block with an axe at x 1.4; box collision of the stack and block"}

