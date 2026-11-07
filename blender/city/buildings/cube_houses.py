"""Cube houses: a row of tilted cube houses standing on hexagonal concrete pillar cores, with a covered pedestrian
street underneath and a bridge-like deck between the pillars. An original design in the spirit of the 1980s Dutch
tilted cube houses (Rotterdam / Helmond) - ours is different: every cube is a square prism tipped 45 deg on one
EDGE (its section is a diamond, the long axis runs into the plot), the row bends in a gentle arc, the cubes
alternate in colour and sit at slightly different heights, undersides in timber.

Parameters (module level, set before build()): COUNT cubes (default 7; 5 fits a 40 m plot, 7 a 60 m plot),
PITCH between pillars, ARC the sag of the row ends towards the hill.
Per cube (edge 5.4 m, section diagonal 7.64 m, 5.4 m long): one flat on 3 levels inside (not modelled), one door at
the foot of its hexagonal pillar (a deep niche, entrance "c<n>", kind flat_door). Faces: the street-facing diamond has
a square window and three triangular windows (top and the two side corners), the back diamond a square one, the two
upper slopes a square roof window each. Underneath: a concrete deck at z 3.0 (rise-free, 6.6 m wide) between the
pillars, railings, and a stair at each end going down to the street (rise 0.158, going 0.30). Headroom under a cube
edge >= 2.4 m. Max height ~13.6 m.
Enterable: nothing inside; the deck is a public walk. Spots: benches on the deck.
Ideas for later: lit lanterns under the cubes, kiosks in the pillars' niches, planters on the deck.
Material requests: none.
"""
import math
import random

from citykit import box, prism, offset_polyline, extrude_profile, Frame, Mesh, DOOR_W, DOOR_H
import geom
from _surrounds import add_surrounds

INFO = {
    "name_ru": "Кубические дома", "name_en": "Cube Houses",
    "style": "experimental / postmodern", "use": "residential",
    "inspired_by": "Tilted cube houses of the Netherlands (1980s): cubes on hexagonal pillar cores over a pedestrian "
                   "street; ours are edge-down prisms in an arc with colour and height variation",
}

COUNT = 7
VARIANTS = {"P-T1H1": {"COUNT": 5}, "P-T2H1": {"COUNT": 7}}   # the plan's two plots: 40 m and 60 m fronts
PITCH = 8.0
ARC = 1.6
E = 5.4                      # cube edge
H = E / math.sqrt(2)         # half diagonal
VC = 9.0                     # distance of the row axis from the street front line
DECK_Z = 3.0
DECK_HALF = 3.3              # half width of the deck
END_EXT = 3.95               # deck beyond the first / last pillar
COLOURS = ["CT_PlasterYellow", "CT_PlasterBlue", "CT_PlasterRed", "CT_PlasterPink", "CT_Plaster", "CT_PlasterBlue",
           "CT_PlasterYellow", "CT_PlasterRed"]
S2 = math.sqrt(0.5)


# ------------------------------------------------------------------------------------------------ helpers
def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _add(a, b, k=1.0):
    return (a[0] + b[0] * k, a[1] + b[1] * k, a[2] + b[2] * k)


def face_to(m, mat, pts, n=None, toward=None):
    """Add a face whose normal points along n (or towards the point `toward`)."""
    nn = _cross(_sub(pts[1], pts[0]), _sub(pts[2], pts[0]))
    if n is not None:
        flip = _dot(nn, n) < 0
    else:
        c = tuple(sum(p[i] for p in pts) / len(pts) for i in range(3))
        flip = _dot(nn, _sub(toward, c)) < 0
    m.face(mat, pts[::-1] if flip else pts)


