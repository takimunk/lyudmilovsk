"""Eggs, nest boxes, tree hole, burrow, eave tufts and the bill bits of the nest kit."""
import math
import os
import zlib

import bpy
import numpy as np

import nest_atlas as AT
from nest_geo import INSET, MB, A, nrm

EAVE = ("HouseSparrow", "WhiteWagtail")


def rng_for(name):
    return np.random.default_rng(zlib.crc32(name.encode()))


# =============================================================================================== eggs
def make_egg(sid):
    L, B, ground, marks, taper = AT.EGGS[sid]
    a, b = L / 2000.0, B / 2000.0
    mb = MB(rng_for("egg" + sid))
    p = mb.part("egg")
    stacks, segs = 12, 16
    ts = [-math.cos(math.pi * i / stacks) for i in range(stacks + 1)]
    prof = [math.sqrt(max(0.0, 1 - t * t)) * (1 + taper * t) for t in ts]
    rmax = max(prof)
    prof = [r / rmax * b for r in prof]
    xs = [t * a for t in ts]
    # arc length along the profile for v
    s = [0.0]
    for i in range(1, len(ts)):
        s.append(s[-1] + math.hypot(xs[i] - xs[i - 1], prof[i] - prof[i - 1]))
    cell = "egg_" + sid
    x0, y0, cw, ch = AT.CELLS[cell]
    lo, hi = INSET + 1, cw - INSET - 1
    rings = []
    for i in range(1, stacks):
        rings.append([mb.vert((xs[i], prof[i] * math.cos(2 * math.pi * k / segs), prof[i] * math.sin(2 * math.pi * k / segs) + b))
                      for k in range(segs)])
    pa = mb.vert((xs[0], 0, b))
    pb = mb.vert((xs[-1], 0, b))

    def uv(k, i):
        return (lo + (hi - lo) * k / segs, lo + (hi - lo) * s[i] / s[-1])
    for i in range(len(rings) - 1):
        for k in range(segs):
            k2 = (k + 1) % segs
            mb.face((rings[i][k], rings[i][k2], rings[i + 1][k2], rings[i + 1][k]), cell, p,
                    [uv(k, i + 1), uv(k + 1, i + 1), uv(k + 1, i + 2), uv(k, i + 2)])
    for k in range(segs):
        k2 = (k + 1) % segs
        mb.face((pa, rings[0][k2], rings[0][k]), cell, p, [((uv(k, 0)[0] + uv(k + 1, 0)[0]) / 2, lo), uv(k + 1, 1), uv(k, 1)])
        mb.face((pb, rings[-1][k], rings[-1][k2]), cell, p, [((uv(k, 0)[0] + uv(k + 1, 0)[0]) / 2, hi), uv(k, stacks - 1), uv(k + 1, stacks - 1)])
    mb.orient(0, mb.nfaces(), lambda c: A(c[0], c[1], c[2] - b))
    return mb


# =============================================================================================== holed grids
def holed_grid(mb, ys, zs, pos, cell, part, hole_ij):
    ids = [[mb.vert(pos(y, z)) for z in zs] for y in ys]
    i0, j0 = hole_ij
    for i in range(len(ys) - 1):
        for j in range(len(zs) - 1):
            if i0 <= i < i0 + 4 and j0 <= j < j0 + 4:
                continue
            mb.face((ids[i][j], ids[i + 1][j], ids[i + 1][j + 1], ids[i][j + 1]), cell, part)
    loop = [(i, j0) for i in range(i0, i0 + 5)] + [(i0 + 4, j) for j in range(j0 + 1, j0 + 5)] + \
           [(i, j0 + 4) for i in range(i0 + 3, i0 - 1, -1)] + [(i0, j) for j in range(j0 + 3, j0, -1)]
    cy, cz = ys[i0 + 2], zs[j0 + 2]
    ang = [math.atan2(zs[j] - cz, ys[i] - cy) for i, j in loop]
    return [ids[i][j] for i, j in loop], ang


def ring(mb, x, ry, rz, ang, cy=0.0, cz=0.0):
    return [mb.vert((x, cy + ry * math.cos(a), cz + rz * math.sin(a))) for a in ang]


def uvlint():
    pass


