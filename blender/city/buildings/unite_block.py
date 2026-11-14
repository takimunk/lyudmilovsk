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
import os
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
    _street_shell(B)
    _flat_shell(B)
    _street(B)
    _flat(B)
    _roof2(B, zr)
    _lobby2(B)

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
    d = kit.office_desk(B, ax(sa, 0.95), 6.9, Z, fyw(sa), w=1.4, d=0.7, typewriter_on=False)
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
    _lift_door(B, "lift_g_%d" % (k + 1), xm + 2.2, yb, Z, 0.0)
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
        B.thing("yard_chess_w_stool%d" % (sgn + 1), "seat", (-46.0 + sgn * 0.55, 8.0, 0.45), 180.0 - sgn * 90.0, height=0.45)
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
        B.thing("yard_chess_e_stool%d" % (sgn + 1), "seat", (46.0 + sgn * 0.55, 8.0, 0.45), 180.0 - sgn * 90.0, height=0.45)
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
        B.thing("roof_chess_stool%d" % (sgn + 1), "seat", (50.5 + sgn * 0.55, 12.2, zr + 0.45), 180.0 - sgn * 90.0, height=0.45)
    _lift_door(B, "lift_roof", (xb(22) + xb(24)) / 2, 11.0, zr, 0.0)
    B.thing("roof_chess", "play", (50.5, 12.2, zr + 0.73), 0.0, what="chess")
    for j, (x, y) in enumerate(((46.0, 6.5), (55.0, 14.0))):
        _planter(B, x, y, seed=20 + j, z=zr, kind=("fern", "ficus")[j % 2])
    kit.telescope(B, 30.5, 3.6, zr, 0.0)
    B.thing("roof_telescope", "machine", (30.5, 3.6, zr + 1.3), 0.0, what="telescope")


# ================================================================================================ round 2
# The shopping street (storeys 7-8, z 24..30) behind the glazed band of the middle segment (bays 10..21, x -21.6..21.6):
# a corridor 3.3 m deep and two storeys high (z 24..29.75) along the glass, shop units 3 m high behind it (the shops 6 m
# deep, the games room right back to the rear windows), and, in bay 13, the private stair of flat 5-12 (Priya Tulegenova)
# which goes DOWN from the street floor into the first level of her duplex (storeys 5-6, z 18..24). The street and the
# flat are reached by the storey teleports; the lobbies' stair slits do not lead up (the cores stay solid).
ZS = zf(7)                       # street floor 24.0
Z_SH = ZS + 3.0                  # shop ceilings (underside)
Z_CE = zf(9) - 0.25              # corridor ceiling (underside) 29.75
YC0 = Y_L + 0.3                  # inner face of the glazed band 4.7
YW = 8.0                         # shop fronts (face toward the corridor)
YE = Y_B - 0.3                   # inner face of the back wall 19.7
YU = 14.0                        # the shops' back walls
SX0, SX1 = xb(10), xb(22)
# id, first bay, end bay, wall colour, opening x0..x1 in the front wall, depth y
UNITS = [
    ("bake", 10, 12, "CT_PlasterYellow", (-20.4, -16.6), YU),
    ("fab", 12, 13, "CT_PlasterPink", (-13.7, -11.5), YU),
    ("tul", 13, 14, "CT_PlasterBlue", (-9.5, -8.5), YU),
    ("game", 14, 16, "CT_PlasterGreen", (-6.7, -0.5), YE),
    ("news", 16, 17, "CT_PlasterBlue", (0.6, 3.0), YU),
    ("groc", 17, 18, "CT_PlasterGreen", (4.2, 6.6), YU),
    ("toys", 18, 20, "CT_PlasterRed", (8.6, 13.0), YU),
    ("barb", 20, 22, "CT_PlasterGrey", (16.0, 20.0), YU),
]
UN = {u[0]: dict(id=u[0], x0=xb(u[1]), x1=xb(u[2]), mat=u[3], op=u[4], yend=u[5]) for u in UNITS}
HALL_HOLE = (-9.6, -8.4, 9.0, 13.6)         # flat 5-12's entry stair through the street floor
FX0, FX1 = xb(13) + 0.2, xb(14) - 0.2         # flat 5-12 between its party walls: -10.6 .. -7.4
FL1, FL2 = zf(5), zf(6)                       # its two levels, 18.0 and 21.0
INNER_HOLE = (FX0, FX0 + 1.0, 14.4, 18.7)     # the inner stair down to level 1, west wall
STEP_H, STEP_D = 3.0 / 16.0, 0.27


def _slab(B, x0, x1, y0, y1, zb, zt, hole=None, top="CT_Floor", body="CT_Concrete"):
    """A floor slab (collision) from zb to zt, top plate in `top`, with an optional rectangular hole (hx0, hx1, hy0, hy1)."""
    if hole:
        hx0, hx1, hy0, hy1 = hole
        rects = [(x0, hx0, y0, y1), (hx1, x1, y0, y1), (hx0, hx1, y0, hy0), (hx0, hx1, hy1, y1)]
    else:
        rects = [(x0, x1, y0, y1)]
    for a, b, c, d in rects:
        box(B.m, body, a, b, c, d, zb, zt - 0.01)
        box(B.m, top, a, b, c, d, zt - 0.01, zt, faces="Z")


def _wallx(B, mat, x0, x1, y0, y1, z0, z1, faces="all"):
    box(B.m, mat, x0, x1, y0, y1, z0, z1, faces=faces)


def _stairs(B, xa, xb_, y0, ztop, zfloor, n=16, dy=1.0, top="CT_WoodLight"):
    """A straight stair of n risers between ztop and zfloor along y (dy=+1: toward +y from y0), solid blocks (collision), wood treads.
    The tread k (1..n-1) is a block from the floor up to its surface; the last riser is the floor itself."""
    rise = (ztop - zfloor) / n
    for k in range(1, n):
        ya = y0 + dy * STEP_D * (k - 1)
        yb = y0 + dy * STEP_D * k
        lo, hi = min(ya, yb), max(ya, yb)
        zt = ztop - rise * k
        box(B.m, "CT_Concrete", xa, xb_, lo, hi, zfloor, zt - 0.02)
        box(B.m, top, xa, xb_, lo, hi, zt - 0.02, zt, faces="Zxy" if dy > 0 else "ZxY")
    return y0 + dy * STEP_D * (n - 1)


