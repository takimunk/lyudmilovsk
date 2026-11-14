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
import interior_kit as kit
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
    B.spot("lobby_bench", "bench", (lx1 - 0.55, 8.6, 0.0), 270)
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

    _furnish_lobby(B)
    _furnish_garden(B)
    _furnish_flat(B)
    _depth_pass(B)
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


# ====================================================================== furnishing (residential interiors)
def _wheel(m, mat, c, u, r, n=10, w=0.03):
    """A thin wheel ring in the vertical plane through c along the horizontal unit vector u."""
    pts = [(c[0] + u[0] * r * math.cos(2 * math.pi * k / n), c[1] + u[1] * r * math.cos(2 * math.pi * k / n),
            c[2] + r * math.sin(2 * math.pi * k / n)) for k in range(n)]
    for k in range(n):
        fu.rod(m, mat, pts[k], pts[(k + 1) % n], w)


def _bike(B, x, y, yaw, mat="CT_PlasterRed"):
    """A leaning city bicycle (detail only) lying along local u (yaw 0: along +x)."""
    a = math.radians(yaw)
    u = (math.cos(a), math.sin(a))
    P = lambda s, z: (x + u[0] * s, y + u[1] * s, z)
    m = B.detail
    for s in (-0.55, 0.55):
        _wheel(m, "CT_Steel", P(s, 0.33), u, 0.33)
    fu.rod(m, mat, P(-0.55, 0.33), P(-0.1, 0.55), 0.035)
    fu.rod(m, mat, P(-0.1, 0.55), P(0.4, 0.62), 0.035)
    fu.rod(m, mat, P(0.4, 0.62), P(0.55, 0.33), 0.035)
    fu.rod(m, mat, P(-0.1, 0.55), P(0.05, 0.3), 0.035)
    fu.rod(m, mat, P(0.4, 0.62), P(0.4, 0.95), 0.03)
    fu.rod(m, "CT_Steel", (P(0.4, 0.95)[0] - u[1] * 0.28, P(0.4, 0.95)[1] + u[0] * 0.28, 0.95),
           (P(0.4, 0.95)[0] + u[1] * 0.28, P(0.4, 0.95)[1] - u[0] * 0.28, 0.95), 0.03)
    box(m, "CT_Leather", min(P(-0.3, 0)[0], P(-0.05, 0)[0]) - 0.05, max(P(-0.3, 0)[0], P(-0.05, 0)[0]) + 0.05,
        min(P(-0.3, 0)[1], P(-0.05, 0)[1]) - 0.05, max(P(-0.3, 0)[1], P(-0.05, 0)[1]) + 0.05, 0.74, 0.79)
    fu.rod(m, mat, P(-0.1, 0.55), P(-0.17, 0.74), 0.03)


def _pram(B, x, y, yaw, mat="CT_PlasterBlue"):
    """A pram: body, hood, handle, four wheels. Local: front toward +v."""
    a = math.radians(yaw)
    u = (math.cos(a), math.sin(a))
    v = (-math.sin(a), math.cos(a))
    P = lambda s, t, z: (x + u[0] * s + v[0] * t, y + u[1] * s + v[1] * t, z)
    m = B.detail
    bx0 = [P(du, dv, 0)[:2] for du, dv in ((-0.27, -0.45), (0.27, -0.45), (0.27, 0.45), (-0.27, 0.45))]
    pr(m, mat, bx0, 0.38, 0.62, top=True, bottom=True)
    hood = [P(du, dv, 0)[:2] for du, dv in ((-0.27, -0.45), (0.27, -0.45), (0.27, -0.05), (-0.27, -0.05))]
    pr(m, "CT_Paper", hood, 0.62, 0.86, top=True, bottom=False)
    for du, dv in ((-0.3, -0.35), (0.3, -0.35), (-0.3, 0.35), (0.3, 0.35)):
        _wheel(m, "CT_Steel", P(du, dv, 0.17), v, 0.17, n=8, w=0.03)
    fu.rod(m, "CT_Steel", P(-0.25, -0.45, 0.6), P(-0.25, -0.75, 1.0), 0.03)
    fu.rod(m, "CT_Steel", P(0.25, -0.45, 0.6), P(0.25, -0.75, 1.0), 0.03)
    fu.rod(m, "CT_Steel", P(-0.25, -0.75, 1.0), P(0.25, -0.75, 1.0), 0.03)


