"""Shell market: a market hall / fair under a 4 x 3 grid of thin concrete "umbrella" shells.

An original design in the spirit of 1950s Mexican concrete-shell architecture (inverted hyperbolic-paraboloid
umbrellas on a single column, drained through the column). Each umbrella is 10 x 10 m, four hypar quadrants
(8 x 8 quads each, straight-line edges: the edge midpoints are low, the corners high, so rain runs to the
centre). The umbrellas stand at slightly different heights (corners 4.7 .. 5.9 m) so open clerestory slots of
light appear between neighbours. Shells are modelled closed: top surface, underside (thicker near the column,
a 9-12 cm edge band) and the edge band all in B.m. Sides are open.

Plot 50 x 40; footprint ~42 x 32 m, height ~6 m. Not walled: the whole hall is enterable (open).
Under it: a paving slab (CT_Paving), 12 stall counters (concrete body + wood top, spot "stall" on the seller side
and "counter" on the buyer side) and 8 benches ("bench"). No flats, no windows, no doors.

Ideas for later: awnings and hanging goods per stall, night lamps in the columns, glass infill in the
clerestory slots, a drain spout visible at each column, a fish/vegetable day switch of the stall props.
Material requests: CT_ConcreteWhite is used on top (painted shell), CT_Concrete for the underside.
"""
import math
import random

import geom
from citykit import box, prism, ring
from _sheet import thick_sheet
import furn_int3 as fu
from furn_int3 import Fr, lbox, cyl, blob
import interior_kit as kit

INFO = {
    "name_ru": "Рынок под оболочками", "name_en": "Shell Market",
    "style": "mexican shell modernism", "use": "public",
    "inspired_by": "1950s Mexican concrete umbrella shells (inverted hypar umbrellas on one column) - original layout and proportions",
}

L = 5.0            # half width of an umbrella
PITCH = 10.4
NX, NY = 4, 3
Y0 = 7.2


def umbrella(B, cx, cy, zc, rnd):
    a = 0.5
    zr = rnd.uniform(1.9, 2.3)               # corner rise over the column top
    b = zr - 2 * a
    for sx in (1, -1):
        for sy in (1, -1):
            def f(u, v, sx=sx, sy=sy):
                return (cx + sx * u * L, cy + sy * v * L, zc + a * (u + v) + b * u * v)

            def thick(u, v):
                r = L * math.hypot(u, v)
                return 0.10 + 0.32 * max(0.0, 1 - r / 2.2) ** 1.5
            thick_sheet(B.m, f, 8, 8, thick, "CT_ConcreteWhite", "CT_Concrete", "CT_ConcreteWhite", edges="UV")
    # column with a flared head (the drain runs inside)
    B.column_round(cx, cy, -0.0, zc - 0.25, 0.28, "CT_Concrete", segs=14)
    B.column_round(cx, cy, zc - 1.0, zc - 0.25, 0.5, "CT_Concrete", segs=14)
    B.column_round(cx, cy, 0.0, 0.35, 0.40, "CT_ConcreteDark", segs=14)
    return zc + zr


def bench(B, x, y, along_x=True):
    if along_x:
        box(B.m, "CT_Wood", x - 0.9, x + 0.9, y - 0.22, y + 0.22, 0.42, 0.48)
        for dx in (-0.75, 0.75):
            box(B.m, "CT_ConcreteDark", x + dx - 0.05, x + dx + 0.05, y - 0.2, y + 0.2, 0, 0.42)
    else:
        box(B.m, "CT_Wood", x - 0.22, x + 0.22, y - 0.9, y + 0.9, 0.42, 0.48)
        for dy in (-0.75, 0.75):
            box(B.m, "CT_ConcreteDark", x - 0.2, x + 0.2, y + dy - 0.05, y + dy + 0.05, 0, 0.42)


def z_under(cx, cy, zc, zr, x, y):
    """Underside of the umbrella shell at (x, y) (the hypar of umbrella()), for hanging lamps."""
    a = 0.5
    b = zr - 2 * a
    u, v = abs(x - cx) / L, abs(y - cy) / L
    r = L * math.hypot(u, v)
    return zc + a * (u + v) + b * u * v - (0.10 + 0.32 * max(0.0, 1 - r / 2.2) ** 1.5)


FISH = ["CT_PlasterBlue", "CT_Steel", "CT_PlasterBlue", "CT_ConcreteDark"]
VEG = ["CT_PlasterRed", "CT_PlasterGreen", "CT_PlasterYellow", "CT_PlasterPink", "CT_PlasterBlue", "CT_SteelRed", "CT_Grass"]
LOAF = ["CT_WoodLight", "CT_PlasterYellow", "CT_PlasterPink", "CT_WoodLight"]
# what each of the 16 counters sells (the shops of tools/city/content/plots.json: shell_market/counter<n>)
KINDS = ["fruit", "dairy", "butcher", "bread", "birds", "fish", "sweets", "bait", "veg", "dairy", "fish", "bread", "birds",
         "fruit", "butcher", "bait"]
SHOPS = {1: ("shop", "market_fruit"), 2: ("shop", "market_dairy"), 3: ("butcher", None), 4: ("shop", "market_bakery"),
         5: ("shop", "market_birds"), 6: ("fishmonger", None), 7: ("shop", "market_sweets"), 8: ("shop", "market_bait"),
         9: ("shop", "market_fruit"), 10: ("shop", "market_dairy"), 11: ("fishmonger", None), 12: ("shop", "market_bakery"),
         13: ("shop", "market_birds"), 14: ("shop", "market_fruit"), 15: ("butcher", None), 16: ("shop", "market_bait")}
FASCIA = {"fruit": "sign_greens", "veg": "sign_greens", "dairy": "sign_milk", "bread": "sign_bread"}     # others: plain
AWN = {"fruit": ("CT_PlasterGreen", "CT_TileWhite"), "veg": ("CT_PlasterGreen", "CT_TileWhite"),
       "dairy": ("CT_PlasterBlue", "CT_TileWhite"), "bread": ("CT_PlasterYellow", "CT_TileWhite"),
       "fish": ("CT_PlasterBlue", "CT_TileWhite"), "butcher": ("CT_SteelRed", "CT_TileWhite"),
       "birds": ("CT_PlasterGreen", "CT_PlasterYellow"), "sweets": ("CT_PlasterPink", "CT_TileWhite"),
       "bait": ("CT_PlasterRed", "CT_TileWhite")}
# the second (third) stall of a trade gets its own awning so no two neighbours look alike
AWN_N = {9: ("CT_PlasterGreen", "CT_PlasterYellow"), 10: ("CT_PlasterYellow", "CT_TileWhite"), 11: ("CT_SteelRed", "CT_PlasterBlue"),
         12: ("CT_PlasterRed", "CT_TileWhite"), 13: ("CT_PlasterBlue", "CT_PlasterYellow"), 14: ("CT_PlasterPink", "CT_TileWhite"),
         15: ("CT_PlasterGreen", "CT_TileWhite"), 16: ("CT_PlasterGreen", "CT_PlasterYellow")}
ZT = 1.03          # counter top


def _fish(d, mat, fr, u, w, z, ang, length=0.3, r=0.04):
    """A fish lying on ice: a round-ish tapered body, a tail fin, a red gill spot."""
    f2 = fr.sub(u, w, ang)
    a, b = f2.p(-length / 2, 0.0, z + r), f2.p(length / 2 - 0.04, 0.0, z + r)
    _tube(d, mat, a, b, r, 10)
    lbox(d, mat, f2, -length / 2 - 0.07, -length / 2, -0.03, 0.03, z + r - 0.012, z + r + 0.012, faces="xXyYZ")
    lbox(d, "CT_SteelRed", f2, length / 2 - 0.045, length / 2 - 0.03, -r * 0.8, r * 0.8, z + r - 0.02, z + r + 0.02, faces="xXyY")


def _mound(m, mat, x, y, z, rx, h, rnd, ry=None, segs=8, yaw=0.0):
    """A lumpy heap (potatoes, grain, a pile of fruit under the fruit): three rings and an apex, no bottom, ~5 * segs tris."""
    ry = ry or rx
    cy_, sy_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    rows = []
    for s_, t_ in ((1.0, 0.0), (0.84, 0.45), (0.48, 0.82)):
        row = []
        for k in range(segs):
            a = 2 * math.pi * (k + 0.5 * (len(rows) % 2)) / segs
            j = 1 + rnd.uniform(-0.1, 0.1)
            ex, ey = rx * s_ * j * math.cos(a), ry * s_ * j * math.sin(a)
            row.append((x + ex * cy_ - ey * sy_, y + ex * sy_ + ey * cy_, z + h * t_ * (1 + rnd.uniform(-0.08, 0.08))))
        rows.append(row)
    for r in range(2):
        for k in range(segs):
            k2 = (k + 1) % segs
            m.face(mat, [rows[r][k], rows[r][k2], rows[r + 1][k2]])
            m.face(mat, [rows[r][k], rows[r + 1][k2], rows[r + 1][k]])
    apex = (x, y, z + h)
    for k in range(segs):
        m.face(mat, [rows[2][k], rows[2][(k + 1) % segs], apex])


def _fruit_pile(B, x, y, z, rnd, mats, r=0.05, yaw=0.0):
    """A tray with a heap of fruit: a lumpy mound and seven fruits sitting on it (a real apple's size)."""
    d = B.detail
    fr = Fr(x, y, yaw)
    lbox(d, "CT_WoodLight", fr, -0.25, 0.25, -0.18, 0.18, z, z + 0.04, faces="xXyYZ")
    _mound(d, mats[0], x, y, z + 0.04, 0.2, 0.1, rnd, ry=0.14, yaw=yaw)
    for k in range(7):
        a = 2 * math.pi * k / 7 + rnd.uniform(-0.3, 0.3)
        rr = 0.03 + 0.1 * (k % 2) * rnd.uniform(0.6, 1.0)
        px, py, _ = fr.p(rr * math.cos(a) * 1.3, rr * math.sin(a))
        blob(d, mats[k % len(mats)], px, py, z + 0.04 + 0.1 * (1 - rr / 0.2) + 0.03, r, 5)


def _loaf(d, mat, fr, u, w, z, length=0.26, r=0.06):
    """A loaf: a rounded body lying along u."""
    _tube(d, mat, fr.p(u - length / 2, w, z + r), fr.p(u + length / 2, w, z + r), r, 10)


def _seed_sack(B, x, y, z, mat):
    """An open sack of seed, rolled down at the mouth: 0.26 m wide, 0.32 m high."""
    cyl(B.detail, mat, x, y, z, z + 0.3, 0.13, 10, top=False, bottom=True)
    cyl(B.detail, mat, x, y, z + 0.26, z + 0.32, 0.145, 10, top=False)
    cyl(B.detail, "CT_WoodLight", x, y, z + 0.28, z + 0.285, 0.12, 10, top=True)


