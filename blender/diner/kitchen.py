"""The diner's kitchen annex fittings and its lamps (docs/diner.md): the range and its hood, the sink, the fridge, the shelves,
the worktable, the mop bucket; the opal globes along the car's crown, the kitchen's lights and the gooseneck lamp over the street
door.

  build(parts) adds the geometry to parts["inside"] (big pieces: collision) and parts["small"] (lamps, pots, cans, utensils: no
  collision) and returns {"lights": [...]} (point lights ALyudDiner makes: at, lumens, colour, kind inside|kitchen|outside).

Everything is modelled with bevelled boxes (rbox), lathe vessels (open pots with a wall thickness), swept tubes (handles, faucet,
gooseneck) and the small helpers below. Nothing touches a wall exactly: fittings keep W = 5 mm off it.
"""
import math
import os
import sys

import bmesh  # noqa: F401
from mathutils import Matrix, Vector  # noqa: F401

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import diner_common as C  # noqa: E402,F401
from diner_common import M, bx, box, rbox, tube, lathe, sphere, ensure_uv, own_uv  # noqa: E402,F401
from plan import *  # noqa: E402,F401,F403
from building_util import oq, ybox  # noqa: E402,F401

W = 0.005                       # the gap between a fitting and the wall
STATS = {}


def tris(bm):
    return sum(len(f.verts) - 2 for f in bm.faces)


# ---- helpers ----------------------------------------------------------------------------------------------------------------

def rb(bm, mi, x0, x1, y0, y1, z0, z1, r=0.006, seg=2, edges="all"):
    seg = 1 if r < 0.01 else seg                       # a small bevel is one chamfer (poly budget)
    return rbox(bm, mi, x0, x1, y0, y1, z0, z1, r=r, seg=seg, edges=edges)


def bar(bm, mi, a, b, w, t):
    """A rectangular bar from a to b, w wide and t thick (its width horizontal when it is not vertical)."""
    a, b = Vector(a), Vector(b)
    d = b - a
    rot = d.to_track_quat("X", "Z").to_matrix()
    box(bm, (a + b) / 2, (d.length, w, t), mi, rot=rot)


def cyl(bm, mi, c, r, h, seg=12, r1=None):
    """A vertical closed cylinder (or cone) from z = c.z to c.z + h."""
    lathe(bm, [(r, 0.0), (r if r1 is None else r1, h)], mi, seg, c)


def sweep(bm, mi, pts, r, seg=8, caps=True):
    """A round tube of radius r along the polyline pts (caps: True, False or "end"): one shared ring at every point and a parallel-transported frame, so bends
    are smooth. UVs wrap round it (u = arc, v = length along)."""
    ensure_uv(bm)
    pts = [Vector(p) for p in pts]
    n = len(pts)
    tans = [(pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]).normalized() for i in range(n)]
    up = Vector((0, 0, 1)) if abs(tans[0].z) < 0.95 else Vector((1, 0, 0))
    side = up.cross(tans[0]).normalized()
    rings = []
    vs = [0.0]
    for i in range(n):
        t = tans[i]
        if i:
            side = side - t * side.dot(t)
            side.normalize()
            vs.append(vs[-1] + (pts[i] - pts[i - 1]).length)
        nrm = t.cross(side)
        rings.append([bm.verts.new(pts[i] + (side * math.cos(2 * math.pi * k / seg) + nrm * math.sin(2 * math.pi * k / seg)) * r)
                      for k in range(seg)])
    ua = 2 * math.pi * r / seg
    for i in range(n - 1):
        for k in range(seg):
            j = (k + 1) % seg
            f = bm.faces.new((rings[i][k], rings[i][j], rings[i + 1][j], rings[i + 1][k]))
            f.material_index = mi
            own_uv(bm, f, ((ua * k, vs[i]), (ua * (k + 1), vs[i]), (ua * (k + 1), vs[i + 1]), (ua * k, vs[i + 1])))
    if caps:                                        # caps: True, False, or "end" (the start is buried in something)
        if caps is True:
            bm.faces.new(rings[0][::-1]).material_index = mi
        bm.faces.new(rings[-1]).material_index = mi


def arc_pts(c, r, a0, a1, n):
    """Points on a circle in the plane x = c.x (centre c, radius r, angles from +y towards +z)."""
    return [(c[0], c[1] + r * math.cos(a0 + (a1 - a0) * i / n), c[2] + r * math.sin(a0 + (a1 - a0) * i / n)) for i in range(n + 1)]


def _inner(outer, t):
    n = len(outer)
    inner = []
    for k, (r, z) in enumerate(outer):
        a, b = outer[max(k - 1, 0)], outer[min(k + 1, n - 1)]
        dr, dz = b[0] - a[0], b[1] - a[1]
        ln = math.hypot(dr, dz)
        inner.append((max(r - dz / ln * t, 0.004), z + dr / ln * t))
    return inner


def vessel(bm, mo, outer, t, seg=16, c=(0, 0, 0), mi=None):
    """A hollow vessel of revolution with wall thickness t: outer = [(r, z), ...] from the bottom to the rim; mi inside."""
    mi = mo if mi is None else mi
    n = len(outer)
    lathe(bm, outer + _inner(outer, t)[::-1], [mo] * n + [mi] * (n - 1), seg, c)


def shell(bm, mo, outer, t, seg=24, c=(0, 0, 0), mi=None):
    """A shade: outer = [(r, z), ...] from the rim to the apex (which keeps a small hole); mo outside, mi inside."""
    mi = mo if mi is None else mi
    n = len(outer)
    inner = _inner(outer, t)
    lathe(bm, [inner[0]] + outer + inner[::-1], [mo] * (n + 1) + [mi] * (n - 1), seg, c, cap0=False, cap1=False)