def _furnish_lobby(B):
    """Lobby (bay 2, x -1.2..5.2, y 3.64..13.04, ceiling 3.18): mailboxes (existing), house notice board, concierge
    desk, bench, pram, bike, plants, radio, runner rug, clock, pictures."""
    lx0, lx1 = bx(2), bx(3)
    xw, xe = lx0 + 0.1, lx1 - 0.1                 # inner faces of the party walls
    yf, yb = YL + 0.3, YB - 0.3                   # inner faces of front and back walls
    # runner between the two doors
    kit.rug(B, XLOB - 0.4, 8.3, 0.0, 0.0, w=1.3, d=6.8, name="rug_3")
    # west wall: pram, mailboxes (existing, y 6.3..7.7), wall shelf with a radio, bike
    _pram(B, xw + 0.5, 4.8, 90, "CT_PlasterBlue")
    ws = kit.wall_shelf(B, xw, 9.1, 0.0, 90.0, w=0.9, zt=1.15, seed=3)
    kit.radio(B, xw + 0.14, 9.5, 1.15, 90.0)
    kit.picture(B, xw, 9.1, 0.0, 90.0, name="painting_sea_calm", w=0.7, zc=1.9)
    B.thing("lobby_radio", "machine", (xw + 0.15, 9.5, 1.3), 90.0, what="jukebox")
    _bike(B, xw + 0.38, 11.4, 90.0)
    kit.plant(B, xw + 0.4, 12.5, 0.0, 0.0, kind="ficus", pot="CT_PlasterYellow", h=1.5, seed=2)
    kit.poster(B, xw, 12.0, 0.0, 90.0, name="poster_cons_2", w=0.6, zc=1.6)
    # east wall: bench under the house notice board, concierge desk
    bn = kit.bench(B, xe - 0.25, 8.6, 0.0, 270.0, w=1.6, kind="park")
    B.thing("lobby_bench_seat", "seat", (xe - 0.3, 8.6, 0.45), 270.0, height=0.45)
    kit.notice_board(B, xe, 8.6, 0.0, 270.0, w=1.1, zc=1.7, seed=7)
    B.thing("house_notices", "board", (xe - 0.05, 8.6, 1.7), 270.0, topic="lost")
    kit.picture(B, xe, 6.2, 0.0, 270.0, name="painting_dunes", w=0.8, zc=1.75)
    kit.picture(B, xe, 7.2, 0.0, 270.0, name="painting_lighthouse", w=0.5, zc=1.75)
    d = kit.office_desk(B, 4.0, 11.8, 0.0, 270.0, w=1.4, d=0.7)
    kit.chair(B, 4.85, 11.8, 0.0, 270.0, kind="wood")
    kit.table_lamp(B, 3.95, 12.35, d["top"], 0.0)
    kit.cup(B, 4.0, 11.35, d["top"], 0.0)
    B.thing("concierge_desk", "counter", (4.0, 11.8, 1.0), 270.0, role="concierge", service="reception")
    B.spot("concierge_chair", "seat", (4.85, 11.8, 0.0), 270)
    kit.wall_clock(B, 3.1, yb, 0.0, 0.0, zc=2.55, face="clock_1")
    kit.plant(B, xe - 0.35, 12.75, 0.0, 0.0, kind="fern", pot="CT_PlasterRed", h=1.0, seed=4)
    kit.picture(B, 0.8, yb, 0.0, 0.0, name="painting_still_life", w=0.55, zc=1.7)
    kit.mirror(B, xe, 5.1, 0.0, 270.0, zc=1.5, w=0.55, h=0.9)
    # front wall: poster either side of the door, hat stand
    kit.poster(B, -0.4, yf, 0.0, 180.0, name="poster_birds_1", w=0.55, zc=1.7)
    kit.hat_stand(B, 4.4, 4.2, 0.0, 0.0)
    kit.plant(B, -0.6, 4.4, 0.0, 0.0, kind="palm", pot="CT_PlasterBlue", h=1.6, seed=5)


