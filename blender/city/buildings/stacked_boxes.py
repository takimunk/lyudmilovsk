"""Stacked boxes: a terraced heap of prefab concrete flat-boxes (11.5 x 5.5 x 3 m) climbing from the street up
to the terrace retaining wall at the back (the ground behind, y = 40, is ~7 m higher). An ORIGINAL design in the
spirit of the 1967 Montreal stacked-box housing: not a copy.

Plan (plot 60 x 40 m, max 28 m):
  * 4 bays of boxes across (x), 6 rows deep (y 3 .. 36), separated by 3 open-air "canyons" 4.6 m wide.
    Each canyon is a stepped street: a 2.4 m stair that climbs one level (3 m) per row, flanked by 1.1 m concrete
    walkway decks (with railings) hung on the box sides. The flat doors are on the canyon sides.
  * Heights climb from 2-3 storeys at the street to 8-9 at the back; random voids make a staggered, cantilevered
    heap. The roof of every box that has nothing above it is a garden terrace (paving, planters with turf, trees,
    wood decks, railings); bridges span the canyons at terrace level.
  * Every box is a flat (a few are duplexes: two boxes on top of each other). Big windows on 2-3 sides.
  * Three lift towers (with the red machine rooms) close the canyons at the back; a rear gallery (open walkways at
    every level) runs along the back of the top row, in front of the retaining wall.
  * At the street: three glazed-concrete entrance lobbies (entrances A, B, C) at the canyon mouths, planters
    with trees and benches along the front yard.
Not enterable (flats are not); each street door leads into a small lobby that opens to its canyon stair.
Ideas for later: lit stair slits per flight, laundry on balconies, roof gardens with people, a market in the voids.
Kit wishes: B.facade without end faces forces the skin + core trick used here (face with holes in front of a
plain core face), and a sloped railing helper would be handy.
"""
import math
import random

import furn_int3 as fu
import interior_kit as kit
import _homekit as hk
from citykit import box, prism, Mesh, Facade, extrude_profile

INFO = {
    "name_ru": "Дом-соты (штабель коробок)", "name_en": "Stacked Boxes",
    "style": "brutalism / prefab megastructure", "use": "residential",
    "inspired_by": "1967 Montreal stacked-box housing (prefab concrete boxes heaped in a terraced pile with roof gardens); "
                   "here: canyon stair-streets, retaining-wall backdrop, towers and bridges of our own",
}

BW, BD, BH = 11.5, 5.5, 3.0
CAN, WALK, STW = 4.6, 1.1, 2.4
NC, NR = 4, 6
X0 = -30.0
Y0 = 3.0
WALL_Y = 39.6
DECK_T = 0.30
RAIL = 0.95
FLAT_BOX = (0, 4, 3)        # flat 3-03, the home of Agata Brandt: opened, with the roof garden on top (round 2)
DOOR_OPEN = (0.5, 1.6, 2.25)  # u0, u1, height of its canyon door (E face)

# heights (boxes) per [row][bay]
H = [[2, 3, 2, 3],
     [3, 4, 4, 3],
     [5, 4, 5, 5],
     [6, 6, 5, 6],
     [7, 7, 7, 6],
     [8, 8, 9, 8]]

MATS = ["CT_Concrete"] * 6 + ["CT_ConcreteWhite"] * 3 + ["CT_PlasterYellow", "CT_PlasterBlue", "CT_PlasterRed"]
DOORS = ["CT_Door", "CT_SteelRed", "CT_PlasterYellow", "CT_PlasterBlue", "CT_Door"]


def bx0(c):
    return X0 + c * (BW + CAN)


def ry0(r):
    return Y0 + r * BD


def sheared_bar(m, mat, x0, x1, ya, za, yb, zb, t):
    """A bar of section (x0..x1) x t, sloped from (ya, za) to (yb, zb); za/zb = bottom of the bar."""
    s = (zb - za) / (yb - ya)
    tmp = Mesh()
    box(tmp, mat, x0, x1, ya, yb, 0.0, t)
    tmp.verts = [(x, y, z + za + s * (y - ya)) for x, y, z in tmp.verts]
    m.extend(tmp)


