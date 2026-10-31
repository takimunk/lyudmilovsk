"""The diner's tableware and food (docs/diner.md): plate, bowl, mug, tea glass in a podstakannik, fork, spoon, the liquids
that fill them, the dishes on a plate and what rides on a fork or spoon after a scoop. Clean low-poly procedural meshes
(Dima's 3D glitch rule: no coplanar faces, no faces inside faces, metre UVs from diner_common). SM_Diner_<Name>.glb in
design/diner/game, constants for the C++ in design/diner/tableware.json, previews in design/diner/previews.

  Blender -b -P blender/diner/tableware.py [-- Name ...]

Frames (metres, x forward, y left, z up):
  Plate, Bowl, Mug, TeaGlass  origin at the bottom centre
  CoffeeFill, GlassFill, SoupFill  origin at the vessel's inner bottom (BOWL_IN_Z / MUG_IN_Z / GLASS_IN_Z up), z 0..FILL_H: the
                              game scales Z 0..1
  Fork, Spoon                 origin at the grip point (0.06 m from the handle end), tines / bowl at +x, flat face up
  Pancakes EggsBacon Spaghetti ApplePie Burger SoupFill  food: origin at the plate's top surface centre (put it at the plate's
                              origin + PLATE_TOP); Burger at its bottom centre
  Load_*                      origin = the fork's load point (TINE_X ahead of the grip, attach at (TINE_X, 0, 0.006))
"""
import json
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import diner_common as C

M = C.M
PI = math.pi
PREV = os.path.join(C.ROOT, "design", "diner", "previews")

# ---- constants (copied by the game's C++ from tableware.json) --------------------------------------------------------------
PLATE_R = 0.120
PLATE_TOP = 0.014
BOWL_IN_Z = 0.012
BOWL_IN_R = 0.0755
BOWL_H = 0.065
SOUP_TOP = 0.055                       # soup surface above the bowl's foot
SOUP_FILL_H = SOUP_TOP - BOWL_IN_Z
MUG_R = 0.0425
MUG_H = 0.095
MUG_IN_R = 0.0375
MUG_IN_Z = 0.010
MUG_FILL_H = 0.075                     # full level at MUG_IN_Z + 0.075 = 0.085, 1 cm under the rim
GLASS_IN_R = 0.0329                    # inner circumradius at the rim
GLASS_IN_Z = 0.013
GLASS_FILL_H = 0.081
GLASS_R = 0.036                        # outer circumradius at the rim (0.072 across)
GLASS_Z0 = 0.0045                      # the glass stands on the holder's floor
GLASS_H = 0.105
TINE_X = 0.115                         # the fork's load point: x ahead of the grip
LOAD_Z = 0.006                         # the load's origin height over the fork's origin
BURGER_R = 0.0515
BURGER_Z0, BURGER_Z1 = 0.002, 0.080

FOOD_MATS = ["DN_Pancake", "DN_Syrup", "DN_SourCream", "DN_Jam", "DN_EggWhite", "DN_Yolk", "DN_Bacon", "DN_Toast",
             "DN_Noodle", "DN_Sauce", "DN_Meatball", "DN_Crust", "DN_PieFilling", "DN_Bun", "DN_Patty", "DN_Cheese",
             "DN_Lettuce", "DN_Soup", "DN_Fish", "DN_Herb", "DN_Tea", "DN_Kompot", "DN_Berry", "DN_Coffee"]
CHINA = ["DN_China", "DN_ChinaRim"]


# ---- helpers -----------------------------------------------------------------------------------------------------------------

def loop_stack(bm, rings, mi, bottom=False, top=False, closed=False):
    """Skins a list of rings (lists of (x, y, z), counter-clockwise from above); the first / last ring may be one point (an
    apex). mi: one material or one per band. Going outward the band faces down, going inward it faces up."""
    vr = [[bm.verts.new(p) for p in r] for r in rings]
    n = max(len(r) for r in vr)
    segs = list(zip(vr, vr[1:])) + ([(vr[-1], vr[0])] if closed else [])
    for k, (a, b) in enumerate(segs):
        m = mi[min(k, len(mi) - 1)] if isinstance(mi, (list, tuple)) else mi
        for i in range(n):
            j = (i + 1) % n
            if len(a) == 1 and len(b) == 1:
                continue
            if len(a) == 1:
                f = (a[0], b[j], b[i])
            elif len(b) == 1:
                f = (a[i], a[j], b[0])
            else:
                f = (a[i], a[j], b[j], b[i])
            bm.faces.new(f).material_index = m
    m0 = mi[0] if isinstance(mi, (list, tuple)) else mi
    m1 = mi[-1] if isinstance(mi, (list, tuple)) else mi
    if bottom and len(vr[0]) > 1:
        bm.faces.new(vr[0][::-1]).material_index = m0
    if top and len(vr[-1]) > 1:
        bm.faces.new(vr[-1]).material_index = m1


def dome(bm, cx, cy, z0, Rf, prof, H, mi, seg=24, bottom=True, zb=None, aspect=1.0, rot=0.0):
    """A soft mound: prof [(f, h)] from the rim inwards (f: fraction of the outline radius Rf(theta), h: fraction of H); the
    last (0, h) is the apex. zb(x, y): a base height that replaces z0 (a sloping seat)."""
    cr, sr = math.cos(rot), math.sin(rot)
    rings = []
    for f, h in prof:
        if f == 0:
            rings.append([(cx, cy, (zb(cx, cy) if zb else z0) + H * h)])
            continue
        ring = []
        for i in range(seg):
            th = 2 * PI * i / seg
            r = Rf(th) * f
            lx, ly = r * math.cos(th), r * math.sin(th) * aspect
            x, y = cx + lx * cr - ly * sr, cy + lx * sr + ly * cr
            ring.append((x, y, (zb(x, y) if zb else z0) + H * h))
        rings.append(ring)
    if bottom:
        rings.insert(0, [(cx, cy, rings[0][0][2])])
    loop_stack(bm, rings, mi, top=True)


def lathe_fan(bm, prof, mats, seg=24, cap0=False, cap1=False):
    """C.lathe with caps made of triangle fans round an apex (n-gon caps trip the overlap lint)."""
    rings = [[(r * math.cos(2 * PI * i / seg), r * math.sin(2 * PI * i / seg), z) for i in range(seg)] for r, z in prof]
    if cap0:
        rings.insert(0, [(0, 0, prof[0][1])])
    if cap1:
        rings.append([(0, 0, prof[-1][1])])
    ms = list(mats)
    if cap0:
        ms.insert(0, ms[0])
    if cap1:
        ms.append(ms[-1])
    loop_stack(bm, rings, ms)


def cap_prof(alpha_deg, n):
    """Profile of a sphere cap from its base (polar angle alpha) up to the pole: (prof, height as a fraction of the radius)."""
    a0 = math.radians(alpha_deg)
    prof = []
    for k in range(n + 1):
        a = a0 * (1 - k / n)
        prof.append((math.sin(a), (math.cos(a) - math.cos(a0)) / (1 - math.cos(a0))))
    prof[-1] = (0, 1.0)
    return prof, 1 - math.cos(a0)


def cap(bm, c, r, mi, alpha=100, n=4, seg=12, sz=1.0, aspect=1.0, rot=0.0, bottom=False):
    """A sphere cap standing on its base circle: base ring at the height c.z - r*sz*cos(alpha)."""
    prof, hf = cap_prof(alpha, n)
    z0 = c[2] - r * sz * math.cos(math.radians(alpha))
    dome(bm, c[0], c[1], z0, lambda th: r, prof, r * sz * hf, mi, seg, bottom, aspect=aspect, rot=rot)


def ellipsoid(bm, c, rx, ry, rz, mi, rot=0.0, seg=10, rings=6):
    cr, sr = math.cos(rot), math.sin(rot)
    rr = [[(c[0], c[1], c[2] - rz)]]
    for k in range(1, rings):
        t = PI * k / rings
        s = math.sin(t)
        ring = []
        for i in range(seg):
            th = 2 * PI * i / seg
            lx, ly = rx * s * math.cos(th), ry * s * math.sin(th)
            ring.append((c[0] + lx * cr - ly * sr, c[1] + lx * sr + ly * cr, c[2] - rz * math.cos(t)))
        rr.append(ring)
    rr.append([(c[0], c[1], c[2] + rz)])
    loop_stack(bm, rr, mi)


