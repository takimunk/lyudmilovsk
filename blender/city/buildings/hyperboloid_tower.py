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
from interior_kit import _ring_rect

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
    keeper_extras(B)
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
    kit.hat_stand(B, *_wallpt(1, -0.5, 0.4), z0)
    # ---- the stair up: a caption thing at its foot
    ta = -math.pi / 2 + 0.4
    B.thing("tower_stair", "door", (CX + 1.15 * math.cos(ta), CY + 1.15 * math.sin(ta), z0 + 1.0), 0.0, to="roof")
    # ---- lamps: over the desk, over the cot and over the door side (fixtures: the old opal globes)
    for a_deg in (90.0, 200.0, 330.0):
        a = math.radians(a_deg)
        x, y = CX + 3.0 * math.cos(a), CY + 3.0 * math.sin(a)
        B.lamp((x, y, 2.3), lumens=640, colour="#FFD8A8", kind="public", radius=7.0)
        lbox(d, "CT_Steel", Fr(x, y), -0.008, 0.008, -0.008, 0.008, 2.38, 2.6, faces="xXyY")


# ------------------------------------------------------------------------------------------------ depth pass 2
# Small pieces of this tower only (the kit has no barometer, flag locker, oilskins or fog bell): drawn with the kit's
# primitives (interior_kit._P) in local metres, front toward -y, a wall piece's back at y = 0.
def _fo(p, mat, pts, centre):
    """A face whose normal points away from `centre` (local coordinates)."""
    n = geom.cross(geom.sub(pts[1], pts[0]), geom.sub(pts[-1], pts[0]))
    mid = tuple(sum(q[i] for q in pts) / len(pts) for i in range(3))
    if sum(n[i] * (mid[i] - centre[i]) for i in range(3)) < 0:
        pts = pts[::-1]
    p.face(mat, pts)


def _tube_y(p, mat, cx, cz, ys, rs, n=10, cap=True):
    """A tube along local y (a bottle, a drum, a lamp barrel): radii rs at ys."""
    rings = [[(cx + r * math.cos(2 * math.pi * k / n), y, cz + r * math.sin(2 * math.pi * k / n)) for k in range(n)]
             for y, r in zip(ys, rs)]
    for a, b, ya, yb in zip(rings, rings[1:], ys, ys[1:]):
        for i in range(n):
            j = (i + 1) % n
            _fo(p, mat, [a[i], a[j], b[j], b[i]], (cx, (ya + yb) / 2, cz))
    if cap:
        _fo(p, mat, rings[0], (cx, ys[0] + 1.0, cz))
        _fo(p, mat, rings[-1], (cx, ys[-1] - 1.0, cz))


def _disc_y(p, mat, cx, cz, r, y, n=14, front=True):
    """A flat disc in the plane y facing -y (front) or +y."""
    pts = [(cx + r * math.cos(2 * math.pi * k / n), y, cz + r * math.sin(2 * math.pi * k / n)) for k in range(n)]
    p.face(mat, pts if front else pts[::-1])


def _wallpt(k, s=0.0, off=0.0):
    """World point on facet k's wall, s metres along the wall (counter-clockwise), off metres out into the room."""
    a = math.radians(-90.0 + 22.5 * k)
    bx, by = CX + (A_IN - off) * math.cos(a), CY + (A_IN - off) * math.sin(a)
    return bx - s * math.sin(a), by + s * math.cos(a)


def _weather_board(B, x, y, z, yaw, zc):
    """Barometer, thermometer and hygrometer on one wooden plaque."""
    p = interior_kit._P(B, x, y, z, yaw, "weather_board")
    p.kind = "wall"
    p.box("CT_Wood", -0.5, 0.5, -0.035, 0.0, zc - 0.3, zc + 0.3, f="xXyZz")
    bx, bz = -0.22, zc + 0.02
    _tube_y(p, "CT_Brass", bx, bz, [-0.075, -0.035], [0.17, 0.17], 14, cap=False)
    _disc_y(p, "CT_Brass", bx, bz, 0.17, -0.075, 14)
    _disc_y(p, "CT_TileWhite", bx, bz, 0.145, -0.078, 14)
    for k in range(9):                      # tick marks
        a = math.radians(-60 + 15 * k)
        p.rod("CT_Steel", (bx + 0.11 * math.sin(a), -0.08, bz + 0.11 * math.cos(a)),
              (bx + 0.135 * math.sin(a), -0.08, bz + 0.135 * math.cos(a)), 0.008, 3)
    p.rod("CT_Steel", (bx, -0.082, bz), (bx + 0.1 * math.sin(math.radians(-14)), -0.082, bz + 0.1 * math.cos(math.radians(-14))), 0.012, 3)
    p.rod("CT_SteelRed", (bx, -0.083, bz), (bx + 0.1 * math.sin(math.radians(-38)), -0.083, bz + 0.1 * math.cos(math.radians(-38))), 0.008, 3)
    p.box("CT_TileWhite", 0.1, 0.17, -0.06, -0.035, zc - 0.25, zc + 0.25, f="xXyZz")
    p.box("CT_SteelRed", 0.125, 0.145, -0.063, -0.06, zc - 0.2, zc + 0.04, f="y")
    _tube_y(p, "CT_Brass", 0.34, zc + 0.1, [-0.06, -0.035], [0.1, 0.1], 12, cap=False)
    _disc_y(p, "CT_Brass", 0.34, zc + 0.1, 0.1, -0.06, 12)
    _disc_y(p, "CT_TileWhite", 0.34, zc + 0.1, 0.082, -0.063, 12)
    p.rod("CT_Steel", (0.34, -0.065, zc + 0.1), (0.38, -0.065, zc + 0.15), 0.008, 3)
    p.box("CT_Brass", 0.27, 0.41, -0.05, -0.035, zc - 0.2, zc - 0.05, f="xXyZz")
    return p.done()


def _flag_locker(B, x, y, z, yaw, zc):
    """Pigeonholes of signal flags: 2 rows x 4, rolled bunting of the code flags."""
    p = interior_kit._P(B, x, y, z, yaw, "flag_locker")
    p.kind = "wall"
    w, h, d = 1.1, 0.56, 0.16
    p.box("CT_Wood", -w / 2, w / 2, -0.012, 0.0, zc - h / 2, zc + h / 2, f="y")
    for z_, hh in ((zc - h / 2, 0.02), (zc + h / 2 - 0.02, 0.02), (zc - 0.01, 0.02)):
        p.box("CT_WoodLight", -w / 2, w / 2, -d, 0.0, z_, z_ + hh, f="xXyYzZ")
    for k in range(5):
        xx = -w / 2 + k * w / 4
        p.box("CT_WoodLight", xx - 0.01, xx + 0.01, -d, 0.0, zc - h / 2, zc + h / 2, f="xXyY")
    cols = ["CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow", "CT_TileWhite", "CT_PlasterGreen", "CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow"]
    for r in range(2):
        for k in range(4):
            xc = -w / 2 + (k + 0.5) * w / 4
            z0 = zc - h / 2 + 0.03 if r == 0 else zc + 0.03
            c0 = cols[r * 4 + k]
            c1 = cols[(r * 4 + k + 3) % 8]
            p.box(c0, xc - 0.1, xc + 0.1, -d + 0.03, -0.03, z0, z0 + 0.1, f="xXyYZ")
            p.box(c1, xc - 0.1, xc + 0.1, -d + 0.03, -0.03, z0 + 0.1, z0 + 0.2, f="xXyYZ")
    return p.done()


