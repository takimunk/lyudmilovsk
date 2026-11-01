"""Mesh builder for the nest kit: faces carry their own atlas uvs, one lint part per twig / blob / shell.

All uvs are kept inside their atlas cell (1.5 texel inset). Tubes map u round, v along (never tiled: a long twig simply
spends the whole cell height on its length). Organic shells use a per-face planar projection, shifted by whole cells
(the cells tile, so the shift is invisible) when it fits, else dropped into a free spot of the cell.
"""
import math

import bmesh  # noqa: F401
import bpy
import numpy as np

from nest_atlas import CELLS, DENS, SIZE

INSET = 1.5


def nrm(v):
    n = np.linalg.norm(v)
    return v / n if n > 1e-12 else v


def A(*p):
    return np.array(p, float)


class MB:
    def __init__(self, rng):
        self.rng = rng
        self.v = []
        self.f = []          # (vids, uv|None, cell, part)
        self.nparts = 0
        self.names = {}
        self.T = None

    # ------------------------------------------------------------------ basics
    def vert(self, p):
        self.v.append(np.asarray(p, float))
        return len(self.v) - 1

    def part(self, name="p"):
        self.nparts += 1
        self.names[self.nparts] = name
        return self.nparts

    def face(self, vids, cell, part, uv=None):
        self.f.append((tuple(vids), uv, cell, part))

    def xform(self, fn):
        self.v = [np.asarray(fn(v), float) for v in self.v]

    def nfaces(self):
        return len(self.f)

    def orient(self, start, end, ref):
        """flip faces [start,end) whose normal disagrees with ref(face_centre) -> wanted direction"""
        for i in range(start, end):
            vids, uv, cell, part = self.f[i]
            P = np.array([self.v[k] for k in vids])
            n = np.zeros(3)
            for k in range(len(P)):
                a, b = P[k], P[(k + 1) % len(P)]
                n += np.cross(a, b)
            if np.dot(n, ref(P.mean(0))) < 0:
                self.f[i] = (vids[::-1], uv[::-1] if uv else None, cell, part)

    def orient_group(self, start, end, wanted):
        """flip the whole group if its first face does not point along `wanted` (a revolved profile winds consistently)"""
        vids = self.f[start][0]
        P = np.array([self.v[k] for k in vids])
        n = np.zeros(3)
        for k in range(len(P)):
            n += np.cross(P[k], P[(k + 1) % len(P)])
        if np.dot(n, wanted) < 0:
            for i in range(start, end):
                v, uv, cell, part = self.f[i]
                self.f[i] = (v[::-1], uv[::-1] if uv else None, cell, part)

    # ------------------------------------------------------------------ tube
    def tube(self, pts, r, cell, part, sides=5, flat=1.0, rfun=None, up=(0, 0, 1), cone=1.0, uvspan=None):
        pts = [np.asarray(self.T(p) if self.T else p, float) for p in pts]
        n = len(pts)
        d = []
        for i in range(n):
            d.append(nrm(pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]))
        up = np.asarray(up, float)
        e1 = np.cross(d[0], up)
        if np.linalg.norm(e1) < 1e-6:
            e1 = A(1, 0, 0) - d[0] * d[0][0]
        e1 = nrm(e1)
        rings, rad = [], []
        for i in range(n):
            if i:
                e1 = e1 - d[i] * np.dot(e1, d[i])
                e1 = nrm(e1) if np.linalg.norm(e1) > 1e-9 else nrm(np.cross(d[i], up))
            e2 = np.cross(d[i], e1)
            t = i / max(n - 1, 1)
            ri = r * (rfun(t) if rfun else (1.0 - 0.3 * t))
            if i in (0, n - 1):
                ri *= 0.8
            ring = [self.vert(pts[i] + ri * (math.cos(2 * math.pi * j / sides) * e1 + math.sin(2 * math.pi * j / sides) * e2 * flat))
                    for j in range(sides)]
            rings.append(ring)
            rad.append(ri)
        if flat < 0.5:
            cone = max(cone, 3.0)
        a0 = self.vert(pts[0] - d[0] * rad[0] * cone)
        a1 = self.vert(pts[-1] + d[-1] * rad[-1] * cone)
        # arc length for v
        s = [rad[0] * cone]
        for i in range(1, n):
            s.append(s[-1] + np.linalg.norm(pts[i] - pts[i - 1]))
        total = s[-1] + rad[-1] * cone
        x0, y0, cw, ch = CELLS[cell]
        dens = DENS[cell]
        cum = []
        for i in range(n):
            P = [self.v[k] for k in rings[i]]
            c = [0.0]
            for j in range(sides):
                c.append(c[-1] + float(np.linalg.norm(P[(j + 1) % sides] - P[j])) * 1000)
            cum.append(c)
        circ = max(c[-1] for c in cum)
        total = s[-1] + rad[-1] * cone
        su = dens
        sv = dens
        if total * 1000 * sv > ch - 2 * INSET - 2:
            sv = (ch - 2 * INSET - 2) / (total * 1000)
        if circ * su > cw - 2 * INSET - 2:
            su = (cw - 2 * INSET - 2) / circ
        rng = self.rng
        u0 = INSET + 1 + rng.random() * max(0.0, cw - 2 * INSET - 2 - circ * su)
        v0 = INSET + 1 + rng.random() * max(0.0, ch - 2 * INSET - 2 - total * 1000 * sv)

        def uvp(j, i):
            return (u0 + cum[i][j] * su, v0 + s[i] * 1000 * sv)
        for i in range(n - 1):
            for j in range(sides):
                j2 = (j + 1) % sides
                self.face((rings[i][j], rings[i][j2], rings[i + 1][j2], rings[i + 1][j]), cell, part,
                          [uvp(j, i), uvp(j + 1, i), uvp(j + 1, i + 1), uvp(j, i + 1)])
        for j in range(sides):
            j2 = (j + 1) % sides
            um0 = (uvp(j, 0)[0] + uvp(j + 1, 0)[0]) / 2
            um1 = (uvp(j, n - 1)[0] + uvp(j + 1, n - 1)[0]) / 2
            self.face((a0, rings[0][j2], rings[0][j]), cell, part, [(um0, v0), uvp(j + 1, 0), uvp(j, 0)])
            self.face((a1, rings[-1][j], rings[-1][j2]), cell, part, [(um1, v0 + total * 1000 * sv), uvp(j, n - 1), uvp(j + 1, n - 1)])

    # ------------------------------------------------------------------ blob
    def blob(self, c, rad, cell, part, rough=0.25, frame=None, squash=1.0, subdiv=1):
        rng = self.rng
        base = [A(1, 0, 0), A(-1, 0, 0), A(0, 1, 0), A(0, -1, 0), A(0, 0, 1), A(0, 0, -1)]
        tris = [(0, 2, 4), (2, 1, 4), (1, 3, 4), (3, 0, 4), (2, 0, 5), (1, 2, 5), (3, 1, 5), (0, 3, 5)]
        vs = list(base)
        mid = {}

        def m(a, b):
            k = (min(a, b), max(a, b))
            if k not in mid:
                vs.append(nrm(vs[a] + vs[b]))
                mid[k] = len(vs) - 1
            return mid[k]
        faces = list(tris)
        for _ in range(subdiv):
            nf = []
            for a, b, cc in faces:
                ab, bc, ca = m(a, b), m(b, cc), m(cc, a)
                nf += [(a, ab, ca), (ab, b, bc), (ca, bc, cc), (ab, bc, ca)]
            faces = nf
        fr = frame if frame is not None else np.eye(3)
        rad = np.asarray(rad, float) * np.array([1, 1, squash])
        ids = []
        for v in vs:
            k = 1 + rough * rng.uniform(-1, 1)
            q = np.asarray(c) + fr @ (v * rad * k)
            ids.append(self.vert(self.T(q) if self.T else q))
        for f in faces:
            self.face([ids[k] for k in f], cell, part, None)
        return ids

    # ------------------------------------------------------------------ revolved shell
    def ring_pts(self, rho, z, n, closed=True, ringfn=None, phase=0.0, noise=None):
        if ringfn:
            return ringfn(rho, z, n)
        out = []
        for k in range(n):
            a = phase + 2 * math.pi * k / n
            q = rho * (1 + (noise[k] if noise is not None else 0))
            out.append(A(q * math.cos(a), q * math.sin(a), z))
        return out

    def revolve(self, prof, n, cell, part, amp=0.0, phase=0.0, ringfn=None, closed=True, wavy=0.0):
        """prof [(rho, z)], first may be rho=0 (apex). Faces wind so the normal points to the right of the profile
        direction (outside when walking bottom-centre -> up the outer wall); use orient() afterwards to be sure."""
        rng = self.rng
        rings = []
        for i, (rho, z) in enumerate(prof):
            if rho < 1e-9 and not ringfn:
                rings.append([self.vert(self.T(A(0, 0, z)) if self.T else (0, 0, z))])
                continue
            nz_ = (rng.normal(0, amp, n) if amp else None)
            pts = self.ring_pts(rho, z + (rng.normal(0, wavy) if wavy else 0), n, closed, ringfn, phase, nz_)
            rings.append([self.vert(self.T(p) if self.T else p) for p in pts])
        start = len(self.f)
        m = n if closed else n - 1
        for i in range(len(rings) - 1):
            A_, B_ = rings[i], rings[i + 1]
            for k in range(m):
                k2 = (k + 1) % n
                if len(A_) == 1:
                    self.face((A_[0], B_[k2], B_[k]), cell, part)
                elif len(B_) == 1:
                    self.face((A_[k], A_[k2], B_[0]), cell, part)
                else:
                    self.face((A_[k], A_[k2], B_[k2], B_[k]), cell, part)
        return rings, start

    def bridge(self, la, lb, cell, part, closed=True):
        n = len(la)
        m = n if closed else n - 1
        for k in range(m):
            k2 = (k + 1) % n
            self.face((la[k], la[k2], lb[k2], lb[k]), cell, part)

    # ------------------------------------------------------------------ finish
    def _planar_uv(self, P, cell):
        x0, y0, cw, ch = CELLS[cell]
        dens = DENS[cell]
        n = np.zeros(3)
        for k in range(len(P)):
            n += np.cross(P[k], P[(k + 1) % len(P)])
        n = nrm(n)
        a = A(0, 0, 1) if abs(n[2]) < 0.9 else A(1, 0, 0)
        ua = nrm(np.cross(a, n))
        va = np.cross(n, ua)
        uv = np.array([[np.dot(p, ua), np.dot(p, va)] for p in P]) * 1000 * dens
        return uv

    def _fit(self, uv, cell, face_idx):
        x0, y0, cw, ch = CELLS[cell]
        lo = INSET + 0.5
        sz = np.array([cw, ch], float)
        span = uv.max(0) - uv.min(0)
        room = sz - 2 * lo
        f = min(1.0, float(np.min((room - 0.5) / np.maximum(span, 1e-6))))
        uv = uv * f if f < 1 else uv
        span = span * f
        out = uv.copy()
        # integer shifts by whole cells (the cell tiles) so the picture continues across neighbouring faces
        for ax in range(2):
            mn = out[:, ax].min() + self._origin[ax]
            sh = None
            for kk in range(-40, 41):
                st = mn + kk * sz[ax]
                if lo <= st <= sz[ax] - lo - span[ax]:
                    sh = kk * sz[ax]
                    break
            if sh is None:
                st = lo + self.rng.random() * max(0.0, room[ax] - span[ax])
                sh = st - mn
            out[:, ax] += sh + self._origin[ax]
        return out

    def to_mesh(self, name, mat=None):
        vs = [tuple(v) for v in self.v]
        fs = [f[0] for f in self.f]
        me = bpy.data.meshes.new(name)
        me.from_pydata(vs, [], fs)
        me.update()
        uvl = me.uv_layers.new(name="UVMap")
        flat = np.zeros((len(me.loops), 2))
        self._origin = (0.0, 0.0)
        li = 0
        planar, maxspan = {}, {}
        for idx, (vids, uv, cell, part) in enumerate(self.f):
            if uv is None:
                planar[idx] = self._planar_uv(np.array([self.v[k] for k in vids]), cell)
                key = (part, cell)
                maxspan[key] = max(maxspan.get(key, 0.0), float((planar[idx].max(0) - planar[idx].min(0)).max()))
        for idx, (vids, uv, cell, part) in enumerate(self.f):
            x0, y0, cw, ch = CELLS[cell]
            if uv is None:
                room = min(cw, ch) - 2 * (INSET + 0.5) - 1
                fct = min(1.0, room / max(maxspan[(part, cell)], 1e-6))
                uvp = self._fit(planar[idx] * fct, cell, idx)
            else:
                uvp = np.array(uv, float)
                uvp[:, 0] = np.clip(uvp[:, 0], INSET + 0.5, cw - INSET - 0.5)
                uvp[:, 1] = np.clip(uvp[:, 1], INSET + 0.5, ch - INSET - 0.5)
            flat[li:li + len(vids)] = np.stack([(x0 + uvp[:, 0]) / SIZE, (y0 + uvp[:, 1]) / SIZE], 1)
            li += len(vids)
        uvl.data.foreach_set("uv", flat.ravel())
        pa = me.attributes.new("lint_part", "INT", "FACE")
        pa.data.foreach_set("value", np.array([f[3] for f in self.f], int))
        me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
        me.update()
        ob = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(ob)
        if mat is not None:
            me.materials.append(mat)
        return ob
