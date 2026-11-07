"""Small furnishing helpers shared by the shop / market / lobby modules (pure Python, no bpy).

A local frame Fr(x, y, yaw): u runs along the wall, w runs OUT of the wall into the room (yaw 0: the piece faces
-y, the street; yaw 90 faces +x; same convention as B.spot). All boxes are built through hexa(), which fixes the
winding whatever the yaw is, so any angle works. Solid furniture goes to B.m, small props to B.detail.
"""
import math

import geom
from geom import prism

GOODS = ["CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterGreen", "CT_PlasterPink",
         "CT_TileWhite", "CT_SteelRed", "CT_WoodLight"]
FLOWERS = ["CT_PlasterRed", "CT_PlasterPink", "CT_PlasterYellow", "CT_TileWhite", "CT_PlasterBlue", "CT_SteelRed"]


class Fr:
    def __init__(self, x, y, yaw=0.0, z=0.0):
        self.x, self.y, self.z = x, y, z
        a = math.radians(yaw)
        self.c, self.s = math.cos(a), math.sin(a)
        self.yaw = yaw

    def p(self, u, w, z=0.0):
        return (self.x + u * self.c + w * self.s, self.y + u * self.s - w * self.c, self.z + z)

    def sub(self, u, w, dyaw=0.0):
        px, py, _ = self.p(u, w)
        return Fr(px, py, self.yaw + dyaw, self.z)


def _area(pts):
    return sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1]
               for i in range(len(pts)))


def hexa(m, mat, bot, top, faces="all", side_mat=None):
    """Hexahedron from 4 bottom and 4 top points (any winding; the sloped / sheared box)."""
    if _area(bot) < 0:
        bot = [bot[0], bot[3], bot[2], bot[1]]
        top = [top[0], top[3], top[2], top[1]]
    if faces == "all" or "Z" in faces:
        m.face(mat, list(top))
    if faces == "all" or "z" in faces:
        m.face(mat, list(reversed(bot)))
    for i in range(4):
        j = (i + 1) % 4
        m.face(side_mat or mat, [bot[i], bot[j], top[j], top[i]])


def lbox(m, mat, fr, u0, u1, w0, w1, z0, z1, faces="all"):
    pts = [(u0, w0), (u1, w0), (u1, w1), (u0, w1)]
    hexa(m, mat, [fr.p(u, w, z0) for u, w in pts], [fr.p(u, w, z1) for u, w in pts], faces)


def cyl(m, mat, x, y, z0, z1, r, segs=8, top=True, bottom=False):
    poly = [(x + r * math.cos(2 * math.pi * k / segs), y + r * math.sin(2 * math.pi * k / segs)) for k in range(segs)]
    prism(m, mat, poly, z0, z1, top=top, bottom=bottom)


def blob(m, mat, x, y, z, r, segs=6):
    """A sphere-ish blob: a short prism with a cone on each end (closed, ~4 * segs tris)."""
    poly = [(x + r * math.cos(2 * math.pi * k / segs), y + r * math.sin(2 * math.pi * k / segs)) for k in range(segs)]
    prism(m, mat, poly, z - 0.3 * r, z + 0.3 * r, top=False, bottom=False)
    for k in range(segs):
        (ax, ay), (bx, by) = poly[k], poly[(k + 1) % segs]
        m.face(mat, [(ax, ay, z + 0.3 * r), (bx, by, z + 0.3 * r), (x, y, z + 0.95 * r)])
        m.face(mat, [(bx, by, z - 0.3 * r), (ax, ay, z - 0.3 * r), (x, y, z - 0.95 * r)])


def shelving(B, fr, u0, u1, rnd, depth=0.40, tiers=4, zmax=2.2, z0=0.25, kind="box", mats=None, fill=0.85,
             carcass="CT_WoodLight", hh=(0.14, 0.34), solid=True):
    """Wall shelving along u0..u1: back, boards and uprights in B.m (collides) with goods boxes on the boards in
    B.detail. kind: box (packs), book (spines), jar (cylinders), can (paint cans)."""
    mats = mats or GOODS
    m = B.m if solid else B.detail
    d = B.detail
    lbox(m, carcass, fr, u0, u1, 0.0, 0.025, z0, zmax)
    n_up = max(2, int((u1 - u0) / 1.25) + 1)
    for i in range(n_up):
        u = u0 + (u1 - u0 - 0.04) * i / (n_up - 1)
        lbox(m, carcass, fr, u, u + 0.04, 0.0, depth, z0, zmax)
    lbox(m, carcass, fr, u0, u1, 0.0, depth, z0, z0 + 0.05)
    pitch = (zmax - z0 - 0.05) / tiers
    for t in range(tiers):
        zb = z0 + 0.05 + pitch * t
        if t:
            lbox(m, carcass, fr, u0, u1, 0.0, depth, zb - 0.025, zb + 0.015)
        zt = zb + (0.015 if t else 0.0)
        room = pitch - 0.06
        u = u0 + 0.06
        while u < u1 - 0.12:
            if rnd.random() > fill:
                u += rnd.uniform(0.08, 0.2)
                continue
            mat = rnd.choice(mats)
            if kind == "book":
                w = rnd.uniform(0.07, 0.2)
                h = min(room - 0.03, rnd.uniform(0.18, 0.30))
                lbox(d, mat, fr, u, u + w, 0.05, depth - 0.08, zt, zt + h, faces="xXyYZ")
                u += w
            elif kind in ("jar", "can"):
                r = rnd.uniform(0.045, 0.08) if kind == "jar" else 0.08
                h = min(room - 0.03, rnd.uniform(0.12, 0.26) if kind == "jar" else 0.18)
                px, py, _ = fr.p(u + r, depth * 0.45)
                cyl(d, mat, px, py, zt, zt + h, r, 5 if kind == "jar" else 6)
                u += 2 * r + 0.03
            else:
                w = rnd.uniform(0.09, 0.28)
                h = min(room - 0.03, rnd.uniform(*hh))
                lbox(d, mat, fr, u, u + w, 0.04, depth - 0.06, zt, zt + h, faces="xXyYZ")
                u += w + 0.02
    return pitch


