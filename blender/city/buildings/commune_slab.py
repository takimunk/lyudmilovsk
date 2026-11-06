"""Commune slab ("дом-коммуна"): a constructivist residential slab on pilotis with a communal block, linked by a
glazed bridge. Original design in the spirit of the Soviet "transitional type" houses of the late 1920s (Ginzburg's
Narkomfin, Moscow collective housing), not a copy of any one building.

Plot 70 x 14 m, max 20 m. Everything is in one line along the street (x -35 .. 35):
  communal block  x -35..-22, y 1..13   two volumes: a glazed double-height hall (canteen / laundry / club room, ENTERABLE,
                                        6.4 m high) and a gym box above it that overhangs the street by 0.7 m
                                        (ribbon windows, flat roof).
  glazed bridge   x -22..-18, y 10..12  at level 2 (z 7.0 = the corridor floor of the slab), glass sides, flat roof.
  slab            x -18..31, y 1..12    49 x 11 m, 5 storeys: pilotis storey (z 0..3.5, round columns, partly open,
                                        a glazed lobby / bike room under the east end) + 4 floors at z 4, 7, 10, 13;
                                        roof terrace at z 16 (railing, timber pergola, glazed solarium shed).
  stair tower     x 31..35, y 0.5..12.5 red (CT_PlasterRed) hollow tower, 18.4 m, glass slits, the street door "main".
Skip-stop flats: 16 two-level flats (8 in floors 1-2 = "1-01".."1-08", 8 in floors 3-4 = "3-01".."3-08"). Access
  corridors run along the back on floors 2 and 4 only (z 7 and 13): at the back you see a continuous corridor
  window band on those floors and small bedroom/kitchen windows on floors 1 and 3. The front is a continuous ribbon
  window on every floor (white piers, grey spandrels). All flats use entrance "main" (the tower).
Entrances: "main" (tower, double leaves, 1.6 m), "hall" (communal hall, double leaves, 2.4 m, red canopy).
Enterable: the communal hall (floor CT_Floor, CT_Interior walls and ceiling). Spots: 6 "table", 1 "counter" (serving
  counter at the back, customers face +y = yaw 180), 5 "work" (laundry machines along the west wall, users face
  -x = yaw 270), 3 "bench" under the slab. Spot yaw = the direction the person using the spot faces (0 = -y,
  towards the street; 90 = +x).
Colours: off-white plaster, grey spandrels/soffits, red tower, canopy and cap, dark steel rails, timber pergola.
Ideas for later: stairs inside the hall to the gym, interior corridors, sliding red shutters, a cinema in the hall,
  hanging planters, roof garden beds, a wall newspaper board under the pilotis.
"""
import math

from citykit import box, prism, DOOR_W
from _cons_util import wall, win, hface, rect

INFO = {
    "name_ru": "Дом-коммуна", "name_en": "Commune Slab",
    "style": "constructivism", "use": "mixed",
    "inspired_by": "Soviet transitional-type communal housing of 1928-30 (Ginzburg, Narkomfin; skip-stop corridors, "
                   "pilotis, ribbon windows, communal block on a bridge)",
}

SX0, SX1, SY0, SY1 = -18.0, 31.0, 1.0, 12.0
NB = 8
BW = (SX1 - SX0) / NB
FL = (4.0, 7.0, 10.0, 13.0)
ZB = 3.5            # underside of the slab
ZT = 15.6           # top of the walls (roof slab 15.6..16.0)
ZR = 16.0
CORR = (7.0, 13.0)
XB0, XB1 = -35.0, -22.0     # communal block
YB0, YB1 = 1.0, 13.0
HALL_H = 6.4


def fid_of(f, k):
    return ("1-%02d" if f < 8 else "3-%02d") % (k + 1)


