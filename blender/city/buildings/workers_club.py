"""Workers' club / house of culture (a cinema + club rooms): constructivist, raw concrete with red accents. Original
design in the spirit of the Soviet workers' clubs of 1927-30 (Melnikov, the Vesnin brothers, Golosov, Ilya Golosov's
"Zuev" club): auditorium volumes thrust out of the upper storeys towards the street. Not a copy of any building.

Plot 35 x 30 m, max 18 m. Footprint x -17.5..17.5, y -3.5..30 (the cantilevers overhang the pavement by up to 3.5 m),
height 18.0 m.
Massing (z up, street at y < 3):
  ground foyer     x +-17.5, y 3..12.6, 5 m high, glazed front (4 big bays between concrete piers, the double door
                   "main" in the middle under a red canopy); ENTERABLE.
  cinema hall      x +-17.5, y 12.6..30, 10.5 m high, concrete box with ribs and a high band of windows; ENTERABLE: raked
                   floor (10 rows, rise 0.16 m, going 1.3 m) falling towards the screen on the foyer-side wall; two doors
                   from the foyer flank the screen; a side exit on the east wall (y 22).
  club storeys     z 5.6..: left block 13 m, tall centre block 18 m (projection tower), right block 12 m.
  three cantilevered auditorium volumes on the front (hollow shells, closed top and soffit):
      A (left)   plan trapezoid, z 6.0..12.4, slanted front, ribbon windows
      B (centre) wedge in profile: soffit and roof both rise towards the street, z 7..17.2 at the front, 6.5 m jut,
                 carries the signage: a red panel (CT_PlasterRed) and a CT_Neon band without text, red edge strips
      C (right)  parallelogram plan, z 6.4..10.8, slanted the other way, windows on the front and east side
Not residential: no flats. Windows are all kind "public" (lit by the building's hours): 3 or 4 per volume face, foyer glazing,
  hall clerestory.
Spots (life sim; yaw = direction the user of the spot faces, 0 = -y toward the street): "counter" (ticket counter in the
  foyer, customers face +y = 180), 4 "bench" in the foyer, 350 "seat" (5 blocks x 7 x 10 rows, yaw 0 faces the screen).
Entrances: "main" (street, double leaves, 3.2 m), "side" (east exit, kind "side", 2.2 m). The two foyer->hall doors are
  plain holes (2.0 x 2.5 m, no door actors), at x = +-13 in the wall y = 12.6.
Collision: steps of the raked floor are in the shell; seats and screen are detail (no collision).
Ideas for later: a stage and curtain, a stair inside the foyer to the club rooms (upper floors are empty shells),
  a projection booth window in the tower, film posters on the foyer wall (a texture slot), hanging lamps.
"""
import math

import interior_kit as kit
from citykit import box, prism
from _cons_util import wall, win, hface, rect

INFO = {
    "name_ru": "Рабочий клуб (кинотеатр)", "name_en": "Workers' Club",
    "style": "constructivism", "use": "public",
    "inspired_by": "Soviet workers' clubs 1927-30 (Melnikov's clubs, Golosov's Zuev club: cantilevered auditorium "
                   "volumes, signage bands, raw concrete and red)",
}

X0, X1 = -17.5, 17.5
YF, YD, YB = 3.0, 12.6, 30.0     # front wall, foyer/hall divider, back wall
ZF = 5.0                          # foyer ceiling
HZ = 10.5                         # hall wall top
ZL, ZC, ZRt = 13.0, 18.0, 12.0    # left, centre, right block tops


