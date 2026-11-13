"""Shell chapel hall: a community hall for concerts and dances under a row of four white parabolic concrete vaults of
different spans. An ORIGINAL design in the spirit of Niemeyer's 1940s Pampulha pavilions (parabolic shells, tile murals,
a free-form canopy on slim columns); not a copy of any of them.

Plot ~40 x 30, height_max 14. Footprint x -16.9..16.9, y 0.4..24.5; highest crown 12.6 m. Four vaults side by side,
each a thin (22 cm) parabolic shell resting on 3 m high walls (springing z 3.0, V-shaped valleys between the shells):
  A  span 7.0   crown 7.2   y 5.0..18.5   foyer and cloak room            street end: azulejo mural (diamond lattice)
  B  span 12.0  crown 12.6  y 2.6..24.5   MAIN HALL: lobby, 60 chairs, raised stage (z 1.0), tile backdrop
                                          street end: glazed wall of window cards (8 upper tall trapezoids cut to the
                                          arch, 6 lower, a fanlight) with the double door "main"
  C  span 8.5   crown 8.8   y 4.2..19.5   dance room with a small bar, tables, a tile dance floor   azulejo (waves)
  D  span 6.0   crown 6.4   y 7.6..16.4    kitchen and service; staff door "staff" in the east wall     azulejo (sun)
The mural / glazed fills stand 0.5 m behind the shell's front edge, so every arch shows its frame. Rooms are joined by
doorways through the shared walls. A free-form concrete canopy on four slim columns covers the entrance.
Walls are boxes (closed, no open ends); the floors are CT_Floor slabs; furniture goes to B.detail, counters and tables
into the shell. Windows: public cards only (kind public), no flats. Lamps: 14 pendants hang from the vaults.
Second pass: sound desk, spotlight bar, backstage, gallery lights, guest book, pennants, mirror ball.
Spots: 60 "seat" in the hall, "work" on stage / bar / kitchen, "stand", "table" + "seat" in the dance room, "counter".
Entrances: "main" (street, double leaves), "staff" (service).
Open hours: 9..23 (HOURS in unreal/scripts/import_city.py; the caretaker starts at 9).
Material requests: none.
"""
import math
import os

from citykit import box, prism, triangulate, ring
from _cons_util import win
from _sheet import thick_sheet
from _surrounds import add_surrounds
from furn_int2 import Furn
import interior_kit as kit

INFO = {
    "name_ru": "Дом собраний «Своды»", "name_en": "Shell Chapel Hall",
    "style": "brazilian modernism", "use": "public", "kind": "hall", "hours": [10, 23],
    "inspired_by": "Niemeyer's 1940s Pampulha pavilions: parabolic concrete vaults, azulejo murals, a free-form canopy "
                   "on slim columns; original massing, here a row of four vaults of different spans as a community hall",
}

ZW = 3.0                      # springing height (wall tops)
VT = 0.22                     # shell thickness (normal)
XS = [-16.75, -9.75, 2.25, 10.75, 16.75]
WT = 0.30                     # wall thickness
WARM = "#FFD6A0"
VAULTS = [
    dict(n="A", Hr=4.2, y0=5.0, y1=18.5),
    dict(n="B", Hr=9.6, y0=2.6, y1=24.5),
    dict(n="C", Hr=5.8, y0=4.2, y1=19.5),
    dict(n="D", Hr=3.4, y0=7.6, y1=16.4),
]
for k, v in enumerate(VAULTS):
    v["k"] = k
    v["xa"], v["xb"] = XS[k] + 0.1, XS[k + 1] - 0.1
    v["xc"] = (v["xa"] + v["xb"]) / 2
    v["hs"] = (v["xb"] - v["xa"]) / 2
    v["ym"] = v["y0"] + 0.5                      # plane of the street-end infill
    v["yb"] = v["y1"] - 0.1                      # outer face of the back wall
    v["cl"], v["cr"] = XS[k] + WT / 2, XS[k + 1] - WT / 2     # clear span between wall faces


def top_z(v, x):
    t = (x - v["xc"]) / v["hs"]
    return ZW + v["Hr"] * (1 - t * t)


def slope(v, x):
    return -2 * v["Hr"] * (x - v["xc"]) / v["hs"] ** 2


def sof_z(v, x):
    return top_z(v, x) - VT * math.sqrt(1 + slope(v, x) ** 2)


def yaw_to(a, b):
    return math.degrees(math.atan2(b[0] - a[0], -(b[1] - a[1])))


def _bars(B, q, n, inset, cols, rows, t):
    """Frame bars over a trapezoid card (top edge slopes with the arch)."""
    p0, p1, p2, p3 = [(p[0] - n[0] * inset + n[0] * 0.03, p[1] - n[1] * inset + n[1] * 0.03, p[2]) for p in q]

    def P(a, b):
        lo = [p0[k] + (p3[k] - p0[k]) * b for k in range(3)]
        hi = [p1[k] + (p2[k] - p1[k]) * b for k in range(3)]
        return tuple(lo[k] + (hi[k] - lo[k]) * a for k in range(3))
    w = math.dist(p0, p1)
    h = min(math.dist(p0, p3), math.dist(p1, p2))
    ta, tb = t / w, t / h
    bars = [(0, 0, 1, tb), (0, 1 - tb, 1, 1), (0, 0, ta, 1), (1 - ta, 0, 1, 1)]
    for i in range(1, cols):
        a = i / cols
        bars.append((a - ta / 2, 0, a + ta / 2, 1))
    for j in range(1, rows):
        b = j / rows
        bars.append((0, b - tb / 2, 1, b + tb / 2))
    for a0, b0, a1, b1 in bars:
        B.detail.face("CT_Frame", [P(a0, b0), P(a1, b0), P(a1, b1), P(a0, b1)])


# ---------------------------------------------------------------------------------------------- walls
def wall_x(B, x0, x1, ya, yb, holes, ml, mr, first=True, last=True, ztop=ZW - 0.03):
    """A wall box x0..x1 along y (ya..yb), z 0..ztop, with openings holes [(y0, y1, z0, z1)] (sill z0 may be 0). The -x face
    has material ml, the +x face mr; the ends (first / last piece only) and the reveals are white."""
    m = B.m
    W = "CT_ConcreteWhite"
    hs = sorted(h for h in holes if ya - 1e-6 <= h[0] and h[1] <= yb + 1e-6)
    cuts = [ya]
    segs = []
    for h in hs:
        segs.append((cuts[-1], h[0]))
        cuts.append(h[1])
    segs.append((cuts[-1], yb))

    def bx(y0, y1, z0, z1, rest):
        box(m, ml, x0, x1, y0, y1, z0, z1, faces="x")
        box(m, mr, x0, x1, y0, y1, z0, z1, faces="X")
        if rest:
            box(m, W, x0, x1, y0, y1, z0, z1, faces=rest)
    for i, (a, b) in enumerate(segs):
        if b - a < 1e-4:
            continue
        rest = "Z" + ("y" if (i > 0 or first) else "") + ("Y" if (i < len(segs) - 1 or last) else "")
        bx(a, b, 0.0, ztop, rest)
    for h in hs:
        if h[2] > 0.01:
            bx(h[0], h[1], 0.0, h[2], "Z")
        bx(h[0], h[1], h[3], ztop, "zZ")


