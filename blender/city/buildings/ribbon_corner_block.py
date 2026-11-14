"""Ribbon block («Дом с лентой»): a five-storey constructivist residential bar with long horizontal ribbon windows,
a fully glazed cylindrical stair drum at the east end and a stair tower at the west end, cantilevered balconies with
tube railings, a flat roof with a solarium pergola, and a shop + laundry on the ground floor. Original design in the
spirit of the Leningrad and Moscow housing of 1928-32 (corridor houses, glazed stair drums, ribbon windows).

Plot 50 x 20 m, max 18 m. Plan (x along the street, y inland):
  bar         x -19..17, y 1.9..12.5, floors at z 0 (shops), 3.1, 6.2, 9.3, 12.4 (flats), roof 15.5, storey 3.1 m.
              Corridor type: a 2 m internal corridor (y 6.2..8.2) joins the two stairs on every floor; 6 bays of 6 m:
              6 front flats (y 1.9..6.2, ribbon windows to the street) and 6 back flats (y 8.2..12.5) per floor.
              Flat ids "<storey>-<nn>" (storeys 1..4; 01..06 front, 07..12 back): 48 flats. Flat doors open onto the corridor.
  drum        centre (20.92, 7.2), radius 4.0, 16 facets, glass from the ground to 17.95 m (ground storey = lobby, the
              main street door "main" in the south facet under a red canopy). A spiral stair (100 steps, rise 0.155,
              going 0.28 at the walking line) climbs round a concrete column to every floor and the roof (floors 1 and
              odd: arrival at the west side where the corridor door is). Ring floors, railings.
  tower       x -25..-19, y 4..10.4, yellow plaster, 17.9 m: a U-stair of two 10-step flights per storey along x, glass
              slit on the west wall, street door "stair_w" on its west side, roof door at 15.5 m.
  balconies   cantilevered 1.5 m on floors 1..4 at bays 1, 3 and 5 (12), concrete slabs, round-tube railings.
  roof        parapet, a timber solarium pergola on steel posts (to 17.9 m), planters and loungers.
Ground floor: shop (x -19..-7, two shop windows, door "shop"), laundry (x -7..-1, door "laundry"), a bike room and a
  red-corner room with plain windows, back rooms; lobby of the drum + the corridor to the west tower.
Windows: flats "flat" (front ribbon + balcony door, back ribbon, end windows), stair drum / tower "stair", shops "public".
Entrances: "main" (drum), "shop", "laundry", "stair_w" (all street doors with door actors).
Spots: shop counter / work / stand, laundry "work" at the machines, lobby benches, bike benches.
Second pass (depth): the grocery gets floor scales, the shopkeeper's abacus and radio, a bread rack, a goods-of-the-day
board and a bottle-deposit corner; the laundry a forgotten-sock line, a mangle, magazines, a lost-property box and
baskets; the roof loungers, a dovecote, a bird feeder, named plant tubs, a rain-barrel shower, a telescope, a samovar
and washing lines. Content: tools/city/content/x_p_t4h2.json.
Ideas for later: interiors of the flats, a glazed solarium cabin on the roof, a cinema poster board, shutters.
"""
import math

from citykit import box, prism, ring, band, band_down, ribbon_wall, quad_bar
from _cons_util import wall, win, hface, rect
from _surrounds import add_surrounds
from furn_int2 import Furn
from furn_int3 import rod, Fr
import furn_int3 as fu
import geom
import interior_kit as kit

INFO = {
    "name_ru": "Дом с лентой (угловой конструктивистский дом)", "name_en": "Ribbon Corner Block",
    "style": "constructivism", "use": "mixed",
    "inspired_by": "Leningrad and Moscow constructivist housing of 1928-32: ribbon windows, glazed cylindrical stair "
                   "drums, cantilevered balconies with tube railings, a shop on the ground floor, roof solarium",
}

X0, X1 = -19.0, 17.0
YF, YB = 1.9, 12.5
YC0, YC1 = 6.2, 8.2
YCC = 7.2
ZF = (0.0, 3.1, 6.2, 9.3, 12.4)
ZR = 15.5
BAY = 6.0
NB = 6
WT = 0.30
XD, YD, RD, ND = 20.92, 7.2, 4.0, 16
TX0, TX1, TY0, TY1 = -25.0, -19.0, 4.8, 9.6
ZDC, ZDT = 17.65, 17.95
BALC = (1, 3, 5)
STEEL, WHITE = "CT_Steel", "CT_FrameWhite"
RISE = 0.155


def fid(s, k):
    return "%d-%02d" % (s, k)


def front_flat(s, b):
    return fid(s, b + 1)


def back_flat(s, b):
    return fid(s, 7 + b)


def tube(m, mat, a, b, r=0.025, n=6):
    """A round tube (n-gon section) from 3D point a to b, caps closed."""
    d = geom.norm(geom.sub(b, a))
    ref = (0, 0, 1) if abs(d[2]) < 0.9 else (1, 0, 0)
    u = geom.norm(geom.cross(d, ref))
    v = geom.cross(d, u)
    ps = [(math.cos(2 * math.pi * i / n) * r, math.sin(2 * math.pi * i / n) * r) for i in range(n)]
    P = lambda p, c: tuple(p[i] + u[i] * c[0] + v[i] * c[1] for i in range(3))
    ra = [P(a, c) for c in ps]
    rb = [P(b, c) for c in ps]
    cen = tuple((a[i] + b[i]) / 2 for i in range(3))
    quads = [[ra[k], ra[(k + 1) % n], rb[(k + 1) % n], rb[k]] for k in range(n)] + [ra[::-1], rb]
    for q in quads:
        nn = geom.cross(geom.sub(q[1], q[0]), geom.sub(q[2], q[0]))
        mid = tuple(sum(p[i] for p in q) / len(q) for i in range(3))
        if sum(nn[i] * (mid[i] - cen[i]) for i in range(3)) < 0:
            q = q[::-1]
        m.face(mat, q)


def tube_rail(m, pts, z, h=1.05, mat=WHITE, post=0.9):
    """Tube railing along the polyline pts: top tube, mid tube, vertical posts every ~0.9 m."""
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        L = math.hypot(bx - ax, by - ay)
        for zz, r in ((z + h, 0.032), (z + h * 0.52, 0.02)):
            tube(m, mat, (ax, ay, zz), (bx, by, zz), r)
        k = max(1, int(round(L / post)))
        for i in range(k + 1):
            px, py = ax + (bx - ax) * i / k, ay + (by - ay) * i / k
            tube(m, mat, (px, py, z), (px, py, z + h), 0.018, n=5)


def DP(a, r):
    a = math.radians(a)
    return (XD + r * math.cos(a), YD + r * math.sin(a))


def drum_ring(r):
    pts = [DP(11.25 + 22.5 * j, r) for j in range(ND)]
    return pts + [pts[0]]