def _lift_door(B, tid, cx, cy, z, yaw):
    """A lift door in a wall: steel frame, two leaves, a call button; the wall is at (cx, cy) for yaw 0 (normal -y) and for yaw 90
    (normal +x, cy runs along the wall). Thing: kind door, to=lift (a caption for now)."""
    def bx(u0, u1, v0, v1, z0, z1, mat, faces="all"):
        if yaw == 0:
            box(B.detail, mat, cx + u0, cx + u1, cy - v1, cy - v0, z + z0, z + z1, faces=faces)
        else:
            box(B.detail, mat, cx + v0, cx + v1, cy + u0, cy + u1, z + z0, z + z1, faces=faces)
    bx(-0.55, 0.55, 0.0, 0.05, 0.0, 2.2, "CT_Steel")
    bx(-0.45, -0.005, 0.05, 0.075, 0.0, 2.1, "CT_SteelWhite")
    bx(0.005, 0.45, 0.05, 0.075, 0.0, 2.1, "CT_SteelWhite")
    bx(-0.04, 0.04, 0.075, 0.09, 0.0, 2.1, "CT_Steel")
    bx(0.62, 0.7, 0.0, 0.03, 1.0, 1.2, "CT_ConcreteWhite")
    bx(0.645, 0.675, 0.03, 0.045, 1.07, 1.13, "CT_Steel")
    px, py = (cx, cy - 0.04) if yaw == 0 else (cx + 0.04, cy)
    B.thing(tid, "door", (px, py, z + 1.1), yaw, to="lift")


def _street_shell(B):
    """Floor, corridor ceiling, shop fronts with openings, party walls, shop ceilings, the hall of flat 5-12."""
    # the street floor: one slab over the whole segment with the hole of flat 5-12's stair
    _slab(B, SX0, SX1, Y_L + 0.2, YE + 0.3, ZS - 0.3, ZS, hole=HALL_HOLE)
    # the corridor ceiling (two storeys high), plain
    _wallx(B, "CT_Interior", SX0, SX1, YC0, YW + 0.2, Z_CE, Z_CE + 0.25)
    for key in UN:
        u = UN[key]
        x0, x1, mat = u["x0"], u["x1"], u["mat"]
        o0, o1 = u["op"]
        zo = ZS + 2.55
        # front wall (face toward the corridor at y = YW), piers and a tall fascia over the opening
        _wallx(B, mat, x0, o0, YW, YW + 0.2, ZS, Z_CE)
        _wallx(B, mat, o1, x1, YW, YW + 0.2, ZS, Z_CE)
        _wallx(B, mat, o0, o1, YW, YW + 0.2, zo, Z_CE)
        # side walls inside, the back wall, the shop ceiling
        ye = u["yend"]
        _wallx(B, mat, x0, x0 + 0.1, YW + 0.2, ye, ZS, Z_SH, faces="xXyY")
        _wallx(B, mat, x1 - 0.1, x1, YW + 0.2, ye, ZS, Z_SH, faces="xXyY")
        if ye < YE:
            _wallx(B, mat, x0, x1, ye, ye + 0.2, ZS, Z_SH, faces="xXyY")
        _wallx(B, "CT_Interior", x0, x1, YW + 0.2, ye, Z_SH, Z_SH + 0.2)
    # a threshold strip in every opening so the floor reads as a doorsill
    for key in UN:
        o0, o1 = UN[key]["op"]
        box(B.detail, "CT_ConcreteDark", o0, o1, YW - 0.04, YW + 0.2, ZS, ZS + 0.012, faces="Z")


def _flat_shell(B):
    """Flat 5-12 (bay 13): party walls, the two levels' floors, the stair down from the street hall, the inner stair to level 1."""
    for xa, xz in ((FX0 - 0.2, FX0), (FX1, FX1 + 0.2)):
        _wallx(B, "CT_Interior", xa, xz, Y_L + 0.3, YE, FL1 - 0.3, ZS - 0.3, faces="xXyY")
    _slab(B, FX0 - 0.2, FX1 + 0.2, Y_L + 0.3, YE, FL1 - 0.3, FL1, top="CT_Floor")
    _slab(B, FX0 - 0.2, FX1 + 0.2, Y_L + 0.3, YE, FL2 - 0.3, FL2, hole=INNER_HOLE, top="CT_Floor")
    # the entry stair: from the hall's landing (z 24) down along +y to level 2's floor, in the middle of the hole
    _stairs(B, HALL_HOLE[0], HALL_HOLE[1], HALL_HOLE[2], ZS, FL2, dy=1.0)
    # the inner stair, west wall: from level 2 down toward +y to level 1
    _stairs(B, INNER_HOLE[0], INNER_HOLE[1], INNER_HOLE[2] - 0.0, FL2, FL1, dy=1.0)


def _stand(B, sid, kind, x, y, z, yaw):
    B.spot(sid, kind, (x, y, z), yaw)


def _fascia(B, key, sign, w=2.2, zc=3.75):
    """The shop's sign on the tall fascia over its opening."""
    u = UN[key]
    kit.sign_plate(B, (u["op"][0] + u["op"][1]) / 2, YW, ZS, 0.0, name=sign, w=w, zc=zc)


