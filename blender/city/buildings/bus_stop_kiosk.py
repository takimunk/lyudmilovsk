"""Bus-stop kiosk: a sculptural concrete shelter with a small glazed kiosk (ice cream, newspapers, sodas).

Original design in the spirit of the wild concrete bus stops of the 1960s-80s USSR (folded-plate roofs, big
mosaic "suns" on a wave wall): a tall wave-shaped back wall rises to a fin on the right, a folded
two-triangle concrete roof cantilevers from it and is propped by two outward-leaning piers; behind
the roof a mosaic sun (blue / white tile rings) decorates the wall over a wooden bench. At the right a small hollow
kiosk box (white tile, red fascia, serving window with a wooden shelf, side window, side door).

Plot 6 x 5 m, height 4.95 m. Not enterable (the seller's room is hollow, one door, no stairs).
Spots: bench (under the roof), counter (seller inside), stand (buyer at the window).
Window kinds: "public" (serving window + side window). No flats.
Ideas for later: a timetable board, a ticket-machine, a litter bin, an ice-cream sign (neon), tile variants.
Requests: none.
"""
import math

from citykit import box, prism, DOOR_H  # noqa: F401
import geom

INFO = {
    "name_ru": "Остановка-киоск", "name_en": "Bus-stop Kiosk",
    "style": "late-soviet expressionism", "use": "public",
    "inspired_by": "Soviet concrete bus stops of the 1960s-80s (folded-plate roofs, mosaic panels, wave walls)",
}

# roof corner heights (top face), x -2.9..2.9, y 0.7..4.6
RX0, RX1, RY0, RY1 = -2.9, 2.9, 0.7, 4.6
ROOF_T = 0.28
Z_FL, Z_FR, Z_BR, Z_BL = 2.9, 3.4, 4.5, 3.6


def xz_prism(m, mat, poly, y0, y1, side_mat=None):
    """Solid shape: polygon in (x, z) (any winding) extruded between y0 < y1 (front face looks to -y)."""
    n = len(poly)
    area = sum(poly[i][0] * poly[(i + 1) % n][1] - poly[(i + 1) % n][0] * poly[i][1] for i in range(n))
    if area < 0:
        poly = poly[::-1]
    for a, b, c in geom.triangulate(poly):
        pa, pb, pc = poly[a], poly[b], poly[c]
        m.face(mat, [(pa[0], y0, pa[1]), (pb[0], y0, pb[1]), (pc[0], y0, pc[1])])
        m.face(mat, [(pa[0], y1, pa[1]), (pc[0], y1, pc[1]), (pb[0], y1, pb[1])])
    for k in range(n):
        (ax, az), (bx, bz) = poly[k], poly[(k + 1) % n]
        m.face(side_mat or mat, [(ax, y0, az), (ax, y1, az), (bx, y1, bz), (bx, y0, bz)][::-1][::-1])


def tri_slab(m, mat, p, t, hidden=()):
    """A triangular slab: top face through the 3 points p (x, y, z), thickness t downwards.
    hidden: indices k of edges (p[k], p[k+1]) whose side face is left out (shared with another slab)."""
    a, b, c = p
    nz = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
    if nz < 0:
        p = [a, c, b]
    top = list(p)
    bot = [(x, y, z - t) for x, y, z in p]
    m.face(mat, top)
    m.face(mat, bot[::-1])
    for k in range(3):
        k2 = (k + 1) % 3
        if (frozenset((tuple(top[k]), tuple(top[k2])))) in hidden:
            continue
        m.face(mat, [bot[k], bot[k2], top[k2], top[k]])


