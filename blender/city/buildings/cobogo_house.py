"""Cobogo house: a 3-4 storey apartment block whose whole street face is a perforated cement breeze-block screen,
standing 1.2 m in front of the loggias. An original design in the spirit of the modernist apartment houses of Recife
and Rio (Cobogo screens, butterfly roofs, pilotis over a garden, a free curved stair) - not a copy of any building.

Plot 40 x 30 m, max 18 m. Block x -14..18 (5 bays of 6.4 m), y 2..15; the street is at y < 0.
  * Street face: a continuous cobogo screen (CT_Cobogo, REAL holes as geometry: 0.6 m blocks with diamond and square
    holes in a checker) at y = 2.0, 0.14 thick, framed by white concrete mullions at every bay and beams at every
    floor. Behind it, 1.2 m back (y = 3.34), the loggia wall of every flat with its windows (coloured plaster), so at
    night the lit windows glow through the holes. The screen has a doorway for the lobby.
  * Ground floor + 3 floors of 3.4 m = 4 storeys. 16 flats: ground "0-01..0-03" (bays 0, 1, 3), floors 1 and 2: five
    flats each, floor 3: three big flats under the roof ("3-01..3-03", bays 0+1, 2+3, 4), each with a tall living
    window. The bay at the east end of the ground floor (x 11.6..18) is open: pilotis (4 round columns) over a garden.
  * Butterfly roof: two copper-green planes falling to a central valley (front edge 15.2 m, valley 13.7 m, back 15.4 m),
    white fascia; the end walls of the top floor are V-gables.
  * Access: a gallery at the back on floors 1-3 (solid parapet, flat doors, kitchen windows) reached by a curved
    external stair, a 66-step spiral (22 steps a turn, rise 0.155, going 0.28 at the inner edge, 0.4+ at the walking
    line) round a hollow drum with stair-light slits, at the west end; ground flats and the lobby open to the back yard.
  * Lobby (ground, bay 2): street door through the screen, tiled floor, mailboxes, bench, plant, three lamps, glazed
    back door to the yard.
Entrances: "front" (through the screen), "rear" (lobby, yard side). Spots: lobby bench and view, garden benches.
Material requests: none (CT_Cobogo, CT_Copper, CT_ConcreteWhite, plasters).
"""
import math
import random

import furn_int3 as fu
import geom
from citykit import box, prism, DOOR_W, DOOR_H
from geom import Facade, extrude_profile
from _cons_util import wall, win, hface
from _surrounds import add_surrounds
from _cobogo_house_blocks import cobogo_screen

INFO = {
    "name_ru": "Дом с решёткой-кобого", "name_en": "Cobogo House",
    "style": "brazilian modernism", "use": "residential",
    "inspired_by": "Recife and Rio modernist apartment houses: cobogo breeze-block street screens, butterfly roofs, "
                   "pilotis over a garden, free curved stair; massing and details are our own",
}

XW, XE = -14.0, 18.0
NB, BW = 5, 6.4
YS, YSB, YL, YB, YG = 2.0, 2.14, 3.34, 13.34, 14.94      # screen front/back, loggia wall, back wall, gallery edge
ST, SL = 3.4, 0.22
ZF = [0.0, 3.4, 6.8, 10.2]
XLOB = XW + 2.5 * BW                                      # lobby centre (bay 2)
STAIR = (XW - 2.5, 14.14)                                 # stair axis
FLATS = {0: [(0, 0), (1, 1), (3, 3)], 1: [(b, b) for b in range(5)], 2: [(b, b) for b in range(5)],
         3: [(0, 1), (2, 3), (4, 4)]}
PLASTERS = ("CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterPink", "CT_PlasterGreen", "CT_Plaster")


def roof_top(y):
    pts = [(1.9, 15.2), (8.6, 13.7), (15.1, 15.4)]
    for (ya, za), (yb, zb) in zip(pts, pts[1:]):
        if y <= yb:
            return za + (zb - za) * (y - ya) / (yb - ya)
    return pts[-1][1]


def roof_under(y):
    return roof_top(y) - 0.25


def bx(b):
    return XW + b * BW


def face_up(m, mat, pts, up=True):
    n = geom.cross(geom.sub(pts[1], pts[0]), geom.sub(pts[2], pts[0]))
    if (n[2] > 0) != up:
        pts = pts[::-1]
    m.face(mat, pts)