def _stove(B, x, y, z, yaw):
    """A small iron stove with a kettle on top and a pipe to the ceiling; a log basket beside it."""
    p = interior_kit._P(B, x, y, z, yaw, "keeper_stove")
    p.kind = "floor"
    for sx in (-0.17, 0.17):
        for sy in (-0.17, 0.17):
            p.box("CT_Steel", sx - 0.02, sx + 0.02, sy - 0.02, sy + 0.02, 0.0, 0.12, f="xXyYZ")
    p.cyl("CT_ConcreteDark", 0, 0, 0.12, 0.74, 0.23, 10, r1=0.23, top=True)
    p.cyl("CT_Steel", 0, 0, 0.74, 0.77, 0.25, 10)
    p.box("CT_Steel", -0.1, 0.1, -0.255, -0.215, 0.3, 0.55, f="xXyZz")
    p.box("CT_Brass", -0.015, 0.015, -0.265, -0.255, 0.4, 0.46, f="xXyZz")
    p.cyl("CT_Steel", 0, 0.12, 0.77, 2.6, 0.045, 6, top=False)
    p.cyl("CT_Steel", 0, 0.12, 1.5, 1.56, 0.06, 6)
    p.lathe("CT_Copper", -0.05, -0.02, [(0.1, 0.77), (0.13, 0.82), (0.12, 0.92), (0.07, 0.97), (0.03, 0.99)], 10)
    p.rod("CT_Copper", (0.08, -0.02, 0.88), (0.19, -0.02, 0.97), 0.025, 4, caps=True)
    p.tube("CT_Steel", [(-0.15, -0.02, 0.9), (-0.05, -0.02, 1.06), (0.05, -0.02, 0.9)], 0.012, 3)
    # log basket
    bx, by = 0.52, -0.05
    p.cyl("CT_WoodLight", bx, by, 0.0, 0.4, 0.2, 8, r1=0.23, top=False)
    for k in range(5):
        p.rod("CT_Wood", (bx - 0.14 + 0.07 * k, by - 0.05, 0.4), (bx - 0.12 + 0.06 * k, by + 0.06, 0.4), 0.075, 5, caps=True)
    p.core(-0.24, 0.24, -0.24, 0.24, 0.2, 0.7)
    return p.done(solid=True, top=0.77, footprint=(-0.28, 0.76, -0.28, 0.28))


def _cat_basket(B, x, y, z, yaw):
    """A wicker basket with a red cushion and a curled-up tabby."""
    p = interior_kit._P(B, x, y, z, yaw, "cat_basket")
    p.kind = "floor"
    p.cyl("CT_WoodLight", 0, 0, 0.0, 0.2, 0.3, 10, r1=0.34, top=False)
    p.cyl("CT_Wood", 0, 0, 0.19, 0.22, 0.34, 10)
    p.cyl("CT_PlasterRed", 0, 0, 0.16, 0.2, 0.28, 10)
    rings = []
    for zz, rx, ry in ((0.19, 0.18, 0.15), (0.26, 0.22, 0.18), (0.33, 0.19, 0.15), (0.38, 0.1, 0.08)):
        rings.append(interior_kit._ring_circle(0.0, 0.0, ry, zz, 8, rx=rx))
    p.loft("CT_Wood", rings, top=True)
    hx = -0.17
    rings = [interior_kit._ring_circle(hx, -0.1, 0.075, zz, 7, rx=rx) for zz, rx in ((0.2, 0.06), (0.26, 0.085), (0.32, 0.07), (0.345, 0.03))]
    p.loft("CT_Wood", rings, top=True)
    for s_ in (-1, 1):
        p.face("CT_PlasterGrey", [(hx - 0.03, -0.1 + s_ * 0.045, 0.32), (hx + 0.03, -0.1 + s_ * 0.045, 0.32), (hx, -0.1 + s_ * 0.06, 0.38)])
        p.face("CT_PlasterGrey", [(hx + 0.03, -0.1 + s_ * 0.045, 0.32), (hx - 0.03, -0.1 + s_ * 0.045, 0.32), (hx, -0.1 + s_ * 0.06, 0.38)])
    p.tube("CT_Wood", [(0.17, 0.1, 0.24), (0.26, 0.04, 0.23), (0.27, -0.1, 0.23), (0.19, -0.2, 0.23)], 0.045, 4)
    p.cyl("CT_TileWhite", 0.2, -0.2, 0.2, 0.26, 0.026, 5)
    return p.done(footprint=(-0.36, 0.36, -0.36, 0.36), top=0.22)


def _oilskins(B, x, y, z, yaw):
    """A plank with hooks, a yellow oilskin, a child's one and a sou'wester."""
    p = interior_kit._P(B, x, y, z, yaw, "oilskins")
    p.kind = "wall"
    zr = 1.78
    p.box("CT_Wood", -0.5, 0.5, -0.045, 0.0, zr - 0.06, zr + 0.06, f="xXyZz")
    for u, hh, ww in ((-0.22, 1.0, 0.42), (0.16, 0.62, 0.3)):
        p.rod("CT_Brass", (u, -0.045, zr), (u, -0.08, zr + 0.04), 0.02, 3, caps=True)
        top = zr - 0.04
        rings = [_ring_rect(u - ww / 2 - 0.04, u + ww / 2 + 0.04, -0.2, -0.05, top - hh, 0.03),
                 _ring_rect(u - ww / 2 - 0.02, u + ww / 2 + 0.02, -0.17, -0.06, top - hh * 0.6, 0.04),
                 _ring_rect(u - ww / 2 + 0.02, u + ww / 2 - 0.02, -0.14, -0.06, top - 0.08, 0.04)]
        p.loft("CT_PlasterYellow", rings, top=True)
    p.rod("CT_Brass", (0.4, -0.045, zr), (0.4, -0.08, zr + 0.04), 0.02, 3, caps=True)
    p.lathe("CT_PlasterYellow", 0.4, -0.12, [(0.15, zr - 0.5), (0.1, zr - 0.44), (0.05, zr - 0.33), (0.0, zr - 0.3)], 8)
    return p.done()