def build(B):
    m, det = B.m, B.detail
    # ------------------------------------------------------------ floors, ceilings, roofs
    prism(m, "CT_ConcreteDark", rect(X0, X1, YF, YB), -0.4, 0.0, top=False, bottom=False)
    hface(m, "CT_Floor", rect(X0, X1, YF, YD), 0.0, True)
    hface(m, "CT_Paving", rect(X0 - 0.5, X1 + 0.5, -4.2, YF), 0.0, True)
    prism(m, "CT_Concrete", rect(X0, X1, YF, YD), ZF, 5.6, top=False, bottom=False)
    hface(m, "CT_Interior", rect(X0, X1, YF, YD), ZF, False)
    hface(m, "CT_Interior", rect(X0, X1, YD, YB), HZ, False)
    hr = rect(X0 - 0.15, X1 + 0.15, YD, YB + 0.15)
    prism(m, "CT_Concrete", hr, HZ, 11.1, top=False, bottom=False)
    hface(m, "CT_Roof", hr, 11.1, True)
    for (xa, xb, zt) in ((X0, -7.5, ZL), (-7.5, 7.5, ZC), (7.5, X1, ZRt)):
        hface(m, "CT_Roof", rect(xa, xb, YF, YD), zt, True)
    # ------------------------------------------------------------ front wall, ground (foyer glazing)
    p0, p1 = (X0, YF), (X1, YF)
    bays = [(0.7, 7.5), (8.2, 14.8), (20.2, 26.8), (27.5, 34.3)]
    holes = [(a, b, 0.4, 4.6) for a, b in bays] + [(15.9, 19.1, 0.0, 2.9), (15.9, 19.1, 3.2, 4.6)]
    wall(B, p0, p1, 0.0, ZF, "CT_Concrete", holes=holes, depth=0.4, mat_in="CT_Interior")
    for a, b in bays:
        win(B, p0, p1, a, b, 0.4, 4.6, kind="public", cols=4, rows=2, inset=0.2, frame=0.06)
    win(B, p0, p1, 15.9, 19.1, 3.2, 4.6, kind="public", cols=4, rows=1, inset=0.2, frame=0.06)
    # front wall, upper blocks
    wall(B, p0, (-7.5, YF), 5.0, ZL, "CT_Concrete", holes=[(8.5, 9.5, 7.0, 12.0)], depth=0.4)
    win(B, p0, (-7.5, YF), 8.5, 9.5, 7.0, 12.0, kind="public", rows=4, inset=0.2, frame=0.05)
    wall(B, (-7.5, YF), (7.5, YF), 5.0, ZC, "CT_Concrete", depth=0.4)
    wall(B, (7.5, YF), p1, 5.0, ZRt, "CT_Concrete", holes=[(0.5, 1.5, 7.0, 11.0)], depth=0.4)
    win(B, (7.5, YF), p1, 0.5, 1.5, 7.0, 11.0, kind="public", rows=4, inset=0.2, frame=0.05)
    # red fins beside the centre block, entrance canopy
    for xa, xb in ((-7.5, -6.0), (6.0, 7.5)):
        box(m, "CT_PlasterRed", xa, xb, YF - 1.0, YF, 5.6, ZC)
        box(det, "CT_Neon", (xa + xb) / 2 - 0.05, (xa + xb) / 2 + 0.05, YF - 1.08, YF - 1.0, 6.5, ZC - 1.0)
    box(m, "CT_SteelRed", -2.6, 2.6, -0.5, YF, 3.6, 3.8)
    for x in (-2.4, 2.4):
        box(det, "CT_SteelRed", x - 0.07, x + 0.07, -0.4, -0.26, 0.0, 3.6)
    B.entrance("main", (0.0, YF, 0.0), 0.0, width=3.2, height=2.9, kind="street", leaves=2)
    # ------------------------------------------------------------ side walls
    # west: club storeys + foyer side, hall
    wall(B, (X0, YD), (X0, YF), 0.0, ZL, "CT_Concrete", holes=[(1.5, 6.0, 0.8, 4.0), (1.0, 8.6, 7.0, 10.6)],
         depth=0.4, mat_in="CT_Interior")
    win(B, (X0, YD), (X0, YF), 1.5, 6.0, 0.8, 4.0, kind="public", cols=3, rows=1, inset=0.2)
    win(B, (X0, YD), (X0, YF), 1.0, 8.6, 7.0, 10.6, kind="public", cols=6, rows=1, inset=0.2)
    wins_side = [16.4, 20.5, 27.5]
    wall(B, (X0, YB), (X0, YD), 0.0, HZ, "CT_Concrete",
         holes=[(YB - y - 1.2, YB - y + 1.2, 8.0, 9.6) for y in wins_side], depth=0.4, mat_in="CT_Interior")
    for y in wins_side:
        win(B, (X0, YB), (X0, YD), YB - y - 1.2, YB - y + 1.2, 8.0, 9.6, kind="public", cols=2, rows=1, inset=0.2)
    # east
    wall(B, (X1, YF), (X1, YD), 0.0, ZRt, "CT_Concrete", holes=[(3.5, 7.5, 0.8, 4.0), (1.0, 8.6, 7.0, 10.6)],
         depth=0.4, mat_in="CT_Interior")
    win(B, (X1, YF), (X1, YD), 3.5, 7.5, 0.8, 4.0, kind="public", cols=3, rows=1, inset=0.2)
    win(B, (X1, YF), (X1, YD), 1.0, 8.6, 7.0, 10.6, kind="public", cols=6, rows=1, inset=0.2)
    yx = 22.0
    holes = [(yx - YD - 1.1, yx - YD + 1.1, 0.0, 2.6)] + [(y - YD - 1.2, y - YD + 1.2, 8.0, 9.6) for y in wins_side]
    wall(B, (X1, YD), (X1, YB), 0.0, HZ, "CT_Concrete", holes=holes, depth=0.4, mat_in="CT_Interior")
    for y in wins_side:
        win(B, (X1, YD), (X1, YB), y - YD - 1.2, y - YD + 1.2, 8.0, 9.6, kind="public", cols=2, rows=1, inset=0.2)
    B.entrance("side", (X1, yx, 0.96), 90.0, width=2.2, height=2.6, kind="side", leaves=1)
    box(m, "CT_PlasterRed", X1, X1 + 0.12, yx - 1.5, yx + 1.5, 2.8, 3.5)
    box(det, "CT_Neon", X1 + 0.12, X1 + 0.18, yx - 1.0, yx + 1.0, 3.0, 3.25)
    # back wall
    xs = [-13.2, -8.8, -4.4, 0.0, 4.4, 8.8, 13.2]
    wall(B, (X1, YB), (X0, YB), 0.0, HZ, "CT_Concrete", holes=[(X1 - x - 1.2, X1 - x + 1.2, 8.0, 9.6) for x in xs],
         depth=0.4, mat_in="CT_Interior")
    for x in xs:
        win(B, (X1, YB), (X0, YB), X1 - x - 1.2, X1 - x + 1.2, 8.0, 9.6, kind="public", cols=2, rows=1, inset=0.2)
    # ribs on the hall (back and sides)
    for i in range(8):
        x = -15.4 + 4.4 * i
        box(m, "CT_ConcreteDark", x - 0.4, x + 0.4, YB, YB + 0.45, 0.0, HZ)
    for y in (14.4, 18.4, 25.6, 29.4):
        box(m, "CT_ConcreteDark", X1, X1 + 0.45, y - 0.4, y + 0.4, 0.0, HZ)
        box(m, "CT_ConcreteDark", X0 - 0.45, X0, y - 0.4, y + 0.4, 0.0, HZ)
    # back walls of the upper blocks (seen above the hall roof)
    wall(B, (-7.5, YD), (X0, YD), 11.1, ZL, "CT_Concrete", depth=0.4)
    wall(B, (7.5, YD), (-7.5, YD), 11.1, ZC, "CT_Concrete", depth=0.4)
    wall(B, (X1, YD), (7.5, YD), 11.1, ZRt, "CT_Concrete", depth=0.4)
    wall(B, (-7.5, YF), (-7.5, YD), ZL, ZC, "CT_Concrete", depth=0.4)       # centre block west face above the left block
    wall(B, (7.5, YD), (7.5, YF), ZRt, ZC, "CT_Concrete", depth=0.4)         # east face above the right block
    # ------------------------------------------------------------ divider wall foyer / hall (doors flank the screen)
    wall(B, (X1, YD), (X0, YD), 0.0, HZ, "CT_Interior", holes=[(3.5, 5.5, 0.0, 2.5), (29.5, 31.5, 0.0, 2.5)],
         depth=0.4, mat_in="CT_Interior", back=True)
    # foyer -> hall doors (kind "inner"): pos on the foyer face of the divider, yaw 0 = the door faces -y (the foyer)
    B.entrance("hall_e", (13.0, YD - 0.4, 0.0), 0.0, width=2.0, height=2.5, kind="inner", leaves=1, to="hall")
    B.entrance("hall_w", (-13.0, YD - 0.4, 0.0), 0.0, width=2.0, height=2.5, kind="inner", leaves=1, to="hall")
    # ------------------------------------------------------------ the three cantilevered volumes
    _volume_a(B)
    _wedge_b(B)
    _volume_c(B)
    # ------------------------------------------------------------ interior: foyer
    _foyer(B)
    _hall_decor(B)
    _deepen(B)
    # ------------------------------------------------------------ interior: hall
    y0 = 14.55
    rows, going, rise = 10, 1.3, 0.16
    hface(m, "CT_Floor", rect(-17.1, 17.1, YD, y0), 0.0, True)           # the flat floor in front of the first row
    for i in range(rows):
        ye = y0 + going * (i + 1) + (0.0 if i < rows - 1 else YB - 0.4 - y0 - going * rows)
        if 4 <= i <= 6:                                  # the side exit (east wall, y 22) opens onto a flat landing
            box(m, "CT_Floor", -17.1, 14.0, y0 + going * i, ye, 0.0, rise * (i + 1))
            if i == 6:
                box(m, "CT_Floor", 14.0, 17.1, y0 + going * 4, ye, 0.0, rise * 6)
        else:
            box(m, "CT_Floor", -17.1, 17.1, y0 + going * i, ye, 0.0, rise * (i + 1))
    centres = [-11.6, -5.8, 0.0, 5.8, 11.6]
    _hall_seats(B, centres, y0, rows, going, rise)
    # stage step, screen on the divider wall, folded curtains, valance
    box(m, "CT_WoodLight", -9.6, 9.6, YD, 13.7, 0.0, 0.4, faces="xXyYZ")
    kit.cinema_screen(B, 0.0, YD + 0.0, 0.0, 180.0, w=12.2, h=5.6, zb=2.7, picture="screen_film_0")
    for sgn in (-1, 1):
        a_, b_ = (7.8, 9.6) if sgn > 0 else (-9.6, -7.8)
        for k in range(6):
            fx0 = a_ + k * (b_ - a_) / 6
            box(det, "CT_SteelRed" if k % 2 else "CT_PlasterRed", fx0, fx0 + (b_ - a_) / 6, YD, YD + (0.5 if k % 2 else 0.28), 0.4, 9.0)
        box(det, "CT_Steel", sgn * 8.7 - 0.3, sgn * 8.7 + 0.3, 13.0, 13.5, 0.4, 1.2)                # speaker stack
    box(det, "CT_SteelRed", -9.6, 9.6, YD, YD + 0.6, 8.75, 9.4)
    box(det, "CT_Neon", -9.6, 9.6, YD + 0.6, YD + 0.64, 8.75, 8.85, faces="y")
    # projection window at the back (frame + dark recess; the wall itself is not cut)
    yb = YB - 0.4
    for (xa, xb, za, zb) in ((-0.8, 0.8, 6.2, 7.5),):
        box(det, "CT_Steel", xa, xb, yb - 0.45, yb, za, zb)
        box(det, "CT_Frame", xa + 0.12, xb - 0.12, yb - 0.47, yb - 0.45, za + 0.12, zb - 0.12, faces="y")
        box(det, "CT_Neon", xa + 0.55, xb - 0.55, yb - 0.49, yb - 0.47, za + 0.45, za + 0.75, faces="y")
    for xx in (-4.4, 4.4):
        box(det, "CT_Steel", xx - 0.3, xx + 0.3, yb - 0.3, yb, 6.2, 6.9)                          # speakers
    # exit signs over the hall doors and the side exit
    for xx in (-13.0, 13.0):
        box(det, "CT_Neon", xx - 0.4, xx + 0.4, YD, YD + 0.06, 2.62, 2.85, faces="y")
    box(det, "CT_Neon", X1 - 0.06, X1, 21.6, 22.4, 2.7, 2.93, faces="x")
    # side exit: threshold block in the wall opening and outside steps along the wall
    zl = rise * 6
    box(m, "CT_Concrete", X1 - 0.4, X1, yx - 1.1, yx + 1.1, 0.0, zl, faces="yYZ")
    box(m, "CT_Concrete", X1, X1 + 1.0, yx - 1.1, yx + 1.1, 0.0, zl, faces="xXyYZ")
    for k in range(5):
        box(m, "CT_Concrete", X1, X1 + 1.0, yx + 1.1 + 0.3 * k, yx + 1.1 + 0.3 * (k + 1), 0.0, zl - 0.16 * (k + 1), faces="xXyYZ")
    det_pts = [(X1 + 0.97, yx - 1.1), (X1 + 0.97, yx + 1.1 + 1.5)]
    B.railing(det_pts, zl, h=0.95, mat="CT_Steel", post=1.2)
    B.footprint = [(X0, -3.5), (X1, -3.5), (X1, yx - 1.1), (X1 + 1.0, yx - 1.1), (X1 + 1.0, yx + 2.6), (X1, yx + 2.6),
                   (X1, YB + 0.45), (X0, YB + 0.45)]
    # lamps: foyer 2 x 6, hall 3 x 5, warm
    for lx in (-15.0, -9.0, -3.0, 3.0, 9.0, 15.0):
        for ly in (5.5, 10.0):
            B.lamp((lx, ly, ZF - 0.3), lumens=800, colour="#FFD2A0", radius=7.5)
    for lx in (-12.0, -6.0, 0.0, 6.0, 12.0):
        for ly in (16.5, 21.5, 26.5):
            B.lamp((lx, ly, HZ - 0.3), lumens=650, colour="#FFC890", radius=9.0)
    B.height = ZC
    B.note("350 seats; three cantilevered volumes; side exit east")


