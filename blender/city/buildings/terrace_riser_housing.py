"""Terrace riser housing: stepped flats set into the retaining wall between two terraces, split by a public
open-air concrete stair. An original design in the spirit of 1970s-80s Japanese hillside concrete-terrace housing
crossed with stacked-box ideas (not a copy of any building).

Size 40 m (x) x 18 m (y, into the hill) x ~12.5 m. Front (y=0) stands on the lower terrace (z 0); the back meets the
upper terrace at z +7.0.
  * 4 tiers of flats going inland, floors at z 0 / 2.4 / 4.7 / 7.0. The roof of tier k is the garden terrace of tier
    k+1 (paving, planter beds with shrubs, railings). Tier 0: 3 flats a side, tier 1: 3, tier 2: 3, tier 3: 2.
  * One upper storey (z 9.33..11.9) over tier 3: one big maisonette a side, cantilevered 2.2 m over the top garden,
    roof deck with planters and a timber pergola; entered from a short side stair rising from the upper terrace.
  * Central public stair, 3.8 m wide, climbs from the street (z 0, y 0) to the upper terrace (z 7.0, y 18) in three
    flights (rise 0.16 m) with landings that touch the garden terraces; rise <= 0.17, going 0.28. Walkable.
  * Raw board-marked concrete, warm timber slat screens before the windows, coloured door canopies and surrounds.
Entrances: "stair_bottom" (street), "stair_top" (back), a flat_door per flat. Flats are not enterable.
Spots: benches on the stair landings.
Ideas for later: lit stair lamps, bicycle racks on the gardens, washing lines, vending machines on the landings.
Material requests: none.
"""
import math
import random

import interior_kit as kit
from citykit import box, prism, extrude_profile, DOOR_W, DOOR_H
from _surrounds import add_surrounds
import _homekit as hk

INFO = {
    "name_ru": "Террасный дом у лестницы", "name_en": "Terrace Riser Housing",
    "style": "brutalism / metabolism", "use": "residential",
    "inspired_by": "Japanese hillside concrete-terrace housing of the 1970s-80s (stepped gardens, public stairs "
                   "between terraces) crossed with stacked-box ideas",
}

XE = 20.0            # half width
SX = 1.9             # half width of the stair
F = [0.0, 2.4, 4.7, 7.0]             # floor of every tier = garden level of the previous roof
ZT = [2.4, 4.7, 7.0, 9.33]           # top of every tier
B_Y = [0.0, 5.2, 10.12, 15.04, 18.0]
WALL = 0.3
GOING = 0.28
UPPER_Z0, UPPER_Z1 = 9.33, 11.93
UPPER_Y0 = 12.8
WING_X = 17.6


def top(m, mat, x0, x1, y0, y1, z):
    m.face(mat, [(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)])


def _stair(B):
    """Central stair: side profile (y, z) extruded across x -SX..SX; three flights and landings."""
    pts = [(0.0, -0.3), (0.0, 0.0)]
    y, z = 0.0, 0.0
    flights = [(0, 15, 2.4, 1.0), (1, 14, 2.3, 1.0), (2, 14, 2.3, 18.0 - 14.04)]
    for _, n, rise, land in flights:
        r = rise / n
        for i in range(n):
            pts.append((y, z + r))
            pts.append((y + GOING, z + r))
            y += GOING
            z += r
        pts.append((y + land, z))
        y += land
    assert abs(y - 18.0) < 0.02 and abs(z - 7.0) < 1e-6, (y, z)
    pts[-1] = (18.0, 7.0)
    pts.append((18.0, -0.3))
    # remove duplicate points
    clean = []
    for p in pts:
        if not clean or math.hypot(p[0] - clean[-1][0], p[1] - clean[-1][1]) > 1e-6:
            clean.append(p)
    extrude_profile(B.m, "CT_Concrete", "CT_Concrete", clean, -SX, SX, lambda u, v, t: (t, u, v))


def _shrub(B, x, y, z, r, rnd):
    """A low dome of foliage: three rings (bottom, belly, crown) and a cap."""
    segs = 8
    h = r * rnd.uniform(1.0, 1.5)
    rings = [(r * 0.55, 0.0), (r, h * 0.45), (r * 0.7, h * 0.8), (r * 0.25, h)]
    ph = rnd.random()
    pts = [[(x + rr * math.cos(2 * math.pi * k / segs + ph), y + rr * math.sin(2 * math.pi * k / segs + ph), z + hh)
            for k in range(segs)] for rr, hh in rings]
    for a, b in zip(pts, pts[1:]):
        for k in range(segs):
            k2 = (k + 1) % segs
            B.detail.face("CT_Grass", [a[k], a[k2], b[k2], b[k]])
    B.detail.face("CT_Grass", list(pts[-1]))


def _planter(B, x0, x1, y0, y1, z, rnd, h=0.5):
    box(B.m, "CT_ConcreteDark", x0, x1, y0, y1, z, z + h, faces="xXyY")
    top(B.m, "CT_Grass", x0, x1, y0, y1, z + h)
    n = max(1, int((x1 - x0) / 1.1))
    for i in range(n):
        _shrub(B, x0 + (i + 0.5) * (x1 - x0) / n, (y0 + y1) / 2, z + h, rnd.uniform(0.35, 0.55), rnd)


def _slats(B, x0, x1, y, z0, z1, mat="CT_Wood", pitch=0.14):
    """Vertical timber slats standing before a window (detail), with top and bottom rails."""
    n = max(2, int((x1 - x0) / pitch))
    for i in range(n + 1):
        x = x0 + (x1 - x0) * i / n
        box(B.detail, mat, x - 0.025, x + 0.025, y - 0.08, y, z0, z1)
    box(B.detail, mat, x0 - 0.03, x1 + 0.03, y - 0.1, y, z0 - 0.06, z0)
    box(B.detail, mat, x0 - 0.03, x1 + 0.03, y - 0.1, y, z1, z1 + 0.06)


