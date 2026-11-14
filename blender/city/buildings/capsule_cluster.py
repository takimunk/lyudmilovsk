"""Capsule cluster: three short hollow concrete service cores (two square, one octagonal, 4-5 storeys) joined by open
steel bridges, with prefab living capsules plugged onto the cores' faces, tilted in plan in 45 degree steps. Every
capsule (or pair of capsules = one flat) is one flat with one big round or rounded-square window. An original low-rise
design in the spirit of Japanese Metabolism (Kurokawa, Kikutake); not a copy of any building. Unlike capsule_tower
(two cores, 13 storeys, one capsule type) this is a horizontal cluster, with enterable stair cores.

Plot 40 x 30 m, height_max 18 (roofs 13.2 / 16.5, hatches to 17.4).
Layout (x along the street, y inland): cores A (square, 5 storeys) x -11.8, B (octagon, 4 storeys) x 0,
C (square, 5 storeys) x +11.8, all at y = 14. Bridges A-B and B-C at levels 1 and 3.
Each core is a hollow shell (facade walls with reveals, solid slabs, a U-shaped concrete stair with 0.16 risers)
with a ground door on the street face (entrances A, B, C), stair windows on the faces that carry no capsule, and
bridge doors. Capsule doors are drawn on the cores' stair landings only as flat records (the capsules are solid pods).
"""
import math
import random

import furn_int3 as fu
import interior_kit as kit
from citykit import box, prism, DOOR_W, DOOR_H
from _surrounds import add_surrounds

INFO = {
    "name_ru": "Капсульный кластер", "name_en": "Capsule Cluster",
    "style": "metabolism", "use": "residential",
    "inspired_by": "Japanese Metabolism of the 1960s-70s (Kurokawa, Kikutake): prefab capsules plugged onto concrete "
                   "service cores linked by bridges, here as a low-rise horizontal cluster",
}

SEED = 1970
STOREY = 3.3
WALL = 0.3
CAP_W, CAP_D, CAP_H = 2.3, 3.3, 2.45
WIN_R = 0.55
SKINS = ("CT_Aluminium", "CT_Aluminium", "CT_Aluminium", "CT_SteelWhite", "CT_SteelWhite", "CT_SteelWhite",
         "CT_PlasterYellow", "CT_PlasterYellow", "CT_SteelRed", "CT_SteelRed")
VARIANTS = {}
STUDIO = "2-01"              # the flat that is opened as a furnished studio (a pair of capsules, tilt 0)

CY = 14.0
CORES = [
    dict(id="A", kind="sq", c=(-11.8, CY), a=2.7, n=5),
    dict(id="B", kind="oct", c=(0.0, CY), a=3.3, n=4),
    dict(id="C", kind="sq", c=(11.8, CY), a=2.8, n=5),
]
BRIDGE_LEVELS = (1, 3)


# ------------------------------------------------------------------ geometry helpers
def _poly(core):
    cx, cy = core["c"]
    a = core["a"]
    if core["kind"] == "sq":
        return [(cx - a, cy - a), (cx + a, cy - a), (cx + a, cy + a), (cx - a, cy + a)]
    R = a / math.cos(math.radians(22.5))
    return [(cx + R * math.cos(math.radians(-22.5 + 45 * k)), cy + R * math.sin(math.radians(-22.5 + 45 * k)))
            for k in range(8)]


def _edges(poly):
    out = []
    n = len(poly)
    for i in range(n):
        p0, p1 = poly[i], poly[(i + 1) % n]
        dx, dy = p1[0] - p0[0], p1[1] - p0[1]
        L = math.hypot(dx, dy)
        yaw = math.degrees(math.atan2(dy, dx)) % 360.0
        out.append(dict(p0=p0, p1=p1, L=L, yaw=round(yaw, 3) % 360.0, mid=((p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2)))
    return out


def _clip(poly, x0, x1, y0, y1):
    """Sutherland-Hodgman clip of a convex polygon to a rectangle."""
    def clip(pts, inside, inter):
        out = []
        for i in range(len(pts)):
            a, b = pts[i], pts[(i + 1) % len(pts)]
            ia, ib = inside(a), inside(b)
            if ia:
                out.append(a)
            if ia != ib:
                out.append(inter(a, b))
        return out
    pts = list(poly)
    for ins, inter in (
        (lambda p: p[0] >= x0, lambda a, b: (x0, a[1] + (b[1] - a[1]) * (x0 - a[0]) / (b[0] - a[0]))),
        (lambda p: p[0] <= x1, lambda a, b: (x1, a[1] + (b[1] - a[1]) * (x1 - a[0]) / (b[0] - a[0]))),
        (lambda p: p[1] >= y0, lambda a, b: (a[0] + (b[0] - a[0]) * (y0 - a[1]) / (b[1] - a[1]), y0)),
        (lambda p: p[1] <= y1, lambda a, b: (a[0] + (b[0] - a[0]) * (y1 - a[1]) / (b[1] - a[1]), y1)),
    ):
        if not pts:
            break
        pts = clip(pts, ins, inter)
    if len(pts) < 3:
        return []
    area = sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1] for i in range(len(pts)))
    return pts if abs(area) > 0.02 else []


def _sat(p, q, margin=0.0):
    """Do the convex polygons p and q overlap (closer than margin)?"""
    for poly in (p, q):
        n = len(poly)
        for i in range(n):
            ex, ey = poly[(i + 1) % n][0] - poly[i][0], poly[(i + 1) % n][1] - poly[i][1]
            L = math.hypot(ex, ey) or 1.0
            nx, ny = -ey / L, ex / L
            a = [x * nx + y * ny for x, y in p]
            b = [x * nx + y * ny for x, y in q]
            if max(a) + margin <= min(b) or max(b) + margin <= min(a):
                return False
    return True


def _rect(x0, x1, y0, y1):
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def _quad_out(m, mat, pts, out):
    """A quad that faces `out` whatever the order of pts."""
    p0, p1, p2 = pts[0], pts[1], pts[2]
    u = (p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2])
    v = (p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2])
    n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
    if n[0] * out[0] + n[1] * out[1] + n[2] * out[2] < 0:
        pts = pts[::-1]
    m.face(mat, list(pts))


# ------------------------------------------------------------------ cores
def _core_shell(B, core, holes_by_edge, wins_by_edge):
    poly = _poly(core)
    edges = core["edges"]
    top = core["n"] * STOREY
    for i, e in enumerate(edges):
        F = B.facade(e["p0"], e["p1"], 0.0, top - WALL, "CT_Concrete", holes=holes_by_edge.get(i, []), depth=WALL,
                     reveal_mat="CT_ConcreteDark")
        for (u0, u1, h0, h1) in wins_by_edge.get(i, []):
            B.facade_window(F, u0, u1, h0, h1, kind="stair", inset=0.18)
    cx, cy = core["c"]
    k = (core["a"] - WALL) / core["a"]
    inner = [(cx + (x - cx) * k, cy + (y - cy) * k) for x, y in poly]
    core["inner"] = inner
    # ground floor
    prism(B.m, "CT_Floor", inner, -0.3, 0.0, top=True, bottom=False)
    # upper floors with the stair well cut out
    for lv in range(1, core["n"]):
        zf = lv * STOREY
        for (x0, x1, y0, y1) in ((cx + 0.1, cx + 9, cy - 9, cy + 9), (cx - 9, cx - 2.1, cy - 9, cy + 9),
                                 (cx - 2.1, cx + 0.1, cy - 9, cy - 1.5), (cx - 2.1, cx + 0.1, cy + 2.1, cy + 9)):
            piece = _clip(inner, x0, x1, y0, y1)
            if piece:
                prism(B.m, "CT_Concrete", piece, zf - WALL, zf, top=True, bottom=True)
    # roof: concrete slab, membrane on top, a hatch and a vent
    prism(B.m, "CT_Roof", poly, top - WALL, top, top=True, bottom=False, side_mat="CT_Concrete")
    B.m.face("CT_ConcreteDark", [(x, y, top - WALL) for x, y in reversed(poly)])
    # parapet ring (a low concrete kerb in the detail mesh)
    ro = [(cx + (x - cx) * 0.93, cy + (y - cy) * 0.93) for x, y in poly]
    ri = [(cx + (x - cx) * 0.88, cy + (y - cy) * 0.88) for x, y in poly]
    for i in range(len(ro)):
        a0, a1 = ro[i], ro[(i + 1) % len(ro)]
        b0, b1 = ri[i], ri[(i + 1) % len(ri)]
        B.detail.face("CT_ConcreteDark", [(a0[0], a0[1], top + 0.05), (a1[0], a1[1], top + 0.05), (b1[0], b1[1], top + 0.05),
                                           (b0[0], b0[1], top + 0.05)][::-1])
    box(B.m, "CT_ConcreteDark", cx - 1.3, cx + 1.3, cy + 0.2, cy + 2.0, top, top + 0.9)           # stair head house
    box(B.detail, "CT_Steel", cx + 1.4, cx + 1.8, cy - 1.8, cy - 1.4, top, top + 0.7)
    fu.cyl(B.detail, "CT_Steel", cx + 1.6, cy - 1.6, top + 0.7, top + 1.0, 0.2, 8)
    return inner


