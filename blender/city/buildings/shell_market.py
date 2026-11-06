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
KINDS = ["fish", "veg", "fruit", "bread", "flowers", "veg", "fruit", "fish", "bread", "veg", "flowers", "fruit",
         "veg", "fish", "bread", "fruit"]
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
    elif kind == "veg":
        for k in range(5):
            u = -1.2 + 0.6 * k
            c = rnd.choice(VEG)
            px, py, _ = fr.p(u, 0.0)
            fu.crate(B, px, py, ZT, 0.5, 0.5, 0.16, "CT_WoodLight", fill=c, yaw=fr.yaw)
            for j in range(4):
                qx, qy, _ = fr.p(u + rnd.uniform(-0.17, 0.17), rnd.uniform(-0.17, 0.17))
                blob(d, c, qx, qy, ZT + 0.2, 0.07, 6)
        for k in range(2):     # crates on the floor at the ends
            px, py, _ = fr.p(-1.85 + 3.7 * k, 0.0)
            fu.crate(B, px, py, 0.0, 0.5, 0.5, 0.25, "CT_WoodLight", fill=rnd.choice(VEG), yaw=fr.yaw)
    elif kind == "fruit":
        for k in range(3):
            px, py, _ = fr.p(-0.95 + 0.95 * k, 0.0)
            fu.pyramid(B, px, py, ZT, 3, 0.12, None, rnd, ["CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterGreen"] if k != 1 else ["CT_PlasterYellow", "CT_SteelRed"])
        for k in range(2):
            px, py, _ = fr.p(-1.4 + 2.8 * k, 0.0)
            fu.crate(B, px, py, ZT, 0.4, 0.45, 0.12, "CT_WoodLight", fill=rnd.choice(VEG), yaw=fr.yaw)
    elif kind == "bread":
        for k in range(2):      # a two-tier wooden rack of loaves
            px, py, _ = fr.p(-0.7 + 1.4 * k, 0.1)
            for t in range(2):
                lbox(d, "CT_WoodLight", fr.sub(-0.7 + 1.4 * k, 0.1), -0.55, 0.55, -0.2, 0.2, ZT + 0.3 * t + 0.1, ZT + 0.3 * t + 0.13)
                for j in range(5):
                    lbox(d, rnd.choice(LOAF), fr.sub(-0.7 + 1.4 * k + (j - 2) * 0.2, 0.1), -0.08, 0.08, -0.12, 0.12,
                         ZT + 0.3 * t + 0.13, ZT + 0.3 * t + 0.22, faces="xXyYZ")
            for u in (-0.55, 0.55):
                lbox(d, "CT_WoodLight", fr.sub(-0.7 + 1.4 * k + u, 0.1), -0.02, 0.02, -0.2, 0.2, ZT, ZT + 0.42)
        px, py, _ = fr.p(0.0, -0.05)
        fu.crate(B, px, py, ZT, 0.45, 0.38, 0.12, "CT_WoodLight", fill="CT_PlasterYellow", yaw=fr.yaw)
    elif kind == "flowers":
        for k in range(6):
            px, py, _ = fr.p(-1.25 + 0.5 * k, rnd.uniform(-0.1, 0.1))
            fu.bucket(B, px, py, ZT, 0.13, 0.24, rnd.choice(("CT_Steel", "CT_PlasterBlue", "CT_TileWhite")), fu.FLOWERS, rnd)
        for k in range(2):
            px, py, _ = fr.p(-1.85 + 3.7 * k, 0.0)
            fu.bucket(B, px, py, 0.0, 0.2, 0.42, "CT_Steel", fu.FLOWERS, rnd)
    # hanging scale on a post at the right end of the counter (bread and flowers sell by the bunch)
    if kind in ("fish", "veg", "fruit"):
        post = fr.sub(1.38, 0.28)
        lbox(d, "CT_Steel", post, -0.02, 0.02, -0.02, 0.02, ZT, ZT + 0.85)
        lbox(d, "CT_Steel", post, -0.7, 0.04, -0.02, 0.02, ZT + 0.85, ZT + 0.89)
        lbox(d, "CT_SteelRed", post, -0.74, -0.66, -0.03, 0.03, ZT + 0.62, ZT + 0.85)
        cyl(d, "CT_Aluminium", *post.p(-0.7, 0.0)[:2], ZT + 0.5, ZT + 0.53, 0.14, 8)
        lbox(d, "CT_Steel", post.sub(-0.7, 0.0), -0.004, 0.004, -0.004, 0.004, ZT + 0.53, ZT + 0.85, faces="xXyY")
    # price board: a slate with coloured tags on the counter's back edge
    bd = fr.sub(-1.15, 0.31)
    lbox(d, "CT_ConcreteDark", bd, -0.28, 0.28, -0.02, 0.02, ZT + 0.04, ZT + 0.42)
    for k in range(3):
        lbox(d, rnd.choice(("CT_TileWhite", "CT_PlasterYellow", "CT_PlasterPink")), bd, -0.22, 0.2, -0.04, -0.015,
             ZT + 0.1 + 0.1 * k, ZT + 0.16 + 0.1 * k, faces="xXyYZ")
    lbox(d, "CT_WoodLight", bd, -0.3, 0.3, -0.03, 0.03, ZT, ZT + 0.04)


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
                zl = max(2.25, zu - 0.45)
                B.lamp((x, y, zl), lumens=820, colour="#FFD2A0", kind="public", radius=8.0)
                lbox(B.detail, "CT_Steel", Fr(x, y), -0.008, 0.008, -0.008, 0.008, zl + 0.08, zu + 0.02, faces="xXyY")
                if ax:
                    box(B.m, "CT_Concrete", x - 1.5, x + 1.5, y - 0.35, y + 0.35, 0, 0.95)
                    box(B.m, "CT_Wood", x - 1.58, x + 1.58, y - 0.42, y + 0.42, 0.95, 1.03)
                    B.spot("counter%d" % n, "counter", (x, y - 1.0, 0), 180)
                    B.spot("stall%d" % n, "stall", (x, y + 1.0, 0), 0)
                else:
                    box(B.m, "CT_Concrete", x - 0.35, x + 0.35, y - 1.5, y + 1.5, 0, 0.95)
                    box(B.m, "CT_Wood", x - 0.42, x + 0.42, y - 1.58, y + 1.58, 0.95, 1.03)
                    s = 1 if x > cx else -1
                    B.spot("counter%d" % n, "counter", (x - 1.0 * s, y, 0), 90 if s > 0 else 270)
                    B.spot("stall%d" % n, "stall", (x + 1.0 * s, y, 0), 270 if s > 0 else 90)
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
            bench(B, x, y, True)
            k += 1
            B.spot("bench%d" % k, "bench", (x, y + 0.5, 0), 180)
    B.footprint = [(-hx, y_a), (hx, y_a), (hx, y_b), (-hx, y_b)]
    B.height = max(tops) + 0.2
    B.note("%d umbrellas, %d stalls, %d benches" % (NX * NY, n, k))
