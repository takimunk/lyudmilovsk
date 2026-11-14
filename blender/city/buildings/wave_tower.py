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
import os
import random

import interior_kit as kit
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
# Round 3 (interior kit): the six ground-floor units are eight trades. The 7.5 m shops hold one trade; the 10 m
# units E and F are two small shops each, left and right of the aisle that leads from the street door:
#   A bakery with a cafe corner | B records and radios | C books | D hardware and garden |
#   E savings bank (left) + hairdresser (right) | F fabrics (left) + toys (right)
SHOP_TRADES = {"shop_a": "bakery", "shop_b": "records", "shop_c": "books", "shop_d": "hardware",
               "shop_e": "salon", "shop_f": "toyfab"}
WARM, COOL = "#FFD2A0", "#FFF0DC"
Z_CEIL = 3.2
PAPER_TOPICS = ["birds", "sea", "science", "poetry", "kids", "travel", "local", "art", "garden"]


def _th(x):
    return math.atan(slope(x))


def _poly_inside(pt, poly):
    c = False
    n = len(poly)
    for i in range(n):
        x0, y0 = poly[i]
        x1, y1 = poly[(i + 1) % n]
        if (y0 > pt[1]) != (y1 > pt[1]) and pt[0] < (x1 - x0) * (pt[1] - y0) / (y1 - y0) + x0:
            c = not c
    return c