def roof_z(x, y):
    """Top height of the folded roof at (x, y): two planar triangles split on the FL-BR diagonal."""
    fl, fr, br, bl = (RX0, RY0, Z_FL), (RX1, RY0, Z_FR), (RX1, RY1, Z_BR), (RX0, RY1, Z_BL)
    # which side of the diagonal fl->br
    side = (br[0] - fl[0]) * (y - fl[1]) - (br[1] - fl[1]) * (x - fl[0])
    a, b, c = (fl, br, bl) if side > 0 else (fl, fr, br)
    d = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
    l1 = ((x - a[0]) * (c[1] - a[1]) - (y - a[1]) * (c[0] - a[0])) / d
    l2 = ((b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0])) / d
    return a[2] + l1 * (b[2] - a[2]) + l2 * (c[2] - a[2])


def _disc(m, cx, cz, y, rings):
    """Concentric mosaic rings on the plane y facing -y: rings = [(r0, r1, mat)]."""
    segs = 24
    for r0, r1, mat in rings:
        for k in range(segs):
            a0, a1 = 2 * math.pi * k / segs, 2 * math.pi * (k + 1) / segs
            P = lambda r, a: (cx + r * math.cos(a), y, cz + r * math.sin(a))
            if r0 == 0:
                m.face(mat, [P(0, 0), P(r1, a0), P(r1, a1)][::-1][::-1])
            else:
                m.face(mat, [P(r0, a0), P(r1, a0), P(r1, a1), P(r0, a1)])