def _furnish_garden(B):
    """Under the pilotis (bay 4, x 11.6..18, y 2.14..13.34, ceiling 3.18): benches, chess table, ping-pong,
    planters."""
    kit.bench(B, 14.1, 4.8, 0.0, 0.0, w=1.8, kind="park")
    B.thing("garden_bench_seat", "seat", (14.1, 4.8, 0.45), 0.0, height=0.45)
    kit.bench(B, 17.2, 7.2, 0.0, 270.0, w=1.6, kind="park")
    B.thing("garden_bench_2", "seat", (17.2, 7.2, 0.45), 270.0, height=0.45)
    ct = kit.chess_table(B, 12.9, 9.2, 0.0, 0.0, seed=4)
    kit.stool(B, 12.9 - 0.55, 9.2, 0.0, 270.0)
    kit.stool(B, 12.9 + 0.55, 9.2, 0.0, 90.0)
    B.thing("garden_chess", "play", (12.9, 9.2, 0.72), 0.0, what="chess")
    B.thing("garden_chess_stool_w", "seat", (12.9 - 0.55, 9.2, 0.45), 270.0, height=0.45)
    B.thing("garden_chess_stool_e", "seat", (12.9 + 0.55, 9.2, 0.45), 90.0, height=0.45)
    pp = kit.pingpong_table(B, 15.4, 11.7, 0.0, 0.0)
    B.thing("garden_pingpong", "play", (15.4, 11.7, 0.76), 0.0, what="table_tennis")
    for i, (x, y) in enumerate(((12.1, 5.9), (17.5, 4.3), (17.5, 9.6))):
        box(B.detail, "CT_Concrete", x - 0.4, x + 0.4, y - 0.3, y + 0.3, 0.0, 0.5)
        kit.plant(B, x, y, 0.5, 0.0, kind=("fern", "ficus", "fern")[i], pot="CT_PlasterGreen", h=0.9, seed=i + 10)


def _furnish_flat(B):
    """Flat 0-01 (bay 0): the home of Viktor Sergeevich, a retired ship's navigator. Living room and bedroom at the
    front, kitchen and hall at the back; door at the back wall."""
    # partition: the bedroom nook (x -9.95..-7.7, y 3.64..8.0) with a 1 m doorway on its east side
    m = B.m
    box(m, "CT_Plaster", -9.95, -9.85, YL + 0.3, 8.0, 0.0, ZF[1] - SL)
    box(m, "CT_Plaster", -9.95, -8.7, 7.95, 8.05, 0.0, ZF[1] - SL)
    B.lamp((-11.8, 6.4, 2.85), lumens=480, colour="#FFE0B0", kind="lobby", radius=5.5)
    B.lamp((-8.8, 6.0, 2.85), lumens=380, colour="#FFE0B0", kind="lobby", radius=4.5)
    B.lamp((-9.4, 10.8, 2.85), lumens=480, colour="#FFE0B0", kind="lobby", radius=5.5)
    # living room
    sf = kit.sofa(B, -13.2, 5.6, 0.0, 90.0, seats=3, mat="CT_PlasterGreen")
    B.thing("flat_sofa", "seat", (-13.15, 5.6, 0.45), 90.0, height=0.45)
    kit.picture(B, -13.7, 5.6, 0.0, 90.0, name="painting_sea_storm", w=1.1, zc=1.75)
    B.thing("flat_painting", "board", (-13.65, 5.6, 1.75), 90.0, topic="painting")
    kit.rug(B, -12.0, 5.6, 0.0, 90.0, w=2.2, d=3.0, name="rug_6")
    ct = kit.coffee_table(B, -12.1, 5.6, 0.0, 90.0)
    kit.chess_set(B, -12.1, 5.6, ct["top"], 0.0, board=0.4, seed=2)
    kit.cup(B, -12.1, 5.2, ct["top"], 0.0)
    B.thing("flat_chess", "play", (-12.1, 5.6, ct["top"]), 90.0, what="chess")
    kit.armchair(B, -10.9, 5.6, 0.0, 270.0, mat="CT_PlasterYellow")
    B.thing("flat_armchair", "seat", (-10.9, 5.6, 0.45), 270.0, height=0.45)
    kit.floor_lamp(B, -10.9, 6.9, 0.0, 0.0)
    bs = kit.bookshelf(B, -9.95, 4.45, 0.0, 270.0, w=1.2, h=2.0, topic="sea", seed=11)
    B.thing("flat_books_sea", "shelf", (-10.05, 4.45, 1.3), 270.0, topic="sea")
    kit.aquarium(B, -10.13, 7.4, 0.0, 270.0, w=1.0, seed=3)
    B.thing("flat_aquarium", "screen", (-10.13, 7.4, 1.0), 270.0, what="aquarium")
    sb = kit.table(B, -12.2, 4.0, 0.0, 0.0, w=1.2, d=0.45, h=0.7)
    kit.record_player(B, -12.45, 4.0, sb["top"], 0.0)
    kit.globe(B, -11.85, 4.0, sb["top"], 0.0)
    B.thing("flat_record_player", "machine", (-12.45, 4.0, sb["top"] + 0.1), 0.0, what="record_player")
    kit.wall_map(B, -13.7, 8.8, 0.0, 90.0, name="map_world", w=1.2, zc=1.7)
    # bedroom
    bd = kit.bed(B, -8.3, 4.85, 0.0, 180.0, size="single", blanket="CT_PlasterBlue")
    B.thing("flat_bed", "bed", (-8.3, 4.85, 0.5), 180.0, rest="private", height=0.5)
    B.spot("flat_bed_spot", "bench", (-8.3, 5.9, 0.0), 180)
    kit.bedside_table(B, -9.35, 3.95, 0.0, 180.0, lamp=True)
    kit.wardrobe(B, -9.85, 6.9, 0.0, 90.0, w=1.0)
    kit.mirror(B, -7.7, 6.4, 0.0, 270.0, zc=1.45, w=0.5, h=0.9)
    kit.rug(B, -8.75, 6.6, 0.0, 0.0, w=0.9, d=1.5, name="rug_2")
    # hall by the door (door at x -13.2..-12.3, back wall)
    kit.hat_stand(B, -13.3, 10.3, 0.0, 0.0)
    kit.shoe_rack(B, -13.7, 11.0, 0.0, 90.0, w=0.8)
    kit.mirror(B, -13.7, 12.2, 0.0, 90.0, zc=1.5, w=0.5, h=0.9)
    # kitchen
    kit.kitchen_unit(B, -9.6, 12.74, 0.0, 0.0, modules="cupboard,stove,sink,fridge", wall_cupboards=False)
    for a_ in (-9.6, -9.0):      # wall cupboards only over the sink and fridge: the back window (x -11.4..-9.8) stays clear
        box(B.detail, "CT_TileWhite", a_, a_ + 0.6, 12.41, 12.74, 1.45, 2.15)
        box(B.detail, "CT_TileWhite", a_ + 0.03, a_ + 0.57, 12.398, 12.41, 1.48, 2.12)
        box(B.detail, "CT_ConcreteDark", a_ + 0.25, a_ + 0.35, 12.386, 12.398, 1.5, 1.52)
    kit.samovar(B, -10.55, 12.7, 0.9, 0.0)
    kit.teapot(B, -9.55, 12.65, 0.9, 0.0)
    tb = kit.table(B, -9.4, 10.6, 0.0, 0.0, w=0.9, d=0.7)
    kit.plates(B, -9.4, 10.6, tb["top"], 0.0, n=2)
    kit.vase(B, -9.1, 10.75, tb["top"], 0.0, seed=4)
    kit.around_table(B, -9.4, 10.6, 0.9, 0.7, 0.0, per_side=(1, 1, 0, 0), kind="wood")
    B.thing("flat_kitchen_seat", "seat", (-9.4, 9.95, 0.45), 180.0, height=0.45)
    kit.bookshelf(B, -7.7, 11.6, 0.0, 270.0, w=0.8, h=1.6, topic="cooking", seed=12)
    B.thing("flat_books_cooking", "shelf", (-7.8, 11.6, 1.3), 270.0, topic="cooking")
    kit.picture(B, -7.7, 10.4, 0.0, 270.0, name="painting_still_life", w=0.6, zc=1.7)
    kit.plant(B, -13.2, 9.7, 0.0, 0.0, kind="ficus", pot="CT_PlasterRed", h=1.4, seed=6)