def _ship_bottle(B, x, y, z, yaw):
    """A model brigantine on a wooden stand and, beside it, a green bottle with a cork and a rolled note."""
    p = interior_kit._P(B, x, y, z, yaw, "ship_bottle")
    p.kind = "prop"
    for xx in (-0.08, 0.08):
        p.box("CT_Wood", xx - 0.012, xx + 0.012, -0.01, 0.01, 0.0, 0.05, f="xXyYZ") if False else None
    for yy in (-0.07, 0.07):
        p.box("CT_Wood", -0.03, 0.03, yy - 0.012, yy + 0.012, 0.0, 0.05, f="xXyYZ")
    p.box("CT_PlasterRed", -0.04, 0.04, -0.14, 0.13, 0.05, 0.085, f="xXyYZ")
    p.box("CT_TileWhite", -0.042, 0.042, -0.14, 0.13, 0.085, 0.1, f="xXyYZ")
    p.face("CT_PlasterRed", [(-0.04, -0.14, 0.05), (0.04, -0.14, 0.05), (0.04, -0.14, 0.1), (-0.04, -0.14, 0.1)][::-1])
    p.face("CT_PlasterRed", [(-0.04, -0.14, 0.1), (0.04, -0.14, 0.1), (0.0, -0.2, 0.1)])
    p.face("CT_PlasterRed", [(0.04, -0.14, 0.1), (-0.04, -0.14, 0.1), (0.0, -0.2, 0.1)][::-1])
    for yy, hh in ((-0.06, 0.2), (0.04, 0.24)):
        p.rod("CT_Wood", (0.0, yy, 0.1), (0.0, yy, 0.1 + hh), 0.008, 3)
        for k in range(2):
            z1, z2 = 0.12 + k * 0.1 * hh / 0.2, 0.12 + (k + 1) * 0.1 * hh / 0.2 - 0.01
            p.face("CT_Paper", [(-0.001, yy - 0.04, z1), (-0.001, yy + 0.04, z1), (-0.001, yy + 0.035, z2), (-0.001, yy - 0.035, z2)])
            p.face("CT_Paper", [(0.001, yy + 0.04, z1), (0.001, yy - 0.04, z1), (0.001, yy - 0.035, z2), (0.001, yy + 0.035, z2)])
    p.rod("CT_Wood", (0.0, -0.2, 0.1), (0.0, -0.3, 0.17), 0.008, 3)
    p.lathe("CT_PlasterGreen", 0.2, 0.0, [(0.04, 0.0), (0.05, 0.03), (0.05, 0.16), (0.025, 0.22), (0.017, 0.27), (0.02, 0.29)], 8)
    p.cyl("CT_Wood", 0.2, 0.0, 0.285, 0.31, 0.017, 6)
    return p.done(top=0.3)


def _clockwork(B, x, y, z, yaw):
    """The lantern's clockwork drive: an iron cabinet with a dial, a gear window, a crank and a hanging log board."""
    p = interior_kit._P(B, x, y, z, yaw, "lamp_clockwork")
    p.kind = "floor"
    w, d, h = 0.5, 0.42, 1.2
    p.box("CT_Steel", -w / 2, w / 2, -d / 2, d / 2, 0.0, h, f="xXyYZz")
    p.box("CT_ConcreteDark", -w / 2 - 0.03, w / 2 + 0.03, -d / 2 - 0.03, d / 2 + 0.03, 0.0, 0.1, f="xXyYZ")
    p.box("CT_Steel", -w / 2 - 0.02, w / 2 + 0.02, -d / 2 - 0.02, d / 2 + 0.02, h, h + 0.04, f="xXyYZz")
    fy = -d / 2
    p.box("CT_Brass", -0.17, 0.17, fy - 0.02, fy, 0.64, 1.1, f="xXyZz")
    p.card_front("clock_2", -0.14, 0.14, 0.82, 1.08, fy - 0.021)
    p.box("CT_Glass", -0.14, 0.14, fy - 0.025, fy - 0.022, 0.67, 0.8, f="y")
    for gx, gz, gr in ((-0.06, 0.735, 0.05), (0.05, 0.725, 0.035)):
        _tube_y(p, "CT_Brass", gx, gz, [fy - 0.021, fy - 0.014], [gr, gr], 10, cap=False)
        _disc_y(p, "CT_Brass", gx, gz, gr, fy - 0.021, 10)
    p.rod("CT_Brass", (0.12, fy, 0.45), (0.12, fy - 0.12, 0.45), 0.025, 4, caps=True)
    p.rod("CT_Brass", (0.12, fy - 0.12, 0.45), (0.12, fy - 0.15, 0.3), 0.02, 3, caps=True)
    p.rod("CT_Brass", (-0.12, fy, 0.3), (-0.12, fy - 0.03, 0.3), 0.07, 6, caps=True)
    p.box("CT_PlasterRed", -0.17, -0.07, fy - 0.012, fy, 0.18, 0.24, f="xXyZz")
    # the log board on a nail at the right side
    sx = w / 2
    p.box("CT_Wood", sx, sx + 0.015, -0.14, 0.14, 0.55, 1.0, f="XyYZz")
    p.card_side("paper_typed", -0.12, 0.12, 0.6, 0.95, sx + 0.016, facing=1)
    return p.done(solid=True, top=h, footprint=(-0.3, 0.3, -0.25, 0.25))


def _fog_bell(B, x, y, z, yaw):
    """A bracket post on the gallery with a brass bell and a rope; local front = the rope's side (toward the hero)."""
    p = interior_kit._P(B, x, y, z, yaw, "fog_bell")
    p.kind = "floor"
    p.cyl("CT_Steel", 0, 0.5, 0.0, 2.25, 0.04, 6, top=True)
    p.rod("CT_Steel", (0, 0.5, 2.2), (0, -0.2, 2.2), 0.05, 4, caps=True)
    p.rod("CT_Steel", (0, 0.5, 1.7), (0, -0.05, 2.18), 0.03, 4)
    p.lathe("CT_Brass", 0, -0.2, [(0.2, 1.62), (0.19, 1.68), (0.15, 1.82), (0.1, 1.95), (0.05, 2.04), (0.02, 2.08)], 10)
    p.rod("CT_Steel", (0, -0.2, 2.08), (0, -0.2, 2.2), 0.02, 3)
    p.tube("CT_Paper", [(0, -0.2, 1.66), (0.01, -0.22, 1.3), (0.0, -0.22, 0.95)], 0.02, 3)
    p.cyl("CT_Wood", 0, -0.22, 0.9, 1.0, 0.03, 6)
    return p.done(footprint=(-0.1, 0.1, 0.4, 0.6), top=None)


def _signal_lamp(B, x, y, z, yaw):
    """A hand signalling lamp (Aldis) on a tripod: barrel, lens, trigger grip, folded shutter vanes."""
    p = interior_kit._P(B, x, y, z, yaw, "signal_lamp")
    p.kind = "floor"
    for k in range(3):
        a = 2 * math.pi * k / 3 + math.pi / 2
        p.rod("CT_Wood", (0.32 * math.cos(a), 0.32 * math.sin(a), 0.0), (0.0, 0.0, 1.05), 0.03, 3)
    p.cyl("CT_Steel", 0, 0, 1.03, 1.1, 0.05, 6)
    _tube_y(p, "CT_SteelWhite", 0.0, 1.22, [-0.28, -0.2, 0.18, 0.24], [0.1, 0.11, 0.11, 0.08], 10)
    _disc_y(p, "CT_Neon", 0.0, 1.22, 0.095, -0.285, 10)
    p.rod("CT_Steel", (0.0, 0.12, 1.12), (0.0, 0.14, 1.0), 0.03, 3, caps=True)
    p.rod("CT_Steel", (0.0, 0.0, 1.1), (0.0, 0.0, 1.13), 0.06, 4)
    p.box("CT_Steel", -0.1, 0.1, -0.31, -0.29, 1.12, 1.32, f="xXzZ")
    return p.done(top=1.3, footprint=(-0.3, 0.3, -0.3, 0.3))


