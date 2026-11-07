"""Curved slab on V-pilotis: a gently curved four-storey residential slab lifted on V-shaped concrete legs, in the spirit
of the 1950s Rio modernists (Reidy's curved housing blocks, Niemeyer's pilotis, Rio brise-soleil, cobogo stair
towers, azulejo panels). Our own massing and details, not a copy of any building.

Plot 60 x 20 m, max 18 m. The plan is an arc concave toward the street: the front line is a circle of radius R = 108 m
about a centre on the street side, so the slab ends come towards the street and the middle sits back (a forecourt).
  * 6 structural bays of 8.6 m along the arc. Five bays carry flats (2 facets each), bay 3 is the stair core.
  * Ground floor (3.4 m): V-shaped legs along the front, round columns at the back; free and open, with a glazed
    lobby under the core (blue azulejo walls and piers with a white-tile pattern, double street door, rear door),
    two glazed rooms (bays 1 and 5: common room and caretaker, public windows) and an open forecourt with benches.
  * 4 storeys of 2.8 m, 5 gallery-access flats a storey = 20 flats ("1-01".."4-05"). Front: deep concrete brise-soleil
    (a vertical fin every 2.15 m, plus a horizontal egg-crate of blades on the sun side, bays 4-5, east); coloured
    spandrels behind. Back: a gallery (access balcony) at every storey with a solid parapet and the flat doors.
  * The core is a hollow stair tower: tiled walls behind a cobogo screen (real holes as geometry, 0.6 m blocks)
    standing 0.36 m in front of the stair windows, a U stair of 24 steps a storey (rise <= 0.142) up to the roof,
    a stair head with cobogo walls over the roof.
  * Roof terrace under a curved concrete canopy (a thin double-curved shell on slender columns) over bays 4-5,
    timber deck, parapet all round.
Entrances: "front" (lobby, double leaves, street), "rear" (lobby, gallery side), "room1_back", "room5_back" (service).
Material requests: none (CT_Tile, CT_TileWhite, CT_Cobogo, CT_Louvre cover it).
"""
import math
import random

import furn_int3 as fu
import geom
from citykit import box, prism, DOOR_W, DOOR_H
from geom import Facade
from _cons_util import wall, win, hface
from _sheet import thick_sheet
from _surrounds import add_surrounds
from _cobogo_house_blocks import cobogo_screen

INFO = {
    "name_ru": "Изогнутый дом на V-опорах", "name_en": "Curved Slab on V-Pilotis",
    "style": "brazilian modernism", "use": "residential",
    "inspired_by": "1950s Rio modernism: Reidy's curved housing blocks, Niemeyer's V pilotis, brise-soleil, "
                   "cobogo stair tower, azulejo lobby panel; massing and details are our own",
}

R = 108.0                # front radius (centre on the street side)
YMID = 5.4               # y of the front wall at x = 0
BAY = 8.6                # arc length of a bay
NBAY = 6
CORE = 3                 # bay of the stair core
BODY = 8.6               # depth of the flats
GAL = 1.6                # gallery depth
PIL = 3.4                # ground floor height (floor 1 level)
ST = 2.8                 # storey height
NST = 4
WALL = 0.3
SL = 0.22                # slab thickness
FIN_D = 0.85
SUN = (4, 5)             # bays with the egg-crate
YC = YMID - R
RB = R + BODY
RG = R + BODY + GAL
ZS = [PIL + ST * j for j in range(NST + 1)]      # floor levels 0..3 and the roof (ZS[4])
ZR = ZS[-1]
STEPS = 12               # steps per flight (two flights a storey)


def P(r, th):
    return (r * math.sin(th), YC + r * math.cos(th))


def TH(k):
    return (k - NBAY / 2.0) * BAY / R


def yaw_of(n):
    return math.degrees(math.atan2(n[0], -n[1]))