def grab(bm):
    return set(bm.verts)


def place(bm, before, rot=None, pivot=(0, 0, 0), move=(0, 0, 0)):
    """Turns everything made since grab() about pivot, then moves it."""
    p, mv = Vector(pivot), Vector(move)
    for v in [v for v in bm.verts if v not in before]:
        q = v.co - p
        v.co = p + (rot @ q if rot is not None else q) + mv


def rot_x(a):
    return Matrix.Rotation(a, 3, "X")


def spin_z(a):
    return Matrix.Rotation(a, 3, "Z")


def loft(bm, mi, r0, r1, bottom=False):
    """Outward sides between two rings of 4 points (x, y, z), counter-clockwise seen from above."""
    for i in range(4):
        j = (i + 1) % 4
        d = Vector(r0[j]) - Vector(r0[i])
        oq(bm, [r0[i], r0[j], r1[j], r1[i]], mi, (d.y, -d.x, 0.0))
    if bottom:
        bm.faces.new([bm.verts.new(p) for p in r0[::-1]]).material_index = mi


def slab_holes(bm, mi, x0, x1, y0, y1, z0, z1, holes, zfloor, mi_in):
    """A flat slab with rectangular holes ((hx0, hx1, hy0, hy1), in order along x) whose walls go on down to zfloor and close with a
    floor: a sink top with its basins. Faces are per strip, so no T-junctions."""
    xs = [x0]
    for h in holes:
        xs += [h[0], h[1]]
    xs.append(x1)
    for s in range(len(xs) - 1):
        xa, xb = xs[s], xs[s + 1]
        hole = next((h for h in holes if abs(h[0] - xa) < 1e-9 and abs(h[1] - xb) < 1e-9), None)
        for ya, yb in ([(y0, y1)] if hole is None else [(y0, hole[2]), (hole[3], y1)]):
            oq(bm, [(xa, ya, z1), (xb, ya, z1), (xb, yb, z1), (xa, yb, z1)], mi, (0, 0, 1))
            oq(bm, [(xa, ya, z0), (xb, ya, z0), (xb, yb, z0), (xa, yb, z0)], mi, (0, 0, -1))
        oq(bm, [(xa, y0, z0), (xb, y0, z0), (xb, y0, z1), (xa, y0, z1)], mi, (0, -1, 0))
        oq(bm, [(xa, y1, z0), (xb, y1, z0), (xb, y1, z1), (xa, y1, z1)], mi, (0, 1, 0))
    for xe, d in ((x0, -1), (x1, 1)):
        oq(bm, [(xe, y0, z0), (xe, y1, z0), (xe, y1, z1), (xe, y0, z1)], mi, (d, 0, 0))
    for hx0, hx1, hy0, hy1 in holes:
        oq(bm, [(hx0, hy0, zfloor), (hx1, hy0, zfloor), (hx1, hy0, z1), (hx0, hy0, z1)], mi_in, (0, 1, 0))
        oq(bm, [(hx0, hy1, zfloor), (hx1, hy1, zfloor), (hx1, hy1, z1), (hx0, hy1, z1)], mi_in, (0, -1, 0))
        oq(bm, [(hx0, hy0, zfloor), (hx0, hy1, zfloor), (hx0, hy1, z1), (hx0, hy0, z1)], mi_in, (1, 0, 0))
        oq(bm, [(hx1, hy0, zfloor), (hx1, hy1, zfloor), (hx1, hy1, z1), (hx1, hy0, z1)], mi_in, (-1, 0, 0))
        oq(bm, [(hx0, hy0, zfloor), (hx1, hy0, zfloor), (hx1, hy1, zfloor), (hx0, hy1, zfloor)], mi_in, (0, 0, 1))


# ---- kitchenware (parts["small"]) -----------------------------------------------------------------------------------------------

def stock_pot(bm, c, r, h, lid=True, seg=10):
    x, y, z = c
    vessel(bm, M("DN_Steel"), [(r - 0.012, 0.0), (r, 0.01), (r + 0.004, h - 0.012), (r + 0.006, h)], 0.0035, seg, (x, y, z))
    for s in (-1, 1):                                   # ear handles
        xa, xb = x + s * (r + 0.002), x + s * (r + 0.06)
        rb(bm, M("DN_Bakelite"), min(xa, xb), max(xa, xb), y - 0.012, y + 0.012, z + h * 0.8, z + h * 0.8 + 0.024, 0.007, 2)
    if lid:
        zl = z + h + 0.003
        lathe(bm, [(0.02, zl), (r + 0.012, zl), (r + 0.012, zl + 0.012), (r * 0.6, zl + 0.034), (0.012, zl + 0.04)],
              M("DN_Steel"), seg, (x, y, 0))
        lathe(bm, [(0.016, zl + 0.039), (0.012, zl + 0.05), (0.02, zl + 0.062), (0.012, zl + 0.07)], M("DN_Bakelite"), 8, (x, y, 0))


def saucepan(bm, c, r, h, yaw=0.0, mat="DN_Steel", seg=12, hl=0.2):
    """A pan with a long bakelite handle along +x, turned by yaw."""
    x, y, z = c
    b = grab(bm)
    vessel(bm, M(mat), [(r - 0.01, 0.0), (r, 0.008), (r, h)], 0.003, seg, (0, 0, 0))
    rb(bm, M("DN_Bakelite"), r + 0.03, r + hl, -0.011, 0.011, h - 0.03, h - 0.008, 0.006, 2)
    rb(bm, M(mat), r - 0.006, r + 0.034, -0.012, 0.012, h - 0.034, h - 0.006, 0.004, 1)
    place(bm, b, spin_z(yaw), (0, 0, 0), (x, y, z))


