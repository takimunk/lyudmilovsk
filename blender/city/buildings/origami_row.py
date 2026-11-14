"""Origami row: seven narrow three-storey terrace houses whose roof and street walls fold into ONE continuous zigzag
concrete plate. An ORIGINAL design in the spirit of the experimental folded-plate housing of the 1960s-70s.

Plot 40 x 30, height_max 18. Footprint 37.8 x 12.65 m (x -18.9..18.9, y 0.3..12.95), highest ridge 12.2 m.
Seven houses of 5.4 m (centre lines of the party walls at x = -18.9 + 5.4 k). Each house is ONE flat ("0-01".."0-07"),
three storeys (floors z 0, 3, 6), with its own street door in the ground-floor wall (y 2.6) and a dog-leg stair
along the party wall at the back (rise 0.15, going 0.30, two half-flights a storey, landings with slit windows).
The folded plate: a raw-concrete roof 22 cm thick zigzags along x (valley over every party wall, ridge over the
middle of every house; ridges alternate 10.3..12.2 m). Below it the upper street walls are folded in plan too: a
chevron (1.2 m deep) that carries the roof fold down to the first-floor slab, which overhangs the doors. Concrete
ribs run up each ridge fold line, concrete blades sit in each valley notch. The top storey is a glazed gable: one
sloping-topped window card per facet (cut to the roof line). Walls are faded plaster, one colour per house;
the rear gable is a plastered pentagon. A low plaster garden wall with a gate and piers closes each front garden.
Windows are all cards (flat windows on the flat id, "stair" slits at the landings). Entrances door_1..door_7.
Material requests: none.
"""
import math

import interior_kit as kit
from citykit import box, prism, triangulate, DOOR_H
import _homekit as hk
from _cons_util import wall, win
from _sheet import thick_sheet
from _surrounds import add_surrounds

INFO = {
    "name_ru": "Дом-оригами (ряд складчатых домов)", "name_en": "Origami Row",
    "style": "experimental folded-plate", "use": "residential",
    "inspired_by": "1960s-70s experimental folded-plate concrete housing: terrace houses whose roofs and street "
                   "walls fold into one zigzag plate; original massing and proportions",
}

N = 7
W = 5.4
X0 = -18.9
XE = [X0 + W * k for k in range(N + 1)]
YF, YC = 2.6, 1.4                 # street wall line, chevron apex
YB = 12.6                         # rear wall outer face
TH = 0.3                          # wall thickness
PT = 0.22                         # plate thickness
SL = 0.25                         # floor slab thickness
ZV = [8.7, 8.6, 8.9, 8.6, 8.8, 8.6, 8.9, 8.7]            # valley heights over the party walls
ZR = [11.8, 10.3, 12.2, 10.6, 11.9, 10.4, 12.0]           # ridge heights (alternate)
COL = ["CT_PlasterPink", "CT_PlasterYellow", "CT_PlasterBlue", "CT_PlasterGreen", "CT_Plaster", "CT_PlasterRed",
       "CT_PlasterGrey"]
SLOPE = (YF - YC) / (W / 2)
FL = math.hypot(W / 2, YF - YC)      # facet length
YS = 7.25                            # front edge of the stairwell void
FAMILY = 2                           # the furnished house (index; flat 0-03, the blue one)
CEIL0, CEIL1 = 2.75, 5.75            # ceilings (floor slab undersides) of the ground and first floor