def pr(m, mat, pts, z0, z1, top=False, bottom=False, side_mat=None):
    a = sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1] for i in range(len(pts)))
    if a < 0:
        pts = pts[::-1]
    prism(m, mat, pts, z0, z1, top=top, bottom=bottom, side_mat=side_mat)


def rect(x0, x1, y0, y1):
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def sloped_slab(m, x0, x1, ya, yb, thick=0.25):
    """A roof plane from y=ya to y=yb (heights from roof_top), top copper, underside and edges white concrete."""
    za, zb = roof_top(ya), roof_top(yb)
    T = [(x0, ya, za), (x1, ya, za), (x1, yb, zb), (x0, yb, zb)]
    Bt = [(p[0], p[1], p[2] - thick) for p in T]
    face_up(m, "CT_Copper", T, True)
    face_up(m, "CT_ConcreteWhite", Bt, False)
    cen = (sum(p[0] for p in T) / 4, sum(p[1] for p in T) / 4, sum(p[2] for p in T) / 4 - thick / 2)
    for i in range(4):
        j = (i + 1) % 4
        q = [T[i], T[j], Bt[j], Bt[i]]
        n = geom.cross(geom.sub(q[1], q[0]), geom.sub(q[2], q[0]))
        fc = tuple(sum(p[k] for p in q) / 4 for k in range(3))
        if sum(n[k] * (fc[k] - cen[k]) for k in range(3)) < 0:
            q = q[::-1]
        m.face("CT_ConcreteWhite", q)


def gable_wall(m, mat, x0, x1, z0, ya, yb):
    """A top-storey cross wall x0..x1 with the butterfly roof profile above it."""
    poly = [(ya, z0), (yb, z0), (yb, roof_under(yb)), (8.6, roof_under(8.6)), (ya, roof_under(ya))]
    extrude_profile(m, mat, mat, poly, x0, x1, lambda u, v, t: (t, u, v))


def tree(B, x, y, h=3.2, r=1.5, mat="CT_Grass"):
    fu.cyl(B.m, "CT_Wood", x, y, 0.0, h * 0.62, 0.13, segs=6)
    fu.blob(B.detail, mat, x, y, h * 0.7, r)
    fu.blob(B.detail, mat, x + r * 0.5, y + r * 0.2, h * 0.55, r * 0.7)


