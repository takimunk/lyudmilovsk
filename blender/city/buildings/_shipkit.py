"""Ship-only pieces of the motor ship «Полдень» (exterior fittings): funnel with the sun disc, masts, lifeboats and
davits, cowl vents, windlass, capstan, bollards, cargo hatch, searchlight, radar, life rings, bell, anchors, awning,
deck stanchions, the name in raised letters. Everything is given in the SHIP frame (_shipframe.py) and goes through
motor_ship.qface, so the mirror of the building frame is taken care of.

fittings(B, S, M): M = the motor_ship module (qface, rail). FITTINGS (filled while building): name -> ship (x, y, z),
the points the furnishers and the game's named ship points use.
"""
import math

FITTINGS = {}

WHITE, HULL, RED, WOOD, BRASS, BLACK, CANVAS = "CT_SteelWhite", "CT_Steel", "CT_SteelRed", "CT_Wood", "CT_Brass", "CT_Lacquer", "CT_Cobogo"

_M = None
_S = None


def _q(m, mat, pts, nrm):
    _M.qface(m, mat, pts, nrm)


def lathe(m, mat, cx, cy, prof, segs=12, mats=None):
    """A body of revolution about the vertical through (cx, cy): prof [(r, z)] bottom to top (r 0 closes an end)."""
    for k, ((r0, z0), (r1, z1)) in enumerate(zip(prof, prof[1:])):
        mt = mats[k] if mats else mat
        for i in range(segs):
            a0, a1 = 2 * math.pi * i / segs, 2 * math.pi * (i + 1) / segs
            c0, s0, c1, s1 = math.cos(a0), math.sin(a0), math.cos(a1), math.sin(a1)
            pts = []
            pts.append((cx + r0 * c0, cy + r0 * s0, z0))
            if r0 > 1e-6:
                pts.append((cx + r0 * c1, cy + r0 * s1, z0))
            if r1 > 1e-6:
                pts.append((cx + r1 * c1, cy + r1 * s1, z1))
            pts.append((cx + r1 * c0, cy + r1 * s0, z1))
            if len(pts) < 3:
                continue
            am = (a0 + a1) / 2
            dz, dr = z1 - z0, r1 - r0
            nrm = (math.cos(am) * dz, math.sin(am) * dz, -dr) if abs(dz) > 1e-9 else (0, 0, 1 if dr < 0 else -1)
            if abs(dz) <= 1e-9:
                nrm = (0, 0, 1) if (k > 0 and prof[k - 1][1] < z0) or k == len(prof) - 2 and k > 0 else (0, 0, -1)
            _q(m, mt, pts, nrm)


def tube(m, mat, path, radii, segs=8, caps=True):
    """A round tube along the 3D path (ship coords) with a radius per point."""
    rings = []
    n = len(path)
    for i in range(n):
        a = path[max(0, i - 1)]
        b = path[min(n - 1, i + 1)]
        t = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
        L = math.sqrt(sum(c * c for c in t)) or 1.0
        t = (t[0] / L, t[1] / L, t[2] / L)
        if i == 0:
            ref = (0, 0, 1) if abs(t[2]) < 0.9 else (1, 0, 0)
            u = (t[1] * ref[2] - t[2] * ref[1], t[2] * ref[0] - t[0] * ref[2], t[0] * ref[1] - t[1] * ref[0])
        else:                           # carry the last ring's frame along (no twist where the path bends)
            dd = sum(u[j] * t[j] for j in range(3))
            u = (u[0] - dd * t[0], u[1] - dd * t[1], u[2] - dd * t[2])
        Lu = math.sqrt(sum(c * c for c in u)) or 1.0
        u = (u[0] / Lu, u[1] / Lu, u[2] / Lu)
        v = (t[1] * u[2] - t[2] * u[1], t[2] * u[0] - t[0] * u[2], t[0] * u[1] - t[1] * u[0])
        r = radii[i] if isinstance(radii, (list, tuple)) else radii
        p = path[i]
        rings.append([(p[0] + r * (math.cos(2 * math.pi * k / segs) * u[0] + math.sin(2 * math.pi * k / segs) * v[0]),
                       p[1] + r * (math.cos(2 * math.pi * k / segs) * u[1] + math.sin(2 * math.pi * k / segs) * v[1]),
                       p[2] + r * (math.cos(2 * math.pi * k / segs) * u[2] + math.sin(2 * math.pi * k / segs) * v[2])) for k in range(segs)])
    for i in range(n - 1):
        for k in range(segs):
            k2 = (k + 1) % segs
            q = [rings[i][k], rings[i][k2], rings[i + 1][k2], rings[i + 1][k]]
            c = [(path[i][j] + path[i + 1][j]) / 2 for j in range(3)]
            mid = [sum(p[j] for p in q) / 4 for j in range(3)]
            nrm = (mid[0] - c[0], mid[1] - c[1], mid[2] - c[2])
            if sum(x * x for x in nrm) < 1e-12:
                continue
            _q(m, mat, q, nrm)
    if caps:
        for i, sgn in ((0, -1), (n - 1, 1)):
            a, b = path[max(0, i - 1)], path[min(n - 1, i + 1)]
            nrm = (sgn * (b[0] - a[0]), sgn * (b[1] - a[1]), sgn * (b[2] - a[2]))
            r = radii[i] if isinstance(radii, (list, tuple)) else radii
            if r > 1e-4:
                _q(m, mat, rings[i], nrm)


