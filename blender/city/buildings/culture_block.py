"""Culture block («Дом культуры и спорта»): Brazilian brutalism, an original leisure-centre complex.

Spirit of Lina Bo Bardi's SESC Pompeia (1977-86): two raw-concrete towers of different height whose walls are
punched by irregular "cave hole" windows (organic polygons, a lit room card behind each, some with red steel
lattice shutters), joined by three zigzag / diagonal flying bridges; next to them a low long brick hall with a
saw-tooth roof whose clerestory glazing faces the hill. The towers and bridges are our own massing, sizes and
detail, the layout is not a copy.

Parts (site 40 x 30 m, plot 60 x 50): sports tower (11 x 11 m, 30 m, stacked halls, big holes), stair / changing
tower (8 x 8 m, 23 m, small holes climbing like stair flights, blue tile stripe), bridges at z 10.5 / 17-21 / 25-28 m,
a brick hall 38 x 12 m (z 0-8.8 with the saw teeth) with the big painted wooden-looking door in the middle of
the front wall. THE HALL IS ENTERABLE: CT_Floor, CT_Interior walls, ceiling following the teeth; tables, a work
area, a bench and a small café counter. Towers are closed shells (not enterable tonight).

Spots: table x2, work x3, bench x2, counter (cafe). Entrance "main" (double leaf, 2.4 m wide).
Windows: ~100 cave holes (kind public), 13 hall and clerestory windows. No flats.
Ideas for later: enterable towers (stairs inside), a pool deck between the towers, hanging plants on the bridges.
Material requests: none (a painted-timber material for the door surround would be nice: CT_Door is used).
"""
import math
import random

from citykit import box, DOOR_W, DOOR_H, Facade, Mesh, wall_with_holes, triangulate
import geom

INFO = {
    "name_ru": "Дом культуры и спорта", "name_en": "Culture Block",
    "style": "brazilian brutalism", "use": "public",
    "inspired_by": "Lina Bo Bardi's SESC Pompeia (Sao Paulo, 1977-86): cave-hole towers, flying bridges, "
                   "brick workshop shed with saw-tooth roof; massing, positions and details are our own",
}

WALL = 0.6


def _flip_to(pts, want):
    n = geom.cross(geom.sub(pts[1], pts[0]), geom.sub(pts[2], pts[0]))
    if n[0] * want[0] + n[1] * want[1] + n[2] * want[2] < 0:
        return pts[::-1]
    return pts


def hexa(m, mat, v, bot=None):
    """Convex hexahedron: v[0..3] bottom ring, v[4..7] top ring (same order); faces oriented outward."""
    c = tuple(sum(p[i] for p in v) / 8 for i in range(3))
    quads = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    for k, q in enumerate(quads):
        pts = [v[i] for i in q]
        fc = tuple(sum(p[i] for p in pts) / 4 for i in range(3))
        want = tuple(fc[i] - c[i] for i in range(3))
        m.face(bot if (k == 0 and bot) else mat, _flip_to(pts, want))


def beam(m, mat, a, b, w, zlo, zhi, lat=0.0, ext=0.0, bot=None):
    ax, ay, az = a
    bx, by, bz = b
    L = math.hypot(bx - ax, by - ay)
    hx, hy = (bx - ax) / L, (by - ay) / L
    ax, ay = ax - hx * ext, ay - hy * ext
    bx, by = bx + hx * ext, by + hy * ext
    sx, sy = -hy, hx

    def P(px, py, pz, l, vz):
        return (px + sx * (lat + l), py + sy * (lat + l), pz + vz)
    ring_ = [(-w / 2, zlo), (w / 2, zlo), (w / 2, zhi), (-w / 2, zhi)]
    s0 = [P(ax, ay, az, l, vz) for l, vz in ring_]
    s1 = [P(bx, by, bz, l, vz) for l, vz in ring_]
    # build as hexahedron-like: bottom ring = lower two corners of both sections
    verts = [s0[0], s0[1], s1[1], s1[0], s0[3], s0[2], s1[2], s1[3]]
    hexa(m, mat, verts, bot=bot)
    # (the two "side" quads at x-section sit between rings; hexa handles all 6 faces)