# =============================================================================================== nest boxes
BOX = {
    "Tit": dict(W=.12, D=.12, Hb=.16, Hf=.075, Hk=.045, rh=.016, w=.012, body="plank_blue", roof="plank_white"),
    "Starling": dict(W=.17, D=.17, Hb=.27, Hf=.13, Hk=.09, rh=.0235, w=.016, body="plank_ochre", roof="plank_blue", perch=True),
    "Sparrow": dict(W=.15, D=.15, Hb=.20, Hf=.085, Hk=.055, rh=.0175, w=.014, body="plank_white", roof="plank_ochre"),
}


def hexa(mb, c, cell, part, inward=False, centre=None):
    """c: 8 corners [x0y0z0, x1y0z0, x1y1z0, x0y1z0, x0y0z1, ...]; 6 quads; winding fixed by orient"""
    idx = [mb.vert(p) for p in c]
    f0 = mb.nfaces()
    for q in ((0, 1, 2, 3), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)):
        mb.face([idx[k] for k in q], cell, part)
    cen = np.mean(c, 0) if centre is None else centre
    mb.orient(f0, mb.nfaces(), (lambda p: cen - p) if inward else (lambda p: p - cen))


def make_box(kind):
    s = BOX[kind]
    mb = MB(rng_for("box" + kind))
    W, D, Hb, Hf, Hk, rh, w = s["W"], s["D"], s["Hb"], s["Hf"], s["Hk"], s["rh"], s["w"]

    def ztop(x):
        return Hk + (Hf - Hk) * (x + D) / D
    hb = max(rh * 2.4, 0.04)
    ys = [-W / 2, -hb, -hb / 2, 0, hb / 2, hb, W / 2]
    zs = [-Hb, -hb, -hb / 2, 0, hb / 2, hb, Hf]
    pf = mb.part("front")
    loop_o, ang = holed_grid(mb, ys, zs, lambda y, z: (0.0, y, z), s["body"], pf, (1, 1))
    f = mb.nfaces()
    mb.orient(0, f, lambda c: A(1, 0, 0))
    # outer back, sides, bottom, top (top is slanted)
    pb = mb.part("shell")
    cs = [(-D, -W / 2, -Hb), (0, -W / 2, -Hb), (0, W / 2, -Hb), (-D, W / 2, -Hb), (-D, -W / 2, Hk), (0, -W / 2, Hf), (0, W / 2, Hf), (-D, W / 2, Hk)]
    cen = np.array([-D / 2, 0, 0.0])
    idx = [mb.vert(p) for p in cs]
    f1 = mb.nfaces()
    for q in ((0, 1, 2, 3), (4, 5, 6, 7), (0, 1, 5, 4), (2, 3, 7, 6), (3, 0, 4, 7)):   # no (1,2,6,5): that is the front
        mb.face([idx[k] for k in q], s["body"], pb if q != (4, 5, 6, 7) else mb.part("top"))
    mb.orient(f1, mb.nfaces(), lambda p: p - cen)
    # front panel's outline edge vs the big faces share positions only along the edge; the front loop of the grid is
    # inside the outline quad ring: weld by reusing the grid corner positions (same coordinates) -> no gap
    # inner cavity (normals inward)
    pc = mb.part("cavity")
    ci = [(-D + w, -W / 2 + w, -Hb + w), (-w, -W / 2 + w, -Hb + w), (-w, W / 2 - w, -Hb + w), (-D + w, W / 2 - w, -Hb + w),
          (-D + w, -W / 2 + w, ztop(-D + w) - w), (-w, -W / 2 + w, ztop(-w) - w), (-w, W / 2 - w, ztop(-w) - w), (-D + w, W / 2 - w, ztop(-D + w) - w)]
    ccen = np.mean(ci, 0)
    idx = [mb.vert(p) for p in ci]
    f2 = mb.nfaces()
    for q in ((0, 1, 2, 3), (4, 5, 6, 7), (0, 1, 5, 4), (2, 3, 7, 6), (3, 0, 4, 7)):
        mb.face([idx[k] for k in q], "wood_raw", pc)
    mb.orient(f2, mb.nfaces(), lambda p: ccen - p)
    # inner front wall (with the hole) at x = -w
    ysi = [-W / 2 + w, -hb, -hb / 2, 0, hb / 2, hb, W / 2 - w]
    zsi = [-Hb + w, -hb, -hb / 2, 0, hb / 2, hb, ztop(-w) - w]
    loop_i, _ = holed_grid(mb, ysi, zsi, lambda y, z: (-w, y, z), "wood_raw", pc, (1, 1))
    f3 = mb.nfaces()
    mb.orient(f2 + 5, f3, lambda p: A(-1, 0, 0))
    # tunnel: outer hole circle (x=0) -> inner circle (x=-w), and the rims between grid loops and the circles
    ring_o = ring(mb, 0.0, rh, rh, ang)
    ring_i = ring(mb, -w, rh, rh, ang)
    pt = mb.part("tunnel")
    g0 = mb.nfaces()
    mb.bridge(loop_o, ring_o, s["body"], pt)
    mb.orient(g0, mb.nfaces(), lambda c: A(1, 0, 0))
    g1 = mb.nfaces()
    mb.bridge(ring_o, ring_i, "wood_raw", pt)
    mb.orient(g1, mb.nfaces(), lambda c: A(0, -c[1], -c[2]))
    g2 = mb.nfaces()
    mb.bridge(loop_i, ring_i, "wood_raw", pt)
    mb.orient(g2, mb.nfaces(), lambda c: A(-1, 0, 0))
    # roof slab, sunk 4 mm into the body so no face is coplanar with another
    pr = mb.part("roof")
    ov_f, ov_s, ov_b, th = 0.045, 0.022, 0.025, 0.018
    xf, xb = ov_f, -D - ov_b
    yw = W / 2 + ov_s
    rc = [(xb, -yw, ztop(xb) - 0.004), (xf, -yw, ztop(xf) - 0.004), (xf, yw, ztop(xf) - 0.004), (xb, yw, ztop(xb) - 0.004),
          (xb, -yw, ztop(xb) + th), (xf, -yw, ztop(xf) + th), (xf, yw, ztop(xf) + th), (xb, yw, ztop(xb) + th)]
    hexa(mb, rc, s["roof"], pr)
    if s.get("perch"):
        pp = mb.part("perch")
        mb.tube([A(-0.01, 0, -0.065), A(0.03, 0, -0.065), A(0.065, 0, -0.065)], 0.0055, "wood_raw", pp, sides=6, rfun=lambda t: 1.0)
    return mb


