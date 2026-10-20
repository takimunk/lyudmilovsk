"""Tiny signed-distance modelling kit for Blender (numpy + the openvdb module bundled with Blender).

A Piece is a list of operations on primitives; it is sampled on a voxel grid and turned into a
quad mesh with openvdb's volume-to-mesh. Every primitive can carry a bone name, which later gives
smooth skin weights (each vertex is weighted by how close it is to the primitives of each bone).

Units are metres. Primitive functions take three equally shaped arrays X, Y, Z and return distances.
"""
import math

import numpy as np
import openvdb


def v3(x):
    return np.asarray(x, dtype=np.float64)


def norm(v):
    v = v3(v)
    return v / np.linalg.norm(v)


def frame(fwd, up):
    """Orthonormal 3x3 (columns: side, fwd, up) from a forward and an approximate up vector."""
    f = norm(fwd)
    u = v3(up) - f * np.dot(up, f)
    u = u / np.linalg.norm(u)
    s = np.cross(f, u)
    return np.stack([s, f, u], axis=1)


def rot(axis, deg):
    a = norm(axis)
    t = math.radians(deg)
    c, s = math.cos(t), math.sin(t)
    x, y, z = a
    return np.array([[c + x * x * (1 - c), x * y * (1 - c) - z * s, x * z * (1 - c) + y * s],
                     [y * x * (1 - c) + z * s, c + y * y * (1 - c), y * z * (1 - c) - x * s],
                     [z * x * (1 - c) - y * s, z * y * (1 - c) + x * s, c + z * z * (1 - c)]])


# ------------------------------------------------------------------ primitives
class Prim:
    def __init__(self, fn, lo, hi):
        self.fn, self.lo, self.hi = fn, v3(lo), v3(hi)

    def __call__(self, X, Y, Z):
        return self.fn(X, Y, Z)


def _local(X, Y, Z, c, R):
    dx, dy, dz = X - c[0], Y - c[1], Z - c[2]
    if R is None:
        return dx, dy, dz
    return (R[0, 0] * dx + R[1, 0] * dy + R[2, 0] * dz,
            R[0, 1] * dx + R[1, 1] * dy + R[2, 1] * dz,
            R[0, 2] * dx + R[1, 2] * dy + R[2, 2] * dz)


def ellipsoid(c, r, R=None):
    """R: 3x3 whose columns are the ellipsoid's local axes in world space."""
    c, r = v3(c), v3(r)

    def fn(X, Y, Z):
        x, y, z = _local(X, Y, Z, c, R)
        k0 = np.sqrt((x / r[0]) ** 2 + (y / r[1]) ** 2 + (z / r[2]) ** 2)
        k1 = np.sqrt((x / r[0] ** 2) ** 2 + (y / r[1] ** 2) ** 2 + (z / r[2] ** 2) ** 2)
        return k0 * (k0 - 1.0) / np.maximum(k1, 1e-9)
    m = r.max()
    return Prim(fn, c - m, c + m)


def sphere(c, r):
    c = v3(c)

    def fn(X, Y, Z):
        return np.sqrt((X - c[0]) ** 2 + (Y - c[1]) ** 2 + (Z - c[2]) ** 2) - r
    return Prim(fn, c - r, c + r)


def round_cone(a, b, r1, r2):
    """Capsule with different end radii (Inigo Quilez)."""
    a, b = v3(a), v3(b)
    ba = b - a
    l2 = float(np.dot(ba, ba))
    rr = r1 - r2
    a2 = l2 - rr * rr
    il2 = 1.0 / l2

    def fn(X, Y, Z):
        px, py, pz = X - a[0], Y - a[1], Z - a[2]
        y = px * ba[0] + py * ba[1] + pz * ba[2]
        z = y - l2
        xx = (px * l2 - ba[0] * y) ** 2 + (py * l2 - ba[1] * y) ** 2 + (pz * l2 - ba[2] * y) ** 2
        y2 = y * y * l2
        z2 = z * z * l2
        k = math.copysign(1.0, rr) * rr * rr * xx
        d = (np.sqrt(xx * a2 * il2) + y * rr) * il2 - r1
        d = np.where(np.sign(z) * a2 * z2 > k, np.sqrt(xx + z2) * il2 - r2, d)
        d = np.where(np.sign(y) * a2 * y2 < k, np.sqrt(xx + y2) * il2 - r1, d)
        return d
    m = max(r1, r2)
    return Prim(fn, np.minimum(a, b) - m, np.maximum(a, b) + m)


def capsule(a, b, r):
    return round_cone(a, b, r, r)


