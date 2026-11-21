"""Walkable timber structures of the trail from the club's headland down to the sea and the town (docs/south.md):
`cliff_stair`, `plank_bridge` (v0 6 m, v1 9 m) and `boardwalk` (v0..v3), on the North's kit and atlas (blender/north).

    python3 blender/south/build_south.py props_walk [cliff_stair | plank_bridge:1 | boardwalk ...]
    python3 blender/south/props_walk.py          self-check of the cliff stair's layout (no bpy build needed beyond the kit import)

Frame as everywhere in the kit: metres, Z up, Blender -Y is the front; each kind's origin is in its notes.

Timber is the atlas's weathered grey `wood` tile (a few `wood_dark` boards and the posts' and sills' darker members), cut ends
`endgrain`. Every member is swept in pieces no longer than SEG metres along its grain, each piece with its own place in the tile,
so a long stringer or rail never shows one tile stretched along it (the tile covers 1.2 m: SEG 1.5 = at most 1.25x).
"""
import json
import math
import os
import random

import bpy  # noqa: F401  (brings mathutils along when run as a plain script)
from mathutils import Vector

try:
    from northkit import Builder, V
except ImportError:                                  # run as a script: python3 blender/south/props_walk.py
    import sys
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "north"))
    from northkit import Builder, V

HERE = os.path.dirname(os.path.abspath(__file__))
SEG = 1.5
WOOD, DARK = "wood", "wood_dark"


# =========================================================================================================
# geometry helpers
# =========================================================================================================

def sweep(b, rings, tile=WOOD, caps=True, cap_tile=DARK, seg=SEG, skip_bottom=False, cap0=True):
    """A closed member through cross-section rings (lists of 4 Vectors, same winding), split into pieces <= seg long.
    U runs along the member (grain), V across each side; each piece takes its own offset inside the tile."""
    rr = [rings[0]]
    for r0, r1 in zip(rings[:-1], rings[1:]):
        c0, c1 = sum(r0, Vector()) / 4, sum(r1, Vector()) / 4
        n = max(1, math.ceil((c1 - c0).length / seg - 1e-6))
        for k in range(1, n + 1):
            t = k / n
            rr.append(r1 if k == n else [a.lerp(c, t) for a, c in zip(r0, r1)])
    aniso = tile in (WOOD, DARK, "plank_edge", "bark")
    for j in range(len(rr) - 1):
        A, B = rr[j], rr[j + 1]
        for k in range(4):
            k2 = (k + 1) % 4
            if skip_bottom and k == 3:
                continue
            q = [A[k], A[k2], B[k2], B[k]]
            la = max((B[k] - A[k]).length, (B[k2] - A[k2]).length)
            lb = (A[k2] - A[k]).length
            off = (b.rnd.random(), b.rnd.random())
            uv = [b.mapuv(tile, 0, 0, la, lb, off, aniso), b.mapuv(tile, 0, lb, la, lb, off, aniso),
                  b.mapuv(tile, (B[k2] - A[k2]).length, lb, la, lb, off, aniso), b.mapuv(tile, (B[k] - A[k]).length, 0, la, lb, off, aniso)]
            b.face(q, uv)
    if caps:
        for ring, flip in ((rr[0], True), (rr[-1], False)):
            if flip and not cap0:
                continue
            pts = ring[::-1] if flip else list(ring)
            c = sum(pts, Vector()) / 4
            e1 = (pts[1] - pts[0]).normalized()
            nrm = (pts[1] - pts[0]).cross(pts[2] - pts[1])
            e2 = nrm.cross(e1).normalized() if nrm.length > 1e-9 else Vector((0, 0, 1))
            rad = max((p - c).length for p in pts)
            if cap_tile == "endgrain":
                u0, v0, du, dv = b.win("endgrain")
                uv = [(u0 + (0.5 + (p - c).dot(e1) / (2 * rad) * 0.9) * du, v0 + (0.5 + (p - c).dot(e2) / (2 * rad) * 0.9) * dv) for p in pts]
            else:
                off = (b.rnd.random(), b.rnd.random())
                uv = [b.mapuv(cap_tile, (p - c).dot(e1) + rad, (p - c).dot(e2) + rad, 2 * rad, 2 * rad, off) for p in pts]
            b.face(pts, uv)


def beam(b, p0, p1, w, h, up=None, tile=WOOD, **kw):
    """A square-cut member from p0 to p1: w across (perpendicular to `up`), h along `up` (default: world Z, or Y for verticals)."""
    p0, p1 = Vector(p0), Vector(p1)
    ax = (p1 - p0).normalized()
    if up is None:
        up = Vector((0, 0, 1)) if abs(ax.z) < 0.9 else Vector((0, 1, 0))
    ew = Vector(up).cross(ax).normalized()
    eh = ax.cross(ew).normalized()
    cs = [ew * (w / 2) - eh * (h / 2), ew * (w / 2) + eh * (h / 2), -ew * (w / 2) + eh * (h / 2), -ew * (w / 2) - eh * (h / 2)]
    sweep(b, [[p0 + c for c in cs], [p1 + c for c in cs]], tile, **kw)


def boxa(b, c, size, tile=WOOD, **kw):
    """Axis-aligned box centred at c; the grain runs along its longest side."""
    c = Vector(c)
    ax = max(range(3), key=lambda i: size[i])
    d = Vector((0, 0, 0))
    d[ax] = size[ax] / 2
    o = [i for i in range(3) if i != ax]
    if ax == 2:
        beam(b, c - d, c + d, size[0], size[1], up=Vector((0, 1, 0)), tile=tile, **kw)
    else:
        upv = Vector((0, 0, 1))
        beam(b, c - d, c + d, size[o[0]] if ax == 1 else size[1], size[2], up=upv, tile=tile, **kw)


def slope_x(b, x0, x1, zt0, zt1, depth, y0, y1, tile=WOOD, **kw):
    """A board along X with plumb-cut ends: top edge from (x0, zt0) to (x1, zt1), `depth` measured vertically, y0..y1 thick."""
    def ring(x, zt):
        return [V(x, y0, zt - depth), V(x, y0, zt), V(x, y1, zt), V(x, y1, zt - depth)]
    sweep(b, [ring(x0, zt0), ring(x1, zt1)], tile, **kw)


def post(b, x, y, z0, z1, s=0.14, tile=DARK, rec=None):
    """A square post from z0 (buried: no bottom face) to z1."""
    beam(b, V(x, y, z0), V(x, y, z1), s, s * 0.96, up=Vector((0, 1, 0)), tile=tile, cap0=False, seg=2.0)
    if rec is not None:
        rec.append((x, y, z1, z0))