DOOR_COLOURS = ["CT_SteelRed", "CT_PlasterYellow", "CT_PlasterBlue", "CT_Wood", "CT_PlasterRed", "CT_PlasterPink"]
# ---- per-plot variant parameters
OPEN_SIDE = 1               # the maisonette that is open inside: +1 east
RESIDENT = "astronomer"     # "astronomer" (telescope, star charts) | "seamstress" (sewing machine, fabrics)
SHR = []                    # shrub positions of the gardens (x, y, z), so the garden furniture keeps off them
SLAT = "CT_Wood"            # window screens (slat screens and sliding panels)
SEED = 1978                 # shrubs and planters
VARIANTS = {
    "P-RISER1": {},
    "P-RISER2": {"SLAT": "CT_SteelWhite", "SEED": 2011, "RESIDENT": "seamstress",
                 "DOOR_COLOURS": ["CT_PlasterBlue", "CT_PlasterGreen", "CT_PlasterPink", "CT_Steel", "CT_PlasterYellow",
                                  "CT_Copper"]},
}


def _door_dressing(B, x, y, z, colour):
    """A coloured canopy and surround around a door hole of DOOR_W at x on the wall y (facing -y)."""
    hw = DOOR_W / 2
    box(B.detail, colour, x - hw - 0.12, x + hw + 0.12, y - 0.9, y, z + DOOR_H + 0.12, z + DOOR_H + 0.2)
    for sx in (-1, 1):
        box(B.detail, colour, x + sx * (hw + 0.04) - 0.04, x + sx * (hw + 0.04) + 0.04, y - 0.05, y, z, z + DOOR_H + 0.12)