def _core_stairs(B, core):
    cx, cy = core["c"]
    for s in range(0, core["n"] - 1):
        z = s * STOREY
        r = STOREY / 20.0
        for i in range(10):                                      # flight 1: x -2.1..-1.1 climbing +y
            ya = cy - 1.5 + 0.3 * i
            box(B.m, "CT_Concrete", cx - 2.1, cx - 1.1, ya, ya + 0.3, z + r * (i + 1) - 0.3, z + r * (i + 1))
        box(B.m, "CT_Concrete", cx - 2.1, cx + 0.1, cy + 1.5, cy + 2.1, z + 10 * r - 0.3, z + 10 * r)   # mid landing
        for j in range(10):                                      # flight 2: x -0.9..0.1 climbing -y
            yb = cy + 1.5 - 0.3 * j
            box(B.m, "CT_Concrete", cx - 0.9, cx + 0.1, yb - 0.3, yb, z + (10 + j + 1) * r - 0.3, z + (10 + j + 1) * r)
        # rail along the well
        B.railing([(cx - 1.05, cy + 1.45), (cx - 1.05, cy - 1.45)], z + 1.6, h=0.95, post=1.5)


# ------------------------------------------------------------------ capsules
def _capsule(B, rnd, fr, tilt, mat, zf, off, fid, shape, wmat, body=True):
    """One capsule on face frame fr (origin on the wall plane, w out): tilt degrees in plan; returns its 2D polygon."""
    fr_t = fu.Fr(fr.x, fr.y, fr.yaw + tilt, 0.0)
    # push the capsule out until its back corners clear the wall
    out = (fr.s, -fr.c)
    w0 = 0.12
    for _ in range(40):
        f2 = fr.sub(0, w0, tilt)
        m = min((f2.p(u, 0.0)[0] - fr.x) * out[0] + (f2.p(u, 0.0)[1] - fr.y) * out[1] for u in (-CAP_W / 2, CAP_W / 2))
        if m >= 0.1:
            break
        w0 += 0.1
    f2 = fr.sub(0, w0, tilt)
    z0 = zf + off - 0.1
    z1 = z0 + CAP_H
    if body:                                                 # (the hollow studio builds its own walls)
        fu.lbox(B.m, "CT_ConcreteDark", fr, -0.5, 0.5, -0.02, w0 + 0.55, zf, min(zf + 2.1, z1 - 0.1))      # neck
        fu.lbox(B.m, mat, f2, -CAP_W / 2, CAP_W / 2, 0.0, CAP_D, z0, z1)
    # skirt, roof vent
    fu.lbox(B.detail, "CT_ConcreteDark", f2, -CAP_W / 2 - 0.03, CAP_W / 2 + 0.03, 0.0, CAP_D + 0.03, z0 - 0.0, z0 + 0.14,
            faces="xXyYz")
    fu.lbox(B.detail, "CT_Steel", f2, -0.3, 0.3, 0.7, 1.2, z1, z1 + 0.22, faces="xXyYZ")
    # window on the end face
    zc = (z0 + z1) / 2 + 0.05
    D = CAP_D
    onx = (f2.s, -f2.c, 0.0)
    r = WIN_R
    if shape == "round":
        corners = [f2.p(-r, D, zc - r), f2.p(r, D, zc - r), f2.p(r, D, zc + r), f2.p(-r, D, zc + r)]
        B.window(corners, flat=fid, kind="flat", inset=-0.03, frame=0, shape="round")
        segs = 16
        for k in range(segs):
            a0, a1 = 2 * math.pi * k / segs, 2 * math.pi * (k + 1) / segs
            P = lambda rr, a: f2.p(rr * math.cos(a), D + 0.05, zc + rr * math.sin(a))
            _quad_out(B.detail, wmat, [P(r - 0.02, a0), P(r - 0.02, a1), P(r + 0.13, a1), P(r + 0.13, a0)], onx)
    else:
        r = WIN_R * 0.95
        corners = [f2.p(-r, D, zc - r), f2.p(r, D, zc - r), f2.p(r, D, zc + r), f2.p(-r, D, zc + r)]
        B.window(corners, flat=fid, kind="flat", inset=-0.03, frame=0.08, shape="rect")
        c = 0.26                                             # chamfered corners: dark triangles over the card
        for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            tri = [f2.p(su * r, D + 0.05, zc + sv * (r - c)), f2.p(su * (r - c), D + 0.05, zc + sv * r),
                   f2.p(su * r, D + 0.05, zc + sv * r)]
            B.detail.face(wmat, tri if _tri_faces(tri, onx) else tri[::-1])
        # outer rounded-square bezel: a frame of four strips
        for (a0, a1, b0, b1) in ((-r - 0.1, r + 0.1, r, r + 0.1), (-r - 0.1, r + 0.1, -r - 0.1, -r),
                                 (-r - 0.1, -r, -r, r), (r, r + 0.1, -r, r)):
            _quad_out(B.detail, wmat, [f2.p(a0, D + 0.05, zc + b0), f2.p(a1, D + 0.05, zc + b0),
                                       f2.p(a1, D + 0.05, zc + b1), f2.p(a0, D + 0.05, zc + b1)], onx)
    poly = [f2.p(-CAP_W / 2, 0.0)[:2], f2.p(CAP_W / 2, 0.0)[:2], f2.p(CAP_W / 2, CAP_D)[:2], f2.p(-CAP_W / 2, CAP_D)[:2]]
    return poly, f2


def _tri_faces(tri, out):
    u = (tri[1][0] - tri[0][0], tri[1][1] - tri[0][1], tri[1][2] - tri[0][2])
    v = (tri[2][0] - tri[0][0], tri[2][1] - tri[0][1], tri[2][2] - tri[0][2])
    n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
    return n[0] * out[0] + n[1] * out[1] + n[2] * out[2] > 0


