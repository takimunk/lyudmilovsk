"""Builds the outdoor map's placeholder geometry (tools/world_map.py) as one .glb: the ground with
its beach, the concrete-slab path, the beach promenade, the town's houses, placeholder trees and
the cable car. Pure Python (no bpy, no numpy), so the Unreal build can run it itself and the .glb
never needs committing.

  python3 tools/world_mesh.py out.glb [textures_dir]

glTF is Y-up and right-handed; Interchange turns it into Unreal the same way as Blender's export:
map (X, Y, Z) m -> glTF (X, Z, Y) -> Unreal (X, Y, Z) cm. Positions are absolute map metres plus
the Unreal portal offset, so the imported actors sit at the origin.
"""
import array
import json
import math
import os
import random
import shutil
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import world_map as W  # noqa: E402

# Materials: name -> (rgb, roughness, texture file or None, uv metres per tile)
MATERIALS = {
    "World_Grass": ((0.32, 0.42, 0.2), 0.95, "grass.jpg", 3.0),
    "World_Sand": ((0.52, 0.45, 0.33), 0.9, None, 1.0),
    "World_SlabConcrete": ((0.55, 0.54, 0.5), 0.9, "ceiling.jpg", 2.0),
    "World_Plaster_White": ((0.78, 0.76, 0.7), 0.85, None, 1.0),
    "World_Plaster_Yellow": ((0.78, 0.66, 0.42), 0.85, None, 1.0),
    "World_Brick": ((0.5, 0.25, 0.18), 0.9, None, 1.0),
    "World_Panel_Grey": ((0.6, 0.6, 0.58), 0.85, None, 1.0),
    "World_Roof_Tile": ((0.55, 0.2, 0.12), 0.7, None, 1.0),
    "World_Roof_Flat": ((0.25, 0.25, 0.26), 0.9, None, 1.0),
    "World_Bark": ((0.25, 0.19, 0.14), 0.95, None, 1.0),
    "World_Crown": ((0.2, 0.33, 0.14), 0.9, None, 1.0),
    "World_Crown_Autumn": ((0.55, 0.4, 0.12), 0.9, None, 1.0),
    "World_Pine": ((0.12, 0.24, 0.13), 0.9, None, 1.0),
    "World_Steel": ((0.35, 0.36, 0.38), 0.5, None, 1.0),
    "World_Cabin_Red": ((0.7, 0.08, 0.06), 0.5, None, 1.0),
}


class Prim:
    """Triangles of one material, flat or smooth."""

    def __init__(self):
        self.pos, self.nrm, self.uv, self.idx = array.array("f"), array.array("f"), array.array("f"), array.array("I")

    def vert(self, p, n, uv):
        # map -> glTF axes
        self.pos.extend((p[0], p[2], p[1]))
        self.nrm.extend((n[0], n[2], n[1]))
        self.uv.extend(uv)
        return len(self.pos) // 3 - 1

    def tri(self, a, b, c):
        """Indices a, b, c; flipped if needed so the front faces along the vertex normals."""
        P = self.pos
        ax, ay, az = P[3 * a:3 * a + 3]
        bx, by, bz = P[3 * b:3 * b + 3]
        cx, cy, cz = P[3 * c:3 * c + 3]
        ux, uy, uz = bx - ax, by - ay, bz - az
        vx, vy, vz = cx - ax, cy - ay, cz - az
        nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
        N = self.nrm
        hx = N[3 * a] + N[3 * b] + N[3 * c]
        hy = N[3 * a + 1] + N[3 * b + 1] + N[3 * c + 1]
        hz = N[3 * a + 2] + N[3 * b + 2] + N[3 * c + 2]
        if nx * hx + ny * hy + nz * hz < 0:
            b, c = c, b
        self.idx.extend((a, b, c))

    def quad(self, ps, n, tile=1.0):
        """Flat quad, corners in order; UVs projected on the quad's plane."""
        u_ax = [ps[1][i] - ps[0][i] for i in range(3)]
        lu = math.sqrt(sum(v * v for v in u_ax)) or 1.0
        u_ax = [v / lu for v in u_ax]
        v_ax = [n[1] * u_ax[2] - n[2] * u_ax[1], n[2] * u_ax[0] - n[0] * u_ax[2], n[0] * u_ax[1] - n[1] * u_ax[0]]
        ids = []
        for p in ps:
            d = [p[i] - ps[0][i] for i in range(3)]
            ids.append(self.vert(p, n, (sum(d[i] * u_ax[i] for i in range(3)) / tile,
                                        sum(d[i] * v_ax[i] for i in range(3)) / tile)))
        self.tri(ids[0], ids[1], ids[2])
        self.tri(ids[0], ids[2], ids[3])

    def poly(self, ps, n, tile=1.0):
        """Flat convex polygon (fan)."""
        ids = [self.vert(p, n, (p[0] / tile, p[1] / tile + p[2] / tile)) for p in ps]
        for i in range(1, len(ids) - 1):
            self.tri(ids[0], ids[i], ids[i + 1])