def hexa8(m, mat, c, faces):
    F = {"x": (0, 2, 6, 4), "X": (1, 3, 7, 5), "y": (0, 1, 5, 4), "Y": (2, 3, 7, 6), "z": (0, 1, 3, 2), "Z": (4, 5, 7, 6)}
    cx = [sum(p[i] for p in c) / 8.0 for i in range(3)]
    for k in faces:
        q = [c[i] for i in F[k]]
        n = geom.cross(geom.sub(q[1], q[0]), geom.sub(q[2], q[0]))
        fc = [sum(p[i] for p in q) / 4.0 for i in range(3)]
        if sum(n[i] * (fc[i] - cx[i]) for i in range(3)) < 0:
            q = q[::-1]
        m.face(mat, q)


def lbox(m, mat, F, u0, u1, y0, y1, z0, z1, faces="xXyYzZ"):
    c = [F.to_building((u1 if i & 1 else u0, y1 if i & 2 else y0, z1 if i & 4 else z0)) for i in range(8)]
    hexa8(m, mat, c, faces)


def pr(m, mat, pts, z0, z1, top=False, bottom=False, side_mat=None):
    """prism over a 2D polygon of either winding (made CCW)."""
    a = sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1] for i in range(len(pts)))
    if a < 0:
        pts = pts[::-1]
    prism(m, mat, pts, z0, z1, top=top, bottom=bottom, side_mat=side_mat)


def wall_line(m, mat, A, B, off, z0, z1, side_mat=None):
    """A thin wall on the line A-B, thickened by the 2D vector off."""
    pr(m, mat, [A, B, (B[0] + off[0], B[1] + off[1]), (A[0] + off[0], A[1] + off[1])], z0, z1, side_mat=side_mat)


def tang(th):
    return (math.cos(th), -math.sin(th))


def radial(th):
    return (math.sin(th), math.cos(th))


def tile_pattern(det, F, u0, u1, z0, z1, cell=0.4):
    """White tiles in a checker on a blue tiled wall (3 cm proud, one quad each)."""
    nu, nz = int((u1 - u0) / cell), int((z1 - z0) / cell)
    for i in range(nu):
        for j in range(nz):
            if (i + j) % 2 == 0:
                lbox(det, "CT_TileWhite", F, u0 + i * cell + 0.04, u0 + (i + 1) * cell - 0.04, 0.0, 0.03,
                     z0 + j * cell + 0.04, z0 + (j + 1) * cell - 0.04, faces="Y")


