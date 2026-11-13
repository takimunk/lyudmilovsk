"""Boards, signs, the barrier and the spring's deck. Words are textures typeset by tools/north_signs.py
(textures/localized/<lang>/north_<id>.png); the GLBs carry the Russian ones in materials M_North_Text_<id>."""
import math
import os
import random

from mathutils import Matrix, Vector

import northkit as K
from northkit import Builder, V
import northtex as T
from props import TWO_PI, guy, lerp, rx, ry, rz

TEXT_DIR = os.path.join(K.ROOT, "textures", "localized", "ru")


def text_slot(b, tex_id):
    path = os.path.join(TEXT_DIR, "north_%s.png" % tex_id)
    K.text_material("M_North_Text_%s" % tex_id, path)
    return b.slot("M_North_Text_%s" % tex_id)


def plate(b, outline, p, axes, depth, tex_id, uvrect=(0, 0, 1, 1), back_tile="wood", edge_tile="metal"):
    """A flat plate whose front (axes[2] > 0 side) carries a text texture; back and edge from the atlas.
    outline: convex-ish loop of (a, b); the front fan is star-shaped from the centroid."""
    ea, eb, n = axes
    slot = text_slot(b, tex_id)
    xs = [q[0] for q in outline]
    ys = [q[1] for q in outline]
    la, lb = max(xs) - min(xs), max(ys) - min(ys)
    Pf = lambda a, bb, s: p + ea * a + eb * bb + n * (s * depth / 2)
    u0, v0, u1, v1 = uvrect

    def tuv(a, bb):
        return (lerp(u0, u1, (a - min(xs)) / la), lerp(v0, v1, (bb - min(ys)) / lb))
    k = len(outline)
    c = (sum(xs) / k, sum(ys) / k)
    for i in range(k):
        a0, a1 = outline[i], outline[(i + 1) % k]
        b.face([Pf(c[0], c[1], 1), Pf(a0[0], a0[1], 1), Pf(a1[0], a1[1], 1)], [tuv(*c), tuv(*a0), tuv(*a1)], mat=slot)
        bu = lambda q: b.mapuv(back_tile, q[0] - min(xs), q[1] - min(ys), la, lb, (0.2, 0.3), aniso=True)
        b.face([Pf(c[0], c[1], -1), Pf(a1[0], a1[1], -1), Pf(a0[0], a0[1], -1)], [bu(c), bu(a1), bu(a0)])
        w = math.hypot(a1[0] - a0[0], a1[1] - a0[1])
        q = [Pf(a0[0], a0[1], -1), Pf(a1[0], a1[1], -1), Pf(a1[0], a1[1], 1), Pf(a0[0], a0[1], 1)]
        b.face(q, [b.mapuv(edge_tile, 0, 0, w, depth, ((0.037 * i) % 1.0, 0.1)), b.mapuv(edge_tile, w, 0, w, depth, ((0.037 * i) % 1.0, 0.1)),
                   b.mapuv(edge_tile, w, depth, w, depth, ((0.037 * i) % 1.0, 0.1)), b.mapuv(edge_tile, 0, depth, w, depth, ((0.037 * i) % 1.0, 0.1))])


def rrect(w, h, r, seg=4):
    pts = []
    for (cx, cy, a0) in ((w / 2 - r, h / 2 - r, 0), (-w / 2 + r, h / 2 - r, math.pi / 2), (-w / 2 + r, -h / 2 + r, math.pi), (w / 2 - r, -h / 2 + r, 3 * math.pi / 2)):
        for i in range(seg + 1):
            a = a0 + (math.pi / 2) * i / seg
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def gable_roof(b, cx, cz, length, depth, overhang=0.12, slope=33, y=0.0):
    """A little two-slab gable roof over a board: ridge along X at height cz, slabs reaching to y +- depth/2."""
    a = math.radians(slope)
    half = depth / 2 / math.cos(a) + overhang
    for s in (-1, 1):
        c = V(cx, y + s * half * 0.5 * math.cos(a), cz - half * 0.5 * math.sin(a) - 0.015)
        b.box(c, (length, half, 0.032), "wood_dark", rot=rx(-s * a), grain=0)
    b.box(V(cx, y, cz + 0.005), (length + 0.02, 0.07, 0.05), "wood_dark")


