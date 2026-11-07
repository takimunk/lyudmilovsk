"""Department store (Univermag): a Soviet-modernist department store on the main square. An ORIGINAL design in the
spirit of 1960s-70s Soviet "Univermag" / "Detsky Mir" buildings (glazed ground floor behind an arcade of piers,
decorative mosaic-and-grille upper floor, giant roof sign), not a copy of one.

Plot 45 x 30 m, 14 m high. Three levels: ground shop hall (z 0-4.6), first-floor shop hall (z 4.9-9.4), roof
(z 9.7) with a 3.4 m red sign board carrying abstract neon "letters" (7-segment style glyphs, no text).
Front (y = 0): an arcade 1.4 m deep with ten slender piers carrying the upper storey; behind it the glass wall of
the ground floor (display windows, double doors: entrances "main" at x 0, "west" x -9, "east" x +9, each with a
red canopy). The upper storey is a blue mosaic wall (CT_Tile) in 15 modules of 3 m between white pilasters: odd modules
carry a window behind a concrete grille of fins, even modules a decorative tile panel (white frame, red diamond).
Enterable: both shop halls (hollow, CT_Floor / CT_Interior, 12 round columns, wall shelves, 4 sales counters per floor
("counter" for the customer side + "work" for the seller behind it), 2 cashier desks near the doors, showcase
"stand" islands, a U-shaped stair in the back (rise 0.163, going 0.3) with a slab opening and railings.
Spots: 8 sales counters + 8 sellers, 2 cashiers (counter + work), 5 stands, benches for the queue. No flats
(3 storeys of shop): the brief's "2 shop storeys + flats" was not used.
Ideas for later: escalator, a cafeteria on the 1st floor, a flats storey above the back half, rooftop terrace.
Kit wishes: B.facade with a different material for the inner face (done here with wall()).
"""
import math
import random

from citykit import box, prism, ring, Mesh, Facade, wall_with_holes, reveal
from _furn1 import rbox, cyl, blob, shelf_unit, goods_row, clothes_rack, mannequin, till

INFO = {
    "name_ru": "Универмаг", "name_en": "Department Store",
    "style": "Soviet modernism", "use": "public",
    "inspired_by": "Soviet Univermag buildings of the 1960s-70s (arcade + glazed ground floor, mosaic/grille upper floor, "
                   "roof sign)",
}

HW, D = 22.5, 30.0                     # half width, depth
T = 0.40                               # wall thickness
Z_SLAB0, Z_SLAB1 = 4.6, 4.9            # first-floor slab
Z_ROOF0, Z_ROOF1 = 9.4, 9.7            # roof slab
ARC = 1.4                              # arcade depth
GLASS_WALL = 0.30


def wall(B, p0, p1, z0, z1, mat, inner_mat, holes, depth=T, reveal_mat=None):
    """Like B.facade but the inner face has its own material. Returns the Facade."""
    F = Facade(p0, p1)
    m = Mesh()
    wall_with_holes(m, mat, F.length, z0, z1, list(holes))
    for (u0, u1, h0, h1) in holes:
        reveal(m, reveal_mat or mat, u0, u1, h0, h1, depth)
    inner = Mesh()
    wall_with_holes(inner, inner_mat, F.length, z0, z1, list(holes))
    inner.verts = [(u, -depth, z) for (u, _, z) in inner.verts]
    inner.faces = [(mt, list(reversed(i)), list(reversed(uv))) for mt, i, uv in inner.faces]
    m.extend(inner)
    B.m.extend(F.apply(m))
    return F


def yaw_to(a, b):
    return math.degrees(math.atan2(b[0] - a[0], -(b[1] - a[1])))


def sheared_bar(m, mat, x0, x1, ya, za, yb, zb, t):
    s = (zb - za) / (yb - ya)
    tmp = Mesh()
    box(tmp, mat, x0, x1, ya, yb, 0.0, t)
    tmp.verts = [(x, y, z + za + s * (y - ya)) for x, y, z in tmp.verts]
    m.extend(tmp)


SEG = {  # 7-segment: top, upper-left, upper-right, mid, lower-left, lower-right, bottom
    0: "abcdef", 1: "bc", 2: "abdeg", 3: "abcdg", 4: "bcfg", 5: "acdfg", 6: "acdefg", 7: "abc", 8: "abcdefg", 9: "abcdfg"}