def walls(B):
    m = B.m
    W, I = "CT_ConcreteWhite", "CT_Interior"
    holes = {1: [(7.0, 9.0, 0.0, 2.45), (13.5, 15.5, 0.0, 2.45)],
             2: [(9.0, 12.0, 0.0, 2.55), (16.0, 18.0, 0.0, 2.45)],
             3: [(10.0, 12.5, 0.0, 2.45)],
             4: [(12.0, 13.3, 0.0, 2.4), (9.0, 10.4, 0.9, 2.4), (14.3, 15.7, 0.9, 2.4)],
             0: [(8.0, 9.5, 0.9, 2.4), (11.0, 12.5, 0.9, 2.4), (14.0, 15.5, 0.9, 2.4), (16.5, 17.5, 0.9, 2.4)]}
    # extra stage-side windows on the exterior walls of the main hall
    holes[1] += [(19.8, 21.3, 1.3, 2.6), (22.3, 23.8, 1.3, 2.6)]
    holes[2] += [(20.2, 21.7, 1.3, 2.6), (22.5, 24.0, 1.3, 2.6)]
    for j in range(5):
        L = VAULTS[j - 1] if j > 0 else None
        R = VAULTS[j] if j < 4 else None
        x0, x1 = XS[j] - WT / 2, XS[j] + WT / 2
        pts = sorted({v[k] for v in (L, R) if v for k in ("ym", "yb")})
        ivs = []
        for ya, yb in zip(pts, pts[1:]):
            mid = (ya + yb) / 2
            inL = L is not None and L["ym"] <= mid <= L["yb"]
            inR = R is not None and R["ym"] <= mid <= R["yb"]
            if inL or inR:
                ivs.append((ya, yb, inL, inR))
        for q, (ya, yb, inL, inR) in enumerate(ivs):
            ml = I if inL else W
            mr = I if inR else W
            hl = holes.get(j, [])
            wall_x(B, x0, x1, ya, yb, hl, ml, mr, first=(q == 0), last=(q == len(ivs) - 1))
            for h in hl:                      # window cards on exterior faces, entrance, thresholds
                if not (ya - 1e-6 <= h[0] and h[1] <= yb + 1e-6):
                    continue
                if inL and inR:
                    box(m, "CT_Floor", x0, x1, h[0], h[1], -0.3, 0.0, faces="xXyYZ")
                elif h[2] > 0.5:
                    if j == 4:
                        P = [(x1, h[0], h[2]), (x1, h[1], h[2]), (x1, h[1], h[3]), (x1, h[0], h[3])]
                    elif not inL and inR:
                        P = [(x0, h[1], h[2]), (x0, h[0], h[2]), (x0, h[0], h[3]), (x0, h[1], h[3])]
                    else:                           # left room only: exterior on +x
                        P = [(x1, h[0], h[2]), (x1, h[1], h[2]), (x1, h[1], h[3]), (x1, h[0], h[3])]
                    B.window(P, kind="public", cols=2, rows=1, inset=0.12, frame=0.05)
                elif j == 4:
                    box(m, "CT_Floor", x0, x1, h[0], h[1], -0.3, 0.0, faces="xXyYZ")
                    B.entrance("staff", (x1, (h[0] + h[1]) / 2, 0.0), 90.0, width=h[1] - h[0], height=h[3], kind="service")
                    box(B.detail, "CT_Steel", x1 + 0.0, x1 + 0.9, h[0] - 0.1, h[1] + 0.1, 3.0 - 0.45, 3.0 - 0.4)


# ---------------------------------------------------------------------------------------------- shells
def shell(B, v):
    def f(u, w):
        x = v["xa"] + u * 2 * v["hs"]
        return (x, v["y0"] + w * (v["y1"] - v["y0"]), top_z(v, x))

    def th(u, w):
        x = v["xa"] + u * 2 * v["hs"]
        return VT * math.sqrt(1 + slope(v, x) ** 2)
    thick_sheet(B.m, f, 28, 1, th, "CT_ConcreteWhite", "CT_ConcreteWhite", "CT_ConcreteWhite", edges="uUvV")


# ---------------------------------------------------------------------------------------------- arch fills
def arch_strips(B, v, y, face, xl, xr, n, mat, z0=0.0, add=0.04):
    """Plain vertical strips under the soffit between xl..xr at plane y; face +1 = outward is +y."""
    cw = (xr - xl) / n
    for c in range(n):
        xa, xb = xl + c * cw, xl + (c + 1) * cw
        za, zb = sof_z(v, xa) + add, sof_z(v, xb) + add
        if face > 0:
            B.m.face(mat, [(xb, y, z0), (xa, y, z0), (xa, y, za), (xb, y, zb)])
        else:
            B.m.face(mat, [(xa, y, z0), (xb, y, z0), (xb, y, zb), (xa, y, za)])


def mural(B, v, pattern):
    xl, xr = v["cl"], v["cr"]
    n = max(4, round((xr - xl) / 0.6))
    cw = (xr - xl) / n
    cs = 0.6
    ym = v["ym"]
    m = B.m
    for c in range(n):
        xa, xb = xl + c * cw, xl + (c + 1) * cw
        za, zb = sof_z(v, xa) + 0.04, sof_z(v, xb) + 0.04
        zmin = min(za, zb)
        z, r = 0.0, 0
        while z + cs <= zmin:
            mat = "CT_Tile" if pattern(c, r, n, v, (xa + xb) / 2, z + cs / 2) else "CT_TileWhite"
            m.face(mat, [(xa, ym, z), (xb, ym, z), (xb, ym, z + cs), (xa, ym, z + cs)])
            z += cs
            r += 1
        mat = "CT_Tile" if pattern(c, r, n, v, (xa + xb) / 2, z + 0.3) else "CT_TileWhite"
        if max(za, zb) - z > 0.03:
            if min(za, zb) - z < 0.03:
                if zb > za:
                    m.face(mat, [(xa, ym, z), (xb, ym, z), (xb, ym, zb)])
                else:
                    m.face(mat, [(xa, ym, z), (xb, ym, z), (xa, ym, za)])
            else:
                m.face(mat, [(xa, ym, z), (xb, ym, z), (xb, ym, zb), (xa, ym, za)])
    arch_strips(B, v, ym + 0.25, 1, xl, xr, n, "CT_Interior")


def pat_lattice(c, r, n, v, x, z):
    return (c + r) % 4 == 0 or (c - r) % 4 == 0


def pat_waves(c, r, n, v, x, z):
    return (r + round(2.1 * math.sin(c * 0.55))) % 4 in (0, 1)


def pat_sun(c, r, n, v, x, z):
    return int(math.hypot(x - v["xc"], z - 1.2) / 0.75) % 2 == 0


# ---------------------------------------------------------------------------------------------- main hall front
def hall_front(B):
    v = VAULTS[1]
    ym = v["ym"]
    xl, xr = v["cl"], v["cr"]
    n = 10
    cw = (xr - xl) / n
    X = [xl + k * cw for k in range(n + 1)]
    m = B.m
    W = "CT_ConcreteWhite"
    for k in (0, 9):
        arch_strips(B, v, ym, -1, X[k], X[k + 1], 3, W)
        arch_strips(B, v, ym + 0.3, 1, X[k], X[k + 1], 3, "CT_Interior")
    for k in range(1, n):
        if k == 5:
            continue
        xp = X[k]
        box(m, W, xp - 0.07, xp + 0.07, ym + (0.02 if k in (1, 9) else 0.0), ym + 0.3, 0.0, sof_z(v, xp) + 0.05, faces="xXyZ")
    for k in (1, 2, 3, 6, 7, 8):
        a, b = X[k] + 0.07, X[k + 1] - 0.07
        box(m, W, a, b, ym + 0.04, ym + 0.3, 0.0, 0.5, faces="xXyYZ")
        B.window([(a, ym, 0.55), (b, ym, 0.55), (b, ym, 3.25), (a, ym, 3.25)], kind="public", cols=1, rows=2,
                 inset=0.12, frame=0.05)
    box(m, W, X[1], X[9], ym + 0.04, ym + 0.3, 3.3, 3.6, faces="yYZz")
    B.window([(X[4] + 0.07, ym, 2.85), (X[6] - 0.07, ym, 2.85), (X[6] - 0.07, ym, 3.25), (X[4] + 0.07, ym, 3.25)],
             kind="public", cols=4, rows=1, inset=0.12, frame=0.05)
    for k in range(1, 9):
        a, b = X[k] + 0.07, X[k + 1] - 0.07
        za, zb = sof_z(v, a) - 0.07, sof_z(v, b) - 0.07
        pts = [(a, ym, 3.7), (b, ym, 3.7), (b, ym, zb), (a, ym, za)]
        B.window(pts, kind="public", inset=0.12, frame=0)
        _bars(B, pts, (0.0, -1.0), 0.12, 1, max(1, int((min(za, zb) - 3.7) / 1.4)), 0.05)
    xm = v["xc"]
    B.entrance("main", (xm, ym, 0.0), 0.0, width=2.2, height=2.8, kind="street", leaves=2)
    # floor continues to the threshold
    # ---- the free-form canopy
    xa_, xb_ = xm - 5.25, xm + 5.25
    yback = ym + 0.1

    def depth(u):
        return 2.35 + 0.45 * math.sin(2 * math.pi * 1.25 * u) + 0.2 * math.sin(2 * math.pi * 2.7 * u + 1.0)

    def cz(u, w):
        return 3.62 + w * (0.5 * math.sin(2 * math.pi * 1.1 * u + 0.6) + 0.12 * math.sin(2 * math.pi * 3.1 * u))

    def f(u, w):
        return (xa_ + u * (xb_ - xa_), yback - w * depth(u), cz(u, w))
    thick_sheet(m, f, 24, 8, lambda u, w: 0.18, "CT_ConcreteWhite", "CT_Concrete", "CT_ConcreteWhite", edges="uUvV")
    for u in (0.1, 0.38, 0.64, 0.9):
        x, y, z = f(u, 1.0)
        B.column_round(x, y + 0.2, 0.0, z - 0.1, 0.1, mat="CT_ConcreteWhite", segs=10)