def bench(b, c, along, facing, L=1.3, seat_h=0.45, back=True, rnd=None, lean=True):
    """A plank bench: c = the point on the deck under the seat's middle, `along` = unit vector of its length, `facing` = the sitter's
    facing (unit, horizontal); legs are two plank frames, back rest on the -facing side. Returns [seat points]."""
    rnd = rnd or random.Random(5)
    c, al, fa = Vector(c), Vector(along).normalized(), Vector(facing).normalized()
    up = Vector((0, 0, 1))
    D = 0.40
    for sgn in (-1, 1):
        q = c + al * (sgn * (L / 2 - 0.16))
        for fy in (-1, 1):                                          # legs
            top = seat_h - 0.05 if (fy > 0 or not back) else 0.88
            p = q + fa * (fy * 0.14) - (fa * 0.03 if fy < 0 and back else Vector())
            beam(b, p + up * 0.0, p + up * top, 0.07, 0.07, up=Vector(al), tile=DARK)
        beam(b, q - fa * 0.22 + up * (seat_h - 0.1), q + fa * 0.22 + up * (seat_h - 0.1), 0.06, 0.09, up=up, tile=DARK)   # cleat
        beam(b, q - fa * 0.17 + up * 0.12, q + fa * 0.17 + up * 0.12, 0.05, 0.07, up=up, tile=DARK)                        # low rung
    for k, f in enumerate((-0.1, 0.1)):                             # two seat planks
        p = c + fa * f + up * (seat_h - 0.025)
        ln = L + rnd.uniform(-0.02, 0.02)
        beam(b, p - al * ln / 2, p + al * ln / 2, 0.185, 0.045, up=up, tile=WOOD)
    if back:
        for z in (0.66, 0.82):
            p = c - fa * 0.19 + up * z
            beam(b, p - al * (L / 2 - 0.05), p + al * (L / 2 - 0.05), 0.035, 0.11, up=up, tile=WOOD)
    return [c + al * (sgn * L / 4) + up * seat_h for sgn in (-1, 1)]


def brace(b, p0, p1, n, w=0.04, h=0.12, tile=WOOD):
    """A brace board from p0 to p1 lying flat on a face whose normal is n (thin along n)."""
    p0, p1, n = Vector(p0), Vector(p1), Vector(n).normalized()
    ax = (p1 - p0).normalized()
    beam(b, p0, p1, w, h, up=ax.cross(n), tile=tile)


def cbox(name, c, size, rx=0.0, ry=0.0):
    return {"name": name, "center": [round(v, 3) for v in c], "size": [round(v, 3) for v in size],
            "rot_x_deg": round(rx, 3), "rot_y_deg": round(ry, 3)}


def yaw_of(facing):
    """Seat yaw: degrees counter-clockwise from above, 0 = facing -Y."""
    return round(math.degrees(math.atan2(facing[0], -facing[1])) % 360.0, 1)


# =========================================================================================================
# cliff_stair: the layout
# =========================================================================================================

PROFILE = json.load(open(os.path.join(HERE, "stair_profile.json")))["profile"]
D_FOOT = 34.1                  # profile distance of the foot (the beach)
Z_TOP = 0.03                   # the top deck's walking surface (3 cm over the flat brow, so it never z-fights the ground)
T = 0.30                       # tread
N_FLIGHTS, N_RISERS = 14, 18   # 14 flights of 18 risers (17 treads, 5.1 m run)
RUN = (N_RISERS - 1) * T
XE = RUN / 2                   # flights run between x -2.55 and 2.55
LAND_W = 1.7                   # landings at |x| 2.55..4.25
XO = XE + LAND_W
HW = 0.65                      # half clear width between the housed stringers (1.3 m)
PY = 0.79                      # post line (centre) off a flight's centre
PS = 0.14                      # post section
GAP_D = 2.0                    # min distance between neighbouring flights' centre lines
BURY = 1.5                     # posts reach this far below the profile
RAIL_TOP = 0.95                # handrail top over the nosing line
LAND_RAIL = 1.0                # landing rails' top over the deck
BENCH_LANDINGS = (5, 10)


def ground(d):
    """Profile height at distance d out from the top."""
    P = PROFILE
    if d <= P[0][0]:
        return P[0][1]
    for (a, za), (c, zc) in zip(P[:-1], P[1:]):
        if d <= c:
            return za + (zc - za) * (d - a) / (c - a)
    (a, za), (c, zc) = P[-2], P[-1]
    return zc + (zc - za) * (d - c) / (c - a)


def gy(y):
    return ground(-y)


def ginv(z):
    """Smallest profile distance where the ground is at or below z."""
    d = 0.0
    while ground(d) > z and d < D_FOOT + 5:
        d += 0.005
    return d


def layout():
    """-> dict(r, ds[flight 1..N], zs[0..N] landing heights; zs[0] = the top deck)."""
    zf = ground(D_FOOT)
    r = (Z_TOP - zf) / (N_FLIGHTS * N_RISERS)
    zs = [Z_TOP - i * N_RISERS * r for i in range(N_FLIGHTS + 1)]
    ds = []
    for i in range(1, N_FLIGHTS + 1):
        if i == N_FLIGHTS:
            ds.append(D_FOOT)
            break
        dmin = ginv(zs[i] - 0.40) + (PY + PS / 2)          # the landing's uphill edge clears the ground by 0.4 m
        ds.append(dmin if not ds else max(ds[-1] + GAP_D, dmin))
    assert ds[-1] - ds[-2] >= GAP_D - 1e-6, "flights do not fit above the foot"
    return {"r": r, "ds": ds, "zs": zs, "zf": zf}


def walk_path(L=None):
    L = L or layout()
    ds, zs = L["ds"], L["zs"]
    xm = (XE + XO) / 2
    pts = [(0.0, 0.0, Z_TOP), (0.0, -1.2, Z_TOP), (-xm, -1.2, Z_TOP), (-xm, -ds[0], Z_TOP)]
    for i in range(1, N_FLIGHTS + 1):
        dr = 1 if i % 2 else -1
        y = -ds[i - 1]
        pts.append((-dr * XE, y, zs[i - 1]))
        pts.append((dr * XE, y, zs[i]))
        if i < N_FLIGHTS:
            pts.append((dr * xm, y, zs[i]))
            pts.append((dr * xm, -ds[i], zs[i]))
        else:
            pts.append((dr * (XE + 0.8), y, zs[i]))
    return [[round(v, 3) for v in p] for p in pts]


# =========================================================================================================
# cliff_stair: the mesh
# =========================================================================================================

