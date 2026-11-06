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
            F = B.facade(p0, p1, z0, z1, mat, holes=holes, depth=0.30, back=False)
            # core behind the holes
            core = [F.to_building((u, -0.30, z)) for u, z in ((0, z0), (L, z0), (L, z1), (0, z1))]
            m.face(mat, core)
            for a, b in ws:
                B.facade_window(F, a, b, wz[0], wz[1], flat=fid, kind="flat", cols=cols, rows=1, inset=0.15)
            if canyon_face and (c, r, k) not in duplex_up:
                du = (0.55, 1.55) if name == "E" else (3.9, 4.9)
                dm = rnd.choice(DOORS)
                pts = [F.to_building((u, 0.02, z)) for u, z in ((du[0], z0), (du[1], z0), (du[1], z0 + 2.1), (du[0], z0 + 2.1))]
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
        # planter strip with turf
        if rnd.random() < 0.8:
            px0, px1 = x0 + 0.8, x1 - 0.8
            py0 = y0 + 0.9
            box(m, "CT_ConcreteDark", px0, px1, py0, py0 + 0.9, zt, zt + 0.45)
            m.face("CT_Grass", [(px0 + 0.08, py0 + 0.08, zt + 0.45), (px1 - 0.08, py0 + 0.08, zt + 0.45),
                                (px1 - 0.08, py0 + 0.82, zt + 0.45), (px0 + 0.08, py0 + 0.82, zt + 0.45)])
        if rnd.random() < 0.55:
            tx = rnd.uniform(x0 + 1.5, x1 - 1.5)
            ty = y0 + 1.35
            tree(d, tx, ty, zt + 0.45, rnd.uniform(1.2, 1.8))
        if rnd.random() < 0.5:
            wx0 = rnd.uniform(x0 + 0.6, x1 - 4.6)
            box(m, "CT_Wood", wx0, wx0 + 3.6, y1 - 2.4, y1 - 0.5, zt, zt + 0.07)

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
        B.facade_window(F, 1.2, 3.4, 2.5, 2.7, kind="stair", inset=0.12) if False else None
        prism(m, "CT_Concrete", [(xa - 0.2, -0.5), (xb + 0.2, -0.5), (xb + 0.2, Y0), (xa - 0.2, Y0)], 2.7, zr + 0.15,
              top=True, bottom=True)
        m.face("CT_Floor", [(xa + 0.3, 0.3, 0.03), (xb - 0.3, 0.3, 0.03), (xb - 0.3, Y0 - 0.3, 0.03), (xa + 0.3, Y0 - 0.3, 0.03)])
        B.entrance(ent_of_canyon[j], ((xa + xb) / 2, 0.0, 0.0), 0.0, width=2.2, height=2.45, kind="street", leaves=2)
        B.spot("bench_%s" % ent_of_canyon[j], "bench", ((xa + xb) / 2 + 3.2, -0.6, 0.0), 0.0)
    # front yard planters with trees under each bay
    for c in range(NC):
        x0, x1 = bx0(c) + 0.6, bx0(c) + BW - 0.6
        box(m, "CT_ConcreteDark", x0, x1, 0.4, 1.8, 0.0, 0.5)
        m.face("CT_Grass", [(x0 + 0.1, 0.5, 0.5), (x1 - 0.1, 0.5, 0.5), (x1 - 0.1, 1.7, 0.5), (x0 + 0.1, 1.7, 0.5)])
        for tx in (x0 + 2.0, x1 - 2.2):
            tree(d, tx, 1.1, 0.5, 1.9)
        box(d, "CT_Wood", (x0 + x1) / 2 - 1.2, (x0 + x1) / 2 + 1.2, 0.0, 0.4, 0.0, 0.45)
        B.spot("bench_front_%d" % c, "bench", ((x0 + x1) / 2, -0.3, 0.0), 0.0)

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