class Mesh:
    def __init__(self, name):
        self.name = name
        self.prims = {}

    def p(self, mat):
        return self.prims.setdefault(mat, Prim())


def _norm(v):
    l = math.sqrt(sum(c * c for c in v)) or 1.0
    return tuple(c / l for c in v)


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def box(prim, c, size, yaw=0.0, tile=1.0, top=True, bottom=False):
    """Box centred at c (x, y, z bottom) of size (w, d, h), turned by yaw around Z."""
    w, d, h = size
    ca, sa = math.cos(yaw), math.sin(yaw)

    def P(u, v, z):
        return (c[0] + u * ca - v * sa, c[1] + u * sa + v * ca, c[2] + z)

    corners = [(-w / 2, -d / 2), (w / 2, -d / 2), (w / 2, d / 2), (-w / 2, d / 2)]
    for i in range(4):
        u0, v0 = corners[i]
        u1, v1 = corners[(i + 1) % 4]
        ps = [P(u0, v0, 0), P(u1, v1, 0), P(u1, v1, h), P(u0, v0, h)]
        e = (ps[1][0] - ps[0][0], ps[1][1] - ps[0][1], 0.0)
        n = _norm((e[1], -e[0], 0.0))
        mid = ((ps[0][0] + ps[1][0]) / 2 - c[0], (ps[0][1] + ps[1][1]) / 2 - c[1])
        if n[0] * mid[0] + n[1] * mid[1] < 0:
            n = (-n[0], -n[1], 0.0)
        prim.quad(ps, n, tile)
    if top:
        prim.quad([P(u, v, h) for u, v in corners], (0, 0, 1), tile)
    if bottom:
        prim.quad([P(u, v, 0) for u, v in corners], (0, 0, -1), tile)


def gable_roof(prim, gable_prim, c, w, d, z, yaw, pitch=math.radians(42), over=0.4):
    """Steep gable roof over a w x d footprint at height z, ridge along the longer side."""
    if w < d:
        w, d, yaw = d, w, yaw + math.pi / 2
    ca, sa = math.cos(yaw), math.sin(yaw)

    def P(u, v, zz):
        return (c[0] + u * ca - v * sa, c[1] + u * sa + v * ca, zz)

    rh = (d / 2) * math.tan(pitch)
    hw, hd = w / 2 + over, d / 2 + over
    ridge0, ridge1 = P(-hw, 0, z + rh), P(hw, 0, z + rh)
    drop = over * math.tan(pitch)
    for side in (-1, 1):
        e0, e1 = P(-hw, side * hd, z - drop), P(hw, side * hd, z - drop)
        n = _norm(_cross((e1[0] - e0[0], e1[1] - e0[1], 0), (ridge0[0] - e0[0], ridge0[1] - e0[1], ridge0[2] - e0[2])))
        if n[2] < 0:
            n = tuple(-x for x in n)
        prim.quad([e0, e1, ridge1, ridge0], n, 1.0)
    for end in (-1, 1):
        u = end * w / 2
        tri = [P(u, -d / 2, z), P(u, d / 2, z), P(u, 0, z + rh)]
        n = (end * ca, end * sa, 0.0)
        gable_prim.poly(tri, n)