def plane_wall(m, mat, O, e1, e2, n, w, h, holes, depth, reveal_mat=None):
    """A wall in the plane O + a*e1 + b*e2 (e1 x e2 = n outward), a 0..w, b 0..h, rectangular holes
    [(a0, a1, b0, b1)] lined by reveals `depth` deep (into -n)."""
    P = lambda a, b: (O[0] + e1[0] * a + e2[0] * b, O[1] + e1[1] * a + e2[1] * b, O[2] + e1[2] * a + e2[2] * b)
    us = sorted(set([0.0, w] + [x for hh in holes for x in hh[:2]]))
    zs = sorted(set([0.0, h] + [x for hh in holes for x in hh[2:]]))
    for i in range(len(us) - 1):
        for j in range(len(zs) - 1):
            cu, cz = (us[i] + us[i + 1]) / 2, (zs[j] + zs[j + 1]) / 2
            if any(hh[0] < cu < hh[1] and hh[2] < cz < hh[3] for hh in holes):
                continue
            face_to(m, mat, [P(us[i], zs[j]), P(us[i + 1], zs[j]), P(us[i + 1], zs[j + 1]), P(us[i], zs[j + 1])], n=n)
    for (a0, a1, b0, b1) in holes:
        ctr = _add(P((a0 + a1) / 2, (b0 + b1) / 2), n, -depth / 2)
        rm = reveal_mat or mat
        for (pa, pb) in (((a0, b0), (a1, b0)), ((a1, b0), (a1, b1)), ((a1, b1), (a0, b1)), ((a0, b1), (a0, b0))):
            A, Bp = P(*pa), P(*pb)
            face_to(m, rm, [A, Bp, _add(Bp, n, -depth), _add(A, n, -depth)], toward=ctr)


def poly_reveal(m, mat, pts, n, depth):
    """Reveal faces round a polygon (pts on the wall plane) into -n, facing the hole."""
    c = tuple(sum(p[i] for p in pts) / len(pts) for i in range(3))
    ctr = _add(c, n, -depth / 2)
    for k in range(len(pts)):
        A, Bp = pts[k], pts[(k + 1) % len(pts)]
        face_to(m, mat, [A, Bp, _add(Bp, n, -depth), _add(A, n, -depth)], toward=ctr)


def shrink(pts, f):
    c = tuple(sum(p[i] for p in pts) / len(pts) for i in range(3))
    return [tuple(c[i] + (p[i] - c[i]) * f for i in range(3)) for p in pts]