def build(B):
    m, d = B.m, B.detail
    # ---- wave back wall (y 4.2..4.6): top curves from 3.5 at the left to a fin of 4.95 at the right
    prof = [(-2.95, 0.0), (2.95, 0.0)]
    n = 18
    for i in range(n + 1):
        t = i / n
        x = 2.95 - 5.9 * t
        s = t * t * (3 - 2 * t)
        z = 4.95 - 1.45 * s + 0.18 * math.sin(t * math.pi * 2)
        prof.append((x, z))
    xz_prism(m, "CT_Concrete", prof, 4.2, 4.6)
    # ---- mosaic sun on the wall, over the bench
    sy = 4.2 - 0.03
    cx, cz = -0.95, 1.7
    _disc(m, cx, cz, sy, [(0, 0.3, "CT_Tile"), (0.3, 0.62, "CT_TileWhite"), (0.62, 0.95, "CT_Tile"),
                          (0.95, 1.08, "CT_TileWhite")])
    # rays: blue/white tile blocks around the sun's upper half
    for k in range(7):
        a = math.pi * (0.0 + 1.0 * k / 6)
        r0, r1 = 1.2, 1.7 if k % 2 == 0 else 1.5
        w = 0.12
        ca, sa = math.cos(a), math.sin(a)
        p0 = (cx + r0 * ca, sy, cz + r0 * sa)
        p1 = (cx + r1 * ca, sy, cz + r1 * sa)
        nx, nz = -sa * w, ca * w
        m.face("CT_Tile" if k % 2 == 0 else "CT_TileWhite",
               [(p0[0] - nx, sy, p0[2] - nz), (p0[0] + nx, sy, p0[2] + nz), (p1[0] + nx, sy, p1[2] + nz), (p1[0] - nx, sy, p1[2] - nz)])
    # base tile band along the wall under the bench
    box(m, "CT_TileWhite", -2.9, 0.5, 4.17, 4.2, 0.0, 0.55)
    # ---- folded roof: two triangles meeting on the FL-BR diagonal
    fl, fr, br, bl = (RX0, RY0, Z_FL), (RX1, RY0, Z_FR), (RX1, RY1, Z_BR), (RX0, RY1, Z_BL)
    diag = frozenset(((fl), (br)))
    tri_slab(m, "CT_Concrete", [fl, fr, br], ROOF_T, hidden={frozenset((fl, br))})
    tri_slab(m, "CT_Concrete", [fl, br, bl], ROOF_T, hidden={frozenset((fl, br))})
    # red fascia stripe on the front edge of the roof
    # ---- leaning piers (foot inside, head outside under the roof)
    for foot0, foot1, head0, head1, yc in ((-2.6, -2.2, -2.95, -2.55, 1.0), (2.15, 2.55, 2.6, 3.0, 1.0)):
        zt = roof_z((head0 + head1) / 2, yc) - ROOF_T + 0.15
        xz_prism(m, "CT_Concrete", [(foot0, 0.0), (foot1, 0.0), (head1, zt), (head0, zt)], yc - 0.2, yc + 0.2)
    # ---- the kiosk: hollow box x 0.6..2.8, y 2.2..4.2, walls 0.15 thick
    kx0, kx1, ky0, ky1, kh = 0.6, 2.8, 2.2, 4.2, 2.45
    W = 0.15
    # front (serving window hole)
    F = B.facade((kx0, ky0), (kx1, ky0), 0.0, kh, "CT_TileWhite", holes=[(0.35, 1.95, 1.0, 2.15)], depth=W)
    B.facade_window(F, 0.35, 1.95, 1.0, 2.15, kind="public", cols=2, rows=1)
    # left (side window): outside faces -x, runs from back to front
    F2 = B.facade((kx0, ky1), (kx0, ky0), 0.0, kh, "CT_TileWhite", holes=[(0.55, 1.45, 1.0, 2.05)], depth=W)
    B.facade_window(F2, 0.55, 1.45, 1.0, 2.05, kind="public")
    # right (door hole)
    F3 = B.facade((kx1, ky0), (kx1, ky1), 0.0, kh, "CT_TileWhite", holes=[(0.9, 1.8, 0.0, 2.0)], depth=W)
    box(d, "CT_Door", kx1 - W + 0.03, kx1 - W + 0.08, ky0 + 0.9, ky0 + 1.8, 0.02, 2.0)
    # the back wall of the kiosk is the concrete wall; floor and roof
    box(m, "CT_Floor", kx0 - W, kx1, ky0 - W, ky1, 0.0, 0.02, faces="Z")
    box(m, "CT_Concrete", kx0 - 0.35, kx1 + 0.2, ky0 - 0.35, ky1, kh, kh + 0.15)
    box(m, "CT_SteelRed", kx0 - 0.37, kx1 + 0.22, ky0 - 0.37, ky0 - 0.34, kh - 0.02, kh + 0.30, faces="all")
    # serving shelf under the window
    box(m, "CT_Wood", kx0 + 0.25, kx1 - 0.45, ky0 - 0.30, ky0, 0.96, 1.02)
    for u in (kx0 + 0.45, kx1 - 0.65):
        box(m, "CT_Steel", u, u + 0.05, ky0 - 0.28, ky0 - 0.03, 0.80, 0.96)
    # red awning blades over the window
    box(m, "CT_SteelRed", kx0 + 0.25, kx1 - 0.45, ky0 - 0.55, ky0 - 0.05, 2.22, 2.27)
    # ---- bench under the roof: concrete ends, three timber slats and back
    bx0, bx1 = -2.3, -0.2
    for u in (bx0 + 0.1, bx1 - 0.35):
        box(m, "CT_Concrete", u, u + 0.25, 3.45, 3.95, 0.0, 0.42)
    for j in range(3):
        box(d, "CT_Wood", bx0, bx1, 3.45 + j * 0.17, 3.45 + j * 0.17 + 0.14, 0.42, 0.47)
    # litter bin / pillar tiling accent
    box(m, "CT_Steel", -2.95, -2.7, 0.2, 0.45, 0.0, 0.75)
    # ---- life
    B.spot("bench", "bench", (-1.25, 3.7, 0.0), 0.0)
    B.spot("counter", "counter", (1.5, 3.2, 0.0), 0.0)
    B.spot("stand", "stand", (1.4, 1.3, 0.0), 180.0)
    B.footprint = [(-3.0, 0.0), (3.0, 0.0), (3.0, 5.0), (-3.0, 5.0)]
    B.height = 4.95