def build(B):
    rnd = random.Random(SEED)
    del SHR[:]
    m = B.m
    # ---------------------------------------------------------------- the stair
    _stair(B)

    flat_no = {}
    outer_flat = {}

    def new_flat(storey):
        flat_no[storey] = flat_no.get(storey, 0) + 1
        return "%d-%02d" % (storey, flat_no[storey])

    # ---------------------------------------------------------------- tiers 0..3: mass, front walls, flats
    counts = [3, 3, 3, 2]
    for k in range(4):
        y0, y1 = B_Y[k], B_Y[k + 1]
        f, zt = F[k], ZT[k]
        xmax = XE
        for side in (1, -1):
            xa, xb = (SX, XE) if side == 1 else (-XE, -SX)
            # solid mass (flat interiors are not modelled): front face closes the window holes
            if k < 3:
                box(m, "CT_Concrete", xa, xb, y0 + WALL, y1, 0.0, zt, faces="xXy")
                top(m, "CT_Paving", xa, xb, y0, y1, zt)
            else:
                # tier 3: the base under the upper terrace level, then the two flats' wing up to 9.33
                box(m, "CT_Concrete", xa, xb, y0 + WALL, y1, 0.0, F[3], faces="xXyY")
                top(m, "CT_Paving", xa, xb, y0 + WALL, y1, F[3])
                top(m, "CT_Paving", xa, xb, y0, y0 + WALL, F[3])
                wx0, wx1 = (SX, WING_X) if side == 1 else (-WING_X, -SX)
                box(m, "CT_Concrete", wx0, wx1, y0 + WALL, y1, F[3], zt, faces="xXyY")
            # front facade of the tier with the windows and the doors (for tier 3 only the wing)
            x_lo, x_hi = (xa, xb) if k < 3 else ((SX, WING_X) if side == 1 else (-WING_X, -SX))
            n = counts[k]
            wf = (x_hi - x_lo) / n
            holes = []
            cards = []
            doors = []
            for i in range(n):
                fx0 = x_lo + i * wf
                # alternate the door side to keep the front lively
                door_left = (i + k + (0 if side == 1 else 1)) % 2 == 0
                dx = fx0 + (0.55 + DOOR_W / 2) if door_left else fx0 + wf - 0.55 - DOOR_W / 2
                win_x0 = fx0 + (0.55 + DOOR_W + 0.55 if door_left else 0.45)
                win_x1 = fx0 + (wf - 0.45 if door_left else wf - (0.55 + DOOR_W + 0.55))
                wz0, wz1 = f + 0.45, f + 1.85
                mid_ = win_x0 + (win_x1 - win_x0) * 0.62
                holes.append((win_x0 - x_lo, mid_ - 0.15 - x_lo, wz0, wz1))
                holes.append((mid_ + 0.15 - x_lo, win_x1 - x_lo, wz0, wz1))
                holes.append((dx - DOOR_W / 2 - x_lo, dx + DOOR_W / 2 - x_lo, f, f + DOOR_H))
                cards.append((i, fx0, win_x0, win_x1, wz0, wz1, dx, door_left))
            F_ = B.facade((x_lo, y0), (x_hi, y0), f - 0.3, zt, "CT_Concrete", holes=holes, depth=WALL, back=False)
            for (i, fx0, win_x0, win_x1, wz0, wz1, dx, door_left) in cards:
                fid = new_flat(k)
                eid = "t%d%s%d" % (k, "e" if side == 1 else "w", i)
                B.flat(fid, storey=k, beds=2 if k % 2 == 0 else 3, door=(dx, y0 + 1.5, f), entrance=eid)
                if i == n - 1:
                    outer_flat[(k, side)] = fid
                mid_ = win_x0 + (win_x1 - win_x0) * 0.62
                B.facade_window(F_, win_x0 - x_lo, mid_ - 0.15 - x_lo, wz0, wz1, flat=fid, cols=2, rows=1)
                B.facade_window(F_, mid_ + 0.15 - x_lo, win_x1 - x_lo, wz0, wz1, flat=fid, cols=1, rows=1)
                B.entrance(eid, (dx, y0, f), 0.0, DOOR_W, DOOR_H, "flat_door")
                if not (k == 3 and side == OPEN_SIDE):       # (its canopy would poke 7 cm out of the open flat's floor slab)
                    _door_dressing(B, dx, y0, f, DOOR_COLOURS[(i + 2 * k + (0 if side == 1 else 3)) % len(DOOR_COLOURS)])
                # timber slat screen before part of the window; a coloured sliding panel before others
                span = win_x1 - win_x0
                if (i + k) % 3 != 1:
                    sx0, sx1 = (win_x0 + 0.1, win_x0 + span * 0.55) if (i + k) % 2 == 0 else (win_x1 - span * 0.55, win_x1 - 0.1)
                    _slats(B, sx0, sx1, y0 - 0.35, wz0 - 0.1, wz1 + 0.1, mat=SLAT)
                else:
                    box(B.detail, SLAT, win_x0 + 0.1, win_x0 + span * 0.5, y0 - 0.38, y0 - 0.33, wz0 - 0.1, wz1 + 0.1)
                # party-wall fins between the flats (thin concrete ribs projecting into the garden)
                if i > 0:
                    box(B.detail, "CT_Concrete", fx0 - 0.12, fx0 + 0.12, y0 - 0.5, y0, f, zt - 0.1)
            # roof edge: cap, railing and planter on top (the garden of the next tier)
            if k < 3:
                x_a, x_b = xa, xb
                box(B.detail, "CT_ConcreteDark", x_a, x_b, y0 - 0.3, y0 + 0.3, zt - 0.1, zt)
                # roof edge cap sits on the wall; its upper face is the garden top at zt
                B.railing([(x_a + 0.05, y0 + 0.15), (x_b - 0.05, y0 + 0.15)], zt, h=1.05)
                _planter(B, x_a + 0.2, x_b - 0.2, y0 + 0.45, y0 + 1.0, zt, rnd)
                # railing along the stair canyon (gate left at the landing that meets this garden)
                rx = (SX + 0.06) * side
                B.railing([(rx, y0 + 0.15), (rx, y1 - 1.1)], zt, h=1.05)
                # a bed along the end wall
                ex = xb - 0.15 if side == 1 else xa + 0.15
                B.railing([(ex, y0 + 0.15), (ex, y1)], zt, h=1.05)
                # shrubs and tubs scattered on the garden
                for _ in range(3):
                    gx = rnd.uniform(abs(xa) if side == 1 else abs(xb), 0) if False else rnd.uniform(min(abs(xa), abs(xb)) + 1.5, max(abs(xa), abs(xb)) - 1.5)
                    gx *= side
                    gy = rnd.uniform(y1 - 1.6, y1 - 0.9)
                    ok = True
                    _shrub(B, gx, gy, zt, rnd.uniform(0.3, 0.45), rnd)
                    SHR.append((gx, gy, zt))
            # side (end) wall facade with a window
            ex0, ex1 = (XE, XE)
            if k < 3:
                pass
        # end walls (x = +-XE) of the tier: facade with one window in the end flat
        for side in (1, -1):
            if k < 3:
                p0, p1 = ((XE * side, y0), (XE * side, y1)) if side == 1 else ((-XE, y1), (-XE, y0))
                length = y1 - y0
                u0, u1 = length * 0.5 - 0.7, length * 0.5 + 0.7
                Fe = B.facade(p0, p1, f - 0.3 if k else 0.0, zt, "CT_Concrete", holes=[(u0, u1, f + 0.5, f + 1.8)], depth=WALL, back=False)
                fid = outer_flat.get((k, side))
                if fid:
                    B.facade_window(Fe, u0, u1, f + 0.5, f + 1.8, flat=fid, cols=2, rows=1)

    # the mass of tier 0..2 wall bottoms for tier 0: the end facade starts at the ground
    # ---------------------------------------------------------------- upper storey over tier 3 + side stairs
    for side in (1, -1):
        wx0, wx1 = (SX, WING_X) if side == 1 else (-WING_X, -SX)
        # solid body (floor slab thickened to a deep cantilever beam at the front)
        if side == OPEN_SIDE:
            _open_body(B, wx0, wx1)
        else:
            box(m, "CT_Concrete", wx0, wx1, UPPER_Y0 + WALL, 18.0, UPPER_Z0 - 0.3, UPPER_Z1 - 0.25, faces="xXyY")
        box(m, "CT_Wood", wx0, wx1, UPPER_Y0, UPPER_Y0 + WALL, UPPER_Z0 - 0.3, UPPER_Z0 - 0.3, faces="")
        # soffit of the cantilever: timber ceiling under the overhang (faces down), y 12.8..15.04
        m.face("CT_Wood", [(wx0, UPPER_Y0, UPPER_Z0 - 0.3), (wx0, B_Y[3], UPPER_Z0 - 0.3), (wx1, B_Y[3], UPPER_Z0 - 0.3), (wx1, UPPER_Y0, UPPER_Z0 - 0.3)])
        # front-edge of the floor slab (the cantilever's nose)
        m.face("CT_ConcreteDark", [(wx0, UPPER_Y0, UPPER_Z0 - 0.3), (wx1, UPPER_Y0, UPPER_Z0 - 0.3), (wx1, UPPER_Y0, UPPER_Z0), (wx0, UPPER_Y0, UPPER_Z0)])
        # front facade of the maisonette: 4 big windows with slats
        fid = new_flat(4)
        B.flat(fid, storey=4, beds=4, door=(WING_X * side, 13.45, UPPER_Z0), entrance="u%s" % ("e" if side == 1 else "w"))
        wall_z1 = UPPER_Z1 - 0.25
        L = wx1 - wx0
        holes = []
        wins = []
        for j in range(4):
            u0 = 0.8 + j * (L - 1.6) / 4
            u1 = u0 + (L - 1.6) / 4 - 0.5
            holes.append((u0, u1, UPPER_Z0 + 0.4, UPPER_Z0 + 2.0))
            wins.append((u0, u1))
        Fu = B.facade((wx0, UPPER_Y0), (wx1, UPPER_Y0), UPPER_Z0, wall_z1, "CT_ConcreteWhite", holes=holes, depth=WALL, back=(side == OPEN_SIDE))
        for j, (u0, u1) in enumerate(wins):
            B.facade_window(Fu, u0, u1, UPPER_Z0 + 0.4, UPPER_Z0 + 2.0, flat=fid, cols=2, rows=1)
            if j % 2 == 0:
                _slats(B, wx0 + u0 + 0.1, wx0 + u1 - 0.1, UPPER_Y0 - 0.35, UPPER_Z0 + 0.3, UPPER_Z0 + 2.1, mat=SLAT)
        # back facade (towards the upper terrace) with two windows
        p0, p1 = ((wx1, 18.0), (wx0, 18.0))
        hb = [(1.2, 3.0, UPPER_Z0 + 0.8, UPPER_Z0 + 1.9), (7.0, 8.8, UPPER_Z0 + 0.8, UPPER_Z0 + 1.9)]
        Fb = B.facade(p0, p1, UPPER_Z0, wall_z1, "CT_ConcreteWhite", holes=hb, depth=WALL, back=(side == OPEN_SIDE))
        for u0, u1, h0, h1 in hb:
            B.facade_window(Fb, u0, u1, h0, h1, flat=fid, cols=1, rows=1)
        # end wall with the door from the side deck
        if side == 1:
            pe0, pe1 = (WING_X, UPPER_Y0), (WING_X, 18.0)
            ud = 13.45 - UPPER_Y0
        else:
            pe0, pe1 = (-WING_X, 18.0), (-WING_X, UPPER_Y0)
            ud = 18.0 - 13.45
        Fe = B.facade(pe0, pe1, UPPER_Z0, wall_z1, "CT_ConcreteWhite",
                      holes=[(ud - DOOR_W / 2, ud + DOOR_W / 2, UPPER_Z0, UPPER_Z0 + DOOR_H)], depth=WALL, back=(side == OPEN_SIDE))
        B.entrance("u%s" % ("e" if side == 1 else "w"), (WING_X * side, 13.45, UPPER_Z0), 90.0 * side, DOOR_W, DOOR_H, "flat_door")
        # coloured canopy over the end door
        box(B.detail, "CT_SteelRed" if side == 1 else "CT_PlasterYellow", WING_X * side + (0 if side == 1 else -0.9) , WING_X * side + (0.9 if side == 1 else 0), 12.85, 14.05, UPPER_Z0 + DOOR_H + 0.12, UPPER_Z0 + DOOR_H + 0.2)
        # roof: slab with overhang, parapet, planters, pergola
        top_z = UPPER_Z1
        box(m, "CT_ConcreteDark", wx0 - (0.0 if side == 1 else 0.3), wx1 + (0.3 if side == 1 else 0.0), UPPER_Y0 - 0.4, 18.0, UPPER_Z1 - 0.25, UPPER_Z1, faces="xXyYzZ")
        _roof_deck(B, wx0, wx1, UPPER_Z1, side, rnd)

        # side stair from the upper terrace (z 7) up to the deck beside the door (z 9.33), solid
        sx0, sx1 = (WING_X, XE) if side == 1 else (-XE, -WING_X)
        n = 14
        r = (UPPER_Z0 - F[3]) / n
        pts = [(18.0, F[3] - 0.3), (18.0, F[3])]
        y = 18.0
        z = F[3]
        # the stair descends in y: profile in (y, z) from y 18 forward (-y)
        for i in range(n):
            z += r
            pts.append((y, z))
            y -= GOING
            pts.append((y, z))
        deck_y0 = UPPER_Y0
        pts.append((deck_y0, UPPER_Z0))
        pts.append((deck_y0, F[3] - 0.3))
        clean = []
        for p in pts:
            if not clean or math.hypot(p[0] - clean[-1][0], p[1] - clean[-1][1]) > 1e-6:
                clean.append(p)
        # mirrored in y: extrude_profile wants any winding of (u, v); u = y, v = z, t = x
        extrude_profile(m, "CT_Concrete", "CT_Concrete", clean, sx0, sx1, lambda u, v, t: (t, u, v))
        # railing along the outer edge of the stair/deck and across the deck front
        ox = (XE - 0.08) * side
        B.railing([(ox, 18.0 - n * GOING), (ox, deck_y0 + 0.08), ((WING_X + 0.15) * side, deck_y0 + 0.08)], UPPER_Z0, h=1.05)
        # railing alongside the climbing steps (follows the slope, built as short posts and a rail)
        _slope_rail(B, ox, 18.0, F[3], n, r)

    # ---------------------------------------------------------------- retaining wall behind the stair top
    # the back of the stair meets the upper terrace: a little landing paving is the terrace's job
    # reach audit: a paved walk along the back edge at the upper terrace level joins the stair top to the two side
    # stairs' feet (y 18) even where the town's terrace ground does not quite meet the plot edge
    box(m, "CT_Paving", -XE, XE, 18.0, 19.2, F[3] - 0.15, F[3], faces="xXyYZ")

    # ---------------------------------------------------------------- stair railings are none (open canyon walls);
    # benches on the landings (concrete + timber), spots for the life simulation
    benches = [(1.0, 4.7), (2.3, 9.62), (4.7 + 0.0, 14.5)]
    for z, y in [(F[1], 4.7), (F[2], 9.62), (F[3], 14.5)]:
        for sgn in (-1,):
            bx = -SX + 0.45
            box(B.detail, "CT_Concrete", bx - 0.2, bx + 0.2, y - 0.4, y + 0.4, z, z + 0.4, faces="xXyYZ")
            box(B.detail, "CT_Wood", bx - 0.24, bx + 0.24, y - 0.44, y + 0.44, z + 0.4, z + 0.46)
            B.spot("bench_%d" % int(z * 10), "bench", (bx, y, z + 0.46), 90.0)
    # wood-clad stair entry portal pillars at the street end
    for sx in (-SX - 0.35, SX + 0.15):
        box(m, "CT_ConcreteDark", sx, sx + 0.2, -0.3, 0.1, 0.0, 3.0, faces="xXyYZ")
    B.entrance("stair_bottom", (0.0, 0.0, 0.0), 0.0, 3.8, 3.0, "street")
    B.entrance("stair_top", (0.0, 18.0, 7.0), 180.0, 3.8, 3.0, "street")
    _gardens(B)
    _maisonette(B)
    _stair_things(B)
    for w_ in kit.check(B):
        print("KIT riser", w_)
    B.footprint = [(-XE, -0.3), (XE, -0.3), (XE, 18.0), (-XE, 18.0)]
    B.height = UPPER_Z1 + 2.6
    B.note("stepped terrace housing: %d flats in 4 tiers + upper maisonettes; stair walkable (rise <= 0.17)" % len(B.flats))
    add_surrounds(B, kinds=("street", "flat_door"), max_z=8.0, bounds=(-XE, XE, -0.3, 18.0), seed=4)