# ====================================================================== second pass: garden, stair, Holm's flat
def _piece(B, x, y, z, yaw, name, kind="prop"):
    p = kit._P(B, x, y, z, yaw, name)
    p.kind = kind
    return p


def _tap(B, x, y, yaw):
    """A standpipe with a brass tap and a zinc bucket under it, against the garden wall."""
    p = _piece(B, x, y, 0.0, yaw, "garden_tap")
    p.cyl("CT_ConcreteDark", 0, 0, 0.0, 0.05, 0.14, 8, bottom=True)
    p.cyl("CT_Steel", 0, 0.0, 0.05, 0.85, 0.035, 6)
    p.box("CT_Brass", -0.03, 0.03, -0.1, 0.03, 0.8, 0.88)
    p.rod("CT_Brass", (0.0, -0.02, 0.84), (0.0, -0.12, 0.84), 0.028, 5)
    p.rod("CT_Brass", (-0.06, 0.0, 0.92), (0.06, 0.0, 0.92), 0.02, 4, caps=True)
    p.cyl("CT_Steel", 0.0, -0.17, 0.0, 0.24, 0.13, 10, r1=0.1, bottom=True)
    p.rod("CT_Steel", (-0.12, -0.17, 0.22), (0.12, -0.17, 0.22), 0.012, 4)
    p.face("CT_Glass", [(0.1 * math.cos(2 * math.pi * k / 10), -0.17 + 0.1 * math.sin(2 * math.pi * k / 10), 0.17) for k in range(10)])
    return p.done()