# =============================================================================================== tree hole
def make_tree_hole():
    mb = MB(rng_for("treehole"))
    Rt = 0.175
    ymax, zmax, back = 0.16, 0.30, -0.27
    ys = [-ymax, -0.125, -0.09, -0.06, -0.03, 0, 0.03, 0.06, 0.09, 0.125, ymax]
    # hole block is 4 cells wide centred on 0: indices of ys 3..7 -> block start i0 = 3 (ys[3]..ys[7]): spans -0.06..0.06
    zs = [-zmax, -0.24, -0.18, -0.12, -0.06, 0, 0.06, 0.12, 0.18, 0.24, zmax]
    # hole block 4 cells: z from index 4..8 -> -0.06..0.18 is not centred; use symmetric zs below
    zs = [-zmax, -0.22, -0.16, -0.12, -0.06, 0, 0.06, 0.12, 0.16, 0.22, zmax]
    rng = mb.rng
    ph = rng.uniform(0, 6.28, 6)

    def disp(y, z):
        e = abs(y) / ymax
        edge = 1.0 if (abs(y) < ymax - 1e-9 and abs(z) < zmax - 1e-9) else 0.0
        v = 0.5 * math.sin(y * 60 + ph[0]) + 0.3 * math.sin(y * 130 + z * 9 + ph[1]) + 0.25 * math.sin(z * 31 + y * 20 + ph[2])
        return 0.010 * v * edge

    def pos(y, z):
        th = y / Rt
        r = Rt + disp(y, z)
        return (-Rt + r * math.cos(th), r * math.sin(th), z)
    pb = mb.part("bark")
    loop, ang = holed_grid(mb, ys, zs, pos, "treebark", pb, (3, 3))
    mb.orient(0, mb.nfaces(), lambda c: A(c[0] + Rt, c[1], 0))
    # the hole: an oval rim of bark, a tunnel, a chamber (all revolved about the X axis, inward normals)
    ry, rz = 0.026, 0.034
    ra = mb.part("hole")
    steps = [(0.014, 1.75, "treebark"), (0.009, 1.25, "treebark"), (-0.002, 1.0, "wood_cut"), (-0.03, 0.97, "wood_cut"),
             (-0.07, 1.05, "soot"), (-0.11, 1.5, "soot"), (-0.15, 1.8, "soot"), (-0.19, 1.6, "soot"), (-0.22, 0.9, "soot")]
    rings = [loop]
    f0 = mb.nfaces()
    for x, sc, cell in steps:
        rings.append(ring(mb, x, ry * sc, rz * sc, ang))
    for i in range(len(rings) - 1):
        g = mb.nfaces()
        cell = steps[i][2]
        mb.bridge(rings[i], rings[i + 1], cell, ra)
        if i == 0:
            mb.orient(g, mb.nfaces(), lambda c: A(1, 0, 0))
        elif i <= 1:
            mb.orient(g, mb.nfaces(), lambda c: A(1, c[1] * 0.0, 0) + A(0, 0, 0))
        else:
            mb.orient(g, mb.nfaces(), lambda c: A(0, -c[1], -c[2]))
    last = rings[-1]
    tip = mb.vert((-0.235, 0, 0))
    g = mb.nfaces()
    for k in range(len(last)):
        mb.face((tip, last[k], last[(k + 1) % len(last)]), "soot", ra)
    mb.orient(g, mb.nfaces(), lambda c: A(0, -c[1], -c[2]) + A(0.3, 0, 0))
    # closing the slab: sides, back, top/bottom fans (zero displacement on the outline)
    ps = mb.part("body")
    th = ymax / Rt
    xa = -Rt + Rt * math.cos(th)
    yarc = [(-Rt + Rt * math.cos(y / Rt), Rt * math.sin(y / Rt)) for y in ys]
    cen = np.array([-0.12, 0, 0])
    g = mb.nfaces()
    for sgn in (-1, 1):
        yy = sgn * Rt * math.sin(th)
        a_, b_, c_, d_ = (mb.vert((xa, yy, -zmax)), mb.vert((back, yy, -zmax)), mb.vert((back, yy, zmax)), mb.vert((xa, yy, zmax)))
        mb.face((a_, b_, c_, d_), "treebark", ps)
    a_, b_, c_, d_ = (mb.vert((back, -ymax, -zmax)), mb.vert((back, ymax, -zmax)), mb.vert((back, ymax, zmax)), mb.vert((back, -ymax, zmax)))
    mb.face((a_, b_, c_, d_), "wood_cut", ps)
    for zc_ in (-zmax, zmax):
        cv = mb.vert((-0.12, 0, zc_))
        poly = [mb.vert((x, y, zc_)) for x, y in yarc] + [mb.vert((back, ymax, zc_)), mb.vert((back, -ymax, zc_))]
        for k in range(len(poly)):
            mb.face((cv, poly[k], poly[(k + 1) % len(poly)]), "wood_cut", ps)
    mb.orient(g, mb.nfaces(), lambda c: c - cen)
    return mb