def crate(B, x, y, z, w, dd, h, mat, goods=None, rnd=None, m=None, fill="CT_PlasterRed", yaw=0.0):
    """An open crate (4 sides + bottom in B.detail unless m given) with a heap of goods (a box) inside."""
    fr = Fr(x, y, yaw, z)
    mm = m or B.detail
    t = 0.02
    lbox(mm, mat, fr, -w / 2, w / 2, -dd / 2, dd / 2, 0.0, 0.02)
    lbox(mm, mat, fr, -w / 2, -w / 2 + t, -dd / 2, dd / 2, 0.0, h)
    lbox(mm, mat, fr, w / 2 - t, w / 2, -dd / 2, dd / 2, 0.0, h)
    lbox(mm, mat, fr, -w / 2 + t, w / 2 - t, -dd / 2, -dd / 2 + t, 0.0, h)
    lbox(mm, mat, fr, -w / 2 + t, w / 2 - t, dd / 2 - t, dd / 2, 0.0, h)
    if fill:
        lbox(B.detail, fill, fr, -w / 2 + t + 0.01, w / 2 - t - 0.01, -dd / 2 + t + 0.01, dd / 2 - t - 0.01, 0.02, h * 0.9,
             faces="xXyYZ")


def pyramid(B, x, y, z, n, size, mat, rnd=None, mats=None, m=None):
    """A pyramid of n x n down to 1 chunky fruit cubes (closed boxes, sloppy sizes)."""
    mm = m or B.detail
    k = 0
    for lvl in range(n):
        cnt = n - lvl
        off = (cnt - 1) * size / 2
        zz = z + lvl * size * 0.88
        for i in range(cnt):
            for j in range(cnt):
                mt = rnd.choice(mats) if mats and rnd else mat
                lbox(mm, mt, Fr(x - off + i * size, y - off + j * size, 0.0), -size * 0.46, size * 0.46,
                     -size * 0.46, size * 0.46, zz, zz + size * 0.8, faces="xXyYZ")
                k += 1


def bucket(B, x, y, z, r, h, mat, flower_mats=None, rnd=None, stems=True):
    cyl(B.detail, mat, x, y, z, z + h, r, 8, top=False, bottom=True)
    if flower_mats:
        for _ in range(4 if r > 0.15 else 3):
            ox, oy = rnd.uniform(-r * 0.5, r * 0.5), rnd.uniform(-r * 0.5, r * 0.5)
            hz = z + h + rnd.uniform(0.18, 0.4)
            if stems:
                lbox(B.detail, "CT_PlasterGreen", Fr(x + ox, y + oy), -0.01, 0.01, -0.01, 0.01, z + h * 0.7, hz)
            blob(B.detail, rnd.choice(flower_mats), x + ox, y + oy, hz, 0.06, 6)


def plant(B, x, y, z=0.0, h=0.8, pot="CT_PlasterRed", scale=1.0):
    """A potted plant (pot = cylinder, foliage = blobs) into B.detail."""
    k = scale
    cyl(B.detail, pot, x, y, z, z + 0.32 * k, 0.19 * k, 8, top=False, bottom=True)
    cyl(B.detail, "CT_Floor", x, y, z + 0.32 * k, z + 0.33 * k, 0.17 * k, 8)
    zb = z + 0.33 * k + 0.26 * k
    blob(B.detail, "CT_Grass", x, y, zb, 0.3 * k, 7)
    blob(B.detail, "CT_Grass", x + 0.06 * k, y - 0.04 * k, zb + 0.2 * k + h * 0.35, 0.2 * k, 6)


