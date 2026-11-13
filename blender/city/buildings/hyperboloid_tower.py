"""Hyperboloid tower: the pier's lighthouse and water tank, a steel lattice tower of three stacked
hyperboloid sections (straight bars, two opposite families, 20 bars each), ~36 m to the top of the rod.

An original design in the spirit of the 1890s-1920s Russian hyperboloid (Shukhov-type) towers.
- Concrete drum base r 5.4 m, 3 m high, hollow: an enterable room (door in the -y face toward the pier
  promenade, floor CT_Floor, walls CT_Interior, ceiling slab with a round stair hole). B.entrance "door".
- Steel spiral stair (treads in B.m, collide; rise 0.17, ~5 turns, with a centre post and handrail in
  detail) climbing inside the lattice from the room to the viewing platform at z = 20 m.
- Platform: round deck with a gallery outside the lattice ring, closed underside, railing. Spots "view" x4.
- Sections: z 3..11.5, 11.5..20, 20..28 (bar section 0.12 m, rings at the joints and mid-sections, all in B.detail).
- A water tank (cone + drum, CT_SteelWhite) at 28..31, a lit lantern room (12-gon, 8 "public" windows, a CT_Neon
  band) and a copper cap with a lightning rod. No flats.
Plot 14 x 14 on the pier head, height 36 m.

Ideas for later: a rotating beam, a red/green signal lamp, an access ladder to the lantern, rust streaks,
mooring bollards at the base, a lit-room window card inside the base.
Material requests: none.
"""
import math
import random

from citykit import prism, band, band_down, ring, DOOR_W, DOOR_H, geom
from geom import Mesh, Facade, wall_with_holes, reveal
import furn_int3 as fu
import interior_kit
from furn_int3 import Fr, lbox, cyl, blob

INFO = {
    "name_ru": "Гиперболоидная башня-маяк", "name_en": "Hyperboloid Lighthouse Tower",
    "style": "constructivism", "use": "public",
    "inspired_by": "Russian hyperboloid lattice towers of the 1890s-1920s (straight steel bars between rings) - original proportions and use",
}

CX, CY = 0.0, 7.0
R_WALL, WALL_T = 5.4, 0.4
BASE_H = 3.0
SECTIONS = [(3.0, 11.5), (11.5, 20.0), (20.0, 28.0)]
RADII = [5.1, 4.2, 3.5, 3.0]          # lattice radius at z = 3, 11.5, 20, 28
TWIST = math.radians(52)
NBARS = 20
BAR = 0.12
PLAT_Z = 20.0
STAIR_R0, STAIR_R1 = 0.3, 1.75