# =============================================================================================== burrow
def make_burrow():
    mb = MB(rng_for("burrow"))
    ymax, zmax, back = 0.25, 0.20, -0.40
    ys = [-ymax, -0.18, -0.12, -0.08, -0.04, 0, 0.04, 0.08, 0.12, 0.18, ymax]
    zs = [-zmax, -0.16, -0.12, -0.08, -0.04, 0, 0.04, 0.08, 0.12, 0.16, zmax]
    rng = mb.rng
    ph = rng.uniform(0, 6.28, 6)

    def pos(y, z):
        edge = 1.0 if (abs(y) < ymax - 1e-9 and abs(z) < zmax - 1e-9) else 0.0
        v = 0.5 * math.sin(y * 40 + ph[0]) + 0.4 * math.sin(z * 55 + y * 17 + ph[1]) + 0.25 * math.sin(y * 120 + z * 90 + ph[2])
        return (-0.004 + 0.008 * v * edge, y, z)
    pb = mb.part("bank")
    loop, ang = holed_grid(mb, ys, zs, pos, "clay", pb, (3, 3))
    mb.orient(0, mb.nfaces(), lambda c: A(1, 0, 0))
    r0 = 0.03
    ra = mb.part("mouth")
    steps = [(0.007, 1.75, "clay"), (0.004, 1.3, "clay"), (-0.004, 1.0, "clay"), (-0.02, 1.0, "soot"), (-0.07, 0.98, "soot"),
             (-0.14, 0.96, "soot"), (-0.22, 0.94, "soot"), (-0.30, 0.9, "soot"), (-0.315, 0.6, "soot")]
    rings = [loop] + [ring(mb, x, r0 * sc, r0 * sc, ang) for x, sc, _ in steps]
    for i in range(len(rings) - 1):
        g = mb.nfaces()
        mb.bridge(rings[i], rings[i + 1], steps[i][2], ra)
        if i == 0:
            mb.orient(g, mb.nfaces(), lambda c: A(1, 0, 0))
        else:
            mb.orient(g, mb.nfaces(), lambda c: A(0, -c[1], -c[2]) + (A(1, 0, 0) * 0.5 if c[0] > -0.01 else 0))
    tip = mb.vert((-0.322, 0, 0))
    g = mb.nfaces()
    for k in range(len(rings[-1])):
        mb.face((tip, rings[-1][k], rings[-1][(k + 1) % len(rings[-1])]), "soot", ra)
    mb.orient(g, mb.nfaces(), lambda c: A(0, -c[1], -c[2]) + A(0.4, 0, 0))
    ps = mb.part("body")
    cen = np.array([-0.2, 0, 0])
    g = mb.nfaces()
    c8 = [(back, -ymax, -zmax), (-0.0, -ymax, -zmax), (0.0, ymax, -zmax), (back, ymax, -zmax),
          (back, -ymax, zmax), (-0.0, -ymax, zmax), (0.0, ymax, zmax), (back, ymax, zmax)]
    idx = [mb.vert(p) for p in c8]
    for q in ((0, 1, 2, 3), (4, 5, 6, 7), (0, 1, 5, 4), (2, 3, 7, 6), (3, 0, 4, 7)):
        mb.face([idx[k] for k in q], "earth", ps)
    mb.orient(g, mb.nfaces(), lambda c: c - cen)
    return mb