def _street(B):
    """The street itself: lamps, benches, boards, the clock, the phone, the photo booth, then each unit."""
    Z = ZS
    for x in (-15.0, -7.5, 0.0, 7.5, 15.0):
        kit.pendant(B, x, 6.3, Z_CE, drop=2.4, kind="opal", light=True, lumens=900, colour="#FFE0B0", lamp_kind="public", radius=11.0)
    # benches under the glazed band (the sitter looks across the street at the shops), planters between them
    for k, bx in enumerate((-14.0, -1.5, 11.5)):
        kit.bench(B, bx, YC0 + 0.35, Z, 180.0, w=1.8, kind="park")
        B.thing("street_bench_%d" % (k + 1), "seat", (bx, YC0 + 0.35, Z + 0.47), 180.0, height=0.45)
        _stand(B, "street_bench_%d" % (k + 1), "bench", bx, YC0 + 1.0, Z, 180.0)
    kit.planter(B, 6.0, YC0 + 0.3, Z, 0.0, w=1.4, d=0.45, h=0.5, seed=40)
    # boards on the piers between shops: the running notices, and the hours
    kit.notice_board(B, -15.5, YW, Z, 0.0, w=1.0, zc=1.6, seed=51)
    B.thing("street_board", "board", (-15.5, YW - 0.05, Z + 1.6), 0.0, topic="local")
    kit.notice_board(B, -0.3, YW, Z, 0.0, w=0.9, zc=1.6, seed=52)
    B.thing("street_hours", "board", (-0.3, YW - 0.05, Z + 1.6), 0.0, topic="timetable")
    kit.wall_clock(B, 5.4, YW, Z, 0.0, zc=4.6, r=0.3, face="clock_2")
    # light switch on the pier between the news stand and the grocery
    x = 3.6
    box(B.detail, "CT_ConcreteWhite", x - 0.04, x + 0.04, YW - 0.015, YW, Z + 1.24, Z + 1.36, faces="yXxzZ")
    box(B.detail, "CT_Steel", x - 0.015, x + 0.015, YW - 0.025, YW - 0.015, Z + 1.28, Z + 1.32, faces="yXxzZ")
    B.thing("street_switch", "machine", (x, YW - 0.03, Z + 1.3), 0.0, what="switch", switch="hall")
    # the payphone on the west end wall, the photo booth at the east end
    _lift_door(B, "lift_street", SX0, 5.3, Z, 90.0)
    kit.wall_phone(B, SX0 + 0.0, 6.2, Z, 90.0, zc=1.45)
    B.thing("street_phone", "machine", (SX0 + 0.1, 6.2, Z + 1.45), 90.0, what="payphone")
    kit.photo_booth(B, 20.7, 6.7, Z, 180.0)
    B.thing("street_photo", "machine", (20.7, 6.7, Z + 1.2), 180.0, what="photo_booth")
    _stand(B, "street_photo_spot", "stand", 20.7, 7.9, Z, 0.0)
    for key, fn in (("bake", _u_bake), ("fab", _u_fab), ("tul", _u_hall), ("game", _u_game), ("news", _u_news),
                    ("groc", _u_groc), ("toys", _u_toys), ("barb", _u_barber)):
        fn(B, UN[key])
    if os.environ.get("UNITE_PREVIEW"):           # preview only: the game lights the shops through the walls (shadowless lamps)
        for key in UN:
            B.lamp(((UN[key]["x0"] + UN[key]["x1"]) / 2, 10.8, Z + 2.5), lumens=500, kind="public", radius=6.0, fixture=False)
        B.lamp((-9.0, 11.0, ZS + 2.0), lumens=400, kind="public", radius=4.0, fixture=False)
        B.lamp((-9.0, 12.0, FL2 + 2.0), lumens=500, kind="public", radius=5.0, fixture=False)
        B.lamp((-9.0, 7.0, FL2 + 2.0), lumens=500, kind="public", radius=5.0, fixture=False)
        B.lamp((-9.0, 7.0, FL1 + 2.0), lumens=500, kind="public", radius=5.0, fixture=False)
        B.lamp((-9.0, 15.0, FL1 + 2.0), lumens=500, kind="public", radius=5.0, fixture=False)
    _stand(B, "street_view_w", "view", -19.0, 6.6, Z, 180.0)
    _stand(B, "street_view_e", "view", 12.0, 6.6, Z, 180.0)


def _counter(B, tid, x, y, z, w, shop, front, role="seller", service="shop", top="CT_TileWhite", spots=None):
    c = kit.shop_counter(B, x, y, z, 0.0, w=w, front=front, top=top)
    B.thing(tid, "counter", (x, y, z + 1.1), 0.0, role=role, service=service, shop=shop)
    if spots:
        _stand(B, spots + "_staff", "work", x, y + 0.9, z, 0.0)
        _stand(B, spots + "_customer", "counter", x, y - 0.8, z, 180.0)
    return c


def _u_bake(B, u):
    Z = ZS
    _fascia(B, "bake", "sign_bread", 2.4)
    kit.awning(B, -18.5, YW, Z, 0.0, w=3.8, proj=0.7, zt=2.95, mat="CT_PlasterRed", mat2="CT_TileWhite")
    kit.sandwich_board(B, -16.2, 7.15, Z, 15.0, name="menu_0")
    _counter(B, "bake_counter", -17.8, 10.5, Z, 2.4, "bakery", "CT_PlasterRed", spots="bake")
    kit.bread_rack(B, -20.95, 11.6, Z, 90.0)
    B.thing("bake_rack", "machine", (-20.95, 11.6, Z + 1.0), 90.0, what="bread_rack")
    kit.shop_shelf(B, -18.6, YU, Z, 0.0, w=1.6, goods="boxes", levels=3, seed=5)
    kit.sack(B, -20.8, 9.2, Z, 20.0)
    kit.sack(B, -20.4, 9.5, Z, 80.0)
    kit.pendant(B, -18.5, 10.8, Z_SH, drop=0.55, kind="cone", light=True, lumens=450, colour="#FFE0B0", lamp_kind="public", radius=4.5)
    kit.menu_board(B, u["x1"] - 0.1, 11.4, Z, 270.0, name="price_0", w=0.8)
    kit.picture(B, u["x0"] + 0.1, 13.0, Z, 90.0, name="painting_still_life", w=0.7, zc=1.75)
    kit.rug(B, -18.3, 9.2, Z + 0.01, 0.0, w=2.6, d=0.8, name="rug_2", seed=3)
    _stand(B, "bake_browse", "stand", -20.1, 10.4, Z, 90.0)


