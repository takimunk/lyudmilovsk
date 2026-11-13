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
import os
import random

from citykit import box, prism, ring, DOOR_W, DOOR_H, Facade, Mesh, wall_with_holes, triangulate
import geom
import furn_int3 as fu
import interior_kit as kit

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


def _line(d, mat, pts, w=0.07, z=0.012, closed=False):
    """A painted line (court markings): a flat strip along the polyline, just above the floor."""
    pts = list(pts) + ([pts[0]] if closed else [])
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        L = math.hypot(bx - ax, by - ay)
        if L < 1e-6:
            continue
        nx, ny = -(by - ay) / L * w / 2, (bx - ax) / L * w / 2
        d.face(mat, [(ax - nx, ay - ny, z), (bx - nx, by - ny, z), (bx + nx, by + ny, z), (ax + nx, ay + ny, z)])


def _arc(cx, cy, r, a0, a1, n=16):
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * k / n)), cy + r * math.sin(math.radians(a0 + (a1 - a0) * k / n)))
            for k in range(n + 1)]


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


def _furnish_hall(B, hx0, hx1, hy0, hy1):
    """Round 3: the brick hall (inner x -18.6..18.6, y 1.4..12.6) as the sports hall: basketball at the west end, gym
    in the middle, art corner and weights at the east, buffet in the east back corner, reception by the door."""
    d = B.detail
    T = B.thing
    fy0, fy1 = hy0 + 0.4, hy1 - 0.4              # inner faces of the front and back walls
    fx0, fx1 = hx0 + 0.4, hx1 - 0.4              # of the end walls
    sp = B.spot

    # ---- floor: court markings (basketball under the back-wall hoop), a rubber mat for the weights
    kx = -9.0
    box(d, "CT_PlasterRed", kx - 1.8, kx + 1.8, 7.0, fy1, 0.010, 0.014, faces="Z")                  # the key
    _line(d, "CT_TileWhite", [(kx - 1.8, fy1), (kx - 1.8, 7.0), (kx + 1.8, 7.0), (kx + 1.8, fy1)], 0.07, 0.016)
    _line(d, "CT_TileWhite", _arc(kx, 7.0, 1.8, 180, 360), 0.07, 0.016)
    _line(d, "CT_TileWhite", [(kx - 5.4, fy1)] + _arc(kx, 11.0, 5.4, 180, 360) + [(kx + 5.4, fy1)], 0.07, 0.016)
    box(d, "CT_Rubber", 8.0, 15.6, 9.1, fy1 - 0.1, 0.010, 0.022, faces="xXyYZ")

    # ---- reception by the door (east of it) and a waiting row (west of it)
    r = kit.reception_desk(B, 4.1, 3.9, yaw=270, w=2.2)
    kit.telephone(B, 4.0, 3.5, 1.1, 270)
    kit.cash_register(B, 4.0, 4.4, 1.1, 270)
    kit.books_stack(B, 3.95, 3.0, 1.1, 0, n=3, seed=3)
    kit.chair(B, 5.0, 4.0, yaw=90, kind="tube", mat="CT_PlasterBlue")
    T("reception", "counter", (4.1, 3.9, 1.1), 270, role="cashier", service="reception")
    sp("reception_spot", "counter", (3.0, 3.9, 0.0), 270)
    kit.sign_plate(B, 5.0, fy0, yaw=180, name="sign_gym", w=1.5, zc=3.9)
    wr = kit.waiting_row(B, -5.4, 2.0, yaw=180, n=4)
    T("waiting_row", "seat", (-5.4, 2.0, 0.45), 180, height=0.45)
    kit.notice_board(B, -5.4, fy0, yaw=180, w=1.5, zc=1.9, seed=2)
    T("bench_-16", "board", (-5.4, fy0 + 0.1, 1.9), 180, topic="club")
    kit.row(kit.poster, B, -7.0, fy0, -3.9, fy0, 2, yaw=180, name="poster_cons", w=0.55, zc=3.0, seed=4)
    kit.wall_clock(B, 2.4, fy0, yaw=180, zc=3.6, r=0.3)
    kit.fire_extinguisher(B, -3.7, fy0, yaw=180)
    kit.plant(B, -3.0, 2.6, kind="palm", pot="CT_PlasterYellow", h=1.8)
    kit.plant(B, 3.3, 2.1, kind="ficus", pot="CT_PlasterRed", h=1.6)
    kit.rug(B, 0.0, 2.9, yaw=0, w=2.4, d=1.5, name="rug", seed=1)
    for k, nm in enumerate(("painting_sea_calm", "painting_dunes", "painting_lighthouse")):
        x = 4.5 + 1.15 * k
        kit.picture(B, x, fy0, yaw=180, name=nm, w=0.8, zc=2.1)
    T("exhibit_0", "board", (4.5, fy0 + 0.2, 2.1), 180, topic="exhibit")
    T("exhibit_1", "board", (5.65, fy0 + 0.2, 2.1), 180, topic="painting")
    T("exhibit_2", "board", (6.8, fy0 + 0.2, 2.1), 180, topic="exhibit")
    sp("exhibit_0", "view", (4.5, 2.4, 0.0), 180)
    sp("exhibit_1", "view", (5.65, 2.4, 0.0), 180)
    sp("exhibit_2", "view", (6.8, 2.4, 0.0), 180)

    # ---- west end: basketball
    kit.basketball_hoop(B, kx, fy1, yaw=0)
    kit.basketball_hoop(B, fx0, 11.0, yaw=90)
    T("hoop_back", "sport", (kx, fy1 - 0.8, 2.3), 0, what="basketball")
    T("hoop_west", "sport", (fx0 + 0.8, 11.0, 2.3), 90, what="basketball")
    kit.banner(B, -17.4, fy1, yaw=0, name="poster_sun_2", w=1.6, zc=3.4)
    kit.poster(B, kx - 2.9, fy1, yaw=0, name="poster_health", w=0.7, zc=4.2)
    kit.poster(B, kx + 2.9, fy1, yaw=0, name="poster_safety", w=0.7, zc=4.2)
    kit.lockers(B, fx0, 3.0, yaw=90, n=7, mat="CT_PlasterBlue")
    kit.bench(B, fx0 + 1.0, 3.0, yaw=270, w=2.4, kind="gym")
    sp("bench_-16", "bench", (fx0 + 1.0, 3.0, 0.0), 270)
    T("changing_bench_w", "seat", (fx0 + 1.0, 3.0, 0.45), 270, height=0.45)
    T("lockers_west", "board", (fx0 + 0.3, 3.0, 1.3), 90, topic="rules")
    kit.mats(B, fx0 + 0.6, 6.9, yaw=0, w=1.0, d=2.0, n=4)
    for k, bx in enumerate((-14.4, -10.4)):
        kit.bench(B, bx, fy0 + 0.35, yaw=180, w=2.0, kind="gym")
        T("sideline_bench_%d" % (k + 1), "seat", (bx, fy0 + 0.35, 0.45), 180, height=0.45)
    kit.plant(B, -17.7, 12.0, kind="monstera", pot="CT_PlasterGreen", h=1.6)
    kit.fire_extinguisher(B, -2.4, fy1, yaw=0)
    kit.sack(B, -16.6, 11.6, yaw=20)
    kit.sack(B, -16.0, 12.0, yaw=75)
    kit.sack(B, -16.9, 10.9, yaw=140)
    kit.crate(B, 17.2, 5.7, yaw=10, fill="CT_PlasterRed", seed=1)

    # ---- middle: gymnastics (wall bars, vaulting horse, landing mats, ping-pong)
    for k, wx in enumerate((-1.2, 0.0, 1.2)):
        kit.wall_bars(B, wx, fy1, yaw=0, w=0.9, h=2.6)
        T("wall_bars_%d" % (k + 1), "sport", (wx, fy1 - 0.2, 1.4), 0, what="wall_bars")
    kit.mats(B, -0.6, 11.3, yaw=0, w=2.0, d=1.4, n=2, mat="CT_PlasterRed")
    kit.mats(B, 1.5, 11.3, yaw=0, w=1.8, d=1.4, n=2)
    hz = kit.vaulting_horse(B, 0.4, 7.3, yaw=0)
    T("vault_horse", "sport", (0.4, 7.3, 1.12), 0, what="vaulting_horse")
    box(d, "CT_Wood", -0.2, 0.9, 5.9, 6.7, 0.0, 0.1, faces="xXyYZ")
    box(d, "CT_PlasterRed", -0.15, 0.85, 5.95, 6.65, 0.1, 0.13, faces="xXyYZ")
    kit.mats(B, 0.4, 9.0, yaw=0, w=2.6, d=1.8, n=2, mat="CT_PlasterBlue")
    T("landing_mats", "sport", (0.4, 9.0, 0.2), 0, what="mats")
    pp = kit.pingpong_table(B, 5.8, 8.3, yaw=0)
    T("pingpong_a", "play", (3.3, 8.3, 0.9), 90, what="table_tennis")
    T("pingpong_b", "play", (8.3, 8.3, 0.9), 270, what="table_tennis")
    sp("pingpong_a", "stand", (3.0, 8.3, 0.0), 90)
    sp("pingpong_b", "stand", (8.6, 8.3, 0.0), 270)
    kit.wall_clock(B, 0.0, fy1, yaw=0, zc=3.6, r=0.3)
    kit.banner(B, 9.0, fy1, yaw=0, name="poster_sun_1", w=1.5, zc=3.6)

    # ---- east: art corner (easels around a still life, two potter's wheels, a work table), shelves of materials
    st = kit.table(B, 11.6, 5.6, w=1.3, d=0.8, h=0.75, mat=kit.WOODL)
    kit.vase(B, 11.4, 5.6, st["top"], 0, seed=2)
    kit.bottles(B, 11.9, 5.7, st["top"], 20, n=2, seed=3)
    kit.books_stack(B, 11.8, 5.45, st["top"], 10, n=2, seed=5)
    for i, (ex, ey, eyaw) in enumerate(((11.6, 3.9, 0), (9.0, 5.6, 270), (14.2, 5.6, 90))):
        e = kit.easel(B, ex, ey, yaw=eyaw, seed=i + 7)
    T("table_-12", "craft", (11.6, 3.9, 1.1), 0, what="easel")
    T("easel_2", "craft", (9.0, 5.6, 1.1), 270, what="easel")
    T("easel_3", "craft", (14.2, 5.6, 1.1), 90, what="easel")
    sp("table_-12", "work", (11.6, 3.0, 0.0), 180)
    for k, wx in enumerate((15.3, 16.7)):
        kit.potters_wheel(B, wx, 3.2, yaw=0)
        kit.stool(B, wx, 3.85, yaw=180, h=0.42)
    T("table_-6", "craft", (15.3, 3.2, 0.55), 0, what="potters_wheel")
    T("wheel_2", "craft", (16.7, 3.2, 0.55), 0, what="potters_wheel")
    sp("table_-6", "work", (15.3, 3.9, 0.0), 0)
    wt = kit.table(B, 8.6, 2.3, yaw=0, w=1.8, d=0.8, h=0.75, mat=kit.WOODL)
    kit.paint_jars(B, 8.2, 2.3, wt["top"], 0, n=5, seed=4)
    kit.tools(B, 9.1, 2.3, wt["top"], 15)
    kit.stool(B, 8.2, 3.1, yaw=0)
    kit.stool(B, 9.0, 3.1, yaw=0)
    sp("work_9", "work", (9.0, 3.1, 0.0), 0)
    sp("work_4", "work", (8.2, 3.1, 0.0), 0)
    sp("work_14", "work", (16.7, 3.9, 0.0), 0)
    T("work_table", "craft", (8.6, 2.3, 0.9), 0, what="workbench")
    kit.bookshelf(B, 12.0, fy0, yaw=180, w=0.9, h=2.0, topic="art", seed=3)
    T("art_shelf", "shelf", (12.0, fy0 + 0.3, 1.3), 180, topic="art")
    kit.shop_shelf(B, fx1, 3.0, yaw=270, w=1.8, h=1.9, goods="jars", seed=2)
    T("clay_shelf", "board", (fx1 - 0.3, 3.0, 1.3), 270, topic="store")
    kit.bench(B, fx1 - 0.35, 6.9, yaw=270, w=2.4, kind="park")
    sp("bench_16", "bench", (fx1 - 0.5, 6.9, 0.0), 270)
    T("bench_east", "seat", (fx1 - 0.35, 6.9, 0.45), 270, height=0.45)
    kit.plant(B, 17.8, 2.1, kind="fern", pot="CT_PlasterYellow", h=1.2)
    kit.plant(B, 7.3, 2.0, kind="cactus", pot="CT_PlasterRed", h=1.0)

    # ---- east back: weights corner
    for k, rx in enumerate((9.4, 11.6)):
        kit.barbell_rack(B, rx, fy1 - 0.4, yaw=0)
        T("barbell_rack_%d" % (k + 1), "sport", (rx, fy1 - 0.45, 1.06), 0, what="barbell")
    for k, (bx_, by_) in enumerate(((10.3, 10.6), (13.3, 10.6))):
        kit.bench_press(B, bx_, by_, yaw=0)
        T("bench_press_%d" % (k + 1), "sport", (bx_, by_, 0.9), 0, what="barbell")
    for k, bx_ in enumerate((14.9, 15.1)):
        kit.exercise_bike(B, bx_ + 0.0 * k, 11.8 - 1.0 * k, yaw=0)
        T("bike_%d" % (k + 1), "sport", (bx_, 11.8 - 1.0 * k, 0.9), 0, what="exercise_bike")
    kit.mirror(B, 8.2, fy1, yaw=0, zc=1.9, w=1.4, h=1.3)

    # ---- east back corner: the buffet (a counter with the cashier behind, stools, little round tables)
    bc = kit.bar_counter(B, 16.6, 11.0, yaw=270, w=2.6, d=0.65, taps=0, seed=2)
    kit.cup(B, 16.5, 10.4, 1.05, 0)
    kit.teapot(B, 16.6, 11.4, 1.05, 0)
    kit.glass(B, 16.5, 11.9, 1.05, 0)
    kit.sign_plate(B, fx1, 11.0, yaw=270, name="sign_buffet", w=1.4, zc=2.7)
    kit.row(kit.bar_stool, B, 15.7, 10.3, 15.7, 11.7, 2, yaw=90)
    T("cafe_counter", "counter", (16.6, 11.0, 1.05), 270, role="cashier", service="bar", shop="sport_cafe")
    sp("cafe_counter", "counter", (15.3, 11.0, 0.0), 90)
    for i, (tx, ty) in enumerate(((13.2, 8.2), (15.2, 8.3), (17.0, 8.6))):
        kit.table_round(B, tx, ty, r=0.42, mat=kit.WOODL)
        if i == 1:                                       # a game of draughts, and dominoes at the next table
            _checkers(B, tx, ty, 0.75, 0.0)
            T("cafe_checkers", "play", (tx, ty, 0.78), 0, what="checkers")
        elif i == 2:
            _dominoes(B, tx - 0.15, ty - 0.12, 0.75, 0.0)
            T("cafe_dominoes", "play", (tx, ty, 0.78), 0, what="dominoes")
        else:
            kit.cup(B, tx, ty, 0.75, 0)
        kit.chair(B, tx - 0.6, ty, yaw=90)
        kit.chair(B, tx + 0.6, ty, yaw=270)
        T("cafe_table_%d" % (i + 1), "seat", (tx - 0.6, ty, 0.45), 90, height=0.45)
        sp("cafe_table_%d" % (i + 1), "table", (tx - 0.6, ty, 0.0), 90.0)
    # ---- Round 4: deeper pass. changing screen, sauna and shower cabin (west front), the kiln, the coach's corner,
    # the timetable, the studio's wall
    cx0, cx1, cy0, cy1, cp, WH, ct = -16.0, -13.0, 3.6, 5.9, -14.3, 2.3, 0.1
    mm = B.m
    for a, b in ((cx0, -15.5), (-14.6, -14.05), (-13.25, cx1)):
        box(mm, "CT_Wood", a, b, cy0, cy0 + ct, 0.0, WH)
    for a, b in ((-15.5, -14.6), (-14.05, -13.25)):
        box(mm, "CT_Wood", a, b, cy0, cy0 + ct, 2.0, WH)
    box(mm, "CT_Wood", cx0, cx0 + ct, cy0, cy1, 0.0, WH)
    box(mm, "CT_Wood", cx1 - ct, cx1, cy0, cy1, 0.0, WH)
    box(mm, "CT_Wood", cx0, cx1, cy1 - ct, cy1, 0.0, WH)
    box(mm, "CT_Wood", cp - ct / 2, cp + ct / 2, cy0 + ct, cy1 - ct, 0.0, WH)
    box(mm, "CT_WoodLight", cx0 - 0.05, cx1 + 0.05, cy0 - 0.05, cy1 + 0.05, WH, WH + 0.08)
    # sauna: duckboards, stove with stones and a flue, a two-level bench, a bucket of water
    box(d, "CT_WoodLight", cx0 + ct, cp - ct / 2, cy0 + ct, cy1 - ct, 0.0, 0.03, faces="Z")
    box(mm, "CT_Steel", -15.88, -15.52, 3.77, 4.13, 0.0, 0.68)
    box(d, "CT_Stone", -15.86, -15.54, 3.79, 4.11, 0.68, 0.8)
    box(d, "CT_Steel", -15.74, -15.66, 3.91, 3.99, 0.8, WH)
    box(mm, "CT_WoodLight", -15.85, -14.37, 5.4, 5.78, 0.0, 0.86)
    box(d, "CT_Wood", -15.88, -14.34, 5.38, 5.8, 0.86, 0.9, faces="xXyYZ")
    kit.bench(B, -15.1, 5.17, yaw=0, w=1.5, kind="sauna")
    box(d, "CT_Wood", -14.7, -14.46, 4.5, 4.7, 0.0, 0.26)
    box(d, "CT_Steel", -14.7, -14.46, 4.5, 4.7, 0.26, 0.27, faces="Z")
    # shower: white tile on the walls and floor, a head on a pipe, a drain
    box(d, "CT_TileWhite", cp + ct / 2 + 0.02, cx1 - ct, cy0 + ct, cy1 - ct, 0.0, 0.02, faces="Z")
    box(d, "CT_TileWhite", cp + ct / 2, cx1 - ct, cy1 - ct - 0.02, cy1 - ct, 0.0, 2.1, faces="xXy")
    box(d, "CT_TileWhite", cx1 - ct - 0.02, cx1 - ct, cy0 + ct, cy1 - ct - 0.02, 0.0, 2.1, faces="xYy")
    box(d, "CT_TileWhite", cp + ct / 2, cp + ct / 2 + 0.02, cy0 + ct, cy1 - ct, 0.0, 2.1, faces="XyY")
    box(d, "CT_Aluminium", -13.72, -13.68, 5.72, 5.76, 1.1, 2.04)
    box(d, "CT_Aluminium", -13.8, -13.6, 5.52, 5.76, 2.0, 2.06)
    box(d, "CT_Aluminium", -13.76, -13.64, 5.66, 5.76, 1.1, 1.16)
    box(d, "CT_ConcreteDark", -13.8, -13.6, 4.9, 5.1, 0.02, 0.03, faces="Z")
    B.lamp((cp, 4.7, 2.15), lumens=380, colour="#FFC890", kind="public", radius=3.5)
    # outside of the cabin: results board (court side), pool notice and a lifebuoy (aisle side)
    kit.notice_board(B, cx1, 4.75, yaw=90, w=1.3, zc=1.6, seed=5)
    kit.notice_board(B, cx0, 5.15, yaw=270, w=0.7, zc=1.5, seed=9)
    kit.lifebuoy(B, cx0, 4.15, yaw=270, zc=1.55)
    kit.mosaic_panel(B, -14.5, cy1, yaw=180, name="tile_4", w=1.1, h=0.9, zc=1.5, seed=3)
    kit.sign_plate(B, -15.05, cy0, yaw=0, name="sign_quiet", w=0.7, zc=2.15)
    _switch(B, -14.33, cy0, 0.0, 1.3, "light_switch_cabin", 3)          # between the two cabin doors
    _switch(B, 1.75, fy0, 180.0, 1.3, "light_switch_hall", "hall")       # inside the front door
    # changing screen by the lockers
    kit.screen_divider(B, -17.2, 5.0, yaw=90)
    # kiln in the art corner (east wall), its drying shelf is the clay shelf
    kit.kiln(B, 17.95, 4.5, yaw=270)
    # coach's corner between the sideline benches: a table, a tactics board, a whistle on a cord
    ct_ = kit.table(B, -12.0, 2.0, yaw=0, w=0.9, d=0.5, h=0.75, mat=kit.WOODL)
    kit.books_stack(B, -12.2, 2.0, ct_["top"], 20, n=2, seed=8)
    kit.cup(B, -11.8, 2.05, ct_["top"], 0)
    kit.picture(B, -12.0, fy0, yaw=180, name="board_1", w=0.8, zc=1.9, frame=kit.WOODL)
    T("coach_table", "counter", (-12.0, 2.0, 0.9), 180, role="coach", service="coach")
    T("tactics_board", "board", (-12.0, fy0 + 0.1, 1.9), 180, topic="tactics")
    sp("coach_table", "stand", (-12.0, 2.8, 0.0), 0)
    # the timetable by the door
    kit.notice_board(B, -2.4, fy0, yaw=180, w=1.1, zc=1.8, seed=7)
    T("training_board", "board", (-2.4, fy0 + 0.1, 1.8), 180, topic="timetable")
    sp("schedule_view", "view", (-2.4, 2.4, 0.0), 180)
    # the studio's wall
    kit.picture(B, 17.2, fy0, yaw=180, name="painting_still_life", w=0.7, zc=2.2)
    kit.picture(B, 18.05, fy0, yaw=180, name="painting_sea_storm", w=0.7, zc=2.2)
    # things
    T("sauna_bench", "seat", (-15.1, 5.17, 0.45), 0, height=0.45)
    T("sauna_stove", "machine", (-15.7, 3.95, 0.6), 90, what="sauna_stove")
    T("shower_stall", "machine", (-13.7, 5.7, 1.4), 0, what="shower")
    T("cabin_results", "board", (cx1 + 0.05, 4.75, 1.6), 90, topic="results")
    T("pool_sign", "board", (cx0 - 0.05, 5.15, 1.5), 270, topic="pool")
    T("changing_screen", "door", (-17.2, 5.0, 0.9), 90, to="staff")
    T("kiln", "craft", (17.55, 4.5, 0.8), 270, what="kiln")
    T("court_game", "sport", (-9.0, 6.6, 0.9), 180, what="basketball")
    sp("sauna_bench", "bench", (-15.1, 4.6, 0.0), 0)
    sp("kiln_spot", "work", (17.0, 4.5, 0.0), 90)
    sp("results_view", "view", (-12.2, 4.75, 0.0), 270)
    sp("court_free", "stand", (-9.0, 6.0, 0.0), 180)
    # ---- Round 5: third layer. lockers area, gym corner, court gear, studio, buffet
    P = kit._P

    def ball(x, y, z):
        q = P(B, x, y, z, 0, "ball")
        q.kind = "prop"
        q.lathe("CT_PlasterYellow", 0, 0, [(0.0, 0.0), (0.06, 0.015), (0.105, 0.07), (0.12, 0.12), (0.105, 0.17), (0.06, 0.225), (0.0, 0.24)], 8)
        q.done()

    def bowl(x, y, z, mat, r=0.09):
        q = P(B, x, y, z, 0, "bowl")
        q.kind = "prop"
        q.lathe(mat, 0, 0, [(r * 0.5, 0.0), (r, 0.05), (r * 1.1, 0.09)], 8, top=False, bottom=True)
        q.done()

    # lockers: a mirror, hooks with a jacket and a scarf, a forgotten towel and trainers
    kit.mirror(B, -18.1, fy0, yaw=180, zc=1.5, w=0.6, h=1.1)
    for k, hx in enumerate((-17.6, -17.3, -17.0, -16.7)):
        box(d, "CT_Steel", hx - 0.02, hx + 0.02, fy0, fy0 + 0.07, 1.68, 1.72)
    box(d, "CT_PlasterGreen", -17.34, -17.26, fy0 + 0.04, fy0 + 0.1, 0.95, 1.7)
    box(d, "CT_PlasterGreen", -17.42, -17.18, fy0 + 0.05, fy0 + 0.12, 0.95, 1.4)
    box(d, "CT_PlasterRed", -17.04, -16.96, fy0 + 0.04, fy0 + 0.08, 1.1, 1.7)
    box(d, "CT_TileWhite", -17.78, -17.42, 3.5, 3.8, 0.45, 0.475)
    box(d, "CT_SteelRed", -17.78, -17.42, 3.62, 3.68, 0.475, 0.478, faces="Z")
    for sy_, yy in ((1, 2.35), (-1, 2.6)):
        box(d, "CT_Rubber", -17.12, -16.88, yy - 0.05, yy + 0.05, 0.0, 0.03)
        box(d, "CT_PlasterBlue", -17.1, -16.9, yy - 0.04, yy + 0.04, 0.03, 0.1)
        box(d, "CT_TileWhite", -17.1, -16.9, yy - 0.04, yy - 0.0, 0.1, 0.12)

    # gym corner: chalk bowl on a stand, a plate tree, a wall chart of exercises
    q = P(B, 12.75, 11.65, 0, 0, "chalk_stand")
    q.cyl("CT_Steel", 0, 0, 0.0, 0.9, 0.025, 6)
    q.lathe("CT_TileWhite", 0, 0, [(0.05, 0.9), (0.15, 0.95), (0.17, 1.02)], 8, top=False, bottom=True)
    q.done()
    q = P(B, 13.6, 12.05, 0, 0, "plate_tree")
    q.cyl("CT_Steel", 0, 0, 0.0, 1.15, 0.025, 6)
    q.cyl("CT_Steel", 0, 0, 0.0, 0.03, 0.2, 8)
    for k, (zz, rr) in enumerate(((0.05, 0.2), (0.11, 0.2), (0.17, 0.17), (0.23, 0.17), (0.29, 0.14), (0.35, 0.14), (0.41, 0.11))):
        q.cyl("CT_ConcreteDark" if k % 2 == 0 else "CT_Steel", 0, 0, zz, zz + 0.05, rr, 10, bottom=True)
    q.done()
    kit.poster(B, 10.35, fy1, yaw=0, name="chart_anatomy", w=0.9, zc=2.1)

    # court: a flip scoreboard high on the back wall, a ball cart, a referee's chair
    box(d, "CT_ConcreteDark", -5.3, -3.7, fy1 - 0.12, fy1, 4.3, 5.1)
    for k, cx_ in enumerate((-5.05, -4.75, -4.25, -3.95)):
        box(d, "CT_TileWhite", cx_ - 0.1, cx_ + 0.1, fy1 - 0.135, fy1 - 0.12, 4.45, 4.95, faces="y")
        box(d, "CT_SteelRed", cx_ - 0.1, cx_ + 0.1, fy1 - 0.14, fy1 - 0.135, 4.69, 4.71, faces="y")
    box(d, "CT_SteelRed", -4.54, -4.46, fy1 - 0.135, fy1 - 0.12, 4.55, 4.85, faces="y")
    box(d, "CT_Steel", -5.35, -3.65, fy1 - 0.14, fy1, 5.1, 5.14)
    T("scoreboard", "board", (-4.5, fy1 - 0.2, 4.7), 0, topic="scores")
    q = P(B, -12.6, 11.9, 0, 0, "ball_cart")
    for sx in (-0.4, 0.4):
        for sy in (-0.25, 0.25):
            q.rod("CT_Steel", (sx, sy, 0.08), (sx, sy, 0.95), 0.03, 4)
            q.cyl("CT_Rubber", sx, sy, 0.0, 0.07, 0.06, 6)
    q.box("CT_Steel", -0.42, 0.42, -0.27, 0.27, 0.3, 0.33, f="xXyYZz")
    for sy in (-0.25, 0.25):
        q.rod("CT_Steel", (-0.4, sy, 0.95), (0.4, sy, 0.95), 0.03, 4)
        q.rod("CT_Steel", (-0.4, sy, 0.6), (0.4, sy, 0.6), 0.02, 4)
    for sx in (-0.4, 0.4):
        q.rod("CT_Steel", (sx, -0.25, 0.6), (sx, 0.25, 0.6), 0.02, 4)
        q.rod("CT_Steel", (sx, -0.25, 0.95), (sx, 0.25, 0.95), 0.02, 4)
    q.done()
    for k, (bx_, by_) in enumerate(((-0.25, -0.1), (0.0, 0.12), (0.25, -0.08), (-0.1, 0.1), (0.15, 0.0))):
        ball(-12.6 + bx_, 11.9 + by_, 0.33)
    ball(-12.5, 11.9, 0.57)
    box(mm, "CT_Steel", -13.0, -12.2, 11.65, 12.15, 0.305, 0.325)
    T("ball_cart", "sport", (-12.6, 11.9, 0.7), 0, what="basketball")
    q = P(B, -7.6, 3.4, 0, 180, "ref_chair")
    for sx in (-0.28, 0.28):
        q.rod("CT_Steel", (sx, 0.22, 0.0), (sx, 0.12, 1.2), 0.04, 4)
        q.rod("CT_Steel", (sx, -0.22, 0.0), (sx, -0.1, 1.2), 0.04, 4)
        q.rod("CT_Steel", (sx, 0.12, 1.2), (sx, 0.12, 1.75), 0.04, 4)
    q.box("CT_Wood", -0.32, 0.32, -0.18, 0.2, 1.2, 1.25, f="xXyYZz")
    q.box("CT_Wood", -0.32, 0.32, 0.1, 0.16, 1.4, 1.75, f="xXyYZz")
    for k in range(3):
        zz = 0.3 + 0.3 * k
        q.box("CT_Steel", -0.3, 0.3, -0.34 + 0.0 * k, -0.2 - 0.0, zz, zz + 0.03, f="xXyYZz")
    q.done(solid=False)
    T("ref_chair", "seat", (-7.6, 3.4, 1.2), 180, height=1.2)
    sp("ref_chair", "stand", (-7.6, 2.7, 0.0), 180)

    # studio: brush jars, drying shelf with the bowls of the week, an apron on a coat stand
    for k, (bx_, by_) in enumerate(((8.45, 2.5), (8.7, 2.55))):
        q = P(B, bx_, by_, wt["top"], 0, "brush_jar")
        q.cyl("CT_TileWhite", 0, 0, 0.0, 0.11, 0.04, 8, bottom=True)
        for j in range(5):
            a_ = 2 * math.pi * j / 5 + k
            q.rod("CT_Wood", (0.015 * math.cos(a_), 0.015 * math.sin(a_), 0.04), (0.04 * math.cos(a_), 0.04 * math.sin(a_), 0.24), 0.01, 3)
            q.rod("CT_PlasterRed" if j % 2 else "CT_PlasterBlue", (0.04 * math.cos(a_), 0.04 * math.sin(a_), 0.24), (0.045 * math.cos(a_), 0.045 * math.sin(a_), 0.29), 0.014, 3)
        q.done()
    kit.wall_shelf(B, 17.4, fy0, yaw=180, w=1.3, zt=1.35, props=False)
    kit.wall_shelf(B, 17.4, fy0, yaw=180, w=1.3, zt=0.95, props=False)
    for k, mt in enumerate(("CT_ConcreteWhite", "CT_PlasterPink", "CT_ConcreteWhite", "CT_PlasterGreen", "CT_PlasterYellow", "CT_ConcreteWhite")):
        bowl(16.9 + 0.2 * k, fy0 + 0.12, 1.35 if k < 4 else 0.95, mt)
    T("drying_shelf", "board", (17.45, fy0 + 0.2, 1.4), 180, topic="store")
    kit.hat_stand(B, 13.4, 2.3)
    box(d, "CT_PlasterBlue", 13.3, 13.5, 2.3 - 0.16, 2.3 - 0.1, 0.95, 1.55)
    box(d, "CT_PlasterBlue", 13.32, 13.48, 2.3 - 0.21, 2.3 - 0.16, 0.95, 1.25)
    box(d, "CT_TileWhite", 13.28, 13.52, 2.3 - 0.215, 2.3 - 0.21, 1.2, 1.5, faces="y")

    # buffet: compote glasses and a tray of buns on the counter
    for gy in (9.85, 10.0, 12.1):                        # compote in tumblers: red, so that they read (clear glass renders dark)
        q = P(B, 16.45, gy, 1.05, 0, "compote")
        q.kind = "prop"
        q.lathe("CT_PlasterRed", 0, 0, [(0.03, 0.0), (0.037, 0.06), (0.04, 0.1)], 8, top=True)
        q.done()
        box(d, "CT_PlasterRed", 16.42, 16.48, gy - 0.003, gy + 0.003, 1.05, 1.1, faces="xXyY") if False else None
    q = P(B, 16.7, 10.9, 1.05, 270, "bun_tray")
    q.kind = "prop"
    q.box("CT_Steel", -0.2, 0.2, -0.15, 0.15, 0.0, 0.015, f="xXyYZz")
    for k in range(6):
        q.lathe("CT_Leather", -0.14 + 0.1 * (k % 3), -0.07 + 0.14 * (k // 3), [(0.0, 0.015), (0.04, 0.02), (0.045, 0.045), (0.02, 0.07), (0.0, 0.075)], 6)
    q.done()
    # ---- the old lamps (pendants under the saw-tooth soffit)
    WARM = "#FFD2A0"

    def soffit(y):
        return 6.0 + 0.6 * ((y - hy0) % 4.0)
    for x in (-15.0, -9.0, -3.0, 3.0, 9.0, 15.0):
        for y in (4.0, 9.0):
            B.lamp((x, y, soffit(y) - 1.3), lumens=900, colour=WARM, kind="public", radius=8)
            box(d, "CT_Steel", x - 0.008, x + 0.008, y - 0.008, y + 0.008, soffit(y) - 1.3 + 0.08, soffit(y))
    B.lamp((13.5, 10.5, 4.2), lumens=700, colour=WARM, kind="public", radius=6)
    if os.environ.get("KITCHECK"):
        print("KITCHECK", kit.check(B))


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
    # big painted wooden door surround on the front wall (the door leaves are the game's actor)
    for (a, b, z0, z1, mat) in ((-3.4, -1.2, 0.0, 4.9, "CT_Door"), (1.2, 3.4, 0.0, 4.9, "CT_Door"),
                                (-3.4, 3.4, 3.2, 4.9, "CT_Door"), (-3.6, 3.6, 4.9, 5.25, "CT_SteelRed")):
        box(B.m, mat, a, b, hy0 - 0.2, hy0 + 0.05, z0, z1, faces="xXyZz" if z0 > 0 else "xXyZ")
    _furnish_hall(B, hx0, hx1, hy0, hy1)
    # vertical colour strip on the brick between door and windows
    B.footprint = [(-19.0, 0.8), (19.0, 0.8), (19.0, 27.0), (-19.0, 27.0)]
    B.height = H1 + 2.4
    B.note("hall 38x12 m enterable; %d cave windows" % sum(1 for w in B.windows if w["kind"] == "public"))
