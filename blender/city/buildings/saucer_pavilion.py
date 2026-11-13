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
Spots: 11 tables ("table" + "seat"), bar "counter", 5 stools ("seat"), 3 "work", 2 "view" + ice-cream/lounge/telescope.
Furnished with interior_kit: tables+chairs, lounge, jukebox, ice-cream counter, telescope, 28 things.
Ideas for later: an outdoor terrace ring on the plaza, awnings, a kiosk window, neon sign on the lantern.
Kit wishes: a revolve helper (done here: revolve()), curved glazing cards (a card per facet is used).
"""
import math
import os
import random

from citykit import box, prism, ring, ribbon_wall, Mesh
from _furn1 import rbox, cyl, blob, goods_row
import furn_int3 as fu
import interior_kit as kit

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


def _switch(B, x, y, yaw, z, tid, scope="room"):
    """A wall light switch: a small white plate with a rocker at height z on the wall at (x, y), facing yaw."""
    fr = fu.Fr(x, y, yaw)
    fu.lbox(B.detail, "CT_SteelWhite", fr, -0.04, 0.04, 0.0, 0.012, z - 0.06, z + 0.06)
    fu.lbox(B.detail, "CT_Steel", fr, -0.012, 0.012, 0.012, 0.02, z - 0.025, z + 0.03)
    B.thing(tid, "machine", fr.p(0.0, 0.03, z), yaw, what="switch", switch=scope)


def _checkers(B, x, y, z, yaw=0.0):
    """[prop] A draughts board (0.34 m, green with white squares) with a few men mid-game, lying on a table top at z."""
    fr = fu.Fr(x, y, yaw)
    s = 0.04
    fu.lbox(B.detail, "CT_PlasterGreen", fr, -4 * s - 0.01, 4 * s + 0.01, -4 * s - 0.01, 4 * s + 0.01, z, z + 0.012)
    for i in range(8):
        for j in range(8):
            if (i + j) % 2 == 0:
                fu.lbox(B.detail, "CT_SteelWhite", fr, -4 * s + i * s, -3 * s + i * s, -4 * s + j * s, -3 * s + j * s, z + 0.012, z + 0.014)
    for k, (i, j, red) in enumerate(((1, 1, 1), (3, 1, 1), (2, 2, 1), (5, 3, 1), (4, 4, 0), (6, 6, 0), (1, 5, 0), (3, 5, 0))):
        px, py = fr.p(-4 * s + (i + 0.5) * s, -4 * s + (j + 0.5) * s)[:2]
        fu.cyl(B.detail, "CT_PlasterRed" if red else "CT_Steel", px, py, z + 0.014, z + 0.03, 0.016, segs=8, top=True)


def _dominoes(B, x, y, z, yaw=0.0):
    """[prop] A short snake of dominoes laid out on a table top at z (white tiles with a dark centre line, 0.04 x 0.08)."""
    fr = fu.Fr(x, y, yaw)
    for u, w, rot in ((-0.16, 0.0, 0), (-0.08, 0.0, 0), (0.0, 0.0, 0), (0.06, 0.05, 90), (0.1, 0.12, 0), (0.1, 0.2, 0), (0.05, 0.27, 90)):
        f = fu.Fr(*fr.p(u, w)[:2], yaw + rot)
        fu.lbox(B.detail, "CT_SteelWhite", f, -0.02, 0.02, -0.04, 0.04, z, z + 0.01)
        fu.lbox(B.detail, "CT_Steel", f, -0.019, 0.019, -0.002, 0.002, z + 0.0095, z + 0.0105)


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
    _switch(B, xr - 0.22, y0 + 3.3, 270.0, 1.2, "light_switch_lobby", "room")      # on the pier between the tunnel's windows

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

    _hall(B, rnd, zf)
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
    for yy in (4.0, 10.0):                        # QA: the second lobby lamp moved 2 m on, so that the stair core is lit
        B.lamp((0.0, yy, 2.85), lumens=450, colour="#FFD2A0", kind="lobby", radius=5.0)

    B.footprint = ring(CX, CY, 11.3, N)
    B.footprint = B.footprint[:35] + [(-1.8, 1.2), (1.8, 1.2)] + B.footprint[38:]
    B.height = 10.0
    B.note("saucer hall at z %.1f; entrance tunnel -> glazed spiral core; closed underside" % Z_FLOOR)
    if os.environ.get("KIT_CHECK"):
        print("KITCHECK", kit.check(B))


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
    em = P(6.2, 101, top)                         # beside the serving hatch (r 7.95, angle 90), not in front of it
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
    for a_ in (78, 84, 87, 120, 124):
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


def _wall_pt(a_w, r, side):
    """A point on the café face of a radial kitchen wall (angle a_w, wall half thickness 0.08) and the yaw facing out."""
    ar = math.radians(a_w)
    tx, ty = -math.sin(ar), math.cos(ar)
    sgn = 1.0 if side > 0 else -1.0
    nx, ny = tx * sgn, ty * sgn
    c = P(r, a_w)
    return (c[0] + nx * 0.085, c[1] + ny * 0.085), math.degrees(math.atan2(nx, -ny)) % 360.0


def _seat_thing(B, tid, r, h=None):
    x, y, z, yw = r["seats"][0]
    B.thing(tid, "seat", (x, y, z), yw, height=h or r.get("height", 0.45))


def _hall(B, rnd, zf):
    """Tables with chairs, bar stools, lounge, jukebox, ice-cream counter, telescope, decor, plants."""
    d = B.detail
    # ------------------------------------------------------------------ tables and chairs
    layout = [(8.4, a) for a in (-32, 4, 38, 160, 178, 214, 250)] + [(4.8, a) for a in (150, 195, 345, 20)]
    cols = ["CT_SteelRed", "CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterGreen"]
    for ti, (r_, a_) in enumerate(layout):
        c = P(r_, a_, zf)
        if r_ < 6:
            kit.rug(B, c[0], c[1], zf, 0.0, w=2.5, d=2.5, round=True, seed=ti)
        t = kit.table_round(B, c[0], c[1], zf, 0.0, r=0.5, mat="CT_WoodLight" if ti % 2 else "CT_TileWhite")
        B.spot("table_%d" % (ti + 1), "table", (c[0], c[1], zf), 0.0)
        ns = 3 if ti % 3 else 2
        a_chair = rnd.uniform(0, 360)
        first = None
        for si in range(ns):
            ac = a_chair + 360.0 * si / ns
            sp = (c[0] + 0.85 * math.cos(math.radians(ac)), c[1] + 0.85 * math.sin(math.radians(ac)))
            yw = yaw_to(sp, c)
            ch = kit.chair(B, sp[0], sp[1], zf, yw, kind="tube", mat=cols[(ti + si) % 4])
            first = first or ch
            B.spot("seat_%d_%d" % (ti + 1, si + 1), "seat", (sp[0], sp[1], zf), yw)
            ox, oy = c[0] + 0.28 * math.cos(math.radians(ac)), c[1] + 0.28 * math.sin(math.radians(ac))
            if (ti + si) % 3 == 0:
                kit.glass(B, ox, oy, t["top"])
            else:
                kit.cup(B, ox, oy, t["top"], math.degrees(math.radians(ac)) )
        if ti % 3 == 1:
            kit.vase(B, c[0], c[1], t["top"], seed=ti)
        elif ti % 3 == 2:
            kit.newspaper(B, c[0] + 0.05, c[1] + 0.05, t["top"], rnd.uniform(0, 360), seed=ti)
        _seat_thing(B, "table_seat_%d" % (ti + 1), first)

    # ------------------------------------------------------------------ bar stools
    for si, a_ in enumerate((62, 76, 90, 104, 118)):
        sp = P(5.3, a_, zf)
        yw = yaw_to(sp, (CX, CY + 6.2))
        s_ = kit.bar_stool(B, sp[0], sp[1], zf, yw)
        B.spot("stool_%d" % (si + 1), "seat", (sp[0], sp[1], zf), yw)
        _seat_thing(B, "stool_%d" % (si + 1), s_, 0.75)
    bar_top = zf + 1.05
    bp = P(6.2, 90, bar_top)
    B.thing("bar", "counter", (bp[0], bp[1], bar_top), 0.0, role="barista", service="bar", shop="cafe_tarelka")
    for a_, kind_ in ((66, "g"), (98, "g"), (106, "c"), (124, "c")):
        q = P(6.2, a_, bar_top)
        (kit.glass if kind_ == "g" else kit.cup)(B, q[0], q[1], bar_top, 0.0)
    kit.plates(B, *P(6.2, 80, bar_top)[:2], bar_top, 0.0, n=3)
    kit.bottles(B, *P(6.55, 100, bar_top)[:2], bar_top, 0.0, n=3, seed=2)

    # ------------------------------------------------------------------ ice-cream counter (west of the entrance)
    a_i = 300
    ic = P(8.6, a_i, zf)
    yw_i = yaw_to(ic, (CX, CY))
    cnt = kit.shop_counter(B, ic[0], ic[1], zf, yw_i, w=2.2, d=0.7, mat="CT_TileWhite", top="CT_TileWhite",
                           front="CT_PlasterPink", till=True)
    # chest freezer behind the counter, sloping lids
    ar = math.radians(a_i)
    ux, uy = math.cos(ar), math.sin(ar)             # outward (behind the counter)
    tx, ty = -uy, ux
    fz = P(9.45, a_i, zf)
    box(d, "CT_SteelWhite", fz[0] - 0.9 * abs(tx) - 0.3 * abs(ux), fz[0] + 0.9 * abs(tx) + 0.3 * abs(ux),
        fz[1] - 0.9 * abs(ty) - 0.3 * abs(uy), fz[1] + 0.9 * abs(ty) + 0.3 * abs(uy), zf, zf + 0.9)
    for k in range(6):                                     # ice-cream tubs on the counter, lids off, scoops in
        u = -0.95 + 0.38 * k
        tx_, ty_ = ic[0] + (-math.sin(math.radians(yw_i)) * 0) + math.cos(math.radians(yw_i)) * u, \
            ic[1] + math.sin(math.radians(yw_i)) * u
        ox_, oy_ = -math.sin(math.radians(yw_i)) * -0.12, math.cos(math.radians(yw_i)) * -0.12
        tub = ("CT_PlasterPink", "CT_TileWhite", "CT_PlasterYellow", "CT_WoodLight", "CT_PlasterGreen", "CT_PlasterRed")[k]
        cyl(d, "CT_Steel", tx_ + ox_, ty_ + oy_, zf + 1.05, zf + 1.17, 0.13, 8, bottom=False)
        cyl(d, tub, tx_ + ox_, ty_ + oy_, zf + 1.17, zf + 1.225, 0.115, 8)
    kit.sign_plate(B, ic[0], ic[1], zf, yw_i, name="sign_milk", w=1.1, zc=2.75, hang=True)
    on = P(8.5, a_i + 4, zf + 1.05)
    kit.cup(B, on[0], on[1], zf + 1.05, 0.0)
    ip = P(8.6, a_i, zf + 1.05)
    B.thing("ice_cream", "counter", (ip[0], ip[1], zf + 1.05), yw_i, role="barista", service="bar", shop="cafe_tarelka")
    cu = P(7.7, a_i, zf)
    B.spot("counter_ice", "counter", (cu[0], cu[1], zf), yw_i + 180.0)
    wp = P(9.15, a_i - 6, zf)
    B.spot("work_ice", "work", (wp[0], wp[1], zf), yw_i)

    # ------------------------------------------------------------------ lounge by the stair: sofa, armchairs, low table
    lc = P(5.7, 225, zf)
    kit.rug(B, lc[0], lc[1], zf, yaw_to(lc, (CX, CY)), w=2.4, d=3.0, name="rug_3", seed=5)
    cof = kit.coffee_table(B, lc[0], lc[1], zf, yaw_to(lc, (CX, CY)))
    kit.newspaper(B, lc[0], lc[1], cof["top"], 20, seed=1)
    kit.cup(B, lc[0] + 0.2, lc[1] + 0.1, cof["top"], 0.0)
    dq = fu.Fr(lc[0], lc[1], yaw_to(lc, (CX, CY))).p(0.27, -0.12)
    _dominoes(B, dq[0], dq[1], cof["top"], yaw_to(lc, (CX, CY)))
    B.thing("lounge_dominoes", "play", (dq[0], dq[1], cof["top"] + 0.03), 0.0, what="dominoes")
    sf_c = P(6.9, 225, zf)
    sofa = kit.sofa(B, sf_c[0], sf_c[1], zf, yaw_to(sf_c, (CX, CY)), seats=3, mat="CT_PlasterBlue")
    _seat_thing(B, "lounge_sofa", sofa, 0.45)
    B.spot("lounge_sofa", "seat", (sf_c[0], sf_c[1], zf), yaw_to(sf_c, (CX, CY)))
    for k, (ra, aa) in enumerate(((5.2, 247), (4.3, 225))):
        ap = P(ra, aa, zf)
        yw = yaw_to(ap, lc)
        arm = kit.armchair(B, ap[0], ap[1], zf, yw, mat=("CT_PlasterGreen", "CT_PlasterYellow")[k])
        _seat_thing(B, "lounge_armchair_%d" % (k + 1), arm, 0.42)
    kit.floor_lamp(B, *P(7.7, 245, zf)[:2], zf, 0.0)
    kit.plant(B, *P(8.0, 266, zf)[:2], zf, 0.0, kind="palm", h=1.7)

    # ------------------------------------------------------------------ kitchen walls: jukebox, menu, rack
    pt, yw = _wall_pt(128, 8.95, 1)
    jb = kit.jukebox(B, pt[0] + math.sin(math.radians(yw)) * 0.31, pt[1] - math.cos(math.radians(yw)) * 0.31, zf, yw)
    B.thing("jukebox", "machine", (jb["pos"][0], jb["pos"][1], zf + 1.1), yw, what="jukebox")
    pt2, yw2 = _wall_pt(128, 9.95, 1)
    kit.picture(B, pt2[0], pt2[1], zf, yw2, name="painting_sea_calm", w=0.9, zc=1.65)
    B.thing("sea_painting", "board", (pt2[0], pt2[1], zf + 1.65), yw2, topic="painting")
    ptc, ywc = _wall_pt(128, 8.95, 1)
    kit.wall_clock(B, ptc[0], ptc[1], zf, ywc, zc=2.5, face="clock_1")
    pt3, yw3 = _wall_pt(52, 9.85, -1)
    kit.menu_board(B, pt3[0], pt3[1], zf, yw3, name="menu_0", w=0.8, zc=1.7)
    B.thing("menu_board", "board", (pt3[0], pt3[1], zf + 1.7), yw3, topic="prices")
    pt4, yw4 = _wall_pt(52, 8.85, -1)
    rk = kit.magazine_rack(B, pt4[0], pt4[1], zf, yw4, w=0.9)
    B.thing("magazine_rack", "shelf", (rk["pos"][0], rk["pos"][1], zf + 1.1), yw4, topic="travel")
    pt5, yw5 = _wall_pt(52, 9.4, -1)
    kit.poster(B, pt5[0], pt5[1], zf, yw5, name="poster_sun_1", w=0.55, zc=2.35)

    # ------------------------------------------------------------------ the window: telescope, benches, plants
    tp = P(9.7, 21, zf)
    yw_t = math.degrees(math.atan2(math.cos(math.radians(21)), -math.sin(math.radians(21)))) % 360.0
    ts = kit.telescope(B, tp[0], tp[1], zf, yw_t)
    B.thing("telescope", "machine", (tp[0], tp[1], zf + 1.25), yw_t, what="telescope")
    B.spot("telescope_view", "view", (tp[0], tp[1], zf), yw_t)
    for k, (a_, h_, kind_) in enumerate(((-14, 1.6, "palm"), (142, 1.3, "monstera"), (196, 1.4, "ficus"),
                                         (232, 1.5, "palm"), (286, 1.5, "ficus"), (336, 1.2, "monstera"))):
        c = P(9.95, a_, zf)
        kit.plant(B, c[0], c[1], zf, 0.0, kind=kind_, pot=("CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow")[k % 3], h=h_)
    B.thing("view_west", "board", (P(10.1, 195, zf + 1.6)), 195 + 90, topic="view")
    B.thing("view_east", "board", (P(10.1, 332, zf + 1.6)), (332 + 90 + 180) % 360, topic="view")
    _extras(B, zf)


def _parrot(d, x, y, z, yw):
    """Kesha: a green parrot (about 250 tris) on a perch at height z, facing yaw yw (0 = -y, 90 = +x)."""
    a = math.radians(yw)
    f = (math.sin(a), -math.cos(a))             # forward
    r = (math.cos(a), math.sin(a))              # to the right

    def at(u, v, h):                            # u forward, v right, h up from the perch
        return x + f[0] * u + r[0] * v, y + f[1] * u + r[1] * v, z + h
    ang = a - math.pi / 2 + math.pi / 2         # rbox local x runs along the bird's right
    for sv in (-0.025, 0.025):                  # feet
        q = at(0.0, sv, 0.0)
        rbox(d, "CT_Steel", q[0], q[1], z, z + 0.03, 0.008, 0.012, a)
    q = at(0.0, 0.0, 0.14)
    blob(d, "CT_PlasterGreen", q[0], q[1], q[2], 0.062, segs=7, sq=1.45)           # body
    q = at(0.03, 0.0, 0.10)
    blob(d, "CT_PlasterYellow", q[0], q[1], q[2], 0.04, segs=6, sq=1.0)            # belly
    q = at(0.045, 0.0, 0.255)
    blob(d, "CT_PlasterGreen", q[0], q[1], q[2], 0.04, segs=7)                     # head
    for sv in (-0.028, 0.028):
        q = at(0.065, sv, 0.27)
        blob(d, "CT_Frame", q[0], q[1], q[2], 0.009, segs=4)                       # eyes
    q = at(0.085, 0.0, 0.245)
    blob(d, "CT_SteelRed", q[0], q[1], q[2], 0.02, segs=5, sq=1.2)                 # beak
    for sv in (-0.05, 0.05):                                                     # wings folded on the sides
        q = at(-0.01, sv, 0.145)
        rbox(d, "CT_PlasterBlue", q[0], q[1], z + 0.07, z + 0.22, 0.05, 0.012, a + math.pi / 2)
    q = at(-0.08, 0.0, -0.02)
    rbox(d, "CT_PlasterGreen", q[0], q[1], z - 0.12, z + 0.1, 0.02, 0.01, a + math.pi / 2)   # tail hangs below the perch
    q = at(-0.08, 0.0, -0.12)
    rbox(d, "CT_SteelRed", q[0], q[1], z - 0.2, z - 0.1, 0.02, 0.01, a + math.pi / 2)


def _extras(B, zf):
    """Second pass: the regulars' chess table, the window ledge for the sunset, the guest book, Kesha's perch,
    the record crate by the jukebox, the cake vitrine and espresso machine as things, a free paper in the lounge."""
    d, m = B.detail, B.m
    top = zf + 1.05
    # ------------------------------------------------------------------ the regulars' chess table (between stair and ice cream)
    cc = P(6.6, 316, zf)
    ct = kit.chess_table(B, cc[0], cc[1], zf, 0.0, seed=7)
    cs = []
    for k, (dy, yw) in enumerate(((-0.62, 180.0), (0.62, 0.0))):
        ch = kit.chair(B, cc[0], cc[1] + dy, zf, yw, kind="tube", mat=("CT_PlasterBlue", "CT_SteelRed")[k])
        cs.append((cc[0], cc[1] + dy, yw))
        B.spot("chess_seat_%s" % "ab"[k], "seat", (cc[0], cc[1] + dy, zf), yw)
        B.thing("chess_chair_%s" % "ab"[k], "seat", (cc[0], cc[1] + dy, zf + 0.45), yw, height=0.45)
    B.thing("regulars_chess", "play", (cc[0], cc[1], zf + 0.72), 0.0, what="chess")
    side = kit.coffee_table(B, cc[0] + 0.95, cc[1] + 0.0, zf, 90.0, w=0.4, d=0.4, h=0.5)
    kit.cup(B, cc[0] + 0.95, cc[1] - 0.05, side["top"], 0.0)
    kit.floor_lamp(B, *P(5.6, 331, zf)[:2], zf, 0.0, light=True)

    # ------------------------------------------------------------------ window ledge with two stools (sunset seat)
    a_l, r_l = 272.0, 10.0
    lc_ = P(r_l, a_l, zf)
    ang = math.radians(a_l + 90.0)
    rbox(d, "CT_WoodLight", lc_[0], lc_[1], zf + 0.82, zf + 0.86, 0.95, 0.23, ang)
    rbox(d, "CT_Steel", lc_[0], lc_[1], zf + 0.78, zf + 0.82, 0.93, 0.03, ang)
    for sx in (-0.88, 0.88):
        q = (lc_[0] + math.cos(ang) * sx, lc_[1] + math.sin(ang) * sx)
        rbox(d, "CT_Steel", q[0], q[1], zf, zf + 0.82, 0.025, 0.025, ang)
    for k, da in enumerate((-3.6, 3.6)):
        sp = P(9.35, a_l + da, zf)
        yw = yaw_to(sp, P(11.0, a_l + da))
        s_ = kit.bar_stool(B, sp[0], sp[1], zf, yw, mat=("CT_PlasterYellow", "CT_PlasterBlue")[k], h=0.68)
        B.spot("ledge_%d" % (k + 1), "seat", (sp[0], sp[1], zf), yw)
        B.thing("ledge_stool_%d" % (k + 1), "seat", (sp[0], sp[1], zf + 0.68), yw, height=0.68)
    lt = P(r_l, a_l + 6.5, zf + 0.86)
    kit.table_lamp(B, lt[0], lt[1], zf + 0.86, 0.0, shade="CT_PlasterYellow", light=True)
    for da, kd in ((-3.0, "c"), (2.4, "g"), (4.0, "c")):
        q = P(r_l - 0.05, a_l + da, zf + 0.86)
        (kit.cup if kd == "c" else kit.glass)(B, q[0], q[1], zf + 0.86, 0.0)
    kit.vase(B, *P(r_l + 0.02, a_l - 6.0, zf + 0.86)[:2], zf + 0.86, 0.0, seed=4)
    yw_o = yaw_to(P(9.0, a_l), P(11.0, a_l))
    B.thing("window_ledge", "board", (lc_[0], lc_[1], zf + 1.3), yw_o, topic="view")

    # ------------------------------------------------------------------ guest book on a high table by the stair
    gb = P(3.75, 312, zf)
    ht = kit.high_table(B, gb[0], gb[1], zf, 0.0)
    kit.newspaper(B, gb[0], gb[1], ht["top"], 15, cellname="paper_letter")
    kit.books_stack(B, gb[0] + 0.09, gb[1] - 0.03, ht["top"], 20, n=2, seed=5)
    cyl(d, "CT_Frame", gb[0] - 0.2, gb[1] + 0.12, ht["top"], ht["top"] + 0.1, 0.022, 6)
    B.thing("guest_pen", "board", (gb[0] - 0.2, gb[1] + 0.12, ht["top"] + 0.08), 0.0, topic="guestpen")
    B.spot("guest_book", "stand", (gb[0], gb[1] - 0.6, zf), 0.0)
    B.thing("guest_book", "board", (gb[0] + 0.1, gb[1] - 0.05, ht["top"] + 0.05), 0.0, topic="guestbook")

    # ------------------------------------------------------------------ Kesha's perch at the end of the bar
    kp = P(6.15, 127, top)
    cyl(d, "CT_Steel", kp[0], kp[1], top, top + 0.025, 0.12, 8)
    cyl(d, "CT_Brass", kp[0], kp[1], top + 0.025, top + 0.36, 0.012, 5)
    rbox(d, "CT_WoodLight", kp[0], kp[1], top + 0.35, top + 0.375, 0.17, 0.014, math.radians(127 + 90))
    _parrot(d, kp[0], kp[1], top + 0.375, yaw_to(kp, (CX, CY)))
    sd = P(6.2, 121.0, top)
    cyl(d, "CT_TileWhite", sd[0], sd[1], top, top + 0.025, 0.07, 8)
    cyl(d, "CT_Frame", sd[0], sd[1], top + 0.025, top + 0.04, 0.045, 6)
    B.thing("kesha_dish", "machine", (sd[0], sd[1], top + 0.1), 0.0, what="kesha_feed")
    B.thing("kesha_perch", "board", (kp[0], kp[1], top + 0.5), (127 + 90) % 360, topic="kesha")

    # ------------------------------------------------------------------ the record crate under the sea painting, by the jukebox
    pr, ywr = _wall_pt(128, 9.95, 1)
    rk = kit.magazine_rack(B, pr[0], pr[1], zf, ywr, w=0.9)
    B.thing("record_crate", "shelf", (rk["pos"][0], rk["pos"][1], zf + 0.9), ywr, topic="music")

    # ------------------------------------------------------------------ the bar: cakes and espresso as things; a free paper
    B.thing("cake_vitrine", "board", P(6.2, 70, top + 0.3), 0.0, topic="cakes")
    B.thing("espresso", "machine", P(6.2, 101, top + 0.3), 0.0, what="espresso")
    lc2 = P(5.7, 225, zf)
    B.thing("lounge_paper", "take", (lc2[0] + 0.1, lc2[1] - 0.1, zf + 0.5), 20, item="daily_paper")

    _third_layer(B, zf)


def _third_layer(B, zf):
    """Back bar, table settings, coat stand, cones at the ice-cream counter, the serving hatch to the kitchen."""
    d = B.detail
    top = zf + 1.05
    rnd = random.Random(31)
    # ---- the bar: a grinder, a row of syrup bottles with a tall glass stand
    g = P(6.3, 80.0, top)
    rbox(d, "CT_Steel", g[0], g[1], top, top + 0.05, 0.1, 0.1, 0.0)
    rbox(d, "CT_SteelRed", g[0], g[1], top + 0.05, top + 0.42, 0.075, 0.08, 0.0)
    cyl(d, "CT_Aluminium", g[0], g[1], top + 0.42, top + 0.62, 0.11, 8, r1=0.04)
    cyl(d, "CT_Frame", g[0], g[1] - 0.12, top + 0.15, top + 0.17, 0.02, 5)
    for k in range(5):
        q = P(6.45, 104 + 3.2 * k, top)
        mat = ("CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterGreen", "CT_PlasterBlue", "CT_PlasterPink")[k]
        cyl(d, mat, q[0], q[1], top, top + 0.2, 0.034, 6, r1=0.03)
        cyl(d, "CT_Steel", q[0], q[1], top + 0.2, top + 0.24, 0.012, 5)
    for k in range(4):
        q = P(6.0, 62 + 2.2 * k, top)
        cyl(d, "CT_TileWhite", q[0], q[1], top, top + 0.1, 0.035, 6, r1=0.04, top=False)
    # ---- table settings that differ
    layout = [(8.4, a) for a in (-32, 4, 38, 160, 178, 214, 250)] + [(4.8, a) for a in (150, 195, 345, 20)]
    for ti, (r_, a_) in enumerate(layout):
        c = P(r_, a_, zf)
        tz = zf + 0.75
        ox, oy = c[0] + 0.17, c[1] - 0.14
        k = ti % 5
        if k == 0:        # sugar bowl with a lid and a spoon
            cyl(d, "CT_TileWhite", ox, oy, tz, tz + 0.06, 0.055, 7, r1=0.045)
            cyl(d, "CT_Brass", ox, oy, tz + 0.06, tz + 0.085, 0.02, 5)
        elif k == 1:      # napkin holder
            rbox(d, "CT_Steel", ox, oy, tz, tz + 0.1, 0.05, 0.015, math.radians(rnd.uniform(0, 90)))
            rbox(d, "CT_TileWhite", ox, oy, tz + 0.1, tz + 0.115, 0.045, 0.012, math.radians(rnd.uniform(0, 90)))
        elif k == 2:      # a forgotten book
            kit.books_stack(B, ox, oy, tz, rnd.uniform(0, 90), n=1, seed=ti)
        elif k == 3:      # a forgotten glove
            rbox(d, "CT_Leather", ox, oy, tz, tz + 0.012, 0.045, 0.035, 0.5)
            for j in range(4):
                rbox(d, "CT_Leather", ox + 0.045 + 0.0, oy + (j - 1.5) * 0.017, tz, tz + 0.01, 0.035, 0.007, 0.2 + 0.1 * j)
        else:             # salt and pepper
            cyl(d, "CT_TileWhite", ox, oy, tz, tz + 0.07, 0.02, 6)
            cyl(d, "CT_Frame", ox + 0.05, oy, tz, tz + 0.07, 0.02, 6)
    # ---- coat stand at the stair exit
    hp = P(3.1, 246, zf)
    kit.hat_stand(B, hp[0], hp[1], zf, 0.0)
    # ---- ice-cream counter: cones in stands behind the tubs, a stack of cups
    a_i = 300
    ic = P(8.6, a_i, zf)
    ywi = math.radians(yaw_to(ic, (CX, CY)))
    fx, fy = math.sin(ywi), -math.cos(ywi)
    tx, ty = math.cos(ywi), math.sin(ywi)
    for u in (-0.55, 0.55):
        cx_, cy_ = ic[0] + tx * u - fx * 0.22, ic[1] + ty * u - fy * 0.22
        cyl(d, "CT_Steel", cx_, cy_, top, top + 0.03, 0.1, 8)
        for j in range(5):
            ang_ = 2 * math.pi * j / 5
            qx, qy = cx_ + 0.045 * math.cos(ang_), cy_ + 0.045 * math.sin(ang_)
            cyl(d, "CT_WoodLight", qx, qy, top + 0.03, top + 0.26, 0.012, 5, r1=0.035)
    for j in range(5):
        cyl(d, "CT_TileWhite", ic[0] + tx * 0.0 - fx * 0.2, ic[1] + ty * 0.0 - fy * 0.2, top + 0.02 * j, top + 0.02 * j + 0.04, 0.04, 6, r1=0.05, top=False)
    # ---- the serving hatch to the kitchen (the gap in the back-bar partition): a pass shelf, plates, a bell
    h = P(7.95, 90, zf)
    rbox(d, "CT_Steel", h[0], h[1], zf + 1.02, zf + 1.06, 0.55, 0.2, 0.0)
    for dx in (-0.55, 0.55):
        rbox(d, "CT_Steel", h[0] + dx, h[1], zf, zf + 1.02, 0.02, 0.02, 0.0)
    kit.plates(B, h[0] - 0.25, h[1], zf + 1.06, 0.0, n=3)
    cyl(d, "CT_Brass", h[0] + 0.3, h[1] - 0.05, zf + 1.06, zf + 1.09, 0.045, 7, r1=0.01)
    cyl(d, "CT_Brass", h[0] + 0.3, h[1] - 0.05, zf + 1.09, zf + 1.1, 0.01, 4)
    arc_band(B.m, "CT_Interior", 7.9, 8.2, 86, 94, zf + 2.25, zf + 2.9)       # closed above the hatch
    for a_ in (85.6, 94.4):                                  # a red frame round the gap so that the hatch reads from the hall
        fp_ = P(8.05, a_)
        rbox(d, "CT_PlasterRed", fp_[0], fp_[1], zf, zf + 2.15, 0.035, 0.17, math.radians(a_ + 90))
    rbox(d, "CT_PlasterRed", h[0], h[1] + 0.08, zf + 2.15, zf + 2.25, 0.6, 0.17, 0.0)
    rbox(d, "CT_PlasterRed", h[0], h[1] - 0.1, zf + 0.98, zf + 1.02, 0.6, 0.12, 0.0)
    B.lamp((h[0], h[1] - 0.4, zf + 2.3), lumens=260, colour="#FFE2B0", radius=3.0, fixture=False)
    ang_k = math.radians(83)                                  # the kitchen's light switch, on the kitchen face by the gap
    kp_ = P(8.22, 83, zf)
    _switch(B, kp_[0], kp_[1], math.degrees(math.atan2(math.cos(ang_k), -math.sin(ang_k))), zf + 1.2, "light_switch_kitchen", "room")
    B.thing("kitchen_hatch", "counter", (h[0] + 0.1, h[1], zf + 1.15), 0.0, role="cook", service="talk")
    hs = P(7.3, 90, zf)
    B.spot("hatch", "stand", (hs[0], hs[1], zf), yaw_to(hs, (CX, CY + 9.0)))