def cave_polygon(rnd, uc, zc, rx, rz, n):
    pts = []
    base = rnd.uniform(0, 2 * math.pi)
    angs = sorted((base + 2 * math.pi * (k + rnd.uniform(-0.28, 0.28)) / n) % (2 * math.pi) for k in range(n))
    for a in angs:
        r = rnd.uniform(0.68, 1.0)
        pts.append((uc + math.cos(a) * rx * r, zc + math.sin(a) * rz * r, a))
    return pts


def hole_plate(m, mat, poly, rect, depth, reveal_mat):
    """Fills the rectangle `rect` (u0,u1,z0,z1) of a wall with a polygon hole (star-shaped, CCW poly list of
    (u, z)); adds the reveal of the hole into the wall (facade-local coordinates)."""
    u0, u1, z0, z1 = rect
    cu = sum(p[0] for p in poly) / len(poly)
    cz = sum(p[1] for p in poly) / len(poly)

    def to_rect(u, z):
        du, dz = u - cu, z - cz
        t = min((u1 - cu) / du if du > 1e-9 else (u0 - cu) / du if du < -1e-9 else 1e9,
                (z1 - cz) / dz if dz > 1e-9 else (z0 - cz) / dz if dz < -1e-9 else 1e9)
        return (cu + du * t, cz + dz * t)
    per = []
    for i, (u, z) in enumerate(poly):
        per.append((math.atan2(z - cz, u - cu) % (2 * math.pi), to_rect(u, z), i))
    for (u, z) in ((u1, z1), (u0, z1), (u0, z0), (u1, z0)):
        per.append((math.atan2(z - cz, u - cu) % (2 * math.pi), (u, z), None))
    per.sort(key=lambda t: t[0])
    # polygon vertex for each perimeter point: its own, or the previous one
    last = None
    for t in per:
        if t[2] is not None:
            last = t[2]
    cur = last
    seq = []
    for a, p, i in per:
        if i is not None:
            cur = i
        seq.append((p, cur))
    n = len(seq)
    for k in range(n):
        (pa, ia), (pb, ib) = seq[k], seq[(k + 1) % n]
        va, vb = poly[ia], poly[ib]
        pts = [(va[0], 0, va[1]), (vb[0], 0, vb[1]), (pb[0], 0, pb[1]), (pa[0], 0, pa[1])]
        if ia == ib:
            pts = [pts[0], pts[2], pts[3]]
        pts = _flip_to(pts, (0, 1, 0))
        m.face(mat, pts, [(-p[0], p[2]) for p in pts])
    # reveal: from each polygon edge into the wall
    np_ = len(poly)
    for i in range(np_):
        a, b = poly[i], poly[(i + 1) % np_]
        q = [(a[0], 0, a[1]), (b[0], 0, b[1]), (b[0], -depth, b[1]), (a[0], -depth, a[1])]
        mid = ((a[0] + b[0]) / 2, 0, (a[1] + b[1]) / 2)
        want = (cu - mid[0], 0, cz - mid[2])         # liner faces the hole's centre
        # local faces: normals are judged in the right-handed local frame
        m.face(reveal_mat, _flip_to(q, want))


