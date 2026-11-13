"""Unite Block ("Жилая единица"): a brutalist housing unit. An ORIGINAL design in the spirit of the early-1950s
French "unite d'habitation" (a village in a slab on pilotis, loggias with coloured side walls, a shopping street at
mid-height, a roof terrace with sculptural concrete forms); proportions, bay system, cores and roof are our own.

Plot 130 m x 22 m, height 55.6 m (stack top). Slab: 115.2 m long (32 bays of 3.6 m), body y 2..20 (18 m deep), raised
on 6 m high tapering board-marked pilotis (two rows of 7 piers, octagonal, narrow at the foot, flared at the head).
  * Storeys: 15 x 3.0 m above the 6 m pilotis zone, roof slab at z 51..51.5.
  * Front (sea/street side): every bay is a deep LOGGIA (2.4 m recess, y 2..4.4) two storeys high, with side panels in
    red / yellow / blue / green (CT_PlasterRed/Yellow/Blue, CT_Copper) and an occasional blue tile; a balcony
    gallery at the mid level of each duplex with steel railings. Window card per storey behind it.
  * Flats: 6 duplex layers (storeys 1-2, 3-4, 5-6, 9-10, 11-12, 13-14: one flat spans two storeys, windows on both,
    front and back) + one single-storey layer at storey 15, 28 flat-bays per layer = 196 flats.
  * Storeys 7-8 (z 24..30): the "shopping street", a recessed glazed band of kind "public" windows with a red
    steel balustrade along its whole length.
  * Two lift/stair cores (red-plaster and yellow-plaster fronts, narrow stair slits) run from the ground to the roof;
    below each a lobby (hollow, glazed front, street door, bench spot) stands under the slab.
  * Back: raw concrete with a window per storey per bay and horizontal ledges at the duplex levels.
  * Roof (z 51.5): parapet with railing, a stadium-shaped red running track with a lawn, a kindergarten box (yellow,
    windows, fenced yard), a tapered ventilation stack, stepped machine rooms over the cores and a concrete pergola.
Enterable: the two lobbies (and the open pilotis space). Spots: lobby benches, a roof viewpoint.
Ideas for later: enterable shopping street, rooftop swimming pool, murals on the gables, actual brise-soleil tiles.
Loggia panels use CT_PlasterGreen for green.
"""
import math
import random

import furn_int3 as fu
import geom
import interior_kit as kit
from citykit import box, prism, band, ribbon_wall, DOOR_W, DOOR_H

INFO = {
    "name_ru": "Жилая единица (бруталистский дом на опорах)", "name_en": "Unite Block",
    "style": "brutalism", "use": "residential",
    "inspired_by": "early-1950s Marseille housing unit (Le Corbusier): pilotis, loggias with coloured sides, "
                   "shopping street at mid-height, roof terrace with sculpted forms; original proportions and bay system",
}

NB, BW = 32, 3.6
X0 = -NB * BW / 2                 # -57.6
Y_F, Y_L, Y_B = 2.0, 4.4, 20.0     # front of the loggia, the loggia back wall, back face
Z_P = 6.0                          # pilotis height
ST = 3.0
NST = 15
Z_ROOF = Z_P + ST * NST            # 51.0
ROOF_T = 0.5
CORES = [(8, 10, "lobby1", "CT_PlasterRed"), (22, 24, "lobby2", "CT_PlasterYellow")]
PIERS = [2, 6, 12, 16, 20, 26, 30]
PAIRS = [(1, 2), (3, 4), (5, 6), (7, 8), (9, 10), (11, 12), (13, 14), (15, 15)]
SHOP = (7, 8)
COLORS = ["CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterBlue", "CT_PlasterGreen", "CT_PlasterRed", "CT_PlasterYellow",
          "CT_PlasterBlue", "CT_PlasterGreen", "CT_Tile"]


def xb(k):
    return X0 + BW * k


def zf(s):
    return Z_P + ST * (s - 1)


def core_of(i):
    for a, b, *_ in CORES:
        if a <= i < b:
            return True
    return False


def fwin(B, F, u0, u1, z0, z1, **kw):
    pts = [F.to_building((u, 0.0, z)) for u, z in ((u0, z0), (u1, z0), (u1, z1), (u0, z1))]
    kw.setdefault("inset", 0.2)
    return B.window(pts, **kw)


def octo(cx, cy, rx, ry, c):
    return [(cx + rx - c, cy - ry), (cx + rx, cy - ry + c), (cx + rx, cy + ry - c), (cx + rx - c, cy + ry),
            (cx - rx + c, cy + ry), (cx - rx, cy + ry - c), (cx - rx, cy - ry + c), (cx - rx + c, cy - ry)]