def _u_fab(B, u):
    Z = ZS
    _fascia(B, "fab", "sign_fabric", 1.8)
    _counter(B, "fab_counter", -12.6, 9.55, Z, 1.8, "fabrics", "CT_PlasterPink", spots="fab")
    kit.shop_shelf(B, -14.3, 11.6, Z, 90.0, w=1.6, goods="fabric", seed=3)
    B.thing("fab_bolts", "shelf", (-14.15, 11.6, Z + 1.1), 90.0, topic="art")
    kit.bin_rack(B, -10.9, 11.5, Z, 270.0, cols=5, rows=7)
    B.thing("fab_buttons", "machine", (-11.0, 11.5, Z + 1.0), 270.0, what="button_drawers")
    kit.sewing_table(B, -12.6, 13.0, Z, 0.0)
    B.thing("fab_sewing", "craft", (-12.6, 13.0, Z + 0.95), 0.0, what="sewing_machine")
    kit.stool(B, -12.6, 13.75, Z, 180.0)
    B.thing("fab_sewing_stool", "seat", (-12.6, 13.75, Z + 0.45), 0.0, height=0.45)
    kit.mirror(B, -12.6, 14.0, Z, 0.0, zc=1.5, w=0.55, h=1.0)
    kit.poster(B, -14.3, 13.2, Z, 90.0, name="fabric_1", w=0.6, zc=1.7)
    kit.rug(B, -12.6, 10.8, Z + 0.01, 0.0, w=1.6, d=0.9, name="rug_6", seed=6)
    kit.pendant(B, -12.6, 10.6, Z_SH, drop=0.5, kind="cone", light=True, lumens=450, colour="#FFE0B0", lamp_kind="public", radius=4.5)


def _u_hall(B, u):
    """The private hall of flat 5-12: a door in the corridor, a hall with the stair hole and its rails."""
    Z = ZS
    kit.rug(B, -9.0, 7.35, Z + 0.01, 0.0, w=1.0, d=0.6, name="rug_2", seed=2)
    kit.plant(B, -10.2, 7.55, Z, 0.0, kind="fern", pot="CT_PlasterRed", h=0.9, seed=61)
    kit.poster(B, -8.1, YW, Z, 0.0, name="paper_typed", w=0.22, zc=1.55)
    B.thing("tul_door", "door", (-9.0, YW - 0.02, Z + 1.3), 0.0, to="flat")
    hx0, hx1, hy0, hy1 = HALL_HOLE
    for xr in (hx0 - 0.04, hx1 + 0.04):
        B.railing([(xr, hy0 + 0.35), (xr, hy1)], Z, h=0.95, post=1.4)
    B.railing([(hx0 - 0.04, hy1 + 0.04), (hx1 + 0.04, hy1 + 0.04)], Z, h=0.95, post=1.4)
    kit.picture(B, u["x0"] + 0.1, 10.8, Z, 90.0, name="painting_sea_calm", w=0.7, zc=1.65)
    kit.mirror(B, u["x1"] - 0.1, 11.3, Z, 270.0, zc=1.5, w=0.5, h=0.9)
    kit.hat_stand(B, -10.2, 13.6, Z, 0.0)
    B.thing("tul_hats", "machine", (-10.2, 13.6, Z + 1.2), 0.0, what="hat_stand")
    kit.pendant(B, -9.0, 10.5, Z_SH, drop=0.5, kind="opal", light=True, lumens=450, colour="#FFE0B0", lamp_kind="public", radius=4.5)


def _u_game(B, u):
    """The games room: table tennis, chess, checkers, dominoes, table football, a score board, spectators' bench."""
    Z = ZS
    _fascia(B, "game", "sign_quiet", 1.6)
    kit.pingpong_table(B, -3.6, 16.4, Z, 0.0)
    B.thing("game_pingpong", "play", (-3.6, 16.4, Z + 0.77), 0.0, what="table_tennis")
    _stand(B, "game_pingpong_a", "stand", -3.6, 15.1, Z, 180.0)
    _stand(B, "game_pingpong_b", "stand", -3.6, 17.7, Z, 0.0)
    for k, (tx, what) in enumerate(((-6.0, "chess"), (-4.4, "checkers"))):
        kit.chess_table(B, tx, 10.9, Z, 0.0, seed=k + 5)
        for sgn in (-1, 1):
            kit.stool(B, tx + sgn * 0.55, 10.9, Z, 90.0 + sgn * 90.0)
            B.thing("game_%s_stool%d" % (what, sgn + 1), "seat", (tx + sgn * 0.55, 10.9, Z + 0.45), 180.0 - sgn * 90.0, height=0.45)
        B.thing("game_" + what, "play", (tx, 10.9, Z + 0.73), 0.0, what=what)
        _stand(B, "game_%s_seat" % what, "table", tx - 0.55, 10.9, Z, 90.0)
    d = B.detail
    tb = kit.table(B, -2.8, 10.9, Z, 0.0, w=0.8, d=0.8)
    for ci, (cu, cv, dy) in enumerate(((0.0, -0.7, 0.0), (0.0, 0.7, 180.0), (-0.7, 0.0, 90.0))):
        kit.chair(B, -2.8 + cu, 10.9 + cv, Z, dy, kind="wood")
        B.thing("game_dominoes_chair%d" % ci, "seat", (-2.8 + cu, 10.9 + cv, Z + 0.45), dy, height=0.45)
    for k in range(7):
        ux, vx = -0.22 + k * 0.07, 0.04 * ((-1) ** k)
        box(d, "CT_ConcreteDark", -2.8 + ux - 0.03, -2.8 + ux + 0.03, 10.9 + vx - 0.055, 10.9 + vx + 0.055, tb["top"], tb["top"] + 0.014, faces="xXyYZ")
    B.thing("game_dominoes", "play", (-2.8, 10.9, tb["top"]), 0.0, what="dominoes")
    _stand(B, "game_dominoes_seat", "table", -3.5, 10.9, Z, 90.0)
    kit.table_football(B, -0.85, 13.6, Z, 90.0)
    B.thing("game_football", "play", (-0.85, 13.6, Z + 0.85), 90.0, what="table_football")
    kit.blackboard(B, u["x0"] + 0.1, 13.6, Z, 90.0, w=1.6, writing=None, seed=3)
    B.thing("game_scores", "board", (u["x0"] + 0.15, 13.6, Z + 1.5), 90.0, topic="rules")
    kit.notice_board(B, u["x0"] + 0.1, 16.8, Z, 90.0, w=0.9, zc=1.6, seed=57)
    B.thing("game_rules", "board", (u["x0"] + 0.15, 16.8, Z + 1.6), 90.0, topic="club")
    kit.bench(B, u["x1"] - 0.3, 18.2, Z, 270.0, w=1.6, kind="park")
    B.thing("game_bench", "seat", (u["x1"] - 0.3, 18.2, Z + 0.47), 270.0, height=0.45)
    _stand(B, "game_bench_spot", "bench", u["x1"] - 0.9, 18.2, Z, 270.0)
    kit.wall_shelf(B, -3.6, YE, Z, 0.0, w=1.3, zt=1.8)
    B.thing("game_trophies", "board", (-3.6, YE - 0.1, Z + 2.0), 0.0, topic="exhibit")
    kit.wall_clock(B, u["x1"] - 0.1, 12.5, Z, 270.0, zc=2.4, r=0.22, face="clock_3")
    kit.rug(B, -4.2, 11.9, Z + 0.01, 0.0, w=4.6, d=0.9, name="rug_3", seed=3)
    kit.pendant(B, -3.6, 12.6, Z_SH, drop=0.5, kind="cone")
    kit.pendant(B, -3.6, 15.0, Z_SH, drop=0.5, kind="cone", light=True, lumens=900, colour="#FFE8C0", lamp_kind="public", radius=9.0)