def pillow(bm, poly, z0, h, mi, prof=((1, 0), (1, .4), (.92, .82), (.65, .97), (0, 1.1)), zb=None, bottom=True):
    """A soft slab over a convex counter-clockwise outline: a vertical wall, a rounded shoulder, a gently domed top."""
    cx = sum(p[0] for p in poly) / len(poly)
    cy = sum(p[1] for p in poly) / len(poly)
    rings = []
    for f, hh in prof:
        if f == 0:
            rings.append([(cx, cy, (zb(cx, cy) if zb else z0) + h * hh)])
        else:
            rings.append([(cx + f * (px - cx), cy + f * (py - cy), (zb(cx + f * (px - cx), cy + f * (py - cy)) if zb else z0) + h * hh)
                          for px, py in poly])
    if bottom:
        rings.insert(0, [(cx, cy, (zb(cx, cy) if zb else z0))])
    loop_stack(bm, rings, mi, top=True)


def sector(R, a0, a1, n, apex=(0, 0), rot=0.0, off=(0, 0)):
    """Counter-clockwise outline of a sector: apex, then the arc from a0 to a1 (radians, relative to rot)."""
    pts = [apex] + [(apex[0] + R * math.cos(a0 + (a1 - a0) * k / n), apex[1] + R * math.sin(a0 + (a1 - a0) * k / n)) for k in range(n + 1)]
    cr, sr = math.cos(rot), math.sin(rot)
    return [(off[0] + x * cr - y * sr, off[1] + x * sr + y * cr) for x, y in pts]


def sweep(bm, pts, rad, mi, seg=5, caps=(True, True)):
    """A tube along a polyline (parallel-transport frames); rad: a number or f(s in 0..1)."""
    P = [Vector(p) for p in pts]
    n = len(P)
    R = [rad(i / (n - 1)) if callable(rad) else rad for i in range(n)]
    T = [(P[min(i + 1, n - 1)] - P[max(i - 1, 0)]).normalized() for i in range(n)]
    up = Vector((0, 0, 1)) if abs(T[0].z) < 0.95 else Vector((1, 0, 0))
    y = up.cross(T[0]).normalized()
    rings = []
    for i in range(n):
        if i:
            y = T[i - 1].rotation_difference(T[i]) @ y
        y = (y - T[i] * y.dot(T[i])).normalized()
        z = T[i].cross(y)
        rings.append([tuple(P[i] + R[i] * (y * math.cos(2 * PI * s / seg) + z * math.sin(2 * PI * s / seg))) for s in range(seg)])
    loop_stack(bm, rings, mi, bottom=caps[0], top=caps[1])


def spline(pts, sub=3):
    """Catmull-Rom through the points, sub segments between each pair."""
    P = [Vector(p) for p in pts]
    out = []
    for i in range(len(P) - 1):
        p0, p1, p2, p3 = P[max(i - 1, 0)], P[i], P[i + 1], P[min(i + 2, len(P) - 1)]
        for k in range(sub):
            t = k / sub
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3))
    out.append(P[-1])
    return out


def extrude(bm, poly, z0, z1, mi_side, mi_top=None, mi_bot=None, skip=()):
    """An upright prism over a counter-clockwise outline; z0/z1 numbers or f(x, y); mi_top / mi_bot None = no face;
    skip: side edge indices to leave out."""
    f0 = z0 if callable(z0) else (lambda x, y: z0)
    f1 = z1 if callable(z1) else (lambda x, y: z1)
    lo = [bm.verts.new((x, y, f0(x, y))) for x, y in poly]
    hi = [bm.verts.new((x, y, f1(x, y))) for x, y in poly]
    n = len(poly)
    for i in range(n):
        if i in skip:
            continue
        j = (i + 1) % n
        bm.faces.new((lo[i], lo[j], hi[j], hi[i])).material_index = mi_side
    def fan(ring, mi, f, flip):
        if n <= 4:
            bm.faces.new(ring[::-1] if flip else ring).material_index = mi
            return
        cx, cy = sum(p[0] for p in poly) / n, sum(p[1] for p in poly) / n
        c = bm.verts.new((cx, cy, f(cx, cy)))
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((c, ring[j], ring[i]) if flip else (c, ring[i], ring[j])).material_index = mi

    if mi_bot is not None:
        fan(lo, mi_bot, f0, True)
    if mi_top is not None:
        fan(hi, mi_top, f1, False)


def clip_poly(subject, clipper):
    """Sutherland-Hodgman: subject clipped by a convex counter-clockwise polygon."""
    out = subject
    for i in range(len(clipper)):
        a, b = clipper[i], clipper[(i + 1) % len(clipper)]
        inp, out = out, []
        if not inp:
            break

        def side(p):
            return (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0])

        for k in range(len(inp)):
            p, q = inp[k], inp[(k + 1) % len(inp)]
            sp, sq = side(p), side(q)
            if sp >= 0:
                out.append(p)
            if (sp >= 0) != (sq >= 0):
                t = sp / (sp - sq)
                out.append((p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t))
    return out


def area(poly):
    return 0.5 * sum(poly[i][0] * poly[(i + 1) % len(poly)][1] - poly[(i + 1) % len(poly)][0] * poly[i][1] for i in range(len(poly)))


def cells(bm, xs, us, wf, zf, t, mat, solid=None, place=(0, 0, 0), yshift=None):
    """A thin plate over a grid: x samples xs, column lines us in -1..1 (y = u*wf(x)), top height zf(x, y), thickness t;
    solid(i, j) leaves cells out (the fork's gaps); walls wherever a neighbour is missing. place = (ox, oy, angle)."""
    nx, nu = len(xs) - 1, len(us) - 1
    solid = solid or (lambda i, j: True)
    cache = {}
    ca, sa = math.cos(place[2]), math.sin(place[2])

    def V(i, j, up):
        k = (i, j, up)
        if k not in cache:
            x = xs[i]
            y = us[j] * wf(x) + (yshift(x) if yshift else 0.0)
            z = zf(x, y) - (0 if up else t)
            cache[k] = bm.verts.new((place[0] + x * ca - y * sa, place[1] + x * sa + y * ca, z))
        return cache[k]

    for i in range(nx):
        for j in range(nu):
            if not solid(i, j):
                continue
            m = mat(i, j) if callable(mat) else mat
            bm.faces.new((V(i, j, 1), V(i + 1, j, 1), V(i + 1, j + 1, 1), V(i, j + 1, 1))).material_index = m
            bm.faces.new((V(i, j, 0), V(i, j + 1, 0), V(i + 1, j + 1, 0), V(i + 1, j, 0))).material_index = m
            if j == 0 or not solid(i, j - 1):
                bm.faces.new((V(i, j, 1), V(i, j, 0), V(i + 1, j, 0), V(i + 1, j, 1))).material_index = m
            if j == nu - 1 or not solid(i, j + 1):
                bm.faces.new((V(i, j + 1, 1), V(i + 1, j + 1, 1), V(i + 1, j + 1, 0), V(i, j + 1, 0))).material_index = m
            if i == 0 or not solid(i - 1, j):
                bm.faces.new((V(i, j, 1), V(i, j + 1, 1), V(i, j + 1, 0), V(i, j, 0))).material_index = m
            if i == nx - 1 or not solid(i + 1, j):
                bm.faces.new((V(i + 1, j, 1), V(i + 1, j, 0), V(i + 1, j + 1, 0), V(i + 1, j + 1, 1))).material_index = m


def interp(tab, x):
    if x <= tab[0][0]:
        return tab[0][1]
    for (x0, y0), (x1, y1) in zip(tab, tab[1:]):
        if x <= x1:
            return y0 + (y1 - y0) * (x - x0) / (x1 - x0)
    return tab[-1][1]


def smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


def crease(bm, deg=38):
    """Hard edges where faces meet at more than deg degrees (so smooth shading doesn't bleed over rims and caps)."""
    bm.normal_update()
    lim = math.radians(deg)
    for e in bm.edges:
        e.smooth = not (len(e.link_faces) == 2 and e.link_faces[0].normal.angle(e.link_faces[1].normal, 0) > lim)


# ---- china and vessels ------------------------------------------------------------------------------------------------------

def plate():
    bm = bmesh.new()
    CH, RIM = M("DN_China"), M("DN_ChinaRim")
    prof = [(0.060, 0.0030), (0.068, 0.0), (0.080, 0.0), (0.1115, 0.0100), (0.1200, 0.0190), (0.1180, 0.0228), (0.1135, 0.0230),
            (0.1060, 0.0226), (0.0985, 0.0212), (0.0890, 0.0150), (0.0840, PLATE_TOP), (0.060, PLATE_TOP)]
    mats = [CH, CH, CH, CH, CH, CH, RIM, CH, CH, CH, CH]
    C.lathe(bm, prof, mats, 28)
    return bm


def bowl():
    bm = bmesh.new()
    CH, RIM = M("DN_China"), M("DN_ChinaRim")
    prof = [(0.030, 0.0), (0.038, 0.0), (0.044, 0.004), (0.064, 0.018), (0.0760, 0.042), (0.07733, 0.048), (0.0793, 0.057),
            (0.0800, 0.0600), (0.0795, 0.0640), (0.0770, 0.0652), (BOWL_IN_R, 0.0640), (0.0700, 0.040), (0.0500, 0.018),
            (0.0300, BOWL_IN_Z), (0.0200, BOWL_IN_Z)]
    mats = [CH] * 15
    mats[5] = RIM
    C.lathe(bm, prof, mats, 28)
    return bm


def bowl_inner_r(z):
    pts = [(BOWL_IN_Z, 0.0300), (0.018, 0.0500), (0.040, 0.0700), (0.064, BOWL_IN_R)]
    return interp(pts, z)


def mug():
    bm = bmesh.new()
    CH, RIM = M("DN_China"), M("DN_ChinaRim")
    prof = [(0.034, 0.0), (0.0408, 0.0012), (MUG_R, 0.0040), (MUG_R, 0.0620), (MUG_R, 0.0740), (MUG_R, 0.0925), (0.0412, 0.0950),
            (0.0388, 0.0950), (MUG_IN_R, 0.0925), (MUG_IN_R, 0.0115), (0.0345, MUG_IN_Z), (0.0300, MUG_IN_Z)]
    mats = [CH, CH, CH, RIM, CH, CH, CH, CH, CH, CH, CH]
    C.lathe(bm, prof, mats, 28)
    path = spline([(0.0, 0.0405, 0.082), (0.0, 0.0520, 0.088), (0.0, 0.0625, 0.080), (0.0, 0.0665, 0.062), (0.0, 0.0615, 0.046),
                   (0.0, 0.0525, 0.036), (0.0, 0.0405, 0.032)], 3)
    sweep(bm, path, 0.0055, CH, 8)
    return bm


def coffee_fill():
    bm = bmesh.new()
    C.lathe(bm, [(MUG_IN_R - 0.001, 0.0), (MUG_IN_R - 0.001, MUG_FILL_H)], [M("DN_Coffee")], 28)
    return bm


def glass_ro(z):                       # circumradius of the glass's outside at height z
    return 0.0310 + 0.0050 * (z - GLASS_Z0) / (GLASS_H - GLASS_Z0)


def tea_glass():
    bm = bmesh.new()
    G = M("DN_Glass")
    rim_in = glass_ro(GLASS_H) - 0.0025
    fl_in = glass_ro(GLASS_IN_Z) - 0.0025
    prof = [(0.026, GLASS_Z0), (glass_ro(GLASS_Z0), GLASS_Z0), (glass_ro(GLASS_H), GLASS_H), (rim_in, GLASS_H), (fl_in, GLASS_IN_Z),
            (0.024, GLASS_IN_Z)]
    C.lathe(bm, prof, [G], 12)
    # the podstakannik: a thin chrome-and-steel cup round the lower three quarters, an ornamented band, a handle on +y
    ST, CR = M("DN_Steel"), M("DN_Chrome")
    ho = lambda z: glass_ro(z) + 0.0022
    zt = 0.0755
    zs = [0.0150 + 0.0034 * k for k in range(0, 14)]
    bump = [0, 0, 0.0012, 0.0018, 0.0012, 0, 0.0012, 0.0018, 0.0012, 0, 0.0012, 0.0018, 0.0012, 0]
    pr = [(0.022, 0.0), (0.0300, 0.0), (ho(0.0035) - 0.0016, 0.0030)]
    mt = [ST, ST]
    for z, b in zip(zs, bump):
        pr.append((ho(z) + b, z))
        mt.append(CR if b else ST)
    zl = zs[-1]
    pr += [(ho(zt) + 0.0007, zt - 0.0004), (ho(zt) + 0.0005, zt + 0.0010), (ho(zt) - 0.0006, zt + 0.0010)]
    mt += [ST, ST, ST]
    # inner wall: just outside the glass (circumradius + 0.8 mm), down to the floor
    pr += [(glass_ro(zt) + 0.0008, zt - 0.0004), (glass_ro(0.0040) + 0.0008, 0.0040), (0.020, 0.0040)]
    mt += [ST, ST]
    mt = mt[:len(pr) - 1]
    C.lathe(bm, pr, mt, 24)
    hp = spline([(0.0, ho(0.060) - 0.0008, 0.061), (0.0, 0.0455, 0.067), (0.0, 0.0555, 0.060), (0.0, 0.0605, 0.045),
                 (0.0, 0.0555, 0.029), (0.0, 0.0460, 0.019), (0.0, ho(0.020) - 0.0008, 0.0185)], 3)
    sweep(bm, hp, 0.0027, ST, 6)
    return bm


def glass_fill():
    bm = bmesh.new()
    C.lathe(bm, [(0.0272, 0.0), (0.0310, GLASS_FILL_H)], [M("DN_Tea")], 12)
    return bm


# ---- cutlery ----------------------------------------------------------------------------------------------------------------

FORK_W = [(-0.06, 0.0045), (-0.05, 0.0085), (-0.035, 0.0090), (-0.015, 0.0070), (0.0, 0.0050), (0.03, 0.0042), (0.055, 0.0055),
          (0.066, 0.0085), (0.078, 0.0106), (0.115, 0.0106), (0.13, 0.0098)]


def fork_ztop(x):
    return 0.002 + 0.004 * (max(0.0, x - 0.05) / (TINE_X - 0.05)) ** 2


def tine_z(dx):
    """Top of the fork's tines at dx ahead of the load point, relative to the load origin."""
    return fork_ztop(TINE_X + dx) - LOAD_Z


def fork():
    bm = bmesh.new()
    xs = [-0.06, -0.05, -0.03, 0.0, 0.03, 0.055, 0.078, 0.096, 0.115, 0.130]
    us = [-1, -0.698, -0.434, -0.132, 0.132, 0.434, 0.698, 1]
    ig = xs.index(0.078)
    cells(bm, xs, us, lambda x: interp(FORK_W, x), lambda x, y: fork_ztop(x), 0.0016, M("DN_Steel"),
          solid=lambda i, j: not (i >= ig and j in (1, 3, 5)))
    return bm


SP_A, SP_B = 0.032, 0.0195


def spoon_zin(x, y):
    """Top (inner) surface of the spoon."""
    s = (x - TINE_X) / SP_A
    if s < -1:
        t = smoothstep((x - 0.02) / 0.063)
        return 0.002 + 0.009 * t
    return LOAD_Z + 0.005 * s * s + 0.005 * (y / SP_B) ** 2