def _bridge(B, xa, xb, yc, zf):
    box(B.m, "CT_Steel", xa - 0.05, xb + 0.05, yc - 0.9, yc + 0.9, zf - 0.28, zf)
    for y in (yc - 0.8, yc + 0.8):
        B.railing([(xa, y), (xb, y)], zf, h=1.1, post=1.0)
        n = max(2, int((xb - xa) / 1.1))
        for i in range(n):
            xs, xe = xa + (xb - xa) * i / n, xa + (xb - xa) * (i + 1) / n
            a, b = ((xs, zf), (xe, zf + 1.05)) if i % 2 == 0 else ((xs, zf + 1.05), (xe, zf))
            fu.rod(B.detail, "CT_Steel", (a[0], y, a[1]), (b[0], y, b[1]), 0.05)
    for x in (xa + 0.4, (xa + xb) / 2, xb - 0.4):                  # cross beams under the deck
        box(B.detail, "CT_Steel", x - 0.05, x + 0.05, yc - 1.0, yc + 1.0, zf - 0.42, zf - 0.28)



# ------------------------------------------------------------------ depth pass: small props
def _lantern(B, x, y, z):
    """A bridge lantern post with a weatherproof switch box on its street side; the one real lamp of the bridge."""
    p = kit._P(B, x, y, z, 0.0, "lantern")
    p.kind = "prop"
    p.cyl("CT_Steel", 0.0, 0.0, 0.0, 0.03, 0.1, 8)
    p.cyl("CT_Steel", 0.0, 0.0, 0.03, 1.5, 0.022, 6)
    p.lathe("CT_Opal", 0.0, 0.0, [(0.0, 1.48), (0.09, 1.55), (0.1, 1.66), (0.07, 1.76), (0.0, 1.78)], 8, top=False)
    p.cyl("CT_Steel", 0.0, 0.0, 1.78, 1.82, 0.11, 8)
    p.box("CT_SteelWhite", -0.06, 0.06, -0.12, -0.02, 0.88, 1.04)
    p.box("CT_Rubber", -0.012, 0.012, -0.135, -0.12, 0.93, 1.0)
    B.lamp((x, y, z + 1.64), lumens=240, colour="#FFE2B8", kind="stair", radius=4.2, fixture=False)
    return p.done(footprint=(-0.1, 0.1, -0.12, 0.1))


def _chime(B, x, y, z, yaw):
    """A wind chime on an arm over the deck (the arm hangs out of the post toward +local y)."""
    p = kit._P(B, x, y, z, yaw, "chime")
    p.kind = "prop"
    p.rod("CT_Steel", (0.0, 0.0, 1.1), (0.0, 0.0, 2.3), 0.025, 4)
    p.rod("CT_Steel", (0.0, 0.0, 2.3), (0.0, 0.5, 2.3), 0.02, 4)
    p.cyl("CT_Brass", 0.0, 0.5, 2.2, 2.3, 0.1, 8)
    for k, (dx, dy, ln) in enumerate(((-0.07, 0.43, 0.34), (0.0, 0.57, 0.42), (0.07, 0.43, 0.3), (-0.03, 0.5, 0.26))):
        p.rod("CT_Rubber", (dx, dy, 2.2), (dx, dy, 2.2 - 0.12), 0.004, 3)
        p.cyl("CT_Aluminium", dx, dy, 2.08 - ln, 2.08, 0.014, 6)
    p.cyl("CT_Wood", 0.0, 0.5, 1.7, 1.74, 0.035, 6)
    return p.done(footprint=(-0.1, 0.1, -0.05, 0.6))


def _vane(B, x, y, z, yaw):
    """A weather vane on a mast over the rail post: a crossbar with four letters' worth of tips and an arrow."""
    p = kit._P(B, x, y, z, yaw, "vane")
    p.kind = "prop"
    p.rod("CT_Steel", (0.0, 0.0, 1.0), (0.0, 0.0, 2.05), 0.02, 4)
    p.rod("CT_Steel", (-0.22, 0.0, 1.8), (0.22, 0.0, 1.8), 0.012, 4)
    p.rod("CT_Steel", (0.0, -0.22, 1.8), (0.0, 0.22, 1.8), 0.012, 4)
    for sx, sy in ((-0.22, 0.0), (0.22, 0.0), (0.0, -0.22), (0.0, 0.22)):
        p.cyl("CT_Brass", sx, sy, 1.78, 1.82, 0.02, 5)
    p.rod("CT_SteelRed", (-0.3, 0.0, 2.05), (0.34, 0.0, 2.05), 0.014, 4)
    p.rod("CT_SteelRed", (0.46, 0.0, 2.05), (0.34, 0.0, 2.13), 0.012, 4)
    p.rod("CT_SteelRed", (0.46, 0.0, 2.05), (0.34, 0.0, 1.97), 0.012, 4)
    p.box("CT_SteelRed", -0.34, -0.2, -0.004, 0.004, 1.96, 2.16)
    return p.done(footprint=(-0.1, 0.1, -0.1, 0.1))


def _morse_key(B, x, y, z, yaw):
    """A straight Morse key on a wooden base (operator side toward -y of the piece)."""
    p = kit._P(B, x, y, z, yaw, "morse_key")
    p.kind = "prop"
    p.bev("CT_Wood", -0.07, 0.07, -0.09, 0.09, 0.0, 0.025, 0.004)
    p.box("CT_Brass", -0.012, 0.012, 0.02, 0.07, 0.025, 0.06)
    p.rod("CT_Brass", (0.0, 0.07, 0.065), (0.0, -0.1, 0.075), 0.007, 4)
    p.cyl("CT_Lacquer", 0.0, -0.095, 0.07, 0.115, 0.022, 8)
    p.cyl("CT_Brass", 0.0, -0.04, 0.025, 0.04, 0.012, 6)
    return p.done(top=0.04, footprint=(-0.07, 0.07, -0.1, 0.09))


def _logbook(B, x, y, z, yaw):
    """An open log book lying on the desk."""
    p = kit._P(B, x, y, z, yaw, "logbook")
    p.kind = "prop"
    p.box("CT_Leather", -0.17, 0.17, -0.12, 0.12, 0.0, 0.012)
    p.box("CT_Paper", -0.16, -0.005, -0.11, 0.11, 0.012, 0.024)
    p.box("CT_Paper", 0.005, 0.16, -0.11, 0.11, 0.012, 0.024)
    p.rod("CT_SteelRed", (0.1, -0.1, 0.03), (0.2, -0.04, 0.035), 0.005, 4)
    return p.done(top=0.03, footprint=(-0.17, 0.2, -0.12, 0.12))


def _tuner(B, x, y, z, yaw):
    """An antenna tuner: a grey box with two big knobs, a needle meter and a coax lead."""
    p = kit._P(B, x, y, z, yaw, "tuner")
    p.kind = "prop"
    p.bev("CT_SteelWhite", -0.16, 0.16, -0.1, 0.1, 0.0, 0.12, 0.008)
    p.box("CT_Rubber", -0.12, 0.0, -0.103, -0.1, 0.06, 0.1)
    p.rod("CT_SteelRed", (-0.06, -0.106, 0.065), (-0.03, -0.106, 0.09), 0.003, 3)
    for kx in (0.06, 0.12):
        p.cyl("CT_Lacquer", kx, -0.115, 0.04, 0.06, 0.016, 8)
    p.tube("CT_Rubber", [(0.16, 0.0, 0.08), (0.24, 0.02, 0.06), (0.3, 0.04, 0.0)], 0.007, 3)
    return p.done(top=0.12, footprint=(-0.16, 0.3, -0.1, 0.1))


def _headphones(B, x, y, z, yaw):
    """Headphones lying on the shelf: a band and two cups."""
    p = kit._P(B, x, y, z, yaw, "headphones")
    p.kind = "prop"
    pts = [(-0.085 * math.cos(math.pi * k / 8), 0.0, 0.02 + 0.09 * math.sin(math.pi * k / 8)) for k in range(9)]
    p.tube("CT_Lacquer", pts, 0.008, 3)
    for sx in (-0.085, 0.085):
        p.cyl("CT_Leather", sx, 0.0, 0.0, 0.035, 0.036, 8)
    return p.done(footprint=(-0.12, 0.12, -0.04, 0.04))