def cave_wall(B, p0, p1, z0, z1, mat, rnd, count, size, forbid=(), zrange=None, lattice=0.35, stair=False, pad=0.45):
    F = Facade(p0, p1)
    L = F.length
    m = Mesh()
    holes = []
    zlo, zhi = zrange or (z0 + 1.6, z1 - 1.6)
    tries = 0
    while len(holes) < count and tries < 3000:
        tries += 1
        rx = rnd.uniform(size[0], size[1])
        rz = rx * rnd.uniform(0.7, 1.25)
        if stair:
            t = rnd.random()
            uc = 1.2 + rx + t * (L - 2.4 - 2 * rx)
            zc = zlo + (t * 0.9 + rnd.uniform(-0.05, 0.1) + (rnd.randint(0, 1) * 0.0)) * (zhi - zlo)
            zc = min(max(zc, zlo + rz), zhi - rz)
        else:
            uc = rnd.uniform(1.0 + rx, L - 1.0 - rx)
            zc = rnd.uniform(zlo + rz, zhi - rz)
        rect = (uc - rx * 1.03 - 0.1, uc + rx * 1.03 + 0.1, zc - rz * 1.03 - 0.1, zc + rz * 1.03 + 0.1)
        if rect[0] < 0.7 or rect[1] > L - 0.7:
            continue
        if any(not (rect[1] + pad < h[0][0] or rect[0] - pad > h[0][1] or rect[3] + pad < h[0][2] or rect[2] - pad > h[0][3])
               for h in holes):
            continue
        if any(not (rect[1] < f[0] or rect[0] > f[1] or rect[3] < f[2] or rect[2] > f[3]) for f in forbid):
            continue
        poly = cave_polygon(rnd, uc, zc, rx, rz, rnd.choice((6, 7, 8, 9)))
        us = [p[0] for p in poly]
        zs = [p[1] for p in poly]
        rect = (min(us) - 0.12, max(us) + 0.12, min(zs) - 0.12, max(zs) + 0.12)
        holes.append((rect, [(p[0], p[1]) for p in poly]))
    # the wall with the bounding-rectangle holes, then the plates
    wall_with_holes(m, mat, L, z0, z1, [h[0] for h in holes])
    for rect, poly in holes:
        hole_plate(m, mat, poly, rect, WALL, mat)
    # inner face: plain, facing in
    inner = Mesh()
    wall_with_holes(inner, mat, L, z0, z1, [])
    inner.verts = [(u, -WALL, z) for (u, _, z) in inner.verts]
    inner.faces = [(mt, list(reversed(i)), list(reversed(uv))) for mt, i, uv in inner.faces]
    m.extend(inner)
    B.m.extend(F.apply(m))
    for rect, poly in holes:
        u0, u1, h0, h1 = rect
        pts = [F.to_building((u, 0.0, z)) for u, z in ((u0, h0), (u1, h0), (u1, h1), (u0, h1))]
        B.window(pts, kind="public", inset=WALL - 0.06, frame=0)
        if rnd.random() < lattice:
            lm = Mesh()
            t = 0.035
            nu = max(2, int((u1 - u0) / 0.2))
            nz = max(2, int((h1 - h0) / 0.2))
            for k in range(nu + 1):
                u = u0 + (u1 - u0) * k / nu
                box(lm, "CT_SteelRed", u - t, u + t, -0.28, -0.2, h0, h1)
            for k in range(nz + 1):
                z = h0 + (h1 - h0) * k / nz
                box(lm, "CT_SteelRed", u0, u1, -0.28, -0.2, z - t, z + t)
            B.detail.extend(F.apply(lm))
    return F, holes


def tower(B, rect, h, rnd, counts, size, mat="CT_Concrete", forbid=None, stair=False, band_every=None):
    x0, x1, y0, y1 = rect
    forbid = forbid or {}
    # roof and base slabs
    box(B.m, "CT_Concrete", x0, x1, y0, y1, h - 0.5, h, faces="xXyYZ")
    box(B.m, "CT_ConcreteDark", x0, x1, y0, y1, h - 0.5, h, faces="z")
    box(B.m, "CT_ConcreteDark", x0, x1, y0, y1, 0.0, 0.4, faces="xXyYz")
    edges = [((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)), ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0))]
    for i, (p0, p1) in enumerate(edges):
        cave_wall(B, p0, p1, 0.4, h - 0.5, mat, rnd, counts[i], size, forbid=forbid.get(i, ()), stair=stair)
    # projecting slab-edge bands (hall floors) and a parapet
    if band_every:
        z = band_every
        while z < h - 1:
            box(B.m, "CT_ConcreteDark", x0 - 0.25, x1 + 0.25, y0 - 0.25, y1 + 0.25, z - 0.2, z + 0.2)
            z += band_every
    box(B.m, "CT_Concrete", x0 - 0.2, x1 + 0.2, y0 - 0.2, y1 + 0.2, h, h + 0.5, faces="xXyY")
    box(B.m, "CT_Concrete", x0 - 0.2, x1 + 0.2, y0 - 0.2, y1 + 0.2, h, h + 0.5, faces="z") if False else None