def spoon():
    bm = bmesh.new()
    xs = [-0.06, -0.045, -0.02, 0.01, 0.04, 0.065]
    xs += [TINE_X - SP_A * math.cos(PI * k / 10) for k in range(0, 11)]

    def w(x):
        s = (x - TINE_X) / SP_A
        hw = interp([(-0.06, 0.0045), (-0.05, 0.0075), (-0.035, 0.0085), (-0.015, 0.0070), (0.0, 0.0050), (0.03, 0.0042), (0.065, 0.0055)], x)
        if s < -1:
            return hw
        ell = SP_B * math.sqrt(max(0.0, 1 - s * s))
        return max(ell, hw if s < 0 else 0.0022)

    us = [-1 + 2 * k / 8 for k in range(9)]
    cells(bm, xs, us, w, spoon_zin, 0.0014, M("DN_Steel"))
    return bm


# ---- food -------------------------------------------------------------------------------------------------------------------

def rot2(x, y, a):
    return x * math.cos(a) - y * math.sin(a), x * math.sin(a) + y * math.cos(a)


def pancakes():
    bm = bmesh.new()
    PC = M("DN_Pancake")
    z0, h = 0.002, 0.008
    cents = []
    for psi in (math.radians(78), math.radians(198), math.radians(318)):
        d = 0.008
        poly = sector(0.074, -PI / 4, PI / 4, 9, apex=(0, 0), rot=psi, off=(d * math.cos(psi), d * math.sin(psi)))
        pillow(bm, poly, z0, h, PC)
        cents.append((sum(p[0] for p in poly) / len(poly), sum(p[1] for p in poly) / len(poly)))
    prof = [(1, 0), (1, .22), (.86, .55), (.58, .85), (.28, 1.0), (0, 1.1)]
    dome(bm, cents[0][0], cents[0][1], z0 + h * 1.0 - 0.0012, lambda th: 0.0165 * (1 + 0.08 * math.sin(3 * th + 1)), prof, 0.014,
         M("DN_SourCream"), 20, bottom=False)
    dome(bm, cents[1][0], cents[1][1], z0 + h * 1.0 - 0.0012, lambda th: 0.0145 * (1 + 0.10 * math.sin(4 * th + 2)), prof, 0.0085,
         M("DN_Jam"), 20, bottom=False)
    return bm


def blob_r(R, seed):
    rr = random.Random(seed)
    p = [rr.uniform(0, 6.3) for _ in range(3)]
    return lambda th: R * (1 + 0.13 * math.sin(2 * th + p[0]) + 0.09 * math.sin(3 * th + p[1]) + 0.05 * math.sin(5 * th + p[2]))


def eggs_bacon():
    bm = bmesh.new()
    EW, YK, BA, TO = M("DN_EggWhite"), M("DN_Yolk"), M("DN_Bacon"), M("DN_Toast")
    z0, h = 0.002, 0.0042
    wp = [(1, 0), (1, .45), (.93, .85), (.72, 1), (.35, 1.02), (0, 1.05)]
    for k, (ex, ey, yx, yy) in enumerate([(0.006, 0.047, 0.003, 0.002), (0.004, -0.047, -0.002, -0.003)]):
        dome(bm, ex, ey, z0, blob_r(0.031, 10 + k), wp, h, EW, 24)
        cap(bm, (ex + yx, ey + yy, z0 + h + 0.0020), 0.0125, YK, alpha=100, n=4, seg=14, sz=0.8)
    # two wavy bacon strips across the plate: stripes of meat and fat
    xs = [k / 12 * 0.12 - 0.06 for k in range(13)]
    for (cx, cy, L, ang, ph, wide) in [(0.056, 0.0, 0.120, PI / 2 + 0.04, 0.0, 0.0185), (0.079, 0.001, 0.07, PI / 2 - 0.03, 2.0, 0.0165)]:
        sx = [x * L / 0.12 for x in xs]
        us = [-1, -0.45, -0.15, 0.15, 0.45, 1]
        cells(bm, sx, us, lambda x, wd=wide: wd / 2, lambda x, y, ph=ph, L=L: 0.0030 + 0.0024 + 0.0022 * math.sin(x / L * 17 + ph) + 0.0004 * (y > 0),
              0.0026, lambda i, j: M("DN_Bacon") if j in (0, 2, 4) else M("DN_Fish"),
              place=(cx, cy, ang), yshift=lambda x, ph=ph: 0.0035 * math.sin(x * 55 + ph))
    # two toast triangles
    for sgn in (1, -1):
        poly = [(-0.032, sgn * 0.008), (-0.078, sgn * 0.008), (-0.032, sgn * 0.054)]
        if sgn < 0:
            poly = poly[::-1]
        pillow(bm, poly, z0, 0.010, TO, prof=((1, 0), (1, .5), (.93, .9), (.74, 1), (0, 1.03)))
    return bm


def spaghetti():
    bm = bmesh.new()
    NO, SA, MB = M("DN_Noodle"), M("DN_Sauce"), M("DN_Meatball")
    rr = random.Random(7)
    layers = [4, 4, 3, 3, 2]
    for L, cnt in enumerate(layers):
        zc = 0.0044 + L * 0.0056
        rmax = 0.052 - L * 0.0062
        for _ in range(cnt):
            n = 11
            ph0 = rr.uniform(0, 2 * PI)
            d = rr.choice((-1, 1))
            sweep_a = rr.uniform(1.4 * PI, 2.1 * PI)
            r0 = rr.uniform(0.4, 0.8) * rmax
            fa, pa = rr.uniform(1.0, 2.2), rr.uniform(0, 6.3)
            fz, pz = rr.uniform(1.5, 3.0), rr.uniform(0, 6.3)
            pts = []
            for k in range(n + 1):
                s = k / n
                ph = ph0 + d * sweep_a * s
                rho = min(rmax, max(0.008, r0 + 0.4 * rmax * math.sin(2 * PI * fa * s + pa) + rr.uniform(-0.002, 0.002)))
                z = zc + 0.0014 * math.sin(2 * PI * fz * s + pz)
                pts.append((rho * math.cos(ph), rho * math.sin(ph), z))
            sweep(bm, pts, 0.0021, NO, 5)
    # tomato sauce on top
    dome(bm, 0.003, -0.002, 0.0245, lambda th: 0.026 * (1 + 0.16 * math.sin(3 * th + 0.5) + 0.08 * math.sin(5 * th + 2)),
         [(1, 0), (1, .3), (.82, .72), (.5, 1.0), (0, 1.1)], 0.012, SA, 20)
    for a in (math.radians(25), math.radians(145), math.radians(265)):
        ellipsoid(bm, (0.069 * math.cos(a), 0.069 * math.sin(a), 0.002 + 0.0115 * 0.97), 0.0115, 0.0115, 0.0112, MB, rot=a, seg=10, rings=6)
    return bm


