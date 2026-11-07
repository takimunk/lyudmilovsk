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
Spots: 60 "seat" in the hall, "work" on stage / bar / kitchen, "stand", "table" + "seat" in the dance room, "counter".
Entrances: "main" (street, double leaves), "staff" (service).
Open hours: about 10..23 (INFO "hours"; the importer's HOURS table has no entry yet, see the kit wishes).
Material requests: none.
"""
import math

from citykit import box, prism, triangulate, ring
from _cons_util import win
from _sheet import thick_sheet
from _surrounds import add_surrounds
from furn_int2 import Furn

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


def hall_b(B, F):
    v = VAULTS[1]
    xc = v["xc"]
    d = B.detail
    # ---- seating: 6 rows of 10, aisle in the middle
    n = 0
    for r in range(6):
        y = 7.8 + 0.95 * r
        for side in (-1, 1):
            for k in range(5):
                x = xc + side * (0.95 + 0.58 * k)
                F.chair(x, y, yaw=180.0, seat="CT_PlasterBlue" if (r + k) % 3 else "CT_Wood")
                n += 1
                B.spot("seat_%d" % n, "seat", (x, y, 0.0), 180.0)
    F.rug(xc - 0.7, xc + 0.7, 4.2, 17.4, mat="CT_PlasterRed")
    # ---- stage
    zs = 1.0
    sx0, sx1, sy0, sy1 = v["cl"], v["cr"], 18.6, v["yb"] - 0.3
    box(B.m, "CT_WoodLight", sx0, sx1, sy0, sy1, 0.0, zs, faces="xXyYZ")
    box(B.m, "CT_Interior", sx0, sx1, sy0, sy0 + 0.02, 0.0, zs, faces="") if False else None
    for xa, xb in ((sx0 + 0.05, sx0 + 0.95), (sx1 - 0.95, sx1 - 0.05)):
        for i in range(7):
            ya = sy0 - 2.1 + i * 0.3
            box(B.m, "CT_Concrete", xa, xb, ya, ya + 0.3, 0.0, zs / 7 * (i + 1), faces="xXyYZ")
    box(d, "CT_PlasterRed", sx0, sx1, sy0 - 0.02, sy0, 0.05, 0.12, faces="y")
    F.border(sx0, sx1, sy0, sy1, w=0.0, mat="CT_ConcreteDark") if False else None
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
    # piano and stools
    px, py = xc + 2.6, 21.0
    F.put(lambda mm: (box(mm, "CT_Steel", -0.75, 0.75, -0.3, 0.3, 0.55, 0.95), box(mm, "CT_Steel", -0.75, 0.75, 0.3, 0.9, 0.55, 0.62),
                      [box(mm, "CT_Steel", sx * 0.65 - 0.03, sx * 0.65 + 0.03, sy * 0.25 - 0.03, sy * 0.25 + 0.03, 0.0, 0.55)
                       for sx in (-1, 1) for sy in (-1, 1)],
                      box(mm, "CT_FrameWhite", -0.7, 0.7, -0.34, -0.3, 0.9, 0.95)), px, py, 0.0)
    for dx in (-1.3,):
        pass
    # spots on stage
    B.spot("stage_a", "work", (xc - 1.8, 21.2, zs), 0.0)
    B.spot("stage_b", "work", (xc + 0.2, 20.6, zs), 0.0)
    B.spot("stage_piano", "work", (px, py - 0.9, zs) if False else (px, py - 0.75, zs), 180.0 if False else 0.0)
    B.spot("stage_mic", "stand", (xc - 0.6, 19.8, zs), 0.0)
    # lobby: benches under the glass, plants, coat stand
    for xx in (xc - 3.4, xc + 3.4):
        F.bench(xx, 4.2, yaw=180.0, w=1.8, back=True)
        B.spot("lobby_bench_%d" % (1 if xx < xc else 2), "bench", (xx, 4.2, 0.0), 180.0)
    for xx in (v["cl"] + 0.6, v["cr"] - 0.6):
        F.plant(xx, 4.1, h=1.8, pot="CT_TileWhite")
    B.spot("lobby_stand_1", "stand", (xc - 1.6, 5.4, 0.0), 0.0)
    B.spot("lobby_stand_2", "stand", (xc + 1.6, 5.6, 0.0), 0.0)
    F.plant(v["cl"] + 0.5, 17.4, h=1.6)
    F.plant(v["cr"] - 0.5, 17.4, h=1.6, pot="CT_TileWhite")
    # side-wall posters (tile panels)
    for yy in (7.0, 11.0, 15.0):
        F.panel(v["cl"] + 0.0, yy, 1.9, 1.4, 1.1, 270.0 + 180.0, mat="CT_Tile", frame="CT_FrameWhite")
    F.border(v["cl"], v["cr"], 3.1 + 0.3, v["yb"] - 0.3, w=0.35, mat="CT_ConcreteDark")
    # lamps hanging from the vault
    for y in (8.0, 12.0, 16.0, 21.0):
        for s in (-1, 1):
            hang(F, v, xc + s * 2.7, y, 2.4, lumens=1500, radius=9.0)


def hall_a(B, F):
    v = VAULTS[0]
    d = B.detail
    # cloak room counter along the west wall
    box(B.m, "CT_WoodLight", -16.55, -15.8, 8.2, 11.4, 0.004, 1.05, faces="xXyYZ")
    box(B.m, "CT_Steel", -16.58, -15.77, 8.15, 11.45, 1.05, 1.08)
    for i in range(6):
        y = 8.45 + i * 0.5
        box(d, "CT_Steel", -16.58, -16.5, y, y + 0.03, 1.0, 2.0)
        box(d, "CT_Steel", -16.58, -16.0, y, y + 0.03, 1.95, 1.98)
    box(d, "CT_Steel", -16.58, -16.54, 8.2, 11.4, 1.9, 1.95)
    for i in range(7):
        y = 8.4 + i * 0.5
        box(d, [("CT_PlasterRed"), ("CT_PlasterBlue"), ("CT_Wood"), ("CT_PlasterGreen")][i % 4], -16.5, -16.1, y, y + 0.3, 1.0, 1.9, faces="xXyYZ")
    B.spot("cloak_clerk", "work", (-16.2, 9.8, 0.0), 90.0)
    B.spot("cloak_counter", "counter", (-15.2, 9.8, 0.0), 270.0)
    # benches and plants
    F.bench(-10.45, 11.2, yaw=270.0, w=1.8, back=True)
    B.spot("foyer_bench_1", "bench", (-10.45, 11.2, 0.0), 270.0)
    F.bench(-15.0, 16.8, yaw=180.0, w=1.8, back=True)
    B.spot("foyer_bench_2", "bench", (-15.0, 16.8, 0.0), 180.0)
    F.bench(-12.0, 6.6, yaw=0.0, w=1.8, back=True)
    F.plant(-16.0, 6.4, h=1.7)
    F.plant(-10.5, 17.6, h=1.7, pot="CT_TileWhite")
    F.plant(-16.0, 17.6, h=1.5)
    F.rug(-14.7, -11.6, 9.0, 14.0, mat="CT_PlasterBlue")
    B.spot("foyer_stand_1", "stand", (-13.2, 12.0, 0.0), 90.0)
    B.spot("foyer_stand_2", "stand", (-12.2, 9.4, 0.0), 0.0)
    F.panel(-9.9, 5.8, 1.9, 1.6, 1.2, 90.0, mat="CT_PlasterYellow", frame="CT_Wood") if False else None
    for xx in (-14.2, -12.4):
        hang(F, v, xx, 12.0, 1.4, lumens=700, radius=6.5)
    F.border(v["cl"], v["cr"], v["ym"] + 0.25, v["yb"] - 0.3, w=0.3, mat="CT_ConcreteDark")


def hall_c(B, F):
    v = VAULTS[2]
    d = B.detail
    # bar along the east wall
    box(B.m, "CT_WoodLight", 8.9, 9.6, 5.8, 10.4, 0.004, 1.05, faces="xXyYZ")
    box(B.m, "CT_Steel", 8.86, 9.64, 5.76, 10.44, 1.05, 1.09)
    box(d, "CT_PlasterRed", 8.86, 8.9, 5.9, 10.3, 0.1, 0.95, faces="x")
    F.shelf(10.4, 8.1, w=4.4, d=0.35, h=2.2, yaw=270.0, rows=4, goods=True, seed=3)
    B.spot("bar_barman", "work", (10.0, 7.6, 0.0), 270.0)
    B.spot("bar_barman2", "work", (10.0, 9.0, 0.0), 270.0)
    for i in range(5):
        y = 6.3 + i * 0.85
        F.stool(8.35, y, seat="CT_PlasterRed" if i % 2 else "CT_PlasterYellow")
        B.spot("bar_stool_%d" % (i + 1), "seat", (8.35, y, 0.0), 90.0)
    B.spot("bar_counter", "counter", (8.5, 10.9, 0.0), 90.0)
    # tables with chairs along the west wall
    t = 0
    for y in (6.2, 8.0, 13.6, 15.4):
        t += 1
        F.round_table(4.2, y, r=0.55, top="CT_WoodLight")
        F.cup(4.1, y, 0.75, mat="CT_PlasterBlue", h=0.2)
        B.spot("c_table_%d" % t, "table", (4.2, y, 0.0), 0.0)
        for i, (cx, cy, yw) in enumerate(((3.2, y, 90.0), (5.2, y, 270.0), (4.2, y + (0.95 if y < 10 else -0.95), 180.0 if y < 10 else 0.0))):
            F.chair(cx, cy, yaw=yw, seat="CT_PlasterYellow")
            B.spot("c_seat_%d_%d" % (t, i + 1), "seat", (cx, cy, 0.0), yw)
    # tiled dance floor
    sq = 0.9
    for i in range(6):
        for j in range(7):
            if (i + j) % 2:
                continue
            x0, y0 = 4.6 + i * sq, 11.6 + j * sq
            box(d, "CT_TileWhite", x0, x0 + sq, y0, y0 + sq, 0.0, 0.014, faces="Z")
            box(d, "CT_Tile", x0 + sq, x0 + 2 * sq, y0, y0 + sq, 0.0, 0.014, faces="Z") if False else None
    for i in range(6):
        for j in range(7):
            if (i + j) % 2 == 0:
                continue
            x0, y0 = 4.6 + i * sq, 11.6 + j * sq
            box(d, "CT_Tile", x0, x0 + sq, y0, y0 + sq, 0.0, 0.014, faces="Z")
    for k, (x, y, yw) in enumerate(((6.0, 14.0, 0.0), (7.0, 15.0, 180.0), (8.0, 13.0, 90.0), (5.6, 16.5, 270.0))):
        B.spot("dance_%d" % (k + 1), "stand", (x, y, 0.0), yw)
    # little band corner
    F.plant(10.0, 18.6, h=1.8)
    F.plant(2.9, 18.6, h=1.8, pot="CT_TileWhite")
    for y in (7.5, 12.5, 17.0):
        hang(F, v, 6.0, y, 1.8, lumens=1000, radius=7.5)
    F.border(v["cl"], v["cr"], v["ym"] + 0.25, v["yb"] - 0.3, w=0.3, mat="CT_ConcreteDark")


def hall_d(B, F):
    v = VAULTS[3]
    d = B.detail
    # kitchen: steel prep table, stove block along the back, shelves
    box(B.m, "CT_Aluminium", 12.4, 14.6, 11.2, 12.0, 0.004, 0.92, faces="xXyYZ")
    box(B.m, "CT_Steel", 12.2, 16.2, 15.2, 16.0, 0.004, 0.95, faces="xXyYZ")
    for x in (12.6, 13.6, 14.6, 15.6):
        box(d, "CT_Frame", x - 0.2, x + 0.2, 15.3, 15.7, 0.95, 0.98)
        box(d, "CT_SteelRed", x - 0.04, x + 0.04, 15.15, 15.2, 0.6, 0.8, faces="y")
    F.shelf(11.2, 14.2, w=3.0, d=0.35, h=2.1, yaw=90.0, rows=4, goods=True, seed=5)
    F.shelf(11.2, 10.0, w=2.2, d=0.35, h=2.1, yaw=90.0, rows=4, goods=True, seed=6)
    B.spot("cook_1", "work", (13.5, 14.5, 0.0), 180.0)
    B.spot("cook_2", "work", (13.5, 10.5, 0.0), 180.0)
    B.spot("staff_break", "stand", (15.4, 9.4, 0.0), 270.0)
    F.stool(15.6, 12.0)
    F.plant(16.1, 8.8, h=1.2)
    hang(F, v, 13.7, 12.0, 0.7, lumens=800, radius=6.0)
    F.border(v["cl"], v["cr"], v["ym"] + 0.25, v["yb"] - 0.3, w=0.25, mat="CT_ConcreteDark")


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
    B.footprint = [(-16.9, 0.4), (16.9, 0.4), (16.9, 24.5), (-16.9, 24.5)]
    B.height = max(v["Hr"] for v in VAULTS) + ZW
    add_surrounds(B, bounds=(-16.9, 16.9, 0.35, 3.0), entrances=["main"], kinds=("street",), seed=3, max_lamps=3, hedges=False)
    B.note("4 parabolic vaults; hall B with stage, C dance room + bar, A foyer, D kitchen")