def _stake(B, x, y, z, yaw, mat="CT_Paper"):
    """A plant label: a thin stake and a small paper tag."""
    p = _piece(B, x, y, z, yaw, "plant_stake")
    p.box("CT_Wood", -0.008, 0.008, -0.004, 0.004, 0.0, 0.34)
    p.box(mat, -0.07, 0.07, -0.012, -0.004, 0.2, 0.31, f="y")
    p.box("CT_ConcreteDark", -0.055, 0.055, -0.014, -0.012, 0.25, 0.262, f="y")
    p.box("CT_ConcreteDark", -0.055, 0.02, -0.014, -0.012, 0.28, 0.288, f="y")
    return p.done()


def _switch_plate(B, x, y, z, yaw):
    """A wall light switch: a pale plate with a black rocker."""
    p = _piece(B, x, y, z, yaw, "light_switch", "wall")
    p.box("CT_ConcreteWhite", -0.045, 0.045, -0.014, 0.0, 1.25, 1.39, f="xXyYzZ")
    p.box("CT_Lacquer", -0.02, 0.02, -0.024, -0.014, 1.3, 1.35, f="xXyYZz")
    return p.done()


def _dominoes(B, x, y, z, yaw):
    p = _piece(B, x, y, z, yaw, "domino_tiles")
    for k, (u, v, a) in enumerate(((-0.12, -0.05, 0.3), (-0.05, 0.02, 1.2), (0.05, -0.02, 0.1), (0.14, 0.05, 2.0), (-0.1, 0.12, 0.8),
                                   (0.03, 0.14, 0.5), (0.18, -0.12, 1.7), (-0.2, -0.16, 0.0))):
        zz = 0.012
        p.box("CT_Paper", u - 0.017, u + 0.017, v - 0.031, v + 0.031, 0.0, zz, f="xXyYZ")
        p.box("CT_Lacquer", u - 0.014, u + 0.014, v - 0.001, v + 0.001, zz, zz + 0.001, f="Z")
    return p.done(top=0.012)


def _sextant_case(B, x, y, z, yaw):
    """An open wooden case with a brass sextant: sector frame, arc scale, index arm, little telescope."""
    p = _piece(B, x, y, z, yaw, "sextant")
    p.box("CT_Wood", -0.17, 0.17, -0.095, 0.095, 0.0, 0.03, f="xXyYZz")
    p.box("CT_Leather", -0.15, 0.15, -0.08, 0.08, 0.03, 0.036, f="Z")
    ap = (0.09, 0.05)
    zz = 0.045
    arc = []
    n = 7
    for k in range(n + 1):
        a = math.radians(205 + 70 * k / n)
        arc.append((ap[0] + 0.16 * math.cos(a), ap[1] + 0.16 * math.sin(a)))
    pts = [ap] + arc
    for k in range(len(pts) - 1):
        p.face("CT_Brass", [(ap[0], ap[1], zz + 0.012), (pts[k + 1][0], pts[k + 1][1], zz + 0.012), (pts[k + 2][0], pts[k + 2][1], zz + 0.012)] if k + 2 < len(pts) else [(ap[0], ap[1], zz + 0.012), (pts[k][0], pts[k][1], zz + 0.012), (pts[k + 1][0], pts[k + 1][1], zz + 0.012)])
    for a_, b_ in zip(arc, arc[1:]):
        p.rod("CT_Steel", (a_[0], a_[1], zz + 0.014), (b_[0], b_[1], zz + 0.014), 0.012, 4, caps=True)
        p.rod("CT_Brass", (a_[0], a_[1], zz), (b_[0], b_[1], zz), 0.016, 4, caps=True)
    p.rod("CT_Brass", (ap[0], ap[1], zz + 0.02), (arc[3][0], arc[3][1], zz + 0.02), 0.012, 4, caps=True)
    p.cyl("CT_Glass", ap[0] - 0.04, ap[1] - 0.05, zz + 0.012, zz + 0.05, 0.012, 6)
    p.rod("CT_Steel", (ap[0] - 0.05, ap[1] - 0.12, zz + 0.04), (ap[0] - 0.02, ap[1] - 0.02, zz + 0.04), 0.022, 6, caps=True)
    p.cyl("CT_Glass", ap[0] + 0.03, ap[1] + 0.03, zz + 0.012, zz + 0.03, 0.014, 6)
    for k in (0, n):
        p.rod("CT_Brass", (arc[k][0], arc[k][1], 0.036), (arc[k][0], arc[k][1], zz), 0.014, 4, caps=True)
    p.rod("CT_Brass", (ap[0], ap[1], 0.036), (ap[0], ap[1], zz), 0.014, 4, caps=True)
    return p.done(top=0.05)