def _closed_top_bottom(B, poly, z0, z1):
    hface(B.m, "CT_Concrete", poly, z1, True)
    hface(B.m, "CT_ConcreteDark", poly, z0, False)


def _volume_a(B):
    z0, z1 = 6.0, 12.4
    P = [(-17.5, YF), (-9.5, YF), (-9.5, -1.0), (-17.5, -3.0)]          # clockwise: walls walked the other way
    _closed_top_bottom(B, P, z0, z1)
    wf0, wf1 = (-17.5, -3.0), (-9.5, -1.0)
    L = math.hypot(8.0, 2.0)
    holes = [(0.6, 2.9, 7.6, 11.2), (3.5, 4.5, 7.6, 11.2), (5.1, 7.4, 7.6, 11.2)]
    wall(B, wf0, wf1, z0, z1, "CT_Concrete", holes=holes, depth=0.4, mat_in="CT_Concrete")
    for a, b, *_ in holes:
        win(B, wf0, wf1, a, b, 7.6, 11.2, kind="public", cols=3 if b - a > 2 else 1, rows=2, inset=0.2, frame=0.05)
    # a red band under the roof edge
    wall(B, (-17.5, YF), (-17.5, -3.0), z0, z1, "CT_Concrete", holes=[(0.8, 5.2, 7.6, 11.2)], depth=0.4)
    win(B, (-17.5, YF), (-17.5, -3.0), 0.8, 5.2, 7.6, 11.2, kind="public", cols=4, rows=2, inset=0.2, frame=0.05)
    wall(B, (-9.5, -1.0), (-9.5, YF), z0, z1, "CT_Concrete", depth=0.4)
    # red band across the slanted front
    d = (8.0 / L, 2.0 / L)
    n = (d[1], -d[0])
    B.m.face("CT_PlasterRed", [(wf0[0] + n[0] * 0.05, wf0[1] + n[1] * 0.05, 11.5), (wf1[0] + n[0] * 0.05, wf1[1] + n[1] * 0.05, 11.5),
                                (wf1[0] + n[0] * 0.05, wf1[1] + n[1] * 0.05, 12.0), (wf0[0] + n[0] * 0.05, wf0[1] + n[1] * 0.05, 12.0)])