def glyph(d, x, zc, digit, y):
    """A neon 7-segment glyph (width 1.0, height 2.0) facing -y at plane y."""
    w, h, t = 1.0, 2.0, 0.2
    z0 = zc - h / 2
    segs = {
        "a": (x - w / 2, x + w / 2, z0 + h - t, z0 + h), "g": (x - w / 2, x + w / 2, z0 + h / 2 - t / 2, z0 + h / 2 + t / 2),
        "d": (x - w / 2, x + w / 2, z0, z0 + t),
        "f": (x - w / 2, x - w / 2 + t, z0 + h / 2, z0 + h), "b": (x + w / 2 - t, x + w / 2, z0 + h / 2, z0 + h),
        "e": (x - w / 2, x - w / 2 + t, z0, z0 + h / 2), "c": (x + w / 2 - t, x + w / 2, z0, z0 + h / 2),
    }
    for k in SEG[digit]:
        a, b, c, e = segs[k]
        box(d, "CT_Neon", a, b, y - 0.07, y, c, e, faces="y")
        box(d, "CT_Neon", a, b, y - 0.07, y, c, e, faces="xXzZ")


def build(B):
    rnd = random.Random(1969)
    m, d = B.m, B.detail
    xi = HW - T                                           # inner x extent 22.1

    # ------------------------------------------------------------ slabs
    hole = (-1.8, 1.8, 21.0, 27.3)
    for (x0, x1, y0, y1) in ((-xi, hole[0], T, D - T), (hole[1], xi, T, D - T),
                             (hole[0], hole[1], T, hole[2]), (hole[0], hole[1], hole[3], D - T)):
        box(m, "CT_Interior", x0, x1, y0, y1, Z_SLAB0, Z_SLAB1, faces="xXyYz")
        box(m, "CT_Floor", x0, x1, y0, y1, Z_SLAB0, Z_SLAB1, faces="Z")
    # roof slab
    box(m, "CT_Interior", -xi, xi, T, D - T, Z_ROOF0, Z_ROOF1, faces="z")
    box(m, "CT_Roof", -xi, xi, T, D - T, Z_ROOF0, Z_ROOF1, faces="Z")
    # ground floor
    m.face("CT_Floor", [(-xi, ARC + GLASS_WALL, 0.03), (xi, ARC + GLASS_WALL, 0.03), (xi, D - T, 0.03), (-xi, D - T, 0.03)])
    m.face("CT_Paving", [(-xi, 0.0, 0.03), (xi, 0.0, 0.03), (xi, ARC, 0.03), (-xi, ARC, 0.03)])

    # ------------------------------------------------------------ ground floor front: arcade + glass wall
    piers = [s * (2.25 + 4.5 * j) for j in range(5) for s in (-1, 1)]
    for px in piers:
        box(m, "CT_ConcreteWhite", px - 0.35, px + 0.35, 0.0, ARC, 0.0, Z_SLAB0)
    xs = -xi
    gh = []                                              # holes in glass wall: (x0, x1, z0, z1, door)
    bays = sorted(piers)
    edges = [-xi] + [v for p in bays for v in (p - 0.4, p + 0.4)] + [xi]
    bay_ranges = [(edges[2 * i] + (0.05 if i == 0 else 0), edges[2 * i + 1]) for i in range(len(bays) + 1)]
    doors = {"west": -9.0, "main": 0.0, "east": 9.0}
    for (a, b) in bay_ranges:
        a += 0.05
        b -= 0.05
        dc = [c for c in doors.values() if a < c < b]
        if dc:
            c = dc[0]
            gh.append((c - 1.2, c + 1.2, 0.0, 2.45, "door"))
            gh.append((c - 1.2, c + 1.2, 2.6, 4.25, "glass"))
            if c - 1.2 - a > 0.3:
                gh.append((a, c - 1.25, 0.5, 4.25, "glass"))
                gh.append((c + 1.25, b, 0.5, 4.25, "glass"))
        else:
            n = max(1, int(round((b - a) / 3.7)))
            for i in range(n):
                gh.append((a + (b - a) * i / n + (0.05 if i else 0), a + (b - a) * (i + 1) / n - (0.05 if i < n - 1 else 0),
                           0.5, 4.25, "glass"))
    holes = [(x0 - xs, x1 - xs, z0, z1) for x0, x1, z0, z1, k in gh]
    F = wall(B, (-xi, ARC), (xi, ARC), 0.0, Z_SLAB0, "CT_ConcreteWhite", "CT_Interior", holes, depth=GLASS_WALL)
    for x0, x1, z0, z1, k in gh:
        if k == "glass":
            B.facade_window(F, x0 - xs, x1 - xs, z0, z1, kind="public", cols=max(1, int((x1 - x0) / 1.25)), rows=1, inset=0.12)
    for name, c in doors.items():
        B.entrance(name, (c, ARC, 0.0), 0.0, width=2.4, height=2.45, kind="street", leaves=2)
        # red canopy over the door on two slim posts
        w = 3.2 if name == "main" else 2.7
        box(m, "CT_PlasterRed", c - w, c + w, -1.0, ARC - 0.02, 3.3, 3.55)
        for sx in (-1, 1):
            B.column_round(c + sx * (w - 0.25), -0.75, 0.0, 3.3, 0.07, mat="CT_Steel", segs=8, m=m)

    # ------------------------------------------------------------ upper front: mosaic modules + grille
    mods = 15
    mw = 2 * HW / mods
    hu = []
    wins = []
    for i in range(mods):
        if i % 2 == 0:
            u0 = i * mw + 0.45
            u1 = (i + 1) * mw - 0.45
            hu.append((u0, u1, 5.6, 9.05))
            wins.append((u0, u1))
    F = wall(B, (-HW, 0.0), (HW, 0.0), Z_SLAB0, Z_ROOF1, "CT_Tile", "CT_Interior", hu, depth=T, reveal_mat="CT_ConcreteWhite")
    for u0, u1 in wins:
        B.facade_window(F, u0, u1, 5.6, 9.05, kind="public", cols=3, rows=1, inset=0.15)
    for i in range(mods + 1):
        x = -HW + i * mw
        box(d, "CT_ConcreteWhite", x - 0.16, x + 0.16, -0.38, 0.0, Z_SLAB0, Z_ROOF1 + 0.5)
    for i in range(mods):
        xm = -HW + i * mw
        if i % 2 == 0:
            # grille of fins in front of the window
            for j in range(7):
                fx = xm + 0.55 + j * ((mw - 1.1) / 6)
                box(d, "CT_Louvre", fx - 0.06, fx + 0.06, -0.36, -0.02, 5.4, 9.25)
            box(d, "CT_Louvre", xm + 0.45, xm + mw - 0.45, -0.36, -0.02, 5.4, 5.55)
            box(d, "CT_Louvre", xm + 0.45, xm + mw - 0.45, -0.36, -0.02, 9.1, 9.25)
        else:
            cx_ = xm + mw / 2
            zc = 7.2
            d.face("CT_TileWhite", [(cx_ - 1.1, -0.03, zc - 1.9), (cx_ + 1.1, -0.03, zc - 1.9), (cx_ + 1.1, -0.03, zc + 1.9), (cx_ - 1.1, -0.03, zc + 1.9)])
            d.face("CT_Tile", [(cx_ - 0.9, -0.05, zc - 1.7), (cx_ + 0.9, -0.05, zc - 1.7), (cx_ + 0.9, -0.05, zc + 1.7), (cx_ - 0.9, -0.05, zc + 1.7)])
            d.face("CT_PlasterRed", [(cx_, -0.07, zc - 1.5), (cx_ + 0.8, -0.07, zc), (cx_, -0.07, zc + 1.5), (cx_ - 0.8, -0.07, zc)])
            d.face("CT_TileWhite", [(cx_, -0.09, zc - 0.6), (cx_ + 0.35, -0.09, zc), (cx_, -0.09, zc + 0.6), (cx_ - 0.35, -0.09, zc)])
    # frieze under the sign: a white band over the upper storey
    box(m, "CT_ConcreteWhite", -HW - 0.2, HW + 0.2, -0.45, T, Z_ROOF1, Z_ROOF1 + 0.6)

    # ------------------------------------------------------------ sides and back
    side_w = [(2.0 + 4.6 * i, 4.2 + 4.6 * i) for i in range(6)]
    for p0, p1, nm in (((HW, 0.0), (HW, D), "E"), ((-HW, D), (-HW, 0.0), "W")):
        holes = []
        for a, b in side_w:
            holes.append((a, b, 6.2, 8.6))
            holes.append((a, b, 2.8, 4.2))
        F = wall(B, p0, p1, 0.0, Z_ROOF1, "CT_ConcreteWhite", "CT_Interior", holes)
        for a, b in side_w:
            B.facade_window(F, a, b, 6.2, 8.6, kind="public", cols=2, inset=0.15)
            B.facade_window(F, a, b, 2.8, 4.2, kind="public", cols=2, inset=0.15)
        # tile band
        for i in range(0, 1):
            pass
    holes = [(a + 18, b + 18, 6.2, 8.6) for a, b in side_w[:2]] + [(a - 0.5, b - 0.5, 6.2, 8.6) for a, b in side_w[:2]]
    holes = [(2.0 + 9.0 * i, 4.4 + 9.0 * i, 6.2, 8.6) for i in range(5)]
    F = wall(B, (HW, D), (-HW, D), 0.0, Z_ROOF1, "CT_ConcreteWhite", "CT_Interior", holes)
    for h in holes:
        B.facade_window(F, h[0], h[1], h[2], h[3], kind="public", cols=2, inset=0.15)
    # parapets
    box(m, "CT_ConcreteWhite", -HW, HW, D - T, D, Z_ROOF1, Z_ROOF1 + 0.6)
    box(m, "CT_ConcreteWhite", HW - T, HW, T, D - T, Z_ROOF1, Z_ROOF1 + 0.6)
    box(m, "CT_ConcreteWhite", -HW, -HW + T, T, D - T, Z_ROOF1, Z_ROOF1 + 0.6)

    # ------------------------------------------------------------ roof sign
    sx0, sx1 = -14.0, 14.0
    zb0, zb1 = 10.4, 13.8
    yf = 1.2
    box(m, "CT_PlasterRed", sx0, sx1, yf, yf + 0.35, zb0, zb1)
    box(m, "CT_ConcreteWhite", sx0 - 0.15, sx1 + 0.15, yf - 0.05, yf + 0.4, zb1, zb1 + 0.0 + 0.02, faces="Z") if False else None
    # frame lines on the board face
    box(d, "CT_SteelWhite", sx0 + 0.3, sx1 - 0.3, yf - 0.05, yf, zb1 - 0.35, zb1 - 0.25, faces="y")
    box(d, "CT_SteelWhite", sx0 + 0.3, sx1 - 0.3, yf - 0.05, yf, zb0 + 0.25, zb0 + 0.35, faces="y")
    digits = [rnd.randrange(10) for _ in range(16)]
    for i, dg in enumerate(digits):
        glyph(d, sx0 + 1.4 + i * 1.65, (zb0 + zb1) / 2, dg, yf)
    # posts and back braces
    for px in (-13.0, -8.0, -3.0, 3.0, 8.0, 13.0):
        box(m, "CT_Steel", px - 0.12, px + 0.12, yf + 0.05, yf + 0.3, Z_ROOF1, zb0 + 0.1)
    for px in (-13.0, -6.0, 0.0, 6.0, 13.0):
        sheared_bar(d, "CT_Steel", px - 0.05, px + 0.05, 5.0, Z_ROOF1, yf + 0.38, zb0 + 2.2, 0.1)

    # ------------------------------------------------------------ interior: columns, shelves, counters
    for cxx in (-16.0, -8.0, 0.0, 8.0, 16.0):
        for cyy in (10.0, 20.0):
            if cxx == 0.0 and cyy == 20.0:
                continue
            B.column_round(cxx, cyy, 0.03, Z_SLAB0, 0.3, segs=10, m=m)
            B.column_round(cxx, cyy, Z_SLAB1, Z_ROOF0, 0.3, segs=10, m=m)
    # stair: two flights side by side, solid, with a landing at the back
    n = 15
    r_ = (Z_SLAB1 - 0.03) / 30.0
    g = 0.30
    ya = hole[2]
    for i in range(n):                                   # flight A up along +y at x -1.8..-0.1
        box(m, "CT_Concrete", -1.8, -0.1, ya + i * g, ya + (i + 1) * g, 0.03, 0.03 + r_ * (i + 1))
    yl = ya + n * g
    box(m, "CT_Concrete", -1.8, 1.8, yl, hole[3] - 0.01, 0.03, 0.03 + r_ * n)
    for i in range(n):                                   # flight B back along -y at x 0.1..1.8
        yb1 = yl - i * g
        box(m, "CT_Concrete", 0.1, 1.8, yb1 - g, yb1, 0.03, 0.03 + r_ * (n + i + 1))
    box(m, "CT_ConcreteWhite", -0.1, 0.1, ya, hole[3] - 0.01, 0.03, Z_SLAB1 + 0.9)
    # railings round the opening upstairs
    B.railing([(0.1, hole[2] + 0.05), (-1.75, hole[2] + 0.05), (-1.75, hole[3] - 0.05), (1.75, hole[3] - 0.05),
               (1.75, hole[2] + 0.4)], Z_SLAB1, h=1.0, post=1.5)

    hall(B, rnd, 0.03, "g", Z_SLAB0)
    hall(B, rnd, Z_SLAB1, "f1", Z_ROOF0)
    _ground_extras(B, rnd)
    _first_extras(B, rnd)
    _stair_rails(B, n, g, r_, ya, yl, hole)
    # lamps: 6 x 3 per floor, 0.3 m under each ceiling (none under the stair opening)
    for zc in (Z_SLAB0, Z_ROOF0):
        for lx in (-18.4, -11.0, -3.7, 3.7, 11.0, 18.4):
            for ly in (8.0, 16.5, 25.0):
                B.lamp((lx, ly, zc - 0.3), lumens=1500, colour="#FFD2A0", radius=10.5)

    B.footprint = [(-HW, -1.0), (HW, -1.0), (HW, D), (-HW, D)]
    B.height = 14.0
    B.note("2 shop halls + roof sign; stair hole at x -1.8..1.8, y 21..27.3; entrances main/west/east")


