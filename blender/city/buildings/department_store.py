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
from _furn1 import rbox, cyl, blob, mannequin
import interior_kit as kit

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

    _ground(B, rnd)
    _first(B, rnd)
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
GARMENT = ["CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterGreen", "CT_PlasterYellow", "CT_PlasterPink", "CT_Steel",
           "CT_PlasterGrey", "CT_Plaster", "CT_Tile"]
FRONTS = ["CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterGreen", "CT_PlasterYellow"]
WALL_GAPS = (5.4, 10.0, 14.6, 19.2, 23.8, 28.3)          # y of the wall between the side windows


# ------------------------------------------------------------------------------------------------ helpers
def _thing(B, tid, kind, pos, yaw, **props):
    B.thing(tid, kind, tuple(round(v, 3) for v in pos), yaw, **props)


def _seat(B, tid, r, **kw):
    """One seat thing at the first seat of a kit piece (or its middle when it has none)."""
    if r["seats"]:
        x, y, z, yaw = r["seats"][0]
        if len(r["seats"]) > 1:
            x = sum(s[0] for s in r["seats"]) / len(r["seats"])
            y = sum(s[1] for s in r["seats"]) / len(r["seats"])
        _thing(B, tid, "seat", (x, y, z), yaw, height=round(r.get("height", 0.45), 2), **kw)


def _back(B, x0, x1, y, z0, h, mat="CT_Plaster"):
    """The closed back of a free-standing shelf unit (the kit's shelves are open behind)."""
    box(B.detail, mat, x0, x1, y, y + 0.03, z0, z0 + h)


def _wall_run(B, x, y0, step, yaw, zf, specs, h=2.0, w=1.6, gap=0.0):
    """Shop shelves side by side along a side wall (x = the wall face), starting at y0; specs = [(goods, levels)]."""
    out = []
    for i, (g, lv) in enumerate(specs):
        out.append(kit.shop_shelf(B, x, y0 + i * step, zf, yaw, w=w, h=h, levels=lv, goods=g, seed=i + int(y0 * 7)))
    return out


def _counter(B, tag, k, cx, cy, zf, shelves, shop, props, front, tid=None):
    """A 5 m sales counter with its goods, the seller's back shelving and the spots / thing of the old layout."""
    c = kit.shop_counter(B, cx, cy, zf, 0.0, w=5.0, d=0.8, mat=kit.WOODL, top="CT_TileWhite", front=front)
    top = zf + 1.05
    for (fn, dx, dy, yaw, kw) in props:
        fn(B, cx + dx, cy + dy, top, yaw, **kw)
    for i, (g, lv) in enumerate(shelves):
        kit.shop_shelf(B, cx - 1.6 + 1.6 * i, cy + 2.05, zf, 0.0, w=1.6, h=2.0, levels=lv, goods=g, seed=k * 5 + i)
    _back(B, cx - 2.4, cx + 2.4, cy + 2.05, zf, 2.1)
    B.spot("%s_counter_%d" % (tag, k), "counter", (cx, cy - 1.0, zf), 180.0)
    B.spot("%s_seller_%d" % (tag, k), "work", (cx, cy + 0.9, zf), 0.0)
    _thing(B, tid or "%s_counter_%d" % (tag, k), "counter", (cx, cy, top), 0.0, service="shop", shop=shop)
    return c


def _desk(B, tag, k, cx, cy, zf, front):
    """A cashier desk: till, scales, a little goods; spots and a counter thing (ids of the old layout)."""
    kit.shop_counter(B, cx, cy, zf, 0.0, w=1.8, d=0.7, mat=kit.WOODL, top="CT_TileWhite", front=front)
    top = zf + 1.05
    kit.kitchen_scales(B, cx - 0.55, cy - 0.05, top, 0.0)
    kit.newspaper(B, cx - 0.1, cy - 0.1, top, 20.0, seed=k)
    kit.cup(B, cx + 0.2, cy + 0.15, top, 0.0)
    pass


def _bag_stand(B, x, y, zf):
    """A paper-bag and string stand at a till."""
    kit.cardboard_box(B, x, y, zf, 0.0, w=0.3, d=0.25, h=0.5, seed=int(x))
    kit.sack(B, x + 0.3, y, zf, 0.0)


def _directory(B, x, y, zf, yaw, cellname, tid, topic, w=1.3):
    """A free-standing two-post information board (floor plan / lost-and-found), facing yaw, readable both sides."""
    d = B.detail
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    at = lambda u, v: (x + u * c - v * s, y + u * s + v * c)
    for sx in (-w / 2 + 0.05, w / 2 - 0.05):
        px, py = at(sx, 0.0)
        box(d, "CT_Steel", px - 0.025, px + 0.025, py - 0.025, py + 0.025, zf, zf + 2.0, faces="xXyYZ")
    kit.notice_board(B, *at(0.0, -0.05), zf, yaw, w=w, zc=1.45, seed=int(x * 3))
    kit.notice_board(B, *at(0.0, 0.05), zf, yaw + 180, w=w, zc=1.45, seed=int(x * 3) + 1)
    px, py = at(0.0, 0.0)
    box(d, "CT_Steel", px - w / 2 - 0.05, px + w / 2 + 0.05, py - 0.04, py + 0.04, zf + 1.95, zf + 2.05) if abs(c) > 0.7 else \
        box(d, "CT_Steel", px - 0.04, px + 0.04, py - w / 2 - 0.05, py + w / 2 + 0.05, zf + 1.95, zf + 2.05)
    _thing(B, tid, "board", (x, y, zf + 1.45), yaw, topic=topic)