# ------------------------------------------------------------------------------------------------ one cube
def _cube(B, fr, idx, zb, colour, fid):
    """The edge-down cube at the local frame fr (origin at the pillar axis, u along the row, v inland)."""
    lm = Mesh()
    v0, v1 = -E / 2, E / 2
    zc = zb + H
    Bt, Rt, Tt, Lt = (0.0, zb), (H, zb + H), (0.0, zb + 2 * H), (-H, zb + H)

    def side(base, e2u, e2z, nu, nz, mat, holes=(), cards=False):
        e2 = (e2u, 0.0, e2z)
        n = (nu, 0.0, nz)
        e1 = _cross(e2, n)
        vv = v0 if e1[1] > 0 else v1
        O = (base[0], vv, base[1])
        plane_wall(lm, mat, O, e1, e2, n, E, E, list(holes), 0.32, "CT_Concrete")
        return O, e1, e2, n

    side(Bt, S2, S2, S2, -S2, "CT_Wood")                    # lower right
    side(Bt, -S2, S2, -S2, -S2, "CT_Wood")                  # lower left
    win_holes = [(1.7, 3.7, 1.7, 3.7)]
    slopes = []
    slopes.append(side(Rt, -S2, S2, S2, S2, "CT_ConcreteWhite", win_holes))      # upper right
    slopes.append(side(Lt, S2, S2, -S2, S2, "CT_ConcreteWhite", win_holes))      # upper left

    # the two diamond ends: a square opening (front: also three triangular ones)
    s = 1.2
    h = H
    tris = {
        "R": [(s, -s), (h, 0), (s, s)], "TR": [(s, s), (h, 0), (0, h)], "T": [(s, s), (0, h), (-s, s)],
        "TL": [(-s, s), (0, h), (-h, 0)], "L": [(-s, s), (-h, 0), (-s, -s)], "BL": [(-s, -s), (-h, 0), (0, -h)],
        "B": [(-s, -s), (0, -h), (s, -s)], "BR": [(s, -s), (0, -h), (h, 0)],
    }
    cards = []

    def Pend(vv, uz):
        return (uz[0], vv, zc + uz[1])

    for vv, n, front in ((v0, (0.0, -1.0, 0.0), True), (v1, (0.0, 1.0, 0.0), False)):
        window_tris = ("T", "L", "R") if front else ()
        for name, tri in tris.items():
            pts = [Pend(vv, p) for p in tri]
            if name in window_tris:
                inner = shrink(pts, 0.6)
                for k in range(3):
                    face_to(lm, colour, [pts[k], pts[(k + 1) % 3], inner[(k + 1) % 3], inner[k]], n=n)
                poly_reveal(lm, "CT_Concrete", inner, n, 0.32)
                cards.append(("tri", inner, n))
            else:
                face_to(lm, colour, pts, n=n)
        sq = [Pend(vv, p) for p in ((-s, -s), (s, -s), (s, s), (-s, s))]
        poly_reveal(lm, "CT_Concrete", sq, n, 0.32)
        cards.append(("sq", sq, n))
    B.m.extend(fr.apply(lm))

    # window cards (world coordinates) behind the openings
    inset = 0.2
    for kind, pts, n in cards:
        w = [fr(p) for p in pts]
        nw = _cross(_sub(w[1], w[0]), _sub(w[2], w[0]))
        # outward direction of the opening in world space (rotation only)
        o = fr((0.0, 0.0, 0.0))
        out = _sub(fr(n), o)
        if _dot(nw, out) < 0:
            w = w[::-1]
        if kind == "sq":
            # bottom-left, bottom-right, top-right, top-left seen from outside
            order = _square_order(w, out)
            B.window(order, flat=fid, inset=inset, cols=2, rows=1, frame=0.05)
        else:
            order = _tri_order(w, out)
            B.window(order, flat=fid, inset=inset, frame=0)
    # upper roof windows on the slopes
    for (O, e1, e2, n) in slopes:
        a0, a1, b0, b1 = win_holes[0]
        P = lambda a, b: (O[0] + e1[0] * a + e2[0] * b, O[1] + e1[1] * a + e2[1] * b, O[2] + e1[2] * a + e2[2] * b)
        pts = [fr(P(a0, b0)), fr(P(a1, b0)), fr(P(a1, b1)), fr(P(a0, b1))]
        B.window(pts, flat=fid, inset=0.2, cols=2, rows=2, frame=0.05)


def _square_order(w, out):
    """Order 4 coplanar points of a square as bottom-left, bottom-right, top-right, top-left seen from outside
    (right = out x up for an observer looking against `out`)."""
    up = (0.0, 0.0, 1.0)
    right = _cross(up, out)                  # up x out: for out = -y (street) gives +x? checked below
    # for a viewer looking against `out`, their right is  up x out ... verify sign with a known case:
    # out = (0,-1,0): viewer looks +y, right is +x;  up x out = (0,0,1) x (0,-1,0) = (1,0,0) -> +x. ok
    cx = [sum(p[i] for p in w) / 4 for i in range(3)]
    def key(p):
        d = _sub(p, tuple(cx))
        return (_dot(d, up) > 0, _dot(d, right) > 0)
    bl = next(p for p in w if key(p) == (False, False))
    br = next(p for p in w if key(p) == (False, True))
    tr = next(p for p in w if key(p) == (True, True))
    tl = next(p for p in w if key(p) == (True, False))
    return [bl, br, tr, tl]