def rbox(c, half, R=None, rad=0.0):
    """Rounded box. half: half extents (before rounding)."""
    c, half = v3(c), v3(half)

    def fn(X, Y, Z):
        x, y, z = _local(X, Y, Z, c, R)
        qx, qy, qz = np.abs(x) - half[0] + rad, np.abs(y) - half[1] + rad, np.abs(z) - half[2] + rad
        out = np.sqrt(np.maximum(qx, 0) ** 2 + np.maximum(qy, 0) ** 2 + np.maximum(qz, 0) ** 2)
        return out + np.minimum(np.maximum(qx, np.maximum(qy, qz)), 0) - rad
    m = float(np.linalg.norm(half))
    return Prim(fn, c - m, c + m)


def plate(c, half, R=None, rad=0.0):
    """Flat plate: a rectangle with rounded corners in local x/z (half extents half[0], half[2]),
    half[1] thick along local y, sharp faces."""
    c, half = v3(c), v3(half)

    def fn(X, Y, Z):
        x, y, z = _local(X, Y, Z, c, R)
        qx, qz = np.abs(x) - half[0] + rad, np.abs(z) - half[2] + rad
        d2 = np.sqrt(np.maximum(qx, 0) ** 2 + np.maximum(qz, 0) ** 2) + np.minimum(np.maximum(qx, qz), 0) - rad
        return np.maximum(d2, np.abs(y) - half[1])
    m = float(np.linalg.norm(half))
    return Prim(fn, c - m, c + m)


def halfspace(p, n):
    """Solid on the side opposite to n (distance positive along n). Unbounded."""
    p, n = v3(p), norm(n)

    def fn(X, Y, Z):
        return (X - p[0]) * n[0] + (Y - p[1]) * n[1] + (Z - p[2]) * n[2]
    return Prim(fn, [-1e9] * 3, [1e9] * 3)


def chain(points, radii):
    """Round cones through a polyline."""
    return [round_cone(points[i], points[i + 1], radii[i], radii[i + 1]) for i in range(len(points) - 1)]


# ------------------------------------------------------------------ blending
def smin(a, b, k):
    if k <= 0:
        return np.minimum(a, b)
    h = np.clip(0.5 + 0.5 * (b - a) / k, 0.0, 1.0)
    return b + (a - b) * h - k * h * (1.0 - h)


def smax(a, b, k):
    return -smin(-a, -b, k)


def _hash(ix, iy, iz, seed):
    h = (ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791) ^ (seed * 2654435761)
    h = (h ^ (h >> 13)) * 1274126177
    h = h ^ (h >> 16)
    return (h & 0xFFFF).astype(np.float32) / 65535.0


def vnoise(X, Y, Z, freq, seed=0):
    """Smooth value noise in [-1, 1]."""
    x, y, z = X * freq, Y * freq, Z * freq
    ix, iy, iz = np.floor(x).astype(np.int64), np.floor(y).astype(np.int64), np.floor(z).astype(np.int64)
    fx, fy, fz = x - ix, y - iy, z - iz
    fx, fy, fz = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy), fz * fz * (3 - 2 * fz)
    out = 0.0
    for dx in (0, 1):
        wx = fx if dx else 1 - fx
        for dy in (0, 1):
            wy = fy if dy else 1 - fy
            for dz in (0, 1):
                wz = fz if dz else 1 - fz
                out = out + wx * wy * wz * _hash(ix + dx, iy + dy, iz + dz, seed)
    return out * 2.0 - 1.0