def _platform(b, rnd, x0, x1, y0, y1, z, bearer_x, joist_step=0.75, cut_buried=True):
    """Deck boards along Y over x0..x1 (y0 > y1, seaward is lower y), joists along X every ~0.6 m, bearers along Y at bearer_x.
    Members wholly under the profile are left out (the top deck lies on the flat brow)."""
    bt = 0.045
    w = abs(x1 - x0)
    n = max(2, round(w / 0.185))
    pitch = w / n
    xs, xe_ = min(x0, x1), max(x0, x1)
    for i in range(n):
        x = xs + pitch * (i + 0.5)
        t = bt + rnd.uniform(-0.004, 0.004)
        e0, e1 = rnd.uniform(-0.03, 0.0), rnd.uniform(0.0, 0.03)
        zz = z + rnd.uniform(-0.004, 0.0)
        tl = DARK if rnd.random() < 0.1 else WOOD
        beam(b, V(x, y1 + e0, zz - t / 2), V(x, y0 + e1, zz - t / 2), pitch - 0.012 - rnd.uniform(0, 0.008), t, tile=tl, skip_bottom=True)
    zj0, zj1 = z - bt, z - bt - 0.16
    nj = max(2, math.ceil((y0 - y1) / joist_step) + 1)
    for k in range(nj):
        y = y1 + 0.06 + (y0 - y1 - 0.12) * k / (nj - 1)
        if cut_buried and gy(y) > zj1 + 0.02:
            continue
        beam(b, V(xs + 0.02, y, (zj0 + zj1) / 2 - 0.001), V(xe_ - 0.02, y, (zj0 + zj1) / 2 - 0.001), 0.07, 0.16, tile=WOOD)
    zb0, zb1 = zj1, zj1 - 0.16
    for x in bearer_x:
        ya, yb = y0, y1
        if cut_buried:                                  # trim the bearer to the stretch over the ground
            while ya > yb and gy(ya) > zb1 + 0.02:
                ya -= 0.05
        if ya - yb > 0.3:
            beam(b, V(x, yb + 0.02, (zb0 + zb1) / 2), V(x, ya - 0.02, (zb0 + zb1) / 2), 0.12, 0.16, tile=DARK)
    return zb1


def _hrail(b, p, q, zl, inward, posts_line=True):
    """A landing's rail between post centres p and q (x, y): a top board and a mid board on the inward faces of the posts."""
    p, q = Vector((p[0], p[1], 0)), Vector((q[0], q[1], 0))
    iv = Vector((inward[0], inward[1], 0)).normalized()
    off = iv * (PS / 2 + 0.02)
    al = (q - p).normalized()
    a, c = p + off - al * 0.05, q + off + al * 0.05
    for zc, h in ((zl + LAND_RAIL - 0.06, 0.12), (zl + 0.5, 0.10)):
        beam(b, V(a.x, a.y, zc), V(c.x, c.y, zc), 0.04, h, tile=WOOD)


def _xbrace(b, pa, pb, za_hi, za_lo, zb_hi, zb_lo, face_off):
    """Diagonal braces between two posts (centres pa, pb) on the face offset by face_off (Vector): one each way when tall enough."""
    hi = min(za_hi, zb_hi)
    lo = max(za_lo, zb_lo)
    if hi - lo < 0.9:
        return
    A = Vector((pa[0], pa[1], 0)) + face_off
    B = Vector((pb[0], pb[1], 0)) + face_off
    n = face_off.normalized()
    brace(b, V(A.x, A.y, hi), V(B.x, B.y, lo), n)
    if hi - lo > 3.0:                                  # tall: a second diagonal the other way (an X)
        off2 = n * 0.04
        brace(b, V(A.x + off2.x, A.y + off2.y, lo), V(B.x + off2.x, B.y + off2.y, hi), n)