def apple_pie():
    bm = bmesh.new()
    CR, FI = M("DN_Crust"), M("DN_PieFilling")
    R, tip = 0.108, (-0.040, 0.0)
    a0, a1 = -PI / 8, PI / 8
    n = 12
    zb, zf, zc = 0.002, 0.0045, 0.0285
    outer = [(tip[0] + (R + (0.0018 if k % 2 else 0)) * math.cos(a0 + (a1 - a0) * k / n), tip[1] + (R + (0.0018 if k % 2 else 0)) * math.sin(a0 + (a1 - a0) * k / n)) for k in range(n + 1)]
    inner = [(tip[0] + (R - 0.014) * math.cos(a0 + (a1 - a0) * k / n), tip[1] + (R - 0.014) * math.sin(a0 + (a1 - a0) * k / n)) for k in range(n + 1)]
    arc0 = [(tip[0] + R * math.cos(a0 + (a1 - a0) * k / n), tip[1] + R * math.sin(a0 + (a1 - a0) * k / n)) for k in range(n + 1)]
    base = [tip] + arc0
    extrude(bm, base, zb, zf, CR, mi_top=None, mi_bot=CR)                               # the pastry bottom
    fil = [tip] + inner
    extrude(bm, fil, zf, zc, FI, mi_top=None, mi_bot=None)                              # the apple filling (its top: the lattice's holes)
    # the crimped collar of the crust (arc band), cut faces at both ends
    ct = 0.038
    vo = [(x, y) for x, y in outer]
    for k in range(n):
        A, B = outer[k], outer[k + 1]
        Ai, Bi = inner[k], inner[k + 1]
        v = lambda p, z: bm.verts.new((p[0], p[1], z))
        bm.faces.new((v(A, zf), v(B, zf), v(B, ct - 0.002), v(A, ct - 0.002))).material_index = CR            # outer wall
        bm.faces.new((v(A, ct - 0.002), v(B, ct - 0.002), v(B, ct), v(A, ct))) .material_index = CR
        bm.faces.new((v(A, ct), v(B, ct), v(Bi, ct), v(Ai, ct))).material_index = CR                           # top
        bm.faces.new((v(Ai, ct), v(Bi, ct), v(Bi, zc), v(Ai, zc))).material_index = CR                         # inner wall
    for e, (A, Ai), flip in ((0, (outer[0], inner[0]), False), (1, (outer[n], inner[n]), True)):
        v = lambda p, z: bm.verts.new((p[0], p[1], z))
        q = (v(A, zf), v(Ai, zf), v(Ai, ct), v(A, ct))
        bm.faces.new(q if not flip else q[::-1]).material_index = CR
    # the lattice: strips along x, then across, clipped to the wedge a bit inside the collar
    clipper = [tip] + [(tip[0] + (R - 0.013) * math.cos(a0 + (a1 - a0) * k / n), tip[1] + (R - 0.013) * math.sin(a0 + (a1 - a0) * k / n)) for k in range(n + 1)]
    wd, sp = 0.0070, 0.0155
    for k in range(-3, 4):
        y0 = k * sp
        rect = [(tip[0] - 0.01, y0 - wd / 2), (tip[0] + R + 0.01, y0 - wd / 2), (tip[0] + R + 0.01, y0 + wd / 2), (tip[0] - 0.01, y0 + wd / 2)]
        p = clip_poly(rect, clipper)
        if len(p) >= 3 and area(p) > 6e-6:
            extrude(bm, p, 0.0286, 0.0346, CR, CR, CR)
    gaps = [(-0.07, -3 * sp - wd / 2)] + [(k * sp + wd / 2, (k + 1) * sp - wd / 2) for k in range(-3, 3)] + [(3 * sp + wd / 2, 0.07)]
    for k in range(0, 7):
        x0 = tip[0] + 0.012 + k * 0.0145
        for ya, yb in gaps:
            rect = [(x0 - wd / 2, ya), (x0 + wd / 2, ya), (x0 + wd / 2, yb), (x0 - wd / 2, yb)]
            p = clip_poly(rect, clipper)
            if len(p) >= 3 and area(p) > 6e-6:
                # the ends that touch a lengthwise strip are left open (hidden contact faces)
                sk = tuple(i for i in range(len(p)) if any(abs(p[i][1] - yy) < 1e-7 and abs(p[(i + 1) % len(p)][1] - yy) < 1e-7
                                                           for yy in (ya, yb) if abs(yy) < 0.069))
                extrude(bm, p, 0.0286, 0.0358, CR, CR, CR, skip=sk)
    xl = [tip[0] + 0.012 + k * 0.0145 for k in range(7)]
    xg = [(-1, xl[0] - wd / 2)] + [(xl[k] + wd / 2, xl[k + 1] - wd / 2) for k in range(6)] + [(xl[-1] + wd / 2, 1)]
    for xa, xb in xg:
        for ya, yb in gaps:
            p = clip_poly([(xa, ya), (xb, ya), (xb, yb), (xa, yb)], fil)
            if len(p) >= 3 and area(p) > 3e-6:
                if len(p) == 3:
                    bm.faces.new([bm.verts.new((x, y, zc)) for x, y in p]).material_index = FI
                else:
                    cx, cy = sum(q[0] for q in p) / len(p), sum(q[1] for q in p) / len(p)
                    cv = bm.verts.new((cx, cy, zc))
                    vv = [bm.verts.new((x, y, zc)) for x, y in p]
                    for i in range(len(p)):
                        bm.faces.new((cv, vv[i], vv[(i + 1) % len(p)])).material_index = FI
    return bm


def burger(parts=None):
    """parts: a list to receive every layer as its own closed bmesh (for the bitten stages); None: one open-bottomed mesh."""
    bm = bmesh.new()
    cl = parts is not None

    def P():
        if parts is None:
            return bm if parts is None else None
        nb = bmesh.new()
        parts.append(nb)
        return nb

    BU, PA, CHE, LE, TOM, SE = M("DN_Bun"), M("DN_Patty"), M("DN_Cheese"), M("DN_Lettuce"), M("DN_Sauce"), M("DN_Noodle")
    z0 = BURGER_Z0
    # the bottom bun
    C.lathe(P(), [(0.038, z0), (0.0460, z0 + 0.0015), (0.0495, z0 + 0.0060), (0.0505, z0 + 0.0115), (0.0495, z0 + 0.0165),
                 (0.0465, 0.0190), (0.0400, 0.0192)], [BU], 24, cap1=cl)
    # the patty
    C.lathe(P(), [(0.0440, 0.0192), (0.0490, 0.0198), (0.0502, 0.0250), (0.0500, 0.0310), (0.0480, 0.0343), (0.0420, 0.0345)],
            [PA], 24, cap0=cl, cap1=cl)
    # the cheese slice: a square set on its corner, the corners melted down the patty
    rr = random.Random(3)

    def wall_r(th):
        t = th - PI / 4
        return 0.0455 / (abs(math.cos(t)) ** 4 + abs(math.sin(t)) ** 4) ** 0.25

    def drop(r):
        return max(0.0, r - 0.0500) * 3.6

    zt = 0.0367
    rings = [[(0, 0, zt - 0.002)]]
    fr = [0.45, 0.8, 0.93, 1.0]
    seg = 28
    for f in fr:
        rings.append([(wall_r(2 * PI * i / seg) * f * math.cos(2 * PI * i / seg), wall_r(2 * PI * i / seg) * f * math.sin(2 * PI * i / seg),
                       zt - 0.002 - drop(wall_r(2 * PI * i / seg) * f)) for i in range(seg)])
    for f in reversed(fr):
        ring = []
        for i in range(seg):
            th = 2 * PI * i / seg
            r = wall_r(th) * f
            ring.append((r * math.cos(th), r * math.sin(th), zt - drop(r)))
        rings.append(ring)
    rings.append([(0, 0, zt)])
    loop_stack(P(), rings, CHE)
    # tomato
    lathe_fan(P(), [(0.0400, 0.0371), (0.0430, 0.0373), (0.0440, 0.0385), (0.0430, 0.0398), (0.0390, 0.0400)], [TOM], 24, cap0=cl, cap1=True)
    # lettuce frill: a closed ruffled ring
    seg = 36
    ring_o_t, ring_i_t, ring_i_b, ring_o_b = [], [], [], []
    for i in range(seg):
        th = 2 * PI * i / seg
        ro = 0.0545 + 0.0030 * math.sin(7 * th + 0.6)
        zo = 0.0408 + 0.0024 * math.sin(7 * th + 2.2) + 0.0008 * math.sin(11 * th)
        ring_o_t.append((ro * math.cos(th), ro * math.sin(th), zo + 0.0012))
        ring_i_t.append((0.030 * math.cos(th), 0.030 * math.sin(th), 0.0414))
        ring_i_b.append((0.030 * math.cos(th), 0.030 * math.sin(th), 0.0402))
        ring_o_b.append(((ro - 0.0012) * math.cos(th), (ro - 0.0012) * math.sin(th), zo - 0.0012))
    loop_stack(P(), [ring_o_t, ring_i_t, ring_i_b, ring_o_b], LE, closed=True)
    # the top bun
    zb = 0.0452
    pr = [(0.0435, zb), (0.0502, zb + 0.0002)]
    for a in (0, 18, 36, 54, 70, 82):
        al = math.radians(a)
        pr.append((0.0517 * math.cos(al), 0.0520 + 0.0280 * math.sin(al)))
    pr.append((0.0, 0.0800))
    pr[-2] = (0.0517 * math.cos(math.radians(82)), 0.0520 + 0.0280 * math.sin(math.radians(82)))
    if cl:
        pr.insert(0, (0.0, zb))
    rings = [[(r * math.cos(2 * PI * i / 28), r * math.sin(2 * PI * i / 28), z) for i in range(28)] if r > 0 else [(0, 0, z)] for r, z in pr]
    loop_stack(P(), rings, BU)
    # sesame seeds
    rs = random.Random(5)
    sb = P()
    for k in range(10):
        th = 2 * PI * (k + rs.uniform(-0.3, 0.3)) / 10
        al = math.radians(rs.uniform(66, 80))
        r = 0.0517 * math.cos(al)
        z = 0.0520 + 0.0280 * math.sin(al)
        ellipsoid(sb, (r * math.cos(th), r * math.sin(th), z + 0.0001), 0.0026, 0.0014, 0.0010, SE, rot=th + PI / 2, seg=6, rings=3)
    return bm if parts is None else None