def build(B):
    m, det = B.m, B.detail
    # ---------------------------------------------------------------- slab: soffit, roof, pilotis
    prism(m, "CT_ConcreteDark", rect(SX0, SX1, SY0, SY1), ZB, FL[0], top=False, bottom=True, side_mat="CT_Plaster")
    roof = rect(SX0 - 0.15, SX1 + 0.15, SY0 - 0.15, SY1 + 0.15)
    prism(m, "CT_Plaster", roof, ZT, ZR, top=False, bottom=True)
    hface(m, "CT_Roof", roof, ZR, True)
    hface(m, "CT_Paving", rect(SX0 - 0.5, SX1, SY0 - 0.5, SY1 + 0.5), 0.0, True)
    for k in range(NB):
        x = SX0 + k * BW
        for y in (2.5, 9.6):
            B.column_round(x, y, 0.0, ZB, 0.33, mat="CT_ConcreteWhite", segs=14)
    B.column_round(SX1 - 0.3, 2.5, 0.0, ZB, 0.33, mat="CT_ConcreteWhite", segs=14)
    # ---------------------------------------------------------------- slab: front wall (ribbon windows)
    p0, p1 = (SX0, SY0), (SX1, SY0)
    for f in FL:
        z0 = f - 0.5
        wall(B, p0, p1, z0, f + 0.85, "CT_Concrete", depth=0.4, mat_in="CT_Plaster")
        holes = [(k * BW + 0.2, (k + 1) * BW - 0.2, f + 0.85, f + 2.35) for k in range(NB)]
        wall(B, p0, p1, f + 0.85, f + 2.35, "CT_Plaster", holes=holes, depth=0.4, mat_in="CT_Plaster")
        top = f + 2.5 if f < FL[-1] else ZT
        wall(B, p0, p1, f + 2.35, top, "CT_Plaster", depth=0.4)
        for k in range(NB):
            win(B, p0, p1, k * BW + 0.2, (k + 1) * BW - 0.2, f + 0.85, f + 2.35, flat=fid_of(f, k), cols=3, rows=1,
                inset=0.22, frame=0.05)
    # ---------------------------------------------------------------- slab: back wall (corridor bands / small windows)
    q0, q1 = (SX1, SY1), (SX0, SY1)
    for f in FL:
        if f in CORR:
            holes = [(0.3, SX1 - SX0 - 0.3, f + 0.9, f + 2.1)]
            wall(B, q0, q1, f - 0.5, f + 2.5 if f < FL[-1] else ZT, "CT_Plaster", holes=holes, depth=0.4)
            nwin = 16
            w = (SX1 - SX0 - 0.6) / nwin
            for i in range(nwin):
                win(B, q0, q1, 0.3 + i * w + 0.03, 0.3 + (i + 1) * w - 0.03, f + 0.9, f + 2.1, kind="stair", cols=1,
                    rows=1, inset=0.2, frame=0.04)
        else:
            holes = []
            for k in range(NB):
                xm = SX0 + (k + 0.5) * BW
                u = SX1 - xm
                holes.append((u - 0.65, u + 0.65, f + 0.9, f + 1.9))
            wall(B, q0, q1, f - 0.5, f + 2.5, "CT_Plaster", holes=holes, depth=0.4)
            for k in range(NB):
                xm = SX0 + (k + 0.5) * BW
                u = SX1 - xm
                win(B, q0, q1, u - 0.65, u + 0.65, f + 0.9, f + 1.9, flat=fid_of(f, k), cols=2, rows=1, inset=0.2,
                    frame=0.05)
    # ---------------------------------------------------------------- slab: west end wall with the bridge door (floor 2)
    wall(B, (SX0, SY1), (SX0, SY0), ZB, ZT, "CT_Plaster", holes=[(0.3, 1.7, 7.0, 9.3)], depth=0.4)
    # corridor/flat doors
    for g, fz in ((0, 7.0), (1, 13.0)):
        for k in range(NB):
            B.flat(fid_of(4.0 if g == 0 else 10.0, k), 1 if g == 0 else 3, beds=2,
                   door=(SX0 + (k + 0.5) * BW, SY1 - 1.0, fz), entrance="main")
    # ---------------------------------------------------------------- lobby / bike room under the east end
    lx0, ly0, ly1 = SX0 + 6 * BW, 3.6, 9.0
    wall(B, (lx0, ly0), (SX1, ly0), 0.0, ZB, "CT_PlasterGrey",
         holes=[(0.5 + i * 4.0, 0.5 + i * 4.0 + 3.2, 0.4, 3.0) for i in range(3)], depth=0.35, mat_in="CT_Plaster")
    for i in range(3):
        win(B, (lx0, ly0), (SX1, ly0), 0.5 + i * 4.0, 0.5 + i * 4.0 + 3.2, 0.4, 3.0, kind="public", cols=3, rows=1,
            inset=0.15, frame=0.06)
    wall(B, (SX1, ly1), (lx0, ly1), 0.0, ZB, "CT_PlasterGrey", depth=0.35, mat_in="CT_Plaster")
    wall(B, (lx0, ly1), (lx0, ly0), 0.0, ZB, "CT_PlasterGrey", depth=0.35, mat_in="CT_Plaster")
    # ---------------------------------------------------------------- stair tower (red)
    tx0, tx1, ty0, ty1 = 31.0, 35.0, 0.5, 12.5
    TH = 17.8
    hface(m, "CT_Paving", rect(tx0, tx1, ty0, ty1), 0.0, True)
    wall(B, (tx0, ty0), (tx1, ty0), 0.0, TH, "CT_PlasterRed",
         holes=[(0.5, 2.1, 0.0, 2.5), (2.7, 3.5, 0.5, 3.1), (2.7, 3.5, 4.6, 17.0)], depth=0.4, mat_in="CT_Plaster")
    win(B, (tx0, ty0), (tx1, ty0), 2.7, 3.5, 0.5, 3.1, kind="stair", rows=2, inset=0.2, frame=0.05)
    win(B, (tx0, ty0), (tx1, ty0), 2.7, 3.5, 4.6, 17.0, kind="stair", rows=8, inset=0.2, frame=0.05)
    wall(B, (tx1, ty0), (tx1, ty1), 0.0, TH, "CT_PlasterRed",
         holes=[(5.4, 6.6, 0.5, 3.1), (5.4, 6.6, 4.6, 17.0)], depth=0.4, mat_in="CT_Plaster")
    win(B, (tx1, ty0), (tx1, ty1), 5.4, 6.6, 0.5, 3.1, kind="stair", rows=2, inset=0.2, frame=0.05)
    win(B, (tx1, ty0), (tx1, ty1), 5.4, 6.6, 4.6, 17.0, kind="stair", rows=8, inset=0.2, frame=0.05)
    wall(B, (tx1, ty1), (tx0, ty1), 0.0, TH, "CT_PlasterRed", holes=[(1.5, 2.5, 4.6, 17.0)], depth=0.4,
         mat_in="CT_Plaster")
    win(B, (tx1, ty1), (tx0, ty1), 1.5, 2.5, 4.6, 17.0, kind="stair", rows=8, inset=0.2, frame=0.05)
    wall(B, (tx0, ty1), (tx0, ty0), 0.0, ZB, "CT_PlasterRed", depth=0.4, mat_in="CT_Plaster")
    tr = rect(tx0 - 0.25, tx1 + 0.25, ty0 - 0.25, ty1 + 0.25)
    prism(m, "CT_Plaster", tr, TH, TH + 0.6, top=False, bottom=True)
    hface(m, "CT_Roof", tr, TH + 0.6, True)
    B.entrance("main", ((tx0 + 0.5 + tx0 + 2.1) / 2, ty0, 0.0), 0.0, width=1.6, height=2.5, kind="street", leaves=2)
    box(m, "CT_SteelRed", tx0 + 0.3, tx0 + 2.3, ty0 - 1.2, ty0, 3.0, 3.15)               # door canopy
    # ---------------------------------------------------------------- roof terrace
    inset = rect(SX0 + 0.3, SX1 - 0.3, SY0 + 0.3, SY1 - 0.3)
    B.railing(inset + [inset[0]], ZR, h=1.05, mat="CT_Steel", post=1.5)
    # pergola
    for x in (2.0, 6.0, 10.0, 14.0):
        for y in (3.0, 9.0):
            box(det, "CT_Steel", x - 0.07, x + 0.07, y - 0.07, y + 0.07, ZR, ZR + 2.6)
    for y in (3.0, 9.0):
        box(det, "CT_Steel", 1.9, 14.1, y - 0.1, y + 0.1, ZR + 2.6, ZR + 2.78)
    for i in range(21):
        x = 2.0 + i * 0.6
        box(det, "CT_Wood", x - 0.05, x + 0.05, 2.9, 9.1, ZR + 2.78, ZR + 2.86)
    box(det, "CT_Wood", 3.0, 8.0, 5.4, 6.6, ZR, ZR + 0.45)                               # a long bench/planter
    # solarium shed
    sx0, sx1, sy0, sy1 = 18.0, 26.0, 6.5, 10.0
    wall(B, (sx0, sy0), (sx1, sy0), ZR, ZR + 2.5, "CT_Plaster",
         holes=[(0.4, 3.8, 0.5, 2.2), (4.2, 7.6, 0.5, 2.2)], depth=0.3)
    for u0, u1 in ((0.4, 3.8), (4.2, 7.6)):
        win(B, (sx0, sy0), (sx1, sy0), u0, u1, ZR + 0.5, ZR + 2.2, kind="public", cols=3, rows=1, inset=0.15, frame=0.05)
    wall(B, (sx1, sy0), (sx1, sy1), ZR, ZR + 2.5, "CT_Plaster", depth=0.3)
    wall(B, (sx1, sy1), (sx0, sy1), ZR, ZR + 2.5, "CT_Plaster", depth=0.3)
    wall(B, (sx0, sy1), (sx0, sy0), ZR, ZR + 2.5, "CT_Plaster", depth=0.3)
    sr = rect(sx0 - 0.4, sx1 + 0.4, sy0 - 0.6, sy1 + 0.4)
    prism(m, "CT_PlasterGrey", sr, ZR + 2.5, ZR + 2.75, top=True, bottom=True)
    # ---------------------------------------------------------------- communal block
    _block(B)
    # ---------------------------------------------------------------- glazed bridge at level 2
    bx0, bx1, by0, by1 = XB1, SX0, 10.0, 12.0
    box(m, "CT_PlasterGrey", bx0 - 0.05, bx1 + 0.05, by0 - 0.1, by1 + 0.1, 6.6, 7.0)
    prism(m, "CT_Plaster", rect(bx0 - 0.1, bx1 + 0.1, by0 - 0.15, by1 + 0.15), 9.3, 9.6, top=False, bottom=True)
    hface(m, "CT_Roof", rect(bx0 - 0.1, bx1 + 0.1, by0 - 0.15, by1 + 0.15), 9.6, True)
    for yy, face in ((by0, -1), (by1, 1)):
        xs = (bx0, bx1) if face < 0 else (bx1, bx0)
        B.window([(xs[0], yy, 7.0), (xs[1], yy, 7.0), (xs[1], yy, 9.3), (xs[0], yy, 9.3)], kind="public", cols=4, rows=2,
                 inset=0.0, frame=0.06)
    for x in (bx0, bx1):
        for yy in (by0, by1):
            box(det, "CT_Frame", x - 0.06, x + 0.06, yy - 0.06, yy + 0.06, 7.0, 9.3)
    # ---------------------------------------------------------------- footprint / benches
    B.footprint = rect(XB0, 35.25, 0.3, 13.0)
    B.height = TH + 0.6
    for i, x in enumerate((-12.0, 0.0, 12.0)):
        box(det, "CT_Wood", x - 0.9, x + 0.9, 5.4, 5.85, 0.0, 0.45)
        box(det, "CT_Steel", x - 0.9, x - 0.8, 5.4, 5.85, 0.0, 0.45)
        box(det, "CT_Steel", x + 0.8, x + 0.9, 5.4, 5.85, 0.0, 0.45)
        B.spot("bench%d" % (i + 1), "bench", (x, 5.6, 0.0), 0.0)
    B.note("16 skip-stop flats, corridors on floors 2 and 4; communal hall enterable at the west end")