def can(bm, c, r, h, label, seg=8):
    lathe(bm, [(r, 0.0), (r, 0.009), (r, h - 0.009), (r, h)], [M("DN_Steel"), M(label), M("DN_Steel")], seg, c)


def jar(bm, c, r, h, content, lid="DN_Chrome", seg=10):
    x, y, z = c
    lathe(bm, [(r - 0.004, z), (r, z + 0.006), (r, z + h * 0.78), (r * 0.78, z + h * 0.88), (r * 0.78, z + h)], M(content), seg, (x, y, 0))
    lathe(bm, [(r * 0.82, z + h - 0.004), (r * 0.82, z + h + 0.016)], M(lid), seg, (x, y, 0))


def mixing_bowl(bm, c, r, h, mat="DN_Steel", seg=12, flip=False):
    pts = []
    for k in range(4):
        a = 0.4 + (math.pi / 2 - 0.4) * k / 3
        pts.append((r * math.sin(a), h * (math.cos(0.4) - math.cos(a)) / math.cos(0.4)))
    pts.append((r * 1.01, h * 1.0))
    if flip:
        b = grab(bm)
        vessel(bm, M(mat), pts, 0.003, seg, (0, 0, 0))
        place(bm, b, rot_x(math.pi), (0, 0, h / 2), (c[0], c[1], c[2]))
    else:
        vessel(bm, M(mat), pts, 0.003, seg, c)


def plate_stack(bm, c, r, n, seg=12):
    x, y, z = c
    prof = [(r * 0.5, z)]
    for k in range(n):
        zk = z + k * 0.016
        prof += [(r * 0.8, zk + 0.008), (r, zk + 0.016), (r * 0.8, zk + 0.01) if k < n - 1 else (r * 0.8, zk + 0.011)]
    prof.append((r * 0.5, z + n * 0.016 + 0.002))
    lathe(bm, prof, M("DN_China"), seg, (x, y, 0))


def rail_hook(bm, y, z, x):
    """A hook over the rail (axis along x at y, z): the loop passes behind it; returns the point the tool hangs from."""
    R = 0.016
    pts = [(x, y - R, z - 0.03)] + arc_pts((x, y, z), R, math.pi, -0.7, 5)
    sweep(bm, M("DN_Chrome"), pts, 0.0022, 4)
    return (x, y - R, z - 0.03)


def grip(bm, x, y, z):
    bx(bm, M("DN_Bakelite"), x - 0.011, x + 0.011, y - 0.011, y + 0.011, z - 0.1, z, "xXyYz")


def ladle(bm, top):
    x, y, z = top
    grip(bm, x, y, z)
    sweep(bm, M("DN_Steel"), [(x, y, z - 0.1), (x, y, z - 0.26)], 0.0045, 6)
    b = grab(bm)
    vessel(bm, M("DN_Steel"), [(0.012, 0.0), (0.04, 0.02), (0.05, 0.036)], 0.0025, 10, (0, 0, 0))
    place(bm, b, rot_x(math.pi / 2 + 0.1), (0, 0, 0), (x, y - 0.045, z - 0.3))


def spatula(bm, top):
    x, y, z = top
    grip(bm, x, y, z)
    sweep(bm, M("DN_Steel"), [(x, y, z - 0.1), (x, y, z - 0.2)], 0.004, 6)
    bx(bm, M("DN_Steel"), x - 0.04, x + 0.04, y - 0.0025, y + 0.0025, z - 0.31, z - 0.19)


def whisk(bm, top):
    x, y, z = top
    grip(bm, x, y, z)
    sweep(bm, M("DN_Steel"), [(x, y, z - 0.1), (x, y, z - 0.17)], 0.004, 6)
    for k in range(3):
        a = math.pi * k / 3
        pts = [(x + 0.03 * math.sin(math.pi * i / 5) * math.cos(a), y + 0.03 * math.sin(math.pi * i / 5) * math.sin(a),
                z - 0.17 - 0.12 * (1 - math.cos(math.pi * i / 5)) / 2) for i in range(6)]
        sweep(bm, M("DN_Steel"), pts, 0.0016, 4, caps=False)


def skimmer(bm, top):
    x, y, z = top
    grip(bm, x, y, z)
    sweep(bm, M("DN_Steel"), [(x, y, z - 0.1), (x, y, z - 0.26)], 0.004, 6)
    b = grab(bm)
    cyl(bm, M("DN_Steel"), (0, 0, 0), 0.06, 0.004, 16)
    place(bm, b, rot_x(math.pi / 2), (0, 0, 0), (x, y, z - 0.32))


def tongs(bm, top):
    x, y, z = top
    for s in (-1, 1):
        sweep(bm, M("DN_Steel"), [(x + s * 0.004, y, z), (x + s * 0.012, y, z - 0.15), (x + s * 0.03, y, z - 0.29), (x + s * 0.012, y, z - 0.32)],
              0.0035, 5)


def meat_fork(bm, top):
    x, y, z = top
    grip(bm, x, y, z)
    sweep(bm, M("DN_Steel"), [(x, y, z - 0.1), (x, y, z - 0.22)], 0.0045, 6)
    bx(bm, M("DN_Steel"), x - 0.03, x + 0.03, y - 0.004, y + 0.004, z - 0.25, z - 0.22, "xXyYz")
    for i in (-1, 0, 1):
        bx(bm, M("DN_Steel"), x + i * 0.025 - 0.003, x + i * 0.025 + 0.003, y - 0.0035, y + 0.0035, z - 0.37, z - 0.25, "xXyYz")


# ---- the kitchen --------------------------------------------------------------------------------------------------------------