def soup_fill():
    bm = bmesh.new()
    SO, FI, NO, HE = M("DN_Soup"), M("DN_Fish"), M("DN_Noodle"), M("DN_Herb")
    zs = [BOWL_IN_Z, 0.018, 0.030, 0.040, SOUP_TOP]
    prof = [(bowl_inner_r(z) - 0.0015, z - BOWL_IN_Z) for z in zs]
    C.lathe(bm, prof, [SO], 28)
    zt = SOUP_FILL_H
    rr = random.Random(11)
    spots = [(0.030, 0.010), (-0.020, 0.030), (-0.035, -0.020), (0.012, -0.035), (-0.005, 0.004), (0.040, -0.025), (-0.045, 0.012)]
    for k, (x, y) in enumerate(spots[:3]):
        cap(bm, (x, y, zt + 0.0014), 0.0105, FI, alpha=105, n=4, seg=10, sz=0.55, aspect=0.7, rot=rr.uniform(0, 3), bottom=False)
    for (x, y) in spots[3:5]:
        a = rr.uniform(0, 1.5)
        box = Matrix.Rotation(a, 3, "Z")
        C.box(bm, (x, y, zt + 0.0020), (0.0105, 0.0105, 0.0105), NO, rot=box, faces="xXyYZ")
    for (x, y) in spots[5:7]:
        for j in range(3):
            a = rr.uniform(0, PI) + j * 1.0
            L = 0.011
            p0 = (x - 0.5 * L * math.cos(a), y - 0.5 * L * math.sin(a), zt + 0.0009)
            p1 = (x + 0.5 * L * math.cos(a), y + 0.5 * L * math.sin(a), zt + 0.0011 + 0.0006 * j)
            C.tube(bm, p0, p1, 0.0007, HE, 4)
    return bm


# ---- loads (what rides on a fork / spoon after a scoop; origin = the load point, z 0 = the tines' top there) ------------------

def strand_pts(y0, ph):
    pts = []
    for dx in (-0.020, -0.012, -0.004, 0.004, 0.011, 0.0155):
        pts.append((dx, y0 + 0.0008 * math.sin(dx * 300 + ph), tine_z(dx) + 0.0019))
    pts += [(0.0195, y0 + 0.001, tine_z(0.0155) + 0.0003), (0.0245, y0 + 0.0014, -0.0020), (0.0272, y0 + 0.0012, -0.0115),
            (0.0280, y0 + 0.0002, -0.0220), (0.0262, y0 - 0.0008, -0.0305)]
    return pts


def load_noodles():
    bm = bmesh.new()
    NO, SA = M("DN_Noodle"), M("DN_Sauce")
    for y0, ph, rad in ((-0.0030, 0.0, 0.0017), (0.0035, 2.0, 0.0017)):
        sweep(bm, spline(strand_pts(y0, ph), 2), lambda s, rad=rad: rad * (1 - 0.3 * s * s), NO, 5)
    dome(bm, -0.004, 0.0003, tine_z(-0.004) + 0.0020, lambda th: 0.0070 * (1 + 0.15 * math.sin(3 * th + 1)),
         [(1, 0), (1, .3), (.8, .75), (.45, 1.0), (0, 1.1)], 0.0042, SA, 14)
    return bm


def load_twirl():
    bm = bmesh.new()
    NO, SA = M("DN_Noodle"), M("DN_Sauce")
    zc = -0.0008
    for layer, (dr, turns, d, ph) in enumerate([(0.0, 7.0, 1, 0.0), (0.0033, 6.5, -1, 1.7)]):
        pts = []
        npt = int(turns * 9)
        for k in range(npt + 1):
            s = k / npt
            a = -0.0165 + 0.033 * s
            rho = 0.0127 + dr + 0.0040 * math.sqrt(max(0.0, 1 - (a / 0.0175) ** 2)) * (1 - 0.0 * layer)
            psi = ph + d * 2 * PI * turns * s
            pts.append((a, rho * math.cos(psi), zc + rho * math.sin(psi)))
        sweep(bm, pts, lambda s: 0.0015 * (0.7 + 0.3 * math.sin(PI * s) ** 0.5), NO, 5)
    dome(bm, -0.001, 0.0, zc + 0.0125, lambda th: 0.0062 * (1 + 0.12 * math.sin(3 * th)), [(1, 0), (1, .3), (.75, .8), (.4, 1.0), (0, 1.1)],
         0.0040, SA, 12)
    return bm


def load_pancake():
    bm = bmesh.new()
    PC, JM = M("DN_Pancake"), M("DN_Jam")
    zb = lambda x, y: tine_z(x) + 0.0003
    poly = sector(0.030, -math.radians(42), math.radians(42), 6, apex=(0, 0), off=(-0.0155, 0.0))
    pillow(bm, poly, 0.0, 0.0085, PC, zb=zb)
    dome(bm, 0.0035, 0.0, tine_z(0.0035) + 0.0085 - 0.0010, lambda th: 0.0078 * (1 + 0.1 * math.sin(3 * th + 0.4)),
         [(1, 0), (1, .25), (.8, .65), (.45, .95), (0, 1.1)], 0.0050, JM, 14, bottom=False)
    return bm


def load_egg():
    bm = bmesh.new()
    EW, YK, BA = M("DN_EggWhite"), M("DN_Yolk"), M("DN_Bacon")
    zb = lambda x, y: tine_z(x) + 0.0003
    dome(bm, -0.002, 0.0, 0.0, blob_r(0.0150, 21), [(1, 0), (1, .45), (.92, .85), (.7, 1), (.3, 1.02), (0, 1.05)], 0.0048, EW, 16, zb=zb,
         aspect=0.8)
    cap(bm, (0.0035, 0.0006, tine_z(0.0035) + 0.0048 + 0.0018), 0.0072, YK, alpha=100, n=3, seg=10, sz=0.8)
    cx, cy = -0.0105, -0.0085
    C.box(bm, (cx, cy, tine_z(cx) + 0.0048 + 0.0008), (0.0150, 0.0055, 0.0024), BA, rot=Matrix.Rotation(0.5, 3, "Z"))
    return bm


def load_pie():
    bm = bmesh.new()
    FI, CR = M("DN_PieFilling"), M("DN_Crust")
    poly = [(-0.015, -0.0095), (0.011, -0.0125), (0.016, 0.0), (0.011, 0.0125), (-0.015, 0.0095)]
    zb = lambda x, y: tine_z(x) + 0.0003
    extrude(bm, poly, zb, lambda x, y: zb(x, y) + 0.0075, FI, mi_top=None, mi_bot=FI)
    big = [(x * 1.06, y * 1.06) for x, y in poly]
    extrude(bm, big, lambda x, y: zb(x, y) + 0.0075, lambda x, y: zb(x, y) + 0.0118, CR, mi_top=CR, mi_bot=None)
    return bm