WARM = ["CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterBlue", "CT_PlasterGreen", "CT_PlasterPink", "CT_Plaster"]
COOL = ["CT_PlasterBlue", "CT_Plaster", "CT_TileWhite", "CT_PlasterGreen", "CT_Tile"]
SHOES = ["CT_Door", "CT_Steel", "CT_PlasterRed", "CT_WoodLight", "CT_PlasterBlue", "CT_Plaster", "CT_PlasterYellow"]
THEMES = [WARM, COOL, ["CT_PlasterPink", "CT_PlasterRed", "CT_Plaster", "CT_WoodLight"],
          ["CT_Steel", "CT_Aluminium", "CT_PlasterYellow", "CT_PlasterRed"]]
GARMENT = ["CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterGreen", "CT_PlasterYellow", "CT_PlasterPink", "CT_Steel",
           "CT_PlasterGrey", "CT_Plaster", "CT_Tile"]


def hall(B, rnd, zf, tag, zc):
    m, d = B.m, B.detail
    xi = HW - T
    ground = tag == "g"
    # wall shelving (side walls and back wall), with goods of many colours
    hs = 2.2 if ground else 1.1
    lv = 5 if ground else 3
    for sx in (-1, 1):
        xc = sx * (xi - 0.25)
        for k in range(3):
            y0, y1 = 5.0 + k * 8.5, 11.0 + k * 8.5
            toys = ground and sx == 1 and k == 1
            hh, ll = (1.2, 3) if toys else (hs, lv)
            p0, p1 = ((xc, y1), (xc, y0)) if sx < 0 else ((xc, y0), (xc, y1))
            shelf_unit(d, rnd, p0, p1, zf, hh, 0.5, ll, THEMES[(k + (sx > 0) * 2) % 4] if not toys else
                       ["CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterBlue", "CT_PlasterGreen"], hmax=0.4)
            # a thin solid backing so people cannot stand inside the unit
            box(m, "CT_Plaster", xc - 0.25 if sx > 0 else xc - 0.25, xc + 0.25, y0, y1, zf, zf + 0.12, faces="xXyYZ")
    for (xa, xb) in ((-3.0, -20.0), (20.0, 3.0)):
        shelf_unit(d, rnd, (xa, D - T - 0.25), (xb, D - T - 0.25), zf, hs, 0.5, lv, THEMES[(int(xa) + 3) % 4], hmax=0.4)
        box(m, "CT_Plaster", min(xa, xb), max(xa, xb), D - T - 0.5, D - T, zf, zf + 0.12, faces="xXyZ")
    # sales counters + sellers (goods on the counter top, back shelving behind the seller)
    pos = [(-12.0, 12.5), (-12.0, 22.5), (12.0, 12.5), (12.0, 22.5)]
    for ci, (cx_, cy_) in enumerate(pos):
        box(m, "CT_WoodLight", cx_ - 2.5, cx_ + 2.5, cy_ - 0.45, cy_ + 0.45, zf, zf + 1.05, faces="xXyYZ")
        box(d, "CT_ConcreteDark", cx_ - 2.45, cx_ + 2.45, cy_ - 0.5, cy_ - 0.45, zf + 0.05, zf + 0.12)    # kick plate
        box(d, "CT_Plaster", cx_ - 2.5, cx_ + 2.5, cy_ - 0.5, cy_ + 0.5, zf + 1.05, zf + 1.08)          # top plate
        goods_row(d, rnd, (cx_ - 2.3, cy_ - 0.2), (cx_ - 0.5, cy_ - 0.2), zf + 1.08, 0.3, 0.3, THEMES[ci % 4], hmin=0.1)
        goods_row(d, rnd, (cx_ + 0.6, cy_ - 0.2), (cx_ + 2.3, cy_ - 0.2), zf + 1.08, 0.3, 0.25, THEMES[(ci + 1) % 4], hmin=0.1)
        till(d, cx_, cy_ + 0.18, zf + 1.08)
        shelf_unit(d, rnd, (cx_ + 2.4, cy_ + 1.75), (cx_ - 2.4, cy_ + 1.75), zf, 2.0, 0.4, 4, THEMES[(ci + 2) % 4], hmax=0.3)
        box(m, "CT_Plaster", cx_ - 2.4, cx_ + 2.4, cy_ + 1.55, cy_ + 1.95, zf, zf + 0.1, faces="xXyYZ")
        B.spot("%s_counter_%d" % (tag, ci + 1), "counter", (cx_, cy_ - 1.0, zf), 180.0)
        B.spot("%s_seller_%d" % (tag, ci + 1), "work", (cx_, cy_ + 0.9, zf), 0.0)
    # display islands (goods on a red plinth)
    for k, (cx_, cy_) in enumerate(((-5.0, 15.5), (5.0, 15.5), (0.0, 7.5))):
        box(m, "CT_WoodLight", cx_ - 1.2, cx_ + 1.2, cy_ - 0.6, cy_ + 0.6, zf, zf + 0.9, faces="xXyYZ")
        box(d, "CT_PlasterRed", cx_ - 1.0, cx_ + 1.0, cy_ - 0.4, cy_ + 0.4, zf + 0.9, zf + 1.0)
        goods_row(d, rnd, (cx_ - 0.9, cy_ - 0.2), (cx_ + 0.9, cy_ - 0.2), zf + 1.0, 0.25, 0.32, THEMES[(k + 1) % 4], hmin=0.12)
        goods_row(d, rnd, (cx_ - 0.9, cy_ + 0.2), (cx_ + 0.9, cy_ + 0.2), zf + 1.0, 0.25, 0.4, THEMES[(k + 2) % 4], hmin=0.12)
        B.spot("%s_stand_%d" % (tag, k + 1), "stand", (cx_, cy_ - 1.0, zf), 180.0)
    # hanging department boards
    zs = zc - 0.9
    spots = {"g": [(-18.0, 8.0, "CT_PlasterBlue", 2.6), (-18.0, 18.0, "CT_PlasterRed", 2.6), (18.0, 8.0, "CT_PlasterYellow", 2.6),
                   (18.0, 18.0, "CT_PlasterGreen", 2.6)],
             "f1": [(-18.0, 14.0, "CT_PlasterRed", 2.6), (14.0, 8.0, "CT_PlasterBlue", 2.6), (16.0, 20.0, "CT_PlasterYellow", 2.6)]}[tag]
    for bx, by, mat, w in spots:
        box(d, mat, bx - w / 2, bx + w / 2, by - 0.03, by + 0.03, zs - 0.3, zs + 0.3)
        box(d, "CT_Plaster", bx - w / 2 + 0.15, bx + w / 2 - 0.15, by - 0.045, by - 0.03, zs - 0.12, zs + 0.12)
        for sx in (-1, 1):
            box(d, "CT_Steel", bx + sx * (w / 2 - 0.2) - 0.01, bx + sx * (w / 2 - 0.2) + 0.01, by - 0.01, by + 0.01, zs + 0.3, zc)