def torus(m, x, y, z, R, r, axis, mats, segs=16, sides=6):
    """A ring (life ring) centred at (x, y, z) whose axis is `axis` ("x" or "y"); mats: one material per quarter."""
    for i in range(segs):
        a0, a1 = 2 * math.pi * i / segs, 2 * math.pi * (i + 1) / segs
        mt = mats[(i * len(mats)) // segs]
        for k in range(sides):
            b0, b1 = 2 * math.pi * k / sides, 2 * math.pi * (k + 1) / sides

            def P(a, b):
                rr = R + r * math.cos(b)
                h = r * math.sin(b)
                if axis == "y":
                    return (x + rr * math.cos(a), y + h, z + rr * math.sin(a))
                return (x + h, y + rr * math.cos(a), z + rr * math.sin(a))
            q = [P(a0, b0), P(a1, b0), P(a1, b1), P(a0, b1)]
            am, bm = (a0 + a1) / 2, (b0 + b1) / 2
            if axis == "y":
                nrm = (math.cos(am) * math.cos(bm), math.sin(bm), math.sin(am) * math.cos(bm))
            else:
                nrm = (math.sin(bm), math.cos(am) * math.cos(bm), math.sin(am) * math.cos(bm))
            _q(m, mt, q, nrm)


def sbox(m, mat, x0, x1, y0, y1, z0, z1, faces="all"):
    _S.box(m, mat, x0, x1, y0, y1, z0, z1, faces=faces)


# ------------------------------------------------------------------------------------------------ pieces
def funnel(B):
    S, m, d = _S, B.m, B.detail
    za = S.Z_A
    # the casing top: a low white house the funnel stands on
    sbox(m, WHITE, 20.0, 27.0, -1.5, 1.5, za, za + 1.0, faces="xXyYZ")
    sbox(d, HULL, 19.98, 27.02, -1.52, 1.52, za, za + 0.12, faces="xXyY")
    zb = za + 1.0
    levels = [(zb, 1.0, WHITE), (za + 2.8, 0.985, RED), (za + 4.0, 0.975, WHITE), (za + 5.0, 0.965, BLACK), (za + 5.3, 0.96, None)]
    segs = 28
    cx0, a, b = 23.9, 2.35, 1.25

    def ring_at(z, k, slope=0.0):
        cx = cx0 - 0.14 * (z - zb)
        out = []
        for i in range(segs):
            t = 2 * math.pi * i / segs
            c, s_ = math.cos(t), math.sin(t)
            e = 2.0 / 2.5
            px = a * k * (abs(c) ** e) * (1 if c >= 0 else -1) * (0.92 if c > 0 else 1.06)
            py = b * k * (abs(s_) ** e) * (1 if s_ >= 0 else -1)
            out.append((cx + px, py, z + slope * px))
        return out
    rings = [ring_at(z, k, 0.05 if j == len(levels) - 1 else 0.0) for j, (z, k, _) in enumerate(levels)]
    for j in range(len(levels) - 1):
        cz = (levels[j][0] + levels[j + 1][0]) / 2
        cx = cx0 - 0.14 * (cz - zb)
        for i in range(segs):
            i2 = (i + 1) % segs
            q = [rings[j][i], rings[j][i2], rings[j + 1][i2], rings[j + 1][i]]
            mid = [sum(p[t] for p in q) / 4 for t in range(3)]
            _q(m, levels[j][2], q, (mid[0] - cx, mid[1] * 3.0, 0))
    top = rings[-1]
    cxt = sum(p[0] for p in top) / segs
    czt = sum(p[2] for p in top) / segs
    for i in range(segs):
        _q(d, BLACK, [top[i], top[(i + 1) % segs], (cxt, 0, czt - 0.25)], (0, 0, 1))
    # the sun disc with 12 short rays on both sides of the band
    zc = za + 3.4
    xc = cx0 - 0.14 * (zc - zb) - 0.1
    for sg in (1, -1):
        yy = sg * (b * 0.98 + 0.035)
        disc = [(xc + 0.45 * math.cos(2 * math.pi * i / 20), yy, zc + 0.45 * math.sin(2 * math.pi * i / 20)) for i in range(20)]
        _q(d, BRASS, disc, (0, sg, 0))
        for i in range(12):
            t = 2 * math.pi * i / 12
            c, s_ = math.cos(t), math.sin(t)
            w = 0.045
            q = [(xc + 0.50 * c - w * s_, yy - sg * 0.012, zc + 0.50 * s_ + w * c), (xc + 0.50 * c + w * s_, yy - sg * 0.012, zc + 0.50 * s_ - w * c),
                 (xc + 0.68 * c, yy - sg * 0.02, zc + 0.68 * s_)]
            _q(d, BRASS, q, (0, sg, 0))
    # the typhon horn on the front, a steam pipe aft
    tube(d, BRASS, [(26.05, 0, za + 4.3), (26.45, 0, za + 4.3), (26.75, 0, za + 4.3)], [0.05, 0.07, 0.16], segs=8)
    tube(d, HULL, [(21.75, 0, za + 1.0), (21.3, 0, za + 5.6)], 0.06, segs=6)
    FITTINGS["funnel_top"] = (cxt, 0.0, czt)
    FITTINGS["funnel"] = (23.5, 0.0, za)


def cowl_vent(B, x, y, z, h=2.0, face=0.0, r=0.2):
    """A cowl ventilator: a white trunk with a bent mouth (red inside) looking along ship yaw `face`."""
    d = B.m
    a = math.radians(face)
    fx, fy = math.cos(a), math.sin(a)
    path = [(x, y, z), (x, y, z + h - 0.80), (x + fx * 0.10, y + fy * 0.10, z + h - 0.42), (x + fx * 0.34, y + fy * 0.34, z + h - 0.16),
            (x + fx * 0.60, y + fy * 0.60, z + h - 0.08), (x + fx * 0.78, y + fy * 0.78, z + h - 0.06)]
    tube(d, WHITE, path, [r, r, r * 1.08, r * 1.35, r * 1.9, r * 2.15], segs=12, caps=False)
    mouth = path[-1]
    tube(B.detail, RED, [(mouth[0] - fx * 0.03, mouth[1] - fy * 0.03, mouth[2]), (mouth[0] - fx * 0.04, mouth[1] - fy * 0.04, mouth[2])], [r * 2.05, r * 2.05], segs=12, caps=True)
    lathe(B.detail, WHITE, x, y, [(r + 0.08, z), (r + 0.08, z + 0.06), (r, z + 0.06)], segs=10)


def mast(B, x, y, z0, z1, r0, r1, mat=WHITE, yard=None, name=None):
    lathe(B.m, mat, x, y, [(r0, z0), (r0 * 0.9, z0 + (z1 - z0) * 0.5), (r1, z1), (0.0, z1 + 0.08)], segs=10)
    if yard:
        zy, half = yard
        tube(B.detail, mat, [(x, y - half, zy), (x, y, zy + 0.05), (x, y + half, zy)], [0.03, 0.05, 0.03], segs=6)
    lathe(B.detail, BRASS, x, y, [(0.0, z1 + 0.08), (0.07, z1 + 0.14), (0.0, z1 + 0.22)], segs=8)
    if name:
        FITTINGS[name] = (x, y, z0)


def stay(B, a, b, r=0.012):
    tube(B.detail, HULL, [a, b], r, segs=4, caps=False)


def lifeboat(B, xc, sg, n):
    """A 7.5 m double-ended boat under its davits on deck A, hanging outboard above the rail. sg: +1 starboard."""
    S, m, d = _S, B.m, B.detail
    za = S.Z_A
    yc = sg * 4.70
    zk = za + 1.32                        # keel
    Lb, Bb, Db = 7.5, 2.2, 0.95
    N, K = 10, 6
    rows = []
    for i in range(N + 1):
        t = i / N
        xx = xc - Lb / 2 + Lb * t
        w = math.sin(math.pi * t) ** 0.55 if 0 < t < 1 else 0.0
        sheer = 0.22 * (2 * t - 1) ** 2
        ring = []
        for k in range(-K, K + 1):
            a = math.pi / 2 * k / K
            ring.append((xx, yc + (Bb / 2) * w * math.sin(a), zk + (Db + sheer) * (1 - math.cos(a) ** 1.4 * (0.25 + 0.75 * w)) if w > 0 else zk + Db + sheer - (Db * 0.9) * (1 - abs(k) / K) * 0.0))
        if w == 0.0:
            ring = [(xx, yc, zk + (Db + sheer) * (0.45 + 0.55 * abs(k) / K)) for k in range(-K, K + 1)]
        rows.append(ring)
    for i in range(N):
        for k in range(2 * K):
            q = [rows[i][k], rows[i + 1][k], rows[i + 1][k + 1], rows[i][k + 1]]
            mid = [sum(p[j] for p in q) / 4 for j in range(3)]
            nrm = (0.0, mid[1] - yc, mid[2] - (zk + Db))
            for tri in ((q[0], q[1], q[2]), (q[0], q[2], q[3])):
                e1 = [tri[1][j] - tri[0][j] for j in range(3)]
                e2 = [tri[2][j] - tri[0][j] for j in range(3)]
                cr = (e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0])
                if sum(c * c for c in cr) < 1e-10:
                    continue
                _q(m, WHITE, list(tri), nrm)
    # the canvas cover: a low tent over the gunwale
    for i in range(N):
        for side in (0, -1):
            g0, g1 = rows[i][side], rows[i + 1][side]
            r0 = (rows[i][0][0], yc, max(rows[i][0][2], rows[i][-1][2]) + 0.22 * math.sin(math.pi * i / N) + 0.02)
            r1 = (rows[i + 1][0][0], yc, max(rows[i + 1][0][2], rows[i + 1][-1][2]) + 0.22 * math.sin(math.pi * (i + 1) / N) + 0.02)
            q = [g0, g1, r1, r0]
            pts = [q[0]]
            for p in q[1:]:
                if all(abs(p[j] - pp[j]) > 1e-6 or False for pp in pts for j in range(1)) or all(math.dist(p, pp) > 1e-6 for pp in pts):
                    pts.append(p)
            if len(pts) >= 3:
                _q(m, CANVAS, pts, (0, (g0[1] - yc) or 0.0, 1.0))
    # davits: a curved arm at each end, feet on the deck inboard of the rail; falls down to the boat's ends
    for xe in (xc - Lb / 2 + 0.55, xc + Lb / 2 - 0.55):
        yf = sg * 4.25
        path = [(xe, yf, za), (xe, yf, za + 2.3), (xe, sg * 4.32, za + 3.0), (xe, sg * 4.5, za + 3.45), (xe, yc + sg * 0.05, za + 3.6)]
        tube(m, WHITE, path, [0.10, 0.09, 0.08, 0.07, 0.06], segs=8)
        sbox(d, WHITE, xe - 0.18, xe + 0.18, yf - 0.18, yf + 0.18, za, za + 0.10)
        stay(B, (xe, yc, za + 3.55), (xe, yc, zk + Db + 0.25), 0.015)
        sbox(d, WOOD, xe - 0.06, xe + 0.06, yc - 0.10, yc + 0.10, za + 3.2, za + 3.4)          # the block
    # gripes
    for xe in (xc - 1.6, xc + 1.6):
        stay(B, (xe, sg * 4.25, za + 1.15), (xe, yc - sg * 1.0, zk + 0.75), 0.012)
    sbox(d, BLACK, xc - 0.16, xc + 0.16, yc + sg * (Bb / 2 - 0.02), yc + sg * (Bb / 2 + 0.005), zk + 0.62, zk + 0.82)   # number plate
    FITTINGS["lifeboat_%d" % n] = (xc, sg * 4.25, za + 1.2)