# =============================================================================================== tufts & bits
def make_tuft(kind):
    mb = MB(rng_for("tuft" + kind))
    rng = mb.rng
    sp = kind == "HouseSparrow"
    n, Lr, spread = (30, (0.07, 0.18), 0.55) if sp else (20, (0.05, 0.12), 0.5)
    # a clump of nest material in the gap (back of the tuft), the straws fan out along +X
    cell0 = "grass_dry"
    mb.blob(A(-0.02, 0, 0), np.array([0.035, 0.05, 0.03]) if sp else np.array([0.03, 0.04, 0.025]), cell0, mb.part("clump"), rough=0.2, subdiv=2)
    for i in range(n):
        a = rng.normal(0, spread)
        b = rng.normal(0, spread)
        u = nrm(A(1, math.tan(a) * 0.8, math.tan(b) * 0.8))
        o = A(-0.01, rng.uniform(-0.03, 0.03), rng.uniform(-0.02, 0.02))
        L = rng.uniform(*Lr)
        pts = [o, o + u * L * 0.5 + A(0, 0, rng.normal(0, 0.005)), o + u * L + A(0, rng.normal(0, 0.01), -rng.uniform(0, 0.04))]
        cell = ["grass_dry", "grass_fine", "grass_bleached", "moss_dry"][i % 4] if sp else ["grass_fine", "moss_green", "grass_dry", "rootlet"][i % 4]
        mb.tube(pts, rng.uniform(0.0007, 0.0012), cell, mb.part("straw"), sides=3, flat=0.6)
    for i in range(4 if sp else 3):
        u = nrm(A(1, rng.normal(0, 0.5), rng.normal(0, 0.5)))
        o = A(-0.005, rng.uniform(-0.02, 0.02), rng.uniform(-0.015, 0.015))
        L = rng.uniform(0.05, 0.08)
        pts = [o, o + u * L * 0.4, o + u * L * 0.75 + A(0, 0, -0.006), o + u * L + A(0, 0.004, -0.012)]
        mb.tube(pts, 0.006, "down" if i % 2 else "feather", mb.part("feather"), sides=4, flat=0.15,
                rfun=lambda t: math.sin(math.pi * (0.12 + 0.76 * t)) + 0.1)
    return mb