def _rod_rack(B, x, y, zf, yaw, n=6, seed=0):
    """A wall rack of fishing rods and a few reels on the shelf under it (local: front toward -y)."""
    rnd = random.Random(seed)
    m = Mesh()
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    at = lambda u, v, z: (x + u * c - v * s, y + u * s + v * c, z)
    w = n * 0.16 + 0.2
    # back board and two rails
    for z in (0.6, 1.6):
        pts = [at(-w / 2, -0.05, zf + z), at(w / 2, -0.05, zf + z), at(w / 2, -0.12, zf + z), at(-w / 2, -0.12, zf + z)]
        for a, b in zip(pts, pts[1:] + pts[:1]):
            pass
    d = B.detail
    for z in (0.55, 1.65):
        a, b = at(-w / 2, -0.06, zf + z), at(w / 2, -0.06, zf + z)
        box(d, kit.WOOD, min(a[0], b[0]) - (0.03 if abs(s) > 0.7 else 0), max(a[0], b[0]) + (0.03 if abs(s) > 0.7 else 0),
            min(a[1], b[1]) - (0.03 if abs(c) > 0.7 else 0), max(a[1], b[1]) + (0.03 if abs(c) > 0.7 else 0), a[2] - 0.03, a[2] + 0.03)
    for k in range(n):
        u = -w / 2 + 0.18 + k * 0.16
        a = at(u, -0.07, zf + 0.55)
        b = at(u + rnd.uniform(-0.04, 0.04), -0.07, zf + 2.25 + rnd.uniform(-0.15, 0.1))
        mat = ("CT_PlasterYellow", "CT_WoodLight", "CT_PlasterGreen", "CT_Steel", "CT_PlasterRed", "CT_Wood")[k % 6]
        m.face(mat, [(a[0] - 0.006, a[1] - 0.006, a[2]), (a[0] + 0.006, a[1] + 0.006, a[2]), (b[0] + 0.004, b[1] + 0.004, b[2]),
                     (b[0] - 0.004, b[1] - 0.004, b[2])])
        m.face(mat, [(a[0] + 0.006, a[1] - 0.006, a[2]), (a[0] - 0.006, a[1] + 0.006, a[2]), (b[0] - 0.004, b[1] + 0.004, b[2]),
                     (b[0] + 0.004, b[1] - 0.004, b[2])])
        m.face(mat, [(a[0] - 0.006, a[1] - 0.006, a[2]), (b[0] - 0.004, b[1] - 0.004, b[2]), (b[0] + 0.004, b[1] + 0.004, b[2]),
                     (a[0] + 0.006, a[1] + 0.006, a[2])])
    d.extend(m)


def _feeder(B, x, y, z, seed=0):
    """A bird feeder house (garden department): a little wooden box with a roof, hung or on a post."""
    d = B.detail
    r = random.Random(seed)
    mat = r.choice(("CT_WoodLight", "CT_PlasterGreen", "CT_PlasterRed", "CT_Wood"))
    box(d, mat, x - 0.11, x + 0.11, y - 0.09, y + 0.09, z, z + 0.16)
    box(d, "CT_ConcreteDark", x - 0.04, x + 0.04, y - 0.092, y - 0.09, z + 0.05, z + 0.12, faces="y")
    prism(d, "CT_PlasterRed", [(x - 0.15, y - 0.12), (x + 0.15, y - 0.12), (x + 0.15, y + 0.12), (x - 0.15, y + 0.12)],
          z + 0.16, z + 0.2, top=True)
    box(d, "CT_Steel", x - 0.005, x + 0.005, y - 0.005, y + 0.005, z + 0.2, z + 0.5, faces="xXyY")