def _block(B):
    m = B.m
    # floor, ceiling, overhanging upper slab
    prism(m, "CT_ConcreteDark", rect(XB0, XB1, YB0, YB1), -0.4, 0.0, top=False, bottom=False)
    hface(m, "CT_Floor", rect(XB0, XB1, YB0, YB1), 0.0, True)
    prism(m, "CT_Plaster", rect(XB0, XB1, 0.3, YB1), HALL_H, 7.0, top=False, bottom=True, side_mat="CT_Plaster")
    hface(m, "CT_Interior", rect(XB0, XB1, YB0, YB1), HALL_H, False)
    hface(m, "CT_Roof", rect(XB0, XB1, 0.3, YB1), 7.0, True)
    # hall walls
    fp0, fp1 = (XB0, YB0), (XB1, YB0)
    holes = [(0.6, 4.4, 0.5, 5.6), (5.3, 7.7, 0.0, 2.7), (5.3, 7.7, 3.0, 5.6), (8.6, 12.4, 0.5, 5.6)]
    wall(B, fp0, fp1, 0.0, HALL_H, "CT_Plaster", holes=holes, depth=0.4, mat_in="CT_Interior")
    for u0, u1, z0, z1, rows in ((0.6, 4.4, 0.5, 5.6, 2), (5.3, 7.7, 3.0, 5.6, 1), (8.6, 12.4, 0.5, 5.6, 2)):
        win(B, fp0, fp1, u0, u1, z0, z1, kind="public", cols=4 if u1 - u0 > 3 else 2, rows=rows, inset=0.2, frame=0.06)
    wall(B, (XB1, YB0), (XB1, YB1), 0.0, HALL_H, "CT_Plaster", depth=0.4, mat_in="CT_Interior")
    wall(B, (XB1, YB1), (XB0, YB1), 0.0, HALL_H, "CT_Plaster", holes=[(1.0, 12.0, 4.0, 5.6)], depth=0.4,
         mat_in="CT_Interior")
    win(B, (XB1, YB1), (XB0, YB1), 1.0, 12.0, 4.0, 5.6, kind="public", cols=8, rows=1, inset=0.2, frame=0.05)
    wall(B, (XB0, YB1), (XB0, YB0), 0.0, HALL_H, "CT_Plaster", holes=[(2.0, 9.5, 3.0, 5.6)], depth=0.4,
         mat_in="CT_Interior")
    win(B, (XB0, YB1), (XB0, YB0), 2.0, 9.5, 3.0, 5.6, kind="public", cols=5, rows=1, inset=0.2, frame=0.05)
    # upper (gym) box: ribbon windows, flat roof
    UZ0, UZ1 = 7.0, 10.6
    up0, up1 = (XB0, 0.3), (XB1, 0.3)
    wall(B, up0, up1, UZ0, UZ0 + 0.9, "CT_PlasterGrey", depth=0.35)
    holes = [(0.5 + i * 3.1, 0.5 + i * 3.1 + 2.7, UZ0 + 0.9, UZ0 + 2.6) for i in range(4)]
    wall(B, up0, up1, UZ0 + 0.9, UZ0 + 2.6, "CT_Plaster", holes=holes, depth=0.35)
    for u0, u1, *_ in holes:
        win(B, up0, up1, u0, u1, UZ0 + 0.9, UZ0 + 2.6, kind="public", cols=3, rows=1, inset=0.18, frame=0.05)
    wall(B, up0, up1, UZ0 + 2.6, UZ1, "CT_Plaster", depth=0.35)
    wall(B, (XB1, 0.3), (XB1, 12.5), UZ0, UZ1, "CT_Plaster", holes=[(10.0, 11.4, 7.0, 9.3)], depth=0.35)
    wall(B, (XB1, 12.5), (XB0, 12.5), UZ0, UZ1, "CT_Plaster", depth=0.35)
    wall(B, (XB0, 12.5), (XB0, 0.3), UZ0, UZ1, "CT_Plaster", holes=[(2.5, 8.5, UZ0 + 0.9, UZ0 + 2.6)], depth=0.35)
    win(B, (XB0, 12.5), (XB0, 0.3), 2.5, 8.5, UZ0 + 0.9, UZ0 + 2.6, kind="public", cols=4, rows=1, inset=0.18, frame=0.05)
    ur = rect(XB0 - 0.15, XB1 + 0.15, 0.15, 12.65)
    prism(m, "CT_Plaster", ur, UZ1, UZ1 + 0.4, top=False, bottom=True)
    hface(m, "CT_Roof", ur, UZ1 + 0.4, True)
    # red canopy over the hall door, red edge band on the gym box
    box(m, "CT_SteelRed", -30.4, -26.6, -0.9, YB0, 3.0, 3.15)
    box(B.detail, "CT_SteelRed", XB0, XB1, 0.2, 0.34, UZ0 + 2.6, UZ0 + 2.9)
    B.entrance("hall", (-28.5, YB0, 0.0), 0.0, width=2.4, height=2.7, kind="street", leaves=2)
    # interior: serving counter, tables, laundry machines
    box(m, "CT_Wood", -33.2, -25.2, 11.2, 12.0, 0.0, 1.1)
    B.spot("counter1", "counter", (-29.2, 10.6, 0.0), 180.0)
    n = 0
    for x in (-32.0, -29.0, -26.0):
        for y in (4.2, 7.0):
            n += 1
            box(m, "CT_Wood", x - 0.6, x + 0.6, y - 0.6, y + 0.6, 0.0, 0.75)
            B.spot("table%d" % n, "table", (x, y, 0.0), 0.0)
    for i in range(5):
        y = 3.2 + i * 1.2
        box(m, "CT_Aluminium", -34.6, -33.8, y, y + 0.9, 0.0, 0.95)
        B.spot("work%d" % (i + 1), "work", (-33.2, y + 0.45, 0.0), 270.0)