def range_and_hood(big, small, xi1):
    """The 1940s six-burner range with two ovens against the east wall under a stainless hood with baffle filters."""
    ST, EW, CI, BK, CH, DK = M("DN_Steel"), M("DN_EnamelWhite"), M("DN_CastIron"), M("DN_Bakelite"), M("DN_Chrome"), M("DN_Dark")
    xb = xi1 - W                                     # the range's back
    xf = xb - 0.695                                  # its front, -2.9
    y0, y1 = 3.0, 4.5
    rb(big, DK, xf + 0.04, xb - 0.01, y0 + 0.03, y1 - 0.03, 0.0, 0.12, 0.004, 1)                    # plinth
    rb(big, EW, xf, xb, y0, y1, 0.1, 0.88, 0.012, 2)                                                 # the body
    rb(big, ST, xf - 0.012, xb, y0 - 0.005, y1 + 0.005, 0.872, 0.905, 0.007, 2)                      # the top plate
    for ya, yb in ((y0 + 0.04, y0 + 0.74), (y0 + 0.76, y1 - 0.04)):                                  # oven doors
        yc = (ya + yb) / 2
        rb(big, EW, xf - 0.036, xf + 0.005, ya, yb, 0.13, 0.6, 0.014, 2)
        rb(big, DK, xf - 0.052, xf - 0.03, yc - 0.17, yc + 0.17, 0.27, 0.46, 0.008, 2)              # the oven window
        for yy in (yc - 0.28, yc + 0.28):
            tube(big, (xf - 0.036, yy, 0.55), (xf - 0.085, yy, 0.55), 0.0075, CH, 8, caps=False)
        tube(big, (xf - 0.085, yc - 0.285, 0.55), (xf - 0.085, yc + 0.285, 0.55), 0.011, CH, 10)
        rb(big, ST, xf - 0.052, xf - 0.03, ya + 0.04, yb - 0.04, 0.14, 0.2, 0.006, 1)                # a kick strip on each door
    rb(big, ST, xf - 0.036, xf + 0.005, y0 + 0.04, y1 - 0.04, 0.625, 0.86, 0.01, 2)                  # the knob panel
    for i in range(6):
        yy = y0 + 0.15 + i * 0.24
        tube(big, (xf - 0.03, yy, 0.745), (xf - 0.05, yy, 0.745), 0.036, CH, 12)
        tube(big, (xf - 0.05, yy, 0.745), (xf - 0.058, yy, 0.745), 0.019, BK, 8, caps=False)
        tube(big, (xf - 0.058, yy, 0.745), (xf - 0.096, yy, 0.745), 0.027, BK, 10, r1=0.03)
        bx(big, CH, xf - 0.0975, xf - 0.0925, yy - 0.004, yy + 0.004, 0.745, 0.775, "xXyYzZ")         # the pointer
    for cx in (xf + 0.19, xf + 0.5):                                                                   # burners and grates
        for k in range(3):
            cy = y0 + 0.25 + k * 0.5
            lathe(big, [(0.03, 0.9), (0.085, 0.9), (0.09, 0.912), (0.06, 0.928), (0.03, 0.926)], CI, 12, (cx, cy, 0))
            for s in (-1, 1):
                bx(big, CI, cx - 0.14, cx + 0.14, cy + s * 0.14 - 0.007, cy + s * 0.14 + 0.007, 0.949, 0.963, "xXyYZ")
                for t in (-1, 1):
                    bx(big, CI, cx + s * 0.14 - 0.008, cx + s * 0.14 + 0.008, cy + t * 0.14 - 0.008, cy + t * 0.14 + 0.008, 0.9, 0.95, "xXyY")
            bx(big, CI, cx - 0.12, cx + 0.12, cy - 0.006, cy + 0.006, 0.951, 0.961, "yYZ")
            bx(big, CI, cx - 0.006, cx + 0.006, cy - 0.12, cy + 0.12, 0.948, 0.958, "xXZ")
    rb(big, EW, xb - 0.1, xb, y0 + 0.01, y1 - 0.01, 0.9, 1.22, 0.01, 2)                              # backguard with a high shelf
    rb(big, ST, xb - 0.27, xb, y0 - 0.005, y1 + 0.005, 1.22, 1.245, 0.008, 2)
    for yy in (y0 + 0.12, y1 - 0.12):
        bar(big, ST, (xb - 0.2, yy, 1.222), (xb - 0.1, yy, 1.1), 0.02, 0.012)
    zb, zs, zt = 1.86, 1.95, 2.13                                                                      # the hood
    xh = xb - 1.0

    def ring(z, xa, ya0, ya1):
        return [(xa, ya0, z), (xb, ya0, z), (xb, ya1, z), (xa, ya1, z)]
    loft(big, ST, ring(zb, xh, 2.85, 4.65), ring(zs, xh, 2.85, 4.65), bottom=True)
    loft(big, ST, ring(zs, xh, 2.85, 4.65), ring(zt, xb - 0.46, 3.45, 4.05))
    rb(big, ST, xb - 0.46, xb, 3.45, 4.05, zt - 0.02, 2.255, 0.008, 2, "vert")
    rb(big, CH, xh - 0.012, xh + 0.002, 2.845, 4.655, zb, zb + 0.012, 0.004, 1)                       # a bright rail on the lip
    for k in range(24):                                                                                # the baffle filters
        bar(big, ST, (xh + 0.06, 2.92 + k * 0.0712, zb - 0.075), (xb - 0.12, 2.92 + k * 0.0712, zb - 0.075), 0.004, 0.07)
    for xx in (xh + 0.05, xb - 0.1):
        rb(big, ST, xx - 0.01, xx + 0.01, 2.9, 4.6, zb - 0.082, zb - 0.062, 0.003, 1)
    stock_pot(small, (xf + 0.19, y0 + 0.25, 0.928), 0.15, 0.3)                                         # things on the range
    saucepan(small, (xf + 0.5, y0 + 0.75, 0.928), 0.09, 0.09, yaw=math.pi, hl=0.2)
    saucepan(small, (xf + 0.19, y0 + 1.25, 0.928), 0.11, 0.045, yaw=math.pi, mat="DN_CastIron", hl=0.24)
    vessel(small, M("DN_Steel"), [(0.05, 1.245), (0.07, 1.26), (0.075, 1.29), (0.055, 1.33), (0.028, 1.345)], 0.003, 14, (xb - 0.14, y0 + 0.5, 0))
    for k, cn in enumerate(("DN_EnamelRed", "DN_Herb", "DN_Pancake")):
        can(small, (xb - 0.15, y1 - 0.15 - k * 0.115, 1.245), 0.05, 0.11, cn)