# ------------------------------------------------------------------------------------------------ ground floor
def _ground(B, rnd):
    zf = 0.03
    m, d = B.m, B.detail
    # ---- red runner from the main door to the stair (kept)
    box(d, "CT_PlasterRed", -1.1, 1.1, 1.8, 20.5, zf, zf + 0.012, faces="Z")
    # ---- window mannequins behind the glass (kept)
    for i, x in enumerate((-17.0, -13.5, -4.5, 4.5, 13.5, 17.0)):
        mannequin(d, x, 2.6, zf, ["CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterGreen", "CT_PlasterPink", "CT_PlasterYellow",
                                  "CT_SteelRed"][i])

    # ---- the four sales counters of the old layout: grocery, sweets, haberdashery and fabrics, drinks
    _counter(B, "g", 1, -12.0, 12.5, zf, [("groceries", 4), ("boxes", 3), ("groceries", 3)], "store_grocery",
             [(kit.kitchen_scales, -1.9, 0.0, 0.0, {}), (kit.crate, -1.1, 0.0, 0.0, {"fill": "CT_PlasterYellow", "seed": 1}),
              (kit.crate, -0.5, 0.0, 6.0, {"fill": "CT_PlasterGreen", "seed": 2}), (kit.bottles, 0.55, 0.05, 0.0, {"n": 3, "seed": 3}),
              (kit.newspaper, 2.0, -0.1, 10.0, {"seed": 3})], "CT_PlasterGreen")
    _counter(B, "g", 2, -12.0, 22.5, zf, [("jars", 3), ("boxes", 3), ("boxes", 3)], "store_sweets",
             [(kit.plates, -1.8, 0.0, 0.0, {"n": 3}), (kit.cardboard_box, 0.5, -0.05, 15.0, {"seed": 5}),
              (kit.cardboard_box, 0.95, 0.1, -10.0, {"seed": 6}), (kit.teapot, 2.0, 0.05, 0.0, {})], "CT_PlasterPink")
    _counter(B, "g", 3, 12.0, 12.5, zf, [("fabric", 2), ("fabric", 2), ("boxes", 3)], "store_haberdashery",
             [(kit.books_stack, -1.8, 0.0, 0.0, {"seed": 7}), (kit.cardboard_box, -1.0, 0.0, 5.0, {"seed": 8}),
              (kit.cardboard_box, -0.5, -0.05, -8.0, {"seed": 9}), (kit.newspaper, 2.0, -0.1, -15.0, {"seed": 4})], "CT_PlasterBlue")
    _counter(B, "g", 4, 12.0, 22.5, zf, [("bottles", 3), ("boxes", 3), ("boxes", 3)], "store_drinks",
             [(kit.bottles, -1.9, 0.0, 0.0, {"n": 4, "seed": 11}), (kit.glass, -1.0, 0.0, 0.0, {}), (kit.glass, -0.85, 0.12, 0.0, {}),
              (kit.crate, 0.4, 0.0, 0.0, {"fill": "CT_PlasterBlue", "seed": 12}), (kit.bottles, 1.0, 0.0, 0.0, {"n": 3, "seed": 13})],
             "CT_PlasterYellow")

    # ---- cashier desks by the doors (spots of the old layout) + bag stands, signs
    for k, cx in enumerate((-4.6, 4.6)):
        cy = 6.0
        kit.shop_counter(B, cx, cy, zf, 0.0, w=1.8, d=0.7, mat=kit.WOODL, top="CT_TileWhite", front=FRONTS[k])
        top = zf + 1.05
        kit.kitchen_scales(B, cx - 0.55, cy - 0.05, top, 0.0)
        kit.newspaper(B, cx - 0.1, cy - 0.12, top, 20.0 - 40 * k, seed=k)
        kit.cup(B, cx + 0.15, cy + 0.15, top, 0.0)
        kit.sack(B, cx - 1.25, cy + 0.2, zf, 0.0)
        B.spot("cashier_%d" % (k + 1), "counter", (cx, cy - 0.9, zf), 180.0)
        B.spot("cashier_work_%d" % (k + 1), "work", (cx, cy + 0.8, zf), 0.0)
        _thing(B, "cashier_%d" % (k + 1), "counter", (cx, cy, top), 0.0, service="cashier", role="cashier")
        kit.sign_plate(B, cx, cy, zf, 0.0, name="sign_till", w=0.9, zc=3.75, hang=True)

    # ---- queue benches at the windows
    for k, cx in enumerate((-15.0, 15.0)):
        r = kit.bench(B, cx, 4.0, zf, 0.0, w=1.8, kind="park")
        B.spot("queue_bench_%d" % (k + 1), "bench", (cx, 4.0, zf), 0.0)
        _seat(B, "g_queue_bench_%d" % (k + 1), r)

    # ---- west front: SHOES (racks along the wall, two back-to-back islands, a counter, try-on stools, a mirror)
    for i in range(5):
        kit.shoe_rack(B, -22.1, 3.5 + i * 1.15, zf, 90.0, w=1.05, rows=3, seed=i)
    for iy in (5.2, 8.2):
        kit.shoe_rack(B, -18.4, iy, zf, 90.0, w=1.05, rows=3, seed=iy)
        kit.shoe_rack(B, -18.4, iy, zf, 270.0, w=1.05, rows=3, seed=iy + 1)
    kit.mirror(B, -22.1, 9.45, zf, 90.0, zc=1.2, w=0.7, h=1.3)
    kit.shop_counter(B, -14.6, 6.7, zf, 90.0, w=1.8, d=0.7, mat=kit.WOODL, top="CT_TileWhite", front="CT_PlasterRed")
    kit.newspaper(B, -14.6, 6.4, zf + 1.05, 90.0, seed=11)
    kit.cardboard_box(B, -14.6, 7.1, zf + 1.05, 90.0, w=0.4, d=0.3, h=0.2, seed=12)
    B.spot("g_shoes_counter", "counter", (-13.7, 6.7, zf), 270.0)
    B.spot("g_shoes_seller", "work", (-15.5, 6.7, zf), 90.0)
    _thing(B, "g_shoes_counter", "counter", (-14.6, 6.7, zf + 1.05), 90.0, service="cashier", role="cashier")
    for j, (sx, sy) in enumerate(((-20.2, 4.6), (-20.2, 6.6), (-20.2, 8.2))):
        r = kit.stool(B, sx, sy, zf, 180.0, h=0.4)
        kit.cardboard_box(B, sx + 0.45, sy + 0.2, zf, 25.0 * j, w=0.34, d=0.22, h=0.12, seed=j)
        _seat(B, "g_shoe_stool_%d" % (j + 1), r)

    # ---- west wall: the grocery wall (7 bays), a sign over it, posters in the gaps between the windows
    _wall_run(B, -22.1, 12.0, 1.65, 90.0, zf, [("groceries", 3), ("boxes", 3), ("jars", 3), ("boxes", 3), ("groceries", 3),
                                               ("boxes", 3), ("boxes", 3)], h=2.1)
    # the stock corner behind the sweets counter
    for k in range(3):
        kit.shop_shelf(B, -20.6 + k * 1.65, 29.6, zf, 0.0, w=1.6, h=2.1, levels=4, goods=("boxes", "hardware", "toys")[k], seed=k)
    for i, (cx, cy, yw) in enumerate(((-21.3, 27.0, 0), (-19.6, 27.6, 20), (-20.4, 26.2, -10), (-17.6, 27.4, 35))):
        kit.crate(B, cx, cy, zf, yw, w=0.55, d=0.38, h=0.3, fill=("CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterGreen", "CT_Leaf")[i], seed=i)
    kit.crate(B, -20.4, 26.2, zf + 0.3, 5.0, w=0.55, d=0.38, h=0.3, fill="CT_PlasterRed", seed=9)
    for i, (sx, sy) in enumerate(((-16.4, 27.9), (-16.0, 27.1), (-15.3, 28.0))):
        kit.sack(B, sx, sy, zf, 30.0 * i)
    kit.cardboard_box(B, -18.0, 26.4, zf, 12.0, w=0.6, d=0.45, h=0.4, seed=3)
    kit.sign_plate(B, -22.1, 14.6, zf, 90.0, name="sign_milk", w=1.3, zc=2.55)

    # ---- east front: HOUSEHOLD (hardware bays, a kitchen row, tables of crockery)
    _wall_run(B, 22.1, 3.6, 1.65, 270.0, zf, [("hardware", 4), ("hardware", 4), ("boxes", 3)], h=2.1)
    r = kit.kitchen_unit(B, 18.0, 10.2, zf, 0.0, modules="cupboard,stove,sink,fridge")
    kit.samovar(B, 18.9, 10.0, zf + 0.9, 0.0)
    kit.kitchen_scales(B, 17.6, 10.1, zf + 0.9, 180.0)
    kit.teapot(B, 17.9, 10.1, zf + 0.9, 0.0)
    tb = kit.table(B, 17.4, 6.4, zf, 0.0, w=1.6, d=0.9, h=0.8)
    kit.plates(B, 16.9, 6.4, tb["top"], 0.0, n=5)
    kit.teapot(B, 17.7, 6.3, tb["top"], 20.0)
    kit.cup(B, 18.0, 6.7, tb["top"], 0.0)
    kit.glass(B, 17.4, 6.8, tb["top"], 0.0)
    tb2 = kit.table(B, 17.4, 8.6, zf, 0.0, w=1.6, d=0.9, h=0.8)
    kit.samovar(B, 16.9, 8.6, tb2["top"], 0.0)
    kit.vase(B, 17.7, 8.5, tb2["top"], 0.0, seed=21)
    kit.plates(B, 18.0, 8.8, tb2["top"], 0.0, n=3)

    # ---- east wall: fabrics (bays), toys corner
    _wall_run(B, 22.1, 11.6, 1.65, 270.0, zf, [("fabric", 2), ("fabric", 2), ("fabric", 2)], h=2.1)
    kit.sign_plate(B, 22.1, 14.6, zf, 270.0, name="sign_fabric", w=1.3, zc=2.55)
    sw = kit.sewing_table(B, 19.4, 14.8, zf, 270.0)
    _thing(B, "g_sewing_machine", "craft", (19.4, 14.8, zf + 0.8), 270.0, what="sewing_machine")
    st = kit.stool(B, 18.5, 14.8, zf, 90.0, h=0.45)
    _seat(B, "g_sewing_stool", st)
    kit.table(B, 20.0, 12.6, zf, 90.0, w=1.4, d=0.8, h=0.85)
    for i in range(6):
        cyl(d, GARMENT[i], 20.0 - 0.0, 12.0 + i * 0.22, zf + 0.85, zf + 0.97, 0.1, 6)
    _wall_run(B, 22.1, 17.4, 1.65, 270.0, zf, [("toys", 5), ("toys", 5), ("toys", 5)], h=2.1)
    kit.sign_plate(B, 22.1, 19.2, zf, 270.0, name="sign_toys", w=1.3, zc=2.55)
    kit.rug(B, 18.8, 24.0, zf, 0.0, w=3.2, d=3.2, round=True, name="rug_round")
    for i in range(12):
        a = rnd.uniform(0, 6.28)
        r_ = rnd.uniform(0.2, 1.4)
        rbox(d, rnd.choice(WARM[:5]), 18.8 + r_ * math.cos(a), 24.0 + r_ * math.sin(a), zf + 0.01, zf + 0.01 + rnd.uniform(0.1, 0.25),
             0.1, 0.1, a)
    for bx, by, c in ((17.9, 24.8, "CT_WoodLight"), (19.4, 23.2, "CT_Door"), (18.6, 24.3, "CT_PlasterPink")):
        blob(d, c, bx, by, zf + 0.36, 0.22, 7)
        blob(d, c, bx, by, zf + 0.7, 0.14, 6)
    rbox(m, "CT_PlasterYellow", 20.5, 27.0, zf, zf + 1.1, 0.7, 0.7)                                    # toy house
    prism(d, "CT_PlasterRed", [(19.65, 26.15), (21.35, 26.15), (21.35, 27.85), (19.65, 27.85)], zf + 1.1, zf + 1.3, top=True)
    box(d, "CT_Door", 20.2, 20.8, 26.28, 26.3, zf, zf + 0.75, faces="y")
    kt = kit.table(B, 16.6, 26.0, zf, 0.0, w=0.9, d=0.6, h=0.5)
    for sx, sy, yw in ((16.0, 26.7, 180), (17.2, 26.7, 180), (16.6, 25.3, 0)):
        kit.stool(B, sx, sy, zf, yw, h=0.3)
    kit.cardboard_box(B, 16.6, 26.0, kt["top"], 0.0, w=0.3, d=0.3, h=0.2, seed=2)
    kit.globe(B, 16.9, 25.9, kt["top"], 0.0)
    _thing(B, "g_toy_house", "board", (20.5, 25.9, zf + 1.0), 180.0, topic="kids")

    # ---- back wall east: haberdashery and notions shelving, a clock, a painting
    for k in range(3):
        kit.shop_shelf(B, 11.2 + k * 1.65, 29.6, zf, 0.0, w=1.6, h=2.1, levels=4, goods=("boxes", "toys", "hardware")[k], seed=k + 20)

    # ---- islands of the old spots (stand_1, stand_2) and the floor plan board (stand_3)
    for k, (cx, cy) in enumerate(((-5.0, 15.5), (5.0, 15.5))):
        t = kit.table(B, cx, cy, zf, 0.0, w=2.4, d=1.2, h=0.9, mat=kit.WOODL)
        top = t["top"]
        if k == 0:
            kit.plates(B, cx - 0.8, cy - 0.2, top, 0.0, n=6)
            kit.teapot(B, cx - 0.1, cy - 0.2, top, 10.0)
            kit.teapot(B, cx + 0.3, cy - 0.1, top, -20.0, mat="CT_PlasterBlue")
            kit.samovar(B, cx + 0.9, cy - 0.1, top, 0.0)
            kit.cup(B, cx - 0.5, cy + 0.25, top, 0.0)
            kit.cup(B, cx - 0.2, cy + 0.3, top, 20.0)
            kit.vase(B, cx + 0.3, cy + 0.3, top, 0.0, seed=14)
            kit.glass(B, cx + 0.7, cy + 0.3, top, 0.0)
        else:
            kit.cardboard_box(B, cx - 0.8, cy - 0.2, top, 5.0, w=0.4, d=0.3, h=0.25, seed=15)
            kit.cardboard_box(B, cx - 0.3, cy - 0.15, top, -8.0, w=0.4, d=0.3, h=0.2, seed=16)
            kit.vase(B, cx + 0.3, cy - 0.2, top, 0.0, seed=17)
            kit.books_stack(B, cx - 0.7, cy + 0.3, top, 0.0, n=5, seed=19)
            kit.table_lamp(B, cx + 0.2, cy + 0.3, top, 0.0)
        B.spot("g_stand_%d" % (k + 1), "stand", (cx, cy - 1.0, zf), 180.0)
        _thing(B, "g_island_%d" % (k + 1), "board", (cx, cy, top), 0.0, topic="catalogue")
    _directory(B, -3.4, 8.8, zf, 0.0, None, "g_stand_3", "store")
    B.spot("g_stand_3", "stand", (-3.4, 7.6, zf), 180.0)

    # ---- the cloakroom (back wall, east of the stair)
    kit.shop_counter(B, 6.4, 26.4, zf, 0.0, w=3.4, d=0.7, mat=kit.WOODL, top="CT_TileWhite", front="CT_PlasterGreen", till=False)
    kit.coat_rack(B, 5.0, 28.8, zf, 0.0, w=2.2, coats=0.8, seed=3)
    kit.coat_rack(B, 7.9, 28.8, zf, 0.0, w=2.2, coats=0.6, seed=4)
    kit.cup(B, 7.2, 26.35, zf + 1.05, 0.0)
    kit.newspaper(B, 5.4, 26.4, zf + 1.05, 8.0, seed=5)
    kit.sign_plate(B, 6.4, 26.4, zf, 0.0, name="sign_cloakroom", w=1.4, zc=3.7, hang=True)
    B.spot("g_cloak_counter", "counter", (6.4, 25.5, zf), 180.0)
    B.spot("g_cloak_work", "work", (6.4, 27.2, zf), 0.0)
    _thing(B, "g_cloakroom", "counter", (6.4, 26.4, zf + 1.05), 0.0, service="cloakroom", role="cloakroom")

    # ---- machines near the doors: payphone, medical scales
    kit.scales(B, -10.8, 3.1, zf, 0.0)
    _thing(B, "g_scales", "machine", (-10.8, 3.1, zf + 1.0), 0.0, what="scales")

    # ---- plants, clocks, posters, paintings, fire extinguishers
    kit.plant(B, -6.6, 3.1, zf, 0.0, kind="palm", pot="CT_PlasterRed", h=1.5)
    kit.plant(B, 6.6, 3.1, zf, 0.0, kind="ficus", pot="CT_PlasterBlue", h=1.4)
    kit.plant(B, -3.4, 28.7, zf, 0.0, kind="monstera", pot="CT_PlasterGreen", h=1.2)
    kit.plant(B, 3.2, 28.7, zf, 0.0, kind="fern", pot="CT_PlasterYellow", h=1.0)
    kit.wall_clock(B, 0.0, 29.6, zf, 0.0, zc=3.3, r=0.3)
    kit.fire_extinguisher(B, -22.1, 11.0, zf, 90.0)
    kit.fire_extinguisher(B, 22.1, 16.2, zf, 270.0)
    wl = ["poster_sun_1", "travel_0", "poster_cons_0", "poster_health", "travel_1", "poster_cons_1"]
    el = ["poster_sun_2", "travel_2", "poster_cons_2", "poster_safety", "travel_3", "poster_cons_3"]
    for y, a, b in zip(WALL_GAPS, wl, el):
        kit.poster(B, -22.1, y, zf, 90.0, name=a, w=0.85, zc=3.35)
        kit.poster(B, 22.1, y, zf, 270.0, name=b, w=0.85, zc=3.35)
    for x, nme in ((-9.6, "painting_sea_calm"), (-3.3, "painting_lighthouse"), (3.3, "painting_dunes"), (9.6, "painting_pines")):
        kit.picture(B, x, 29.6, zf, 0.0, name=nme, w=1.2, zc=3.2)
    # the lost-and-found wall board is on the first floor; a price list by the groceries
    _thing(B, "g_prices", "board", (-22.0, 14.8, zf + 1.6), 90.0, topic="prices")
    # hanging department signs
    kit.sign_plate(B, -12.0, 17.0, zf, 0.0, name="sign_greens", w=1.7, zc=3.7, hang=True)
    # QA dressing: the middle of both side halls was bare floor
    for k, (rx, ry) in enumerate(((-17.4, 17.0), (-17.4, 19.2))):
        kit.clothes_rail(B, rx, ry, zf, 0.0, w=1.6, seed=30 + k)
    kit.display_case(B, 16.4, 18.4, zf, 0.0, w=1.2, goods="toys", seed=33)
    kit.display_case(B, 18.0, 18.4, zf, 0.0, w=1.2, goods="boxes", seed=34)
    kit.plant(B, 20.6, 19.4, zf, 0.0, kind="ficus", pot="CT_PlasterYellow", h=1.3)
    kit.plant(B, -20.6, 19.4, zf, 0.0, kind="fern", pot="CT_PlasterRed", h=1.0)