def _slope_rail(B, x, y0, z0, n, r):
    """A rail that follows the slope of a stair descending in -y from (y0, z0): posts every 4 steps + sloped rail."""
    d = B.detail
    y1 = y0 - n * GOING
    zA, zB = z0 + 1.05 + r, z0 + n * r + 1.05
    for i in range(0, n + 1, 4):
        yy = y0 - i * GOING
        zz = z0 + (i + 1) * r
        box(d, "CT_Steel", x - 0.02, x + 0.02, yy - 0.02, yy + 0.02, zz, zz + 1.05)
    # sloped rail as a quad-bar
    for dx in (-0.02, 0.02):
        pass
    d.face("CT_Steel", [(x - 0.02, y0, zA), (x - 0.02, y1, zB), (x + 0.02, y1, zB), (x + 0.02, y0, zA)])
    d.face("CT_Steel", [(x + 0.02, y0, zA), (x + 0.02, y1, zB), (x - 0.02, y1, zB), (x - 0.02, y0, zA)])


def _roof_deck(B, x0, x1, z, side, rnd):
    """Parapet, planters and a timber pergola on the roof of the upper maisonette."""
    y0, y1 = UPPER_Y0 - 0.1, 18.0
    # parapet low wall around (front, back)
    ox0, ox1 = x0 - (0 if side == 1 else 0.3), x1 + (0.3 if side == 1 else 0)
    box(B.m, "CT_Concrete", ox0, ox1, y0 - 0.3, y0 - 0.1, z, z + 0.45, faces="xXyYZ")
    for xx in (ox0 + 0.3, ox1 - 0.3):
        pass
    _planter(B, ox0 + 0.3, ox0 + 5.0, 16.3, 17.2, z, rnd, h=0.5)
    _planter(B, ox1 - 5.0, ox1 - 0.5, 16.3, 17.2, z, rnd, h=0.5)
    # pergola: steel posts, timber beams and slats across the middle
    px0, px1 = x0 + 4.5, x1 - 4.5
    ya, yb = 14.0, 17.0
    for xx in (px0, px1):
        for yy in (ya, yb):
            box(B.detail, "CT_SteelWhite", xx - 0.06, xx + 0.06, yy - 0.06, yy + 0.06, z, z + 2.5)
    n = int((px1 - px0) / 0.45)
    for i in range(n + 1):
        xx = px0 + (px1 - px0) * i / n
        box(B.detail, "CT_Wood", xx - 0.03, xx + 0.03, ya - 0.1, yb + 0.1, z + 2.5, z + 2.58)
    for yy in (ya, yb):
        box(B.detail, "CT_Wood", px0 - 0.1, px1 + 0.1, yy - 0.07, yy + 0.07, z + 2.42, z + 2.5)