def _switch_plate(B, x, y, z, yaw):
    p = kit._P(B, x, y, z, yaw, "switch")
    p.kind = "prop"
    p.box("CT_SteelWhite", -0.04, 0.04, -0.012, 0.0, 0.0, 0.12)
    p.box("CT_Rubber", -0.012, 0.012, -0.02, -0.012, 0.03, 0.08)
    return p.done(footprint=(-0.04, 0.04, -0.02, 0.0))


def _pins(B, x, y, z, yaw):
    """Pins on the wall map (a wall piece: x across the map, -y out of the wall)."""
    p = kit._P(B, x, y, z, yaw, "pins")
    p.kind = "wall"
    mats = ("CT_SteelRed", "CT_PlasterYellow", "CT_PlasterBlue", "CT_SteelRed", "CT_PlasterYellow", "CT_PlasterBlue", "CT_SteelRed")
    for k, (px, pz) in enumerate(((-0.3, 0.1), (-0.12, 0.2), (0.0, 0.05), (0.18, 0.1), (0.3, -0.12), (-0.38, -0.08), (0.05, -0.2))):
        p.rod(mats[k], (px, -0.004, pz), (px, -0.026, pz), 0.012, 4)
    return p.done(footprint=(-0.4, 0.4, -0.03, 0.0))


def _kettle(B, x, y, z, yaw):
    """An enamel kettle on the kitchen unit."""
    p = kit._P(B, x, y, z, yaw, "kettle")
    p.kind = "prop"
    p.lathe("CT_PlasterBlue", 0.0, 0.0, [(0.0, 0.0), (0.1, 0.0), (0.12, 0.08), (0.1, 0.17), (0.05, 0.21), (0.0, 0.215)], 10)
    p.cyl("CT_Steel", 0.0, 0.0, 0.215, 0.235, 0.02, 6)
    p.rod("CT_Steel", (0.1, 0.0, 0.1), (0.17, 0.0, 0.16), 0.012, 4)
    p.tube("CT_Steel", [(-0.09, 0.0, 0.2), (0.0, 0.0, 0.3), (0.09, 0.0, 0.2)], 0.008, 3)
    return p.done(top=0.24, footprint=(-0.12, 0.17, -0.12, 0.12))


def _bridge_things(B, xa, xb, yc, lv, tag, extra=None):
    """Furniture of one bridge: a bench and a planter on the north rail, the lantern with its switch between them, a
    view point on the south rail (and a wind chime or a weather vane); the things are named br_<tag>_*."""
    zf = lv * STOREY
    xm = (xa + xb) / 2.0
    yn, ys = yc + 0.55, yc - 0.8
    bn = kit.bench(B, xa + 1.45, yn, zf, 0.0, w=1.1, kind="park", mat=kit.WOOD)
    s = bn["seats"][0]
    B.thing("br_%s_bench" % tag, "seat", (s[0], s[1], s[2]), s[3], height=round(s[2] - zf, 2))
    B.spot("br_%s_bench" % tag, "bench", (s[0], s[1], zf), s[3])
    kit.planter(B, xb - 1.25, yn, zf, 0.0, w=0.9, d=0.4, h=0.46, plants=3, seed=lv * 7 + int(xa))
    B.thing("br_%s_planter" % tag, "board", (xb - 1.25, yn, zf + 0.8), 0.0, topic="garden")
    _lantern(B, xm, yc + 0.72, zf)
    B.thing("br_%s_switch" % tag, "machine", (xm, yc + 0.72 - 0.14, zf + 0.96), 0.0, what="switch", switch=5)
    B.thing("br_%s_view" % tag, "board", (xm - 0.4, ys, zf + 1.45), 180.0, topic="view")
    B.spot("br_%s_view" % tag, "stand", (xm - 0.4, ys + 0.6, zf), 180.0)
    if extra == "chime":
        _chime(B, xa + 1.5, ys, zf, 180.0)
        B.thing("br_%s_chime" % tag, "board", (xa + 1.5, ys + 0.3, zf + 1.85), 180.0, topic="chime")
    elif extra == "vane":
        _vane(B, xb - 1.45, ys, zf, 0.0)
        B.thing("br_%s_vane" % tag, "board", (xb - 1.45, ys, zf + 1.95), 180.0, topic="vane")


# ------------------------------------------------------------------ interiors
def _bike(B, x, y, yaw=0.0, mat="CT_SteelRed"):
    """A leaning city bicycle (local: length along y, front wheel toward -y)."""
    p = kit._P(B, x, y, 0.0, yaw, "bicycle")
    p.kind = "prop"
    R = 0.33
    for yc in (-0.55, 0.55):
        ring_pts = [(0.0, yc + R * math.cos(2 * math.pi * k / 14), R + R * math.sin(2 * math.pi * k / 14)) for k in range(15)]
        p.tube("CT_Rubber", ring_pts, 0.04, 4)
    p.rod(mat, (0.0, 0.55, R), (0.0, 0.05, 0.62), 0.03, 4)
    p.rod(mat, (0.0, 0.05, 0.62), (0.0, -0.4, 0.66), 0.03, 4)
    p.rod(mat, (0.0, 0.55, R), (0.0, 0.0, 0.3), 0.03, 4)
    p.rod(mat, (0.0, 0.0, 0.3), (0.0, 0.05, 0.62), 0.03, 4)
    p.rod(mat, (0.0, -0.4, 0.66), (0.0, -0.55, R), 0.03, 4)
    p.rod(kit.STEEL, (0.0, -0.42, 0.62), (0.0, -0.42, 0.98), 0.025, 4)
    p.rod(kit.STEEL, (-0.28, -0.42, 0.99), (0.28, -0.42, 0.99), 0.025, 4)
    p.rod(kit.LAQ, (0.0, 0.1, 0.62), (0.0, 0.1, 0.92), 0.025, 4)
    p.box(kit.LAQ, -0.06, 0.06, 0.02, 0.24, 0.92, 0.96)
    return p.done()


def _pram(B, x, y, yaw=0.0, mat="CT_PlasterBlue"):
    """A pram: a hooded body on a chassis with four wheels and a handle (local: handle toward +y)."""
    p = kit._P(B, x, y, 0.0, yaw, "pram")
    p.kind = "prop"
    for sx in (-0.26, 0.26):
        for yc in (-0.34, 0.34):
            pts = [(sx, yc + 0.14 * math.cos(2 * math.pi * k / 10), 0.14 + 0.14 * math.sin(2 * math.pi * k / 10)) for k in range(11)]
            p.tube("CT_Rubber", pts, 0.035, 4)
            p.rod(kit.ALU, (sx, yc, 0.14), (sx * 0.8, yc, 0.4), 0.02, 4)
    p.bev(mat, -0.27, 0.27, -0.46, 0.46, 0.4, 0.62, 0.03, 0.04)
    p.cushion("CT_TileWhite", -0.24, 0.24, -0.4, 0.36, 0.6, 0.66, 0.02)
    p.cushion(mat, -0.27, 0.27, -0.46, -0.04, 0.62, 0.98, 0.04)
    p.tube(kit.STEEL, [(-0.25, 0.42, 0.6), (-0.25, 0.62, 0.95), (0.25, 0.62, 0.95), (0.25, 0.42, 0.6)], 0.025, 4)
    return p.done()


def _seat_thing(B, tid, r, i=0):
    s = r["seats"][i]
    B.thing(tid, "seat", (s[0], s[1], s[2]), s[3], height=round(s[2] - r.get("z0", 0.0), 2))