def build(B):
    m, det = B.m, B.detail
    # ------------------------------------------------------------ ground slab, floors, roof slab
    hface(m, "CT_Floor", rect(X0, X1, YF, YB), 0.0, True)
    prism(m, "CT_ConcreteDark", rect(X0 - 0.1, X1 + 0.1, YF - 0.1, YB + 0.1), -0.4, 0.0, top=False, bottom=False)
    for z in ZF[1:] + (ZR,):
        poly = rect(X0, X1, YF, YB)
        hface(m, "CT_Floor" if z < ZR else "CT_Roof", poly, z, True)
        hface(m, "CT_Plaster", poly, z - 0.3, False)
    # the roof terrace is the roof of the bar; its floor finish (timber) is added with the pergola
    # ------------------------------------------------------------ front wall: ribbon windows, shopfront
    front0, front1 = (X0, YF), (X1, YF)
    _front_ground(B)
    for s in range(1, 5):
        z = ZF[s]
        holes, wins = [], []
        for b in range(NB):
            bx = b * BAY
            if b in BALC:
                holes += [(bx + 0.3, bx + 3.7, z + 0.9, z + 2.3), (bx + 4.1, bx + 5.4, z + 0.05, z + 2.3)]
                wins += [(front_flat(s, b), bx + 0.3, bx + 3.7, z + 0.9, z + 2.3, 3, 1),
                         (front_flat(s, b), bx + 4.1, bx + 5.4, z + 0.05, z + 2.3, 1, 3)]
            else:
                holes += [(bx + 0.3, bx + 5.7, z + 0.9, z + 2.3)]
                wins += [(front_flat(s, b), bx + 0.3, bx + 5.7, z + 0.9, z + 2.3, 5, 1)]
        wall(B, front0, front1, z - 0.3, z + 2.8, "CT_Plaster", holes=holes, depth=WT, mat_in="CT_Plaster")
        for f, u0, u1, z0, z1, cols, rows in wins:
            win(B, front0, front1, u0, u1, z0, z1, flat=f, cols=cols, rows=rows, inset=0.16, frame=0.05)
    wall(B, front0, front1, ZF[4] + 2.8, ZR, "CT_Plaster", depth=WT, mat_in="CT_Plaster")
    wall(B, (X1, YB), (X0, YB), ZF[4] + 2.8, ZR, "CT_Plaster", depth=WT, mat_in="CT_Plaster")
    # grey spandrel bands under the ribbons (not across the balcony doors) and slab-edge lines
    for s in range(1, 5):
        z = ZF[s]
        segs = []
        for b in range(NB):
            bx = X0 + b * BAY
            if b in BALC:
                segs += [(bx, bx + 4.1), (bx + 5.4, bx + BAY)]
            else:
                segs += [(bx, bx + BAY)]
        for xa, xb in segs:
            box(det, "CT_PlasterGrey", xa + (0.0 if xa > X0 else 0.0), xb, YF - 0.04, YF, z - 0.3, z + 0.9, faces="yxXZz")
    # ------------------------------------------------------------ back wall (ribbon windows of the back flats)
    back0, back1 = (X1, YB), (X0, YB)
    wall(B, back0, back1, 0.0, 2.8, "CT_PlasterGrey",
         holes=[(1.0 + b * BAY, 1.0 + b * BAY + 3.4, 0.9, 2.2) for b in range(NB)], depth=WT, mat_in="CT_Plaster")
    for b in range(NB):
        win(B, back0, back1, 1.0 + b * BAY, 1.0 + b * BAY + 3.4, 0.9, 2.2, kind="public", cols=3, rows=1, inset=0.16, frame=0.05)
    for s in range(1, 5):
        z = ZF[s]
        holes = [(b * BAY + 0.3, b * BAY + 5.7, z + 0.9, z + 2.3) for b in range(NB)]
        wall(B, back0, back1, z - 0.3, z + 2.8, "CT_Plaster", holes=holes, depth=WT, mat_in="CT_Plaster")
        for b in range(NB):
            # u runs from x = X1 towards the west: the flat at bay column (NB-1-b) from the west
            win(B, back0, back1, b * BAY + 0.3, b * BAY + 5.7, z + 0.9, z + 2.3,
                flat=back_flat(s, NB - 1 - b), cols=5, rows=1, inset=0.16, frame=0.05)
    # ------------------------------------------------------------ east wall (bar end, drum door) and west wall
    east0, east1 = (X1, YF), (X1, YB)
    doorh = (YCC - 0.6 - YF, YCC + 0.6 - YF)
    for s in range(0, 5):
        z = ZF[s]
        wall(B, east0, east1, 0.0 if s == 0 else z - 0.3, z + 2.8, "CT_Plaster",
             holes=[(doorh[0], doorh[1], z + 0.03, z + 2.3)], depth=WT, mat_in="CT_Plaster")
    wall(B, east0, east1, ZF[4] + 2.8, ZR, "CT_Plaster", depth=WT, mat_in="CT_Plaster")
    west0, west1 = (X0, YB), (X0, YF)
    for s in range(0, 5):
        z = ZF[s]
        # tower door at the corridor, end windows of the corner flats on the exposed bits of the wall
        holes = [(YB - YCC - 0.6, YB - YCC + 0.6, z + 0.03, z + 2.3)]
        if s >= 1:
            holes += [(YB - 12.0, YB - 10.6, z + 0.9, z + 2.3), (YB - 4.4, YB - 3.0, z + 0.9, z + 2.3)]
        wall(B, west0, west1, 0.0 if s == 0 else z - 0.3, z + 2.8, "CT_Plaster", holes=holes, depth=WT, mat_in="CT_Plaster")
        if s >= 1:
            win(B, west0, west1, YB - 12.0, YB - 10.6, z + 0.9, z + 2.3, flat=back_flat(s, 0), cols=2, rows=1, inset=0.16, frame=0.045)
            win(B, west0, west1, YB - 4.4, YB - 3.0, z + 0.9, z + 2.3, flat=front_flat(s, 0), cols=2, rows=1, inset=0.16, frame=0.045)
    wall(B, west0, west1, ZF[4] + 2.8, ZR, "CT_Plaster", depth=WT, mat_in="CT_Plaster")
    # ------------------------------------------------------------ corridor walls and flat partitions
    for s in range(0, 5):
        z = ZF[s]
        z0 = z if s else 0.0
        # corridor wall, front flats side (outside on the right when walking east along y = YC0)
        if s >= 1:
            hs = [(b * BAY + 3.0 - 0.5, b * BAY + 3.0 + 0.5, z + 0.03, z + 2.2) for b in range(NB)]
            wall(B, (X0, YC0), (X1, YC0), z0, z + 2.8, "CT_Plaster", holes=hs, depth=0.2, mat_in="CT_Plaster")
            hb = [((NB - 1 - b) * BAY + 3.0 - 0.5, (NB - 1 - b) * BAY + 3.0 + 0.5, z + 0.03, z + 2.2) for b in range(NB)]
            hb = [(X1 - (X0 + (b + 0.5) * BAY) - 0.5, X1 - (X0 + (b + 0.5) * BAY) + 0.5, z + 0.03, z + 2.2) for b in range(NB)]
            wall(B, (X1, YC1), (X0, YC1), z0, z + 2.8, "CT_Plaster", holes=hb, depth=0.2, mat_in="CT_Plaster")
            for k in range(1, NB):
                x = X0 + k * BAY
                box(m, "CT_Plaster", x - 0.1, x + 0.1, YF + WT, YC0 - 0.2, z, z + 2.8)
                box(m, "CT_Plaster", x - 0.1, x + 0.1, YC1 + 0.2, YB - WT, z, z + 2.8)
        else:
            wall(B, (X0, YC0), (X1, YC0), 0.0, 2.8, "CT_Plaster", depth=0.2, mat_in="CT_Plaster")
            wall(B, (X1, YC1), (X0, YC1), 0.0, 2.8, "CT_Plaster", depth=0.2, mat_in="CT_Plaster")
    # flats, doors, entrances
    for s in range(1, 5):
        z = ZF[s]
        for b in range(NB):
            ent = "main" if b >= 3 else "stair_w"
            B.flat(front_flat(s, b), s, beds=2 + (b % 2), door=(X0 + (b + 0.5) * BAY, YC0, z), entrance=ent)
            B.flat(back_flat(s, b), s, beds=2 + ((b + 1) % 2), door=(X0 + (b + 0.5) * BAY, YC1, z), entrance=ent)
    # ------------------------------------------------------------ balconies
    for s in range(1, 5):
        z = ZF[s]
        for b in BALC:
            xa, xb = X0 + b * BAY + 3.0, X0 + b * BAY + 5.9
            y0 = 0.4
            box(m, "CT_Concrete", xa, xb, y0, YF, z - 0.28, z)
            box(det, "CT_SteelRed", xa, xb, y0 - 0.04, y0, z - 0.28, z + 0.04, faces="yxXZz")
            box(det, "CT_ConcreteDark", xa, xb, y0, y0 + 0.06, z - 0.28, z + 0.06, faces="yxX") if False else None
            tube_rail(det, [(xa + 0.03, YF - 0.05), (xa + 0.03, y0 + 0.03), (xb - 0.03, y0 + 0.03), (xb - 0.03, YF - 0.05)], z)
    # ------------------------------------------------------------ drum, tower, roof
    _drum(B)
    _tower(B)
    _roof(B)
    # ------------------------------------------------------------ ground floor rooms
    _ground_rooms(B)
    _depth_shop(B)
    _depth_laundry(B)
    _depth_roof(B)
    _depth2(B)
    # ------------------------------------------------------------ footprint, surrounds
    fp = [(-25.0, TY0), (X0, TY0), (X0, 0.4), (X1, 0.4)]
    ch = [DP(11.25 + 22.5 * j + 180 + 22.5, RD) for j in range(ND - 1)]
    fp += [(X1, DP(191.25, RD)[1])] + [DP(191.25 + 22.5 * j, RD) for j in range(1, ND)] + [(X1, YB), (X0, YB), (X0, TY1), (-25.0, TY1)]
    B.footprint = fp
    B.height = ZDT
    add_surrounds(B, bounds=(-24.5, 24.5, 0.3, 12.0), max_lamps=0, seed=5)
    _drop_stray_rack(B, (-11.5, -9.1, 3.5, 4.5))
    for x in (-4.0, 8.0):                                             # street lamps between the balcony bays
        _street_lamp(B, x, 1.0)
    B.note("48 flats (corridor type) in 6 bays x 4 floors, two stairs (drum and tower), shop and laundry below")


def _street_lamp(B, x, y):
    prism(B.m, "CT_Steel", ring(x, y, 0.07, 8), 0.0, 4.4, top=True, bottom=False)
    box(B.m, "CT_ConcreteDark", x - 0.18, x + 0.18, y - 0.18, y + 0.18, 0.0, 0.25)
    box(B.detail, STEEL, x - 0.05, x + 0.05, y - 0.05, y + 0.45, 4.4, 4.48)
    box(B.detail, STEEL, x - 0.28, x + 0.28, y + 0.25, y + 0.65, 4.48, 4.58)
    box(B.detail, "CT_Neon", x - 0.22, x + 0.22, y + 0.29, y + 0.61, 4.42, 4.48)
    B.lamp((x, y + 0.45, 4.3), lumens=1100, colour="#FFD9A8", kind="lobby", radius=10, fixture=False)


# ------------------------------------------------------------------ ground floor front
SHOP = (X0, X0 + 12.0)
LAUN = (X0 + 12.0, X0 + 18.0)
BIKE = (X0 + 18.0, X0 + 27.0)
RED = (X0 + 27.0, X1)


def _front_ground(B):
    u = lambda x: x - X0
    # shop: 2 big windows + door; laundry: 1 big window + door; red corner: 2 windows; bike: solid
    holes = [(u(-18.2), u(-15.0), 0.4, 2.5), (u(-14.5), u(-11.3), 0.4, 2.5), (u(-10.7), u(-9.3), 0.03, 2.4),
             (u(-6.3), u(-3.7), 0.4, 2.5), (u(-3.0), u(-1.7), 0.03, 2.4),
             (u(9.0), u(12.0), 0.9, 2.4), (u(12.8), u(15.8), 0.9, 2.4)]
    wall(B, (X0, YF), (X1, YF), 0.0, 2.8, "CT_PlasterGrey", holes=holes, depth=WT, mat_in="CT_Plaster")
    for (a, b, z0, z1) in holes:
        if z0 > 0.2 and z0 < 0.5 or z0 == 0.9:
            win(B, (X0, YF), (X1, YF), a, b, z0, z1, kind="public", cols=3 if b - a > 2.8 else 2, rows=2, inset=0.18, frame=0.06)
    B.entrance("shop", (X0 + u(-10.0), YF, 0.0), 0.0, width=1.4, height=2.4, kind="street", leaves=1)
    B.entrance("laundry", (X0 + u(-2.35), YF, 0.0), 0.0, width=1.3, height=2.4, kind="street", leaves=1)


def _washer(B, x, y, z=0.0, yaw=0.0, solid=True):
    """[wall] A front-loading laundry machine (white body, round porthole, red control strip); user stands at -y."""
    p = kit._P(B, x, y, z, yaw, "washer")
    p.kind = "wall"
    w, d, h = 0.62, 0.62, 0.95
    p.bev("CT_SteelWhite", -w / 2, w / 2, -d, 0.0, 0.0, h, 0.012)
    p.box("CT_SteelRed", -w / 2 + 0.04, w / 2 - 0.04, -d - 0.012, -d, 0.8, 0.9, f="y")
    p.box("CT_Steel", -0.06, 0.06, -d - 0.016, -d - 0.012, 0.84, 0.86, f="y")
    n = 10
    for rr, mat, dz in ((0.24, "CT_Aluminium", 0.012), (0.19, "CT_Steel", 0.02)):
        pts = [(rr * math.cos(2 * math.pi * k / n), -d - dz, 0.42 + rr * math.sin(2 * math.pi * k / n)) for k in range(n)]
        p.face(mat, pts)
    p.core(-w / 2 + 0.02, w / 2 - 0.02, -d + 0.02, -0.02, 0.1, h - 0.05)
    return p.done(solid=solid, top=h, front=0.8)


def _deck_chair(B, x, y, z=0.0, yaw=0.0, mat="CT_PlasterBlue", solid=False):
    """A folding timber deck chair with a canvas sling; the sitter faces yaw."""
    p = kit._P(B, x, y, z, yaw, "deck_chair")
    for s in (-0.3, 0.3):
        p.rod(kit.WOOD, (s, -0.35, 0.0), (s, 0.1, 0.36), 0.035, 4)
        p.rod(kit.WOOD, (s, 0.35, 0.0), (s, 0.0, 0.3), 0.035, 4)
        p.rod(kit.WOOD, (s, 0.1, 0.22), (s, 0.5, 0.78), 0.035, 4)
        p.rod(kit.WOOD, (s, -0.36, 0.3), (s, 0.1, 0.36), 0.03, 4)
    p.hexa(mat, [(-0.29, -0.36, 0.3), (0.29, -0.36, 0.3), (0.29, 0.1, 0.2), (-0.29, 0.1, 0.2)],
           [(-0.29, -0.36, 0.312), (0.29, -0.36, 0.312), (0.29, 0.1, 0.212), (-0.29, 0.1, 0.212)])
    p.hexa(mat, [(-0.29, 0.1, 0.2), (0.29, 0.1, 0.2), (0.29, 0.48, 0.74), (-0.29, 0.48, 0.74)],
           [(-0.29, 0.1, 0.212), (0.29, 0.1, 0.212), (0.29, 0.48, 0.752), (-0.29, 0.48, 0.752)])
    p.rod(kit.WOOD, (-0.31, 0.5, 0.78), (0.31, 0.5, 0.78), 0.03, 4)
    return p.done(solid=solid, seats=[(0.0, 0.0, 0.3, 0.0)])