def _volume_c(B):
    z0, z1 = 6.4, 10.8
    P = [(9.5, YF), (17.5, YF), (17.5, -3.5), (9.5, -0.5)]
    _closed_top_bottom(B, P, z0, z1)
    wf0, wf1 = (9.5, -0.5), (17.5, -3.5)
    holes = [(0.5, 3.6, 7.8, 10.2), (4.4, 7.5, 7.8, 10.2)]
    wall(B, wf0, wf1, z0, z1, "CT_Concrete", holes=holes, depth=0.4)
    for a, b in ((0.5, 3.6), (4.4, 7.5)):
        win(B, wf0, wf1, a, b, 7.8, 10.2, kind="public", cols=3, rows=1, inset=0.2, frame=0.05)
    wall(B, (17.5, -3.5), (17.5, YF), z0, z1, "CT_Concrete", holes=[(1.5, 5.5, 7.8, 10.2)], depth=0.4)
    win(B, (17.5, -3.5), (17.5, YF), 1.5, 5.5, 7.8, 10.2, kind="public", cols=3, rows=1, inset=0.2, frame=0.05)
    wall(B, (9.5, YF), (9.5, -0.5), z0, z1, "CT_Concrete", depth=0.4)


def _wedge_b(B):
    m = B.m
    xa, xb = -6.0, 6.0
    yf = -3.5
    zb0, zb1 = 9.0, 17.2           # front bottom / top
    zk0, zk1 = 7.0, 16.0           # at the back (y = YF)
    # soffit (faces down) and roof (faces up), sloping towards the street
    m.face("CT_ConcreteDark", [(xa, YF, zk0), (xb, YF, zk0), (xb, yf, zb0), (xa, yf, zb0)][::-1])
    m.face("CT_Concrete", [(xa, YF, zk1), (xb, YF, zk1), (xb, yf, zb1), (xa, yf, zb1)][::-1])
    # side faces (x = xa faces -x; x = xb faces +x)
    m.face("CT_Concrete", [(xa, YF, zk0), (xa, yf, zb0), (xa, yf, zb1), (xa, YF, zk1)])
    m.face("CT_Concrete", [(xb, yf, zb0), (xb, YF, zk0), (xb, YF, zk1), (xb, yf, zb1)])
    # front wall: three window groups
    f0, f1 = (xa, yf), (xb, yf)
    holes = [(0.7, 2.3, 9.8, 13.0), (5.2, 6.8, 9.8, 13.0), (9.7, 11.3, 9.8, 13.0)]
    wall(B, f0, f1, zb0, zb1, "CT_Concrete", holes=holes, depth=0.4)
    for a, b, z0, z1 in holes:
        win(B, f0, f1, a, b, z0, z1, kind="public", cols=2, rows=3, inset=0.2, frame=0.05)
    # signage: red board + neon band, red edge strips
    box(m, "CT_PlasterRed", -5.4, 5.4, yf - 0.2, yf, 14.3, 16.7)
    box(B.detail, "CT_Neon", -5.4, 5.4, yf - 0.1, yf, 13.6, 14.0)
    for a, b in ((-6.0, -5.6), (5.6, 6.0)):
        box(m, "CT_PlasterRed", a, b, yf - 0.15, yf, zb0, zb1)


def _hall_seats(B, centres, y0, rows, going, rise):
    """Rows of folding cinema seats (kit.cinema_row, 7 seats a block); the 350 'seatN' spots sit on the seats. A few
    seat things (the hero sits and watches the film)."""
    n = 0
    for i in range(rows):
        top = rise * (i + 1)
        yc = y0 + going * i + going / 2
        for bi, cx in enumerate(centres):
            r = kit.cinema_row(B, cx, yc, top, 0.0, n=7, mat="CT_PlasterRed" if (i + bi) % 2 == 0 else "CT_SteelRed",
                               solid=False)
            for k in range(7):
                n += 1
                sx, sy, sz, syaw = r["seats"][k]
                B.spot("seat%d" % n, "seat", (sx, yc, top), 0.0)
            if i in (1, 4, 7) or (i == 9 and bi in (0, 2, 4)):
                B.thing("seat_r%d_b%d" % (i + 1, bi + 1), "screen", (cx, yc, top + 0.45), 0.0, what="cinema", height=0.45)
        for ax in (-8.7, -2.9, 2.9, 8.7):                 # aisle lights on the risers
            box(B.detail, "CT_Neon", ax - 0.14, ax + 0.14, y0 + going * i - 0.03, y0 + going * i, rise * i, rise * i + 0.07)


def _hall_decor(B):
    """The auditorium walls: framed film posters between the ribs, an extinguisher, plants at the stage."""
    xw, xe = X0 + 0.4, X1 - 0.4
    west = (("poster_cons_3", 15.8), ("poster_sun_1", 19.6), ("poster_health", 23.4), ("poster_cons_4", 27.2))
    for nm, y in west:
        kit.poster(B, xw, y, 0.0, 90.0, name=nm, w=1.15, zc=3.7)
    east = (("poster_birds_0", 16.2), ("poster_safety", 18.6), ("travel_2", 25.6), ("kids_0", 28.0))
    for nm, y in east:
        kit.poster(B, xe, y, 0.0, 270.0, name=nm, w=1.15, zc=3.7)
    kit.fire_extinguisher(B, xe, 24.4, 1.28, 270.0)
    kit.plant(B, -15.6, 13.6, 0.0, 0.0, kind="palm", pot="CT_PlasterRed", h=2.2)
    kit.plant(B, 15.6, 13.6, 0.0, 0.0, kind="palm", pot="CT_PlasterRed", h=2.2)
    B.thing("hall_board_w", "board", (xw, 21.6, 1.6), 90.0, topic="cinema")
    kit.notice_board(B, xw, 21.6, 0.0, 90.0, w=1.2, zc=1.6, seed=3)
    B.thing("hall_stage_step", "screen", (0.0, 13.2, 0.9), 180.0, what="cinema")