def _anemometer(B, x, y, z, yaw):
    """A railing-post anemometer (three cups) with a vane."""
    p = interior_kit._P(B, x, y, z, yaw, "anemometer")
    p.kind = "prop"
    p.cyl("CT_Steel", 0, 0, 0.0, 2.3, 0.03, 6)
    p.cyl("CT_Steel", 0, 0, 0.0, 0.15, 0.06, 6)
    zc = 2.2
    for k in range(3):
        a = 2 * math.pi * k / 3
        ex, ey = 0.22 * math.cos(a), 0.22 * math.sin(a)
        p.rod("CT_Steel", (0, 0, zc), (ex, ey, zc), 0.015, 3)
        p.lathe("CT_Aluminium", ex, ey, [(0.05, zc - 0.05), (0.05, zc), (0.0, zc)], 6)
    p.rod("CT_Steel", (0, 0.0, 2.3), (0, 0.0, 2.55), 0.02, 3)
    p.rod("CT_Steel", (-0.25, 0.0, 2.55), (0.25, 0.0, 2.55), 0.02, 3)
    p.face("CT_SteelRed", [(0.25, 0.0, 2.5), (0.25, 0.0, 2.6), (0.4, 0.0, 2.55)])
    p.face("CT_SteelRed", [(0.25, 0.0, 2.6), (0.25, 0.0, 2.5), (0.4, 0.0, 2.55)])
    p.face("CT_SteelRed", [(-0.25, -0.0, 2.48), (-0.25, 0.0, 2.62), (-0.42, 0.0, 2.62), (-0.42, 0.0, 2.48)])
    p.face("CT_SteelRed", [(-0.25, 0.0, 2.62), (-0.25, 0.0, 2.48), (-0.42, 0.0, 2.48), (-0.42, 0.0, 2.62)])
    return p.done(footprint=(-0.1, 0.1, -0.1, 0.1))


def _stevenson(B, x, y, z, yaw):
    """A louvred instrument screen on four legs."""
    p = interior_kit._P(B, x, y, z, yaw, "stevenson")
    p.kind = "floor"
    for sx in (-0.2, 0.2):
        for sy in (-0.17, 0.17):
            p.box("CT_Wood", sx - 0.02, sx + 0.02, sy - 0.02, sy + 0.02, 0.0, 0.85, f="xXyYZ")
    p.box("CT_TileWhite", -0.27, 0.27, -0.24, 0.24, 0.85, 0.89, f="xXyYZz")
    p.box("CT_TileWhite", -0.3, 0.3, -0.27, 0.27, 1.3, 1.34, f="xXyYZz")
    for k in range(6):
        zz = 0.9 + k * 0.07
        p.box("CT_TileWhite", -0.25, 0.25, -0.25, -0.2, zz, zz + 0.035, f="xXyYZz")
    p.box("CT_TileWhite", -0.25, 0.25, 0.2, 0.25, 0.9, 1.3, f="xXyYZz")
    p.box("CT_TileWhite", -0.25, -0.2, -0.2, 0.2, 0.9, 1.3, f="xYZz")
    p.box("CT_TileWhite", 0.2, 0.25, -0.2, 0.2, 0.9, 1.3, f="XYZz")
    return p.done(footprint=(-0.3, 0.3, -0.27, 0.27), top=1.34)


def _lights_lectern(B, x, y, z, yaw):
    """A slanted table of ships' lights on a steel post at the gallery rail."""
    p = interior_kit._P(B, x, y, z, yaw, "lights_lectern")
    p.kind = "prop"
    p.cyl("CT_Steel", 0, 0, 0.0, 1.0, 0.035, 6)
    p.cyl("CT_Steel", 0, 0, 0.0, 0.05, 0.12, 6)
    p.box("CT_Wood", -0.32, 0.32, -0.22, 0.2, 0.98, 1.05, f="xXyYz")
    p.card_tilt("poster_safety", -0.28, 0.28, -0.2, 1.065, 0.18, 1.22)
    return p.done(footprint=(-0.35, 0.35, -0.25, 0.25), top=1.2)


def keeper_extras(B):
    """More life in the keeper's room: stove and kettle, cat, oilskins, barometer, tide board, flag locker, ship in a
    bottle, the lantern's clockwork, a storm picture; their things (acts in tools/city/content/x_p_pier.json)."""
    kit = interior_kit
    z0 = 0.03
    # stove (facet 15 toward 14) with a stool, and the cat's basket the other side of the extinguisher
    sx, sy = _wallpt(15, -0.62, 0.42)
    st = _stove(B, sx, sy, z0, _yin(-90.0 + 22.5 * 15))
    B.thing("keeper_stove", "machine", (sx, sy, z0 + 0.9), _yin(-90.0 + 22.5 * 15), what="stove")
    stx, sty = _wallpt(15, -0.62, 1.15)
    kit.stool(B, stx, sty, z0, mat="CT_PlasterRed", h=0.4)
    B.thing("keeper_stove_stool", "seat", (stx, sty, z0 + 0.4), _yin(-90.0 + 22.5 * 15) + 20, height=0.4)
    B.spot("stove_warm", "stand", (stx + 0.0, sty, z0), _yin(-90.0 + 22.5 * 15))
    cx_, cy_ = _wallpt(15, 0.3, 0.45)
    _cat_basket(B, cx_, cy_, z0, _yin(-90.0 + 22.5 * 15) + 30)
    B.thing("keeper_cat", "board", (cx_, cy_, z0 + 0.3), _yin(-90.0 + 22.5 * 15), topic="cat")
    # oilskins on facet 1 (beside the hat stand)
    ox, oy = _wallpt(1, 0.5)
    _oilskins(B, ox, oy, z0, _yin(-90.0 + 22.5 * 1))
    B.thing("keeper_oilskins", "board", (ox, oy, z0 + 1.2), _yin(-90.0 + 22.5), topic="oilskins")
    # instruments board over the filing cabinet (5), tide board over the drafting table (6)
    wx, wy = _wallpt(5, 0.0)
    _weather_board(B, wx, wy, z0, _yin(-90.0 + 22.5 * 5), 1.95)
    B.thing("keeper_weather", "board", (wx, wy, z0 + 1.95), _yin(-90.0 + 22.5 * 5), topic="weather")
    wx, wy = _wallpt(6, 0.0)
    kit.blackboard(B, wx, wy, z0, _yin(-90.0 + 22.5 * 6), w=1.0, h=0.62, zb=1.45, writing="board_2")
    B.thing("keeper_tides", "board", (wx, wy, z0 + 1.75), _yin(-90.0 + 22.5 * 6), topic="tides")
    # signal flags over the crates (2), ship in a bottle on a shelf over the boots (3)
    wx, wy = _wallpt(2, 0.0)
    _flag_locker(B, wx, wy, z0, _yin(-90.0 + 22.5 * 2), 1.75)
    B.thing("keeper_flags", "board", (wx, wy, z0 + 1.75), _yin(-90.0 + 22.5 * 2), topic="flags")
    wx, wy = _wallpt(3, 0.0)
    sh = kit.wall_shelf(B, wx, wy, z0, _yin(-90.0 + 22.5 * 3), w=0.9, zt=1.6, props=False)
    bx_, by_ = _wallpt(3, -0.12, 0.13)
    _ship_bottle(B, bx_, by_, z0 + 1.6, _yin(-90.0 + 22.5 * 3) + 90)
    cmx, cmy = _wallpt(3, 0.22, 0.13)
    kit.books_stack(B, cmx, cmy, z0 + 1.6, yaw=_yin(-90.0 + 22.5 * 3) + 70, n=3, seed=11)
    B.thing("keeper_bottle", "board", (bx_, by_, z0 + 1.7), _yin(-90.0 + 22.5 * 3), topic="bottle")
    # a storm over the bedside table (11)
    wx, wy = _wallpt(11, 0.0)
    kit.picture(B, wx, wy, z0, _yin(-90.0 + 22.5 * 11), name="painting_sea_storm", w=0.6, zc=1.55)
    B.thing("keeper_storm", "board", (wx, wy, z0 + 1.55), _yin(-90.0 + 22.5 * 11), topic="storm")
    # the lantern's clockwork under the stair, and the cup on the desk that wants tea
    ca = 100.0
    cx2, cy2 = _pol(1.15, ca)
    _clockwork(B, cx2, cy2, z0, 10.0)
    B.thing("keeper_clockwork", "machine", (cx2 - 0.04, cy2 - 0.25, z0 + 0.85), 10.0, what="clockwork")
    c10, s10 = math.cos(math.radians(10.0)), math.sin(math.radians(10.0))
    B.thing("keeper_watch", "board", (cx2 + 0.27 * c10, cy2 + 0.27 * s10, z0 + 0.78), 100.0, topic="watch")
    B.spot("clockwork_watch", "stand", (cx2 - 0.1, cy2 - 0.75, z0), 0.0)
    ph = -90.0 + 22.5 * 8
    dx, dy = _pol(A_IN - 0.37, ph)
    B.thing("keeper_cup", "board", (dx + 0.2, dy - 0.18, z0 + 0.8), _yin(ph), topic="cup")
    # a lamp over the stove corner
    lx, ly = _pol(3.2, 247.5)
    B.lamp((lx, ly, 2.3), lumens=520, colour="#FFC890", kind="public", radius=6.0)
    lbox(B.detail, "CT_Steel", Fr(lx, ly), -0.008, 0.008, -0.008, 0.008, 2.38, 2.6, faces="xXyY")
    keeper_extras2(B)