# ---------------------------------------------------------------------------------------------- rooms
def floors(B):
    for v in VAULTS:
        y0 = v["ym"] + (0.0 if v["n"] == "B" else 0.25)
        box(B.m, "CT_Floor", v["cl"], v["cr"], y0, v["yb"] - 0.3, -0.3, 0.0, faces="xXyYZ")


def backs(B):
    for v in VAULTS:
        n = 12 if v["n"] == "B" else 8
        arch_strips(B, v, v["yb"], 1, v["cl"], v["cr"], n, "CT_ConcreteWhite")
        arch_strips(B, v, v["yb"] - 0.3, -1, v["cl"], v["cr"], n, "CT_Interior")


def hang(F, v, x, y, drop, lumens=1100, radius=8.0):
    F.pendant(x, y, sof_z(v, x) + 0.05, lumens=lumens, colour=WARM, radius=radius, drop=drop)


def _mic(B, x, y, z):
    """A microphone stand (a base, a rod, a boom, the capsule): local, no kit piece for it."""
    d = B.detail
    box(d, "CT_Steel", x - 0.2, x + 0.2, y - 0.2, y + 0.2, z, z + 0.02, faces="xXyYZ")
    box(d, "CT_Steel", x - 0.012, x + 0.012, y - 0.012, y + 0.012, z + 0.02, z + 1.45, faces="xXyY")
    box(d, "CT_Steel", x - 0.012, x + 0.012, y - 0.26, y, z + 1.43, z + 1.46, faces="xXyYzZ")
    box(d, "CT_Aluminium", x - 0.04, x + 0.04, y - 0.34, y - 0.24, z + 1.4, z + 1.5, faces="xXyYzZ")


def hall_b(B, F):
    v = VAULTS[1]
    xc = v["xc"]
    d = B.detail
    T = B.thing
    # ---- seating: 6 rows of 10, aisle in the middle
    n = 0
    for r in range(6):
        y = 7.8 + 0.95 * r
        for side in (-1, 1):
            xs = []
            for k in range(5):
                x = xc + side * (0.95 + 0.58 * k)
                kit.chair(B, x, y, yaw=180.0, kind="tube", mat=("CT_PlasterBlue", "CT_PlasterBlue", "CT_PlasterRed")[(r + k) % 3])
                n += 1
                B.spot("seat_%d" % n, "seat", (x, y, 0.0), 180.0)
                xs.append(x)
            T("row_%d_%s" % (r + 1, "w" if side < 0 else "e"), "seat", (sum(xs) / 5, y, 0.45), 180.0, height=0.45)
    F.rug(xc - 0.7, xc + 0.7, 4.2, 17.4, mat="CT_PlasterRed")
    # ---- stage
    zs = 1.0
    sx0, sx1, sy0, sy1 = v["cl"], v["cr"], 18.6, v["yb"] - 0.3
    box(B.m, "CT_WoodLight", sx0, sx1, sy0, sy1, 0.0, zs, faces="xXyYZ")
    for xa, xb in ((sx0 + 0.05, sx0 + 0.95), (sx1 - 0.95, sx1 - 0.05)):
        for i in range(7):
            ya = sy0 - 2.1 + i * 0.3
            box(B.m, "CT_Concrete", xa, xb, ya, ya + 0.3, 0.0, zs / 7 * (i + 1), faces="xXyYZ")
    box(d, "CT_PlasterRed", sx0, sx1, sy0 - 0.02, sy0, 0.05, 0.12, faces="y")
    # tile backdrop on the rear wall
    yb = v["yb"] - 0.3 - 0.015
    cs = 0.6
    for c in range(14):
        for r in range(8):
            xa = xc - 4.2 + c * cs
            za = zs + 0.4 + r * cs
            blue = ((c + r) % 4 == 0 or (c - r) % 4 == 0)
            d.face("CT_Tile" if blue else "CT_TileWhite", [(xa, yb, za), (xa + cs, yb, za), (xa + cs, yb, za + cs), (xa, yb, za + cs)])
    box(d, "CT_Frame", xc - 4.26, xc + 4.26, yb - 0.02, yb, zs + 0.34, zs + 0.4)
    box(d, "CT_Frame", xc - 4.26, xc + 4.26, yb - 0.02, yb, zs + 0.4 + 8 * cs, zs + 0.46 + 8 * cs)
    for xx in (xc - 4.26, xc + 4.2):
        box(d, "CT_Frame", xx, xx + 0.06, yb - 0.02, yb, zs + 0.34, zs + 0.46 + 8 * cs)
    for sgn in (-1, 1):                                    # curtains
        for i in range(4):
            xx = xc + sgn * (4.9 + 0.22 * i) if sgn > 0 else xc - (4.9 + 0.22 * i) - 0.2
            box(d, "CT_PlasterRed" if i % 2 else "CT_SteelRed", xx, xx + 0.2, yb - 0.3 + 0.2, yb - 0.02, zs, 5.2)
    # grand piano (the pianist sits at its -x side, seen from the side by the audience), a stool, music
    gp = kit.grand_piano(B, xc + 3.0, 21.5, zs, 270.0)
    sx_, sy_, sz_ = gp["seats"][0][:3] if gp.get("seats") else (xc + 1.6, 21.5, zs)
    kit.stool(B, xc + 1.55, 21.5, zs, 90.0, h=0.5)
    T("stage_piano", "play", (xc + 3.0, 21.5, zs + 0.9), 270.0, what="piano")
    B.spot("stage_piano", "work", (xc + 1.55, 21.5, zs), 90.0)
    # a choir: two rows of wooden chairs on the stage's west half, a conductor's music stand, a microphone
    for rr, yy in enumerate((22.9, 23.6)):
        kit.row(kit.chair, B, xc - 3.6, yy, xc - 0.2, yy, 6, yaw=0.0, z=zs, kind="wood")
    T("choir_riser", "screen", (xc - 1.9, 22.9, zs + 0.5), 0.0, what="choir")
    _mic(B, xc - 1.9, 20.1, zs)
    T("stage_mic", "machine", (xc - 1.9, 19.9, zs + 1.3), 0.0, what="microphone")
    kit.table(B, xc - 4.6, 20.6, zs, 0.0, w=1.0, d=0.5, h=0.75, mat=kit.WOODL)
    kit.books_stack(B, xc - 4.6, 20.6, zs + 0.75, 20, n=3, seed=1)
    T("stage", "screen", (xc, 19.4, zs + 0.9), 0.0, what="stage")
    B.spot("stage_a", "work", (xc - 1.8, 21.2, zs), 0.0)
    B.spot("stage_b", "work", (xc + 0.2, 20.6, zs), 0.0)
    B.spot("stage_mic", "stand", (xc - 1.9, 19.5, zs), 0.0)
    # lobby: benches under the glass, plants, boards on the side walls
    for xx in (xc - 3.4, xc + 3.4):
        kit.bench(B, xx, 4.2, yaw=180.0, w=1.8, kind="park")
        k = 1 if xx < xc else 2
        B.spot("lobby_bench_%d" % k, "bench", (xx, 4.2, 0.0), 180.0)
        T("lobby_bench_%d" % k, "seat", (xx, 4.2, 0.45), 180.0, height=0.45)
    kit.plant(B, v["cl"] + 0.6, 3.7, kind="palm", pot="CT_TileWhite", h=1.8)
    kit.plant(B, v["cr"] - 0.6, 4.1, kind="ficus", pot="CT_TileWhite", h=1.8)
    B.spot("lobby_stand_1", "stand", (xc - 1.6, 5.4, 0.0), 0.0)
    B.spot("lobby_stand_2", "stand", (xc + 1.6, 5.6, 0.0), 0.0)
    kit.notice_board(B, v["cl"], 5.4, yaw=90.0, w=1.3, zc=1.55, seed=3)
    T("lobby_stand_1", "board", (v["cl"] + 0.2, 5.4, 1.55), 90.0, topic="club")
    kit.poster(B, v["cr"], 4.5, yaw=270.0, name="poster_cons_0", w=0.8, zc=1.6)
    kit.poster(B, v["cr"], 5.5, yaw=270.0, name="poster_cons_1", w=0.8, zc=1.6)
    T("lobby_stand_2", "board", (v["cr"] - 0.2, 5.0, 1.6), 270.0, topic="timetable")
    kit.plant(B, v["cl"] + 0.5, 17.4, kind="fern", pot="CT_PlasterRed", h=1.5)
    # side walls: tile panels and posters between the doorways, a clock
    for yy, nm in ((10.2, "tile_3"), (12.9, "tile_6")):
        kit.mosaic_panel(B, v["cl"], yy, yaw=90.0, name=nm, w=1.0, h=1.0, zc=1.9)
    kit.poster(B, v["cl"], 11.55, yaw=90.0, name="poster_cons_2", w=0.7, zc=1.8)
    for yy, nm in ((13.0, "tile_1"), (15.0, "tile_8")):
        kit.mosaic_panel(B, v["cr"], yy, yaw=270.0, name=nm, w=1.0, h=1.0, zc=1.9)
    kit.poster(B, v["cr"], 14.0, yaw=270.0, name="poster_cons_3", w=0.7, zc=1.8)
    kit.wall_clock(B, v["cr"], 7.0, yaw=270.0, zc=2.6, r=0.25)
    kit.fire_extinguisher(B, v["cr"], 8.4, yaw=270.0)
    kit.mosaic_panel(B, v["cl"], 6.0, yaw=90.0, name="tile_5", w=0.9, h=0.9, zc=1.8) if False else None
    F.border(v["cl"], v["cr"], 3.1 + 0.3, v["yb"] - 0.3, w=0.35, mat="CT_ConcreteDark")
    # lamps hanging from the vault
    for y in (8.0, 12.0, 16.0, 21.0):
        for s in (-1, 1):
            hang(F, v, xc + s * 2.7, y, 2.4, lumens=1500, radius=9.0)