def _prism_pt(m, mat, a, b, w):
    """A square bar of side w between 3D points a and b."""
    d = geom.norm(geom.sub(b, a))
    ref = (0, 0, 1) if abs(d[2]) < 0.9 else (1, 0, 0)
    u = geom.norm(geom.cross(d, ref))
    v = geom.cross(d, u)
    h = w / 2
    def P(p, su, sv):
        return tuple(p[i] + u[i] * su * h + v[i] * sv * h for i in range(3))
    for p0, p1, (s0, s1, s2, s3) in (
            (a, b, ((1, 1), (-1, 1), (-1, -1), (1, -1))),):
        ring_a = [P(a, *s) for s in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        ring_b = [P(b, *s) for s in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    # orientation: (u, v, d) is right-handed? ensure outward by testing the centre
    cen = tuple((a[i] + b[i]) / 2 for i in range(3))
    for k in range(4):
        q = [ring_a[k], ring_a[(k + 1) % 4], ring_b[(k + 1) % 4], ring_b[k]]
        n = geom.cross(geom.sub(q[1], q[0]), geom.sub(q[3], q[0]))
        mid = tuple(sum(p[i] for p in q) / 4 for i in range(3))
        if sum(n[i] * (mid[i] - cen[i]) for i in range(3)) < 0:
            q = q[::-1]
        m.face(mat, q)
    for rr, flip in ((ring_a, True), (ring_b, False)):
        q = rr[::-1] if flip else rr
        n = geom.cross(geom.sub(q[1], q[0]), geom.sub(q[3], q[0]))
        outward = geom.sub(b, a) if not flip else geom.sub(a, b)
        if sum(n[i] * outward[i] for i in range(3)) < 0:
            q = q[::-1]
        m.face(mat, q)


def _lattice_r(z, sec):
    """Radius of the lattice at height z in section index sec."""
    z0, z1 = SECTIONS[sec]
    r0, r1 = RADII[sec], RADII[sec + 1]
    a = (r0, 0.0, z0)
    b = (r1 * math.cos(TWIST), r1 * math.sin(TWIST), z1)
    t = (z - z0) / (z1 - z0)
    x = a[0] + (b[0] - a[0]) * t
    y = a[1] + (b[1] - a[1]) * t
    return math.hypot(x, y)


def ring_beam(m, mat, r, z, w=0.22, h=0.22, segs=24):
    ro, ri = r + w / 2, r - w / 2
    o = ring(CX, CY, ro, segs, closed=False)
    i_ = ring(CX, CY, ri, segs, closed=False)
    band(m, mat, o, i_, z + h / 2)
    band_down(m, mat, o, i_, z - h / 2)
    for pts, up in ((ring(CX, CY, ro, segs, closed=False), False), (ring(CX, CY, ri, segs, closed=False), True)):
        for k in range(segs):
            (ax, ay), (bx, by) = pts[k], pts[k + 1]
            q = [(ax, ay, z - h / 2), (bx, by, z - h / 2), (bx, by, z + h / 2), (ax, ay, z + h / 2)]
            m.face(mat, q[::-1] if up else q)


def disk_poly(r0, r1, a0, a1, segs):
    """Annular sector polygon (CCW), r0 = 0 gives a fan from the centre."""
    outer = [(CX + r1 * math.cos(a0 + (a1 - a0) * k / segs), CY + r1 * math.sin(a0 + (a1 - a0) * k / segs)) for k in range(segs + 1)]
    if r0 <= 1e-6:
        return [(CX, CY)] + outer
    inner = [(CX + r0 * math.cos(a0 + (a1 - a0) * k / segs), CY + r0 * math.sin(a0 + (a1 - a0) * k / segs)) for k in range(segs + 1)]
    return outer + inner[::-1]


def lattice(B):
    d = B.detail
    for si, (z0, z1) in enumerate(SECTIONS):
        r0, r1 = RADII[si], RADII[si + 1]
        for fam in (1, -1):
            for k in range(NBARS):
                ph = 2 * math.pi * k / NBARS
                a = (CX + r0 * math.cos(ph), CY + r0 * math.sin(ph), z0)
                e = ph + fam * TWIST
                b = (CX + r1 * math.cos(e), CY + r1 * math.sin(e), z1)
                _prism_pt(d, "CT_SteelWhite", a, b, BAR)
        # mid-section ring
        zm = (z0 + z1) / 2
        ring_beam(d, "CT_SteelWhite", _lattice_r(zm, si), zm, 0.14, 0.14)
    for z, r in zip([s[0] for s in SECTIONS] + [SECTIONS[-1][1]], RADII):
        ring_beam(d, "CT_SteelWhite", r, z, 0.30, 0.30)


def base(B):
    N = 16
    ang0 = -math.pi / 2 - math.pi / N          # a segment is centred on the -y direction (the door)
    outer = [(CX + R_WALL * math.cos(ang0 + 2 * math.pi * k / N), CY + R_WALL * math.sin(ang0 + 2 * math.pi * k / N)) for k in range(N)]
    for i in range(N):
        p0, p1 = outer[i], outer[(i + 1) % N]
        F = Facade(p0, p1)
        L = F.length
        holes = []
        if i == 0:
            holes = [((L - 1.25) / 2, (L + 1.25) / 2, 0.0, 2.3)]
        # outer face and reveal
        m = Mesh()
        wall_with_holes(m, "CT_Concrete", L, 0.0, BASE_H, holes)
        for (u0, u1, h0, h1) in holes:
            reveal(m, "CT_Concrete", u0, u1, h0, h1, WALL_T)
        # inner face (CT_Interior)
        inner = Mesh()
        wall_with_holes(inner, "CT_Interior", L, 0.0, BASE_H, holes)
        inner.verts = [(u, -WALL_T, z) for (u, _, z) in inner.verts]
        inner.faces = [(mt, list(reversed(ix)), list(reversed(uv))) for mt, ix, uv in inner.faces]
        m.extend(inner)
        B.m.extend(F.apply(m))
        # top of the wall under the deck: covered by the ceiling slab; bottom sits on the floor slab
    mid = ((outer[0][0] + outer[1][0]) / 2, (outer[0][1] + outer[1][1]) / 2)
    B.entrance("door", (mid[0], mid[1], 0.0), 0.0, width=DOOR_W, height=DOOR_H, kind="street", leaves=1)
    # floor slab (inside and a 0.6 m apron out)
    ap = [(CX + (R_WALL + 0.6) * math.cos(ang0 + 2 * math.pi * k / N), CY + (R_WALL + 0.6) * math.sin(ang0 + 2 * math.pi * k / N)) for k in range(N)]
    B.slab(ap, -0.3, 0.0, "CT_ConcreteDark")
    inner_poly = [(CX + (R_WALL - WALL_T) * math.cos(ang0 + 2 * math.pi * k / N), CY + (R_WALL - WALL_T) * math.sin(ang0 + 2 * math.pi * k / N)) for k in range(N)]
    B.slab(inner_poly, 0.0, 0.03, "CT_Floor")
    # ceiling/deck slab with a round hole for the stair: annular sectors
    nseg = 32
    for k in range(nseg):
        a0, a1 = 2 * math.pi * k / nseg, 2 * math.pi * (k + 1) / nseg
        # outer boundary follows the 16-gon approx: use a circle slightly inside the polygon (R_WALL * cos(pi/N))
        rr = R_WALL * math.cos(math.pi / N) - 0.02
        poly = disk_poly(2.4, rr, a0, a1, 1)
        prism(B.m, "CT_Concrete", poly, BASE_H - 0.4, BASE_H, top=True, bottom=True)
    # the top of the 16-gon wall ring outside circle rr: a coping slab
    cop_o = [(CX + (R_WALL + 0.15) * math.cos(ang0 + 2 * math.pi * k / N), CY + (R_WALL + 0.15) * math.sin(ang0 + 2 * math.pi * k / N)) for k in range(N)]
    hole = None
    # deck is the annulus; the corners outside rr are filled by the polygon fan
    for k in range(N):
        a, b = outer[k], outer[(k + 1) % N]
        ca = (CX + (R_WALL * math.cos(math.pi / N) - 0.05) * math.cos(ang0 + math.pi / N + 2 * math.pi * k / N),
              CY + (R_WALL * math.cos(math.pi / N) - 0.05) * math.sin(ang0 + math.pi / N + 2 * math.pi * k / N))
        prism(B.m, "CT_Concrete", [a, b, (CX + (R_WALL - 0.05) * (b[0] - CX) / R_WALL, CY + (R_WALL - 0.05) * (b[1] - CY) / R_WALL),
                                   (CX + (R_WALL - 0.05) * (a[0] - CX) / R_WALL, CY + (R_WALL - 0.05) * (a[1] - CY) / R_WALL)],
              BASE_H - 0.4, BASE_H, top=True, bottom=True)
    keeper_room(B)
    B.spot("door_bench", "bench", (CX + 2.2, CY - R_WALL - 1.8, 0.0), 180)


A_IN = R_WALL * math.cos(math.pi / 16) - WALL_T      # distance of the drum's inner wall faces from the axis


def _yin(deg):
    """Kit yaw of a piece at polar angle deg that faces the axis of the drum (into the room)."""
    return (deg - 90.0) % 360.0


def _yout(deg):
    return (deg + 90.0) % 360.0


def _pol(r, deg):
    a = math.radians(deg)
    return (CX + r * math.cos(a), CY + r * math.sin(a))


def _lifebuoy(B, x, y, yaw, zc, r_out=0.36, r_in=0.19, t=0.09, n=12):
    """A lifebuoy hung on the wall (red and white segments); (x, y) = the wall point, yaw = facing out of the wall."""
    fr = geom.Frame(x, y, math.radians(yaw))
    for k in range(n):
        a0, a1 = 2 * math.pi * k / n, 2 * math.pi * (k + 1) / n
        mat = "CT_PlasterRed" if k % 3 == 0 else "CT_TileWhite"
        P = {}
        for name, a, r in (("o0", a0, r_out), ("o1", a1, r_out), ("i0", a0, r_in), ("i1", a1, r_in)):
            for tag, v in (("f", -t), ("b", -0.01)):
                P[name + tag] = fr((r * math.cos(a), v, zc + r * math.sin(a)))
        cen = tuple(sum(P[key][i] for key in P) / len(P) for i in range(3))
        for quad in (("o0f", "o1f", "i1f", "i0f"), ("o0b", "o1b", "o1f", "o0f"), ("i1b", "i0b", "i0f", "i1f"),
                     ("o0f", "i0f", "i0b", "o0b"), ("i1f", "o1f", "o1b", "i1b")):
            pts = [P[q] for q in quad]
            nrm = geom.cross(geom.sub(pts[1], pts[0]), geom.sub(pts[3], pts[0]))
            mid = tuple(sum(p[i] for p in pts) / 4 for i in range(3))
            if sum(nrm[i] * (mid[i] - cen[i]) for i in range(3)) < 0:
                pts = pts[::-1]
            B.detail.face(mat, pts)


def keeper_room(B):
    """The keeper's room in the drum (floor z = 0.03, ceiling underside 2.6, door in the -y facet, the spiral stair in
    the middle, r < 1.8). Sixteen wall facets (centre angle -90 + 22.5 k, inner face A_IN from the axis): desk with log
    book and lamp facing the wall (k 8), the radio table (7), bookshelf (9), a safe (10), the cot with a bedside table
    (12, 11), a wardrobe (13), lifebuoy and rope (14), tea corner (4), chart table (6), crates (2), boots (3)."""
    z0 = 0.03
    d = B.detail
    kit = interior_kit

    def fac(k):
        return -90.0 + 22.5 * k

    def wall(k, off=0.0):
        """Point on the wall of facet k, `off` metres off the wall into the room."""
        return _pol(A_IN - off, fac(k))

    def put(fn, k, depth, **kw):
        """A free-standing piece of depth `depth` against the wall of facet k, front toward the axis."""
        x, y = _pol(A_IN - depth / 2 - 0.02, fac(k))
        return fn(B, x, y, z0, _yin(fac(k)), **kw)

    # ---- desk against the north wall, the keeper's chair pulled up to it facing the wall
    ph = fac(8)
    dx, dy = _pol(A_IN - 0.37, ph)
    desk = kit.office_desk(B, dx, dy, z0, _yin(ph), w=1.4, d=0.7, typewriter_on=False, papers=False, phone=False)
    ch = _pol(A_IN - 1.25, ph)
    kit.chair(B, ch[0], ch[1], z0, _yout(ph), kind="wood", mat="CT_PlasterRed")
    B.spot("keeper_seat", "seat", (ch[0], ch[1], z0), _yout(ph))
    B.thing("keeper_chair", "seat", (ch[0], ch[1], z0 + 0.45), _yout(ph), height=0.45)
    top = desk["top"]
    kit.newspaper(B, dx + 0.0, dy - 0.12, top, yaw=ph + 10, cellname="paper_typed")
    kit.books_stack(B, dx - 0.35, dy - 0.05, top, yaw=_yin(ph) + 15, n=1, seed=7)
    kit.lamp_green(B, dx + 0.5, dy + 0.12, top, yaw=_yin(ph), light=False)
    kit.cup(B, dx + 0.2, dy - 0.18, top, yaw=30)
    kit.telephone(B, dx - 0.5, dy + 0.1, top, yaw=_yin(ph) - 25)
    B.thing("keeper_log", "board", (dx - 0.1, dy - 0.05, top), _yin(ph), topic="logbook")
    wx, wy = wall(8)
    kit.wall_map(B, wx, wy, z0, _yin(ph), w=1.2, h=0.9, zc=1.85, name="map_coast")
    B.thing("keeper_chart", "board", (wx, wy, z0 + 1.85), _yin(ph), topic="view")
    wx, wy = wall(8)
    wx2, wy2 = wx + 0.85 * math.cos(math.radians(ph + 90)), wy + 0.85 * math.sin(math.radians(ph + 90))
    kit.wall_clock(B, wx2, wy2, z0, _yin(ph), zc=2.05)
    # ---- radio table (facet 7) and a picture of the tower
    pr = fac(7)
    tb = put(kit.table, 7, 0.5, w=0.8, d=0.5, kind="steel")
    rx, ry = _pol(A_IN - 0.27, pr)
    rd = kit.radio(B, rx, ry, tb["top"], _yin(pr))
    B.thing("tower_radio", "machine", (rx, ry, tb["top"] + 0.15), _yin(pr), what="radio")
    kit.cup(B, rx + 0.28 * math.cos(math.radians(pr + 90)), ry + 0.28 * math.sin(math.radians(pr + 90)), tb["top"])
    wx, wy = wall(7)
    kit.picture(B, wx, wy, z0, _yin(pr), name="painting_lighthouse", w=0.55, zc=1.75)
    # ---- bookshelf (facet 9), safe and a clock (facet 10)
    wx, wy = wall(9)
    bs = kit.bookshelf(B, wx, wy, z0, _yin(fac(9)), w=0.9, topic="library", seed=3)
    B.thing("keeper_shelf", "shelf", (wx - 0.15 * math.cos(math.radians(fac(9))), wy - 0.15 * math.sin(math.radians(fac(9))),
                                       z0 + 1.3), _yin(fac(9)), topic="sea")
    wx, wy = wall(10)
    kit.safe(B, wx, wy, z0, _yin(fac(10)))
    kit.poster(B, wx, wy, z0, _yin(fac(10)), name="poster_cons_2", w=0.6, zc=1.7)
    # ---- the cot (head to the wall, facet 12) with a bedside table, a wardrobe (13)
    pb = fac(12)
    bx, by = _pol(A_IN - 2.05 / 2 - 0.02, pb)
    bd = kit.bed(B, bx, by, z0, _yin(pb), size="single", blanket="CT_PlasterGreen")
    B.thing("keeper_bed", "bed", (bx, by, z0 + 0.5), _yin(pb), height=0.5, rest="nap")
    B.spot("keeper_cot", "bench", (bx, by, z0), _yin(pb))
    put(kit.bedside_table, 11, 0.4, lamp=True)
    put(kit.wardrobe, 13, 0.58, w=1.0)
    # ---- lifebuoy and a rope coil (facet 14), a fire extinguisher (15)
    wx, wy = wall(14)
    _lifebuoy(B, wx, wy, _yin(fac(14)), z0 + 1.6)
    rx, ry = _pol(A_IN - 0.55, fac(14))
    cyl(d, "CT_WoodLight", rx, ry, z0, z0 + 0.35, 0.28, 8, top=False)
    cyl(d, "CT_TileWhite", rx, ry, z0 + 0.35, z0 + 0.38, 0.28, 8)
    cyl(d, "CT_ConcreteDark", rx, ry, z0 + 0.38, z0 + 0.4, 0.1, 6)
    wx, wy = wall(15)
    kit.fire_extinguisher(B, wx, wy, z0, _yin(fac(15)))
    # ---- tea corner (facet 4): a round table with a samovar and two stools
    pt = fac(4)
    tx, ty = _pol(A_IN - 0.85, pt)
    tt = kit.table_round(B, tx, ty, z0, r=0.42, h=0.72)
    kit.samovar(B, tx, ty, tt["top"], yaw=_yin(pt))
    kit.cup(B, tx + 0.22 * math.cos(math.radians(pt + 90)), ty + 0.22 * math.sin(math.radians(pt + 90)), tt["top"])
    kit.glass(B, tx - 0.2 * math.cos(math.radians(pt + 90)), ty - 0.2 * math.sin(math.radians(pt + 90)), tt["top"])
    for sg in (-1, 1):
        sx, sy = _pol(A_IN - 0.85, pt + sg * math.degrees(0.72 / (A_IN - 0.85)))
        kit.stool(B, sx, sy, z0, mat="CT_PlasterBlue", h=0.45)
    sx, sy = _pol(A_IN - 0.85, pt - math.degrees(0.72 / (A_IN - 0.85)))
    B.thing("keeper_stool", "seat", (sx, sy, z0 + 0.45), _yin(pt) + 90, height=0.45)
    kit.rug(B, tx, ty, z0, w=1.9, round=True, seed=9)
    # ---- filing cabinet (5), chart table (6), crates (2), boots and a bench (3), a hat stand (1)
    wx, wy = wall(5)
    kit.filing_cabinet(B, wx - 0.31 * math.cos(math.radians(fac(5))) - 0.0, wy - 0.31 * math.sin(math.radians(fac(5))), z0,
                       _yin(fac(5)), mat="CT_PlasterGreen")
    dt = put(kit.drafting_table, 6, 0.9)
    wx, wy = wall(2)
    for k, (dr, ds) in enumerate(((0.3, -0.35), (0.3, 0.3), (0.35, 0.0))):
        cx_, cy_ = _pol(A_IN - dr - 0.05, fac(2) + math.degrees(ds / A_IN))
        if k < 2:
            kit.crate(B, cx_, cy_, z0, _yin(fac(2)), fill="CT_PlasterBlue", seed=k)
        else:
            kit.sack(B, cx_, cy_, z0, _yin(fac(2)))
    kit.crate(B, *_pol(A_IN - 0.3, fac(2) - math.degrees(0.35 / A_IN)), z0 + 0.3, _yin(fac(2)) + 20, seed=5)
    wx, wy = wall(3)
    kit.shoe_rack(B, wx, wy, z0, _yin(fac(3)), w=1.0, seed=2)
    bx_, by_ = _pol(A_IN - 0.75, fac(3))
    bn = kit.bench(B, bx_, by_, z0, _yin(fac(3)), w=1.2, kind="gym")
    B.thing("keeper_bench", "seat", (bx_, by_, z0 + 0.45), _yin(fac(3)), height=0.45)
    kit.hat_stand(B, *_pol(A_IN - 0.4, fac(1)), z0)
    # ---- the stair up: a caption thing at its foot
    ta = -math.pi / 2 + 0.4
    B.thing("tower_stair", "door", (CX + 1.15 * math.cos(ta), CY + 1.15 * math.sin(ta), z0 + 1.0), 0.0, to="roof")
    # ---- lamps: over the desk, over the cot and over the door side (fixtures: the old opal globes)
    for a_deg in (90.0, 200.0, 330.0):
        a = math.radians(a_deg)
        x, y = CX + 3.0 * math.cos(a), CY + 3.0 * math.sin(a)
        B.lamp((x, y, 2.3), lumens=640, colour="#FFD8A8", kind="public", radius=7.0)
        lbox(d, "CT_Steel", Fr(x, y), -0.008, 0.008, -0.008, 0.008, 2.38, 2.6, faces="xXyY")


def stair(B):
    n = int(math.ceil(PLAT_Z / 0.17))
    rise = PLAT_Z / n
    dth = 0.27
    th0 = -math.pi / 2 + 0.4
    for i in range(n):
        a0 = th0 + dth * i
        a1 = a0 + dth * 1.12
        z1 = rise * (i + 1)
        poly = disk_poly(STAIR_R0, STAIR_R1, a0, a1, 2)
        prism(B.m, "CT_Steel", poly, z1 - 0.10, z1, top=True, bottom=True)
        # handrail posts (every 4th) and the rail in detail
    ths = th0 + dth * (n - 1) + dth * 1.12
    d = B.detail
    B.column_round(CX, CY, 0.0, PLAT_Z - 0.1, 0.10, "CT_Steel", segs=8, m=d)
    prev = None
    for i in range(0, n, 2):
        a = th0 + dth * (i + 0.5)
        z = rise * (i + 1)
        p = (CX + (STAIR_R1 - 0.05) * math.cos(a), CY + (STAIR_R1 - 0.05) * math.sin(a))
        if i % 4 == 0:
            B.column_round(p[0], p[1], z, z + 1.0, 0.025, "CT_Steel", segs=4, m=d)
        top = (p[0], p[1], z + 1.0)
        if prev:
            _prism_pt(d, "CT_Steel", prev, top, 0.04)
        prev = top
    return th0 + dth * (n - 1), n


def platform(B, th_end):
    r_ring = RADII[2]
    r_out = 4.35
    notch0 = th_end - 3.4            # angular range under which the top turns pass
    notch1 = th_end + 0.2
    nseg = 48
    for k in range(nseg):
        a0, a1 = 2 * math.pi * k / nseg, 2 * math.pi * (k + 1) / nseg
        am = (a0 + a1) / 2
        # is this sector inside the notch (mod 2pi)?
        rel = (am - notch0) % (2 * math.pi)
        inside = rel < (notch1 - notch0)
        r0 = (STAIR_R1 + 0.15) if inside else 0.0
        poly = disk_poly(r0, r_out, a0, a1, 1)
        prism(B.m, "CT_Concrete", poly, PLAT_Z - 0.22, PLAT_Z, top=True, bottom=True)
    # railing: posts + two rails around r_out - 0.08, leaving a gap at the stair arrival
    pts = []
    for k in range(0, 73):
        a = 2 * math.pi * k / 72
        rel = (a - (th_end - 0.3)) % (2 * math.pi)
        pts.append((a, rel))
    d = B.detail
    prev_top = prev_mid = None
    for a, rel in pts:
        p = (CX + (r_out - 0.08) * math.cos(a), CY + (r_out - 0.08) * math.sin(a))
        gap = rel < 0.0  # no gap in the gallery: the stair arrives inside the notch
        B.column_round(p[0], p[1], PLAT_Z, PLAT_Z + 1.05, 0.025, "CT_Steel", segs=4, m=d)
        t = (p[0], p[1], PLAT_Z + 1.05)
        mm = (p[0], p[1], PLAT_Z + 0.55)
        if prev_top:
            _prism_pt(d, "CT_Steel", prev_top, t, 0.04)
            _prism_pt(d, "CT_Steel", prev_mid, mm, 0.03)
        prev_top, prev_mid = t, mm
    # two lamp posts on the gallery (lit dusk to dawn: the platform is lit for people up there)
    for a in (math.radians(75), math.radians(255)):
        px, py = CX + (r_out - 0.2) * math.cos(a), CY + (r_out - 0.2) * math.sin(a)
        lbox(d, "CT_Steel", Fr(px, py), -0.03, 0.03, -0.03, 0.03, PLAT_Z, PLAT_Z + 2.15, faces="xXyY")
        cyl(d, "CT_Steel", px, py, PLAT_Z, PLAT_Z + 0.12, 0.09, 6)
        B.lamp((px, py, PLAT_Z + 2.3), lumens=700, colour="#FFE2B8", kind="lobby", radius=9.0)
    for j, a in enumerate((math.radians(30), math.radians(120), math.radians(210), math.radians(300))):
        p = (CX + (r_out - 0.9) * math.cos(a), CY + (r_out - 0.9) * math.sin(a), PLAT_Z)
        B.spot("view%d" % (j + 1), "view", p, math.degrees(math.atan2(math.cos(a), -math.sin(a))))
    # lamp gallery furniture: a telescope aimed at the sea (+y) and a bench on the east side facing out
    kit = interior_kit
    ph = 75.0
    tx, ty = _pol(4.0, ph)
    kit.telescope(B, tx, ty, PLAT_Z, _yout(ph))
    B.thing("tower_telescope", "machine", (tx, ty, PLAT_Z + 1.2), _yout(ph), what="telescope")
    for ph_b, tid in ((165.0, "gallery_bench_w"), (255.0, "gallery_bench_s")):
        bx, by = _pol(3.95, ph_b)
        kit.bench(B, bx, by, PLAT_Z, _yout(ph_b), w=1.3, kind="park")
        B.thing(tid, "seat", (bx, by, PLAT_Z + 0.45), _yout(ph_b), height=0.45)
    for j, ph_v in enumerate((30.0, 120.0, 210.0, 300.0)):
        vx, vy = _pol(r_out - 0.45, ph_v)
        B.thing("gallery_view_%d" % (j + 1), "board", (vx, vy, PLAT_Z + 1.3), _yout(ph_v), topic="view")


def top(B):
    z0 = SECTIONS[-1][1]
    # tank: a cone from the lattice ring, a drum, a lid
    def cone(zA, zB, rA, rB, mat, n=20):
        for k in range(n):
            a0, a1 = 2 * math.pi * k / n, 2 * math.pi * (k + 1) / n
            q = [(CX + rA * math.cos(a0), CY + rA * math.sin(a0), zA), (CX + rA * math.cos(a1), CY + rA * math.sin(a1), zA),
                 (CX + rB * math.cos(a1), CY + rB * math.sin(a1), zB), (CX + rB * math.cos(a0), CY + rB * math.sin(a0), zB)]
            B.m.face(mat, q)
    def cap(z, r, mat, up=True, n=20):
        pts = [(CX + r * math.cos(2 * math.pi * k / n), CY + r * math.sin(2 * math.pi * k / n), z) for k in range(n)]
        B.m.face(mat, pts if up else pts[::-1])
    cone(z0, z0 + 0.9, RADII[3] + 0.1, 3.5, "CT_SteelWhite")
    cap(z0, RADII[3] + 0.1, "CT_SteelWhite", up=False)
    cone(z0 + 0.9, z0 + 3.0, 3.5, 3.5, "CT_SteelWhite")
    cone(z0 + 3.0, z0 + 3.35, 3.5, 2.6, "CT_SteelWhite")
    # lantern room: 12-gon with 8 lit windows, base plate, neon band, copper cap
    zl = z0 + 3.35
    N = 12
    R = 2.4
    wp = [(CX + R * math.cos(2 * math.pi * k / N), CY + R * math.sin(2 * math.pi * k / N)) for k in range(N)]
    ZL1 = zl + 2.5
    for k in range(N):
        F = Facade(wp[k], wp[(k + 1) % N])
        L = F.length
        has = k % 3 != 2 or k % 12 in (2, 5, 8, 11) and False
        holes = [(0.28, L - 0.28, zl + 0.55, zl + 2.2)] if k not in (2, 5, 8, 11) else []
        B.facade(wp[k], wp[(k + 1) % N], zl, ZL1, "CT_SteelWhite", holes=holes, depth=0.12, back=True)
        if holes:
            B.facade_window(F, 0.28, L - 0.28, zl + 0.55, zl + 2.2, kind="public", cols=2, rows=1, inset=0.05)
    B.m.face("CT_SteelWhite", [(x, y, zl) for x, y in wp[::-1]])      # floor of the lantern (seen from below)
    B.m.face("CT_Floor", [(x, y, zl + 0.01) for x, y in wp])
    # neon band (a ring just under the cap)
    ring_beam(B.detail, "CT_Neon", R + 0.02, ZL1 - 0.12, 0.10, 0.10, segs=24)
    ring_beam(B.detail, "CT_Neon", R + 0.02, zl + 0.2, 0.10, 0.10, segs=24)
    # copper roof cone
    cone(ZL1, ZL1 + 1.4, R + 0.25, 0.15, "CT_Copper", n=12)
    cap(ZL1, R + 0.25, "CT_Copper", up=False, n=12)
    cone(ZL1 + 1.4, ZL1 + 2.6, 0.04, 0.04, "CT_Steel", n=4)
    return ZL1 + 2.6


def build(B):
    base(B)
    th_end, n = stair(B)
    platform(B, th_end)
    lattice(B)
    h = top(B)
    N = 16
    ang0 = -math.pi / 2 - math.pi / N
    B.footprint = [(CX + R_WALL * math.cos(ang0 + 2 * math.pi * k / N), CY + R_WALL * math.sin(ang0 + 2 * math.pi * k / N)) for k in range(N)]
    B.height = h
    for m_ in interior_kit.check(B):
        print("KIT:", m_)
    B.note("%d stair treads up to %.1f m; lattice %d bars x2 x3 sections" % (n, PLAT_Z, NBARS))