def _hall(B, core):
    """The ground-floor hall of a core as a house entrance: letter boxes over a bench, boards, a book swap, a pram and a
    bike, a radio on a table; plants on every landing. (Window faces are kept clear: they are known from the plan.)"""
    cid, cx, cy = core["id"], core["c"][0], core["c"][1]
    H = core["a"] - WALL
    P = lambda fn, x, y, yaw=0.0, z=0.0, **kw: fn(B, x, y, z, yaw, **kw)
    tag = cid.lower()
    if core["kind"] == "sq":
        xe, ys, yn = cx + H, cy - H, cy + H
        ym = cy - 0.3 if cid == "A" else cy - 1.35         # C has a window in the middle of its east wall
        fu.mailboxes(B, fu.Fr(xe, ym, 270.0, 0.0), 0.0, 0.9, cols=4, rows=3)
        bn = P(kit.bench, xe - 0.2, ym, 270.0, w=1.1, kind="park", mat=kit.WOOD)
        P(kit.notice_board, xe, cy + 1.5, 270.0, w=0.8, zc=1.55, seed=3 if cid == "A" else 4)
        P(kit.bookshelf, cx + 1.1, yn, 0.0, w=0.9, h=1.5, levels=4, topic="travel", seed=2 if cid == "A" else 6)
        pr = _pram(B, xe - 0.45, cy + 1.45, 0.0)
        tb = P(kit.table, cx + 1.5, ys + 0.22, 180.0, w=0.8, d=0.4, h=0.75)
        P(kit.radio, cx + 1.3, ys + 0.22, 180.0, z=0.75)
        P(kit.newspaper, cx + 1.75, ys + 0.22, 180.0, z=0.75, cellname="news_%d" % (0 if cid == "A" else 1))
        P(kit.notice_board, cx + 1.5, ys, 180.0, w=0.7, zc=1.8, seed=5 if cid == "A" else 7)
        P(kit.picture, cx - 1.55, ys, 180.0, name="painting_sea_calm" if cid == "A" else "painting_pines", w=0.5, zc=1.8)
        _bike(B, cx - 1.5, ys + 0.34, 90.0, mat="CT_SteelRed" if cid == "A" else "CT_PlasterBlue")
        P(kit.plant, xe - 0.2, ys + 0.25, 0.0, kind="ficus", pot="CT_PlasterRed", h=1.1, seed=1)
        P(kit.rug, cx + 1.1, cy + 0.0, 0.0, w=1.0, d=1.5, name="rug_%d" % (3 if cid == "A" else 5))
        P(kit.floor_lamp, cx + 0.3, cy + 1.7, 0.0)
        _seat_thing(B, "hall_%s_bench" % tag, bn)
        B.thing("hall_%s_board_lost" % tag, "board", (xe - 0.03, cy + 1.5, 1.55), 270.0, topic="lost")
        B.thing("hall_%s_board_club" % tag, "board", (cx + 1.5, ys + 0.03, 1.8), 180.0, topic="club")
        B.thing("hall_%s_swap_shelf" % tag, "shelf", (cx + 1.1, yn - 0.05, 1.2), 0.0, topic="travel")
        B.thing("hall_%s_radio" % tag, "machine", (cx + 1.3, ys + 0.22, 1.0), 180.0, what="jukebox")
        B.thing("hall_%s_paper" % tag, "take", (cx + 1.75, ys + 0.22, 0.8), 180.0, item="daily_paper")
        B.thing("hall_%s_picture" % tag, "board", (cx - 1.55, ys + 0.03, 1.8), 180.0, topic="painting")
        B.thing("hall_%s_mail" % tag, "board", (xe - 0.05, ym, 1.25), 270.0, topic="mail")
        B.spot("hall_%s_bench" % tag, "bench", bn["seats"][0][:2] + (0.0,), bn["seats"][0][3])
    else:                                                # the octagon B
        xe, xw = cx + H, cx - H
        fu.mailboxes(B, fu.Fr(xw, cy - 0.3, 90.0, 0.0), 0.0, 0.9, cols=4, rows=3)
        bn = P(kit.bench, xe - 0.2, cy, 270.0, w=1.1, kind="park", mat=kit.WOOD)
        P(kit.notice_board, xe, cy, 270.0, w=0.8, zc=1.55, seed=8)
        P(kit.rug, cx + 1.5, cy - 0.1, 0.0, w=1.7, d=1.7, round=True, name="rug_round_0")
        ar = P(kit.armchair, cx + 0.95, cy + 1.75, 0.0, mat="CT_PlasterGreen", solid=False)
        ct = P(kit.coffee_table, cx + 1.6, cy + 0.35, 90.0, w=0.8, d=0.5)
        P(kit.newspaper, cx + 1.6, cy + 0.35, 0.0, z=0.42, cellname="news_0")
        P(kit.vase, cx + 1.6, cy + 0.6, 0.0, z=0.42)
        tb = P(kit.table, cx + 1.95, cy - 1.95, 225.0, w=0.8, d=0.4, h=0.75)
        P(kit.radio, cx + 1.9, cy - 1.9, 225.0, z=0.75)
        P(kit.plant, cx + 2.35, cy + 1.45, 0.0, kind="monstera", pot="CT_PlasterRed", h=1.1, seed=3)
        P(kit.floor_lamp, cx + 2.4, cy - 0.85, 0.0)
        pr = _pram(B, cx - 2.5, cy + 1.7, 20.0, mat="CT_PlasterRed")
        _seat_thing(B, "hall_b_bench", bn)
        _seat_thing(B, "hall_b_armchair", ar)
        B.thing("hall_b_board_lost", "board", (xe - 0.03, cy, 1.55), 270.0, topic="lost")
        B.thing("hall_b_mail", "board", (xw + 0.05, cy - 0.3, 1.25), 90.0, topic="mail")
        B.thing("hall_b_radio", "machine", (cx + 1.9, cy - 1.9, 1.0), 225.0, what="jukebox")
        B.thing("hall_b_paper", "take", (cx + 1.6, cy + 0.35, 0.45), 90.0, item="daily_paper")
        B.spot("hall_b_bench", "bench", bn["seats"][0][:2] + (0.0,), bn["seats"][0][3])
    # a plant on every landing (east side of the stair hall)
    for lv in range(1, core["n"]):
        P(kit.plant, cx + 1.9, cy + 1.6, 0.0, z=lv * STOREY, kind=("fern", "ficus", "palm", "monstera")[(lv + len(cid)) % 4],
          pot=("CT_PlasterYellow", "CT_PlasterBlue", "CT_PlasterGreen")[lv % 3], h=0.9, seed=lv)


def _strips(u0, u1, z0, z1, holes):
    """Rectangles (ua, ub, za, zb) of a wall u0..u1 x z0..z1 with rectangular holes [(ua, ub, za, zb)]."""
    out, cur = [], u0
    for (a, b, za, zb) in sorted(holes):
        if a > cur + 1e-6:
            out.append((cur, a, z0, z1))
        out.append((a, b, z0, za))
        out.append((a, b, zb, z1))
        cur = b
    if u1 > cur + 1e-6:
        out.append((cur, u1, z0, z1))
    return [r for r in out if r[1] - r[0] > 1e-6 and r[3] - r[2] > 1e-6]