def _compass(B, x, y, z, yaw):
    """A brass hand compass: a bowl, a dark card with four ticks and a red-and-white needle, a hinged lid."""
    p = _piece(B, x, y, z, yaw, "compass")
    p.cyl("CT_Brass", 0, 0, 0.0, 0.035, 0.07, 12, r1=0.065, bottom=True)
    p.cyl("CT_Lacquer", 0, 0, 0.035, 0.038, 0.058, 12)
    for k in range(4):
        a = k * math.pi / 2
        p.rod("CT_Paper", (0.03 * math.cos(a), 0.03 * math.sin(a), 0.04), (0.052 * math.cos(a), 0.052 * math.sin(a), 0.04), 0.006, 3)
    p.rod("CT_PlasterRed", (0.0, 0.0, 0.043), (0.0, 0.05, 0.043), 0.01, 3, caps=True)
    p.rod("CT_Paper", (0.0, 0.0, 0.043), (0.0, -0.05, 0.043), 0.01, 3, caps=True)
    p.cyl("CT_Brass", 0, 0, 0.038, 0.046, 0.006, 6)
    p.rod("CT_Brass", (-0.07, 0.0, 0.0), (-0.065, 0.0, 0.035), 0.012, 3)
    return p.done(top=0.05)


def _postcards(B, x, y, z, yaw):
    """A stack of postcards held with a rubber band, a lighthouse card leaning on it."""
    p = _piece(B, x, y, z, yaw, "postcards")
    for k in range(6):
        p.box("CT_Paper", -0.07 + 0.002 * k, 0.07 + 0.002 * (k % 2), -0.05, 0.05, 0.006 * k, 0.006 * (k + 1), f="xXyYZ")
    p.box("CT_PlasterRed", -0.071, 0.071, -0.008, 0.008, 0.0, 0.037, f="xXyYZ")
    p.card_top("paper_letter", -0.065, 0.065, -0.045, 0.045, 0.037)
    return p.done(top=0.04)


def _barometer(B, x, y, z, yaw):
    """A round aneroid barometer: a wooden ring, a brass bezel, a pale dial with ticks, a red needle, a brass set-hand."""
    p = _piece(B, x, y, z, yaw, "barometer", "wall")
    zc, r = 1.62, 0.15
    ring = lambda rr, yy: [(rr * math.cos(2 * math.pi * k / 12), yy, zc + rr * math.sin(2 * math.pi * k / 12)) for k in range(12)]
    p.loft("CT_Wood", [ring(r + 0.035, 0.0), ring(r + 0.035, -0.025)], top=False)
    p.face("CT_Wood", ring(r + 0.035, -0.025))
    p.loft("CT_Brass", [ring(r, -0.025), ring(r, -0.06), ring(r - 0.014, -0.06)], top=False)
    p.face("CT_Paper", ring(r - 0.014, -0.05)[::-1])
    for k in range(12):
        a = 2 * math.pi * k / 12
        c1, s1 = math.cos(a), math.sin(a)
        p.rod("CT_ConcreteDark", (0.1 * c1, -0.052, zc + 0.1 * s1), (0.125 * c1, -0.052, zc + 0.125 * s1), 0.007, 3)
    p.rod("CT_PlasterRed", (0.0, -0.056, zc), (-0.085, -0.056, zc + 0.05), 0.012, 3, caps=True)
    p.rod("CT_Brass", (0.0, -0.054, zc), (0.06, -0.054, zc - 0.07), 0.008, 3, caps=True)
    p.box("CT_Brass", -0.014, 0.014, -0.066, -0.054, zc - 0.014, zc + 0.014)
    p.box("CT_Brass", -0.045, 0.045, -0.026, -0.008, zc - 0.265, zc - 0.225)
    p.box("CT_PlasterRed", -0.005, 0.005, -0.03, -0.026, zc - 0.26, zc - 0.17)
    return p.done()


def _kettle(B, x, y, z, yaw):
    """An enamel kettle: round body, spout, lid with a knob, a wooden handle over the top."""
    p = _piece(B, x, y, z, yaw, "kettle")
    p.lathe("CT_PlasterBlue", 0, 0, [(0.1, 0.0), (0.125, 0.04), (0.12, 0.12), (0.08, 0.16), (0.05, 0.17)], 10)
    p.cyl("CT_PlasterBlue", 0, 0, 0.17, 0.175, 0.05, 10)
    p.cyl("CT_Wood", 0, 0, 0.175, 0.205, 0.016, 5)
    p.rod("CT_PlasterBlue", (0.1, 0.0, 0.08), (0.2, 0.0, 0.17), 0.026, 5, caps=True)
    p.rod("CT_Wood", (-0.1, 0.0, 0.14), (0.0, 0.0, 0.26), 0.014, 4, caps=True)
    p.rod("CT_Wood", (0.0, 0.0, 0.26), (0.1, 0.0, 0.14), 0.014, 4, caps=True)
    return p.done(top=0.2)


