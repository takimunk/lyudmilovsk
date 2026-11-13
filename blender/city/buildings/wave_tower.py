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
        return self.check(fn(self.B, self.xl - 0.005, y, 0.0, 90.0, **kw))

    def right(self, fn, s, **kw):
        y = self.y_front(self.xr) + s
        return self.check(fn(self.B, self.xr + 0.005, y, 0.0, 270.0, **kw))

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
        for y in ys:
            for x in xs:
                B.lamp((x, y, 2.9), lumens=lumens, colour=colour, kind="public", radius=7.0, fixture=False)
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


def furnish(B, rnd):
    done = []
    for a, b, name in UNITS:
        if name in SHOP_TRADES:
            S = Shop(B, a, b, name, rnd)
            S.awning()
            {"bakery": _bakery, "records": _records, "books": _books, "hardware": _hardware,
             "salon": _salon, "toyfab": _toyfab}[S.trade](S)
            done.append(S)
        elif name.startswith("core"):
            _lobby(B, rnd, a, b, name)
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