def hall_a(B, F):
    """Foyer and cloak room: an exhibition of paintings and travel posters, sofas, a coffee corner."""
    v = VAULTS[0]
    d = B.detail
    T = B.thing
    cl, cr = v["cl"], v["cr"]
    # cloak room: a counter facing east, the coat rail behind it by the west wall
    kit.shop_counter(B, -14.9, 9.8, yaw=90.0, w=3.2, d=0.65, front="CT_PlasterGreen")
    kit.coat_rack(B, -16.25, 9.8, yaw=90.0, w=2.4, coats=0.8, seed=2)
    kit.cash_register(B, -14.9, 10.8, 1.05, 90.0) if False else None
    kit.books_stack(B, -14.95, 8.7, 1.05, 90.0, n=2, seed=2)
    kit.sign_plate(B, cl, 9.8, yaw=90.0, name="sign_cloakroom", w=1.3, zc=2.55)
    T("cloak_counter", "counter", (-14.9, 9.8, 1.05), 90.0, role="caretaker", service="cloakroom")
    B.spot("cloak_clerk", "work", (-15.6, 9.8, 0.0), 90.0)
    B.spot("cloak_counter", "counter", (-14.15, 9.8, 0.0), 270.0)
    # the exhibition: paintings on the east wall (towards the doors), the north wall and the west wall
    east = [(9.7, "painting_abstract", 1.0), (11.25, "painting_portrait", 1.0), (12.8, "painting_still_life", 1.0),
            (16.2, "painting_sea_night", 0.8), (17.3, "travel_2", 0.6), (6.25, "painting_pines", 0.8)]
    for yy, nm, w in east:
        kit.picture(B, cr, yy, yaw=270.0, name=nm, w=w, zc=1.65)
    north = [(-15.2, "painting_lighthouse", 1.2), (-13.25, "painting_sea_calm", 1.4), (-11.3, "painting_dunes", 1.2)]
    for xx, nm, w in north:
        kit.picture(B, xx, v["yb"] - 0.3, yaw=0.0, name=nm, w=w, zc=1.7)
    west = [(6.3, "travel_0", 0.7), (10.25, "painting_sea_storm", 0.8), (13.25, "travel_3", 0.7), (16.0, "travel_4", 0.6)]
    for yy, nm, w in west:
        kit.picture(B, cl, yy, yaw=90.0, name=nm, w=w, zc=1.65)
    T("exhibit_east", "board", (cr - 0.3, 11.25, 1.65), 270.0, topic="painting")
    T("exhibit_east_2", "board", (cr - 0.3, 16.7, 1.65), 270.0, topic="exhibit")
    T("exhibit_north", "board", (-13.25, v["yb"] - 0.6, 1.7), 0.0, topic="painting")
    T("exhibit_west", "board", (cl + 0.3, 10.25, 1.65), 90.0, topic="exhibit")
    T("exhibit_west_2", "board", (cl + 0.3, 6.3, 1.65), 90.0, topic="exhibit")
    # seating for looking: a bench facing the east wall, one facing the north wall, a sofa by the entrance wall
    kit.bench(B, -11.0, 11.25, yaw=90.0, w=2.4, kind="gym")
    B.spot("foyer_bench_1", "bench", (-11.0, 11.25, 0.0), 90.0)
    T("foyer_bench_1", "seat", (-11.0, 11.25, 0.45), 90.0, height=0.45)
    kit.bench(B, -13.25, 17.2, yaw=180.0, w=2.4, kind="gym")
    B.spot("foyer_bench_2", "bench", (-13.25, 17.2, 0.0), 180.0)
    T("foyer_bench_2", "seat", (-13.25, 17.2, 0.45), 180.0, height=0.45)
    kit.sofa(B, -13.3, 6.4, yaw=180.0, seats=3, mat="CT_PlasterRed")
    T("foyer_sofa", "seat", (-13.3, 6.45, 0.45), 180.0, height=0.45)
    kit.coffee_table(B, -13.3, 7.7, w=1.0, d=0.55)
    kit.newspaper(B, -13.3, 7.7, 0.42, 0.0, seed=1)
    kit.vase(B, -13.0, 7.75, 0.42, 0.0, seed=4)
    # coffee corner by the north-west wall
    tb = kit.table_round(B, -14.0, 14.9, r=0.45, mat=kit.WOODL)
    kit.cup(B, -14.1, 14.9, tb["top"], 0.0)
    kit.cup(B, -13.85, 14.8, tb["top"], 90.0, mat="CT_PlasterBlue")
    kit.armchair(B, -15.2, 14.9, yaw=90.0, mat="CT_PlasterGreen")
    kit.armchair(B, -14.0, 16.0, yaw=0.0, mat="CT_PlasterGreen")
    T("foyer_armchair_1", "seat", (-15.2, 14.9, 0.45), 90.0, height=0.45)
    T("foyer_armchair_2", "seat", (-14.0, 16.0, 0.45), 0.0, height=0.45)
    kit.rug(B, -12.7, 11.8, w=2.6, d=3.4, name="rug", seed=3)
    kit.floor_lamp(B, -11.0, 7.0, shade="CT_Paper")
    kit.plant(B, -16.0, 6.4, kind="ficus", pot="CT_PlasterRed", h=1.7)
    kit.plant(B, -16.0, 17.6, kind="fern", pot="CT_PlasterYellow", h=1.3)
    B.spot("foyer_stand_1", "stand", (-13.2, 12.6, 0.0), 90.0)
    B.spot("foyer_stand_2", "stand", (-12.2, 9.4, 0.0), 0.0)
    for xx in (-14.2, -12.4):
        hang(F, v, xx, 12.0, 1.4, lumens=700, radius=6.5)
    F.border(v["cl"], v["cr"], v["ym"] + 0.25, v["yb"] - 0.3, w=0.3, mat="CT_ConcreteDark")