# ------------------------------------------------------------------------------------------------ first floor
def _first(B, rnd):
    zf = Z_SLAB1
    m, d = B.m, B.detail
    # ---- window mannequins (fashion, front windows) kept
    for i, x in enumerate((-15.0, -9.0, -3.0, 3.0, 9.0, 15.0)):
        pass
    _first_counters(B, zf)
    _first_west(B, zf, rnd)
    _first_east(B, zf, rnd)
    _first_mid(B, zf, rnd)


def _first_counters(B, zf):
    _counter(B, "f1", 1, -12.0, 12.5, zf, [("boxes", 3), ("boxes", 3), ("hardware", 4)], "store_books",
             [(kit.typewriter, -1.9, 0.0, 0.0, {}), (kit.books_stack, -1.2, 0.0, 0.0, {"n": 5, "seed": 31}),
              (kit.paint_jars, -0.4, 0.0, 0.0, {"n": 4, "seed": 32}), (kit.newspaper, 0.5, -0.1, 12.0, {"seed": 5}),
              (kit.newspaper, 0.9, 0.05, -20.0, {"seed": 6}), (kit.globe, 2.1, 0.0, 0.0, {})], "CT_PlasterBlue")
    _counter(B, "f1", 2, -12.0, 22.5, zf, [("hardware", 4), ("boxes", 3), ("hardware", 4)], "store_sports",
             [(kit.tools, -1.9, 0.0, 0.0, {}), (kit.cardboard_box, -1.0, 0.0, 10.0, {"seed": 33}),
              (kit.cardboard_box, -0.55, 0.0, -10.0, {"seed": 34}), (kit.books_stack, 0.6, 0.0, 0.0, {"n": 3, "seed": 35}),
              (kit.glass, 2.0, 0.1, 0.0, {})], "CT_PlasterRed")
    _counter(B, "f1", 3, 12.0, 12.5, zf, [("boxes", 3), ("jars", 4), ("boxes", 3)], "store_garden",
             [(kit.sack, -1.9, 0.0, 0.0, {}), (kit.sack, -1.45, 0.1, 30.0, {}), (kit.crate, -0.7, 0.0, 0.0, {"fill": "CT_Leaf", "seed": 36}),
              (kit.cardboard_box, 0.4, -0.05, 5.0, {"seed": 37}), (kit.kitchen_scales, 0.9, 0.0, 0.0, {})], "CT_PlasterGreen")
    _counter(B, "f1", 4, 12.0, 22.5, zf, [("boxes", 3), ("toys", 4), ("boxes", 3)], "store_souvenirs",
             [(kit.globe, -1.9, 0.0, 0.0, {}), (kit.vase, -1.2, 0.0, 0.0, {"seed": 39}), (kit.plates, 0.5, 0.0, 0.0, {"n": 3}), (kit.cup, 1.0, 0.0, 0.0, {}), (kit.telephone, 2.1, 0.0, 15.0, {})],
             "CT_PlasterYellow")