def stall_goods(B, fr, kind, rnd, n):
    d = B.detail
    if kind == "fish":
        lbox(d, "CT_Steel", fr, -1.4, 1.4, -0.36, 0.36, ZT, ZT + 0.05)
        for (u0, u1, w0, w1) in ((-1.4, 1.4, -0.36, -0.32), (-1.4, 1.4, 0.32, 0.36), (-1.4, -1.36, -0.36, 0.36), (1.36, 1.4, -0.36, 0.36)):
            lbox(d, "CT_Steel", fr, u0, u1, w0, w1, ZT + 0.05, ZT + 0.12)
        lbox(d, "CT_TileWhite", fr, -1.34, 1.34, -0.31, 0.31, ZT + 0.05, ZT + 0.11)
        for k in range(12):
            u, w = -1.2 + 0.22 * k + rnd.uniform(-0.04, 0.04), rnd.uniform(-0.2, 0.2)
            _fish(d, rnd.choice(FISH), fr, u, w, ZT + 0.11, rnd.uniform(-30, 30) + (90 if k % 2 else 0))
        for k in range(2):     # tubs of ice on the floor behind
            px, py, _ = fr.p(-1.1 + 2.2 * k, -0.85)
            fu.bucket(B, px, py, 0.0, 0.24, 0.45, "CT_Steel")
    elif kind in ("veg", "fruit"):
        if kind == "veg":
            for k in range(5):
                u = -1.2 + 0.6 * k
                c = rnd.choice(VEG)
                px, py, _ = fr.p(u, 0.0)
                fu.crate(B, px, py, ZT, 0.5, 0.5, 0.16, "CT_WoodLight", fill=c, yaw=fr.yaw)
                px2, py2, _ = fr.p(u, 0.0)
                _mound(d, c, px2, py2, ZT + 0.12, 0.2, 0.11, rnd, yaw=fr.yaw)
                for j in range(2):
                    qx, qy, _ = fr.p(u + rnd.uniform(-0.1, 0.1), rnd.uniform(-0.1, 0.1))
                    blob(d, c, qx, qy, ZT + 0.25, 0.06, 5)
        else:
            for k in range(3):
                px, py, _ = fr.p(-0.95 + 0.95 * k, 0.0)
                _fruit_pile(B, px, py, ZT, rnd, ["CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterGreen"] if k != 1 else ["CT_PlasterYellow", "CT_SteelRed"], yaw=fr.yaw)
            for k in range(2):
                px, py, _ = fr.p(-1.4 + 2.8 * k, 0.0)
                fu.crate(B, px, py, ZT, 0.4, 0.45, 0.12, "CT_WoodLight", fill=rnd.choice(VEG), yaw=fr.yaw)
        for k in range(2):     # crates on the floor behind the seller
            px, py, _ = fr.p(-1.05 + 2.1 * k, -0.9)
            fu.crate(B, px, py, 0.0, 0.5, 0.5, 0.25, "CT_WoodLight", fill=rnd.choice(VEG), yaw=fr.yaw)
            fu.crate(B, px, py, 0.25, 0.5, 0.5, 0.25, "CT_WoodLight", fill=rnd.choice(VEG), yaw=fr.yaw)
    elif kind == "bread":
        for k in range(2):      # a two-tier wooden rack of loaves
            for t in range(2):
                lbox(d, "CT_WoodLight", fr.sub(-0.7 + 1.4 * k, 0.1), -0.55, 0.55, -0.2, 0.2, ZT + 0.3 * t + 0.1, ZT + 0.3 * t + 0.13)
                for j in range(4):
                    _loaf(d, rnd.choice(LOAF), fr.sub(-0.7 + 1.4 * k + (j - 1.5) * 0.24, 0.1, 90), 0.0, 0.0, ZT + 0.3 * t + 0.13, length=0.22)
            for u in (-0.55, 0.55):
                lbox(d, "CT_WoodLight", fr.sub(-0.7 + 1.4 * k + u, 0.1), -0.02, 0.02, -0.2, 0.2, ZT, ZT + 0.42)
        px, py, _ = fr.p(0.0, -0.05)
        fu.crate(B, px, py, ZT, 0.45, 0.38, 0.12, "CT_WoodLight", fill="CT_PlasterYellow", yaw=fr.yaw)
        for k in range(3):     # trays of loaves on a rack behind
            lbox(d, "CT_Steel", fr, -1.2 + 1.2 * k, -0.4 + 1.2 * k, -1.0, -0.7, 0.55 + 0.4 * (k % 2), 0.58 + 0.4 * (k % 2))
            for j in range(3):
                _loaf(d, rnd.choice(LOAF), fr, -1.03 + 1.2 * k + j * 0.25, -0.85, 0.58 + 0.4 * (k % 2), length=0.22)
    elif kind == "flowers":
        for k in range(6):
            px, py, _ = fr.p(-1.25 + 0.5 * k, rnd.uniform(-0.1, 0.1))
            fu.bucket(B, px, py, ZT, 0.13, 0.24, rnd.choice(("CT_Steel", "CT_PlasterBlue", "CT_TileWhite")), fu.FLOWERS, rnd)
    elif kind == "dairy":
        for k in range(3):     # milk cans with lids, on the floor and the counter
            px, py, _ = fr.p(-1.3 + 0.38 * k, -0.9)
            cyl(d, "CT_Steel", px, py, 0.0, 0.55, 0.14, 12, top=True)
            cyl(d, "CT_PlasterBlue", px, py, 0.55, 0.6, 0.15, 12, top=True)
        for k in range(4):     # cheese wheels, stacked
            px, py, _ = fr.p(-1.05 + 0.5 * (k % 2), 0.1 + 0.0 * k)
            for t in range(2 + k % 2):
                cyl(d, "CT_PlasterYellow", px + 0.9 * (k // 2), py, ZT + 0.1 * t, ZT + 0.1 * t + 0.09, 0.2 - 0.02 * t, 16, top=True)
        for k in range(3):
            px, py, _ = fr.p(0.3 + 0.35 * k, -0.05)
            cyl(d, "CT_TileWhite", px, py, ZT, ZT + 0.22, 0.07, 6, top=True)
            cyl(d, "CT_PlasterBlue", px, py, ZT + 0.22, ZT + 0.25, 0.072, 6, top=True)
        px, py, _ = fr.p(1.2, 0.15)
        fu.crate(B, px, py, ZT, 0.42, 0.34, 0.2, "CT_WoodLight", fill="CT_TileWhite", yaw=fr.yaw)
    elif kind == "butcher":
        lbox(d, "CT_WoodLight", fr, -0.5, 0.5, -0.25, 0.25, ZT, ZT + 0.12)                       # block
        lbox(d, "CT_Steel", fr, 0.0, 0.25, 0.05, 0.08, ZT + 0.12, ZT + 0.14)                     # cleaver
        lbox(d, "CT_TileWhite", fr, -1.35, -0.6, -0.3, 0.3, ZT, ZT + 0.04)
        for k in range(5):
            lbox(d, ("CT_SteelRed", "CT_PlasterPink", "CT_SteelRed")[k % 3], fr.sub(-1.05 + 0.15 * k, 0.0, 20 * k - 40),
                 -0.1, 0.1, -0.07, 0.07, ZT + 0.04, ZT + 0.11, faces="xXyYZ")
        lbox(d, "CT_TileWhite", fr, 0.7, 1.4, -0.3, 0.3, ZT, ZT + 0.04)
        for k in range(4):
            lbox(d, ("CT_PlasterPink", "CT_SteelRed")[k % 2], fr.sub(0.85 + 0.17 * k, 0.0, 15 * k), -0.08, 0.08, -0.12, 0.12,
                 ZT + 0.04, ZT + 0.1, faces="xXyYZ")
        # hanging rail with sausages and a carcass behind the counter
        for u in (-1.5, 1.5):
            lbox(d, "CT_Steel", fr, u - 0.02, u + 0.02, -0.62, -0.58, 0.0, 2.0, faces="xXyY")
        lbox(d, "CT_Steel", fr, -1.55, 1.55, -0.62, -0.58, 1.95, 1.99)
        for k in range(9):
            u = -1.2 + 0.3 * k
            f2 = fr.sub(u, -0.6)
            _tube(d, ("CT_SteelRed", "CT_WoodLight", "CT_SteelRed")[k % 3], f2.p(0.0, 0.0, 1.32 - 0.1 * (k % 2)), f2.p(0.0, 0.0, 1.93),
                  0.035, 10)
        hexa_u = fr.sub(0.0, -0.6)
        lbox(d, "CT_PlasterPink", hexa_u, -0.17, 0.17, -0.1, 0.1, 1.05, 1.9, faces="xXyYz")
        lbox(d, "CT_SteelRed", hexa_u, -0.1, 0.1, -0.105, 0.105, 1.3, 1.6, faces="y")
    elif kind == "birds":
        for k in range(3):     # open sacks of seed on the counter
            px, py, _ = fr.p(-1.15 + 0.5 * k, 0.05)
            _seed_sack(B, px, py, ZT, "CT_Paper" if k else "CT_PlasterYellow")
        for k in range(3):     # bowls of seeds
            px, py, _ = fr.p(0.1 + 0.32 * k, 0.12)
            cyl(d, "CT_PlasterRed", px, py, ZT, ZT + 0.08, 0.1, 8, top=True)
            cyl(d, ("CT_PlasterYellow", "CT_WoodLight", "CT_PlasterGreen")[k], px, py, ZT + 0.06, ZT + 0.1, 0.085, 8, top=True)
        for k in range(4):     # feeders on a shelf behind
            px, py, _ = fr.p(-1.1 + 0.7 * k, -0.65)
            _feeder(B, px, py, 1.25, rnd.randint(0, 99))
        lbox(d, "CT_WoodLight", fr, -1.45, 1.45, -0.8, -0.5, 1.2, 1.24)
        for u in (-1.45, 1.45):
            lbox(d, "CT_WoodLight", fr, u - 0.02, u + 0.02, -0.8, -0.76, 0.0, 1.24, faces="xXyY")
        for k in range(2):
            px, py, _ = fr.p(-1.2 + 2.4 * k, -0.85)
            kit.sack(B, px, py, 0.0, rnd.uniform(0, 360))
    elif kind == "sweets":
        for k in range(5):     # jars of sweets with coloured lids
            px, py, _ = fr.p(-1.2 + 0.32 * k, 0.05)
            cyl(d, ("CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterPink", "CT_PlasterGreen", "CT_PlasterBlue")[k], px, py, ZT, ZT + 0.2,
                0.09, 8, top=True)
            cyl(d, "CT_SteelRed" if k % 2 else "CT_Steel", px, py, ZT + 0.2, ZT + 0.24, 0.092, 8, top=True)
        for k in range(3):     # trays of biscuits
            lbox(d, "CT_Steel", fr, 0.55 + 0.28 * k, 0.8 + 0.28 * k, -0.2, 0.2, ZT, ZT + 0.03)
            lbox(d, rnd.choice(LOAF), fr, 0.57 + 0.28 * k, 0.78 + 0.28 * k, -0.17, 0.17, ZT + 0.03, ZT + 0.08, faces="xXyYZ")
        for k in range(6):     # lollipops in a stand
            px, py, _ = fr.p(-1.35 + 0.0, -0.25 + 0.1 * k)
            blob(d, rnd.choice(FLOWERS_), px, py, ZT + 0.32, 0.05, 6)
            lbox(d, "CT_Paper", Fr(px, py), -0.004, 0.004, -0.004, 0.004, ZT + 0.05, ZT + 0.3, faces="xXyY")
        lbox(d, "CT_WoodLight", fr, -1.4, -1.3, -0.3, 0.0, ZT, ZT + 0.05)
        for k in range(3):     # boxes of chocolates on a stack
            lbox(d, ("CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow")[k], fr, 0.0, 0.36, 0.1, 0.34, ZT + 0.0 + 0.06 * k,
                 ZT + 0.06 * (k + 1), faces="xXyYZ")
    elif kind == "bait":
        for k in range(4):     # tubs with lids, worm and maggot boxes
            px, py, _ = fr.p(-1.25 + 0.4 * k, 0.05)
            cyl(d, ("CT_PlasterGreen", "CT_PlasterBlue", "CT_PlasterGreen", "CT_PlasterRed")[k], px, py, ZT, ZT + 0.16, 0.11, 8, top=True)
            cyl(d, "CT_TileWhite", px, py, ZT + 0.16, ZT + 0.18, 0.115, 8, top=True)
        for k in range(3):
            lbox(d, ("CT_Steel", "CT_PlasterYellow", "CT_Steel")[k], fr.sub(0.4 + 0.35 * k, 0.0, 10 * k), -0.14, 0.14, -0.1, 0.1,
                 ZT, ZT + 0.07, faces="xXyYZ")
        for k in range(5):     # rods leaning behind, a net, floats on a board
            a = fr.p(-1.2 + 0.55 * k, -0.7, 0.0)
            b = fr.p(-1.2 + 0.55 * k + 0.15, -0.72, 2.0)
            fu.rod(d, ("CT_WoodLight", "CT_PlasterGreen", "CT_Wood")[k % 3], a, b, 0.02, caps=True)
        px, py, _ = fr.p(1.4, -0.8)
        fu.bucket(B, px, py, 0.0, 0.2, 0.34, "CT_PlasterBlue")
        lbox(d, "CT_PlasterGreen", fr, 1.0, 1.35, -0.2, 0.2, ZT, ZT + 0.12, faces="xXyYZ")
    # scale on a post at the right end of the counter (everything sells by weight)
    if kind in ("fish", "veg", "fruit", "dairy", "butcher"):
        post = fr.sub(1.38, 0.28)
        lbox(d, "CT_Steel", post, -0.02, 0.02, -0.02, 0.02, ZT, ZT + 0.85)
        lbox(d, "CT_Steel", post, -0.7, 0.04, -0.02, 0.02, ZT + 0.85, ZT + 0.89)
        lbox(d, "CT_SteelRed", post, -0.74, -0.66, -0.03, 0.03, ZT + 0.62, ZT + 0.85)
        cyl(d, "CT_Aluminium", *post.p(-0.7, 0.0)[:2], ZT + 0.5, ZT + 0.53, 0.14, 8)
        lbox(d, "CT_Steel", post.sub(-0.7, 0.0), -0.004, 0.004, -0.004, 0.004, ZT + 0.53, ZT + 0.85, faces="xXyY")
    else:
        x_, y_, _ = fr.p(1.25, 0.25)
        kit.kitchen_scales(B, x_, y_, ZT, fr.yaw)
    # a price board standing on the counter's customer edge (atlas price list) and a cash tin
    bx, by, _ = fr.p(-1.15, 0.3)
    if kind in ("fruit", "veg", "bread"):
        kit.menu_board(B, bx, by, ZT, fr.yaw, name="price_1" if kind == "bread" else "price_0", w=0.4, zc=0.3, seed=n)
    else:                      # a plain chalk slate with a few lines of chalk
        f2 = fr.sub(-1.15, 0.3)
        lbox(d, "CT_WoodLight", f2, -0.2, 0.2, -0.03, 0.03, ZT + 0.0, ZT + 0.46, faces="xXyYZ")
        lbox(d, "CT_ConcreteDark", f2, -0.17, 0.17, 0.03, 0.035, ZT + 0.04, ZT + 0.42, faces="y")
        for q in range(4):
            lbox(d, "CT_TileWhite", f2, -0.13, 0.13 - 0.05 * (q % 3), 0.035, 0.038, ZT + 0.1 + 0.08 * q, ZT + 0.115 + 0.08 * q, faces="y")
    lbox(d, "CT_WoodLight", fr.sub(-1.15, 0.28), -0.18, 0.18, -0.02, 0.03, ZT, ZT + 0.03)


FLOWERS_ = ["CT_PlasterRed", "CT_PlasterPink", "CT_PlasterYellow", "CT_PlasterBlue", "CT_PlasterGreen"]


def _feeder(B, x, y, z, seed=0):
    r = random.Random(seed)
    d = B.detail
    mat = r.choice(("CT_WoodLight", "CT_PlasterGreen", "CT_PlasterRed", "CT_Wood"))
    box(d, mat, x - 0.11, x + 0.11, y - 0.09, y + 0.09, z + 0.04, z + 0.2)
    prism(d, "CT_PlasterRed", [(x - 0.15, y - 0.12), (x + 0.15, y - 0.12), (x + 0.15, y + 0.12), (x - 0.15, y + 0.12)],
          z + 0.2, z + 0.24, top=True)


def awning(B, x, y, yaw, kind, n=0):
    """A striped awning over a stall, on two posts behind the counter, struts to the front edge (the lamp is above it)."""
    fr = Fr(x, y, yaw)
    mats = AWN_N.get(n) or AWN[kind]
    fu.awning(B, fr.sub(0.0, -0.62), -1.7, 1.7, 1.7, 2.4, 2.08, stripe=0.57, mats=mats)
    for u in (-1.65, 1.65):
        lbox(B.detail, "CT_Steel", fr, u - 0.025, u + 0.025, -0.66, -0.6, 0.0, 2.4, faces="xXyY")
        a, b = fr.p(u, -0.63, 2.05), fr.p(u, 0.9, 1.88)
        fu.rod(B.detail, "CT_Steel", a, b, 0.025, caps=True)
        lbox(B.detail, "CT_Steel", fr, u - 0.015, u + 0.015, 0.88, 0.92, 0.0, 1.86, faces="xXyY")
    # fascia with a sign on the back posts
    bx, by, _ = fr.p(0.0, -0.66)
    if kind in FASCIA:
        kit.sign_plate(B, bx, by, 0.0, yaw, name=FASCIA[kind], w=1.5, zc=2.1)
    else:
        lbox(B.detail, mats[0], fr, -0.75, 0.75, -0.67, -0.64, 1.9, 2.3, faces="xXyYZz")
        lbox(B.detail, "CT_TileWhite", fr, -0.7, 0.7, -0.675, -0.67, 1.95, 2.25, faces="y")


def _directory(B, x, y, yaw, w=1.3):
    d = B.detail
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    at = lambda u, v: (x + u * c - v * s, y + u * s + v * c)
    for sx in (-w / 2 + 0.05, w / 2 - 0.05):
        px, py = at(sx, 0.0)
        box(d, "CT_Steel", px - 0.025, px + 0.025, py - 0.025, py + 0.025, 0.0, 2.0, faces="xXyYZ")
    kit.notice_board(B, *at(0.0, -0.05), 0.0, yaw, w=w, zc=1.45, seed=int(x))
    kit.notice_board(B, *at(0.0, 0.05), 0.0, yaw + 180, w=w, zc=1.45, seed=int(x) + 1)


# ------------------------------------------------------------------------------------------------ second pass: depth
def _tube(m, mat, a, b, r, segs=8, caps=True):
    """A round bar (n-gon section) from point a to point b, faces turned outward (wheels, hoops, bodies, barrels on their side)."""
    d = geom.norm(geom.sub(b, a))
    ref = (0, 0, 1) if abs(d[2]) < 0.9 else (1, 0, 0)
    u = geom.norm(geom.cross(d, ref))
    v = geom.cross(d, u)
    cen = tuple((a[i] + b[i]) / 2 for i in range(3))

    def P(p, k):
        t = 2 * math.pi * k / segs
        return tuple(p[i] + r * (u[i] * math.cos(t) + v[i] * math.sin(t)) for i in range(3))
    ra = [P(a, k) for k in range(segs)]
    rb = [P(b, k) for k in range(segs)]
    quads = [[ra[k], ra[(k + 1) % segs], rb[(k + 1) % segs], rb[k]] for k in range(segs)]
    if caps:
        quads += [ra[::-1], rb]
    for q in quads:
        n = geom.cross(geom.sub(q[1], q[0]), geom.sub(q[2], q[0]))
        mid = tuple(sum(p[i] for p in q) / len(q) for i in range(3))
        if sum(n[i] * (mid[i] - cen[i]) for i in range(3)) < 0:
            q = q[::-1]
        m.face(mat, q)


def _barrel(B, x, y, z, r, h, mat="CT_Wood", fill=None):
    cyl(B.detail, mat, x, y, z, z + h, r, 8, top=fill is None, bottom=False)
    for t in (0.2, 0.75):
        cyl(B.detail, "CT_Steel", x, y, z + h * t, z + h * t + 0.025, r + 0.008, 8, top=False)
    if fill:
        cyl(B.detail, fill, x, y, z + h - 0.04, z + h - 0.02, r - 0.02, 10, top=True)


def _basket(B, x, y, z, r, h, fill):
    cyl(B.detail, "CT_WoodLight", x, y, z, z + h, r, 8, top=False, bottom=True)
    cyl(B.detail, fill, x, y, z + h * 0.6, z + h * 0.95, r - 0.015, 8, top=True)


def _shelf_jars(B, fr, u0, u1, w, z, mats, r=0.055, h=0.15, posts=True):
    """A plank on two posts behind a counter with a row of lidded jars on it."""
    d = B.detail
    lbox(d, "CT_WoodLight", fr, u0 - 0.04, u1 + 0.04, w - 0.12, w + 0.12, z - 0.03, z)
    if posts:
        for u in (u0 - 0.03, u1 + 0.03):
            lbox(d, "CT_Wood", fr, u - 0.02, u + 0.02, w - 0.02, w + 0.02, 0.0, z - 0.03, faces="xXyY")
    n = max(2, int((u1 - u0) / (2 * r + 0.17)) + 1)
    for k in range(n):
        x, y, _ = fr.p(u0 + k * (u1 - u0) / (n - 1), w)
        cyl(d, mats[k % len(mats)], x, y, z, z + h, r, 6, top=False)
        cyl(d, "CT_Steel" if k % 3 else "CT_SteelRed", x, y, z + h, z + h + 0.025, r * 1.04, 6, top=True)


def _braid(B, fr, u, w, z_top, n, mat, length=0.5):
    """A hanging braid (garlic, onions, dried mushrooms): a cord and n blobs on it."""
    d = B.detail
    lbox(d, "CT_Paper", fr, u - 0.006, u + 0.006, w - 0.006, w + 0.006, z_top - length, z_top, faces="xXyY")
    for k in range(n):
        x, y, _ = fr.p(u + (0.035 if k % 2 else -0.035), w)
        blob(d, mat, x, y, z_top - 0.06 - k * length / n, 0.055, 6)


# atlas goods labels (label_NN = GOODS[NN % 30] of tools/city/atlas.py) that fit each trade, so a dairy stall never hangs "glue"
CARD_GOODS = {"fruit": (3, 10, 25, 26, 27), "veg": (2, 3, 25, 26, 27), "dairy": (4, 23, 24), "butcher": (8, 27),
              "bread": (7, 9, 12, 13), "birds": (7, 12), "fish": (0, 1, 8, 28), "sweets": (9, 10, 11, 13, 14, 29),
              "bait": (12, 15)}


def _price_cards(B, fr, rnd, z=1.72, kind=None):
    """Little price cards on a string along the awning's front edge."""
    d = B.detail
    lbox(d, "CT_Paper", fr, -1.62, 1.62, 0.9, 0.915, z + 0.13, z + 0.145, faces="xXyYZz")
    for k, u in enumerate((-1.2, -0.45, 0.35, 1.1)):
        x, y, _ = fr.p(u + rnd.uniform(-0.1, 0.1), 0.905)
        goods = CARD_GOODS.get(kind)
        cell = rnd.randint(0, 59) if not goods else goods[(k + rnd.randint(0, 9)) % len(goods)] + 30 * rnd.randint(0, 1)
        kit.poster(B, x, y, 0.0, fr.yaw, name="label_%02d" % cell, w=0.2, h=0.14, zc=z + 0.05)


def _knives(B, fr, u, w, z):
    """A strip with knives on the post behind the butcher's counter."""
    d = B.detail
    lbox(d, "CT_WoodLight", fr, u - 0.4, u + 0.4, w - 0.02, w + 0.03, z, z + 0.05)
    for k in range(6):
        uu = u - 0.33 + 0.13 * k
        lbox(d, "CT_Steel", fr, uu - 0.012, uu + 0.012, w + 0.03, w + 0.036, z - 0.12 - 0.05 * (k % 3), z, faces="xXyYz")
        lbox(d, "CT_Wood", fr, uu - 0.014, uu + 0.014, w + 0.03, w + 0.045, z + 0.0, z + 0.09, faces="xXyYZ")


def _steelyard(B, fr, u, w, z, mat="CT_Steel"):
    """A hanging steelyard (a beam, a hook, a pan on chains and a sliding weight) from the awning."""
    d = B.detail
    a, b = fr.p(u, w, z + 0.45), fr.p(u, w, z + 0.18)
    fu.rod(d, "CT_Steel", a, b, 0.012)
    fu.rod(d, mat, fr.p(u - 0.34, w, z + 0.15), fr.p(u + 0.34, w, z + 0.15), 0.02)
    x, y, _ = fr.p(u + 0.18, w)
    cyl(d, "CT_SteelRed", x, y, z + 0.08, z + 0.15, 0.03, 6, top=True)
    for s in (-1, 1):
        fu.rod(d, "CT_Steel", fr.p(u - 0.34, w, z + 0.15), fr.p(u - 0.34 + 0.06 * s, w, z - 0.18), 0.005)
    x, y, _ = fr.p(u - 0.34, w)
    cyl(d, "CT_Aluminium", x, y, z - 0.22, z - 0.19, 0.11, 8, top=True)


def _birdhouse(B, fr, u, w, z, mat):
    d = B.detail
    lbox(d, "CT_Paper", fr, u - 0.006, u + 0.006, w - 0.006, w + 0.006, z, z + 0.3, faces="xXyY")
    lbox(d, mat, fr, u - 0.09, u + 0.09, w - 0.08, w + 0.08, z - 0.2, z, faces="xXyYZz")
    lbox(d, "CT_ConcreteDark", fr, u - 0.03, u + 0.03, w + 0.08, w + 0.085, z - 0.12, z - 0.06, faces="y")
    lbox(d, "CT_PlasterRed", fr, u - 0.12, u + 0.12, w - 0.1, w + 0.1, z, z + 0.04, faces="xXyYZz")
    lbox(d, "CT_WoodLight", fr, u - 0.012, u + 0.012, w + 0.085, w + 0.12, z - 0.17, z - 0.155, faces="xXyYZz")


def _cage(B, fr, u, w, z, mat="CT_Steel"):
    d = B.detail
    x, y, _ = fr.p(u, w)
    cyl(d, "CT_PlasterGreen", x, y, z, z + 0.03, 0.17, 10, top=True, bottom=True)
    for k in range(10):
        t = 2 * math.pi * k / 10
        lbox(d, mat, fr, u + 0.165 * math.cos(t) - 0.004, u + 0.165 * math.cos(t) + 0.004, w + 0.165 * math.sin(t) - 0.004,
             w + 0.165 * math.sin(t) + 0.004, z + 0.03, z + 0.34, faces="xXyY")
    cyl(d, "CT_PlasterGreen", x, y, z + 0.34, z + 0.37, 0.17, 10, top=True)
    lbox(d, "CT_PlasterYellow", fr, u - 0.03, u + 0.03, w - 0.03, w + 0.03, z + 0.12, z + 0.18, faces="xXyYZz")


def _net(B, fr, u0, u1, w, z0, z1, mat="CT_PlasterGreen"):
    """A fishing net draped on a post: a thin mesh drawn as crossing cords."""
    d = B.detail
    for k in range(int((u1 - u0) / 0.12) + 1):
        u = u0 + k * 0.12
        lbox(d, mat, fr, u - 0.004, u + 0.004, w, w + 0.01, z0 + 0.03 * (k % 3), z1, faces="xXyY")
    for q in range(int((z1 - z0) / 0.14) + 1):
        zz = z0 + 0.14 * q
        lbox(d, mat, fr, u0, u1, w, w + 0.01, zz, zz + 0.008, faces="yYZ")


def _smoked_fish_rail(B, fr, w, z, rnd, n=8):
    d = B.detail
    lbox(d, "CT_Wood", fr, -1.45, 1.45, w - 0.02, w + 0.02, z, z + 0.04)
    for u in (-1.45, 1.45):
        lbox(d, "CT_Wood", fr, u - 0.025, u + 0.025, w - 0.025, w + 0.025, 0.0, z + 0.04, faces="xXyY")
    for k in range(n):
        u = -1.2 + k * 2.4 / (n - 1)
        for s in (-0.03, 0.03):
            lbox(d, ("CT_PlasterYellow", "CT_WoodLight")[k % 2], fr.sub(u + s, w, 0), -0.025, 0.025, -0.012, 0.012, z - 0.28, z,
                 faces="xXyYz")


def _hex_box(B, fr, u, w, z, mat):
    lbox(B.detail, mat, fr, u - 0.14, u + 0.14, w - 0.1, w + 0.1, z, z + 0.1, faces="xXyYZ")


def stall_extra(B, fr, n, kind, rg):
    """The second pass of a stall's dressing: what makes this very stall different from the other of its trade."""
    rx = random.Random(n * 31 + 7)
    d = B.detail
    _price_cards(B, fr, rx, kind=kind)
    if n == 1:      # onions and garlic in braids, a basket of beets
        for k in range(3):
            _braid(B, fr, -1.35 + 0.28 * k, -0.6, 1.95, 3, ("CT_PlasterYellow", "CT_TileWhite", "CT_PlasterRed")[k])
    elif n == 9:    # pumpkins and a barrel of cabbage
        for k in range(4):
            px, py, _ = fr.p(-1.35 + 0.3 * k, -0.95 + 0.1 * (k % 2))
            blob(d, "CT_PlasterYellow", px, py, 0.16, 0.17, 6)
        px, py, _ = fr.p(1.3, -0.95)
        _barrel(B, px, py, 0.0, 0.26, 0.6, fill="CT_Leaf")
        for k in range(3):
            px, py, _ = fr.p(0.2 + 0.28 * k, -0.9)
            blob(d, "CT_PlasterGreen", px, py, 0.12, 0.12, 6)
    elif n == 14:   # the jam stall: shelf of jars with ribbons, pots of honey
        _shelf_jars(B, fr, -1.2, 1.2, -0.7, 1.15, ("CT_PlasterRed", "CT_SteelRed", "CT_PlasterYellow", "CT_PlasterGreen", "CT_PlasterPink"), posts=True)
        for k in range(3):
            px, py, _ = fr.p(-1.0 + 0.25 * k, 0.12)
            cyl(d, "CT_PlasterRed", px, py, ZT, ZT + 0.12, 0.07, 8, top=True)
    elif n == 2:    # eggs in trays, butter blocks
        for k in range(3):
            lbox(d, "CT_Paper", fr, 0.55, 0.95, -0.28 + 0.2 * k, -0.1 + 0.2 * k, ZT, ZT + 0.03)
            for j in range(3):
                lbox(d, "CT_TileWhite", fr, 0.6 + 0.12 * j, 0.68 + 0.12 * j, -0.24 + 0.2 * k, -0.14 + 0.2 * k, ZT + 0.03, ZT + 0.07, faces="xXyYZ")
        lbox(d, "CT_PlasterYellow", fr, -1.4, -1.0, 0.0, 0.2, ZT, ZT + 0.07, faces="xXyYZ")
    elif n == 10:   # honey: amber jars on two shelves, a wax-and-frame box
        _shelf_jars(B, fr, -1.25, 1.25, -0.7, 1.2, ("CT_WoodLight", "CT_PlasterYellow", "CT_WoodLight", "CT_PlasterYellow", "CT_Wood"), r=0.06, h=0.16)
        px, py, _ = fr.p(1.15, -0.95)
        lbox(d, "CT_WoodLight", fr.sub(1.15, -0.95), -0.25, 0.25, -0.2, 0.2, 0.0, 0.5, faces="xXyYZ")
        lbox(d, "CT_Wood", fr.sub(1.15, -0.95), -0.27, 0.27, -0.22, 0.22, 0.5, 0.54, faces="xXyYZz")
    elif n == 3:    # the old butcher: knives, a block of logs, an apron on a hook
        _knives(B, fr, 0.0, -0.64, 1.45)
        px, py, _ = fr.p(-1.3, -0.95)
        cyl(d, "CT_WoodLight", px, py, 0.0, 0.5, 0.2, 10, top=True)
        lbox(d, "CT_TileWhite", fr, 1.2, 1.5, -0.66, -0.6, 1.0, 1.5, faces="xXyYz")
    elif n == 15:   # the deli: strings of sausages, a brine barrel, pickle jars
        d2 = d
        lbox(d2, "CT_Wood", fr, -1.45, 1.45, -0.7, -0.66, 1.85, 1.89)
        for k in range(6):
            u = -1.2 + 0.5 * k
            fu.rod(d2, "CT_SteelRed" if k % 2 else "CT_PlasterPink", fr.p(u, -0.68, 1.85), fr.p(u + 0.12, -0.68, 1.55), 0.045)
            fu.rod(d2, "CT_SteelRed" if k % 2 else "CT_PlasterPink", fr.p(u + 0.12, -0.68, 1.55), fr.p(u + 0.24, -0.68, 1.85), 0.045)
        px, py, _ = fr.p(1.25, -0.95)
        _barrel(B, px, py, 0.0, 0.26, 0.62, fill="CT_PlasterGreen")
        _shelf_jars(B, fr, -1.3, -0.3, -0.7, 0.9, ("CT_PlasterGreen", "CT_PlasterYellow", "CT_PlasterGreen"), posts=False)
    elif n == 4:    # flour sacks by the oven and a basket of rolls
        for k in range(3):
            px, py, _ = fr.p(1.1 + 0.3 * (k % 2), -1.0 + 0.0)
            kit.sack(B, px, py, 0.0 + 0.22 * (k // 2), rx.uniform(0, 360), mat="CT_Paper")
        for k in range(2):
            px, py, _ = fr.p(-1.35 + 0.4 * k, 0.3)
            _basket(B, px, py, ZT, 0.14, 0.14, "CT_PlasterYellow")
    elif n == 12:   # pies and a baker's board with cloth, small round loaves
        for k in range(3):
            px, py, _ = fr.p(-1.4 + 0.45 * k, 0.28)
            cyl(d, "CT_Steel", px, py, ZT, ZT + 0.025, 0.17, 10, top=True)
            cyl(d, ("CT_PlasterYellow", "CT_WoodLight", "CT_PlasterRed")[k], px, py, ZT + 0.025, ZT + 0.09, 0.15, 10, top=True)
        lbox(d, "CT_PlasterRed", fr, 0.9, 1.5, 0.15, 0.4, ZT, ZT + 0.015, faces="xXyYZ")
        px, py, _ = fr.p(-1.35, -0.95)
        _basket(B, px, py, 0.0, 0.25, 0.35, "CT_WoodLight")
    elif n == 5:    # birdhouses on strings, a sack of sunflower seeds
        for k in range(2):
            _birdhouse(B, fr, -0.8 + 1.6 * k, 0.85, 1.95, ("CT_WoodLight", "CT_PlasterBlue", "CT_Wood")[k])
    elif n == 13:   # cages and feeders standing on the floor
        _cage(B, fr, -1.2, -0.9, 0.0)
        px, py, _ = fr.p(0.0, -0.9)
        kit.bird_feeder(B, px, py, 0.0, fr.yaw, kind="post")
        _birdhouse(B, fr, 0.9, 0.85, 1.9, "CT_PlasterGreen")
    elif n == 6:    # the day's catch: a steelyard on a hook and tubs of ice
        _steelyard(B, fr, -0.3, 0.85, 1.55)
        px, py, _ = fr.p(0.0, -0.9)
        _barrel(B, px, py, 0.0, 0.22, 0.45, mat="CT_Steel", fill="CT_TileWhite")
    elif n == 11:   # smoked fish on a rail and a barrel of salted herring
        _smoked_fish_rail(B, fr, -0.6, 1.72, rx)
        px, py, _ = fr.p(0.0, -1.0)
        _barrel(B, px, py, 0.0, 0.26, 0.6, fill="CT_PlasterBlue")
    elif n == 7:    # sweets: a shelf of jars behind, gingerbread stack
        lbox(d, "CT_WoodLight", fr, -1.3, 0.6, -0.8, -0.55, 1.2, 1.24)
        for u in (-1.28, 0.58):
            lbox(d, "CT_Wood", fr, u - 0.02, u + 0.02, -0.8, -0.76, 0.0, 1.24, faces="xXyY")
        for k in range(4):
            lbox(d, ("CT_PlasterRed", "CT_PlasterPink", "CT_PlasterYellow", "CT_PlasterBlue")[k], fr, -1.15 + 0.45 * k, -0.9 + 0.45 * k, -0.74, -0.6, 1.24, 1.5, faces="xXyYZ")
        for k in range(3):
            cyl(d, "CT_Wood", *fr.p(1.0, -0.1)[:2], ZT + 0.025 * k, ZT + 0.025 * k + 0.022, 0.16 - 0.02 * k, 6, top=True)
    elif n == 8:    # bait: a net on the post, a bucket of minnows, floats on a board
        _net(B, fr, -1.4, -0.5, -0.64, 0.9, 1.9)
        px, py, _ = fr.p(-0.1, -0.95)
        fu.bucket(B, px, py, 0.0, 0.2, 0.34, "CT_PlasterGreen")
        for k in range(8):
            lbox(d, ("CT_PlasterRed", "CT_TileWhite")[k % 2], fr.sub(0.5 + 0.12 * k, -0.64), -0.025, 0.025, -0.012, 0.012, 1.55, 1.7, faces="xXyYZ")
        lbox(d, "CT_WoodLight", fr, 0.4, 1.5, -0.66, -0.64, 1.5, 1.74, faces="xXYz")
    elif n == 16:   # tackle: a pegboard with spoons and floats, a rope coil, a stool for the angler
        lbox(d, "CT_WoodLight", fr, -1.2, 0.2, -0.66, -0.63, 1.2, 1.9)
        for q in range(3):
            for k in range(5):
                lbox(d, ("CT_Aluminium", "CT_SteelRed", "CT_PlasterYellow")[(k + q) % 3], fr, -1.1 + 0.27 * k, -1.03 + 0.27 * k, -0.63, -0.615,
                     1.3 + 0.2 * q, 1.42 + 0.2 * q, faces="xXyYZ")
        for t in range(3):
            cyl(d, "CT_WoodLight", *fr.p(1.1, -0.9)[:2], 0.05 * t, 0.05 * t + 0.045, 0.22 - 0.03 * t, 6, top=True)
        kit.stool(B, *fr.p(1.25, 0.85)[:2], 0.0, fr.yaw, h=0.45)


def _slate_thing(B, fr, n, kind):
    """The stall's chalk slate: the hero reads the day's offer (by hour and by weekday); id slate_<n>."""
    x, y, z = fr.p(-1.15, 0.3, ZT + 0.25)
    B.thing("slate_%d" % n, "board", (x, y, z), fr.yaw % 360, topic="slate", trade=kind)


def _cat(B, x, y, z, yaw, mat="CT_Wood"):
    """The market cat, curled up on its crate: a body, a head with ears, a tail."""
    fr = Fr(x, y, yaw, z)
    d = B.detail
    _tube(d, mat, fr.p(-0.12, 0.0, 0.09), fr.p(0.1, 0.0, 0.09), 0.09, 8)
    blob(d, mat, *fr.p(0.17, 0.0)[:2], z + 0.1, 0.075, 6)
    for s in (-1, 1):
        lbox(d, mat, fr, 0.16, 0.2, s * 0.05 - 0.015, s * 0.05 + 0.015, 0.15, 0.21, faces="xXyYZ")
    fu.rod(d, mat, fr.p(-0.12, 0.0, 0.06), fr.p(-0.1, 0.16, 0.04), 0.03)
    fu.rod(d, mat, fr.p(-0.1, 0.16, 0.04), fr.p(0.06, 0.18, 0.04), 0.03)
    lbox(d, "CT_TileWhite", fr, 0.2, 0.23, -0.025, 0.025, 0.07, 0.1, faces="xXyYZ")


def _pigeon(B, x, y, yaw, mat="CT_ConcreteDark"):
    fr = Fr(x, y, yaw)
    d = B.detail
    _tube(d, mat, fr.p(-0.06, 0.0, 0.06), fr.p(0.06, 0.0, 0.07), 0.035, 6)
    blob(d, "CT_Steel", *fr.p(0.09, 0.0)[:2], 0.1, 0.027, 5)
    lbox(d, mat, fr, -0.1, -0.05, -0.02, 0.02, 0.05, 0.06, faces="xXyYZ")


def _grinder(B, x, y):
    """A pedal grindstone on a bench frame, with a water trough, a stool and a bundle of knives to sharpen."""
    d = B.detail
    fr = Fr(x, y, 0.0)
    for u in (-0.38, 0.38):
        for w in (-0.2, 0.2):
            fu.rod(d, "CT_Wood", fr.p(u, w, 0.0), fr.p(u * 0.8, w * 0.6, 0.78), 0.05)
    lbox(d, "CT_WoodLight", fr, -0.5, 0.5, -0.16, 0.16, 0.78, 0.82)
    _tube(d, "CT_ConcreteDark", fr.p(0.0, -0.06, 0.98), fr.p(0.0, 0.06, 0.98), 0.19, 16)   # the stone: its round face toward the user
    fu.rod(d, "CT_Steel", fr.p(0.0, -0.16, 0.98), fr.p(0.0, 0.16, 0.98), 0.02)
    for w in (-0.14, 0.14):
        fu.rod(d, "CT_Wood", fr.p(0.0, w, 0.82), fr.p(0.0, w, 0.98), 0.03)
    lbox(d, "CT_Steel", fr, -0.1, 0.1, -0.1, 0.1, 0.84, 0.88, faces="xXyYZ")       # the trough under the stone
    fu.rod(d, "CT_Steel", fr.p(0.1, 0.16, 0.98), fr.p(0.32, 0.16, 0.14), 0.012)            # the pedal rod
    lbox(d, "CT_Wood", fr, 0.22, 0.46, -0.16, 0.16, 0.1, 0.14)
    kit.stool(B, *fr.p(0.0, -0.78)[:2], 0.0, 180.0, h=0.45)
    # no modelled person here: people are the life sim's own characters (the grinder_work spot is where one would sit)
    for k in range(3):                                                                     # knives waiting on the bench
        lbox(d, "CT_Steel", fr.sub(-0.35 + 0.06 * k, 0.0, 18 * k - 20), -0.12, 0.12, -0.012, 0.012, 0.82, 0.835, faces="xXyYZ")
        lbox(d, "CT_Wood", fr.sub(-0.35 + 0.06 * k, 0.0, 18 * k - 20), 0.12, 0.2, -0.015, 0.015, 0.82, 0.84, faces="xXyYZ")
    lbox(d, "CT_Paper", fr, 0.26, 0.4, 0.17, 0.172, 0.4, 0.6, faces="y")                  # a tag


def _control_scales(B, x, y):
    """A table with balance scales and a box of brass weights: the market's control scales."""
    d = B.detail
    kit.table(B, x, y, 0.0, 0.0, w=1.1, d=0.6, h=0.82, mat=kit.WOODL)
    t = 0.82
    lbox(d, "CT_Steel", Fr(x, y), -0.03, 0.03, -0.02, 0.02, t, t + 0.5)
    lbox(d, "CT_SteelRed", Fr(x, y), -0.14, 0.14, -0.1, 0.1, t, t + 0.04, faces="xXyYZ")
    fu.rod(d, "CT_Steel", (x - 0.34, y, t + 0.5), (x + 0.34, y, t + 0.5), 0.02)
    for s in (-1, 1):
        px = x + 0.34 * s
        for q in (-0.07, 0.07):
            fu.rod(d, "CT_Steel", (px, y, t + 0.5), (px + 0.0, y + q, t + 0.22), 0.004)
        cyl(d, "CT_Aluminium", px, y, t + 0.2, t + 0.225, 0.11, 10, top=True)
    lbox(d, "CT_Wood", Fr(x, y), 0.3, 0.52, 0.1, 0.26, t, t + 0.05, faces="xXyYZ")
    for k in range(5):
        cyl(d, "CT_Aluminium", x + 0.33 + 0.045 * k, y + 0.18, t + 0.05, t + 0.05 + 0.02 + 0.012 * (5 - k), 0.015 + 0.002 * (5 - k), 6, top=True)
    cyl(d, "CT_Steel", x - 0.34, y, t + 0.225, t + 0.31, 0.04, 6, top=True)               # a big weight on the left pan


def _jar(B, x, y, z, mat, lid="CT_Steel", r=0.05, h=0.14):
    cyl(B.detail, mat, x, y, z, z + h, r, 6, top=False)
    cyl(B.detail, lid, x, y, z + h, z + h + 0.025, r * 1.04, 6, top=True)


def _jam_table(B, x, y):
    """The jury's table of the jam contest: rows of numbered jars with ribbons, spoons, napkins; jury chairs behind."""
    d = B.detail
    kit.table(B, x, y, 0.0, 0.0, w=1.9, d=0.7, h=0.76, mat=kit.WOODL)
    t = 0.76
    lbox(d, "CT_TileWhite", Fr(x, y), -0.95, 0.95, -0.35, 0.35, t, t + 0.01, faces="xXyYZ")        # a cloth
    mats = ("CT_PlasterRed", "CT_PlasterYellow", "CT_SteelRed", "CT_PlasterGreen", "CT_PlasterPink", "CT_PlasterBlue", "CT_PlasterRed")
    for q in range(2):
        for k in range(6):
            jx, jy = x - 0.72 + 0.29 * k, y - 0.12 + 0.24 * q
            _jar(B, jx, jy, t + 0.01, mats[(k + 3 * q) % 7], lid="CT_Paper", r=0.05, h=0.12)
            lbox(d, ("CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterGreen")[(k + q) % 3], Fr(jx, jy), -0.055, 0.055, -0.055, 0.055,
                 t + 0.1, t + 0.108, faces="xXyYZ")
            if q == 0:
                kit.poster(B, jx, jy - 0.052, 0.0, 0.0, name="label_%02d" % (10 + 3 * k), w=0.07, h=0.05, zc=t + 0.06)
    cyl(d, "CT_TileWhite", x + 0.0, y + 0.3, t + 0.01, t + 0.12, 0.04, 8, top=False)        # a glass of spoons
    for s in range(4):
        fu.rod(d, "CT_Aluminium", (x, y + 0.3, t + 0.05), (x + 0.04 * (s - 1.5), y + 0.3, t + 0.2), 0.006)
    for s in (-0.5, 0.5):
        kit.stool(B, x + s, y + 0.75, 0.0, 180.0, h=0.45)


def _tasting_table(B, x, y):
    d = B.detail
    kit.table(B, x, y, 0.0, 0.0, w=1.3, d=0.65, h=0.76, mat=kit.WOODL)
    t = 0.76
    lbox(d, "CT_PlasterRed", Fr(x, y), -0.65, 0.65, -0.32, 0.32, t, t + 0.01, faces="xXyYZ")       # a red cloth
    for k in range(4):                                                                           # saucers with a spoon and a rusk
        sx = x - 0.45 + 0.3 * k
        kit.plates(B, sx, y - 0.1, t + 0.01, 0.0, n=1)
        fu.rod(d, "CT_Aluminium", (sx - 0.05, y - 0.1, t + 0.02), (sx + 0.05, y - 0.05, t + 0.025), 0.008)
    for k in range(3):
        _jar(B, x - 0.35 + 0.35 * k, y + 0.18, t + 0.01, ("CT_PlasterYellow", "CT_PlasterRed", "CT_PlasterGreen")[k], r=0.055, h=0.13)
    kit.cup(B, x + 0.5, y + 0.15, t + 0.01, 0.0)
    kit.sandwich_board(B, x, y - 0.75, 0.0, 0.0, w=0.5, name="price_0")


def _unlabeled_jar(B, x, y):
    """A jar without a label on a little stand by the control scales, a blue hair ribbon on its neck."""
    d = B.detail
    lbox(d, "CT_WoodLight", Fr(x, y), -0.15, 0.15, -0.15, 0.15, 0.0, 0.5, faces="xXyYZ")
    cyl(d, "CT_PlasterYellow", x, y, 0.5, 0.68, 0.075, 10, top=True)
    cyl(d, "CT_Paper", x, y, 0.68, 0.71, 0.08, 10, top=True)
    cyl(d, "CT_PlasterBlue", x, y, 0.655, 0.675, 0.082, 10, top=False)
    lbox(d, "CT_PlasterBlue", Fr(x, y, 20), -0.01, 0.07, -0.082, -0.07, 0.6, 0.65, faces="xXyYZz")


def _herb_corner(B, x, y):
    """A stepped plank stand of pots of herbs, buckets of flowers on the floor, a watering can."""
    d = B.detail
    rr = random.Random(41)
    fr = Fr(x, y, 90.0)
    for t in range(2):
        lbox(d, "CT_WoodLight", fr, -0.7, 0.7, -0.2 + 0.2 * t, 0.0 + 0.2 * t, 0.2 + 0.25 * t, 0.23 + 0.25 * t)
        lbox(d, "CT_Wood", fr, -0.7, 0.7, -0.2 + 0.2 * t, -0.18 + 0.2 * t, 0.0, 0.2 + 0.25 * t, faces="xXyYz")
    for t, mats in enumerate((("CT_PlasterGreen", "CT_Leaf", "CT_PlasterGreen"), ("CT_Leaf", "CT_Grass", "CT_PlasterGreen"))):
        for k in range(3):
            px, py, _ = fr.p(-0.4 + 0.4 * k, -0.1 + 0.2 * t)
            z0 = 0.23 + 0.25 * t
            cyl(d, ("CT_PlasterRed", "CT_PlasterBlue", "CT_TileWhite")[(k + t) % 3], px, py, z0, z0 + 0.1, 0.06, 6, top=True)
            blob(d, mats[k % 3], px, py, z0 + 0.18, 0.09, 5)
    for k in range(3):
        px, py, _ = fr.p(-0.6 + 0.55 * k, 0.55)
        fu.bucket(B, px, py, 0.0, 0.14, 0.26, ("CT_Steel", "CT_PlasterBlue", "CT_TileWhite")[k], fu.FLOWERS, rr)
    px, py, _ = fr.p(0.95, 0.4)
    cyl(d, "CT_PlasterGreen", px, py, 0.0, 0.2, 0.09, 8, top=True)
    fu.rod(d, "CT_PlasterGreen", (px, py, 0.14), (px + 0.0, py + 0.2, 0.28), 0.025)
    fu.rod(d, "CT_PlasterGreen", (px - 0.05, py, 0.2), (px - 0.05, py, 0.3), 0.012)
    kit.sandwich_board(B, *fr.p(1.2, 0.75)[:2], 0.0, 90.0, w=0.45, name="price_0")
    _hanging_herbs(B, fr, -0.8, 0.8, -0.25, 1.95, rr, n=5)                      # drying bunches overhead
    for k in range(2):                                                           # a lidded jar of dried herbs and a bag, by the pots
        px, py, _ = fr.p(-0.62 + 1.24 * k, 0.1)
        _jar(B, px, py, 0.48, ("CT_PlasterGreen", "CT_WoodLight")[k], lid="CT_Paper", r=0.06, h=0.16)


def _complaint_stand(B, x, y):
    d = B.detail
    fr = Fr(x, y, 0.0)
    lbox(d, "CT_Wood", fr, -0.2, 0.2, -0.15, 0.15, 0.0, 0.05)
    lbox(d, "CT_Wood", fr, -0.03, 0.03, -0.03, 0.03, 0.05, 1.0, faces="xXyY")
    fu.rod(d, "CT_Wood", fr.p(0.0, -0.12, 1.1), fr.p(0.0, 0.12, 1.0), 0.03)
    lbox(d, "CT_PlasterRed", fr.sub(0.0, 0.0, 0), -0.17, 0.17, -0.2, 0.0, 1.0, 1.07, faces="xXyYZz")
    lbox(d, "CT_TileWhite", fr, -0.15, 0.15, -0.18, -0.02, 1.07, 1.09, faces="xXyYZ")
    fu.rod(d, "CT_Steel", fr.p(0.18, 0.0, 1.11), fr.p(0.1, 0.1, 1.12), 0.006)       # a pen on a string


# ------------------------------------------------------------------------------------------------ round 2: what the eye meets first
def _cone(m, mat, base, tip, r, segs=6, cap=True):
    """A cone / tapered spike from base (radius r) to tip: carrots, paper cones, ears, beaks (segs + segs - 2 tris)."""
    ax = geom.norm(geom.sub(tip, base))
    ref = (0, 0, 1) if abs(ax[2]) < 0.9 else (1, 0, 0)
    u = geom.norm(geom.cross(ax, ref))
    v = geom.cross(ax, u)
    ring = [tuple(base[i] + r * (u[i] * math.cos(2 * math.pi * k / segs) + v[i] * math.sin(2 * math.pi * k / segs)) for i in range(3))
            for k in range(segs)]
    cen = tuple((base[i] + tip[i]) / 2 for i in range(3))
    quads = [[ring[k], ring[(k + 1) % segs], tip] for k in range(segs)]
    if cap:
        quads.append(ring[::-1])
    for q in quads:
        n = geom.cross(geom.sub(q[1], q[0]), geom.sub(q[2], q[0]))
        mid = tuple(sum(p[i] for p in q) / len(q) for i in range(3))
        if sum(n[i] * (mid[i] - cen[i]) for i in range(3)) < 0:
            q = q[::-1]
        m.face(mat, q)


def _ell(m, mat, c, rx, ry, rz, yaw=0.0, segs=6):
    """An ellipsoid-ish body (blob scaled and turned): breasts, bodies, heads (4 * segs tris)."""
    cy_, sy_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))

    def P(a, t, s):
        ex, ey = rx * s * math.cos(a), ry * s * math.sin(a)
        return (c[0] + ex * cy_ - ey * sy_, c[1] + ex * sy_ + ey * cy_, c[2] + rz * t)
    top = (c[0], c[1], c[2] + rz * 0.97)
    bot = (c[0], c[1], c[2] - rz * 0.97)
    up = [P(2 * math.pi * k / segs, 0.35, 1.0) for k in range(segs)]
    lo = [P(2 * math.pi * k / segs, -0.35, 1.0) for k in range(segs)]
    for k in range(segs):
        k2 = (k + 1) % segs
        m.face(mat, [up[k], up[k2], top])
        m.face(mat, [lo[k], up[k], up[k2], lo[k2]])
        m.face(mat, [lo[k2], lo[k], bot])


def _weights(B, fr, u, w, z, n=2):
    """Brass weights by the scales: a short stack of ever smaller weights with a knob each."""
    d = B.detail
    for k in range(n):
        x, y, _ = fr.p(u + 0.09 * k, w)
        r = 0.04 - 0.008 * k
        cyl(d, "CT_Aluminium", x, y, z, z + 0.05 - 0.01 * k, r, 5, top=True)
        cyl(d, "CT_Aluminium", x, y, z + 0.05 - 0.01 * k, z + 0.07 - 0.01 * k, r * 0.4, 4, top=True)


def _bunch(B, fr, u, w, z, mat, leaf, n=5, length=0.26, spread=14):
    """A tied bunch lying on the counter (carrots, radishes, dill): tapering roots in a fan, a leafy tuft, a string round the middle."""
    d = B.detail
    for k in range(n):
        a = math.radians((k - (n - 1) / 2) * spread)
        base = fr.p(u - length * 0.45, w + 0.03 * (k - n / 2), z + 0.03 + 0.012 * (k % 2))
        tip = fr.p(u + length * 0.55 * math.cos(a), w + length * 0.55 * math.sin(a) + 0.03 * (k - n / 2), z + 0.022)
        _cone(d, mat, base, tip, 0.022, 5, cap=False)
    for k in range(2):
        px, py, _ = fr.p(u - length * 0.5, w + 0.02 * (k - 0.5) * 2)
        blob(d, leaf, px, py, z + 0.05, 0.045 + 0.01 * k, 5)
    _tube(d, "CT_Paper", fr.p(u, w - 0.05, z + 0.045), fr.p(u, w + 0.05, z + 0.045), 0.011, 4, caps=False)


def _paper_cones(B, fr, u, w, z, mats, n=3):
    """Paper cones of seeds / sweets standing in a wooden block: tip down in the holes, a heap in each mouth."""
    d = B.detail
    lbox(d, "CT_WoodLight", fr, u - 0.06 * n - 0.02, u + 0.06 * n + 0.02, w - 0.07, w + 0.07, z, z + 0.05, faces="xXyYZ")
    for k in range(n):
        x, y, _ = fr.p(u + (k - (n - 1) / 2) * 0.12, w)
        _cone(d, "CT_Paper", (x, y, z + 0.22), (x, y, z + 0.05), 0.05, 6, cap=False)
        cyl(d, mats[k % len(mats)], x, y, z + 0.215, z + 0.235, 0.044, 6, top=True)


def _abacus(B, fr, u, w, z):
    """Wooden counting frame standing on the counter's edge: frame, three wires, two blocks of beads on each."""
    d = B.detail
    lbox(d, "CT_Wood", fr, u - 0.16, u + 0.16, w - 0.025, w + 0.025, z, z + 0.02, faces="xXyYZz")
    lbox(d, "CT_Wood", fr, u - 0.16, u + 0.16, w - 0.025, w + 0.025, z + 0.22, z + 0.24, faces="xXyYZz")
    for s in (-1, 1):
        lbox(d, "CT_Wood", fr, u + s * 0.16 - 0.012, u + s * 0.16 + 0.012, w - 0.025, w + 0.025, z + 0.02, z + 0.22, faces="xXyY")
    for q in range(3):
        zz = z + 0.06 + 0.06 * q
        lbox(d, "CT_Steel", fr, u - 0.15, u + 0.15, w - 0.003, w + 0.003, zz, zz + 0.004, faces="yYZ")
        c1, c2 = ("CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterBlue")[q], ("CT_PlasterYellow", "CT_PlasterBlue", "CT_PlasterRed")[q]
        lbox(d, c1, fr, u - 0.14, u - 0.14 + 0.07 + 0.03 * q, w - 0.014, w + 0.014, zz - 0.012, zz + 0.016, faces="xXyYZ")
        lbox(d, c2, fr, u + 0.14 - 0.07 - 0.02 * q, u + 0.14, w - 0.014, w + 0.014, zz - 0.012, zz + 0.016, faces="xXyYZ")


def _booklet(B, fr, u, w, z):
    """A seed catalogue hung on a cord from the awning's price string."""
    d = B.detail
    lbox(d, "CT_Paper", fr, u - 0.003, u + 0.003, w - 0.003, w + 0.003, z + 0.2, z + 0.4, faces="xXyY")
    lbox(d, "CT_PlasterGreen", fr, u - 0.1, u + 0.1, w - 0.012, w + 0.012, z - 0.04, z + 0.22, faces="xXyYZz")
    lbox(d, "CT_TileWhite", fr, u - 0.07, u + 0.07, w + 0.012, w + 0.014, z + 0.04, z + 0.15, faces="y")
    lbox(d, "CT_PlasterYellow", fr, u - 0.05, u + 0.05, w + 0.014, w + 0.016, z + 0.16, z + 0.19, faces="y")


def _clipboard(B, x, y, z, yaw):
    d = B.detail
    fr = Fr(x, y, yaw)
    lbox(d, "CT_WoodLight", fr, -0.11, 0.11, -0.15, 0.15, z, z + 0.012, faces="xXyYZ")
    lbox(d, "CT_Paper", fr, -0.095, 0.095, -0.13, 0.13, z + 0.012, z + 0.017, faces="xXyYZ")
    lbox(d, "CT_Steel", fr, -0.04, 0.04, 0.12, 0.15, z + 0.012, z + 0.025, faces="xXyYZ")


def _bird(B, x, y, yaw, mat="CT_ConcreteDark", head_up=0.0, peck=False):
    """A pigeon: a plump body with a puffed breast, neck and small head, a beak, a wing, a wedge tail and two pink legs (~100 tris)."""
    fr = Fr(x, y, yaw)
    d = B.detail
    z0 = 0.075
    pitch = 0.04 if peck else 0.0
    _ell(d, mat, fr.p(-0.005, 0.0, z0 + 0.03), 0.075, 0.05, 0.05, yaw, 6)
    _ell(d, "CT_Steel" if mat != "CT_Steel" else "CT_TileWhite", fr.p(0.055, 0.0, z0 + 0.05), 0.04, 0.042, 0.045, yaw, 5)       # the breast
    hx, hz = (0.115, z0 + 0.1 - 0.07 * pitch * 10) if peck else (0.095, z0 + 0.1 + head_up)
    _ell(d, mat, fr.p(hx, 0.0, hz), 0.027, 0.025, 0.027, yaw, 5)
    _cone(d, "CT_PlasterYellow", fr.p(hx + 0.015, 0.0, hz - 0.002), fr.p(hx + 0.052, 0.0, hz - 0.012), 0.009, 4, cap=False)
    _tube(d, mat, fr.p(0.06, 0.0, z0 + 0.05), fr.p(hx - 0.01, 0.0, hz - 0.01), 0.018, 4, caps=False)                          # the neck
    lbox(d, "CT_Steel" if mat != "CT_Steel" else "CT_ConcreteDark", fr, -0.07, 0.03, 0.036, 0.046, z0 + 0.02, z0 + 0.075, faces="xXyYZ")   # wing
    lbox(d, "CT_Steel" if mat != "CT_Steel" else "CT_ConcreteDark", fr, -0.07, 0.03, -0.046, -0.036, z0 + 0.02, z0 + 0.075, faces="xXyYZ")
    f2 = fr.sub(-0.1, 0.0)
    fu.rod(d, mat, f2.p(0.0, 0.0, z0 + 0.04), f2.p(-0.085, 0.0, z0 + 0.02), 0.035, caps=True)                               # the tail
    for s in (-0.02, 0.02):
        _tube(d, "CT_PlasterPink", fr.p(0.0, s, z0 - 0.01), fr.p(0.01, s, 0.0), 0.006, 3, caps=False)


def _cat2(B, x, y, z, yaw, mat="CT_Wood", patch="CT_TileWhite"):
    """The market cat, sitting loaf-style: a long body with a haunch, white chest and paws, a round head with ears, whiskers' pad,
    a tail curled round the front, tabby stripes (~220 tris)."""
    fr = Fr(x, y, yaw, z)
    d = B.detail
    _ell(d, mat, fr.p(-0.04, 0.0, 0.1), 0.19, 0.115, 0.11, yaw, 8)                      # the body
    _ell(d, mat, fr.p(-0.1, 0.0, 0.11), 0.11, 0.14, 0.1, yaw, 6)                        # the haunch bulge
    _ell(d, patch, fr.p(0.1, 0.0, 0.1), 0.06, 0.07, 0.085, yaw, 6)                       # the white chest
    _ell(d, mat, fr.p(0.17, 0.0, 0.2), 0.07, 0.07, 0.065, yaw, 6)                       # the head
    for s in (-1, 1):
        _cone(d, mat, fr.p(0.155, s * 0.045, 0.235), fr.p(0.15, s * 0.06, 0.3), 0.03, 3, cap=False)   # ears
        lbox(d, patch, fr, 0.18, 0.26, s * 0.045 - 0.014, s * 0.045 + 0.014, 0.0, 0.035, faces="xXyYZ")  # white front paws
    lbox(d, patch, fr, 0.22, 0.24, -0.018, 0.018, 0.17, 0.2, faces="xXyYZ")              # muzzle
    for k in range(3):                                                                  # tabby stripes over the back
        lbox(d, "CT_ConcreteDark", fr.sub(-0.14 + 0.07 * k, 0.0), -0.012, 0.012, -0.1, 0.1, 0.165 + 0.01 * (k % 2), 0.185 + 0.01 * (k % 2), faces="xXyYZ")
    a, b, c_ = fr.p(-0.2, 0.05, 0.05), fr.p(-0.2, 0.16, 0.03), fr.p(-0.02, 0.17, 0.03)
    _tube(d, mat, a, b, 0.028, 5, caps=False)
    _tube(d, mat, b, c_, 0.026, 5, caps=False)
    _ell(d, patch, fr.p(0.0, 0.17, 0.04), 0.03, 0.025, 0.025, yaw, 4)                   # the white tail tip


def _figure(B, fr, mat_coat="CT_PlasterBlue", mat_cap="CT_Wood", seated=True):
    """A man at work, seated on a stool, leaning toward what he does (the knife grinder): legs, boots, torso, apron, arms, head, cap.
    His front is the frame's +w side (toward -y at yaw 0). ~230 tris."""
    d = B.detail
    skin = "CT_PlasterPink"
    zs = 0.47
    for s in (-1, 1):
        _tube(d, "CT_ConcreteDark", fr.p(s * 0.1, -0.02, zs + 0.06), fr.p(s * 0.11, 0.36, zs + 0.06), 0.068, 5, caps=True)     # thighs
        _tube(d, "CT_ConcreteDark", fr.p(s * 0.11, 0.36, zs + 0.06), fr.p(s * 0.11, 0.4, 0.07), 0.055, 5, caps=False)          # shins
        lbox(d, "CT_Wood", fr.sub(s * 0.11, 0.44), -0.05, 0.05, -0.1, 0.1, 0.0, 0.07, faces="xXyYZ")                           # boots
    _tube(d, mat_coat, fr.p(0.0, -0.01, zs + 0.1), fr.p(0.0, 0.1, zs + 0.52), 0.15, 8, caps=True)                                # torso, leaning
    lbox(d, "CT_Paper", fr, -0.12, 0.12, 0.14, 0.165, zs - 0.08, zs + 0.36, faces="xXyYZz")                                      # apron
    for s in (-1, 1):
        sh = fr.p(s * 0.17, 0.09, zs + 0.5)
        _tube(d, mat_coat, sh, fr.p(s * 0.1, 0.4, zs + 0.38), 0.05, 5, caps=True)                                              # arms to the stone
        blob(d, skin, *fr.p(s * 0.09, 0.44)[:2], zs + 0.37, 0.04, 5)
    _tube(d, skin, fr.p(0.0, 0.12, zs + 0.5), fr.p(0.0, 0.15, zs + 0.64), 0.05, 5, caps=False)                                  # neck
    _ell(d, skin, fr.p(0.0, 0.17, zs + 0.7), 0.085, 0.085, 0.1, fr.yaw, 6)                                                       # the head
    cyl(d, mat_cap, *fr.p(0.0, 0.17)[:2], zs + 0.76, zs + 0.8, 0.095, 8, top=True)                                                # the cap
    lbox(d, mat_cap, fr, -0.09, 0.09, 0.25, 0.3, zs + 0.74, zs + 0.765, faces="xXyYZ")                                          # peak


def _tufts(B, fr, u, w, z, mat, n=3, r=0.07):
    for k in range(n):
        x, y, _ = fr.p(u + (k - (n - 1) / 2) * r * 1.2, w + 0.01 * (k % 2))
        _cone(B.detail, mat, (x, y, z), (x + 0.01 * (k - 1), y, z + 0.16 + 0.03 * (k % 2)), r, 5, cap=False)


def _hanging_herbs(B, fr, u0, u1, w, z, rnd, n=6):
    """Drying bunches hung head-down from a pole across the herb stand (cones of leaves, tied with a string)."""
    d = B.detail
    lbox(d, "CT_Wood", fr, u0 - 0.03, u1 + 0.03, w - 0.02, w + 0.02, z, z + 0.04)
    for u in (u0, u1):
        lbox(d, "CT_Wood", fr, u - 0.03, u + 0.03, w - 0.03, w + 0.03, 0.0, z + 0.04, faces="xXyY")
    for k in range(n):
        u = u0 + 0.2 + k * (u1 - u0 - 0.4) / (n - 1)
        x, y, _ = fr.p(u, w)
        lbox(d, "CT_Paper", fr, u - 0.004, u + 0.004, w - 0.004, w + 0.004, z - 0.1, z, faces="xXyY")
        _cone(d, ("CT_Leaf", "CT_Grass", "CT_PlasterGreen", "CT_PlasterYellow")[k % 4], (x, y, z - 0.42 - 0.05 * (k % 3)), (x, y, z - 0.1), 0.07, 5, cap=False)
        _tube(d, "CT_Paper", (x, y, z - 0.14), (x, y, z - 0.16), 0.03, 5, caps=False)


CHAT_KINDS = ("fruit", "veg", "dairy", "bread", "birds", "sweets", "bait")


def stall_things(B, fr, n, kind):
    """Round 2 things of a stall: a talk with the seller (every shop stall), haggling where an abacus stands, the seed catalogue."""
    yaw = fr.yaw % 360
    back = (fr.yaw + 180) % 360
    if kind in CHAT_KINDS:
        B.thing("chat_%d" % n, "counter", fr.p(0.8, 0.0, ZT), yaw, service="talk", role="seller", trade=kind)
    if n in (1, 3, 6):
        B.thing("haggle_%d" % n, "play", fr.p(-0.45, 0.39, ZT + (0.12 if kind == "fish" else 0.0) + 0.1), yaw, what="haggle", trade=kind)
        x, y, _ = fr.p(-0.45, 1.1)
        B.spot("haggle_stand_%d" % n, "stand", (x, y, 0), back)
    if n == 13:
        B.thing("seed_catalogue", "shelf", fr.p(-1.45, 0.9, 1.5), yaw, topic="catalogue")
        x, y, _ = fr.p(-1.45, 1.6)
        B.spot("seed_stand", "stand", (x, y, 0), back)


def stall_depth(B, fr, n, kind, rg):
    """Round 2 of a stall: weights by the scales, bunches, cones, an abacus where one haggles, the seed catalogue."""
    ZT_ = ZT
    if kind in ("fish", "veg", "fruit", "dairy", "butcher"):
        _weights(B, fr, 1.0, 0.21, ZT_ if kind != "fish" else ZT_ + 0.12)
    else:
        _weights(B, fr, 0.98, 0.3, ZT_)
    if kind == "veg":
        for k, (u, mt, lf) in enumerate(((-0.6, "CT_PlasterRed", "CT_PlasterGreen"), (0.3, "CT_PlasterYellow", "CT_Leaf"))):
            _bunch(B, fr, u, 0.33, ZT_ + 0.0, mt, lf)
    if kind in ("birds",):
        _paper_cones(B, fr, 1.12, -0.12, ZT_, ("CT_PlasterYellow", "CT_WoodLight", "CT_PlasterGreen"))
    if kind == "sweets":
        _paper_cones(B, fr, 1.12, -0.12, ZT_, ("CT_PlasterRed", "CT_PlasterPink", "CT_PlasterYellow"))
    if n in (1, 3, 6):
        _abacus(B, fr, -0.45, 0.39, ZT_ + (0.12 if kind == "fish" else 0.0))
    if n == 13:
        _booklet(B, fr, -1.45, 0.9, 1.45)
    if n == 14:      # the jam stall: a bunch of herbs and a paper cone of sugar on the counter
        _paper_cones(B, fr, 1.12, -0.12, ZT_, ("CT_TileWhite",), n=2)


def _at_umbrella(B, x, y, ums):
    i = max(0, min(NX - 1, int(round(x / PITCH + (NX - 1) / 2))))
    j = max(0, min(NY - 1, int(round((y - Y0) / PITCH))))
    cx, cy, zc, zr = ums[(i, j)]
    return z_under(cx, cy, zc, zr, x, y)


def _hang_lamp(B, x, y, ums):
    zu = _at_umbrella(B, x, y, ums)
    zl = max(2.35, zu - 0.45)
    B.lamp((x, y, zl), lumens=760, colour="#FFD9A8", kind="public", radius=7.0)
    lbox(B.detail, "CT_Steel", Fr(x, y), -0.008, 0.008, -0.008, 0.008, zl + 0.08, zu + 0.02, faces="xXyY")


def _carp_tank(B, x, y):
    """A glass tank of live carp on a steel stand at the east end of the fish stall 6: a lid board, a dip net, a water line."""
    d = B.detail
    fr = Fr(x, y, 0.0)
    for u in (-0.42, 0.42):
        for w in (-0.22, 0.22):
            lbox(d, "CT_Steel", fr, u - 0.025, u + 0.025, w - 0.025, w + 0.025, 0.0, 0.6, faces="xXyYz")
    lbox(d, "CT_Steel", fr, -0.48, 0.48, -0.28, 0.28, 0.58, 0.62, faces="xXyYZz")          # the tray
    lbox(d, "CT_Glass", fr, -0.46, 0.46, -0.26, 0.26, 0.62, 0.9, faces="xXyY")            # the glass walls
    lbox(d, "CT_TileWhite", fr, -0.44, 0.44, -0.24, 0.24, 0.62, 0.66, faces="xXyYZ")     # a pale tank floor
    for k, (u, w, a, mat) in enumerate(((-0.2, 0.05, 20.0, "CT_Wood"), (0.08, -0.08, 160.0, "CT_WoodLight"),
                                        (0.16, 0.12, 70.0, "CT_Wood"))):
        _fish(d, mat, fr, u, w, 0.74 + 0.05 * k, a, length=0.26, r=0.045)
    lbox(d, "CT_WoodLight", fr, 0.2, 0.56, 0.3, 0.46, 0.05, 0.09, faces="xXyYZ")            # a lid board leaning by the stand
    lbox(d, "CT_WoodLight", fr, 0.3, 0.46, 0.3, 0.46, 0.05, 0.62, faces="xXyYZ")
    fu.rod(d, "CT_Steel", fr.p(-0.6, 0.38, 0.0), fr.p(-0.56, 0.3, 1.0), 0.012)           # a dip net on a pole
    _tube(d, "CT_Steel", fr.p(-0.56, 0.3, 1.0), fr.p(-0.5, 0.18, 1.04), 0.008, 6)
    cyl(d, "CT_PlasterGreen", *fr.p(-0.52, 0.22)[:2], 0.98, 1.02, 0.1, 8, top=True)  # the net bag


def _ice_bed(B, x, y):
    """The day's catch on a bed of crushed ice: a steel tray on a trestle, ice heaps, six fish, a chalk A-board."""
    d = B.detail
    fr = Fr(x, y, 0.0)
    for u in (-0.26, 0.26):
        for w in (-0.46, 0.46):
            lbox(d, "CT_Wood", fr, u - 0.03, u + 0.03, w - 0.03, w + 0.03, 0.0, 0.86, faces="xXyYz")
    lbox(d, "CT_Steel", fr, -0.34, 0.34, -0.54, 0.54, 0.84, 0.88, faces="xXyYZz")
    for u, w in ((-0.34, 0), (0.34, 0)):
        lbox(d, "CT_Steel", fr, u - 0.012, u + 0.012, -0.54, 0.54, 0.88, 0.95, faces="xXyYZ")
    for w in (-0.54, 0.54):
        lbox(d, "CT_Steel", fr, -0.34, 0.34, w - 0.012, w + 0.012, 0.88, 0.95, faces="xXyYZ")
    r = random.Random(41)
    for k in range(14):
        blob(d, "CT_TileWhite", x + r.uniform(-0.26, 0.26), y + r.uniform(-0.46, 0.46), 0.92 + r.uniform(0, 0.03), 0.07 + r.uniform(0, 0.025), 5)
    for k, (u, w, a, mat, ln) in enumerate(((-0.15, -0.36, 80.0, "CT_Steel", 0.3), (0.12, -0.2, 100.0, "CT_PlasterBlue", 0.26),
                                           (-0.1, -0.02, 70.0, "CT_ConcreteDark", 0.28), (0.15, 0.14, 105.0, "CT_Steel", 0.3),
                                           (-0.08, 0.3, 85.0, "CT_WoodLight", 0.26), (0.14, 0.42, 95.0, "CT_PlasterBlue", 0.24))):
        _fish(d, mat, fr, u, w, 0.93 + 0.02 * (k % 2), a, length=ln, r=0.035)
    lbox(d, "CT_ConcreteDark", fr, 0.4, 0.44, 0.6, 1.05, 0.0, 0.5, faces="xXyYZ")           # the chalk board on its own foot
    lbox(d, "CT_Paper", fr, 0.44, 0.445, 0.66, 0.99, 0.18, 0.44, faces="X")


def _gallows(B, x, y):
    """A post with an arm and a hanging steelyard for weighing the catch (east of the carp tank)."""
    d = B.detail
    fr = Fr(x, y, 0.0)
    lbox(d, "CT_Steel", fr, -0.04, 0.04, -0.04, 0.04, 0.0, 2.0, faces="xXyYZ")
    lbox(d, "CT_Steel", fr, -0.4, 0.04, -0.02, 0.02, 1.96, 2.0, faces="xXyYZz")
    _steelyard(B, Fr(x - 0.4, y, 0.0), 0.0, 0.0, 1.5)


def _cuts_chart(B, x, y):
    """The cuts chart of the meat row: a notice board on two posts with butcher's labels pinned in a row (the labels of the stall cards)."""
    kit.notice_stand(B, x, y, 0.0, 90.0, w=1.1, h=0.8, front="notice_1", back="notice_2", seed=11)
    fr = Fr(x, y, 90.0)
    for k, cell in enumerate((8, 38, 27, 57)):
        px, py, _ = fr.p(-0.45 + 0.3 * k, 0.17)
        kit.poster(B, px, py, 0.0, 90.0, name="label_%02d" % cell, w=0.24, h=0.17, zc=1.5 - 0.02 * (k % 2))


def _hook_rack(B, x, y):
    """A rail of meat hooks along y at 1.85 m: posts, six hooks, three joints and a ham hanging, a cleaver and a hanging steel."""
    d = B.detail
    fr = Fr(x, y, 0.0)
    for w in (-0.7, 0.7):
        lbox(d, "CT_Steel", fr, -0.04, 0.04, w - 0.04, w + 0.04, 0.0, 2.0, faces="xXyYZ")
    lbox(d, "CT_Steel", fr, -0.03, 0.03, -0.7, 0.7, 1.9, 1.96, faces="xXyYZz")
    for k in range(6):
        w = -0.55 + 0.22 * k
        fu.rod(d, "CT_Steel", fr.p(0.0, w, 1.9), fr.p(0.05, w, 1.7), 0.012)
    for k, (w, mat) in enumerate(((-0.55, "CT_PlasterRed"), (-0.11, "CT_SteelRed"), (0.33, "CT_PlasterRed"))):
        px, py, _ = fr.p(0.05, w)
        _tube(d, mat, (px, py, 1.66), (px, py, 1.34), 0.055, 8)
        blob(d, mat, px, py, 1.42, 0.12, 8)
        lbox(d, "CT_WoodLight", fr, 0.0, 0.1, w - 0.015, w + 0.015, 1.7, 1.72, faces="xXyYZ")
    lbox(d, "CT_Paper", fr, 0.05, 0.065, 0.58, 0.68, 1.2, 1.3, faces="xXyYZ")          # a tag on the post


def _bone_bowl(B, x, y):
    """The dog's enamel bowl with a marrow bone and a chalk-marked brick."""
    d = B.detail
    cyl(d, "CT_PlasterRed", x, y, 0.0, 0.05, 0.14, 10, top=False, bottom=True)
    cyl(d, "CT_TileWhite", x, y, 0.04, 0.062, 0.12, 10, top=True)
    fr = Fr(x, y, 25.0)
    _tube(d, "CT_Paper", fr.p(-0.1, 0.0, 0.09), fr.p(0.1, 0.0, 0.09), 0.022, 6)
    for s_ in (-1, 1):
        for t_ in (-1, 1):
            blob(d, "CT_Paper", *fr.p(s_ * 0.108, t_ * 0.018)[:2], 0.092, 0.026, 5)
    lbox(d, "CT_Brick", Fr(x + 0.28, y + 0.05, 10.0), -0.1, 0.1, -0.05, 0.05, 0.0, 0.07, faces="xXyYZ")


def _bell_post(B, x, y):
    """The market bell on a steel post with an arm: a bronze bell, a clapper and a rope with a knot."""
    d = B.detail
    lbox(d, "CT_Steel", Fr(x, y), -0.04, 0.04, -0.04, 0.04, 0.0, 2.15, faces="xXyYZ")
    lbox(d, "CT_Steel", Fr(x, y), -0.04, 0.4, -0.025, 0.025, 2.08, 2.14, faces="xXyYZz")
    bx = x + 0.32
    cyl(d, "CT_Brass", bx, y, 1.74, 1.84, 0.13, 12, top=False, bottom=True)
    cyl(d, "CT_Brass", bx, y, 1.84, 1.93, 0.095, 12, top=False)
    cyl(d, "CT_Brass", bx, y, 1.93, 1.99, 0.045, 10, top=True)
    fu.rod(d, "CT_Steel", (bx, y, 1.99), (bx, y, 2.08), 0.012)
    blob(d, "CT_Steel", bx, y, 1.72, 0.03, 5)
    fu.rod(d, "CT_Paper", (bx, y, 1.72), (bx, y + 0.0, 1.0), 0.012)
    blob(d, "CT_Paper", bx, y, 1.04, 0.025, 5)
    cyl(d, "CT_Brass", x, y, 1.3, 1.34, 0.06, 8, top=True)


def _ticket_post(B, x, y):
    """The grinder's ticket board on a post: a pegboard with paper tags and a bag of whetstones."""
    d = B.detail
    fr = Fr(x, y, 0.0)
    lbox(d, "CT_Wood", fr, -0.03, 0.03, -0.03, 0.03, 0.0, 1.5, faces="xXyYZ")
    lbox(d, "CT_WoodLight", fr, -0.24, 0.24, 0.03, 0.06, 1.12, 1.5, faces="xXyYZz")
    for k in range(5):
        lbox(d, "CT_Paper", fr, -0.2 + 0.085 * k, -0.15 + 0.085 * k, 0.06, 0.085, 1.2 + 0.07 * (k % 2), 1.31 + 0.07 * (k % 2), faces="xXyYZ")
    for k in range(3):
        lbox(d, "CT_ConcreteDark", fr, 0.12 + 0.07 * k, 0.17 + 0.07 * k, 0.08, 0.13, 0.0, 0.05, faces="xXyYZ")


def _rows(B):
    """Third depth pass: what the fish and meat rows and the corners lacked: live carp, an ice bed with the day's board, a catch
    weighing, a cuts chart, a hook rail, the dog's bowl, the market bell and the grinder's tickets (+ the staff's posts)."""
    # fish row
    _carp_tank(B, -2.7, 20.3)
    B.thing("carp_tank", "board", (-2.7, 20.3, 1.05), 0.0, topic="carp")
    B.spot("carp_stand", "stand", (-2.7, 19.3, 0), 180)
    _gallows(B, -1.4, 20.3)
    B.thing("fish_weigh", "play", (-1.8, 20.3, 1.4), 0.0, what="weigh", trade="fish")
    B.spot("weigh_stand", "stand", (-1.8, 19.3, 0), 180)
    _ice_bed(B, 2.5, 19.7)
    B.thing("ice_board", "board", (2.5, 19.75, 1.0), 270.0, topic="ice")
    B.spot("ice_stand", "stand", (1.5, 19.7, 0), 90)
    # meat row
    _cuts_chart(B, -17.6, 14.9)
    B.thing("cuts_chart", "board", (-17.6, 14.9, 1.3), 90.0, topic="cuts")
    B.spot("cuts_stand", "stand", (-16.7, 14.9, 0), 270)
    _hook_rack(B, 18.4, 19.85)
    B.thing("hook_rack", "board", (18.4, 19.85, 1.6), 90.0, topic="hooks")
    B.spot("hook_stand", "stand", (19.4, 19.85, 0), 270)
    _bone_bowl(B, 18.95, 19.2)
    B.thing("bone_bowl", "board", (18.95, 19.2, 0.15), 90.0, topic="dog")
    # the bell at the entrance, the grinder's tickets
    _bell_post(B, 1.6, 2.4)
    B.thing("market_bell", "board", (1.9, 2.4, 1.2), 0.0, topic="bell")
    B.spot("bell_stand", "stand", (1.9, 1.6, 0), 180)
    _ticket_post(B, -9.4, 12.6)
    B.thing("grinder_tickets", "board", (-9.4, 12.56, 1.3), 0.0, topic="tickets")
    B.spot("tickets_stand", "stand", (-9.4, 11.7, 0), 180)
    # the stalls' own people at their posts (roles of the workplace: fishmonger, butcher)
    B.spot("staff_fishmonger_1", "work", (-5.2, 21.3, 0), 0)
    B.spot("staff_fishmonger_2", "work", (3.5, 17.4, 0), 270)
    B.spot("staff_butcher_1", "work", (-19.3, 17.4, 0), 90)
    B.spot("staff_butcher_2", "work", (19.3, 17.4, 0), 270)


def _corners(B, ums):
    """The market's life between the stalls: the jam contest, control scales, a knife grinder, herbs and flowers, the cat,
    the pigeons, the complaint book, the day board, the tea stand's radio, the fishmongers' trays."""
    d = B.detail
    # the jam contest: jury table, tasting table, the unlabelled jar by the control scales
    _jam_table(B, -6.4, 5.7)
    B.thing("jam_jars", "shelf", (-6.4, 5.7, 0.95), 0.0, topic="jam")
    B.spot("jam_stand", "stand", (-6.4, 4.8, 0), 0)
    _tasting_table(B, -3.2, 5.7)
    B.thing("jam_tasting", "shelf", (-3.2, 5.7, 0.95), 0.0, topic="jam")
    B.spot("tasting_stand", "stand", (-3.2, 4.7, 0), 0)
    _control_scales(B, -11.9, 6.2)
    B.thing("control_scales", "machine", (-11.9, 6.2, 1.15), 0.0, what="control_scales")
    B.spot("control_stand", "stand", (-11.9, 5.2, 0), 0)
    _unlabeled_jar(B, -10.9, 6.5)
    B.thing("unlabeled_jar", "board", (-10.9, 6.5, 0.7), 0.0, topic="jam")
    _hang_lamp(B, -8.8, 6.2, ums)
    _clipboard(B, -5.75, 5.85, 0.77, 20.0)
    B.thing("jury_protocol", "board", (-5.75, 5.85, 0.82), 0.0, topic="jam")
    B.spot("jury_stand", "stand", (-5.3, 4.9, 0), 0)
    # the market clock on a post by the entrance, a rules plaque, the tasting card at the office, the lamp switch
    lbox(d, "CT_Steel", Fr(3.0, 2.5), -0.04, 0.04, -0.04, 0.04, 0.0, 2.45, faces="xXyYZ")
    kit.wall_clock(B, 3.0, 2.46, 0.0, 0.0, zc=2.2, r=0.2, face="clock_1")
    B.spot("clock_stand", "stand", (3.0, 1.6, 0), 180)
    kit.notice_stand(B, -15.6, 3.6, 0.0, 0.0, w=1.0, h=0.7, front="notice_1", back="notice_2", seed=7)
    B.thing("rules_plaque", "board", (-15.6, 3.55, 1.4), 0.0, topic="rules")
    B.spot("rules_stand", "stand", (-15.6, 2.8, 0), 180)
    kit.poster(B, -9.8, 2.69, 0.0, 0.0, name="board_3", w=0.42, h=0.55, zc=1.55)
    B.thing("tastes_board", "board", (-9.8, 2.62, 1.55), 0.0, topic="catalogue")
    B.spot("tastes_stand", "stand", (-9.8, 1.7, 0), 180)
    lbox(d, "CT_TileWhite", Fr(-9.47, 3.1), -0.012, 0.012, -0.05, 0.05, 1.2, 1.32, faces="xXyYZz")
    lbox(d, "CT_SteelRed", Fr(-9.47, 3.1), -0.02, 0.02, -0.012, 0.012, 1.245, 1.275, faces="xXyYZ")
    B.thing("office_switch", "machine", (-9.4, 3.1, 1.26), 90.0, what="switch", switch="hall")
    B.spot("switch_stand", "stand", (-8.8, 3.3, 0), 270)
    # the knife grinder
    _grinder(B, -10.4, 12.6)
    B.thing("knife_grinder", "craft", (-10.4, 12.6, 1.0), 0.0, what="grinder")
    B.spot("grinder_work", "work", (-10.4, 13.6, 0), 0)
    B.spot("grinder_customer", "stand", (-10.4, 11.6, 0), 0)
    _hang_lamp(B, -10.4, 12.6, ums)
    # herbs and flowers
    _herb_corner(B, -19.9, 21.6)
    B.thing("herb_corner", "shelf", (-19.9, 21.6, 0.75), 270.0, topic="herbs")
    B.spot("herb_stand", "stand", (-18.6, 21.6, 0), 90)
    _hang_lamp(B, -19.4, 21.6, ums)
    # the cat on its crate and its bowl; the aisle's pigeons by the bench
    fu.crate(B, 0.9, 16.4, 0.0, 0.55, 0.4, 0.4, "CT_WoodLight", fill="CT_Paper", yaw=10.0)
    _cat2(B, 0.9, 16.4, 0.4, 200.0)
    cyl(d, "CT_PlasterRed", 0.15, 16.7, 0.0, 0.06, 0.1, 8, top=False, bottom=True)
    cyl(d, "CT_PlasterYellow", 0.15, 16.7, 0.04, 0.045, 0.085, 8, top=True)
    cyl(d, "CT_TileWhite", -0.1, 16.5, 0.0, 0.04, 0.085, 8, top=True)
    B.thing("cat_bowl", "board", (0.5, 16.55, 0.3), 0.0, topic="cat")
    B.spot("cat_stand", "stand", (0.3, 15.7, 0), 0)
    _hang_lamp(B, 0.5, 16.9, ums)
    _hang_lamp(B, 0.35, 25.2, ums)          # over the pigeons' bench
    _hang_lamp(B, 10.4, 6.5, ums)           # over the tea tables
    pr = random.Random(5)
    for k in range(5):
        a = 2 * math.pi * k / 5
        _bird(B, 0.35 + 0.46 * math.cos(a) * (0.6 + 0.4 * pr.random()), 26.7 + 0.46 * math.sin(a) * (0.6 + 0.4 * pr.random()),
              math.degrees(a) + 180 + pr.uniform(-40, 40), "CT_ConcreteDark" if k % 3 else "CT_Steel", head_up=0.02 * (k % 2), peck=k % 2 == 1)
    for k in range(9):
        blob(d, "CT_WoodLight" if k % 2 else "CT_PlasterYellow", 0.35 + pr.uniform(-0.25, 0.25), 26.7 + pr.uniform(-0.25, 0.25), 0.006, 0.012, 4)
    B.thing("pigeon_crumbs", "board", (0.35, 26.7, 0.2), 0.0, topic="pigeons")
    B.spot("pigeon_stand", "stand", (0.35, 25.7, 0), 0)
    # the complaint book by the office, the day board at the entrance, the tea stand's radio
    _complaint_stand(B, -12.3, 2.5)
    B.thing("complaint_book", "board", (-12.3, 2.5, 1.07), 0.0, topic="complaints")
    kit.notice_stand(B, 0.0, 2.2, 0.0, 0.0, w=1.4, h=0.95, front="board_1", back="board_2", seed=3)
    B.thing("day_board", "board", (0.0, 2.2, 1.4), 0.0, topic="fair")
    B.spot("day_board_stand", "stand", (0.0, 1.5, 0), 0)
    kit.radio(B, 11.35, 3.55, 1.05, 0.0)
    B.thing("tea_radio", "machine", (11.35, 3.55, 1.2), 0.0, what="radio")
    # the fishmongers' trays
    B.thing("catch_6", "board", (-5.9, 20.3, 1.15), 0.0, topic="catch")
    B.thing("catch_11", "board", (2.5, 17.1, 1.15), 270.0, topic="catch")
    _rows(B)


def build(B):
    rnd = random.Random(1955)
    tops = []
    ums = {}
    rg = random.Random(77)               # goods use their own generator so the shells keep their shapes
    for i in range(NX):
        for j in range(NY):
            cx = (i - (NX - 1) / 2) * PITCH
            cy = Y0 + j * PITCH
            zc = 2.5 + rnd.uniform(0.0, 1.0)
            tops.append(umbrella(B, cx, cy, zc, rnd))
            ums[(i, j)] = (cx, cy, zc, tops[-1] - zc)
    # floor slab
    hx, y_a, y_b = NX * PITCH / 2 + 0.6, 1.2, Y0 + (NY - 1) * PITCH + L + 1.0
    B.slab([(-hx, y_a), (hx, y_a), (hx, y_b), (-hx, y_b)], -0.3, 0.0, "CT_Paving")
    # stalls: one per umbrella (some along y), plus a second one for the middle row
    n = 0
    for i in range(NX):
        for j in range(NY):
            cx = (i - (NX - 1) / 2) * PITCH
            cy = Y0 + j * PITCH
            variants = [(cx, cy + 2.7, True)]
            if j == 1:
                variants.append((cx + (2.7 if i % 2 else -2.7), cy - 0.2, False))
            for (x, y, ax) in variants:
                n += 1
                kind = KINDS[(n - 1) % len(KINDS)]
                if ax:
                    fr = Fr(x, y, 0.0)
                else:
                    fr = Fr(x, y, -90 if x > cx else 90)
                stall_goods(B, fr, kind, rg, n)
                zu = z_under(cx, cy, ums[(i, j)][2], ums[(i, j)][3], x, y)
                zl = max(2.62, zu - 0.4)
                B.lamp((x, y, zl), lumens=820, colour="#FFD2A0", kind="public", radius=8.0)
                lbox(B.detail, "CT_Steel", Fr(x, y), -0.008, 0.008, -0.008, 0.008, zl + 0.08, zu + 0.02, faces="xXyY")
                awning(B, x, y, fr.yaw % 360, kind, n)
                stall_extra(B, fr, n, kind, rg)
                stall_depth(B, fr, n, kind, rg)
                stall_things(B, fr, n, kind)
                _slate_thing(B, fr, n, kind)
                svc, shop = SHOPS[n]
                if ax:
                    kit.shop_counter(B, x, y, 0.0, 0.0, w=3.0, d=0.84, h=ZT, mat="CT_Concrete", top="CT_WoodLight", front=None, till=False)
                    B.spot("counter%d" % n, "counter", (x, y - 1.0, 0), 180)
                    B.spot("stall%d" % n, "stall", (x, y + 1.0, 0), 0)
                    face = 0.0
                else:
                    s = 1 if x > cx else -1
                    face = 270.0 if s > 0 else 90.0
                    kit.shop_counter(B, x, y, 0.0, face, w=3.0, d=0.84, h=ZT, mat="CT_Concrete", top="CT_WoodLight", front=None, till=False)
                    B.spot("counter%d" % n, "counter", (x - 1.0 * s, y, 0), 90 if s > 0 else 270)
                    B.spot("stall%d" % n, "stall", (x + 1.0 * s, y, 0), 270 if s > 0 else 90)
                props = {"service": svc}
                if shop:
                    props["shop"] = shop
                else:
                    props["role"] = svc
                B.thing("counter%d" % n, "counter", (x, y, ZT), face, **props)
    # benches at the corners of the grid
    k = 0
    for i in range(NX + 1):
        for j in range(NY):
            if (i + j) % 2:
                continue
            x = (i - NX / 2) * PITCH
            if abs(x) > hx - 1:
                x = math.copysign(hx - 1.5, x)
            y = Y0 + j * PITCH - 3.4
            r = kit.bench(B, x, y, 0.0, 180.0, w=1.8, kind="park")
            k += 1
            B.spot("bench%d" % k, "bench", (x, y + 0.5, 0), 180)
            B.thing("seat_bench_%d" % k, "seat", (x, y, 0.45), 180.0, height=0.45)
    _extras(B, hx)
    _corners(B, ums)
    B.footprint = [(-hx, y_a), (hx, y_a), (hx, y_b), (-hx, y_b)]
    B.height = max(tops) + 0.2
    B.note("%d umbrellas, %d stalls, %d benches" % (NX * NY, n, k))


def _extras(B, hx):
    """Market office, price boards, the tea stand, scales, litter bins."""
    d = B.detail
    # market office (a kiosk by the street edge, west) and the notice boards beside it
    kit.ticket_booth(B, -10.4, 3.4, 0.0, 0.0, w=1.8, d=1.4, mat="CT_PlasterBlue", sign="sign_till")
    B.thing("market_office", "counter", (-10.4, 2.7, 1.0), 0.0, service="reception")
    B.spot("office_window", "counter", (-10.4, 1.9, 0), 180)
    _directory(B, -7.2, 3.6, 0.0)
    B.thing("price_board", "board", (-7.2, 3.6, 1.45), 0.0, topic="prices")
    _directory(B, -13.6, 3.6, 0.0)
    B.thing("rules_board", "board", (-13.6, 3.6, 1.45), 0.0, topic="rules")
    kit.scales(B, -8.7, 2.5, 0.0, 0.0)
    B.thing("market_scales", "machine", (-8.7, 2.5, 1.0), 0.0, what="scales")
    # tea stand (east): counter with samovars, a few round tables and stools
    kit.shop_counter(B, 10.4, 3.2, 0.0, 0.0, w=2.6, d=0.7, h=1.05, mat=kit.WOODL, top="CT_TileWhite", front="CT_PlasterRed", till=True)
    kit.samovar(B, 9.5, 3.3, 1.05, 0.0)
    kit.samovar(B, 10.2, 3.3, 1.05, 0.0)
    kit.teapot(B, 11.0, 3.2, 1.05, 0.0)
    kit.cup(B, 9.2, 3.0, 1.05, 0.0)
    kit.cup(B, 9.5, 2.95, 1.05, 0.0)
    kit.plates(B, 10.6, 2.95, 1.05, 0.0, n=3)
    kit.sign_plate(B, 10.4, 4.0, 0.0, 0.0, name="sign_buffet", w=1.4, zc=2.5, hang=True)
    kit.menu_board(B, 12.0, 3.05, 1.05, 0.0, name="menu_1", w=0.4, zc=0.3)
    B.spot("tea_counter", "counter", (10.4, 2.1, 0), 180)
    B.spot("tea_seller", "work", (10.4, 4.0, 0), 0)
    B.thing("tea_counter", "counter", (10.4, 3.2, 1.05), 0.0, service="bar")
    for k, (tx, ty) in enumerate(((7.0, 6.2), (13.8, 6.2), (10.4, 7.0))):
        t = kit.table_round(B, tx, ty, 0.0, 0.0, r=0.42, h=0.74)
        kit.cup(B, tx - 0.1, ty, t["top"], 0.0)
        kit.plates(B, tx + 0.1, ty + 0.08, t["top"], 0.0, n=1)
        for j, (dx, dy, yw) in enumerate(((-0.6, 0.0, 90.0), (0.6, 0.0, 270.0))):
            r = kit.stool(B, tx + dx, ty + dy, 0.0, yw, h=0.45)
            if j == 0:
                B.thing("tea_seat_%d" % (k + 1), "seat", (tx + dx, ty + dy, 0.45), yw, height=0.45)
    kit.sandwich_board(B, 8.3, 2.0, 0.0, 0.0, w=0.5, name="menu_1")
    B.thing("tea_notice", "board", (8.3, 1.95, 0.75), 0.0, topic="menu")
    B.thing("tea_chat", "counter", (9.2, 3.25, 1.05), 0.0, service="talk", role="seller", trade="tea")
    B.spot("tea_chat_stand", "stand", (8.9, 2.1, 0), 180)
    B.spot("tea_notice_stand", "stand", (8.3, 1.2, 0), 180)
    kit.newspaper(B, 11.7, 3.1, 1.05, 12.0, seed=1)
    B.thing("tea_paper", "take", (11.7, 3.1, 1.05), 0.0, item="daily_paper")
    # a second scales + litter bins at the columns
    kit.scales(B, 5.9, 2.4, 0.0, 0.0)
    B.thing("market_scales_2", "machine", (5.9, 2.4, 1.0), 0.0, what="scales")
    for (bx, by) in ((-5.2, 12.0), (5.2, 12.0), (-15.6, 22.6), (15.6, 22.6), (0.0, 33.0)):
        cyl(d, "CT_SteelRed", bx + 0.9, by, 0.0, 0.7, 0.2, 8, top=False, bottom=True)
        cyl(d, "CT_Steel", bx + 0.9, by, 0.7, 0.74, 0.215, 8, top=True)
    # QA dressing: the wide aisles were bare paving - crate and sack piles, pot plants
    for k, (px, py) in enumerate(((-10.4, 20.0), (10.4, 20.0), (-20.0, 11.5), (20.0, 11.5), (0.0, 13.6), (0.0, 30.4))):
        kit.crate(B, px, py, 0.0, 12.0 * k, w=0.55, d=0.38, h=0.3, fill=("CT_PlasterRed", "CT_PlasterYellow", "CT_Leaf")[k % 3], seed=k)
        kit.crate(B, px + 0.62, py + 0.1, 0.0, -8.0 * k, w=0.55, d=0.38, h=0.3, fill="CT_PlasterGreen", seed=k + 9)
        kit.sack(B, px - 0.55, py + 0.1, 0.0, 40.0 * k, mat="CT_Paper" if k % 2 else "CT_PlasterYellow")
    for k, (px, py) in enumerate(((-10.4, 31.0), (10.4, 31.0), (-20.4, 28.0), (20.4, 28.0))):
        kit.plant(B, px, py, 0.0, 0.0, kind=("ficus", "palm")[k % 2], pot="CT_Concrete", h=1.3)