def bollard_pair(B, x, y, z, along=True):
    for o in (-0.3, 0.3):
        px, py = (x + o, y) if along else (x, y + o)
        lathe(B.m, HULL, px, py, [(0.11, z), (0.11, z + 0.42), (0.15, z + 0.44), (0.15, z + 0.50), (0.0, z + 0.52)], segs=10)
    if along:
        sbox(B.m, HULL, x - 0.55, x + 0.55, y - 0.18, y + 0.18, z, z + 0.06)
    else:
        sbox(B.m, HULL, x - 0.18, x + 0.18, y - 0.55, y + 0.55, z, z + 0.06)


def capstan(B, x, y, z):
    lathe(B.m, HULL, x, y, [(0.42, z), (0.42, z + 0.10), (0.24, z + 0.16), (0.19, z + 0.50), (0.26, z + 0.74), (0.30, z + 0.78),
                           (0.30, z + 0.86), (0.0, z + 0.90)], segs=14)
    lathe(B.detail, BRASS, x, y, [(0.0, z + 0.90), (0.10, z + 0.905), (0.0, z + 0.93)], segs=10)
    FITTINGS["capstan"] = (x, y, z + 0.5)


def windlass(B):
    S, m, d = _S, B.m, B.detail
    z = S.foredeck_z(56.25)
    zz = z + 0.02
    sbox(m, HULL, 55.6, 56.9, -1.15, 1.15, zz - 0.3, zz + 0.12)             # bedplate (sunk into the ramp)
    sbox(m, "CT_PlasterGrey", 55.85, 56.65, -0.35, 0.35, zz + 0.12, zz + 0.85)       # gearbox / motor
    tube(m, HULL, [(56.25, -1.15, zz + 0.55), (56.25, 1.15, zz + 0.55)], 0.07, segs=8)
    for sg in (1, -1):
        tube(m, HULL, [(56.25, sg * 0.5, zz + 0.55), (56.25, sg * 0.56, zz + 0.55), (56.25, sg * 0.80, zz + 0.55), (56.25, sg * 0.86, zz + 0.55)],
             [0.36, 0.30, 0.30, 0.36], segs=12)                              # gypsy
        tube(m, HULL, [(56.25, sg * 0.96, zz + 0.55), (56.25, sg * 1.12, zz + 0.55)], [0.20, 0.17], segs=10)     # warping drum
        sbox(m, HULL, 56.1, 56.4, sg * 0.88, sg * 0.95, zz + 0.12, zz + 0.6)
        # the chain forward to the hawse pipe, and the pipe's lip on deck
        tube(d, HULL, [(56.5, sg * 0.68, zz + 0.75), (57.6, sg * 0.62, S.foredeck_z(57.6) + 0.12), (58.5, sg * 0.55, S.foredeck_z(58.5) + 0.06)], 0.035, segs=5)
        lathe(d, HULL, 58.6, sg * 0.55, [(0.17, S.foredeck_z(58.6)), (0.17, S.foredeck_z(58.6) + 0.10), (0.10, S.foredeck_z(58.6) + 0.10)], segs=10)
    sbox(d, BRASS, 55.80, 55.84, -0.12, 0.12, zz + 0.45, zz + 0.65)
    FITTINGS["windlass"] = (55.6, 0.0, z + 0.6)