def _first_west(B, zf, rnd):
    d = B.detail
    # ---- records and radios (west front): record bays along the wall, listening corner, TV row
    for i, g in enumerate(("records", "records", "records", "boxes")):
        kit.shop_shelf(B, -22.1, 3.6 + i * 1.7, zf, 90.0, w=1.6, h=1.15, levels=3, goods=g, seed=50 + i)
    t = kit.table(B, -18.9, 4.7, zf, 90.0, w=1.8, d=0.8, h=0.8)
    rp = kit.record_player(B, -18.9, 4.2, t["top"], 90.0)
    kit.radio(B, -18.9, 5.1, t["top"], 90.0)
    _thing(B, "f1_record_player", "machine", (-18.9, 4.2, t["top"] + 0.1), 90.0, what="record_player")
    for j, yy in enumerate((3.9, 5.5)):
        r = kit.stool(B, -17.7, yy, zf, 90.0, h=0.45)
        if j == 0:
            _seat(B, "f1_listen_stool_1", r)
    # radios on a long table + TVs
    t2 = kit.table(B, -18.9, 8.0, zf, 90.0, w=2.4, d=0.8, h=0.8)
    for j in range(4):
        kit.radio(B, -18.9, 7.1 + j * 0.6, t2["top"], 90.0)
    _thing(B, "f1_radio_table", "board", (-18.9, 8.0, t2["top"]), 90.0, topic="catalogue")
    for j, yy in enumerate((9.3, 10.3)):
        tv = kit.tv_set(B, -15.2, yy, zf, 90.0, screen=("screen_tv_news", "screen_tv_sea")[j])
        _thing(B, "f1_tv_%d" % (j + 1), "screen", (-15.2, yy, zf + 0.75), 90.0, what="tv")
    kit.jukebox(B, -21.5, 10.2, zf, 90.0)
    _thing(B, "f1_jukebox", "machine", (-21.5, 10.2, zf + 1.2), 90.0, what="jukebox")
    # ---- clothes (west middle): rails in two columns, a wardrobe, a fitting mirror, a screen
    for k in range(5):
        kit.clothes_rail(B, -20.4, 12.2 + k * 1.75, zf, 90.0, w=1.55, seed=k)
    for k in range(3):
        kit.clothes_rail(B, -18.3, 12.2 + k * 1.75, zf, 90.0, w=1.55, seed=k + 10)
    kit.screen_divider(B, -20.4, 21.0, zf, 90.0)
    kit.mirror(B, -22.1, 19.8, zf, 90.0, zc=1.3, w=0.7, h=1.4)
    # ---- sports and fishing (west back)
    for k in range(3):
        kit.wall_bars(B, -22.1, 24.0 + k * 1.0, zf, 90.0, w=0.9, h=2.4)
    kit.mats(B, -20.2, 26.4, zf, 90.0, w=1.0, d=1.8, n=2)
    _rod_rack(B, -22.1, 28.2, zf, 90.0, n=6, seed=3)
    kit.exercise_bike(B, -18.0, 27.6, zf, 90.0)
    _thing(B, "f1_exercise_bike", "sport", (-18.0, 27.6, zf + 0.9), 90.0, what="exercise_bike")
    _thing(B, "f1_wall_bars", "sport", (-22.0, 24.9, zf + 1.3), 90.0, what="wall_bars")
    kit.barbell_rack(B, -16.4, 28.0, zf, 0.0)
    _thing(B, "f1_barbell", "sport", (-16.4, 28.0, zf + 1.0), 0.0, what="barbell")
    kit.sign_plate(B, -22.1, 23.8, zf, 90.0, name="sign_gym", w=1.2, zc=2.95)
    # ---- the books corner on the back wall: topic bookcases, magazine racks, a reading table
    for k, tp in enumerate(("sea", "birds", "science")):
        kit.bookshelf(B, -13.6 + k * 1.7, 29.6, zf, 0.0, w=1.6, h=2.1, levels=5, topic=tp, seed=k + 3)
        _thing(B, "f1_books_%s" % tp, "shelf", (-13.6 + k * 1.7, 29.3, zf + 1.3), 0.0, topic=tp)
    for k, x in enumerate((-8.4, -7.4)):
        kit.magazine_rack(B, x, 29.6, zf, 0.0, w=0.9, seed=k)
    _thing(B, "f1_magazines", "shelf", (-7.9, 29.3, zf + 1.1), 0.0, topic="local")
    rt = kit.reading_table(B, -10.6, 26.9, zf, 0.0, w=2.2, d=0.9, lamps=2)
    for j, (dx, dy, yw) in enumerate(((-0.55, -0.7, 0.0), (0.55, -0.7, 0.0), (-0.55, 0.7, 180.0), (0.55, 0.7, 180.0))):
        r = kit.chair(B, -10.6 + dx, 26.9 + dy, zf, yw, kind="wood")
    _thing(B, "f1_reading_seats", "seat", (-10.6, 26.2, zf + 0.45), 0.0, height=0.45)


