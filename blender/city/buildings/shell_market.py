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


def build(B):
    rnd = random.Random(1955)
    tops = []
    for i in range(NX):
        for j in range(NY):
            cx = (i - (NX - 1) / 2) * PITCH
            cy = Y0 + j * PITCH
            zc = 2.5 + rnd.uniform(0.0, 1.0)
            tops.append(umbrella(B, cx, cy, zc, rnd))
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
