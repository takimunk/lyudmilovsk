"""Round sanatorium («Санаторий «Корона»»): Soviet modernist resort ring, an original design.

In the spirit of the round sanatorium buildings of the 1980s Crimean / Black Sea coast: a ring (44 m outer
diameter at the balcony tips, 10 m deep) of 5 storeys of guest rooms, floating over a lower podium (R 18 m) on
cantilevers and slanting concrete struts. The ring is cut into 24 room bays; each bay's outer wall is a long
angled window face turned 40 degrees from the radial, so the outline is a sawtooth "crown"; the triangular notch in
front of every window face is that room's balcony (slab with railing), the short return wall of the neighbouring
bay closes it and is painted blue / yellow / pink in turn. Light concrete + white + colour.

Size 44 x 44 m (plot 60 x 60), height ~22 m. 5 storeys x 24 bays = 120 flats ("<storey>-<nn>", 1-2 beds), each with
a living-room window and a balcony-door window (240 flat windows) + corridor windows towards the courtyard.
Podium: solid blue-tile wall, except the ground hall in the 90 degree sector facing the street: ENTERABLE lobby
with the main entrance (under the first balcony tips), reception counter, benches, a dining area; floor
CT_Floor, walls CT_Interior, ceiling; big windows to the sea side and to the courtyard.
Courtyard (R 12 m): lawn, fountain basin, trees (not reachable tonight).
Spots: counter (reception), bench x2, table x3 (dining). Entrance "main" (2.4 m, double leaf).
Ideas for later: garden door from the hall, roof solarium, a sea-water pool in the courtyard.
Material requests: none.
"""
import math
import random

from citykit import box, prism, ring, Facade, Mesh, wall_with_holes, DOOR_W
import geom
from furn_int2 import Furn

INFO = {
    "name_ru": "Санаторий «Корона»", "name_en": "Crown Sanatorium",
    "style": "soviet modernism", "use": "residential",
    "inspired_by": "1980s round resort buildings of the Crimean / Black Sea coast (ring sanatoria on stilts), "
                   "Soviet late-modernist concrete with painted accents; the sawtooth crown of angled balcony "
                   "bays and all proportions are our own",
}

CX, CY = 0.0, 26.0
R_IN, R_P, R_BODY, R_TIP = 12.0, 18.0, 18.6, 22.0
NB = 24
HB = 7.5                    # half bay angle, degrees
DELTA = 0.6                 # the tip leads the bay start by this angle
STH = 3.1                   # storey height
Z0 = 4.5                    # underside of the ring body (top of the podium)
NS = 5
ZT = Z0 + NS * STH          # roof level = 20.0


def pol(R, deg, cx=CX, cy=CY):
    a = math.radians(deg)
    return (cx + R * math.cos(a), cy + R * math.sin(a))


def _orient(pts, want):
    n = geom.cross(geom.sub(pts[1], pts[0]), geom.sub(pts[2], pts[0]))
    if n[0] * want[0] + n[1] * want[1] + n[2] * want[2] < 0:
        return pts[::-1]
    return pts


def hexa(m, mat, v):
    c = tuple(sum(p[i] for p in v) / 8 for i in range(3))
    for q in [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]:
        pts = [v[i] for i in q]
        fc = tuple(sum(p[i] for p in pts) / 4 for i in range(3))
        m.face(mat, _orient(pts, tuple(fc[i] - c[i] for i in range(3))))


def beam(m, mat, a, b, w, zlo, zhi):
    L = math.hypot(b[0] - a[0], b[1] - a[1])
    hx, hy = (b[0] - a[0]) / L, (b[1] - a[1]) / L
    sx, sy = -hy, hx
    ringc = [(-w / 2, zlo), (w / 2, zlo), (w / 2, zhi), (-w / 2, zhi)]
    s0 = [(a[0] + sx * l, a[1] + sy * l, a[2] + vz) for l, vz in ringc]
    s1 = [(b[0] + sx * l, b[1] + sy * l, b[2] + vz) for l, vz in ringc]
    hexa(m, mat, [s0[0], s0[1], s1[1], s1[0], s0[3], s0[2], s1[2], s1[3]])