def anchor(B, sg):
    """A stockless anchor housed in its hawse pipe on the bow."""
    S, d = _S, B.detail
    x, z = 59.2, 2.75
    y = S.hull_half(x, z)
    # the hawse pipe's rim on the side
    for k in range(10):
        a0, a1 = 2 * math.pi * k / 10, 2 * math.pi * (k + 1) / 10
        pts = []
        for (a, r) in ((a0, 0.16), (a0, 0.30), (a1, 0.30), (a1, 0.16)):
            xx, zz = x + r * math.cos(a), z + r * math.sin(a)
            pts.append((xx, sg * (S.hull_half(xx, zz) + 0.03), zz))
        _q(d, HULL, pts, (0.3, sg, 0.1))
    # shank and flukes, lying on the plating below the pipe
    def P(dx, dz, off):
        xx, zz = x + dx, z + dz
        return (xx, sg * (S.hull_half(xx, zz) + off), zz)
    for (a, b, w) in (((-0.02, 0.0), (-0.12, -0.75), 0.07), ((-0.50, -0.78), (0.26, -0.72), 0.09)):
        q = [P(a[0] - w, a[1], 0.05), P(a[0] + w, a[1], 0.05), P(b[0] + w, b[1], 0.05), P(b[0] - w, b[1], 0.05)]
        if abs(a[1] - b[1]) < 0.2:
            q = [P(a[0], a[1] - w, 0.05), P(b[0], b[1] - w, 0.05), P(b[0], b[1] + w, 0.05), P(a[0], a[1] + w, 0.05)]
        _q(d, HULL, q, (0.3, sg, 0.1))
    for dx in (-0.50, 0.26):
        q = [P(dx - 0.09, -0.75, 0.06), P(dx + 0.09, -0.75, 0.06), P(dx + (0.05 if dx > 0 else -0.05), -0.30, 0.06)]
        _q(d, HULL, q, (0.3, sg, 0.1))


def hatch(B):
    S, m, d = _S, B.m, B.detail
    x0, x1, hy = 47.6, 50.0, 1.4
    z0, z1 = S.foredeck_z(x0), S.foredeck_z(x1)
    top = z1 + 0.40          # a low coaming: the hero (step 0.45) can climb onto the cover
    sbox(m, WHITE, x0, x1, -hy, hy, z0 - 0.02, top - 0.08, faces="xXyY")
    # the canvas cover: slightly domed, lashed
    for (ya, yb, za_, zb_) in ((-hy - 0.05, 0.0, top - 0.08, top + 0.06), (0.0, hy + 0.05, top + 0.06, top - 0.08)):
        _q(m, CANVAS, [(x0 - 0.05, ya, za_), (x1 + 0.05, ya, za_), (x1 + 0.05, yb, zb_), (x0 - 0.05, yb, zb_)], (0, 0, 1))
    for xe, sg in ((x0 - 0.05, -1), (x1 + 0.05, 1)):
        _q(m, CANVAS, [(xe, -hy - 0.05, top - 0.08), (xe, hy + 0.05, top - 0.08), (xe, 0, top + 0.06)], (sg, 0, 0))
    for sg in (1, -1):
        _q(m, CANVAS, [(x0 - 0.05, sg * (hy + 0.05), top - 0.22), (x1 + 0.05, sg * (hy + 0.05), top - 0.22), (x1 + 0.05, sg * (hy + 0.05), top - 0.08), (x0 - 0.05, sg * (hy + 0.05), top - 0.08)], (0, sg, 0))
    for xe, sg in ((x0 - 0.05, -1), (x1 + 0.05, 1)):
        _q(m, CANVAS, [(xe, -hy - 0.05, top - 0.22), (xe, hy + 0.05, top - 0.22), (xe, hy + 0.05, top - 0.08), (xe, -hy - 0.05, top - 0.08)], (sg, 0, 0))
    for xx in (48.2, 48.8, 49.4):
        for sg in (1, -1):
            _q(d, WOOD, [(xx - 0.04, 0, top + 0.068), (xx + 0.04, 0, top + 0.068), (xx + 0.04, sg * (hy + 0.06), top - 0.072), (xx - 0.04, sg * (hy + 0.06), top - 0.072)], (0, sg * 0.1, 1))
    FITTINGS["cargo_hatch"] = (48.8, 0.0, top)


def life_ring(B, x, y, z, axis):
    torus(B.detail, x, y, z, 0.29, 0.065, axis, [RED, WHITE, RED, WHITE], segs=16, sides=6)


def searchlight(B, x, y, z):
    d = B.detail
    lathe(d, WHITE, x, y, [(0.16, z), (0.16, z + 0.05), (0.05, z + 0.08), (0.05, z + 0.45), (0.0, z + 0.45)], segs=8)
    tube(d, WHITE, [(x - 0.20, y, z + 0.62), (x + 0.16, y, z + 0.62)], [0.17, 0.21], segs=12)
    tube(d, "CT_Opal", [(x + 0.161, y, z + 0.62), (x + 0.17, y, z + 0.62)], [0.19, 0.19], segs=12)
    FITTINGS["searchlight"] = (x, y, z + 0.6)


def radar_mast(B, x, y, z):
    d = B.detail
    h = 1.9
    for (dx, dy) in ((-0.22, -0.22), (0.22, -0.22), (0.22, 0.22), (-0.22, 0.22)):
        tube(d, WHITE, [(x + dx, y + dy, z), (x + dx * 0.45, y + dy * 0.45, z + h)], 0.025, segs=4, caps=False)
    for k in range(4):
        t0, t1 = k / 4, (k + 1) / 4
        w0, w1 = 0.22 * (1 - 0.55 * t0), 0.22 * (1 - 0.55 * t1)
        c = [(-1, -1), (1, -1), (1, 1), (-1, 1)]
        for i in range(4):
            a, b = c[i], c[(i + 1) % 4]
            tube(d, WHITE, [(x + a[0] * w0, y + a[1] * w0, z + h * t0), (x + b[0] * w1, y + b[1] * w1, z + h * t1)], 0.012, segs=3, caps=False)
    sbox(d, WHITE, x - 0.2, x + 0.2, y - 0.2, y + 0.2, z + h, z + h + 0.18)
    sbox(d, WHITE, x - 0.08, x + 0.08, y - 1.1, y + 1.1, z + h + 0.24, z + h + 0.40)       # the open-array scanner
    sbox(d, HULL, x + 0.08, x + 0.085, y - 1.05, y + 1.05, z + h + 0.27, z + h + 0.37, faces="X")
    FITTINGS["radar_scanner"] = (x, y, z + h + 0.3)