def backboard(b, cx, z0, z1, width, y=0.0, planks=6, thick=0.03):
    rnd = random.Random(int(width * 100))
    h = (z1 - z0) / planks
    for i in range(planks):
        b.box(V(cx, y, z0 + h * (i + 0.5)), (width + rnd.uniform(-0.01, 0.01), thick, h - 0.006), "wood", tiles={"x": "endgrain", "top": "wood"})


def board_camp(variant):
    b = Builder("board_camp")
    pw = 1.4
    backboard(b, 0, 0.62, 1.82, pw, planks=6)
    for sx in (-1, 1):
        b.cyl(V(sx * 0.77, 0, -0.1), V(sx * 0.77, 0, 2.02), 0.055, 0.05, seg=8, tile="bark", cap_tile="endgrain", wobble=0.03, cap0=False)
        b.box(V(sx * 0.705, 0.0, 1.0), (0.1, 0.07, 0.07), "wood_dark")
        b.box(V(sx * 0.705, 0.0, 1.6), (0.1, 0.07, 0.07), "wood_dark")
    # notice panel (the words) in a frame
    cz = 1.22
    plate(b, [(-0.5, -0.5), (0.5, -0.5), (0.5, 0.5), (-0.5, 0.5)], V(0, -0.0225, cz), (V(1, 0, 0), V(0, 0, 1), V(0, -1, 0)), 0.015, "board_camp",
          back_tile="wood")
    for (cx_, cz_, sx_, sz_) in ((0, cz + 0.525, 1.1, 0.05), (0, cz - 0.525, 1.1, 0.05), (-0.525, cz, 0.05, 1.0), (0.525, cz, 0.05, 1.0)):
        b.box(V(cx_, -0.03, cz_), (sx_, 0.03, sz_), "wood_dark")
    gable_roof(b, 0, 2.02, 1.7, 0.7)
    for sx in (-1, 1):
        for i in range(3):
            a = TWO_PI * i / 3 + sx
            b.stone(V(sx * 0.77 + math.cos(a) * 0.11, math.sin(a) * 0.11, 0.03), (0.17, 0.15, 0.12), tile="stone", seed=i * 7 + sx, flatten=1.0)
    return b, {"notes": "board 1.6 m wide with a little roof; the notice panel (1 x 1 m, M_North_Text_board_camp, square texture) faces the front -Y"}


def board_reserve(variant):
    b = Builder("board_reserve")
    backboard(b, 0, 0.88, 2.04, 2.0, planks=6)
    for sx in (-1, 1):
        b.cyl(V(sx * 1.06, 0, -0.1), V(sx * 1.06, 0, 2.36), 0.075, 0.068, seg=8, tile="bark", cap_tile="endgrain", wobble=0.03, cap0=False)
        for z in (1.0, 1.9):
            b.box(V(sx * 0.985, 0.0, z), (0.14, 0.08, 0.08), "wood_dark")
    cz = 1.46
    pwid, phgt = 1.9, 0.95
    plate(b, [(-pwid / 2, -phgt / 2), (pwid / 2, -phgt / 2), (pwid / 2, phgt / 2), (-pwid / 2, phgt / 2)], V(0, -0.0225, cz),
          (V(1, 0, 0), V(0, 0, 1), V(0, -1, 0)), 0.015, "board_reserve")
    for (cx_, cz_, sx_, sz_) in ((0, cz + phgt / 2 + 0.03, pwid + 0.12, 0.06), (0, cz - phgt / 2 - 0.03, pwid + 0.12, 0.06),
                                 (-pwid / 2 - 0.03, cz, 0.06, phgt), (pwid / 2 + 0.03, cz, 0.06, phgt)):
        b.box(V(cx_, -0.03, cz_), (sx_, 0.03, sz_), "wood_dark")
    gable_roof(b, 0, 2.42, 2.45, 0.9, overhang=0.14)
    # a center prop so the board does not sag, and a stone footing
    b.box(V(0, 0.07, 0.5), (0.1, 0.1, 1.0), "wood_dark")
    for sx in (-1, 0, 1):
        for i in range(4):
            a = TWO_PI * i / 4 + sx
            b.stone(V(sx * 1.06 + math.cos(a) * 0.16, math.sin(a) * 0.16, 0.04), (0.2, 0.17, 0.14), tile="stone", seed=i * 5 + sx, flatten=1.0)
    return b, {"notes": "reserve board 2.2 m wide with a little roof; the panel (1.9 x 0.95 m, M_North_Text_board_reserve, 2:1) faces -Y: title, rules, map"}