def _tri_order(w, out):
    """A triangle as a degenerate quad [A, B, C, C] wound counter-clockwise seen from outside."""
    nw = _cross(_sub(w[1], w[0]), _sub(w[2], w[0]))
    if _dot(nw, out) < 0:
        w = [w[0], w[2], w[1]]
    return [w[0], w[1], w[2], w[2]]


# ------------------------------------------------------------------------------------------------ pillar
def _pillar(B, fr, ztop, eid):
    """Hexagonal concrete core with a deep door niche on the street-facing face."""
    lm = Mesh()
    hx = [(-1.3, -1.5), (1.3, -1.5), (2.0, 0.0), (1.3, 1.5), (-1.3, 1.5), (-2.0, 0.0)]
    # front face (edge 0): wall with the door niche
    O = (-1.3, -1.5, 0.0)
    e1, e2, n = (1.0, 0.0, 0.0), (0.0, 0.0, 1.0), (0.0, -1.0, 0.0)
    door = (0.75, 0.75 + DOOR_W, 0.0, DOOR_H)
    plane_wall(lm, "CT_ConcreteDark", O, e1, e2, n, 2.6, ztop, [door], 1.4, "CT_ConcreteDark")
    # back panel of the niche (the lobby wall)
    face_to(lm, "CT_Interior", [(-1.3 + door[0], -1.5 + 1.4, 0.0), (-1.3 + door[1], -1.5 + 1.4, 0.0),
                                (-1.3 + door[1], -1.5 + 1.4, door[3]), (-1.3 + door[0], -1.5 + 1.4, door[3])], n=n)
    face_to(lm, "CT_Floor", [(-1.3 + door[0], -1.5, 0.01), (-1.3 + door[1], -1.5, 0.01),
                             (-1.3 + door[1], -1.5 + 1.4, 0.01), (-1.3 + door[0], -1.5 + 1.4, 0.01)], n=(0, 0, 1))
    for k in range(1, 6):
        a, b = hx[k], hx[(k + 1) % 6]
        face_to(lm, "CT_ConcreteDark", [(a[0], a[1], 0.0), (b[0], b[1], 0.0), (b[0], b[1], ztop), (a[0], a[1], ztop)],
                n=(0.0, 0.0, 0.0) if False else (((a[0] + b[0]) / 2) * 1.0, ((a[1] + b[1]) / 2) * 1.0, 0.0))
    B.m.extend(fr.apply(lm))
    dm = Mesh()
    box(dm, "CT_Neon", -0.15 + 0.0 + (door[0] + door[1]) / 2 - 1.3 + 0.0, 0.15 + (door[0] + door[1]) / 2 - 1.3, -1.58, -1.5, 2.45, 2.6)
    B.detail.extend(fr.apply(dm))
    pos = fr((0.0, -1.5, 0.0))
    o = fr((0.0, 0.0, 0.0))
    out = _sub(fr((0.0, -1.0, 0.0)), o)
    B.entrance(eid, pos, math.degrees(math.atan2(out[0], -out[1])), DOOR_W, DOOR_H, "flat_door")