def icosphere():
    t = (1 + 5 ** 0.5) / 2
    v = [_norm(p) for p in [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t), (0, -1, -t),
                            (0, 1, -t), (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]]
    f = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6),
         (7, 1, 8), (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5), (2, 4, 11), (6, 2, 10),
         (8, 6, 7), (9, 8, 1)]
    mid = {}

    def m(a, b):
        k = (min(a, b), max(a, b))
        if k not in mid:
            v.append(_norm(tuple((v[a][i] + v[b][i]) / 2 for i in range(3))))
            mid[k] = len(v) - 1
        return mid[k]

    f2 = []
    for a, b, c in f:
        ab, bc, ca = m(a, b), m(b, c), m(c, a)
        f2 += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
    return v, f2


ICO = icosphere()


def blob(prim, c, r, squash=1.0, rnd=None):
    vs, fs = ICO
    base = len(prim.pos) // 3
    for p in vs:
        k = 1.0 + (rnd.uniform(-0.12, 0.12) if rnd else 0.0)
        prim.vert((c[0] + p[0] * r * k, c[1] + p[1] * r * k, c[2] + p[2] * r * squash * k), p,
                  (p[0] * 0.5 + 0.5, p[2] * 0.5 + 0.5))
    for a, b, cc in fs:
        prim.tri(base + a, base + b, base + cc)


def cone(prim, c, r, h, n=8):
    base = len(prim.pos) // 3
    for i in range(n):
        a = 2 * math.pi * i / n
        prim.vert((c[0] + r * math.cos(a), c[1] + r * math.sin(a), c[2]), _norm((math.cos(a), math.sin(a), r / h)),
                  (i / n, 0))
    tip = prim.vert((c[0], c[1], c[2] + h), (0, 0, 1), (0.5, 1))
    for i in range(n):
        prim.tri(base + i, base + (i + 1) % n, tip)


def tree(mesh, x, y, r, h, rnd, autumn=False, pine=False):
    z = W.height(x, y)
    trunk_h = h * (0.55 if pine else 0.35)
    box(mesh.p("World_Bark"), (x, y, z - 0.3), (0.35 + h * 0.012, 0.35 + h * 0.012, trunk_h + 0.3), rnd.uniform(0, 3), top=False)
    if pine:
        cone(mesh.p("World_Pine"), (x, y, z + trunk_h * 0.6), r * 0.9, h - trunk_h * 0.6)
    else:
        mat = "World_Crown_Autumn" if autumn else "World_Crown"
        cr = (h - trunk_h) / 2
        blob(mesh.p(mat), (x, y, z + trunk_h + cr * 0.9), r, squash=cr / r, rnd=rnd)


# --- the pieces ------------------------------------------------------------------------------

def axis(lo, hi, steps):
    """Coordinates from lo to hi; steps = [(until, step), ...] for the absolute value of the
    coordinate (fine near 0, coarse far away)."""
    out = set()
    for sign in (-1, 1):
        v = 0.0
        while True:
            st = next(s for u, s in steps if abs(v) < u)
            v += st
            if sign * v < lo or sign * v > hi:
                break
            out.add(round(sign * v, 2))
    out.add(0.0)
    out.add(lo)
    out.add(hi)
    return sorted(x for x in out if lo <= x <= hi)


def ground(meshes):
    """Height grid over the whole map, finer at the club and along the shore."""
    xs = set(axis(W.X_MIN, W.X_MAX, [(20, 0.5), (60, 2.0), (450, 4.0), (1e9, 20.0)]))
    xs |= {W.DOOR_X - 6.2, 6.2, -6.2}
    xs = sorted(xs)
    ys = set(axis(W.Y_MIN, W.Y_MAX, [(15, 0.5), (200, 2.0), (2000, 4.0), (1e9, 20.0)]))
    ys |= {0.01, -0.3}
    ys = sorted(y for y in ys if not (-0.3 < y < 0.0))   # the portal face: the mound drops at 0.0 / 0.01
    # Behind the portal the ground stays fine only over the corridor; coarse elsewhere is fine.
    m_grass, m_sand = meshes.p("World_Grass"), meshes.p("World_Sand")
    rows = []
    for y in ys:
        row = []
        for x in xs:
            z = W.height(x, y) if not (y == 0.0 and abs(x) <= 6.2) else W.height(x, -0.001)
            if y == 0.01 and abs(x) <= 6.2:
                z = W.height(x, 0.02)
            row.append((x, y, z))
        rows.append(row)
    # one shared vertex per grid point per material
    idmap = [{}, {}]

    def vid(i, j, sand):
        d = idmap[sand]
        k = (i, j)
        if k not in d:
            x, y, z = rows[i][j]
            pr = m_sand if sand else m_grass
            tile = MATERIALS["World_Sand" if sand else "World_Grass"][3]
            d[k] = pr.vert((x, y, z), W.normal(x, y), (x / tile, y / tile))
        return d[k]

    for i in range(len(ys) - 1):
        for j in range(len(xs) - 1):
            if ys[i] == 0.0 and abs(xs[j]) <= 6.2 and abs(xs[j + 1]) <= 6.2:
                continue                              # the portal stands in the drop from the mound
            cx, cy = (xs[j] + xs[j + 1]) / 2, (ys[i] + ys[i + 1]) / 2
            sand = 1 if W.is_beach(cx, cy) else 0
            pr = m_sand if sand else m_grass
            a, b, c, d = vid(i, j, sand), vid(i, j + 1, sand), vid(i + 1, j + 1, sand), vid(i + 1, j, sand)
            pr.tri(a, b, c)
            pr.tri(a, c, d)