def sink_unit(big, small):
    ST, EW, CH, BK, DK = M("DN_Steel"), M("DN_EnamelWhite"), M("DN_Chrome"), M("DN_Bakelite"), M("DN_Dark")
    x0, x1 = -7.1, -4.75
    yb = AY1 - ATH - W                               # the wall less the gap
    yf, zt = 5.42, 0.9                               # the top's front edge and height
    bowls = [(-6.40, -5.95, 5.52, 6.0), (-5.90, -5.45, 5.52, 6.0)]
    slab_holes(big, ST, x0, x1, yf, yb, 0.86, zt, bowls, 0.73, ST)
    for hx0, hx1, hy0, hy1 in bowls:                                                                   # strainers
        lathe(big, [(0.03, 0.728), (0.04, 0.732), (0.04, 0.744), (0.02, 0.75)], CH, 12, ((hx0 + hx1) / 2, (hy0 + hy1) / 2, 0))
    for xa, xb_ in ((x0 + 0.04, -6.5), (-5.35, x1 - 0.04)):                                            # drainboard grooves
        for k in range(6):
            yy = yf + 0.07 + k * 0.095
            bx(big, ST, xa, xb_, yy - 0.007, yy + 0.007, zt - 0.003, zt + 0.0135, "xXyYZ")
    rb(big, ST, x0, x1, yb - 0.075, yb, zt - 0.005, 0.99, 0.01, 2)                                     # splashback
    rb(big, EW, x0 + 0.01, -6.46, 5.455, yb, 0.1, 0.87, 0.008, 2)                                      # the cabinets
    rb(big, EW, -5.39, x1 - 0.01, 5.455, yb, 0.1, 0.87, 0.008, 2)
    rb(big, EW, -6.46, -5.39, 5.47, yb, 0.1, 0.70, 0.008, 2)
    rb(big, EW, -6.46, -5.39, 5.455, 5.48, 0.1, 0.87, 0.008, 2)
    rb(big, DK, x0 + 0.05, x1 - 0.05, 5.5, yb - 0.02, 0.0, 0.12, 0.004, 1)
    for xa, xb_ in ((x0 + 0.03, -6.48), (-6.44, -5.93), (-5.92, -5.41), (-5.37, x1 - 0.03)):
        rb(big, EW, xa, xb_, 5.425, 5.462, 0.15, 0.82, 0.01, 2)
        px = xb_ - 0.05 if xa < -6.0 else xa + 0.05
        sweep(big, CH, [(px, 5.432, 0.58), (px, 5.395, 0.58), (px, 5.395, 0.72), (px, 5.432, 0.72)], 0.008, 8, caps=False)
    fx, fy, fz = -5.925, yb - 0.02, 1.13                                                               # the faucet
    tube(big, (fx - 0.13, fy, fz), (fx + 0.13, fy, fz), 0.015, CH, 12)
    for s in (-1, 1):
        tube(big, (fx + s * 0.1, fy, fz), (fx + s * 0.1, fy, fz + 0.045), 0.011, CH, 10)
        tube(big, (fx + s * 0.1, fy, fz + 0.045), (fx + s * 0.1, fy, fz + 0.056), 0.02, CH, 12)
        tube(big, (fx + s * 0.1 - 0.03, fy, fz + 0.065), (fx + s * 0.1 + 0.03, fy, fz + 0.065), 0.0075, BK, 8)
        sphere(big, (fx + s * 0.1 + 0.032, fy, fz + 0.065), 0.0095, BK, 8, 4)
        sphere(big, (fx + s * 0.1 - 0.032, fy, fz + 0.065), 0.0095, BK, 8, 4)
    tube(big, (fx, fy + 0.012, fz), (fx, yb - 0.003, fz), 0.026, CH, 14)                               # the wall flange
    path = [(fx, fy, fz), (fx, fy - 0.1, fz)]
    for i in range(1, 7):
        a = i * math.pi / 2 / 6
        path.append((fx, fy - 0.1 - 0.07 * math.sin(a), fz - 0.07 * (1 - math.cos(a))))
    path.append((fx, path[-1][1], path[-1][2] - 0.035))
    sweep(big, CH, path, 0.0105, 10)
    tube(big, (fx, path[-1][1], path[-1][2]), (fx, path[-1][1], path[-1][2] - 0.012), 0.014, CH, 10)
    mixing_bowl(small, (x0 + 0.33, 5.78, zt), 0.14, 0.12, flip=True)                                   # on the drainboards
    plate_stack(small, (-5.05, 5.75, zt), 0.11, 6)