def sign_hot_ground(variant):
    b = Builder("sign_hot_ground")
    b.cyl(V(0, 0.06, -0.1), V(0, 0.06, 1.72), 0.045, 0.04, seg=8, tile="wood", cap_tile="endgrain", wobble=0.03, cap0=False)
    for z in (1.12, 1.58):
        b.box(V(0, 0.015, z), (0.1, 0.03, 0.05), flatc="steel")
    plate(b, rrect(0.7, 0.7, 0.048), V(0, -0.012, 1.36), (V(1, 0, 0), V(0, 0, 1), V(0, -1, 0)), 0.018, "sign_hot_ground", back_tile="metal")
    for i in range(5):
        a = TWO_PI * i / 5
        b.stone(V(math.cos(a) * 0.14, 0.06 + math.sin(a) * 0.14, 0.04), (0.18, 0.16, 0.12), tile="stone", seed=i, flatten=1.0)
    return b, {"notes": "0.7 m square yellow plate on a wooden post; M_North_Text_sign_hot_ground faces -Y"}


def sign_post(variant):
    b = Builder("sign_post")
    b.cyl(V(0, 0, -0.1), V(0, 0, 2.15), 0.07, 0.06, seg=8, tile="bark", cap_tile="endgrain", wobble=0.03, cap0=False)
    b.cyl(V(0, 0, 2.15), V(0, 0, 2.2), 0.065, 0.02, seg=8, tile="wood_dark", cap_tile="wood_dark")
    ar = [(-0.5, -0.105), (0.37, -0.105), (0.5, 0.0), (0.37, 0.105), (-0.5, 0.105)]
    for row, (z, right) in enumerate(((1.88, True), (1.6, False))):
        pts = ar if right else [(-a, bb) for a, bb in ar][::-1]
        v0 = 0.5 if row == 0 else 0.0
        plate(b, pts, V(0.0, -0.1, z), (V(1, 0, 0), V(0, 0, 1), V(0, -1, 0)), 0.03, "sign_post_spring", uvrect=(0, v0, 1, v0 + 0.5), back_tile="wood",
              edge_tile="wood_dark")
        for sx in (-0.3, 0.3):
            b.cyl(V(sx, -0.118, z), V(sx, -0.126, z), 0.011, 0.011, seg=6, flatc="steel")
    for i in range(6):
        a = TWO_PI * i / 6
        b.stone(V(math.cos(a) * 0.17, math.sin(a) * 0.17, 0.04), (0.2, 0.17, 0.14), tile="stone", seed=i, flatten=1.0)
    return b, {"notes": "fingerpost, two arrow planks (upper points +X, lower points -X); M_North_Text_sign_post_spring in the GLB, swap per instance for sign_post_cone / sign_post_camp (same UVs: upper half = plank 1)"}