def build(B):
    rnd = random.Random(1967)
    m, d = B.m, B.detail
    TERR = {}

    occ = set()
    for r in range(NR):
        for c in range(NC):
            for k in range(H[r][c]):
                occ.add((c, r, k))
    # voids: not in the top two levels, never ground level
    for r in range(NR):
        for c in range(NC):
            for k in range(1, H[r][c] - 1):
                if rnd.random() < 0.22 and (c, r + 1, k) in occ:
                    occ.discard((c, r, k))
    O = lambda c, r, k: (c, r, k) in occ

    # which boxes are the lower half of a duplex
    duplex_low, duplex_up = set(), set()
    for (c, r, k) in sorted(occ):
        if k % 2 == 1 and (c, r, k) not in duplex_up and O(c, r, k + 1) and rnd.random() < 0.3:
            duplex_low.add((c, r, k))
            duplex_up.add((c, r, k + 1))
    storey_n = {}
    flat_of = {}

    def new_flat(c, r, k, side):
        n = storey_n.get(k, 0) + 1
        storey_n[k] = n
        fid = "%d-%02d" % (k, n)
        return fid

    # canyon each bay's door faces
    door_side = {}
    for c in range(NC):
        if c == 0:
            door_side[c] = "E"
        elif c == NC - 1:
            door_side[c] = "W"
        else:
            door_side[c] = rnd.choice(("E", "W"))
    ent_of_canyon = {0: "A", 1: "B", 2: "C"}

    def canyon_of(c, side):
        return c if side == "E" else c - 1

    # ------------------------------------------------------------- boxes
    for (c, r, k) in sorted(occ):
        x0, x1 = bx0(c), bx0(c) + BW
        y0, y1 = ry0(r), ry0(r) + BD
        z0, z1 = k * BH, (k + 1) * BH
        mat = rnd.choice(MATS)
        if (c, r, k - 1) in duplex_low and (c, r, k) in duplex_up:
            pass
        # flat ownership
        if (c, r, k) in duplex_up:
            fid = flat_of[(c, r, k - 1)]
        else:
            side = door_side[c]
            fid = new_flat(c, r, k, side)
            beds = 3 if (c, r, k) in duplex_low else rnd.choice((1, 2, 2))
            cy = canyon_of(c, side)
            B.flat(fid, storey=k, beds=beds, entrance=ent_of_canyon[cy])
        flat_of[(c, r, k)] = fid
        # top / bottom
        if not O(c, r, k + 1):
            B.m.face("CT_Paving", [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)])
        if k > 0 and not O(c, r, k - 1):
            B.m.face("CT_ConcreteDark", [(x0, y1, z0), (x1, y1, z0), (x1, y0, z0), (x0, y0, z0)])
        # faces: S E N W
        sides = {
            "S": ((x0, y0), (x1, y0), not O(c, r - 1, k)),
            "E": ((x1, y0), (x1, y1), True),
            "N": ((x1, y1), (x0, y1), not O(c, r + 1, k)),
            "W": ((x0, y1), (x0, y0), True),
        }
        pat = rnd.randrange(3)
        for name, (p0, p1, exposed) in sides.items():
            if not exposed:
                continue
            L = BD if name in "EW" else BW
            canyon_face = (name == door_side[c]) and name in "EW"
            if name in "SN":
                if pat == 0:
                    ws = [(0.8, 5.4), (6.1, 10.7)]
                    cols = 2
                elif pat == 1:
                    ws = [(1.6, 9.9)]
                    cols = 4
                else:
                    ws = [(0.8, 3.8), (4.5, 7.5), (8.2, 10.7)]
                    cols = 1
                wz = (z0 + 0.75, z0 + 2.55)
            else:
                if canyon_face:
                    ws = [(2.1, 5.0)] if name == "E" else [(0.5, 3.4)]
                else:
                    ws = [(1.0, 4.5)]
                cols = 1
                wz = (z0 + 0.8, z0 + 2.5)
            holes = [(a, b, wz[0], wz[1]) for a, b in ws]
            opened = (c, r, k) == FLAT_BOX and name in "EW"
            if opened and name == "E":
                holes.append((DOOR_OPEN[0], DOOR_OPEN[1], z0, z0 + DOOR_OPEN[2]))
            F = B.facade(p0, p1, z0, z1, mat, holes=holes, depth=0.30, back=False)
            # core behind the holes (the opened flat is lined from inside instead: _agata_shell)
            if not opened:
                core = [F.to_building((u, -0.30, z)) for u, z in ((0, z0), (L, z0), (L, z1), (0, z1))]
                m.face(mat, core)
            for a, b in ws:
                B.facade_window(F, a, b, wz[0], wz[1], flat=fid, kind="flat", cols=cols, rows=1, inset=0.15)
            if canyon_face and (c, r, k) not in duplex_up:
                du = (0.55, 1.55) if name == "E" else (3.9, 4.9)
                dm = rnd.choice(DOORS)
                pts = [F.to_building((u, 0.02, z)) for u, z in ((du[0], z0), (du[1], z0), (du[1], z0 + 2.1), (du[0], z0 + 2.1))]
                if not opened:
                    d.face(dm, pts)
                # door lintel / canopy
                F2 = F
                pts = [F2.to_building((du[0] - 0.2, 0.0, z0 + 2.1)), F2.to_building((du[1] + 0.2, 0.0, z0 + 2.1))]
                ucen = (du[0] + du[1]) / 2
                dpos = F.to_building((ucen, -0.8, z0 + 0.15))
                B.flat(fid, door=dpos)

    # ------------------------------------------------------------- terraces
    for (c, r, k) in sorted(occ):
        if O(c, r, k + 1):
            continue
        x0, x1 = bx0(c), bx0(c) + BW
        y0, y1 = ry0(r), ry0(r) + BD
        zt = (k + 1) * BH
        # railing edges
        e = 0.06
        if not O(c, r - 1, k) and not O(c, r - 1, k + 1) or (not O(c, r - 1, k)):
            B.railing([(x0 + e, y0 + e), (x1 - e, y0 + e)], zt, h=RAIL, post=2.0)
        if not O(c, r + 1, k) and not O(c, r + 1, k + 1):
            B.railing([(x1 - e, y1 - e), (x0 + e, y1 - e)], zt, h=RAIL, post=2.0)
        # outer plot edges have no deck: railing
        if c == 0:
            B.railing([(x0 + e, y1 - e), (x0 + e, y0 + e)], zt, h=RAIL, post=2.0)
        if c == NC - 1:
            B.railing([(x1 - e, y0 + e), (x1 - e, y1 - e)], zt, h=RAIL, post=2.0)
        TERR[(c, r, k)] = {"planter": False, "tree": None, "deck": None}
        # planter strip with turf
        if rnd.random() < 0.8:
            px0, px1 = x0 + 0.8, x1 - 0.8
            py0 = y0 + 0.9
            TERR[(c, r, k)]["planter"] = True
            box(m, "CT_ConcreteDark", px0, px1, py0, py0 + 0.9, zt, zt + 0.45)
            m.face("CT_Grass", [(px0 + 0.08, py0 + 0.08, zt + 0.45), (px1 - 0.08, py0 + 0.08, zt + 0.45),
                                (px1 - 0.08, py0 + 0.82, zt + 0.45), (px0 + 0.08, py0 + 0.82, zt + 0.45)])
        if rnd.random() < 0.55:
            tx = rnd.uniform(x0 + 1.5, x1 - 1.5)
            ty = y0 + 1.35
            tree(d, tx, ty, zt + 0.45, rnd.uniform(1.2, 1.8))
            TERR[(c, r, k)]["tree"] = (tx, ty)
        if rnd.random() < 0.5:
            wx0 = rnd.uniform(x0 + 0.6, x1 - 4.6)
            box(m, "CT_Wood", wx0, wx0 + 3.6, y1 - 2.4, y1 - 0.5, zt, zt + 0.07)
            TERR[(c, r, k)]["deck"] = (wx0, wx0 + 3.6, y1 - 2.4, y1 - 0.5, zt + 0.07)

    # ------------------------------------------------------------- canyon decks, rails
    bridges = {}
    for j in range(NC - 1):
        for r in range(NR):
            k = min(H[r][j], H[r][j + 1])
            if k >= r + 3 and r in (1, 2, 3, 4) and O(j, r, k - 1) and O(j + 1, r, k - 1):
                bridges[(j, r)] = k
    for c in range(NC):
        for side in ("E", "W"):
            if (side == "E" and c == NC - 1) or (side == "W" and c == 0):
                continue
            j = canyon_of(c, side)
            xa = bx0(c) + BW if side == "E" else bx0(c) - WALK
            xb = xa + WALK
            xe = xb if side == "E" else xa            # outer (stair-side) edge
            for lvl in range(1, 10):
                rows = [r for r in range(NR) if O(c, r, lvl) or (O(c, r, lvl - 1) and not O(c, r, lvl))]
                for r in rows:
                    y0, y1 = ry0(r), ry0(r) + BD
                    z = lvl * BH
                    box(m, "CT_ConcreteDark", xa, xb, y0, y1, z - DECK_T + 0.12, z + 0.12)
                    # rail along the stair side, broken at a bridge
                    gap = None
                    if bridges.get((j, r)) == lvl:
                        gap = (y0 + 2.0, y0 + 3.6)
                    segs = [(y0, y1)] if gap is None else [(y0, gap[0]), (gap[1], y1)]
                    for a, b in segs:
                        if b - a > 0.2:
                            B.railing([(xe, a), (xe, b)], z + 0.12, h=RAIL, post=2.0)
                    # end rails where there is no neighbour deck
                    for rn, yy in ((r - 1, y0), (r + 1, y1)):
                        if rn not in rows:
                            B.railing([(xa, yy), (xb, yy)] if abs(yy - y0) < 1e-6 else [(xb, yy), (xa, yy)],
                                      z + 0.12, h=RAIL, post=2.0) if (rn < 0 or rn >= NR or True) else None
    # bridges
    for (j, r), k in bridges.items():
        xa = bx0(j) + BW + WALK
        xb = xa + STW
        y0 = ry0(r) + 2.0
        z = k * BH
        box(m, "CT_Concrete", xa, xb, y0, y0 + 1.6, z - 0.30 + 0.12, z + 0.12)
        B.railing([(xa, y0 + 0.03), (xb, y0 + 0.03)], z + 0.12, h=RAIL, post=2.0)
        B.railing([(xb, y0 + 1.57), (xa, y0 + 1.57)], z + 0.12, h=RAIL, post=2.0)

    # ------------------------------------------------------------- stairs
    for j in range(NC - 1):
        xa = bx0(j) + BW + WALK
        xb = xa + STW
        for r in range(NR):
            ys = ry0(r)
            z0 = r * BH
            n = 18
            rs = BH / n
            g = 0.28
            poly = []
            low = z0 - 0.45 if r > 0 else 0.0
            poly.append((ys, low))
            poly.append((ys, z0))
            for i in range(1, n + 1):
                poly.append((ys + (i - 1) * g, z0 + i * rs))
                poly.append((ys + i * g, z0 + i * rs))
            poly.append((ys + BD, z0 + BH))
            if r > 0:
                poly.append((ys + BD, z0 + BH - 0.45))
            else:
                poly.append((ys + BD, 0.0))
            # drop duplicate consecutive points
            pp = [poly[0]]
            for p in poly[1:]:
                if math.dist(p, pp[-1]) > 1e-6:
                    pp.append(p)
            extrude_profile(m, "CT_Concrete", "CT_ConcreteDark", pp, xa, xb, lambda u, v, t: (t, u, v))
            # sloped hand rails on both sides
            for xr in (xa + 0.03, xb - 0.03):
                sheared_bar(d, "CT_Steel", xr - 0.025, xr + 0.025, ys, z0 + 0.95, ys + n * g, z0 + BH + 0.95, 0.05)
                sheared_bar(d, "CT_Steel", xr - 0.025, xr + 0.025, ys, z0 + 0.45, ys + n * g, z0 + BH + 0.45, 0.04)
                for i in range(0, n + 1, 5):
                    yy = ys + i * g
                    box(d, "CT_Steel", xr - 0.025, xr + 0.025, yy - 0.025, yy + 0.025, z0 + i * rs, z0 + i * rs + 0.97)

    # ------------------------------------------------------------- lift towers (canyon ends)
    TY0, TY1 = Y0 + NR * BD, WALL_Y
    for j in range(NC - 1):
        xa = bx0(j) + BW + WALK
        xb = xa + STW
        zt = 9 * BH
        tw = []
        for p0, p1, L in (((xa, TY0), (xb, TY0), STW), ((xb, TY0), (xb, TY1), TY1 - TY0),
                          ((xb, TY1), (xa, TY1), STW), ((xa, TY1), (xa, TY0), TY1 - TY0)):
            holes = []
            for lv in range(1, 9):
                zz = lv * BH + 0.9
                holes.append((L / 2 - 0.25, L / 2 + 0.25, zz, zz + 1.5))
            F = B.facade(p0, p1, 0.0, zt, "CT_Concrete", holes=holes, depth=0.3, back=False)
            m.face("CT_Concrete", [F.to_building((u, -0.3, z)) for u, z in ((0, 0), (L, 0), (L, zt), (0, zt))])
            for h in holes:
                B.facade_window(F, h[0], h[1], h[2], h[3], kind="stair", rows=3, inset=0.12)
        m.face("CT_Roof", [(xa, TY0, zt), (xb, TY0, zt), (xb, TY1, zt), (xa, TY1, zt)])
        box(m, "CT_PlasterRed", xa - 0.2, xb + 0.2, TY0 - 0.2, TY1, zt, zt + 0.95, faces="xXyYZ")
        # landing between the last flight and the tower is the flight's own landing; a tower door panel
        pts = [(xa + 0.6, TY0 + 0.02, 5 * BH + 0.0), (xb - 0.6, TY0 + 0.02, 5 * BH + 0.0)]
        d.face("CT_Door", [(xa + 0.6, TY0, 6 * BH), (xb - 0.6, TY0, 6 * BH), (xb - 0.6, TY0, 6 * BH + 2.1), (xa + 0.6, TY0, 6 * BH + 2.1)])
        # NB the door panel faces -y (the canyon): winding as seen from -y, u to the right
        d.faces[-1] = (d.faces[-1][0], d.faces[-1][1], d.faces[-1][2])
        d.verts[-4:] = [(xa + 0.6, TY0 - 0.02, 6 * BH), (xb - 0.6, TY0 - 0.02, 6 * BH),
                        (xb - 0.6, TY0 - 0.02, 6 * BH + 2.1), (xa + 0.6, TY0 - 0.02, 6 * BH + 2.1)]

    # ------------------------------------------------------------- rear gallery along the back of row 5
    r = NR - 1
    y0, y1 = ry0(r) + BD, ry0(r) + BD + 1.2
    for c in range(NC):
        xa = bx0(c) - (WALK if c > 0 else 0.0)
        xb = bx0(c) + BW + (WALK if c < NC - 1 else 0.0)
        for lvl in range(1, 10):
            if O(c, r, lvl) or (O(c, r, lvl - 1) and not O(c, r, lvl)):
                z = lvl * BH
                box(m, "CT_ConcreteDark", xa, xb, y0, y1, z - DECK_T + 0.12, z + 0.12)
                B.railing([(xb, y1 - 0.03), (xa, y1 - 0.03)], z + 0.12, h=RAIL, post=2.0)
                if c == 0:
                    B.railing([(xa + 0.03, y1), (xa + 0.03, y0)], z + 0.12, h=RAIL, post=2.0)
                if c == NC - 1:
                    B.railing([(xb - 0.03, y0), (xb - 0.03, y1)], z + 0.12, h=RAIL, post=2.0)
                for (xx, sgn) in ((xa, 1), (xb, -1)):
                    if 0 < c < NC - 1 or (c == 0 and sgn == -1) or (c == NC - 1 and sgn == 1):
                        pass

    # ------------------------------------------------------------- retaining wall at the back
    xl, xr = X0, bx0(NC - 1) + BW
    box(m, "CT_ConcreteDark", xl, xr, WALL_Y, 40.0, 0.0, 7.0)
    for xx in [xl + 6 * i for i in range(1, int((xr - xl) // 6) + 1)]:
        box(d, "CT_Concrete", xx - 0.15, xx + 0.15, WALL_Y - 0.15, WALL_Y, 0.0, 7.0)

    # ------------------------------------------------------------- street lobbies and front yard
    for j in range(NC - 1):
        xa = bx0(j) + BW
        xb = xa + CAN
        zr = 3.0
        for p0, p1, L, holes in (
                ((xa, 0.0), (xb, 0.0), CAN, [(1.2, 3.4, 0.0, 2.45), (0.45, 0.95, 0.8, 2.3), (3.65, 4.15, 0.8, 2.3)]),
                ((xb, 0.0), (xb, Y0), Y0, [(0.9, 2.1, 0.8, 2.3)]),
                ((xb, Y0), (xa, Y0), CAN, [(1.2, 3.4, 0.0, 2.45)]),
                ((xa, Y0), (xa, 0.0), Y0, [(0.9, 2.1, 0.8, 2.3)])):
            F = B.facade(p0, p1, 0.0, 2.7, "CT_ConcreteWhite", holes=holes, depth=0.3, back=True)
            for h in holes:
                if h[2] > 0.1:
                    B.facade_window(F, h[0], h[1], h[2], h[3], kind="stair", inset=0.12)
        # transom glass above the front door
        F = Facade((xa, 0.0), (xb, 0.0))
        prism(m, "CT_Concrete", [(xa - 0.2, -0.5), (xb + 0.2, -0.5), (xb + 0.2, Y0), (xa - 0.2, Y0)], 2.7, zr + 0.15,
              top=True, bottom=True)
        m.face("CT_Floor", [(xa + 0.3, 0.3, 0.03), (xb - 0.3, 0.3, 0.03), (xb - 0.3, Y0 - 0.3, 0.03), (xa + 0.3, Y0 - 0.3, 0.03)])
        B.entrance(ent_of_canyon[j], ((xa + xb) / 2, 0.0, 0.0), 0.0, width=2.2, height=2.45, kind="street", leaves=2)
        B.spot("bench_%s" % ent_of_canyon[j], "bench", ((xa + xb) / 2 + 3.2, -0.6, 0.0), 0.0)
        # lobby furnishing (floor z 0.03, inner walls 0.3 thick): letter boxes on the left wall, a mat, a plant, a lamp
        fu.doormat(B, fu.Fr((xa + xb) / 2, 0.3, 0.0, 0.03), 0.0, -0.45)
        _lobby(B, j, xa, xb)
        B.lamp(((xa + xb) / 2, Y0 / 2, 2.4), lumens=380, colour="#FFE0B0", kind="lobby", radius=4.5)
        box(d, "CT_Steel", (xa + xb) / 2 - 0.01, (xa + xb) / 2 + 0.01, Y0 / 2 - 0.01, Y0 / 2 + 0.01, 2.48, 2.7, faces="xXyY")
    # front yard planters with trees under each bay
    for c in range(NC):
        x0, x1 = bx0(c) + 0.6, bx0(c) + BW - 0.6
        box(m, "CT_ConcreteDark", x0, x1, 0.4, 1.8, 0.0, 0.5)
        m.face("CT_Grass", [(x0 + 0.1, 0.5, 0.5), (x1 - 0.1, 0.5, 0.5), (x1 - 0.1, 1.7, 0.5), (x0 + 0.1, 1.7, 0.5)])
        for tx in (x0 + 2.0, x1 - 2.2):
            tree(d, tx, 1.1, 0.5, 1.9)
        box(d, "CT_Wood", (x0 + x1) / 2 - 1.2, (x0 + x1) / 2 + 1.2, 0.0, 0.4, 0.0, 0.45)
        B.spot("bench_front_%d" % c, "bench", ((x0 + x1) / 2, -0.3, 0.0), 0.0)

    _street_life(B, O, TERR)
    _round2(B, O, TERR)
    for w_ in kit.check(B):
        print("KIT boxes", w_)
    for a_ in getattr(B, "_late_spots", []):          # spots on a seat or at a cooker stand inside the piece by design
        B.spot(*a_)
    B.footprint = [(X0, -0.5), (bx0(NC - 1) + BW, -0.5), (bx0(NC - 1) + BW, 40.0), (X0, 40.0)]
    B.height = 28.0
    B.note("%d flats (boxes; %d duplexes), 3 canyons with stairs, 3 lift towers, %d bridges" % (len(B.flats), len(duplex_low), len(bridges)))


def tree(d, x, y, z, h):
    """A low-poly tree (trunk + two-cone crown) into the detail mesh."""
    n = 6
    r = 0.09
    trunk = [(x + r * math.cos(2 * math.pi * k / n), y + r * math.sin(2 * math.pi * k / n)) for k in range(n)]
    prism(d, "CT_Wood", trunk, z, z + h * 0.5, top=False, bottom=False)
    R = h * 0.42
    ring = [(x + R * math.cos(2 * math.pi * k / 8), y + R * math.sin(2 * math.pi * k / 8)) for k in range(8)]
    zc = z + h * 0.35
    for k in range(8):
        a, b = ring[k], ring[(k + 1) % 8]
        d.face("CT_Grass", [(a[0], a[1], zc), (b[0], b[1], zc), (x, y, zc + h * 0.55)])
        d.face("CT_Grass", [(b[0], b[1], zc), (a[0], a[1], zc), (x, y, zc - h * 0.12)])


# ================================================================================================ interiors
def _lobby(B, j, xa, xb):
    """The three street lobbies (inner x xa+0.3..xb-0.3, y 0.3..2.7, floor 0.03, ceiling 2.7; the passage x +1.2..+3.4
    from the street door to the canyon stair stays clear): letter boxes, a house board, a bench under the side window,
    and one more thing that tells who lives here (a pram, a bike, a hat stand and a payphone)."""
    z = 0.03
    iw, ie, iy0, iy1 = xa + 0.3, xb - 0.3, 0.3, Y0 - 0.3
    H = hk.Home(B, lambda x, y: iw + 0.02 < x < ie - 0.02 and iy0 + 0.02 < y < iy1 - 0.02)
    f = H.f
    tag = "ABC"[j]
    f(hk.mailboxes, iw, 2.42, 90.0, z=z, cols=2, rows=5)
    f(kit.notice_board, iw, 0.62, 90.0, z=z, w=0.55, zc=1.6, seed=60 + j)
    bn = f(kit.bench, ie - 0.25, 1.4, 270.0, z=z, w=1.0, kind="gym")
    f(kit.hat_stand, ie - 0.38, 0.62, 0.0, z=z)
    f(kit.plant, ie - 0.45, 2.28, 0.0, z=z, kind=("fern", "cactus", "fern")[j], pot="CT_PlasterRed", h=0.6, seed=70 + j)
    if j == 0:      # a pram under the west window, a radio on a shelf
        _prop_at(B, hk.pram, iw + 0.5, 1.5, z, 90.0, mat="CT_PlasterBlue")
        B.thing("lobby_%s_board" % tag, "board", (iw, 0.62, z + 1.6), 90.0, topic="lost")
    elif j == 1:    # a bike leaning at the west wall
        _prop_at(B, hk.bike, iw + 0.4, 1.5, z, 0.0, mat="CT_PlasterYellow")
        B.thing("lobby_%s_board" % tag, "board", (iw, 0.62, z + 1.6), 90.0, topic="club")
        f(kit.picture, ie, 0.62, 270.0, z=z, name="painting_sea_night", w=0.45, zc=2.15)
    else:           # a payphone on the back wall, the house rules on the board
        f(kit.wall_phone, xa + 0.95, iy1, 0.0, z=z)
        B.thing("lobby_%s_board" % tag, "board", (iw, 0.62, z + 1.6), 90.0, topic="rules")
        B.thing("lobby_%s_payphone" % tag, "machine", (xa + 0.95, iy1, z + 1.4), 0.0, what="payphone")
        bw = f(kit.bench, iw + 0.25, 1.5, 90.0, z=z, w=1.2, kind="gym")
        H.seat("lobby_%s_bench_w" % tag, bw)
    H.seat("lobby_%s_bench" % tag, bn)
    H.check("boxes-lobby-" + tag, overlaps=False)


def _prop_at(B, fn, x, y, z, yaw, **kw):
    """hk.pram / hk.bike stand on z = 0: draw one and lift the new faces by z."""
    n0 = len(B.detail.verts)
    r = fn(B, x, y, yaw, **kw)
    B.detail.verts[n0:] = [(vx, vy, vz + z) for vx, vy, vz in B.detail.verts[n0:]]
    return r


def _street_life(B, O, TERR):
    """Life on the stair-streets and the roof: on the lowest deck of every canyon a bench, a pot and a board; one roof
    garden corner (a deck with a chess table, stools, a bench and planters)."""
    rng = random.Random(8)
    f = lambda fn, x, y, yaw=0.0, z=0.0, **kw: fn(B, x, y, z, yaw, **kw)
    # ---- canyon decks at level 1 (z 3.0), the first row
    for j in range(NC - 1):
        for c, side in ((j, "E"), (j + 1, "W")):
            if not (O(c, 0, 1) or O(c, 0, 0) and not O(c, 0, 1)):
                continue
            z = BH
            xa = bx0(c) + BW if side == "E" else bx0(c) - WALK
            xw = xa if side == "E" else xa + WALK            # the box wall the deck hangs on
            if side == "E":
                wall_x, yaw_in = bx0(c) + BW, 90.0
            else:
                wall_x, yaw_in = bx0(c), 270.0
            y = ry0(0) + 3.6
            b = f(kit.bench, wall_x + (0.22 if side == "E" else -0.22), y, yaw_in, z=z + 0.12, w=1.1, kind="gym")
            tid = "canyon_%d%s_bench" % (j, side.lower())
            s = b["seats"][0]
            B.thing(tid, "seat", s[:3], s[3], height=round(s[2] - z - 0.12, 2))
            f(kit.plant, wall_x + (0.3 if side == "E" else -0.3), ry0(0) + 2.5, 0.0, z=z + 0.12, kind="fern", pot="CT_PlasterYellow", h=0.7, seed=80 + j)
            f(kit.notice_board, wall_x, ry0(0) + 4.6, yaw_in, z=z + 0.12, w=0.6, zc=1.6, seed=90 + j + (0 if side == "E" else 5))
            B.thing("canyon_%d%s_board" % (j, side.lower()), "board", (wall_x, ry0(0) + 4.6, z + 1.7), yaw_in, topic="lost" if side == "E" else "club")
    # ---- the roof garden corner: the street-most terrace with a timber deck
    cands = [(r_ * 100 + abs(c_ - 1) + k_, (c_, r_, k_)) for (c_, r_, k_), t in TERR.items() if t["deck"] and r_ <= 3]
    if not cands:
        return
    key = min(cands)[1]
    t = TERR[key]
    wx0, wx1, dy0, dy1, dz = t["deck"]
    cx = (wx0 + wx1) / 2
    ch = f(kit.chess_table, cx - 0.6, (dy0 + dy1) / 2, 0.0, z=dz)
    f(kit.stool, cx - 0.6, (dy0 + dy1) / 2 - 0.7, 0.0, z=dz, h=0.45)
    f(kit.stool, cx - 0.6, (dy0 + dy1) / 2 + 0.7, 0.0, z=dz, h=0.45)
    bn = f(kit.bench, wx1 - 0.8, dy1 - 0.3, 0.0, z=dz, w=1.3, kind="park")
    f(kit.plant, wx0 + 0.3, dy1 - 0.4, 0.0, z=dz, kind="palm", pot="CT_PlasterRed", h=1.2, seed=95)
    f(kit.plant, wx1 - 0.3, dy0 + 0.35, 0.0, z=dz, kind="ficus", pot="CT_TileWhite", h=1.0, seed=96)
    s = bn["seats"][0]
    B.thing("roof_garden_bench", "seat", s[:3], s[3], height=round(s[2] - dz, 2))
    B.thing("roof_garden_chess", "play", (cx - 0.6, (dy0 + dy1) / 2, dz + 0.78), 0.0, what="chess")
    B.spot("roof_garden", "bench", (s[0], s[1], dz), s[3])


# ================================================================================================ round 2: depth pass
# The canyon stair-streets and the roof-garden flat 3-03 of Agata Brandt. Everything here is drawn into the module's own
# meshes with kit primitives; nothing outside the boxes moves. Heights: a canyon deck of level L has its top at z = 3 L + 0.12.
XW, XE, YS, YN = -30.0, -18.5, 25.0, 30.5         # the box (0, 4, 3): its footprint
Z0, FL, ZT = 9.0, 9.12, 12.0                      # floor of the box, top of the deck and of the flat's floor, roof terrace
IX0, IX1, IY0, IY1 = -29.6, -18.9, 25.4, 30.1     # the flat inside (lining 0.1 behind the old core plane)
CEIL = ZT - 0.12


def _prop(B, x, y, z, yaw, name):
    p = kit._P(B, x, y, z, yaw, name)
    p.kind = "prop"
    return p


def _round2(B, O, TERR):
    assert O(*FLAT_BOX) and not O(0, 4, 4) and O(0, 3, 3) and O(0, 5, 3), "the plan of the heap changed: flat 3-03"
    _agata_shell(B)
    _agata_flat(B)
    _roof_garden(B, TERR)
    _canyon_life(B, O)


def _agata_shell(B):
    """Lining, floor and ceiling inside the box (the facade's reveals are 0.3 deep: the lining stands 0.1 behind them),
    the open door leaf, the sills."""
    m = B.m
    # floor and ceiling slabs
    box(m, "CT_ConcreteDark", XW, XE, YS, YN, Z0, FL, faces="xXyYz")
    box(m, "CT_Wood", XW + 0.3, XE - 0.3, YS + 0.4, YN - 0.4, Z0, FL, faces="Z")
    box(m, "CT_Concrete", XE - 0.3, XE, YS, YN, Z0, FL, faces="Z")          # the door's threshold: concrete
    box(m, "CT_ConcreteDark", XW, XE, YS, YN, CEIL, ZT - 0.005, faces="xXyY")
    box(m, "CT_Interior", XW, XE, YS, YN, CEIL, ZT - 0.005, faces="z")
    wm = "CT_Interior"
    # south and north walls (the neighbours' boxes are open voids behind them)
    box(m, wm, XW, XE, YS, IY0, FL, CEIL)
    box(m, wm, XW, XE, IY1, YN, FL, CEIL)
    # east wall x -18.9..-18.8: door 25.5..26.6 (to 2.25 over Z0), window 27.1..30.0 (9.8..11.5)
    ex0, ex1 = XE - 0.4, XE - 0.3
    box(m, wm, ex0, ex1, IY0, 25.5, FL, CEIL)
    box(m, wm, ex0, ex1, 25.5, 26.6, Z0 + DOOR_OPEN[2], CEIL)
    box(m, wm, ex0, ex1, 26.6, 27.1, FL, CEIL)
    box(m, wm, ex0, ex1, 27.1, 30.0, FL, 9.8)
    box(m, wm, ex0, ex1, 27.1, 30.0, 11.5, CEIL)
    box(m, wm, ex0, ex1, 30.0, IY1, FL, CEIL)
    # west wall x -29.7..-29.6: window 26.0..29.5
    wx0, wx1 = XW + 0.3, XW + 0.4
    box(m, wm, wx0, wx1, IY0, 26.0, FL, CEIL)
    box(m, wm, wx0, wx1, 29.5, IY1, FL, CEIL)
    box(m, wm, wx0, wx1, 26.0, 29.5, FL, 9.8)
    box(m, wm, wx0, wx1, 26.0, 29.5, 11.5, CEIL)
    d = B.detail
    # window sills inside: a wooden board 0.18 deep
    box(d, "CT_Wood", ex0 - 0.18, ex0, 27.1, 30.0, 9.77, 9.81)
    box(d, "CT_Wood", wx1, wx1 + 0.18, 26.0, 29.5, 9.77, 9.81)
    # the front door of the flat stands open against the south wall; frame, brass plate and a number
    box(d, "CT_Wood", ex0 - 0.02, ex1 + 0.02, 25.44, 25.5, FL, Z0 + DOOR_OPEN[2] - 0.05, faces="xXyY")
    box(d, "CT_Wood", ex0 - 0.02, ex1 + 0.02, 26.6, 26.66, FL, Z0 + DOOR_OPEN[2] - 0.05, faces="xXyY")
    box(d, "CT_Wood", ex0 - 0.02, ex1 + 0.02, 25.44, 26.66, Z0 + DOOR_OPEN[2] - 0.05, Z0 + DOOR_OPEN[2] + 0.02, faces="xXyYZ")
    box(d, "CT_WoodLight", -20.0, ex0 - 0.03, IY0, IY0 + 0.045, FL, FL + 2.08)              # the leaf
    box(d, "CT_Brass", -19.2, -19.13, IY0 + 0.045, IY0 + 0.06, FL + 1.0, FL + 1.04)
    # outside: the nameplate on the wall beside the door, a mat on the deck
    box(d, "CT_Brass", XE, XE + 0.012, 26.72, 26.98, Z0 + 1.45, Z0 + 1.6)
    box(d, "CT_PlasterRed", XE + 0.0, XE + 0.45, 25.55, 26.55, FL, FL + 0.014, faces="xXyYZ")
    box(d, "CT_Steel", XE + 0.0, XE + 0.45, 25.55, 26.55, FL + 0.014, FL + 0.016, faces="Z")


def _agata_flat(B):
    """Flat 3-03: Agata Brandt, 71, who ran the seed bank of the harbour's botanic station for thirty years and now keeps
    a garden on the roof of her own flat. A long room: the door and the kitchen at the east end, the dining table and the
    window armchair, the seed desk and the shelves along the north wall, the bed and the guest sofa in the west."""
    f = lambda fn, x, y, yaw=0.0, **kw: fn(B, x, y, FL, yaw, **kw)
    # --- the entrance and the kitchen run along the south wall
    f(kit.shoe_rack, -21.25, IY0, 180.0, w=0.8, seed=31)
    f(kit.hat_stand, -20.35, 25.72, 0.0)
    ku = f(kit.kitchen_unit, -24.0, 25.7, 180.0, modules="cupboard,sink,stove,fridge", wall_cupboards=False)
    kit.teapot(B, -23.25, 25.7, FL + 0.9, 0.0)
    kit.cup(B, -22.98, 25.62, FL + 0.9, 0.0)
    f(kit.wall_clock, -24.0, IY0, 180.0, zc=2.15, face="clock_1")
    f(kit.poster, -18.9, 26.85, 270.0, name="poster", w=0.3, zc=1.78, seed=8)
    # --- the dining corner: a round table on a rug with two chairs
    f(kit.rug, -21.8, 27.25, 0.0, w=2.3, d=1.9, name="rug_2")
    tb = f(kit.table_round, -21.8, 27.25, 0.0, r=0.5, h=0.74)
    c_s = f(kit.chair, -21.8, 26.45, 180.0, kind="wood")
    c_n = f(kit.chair, -21.8, 28.05, 0.0, kind="wood")
    kit.vase(B, -21.8, 27.25, tb["top"], 0.0, seed=4)
    kit.cup(B, -22.1, 27.1, tb["top"], 0.0)
    kit.newspaper(B, -21.5, 27.4, tb["top"], 0.0, seed=2)
    # --- the window armchair and a lamp
    ar = f(kit.armchair, -20.25, 28.6, 90.0, mat="CT_PlasterGreen")
    ct = f(kit.table_round, -19.6, 27.55, 0.0, r=0.25, h=0.5)
    kit.cup(B, -19.6, 27.55, ct["top"], 0.0)
    f(kit.floor_lamp, -19.45, 29.8, 0.0)
    # seedling trays on the east sill, a row of pots on the west sill
    for k, y in enumerate((27.6, 28.4, 29.2)):
        _tray(B, -19.03, y, 9.81, 90.0, k)
    for k, y in enumerate((26.5, 27.5, 28.5)):
        kit.plant(B, -29.46, y, 9.81, 0.0, kind=("cactus", "fern", "cactus")[k], pot=("CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterBlue")[k], h=0.4, seed=40 + k)
    # --- the north wall: the bed in the corner, two bookshelves, the seed desk, a radio shelf
    bd = f(kit.bed, -28.55, 29.55, 90.0, size="single", blanket="CT_PlasterYellow")
    bt = f(kit.bedside_table, -29.2, 28.85, 90.0, lamp=True)
    f(kit.picture, -28.6, IY1, 0.0, name="painting_dunes", w=0.5, zc=1.85)
    sh1 = f(kit.bookshelf, -26.4, IY1 - 0.01, 0.0, w=1.1, h=1.9, levels=5, topic="garden", seed=71)
    dk = f(kit.table, -23.0, 29.75, 0.0, w=1.4, d=0.7, h=0.76)
    kit.lamp_green(B, -23.55, 29.9, dk["top"], 0.0)
    kit.books_stack(B, -22.5, 29.8, dk["top"], 0.0, n=3, seed=9)
    _packets(B, -23.1, 29.45, dk["top"])
    f(kit.chair, -23.0, 29.05, 180.0, kind="wood")
    f(kit.notice_board, -23.0, IY1, 0.0, w=0.9, zc=1.75, seed=77)
    f(kit.poster, -21.55, IY1, 0.0, name="poster", w=0.34, zc=1.5, seed=3)
    ws = f(kit.wall_shelf, -20.45, IY1, 0.0, w=0.9, zt=1.3, seed=5)
    kit.radio(B, -20.45, 29.95, ws["top"], 0.0)
    jar = _prop(B, -20.8, 29.95, ws["top"], 0.0, "seed_jar")
    jar.cyl("CT_Glass", 0, 0, 0.0, 0.2, 0.065, 8)
    jar.cyl("CT_Paper", 0, 0, 0.0, 0.13, 0.055, 8)
    jar.cyl("CT_Steel", 0, 0, 0.2, 0.22, 0.068, 8)
    jar.done()
    # --- the west zone: the guest sofa under the south wall, a low table, photographs
    f(kit.rug, -27.9, 27.0, 0.0, w=1.8, d=1.3, name="rug_4")
    sf = f(kit.sofa, -28.0, 25.85, 180.0, seats=2, mat="CT_PlasterBlue")
    cf = f(kit.coffee_table, -28.0, 27.0, 0.0, w=0.9, d=0.5)
    kit.books_stack(B, -28.2, 27.0, cf["top"], 0.0, n=2, seed=5)
    f(kit.picture, -29.6 + 0.0, 25.72, 90.0, name="painting_sea_calm", w=0.4, zc=1.65)
    f(kit.picture, -29.6 + 0.0, 29.8, 90.0, name="painting_lighthouse", w=0.4, zc=1.65)
    # --- light: two lamps in a 10 m room
    B.lamp((-20.9, 27.0, CEIL - 0.28), lumens=420, colour="#FFE0B0", kind="public", radius=6.2)
    B.lamp((-26.6, 27.7, CEIL - 0.28), lumens=380, colour="#FFD9A8", kind="public", radius=6.2)
    # --- things (ids ag_*)
    T = B.thing
    T("ag_door", "door", (-18.95, 26.05, FL + 1.1), 270.0, to="flat")
    T("ag_switch", "machine", (-18.9, 26.85, FL + 1.3), 270.0, what="switch", switch="room")
    T("ag_note", "board", (-18.9, 26.85, FL + 1.78), 270.0, topic="note")
    T("ag_armchair", "seat", (-20.25, 28.6, FL + 0.45), 90.0, height=0.45)
    T("ag_table", "play", (-21.8, 27.25, FL + 0.8), 0.0, what="dominoes")
    T("ag_chair_s", "seat", (-21.8, 26.45, FL + 0.45), 180.0, height=0.45)
    T("ag_chair_n", "seat", (-21.8, 28.05, FL + 0.45), 0.0, height=0.45)
    T("ag_desk_chair", "seat", (-23.0, 29.05, FL + 0.45), 180.0, height=0.45)
    T("ag_notebook", "shelf", (-23.2, 29.85, FL + 0.8), 0.0, topic="diary")
    T("ag_packets", "board", (-23.0, IY1, FL + 1.75), 0.0, topic="packets")
    T("ag_letters", "shelf", (-22.5, 29.8, FL + 0.85), 0.0, topic="letters")
    T("ag_shelf_garden", "shelf", (-26.4, IY1 - 0.1, FL + 1.3), 0.0, topic="garden")
    T("ag_clock", "board", (-24.0, IY0, FL + 2.15), 180.0, topic="clock")
    T("ag_kettle", "machine", (-24.3, 25.7, FL + 1.0), 180.0, what="stove")
    T("ag_sink", "machine", (-23.7, 25.7, FL + 1.0), 180.0, what="sink")
    T("ag_radio", "machine", (-20.45, 29.95, FL + 1.45), 0.0, what="radio")
    T("ag_jar", "board", (-20.8, 29.95, FL + 1.45), 0.0, topic="jar")
    T("ag_sill_east", "board", (-19.03, 28.35, FL + 0.8), 270.0, topic="seedlings")
    T("ag_sill_west", "board", (-29.46, 27.8, FL + 0.8), 90.0, topic="pots")
    T("ag_view_east", "board", (-18.95, 28.6, FL + 1.4), 270.0, topic="view")
    T("ag_photo_station", "board", (-29.6, 25.72, FL + 1.65), 90.0, topic="photo")
    T("ag_photo_ruben", "board", (-29.6, 29.8, FL + 1.65), 90.0, topic="photo")
    T("ag_bed", "bed", (-28.55, 29.55, FL + 0.5), 90.0, rest="private", height=0.5)
    T("ag_sofa", "bed", (-28.0, 25.85, FL + 0.45), 180.0, rest="night", height=0.45)
    late = B.__dict__.setdefault("_late_spots", [])
    late.append(("agata_armchair", "seat", (-20.25, 28.6, FL), 90.0))
    B.spot("agata_desk", "seat", (-23.0, 29.05, FL), 180.0)
    late.append(("agata_kitchen", "work", (-24.0, 25.95, FL), 180.0))
    B.spot("agata_table", "seat", (-21.8, 28.05, FL), 0.0)
    B.spot("agata_window", "view", (-19.4, 28.0, FL), 90.0)
    late.append(("agata_sofa", "seat", (-28.0, 26.1, FL), 180.0))


def _tray(B, x, y, z, yaw, seed):
    """A seedling tray 0.5 x 0.14: a wooden box, soil, rows of little green shoots."""
    p = _prop(B, x, y, z, yaw, "tray")
    rnd = random.Random(seed)
    p.box("CT_Wood", -0.25, 0.25, -0.07, 0.07, 0.0, 0.05, f="xXyYz")
    p.box("CT_ConcreteDark", -0.235, 0.235, -0.055, 0.055, 0.03, 0.052, f="Z")
    for i in range(5):
        u = -0.2 + i * 0.1
        h = 0.07 + rnd.random() * 0.05
        p.box("CT_Leaf", u - 0.03, u + 0.03, -0.012, 0.012, 0.05, 0.05 + h, f="xXyYZ")
    p.done()


def _packets(B, x, y, z):
    """Seed packets fanned on the desk (flat paper cards)."""
    p = _prop(B, x, y, z, 0.0, "packets")
    cols = ["CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterGreen", "CT_Paper", "CT_PlasterBlue", "CT_PlasterPink"]
    for k in range(6):
        u = -0.3 + k * 0.11
        p.box(cols[k], u, u + 0.08, -0.06 + 0.012 * (k % 3), 0.05 + 0.012 * (k % 3), 0.002 * k, 0.002 * k + 0.003, f="xXyYZ")
    p.done()


# ---------------------------------------------------------------------------------------------------- roof garden
def _roof_garden(B, TERR):
    """The garden on the roof of flat 3-03 (terrace z 12): tomatoes, herbs and sunflowers in the planter strip, a table with
    two chairs on the timber deck, a rain barrel, a cold frame, an apple tree in a tub, a bird feeder, a lamp on a pole.
    The terrace is open to the canyon on its east side (the deck of level 4 is flush with it) and walled by the
    neighbours' windows on the north and the south."""
    t = TERR[FLAT_BOX]
    assert t["planter"] and t["deck"], "the terrace of flat 3-03 lost its planter or its deck"
    wx0, wx1, dy0, dy1, dz = t["deck"]
    z = ZT
    f = lambda fn, x, y, yaw=0.0, zz=None, **kw: fn(B, x, y, z if zz is None else zz, yaw, **kw)
    pz = z + 0.45                        # top of the planter strip
    _tomatoes(B, -29.0, -25.4, 26.35, pz)
    _herbs(B, -25.2, -22.0, 26.35, pz)
    _sunflowers(B, -21.8, -19.6, 26.35, pz)
    tb = f(kit.table_round, -25.05, 29.05, 0.0, dz, r=0.45, h=0.72)
    f(kit.chair, -25.95, 29.05, 90.0, dz, kind="wood")
    f(kit.chair, -24.15, 29.05, 270.0, dz, kind="wood")
    kit.cup(B, -25.2, 28.95, tb["top"], 0.0)
    kit.teapot(B, -24.9, 29.15, tb["top"], 0.0)
    ds = _prop(B, -25.05, 28.75, tb["top"], 0.0, "dominoes")
    for k in range(7):
        ds.box("CT_Paper", -0.11 + 0.032 * k, -0.11 + 0.032 * k + 0.026, -0.02, 0.05, 0.0, 0.012, f="xXyYZ")
    ds.done()
    _rain_barrel(B, -19.75, 29.95, z)
    _cold_frame(B, -22.3, 29.8, z)
    _apple_tub(B, -28.85, 29.35, z)
    kit.bird_feeder(B, -22.6, 27.5, z, 0.0, kind="post")
    kit.notice_stand(B, -19.25, 28.9, z, 90.0, w=0.9, front="notice", back="notice", seed=17)
    kit.plant(B, -26.95, 30.1, dz, 0.0, kind="palm", pot="CT_PlasterRed", h=1.3, seed=12)
    kit.plant(B, -23.45, 30.1, dz, 0.0, kind="fern", pot="CT_PlasterYellow", h=0.6, seed=13)
    # a lamp on a pole and a string of bulbs to the south wall
    lx, ly = -24.3, 27.5
    box(B.detail, "CT_Steel", lx - 0.03, lx + 0.03, ly - 0.03, ly + 0.03, z, z + 2.35)
    box(B.detail, "CT_Concrete", lx - 0.15, lx + 0.15, ly - 0.15, ly + 0.15, z, z + 0.06)
    box(B.detail, "CT_Opal", lx - 0.14, lx + 0.14, ly - 0.14, ly + 0.14, z + 2.35, z + 2.6)
    B.lamp((lx, ly, z + 2.5), lumens=360, colour="#FFE2B8", kind="lobby", radius=6.5, fixture=False)
    _string_lights(B, (lx, ly, z + 2.3), (-27.4, 25.1, z + 2.4))
    _string_lights(B, (lx, ly, z + 2.3), (-20.4, 25.1, z + 2.4))
    T = B.thing
    T("rg_tomatoes", "board", (-27.2, 26.35, pz + 0.8), 0.0, topic="tomatoes")
    T("rg_herbs", "board", (-23.6, 26.35, pz + 0.3), 0.0, topic="herbs")
    T("rg_sunflowers", "board", (-20.7, 26.35, pz + 0.9), 0.0, topic="sunflowers")
    T("rg_table", "play", (-25.05, 29.05, tb["top"] + 0.05), 0.0, what="dominoes")
    T("rg_chair_w", "seat", (-25.95, 29.05, dz + 0.45), 90.0, height=0.45)
    T("rg_chair_e", "seat", (-24.15, 29.05, dz + 0.45), 270.0, height=0.45)
    T("rg_barrel", "machine", (-19.75, 29.95, z + 0.9), 0.0, what="sink")
    T("rg_frame", "board", (-22.3, 29.8, z + 0.4), 0.0, topic="frame")
    T("rg_apple", "take", (-28.85, 29.35, z + 1.4), 0.0, item="apple")
    T("rg_feeder", "board", (-22.6, 27.5, z + 1.6), 0.0, topic="feeder")
    T("rg_sign", "board", (-19.25, 28.9, z + 1.4), 90.0, topic="rules")
    T("rg_switch", "machine", (lx, ly - 0.04, z + 1.3), 0.0, what="switch", switch="room")
    T("rg_view", "board", (-29.9, 27.7, z + 1.3), 270.0, topic="view")
    B.spot("agata_roof_table", "seat", (-24.15, 29.05, dz), 270.0)
    B.spot("agata_roof_beds", "stand", (-24.0, 27.2, z), 180.0)
    B.spot("agata_roof_rail", "view", (-29.3, 27.7, z), 270.0)
    B.spot("agata_roof_apple", "stand", (-28.2, 28.6, z), 90.0)


def _leaf_clump(p, x, y, z, r, mat="CT_Leaf", n=3, seed=0):
    rnd = random.Random(seed)
    for k in range(n):
        a = k * 2 * math.pi / n + rnd.random()
        dx, dy = math.cos(a) * r * 0.5, math.sin(a) * r * 0.5
        h = r * (0.7 + rnd.random() * 0.6)
        p.box(mat, x + dx - r * 0.45, x + dx + r * 0.45, y + dy - r * 0.45, y + dy + r * 0.45, z + h * 0.2, z + h, f="xXyYZ")


def _tomatoes(B, xa, xb, y, z):
    p = _prop(B, 0.0, 0.0, 0.0, 0.0, "tomatoes")
    rnd = random.Random(21)
    n = int((xb - xa) / 0.55)
    for k in range(n):
        x = xa + 0.28 + k * 0.55
        p.rod("CT_Wood", (x, y, z), (x, y, z + 1.25), 0.03, 4)
        for j in range(3):
            zz = z + 0.25 + j * 0.34
            _leaf_clump(p, x, y + (0.04 if j % 2 else -0.04), zz, 0.17, seed=k * 7 + j, n=3)
            if j:
                p.box("CT_SteelRed", x + 0.1, x + 0.17, y - 0.05, y + 0.02, zz - 0.04, zz + 0.03, f="xXyYZ")
                p.box("CT_SteelRed", x - 0.15, x - 0.08, y + 0.02, y + 0.09, zz - 0.1, zz - 0.03, f="xXyYZ")
    p.rod("CT_Rubber", (xa + 0.1, y - 0.25, z + 0.55), (xb - 0.1, y - 0.25, z + 0.55), 0.01, 3)
    p.done()


def _herbs(B, xa, xb, y, z):
    p = _prop(B, 0.0, 0.0, 0.0, 0.0, "herbs")
    mats = ["CT_Leaf", "CT_PlasterGreen", "CT_Grass", "CT_PlasterBlue", "CT_Leaf", "CT_PlasterGreen"]
    n = int((xb - xa) / 0.5)
    for k in range(n):
        x = xa + 0.25 + k * 0.5
        for dy in (-0.2, 0.2):
            _leaf_clump(p, x, y + dy, z, 0.14, mat=mats[(k + (dy > 0)) % len(mats)], n=3, seed=k * 3 + (dy > 0))
    # slate markers on sticks
    for k in range(0, n, 2):
        x = xa + 0.25 + k * 0.5
        p.rod("CT_Wood", (x + 0.2, y - 0.3, z), (x + 0.2, y - 0.3, z + 0.3), 0.02, 4)
        p.box("CT_ConcreteDark", x + 0.14, x + 0.26, y - 0.31, y - 0.29, z + 0.22, z + 0.32, f="xXyYZ")
    p.done()


def _sunflowers(B, xa, xb, y, z):
    p = _prop(B, 0.0, 0.0, 0.0, 0.0, "sunflowers")
    rnd = random.Random(33)
    n = int((xb - xa) / 0.45)
    for k in range(n):
        x = xa + 0.25 + k * 0.45
        h = 1.35 + rnd.random() * 0.4
        yy = y + (0.06 if k % 2 else -0.06)
        p.rod("CT_Leaf", (x, yy, z), (x, yy, z + h), 0.035, 5)
        p.box("CT_Leaf", x - 0.14, x + 0.14, yy - 0.02, yy + 0.02, z + 0.6, z + 0.66, f="xXyYZ")
        p.box("CT_Leaf", x - 0.02, x + 0.02, yy - 0.14, yy + 0.14, z + 0.9, z + 0.95, f="xXyYZ")
        # the head faces the canyon (east): a yellow ring and a dark middle
        hx = x + 0.04
        p.rod("CT_PlasterYellow", (hx, yy, z + h - 0.14), (hx, yy, z + h + 0.14), 0.02, 4)
        p.box("CT_PlasterYellow", hx - 0.02, hx + 0.02, yy - 0.15, yy + 0.15, z + h - 0.05, z + h + 0.05, f="xXyYZz")
        p.box("CT_PlasterYellow", hx - 0.02, hx + 0.02, yy - 0.05, yy + 0.05, z + h - 0.15, z + h + 0.15, f="xXyYZz")
        p.box("CT_Wood", hx - 0.035, hx + 0.035, yy - 0.09, yy + 0.09, z + h - 0.09, z + h + 0.09, f="xXyYZz")
    p.done()


def _rain_barrel(B, x, y, z):
    p = _prop(B, x, y, z, 0.0, "rain_barrel")
    p.lathe("CT_PlasterBlue", 0, 0, [(0.27, 0.0), (0.32, 0.12), (0.34, 0.45), (0.32, 0.78), (0.28, 0.85)], 10, top=False)
    p.cyl("CT_ConcreteDark", 0, 0, 0.84, 0.855, 0.28, 10)
    for zz in (0.14, 0.44, 0.72):
        r = 0.33 if zz > 0.3 else 0.3
        p.cyl("CT_Steel", 0, 0, zz, zz + 0.025, r + 0.012, 10, top=False)
    p.rod("CT_Steel", (0.0, -0.3, 0.3), (0.0, -0.42, 0.3), 0.035, 5)
    p.rod("CT_Steel", (0.0, -0.42, 0.3), (0.0, -0.42, 0.24), 0.03, 5)
    p.box("CT_SteelRed", -0.04, 0.04, -0.34, -0.3, 0.34, 0.38, f="xXyYZ")
    # a watering can on the lid
    p.cyl("CT_Aluminium", 0.04, 0.02, 0.855, 1.02, 0.09, 8, r1=0.085)
    p.rod("CT_Aluminium", (0.1, 0.02, 0.9), (0.28, 0.02, 1.07), 0.03, 5)
    p.rod("CT_Aluminium", (-0.02, 0.02, 1.0), (-0.1, 0.02, 0.9), 0.025, 4)
    p.rod("CT_Aluminium", (-0.02, 0.02, 1.0), (0.09, 0.02, 1.02), 0.025, 4)
    p.done()


def _cold_frame(B, x, y, z):
    p = _prop(B, x, y, z, 0.0, "cold_frame")
    p.box("CT_Wood", -0.55, 0.55, -0.3, 0.3, 0.0, 0.3, f="xXyY")
    p.box("CT_ConcreteDark", -0.55, 0.55, -0.3, 0.3, 0.26, 0.27, f="Z")
    rnd = random.Random(8)
    for i in range(6):
        for j in range(2):
            u, v = -0.42 + i * 0.17, -0.12 + j * 0.24
            h = 0.06 + rnd.random() * 0.06
            p.box("CT_Leaf", u - 0.03, u + 0.03, v - 0.03, v + 0.03, 0.27, 0.27 + h, f="xXyYZ")
    # the lid propped open on the back edge: a glass pane and a wooden frame
    zb, zt = 0.3, 0.62
    p.hexa("CT_Glass", [(-0.55, 0.3, zb), (0.55, 0.3, zb), (0.55, 0.3, zb + 0.02), (-0.55, 0.3, zb + 0.02)],
           [(-0.55, 0.62, zt), (0.55, 0.62, zt), (0.55, 0.62, zt + 0.02), (-0.55, 0.62, zt + 0.02)], f="all")
    p.rod("CT_WoodLight", (-0.5, 0.3, zb), (-0.5, 0.3, 0.12), 0.025, 4)
    p.done()


def _apple_tub(B, x, y, z):
    p = _prop(B, x, y, z, 0.0, "apple_tub")
    p.lathe("CT_Wood", 0, 0, [(0.33, 0.0), (0.4, 0.2), (0.42, 0.5)], 10, top=False)
    p.cyl("CT_Steel", 0, 0, 0.12, 0.15, 0.415, 10, top=False)
    p.cyl("CT_ConcreteDark", 0, 0, 0.49, 0.5, 0.4, 10)
    p.done()
    tree(B.detail, x, y, z + 0.5, 2.1)
    rnd = random.Random(5)
    q = _prop(B, x, y, z, 0.0, "apples")
    for k in range(8):
        a = rnd.random() * 6.28
        r = 0.6 + rnd.random() * 0.22
        zz = 1.08 + rnd.random() * 0.1
        q.box("CT_SteelRed", math.cos(a) * r - 0.04, math.cos(a) * r + 0.04, math.sin(a) * r - 0.04, math.sin(a) * r + 0.04,
              zz, zz + 0.08, f="xXyYZz")
    q.done()


def _string_lights(B, a, b, n=7):
    """A sagging cord with small opal bulbs (decoration only) from a to b (x, y, z)."""
    d = B.detail
    pts = []
    for i in range(n + 1):
        t = i / n
        pts.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t - 0.28 * math.sin(math.pi * t)))
    for (x0, y0, z0), (x1, y1, z1) in zip(pts, pts[1:]):
        quad_line(d, (x0, y0, z0), (x1, y1, z1), 0.012)
    for x, y, zz in pts[1:-1]:
        box(d, "CT_Opal", x - 0.045, x + 0.045, y - 0.045, y + 0.045, zz - 0.1, zz - 0.01)


def quad_line(m, p0, p1, w):
    """A thin square rod between two points (any direction) as a sheared box."""
    (x0, y0, z0), (x1, y1, z1) = p0, p1
    h = w / 2
    bot = [(x0 - h, y0 - h, z0), (x0 + h, y0 - h, z0), (x0 + h, y0 + h, z0), (x0 - h, y0 + h, z0)]
    top = [(x1 - h, y1 - h, z1), (x1 + h, y1 - h, z1), (x1 + h, y1 + h, z1), (x1 - h, y1 + h, z1)]
    for i in range(4):
        j = (i + 1) % 4
        m.face("CT_Steel", [bot[i], bot[j], top[j], top[i]])


# ---------------------------------------------------------------------------------------------------- the canyons
def _deck(c, side, L):
    """(wall x, outward x sign, outward kit yaw, deck top z) of the walkway of bay c on its `side` at level L."""
    if side == "E":
        return bx0(c) + BW, 1.0, 90.0, 3.0 * L + 0.12
    return bx0(c), -1.0, 270.0, 3.0 * L + 0.12


def _canyon_life(B, O):
    """Things on the stair-streets, all on decks the hero reaches from a landing: canyon 0 (Agata's: a tap, a post box, the
    house board, laundry across the canyon, a cat and its bowl, benches, a lamp switch and a view point), canyon 1
    (hopscotch, pigeons, laundry, a bench) and canyon 2 (a swap shelf, a bicycle)."""
    def has(c, side, L, r):
        return O(c, r, L) or (O(c, r, L - 1) and not O(c, r, L))

    def at(c, side, L, r, dy, dist=0.0):
        wx, sg, yw, z = _deck(c, side, L)
        assert has(c, side, L, r), "no deck at bay %d %s level %d row %d" % (c, side, L, r)
        return wx + sg * dist, ry0(r) + dy, yw, z

    T = B.thing
    d = B.detail
    # ---------------- canyon 0, west deck of bay 1: the tap
    x, y, yw, z = at(1, "W", 2, 2, 1.85)
    _standpipe(B, x, y, z, yw)
    T("c0_tap", "machine", (x - 0.2 * 1.0, y, z + 0.8), yw, what="sink")
    # ---------------- canyon 0, east deck of bay 0
    x, y, yw, z = at(0, "E", 2, 1, 5.25)
    _postbox(B, x, y, z, yw)
    T("c0_postbox", "board", (x, y, z + 1.4), yw, topic="post")
    x, y, yw, z = at(0, "E", 3, 3, 1.83)
    kit.notice_board(B, x, y, z, yw, w=0.4, zc=1.6, seed=63)
    T("c0_board", "board", (x, y, z + 1.6), yw, topic="stairnews")
    x, y, yw, z = at(0, "E", 3, 4, 0.25)
    box(d, "CT_ConcreteWhite", x, x + 0.014, y - 0.06, y + 0.06, z + 1.25, z + 1.4, faces="xXyYZz")
    box(d, "CT_SteelRed", x + 0.014, x + 0.022, y - 0.025, y + 0.025, z + 1.3, z + 1.34)
    T("c0_switch", "machine", (x + 0.02, y, z + 1.32), yw, what="switch", switch="hall")
    B.lamp((x + 0.5, 22.7, z + 2.45), lumens=260, colour="#FFE0B0", kind="stair", radius=5.5)
    B.lamp((x + 0.5, 27.1, z + 2.45), lumens=260, colour="#FFE0B0", kind="stair", radius=5.5)
    # the bench under Agata's window, a fern and a geranium pot
    x, y, yw, z = at(0, "E", 3, 4, 2.75, 0.22)
    bn = kit.bench(B, x, y + 0.0, z, yw, w=1.2, kind="park")
    s = bn["seats"][0]
    T("c0_bench_l3", "seat", (s[0], s[1], s[2]), s[3], height=round(s[2] - z, 2))
    # (the flat's window is at y 27.1..30.0: the bench sits under its southern half)
    x, y, yw, z = at(0, "E", 3, 4, 1.78, 0.2)
    kit.plant(B, x, y, z, 0.0, kind="fern", pot="CT_PlasterYellow", h=0.75, seed=51)
    x, y, yw, z = at(0, "E", 3, 4, 5.3, 0.2)
    kit.plant(B, x, y, z, 0.0, kind="cactus", pot="CT_PlasterRed", h=0.45, seed=52)
    T("c0_pots", "board", (x, ry0(4) + 3.9, z + 0.55), yw, topic="pots")
    kit.plant(B, bx0(0) + BW + 0.2, ry0(4) + 3.9, z, 0.0, kind="fern", pot="CT_PlasterBlue", h=0.55, seed=53)
    # the cat and its bowl (canyon 0, level 3, row 3)
    x, y, yw, z = at(0, "E", 3, 3, 3.55, 0.28)
    _cat(B, x, y, z, 150.0)
    _bowl(B, x + 0.02, y + 0.42, z)
    T("c0_cat", "board", (x, y + 0.2, z + 0.25), yw, topic="cat")
    # laundry across the canyon at the start of the row of level 3
    _laundry(B, bx0(0) + BW, bx0(1), ry0(3) + 0.45, 3 * 3.0 + 2.82, seed=1)
    T("c0_laundry", "board", (bx0(0) + BW + 2.3, ry0(3) + 0.45, 3 * 3.0 + 2.2), 90.0, topic="laundry")
    # the view point: binoculars on the rail at the south end of the level 4 deck (row 2), a bench under the window
    x, y, yw, z = at(0, "E", 4, 2, 0.55, 1.0)
    _binocular_post(B, x, y, z, 0.0)
    T("c0_view", "board", (x, y - 0.05, z + 1.28), 0.0, topic="canyonview")
    B.lamp((bx0(0) + BW + 0.75, 16.6, 4 * 3.0 + 2.2), lumens=240, colour="#FFE0B0", kind="stair", radius=5.0)
    x, y, yw, z = at(0, "E", 4, 2, 4.0, 0.22)
    bv = kit.bench(B, x, y, z, yw, w=1.2, kind="park")
    s = bv["seats"][0]
    T("c0_bench_view", "seat", (s[0], s[1], s[2]), s[3], height=round(s[2] - z, 2))
    B.spot("canyon0_view", "view", (bx0(0) + BW + 0.55, ry0(2) + 0.8, 4 * 3.0 + 0.12), 0.0)
    B.spot("canyon0_bench_l3", "bench", (s[0] * 0 + bx0(0) + BW + 0.45, ry0(4) + 2.55, 3 * 3.0 + 0.12), yw)
    B.spot("canyon0_tap", "stand", (bx0(1) - 0.55, ry0(2) + 1.85, 2 * 3.0 + 0.12), 270.0)
    B.spot("canyon0_cat", "stand", (bx0(0) + BW + 0.55, ry0(3) + 3.9, 3 * 3.0 + 0.12), 90.0)
    # ---------------- canyon 1: hopscotch on the east deck of bay 1 (no door on that side), pigeons, laundry, a bench
    x, y, yw, z = at(1, "E", 2, 1, 1.2, 0.55)
    _hopscotch(B, x, ry0(1) + 1.2, z)
    T("c1_hopscotch", "board", (x, ry0(1) + 3.0, z + 0.05), yw, topic="hopscotch")
    B.spot("canyon1_hopscotch", "stand", (x, ry0(1) + 0.6, z), 0.0)
    x, y, yw, z = at(1, "E", 3, 3, 3.1, 0.55)
    for k, (dx, dy, a) in enumerate(((0.0, 0.0, 20.0), (0.2, 0.32, 200.0), (-0.12, 0.6, 80.0))):
        _pigeon(B, x + dx, y + dy, z, a, k)
    T("c1_pigeons", "board", (x, y + 0.3, z + 0.12), yw, topic="pigeons")
    _laundry(B, bx0(1) + BW, bx0(2), ry0(2) + 0.5, 2 * 3.0 + 2.82, seed=2)
    T("c1_laundry", "board", (bx0(1) + BW + 2.3, ry0(2) + 0.5, 2 * 3.0 + 2.2), 90.0, topic="laundry")
    x, y, yw, z = at(2, "W", 2, 2, 3.55, 0.22)
    bw_ = kit.bench(B, x, y, z, yw, w=1.2, kind="park")
    s = bw_["seats"][0]
    T("c1_bench", "seat", (s[0], s[1], s[2]), s[3], height=round(s[2] - z, 2))
    B.spot("canyon1_bench", "bench", (bx0(2) - 0.45, ry0(2) + 3.55, 2 * 3.0 + 0.12), yw)
    # ---------------- canyon 2: a swap shelf and a leaning bicycle on the west deck of bay 3
    x, y, yw, z = at(3, "W", 2, 2, 1.85)
    sw = kit.bookshelf(B, x, y, z, yw, w=0.45, h=1.25, d=0.3, levels=3, topic="travel", seed=88)
    T("c2_swap", "take", (x - 0.2, y, z + 0.9), yw, item="magazine_travel")
    x, y, yw, z = at(3, "W", 2, 3, 2.6, 0.45)
    kit.bicycle(B, x, y, z, 0.0, kind="lean", mat="CT_PlasterGreen")
    T("c2_bike", "board", (x, y, z + 0.7), yw, topic="bike")
    B.spot("canyon2_swap", "stand", (bx0(3) - 0.55, ry0(2) + 1.85, 2 * 3.0 + 0.12), 90.0)


def _standpipe(B, x, y, z, yaw):
    """A street standpipe with a tap, a galvanised bucket and a drain grating (wall piece: front -y)."""
    p = _prop(B, x, y, z, yaw, "standpipe")
    p.rod("CT_Steel", (0, -0.05, 0.02), (0, -0.05, 0.95), 0.06, 6)
    p.rod("CT_Steel", (0, -0.05, 0.95), (0, -0.22, 0.95), 0.05, 6)
    p.rod("CT_Steel", (0, -0.22, 0.95), (0, -0.22, 0.86), 0.045, 6)
    p.box("CT_SteelRed", -0.06, 0.06, -0.075, -0.045, 0.98, 1.0, f="xXyYZz")
    p.box("CT_Concrete", -0.09, 0.09, -0.12, 0.0, 0.0, 0.06, f="xXyYZ")
    p.box("CT_ConcreteDark", -0.2, 0.2, -0.55, -0.15, 0.0, 0.006, f="Z")
    for k in range(5):
        p.box("CT_Steel", -0.18 + 0.075 * k, -0.15 + 0.075 * k, -0.53, -0.17, 0.0, 0.012, f="xXyYZ")
    p.cyl("CT_Aluminium", 0, -0.3, 0.012, 0.3, 0.14, 8, r1=0.17, top=False)
    p.cyl("CT_ConcreteDark", 0, -0.3, 0.27, 0.275, 0.165, 8)
    p.rod("CT_Steel", (-0.17, -0.3, 0.28), (0.0, -0.3, 0.42), 0.012, 4)
    p.rod("CT_Steel", (0.17, -0.3, 0.28), (0.0, -0.3, 0.42), 0.012, 4)
    p.done()


def _postbox(B, x, y, z, yaw):
    """A small red letter box on the wall of the deck: a slot, a flap, a yellow horn plate and the collection times."""
    p = _prop(B, x, y, z, yaw, "postbox")
    p.box("CT_SteelRed", -0.16, 0.16, -0.14, 0.0, 1.2, 1.52, f="xXyYz")
    p.hexa("CT_SteelRed", [(-0.16, -0.14, 1.52), (0.16, -0.14, 1.52), (0.16, 0.0, 1.52), (-0.16, 0.0, 1.52)],
           [(-0.13, -0.12, 1.6), (0.13, -0.12, 1.6), (0.13, 0.0, 1.6), (-0.13, 0.0, 1.6)])
    p.box("CT_ConcreteDark", -0.1, 0.1, -0.146, -0.14, 1.42, 1.455, f="xXyYZz")
    p.box("CT_PlasterYellow", -0.07, 0.07, -0.145, -0.14, 1.26, 1.36, f="xXyYZz")
    p.box("CT_SteelWhite", -0.1, 0.1, -0.145, -0.14, 1.215, 1.245, f="xXyYZz")
    p.done()


def _cat(B, x, y, z, yaw):
    """A grey cat sitting, facing the way yaw says, white paws."""
    p = _prop(B, x, y, z, yaw, "cat")
    g = "CT_PlasterGrey"
    p.lathe(g, 0, 0.1, [(0.0, 0.0), (0.075, 0.01), (0.105, 0.08), (0.085, 0.2), (0.05, 0.27), (0.0, 0.28)], 8)
    p.lathe(g, 0, -0.03, [(0.0, 0.2), (0.055, 0.215), (0.068, 0.27), (0.05, 0.32), (0.0, 0.33)], 8)
    for sx in (-1, 1):
        p.face(g, [(sx * 0.02, -0.03, 0.305), (sx * 0.062, -0.03, 0.31), (sx * 0.05, -0.03, 0.375)])
        p.face(g, [(sx * 0.062, -0.03, 0.31), (sx * 0.02, -0.03, 0.305), (sx * 0.05, -0.03, 0.375)])
        p.box("CT_TileWhite", sx * 0.04 - 0.017, sx * 0.04 + 0.017, -0.1, -0.06, 0.0, 0.1, f="xXyYZ")
        p.box("CT_ConcreteDark", sx * 0.025 - 0.008, sx * 0.025 + 0.008, -0.088, -0.083, 0.275, 0.29, f="y")
    p.tube(g, [(0.06, 0.2, 0.015), (0.16, 0.2, 0.015), (0.2, 0.08, 0.015), (0.16, -0.02, 0.015)], 0.034, 4)
    p.done()


def _bowl(B, x, y, z):
    p = _prop(B, x, y, z, 0.0, "bowl")
    p.cyl("CT_PlasterRed", 0, 0, 0.0, 0.05, 0.075, 8, r1=0.11)
    p.cyl("CT_Wood", 0, 0, 0.044, 0.048, 0.085, 8)
    p.done()


def _pigeon(B, x, y, z, yaw, seed):
    p = _prop(B, x, y, z, yaw, "pigeon")
    g = "CT_PlasterGrey"
    p.rod(g, (0, 0.05, 0.1), (0, -0.04, 0.12), 0.1, 6)
    p.rod(g, (0, -0.05, 0.13), (0, -0.08, 0.18), 0.05, 6)
    p.rod("CT_PlasterBlue", (0, -0.03, 0.14), (0, -0.05, 0.16), 0.056, 6)
    p.rod("CT_Brass", (0, -0.1, 0.18), (0, -0.125, 0.175), 0.014, 4)
    p.box("CT_ConcreteDark", -0.04, 0.04, 0.08, 0.2, 0.1, 0.112, f="xXyYZ")
    for sx in (-0.02, 0.02):
        p.rod("CT_PlasterPink", (sx, 0.0, 0.0), (sx, 0.0, 0.07), 0.01, 4)
    p.done()


def _laundry(B, xa, xb, y, z, seed=0):
    """A washing line across a canyon from bracket to bracket at height z, with pegged clothes."""
    d = B.detail
    rnd = random.Random(seed)
    for xx in (xa, xb):
        box(d, "CT_Steel", min(xx, xx - 0.0) - 0.03, xx + 0.03, y - 0.03, y + 0.03, z - 0.04, z + 0.04)
    n = 6
    pts = [(xa + (xb - xa) * i / n, z - 0.12 * math.sin(math.pi * i / n)) for i in range(n + 1)]
    for (x0, z0), (x1, z1) in zip(pts, pts[1:]):
        quad_line(d, (x0, y, z0), (x1, y, z1), 0.01)
    mats = ["CT_TileWhite", "CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterRed", "CT_PlasterGreen", "CT_PlasterPink", "CT_Paper"]
    t = 0.12
    while t < 0.9:
        xx = xa + (xb - xa) * t
        zz = z - 0.12 * math.sin(math.pi * t)
        kind = rnd.choice(("shirt", "sheet", "socks", "towel"))
        mat = rnd.choice(mats)
        if kind == "sheet":
            w, h = 0.8, 0.5
        elif kind == "socks":
            w, h = 0.1, 0.22
        elif kind == "towel":
            w, h = 0.38, 0.55
        else:
            w, h = 0.34, 0.5
        box(d, mat, xx - w / 2, xx + w / 2, y - 0.006, y + 0.006, zz - h, zz, faces="yY")
        if kind == "shirt":
            box(d, mat, xx - w / 2 - 0.12, xx - w / 2, y - 0.006, y + 0.006, zz - 0.2, zz - 0.02, faces="yY")
            box(d, mat, xx + w / 2, xx + w / 2 + 0.12, y - 0.006, y + 0.006, zz - 0.2, zz - 0.02, faces="yY")
        box(d, "CT_Wood", xx - 0.015, xx + 0.015, y - 0.02, y + 0.02, zz - 0.05, zz + 0.02, faces="xXyYZ")
        t += w / (xb - xa) + rnd.uniform(0.07, 0.14)


def _hopscotch(B, x, y, z):
    """Chalk hopscotch squares on a deck (flat painted quads) from y: 1, 2, 3 and 4-5, 6, 7-8, a half-moon."""
    d = B.detail
    mats = ["CT_PlasterYellow", "CT_PlasterPink", "CT_PlasterBlue", "CT_TileWhite"]
    zz = z + 0.004
    rows = [(0.0, 1), (0.42, 1), (0.84, 1), (1.26, 2), (1.68, 1), (2.1, 2), (2.55, 1)]
    for k, (dy, n) in enumerate(rows):
        for j in range(n):
            w = 0.36 if n == 1 else 0.17
            cx = x + (j - (n - 1) / 2) * 0.19
            box(d, mats[(k + j) % 4], cx - w / 2 + (0.02 if n == 1 else 0.0), cx + w / 2 - (0.02 if n == 1 else 0.0), y + dy, y + dy + 0.38, zz, zz + 0.002, faces="Z")
    ring = [(x + 0.2 * math.cos(math.pi * i / 8), y + 2.98 + 0.2 * math.sin(math.pi * i / 8)) for i in range(9)]
    d.face("CT_PlasterRed", [(a, b, zz + 0.002) for a, b in ring])


def _binocular_post(B, x, y, z, yaw):
    """Coin-operated binoculars on a post at a rail: the viewer looks toward -y (local)."""
    p = _prop(B, x, y, z, yaw, "binocular_post")
    p.rod("CT_Steel", (0, 0, 0.02), (0, 0, 1.18), 0.07, 6)
    p.box("CT_Concrete", -0.16, 0.16, -0.16, 0.16, 0.0, 0.04, f="xXyYZ")
    p.box("CT_SteelRed", -0.05, 0.05, -0.1, -0.035, 0.85, 0.98, f="xXyYZz")
    p.rod("CT_Steel", (0, 0, 1.18), (0, 0, 1.28), 0.09, 6)
    for sx in (-0.055, 0.055):
        p.rod("CT_ConcreteDark", (sx, 0.11, 1.32), (sx, -0.14, 1.32), 0.095, 8)
        p.rod("CT_Steel", (sx, -0.14, 1.32), (sx, -0.17, 1.32), 0.11, 8)
        p.rod("CT_Rubber", (sx, 0.11, 1.32), (sx, 0.15, 1.32), 0.06, 6)
    p.box("CT_Steel", -0.07, 0.07, -0.01, 0.03, 1.3, 1.34, f="xXyYZz")
    p.done()