def hall_c(B, F):
    """Dance room: a bar with stools, tables along the west wall, the tiled dance floor, a jukebox and a piano."""
    v = VAULTS[2]
    d = B.detail
    T = B.thing
    cl, cr = v["cl"], v["cr"]
    yn = v["yb"] - 0.3                                     # inner face of the back wall
    # bar along the east wall: the counter faces west, the back bar of bottles against the wall
    bc = kit.bar_counter(B, 8.7, 8.1, yaw=270.0, w=4.6, d=0.65, taps=3, seed=5)
    for k, yy in enumerate((6.3, 7.5, 8.7, 9.9)):
        kit.glass(B, 8.7, yy, 1.05, 0.0)
    kit.teapot(B, 8.75, 6.6, 1.05, 0.0)
    kit.cup(B, 8.7, 9.3, 1.05, 0.0, mat="CT_PlasterYellow")
    kit.sign_plate(B, cr, 8.1, yaw=270.0, name="sign_buffet", w=1.5, zc=2.75)
    for i in range(5):
        yy = 6.1 + i * 0.95
        kit.bar_stool(B, 7.95, yy, yaw=90.0, mat="CT_PlasterRed" if i % 2 else "CT_PlasterYellow")
        B.spot("bar_stool_%d" % (i + 1), "seat", (7.95, yy, 0.0), 90.0)
    T("bar_stools", "seat", (7.95, 8.1, 0.75), 90.0, height=0.75)
    T("bar_counter", "counter", (8.7, 8.1, 1.05), 270.0, role="caretaker", service="bar", shop="hall_bar")
    B.spot("bar_barman", "work", (9.5, 7.6, 0.0), 270.0)
    B.spot("bar_barman2", "work", (9.5, 9.0, 0.0), 270.0)
    B.spot("bar_counter", "counter", (7.4, 10.4, 0.0), 90.0)
    # tables with chairs along the west wall
    t = 0
    for y in (6.2, 8.0, 13.6, 15.4):
        t += 1
        tb = kit.table_round(B, 4.2, y, r=0.45, mat=kit.WOODL)
        kit.cup(B, 4.1, y, tb["top"], 0.0, mat="CT_PlasterBlue")
        B.spot("c_table_%d" % t, "table", (3.3, y, 0.0), 90.0)
        for i, (cx, cy, yw) in enumerate(((3.25, y, 90.0), (5.15, y, 270.0), (4.2, y + (0.85 if y < 10 else -0.85), 180.0 if y < 10 else 0.0))):
            kit.chair(B, cx, cy, yaw=yw, mat="CT_PlasterYellow")
            B.spot("c_seat_%d_%d" % (t, i + 1), "seat", (cx, cy, 0.0), yw)
        T("c_table_%d" % t, "seat", (3.25, y, 0.45), 90.0, height=0.45)
    # tiled dance floor
    sq = 0.9
    for i in range(6):
        for j in range(7):
            x0, y0 = 4.6 + i * sq, 11.6 + j * sq
            box(d, "CT_TileWhite" if (i + j) % 2 == 0 else "CT_Tile", x0, x0 + sq, y0, y0 + sq, 0.0, 0.014, faces="Z")
    for k, (x, y, yw) in enumerate(((6.0, 14.0, 0.0), (7.0, 15.0, 180.0), (8.0, 13.0, 90.0), (5.6, 16.5, 270.0))):
        B.spot("dance_%d" % (k + 1), "stand", (x, y, 0.0), yw)
        T("dance_%d" % (k + 1), "sport", (x, y, 0.1), yw, what="dance")
    # the corner of the band: jukebox and an upright piano with its stool against the back wall
    kit.jukebox(B, 4.0, yn - 0.32, yaw=0.0)
    T("dance_jukebox", "machine", (4.0, yn - 0.4, 1.0), 0.0, what="jukebox")
    kit.piano(B, 8.3, yn - 0.02, yaw=0.0)
    kit.stool(B, 8.3, yn - 0.95, yaw=0.0, h=0.5)
    T("dance_piano", "play", (8.3, yn - 0.4, 1.0), 0.0, what="piano")
    kit.plant(B, 10.0, 18.6, kind="palm", pot="CT_PlasterRed", h=1.8)
    kit.plant(B, 2.85, 18.7, kind="ficus", pot="CT_TileWhite", h=1.8)
    # dance mirrors on the west wall between the doorways; banner and pictures on the east wall
    kit.mirror(B, cl, 13.2, yaw=90.0, zc=1.5, w=1.0, h=1.5)
    kit.mirror(B, cl, 14.8, yaw=90.0, zc=1.5, w=1.0, h=1.5)
    kit.banner(B, cr, 14.2, yaw=270.0, name="poster_sun_1", w=1.2, zc=2.4)
    kit.picture(B, cr, 16.6, yaw=270.0, name="painting_still_life", w=0.8, zc=1.8) if False else None
    kit.poster(B, cr, 16.5, yaw=270.0, name="poster_cons_4", w=0.7, zc=1.8)
    kit.wall_clock(B, cr, 13.0, yaw=270.0, zc=2.6, r=0.22)
    kit.rug(B, 7.6, 12.6, w=0.01, d=0.01, name="rug") if False else None
    for y in (7.5, 12.5, 17.0):
        hang(F, v, 6.0, y, 1.8, lumens=1000, radius=7.5)
    F.border(v["cl"], v["cr"], v["ym"] + 0.25, v["yb"] - 0.3, w=0.3, mat="CT_ConcreteDark")