def path(mesh):
    """Soviet concrete slabs, 2 x 3 m, laid along the path a little crooked."""
    rnd = random.Random(3)
    pts = W.path_points(0.5)
    s = 0.0
    seg = []
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        seg.append((x0, y0, x1, y1, math.hypot(x1 - x0, y1 - y0)))
    total = sum(sg[4] for sg in seg)
    pr = mesh.p("World_SlabConcrete")
    while s + 3.0 <= total:
        c = s + 1.5
        acc = 0.0
        for x0, y0, x1, y1, l in seg:
            if acc + l >= c:
                t = (c - acc) / l
                x, y = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
                yaw = math.atan2(y1 - y0, x1 - x0) - math.pi / 2
                break
            acc += l
        z = min(W.height(x + dx, y + dy) for dx in (-1, 0, 1) for dy in (-1.5, 0, 1.5))
        box(pr, (x, y, z - 0.12), (2.0, 2.95, 0.2 + rnd.uniform(0, 0.04)), yaw + rnd.uniform(-0.03, 0.03), tile=2.0)
        s += 3.05


def promenade(mesh):
    """Concrete promenade at the foot of the bluff under the town, from the cable car both ways."""
    pr = mesh.p("World_SlabConcrete")
    y = 640.0
    while y < 1320.0:
        x = W.shore_x(y + 5) - W.BEACH_W + 5.0
        z = W.height(x, y + 5)
        x2 = W.shore_x(y + 5)
        box(pr, (x, y + 5, z - 0.6), (8.0, 10.2, 1.0), math.atan2(W.shore_x(y + 10) - W.shore_x(y), 10.0) * -1,
            tile=2.0)
        _ = x2
        y += 10.0


def town(mesh):
    rnd = random.Random(11)
    houses, trees = W.town()
    walls = ["World_Plaster_White", "World_Plaster_Yellow", "World_Brick", "World_Plaster_White"]
    for x, y, yaw, w, d, fl, kind in houses:
        z = min(W.height(x + dx, y + dy) for dx in (-w / 2, 0, w / 2) for dy in (-d / 2, 0, d / 2))
        if kind == "block":
            h = fl * 2.8 + 0.6
            box(mesh.p("World_Panel_Grey"), (x, y, z - 0.5), (w, d, h + 0.5), yaw, top=False)
            box(mesh.p("World_Roof_Flat"), (x, y, z + h), (w + 0.3, d + 0.3, 0.3), yaw)
        elif kind == "kirche":
            box(mesh.p("World_Brick"), (x, y, z - 0.5), (w, d, 30.5), yaw, top=False)
            box(mesh.p("World_Brick"), (x - 14 * math.cos(yaw), y - 14 * math.sin(yaw), z - 0.5), (22, 14, 14.5), yaw,
                top=False)
            gable_roof(mesh.p("World_Roof_Tile"), mesh.p("World_Brick"),
                       (x - 14 * math.cos(yaw), y - 14 * math.sin(yaw)), 22, 14, z + 14, yaw)
            cone(mesh.p("World_Roof_Flat"), (x, y, z + 30), w * 0.72, 16, n=4)
        else:
            h = fl * 3.0 + 0.4
            wall = rnd.choice(walls)
            box(mesh.p(wall), (x, y, z - 0.5), (w, d, h + 0.5), yaw, top=False)
            gable_roof(mesh.p("World_Roof_Tile"), mesh.p(wall), (x, y), w, d, z + h, yaw)
    for x, y, r, h in trees:
        tree(mesh, x, y, r, h, rnd, autumn=rnd.random() < 0.3)