def make_mat(kind):
    mb = MB(rng_for("mat" + kind))
    rng = mb.rng
    if kind == "Twigs":
        for i in range(4):
            L = rng.uniform(0.07, 0.10)
            yo = (i - 1.5) * 0.004
            a = rng.normal(0, 0.18)
            u = A(math.sin(a) * 0.5, math.cos(a), 0)
            c = A(rng.normal(0, 0.003), yo, rng.normal(0, 0.003) + (i - 1.5) * 0.0045)
            pts = [c - u * L / 2, c + A(0, 0, 0.003 * rng.normal()), c + u * L / 2]
            mb.tube(pts, rng.uniform(0.0011, 0.0016), "bark_brown", mb.part("twig"), sides=5)
    elif kind in ("Grass", "Reed"):
        n = 9 if kind == "Grass" else 4
        for i in range(n):
            L = rng.uniform(0.07, 0.11) if kind == "Grass" else rng.uniform(0.09, 0.12)
            a = rng.normal(0, 0.12)
            u = A(math.sin(a) * 0.4, math.cos(a), 0)
            c = A(rng.normal(0, 0.002), (i - n / 2) * 0.0007, (i % 3 - 1) * 0.0022 + rng.normal(0, 0.0005))
            pts = [c - u * L / 2 + A(0, 0, 0.004 * rng.random()), c + A(0, 0, 0.005 * rng.random()), c + u * L / 2 + A(0, 0, -0.01 * rng.random())]
            if kind == "Grass":
                mb.tube(pts, rng.uniform(0.0008, 0.0012), "grass_dry" if i % 3 else "grass_fine", mb.part("blade"), sides=3, flat=0.6)
            else:
                mb.tube(pts, rng.uniform(0.0016, 0.0024), "reed", mb.part("reed"), sides=5)
    elif kind == "Moss":
        for i in range(6):
            c = A(rng.normal(0, 0.003), (i - 2.5) * 0.007, rng.normal(0, 0.004))
            mb.blob(c, np.array([0.011, 0.012, 0.009]) * rng.uniform(0.8, 1.2), "moss_green", mb.part("tuft"), rough=0.3)
    elif kind == "Mud":
        mb.blob(A(0, 0, 0), np.array([0.011, 0.014, 0.010]), "mud_wet", mb.part("ball"), rough=0.12)
        for i in range(2):
            mb.blob(A(rng.normal(0, 0.003), (-1) ** i * 0.014, 0.003), np.array([0.005, 0.006, 0.005]), "mud_wet", mb.part("pellet"), rough=0.12)
    elif kind == "Feathers":
        for i in range(3):
            o = A(0, (i - 1) * 0.006, (i - 1) * 0.004)
            a = (i - 1) * 0.5
            u = A(math.sin(a) * 0.3, math.cos(a), 0)
            L = rng.uniform(0.065, 0.08)
            pts = [o - u * L * 0.5, o - u * L * 0.15 + A(0, 0, 0.004), o + u * L * 0.2 + A(0, 0, 0.006), o + u * L * 0.5 + A(0, 0, 0.002)]
            mb.tube(pts, 0.0065, "feather" if i != 1 else "down", mb.part("feather"), sides=4, flat=0.15, rfun=lambda t: math.sin(math.pi * (0.1 + 0.8 * t)) + 0.1, up=(1, 0, 0))
    return mb


# =============================================================================================== registry
def all_misc(only, table):
    L = []
    big = lambda s, f: (s, f)
    for sid in table:
        if only and only != sid and not sid.startswith(only):
            pass
    wanted = lambda key: only is None or only == key
    for sid in table:
        if only is None or only == sid:
            L.append((f"SM_Egg_{sid}", (lambda s=sid: make_egg(s)),
                      dict(kind="Egg", species=sid, stage="Egg", eggLengthMm=AT.EGGS[sid][0], eggWidthMm=AT.EGGS[sid][1],
                           origin="egg lying on its side, long axis along X (tip to +X), origin at its lowest point under its centre"), 400))
    if only is None or only in ("Boxes", "Box"):
        for k, h in (("Tit", 0.032), ("Starling", 0.047), ("Sparrow", 0.035)):
            L.append((f"SM_NestBox_{k}", (lambda kk=k: make_box(kk)),
                      dict(kind="NestBox", stage="Box", holeDiameterM=h, origin="centre of the entrance hole on the front face; entrance faces +X, back toward -X; "
                           "cavity: floor at z=-%.3f, interior depth %.3f" % (BOX[k]["Hb"] - BOX[k]["w"], BOX[k]["D"] - 2 * BOX[k]["w"]),
                           species=["Jackdaw (Starling box)"] if k == "Starling" else (["GreatTit", "BlueTit"] if k == "Tit" else ["TreeSparrow", "HouseSparrow"])), 1500))
        L.append(("SM_TreeHole", make_tree_hole, dict(kind="TreeHole", stage="Hole", origin="centre of the hole on the bark surface (trunk radius 0.175, axis at x=-0.175); entrance +X; "
                                                      "bark patch +-0.16 in Y, +-0.30 in Z; chamber 0.22 deep behind the hole", species=["Jackdaw", "GreatTit", "BlueTit", "Starling"]), 2500))
        L.append(("SM_Burrow", make_burrow, dict(kind="Burrow", stage="Burrow", origin="centre of the 6 cm tunnel mouth on the bank face; tunnel 0.30 m deep along -X, "
                                                 "bank slab 0.5 x 0.4 x 0.4", species=["SandMartin"]), 1500))
        for k in EAVE:
            L.append((f"SM_Nest_{k}_Tuft", (lambda kk=k: make_tuft(kk)), dict(kind="Tuft", species=k, stage="Tuft",
                                                                           origin="at the gap in the eave; straws stick out along +X"), 1200))
        for k in ("Twigs", "Grass", "Moss", "Mud", "Reed", "Feathers"):
            L.append((f"SM_NestMat_{k}", (lambda kk=k: make_mat(kk)), dict(kind="NestMat", stage="Bill", origin="grip point at the origin; material runs along +-Y across the bill; sized for a ~20 cm bird"), 400))
    return L


