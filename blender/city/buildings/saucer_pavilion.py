"""Saucer cafe: a round concrete saucer (22 m, lens-shaped edge) lifted 3.3 m on a ring of ten slanted legs; the rim
is a continuous band of glazing, the café hall is inside. An ORIGINAL design in the spirit of 1970s Kyiv's
"flying saucer" pavilion (not a copy): here the saucer is lifted on splayed legs, a glass tunnel + a glazed
spiral-stair core in the middle lead up through the closed underside into the hall.

Plot 26 x 26 m, 10 m high. Saucer centre (0, 13). Z: ground 0, saucer underside 3.3-4.0, hall floor 4.3, glass band
4.3-7.35, domed roof 7.35-9.7, lantern 9.95.
Enterable:
  * street entrance "main" (double door, 1.8 m) into a 3.3 m wide glazed entrance pavilion (ground level), which
    opens into the glass stair core (r 2.3): a spiral stair of 26 steps (rise 0.165, going 0.3 at mid radius)
    climbing 4.3 m round a concrete column and exiting onto the hall floor;
  * the café hall (r 10.5, floor CT_Floor, walls = the glass band, ceiling CT_Interior): 12 round tables with 2-3
    seats each, a curved bar counter with stools (spots "counter" for the customers' side, 2 "work" baristas behind
    it), a kitchen corner behind a partition (1 "work" cook), pendant lamps, glass railing-free stair well with a
    steel railing.
Spots: 12 tables ("table" + "seat"), bar "counter", 5 stools ("seat"), 3 "work", 2 "view".
Ideas for later: an outdoor terrace ring on the plaza, awnings, a kiosk window, neon sign on the lantern.
Kit wishes: a revolve helper (done here: revolve()), curved glazing cards (a card per facet is used).
"""
import math
import random

from citykit import box, prism, ring, ribbon_wall, Mesh
from _furn1 import rbox, cyl, blob, chair, stool, plant, goods_row

INFO = {
    "name_ru": "Кафе «Летающая тарелка»", "name_en": "Saucer Café",
    "style": "late-Soviet experimental modernism", "use": "public",
    "inspired_by": "1970s Kyiv 'flying saucer' pavilion and Soviet space-age café architecture; "
                   "here a lens-shaped saucer on splayed legs with a spiral glazed core",
}

CX, CY = 0.0, 13.0
R_GLASS = 10.5
Z_FLOOR = 4.3
Z_CEIL = 7.35
N = 48
CORE_R = 2.3


def P(r, a_deg, z=0.0):
    a = math.radians(a_deg)
    return (CX + r * math.cos(a), CY + r * math.sin(a), z)


def yaw_to(a, b):
    """yaw (deg; 0 faces -y, 90 faces +x) of someone at a looking at b."""
    return math.degrees(math.atan2(b[0] - a[0], -(b[1] - a[1])))


def revolve(m, prof, mats, n=N):
    """Revolve the closed profile [(r, z)] (counter-clockwise in the r-z plane, r to the right) about the vertical
    axis through (CX, CY). mats[i] is the material of the edge prof[i] -> prof[i+1]."""
    k = len(prof)
    for i in range(k):
        (r0, z0), (r1, z1) = prof[i], prof[(i + 1) % k]
        if r0 < 1e-6 and r1 < 1e-6:
            continue
        for s in range(n):
            a0, a1 = 360.0 * s / n, 360.0 * (s + 1) / n
            pts = [P(r0, a0, z0), P(r0, a1, z0), P(r1, a1, z1), P(r1, a0, z1)]
            if r0 < 1e-6:
                pts = [pts[0], pts[2], pts[3]]
            elif r1 < 1e-6:
                pts = [pts[0], pts[1], pts[2]]
            m.face(mats[i], pts)


def convex_solid(m, mat, pts8):
    """pts8: 8 points of a (possibly sheared) box: bottom 4 (CCW from above) then top 4 above them. Faces out."""
    b, t = pts8[:4], pts8[4:]
    c = tuple(sum(p[i] for p in pts8) / 8 for i in range(3))

    def out(pts):
        a, bb, cc = pts[0], pts[1], pts[2]
        u = (bb[0] - a[0], bb[1] - a[1], bb[2] - a[2])
        v = (cc[0] - a[0], cc[1] - a[1], cc[2] - a[2])
        n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
        fc = tuple(sum(p[i] for p in pts) / len(pts) for i in range(3))
        if sum(n[i] * (fc[i] - c[i]) for i in range(3)) < 0:
            pts = pts[::-1]
        m.face(mat, pts)
    out(list(b))
    out(list(t))
    for i in range(4):
        out([b[i], b[(i + 1) % 4], t[(i + 1) % 4], t[i]])