def _ground_extras(B, rnd):
    m, d = B.m, B.detail
    zf = 0.03
    # red runner from the main door to the stair
    box(d, "CT_PlasterRed", -1.1, 1.1, 1.8, 20.5, zf, zf + 0.012, faces="Z")
    # cashier desks near the doors: till, bag stand, conveyor strip
    for k, cx_ in enumerate((-4.6, 4.6)):
        cy_ = 6.0
        box(m, "CT_WoodLight", cx_ - 0.8, cx_ + 0.8, cy_ - 0.35, cy_ + 0.35, zf, 1.0, faces="xXyYZ")
        box(d, "CT_Plaster", cx_ - 0.85, cx_ + 0.85, cy_ - 0.4, cy_ + 0.4, 1.0, 1.03)
        till(d, cx_ + 0.3, cy_ + 0.15, 1.03)
        box(d, "CT_Steel", cx_ - 0.7, cx_ - 0.1, cy_ - 0.25, cy_ + 0.05, 1.03, 1.05)               # scales
        goods_row(d, rnd, (cx_ - 0.7, cy_ - 0.15), (cx_ - 0.2, cy_ - 0.15), 1.05, 0.2, 0.2, WARM, hmin=0.1)
        box(m, "CT_Aluminium", cx_ - 1.0, cx_ - 0.95, cy_ + 0.5, cy_ + 0.95, zf, 1.25, faces="xXyYZ") if False else None
        B.spot("cashier_%d" % (k + 1), "counter", (cx_, cy_ - 0.9, zf), 180.0)
        B.spot("cashier_work_%d" % (k + 1), "work", (cx_, cy_ + 0.8, zf), 0.0)
    for k, cx_ in enumerate((-15.0, 15.0)):
        _bench(d, cx_, 4.0, zf)
        B.spot("queue_bench_%d" % (k + 1), "bench", (cx_, 4.0, zf), 0.0)
    # window mannequins behind the shop glass
    for i, x in enumerate((-17.0, -13.5, -4.5, 4.5, 13.5, 17.0)):
        mannequin(d, x, 2.6, zf, ["CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterGreen", "CT_PlasterPink", "CT_PlasterYellow",
                                  "CT_SteelRed"][i])
    # shoe department (west front): tiered stands with pairs, try-on stools, a mirror
    for sy in (6.0, 9.2):
        for t, (hx, hy, h) in enumerate(((1.4, 0.55, 0.25), (1.1, 0.42, 0.5), (0.8, 0.3, 0.75))):
            rbox(m, "CT_WoodLight", -17.6, sy, zf, zf + h, hx, hy, math.pi / 2 * 0 + 0.0)
        for t, h in enumerate((0.25, 0.5, 0.75)):
            hx = (1.4, 1.1, 0.8)[t] - 0.1
            goods_row(d, rnd, (-17.6 - hx, sy), (-17.6 + hx, sy), zf + h, 0.22, 0.12, SHOES, hmin=0.07, wmin=0.12, wmax=0.2)
    for sy in (7.6,):
        for sx in (-19.8, -15.4):
            box(d, "CT_WoodLight", sx - 0.5, sx + 0.5, sy - 0.2, sy + 0.2, zf, zf + 0.42)
            box(d, "CT_SteelRed", sx - 0.5, sx + 0.5, sy - 0.2, sy + 0.2, zf + 0.42, zf + 0.48)
    box(d, "CT_Aluminium", -21.5, -21.2, 9.5, 10.9, zf + 0.2, zf + 2.0)                                # mirror on a post
    # clothes racks (west middle), two columns
    for cx_ in (-19.4, -16.8):
        for ry in (12.6, 15.2, 17.8, 20.4, 23.0, 25.6):
            clothes_rack(d, rnd, cx_, ry, zf, 2.0, math.pi / 2, GARMENT)
    # toys corner (east middle): play mat, bricks, bears, toy house, low tables
    rbox(d, "CT_PlasterBlue", 18.5, 21.0, zf, zf + 0.015, 2.8, 2.2)
    rbox(d, "CT_PlasterYellow", 18.5, 21.0, zf + 0.015, zf + 0.03, 1.2, 1.0)
    for i in range(14):
        a = rnd.uniform(0, 6.28)
        r = rnd.uniform(0.2, 2.0)
        rbox(d, rnd.choice(WARM[:5]), 18.5 + r * math.cos(a) * 1.3, 21.0 + r * math.sin(a), zf + 0.03, zf + 0.03 + rnd.uniform(0.1, 0.3),
             0.1, 0.1, a)
    for bx, by, c in ((17.3, 22.0, "CT_WoodLight"), (19.8, 20.0, "CT_Door"), (18.5, 21.6, "CT_PlasterPink")):
        blob(d, c, bx, by, zf + 0.35, 0.22, 7)
        blob(d, c, bx, by, zf + 0.7, 0.14, 6)
    rbox(m, "CT_PlasterYellow", 20.4, 23.6, zf, zf + 1.1, 0.7, 0.7)                                       # toy house
    prism(d, "CT_PlasterRed", [(19.55, 22.85), (21.25, 22.85), (21.25, 24.35), (19.55, 24.35)], zf + 1.1, zf + 1.3, top=True)
    for tx in (16.3,):
        box(m, "CT_WoodLight", tx - 0.5, tx + 0.5, 15.6, 17.2, zf, zf + 0.5, faces="xXyYZ")
        goods_row(d, rnd, (tx - 0.4, 16.0), (tx + 0.4, 16.0), zf + 0.5, 0.25, 0.3, WARM, hmin=0.15)
    # household goods (east front): pots and pans on tiers
    for sy in (6.0, 9.2):
        box(m, "CT_WoodLight", 16.2, 20.2, sy - 0.5, sy + 0.5, zf, zf + 0.4, faces="xXyYZ")
        box(m, "CT_WoodLight", 17.0, 19.4, sy - 0.3, sy + 0.3, zf + 0.4, zf + 0.8, faces="xXyYZ")
        for i in range(6):
            cyl(d, ("CT_Aluminium", "CT_PlasterRed", "CT_PlasterBlue", "CT_Steel")[i % 4], 16.5 + i * 0.6, sy, zf + 0.4, zf + 0.6, 0.17, 7)
        for i in range(4):
            cyl(d, ("CT_PlasterYellow", "CT_Aluminium", "CT_PlasterGreen", "CT_PlasterRed")[i % 4], 17.3 + i * 0.6, sy, zf + 0.8, zf + 1.0, 0.16, 7)