def bell(B, x, y, z, side=1):
    """The ship's bell on a bracket off the mast's side (side +1: to starboard)."""
    d = B.detail
    sbox(d, WHITE, x - 0.02, x + 0.02, y, y + side * 0.50, z + 0.36, z + 0.40)            # bracket
    yb = y + side * 0.46
    lathe(d, BRASS, x, yb, [(0.0, z + 0.36), (0.05, z + 0.34), (0.10, z + 0.24), (0.12, z + 0.10), (0.165, z + 0.0), (0.15, z + 0.0), (0.0, z + 0.05)], segs=12)
    tube(d, WHITE, [(x, yb, z - 0.02), (x, yb, z - 0.30)], 0.008, segs=3, caps=False)   # the lanyard
    FITTINGS["ship_bell"] = (x, yb, z + 0.15)


# ---- the name in raised block letters -------------------------------------------------------------
# each glyph: bars (u0, v0, u1, v1) on a 0..1 x 0..1 cell (u to the right, v up)
_T = 0.2
GLYPHS = {
    "П": [(0, 0, _T, 1), (1 - _T, 0, 1, 1), (0, 1 - _T, 1, 1)],
    "О": [(0, 0, _T, 1), (1 - _T, 0, 1, 1), (0, 1 - _T, 1, 1), (0, 0, 1, _T)],
    "Л": [(0.15, 0, 0.15 + _T, 1), (1 - _T, 0, 1, 1), (0.15, 1 - _T, 1, 1), (0, 0, 0.25, _T)],
    "Д": [(0.15, 0.2, 0.15 + _T, 1), (0.85 - _T, 0.2, 0.85, 1), (0.15, 1 - _T, 0.85, 1), (0, 0.2, 1, 0.2 + _T), (0, 0, _T * 0.8, 0.25), (1 - _T * 0.8, 0, 1, 0.25)],
    "Е": [(0, 0, _T, 1), (0, 1 - _T, 1, 1), (0, 0, 1, _T), (0, 0.5 - _T / 2, 0.8, 0.5 + _T / 2)],
    "Н": [(0, 0, _T, 1), (1 - _T, 0, 1, 1), (0, 0.5 - _T / 2, 1, 0.5 + _T / 2)],
    "Ь": [(0, 0, _T, 1), (0, 0, 1, _T), (0, 0.55 - _T, 1, 0.55), (1 - _T, 0, 1, 0.55)],
}


def lettering(B, word, x_mid, z, sg, h=0.42, y_of=None, mat=BLACK, w=0.30, gap=0.12, d_out=0.02, face_x=None):
    """Raised block letters on the ship's side sg at height z; the word reads left to right for someone looking at
    that side from outside (on the starboard side the bow is on the viewer's right, so it runs toward the bow)."""
    S, d = _S, B.detail
    n = len(word)
    total = n * w + (n - 1) * gap
    for i, ch in enumerate(word):
        s0 = -total / 2 + i * (w + gap)
        for (u0, v0, u1, v1) in GLYPHS[ch]:
            xa = x_mid + sg * (s0 + u0 * w)
            xb = x_mid + sg * (s0 + u1 * w)
            za, zb = z + v0 * h, z + v1 * h
            ya = y_of(xa, (za + zb) / 2) + d_out
            yb = y_of(xb, (za + zb) / 2) + d_out
            _q(d, mat, [(xa, sg * ya, za), (xb, sg * yb, za), (xb, sg * yb, zb), (xa, sg * ya, zb)], (0, sg, 0))


def stanchion(B, x, y, z0, z1, r=0.03):
    sbox(B.detail, WHITE, x - r, x + r, y - r, y + r, z0, z1, faces="xXyY")