# ================================================================================================ interiors
def _open_body(B, wx0, wx1):
    """The east maisonette is hollow: floor slab (its top is the floor, z 9.33), the west end wall toward the stair
    canyon, a jamb filler at the door corner. The three facades get their inner faces (back=True) where they are made."""
    m = B.m
    z0, z1 = UPPER_Z0 - 0.3, UPPER_Z1 - 0.25
    box(m, "CT_Concrete", wx0, wx1, UPPER_Y0 + WALL, 18.0, z0, UPPER_Z0, faces="xXyYz")
    m.face("CT_Floor", [(wx0 + WALL, UPPER_Y0 + WALL, UPPER_Z0), (wx1 - WALL, UPPER_Y0 + WALL, UPPER_Z0),
                        (wx1 - WALL, 18.0 - WALL, UPPER_Z0), (wx0 + WALL, 18.0 - WALL, UPPER_Z0)])
    # the west end wall (outside faces the stair canyon, inside the room)
    box(m, "CT_Concrete", wx0, wx0 + WALL, UPPER_Y0, 18.0, UPPER_Z0, z1, faces="xyYZz")
    box(m, "CT_Interior", wx0, wx0 + WALL, UPPER_Y0 + WALL, 18.0 - WALL, UPPER_Z0, z1, faces="X")
    # jamb filler in the door corner (x 17.3..17.6, y 12.8..13.1)
    box(m, "CT_Interior", wx1 - WALL, wx1, UPPER_Y0, UPPER_Y0 + WALL, UPPER_Z0, z1, faces="XY")
    # plaster ceiling under the roof slab
    B.detail.face("CT_Interior", [(wx0 + WALL, UPPER_Y0 + WALL, z1 - 0.004), (wx0 + WALL, 18.0 - WALL, z1 - 0.004),
                                  (wx1 - WALL, 18.0 - WALL, z1 - 0.004), (wx1 - WALL, UPPER_Y0 + WALL, z1 - 0.004)])


def _near_shrub(x, y, r=0.9):
    return any(math.hypot(x - sx, y - sy) < r for sx, sy, _ in SHR)