def fridge(big, small):
    """A rounded Coolerator: a freezer door over a tall door, chrome lever handles, against the west wall."""
    EW, CH, DK = M("DN_EnamelWhite"), M("DN_Chrome"), M("DN_Dark")
    xw = AX0 + ATH + W
    xf = xw + 0.595
    y0, y1 = 3.76, 4.40
    yc = (y0 + y1) / 2
    rb(big, DK, xw + 0.03, xf - 0.04, y0 + 0.05, y1 - 0.05, 0.0, 0.1, 0.004, 1)
    rb(big, EW, xw, xf, y0, y1, 0.08, 1.66, 0.045, 4, "topvert")
    d0 = xf - 0.005
    rb(big, EW, d0, d0 + 0.045, y0 + 0.012, y1 - 0.012, 0.12, 1.12, 0.026, 3)                      # the main door
    rb(big, EW, d0, d0 + 0.045, y0 + 0.012, y1 - 0.012, 1.15, 1.62, 0.026, 3)                      # the freezer door
    ye = y1 - 0.1
    for za, zb in ((0.72, 1.02), (1.3, 1.46)):                                                      # lever handles on stand-offs
        for z in (za + 0.03, zb - 0.03):
            tube(big, (d0 + 0.04, ye, z), (d0 + 0.075, ye, z), 0.009, CH, 8, caps=False)
        sweep(big, CH, [(d0 + 0.075, ye, za + (zb - za) * i / 6) for i in range(7)], 0.0125, 10)
    rb(big, CH, d0 + 0.04, d0 + 0.06, yc - 0.1, yc + 0.1, 1.53, 1.59, 0.008, 2)                  # the name badge


def worktable(big, small):
    ST, CH = M("DN_Steel"), M("DN_Chrome")
    x0, x1, y0, y1 = -5.2, -3.8, 4.0, 4.7
    rb(big, ST, x0, x1, y0, y1, 0.86, 0.9, 0.012, 2)
    rb(big, ST, x0 + 0.02, x1 - 0.02, y0 + 0.02, y1 - 0.02, 0.8, 0.865, 0.006, 1)
    for xl in (x0 + 0.05, x1 - 0.05):
        for yl in (y0 + 0.05, y1 - 0.05):
            rb(big, ST, xl - 0.02, xl + 0.02, yl - 0.02, yl + 0.02, 0.025, 0.81, 0.005, 1)
            lathe(big, [(0.026, 0.0), (0.03, 0.006), (0.024, 0.02), (0.016, 0.026)], CH, 10, (xl, yl, 0))
    rb(big, ST, x0 + 0.03, x1 - 0.03, y0 + 0.03, y1 - 0.03, 0.2, 0.225, 0.008, 2)                  # the undershelf
    for yl in (y0 + 0.05, y1 - 0.05):
        rb(big, ST, x0 + 0.05, x1 - 0.05, yl - 0.012, yl + 0.012, 0.6, 0.63, 0.004, 1)
    xc = -4.5
    rb(small, M("DN_Toast"), xc - 0.28, xc + 0.28, 4.2, 4.5, 0.9, 0.93, 0.008, 2)                   # the cutting board and its knife
    kn = grab(small)
    rb(small, M("DN_Steel"), -0.12, 0.08, -0.002, 0.002, 0.0, 0.04, 0.0015, 1)
    rb(small, M("DN_Bakelite"), 0.08, 0.2, -0.008, 0.008, 0.0, 0.026, 0.007, 2)
    place(small, kn, spin_z(0.3), (0, 0, 0), (xc - 0.04, 4.35, 0.934))
    mixing_bowl(small, (-4.95, 4.55, 0.9), 0.13, 0.1)
    stock_pot(small, (-3.98, 4.5, 0.9), 0.09, 0.14)
    mixing_bowl(small, (-4.9, 4.35, 0.225), 0.15, 0.1)
    stock_pot(small, (-4.2, 4.35, 0.225), 0.13, 0.24)


def mop_bucket(big, small):
    ST, CH, RU = M("DN_Steel"), M("DN_Chrome"), M("DN_Rubber")
    c = (-4.36, 5.78)
    vessel(big, ST, [(0.12, 0.05), (0.15, 0.06), (0.19, 0.3), (0.2, 0.31)], 0.0025, 18, (c[0], c[1], 0.0))
    lathe(big, [(0.19, 0.285), (0.205, 0.292), (0.205, 0.312), (0.195, 0.318)], ST, 18, (c[0], c[1], 0))
    for a in (0.7, 2.4, 4.0, 5.5):
        tube(big, (c[0] + 0.13 * math.cos(a), c[1] + 0.13 * math.sin(a), 0.0), (c[0] + 0.13 * math.cos(a), c[1] + 0.13 * math.sin(a), 0.052), 0.016, RU, 8)
    sweep(small, CH, [(c[0] - 0.2 * math.cos(t), c[1], 0.31 + 0.2 * math.sin(t)) for t in [i * math.pi / 8 for i in range(9)]], 0.0035, 5)
    ytop = AY1 - ATH - 0.04
    sweep(small, M("DN_Mahogany"), [(c[0] + 0.17, c[1] + 0.15, 0.16), (c[0] + 0.25, ytop, 1.45)], 0.012, 8)
    lathe(small, [(0.03, 0.0), (0.05, 0.05), (0.075, 0.14), (0.02, 0.17)], M("DN_Paper"), 10, (c[0] + 0.17, c[1] + 0.15, 0), cap1=False)


