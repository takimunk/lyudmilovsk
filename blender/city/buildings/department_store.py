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

    def hall(zf, tag):
        # wall shelves
        for sx in (-1, 1):
            for k in range(3):
                yy = 5.0 + k * 8.5
                box(d, "CT_Wood", sx * (xi - 0.5) - 0.25, sx * (xi - 0.5) + 0.25, yy, yy + 6.0, zf, zf + 2.2)
        # sales counters + sellers
        pos = [(-12.0, 12.5), (-12.0, 22.5), (12.0, 12.5), (12.0, 22.5)]
        for ci, (cx_, cy_) in enumerate(pos):
            box(m, "CT_Wood", cx_ - 2.5, cx_ + 2.5, cy_ - 0.45, cy_ + 0.45, zf, zf + 1.05, faces="xXyYZ")
            box(m, "CT_Plaster", cx_ - 2.5, cx_ + 2.5, cy_ - 0.45, cy_ + 0.45, zf, zf + 1.05, faces="Z") if False else None
            box(d, "CT_Steel", cx_ - 2.4, cx_ + 2.4, cy_ + 1.55, cy_ + 1.9, zf, zf + 2.0)          # back shelving
            B.spot("%s_counter_%d" % (tag, ci + 1), "counter", (cx_, cy_ - 1.0, zf), 180.0)
            B.spot("%s_seller_%d" % (tag, ci + 1), "work", (cx_, cy_ + 0.9, zf), 0.0)
        # display islands
        for k, (cx_, cy_) in enumerate(((-5.0, 15.5), (5.0, 15.5), (0.0, 7.5))):
            if tag == "f1" and abs(cx_) < 2.5 and cy_ > 17:
                continue
            box(d, "CT_Wood", cx_ - 1.2, cx_ + 1.2, cy_ - 0.6, cy_ + 0.6, zf, zf + 0.9)
            box(d, "CT_PlasterRed", cx_ - 1.0, cx_ + 1.0, cy_ - 0.4, cy_ + 0.4, zf + 0.9, zf + 1.0)
            B.spot("%s_stand_%d" % (tag, k + 1), "stand", (cx_, cy_ - 1.0, zf), 180.0)

    hall(0.03, "g")
    hall(Z_SLAB1, "f1")
    # cashiers near the doors (ground floor)
    for k, cx_ in enumerate((-4.6, 4.6)):
        cy_ = 6.0
        box(m, "CT_Wood", cx_ - 0.8, cx_ + 0.8, cy_ - 0.35, cy_ + 0.35, 0.03, 1.0, faces="xXyYZ")
        box(d, "CT_Aluminium", cx_ - 0.25, cx_ + 0.25, cy_ + 0.0, cy_ + 0.3, 1.0, 1.3)
        B.spot("cashier_%d" % (k + 1), "counter", (cx_, cy_ - 0.9, 0.03), 180.0)
        B.spot("cashier_work_%d" % (k + 1), "work", (cx_, cy_ + 0.8, 0.03), 0.0)
    for k, cx_ in enumerate((-15.0, 15.0)):
        B.spot("queue_bench_%d" % (k + 1), "bench", (cx_, 4.0, 0.03), 0.0)

    B.footprint = [(-HW, -1.0), (HW, -1.0), (HW, D), (-HW, D)]
    B.height = 14.0
    B.note("2 shop halls + roof sign; stair hole at x -1.8..1.8, y 21..27.3; entrances main/west/east")