# ------------------------------------------------------------------------------------------------ build
def build(B):
    rnd = random.Random(1983)
    m = B.m
    n = COUNT
    xs = [(i - (n - 1) / 2) * PITCH for i in range(n)]
    half = max(abs(xs[0]), 1.0) + END_EXT
    P = [(x, VC - ARC + ARC * (x / half) ** 2) for x in xs]            # pillar axis points
    # heading at every pillar (central differences)
    th = []
    for i in range(n):
        a, b = P[max(i - 1, 0)], P[min(i + 1, n - 1)]
        th.append(math.atan2(b[1] - a[1], b[0] - a[0]))
    top = 0.0
    for i in range(n):
        zb = 5.4 + 0.25 * ((i * 3) % 4)             # cube bottom edge heights vary a little
        fr = Frame(P[i][0], P[i][1], th[i])
        fid = "0-%02d" % (i + 1)
        B.flat(fid, storey=0, beds=2 + (i % 2), door=fr((0.0, -1.5 + 1.3, 0.0)), entrance="c%d" % (i + 1))
        _pillar(B, fr, zb + 1.9, "c%d" % (i + 1))
        _cube(B, fr, i, zb, COLOURS[i % len(COLOURS)], fid)
        top = max(top, zb + 2 * H)

    # ---------------------------------------------------------------- the deck (bridge) along the axis
    t0 = (math.cos(th[0]), math.sin(th[0]))
    t1 = (math.cos(th[-1]), math.sin(th[-1]))
    Q = [(P[0][0] - t0[0] * END_EXT, P[0][1] - t0[1] * END_EXT)] + P + [(P[-1][0] + t1[0] * END_EXT, P[-1][1] + t1[1] * END_EXT)]
    Lp = offset_polyline(Q, DECK_HALF)           # inland side
    Rp = offset_polyline(Q, -DECK_HALF)          # street side
    for i in range(len(Q) - 1):
        poly = [Rp[i], Rp[i + 1], Lp[i + 1], Lp[i]]
        prism(m, "CT_ConcreteDark", poly, DECK_Z - 0.45, DECK_Z, top=False, bottom=True)
        m.face("CT_Paving", [(x, y, DECK_Z) for x, y in poly])
    # railings: inland side all along; street side except the two stair openings; ends closed
    B.railing(Lp, DECK_Z, h=1.05)
    k_open = 3.4                                   # the stair opening runs this far from each end
    def cut(pts, d0, d1):
        """Sub-polyline of pts between arc distances d0 and d1."""
        out = []
        acc = 0.0
        for (a, b) in zip(pts, pts[1:]):
            L = math.dist(a, b)
            lo, hi = max(d0 - acc, 0.0), min(d1 - acc, L)
            if hi > lo:
                f = lambda t: (a[0] + (b[0] - a[0]) * t / L, a[1] + (b[1] - a[1]) * t / L)
                if not out:
                    out.append(f(lo))
                out.append(f(hi))
            acc += L
        return out
    total = sum(math.dist(a, b) for a, b in zip(Rp, Rp[1:]))
    seg = cut(Rp, k_open, total - k_open)
    if len(seg) >= 2:
        B.railing(seg, DECK_Z, h=1.05)
    B.railing([Rp[0], Lp[0]], DECK_Z, h=1.05)
    B.railing([Lp[-1], Rp[-1]], DECK_Z, h=1.05)

    # ---------------------------------------------------------------- stairs at both ends (down to the street)
    stair_pts = []
    for end, (qa, qb, tt) in enumerate(((Q[0], Q[1], t0), (Q[-1], Q[-2], t1))):
        sgn = 1 if end == 0 else -1
        # the flight starts on the street-side deck edge, 1.7 m from the deck end
        d = sgn * 1.7
        cx, cy = Q[0 if end == 0 else -1][0] + tt[0] * d, Q[0 if end == 0 else -1][1] + tt[1] * d
        th_e = math.atan2(tt[1], tt[0])
        fr = Frame(cx - (-math.sin(th_e)) * (-DECK_HALF) * -1 * 0.0 + math.sin(th_e) * DECK_HALF,
                   cy - math.cos(th_e) * DECK_HALF, th_e)
        _flight(B, fr)
        pos = fr((0.0, -5.4, 0.0))
        stair_pts.append(pos)
        o = fr((0.0, 0.0, 0.0))
        out = _sub(fr((0.0, -1.0, 0.0)), o)
        B.entrance("deck_stair_%s" % ("w" if end == 0 else "e"), pos, math.degrees(math.atan2(out[0], -out[1])), 2.6, 2.2, "street")

    # ---------------------------------------------------------------- benches on the deck (back edge)
    for i in range(1, n - 1, 2):
        a = Lp[i + 1]
        b = Lp[i + 2] if i + 2 < len(Lp) else Lp[i + 1]
        mx, my = (P[i][0] + P[i + 1][0]) / 2, (P[i][1] + P[i + 1][1]) / 2
        nl = (-math.sin(th[i]), math.cos(th[i]))
        bx, by = mx + nl[0] * (DECK_HALF - 0.6), my + nl[1] * (DECK_HALF - 0.6)
        fr = Frame(bx, by, th[i])
        bm = Mesh()
        box(bm, "CT_Wood", -0.9, 0.9, -0.22, 0.22, 0.42, 0.47)
        box(bm, "CT_Wood", -0.9, 0.9, 0.17, 0.22, 0.47, 0.95)
        for sx in (-0.7, 0.7):
            box(bm, "CT_Steel", sx - 0.04, sx + 0.04, -0.18, 0.18, 0.0, 0.42)
        for k in range(len(bm.faces)):
            pass
        # bench sits on the deck: lift by DECK_Z
        bm = bm.transformed(lambda p: (p[0], p[1], p[2] + DECK_Z))
        B.detail.extend(fr.apply(bm))
        B.spot("deck_bench_%d" % i, "bench", fr((0.0, -0.1, DECK_Z + 0.47)), math.degrees(th[i]) + 180.0)

    # ---------------------------------------------------------------- footprint, height
    xs_all = [v[0] for v in m.verts] + [v[0] for v in B.detail.verts]
    ys_all = [v[1] for v in m.verts] + [v[1] for v in B.detail.verts]
    x0, x1, y0, y1 = min(xs_all), max(xs_all), min(ys_all), max(ys_all)
    B.footprint = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    B.height = top
    B.note("%d cube houses on hexagonal pillars, deck at z %.1f, max height %.1f m" % (n, DECK_Z, top))
    add_surrounds(B, kinds=("street", "flat_door"), seed=5)


