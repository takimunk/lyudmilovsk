"""Geometry helpers for the North props: a bmesh Builder with trim-sheet UVs (northtex.TILES), primitives (boxes,
tapered cylinders, tubes along curves, lathes, stones, cloth shells), the material set and the GLB export.

Conventions: metres, Z up, Blender -Y is the front, origin on the ground under the middle, transforms applied.
"""
import math
import os
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

import northtex as T

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
GEN = os.path.join(HERE, "gen")
EXPORT = os.path.join(ROOT, "blender", "export", "north")

# physical size (metres) one tile window covers for the trim-sheet mapping, per tile (default 1.0)
TILE_M = {"canvas_khaki": 3.0, "canvas_green": 3.0, "nylon_orange": 3.0, "nylon_bluegrey": 3.0, "nylon_olive": 3.0,
          "nylon_yellow": 3.0, "tarp_blue": 4.5, "stone": 0.8, "stone_soot": 0.8, "metal": 0.6, "wood": 1.2,
          "wood_dark": 1.2, "bark": 2.4, "rope": 0.12, "ash": 1.0, "char": 0.5, "stripes": 4.2, "knit": 0.25,
          "plaid": 0.5, "towel": 0.5, "mat": 0.5, "label": 0.1, "plank_edge": 0.8, "endgrain": 0.35}


def V(*a):
    return Vector(a)