def mailboxes(B, fr, u, z0, cols=6, rows=3, mats=("CT_SteelRed", "CT_Steel", "CT_SteelRed", "CT_SteelWhite")):
    """A wall panel of letter boxes (centre u, bottom z0) into B.detail."""
    bw, bh = 0.27, 0.32
    W, Hh = cols * bw + 0.06, rows * bh + 0.06
    lbox(B.detail, "CT_Steel", fr, u - W / 2, u + W / 2, 0.0, 0.03, z0, z0 + Hh)
    for r in range(rows):
        for c in range(cols):
            mat = mats[(r * 3 + c) % len(mats)]
            uu = u - W / 2 + 0.03 + c * bw
            lbox(B.detail, mat, fr, uu + 0.01, uu + bw - 0.01, 0.03, 0.13, z0 + 0.03 + r * bh + 0.01,
                 z0 + 0.03 + (r + 1) * bh - 0.01, faces="xXyYZ")
            lbox(B.detail, "CT_ConcreteDark", fr, uu + 0.06, uu + bw - 0.06, 0.13, 0.135,
                 z0 + 0.03 + r * bh + bh * 0.62, z0 + 0.03 + r * bh + bh * 0.72, faces="Y")


def doormat(B, fr, u, w, z=0.0, size=(0.9, 0.6), mat="CT_ConcreteDark"):
    lbox(B.detail, mat, fr, u - size[0] / 2, u + size[0] / 2, w - size[1] / 2, w + size[1] / 2, z, z + 0.02,
         faces="xXyYZ")


def awning(B, fr, u0, u1, proj, z_wall, z_edge, k0=0, stripe=0.5, mats=("CT_SteelRed", "CT_TileWhite")):
    """A striped sloped awning over a door: strips of `stripe` m alternate mats[0], mats[1]; the front edge has
    a valance. fr is on the wall face; w out. Into B.detail (a thin closed slab). Returns the next stripe index."""
    n = max(1, int(round((u1 - u0) / stripe)))
    t = 0.035
    for i in range(n):
        a = u0 + (u1 - u0) * i / n
        b = u0 + (u1 - u0) * (i + 1) / n
        mat = mats[(k0 + i) % 2]
        bot = [fr.p(a, 0.0, z_wall - t), fr.p(b, 0.0, z_wall - t), fr.p(b, proj, z_edge - t), fr.p(a, proj, z_edge - t)]
        top = [fr.p(a, 0.0, z_wall), fr.p(b, 0.0, z_wall), fr.p(b, proj, z_edge), fr.p(a, proj, z_edge)]
        hexa(B.detail, mat, bot, top)
        lbox(B.detail, mat, fr, a, b, proj - 0.02, proj + 0.01, z_edge - 0.2, z_edge, faces="xXyYz")
    return k0 + n


def chair(B, x, y, yaw, seat_mat="CT_PlasterBlue", leg="CT_Steel", back=True, z=0.0):
    """A small chair into B.detail; yaw = the way the sitter faces."""
    fr = Fr(x, y, yaw, z)
    lbox(B.detail, seat_mat, fr, -0.21, 0.21, -0.21, 0.21, 0.44, 0.48)
    for du in (-0.19, 0.19):
        for dw in (-0.19, 0.19):
            lbox(B.detail, leg, fr, du - 0.015, du + 0.015, dw - 0.015, dw + 0.015, 0.0, 0.44, faces="xXyY")
    if back:
        lbox(B.detail, seat_mat, fr, -0.21, 0.21, -0.22, -0.19, 0.48, 0.88)      # back behind the sitter (the sitter faces +w)


def table_round(B, x, y, r=0.45, h=0.74, top="CT_WoodLight", leg="CT_Steel"):
    cyl(B.m, leg, x, y, 0.0, h - 0.04, 0.04, 6, top=False)
    cyl(B.m, top, x, y, h - 0.04, h, r, 10, top=True, bottom=True)
    cyl(B.m, leg, x, y, 0.0, 0.03, 0.2, 8)


def rod(m, mat, a, b, w, caps=True):
    """A square bar of side w between 3D points a and b (any direction), every face checked to point outward."""
    d = geom.norm(geom.sub(b, a))
    ref = (0, 0, 1) if abs(d[2]) < 0.9 else (1, 0, 0)
    u = geom.norm(geom.cross(d, ref))
    v = geom.cross(d, u)
    h = w / 2

    def P(p, su, sv):
        return tuple(p[i] + u[i] * su * h + v[i] * sv * h for i in range(3))
    sq = ((-1, -1), (1, -1), (1, 1), (-1, 1))
    ra = [P(a, *s) for s in sq]
    rb = [P(b, *s) for s in sq]
    cen = tuple((a[i] + b[i]) / 2 for i in range(3))
    quads = [[ra[k], ra[(k + 1) % 4], rb[(k + 1) % 4], rb[k]] for k in range(4)]
    if caps:
        quads += [ra[::-1], rb]
    for q in quads:
        n = geom.cross(geom.sub(q[1], q[0]), geom.sub(q[2], q[0]))
        mid = tuple(sum(p[i] for p in q) / len(q) for i in range(3))
        if sum(n[i] * (mid[i] - cen[i]) for i in range(3)) < 0:
            q = q[::-1]
        m.face(mat, q)