def previews(cam, objs, pdir, table, show, orbit, look, shoot, sheet, extent):
    import bpy as _b
    out = []
    # all eggs in a grid
    eggs = [n for n in objs if n.startswith("SM_Egg_")]
    if eggs:
        made = []
        for i, n in enumerate(eggs):
            c = objs[n].copy()
            _b.context.scene.collection.objects.link(c)
            c.location = ((i % 8) * 0.14 - 0.49, -(i // 8) * 0.1 + 0.2, 0)
            c.scale = (1, 1, 1)
            c.hide_render = False
            c.hide_viewport = False
            made.append(c)
        for n in objs:
            objs[n].hide_render = True
        g = _b.data.objects.get("_ground")
        if g:
            g.location.z = -0.0005
        look(cam, (0, -0.0, 1.0), (0, 0.0, 0), 50)
        cam.location = (0, 0.0, 1.45)
        cam.rotation_euler = (0, 0, 0)
        cam.data.lens = 50
        p = os.path.join(pdir, "eggs_all.png")
        _b.context.scene.render.resolution_x = 1600
        _b.context.scene.render.resolution_y = 700
        shoot(p)
        _b.context.scene.render.resolution_x = _b.context.scene.render.resolution_y = 480
        for c in made:
            _b.data.objects.remove(c)
    # boxes etc.
    items = [("SM_NestBox_Tit", 0.5), ("SM_NestBox_Starling", 0.7), ("SM_NestBox_Sparrow", 0.6), ("SM_TreeHole", 0.8), ("SM_Burrow", 0.9),
             ("SM_Nest_HouseSparrow_Tuft", 0.35), ("SM_Nest_WhiteWagtail_Tuft", 0.3)]
    files = []
    for n, d in items:
        if n not in objs:
            continue
        show(objs, [n])
        lo, hi = extent(objs[n])
        tz = (lo[2] + hi[2]) / 2 * 0.6
        orbit(cam, (-0.03, 0, 0), d, 8, 0, 50)          # front, from +X
        p1 = os.path.join(pdir, f"_{n}_front.png")
        shoot(p1)
        orbit(cam, (-0.05, 0, 0), d, 6, 78, 50)         # grazing from the side, slightly ahead
        p2 = os.path.join(pdir, f"_{n}_side.png")
        shoot(p2)
        orbit(cam, (-0.05, 0, 0), d, 30, -35, 50)
        p3 = os.path.join(pdir, f"_{n}_three.png")
        shoot(p3)
        sheet(os.path.join(pdir, f"{n}_views.png"), 3, 400, [p1, p2, p3])
        files += [p1, p2]
    mats = [n for n in objs if n.startswith("SM_NestMat_")]
    mf = []
    for n in mats:
        show(objs, [n])
        orbit(cam, (0, 0, 0), 0.22, 35, -50, 50)
        p = os.path.join(pdir, f"_{n}.png")
        shoot(p)
        mf.append(p)
    if mf:
        sheet(os.path.join(pdir, "sheet_billbits.png"), 3, 320, mf)
    vs = [os.path.join(pdir, f"{n}_views.png") for n, _ in items if os.path.exists(os.path.join(pdir, f"{n}_views.png"))]
    return None