def woods(mesh):
    rnd = random.Random(12)
    for x, y, r, h in W.woods():
        pine = rnd.random() < 0.6                     # Baltic: pines on the ridges, broadleaf between
        tree(mesh, x, y, r, h, rnd, autumn=(not pine and rnd.random() < 0.35), pine=pine)


def cable_car(mesh):
    cc = W.cable_car()
    y = W.CABLE_Y
    (ux, _), (lx, _), (tx, _) = cc["upper"], cc["lower"], cc["tower"]
    uz, lz, tz = W.height(ux, y), W.height(lx, y), W.height(tx, y)
    conc, steel = mesh.p("World_SlabConcrete"), mesh.p("World_Steel")
    # stations: a concrete hall with a flat roof, open toward the line
    box(conc, (ux - 4, y, uz - 1), (14, 12, 7), 0.0, tile=2.0)
    box(mesh.p("World_Roof_Flat"), (ux - 4, y, uz + 6), (15, 13, 0.4), 0.0)
    box(conc, (lx - 2, y, lz - 0.5), (12, 12, 6), 0.0, tile=2.0)
    box(mesh.p("World_Roof_Flat"), (lx - 2, y, lz + 5.5), (13, 13, 0.4), 0.0)
    # tower at the bluff edge
    th = 14.0
    box(steel, (tx, y, tz - 0.5), (1.2, 1.2, th + 0.5), 0.0)
    box(steel, (tx, y, tz + th), (1.0, 7.0, 0.6), 0.0)
    # two ropes (up and down line) sagging between stations over the tower, a cabin on each
    a = (ux + 3, uz + 5.0)
    b = (tx, tz + th + 0.3)
    c = (lx + 4, lz + 4.5)
    for side in (-3.0, 3.0):
        pts = []
        for (x0, z0), (x1, z1) in ((a, b), (b, c)):
            for k in range(12):
                t = k / 12
                sag = 4 * 0.6 * t * (1 - t) * (abs(x1 - x0) / 40.0)
                pts.append((x0 + (x1 - x0) * t, z0 + (z1 - z0) * t - sag))
        pts.append(c)
        for (x0, z0), (x1, z1) in zip(pts, pts[1:]):
            l = math.hypot(x1 - x0, z1 - z0)
            mid = ((x0 + x1) / 2, (z0 + z1) / 2)
            ang = math.atan2(z1 - z0, x1 - x0)
            # thin rope as a 4-sided tube along X
            r = 0.04
            for i in range(4):
                a0, a1 = i * math.pi / 2, (i + 1) * math.pi / 2
                def at(u, aa):
                    dy, dz = r * math.cos(aa), r * math.sin(aa)
                    return (mid[0] + u * math.cos(ang) - dz * math.sin(ang), y + side + dy,
                            mid[1] + u * math.sin(ang) + dz * math.cos(ang))
                ps = [at(-l / 2, a0), at(l / 2, a0), at(l / 2, a1), at(-l / 2, a1)]
                am = (a0 + a1) / 2
                n = (-math.sin(am) * math.sin(ang), math.cos(am), math.sin(am) * math.cos(ang))
                steel.quad(ps, n)
        # a cabin hanging from this rope, a third of the way down (the other line's further on)
        k = int(len(pts) * (0.3 if side < 0 else 0.62))
        cx_, cz_ = pts[k]
        box(steel, (cx_, y + side, cz_ - 1.2), (0.12, 0.12, 1.2), 0.0)
        box(mesh.p("World_Cabin_Red"), (cx_, y + side, cz_ - 3.4), (1.9, 1.7, 2.2), 0.0, bottom=True)


# --- glb writer --------------------------------------------------------------------------------