def hall_d(B, F):
    """Kitchen: a cooking run along the north wall, a prep island, pantry shelves, the staff table."""
    v = VAULTS[3]
    d = B.detail
    T = B.thing
    yn = v["yb"] - 0.3
    run = kit.kitchen_unit(B, 13.2, yn, yaw=0.0, modules="fridge,cupboard,sink,cupboard,stove,cupboard")
    kit.teapot(B, 14.55, yn - 0.3, 0.9, 0.0, mat="CT_SteelWhite")
    kit.plates(B, 12.25, yn - 0.3, 0.9, 0.0, n=4)
    isl = kit.table(B, 13.9, 13.9, w=2.2, d=0.9, h=0.9, mat="CT_Aluminium", kind="steel")
    kit.plates(B, 13.3, 13.9, 0.9, 0.0, n=5)
    kit.kitchen_scales(B, 14.4, 13.8, 0.9, 20.0)
    kit.bottles(B, 14.9, 14.1, 0.9, 0.0, n=2, seed=2)
    kit.shop_shelf(B, 12.4, 8.35, yaw=180.0, w=1.8, h=2.0, goods="groceries", seed=5)
    kit.shop_shelf(B, 14.4, 8.35, yaw=180.0, w=1.8, h=2.0, goods="jars", seed=6)
    kit.crate(B, 11.6, 9.2, yaw=15.0, fill="CT_PlasterRed", seed=2)
    kit.sack(B, 11.2, 9.8, yaw=40.0)
    kit.cardboard_box(B, 16.2, 8.8, yaw=10.0, w=0.45, d=0.35, h=0.35, seed=1)
    st = kit.table(B, 15.6, 10.2, w=1.1, d=0.7, h=0.75, mat=kit.WOODL)
    kit.cup(B, 15.5, 10.2, st["top"], 0.0)
    kit.cup(B, 15.8, 10.1, st["top"], 90.0, mat="CT_PlasterYellow")
    kit.chair(B, 14.85, 10.2, yaw=90.0, kind="school")
    kit.chair(B, 15.6, 9.6, yaw=180.0, kind="school")
    kit.menu_board(B, v["cr"], 10.95, yaw=270.0, name="menu", w=0.7, zc=1.8, seed=3)
    kit.wall_clock(B, 12.5, 8.35, yaw=180.0, zc=2.6, r=0.2) if False else None
    kit.fire_extinguisher(B, v["cr"], 11.2, yaw=270.0) if False else None
    kit.plant(B, 16.2, 15.6, kind="fern", pot="CT_PlasterRed", h=1.2)
    B.spot("cook_1", "work", (13.2, 14.9, 0.0), 180.0)
    B.spot("cook_2", "work", (13.9, 12.9, 0.0), 0.0)
    B.spot("staff_break", "stand", (15.4, 9.4, 0.0), 270.0)
    T("kitchen_stove", "machine", (14.4, yn - 0.3, 0.95), 0.0, what="stove")
    T("kitchen_sink", "machine", (13.2, yn - 0.3, 0.95), 0.0, what="sink")
    T("prep_table", "craft", (13.9, 13.9, 0.95), 0.0, what="cooking")
    T("pantry_shelf", "shelf", (12.4, 8.5, 1.3), 180.0, topic="cooking")
    T("kitchen_menu", "board", (v["cr"] - 0.2, 10.95, 1.8), 270.0, topic="prices")
    T("staff_table", "seat", (14.85, 10.2, 0.45), 90.0, height=0.45)
    hang(F, v, 13.7, 12.0, 0.7, lumens=800, radius=6.0)
    F.border(v["cl"], v["cr"], v["ym"] + 0.25, v["yb"] - 0.3, w=0.25, mat="CT_ConcreteDark")


# ---------------------------------------------------------------------------------------------- second pass: depth
def _mixing_desk(B, x, y, z=0.0):
    """Sound desk of the hall (local piece, yaw 0): a table, a slanted mixer with faders and knobs, a reel-to-reel
    recorder with two reels. The operator sits on the -y side and looks toward +y (the stage)."""
    t = kit.table(B, x, y, z, 0.0, w=1.8, d=0.7, h=0.78, mat=kit.WOODL)
    p = kit._P(B, x, y, z, 0.0, "mixing_desk")
    p.kind = "prop"
    zt = 0.78
    p.hexa("CT_Rubber", [(-0.45, -0.22, zt), (0.45, -0.22, zt), (0.45, 0.22, zt), (-0.45, 0.22, zt)],
           [(-0.45, -0.22, zt + 0.07), (0.45, -0.22, zt + 0.07), (0.45, 0.22, zt + 0.2), (-0.45, 0.22, zt + 0.2)], f="all")
    for k in range(9):                               # faders in the lower half, knobs above
        u = -0.38 + k * 0.095
        for yy, h_ in ((-0.1, 0.004), ):
            zz = zt + 0.07 + (yy + 0.22) / 0.44 * 0.13
            p.box("CT_Aluminium", u - 0.006, u + 0.006, yy - 0.06, yy + 0.06, zz + 0.002, zz + 0.012, f="xXyYZ")
        zz2 = zt + 0.07 + (0.12 + 0.22) / 0.44 * 0.13
        p.cyl("CT_SteelRed" if k % 3 == 0 else "CT_Aluminium", u, 0.12, zz2 + 0.001, zz2 + 0.022, 0.016, 6)
    p.box("CT_Steel", 0.5, 0.9, -0.2, 0.18, zt, zt + 0.16, f="xXyYZ")        # recorder
    for ru in (0.62, 0.78):
        p.cyl("CT_Aluminium", ru, 0.0, zt + 0.16, zt + 0.19, 0.075, 10, top=True)
        p.cyl("CT_Rubber", ru, 0.0, zt + 0.19, zt + 0.2, 0.02, 6)
    p.box("CT_PlasterRed", 0.52, 0.7, -0.205, -0.19, zt + 0.02, zt + 0.06, f="y")
    p.core(-0.9, 0.9, -0.35, 0.35, 0.0, 0.8)
    r = p.done(solid=False, top=zt + 0.2)
    return r


def _lectern_book(B, x, y, yaw):
    """A guest book open on a slanted lectern: front (the reader) = -y of the piece."""
    p = kit._P(B, x, y, 0.0, yaw, "lectern")
    p.box("CT_Wood", -0.12, 0.12, -0.12, 0.12, 0.0, 0.92, f="xXyY")
    p.box("CT_Wood", -0.2, 0.2, -0.2, 0.2, 0.0, 0.05, f="xXyYZ")
    p.hexa("CT_Wood", [(-0.27, -0.2, 0.92), (0.27, -0.2, 0.92), (0.27, 0.2, 0.92), (-0.27, 0.2, 0.92)],
           [(-0.27, -0.2, 1.0), (0.27, -0.2, 1.0), (0.27, 0.2, 1.14), (-0.27, 0.2, 1.14)], f="all")
    def zs(yy):
        return 1.0 + (yy + 0.2) / 0.4 * 0.14 + 0.004
    p.card_tilt("paper_letter", -0.22, 0.0, -0.15, zs(-0.15), 0.15, zs(0.15))
    p.card_tilt("paper_typed", 0.0, 0.22, -0.15, zs(-0.15), 0.15, zs(0.15))
    p.rod("CT_Brass", (0.15, -0.17, zs(-0.17) + 0.01), (0.2, -0.05, zs(-0.05) + 0.01), 0.012, 5)
    return p.done(top=1.14)


def _spotlights(B, xl, xr, y, zbar, n):
    """A steel bar over the stage hung from the vault on rods, with n stage spotlights aimed down toward the front."""
    v = VAULTS[1]
    p = kit._P(B, 0.0, 0.0, 0.0, 0.0, "light_bar")
    p.kind = "hang"
    p.rod("CT_Steel", (xl, y, zbar), (xr, y, zbar), 0.07, 6)
    for xx in (xl + 0.4, xr - 0.4):
        p.rod("CT_Steel", (xx, y, zbar), (xx, y, sof_z(v, xx) + 0.05), 0.02, 4)
    for k in range(n):
        xx = xl + 0.9 + k * (xr - xl - 1.8) / max(1, n - 1)
        p.rod("CT_Steel", (xx, y, zbar - 0.03), (xx, y - 0.06, zbar - 0.12), 0.03, 4)
        p.rod("CT_Rubber", (xx, y + 0.12, zbar + 0.02 - 0.28), (xx, y - 0.22, zbar - 0.52), 0.2, 6, caps=True)
        p.rod("CT_Opal", (xx, y - 0.215, zbar - 0.5), (xx, y - 0.235, zbar - 0.53), 0.15, 6, caps=True)
    return p.done(solid=False)