def _first_extras(B, rnd):
    m, d = B.m, B.detail
    zf = Z_SLAB1
    # first-floor cashier desk by the stair
    cx_, cy_ = -6.0, 24.0
    box(m, "CT_WoodLight", cx_ - 1.0, cx_ + 1.0, cy_ - 0.35, cy_ + 0.35, zf, zf + 1.0, faces="xXyYZ")
    box(d, "CT_Plaster", cx_ - 1.05, cx_ + 1.05, cy_ - 0.4, cy_ + 0.4, zf + 1.0, zf + 1.03)
    till(d, cx_ + 0.4, cy_ + 0.15, zf + 1.03)
    B.spot("f1_cashier", "counter", (cx_, cy_ - 0.9, zf), 180.0)
    B.spot("f1_cashier_work", "work", (cx_, cy_ + 0.8, zf), 0.0)
    for x in (-15.0, 15.0):
        _bench(d, x, 4.2, zf)
        B.spot("f1_bench_%s" % ("w" if x < 0 else "e"), "bench", (x, 4.2, zf), 0.0)
    # fashion: racks on the west side, mannequins at the front windows
    for cx_ in (-19.4, -16.8):
        for ry in (12.0, 14.6, 17.2, 19.8, 22.4, 25.0):
            if cx_ > -17 and 11 < ry < 15:
                continue
            clothes_rack(d, rnd, cx_, ry, zf, 2.0, math.pi / 2, GARMENT)
    for i, x in enumerate((-15.0, -9.0, -3.0, 3.0, 9.0, 15.0)):
        mannequin(d, x, 1.1, zf, ["CT_PlasterRed", "CT_PlasterGreen", "CT_PlasterBlue", "CT_PlasterPink", "CT_PlasterYellow",
                                  "CT_SteelRed"][i])
    # furniture department (east side): sofas, coffee tables, a wardrobe wall, standard lamps
    for sx, sy, ang, c in ((17.5, 8.0, 0.0, "CT_PlasterBlue"), (17.5, 13.5, 0.0, "CT_PlasterRed"), (9.0, 7.5, math.pi, "CT_PlasterGreen")):
        _sofa(m, d, sx, sy, zf, ang, c)
        px, py = sx + (0.0), sy - 1.1 * math.cos(ang)
        rbox(m, "CT_WoodLight", px, py, zf, zf + 0.4, 0.45, 0.3, 0.0)
    for k in range(4):
        wx = 5.0 + k * 3.0
        box(m, "CT_WoodLight", wx, wx + 1.4, D - 1.4, D - T, zf, zf + 2.1, faces="xXyZ" if False else "xXyYZ")
        box(d, "CT_Door", wx + 0.05, wx + 1.35, D - 1.43, D - 1.4, zf + 0.1, zf + 2.0)
        box(d, "CT_Steel", wx + 0.66, wx + 0.74, D - 1.45, D - 1.43, zf + 0.9, zf + 1.2)
    for lx, ly in ((19.5, 17.5), (13.0, 9.5)):
        cyl(d, "CT_Steel", lx, ly, zf, zf + 1.5, 0.02, 4, top=False)
        cyl(d, "CT_Steel", lx, ly, zf, zf + 0.03, 0.17, 8)
        cyl(d, "CT_Opal", lx, ly, zf + 1.4, zf + 1.75, 0.22, 8, r1=0.15)
    # fabric rolls on a table in the middle
    box(m, "CT_WoodLight", 8.5, 12.5, 17.0, 18.6, zf, zf + 0.8, faces="xXyYZ")
    for i in range(8):
        cyl(d, GARMENT[i % 9], 8.8 + i * 0.45, 17.8, zf + 0.8, zf + 1.0, 0.16, 7)