def cliff_stair(variant):
    b = Builder("cliff_stair")
    rnd = random.Random(1941)
    L = layout()
    r, ds, zs, zf = L["r"], L["ds"], L["zs"], L["zf"]
    posts = []                                          # (x, y, top, bottom) for the self-check
    boxes = []
    seats = []
    xm = (XE + XO) / 2
    xi_ = XE + PS / 2                                    # landing posts' inner line |x|
    xo_ = XO - PS / 2                                    # outer line
    slope = N_RISERS * r / RUN
    ang = math.degrees(math.atan(slope))

    def zn(i, u):
        """nosing line of flight i at distance u from its top edge."""
        return zs[i - 1] - u * r / T

    def ppost(x, y, top):
        post(b, x, y, gy(y) - BURY, top, PS, DARK, posts)

    # ---- flights ------------------------------------------------------------------------------------------
    for i in range(1, N_FLIGHTS + 1):
        dr = 1 if i % 2 else -1
        yc = -ds[i - 1]
        xa = -dr * XE
        X = lambda u: xa + dr * u
        last = i == N_FLIGHTS
        # treads: two boards each, housed 2 cm into the stringers
        for k in range(1, N_RISERS):
            zt = zs[i - 1] - k * r
            for (u0, u1) in (((k - 1) * T + 0.006, k * T - 0.156), (k * T - 0.146, k * T)):
                th = 0.05 + rnd.uniform(-0.003, 0.003)
                ztt = zt + rnd.uniform(-0.004, 0.0)
                ln = HW + 0.02
                e = rnd.uniform(-0.006, 0.006)
                beam(b, V(X((u0 + u1) / 2), yc - ln + e, ztt - th / 2), V(X((u0 + u1) / 2), yc + ln + e, ztt - th / 2),
                     u1 - u0 - rnd.uniform(0, 0.006), th, tile=DARK if rnd.random() < 0.07 else WOOD,
                     caps=False, skip_bottom=True)        # ends housed in the stringers; the underside is never in view
        if last:                                         # the bottom step: a sleeper block sunk 0.4 m into the sand
            zt = zs[i - 1] - (N_RISERS - 1) * r - 0.05
            boxa(b, V(X(RUN - T / 2), yc, (zt + zf - 0.4) / 2), (T - 0.02, 2 * HW + 0.04, zt - zf + 0.4), DARK)
        # housed stringers (plumb cut): top 0.10 over the nosing line, 0.32 deep
        u_end = RUN + (0.10 if not last else 0.25)
        for sy in (-1, 1):
            y0, y1 = yc + sy * HW, yc + sy * (HW + 0.07)
            ua, ub = 0.0, u_end
            slope_x(b, X(ua), X(ub), zn(i, ua) + 0.10, zn(i, ub) + 0.10, 0.32 if not last else 0.32, min(y0, y1), max(y0, y1), WOOD)
        # posts: the landing posts carry the ends; two pairs between, rising from the ground to the handrail
        us = [-PS / 2 + (RUN + PS) * k / 3 for k in (1, 2)]
        if last:
            us.append(RUN + PS / 2)
        for u in us:
            x = X(u)
            for sy in (-1, 1):
                ppost(x, yc + sy * PY, zn(i, u) + RAIL_TOP - 0.07)
            # ledger under the stringers, through both posts
            zl_ = zn(i, u) + 0.10 - 0.32 - 0.07
            beam(b, V(x - dr * 0.0, yc - PY - 0.1, zl_), V(x, yc + PY + 0.1, zl_), 0.09, 0.14, up=Vector((0, 0, 1)), tile=DARK)
            # across the pair: a diagonal (and a cross + tie when tall)
            hi = zl_ - 0.12
            _xbrace(b, (x, yc + PY), (x, yc - PY), hi, gy(yc + PY) + 0.35, hi, gy(yc - PY) + 0.35,
                    Vector((-dr * (PS / 2 + 0.02), 0, 0)))
        # along each side: diagonals between consecutive posts (the landing posts at both ends included)
        ul = [-PS / 2] + us[:2] + [RUN + PS / 2]
        for sy in (-1,):                                 # the seaward side only (the uphill side faces the cliff, 0.3 m from the next flight)
            y = yc + sy * PY
            gl = gy(y) + 0.4
            for k in range(3):
                ua, ub = ul[k], ul[k + 1]
                if last and k == 2:
                    continue
                hia = zn(i, ua) + 0.10 - 0.32 - 0.25
                hib = zn(i, ub) + 0.10 - 0.32 - 0.25
                A, B_ = (X(ua), y), (X(ub), y)
                if k % 2:
                    A, B_, hia, hib = B_, A, hib, hia
                hi, lo = min(hia, hib), gl
                if hi - lo > 0.9:
                    o = sy * (PS / 2 + 0.02)
                    brace(b, V(A[0], A[1] + o, hi), V(B_[0], B_[1] + o, lo), (0, 1, 0))
        # handrails on the posts' tops and mid rails on their inner faces, from landing post to landing post
        u0, u1 = -PS / 2 - 0.06, RUN + PS / 2 + 0.06
        for sy in (-1, 1):
            yp = yc + sy * PY
            slope_x(b, X(u0), X(u1), zn(i, u0) + RAIL_TOP, zn(i, u1) + RAIL_TOP, 0.07, yp - 0.045, yp + 0.045, WOOD)
            yi = yc + sy * (PY - PS / 2 - 0.02)
            slope_x(b, X(u0), X(u1), zn(i, u0) + 0.55, zn(i, u1) + 0.55, 0.10, yi - 0.02, yi + 0.02, WOOD)
        # collision: the ramp (top plane = the nosing line from landing edge to landing edge), two rail walls
        a = math.radians(ang) * dr
        Ls = math.hypot(RUN, N_RISERS * r)
        ext = 0.3 if last else 0.1
        Ltot = Ls + ext
        cu = (Ltot / 2) * math.cos(math.radians(ang))
        mx, mz = X(cu), zs[i - 1] - cu * slope
        nx, nz = math.sin(a), math.cos(a)
        boxes.append(cbox("flight_%d" % i, (mx - nx * 0.15, yc, mz - nz * 0.15), (Ltot, 2 * PY, 0.3), ry=math.degrees(a)))
        cu = RUN / 2
        for sy, nm in ((1, "n"), (-1, "s")):
            boxes.append(cbox("flight_%d_rail_%s" % (i, nm), (X(cu), yc + sy * PY, zs[i - 1] - cu * slope + 0.5),
                              (math.hypot(RUN + PS + 0.12, (RUN + PS + 0.12) * slope), 0.14, 1.1), ry=math.degrees(a)))

    # ---- landings 1..N-1 ---------------------------------------------------------------------------------
    for i in range(1, N_FLIGHTS):
        s = 1 if i % 2 else -1
        zl = zs[i]
        yA, yB = -ds[i - 1], -ds[i]
        y0, y1 = yA + PY + PS / 2, yB - PY - PS / 2
        xin, xout = s * XE, s * XO
        _platform(b, rnd, xin, xout, y0, y1, zl, [s * xi_, s * xo_])
        P = {}
        for nm, y in (("A+", yA + PY), ("A-", yA - PY), ("B+", yB + PY), ("B-", yB - PY)):
            flight_top = None
            if nm in ("A+", "A-"):                       # flight i's bottom end
                flight_top = zs[i - 1] - (RUN + PS / 2) * r / T + RAIL_TOP
            else:                                        # flight i+1's top end
                flight_top = zs[i] + (PS / 2) * r / T + RAIL_TOP
            ppost(s * xi_, y, max(zl + LAND_RAIL + 0.04, flight_top + 0.05))
            ppost(s * xo_, y, zl + LAND_RAIL + 0.04)
            P[nm] = y
        # outer rail posts in the long gap
        ys_out = [P["A+"], P["A-"], P["B+"], P["B-"]]
        cen = Vector((s * xm, (y0 + y1) / 2))
        inward = lambda p, q: (cen - Vector(((p[0] + q[0]) / 2, (p[1] + q[1]) / 2)))
        for pa, pb in ((P["A+"], P["B-"]),):
            _hrail(b, (s * xo_, pa), (s * xo_, pb), zl, (-s, 0))
        _hrail(b, (s * xi_, P["A+"]), (s * xo_, P["A+"]), zl, (0, -1))
        _hrail(b, (s * xi_, P["B-"]), (s * xo_, P["B-"]), zl, (0, 1))
        _hrail(b, (s * xi_, P["A-"]), (s * xi_, P["B+"]), zl, (s, 0))
        # bracing on the outer face and the uphill / seaward faces
        zlo = zl - 0.045 - 0.32 - 0.12
        for ya, yb in zip(ys_out[:-1], ys_out[1:]):
            _xbrace(b, (s * xo_, ya), (s * xo_, yb), zlo, gy(ya) + 0.35, zlo, gy(yb) + 0.35, Vector((s * (PS / 2 + 0.02), 0, 0)))
        _xbrace(b, (s * xi_, P["B-"]), (s * xo_, P["B-"]), zlo, gy(P["B-"]) + 0.35, zlo, gy(P["B-"]) + 0.35, Vector((0, -(PS / 2 + 0.02), 0)))
        # collision
        cx = s * (XE + XO) / 2
        boxes.append(cbox("landing_%d" % i, (cx + s * 0.05 * 0, (y0 + y1) / 2, zl - 0.15), (LAND_W + 0.2, y0 - y1, 0.3)))
        boxes.append(cbox("landing_%d_rail_out" % i, (s * xo_, (P["A+"] + P["B-"]) / 2, zl + 0.5), (0.14, P["A+"] - P["B-"] + 0.14, 1.1)))
        boxes.append(cbox("landing_%d_rail_up" % i, (s * (xi_ + xo_) / 2, P["A+"], zl + 0.5), (xo_ - xi_ + 0.14, 0.14, 1.1)))
        boxes.append(cbox("landing_%d_rail_sea" % i, (s * (xi_ + xo_) / 2, P["B-"], zl + 0.5), (xo_ - xi_ + 0.14, 0.14, 1.1)))
        boxes.append(cbox("landing_%d_rail_in" % i, (s * xi_, (P["A-"] + P["B+"]) / 2, zl + 0.5), (0.14, P["A-"] - P["B+"] + 0.14, 1.1)))
        if i in BENCH_LANDINGS:
            yb_ = (P["A-"] + P["B+"]) / 2
            fac = Vector((-s, 0, 0))
            c = V(s * (xo_ - PS / 2 - 0.25), yb_, zl)
            for p in bench(b, c, (0, 1, 0), fac, L=1.3, rnd=rnd):
                seats.append([round(p.x, 3), round(p.y, 3), round(p.z, 3), yaw_of(fac)])
            boxes.append(cbox("landing_%d_bench" % i, (c.x - s * 0.02, c.y, zl + 0.3), (0.5, 1.34, 0.6)))

    # ---- the top deck at the brow and the strip down to flight 1 -----------------------------------------
    d1 = ds[0]
    XA0, XA1, YA1 = -XO, 0.85, -3.2
    yB1 = -d1 - PY - PS / 2
    _platform(b, rnd, XA0, XA1, 0.0, YA1, Z_TOP, [-xo_, -xi_, -0.9, 0.78])
    _platform(b, rnd, XA0, -XE, YA1, yB1, Z_TOP, [-xo_, -xi_])
    zt_rail = Z_TOP + LAND_RAIL + 0.04
    west = [-PS / 2, -1.6, YA1 + PS / 2, -4.9, -d1 + PY, -d1 - PY]
    for y in west:
        ppost(-xo_, y, zt_rail)
    for y in (YA1 + PS / 2, -4.9, -d1 + PY, -d1 - PY):
        top = zt_rail if y > -d1 + 1 else max(zt_rail, Z_TOP + (PS / 2) * r / T + RAIL_TOP + 0.05)
        ppost(-xi_, y, top)
    for x in (-2.1, -0.9):
        ppost(x, -PS / 2, zt_rail)
        ppost(x, YA1 + PS / 2, zt_rail)
    ppost(0.78, -1.6, zt_rail)
    ppost(0.78, YA1 + PS / 2, zt_rail)
    # the entry arch: two posts, a lintel, knee braces
    for x in (-0.78, 0.78):
        post(b, x, -0.09, -BURY, 2.52, 0.16, DARK, posts)
    beam(b, V(-1.08, -0.09, 2.42), V(1.08, -0.09, 2.42), 0.16, 0.22, tile=WOOD)
    beam(b, V(-1.0, -0.09, 2.559), V(1.0, -0.09, 2.559), 0.1, 0.06, tile=DARK)          # weather board on the lintel
    for x in (-0.78, 0.78):
        sg = 1 if x < 0 else -1
        beam(b, V(x + sg * 0.06, -0.09, 1.95), V(x + sg * 0.42, -0.09, 2.32), 0.08, 0.08, up=Vector((0, 1, 0)), tile=WOOD)
    yt = -PS / 2
    rails = [((-xo_, yt), (-0.78, yt), (0, -1)),
             ((-xo_, yt), (-xo_, -d1 - PY), (1, 0)),
             ((0.78, yt), (0.78, YA1 + PS / 2), (-1, 0)),
             ((0.78, YA1 + PS / 2), (-xi_, YA1 + PS / 2), (0, 1)),
             ((-xi_, YA1 + PS / 2), (-xi_, -d1 + PY), (-1, 0)),
             ((-xo_, -d1 - PY), (-xi_, -d1 - PY), (0, 1))]
    for p, q, iv in rails:
        _hrail(b, p, q, Z_TOP, iv)
    zlo = Z_TOP - 0.045 - 0.32 - 0.12
    for ya, yb in zip(west[2:-1], west[3:]):
        _xbrace(b, (-xo_, ya), (-xo_, yb), zlo, gy(ya) + 0.35, zlo, gy(yb) + 0.35, Vector((-(PS / 2 + 0.02), 0, 0)))
    _xbrace(b, (-xo_, -d1 - PY), (-xi_, -d1 - PY), zlo, gy(-d1 - PY) + 0.35, zlo, gy(-d1 - PY) + 0.35, Vector((0, -(PS / 2 + 0.02), 0)))
    seat_c = V(-1.25, -2.42, Z_TOP)
    fac = Vector((0, -1, 0))
    for p in bench(b, seat_c, (1, 0, 0), fac, L=1.5, rnd=rnd):
        seats.append([round(p.x, 3), round(p.y, 3), round(p.z, 3), yaw_of(fac)])
    boxes.append(cbox("top_bench", (seat_c.x, seat_c.y + 0.02, Z_TOP + 0.3), (1.54, 0.5, 0.6)))
    boxes.append(cbox("top_deck", ((XA0 + XA1) / 2, YA1 / 2, Z_TOP - 0.15), (XA1 - XA0, -YA1, 0.3)))
    boxes.append(cbox("top_strip", ((XA0 - XE) / 2, (YA1 + yB1) / 2, Z_TOP - 0.15), (LAND_W + 0.2, YA1 - yB1, 0.3)))
    for k, (p, q, iv) in enumerate(rails):
        cx, cy = (p[0] + q[0]) / 2, (p[1] + q[1]) / 2
        boxes.append(cbox("top_rail_%d" % k, (cx, cy, Z_TOP + 0.5), (abs(q[0] - p[0]) + 0.14, abs(q[1] - p[1]) + 0.14, 1.1)))

    wp = walk_path(L)
    notes = ("timber stair down the 41.6 m sea cliff (stair_profile.json): origin = the top deck's landward walking edge on the brow "
             "(profile distance 0, ground z 0; the deck's walking surface is z %.2f so it never z-fights the flat brow); the cliff falls toward "
             "the front (-Y): profile distance d = -y. A top deck (x -4.25..0.85, y 0..-3.2) entered through a timber arch at x 0, y 0, a "
             "strip down its west side to flight 1, then %d flights of %d risers (%.3f m) x %d treads (0.30 m), %.1f deg ramp, clear width 1.3 m, "
             "zigzagging along local X between x -2.55 and 2.55 with 1.7 m landings at |x| 2.55..4.25 (overall width 8.5 m); flight centre "
             "lines at y %s. Benches on landings %s and on the top deck. Posts reach 1.5 m below the profile at their spot. The last flight "
             "ends on the beach at x %.2f, y %.2f, z %.2f (last step sunk 0.4 m into the sand). collision_boxes: mesh space, Blender axes; one "
             "sloped ramp per flight (top plane = the nosing line, rot_y_deg right-handed about +Y: positive tips +X down), one box per "
             "landing, rail walls 1.1 m high along every open edge"
             % (Z_TOP, N_FLIGHTS, N_RISERS, r, N_RISERS - 1, ang, ", ".join("%.2f" % -d for d in ds), BENCH_LANDINGS,
                wp[-1][0], wp[-1][1], wp[-1][2]))
    b._stair_posts = posts
    return b, {"seats": seats, "notes": notes, "posts": posts,
               "manifest": {"axis": "y", "drop": round(Z_TOP - zf, 3), "riser": round(r, 4), "tread": T, "flights": N_FLIGHTS,
                            "risers_per_flight": N_RISERS, "flight_slope_deg": round(ang, 2), "clear_width": 2 * HW,
                            "landing_z": [round(z, 3) for z in zs], "flight_y": [round(-d, 3) for d in ds],
                            "top": wp[0], "foot": wp[-1], "walk_path": wp, "collision_boxes": boxes}}