def build(B):
    rnd = random.Random(1952)
    m, det = B.m, B.detail
    owner, first = {}, {}
    names = {}
    for s, lst in FLATS.items():
        for n, (b0, b1) in enumerate(lst):
            fid = "%d-%02d" % (s, n + 1)
            names[(s, b0)] = fid
            for b in range(b0, b1 + 1):
                owner[(s, b)] = fid
                first[(s, b)] = (b == b0)

    # ---------------------------------------------------------------- floors and ground
    for s in range(1, 4):
        z = ZF[s]
        pr(m, "CT_ConcreteDark", rect(XW, XE, YSB, YG), z - SL, z, top=False, bottom=True, side_mat="CT_ConcreteWhite")
        hface(m, "CT_Floor", rect(XW, XE, YL, YB), z, True)
        hface(m, "CT_Paving", rect(XW, XE, YSB, YL), z, True)
        hface(m, "CT_Paving", rect(XW, XE, YB, YG), z, True)
    hface(m, "CT_Floor", rect(XW, bx(4), YL, YB), 0.0, True)
    hface(m, "CT_Paving", rect(XW, bx(4), YSB, YL), 0.0, True)
    hface(m, "CT_Paving", rect(XW - 0.3, XE + 2.0, YB, 15.9), 0.0, True)
    hface(m, "CT_Paving", rect(bx(4) + 0.5, XE - 0.5, 4.0, 5.2), 0.0, True)          # path through the pilotis garden
    for (x0, x1, y0, y1) in ((-20.0, 20.0, 0.0, 1.9), (-20.0, 20.0, 16.4, 30.0), (bx(4), 20.0, YSB, YB),
                              (XE, 20.0, 1.9, 16.4), (-20.0, XW - 3.0, 1.9, 16.4)):
        hface(m, "CT_Grass", rect(x0, x1, y0, y1), 0.01, True)

    # ---------------------------------------------------------------- loggia walls (front) and back walls
    for s in range(4):
        for b in range(NB):
            x0, x1 = bx(b), bx(b) + BW
            z0 = ZF[s]
            top = s == 3
            p0, p1 = (x0, YL), (x1, YL)
            q0, q1 = (x1, YB), (x0, YB)
            if s == 0 and b == 4:
                continue
            if s == 0 and b == 2:                                    # lobby: open doorway at the front, glazed back
                wall(B, p0, p1, 0.0, ZF[1] - SL, "CT_Plaster", holes=[(1.6, 4.8, 0.0, 2.8)], depth=0.3, mat_in="CT_Interior")
                holes = [(BW / 2 - 0.8, BW / 2 + 0.8, 0.0, 2.4), (0.5, 1.6, 0.5, 2.5), (BW - 1.6, BW - 0.5, 0.5, 2.5)]
                wall(B, q0, q1, 0.0, ZF[1] - SL, "CT_ConcreteWhite", holes=holes, depth=0.3, mat_in="CT_Interior")
                for h in holes[1:]:
                    win(B, q0, q1, h[0], h[1], h[2], h[3], kind="stair", cols=2, rows=1, inset=0.2)   # lobby: dusk to dawn
                Fq = Facade(q0, q1)
                B.entrance("rear", Fq.to_building((BW / 2, 0.0, 0.0)), 180.0, 1.6, 2.4, "street", leaves=2)
                continue
            fid = owner[(s, b)]
            z1 = roof_under(YL) if top else ZF[s + 1] - SL
            zb1 = roof_under(YB) if top else ZF[s + 1] - SL
            mat = PLASTERS[(b + 2 * s) % 5]
            liv = (0.35, 3.75, z0 + 0.15, z0 + (3.5 if top else 2.75))
            bed = (4.25, 6.05, z0 + 0.9, z0 + 2.5)
            wall(B, p0, p1, z0, z1, mat, holes=[liv, bed], depth=0.3, mat_in="CT_Interior")
            win(B, p0, p1, liv[0], liv[1], liv[2], liv[3], flat=fid, cols=2, rows=2 if top else 1, inset=0.2)
            win(B, p0, p1, bed[0], bed[1], bed[2], bed[3], flat=fid, cols=1, rows=1, inset=0.2)
            du = BW - 1.7
            holes = [(BW - 4.2, BW - 2.6, z0 + 1.0, z0 + 2.0)]
            if first[(s, b)]:
                holes.append((du, du + DOOR_W, z0, z0 + DOOR_H))
            else:
                holes.append((du, du + 0.9, z0 + 1.0, z0 + 2.0))
            wall(B, q0, q1, z0, zb1, "CT_ConcreteWhite", holes=holes, depth=0.3, mat_in="CT_Interior")
            win(B, q0, q1, holes[0][0], holes[0][1], holes[0][2], holes[0][3], flat=fid, cols=2, rows=1, inset=0.2)
            Fq = Facade(q0, q1)
            if first[(s, b)]:
                dm = (du + DOOR_W / 2)
                box(det, "CT_Door", x1 - (du + DOOR_W) + 0.0, x1 - du, YB + 0.07, YB + 0.12, z0, z0 + DOOR_H)
                B.flat(fid, storey=s, beds=1 + (b + s) % 3 + (1 if top else 0),
                       door=(x1 - dm, YB + 0.6, z0), entrance="front")
            else:
                win(B, q0, q1, holes[1][0], holes[1][1], holes[1][2], holes[1][3], flat=fid, cols=1, rows=1, inset=0.2)

    # ---------------------------------------------------------------- party walls, end walls
    for k in range(NB + 1):
        x = bx(k)
        for s in range(4):
            if s == 0 and k == 5:
                continue
            t = 0.3 if k in (0, NB) else 0.2
            xa, xb = (x, x + t) if k == 0 else ((x - t, x) if k == NB else (x - t / 2, x + t / 2))
            if 0 < k < NB:
                if s == 0 and k == 5:
                    continue
                if s == 3 and k in (1, 3):
                    continue
            mat = "CT_Concrete" if k in (0, NB) else "CT_ConcreteWhite"
            if s == 3:
                gable_wall(m, mat, xa, xb, ZF[3], YSB, YB)
            else:
                pr(m, mat, rect(xa, xb, YSB, YB), ZF[s], ZF[s + 1] - SL)
            if k in (0, NB):
                pass
    # the ground end wall of the east garden bay is a party wall of flat 0-03 (k = 4) built above (s = 0)

    # ---------------------------------------------------------------- the cobogo screen, mullions, beams
    F = Facade((XW, YS), (XE, YS))                 # u = x - XW, y out = -y world
    top = roof_under(YSB) - 0.04
    for k in range(NB + 1):
        u = k * BW
        z0 = ZF[1] - SL if k == 5 else 0.0
        u0, u1 = max(0.0, u - 0.15), min(NB * BW, u + 0.15)
        _lbox(m if z0 == 0.0 else det, "CT_ConcreteWhite", F, u0, u1, -0.14, 0.06, z0, top + 0.04)   # ground mullions solid
    nblk = 0
    holes_v = (0.31, 0.27, 0.34)
    for b in range(NB):
        ua, ub = b * BW + 0.15, (b + 1) * BW - 0.15
        for s in range(4):
            zlo = ZF[s] + (0.25 if s == 0 else 0.17)
            zhi = (ZF[s + 1] - 0.28) if s < 3 else top
            if s == 0 and b == 4:
                continue
            skip = None
            if s == 0 and b == 2:
                cu = (b + 0.5) * BW
                skip = (lambda uc, zc, cu=cu: abs(uc - cu) < 1.0 and zc < 2.7)
            # the ground storey's screen is solid to walk into (shell); the upper ones are detail
            nblk += cobogo_screen(m if s == 0 else det, F, ua, ub, zlo, zhi, y_front=0.0, thick=0.14, block=0.6,
                                  hole=holes_v[(b + s) % 3], skip=skip)
            # beams at the floor lines (between the mullions)
            if s < 3:
                _lbox(m if s == 0 else det, "CT_ConcreteWhite", F, ua, ub, -0.14, 0.06, zhi, ZF[s + 1] + 0.17)
        if b != 4:
            if b == 2:
                cu = (b + 0.5) * BW
                _lbox(m, "CT_ConcreteDark", F, ua, cu - 1.0, -0.14, 0.06, 0.0, 0.25)
                _lbox(m, "CT_ConcreteDark", F, cu + 1.0, ub, -0.14, 0.06, 0.0, 0.25)
            else:
                _lbox(m, "CT_ConcreteDark", F, ua, ub, -0.14, 0.06, 0.0, 0.25)
    # lintel and jambs of the doorway
    cu = (2 + 0.5) * BW
    for (u0, u1) in ((cu - 1.0 - 0.12, cu - 1.0), (cu + 1.0, cu + 1.0 + 0.12)):
        _lbox(m, "CT_ConcreteWhite", F, u0, u1, -0.14, 0.1, 0.0, 2.95)
    # entrance
    B.entrance("front", (XLOB, YS, 0.0), 0.0, 1.8, DOOR_H + 0.1, "street", leaves=2)
    # canopy over the doorway
    box(det, "CT_ConcreteWhite", XLOB - 1.8, XLOB + 1.8, YS - 1.3, YS + 0.2, 3.0, 3.16)

    # azulejo murals on the two end walls (no ground panel on the east: that bay is open pilotis)
    for Fe, lo in ((Facade((XE, YSB), (XE, YB)), 1), (Facade((XW, YB), (XW, YSB)), 0)):
        for s_ in range(lo, 3):
            for q in range(3):
                u0 = 1.8 + q * 2.9
                z0 = ZF[s_] + 0.9
                _lbox(det, "CT_Tile" if (q + s_) % 2 == 0 else "CT_TileWhite", Fe, u0, u0 + 2.4, 0.0, 0.03,
                      z0, z0 + 1.7, faces="Y")

    # ---------------------------------------------------------------- roof
    sloped_slab(m, XW - 0.3, XE + 0.3, 1.9, 8.6)
    sloped_slab(m, XW - 0.3, XE + 0.3, 8.6, 15.1)
    # back fascia between wall top and roof is the wall itself; gallery parapets
    for s in range(1, 4):
        z = ZF[s]
        B.facade((XE, YG), (XW, YG), z, z + 1.0, "CT_ConcreteWhite", depth=0.15, back=True)
        hface(m, "CT_ConcreteWhite", rect(XW, XE, YG - 0.15, YG), z + 1.0, True)
        B.facade((XE, YB), (XE, YG), z, z + 1.0, "CT_ConcreteWhite", depth=0.15, back=True)
        hface(m, "CT_ConcreteWhite", rect(XE - 0.15, XE, YB, YG), z + 1.0, True)
        _gallery_deck(B, z)
    # roof valley gutter
    box(det, "CT_Steel", XW - 0.2, XE + 0.2, 8.45, 8.75, roof_top(8.6) + 0.0, roof_top(8.6) + 0.03)

    # ---------------------------------------------------------------- lobby
    lx0, lx1 = bx(2), bx(3)
    fu.mailboxes(B, fu.Fr(lx0 + 0.15, 7.0, 90, 0.0), 0.0, 0.95, cols=5, rows=3)
    fu.plant(B, lx1 - 0.7, 4.4, 0.0, 0.9, "CT_PlasterRed")
    fu.doormat(B, fu.Fr(XLOB, 3.9, 0, 0.0), 0.0, 0.0)
    box(det, "CT_Wood", lx1 - 1.9, lx1 - 0.3, 9.0, 9.5, 0.42, 0.48)
    box(det, "CT_Concrete", lx1 - 1.8, lx1 - 1.65, 9.05, 9.45, 0.0, 0.42)
    box(det, "CT_Concrete", lx1 - 0.55, lx1 - 0.4, 9.05, 9.45, 0.0, 0.42)
    B.spot("lobby_bench", "bench", (lx1 - 1.1, 9.25, 0.0), 180)
    B.spot("lobby_view", "view", (XLOB, 6.0, 0.0), 0)
    for y in (4.8, 8.0, 11.0):
        B.lamp((XLOB, y, ZF[1] - 0.6), lumens=520, colour="#FFE0B0", kind="lobby", radius=6.0)

    # ---------------------------------------------------------------- pilotis corner, garden
    for (x, y) in ((bx(4) + 0.9, 3.6), (XE - 0.9, 3.6), (bx(4) + 0.9, 12.8), (XE - 0.9, 12.8)):
        for k in range(1):
            B.column_round(x, y, 0.0, ZF[1] - SL, 0.3, mat="CT_ConcreteWhite", segs=12)
    tree(B, 15.0, 8.8, 2.4, 0.8, "CT_Grass")
    for (x, y) in ((13.2, 6.5), (16.6, 10.0), (13.4, 10.8)):
        fu.blob(det, "CT_Grass", x, y, 0.3, 0.5)
    box(det, "CT_Wood", 13.2, 15.0, 4.55, 5.05, 0.42, 0.48)
    box(det, "CT_Concrete", 13.3, 13.45, 4.6, 5.0, 0.0, 0.42)
    box(det, "CT_Concrete", 14.75, 14.9, 4.6, 5.0, 0.0, 0.42)
    B.spot("garden_bench", "bench", (14.1, 4.8, 0.0), 0)
    B.lamp((15.0, 5.0, ZF[1] - 0.5), lumens=700, colour="#FFD9A8", kind="lobby", radius=7.0)
    for (x, y, h, r) in ((-17.0, 24.0, 3.8, 1.8), (-2.0, 22.0, 3.2, 1.5), (9.0, 26.0, 4.0, 1.9), (17.0, 20.0, 3.4, 1.6),
                         (-12.0, 19.0, 3.0, 1.4)):
        tree(B, x, y, h, r, "CT_Grass")
    for (x, y) in ((-6.0, 17.6), (4.0, 17.6), (12.0, 17.6)):
        box(det, "CT_Wood", x - 0.9, x + 0.9, y - 0.25, y + 0.25, 0.42, 0.48)
        box(det, "CT_Concrete", x - 0.8, x - 0.65, y - 0.2, y + 0.2, 0.0, 0.42)
        box(det, "CT_Concrete", x + 0.65, x + 0.8, y - 0.2, y + 0.2, 0.0, 0.42)
        B.spot("yard_bench_%d" % int(x), "bench", (x, y, 0.0), 0)

    # ---------------------------------------------------------------- the curved external stair
    _stair(B)

    B.footprint = [(-19.1, 1.9), (XE + 0.3, 1.9), (XE + 0.3, 16.7), (-19.1, 16.7)]
    B.height = roof_top(15.1) + 0.0
    add_surrounds(B, seed=2, kinds=("street",), bounds=(-20.0, 20.0, 0.3, 30.0))
    B.note("%d flats, %d cobogo blocks" % (len(B.flats), nblk))