def _u_news(B, u):
    Z = ZS
    _fascia(B, "news", "sign_books", 1.8)
    _counter(B, "news_counter", 1.8, 9.55, Z, 1.9, "bookshop", "CT_PlasterBlue", spots="news")
    kit.newspaper(B, 1.2, 9.55, Z + 1.05, 20.0, seed=3)
    kit.magazine_rack(B, u["x0"] + 0.1, 11.4, Z, 90.0, w=1.0)
    B.thing("news_mags", "shelf", (u["x0"] + 0.3, 11.4, Z + 0.9), 90.0, topic="travel")
    kit.bookshelf(B, u["x1"] - 0.1, 11.6, Z, 270.0, w=1.2, topic="local", seed=4)
    B.thing("news_books", "shelf", (u["x1"] - 0.3, 11.6, Z + 1.0), 270.0, topic="local")
    kit.wall_map(B, 1.8, YU, Z, 0.0, w=1.3, zc=1.8, name="map_town")
    kit.poster(B, u["x0"] + 0.1, 13.0, Z, 90.0, name="poster_cons_1", w=0.6, zc=1.7)
    kit.rug(B, 1.8, 11.4, Z + 0.01, 0.0, w=1.4, d=1.4, name="rug_1", seed=1)
    B.thing("news_map", "board", (1.8, YU - 0.05, Z + 1.8), 0.0, topic="travel")
    kit.pendant(B, 1.8, 10.8, Z_SH, drop=0.5, kind="cone", light=True, lumens=450, colour="#FFE0B0", lamp_kind="public", radius=4.5)


def _u_groc(B, u):
    Z = ZS
    _fascia(B, "groc", "sign_milk", 1.8)
    kit.awning(B, 5.4, YW, Z, 0.0, w=2.8, proj=0.6, zt=2.95, mat="CT_PlasterGreen", mat2="CT_TileWhite")
    _counter(B, "groc_counter", 5.4, 9.55, Z, 1.9, "corner_grocery", "CT_PlasterGreen", spots="groc")
    kit.shop_shelf(B, u["x0"] + 0.1, 11.6, Z, 90.0, w=1.7, goods="boxes", levels=3, seed=7)
    kit.shop_shelf(B, u["x1"] - 0.1, 11.6, Z, 270.0, w=1.7, goods="boxes", levels=3, seed=8)
    kit.poster(B, 5.4, YU, Z, 0.0, name="poster_health", w=0.7, zc=1.9)
    kit.rug(B, 5.4, 11.6, Z + 0.01, 0.0, w=1.4, d=1.6, name="rug_7", seed=7)
    kit.crate(B, 4.3, 7.2, Z, 0.0, fill="CT_PlasterRed", seed=1)
    B.thing("groc_apples", "machine", (4.6, 7.2, Z + 0.4), 0.0, what="fruit_crates")
    kit.kitchen_scales(B, 4.7, 9.55, Z + 1.05, 0.0)
    kit.pendant(B, 5.4, 10.8, Z_SH, drop=0.5, kind="cone", light=True, lumens=450, colour="#FFE0B0", lamp_kind="public", radius=4.5)


def _u_toys(B, u):
    Z = ZS
    _fascia(B, "toys", "sign_toys", 2.2)
    kit.awning(B, 10.8, YW, Z, 0.0, w=4.6, proj=0.7, zt=2.95, mat="CT_PlasterYellow", mat2="CT_TileWhite")
    _counter(B, "toys_counter", 12.0, 10.4, Z, 2.0, "toy_shop", "CT_PlasterYellow", spots="toys")
    kit.display_case(B, 8.0, 10.5, Z, 90.0, w=1.3, goods="toys", seed=3)
    kit.toy_house(B, 8.5, 13.0, Z, 0.0)
    B.thing("toys_house", "machine", (8.5, 13.0, Z + 0.7), 0.0, what="toy_house")
    kit.toy_bin(B, 10.6, 13.2, Z, 0.0, seed=1)
    B.thing("toys_bins", "machine", (11.0, 13.2, Z + 0.4), 0.0, what="toy_bin")
    kit.poster(B, u["x1"] - 0.1, 12.2, Z, 270.0, name="kids_1", w=0.7, zc=1.8)
    kit.poster(B, 11.0, YU, Z, 0.0, name="kids_3", w=0.8, zc=1.8)
    kit.rug(B, 9.8, 12.0, Z + 0.01, 0.0, w=2.6, d=2.4, name="rug_round_0", round=True)
    kit.pendant(B, 10.8, 11.0, Z_SH, drop=0.55, kind="opal", light=True, lumens=450, colour="#FFE0B0", lamp_kind="public", radius=4.5)