def self_check(verbose=True):
    """Walks walk_path and the posts: flights <= 36 deg, every post bottom under the profile and its top above it, the top at
    (0, 0, 0) +- 0.05, the foot within 0.5 m of the profile's foot (distance in y/z, the profile's plane), ramps meet landings."""
    L = layout()
    wp = walk_path(L)
    worst = 0.0
    for p, q in zip(wp[:-1], wp[1:]):
        h = math.hypot(q[0] - p[0], q[1] - p[1])
        dz = abs(q[2] - p[2])
        if dz > 1e-6:
            a = math.degrees(math.atan2(dz, h))
            worst = max(worst, a)
            assert a <= 36.0, "flight steeper than 36 deg: %s -> %s (%.1f)" % (p, q, a)
    top = wp[0]
    assert math.dist(top, (0, 0, 0)) <= 0.05, "top not at the origin: %s" % (top,)
    foot = wp[-1]
    fd = math.hypot(-foot[1] - D_FOOT, foot[2] - ground(D_FOOT))
    assert fd <= 0.5, "foot %.2f m off the profile's foot" % fd
    b, extra = cliff_stair(0)
    posts = extra["posts"]
    for (x, y, zt, zb) in posts:
        g = gy(y)
        assert zb < g - 1.0, "post at %.2f, %.2f does not reach into the ground (%.2f vs %.2f)" % (x, y, zb, g)
        assert zt > g, "post at %.2f, %.2f wholly underground" % (x, y)
    # ramps meet landings: each flight's top line ends at its landings' heights
    boxes = extra["manifest"]["collision_boxes"]
    for i in range(1, N_FLIGHTS + 1):
        bx = [q for q in boxes if q["name"] == "flight_%d" % i][0]
        a = math.radians(bx["rot_y_deg"])
        cx, cz = bx["center"][0], bx["center"][2]
        nx, nz = math.sin(a), math.cos(a)
        tx, tz = cx + nx * 0.15, cz + nz * 0.15                 # top plane centre
        dx, dz = math.cos(a), -math.sin(a)
        # the top plane at the flight's upper edge x
        dr = 1 if i % 2 else -1
        xa = -dr * XE
        zt = tz + (xa - tx) / dx * dz
        assert abs(zt - L["zs"][i - 1]) < 0.06, "flight %d ramp top %.3f vs landing %.3f" % (i, zt, L["zs"][i - 1])
        xb = dr * XE
        zb = tz + (xb - tx) / dx * dz
        assert abs(zb - L["zs"][i]) < 0.06, "flight %d ramp foot %.3f vs landing %.3f" % (i, zb, L["zs"][i])
    if verbose:
        print("cliff_stair self-check OK: %d flights, steepest walk segment %.1f deg, %d posts all 1.5 m into the profile, "
              "top %s, foot %s (%.2f m from the profile's foot)" % (N_FLIGHTS, worst, len(posts), top, foot, fd))
    b.bm.free()
    return True