def fittings(B, S, M):
    global _M, _S
    _M, _S = M, S
    FITTINGS.clear()
    m, d = B.m, B.detail
    za, zb, zc = S.Z_A, S.Z_B, S.Z_C
    funnel(B)
    for x in (20.6, 26.6):
        for sg in (1, -1):
            cowl_vent(B, x, sg * 2.15, za, h=2.1, face=0.0, r=0.17)
    for sg in (1, -1):
        cowl_vent(B, 51.3, sg * 1.7, S.foredeck_z(51.3), h=1.9, face=0.0, r=0.16)
    # ---- masts
    zf = S.foredeck_z(50.6)
    mast(B, 50.6, 0.0, zf - 0.05, 18.0, 0.22, 0.07, yard=(14.6, 1.6), name="foremast")
    mast(B, 17.5, 0.0, za, 17.5, 0.175, 0.06, yard=(15.0, 1.3), name="mainmast")
    # derrick boom stowed aft over the hatch, its crutch, the cargo winch
    tube(d, WHITE, [(50.45, 0.0, zf + 1.3), (44.6, 0.0, S.Z_B + 1.75)], [0.09, 0.06], segs=8)
    sbox(m, HULL, 51.0, 51.8, -0.45, 0.45, S.foredeck_z(51.4) - 0.1, S.foredeck_z(51.4) + 0.55)
    tube(d, HULL, [(51.4, -0.6, S.foredeck_z(51.4) + 0.4), (51.4, 0.6, S.foredeck_z(51.4) + 0.4)], 0.2, segs=10)
    # stays and shrouds
    for sg in (1, -1):
        stay(B, (50.6, sg * 0.05, 14.4), (50.0, sg * (S.half(50.0) - 0.08), S.foredeck_z(50.0) + 1.12))
        stay(B, (50.6, sg * 0.05, 14.4), (51.6, sg * (S.half(51.6) - 0.08), S.foredeck_z(51.6) + 1.12))
        stay(B, (17.5, sg * 0.05, 14.8), (16.6, sg * 4.9, za + 0.05))
        stay(B, (17.5, sg * 0.05, 14.8), (18.4, sg * 4.9, za + 0.05))
    stay(B, (50.6, 0.0, 17.6), (61.4, 0.0, S.foredeck_z(61.4) + 1.2))
    stay(B, (50.6, 0.0, 17.8), (37.5, 0.0, 18.3), 0.008)             # the aerial, foremast - signal mast - mainmast
    stay(B, (37.5, 0.0, 18.3), (17.5, 0.0, 17.3), 0.008)
    # loudspeaker horns on the mainmast, anchor light
    for sg in (1, -1):
        tube(d, "CT_Aluminium", [(17.5, sg * 0.12, za + 3.4), (17.5, sg * 0.45, za + 3.3)], [0.05, 0.17], segs=8)
    bell(B, 50.6, 0.18, zf + 1.45, side=1)
    # ---- boats (odd numbers starboard): aft pair x 11..18.5, fwd pair x 21..28.5
    n = 1
    for xc in (24.75, 14.75):
        for sg in (1, -1):
            lifeboat(B, xc, sg, n)
            n += 1
    # raft capsules on cradles, 2 a side at x 30..33
    for sg in (1, -1):
        for xc in (30.6, 32.6):
            tube(m, WHITE, [(xc - 0.7, sg * 4.35, za + 0.55), (xc - 0.6, sg * 4.35, za + 0.55), (xc + 0.6, sg * 4.35, za + 0.55), (xc + 0.7, sg * 4.35, za + 0.55)],
                 [0.2, 0.33, 0.33, 0.2], segs=10)
            for xx in (xc - 0.4, xc + 0.4):
                sbox(d, HULL, xx - 0.04, xx + 0.04, sg * 4.35 - 0.3, sg * 4.35 + 0.3, za, za + 0.3)
    # ---- foredeck and aft deck gear
    windlass(B)
    hatch(B)
    for sg in (1, -1):
        anchor(B, sg)
        bollard_pair(B, 53.0, sg * (S.half(53.0) - 0.75), S.foredeck_z(53.0))
        bollard_pair(B, 58.0, sg * (S.half(58.0) - 0.62), S.foredeck_z(58.0))
        bollard_pair(B, 2.2, sg * 2.55, zc)
        bollard_pair(B, 6.0, sg * 3.75, zc)
    capstan(B, 3.0, 0.0, zc)
    # jackstaff and ensign staff
    tube(d, WHITE, [(61.45, 0.0, S.foredeck_z(61.45) + 1.1), (61.45, 0.0, S.foredeck_z(61.45) + 3.3)], [0.03, 0.018], segs=6)
    tube(d, WHITE, [(2.25, 0.0, zb + 1.05), (1.75, 0.0, zb + 3.6)], [0.035, 0.02], segs=6)
    # ---- wheelhouse top: searchlight, radar mast, standard compass, signal mast with a yard
    zt = S.Z_WHEELHOUSE_TOP
    searchlight(B, 42.2, 0.0, zt)
    radar_mast(B, 40.2, 0.0, zt)
    lathe(d, BRASS, 41.3, 0.0, [(0.16, zt), (0.13, zt + 0.9), (0.2, zt + 1.0), (0.2, zt + 1.12), (0.0, zt + 1.25)], segs=10)
    mast(B, 37.5, 0.0, zt, 18.4, 0.09, 0.04, yard=(16.8, 1.5), name="signal_mast")
    for sg in (1, -1):
        stay(B, (37.5, sg * 1.45, 16.8), (37.0, sg * 2.9, zt + 0.02), 0.006)
        # sidelight boxes on the wing ends (green starboard, red port) and the name boards on the wing fronts
        sbox(d, BLACK, 40.9, 41.35, sg * 4.96, sg * 5.14, S.Z_BRIDGE + 1.12, S.Z_BRIDGE + 1.42)
        sbox(d, "CT_PlasterGreen" if sg > 0 else RED, 41.0, 41.3, sg * 5.14, sg * 5.17, S.Z_BRIDGE + 1.18, S.Z_BRIDGE + 1.36)
        sbox(d, WOOD, 41.47, 41.50, sg * 3.3, sg * 4.8, S.Z_BRIDGE + 0.45, S.Z_BRIDGE + 0.85)
    # ---- life rings: 10 on the promenade rails, 4 on the boat deck (inboard face of the rail)
    rings = []
    for sg in (1, -1):
        for x in (14.0, 20.0, 26.0, 34.5, 40.0):
            y = sg * (S.deck_half("B", x) - 0.06 - 0.11)
            life_ring(B, x, y, zb + 0.62, "y")
            rings.append((x, y, zb + 0.62))
        for x in (9.0, 31.5):
            y = sg * (S.deck_half("A", x) - 0.06 - 0.11)
            if x == 9.0 and sg < 0:
                y = sg * (S.deck_half("A", x) - 0.17)
            life_ring(B, x, y, za + 0.62, "y")
            rings.append((x, y, za + 0.62))
    FITTINGS["life_rings"] = rings
    # ---- stanchions: under the boat deck along the promenades, under the promenade deck at the stern, the awning
    for sg in (1, -1):
        for x in [12.0 + 3.05 * k for k in range(11)]:
            stanchion(B, x, sg * (S.deck_half("B", x) - 0.06), zb + S.RAIL_H, za - S.DECK_T)
        for x in (6.2, 9.0):
            stanchion(B, x, sg * (S.deck_half("B", x) - 0.06), zb + S.RAIL_H, za - S.DECK_T)
        for x in (2.3, 4.2, 6.2):
            stanchion(B, x, sg * (S.half(x) - 0.06), zc + S.BULWARK_H + 0.05, zb - S.DECK_T, r=0.035)
        # the awning frame over the open end of the smoking deck x 2..6
        for x in (2.1, 4.05):
            stanchion(B, x, sg * (S.deck_half("B", x) - 0.06), zb + S.RAIL_H, zb + 2.38, r=0.022)
    hb = S.deck_half("B", 2.1) - 0.06
    for x in (2.1, 4.05, 6.0):
        h = S.deck_half("B", x) - 0.06
        tube(d, WHITE, [(x, -h, zb + 2.38), (x, 0.0, zb + 2.48), (x, h, zb + 2.38)], 0.022, segs=4, caps=False)
    for sg in (1, -1):
        xs = [2.1, 4.05, 6.0]
        for xa, xb in zip(xs, xs[1:]):
            ha, hbb = S.deck_half("B", xa) - 0.03, S.deck_half("B", xb) - 0.03
            _q(d, CANVAS, [(xa, 0.0, zb + 2.51), (xb, 0.0, zb + 2.51), (xb, sg * hbb, zb + 2.41), (xa, sg * ha, zb + 2.41)], (0, 0, 1))
            _q(d, CANVAS, [(xa, 0.0, zb + 2.505), (xb, 0.0, zb + 2.505), (xb, sg * hbb, zb + 2.405), (xa, sg * ha, zb + 2.405)], (0, 0, -1))
    # ---- the name on both bows (raised letters on the white bulwark)
    for sg in (1, -1):
        lettering(B, "ПОЛДЕНЬ", 54.2, S.foredeck_z(54.2) + 0.38, sg, y_of=lambda x, z: S.half(x))
    FITTINGS["bow"] = (60.6, 0.0, S.foredeck_z(60.6))
    FITTINGS["stern_rail"] = (2.3, 0.0, zb)
    FITTINGS["gangway"] = (31.1, 4.4, zc)
    B.note("fittings (ship frame): " + "; ".join("%s %s" % (k, tuple(round(c, 2) for c in v)) for k, v in sorted(FITTINGS.items()) if k != "life_rings"))
    B.note("life rings (ship frame): " + "; ".join("(%.2f, %.2f, %.2f)" % r for r in rings))


# ================================================================================================ the pier's landing stage
# (building type hyperboloid_tower, plot P-PIER: called from its build()). Everything here is in THAT building's frame.
PIER_PLOT_YAW = -4.7        # the yaw of plot P-PIER (tools/city_plan.py seaward_yaw(905)); the pier itself runs along map +X
PIER_HEAD_HALF = 13.0       # tools/city_mesh.py PIER_HEAD 26 m: the head's square deck is centred on the building's origin
LANDING = {}                # name -> building-frame (x, y, z): filled by pier_landing (things, the stage's corners)