def _drop_stray_rack(B, rect):
    """add_surrounds drops one bike rack inside the shop (it places by the shop door and misreads the free floor):
    cut its steel loops and base out of the detail mesh."""
    x0, x1, y0, y1 = rect
    keep = []
    for fc in B.detail.faces:
        mt, idx, uv = fc
        vs = [B.detail.verts[i] for i in idx]
        if mt in ("CT_Steel", "CT_ConcreteDark") and all(x0 <= v[0] <= x1 and y0 <= v[1] <= y1 and v[2] <= 0.85 for v in vs):
            continue
        keep.append(fc)
    B.detail.faces = keep


# the back wall's ten shop shelves, west to east: tins, tins, flour and biscuits, dairy, tins, jars, household, drinks, sweets, jars
SHELF_TRADES = ["grocery", "grocery", "bakery", "dairy", "grocery", "grocery", "hardware", "drinks", "bakery", "grocery"]


def _ground_rooms(B):
    m, det = B.m, B.detail
    f = Furn(B)
    T = B.thing
    # partitions between the ground rooms (front row YF+WT .. the corridor wall YC0)
    for x in (-7.0, -1.0, 8.0):
        box(m, "CT_Plaster", x - 0.1, x + 0.1, YF + WT, YC0, 0.0, 2.8)
    # ---- shop (x -18.7..-7.1, y 2.2..6.2): grocery with milk and bread
    YBK = YC0 - 0.01
    goods = ["groceries", "jars", "boxes", "bottles", "groceries", "jars", "boxes", "bottles", "boxes", "jars"]
    trades = SHELF_TRADES
    xs = [-18.2 + 1.05 * i for i in range(11)]
    for i, x in enumerate(xs):
        if x > -7.8:
            break
        kit.shop_shelf(B, x, YBK, 0.0, 0.0, w=1.0, goods=goods[i % len(goods)], h=2.0, levels=4, seed=i,
                       trade=trades[i % len(trades)])
    cr = kit.shop_counter(B, -14.0, 4.55, 0.0, 0.0, w=3.4, front="CT_PlasterBlue", top="CT_TileWhite")
    B.spot("shop_counter", "counter", (-14.0, 3.7, 0.0), 180.0)
    B.spot("shop_staff", "work", (-14.0, 5.3, 0.0), 0.0)
    T("shop_counter", "counter", (-14.0, 4.55, 1.05), 0.0, role="seller", shop="store_grocery", service="shop")
    kit.kitchen_scales(B, -15.0, 4.5, 1.05, 0)
    kit.plates(B, -13.2, 4.5, 1.05, 0, n=2)
    kit.bottles(B, -14.0, 4.6, 1.05, 0, n=3, seed=2)
    kit.sign_plate(B, -14.0, 4.55, 0.0, 0.0, name="sign_milk", w=1.5, zc=2.62, hang=True)
    for i, (x, g) in enumerate(((-17.0, "groceries"), (-16.0, "boxes"))):
        pass
    c1 = kit.display_case(B, -17.0, 3.0, 0.0, 0.0, w=1.3, d=0.55, goods="boxes", seed=1)
    c2 = kit.display_case(B, -11.8, 3.0, 0.0, 0.0, w=1.3, d=0.55, goods="groceries", seed=2)
    T("shop_case_west", "board", (-17.0, 3.0, 1.0), 0.0, topic="store")
    T("shop_case_east", "board", (-11.8, 3.0, 1.0), 0.0, topic="prices")
    for i, y in enumerate((2.75, 3.75, 4.75)):
        r = kit.shop_shelf(B, -18.7 + 0.005, y, 0.0, 90.0, w=0.95, goods=("boxes", "jars", "groceries")[i], h=1.9, levels=4, seed=7 + i,
                           trade=("bakery", "grocery", "grocery")[i])
        T("shop_west_%d" % (i + 1), "board", (-18.5, y, 1.3), 90.0, topic="store")
    for i, y in enumerate((2.75, 3.75, 4.75)):
        kit.shop_shelf(B, -7.1 - 0.005, y, 0.0, 270.0, w=0.95, goods=("bottles", "groceries", "boxes")[i], h=1.9, levels=4, seed=12 + i,
                       trade=("drinks", "grocery", "hardware")[i])
    T("shop_east_1", "board", (-7.3, 3.8, 1.3), 270.0, topic="store")
    kit.wall_clock(B, -16.0, YBK, 0.0, 0.0, zc=2.55, r=0.2)
    kit.plant(B, -8.0, 2.7, kind="ficus", pot="CT_PlasterRed", h=1.5)
    kit.crate(B, -9.0, 5.6, 0.0, 0.0, fill="CT_PlasterYellow", seed=1)
    kit.crate(B, -9.6, 5.7, 0.0, 15.0, fill="CT_PlasterRed", seed=2)
    kit.sack(B, -8.3, 5.5, 0.0, 30)
    B.spot("shop_stand1", "stand", (-15.0, 2.8, 0.0), 180.0)
    B.spot("shop_stand2", "stand", (-12.9, 2.7, 0.0), 180.0)
    for x in (-15.0, -10.5):
        f.pendant(x, 3.8, 2.8, lumens=700, colour="#FFE2B5", radius=6.0, drop=0.5)
    # ---- laundry (x -6.9..-1.1): machines at the back wall, a folding table, a bench, soap on a shelf
    for i in range(4):
        x = -6.2 + i * 1.15
        _washer(B, x, YBK, 0.0, 0.0)
        B.spot("wash%d" % (i + 1), "work", (x, 4.9, 0.0), 180.0)
        T("washer_%d" % (i + 1), "machine", (x, 5.7, 0.9), 0.0, what="washing_machine")
    tb = kit.table(B, -4.0, 3.2, 0.0, 0.0, w=2.0, d=0.9, kind="steel", mat="CT_Aluminium")
    kit.crate(B, -4.5, 3.2, 0.75, 0.0, w=0.6, d=0.42, h=0.22, fill="CT_TileWhite", seed=3)
    kit.crate(B, -3.5, 3.2, 0.75, 0.0, w=0.6, d=0.42, h=0.22, fill="CT_PlasterBlue", seed=4)
    T("laundry_folding_table", "craft", (-4.0, 3.2, 0.8), 0.0, what="workbench")
    bn = kit.bench(B, -1.45, 4.0, 0.0, 270.0, w=1.6, kind="park")
    T("laundry_bench", "seat", (-1.45, 4.0, 0.45), 270.0, height=0.45)
    ws = kit.wall_shelf(B, -6.9 + 0.005, 3.3, 0.0, 90.0, w=1.0, zt=1.6, props=False)
    kit.cardboard_box(B, -6.7, 3.0, 1.6, 90.0, w=0.3, d=0.22, h=0.22, seed=1)
    kit.cardboard_box(B, -6.7, 3.45, 1.6, 90.0, w=0.3, d=0.22, h=0.28, seed=2)
    kit.notice_board(B, -1.1 - 0.005, 5.0, 0.0, 270.0, w=0.9, zc=1.7, seed=4)
    T("laundry_rules", "board", (-1.3, 5.0, 1.7), 270.0, topic="rules")
    kit.wall_clock(B, -6.9 + 0.005, 4.7, 0.0, 90.0, zc=2.4, r=0.2)
    kit.plant(B, -1.5, 5.6, kind="cactus", pot="CT_PlasterYellow", h=0.9)
    f.pendant(-4.0, 3.8, 2.8, lumens=700, colour="#FFE2B5", radius=6.0, drop=0.5)
    # ---- bike room and red corner: bare rooms with a bench each
    f.bench(12.0, 4.0, yaw=0.0, w=1.8)
    B.spot("lobby_redcorner", "bench", (12.0, 4.0, 0.0), 180.0)
    f.panel(16.9, 4.0, 1.7, 2.4, 1.2, yaw=270.0, mat="CT_PlasterRed", frame="CT_Frame")
    for x in (3.0, 6.0):
        B.spot("bike_%d" % int(x), "stand", (x, 3.8, 0.0), 0.0)
    # dressing of the two bare rooms (x -0.9..7.9 bike room, x 8.1..16.7 red corner): one lamp each, bikes and a shelf
    # of tools in the first; a long table with chairs, banner, shelf and a plant in the second
    f.pendant(3.5, 4.4, 2.8, lumens=500, colour="#FFE2B5", radius=6.0, drop=0.5)
    for x_, m_ in ((0.45, "CT_SteelRed"), (1.35, "CT_PlasterBlue"), (4.7, "CT_PlasterYellow")):
        kit.bicycle(B, x_, 4.6, 0.0, 0.0, kind="stand", mat=m_)
    kit.shop_shelf(B, 2.9, YBK, 0.0, 0.0, w=1.0, goods="hardware", h=2.0, levels=4, seed=21, trade="hardware")
    kit.poster(B, -0.9 + 0.005, 3.2, 0.0, 90.0, name="poster_cons_1", w=0.6, zc=1.6)
    f.pendant(12.6, 4.4, 2.8, lumens=500, colour="#FFE2B5", radius=6.0, drop=0.5)
    kit.table(B, 14.6, 4.8, 0.0, 0.0, w=1.8, d=0.9)
    kit.around_table(B, 14.6, 4.8, 1.8, 0.9, 0.0, per_side=(2, 2, 0, 0), kind="wood")
    kit.banner(B, 12.6, YBK, 0.0, 0.0, name="poster_cons_3", w=1.4, zc=2.1)
    kit.bookshelf(B, 15.6, YBK, 0.0, 0.0, w=1.0, h=1.9, levels=5, topic="local", seed=23)
    kit.plant(B, 8.7, 5.7, kind="ficus", pot="CT_PlasterRed", h=1.4)
    # lamps in the ground corridor
    B.lamp((-9.0, YCC, 2.5), lumens=500, colour="#FFD9A8", kind="stair", radius=7.0)
    B.lamp((9.0, YCC, 2.5), lumens=500, colour="#FFD9A8", kind="stair", radius=7.0)
    for s in (2, 4):                                                  # every other corridor
        z = ZF[s]
        B.lamp((-1.0, YCC, z + 2.5), lumens=500, colour="#FFD9A8", kind="stair", radius=9.0)