# =========================================================================================================
# plank_bridge
# =========================================================================================================

PB_LEN = (6.0, 9.0)
PB_W = 1.3
PB_RAMP = 0.6
PB_DROP = 0.25


def plank_bridge(variant):
    b = Builder("plank_bridge_v%d" % variant)
    rnd = random.Random(60 + variant)
    L = PB_LEN[variant]
    a = L / 2 - PB_RAMP                                # the level deck: x -a..a
    hw = PB_W / 2
    pt = 0.05
    sd = 0.18                                          # stringer depth
    zs0 = -pt                                          # stringer top
    sill_x = a - 0.25
    trestles = [-1.1, 1.1] if variant == 0 else [-1.85, 1.85]
    rail_x = sorted(set([-sill_x, sill_x] + trestles + ([0.0] if variant == 1 else [])))
    # deck planks across Y
    n = round(2 * a / 0.17)
    pitch = 2 * a / n
    for i in range(n):
        x = -a + pitch * (i + 0.5)
        t = pt + rnd.uniform(-0.004, 0.004)
        e0, e1 = rnd.uniform(-0.03, 0.0), rnd.uniform(0.0, 0.03)
        beam(b, V(x, -hw + e0, -t / 2 + rnd.uniform(-0.004, 0)), V(x, hw + e1, -t / 2), pitch - 0.014 - rnd.uniform(0, 0.01), t,
             tile=DARK if rnd.random() < 0.1 else WOOD)
    # ramps: planks on two short sloped stringers each end, down 0.25 m to the ground
    th = math.atan2(PB_DROP, PB_RAMP)
    sl = math.hypot(PB_RAMP, PB_DROP)
    nr = 4
    for sg in (-1, 1):
        nrm = Vector((sg * math.sin(th), 0, math.cos(th)))
        for k in range(nr):
            s = (k + 0.5) / nr * PB_RAMP
            top = V(sg * (a + s), 0, -PB_DROP * s / PB_RAMP)
            c = top - nrm * (pt / 2)
            ln = hw + rnd.uniform(-0.02, 0.02)
            beam(b, c - V(0, ln, 0), c + V(0, ln, 0), sl / nr - 0.015, pt, up=nrm, tile=WOOD)
        for y in (-0.45, 0.45):
            slope_x(b, sg * (a - 0.05), sg * (a + PB_RAMP), -pt, -pt - PB_DROP, 0.14, y - 0.04, y + 0.04, WOOD)
        # a kicker log at the toe, half buried
        b.cyl(V(sg * (L / 2 - 0.06), -hw - 0.05, -PB_DROP - 0.09), V(sg * (L / 2 - 0.06), hw + 0.05, -PB_DROP - 0.09), 0.1, 0.095, seg=7,
              tile=DARK, cap_tile="endgrain", wobble=0.04)
    # stringers along X
    for y in (-0.48, 0.0, 0.48):
        beam(b, V(-a - 0.02, y, zs0 - sd / 2), V(a + 0.02, y, zs0 - sd / 2), 0.10, sd, tile=WOOD)
    # log sills on the banks
    zsill = zs0 - sd - 0.15
    for sg in (-1, 1):
        x = sg * sill_x
        b.cyl(V(x, -hw - 0.25, zsill), V(x, hw + 0.25, zsill + 0.01), 0.16, 0.15, seg=8, tile=DARK, cap_tile="endgrain", wobble=0.04)
    # trestles: post pairs from 1.6 m under the deck to the handrail, a cap beam under the stringers, X braces
    yp = hw + 0.06
    zcap = zs0 - sd - 0.07
    for x in rail_x:
        for sy in (-1, 1):
            post(b, x, sy * yp, -1.6, 0.93, 0.12, DARK)
        if x in trestles:
            beam(b, V(x, -yp - 0.12, zcap), V(x, yp + 0.12, zcap), 0.10, 0.14, tile=DARK)
            beam(b, V(x - 0.08, -yp, zcap - 0.12), V(x - 0.08, yp, -1.35), 0.035, 0.11, tile=WOOD)
            beam(b, V(x + 0.08, -yp, -1.35), V(x + 0.08, yp, zcap - 0.12), 0.035, 0.11, tile=WOOD)
    # handrails: a top board on the posts and a mid board on their outer faces
    for sy in (-1, 1):
        beam(b, V(-a - 0.12, sy * yp, 0.965), V(a + 0.12, sy * yp, 0.965), 0.10, 0.07, tile=WOOD)
        yo = sy * (yp + 0.06 + 0.02)
        beam(b, V(-sill_x - 0.1, yo, 0.5), V(sill_x + 0.1, yo, 0.5), 0.04, 0.11, tile=WOOD)
    # collision
    boxes = [cbox("deck", (0, 0, -0.15), (2 * a, PB_W + 0.2, 0.3))]
    for sg, nm in ((1, "east"), (-1, "west")):
        mid = (sg * (a + PB_RAMP / 2), -PB_DROP / 2)
        an = th * sg
        boxes.append(cbox("ramp_" + nm, (mid[0] - math.sin(an) * 0.15, 0, mid[1] - math.cos(an) * 0.15), (sl + 0.05, PB_W + 0.2, 0.3),
                          ry=math.degrees(an)))
    for sy, nm in ((1, "n"), (-1, "s")):
        boxes.append(cbox("rail_" + nm, (0, sy * yp, 0.5), (2 * a + 0.24, 0.14, 1.1)))
    notes = ("plank footbridge %.0f m over a brook 2.5..4 m wide with banks ~0.9 m high: deck along local X (\"axis\": \"x\"), origin = the "
             "middle of the deck on its walking surface (z 0), deck 1.3 m wide and level over x -%.2f..%.2f, a 0.6 m ramp at each end drops "
             "0.25 m to the ground (toes at x +-%.1f, z -0.25); stringers rest on log sills at x +-%.2f (sill tops z -0.24); post pairs at x %s "
             "reach z -1.6, carry the two handrails (top z 1.0) and, at x %s, the trestle cap beams and X braces; collision_boxes in mesh space "
             "(rot_y_deg right-handed about +Y, positive tips +X down)" % (L, a, a, L / 2, sill_x, ", ".join("%g" % x for x in rail_x),
                                                                            ", ".join("%g" % x for x in trestles)))
    return b, {"notes": notes, "manifest": {"axis": "x", "length": L, "deck_length": 2 * a, "width": PB_W, "ramp_length": PB_RAMP,
                                            "ramp_drop": PB_DROP, "post_depth": 1.6, "collision_boxes": boxes}}