def _flight(B, fr):
    """A stair of 19 risers going down from the deck edge (local v = 0, z = 3.0) towards -v, 2.6 m wide."""
    nst = 19
    r = DECK_Z / nst
    g = 0.30
    pts = [(0.0, DECK_Z)]
    for k in range(1, nst):
        pts.append((-g * (k - 1), DECK_Z - k * r))
        pts.append((-g * k, DECK_Z - k * r))
    pts.append((-g * (nst - 1), 0.0))
    pts.append((-g * (nst - 1), -0.2))
    pts.append((0.0, -0.2))
    clean = []
    for p in pts:
        if not clean or math.hypot(p[0] - clean[-1][0], p[1] - clean[-1][1]) > 1e-6:
            clean.append(p)
    lm = Mesh()
    # profile (a = v, b = z) extruded along u (v x z = u, right-handed)
    extrude_profile(lm, "CT_Concrete", "CT_Concrete", clean, -1.3, 1.3, lambda a, b, t: (t, a, b))
    B.m.extend(fr.apply(lm))
    # rails along both sides (posts + sloped rail), detail
    dm = Mesh()
    for sx in (-1.22, 1.22):
        for k in range(1, nst, 4):
            v = -g * (k - 1)
            z = DECK_Z - k * r
            box(dm, "CT_Steel", sx - 0.02, sx + 0.02, v - 0.02 - g, v + 0.02 - g, z, z + 1.05)
        za, zb = DECK_Z + 1.05 - r, 1.05
        va, vb = 0.0, -g * (nst - 1)
        for dx in (0.0,):
            dm.face("CT_Steel", [(sx - 0.02, va, za), (sx - 0.02, vb, zb), (sx + 0.02, vb, zb), (sx + 0.02, va, za)])
            dm.face("CT_Steel", [(sx + 0.02, va, za), (sx + 0.02, vb, zb), (sx - 0.02, vb, zb), (sx - 0.02, va, za)])
    B.detail.extend(fr.apply(dm))
