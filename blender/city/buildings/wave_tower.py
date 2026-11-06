"""Wave Tower ("Башня-Волна"): the landmark of the town seen from the sea. An ORIGINAL design in the spirit
of 1950s-60s Brazilian modernist residential towers (Sao Paulo / Rio: continuous undulating horizontal
brise-soleil, azulejo-tiled cores and gables, a shopping arcade under a glazed base); not a copy of any building.

Plot 80 m (front, x) x 22 m deep (y), height 58 m.
Massing: a slab 12 m deep, 75 m long, whose plan is one gentle S-curve (yc(x) = 10.4 + 3 sin(2 pi x / 75)).
  * Ground floor (z 0..3.5): an ENTERABLE arcade 3.7 m deep along the whole curved front under the overhanging
    mezzanine, round columns, paving; behind it six hollow shops (glazed fronts with door holes, interior walls, floor,
    ceiling, counters), a through passage (blue-tiled) from the arcade to the back, and three lobbies of the
    lift/stair cores with street doors.
  * Mezzanine (z 3.5..6.5): glazed band of "public" windows (not enterable).
  * 16 flat storeys (storeys 2..17, floor-to-floor 3.0 m, z 6.5..54.5): the whole sea-facing front is wrapped by
    continuous white-concrete sun-breaker bands at every floor level that follow the S-curve and whose fronts
    undulate (phase shifts storey by storey, so the facade ripples). The window wall is set 1.3 m behind the bands;
    one window card per 2.5 m bay. Cores (3: x = -25, 0, 25) are clad in blue tile (CT_Tile) with narrow stair windows;
    the end gables are blue tile too, with side windows of the end flats.
  * Flats: each storey carries 12-14 flats of 2-4 bays (studios, 2-room, 3-room), all with 2-4 front windows and one
    small back window per bay; flat doors are at the nearest core.
  * Roof: three plant rooms over the cores, a water tank, a roof terrace (timber deck) under an undulating thin
    concrete canopy ("wave") on posts, a rail round the roof.
Enterable: arcade, passage, shops, lobbies. Spots: shop counters, arcade benches, a roof view point.
Ideas for later: shop signs with real text, cobogo screens in the passage, tiled mural on the gables, real lift cabs.
Material requests: none (uses CT_ConcreteWhite, CT_Tile, CT_Wood, CT_Neon...).
"""
import math
import random

from citykit import box, prism, DOOR_W, DOOR_H
import furn_int3 as fu
from furn_int3 import Fr, lbox, cyl, blob

INFO = {
    "name_ru": "Башня-Волна", "name_en": "Wave Tower",
    "style": "brazilian modernism", "use": "mixed",
    "inspired_by": "1950s-60s Sao Paulo / Rio residential towers with undulating brise-soleil, azulejo cores, "
                   "shopping arcades under glazed bases (Niemeyer / Reidy / Artigas era); original design",
}

N = 30                      # bays
STEP = 2.5
X0 = -N * STEP / 2          # -37.5
XS = [X0 + i * STEP for i in range(N + 1)]
DEPTH = 12.0
H_G, H_M, H_F = 3.5, 3.0, 3.0
Z_FLAT0 = 6.5
N_FLAT_ST = 16
Z_ROOF = Z_FLAT0 + H_F * N_FLAT_ST      # 54.5
ROOF_T = 0.4
CORES = [(4, 6, "core1"), (14, 16, "core2"), (24, 26, "core3")]
GROUPS = [(0, 4), (6, 14), (16, 24), (26, 30)]
UNITS = [(0, 4, "shop_a"), (4, 6, "core1"), (6, 9, "shop_b"), (9, 11, "passage"), (11, 14, "shop_c"),
         (14, 16, "core2"), (16, 20, "shop_d"), (20, 24, "shop_e"), (24, 26, "core3"), (26, 30, "shop_f")]


def yc(x):
    return 10.4 + 3.0 * math.sin(2 * math.pi * x / 75.0)


def yf(x):
    return yc(x) - DEPTH / 2


def yb(x):
    return yc(x) + DEPTH / 2


def slope(x):
    return 3.0 * 2 * math.pi / 75.0 * math.cos(2 * math.pi * x / 75.0)


def is_core(i):
    return any(a <= i < b for a, b, _ in CORES)


def _area(pts):
    return sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1] for i in range(len(pts)))


def sweep(m, mat, xs, base, section, caps=True):
    """Solid swept along +x: section(x) -> [(dy, z)] (convex, same count), y = base(x) + dy."""
    secs = [[(base(x) + dy, z) for dy, z in section(x)] for x in xs]
    ccw = _area(secs[0]) > 0
    n = len(secs[0])
    for j in range(len(xs) - 1):
        for i in range(n):
            k = (i + 1) % n
            A = (xs[j], secs[j][i][0], secs[j][i][1])
            Bq = (xs[j + 1], secs[j + 1][i][0], secs[j + 1][i][1])
            C = (xs[j + 1], secs[j + 1][k][0], secs[j + 1][k][1])
            D = (xs[j], secs[j][k][0], secs[j][k][1])
            m.face(mat, [A, D, C, Bq] if ccw else [A, Bq, C, D])
    if caps:
        for j, sign in ((0, -1), (len(xs) - 1, 1)):
            pts = [(xs[j], y, z) for y, z in secs[j]]
            if (sign > 0) != ccw:
                pts = pts[::-1]
            m.face(mat, pts)


def fwin(B, F, u0, u1, z0, z1, **kw):
    """Window card in a hole of facade F (corrects citykit.facade_window's inward-facing corner order)."""
    pts = [F.to_building((u, 0.0, z)) for u, z in ((u0, z0), (u1, z0), (u1, z1), (u0, z1))]
    kw.setdefault("inset", 0.2)
    return B.window(pts, **kw)


def _compose(rnd, n):
    out = []
    while n > 0:
        opts = [s for s in (2, 3, 4) if s <= n and (n - s == 0 or n - s >= 2)]
        s = rnd.choice(opts)
        out.append(s)
        n -= s
    return out


# ==================================================================================================== shops
SHOP_TRADES = {"shop_a": "bakery", "shop_b": "flowers", "shop_c": "books", "shop_d": "hardware",
               "shop_e": "pharmacy", "shop_f": "souvenirs"}
WARM, COOL = "#FFD2A0", "#FFF0DC"
Z_CEIL = 3.2