def inner_skin(B, x0, x1, y0, y1, t, z0, z1, hole_sets, mat="CT_Interior"):
    """Interior lining of a rectangular room (outer rect x0..x1,y0..y1, wall thickness t): hole_sets[i] are the
    holes (u0, u1, h0, h1) of outer CCW edge i."""
    outer = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    q = [(x0 + t, y0 + t), (x1 - t, y0 + t), (x1 - t, y1 - t), (x0 + t, y1 - t)]
    for i in range(4):
        p0, p1 = outer[i], outer[(i + 1) % 4]
        L = math.hypot(p1[0] - p0[0], p1[1] - p0[1])
        Fi = Facade(q[(i + 1) % 4], q[i])
        Li = Fi.length
        ih = []
        for (u0, u1, h0, h1) in hole_sets[i]:
            a, b = L - t - u1, L - t - u0
            ih.append((max(0.0, a), min(Li, b), h0, h1))
        mi = Mesh()
        wall_with_holes(mi, mat, Li, z0, z1, ih)
        B.m.extend(Fi.apply(mi))


def build(B):
    rnd = random.Random(1977)
    # ------------------------------------------------------------------ towers
    T1 = (-19.0, -8.0, 16.0, 27.0)      # sports tower, 30 m
    T2 = (8.0, 16.0, 18.0, 26.0)        # stairs and changing rooms, 23 m
    H1, H2 = 30.0, 23.0
    bridges = [
        [(-8.0, 18.5, 10.5), (-2.5, 22.5, 10.5), (3.0, 19.0, 10.5), (8.0, 22.8, 10.5)],
        [(-8.0, 24.5, 17.0), (-2.5, 21.0, 18.6), (3.0, 25.0, 20.0), (8.0, 20.5, 21.3)],
        [(-8.0, 20.0, 26.0), (-2.5, 24.5, 26.7), (3.0, 19.8, 27.4), (8.0, 24.5, 28.0)],
    ]
    f1 = [(a[1] - T1[2] - 2.2, a[1] - T1[2] + 2.2, a[2] - 2.3, a[2] + 2.3) for a in [b[0] for b in bridges]]
    f2 = [(T2[3] - b[-1][1] - 2.2, T2[3] - b[-1][1] + 2.2, b[-1][2] - 2.3, b[-1][2] + 2.3) for b in bridges]
    tower(B, T1, H1, rnd, (14, 14, 12, 11), (1.1, 2.1), forbid={1: f1}, band_every=7.5)
    tower(B, T2, H2, rnd, (12, 10, 9, 10), (0.55, 1.0), forbid={3: f2}, stair=True, band_every=None)
    # tile stripe + painted base on tower 2, painted base of tower 1
    box(B.m, "CT_Tile", T2[0] + 1.0, T2[0] + 2.2, T2[2] - 0.12, T2[2] + 0.02, 0.4, H2 - 0.5)
    box(B.m, "CT_PlasterYellow", T1[0] + 0.5, T1[1] - 0.5, T1[2] - 0.12, T1[2] + 0.02, 0.4, 3.0)
    # roof extras
    box(B.m, "CT_ConcreteDark", -17.0, -13.0, 20.0, 24.0, H1 + 0.5, H1 + 2.4)
    box(B.m, "CT_Concrete", 10.0, 14.0, 20.0, 24.0, H2 + 0.5, H2 + 2.0)
    # ------------------------------------------------------------------ bridges
    for bi, pts in enumerate(bridges):
        w = 2.2
        for k in range(len(pts) - 1):
            a, b = pts[k], pts[k + 1]
            ea = 0.2 if k == 0 else 0.3
            eb = 0.2 if k == len(pts) - 2 else 0.3
            # extend manually so the end overlaps differ
            L = math.hypot(b[0] - a[0], b[1] - a[1])
            hx, hy = (b[0] - a[0]) / L, (b[1] - a[1]) / L
            sl = (b[2] - a[2]) / L
            a2 = (a[0] - hx * ea, a[1] - hy * ea, a[2] - sl * ea)
            b2 = (b[0] + hx * eb, b[1] + hy * eb, b[2] + sl * eb)
            beam(B.m, "CT_Concrete", a2, b2, w, -0.5, 0.0, bot=None)
            beam(B.m, "CT_Concrete", a2, b2, 0.22, 0.0, 1.05, lat=w / 2 - 0.11)
            beam(B.m, "CT_Concrete", a2, b2, 0.22, 0.0, 1.05, lat=-(w / 2 - 0.11))
    # a pier under the first bridge
    px, py, pz = bridges[0][1]
    box(B.m, "CT_Concrete", px - 0.5, px + 0.5, py - 0.5, py + 0.5, 0.0, pz - 0.5)
    # ------------------------------------------------------------------ the brick hall (enterable)
    hx0, hx1, hy0, hy1 = -19.0, 19.0, 1.0, 13.0
    WT = 6.0
    t = 0.4
    fl = box
    box(B.m, "CT_Floor", hx0, hx1, hy0, hy1, -0.3, 0.0, faces="Z")
    box(B.m, "CT_ConcreteDark", hx0, hx1, hy0, hy1, -0.3, 0.0, faces="xXyYz")
    hs = []
    # holes per outer CCW edge: front, right end, back, left end   (u from the edge start)
    front = [(2.5, 6.5, 0.9, 3.8), (7.5, 11.5, 0.9, 3.8), (19 - DOOR_W - 0.2, 19 + DOOR_W + 0.2, 0.0, 3.2),
             (26.5, 30.5, 0.9, 3.8), (31.5, 35.5, 0.9, 3.8)]
    front[2] = (19 - 1.2, 19 + 1.2, 0.0, 3.2)
    right = [(3.5, 8.5, 1.0, 3.8)]
    back = [(3.0, 8.0, 0.9, 3.6), (12.0, 17.0, 0.9, 3.6), (21.0, 26.0, 0.9, 3.6), (30.0, 35.0, 0.9, 3.6)]
    left = [(3.5, 8.5, 1.0, 3.8)]
    sets = [front, right, back, left]
    pts = [(hx0, hy0), (hx1, hy0), (hx1, hy1), (hx0, hy1)]
    for i in range(4):
        p0, p1 = pts[i], pts[(i + 1) % 4]
        F = B.facade(p0, p1, 0.0, WT, "CT_Brick", holes=sets[i], depth=t, back=False)
        for hi, h in enumerate(sets[i]):
            if i == 0 and hi == 2:
                continue
            B.facade_window(F, h[0], h[1], h[2], h[3], kind="public", cols=3, rows=2, inset=0.2)
    inner_skin(B, hx0, hx1, hy0, hy1, t, 0.0, WT, sets)
    B.entrance("main", (0.0, hy0, 0.0), 0.0, width=2.4, height=3.2, leaves=2, kind="street")
    # saw-tooth roof: 3 teeth along y, vertical clerestory faces at y = 5, 9, 13 facing +y (to the hill)
    TL, TH, TT = 6.4, 8.8, 0.4
    for k in range(3):
        ya, yb = hy0 + 4 * k, hy0 + 4 * (k + 1)
        # sloped slab top / soffit / ends
        B.m.face("CT_Roof", [(hx0, ya, TL), (hx1, ya, TL), (hx1, yb, TH), (hx0, yb, TH)][::-1] if False else
                 [(hx0, ya, TL), (hx1, ya, TL), (hx1, yb, TH), (hx0, yb, TH)][::-1])
        B.m.face("CT_Interior", [(hx0, ya, TL - TT), (hx0, yb, TH - TT), (hx1, yb, TH - TT), (hx1, ya, TL - TT)][::-1][::-1])
        for xe, s in ((hx0, -1), (hx1, 1)):
            q = [(xe, ya, TL - TT), (xe, yb, TH - TT), (xe, yb, TH), (xe, ya, TL)]
            B.m.face("CT_Concrete", q if s > 0 else q[::-1])
            # brick gable below the slab outer face
            tri = [(xe, ya, WT), (xe, yb, WT), (xe, yb, TH - TT), (xe, ya, TL - TT)]
            ti = [(xe + (-s) * t, ya, WT), (xe + (-s) * t, yb, WT), (xe + (-s) * t, yb, TH - TT), (xe + (-s) * t, ya, TL - TT)]
            if s > 0:
                B.m.face("CT_Brick", [tri[1], tri[0], tri[3], tri[2]][::-1][::-1])
                B.m.face("CT_Interior", ti)
            else:
                B.m.face("CT_Brick", tri)
                B.m.face("CT_Interior", ti[::-1])
        # vertical clerestory wall at yb (outside faces +y)
        p0, p1 = (hx1, yb), (hx0, yb)
        holes = [(1.0 + 3.4 * i, 1.0 + 3.4 * i + 2.6, 6.9, 8.1) for i in range(int((38 - 2) // 3.4))]
        F = B.facade(p0, p1, WT, TH, "CT_Concrete", holes=holes, depth=0.35)
        for h in holes:
            B.facade_window(F, h[0], h[1], h[2], h[3], kind="public", inset=0.15)
    # interior of the end faces' slab edge and the far side: the first tooth's low edge meets the front wall top
    # furniture and spots
    for tx in (-12.0, -6.0):
        box(B.m, "CT_Wood", tx - 1.6, tx + 1.6, 6.5, 7.7, 0.0, 0.78)
        B.spot("table_%d" % int(tx), "table", (tx, 7.1, 0.0), 0.0)
    for wx in (4.0, 9.0, 14.0):
        box(B.m, "CT_Wood", wx - 1.4, wx + 1.4, 4.2, 5.2, 0.0, 0.9)
        box(B.m, "CT_Steel", wx - 1.4, wx + 1.4, 4.2, 5.2, 0.9, 0.94)
        B.spot("work_%d" % int(wx), "work", (wx, 6.0, 0.0), 0.0)
    for bx in (-16.0, 16.0):
        box(B.m, "CT_Wood", bx - 1.0, bx + 1.0, 1.6, 2.1, 0.0, 0.45)
        B.spot("bench_%d" % int(bx), "bench", (bx, 2.9, 0.0), 0.0)
    # cafe counter in the back-right corner
    box(B.m, "CT_Wood", 10.0, 17.0, 11.2, 12.1, 0.0, 1.1)
    box(B.detail, "CT_Steel", 10.0, 17.0, 11.2, 12.1, 1.1, 1.14)
    B.spot("cafe_counter", "counter", (13.5, 10.4, 0.0), 180.0)
    # big painted wooden door surround on the front wall (the door leaves are the game's actor)
    for (a, b, z0, z1, mat) in ((-3.4, -1.2, 0.0, 4.9, "CT_Door"), (1.2, 3.4, 0.0, 4.9, "CT_Door"),
                                (-3.4, 3.4, 3.2, 4.9, "CT_Door"), (-3.6, 3.6, 4.9, 5.25, "CT_SteelRed")):
        box(B.m, mat, a, b, hy0 - 0.2, hy0 + 0.05, z0, z1, faces="xXyZz" if z0 > 0 else "xXyZ")
    # vertical colour strip on the brick between door and windows
    B.footprint = [(-19.0, 0.8), (19.0, 0.8), (19.0, 27.0), (-19.0, 27.0)]
    B.height = H1 + 2.4
    B.note("hall 38x12 m enterable; %d cave windows" % sum(1 for w in B.windows if w["kind"] == "public"))