def _studio(B, rnd, fr, zf, off, mat, wmat, shape, fid):
    """The pair of capsules of flat `fid` opened as ONE studio: a radio amateur's home. The outer shape is the pair's
    (two 2.3 m pods, the slit between them closed, the window cards unchanged); the way in is the 1.1 m hole in the
    core's wall at the landing (a kerb step in the wall), the floor stands 0.27 m over the landing like the pods'."""
    W2, D = CAP_W + 0.05, 0.12 + CAP_D                    # half width 2.35, depth to the front face 3.42
    z0 = zf + off - 0.1
    z1 = z0 + CAP_H
    FL = zf + 0.27
    CE = z1 - 0.08                                         # inner ceiling
    out = (fr.s, -fr.c, 0.0)
    for u in (-1.2, 1.2):                                  # the skirts, vents and window cards of the two pods
        _capsule(B, rnd, fr.sub(u, 0.0), 0, mat, zf, off, fid, shape, wmat, body=False)
    zc = (z0 + z1) / 2 + 0.05
    r = WIN_R if shape == "round" else WIN_R * 0.95
    L = fu.lbox
    # shell: floor slab, roof, side walls, front wall with the two window holes
    L(B.m, "CT_ConcreteDark", fr, -W2, W2, 0.0, D, z0, FL)
    L(B.m, mat, fr, -W2, W2, 0.0, D, CE, z1)
    for a, b in ((-W2, -W2 + 0.12), (W2 - 0.12, W2)):
        L(B.m, mat, fr, a, b, 0.0, D - 0.12, z0, z1)
    holes = [(u - r, u + r, zc - r, zc + r) for u in (-1.2, 1.2)]
    for (a, b, za, zb) in _strips(-W2, W2, z0, z1, holes):
        L(B.m, mat, fr, a, b, D - 0.12, D, za, zb)
    # the doorway: a kerb step in the thickness of the core wall (landing zf -> 0.135 -> floor 0.27)
    L(B.m, "CT_Concrete", fr, -0.55, 0.55, -WALL, 0.0, zf, zf + 0.135)
    if shape == "round":                                    # fill the corners of the square holes outside the circle
        wy = D + 0.004
        for u in (-1.2, 1.2):
            for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                arc = [fr.p(u + su * r * math.cos(math.pi / 2 * q / 4), wy, zc + sv * r * math.sin(math.pi / 2 * q / 4)) for q in range(5)]
                corner = fr.p(u + su * r, wy, zc + sv * r)
                for q in range(4):
                    tri = [corner, arc[q], arc[q + 1]]
                    B.detail.face(mat, tri if _tri_faces(tri, out) else tri[::-1])
    # lining (detail, 2 cm into the room): sides, the front wall round its holes, the back round the door, floor, ceiling
    iu, iw0, iw1 = W2 - 0.12, 0.022, D - 0.12 - 0.02
    for a, b in ((-iu, -iu + 0.02), (iu - 0.02, iu)):
        L(B.detail, "CT_Plaster", fr, a, b, iw0, iw1, FL + 0.01, CE - 0.01)
    for (a, b, za, zb) in _strips(-iu, iu, FL + 0.01, CE - 0.01, holes):
        L(B.detail, "CT_Plaster", fr, a, b, iw1, iw1 + 0.02, za, zb)
    for (a, b, za, zb) in _strips(-iu, iu, FL + 0.01, CE - 0.01, [(-0.55, 0.55, FL + 0.01, FL + 2.3)]):
        if zb > FL + 0.3 or abs(a) > 0.5:
            L(B.detail, "CT_Plaster", fr, a, b, 0.0, iw0, za, zb)
    L(B.detail, "CT_WoodLight", fr, -iu, iu, iw0, iw1, FL, FL + 0.012)
    L(B.detail, "CT_Interior", fr, -iu, iu, iw0, iw1, CE - 0.012, CE)

    def at(fn, pu, pv, yaw=0.0, z=None, **kw):
        x, y, _ = fr.p(pu, pv)
        return fn(B, x, y, FL if z is None else z, fr.yaw + yaw, **kw)

    def pt(u, w, z):
        return fr.p(u, w, z)
    WF, WB = iw1, iw0                                       # front lining face, back lining face
    # --- west pod: the bed under the shelves, the radio desk at the round window
    bed = at(kit.bed, -1.7, 1.12, 0.0, size="single", blanket="CT_PlasterGreen")
    bs = at(kit.bedside_table, -0.95, 0.27, 0.0, lamp=True)
    desk = at(kit.office_desk, -1.2, WF - 0.27, 0.0, w=1.2, d=0.5, typewriter_on=False, papers=True, phone=False, seed=5)
    ch = at(kit.chair, -0.95, WF - 0.78, 0.0, kind="wood", mat="CT_PlasterYellow")
    top = FL + 0.75
    at(kit.radio, -1.55, WF - 0.14, 180.0, z=top)
    at(kit.radio, -0.95, WF - 0.14, 180.0, z=top)
    at(kit.telephone, -1.2, WF - 0.3, 200.0, z=top)
    at(kit.books_stack, -1.7, WF - 0.3, 20.0, z=top, n=4, seed=3)
    at(kit.lamp_green, -0.62, WF - 0.14, 180.0, z=top)
    sh_w = at(kit.bookshelf, -iu, 2.75, 90.0, w=0.9, h=1.9, levels=5, topic="engineering", seed=4)
    at(kit.poster, -iu + 0.02, 1.15, 90.0, name="map_world", w=1.0, zc=1.75)
    # --- east pod: the lounge round the other window, the kitchenette by the door, shelves on the wall
    cab = at(kit.coffee_table, 1.2, WF - 0.3, 0.0, w=1.0, d=0.5)
    rp = at(kit.record_player, 1.0, WF - 0.3, 0.0, z=FL + 0.42)
    at(kit.vase, 1.5, WF - 0.3, 0.0, z=FL + 0.42)
    ar = at(kit.armchair, 1.2, 2.25, 0.0, mat="CT_PlasterRed", solid=False)
    at(kit.floor_lamp, 1.95, 2.85, 0.0)
    ku = at(kit.kitchen_unit, 1.3, 0.34, 0.0, modules="cupboard,sink")
    at(kit.samovar, 1.65, 0.3, 0.0, z=FL + 0.9)
    sh_e1 = at(kit.bookshelf, iu, 1.25, 270.0, w=0.9, h=1.9, levels=5, topic="music", seed=6)
    sh_e2 = at(kit.bookshelf, iu, 2.2, 270.0, w=0.9, h=1.9, levels=5, topic="engineering", seed=8)
    # --- the middle: rug, a picture and a clock between the windows, a plant
    at(kit.rug, 0.1, 1.85, 0.0, w=1.8, d=1.8, round=True, name="rug_round_1")
    at(kit.picture, 0.0, WF, 180.0, name="painting_dunes", w=0.5, zc=1.5)
    at(kit.wall_clock, 0.0, WF, 180.0, zc=2.0, face="clock_2", r=0.13)
    at(kit.plant, 0.0, WF - 0.35, 0.0, kind="fern", pot="CT_PlasterGreen", h=0.8, seed=5)
    kit.pendant(B, *fr.p(0.1, 1.5)[:2], CE, drop=0.2, light=True, lumens=300, colour="#FFE0B0", radius=4.0)
    # --- depth pass: Morse key, log book, tuner and headphones on the bed-head shelf, QSL cards, map pins, kettle, guest chair
    at(_morse_key, -1.4, WF - 0.4, 180.0, z=top)
    at(_logbook, -0.85, WF - 0.4, 180.0, z=top)
    at(kit.wall_shelf, -1.5, WB + 0.0, 0.0, z=FL, w=0.9, zt=1.5, props=False)
    at(_tuner, -1.78, 0.12, 0.0, z=FL + 1.5)
    at(_headphones, -1.22, 0.1, 0.0, z=FL + 1.5)
    for (cw, cz, nm) in ((1.88, 1.42, "paper_letter"), (2.12, 1.45, "paper_typed"), (1.9, 1.64, "paper_drawing"), (2.14, 1.68, "paper_letter")):
        at(kit.poster, -iu + 0.02, cw, 90.0, name=nm, w=0.19, zc=cz)
    at(_pins, -iu + 0.035, 1.15, 90.0, z=FL + 1.75)
    at(_kettle, 0.95, 0.3, 0.0, z=FL + 0.9)
    gch = at(kit.chair, -1.5, WF - 0.78, 0.0, kind="wood", mat="CT_PlasterRed")
    at(_switch_plate, -0.72, WB, 0.0, z=FL + 1.3)
    # --- things
    s = ch["seats"][0]
    B.thing("studio_desk_chair", "seat", (s[0], s[1], s[2]), s[3], height=round(s[2] - FL, 2))
    s = ar["seats"][0]
    B.thing("studio_armchair", "seat", (s[0], s[1], s[2]), s[3], height=round(s[2] - FL, 2))
    B.thing("studio_bed", "bed", pt(-1.7, 1.12, FL + 0.5), 0.0, height=0.5, rest="private")
    B.thing("studio_radio_set", "machine", pt(-1.55, WF - 0.14, top + 0.15), 180.0, what="radio_set")
    B.thing("studio_records", "machine", pt(1.0, WF - 0.3, FL + 0.5), 0.0, what="record_player")
    B.thing("studio_shelf_radio", "shelf", pt(-iu + 0.1, 2.75, FL + 1.3), 90.0, topic="engineering")
    B.thing("studio_shelf_music", "shelf", pt(iu - 0.1, 1.25, FL + 1.3), 270.0, topic="music")
    B.thing("studio_shelf_manuals", "shelf", pt(iu - 0.1, 2.2, FL + 1.3), 270.0, topic="engineering")
    B.thing("studio_map", "board", pt(-iu + 0.03, 1.15, FL + 1.75), 90.0, topic="travel")
    B.thing("studio_window_west", "board", pt(-1.2, WF, zc), 180.0, topic="view")
    B.thing("studio_window_east", "board", pt(1.2, WF, zc), 180.0, topic="view")
    B.spot("studio_chair", "seat", ch["seats"][0][:2] + (FL,), ch["seats"][0][3])
    s = gch["seats"][0]
    B.thing("studio_guest_chair", "seat", (s[0], s[1], s[2]), s[3], height=round(s[2] - FL, 2))
    B.spot("studio_guest", "seat", s[:2] + (FL,), s[3])
    B.thing("studio_morse", "machine", pt(-1.4, WF - 0.4, top + 0.06), 180.0, what="morse_key")
    B.thing("studio_logbook", "board", pt(-0.85, WF - 0.4, top + 0.05), 180.0, topic="log")
    B.thing("studio_tuner", "machine", pt(-1.78, 0.14, FL + 1.62), 0.0, what="tuner")
    B.thing("studio_headphones", "board", pt(-1.22, 0.12, FL + 1.54), 0.0, topic="headphones")
    B.thing("studio_qsl", "board", pt(-iu + 0.03, 2.0, FL + 1.55), 90.0, topic="qsl")
    B.thing("studio_kettle", "take", pt(0.95, 0.3, FL + 1.1), 0.0, item="tea")
    B.thing("studio_switch", "machine", pt(-0.72, 0.03, FL + 1.36), 0.0, what="switch", switch="room")