def _bench(d, x, y, z):
    box(d, "CT_WoodLight", x - 0.9, x + 0.9, y - 0.25, y + 0.25, z + 0.43, z + 0.47)
    box(d, "CT_WoodLight", x - 0.9, x + 0.9, y + 0.25, y + 0.29, z + 0.47, z + 0.9)
    for sx in (-0.8, 0.8):
        box(d, "CT_Steel", x + sx - 0.02, x + sx + 0.02, y - 0.22, y + 0.28, z, z + 0.43)


def _sofa(m, d, x, y, z, ang, mat):
    """A 2-seat sofa facing local -y rotated by ang (0 = faces -y)."""
    c, s = math.cos(ang), math.sin(ang)
    P = lambda u, v: (x + c * u - s * v, y + s * u + c * v)
    px, py = P(0, 0)
    rbox(m, mat, px, py, z, z + 0.42, 0.9, 0.45, ang)
    px, py = P(0, 0.38)
    rbox(d, mat, px, py, z + 0.42, z + 0.85, 0.9, 0.1, ang)
    for sx in (-0.85, 0.85):
        px, py = P(sx, 0)
        rbox(d, mat, px, py, z + 0.42, z + 0.62, 0.07, 0.45, ang)


def _stair_rails(B, n, g, r_, ya, yl, hole):
    d = B.detail
    z0 = 0.03
    for x, rise_up_toward in ((-1.8, "+"), (1.8, "-")):
        xa, xb = (x - 0.03, x + 0.03) if x < 0 else (x - 0.03, x + 0.03)
        if x < 0:      # flight A rises with y
            sheared_bar(d, "CT_Steel", xa, xb, ya, z0 + 0.95, yl, z0 + r_ * n + 0.95, 0.04)
        else:          # flight B rises as y falls
            sheared_bar(d, "CT_Steel", xa, xb, ya, z0 + r_ * 2 * n + 0.95, yl, z0 + r_ * n + 0.95, 0.04)
        for k in range(0, n + 1, 3):
            yy = ya + k * g
            zz = (z0 + r_ * k) if x < 0 else (z0 + r_ * (2 * n - k))
            box(d, "CT_Steel", x - 0.015, x + 0.015, min(yy, yl) - 0.015, min(yy, yl) + 0.015, zz, zz + 0.95)