def _gardens(B):
    """Life on the terrace gardens in front of every tier's flats: benches, tables with chairs, a chess table, bikes,
    a pram, a washing line, pots at the doors."""
    f = lambda fn, x, y, yaw=0.0, z=0.0, **kw: fn(B, x, y, z, yaw, **kw)
    spec = {   # (tier, side): list of (kind, |x|)
        (0, 1): [("bench", 5.5), ("table", 9.8), ("bike", 14.0), ("line", 17.0)],
        (0, -1): [("bench", 6.0), ("chess", 10.0), ("pram", 14.5), ("line", 17.5)],
        (1, 1): [("chess", 6.5), ("bench", 10.5), ("pram", 14.0), ("tubs", 17.0)],
        (1, -1): [("table", 6.5), ("bench", 11.0), ("bike", 14.5), ("line", 17.5)],
        (2, 1): [("bench", 5.5), ("table", 9.5), ("bike", 13.8), ("chess", 17.0)],
        (2, -1): [("bench", 5.8), ("tubs", 9.0), ("table", 12.5), ("line", 17.0)],
    }
    n = 0
    for (k, side), items in spec.items():
        y0, y1 = B_Y[k], B_Y[k + 1]
        zt = ZT[k]
        for kind, ax in items:
            x = ax * side
            if kind == "bench":
                y = y0 + 1.5
                b = f(kit.bench, x, y, 0.0, z=zt, w=1.7, kind="park", solid=False)
                s = b["seats"][0]
                B.thing("garden_bench_t%d%s%d" % (k, "e" if side == 1 else "w", n), "seat", s[:3], s[3], height=round(s[2] - zt, 2))
                B.spot("garden_bench_t%d%s" % (k, "e" if side == 1 else "w"), "bench", (s[0], s[1], zt), s[3])
            elif kind == "table":
                y = y0 + 2.2
                t = f(kit.table, x, y, 0.0, z=zt, w=0.9, d=0.9, kind="steel")
                f(kit.cup, x - 0.15, y, 0.0, z=zt + 0.75)
                f(kit.teapot, x + 0.12, y + 0.1, 0.0, z=zt + 0.75)
                c1 = f(kit.chair, x - 0.75, y, 90.0, z=zt, kind="tube", mat="CT_PlasterYellow")
                c2 = f(kit.chair, x + 0.75, y, 270.0, z=zt, kind="tube", mat="CT_PlasterBlue")
                s = c1["seats"][0]
                B.thing("garden_table_t%d%s%d" % (k, "e" if side == 1 else "w", n), "seat", s[:3], s[3], height=round(s[2] - zt, 2))
            elif kind == "chess":
                y = y0 + 2.2
                t = f(kit.chess_table, x, y, 0.0, z=zt)
                c1 = f(kit.stool, x, y - 0.75, 0.0, z=zt, h=0.45)
                c2 = f(kit.stool, x, y + 0.75, 0.0, z=zt, h=0.45)
                B.thing("garden_chess_t%d%s" % (k, "e" if side == 1 else "w"), "play", (x, y, zt + 0.78), 0.0, what="chess")
                for j, yw in enumerate((180.0, 0.0)):     # the stools are seats the player takes
                    B.thing("garden_chess_t%d%s_stool%d" % (k, "e" if side == 1 else "w", j), "seat", (x, y + (-0.75 if j == 0 else 0.75), zt + 0.45), yw, height=0.45)
            elif kind == "bike":
                _bike_at(B, x, y0 + 2.2, zt, 90.0)
            elif kind == "pram":
                _pram_at(B, x, y0 + 2.3, zt)
            elif kind == "line":
                _washing_line(B, x, y0 + 2.4, zt, side)
            elif kind == "tubs":
                for j, px in enumerate((x - 0.6, x, x + 0.6)):
                    f(kit.plant, px, y0 + 1.9 + 0.4 * (j % 2), 0.0, z=zt, kind=("fern", "ficus", "cactus")[j], pot="CT_PlasterRed", h=0.8, seed=k * 7 + j)
            n += 1
    # pots and a mat at every flat door of tiers 0..3 (the doors are the flat_door entrances)
    pots = []
    for i, e in enumerate(B.entrances):
        if e["kind"] != "flat_door" or e["id"].startswith("u"):
            continue
        x, y, z = e["pos"]
        px = x + (0.95 if i % 2 else -0.95)
        if any(abs(px - qx) < 1.0 and abs(y - qy) < 1.0 for qx, qy in pots):
            px = x + (-0.95 if i % 2 else 0.95)
        pots.append((px, y))
        f(kit.plant, px, y - 0.45, 0.0, z=z, kind=("fern", "cactus", "ficus")[i % 3], pot=DOOR_COLOURS[i % len(DOOR_COLOURS)], h=0.65, seed=i)
        f(kit.rug, x, y - 0.55, 0.0, z=z + 0.002, w=0.9, d=0.6, name="rug_%d" % (i % 8))


def _bike_at(B, x, y, z, yaw):
    p = kit._P(B, x, y, z, yaw, "bicycle")
    p.kind = "prop"
    R = 0.33
    for yc in (-0.55, 0.55):
        pts = [(0.0, yc + R * math.cos(2 * math.pi * k / 14), R + R * math.sin(2 * math.pi * k / 14)) for k in range(15)]
        p.tube("CT_Rubber", pts, 0.04, 4)
    p.rod("CT_SteelRed", (0.0, 0.55, R), (0.0, 0.05, 0.62), 0.03, 4)
    p.rod("CT_SteelRed", (0.0, 0.05, 0.62), (0.0, -0.4, 0.66), 0.03, 4)
    p.rod("CT_SteelRed", (0.0, 0.55, R), (0.0, 0.0, 0.3), 0.03, 4)
    p.rod("CT_SteelRed", (0.0, -0.4, 0.66), (0.0, -0.55, R), 0.03, 4)
    p.rod(kit.STEEL, (-0.28, -0.42, 0.99), (0.28, -0.42, 0.99), 0.025, 4)
    p.box(kit.LAQ, -0.06, 0.06, 0.02, 0.24, 0.92, 0.96)
    return p.done()