def _foyer(B):
    """Ground foyer 35 x 8.8 m (inner faces x +-17.1, y 3.4..12.2): ticket office, buffet, cloakroom along the divider
    wall, poster wall, hobby corners (chess + radio amateurs in the west, dance in the east), a lounge, benches."""
    det = B.detail
    yp = YD - 0.4                      # the foyer face of the divider wall
    yw = YF + 0.4                      # the inner face of the front wall
    # ---------------------------------------------------------------- ticket office (the booth shell stays)
    kit.shop_counter(B, -9.0, 11.0, 0.0, 0.0, w=5.7, d=0.6, h=1.05, top="CT_WoodLight", front="CT_PlasterBlue", till=False)
    kit.cash_register(B, -7.4, 11.0, 1.05, 0.0)
    kit.telephone(B, -10.6, 11.15, 1.05, 180.0)
    kit.books_stack(B, -9.0, 11.1, 1.05, 0.0, n=3, seed=2)
    kit.cup(B, -8.2, 11.2, 1.05, 0.0)
    kit.sign_plate(B, -9.0, 10.6, 0.0, 0.0, name="sign_till", w=3.0, zc=2.35)
    kit.chair(B, -10.2, 11.75, 0.0, 0.0, kind="school", mat="CT_PlasterRed")
    kit.filing_cabinet(B, -6.5, yp, 0.0, 0.0)
    kit.wall_clock(B, -11.2, yp, 0.0, 0.0, zc=2.4, face="clock_1")
    kit.table_lamp(B, -11.2, 11.2, 1.05, 0.0)
    B.spot("ticket_counter", "counter", (-9.0, 10.2, 0.0), 180.0)
    B.spot("ticket_work", "work", (-9.0, 11.75, 0.0), 0.0)
    B.thing("ticket_counter", "counter", (-9.0, 11.0, 1.05), 0.0, role="clerk", service="tickets")
    # ---------------------------------------------------------------- cloakroom
    kit.shop_counter(B, -15.8, 10.2, 0.0, 0.0, w=2.4, d=0.6, h=1.05, top="CT_WoodLight", front="CT_SteelRed", till=False)
    kit.coat_rack(B, -15.8, 11.75, 0.0, 0.0, w=2.4, coats=0.8, seed=5)
    kit.coat_rack(B, -15.8, 11.2, 0.0, 180.0, w=2.4, coats=0.5, seed=6, solid=False)
    kit.sign_plate(B, -15.8, yp, 0.0, 0.0, name="sign_cloakroom", w=1.9, zc=2.7)
    kit.plates(B, -16.6, 10.2, 1.05, 0.0, n=2)
    kit.hat_stand(B, -14.55, 9.0, 0.0, 0.0)
    B.spot("coat_check", "counter", (-15.8, 9.3, 0.0), 180.0)
    B.spot("coat_work", "work", (-15.8, 10.95, 0.0), 0.0)
    B.thing("coat_check", "counter", (-15.8, 10.2, 1.05), 0.0, role="clerk", service="cloakroom")
    # ---------------------------------------------------------------- buffet
    bar = kit.bar_counter(B, 0.0, 10.4, 0.0, 0.0, w=8.6, d=0.65, h=1.05, taps=0, back=True, seed=4,
                          front="CT_PlasterRed", top="CT_WoodLight")
    kit.samovar(B, -3.6, 10.35, 1.05, 0.0)
    for k in range(4):
        kit.cup(B, -2.6 + 0.3 * k, 10.2, 1.05, 0.0)
    kit.plates(B, -0.8, 10.3, 1.05, 0.0, n=3)
    kit.teapot(B, 0.5, 10.4, 1.05, 0.0)
    kit.glass(B, 1.0, 10.2, 1.05, 0.0)
    kit.glass(B, 1.2, 10.3, 1.05, 0.0)
    kit.bottles(B, 2.2, 10.45, 1.05, 0.0, n=3, seed=1)
    kit.cash_register(B, 3.5, 10.45, 1.05, 0.0)
    kit.sign_plate(B, 0.0, yp, 0.0, 0.0, name="sign_buffet", w=2.6, zc=2.6)
    B.spot("buffet", "counter", (0.0, 9.5, 0.0), 180.0)
    B.spot("buffet_work", "work", (0.0, 11.2, 0.0), 0.0)
    B.thing("buffet", "counter", (0.0, 10.4, 1.05), 0.0, role="clerk", service="bar", shop="club_buffet")
    for k, (tx, ty) in enumerate(((-3.9, 7.2), (3.9, 7.2), (8.4, 7.2))):
        kit.table_round(B, tx, ty, 0.0, 0.0, r=0.38, h=1.0, mat="CT_WoodLight")
        kit.cup(B, tx + 0.12, ty - 0.05, 1.0, 0.0)
        kit.glass(B, tx - 0.12, ty + 0.1, 1.0, 0.0)
        B.spot("buffet_table_%d" % (k + 1), "table", (tx, ty, 0.0), 0.0)
        for j, (ox, oy, yw_) in enumerate(((0.0, -0.75, 180.0), (0.0, 0.75, 0.0))):
            B.spot("buffet_stand_%d_%d" % (k + 1, j + 1), "stand", (tx + ox, ty + oy, 0.0), yw_)
    # ---------------------------------------------------------------- poster wall (east of the buffet) and banners
    for k, nm in enumerate(("cinema_0", "cinema_1", "cinema_2", "cinema_3")):
        kit.poster(B, 6.55 + 1.35 * k, yp, 0.0, 0.0, name=nm, w=1.05, zc=1.95)
    box(det, "CT_Steel", 5.75, 11.05, yp - 0.02, yp, 0.95, 1.03)                       # poster-wall rail
    B.thing("poster_wall", "board", (8.4, yp - 0.02, 1.95), 0.0, topic="cinema")
    kit.banner(B, -9.0, yp, 0.0, 0.0, name="poster_cons_0", w=1.4, zc=3.6)
    kit.banner(B, 0.0, yp, 0.0, 0.0, name="poster_cons_1", w=1.4, zc=3.9)
    kit.banner(B, 8.4, yp, 0.0, 0.0, name="poster_cons_2", w=1.4, zc=3.7)
    kit.sign_plate(B, -13.0, yp, 0.0, 0.0, name="sign_cinema", w=1.6, zc=2.85)
    kit.sign_plate(B, 13.0, yp, 0.0, 0.0, name="sign_quiet", w=1.6, zc=2.85)
    kit.wall_clock(B, 4.9, yp, 0.0, 0.0, zc=3.0, face="clock_2")
    kit.poster(B, -9.65, yw, 0.0, 180.0, name="travel_0", w=0.55, zc=1.7)
    kit.poster(B, 9.65, yw, 0.0, 180.0, name="travel_1", w=0.55, zc=1.7)
    kit.poster(B, -2.15, yw, 0.0, 180.0, name="poster_sun_2", w=0.5, zc=1.7)
    kit.poster(B, 2.15, yw, 0.0, 180.0, name="cinema_4", w=0.5, zc=1.7)
    # ---------------------------------------------------------------- front-wall benches (kept spots)
    for i, x in enumerate((-13.0, -5.0, 5.0, 13.0)):
        r = kit.bench(B, x, yw + 0.28, 0.0, 180.0, w=2.0, kind="park")
        B.spot("foyer_bench%d" % (i + 1), "bench", (x, yw + 0.4, 0.0), 180.0)
        B.thing("foyer_seat_%d" % (i + 1), "seat", (x, yw + 0.4, 0.45), 180.0, height=0.45)
    for x in (-6.0, 6.0):
        B.column_round(x, 8.0, 0.0, ZF, 0.35, mat="CT_Concrete", segs=14)
    # ---------------------------------------------------------------- west: chess corner and radio amateurs
    kit.rug(B, -14.0, 6.0, 0.0, 0.0, w=4.4, d=3.0, name="rug_3")
    kit.bookshelf(B, X0 + 0.4, 4.0, 0.0, 90.0, w=0.9, topic="old", seed=11)
    kit.bookshelf(B, X0 + 0.4, 5.0, 0.0, 90.0, w=0.9, topic="modern", seed=12)
    B.thing("shelf_chess", "shelf", (X0 + 0.6, 4.0, 1.3), 90.0, topic="chess")
    B.thing("shelf_radio", "shelf", (X0 + 0.6, 5.0, 1.3), 90.0, topic="engineering")
    kit.picture(B, X0 + 0.4, 6.05, 0.0, 90.0, name="chess_board", w=0.55, zc=1.6)
    for k, cx in enumerate((-15.2, -12.8)):
        kit.chess_table(B, cx, 6.2, 0.0, 0.0, seed=k)
        kit.chair(B, cx, 5.55, 0.0, 180.0, kind="wood")
        kit.chair(B, cx, 6.85, 0.0, 0.0, kind="wood")
        B.thing("chess_%d" % (k + 1), "play", (cx, 6.2, 0.72), 0.0, what="chess")
        B.spot("chess_seat_%d" % (k + 1), "seat", (cx, 5.55, 0.0), 180.0)
    kit.table(B, -8.6, 5.4, 0.0, 0.0, w=2.2, d=0.8, mat="CT_WoodLight")
    for k, rx in enumerate((-9.3, -8.6, -7.9)):
        kit.radio(B, rx, 5.45, 0.75, 0.0)
    kit.table_lamp(B, -9.6, 5.4, 0.75, 0.0)
    kit.typewriter(B, -7.5, 5.25, 0.75, 0.0)
    kit.chair(B, -9.0, 6.1, 0.0, 0.0, kind="school")
    kit.chair(B, -8.0, 6.1, 0.0, 0.0, kind="school")
    B.thing("radio_club_table", "craft", (-8.6, 5.4, 0.78), 180.0, what="workbench")
    B.thing("radio_seat", "seat", (-9.0, 6.1, 0.45), 0.0, height=0.45)
    kit.plant(B, -16.5, 8.7, 0.0, 0.0, kind="ficus", pot="CT_PlasterBlue", h=1.6)
    kit.plant(B, -10.5, 4.1, 0.0, 0.0, kind="fern", pot="CT_PlasterGreen", h=0.7)
    # ---------------------------------------------------------------- east: dance floor, piano, lounge
    box(det, "CT_WoodLight", 10.9, 16.9, 4.4, 8.6, 0.0, 0.02, faces="xXyYZ")
    box(det, "CT_ConcreteDark", 10.9, 16.9, 4.4, 4.46, 0.02, 0.025, faces="Z")           # inlaid border lines
    box(det, "CT_ConcreteDark", 10.9, 16.9, 8.54, 8.6, 0.02, 0.025, faces="Z")
    box(det, "CT_ConcreteDark", 10.9, 10.96, 4.4, 8.6, 0.02, 0.025, faces="Z")
    box(det, "CT_ConcreteDark", 16.84, 16.9, 4.4, 8.6, 0.02, 0.025, faces="Z")
    kit.table(B, 15.0, 3.75, 0.0, 0.0, w=0.8, d=0.5, h=0.65, mat="CT_WoodLight")
    kit.record_player(B, 15.0, 3.75, 0.65, 0.0)
    kit.piano(B, X1 - 0.43, 4.2, 0.0, 270.0)
    kit.stool(B, 16.0, 4.2, 0.0, 90.0)
    kit.mirror(B, X1 - 0.4, 5.55, 0.0, 270.0, zc=1.5, w=0.8, h=1.7)
    B.thing("hall_piano", "play", (X1 - 0.8, 4.2, 1.0), 270.0, what="piano")
    B.thing("dance_floor", "sport", (13.8, 6.5, 0.0), 0.0, what="dance")
    B.thing("dance_mirror", "board", (X1 - 0.42, 5.55, 1.5), 270.0, topic="view")
    kit.sofa(B, X1 - 0.6, 9.6, 0.0, 270.0, seats=2, mat="CT_PlasterBlue")
    kit.coffee_table(B, 15.5, 9.6, 0.0, 90.0, w=0.9, d=0.5)
    kit.armchair(B, 15.1, 11.2, 0.0, 0.0, mat="CT_PlasterRed")
    kit.rug(B, 15.6, 10.2, 0.0, 0.0, w=2.6, d=2.8, name="rug_1")
    kit.magazine_rack(B, 16.3, yp, 0.0, 0.0, w=0.9)
    kit.newspaper(B, 15.5, 9.55, 0.42, 0.0)
    kit.cup(B, 15.4, 9.75, 0.42, 0.0)
    kit.notice_board(B, X1 - 0.4, 11.6, 0.0, 270.0, w=0.9, zc=1.6, seed=7)
    kit.plant(B, 13.9, 11.3, 0.0, 0.0, kind="monstera", pot="CT_PlasterYellow", h=1.5)
    B.thing("lounge_sofa", "seat", (X1 - 0.9, 9.6, 0.45), 270.0, height=0.45)
    B.thing("lounge_armchair", "seat", (15.1, 11.2, 0.45), 0.0, height=0.45)
    B.thing("magazines", "shelf", (16.3, yp - 0.15, 1.0), 0.0, topic="local")
    B.thing("foyer_board", "board", (X1 - 0.45, 11.6, 1.6), 270.0, topic="club")
    # seated café tables (west of the lane and east of the columns)
    for k, (tx, ty) in enumerate(((-8.8, 8.7), (7.2, 5.0))):
        kit.table_round(B, tx, ty, 0.0, 0.0, r=0.45, h=0.75)
        kit.cup(B, tx + 0.1, ty, 0.75, 0.0)
        kit.vase(B, tx - 0.1, ty + 0.1, 0.75, 0.0, seed=k)
        cs = kit.around_table(B, tx, ty, 0.8, 0.8, 0.0, per_side=(1, 1, 1, 1), chair_fn=kit.chair, kind="wood")
        B.thing("cafe_seat_%d" % (k + 1), "seat", (cs[0]["seats"][0][0], cs[0]["seats"][0][1], 0.45), cs[0]["seats"][0][3],
                height=0.45)
    # pendants over the buffet and the corners (no light: the building's own lamps stay)
    for px, py in ((-2.5, 9.1), (2.5, 9.1), (-14.0, 7.0), (14.0, 7.0)):
        kit.pendant(B, px, py, ZF, drop=0.9, kind="cone", light=False)