# ------------------------------------------------------------------ the drum
def _drum(B):
    m, det = B.m, B.detail
    outer = drum_ring(RD)
    inner = drum_ring(2.75)
    zs = list(ZF) + [ZR]
    hface(m, "CT_Floor", outer[:-1], 0.0, True)
    prism(m, "CT_ConcreteDark", outer[:-1], -0.4, 0.0, top=False, bottom=False)
    for z in zs[1:]:
        band(m, "CT_Floor", outer, inner, z)
        band_down(m, "CT_Plaster", outer, inner, z - 0.3)
        ribbon_wall(m, "CT_Concrete", inner[::-1], z - 0.3, z)
    hface(m, "CT_Interior", outer[:-1], ZDC, False)
    hface(m, "CT_Roof", outer[:-1], ZDT, True)
    for k in range(ND):
        if k == 8:
            # the west facet: the bar's east wall below the roof; above the roof the facet itself, with the roof door
            p0, p1 = DP(22.5 * k - 11.25, RD), DP(22.5 * k + 11.25, RD)
            L = math.dist(p0, p1)
            wall(B, p0, p1, ZR - 0.3, ZDC, "CT_Plaster", holes=[(0.18, L - 0.18, ZR + 0.03, ZR + 2.05)], depth=WT,
                 mat_in="CT_Plaster")
            wall(B, p0, p1, ZDC, ZDT, "CT_PlasterRed", depth=WT, mat_in="CT_Concrete")
            continue
        p0, p1 = DP(22.5 * k - 11.25, RD), DP(22.5 * k + 11.25, RD)
        L = math.dist(p0, p1)
        for s in range(6):
            z = zs[s]
            z0 = 0.0 if s == 0 else z - 0.3
            ztop = z + 2.8 if s < 5 else ZDC
            if k == 12 and s == 0:
                wall(B, p0, p1, 0.0, 2.8, "CT_Plaster", holes=[(0.18, L - 0.18, 0.03, 2.5)], depth=WT, mat_in="CT_Plaster")
                continue
            glass0 = z + (0.45 if s == 0 else 0.4)
            glass1 = z + 2.75 if s < 5 else ZDC - 0.05
            wall(B, p0, p1, z0, glass0 - 0.05, "CT_Concrete", depth=WT, mat_in="CT_Plaster")
            wall(B, p0, p1, glass0 - 0.05, ztop, "CT_Plaster", holes=[(0.12, L - 0.12, glass0, glass1)], depth=WT,
                 mat_in="CT_Plaster")
            win(B, p0, p1, 0.12, L - 0.12, glass0, glass1, kind="stair", cols=2, rows=2, inset=0.12, frame=0.04)
        wall(B, p0, p1, ZDC, ZDT, "CT_PlasterRed", depth=WT, mat_in="CT_Concrete")
    # column + spiral stair
    B.column_round(XD, YD, 0.0, ZDC, 0.36, mat="CT_ConcreteWhite", segs=12)
    da = 9.0
    ri, ro = 0.40, 2.55
    for i in range(100):
        z = (i + 1) * RISE
        pa, pb = da * i, da * (i + 1)
        poly = [DP(pa, ri), DP(pa, ro), DP(pb, ro), DP(pb, ri)]
        prism(m, "CT_WoodLight", poly, z - 0.28, z, top=True, bottom=True, side_mat="CT_ConcreteDark")
    arrive = {19, 39, 59, 79, 99}
    for i in range(100):
        if any(a - 4 <= i <= a + 1 for a in arrive):
            continue
        z = (i + 1) * RISE
        x, y = DP(da * (i + 0.5), 2.5)
        box(det, "CT_Steel", x - 0.02, x + 0.02, y - 0.02, y + 0.02, z, z + 0.95)
        j = i + 1
        if j < 100 and not any(a - 4 <= j <= a + 1 for a in arrive):
            x2, y2 = DP(da * (j + 0.5), 2.5)
            rod(det, "CT_Steel", (x, y, z + 0.95), (x2, y2, (j + 1) * RISE + 0.95), 0.04)
    for fl in range(1, 6):
        z = zs[fl]
        arr = 180.0 if fl % 2 else 0.0
        for j in range(ND):
            a0, a1 = 11.25 + 22.5 * j, 33.75 + 22.5 * j
            mid = (a0 + a1) / 2
            d = (mid - arr + 180) % 360 - 180
            if -45 <= d <= 15:
                continue
            B.railing([DP(a0, 2.85), DP(a1, 2.85)], z, h=1.05, mat="CT_Steel", post=1.0)
    # canopy over the street door, red
    box(m, "CT_SteelRed", XD - 1.6, XD + 1.6, 1.5, YD - 3.92 + 0.0, 2.8, 3.0)
    B.entrance("main", (XD, YD - RD * math.cos(math.radians(11.25)), 0.0), 0.0, width=1.2, height=2.5, kind="street", leaves=2)
    # lobby: furniture and lamps
    for a, a_pos in ((292.5, 308.0), (337.5, 340.0)):         # ids keep the old angles; 292 moved off the door's clear zone
        x, y = DP(a_pos, 3.3)
        yaw = math.degrees(math.atan2(XD - x, -(YD - y))) % 360
        kit.bench(B, x, y, 0.0, yaw, w=1.5, kind="park")
        B.thing("lobby_bench_%d" % int(a), "seat", (x, y, 0.45), yaw, height=0.45)
        B.spot("lobby_bench_%d" % int(a), "bench", (x, y, 0.0), yaw)
    for a, kd, pc in ((22.5, "ficus", "CT_PlasterRed"), (67.5, "palm", "CT_TileWhite"), (112.5, "fern", "CT_PlasterYellow")):
        x, y = DP(a, 3.2)
        kit.plant(B, x, y, 0.0, 0.0, kind=kd, pot=pc, h=1.5)
    # the ground corridor next to the lobby: letter boxes, a house notice board, a clock
    fu.mailboxes(B, Fr(13.5, YC1 - 0.2, 0.0), 0.0, 0.9, cols=6, rows=4)
    B.thing("lobby_mailboxes", "board", (13.5, YC1 - 0.3, 1.4), 0.0, topic="rules")
    kit.notice_board(B, 11.0, YC0 + 0.2 + 0.005, 0.0, 180.0, w=1.2, zc=1.7, seed=3)
    B.thing("lobby_notices", "board", (11.0, YC0 + 0.4, 1.7), 180.0, topic="rules")
    kit.wall_clock(B, 8.8, YC0 + 0.2 + 0.005, 0.0, 180.0, zc=2.4, r=0.2)
    kit.fire_extinguisher(B, 15.2, YC0 + 0.2 + 0.005, 0.0, 180.0)
    kit.rug(B, 15.0, YCC, 0.0, 90.0, w=1.2, d=2.6, name="rug_5")
    band(det, "CT_PlasterRed", drum_ring(3.0), drum_ring(2.85), 0.012)
    for k_, a in enumerate((247.5, 22.5)):
        x, y = DP(a, 3.3)
        B.lamp((x, y, 2.5), lumens=700, colour="#FFD9A8", kind="lobby", radius=7.0)
    for fl in (1, 3, 5):                                              # one lamp per two landings
        x, y = DP(60.0 + 120 * (fl % 3), 3.3)
        B.lamp((x, y, min(zs[fl] + 2.5, 17.2)), lumens=500, colour="#FFD9A8", kind="stair", radius=7.0)
    B.spot("lobby_stand", "stand", (XD - 1.0, YD - 3.0, 0.0), 180.0)


# ------------------------------------------------------------------ the west stair tower
def _tower(B):
    m, det = B.m, B.detail
    TH = 17.9
    mat = "CT_PlasterYellow"
    zs = list(ZF) + [ZR]
    # south wall: slits and the street door next to the corridor end; west wall: tall glass; north wall: slits
    south_h = [(2.0, 3.2, z + 0.9, z + 2.5) for z in zs] + [(4.5, 5.7, 0.03, 2.4)]
    wall(B, (TX0, TY0), (TX1, TY0), 0.0, TH, mat, holes=south_h, depth=WT, mat_in="CT_Interior")
    for (a, b, c, d) in south_h[:-1]:
        win(B, (TX0, TY0), (TX1, TY0), a, b, c, d, kind="stair", cols=1, rows=2, inset=0.14, frame=0.04)
    wall(B, (TX0, TY1), (TX0, TY0), 0.0, TH, mat, holes=[(1.0, 3.8, 0.9, 16.4)], depth=WT, mat_in="CT_Interior")
    win(B, (TX0, TY1), (TX0, TY0), 1.0, 3.8, 0.9, 16.4, kind="stair", cols=2, rows=9, inset=0.14, frame=0.05)
    north_h = [(2.0, 3.2, z + 0.9, z + 2.5) for z in zs]
    wall(B, (TX1, TY1), (TX0, TY1), 0.0, TH, mat, holes=north_h, depth=WT, mat_in="CT_Interior")
    for (a, b, c, d) in north_h:
        win(B, (TX1, TY1), (TX0, TY1), a, b, c, d, kind="stair", cols=1, rows=2, inset=0.14, frame=0.04)
    # above the bar roof the tower's east wall (outside faces the roof) with the roof door
    wall(B, (X0, TY0), (X0, TY1), ZR, TH, mat, holes=[(YCC - 0.6 - TY0, YCC + 0.6 - TY0, ZR + 0.03, ZR + 2.1)],
         depth=WT, mat_in="CT_Interior")
    hface(m, "CT_Floor", rect(TX0 + WT, X0, TY0 + WT, TY1 - WT), 0.0, True)
    tr = rect(TX0 - 0.15, TX1 + 0.15, TY0 - 0.15, TY1 + 0.15)
    prism(m, "CT_Concrete", tr, TH - 0.4, TH, top=False, bottom=True)
    hface(m, "CT_Roof", tr, TH, True)
    hface(m, "CT_Interior", rect(TX0 + WT, X0, TY0 + WT, TY1 - WT), TH - 0.4, False)
    # U-stair along x: the landing (east) at the corridor door, flights west on the south lane and back on the north lane
    ix0, ix1, iy0, iy1 = TX0 + WT, X0, TY0 + WT, TY1 - WT
    xl = ix1 - 1.4
    going, n = 0.28, 10
    for s in range(5):
        zb = zs[s]
        for i in range(n):
            box(m, "CT_Concrete", xl - (i + 1) * going, xl - i * going, iy0, iy0 + 1.1, zb, zb + (i + 1) * RISE)
        xw = xl - n * going
        zh = zb + n * RISE
        box(m, "CT_Concrete", ix0, xw, iy0, iy1, zh - 0.3, zh)
        for i in range(n):
            box(m, "CT_Concrete", xw + i * going, xw + (i + 1) * going, iy1 - 1.1, iy1, zh, zh + (i + 1) * RISE)
        zn = zs[s + 1]
        box(m, "CT_Concrete", xl, ix1, iy0, iy1, zn - 0.3, zn)
        # a rail along the open edge of each lane
    B.entrance("stair_w", (-19.9, TY0, 0.0), 0.0, width=1.2, height=2.4, kind="street", leaves=1)
    for z in (0.0, 6.2, 12.4):
        B.lamp((-22.0, 7.2, z + 2.5), lumens=500, colour="#FFD9A8", kind="stair", radius=7.0)