# ------------------------------------------------------------------------------------------------ depth pass 3
# Visitors' desk, ship's wheel, glass floats, bunting, chess corner (the keeper's room); morse key, sea-level staff,
# bottle post, plaque, flag mast, gulls (the gallery). All drawn here with the kit's primitives, no people.
def _yaw_to(dx, dy):
    """Kit yaw of a piece that faces the direction (dx, dy)."""
    return math.degrees(math.atan2(dx, -dy)) % 360.0


def _guest_lectern(B, x, y, z, yaw):
    """A visitors' lectern: turned post, slanted top with an open guest book, a ribbon and a pencil on a string."""
    p = interior_kit._P(B, x, y, z, yaw, "guest_lectern")
    p.kind = "floor"
    p.cyl("CT_Wood", 0, 0, 0.0, 0.05, 0.25, 10, top=True)
    p.lathe("CT_Wood", 0, 0, [(0.05, 0.05), (0.07, 0.3), (0.05, 0.6), (0.06, 0.9), (0.08, 0.96)], 8, top=True)
    r0 = [(-0.27, -0.2, 0.95), (0.27, -0.2, 0.95), (0.27, 0.2, 1.07), (-0.27, 0.2, 1.07)]
    r1 = [(-0.27, -0.2, 1.0), (0.27, -0.2, 1.0), (0.27, 0.2, 1.12), (-0.27, 0.2, 1.12)]
    p.loft("CT_Wood", [r0, r1], top=True, bottom=True)
    p.box("CT_Wood", -0.27, 0.27, -0.235, -0.2, 0.98, 1.03, f="xXyYZz")
    p.card_tilt("paper_letter", -0.23, -0.005, -0.15, 1.027, 0.16, 1.12, rect=(0, 0, 1, 1))
    p.card_tilt("paper_typed", 0.005, 0.23, -0.15, 1.027, 0.16, 1.12)
    p.rod("CT_PlasterRed", (0.0, -0.15, 1.02), (0.0, -0.3, 0.98), 0.012, 3)
    p.rod("CT_Wood", (0.26, -0.1, 1.04), (0.17, 0.0, 1.075), 0.012, 5, caps=True)
    p.tube("CT_Paper", [(0.265, -0.1, 1.03), (0.3, -0.1, 0.9), (0.28, -0.05, 0.78)], 0.006, 3)
    p.rod("CT_Wood", (0.28, -0.05, 0.78), (0.28, -0.05, 0.7), 0.012, 5, caps=True)
    return p.done(top=1.12, footprint=(-0.28, 0.28, -0.25, 0.25))


def _switch_plate(B, x, y, z, yaw, zc):
    """A wall switch: a brass plate with a lever (wall piece, back at y = 0)."""
    p = interior_kit._P(B, x, y, z, yaw, "switch_plate")
    p.kind = "wall"
    p.box("CT_PlasterGrey", -0.045, 0.045, -0.02, 0.0, zc - 0.07, zc + 0.07, f="xXyZz")
    p.box("CT_Brass", -0.03, 0.03, -0.024, -0.02, zc - 0.045, zc + 0.045, f="xXyZz")
    p.rod("CT_Steel", (0, -0.024, zc - 0.015), (0, -0.065, zc + 0.02), 0.014, 4, caps=True)
    return p.done()


def _ships_wheel(B, x, y, z, yaw, zc, R=0.3):
    """A ship's wheel on a wall plaque: rim in 16 bars, eight spokes with turned handles, brass hub."""
    p = interior_kit._P(B, x, y, z, yaw, "ships_wheel")
    p.kind = "wall"
    yv = -0.07
    n = 16
    p.box("CT_Wood", -R - 0.1, R + 0.1, -0.025, 0.0, zc - R - 0.1, zc + R + 0.1, f="xXyZz")
    for k in range(n):
        a0, a1 = 2 * math.pi * k / n, 2 * math.pi * (k + 1) / n
        p.rod("CT_WoodLight", (R * math.cos(a0), yv, zc + R * math.sin(a0)), (R * math.cos(a1), yv, zc + R * math.sin(a1)), 0.04, 4)
    for k in range(8):
        a = 2 * math.pi * k / 8
        p.rod("CT_Wood", (0.0, yv, zc), ((R + 0.09) * math.cos(a), yv, zc + (R + 0.09) * math.sin(a)), 0.03, 4)
        p.rod("CT_Brass", (R * math.cos(a) * 1.03, yv, zc + R * math.sin(a) * 1.03),
              ((R + 0.1) * math.cos(a), yv, zc + (R + 0.1) * math.sin(a)), 0.035, 5, caps=True)
    p.rod("CT_Steel", (0, -0.025, zc), (0, -0.13, zc), 0.03, 5, caps=True)
    _tube_y(p, "CT_Brass", 0.0, zc, [yv - 0.06, yv - 0.02, yv + 0.02], [0.05, 0.065, 0.05], 10)
    return p.done()


