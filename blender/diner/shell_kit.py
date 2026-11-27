"""Profile sweeps and path helpers for the diner's shell (building.py): rounded stainless beads, frames, mouldings and rails as
swept cross-sections with their own metre UVs (u along the path, v along the profile), so brushed steel and mahogany grain
run along every bar.

A sweep walks a profile [(off, dz), ...] along a path P (3D points). Each path point carries two unit-ish vectors: N (the
`off` direction; mitred, so its length grows at a corner and every side keeps the same offset) and W (the `dz` direction).
A profile may be open (it is attached to a surface at off = 0: the solid is the profile closed by a straight line) or closed
(close_prof). The face orientation is worked out from the profile's winding, so list the profile in either direction.

Call C.ensure_uv(bm) before the first sweep (own UVs need the layer; adding one later invalidates earlier faces).
"""
import math

from mathutils import Vector

import diner_common as C


def face_uv(bm, pts, uvs, mi, want):
    """A face with its normal along want; uvs: one (u, v) per point (metres) or None (box projection at export)."""
    vs = [Vector(p) for p in pts]
    n = Vector((0.0, 0.0, 0.0))
    for i in range(len(vs)):
        a, b = vs[i], vs[(i + 1) % len(vs)]
        n += Vector(((a.y - b.y) * (a.z + b.z), (a.z - b.z) * (a.x + b.x), (a.x - b.x) * (a.y + b.y)))
    idx = list(range(len(vs)))
    if n.dot(Vector(want)) < 0:
        idx.reverse()
    f = bm.faces.new([bm.verts.new(vs[i]) for i in idx])
    f.material_index = mi
    if uvs is not None:
        lay = bm.loops.layers.uv.active
        for lp, i in zip(f.loops, idx):
            lp[lay].uv = uvs[i]
        f[bm.faces.layers.int.get("uvset")] = 1
    return f


def mitre_normals(P, closed, side):
    """Unit-offset normals at the points of the plan polyline P [(x, y)]: side +1 the right of the walk, -1 the left."""
    P = [Vector(p) for p in P]
    n = len(P)
    seg = []
    for i in range(n if closed else n - 1):
        d = (P[(i + 1) % n] - P[i]).normalized()
        seg.append(Vector((d.y, -d.x)) * side)
    out = []
    for i in range(n):
        if closed:
            a, b = seg[i - 1], seg[i]
        else:
            a = seg[i - 1] if i > 0 else None
            b = seg[i] if i < n - 1 else None
        m = b if a is None else (a if b is None else (a + b) / (1.0 + a.dot(b)))
        out.append(Vector((m.x, m.y, 0.0)))
    return out


def dedupe(P, eps=1e-5):
    out = [P[0]]
    for p in P[1:]:
        if (Vector(p) - Vector(out[-1])).length > eps:
            out.append(p)
    return out


def sweep(bm, P, N, W, prof, mat, closed=False, close_prof=False, caps=True, s0=0.0):
    """Sweeps prof along the 3D path P (see the module doc). Returns nothing; faces get material mat."""
    P = [Vector(p) for p in P]
    N = [Vector(v) for v in N]
    W = [Vector(v) for v in W]
    n, kk = len(P), len(prof)
    S = [0.0]
    for i in range(1, n):
        S.append(S[-1] + (P[i] - P[i - 1]).length)
    if closed:
        S.append(S[-1] + (P[0] - P[-1]).length)
    pv = [0.0]
    for k in range(1, kk):
        pv.append(pv[-1] + math.hypot(prof[k][0] - prof[k - 1][0], prof[k][1] - prof[k - 1][1]))
    if close_prof:
        pv.append(pv[-1] + math.hypot(prof[0][0] - prof[-1][0], prof[0][1] - prof[-1][1]))
    area = sum(prof[k][0] * prof[(k + 1) % kk][1] - prof[(k + 1) % kk][0] * prof[k][1] for k in range(kk))
    sgn = 1.0 if area > 0 else -1.0
    Q = [[P[i] + N[i] * prof[k][0] + W[i] * prof[k][1] for k in range(kk)] for i in range(n)]
    for i in range(n if closed else n - 1):
        j = (i + 1) % n
        if (P[j] - P[i]).length < 1e-7:
            continue
        Nu = ((N[i].normalized() + N[j].normalized()) / 2)
        Wu = ((W[i].normalized() + W[j].normalized()) / 2)
        for k in range(kk if close_prof else kk - 1):
            k2 = (k + 1) % kk
            dO, dZ = prof[k2][0] - prof[k][0], prof[k2][1] - prof[k][1]
            if math.hypot(dO, dZ) < 1e-7:
                continue
            want = Nu * (sgn * dZ) + Wu * (-sgn * dO)
            uvs = [(s0 + S[i], pv[k]), (s0 + S[i + 1], pv[k]), (s0 + S[i + 1], pv[k + 1]), (s0 + S[i], pv[k + 1])]
            face_uv(bm, [Q[i][k], Q[j][k], Q[j][k2], Q[i][k2]], uvs, mat, want)
    if caps and not closed and kk >= 3:
        t0 = (P[1] - P[0]).normalized()
        t1 = (P[-1] - P[-2]).normalized()
        face_uv(bm, Q[0], None, mat, -t0)
        face_uv(bm, Q[-1], None, mat, t1)