def load_soup():
    bm = bmesh.new()
    SO, FI, HE = M("DN_Soup"), M("DN_Fish"), M("DN_Herb")
    L = 0.0038
    rmax = math.sqrt(L / 0.005)
    seg = 24
    zin = lambda rho: 0.005 * rho * rho

    def ring(f, z):
        return [(SP_A * rmax * f * math.cos(2 * PI * i / seg), SP_B * rmax * f * math.sin(2 * PI * i / seg), z) for i in range(seg)]

    rings = [[(0, 0, 0.0002)], ring(0.5, zin(0.5 * rmax) + 0.0002), ring(0.85, zin(0.85 * rmax) + 0.0002), ring(1.0, zin(rmax) + 0.0002),
             ring(1.0, L + 0.0004), ring(0.85, L + 0.0008), ring(0.5, L + 0.0012), [(0, 0, L + 0.0014)]]
    loop_stack(bm, rings, SO)
    cap(bm, (0.004, 0.002, L + 0.0020), 0.0085, FI, alpha=105, n=4, seg=10, sz=0.55, aspect=0.7, rot=0.4)
    for j in range(3):
        a = 0.6 + 1.1 * j
        p0 = (-0.011 - 0.005 * math.cos(a), -0.003 - 0.005 * math.sin(a), L + 0.0010)
        p1 = (-0.011 + 0.005 * math.cos(a), -0.003 + 0.005 * math.sin(a), L + 0.0012 + 0.0007 * j)
        C.tube(bm, p0, p1, 0.0006, HE, 4)
    return bm


# ---- previews ---------------------------------------------------------------------------------------------------------------

BUILD = {
    "Plate": (plate, CHINA), "Bowl": (bowl, CHINA), "Mug": (mug, CHINA), "CoffeeFill": (coffee_fill, ["DN_Coffee"]),
    "TeaGlass": (tea_glass, ["DN_Steel", "DN_Chrome"]), "GlassFill": (glass_fill, ["DN_Tea"]),
    "Fork": (fork, []), "Spoon": (spoon, []),
    "Pancakes": (pancakes, FOOD_MATS), "EggsBacon": (eggs_bacon, FOOD_MATS), "Spaghetti": (spaghetti, FOOD_MATS),
    "ApplePie": (apple_pie, FOOD_MATS), "Burger": (burger, FOOD_MATS), "SoupFill": (soup_fill, FOOD_MATS),
    "Load_Noodles": (load_noodles, FOOD_MATS), "Load_Twirl": (load_twirl, FOOD_MATS), "Load_Pancake": (load_pancake, FOOD_MATS),
    "Load_Egg": (load_egg, FOOD_MATS), "Load_Pie": (load_pie, FOOD_MATS), "Load_Soup": (load_soup, FOOD_MATS),
}
ORDER = list(BUILD)
BITE_EXPORT = "SM_Food_diner_burger_%d"
# what to draw under / round each preview: [(builder name, offset)]
COMPANION = {
    "CoffeeFill": [("Mug", (0, 0, 0)), ("CoffeeFill", (0, 0, MUG_IN_Z))],
    "GlassFill": [("TeaGlass", (0, 0, 0)), ("GlassFill", (0, 0, GLASS_IN_Z))],
    "SoupFill": [("Plate", (0, 0, -PLATE_TOP)), ("Bowl", (0, 0, 0)), ("SoupFill", (0, 0, BOWL_IN_Z))],
    "Pancakes": [("Plate", (0, 0, -PLATE_TOP)), ("Pancakes", (0, 0, 0))],
    "EggsBacon": [("Plate", (0, 0, -PLATE_TOP)), ("EggsBacon", (0, 0, 0))],
    "Spaghetti": [("Plate", (0, 0, -PLATE_TOP)), ("Spaghetti", (0, 0, 0))],
    "ApplePie": [("Plate", (0, 0, -PLATE_TOP)), ("ApplePie", (0, 0, 0))],
    "Burger": [("Plate", (0, 0, -PLATE_TOP)), ("Burger", (0, 0, 0))],
    "Load_Noodles": [("Fork", (-TINE_X, 0, -LOAD_Z)), ("Load_Noodles", (0, 0, 0))],
    "Load_Twirl": [("Fork", (-TINE_X, 0, -LOAD_Z)), ("Load_Twirl", (0, 0, 0))],
    "Load_Pancake": [("Fork", (-TINE_X, 0, -LOAD_Z)), ("Load_Pancake", (0, 0, 0))],
    "Load_Egg": [("Fork", (-TINE_X, 0, -LOAD_Z)), ("Load_Egg", (0, 0, 0))],
    "Load_Pie": [("Fork", (-TINE_X, 0, -LOAD_Z)), ("Load_Pie", (0, 0, 0))],
    "Load_Soup": [("Spoon", (-TINE_X, 0, -LOAD_Z)), ("Load_Soup", (0, 0, 0))],
}
PREV_VIEW = {"Fork": (1, -0.8, 0.9), "Spoon": (1, -0.8, 0.9)}


# ---- the burger in the palm: bite stages (SM_Food_diner_burger_<k>) ---------------------------------------------------------
# Palm frame: origin the middle of the right palm on the skin, +X along the fingers, +Z out of the palm, +Y to the little finger.
# The burger lies on its side: its axis (bottom bun -> top bun) along +Y, centre at X = 0.02, its rounded side on the palm at
# Z = 0.003; the far side (+Z) is bitten. Burger-local (x, y, z) -> palm (X, Y, Z) = (0.02 + y, z - 0.041, 0.003 + 0.0517 + x).
BITE_R = 0.0517
BITE_CUTS = [                              # per stage: the new circular scallops (x, y, r) in the burger's disc plane (x -> +Z of the palm)
    [(0.058, 0.000, 0.024)],
    [(0.040, 0.004, 0.024), (0.052, -0.030, 0.021)],
    [(0.046, 0.034, 0.023), (0.030, -0.030, 0.024), (0.034, 0.036, 0.020)],
    [(0.018, 0.002, 0.027), (0.020, -0.034, 0.025), (0.022, 0.036, 0.023), (0.040, -0.055, 0.020), (0.040, 0.058, 0.019)],
    [(-0.006, 0.0, 0.029), (-0.002, -0.030, 0.027), (-0.002, 0.030, 0.027), (0.004, -0.058, 0.025), (0.004, 0.058, 0.025),
     (0.014, -0.064, 0.022), (0.014, 0.064, 0.022)],
]
BITE_STAGES = 6
BITE_PLANES = (0.0192, 0.0345, 0.0371, 0.0452)      # where the closed layers touch: their hidden caps are dropped


def _cutter(bm, x, y, r, k):
    n = 20
    pts = [(x + r * (1 + (0.07 if i % 2 else -0.04)) * math.cos(2 * PI * i / n), y + r * (1 + (0.07 if i % 2 else -0.04)) * math.sin(2 * PI * i / n)) for i in range(n)]
    extrude(bm, pts, -0.01, 0.10, 0, 0, 0)