def _first_east(B, zf, rnd):
    d = B.detail
    # ---- the café corner (east front, by the street windows)
    cc = kit.shop_counter(B, 21.2, 5.4, zf, 90.0, w=2.2, d=0.7, mat=kit.WOODL, top="CT_TileWhite", front="CT_PlasterRed", till=True)
    kit.samovar(B, 21.2, 4.7, zf + 1.05, 270.0)
    kit.cup(B, 21.2, 5.4, zf + 1.05, 0.0)
    kit.cup(B, 21.2, 5.8, zf + 1.05, 0.0)
    kit.plates(B, 21.2, 6.2, zf + 1.05, 0.0, n=3)
    kit.teapot(B, 21.2, 4.5, zf + 1.05, 0.0)
    kit.menu_board(B, 22.1, 5.4, zf, 270.0, name="menu_0", w=1.0, zc=2.3)
    B.spot("f1_cafe_counter", "counter", (20.3, 5.4, zf), 90.0)
    B.spot("f1_cafe_work", "work", (22.0, 5.4, zf), 270.0)
    _thing(B, "f1_cafe_counter", "counter", (21.2, 5.4, zf + 1.05), 90.0, service="bar")
    kit.sign_plate(B, 21.2, 5.4, zf, 270.0, name="sign_buffet", w=1.5, zc=3.6, hang=True)
    for k, (tx, ty) in enumerate(((16.5, 3.4), (16.5, 6.4), (13.3, 4.9), (13.3, 8.2), (19.0, 8.6))):
        t = kit.table_round(B, tx, ty, zf, 0.0, r=0.4, h=0.74)
        kit.cup(B, tx - 0.1, ty, t["top"], 0.0)
        kit.plates(B, tx + 0.12, ty + 0.05, t["top"], 0.0, n=1)
        rs = []
        for a in (90.0, 270.0):
            ox = 0.62 if a == 270.0 else -0.62
            rs.append(kit.chair(B, tx + ox, ty, zf, a, kind="tube", mat=("CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow")[k % 3]))
        _seat(B, "f1_cafe_seat_%d" % (2 * k + 1), rs[0])
        _seat(B, "f1_cafe_seat_%d" % (2 * k + 2), rs[1])
    kit.plant(B, 11.6, 3.0, zf, 0.0, kind="palm", pot="CT_PlasterYellow", h=1.6)
    # ---- garden and bird feed (east middle): wall bays, feeders, plants, sacks
    for i, g in enumerate(("boxes", "jars", "boxes")):
        kit.shop_shelf(B, 22.1, 11.4 + i * 1.65, zf, 270.0, w=1.6, h=1.15, levels=3, goods=g, seed=60 + i)
    ft = kit.table(B, 18.0, 15.2, zf, 0.0, w=2.6, d=0.6, h=0.75)           # the feeders stand on a display table
    for i in range(5):
        _feeder(B, 17.0 + i * 0.5, 15.2, ft["top"], seed=i)
    kit.plant(B, 20.6, 15.6, zf, 0.0, kind="fern", pot="CT_PlasterGreen", h=1.0)
    kit.plant(B, 21.0, 17.2, zf, 0.0, kind="cactus", pot="CT_PlasterRed", h=0.7)
    kit.sack(B, 18.3, 16.4, zf, 15.0)
    kit.sack(B, 18.8, 16.8, zf, 80.0)
    kit.crate(B, 19.5, 16.7, zf, 0.0, fill="CT_Leaf", seed=3)
    kit.poster(B, 22.1, 14.6, zf, 270.0, name="poster_birds_0", w=0.9, zc=2.9)
    kit.poster(B, 22.1, 19.2, zf, 270.0, name="poster_birds_1", w=0.9, zc=2.9)
    _thing(B, "f1_birds_board", "board", (22.0, 14.6, zf + 2.0), 270.0, topic="birds")
    kit.sign_plate(B, 22.1, 10.4, zf, 270.0, name="sign_greens", w=1.1, zc=3.0)
    # ---- furniture department + souvenirs (east back): sofas, armchairs, wardrobe, lamps
    for i, (nm, kw) in enumerate((("wardrobe", {"w": 1.1}), ("wardrobe", {"w": 1.1}))):
        kit.wardrobe(B, 22.1 - 0.0, 28.0 - i * 1.3, zf, 270.0, **kw)
    sf = kit.sofa(B, 18.6, 27.4, zf, 0.0, seats=3, mat="CT_PlasterBlue")
    _seat(B, "f1_sofa_1", sf)
    af = kit.armchair(B, 15.4, 27.9, zf, 0.0, mat="CT_PlasterRed")
    _seat(B, "f1_armchair_1", af)
    ct = kit.coffee_table(B, 18.4, 25.4 + 0.0, zf, 0.0)
    kit.books_stack(B, 18.3, 25.4, ct["top"], 0.0, n=3, seed=41)
    kit.floor_lamp(B, 20.6, 28.8, zf, 0.0)
    kit.rug(B, 17.8, 26.3, zf, 0.0, w=3.0, d=2.4, name="rug")
    kit.plant(B, 14.3, 29.0, zf, 0.0, kind="ficus", pot="CT_PlasterGreen", h=1.5)
    kit.picture(B, 17.0, 29.6, zf, 0.0, name="painting_abstract", w=1.2, zc=1.9)
    dc = kit.display_case(B, 16.2, 19.2, zf, 0.0, w=1.2, goods="toys", seed=3)
    kit.display_case(B, 17.8, 19.2, zf, 0.0, w=1.2, goods="boxes", seed=4)
    _thing(B, "f1_souvenir_case", "board", (17.0, 19.2, zf + 1.0), 0.0, topic="local")
    kit.wall_clock(B, 22.1, 23.8, zf, 270.0, zc=2.6)