class Shop:
    """One hollow shop unit. Frame (u, v): u = metres from the left (west) wall, v = metres from the front wall (in y);
    pieces placed with f() turn with the front wall. Wall runs: back(), left(), right()."""

    def __init__(self, B, a, b, name, rnd):
        self.B, self.a, self.b, self.name, self.rnd = B, a, b, name, rnd
        self.xl = XS[a] + (0.3 if a == 0 else 0.15)
        self.xr = XS[b] - (0.3 if b == N else 0.15)
        self.xm = (XS[a] + XS[b]) / 2
        self.W = self.xr - self.xl
        self.trade = SHOP_TRADES[name]
        door_seg = a + (b - a - 1) // 2
        self.xd = (XS[door_seg] + XS[door_seg + 1]) / 2
        self.ud = self.xd - self.xl
        xs = [self.xl + (self.xr - self.xl) * k / 24.0 for k in range(25)]
        self.poly = [(x, self.y_front(x)) for x in xs] + [(x, self.y_back(x)) for x in reversed(xs)]
        self.warn = []
        self.tris = 0

    # ------------------------------------------------------------------ geometry
    def y_front(self, x):          # inner face of the shop front wall
        return yf(x) + 2.5 + 0.3 / math.cos(_th(x))

    def y_back(self, x):           # inner face of the back wall
        return yb(x) - 0.3 / math.cos(_th(x))

    def P(self, u, v):
        x = self.xl + u
        return (x, self.y_front(x) + v)

    def yw(self, u):
        return math.degrees(_th(self.xl + u))

    def vback(self, u):
        x = self.xl + u
        return self.y_back(x) - self.y_front(x)

    def _edge_dist(self, p):
        best = 9.0
        n = len(self.poly)
        for i in range(n):
            ax, ay = self.poly[i]
            bx, by = self.poly[(i + 1) % n]
            dx, dy = bx - ax, by - ay
            t = max(0.0, min(1.0, ((p[0] - ax) * dx + (p[1] - ay) * dy) / (dx * dx + dy * dy)))
            best = min(best, math.hypot(p[0] - ax - t * dx, p[1] - ay - t * dy))
        return best

    def check(self, r):
        f = r.get("foot")
        if f:
            for p in f:
                if not _poly_inside(p, self.poly) and self._edge_dist(p) > 0.06:
                    self.warn.append("%s: %s outside at (%.2f, %.2f)" % (self.name, r["name"], p[0], p[1]))
                    break
        self.tris += r.get("tris", 0)
        return r

    # ------------------------------------------------------------------ placing
    def f(self, fn, u, v, a=0.0, **kw):
        """A floor piece at (u, v); a = angle from 'facing the street' (180 = facing the back wall)."""
        x, y = self.P(u, v)
        return self.check(fn(self.B, x, y, 0.0, self.yw(u) + a, **kw))

    def back(self, fn, u, **kw):
        """A wall piece on the back wall at u, facing into the room (along the wall's own slope)."""
        x = self.xl + u
        th = _th(x)
        y = yb(x) - 0.3 / math.cos(th) + 0.025
        return self.check(fn(self.B, x, y, 0.0, math.degrees(th), **kw))

    def left(self, fn, s, **kw):
        y = self.y_front(self.xl) + s
        return self.check(fn(self.B, self.xl + 0.002, y, 0.0, 90.0, **kw))

    def right(self, fn, s, **kw):
        y = self.y_front(self.xr) + s
        return self.check(fn(self.B, self.xr - 0.002, y, 0.0, 270.0, **kw))

    def left_f(self, fn, s, off=0.35, **kw):
        """A free-standing piece against the left wall (its centre `off` from the wall), facing +x."""
        return self.check(fn(self.B, self.xl + off, self.y_front(self.xl) + s, 0.0, 90.0, **kw))

    def right_f(self, fn, s, off=0.35, **kw):
        return self.check(fn(self.B, self.xr - off, self.y_front(self.xr) + s, 0.0, 270.0, **kw))

    def back_run(self, fn, u0, u1, w=0.95, **kw):
        n = max(1, int(round((u1 - u0) / w)))
        ww = (u1 - u0) / n
        return [self.back(fn, u0 + (k + 0.5) * ww, w=ww - 0.1, seed=k + 3 + int(self.xm), **kw) for k in range(n)]

    def side_run(self, fn, side, s0, s1, w=0.95, **kw):
        n = max(1, int(round((s1 - s0) / w)))
        ww = (s1 - s0) / n
        put = self.left if side == "l" else self.right
        return [put(fn, s0 + (k + 0.5) * ww, w=ww - 0.1, seed=k + 7 + int(self.xm), **kw) for k in range(n)]

    # ------------------------------------------------------------------ things and spots
    def counter(self, fn, u, v, tag, w, role="seller", front="CT_PlasterRed", top="CT_TileWhite", **props):
        """A shop counter facing the street; spots counter/clerk/queue and a counter thing."""
        B = self.B
        r = self.f(fn, u, v, 0, w=w, front=front, top=top)
        yaw = self.yw(u)
        self.counter_spots("%s%s" % (self.name, tag), r, yaw)
        B.thing("%s%s_counter" % (self.name, tag), "counter", (r["pos"][0], r["pos"][1], 1.05), yaw, role=role, **props)
        return r

    def counter_spots(self, sid, r, yaw):
        B = self.B
        a = math.radians(yaw)
        B.spot(sid + "_counter", "counter", (r["customer"][0], r["customer"][1], 0.0), yaw + 180)
        B.spot(sid + "_clerk", "work", (r["staff"][0], r["staff"][1], 0.0), yaw)
        B.spot(sid + "_queue", "stand", (r["customer"][0] + 0.9 * math.sin(a), r["customer"][1] - 0.9 * math.cos(a), 0.0), yaw + 180)

    def on_top(self, fn, u, v, du=0.0, dv=0.0, z=1.05, **kw):
        x, y = self.P(u + du, v + dv)
        return fn(self.B, x, y, z, self.yw(u), **kw)

    def shelf_things(self, results, prefix, topics, z=1.3):
        for i, r in enumerate(results):
            self.B.thing("%s_%d" % (prefix, i + 1), "shelf", (r["pos"][0], r["pos"][1], z), r["yaw"],
                         topic=topics[i % len(topics)])

    def seat(self, tid, r, k=0):
        s = r["seats"][k]
        self.B.thing(tid, "seat", (s[0], s[1], 0.45), s[3], height=0.45)
        return s

    def sign(self, u, v, name, w=1.7, zc=2.72):
        x, y = self.P(u, v)
        kit.sign_plate(self.B, x, y, 0.0, self.yw(u), name=name, w=w, zc=zc, hang=True)

    # ------------------------------------------------------------------ common
    def lamps(self, colour, lumens=700, cols=2):
        B = self.B
        ys = [(self.y_front(self.xm) + 1.7), (self.y_back(self.xm) - 2.2)]
        xs = [self.xm + (c - (cols - 1) / 2) * self.W / (cols + 0.0) for c in range(cols)]
        # one real light per shop, in the middle (the game lights the nearest 24 lamps of a building and the whole
        # colonnade is one storey); every pendant still glows
        B.lamp((self.xm, (ys[0] + ys[1]) / 2, 2.9), lumens=lumens * 2.4, colour=colour, kind="public", radius=10.0,
               fixture=False)
        for y in ys:
            for x in xs:
                box(B.detail, "CT_Steel", x - 0.01, x + 0.01, y - 0.01, y + 0.01, 2.98, Z_CEIL)
                kit.pendant(B, x, y, Z_CEIL, drop=0.3, kind="opal")

    def awning(self):
        k = 0
        for i in range(self.a, self.b):
            p0, p1 = (XS[i], yf(XS[i]) + 2.5), (XS[i + 1], yf(XS[i + 1]) + 2.5)
            L = math.dist(p0, p1)
            tx, ty = (p1[0] - p0[0]) / L, (p1[1] - p0[1]) / L
            fr = Fr((p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2, math.degrees(math.atan2(ty, tx)))
            k = fu.awning(self.B, fr, -L / 2, L / 2, 1.15, 3.12, 2.62, k0=k, stripe=L / 5.0)

    def spot(self, sid, kind, u, v, a=0.0):
        x, y = self.P(u, v)
        self.B.spot(sid, kind, (x, y, 0.0), self.yw(u) + a)

    def spot_w(self, sid, kind, side, s, a=None):
        x = self.xl + 0.9 if side == "l" else self.xr - 0.9
        y = self.y_front(x) + s
        self.B.spot(sid, kind, (x, y, 0.0), 270 if side == "l" else 90)


# ------------------------------------------------------------------------------------------ local pieces
def _bread_rack(B, x, y, z=0.0, yaw=0.0, w=1.0, seed=0, solid=True):
    """[wall] Bakery rack: four open tiers with loaves, baguettes and buns."""
    p = kit._P(B, x, y, z, yaw, "bread_rack")
    p.kind = "wall"
    rnd = random.Random(seed * 7 + 3)
    d, h = 0.42, 1.9
    mat = kit.WOODL
    p.box(mat, -w / 2, w / 2, -0.015, 0.0, 0.0, h, f="y")
    for s in (-1, 1):
        p.box(mat, s * (w / 2 - 0.02) - 0.02, s * (w / 2 - 0.02) + 0.02, -d, 0.0, 0.0, h, f="xXyZ")
    loaf = ["CT_WoodLight", "CT_PlasterYellow", "CT_Leather", "CT_Brick", "CT_WoodLight"]
    for k in range(4):
        zb = 0.18 + k * 0.45
        p.box(mat, -w / 2 + 0.04, w / 2 - 0.04, -d, -0.015, zb - 0.02, zb, f="yZz")
        u = -w / 2 + 0.07
        while u < w / 2 - 0.22:
            m = rnd.choice(loaf)
            if rnd.random() < 0.4:                           # baguettes lying side by side
                for j in range(3):
                    p.rod(m, (u + 0.07 * j + 0.03, -0.05, zb + 0.05), (u + 0.07 * j + 0.03, -0.36, zb + 0.05), 0.055, 5, caps=True)
                u += 0.24
            else:                                            # a round or oval loaf
                p.bev(m, u, u + 0.26, -0.36, -0.06, zb, zb + 0.17, 0.06, 0.05)
                u += 0.3
    p.bev(mat, -w / 2 - 0.01, w / 2 + 0.01, -d - 0.01, 0.0, h, h + 0.025, 0.006)
    p.core(-w / 2 + 0.03, w / 2 - 0.03, -d + 0.02, -0.02, 0.6, 0.62)
    return p.done(solid=solid, top=h, front=0.7)


def _barber_chair(B, x, y, z=0.0, yaw=0.0, mat="CT_Leather", solid=False):
    """A hairdresser's chair on a chrome pedestal; the sitter faces yaw."""
    p = kit._P(B, x, y, z, yaw, "barber_chair")
    p.cyl(kit.ALU, 0, 0, 0.0, 0.03, 0.3, 10, top=True)
    p.cyl("CT_Steel", 0, 0, 0.03, 0.42, 0.05, 8, top=False)
    p.bev(mat, -0.26, 0.26, -0.26, 0.24, 0.42, 0.5, 0.025, 0.03)
    p.bev(mat, -0.25, 0.25, 0.18, 0.28, 0.5, 1.05, 0.03, 0.03)
    p.bev(mat, -0.12, 0.12, 0.17, 0.26, 1.05, 1.28, 0.03, 0.02)
    for s in (-1, 1):
        p.bev("CT_Steel", s * 0.31 - 0.03, s * 0.31 + 0.03, -0.18, 0.2, 0.62, 0.66, 0.01)
        p.rod("CT_Steel", (s * 0.31, 0.2, 0.5), (s * 0.31, 0.2, 0.64), 0.03, 4)
    p.rod("CT_Steel", (0, -0.2, 0.4), (0, -0.4, 0.2), 0.03, 4)
    p.bev("CT_Steel", -0.16, 0.16, -0.5, -0.36, 0.14, 0.18, 0.01)
    return p.done(solid=solid, seats=[(0.0, 0.0, 0.5, 0.0)], front=0.9)


def _teller_grille(B, x, y, z=0.0, yaw=0.0, w=2.0, solid=False):
    """Teller's frame on a counter top: slender posts, a head rail and grille panels with gaps (savings bank)."""
    p = kit._P(B, x, y, z, yaw, "teller_grille")
    top, hh = 1.05, 0.62
    for s in (-w / 2, w / 2):
        p.box(kit.ALU, s - 0.02, s + 0.02, -0.03, 0.03, top, top + hh + 0.05, f="xXyYZ")
    p.box(kit.ALU, -w / 2, w / 2, -0.03, 0.03, top + hh, top + hh + 0.05, f="xXyYZz")
    n = max(1, int(w / 0.9))
    for k in range(n):
        u0 = -w / 2 + 0.06 + k * (w - 0.12) / n
        u1 = u0 + (w - 0.12) / n - 0.05
        p.card_front("grille", u0, u1, top + 0.3, top + hh - 0.02, -0.032)
        p.card_back("grille", u0, u1, top + 0.3, top + hh - 0.02, 0.032)
        p.box(kit.ALU, u0, u1, -0.03, 0.03, top + 0.27, top + 0.3, f="xXZz")
    return p.done()


def _toy_bin(B, x, y, z=0.0, yaw=0.0, seed=0, solid=True):
    """A low kids' toy bin: wooden tub with balls, blocks and a teddy."""
    p = kit._P(B, x, y, z, yaw, "toy_bin")
    rnd = random.Random(seed + 11)
    p.bev(kit.WOOD, -0.45, 0.45, -0.3, 0.3, 0.0, 0.5, 0.015)
    p.box("CT_Paper", -0.4, 0.4, -0.25, 0.25, 0.49, 0.5, f="Z")
    for k in range(7):
        m = rnd.choice(["CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterGreen", "CT_PlasterPink"])
        u, v = rnd.uniform(-0.33, 0.33), rnd.uniform(-0.18, 0.18)
        if k % 2:
            p.cyl(m, u, v, 0.5, 0.62, 0.07, 6)
        else:
            p.box(m, u - 0.06, u + 0.06, v - 0.06, v + 0.06, 0.5, 0.6)
    p.cyl("CT_WoodLight", 0.0, 0.0, 0.5, 0.78, 0.1, 6, r1=0.07)
    p.cyl("CT_WoodLight", 0.0, 0.0, 0.78, 0.9, 0.07, 6, r1=0.03, bottom=False)
    p.core(-0.43, 0.43, -0.28, 0.28, 0.0, 0.48)
    return p.done(solid=solid, top=0.5)


def _hard_bins(B, x, y, z=0.0, yaw=0.0, w=1.6, seed=0, solid=True):
    """Steel bin rack with drawers of nails, screws and washers sold by weight."""
    p = kit._P(B, x, y, z, yaw, "hard_bins")
    rnd = random.Random(seed + 5)
    d, h = 0.5, 1.0
    p.box("CT_Steel", -w / 2, w / 2, -d / 2, d / 2, 0.0, h, f="xXyYZz")
    n = max(1, int(w / 0.3))
    for k in range(n):
        u0 = -w / 2 + 0.03 + k * (w - 0.06) / n
        u1 = u0 + (w - 0.06) / n - 0.03
        for j in range(4):
            zb = 0.06 + j * 0.23
            p.box(rnd.choice(["CT_SteelRed", "CT_PlasterBlue", "CT_Steel", "CT_PlasterYellow"]), u0, u1, -d / 2 - 0.02, -d / 2, zb, zb + 0.19, f="xXyYZ")
            p.box(kit.ALU, u0 + 0.05, u1 - 0.05, -d / 2 - 0.035, -d / 2 - 0.02, zb + 0.12, zb + 0.15, f="y")
    p.core(-w / 2 + 0.02, w / 2 - 0.02, -d / 2 + 0.02, d / 2 - 0.02, 0.05, 0.95)
    return p.done(solid=solid, top=h, front=0.7)


def _pyramid(B, S, u, v, rows=(4, 3, 2)):
    for lvl, n in enumerate(rows):
        for j in range(n):
            px, py = S.P(u + (j - (n - 1) / 2) * 0.27, v)
            kit.paint_jars(B, px, py, lvl * 0.2, S.yw(u), n=1, seed=lvl * 5 + j)


# ==================================================================================================== A bakery
def _bakery(S):
    B = S.B
    T = B.thing
    uc = 6.6
    # ---- back wall: bread racks behind the counter, boxed goods and tea at the left
    racks = S.back_run(_bread_rack, 4.6, 9.4, w=0.96)
    S.shelf_things(racks, "bakery_rack", ["cooking"], z=1.0)
    sh = S.back_run(kit.shop_shelf, 0.15, 3.0, w=0.95, goods="boxes", h=2.0, levels=4)
    S.shelf_things(sh, "bakery_shelf", ["cooking"])
    S.back(kit.wall_clock, 3.75, zc=2.6, r=0.22)
    # ---- counter with cakes under way
    r = S.counter(kit.shop_counter, uc, 6.0, "", 3.4, role="seller", front="CT_PlasterYellow", shop="market_bakery", service="shop")
    S.on_top(kit.plates, uc, 6.0, -1.2, 0.0, n=3)
    S.on_top(kit.cup, uc, 6.0, -0.6, 0.0)
    S.on_top(kit.teapot, uc, 6.0, -0.2, 0.0)
    S.on_top(kit.kitchen_scales, uc, 6.0, 0.2, 0.0)
    S.on_top(kit.crate, uc, 6.0, 1.0, 0.0, w=0.5, d=0.34, h=0.14, fill="CT_WoodLight", seed=2)
    S.sign(uc, 6.0, "sign_bread", w=1.7)
    # ---- glass cases along the right wall, boxed shelves above the left front, groceries by the corner
    c1 = S.right_f(kit.display_case, 2.4, 0.4, w=1.5, d=0.6, goods="boxes", seed=3)
    c2 = S.right_f(kit.display_case, 4.1, 0.4, w=1.5, d=0.6, goods="boxes", seed=4)
    T("bakery_pies_case", "board", (c1["pos"][0], c1["pos"][1], 1.0), 270, topic="prices")
    rs = S.side_run(kit.shop_shelf, "r", 4.95, 8.1, w=0.95, goods="groceries", h=1.9, levels=4)
    S.shelf_things(rs, "bakery_right", ["cooking"])
    ls = S.side_run(kit.shop_shelf, "l", 4.8, 8.1, w=0.95, goods="boxes", h=1.9, levels=4)
    S.shelf_things(ls, "bakery_left", ["cooking"])
    # ---- bread tables in the middle of the shop, loaves in crates
    for k, (u, v) in enumerate(((5.6, 3.4), (8.0, 3.7))):
        S.f(kit.table, u, v, 0, w=1.3, d=0.8, mat=kit.WOOD)
        for j in range(2):
            S.on_top(kit.crate, u, v, -0.33 + 0.66 * j, 0.0, z=0.75, w=0.55, d=0.4, h=0.18, fill=("CT_WoodLight", "CT_PlasterYellow")[(j + k) % 2], seed=j + k)
    # ---- cafe corner: two round tables by the window (left), one more at the right
    for k, (u, v) in enumerate(((1.2, 1.4), (1.2, 3.1), (6.4, 1.4))):
        S.f(kit.table_round, u, v, 0, r=0.38, mat=kit.WOODL)
        S.on_top(kit.cup, u, v, -0.1, 0.0, z=0.75)
        S.on_top(kit.plates, u, v, 0.12, 0.03, z=0.75, n=2)
        for j, s in enumerate((-1, 1)):
            ch = S.f(kit.chair, u + s * 0.58, v, -s * 90, mat=("CT_PlasterRed", "CT_PlasterYellow")[(k + j) % 2])
            S.seat("bakery_seat_%d%s" % (k + 1, "ab"[j]), ch)
        S.spot("bakery_table_%d" % (k + 1), "table", u + 0.58, v, 270)
    S.f(kit.rug, 1.2, 2.3, 0, w=1.5, d=2.8, name="rug_2")
    # ---- walls, plants
    S.left(kit.poster, 1.8, name="poster_cons_1", w=0.6, zc=1.8)
    S.left(kit.picture, 3.5, name="painting_still_life", w=0.6, zc=1.9)
    kit.plant(B, *S.P(0.55, 0.55), 0.0, S.yw(0.5), kind="ficus", pot="CT_PlasterYellow", h=1.5)
    kit.plant(B, *S.P(8.9, 0.55), 0.0, S.yw(8.9), kind="fern", pot="CT_PlasterRed", h=1.0)
    kit.fire_extinguisher(B, S.xl + 0.005, S.y_front(S.xl) + 0.8, 0.0, 90)
    # ---- spots (old ids)
    S.spot_w("shop_a_browse1", "stand", "l", 6.4)
    S.spot("shop_a_browse3", "stand", 4.0, 3.4, 180)
    S.lamps(WARM, 760, cols=3)


# ==================================================================================================== B records
def _records(S):
    B = S.B
    T = B.thing
    uc = 5.55
    sh = S.back_run(kit.shop_shelf, 0.15, 4.2, w=1.0, goods="records", h=1.9, levels=5)
    S.shelf_things(sh, "records_wall", ["music"])
    # radios on wall shelves behind the counter
    for k, u in enumerate((4.85, 6.25)):
        for j, zt in enumerate((1.2, 1.75)):
            x = S.xl + u
            th = _th(x)
            y = yb(x) - 0.3 / math.cos(th) + 0.02
            yaw = math.degrees(th)
            kit.wall_shelf(B, x, y, 0.0, yaw, w=1.1, zt=zt, props=False)
            kit.radio(B, x - 0.25, y - 0.1, zt, yaw)
            kit.radio(B, x + 0.2, y - 0.1, zt, yaw + 6)
    x = S.xl + 5.55
    B.thing("records_radios", "board", (x, S.y_back(x) - 0.2, 1.6), S.yw(5.55), topic="catalogue")
    S.back(kit.wall_clock, 4.4, zc=2.7, r=0.2)
    r = S.counter(kit.shop_counter, uc, 6.0, "", 2.2, role="seller", front="CT_PlasterBlue", top="CT_Wood", service="shop")
    S.on_top(kit.telephone, uc, 6.0, -0.8, 0.0)
    rp = S.on_top(kit.record_player, uc, 6.0, 0.55, 0.0)
    B.thing("records_counter_player", "machine", (rp["pos"][0], rp["pos"][1], 1.1), S.yw(uc), what="record_player")
    # record bins in the middle (two rows), a poster-covered pillar of sleeves
    for k, (u, v) in enumerate(((5.5, 3.3), (5.5, 1.5))):
        c = S.f(kit.display_case, u, v, 0, w=1.5, d=0.6, goods="records", seed=k + 2)
        B.thing("records_bin_%d" % (k + 1), "shelf", (c["pos"][0], c["pos"][1], 0.9), S.yw(u), topic="music")
    # listening corner: rug, low table with a player, two armchairs
    S.f(kit.rug, 1.4, 1.8, 0, w=2.3, d=1.8, name="rug_4")
    S.f(kit.coffee_table, 1.45, 1.4, 0, w=0.9, d=0.5)
    lp = S.on_top(kit.record_player, 1.45, 1.4, 0.0, 0.0, z=0.42)
    B.thing("records_listening", "machine", (lp["pos"][0], lp["pos"][1], 0.5), S.yw(1.45), what="record_player")
    a1 = S.f(kit.armchair, 0.6, 2.6, 90, mat="CT_PlasterBlue")
    S.seat("records_armchair_1", a1)
    a2 = S.f(kit.armchair, 2.3, 2.9, -30, mat="CT_PlasterRed")
    S.seat("records_armchair_2", a2)
    jb = S.right_f(kit.jukebox, 2.3, 0.4)
    B.thing("records_jukebox", "machine", (jb["pos"][0], jb["pos"][1], 1.0), 270, what="jukebox")
    ls = S.side_run(kit.shop_shelf, "l", 4.4, 8.1, w=0.95, goods="records", h=1.9, levels=5)
    S.shelf_things(ls, "records_left", ["music"])
    rs = S.side_run(kit.shop_shelf, "r", 3.1, 8.1, w=0.95, goods="records", h=1.9, levels=5)
    S.shelf_things(rs, "records_right", ["music"])
    # sleeves as pictures
    S.left(kit.poster, 1.0, name="sleeve_1", w=0.55, zc=1.7)
    S.left(kit.poster, 2.0, name="sleeve_6", w=0.55, zc=1.7)
    S.left(kit.poster, 3.0, name="sleeve_0", w=0.55, zc=1.7)
    S.spot_w("shop_b_browse1", "stand", "l", 6.3)
    S.spot("shop_b_browse3", "stand", 3.1, 5.0, 180)
    S.lamps(WARM, 720, cols=2)


# ==================================================================================================== C books
def _books(S):
    B = S.B
    T = B.thing
    uc = 5.55
    wall = S.back_run(kit.bookshelf, 0.15, 7.05, w=0.95, h=2.3, levels=6)
    S.shelf_things(wall, "books_wall", PAPER_TOPICS)
    ls = S.side_run(kit.bookshelf, "l", 4.4, 8.1, w=0.95, h=2.2, levels=6)
    S.shelf_things(ls, "books_left", PAPER_TOPICS[3:] + PAPER_TOPICS[:3])
    rs = S.side_run(kit.bookshelf, "r", 2.6, 8.1, w=0.95, h=2.2, levels=6)
    S.shelf_things(rs, "books_right", PAPER_TOPICS[5:] + PAPER_TOPICS[:5])
    r = S.counter(kit.shop_counter, uc, 6.0, "", 2.2, role="seller", front="CT_PlasterGreen", top="CT_Wood", shop="store_books", service="shop")
    S.on_top(kit.books_stack, uc, 6.0, -0.7, 0.0, n=4, seed=3)
    S.on_top(kit.newspaper, uc, 6.0, 0.6, 0.0)
    S.on_top(kit.table_lamp, uc, 6.0, 0.0, 0.15)
    S.sign(uc, 6.0, "sign_books", w=1.7)
    # new arrivals: cases and a magazine rack in the middle
    ic = S.f(kit.display_case, 5.3, 3.5, 0, w=1.4, d=0.6, goods="boxes", seed=1)
    T("books_new_arrivals", "shelf", (ic["pos"][0], ic["pos"][1], 0.9), S.yw(5.3), topic="local")
    mr = S.f(kit.magazine_rack, 5.0, 1.6, 0, w=0.9)
    T("books_magazines", "shelf", (mr["pos"][0], mr["pos"][1], 1.0), S.yw(5.0), topic="science")
    # reading corner by the left window
    S.f(kit.rug, 1.4, 2.0, 0, w=2.4, d=1.8, name="rug_1")
    S.f(kit.coffee_table, 1.5, 1.7, 0, w=0.8, d=0.5)
    S.on_top(kit.books_stack, 1.5, 1.7, -0.15, 0.0, z=0.42, n=3, seed=5)
    S.on_top(kit.cup, 1.5, 1.7, 0.2, 0.0, z=0.42)
    c1 = S.f(kit.armchair, 0.6, 2.7, 90, mat="CT_PlasterGreen")
    T("books_armchair_1", "seat", (c1["seats"][0][0], c1["seats"][0][1], 0.45), c1["seats"][0][3], height=0.45)
    B.spot("shop_c_seat1", "seat", (c1["seats"][0][0], c1["seats"][0][1], 0.0), c1["seats"][0][3])
    c2 = S.f(kit.armchair, 2.3, 3.0, -30, mat="CT_PlasterBlue")
    T("books_armchair_2", "seat", (c2["seats"][0][0], c2["seats"][0][1], 0.45), c2["seats"][0][3], height=0.45)
    B.spot("shop_c_seat2", "seat", (c2["seats"][0][0], c2["seats"][0][1], 0.0), c2["seats"][0][3])
    kit.floor_lamp(B, *S.P(0.5, 3.8), 0.0, S.yw(0.5))
    kit.plant(B, *S.P(6.8, 0.5), 0.0, S.yw(6.8), kind="palm", pot="CT_PlasterYellow", h=1.8)
    kit.globe(B, *S.P(3.0, 4.6), 0.0, S.yw(3.0), floor=True)
    S.left(kit.poster, 1.0, name="poster_birds_0", w=0.5, zc=1.8)
    S.left(kit.poster, 2.0, name="travel_1", w=0.5, zc=1.8)
    S.right(kit.poster, 1.4, name="poster_sun_1", w=0.5, zc=1.8)
    T("books_poster_birds", "board", (S.xl + 0.1, S.y_front(S.xl) + 1.0, 1.8), 90, topic="exhibit")
    S.spot_w("shop_c_browse1", "stand", "l", 6.2)
    S.spot_w("shop_c_browse2", "stand", "r", 5.0)
    S.spot("shop_c_browse3", "stand", 3.0, 5.4, 180)
    S.spot("shop_c_browse4", "stand", 1.0, 5.4, 180)
    S.lamps(WARM, 720, cols=2)


# ==================================================================================================== D hardware
def _hardware(S):
    B = S.B
    T = B.thing
    uc = 7.0
    wa = S.back_run(kit.shop_shelf, 0.15, 5.3, w=0.95, goods="hardware", h=2.0, levels=5)
    S.shelf_things(wa, "hardware_wall", ["engineering"])
    wb = S.back_run(kit.shop_shelf, 8.7, 9.55, w=0.85, goods="boxes", h=2.0, levels=5)
    S.shelf_things(wb, "hardware_corner", ["engineering"])
    # pegboard shelves with tools behind the counter
    for k, u in enumerate((5.9, 7.0, 8.1)):
        x = S.xl + u
        th = _th(x)
        y = yb(x) - 0.3 / math.cos(th) + 0.02
        yaw = math.degrees(th)
        kit.wall_shelf(B, x, y, 0.0, yaw, w=1.0, zt=1.5, props=False)
        kit.tools(B, x, y - 0.1, 1.5, yaw + 90 * (k % 2))
        kit.paint_jars(B, x + 0.3, y - 0.1, 1.5, yaw, n=2, seed=k)
    x = S.xl + 7.0
    T("hardware_pegboard", "board", (x, S.y_back(x) - 0.2, 1.8), S.yw(7.0), topic="catalogue")
    S.back(kit.wall_clock, 5.5, zc=2.7, r=0.2)
    S.back(kit.poster, 8.4, name="poster_safety", w=0.5, zc=2.3)
    r = S.counter(kit.shop_counter, uc, 6.0, "", 3.0, role="seller", front="CT_SteelRed", top="CT_Wood", shop="store_garden", service="shop")
    S.on_top(kit.paint_jars, uc, 6.0, -1.0, 0.0, n=4, seed=3)
    S.on_top(kit.kitchen_scales, uc, 6.0, 0.4, 0.0)
    S.on_top(kit.tools, uc, 6.0, -0.2, 0.0)
    # bins and an island of nails; shelves along the walls
    hb = S.f(_hard_bins, 1.5, 4.2, 0, w=1.6, seed=3)
    B.thing("hardware_bins", "shelf", (hb["pos"][0], hb["pos"][1], 1.0), S.yw(1.5), topic="engineering")
    S.f(_hard_bins, 5.3, 3.3, 0, w=1.8, seed=4)
    ls = S.side_run(kit.shop_shelf, "l", 3.6, 8.1, w=0.95, goods="hardware", h=2.0, levels=5)
    S.shelf_things(ls, "hardware_left", ["engineering"])
    rs = S.side_run(kit.shop_shelf, "r", 2.2, 8.1, w=0.95, goods="boxes", h=2.0, levels=5)
    S.shelf_things(rs, "hardware_right", ["garden", "engineering"])
    # garden corner at the right front: a table of feeders and bird food in boxes and sacks
    S.f(kit.table, 7.2, 1.3, 0, w=1.8, d=0.8, mat=kit.WOOD)
    for j in range(3):
        S.on_top(kit.cardboard_box, 7.2, 1.3, -0.6 + 0.6 * j, 0.0, z=0.75, w=0.36, d=0.28, h=0.22, seed=j + 4)
    x, y = S.P(7.2, 1.3)
    T("hardware_garden_table", "board", (x, y, 0.8), S.yw(7.2), topic="garden")
    kit.sack(B, *S.P(8.9, 1.0), 0.0, 20)
    kit.sack(B, *S.P(8.9, 1.6), 0.0, 80)
    kit.sack(B, *S.P(5.6, 1.0), 0.0, 140)
    kit.crate(B, *S.P(5.8, 2.0), 0.0, S.yw(5.8), fill="CT_SteelRed", seed=2)
    # front left: paint pyramid, cement sacks, brooms leaning on the front wall
    _pyramid(B, S, 1.4, 1.0)
    for j in range(3):
        kit.sack(B, *S.P(0.8 + 0.5 * j, 2.4), 0.0, 10 + 25 * j)
    for j, m in enumerate(("CT_WoodLight", "CT_Steel", "CT_SteelRed")):
        x, y = S.P(2.4 + 0.25 * j, 0.14)
        box(B.detail, m, x - 0.02, x + 0.02, y - 0.02, y + 0.02, 0.0, 1.7 - 0.12 * j)
    kit.plant(B, *S.P(9.0, 3.2), 0.0, 0.0, kind="cactus", pot="CT_PlasterRed", h=0.9)
    S.spot_w("shop_d_browse1", "stand", "l", 6.0)
    S.spot_w("shop_d_browse2", "stand", "r", 5.0)
    S.spot("shop_d_browse3", "stand", 2.6, 4.4, 90)
    S.spot("shop_d_browse4", "stand", 5.3, 4.4, 180)
    S.lamps("#FFE2B8", 800, cols=3)


# ==================================================================================================== E savings bank + hairdresser
def _salon(S):
    B = S.B
    T = B.thing
    # ---------------- savings bank, left of the aisle (u 0 .. 2.8)
    bu = 1.65
    r = S.f(kit.shop_counter, bu, 5.2, 0, w=2.0, front="CT_PlasterGreen", top="CT_Wood", till=False)
    yaw = S.yw(bu)
    S.counter_spots("shop_e_bank", r, yaw)
    B.thing("shop_e_bank_counter", "counter", (r["pos"][0], r["pos"][1], 1.05), yaw, role="cashier", service="cashier")
    x, y = S.P(bu, 5.2)
    _teller_grille(B, x, y, 0.0, yaw, w=2.0)
    S.on_top(kit.cash_register, bu, 5.2, 0.5, 0.0)
    S.on_top(kit.telephone, bu, 5.2, -0.6, 0.15)
    S.sign(bu, 5.2, "sign_till", w=1.4, zc=2.7)
    # behind the counter: a safe, filing cabinets, a desk with a typewriter
    S.back(kit.safe, 0.6, mat="CT_PlasterGreen")
    S.back(kit.filing_cabinet, 1.35)
    S.back(kit.filing_cabinet, 1.95)
    dk = S.f(kit.office_desk, 1.35, 7.4, 180, w=1.3, d=0.65)
    # waiting bench and posters for the customers
    wr = S.left_f(kit.waiting_row, 1.7, 0.4, n=2)
    B.thing("shop_e_bank_seat", "seat", (wr["seats"][0][0], wr["seats"][0][1], 0.45), 90, height=0.45)
    B.thing("shop_e_bank_board", "board", (S.xl + 0.05, S.y_front(S.xl) + 3.3, 1.7), 90, topic="prices")
    S.left(kit.notice_board, 3.3, w=0.9, zc=1.7, seed=3)
    S.left(kit.poster, 4.4, name="poster_cons_2", w=0.5, zc=1.8)
    kit.plant(B, *S.P(0.55, 0.55), 0.0, S.yw(0.5), kind="fern", pot="CT_PlasterGreen", h=1.1)
    # ---------------- hairdresser, right of the aisle (u 4.4 .. 9.7)
    # mirrors on the back wall over a long shelf with bottles and combs, two chairs facing them
    for k, u in enumerate((5.3, 7.1, 8.9)):
        x = S.xl + u
        th = _th(x)
        y = yb(x) - 0.3 / math.cos(th) + 0.02
        yaw = math.degrees(th)
        kit.mirror(B, x, y, 0.0, yaw, zc=1.6, w=1.1, h=1.2)
        kit.wall_shelf(B, x, y, 0.0, yaw, w=1.3, zt=0.95, props=False)
        kit.bottles(B, x - 0.3, y - 0.1, 0.95, yaw, n=3, seed=k)
    S.back(kit.sign_plate, 7.1, name="sign_barber", w=1.5, zc=2.75)
    chairs = []
    for k, u in enumerate((6.2, 8.0)):
        ch = S.f(_barber_chair, u, 7.0, 180)
        chairs.append(ch)
        s = ch["seats"][0]
        B.thing("salon_chair_%d" % (k + 1), "machine", (s[0], s[1], 0.6), s[3], what="haircut")
        B.spot("shop_e_chair_%d" % (k + 1), "seat", (s[0], s[1], 0.0), s[3])
    S.f(kit.rug, 7.1, 7.0, 0, w=3.4, d=1.6, name="rug_3")
    # reception desk at the front, waiting sofa on the right wall, magazines
    rc = S.f(kit.shop_counter, 7.0, 4.3, 0, w=1.8, front="CT_PlasterPink", top="CT_TileWhite")
    yw_ = S.yw(7.0)
    S.counter_spots("shop_e", rc, yw_)
    B.thing("shop_e_counter", "counter", (rc["pos"][0], rc["pos"][1], 1.05), yw_, role="seller", service="reception")
    S.on_top(kit.telephone, 7.0, 4.3, -0.5, 0.1)
    sf = S.right_f(kit.sofa, 2.4, 0.5, seats=3, mat="CT_PlasterBlue")
    for j in range(3):
        sx = sf["seats"][j] if j < len(sf["seats"]) else sf["seats"][-1]
        B.spot("shop_e_seat%d" % (j + 1), "seat", (sx[0], sx[1], 0.0), 270)
    B.thing("shop_e_waiting_sofa", "seat", (sf["seats"][0][0], sf["seats"][0][1], 0.45), 270, height=0.45)
    mr = S.right_f(kit.magazine_rack, 4.0, 0.2)
    B.thing("salon_magazines", "shelf", (mr["pos"][0], mr["pos"][1], 1.0), 270, topic="art")
    S.right(kit.picture, 1.1, name="painting_portrait", w=0.5, zc=1.9)
    S.right(kit.poster, 6.3, name="poster_health", w=0.5, zc=1.8)
    kit.plant(B, *S.P(9.0, 0.6), 0.0, S.yw(9.0), kind="ficus", pot="CT_PlasterRed", h=1.5)
    S.spot("shop_e_browse3", "stand", 5.2, 3.5, 90)
    S.spot("shop_e_browse4", "stand", 3.6, 5.5, 180)
    S.lamps(COOL, 800, cols=3)


# ==================================================================================================== F fabrics + toys
def _toyfab(S):
    B = S.B
    T = B.thing
    # ---------------- fabrics (left, u 0 .. 2.8): rolls on shelves, a cutting counter, a sewing table, a rail of dresses
    fs = S.back_run(kit.shop_shelf, 0.15, 2.7, w=0.85, goods="fabric", h=2.0, levels=6)
    S.shelf_things(fs, "fabric_wall", ["art"])
    ls = S.side_run(kit.shop_shelf, "l", 3.6, 8.2, w=0.95, goods="fabric", h=2.0, levels=6)
    S.shelf_things(ls, "fabric_left", ["art"])
    r = S.f(kit.shop_counter, 1.6, 5.0, 0, w=1.9, front="CT_PlasterPink", top="CT_Wood", till=False)
    yaw = S.yw(1.6)
    S.counter_spots("shop_f_fab", r, yaw)
    B.thing("shop_f_fab_counter", "counter", (r["pos"][0], r["pos"][1], 1.05), yaw, role="seller", shop="store_haberdashery", service="shop")
    S.on_top(kit.cardboard_box, 1.6, 5.0, -0.4, 0.0, w=0.4, d=0.3, h=0.2, seed=2)
    S.sign(1.6, 5.0, "sign_fabric", w=1.4, zc=2.72)
    sw = S.left_f(kit.sewing_table, 2.6, 0.45)
    B.thing("fabric_sewing", "craft", (sw["pos"][0], sw["pos"][1], 0.8), 90, what="sewing_machine")
    sx_, sy_, _sz, syaw = sw["seat"][0], sw["seat"][1], 0.0, (sw["yaw"] + 180.0) % 360
    kit.stool(B, sx_, sy_, 0.0, syaw)
    B.thing("fabric_sewing_stool", "seat", (sx_, sy_, 0.45), syaw, height=0.45)
    cr = S.f(kit.clothes_rail, 1.6, 1.5, 0, w=1.4)
    S.left(kit.poster, 1.0, name="fabric_1", w=0.5, h=0.5, zc=1.8)
    S.left(kit.poster, 1.6, name="fabric_4", w=0.5, h=0.5, zc=1.8)
    kit.plant(B, *S.P(0.55, 0.55), 0.0, S.yw(0.5), kind="cactus", pot="CT_PlasterPink", h=0.9)
    # ---------------- toys (right, u 4.4 .. 9.55)
    ts = S.back_run(kit.shop_shelf, 4.5, 9.4, w=0.98, goods="toys", h=2.0, levels=5)
    S.shelf_things(ts, "toys_wall", ["kids"])
    rs = S.side_run(kit.shop_shelf, "r", 3.6, 8.2, w=0.95, goods="toys", h=1.8, levels=5)
    S.shelf_things(rs, "toys_right", ["kids"])
    rc = S.f(kit.shop_counter, 7.0, 5.6, 0, w=2.2, front="CT_PlasterRed", top="CT_TileWhite")
    yw_ = S.yw(7.0)
    S.counter_spots("shop_f", rc, yw_)
    B.thing("shop_f_counter", "counter", (rc["pos"][0], rc["pos"][1], 1.05), yw_, role="seller", shop="store_souvenirs", service="shop")
    S.sign(7.0, 5.6, "sign_toys", w=1.5, zc=2.72)
    for j in range(2):
        tb = S.f(_toy_bin, 5.2 + 0.0, 1.7 + 1.5 * j, 0, seed=j)
    S.f(_toy_bin, 8.6, 3.0, 0, seed=5)
    dc = S.f(kit.display_case, 6.9, 3.4, 0, w=1.3, d=0.6, goods="toys", seed=7)
    B.thing("toys_case", "board", (dc["pos"][0], dc["pos"][1], 1.0), S.yw(6.9), topic="catalogue")
    tf = S.f(kit.table_football, 8.3, 1.4, 90)
    B.thing("toys_table_football", "play", (tf["pos"][0], tf["pos"][1], 0.9), S.yw(8.3) + 90, what="table_football")
    S.right(kit.poster, 1.2, name="kids_0", w=0.55, zc=1.7)
    S.right(kit.poster, 2.4, name="kids_2", w=0.55, zc=1.7)
    S.spot_w("shop_f_browse1", "stand", "l", 6.4)
    S.spot_w("shop_f_browse2", "stand", "r", 5.2)
    S.lamps("#FFC888", 780, cols=3)


# ==================================================================================================== lobbies
def _lobby(B, rnd, a, b, name):
    """A core lobby: mailboxes on the west wall, a porter's desk with a phone, a bench, a notice board, plants, lamp."""
    xl, xr = XS[a] + 0.15, XS[b] - 0.15
    xm = (XS[a] + XS[b]) / 2
    yfm = yf(xm) + 2.8
    yy = yfm + 2.4
    fu.mailboxes(B, Fr(xl, yy, 90), 1.3, 0.9, cols=6, rows=4)
    B.thing("%s_mailboxes" % name, "board", (xl + 0.1, yy + 1.3, 1.5), 90, topic="rules")
    xd = (XS[a] + XS[a + 1]) / 2
    fu.doormat(B, Fr(xd, yf(xd) + 2.8, 0.0), 0.0, -0.45, z=0.0)
    n = name[-1]
    th = _th(xm)
    yw = math.degrees(th)
    # porter's desk against the east wall with a telephone and a bell, an armchair for the porter
    r = kit.reception_desk(B, xr - 0.45, yy + 1.2, 0.0, 270, w=1.6)
    kit.telephone(B, xr - 0.45, yy + 1.7, 1.1, 270)
    B.thing("%s_porter" % name, "counter", (xr - 0.45, yy + 1.2, 1.1), 270, role="janitor", service="reception")
    B.spot("%s_porter" % name, "counter", (xr - 0.9, yy + 1.2, 0.0), 270)
    # bench and a plant by the west wall, notice board and clock on the east wall
    bn = kit.bench(B, xl + 0.35, yy + 3.6, 0.0, 90, w=1.5, kind="park")
    B.thing("%s_bench" % name, "seat", (bn["pos"][0], bn["pos"][1], 0.45), 90, height=0.45)
    B.spot("%s_bench" % name, "bench", (xl + 0.9, yy + 3.6, 0.0), 90)
    kit.notice_board(B, xr + 0.005, yy + 3.4, 0.0, 270, w=1.0, zc=1.7, seed=int(n))
    B.thing("%s_notices" % name, "board", (xr - 0.1, yy + 3.4, 1.7), 270, topic="rules")
    kit.wall_clock(B, xr + 0.005, yy + 2.2, 0.0, 270, zc=2.6, r=0.2)
    kit.plant(B, xl + 0.5, yy + 5.2, 0.0, 0.0, kind="ficus", pot="CT_PlasterBlue", h=1.6)
    kit.plant(B, xr - 0.5, yfm + 0.7, 0.0, 0.0, kind="fern", pot="CT_PlasterRed", h=1.1)
    kit.rug(B, xm, yy + 2.0, 0.0, yw, w=1.8, d=3.0, name="rug_%d" % (int(n) % 6))
    kit.floor_lamp(B, xr - 0.5, yy + 5.2, 0.0, 0.0)
    B.lamp((xm, yy + 0.8, 2.9), lumens=520, colour="#FFE0B0", kind="lobby", radius=6.0, fixture=False)
    kit.pendant(B, xm, yy + 0.8, Z_CEIL, drop=0.3, kind="cone")
    # QA: the back half of the lobby was dark and its back wall bare: a second lamp, a picture and a poster
    B.lamp((xm, yy + 4.4, 2.9), lumens=420, colour="#FFE0B0", kind="lobby", radius=6.0, fixture=False)
    kit.pendant(B, xm, yy + 4.4, Z_CEIL, drop=0.3, kind="cone")
    yback = yb(xm) - 0.3 / math.cos(th) + 0.025
    kit.picture(B, xm - 0.9, yback, 0.0, yw, name=("painting_sea_calm", "painting_dunes", "painting_lighthouse")[(int(n) - 1) % 3],
                w=1.0, zc=1.8)
    kit.poster(B, xm + (0.0 if name == "core2" else 0.9), yback, 0.0, yw,
               name=("poster_sun_1", "poster_health", "poster_cons_2")[(int(n) - 1) % 3], w=0.6, zc=1.8)


def _passage(B, a, b):
    """QA: the blue-tiled through passage had no light at all: two lamps with pendants and a poster on each wall."""
    xm = (XS[a] + XS[b]) / 2
    for k, off in enumerate((2.8, 7.6)):
        y = yf(xm) + off
        B.lamp((xm, y, 2.9), lumens=520, colour="#FFE0B0", kind="lobby", radius=6.0, fixture=False)
        kit.pendant(B, xm, y, Z_CEIL, drop=0.3, kind="cone")
    y = yf(xm) + 5.0
    kit.poster(B, XS[a] + 0.16, y, 0.0, 90.0, name="poster_birds_1", w=0.7, zc=1.7)
    kit.poster(B, XS[b] - 0.16, y + 0.6, 0.0, 270.0, name="poster_sun_2", w=0.7, zc=1.7)



# ==================================================================================================== depth pass (round 2)
# Every shop gets a wall dado of its own colour, a window display and a few things to look at or do; the arcade gets a
# notice board with a running story, a weighing machine, benches, sandwich boards and four lamps.
def _strip_back(S, mat, u0, u1, z0, z1, t=0.025):
    x = S.xl + (u0 + u1) / 2
    th = _th(x)
    y = yb(x) - 0.3 / math.cos(th) + 0.004
    p = kit._P(S.B, x, y, 0.0, math.degrees(th), "wall_strip")
    p.kind = "wall"
    w = (u1 - u0) / math.cos(th)
    p.box(mat, -w / 2, w / 2, -t, 0.0, z0, z1, f="xXyZz")
    r = p.done()
    S.B.kit_items.pop()
    return r


def _strip_side(S, side, s0, s1, z0, z1, mat, t=0.025):
    xa = S.xl if side == "l" else S.xr - t
    ya, yb_ = S.y_front(S.xl if side == "l" else S.xr) + s0, S.y_front(S.xl if side == "l" else S.xr) + s1
    box(S.B.detail, mat, xa + (0.004 if side == "l" else -0.004), xa + t + (0.004 if side == "l" else -0.004), ya, yb_, z0, z1)


def _dado(S, mat, trim, zh=1.1, front=0.2, side_to=None, back=True):
    """A painted panel band along the side walls (from the front wall to side_to) and the back wall."""
    sb = side_to if side_to is not None else S.vback(S.W / 2) - 0.1
    for side in ("l", "r"):
        _strip_side(S, side, front, sb, 0.0, zh, mat)
        _strip_side(S, side, front, sb, zh, zh + 0.06, trim)
    if back:
        _strip_back(S, mat, 0.12, S.W - 0.12, 0.0, zh)
        _strip_back(S, trim, 0.12, S.W - 0.12, zh, zh + 0.06, t=0.03)


def _headphones(B, x, y, z=0.0, yaw=0.0, seed=0):
    """[prop] Two pairs of headphones lying on a table: a band and two cups each."""
    p = kit._P(B, x, y, z, yaw, "headphones")
    p.kind = "prop"
    for k, (cu, cv) in enumerate(((-0.2, 0.0), (0.2, 0.02))):
        m = ("CT_Leather", "CT_PlasterRed")[k]
        for sgn in (-1, 1):
            p.cyl("CT_Steel", cu + sgn * 0.075, cv, 0.0, 0.035, 0.04, 8)
        pts = [(cu - 0.075, cv, 0.03), (cu - 0.065, cv, 0.1), (cu - 0.03, cv, 0.13), (cu + 0.03, cv, 0.13),
               (cu + 0.065, cv, 0.1), (cu + 0.075, cv, 0.03)]
        p.tube(m, pts, 0.015, 3)
    return p.done()


def _headphone_post(B, x, y, z=0.0, yaw=0.0):
    """Listening post: a long table with a record player, two pairs of headphones and a shelf of sleeves under it;
    two stools stand behind (+y). Returns the stools' seats."""
    r = kit.table(B, x, y, z, yaw, w=1.3, d=0.55, mat=kit.WOOD)
    zt = r["top"]
    p = kit._P(B, x, y, z, yaw, "post_shelf")
    p.box("CT_Leather", -0.6, 0.6, -0.22, 0.22, 0.2, 0.23, f="xXyYZz")
    for k in range(6):
        p.card_front("sleeve_%d" % ((k * 3 + 1) % 8), -0.56 + k * 0.19, -0.4 + k * 0.19, 0.23, 0.4, -0.2)
    p.done()
    B.kit_items.pop()
    rp = kit.record_player(B, x - 0.28, y, zt, yaw)
    _headphones(B, x + 0.3, y, zt, yaw)
    kit.lamp_green(B, x - 0.52, y, zt, yaw + 90)
    seats = []
    for sx in (-0.32, 0.32):
        px, py = r["foot"][0][0] * 0 + x, y
        a = math.radians(yaw)
        # stools sit behind the table: local (sx, +0.6)
        wx = x + sx * math.cos(a) - 0.62 * math.sin(a)
        wy = y + sx * math.sin(a) + 0.62 * math.cos(a)
        st = kit.stool(B, wx, wy, z, yaw)
        seats.append(st)
    return r, rp, seats


def _sleeve_stand(B, x, y, z=0.0, yaw=0.0, w=1.2, names=("sleeve_3", "sleeve_5", "sleeve_1", "sleeve_7")):
    """Window display: a wooden plinth with slanted record sleeves facing the street (-y) on a low stand."""
    p = kit._P(B, x, y, z, yaw, "sleeve_stand")
    p.bev(kit.WOOD, -w / 2, w / 2, -0.25, 0.25, 0.0, 0.45, 0.01)
    p.box("CT_Leather", -w / 2 + 0.03, w / 2 - 0.03, -0.21, 0.21, 0.45, 0.46, f="Z")
    n = len(names)
    cw = (w - 0.1) / n
    for k, nm in enumerate(names):
        x0 = -w / 2 + 0.05 + k * cw + 0.02
        p.card_tilt(nm, x0, x0 + cw - 0.04, -0.12, 0.47, 0.0, 0.47 + 0.34)
        p.rod(kit.ALU, (x0 + 0.02, -0.1, 0.47), (x0 + 0.02, 0.0, 0.5), 0.015, 3)
    p.core(-w / 2 + 0.02, w / 2 - 0.02, -0.23, 0.23, 0.0, 0.4)
    return p.done(solid=True, top=0.46)


def _cat_basket(B, x, y, z=0.0, yaw=0.0):
    """A wicker basket with a folded blanket, a sleeping ginger cat and, under the blanket's corner, a glint of shiny
    small things (a green toy locomotive's chimney)."""
    p = kit._P(B, x, y, z, yaw, "cat_basket")
    p.kind = "prop"
    p.bev("CT_WoodLight", -0.33, 0.33, -0.25, 0.25, 0.0, 0.2, 0.012, 0.05)
    for k in range(3):
        zz = 0.05 + k * 0.05
        p.box("CT_Wood", -0.335, 0.335, -0.255, -0.248, zz, zz + 0.012, f="xXyYZ")
        p.box("CT_Wood", -0.335, 0.335, 0.248, 0.255, zz, zz + 0.012, f="xXyYZ")
    p.bev("CT_PlasterBlue", -0.28, 0.28, -0.2, 0.2, 0.17, 0.22, 0.015, 0.03)
    p.cushion("CT_PlasterRed", -0.1, 0.22, -0.12, 0.14, 0.2, 0.3, puff=0.02)             # blanket fold
    p.lathe("CT_PlasterYellow", -0.08, 0.0, [(0.0, 0.2), (0.12, 0.23), (0.17, 0.3), (0.12, 0.38), (0.0, 0.4)], 8)  # curled cat
    p.lathe("CT_PlasterYellow", -0.2, -0.1, [(0.0, 0.22), (0.07, 0.27), (0.075, 0.33), (0.0, 0.38)], 7)             # head
    for sgn in (-1, 1):
        p.rod("CT_PlasterYellow", (-0.2 + sgn * 0.04, -0.1, 0.36), (-0.2 + sgn * 0.045, -0.1, 0.42), 0.035, 3)
    p.cyl("CT_Leaf", 0.2, 0.1, 0.22, 0.31, 0.022, 6)                                      # loco chimney
    p.box("CT_Leaf", 0.12, 0.28, 0.06, 0.15, 0.22, 0.27)
    p.cyl("CT_SteelRed", 0.2, 0.1, 0.31, 0.33, 0.032, 6)
    return p.done(top=0.3, footprint=(-0.33, 0.33, -0.25, 0.25), front=0.7)


def _book_cart(B, x, y, z=0.0, yaw=0.0, seed=0, solid=True):
    """A two-shelf wooden book trolley on castors with a push bar; second-hand books in rows."""
    p = kit._P(B, x, y, z, yaw, "book_cart")
    rnd = random.Random(seed + 41)
    w, d = 0.9, 0.42
    for sx in (-w / 2, w / 2):
        p.box(kit.WOOD, sx - 0.02, sx + 0.02, -d / 2, d / 2, 0.08, 0.86, f="xXyYZz")
        p.cyl("CT_Steel", sx, -d / 2 + 0.04, 0.0, 0.07, 0.035, 6)
        p.cyl("CT_Steel", sx, d / 2 - 0.04, 0.0, 0.07, 0.035, 6)
    for zb in (0.14, 0.5):
        p.bev(kit.WOODL, -w / 2, w / 2, -d / 2, d / 2, zb, zb + 0.03, 0.006)
        u = -w / 2 + 0.05
        while u < w / 2 - 0.08:
            bw = rnd.uniform(0.025, 0.05)
            bh = rnd.uniform(0.18, 0.28)
            m = rnd.choice(["CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterGreen", "CT_PlasterYellow", "CT_Leather", "CT_Paper"])
            p.box(m, u, u + bw, -d / 2 + 0.04, d / 2 - 0.05, zb + 0.03, zb + 0.03 + bh, f="xXyYZ")
            u += bw + 0.004
    p.rod(kit.ALU, (-w / 2 + 0.02, d / 2, 0.86), (w / 2 - 0.02, d / 2, 0.86), 0.03, 5)
    p.box("CT_Paper", -0.3, 0.3, -d / 2 - 0.004, -d / 2, 0.62, 0.78, f="y")
    p.core(-w / 2 + 0.03, w / 2 - 0.03, -d / 2 + 0.03, d / 2 - 0.03, 0.14, 0.17)
    return p.done(solid=solid, top=0.86, footprint=(-w / 2, w / 2, -d / 2, d / 2), front=0.7)


def _dryer_hood(B, x, y, z=0.0, yaw=0.0, mat="CT_PlasterPink", solid=False):
    """A salon chair with a hood drier on a pole behind it (hood swung up over the sitter's head). The sitter faces
    -y at yaw 0."""
    p = kit._P(B, x, y, z, yaw, "dryer_hood")
    p.cyl(kit.ALU, 0, 0, 0.0, 0.03, 0.3, 10, top=True)
    p.cyl("CT_Steel", 0, 0, 0.03, 0.4, 0.045, 8)
    p.bev(mat, -0.26, 0.26, -0.26, 0.24, 0.4, 0.48, 0.025, 0.03)
    p.bev(mat, -0.25, 0.25, 0.18, 0.28, 0.48, 0.98, 0.03, 0.03)
    for s in (-1, 1):
        p.bev("CT_Steel", s * 0.3 - 0.03, s * 0.3 + 0.03, -0.18, 0.2, 0.6, 0.64, 0.01)
    p.cyl(kit.ALU, 0, 0.42, 0.0, 1.7, 0.025, 8, top=False)
    p.cyl("CT_Steel", 0, 0.42, 0.0, 0.03, 0.2, 10)
    p.rod(kit.ALU, (0, 0.42, 1.7), (0, 0.08, 1.64), 0.03, 5)
    p.lathe("CT_SteelWhite", 0.0, 0.06, [(0.0, 1.5), (0.26, 1.54), (0.33, 1.66), (0.27, 1.84), (0.1, 1.92), (0.0, 1.93)], 8, top=True)
    p.cyl("CT_SteelRed", 0.0, 0.06, 1.56, 1.6, 0.34, 8, r1=0.345, top=False)
    return p.done(solid=solid, seats=[(0.0, 0.0, 0.48, 0.0)], front=0.9)


def _dress_form(B, x, y, z=0.0, yaw=0.0, mat="CT_Paper"):
    """A tailor's dummy on a tripod: torso without head, a pinned strip of fabric (atlas) on one side."""
    p = kit._P(B, x, y, z, yaw, "dress_form")
    p.cyl("CT_Steel", 0, 0, 0.0, 1.0, 0.016, 6, top=False)
    for k in range(3):
        a = 2 * math.pi * k / 3 + 0.4
        p.rod("CT_Steel", (0.0, 0.0, 0.25), (0.22 * math.cos(a), 0.22 * math.sin(a), 0.0), 0.02, 3)
    p.lathe(mat, 0.0, 0.0, [(0.0, 0.85), (0.15, 0.88), (0.17, 1.02), (0.13, 1.18), (0.15, 1.32), (0.17, 1.45), (0.08, 1.52), (0.0, 1.54)], 10)
    p.cyl("CT_WoodLight", 0, 0, 1.52, 1.62, 0.025, 6)
    p.card_front("fabric_1", -0.16, 0.16, 0.9, 1.4, -0.168)
    return p.done(top=None, footprint=(-0.22, 0.22, -0.22, 0.22), front=0.7)


def _pattern_stand(B, x, y, z=0.0, yaw=0.0, seed=0):
    """[prop] A slanted stand on a counter with the big pattern album open: six swatches of the atlas fabrics."""
    p = kit._P(B, x, y, z, yaw, "pattern_stand")
    p.kind = "prop"
    p.box("CT_Wood", -0.24, 0.24, -0.1, 0.12, 0.0, 0.03, f="xXyYZ")
    p.hexa("CT_Leather", [(-0.24, -0.1, 0.03), (0.24, -0.1, 0.03), (0.24, 0.12, 0.03), (-0.24, 0.12, 0.03)],
           [(-0.24, 0.1, 0.34), (0.24, 0.1, 0.34), (0.24, 0.12, 0.34), (-0.24, 0.12, 0.34)])
    k = 0
    for cu in (-0.22, 0.0):
        for t0, t1 in ((0.08, 0.45), (0.53, 0.92)):
            nm = "fabric_%d" % ((k * 2 + seed) % 6)
            k += 1
            p.card_tilt(nm, cu + 0.01, cu + 0.21, -0.1 + 0.2 * t0 - 0.004, 0.03 + 0.31 * t0, -0.1 + 0.2 * t1 - 0.004, 0.03 + 0.31 * t1)
    return p.done(top=0.34)


def _ticket_post(B, x, y, z=0.0, yaw=0.0):
    """A pedestal with the queue-ticket dispenser on top (a red box with a paper tongue) and a small number display."""
    p = kit._P(B, x, y, z, yaw, "ticket_post")
    p.cyl("CT_Steel", 0, 0, 0.0, 0.03, 0.2, 8)
    p.cyl(kit.ALU, 0, 0, 0.03, 1.0, 0.03, 8, top=False)
    p.bev("CT_SteelRed", -0.13, 0.13, -0.1, 0.1, 1.0, 1.22, 0.015)
    p.box("CT_Paper", -0.04, 0.04, -0.18, -0.1, 1.06, 1.065, f="Zz")
    p.card_front("price_0", -0.1, 0.1, 1.08, 1.2, -0.1015, rect=(0.0, 0.5, 1.0, 1.0))
    p.box("CT_Steel", -0.09, 0.09, -0.06, 0.06, 1.22, 1.5, f="xXyYZ")
    p.card_front("cash_display", -0.08, 0.08, 1.3, 1.42, -0.0615)
    return p.done(top=None, footprint=(-0.2, 0.2, -0.2, 0.2), front=0.7)


def _model_railway(B, x, y, z=0.0, yaw=0.0, w=1.5, d=0.9):
    """A model railway on a low table: green baize, an oval track with sleepers, a red loco with three wagons, a little
    station, trees, and an empty siding with a buffer stop (the depot) at the back right."""
    p = kit._P(B, x, y, z, yaw, "model_railway")
    for sx in (-w / 2 + 0.06, w / 2 - 0.06):
        for sy in (-d / 2 + 0.06, d / 2 - 0.06):
            p.box(kit.WOOD, sx - 0.03, sx + 0.03, sy - 0.03, sy + 0.03, 0.0, 0.7, f="xXyYZ")
    p.bev(kit.WOOD, -w / 2, w / 2, -d / 2, d / 2, 0.7, 0.76, 0.01)
    p.box("CT_Baize", -w / 2 + 0.03, w / 2 - 0.03, -d / 2 + 0.03, d / 2 - 0.03, 0.76, 0.77, f="Z")
    zt = 0.77
    rx, ry, cu, cv = w / 2 - 0.3, d / 2 - 0.2, -0.1, -0.02
    n = 16
    pts = [(cu + rx * math.cos(2 * math.pi * k / n), cv + ry * math.sin(2 * math.pi * k / n)) for k in range(n)]
    for k in range(n):
        a, b = pts[k], pts[(k + 1) % n]
        p.rod("CT_Steel", (a[0], a[1], zt + 0.012), (b[0], b[1], zt + 0.012), 0.012, 4)
        mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
        ang = math.atan2(b[1] - a[1], b[0] - a[0]) + math.pi / 2
        p.rod("CT_Wood", (mx - 0.05 * math.cos(ang), my - 0.05 * math.sin(ang), zt + 0.004),
              (mx + 0.05 * math.cos(ang), my + 0.05 * math.sin(ang), zt + 0.004), 0.016, 4)
    # train on the front straight (y = cv - ry), heading +x
    ty = cv - ry
    ux = cu - 0.28
    p.box("CT_PlasterRed", ux, ux + 0.11, ty - 0.03, ty + 0.03, zt + 0.015, zt + 0.06)
    p.box("CT_PlasterRed", ux + 0.07, ux + 0.11, ty - 0.03, ty + 0.03, zt + 0.06, zt + 0.095)
    p.cyl("CT_Steel", ux + 0.02, ty, zt + 0.06, zt + 0.1, 0.012, 5)
    for k, m in enumerate(("CT_PlasterYellow", "CT_PlasterBlue", "CT_PlasterGreen")):
        a0 = ux - 0.13 * (k + 1) - 0.01
        p.box(m, a0, a0 + 0.11, ty - 0.028, ty + 0.028, zt + 0.015, zt + 0.07)
    # station on the back straight: a house with a roof, a platform and a lamp
    sx0 = cu - 0.25
    p.box("CT_Concrete", sx0 - 0.02, sx0 + 0.34, cv + ry + 0.06, cv + ry + 0.14, zt, zt + 0.02)
    p.box("CT_PlasterYellow", sx0, sx0 + 0.3, cv + ry + 0.1, cv + ry + 0.2, zt + 0.02, zt + 0.1)
    p.bev("CT_SteelRed", sx0 - 0.03, sx0 + 0.33, cv + ry + 0.07, cv + ry + 0.23, zt + 0.1, zt + 0.12, 0.008)
    # trees and a tunnel mouth
    for tx, ty2 in ((cu + rx + 0.12, cv - 0.15), (cu + rx + 0.12, cv + 0.2), (cu - rx - 0.1, cv + 0.18), (cu - rx - 0.12, cv - 0.1)):
        p.cyl("CT_Wood", tx, ty2, zt, zt + 0.05, 0.008, 4)
        p.cyl("CT_Leaf", tx, ty2, zt + 0.04, zt + 0.2, 0.05, 6, r1=0.005)
    p.box("CT_ConcreteDark", cu + 0.35, cu + 0.55, cv - ry - 0.07, cv - ry + 0.07, zt, zt + 0.04, f="xXyYZz")
    # siding with the empty depot (the 'Sever' belongs here)
    sdx = w / 2 - 0.12
    p.rod("CT_Steel", (sdx, cv + 0.02, zt + 0.012), (sdx, cv + ry - 0.04, zt + 0.012), 0.012, 4)
    p.rod("CT_Steel", (sdx + 0.04, cv + 0.02, zt + 0.012), (sdx + 0.04, cv + ry - 0.04, zt + 0.012), 0.012, 4)
    p.box("CT_SteelRed", sdx - 0.01, sdx + 0.05, cv + ry - 0.04, cv + ry - 0.02, zt, zt + 0.03)
    p.box("CT_Wood", sdx - 0.06, sdx + 0.1, cv + 0.02, cv + 0.14, zt + 0.0, zt + 0.07, f="xXyYZ")
    p.core(-w / 2 + 0.03, w / 2 - 0.03, -d / 2 + 0.03, d / 2 - 0.03, 0.66, 0.76)
    return p.done(solid=True, top=0.78, front=0.7)


def _arcade(B):
    """Under the mezzanine: a notice board with the running story, a weighing machine, benches, sandwich boards, a
    planter, four ceiling lamps."""
    def at(x, d):
        return x, yf(x) + d, math.degrees(_th(x))
    x, y, a = at(-9.0, 1.4)
    r = kit.notice_stand(B, x, y, 0.0, a, w=1.2, front="notice", back="price", seed=5, solid=True)
    B.thing("arcade_board", "board", (x, y, 1.5), a, topic="local")
    B.spot("arcade_board_stand", "stand", (x, y - 0.8, 0.0), a)
    x, y, a = at(-21.3, 1.3)
    kit.scales(B, x, y, 0.0, a)
    B.thing("arcade_scales", "machine", (x, y, 1.2), a, what="scales")
    B.spot("arcade_scales_stand", "stand", (x, y - 0.7, 0.0), a)
    for bx in (-3.8, 8.6, 28.2):
        x, y, a = at(bx, 1.55)
        kit.bench(B, x, y, 0.0, a, w=1.7, kind="park")
        B.spot("arcade_bench_%d" % int(bx), "bench", (x, y, 0.0), a)
    for bx, nm in ((-32.2, "menu_0"), (-7.5, "price_1"), (21.0, "menu_1")):
        x, y, a = at(bx, 1.6)
        kit.sandwich_board(B, x, y, 0.0, a, name=nm)
    x, y, a = at(11.0, 1.5)
    kit.planter(B, x, y, 0.0, a, w=1.6, seed=3)
    for lx in (-30.0, -8.0, 14.0, 32.0):
        x, y, a = at(lx, 1.0)
        B.lamp((x, y, 2.9), lumens=620, colour="#FFE2B8", kind="public", radius=7.0, fixture=False)
        kit.pendant(B, x, y, 3.2, drop=0.3, kind="opal")


def _bakery2(S):
    B = S.B
    _dado(S, "CT_TileWhite", "CT_PlasterRed", zh=1.15)
    ck = S.f(_cat_basket, 5.2, 0.75, 0)
    B.thing("bakery_cat_basket", "machine", (ck["pos"][0], ck["pos"][1], 0.3), S.yw(5.2), what="cat_basket")
    S.spot("bakery_cat_stand", "stand", 5.2, 1.7, 0)
    S.back(kit.menu_board, 3.75, name="menu_0", w=0.8, zc=1.75)
    x = S.xl + 3.75
    B.thing("bakery_menu", "board", (x, S.y_back(x) - 0.25, 1.75), S.yw(3.75), topic="prices")
    S.spot("bakery_menu_stand", "stand", 3.75, 6.9, 180)


def _records2(S):
    B = S.B
    _dado(S, "CT_Leather", "CT_Brass", zh=1.0)
    r, rp, seats = _headphone_post(B, *S.P(3.7, 1.95), 0.0, S.yw(3.7))
    S.check(r)
    B.thing("records_booth", "machine", (rp["pos"][0], rp["pos"][1], 1.0), S.yw(3.7), what="record_player")
    for k, st in enumerate(seats):
        B.spot("records_booth_seat%d" % (k + 1), "seat", (st["seats"][0][0], st["seats"][0][1], 0.0), S.yw(3.7))
    S.f(_sleeve_stand, 5.5, 0.5, 0, w=1.2)
    S.right(kit.notice_board, 1.0, w=0.9, zc=1.7, seed=9)
    B.thing("records_hits", "board", (S.xr - 0.1, S.y_front(S.xr) + 1.0, 1.7), 270, topic="catalogue")
    S.spot_w("records_hits_stand", "stand", "r", 1.0)


def _books2(S):
    B = S.B
    _dado(S, "CT_PlasterGreen", "CT_WoodLight", zh=1.0)
    c = S.f(_book_cart, 3.6, 2.4, 0)
    B.thing("books_cart", "shelf", (c["pos"][0], c["pos"][1], 0.9), S.yw(3.6), topic="local")
    S.spot("books_cart_stand", "stand", 3.6, 3.1, 180)
    S.f(kit.table, 1.9, 0.55, 0, w=0.9, d=0.6, mat=kit.WOOD)
    S.on_top(kit.books_stack, 1.9, 0.55, -0.22, 0.0, z=0.75, n=5, seed=8)
    S.on_top(kit.books_stack, 1.9, 0.55, 0.05, 0.05, z=0.75, n=3, seed=2)
    S.on_top(kit.lamp_green, 1.9, 0.55, 0.3, 0.0, z=0.75)
    S.right(kit.picture, 0.6, name="painting_lighthouse", w=0.5, zc=1.9)
    S.spot("books_new_stand", "stand", 5.3, 4.5, 180)


def _hardware2(S):
    B = S.B
    _dado(S, "CT_Steel", "CT_PlasterYellow", zh=1.0, front=2.2)
    S.left(kit.bin_rack, 1.4, cols=4, rows=6, seed=3)
    for k, u in enumerate((6.3, 7.2, 8.1)):
        x, y = S.P(u, 1.3)
        kit.bird_feeder(B, x, y, 0.0, S.yw(u), kind="hang", top_z=3.1)
    fp = S.f(kit.bird_feeder, 4.9, 0.6, 0, kind="post")
    B.thing("hardware_feeders", "board", (fp["pos"][0], fp["pos"][1], 1.3), S.yw(4.9), topic="birds")
    S.spot("hardware_feeder_stand", "stand", 4.9, 1.6, 0)
    x, y = S.P(5.3, 3.3)
    ks = kit.kitchen_scales(B, x - 0.5, y, 1.0, S.yw(5.3))
    B.thing("hardware_nail_scale", "machine", (x - 0.5, y, 1.1), S.yw(5.3), what="weigh_nails")
    S.spot("hardware_nails_stand", "stand", 4.8, 4.2, 180)
    S.spot("hardware_garden_stand", "stand", 7.2, 2.4, 0)


def _salon2(S):
    B = S.B
    _dado(S, "CT_PlasterPink", "CT_TileWhite", zh=1.1, front=0.2, side_to=4.2, back=False)
    for k, u in enumerate((5.4, 6.55)):
        d = S.f(_dryer_hood, u, 1.0, 0)
        s = d["seats"][0]
        B.thing("salon_dryer_%d" % (k + 1), "machine", (s[0], s[1], 0.6), s[3], what="dryer")
        B.spot("salon_dryer_seat%d" % (k + 1), "seat", (s[0], s[1], 0.0), s[3])
    # savings bank: the queue-ticket post by the door, the bond board in the waiting hall
    tp = S.f(_ticket_post, 2.45, 1.5, 0)
    B.thing("bank_ticket", "machine", (tp["pos"][0], tp["pos"][1], 1.15), S.yw(2.45), what="queue_ticket")
    S.spot("bank_ticket_stand", "stand", 2.45, 2.2, 0)
    ns = S.f(kit.notice_stand, 1.55, 3.2, 0, w=1.0, front="price", back="notice", seed=4)
    B.thing("bank_bonds", "board", (ns["pos"][0], ns["pos"][1], 1.45), S.yw(1.55), topic="prices")
    S.spot("bank_bonds_stand", "stand", 1.55, 2.5, 0)
    S.f(kit.coffee_table, 7.7, 2.5, 0, w=0.9, d=0.5)
    S.on_top(kit.newspaper, 7.7, 2.5, -0.25, 0.0, z=0.42, cellname="mag_3")
    S.on_top(kit.newspaper, 7.7, 2.5, 0.05, 0.05, z=0.42, cellname="mag_6")
    S.on_top(kit.cup, 7.7, 2.5, 0.3, 0.0, z=0.42)
    x, y = S.P(7.7, 2.5)
    B.thing("salon_magtable", "shelf", (x, y, 0.5), S.yw(7.7), topic="art")
    S.spot("salon_magtable_stand", "stand", 7.7, 3.4, 180)


def _toyfab2(S):
    B = S.B
    _dado(S, "CT_PlasterYellow", "CT_PlasterRed", zh=0.9, front=0.2, side_to=3.4, back=False)
    # fabrics: a dummy in the window, the pattern album on the counter
    S.f(_dress_form, 1.9, 3.0, 0)
    S.spot("fabric_dummy_stand", "stand", 1.9, 3.9, 180)
    pa = S.on_top(_pattern_stand, 1.6, 5.0, 0.55, -0.05, z=1.05)
    x, y = S.P(1.6 + 0.55, 5.0)
    B.thing("fabric_pattern_book", "board", (x, y, 1.25), S.yw(1.6), topic="art")
    # toys: the model railway in the window, two planes in the air
    rr = S.f(_model_railway, 6.6, 1.2, 0, w=1.5, d=0.9)
    x, y = S.P(6.6, 1.2)
    B.thing("toys_railway", "machine", (x - 0.2, y, 0.85), S.yw(6.6), what="railway")
    B.thing("toys_loco_depot", "board", (x + 0.55, y + 0.2, 0.85), S.yw(6.6), topic="kids")
    S.spot("toys_railway_watch1", "stand", 6.0, 2.3, 0)
    S.spot("toys_railway_watch2", "stand", 7.2, 2.3, 0)
    x, y = S.P(5.6, 3.9)
    kit.model_plane(B, x, y, 0.0, S.yw(5.6), top_z=3.15)


# ==================================================================================================== round 2b
# The through passage becomes an amusement alley (cabinets, pinball, change machine, prize counter, score board, a
# broken cabinet with a repair note); the lobbies get a switch and the middle core a lift to the roof terrace, where the
# terrace gets two viewing tables, a coin telescope and benches that face the sea.
def _lift_leaf2(B, cx, cy, z, yaw):
    """A lift door set in a wall plane through (cx, cy): frame, two leaves, call button, floor lamp. yaw 0: the wall's
    face looks to -y; yaw 180: to +y. Drawing only."""
    def bx(u0, u1, v0, v1, z0, z1, mat):
        if yaw == 0:
            box(B.detail, mat, cx + u0, cx + u1, cy - v1, cy - v0, z + z0, z + z1)
        else:
            box(B.detail, mat, cx - u1, cx - u0, cy + v0, cy + v1, z + z0, z + z1)
    bx(-0.52, 0.52, 0.0, 0.05, 0.0, 2.2, "CT_Steel")
    bx(-0.45, -0.005, 0.05, 0.075, 0.0, 2.1, "CT_SteelWhite")
    bx(0.005, 0.45, 0.05, 0.075, 0.0, 2.1, "CT_SteelWhite")
    bx(-0.04, 0.04, 0.075, 0.09, 0.0, 2.1, "CT_Steel")
    bx(0.6, 0.68, 0.0, 0.03, 1.0, 1.2, "CT_ConcreteWhite")
    bx(0.625, 0.655, 0.03, 0.045, 1.07, 1.13, "CT_SteelRed")
    bx(-0.12, 0.12, 0.0, 0.03, 2.28, 2.38, "CT_Opal")


def _pinball(B, x, y, z=0.0, yaw=0.0):
    """A pinball table: legs, a cabinet with a baize playfield dotted with bumpers, a back box with a lit picture.
    The player stands at -y."""
    p = kit._P(B, x, y, z, yaw, "pinball")
    p.kind = "floor"
    for sx in (-0.3, 0.3):
        for sy in (-0.5, 0.5):
            p.box("CT_Steel", sx - 0.025, sx + 0.025, sy - 0.025, sy + 0.025, 0.0, 0.72)
    p.box("CT_Wood", -0.36, 0.36, -0.58, 0.58, 0.72, 0.85, f="xXyYz")
    p.box("CT_Baize", -0.31, 0.31, -0.53, 0.53, 0.85, 0.86, f="Z")
    for bxx, byy, mt in ((-0.1, 0.25, "CT_SteelRed"), (0.12, 0.15, "CT_Neon"), (0.0, 0.0, "CT_SteelRed"),
                         (-0.15, -0.12, "CT_Neon"), (0.17, -0.2, "CT_SteelRed")):
        p.cyl(mt, bxx, byy, 0.86, 0.92, 0.04, 8)
    p.box("CT_Steel", -0.3, -0.08, -0.46, -0.4, 0.86, 0.9)
    p.box("CT_Steel", 0.08, 0.3, -0.46, -0.4, 0.86, 0.9)
    p.box("CT_Steel", 0.32, 0.36, -0.5, 0.5, 0.85, 0.95, f="xXyYZ")
    p.box("CT_Steel", -0.36, -0.32, -0.5, 0.5, 0.85, 0.95, f="xXyYZ")
    p.box("CT_PlasterBlue", -0.34, 0.34, 0.4, 0.58, 0.86, 1.62, f="xXyYZz")
    p.card_front("screen_arcade_1", -0.28, 0.28, 1.0, 1.52, 0.395)
    p.box("CT_Neon", -0.3, 0.3, 0.39, 0.42, 1.54, 1.6)
    p.core(-0.36, 0.36, -0.58, 0.58, 0.0, 0.88)
    return p.done(solid=False, top=0.9, footprint=(-0.38, 0.38, -0.6, 0.6), front=0.7)


def _change_machine(B, x, y, z=0.0, yaw=0.0):
    """A tall token changer: blue body, a note slot, a display, a steel coin tray and a price plate. Front at -y."""
    p = kit._P(B, x, y, z, yaw, "change_machine")
    p.kind = "floor"
    p.bev("CT_PlasterBlue", -0.25, 0.25, -0.2, 0.2, 0.0, 1.7, 0.02, 0.04)
    p.box("CT_SteelRed", -0.2, 0.2, -0.205, -0.2, 1.4, 1.6, f="y")
    p.card_front("cash_display", -0.12, 0.12, 1.45, 1.57, -0.206)
    p.box("CT_Steel", -0.1, 0.1, -0.215, -0.2, 1.22, 1.3, f="xXyYZz")
    p.box("CT_Steel", -0.14, 0.14, -0.26, -0.2, 0.86, 0.96, f="xXyYZz")
    p.box("CT_ConcreteDark", -0.1, 0.1, -0.262, -0.255, 0.88, 0.94, f="y")
    p.card_front("price_0", -0.16, 0.16, 0.45, 0.75, -0.206, rect=(0.0, 0.0, 1.0, 0.6))
    p.core(-0.25, 0.25, -0.2, 0.2, 0.0, 1.7)
    return p.done(solid=False, top=None, footprint=(-0.28, 0.28, -0.28, 0.2), front=0.7)


def _bin(B, x, y, z=0.0):
    p = kit._P(B, x, y, z, 0.0, "bin")
    p.kind = "prop"
    p.cyl("CT_Steel", 0, 0, 0.0, 0.62, 0.19, 10)
    p.cyl("CT_SteelRed", 0, 0, 0.62, 0.66, 0.21, 10)
    p.box("CT_Paper", -0.06, 0.04, -0.03, 0.05, 0.64, 0.7, f="xXyYZ")
    return p.done(solid=False, top=None, footprint=(-0.21, 0.21, -0.21, 0.21))


def _view_table(B, x, y, z, yaw):
    """A brass orientation table on a post: a tilted disc with a compass rose, a plate in front. Faces -y at yaw 0."""
    p = kit._P(B, x, y, z, yaw, "view_table")
    p.kind = "prop"
    p.cyl("CT_Steel", 0, 0, 0.0, 0.05, 0.22, 10)
    p.cyl("CT_Steel", 0, 0, 0.05, 0.98, 0.05, 8)
    p.loft("CT_Brass", [[(-0.3, -0.18, 0.98), (0.3, -0.18, 0.98), (0.3, 0.18, 1.08), (-0.3, 0.18, 1.08)],
                        [(-0.3, -0.18, 1.0), (0.3, -0.18, 1.0), (0.3, 0.18, 1.1), (-0.3, 0.18, 1.1)]], top=True, bottom=True)
    p.card_tilt("travel_0", -0.26, 0.26, -0.15, 1.005, 0.15, 1.097)
    p.core(-0.3, 0.3, -0.2, 0.2, 0.0, 1.1)
    return p.done(solid=False, top=None, footprint=(-0.32, 0.32, -0.25, 0.25), front=0.7)


def _alley(B):
    """The through passage between the records shop and the bookshop, x -15..-10: left wall faces +x (yaw 90), right
    wall faces -x (yaw 270). Everything with a thing here belongs to the rules 'wave_*' in x_p_wave.json."""
    xl, xr = XS[9] + 0.15, XS[11] - 0.15
    # left wall: shooter, racing cabinet with its stool, the broken cabinet (note above), score board, picture
    for y, nm, mat, sd in ((6.8, "shoot", "CT_PlasterBlue", 1), (7.95, "race", "CT_PlasterRed", 2), (9.1, "broken", "CT_PlasterGreen", 3)):
        kit.arcade(B, xl + 0.43, y, 0.0, 90, screen="screen_arcade_%d" % (sd % 3), mat=mat, seed=sd)
        B.thing("wave_cab_" + nm, "machine", (xl + 0.43, y, 1.2), 90, what="arcade")
    kit.stool(B, xl + 1.35, 7.95, 0.0, 270, mat="CT_PlasterRed")
    B.thing("wave_stool_race", "seat", (xl + 1.35, 7.95, 0.45), 270, height=0.45)
    B.spot("wave_race_seat", "seat", (xl + 1.35, 7.95, 0.0), 270)
    B.spot("wave_shoot_stand", "stand", (xl + 1.2, 6.8, 0.0), 270)
    B.spot("wave_broken_stand", "stand", (xl + 1.2, 9.1, 0.0), 270)
    kit.poster(B, xl + 0.005, 9.1, 0.0, 90, name="notice_1", w=0.3, zc=2.05)
    B.thing("wave_repair_note", "board", (xl + 0.05, 9.1, 2.05), 90, topic="local")
    kit.notice_board(B, xl + 0.005, 10.5, 0.0, 90, w=1.0, zc=1.65, seed=11)
    B.thing("wave_hiscores", "board", (xl + 0.06, 10.5, 1.65), 90, topic="local")
    B.spot("wave_hiscores_stand", "stand", (xl + 0.9, 10.5, 0.0), 270)
    kit.poster(B, xl + 0.005, 11.7, 0.0, 90, name="poster_cons_3", w=0.7, zc=1.75)
    kit.poster(B, xl + 0.005, 12.5, 0.0, 90, name="poster_safety", w=0.55, zc=1.75)
    kit.wall_clock(B, xl + 0.005, 12.0, 0.0, 90, zc=2.5, r=0.2)
    # right wall: switch, pinball, change machine, prize counter and its shelf, bin
    kit.poster(B, xr - 0.005, 5.7, 0.0, 270, name="poster_cons_0", w=0.7, zc=1.75)
    box(B.detail, "CT_ConcreteWhite", xr - 0.03, xr, 4.72, 4.8, 1.38, 1.5)
    box(B.detail, "CT_SteelRed", xr - 0.045, xr - 0.03, 4.74, 4.78, 1.42, 1.46)
    B.thing("wave_switch", "machine", (xr - 0.05, 4.76, 1.45), 270, what="switch", switch="room")
    _pinball(B, xr - 0.45, 6.8, 0.0, 270)
    B.thing("wave_pinball", "play", (xr - 0.45, 6.8, 0.9), 270, what="pinball")
    B.spot("wave_pinball_stand", "stand", (xr - 1.3, 6.8, 0.0), 90)
    _change_machine(B, xr - 0.3, 8.0, 0.0, 270)
    B.thing("wave_change", "machine", (xr - 0.3, 8.0, 1.2), 270, what="change")
    B.spot("wave_change_stand", "stand", (xr - 1.0, 8.0, 0.0), 90)
    kit.shop_shelf(B, xr - 0.225, 10.4, 0.0, 270, w=1.6, goods="toys")
    kit.shop_counter(B, xr - 1.35, 10.4, 0.0, 270, w=1.6, d=0.7, front="CT_PlasterBlue", till=False)
    B.thing("wave_prize_counter", "counter", (xr - 1.35, 10.4, 1.05), 270, role="seller", shop="wave_prizes")
    B.thing("wave_prize_shelf", "board", (xr - 0.3, 10.4, 1.4), 270, topic="kids")
    B.spot("wave_prize_counter", "counter", (xr - 2.2, 10.4, 0.0), 90)
    B.spot("wave_prize_clerk", "work", (xr - 0.7, 10.4, 0.0), 270)
    B.spot("wave_prize_queue", "stand", (xr - 2.2, 11.4, 0.0), 90)
    kit.poster(B, xr - 0.005, 12.4, 0.0, 270, name="poster_cons_4", w=0.7, zc=1.75)
    _bin(B, xr - 0.35, 12.9)
    B.thing("wave_bin", "machine", (xr - 0.35, 12.9, 0.5), 270, what="bin")


def _lobby2(B, a, b, name):
    """A light switch by the street door of each lobby (the middle core's lift is in _alley's neighbour, below)."""
    xl = XS[a] + 0.15
    xm = (XS[a] + XS[b]) / 2
    yy = yf(xm) + 2.8 + 2.4
    box(B.detail, "CT_ConcreteWhite", xl, xl + 0.03, yy - 0.5, yy - 0.42, 1.38, 1.5)
    box(B.detail, "CT_SteelRed", xl + 0.03, xl + 0.045, yy - 0.48, yy - 0.44, 1.42, 1.46)
    B.thing("%s_switch" % name, "machine", (xl + 0.05, yy - 0.46, 1.45), 90, what="switch", switch="room")
    if name == "core2":
        th = _th(xm)
        yback = yb(xm) - 0.3 / math.cos(th) + 0.025
        cx = xm + 1.45
        _lift_leaf2(B, cx, yback, 0.0, 0)
        B.thing("lift_g", "door", (cx, yback - 0.04, 1.1), 0.0, to="lift_roof")
        B.spot("lift_g_wait", "stand", (cx, yback - 0.9, 0.0), 0.0)


def _roof2(B, zr):
    """Roof terrace: the lift stop on the back of the middle plant room, two viewing tables (sea, town), a coin
    telescope, and the two timber benches as seats that face the sea."""
    xm = (XS[14] + XS[16]) / 2
    cy = yc(xm) + 3.0
    _lift_leaf2(B, xm, cy, zr, 180)
    B.thing("lift_roof", "door", (xm, cy + 0.04, zr + 1.1), 180.0, to="lift_g")
    B.spot("lift_roof_wait", "stand", (xm, cy + 0.9, zr), 180.0)
    x, y = 20.0, yf(20.0) + 0.75
    _view_table(B, x, y, zr, 0)
    B.thing("roof_view_sea", "board", (x, y, zr + 1.05), 0.0, topic="view")
    x2, y2 = 27.0, yf(27.0) + 0.75
    kit.telescope(B, x2, y2, zr, 0)
    B.thing("roof_telescope", "machine", (x2, y2, zr + 1.3), 0.0, what="telescope")
    x3 = 16.0
    y3 = yb(x3) - 0.9
    _view_table(B, x3, y3, zr, 180)
    B.thing("roof_view_town", "board", (x3, y3, zr + 1.05), 180.0, topic="view")
    for bx in (13.0, 25.0):
        B.thing("roof_seat_%d" % int(bx), "seat", (bx, yc(bx) - 1.58, zr + 0.55), 0.0, height=0.55)


# ==================================================================================================== round 3 (shops)
# The thinnest shops get a second look: the savings bank (coin tray, bond-draw calendar, clock), the hairdresser
# (style album, stylist's trolley, radio), the fabrics (cutting table with a brass rule, remnant basket, clock) and the
# bakery (bread of the day, order ledger, clock). Acts and words: tools/city/content/x_p_wave.json.
def _coin_tray(B, x, y, z=0.0, yaw=0.0):
    """[prop] A wooden coin-sorting tray with stacks of brass and steel coins, a paper roll and a small cloth bag."""
    p = kit._P(B, x, y, z, yaw, "coin_tray")
    p.kind = "prop"
    p.bev(kit.WOOD, -0.26, 0.26, -0.15, 0.15, 0.0, 0.035, 0.006)
    p.box("CT_Leather", -0.24, 0.24, -0.13, 0.13, 0.035, 0.04, f="Z")
    for k in range(5):
        for j, h in enumerate((0.05, 0.035)):
            m = kit.BRASS if (k + j) % 2 == 0 else "CT_Steel"
            p.cyl(m, -0.2 + k * 0.09, -0.06 + j * 0.1, 0.04, 0.04 + h * (1 + (k % 3) * 0.5), 0.02, 8)
    p.cyl("CT_Paper", 0.17, 0.1, 0.04, 0.12, 0.022, 8)
    p.lathe("CT_PlasterBlue", 0.2, -0.06, [(0.0, 0.04), (0.04, 0.045), (0.05, 0.08), (0.025, 0.13), (0.0, 0.13)], 7)
    return p.done(top=0.14, footprint=(-0.26, 0.26, -0.15, 0.15))


def _stylist_trolley(B, x, y, z=0.0, yaw=0.0):
    """A three-tier steel trolley on castors: a jar of combs, scissors in a case, bottles, curlers in a bowl, towels."""
    p = kit._P(B, x, y, z, yaw, "stylist_trolley")
    for sx in (-0.26, 0.26):
        for sy in (-0.17, 0.17):
            p.box("CT_Steel", sx - 0.015, sx + 0.015, sy - 0.015, sy + 0.015, 0.05, 0.84, f="xXyY")
            p.cyl("CT_Steel", sx, sy, 0.0, 0.05, 0.03, 6)
    for zb in (0.2, 0.5, 0.82):
        p.bev(kit.ALU, -0.3, 0.3, -0.21, 0.21, zb, zb + 0.025, 0.006)
    p.cyl("CT_TileWhite", -0.2, -0.05, 0.845, 0.95, 0.045, 8)
    for k in range(4):
        p.rod("CT_Leather", (-0.22 + k * 0.014, -0.06, 0.95), (-0.23 + k * 0.03, -0.05, 1.03), 0.012, 3)
    p.box("CT_Leather", -0.05, 0.12, -0.1, 0.02, 0.845, 0.875, f="xXyYZ")
    for k, m in enumerate(("CT_Leaf", "CT_PlasterBlue", "CT_PlasterRed")):
        p.cyl(m, 0.17 + 0.0 * k, -0.12 + k * 0.1, 0.845, 0.99 - 0.04 * k, 0.03, 7, r1=0.022)
    p.lathe("CT_PlasterPink", 0.05, 0.12, [(0.0, 0.525), (0.1, 0.55), (0.12, 0.62), (0.0, 0.62)], 8, top=False)
    p.cushion("CT_Paper", -0.26, 0.05, -0.12, 0.14, 0.525, 0.62, puff=0.015)
    p.cushion("CT_PlasterBlue", -0.25, 0.04, -0.12, 0.14, 0.62, 0.68, puff=0.015)
    p.core(-0.28, 0.28, -0.19, 0.19, 0.5, 0.52)
    return p.done(solid=True, top=0.85, footprint=(-0.3, 0.3, -0.21, 0.21), front=0.7)


def _cutting_table(B, x, y, z=0.0, yaw=0.0, w=1.3):
    """A tall cutting table with a brass rule let into the customer edge, a bolt of cloth, shears and chalk."""
    p = kit._P(B, x, y, z, yaw, "cutting_table")
    d = 0.62
    for sx in (-w / 2 + 0.06, w / 2 - 0.06):
        for sy in (-d / 2 + 0.06, d / 2 - 0.06):
            p.box(kit.WOOD, sx - 0.03, sx + 0.03, sy - 0.03, sy + 0.03, 0.0, 0.88, f="xXyY")
    p.bev(kit.WOODL, -w / 2, w / 2, -d / 2, d / 2, 0.88, 0.93, 0.01)
    p.box("CT_Leather", -w / 2 + 0.05, w / 2 - 0.05, -d / 2 + 0.05, d / 2 - 0.05, 0.3, 0.32, f="xXyYZ")
    p.box(kit.BRASS, -w / 2 + 0.04, w / 2 - 0.04, -d / 2 + 0.015, -d / 2 + 0.065, 0.93, 0.935, f="Z")
    for k in range(int((w - 0.1) / 0.1) + 1):
        h = 0.03 if k % 5 == 0 else 0.018
        p.box("CT_Steel", -w / 2 + 0.05 + k * 0.1 - 0.004, -w / 2 + 0.05 + k * 0.1 + 0.004, -d / 2 + 0.065, -d / 2 + 0.065 + h, 0.935, 0.937, f="Z")
    p.rod("CT_PlasterPink", (-0.5, 0.12, 1.0), (0.05, 0.12, 1.0), 0.17, 8)
    p.rod("CT_PlasterBlue", (-0.2, 0.12, 1.17), (0.3, 0.12, 1.17), 0.15, 8)
    p.box("CT_PlasterYellow", 0.18, 0.5, -0.12, 0.12, 0.93, 0.945, f="xXyYZ")
    p.rod("CT_Steel", (0.28, -0.1, 0.96), (0.46, -0.02, 0.96), 0.014, 3)
    p.rod("CT_Steel", (0.28, -0.02, 0.96), (0.46, -0.1, 0.96), 0.014, 3)
    p.box("CT_Paper", -0.55, -0.45, -0.17, -0.13, 0.93, 0.95, f="xXyYZ")
    p.core(-w / 2 + 0.03, w / 2 - 0.03, -d / 2 + 0.03, d / 2 - 0.03, 0.8, 0.9)
    return p.done(solid=True, top=0.94, footprint=(-w / 2, w / 2, -d / 2, d / 2), front=0.7)


def _remnant_basket(B, x, y, z=0.0, yaw=0.0):
    """A wicker basket heaped with fabric remnants, one strip of print hanging over the front rim."""
    p = kit._P(B, x, y, z, yaw, "remnant_basket")
    p.kind = "prop"
    p.bev("CT_WoodLight", -0.3, 0.3, -0.24, 0.24, 0.0, 0.34, 0.012, 0.05)
    for k in range(4):
        zz = 0.06 + k * 0.07
        p.box("CT_Wood", -0.305, 0.305, -0.245, -0.238, zz, zz + 0.012, f="xXyYZ")
        p.box("CT_Wood", -0.305, 0.305, 0.238, 0.245, zz, zz + 0.012, f="xXyYZ")
    for k, (m, a, b, c, d2) in enumerate((("CT_PlasterRed", -0.26, 0.0, -0.18, 0.1), ("CT_PlasterBlue", -0.05, 0.25, -0.12, 0.16),
                                          ("CT_PlasterYellow", -0.2, 0.1, 0.02, 0.2), ("CT_PlasterGreen", 0.05, 0.27, 0.0, 0.2))):
        p.cushion(m, a, b, c, d2, 0.3, 0.38 + 0.03 * (k % 2), puff=0.02)
    p.card_front("fabric_3", -0.14, 0.1, 0.2, 0.36, -0.252)
    return p.done(top=0.4, footprint=(-0.3, 0.3, -0.24, 0.24), front=0.7)


def _ledger(B, x, y, z=0.0, yaw=0.0):
    """[prop] A fat order ledger in black oilcloth lying open, a pencil on a string and a paper bookmark."""
    p = kit._P(B, x, y, z, yaw, "ledger")
    p.kind = "prop"
    p.box("CT_Leather", -0.17, 0.17, -0.12, 0.12, 0.0, 0.035, f="xXyYZ")
    p.box("CT_Paper", -0.15, -0.005, -0.1, 0.1, 0.035, 0.05, f="xXyYZ")
    p.box("CT_Paper", 0.005, 0.15, -0.1, 0.1, 0.035, 0.045, f="xXyYZ")
    p.rod("CT_PlasterRed", (0.1, -0.05, 0.05), (0.2, -0.13, 0.055), 0.01, 3)
    p.rod("CT_Wood", (0.02, 0.06, 0.05), (0.14, 0.02, 0.055), 0.012, 4)
    return p.done(top=0.055)


def _clock_thing(S, side, s, zc, tid, r=0.2):
    """A wall clock on the left/right wall with its `clock` board thing."""
    if side == "l":
        S.left(kit.wall_clock, s, zc=zc, r=r)
        S.B.thing(tid, "board", (S.xl + 0.1, S.y_front(S.xl) + s, zc), 90, topic="clock")
    else:
        S.right(kit.wall_clock, s, zc=zc, r=r)
        S.B.thing(tid, "board", (S.xr - 0.1, S.y_front(S.xr) + s, zc), 270, topic="clock")


def _salon3(S):
    B = S.B
    # savings bank: a clock and the bond-draw calendar above the notice board, a coin-sorting table
    _clock_thing(S, "l", 2.5, 2.45, "bank_clock")
    kit.poster(B, S.xl + 0.012, S.y_front(S.xl) + 3.3, 0.0, 90.0, name="chart_calendar", w=0.4, zc=2.45)
    B.thing("bank_calendar", "board", (S.xl + 0.1, S.y_front(S.xl) + 3.3, 2.45), 90, topic="prices")
    tb = S.f(kit.table, 2.5, 3.9, 0, w=0.6, d=0.5, mat=kit.WOOD)
    ct = S.on_top(_coin_tray, 2.5, 3.9, 0.0, 0.0, z=0.75)
    x, y = S.P(2.5, 3.9)
    B.thing("bank_coins", "machine", (x, y, 0.85), S.yw(2.5), what="coin_count")
    S.spot("bank_coins_stand", "stand", 2.4, 4.5, 0)
    # hairdresser: the style album on the right wall (three ink drawings), the stylist's trolley, a radio at the desk
    ys = S.y_front(S.xr) + 5.2
    kit.mirror(B, S.xr - 0.012, ys, 0.0, 270.0, zc=1.7, w=0.55, h=0.8)
    kit.wall_shelf(B, S.xr - 0.012, ys, 0.0, 270.0, w=0.8, zt=1.0, props=False)
    _ledger(B, S.xr - 0.17, ys, 1.0, 270.0)
    B.thing("salon_styles", "board", (S.xr - 0.15, S.y_front(S.xr) + 5.2, 1.15), 270, topic="prices")
    S.spot_w("salon_styles_stand", "stand", "r", 5.2)
    tr = S.f(_stylist_trolley, 9.05, 6.0, a=-90)
    B.thing("salon_trolley", "machine", (tr["pos"][0], tr["pos"][1], 0.9), S.yw(9.05) + 90, what="hair_trolley")
    S.spot("salon_trolley_stand", "stand", 8.3, 5.4, 90)
    rd = S.on_top(kit.radio, 7.0, 4.3, 0.55, 0.05)
    x, y = S.P(7.55, 4.35)
    B.thing("salon_radio", "machine", (x, y, 1.2), S.yw(7.0), what="radio")


def _toyfab3(S):
    B = S.B
    ct = S.f(_cutting_table, 3.5, 6.4, 0, w=1.3)
    B.thing("fabric_cutting", "machine", (ct["pos"][0], ct["pos"][1], 1.0), S.yw(3.5), what="cutting")
    S.spot("fabric_cut_stand", "stand", 3.5, 5.55, 180)
    rb = S.f(_remnant_basket, 3.15, 4.3, a=90)
    B.thing("fabric_remnants", "machine", (rb["pos"][0], rb["pos"][1], 0.45), S.yw(3.15) + 90, what="remnants")
    S.spot("fabric_remnants_stand", "stand", 3.7, 3.6, 0)
    _clock_thing(S, "l", 5.0, 2.5, "fabric_clock")


def _bakery3(S):
    B = S.B
    kit.poster(B, S.xr - 0.012, S.y_front(S.xr) + 0.95, 0.0, 270.0, name="paper_typed", w=0.45, zc=1.7)
    B.thing("bakery_bread_day", "board", (S.xr - 0.1, S.y_front(S.xr) + 0.95, 1.75), 270, topic="prices")
    S.spot_w("bakery_bread_stand", "stand", "r", 1.1)
    S.on_top(_ledger, 6.6, 6.0, 1.5, 0.1)
    x, y = S.P(6.6 + 1.5, 6.1)
    B.thing("bakery_orders", "board", (x, y, 1.15), S.yw(6.6), topic="prices")
    xb = S.xl + 3.75
    B.thing("bakery_clock", "board", (xb, S.y_back(xb) - 0.25, 2.6), S.yw(3.75), topic="clock")


def furnish(B, rnd):
    done = []
    for a, b, name in UNITS:
        if name in SHOP_TRADES:
            S = Shop(B, a, b, name, rnd)
            S.awning()
            {"bakery": _bakery, "records": _records, "books": _books, "hardware": _hardware,
             "salon": _salon, "toyfab": _toyfab}[S.trade](S)
            {"bakery": _bakery2, "records": _records2, "books": _books2, "hardware": _hardware2,
             "salon": _salon2, "toyfab": _toyfab2}[S.trade](S)
            {"bakery": _bakery3, "salon": _salon3, "toyfab": _toyfab3}.get(S.trade, lambda S: None)(S)
            done.append(S)
        elif name.startswith("core"):
            _lobby(B, rnd, a, b, name)
            _lobby2(B, a, b, name)
        elif name == "passage":
            _passage(B, a, b)
    if os.environ.get("KITCHECK"):
        for S in done:
            print("SHOP", S.name, S.trade, "tris", S.tris)
            for w in S.warn:
                print("  WARN", w)
        print("KITCHECK", kit.check(B))


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
    furnish(B, random.Random(1960))
    _arcade(B)
    _alley(B)
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
    _roof2(B, zr)

    # ---------------------------------------------------------------- footprint, height
    right = [(XS[i], yf(XS[i]) - 1.5) for i in range(N + 1)]
    B.footprint = right + [(XS[-1], yb(XS[-1]))] + [(XS[i], yb(XS[i])) for i in range(N - 1, -1, -1)]
    B.height = zr + 2.9 + 0.12
    B.note("%d flats on %d storeys, S-curved slab with undulating brise-soleil bands" % (len(flats), N_FLAT_ST))