def _rod(m, x, y, z0, z1, r, mat, segs):
    prism(m, mat, [(x + r * math.cos(2 * math.pi * k / segs), y + r * math.sin(2 * math.pi * k / segs))
                   for k in range(segs)], z0, z1)


def _deepen(B):
    """Second pass: the chess ladder, the radio amateurs' corner, a door with a rehearsal behind it, the ticket stubs,
    the buffet menu, the lost-property stand, the old film tins on the stage step and the extra people's spots."""
    det = B.detail
    yp = YD - 0.4
    xw, xe = X0 + 0.4, X1 - 0.4
    # ---------------------------------------------------------------- chess ladder (chalk board, west wall)
    xb = xw + 0.65                      # a standing board (the wall here is a window)
    kit.blackboard(B, xb, 7.25, 0.0, 90.0, w=0.95, h=0.75, zb=0.9, writing="board_3")
    for yl in (6.74, 7.71):
        box(det, "CT_Wood", xb - 0.1, xb + 0.0, yl, yl + 0.05, 0.0, 1.75)
    box(det, "CT_Wood", xb - 0.1, xb + 0.0, 6.74, 7.76, 1.7, 1.75)
    B.thing("chess_ladder", "board", (xb + 0.06, 7.25, 1.28), 90.0, topic="chess")
    B.spot("chess_ladder_stand", "stand", (xb + 1.0, 7.25, 0.0), 270.0)
    for k, cx in enumerate((-15.2, -12.8)):                 # the second seat of each board
        B.spot("chess_seat_%db" % (k + 1), "seat", (cx, 6.85, 0.0), 0.0)
    # ---------------------------------------------------------------- radio amateurs: two sets on the table
    B.thing("radio_set_a", "craft", (-9.3, 5.45, 0.92), 180.0, what="radio")
    B.thing("radio_set_b", "craft", (-7.9, 5.45, 0.92), 180.0, what="radio_set")
    box(det, "CT_Steel", -8.32, -8.18, 5.62, 5.74, 0.75, 0.78)                  # a morse key on a bakelite base
    box(det, "CT_Rubber", -8.34, -8.16, 5.60, 5.76, 0.75, 0.765)
    box(det, "CT_Rubber", -8.95, -8.8, 5.62, 5.72, 0.75, 0.78)                  # headphones, one cup each
    box(det, "CT_Rubber", -8.77, -8.62, 5.62, 5.72, 0.75, 0.78)
    for k, rx in enumerate((-9.0, -8.0)):
        B.spot("radio_seat_%d" % (k + 1), "seat", (rx, 6.1, 0.0), 0.0)
    # ---------------------------------------------------------------- a door in the east wall with a rehearsal behind
    box(det, "CT_PlasterGreen", xe - 0.18, xe, 6.55, 8.35, 0.0, 2.55, faces="xyYZz")      # a plywood partition built in
    xd = xe - 0.18                                                                         # front of the window
    box(det, "CT_WoodLight", xd - 0.09, xd, 6.95, 7.95, 0.0, 2.1, faces="xyYZz")
    box(det, "CT_Wood", xd - 0.06, xd - 0.02, 7.03, 7.87, 0.06, 2.04, faces="xyYZz")
    box(det, "CT_Brass", xd - 0.09, xd - 0.04, 7.78, 7.84, 0.98, 1.02, faces="xyYZz")
    box(det, "CT_PlasterRed", xd - 0.12, xd, 7.25, 7.65, 2.18, 2.34, faces="xyYZz")  # the red "quiet" lamp over it
    box(det, "CT_Neon", xd - 0.125, xd - 0.12, 7.3, 7.6, 2.21, 2.31, faces="x")
    B.lamp((xd - 0.4, 7.45, 2.35), lumens=110, colour="#FF5A40", radius=2.4)
    B.thing("rehearsal_door", "board", (xd - 0.06, 7.45, 1.4), 270.0, topic="rehearsal")
    B.spot("rehearsal_listen", "stand", (xd - 0.8, 7.45, 0.0), 90.0)
    # ---------------------------------------------------------------- dance floor and piano: people's places
    for k, (dx, dy, dyaw) in enumerate(((12.3, 5.4, 90.0), (13.2, 5.4, 270.0), (14.8, 7.6, 90.0), (15.7, 7.6, 270.0))):
        B.spot("dance_stand_%d" % (k + 1), "stand", (dx, dy, 0.0), dyaw)
    B.spot("piano_stool", "seat", (16.0, 4.2, 0.0), 90.0)
    B.spot("lounge_seat_1", "seat", (X1 - 0.9, 9.3, 0.0), 270.0)
    B.spot("lounge_seat_2", "seat", (X1 - 0.9, 9.9, 0.0), 270.0)
    # ---------------------------------------------------------------- ticket stubs on the box-office counter
    box(det, "CT_WoodLight", -7.0, -6.6, 10.72, 10.88, 1.05, 1.07)
    box(det, "CT_WoodLight", -7.0, -6.6, 10.72, 10.74, 1.07, 1.11)
    box(det, "CT_WoodLight", -7.0, -6.6, 10.86, 10.88, 1.07, 1.11)
    for k, c in enumerate(("CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterBlue", "CT_PlasterRed", "CT_PlasterYellow")):
        box(det, c, -6.95 + 0.07 * k, -6.9 + 0.07 * k, 10.75, 10.84, 1.07, 1.1 + 0.005 * k)
    B.thing("ticket_stubs", "board", (-6.8, 10.8, 1.14), 0.0, topic="stubs")
    # ---------------------------------------------------------------- buffet menu board (divider wall)
    kit.menu_board(B, -5.1, yp, 0.0, 0.0, name="menu_1", w=0.8, zc=1.9)
    B.thing("buffet_menu", "board", (-5.1, yp - 0.04, 1.9), 0.0, topic="prices")
    # ---------------------------------------------------------------- lost property: an umbrella bucket by the hat stand
    _rod(det, -13.95, 9.05, 0.0, 0.34, 0.17, "CT_Steel", 12)
    for k, (ox, oy, hh) in enumerate(((-0.06, 0.0, 0.92), (0.04, 0.05, 0.84), (0.05, -0.05, 0.78))):
        _rod(det, -13.95 + ox, 9.05 + oy, 0.3, hh, 0.012, "CT_Wood", 6)
    B.thing("lost_property", "board", (-14.55, 9.0, 1.45), 0.0, topic="lost")
    # ---------------------------------------------------------------- the old film tins on the stage step, a seat in row 1
    for k, (tx, tw) in enumerate(((-2.4, 0.36), (-1.9, 0.3))):
        box(det, "CT_Steel", tx - tw / 2, tx + tw / 2, 13.0, 13.0 + tw, 0.4, 0.44)
        box(det, "CT_Brass", tx - 0.04, tx + 0.04, 13.0 + tw / 2 - 0.04, 13.0 + tw / 2 + 0.04, 0.44, 0.455)
    box(det, "CT_Paper", -2.2, -1.95, 13.5, 13.65, 0.4, 0.42)                    # the label, still unread
    B.thing("reel_seat", "screen", (0.0, 15.2, 0.16 + 0.45), 0.0, what="cinema", height=0.45)
    _third(B)