def inset_poly(poly, d):
    """CCW polygon moved inward by d (mitred)."""
    n = len(poly)
    lines = []
    for i in range(n):
        p, q = poly[i], poly[(i + 1) % n]
        L = math.hypot(q[0] - p[0], q[1] - p[1])
        ux, uy = (q[0] - p[0]) / L, (q[1] - p[1]) / L
        nx, ny = -uy, ux                       # left = inward for CCW
        lines.append(((p[0] + nx * d, p[1] + ny * d), (ux, uy)))
    out = []
    for i in range(n):
        (p1, d1), (p2, d2) = lines[i - 1], lines[i]
        den = d1[0] * d2[1] - d1[1] * d2[0]
        if abs(den) < 1e-9:
            out.append(p2)
            continue
        t = ((p2[0] - p1[0]) * d2[1] - (p2[1] - p1[1]) * d2[0]) / den
        out.append((p1[0] + d1[0] * t, p1[1] + d1[1] * t))
    return out


def tri_orient_up(pts2, z):
    area = sum(pts2[i][0] * pts2[(i + 1) % len(pts2)][1] - pts2[(i + 1) % len(pts2)][0] * pts2[i][1] for i in range(len(pts2)))
    p = pts2 if area > 0 else pts2[::-1]
    return [(x, y, z) for x, y in p]