# ------------------------------------------------------------------ roof
def _roof(B):
    m, det = B.m, B.detail
    # parapet round the bar roof (a gap at the drum roof door and the tower), grey coping
    pts = [(X0, YF), (X1, YF), (X1, YB), (X0, YB), (X0, YF)]
    for (a, b) in zip(pts, pts[1:]):
        if a == (X1, YF):
            # east edge: gap for the roof door of the drum between y 6.0 and 8.4
            segs = [((X1, YF), (X1, 6.0)), ((X1, 8.4), (X1, YB))]
        elif a == (X0, YB):
            segs = [((X0, YB), (X0, TY1)), ((X0, TY0), (X0, YF))]
        else:
            segs = [(a, b)]
        for p, q in segs:
            if math.dist(p, q) < 0.01:
                continue
            ox, oy = (q[1] - p[1]), -(q[0] - p[0])
            L = math.hypot(ox, oy)
            ox, oy = ox / L * 0.2, oy / L * 0.2
            quad_bar(m, "CT_Plaster", p, q, (p[0] - ox * 0, p[1] - oy * 0), (q[0] - ox * 0, q[1] - oy * 0), ZR, ZR) if False else None
            qa = (p[0] - ox, p[1] - oy)
            qb = (q[0] - ox, q[1] - oy)
            quad_bar(m, "CT_Plaster", p, q, qa, qb, ZR, ZR + 0.95)
            quad_bar(det, "CT_Concrete", p, q, (p[0] - ox * 1.3, p[1] - oy * 1.3), (q[0] - ox * 1.3, q[1] - oy * 1.3), ZR + 0.95, ZR + 1.02)
    # timber deck and the solarium pergola
    px0, px1, py0, py1 = -7.0, 12.0, 3.2, 11.2
    box(det, "CT_Wood", px0, px1, py0, py1, ZR, ZR + 0.04, faces="Z")
    for x in (-7.0, -2.0, 3.0, 8.0, 12.0):
        for y in (3.4, 11.0):
            box(det, "CT_Steel", x - 0.07, x + 0.07, y - 0.07, y + 0.07, ZR, ZR + 2.1)
    for y in (3.4, 11.0):
        box(det, "CT_Steel", px0 - 0.1, px1 + 0.1, y - 0.1, y + 0.1, ZR + 2.1, ZR + 2.22)
    for i in range(33):
        x = px0 + 0.1 + i * 0.6
        box(det, "CT_Wood", x - 0.05, x + 0.05, 3.3, 11.1, ZR + 2.22, ZR + 2.3)
    # deck chairs facing the sea, a table with a samovar and four chairs, benches under the north rail
    zd = ZR + 0.04
    for k, x in enumerate((-5.6, -4.4, 5.0, 6.2)):
        r = _deck_chair(B, x, 5.6, zd, 0.0, mat=("CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterRed", "CT_PlasterGreen")[k])
        B.thing("roof_deck_chair_%d" % (k + 1), "seat", (x, 5.6, zd + 0.3), 0.0, height=0.3)
    kit.table(B, 0.5, 7.2, zd, 0.0, w=1.6, d=0.9)
    kit.samovar(B, 0.2, 7.2, zd + 0.75, 0.0)
    kit.cup(B, 0.8, 7.0, zd + 0.75, 0.0)
    kit.plates(B, 0.9, 7.45, zd + 0.75, 0.0, n=2)
    for k, (x, y, yw) in enumerate(((-0.1, 6.3, 0.0), (1.1, 6.3, 0.0), (-0.1, 8.1, 180.0), (1.1, 8.1, 180.0))):
        kit.chair(B, x, y, zd, yw, mat="CT_PlasterRed" if k % 2 else "CT_PlasterYellow")
    B.thing("roof_table_seats", "seat", (0.5, 6.3, zd + 0.45), 0.0, height=0.45)
    for k, x in enumerate((-3.0, 4.0, 9.0)):
        kit.bench(B, x, 10.55, zd, 0.0, w=1.8, kind="park")
        B.thing("roof_bench_%d" % (k + 1), "seat", (x, 10.55, zd + 0.45), 0.0, height=0.45)
        B.spot("roof_bench_%d" % (k + 1), "bench", (x, 10.3, zd), 0.0)
    kit.plant(B, 10.6, 4.2, zd, 0.0, kind="ficus", pot="CT_PlasterRed", h=1.4)
    kit.plant(B, -6.2, 9.0, zd, 0.0, kind="palm", pot="CT_PlasterYellow", h=1.6)
    kit.radio(B, 1.5, 7.3, zd + 0.75, 20.0)
    B.thing("roof_view", "board", (5.0, 3.0, ZR + 1.3), 0.0, topic="view")
    B.thing("roof_solarium", "board", (-6.0, 7.0, zd + 1.3), 90.0, topic="sun")
    for x in (-8.5, 14.5):
        for y in (4.0, 10.5):
            box(m, "CT_Concrete", x - 0.5, x + 0.5, y - 0.5, y + 0.5, ZR, ZR + 0.5)
            box(m, "CT_Grass", x - 0.45, x + 0.45, y - 0.45, y + 0.45, ZR + 0.5, ZR + 0.54, faces="Z")
            box(det, "CT_Grass", x - 0.25, x + 0.25, y - 0.25, y + 0.25, ZR + 0.54, ZR + 1.0)
    # ventilation stacks
    for x in (-12.0, -9.0):
        box(m, "CT_Concrete", x - 0.3, x + 0.3, 10.2, 10.8, ZR, ZR + 1.4)


# ------------------------------------------------------------------ second pass: pieces drawn here
def _abacus(B, x, y, z=0.0, yaw=0.0):
    """[prop] A shopkeeper's abacus 0.36 x 0.24: timber frame on two feet, six wires, beads slid to both sides."""
    p = kit._P(B, x, y, z, yaw, "abacus")
    w, h = 0.36, 0.25
    for (a, b, c, d) in ((-w / 2, w / 2, 0.0, 0.02), (-w / 2, w / 2, h - 0.02, h)):
        p.box(kit.WOOD, a, b, -0.015, 0.015, c + 0.02 if c == 0.0 else c, d + (0.02 if c == 0.0 else 0.0))
    for sx in (-w / 2, w / 2 - 0.02):
        p.box(kit.WOOD, sx, sx + 0.02, -0.015, 0.015, 0.0, h)
    p.box(kit.WOOD, -w / 2 + 0.03, -w / 2 + 0.06, 0.015, 0.1, 0.0, 0.02)
    p.box(kit.WOOD, w / 2 - 0.06, w / 2 - 0.03, 0.015, 0.1, 0.0, 0.02)
    mats = ["CT_PlasterRed", "CT_PlasterYellow", "CT_Wood", "CT_PlasterRed", "CT_PlasterYellow", "CT_Wood"]
    cuts = (0.07, 0.11, 0.05, 0.13, 0.09, 0.06)
    for i in range(6):
        zz = 0.045 + i * 0.033
        p.box(kit.BRASS, -w / 2 + 0.02, w / 2 - 0.02, -0.003, 0.003, zz - 0.002, zz + 0.002)
        for k in range(4):
            xa = -w / 2 + 0.025 + k * 0.026
            p.box(mats[i], xa, xa + 0.022, -0.011, 0.011, zz - 0.012, zz + 0.012)
        for k in range(3):
            xb = w / 2 - 0.025 - 0.022 - k * 0.026
            p.box(mats[i], xb, xb + 0.022, -0.011, 0.011, zz - 0.012, zz + 0.012)
    return p.done()


def _floor_scale(B, x, y, z=0.0, yaw=0.0, solid=True):
    """A platform scale with a column and a round dial (customer steps on it; the dial faces -y)."""
    p = kit._P(B, x, y, z, yaw, "floor_scale")
    p.bev("CT_Steel", -0.25, 0.25, -0.3, 0.3, 0.0, 0.07, 0.01)
    p.bev("CT_Aluminium", -0.22, 0.22, -0.27, 0.2, 0.07, 0.1, 0.006)
    p.box("CT_Steel", -0.04, 0.04, 0.2, 0.28, 0.07, 1.0)
    p.box("CT_SteelRed", -0.15, 0.15, 0.12, 0.3, 1.0, 1.26)
    cn = kit.cell("clock", kit._rng(3, x, y, "scale"))
    p.card_front(cn, -0.12, 0.12, 1.02, 1.24, 0.118)
    p.box(kit.BRASS, -0.16, 0.16, 0.12, 0.3, 1.26, 1.28, f="xXyYZ")
    p.core(-0.22, 0.22, -0.27, 0.27, 0.01, 0.06)
    return p.done(solid=solid, top=0.1, front=0.7)


def _mangle(B, x, y, z=0.0, yaw=0.0, solid=True):
    """[floor] A laundry mangle: cast-iron cheeks, two timber rollers on a crank, a zinc tray; user at -y."""
    p = kit._P(B, x, y, z, yaw, "mangle")
    for sx in (-0.4, 0.34):
        p.box("CT_SteelRed", sx, sx + 0.06, -0.22, 0.22, 0.0, 0.98)
    p.box("CT_Steel", -0.4, 0.4, -0.22, -0.16, 0.0, 0.08)
    p.box("CT_Steel", -0.4, 0.4, 0.16, 0.22, 0.0, 0.08)
    p.box("CT_Steel", -0.4, 0.4, -0.02, 0.02, 0.3, 0.34)
    p.rod(kit.WOODL, (-0.36, 0.0, 0.82), (0.36, 0.0, 0.82), 0.16, 10, True)
    p.rod(kit.WOODL, (-0.36, 0.0, 0.67), (0.36, 0.0, 0.67), 0.16, 10, True)
    p.rod(kit.STEEL, (0.4, 0.0, 0.82), (0.55, 0.0, 0.82), 0.03, 5, True)
    p.rod(kit.STEEL, (0.55, 0.0, 0.82), (0.55, 0.0, 0.62), 0.025, 4, True)
    p.rod(kit.WOOD, (0.55, 0.0, 0.62), (0.55, -0.1, 0.62), 0.035, 5, True)
    p.box("CT_Aluminium", -0.34, 0.3, -0.5, -0.24, 0.6, 0.63, f="xXyYZz")
    p.box("CT_Aluminium", -0.34, 0.3, -0.5, -0.47, 0.6, 0.68)
    p.box("CT_Steel", -0.34, 0.34, -0.16, 0.16, 0.08, 0.5)
    p.core(-0.3, 0.3, -0.12, 0.12, 0.12, 0.46)
    return p.done(solid=solid, top=0.98, front=0.8, footprint=(-0.4, 0.62, -0.5, 0.22))


def _sock_line(B, pts, z):
    """A rope across the laundry (building coords) with pegs, a forgotten shirt and a handful of lone socks."""
    p = kit._P(B, 0.0, 0.0, 0.0, 0.0, "sock_line")
    for a, b in zip(pts, pts[1:]):
        p.rod(kit.STEEL, (a[0], a[1], z + a[2]), (b[0], b[1], z + b[2]), 0.012, 5)
    for (x, y, _) in (pts[0], pts[-1]):
        pass
    hang = [(0.35, "CT_PlasterRed", 0.22, 0.07), (0.9, "CT_PlasterBlue", 0.2, 0.07), (1.15, "CT_PlasterYellow", 0.18, 0.06),
            (2.2, "CT_PlasterGreen", 0.2, 0.07), (2.55, "CT_FrameWhite", 0.18, 0.07), (4.1, "CT_PlasterRed", 0.22, 0.07)]
    x0, y0 = pts[0][0], pts[0][1]
    x1 = pts[-1][0]
    for (u, mat, hh, ww) in hang:
        x = x0 + u
        if x > x1 - 0.1:
            continue
        zt = z - 0.012 + (0.0 if abs(x - (x0 + x1) / 2) > 1.0 else -0.05)
        zt -= 0.05 * (1.0 - abs(2 * (x - x0) / (x1 - x0) - 1.0) ** 2)
        p.box(kit.WOOD, x - 0.012, x + 0.012, y0 - 0.012, y0 + 0.012, zt - 0.005, zt + 0.07)
        p.box(mat, x - ww / 2, x + ww / 2, y0 - 0.02, y0 + 0.02, zt - hh, zt)
        p.box(mat, x - ww / 2 - 0.05, x + ww / 2, y0 - 0.02, y0 + 0.02, zt - hh, zt - hh + 0.07)
    xm = x0 + 2.9
    p.box(kit.WOOD, xm - 0.012, xm + 0.012, y0 - 0.012, y0 + 0.012, z - 0.07, z + 0.07)
    xs = x0 + 1.8
    zs = z - 0.07
    p.box("CT_PlasterBlue", xs - 0.25, xs + 0.25, y0 - 0.012, y0 + 0.012, zs - 0.5, zs)
    p.box("CT_PlasterBlue", xs - 0.42, xs - 0.25, y0 - 0.012, y0 + 0.012, zs - 0.18, zs - 0.02)
    p.box("CT_PlasterBlue", xs + 0.25, xs + 0.42, y0 - 0.012, y0 + 0.012, zs - 0.18, zs - 0.02)
    return p.done()