def _third(B):
    """Third layer: the cloakroom as a place, the ticket window, the projection booth seen from the hall, the buffet's
    little things, the hobby corners' walls."""
    det = B.detail
    yp = YD - 0.4
    # ---- cloakroom: a bell, number tags on the counter, a key board on the wall
    _rod(det, -14.95, 10.1, 1.05, 1.08, 0.06, "CT_Brass", 10)
    _rod(det, -14.95, 10.1, 1.08, 1.12, 0.045, "CT_Brass", 10)
    box(det, "CT_Brass", -14.97, -14.93, 10.08, 10.12, 1.12, 1.15)
    for k in range(6):
        box(det, "CT_Brass", -16.1 + 0.11 * k, -16.1 + 0.11 * k + 0.07, 10.0, 10.07, 1.05, 1.062)
    box(det, "CT_WoodLight", -14.05, -13.35, yp - 0.03, yp, 1.2, 1.95)
    for r in range(3):
        for c in range(5):
            x = -13.95 + 0.14 * c
            z = 1.32 + 0.22 * r
            box(det, "CT_Brass", x, x + 0.05, yp - 0.05, yp - 0.03, z, z + 0.05)
            box(det, "CT_Steel", x + 0.015, x + 0.035, yp - 0.07, yp - 0.05, z - 0.07, z - 0.04)
    # ---- ticket window: price card, seating plan of the hall (geometry), a roll of tickets
    kit.menu_board(B, -7.7, yp, 0.0, 0.0, name="price_1", w=0.55, zc=1.85)
    box(det, "CT_PlasterBlue", -10.75, -10.05, yp - 0.03, yp, 1.4, 1.95)
    box(det, "CT_WoodLight", -10.78, -10.02, yp - 0.04, yp - 0.03, 1.37, 1.98)
    box(det, "CT_Paper", -10.70, -10.10, yp - 0.045, yp - 0.03, 1.77, 1.80)           # the screen on the plan
    for bl in range(5):
        for row in range(6):
            x = -10.70 + 0.12 * bl
            z = 1.44 + 0.05 * row
            box(det, "CT_PlasterRed", x, x + 0.1, yp - 0.045, yp - 0.03, z, z + 0.035)
    _rod(det, -8.6, 10.85, 1.05, 1.1, 0.06, "CT_PlasterRed", 12)
    box(det, "CT_Paper", -8.56, -8.46, 10.8, 10.95, 1.05, 1.052)
    # ---- the projection booth: two portholes in the back wall, tins on a shelf, a red "film running" lamp
    yb = YB - 0.4
    for px in (-1.1, 1.1):
        box(det, "CT_Steel", px - 0.3, px + 0.3, yb - 0.1, yb, 5.1, 5.7, faces="xXyZzy")
        box(det, "CT_Rubber", px - 0.22, px + 0.22, yb - 0.115, yb - 0.1, 5.18, 5.62, faces="y")
        box(det, "CT_Neon", px - 0.04, px + 0.04, yb - 0.12, yb - 0.115, 5.36, 5.44, faces="y")
    box(det, "CT_PlasterRed", -0.15, 0.15, yb - 0.08, yb, 5.95, 6.1, faces="xXyZz")
    kit.wall_shelf(B, 3.6, yb, 0.0, 180.0, w=1.2, zt=5.0, props=False)
    for k in range(4):
        box(det, "CT_Steel", 3.1 + 0.28 * k, 3.1 + 0.28 * k + 0.24, yb - 0.3, yb - 0.06, 5.0, 5.05 + 0.05 * (k % 2))
        box(det, "CT_Brass", 3.2 + 0.28 * k, 3.2 + 0.28 * k + 0.04, yb - 0.2, yb - 0.16, 5.05, 5.07 + 0.05 * (k % 2))
    # ---- buffet: tumblers in holders, buns under a glass dome
    for k, hx in enumerate((-3.2, -3.0)):
        _rod(det, hx, 10.55, 1.05, 1.12, 0.035, "CT_Steel", 8)
        _rod(det, hx, 10.55, 1.07, 1.13, 0.028, "CT_Glass", 8)
    for k in range(4):
        _rod(det, 2.82 + 0.1 * (k % 2) + 0.0, 10.35 + 0.1 * (k // 2), 1.05, 1.09, 0.05, "CT_PlasterYellow", 8)
    box(det, "CT_Steel", 2.7, 3.1, 10.2, 10.6, 1.05, 1.055)
    _rod(det, 2.92, 10.4, 1.055, 1.23, 0.18, "CT_Glass", 12)
    _rod(det, 2.92, 10.4, 1.23, 1.27, 0.07, "CT_Glass", 10)
    # ---- radio room: QSL-card stand with diplomas on its back
    xs_, ys_ = -8.6, 4.62
    box(det, "CT_WoodLight", xs_ - 0.8, xs_ + 0.8, ys_ - 0.03, ys_ + 0.03, 0.85, 2.0)
    for lx in (-0.75, 0.75):
        box(det, "CT_Wood", xs_ + lx - 0.04, xs_ + lx + 0.04, ys_ - 0.12, ys_ + 0.12, 0.0, 0.85)
    kit.notice_board(B, xs_, ys_ + 0.03, 0.0, 180.0, w=1.5, zc=1.45, seed=21)
    cols = ("CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterBlue", "CT_PlasterGreen", "CT_PlasterPink")
    for k in range(10):
        x = xs_ - 0.68 + 0.15 * k
        z = 1.2 + 0.28 * (k % 2)
        box(det, cols[k % 5], x, x + 0.11, ys_ + 0.07, ys_ + 0.075, z, z + 0.075)
    for k, nm in enumerate(("paper_letter", "paper_typed", "paper_drawing")):
        kit.picture(B, xs_ - 0.5 + 0.5 * k, ys_ - 0.03, 0.0, 0.0, name=nm, w=0.34, zc=1.5)
    # ---- dance shoes on a rack by the floor
    kit.shoe_rack(B, 11.35, YF + 0.4, 0.0, 180.0, w=0.9, rows=3)