def barrier(variant):
    b = Builder("barrier")
    px = -2.15
    b.box(V(px, 0, 0.1), (0.6, 0.6, 0.2), "stone")
    b.box(V(px, 0, 0.65), (0.15, 0.15, 1.1), flatc="steel")
    b.box(V(px, 0, 1.15), (0.2, 0.32, 0.34), flatc="red_paint")
    b.box(V(px, 0, 1.15), (0.22, 0.12, 0.2), flatc="black_paint")
    P = V(px, 0, 1.15)
    ang = math.radians(62)
    d = V(math.cos(ang), 0, math.sin(ang))
    Lp = 4.1
    b.cyl(P - d * 0.15, P + d * Lp, 0.05, 0.034, seg=10, tile="stripes", cap_tile="stripes")
    # tail and counterweight
    b.cyl(P - d * 0.15, P - d * 0.85, 0.05, 0.05, seg=10, tile="stripes", cap_tile="stripes")
    w = P - d * 0.95
    b.box(w + V(0, 0, -0.1), (0.3, 0.2, 0.36), flatc="steel", rot=ry(-(math.pi / 2 - ang)))
    b.box(w + V(0, 0, -0.1), (0.31, 0.12, 0.2), flatc="rust", rot=ry(-(math.pi / 2 - ang)))
    # the pivot bolt
    b.cyl(P + V(0, -0.09, 0), P + V(0, -0.14, 0), 0.04, 0.04, seg=8, flatc="steel")
    # receiver post on the other side of the track with a fork
    rx_ = 2.15
    b.box(V(rx_, 0, 0.1), (0.5, 0.5, 0.2), "stone")
    b.box(V(rx_, 0, 0.7), (0.13, 0.13, 1.2), flatc="steel")
    for s in (-1, 1):
        b.box(V(rx_, s * 0.06, 1.33), (0.12, 0.025, 0.14), flatc="red_paint")
    return b, {"notes": "counterweighted pole barrier across a 4.3 m track, pole raised at 62 degrees; collision only for the two posts (the pole is raised)"}


# =========================================================================================================
# spring_deck
# =========================================================================================================