def _dovecote(B, x, y, z=0.0, yaw=0.0, solid=True):
    """A pigeon loft on legs: a timber box with three round holes and perches, a gable roof and a landing board, four birds."""
    p = kit._P(B, x, y, z, yaw, "dovecote")
    for sx in (-0.38, 0.32):
        for sy in (-0.28, 0.22):
            p.box(kit.WOOD, sx, sx + 0.06, sy, sy + 0.06, 0.0, 1.15)
    p.box("CT_WoodLight", -0.5, 0.5, -0.38, 0.38, 1.15, 1.2)
    p.box("CT_PlasterBlue", -0.46, 0.46, -0.34, 0.34, 1.2, 1.85, f="xXyYz")
    for hx in (-0.28, 0.0, 0.28):
        p.box("CT_ConcreteDark", hx - 0.06, hx + 0.06, -0.342, -0.338, 1.52, 1.64, f="y")
        p.rod(kit.WOOD, (hx, -0.34, 1.46), (hx, -0.5, 1.46), 0.018, 4)
    p.box(kit.WOODL, -0.5, 0.5, -0.56, -0.34, 1.16, 1.2)
    for sgn in (-1, 1):
        bot = [(sgn * 0.54, -0.44, 1.83), (0.0, -0.44, 2.2), (0.0, 0.44, 2.2), (sgn * 0.54, 0.44, 1.83)]
        top = [(a, b, c + 0.035) for (a, b, c) in bot]
        p.hexa("CT_SteelRed", bot, top)
    p.box(kit.BRASS, -0.02, 0.02, -0.46, 0.46, 2.2, 2.235)
    for (bx, by, bz, ba) in ((-0.3, -0.46, 1.2, 0), (0.1, -0.5, 1.2, 0), (0.24, -0.46, 1.2, 0), (0.0, -0.1, 2.235, 0)):
        p.box("CT_PlasterGrey", bx - 0.08, bx + 0.08, by - 0.045, by + 0.045, bz, bz + 0.1)
        p.box("CT_PlasterGrey", bx + 0.04, bx + 0.1, by - 0.075, by - 0.01, bz + 0.07, bz + 0.17)
        p.box("CT_ConcreteDark", bx - 0.1, bx - 0.07, by - 0.03, by + 0.03, bz + 0.03, bz + 0.07)
    p.box("CT_WoodLight", -0.42, 0.42, -0.3, 0.3, 0.0, 1.15)
    p.box(kit.WOOD, -0.3, 0.3, -0.302, -0.3, 0.1, 1.0, f="y")
    p.core(-0.38, 0.38, -0.26, 0.26, 0.05, 1.1)
    return p.done(solid=solid, top=1.2, front=0.9, footprint=(-0.5, 0.5, -0.4, 0.4))


def _lounger(B, x, y, z=0.0, yaw=0.0, mat="CT_PlasterBlue", mat2="CT_FrameWhite", solid=False):
    """A long sun lounger of timber and canvas: feet toward -y, the back rises toward +y; lie with the head at +y."""
    p = kit._P(B, x, y, z, yaw, "lounger")
    for s in (-0.3, 0.3):
        p.rod(kit.WOOD, (s, -0.95, 0.15), (s, 0.15, 0.3), 0.04, 4)
        p.rod(kit.WOOD, (s, 0.15, 0.3), (s, 0.95, 0.72), 0.04, 4)
        p.rod(kit.WOOD, (s, -0.8, 0.0), (s, -0.8, 0.2), 0.04, 4)
        p.rod(kit.WOOD, (s, 0.3, 0.0), (s, 0.3, 0.33), 0.04, 4)
    for k in range(4):
        y0 = -0.95 + k * 0.275
        z0 = 0.16 + (y0 + 0.95) * (0.14 / 1.1)
        z1 = 0.16 + (y0 + 0.275 + 0.95) * (0.14 / 1.1)
        m_ = mat if k % 2 == 0 else mat2
        bot = [(-0.28, y0, z0 + 0.012), (0.28, y0, z0 + 0.012), (0.28, y0 + 0.275, z1 + 0.012), (-0.28, y0 + 0.275, z1 + 0.012)]
        p.hexa(m_, bot, [(a, b, c + 0.012) for (a, b, c) in bot])
    for k in range(3):
        y0 = 0.15 + k * 0.267
        z0 = 0.31 + (y0 - 0.15) * (0.41 / 0.8)
        z1 = 0.31 + (y0 + 0.267 - 0.15) * (0.41 / 0.8)
        m_ = mat if k % 2 == 0 else mat2
        bot = [(-0.28, y0, z0 + 0.012), (0.28, y0, z0 + 0.012), (0.28, y0 + 0.267, z1 + 0.012), (-0.28, y0 + 0.267, z1 + 0.012)]
        p.hexa(m_, bot, [(a, b, c + 0.012) for (a, b, c) in bot])
    p.box("CT_FrameWhite", -0.2, 0.2, 0.62, 0.9, 0.62, 0.69)
    return p.done(solid=solid, top=0.32, seats=[(0.0, 0.2, 0.35, 0.0)], footprint=(-0.35, 0.35, -0.97, 0.97))


def _shower(B, x, y, z=0.0, yaw=0.0, solid=True):
    """A roof shower: duckboard, canvas screen on two sides, a sun-warmed tank on posts and a rose; step in from -y."""
    p = kit._P(B, x, y, z, yaw, "shower")
    p.box(kit.WOODL, -0.45, 0.45, -0.45, 0.45, 0.0, 0.05)
    for k in range(4):
        p.box(kit.WOOD, -0.45, 0.45, -0.45 + k * 0.23, -0.45 + k * 0.23 + 0.02, 0.05, 0.06, f="xXyYZ")
    for sx in (-0.45, 0.43):
        p.box(kit.STEEL, sx, sx + 0.03, 0.4, 0.43, 0.0, 2.2)
    p.box(kit.STEEL, -0.45, -0.42, -0.43, -0.4, 0.0, 2.2)
    p.box("CT_PlasterBlue", -0.43, 0.43, 0.4, 0.412, 0.5, 2.0)
    p.box("CT_PlasterBlue", -0.43, -0.415, -0.4, 0.4, 0.5, 2.0)
    p.cyl("CT_SteelRed", 0.0, 0.3, 2.2, 2.62, 0.24, 10, top=True, bottom=True)
    p.rod(kit.STEEL, (0.0, 0.3, 2.2), (0.0, 0.3, 1.95), 0.03, 5)
    p.rod(kit.STEEL, (0.0, 0.3, 1.95), (0.0, 0.05, 1.95), 0.03, 5)
    p.cyl(kit.ALU, 0.0, 0.0, 1.9, 1.95, 0.09, 8, r1=0.04, top=True, bottom=False)
    p.rod(kit.STEEL, (0.2, 0.3, 2.2), (0.2, 0.3, 1.2), 0.012, 4)
    p.box("CT_PlasterRed", 0.36, 0.42, 0.405, 0.42, 1.0, 1.9)
    return p.done(solid=False, top=0.06, front=0.8, footprint=(-0.45, 0.45, -0.45, 0.45))


def _tub_dressing(B, x, y, kind, zt):
    """Plants of one roof tub (the tub itself is in _roof) and its name stick; zt = the soil level."""
    p = kit._P(B, x, y, zt, 0.0, "tub_" + kind)
    if kind == "tomato":
        for sx, sy in ((-0.25, -0.2), (0.0, 0.25), (0.25, -0.15)):
            p.rod(kit.WOOD, (sx, sy, 0.0), (sx, sy, 1.35), 0.025, 4)
            for hz in (0.5, 0.8, 1.05):
                p.box("CT_PlasterRed" if hz > 0.6 else "CT_PlasterYellow", sx + 0.03, sx + 0.11, sy - 0.04, sy + 0.04, hz, hz + 0.08)
            p.box("CT_Leaf", sx - 0.14, sx + 0.14, sy - 0.02, sy + 0.02, 0.9, 1.0)
    elif kind == "dahlia":
        for i, (sx, sy) in enumerate(((-0.3, -0.2), (0.0, 0.1), (0.3, -0.1), (-0.1, 0.3), (0.25, 0.3))):
            hh = 0.6 + 0.12 * (i % 3)
            p.rod("CT_Leaf", (sx, sy, 0.0), (sx, sy, hh), 0.02, 4)
            p.cyl(("CT_PlasterPink", "CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterPink", "CT_PlasterRed")[i], sx, sy, hh - 0.04, hh + 0.04, 0.09, 7, r1=0.07)
    elif kind == "mint":
        for sx, sy in ((-0.3, -0.25), (0.1, -0.3), (0.3, 0.1), (-0.2, 0.2), (0.0, 0.0)):
            p.box("CT_Leaf", sx - 0.12, sx + 0.12, sy - 0.12, sy + 0.12, 0.0, 0.28)
        for sx, sy in ((-0.3, 0.3), (0.3, -0.3)):
            p.cyl("CT_PlasterYellow", sx, sy, 0.12, 0.2, 0.09, 6, r1=0.06)
            p.cyl("CT_PlasterYellow", sx, sy + 0.01, 0.0, 0.12, 0.012, 4)
    else:
        for sx, sy in ((-0.3, -0.2), (0.0, 0.3), (0.3, -0.1), (-0.2, 0.25)):
            p.box("CT_Leaf", sx - 0.015, sx + 0.015, sy - 0.015, sy + 0.015, 0.0, 0.85)
            p.box("CT_PlasterYellow", sx - 0.09, sx + 0.09, sy - 0.09, sy + 0.09, 0.82, 0.88)
        for sx, sy in ((0.1, -0.3), (-0.35, 0.05)):
            p.box("CT_Leaf", sx - 0.1, sx + 0.1, sy - 0.1, sy + 0.1, 0.0, 0.22)
    p.rod(kit.WOOD, (0.0, -0.46, -0.45), (0.0, -0.46, 0.3), 0.025, 4)
    p.box("CT_Paper", -0.13, 0.13, -0.475, -0.46, 0.15, 0.3)
    return p.done()