# ------------------------------------------------------------------ build
def build(B):
    rnd = random.Random(SEED)
    for core in CORES:
        core["poly"] = _poly(core)
        core["edges"] = _edges(core["poly"])

    def edge_idx(core, yaw):
        for i, e in enumerate(core["edges"]):
            if abs(((e["yaw"] - yaw + 180) % 360) - 180) < 1.0:
                return i
        return None

    holes = {c["id"]: {} for c in CORES}
    wins = {c["id"]: {} for c in CORES}
    reserved = {c["id"]: set() for c in CORES}          # edges taken by a door or a bridge
    bridges = []                                        # (xa, xb, yc)
    for ca, cb in zip(CORES, CORES[1:]):
        ea, eb = edge_idx(ca, 90.0), edge_idx(cb, 270.0)
        xa = ca["edges"][ea]["mid"][0]
        xb = cb["edges"][eb]["mid"][0]
        bridges.append((xa, xb, ca["c"][1], ca["id"], ea, cb["id"], eb))
        reserved[ca["id"]].add(ea)
        reserved[cb["id"]].add(eb)
        for lv in BRIDGE_LEVELS:
            for cid, ei, cc in ((ca["id"], ea, ca), (cb["id"], eb, cb)):
                if lv < cc["n"]:
                    L = cc["edges"][ei]["L"]
                    holes[cid].setdefault(ei, []).append((L / 2 - 0.75, L / 2 + 0.75, lv * STOREY, lv * STOREY + 2.35))
    bdirs = {c["id"]: [] for c in CORES}
    for b in bridges:
        bdirs[b[3]].append(90.0)
        bdirs[b[5]].append(270.0)
    # street doors (south faces)
    for core in CORES:
        ei = edge_idx(core, 0.0)
        e = core["edges"][ei]
        holes[core["id"]].setdefault(ei, []).append((e["L"] / 2 - 0.7, e["L"] / 2 + 0.7, 0.0, 2.4))
        reserved[core["id"]].add(ei)
        core["door_edge"] = ei

    # ---- capsules (decide first so that stair windows go to the free faces)
    cap_polys = {}                                      # level -> polygons
    nflat = {}
    used = {c["id"]: {} for c in CORES}                 # level -> set of edges with a capsule
    plan = []                                           # deferred capsule builds
    bound = (-19.6, 19.6, 0.8, 29.4)
    for core in CORES:
        for lv in range(1, core["n"]):
            order = list(range(len(core["edges"])))
            rnd.shuffle(order)
            for ei in order:
                e = core["edges"][ei]
                if ei in reserved[core["id"]] and ei != core["door_edge"]:
                    continue
                bridging = lv in BRIDGE_LEVELS
                if bridging and any(abs(((e["yaw"] - yw + 180) % 360) - 180) <= 46 for yw in bdirs[core["id"]]):
                    continue
                if rnd.random() < (0.22 if core["kind"] == "sq" else 0.32):
                    continue
                pair = core["kind"] == "sq" and rnd.random() < 0.22
                tilts = [0, 0, 45, -45] if core["kind"] == "sq" else [0, 0, 0, 45, -45]
                if pair:
                    tilts = [0]
                rnd.shuffle(tilts)
                fr = fu.Fr(e["mid"][0], e["mid"][1], e["yaw"], 0.0)
                placed = None
                for tilt in tilts:
                    offs = [-1.2, 1.2] if pair else [0.0]
                    fr_list = [fr.sub(u, 0.0) for u in offs]
                    res = []
                    ok = True
                    for fj in fr_list:
                        w0 = 0.12
                        for _ in range(40):
                            f2 = fj.sub(0, w0, tilt)
                            out = (fj.s, -fj.c)
                            mn = min((f2.p(u, 0.0)[0] - fj.x) * out[0] + (f2.p(u, 0.0)[1] - fj.y) * out[1]
                                     for u in (-CAP_W / 2, CAP_W / 2))
                            if mn >= 0.1:
                                break
                            w0 += 0.1
                        f2 = fj.sub(0, w0, tilt)
                        poly = [f2.p(-CAP_W / 2, 0.0)[:2], f2.p(CAP_W / 2, 0.0)[:2],
                                f2.p(CAP_W / 2, CAP_D + 0.15)[:2], f2.p(-CAP_W / 2, CAP_D + 0.15)[:2]]
                        neck = [fj.p(-0.5, 0.0)[:2], fj.p(0.5, 0.0)[:2], fj.p(0.5, w0 + 0.55)[:2], fj.p(-0.5, w0 + 0.55)[:2]]
                        if not (bound[0] < min(p[0] for p in poly) and max(p[0] for p in poly) < bound[1] and
                                bound[2] < min(p[1] for p in poly) and max(p[1] for p in poly) < bound[3]):
                            ok = False
                            break
                        for q in cap_polys.get(lv, []):
                            if _sat(poly, q, 0.12):
                                ok = False
                                break
                        if not ok:
                            break
                        for c2 in CORES:
                            if c2 is core:
                                if _sat(poly, c2["poly"], 0.05):
                                    ok = False
                            elif _sat(poly, c2["poly"], 0.6):
                                ok = False
                        for b in bridges:
                            if lv in BRIDGE_LEVELS and _sat(poly, _rect(b[0], b[1], b[2] - 1.5, b[2] + 1.5), 0.0):
                                ok = False
                        if not ok:
                            break
                        res.append(poly)
                    if ok:
                        placed = (tilt, res, offs)
                        break
                if not placed:
                    continue
                tilt, polys, offs = placed
                cap_polys.setdefault(lv, []).extend(polys)
                used[core["id"]].setdefault(lv, set()).add(ei)
                plan.append((core, lv, ei, tilt, offs, rnd.choice(SKINS), rnd.choice((0.0, 0.0, 0.3)),
                             rnd.choice(("round", "round", "rect"))))
    # which flat becomes the furnished studio: its door is a hole in the core wall at the landing of its level
    fids, cnt_ = {}, {}
    for k_, pl in enumerate(plan):
        cnt_[pl[1]] = cnt_.get(pl[1], 0) + 1
        fids[k_] = "%d-%02d" % (pl[1], cnt_[pl[1]])
    studio = next((k_ for k_, pl in enumerate(plan) if fids[k_] == STUDIO and len(pl[4]) == 2 and pl[3] == 0
                   and abs(pl[0]["edges"][pl[2]]["yaw"]) < 1.0), None)
    if studio is not None:
        sc, slv, sei = plan[studio][:3]
        sL = sc["edges"][sei]["L"]
        holes[sc["id"]].setdefault(sei, []).append((sL / 2 - 0.55, sL / 2 + 0.55, slv * STOREY, slv * STOREY + 2.3))
    # stair windows on the free faces
    for core in CORES:
        for ei, e in enumerate(core["edges"]):
            if e["L"] < 1.8:
                continue
            for lv in range(0, core["n"]):
                if lv >= 1 and ei in used[core["id"]].get(lv, set()):
                    continue
                if ei in reserved[core["id"]] and (lv == 0 or lv in BRIDGE_LEVELS) :
                    continue
                if ei in reserved[core["id"]] and ei == core["door_edge"]:
                    if lv == 0:
                        continue
                if lv == 0 and ei == core["door_edge"]:
                    continue
                zf = lv * STOREY
                wins[core["id"]].setdefault(ei, []).append((e["L"] / 2 - 0.3, e["L"] / 2 + 0.3, zf + 1.0, zf + 2.4))
    # the shells
    for core in CORES:
        hs, ws = holes[core["id"]], wins[core["id"]]
        # windows are holes too
        for ei, lst in ws.items():
            hs.setdefault(ei, []).extend(lst)
        _core_shell(B, core, hs, ws)
        _core_stairs(B, core)

    # bridges
    for (xa, xb, yc, ida, ea, idb, eb) in bridges:
        for lv in BRIDGE_LEVELS:
            if lv < min(c["n"] for c in CORES if c["id"] in (ida, idb)):
                _bridge(B, xa, xb, yc, lv * STOREY)
                tag = "%s%s%d" % (ida.lower(), idb.lower(), lv)
                _bridge_things(B, xa, xb, yc, lv, tag, "chime" if tag == "ab3" else "vane" if tag == "bc3" else None)

    # capsules (now built)
    n_flat = 0
    lvl_count = {}
    n_win = 0
    for k_, (core, lv, ei, tilt, offs, mat, off, shape) in enumerate(plan):
        e = core["edges"][ei]
        zf = lv * STOREY
        lvl_count[lv] = lvl_count.get(lv, 0) + 1
        fid = "%d-%02d" % (lv, lvl_count[lv])
        B.flat(fid, storey=lv, beds=2 if len(offs) == 2 else 1,
               door=(e["mid"][0], e["mid"][1], zf), entrance=core["id"])
        wmat = "CT_Frame" if mat in ("CT_Aluminium", "CT_SteelWhite") else "CT_SteelWhite"
        fr = fu.Fr(e["mid"][0], e["mid"][1], e["yaw"], 0.0)
        if k_ == studio:
            _studio(B, rnd, fr, zf, off, mat, wmat, shape, fid)
            n_flat += 1
            continue
        for u in offs:
            _capsule(B, rnd, fr.sub(u, 0.0), tilt, mat, zf, off, fid, shape, wmat)
        if len(offs) == 2:                                                     # collar joining a pair
            fu.lbox(B.m, "CT_ConcreteDark", fr, -1.3, 1.3, 0.0, 0.5, zf, zf + 2.0)
        n_flat += 1

    # ---- ground floor: entrances, lobby details, lamps
    z_top = max(c["n"] for c in CORES) * STOREY
    for core in CORES:
        e = core["edges"][core["door_edge"]]
        x, y = e["mid"]
        B.entrance(core["id"], (x, y, 0.0), 0.0, width=1.4, height=2.4, kind="street", leaves=1)
        box(B.m, "CT_Steel", x - 1.3, x + 1.3, y - 1.3, y, 2.55, 2.7)               # canopy
        for sx in (-1.25, 1.25):
            box(B.detail, "CT_Steel", x + sx - 0.04, x + sx + 0.04, y - 1.25, y - 1.17, 0.0, 2.55)
        cx, cy = core["c"]
        # lobby: doormat, lamps, the furniture of the hall
        fu.doormat(B, fu.Fr(x, y, 0.0), 0.0, -0.7, size=(1.0, 0.6))
        B.lamp((cx + 1.2, cy + 0.2, 2.7), lumens=420, colour="#FFE0B0", kind="lobby", radius=5.5)   # over the free lane beside the stair
        for lv in range(1, core["n"]):
            B.lamp((cx - 0.5, cy - 1.95, lv * STOREY + 2.7), lumens=380, colour="#FFE8C0", kind="stair", radius=5.0)
        _hall(B, core)
        if core["kind"] == "sq":                             # the house board beside the street door
            kit.notice_board(B, cx + 1.75, y, 0.0, 0.0, w=0.7, zc=1.6, seed=21 if core["id"] == "A" else 22)
            B.thing("house_%s_news" % core["id"].lower(), "board", (cx + 1.75, y - 0.06, 1.6), 0.0, topic="house")
    # a paving path from the street to the middle door
    box(B.m, "CT_Paving", -1.0, 1.0, 0.3, CORES[1]["edges"][CORES[1]["door_edge"]]["mid"][1] - 2.6, 0.0, 0.02, faces="Z")

    ext = [CORES[0]["c"][0] - 7.0, CORES[-1]["c"][0] + 7.0]
    xs, ys = [], []
    for polys in cap_polys.values():
        for p in polys:
            xs += [q[0] for q in p]
            ys += [q[1] for q in p]
    for c in CORES:
        xs += [q[0] for q in c["poly"]]
        ys += [q[1] for q in c["poly"]]
    x0, x1 = max(-20.0, min(xs) - 0.1), min(20.0, max(xs) + 0.1)
    y0, y1 = 0.0, min(30.0, max(ys) + 0.1)
    B.footprint = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    B.height = z_top + 0.9
    B.note("%d flats (%d paired); cores A/C 5 storeys, B 4; bridges at levels 1 and 3" % (
        n_flat, sum(1 for p in plan if len(p[4]) == 2)))
    add_surrounds(B, seed=11, bounds=(x0, x1, 0.3, 12.0))