# =========================================================================================================
# boardwalk
# =========================================================================================================

BW_H = 0.5           # walking surface over the ground
BW_W = 1.2
BW_HALF = 3.0
BW_PILE_X = (-2.25, -0.75, 0.75, 2.25)


def _bw_planks(b, rnd, x0, x1, y0, y1, along="y", z=BW_H, pitch_target=0.158):
    """Planks over a rectangle, their length along `along`, top at z, butt ends jittered a little."""
    t = 0.04
    if along == "y":
        n = max(1, round((x1 - x0) / pitch_target))
        p = (x1 - x0) / n
        for i in range(n):
            x = x0 + p * (i + 0.5)
            tt = t + rnd.uniform(-0.003, 0.003)
            e0, e1 = rnd.uniform(-0.025, 0.01), rnd.uniform(-0.01, 0.025)
            beam(b, V(x, y0 + e0, z - tt / 2 + rnd.uniform(-0.004, 0)), V(x, y1 + e1, z - tt / 2), p - 0.013 - rnd.uniform(0, 0.008), tt,
                 tile=DARK if rnd.random() < 0.1 else WOOD)
    else:
        n = max(1, round((y1 - y0) / pitch_target))
        p = (y1 - y0) / n
        for i in range(n):
            y = y0 + p * (i + 0.5)
            tt = t + rnd.uniform(-0.003, 0.003)
            e0, e1 = rnd.uniform(-0.025, 0.01), rnd.uniform(-0.01, 0.025)
            beam(b, V(x0 + e0, y, z - tt / 2 + rnd.uniform(-0.004, 0)), V(x1 + e1, y, z - tt / 2), p - 0.013 - rnd.uniform(0, 0.008), tt,
                 tile=DARK if rnd.random() < 0.1 else WOOD)


def _pile(b, x, y, top, r=0.075):
    b.cyl(V(x, y, -1.2), V(x, y, top), r, r * 0.92, seg=7, tile=DARK, cap_tile="endgrain", wobble=0.05)