class Shop:
    """The geometry of one hollow shop unit: x range, curved front / back walls, frames along them."""
    def __init__(self, B, a, b, name, rnd):
        self.B, self.a, self.b, self.name, self.rnd = B, a, b, name, rnd
        self.xl, self.xr = XS[a] + 0.15, XS[b] - 0.15
        self.xm = (XS[a] + XS[b]) / 2
        self.W = self.xr - self.xl
        self.trade = SHOP_TRADES[name]

    def y_front(self, x):          # inner face of the shop front wall
        return yf(x) + 2.8

    def y_back(self, x):           # inner face of the back wall
        return yb(x) - 0.3

    def back_frames(self):
        """[(Fr on the inner back wall face at the bay's middle, u0, u1)] for the bays of the shop."""
        out = []
        for i in range(self.a, self.b):
            p0, p1 = (XS[i], yb(XS[i])), (XS[i + 1], yb(XS[i + 1]))
            L = math.dist(p0, p1)
            tx, ty = (p1[0] - p0[0]) / L, (p1[1] - p0[1]) / L
            yaw = math.degrees(math.atan2(ty, tx))
            cx, cy = (p0[0] + p1[0]) / 2 + ty * 0.3, (p0[1] + p1[1]) / 2 - tx * 0.3
            u0 = -L / 2 + (0.2 if i == self.a else 0.0)
            u1 = L / 2 - (0.2 if i == self.b - 1 else 0.0)
            out.append((Fr(cx, cy, yaw), u0, u1))
        return out

    def side_frames(self, lo=0.5, hi=1.0):
        """Left and right wall frames: (Fr, length). The right one runs from the back to the front."""
        yl = self.y_front(self.xl) + lo
        yh = self.y_back(self.xl) - 0.75 - hi + 1.0
        yl2 = self.y_front(self.xr) + lo
        yh2 = self.y_back(self.xr) - 0.75 - hi + 1.0
        return (Fr(self.xl, yl, 90), yh - yl), (Fr(self.xr, yh2, 270), yh2 - yl2)

    def counter_fr(self):
        ycn = yb(self.xm) - 4.2
        return Fr(self.xm, ycn, 0.0), ycn

    def lamps(self, colour, lumens=700, cols=2):
        B = self.B
        ys = [(self.y_front(self.xm) + 1.9), (self.y_back(self.xm) - 2.2)]
        xs = [self.xm + (c - (cols - 1) / 2) * self.W / (cols + 0.0) for c in range(cols)]
        for y in ys:
            for x in xs:
                B.lamp((x, y, 2.9), lumens=lumens, colour=colour, kind="public", radius=7.0)
                lbox(B.detail, "CT_Steel", Fr(x, y), -0.01, 0.01, -0.01, 0.01, 2.98, Z_CEIL, faces="xXyY")

    def awning(self):
        k = 0
        for i in range(self.a, self.b):
            p0, p1 = (XS[i], yf(XS[i]) + 2.5), (XS[i + 1], yf(XS[i + 1]) + 2.5)
            L = math.dist(p0, p1)
            tx, ty = (p1[0] - p0[0]) / L, (p1[1] - p0[1]) / L
            fr = Fr((p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2, math.degrees(math.atan2(ty, tx)))
            k = fu.awning(self.B, fr, -L / 2, L / 2, 1.15, 3.12, 2.62, k0=k, stripe=L / 5.0)

    def counter_top(self):
        pass


def _shelves_back(S, **kw):
    for fr, u0, u1 in S.back_frames():
        fu.shelving(S.B, fr, u0 + 0.02, u1 - 0.02, S.rnd, **kw)


def _shelves_sides(S, left=True, right=True, **kw):
    (fl, Ll), (fr_, Lr) = S.side_frames()
    spots = []
    if left:
        fu.shelving(S.B, fl, 0.0, Ll, S.rnd, **kw)
        spots.append((fl.p(Ll * 0.35, 0.95)[:2], 270))
    if right:
        fu.shelving(S.B, fr_, 0.0, Lr, S.rnd, **kw)
        spots.append((fr_.p(Lr * 0.65, 0.95)[:2], 90))
    for k, ((x, y), yaw) in enumerate(spots):
        S.B.spot("%s_browse%d" % (S.name, k + 1), "stand", (x, y, 0.0), yaw)


def _counter_register(S, cfr, mat="CT_Steel", u=1.0):
    d = S.B.detail
    lbox(d, mat, cfr, u - 0.18, u + 0.18, -0.55, -0.2, 1.0, 1.22, faces="xXyYZ")
    lbox(d, "CT_ConcreteDark", cfr, u - 0.14, u + 0.14, -0.42, -0.36, 1.22, 1.46, faces="xXyYZ")


def _island(S, x, y0, y1, rnd, **kw):
    """A double-sided free-standing shelf along y at x (back to back)."""
    B = S.B
    fu.shelving(B, Fr(x - 0.0, y0, 90), 0.0, y1 - y0, rnd, **kw)
    fu.shelving(B, Fr(x - 0.03, y1, 270), 0.0, y1 - y0, rnd, **kw)


def _bakery(S):
    B, rnd = S.B, S.rnd
    BREAD = ["CT_WoodLight", "CT_PlasterYellow", "CT_PlasterPink", "CT_WoodLight", "CT_PlasterYellow"]
    _shelves_back(S, tiers=3, zmax=2.05, z0=0.3, mats=BREAD, hh=(0.14, 0.26), fill=0.9, depth=0.45, carcass="CT_TileWhite")
    _shelves_sides(S, tiers=4, zmax=1.9, z0=0.3, mats=BREAD, hh=(0.12, 0.22), depth=0.4, left=True, right=False, carcass="CT_TileWhite")
    cfr, ycn = S.counter_fr()
    # glass pastry case to the right of the counter
    cx0, cx1 = 1.7, 4.0
    lbox(B.m, "CT_TileWhite", cfr, cx0, cx1, -0.75, 0.0, 0.0, 0.85)
    lbox(B.detail, "CT_WoodLight", cfr, cx0 - 0.02, cx1 + 0.02, -0.77, 0.02, 0.85, 0.89, faces="xXyYZ")
    for u in [cx0 + 0.25 + 0.3 * i for i in range(7)]:
        lbox(B.detail, rnd.choice(BREAD), cfr, u, u + 0.2, -0.6, -0.15, 0.89, 1.0 + rnd.random() * 0.08, faces="xXyYZ")
    # on the counter: cakes under domes, bread basket, scale, register
    for u, mat in ((-1.1, "CT_PlasterPink"), (-0.4, "CT_PlasterYellow")):
        px, py, _ = cfr.p(u, -0.35)
        cyl(B.detail, "CT_TileWhite", px, py, 1.0, 1.04, 0.2, 8, bottom=True)
        cyl(B.detail, mat, px, py, 1.04, 1.2, 0.15, 8)
        blob(B.detail, "CT_SteelRed", px, py, 1.26, 0.05, 5)
    fu.crate(B, *cfr.p(0.3, -0.35)[:2], 1.0, 0.5, 0.4, 0.14, "CT_WoodLight", fill="CT_WoodLight", yaw=cfr.yaw)
    _counter_register(S, cfr, u=1.3)
    # bread tables at the front, each with a basket of loaves
    for k, dx in enumerate((-3.3, -1.4, 3.1)):
        x, y = S.xm + dx, S.y_front(S.xm) + 1.7 + (0.5 if k == 1 else 0.0)
        fu.table_round(B, x, y, 0.55, 0.85)
        fu.crate(B, x, y, 0.85, 0.5, 0.36, 0.1, "CT_WoodLight", fill=rnd.choice(BREAD))
        for j in range(3):
            lbox(B.detail, rnd.choice(BREAD), Fr(x + 0.15 * (j - 1), y + 0.1 * j - 0.1), -0.03, 0.03, -0.2, 0.2, 0.95, 1.0, faces="xXyYZ")
    B.spot("shop_a_browse3", "stand", (S.xm - 1.4, S.y_front(S.xm) + 2.9, 0.0), 0.0)
    # baguette bin and flour sacks behind the counter
    bx, by, _ = Fr(S.xm - 3.2, S.y_back(S.xm) - 2.0).p(0, 0)
    cyl(B.detail, "CT_WoodLight", bx, by, 0.0, 0.7, 0.28, 8, top=False, bottom=True)
    for j in range(7):
        a = 2 * math.pi * j / 7
        lbox(B.detail, rnd.choice(("CT_WoodLight", "CT_PlasterYellow")), Fr(bx + 0.14 * math.cos(a), by + 0.14 * math.sin(a)),
             -0.025, 0.025, -0.025, 0.025, 0.4, 1.1 + 0.05 * j % 3, faces="xXyYZ")
    for j, (dx, dz) in enumerate(((0, 0), (0.5, 0), (0.25, 0.28))):
        lbox(B.detail, "CT_TileWhite", Fr(S.xm + 3.0 + dx, S.y_back(S.xm) - 1.0), -0.22, 0.22, -0.16, 0.16, dz, dz + 0.28, faces="xXyYZ")
    S.lamps(WARM, 760, cols=3)


def _flowers(S):
    B, rnd = S.B, S.rnd
    _shelves_back(S, tiers=3, zmax=2.0, z0=0.3, kind="jar", mats=["CT_PlasterBlue", "CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterPink"], depth=0.4, carcass="CT_TileWhite")
    _shelves_sides(S, tiers=3, zmax=1.7, z0=0.3, kind="jar", mats=["CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterBlue"], left=True, right=False, carcass="CT_TileWhite")
    cfr, ycn = S.counter_fr()
    # flower fridge on the right wall (glass front)
    fr_r = S.side_frames()[1][0]
    lbox(B.m, "CT_TileWhite", fr_r, 0.4, 2.2, 0.0, 0.7, 0.0, 2.1)
    for j in range(4):
        fu.bucket(B, *fr_r.p(0.7 + 0.4 * j, 0.35)[:2], 0.7, 0.1, 0.25, "CT_Steel", fu.FLOWERS, rnd)
    # buckets in rows at the front
    yb0 = S.y_front(S.xm) + 1.0
    for row in range(2):
        for j in range(5):
            x = S.xm - 2.6 + j * 1.3
            fu.bucket(B, x, yb0 + row * 0.9, 0.0, 0.2, 0.38, "CT_Steel", fu.FLOWERS, rnd)
    # tiered stand with potted plants
    sx, sy = S.xm + 1.9, S.y_front(S.xm) + 4.3
    for t in range(3):
        lbox(B.m, "CT_WoodLight", Fr(sx, sy), -0.9, 0.9, -0.3 + 0.2 * t, 0.3, 0.0, 0.3 + 0.3 * t)
    for t in range(3):
        for j in range(3):
            fu.plant(B, sx - 0.55 + 0.55 * j, sy - 0.1 + 0.2 * t, 0.3 + 0.3 * t, 0.3, rnd.choice(("CT_PlasterRed", "CT_PlasterBlue", "CT_TileWhite")), scale=0.5)
    B.spot("shop_b_browse3", "stand", (sx, sy - 1.4, 0.0), 180)
    # counter: paper roll, bouquets in a vase, scissors
    for u, mat in ((-1.0, "CT_PlasterPink"), (1.2, "CT_PlasterYellow")):
        px, py, _ = cfr.p(u, -0.3)
        cyl(B.detail, mat, px, py, 1.0, 1.3, 0.09, 8, bottom=True)
    px, py, _ = cfr.p(0.2, -0.3)
    fu.bucket(B, px, py, 1.0, 0.13, 0.25, "CT_PlasterBlue", fu.FLOWERS, rnd)
    _counter_register(S, cfr, u=-0.1)
    # hanging plants over the front window
    for j in range(5):
        x = S.xm - 2.8 + 1.4 * j
        y = S.y_front(x) + 0.45
        lbox(B.detail, "CT_Steel", Fr(x, y), -0.006, 0.006, -0.006, 0.006, 2.2, Z_CEIL, faces="xXyY")
        cyl(B.detail, "CT_PlasterRed", x, y, 2.05, 2.2, 0.1, 6, bottom=True)
        blob(B.detail, "CT_PlasterGreen", x, y, 2.0, 0.17, 6)
    S.lamps(WARM, 720, cols=2)


def _books(S):
    B, rnd = S.B, S.rnd
    BOOK = ["CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterGreen", "CT_WoodLight", "CT_PlasterYellow", "CT_TileWhite",
            "CT_PlasterPink", "CT_SteelRed"]
    _shelves_back(S, tiers=6, zmax=2.6, z0=0.1, kind="book", mats=BOOK, fill=0.95, depth=0.35)
    _shelves_sides(S, tiers=6, zmax=2.4, z0=0.1, kind="book", mats=BOOK, fill=0.95, depth=0.32)
    cfr, ycn = S.counter_fr()
    # two double-sided islands
    y0 = S.y_front(S.xm) + 2.3
    y1 = ycn - 0.9
    for dx in (-1.55, 1.55):
        _island(S, S.xm + dx, y0, y1, rnd, tiers=4, zmax=1.7, z0=0.1, kind="book", mats=BOOK, fill=0.95, depth=0.3)
    B.spot("shop_c_browse3", "stand", (S.xm - 2.45, (y0 + y1) / 2, 0.0), 90)
    B.spot("shop_c_browse4", "stand", (S.xm + 2.45, (y0 + y1) / 2, 0.0), 270)
    # reading corner at the front: round table with a stack and a chair
    tx, ty = S.xm, S.y_front(S.xm) + 1.2
    fu.table_round(B, tx, ty, 0.45, 0.74)
    lbox(B.detail, "CT_PlasterRed", Fr(tx, ty), -0.14, 0.14, -0.1, 0.1, 0.74, 0.8, faces="xXyYZ")
    lbox(B.detail, "CT_PlasterBlue", Fr(tx, ty, 20), -0.12, 0.12, -0.09, 0.09, 0.8, 0.85, faces="xXyYZ")
    fu.chair(B, tx - 0.75, ty, 90, "CT_PlasterGreen")
    fu.chair(B, tx + 0.75, ty, 270, "CT_PlasterYellow")
    B.spot("shop_c_seat1", "seat", (tx - 0.75, ty, 0.0), 90)
    B.spot("shop_c_seat2", "seat", (tx + 0.75, ty, 0.0), 270)
    # counter: book stacks, lamp, register
    for u, h in ((-1.2, 0.3), (-0.7, 0.2)):
        lbox(B.detail, rnd.choice(BOOK), cfr, u - 0.15, u + 0.15, -0.5, -0.2, 1.0, 1.0 + h, faces="xXyYZ")
    _counter_register(S, cfr, u=1.1)
    S.lamps(WARM, 720, cols=2)


def _hardware(S):
    B, rnd = S.B, S.rnd
    TOOLS = [("CT_SteelRed", 0.04, 0.28), ("CT_Steel", 0.05, 0.5), ("CT_WoodLight", 0.035, 0.4), ("CT_PlasterYellow", 0.04, 0.3),
             ("CT_Steel", 0.15, 0.04)]
    _shelves_back(S, tiers=3, zmax=1.4, z0=0.0, kind="can", mats=["CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterGreen", "CT_TileWhite"],
                  carcass="CT_Steel", depth=0.45)
    # pegboard with tools over the shelves
    for fr, u0, u1 in S.back_frames():
        lbox(B.detail, "CT_ConcreteDark", fr, u0 + 0.02, u1 - 0.02, 0.0, 0.02, 1.5, 2.7)
        u = u0 + 0.2
        while u < u1 - 0.2:
            mat, w, h = rnd.choice(TOOLS)
            lbox(B.detail, mat, fr, u, u + w, 0.02, 0.06, 2.45 - h, 2.45, faces="xXyYZ")
            u += w + rnd.uniform(0.1, 0.22)
    _shelves_sides(S, tiers=4, zmax=2.0, z0=0.0, kind="box", mats=["CT_PlasterYellow", "CT_SteelRed", "CT_PlasterBlue", "CT_Steel"],
                   carcass="CT_Steel", depth=0.4)
    cfr, ycn = S.counter_fr()
    # two steel bin racks (islands) with crates
    y0 = S.y_front(S.xm) + 2.2
    y1 = ycn - 1.0
    for dx in (-2.0, 2.0):
        x = S.xm + dx
        fu.shelving(B, Fr(x, y0, 90), 0.0, y1 - y0, rnd, tiers=3, zmax=1.5, z0=0.0, kind="box",
                    mats=["CT_Steel", "CT_SteelRed", "CT_PlasterBlue", "CT_PlasterYellow"], carcass="CT_Steel", depth=0.5, hh=(0.12, 0.26))
        fu.shelving(B, Fr(x - 0.03, y1, 270), 0.0, y1 - y0, rnd, tiers=3, zmax=1.5, z0=0.0, kind="box",
                    mats=["CT_Steel", "CT_SteelRed", "CT_PlasterBlue", "CT_PlasterYellow"], carcass="CT_Steel", depth=0.5, hh=(0.12, 0.26))
    B.spot("shop_d_browse3", "stand", (S.xm - 3.0, (y0 + y1) / 2, 0.0), 90)
    B.spot("shop_d_browse4", "stand", (S.xm + 3.0, (y0 + y1) / 2, 0.0), 270)
    # paint cans pyramid, cement sacks and tool leaning in the front corner
    px, py = S.xm + 3.6, S.y_front(S.xm + 3.6) + 1.2
    for lvl, n in enumerate((4, 3, 2, 1)):
        for j in range(n):
            cyl(B.detail, ("CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow")[(lvl + j) % 3], px + (j - (n - 1) / 2) * 0.3, py, lvl * 0.22,
                lvl * 0.22 + 0.2, 0.13, 8)
    for j, (dx, dz) in enumerate(((0, 0), (0.62, 0), (0.31, 0.2), (0, 0.4))):
        lbox(B.detail, "CT_Concrete", Fr(S.xm - 3.7 + dx, S.y_front(S.xm - 3.7) + 1.3 + 0.1 * j), -0.28, 0.28, -0.2, 0.2, dz, dz + 0.2, faces="xXyYZ")
    for j, mat in enumerate(("CT_Steel", "CT_WoodLight", "CT_Steel", "CT_SteelRed")):      # brooms, shovels leaning on the front wall
        x = S.xm - 1.0 + j * 0.28
        lbox(B.detail, mat, Fr(x, S.y_front(x) + 0.12), -0.02, 0.02, -0.02, 0.02, 0.0, 1.7 - 0.1 * j, faces="xXyYZ")
    # counter: a toolbox and tape reels
    lbox(B.detail, "CT_SteelRed", cfr, -1.2, -0.7, -0.5, -0.25, 1.0, 1.18, faces="xXyYZ")
    cyl(B.detail, "CT_PlasterYellow", *cfr.p(0.1, -0.35)[:2], 1.0, 1.1, 0.1, 8)
    _counter_register(S, cfr, u=1.1)
    S.lamps("#FFE2B8", 800, cols=3)


def _pharmacy(S):
    B, rnd = S.B, S.rnd
    PK = ["CT_TileWhite", "CT_PlasterGreen", "CT_PlasterBlue", "CT_TileWhite", "CT_PlasterPink"]
    # back wall: drawer cabinet (white, green handles) with shelves of boxes above, a green cross
    for fr, u0, u1 in S.back_frames():
        lbox(B.m, "CT_TileWhite", fr, u0 + 0.02, u1 - 0.02, 0.0, 0.5, 0.0, 1.1)
        n = int((u1 - u0) / 0.45)
        for i in range(n):
            for r in range(3):
                uu = u0 + 0.04 + i * (u1 - u0 - 0.08) / n
                lbox(B.detail, "CT_PlasterGreen", fr, uu + 0.12, uu + (u1 - u0 - 0.08) / n - 0.12, 0.5, 0.52,
                     0.18 + r * 0.33, 0.22 + r * 0.33, faces="xXyYZ")
    _shelves_back_hi(S, PK)
    mid = S.back_frames()[len(S.back_frames()) // 2][0]
    lbox(B.detail, "CT_PlasterGreen", mid, -0.35, 0.35, 0.0, 0.05, 2.78, 3.0, faces="xXyYZ")
    lbox(B.detail, "CT_PlasterGreen", mid, -0.11, 0.11, 0.0, 0.05, 2.58, 3.2, faces="xXyYZ")
    cfr, ycn = S.counter_fr()
    # glass display counter left of the counter, and a cosmetics island
    lbox(B.m, "CT_TileWhite", cfr, -4.3, -1.8, -0.7, 0.0, 0.0, 0.9)
    lbox(B.detail, "CT_ConcreteDark", cfr, -4.32, -1.78, -0.72, 0.02, 0.9, 0.94, faces="xXyYZ")
    for u in [-4.1 + 0.3 * i for i in range(7)]:
        lbox(B.detail, rnd.choice(PK), cfr, u, u + 0.2, -0.55, -0.2, 0.94, 1.08, faces="xXyYZ")
    y0 = S.y_front(S.xm) + 2.5
    y1 = ycn - 1.2
    _island(S, S.xm + 0.0, y0, y1, rnd, tiers=4, zmax=1.6, z0=0.2, kind="box", mats=PK, carcass="CT_TileWhite", depth=0.35)
    B.spot("shop_e_browse3", "stand", (S.xm - 0.9, (y0 + y1) / 2, 0.0), 90)
    B.spot("shop_e_browse4", "stand", (S.xm + 0.9, (y0 + y1) / 2, 0.0), 270)
    # waiting chairs along the right wall, magazine table, a pill-dispenser scale
    (fl, Ll), (frr, Lr) = S.side_frames()
    for j in range(3):
        px, py, _ = frr.p(Lr - 0.5 - j * 0.6, 0.35)
        fu.chair(B, px, py, 270, "CT_PlasterGreen")
        B.spot("shop_e_seat%d" % (j + 1), "seat", (px, py, 0.0), 270)
    lbox(B.detail, "CT_TileWhite", fl, 1.2, 1.5, 0.15, 0.45, 0.0, 1.5, faces="xXyYZ")
    lbox(B.detail, "CT_ConcreteDark", fl, 1.22, 1.48, 0.45, 0.47, 1.1, 1.4, faces="Y")
    # counter: tablet blisters, hand sanitiser, register
    for u in (-1.2, -0.9):
        lbox(B.detail, "CT_PlasterBlue", cfr, u, u + 0.2, -0.5, -0.2, 1.0, 1.15, faces="xXyYZ")
    cyl(B.detail, "CT_PlasterGreen", *cfr.p(0.2, -0.3)[:2], 1.0, 1.2, 0.05, 6)
    _counter_register(S, cfr, u=1.1)
    S.lamps(COOL, 800, cols=3)


def _shelves_back_hi(S, PK):
    for fr, u0, u1 in S.back_frames():
        fu.shelving(S.B, fr, u0 + 0.02, u1 - 0.02, S.rnd, tiers=3, zmax=2.5, z0=1.2, kind="box", mats=PK,
                    carcass="CT_TileWhite", depth=0.3, hh=(0.1, 0.22), solid=False)


def _souvenirs(S):
    B, rnd = S.B, S.rnd
    AMBER = ["CT_PlasterYellow", "CT_Copper", "CT_PlasterYellow", "CT_Brick"]
    TOYS = ["CT_WoodLight", "CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow", "CT_Copper"]
    _shelves_back(S, tiers=4, zmax=2.3, z0=0.3, kind="box", mats=TOYS, depth=0.38, hh=(0.1, 0.24))
    _shelves_sides(S, tiers=4, zmax=2.0, z0=0.3, kind="jar", mats=AMBER, left=True, right=True, depth=0.35)
    cfr, ycn = S.counter_fr()
    # two glass cases with amber, one big amber chunk on a plinth in the middle
    y = S.y_front(S.xm) + 2.8
    for dx in (-2.6, 2.6):
        fr = Fr(S.xm + dx, y, 0.0)
        lbox(B.m, "CT_WoodLight", fr, -0.9, 0.9, -0.35, 0.35, 0.0, 0.85)
        lbox(B.detail, "CT_ConcreteDark", fr, -0.92, 0.92, -0.37, 0.37, 0.85, 0.89, faces="xXyYZ")
        for k in range(9):
            blob(B.detail, rnd.choice(AMBER), fr.p(-0.75 + 0.19 * k, rnd.uniform(-0.15, 0.15))[0],
                 y + rnd.uniform(-0.15, 0.15), 0.89 + 0.07, rnd.uniform(0.05, 0.09), 6)
        B.spot("shop_f_browse%d" % (1 if dx < 0 else 2), "stand", (S.xm + dx, y - 0.9, 0.0), 0.0)
    cx, cy = S.xm, S.y_front(S.xm) + 4.0
    cyl(B.m, "CT_ConcreteDark", cx, cy, 0.0, 0.7, 0.4, 8)
    blob(B.detail, "CT_PlasterYellow", cx, cy, 0.7 + 0.28, 0.32, 8)
    blob(B.detail, "CT_Copper", cx + 0.15, cy - 0.1, 0.7 + 0.62, 0.17, 7)
    # postcard spinner rack and a bowl of shells near the door
    rx, ry = S.xm + 3.9, S.y_front(S.xm + 3.9) + 2.8
    cyl(B.detail, "CT_Steel", rx, ry, 0.0, 1.6, 0.025, 4)
    cyl(B.detail, "CT_Steel", rx, ry, 0.0, 0.03, 0.22, 8)
    for k in range(4):
        lbox(B.detail, rnd.choice(TOYS), Fr(rx, ry, 90 * k), -0.12, 0.12, 0.1, 0.12, 1.0 + 0.15 * (k % 2), 1.35 + 0.15 * (k % 2), faces="xXyYZ")
    # amber strings hanging from the ceiling over the counter
    for k in range(7):
        x = S.xm - 1.5 + 0.5 * k
        y2 = ycn - 0.4
        lbox(B.detail, "CT_Steel", Fr(x, y2), -0.005, 0.005, -0.005, 0.005, 1.7, Z_CEIL, faces="xXyY")
        for j in range(5):
            blob(B.detail, rnd.choice(AMBER), x, y2, 1.78 + 0.07 * j, 0.04, 6)
    # counter: mugs, small ships, register
    for k in range(5):
        cyl(B.detail, rnd.choice(("CT_PlasterBlue", "CT_TileWhite", "CT_PlasterRed")), *cfr.p(-1.4 + 0.25 * k, -0.25)[:2], 1.0, 1.1, 0.05, 6)
    for u in (0.1, 0.55):
        lbox(B.detail, "CT_WoodLight", cfr, u, u + 0.28, -0.5, -0.3, 1.0, 1.06, faces="xXyYZ")
        lbox(B.detail, "CT_TileWhite", cfr, u + 0.12, u + 0.14, -0.41, -0.39, 1.06, 1.3, faces="xXyYZ")
    _counter_register(S, cfr, u=1.2)
    S.lamps("#FFC888", 780, cols=3)


def _lobby(B, rnd, a, b, name):
    """A core lobby: mailboxes on the left wall, a plant, a doormat inside the door, a lobby lamp."""
    xl, xr = XS[a] + 0.15, XS[b] - 0.15
    xm = (XS[a] + XS[b]) / 2
    yfm = yf(xm) + 2.8
    yy = yfm + 1.6
    fu_fr = Fr(xl, yy - 0.0, 90)
    fu.mailboxes(B, fu_fr, 1.3, 0.9, cols=6, rows=4)
    # doormat inside the door (door is in the first bay)
    xd = (XS[a] + XS[a + 1]) / 2
    fu.doormat(B, Fr(xd, yf(xd) + 2.8, 0.0), 0.0, -0.45, z=0.0)
    for (px, py) in ((xr - 0.45, yy + 1.5), (xl + 0.5, yy + 5.2)):
        fu.plant(B, px, py, 0.0, 0.9, "CT_PlasterBlue" if px > xm else "CT_PlasterRed")
    B.lamp((xm, yy + 0.8, 2.9), lumens=520, colour="#FFE0B0", kind="lobby", radius=6.0)
    lbox(B.detail, "CT_Steel", Fr(xm, yy + 0.8), -0.01, 0.01, -0.01, 0.01, 2.98, Z_CEIL, faces="xXyY")


def furnish(B, rnd):
    for a, b, name in UNITS:
        if name in SHOP_TRADES:
            S = Shop(B, a, b, name, rnd)
            S.awning()
            {"bakery": _bakery, "flowers": _flowers, "books": _books, "hardware": _hardware,
             "pharmacy": _pharmacy, "souvenirs": _souvenirs}[S.trade](S)
            ycn = yb(S.xm) - 4.2
            B.spot(name + "_clerk", "work", (S.xm, ycn + 1.3, 0.0), 0.0)
            B.spot(name + "_queue", "stand", (S.xm, ycn - 0.9, 0.0), 180)
        elif name.startswith("core"):
            _lobby(B, rnd, a, b, name)


def build(B):
    rnd = random.Random(1958)
    zt = Z_ROOF
    chord = lambda i, d: (XS[i], yf(XS[i]) + d)
    front_line = lambda d: [chord(i, d) for i in range(N + 1)]
    back_line = [(XS[i], yb(XS[i])) for i in range(N + 1)]

    # ---------------------------------------------------------------- slabs
    arc = front_line(-1.8) + [(XS[i], yf(XS[i]) + 2.5) for i in range(N, -1, -1)]
    prism(B.m, "CT_Paving", arc, -0.4, 0.0, top=True, bottom=True, side_mat="CT_ConcreteDark")
    inner = front_line(2.5) + back_line[::-1]
    prism(B.m, "CT_Floor", inner, -0.4, 0.0, top=True, bottom=True, side_mat="CT_ConcreteDark")
    mezz = front_line(0.0) + back_line[::-1]
    prism(B.m, "CT_ConcreteDark", mezz, 3.2, 3.5, top=True, bottom=True)
    prism(B.m, "CT_Roof", mezz, zt, zt + ROOF_T, top=True, bottom=True, side_mat="CT_ConcreteWhite")

    # ---------------------------------------------------------------- brise-soleil bands (front)
    fine = [X0 + k * (N * STEP) / 120.0 for k in range(121)]
    for j in range(N_FLAT_ST + 2):
        zs = H_G + H_F * j
        top = 0.75
        ph = 0.55 * j

        def sec(x, zs=zs, ph=ph, top=top):
            out = 1.6 + 0.32 * math.sin(2 * math.pi * x / 11.0 + ph)
            return [(0.2, zs - 0.35), (-out, zs - 0.35), (-out, zs + top), (0.2, zs + 0.25)]
        sweep(B.m, "CT_ConcreteWhite", fine, yf, sec)

    # ---------------------------------------------------------------- flats layout
    flats = []                       # (storey, n, a, b)
    seg_flat = {}                    # (storey, seg) -> fid
    for s in range(2, 2 + N_FLAT_ST):
        n = 0
        for ga, gb in GROUPS:
            a = ga
            for size in _compose(rnd, gb - ga):
                n += 1
                fid = "%d-%02d" % (s, n)
                flats.append((s, n, a, a + size, fid))
                for i in range(a, a + size):
                    seg_flat[(s, i)] = fid
                a += size

    # ---------------------------------------------------------------- front facade (upper, mezz + flats)
    for i in range(N):
        x0, x1 = XS[i], XS[i + 1]
        p0, p1 = (x0, yf(x0)), (x1, yf(x1))
        L = math.dist(p0, p1)
        holes = []
        core = is_core(i)
        first_core = core and (i == min(a for a, b, _ in CORES if a <= i < b))
        if not core:
            holes.append((0.25, L - 0.25, 4.2, 6.15))
        elif first_core:
            holes.append((0.8, L - 0.8, 4.4, 6.0))
        for s in range(2, 2 + N_FLAT_ST):
            zs = Z_FLAT0 + H_F * (s - 2)
            if not core:
                holes.append((0.25, L - 0.25, zs + 0.7, zs + 2.55))
            elif first_core:
                holes.append((0.8, L - 0.8, zs + 0.8, zs + 2.3))
        F = B.facade(p0, p1, 3.5, zt, "CT_Tile" if core else "CT_ConcreteWhite", holes=holes, depth=0.3)
        if not core:
            fwin(B, F, 0.25, L - 0.25, 4.2, 6.15, kind="public", cols=2, rows=1, frame=0.06)
        elif first_core:
            fwin(B, F, 0.8, L - 0.8, 4.4, 6.0, kind="stair", frame=0.05)
        for s in range(2, 2 + N_FLAT_ST):
            zs = Z_FLAT0 + H_F * (s - 2)
            if not core:
                fwin(B, F, 0.25, L - 0.25, zs + 0.7, zs + 2.55, flat=seg_flat[(s, i)], kind="flat", cols=2, rows=1, frame=0.05)
            elif first_core:
                fwin(B, F, 0.8, L - 0.8, zs + 0.8, zs + 2.3, kind="stair", frame=0.05)

    # ---------------------------------------------------------------- back facade (upper + ground)
    passage_segs = set(range(9, 11))
    for i in range(N):
        x0, x1 = XS[i], XS[i + 1]
        p0, p1 = (x1, yb(x1)), (x0, yb(x0))
        L = math.dist(p0, p1)
        core = is_core(i)
        first_core = core and (i == min(a for a, b, _ in CORES if a <= i < b))
        holes = []
        for s in range(2, 2 + N_FLAT_ST):
            zs = Z_FLAT0 + H_F * (s - 2)
            if not core:
                holes.append((0.6, L - 0.6, zs + 0.95, zs + 2.35))
            elif first_core:
                holes.append((0.85, L - 0.85, zs + 0.8, zs + 2.3))
        F = B.facade(p0, p1, 3.5, zt, "CT_Tile" if core else "CT_Plaster", holes=holes, depth=0.3)
        for s in range(2, 2 + N_FLAT_ST):
            zs = Z_FLAT0 + H_F * (s - 2)
            if not core:
                fwin(B, F, 0.6, L - 0.6, zs + 0.95, zs + 2.35, flat=seg_flat[(s, i)], kind="flat", cols=2, rows=1, frame=0.05)
            elif first_core:
                fwin(B, F, 0.85, L - 0.85, zs + 0.8, zs + 2.3, kind="stair", frame=0.05)
        if i not in passage_segs:
            B.facade(p0, p1, 0.0, 3.5, "CT_ConcreteWhite", depth=0.3)

    # ---------------------------------------------------------------- end gables (blue tile, side windows)
    for xe, left in ((X0, True), (-X0, False)):
        p0, p1 = ((xe, yb(xe)), (xe, yf(xe))) if left else ((xe, yf(xe)), (xe, yb(xe)))
        L = DEPTH
        holes = []
        for s in range(2, 2 + N_FLAT_ST):
            zs = Z_FLAT0 + H_F * (s - 2)
            holes.append((L / 2 - 0.8, L / 2 + 0.8, zs + 0.9, zs + 2.4))
        F = B.facade(p0, p1, 0.0, zt, "CT_Tile", holes=holes, depth=0.3)
        for s in range(2, 2 + N_FLAT_ST):
            zs = Z_FLAT0 + H_F * (s - 2)
            fid = seg_flat[(s, 0 if left else N - 1)]
            fwin(B, F, L / 2 - 0.8, L / 2 + 0.8, zs + 0.9, zs + 2.4, flat=fid, kind="flat", cols=2, rows=1, frame=0.05)

    # ---------------------------------------------------------------- flats data
    for s, n, a, b, fid in flats:
        zs = Z_FLAT0 + H_F * (s - 2)
        best = None
        for ca, cb, cid in CORES:
            for edge in (a, b):
                for ce in (ca, cb):
                    d = abs(XS[edge] - XS[ce])
                    if best is None or d < best[0]:
                        best = (d, XS[edge], cid)
        xd = best[1]
        B.flat(fid, storey=s, beds={2: 1, 3: 2, 4: 3}[b - a], door=(xd, yc(xd), zs + 0.1), entrance=best[2])

    # ---------------------------------------------------------------- arcade columns, benches, signs
    for i in range(0, N + 1, 2):
        B.column_round(XS[i], yf(XS[i]) - 0.65, 0.0, 3.2, 0.22, mat="CT_ConcreteWhite", segs=12)
    for bx in (-33.0, 17.5):
        by = yf(bx) + 1.2
        box(B.detail, "CT_Wood", bx - 0.9, bx + 0.9, by, by + 0.45, 0.4, 0.5)
        box(B.detail, "CT_Steel", bx - 0.8, bx - 0.7, by + 0.05, by + 0.4, 0.0, 0.4)
        box(B.detail, "CT_Steel", bx + 0.7, bx + 0.8, by + 0.05, by + 0.4, 0.0, 0.4)
        B.spot("arcade_bench_%d" % int(bx), "bench", (bx, by + 0.7, 0.0), 0.0)

    # ---------------------------------------------------------------- ground units
    for a, b, name in UNITS:
        if name == "passage":
            continue
        core = name.startswith("core")
        for i in range(a, b):
            x0, x1 = XS[i], XS[i + 1]
            p0, p1 = (x0, yf(x0) + 2.5), (x1, yf(x1) + 2.5)
            L = math.dist(p0, p1)
            door_seg = a + (b - a - 1) // 2 if not core else a
            holes = []
            if i == door_seg:
                holes += [(L / 2 - DOOR_W / 2 - 0.05, L / 2 + DOOR_W / 2 + 0.05, 0.0, 2.3),
                          (L / 2 - DOOR_W / 2 - 0.05, L / 2 + DOOR_W / 2 + 0.05, 2.5, 3.05)]
            else:
                holes.append((0.2, L - 0.2, 0.45 if not core else 0.7, 3.05 if not core else 2.8))
            F = B.facade(p0, p1, 0.0, 3.2, "CT_Plaster" if not core else "CT_Tile", holes=holes, depth=0.3)
            kind = "stair" if core else "public"
            if i == door_seg:
                fwin(B, F, L / 2 - DOOR_W / 2 - 0.05, L / 2 + DOOR_W / 2 + 0.05, 2.5, 3.05, kind=kind, frame=0.04, inset=0.15)
                xm = (x0 + x1) / 2
                B.entrance("shop_door_" + name if not core else name, (xm, yf(xm) + 2.5, 0.0),
                           round(math.degrees(math.atan(slope(xm))), 1), width=DOOR_W, height=DOOR_H,
                           kind="street", leaves=1)
            else:
                fwin(B, F, 0.2, L - 0.2, 0.45 if not core else 0.7, 3.05 if not core else 2.8, kind=kind, cols=2 if L > 2 else 1, rows=1, frame=0.05, inset=0.15)
        if not core:
            xm = (XS[a] + XS[b]) / 2
            ycn = yb(xm) - 4.2
            box(B.m, "CT_Wood", xm - 1.6, xm + 1.6, ycn, ycn + 0.7, 0.0, 1.0)
            B.spot(name + "_counter", "counter", (xm, ycn - 0.1, 0.0), 0.0)
    furnish(B, random.Random(1960))
    # partitions between units (the passage sides are blue tile)
    bounds = sorted({a for a, b, _ in UNITS} | {b for a, b, _ in UNITS})
    for k in bounds:
        if k in (0, N):
            continue
        x = XS[k]
        names = {n: (a, b) for a, b, n in UNITS}
        mat = "CT_Tile" if any(n == "passage" and (a == k or b == k) for a, b, n in UNITS) else "CT_Interior"
        box(B.m, mat, x - 0.15, x + 0.15, yf(x) + 2.7, yb(x) - 0.2, 0.0, 3.2)

    # ---------------------------------------------------------------- roof
    ring = front_line(0.4)[:] + [(XS[-1] - 0.4, yf(XS[-1]) + 0.4)]
    pts = [(XS[0] + 0.4, yf(XS[0]) + 0.4)] + front_line(0.4)[1:-1] + [(XS[-1] - 0.4, yf(XS[-1]) + 0.4),
           (XS[-1] - 0.4, yb(XS[-1]) - 0.4)] + [(XS[i], yb(XS[i]) - 0.4) for i in range(N - 1, 0, -1)] + \
          [(XS[0] + 0.4, yb(XS[0]) - 0.4), (XS[0] + 0.4, yf(XS[0]) + 0.4)]
    B.railing(pts, zt + ROOF_T, h=1.05, post=1.5)
    zr = zt + ROOF_T
    # plant rooms over the cores
    for ca, cb, cid in CORES:
        xm = (XS[ca] + XS[cb]) / 2
        h = 2.9 if cid != "core2" else 2.5
        box(B.m, "CT_ConcreteWhite", xm - 3.2, xm + 3.2, yc(xm) - 3.0, yc(xm) + 3.0, zr, zr + h)
        box(B.m, "CT_Steel", xm - 3.3, xm + 3.3, yc(xm) - 3.1, yc(xm) + 3.1, zr + h, zr + h + 0.12)
        # louvre band on the sea side
        for k in range(8):
            xx = xm - 2.8 + k * 0.8
            box(B.detail, "CT_Louvre", xx, xx + 0.1, yc(xm) - 3.08, yc(xm) - 3.0, zr + 0.6, zr + h - 0.4)
    B.column_round(-12.0, yc(-12.0) + 1.0, zr, zr + 2.6, 1.6, mat="CT_ConcreteDark", segs=16)
    prism(B.m, "CT_Steel", [(-12.0 + 1.7 * math.cos(2 * math.pi * k / 16), yc(-12.0) + 1.0 + 1.7 * math.sin(2 * math.pi * k / 16)) for k in range(16)], zr + 2.6, zr + 2.75, top=True, bottom=True)
    # terrace deck and wave canopy
    tf = [6.0 + k * 28.0 / 56 for k in range(57)]
    sweep(B.m, "CT_Wood", tf, yc, lambda x: [(-4.2, zr), (4.2, zr), (4.2, zr + 0.1), (-4.2, zr + 0.1)][::-1] if False else [(-4.2, zr), (4.2, zr), (4.2, zr + 0.1), (-4.2, zr + 0.1)])
    cz = lambda x: 56.85 + 0.35 * math.sin(2 * math.pi * x / 12.0)
    cf = [8.0 + k * 24.0 / 96 for k in range(97)]
    sweep(B.m, "CT_ConcreteWhite", cf, yc, lambda x: [(-3.2, cz(x) - 0.08), (3.2, cz(x) - 0.08), (3.2, cz(x) + 0.12), (-3.2, cz(x) + 0.12)])
    for px in (10.0, 16.0, 22.0, 28.0):
        for dy in (-2.9, 2.9):
            B.column_round(px, yc(px) + dy, zr + 0.1, cz(px) - 0.05, 0.12, mat="CT_Steel", segs=8, m=B.detail)
    for bx in (13.0, 25.0):
        box(B.detail, "CT_Wood", bx - 1.0, bx + 1.0, yc(bx) - 1.8, yc(bx) - 1.35, zr + 0.5, zr + 0.6)
        B.spot("roof_bench_%d" % int(bx), "bench", (bx, yc(bx) - 1.0, zr + 0.1), 0.0)
    B.spot("roof_view", "view", (20.0, yf(20.0) + 0.9, zr), 0.0)

    # ---------------------------------------------------------------- footprint, height
    right = [(XS[i], yf(XS[i]) - 1.5) for i in range(N + 1)]
    B.footprint = right + [(XS[-1], yb(XS[-1]))] + [(XS[i], yb(XS[i])) for i in range(N - 1, -1, -1)]
    B.height = zr + 2.9 + 0.12
    B.note("%d flats on %d storeys, S-curved slab with undulating brise-soleil bands" % (len(flats), N_FLAT_ST))