def _pram_at(B, x, y, z):
    return hk.pram(B, x, y, 0.0, mat="CT_PlasterYellow") if z == 0 else _lift(B, hk.pram, x, y, z)


def _lift(B, fn, x, y, z):
    """hk.pram/bike put things at z 0: draw them and move the new faces up by z."""
    n0 = len(B.detail.verts)
    r = fn(B, x, y, 0.0, mat="CT_PlasterYellow")
    B.detail.verts[n0:] = [(vx, vy, vz + z) for vx, vy, vz in B.detail.verts[n0:]]
    return r


def _washing_line(B, x, y, z, side):
    """Two posts and a line with a few cloths (detail)."""
    d = B.detail
    for px in (x - 1.4, x + 1.4):
        box(d, "CT_Steel", px - 0.03, px + 0.03, y - 0.03, y + 0.03, z, z + 1.9)
    box(d, "CT_Steel", x - 1.4, x + 1.4, y - 0.01, y + 0.01, z + 1.82, z + 1.84)
    for j, (cx, col) in enumerate(((x - 0.9, "CT_PlasterRed"), (x - 0.3, "CT_TileWhite"), (x + 0.3, "CT_PlasterBlue"), (x + 0.95, "CT_PlasterYellow"))):
        w = 0.34 + 0.08 * (j % 2)
        h = 0.55 + 0.1 * (j % 3)
        d.face(col, [(cx - w / 2, y - 0.012, z + 1.82 - h), (cx + w / 2, y - 0.012, z + 1.82 - h), (cx + w / 2, y - 0.012, z + 1.82), (cx - w / 2, y - 0.012, z + 1.82)])
        d.face(col, [(cx + w / 2, y + 0.012, z + 1.82 - h), (cx - w / 2, y + 0.012, z + 1.82 - h), (cx - w / 2, y + 0.012, z + 1.82), (cx + w / 2, y + 0.012, z + 1.82)])


def _stair_things(B):
    """The public stair: mailboxes and boards at the foot, a payphone, a timetable; planters on the landings."""
    f = lambda fn, x, y, yaw=0.0, z=0.0, **kw: fn(B, x, y, z, yaw, **kw)
    zs = lambda y: 0.16 * y / GOING * 1.0       # stair level at y on flight 1 (rise 2.4 over 15 treads of 0.28)
    z1_ = zs(1.6)
    mb = f(hk.mailboxes, SX + 0.02, 1.7, 270.0, z=z1_ - 0.4, cols=4, rows=3)
    nb = f(kit.notice_board, SX + 0.02, 3.0, 270.0, z=zs(3.0) - 0.3, w=1.0, zc=1.7, seed=51)
    B.thing("stair_board_lost", "board", (SX, 3.0, zs(3.0) + 1.4), 270.0, topic="lost")
    B.thing("stair_board_club", "board", (-SX, 3.2, zs(3.2) + 1.4), 90.0, topic="club")
    f(kit.notice_board, -SX - 0.02, 3.2, 90.0, z=zs(3.2) - 0.3, w=0.9, zc=1.7, seed=52)
    ph = f(kit.wall_phone, -SX - 0.02, 1.8, 90.0, z=zs(1.8) - 0.3)
    B.thing("stair_payphone", "machine", (-SX, 1.8, zs(1.8) + 1.1), 90.0, what="payphone")