def _u_barber(B, u):
    Z = ZS
    _fascia(B, "barb", "sign_barber", 2.2)
    for k, cy in enumerate((10.7, 12.4)):
        kit.barber_chair(B, 16.2, cy, Z, 270.0)
        kit.mirror(B, u["x0"] + 0.1, cy, Z, 90.0, zc=1.5, w=0.8, h=1.1)
        B.thing("barber_chair_%d" % (k + 1), "machine", (16.2, cy, Z + 0.7), 270.0, what="haircut")
        _stand(B, "barber_chair_%d" % (k + 1), "seat", 16.2, cy, Z, 270.0)
    kit.wall_shelf(B, u["x0"] + 0.1, 11.55, Z, 90.0, w=1.2, zt=1.0)
    kit.menu_board(B, 18.0, YU, Z, 0.0, name="sign_barber", w=1.0)
    kit.picture(B, 16.8, YU, Z, 0.0, name="painting_portrait", w=0.6, zc=1.7)
    kit.rug(B, 17.4, 11.6, Z + 0.01, 0.0, w=2.2, d=3.0, name="rug_3", seed=8)
    kit.reception_desk(B, 19.9, 9.6, Z, 0.0, w=1.4)
    B.thing("barber_desk", "counter", (19.9, 9.6, Z + 1.0), 0.0, role="barber", service="reception")
    _stand(B, "barber_desk_staff", "work", 19.9, 10.5, Z, 0.0)
    kit.bench(B, u["x1"] - 0.3, 12.2, Z, 270.0, w=1.6, kind="park")
    B.thing("barber_bench", "seat", (u["x1"] - 0.3, 12.2, Z + 0.47), 270.0, height=0.45)
    kit.magazine_rack(B, u["x1"] - 0.1, 13.4, Z, 270.0, w=0.8)
    B.thing("barber_mags", "shelf", (u["x1"] - 0.3, 13.4, Z + 0.9), 270.0, topic="art")
    kit.pendant(B, 18.0, 11.5, Z_SH, drop=0.55, kind="opal", light=True, lumens=450, colour="#FFE0B0", lamp_kind="public", radius=4.5)