def _lbox(m, mat, F, u0, u1, y0, y1, z0, z1, faces="xXyYzZ"):
    c = [F.to_building((u1 if i & 1 else u0, y1 if i & 2 else y0, z1 if i & 4 else z0)) for i in range(8)]
    Fc = {"x": (0, 2, 6, 4), "X": (1, 3, 7, 5), "y": (0, 1, 5, 4), "Y": (2, 3, 7, 6), "z": (0, 1, 3, 2), "Z": (4, 5, 7, 6)}
    cx = [sum(p[i] for p in c) / 8.0 for i in range(3)]
    for k in faces:
        q = [c[i] for i in Fc[k]]
        n = geom.cross(geom.sub(q[1], q[0]), geom.sub(q[2], q[0]))
        fc = [sum(p[i] for p in q) / 4.0 for i in range(3)]
        if sum(n[i] * (fc[i] - cx[i]) for i in range(3)) < 0:
            q = q[::-1]
        m.face(mat, q)


def _gallery_deck(B, z):
    """The west end of the gallery reaches the stair: a short deck from the end wall to the stair's outer edge."""
    cx, cy = STAIR
    pr(B.m, "CT_ConcreteDark", rect(XW - 0.45, XW, cy - 0.8, cy + 0.8), z - SL, z, top=False, bottom=True,
       side_mat="CT_ConcreteWhite")
    hface(B.m, "CT_Paving", rect(XW - 0.45, XW, cy - 0.8, cy + 0.8), z, True)