def boardwalk(variant):
    b = Builder("boardwalk_v%d" % variant)
    rnd = random.Random(80 + variant)
    hw = BW_W / 2
    zst = BW_H - 0.04                      # stringer top
    zbt = zst - 0.15                       # bearer top
    zbb = zbt - 0.12
    boxes = []
    seats = []
    extra = {}
    if variant < 3:
        _bw_planks(b, rnd, -BW_HALF, BW_HALF, -hw, hw, "y")
        for y in (-0.42, 0.42):
            beam(b, V(-BW_HALF + 0.003, y, zst - 0.075), V(BW_HALF - 0.003, y, zst - 0.075), 0.08, 0.15, tile=WOOD)
        for x in BW_PILE_X:
            beam(b, V(x, -0.7, (zbt + zbb) / 2), V(x, 0.7, (zbt + zbb) / 2), 0.10, 0.12, tile=DARK)
            for y in (-0.6, 0.6):
                _pile(b, x, y, zbt - 0.01)
        boxes.append(cbox("deck", (0, 0, BW_H - 0.15), (2 * BW_HALF, BW_W, 0.3)))
        if variant == 1:
            yr = hw + 0.07
            for x in BW_PILE_X:
                post(b, x, yr, -1.2, BW_H + 0.93, 0.1, DARK)
            beam(b, V(-BW_HALF + 0.002, yr, BW_H + 0.965), V(BW_HALF - 0.002, yr, BW_H + 0.965), 0.09, 0.07, tile=WOOD)
            beam(b, V(-BW_HALF + 0.002, yr + 0.07, BW_H + 0.5), V(BW_HALF - 0.002, yr + 0.07, BW_H + 0.5), 0.04, 0.1, tile=WOOD)
            boxes.append(cbox("rail", (0, yr, BW_H + 0.5), (2 * BW_HALF, 0.12, 1.1)))
        if variant == 2:
            bx0, bx1, by1 = -1.1, 1.1, -hw - 2.2
            _bw_planks(b, rnd, bx0, bx1, by1, -hw - 0.006, "x")
            for x in (-0.85, 0.0, 0.85):
                beam(b, V(x, -0.47, zst - 0.075), V(x, by1 + 0.03, zst - 0.075), 0.08, 0.15, tile=WOOD)
            for y in (-1.55, by1 + 0.12):
                beam(b, V(-1.06, y, (zbt + zbb) / 2), V(1.06, y, (zbt + zbb) / 2), 0.10, 0.12, tile=DARK)
                for x in (-0.85, 0.85):
                    _pile(b, x + 0.0, y, zbt - 0.01)
            # rails round the bay's three open sides
            pr = [(-1.04, -hw - 0.06), (-1.04, -1.55), (-1.04, by1 + 0.06), (0.0, by1 + 0.06), (1.04, by1 + 0.06), (1.04, -1.55),
                  (1.04, -hw - 0.06)]
            for x, y in pr:
                post(b, x, y, -1.2, BW_H + 0.93, 0.1, DARK)
            for (p, q) in ((pr[0], pr[2]), (pr[2], pr[4]), (pr[4], pr[6])):
                beam(b, V(p[0], p[1], BW_H + 0.965), V(q[0], q[1], BW_H + 0.965), 0.09, 0.07, tile=WOOD)
                al = (Vector(q) - Vector(p)).normalized()
                o = Vector((-al.y, al.x)) * 0.07
                cen = Vector((0, -1.7))
                if (Vector(p) + o - cen).length < (Vector(p) - cen).length:
                    o = -o
                beam(b, V(p[0] + o.x, p[1] + o.y, BW_H + 0.5), V(q[0] + o.x, q[1] + o.y, BW_H + 0.5), 0.04, 0.1, tile=WOOD)
            fac = Vector((0, -1, 0))
            c = V(0.0, -1.45, BW_H)
            for p in bench(b, c, (1, 0, 0), fac, L=1.4, seat_h=0.42, back=False, rnd=rnd):
                seats.append([round(p.x, 3), round(p.y, 3), round(p.z, 3), yaw_of(fac)])
            boxes.append(cbox("bay", (0, (by1 - hw) / 2, BW_H - 0.15), (2.2, 2.2, 0.3)))
            boxes.append(cbox("bay_rail_w", (-1.04, (by1 - hw) / 2, BW_H + 0.5), (0.12, 2.2, 1.1)))
            boxes.append(cbox("bay_rail_e", (1.04, (by1 - hw) / 2, BW_H + 0.5), (0.12, 2.2, 1.1)))
            boxes.append(cbox("bay_rail_s", (0, by1 + 0.06, BW_H + 0.5), (2.2, 0.12, 1.1)))
            boxes.append(cbox("bay_bench", (0, -1.45, BW_H + 0.22), (1.44, 0.46, 0.44)))
            extra["bay"] = {"x": [bx0, bx1], "y": [round(by1, 3), -hw]}
        names = ("straight 6 m, plain", "straight 6 m, a handrail on +Y (top z %.2f)" % (BW_H + 1.0),
                 "6 m with a 2.2 x 2.2 m viewing bay on -Y (x -1.1..1.1, y -0.6..-2.8, rails round it) and a low backless bench "
                 "facing -Y (seat z %.2f)" % (BW_H + 0.42))
        desc = names[variant]
        ends = "ends at x -3.0 / +3.0 exactly with the same cross-section, so pieces butt end to end (piles at x %s keep a 1.5 m rhythm across joints)" % \
               ", ".join("%g" % x for x in BW_PILE_X)
    else:
        # end ramp: deck height at x -1.5 (joins a piece's end) down to the ground at x +1.5
        x0, x1 = -1.5, 1.5
        zt0, zt1 = BW_H, 0.02
        th = math.atan2(zt0 - zt1, x1 - x0)
        sl = math.hypot(x1 - x0, zt0 - zt1)
        nrm = Vector((math.sin(th), 0, math.cos(th)))
        n = round(sl / 0.158)
        pz = lambda x: zt0 + (zt1 - zt0) * (x - x0) / (x1 - x0)
        for i in range(n):
            s = (i + 0.5) / n * sl
            x = x0 + s * math.cos(th)
            top = V(x, rnd.uniform(-0.01, 0.01), pz(x))
            c = top - nrm * 0.02
            beam(b, c - V(0, hw + rnd.uniform(-0.02, 0.01), 0), c + V(0, hw + rnd.uniform(-0.01, 0.02), 0), sl / n - 0.014, 0.04 + rnd.uniform(-0.003, 0.003),
                 up=nrm, tile=DARK if rnd.random() < 0.1 else WOOD)
        for y in (-0.42, 0.42):
            slope_x(b, x0 + 0.003, x1 - 0.05, zst, pz(x1 - 0.05) - 0.04, 0.15, y - 0.04, y + 0.04, WOOD)
        xb = -0.75
        zb = pz(xb) - 0.04 - 0.15
        beam(b, V(xb, -0.7, zb - 0.06), V(xb, 0.7, zb - 0.06), 0.10, 0.12, tile=DARK)
        for y in (-0.6, 0.6):
            _pile(b, xb, y, zb - 0.13)
        b.cyl(V(0.85, -0.75, -0.04), V(0.85, 0.75, -0.03), 0.13, 0.125, seg=8, tile=DARK, cap_tile="endgrain", wobble=0.04)
        ang = math.degrees(th)
        mx, mz = 0.0, (zt0 + zt1) / 2
        boxes.append(cbox("ramp", (mx - nrm.x * 0.15, 0, mz - nrm.z * 0.15), (sl + 0.1, BW_W, 0.3), ry=ang))
        desc = "end ramp 3 m long: from deck height (z 0.5) at x -1.5 down to the ground (z 0.02) at x +1.5, %.1f deg" % ang
        ends = ("its high end at x -1.5 butts a piece's end (same cross-section): to continue a walk that ends at a piece's +X end, place the "
                "ramp 4.5 m further along with the same yaw (yaw + 180 for a -X end)")
        extra["join_x"] = -1.5
    notes = ("reedbed boardwalk (мостки) piece, %s: deck along local X (\"axis\": \"x\"), 1.2 m wide, walking surface z %.2f over the ground, "
             "origin = on the ground under the middle; piles reach z -1.2; %s; collision_boxes in mesh space (rot_y_deg right-handed about "
             "+Y, positive tips +X down)" % (desc, BW_H, ends))
    man = {"axis": "x", "width": BW_W, "deck_z": BW_H, "length": 6.0 if variant < 3 else 3.0, "pile_depth": 1.2, "collision_boxes": boxes}
    man.update(extra)
    return b, {"notes": notes, "seats": seats, "manifest": man}


KINDS = {
    "cliff_stair": (cliff_stair, {"variants": [0], "collision": "box"}),
    "plank_bridge": (plank_bridge, {"variants": [0, 1], "collision": "box"}),
    "boardwalk": (boardwalk, {"variants": [0, 1, 2, 3], "collision": "box"}),
}


if __name__ == "__main__":
    self_check()