def _flat(B):
    """Flat 5-12, the home of Priya Tulegenova (75, a pensioner, who likes her bench, the sunrise and the shops of the street
    she lives over): level 2 (z 21) is the living room at the glass, the stair hall, the kitchen at the back; level 1 (z 18)
    the bedroom with her sewing corner, the postcard shelf and a reading chair by the back window."""
    A, Zb = FL2, FL1
    xm = (FX0 + FX1) / 2
    # ---------------------------------------------------------------- level 2: living room
    kit.curtains(B, xm, YC0, A, 180.0, w=2.6, h=2.3, mat="CT_PlasterYellow")
    kit.tv_set(B, FX0 + 0.3, 7.3, A, 90.0)
    B.thing("tul_tv", "screen", (FX0 + 0.3, 7.3, A + 0.85), 90.0, what="tv")
    kit.sofa(B, FX1 - 0.45, 7.2, A, 270.0, seats=2, mat="CT_PlasterRed")
    B.thing("tul_sofa", "seat", (FX1 - 0.45, 7.2, A + 0.47), 270.0, height=0.45)
    kit.armchair(B, -8.45, 5.7, A, 0.0, mat="CT_PlasterGreen")
    B.thing("tul_armchair", "seat", (-8.45, 5.7, A + 0.47), 0.0, height=0.45)
    ct = kit.coffee_table(B, -9.15, 7.2, A, 90.0)
    kit.teapot(B, -9.15, 7.0, ct["top"], 0.0)
    kit.cup(B, -9.1, 7.4, ct["top"], 0.0)
    kit.newspaper(B, -9.2, 7.55, ct["top"], 15.0, seed=5)
    kit.rug(B, -9.0, 7.0, A + 0.01, 0.0, w=1.7, d=2.3, name="rug_4", seed=4)
    kit.floor_lamp(B, FX0 + 0.3, 5.3, A, 0.0)
    kit.plant(B, FX1 - 0.3, 5.3, A, 0.0, kind="fern", pot="CT_PlasterBlue", h=0.9, seed=12)
    kit.picture(B, FX1, 7.2, A, 270.0, name="painting_lighthouse", w=0.7, zc=1.7)
    kit.notice_board(B, FX0, 5.55, A, 90.0, w=0.8, zc=1.6, seed=62)
    for k, nm in enumerate(("travel_0", "travel_2", "travel_3", "travel_4")):
        kit.poster(B, FX0 + 0.04, 5.3 + 0.22 * (k % 2) + 0.0, A + 0.18 * (k // 2) - 0.1, 90.0, name=nm, w=0.2, zc=1.62 + 0.0)
    B.thing("tul_postcards", "board", (FX0 + 0.08, 5.55, A + 1.6), 90.0, topic="travel")
    B.thing("tul_window", "board", (xm, YC0 + 0.05, A + 1.6), 180.0, topic="view")
    # the stair hall of level 2: pictures on the lanes' walls
    kit.picture(B, FX0, 10.6, A, 90.0, name="painting_dunes", w=0.6, zc=1.7)
    kit.picture(B, FX1, 11.2, A, 270.0, name="painting_pines", w=0.6, zc=1.7)
    B.thing("tul_photos", "board", (FX1 - 0.06, 11.2, A + 1.7), 270.0, topic="local")
    # ---------------------------------------------------------------- level 2: kitchen
    kit.kitchen_unit(B, FX1 - 0.3, 16.6, A, 270.0, modules="cupboard,sink,stove,fridge", wall_cupboards=False)
    B.thing("tul_sink", "machine", (FX1 - 0.3, 16.8, A + 1.0), 270.0, what="sink")
    B.thing("tul_stove", "machine", (FX1 - 0.3, 16.2, A + 1.0), 270.0, what="stove")
    # reach audit: a smaller table at the edge of the floor (x -9.6, the void over the bedroom beyond) leaves 0.75 m free
    # to the kitchen unit (was 0.34 m: the back of the kitchen, the sink and the radio could not be reached)
    tb = kit.table(B, -9.3, 16.8, A, 0.0, w=0.6, d=0.7)
    kit.samovar(B, -9.3, 16.8, tb["top"], 0.0)
    B.thing("tul_samovar", "machine", (-9.3, 16.8, A + 1.05), 0.0, what="samovar")
    kit.chair(B, -9.3, 16.05, A, 180.0, kind="wood")
    B.thing("tul_kitchen_seat", "seat", (-9.3, 16.05, A + 0.45), 180.0, height=0.45)
    kit.chair(B, -9.3, 17.55, A, 0.0, kind="wood")
    bt = kit.bedside_table(B, -8.0, 19.3, A, 0.0, lamp=False)
    kit.radio(B, -8.0, 19.3, bt["top"] or 0.55 + A, 0.0)
    B.thing("tul_radio", "machine", (-8.0, 19.3, A + 0.7), 0.0, what="radio")
    kit.wall_clock(B, FX1, 14.8, A, 270.0, zc=2.0, r=0.17, face="clock_0")
    kit.plant(B, FX0 + 0.35, 19.3, A, 0.0, kind="ficus", pot="CT_PlasterRed", h=1.2, seed=13)
    # ---------------------------------------------------------------- level 1: bedroom, sewing corner, shelf, reading chair
    kit.curtains(B, xm, YC0, Zb, 180.0, w=2.6, h=2.3, mat="CT_PlasterBlue")
    kit.bed(B, FX0 + 0.55, 7.6, Zb, 0.0, size="single", blanket="CT_PlasterGreen")
    B.thing("tul_bed", "bed", (FX0 + 0.55, 7.6, Zb + 0.5), 0.0, rest="private", height=0.5)
    kit.bedside_table(B, FX0 + 0.3, 8.95, Zb, 0.0, lamp=True)
    kit.wardrobe(B, FX1 - 0.0, 8.2, Zb, 270.0, w=1.0)
    B.thing("tul_wardrobe", "board", (FX1 - 0.3, 8.2, Zb + 1.2), 270.0, topic="store")
    kit.sewing_table(B, -8.7, 5.5, Zb, 0.0)
    B.thing("tul_sewing", "craft", (-8.7, 5.5, Zb + 0.95), 0.0, what="sewing_machine")
    kit.stool(B, -8.7, 6.25, Zb, 180.0)
    B.thing("tul_sewing_stool", "seat", (-8.7, 6.25, Zb + 0.45), 0.0, height=0.45)
    kit.rug(B, -9.0, 7.0, Zb + 0.01, 0.0, w=1.5, d=2.4, name="rug_5", seed=5)
    kit.bookshelf(B, FX1, 11.6, Zb, 270.0, w=1.2, topic="travel", seed=6)
    B.thing("tul_album", "shelf", (FX1 - 0.3, 11.6, Zb + 1.0), 270.0, topic="travel")
    kit.picture(B, FX0, 12.4, Zb, 90.0, name="painting_sea_night", w=0.7, zc=1.7)
    B.thing("tul_window_low", "board", (xm, YC0 + 0.05, Zb + 1.5), 180.0, topic="view")
    kit.armchair(B, -8.3, 18.8, Zb, 180.0, mat="CT_PlasterYellow")
    B.thing("tul_nook", "seat", (-8.3, 18.8, Zb + 0.47), 180.0, height=0.45)
    kit.floor_lamp(B, FX1 - 0.3, 17.3, Zb, 0.0, light=True)
    kit.plant(B, FX1 - 0.3, 14.6, Zb, 0.0, kind="fern", pot="CT_PlasterGreen", h=0.9, seed=14)
    kit.books_stack(B, -8.3, 18.0, Zb, 0.0, n=3, seed=2)
    # lamps: one per room of the flat (level 2 living room, stair hall, kitchen; level 1 bedroom, reading nook)
    FP = dict(light=True, lumens=300, colour="#FFD9A8", lamp_kind="public", radius=4.0)
    kit.pendant(B, -9.0, 6.4, ZS - 0.3, drop=0.4, kind="opal", **FP)
    kit.pendant(B, -7.9, 11.3, ZS - 0.3, drop=0.3, kind="opal", **FP)
    kit.pendant(B, -9.0, 16.8, ZS - 0.3, drop=0.5, kind="cone", **FP)
    kit.pendant(B, -9.0, 7.2, FL2 - 0.3, drop=0.4, kind="opal", **FP)
    kit.pendant(B, -8.4, 12.4, FL2 - 0.3, drop=0.3, kind="opal", **FP)
    # ---------------------------------------------------------------- townsfolk: Priya's places
    B.spot("tul_sofa_spot", "bench", (FX1 - 0.9, 7.2, A), 270.0)
    B.spot("tul_window_spot", "view", (xm, 5.7, A), 180.0)
    B.spot("tul_table_spot", "table", (-9.0, 15.95, A), 180.0)
    B.spot("tul_sewing_spot", "table", (-8.7, 6.25, Zb), 0.0)
    B.spot("tul_nook_spot", "bench", (-8.3, 18.2, Zb), 180.0)



def _circle(cx, cy, r, n=16):
    return [(cx + r * math.cos(2 * math.pi * k / n), cy + r * math.sin(2 * math.pi * k / n)) for k in range(n)]


def _roof2(B, zr):
    """Round 2 on the roof terrace: a paddling pool with deck chairs, the running track's start with a lap board, a badminton
    court with a net, the kindergarten's sandbox and playhouse, a bird feeder by the pergola, a panorama at the parapet."""
    d = B.detail
    # ---- paddling pool (no collision: the children wade in), a low white rim, blue water, a towel on the edge
    pc = (-38.5, 8.5)
    ring = _circle(pc[0], pc[1], 2.1) + [_circle(pc[0], pc[1], 2.1)[0]]
    inner = _circle(pc[0], pc[1], 1.8) + [_circle(pc[0], pc[1], 1.8)[0]]
    ribbon_wall(d, "CT_ConcreteWhite", ring, zr, zr + 0.34)
    ribbon_wall(d, "CT_ConcreteWhite", inner[::-1], zr + 0.0, zr + 0.34)
    band(d, "CT_ConcreteWhite", ring, inner, zr + 0.34)
    d.face("CT_PlasterBlue", [(x, y, zr + 0.26) for x, y in _circle(pc[0], pc[1], 1.8)][::-1][::-1])
    box(d, "CT_PlasterRed", pc[0] + 1.5, pc[0] + 2.2, pc[1] - 0.2, pc[1] + 0.3, zr + 0.34, zr + 0.37, faces="Zx")
    B.thing("roof_pool", "machine", (pc[0], pc[1], zr + 0.4), 0.0, what="paddling_pool")
    B.spot("roof_pool_edge", "view", (pc[0], pc[1] + 2.6, zr), 0.0)
    for k, dx in enumerate((-2.6, -0.4, 1.8)):
        kit.deck_chair(B, pc[0] + dx, 12.2, zr, 0.0, mat=("CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow")[k])
        B.thing("roof_deck_%d" % (k + 1), "seat", (pc[0] + dx, 12.4, zr + 0.4), 0.0, height=0.37)
        B.spot("roof_deck_%d" % (k + 1), "bench", (pc[0] + dx, 12.4, zr), 0.0)
    # ---- the running track: a start line, a lap board in the infield
    box(d, "CT_ConcreteWhite", -15.08, -14.92, 4.5, 7.3, zr + 0.06, zr + 0.065, faces="Z")
    box(d, "CT_ConcreteWhite", 14.92, 15.08, 4.5, 7.3, zr + 0.06, zr + 0.065, faces="Z")
    B.thing("roof_track", "sport", (-15.0, 5.9, zr + 0.9), 90.0, what="track")
    kit.notice_stand(B, -14.0, 8.4, zr, 0.0, w=1.2, front="chart_calendar", back="notice", seed=2)
    B.thing("roof_laps", "board", (-14.0, 8.4, zr + 1.5), 0.0, topic="local")
    B.spot("roof_track_stand", "stand", (-13.5, 5.9, zr), 90.0)
    # ---- a badminton court with a net across its middle
    cx, cy = 33.3, 8.2
    kit.court_lines(B, cx, cy, zr + 0.06, 90.0, kind="badminton", w=5.2, d=9.4)
    for yy in (cy - 2.7, cy + 2.7):
        fu.rod(d, "CT_Steel", (cx, yy, zr + 0.06), (cx, yy, zr + 1.55), 0.025)
    for k in range(5):
        zz = zr + 0.80 + k * 0.15
        fu.rod(d, "CT_ConcreteWhite", (cx, cy - 2.7, zz), (cx, cy + 2.7, zz), 0.008)
    fu.rod(d, "CT_ConcreteWhite", (cx, cy - 2.7, zr + 1.55), (cx, cy + 2.7, zr + 1.55), 0.02)
    B.thing("roof_badminton", "play", (cx, cy, zr + 1.0), 90.0, what="badminton")
    B.spot("roof_badminton_a", "stand", (cx - 2.2, cy, zr), 90.0)
    B.spot("roof_badminton_b", "stand", (cx + 2.2, cy, zr), 270.0)
    # ---- the kindergarten's sandbox (the wooden frame is the shell's), sand, a bucket, a castle, the playhouse
    sx0, sx1, sy0, sy1 = -53.0, -50.0, 6.0, 9.0
    box(d, "CT_PlasterYellow", sx0 + 0.1, sx1 - 0.1, sy0 + 0.1, sy1 - 0.1, zr + 0.26, zr + 0.28, faces="Z")
    for k, (w_, h_) in enumerate(((0.7, 0.12), (0.5, 0.2), (0.3, 0.28))):
        box(d, "CT_PlasterYellow", -51.9 - w_ / 2, -51.9 + w_ / 2, 7.2 - w_ / 2, 7.2 + w_ / 2, zr + 0.28 + [0, 0.12, 0.32][k], zr + 0.28 + [0.12, 0.32, 0.60][k], faces="xXyYZ")
    box(d, "CT_PlasterRed", -51.0, -50.8, 8.0, 8.2, zr + 0.28, zr + 0.41, faces="xXyYZ")
    fu.rod(d, "CT_Steel", (-51.4, 8.3, zr + 0.29), (-51.2, 8.6, zr + 0.55), 0.015)
    B.thing("roof_sandbox", "craft", (-51.5, 7.5, zr + 0.5), 0.0, what="sandbox")
    B.spot("roof_sandbox_spot", "stand", (-49.7, 7.5, zr), 90.0)
    kit.toy_house(B, -47.0, 9.0, zr, 0.0)
    B.thing("roof_playhouse", "machine", (-47.0, 9.0, zr + 0.7), 0.0, what="toy_house")
    kit.bench(B, -48.5, 5.0, zr, 180.0, w=1.6, kind="park")
    B.thing("roof_kg_bench", "seat", (-48.5, 5.0, zr + 0.47), 180.0, height=0.45)
    B.spot("roof_kg_bench", "bench", (-48.5, 5.6, zr), 180.0)
    # ---- a bird feeder by the pergola, the panorama at the parapet
    kit.bird_feeder(B, 55.5, 8.0, zr, 0.0, kind="post")
    B.thing("roof_feeder", "machine", (55.5, 8.0, zr + 1.4), 0.0, what="bird_feeder")
    B.thing("roof_panorama", "board", (26.0, 3.0, zr + 1.2), 0.0, topic="view")
    B.spot("roof_panorama_spot", "view", (26.0, 3.8, zr), 0.0)


def _lobby2(B):
    """Round 2 in both lobbies: the letter boxes and the light switch get their things, the hat stand its rain, lobby 1's bicycle
    and pram, lobby 2's watering can."""
    Z = 0.02
    for k, (a, b, lid, cmat) in enumerate(CORES):
        xa, xz = xb(a), xb(b)
        xm = (xa + xz) / 2
        pre = lid + "_"
        sa = 1 if k == 1 else -1
        sb = -sa
        xl, xr = xa + 0.3, xz - 0.3
        ax = lambda s, off: xl + off if s > 0 else xr - off
        # the letter boxes on the left wall (y 4.6..6.0)
        B.thing(pre + "mail", "board", (xl + 0.12, 5.3, 1.3), 90.0, topic="mail")
        # a light switch on the front wall between the window and the door
        sx = xa + 2.2
        box(B.detail, "CT_ConcreteWhite", sx - 0.04, sx + 0.04, 1.3, 1.315, 1.24, 1.36, faces="YXxzZ")
        box(B.detail, "CT_Steel", sx - 0.015, sx + 0.015, 1.315, 1.325, 1.28, 1.32, faces="YXxzZ")
        B.thing(pre + "switch", "machine", (sx, 1.33, 1.3), 180.0, what="switch", switch="hall")
        # the hat stand by the door
        B.thing(pre + "hats", "machine", (xm + 1.7, 1.9, 1.2), 0.0, what="hat_stand")
        if k == 0:
            B.thing(pre + "bike", "machine", (ax(sb, 0.55), 8.1, 0.65), 90.0, what="bike_rack")
            B.thing(pre + "pram", "machine", (ax(sa, 0.55), 1.9, 0.7), 0.0, what="pram")
        else:
            kit.planter(B, xm - 1.6, 1.75, Z, 0.0, w=1.2, d=0.45, h=0.5, seed=77)
            B.thing(pre + "plants", "machine", (xm - 1.6, 1.75, 0.75), 0.0, what="watering_can")



from _wallhang import hung  # noqa: E402  (QA r9: nothing hangs over the glass)
build = hung(build)