def _navtable_props(B, x, y, z, yaw):
    """On the chart table (local x across, y deep): a coastal chart, a rough log, brass dividers, a parallel rule, a pencil."""
    p = _piece(B, x, y, z, yaw, "chart_table_props")
    p.card_top("map_coast", -0.58, 0.02, -0.33, 0.33, 0.004)
    p.box("CT_Paper", -0.59, 0.03, -0.34, 0.34, 0.0, 0.003, f="xXyY")
    # the rough log: a leather cover, two pages
    p.box("CT_Leather", 0.12, 0.58, -0.3, 0.12, 0.0, 0.02, f="xXyYZz")
    p.box("CT_Paper", 0.14, 0.35, -0.28, 0.1, 0.02, 0.03, f="xXyYZ")
    p.box("CT_Paper", 0.355, 0.56, -0.28, 0.1, 0.02, 0.03, f="xXyYZ")
    p.card_top("paper_typed", 0.15, 0.34, -0.27, 0.09, 0.0305)
    p.card_top("paper_letter", 0.365, 0.55, -0.27, 0.09, 0.0305)
    # brass dividers on the chart, hinge up
    p.rod("CT_Brass", (-0.3, 0.1, 0.05), (-0.36, -0.1, 0.006), 0.012, 4, caps=True)
    p.rod("CT_Brass", (-0.3, 0.1, 0.05), (-0.24, -0.1, 0.006), 0.012, 4, caps=True)
    p.cyl("CT_Brass", -0.3, 0.1, 0.045, 0.06, 0.018, 6)
    # parallel rule
    p.rod("CT_Wood", (-0.5, -0.2, 0.01), (-0.1, 0.18, 0.01), 0.026, 4, caps=True)
    p.rod("CT_Wood", (-0.49, -0.12, 0.01), (-0.09, 0.26, 0.01), 0.026, 4, caps=True)
    # pencil
    p.rod("CT_PlasterYellow", (0.2, 0.2, 0.012), (0.42, 0.28, 0.012), 0.01, 4, caps=True)
    return p.done(top=0.05)


def _shadow_lattice(B, x0, x1, y0, step=0.34, rows=4, z=0.012):
    """The cobogo's diamond shadow as pale patches of sun on the living-room floor, rows running away from the window."""
    for r in range(rows):
        for k in range(int((x1 - x0) / step)):
            cx = x0 + step * (k + 0.5 + 0.5 * (r % 2))
            cy = y0 + r * 0.30
            if cx + 0.11 > x1:
                continue
            h = 0.105 - 0.015 * r
            kit_m = B.detail
            for_pts = [(cx - h, cy), (cx, cy - h * 0.9), (cx + h, cy), (cx, cy + h * 0.9)]
            pr(kit_m, "CT_PlasterYellow", for_pts, z, z + 0.002, top=True, bottom=False)


