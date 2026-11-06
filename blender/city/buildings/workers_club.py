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
    B.entrance("side", (X1, yx, 0.0), 90.0, width=2.2, height=2.6, kind="side", leaves=1)
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
    # ------------------------------------------------------------ the three cantilevered volumes
    _volume_a(B)
    _wedge_b(B)
    _volume_c(B)
    # ------------------------------------------------------------ interior: foyer
    box(m, "CT_Wood", -12.0, -6.0, 10.9, 11.7, 0.0, 1.1)
    B.spot("ticket_counter", "counter", (-9.0, 10.2, 0.0), 180.0)
    for i, x in enumerate((-13.0, -5.0, 5.0, 13.0)):
        box(det, "CT_Wood", x - 1.0, x + 1.0, 4.4, 4.9, 0.0, 0.45)
        box(det, "CT_Steel", x - 1.0, x - 0.9, 4.4, 4.9, 0.0, 0.45)
        box(det, "CT_Steel", x + 0.9, x + 1.0, 4.4, 4.9, 0.0, 0.45)
        B.spot("foyer_bench%d" % (i + 1), "bench", (x, 4.65, 0.0), 180.0)
    for x in (-6.0, 6.0):
        B.column_round(x, 8.0, 0.0, ZF, 0.35, mat="CT_Concrete", segs=14)
    box(det, "CT_Wood", -4.0, 4.0, 11.9, 12.2, 0.0, 0.9)
    # ------------------------------------------------------------ interior: hall
    y0 = 14.55
    rows, going, rise = 10, 1.3, 0.16
    for i in range(rows):
        box(m, "CT_Floor", -17.1, 17.1, y0 + going * i, y0 + going * (i + 1) + (0.0 if i < rows - 1 else YB - 0.4 - y0 - going * rows),
            0.0, rise * (i + 1))
    centres = [-11.6, -5.8, 0.0, 5.8, 11.6]
    n = 0
    for i in range(rows):
        top = rise * (i + 1)
        yc = y0 + going * i + going / 2
        for bi, cx in enumerate(centres):
            box(det, "CT_Wood", cx - 2.1, cx + 2.1, yc - 0.3, yc + 0.25, top, top + 0.45)
            box(det, "CT_SteelRed", cx - 2.1, cx + 2.1, yc + 0.25, yc + 0.4, top + 0.45, top + 0.95)
            for s in range(7):
                n += 1
                B.spot("seat%d" % n, "seat", (cx - 1.8 + 0.6 * s, yc, top), 0.0)
    # screen + curtains on the divider wall
    box(det, "CT_Steel", -7.8, 7.8, YD, YD + 0.08, 2.35, 8.45)
    box(det, "CT_TileWhite", -7.5, 7.5, YD + 0.08, YD + 0.13, 2.65, 8.15)
    for a, b in ((-9.6, -7.8), (7.8, 9.6)):
        box(det, "CT_SteelRed", a, b, YD, YD + 0.5, 0.0, 9.4)
    B.footprint = [(X0, -3.5), (X1, -3.5), (X1, YB + 0.45), (X0, YB + 0.45)]
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