class Builder:
    """Collects faces with a material index and UVs into one bmesh."""

    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UV0")
        self.mat_slots = ["M_North_Atlas"]       # slot 0 is always the atlas
        self.rnd = random.Random(name)

    def slot(self, matname):
        if matname not in self.mat_slots:
            self.mat_slots.append(matname)
        return self.mat_slots.index(matname)

    # ---- raw face --------------------------------------------------------------------------------------------
    def face(self, pts, uvs, smooth=False, mat=0):
        vs = [self.bm.verts.new(p) for p in pts]
        try:
            f = self.bm.faces.new(vs)
        except ValueError:
            return None
        for lp, uv in zip(f.loops, uvs):
            lp[self.uv].uv = uv
        f.smooth = smooth
        f.material_index = mat
        return f

    # ---- UV helpers ------------------------------------------------------------------------------------------
    @staticmethod
    def win(tile, off=(0.0, 0.0)):
        u0, v0, u1, v1 = T.rect_uv(tile)
        return u0, v0, u1 - u0, v1 - v0

    def mapuv(self, tile, a, b, la, lb, off=None, aniso=False):
        """UV for plane coords (a, b) in metres relative to the face's own origin; la, lb = the face's extent.
        Detail stays at a fixed metres-per-pixel unless the face is bigger than the tile (then it is compressed
        uniformly; wood-like tiles (aniso) compress only along U)."""
        u0, v0, du, dv = self.win(tile)
        m = TILE_M.get(tile, 1.0)
        su = min(1.0, m / max(la, 1e-6)) if True else 1.0
        sv = min(1.0, m / max(lb, 1e-6))
        if not aniso:
            su = sv = min(su, sv)
        else:
            sv = 1.0
        if off is None:
            off = (self.rnd.random(), self.rnd.random())
        # offset the face's origin inside the window so each part differs, keeping it inside
        spanu, spanv = la / m * su, lb / m * sv
        ou = off[0] * max(0.0, 1.0 - spanu)
        ov = off[1] * max(0.0, 1.0 - spanv)
        return (u0 + (ou + a / m * su) * du, v0 + (ov + b / m * sv) * dv)

    # ---- primitives ------------------------------------------------------------------------------------------
    def box(self, c, size, tile="wood", rot=None, tiles=None, smooth=False, grain=None, mat=0, bevel=0.0, flatc=None):
        """Axis box of size (x, y, z) centred at c, rotated by Matrix `rot` about its centre. tile may be per face
        group via tiles = {'top','bottom','side'|'x'|'y'...: tile}. grain = axis index (0,1,2) the U runs along."""
        sx, sy, sz = size[0] / 2, size[1] / 2, size[2] / 2
        rot = rot if rot is not None else Matrix.Identity(3)
        if grain is None:
            grain = max(range(3), key=lambda i: size[i])
        corners = {}
        for ix in (-1, 1):
            for iy in (-1, 1):
                for iz in (-1, 1):
                    corners[(ix, iy, iz)] = (ix * sx, iy * sy, iz * sz)

        def P(k):
            return c + rot @ Vector(corners[k])
        faces = [  # (normal axis, sign, corners in CCW order seen from outside)
            (0, 1, [(1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1)]),
            (0, -1, [(-1, 1, -1), (-1, -1, -1), (-1, -1, 1), (-1, 1, 1)]),
            (1, 1, [(1, 1, -1), (-1, 1, -1), (-1, 1, 1), (1, 1, 1)]),
            (1, -1, [(-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)]),
            (2, 1, [(-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]),
            (2, -1, [(-1, 1, -1), (1, 1, -1), (1, -1, -1), (-1, -1, -1)]),
        ]
        for axis, sign, ks in faces:
            others = [i for i in range(3) if i != axis]
            ua = grain if grain in others else others[0]
            va = [i for i in others if i != ua][0]
            t = tile
            if tiles:
                nm = "top" if (axis == 2 and sign > 0) else "bottom" if (axis == 2) else "side"
                nm2 = "xy"[axis] if axis < 2 else nm
                t = tiles.get(nm2, tiles.get(nm, tiles.get("all", tile)))
            la, lb = size[ua] , size[va]
            off = (self.rnd.random(), self.rnd.random())
            uvs = []
            for k in ks:
                p = Vector(corners[k])
                uvs.append(self.mapuv(t, p[ua] + size[ua] / 2, p[va] + size[va] / 2, la, lb, off, aniso=t in ("wood", "wood_dark", "plank_edge", "bark", "stripes")))
            if t == "endgrain" or t == "swatch":
                uvs = [self.endgrain_uv(Vector(corners[k]), ua, va, size) for k in ks] if t == "endgrain" else uvs
            if flatc:
                uvs = [T.swatch_uv(flatc)] * 4
            self.face([P(k) for k in ks], uvs, smooth=smooth, mat=mat)

    def endgrain_uv(self, p, ua, va, size):
        u0, v0, du, dv = self.win("endgrain")
        r = max(size[ua], size[va]) / 2
        return (u0 + (0.5 + p[ua] / (2 * r) * 0.92) * du, v0 + (0.5 + p[va] / (2 * r) * 0.92) * dv)

    def flat(self, pts, color, smooth=False, mat=0):
        """A polygon in a flat swatch colour."""
        uv = T.swatch_uv(color)
        self.face(pts, [uv] * len(pts), smooth=smooth, mat=mat)

    def cyl(self, p0, p1, r0, r1=None, seg=8, tile="wood", caps=True, cap_tile=None, smooth=True, sides_tile=None, wobble=0.0,
            turns=1.0, cap0=True, cap1=True, mat=0, aspect=1.0, flatc=None):
        """Tapered cylinder from p0 to p1. U along the axis, V around. caps use cap_tile (endgrain by default for wood)."""
        p0, p1 = Vector(p0), Vector(p1)
        r1 = r0 if r1 is None else r1
        ax = p1 - p0
        L = ax.length
        z = ax.normalized()
        h = Vector((0, 0, 1)) if abs(z.z) < 0.95 else Vector((1, 0, 0))
        x = z.cross(h).normalized()
        y = z.cross(x).normalized()
        m = TILE_M.get(tile, 1.0)
        circ = 2 * math.pi * max(r0, r1) * 0.5
        u0, v0, du, dv = self.win(tile)
        sc = min(1.0, m / max(L, 1e-6))
        off = (self.rnd.random(), self.rnd.random())
        rings = []
        for i in range(seg):
            a = 2 * math.pi * i / seg
            w = 1.0 + (self.rnd.uniform(-wobble, wobble) if wobble else 0.0)
            rings.append((a, w))
        cs = lambda a, r, w: (x * math.cos(a) * r * w + y * math.sin(a) * r * w * aspect)
        for i in range(seg):
            a0, w0 = rings[i]
            a1, w1 = rings[(i + 1) % seg]
            q = [p0 + cs(a0, r0, w0), p0 + cs(a1, r0, w1), p1 + cs(a1, r1, w1), p1 + cs(a0, r1, w0)]
            ua = off[0] * max(0, 1 - sc)
            va0 = off[1] + (i / seg) * circ * 2 / m
            va1 = off[1] + ((i + 1) / seg) * circ * 2 / m
            va0 = (va0 % 1.0) * 0.95
            va1 = va0 + (circ * 2 / seg / m)
            va1 = min(va1, 0.99)
            uvs = [(u0 + (ua + 0.0) * du, v0 + va0 * dv), (u0 + (ua + 0.0) * du, v0 + va1 * dv),
                   (u0 + (ua + L / m * sc) * du, v0 + va1 * dv), (u0 + (ua + L / m * sc) * du, v0 + va0 * dv)]
            if flatc:
                uvs = [T.swatch_uv(flatc)] * 4
            self.face(q, uvs, smooth=smooth, mat=mat)
        ct = cap_tile or ("endgrain" if tile in ("wood", "wood_dark", "bark", "plank_edge") else tile)
        for end, (p, r, flip, on) in enumerate(((p0, r0, True, cap0), (p1, r1, False, cap1))):
            if not on or r <= 1e-5:
                continue
            ring = [p + cs(rings[i][0], r, rings[i][1]) for i in range(seg)]
            if flip:
                ring = ring[::-1]
            uvs = []
            for q in ring:
                d = q - p
                lu, lv = d.dot(x), d.dot(y)
                if ct == "endgrain":
                    rr = max(r, 1e-4)
                    uu, vv = 0.5 + lu / (2 * rr) * 0.9, 0.5 + lv / (2 * rr) * 0.9
                    e0, e1, edu, edv = self.win("endgrain")
                    uvs.append((e0 + uu * edu, e1 + vv * edv))
                else:
                    uu, vv = lu / m + 0.5, lv / m + 0.5
                    u00, v00, du2, dv2 = self.win(ct)
                    uvs.append((u00 + (uu % 1.0) * 0.9 * du2, v00 + (vv % 1.0) * 0.9 * dv2))
            if flatc:
                uvs = [T.swatch_uv(flatc)] * len(ring)
            self.face(ring, uvs, smooth=False, mat=mat)

    def tube(self, pts, r, seg=5, tile="rope", caps=True, smooth=True, rscale=None, mat=0, flatc=None, aspect=1.0):
        """A round tube along a polyline (parallel-transport frame). rscale(i, n) scales the radius per point."""
        pts = [Vector(p) for p in pts]
        n = len(pts)
        tans = []
        for i in range(n):
            a = pts[max(0, i - 1)]
            b = pts[min(n - 1, i + 1)]
            tans.append((b - a).normalized())
        ref = Vector((0, 0, 1)) if abs(tans[0].z) < 0.9 else Vector((1, 0, 0))
        nrm = tans[0].cross(ref).normalized()
        frames = []
        for i in range(n):
            t = tans[i]
            nrm = (nrm - t * nrm.dot(t))
            if nrm.length < 1e-6:
                nrm = t.cross(Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((1, 0, 0)))
            nrm = nrm.normalized()
            frames.append((nrm, t.cross(nrm).normalized()))
        m = TILE_M.get(tile, 1.0)
        u0, v0, du, dv = self.win(tile)
        lens = [0.0]
        for i in range(1, n):
            lens.append(lens[-1] + (pts[i] - pts[i - 1]).length)
        total = lens[-1]
        rings = []
        for i in range(n):
            rr = r * (rscale(i, n) if rscale else 1.0)
            nrm_, bin_ = frames[i]
            rings.append([pts[i] + nrm_ * math.cos(2 * math.pi * k / seg) * rr + bin_ * math.sin(2 * math.pi * k / seg) * rr * aspect for k in range(seg)])
        sc = min(1.0, m / max(total, 1e-6)) if tile not in ("rope",) else 1.0
        for i in range(n - 1):
            for k in range(seg):
                k2 = (k + 1) % seg
                q = [rings[i][k], rings[i][k2], rings[i + 1][k2], rings[i + 1][k]]
                ua, ub = (lens[i] / m * sc) % 1.0, (lens[i] / m * sc) % 1.0 + (lens[i + 1] - lens[i]) / m * sc
                ub = min(ub, 1.0)
                va, vb = k / seg * 0.95, (k + 1) / seg * 0.95
                uvs = [(u0 + ua * du, v0 + va * dv), (u0 + ua * du, v0 + vb * dv), (u0 + ub * du, v0 + vb * dv), (u0 + ub * du, v0 + va * dv)]
                if flatc:
                    uvs = [T.swatch_uv(flatc)] * 4
                self.face(q, uvs, smooth=smooth, mat=mat)
        if caps:
            for idx, flip in ((0, True), (n - 1, False)):
                ring = rings[idx][::-1] if flip else rings[idx]
                uv = [T.swatch_uv(flatc) if flatc else (u0 + 0.5 * du, v0 + 0.5 * dv)] * len(ring)
                self.face(ring, uv, mat=mat)

    def lathe(self, profile, seg=12, center=(0, 0, 0), tile="metal", smooth=True, rot=None, mat=0, flatc=None):
        """Surface of revolution round the local Z axis through `center`. profile = [(radius, z)...] bottom to top
        (outward normals when the profile climbs counter-clockwise seen from the axis side)."""
        cx = Vector(center)
        rot = rot if rot is not None else Matrix.Identity(3)
        u0, v0, du, dv = self.win(tile)
        z0, z1 = profile[0][1], profile[-1][1]
        P = lambda r, z, a: cx + rot @ Vector((math.cos(a) * r, math.sin(a) * r, z))
        for j in range(len(profile) - 1):
            (ra, za), (rb, zb) = profile[j], profile[j + 1]
            for i in range(seg):
                a0, a1 = 2 * math.pi * i / seg, 2 * math.pi * (i + 1) / seg
                ua0, ua1 = 0.05 + 0.9 * i / seg, 0.05 + 0.9 * (i + 1) / seg
                va = 0.05 + 0.9 * (za - z0) / max(1e-6, z1 - z0)
                vb = 0.05 + 0.9 * (zb - z0) / max(1e-6, z1 - z0)
                q = [P(ra, za, a0), P(rb, zb, a0), P(rb, zb, a1), P(ra, za, a1)]
                uv = [(ua0, va), (ua0, vb), (ua1, vb), (ua1, va)]
                if ra < 1e-6:
                    q, uv = [q[0], q[2], q[3]][::1], [uv[0], uv[2], uv[3]]
                    q = [P(ra, za, a0), P(rb, zb, a1), P(rb, zb, a0)][::-1] if False else [P(ra, za, a0), P(rb, zb, a0), P(rb, zb, a1)]
                    uv = [uv[0], (ua0, vb), (ua1, vb)]
                elif rb < 1e-6:
                    q = [P(ra, za, a0), P(rb, zb, a0), P(ra, za, a1)]
                    uv = [uv[0], (ua0, vb), uv[3]]
                n = (q[1] - q[0]).cross(q[2] - q[0])
                mid = sum(q, Vector()) / len(q)
                nrm = (mid - cx)
                # outward test: radial for side faces, axial (up on the top, down on the bottom) for end fans
                if ra < 1e-6:
                    want = Vector((0, 0, -1))
                elif rb < 1e-6:
                    want = Vector((0, 0, 1))
                else:
                    want = Vector((nrm.x, nrm.y, 0)) if abs(rb - ra) < 1e-9 or True else nrm
                    want = (rot @ Vector((math.cos((a0 + a1) / 2), math.sin((a0 + a1) / 2), 0)))
                if n.dot(want) < 0:
                    q, uv = q[::-1], uv[::-1]
                if flatc:
                    uv = [T.swatch_uv(flatc)] * len(q)
                else:
                    uv = [(u0 + a * du, v0 + b * dv) for a, b in uv]
                self.face(q, uv, smooth=smooth, mat=mat)

    def stone(self, c, size, tile="stone", seed=None, subdiv=2, rot=None, flatten=0.0, rough=0.12):
        """A rounded field stone: a displaced icosphere (80 tris at subdiv 1), flatter base, smooth-shaded."""
        r = random.Random(seed if seed is not None else self.rnd.random())
        tmp = bmesh.new()
        bmesh.ops.create_icosphere(tmp, subdivisions=subdiv, radius=0.5)
        sx, sy, sz = size
        ph = [r.uniform(0, 6.28) for _ in range(6)]
        rotm = rot if rot is not None else Matrix.Rotation(r.uniform(0, 6.28), 3, "Z")
        for v in tmp.verts:
            d = v.co.normalized()
            k = 1.0 + rough * (math.sin(3 * d.x + ph[0]) * math.sin(2.3 * d.y + ph[1]) + 0.6 * math.sin(4 * d.z + ph[2]))
            v.co = Vector((d.x * 0.5 * sx * k, d.y * 0.5 * sy * k, d.z * 0.5 * sz * k))
            if v.co.z < -0.15 * sz and flatten:
                v.co.z = -0.15 * sz - (v.co.z + 0.15 * sz) * (1 - flatten)
        u0, v0, du, dv = self.win(tile)
        m = TILE_M.get(tile, 1.0)
        off = (r.random() * 0.4, r.random() * 0.4)
        for f in tmp.faces:
            pts = [c + rotm @ v.co for v in f.verts]
            n = f.normal
            ax = max(range(3), key=lambda i: abs(n[i]))
            ia, ib = [i for i in range(3) if i != ax]
            raw = [(off[0] + (v.co[ia] / m) * 0.8 + 0.5, off[1] + (v.co[ib] / m) * 0.8 + 0.5) for v in f.verts]
            cu = sum(r_[0] for r_ in raw) / len(raw)
            cv = sum(r_[1] for r_ in raw) / len(raw)
            su, sv = math.floor(cu), math.floor(cv)
            uvs = [(u0 + (r_[0] - su) * du * 0.96 + 0.02 * du, v0 + (r_[1] - sv) * dv * 0.96 + 0.02 * dv) for r_ in raw]
            nf = self.face(pts, uvs, smooth=True)
        tmp.free()

    def polyprism(self, outline, z0, z1, tile="wood", frame=None, smooth_sides=False, flip=False, uvmode="wood", mat=0, front_mat=None,
                  cap_tile=None, cap_uv=None, side_tile=None, grain_u=True):
        """Prism from a convex-ish 2D outline [(x, y)...] CCW, between z0..z1 (z in `frame` axis: the outline lies in
        the XY plane of `frame` = Matrix(3x3) + offset origin). Caps are triangulated as fans from the centroid."""
        # kept simple: handled by prism_xz below for the shapes we need
        raise NotImplementedError

    def prism(self, outline, p, axes, depth, tile="wood", side_tile=None, cap_tile=None, smooth=False, cap_uv="plane", mat=0,
              cap_mat=None, back_tile=None):
        """Extrude a convex outline (list of (a, b) in plane coords, CCW seen from +n) along n for `depth` (centred).
        axes = (ea, eb, n) orthonormal vectors; p = centre. Cap UV: the outline's bounding box in the cap tile."""
        ea, eb, n = axes
        st = side_tile if (side_tile and not callable(side_tile)) else tile
        ct = cap_tile or tile
        cm = mat if cap_mat is None else cap_mat
        Pf = lambda a, b, s: p + ea * a + eb * b + n * (s * depth / 2)
        k = len(outline)
        xs = [q[0] for q in outline]
        ys = [q[1] for q in outline]
        la, lb = max(xs) - min(xs), max(ys) - min(ys)
        c = (sum(xs) / k, sum(ys) / k)
        off = (self.rnd.random(), self.rnd.random())

        def capuv(tl, a, b):
            if tl == "endgrain":
                return self.endgrain_uv(Vector((a - c[0], b - c[1], 0)), 0, 1, (la, lb, 0))
            return self.mapuv(tl, a - min(xs), b - min(ys), la, lb, off, aniso=tl in ("wood", "wood_dark", "plank_edge"))
        front = [Pf(a, b, 1) for a, b in outline]
        back = [Pf(a, b, -1) for a, b in outline][::-1]
        if cap_uv == "flat":
            fu = [T.swatch_uv(ct)] * k
            bu = fu
        else:
            fu = [capuv(ct, a, b) for a, b in outline]
            bu = [capuv(back_tile or ct, a, b) for a, b in outline][::-1]
        if k <= 4:
            self.face(front, fu, mat=cm)
            self.face(back, bu, mat=cm)
        else:
            self._fan(front, fu, cm)
            self._fan(back, bu, cm)
        for i in range(k):
            sti = side_tile(i) if callable(side_tile) else st
            u0, v0, du, dv = self.win(sti)
            m = TILE_M.get(sti, 1.0)
            (a0, b0), (a1, b1) = outline[i], outline[(i + 1) % k]
            w = math.hypot(a1 - a0, b1 - b0)
            q = [Pf(a0, b0, -1), Pf(a1, b1, -1), Pf(a1, b1, 1), Pf(a0, b0, 1)]
            o = (self.rnd.random() * 0.5, self.rnd.random() * 0.5)
            s = min(1.0, m / max(w, depth, 1e-6))
            uvs = [(u0 + (o[0] + 0) * du, v0 + (o[1] + 0) * dv), (u0 + (o[0] + w / m * s) * du, v0 + (o[1]) * dv),
                   (u0 + (o[0] + w / m * s) * du, v0 + (o[1] + depth / m * s) * dv), (u0 + o[0] * du, v0 + (o[1] + depth / m * s) * dv)]
            self.face(q, uvs, smooth=smooth, mat=mat)

    def _fan(self, pts, uvs, mat):
        k = len(pts)
        cp = sum(pts, Vector()) / k
        cu = (sum(u[0] for u in uvs) / k, sum(u[1] for u in uvs) / k)
        for i in range(k):
            self.face([pts[i], pts[(i + 1) % k], cp], [uvs[i], uvs[(i + 1) % k], cu], mat=mat)

    def grid(self, fn, us, vs, tile="canvas_khaki", thick=0.004, smooth=True, skip=None, inner_tile=None, inner_flat=None,
             wrap_u=False, ntile=None, mat=0, closed_ends=True, scale_uv=1.0, off=None):
        """A cloth shell: fn(u, v) -> point for u in us, v in vs; thickness `thick` along the surface normal (both skins and
        a rim). skip(iu, iv) -> True drops that cell. inner skin uses inner_tile / inner_flat. Returns the sampled points."""
        nu, nv = len(us), len(vs)
        P = [[Vector(fn(u, v)) for v in vs] for u in us]

        def nrm(i, j):
            i0, i1 = (i - 1) % nu if wrap_u else max(0, i - 1), (i + 1) % nu if wrap_u else min(nu - 1, i + 1)
            j0, j1 = max(0, j - 1), min(nv - 1, j + 1)
            du_ = P[i1][j] - P[i0][j]
            dv_ = P[i][j1] - P[i][j0]
            n = du_.cross(dv_)
            return n.normalized() if n.length > 1e-12 else Vector((0, 0, 1))
        N = [[nrm(i, j) for j in range(nv)] for i in range(nu)]
        # lengths for UV
        lu = [0.0]
        for i in range(1, nu):
            lu.append(lu[-1] + sum((P[i][j] - P[i - 1][j]).length for j in range(nv)) / nv)
        lv = [0.0]
        for j in range(1, nv):
            lv.append(lv[-1] + sum((P[i][j] - P[i][j - 1]).length for i in range(nu)) / nu)
        la, lb = lu[-1], lv[-1]
        u0, v0, du, dv = self.win(tile)
        m = TILE_M.get(tile, 1.0)
        s = min(1.0, m / max(la, 1e-6), m / max(lb, 1e-6)) * scale_uv
        off = off or (self.rnd.random(), self.rnd.random())
        ou = off[0] * max(0.0, 1.0 - la / m * s)
        ov = off[1] * max(0.0, 1.0 - lb / m * s)
        UV = lambda i, j: (u0 + (ou + lu[i] / m * s) * du, v0 + (ov + lv[j] / m * s) * dv)
        iu0, iv0, idu, idv = self.win(inner_tile) if inner_tile else (0, 0, 0, 0)
        iuv = (lambda i, j: (iu0 + (0.1 + 0.8 * (lu[i] / max(la, 1e-6))) * idu, iv0 + (0.1 + 0.8 * (lv[j] / max(lb, 1e-6))) * idv)) if inner_tile else None
        ni = nu if wrap_u else nu - 1
        cells = set()
        for i in range(ni):
            for j in range(nv - 1):
                if skip and skip(i, j):
                    continue
                cells.add((i, j))
                i2 = (i + 1) % nu
                outer = [P[i][j] + N[i][j] * thick / 2, P[i2][j] + N[i2][j] * thick / 2,
                         P[i2][j + 1] + N[i2][j + 1] * thick / 2, P[i][j + 1] + N[i][j + 1] * thick / 2]
                ouv = [UV(i, j), UV(i2 if not (wrap_u and i2 == 0) else nu - 1 + 0, j), UV(i2 if not (wrap_u and i2 == 0) else nu - 1, j + 1), UV(i, j + 1)]
                if wrap_u and i2 == 0:
                    ouv = [UV(i, j), (UV(nu - 1, j)[0] + (lu[1] - lu[0]) / m * s * du, UV(nu - 1, j)[1]), (UV(nu - 1, j + 1)[0] + (lu[1] - lu[0]) / m * s * du, UV(nu - 1, j + 1)[1]), UV(i, j + 1)]
                self.face(outer, ouv, smooth=smooth, mat=mat)
                inner = [P[i][j] - N[i][j] * thick / 2, P[i][j + 1] - N[i][j + 1] * thick / 2,
                         P[i2][j + 1] - N[i2][j + 1] * thick / 2, P[i2][j] - N[i2][j] * thick / 2]
                if inner_flat:
                    iuvs = [T.swatch_uv(inner_flat)] * 4
                elif iuv:
                    iuvs = [iuv(i, j), iuv(i, j + 1), iuv(i2, j + 1), iuv(i2, j)]
                else:
                    iuvs = [ouv[0], ouv[3], ouv[2], ouv[1]]
                self.face(inner, iuvs, smooth=smooth, mat=mat)
        # rim faces on every boundary edge of the kept cells
        if closed_ends:
            def edge(a, b, swap):
                (ia, ja), (ib, jb) = a, b
                p0o, p1o = P[ia][ja] + N[ia][ja] * thick / 2, P[ib][jb] + N[ib][jb] * thick / 2
                p0i, p1i = P[ia][ja] - N[ia][ja] * thick / 2, P[ib][jb] - N[ib][jb] * thick / 2
                q = [p0o, p1o, p1i, p0i] if not swap else [p1o, p0o, p0i, p1i]
                uvs = [T.swatch_uv(inner_flat or "hem")] * 4
                self.face(q, uvs, mat=mat)
            for (i, j) in cells:
                i2 = (i + 1) % nu
                if (i, j - 1) not in cells:      # bottom edge j
                    edge((i, j), (i2, j), True)
                if (i, j + 1) not in cells:      # top edge j+1
                    edge((i, j + 1), (i2, j + 1), False)
                if (i - 1 if (i > 0 or not wrap_u) else ni - 1, j) not in cells or (i == 0 and not wrap_u):
                    edge((i, j), (i, j + 1), False)
                if ((i + 1) % ni if wrap_u else i + 1, j) not in cells or (i == ni - 1 and not wrap_u):
                    edge((i2, j), (i2, j + 1), True)
        return P

    def slab_hole(self, outer, inner, p, axes, depth, tile="wood_dark", edge_tile=None, hole_flat="dark", swap=False):
        """A plate with a hole: outer and inner outlines (lists of (a, b)), triangulated with scanfill; front/back faces,
        outer rim and the hole's wall. swap: grain runs along b instead of a (vertical boards)."""
        ea, eb, n = axes
        t = bmesh.new()
        vo = [t.verts.new((a, b, 0)) for a, b in outer]
        vi = [t.verts.new((a, b, 0)) for a, b in inner]
        es = []
        for L in (vo, vi):
            for i in range(len(L)):
                es.append(t.edges.new((L[i], L[(i + 1) % len(L)])))
        bmesh.ops.triangle_fill(t, edges=es, use_beauty=True)
        xs = [q[0] for q in outer]
        ys = [q[1] for q in outer]
        la, lb = max(xs) - min(xs), max(ys) - min(ys)
        off = (self.rnd.random(), self.rnd.random())
        Pf = lambda a, b, s: p + ea * a + eb * b + n * (s * depth / 2)

        def uv(a, b):
            if swap:
                return self.mapuv(tile, b - min(ys), a - min(xs), lb, la, off, aniso=True)
            return self.mapuv(tile, a - min(xs), b - min(ys), la, lb, off, aniso=True)
        for f in t.faces:
            vs = [v.co for v in f.verts]
            self.face([Pf(v.x, v.y, 1) for v in vs], [uv(v.x, v.y) for v in vs])
            self.face([Pf(v.x, v.y, -1) for v in vs][::-1], [uv(v.x, v.y) for v in vs][::-1])
        t.free()
        et = edge_tile or tile
        for loop, flat in ((outer, None), (inner, hole_flat)):
            k = len(loop)
            for i in range(k):
                (a0, b0), (a1, b1) = loop[i], loop[(i + 1) % k]
                q = [Pf(a0, b0, -1), Pf(a1, b1, -1), Pf(a1, b1, 1), Pf(a0, b0, 1)]
                if flat:
                    self.face(q, [T.swatch_uv(flat)] * 4)
                else:
                    w = math.hypot(a1 - a0, b1 - b0)
                    self.face(q, [self.mapuv(et, 0, 0, w, depth, off), self.mapuv(et, w, 0, w, depth, off),
                                  self.mapuv(et, w, depth, w, depth, off), self.mapuv(et, 0, depth, w, depth, off)])

    def sheet(self, pts, tile, uvs=None, mat=0, smooth=False, thick=0.0):
        self.face(pts, uvs or [self.mapuv(tile, 0, 0, 1, 1)] * len(pts), mat=mat, smooth=smooth)

    # ---- finishing -------------------------------------------------------------------------------------------
    def peg(self, pos, direction=(0, 0, -1), r=0.012, length=0.22, tile="plank_edge"):
        d = Vector(direction).normalized()
        self.cyl(Vector(pos), Vector(pos) + d * length, r, r * 0.55, seg=5, tile=tile, cap_tile=tile)

    def shift(self, dx, dy, dz=0.0):
        for v in self.bm.verts:
            v.co += Vector((dx, dy, dz))

    def fit_uvs(self):
        """Keep every atlas face's UVs inside its tile window (compress about the centroid when a face spills over, so
        it never samples a neighbouring tile). Faces in text materials are left alone."""
        rects = {n: T.rect_uv(n) for n in T.TILES}
        for f in self.bm.faces:
            if f.material_index != 0:
                continue
            uvs = [lp[self.uv].uv for lp in f.loops]
            us, vs = [u[0] for u in uvs], [u[1] for u in uvs]
            hit = None
            for corner in ((min(us), min(vs)), (max(us), max(vs)), (min(us), max(vs)), (max(us), min(vs))):
                for n, (u0, v0, u1, v1) in rects.items():
                    if u0 - 0.003 <= corner[0] <= u1 + 0.003 and v0 - 0.003 <= corner[1] <= v1 + 0.003:
                        hit = n
                        break
                if hit:
                    break
            if not hit:
                continue
            u0, v0, u1, v1 = rects[hit]
            if hit == "swatch":
                continue
            lu, lv = max(us) - min(us), max(vs) - min(vs)
            s = min(1.0, (u1 - u0) / max(lu, 1e-9), (v1 - v0) / max(lv, 1e-9))
            cu, cv = (max(us) + min(us)) / 2, (max(vs) + min(vs)) / 2
            for lp in f.loops:
                uu = cu + (lp[self.uv].uv[0] - cu) * s
                vv = cv + (lp[self.uv].uv[1] - cv) * s
                lp[self.uv].uv = (uu, vv)
            us = [lp[self.uv].uv[0] for lp in f.loops]
            vs = [lp[self.uv].uv[1] for lp in f.loops]
            du_ = 0.0
            if min(us) < u0:
                du_ = u0 - min(us)
            elif max(us) > u1:
                du_ = u1 - max(us)
            dv_ = 0.0
            if min(vs) < v0:
                dv_ = v0 - min(vs)
            elif max(vs) > v1:
                dv_ = v1 - max(vs)
            if du_ or dv_:
                for lp in f.loops:
                    lp[self.uv].uv = (lp[self.uv].uv[0] + du_, lp[self.uv].uv[1] + dv_)

    def finish(self, weld=0.0005):
        bmesh.ops.remove_doubles(self.bm, verts=list(self.bm.verts), dist=weld)
        # drop zero-area faces
        bad = [f for f in self.bm.faces if f.calc_area() < 1e-9]
        if bad:
            bmesh.ops.delete(self.bm, geom=bad, context="FACES")
        bmesh.ops.recalc_face_normals(self.bm, faces=list(self.bm.faces))
        for f in self.bm.faces:
            f.smooth = True
        for e in self.bm.edges:
            if len(e.link_faces) == 2 and e.link_faces[0].normal.angle(e.link_faces[1].normal, 0.0) > math.radians(50):
                e.smooth = False
        # recalc may flip open shells' inner skins relative to the outside; shells here are closed (rim faces), good.
        self.fit_uvs()
        mesh = bpy.data.meshes.new(self.name)
        self.bm.to_mesh(mesh)
        mesh.update()
        return mesh


# =========================================================================================================
# materials and export
# =========================================================================================================

def _img(name, path, nonc=False):
    for im in bpy.data.images:
        if im.name == name:
            return im
    im = bpy.data.images.load(path)
    im.name = name
    if nonc:
        im.colorspace_settings.name = "Non-Color"
    im.pack()
    return im


def atlas_material():
    if "M_North_Atlas" in bpy.data.materials:
        return bpy.data.materials["M_North_Atlas"]
    if not os.path.exists(os.path.join(GEN, "north_atlas.png")):
        T.write(GEN)
    m = bpy.data.materials.new("M_North_Atlas")
    m.use_nodes = True
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Roughness"].default_value = 0.85
    bsdf.inputs["Specular IOR Level"].default_value = 0.25
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = _img("north_atlas.png", os.path.join(GEN, "north_atlas.png"))
    tex.interpolation = "Linear"
    nm = nt.nodes.new("ShaderNodeTexImage")
    nm.image = _img("north_atlas_n.png", os.path.join(GEN, "north_atlas_n.png"), nonc=True)
    nmap = nt.nodes.new("ShaderNodeNormalMap")
    nmap.inputs["Strength"].default_value = 0.8
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(nm.outputs["Color"], nmap.inputs["Color"])
    nt.links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    return m


def text_material(name, path):
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Roughness"].default_value = 0.7
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = _img(os.path.basename(path), path)
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    return m


def make_object(builder, collection=None):
    mesh = builder.finish()
    mats = {"M_North_Atlas": atlas_material()}
    for nm in builder.mat_slots[1:]:
        mats[nm] = bpy.data.materials[nm]
    for nm in builder.mat_slots:
        mesh.materials.append(mats[nm])
    ob = bpy.data.objects.new("SM_North_" + builder.name, mesh)
    (collection or bpy.context.scene.collection).objects.link(ob)
    # smooth shading follows the face flags; sharp edges stay sharp on flat faces
    return ob


def tri_count(ob):
    me = ob.data
    me.calc_loop_triangles()
    return len(me.loop_triangles)


def bbox(ob):
    vs = [ob.matrix_world @ Vector(c) for c in ob.bound_box]
    mn = Vector((min(v.x for v in vs), min(v.y for v in vs), min(v.z for v in vs)))
    mx = Vector((max(v.x for v in vs), max(v.y for v in vs), max(v.z for v in vs)))
    return mn, mx


def export_glb(ob, path):
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True, export_apply=True,
                              export_yup=True, export_texcoords=True, export_normals=True, export_tangents=False,
                              export_materials="EXPORT", export_image_format="AUTO", export_cameras=False,
                              export_lights=False)


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for coll in (bpy.data.meshes, bpy.data.materials, bpy.data.images):
        for b in list(coll):
            coll.remove(b)


def uv_lint(ob, name):
    """UV sanity of the atlas faces: inside the atlas, inside one tile window, no NaN. Returns a list of problems."""
    me = ob.data
    uv = me.uv_layers.active.data
    rects = {n: T.rect_uv(n, 0) for n in T.TILES}
    bad = []
    for p in me.polygons:
        if p.material_index != 0:
            continue
        pts = [uv[i].uv for i in p.loop_indices]
        us, vs = [q[0] for q in pts], [q[1] for q in pts]
        if min(us) < 0 or max(us) > 1 or min(vs) < 0 or max(vs) > 1 or any(x != x for x in us + vs):
            bad.append((p.index, "outside the atlas"))
            continue
        cu, cv = sum(us) / len(us), sum(vs) / len(vs)
        hit = [n for n, (u0, v0, u1, v1) in rects.items() if u0 - 1e-4 <= min(us) and max(us) <= u1 + 1e-4 and v0 - 1e-4 <= min(vs) and max(vs) <= v1 + 1e-4]
        if not hit:
            bad.append((p.index, "straddles tiles"))
    return bad