def _flags(B, x0, x1, y, zh, sag, n, seed=0):
    """A garland of pennants across the dance room between two points at height zh, sagging by `sag`."""
    p = kit._P(B, 0.0, 0.0, 0.0, 0.0, "garland")
    p.kind = "hang"
    mats = ["CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterBlue", "CT_PlasterGreen", "CT_PlasterPink"]
    pts = []
    for k in range(n + 1):
        u = k / n
        pts.append((x0 + (x1 - x0) * u, y, zh - sag * 4 * u * (1 - u)))
    for a, b in zip(pts, pts[1:]):
        p.rod("CT_Paper", a, b, 0.008, 3)
    for k in range(n):
        a, b = pts[k], pts[k + 1]
        mx, mz = (a[0] + b[0]) / 2, (a[2] + b[2]) / 2
        mat = mats[(k + seed) % len(mats)]
        tri = [(a[0] + 0.02, y, a[2] - 0.01), (b[0] - 0.02, y, b[2] - 0.01), (mx, y, mz - 0.26)]
        p.face(mat, [(q[0], q[1] - 0.003, q[2]) for q in tri])
        p.face(mat, [(q[0], q[1] + 0.003, q[2]) for q in tri[::-1]])
    return p.done(solid=False)


def _mirror_ball(B, x, y, zc, ztop):
    p = kit._P(B, x, y, 0.0, 0.0, "mirror_ball")
    p.kind = "hang"
    p.rod("CT_Steel", (0, 0, zc + 0.24), (0, 0, ztop), 0.012, 4)
    prof = [(0.0001, zc - 0.25)]
    for k in range(1, 8):
        a = -math.pi / 2 + k * math.pi / 8
        prof.append((0.25 * math.cos(a), zc + 0.25 * math.sin(a)))
    prof.append((0.0001, zc + 0.25))
    p.lathe("CT_Aluminium", 0, 0, prof, 12, top=False)
    return p.done(solid=False)


def _music_stand(B, x, y, z, yaw):
    p = kit._P(B, x, y, z, yaw, "music_stand")
    p.cyl("CT_Steel", 0, 0, 0.0, 0.02, 0.2, 6)
    p.rod("CT_Steel", (0, 0, 0.02), (0, 0, 1.1), 0.018, 5)
    p.box("CT_Steel", -0.24, 0.24, -0.1, 0.0, 1.08, 1.1, f="xXyYZ")
    p.card_tilt("paper_typed", -0.22, 0.22, -0.11, 1.1, -0.0, 1.38)
    return p.done(solid=False)


def _gallery_light(B, x, y, yaw, zt, w):
    """[wall] A brass picture light over a frame and a small name plate under it (zt = top of the frame)."""
    p = kit._P(B, x, y, 0.0, yaw, "picture_light")
    p.kind = "wall"
    p.box("CT_Brass", -w / 2, w / 2, -0.12, -0.08, zt + 0.08, zt + 0.1, f="xXyYZz")
    p.box("CT_Brass", -0.01, 0.01, -0.08, 0.0, zt + 0.08, zt + 0.1, f="xXZ")
    return p.done()


def _plaque(B, x, y, yaw, zb):
    p = kit._P(B, x, y, 0.0, yaw, "plaque")
    p.kind = "wall"
    p.box("CT_Brass", -0.1, 0.1, -0.012, 0.0, zb - 0.09, zb, f="xXyZz")
    return p.done()