def spring_deck(variant):
    b = Builder("spring_deck")
    rnd = random.Random(17)
    zt = 0.45
    x0, x1 = 2.3, 4.5
    y0, y1 = -1.5, 1.5
    # planks along Y
    n = 14
    pw = (x1 - x0) / n
    for i in range(n):
        x = x0 + pw * (i + 0.5)
        b.box(V(x, rnd.uniform(-0.01, 0.01), zt - 0.02 + rnd.uniform(-0.0015, 0.0015)), (pw - 0.012, 3.0 - rnd.uniform(0, 0.05), 0.04), "wood",
              tiles={"y": "endgrain", "top": "wood"}, rot=rz(rnd.uniform(-0.003, 0.003)))
    # joists, posts, skirt boards
    for y in (-1.4, -0.47, 0.47, 1.4):
        b.box(V((x0 + x1) / 2, y, zt - 0.1), (x1 - x0 - 0.1, 0.08, 0.12), "wood_dark")
    for x in (x0 + 0.1, (x0 + x1) / 2, x1 - 0.1):
        for y in (-1.42, 0.0, 1.42):
            b.box(V(x, y, (zt - 0.04 - 0.4) / 2), (0.12, 0.12, zt - 0.04 + 0.4), "wood_dark")
    for y in (y0 + 0.012, y1 - 0.012):
        b.box(V((x0 + x1) / 2, y, zt / 2 - 0.12), (x1 - x0, 0.025, zt - 0.2), "wood", grain=0)
    b.box(V(x1 - 0.012, 0, zt / 2 - 0.12), (0.025, 3.0, zt - 0.2), "wood", grain=1)
    # two steps down to the water on the west side
    for k, (xa, xb, zs) in enumerate(((2.075, 2.3, 0.30), (1.85, 2.075, 0.15))):
        b.box(V((xa + xb) / 2, 0, (zs - 0.5) / 2 - 0.0), (xb - xa, 1.4, zs + 0.5 - 0.0), "wood", tiles={"top": "wood", "x": "wood_dark", "y": "wood_dark"}, grain=1)
        b.box(V((xa + xb) / 2, 0, zs - 0.012), (xb - xa + 0.02, 1.42, 0.03), "wood")
    # handrail: posts and two rails on the north, south and east sides, openings left at the steps
    rz_top, rz_mid = zt + 0.95, zt + 0.5
    post_pts = [(x0 + 0.12, y0 + 0.07), (x0 + 0.12, y1 - 0.07), ((x0 + x1) / 2, y0 + 0.07), ((x0 + x1) / 2, y1 - 0.07), (x1 - 0.07, y0 + 0.07),
                (x1 - 0.07, y1 - 0.07), (x1 - 0.07, 0.0)]
    for (px, py) in post_pts:
        b.box(V(px, py, zt + 0.5), (0.08, 0.08, 1.0), "wood_dark")
    for y in (y0 + 0.07, y1 - 0.07):
        for z in (rz_top - 0.0, rz_mid):
            b.box(V((x0 + 0.12 + x1 - 0.07) / 2, y, z), (x1 - x0 - 0.05, 0.05, 0.06 if z == rz_top else 0.045), "wood", grain=0)
    for z in (rz_top, rz_mid):
        b.box(V(x1 - 0.07, 0, z), (0.05, 3.0 - 0.14, 0.06 if z == rz_top else 0.045), "wood", grain=1)
    # bench against the east rail, facing the pool
    bx = x1 - 0.32
    b.box(V(bx, 0, zt + 0.43), (0.38, 2.2, 0.045), "wood", grain=1, tiles={"y": "endgrain"})
    for s in (-1, 1):
        b.box(V(bx, s * 0.95, zt + 0.21), (0.3, 0.07, 0.42), "wood_dark")
    # a weathered wood bucket and a towel hook would clutter the deck: only a bucket near the steps
    b.cyl(V(3.0, 1.15, zt), V(3.0, 1.15, zt + 0.26), 0.14, 0.17, seg=10, tile="wood_dark", cap0=False, cap_tile="wood_dark")
    b.tube([V(3.0 - 0.17, 1.15, zt + 0.24), V(3.0, 1.15, zt + 0.42), V(3.0 + 0.17, 1.15, zt + 0.24)], 0.006, seg=4, tile="metal", caps=False)
    # the ring of rounded stones round the pool (r 2.6), left open where the deck and its steps are
    for i in range(34):
        a = TWO_PI * i / 34 + rnd.uniform(-0.03, 0.03)
        ad = (a + math.pi) % TWO_PI - math.pi
        if abs(ad) < 0.8:
            continue
        sz = rnd.uniform(0.3, 0.52)
        R = 2.66 + rnd.uniform(-0.06, 0.09)
        b.stone(V(math.cos(a) * R, math.sin(a) * R, sz * 0.12), (sz * 1.15, sz * rnd.uniform(0.8, 1.0), sz * 0.62), tile="stone", seed=i, flatten=1.0,
                rot=rz(a + rnd.uniform(-0.3, 0.3)))
    for i in range(16):
        a = TWO_PI * i / 16 + 0.2
        ad = (a + math.pi) % TWO_PI - math.pi
        if abs(ad) < 0.9:
            continue
        sz = rnd.uniform(0.18, 0.3)
        R = 3.15 + rnd.uniform(-0.1, 0.25)
        b.stone(V(math.cos(a) * R, math.sin(a) * R, sz * 0.12), (sz * 1.2, sz, sz * 0.6), tile="stone", seed=100 + i, flatten=1.0)
    return b, {"seats": [[4.18, -0.6, zt + 0.45, -90.0], [4.18, 0.6, zt + 0.45, -90.0]],
               "notes": "origin = pool centre (r 2.6); deck x 2.3..4.5, y -1.5..1.5, top z 0.45 over the rim ground; two steps (z 0.30, 0.15) lead down west "
                        "into the pool: the game's water surface should sit at about z -0.05 and the pool floor lower; rim stones open at the deck; "
                        "no water in the mesh; bench seats face -X (yaw -90 in the seat convention)"}