def _furnish_hall(B, hall, t):
    """Round 2: the ground hall (annular sector R 12..18, angles -135..-45, ceiling 4.5): reception, dining, lounge, piano."""
    f = Furn(B)
    d = B.detail
    WARM = "#FFD2A0"
    # ---- reception desk dressing + key board on the inner wall behind it
    box(d, "CT_Steel", 3.6, 4.1, 11.45, 11.9, 1.14, 1.5)
    box(d, "CT_Frame", 3.55, 4.15, 11.5, 11.55, 1.2, 1.55)
    f.cup(6.3, 11.6, 1.14, mat="CT_PlasterRed", h=0.07)
    f.cup(5.6, 11.5, 1.14, mat="CT_PlasterYellow", h=0.07)
    box(d, "CT_PlasterBlue", 3.05, 6.95, 11.17, 11.2, 0.15, 0.95, faces="y")
    # the long walls are glazed: wall art goes on the solid end walls, a hanging sign over the desk
    f.panel(5.0, 12.0, 2.95, 2.6, 0.5, 0.0, mat="CT_PlasterRed", frame="CT_Frame")
    for sx in (3.9, 6.1):
        box(d, "CT_Steel", sx - 0.01, sx + 0.01, 11.97, 11.99, 3.2, 4.5)
    px, py = pol(14.9, -46.5)
    f.panel(px, py, 2.4, 1.6, 1.0, 315.0, mat="CT_PlasterBlue", frame="CT_Wood")
    px, py = pol(15.0, -133.4)
    f.panel(px, py, 1.9, 1.4, 1.0, 45.0, mat="CT_PlasterYellow", frame="CT_Frame")
    # ---- dining: chairs, cloths, tableware, pendants (tables are boxes 1.6 x 1.6 at the "dining_i" spots)
    for i, th in enumerate((-128.0, -118.0, -108.0)):
        x, y = pol(15.0, th)
        box(d, "CT_TileWhite", x - 0.85, x + 0.85, y - 0.85, y + 0.85, 0.76, 0.772, faces="Z")
        for dx, dy, yaw in ((-1.05, 0.0, 90.0), (1.05, 0.0, 270.0), (0.0, -1.05, 180.0), (0.0, 1.05, 0.0)):
            f.chair(x + dx, y + dy, yaw=yaw, seat=("CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterPink")[i])
        f.cup(x - 0.3, y - 0.3, 0.772, mat="CT_PlasterRed", h=0.18)
        f.cup(x + 0.3, y + 0.25, 0.772, mat="CT_TileWhite", h=0.1)
        f.cup(x + 0.3, y - 0.3, 0.772, mat="CT_PlasterYellow", h=0.06)
        f.cup(x - 0.3, y + 0.3, 0.772, mat="CT_PlasterGreen", h=0.06)
        f.pendant(x, y, 4.5, lumens=800, colour=WARM, radius=6.5, drop=1.1)
    # ---- lounge: armchairs round a low table, a rug, plants
    f.round_table(8.0, 12.9, r=0.4, h=0.42, top="CT_Wood")
    f.armchair(7.0, 12.9, yaw=90.0, mat="CT_PlasterRed")
    f.armchair(8.0, 11.9, yaw=180.0, mat="CT_PlasterRed")
    f.armchair(9.0, 12.9, yaw=270.0, mat="CT_PlasterYellow")
    f.rug(6.6, 9.5, 11.5, 14.0, mat="CT_PlasterBlue")
    B.spot("lounge_1", "seat", (7.0, 12.9, 0.0), 90.0)
    B.spot("lounge_2", "seat", (8.0, 11.9, 0.0), 180.0)
    B.spot("lounge_3", "seat", (9.0, 12.9, 0.0), 270.0)
    # ---- piano against the east end wall (radial line at -45 deg), stool in front
    pxy = pol(15.0, -47.9)
    def piano(m):
        box(m, "CT_Frame", -0.75, 0.75, -0.1, 0.3, 0.2, 1.25)
        box(m, "CT_Frame", -0.78, 0.78, -0.12, 0.3, 1.25, 1.28)
        box(m, "CT_TileWhite", -0.7, 0.7, -0.38, -0.1, 0.68, 0.72)
        box(m, "CT_Steel", -0.7, 0.7, -0.38, -0.34, 0.6, 0.68)
        for sx in (-1, 1):
            box(m, "CT_Frame", sx * 0.7 - 0.05, sx * 0.7 + 0.05, -0.34, 0.26, 0.0, 0.2)
        box(m, "CT_Frame", -0.75, 0.75, -0.1, 0.3, 0.0, 0.2, faces="xXyY")
    f.put(piano, pxy[0], pxy[1], 315.0, solid=True)
    sx_, sy_ = pxy[0] - 0.75 * math.sin(math.radians(45)) * 1.0 - 0.0, pxy[1] - 0.75 * math.sin(math.radians(45))
    f.stool(sx_ - 0.1, sy_ - 0.1, seat="CT_PlasterRed", h=0.5)
    B.spot("piano", "seat", (sx_ - 0.1, sy_ - 0.1, 0.0), 135.0)
    # ---- plants, floor border and skirting along the walls
    for px, py in ((-1.9, 9.0), (1.9, 9.0), (-6.2, 10.0), pol(13.5, -131.5), pol(16.4, -131.5), pol(13.2, -54.0)):
        f.plant(px, py, h=1.7, pot=("CT_PlasterRed", "CT_TileWhite")[int(px * 3) % 2])
    q = inset_poly(hall, t)
    nh = len(q)
    for i in range(nh):
        a, b = q[i], q[(i + 1) % nh]
        if i == 3:
            mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
            ux, uy = (b[0] - a[0]), (b[1] - a[1])
            L = math.hypot(ux, uy)
            ux, uy = ux / L, uy / L
            f.skirting([a, (mx - ux * 1.3, my - uy * 1.3)], h=0.13)
            f.skirting([(mx + ux * 1.3, my + uy * 1.3), b], h=0.13)
            f.skirting([a, (mx - ux * 1.3, my - uy * 1.3)], h=0.012, t=0.4, mat="CT_ConcreteDark")
            f.skirting([(mx + ux * 1.3, my + uy * 1.3), b], h=0.012, t=0.4, mat="CT_ConcreteDark")
        else:
            f.skirting([a, b], h=0.13)
            f.skirting([a, b], h=0.012, t=0.4, mat="CT_ConcreteDark")
    # doormat
    box(d, "CT_PlasterGrey", -1.0, 1.0, 8.5, 9.5, 0.0, 0.014, faces="Z")
    # ---- lamps
    for th in (-125.0, -110.0, -98.0, -82.0, -66.0, -54.0):
        x, y = pol(15.0, th)
        B.lamp((x, y, 4.2), lumens=800, colour=WARM, kind="public", radius=7)