def arc_pts(r, a0, a1, n):
    return [P(r, a0 + (a1 - a0) * i / n, 0)[:2] for i in range(n + 1)]


def arc_band(m, mat, r0, r1, a0, a1, z0, z1, n=None, top_mat=None):
    """A curved wall/counter between radii r0<r1 and angles a0<a1 (deg), z0..z1, closed."""
    n = n or max(2, int(abs(a1 - a0) / 6))
    outer = arc_pts(r1, a0, a1, n)
    inner = arc_pts(r0, a1, a0, n)
    poly = outer + inner                       # CCW
    prism(m, top_mat or mat, poly, z0, z1, top=True, bottom=False, side_mat=mat)


def build(B):
    rnd = random.Random(1974)
    m, d = B.m, B.detail

    # -------------------------------------------------------------- the saucer shell
    lower = [(CORE_R, 3.3), (6.0, 3.35), (9.6, 3.6), (11.0, 4.0), (11.2, 4.15), (R_GLASS, Z_FLOOR), (CORE_R, Z_FLOOR)]
    lmat = ["CT_ConcreteDark", "CT_ConcreteDark", "CT_ConcreteDark", "CT_ConcreteDark", "CT_ConcreteWhite",
            "CT_Floor", "CT_Interior"]
    revolve(m, lower, lmat)
    upper = [(0.0, 9.2), (5.0, 8.85), (R_GLASS, Z_CEIL), (11.3, Z_CEIL), (11.3, 7.65), (9.0, 8.5), (5.0, 9.3), (0.0, 9.7)]
    umat = ["CT_Interior", "CT_Interior", "CT_ConcreteDark", "CT_ConcreteWhite", "CT_ConcreteWhite",
            "CT_ConcreteWhite", "CT_ConcreteWhite", "CT_ConcreteWhite"]
    revolve(m, upper, umat)
    # lantern on the crown
    lp = [(0.0, 9.7), (0.9, 9.7), (0.9, 9.95), (0.0, 9.95)]
    revolve(m, [(0.0, 9.7), (0.9, 9.7), (0.9, 9.95), (0.0, 9.95)], ["CT_Steel", "CT_Steel", "CT_Steel", "CT_Steel"], n=12)
    # neon band on the roof fascia
    rp = ring(CX, CY, 11.33, N)
    ribbon_wall(d, "CT_Neon", rp + [rp[0]], 7.40, 7.58)

    # glass band: a card per facet
    for s in range(N):
        a0, a1 = 360.0 * s / N, 360.0 * (s + 1) / N
        p0, p1 = P(R_GLASS, a0), P(R_GLASS, a1)
        B.window([(p0[0], p0[1], Z_FLOOR), (p1[0], p1[1], Z_FLOOR), (p1[0], p1[1], Z_CEIL), (p0[0], p0[1], Z_CEIL)],
                 kind="public", inset=0.06, rows=2, frame=0.05)

    # legs: ten splayed posts
    for i in range(10):
        a = 36.0 * i + 18
        ur = (math.cos(math.radians(a)), math.sin(math.radians(a)))
        ut = (-ur[1], ur[0])
        hw, hr = 0.45, 0.28
        c0 = (CX + 7.0 * ur[0], CY + 7.0 * ur[1])
        c1 = (CX + 9.0 * ur[0], CY + 9.0 * ur[1])
        pts = []
        for (cc, z, sc) in ((c0, 0.0, 1.0), (c1, 3.75, 0.8)):
            for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                pts.append((cc[0] + ut[0] * hw * sx * sc + ur[0] * hr * sy * sc,
                            cc[1] + ut[1] * hw * sx * sc + ur[1] * hr * sy * sc, z))
        convex_solid(m, "CT_Concrete", pts)
        box(m, "CT_ConcreteDark", c0[0] - 0.7, c0[0] + 0.7, c0[1] - 0.7, c0[1] + 0.7, 0.0, 0.0) if False else None
        pad = [(c0[0] + ut[0] * 0.8 * sx + ur[0] * 0.55 * sy, c0[1] + ut[1] * 0.8 * sx + ur[1] * 0.55 * sy)
               for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        prism(m, "CT_ConcreteDark", pad, 0.0, 0.25, top=True)

    # plaza disc
    prism(m, "CT_Paving", ring(CX, CY, 12.4, 48), 0.0, 0.06, top=True)

    # -------------------------------------------------------------- the glazed core with the spiral stair
    nf = 24
    for s in range(nf):
        a0, a1 = 360.0 * s / nf, 360.0 * (s + 1) / nf
        if 225 - 1e-6 <= a0 and a1 <= 315 + 1e-6:
            continue                                   # the opening to the entrance tunnel
        p0, p1 = P(CORE_R, a0), P(CORE_R, a1)
        B.window([(p0[0], p0[1], 0.06), (p1[0], p1[1], 0.06), (p1[0], p1[1], 3.3), (p0[0], p0[1], 3.3)],
                 kind="public", inset=0.0, rows=3, frame=0.05)
    prism(m, "CT_Concrete", ring(CX, CY, 0.5, 12), 0.0, Z_FLOOR, top=False, bottom=False)
    nst = 26
    rs = Z_FLOOR / nst
    dang = 360.0 / nst
    a_start = 290.0
    for i in range(nst):
        a0 = a_start + i * dang
        a1 = a0 + dang + 0.9
        poly = [P(0.45, a0)[:2], P(2.27, a0)[:2]] + [P(2.27, a0 + (a1 - a0) * j / 2)[:2] for j in range(1, 3)] + \
               [P(0.45, a1)[:2]]
        zt = (i + 1) * rs
        prism(m, "CT_Concrete", poly, zt - 0.2, zt, top=True, bottom=True)
    # stairwell railing round the hole, open where the stair exits (angles 250..290)
    rp = [P(CORE_R - 0.06, 290 + (320.0) * i / 24)[:2] for i in range(25)]
    B.railing(rp, Z_FLOOR, h=1.0, mat="CT_Steel", post=1.5)
    # core bottom threshold ring
    prism(m, "CT_ConcreteDark", ring(CX, CY, CORE_R + 0.25, 24), 0.0, 0.12, top=True)

    # -------------------------------------------------------------- entrance tunnel
    hx = CORE_R * math.sin(math.radians(45))             # half width where the tunnel meets the core
    yj = CY - CORE_R * math.cos(math.radians(45))
    y0 = 1.2
    xl, xr = -hx, hx
    # side walls (glass in a concrete frame), front wall with the door
    side_len = yj - y0
    for p0, p1, L, holes in (
            ((xl, y0), (xr, y0), 2 * hx, [(0.9, 2 * hx - 0.9, 0.0, 2.35)] if False else
             [((2 * hx - 1.8) / 2, (2 * hx + 1.8) / 2, 0.0, 2.3), (0.15, (2 * hx - 1.8) / 2 - 0.12, 0.3, 2.6),
              ((2 * hx + 1.8) / 2 + 0.12, 2 * hx - 0.15, 0.3, 2.6)]),
            ((xr, y0), (xr, yj), side_len, [(0.5, 3.1, 0.3, 2.65), (3.5, 6.1, 0.3, 2.65), (6.5, side_len - 0.4, 0.3, 2.65)]),
            ((xl, yj), (xl, y0), side_len, [(0.4, side_len - 6.5, 0.3, 2.65), (side_len - 6.1, side_len - 3.5, 0.3, 2.65),
                                            (side_len - 3.1, side_len - 0.5, 0.3, 2.65)])):
        F = B.facade(p0, p1, 0.0, 2.9, "CT_ConcreteWhite", holes=holes, depth=0.22, back=True)
        for h in holes:
            if h[2] > 0.1:
                B.facade_window(F, h[0], h[1], h[2], h[3], kind="public", cols=max(1, int((h[1] - h[0]) / 1.3)), rows=1,
                                inset=0.10)
    # transom over the door
    prism(m, "CT_Concrete", [(xl - 0.3, y0 - 0.5), (xr + 0.3, y0 - 0.5), (xr + 0.3, yj), (xl - 0.3, yj)], 2.9, 3.15,
          top=True, bottom=True)
    m.face("CT_Floor", [(xl, y0, 0.03), (xr, y0, 0.03), (xr, yj, 0.03), (xl, yj, 0.03)])
    B.entrance("main", (0.0, y0, 0.0), 0.0, width=1.8, height=2.3, kind="street", leaves=2)

    # -------------------------------------------------------------- the hall: bar, kitchen, tables
    zf = Z_FLOOR
    # bar counter (curved), with a wooden top
    arc_band(m, "CT_Concrete", 5.8, 6.6, 50, 130, zf, zf + 1.05, top_mat="CT_WoodLight")
    # back bar / shelf units along the kitchen partition
    arc_band(m, "CT_Interior", 7.9, 8.2, 52, 86, zf, zf + 2.9)
    arc_band(m, "CT_Interior", 7.9, 8.2, 94, 128, zf, zf + 2.9)
    # kitchen side walls (radial)
    for a in (52, 128):
        ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
        n_ = (-sa, ca)
        pa = (CX + 8.2 * ca, CY + 8.2 * sa)
        pb = (CX + (R_GLASS - 0.05) * ca, CY + (R_GLASS - 0.05) * sa)
        t = 0.08
        poly = [(pa[0] - n_[0] * t, pa[1] - n_[1] * t), (pb[0] - n_[0] * t, pb[1] - n_[1] * t),
                (pb[0] + n_[0] * t, pb[1] + n_[1] * t), (pa[0] + n_[0] * t, pa[1] + n_[1] * t)]
        area = sum(poly[i][0] * poly[(i + 1) % 4][1] - poly[(i + 1) % 4][0] * poly[i][1] for i in range(4))
        if area < 0:
            poly = poly[::-1]
        prism(m, "CT_Interior", poly, zf, zf + 2.9, top=True)
    # kitchen kit: stove block + prep table, espresso machine on the bar
    k_c = P(9.4, 90, zf)
    box(d, "CT_Steel", k_c[0] - 1.6, k_c[0] - 0.4, k_c[1] - 0.4, k_c[1] + 0.4, zf, zf + 0.9)
    box(d, "CT_Aluminium", k_c[0] + 0.5, k_c[0] + 1.7, k_c[1] - 0.35, k_c[1] + 0.35, zf, zf + 0.9)

    # tables: round tops on a pedestal (solid, in the shell), real chairs with backs at each seat spot
    layout = [(8.4, a) for a in (-32, 4, 38, 142, 178, 214, 250, 322)] + [(4.8, a) for a in (150, 195, 345, 20)]
    cols = ["CT_SteelRed", "CT_Steel", "CT_PlasterBlue", "CT_PlasterYellow"]
    for ti, (r_, a_) in enumerate(layout):
        c = P(r_, a_, zf)
        cyl(m, "CT_WoodLight", c[0], c[1], zf + 0.72, zf + 0.77, 0.5, 10, bottom=True)
        cyl(m, "CT_Steel", c[0], c[1], zf, zf + 0.72, 0.05, 6, top=False)
        cyl(m, "CT_Steel", c[0], c[1], zf, zf + 0.03, 0.22, 8)
        cyl(d, "CT_TileWhite", c[0] + 0.12, c[1] - 0.08, zf + 0.77, zf + 0.82, 0.07, 6)        # a saucer
        cyl(d, "CT_PlasterRed" if ti % 2 else "CT_PlasterBlue", c[0] - 0.15, c[1] + 0.1, zf + 0.77, zf + 0.84, 0.045, 6)
        tid = "table_%d" % (ti + 1)
        B.spot(tid, "table", (c[0], c[1], zf), 0.0)
        ns = 3 if ti % 3 else 2
        a_chair = rnd.uniform(0, 360)
        for si in range(ns):
            ac = a_chair + 360.0 * si / ns
            sp = (c[0] + 0.92 * math.cos(math.radians(ac)), c[1] + 0.92 * math.sin(math.radians(ac)))
            chair(d, sp[0], sp[1], zf, c[0] - sp[0], c[1] - sp[1], seat=cols[(ti + si) % 4])
            B.spot("seat_%d_%d" % (ti + 1, si + 1), "seat", (sp[0], sp[1], zf), yaw_to(sp, c))
    # bar stools + customers' side of the counter
    for si, a_ in enumerate((62, 76, 90, 104, 118)):
        sp = P(5.25, a_, zf)
        stool(d, sp[0], sp[1], zf)
        B.spot("stool_%d" % (si + 1), "seat", (sp[0], sp[1], zf), yaw_to(sp, (CX, CY + 6.2)))
    cu = P(5.15, 90, zf)
    B.spot("bar", "counter", (cu[0], cu[1], zf), yaw_to(cu, (CX, CY + 6.2)))
    for nm, a_ in (("barista_1", 72), ("barista_2", 108)):
        wp = P(7.25, a_, zf)
        B.spot(nm, "work", (wp[0], wp[1], zf), yaw_to(wp, (CX, CY)))
    kp = P(9.0, 90, zf)
    B.spot("cook", "work", (kp[0], kp[1], zf), 0.0)
    for a_ in (205, 335):
        vp = P(9.7, a_, zf)
        B.spot("view_%d" % a_, "view", (vp[0], vp[1], zf), yaw_to(vp, (CX, CY)) + 180)
    _bar_and_kitchen(B, rnd, zf)
    # plants between the tables, by the glass
    for a_, h in ((-14, 1.5), (21, 1.3), (160, 1.5), (196, 1.2), (232, 1.4), (286, 1.5), (-60 + 0, 1.2)):
        c = P(9.85, a_, zf)
        plant(m, c[0], c[1], zf, h=h, pot=("CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow")[int(a_) % 3])
    for a_ in (60, 120):                                                    # hanging pots on the partition's café side? small
        c = P(7.55, a_, zf + 1.05)
        cyl(d, "CT_PlasterYellow", c[0], c[1], zf + 1.05, zf + 1.2, 0.08, 6)
    # pendant lamps: a real point light in each shade; plus a ring of ceiling lights and the kitchen
    for i in range(12):
        a_ = 30 * i + 15
        if 45 < a_ < 135:
            continue
        lp_ = P(6.8, a_)
        box(d, "CT_Steel", lp_[0] - 0.01, lp_[0] + 0.01, lp_[1] - 0.01, lp_[1] + 0.01, 6.6, 8.45)
        box(d, "CT_Opal", lp_[0] - 0.22, lp_[0] + 0.22, lp_[1] - 0.22, lp_[1] + 0.22, 6.4, 6.6)
        B.lamp((lp_[0], lp_[1], 6.35), lumens=700, colour="#FFD2A0", radius=7.5, fixture=False)
    for a_ in (0, 60, 120, 180, 240, 300):
        lp_ = P(9.4, a_)
        B.lamp((lp_[0], lp_[1], 7.05), lumens=500, colour="#FFD2A0", radius=6.5)
    for a_ in (75, 105):
        lp_ = P(9.2, a_)
        B.lamp((lp_[0], lp_[1], 7.25 - 0.0), lumens=600, colour="#FFF0DC", radius=5.0)
    for yy in (4.0, 8.0):
        B.lamp((0.0, yy, 2.85), lumens=450, colour="#FFD2A0", kind="lobby", radius=5.0)

    B.footprint = ring(CX, CY, 11.3, N)
    B.footprint = B.footprint[:35] + [(-1.8, 1.2), (1.8, 1.2)] + B.footprint[38:]
    B.height = 10.0
    B.note("saucer hall at z %.1f; entrance tunnel -> glazed spiral core; closed underside" % Z_FLOOR)


def _bar_and_kitchen(B, rnd, zf):
    m, d = B.m, B.detail
    top = zf + 1.05
    # back bar: bottle shelves on the café-side face of the partition (r 7.85), a coffee machine, cake vitrine, cups
    for a0, a1 in ((54, 85), (95, 126)):
        for lv, zl in enumerate((1.45, 1.85, 2.25)):
            for k in range(0, 10):
                a_ = a0 + (a1 - a0) * (k + 0.5) / 10
                p = P(7.7, a_, zf)
                rbox(d, "CT_WoodLight", p[0], p[1], zf + zl, zf + zl + 0.03, 0.2, 0.13, math.radians(a_ + 90))
                for j in range(3):
                    off = (j - 1) * 0.12
                    q = P(7.7, a_ + off / 7.7 * 57.3 * 1.0, zf)
                    mat = ("CT_PlasterGreen", "CT_Door", "CT_PlasterBlue", "CT_PlasterYellow", "CT_SteelRed")[(k + j + lv) % 5]
                    cyl(d, mat, q[0], q[1], zf + zl + 0.03, zf + zl + 0.03 + rnd.uniform(0.2, 0.3), 0.032, 5)
    # counter equipment
    em = P(6.2, 90, top)
    rbox(m, "CT_Steel", em[0], em[1], top, top + 0.42, 0.38, 0.22, 0.0)
    rbox(d, "CT_Aluminium", em[0], em[1] - 0.2, top + 0.30, top + 0.4, 0.34, 0.03, 0.0)
    for dx in (-0.2, 0.2):
        cyl(d, "CT_Frame", em[0] + dx, em[1] - 0.2, top, top + 0.02, 0.06, 6)
        cyl(d, "CT_TileWhite", em[0] + dx, em[1] - 0.2, top + 0.02, top + 0.08, 0.04, 6)
    # cake vitrine (white base, shelves with cakes, steel frame; no glass: open display)
    v = P(6.2, 70, top)
    ang = math.radians(70 + 90)
    rbox(m, "CT_TileWhite", v[0], v[1], top, top + 0.22, 0.55, 0.26, ang)
    for k, zz in enumerate((0.22, 0.44)):
        rbox(d, "CT_Steel", v[0], v[1], top + zz, top + zz + 0.02, 0.55, 0.26, ang)
        for j in range(5):
            c = P(6.2, 70, top)
            ox, oy = math.cos(ang) * (-0.42 + 0.21 * j), math.sin(ang) * (-0.42 + 0.21 * j)
            cyl(d, ("CT_PlasterPink", "CT_WoodLight", "CT_Plaster", "CT_PlasterRed", "CT_PlasterYellow")[(j + k) % 5],
                c[0] + ox, c[1] + oy, top + zz + 0.02, top + zz + 0.11, 0.075, 7)
    for sx in (-1, 1):
        for sy in (-1, 1):
            rbox(d, "CT_Steel", v[0] + math.cos(ang) * 0.54 * sx - math.sin(ang) * 0.25 * sy,
                 v[1] + math.sin(ang) * 0.54 * sx + math.cos(ang) * 0.25 * sy, top, top + 0.62, 0.012, 0.012, 0.0)
    rbox(d, "CT_Steel", v[0], v[1], top + 0.62, top + 0.65, 0.57, 0.28, ang)
    # till + cups + a few plates on the counter
    tl = P(6.2, 112, top)
    rbox(d, "CT_Steel", tl[0], tl[1], top, top + 0.14, 0.2, 0.17, math.radians(112 + 90))
    rbox(d, "CT_Frame", tl[0], tl[1] + 0.05, top + 0.14, top + 0.34, 0.1, 0.02, math.radians(112 + 90))
    for a_ in (78, 84, 96, 102, 120, 124):
        c = P(6.2, a_, top)
        cyl(d, "CT_TileWhite", c[0], c[1], top, top + 0.07, 0.04, 6)
    # kitchen: fridge, sink with tap, worktops along the kitchen walls, hood over the stove
    zk = zf
    k_c = P(9.4, 90, zk)
    for a_, w, h_, mat in ((64, 0.4, 1.9, "CT_SteelWhite"), (118, 0.45, 0.9, "CT_Aluminium")):
        c = P(9.3, a_, zk)
        rbox(m, mat, c[0], c[1], zk, zk + h_, w, 0.38, math.radians(a_ + 90))
    c = P(9.3, 118, zk + 0.9)
    cyl(d, "CT_Steel", c[0], c[1], zk + 0.9, zk + 1.15, 0.015, 4)
    box(d, "CT_Steel", k_c[0] - 1.6, k_c[0] - 0.4, k_c[1] - 0.4, k_c[1] + 0.4, zk + 0.9, zk + 0.93)
    for bx in (-1.25, -0.75):
        cyl(d, "CT_Frame", k_c[0] + bx, k_c[1] - 0.1, zk + 0.93, zk + 0.95, 0.12, 8)
    box(d, "CT_Steel", k_c[0] - 1.7, k_c[0] - 0.3, k_c[1] + 0.15, k_c[1] + 0.45, zk + 1.7, zk + 2.1)
    box(d, "CT_Steel", k_c[0] - 1.2, k_c[0] - 0.8, k_c[1] + 0.2, k_c[1] + 0.4, zk + 2.1, zk + 2.9)
    cyl(d, "CT_Steel", k_c[0] + 1.1, k_c[1] - 0.05, zk + 0.9, zk + 1.05, 0.12, 8)
    cyl(d, "CT_Aluminium", k_c[0] + 0.8, k_c[1] + 0.05, zk + 0.9, zk + 1.0, 0.09, 8)
