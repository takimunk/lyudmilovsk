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
Ideas for later: interiors of the flats, a glazed solarium cabin on the roof, a cinema poster board, shutters.
"""
import math

from citykit import box, prism, ring, band, band_down, ribbon_wall, quad_bar
from _cons_util import wall, win, hface, rect
from _surrounds import add_surrounds
from furn_int2 import Furn
from furn_int3 import rod
import geom

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
    # ------------------------------------------------------------ footprint, surrounds
    fp = [(-25.0, TY0), (X0, TY0), (X0, 0.4), (X1, 0.4)]
    ch = [DP(11.25 + 22.5 * j + 180 + 22.5, RD) for j in range(ND - 1)]
    fp += [(X1, DP(191.25, RD)[1])] + [DP(191.25 + 22.5 * j, RD) for j in range(1, ND)] + [(X1, YB), (X0, YB), (X0, TY1), (-25.0, TY1)]
    B.footprint = fp
    B.height = ZDT
    add_surrounds(B, bounds=(-24.5, 24.5, 0.3, 12.0), max_lamps=0, seed=5)
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


def _ground_rooms(B):
    m, det = B.m, B.detail
    f = Furn(B)
    # partitions between the ground rooms (front row y YF+.3..YC0)
    for x in (-7.0, -1.0, 8.0):
        box(m, "CT_Plaster", x - 0.1, x + 0.1, YF + WT, YC0 - 0.2, 0.0, 2.8)
    # ---- shop (x -19..-7)
    box(m, "CT_WoodLight", -17.4, -11.2, 4.3, 5.0, 0.0, 1.05)
    box(m, "CT_Steel", -17.45, -11.15, 4.25, 5.05, 1.05, 1.09)
    B.spot("shop_counter", "counter", (-14.3, 3.7, 0.0), 180.0)
    B.spot("shop_staff", "work", (-14.3, 5.5, 0.0), 0.0)
    for i, x in enumerate((-17.6, -16.0, -14.4, -12.8, -11.2)):
        f.shelf(x, 5.88, w=1.5, d=0.4, h=2.0, yaw=0.0, rows=4, seed=i)
    for i, y in enumerate((3.2, 4.6)):
        f.shelf(-18.5, y, w=1.2, d=0.4, h=1.9, yaw=270.0, rows=4, seed=7 + i)
    f.plant(-8.1, 2.8, h=1.5)
    B.spot("shop_stand1", "stand", (-15.0, 2.8, 0.0), 180.0)
    B.spot("shop_stand2", "stand", (-12.0, 2.8, 0.0), 180.0)
    for x in (-15.0, -10.5):
        f.pendant(x, 3.8, 2.8, lumens=700, colour="#FFE2B5", radius=6.0, drop=0.5)
    # ---- laundry (x -7..-1)
    for i in range(4):
        x = -6.3 + i * 1.15
        box(m, "CT_Aluminium", x - 0.45, x + 0.45, 5.2, 6.0, 0.0, 0.95)
        box(det, "CT_Steel", x - 0.3, x + 0.3, 5.18, 5.2, 0.3, 0.75, faces="y")
        box(det, "CT_SteelRed", x - 0.4, x + 0.4, 5.17, 5.2, 0.8, 0.9, faces="y")
        B.spot("wash%d" % (i + 1), "work", (x, 4.6, 0.0), 180.0)
    f.table(-4.0, 2.9, w=2.0, d=0.9, h=0.9, top="CT_Aluminium")
    f.bench(-1.9, 3.8, yaw=90.0, w=1.4)
    f.pendant(-4.0, 3.8, 2.8, lumens=700, colour="#FFE2B5", radius=6.0, drop=0.5)
    # ---- bike room and red corner: bare rooms with a bench each
    f.bench(12.0, 4.0, yaw=0.0, w=1.8)
    B.spot("redcorner_bench", "bench", (12.0, 4.0, 0.0), 180.0)
    f.panel(16.9, 4.0, 1.7, 2.4, 1.2, yaw=270.0, mat="CT_PlasterRed", frame="CT_Frame")
    for x in (3.0, 6.0):
        for k in range(4):
            box(det, "CT_Steel", x - 0.9 + k * 0.5 - 0.02, x - 0.9 + k * 0.5 + 0.02, 3.2, 5.2, 0.0, 0.8) if False else None
        B.spot("bike_%d" % int(x), "stand", (x, 3.8, 0.0), 0.0)
    # lamps in the ground corridor
    B.lamp((-9.0, YCC, 2.5), lumens=500, colour="#FFD9A8", kind="stair", radius=7.0)
    B.lamp((0.0, YCC, 2.5), lumens=500, colour="#FFD9A8", kind="stair", radius=7.0)
    B.lamp((9.0, YCC, 2.5), lumens=500, colour="#FFD9A8", kind="stair", radius=7.0)
    for s in range(1, 5):
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
    f = Furn(B)
    for a in (292.5, 337.5):
        x, y = DP(a, 3.3)
        f.bench(x, y, yaw=math.degrees(math.atan2(XD - x, -(YD - y))) % 360, w=1.5, back=True)
        B.spot("lobby_bench_%d" % int(a), "bench", (x, y, 0.0), math.degrees(math.atan2(XD - x, -(YD - y))) % 360)
    for a in (22.5, 67.5, 112.5):
        x, y = DP(a, 3.2)
        f.plant(x, y, h=1.5, pot="CT_PlasterRed" if a != 67.5 else "CT_TileWhite")
    band(det, "CT_PlasterRed", drum_ring(3.0), drum_ring(2.85), 0.012)
    for k_, a in enumerate((247.5, 22.5)):
        x, y = DP(a, 3.3)
        B.lamp((x, y, 2.5), lumens=700, colour="#FFD9A8", kind="lobby", radius=7.0)
    for fl in range(1, 6):
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
        B.railing([(xl, iy0 + 1.15), (xw, iy0 + 1.15)], zh - 0.3 + 0.0, h=0.0001) if False else None
    B.entrance("stair_w", (-19.9, TY0, 0.0), 0.0, width=1.2, height=2.4, kind="street", leaves=1)
    for z in (0.0, 3.1, 6.2, 9.3, 12.4):
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
    # loungers, a table, planters
    f = Furn(B, z0=ZR)
    for x in (-5.5, -3.5, 5.0, 7.0):
        box(det, "CT_Wood", x - 0.35, x + 0.35, 4.6, 6.4, ZR + 0.04, ZR + 0.3)
        box(det, "CT_PlasterBlue", x - 0.3, x + 0.3, 4.65, 6.0, ZR + 0.3, ZR + 0.36)
    f.table(0.5, 7.2, w=1.6, d=0.9, h=0.75)
    for x, y in ((-0.6, 6.3), (1.6, 6.3), (-0.6, 8.1), (1.6, 8.1)):
        f.chair(x, y, yaw=0.0 if y > 7.2 else 180.0)
    for x in (-8.5, 14.5):
        for y in (4.0, 10.5):
            box(m, "CT_Concrete", x - 0.5, x + 0.5, y - 0.5, y + 0.5, ZR, ZR + 0.5)
            box(m, "CT_Grass", x - 0.45, x + 0.45, y - 0.45, y + 0.45, ZR + 0.5, ZR + 0.54, faces="Z")
            box(det, "CT_Grass", x - 0.25, x + 0.25, y - 0.25, y + 0.25, ZR + 0.54, ZR + 1.0)
    # ventilation stacks
    for x in (-12.0, -9.0):
        box(m, "CT_Concrete", x - 0.3, x + 0.3, 10.2, 10.8, ZR, ZR + 1.4)