def pier_landing(B):
    """The launch's landing stage at the seaward edge of the pier head: a balcony and a flight of 14 steps down along the
    edge to a railed platform 2.3 m under the deck (1.2 m over the high water), on piles; on the deck the ship's ticket
    box and the timetable. Things: ship_ticket_box (machine, what ship_tickets), ship_timetable (board, topic ship),
    launch_gate (door, to P-SHIP/gangway). Rails, stringers and the gate are in the shell (collision).
    Returns the stage's outline points (building frame) for the footprint."""
    import geom
    from geom import Mesh
    a = math.radians(PIER_PLOT_YAW)
    sv = (math.sin(a), -math.cos(a))                 # seaward (map +X) in the building frame
    eu = (-sv[1] * -1.0, sv[0] * -1.0)               # along the edge, so that (eu, sv, z) is right-handed
    eu = (sv[1], -sv[0])
    E0 = (sv[0] * PIER_HEAD_HALF, sv[1] * PIER_HEAD_HALF)

    def P(u, v, z=0.0):
        return (E0[0] + u * eu[0] + v * sv[0], E0[1] + u * eu[1] + v * sv[1], z)

    def kyaw(du, dv):
        dx, dy = du * eu[0] + dv * sv[0], du * eu[1] + dv * sv[1]
        return round(math.degrees(math.atan2(dx, -dy)), 1)

    def lbox(m, mat, u0, u1, v0, v1, z0, z1, faces="all"):
        t = Mesh()
        geom.box(t, mat, min(u0, u1), max(u0, u1), min(v0, v1), max(v0, v1), min(z0, z1), max(z0, z1), faces=faces)
        t.verts = [P(*p) for p in t.verts]
        t.faces = [(mt, i, geom.auto_uv([t.verts[k] for k in i])) for mt, i, _ in t.faces]
        m.extend(t)

    def lrail(u0, v0, u1, v1, z, h=1.05, zb=None):
        """A three-bar rail from (u0, v0) to (u1, v1) on z (zb: the far end's z)."""
        zb = z if zb is None else zb
        L = math.hypot(u1 - u0, v1 - v0)
        du, dv = (u1 - u0) / L, (v1 - v0) / L
        nu, nv = -dv, du
        for (b0, b1, w, mat) in ((h - 0.06, h, 0.035, WOOD), (h * 0.66 - 0.015, h * 0.66 + 0.015, 0.014, WHITE), (h * 0.33 - 0.015, h * 0.33 + 0.015, 0.014, WHITE)):
            c = [(u0 + nu * w, v0 + nv * w), (u1 + nu * w, v1 + nv * w), (u1 - nu * w, v1 - nv * w), (u0 - nu * w, v0 - nv * w)]
            for (i, j, flip) in ((0, 1, False), (3, 2, True)):
                q = [P(c[i][0], c[i][1], z + b0), P(c[j][0], c[j][1], zb + b0), P(c[j][0], c[j][1], zb + b1), P(c[i][0], c[i][1], z + b1)]
                B.m.face(mat, q[::-1] if flip else q)
            B.m.face(mat, [P(c[0][0], c[0][1], z + b1), P(c[3][0], c[3][1], z + b1), P(c[2][0], c[2][1], zb + b1), P(c[1][0], c[1][1], zb + b1)][::-1])
        k = max(1, int(round(L / 1.3)))
        for i in range(k + 1):
            uu, vv, zz = u0 + (u1 - u0) * i / k, v0 + (v1 - v0) * i / k, z + (zb - z) * i / k
            lbox(B.m, WHITE, uu - 0.025, uu + 0.025, vv - 0.025, vv + 0.025, zz, zz + h - 0.06, faces="xXyY")

    m, d = B.m, B.detail
    ZP = -2.30                                   # the platform
    N = 14
    r = -ZP / N
    UT, G = 2.5, 0.30                            # the top of the flight, the going
    UF = UT - G * (N - 1)                        # the foot: -1.4
    V0, V1, V2 = 0.06, 1.36, 2.70
    CONC, DARK = "CT_Concrete", "CT_ConcreteDark"
    # the balcony at deck level, outside the edge
    lbox(m, CONC, UT, UT + 1.5, 0.0, V1, -0.25, 0.0)
    # the flight (down toward -u), each step a slab on a sloping beam
    for k in range(1, N):
        ua, ub = UT - G * (k - 1), UT - G * k
        zt = -r * k
        lbox(m, CONC, ub, ua, V0, V1, zt - 0.20, zt)
    # the platform
    lbox(m, CONC, UF - 4.4, UF, V0, V2, ZP - 0.25, ZP)
    lbox(d, WOOD, UF - 4.4, UF, V2, V2 + 0.12, ZP - 0.30, ZP - 0.02)          # the fender beam along the berth
    # stringers (solid panels) both sides of the flight; the inner one closes the gap under the pier's slab
    for v in (V0 - 0.03, V1 + 0.03):
        poly = [(UT, -0.35), (UF, ZP - 0.35), (UF, ZP + 1.0), (UT, 1.0)]
        for sgn, vv in ((-1, v - 0.025), (1, v + 0.025)):
            q = [P(pu, vv, pz) for pu, pz in poly]
            m.face(WHITE, q if sgn < 0 else q[::-1])
        for i in range(4):
            (au, az), (bu, bz) = poly[i], poly[(i + 1) % 4]
            m.face(WHITE, [P(au, v - 0.025, az), P(au, v + 0.025, az), P(bu, v + 0.025, bz), P(bu, v - 0.025, bz)])
        d.face(WOOD, [P(UT, v - 0.04, 1.004), P(UF, v - 0.04, ZP + 1.004), P(UF, v + 0.04, ZP + 1.004), P(UT, v + 0.04, 1.004)][::-1])
    # rails: balcony, platform (a gap for the launch gate in the seaward rail)
    lrail(UT + 0.02, V1 + 0.03, UT + 1.47, V1 + 0.03, 0.0)
    lrail(UT + 1.47, V1 + 0.03, UT + 1.47, 0.03, 0.0)
    GA, GB = UF - 2.9, UF - 1.7                  # the gate
    lrail(UF - 0.03, V1 + 0.06, UF - 0.03, V2 - 0.03, ZP)
    lrail(UF - 0.03, V2 - 0.03, GB, V2 - 0.03, ZP)
    lrail(GA, V2 - 0.03, UF - 4.37, V2 - 0.03, ZP)
    lrail(UF - 4.37, V2 - 0.03, UF - 4.37, V0 + 0.03, ZP)
    lrail(UF - 4.37, V0 + 0.03, UF - 0.2, V0 + 0.03, ZP)
    # the launch gate: a solid white leaf with a red band between two posts, a brass latch
    lbox(m, WHITE, GA, GB, V2 - 0.05, V2 - 0.01, ZP + 0.08, ZP + 1.05)
    lbox(d, RED, GA + 0.04, GB - 0.04, V2 - 0.06, V2 - 0.05, ZP + 0.55, ZP + 0.75)
    for uu in (GA, GB):
        lbox(m, WHITE, uu - 0.05, uu + 0.05, V2 - 0.08, V2 + 0.02, ZP, ZP + 1.25)
    lbox(d, BRASS, GB - 0.16, GB - 0.08, V2 - 0.075, V2 - 0.06, ZP + 0.95, ZP + 1.0)
    # piles and bracing; two mooring bollards; a life ring on the inner rail
    for (uu, vv) in ((UF - 0.4, 0.5), (UF - 0.4, V2 - 0.4), (UF - 4.0, 0.5), (UF - 4.0, V2 - 0.4), (UT + 0.75, 0.8), ((UT + UF) / 2, 0.8)):
        ztop = ZP - 0.25 if uu < UF else (-0.25 if uu > UT else -r * (N / 2) - 0.2)
        lbox(m, DARK, uu - 0.17, uu + 0.17, vv - 0.17, vv + 0.17, -7.5, ztop, faces="xXyY")
    for uu in (UF - 0.6, UF - 3.8):
        t = Mesh()
        c = P(uu, V2 - 0.32, ZP)
        global _M, _S
        for (r0, z0, r1, z1) in ((0.10, 0.0, 0.10, 0.34), (0.10, 0.34, 0.14, 0.37), (0.14, 0.37, 0.14, 0.43)):
            for i in range(10):
                a0, a1 = 2 * math.pi * i / 10, 2 * math.pi * (i + 1) / 10
                m.face(HULL, [(c[0] + r0 * math.cos(a0), c[1] + r0 * math.sin(a0), ZP + z0), (c[0] + r0 * math.cos(a1), c[1] + r0 * math.sin(a1), ZP + z0),
                              (c[0] + r1 * math.cos(a1), c[1] + r1 * math.sin(a1), ZP + z1), (c[0] + r1 * math.cos(a0), c[1] + r1 * math.sin(a0), ZP + z1)])
        m.face(HULL, [(c[0] + 0.14 * math.cos(2 * math.pi * i / 10), c[1] + 0.14 * math.sin(2 * math.pi * i / 10), ZP + 0.43) for i in range(10)])
    # ---- on the deck: the ticket box (a small white booth with a window and a red roof board) and the timetable
    bu0, bu1, bv0, bv1 = 4.9, 6.2, -1.7, -0.4
    lbox(m, WHITE, bu0, bu1, bv0, bv1, 0.0, 2.25, faces="xXyY")
    lbox(m, RED, bu0 - 0.12, bu1 + 0.12, bv0 - 0.25, bv1 + 0.12, 2.25, 2.33)
    lbox(d, "CT_Glass", bu0 + 0.25, bu1 - 0.25, bv0 - 0.012, bv0, 1.05, 1.75, faces="y")
    lbox(d, WOOD, bu0 + 0.15, bu1 - 0.15, bv0 - 0.22, bv0, 0.98, 1.03)                 # the till shelf
    lbox(d, "CT_Frame", bu0 + 0.2, bu1 - 0.2, bv0 - 0.02, bv0 - 0.01, 1.0, 1.05)
    lbox(d, "CT_Frame", bu0 + 0.2, bu1 - 0.2, bv0 - 0.02, bv0 - 0.01, 1.75, 1.80)
    lbox(d, HULL, bu0 - 0.01, bu1 + 0.01, bv0 - 0.01, bv1 + 0.01, 0.0, 0.12, faces="xXyY")
    # the sun disc of the ship's funnel over the window
    cu, cz = (bu0 + bu1) / 2, 2.02
    d.face(BRASS, [P(cu + 0.14 * math.cos(2 * math.pi * i / 14), bv0 - 0.015, cz + 0.14 * math.sin(2 * math.pi * i / 14)) for i in range(14)][::-1])
    B.thing("ship_ticket_box", "machine", P(cu, bv0 - 0.05, 1.3), kyaw(0, -1), what="ship_tickets")
    B.spot("ship_ticket_queue", "stand", P(cu, bv0 - 0.9, 0.0), kyaw(0, 1))
    # the timetable: a board on two posts, its back to the sea
    tu0, tu1, tv = 2.9, 4.3, -0.55
    for uu in (tu0 + 0.05, tu1 - 0.05):
        lbox(m, WHITE, uu - 0.04, uu + 0.04, tv - 0.04, tv + 0.04, 0.0, 2.1, faces="xXyYZ")
    lbox(d, WHITE, tu0, tu1, tv - 0.03, tv + 0.03, 1.0, 2.0)
    lbox(d, "CT_Paper", tu0 + 0.08, tu1 - 0.08, tv - 0.036, tv - 0.03, 1.08, 1.92, faces="y")
    lbox(d, RED, tu0 + 0.08, tu1 - 0.08, tv - 0.040, tv - 0.036, 1.74, 1.92, faces="y")
    for k in range(5):
        lbox(d, HULL, tu0 + 0.16, tu1 - 0.16 - 0.1 * (k % 2), tv - 0.040, tv - 0.036, 1.60 - 0.11 * k, 1.63 - 0.11 * k, faces="y")
    B.thing("ship_timetable", "board", P((tu0 + tu1) / 2, tv - 0.05, 1.5), kyaw(0, -1), topic="ship")
    # the launch gate (the fade door to the ship) and where people wait
    B.thing("launch_gate", "door", P((GA + GB) / 2, V2 - 0.08, ZP + 1.0), kyaw(0, -1), to="P-SHIP/gangway")
    B.spot("launch_wait_1", "stand", P(UF - 1.2, 1.6, ZP), kyaw(0, 1))
    B.spot("launch_wait_2", "stand", P(UF - 3.6, 1.4, ZP), kyaw(0, 1))
    LANDING.clear()
    LANDING.update({"ship_ticket_box": P(cu, bv0 - 0.05, 1.3), "ship_timetable": P((tu0 + tu1) / 2, tv - 0.05, 1.5),
                    "launch_gate": P((GA + GB) / 2, V2 - 0.08, ZP + 1.0), "platform_z": ZP})
    B.note("ship landing stage at the pier head's seaward edge: balcony + 14 steps down to a platform at z %.2f; things "
           "ship_ticket_box, ship_timetable, launch_gate (-> P-SHIP/gangway)" % ZP)
    return [P(UT + 1.6, -1.9)[:2], P(bu1 + 0.3, bv0 - 0.4)[:2], P(UT + 1.6, V1 + 0.1)[:2], P(UF, V2 + 0.1)[:2], P(UF - 4.5, V2 + 0.1)[:2], P(UF - 4.5, 0.0)[:2]]