# ------------------------------------------------------------------ pieces
class Piece:
    """ops: ('add'|'sub'|'int', prim, k, bone) and ('disp', fn(X,Y,Z)->offset, lo, hi)."""

    def __init__(self, name, voxel):
        self.name, self.voxel, self.ops = name, voxel, []

    def add(self, prim, k=0.0, bone=None):
        self.ops.append(("add", prim, k, bone))
        return prim

    def sub(self, prim, k=0.0):
        self.ops.append(("sub", prim, k, None))

    def clip(self, prim, k=0.0):
        """Keep only the inside of prim (intersection)."""
        self.ops.append(("int", prim, k, None))

    def disp(self, fn, lo, hi):
        self.ops.append(("disp", fn, v3(lo), v3(hi)))

    def bounds(self):
        los = [op[1].lo for op in self.ops if op[0] == "add"]
        his = [op[1].hi for op in self.ops if op[0] == "add"]
        return np.min(los, axis=0), np.max(his, axis=0)

    def sample(self):
        vs = self.voxel
        lo, hi = self.bounds()
        lo, hi = lo - 4 * vs, hi + 4 * vs
        n = np.ceil((hi - lo) / vs).astype(int) + 1
        axes = [lo[i] + np.arange(n[i]) * vs for i in range(3)]
        far = 1.0
        d = np.full(n, far, dtype=np.float32)

        def block(plo, phi, margin):
            sl = []
            for i in range(3):
                a = max(int(np.floor((plo[i] - margin - lo[i]) / vs)), 0)
                b = min(int(np.ceil((phi[i] + margin - lo[i]) / vs)) + 1, n[i])
                if b <= a:
                    return None
                sl.append(slice(a, b))
            return tuple(sl)

        for op in self.ops:
            kind = op[0]
            if kind == "disp":
                sl = block(op[2], op[3], 0)
                if sl is None:
                    continue
                X, Y, Z = np.meshgrid(axes[0][sl[0]], axes[1][sl[1]], axes[2][sl[2]], indexing="ij", sparse=True)
                d[sl] += op[1](X, Y, Z).astype(np.float32)
                continue
            prim, k = op[1], op[2]
            margin = 3 * k + 6 * vs
            if kind == "int":
                sl = (slice(0, n[0]), slice(0, n[1]), slice(0, n[2]))
            else:
                sl = block(prim.lo, prim.hi, margin)
                if sl is None:
                    continue
            X, Y, Z = np.meshgrid(axes[0][sl[0]], axes[1][sl[1]], axes[2][sl[2]], indexing="ij", sparse=True)
            p = prim(X, Y, Z).astype(np.float32)
            p = np.broadcast_to(p, d[sl].shape)
            if kind == "add":
                d[sl] = smin(d[sl], p, k)
            elif kind == "sub":
                d[sl] = smax(d[sl], -p, k)
            else:
                d[sl] = smax(d[sl], p, k)
        return d, lo

    def mesh(self):
        """Returns (verts Nx3 float, quads Mx4 int)."""
        d, lo = self.sample()
        g = openvdb.FloatGrid(1.0)
        g.copyFromArray(np.clip(d, -0.05, 1.0))
        pts, tris, quads = g.convertToPolygons(isovalue=0.0, adaptivity=0.0)
        verts = lo + pts.astype(np.float64) * self.voxel
        return verts, quads, tris

    def eval(self, P):
        """Signed distance of the whole piece at points P (N x 3)."""
        X, Y, Z = P[:, 0], P[:, 1], P[:, 2]
        d = np.full(len(P), 1.0)
        for op in self.ops:
            if op[0] == "disp":
                d = d + op[1](X, Y, Z)
                continue
            p = op[1](X, Y, Z)
            if op[0] == "add":
                d = smin(d, p, op[2])
            elif op[0] == "sub":
                d = smax(d, -p, op[2])
            else:
                d = smax(d, p, op[2])
        return d

    def surface(self, P, direction, offset=0.0, reach=0.08):
        """Moves points P along `direction` onto the first surface they meet (+ offset):
        marches in small steps until inside, then bisects the last step."""
        return march(self.eval, P, direction, offset, reach)

    def bone_distances(self, P):
        """Per bone: min distance to its 'add' primitives, for points P (N x 3)."""
        X, Y, Z = P[:, 0], P[:, 1], P[:, 2]
        out = {}
        for op in self.ops:
            if op[0] != "add" or op[3] is None:
                continue
            dd = op[1](X, Y, Z)
            out[op[3]] = np.minimum(out[op[3]], dd) if op[3] in out else dd
        return out


def skin_weights(dists, falloff=0.012, keep=4):
    """dists: bone -> distances. Returns bone -> weights (soft, top `keep` per vertex, normalised)."""
    names = list(dists)
    D = np.stack([dists[b] for b in names], axis=1)
    D = np.maximum(D, 0.0)
    W = np.exp(-(D - D.min(axis=1, keepdims=True)) / falloff)
    if len(names) > keep:
        thr = -np.sort(-W, axis=1)[:, keep - 1:keep]
        W = np.where(W >= thr, W, 0.0)
    W /= W.sum(axis=1, keepdims=True)
    return {b: W[:, i] for i, b in enumerate(names)}


def march(evalf, P, direction, offset=0.0, reach=0.08, step=0.0015):
    P = np.atleast_2d(np.asarray(P, dtype=np.float64))
    dirn = np.asarray(direction, dtype=np.float64)
    dirn = dirn / np.linalg.norm(dirn)
    t = np.zeros(len(P))
    done = np.zeros(len(P), dtype=bool)
    while True:
        d = evalf(P + dirn * t[:, None]) - offset
        done |= d < 0
        if done.all() or (t >= reach).all():
            break
        t = np.where(done, t, t + np.clip(d * 0.8, step * 0.3, step * 4))
    lo, hi = np.maximum(t - step * 4, 0), t
    for _ in range(25):
        mid = (lo + hi) / 2
        inside = evalf(P + dirn * mid[:, None]) - offset < 0
        hi = np.where(inside, mid, hi)
        lo = np.where(inside, lo, mid)
    return P + dirn * lo[:, None]