def _float_row(B, x, y, z, yaw):
    """A row of green glass fishing floats in a rope sling lying on a shelf (z = the shelf's top)."""
    p = interior_kit._P(B, x, y, z, yaw, "glass_floats")
    p.kind = "prop"
    for k, (u, r) in enumerate(((-0.3, 0.075), (-0.15, 0.09), (0.0, 0.07), (0.15, 0.1), (0.3, 0.08))):
        p.lathe("CT_PlasterGreen" if k % 2 == 0 else "CT_Glass", u, -0.08, [(0.02, 0.0), (r * 0.8, r * 0.3), (r, r), (r * 0.8, r * 1.7), (0.02, r * 2.0)], 8)
        p.rod("CT_Paper", (u - r * 0.8, -0.08, r * 1.4), (u + r * 0.8, -0.08, r * 1.4), 0.012, 3)
    p.tube("CT_Paper", [(-0.4, -0.04, 0.02), (-0.2, -0.17, 0.03), (0.0, -0.04, 0.03), (0.2, -0.17, 0.03), (0.4, -0.04, 0.02)], 0.012, 3)
    return p.done()


def _bunting(B, a, b, sag=0.25, n=12, mats=None):
    """Signal bunting on a string between world points a and b (z included): small flags hanging from a sagging line."""
    mats = mats or ["CT_PlasterRed", "CT_TileWhite", "CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterGreen"]
    d = B.detail
    prev = None
    for i in range(n + 1):
        t = i / n
        pt = (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t - sag * 4 * t * (1 - t))
        if prev is not None:
            _prism_pt(d, "CT_Paper", prev, pt, 0.012)
            mid = tuple((prev[k] + pt[k]) / 2 for k in range(3))
            ux, uy = pt[0] - prev[0], pt[1] - prev[1]
            ln = math.hypot(ux, uy) or 1.0
            ux, uy = ux / ln * 0.09, uy / ln * 0.09
            q = [(mid[0] - ux, mid[1] - uy, mid[2]), (mid[0] + ux, mid[1] + uy, mid[2]), (mid[0], mid[1], mid[2] - 0.2)]
            mat = mats[(i - 1) % len(mats)]
            nx, ny = -uy / 0.09 * 0.004, ux / 0.09 * 0.004
            d.face(mat, [(v[0] + nx, v[1] + ny, v[2]) for v in q])
            d.face(mat, [(v[0] - nx, v[1] - ny, v[2]) for v in q[::-1]])
        prev = pt


def _morse_post(B, x, y, z, yaw):
    """A signal-practice post: a little round table with a brass Morse key, a paper tape and a pencil."""
    p = interior_kit._P(B, x, y, z, yaw, "morse_post")
    p.kind = "floor"
    p.cyl("CT_Steel", 0, 0, 0.0, 0.05, 0.18, 8)
    p.cyl("CT_Steel", 0, 0, 0.05, 0.95, 0.035, 6, top=False)
    p.cyl("CT_Wood", 0, 0, 0.95, 0.99, 0.24, 10, top=True, bottom=True)
    p.box("CT_Wood", -0.12, 0.12, -0.08, 0.08, 0.99, 1.02, f="xXyYZ")
    p.box("CT_Brass", -0.1, 0.1, -0.05, 0.05, 1.02, 1.035, f="xXyYZ")
    p.rod("CT_Brass", (-0.08, 0.0, 1.05), (0.08, 0.0, 1.07), 0.014, 4)
    p.lathe("CT_Wood", 0.1, 0.0, [(0.02, 1.07), (0.025, 1.09), (0.02, 1.11), (0.008, 1.115)], 6)
    p.rod("CT_Brass", (-0.08, 0.0, 1.035), (-0.08, 0.0, 1.06), 0.02, 4)
    p.box("CT_Paper", 0.04, 0.2, -0.17, -0.12, 0.99, 0.995, f="xXyYZ")
    p.rod("CT_Wood", (-0.19, -0.1, 1.0), (-0.07, -0.15, 1.0), 0.012, 5, caps=True)
    return p.done(footprint=(-0.25, 0.25, -0.25, 0.25), top=1.0)


def _tide_staff(B, x, y, z, yaw):
    """A sea-level staff: a painted post with bands every 10 cm and red decimetre marks, on a stand."""
    p = interior_kit._P(B, x, y, z, yaw, "level_staff")
    p.kind = "floor"
    p.box("CT_ConcreteDark", -0.2, 0.2, -0.15, 0.15, 0.0, 0.08, f="xXyYZ")
    p.box("CT_Wood", -0.09, 0.09, -0.02, 0.02, 0.08, 1.9, f="xXyYZz")
    for k in range(16):
        zz = 0.12 + k * 0.11
        p.box("CT_PlasterBlue" if k % 2 == 0 else "CT_TileWhite", -0.085, 0.085, -0.025, -0.02, zz, zz + 0.1, f="y")
        if k % 5 == 0:
            p.box("CT_PlasterRed", 0.03, 0.085, -0.027, -0.025, zz, zz + 0.02, f="y")
    p.rod("CT_Steel", (-0.12, 0.0, 0.25), (-0.12, 0.0, 1.4), 0.014, 3)
    p.rod("CT_Steel", (0.12, 0.0, 0.25), (0.12, 0.0, 1.4), 0.014, 3)
    return p.done(footprint=(-0.2, 0.2, -0.15, 0.15), top=1.9)


def _bottle_box(B, x, y, z, yaw):
    """The bottle post: a tarred chest with a slot, a stack of corked bottles lying on a rack and a paper tag."""
    p = interior_kit._P(B, x, y, z, yaw, "bottle_post")
    p.kind = "floor"
    w, d, h = 0.5, 0.32, 0.5
    p.box("CT_ConcreteDark", -w / 2, w / 2, -d / 2, d / 2, 0.0, h, f="xXyYZ")
    p.box("CT_Wood", -w / 2 - 0.02, w / 2 + 0.02, -d / 2 - 0.02, d / 2 + 0.02, h, h + 0.04, f="xXyYZz")
    p.box("CT_Steel", -0.1, 0.1, -d / 2 - 0.023, -d / 2 - 0.02, h + 0.01, h + 0.03, f="y")
    for k in range(3):
        zz = 0.07 + k * 0.1
        p.rod("CT_Steel", (-w / 2 - 0.03, -d / 2 - 0.02, zz), (w / 2 + 0.03, -d / 2 - 0.02, zz), 0.012, 3)
    for k, u in enumerate((-0.14, 0.0, 0.14)):
        _tube_y(p, "CT_PlasterGreen" if k != 1 else "CT_Glass", u, h + 0.09, [-0.13, -0.1, 0.06, 0.1, 0.13],
                [0.015, 0.045, 0.045, 0.02, 0.015], 8)
        p.cyl("CT_Wood", u, -0.135, h + 0.075, h + 0.105, 0.014, 5)
    p.card_front("paper_letter", 0.14, 0.22, 0.16, 0.3, -d / 2 - 0.0235)
    return p.done(top=h + 0.04, footprint=(-0.28, 0.28, -0.18, 0.18))