def loft(m, mat, pa, za, pb, zb, caps=True):
    n = len(pa)
    for i in range(n):
        j = (i + 1) % n
        m.face(mat, [(pa[i][0], pa[i][1], za), (pa[j][0], pa[j][1], za), (pb[j][0], pb[j][1], zb), (pb[i][0], pb[i][1], zb)])
    if caps:
        m.face(mat, [(x, y, za) for x, y in reversed(pa)])
        m.face(mat, [(x, y, zb) for x, y in pb])


def stadium(cx, cy, half, r, n=14, grow=0.0):
    r = r + grow
    pts = []
    for k in range(n + 1):
        a = -math.pi / 2 + math.pi * k / n
        pts.append((cx + half + r * math.cos(a), cy + r * math.sin(a)))
    for k in range(n + 1):
        a = math.pi / 2 + math.pi * k / n
        pts.append((cx - half + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def build(B):
    rnd = random.Random(1952)
    XE = -X0
    zr = Z_ROOF + ROOF_T
    ranges = [(X0, xb(8)), (xb(10), xb(22)), (xb(24), XE)]
    # ---------------------------------------------------------------- ground, belly, roof slab
    prism(B.m, "CT_Paving", [(X0 - 1, 0.0), (XE + 1, 0.0), (XE + 1, Y_B), (X0 - 1, Y_B)], -0.4, 0.0, top=True, bottom=True, side_mat="CT_ConcreteDark")
    outline = [(X0, Y_F), (XE, Y_F), (XE, Y_B), (X0, Y_B)]
    prism(B.m, "CT_ConcreteDark", outline, Z_P - 0.9, Z_P - 0.5, top=True, bottom=True, side_mat="CT_Concrete")
    prism(B.m, "CT_Roof", outline, Z_ROOF, zr, top=True, bottom=True, side_mat="CT_Concrete")

    # ---------------------------------------------------------------- pilotis
    for k in PIERS:
        for cy in (6.5, 16.5):
            cx = xb(k)
            foot = octo(cx, cy, 0.5, 1.1, 0.28)
            mid = octo(cx, cy, 0.95, 1.8, 0.45)
            head = octo(cx, cy, 1.15, 2.1, 0.55)
            loft(B.m, "CT_Concrete", foot, 0.0, mid, Z_P - 1.7, caps=False)
            B.m.face("CT_Concrete", [(x, y, 0.0) for x, y in reversed(foot)])
            loft(B.m, "CT_Concrete", mid, Z_P - 1.7, head, Z_P - 0.9, caps=False)
            B.m.face("CT_ConcreteDark", [(x, y, Z_P - 0.9) for x, y in reversed(head)])

    # ---------------------------------------------------------------- loggia frame: beams, panels, balconies
    layers = [(p, (zf(p[0]), zf(p[0]) + ST * (p[1] - p[0] + 1))) for p in PAIRS]
    for (a, b), (z0, z1) in layers:
        for xa, xz in ranges:
            box(B.m, "CT_Concrete", xa, xz, Y_F, Y_L + 0.2, z0 - 0.5, z0)
    for xa, xz in ranges:
        box(B.m, "CT_Concrete", xa, xz, Y_F, Y_L + 0.2, Z_ROOF - 0.5, Z_ROOF)
    prev = None
    flats = {}                          # (storey, bay) -> fid
    nflat = {}
    for (a, b), (z0, z1) in layers:
        shop = (a, b) == SHOP
        for i in range(NB):
            if core_of(i):
                continue
            xa, xz = xb(i), xb(i + 1)
            col = "CT_Plaster" if shop else rnd.choice(COLORS)
            while not shop and prev is not None and col == prev:
                col = rnd.choice(COLORS)
            prev = col
            h = z1 - z0
            box(B.m, col, xa, xa + 0.2, Y_F, Y_L, z0, z1)
            box(B.m, col, xz - 0.2, xz, Y_F, Y_L, z0, z1)
            # loggia back wall with its windows
            L = BW
            holes, wins = [], []
            if shop:
                for lvl in (a, b):
                    zz = zf(lvl)
                    holes.append((0.25, L - 0.25, zz + 0.4, zz + 2.6))
                    wins.append((0.25, L - 0.25, zz + 0.4, zz + 2.6, lvl))
            else:
                for lvl in range(a, b + 1):
                    zz = zf(lvl)
                    holes.append((0.5, L - 0.5, zz + 0.5, zz + 2.5))
                    wins.append((0.5, L - 0.5, zz + 0.5, zz + 2.5, lvl))
            F = B.facade((xa, Y_L), (xz, Y_L), z0, z1, "CT_Concrete", holes=holes, depth=0.3)
            fid = None
            if not shop:
                nflat[a] = nflat.get(a, 0) + 1
                fid = "%d-%02d" % (a, nflat[a])
                for lvl in range(a, b + 1):
                    flats[(lvl, i)] = fid
            for u0, u1, w0, w1, lvl in wins:
                fwin(B, F, u0, u1, w0, w1, flat=fid, kind="public" if shop else "flat", cols=2, rows=1, frame=0.05)
            if not shop:
                # balcony slab and rails at the upper level of the duplex; rail at the lower level
                for lvl in range(a, b + 1):
                    zz = zf(lvl)
                    if lvl > a:
                        box(B.m, "CT_Concrete", xa + 0.2, xz - 0.2, Y_F, Y_F + 1.4, zz - 0.2, zz)
                    B.railing([(xa + 0.2, Y_F + 0.06), (xz - 0.2, Y_F + 0.06)], zz, h=1.05, post=1.6, bar=0.04)
    # shopping street balustrade (red) along the whole gallery
    zs = zf(SHOP[0])
    for xa, xz in ranges:
        B.railing([(xa + 0.2, Y_F + 0.06), (xz - 0.2, Y_F + 0.06)], zs, h=1.1, mat="CT_SteelRed", post=1.8)

    # ---------------------------------------------------------------- cores and lobbies
    for k, (a, b, lid, cmat) in enumerate(CORES):
        xa, xz = xb(a), xb(b)
        xm = (xa + xz) / 2
        box(B.m, "CT_Concrete", xa, xz, 9.0, Y_B, 0.0, Z_ROOF)                              # shaft mass down to the ground
        box(B.m, "CT_Concrete", xa, xz, Y_F, 9.0, 3.9, Z_ROOF, faces="xXZz")
        holes = [(BW - 0.35, BW + 0.35, zf(s) + 0.8, zf(s) + 2.3) for s in range(1, NST + 1)]
        F = B.facade((xa, Y_F), (xz, Y_F), Z_P - 0.5, Z_ROOF, cmat, holes=holes, depth=0.3)
        for s in range(1, NST + 1):
            fwin(B, F, BW - 0.35, BW + 0.35, zf(s) + 0.8, zf(s) + 2.3, kind="stair", frame=0.05)
        box(B.m, cmat, xa, xz, Y_F, Y_F + 0.01, 3.9, Z_P - 0.5, faces="y")
        # lobby: hollow, glazed front at y = 1.0
        LY = 1.0
        wlen = xz - xa
        holes = [(0.35, 1.35, 0.3, 3.1), (wlen / 2 - DOOR_W / 2 - 0.05, wlen / 2 + DOOR_W / 2 + 0.05, 0.0, 2.3),
                 (wlen / 2 - DOOR_W / 2 - 0.05, wlen / 2 + DOOR_W / 2 + 0.05, 2.5, 3.2),
                 (wlen - 1.35, wlen - 0.35, 0.3, 3.1)]
        F = B.facade((xa, LY), (xz, LY), 0.0, 3.9, "CT_Concrete", holes=holes, depth=0.3)
        for hh in (holes[0], holes[2], holes[3]):
            fwin(B, F, hh[0], hh[1], hh[2], hh[3], kind="stair", frame=0.05, inset=0.15)
        B.entrance(lid, (xm, LY, 0.0), 0.0, width=DOOR_W, height=DOOR_H, kind="street", leaves=2)
        box(B.m, "CT_Interior", xa, xa + 0.3, LY, 9.0, 0.0, 3.9)
        box(B.m, "CT_Interior", xz - 0.3, xz, LY, 9.0, 0.0, 3.9)
        box(B.m, "CT_Floor", xa + 0.3, xz - 0.3, LY + 0.3, 9.0, 0.0, 0.02, faces="Z")
        box(B.m, "CT_ConcreteDark", xa, xz, LY - 0.3, Y_F, 3.9, 4.2)                           # canopy
        B.spot(lid + "_bench", "bench", (xm, 8.45, 0.0), 0.0)
        # furnishing: letter boxes on the left wall (inner face x = xa + 0.3), a doormat inside the door, plants, lamps
        fu.mailboxes(B, fu.Fr(xa + 0.3, LY + 0.3, 90.0, 0.02), 4.0, 0.95, cols=6, rows=3)
        fu.doormat(B, fu.Fr(xm, LY + 0.3, 0.0, 0.02), 0.0, -0.5)
        _dress_lobby(B, k, lid, xa, xz)
        for ly_ in (3.2, 6.8):
            B.lamp((xm, ly_, 3.5), lumens=520, colour="#FFE0B0", kind="lobby", radius=6.0)
            box(B.detail, "CT_Steel", xm - 0.01, xm + 0.01, ly_ - 0.01, ly_ + 0.01, 3.58, 3.9, faces="xXyY")

    # ---------------------------------------------------------------- back wall, ends
    for i in range(NB):
        xa, xz = xb(i), xb(i + 1)
        core = core_of(i)
        holes, wins = [], []
        for s in range(1, NST + 1):
            z = zf(s)
            if core:
                if i == min(a for a, b, *_ in CORES if a <= i < b):
                    holes.append((BW - 0.35, BW + 0.35, z + 0.8, z + 2.3))
                    wins.append((BW - 0.35, BW + 0.35, z + 0.8, z + 2.3, None, "stair"))
            else:
                holes.append((1.0, 2.6, z + 0.9, z + 2.3))
                wins.append((1.0, 2.6, z + 0.9, z + 2.3, flats.get((s, i)), "flat" if (s, i) in flats else "public"))
        if core and holes:
            holes = [(1.8 - 0.35, 1.8 + 0.35, h[2], h[3]) for h in holes]
            wins = [(1.8 - 0.35, 1.8 + 0.35, w[2], w[3], None, "stair") for w in wins]
        F = B.facade((xz, Y_B), (xa, Y_B), Z_P - 0.5, Z_ROOF, "CT_Concrete", holes=holes, depth=0.3)
        for u0, u1, w0, w1, fid, kind in wins:
            fwin(B, F, u0, u1, w0, w1, flat=fid, kind=kind, frame=0.05, inset=0.15)
    for z in (zf(3), zf(5), zf(7), zf(9), zf(11), zf(13), zf(15)):
        box(B.m, "CT_ConcreteDark", X0, XE, Y_B, Y_B + 0.3, z - 0.35, z - 0.05)
    for k in range(2, NB - 1, 2):
        box(B.m, "CT_ConcreteDark", xb(k) - 0.2, xb(k) + 0.2, Y_B, Y_B + 0.55, Z_P - 0.5, Z_ROOF)
    for xe, left in ((X0, True), (XE, False)):
        p0, p1 = ((xe, Y_B), (xe, Y_L)) if left else ((xe, Y_L), (xe, Y_B))
        L = Y_B - Y_L
        holes = []
        for (a, b), (z0, z1) in layers:
            if (a, b) == SHOP:
                continue
            for lvl in range(a, b + 1):
                holes.append((L / 2 - 0.7, L / 2 + 0.7, zf(lvl) + 0.9, zf(lvl) + 2.3))
        F = B.facade(p0, p1, Z_P - 0.5, Z_ROOF, "CT_Concrete", holes=holes, depth=0.3)
        bay = 0 if left else NB - 1
        for (a, b), (z0, z1) in layers:
            if (a, b) == SHOP:
                continue
            for lvl in range(a, b + 1):
                fwin(B, F, L / 2 - 0.7, L / 2 + 0.7, zf(lvl) + 0.9, zf(lvl) + 2.3, flat=flats[(lvl, bay)], kind="flat", frame=0.05, inset=0.15)

    # ---------------------------------------------------------------- flats data
    for (a, b), (z0, z1) in layers:
        if (a, b) == SHOP:
            continue
        for i in range(NB):
            if core_of(i):
                continue
            best = None
            for ca, cb, cid, _ in CORES:
                for ce in (ca, cb):
                    d = abs(i + 0.5 - ce)
                    if best is None or d < best[0]:
                        best = (d, xb(ce), cid)
            B.flat(flats[(a, i)], storey=a, beds=3 if b > a and i % 3 else 2 if b > a else 1,
                   door=(best[1], 12.0, z0 + 0.1), entrance=best[2])

    # ---------------------------------------------------------------- roof
    for (x0_, x1_, y0_, y1_) in ((X0, XE, Y_F, Y_F + 0.3), (X0, XE, Y_B - 0.3, Y_B), (X0, X0 + 0.3, Y_F, Y_B), (XE - 0.3, XE, Y_F, Y_B)):
        box(B.m, "CT_Concrete", x0_, x1_, y0_, y1_, zr, zr + 0.55)
    edge = [(X0 + 0.15, Y_F + 0.15), (XE - 0.15, Y_F + 0.15), (XE - 0.15, Y_B - 0.15), (X0 + 0.15, Y_B - 0.15), (X0 + 0.15, Y_F + 0.15)]
    B.railing(edge, zr + 0.55, h=0.55, post=2.5)
    # running track
    cx, cy = 0.0, 11.0
    outer = stadium(cx, cy, 15.0, 6.5)
    inner = stadium(cx, cy, 15.0, 3.7)
    outer_c = outer + [outer[0]]
    inner_c = inner + [inner[0]]
    band(B.m, "CT_PlasterRed", outer_c, inner_c, zr + 0.06)
    B.m.face("CT_Grass", [(x, y, zr + 0.04) for x, y in inner])
    ribbon_wall(B.m, "CT_ConcreteWhite", outer_c, zr, zr + 0.3)
    ribbon_wall(B.m, "CT_ConcreteWhite", inner_c[::-1], zr, zr + 0.3)
    B.spot("track_start", "view", (cx - 15.0, cy - 5.2, zr), 0.0)
    # kindergarten box
    kx0, kx1, ky0, ky1, kz = -53.0, -44.0, 12.0, 19.0, 3.2
    holes = [(0.8, 2.6, zr + 0.8, zr + 2.5), (3.4, 5.6, zr + 0.8, zr + 2.5), (6.4, 8.2, zr + 0.8, zr + 2.5)]
    F = B.facade((kx0, ky0), (kx1, ky0), zr, zr + kz, "CT_PlasterYellow", holes=holes, depth=0.3)
    for u0, u1, w0, w1 in holes:
        fwin(B, F, u0, u1, w0, w1, kind="public", frame=0.05)
    box(B.m, "CT_PlasterYellow", kx0, kx0 + 0.3, ky0, ky1, zr, zr + kz)
    box(B.m, "CT_PlasterYellow", kx1 - 0.3, kx1, ky0, ky1, zr, zr + kz)
    box(B.m, "CT_PlasterYellow", kx0, kx1, ky1 - 0.3, ky1, zr, zr + kz)
    box(B.m, "CT_ConcreteWhite", kx0 - 0.5, kx1 + 0.5, ky0 - 0.6, ky1 + 0.3, zr + kz, zr + kz + 0.25)
    B.railing([(kx0 - 0.5, 4.0), (kx1 + 0.5, 4.0), (kx1 + 0.5, ky0 - 0.2), (kx0 - 0.5, ky0 - 0.2), (kx0 - 0.5, 4.0)], zr + 0.55 * 0, h=1.0, mat="CT_SteelRed", post=1.2)
    # sandpit / play forms
    box(B.m, "CT_Wood", kx0, kx0 + 3.0, 6.0, 9.0, zr, zr + 0.25)
    # ventilation stack (tapered, sculptural)
    sx, sy = 40.0, 11.0
    loft(B.m, "CT_Concrete", octo(sx, sy, 1.9, 1.9, 0.7), zr, octo(sx, sy, 1.1, 1.1, 0.4), zr + 3.4)
    box(B.m, "CT_ConcreteDark", sx - 1.5, sx + 1.5, sy - 1.5, sy + 1.5, zr + 3.4, zr + 3.65)
    # machine rooms over the cores (stepped)
    for a, b, lid, cm in CORES:
        mx = (xb(a) + xb(b)) / 2
        box(B.m, "CT_Concrete", mx - 3.4, mx + 3.4, 11.0, 19.0, zr, zr + 3.2)
        box(B.m, "CT_ConcreteDark", mx - 2.0, mx + 2.0, 12.0, 16.0, zr + 3.2, zr + 4.0)
        box(B.m, "CT_ConcreteDark", mx - 3.6, mx + 3.6, 10.8, 19.2, zr + 3.2, zr + 3.4)
    # pergola over a sitting area
    for px in (47.0, 50.5, 54.0):
        for py in (5.0, 15.0):
            box(B.m, "CT_Concrete", px - 0.2, px + 0.2, py - 0.2, py + 0.2, zr, zr + 3.0)
        box(B.m, "CT_Concrete", px - 0.2, px + 0.2, 4.8, 15.2, zr + 3.0, zr + 3.3)
    for py in (5.0, 15.0):
        box(B.m, "CT_Concrete", 46.8, 54.2, py - 0.2, py + 0.2, zr + 3.3, zr + 3.6)
    for k in range(5):
        px = 47.0 + k * 1.75
        for q in range(4):
            box(B.detail, "CT_Wood", px, px + 0.1, 5.5 + q * 3.0, 7.0 + q * 3.0, zr + 3.3, zr + 3.4) if False else None
    for bx in (48.5, 52.0):
        kit.bench(B, bx, 9.25, zr, 0.0, w=2.0, kind="park")
        B.thing("roof_bench_%d" % int(bx), "seat", (bx, 9.25, zr + 0.45), 0.0, height=0.45)
        B.spot("roof_bench_%d" % int(bx), "bench", (bx, 8.6, zr), 0.0)
    _dress_roof(B, zr)
    _dress_pilotis(B)
    B.spot("roof_view", "view", (30.0, Y_F + 1.0, zr + 0.55), 0.0)

    B.footprint = [(X0, 1.0), (XE, 1.0), (XE, Y_B + 0.3), (X0, Y_B + 0.3)]
    B.height = zr + 4.0
    B.note("%d flats (duplex), 15 storeys on 6 m pilotis" % len({f for f in B.flats}))


# ------------------------------------------------------------------------------------------------ furnishing
def _wheel(m, mat, c, u, r, n=8, w=0.03):
    pts = [(c[0] + u[0] * r * math.cos(2 * math.pi * k / n), c[1] + u[1] * r * math.cos(2 * math.pi * k / n),
            c[2] + r * math.sin(2 * math.pi * k / n)) for k in range(n)]
    for k in range(n):
        fu.rod(m, mat, pts[k], pts[(k + 1) % n], w)


def _bike(B, x, y, dirv, mat="CT_PlasterRed", z=0.0):
    """A leaning city bicycle (detail) lying along the unit vector dirv."""
    u = dirv
    P3 = lambda s, h: (x + u[0] * s, y + u[1] * s, z + h)
    m = B.detail
    for s in (-0.55, 0.55):
        _wheel(m, "CT_Steel", P3(s, 0.33), u, 0.33)
    fu.rod(m, mat, P3(-0.55, 0.33), P3(-0.1, 0.55), 0.035)
    fu.rod(m, mat, P3(-0.1, 0.55), P3(0.4, 0.62), 0.035)
    fu.rod(m, mat, P3(0.4, 0.62), P3(0.55, 0.33), 0.035)
    fu.rod(m, mat, P3(-0.1, 0.55), P3(0.05, 0.3), 0.035)
    fu.rod(m, mat, P3(0.4, 0.62), P3(0.4, 0.95), 0.03)
    hb = P3(0.4, 0.95)
    fu.rod(m, "CT_Steel", (hb[0] - u[1] * 0.28, hb[1] + u[0] * 0.28, hb[2]), (hb[0] + u[1] * 0.28, hb[1] - u[0] * 0.28, hb[2]), 0.03)
    sd = P3(-0.17, 0.77)
    fu.rod(m, "CT_Leather", (sd[0] - u[0] * 0.12, sd[1] - u[1] * 0.12, sd[2]), (sd[0] + u[0] * 0.12, sd[1] + u[1] * 0.12, sd[2]), 0.07)
    fu.rod(m, mat, P3(-0.1, 0.55), P3(-0.17, 0.74), 0.03)


def _pram(B, x, y, front, mat="CT_PlasterBlue", z=0.0):
    f = front
    s_ = (-f[1], f[0])
    P3 = lambda s, t: (x + s_[0] * s + f[0] * t, y + s_[1] * s + f[1] * t)
    m = B.detail
    geom.prism(m, mat, [P3(du, dv) for du, dv in ((-0.27, -0.45), (0.27, -0.45), (0.27, 0.45), (-0.27, 0.45))], z + 0.38, z + 0.62, top=True, bottom=True)
    geom.prism(m, "CT_Paper", [P3(du, dv) for du, dv in ((-0.27, -0.45), (0.27, -0.45), (0.27, -0.05), (-0.27, -0.05))], z + 0.62, z + 0.86, top=True, bottom=False)
    for du, dv in ((-0.3, -0.35), (0.3, -0.35), (-0.3, 0.35), (0.3, 0.35)):
        c = P3(du, dv)
        _wheel(m, "CT_Steel", (c[0], c[1], z + 0.17), f, 0.17, n=8, w=0.03)
    for du in (-0.25, 0.25):
        a, b = P3(du, -0.45), P3(du, -0.75)
        fu.rod(m, "CT_Steel", (a[0], a[1], z + 0.6), (b[0], b[1], z + 1.0), 0.03)
    a, b = P3(-0.25, -0.75), P3(0.25, -0.75)
    fu.rod(m, "CT_Steel", (a[0], a[1], z + 1.0), (b[0], b[1], z + 1.0), 0.03)


def _planter(B, x, y, w=1.0, d=0.5, kind="fern", seed=0, z=0.0, pot="CT_PlasterGreen"):
    box(B.detail, "CT_Concrete", x - w / 2, x + w / 2, y - d / 2, y + d / 2, z, z + 0.5)
    kit.plant(B, x, y, z + 0.5, 0.0, kind=kind, pot=pot, h=0.9, seed=seed)


def _dress_lobby(B, k, lid, xa, xz):
    """One lobby design (6.6 x 7.7 m, 3.9 m high; door mid front, mailboxes on the left wall y 4.5..6.1) with
    differences per lobby: desk and board side, colours, extras (bike + pram / aquarium + chess table)."""
    r = random.Random(8100 + k * 17)
    xm = (xa + xz) / 2
    Z = 0.02
    xl, xr = xa + 0.3, xz - 0.3
    yb = 9.0
    sa = 1 if k == 1 else -1                     # side A (desk, notice board): +1 left wall, -1 right wall
    sb = -sa
    pal = ["CT_PlasterBlue", "CT_PlasterGreen", "CT_PlasterYellow", "CT_PlasterRed", "CT_PlasterPink"]
    r.shuffle(pal)
    pre = lid + "_"

    def ax(s, off):
        return xl + off if s > 0 else xr - off

    def fyw(s):
        return 90.0 if s > 0 else 270.0

    dirv = (0.0, 1.0)
    kit.rug(B, xm, 5.4, Z, 0.0, w=2.8, d=2.8, name="rug_round_%d" % k, round=True)
    # side A: notice board over a bench, desk with radio, plant
    kit.notice_board(B, ax(sa, 0.0), 3.0, Z, fyw(sa), w=1.0, zc=1.8, seed=11 + k)
    B.thing(pre + "notices", "board", (ax(sa, 0.05), 3.0, 1.8), fyw(sa), topic="lost" if k == 0 else "club")
    kit.bench(B, ax(sa, 0.25), 3.0, Z, fyw(sa), w=1.6, kind="park")
    B.thing(pre + "bench_a", "seat", (ax(sa, 0.25), 3.0, 0.47), fyw(sa), height=0.45)
    d = kit.office_desk(B, ax(sa, 0.95), 6.9, Z, fyw(sa), w=1.4, d=0.7)
    kit.chair(B, ax(sa, 0.3), 6.9, Z, fyw(sa), kind="wood")
    kit.radio(B, ax(sa, 0.95), 6.6, d["top"], fyw(sa))
    kit.table_lamp(B, ax(sa, 0.95), 7.35, d["top"], 0.0)
    B.thing(pre + "concierge_desk", "counter", (ax(sa, 0.95), 6.9, 1.0), fyw(sa), role="concierge", service="reception")
    B.thing(pre + "radio", "machine", (ax(sa, 0.95), 6.6, d["top"] + 0.15), fyw(sa), what="jukebox")
    kit.plant(B, ax(sa, 0.4), 8.4, Z, 0.0, kind=("ficus", "ficus")[k % 2], pot=pal[0], h=1.5, seed=k)
    # QA: on the left wall (sa > 0) the letter boxes hang at y 4.5..6.1: hang the picture over the desk instead
    kit.picture(B, ax(sa, 0.0), 5.4 if sa < 0 else 7.0, Z, fyw(sa), name=("painting_sea_calm", "painting_dunes")[k % 2], w=0.9, zc=2.0)
    # side B: sofa corner with a coffee table, armchair, lamp, pictures
    kit.sofa(B, ax(sb, 0.45), 3.4, Z, fyw(sb), seats=2, mat=pal[1])
    B.thing(pre + "sofa", "seat", (ax(sb, 0.45), 3.4, 0.47), fyw(sb), height=0.45)
    ct = kit.coffee_table(B, ax(sb, 1.7), 3.4, Z, fyw(sb))
    kit.newspaper(B, ax(sb, 1.7), 3.4, ct["top"], 0.0, seed=k)
    kit.cup(B, ax(sb, 1.65), 3.6, ct["top"], 0.0)
    kit.picture(B, ax(sb, 0.0), 3.4, Z, fyw(sb), name=("painting_lighthouse", "painting_pines")[k % 2], w=1.0, zc=2.0)
    kit.armchair(B, ax(sb, 0.8), 5.6, Z, fyw(sb) + (30 if sb > 0 else -30), mat=pal[2])
    B.thing(pre + "armchair", "seat", (ax(sb, 0.8), 5.6, 0.47), fyw(sb), height=0.45)
    kit.floor_lamp(B, ax(sb, 0.4), 6.9, Z, 0.0)
    kit.poster(B, ax(sb, 0.0), 7.6, Z, fyw(sb), name=("poster_birds_0", "poster_cons_3")[k % 2], w=0.6, zc=1.8)
    # back wall: bench under a big mosaic and the clock
    kit.bench(B, xm, yb - 0.3, Z, 0.0, w=2.0, kind="park")
    B.thing(pre + "bench_back", "seat", (xm, yb - 0.3, 0.47), 0.0, height=0.45)
    kit.mosaic_panel(B, xm, yb, Z, 0.0, w=2.4, h=1.3, zc=2.5, seed=k)
    kit.wall_clock(B, xm, yb, Z, 0.0, zc=3.45, face="clock_%d" % (k + 1))
    kit.notice_board(B, xm - 2.2, yb, Z, 0.0, w=0.9, zc=1.7, seed=21 + k)
    B.thing(pre + "club_board", "board", (xm - 2.2, yb - 0.05, 1.7), 0.0, topic="club" if k == 0 else "lost")
    kit.picture(B, xm + 2.2, yb, Z, 0.0, name=("painting_portrait", "painting_abstract")[k % 2], w=0.7, zc=1.8)
    # front: hat stand beside the door
    kit.hat_stand(B, xm + 1.7, 1.9, Z, 0.0)
    # extras
    if k == 0:
        _bike(B, ax(sb, 0.55), 8.1, dirv, mat=pal[4], z=Z)
        _pram(B, ax(sa, 0.55), 1.9, (0.0, 1.0), mat=pal[3], z=Z)
    else:
        kit.aquarium(B, ax(sb, 0.3), 8.1, Z, fyw(sb), w=1.0, seed=3)
        B.thing(pre + "aquarium", "screen", (ax(sb, 0.3), 8.1, 1.0), fyw(sb), what="aquarium")


def _dress_pilotis(B):
    """The open ground under the slab (x -50..50, y 0..20, ceiling 5.1 m, piers at x = -50.4, -36, -14.4, 0, 14.4,
    36, 50.4, y 6.5 and 16.5): a games yard west and east, benches, planters, a bike rack."""
    Z = 0.0
    # west yard between piers 2 and 6: table tennis, chess, benches, planters
    kit.pingpong_table(B, -43.0, 12.0, Z, 0.0)
    B.thing("yard_pingpong_w", "play", (-43.0, 12.0, 0.77), 0.0, what="table_tennis")
    kit.chess_table(B, -46.0, 8.0, Z, 0.0, seed=1)
    for sgn in (-1, 1):
        kit.stool(B, -46.0 + sgn * 0.55, 8.0, Z, 90.0 + sgn * 90.0)
    B.thing("yard_chess_w", "play", (-46.0, 8.0, 0.73), 0.0, what="chess")
    for bx in (-46.0, -40.0):
        kit.bench(B, bx, 15.4, Z, 0.0, w=1.8, kind="park")
        B.thing("yard_bench_w_%d" % int(-bx), "seat", (bx, 15.4, 0.47), 0.0, height=0.45)
    # QA: no light in the yards at dusk: one lamp under the slab over each
    B.lamp((-43.0, 12.0, 4.95), lumens=1200, colour="#FFE0B0", kind="lobby", radius=9.0)
    B.lamp((43.0, 12.0, 4.95), lumens=1200, colour="#FFE0B0", kind="lobby", radius=9.0)
    _planter(B, -38.0, 8.2, seed=1)
    _planter(B, -49.0, 12.0, d=1.0, w=0.5, kind="ficus", seed=2)
    # east yard, mirrored with a second chess table instead of the second bench
    kit.pingpong_table(B, 43.0, 12.0, Z, 0.0)
    B.thing("yard_pingpong_e", "play", (43.0, 12.0, 0.77), 0.0, what="table_tennis")
    kit.chess_table(B, 46.0, 8.0, Z, 0.0, seed=2)
    for sgn in (-1, 1):
        kit.stool(B, 46.0 + sgn * 0.55, 8.0, Z, 90.0 + sgn * 90.0)
    B.thing("yard_chess_e", "play", (46.0, 8.0, 0.73), 0.0, what="chess")
    kit.bench(B, 41.0, 15.4, Z, 0.0, w=1.8, kind="park")
    B.thing("yard_bench_e", "seat", (41.0, 15.4, 0.47), 0.0, height=0.45)
    _planter(B, 38.0, 8.2, seed=3)
    _planter(B, 49.0, 12.0, d=1.0, w=0.5, kind="ficus", seed=4)
    # a bike rack by each lobby (bikes lined up along y, leaning on a rail) and a bench in front of the pilotis row
    for cx, sgn in ((-18.0, 1), (18.0, -1)):
        box(B.detail, "CT_Steel", cx - 0.03, cx + 0.03, 10.5, 14.5, 0.5, 0.56)
        for j in range(2):
            _bike(B, cx + 0.45 * sgn, 11.4 + j * 1.2, (0.0, 1.0), mat=("CT_PlasterRed", "CT_PlasterBlue")[j], z=Z)


def _dress_roof(B, zr):
    """Roof terrace: chess under the pergola, planters, a telescope at the viewpoint, a notice board by the track."""
    kit.chess_table(B, 50.5, 12.2, zr, 0.0, seed=7)
    for sgn in (-1, 1):
        kit.stool(B, 50.5 + sgn * 0.55, 12.2, zr, 90.0 + sgn * 90.0)
    B.thing("roof_chess", "play", (50.5, 12.2, zr + 0.73), 0.0, what="chess")
    for j, (x, y) in enumerate(((46.0, 6.5), (55.0, 14.0))):
        _planter(B, x, y, seed=20 + j, z=zr, kind=("fern", "ficus")[j % 2])
    kit.telescope(B, 30.5, 3.6, zr, 0.0)
    B.thing("roof_telescope", "machine", (30.5, 3.6, zr + 1.3), 0.0, what="telescope")