def shelves(big, small, xi0, xi1):
    """Three steel shelves on brackets on the east wall north of the range (the west and back walls have windows), and the
    utensil rail on the north wall."""
    ST, BK = M("DN_Steel"), M("DN_Bakelite")
    xe = xi1 - W
    ya, yb = 4.68, 5.98
    for z in (1.0, 1.4, 1.8):
        rb(big, ST, xe - 0.3, xe, ya, yb, z, z + 0.022, 0.006, 2)
        for yy in (ya + 0.06, yb - 0.06):
            bar(big, ST, (xe - 0.26, yy, z - 0.003), (xe - 0.01, yy, z - 0.18), 0.018, 0.012)
            rb(big, ST, xe - 0.012, xe, yy - 0.025, yy + 0.025, z - 0.2, z - 0.15, 0.003, 1)
    z = 1.022                                        # pots and tins
    stock_pot(small, (xe - 0.15, 4.9, z), 0.125, 0.22)
    stock_pot(small, (xe - 0.15, 5.27, z), 0.11, 0.2)
    for cn in ("DN_EnamelCream",):
        x, y = xe - 0.15, 5.65
        lathe(small, [(0.1, z), (0.105, z + 0.008), (0.105, z + 0.18), (0.1, z + 0.195)], M(cn), 12, (x, y, 0))
        lathe(small, [(0.098, z + 0.195), (0.103, z + 0.202), (0.103, z + 0.215), (0.08, z + 0.225), (0.02, z + 0.227)], M("DN_Steel"), 12, (x, y, 0))
        lathe(small, [(0.018, z + 0.226), (0.014, z + 0.235), (0.02, z + 0.242), (0.01, z + 0.247)], BK, 8, (x, y, 0))
    z = 1.422                                        # pans and jars
    saucepan(small, (xe - 0.1, 4.82, z), 0.08, 0.075, yaw=math.pi, hl=0.14)
    saucepan(small, (xe - 0.1, 4.82, z + 0.075), 0.07, 0.065, yaw=math.pi, hl=0.14)
    saucepan(small, (xe - 0.1, 5.08, z), 0.08, 0.05, yaw=math.pi, mat="DN_CastIron", hl=0.14)
    for k, cn in enumerate(("DN_Jam", "DN_Syrup", "DN_Noodle", "DN_Herb")):
        jar(small, (xe - 0.12, 5.4 + k * 0.1, z), 0.04, 0.14, cn)
    z = 1.822                                        # cans
    labels = ("DN_EnamelRed", "DN_Herb", "DN_Pancake", "DN_Sauce")
    for k in range(5):
        can(small, (xe - 0.08, 4.78 + k * 0.105, z), 0.045, 0.11, labels[k % 4])
        can(small, (xe - 0.19, 4.78 + k * 0.105, z), 0.045, 0.11, labels[(k + 1) % 4])
    for k in range(3):
        can(small, (xe - 0.135, 4.83 + k * 0.105, z + 0.11), 0.045, 0.11, labels[(k + 2) % 4])
    yr = AY1 - ATH - W - 0.026                       # the north wall over the sink: the utensil rail
    zr = 1.95
    x0r, x1r = -6.95, -5.15
    sweep(small, M("DN_Chrome"), [(x0r, yr, zr), (x1r, yr, zr)], 0.008, 10)
    for xx in (x0r, x1r):
        sweep(small, M("DN_Chrome"), [(xx, yr, zr), (xx, AY1 - ATH - W, zr)], 0.008, 8)
    for fn, xx in zip((ladle, spatula, whisk, skimmer, tongs, meat_fork, ladle), (-6.8, -6.55, -6.3, -6.05, -5.8, -5.55, -5.3)):
        fn(small, rail_hook(small, yr, zr, xx))


def kitchen(bm, small=None):
    """Range with its hood, sink, fridge, worktable, shelves and a mop bucket in the annex, clear of both doors' swing zones."""
    small = small if small is not None else bm
    ensure_uv(bm)
    ensure_uv(small)
    xi0, xi1 = AX0 + ATH, AX1 - ATH
    for name, fn in (("range+hood", lambda: range_and_hood(bm, small, xi1)), ("sink", lambda: sink_unit(bm, small)),
                     ("fridge", lambda: fridge(bm, small)), ("worktable", lambda: worktable(bm, small)),
                     ("mop", lambda: mop_bucket(bm, small)), ("shelves", lambda: shelves(bm, small, xi0, xi1))):
        a, b = tris(bm), tris(small)
        fn()
        STATS[name] = (tris(bm) - a, tris(small) - b)


# ---- lamps ---------------------------------------------------------------------------------------------------------------------

GA, GB = 0.125, 0.1475          # the globe's radii: round 12.5 cm, a little long


def pendant(bm, x, top):
    """A 1940s opal globe on a stem: chrome ceiling canopy, a stem with a knuckle, a chrome fitter with a lip, the globe.
    Returns the globe's centre height (the light's)."""
    CH, OP = M("DN_Chrome"), M("DN_Opal")
    zc = top - 0.4
    t0, tn, K = 0.12, math.pi - 0.42, 9
    lathe(bm, [(GA * math.sin(t0 + (tn - t0) * k / K), zc - GB * math.cos(t0 + (tn - t0) * k / K)) for k in range(K + 1)], OP, 24, (x, 0, 0))
    zn = zc + GB * math.cos(0.42)                                # the globe's neck, under the fitter
    fp = [(0.012, -0.012), (0.056, -0.012), (0.059, 0.0), (0.05, 0.022), (0.03, 0.04), (0.02, 0.06), (0.011, 0.085)]
    lathe(bm, [(r, zn + z) for r, z in fp], CH, 12, (x, 0, 0))
    zf, zcan = zn + 0.085, top - 0.04
    sweep(bm, CH, [(x, 0, zf - 0.004), (x, 0, zcan + 0.004)], 0.0065, 6, caps=False)
    zk = (zf + zcan) / 2
    lathe(bm, [(0.004, zk - 0.02), (0.014, zk - 0.012), (0.017, zk), (0.014, zk + 0.012), (0.004, zk + 0.02)], CH, 10, (x, 0, 0))
    lathe(bm, [(0.012, top - 0.04), (0.035, top - 0.04), (0.04, top - 0.03), (0.055, top - 0.017), (0.068, top - 0.004), (0.07, top + 0.006)],
          CH, 12, (x, 0, 0))
    return zc


