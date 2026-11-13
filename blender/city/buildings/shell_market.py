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
ZT = 1.03          # counter top


def stall_goods(B, fr, kind, rnd, n):
    d = B.detail
    if kind == "fish":
        lbox(d, "CT_Steel", fr, -1.4, 1.4, -0.36, 0.36, ZT, ZT + 0.05)
        for (u0, u1, w0, w1) in ((-1.4, 1.4, -0.36, -0.32), (-1.4, 1.4, 0.32, 0.36), (-1.4, -1.36, -0.36, 0.36), (1.36, 1.4, -0.36, 0.36)):
            lbox(d, "CT_Steel", fr, u0, u1, w0, w1, ZT + 0.05, ZT + 0.12)
        lbox(d, "CT_TileWhite", fr, -1.34, 1.34, -0.31, 0.31, ZT + 0.05, ZT + 0.11)
        for k in range(12):
            u, w = -1.2 + 0.22 * k + rnd.uniform(-0.04, 0.04), rnd.uniform(-0.2, 0.2)
            f2 = fr.sub(u, w, rnd.uniform(-30, 30) + (90 if k % 2 else 0))
            lbox(d, rnd.choice(FISH), f2, -0.15, 0.15, -0.045, 0.045, ZT + 0.11, ZT + 0.17, faces="xXyYZ")
            lbox(d, "CT_SteelRed", f2, 0.15, 0.19, -0.04, 0.04, ZT + 0.11, ZT + 0.15, faces="xXyYZ")
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
                for j in range(4):
                    qx, qy, _ = fr.p(u + rnd.uniform(-0.17, 0.17), rnd.uniform(-0.17, 0.17))
                    blob(d, c, qx, qy, ZT + 0.2, 0.07, 6)
        else:
            for k in range(3):
                px, py, _ = fr.p(-0.95 + 0.95 * k, 0.0)
                fu.pyramid(B, px, py, ZT, 3, 0.12, None, rnd, ["CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterGreen"] if k != 1 else ["CT_PlasterYellow", "CT_SteelRed"])
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
                for j in range(5):
                    lbox(d, rnd.choice(LOAF), fr.sub(-0.7 + 1.4 * k + (j - 2) * 0.2, 0.1), -0.08, 0.08, -0.12, 0.12,
                         ZT + 0.3 * t + 0.13, ZT + 0.3 * t + 0.22, faces="xXyYZ")
            for u in (-0.55, 0.55):
                lbox(d, "CT_WoodLight", fr.sub(-0.7 + 1.4 * k + u, 0.1), -0.02, 0.02, -0.2, 0.2, ZT, ZT + 0.42)
        px, py, _ = fr.p(0.0, -0.05)
        fu.crate(B, px, py, ZT, 0.45, 0.38, 0.12, "CT_WoodLight", fill="CT_PlasterYellow", yaw=fr.yaw)
        for k in range(3):     # trays of loaves on a rack behind
            lbox(d, "CT_Steel", fr, -1.2 + 1.2 * k, -0.4 + 1.2 * k, -1.0, -0.7, 0.55 + 0.4 * (k % 2), 0.58 + 0.4 * (k % 2))
            for j in range(3):
                lbox(d, rnd.choice(LOAF), fr, -1.15 + 1.2 * k + j * 0.25, -0.95 + 1.2 * k + j * 0.25, -0.97, -0.73,
                     0.58 + 0.4 * (k % 2), 0.66 + 0.4 * (k % 2), faces="xXyYZ")
    elif kind == "flowers":
        for k in range(6):
            px, py, _ = fr.p(-1.25 + 0.5 * k, rnd.uniform(-0.1, 0.1))
            fu.bucket(B, px, py, ZT, 0.13, 0.24, rnd.choice(("CT_Steel", "CT_PlasterBlue", "CT_TileWhite")), fu.FLOWERS, rnd)
    elif kind == "dairy":
        for k in range(3):     # milk cans with lids, on the floor and the counter
            px, py, _ = fr.p(-1.3 + 0.38 * k, -0.9)
            cyl(d, "CT_Steel", px, py, 0.0, 0.55, 0.14, 8, top=True)
            cyl(d, "CT_PlasterBlue", px, py, 0.55, 0.6, 0.15, 8, top=True)
        for k in range(4):     # cheese wheels, stacked
            px, py, _ = fr.p(-1.05 + 0.5 * (k % 2), 0.1 + 0.0 * k)
            for t in range(2 + k % 2):
                cyl(d, "CT_PlasterYellow", px + 0.9 * (k // 2), py, ZT + 0.1 * t, ZT + 0.1 * t + 0.09, 0.2, 8, top=True)
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
            lbox(d, ("CT_SteelRed", "CT_WoodLight", "CT_SteelRed")[k % 3], f2, -0.035, 0.035, -0.035, 0.035, 1.32 - 0.1 * (k % 2),
                 1.93, faces="xXyYz")
        hexa_u = fr.sub(0.0, -0.6)
        lbox(d, "CT_PlasterPink", hexa_u, -0.17, 0.17, -0.1, 0.1, 1.05, 1.9, faces="xXyYz")
        lbox(d, "CT_SteelRed", hexa_u, -0.1, 0.1, -0.105, 0.105, 1.3, 1.6, faces="y")
    elif kind == "birds":
        for k in range(3):     # open sacks of seed on the counter
            px, py, _ = fr.p(-1.15 + 0.5 * k, 0.05)
            kit.sack(B, px, py, ZT, rnd.uniform(0, 360), mat="CT_Paper" if k else "CT_PlasterYellow")
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


def awning(B, x, y, yaw, kind):
    """A striped awning over a stall, on two posts behind the counter, struts to the front edge (the lamp is above it)."""
    fr = Fr(x, y, yaw)
    mats = AWN[kind]
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
                awning(B, x, y, fr.yaw % 360, kind)
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
    kit.newspaper(B, 11.7, 3.1, 1.05, 12.0, seed=1)
    B.thing("tea_paper", "take", (11.7, 3.1, 1.05), 0.0, item="daily_paper")
    # a second scales + litter bins at the columns
    kit.scales(B, 5.9, 2.4, 0.0, 0.0)
    B.thing("market_scales_2", "machine", (5.9, 2.4, 1.0), 0.0, what="scales")
    for (bx, by) in ((-5.2, 12.0), (5.2, 12.0), (-15.6, 22.6), (15.6, 22.6), (0.0, 33.0)):
        cyl(d, "CT_SteelRed", bx + 0.9, by, 0.0, 0.7, 0.2, 8, top=False, bottom=True)
        cyl(d, "CT_Steel", bx + 0.9, by, 0.7, 0.74, 0.215, 8, top=True)