def roof_z(x):
    j = (x - X0) / (W / 2)
    j0 = max(0, min(2 * N - 1, int(math.floor(j + 1e-9))))
    t = j - j0
    za = ZV[j0 // 2] if j0 % 2 == 0 else ZR[j0 // 2]
    zb = ZR[j0 // 2] if j0 % 2 == 0 else ZV[j0 // 2 + 1]
    return za + (zb - za) * t


def chev(i, x):
    return YC + SLOPE * abs(x - (XE[i] + W / 2))


def _ccw(poly):
    a = sum(poly[k][0] * poly[(k + 1) % len(poly)][1] - poly[(k + 1) % len(poly)][0] * poly[k][1] for k in range(len(poly)))
    return poly if a > 0 else poly[::-1]


def slab_poly(m, poly, z0, z1, top, bot, side):
    poly = _ccw(poly)
    n = len(poly)
    for k in range(n):
        (ax, ay), (bx, by) = poly[k], poly[(k + 1) % n]
        m.face(side, [(ax, ay, z0), (bx, by, z0), (bx, by, z1), (ax, ay, z1)])
    for a, b, c in triangulate(poly):
        m.face(top, [(poly[a][0], poly[a][1], z1), (poly[b][0], poly[b][1], z1), (poly[c][0], poly[c][1], z1)])
        m.face(bot, [(poly[c][0], poly[c][1], z0), (poly[b][0], poly[b][1], z0), (poly[a][0], poly[a][1], z0)])


def cap_quad(m, mat, a, b, c, d, z):
    poly = _ccw([a, b, c, d])
    m.face(mat, [(x, y, z) for x, y in poly])


def _trap_bars(B, q, n, inset, cols, rows, t):
    """Frame bars over a trapezoid card (the top edge slopes with the roof): bilinear between the side edges."""
    p0, p1, p2, p3 = [(p[0] - n[0] * inset + n[0] * 0.03, p[1] - n[1] * inset + n[1] * 0.03, p[2]) for p in q]

    def P(a, b):
        lo = [p0[k] + (p3[k] - p0[k]) * b for k in range(3)]
        hi = [p1[k] + (p2[k] - p1[k]) * b for k in range(3)]
        return tuple(lo[k] + (hi[k] - lo[k]) * a for k in range(3))
    w = math.dist(p0, p1)
    h = min(math.dist(p0, p3), math.dist(p1, p2))
    ta, tb = t / w, t / h
    bars = [(0, 0, 1, tb), (0, 1 - tb, 1, 1), (0, 0, ta, 1), (1 - ta, 0, 1, 1)]
    for i in range(1, cols):
        a = i / cols
        bars.append((a - ta / 2, 0, a + ta / 2, 1))
    for j in range(1, rows):
        b = j / rows
        bars.append((0, b - tb / 2, 1, b + tb / 2))
    for a0, b0, a1, b1 in bars:
        B.detail.face("CT_Frame", [P(a0, b0), P(a1, b0), P(a1, b1), P(a0, b1)])


def build(B):
    m, det = B.m, B.detail
    for i in range(N):
        _house(B, i)
    # ---------------------------------------------------------------- party walls (raw concrete, plaster inside)
    for k in range(1, N):
        x = XE[k]
        zt = ZV[k] - 0.05
        box(m, "CT_Interior", x - 0.15, x + 0.15, YF + 0.02, 12.45, 0.0, zt, faces="xXz")
        box(m, "CT_Concrete", x - 0.15, x + 0.15, YF + 0.02, 12.45, 0.0, zt, faces="yYZ")
        # valley blade in the street notch
        box(m, "CT_Concrete", x - 0.09, x + 0.09, 2.1, 2.66, 2.75, ZV[k] - 0.1, faces="xXyz")
    for k, sgn in ((0, 1), (N, -1)):                                       # end walls, coloured outside
        x = XE[k]
        zt = ZV[k] - 0.05
        xa, xb = (x, x + TH) if sgn > 0 else (x - TH, x)
        face_out, face_in = ("x", "X") if sgn > 0 else ("X", "x")
        box(m, "CT_Interior", xa, xb, YF + 0.02, 12.45, 0.0, zt, faces=face_in + "yYZ")
        box(m, COL[0 if k == 0 else N - 1], xa, xb, YF, YB, 0.0, ZV[k] - PT, faces=face_out)
    # ---------------------------------------------------------------- the folded plate (roof)
    nodes = [X0 + (W / 2) * j for j in range(2 * N + 1)]
    zn = [ZV[j // 2] if j % 2 == 0 else ZR[j // 2] for j in range(2 * N + 1)]
    yfr = [YF - 0.55 if j % 2 == 0 else YC - 0.55 for j in range(2 * N + 1)]
    ybk = 12.95

    def f(u, v):
        j = int(round(u * (2 * N)))
        return (nodes[j], yfr[j] + (ybk - yfr[j]) * v, zn[j])
    thick_sheet(m, f, 2 * N, 1, lambda u, v: PT, "CT_Concrete", "CT_Concrete", "CT_Concrete", edges="uUvV")
    # ---------------------------------------------------------------- front garden walls with gates
    for i in range(N):
        gx = _door_x(i)
        c = COL[i]
        xa, xb = XE[i] + 0.12, XE[i + 1] - 0.12
        for (a, b, fc) in ((xa, gx - 0.75, "xyY"), (gx + 0.75, xb, "XyY")):
            if b - a < 0.3:
                continue
            box(m, c, a, b, 0.35, 0.6, 0.0, 0.95, faces=fc)
            box(m, "CT_ConcreteWhite", a - 0.03, b + 0.03, 0.31, 0.64, 0.95, 1.02)
        for px in (gx - 0.95, gx + 0.95):
            box(m, "CT_Concrete", px - 0.15, px + 0.15, 0.3, 0.65, 0.0, 1.3)
    # ---------------------------------------------------------------- surrounds at the doors
    B.footprint = [(X0, 0.3), (-X0, 0.3), (-X0, 12.95), (X0, 12.95)]
    B.height = max(ZR)
    add_surrounds(B, bounds=(X0, -X0, 0.7, 6.0), seed=7, max_lamps=3, hedges=False, bikes=True)
    _family(B, FAMILY)
    B.note("7 flats over 3 storeys, one folded concrete plate roof, rear stairs; house %d furnished (a family with three children)" % (FAMILY + 1))


def _door_x(i):
    u = (0.7, 1.9)
    return XE[i] + (u[0] + u[1]) / 2


def _house(B, i):
    m, det = B.m, B.detail
    xe, xr = XE[i], XE[i + 1]
    xm = xe + W / 2
    col = COL[i]
    fid = "0-%02d" % (i + 1)
    IL = xe + 0.15 if i > 0 else xe + TH
    IR = xr - 0.15 if i < N - 1 else xr - TH
    sg = 1 if i % 2 == 0 else -1

    def X(s):
        return IL + s if sg > 0 else IR - s

    def xs(s0, s1):
        a, b = X(s0), X(s1)
        return (min(a, b), max(a, b))

    # ---------------------------------------------------------------- ground floor
    xa, xb = IL - 0.05, IR + 0.05
    slab_poly(m, [(xa, YF + TH), (xb, YF + TH), (xb, 12.4), (xa, 12.4)], -0.3, 0.0, "CT_Floor", "CT_ConcreteDark", "CT_Concrete")
    even = i % 2 == 0
    du = (0.7, 1.9)
    wu = (2.7, 4.7)
    p0, p1 = (xe, YF), (xr, YF)
    wall(B, p0, p1, 0.0, 2.75, col, holes=[(du[0], du[1], 0.0, DOOR_H + 0.2), (wu[0], wu[1], 0.9, 2.3)], depth=TH,
         mat_in="CT_Interior")
    win(B, p0, p1, wu[0], wu[1], 0.9, 2.3, flat=fid, cols=3, rows=1, inset=0.2, frame=0.05)
    gx = _door_x(i)
    B.entrance("door_%d" % (i + 1), (gx, YF, 0.0), 0.0, width=1.2, height=DOOR_H + 0.2, kind="street")
    box(det, "CT_FrameWhite", gx - 0.62, gx + 0.62, YF - 0.02, YF + 0.02, DOOR_H + 0.2, DOOR_H + 0.26)
    # ---------------------------------------------------------------- the floors with the stairwell void
    for z, front_off in ((3.0, 0.0), (6.0, 0.164)):
        pts = []
        for x in (IL - 0.05, xm, IR + 0.05):
            pts.append((x, chev(i, x) + front_off))
        if sg > 0:
            poly = pts + [(IR + 0.05, 12.4), (X(2.15), 12.4), (X(2.15), YS), (IL - 0.05, YS)]
        else:
            poly = pts + [(IR + 0.05, YS), (X(2.15), YS), (X(2.15), 12.4), (IL - 0.05, 12.4)]
        slab_poly(m, poly, z - SL, z, "CT_Floor", "CT_Interior", "CT_Concrete")
        xx = X(2.25)
        B.railing([(xx, YS + 0.2), (xx, 12.2)], z, h=1.0, post=1.5)
        if z > 5:
            a, b = xs(0.15, 1.12)
            B.railing([(a, YS - 0.05), (b, YS - 0.05)], z, h=1.0, post=1.5)
    # ---------------------------------------------------------------- dog-leg stair (two half-flights a storey)
    for st in (0, 1):
        zf = 3.0 * st
        zl = zf + 1.5
        xa_, xb_ = xs(0.15, 1.12)
        xc_, xd_ = xs(1.18, 2.15)
        for k in range(10):
            ya = 7.3 + 0.3 * k
            top = zf + 0.15 * (k + 1)
            base = 0.0 if st == 0 else top - 0.4
            box(m, "CT_Concrete", xa_, xb_, ya, ya + 0.3, base, top, faces="xXyYZ" if st == 0 else "all")
            yb = 10.3 - 0.3 * k
            top2 = zl + 0.15 * (k + 1)
            base2 = 0.0 if st == 0 else top2 - 0.4
            box(m, "CT_Concrete", xc_, xd_, yb - 0.3, yb, base2, top2, faces="xXyYZ" if st == 0 else "all")
        xl0, xl1 = xs(0.15, 2.15)
        box(m, "CT_Concrete", xl0, xl1, 10.3, 12.3, 0.0 if st == 0 else zl - 0.4, zl,
            faces="xXyYZ" if st == 0 else "all")
    # ---------------------------------------------------------------- the folded street wall (storeys 1-2)
    hl = (0.45, FL - 0.45, 3.9, 5.6)
    for fi, (a, b) in enumerate((((xe, YF), (xm, YC)), ((xm, YC), (xr, YF)))):
        F = wall(B, a, b, 3.0, 6.5, col, holes=[hl], depth=TH, mat_in="CT_Interior")
        win(B, a, b, hl[0], hl[1], hl[2], hl[3], flat=fid, cols=2, rows=1, inset=0.2, frame=0.05)
        # sill cap on the wall top
        # glazed gable: one card per facet, top cut to the roof line
        u0, u1 = 0.4, FL - 0.4
        P = [F.to_building((u, 0.0, 0.0)) for u in (u0, u1)]
        zt0, zt1 = roof_z(P[0][0]) - 0.12, roof_z(P[1][0]) - 0.12
        pts = [(P[0][0], P[0][1], 6.6), (P[1][0], P[1][1], 6.6), (P[1][0], P[1][1], zt1), (P[0][0], P[0][1], zt0)]
        B.window(pts, flat=fid, inset=0.2, frame=0)
        _trap_bars(B, pts, F.n, 0.2, cols=2, rows=3, t=0.06)
    di = TH * math.sqrt(1 + SLOPE ** 2)
    cap = _ccw([(xe, YF), (xm, YC), (xr, YF), (xr, YF + di), (xm, YC + di), (xe, YF + di)])
    for a_, b_, c_ in triangulate(cap):
        m.face("CT_Concrete", [(cap[k][0], cap[k][1], 6.5) for k in (a_, b_, c_)])
    # ---------------------------------------------------------------- ridge rib (fold line)
    rib = [(xm, YC - 0.1), (xm + 0.14, YC + 0.2), (xm, YC + 0.7), (xm - 0.14, YC + 0.2)]
    prism(m, "CT_Concrete", rib, 3.0, ZR[i] - 0.205, top=False, bottom=False)
    # ---------------------------------------------------------------- rear wall, rear gable
    rp0, rp1 = (xr, YB), (xe, YB)
    def u_of(x):
        return xr - x
    holes = []
    a, b = xs(0.5, 1.8)
    for z0, z1 in ((1.9, 3.9), (4.9, 6.9)):
        holes.append((u_of(b), u_of(a), z0, z1))
        win(B, rp0, rp1, u_of(b), u_of(a), z0, z1, kind="stair", cols=1, rows=3, inset=0.2, frame=0.05)
    a, b = xs(3.0, 4.4)
    for z0, z1 in ((0.9, 2.3), (3.9, 5.3), (6.9, 8.2)):
        holes.append((u_of(b), u_of(a), z0, z1))
        win(B, rp0, rp1, u_of(b), u_of(a), z0, z1, flat=fid, cols=2, rows=1, inset=0.2, frame=0.05)
    wall(B, rp0, rp1, 0.0, 8.4, col, holes=holes, depth=TH, mat_in="CT_Interior")
    zl_, zr_ = ZV[i] - 0.1, ZV[i + 1] - 0.1
    pent = [(xr, 8.4), (xe, 8.4), (xe, zl_), (xm, ZR[i] - 0.1), (xr, zr_)]
    m.face(col, [(x, YB, z) for x, z in pent])
    m.face("CT_Interior", [(x, YB - TH, z) for x, z in reversed(pent)])
    # ---------------------------------------------------------------- flat and lamps
    B.flat(fid, storey=0, beds=3, door=(gx, 4.0, 0.0), entrance="door_%d" % (i + 1))
    B.lamp((gx, 4.6, 2.55), lumens=450, colour="#FFD9A8", kind="lobby", radius=5.5)
    B.lamp((X(1.15), 9.2, 5.3), lumens=380, colour="#FFE2B8", kind="stair", radius=5.5)


# ================================================================================================ interiors
def _family(B, i):
    """House i as the home of a family of five (parents, two school children, a baby): ground floor hall, living room,
    dining kitchen; first floor parents' room with the cot and the children's bunks; the loft: father's chess and
    telescope corner, mother's sewing machine, a playroom under the folded roof."""
    xe, xr = XE[i], XE[i + 1]
    xm = xe + W / 2
    IL = xe + 0.15 if i > 0 else xe + TH
    IR = xr - 0.15 if i < N - 1 else xr - TH
    assert i % 2 == 0, "the layout is for a house whose stair runs along the west wall"
    XS = IL + 2.15                                   # east edge of the stair block / the stairwell void
    f = lambda fn, x, y, yaw=0.0, z=0.0, **kw: fn(B, x, y, z, yaw, **kw)
    EW, WW, RW, FW = IR - 0.02, IL + 0.02, 12.28, YF + TH + 0.02          # wall lines for [wall] pieces

    def floor_poly(front_off):
        pts = [(IL, chev(i, IL) + 0.38 + front_off), (xm, chev(i, xm) + 0.38 + front_off),
               (IR, chev(i, IR) + 0.38 + front_off)]
        return pts + [(IR, 12.3), (XS + 0.1, 12.3), (XS + 0.1, YS - 0.05), (IL, YS - 0.05)]

    ground = [(IL, YF + TH), (IR, YF + TH), (IR, 12.3), (XS, 12.3), (XS, 7.3), (IL, 7.3)]
    FALLS = {0: ground, 1: floor_poly(0.0), 2: floor_poly(0.164)}
    tests = {k: hk.poly_test(v) for k, v in FALLS.items()}
    yaw_e, yaw_w, yaw_n, yaw_s = 270.0, 90.0, 180.0, 0.0     # facing -x / +x / +y / -y

    def run(level, fn):
        H = hk.Home(B, lambda x, y: tests[level](x, y, 0.02))
        H.n0 = len(getattr(B, "kit_items", []))
        fn(H)
        H.check("origami-%d" % level, overlaps=False)
        return H

    # ------------------------------------------------------------------------------------ ground floor
    def ground_floor(H):
        z = 0.0
        gx = _door_x(i)
        # hall: a rug at the door, shoes, coats, the bike and the pram along the west wall
        f(kit.rug, gx, 3.65, 0.0, w=1.2, d=0.8, name="rug_5")
        f(kit.shoe_rack, WW, 3.45, 90.0, w=0.9)
        f(kit.hat_stand, -5.85, 3.35, 0.0)
        hk.bike(B, -7.45, 6.2, 0.0, mat="CT_PlasterYellow")
        hk.pram(B, -7.55, 4.45, 0.0, mat="CT_PlasterBlue")
        f(kit.picture, WW, 4.3, 90.0, name="painting_still_life", w=0.5, zc=1.75)
        # living room: sofa facing the TV on the east wall, armchair under the window, coffee table, rug, shelf
        f(kit.rug, -4.3, 5.0, 90.0, w=2.2, d=2.6, name="rug_1")
        sofa = f(kit.sofa, -5.35, 5.2, 90.0, seats=3, mat="CT_PlasterRed")
        ct = f(kit.coffee_table, -4.25, 5.2, 90.0, w=0.9, d=0.5)
        f(kit.books_stack, -4.25, 4.95, 0.0, z=0.42, n=3, seed=2)
        f(kit.cup, -4.3, 5.5, 0.0, z=0.42)
        tv = f(kit.tv_set, EW - 0.25, 4.7, 270.0, screen="screen_tv_sea")
        arm = f(kit.armchair, -3.8, 3.5, 180.0, mat="CT_PlasterGreen")
        f(kit.floor_lamp, -3.12, 3.4, 0.0)
        sh = f(kit.bookshelf, EW, 6.45, 270.0, w=0.9, h=1.7, levels=4, topic="kids", seed=4)
        cr = f(kit.crate, -3.6, 6.9, 20.0, w=0.5, d=0.35, h=0.3, fill="CT_PlasterBlue", seed=1)
        f(kit.poster, FW, -5.9, 180.0, name="kids_2", w=0.5, zc=1.6)
        f(kit.plant, -4.9, 3.55, 0.0, kind="monstera", pot="CT_PlasterRed", h=0.8, seed=3)
        # dining kitchen: table with five chairs, kitchen run on the rear wall
        dt = f(kit.table, -4.3, 9.4, 90.0, w=1.5, d=0.85)
        f(kit.plates, -4.55, 9.2, 0.0, z=0.75, n=3)
        f(kit.vase, -4.3, 9.6, 0.0, z=0.75)
        f(kit.teapot, -4.1, 9.2, 0.0, z=0.75)
        c1 = f(kit.chair, -5.05, 8.95, 90.0, kind="wood", mat="CT_PlasterBlue")
        c2 = f(kit.chair, -5.05, 9.85, 90.0, kind="wood", mat="CT_PlasterYellow")
        c3 = f(kit.chair, -3.55, 8.95, 270.0, kind="wood", mat="CT_PlasterGreen")
        c4 = f(kit.chair, -3.55, 9.85, 270.0, kind="wood", mat="CT_PlasterRed")
        c5 = f(kit.chair, -4.3, 10.55, 0.0, kind="wood", mat="CT_PlasterPink")
        ku = f(kit.kitchen_unit, -4.3, RW, 0.0, modules="fridge,cupboard,sink,stove", wall_cupboards=False)
        f(kit.samovar, -4.05, 12.0, 0.0, z=0.9)
        sb = f(kit.table, EW - 0.2, 11.2, 270.0, w=0.9, d=0.4, h=0.8)
        f(kit.radio, EW - 0.2, 11.2, 270.0, z=0.8)
        f(kit.wall_clock, EW, 8.2, 270.0, zc=2.3, face="clock_2")
        f(kit.picture, EW, 9.4, 270.0, name="painting_sea_calm", w=0.6, zc=1.7)
        f(kit.notice_board, EW, 10.4, 270.0, w=0.7, zc=1.55, seed=31)
        kit.pendant(B, -4.3, 9.4, CEIL0, drop=0.5, light=True, lumens=500, colour="#FFE2B8")
        kit.pendant(B, -4.4, 5.0, CEIL0, drop=0.4, light=True, lumens=450, colour="#FFE2B8")
        B.thing("home_sofa", "seat", sofa["seats"][1][:3], sofa["seats"][1][3], height=round(sofa["seats"][1][2], 2))
        H.seat("home_armchair", arm)
        H.seat("home_dining_chair", c2)
        H.seat("home_dining_chair_head", c5)
        B.thing("home_tv", "screen", tv["pos"][:2] + (1.0,), tv["yaw"], what="tv")
        B.thing("home_kids_shelf", "shelf", sh["pos"][:2] + (1.2,), sh["yaw"], topic="kids")
        B.thing("home_board_kitchen", "board", (EW, 10.4, 1.55), 270.0, topic="club")
        B.thing("home_radio", "machine", (EW - 0.2, 11.2, 0.95), 270.0, what="jukebox")
        B.thing("home_picture_still", "board", (WW, 4.3, 1.75), 90.0, topic="painting")
        B.thing("home_picture_sea", "board", (EW, 8.2, 1.7), 270.0, topic="painting")
        B.thing("home_paper", "take", (-4.25, 5.0, 0.45), 0.0, item="daily_paper")
    run(0, ground_floor)

    # ------------------------------------------------------------------------------------ first floor
    def first_floor(H):
        z = 3.0
        zz = lambda a: a + z
        # parents' room (front): double bed with the head to the east wall, bedside tables, wardrobe, cot, rug
        f(kit.rug, -4.6, 5.3, 90.0, z=z, w=2.2, d=2.8, name="rug_4")
        bed = f(kit.bed, EW - 1.1, 5.3, 270.0, z=z, size="double", blanket="CT_PlasterRed")
        b1 = f(kit.bedside_table, EW - 0.25, 4.15, 270.0, z=z, lamp=True)
        b2 = f(kit.bedside_table, EW - 0.25, 6.45, 270.0, z=z, lamp=True)
        wr = f(kit.wardrobe, WW, 4.5, 90.0, z=z, w=1.1)
        cot = f(hk.cot, -6.5, 3.55, 0.0, z=z)
        f(kit.floor_lamp, -3.3, 3.65, 0.0, z=z)
        # children's room (back east): bunk beds, school desk by the window, toys, shelf, rug
        f(kit.rug, -4.4, 10.1, 0.0, z=z, w=1.6, d=2.0, name="rug_7")
        bk = f(kit.bunk_bed, EW - 0.58, 10.9, 0.0, z=z, blanket="CT_PlasterBlue")
        sd = f(kit.school_desk, -4.9, RW - 0.45, 180.0, z=z)
        ch = f(kit.chair, -5.17, RW - 1.15, 180.0, z=z, kind="school")
        ch2 = f(kit.chair, -4.63, RW - 1.15, 180.0, z=z, kind="school")
        sh = f(kit.bookshelf, EW, 8.5, 270.0, z=z, w=0.9, h=1.6, levels=4, topic="kids", seed=8)
        f(kit.bean_bag, -5.1, 8.5, 0.0, z=z, mat="CT_PlasterGreen")
        f(kit.poster, EW, 9.6, 270.0, z=z, name="kids_0", w=0.55, zc=1.6)
        kit.pendant(B, -4.2, 5.3, CEIL1, drop=0.4, light=True, lumens=500, colour="#FFE2B8")
        kit.pendant(B, -4.3, 10.0, CEIL1, drop=0.4, light=True, lumens=500, colour="#FFE2B8")
        B.thing("home_parents_bed", "bed", bed["pos"][:2] + (z + 0.5,), bed["yaw"], height=0.5, rest="private")
        B.thing("home_bunk_bed", "bed", bk["pos"][:2] + (z + 0.5,), bk["yaw"], height=0.46, rest="private")
        B.thing("home_cot", "bed", cot["pos"][:2] + (z + 0.45,), cot["yaw"], height=0.45, rest="private")
        B.thing("home_school_desk", "craft", sd["pos"][:2] + (z + 0.8,), (sd["yaw"] + 180.0) % 360.0, what="school_desk")
        for j, c_ in enumerate((ch, ch2)):
            B.thing("home_school_chair_%d" % j, "seat", c_["seats"][0][:2] + (z + 0.45,), c_["seats"][0][3], height=0.45)
        B.thing("home_children_shelf", "shelf", sh["pos"][:2] + (z + 1.2,), sh["yaw"], topic="kids")
        B.thing("home_wardrobe_poster", "board", (EW, 9.6, z + 1.6), 270.0, topic="wallpaper")
    run(1, first_floor)

    # ------------------------------------------------------------------------------------ the loft
    def loft(H):
        z = 6.0
        # front under the glazed gable: telescope; father's chess table; mother's sewing table; playroom
        tel = f(kit.telescope, xm, 3.6, 180.0, z=z)
        f(kit.rug, -4.4, 5.6, 0.0, z=z, w=2.4, d=2.4, name="rug_round_1", round=True)
        cht = f(kit.chess_table, -4.4, 5.6, 0.0, z=z)
        c1 = f(kit.chair, -4.4, 6.25, 0.0, z=z, kind="wood", mat="CT_PlasterBlue")
        c2 = f(kit.chair, -4.4, 4.95, 180.0, z=z, kind="wood", mat="CT_PlasterRed")
        sw = f(kit.sewing_table, EW - 0.5, 9.0, 270.0, z=z)
        sh = f(kit.bookshelf, EW, 7.3, 270.0, z=z, w=1.0, h=1.9, levels=5, topic="science", seed=12)
        f(kit.bean_bag, -5.1, 8.3, 0.0, z=z, mat="CT_PlasterRed")
        f(kit.bean_bag, -4.0, 8.9, 0.0, z=z, mat="CT_PlasterBlue")
        mt = f(kit.mats, -4.5, 10.6, 0.0, z=z, w=1.5, d=1.3, n=1, mat="CT_PlasterGreen")
        f(kit.crate, -3.6, 11.5, 15.0, z=z, w=0.55, d=0.38, h=0.3, fill="CT_PlasterYellow", seed=3)
        f(kit.crate, -5.0, 11.7, 80.0, z=z, w=0.5, d=0.35, h=0.3, fill="CT_PlasterRed", seed=4)
        f(kit.wall_map, -4.3, RW, 0.0, z=z, w=1.4, h=1.0, zc=2.2, name="map_world")   # clear of the rear window
        f(kit.poster, EW, 10.4, 270.0, z=z, name="poster_birds_1", w=0.6, zc=1.7)
        f(kit.floor_lamp, -3.2, 4.0, 0.0, z=z, shade="CT_PlasterYellow")
        kit.pendant(B, -4.4, 5.6, roof_z(-4.4) - 0.3, drop=1.6, light=True, lumens=900, colour="#FFE2B8")
        kit.pendant(B, -4.4, 10.0, roof_z(-4.4) - 0.3, drop=1.6, light=True, lumens=700, colour="#FFE2B8")
        H.seat("loft_chess_chair", c1)
        B.thing("loft_chess_table", "play", cht["pos"][:2] + (z + 0.78,), cht["yaw"], what="chess")
        B.thing("loft_telescope", "machine", tel["pos"][:2] + (z + 1.3,), tel["yaw"], what="telescope")
        B.thing("loft_sewing_machine", "craft", sw["pos"][:2] + (z + 0.85,), sw["yaw"], what="sewing_machine")
        B.thing("loft_shelf_science", "shelf", sh["pos"][:2] + (z + 1.3,), sh["yaw"], topic="science")
        B.thing("loft_map", "board", (-4.3, RW, z + 1.8), 0.0, topic="view")
        B.thing("loft_bird_poster", "board", (EW, 10.4, z + 1.7), 270.0, topic="view")
        B.thing("loft_mats", "bed", mt["pos"][:2] + (z + 0.1,), 0.0, height=0.1, rest="nap")
        B.thing("loft_beanbag", "seat", (-5.1, 8.3, z + 0.4), 0.0, height=0.4)
    run(2, loft)
    for w_ in kit.check(B):
        print("KIT origami", w_)


from _wallhang import hung  # noqa: E402  (QA r9: nothing hangs over the glass)
build = hung(build)