def deepen(B, F):
    """The second pass: the stage's technical side and backstage, the audience hall, the exhibition's lights and plaques,
    the guest book, the dance room's garlands, the bar's back shelf and the kitchen's little things."""
    v = VAULTS[1]
    xc = v["xc"]
    cl, cr = v["cl"], v["cr"]
    T = B.thing
    zs = 1.0
    # ---- the sound desk in the middle of the hall, the operator facing the stage; its lamp
    dx, dy = xc + 3.1, 15.4
    r = _mixing_desk(B, dx, dy)
    kit.table_lamp(B, dx - 0.7, dy + 0.2, 0.78, 0.0, shade="CT_PlasterGreen", light=True)
    kit.chair(B, dx - 0.2, dy - 0.85, yaw=180.0, kind="school")
    kit.chair(B, dx + 0.6, dy - 0.85, yaw=180.0, kind="school")
    T("sound_desk", "machine", (dx, dy - 0.1, 1.05), 0.0, what="mixer")
    B.spot("sound_seat", "seat", (dx - 0.2, dy - 0.85, 0.0), 180.0)
    B.spot("sound_operator", "stand", (dx + 1.35, dy - 0.7, 0.0), 270.0)
    kit.cardboard_box(B, dx - 1.25, dy + 0.1, yaw=20.0, w=0.4, d=0.3, h=0.3, seed=4)
    # ---- stage lighting: a bar of spotlights, the light panel in the east wing, speakers on the stage's front corners
    _spotlights(B, cl + 0.9, cr - 0.9, 19.7, 4.75, 6)
    p = kit._P(B, cr, 19.3, 0.0, 270.0, "light_panel")
    p.kind = "wall"
    p.box("CT_SteelWhite", -0.17, 0.17, -0.03, 0.0, zs + 1.15, zs + 1.65, f="xXyZzY")
    for k in range(6):
        p.box("CT_PlasterRed" if k % 2 else "CT_Aluminium", -0.135 + k * 0.054 - 0.008, -0.135 + k * 0.054 + 0.008, -0.05, -0.03,
              zs + 1.28 + (k % 2) * 0.05, zs + 1.33 + (k % 2) * 0.05 + 0.1, f="xXyYZ")
    p.done()
    T("light_panel", "machine", (cr - 0.15, 19.3, zs + 1.4), 270.0, what="stage_lights")
    for sx in (cl + 1.3, cr - 1.3):
        q = kit._P(B, sx, 19.05, zs, 0.0, "speaker")
        q.box("CT_Rubber", -0.25, 0.25, -0.2, 0.2, 0.0, 0.85, f="xXyYZ")
        q.cyl("CT_Aluminium", 0, -0.205, 0.5, 0.505, 0.14, 10, top=True)
        q.cyl("CT_Steel", 0, -0.215, 0.5, 0.52, 0.035, 8, top=True)
        q.cyl("CT_Aluminium", 0, -0.205, 0.17, 0.175, 0.07, 8, top=True)
        q.done()
    # ---- the premiere seat: the stage seen from the front of the house
    T("premiere_seat", "screen", (xc - 2.7, 18.5, zs + 0.9), 0.0, what="premiere")
    # ---- the cloak room's postcards of the hall
    pc = kit._P(B, -14.9, 11.0, 1.05, 90.0, "postcard_rack")
    pc.kind = "prop"
    pc.box("CT_WoodLight", -0.14, 0.14, -0.07, 0.07, 0.0, 0.03, f="xXyYZ")
    pc.card_tilt("travel_1", -0.12, 0.12, -0.06, 0.04, 0.05, 0.3)
    pc.done()
    T("foyer_postcards", "take", (-14.9, 11.0, 1.15), 90.0, item="postcard")
    # ---- ghost light: the bare lamp left on the stage when everyone has gone
    kit.floor_lamp(B, xc + 0.7, 19.15, zs, 0.0, shade="CT_Paper", light=True)
    T("ghost_light", "board", (xc + 0.7, 19.15, zs + 1.2), 0.0, topic="ghost_light")
    # ---- the audience: a chamber group's chairs and stands in front of the stage; a rehearsal prompter's table
    for k in range(5):
        a = math.radians(-60 + k * 30)
        cx, cy = xc - 2.2 + 2.2 * math.sin(a) * 1.3, 17.2 - 0.5 * math.cos(a)
        kit.chair(B, cx, cy, yaw=180.0, kind="wood")
        _music_stand(B, cx, cy + 0.65, 0.0, 180.0)
        B.spot("quartet_%d" % (k + 1), "seat", (cx, cy, 0.0), 180.0)
    # ---- backstage: costume rail, a folding screen, a props table in the west wing; a dressing table in the east wing
    kit.clothes_rail(B, cl + 0.38, 19.15, zs, 90.0, w=1.0, seed=9)
    T("costume_rail", "board", (cl + 0.4, 19.15, zs + 1.0), 90.0, topic="costumes")
    kit.screen_divider(B, cl + 1.3, 21.9, zs, 90.0)
    pt = kit.table(B, cl + 0.38, 23.1, zs, 90.0, w=1.3, d=0.62, h=0.75, mat=kit.WOODL)
    kit.chess_set(B, cl + 0.38, 22.75, pt["top"], 90.0, board=0.3, seed=2)
    kit.telephone(B, cl + 0.38, 23.45, pt["top"], 90.0)
    kit.globe(B, cl + 0.6, 23.0, pt["top"], 0.0)
    kit.crate(B, cl + 0.5, 21.75, zs, yaw=8.0, fill="CT_PlasterBlue", seed=3)
    T("prop_table", "board", (cl + 0.38, 23.1, zs + 0.9), 90.0, topic="props")
    kit.table(B, cr - 0.3, 22.1, zs, 270.0, w=0.8, d=0.5, h=0.75, mat=kit.WOODL)
    kit.mirror(B, cr, 22.1, zs, 270.0, zc=1.6, w=0.62, h=0.95)
    kit.table_lamp(B, cr - 0.3, 22.4, zs + 0.75, 0.0, light=True)
    kit.stool(B, cr - 0.95, 22.1, zs, 90.0, h=0.5)
    T("makeup_mirror", "machine", (cr - 0.2, 22.1, zs + 1.5), 270.0, what="makeup")
    B.spot("backstage_stand", "stand", (cr - 1.2, 21.0, zs), 90.0)
    B.spot("backstage_prop", "stand", (cl + 1.4, 22.6, zs), 270.0)
    # ---- the lobby: the weekly programme of the evenings, a pennant over the doors
    kit.notice_stand(B, xc + 2.1, 4.95, yaw=0.0, w=1.2, h=0.85, front="notice_0", back="poster_cons_4", seed=2)
    T("programme_wall", "board", (xc + 2.1, 4.95, 1.4), 0.0, topic="programme")
    kit.banner(B, cr, 3.6, yaw=270.0, name="poster_cons_2", w=0.55, zc=2.4)
    kit.notice_board(B, cr, 6.45, yaw=270.0, w=0.55, zc=1.45, seed=12)
    T("rehearsal_board", "board", (cr - 0.2, 6.45, 1.45), 270.0, topic="rehearsal")
    # ---- the foyer: exhibition lights and plaques, a case, a statue, the guest book
    A = VAULTS[0]
    for yy, w in ((9.7, 1.0), (11.25, 1.0), (12.8, 1.0), (16.2, 0.8)):
        _gallery_light(B, A["cr"], yy, 270.0, 2.25, w)
        _plaque(B, A["cr"], yy, 270.0, 1.0)
    for xx, w in ((-15.2, 1.2), (-13.25, 1.4), (-11.3, 1.2)):
        _gallery_light(B, xx, A["yb"] - 0.3, 0.0, 2.2, w)
        _plaque(B, xx, A["yb"] - 0.3, 0.0, 1.0)
    for yy, w in ((10.25, 0.8), (6.3, 0.7), (13.25, 0.7)):
        _gallery_light(B, A["cl"], yy, 90.0, 2.15, w)
        _plaque(B, A["cl"], yy, 90.0, 1.1)
    kit.display_case(B, -11.9, 14.5, yaw=0.0, w=1.2, d=0.6, h=1.0, goods="toys", seed=3)
    T("exhibit_case", "board", (-11.9, 14.5, 0.9), 0.0, topic="case")
    sp = kit._P(B, -10.75, 17.1, 0.0, 0.0, "plinth")
    sp.box("CT_ConcreteWhite", -0.2, 0.2, -0.2, 0.2, 0.0, 0.9, f="xXyYZ")
    sp.lathe("CT_Brass", 0, 0, [(0.12, 0.92), (0.2, 1.05), (0.11, 1.22), (0.19, 1.38), (0.07, 1.55), (0.02, 1.66)], 8, top=True)
    sp.core(-0.18, 0.18, -0.18, 0.18, 0.0, 0.88)
    sp.done(solid=True, top=0.9)
    _lectern_book(B, -12.2, 8.8, 180.0)
    T("guest_book", "board", (-12.2, 8.8, 1.1), 180.0, topic="guestbook")
    # ---- the dance room: pennants across, a mirror ball over the floor, a barre by the mirrors, chalk menu, a back shelf
    _flags(B, 2.75, 10.3, 11.2, 3.2, 0.3, 16, seed=0)
    _flags(B, 2.75, 10.3, 17.6, 3.2, 0.3, 16, seed=2)
    _mirror_ball(B, 7.3, 14.4, 5.0, 8.4)
    C = VAULTS[2]
    kit.ballet_barre(B, C["cl"], 14.0, yaw=90.0, w=2.6, low=True)
    B.spot("barre_1", "stand", (C["cl"] + 1.0, 13.2, 0.0), 90.0)
    B.spot("barre_2", "stand", (C["cl"] + 1.0, 14.8, 0.0), 90.0)
    T("dance_mirror", "board", (C["cl"] + 0.25, 14.0, 1.5), 90.0, topic="dance_mirror")
    T("dance_floor_centre", "sport", (6.9, 13.9, 0.1), 0.0, what="dance_night")
    kit.menu_board(B, C["cr"], 5.9, yaw=270.0, name="menu", w=0.7, zc=1.7, seed=7)
    T("bar_chalk", "board", (C["cr"] - 0.2, 5.9, 1.7), 270.0, topic="bar_chalk")
    bs = kit._P(B, C["cr"], 8.1, 0.0, 270.0, "back_shelf")
    bs.kind = "prop"
    for zt_ in (1.75, 2.1):
        bs.box("CT_WoodLight", -1.3, 1.3, -0.2, 0.0, zt_ - 0.025, zt_, f="xXyYZz")
        for bx in (-1.2, 0.0, 1.2):
            bs.box("CT_Steel", bx - 0.015, bx + 0.015, -0.18, 0.0, zt_ - 0.14, zt_ - 0.025, f="xXyZ")
    bs.done()
    for zt_, sd in ((1.75, 8), (2.1, 9)):
        kit.bottles(B, C["cr"] - 0.12, 7.4 + 0.4 * (sd - 8), zt_, 0.0, n=3, seed=sd)
    # ---- the kitchen: pots on a rail, a samovar and a radio on the staff table
    D = VAULTS[3]
    yn = D["yb"] - 0.3
    kit.samovar(B, 15.55, 10.3, 0.75, 0.0)
    kit.radio(B, 15.95, 10.0, 0.75, 270.0)
    kit.wall_clock(B, 12.4, 8.35, yaw=180.0, zc=2.55, r=0.2)


def build(B):
    walls(B)
    for v in VAULTS:
        shell(B, v)
    mural(B, VAULTS[0], pat_lattice)
    mural(B, VAULTS[2], pat_waves)
    mural(B, VAULTS[3], pat_sun)
    hall_front(B)
    floors(B)
    backs(B)
    F = Furn(B, z0=0.004)
    hall_b(B, F)
    hall_a(B, F)
    hall_c(B, F)
    hall_d(B, F)
    deepen(B, F)
    if os.environ.get("KITCHECK"):
        print("KITCHECK", kit.check(B))
    B.footprint = [(-16.9, 0.4), (16.9, 0.4), (16.9, 24.5), (-16.9, 24.5)]
    B.height = max(v["Hr"] for v in VAULTS) + ZW
    add_surrounds(B, bounds=(-16.9, 16.9, 0.35, 3.0), entrances=["main"], kinds=("street",), seed=3, max_lamps=3, hedges=False)
    B.note("4 parabolic vaults; hall B with stage, C dance room + bar, A foyer, D kitchen")