def _wash_line(B, x0, x1, y, z, sheets):
    """A roof drying line between two posts: pegs and hanging linen (sheets = [(centre x, width, drop, material)])."""
    p = kit._P(B, 0.0, 0.0, 0.0, 0.0, "wash_line")
    zt = z + 1.9
    for x in (x0, x1):
        p.box(kit.STEEL, x - 0.04, x + 0.04, y - 0.04, y + 0.04, z, zt + 0.1)
    p.rod(kit.STEEL, (x0, y, zt), (x1, y, zt), 0.014, 5)
    for cx, w, drop, mat in sheets:
        sag = 0.04 * (1 - ((cx - (x0 + x1) / 2) / ((x1 - x0) / 2)) ** 2)
        top = zt - sag
        p.box(mat, cx - w / 2, cx + w / 2, y - 0.012, y + 0.012, top - drop, top)
        for ex in (cx - w / 2 + 0.05, cx + w / 2 - 0.05):
            p.box(kit.WOOD, ex - 0.012, ex + 0.012, y - 0.02, y + 0.02, top - 0.04, top + 0.06)
    return p.done()


# ------------------------------------------------------------------ second pass: shop, laundry, roof
def _depth_shop(B):
    T = B.thing
    f = Furn(B)
    # floor scales on the customer side, left of the counter
    _floor_scale(B, -16.8, 4.55, 0.0, 0.0)
    T("shop_floor_scale", "machine", (-16.8, 4.55, 1.1), 0.0, what="weighing_scale")
    B.spot("shop_scale_stand", "stand", (-16.8, 3.9, 0.0), 180.0)
    # the shopkeeper's abacus and the radio on the counter
    _abacus(B, -14.5, 4.3, 1.05, 0.0)
    T("shop_abacus", "craft", (-14.5, 4.3, 1.17), 0.0, what="abacus")
    kit.radio(B, -12.65, 4.55, 1.05, 340.0)
    T("shop_radio", "machine", (-12.65, 4.55, 1.2), 0.0, what="radio")
    # bread rack right of the counter, goods of the day above the back shelves
    kit.bread_rack(B, -11.1, 5.3, 0.0, 0.0, w=0.9, seed=5)
    T("shop_bread_rack", "board", (-11.1, 5.3, 1.0), 0.0, topic="bread")
    B.spot("shop_bread_stand", "stand", (-11.1, 4.5, 0.0), 180.0)
    kit.menu_board(B, -10.0, YC0 - 0.01, 0.0, 0.0, name="price_1", w=1.2, zc=2.42, seed=3)
    T("shop_goods_day", "board", (-10.0, 6.1, 2.42), 0.0, topic="goods_day")
    # bottle-deposit corner: a table of empties and crates stacked to the east wall
    kit.table(B, -8.7, 4.35, 0.0, 90.0, w=0.9, d=0.5, kind="wood")
    for k, y_ in enumerate((4.05, 4.35, 4.65)):
        kit.bottles(B, -8.7, y_, 0.75, 0.0, n=3, seed=20 + k)
    for k in range(3):
        kit.crate(B, -9.6, 3.55 if k < 2 else 3.55, 0.3 * k, 90.0, w=0.55, d=0.38, h=0.3, fill="CT_PlasterGreen", seed=30 + k)
    kit.bottles(B, -9.6, 3.55, 0.9, 0.0, n=4, seed=35)
    T("shop_deposit", "machine", (-8.8, 4.35, 0.95), 90.0, what="deposit_window")
    B.spot("shop_deposit_stand", "stand", (-9.6, 4.35, 0.0), 270.0)
    B.lamp((-17.0, 4.2, 2.65), lumens=600, colour="#FFE2B5", kind="public", radius=6.0)


def _depth_laundry(B):
    T = B.thing
    f = Furn(B)
    # a forgotten-sock line over the folding table, under the front window
    _sock_line(B, [(-6.8, 2.6, 0.0), (-5.1, 2.6, -0.1), (-3.5, 2.6, 0.0)], 2.18)
    T("laundry_sock_line", "board", (-5.1, 2.6, 2.0), 0.0, topic="socks")
    # mangle at the west wall, magazines at the east wall
    _mangle(B, -6.5, 4.2, 0.0, 90.0)
    T("laundry_mangle", "craft", (-6.45, 4.2, 0.9), 90.0, what="mangle")
    B.spot("laundry_mangle_stand", "stand", (-5.6, 4.2, 0.0), 270.0)
    kit.magazine_rack(B, -1.105, 2.65, 0.0, 270.0, w=0.8, rows=4, seed=9)
    T("laundry_magazines", "shelf", (-1.3, 2.65, 1.0), 270.0, topic="laundry_mags")
    B.spot("laundry_read_stand", "stand", (-2.1, 2.7, 0.0), 270.0)
    # lost-property box = the upper cardboard box on the wall shelf; baskets by the machines
    T("laundry_lost_box", "board", (-6.7, 3.0, 1.75), 90.0, topic="lost_box")
    kit.crate(B, -5.6, 4.45, 0.0, 0.0, w=0.5, d=0.38, h=0.28, fill="CT_FrameWhite", seed=41)
    kit.crate(B, -3.3, 4.45, 0.0, 10.0, w=0.5, d=0.38, h=0.28, fill="CT_PlasterBlue", seed=42)
    kit.poster(B, -6.9 + 0.005, 5.3, 0.0, 90.0, name="poster_health_0" if "poster_health_0" in kit.ATLAS else "poster_health", w=0.5, zc=1.7)
    B.spot("laundry_fold", "stand", (-4.0, 2.45, 0.0), 180.0)
    B.spot("laundry_bench_sit", "bench", (-1.45, 4.0, 0.0), 270.0)
    f.pendant(-2.0, 4.6, 2.8, lumens=600, colour="#FFE2B5", radius=6.0, drop=0.5)


def _depth_roof(B):
    T = B.thing
    zd = ZR + 0.04
    # loungers
    for k, x in enumerate((-4.7, -3.5)):
        _lounger(B, x, 8.2, zd, 0.0, mat=("CT_PlasterBlue", "CT_PlasterRed")[k])
        T("roof_lounger_%d" % (k + 1), "bed", (x, 8.2, zd + 0.35), 0.0, height=0.35)
        B.spot("roof_lounger_%d" % (k + 1), "bench", (x, 8.0, zd), 0.0)
    # telescope on its tripod, aimed at the sea
    kit.telescope(B, 9.5, 4.0, zd, 0.0)
    T("roof_telescope", "machine", (9.5, 4.0, zd + 1.3), 0.0, what="roof_telescope")
    B.spot("roof_telescope_stand", "stand", (9.5, 3.4, zd), 180.0 if False else 0.0)
    # pigeon loft and a bird feeder on the east end of the roof, away from the drum's roof door
    _dovecote(B, 15.9, 9.7, ZR, 0.0)
    T("roof_dovecote", "machine", (15.9, 9.7, ZR + 1.5), 0.0, what="dovecote")
    B.spot("roof_pigeons", "stand", (15.9, 8.8, ZR), 180.0)
    kit.bird_feeder(B, 15.8, 5.2, ZR, 0.0, kind="post")
    T("roof_feeder", "board", (15.8, 5.2, ZR + 1.5), 0.0, topic="feeder")
    B.spot("roof_feeder_stand", "stand", (15.8, 4.5, ZR), 180.0)
    # named plant tubs: dressing on the four concrete tubs of _roof
    for k, ((x, y), kd) in enumerate((((-8.5, 4.0), "tomato"), ((-8.5, 10.5), "dahlia"), ((14.5, 4.0), "mint"), ((14.5, 10.5), "dill"))):
        _tub_dressing(B, x, y, kd, ZR + 0.54)
        T("roof_tub_%d" % (k + 1), "board", (x, y - 0.4, ZR + 0.9), 0.0, topic="tub")
    B.spot("roof_tub_work", "work", (-8.5, 3.2, ZR), 0.0)
    # rain-barrel shower west of the pergola
    _shower(B, -8.2, 8.7, ZR, 0.0)
    T("roof_shower", "machine", (-8.2, 8.5, ZR + 1.2), 0.0, what="roof_shower")
    B.spot("roof_shower_stand", "stand", (-8.2, 7.7, ZR), 180.0)
    # samovar on the table: tea in the evening
    T("roof_samovar", "machine", (0.2, 7.2, zd + 0.95), 0.0, what="samovar")
    # drying lines west of the pergola, either side of the tower's roof door path (y ~7.2)
    _wash_line(B, -17.0, -10.5, 4.6, ZR, [(-16.0, 1.3, 1.0, "CT_FrameWhite"), (-14.6, 1.0, 0.9, "CT_PlasterBlue"),
                                         (-13.3, 0.5, 0.6, "CT_PlasterYellow"), (-12.4, 1.3, 1.1, "CT_FrameWhite"),
                                         (-11.2, 0.6, 0.8, "CT_PlasterRed")])
    _wash_line(B, -17.0, -10.5, 9.3, ZR, [(-16.2, 1.0, 0.8, "CT_PlasterGreen"), (-14.9, 1.4, 1.1, "CT_FrameWhite"),
                                         (-13.5, 0.6, 0.7, "CT_PlasterBlue"), (-12.3, 1.0, 1.0, "CT_PlasterPink")])
    T("roof_washing_line", "board", (-13.7, 4.6, ZR + 1.3), 0.0, topic="washing_line")
    B.spot("roof_washing", "work", (-13.7, 3.9, ZR), 180.0)
    B.lamp((0.5, 7.2, ZR + 2.0), lumens=700, colour="#FFD9A8", kind="lobby", radius=8.0)
    box(B.detail, STEEL, -13.43, -13.37, 6.87, 6.93, ZR, ZR + 2.2)
    B.lamp((-13.4, 6.9, ZR + 2.3), lumens=500, colour="#FFD9A8", kind="lobby", radius=8.0)


# ------------------------------------------------------------------ third pass (round 5)
def _switch_plate(B, x, y, z=0.0, yaw=0.0, zc=1.3):
    """[wall] A two-button light switch plate on a backing block (0.1 x 0.14), buttons toward the room."""
    p = kit._P(B, x, y, z, yaw, "switch_plate")
    p.kind = "wall"
    p.box("CT_FrameWhite", -0.05, 0.05, -0.012, 0.0, zc - 0.07, zc + 0.07)
    p.box("CT_Steel", -0.022, -0.004, -0.02, -0.012, zc - 0.045, zc - 0.005, f="xXyYZ")
    p.box("CT_Steel", 0.004, 0.022, -0.02, -0.012, zc + 0.005, zc + 0.045, f="xXyYZ")
    return p.done(top=zc + 0.07)


def _ledger(B, x, y, z=0.0, yaw=0.0):
    """[prop] The shopkeeper's credit notebook: an oilcloth cover, a ribbon, a pencil on a string (0.2 x 0.28)."""
    p = kit._P(B, x, y, z, yaw, "ledger")
    p.kind = "prop"
    p.box("CT_PlasterBlue", -0.1, 0.1, -0.14, 0.14, 0.0, 0.012)
    p.box("CT_Paper", -0.092, 0.095, -0.132, 0.135, 0.012, 0.03)
    p.box("CT_PlasterBlue", -0.1, 0.1, -0.14, 0.14, 0.03, 0.04)
    p.box("CT_PlasterRed", 0.03, 0.045, -0.15, -0.13, 0.04, 0.043, f="Z")
    p.box("CT_Wood", 0.07, 0.085, -0.1, 0.12, 0.04, 0.052, f="xXyYZ")
    return p.done(top=0.05)