def _depth_pass(B):
    det = B.detail
    T = B.thing
    # ------------------------------------------------------------ the garden under the pilotis
    kit.notice_board(B, 11.7, 7.4, 0.0, 90.0, w=1.2, zc=1.6, seed=21)
    T("garden_notices", "board", (11.76, 7.4, 1.6), 90.0, topic="local")
    fu.mailboxes(B, fu.Fr(11.75, 11.0, 90, 0.0), 0.0, 0.95, cols=3, rows=2)
    T("garden_post", "board", (11.82, 11.0, 1.3), 90.0, topic="local")
    _tap(B, 12.0, 12.0, 90.0)
    T("garden_tap", "machine", (12.04, 12.0, 0.85), 90.0, what="tap")
    B.spot("garden_tap_stand", "stand", (12.9, 12.0, 0.0), 270)
    _bike(B, 17.78, 5.7, 90.0, "CT_PlasterBlue")
    T("garden_bike", "board", (17.78, 5.7, 0.6), 270.0, topic="local")
    for i, (x, y, tag) in enumerate(((12.1, 5.9, "CT_Paper"), (17.5, 4.3, "CT_PlasterYellow"), (17.5, 9.6, "CT_Paper"))):
        _stake(B, x + 0.2, y - 0.12, 0.5, 0.0, tag)
        T("garden_pot_%d" % (i + 1), "board", (x, y - 0.1, 0.95), 0.0, topic="garden")
    T("garden_tree", "board", (15.0, 8.8, 1.2), 0.0, topic="garden")
    _stake(B, 14.45, 8.4, 0.0, 0.0, "CT_PlasterYellow")
    gt = kit.table(B, 14.0, 6.9, 0.0, 0.0, w=0.9, d=0.7)
    _dominoes(B, 14.0, 6.9, gt["top"], 20.0)
    kit.stool(B, 14.0, 6.25, 0.0, 180.0)
    kit.stool(B, 14.0, 7.55, 0.0, 0.0)
    T("garden_games", "play", (14.0, 6.9, gt["top"]), 0.0, what="dominoes")
    T("garden_games_stool_s", "seat", (14.0, 6.25, 0.45), 180.0, height=0.45)
    T("garden_games_stool_n", "seat", (14.0, 7.55, 0.45), 0.0, height=0.45)
    _switch_plate(B, 11.7, 4.6, 0.0, 90.0)
    T("garden_switch", "machine", (11.76, 4.6, 1.3), 90.0, what="switch", switch="room")
    B.lamp((13.0, 10.9, ZF[1] - 0.5), lumens=520, colour="#FFD9A8", kind="lobby", radius=6.0)
    B.lamp((16.6, 7.6, ZF[1] - 0.5), lumens=480, colour="#FFD9A8", kind="lobby", radius=6.0)
    B.spot("garden_notice_stand", "stand", (12.5, 7.4, 0.0), 90)
    B.spot("garden_games_stand", "stand", (15.0, 6.9, 0.0), 270)
    B.spot("staff_concierge", "work", (4.9, 11.1, 0.0), 270)
    # ------------------------------------------------------------ the stair: gallery landings
    kit.wall_clock(B, -13.0, YB, ZF[1], 180.0, zc=2.5, face="clock_2")
    T("stair_view_1", "board", (-11.9, 14.78, ZF[1] + 1.3), 0.0, topic="view")
    T("stair_view_3", "board", (-11.9, 14.78, ZF[3] + 1.3), 0.0, topic="view")
    B.spot("stair_view_spot_1", "view", (-11.9, 14.1, ZF[1]), 0)
    B.spot("stair_view_spot_3", "view", (-11.9, 14.1, ZF[3]), 0)
    # ------------------------------------------------------------ Holm's flat 0-01
    _shadow_lattice(B, -13.2, -10.4, 4.35)
    T("flat_shadow", "board", (-11.2, 3.7, 1.5), 0.0, topic="view")
    ct = kit.table(B, -11.9, 8.65, 0.0, 0.0, w=1.3, d=0.8, h=0.75)
    _navtable_props(B, -11.9, 8.65, ct["top"], 0.0)
    T("flat_charts", "board", (-12.3, 8.65, ct["top"] + 0.02), 0.0, topic="travel")
    T("flat_log_open", "board", (-11.45, 8.65, ct["top"] + 0.05), 0.0, topic="logbook")
    B.spot("flat_chart_spot", "stand", (-11.9, 7.8, 0.0), 180)
    kit.wall_shelf(B, -13.7, 7.2, 0.0, 90.0, w=1.04, zt=1.15, props=False)
    _sextant_case(B, -13.58, 6.88, 1.15, 90.0)
    T("flat_sextant", "board", (-13.55, 6.88, 1.2), 90.0, topic="travel")
    _compass(B, -13.55, 7.22, 1.15, 90.0)
    T("flat_compass", "board", (-13.55, 7.22, 1.22), 90.0, topic="travel")
    _postcards(B, -13.55, 7.58, 1.15, 90.0)
    T("flat_postcards", "board", (-13.55, 7.58, 1.22), 90.0, topic="local")
    _barometer(B, -9.85, 4.4, 0.0, 90.0)
    T("flat_barometer", "board", (-9.8, 4.4, 1.62), 90.0, topic="local")
    _kettle(B, -10.0, 12.7, 0.9, 0.0)
    T("flat_kettle", "machine", (-10.0, 12.7, 1.0), 0.0, what="kettle")
    kit.wall_shelf(B, -7.7, 10.95, 0.0, 270.0, w=0.5, zt=1.2, props=False)
    kit.radio(B, -7.92, 10.95, 1.2, 270.0)
    T("flat_radio", "machine", (-7.95, 10.95, 1.32), 270.0, what="radio_dead")
    _switch_plate(B, -12.0, 13.04, 0.0, 0.0)
    T("flat_switch", "machine", (-12.0, 12.98, 1.3), 0.0, what="switch", switch="room")
    B.spot("flat_kettle_spot", "stand", (-10.0, 11.9, 0.0), 180)