def burger_bite(k):
    if k == 0:
        parts = []
        burger(parts)
    else:
        parts = []
        burger(parts)
        coll = bpy.data.collections.new("cutters%d" % k)
        bpy.context.scene.collection.children.link(coll)
        cuts = [c for st in BITE_CUTS[:k] for c in st]
        for i, (x, y, r) in enumerate(cuts):
            cb = bmesh.new()
            _cutter(cb, x, y, r, k)
            me = bpy.data.meshes.new("cut")
            cb.to_mesh(me)
            cb.free()
            o = bpy.data.objects.new("cut%d" % i, me)
            coll.objects.link(o)
    out = bmesh.new()
    for pb in parts:
        pb.faces.ensure_lookup_table()
        mi = pb.faces[0].material_index
        if k > 0:
            me = bpy.data.meshes.new("part")
            pb.to_mesh(me)
            ob = bpy.data.objects.new("part", me)
            bpy.context.scene.collection.objects.link(ob)
            md = ob.modifiers.new("b", "BOOLEAN")
            md.operation, md.solver, md.operand_type, md.collection = "DIFFERENCE", "EXACT", "COLLECTION", coll
            dg = bpy.context.evaluated_depsgraph_get()
            me2 = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
            pb.free()
            pb = bmesh.new()
            pb.from_mesh(me2)
            bpy.data.objects.remove(ob)
            bpy.data.meshes.remove(me)
            bpy.data.meshes.remove(me2)
        for f in pb.faces:
            f.material_index = mi
        out_map = {}
        for f in pb.faces:
            out_map[f] = f
        # one mesh: copy the part in
        vm = {v: out.verts.new(v.co) for v in pb.verts}
        for f in pb.faces:
            try:
                out.faces.new([vm[v] for v in f.verts]).material_index = mi
            except ValueError:
                pass
        pb.free()
    # drop the hidden contact caps (flat faces on the planes where layers touch)
    out.normal_update()
    dead = [f for f in out.faces if abs(f.normal.z) > 0.99999 and any(abs(f.calc_center_median().z - zz) < 3e-4 for zz in BITE_PLANES)]
    bmesh.ops.delete(out, geom=dead, context="FACES")
    bmesh.ops.remove_doubles(out, verts=out.verts, dist=1e-6)
    for v in out.verts:
        x, y, z = v.co
        v.co = Vector((0.02 + y, z - 0.041, 0.003 + BITE_R + x))
    for c in list(bpy.data.collections):
        if c.name.startswith("cutters"):
            for o in list(c.objects):
                m = o.data
                bpy.data.objects.remove(o)
                bpy.data.meshes.remove(m)
            bpy.data.collections.remove(c)
    return out


def palm_ref():
    bm = bmesh.new()
    C.bx(bm, M("DN_EnamelCream"), -0.06, 0.10, -0.05, 0.05, -0.004, -0.0005)
    return bm


for _k in range(BITE_STAGES):
    BUILD["BurgerBite%d" % _k] = ((lambda k=_k: burger_bite(k)), FOOD_MATS)
    COMPANION["BurgerBite%d" % _k] = [("PalmRef", (0, 0, 0)), ("BurgerBite%d" % _k, (0, 0, 0))]
    PREV_VIEW["BurgerBite%d" % _k] = (0.55, -1.0, 0.9)
BUILD["PalmRef"] = (palm_ref, [])
BITE_NAMES = ["BurgerBite%d" % _k for _k in range(BITE_STAGES)]


def render_preview(name):
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o)
    parts = COMPANION.get(name, [(name, (0, 0, 0))])
    smooth = set(CHINA + FOOD_MATS + (['DN_Steel', 'DN_Chrome'] if name == 'TeaGlass' else []))
    objs = []
    for bn, off in parts:
        bm = BUILD[bn][0]()
        crease(bm)
        me = bpy.data.meshes.new(bn)
        bm.to_mesh(me)
        bm.free()
        for k in C.MATS:
            mat = bpy.data.materials.get(k) or bpy.data.materials.new(k)
            mat.diffuse_color = C._lin(C.MAT_SPEC[k][0]) + (1,)
            me.materials.append(mat)
        for p in me.polygons:
            p.use_smooth = me.materials[p.material_index].name in smooth
        o = bpy.data.objects.new(bn, me)
        o.location = off
        bpy.context.scene.collection.objects.link(o)
        objs.append(o)
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.render.resolution_x = sc.render.resolution_y = 512
    sc.display.shading.light = "STUDIO"
    sc.display.shading.color_type = "MATERIAL"
    sc.display.shading.show_object_outline = True
    sc.display.shading.show_cavity = False
    sc.view_settings.view_transform = "Standard"
    if not sc.world:
        sc.world = bpy.data.worlds.new("W")
    sc.world.color = (0.62, 0.64, 0.66)
    pts = [o.matrix_world @ Vector(c) for o in (objs[-1:] if name.startswith('Load_') else objs) for c in o.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    ctr = (lo + hi) / 2
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    bpy.context.scene.collection.objects.link(cam)
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = (hi - lo).length * (1.5 if name.startswith('Load_') else 0.82)
    cam.data.clip_end = 10
    d = Vector(PREV_VIEW.get(name, (1, -1.15, 0.95))).normalized()
    cam.location = ctr + d * 1.0
    cam.rotation_euler = (ctr - cam.location).to_track_quat("-Z", "Y").to_euler()
    sc.camera = cam
    os.makedirs(PREV, exist_ok=True)
    sc.render.filepath = os.path.join(PREV, name + ".png")
    bpy.ops.render.render(write_still=True)


def contact_sheet(names=None, out_name="tableware_sheet.png", cols=5):
    import numpy as np
    S = 384
    names = [n for n in (names or ORDER) if os.path.isfile(os.path.join(PREV, n + ".png"))]
    rows = (len(names) + cols - 1) // cols
    W, H = cols * S, rows * S
    canvas = np.ones((H, W, 4), dtype=np.float32)
    canvas[:, :, :3] = 0.62
    for k, n in enumerate(names):
        img = bpy.data.images.load(os.path.join(PREV, n + ".png"))
        w, h = img.size
        a = np.empty(w * h * 4, dtype=np.float32)
        img.pixels.foreach_get(a)
        a = a.reshape(h, w, 4)
        ys = (np.arange(S) * h // S)
        xs = (np.arange(S) * w // S)
        a = a[ys][:, xs]
        r, c = k // cols, k % cols
        y0 = H - (r + 1) * S
        canvas[y0:y0 + S, c * S:(c + 1) * S] = a
        bpy.data.images.remove(img)
    out = bpy.data.images.new("sheet", W, H, alpha=False)
    out.pixels.foreach_set(canvas.ravel())
    out.filepath_raw = os.path.join(PREV, out_name)
    out.file_format = "PNG"
    out.save()
    print("DINER sheet", names, flush=True)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if "BurgerBites" in argv:
        C.reset()
        for k in range(BITE_STAGES):
            bm = burger_bite(k)
            crease(bm)
            C.export(bm, BITE_EXPORT % k, smooth=FOOD_MATS)
        for n in BITE_NAMES:
            render_preview(n)
        contact_sheet(BITE_NAMES, "burger_stages.png", 3)
        return
    want = argv or ORDER
    C.reset()
    for name in want:
        build, smooth = BUILD[name]
        bm = build()
        crease(bm)
        C.export(bm, "SM_Diner_" + name, smooth=smooth)
    consts = {
        "PLATE_TOP": PLATE_TOP, "PLATE_R": PLATE_R, "BOWL_IN_Z": BOWL_IN_Z, "BOWL_IN_R": BOWL_IN_R, "BOWL_H": BOWL_H,
        "SOUP_FILL_H": SOUP_FILL_H, "MUG_R": MUG_R, "MUG_HALF_H": MUG_H / 2, "MUG_IN_R": MUG_IN_R, "MUG_IN_Z": MUG_IN_Z,
        "MUG_FILL_H": MUG_FILL_H, "GLASS_R": GLASS_R, "GLASS_H": GLASS_H, "GLASS_IN_R": GLASS_IN_R, "GLASS_FILL_R_BOTTOM": 0.0272,
        "GLASS_FILL_R_TOP": 0.0310, "GLASS_IN_Z": GLASS_IN_Z, "GLASS_FILL_H": GLASS_FILL_H, "TINE_X": TINE_X, "LOAD_Z": LOAD_Z,
        "FORK_LEN": 0.19, "GRIP_FROM_END": 0.06, "BURGER_R": BURGER_R, "BURGER_HALF_H": (BURGER_Z1 - BURGER_Z0) / 2,
        "BURGER_CENTER_Z": (BURGER_Z0 + BURGER_Z1) / 2, "BURGER_Z0": BURGER_Z0,
    }
    os.makedirs(os.path.join(C.ROOT, "design", "diner"), exist_ok=True)
    with open(os.path.join(C.ROOT, "design", "diner", "tableware.json"), "w") as f:
        json.dump(consts, f, indent=2)
    for name in want:
        render_preview(name)
    contact_sheet()


main()