def _stair(B):
    cx, cy = STAIR
    r0, r1 = 1.0, 2.5
    n_turn = 22
    dth = 2 * math.pi / n_turn
    rise = ST / n_turn
    total = n_turn * 3
    rail = []
    for i in range(total):
        a0, a1 = i * dth, (i + 1) * dth
        zt = rise * (i + 1)
        poly = [(cx + r0 * math.cos(a0), cy + r0 * math.sin(a0)), (cx + r1 * math.cos(a0), cy + r1 * math.sin(a0)),
                (cx + r1 * math.cos(a1), cy + r1 * math.sin(a1)), (cx + r0 * math.cos(a1), cy + r0 * math.sin(a1))]
        pr(B.m, "CT_ConcreteWhite", poly, zt - 0.22, zt, top=True, bottom=True)
        loc = i % n_turn
        if loc not in (0, 1, n_turn - 1, n_turn - 2) and i % 2 == 0:
            rail.append((i, cx + (r1 - 0.04) * math.cos((a0 + a1) / 2), cy + (r1 - 0.04) * math.sin((a0 + a1) / 2), zt))
    # balusters and a handrail along the outer edge
    for (i, x, y, zt) in rail:
        box(B.detail, "CT_Steel", x - 0.025, x + 0.025, y - 0.025, y + 0.025, zt, zt + 1.0)
    for (a, b) in zip(rail, rail[1:]):
        if b[0] - a[0] == 2:
            fu.rod(B.detail, "CT_Steel", (a[1], a[2], a[3] + 1.0), (b[1], b[2], b[3] + 1.0), 0.05, caps=False)
    # hollow drum (12-gon) with stair-light slits
    nseg = 12
    rd = r0 - 0.02
    zt = ST * 3 + 3.2
    for k in range(nseg):
        a, b = 2 * math.pi * k / nseg, 2 * math.pi * (k + 1) / nseg
        p0 = (cx + rd * math.cos(a), cy + rd * math.sin(a))
        p1 = (cx + rd * math.cos(b), cy + rd * math.sin(b))
        L = math.dist(p0, p1)
        holes = []
        if k % 2 == 0:
            holes = [(0.13, L - 0.13, s * ST + 0.55, s * ST + 3.05) for s in range(4)]
        wall(B, p0, p1, 0.0, zt, "CT_ConcreteWhite", holes=holes, depth=0.12, mat_in="CT_Interior")
        for h in holes:
            win(B, p0, p1, h[0], h[1], h[2], h[3], kind="stair", inset=0.1, frame=0.03)
    cap = [(cx + 1.7 * math.cos(2 * math.pi * k / nseg), cy + 1.7 * math.sin(2 * math.pi * k / nseg)) for k in range(nseg)]
    pr(B.m, "CT_PlasterRed", cap, zt, zt + 0.2, top=True, bottom=True)
    # lamps at the gallery landings
    for s in (1, 2, 3):
        B.lamp((XW + 1.0, YG - 0.5, ZF[s] + 2.4), lumens=380, colour="#FFE0B0", kind="stair", radius=5.5, fixture=True)
    B.lamp((cx + 1.8, cy - 1.4, 2.6), lumens=380, colour="#FFE0B0", kind="stair", radius=5.5)