def kitchen_light(bm, x, y):
    """A white enamel dome shade on a short stem with an opal bulb under it (the light point is 20 cm under the ceiling)."""
    z = A_TOP - 0.12
    EW, CH, OP = M("DN_EnamelWhite"), M("DN_Chrome"), M("DN_Opal")
    lathe(bm, [(0.02, z - 0.012), (0.045, z - 0.012), (0.056, z - 0.004)], CH, 12, (x, y, 0))
    sweep(bm, CH, [(x, y, z - 0.008), (x, y, z - 0.08)], 0.008, 6, caps=False)
    shell(bm, EW, [(0.2, z - 0.19), (0.2, z - 0.17), (0.185, z - 0.13), (0.14, z - 0.09), (0.08, z - 0.066), (0.03, z - 0.058)], 0.0035, 20, (x, y, 0))
    lathe(bm, [(0.02, z - 0.1), (0.032, z - 0.09), (0.032, z - 0.068), (0.016, z - 0.058)], CH, 10, (x, y, 0), cap1=False)
    sphere(bm, (x, y, z - 0.152), 0.055, OP, 12, 6, 1.15)


def gooseneck(bm):
    """The red-enamel barn lamp over the street door: a chrome wall plate, a curved arm, a shade white inside, a bulb."""
    CH, RED, WH, OP = M("DN_Chrome"), M("DN_EnamelRed"), M("DN_EnamelWhite"), M("DN_Opal")
    x, y0, z0 = DOOR_X, YF, 2.32
    rb(bm, CH, x - 0.05, x + 0.05, y0 - 0.026, y0 + 0.015, z0 - 0.075, z0 + 0.075, 0.012, 3)
    tube(bm, (x, y0 - 0.012, z0), (x, y0 - 0.045, z0), 0.026, CH, 14)
    pts = [(x, y0 - 0.03, z0), (x, y0 - 0.17, z0)]
    c1 = (y0 - 0.17, z0 + 0.06)                      # a quarter arc up (r .06), a riser, a half arc over and down (r .08)
    for i in range(1, 7):
        a = -math.pi / 2 - (math.pi / 2) * i / 6
        pts.append((x, c1[0] + 0.06 * math.cos(a), c1[1] + 0.06 * math.sin(a)))
    ys, zr = c1[0] - 0.06, c1[1]
    pts.append((x, ys, zr + 0.05))
    c2 = (ys - 0.08, zr + 0.05)
    for i in range(1, 13):
        a = math.pi * (1 - i / 12)
        pts.append((x, c2[0] + 0.08 * math.cos(a), c2[1] + 0.08 * math.sin(a)))
    pts.append((x, c2[0] - 0.08, c2[1] - 0.05))
    sweep(bm, CH, pts, 0.0105, 6, caps="end")
    ysh, zr_ = YF - 0.42, 2.28
    shell(bm, RED, [(0.185, zr_), (0.185, zr_ + 0.01), (0.17, zr_ + 0.05), (0.13, zr_ + 0.1), (0.08, zr_ + 0.135), (0.04, zr_ + 0.15),
                    (0.03, zr_ + 0.153)], 0.003, 20, (x, ysh, 0), mi=WH)
    lathe(bm, [(0.022, zr_ + 0.152), (0.03, zr_ + 0.156), (0.03, zr_ + 0.176), (0.022, zr_ + 0.18)], CH, 10, (x, ysh, 0))
    sphere(bm, (x, ysh, zr_ + 0.045), 0.05, OP, 12, 6, 1.2)


def lamps(bm):
    """Opal globes on chrome stems along the crown, one in the nose, two in the kitchen, one over the door outside."""
    out = []
    ensure_uv(bm)
    for x in (-6.0, -3.6, -1.2, 1.2, 3.6, 6.0, XE + 1.2):
        top = CROWN_IN - 0.01 if x <= XE else EAVE_IN + (CROWN_IN - EAVE_IN) * math.sqrt(max(0.0, 1 - ((x - XE) / (R - TH)) ** 2)) - 0.01
        a = tris(bm)
        zc = pendant(bm, x, top)
        STATS["pendant"] = (0, tris(bm) - a)
        out.append({"at": [x, 0.0, zc], "lumens": 900, "colour": "#FFE2B8", "kind": "inside"})
    for x, y in ((-5.8, 3.9), (-3.5, 4.9)):
        a = tris(bm)
        kitchen_light(bm, x, y)
        STATS["kitchen_light"] = (0, tris(bm) - a)
        out.append({"at": [x, y, A_TOP - 0.12 - 0.2], "lumens": 1100, "colour": "#F4F1EA", "kind": "kitchen"})
    a = tris(bm)
    gooseneck(bm)
    STATS["gooseneck"] = (0, tris(bm) - a)
    out.append({"at": [DOOR_X, YF - 0.42, 2.25], "lumens": 600, "colour": "#FFD8A0", "kind": "outside"})
    return out


def build(parts):
    kitchen(parts["inside"], parts["small"])
    lights = lamps(parts["inside"])          # drawn as far as the room (lit globes seen from the street)
    for k, v in STATS.items():
        print("KITCHEN %-14s inside %5d small %5d tris" % (k, v[0], v[1]), flush=True)
    return {"lights": lights}