def _maisonette(B):
    """The east maisonette (flat 4-01), home of an amateur astronomer (or, in the second plot, a seamstress): a long
    flat with the front door in the east wall corner, kitchen and dining table at that end, the living room with the
    sofa and a record player in the middle, a desk and shelves, and the bed behind a wardrobe at the west end."""
    z = UPPER_Z0
    xi0, xi1, yi0, yi1 = SX + WALL, WING_X - WALL, UPPER_Y0 + WALL, 18.0 - WALL
    ceil = UPPER_Z1 - 0.25
    inside = hk.poly_test([(xi0, yi0), (xi1, yi0), (xi1, yi1), (xi0, yi1)])
    H = hk.Home(B, lambda x, y: inside(x, y, 0.02))
    f = H.f
    seam = RESIDENT == "seamstress"
    # ---- kitchen end (east): run along the back wall under the windows, dining table, mat at the door
    f(kit.rug, 16.5, 13.7, 0.0, z=z + 0.003, w=1.0, d=0.7, name="rug_5")
    f(kit.kitchen_unit, 15.8, yi1 - 0.02, 0.0, z=z, modules="fridge,cupboard,sink,stove", wall_cupboards=False)
    f(kit.samovar, 16.3, yi1 - 0.3, 0.0, z=z + 0.9)
    dt = f(kit.table, 15.2, 15.6, 0.0, z=z, w=1.3, d=0.85)
    f(kit.plates, 15.0, 15.6, 0.0, z=z + 0.75, n=2)
    f(kit.vase, 15.5, 15.6, 0.0, z=z + 0.75)
    ch1 = f(kit.chair, 14.6, 16.3, 180.0, z=z, kind="wood", mat="CT_PlasterBlue")
    ch2 = f(kit.chair, 15.8, 16.3, 180.0, z=z, kind="wood", mat="CT_PlasterYellow")
    ch3 = f(kit.chair, 14.6, 14.9, 0.0, z=z, kind="wood", mat="CT_PlasterRed")
    f(kit.chair, 15.8, 14.9, 0.0, z=z, kind="wood", mat="CT_PlasterGreen")
    f(kit.wall_clock, xi1, 16.4, 270.0, z=z, zc=1.95, face="clock_1")
    f(kit.picture, xi1, 15.2, 270.0, z=z, name="painting_still_life", w=0.5, zc=1.5)
    f(kit.hat_stand, xi1 - 0.35, 14.3, 0.0, z=z)
    # ---- the living room
    f(kit.rug, 11.3, 15.4, 0.0, z=z + 0.002, w=3.0, d=2.3, name="rug_6")
    sofa = f(kit.sofa, 11.3, yi1 - 0.55, 0.0, z=z, seats=3, mat="CT_PlasterGreen")
    ct = f(kit.coffee_table, 11.3, 15.6, 0.0, z=z, w=1.0, d=0.55)
    f(kit.newspaper, 11.1, 15.6, 10.0, z=z + 0.42, cellname="news_1")
    f(kit.cup, 11.55, 15.55, 0.0, z=z + 0.42)
    a1 = f(kit.armchair, 9.6, 15.5, 90.0, z=z, mat="CT_PlasterRed")
    a2 = f(kit.armchair, 13.0, 15.5, 270.0, z=z, mat="CT_PlasterYellow")
    f(kit.floor_lamp, 9.5, 16.9, 0.0, z=z)
    rp_c = f(kit.table, 13.0, yi1 - 0.25, 0.0, z=z, w=0.9, d=0.4, h=0.55)
    rp = f(kit.record_player, 13.0, yi1 - 0.25, 0.0, z=z + 0.55)
    f(kit.plant, 13.3, 13.95, 0.0, z=z, kind="monstera", pot="CT_PlasterBlue", h=1.1, seed=3)
    f(kit.plant, 7.0, 13.7, 0.0, z=z, kind="ficus", pot="CT_PlasterRed", h=1.2, seed=4)
    # ---- the hobby corner at the front window (astronomer: a telescope; seamstress: the machine and dress forms)
    if seam:
        hb = f(kit.sewing_table, 11.4, 13.75, 0.0, z=z)
        hs = f(kit.chair, 11.4, 14.4, 0.0, z=z, kind="wood", mat="CT_PlasterPink")
        B.thing("home_sewing_machine", "craft", hb["pos"][:2] + (z + 0.85,), 0.0, what="sewing_machine")
    else:
        hb = f(kit.telescope, 11.4, 13.95, 0.0, z=z)
        B.thing("home_telescope", "machine", hb["pos"][:2] + (z + 1.2,), 0.0, what="telescope")
    # ---- study (middle-west): desk under the back wall, shelves, charts
    dk = f(kit.office_desk, 7.4, yi1 - 0.5, 180.0, z=z, w=1.3, d=0.6, typewriter_on=not seam, papers=True, phone=False, seed=5)
    dc = f(kit.chair, 7.4, yi1 - 1.15, 180.0, z=z, kind="wood", mat="CT_PlasterYellow")
    f(kit.lamp_green, 6.9, yi1 - 0.5, 180.0, z=z + 0.75)
    f(kit.globe, 7.9, yi1 - 0.5, 0.0, z=z + 0.75)
    sh1 = f(kit.bookshelf, 5.6, yi1, 0.0, z=z, w=1.0, h=1.9, levels=5, topic="art" if seam else "sun", seed=21)
    sh2 = f(kit.bookshelf, 4.5, yi1, 0.0, z=z, w=1.0, h=1.9, levels=5, topic="science", seed=22)
    f(kit.poster, 9.5, yi0, 180.0, z=z, name="poster_sun_1" if not seam else "fabric_2", w=0.42, zc=1.6)   # on the pier between two windows
    f(kit.poster, 5.97, yi0, 180.0, z=z, name="chart_calendar" if not seam else "fabric_4", w=0.42, zc=1.6)
    # ---- the bed at the west end, behind a wardrobe and a screen
    bd = f(kit.bed, xi0 + 1.1, 15.7, 90.0, z=z, size="double", blanket="CT_PlasterBlue")
    f(kit.bedside_table, xi0 + 0.3, 14.55, 90.0, z=z, lamp=True)
    f(kit.bedside_table, xi0 + 0.3, 16.85, 90.0, z=z, lamp=True)
    f(kit.wardrobe, 4.0, yi0, 180.0, z=z, w=1.1)
    f(kit.mirror, xi0, 17.4, 90.0, z=z, zc=1.45, w=0.5, h=0.9)
    f(kit.rug, 3.9, 15.7, 90.0, z=z + 0.002, w=1.8, d=2.4, name="rug_3")
    # ---- lights: four pendants
    for x, y in ((4.2, 15.7), (8.4, 15.5), (11.3, 15.4), (15.2, 15.6)):
        kit.pendant(B, x, y, ceil, drop=0.45, light=True, lumens=450, colour="#FFE2B8", radius=6.0)
    # ---- things
    B.thing("home_bed", "bed", bd["pos"][:2] + (z + 0.5,), bd["yaw"], height=0.5, rest="private")
    H.seat("home_sofa", sofa, 1)
    H.seat("home_armchair_red", a1)
    H.seat("home_armchair_yellow", a2)
    H.seat("home_dining_chair", ch1)
    H.seat("home_desk_chair", dc)
    B.thing("home_record_player", "machine", rp["pos"][:2] + (z + 0.7,), 0.0, what="record_player")
    B.thing("home_shelf_main", "shelf", sh1["pos"][:2] + (z + 1.3,), 180.0, topic="art" if seam else "sun")
    B.thing("home_shelf_science", "shelf", sh2["pos"][:2] + (z + 1.3,), 180.0, topic="science")
    B.thing("home_board_chart", "board", (9.5, yi0, z + 1.6), 180.0, topic="view")
    B.thing("home_board_calendar", "board", (5.97, yi0, z + 1.6), 180.0, topic="club")
    B.thing("home_picture", "board", (xi1, 15.2, z + 1.5), 270.0, topic="painting")
    B.thing("home_paper", "take", (11.1, 15.6, z + 0.45), 10.0, item="daily_paper")
    H.check("riser-maisonette", overlaps=False)


from _wallhang import hung  # noqa: E402  (QA r9: nothing hangs over the glass)
build = hung(build)