def write_glb(out, meshes, tex_dir):
    out_dir = os.path.dirname(os.path.abspath(out))
    gl = {"asset": {"version": "2.0", "generator": "lyudmilovsk world_mesh.py"}, "scene": 0,
          "scenes": [{"nodes": []}], "nodes": [], "meshes": [], "materials": [], "accessors": [],
          "bufferViews": [], "buffers": [], "images": [], "textures": [], "samplers": [{"wrapS": 10497, "wrapT": 10497}]}
    blob_ = bytearray()
    mat_ids = {}

    def material(name):
        if name in mat_ids:
            return mat_ids[name]
        rgb, rough, tex, _ = MATERIALS[name]
        m = {"name": name, "pbrMetallicRoughness": {"baseColorFactor": list(rgb) + [1.0], "metallicFactor": 0.0,
                                                    "roughnessFactor": rough}}
        if tex and tex_dir and os.path.exists(os.path.join(tex_dir, tex)):
            shutil.copy(os.path.join(tex_dir, tex), os.path.join(out_dir, tex))
            gl["images"].append({"uri": tex})
            gl["textures"].append({"source": len(gl["images"]) - 1, "sampler": 0})
            m["pbrMetallicRoughness"]["baseColorTexture"] = {"index": len(gl["textures"]) - 1}
            m["pbrMetallicRoughness"]["baseColorFactor"] = [1, 1, 1, 1]
        gl["materials"].append(m)
        mat_ids[name] = len(gl["materials"]) - 1
        return mat_ids[name]

    def view(data, target):
        while len(blob_) % 4:
            blob_.append(0)
        off = len(blob_)
        blob_.extend(data)
        gl["bufferViews"].append({"buffer": 0, "byteOffset": off, "byteLength": len(data), "target": target})
        return len(gl["bufferViews"]) - 1

    def accessor(arr, comp, typ, n, target, minmax=False):
        a = {"bufferView": view(arr.tobytes(), target), "componentType": comp, "count": n, "type": typ}
        if minmax:
            k = 3
            a["min"] = [min(arr[i::k]) for i in range(k)]
            a["max"] = [max(arr[i::k]) for i in range(k)]
        gl["accessors"].append(a)
        return len(gl["accessors"]) - 1

    tris = 0
    for mesh in meshes:
        prims = []
        for mname, p in mesh.prims.items():
            if not p.idx:
                continue
            n = len(p.pos) // 3
            prims.append({"attributes": {"POSITION": accessor(p.pos, 5126, "VEC3", n, 34962, True),
                                         "NORMAL": accessor(p.nrm, 5126, "VEC3", n, 34962),
                                         "TEXCOORD_0": accessor(p.uv, 5126, "VEC2", n, 34962)},
                          "indices": accessor(p.idx, 5125, "SCALAR", len(p.idx), 34963),
                          "material": material(mname)})
            tris += len(p.idx) // 3
        gl["meshes"].append({"name": mesh.name, "primitives": prims})
        gl["nodes"].append({"name": mesh.name, "mesh": len(gl["meshes"]) - 1})
        gl["scenes"][0]["nodes"].append(len(gl["nodes"]) - 1)
    for k in ("images", "textures"):
        if not gl[k]:
            del gl[k]
    gl["buffers"].append({"byteLength": len(blob_)})
    js = json.dumps(gl, separators=(",", ":")).encode()
    js += b" " * ((4 - len(js) % 4) % 4)
    while len(blob_) % 4:
        blob_.append(0)
    with open(out, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(blob_)))
        f.write(struct.pack("<II", len(js), 0x4E4F534A) + js)
        f.write(struct.pack("<II", len(blob_), 0x004E4942) + bytes(blob_))
    return tris


def build(out, tex_dir=None):
    meshes = []
    for name, fn in (("World_Ground", ground), ("World_Path", path), ("World_Promenade", promenade),
                     ("World_Town", town), ("World_Woods", woods), ("World_CableCar", cable_car)):
        m = Mesh(name)
        fn(m)
        meshes.append(m)
    return write_glb(out, meshes, tex_dir)


if __name__ == "__main__":
    import time
    t0 = time.time()
    n = build(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
    print("%s: %d triangles, %.1f s" % (sys.argv[1], n, time.time() - t0))