def build(B):
    rnd = random.Random(1984)
    m = B.m
    cols = ["CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterRed", "CT_PlasterPink"]
    # ------------------------------------------------------------------ the ring body: 24 bays x 5 storeys
    bays = []
    for k in range(NB):
        th = -90.0 + 15.0 * k
        a = pol(R_BODY, th - HB)
        b = pol(R_TIP, th - HB + DELTA)
        c = pol(R_BODY, th + HB)
        ic, im, ia = pol(R_IN, th + HB), pol(R_IN, th), pol(R_IN, th - HB)
        bays.append(dict(th=th, a=a, b=b, c=c, ic=ic, im=im, ia=ia))
    for k, bay in enumerate(bays):
        a, b, c, ic, im, ia = (bay[x] for x in ("a", "b", "c", "ic", "im", "ia"))
        nxt = bays[(k + 1) % NB]
        poly = [a, b, c, ic, im, ia]
        m.face("CT_Roof", tri_orient_up(poly, ZT))
        m.face("CT_ConcreteDark", [(x, y, Z0) for x, y in tri_orient_up(poly, 0)[::-1]][::1] if False else
               [(x, y, Z0) for x, y, _ in tri_orient_up(poly, 0)][::-1])
        colr = cols[k % 4]
        wl = math.dist(bay["b"], bay["c"])
        for s in range(NS):
            z0, z1 = Z0 + s * STH, Z0 + (s + 1) * STH
            # the return wall a->b (painted) : closes the previous bay's balcony
            m.face(colr, [(a[0], a[1], z0), (b[0], b[1], z0), (b[0], b[1], z1), (a[0], a[1], z1)])
            # the window face b->c
            hl = [(0.10 * wl, 0.60 * wl, z0 + 0.95, z0 + 2.45), (0.68 * wl, 0.86 * wl, z0 + 0.15, z0 + 2.5)]
            F = B.facade(b, c, z0, z1, "CT_Concrete", holes=hl, depth=0.3, back=False)
            fid = "%d-%02d" % (s + 1, k + 1)
            B.flat(fid, storey=s + 1, beds=rnd.choice((1, 2, 2)), door=(*pol(R_IN + 1.6, bay["th"]), z0 + 0.1), entrance="main")
            B.facade_window(F, *hl[0], flat=fid, cols=2, rows=1, inset=0.2)
            B.facade_window(F, *hl[1], flat=fid, cols=1, rows=1, inset=0.2)
            # inner (courtyard) arc: two chords with a corridor window
            for (p0, p1) in ((ic, im), (im, ia)):
                Lh = math.dist(p0, p1)
                F2 = B.facade(p0, p1, z0, z1, "CT_Concrete", holes=[(Lh / 2 - 0.4, Lh / 2 + 0.4, z0 + 1.0, z0 + 2.1)], depth=0.3, back=False)
                B.facade_window(F2, Lh / 2 - 0.4, Lh / 2 + 0.4, z0 + 1.0, z0 + 2.1, kind="stair", inset=0.15)
            # balcony: the triangle b, c, b' with its slab and railing
            tb = nxt["b"]
            zs = z0
            tri = [b, c, tb]
            area = sum(tri[i][0] * tri[(i + 1) % 3][1] - tri[(i + 1) % 3][0] * tri[i][1] for i in range(3))
            T = tri if area > 0 else tri[::-1]
            m.face("CT_Concrete", [(x, y, zs) for x, y in T])
            m.face("CT_ConcreteDark", [(x, y, zs - 0.25) for x, y in T[::-1]])
            # the outer edge b -> b' (facing out): CCW order of the triangle gives the outward direction
            for i in range(3):
                p, q = T[i], T[(i + 1) % 3]
                if (p, q) in ((b, tb), (tb, b)):
                    m.face("CT_Concrete", [(p[0], p[1], zs - 0.25), (q[0], q[1], zs - 0.25), (q[0], q[1], zs), (p[0], p[1], zs)])
            B.railing([b, tb], zs, h=1.05, mat="CT_SteelWhite", post=1.4)
        # roof crown: a low coloured parapet on the window face and the tip
        for (p, q, mat) in ((a, b, colr), (b, c, "CT_ConcreteWhite")):
            L = math.dist(p, q)
            ux, uy = (q[0] - p[0]) / L, (q[1] - p[1]) / L
            nx, ny = uy, -ux                       # outward
            d = 0.25
            quad = [(p[0], p[1]), (q[0], q[1]), (q[0] - nx * d, q[1] - ny * d), (p[0] - nx * d, p[1] - ny * d)]
            m.face(mat, [(p[0], p[1], ZT), (q[0], q[1], ZT), (q[0], q[1], ZT + 0.7), (p[0], p[1], ZT + 0.7)])
            m.face(mat, [(q[0] - nx * d, q[1] - ny * d, ZT), (p[0] - nx * d, p[1] - ny * d, ZT),
                         (p[0] - nx * d, p[1] - ny * d, ZT + 0.7), (q[0] - nx * d, q[1] - ny * d, ZT + 0.7)])
            m.face(mat, [(x, y, ZT + 0.7) for x, y in (quad[0], quad[1], quad[2], quad[3])][::-1] if False else
                   [(quad[0][0], quad[0][1], ZT + 0.7), (quad[1][0], quad[1][1], ZT + 0.7), (quad[2][0], quad[2][1], ZT + 0.7), (quad[3][0], quad[3][1], ZT + 0.7)])
    # ------------------------------------------------------------------ podium
    # solid sectors outside the hall (hall = angles -135 .. -45)
    def sector(t0, t1, nseg):
        angs = [t0 + (t1 - t0) * i / nseg for i in range(nseg + 1)]
        outer = [pol(R_P, t) for t in angs]
        inner = [pol(R_IN, t) for t in angs]
        for i in range(nseg):
            (ax, ay), (bx, by) = outer[i], outer[i + 1]
            m.face("CT_Tile", [(ax, ay, 0), (bx, by, 0), (bx, by, Z0), (ax, ay, Z0)])
            (ax, ay), (bx, by) = inner[i + 1], inner[i]
            m.face("CT_Concrete", [(ax, ay, 0), (bx, by, 0), (bx, by, Z0), (ax, ay, Z0)])
    sector(-45.0, 225.0, 36)
    # ground hall, sector -135..-45
    ang = [-135.0 + 90.0 * i / 7 for i in range(8)]
    outer = [pol(R_P, t) for t in ang]
    inner = [pol(R_IN, t) for t in ang][::-1]
    hall = outer + inner                       # CCW
    nh = len(hall)
    t = 0.4
    holes = {}
    for i in range(7):
        L = math.dist(outer[i], outer[i + 1])
        holes[i] = [(1.1, 3.1, 0.0, 2.6)] if i == 3 else [(0.45, L - 0.45, 0.5, 3.7)]
    for j in range(7):
        i = 8 + j
        L = math.dist(hall[i], hall[(i + 1) % nh])
        holes[i] = [(0.4, L - 0.4, 0.3, 3.8)]
    q = inset_poly(hall, t)
    for i in range(nh):
        p0, p1 = hall[i], hall[(i + 1) % nh]
        hl = holes.get(i, [])
        F = B.facade(p0, p1, 0.0, Z0, "CT_ConcreteWhite", holes=hl, depth=t, back=False)
        L = F.length
        for hi, h in enumerate(hl):
            if i == 3:
                continue
            B.facade_window(F, h[0], h[1], h[2], h[3], kind="public", cols=2 if i < 8 else 1, rows=1, inset=0.15)
        # inner skin
        Fi = Facade(q[(i + 1) % nh], q[i])
        Li = Fi.length
        ih = [(max(0.0, Li - h[1] * Li / L), min(Li, Li - h[0] * Li / L), h[2], h[3]) for h in hl]
        mi = Mesh()
        wall_with_holes(mi, "CT_Interior", Li, 0.0, Z0, ih)
        m.extend(Fi.apply(mi))
    prism(m, "CT_Floor", hall, -0.3, 0.0, top=True, bottom=True, side_mat="CT_ConcreteDark")
    m.face("CT_Interior", [(x, y, Z0 - 0.01) for x, y in q][::-1])
    ex, ey = pol(R_P, -90.0)
    B.entrance("main", (ex, ey, 0.0), 0.0, width=2.4, height=2.6, leaves=2, kind="street")
    # hall furniture
    box(m, "CT_Wood", 3.0, 7.0, 11.2, 12.1, 0.0, 1.1)
    box(B.detail, "CT_Steel", 3.0, 7.0, 11.2, 12.1, 1.1, 1.14)
    B.spot("reception", "counter", (5.0, 12.9, 0.0), 0.0)
    for bx in (-4.0, 4.0):
        box(m, "CT_Wood", bx - 1.0, bx + 1.0, 9.0, 9.5, 0.0, 0.45)
        B.spot("bench_%d" % int(bx), "bench", (bx, 10.2, 0.0), 0.0)
    for i, th in enumerate((-128.0, -118.0, -108.0)):
        x, y = pol(15.0, th)
        box(m, "CT_Wood", x - 0.8, x + 0.8, y - 0.8, y + 0.8, 0.0, 0.76)
        B.spot("dining_%d" % i, "table", (x, y, 0.0), 0.0)
    _furnish_hall(B, hall, t)
    # ------------------------------------------------------------------ struts under some balcony tips
    for k in range(0, NB, 3):
        th = bays[k]["th"] - HB + DELTA
        top = pol(R_TIP - 0.5, th)
        bot = pol(R_P + 0.7, th)
        beam(m, "CT_Concrete", (bot[0], bot[1], 0.0), (top[0], top[1], Z0 - 0.7), 0.9, -0.2, 0.2)
    # ------------------------------------------------------------------ courtyard: lawn, fountain, trees
    prism(m, "CT_Grass", ring(CX, CY, R_IN - 0.05, 40), 0.0, 0.08, top=True, bottom=False)
    prism(m, "CT_Stone", ring(CX, CY, 3.2, 24), 0.08, 0.55, top=True)
    prism(m, "CT_TileWhite", ring(CX, CY, 2.7, 24), 0.55, 0.58, top=True)
    prism(m, "CT_Stone", ring(CX, CY, 0.45, 12), 0.58, 1.9, top=True)
    prism(m, "CT_Stone", ring(CX, CY, 1.1, 16), 1.9, 2.1, top=True)
    for i in range(9):
        th = 360.0 * i / 9 + 10
        x, y = pol(8.0, th)
        box(m, "CT_Wood", x - 0.15, x + 0.15, y - 0.15, y + 0.15, 0.08, 2.6)
        prism(m, "CT_Grass", ring(x, y, 1.7, 8), 2.4, 4.6, top=True, bottom=True)
    # ------------------------------------------------------------------ roof extras: stair cap, courtyard-side rail
    for k in range(0, NB, 4):
        x, y = pol(15.5, bays[k]["th"])
        box(m, "CT_ConcreteWhite", x - 1.2, x + 1.2, y - 1.2, y + 1.2, ZT, ZT + 2.2)
    B.footprint = ring(CX, CY, R_TIP, 48)
    B.height = ZT + 2.2
    B.note("120 flats (5 storeys x 24 bays); hall sector -135..-45 deg enterable; entrance at (0, 8)")