def build(B):
    rnd = random.Random(1956)
    m, det = B.m, B.detail
    th = [TH(k) for k in range(NBAY + 1)]
    segs = []                                    # (bay, k, a, b)
    for b in range(NBAY):
        if b == CORE:
            segs.append((b, 0, th[b], th[b + 1]))
        else:
            mid = (th[b] + th[b + 1]) / 2
            segs += [(b, 0, th[b], mid), (b, 1, mid, th[b + 1])]

    def flat_id(b, j):
        return "%d-%02d" % (j + 1, b + 1 if b < CORE else b)

    # ---------------------------------------------------------------- the core's local frame
    f0, f1 = P(R, th[CORE]), P(R, th[CORE + 1])
    Fc = Facade(f0, f1)
    LC = Fc.length
    bl, br = P(RB, th[CORE]), P(RB, th[CORE + 1])

    def loc(p):
        d = (p[0] - f0[0], p[1] - f0[1])
        return (d[0] * Fc.d[0] + d[1] * Fc.d[1], -(d[0] * Fc.n[0] + d[1] * Fc.n[1]))
    ubl, VB = loc(bl)
    ubr, _ = loc(br)

    def uleft(v):
        return ubl * v / VB

    def uright(v):
        return LC + (ubr - LC) * v / VB

    def W(u, v, z=0.0):
        return Fc.to_building((u, -v, z))

    def W2(u, v):
        return W(u, v)[:2]

    # ---------------------------------------------------------------- slabs
    WELL = (0.8, 4.0, 2.1, 7.1)                 # u0, u1, v0, v1 of the stair well in the core
    for j in range(NST + 1):
        z = ZS[j]
        top_mat = "CT_Roof" if j == NST else "CT_Floor"
        for (b, k, a, c) in segs:
            if b == CORE:
                pieces = [
                    [(uleft(WELL[3]), WELL[3]), (uright(WELL[3]), WELL[3]), (ubr, VB), (ubl, VB)],
                    [(0, 0), (LC, 0), (uright(WELL[2]), WELL[2]), (uleft(WELL[2]), WELL[2])],
                    [(uleft(WELL[2]), WELL[2]), (WELL[0], WELL[2]), (WELL[0], WELL[3]), (uleft(WELL[3]), WELL[3])],
                    [(WELL[1], WELL[2]), (uright(WELL[2]), WELL[2]), (uright(WELL[3]), WELL[3]), (WELL[1], WELL[3])],
                ]
                for pc in pieces:
                    pts = [W2(*q) for q in pc]
                    pr(m, "CT_ConcreteDark", pts, z - SL, z, top=False, bottom=True, side_mat="CT_ConcreteWhite")
                    hface(m, top_mat if j == NST else "CT_Floor", pts, z, True)
                gp = [P(RB, a), P(RB, c), P(RG, c), P(RG, a)]
                pr(m, "CT_ConcreteDark", gp, z - SL, z, top=False, bottom=True, side_mat="CT_ConcreteWhite")
                hface(m, "CT_Roof" if j == NST else "CT_Paving", gp, z, True)
            else:
                bp = [P(R, a), P(R, c), P(RB, c), P(RB, a)]
                gp = [P(RB, a), P(RB, c), P(RG, c), P(RG, a)]
                pr(m, "CT_ConcreteDark", [P(R, a), P(R, c), P(RG, c), P(RG, a)], z - SL, z, top=False, bottom=True,
                   side_mat="CT_ConcreteWhite")
                hface(m, top_mat, bp, z, True)
                hface(m, "CT_Roof" if j == NST else "CT_Paving", gp, z, True)

    # ---------------------------------------------------------------- facades of the storeys
    for (b, k, a, c) in segs:
        p0, p1 = P(R, a), P(R, c)
        q0, q1 = P(RB, c), P(RB, a)             # back wall runs from the right end to the left end
        L = math.dist(p0, p1)
        Lb = math.dist(q0, q1)
        Fb = Facade(q0, q1)
        for j in range(NST):
            z0, z1 = ZS[j], ZS[j + 1] - SL
            if b != CORE:
                fid = flat_id(b, j)
                mat = ("CT_ConcreteWhite", "CT_PlasterBlue", "CT_ConcreteWhite", "CT_PlasterYellow", "CT_ConcreteWhite",
                       "CT_PlasterPink")[(b * 3 + j * 2 + k) % 6] if (b + j) % 2 else "CT_ConcreteWhite"
                hole = (0.6, L - 0.6, z0 + 0.8, z0 + 2.05)
                wall(B, p0, p1, z0, z1, mat, holes=[hole], depth=WALL, mat_in="CT_Interior")
                win(B, p0, p1, hole[0], hole[1], hole[2], hole[3], flat=fid, cols=3, rows=1, inset=0.2)
                if k == 0:
                    du0 = Lb - 1.9
                    holes = [(du0, du0 + DOOR_W, z0, z0 + DOOR_H)]
                    wall(B, q0, q1, z0, z1, "CT_ConcreteWhite", holes=holes, depth=WALL, mat_in="CT_Interior")
                    lbox(det, "CT_Door", Fb, du0, du0 + DOOR_W, -0.12, -0.07, z0, z0 + DOOR_H, faces="Y")
                    B.flat(fid, storey=j + 1, beds=1 + (b + j) % 3,
                           door=Fb.to_building((du0 + DOOR_W / 2, 0.6, z0)), entrance="front")
                else:
                    holes = [(1.4, 2.7, z0 + 1.0, z0 + 2.0)]
                    wall(B, q0, q1, z0, z1, "CT_ConcreteWhite", holes=holes, depth=WALL, mat_in="CT_Interior")
                    win(B, q0, q1, 1.4, 2.7, z0 + 1.0, z0 + 2.0, flat=fid, cols=2, rows=1, inset=0.2)
            else:
                holes = [(1.2, 3.6, z0 + 0.5, z0 + 2.2), (5.0, L - 1.2, z0 + 0.5, z0 + 2.2)]
                wall(B, p0, p1, z0, z1, "CT_Tile", holes=holes, depth=WALL, mat_in="CT_Interior")
                for h in holes:
                    win(B, p0, p1, h[0], h[1], h[2], h[3], kind="stair", cols=1, rows=2, inset=0.2)
                mid = Lb / 2
                holes = [(mid - DOOR_W / 2, mid + DOOR_W / 2, z0, z0 + DOOR_H), (0.7, 1.7, z0 + 0.7, z0 + 2.0),
                         (Lb - 1.7, Lb - 0.7, z0 + 0.7, z0 + 2.0)]
                wall(B, q0, q1, z0, z1, "CT_Tile", holes=holes, depth=WALL, mat_in="CT_Interior")
                for h in holes[1:]:
                    win(B, q0, q1, h[0], h[1], h[2], h[3], kind="stair", cols=1, rows=2, inset=0.2)
                lbox(det, "CT_Door", Fb, mid - DOOR_W / 2, mid + DOOR_W / 2, -0.12, -0.07, z0, z0 + DOOR_H, faces="Y")
                # a white band at the floor line on the gallery side
                lbox(det, "CT_TileWhite", Fb, 0.0, Lb, 0.0, 0.03, z0 + 2.1, z0 + 2.45, faces="Y")

    # ---------------------------------------------------------------- party walls and end walls (storeys 1-4)
    for kk in range(NBAY + 1):
        t = th[kk]
        A, Bk = P(R + WALL, t), P(RB - WALL, t)
        tg = tang(t)
        if kk in (0, NBAY):
            sgn = 1 if kk == 0 else -1
            # end wall over the whole depth including the gallery, from the soffit to the parapet top
            A, Bk = P(R, t), P(RG, t)
            wall_line(m, "CT_ConcreteWhite", A, Bk, (tg[0] * 0.3 * sgn, tg[1] * 0.3 * sgn), PIL - SL, ZR + 0.9)
            # azulejo mural panels on the end wall's outside
            F = Facade(P(RG, t), P(R, t)) if kk == 0 else Facade(P(R, t), P(RG, t))
            Lw = F.length
            for q in range(4):
                h0 = PIL + 1.0 + q * 3.1
                lbox(det, "CT_Tile" if q % 2 == 0 else "CT_TileWhite", F, Lw * 0.22, Lw * 0.78, 0.0, 0.03, h0, h0 + 2.2, faces="Y")
        else:
            off = (tg[0] * 0.2, tg[1] * 0.2)
            wall_line(m, "CT_ConcreteWhite", A, Bk, off, PIL, ZR - SL)

    # ---------------------------------------------------------------- gallery parapets, roof parapets
    for (b, k, a, c) in segs:
        for j in range(NST):
            z = ZS[j]
            ga, gb = P(RG, c), P(RG, a)         # outside right when walking from the right end to the left
            B.facade(ga, gb, z, z + 1.0, "CT_ConcreteWhite", depth=0.14, back=True)
            tl = (-(gb[1] - ga[1]), gb[0] - ga[0])
            Ln = math.hypot(*tl)
            tl = (tl[0] / Ln * 0.14, tl[1] / Ln * 0.14)
            hface(m, "CT_ConcreteWhite", [ga, gb, (gb[0] + tl[0], gb[1] + tl[1]), (ga[0] + tl[0], ga[1] + tl[1])], z + 1.0, True)
        # roof parapet, street and gallery edges
        for (pa, pb) in ((P(R, a), P(R, c)), (P(RG, c), P(RG, a))):
            B.facade(pa, pb, ZR, ZR + 0.9, "CT_ConcreteWhite", depth=0.25, back=True)
            tl = (-(pb[1] - pa[1]), pb[0] - pa[0])
            Ln = math.hypot(*tl)
            tl = (tl[0] / Ln * 0.25, tl[1] / Ln * 0.25)
            hface(m, "CT_ConcreteWhite", [pa, pb, (pb[0] + tl[0], pb[1] + tl[1]), (pa[0] + tl[0], pa[1] + tl[1])], ZR + 0.9, True)

    # ---------------------------------------------------------------- brise-soleil
    fin_z0, fin_z1 = PIL - SL, ZR + 1.1
    for si, (b, k, a, c) in enumerate(segs):
        if b == CORE:
            continue
        p0, p1 = P(R, a), P(R, c)
        L = math.dist(p0, p1)
        F = Facade(p0, p1)
        pos = [0.0, L / 2]
        if si == len(segs) - 1 or segs[si + 1][0] == CORE:
            pos.append(L)
        for u in pos:
            uu = min(max(u, 0.06), L - 0.06)
            lbox(det, "CT_Louvre", F, uu - 0.06, uu + 0.06, 0.0, FIN_D, fin_z0, fin_z1, faces="xXYZ")
        if b in SUN:
            for j in range(NST + 1):
                for dz in ((-0.2, -0.06), (1.3, 1.44)):
                    zb = ZS[j] + dz[0]
                    if j == NST and dz[0] > 0:
                        continue
                    lbox(det, "CT_Louvre", F, 0.06, L - 0.06, 0.0, FIN_D, zb, ZS[j] + dz[1], faces="YzZ")

    # ---------------------------------------------------------------- core: cobogo screen, posts
    nblk = cobogo_screen(det, Fc, 0.4, LC - 0.4, PIL, ZR, y_front=0.5, thick=0.14, block=0.6)
    for (u0, u1) in ((0.15, 0.37), (LC - 0.37, LC - 0.15)):
        lbox(det, "CT_ConcreteWhite", Fc, u0, u1, 0.0, 0.52, PIL - SL, ZR + 0.9, faces="xXYZ")

    # stairs inside the core: two flights of 12 steps a storey with a half landing
    for j in range(NST + 1):
        za = 0.0 if j == 0 else ZS[j - 1]
        zb = ZS[j]
        # storeys 0..4: from the level below to this floor
        rise = (zb - za) / (2 * STEPS)
        solid = (j == 0)
        for i in range(STEPS):                                  # flight 1 along -v
            v1, v0 = 7.1 - i * 0.3, 7.1 - (i + 1) * 0.3
            ztop = za + rise * (i + 1)
            lbox(m, "CT_ConcreteWhite", Fc, 0.9, 2.2, -v1, -v0, 0.0 if solid else ztop - 0.2, ztop, faces="xXyYZ" + ("" if solid else "z"))
        zl = za + rise * STEPS
        lbox(m, "CT_ConcreteWhite", Fc, 0.9, 3.9, -3.5, -2.2, 0.0 if solid else zl - 0.2, zl, faces="xXyYZ" + ("" if solid else "z"))
        for i in range(STEPS):                                  # flight 2 along +v
            v0, v1 = 3.5 + i * 0.3, 3.5 + (i + 1) * 0.3
            ztop = za + rise * (STEPS + i + 1)
            lbox(m, "CT_ConcreteWhite", Fc, 2.6, 3.9, -v1, -v0, 0.0 if solid else ztop - 0.2, ztop, faces="xXyYZ" + ("" if solid else "z"))
        # stair lamp on the half landing
        B.lamp(W(2.4, 2.9, zl + 2.3), lumens=420, colour="#FFE0B0", kind="stair", radius=5.0)

    # ---------------------------------------------------------------- stair head on the roof
    hv0, hv1, hu0, hu1 = 0.5, 8.3, 0.4, LC - 0.4
    HH = 1.9
    ZH = ZR + HH
    hfl, hfr, hbr, hbl = W2(hu0, hv0), W2(hu1, hv0), W2(hu1, hv1), W2(hu0, hv1)
    FH = Facade(hfl, hfr)
    cobogo_screen(det, FH, 0.0, FH.length, ZR + 0.45, ZH, y_front=0.0, thick=0.14, block=0.6)
    wall_line(m, "CT_TileWhite", hfl, hfr, (-FH.n[0] * 0.2, -FH.n[1] * 0.2), ZR, ZR + 0.45)
    FBk = Facade(hbr, hbl)
    cobogo_screen(det, FBk, 0.0, FBk.length, ZR + 0.45, ZH, y_front=0.0, thick=0.14, block=0.6)
    wall_line(m, "CT_TileWhite", hbr, hbl, (-FBk.n[0] * 0.2, -FBk.n[1] * 0.2), ZR, ZR + 0.45)
    wall(B, hbl, hfl, ZR, ZH, "CT_Tile", depth=0.2, mat_in="CT_TileWhite")
    wall(B, hfr, hbr, ZR, ZH, "CT_Tile", holes=[(0.9, 0.9 + DOOR_W, 0.0, DOOR_H)], depth=0.2, mat_in="CT_TileWhite")
    cap = [W2(hu0 - 0.3, hv0 - 0.3), W2(hu1 + 0.3, hv0 - 0.3), W2(hu1 + 0.3, hv1 + 0.3), W2(hu0 - 0.3, hv1 + 0.3)]
    pr(m, "CT_ConcreteWhite", cap, ZH, ZH + 0.18, top=True, bottom=True)
    B.lamp(W(3.0, 5.0, ZR + 1.6), lumens=300, colour="#FFE0B0", kind="stair", radius=4.5)

    # ---------------------------------------------------------------- ground floor: paving, lobby, rooms
    hface(m, "CT_Paving", [P(R, th[0]), P(R, th[NBAY])] + [P(RG, th[NBAY]), P(RG, th[0])], 0.0, True)
    # lobby (core bay)
    mid = LC / 2
    holes = [(mid - 1.1, mid + 1.1, 0.0, DOOR_H), (1.6, 3.0, 0.3, 2.7), (5.6, 7.0, 0.3, 2.7)]
    wall(B, f0, f1, 0.0, PIL - SL, "CT_Tile", holes=holes, depth=WALL, mat_in="CT_Tile")
    for h in holes[1:]:
        win(B, f0, f1, h[0], h[1], h[2], h[3], kind="public", cols=2, rows=1, inset=0.2)
    tile_pattern(det, Fc, 0.35, 1.45, 0.4, 2.8)
    tile_pattern(det, Fc, LC - 1.45, LC - 0.35, 0.4, 2.8)
    tile_pattern(det, Fc, 1.6, 3.0, 2.9, PIL - SL - 0.05, cell=0.35)
    tile_pattern(det, Fc, 5.6, 7.0, 2.9, PIL - SL - 0.05, cell=0.35)
    B.entrance("front", Fc.to_building((mid, 0.0, 0.0)), yaw_of(Fc.n), 2.2, DOOR_H, "street", leaves=2)
    lbox(det, "CT_ConcreteWhite", Fc, mid - 1.7, mid + 1.7, 0.0, 1.5, 2.72, 2.88, faces="YzZxX")
    FbC = Facade(br, bl)
    Lb = FbC.length
    holes = [(Lb / 2 - DOOR_W / 2, Lb / 2 + DOOR_W / 2, 0.0, DOOR_H), (1.0, 2.6, 0.3, 2.7), (Lb - 2.6, Lb - 1.0, 0.3, 2.7)]
    wall(B, br, bl, 0.0, PIL - SL, "CT_Tile", holes=holes, depth=WALL, mat_in="CT_Tile")
    for h in holes[1:]:
        win(B, br, bl, h[0], h[1], h[2], h[3], kind="public", cols=2, rows=1, inset=0.2)
    B.entrance("rear", FbC.to_building((Lb / 2, 0.0, 0.0)), yaw_of(FbC.n), DOOR_W, DOOR_H, "street")
    for kk in (CORE, CORE + 1):                                  # lobby side walls, tiled both sides
        t = th[kk]
        tg = tang(t)
        sgn = 1 if kk == CORE else -1
        wall_line(m, "CT_Tile", P(R + WALL, t), P(RB - WALL, t), (tg[0] * 0.2 * sgn, tg[1] * 0.2 * sgn), 0.0, PIL - SL)
    hface(m, "CT_Floor", [W2(0, 0), W2(LC, 0), W2(ubr, VB), W2(ubl, VB)], 0.01, True)
    # lobby furniture: letter boxes on the west wall, mat, bench, plant, lamps
    fu.mailboxes(B, fu.Fr(W(0.5, 5.2)[0], W(0.5, 5.2)[1], 90, 0.01), 0.0, 0.95, cols=5, rows=3)
    fu.doormat(B, fu.Fr(W(mid, 0.9)[0], W(mid, 0.9)[1], 0, 0.01), 0.0, 0.0)
    fu.plant(B, W(LC - 0.9, 5.4)[0], W(LC - 0.9, 5.4)[1], 0.01, 0.9, "CT_PlasterBlue")
    bx, by, _ = W(LC - 1.2, 3.0)
    box(det, "CT_Wood", bx - 0.9, bx + 0.9, by - 0.25, by + 0.25, 0.42, 0.48)
    box(det, "CT_Concrete", bx - 0.8, bx - 0.65, by - 0.2, by + 0.2, 0.0, 0.42)
    box(det, "CT_Concrete", bx + 0.65, bx + 0.8, by - 0.2, by + 0.2, 0.0, 0.42)
    B.spot("lobby_bench", "bench", (bx, by, 0.0), yaw_of(Fc.n))
    B.spot("lobby_view", "view", W(mid, 1.8), yaw_of(Fc.n))
    for (u, v) in ((mid, 1.6), (mid, 5.5)):
        B.lamp(W(u, v, PIL - 0.55), lumens=520, colour="#FFE0B0", kind="lobby", radius=6.0)

    # two glazed rooms
    for rb_, name in ((1, "common"), (5, "caretaker")):
        a0, a1 = th[rb_], th[rb_ + 1]
        mida = (a0 + a1) / 2
        for (aa, cc, kk2) in ((a0, mida, 0), (mida, a1, 1)):
            p0, p1 = P(R, aa), P(R, cc)
            L = math.dist(p0, p1)
            hole = (0.6, L - 0.6, 0.35, 2.75)
            wall(B, p0, p1, 0.0, PIL - SL, "CT_Tile" if rb_ == 5 else "CT_PlasterBlue", holes=[hole], depth=WALL, mat_in="CT_Interior")
            win(B, p0, p1, hole[0], hole[1], hole[2], hole[3], kind="public", cols=3, rows=1, inset=0.2)
            q0, q1 = P(RB, cc), P(RB, aa)
            Lq = math.dist(q0, q1)
            if kk2 == 0:
                holes = [(Lq - 1.9, Lq - 1.9 + DOOR_W, 0.0, DOOR_H)]
            else:
                holes = [(1.0, 2.8, 0.7, 2.4)]
            wall(B, q0, q1, 0.0, PIL - SL, "CT_ConcreteWhite", holes=holes, depth=WALL, mat_in="CT_Interior")
            if kk2 == 0:
                Fq = Facade(q0, q1)
                B.entrance("room%d_back" % rb_, Fq.to_building((Lq - 1.9 + DOOR_W / 2, 0.0, 0.0)), yaw_of(Fq.n), DOOR_W, DOOR_H, "service")
            else:
                win(B, q0, q1, 1.0, 2.8, 0.7, 2.4, kind="public", cols=2, rows=1, inset=0.2)
            hface(m, "CT_Floor", [P(R, aa), P(R, cc), P(RB, cc), P(RB, aa)], 0.01, True)
        for kk in (rb_, rb_ + 1):
            t = th[kk]
            tg = tang(t)
            sgn = 1 if kk == rb_ else -1
            wall_line(m, "CT_ConcreteWhite", P(R + WALL, t), P(RB - WALL, t), (tg[0] * 0.2 * sgn, tg[1] * 0.2 * sgn), 0.0, PIL - SL)
        if rb_ == 5:
            t = th[NBAY]
            tg = tang(t)
            wall_line(m, "CT_ConcreteWhite", P(R, t), P(RB, t), (-tg[0] * 0.3, -tg[1] * 0.3), 0.0, PIL - SL)
        cx, cy = P((R + RB) / 2, mida)
        for dd in (-2.2, 2.2):
            tg = tang(mida)
            B.lamp((cx + tg[0] * dd, cy + tg[1] * dd, PIL - 0.5), lumens=600, colour="#FFE8C0", kind="public", radius=6.5)
        # a table and chairs in the common room
        if rb_ == 1:
            fu.table_round(B, cx, cy, 0.6)
            B.spot("common_table", "table", (cx, cy, 0.0), 0)
        else:
            B.spot("caretaker_desk", "work", (cx, cy, 0.0), 0)
            box(m, "CT_WoodLight", cx - 0.8, cx + 0.8, cy - 0.3, cy + 0.3, 0.0, 0.76)

    # ---------------------------------------------------------------- V pilotis, back columns
    for kk in range(NBAY + 1):
        t = th[kk]
        tg, rd = tang(t), radial(t)

        def pt(rr, tt, z):
            x, y = P(R + rr, t)
            return (x + tg[0] * tt, y + tg[1] * tt, z)
        for sg in (-1, 1):
            foot = [pt(0.95 + dr, tt, 0.0) for dr, tt in ((-0.26, -0.14), (-0.26, 0.14), (0.26, 0.14), (0.26, -0.14))]
            topq = [pt(0.95 + dr, sg * 1.2 + tt, PIL - SL) for dr, tt in ((-0.26, -0.17), (-0.26, 0.17), (0.26, 0.17), (0.26, -0.17))]
            fu.hexa(m, "CT_ConcreteWhite", foot, topq, faces="")
        x, y = P(RB - 0.7, t)
        B.column_round(x, y, 0.0, PIL - SL, 0.25, mat="CT_ConcreteWhite", segs=10)
        # slender posts carrying the gallery edge
        x, y = P(RG - 0.25, t)
        if kk not in (0, NBAY):
            B.column_round(x, y, 0.0, PIL - SL, 0.1, mat="CT_ConcreteWhite", segs=8)

    # ---------------------------------------------------------------- roof: canopy, deck
    ta, tb = th[SUN[0]], th[SUN[1] + 1]

    def canopy(u, v):
        t = ta + (tb - ta) * u
        r = R + 0.6 + v * (RB - R - 1.2)
        x, y = P(r, t)
        return (x, y, 17.0 + 0.55 * math.sin(math.pi * v) + 0.3 * math.sin(math.pi * u) ** 2)
    thick_sheet(det, canopy, 18, 8, lambda u, v: 0.2, "CT_ConcreteWhite", "CT_ConcreteWhite", "CT_ConcreteWhite")
    for u in (0.05, 0.35, 0.65, 0.95):
        for v in (0.0, 1.0):
            x, y, z = canopy(u, v)
            B.column_round(x, y, ZR, z - 0.18, 0.09, mat="CT_ConcreteWhite", segs=8, m=det)
    for b in SUN:
        for (aa, cc) in ((th[b], (th[b] + th[b + 1]) / 2), ((th[b] + th[b + 1]) / 2, th[b + 1])):
            hface(m, "CT_Wood", [P(R + 0.8, aa), P(R + 0.8, cc), P(RB - 0.6, cc), P(RB - 0.6, aa)], ZR + 0.03, True)
    # roof planters and a bench under the canopy
    for (u, v) in ((0.2, 0.8), (0.8, 0.8)):
        t = ta + (tb - ta) * u
        x, y = P(R + 0.7 + v * 6.2, t)
        box(det, "CT_Concrete", x - 1.0, x + 1.0, y - 0.35, y + 0.35, ZR, ZR + 0.5)
        box(det, "CT_Grass", x - 0.9, x + 0.9, y - 0.25, y + 0.25, ZR + 0.5, ZR + 0.62)

    # ---------------------------------------------------------------- forecourt benches
    for kk in (0, 2, 4):
        t = (th[kk] + th[kk + 1]) / 2
        x, y = P(R - 0.4, t)
        pos = (x, y - 0.0, 0.0)
        box(det, "CT_Wood", x - 1.0, x + 1.0, y - 0.25, y + 0.25, 0.42, 0.48)
        box(det, "CT_Concrete", x - 0.9, x - 0.7, y - 0.2, y + 0.2, 0.0, 0.42)
        box(det, "CT_Concrete", x + 0.7, x + 0.9, y - 0.2, y + 0.2, 0.0, 0.42)
        B.spot("forecourt_bench_%d" % kk, "bench", (x, y, 0.48), yaw_of(radial(t)) + 180)

    B.footprint = [P(R, t) for t in th] + [P(RG, t) for t in reversed(th)]
    B.height = 17.9
    add_surrounds(B, seed=4, kinds=("street",))
    B.note("%d flats, %d cobogo blocks on the core front" % (len(B.flats), nblk))