def _plaque_stand(B, x, y, z, yaw):
    """A weathered information stand: two posts and a slanted board with a notice."""
    p = interior_kit._P(B, x, y, z, yaw, "plaque_stand")
    p.kind = "floor"
    for sx in (-0.38, 0.38):
        p.cyl("CT_Wood", sx, 0, 0.0, 1.15, 0.04, 6, top=True)
    r0 = [(-0.47, -0.05, 0.62), (0.47, -0.05, 0.62), (0.47, 0.03, 1.1), (-0.47, 0.03, 1.1)]
    r1 = [(-0.47, -0.08, 0.62), (0.47, -0.08, 0.62), (0.47, 0.0, 1.1), (-0.47, 0.0, 1.1)]
    p.loft("CT_WoodLight", [r0, r1], top=True, bottom=True)
    p.card_tilt("notice_1", -0.4, 0.4, -0.085, 0.67, -0.0, 1.05, rect=None)
    p.box("CT_Brass", -0.47, 0.47, -0.095, -0.08, 0.6, 0.62, f="xXyZz")
    return p.done(top=1.15, footprint=(-0.5, 0.5, -0.1, 0.1))


def _flag_mast(B, x, y, z, yaw):
    """A flag mast on the gallery: pole, spar, halyard and three pennants (the day's signal)."""
    p = interior_kit._P(B, x, y, z, yaw, "flag_mast")
    p.kind = "floor"
    p.cyl("CT_Steel", 0, 0, 0.0, 0.18, 0.1, 8)
    p.cyl("CT_Wood", 0, 0, 0.0, 3.3, 0.04, 6, top=True)
    p.lathe("CT_Brass", 0, 0, [(0.02, 3.3), (0.04, 3.33), (0.02, 3.37)], 6)
    p.rod("CT_Wood", (-0.4, 0.0, 2.9), (0.4, 0.0, 2.9), 0.03, 4)
    p.tube("CT_Paper", [(-0.4, 0.0, 2.9), (-0.38, 0.03, 2.2), (-0.3, 0.05, 1.5)], 0.008, 3)
    for k, (c, w_, h_) in enumerate((("CT_PlasterRed", 0.45, 0.2), ("CT_PlasterBlue", 0.38, 0.17), ("CT_PlasterYellow", 0.3, 0.14))):
        zz = 3.2 - 0.3 * k
        q1 = [(0.0, 0.0, zz), (0.0, 0.0, zz - h_), (w_ * 0.5, 0.1, zz - h_ * 0.5)]
        q2 = [(w_ * 0.5, 0.1, zz - h_ * 0.5), (0.0, 0.0, zz - h_), (w_, 0.04, zz - h_ * 0.55)]
        q3 = [(0.0, 0.0, zz), (w_ * 0.5, 0.1, zz - h_ * 0.5), (w_, 0.04, zz - h_ * 0.5)]
        for q in (q1, q2, q3):
            p.face(c, q)
            p.face(c, q[::-1])
    return p.done(footprint=(-0.1, 0.1, -0.1, 0.1), top=None)


def _gull(B, x, y, z, yaw, mat="CT_TileWhite"):
    """A gull standing on the rail (an animal prop, 100 triangles)."""
    p = interior_kit._P(B, x, y, z, yaw, "gull")
    p.kind = "prop"
    _tube_y(p, mat, 0.0, 0.14, [-0.14, -0.06, 0.08, 0.2], [0.035, 0.07, 0.065, 0.02], 8)
    p.lathe(mat, 0.0, -0.14, [(0.025, 0.2), (0.035, 0.24), (0.03, 0.28), (0.01, 0.295)], 6)
    p.face("CT_PlasterYellow", [(-0.012, -0.17, 0.25), (0.012, -0.17, 0.25), (0.0, -0.24, 0.24)])
    p.face("CT_PlasterYellow", [(0.012, -0.17, 0.25), (-0.012, -0.17, 0.25), (0.0, -0.24, 0.24)])
    for s_ in (-1, 1):
        wq = [(s_ * 0.06, -0.06, 0.2), (s_ * 0.075, 0.1, 0.19), (s_ * 0.06, 0.24, 0.15), (s_ * 0.045, 0.0, 0.12)]
        p.face("CT_PlasterGrey", wq)
        p.face("CT_PlasterGrey", wq[::-1])
        p.rod("CT_PlasterYellow", (s_ * 0.025, 0.0, 0.1), (s_ * 0.025, 0.0, 0.0), 0.012, 3)
    return p.done(footprint=(-0.08, 0.08, -0.1, 0.1), top=0.3)


def keeper_extras2(B):
    """Third pass in the keeper's room: visitors' lectern and desk at the door, a light switch, ship's wheel, glass
    floats, bunting, a chess corner with its own lamp."""
    kit = interior_kit
    z0 = 0.03
    # light switch beside the door (facet 0)
    wx, wy = _wallpt(0, 0.8, 0.0)
    _switch_plate(B, wx, wy, z0, _yin(-90.0), 1.3)
    B.thing("tower_switch", "machine", (wx, wy + 0.05, z0 + 1.3), _yin(-90.0), what="switch", switch="room")
    # visitors' lectern: free-standing right of the door path, facing the door
    lx, ly = _pol(3.55, -62.0)
    _guest_lectern(B, lx, ly, z0, _yaw_to(CX - lx, CY - 4.4 - ly))
    B.thing("tower_guestbook", "board", (lx, ly, z0 + 1.1), _yaw_to(CX - lx, CY - 4.4 - ly), topic="guestbook")
    B.spot("guestbook_stand", "stand", (lx + 0.55, ly - 0.25, z0), _yaw_to(CX - lx, CY - 4.4 - ly) + 180.0)
    # the keeper's visitors' desk: a counter by the stair post, a steel-top table with a stamp pad and a bell
    dxp, dyp = _pol(2.95, -118.0)
    yw = _yaw_to(CX - dxp, CY - 4.4 - dyp)
    tb = kit.table(B, dxp, dyp, z0, yw, w=0.9, d=0.5, kind="steel")
    kit.cup(B, dxp - 0.28 * math.cos(math.radians(yw)), dyp - 0.28 * math.sin(math.radians(yw)), tb["top"])
    kit.books_stack(B, dxp + 0.25 * math.cos(math.radians(yw)), dyp + 0.25 * math.sin(math.radians(yw)), tb["top"], yaw=yw + 10, n=2, seed=4)
    B.thing("tower_desk", "counter", (dxp, dyp, z0 + 0.95), yw, role="lighthouse_keeper", service="visits")
    B.spot("desk_keeper", "stand", (dxp - 0.65 * math.sin(math.radians(yw)), dyp + 0.65 * math.cos(math.radians(yw)), z0), yw)
    # ship's wheel over the cot (facet 12), glass floats under the lifebuoy (facet 14)
    wx, wy = _wallpt(12, 0.0)
    _ships_wheel(B, wx, wy, z0, _yin(fac_(12)), 1.62)
    B.thing("keeper_wheel", "board", (wx, wy, z0 + 1.62), _yin(fac_(12)), topic="wheel")
    wx, wy = _wallpt(14, 0.0)
    kit.wall_shelf(B, wx, wy, z0, _yin(fac_(14)), w=0.9, zt=1.0, props=False)
    fx, fy = _wallpt(14, 0.0, 0.12)
    _float_row(B, fx, fy, z0 + 1.0, _yin(fac_(14)))
    B.thing("keeper_floats", "board", (fx, fy, z0 + 1.1), _yin(fac_(14)), topic="floats")
    # bunting across two wall corners and a doormat
    for a_, b_ in ((7, 9), (10, 12)):
        pa = _wallpt(a_, 0.0, 0.08)
        pb = _wallpt(b_, 0.0, 0.08)
        _bunting(B, (pa[0], pa[1], 2.58), (pb[0], pb[1], 2.58), sag=0.18, n=14)
    kit.rug(B, *_pol(4.15, -90.0), z0, w=1.2, d=0.8, seed=3)
    # chess corner (between the safe and the cot) with two stools and a lamp over it
    cxp, cyp = _pol(3.15, 135.0)
    kit.chess_table(B, cxp, cyp, z0, 45.0, seed=2)
    B.thing("keeper_chess", "play", (cxp, cyp, z0 + 0.78), 0.0, what="chess")
    kit.rug(B, cxp, cyp, z0, w=1.7, round=True, seed=5)
    tx_, ty_ = -math.sin(math.radians(135.0)), math.cos(math.radians(135.0))
    for k, sg in enumerate((-1, 1)):
        sx, sy = cxp + sg * 0.68 * tx_, cyp + sg * 0.68 * ty_
        kit.stool(B, sx, sy, z0, mat="CT_PlasterBlue", h=0.45)
        yws = _yaw_to(cxp - sx, cyp - sy)
        B.thing("keeper_chess_stool_%s" % "ab"[k], "seat", (sx, sy, z0 + 0.45), yws, height=0.45)
        B.spot("chess_seat_%s" % "ab"[k], "seat", (sx, sy, z0), yws)
    lx2, ly2 = _pol(3.15, 135.0)
    B.lamp((lx2, ly2, 2.3), lumens=520, colour="#FFD8A8", kind="public", radius=6.0)
    lbox(B.detail, "CT_Steel", Fr(lx2, ly2), -0.008, 0.008, -0.008, 0.008, 2.38, 2.6, faces="xXyY")