def _first_mid(B, zf, rnd):
    d = B.detail
    # ---- cashier desk by the stair (kept spots), tills near the front
    cx_, cy_ = -6.0, 24.0
    kit.shop_counter(B, cx_, cy_, zf, 0.0, w=2.0, d=0.7, mat=kit.WOODL, top="CT_TileWhite", front="CT_PlasterRed")
    kit.kitchen_scales(B, cx_ - 0.6, cy_ - 0.05, zf + 1.05, 0.0)
    kit.cup(B, cx_ + 0.1, cy_ + 0.15, zf + 1.05, 0.0)
    kit.newspaper(B, cx_ - 0.2, cy_ - 0.12, zf + 1.05, 10.0, seed=9)
    B.spot("f1_cashier", "counter", (cx_, cy_ - 0.9, zf), 180.0)
    B.spot("f1_cashier_work", "work", (cx_, cy_ + 0.8, zf), 0.0)
    _thing(B, "f1_cashier", "counter", (cx_, cy_, zf + 1.05), 0.0, service="cashier", role="cashier")
    kit.sign_plate(B, cx_, cy_, zf, 0.0, name="sign_till", w=0.9, zc=3.65, hang=True)
    # ---- benches at the front windows (kept spots)
    for x in (-15.0, 15.0):
        side = "w" if x < 0 else "e"
        if x > 0:
            continue
        r = kit.bench(B, x, 4.2, zf, 0.0, w=1.8, kind="park")
    # (east front is the café: its bench spot moves to the café window)
    B.spot("f1_bench_w", "bench", (-15.0, 4.2, zf), 0.0)
    B.spot("f1_bench_e", "bench", (13.2, 2.4, zf), 0.0)
    kit.bench(B, 13.2, 2.4, zf, 0.0, w=1.8, kind="park")
    _thing(B, "f1_bench_e", "seat", (13.2, 2.4, zf + 0.45), 0.0, height=0.45)
    # ---- islands (stand_1, stand_2) and the lost-and-found board (stand_3)
    for k, (cx, cy) in enumerate(((-5.0, 15.5), (5.0, 15.5))):
        t = kit.table(B, cx, cy, zf, 0.0, w=2.4, d=1.2, h=0.9, mat=kit.WOODL)
        top = t["top"]
        if k == 0:
            for j in range(3):
                kit.radio(B, cx - 0.8 + j * 0.7, cy - 0.2, top, 0.0)
            kit.record_player(B, cx + 0.85, cy - 0.1, top, 0.0)
            kit.telephone(B, cx - 0.5, cy + 0.3, top, 15.0)
            kit.typewriter(B, cx + 0.3, cy + 0.3, top, 0.0)
        else:
            kit.books_stack(B, cx - 0.8, cy - 0.2, top, 0.0, n=6, seed=42)
            kit.paint_jars(B, cx - 0.1, cy - 0.2, top, 0.0, n=5, seed=43)
            kit.typewriter(B, cx + 0.6, cy - 0.15, top, 0.0)
            kit.globe(B, cx + 0.9, cy + 0.25, top, 0.0)
            kit.cardboard_box(B, cx - 0.4, cy + 0.3, top, 0.0, w=0.5, d=0.3, h=0.2, seed=44)
        B.spot("f1_stand_%d" % (k + 1), "stand", (cx, cy - 1.0, zf), 180.0)
        _thing(B, "f1_island_%d" % (k + 1), "board", (cx, cy, top), 0.0, topic="catalogue")
    _directory(B, 3.4, 8.8, zf, 0.0, None, "f1_stand_3", "lost")
    B.spot("f1_stand_3", "stand", (3.4, 7.6, zf), 180.0)
    # ---- photo booth by the stair, wall clock, pictures, signs
    for i, x in enumerate((-15.0, -9.0, -3.0, 3.0, 9.0)):
        mannequin(d, x, 1.1, zf, ["CT_PlasterRed", "CT_PlasterGreen", "CT_PlasterBlue", "CT_PlasterPink", "CT_PlasterYellow"][i])
    kit.photo_booth(B, 6.2, 28.5, zf, 0.0)
    _thing(B, "f1_photo_booth", "machine", (6.2, 28.5, zf + 1.1), 0.0, what="photo_booth")
    kit.wall_clock(B, 0.0, 29.6, zf, 0.0, zc=3.2, r=0.3)
    kit.picture(B, -9.6, 29.6, zf, 0.0, name="painting_sea_night", w=1.2, zc=3.0)
    kit.plant(B, -3.4, 28.8, zf, 0.0, kind="palm", pot="CT_PlasterRed", h=1.5)
    kit.fire_extinguisher(B, 22.1, 9.2, zf, 270.0)
    kit.sign_plate(B, -12.0, 17.0, zf, 0.0, name="sign_books", w=1.6, zc=3.65, hang=True)
    for nme, y, zc_ in (("sleeve_0", 5.4, 2.4), ("travel_0", 10.0, 3.0), ("poster_cons_0", 14.6, 3.0),
                        ("poster_health", 19.2, 3.0), ("travel_1", 28.3, 3.0)):
        if nme.startswith("sleeve"):
            kit.picture(B, -22.1, y, zf, 90.0, name="sleeve_0", w=0.7, zc=zc_)
        else:
            kit.poster(B, -22.1, y, zf, 90.0, name=nme, w=0.85, zc=zc_)


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
