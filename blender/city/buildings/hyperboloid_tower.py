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


def keeper_room(B):
    """The keeper's room in the drum: a desk with a radio, a chair, a cabinet, a cot, buoys, rope and three lamps
    (floor z = 0.03, ceiling underside 2.6; the spiral stair stands in the middle, r < 1.8, and the door is at -y)."""
    z0 = 0.03
    d = B.detail
    rnd = random.Random(1923)
    # desk at the back (+y) against the wall, facing the room
    ydesk = CY + 4.15
    fr = Fr(CX - 0.4, ydesk, 0.0, z0)
    lbox(B.m, "CT_WoodLight", fr, -1.2, -0.9, 0.0, 0.7, 0.0, 0.72)
    lbox(B.m, "CT_WoodLight", fr, 0.9, 1.2, 0.0, 0.7, 0.0, 0.72)
    lbox(B.m, "CT_WoodLight", fr, -1.2, 1.2, 0.0, 0.7, 0.72, 0.76)
    lbox(B.m, "CT_WoodLight", fr, -0.9, 0.9, 0.55, 0.62, 0.3, 0.72)             # modesty panel (the desk sits against the wall)
    # radio set: a box with a dial and an antenna, a headset, a logbook and a mug, a map on the wall
    lbox(d, "CT_ConcreteDark", fr, 0.2, 0.75, 0.1, 0.45, 0.76, 0.98, faces="xXyYZ")
    lbox(d, "CT_Neon", fr, 0.28, 0.5, 0.45, 0.46, 0.84, 0.92, faces="y")
    ax, ay, _ = fr.p(0.7, 0.15)
    cyl(d, "CT_Steel", ax, ay, z0 + 0.98, z0 + 1.7, 0.012, 4)
    lbox(d, "CT_PlasterBlue", fr, -0.8, -0.35, 0.15, 0.5, 0.76, 0.79, faces="xXyYZ")
    lbox(d, "CT_TileWhite", fr, -0.7, -0.45, 0.2, 0.42, 0.79, 0.81, faces="xXyYZ")
    cyl(d, "CT_PlasterRed", *fr.p(-0.1, 0.3)[:2], z0 + 0.76, z0 + 0.86, 0.04, 6)
    lbox(d, "CT_TileWhite", Fr(CX - 0.4, CY + 4.9, 0.0, z0), -0.9, 0.5, 0.0, 0.03, 1.3, 2.0)       # chart on the wall
    for k in range(4):
        lbox(d, rnd.choice(("CT_PlasterBlue", "CT_PlasterGreen", "CT_PlasterYellow")), Fr(CX - 0.4, CY + 4.9, 0.0, z0),
             -0.8 + 0.3 * k, -0.6 + 0.3 * k, 0.03, 0.04, 1.4 + 0.1 * k, 1.5 + 0.12 * k, faces="xXyYZ")
    # the keeper's chair behind the desk, facing +y (the sitter faces the desk)
    fu.chair(B, CX - 0.4, CY + 3.3, 180, "CT_PlasterRed", z=z0)
    B.spot("keeper_seat", "seat", (CX - 0.4, CY + 3.3, z0), 180.0)
    # tall cabinet on the east wall (angle ~0 deg), with shelves and tins
    fc = Fr(CX + 4.88, CY - 0.6, 270, z0)             # faces -x into the room; u runs -y
    lbox(B.m, "CT_WoodLight", fc, 0.0, 1.5, 0.0, 0.5, 0.0, 2.0)
    for k in range(5):
        lbox(d, "CT_ConcreteDark", fc, 0.04, 0.72, 0.5, 0.52, 0.2 + 0.38 * k, 0.22 + 0.38 * k, faces="Y")
        lbox(d, "CT_ConcreteDark", fc, 0.78, 1.46, 0.5, 0.52, 0.2 + 0.38 * k, 0.22 + 0.38 * k, faces="Y")
    lbox(d, "CT_Steel", fc, 0.7, 0.8, 0.5, 0.56, 0.9, 1.05)
    # a cot along the west wall, a rolled blanket on it
    fw = Fr(CX - 4.88, CY + 0.2, 90, z0)               # faces +x; u runs +y
    lbox(B.m, "CT_Steel", fw, -0.1, 1.9, 0.0, 0.8, 0.0, 0.38)
    lbox(d, "CT_PlasterGreen", fw, 0.0, 1.8, 0.02, 0.78, 0.38, 0.45, faces="xXyYZ")
    cyl(d, "CT_PlasterRed", *fw.p(1.65, 0.4)[:2], z0 + 0.45, z0 + 0.6, 0.12, 6)
    B.spot("keeper_cot", "bench", (CX - 3.6, CY + 1.1, z0), 90.0)
    # life rings and a rope coil by the door; lifebuoys hang on the wall beside it
    # a rope coil by the door side
    rx, ry = CX - 3.2, CY - 3.2
    cyl(d, "CT_WoodLight", rx, ry, z0, z0 + 0.35, 0.28, 8, top=False)
    cyl(d, "CT_TileWhite", rx, ry, z0 + 0.35, z0 + 0.38, 0.28, 8)
    cyl(d, "CT_ConcreteDark", rx, ry, z0 + 0.38, z0 + 0.4, 0.1, 6)
    # lamps: over the desk, over the cot and over the door side
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
        holes = [(0.28, L - 0.28, 0.55, 2.2)] if k not in (2, 5, 8, 11) else []
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
    B.note("%d stair treads up to %.1f m; lattice %d bars x2 x3 sections" % (n, PLAT_Z, NBARS))