def fac_(k):
    return -90.0 + 22.5 * k


def gallery_extras2(B, r_out):
    """Third pass on the lamp gallery: Morse key, sea-level staff, bottle post, plaque, flag mast, gulls, a switch."""
    z = PLAT_Z
    ma = 22.0
    mx, my = _pol(r_out - 0.45, ma)
    _morse_post(B, mx, my, z, _yin(ma))
    B.thing("gallery_morse", "machine", (mx, my, z + 1.1), _yin(ma), what="morse")
    B.spot("morse_stand", "stand", _pol(r_out - 1.1, ma), _yout(ma))
    ga = 140.0
    gx, gy = _pol(r_out - 0.42, ga)
    _tide_staff(B, gx, gy, z, _yin(ga))
    B.thing("gallery_gauge", "board", (gx, gy, z + 1.2), _yin(ga), topic="gauge")
    ba = 282.0
    bx, by = _pol(r_out - 0.5, ba)
    _bottle_box(B, bx, by, z, _yin(ba))
    B.thing("gallery_bottlepost", "machine", (bx, by, z + 0.6), _yin(ba), what="bottle_post")
    B.spot("bottle_stand", "stand", _pol(r_out - 1.2, ba), _yout(ba))
    pa = 232.0
    px, py = _pol(r_out - 0.5, pa)
    _plaque_stand(B, px, py, z, _yin(pa))
    B.thing("gallery_plaque", "board", (px, py, z + 0.9), _yin(pa), topic="plaque")
    ma2 = 357.0
    fx, fy = _pol(r_out - 0.4, ma2)
    _flag_mast(B, fx, fy, z, _yaw_to(1.0, 0.3))
    for a_ in (42.0, 176.0, 268.0):
        gx2, gy2 = _pol(r_out - 0.08, a_)
        _gull(B, gx2, gy2, z + 1.05, (a_ + 90.0 + 15.0) % 360.0)
    # a switch for the gallery lamps on the lamp post at 190 deg
    sa = 190.0
    spx, spy = _pol(r_out - 0.2 - 0.04, sa)
    _switch_plate(B, spx, spy, z, _yin(sa), 1.4)
    B.thing("gallery_switch", "machine", (spx, spy, z + 1.4), _yin(sa), what="switch", switch="room")
    # one more lamp over the morse key and the staff
    lx, ly = _pol(r_out - 0.2, 305.0)
    B.lamp((lx, ly, z + 2.3), lumens=600, colour="#FFE2B8", kind="lobby", radius=8.0)
    lbox(B.detail, "CT_Steel", Fr(lx, ly), -0.03, 0.03, -0.03, 0.03, z, z + 2.15, faces="xXyY")


def gallery_extras(B, r_out):
    """The lamp gallery: fog bell, signalling lamp, weather instruments, a table of ships' lights, a lamp."""
    z = PLAT_Z
    bell = 335.0
    bx, by = _pol(r_out - 0.55, bell)
    _fog_bell(B, bx, by, z, _yin(bell) + 180.0)
    B.thing("gallery_bell", "machine", (bx - 0.2 * math.cos(math.radians(bell)), by - 0.2 * math.sin(math.radians(bell)), z + 1.2),
            _yout(bell), what="fog_bell")
    B.spot("bell_stand", "stand", _pol(r_out - 1.35, bell), _yout(bell))
    sa = 8.0
    sx, sy = _pol(r_out - 0.5, sa)
    _signal_lamp(B, sx, sy, z, _yout(sa))
    B.thing("gallery_signal", "machine", (sx, sy, z + 1.2), _yout(sa), what="signal_lamp")
    wa = 52.0
    wx, wy = _pol(r_out - 0.55, wa)
    _stevenson(B, wx, wy, z, _yout(wa))
    ax, ay = _pol(r_out - 0.2, wa + 9.0)
    _anemometer(B, ax, ay, z, 0.0)
    B.thing("gallery_weather", "board", (wx, wy, z + 1.1), _yout(wa), topic="gallery_weather")
    la = 100.0
    lx, ly = _pol(r_out - 0.55, la)
    _lights_lectern(B, lx, ly, z, _yin(la) + 180.0)
    B.thing("gallery_ships", "board", (lx, ly, z + 1.12), _yout(la), topic="ships")
    B.spot("ships_watch", "view", _pol(r_out - 1.2, la), _yout(la))
    gx, gy = _pol(r_out - 0.2, 190.0)
    B.lamp((gx, gy, z + 2.3), lumens=600, colour="#FFE2B8", kind="lobby", radius=8.0)
    lbox(B.detail, "CT_Steel", Fr(gx, gy), -0.03, 0.03, -0.03, 0.03, z, z + 2.15, faces="xXyY")
    gallery_extras2(B, r_out)


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
    gallery_extras(B, r_out)


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