def sweep_plan(bm, pts2, z, prof, mat, side=1, closed=False, caps=True, s0=0.0):
    """A sweep along the plan polyline pts2 at height z; off runs to the right (side 1) or left (-1) of the walk, dz up."""
    pts2 = dedupe([Vector(p) for p in pts2])
    N = mitre_normals(pts2, closed, side)
    sweep(bm, [(p.x, p.y, z) for p in pts2], N, [(0, 0, 1)] * len(pts2), prof, mat, closed=closed, caps=caps, s0=s0)


def bead(r, nseg=6, off0=0.0):
    """A half-round bead profile (off, dz): radius r standing on the surface, from its bottom to its top."""
    return [(off0 + r * math.cos(-math.pi / 2 + math.pi * i / nseg), r * math.sin(-math.pi / 2 + math.pi * i / nseg)) for i in range(nseg + 1)]


def rrect(a0, a1, w0, w1, r, seg=3):
    """A closed rounded rectangle profile a0..a1 x w0..w1, corner radius r."""
    pts = []
    for cx, cy, a_start in ((a1 - r, w1 - r, 0.0), (a0 + r, w1 - r, 90.0), (a0 + r, w0 + r, 180.0), (a1 - r, w0 + r, 270.0)):
        for i in range(seg + 1):
            a = math.radians(a_start + 90.0 * i / seg)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


class Loop:
    """A closed plan polygon with arc-length parameter: walk(s0, s1) gives the sub-polyline between two arc lengths (s1 may run past
    the end: it wraps)."""

    def __init__(self, pts):
        self.pts = [Vector((p[0], p[1])) for p in pts]
        n = len(self.pts)
        self.cum = [0.0]
        for i in range(n):
            self.cum.append(self.cum[-1] + (self.pts[(i + 1) % n] - self.pts[i]).length)
        self.L = self.cum.pop()

    def at(self, s):
        n = len(self.pts)
        s = s % self.L
        for i in range(n):
            e = self.cum[i + 1] if i + 1 < n else self.L
            if s <= e + 1e-9:
                a, b = self.pts[i], self.pts[(i + 1) % n]
                t = (s - self.cum[i]) / max(e - self.cum[i], 1e-9)
                return a + (b - a) * min(max(t, 0.0), 1.0)
        return self.pts[0]

    def walk(self, s0, s1):
        n = len(self.pts)
        out = [self.at(s0)]
        for lap in (0, 1):
            for i in range(n):
                c = self.cum[i] + lap * self.L
                if s0 + 1e-6 < c < s1 - 1e-6:
                    out.append(self.pts[i])
        out.append(self.at(s1))
        return dedupe(out)

    def complement(self, blocks, wrap=True):
        """The arc-length intervals not covered by the blocks [(a, b)] (a < b within 0..L); a piece that runs through the end
        of the loop and on from its start is merged (wrap)."""
        bl = sorted((max(a, 0.0), min(b, self.L)) for a, b in blocks if b > a)
        merged = []
        for a, b in bl:
            if merged and a <= merged[-1][1] + 1e-6:
                merged[-1] = (merged[-1][0], max(merged[-1][1], b))
            else:
                merged.append((a, b))
        free = []
        pos = 0.0
        for a, b in merged:
            if a - pos > 1e-4:
                free.append((pos, a))
            pos = max(pos, b)
        if self.L - pos > 1e-4:
            free.append((pos, self.L))
        if wrap and len(free) > 1 and free[0][0] < 1e-6 and free[-1][1] > self.L - 1e-6:
            first = free.pop(0)
            last = free.pop()
            free.append((last[0], first[1] + self.L))
        return free


def pipe(bm, pts, r, mi, seg=10, bend=0.1, steps=5, caps=True):
    """A round tube along the polyline pts with rounded corners (radius ~ bend); own UVs (u round, v along)."""
    C.ensure_uv(bm)
    P = [Vector(p) for p in pts]
    path = [P[0]]
    for i in range(1, len(P) - 1):
        a, b, c = P[i - 1], P[i], P[i + 1]
        m = min(bend, (b - a).length * 0.45, (c - b).length * 0.45)
        s0 = b - (b - a).normalized() * m
        e0 = b + (c - b).normalized() * m
        for t in range(steps + 1):
            u = t / steps
            path.append(s0 * (1 - u) ** 2 + b * 2 * (1 - u) * u + e0 * u * u)
    path.append(P[-1])
    path = dedupe(path)
    tang = [(path[min(i + 1, len(path) - 1)] - path[max(i - 1, 0)]).normalized() for i in range(len(path))]
    up = Vector((0, 0, 1)) if abs(tang[0].z) < 0.95 else Vector((1, 0, 0))
    y = up.cross(tang[0]).normalized()
    rings, S = [], [0.0]
    for i, (p, t) in enumerate(zip(path, tang)):
        if i > 0:
            S.append(S[-1] + (p - path[i - 1]).length)
            y = (y - t * y.dot(t)).normalized()
        z = t.cross(y)
        rings.append([bm.verts.new(p + (y * math.cos(2 * math.pi * k / seg) + z * math.sin(2 * math.pi * k / seg)) * r) for k in range(seg)])
    ua = 2 * math.pi * r / seg
    for i in range(len(path) - 1):
        for k in range(seg):
            j = (k + 1) % seg
            f = bm.faces.new((rings[i][k], rings[i][j], rings[i + 1][j], rings[i + 1][k]))
            f.material_index = mi
            C.own_uv(bm, f, ((ua * k, S[i]), (ua * (k + 1), S[i]), (ua * (k + 1), S[i + 1]), (ua * k, S[i + 1])))
    if caps:
        bm.faces.new(rings[0][::-1]).material_index = mi
        bm.faces.new(rings[-1]).material_index = mi