def _seedling_tray(B, x, y, z=0.0, yaw=0.0):
    """[prop] A timber seed tray (0.5 x 0.3) with six pots of seedlings in two rows and a name stick."""
    p = kit._P(B, x, y, z, yaw, "seedling_tray")
    p.kind = "prop"
    p.box("CT_Wood", -0.25, 0.25, -0.15, 0.15, 0.0, 0.02)
    for sx in (-0.25, 0.23):
        p.box("CT_Wood", sx, sx + 0.02, -0.15, 0.15, 0.02, 0.1)
    for sy in (-0.15, 0.13):
        p.box("CT_Wood", -0.25, 0.25, sy, sy + 0.02, 0.02, 0.1)
    for i in range(3):
        for j in range(2):
            cx, cy = -0.15 + i * 0.15, -0.065 + j * 0.13
            p.box("CT_PlasterRed", cx - 0.045, cx + 0.045, cy - 0.045, cy + 0.045, 0.02, 0.085)
            p.box("CT_Leaf", cx - 0.012, cx + 0.012, cy - 0.012, cy + 0.012, 0.085, 0.14 + 0.02 * ((i + j) % 3))
            p.box("CT_Leaf", cx - 0.05, cx + 0.05, cy - 0.01, cy + 0.01, 0.12 + 0.02 * ((i + j) % 3), 0.135 + 0.02 * ((i + j) % 3))
    p.rod(kit.WOOD, (0.2, -0.17, 0.0), (0.2, -0.17, 0.2), 0.012, 4)
    p.box("CT_Paper", 0.17, 0.23, -0.185, -0.17, 0.12, 0.2)
    return p.done(top=0.14)


def _watering_can(B, x, y, z=0.0, yaw=0.0):
    """[prop] A zinc watering can with a long spout and a rose (0.4 x 0.2 x 0.3), the spout toward -y."""
    p = kit._P(B, x, y, z, yaw, "watering_can")
    p.kind = "prop"
    p.cyl("CT_Aluminium", 0.0, 0.0, 0.0, 0.17, 0.085, 9, r1=0.075)
    p.rod("CT_Aluminium", (0.0, -0.06, 0.06), (0.0, -0.32, 0.24), 0.016, 5)
    p.cyl("CT_Aluminium", 0.0, -0.33, 0.235, 0.265, 0.04, 6, r1=0.032)
    p.rod("CT_Aluminium", (0.0, 0.07, 0.05), (0.0, 0.16, 0.12), 0.012, 4)
    p.rod("CT_Aluminium", (0.0, 0.16, 0.12), (0.0, 0.07, 0.19), 0.012, 4)
    return p.done(top=0.3)


def _weather_post(B, x, y, z=0.0, yaw=0.0):
    """A timber post 1.9 m with a tin vane, a thermometer board with a red column and a rain gauge made of a tin can
    on a bracket; the board faces -y."""
    p = kit._P(B, x, y, z, yaw, "weather_post")
    p.box(kit.WOOD, -0.04, 0.04, -0.04, 0.04, 0.0, 1.9)
    p.box(kit.WOODL, -0.09, 0.09, -0.07, -0.04, 1.0, 1.55)
    p.box("CT_FrameWhite", -0.012, 0.012, -0.075, -0.07, 1.06, 1.5, f="y")
    p.box("CT_PlasterRed", -0.006, 0.006, -0.08, -0.075, 1.06, 1.33, f="y")
    for k in range(7):
        p.box("CT_Steel", 0.02, 0.05, -0.078, -0.074, 1.1 + k * 0.06, 1.11 + k * 0.06, f="y")
    p.box(kit.WOOD, 0.04, 0.3, -0.03, 0.03, 0.78, 0.82)
    p.cyl("CT_Aluminium", 0.2, 0.0, 0.82, 1.0, 0.065, 8, r1=0.065, top=False)
    p.rod(kit.STEEL, (0.0, 0.0, 1.9), (0.0, 0.0, 2.05), 0.01, 4)
    p.box("CT_SteelRed", -0.25, 0.1, -0.006, 0.006, 1.99, 2.03)
    p.box("CT_SteelRed", -0.3, -0.25, -0.006, 0.006, 1.96, 2.06)
    return p.done(top=2.05, footprint=(-0.1, 0.3, -0.1, 0.1))


def _checkerboard(B, x, y, z=0.0, yaw=0.0):
    """[prop] A draughts board 0.4 x 0.4 with ten men a side lost into mid-game (z = the table top)."""
    p = kit._P(B, x, y, z, yaw, "checkerboard")
    p.kind = "prop"
    p.box(kit.WOOD, -0.22, 0.22, -0.22, 0.22, 0.0, 0.02)
    p.box("CT_PlasterYellow", -0.2, 0.2, -0.2, 0.2, 0.02, 0.026, f="Z")
    for i in range(8):
        for j in range(8):
            if (i + j) % 2:
                p.box("CT_ConcreteDark", -0.2 + i * 0.05, -0.15 + i * 0.05, -0.2 + j * 0.05, -0.15 + j * 0.05, 0.026, 0.028, f="Z")
    for k, (i, j, mat) in enumerate(((1, 0, "CT_FrameWhite"), (3, 0, "CT_FrameWhite"), (0, 1, "CT_FrameWhite"), (2, 1, "CT_FrameWhite"),
                                     (4, 3, "CT_FrameWhite"), (5, 4, "CT_ConcreteDark"), (3, 4, "CT_ConcreteDark"),
                                     (6, 5, "CT_ConcreteDark"), (1, 6, "CT_ConcreteDark"), (7, 6, "CT_ConcreteDark"))):
        p.cyl(mat, -0.175 + i * 0.05, -0.175 + j * 0.05, 0.028, 0.048, 0.02, 8)
    return p.done(top=0.05)


def _depth2(B):
    """Round 5: depth. Trade-correct shop shelving (labels by trade, see SHELF_TRADES) with four shelf things, the
    credit ledger on the counter, a soap shelf and the dryer's notice in the laundry, switches at the doors, a wall
    clock in the red corner with a draughts table and a local-history shelf there, the roof's seed table, peg basket,
    weather post, switch and the north view. Ids: shop_shelf_*, shop_ledger, shop_switch, laundry_soap, laundry_dryer_note,
    laundry_switch, redcorner_*, roof_seedlings, roof_peg_basket, roof_weather_post, roof_switch, roof_view_n."""
    T = B.thing
    zd = ZR + 0.04
    YBK = YC0 - 0.01
    # ---- shop: the four trade shelves as things (x of the shelf on the back wall, labels by trade), the ledger on the counter
    for nm, i in (("dairy", 3), ("bakery", 2), ("hardware", 6), ("drinks", 7)):
        T("shop_shelf_" + nm, "board", (-18.2 + 1.05 * i, YBK - 0.15, 1.3), 0.0, topic="store")
    _ledger(B, -13.6, 4.3, 1.05, 0.0)
    T("shop_ledger", "board", (-13.6, 4.3, 1.12), 0.0, topic="store")
    _switch_plate(B, -10.95, YF + WT, 0.0, 180.0, zc=1.3)
    T("shop_switch", "machine", (-10.95, YF + WT + 0.08, 1.3), 180.0, what="switch", switch=6.8)
    # ---- laundry: soap and powder on a low hardware shelf, the dryer's notice above it, a switch by the door
    kit.shop_shelf(B, -2.0, YBK, 0.0, 0.0, w=0.8, goods="boxes", h=1.6, levels=3, seed=51, trade="hardware")
    T("laundry_soap", "board", (-2.0, YBK - 0.2, 1.1), 0.0, topic="store")
    kit.poster(B, -2.0, YBK, 0.0, 0.0, name="paper_typed", w=0.4, zc=2.05)
    T("laundry_dryer_note", "board", (-2.0, YBK - 0.1, 2.05), 0.0, topic="rules")
    _switch_plate(B, -1.6, YF + WT, 0.0, 180.0, zc=1.3)
    T("laundry_switch", "machine", (-1.6, YF + WT + 0.08, 1.3), 180.0, what="switch", switch=4.5)
    # ---- red corner: a draughts table, a seat each side, the local-history shelf
    _checkerboard(B, 14.6, 4.8, 0.75, 0.0)
    T("redcorner_checkers", "play", (14.6, 4.8, 0.8), 0.0, what="checkers")
    for k, (cx, cy, fy) in enumerate(((14.15, 4.08, 180.0), (15.05, 4.08, 180.0), (14.15, 5.52, 0.0), (15.05, 5.52, 0.0))):
        T("redcorner_seat_%d" % (k + 1), "seat", (cx, cy, 0.45), fy, height=0.45)
    B.spot("redcorner_seat_1", "seat", (14.15, 4.08, 0.0), 180.0)
    B.spot("redcorner_seat_3", "seat", (14.15, 5.52, 0.0), 0.0)
    T("redcorner_shelf", "shelf", (15.6, YBK - 0.2, 1.3), 0.0, topic="local")
    # ---- roof: the seed table between the benches, the peg basket by the first drying line, the weather post, the switch
    kit.table(B, 6.5, 9.3, zd, 0.0, w=1.4, d=0.6, kind="wood")
    _seedling_tray(B, 6.2, 9.3, zd + 0.75, 0.0)
    _seedling_tray(B, 6.85, 9.3, zd + 0.75, 0.0)
    _watering_can(B, 7.15, 9.45, zd + 0.75, 200.0)
    T("roof_seedlings", "craft", (6.5, 9.3, zd + 0.95), 0.0, what="seedlings")
    B.spot("roof_seedlings", "work", (6.5, 8.6, zd), 180.0)
    kit.crate(B, -11.4, 5.3, ZR, 0.0, w=0.5, d=0.38, h=0.3, fill="CT_FrameWhite", seed=61)
    for k in range(6):
        box(B.detail, "CT_Wood", -11.6 + k * 0.07, -11.56 + k * 0.07, 5.2, 5.28, ZR + 0.3, ZR + 0.4)
    T("roof_peg_basket", "craft", (-11.4, 5.3, ZR + 0.45), 0.0, what="pegs")
    B.spot("roof_peg_stand", "work", (-11.4, 4.75, ZR), 180.0)
    _weather_post(B, 12.8, 9.0, ZR, 0.0)
    T("roof_weather_post", "board", (12.8, 9.0, ZR + 1.3), 0.0, topic="sun")
    B.spot("roof_weather_stand", "stand", (12.8, 8.3, ZR), 180.0)
    _switch_plate(B, 3.0, 3.4 - 0.07, ZR, 0.0, zc=1.3)
    T("roof_switch", "machine", (3.0, 3.3, ZR + 1.3), 0.0, what="switch", switch=8)
    T("roof_view_n", "board", (2.0, 12.1, ZR + 1.3), 180.0, topic="view")
    B.spot("roof_view_n_stand", "stand", (2.0, 11.6, ZR), 180.0)
    # ---- the red corner's wall clock last: new kit clocks take the next kit_clock_<n>
    kit.wall_clock(B, 14.2, YBK, 0.0, 0.0, zc=2.3, r=0.2)
