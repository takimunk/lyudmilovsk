"""Parametric DETAIL meshes for the Academgorodok of Laboratory No. 2 (Moscow 1945-48).

Every public function builds ONE new mesh object (linked to the current scene collection) and
returns it. Units are metres. Local frame of a detail that sits on a wall:

    x = along the facade, y = outward normal of the facade (the wall face is the plane y = 0,
    the detail sits at y >= 0), z = up from the base of the detail.

Free-standing pieces (column, scroll pylon, wicket block, fountain, flowerbed railing) are
centred on x = y = 0 instead. Units that fill a wall opening (window, door) fill an opening of
exactly w x h whose bottom-left corner is at x = -w/2, z = 0, with reveals running from y = 0
back to y = -REVEAL_DEPTH. Round things that sit on a wall (oculus) are centred on x = z = 0.

Only material slot names from common.MATERIALS are used. No modifiers are used (everything is
built directly with bmesh), so the returned objects have nothing left to apply. Shading: faces are
smooth with edges sharper than ~35 deg marked sharp. UVs: box projection, 1 UV unit = 1 metre.
Each object gets a custom property "tris" with its triangle count.

Dimensions come from docs/academgorodok/facade_notes.md and the photos in refs/exterior/.
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector, noise

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)
from common import MATERIALS  # noqa: E402

REVEAL_DEPTH = 0.5      # wall thickness used for window / door reveals
GLASS_Y = -0.12         # glass plane, set back from the wall face
SHARP_ANGLE = math.radians(35.0)

# Novikova 1 eaves cornice: where the modillions attach (see eaves_cornice_profile_points)
EAVES_MODILLION_Y = 0.06    # face of the cornice core under the soffit
EAVES_MODILLION_Z = 0.12    # top of the bed moulding = bottom of the modillions
EAVES_SOFFIT_Z = 0.34       # underside of the soffit = top of the modillions
EAVES_OVERHANG = 1.03
MODILLION_SPACING = 0.6


# --------------------------------------------------------------------------------------------
# materials

def _srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def get_material(name):
    """Return the bpy material for a slot name of common.MATERIALS, creating it if missing."""
    if name not in MATERIALS:
        raise KeyError("unknown material slot %r (use common.MATERIALS)" % name)
    mat = bpy.data.materials.get(name)
    if mat is not None:
        return mat
    hexc, rough, _desc = MATERIALS[name]
    h = hexc.lstrip("#")
    rgb = [_srgb_to_linear(int(h[i:i + 2], 16) / 255.0) for i in (0, 2, 4)]
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (rgb[0], rgb[1], rgb[2], 1.0)
    mat.roughness = rough
    nt = mat.node_tree
    if nt is not None:
        for n in nt.nodes:
            if n.type == 'BSDF_PRINCIPLED':
                n.inputs["Base Color"].default_value = (rgb[0], rgb[1], rgb[2], 1.0)
                n.inputs["Roughness"].default_value = rough
                if name == "AC_Glass":
                    n.inputs["Metallic"].default_value = 0.0
                    try:
                        n.inputs["Specular IOR Level"].default_value = 0.8
                    except KeyError:
                        pass
                if name in ("AC_Zinc",):
                    n.inputs["Metallic"].default_value = 0.6
    return mat


# --------------------------------------------------------------------------------------------
# 2D helpers

def _arc(cx, cz, r, a0, a1, n):
    return [(cx + r * math.cos(a0 + (a1 - a0) * i / n), cz + r * math.sin(a0 + (a1 - a0) * i / n))
            for i in range(n + 1)]


def _chaikin(pts, sharp=None, iters=3, closed=True):
    """Corner-cutting smoothing that keeps vertices flagged sharp (and the ends of open paths)."""
    pts = [tuple(p) for p in pts]
    sharp = list(sharp) if sharp is not None else [False] * len(pts)
    for _ in range(iters):
        n = len(pts)
        npts, nsh = [], []
        for i in range(n):
            p = pts[i]
            end = (not closed) and (i == 0 or i == n - 1)
            if sharp[i] or end:
                npts.append(p)
                nsh.append(True)
                continue
            a = pts[(i - 1) % n]
            b = pts[(i + 1) % n]
            npts.append(tuple(0.75 * p[k] + 0.25 * a[k] for k in range(len(p))))
            npts.append(tuple(0.75 * p[k] + 0.25 * b[k] for k in range(len(p))))
            nsh += [False, False]
        pts, sharp = npts, nsh
    return pts


def _spiral(cx, cz, r0, r1, a0, turns, n, cw=False):
    """Planar spiral from radius r0 at angle a0 out to r1 after `turns` turns."""
    out = []
    s = -1.0 if cw else 1.0
    for i in range(n + 1):
        t = i / n
        a = a0 + s * turns * 2 * math.pi * t
        r = r0 + (r1 - r0) * t
        out.append((cx + r * math.cos(a), cz + r * math.sin(a)))
    return out


def _resample(pts, step):
    """Resample an open 2D/3D polyline to roughly uniform spacing."""
    out = [pts[0]]
    acc = 0.0
    for i in range(1, len(pts)):
        a, b = Vector(pts[i - 1]), Vector(pts[i])
        seg = (b - a).length
        if seg < 1e-9:
            continue
        acc += seg
        if acc >= step or i == len(pts) - 1:
            out.append(pts[i])
            acc = 0.0
    return out


# --------------------------------------------------------------------------------------------
# mesh builder

class _B:
    """Small bmesh builder: primitives write straight into one bmesh with material indices."""

    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.mats = []
        self._gv = []
        self._gf = []

    # bookkeeping ---------------------------------------------------------------------------
    def mi(self, mat):
        if mat not in MATERIALS:
            raise KeyError(mat)
        if mat not in self.mats:
            self.mats.append(mat)
        return self.mats.index(mat)

    def begin(self):
        self._gv, self._gf = [], []

    def v(self, co):
        vert = self.bm.verts.new(co)
        self._gv.append(vert)
        return vert

    def f(self, verts, mat):
        try:
            face = self.bm.faces.new(verts)
        except ValueError:
            return None
        face.material_index = self.mi(mat)
        self._gf.append(face)
        return face

    def end(self, recalc=False, bevel=0.0, seg=1, xform=None, orient=None):
        verts, faces = self._gv, self._gf
        if recalc and faces:
            bmesh.ops.recalc_face_normals(self.bm, faces=faces)
        if orient is not None:
            c = Vector(orient)
            for fc in faces:
                if fc.is_valid and fc.normal.dot(fc.calc_center_median() - c) < 0:
                    fc.normal_flip()
        if bevel > 0 and faces:
            fs = set(faces)
            edges = []
            for fc in faces:
                for e in fc.edges:
                    lf = [x for x in e.link_faces]
                    if len(lf) == 2 and lf[0] in fs and lf[1] in fs:
                        if lf[0].normal.angle(lf[1].normal, 0.0) > 0.5:
                            edges.append(e)
            edges = list(set(edges))
            if edges:
                bmesh.ops.bevel(self.bm, geom=edges, offset=bevel, offset_type='OFFSET',
                                segments=seg, profile=0.5, affect='EDGES', clamp_overlap=True)
        if xform is not None:
            vv = [x for x in verts if x.is_valid]
            bmesh.ops.transform(self.bm, matrix=xform, verts=vv)
        self._gv, self._gf = [], []
        return verts

    # primitives -----------------------------------------------------------------------------
    def box(self, mn, mx, mat, bevel=0.0, seg=1, xform=None):
        self.begin()
        x0, y0, z0 = mn
        x1, y1, z1 = mx
        c = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
             (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
        v = [self.v(p) for p in c]
        for q in ((0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)):
            self.f([v[i] for i in q], mat)
        return self.end(bevel=bevel, seg=seg, xform=xform)

    def bar(self, p0, p1, width, y0, y1, mat):
        """Box between two points (x, z) of the facade plane, `width` across, y0..y1 deep."""
        a, b = Vector((p0[0], p0[1])), Vector((p1[0], p1[1]))
        d = b - a
        L = d.length
        if L < 1e-6:
            return
        ang = math.atan2(d.y, d.x)
        m = (Matrix.Translation((a.x, 0, a.y)) @ Matrix.Rotation(-ang, 4, 'Y'))
        self.box((0, y0, -width / 2), (L, y1, width / 2), mat, xform=m)

    def _map(self, axis, p, s):
        if axis == 'y':
            return (p[0], s, p[1])
        if axis == 'x':
            return (s, p[0], p[1])
        return (p[0], p[1], s)

    def prism(self, poly, a0, a1, mat, axis='y', bevel=0.0, seg=1, xform=None, fan=None):
        """Extrude a closed 2D polygon. axis 'y': poly is (x, z); 'x': (y, z); 'z': (x, y).
        fan=(cx, cz): build the caps as a triangle fan around that point (star shapes)."""
        self.begin()
        n = len(poly)
        A = [self.v(self._map(axis, p, a0)) for p in poly]
        B = [self.v(self._map(axis, p, a1)) for p in poly]
        if fan is None:
            self.f(list(reversed(A)), mat)
            self.f(B, mat)
        else:
            ca = self.v(self._map(axis, fan, a0))
            cb = self.v(self._map(axis, fan, a1))
            for i in range(n):
                j = (i + 1) % n
                self.f([ca, A[j], A[i]], mat)
                self.f([cb, B[i], B[j]], mat)
        for i in range(n):
            j = (i + 1) % n
            self.f([A[i], A[j], B[j], B[i]], mat)
        return self.end(recalc=True, bevel=bevel, seg=seg, xform=xform)

    def ring_prism(self, outer, inner, a0, a1, mat, axis='y', xform=None, bevel=0.0):
        """Extrude the band between two closed loops with the same vertex count."""
        self.begin()
        n = len(outer)
        O0 = [self.v(self._map(axis, p, a0)) for p in outer]
        O1 = [self.v(self._map(axis, p, a1)) for p in outer]
        I0 = [self.v(self._map(axis, p, a0)) for p in inner]
        I1 = [self.v(self._map(axis, p, a1)) for p in inner]
        for i in range(n):
            j = (i + 1) % n
            self.f([O1[i], O1[j], I1[j], I1[i]], mat)
            self.f([O0[i], I0[i], I0[j], O0[j]], mat)
            self.f([O0[i], O0[j], O1[j], O1[i]], mat)
            self.f([I0[i], I1[i], I1[j], I0[j]], mat)
        return self.end(recalc=True, xform=xform, bevel=bevel)

    def lathe(self, prof, segs, mat, center=(0, 0, 0), mod=None, a0=0.0, a1=2 * math.pi,
              xform=None, recalc=True):
        """Revolve profile [(r, z)] (ordered bottom -> outside -> top) around the z axis."""
        self.begin()
        full = abs((a1 - a0) - 2 * math.pi) < 1e-6
        nseg = segs if full else segs + 1
        cx, cy, cz = center
        rings = []
        for i, (r, z) in enumerate(prof):
            if r < 1e-7:
                rings.append([self.v((cx, cy, cz + z))])
                continue
            ring = []
            for k in range(nseg):
                a = a0 + (a1 - a0) * k / segs
                rr = mod(i, a, r, z) if mod else r
                ring.append(self.v((cx + rr * math.cos(a), cy + rr * math.sin(a), cz + z)))
            rings.append(ring)
        kmax = segs if full else segs
        for i in range(len(rings) - 1):
            R0, R1 = rings[i], rings[i + 1]
            for k in range(kmax):
                k1 = (k + 1) % nseg if full else k + 1
                if len(R0) == 1 and len(R1) == 1:
                    continue
                if len(R0) == 1:
                    self.f([R0[0], R1[k1], R1[k]], mat)
                elif len(R1) == 1:
                    self.f([R0[k], R0[k1], R1[0]], mat)
                else:
                    self.f([R0[k], R0[k1], R1[k1], R1[k]], mat)
        return self.end(recalc=recalc and full, xform=xform)

    def cyl(self, x, y, z0, z1, r, mat, sides=8):
        self.lathe([(0, z0), (r, z0), (r, z1), (0, z1)], sides, mat, center=(x, y, 0))

    def sweep_plan(self, prof, path, mat, closed=False, cap=True, cw_out=False):
        """Sweep a profile [(o, z)] along a horizontal path [(x, y)].

        o is the offset along the path normal that points to the RIGHT of the direction of travel
        rotated CCW (for a path travelling +x the normal is +y). Corners are mitred."""
        self.begin()
        n = len(path)
        P = [Vector((p[0], p[1])) for p in path]

        def nrm(d):
            d = d.normalized()
            return Vector((-d.y, d.x))
        segn = []
        rng = n if closed else n - 1
        for i in range(rng):
            segn.append(nrm(P[(i + 1) % n] - P[i]))
        rings = []
        for i in range(n):
            if closed:
                ni, no = segn[i - 1], segn[i % len(segn)]
            else:
                ni = segn[i - 1] if i > 0 else segn[0]
                no = segn[i] if i < len(segn) else segn[-1]
            m = (ni + no) / (1.0 + ni.dot(no))
            ring = [self.v((P[i].x + m.x * o, P[i].y + m.y * o, z)) for (o, z) in prof]
            rings.append(ring)
        k = len(prof)
        for i in range(rng):
            R0, R1 = rings[i], rings[(i + 1) % n]
            for j in range(k):
                j1 = (j + 1) % k
                self.f([R0[j], R1[j], R1[j1], R0[j1]], mat)
        if not closed and cap:
            self.f(list(reversed(rings[0])), mat)
            self.f(rings[-1], mat)
        return self.end(recalc=True)

    def tube(self, pts, radius, mat, sides=6, cap=True, closed=False):
        """Tube along a 3D polyline; radius is a float or a list (one per point)."""
        self.begin()
        P = [Vector(p) for p in pts]
        n = len(P)
        rad = radius if isinstance(radius, (list, tuple)) else [radius] * n
        T = []
        for i in range(n):
            if closed:
                d = P[(i + 1) % n] - P[i - 1]
            else:
                d = P[min(i + 1, n - 1)] - P[max(i - 1, 0)]
            T.append(d.normalized())
        up = Vector((0, 0, 1)) if abs(T[0].z) < 0.9 else Vector((1, 0, 0))
        N = T[0].cross(up).normalized()
        rings = []
        for i in range(n):
            if i > 0:
                ax = T[i - 1].cross(T[i])
                if ax.length > 1e-9:
                    ang = T[i - 1].angle(T[i], 0.0)
                    N = (Matrix.Rotation(ang, 3, ax.normalized()) @ N)
                N = (N - T[i] * N.dot(T[i])).normalized()
            Bn = T[i].cross(N)
            ring = []
            for s in range(sides):
                a = 2 * math.pi * s / sides
                ring.append(self.v(P[i] + (N * math.cos(a) + Bn * math.sin(a)) * rad[i]))
            rings.append(ring)
        rng = n if closed else n - 1
        for i in range(rng):
            R0, R1 = rings[i], rings[(i + 1) % n]
            for s in range(sides):
                s1 = (s + 1) % sides
                self.f([R0[s], R0[s1], R1[s1], R1[s]], mat)
        if cap and not closed:
            self.f(list(reversed(rings[0])), mat)
            self.f(rings[-1], mat)
        return self.end(recalc=True)

    def flat_bar(self, pts, frame, width, thick, mat, closed=False, off=0.0):
        """Rectangular bar along a 2D path (u, v) drawn in a plane.

        frame = (origin, U, V, N): plane axes and the direction of the bar thickness.
        width: in-plane width (float or per-point list); thick: along N starting at `off`."""
        self.begin()
        O, U, V, Nn = (Vector(a) for a in frame)
        n = len(pts)
        W = width if isinstance(width, (list, tuple)) else [width] * n
        P = [Vector(p) for p in pts]
        quads = []
        for i in range(n):
            if closed:
                d = P[(i + 1) % n] - P[i - 1]
            else:
                d = P[min(i + 1, n - 1)] - P[max(i - 1, 0)]
            if d.length < 1e-9:
                d = Vector((1, 0))
            d.normalize()
            m = Vector((-d.y, d.x)) * (W[i] / 2)
            pl, pr = P[i] + m, P[i] - m

            def w3(p, t):
                return O + U * p.x + V * p.y + Nn * t
            quads.append([self.v(w3(pl, off)), self.v(w3(pr, off)),
                          self.v(w3(pr, off + thick)), self.v(w3(pl, off + thick))])
        rng = n if closed else n - 1
        for i in range(rng):
            A, Bq = quads[i], quads[(i + 1) % n]
            for s in range(4):
                s1 = (s + 1) % 4
                self.f([A[s], Bq[s], Bq[s1], A[s1]], mat)
        if not closed:
            self.f(list(reversed(quads[0])), mat)
            self.f(quads[-1], mat)
        return self.end(recalc=True)

    def rock(self, front, back, mat, mode='rock', seed=0.0, rough=0.02, chamfer=0.03,
             cell=0.05, edges=(True, True, True, True), groove=0.012, sides=(True, True, True, True),
             arris=None):
        """A block with a displaced front face.

        front: 4 corners (bl, br, tr, tl) of the front face (3D); back: the same 4 corners on the
        wall. Sides are built for the edges flagged in `sides` (bottom, right, top, left).
        `edges` flags which front edges get the chamfer (an unchamfered edge is left crisp, e.g.
        the corner arris of a quoin). mode: 'rock' (pitched face), 'verm' (vermiculated),
        'smooth' (just the chamfer)."""
        self.begin()
        F = [Vector(p) for p in front]
        Bk = [Vector(p) for p in back]
        nrm = (F[1] - F[0]).cross(F[3] - F[0]).normalized()
        if nrm.dot(F[0] - Bk[0]) < 0:
            nrm = -nrm
        Lu = ((F[1] - F[0]).length + (F[2] - F[3]).length) / 2
        Lv = ((F[3] - F[0]).length + (F[2] - F[1]).length) / 2

        def params(L, lo, hi):
            n = max(1, int(math.ceil(L / cell)))
            s = {i / n for i in range(n + 1)}
            c = chamfer / L if L > 0 else 0
            if lo and c < 0.45:
                s.add(c)
            if hi and c < 0.45:
                s.add(1 - c)
            return sorted(s)
        su = params(Lu, edges[3], edges[1])
        sv = params(Lv, edges[0], edges[2])
        grid = []
        for t in sv:
            row = []
            for s in su:
                p = F[0].lerp(F[1], s).lerp(F[3].lerp(F[2], s), t)
                dl, dr = s * Lu, (1 - s) * Lu
                db, dt = t * Lv, (1 - t) * Lv
                dists = []
                for flag, dd in ((edges[0], db), (edges[1], dr), (edges[2], dt), (edges[3], dl)):
                    dists.append(dd if flag else 1e9)
                dmin = min(dists)
                dall = min(dl, dr, db, dt)
                disp = 0.0
                if dmin < chamfer:
                    disp = -(chamfer - dmin)
                else:
                    fall = min(1.0, (dall - (chamfer if dmin < 1e8 else 0.0)) / 0.04)
                    fall = max(0.0, fall) if dall > 0.015 else 0.0
                    q = Vector((p.x * 7.0 + seed * 3.1, p.y * 7.0 + seed * 1.7, p.z * 7.0))
                    if mode == 'rock':
                        nz = noise.noise(q) * 0.55 + noise.noise(q * 2.7) * 0.35 + noise.noise(q * 6.1) * 0.25
                        disp = rough * (0.35 + nz) * fall
                    elif mode == 'verm':
                        w = noise.noise(q * 1.8)
                        w2 = noise.noise(q * 1.8 + Vector((11.3, 5.1, 2.2)))
                        g = min(1.0, max(0.0, 1.6 - abs(w) / 0.11))
                        g2 = min(1.0, max(0.0, 1.6 - abs(w2) / 0.09))
                        disp = -groove * max(g, g2) * fall + 0.004 * noise.noise(q * 5) * fall
                if arris is not None and ((arris[0] == 1 and s == su[-1]) or (arris[0] == 3 and s == su[0])):
                    # shared corner arris of a quoin: move inwards along both faces so the two
                    # blocks of a course meet without a crack
                    row.append(self.v(p + (nrm + Vector(arris[1])) * disp))
                else:
                    row.append(self.v(p + nrm * disp))
            grid.append(row)
        nu, nv = len(su), len(sv)
        for j in range(nv - 1):
            for i in range(nu - 1):
                self.f([grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]], mat)
        bv = [self.v(p) for p in Bk]
        if sides[0]:
            self.f([bv[0]] + [grid[0][i] for i in range(nu)] + [bv[1]], mat)
        if sides[1]:
            self.f([bv[1]] + [grid[j][nu - 1] for j in range(nv)] + [bv[2]], mat)
        if sides[2]:
            self.f([bv[2]] + [grid[nv - 1][i] for i in reversed(range(nu))] + [bv[3]], mat)
        if sides[3]:
            self.f([bv[3]] + [grid[j][0] for j in reversed(range(nv))] + [bv[0]], mat)
        cen = sum(Bk, Vector()) / 4 * 0.5 + sum(F, Vector()) / 4 * 0.5
        # orient every face away from the block centre (front grid faces: along nrm)
        for fc in self._gf:
            if fc.is_valid and fc.normal.dot(fc.calc_center_median() - cen) < 0:
                fc.normal_flip()
        return self.end()

    def add_mesh(self, me, matrix, mat):
        self.begin()
        vs = [self.v(matrix @ v.co) for v in me.vertices]
        for p in me.polygons:
            self.f([vs[i] for i in p.vertices], mat)
        return self.end()

    def quad(self, pts, mat):
        self.begin()
        self.f([self.v(p) for p in pts], mat)
        return self.end()

    # finish -----------------------------------------------------------------------------------
    def finish(self):
        bm = self.bm
        bmesh.ops.remove_doubles(bm, verts=[v for v in bm.verts if not v.link_faces], dist=1e-6)
        loose = [v for v in bm.verts if not v.link_faces]
        if loose:
            bmesh.ops.delete(bm, geom=loose, context='VERTS')
        bm.normal_update()
        uv = bm.loops.layers.uv.new("UVMap")
        for fc in bm.faces:
            fc.smooth = True
            n = fc.normal
            ax = max(range(3), key=lambda i: abs(n[i]))
            for lp in fc.loops:
                co = lp.vert.co
                if ax == 2:
                    u, v = co.x * (1 if n.z >= 0 else -1), co.y
                elif ax == 1:
                    u, v = co.x * (-1 if n.y >= 0 else 1), co.z
                else:
                    u, v = co.y * (1 if n.x >= 0 else -1), co.z
                lp[uv].uv = (u, v)
        for e in bm.edges:
            lf = e.link_faces
            if len(lf) != 2:
                e.smooth = False
            else:
                e.smooth = lf[0].normal.angle(lf[1].normal, 0.0) < SHARP_ANGLE
        me = bpy.data.meshes.new(self.name)
        bm.to_mesh(me)
        bm.free()
        for m in self.mats:
            me.materials.append(get_material(m))
        me.update()
        obj = bpy.data.objects.new(self.name, me)
        coll = bpy.context.collection or bpy.context.scene.collection
        coll.objects.link(obj)
        obj["tris"] = triangle_count(obj)
        return obj


def triangle_count(obj):
    return sum(len(p.vertices) - 2 for p in obj.data.polygons)


def _text_mesh(body, size, extrude, align='CENTER'):
    """Blender text object converted to a mesh datablock (default Bfont). Caller frees it."""
    cu = bpy.data.curves.new("tmp_text", 'FONT')
    cu.body = body
    cu.size = size
    cu.extrude = extrude
    cu.align_x = align
    cu.align_y = 'CENTER'
    cu.resolution_u = 4
    ob = bpy.data.objects.new("tmp_text", cu)
    bpy.context.scene.collection.objects.link(ob)
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    bpy.data.objects.remove(ob)
    bpy.data.curves.remove(cu)
    return me


def _opening(w, h, arched=False, inset=0.0, n_arc=16):
    """Outline of a wall opening (x, z), CCW seen from outside, starting bottom-left.

    inset > 0 shrinks it (frame inner edge), inset < 0 grows it (surround outer edge)."""
    x0, x1 = -w / 2 + inset, w / 2 - inset
    z0 = inset
    if not arched:
        return [(x0, z0), (x1, z0), (x1, h - inset), (x0, h - inset)]
    r = w / 2
    zs = h - r
    return [(x0, z0), (x1, z0)] + _arc(0.0, zs, r - inset, 0.0, math.pi, n_arc)


def _arch_band(w, h, i_in, i_out, n_arc=20):
    """Closed (x, z) polygon of a round-headed band open at the bottom (legs stand on z = 0):
    between the arched outline grown by -i_in and by -i_out (i_out > i_in >= 0)."""
    r = w / 2
    zs = h - r
    outer = [(r + i_out, 0.0)] + _arc(0.0, zs, r + i_out, 0.0, math.pi, n_arc) + [(-r - i_out, 0.0)]
    inner = [(-r - i_in, 0.0)] + _arc(0.0, zs, r + i_in, math.pi, 0.0, n_arc) + [(r + i_in, 0.0)]
    return outer + inner


# --------------------------------------------------------------------------------------------
# cornice profiles

def eaves_cornice_profile_points():
    """Novikova 1 eaves cornice, closed (y, z) profile; wall face y = 0, z = 0 at the bottom of
    the bed moulding. White plastered: bed band, core face under the soffit where the
    modillions sit (y = EAVES_MODILLION_Y, z = EAVES_MODILLION_Z..EAVES_SOFFIT_Z), flat soffit
    ~1.0 m deep, drip, fascia with a gutter lip. Overhang 1.03 m, height 0.56 m."""
    return [
        (0.0, 0.0), (0.025, 0.0), (0.025, 0.05), (0.04, 0.065), (0.045, 0.085),
        (0.06, 0.10), (0.06, EAVES_MODILLION_Z),
        (EAVES_MODILLION_Y, EAVES_SOFFIT_Z), (0.93, EAVES_SOFFIT_Z), (0.93, EAVES_SOFFIT_Z - 0.025),
        (0.97, EAVES_SOFFIT_Z - 0.025), (0.97, 0.33), (0.99, 0.35), (0.99, 0.47), (1.01, 0.48),
        (1.03, 0.50), (1.03, 0.56), (0.0, 0.56),
    ]


def hood_cornice_profile_points():
    """Straight hood cornice (сандрик) over a window, closed (y, z) profile, 0.22 m tall,
    0.16 m projection: fillet, cyma reversa bed, corona with drip, cymatium, sloped top."""
    return [
        (0.0, 0.0), (0.02, 0.0), (0.02, 0.02), (0.035, 0.03), (0.06, 0.045), (0.08, 0.07),
        (0.085, 0.085), (0.15, 0.085), (0.15, 0.135), (0.16, 0.14), (0.16, 0.15),
        (0.14, 0.17), (0.12, 0.19), (0.11, 0.195), (0.0, 0.22),
    ]


def plinth_profile_points():
    """Plinth band (цоколь), closed (y, z) profile, 0.7 m tall, 0.05 m proud of the wall
    above, with a weathered (sloped) top and a small drip."""
    return [(0.0, 0.0), (0.05, 0.0), (0.05, 0.64), (0.06, 0.65), (0.06, 0.665), (0.0, 0.70)]


def cornice_run(profile, length, mat="AC_Trim", returns=True, name="cornice_run"):
    """Straight run of a cornice profile along x (x = -length/2..length/2) with mitred returns
    back into the wall at both ends (when `returns`)."""
    b = _B(name)
    L = length / 2
    if returns:
        d = max(p[0] for p in profile) + 0.05
        path = [(-L, -d), (-L, 0.0), (L, 0.0), (L, -d)]
    else:
        path = [(-L, 0.0), (L, 0.0)]
    b.sweep_plan(profile, path, mat)
    return b.finish()


# --------------------------------------------------------------------------------------------
# eaves pieces

def modillion(depth=0.36, width=0.12, height=0.22):
    """White eaves bracket of Novikova 1 (AC_Trim). Back on y = 0 (the cornice core face,
    EAVES_MODILLION_Y), top at z = height touching the soffit. S-curve underside: full height
    at the wall, small scroll at the tip, thin cap plate."""
    b = _B("modillion")
    d, h = depth, height
    prof = [(0.0, h), (0.0, 0.18 * h), (0.03 * d, 0.05 * h), (0.12 * d, 0.0), (0.22 * d, 0.02 * h),
            (0.40 * d, 0.16 * h), (0.62 * d, 0.40 * h), (0.80 * d, 0.50 * h),
            (0.90 * d, 0.47 * h), (0.97 * d, 0.52 * h), (d, 0.64 * h), (d, 0.86 * h), (d, 0.86 * h)]
    sharp = [True, False, False, False, False, False, False, False, False, False, False, True, True]
    pts = _chaikin(prof[:-1] + [(0.0, h)], sharp[:-1] + [True], iters=2, closed=True)
    # pts is (y, z); remove the duplicated closing point
    clean = []
    for p in pts:
        if not clean or (Vector(p) - Vector(clean[-1])).length > 1e-5:
            clean.append(p)
    if (Vector(clean[0]) - Vector(clean[-1])).length < 1e-5:
        clean.pop()
    b.prism(clean, -width / 2, width / 2, "AC_Trim", axis='x', bevel=0.006)
    # cap plate under the soffit
    b.box((-width / 2 - 0.012, 0.0, 0.86 * h), (width / 2 + 0.012, d + 0.012, h), "AC_Trim", bevel=0.004)
    return b.finish()


def rafter_tail(length=1.1, w=0.12, h=0.18, pitch_deg=0.0):
    """Side-building timber rafter end / bracket (AC_Timber), from the wall (y = 0) out to
    y = length, with an ogee-cut end. pitch_deg tilts it (tip down) to follow a roof slope."""
    b = _B("rafter_tail")
    L = length
    pts = [(0.0, 0.0), (L - 0.28, 0.0)]
    # ogee: convex then concave up to 0.55 h at the tip
    for i in range(1, 9):
        t = i / 8.0
        y = L - 0.28 + 0.26 * t
        z = 0.55 * h * (0.5 - 0.5 * math.cos(math.pi * t)) + 0.06 * h * math.sin(2 * math.pi * t)
        pts.append((y, z))
    pts += [(L, 0.62 * h), (L, h), (0.0, h)]
    b.prism(pts, -w / 2, w / 2, "AC_Timber", axis='x', bevel=0.005)
    if pitch_deg:
        for v in b.bm.verts:
            v.co = Matrix.Rotation(-math.radians(pitch_deg), 3, 'X') @ v.co
    return b.finish()


# --------------------------------------------------------------------------------------------
# windows and doors

def _reveal(b, outline, depth, mat="AC_Trim", y0=0.0):
    """Inward-facing liner of a wall opening from y0 back to -depth."""
    b.begin()
    n = len(outline)
    A = [b.v((p[0], y0, p[1])) for p in outline]
    Bv = [b.v((p[0], -depth, p[1])) for p in outline]
    for i in range(n):
        j = (i + 1) % n
        b.f([A[i], Bv[i], Bv[j], A[j]], mat)
    xs = [p[0] for p in outline]
    zs = [p[1] for p in outline]
    cx, cz = (min(xs) + max(xs)) / 2, (min(zs) + max(zs)) / 2
    for fc in b._gf:
        c = fc.calc_center_median()
        if fc.normal.dot(Vector((cx, c.y, cz)) - c) < 0:
            fc.normal_flip()
    b.end()


def _glass(b, poly, y=GLASS_Y):
    b.begin()
    f = b.f([b.v((p[0], y, p[1])) for p in poly], "AC_Glass")
    if f is not None and f.normal.y < 0:
        f.normal_flip()
    b.end()


def _sash(b, x0, z0, x1, z1, sw=0.05, y0=-0.145, y1=-0.09, bars_h=(), bars_v=(), bw=0.028,
          mat="AC_Frame"):
    """Rectangular sash ring with glazing bars (fractions of the inner size)."""
    b.box((x0, y0, z0), (x1, y1, z0 + sw), mat)
    b.box((x0, y0, z1 - sw), (x1, y1, z1), mat)
    b.box((x0, y0, z0 + sw), (x0 + sw, y1, z1 - sw), mat)
    b.box((x1 - sw, y0, z0 + sw), (x1, y1, z1 - sw), mat)
    ix0, ix1, iz0, iz1 = x0 + sw, x1 - sw, z0 + sw, z1 - sw
    for t in bars_h:
        z = iz0 + (iz1 - iz0) * t
        b.box((ix0, y0 + 0.01, z - bw / 2), (ix1, y1 - 0.005, z + bw / 2), mat)
    for t in bars_v:
        x = ix0 + (ix1 - ix0) * t
        b.box((x - bw / 2, y0 + 0.01, iz0), (x + bw / 2, y1 - 0.005, iz1), mat)


def _zinc_sill(b, w, proj=0.07):
    """Galvanised drip sheet at the bottom of an opening, sloping out over the wall face."""
    x0, x1 = -w / 2 - 0.03, w / 2 + 0.03
    yb, yf = -0.17, proj
    t = 0.006
    zb, zf = 0.012, -0.03
    b.begin()
    q = [(yb, zb), (yf, zf), (yf, zf - 0.035), (yf - t, zf - 0.035), (yf - t, zf - t), (yb, zb - t)]
    A = [b.v((x0, y, z)) for (y, z) in q]
    Bv = [b.v((x1, y, z)) for (y, z) in q]
    n = len(q)
    b.f(list(reversed(A)), "AC_Zinc")
    b.f(Bv, "AC_Zinc")
    for i in range(n):
        j = (i + 1) % n
        b.f([A[i], A[j], Bv[j], Bv[i]], "AC_Zinc")
    b.end(recalc=True)


def _glazed_rect(b, x0, x1, z0, z1, cols=2, transom=0.45, panel=0.0, fw=0.06, y_face=0.0,
                 bars=(0.55,)):
    """Frame + casements of one rectangular light in the plane y_face (frame y_face-0.17 ..
    y_face-0.08): `cols` sashes, a transom light `transom` m tall (0 = none), an optional
    solid lower panel to `panel` m (doors / bay windows)."""
    yo = y_face
    b.ring_prism([(x0, z0), (x1, z0), (x1, z1), (x0, z1)],
                 [(x0 + fw, z0 + fw), (x1 - fw, z0 + fw), (x1 - fw, z1 - fw), (x0 + fw, z1 - fw)],
                 yo - 0.17, yo - 0.08, "AC_Frame")
    ix0, ix1, iz0, iz1 = x0 + fw, x1 - fw, z0 + fw, z1 - fw
    _glass(b, [(ix0, iz0), (ix1, iz0), (ix1, iz1), (ix0, iz1)], y=yo + GLASS_Y)
    tb = 0.06
    zt = iz1 - transom if transom > 0 else iz1 + tb / 2
    if transom > 0:
        b.box((ix0, yo - 0.16, zt - tb / 2), (ix1, yo - 0.085, zt + tb / 2), "AC_Frame")
    xs = [ix0 + (ix1 - ix0) * k / cols for k in range(cols + 1)]
    for k in range(1, cols):
        b.box((xs[k] - tb / 2, yo - 0.16, iz0), (xs[k] + tb / 2, yo - 0.085, iz1), "AC_Frame")
    for k in range(cols):
        a0 = xs[k] + (tb / 2 if k else 0.0)
        a1 = xs[k + 1] - (tb / 2 if k < cols - 1 else 0.0)
        lo = iz0
        if panel > 0:
            b.box((a0, yo - 0.15, iz0), (a1, yo - 0.10, panel), "AC_Frame")
            lo = panel
        if zt - tb / 2 - lo > 0.15:
            _sash(b, a0, lo, a1, zt - tb / 2, y0=yo - 0.145, y1=yo - 0.09, bars_h=bars)
        if transom > 0:
            _sash(b, a0, zt + tb / 2, a1, iz1, sw=0.04, y0=yo - 0.145, y1=yo - 0.09)


def _palladian(b, w, h):
    """Serliana filling a rectangular w x h hole: centre door with an arched lunette, flat side
    lights, white piers + entablature + archivolt (AC_Trim), ochre spandrels (AC_Wall)."""
    T = "AC_Trim"
    pier = 0.20
    av = 0.13                     # archivolt band
    wc = max(0.9, min(1.3, 0.44 * w))
    rc = wc / 2
    zs = h - av - rc              # springing line of the centre arch
    ent = 0.16
    D = -REVEAL_DEPTH
    for s in (-1, 1):
        xa, xb = sorted((s * wc / 2, s * (wc / 2 + pier)))
        b.box((xa, D, 0.0), (xb, 0.03, zs), T, bevel=0.006)                 # pier
        b.box((xa - 0.02, D, zs - 0.05), (xb + 0.02, 0.05, zs), T, bevel=0.006)   # impost
        xo = s * w / 2
        xi = s * (wc / 2 + pier)
        xa, xb = sorted((xo, xi))
        b.box((xa, D, zs - ent), (xb, 0.03, zs), T, bevel=0.006)            # entablature
        # spandrel above the side light up to the rectangle, cut by the archivolt
        arc = _arc(0.0, zs, rc + av, math.pi if s < 0 else 0.0, math.pi / 2, 10)
        if s < 0:
            poly = [(xo, zs)] + arc + [(xo, h)]
        else:
            poly = [(xo, zs), (xo, h)] + list(reversed(arc))
        b.prism(poly, D, 0.0, "AC_Wall")
        # side light (low panel: they open onto the balcony)
        x0, x1 = sorted((xo, xi))
        _glazed_rect(b, x0, x1, 0.0, zs - ent, cols=1, transom=0.4, panel=0.0, fw=0.055)
    # archivolt (band between rc and rc + av), proud of the wall
    outer = _arc(0.0, zs, rc + av, 0.0, math.pi, 20)
    inner = _arc(0.0, zs, rc, math.pi, 0.0, 20)
    b.prism(outer + inner, D, 0.035, T, bevel=0.006)
    b.prism([(-0.07, zs + rc - 0.01), (0.07, zs + rc - 0.01), (0.09, zs + rc + av + 0.06),
             (-0.09, zs + rc + av + 0.06)], 0.0, 0.06, T, bevel=0.006)                 # keystone
    # centre: glazed double door + lunette
    _glazed_rect(b, -wc / 2, wc / 2, 0.0, zs, cols=2, transom=0.0, panel=0.85, fw=0.06, bars=(0.6,))
    lun = [(rc, zs)] + _arc(0.0, zs, rc, 0.0, math.pi, 16)[1:]
    lin = [(rc - 0.06, zs + 0.0)] + _arc(0.0, zs, rc - 0.06, 0.0, math.pi, 16)[1:]
    b.ring_prism(lun, lin, -0.17, -0.08, "AC_Frame")
    _glass(b, lin)
    for a in (math.pi / 4, math.pi / 2, 3 * math.pi / 4):
        b.bar((0.0, zs), ((rc - 0.05) * math.cos(a), zs + (rc - 0.05) * math.sin(a)), 0.035, -0.15, -0.09,
              "AC_Frame")
    b.box((-rc, -0.17, zs - 0.03), (rc, -0.08, zs + 0.03), "AC_Frame")


def _paired(b, w, h):
    """Two narrow windows in one w x h hole with a turned white colonnette between them
    (courtyard wing faces, refs 18996369890 / _1_4_)."""
    col = 0.26
    lw = (w - col) / 2
    D = -REVEAL_DEPTH
    b.box((-col / 2, D, 0.0), (col / 2, -0.16, h), "AC_Wall")
    for s in (-1, 1):
        x0, x1 = sorted((s * col / 2, s * (col / 2 + lw)))
        _glazed_rect(b, x0, x1, 0.0, h, cols=2, transom=min(0.48, 0.25 * h), fw=0.06)
    # colonnette: base block, turned shaft with rings, capital block
    d = 0.16
    yc = -0.06
    b.box((-col / 2 + 0.01, yc - 0.10, 0.0), (col / 2 - 0.01, yc + 0.10, 0.10), "AC_Trim", bevel=0.01)
    b.box((-col / 2, yc - 0.11, h - 0.10), (col / 2, yc + 0.11, h), "AC_Trim", bevel=0.01)
    z0, z1 = 0.10, h - 0.10
    r = d / 2
    H = z1 - z0
    prof = [(0.0, z0), (r * 1.3, z0), (r * 1.3, z0 + 0.03), (r * 1.05, z0 + 0.07), (r, z0 + 0.12)]
    for k in range(1, 10):
        t = k / 10
        prof.append((r * (1.0 - 0.12 * t), z0 + 0.12 + (H - 0.30) * t))
    prof += [(r * 0.98, z1 - 0.16), (r * 1.15, z1 - 0.14), (r * 1.15, z1 - 0.11), (r * 0.95, z1 - 0.09),
             (r * 1.25, z1 - 0.04), (r * 1.35, z1), (0.0, z1)]
    b.lathe(prof, 16, "AC_Trim", center=(0.0, yc, 0.0))


def window(w=1.5, h=1.95, kind='double', reveal=True):
    """Complete window unit for a w x h wall opening (bottom-left at x = -w/2, z = 0).

    1940s painted timber casements (AC_Frame) with glazing bars, glass (AC_Glass) at
    y = -0.12, plastered reveal liner (AC_Trim) to y = -0.5 and a zinc drip sill (AC_Zinc).
    kind: 'double' (2 side-hung sashes + split transom light), 'triple' (3-light stair
    window), 'french_door' (loggia door + sidelights + transom), 'arched' (round-headed,
    radial fanlight), 'shop' (large ground-floor shop window), 'palladian' (serliana: arched
    centre light/door, flat-topped side lights, white piers and archivolt, wall spandrels up
    to the rectangle w x h), 'paired' (two narrow windows with a turned colonnette between).
    reveal=False leaves out the reveal liner and the dark back (when the caller builds them)."""
    b = _B("window_" + kind)
    if kind in ('palladian', 'paired'):
        (_palladian if kind == 'palladian' else _paired)(b, w, h)
        if reveal:
            _reveal(b, _opening(w, h), REVEAL_DEPTH)
        _zinc_sill(b, w) if kind == 'paired' else None
        return b.finish()
    arched = kind == 'arched'
    fw = 0.09 if kind == 'shop' else 0.065
    outer = _opening(w, h, arched)
    if reveal:
        _reveal(b, outer, REVEAL_DEPTH)
        # back of the reveal: dark interior so the opening never shows the void
        b.quad([(-w / 2, -REVEAL_DEPTH + 0.01, 0), (-w / 2, -REVEAL_DEPTH + 0.01, h),
                (w / 2, -REVEAL_DEPTH + 0.01, h), (w / 2, -REVEAL_DEPTH + 0.01, 0)][::-1], "AC_Iron")
    # outer frame (box frame in the reveal)
    b.ring_prism(outer, _opening(w, h, arched, fw), -0.17, -0.08, "AC_Frame")
    ix0, ix1, iz0, iz1 = -w / 2 + fw, w / 2 - fw, fw, h - fw
    _glass(b, _opening(w, h, arched, fw))
    tb = 0.065  # transom / mullion width
    if kind == 'double':
        zt = iz1 - min(0.5, 0.26 * h)
        b.box((ix0, -0.16, zt - tb / 2), (ix1, -0.085, zt + tb / 2), "AC_Frame")
        b.box((-tb / 2, -0.16, zt + tb / 2), (tb / 2, -0.085, iz1), "AC_Frame")
        _sash(b, ix0, iz0, 0.0, zt - tb / 2, bars_h=(0.55,))
        _sash(b, 0.0, iz0, ix1, zt - tb / 2, bars_h=(0.55,))
        _sash(b, ix0, zt + tb / 2, -tb / 2, iz1, sw=0.04)
        _sash(b, tb / 2, zt + tb / 2, ix1, iz1, sw=0.04)
    elif kind == 'triple':
        zt = iz1 - min(0.5, 0.26 * h)
        b.box((ix0, -0.16, zt - tb / 2), (ix1, -0.085, zt + tb / 2), "AC_Frame")
        xs = [ix0 + (ix1 - ix0) * k / 3 for k in range(4)]
        for k in (1, 2):
            b.box((xs[k] - tb / 2, -0.16, iz0), (xs[k] + tb / 2, -0.085, iz1), "AC_Frame")
        for k in range(3):
            a0 = xs[k] + (tb / 2 if k else 0)
            a1 = xs[k + 1] - (tb / 2 if k < 2 else 0)
            _sash(b, a0, iz0, a1, zt - tb / 2, bars_h=(0.5,))
            _sash(b, a0, zt + tb / 2, a1, iz1, sw=0.04)
    elif kind == 'french_door':
        zt = iz1 - min(0.5, 0.2 * h)
        dw = min(0.9, 0.5 * (ix1 - ix0))
        b.box((ix0, -0.16, zt - tb / 2), (ix1, -0.085, zt + tb / 2), "AC_Frame")
        for s in (-1, 1):
            b.box((s * dw / 2 - tb / 2, -0.16, iz0 - fw + 0.03), (s * dw / 2 + tb / 2, -0.085, iz1),
                  "AC_Frame")
        panel = 0.85
        # door leaves (pair of narrow leaves)
        for x0, x1 in ((-dw / 2 + tb / 2, 0.0), (0.0, dw / 2 - tb / 2)):
            _sash(b, x0, 0.03, x1, zt - tb / 2, sw=0.06, bars_h=(0.62,))
            b.box((x0 + 0.06, -0.14, 0.09), (x1 - 0.06, -0.10, panel), "AC_Frame")
            b.box((x0 + 0.06, -0.15, panel - 0.03), (x1 - 0.06, -0.09, panel + 0.03), "AC_Frame")
        # sidelights with a low panel
        for x0, x1 in ((ix0, -dw / 2 - tb / 2), (dw / 2 + tb / 2, ix1)):
            _sash(b, x0, iz0, x1, zt - tb / 2, sw=0.05, bars_h=(0.62,))
            b.box((x0 + 0.05, -0.14, iz0 + 0.05), (x1 - 0.05, -0.10, panel), "AC_Frame")
            b.box((x0 + 0.05, -0.15, panel - 0.03), (x1 - 0.05, -0.09, panel + 0.03), "AC_Frame")
        for x0, x1 in ((ix0, -dw / 2 - tb / 2), (-dw / 2 + tb / 2, dw / 2 - tb / 2), (dw / 2 + tb / 2, ix1)):
            _sash(b, x0, zt + tb / 2, x1, iz1, sw=0.04)
        # threshold instead of a sill
        b.box((-w / 2, -0.20, 0.0), (w / 2, 0.02, 0.03), "AC_Frame", bevel=0.005)
    elif kind == 'arched':
        r = w / 2
        zs = h - r
        b.box((ix0, -0.16, zs - tb / 2), (ix1, -0.085, zs + tb / 2), "AC_Frame")
        _sash(b, ix0, iz0, 0.0, zs - tb / 2, bars_h=(0.5,))
        _sash(b, 0.0, iz0, ix1, zs - tb / 2, bars_h=(0.5,))
        rin = r - fw
        for a in (math.pi / 4, math.pi / 2, 3 * math.pi / 4):
            b.bar((0.0, zs), (rin * math.cos(a) * 1.02, zs + rin * math.sin(a) * 1.02), 0.035,
                  -0.15, -0.09, "AC_Frame")
        # small hub over the transom
        hub = _arc(0.0, zs + tb / 2, 0.12, 0.0, math.pi, 8)
        b.ring_prism(hub, _arc(0.0, zs + tb / 2, 0.09, 0.0, math.pi, 8), -0.15, -0.09, "AC_Frame")
    elif kind == 'shop':
        # refs/dima/shop.jpg: a narrow top light (the blind box behind it), below it four big lights
        # on a thin cross
        zt = iz0 + (iz1 - iz0) * 0.86
        zm = iz0 + (zt - iz0) * 0.5
        b.box((ix0, -0.17, zt - 0.04), (ix1, -0.08, zt + 0.04), "AC_Frame")
        b.box((-0.03, -0.17, iz0), (0.03, -0.08, zt), "AC_Frame")
        b.box((ix0, -0.165, zm - 0.03), (ix1, -0.085, zm + 0.03), "AC_Frame")
        for x0, x1 in ((ix0, -0.03), (0.03, ix1)):
            for z0_, z1_ in ((iz0, zm - 0.03), (zm + 0.03, zt - 0.04)):
                _sash(b, x0, z0_, x1, z1_, sw=0.025, y0=-0.135, y1=-0.105)
    else:
        raise ValueError(kind)
    if kind != 'french_door':
        _zinc_sill(b, w)
    return b.finish()


def _small_bracket(b, x, ztop, w, h, d, mat):
    """Small scrolled bracket under a hood or sill: top at ztop, hanging h, projecting d."""
    prof = [(0.0, 0.0), (0.0, -h), (0.25 * d, -h), (0.4 * d, -0.8 * h), (0.55 * d, -0.45 * h),
            (0.85 * d, -0.25 * h), (d, -0.1 * h), (d, 0.0)]
    pts = _chaikin(prof, [True, True, True, False, False, False, False, True], iters=2)
    pts = [(y, z + ztop) for (y, z) in pts]
    b.prism(pts, x - w / 2, x + w / 2, mat, axis='x', bevel=0.005)


def window_surround(w=1.5, h=1.95, style='band'):
    """White plaster surround (AC_Trim) on the wall face around a w x h opening (same
    convention as window()). style: 'band' (flat 0.18 m band), 'ears' (two-step architrave with
    crossettes + sill shelf), 'hood' (architrave + frieze + straight hood cornice / сандрик +
    sill shelf), 'hood_ears' (ears architrave + hood cornice on two small brackets + sill on
    brackets), 'pediment' (ears architrave, frieze, cornice and a triangular pediment, sill on
    brackets; 1946 elevation), 'arch' (round-headed band with imposts and keystone; the opening
    top is a semicircle of radius w/2 with its crown at h)."""
    b = _B("surround_" + style)
    M = "AC_Trim"
    if style == 'band':
        bw = 0.18
        b.ring_prism(_opening(w, h, False, -bw), _opening(w, h), 0.0, 0.035, M, bevel=0.008)
    elif style in ('ears', 'hood', 'hood_ears', 'pediment'):
        f1, f2 = 0.10, 0.16
        ears = style in ('ears', 'hood_ears', 'pediment')
        b.ring_prism(_opening(w, h, False, -f1), _opening(w, h), 0.0, 0.03, M, bevel=0.006)
        b.ring_prism(_opening(w, h, False, -f2), _opening(w, h, False, -f1), 0.0, 0.05, M, bevel=0.008)
        e = 0.07
        if ears:
            for s in (-1, 1):
                x0 = s * (w / 2 + f2)
                b.box((min(x0, x0 + s * e), 0.0, h - 0.10), (max(x0, x0 + s * e), 0.05, h + f2), M,
                      bevel=0.008)
            if style == 'ears':
                for s in (-1, 1):
                    x0 = s * (w / 2 + f2)
                    b.box((min(x0, x0 + s * e), 0.0, -f2), (max(x0, x0 + s * e), 0.05, 0.10), M,
                          bevel=0.008)
        top = h + f2
        if style in ('hood', 'hood_ears', 'pediment'):
            fz = 0.16 if style == 'hood' else 0.20
            half = w / 2 + f2 + (e if ears else 0.0)
            b.box((-half, 0.0, top), (half, 0.03, top + fz), M, bevel=0.006)
            if style == 'hood_ears':
                for s in (-1, 1):
                    _small_bracket(b, s * (half - 0.05), top + fz, 0.10, fz + 0.12, 0.17, M)
            zc = top + fz
            core = half + 0.03
            prof = [(o, z + zc) for (o, z) in hood_cornice_profile_points()]
            b.sweep_plan(prof, [(-core, -0.2), (-core, 0.0), (core, 0.0), (core, -0.2)], M)
            if style == 'pediment':
                zp = zc + 0.20
                span = core + 0.16
                rise = span * math.tan(math.radians(22))
                b.prism([(-span + 0.05, zp), (span - 0.05, zp), (0.0, zp + rise - 0.08)], 0.0, 0.03, M)
                rp = [(0.0, 0.0), (0.16, 0.0), (0.16, 0.06), (0.17, 0.07), (0.17, 0.10), (0.14, 0.13),
                      (0.0, 0.15)]
                for s in (-1, 1):
                    # raking cornice: a profile swept along x, then sheared up to the apex
                    b.begin()
                    L = span + 0.02
                    A = [b.v((0.0, o, z)) for (o, z) in rp]
                    Bv = [b.v((s * L, o, z)) for (o, z) in rp]
                    b.f(list(reversed(A)), M)
                    b.f(Bv, M)
                    n = len(rp)
                    for i in range(n):
                        j = (i + 1) % n
                        b.f([A[i], A[j], Bv[j], Bv[i]], M)
                    tanv = math.tan(math.radians(22))
                    sh = Matrix(((1, 0, 0, 0), (0, 1, 0, 0), (-s * tanv, 0, 1, zp + rise - 0.05), (0, 0, 0, 1)))
                    b.end(recalc=True, xform=sh)
        # sill shelf with a small cyma (on brackets for hood_ears / pediment)
        sw = w / 2 + f2 + 0.06
        z = -f2
        prof = [(0.0, z - 0.10), (0.03, z - 0.10), (0.05, z - 0.07), (0.075, z - 0.05),
                (0.075, z), (0.0, z)]
        b.sweep_plan(prof, [(-sw, -0.15), (-sw, 0.0), (sw, 0.0), (sw, -0.15)], M)
        if style in ('hood_ears', 'pediment'):
            for s in (-1, 1):
                _small_bracket(b, s * (sw - 0.12), z - 0.10, 0.09, 0.22, 0.07, M)
    elif style == 'arch':
        bw = 0.18
        r = w / 2
        zs = h - r
        b.prism(_arch_band(w, h, 0.0, bw), 0.0, 0.04, M, bevel=0.008)
        for s in (-1, 1):
            x0, x1 = sorted((s * (w / 2 - 0.02), s * (w / 2 + bw + 0.05)))
            b.box((x0, 0.0, zs - 0.18), (x1, 0.065, zs), M, bevel=0.01)
        # keystone (trapezoid)
        kt = [(-0.08, h - 0.04), (0.08, h - 0.04), (0.11, h + bw + 0.06), (-0.11, h + bw + 0.06)]
        b.prism(kt, 0.0, 0.07, M, bevel=0.008)
    else:
        raise ValueError(style)
    return b.finish()


def _door_leaf(b, x0, x1, z0, z1, glazed_frac=0.55, mat="AC_Door", n_vbar=1, n_hbar=1, lock_z=1.05):
    """Panelled leaf in the opening plane: stiles/rails y -0.24..-0.18, recessed lower panel,
    glazed upper panel with muntins. Glass at y = -0.21."""
    y0, y1 = -0.24, -0.18
    st, tr, br, lr = 0.10, 0.10, 0.20, 0.12
    b.box((x0, y0, z0), (x0 + st, y1, z1), mat)
    b.box((x1 - st, y0, z0), (x1, y1, z1), mat)
    b.box((x0 + st, y0, z0), (x1 - st, y1, z0 + br), mat)
    b.box((x0 + st, y0, z1 - tr), (x1 - st, y1, z1), mat)
    zl = max(lock_z, z0 + br + 0.3)
    b.box((x0 + st, y0, zl - lr / 2), (x1 - st, y1, zl + lr / 2), mat)
    # lower panel: recessed field with a raised fielded centre
    px0, px1 = x0 + st, x1 - st
    b.box((px0, y0 + 0.01, z0 + br), (px1, y1 - 0.025, zl - lr / 2), mat)
    b.box((px0 + 0.05, y1 - 0.03, z0 + br + 0.05), (px1 - 0.05, y1 - 0.012, zl - lr / 2 - 0.05), mat,
          bevel=0.008)
    # upper: glazed
    gz0, gz1 = zl + lr / 2, z1 - tr
    _glass(b, [(px0, gz0), (px1, gz0), (px1, gz1), (px0, gz1)], y=-0.21)
    for k in range(1, n_vbar + 1):
        x = px0 + (px1 - px0) * k / (n_vbar + 1)
        b.box((x - 0.018, -0.225, gz0), (x + 0.018, -0.19, gz1), mat)
    for k in range(1, n_hbar + 1):
        z = gz0 + (gz1 - gz0) * k / (n_hbar + 1)
        b.box((px0, -0.225, z - 0.018), (px1, -0.19, z + 0.018), mat)
    # glazing beads
    b.box((px0, -0.20, gz0), (px1, -0.18, gz0 + 0.02), mat)


def door(w=1.5, h=2.6, kind='double', reveal=True):
    """Entrance door unit for a w x h opening (same convention as window()).

    'double': pair of dark panelled leaves (AC_Door) with glazed upper panels, frame, threshold.
    'arched': pair of leaves under a round-headed glazed fanlight; the opening top is a
    semicircle of radius w/2 with its crown at h.
    reveal=False leaves out the reveal liner and dark back. Steps are a separate mesh: steps()."""
    b = _B("door_" + kind)
    arched = kind == 'arched'
    fw = 0.08
    outer = _opening(w, h, arched)
    if reveal:
        _reveal(b, outer, REVEAL_DEPTH)
        b.quad([(-w / 2, -REVEAL_DEPTH + 0.01, 0), (-w / 2, -REVEAL_DEPTH + 0.01, h),
                (w / 2, -REVEAL_DEPTH + 0.01, h), (w / 2, -REVEAL_DEPTH + 0.01, 0)][::-1], "AC_Iron")
    ix0, ix1 = -w / 2 + fw, w / 2 - fw
    if not arched:
        b.box((-w / 2, -0.27, 0.0), (ix0, -0.15, h), "AC_Door")
        b.box((ix1, -0.27, 0.0), (w / 2, -0.15, h), "AC_Door")
        b.box((ix0, -0.27, h - fw), (ix1, -0.15, h), "AC_Door")
        top = h - fw
    else:
        r = w / 2
        zs = h - r
        b.ring_prism(outer, _opening(w, h, True, fw), -0.27, -0.15, "AC_Door")
        top = zs - 0.03
        b.box((ix0, -0.27, top), (ix1, -0.15, zs + 0.05), "AC_Door")      # transom
        fan = _arc(0.0, zs + 0.05, r - fw, 0.0, math.pi, 16)
        _glass(b, fan, y=-0.21)
        for a in (math.pi / 4, math.pi / 2, 3 * math.pi / 4):
            b.bar((0.0, zs + 0.05), ((r - fw) * math.cos(a), zs + 0.05 + (r - fw) * math.sin(a)), 0.03,
                  -0.23, -0.18, "AC_Door")
        hub = _arc(0.0, zs + 0.05, 0.14, 0.0, math.pi, 8)
        b.ring_prism(hub, _arc(0.0, zs + 0.05, 0.10, 0.0, math.pi, 8), -0.23, -0.18, "AC_Door")
    lw = (ix1 - ix0) / 2
    lock = min(1.05, 0.45 * top)
    _door_leaf(b, ix0, ix0 + lw - 0.003, 0.03, top - 0.005, lock_z=lock)
    _door_leaf(b, ix0 + lw + 0.003, ix1, 0.03, top - 0.005, lock_z=lock)
    # meeting-stile astragal + handles
    b.box((-0.02, -0.18, 0.05), (0.02, -0.165, top - 0.02), "AC_Door")
    for s in (-1, 1):
        b.box((s * 0.07 - 0.012, -0.175, 0.95), (s * 0.07 + 0.012, -0.14, 1.25), "AC_Iron", bevel=0.004)
    # threshold
    b.box((-w / 2, -0.30, 0.0), (w / 2, 0.02, 0.03), "AC_Stone", bevel=0.006)
    return b.finish()


def steps(w=2.0, n=2, rise=0.15, tread=0.32):
    """Concrete entrance steps (AC_Concrete) against the wall: top step at z = n*rise, steps
    run out to y = n*tread, x = -w/2..w/2."""
    b = _B("steps")
    for i in range(n):
        y1 = tread * (n - i)
        b.box((-w / 2, -0.02, rise * i), (w / 2, y1, rise * (i + 1)), "AC_Concrete", bevel=0.012)
    return b.finish()


# --------------------------------------------------------------------------------------------
# portals

def portal_dark(w=1.6, h=2.6):
    """Novikova 1 central portal (AC_RusticDark): rough-faced block jambs of alternating
    width (0.55 / 0.40 m) and a stepped fan of voussoirs over the door head; door w x h."""
    b = _B("portal_dark")
    M = "AC_RusticDark"
    gap = 0.018
    D = 0.12
    nc = 5
    ch = h / nc
    seed = 0.0
    for s in (-1, 1):
        for i in range(nc):
            wid = 0.55 if i % 2 == 0 else 0.40
            xa, xb = w / 2 + gap / 2, w / 2 + wid - gap / 2
            if s < 0:
                xa, xb = -xb, -xa
            z0, z1 = i * ch + gap / 2, (i + 1) * ch - gap / 2
            fr = [(xa, D, z0), (xb, D, z0), (xb, D, z1), (xa, D, z1)]
            bk = [(xa, 0, z0), (xb, 0, z0), (xb, 0, z1), (xa, 0, z1)]
            seed += 1.37
            b.rock(fr, bk, M, mode='rock', seed=seed, rough=0.05, chamfer=0.03, cell=0.05)
    # lintel course: 2 side blocks + a keystone
    lz0, lz1 = h + gap / 2, h + 0.42
    xo = w / 2 + 0.55 - gap / 2
    kw = 0.36
    for (xa, xb) in ((-xo, -kw / 2 - gap / 2), (kw / 2 + gap / 2, xo)):
        fr = [(xa, D, lz0), (xb, D, lz0), (xb, D, lz1), (xa, D, lz1)]
        bk = [(xa, 0, lz0), (xb, 0, lz0), (xb, 0, lz1), (xa, 0, lz1)]
        seed += 1.1
        b.rock(fr, bk, M, seed=seed, rough=0.05, chamfer=0.03, cell=0.05)
    # stepped fan of voussoirs radiating from a centre below the door head
    cz = h - 0.25
    nv = 7
    a0, a1 = math.radians(28), math.radians(152)
    tops = [0.95, 1.05, 1.15, 1.25, 1.15, 1.05, 0.95]   # height above cz of the outer end
    for k in range(nv):
        aa = a0 + (a1 - a0) * k / nv + 0.012
        ab = a0 + (a1 - a0) * (k + 1) / nv - 0.012
        r_in = (lz1 + gap - cz)
        def pt(a, r):
            return (r * math.cos(a), cz + r * math.sin(a))
        top = cz + tops[k] + 0.15
        # inner points sit on the lintel top line, outer ends are cut horizontal (stepped)
        pa_in = (r_in / math.tan(aa) if abs(math.tan(aa)) > 1e-6 else 0, lz1 + gap)
        pb_in = (r_in / math.tan(ab), lz1 + gap)
        rr_a = (top - cz) / math.sin(aa)
        rr_b = (top - cz) / math.sin(ab)
        pa_out = pt(aa, rr_a)
        pb_out = pt(ab, rr_b)
        # order bl, br, tr, tl with x increasing along the bottom
        bl, br = (pb_in, pa_in) if pb_in[0] < pa_in[0] else (pa_in, pb_in)
        tl, tr = (pb_out, pa_out) if pb_out[0] < pa_out[0] else (pa_out, pb_out)
        fr = [(bl[0], D, bl[1]), (br[0], D, br[1]), (tr[0], D, tr[1]), (tl[0], D, tl[1])]
        bk = [(bl[0], 0, bl[1]), (br[0], 0, br[1]), (tr[0], 0, tr[1]), (tl[0], 0, tl[1])]
        seed += 0.9
        b.rock(fr, bk, M, seed=seed, rough=0.03, chamfer=0.022, cell=0.06)
    # keystone of the lintel, slightly proud
    fr = [(-kw / 2, D + 0.03, lz0), (kw / 2, D + 0.03, lz0), (kw / 2, D + 0.03, lz1), (-kw / 2, D + 0.03, lz1)]
    bk = [(-kw / 2, 0, lz0), (kw / 2, 0, lz0), (kw / 2, 0, lz1), (-kw / 2, 0, lz1)]
    b.rock(fr, bk, M, seed=99.0, rough=0.03, chamfer=0.025, cell=0.06)
    return b.finish()


def portal_side(w=1.3, h=2.6, arched=True):
    """Side-building street portal around a w x h door (1946 elevation, m24_2015_houses_06):
    arched=True (default): round-headed opening (semicircle radius w/2, crown at h), thin
    moulded arch band (AC_Trim), jambs of alternating smooth chamfered rusticated blocks
    (AC_Quoin), radiating voussoirs with alternating lengths and a big projecting keystone.
    arched=False: flat-headed variant of panoramio_58 (architrave, blocks, moulded hood slab)."""
    if arched:
        return _portal_side_arched(w, h)
    b = _B("portal_side")
    T, Q = "AC_Trim", "AC_Quoin"
    a1, a2 = 0.08, 0.15
    b.ring_prism(_opening(w, h + 0.0, False, -a1), _opening(w, h), 0.0, 0.05, T, bevel=0.006)
    # architrave: open at the bottom -> build legs + head as boxes on top of the ring
    b.box((-w / 2 - a2, 0.0, 0.0), (-w / 2 - a1, 0.075, h + a2), T, bevel=0.008)
    b.box((w / 2 + a1, 0.0, 0.0), (w / 2 + a2, 0.075, h + a2), T, bevel=0.008)
    b.box((-w / 2 - a1, 0.0, h + a1), (w / 2 + a1, 0.075, h + a2), T, bevel=0.008)
    # rusticated jambs
    courses = [(0.0, 0.42), (0.42, 1.12), (1.12, 1.70), (1.70, h + a2 + 0.02)]
    D = 0.09
    gap = 0.02
    for s in (-1, 1):
        for i, (z0, z1) in enumerate(courses):
            wid = 0.40 if i % 2 == 0 else 0.28
            xa, xb = w / 2 + a2, w / 2 + a2 + wid
            if s < 0:
                xa, xb = -xb, -xa
            z0g, z1g = z0 + (gap / 2 if i else 0.0), z1 - gap / 2
            fr = [(xa, D, z0g), (xb, D, z0g), (xb, D, z1g), (xa, D, z1g)]
            bk = [(xa, 0, z0g), (xb, 0, z0g), (xb, 0, z1g), (xa, 0, z1g)]
            b.rock(fr, bk, Q, mode='smooth', chamfer=0.03, cell=1.0,
                   edges=(i > 0, True, True, True))
    # frieze + hood slab
    hz = courses[-1][1]
    half = w / 2 + a2 + 0.40
    b.box((-half, 0.0, hz), (half, 0.06, hz + 0.18), T, bevel=0.008)
    prof = [(o, z + hz + 0.18) for (o, z) in hood_cornice_profile_points()]
    prof = [(o * 1.6, z0 + (z - hz - 0.18) * 1.35) for (o, z), z0 in zip(prof, [hz + 0.18] * len(prof))]
    core = half + 0.02
    b.sweep_plan(prof, [(-core, -0.2), (-core, 0.0), (core, 0.0), (core, -0.2)], T)
    return b.finish()


def _portal_side_arched(w, h):
    b = _B("portal_side")
    T, Q = "AC_Trim", "AC_Quoin"
    r = w / 2
    zs = h - r
    band = 0.08
    D = 0.09
    gap = 0.02
    # moulded arch band in the reveal edge
    b.prism(_arch_band(w, h, 0.0, band), 0.0, 0.05, T, bevel=0.006)
    r0 = r + band
    nc = 4
    ch = zs / nc
    for s in (-1, 1):
        for i in range(nc):
            wid = 0.46 if i % 2 == 0 else 0.32
            xa, xb = r0, r0 + wid
            if s < 0:
                xa, xb = -xb, -xa
            z0 = i * ch + (gap / 2 if i else 0.0)
            z1 = (i + 1) * ch - gap / 2
            fr = [(xa, D, z0), (xb, D, z0), (xb, D, z1), (xa, D, z1)]
            bk = [(xa, 0, z0), (xb, 0, z0), (xb, 0, z1), (xa, 0, z1)]
            b.rock(fr, bk, Q, mode='smooth', chamfer=0.03, cell=1.0, edges=(i > 0, True, True, True))
    # voussoirs: 4 a side + keystone; outer ends alternate long/short like the jambs
    nv = 9
    da = math.pi / nv
    for k in range(nv):
        a0 = k * da + 0.012
        a1 = (k + 1) * da - 0.012
        key = k == nv // 2
        ro = r0 + (0.62 if key else (0.46 if k % 2 == 0 else 0.34))
        if key:
            a0 -= 0.06
            a1 += 0.06
            ri = r0 - band + 0.0
            pts = [(ri * math.cos(a0 + 0.06), zs + ri * math.sin(a0 + 0.06)),
                   (ri * math.cos(a1 - 0.06), zs + ri * math.sin(a1 - 0.06)),
                   (ro * math.cos(a1), zs + ro * math.sin(a1)), (ro * math.cos(a0), zs + ro * math.sin(a0))]
            dd = D + 0.04
        else:
            ri = r0
            pts = [(ri * math.cos(a0), zs + ri * math.sin(a0)), (ri * math.cos(a1), zs + ri * math.sin(a1)),
                   (ro * math.cos(a1), zs + ro * math.sin(a1)), (ro * math.cos(a0), zs + ro * math.sin(a0))]
            dd = D
        # rock() wants (bl, br, tr, tl): inner edge is the 'bottom'
        pts = [pts[1], pts[0], pts[3], pts[2]] if pts[1][0] < pts[0][0] else pts
        fr = [(p[0], dd, p[1]) for p in pts]
        bk = [(p[0], 0, p[1]) for p in pts]
        b.rock(fr, bk, Q, mode='smooth', chamfer=0.03, cell=1.0)
    # impost blocks at the springing (slightly proud)
    for s in (-1, 1):
        xa, xb = sorted((s * (r - 0.02), s * (r0 + 0.50)))
        b.box((xa, 0.0, zs - 0.12), (xb, D + 0.03, zs), T, bevel=0.01)
    return b.finish()


# --------------------------------------------------------------------------------------------
# loggia-bay ornament

def console_big(h=0.8, w=0.4, depth=None):
    """Large carved console / ancone under a balcony slab (AC_Trim). Back on the wall (y = 0),
    top at z = h carries the slab; big scroll at the top front, small scroll at the bottom,
    spiral volutes on both sides and a raised leaf strip down the front."""
    b = _B("console_big")
    M = "AC_Trim"
    D = depth if depth is not None else 0.5 * h
    prof = [(0.0, 0.0), (0.20 * D, 0.0), (0.33 * D, 0.04 * h), (0.38 * D, 0.13 * h), (0.32 * D, 0.24 * h),
            (0.30 * D, 0.32 * h), (0.42 * D, 0.46 * h), (0.66 * D, 0.58 * h), (0.88 * D, 0.66 * h),
            (1.0 * D, 0.76 * h), (1.0 * D, 0.86 * h), (0.0, 0.86 * h)]
    sharp = [True, False, False, False, False, False, False, False, False, False, True, True]
    pts = _chaikin(prof, sharp, iters=3, closed=True)
    b.prism(pts, -w / 2, w / 2, M, axis='x', bevel=0.008)
    # abacus / top block
    b.box((-w / 2 - 0.02, 0.0, 0.86 * h), (w / 2 + 0.02, D + 0.04, h), M, bevel=0.008)
    b.box((-w / 2 - 0.01, 0.0, 0.86 * h - 0.025), (w / 2 + 0.01, D + 0.015, 0.86 * h), M)
    # side volutes (raised spiral ribs)
    for s in (-1, 1):
        x = s * w / 2
        fr = ((x, 0, 0), (0, 1, 0), (0, 0, 1), (s, 0, 0))
        sp = _spiral(0.70 * D, 0.62 * h, 0.012 * h, 0.13 * h, -0.5 * math.pi, 1.6, 40)
        b.flat_bar(sp, fr, 0.022, 0.014, M)
        b.flat_bar(_spiral(0.70 * D, 0.62 * h, 0.0, 0.03 * h, 0, 1.0, 10), fr, 0.03, 0.02, M)
        sp2 = _spiral(0.20 * D, 0.13 * h, 0.01 * h, 0.075 * h, 0.5 * math.pi, 1.4, 30, cw=True)
        b.flat_bar(sp2, fr, 0.018, 0.012, M)
        # a ridge connecting the two volutes along the curve
        rib = [(0.30 * D, 0.24 * h), (0.36 * D, 0.40 * h), (0.52 * D, 0.50 * h)]
        b.flat_bar(_chaikin(rib, iters=2, closed=False), fr, 0.02, 0.012, M)
    # front leaf strip following the front curve (offset outward)
    front = [p for p in pts if p[1] > 0.1 * h and p[1] < 0.84 * h and p[0] > 0.25 * D]
    front.sort(key=lambda p: p[1])
    outer, nls = [], []
    for i, p in enumerate(front):
        a = Vector(front[max(i - 1, 0)])
        c = Vector(front[min(i + 1, len(front) - 1)])
        t = (c - a).normalized()
        n = Vector((t.y, -t.x))
        if n.x < 0:
            n = -n
        bulge = 0.012 + 0.012 * math.sin(math.pi * i / max(1, len(front) - 1))
        outer.append((p[0] + n.x * bulge, p[1] + n.y * bulge))
        nls.append(n)
    poly = [(p[0] - 0.01, p[1]) for p in front] + list(reversed(outer))
    b.prism(poly, -0.28 * w, 0.28 * w, M, axis='x')
    # two beads along the edges of the leaf strip and a central rib: reads as carving head-on
    rib = [(p[0] + nn.x * 0.02, p[1] + nn.y * 0.02) for p, nn in zip(outer, nls)]
    poly2 = [(p[0] - 0.005, p[1]) for p in outer] + list(reversed(rib))
    for xa, xb in ((-0.30 * w, -0.22 * w), (0.22 * w, 0.30 * w), (-0.03 * w, 0.03 * w)):
        b.prism(poly2, xa, xb, M, axis='x')
    # rosette on the front of the big scroll
    top = outer[int(len(outer) * 0.8)]
    nn = nls[int(len(outer) * 0.8)]
    b.lathe([(0, 0), (0.06, 0), (0.05, 0.02), (0.025, 0.035), (0, 0.04)], 12, M,
            xform=Matrix.Translation((0, top[0] + nn.x * 0.01, top[1] + nn.y * 0.01))
            @ Matrix.Rotation(math.atan2(nn.y, nn.x), 4, 'X') @ Matrix.Rotation(-math.pi / 2, 4, 'X'))
    # leaf ribs across the strip
    for i in range(2, len(outer) - 2, 4):
        p = outer[i]
        b.box((-0.2 * w, p[0] - 0.02, p[1] - 0.008), (0.2 * w, p[0] + 0.012, p[1] + 0.008), M)
    return b.finish()


def column(h=2.9, d=0.22):
    """Slender Tuscan column (AC_Trim), axis at x = y = 0, from z = 0 to h: square plinth, torus,
    shaft with entasis, astragal, echinus and square abacus."""
    b = _B("column")
    M = "AC_Trim"
    r = d / 2
    pb = 0.06       # plinth height
    ab = 0.055      # abacus height
    b.box((-0.7 * d, -0.7 * d, 0.0), (0.7 * d, 0.7 * d, pb), M, bevel=0.005)
    b.box((-0.68 * d, -0.68 * d, h - ab), (0.68 * d, 0.68 * d, h), M, bevel=0.005)
    z0, z1 = pb, h - ab
    prof = [(0.0, z0), (0.62 * d, z0)]
    prof += [(r * 1.18 + 0.045 * d * math.sin(math.pi * k / 6), z0 + 0.06 * d * (1 - math.cos(math.pi * k / 6)) / 2 * 2)
             for k in range(1, 6)]
    prof += [(r * 1.08, z0 + 0.13 * d), (r, z0 + 0.17 * d)]
    shaft0, shaft1 = z0 + 0.17 * d, z1 - 0.42 * d
    for k in range(1, 9):
        t = k / 8
        ent = 1.0 - 0.15 * max(0.0, (t - 0.33) / 0.67) ** 1.5
        prof.append((r * ent, shaft0 + (shaft1 - shaft0) * t))
    rt = r * 0.85
    prof += [(rt * 1.12, shaft1 + 0.02 * d), (rt * 1.12, shaft1 + 0.07 * d), (rt, shaft1 + 0.09 * d),
             (rt, z1 - 0.17 * d), (rt * 1.25, z1 - 0.12 * d), (rt * 1.45, z1 - 0.05 * d),
             (rt * 1.5, z1), (0.0, z1)]
    b.lathe(prof, 20, M)
    return b.finish()


def pilaster(h=2.9, w=0.4, depth=0.08):
    """Loggia-bay pilaster on the wall (AC_Trim): moulded base and capital, shaft with a sunk
    panel carrying a low-relief candelabrum (stem, vases, scroll pairs, leaves)."""
    b = _B("pilaster")
    M = "AC_Trim"
    hb, hc = 0.22, 0.26
    # base: plinth + torus-ish step
    b.box((-w / 2 - 0.04, 0.0, 0.0), (w / 2 + 0.04, depth + 0.04, 0.12), M, bevel=0.006)
    b.box((-w / 2 - 0.02, 0.0, 0.12), (w / 2 + 0.02, depth + 0.02, hb), M, bevel=0.02, seg=2)
    # shaft frame (left/right stiles + top/bottom rails) around the sunk panel
    fr = 0.06
    z0, z1 = hb, h - hc
    b.box((-w / 2, 0.0, z0), (-w / 2 + fr, depth, z1), M)
    b.box((w / 2 - fr, 0.0, z0), (w / 2, depth, z1), M)
    b.box((-w / 2 + fr, 0.0, z0), (w / 2 - fr, depth, z0 + fr), M)
    b.box((-w / 2 + fr, 0.0, z1 - fr), (w / 2 - fr, depth, z1), M)
    b.box((-w / 2 + fr, 0.0, z0 + fr), (w / 2 - fr, depth - 0.025, z1 - fr), M)
    # capital: astragal, neck, echinus-ish steps, abacus
    b.box((-w / 2 - 0.01, 0.0, z1), (w / 2 + 0.01, depth + 0.012, z1 + 0.03), M, bevel=0.008)
    b.box((-w / 2, 0.0, z1 + 0.03), (w / 2, depth, z1 + 0.15), M)
    b.box((-w / 2 - 0.03, 0.0, z1 + 0.15), (w / 2 + 0.03, depth + 0.03, z1 + 0.20), M, bevel=0.012, seg=2)
    b.box((-w / 2 - 0.05, 0.0, z1 + 0.20), (w / 2 + 0.05, depth + 0.05, h), M, bevel=0.006)
    # neck rosettes
    for s in (-1, 0, 1):
        b.lathe([(0, 0), (0.03, 0), (0.025, 0.012), (0, 0.016)], 10, M,
                xform=None, center=(0, 0, 0))
        vs = b.bm.verts[-12:] if False else None
    # (rosettes are placed below via a transformed lathe)
    b.bm.verts.ensure_lookup_table()
    # relief: stem + vases + scroll pairs, on the panel face y = depth - 0.025
    yf = depth - 0.025
    pw = w - 2 * fr
    pz0, pz1 = z0 + fr + 0.05, z1 - fr - 0.05
    frame = ((0, yf, 0), (1, 0, 0), (0, 0, 1), (0, 1, 0))
    b.flat_bar([(0.0, pz0), (0.0, pz1)], frame, 0.022, 0.012, M)
    H = pz1 - pz0
    m_rel = Matrix.Diagonal((1.0, 0.35, 1.0, 1.0))
    for k, t in enumerate((0.06, 0.40, 0.74)):
        zc = pz0 + H * t
        vh = 0.16
        prof = [(0, zc), (0.05 * pw, zc), (0.06 * pw, zc + 0.02), (0.25 * pw, zc + 0.07),
                (0.22 * pw, zc + 0.11), (0.10 * pw, zc + 0.13), (0.14 * pw, zc + vh), (0, zc + vh)]
        b.lathe(prof, 10, M, a0=0.0, a1=math.pi, recalc=False,
                xform=Matrix.Translation((0, yf, 0)) @ m_rel @ Matrix.Rotation(math.pi / 2, 4, 'Z')
                @ Matrix.Rotation(0, 4, 'X'))
        # scroll pair above each vase
        for s in (-1, 1):
            zs = zc + vh + 0.05
            sp = _spiral(s * 0.27 * pw, zs + 0.10, 0.005, 0.055, (math.pi if s > 0 else 0.0), 1.1,
                         26, cw=(s > 0))
            sp = [(0.0, zs)] + list(reversed(sp))[::-1]
            b.flat_bar(sp, frame, 0.016, 0.012, M)
            # leaf
            leaf = [(0.0, zs + 0.18), (s * 0.12 * pw, zs + 0.24), (s * 0.30 * pw, zs + 0.30)]
            b.flat_bar(_chaikin(leaf, iters=2, closed=False), frame, [0.02, 0.03, 0.02, 0.012, 0.006, 0.004, 0.004][:len(_chaikin(leaf, iters=2, closed=False))], 0.01, M)
    # mask / rosette at the top of the panel
    b.lathe([(0, 0), (0.07, 0), (0.06, 0.02), (0.03, 0.035), (0, 0.04)], 12, M,
            xform=Matrix.Translation((0, yf, pz1 - 0.06)) @ Matrix.Rotation(-math.pi / 2, 4, 'X'))
    return b.finish()


def balcony_railing(length=3.0, h=1.0, style='bars'):
    """Balcony railing (AC_Iron) along x = -length/2..length/2 at y = 0..0.05, z = 0..h.

    'bars': flat top rail, bottom rail, round vertical bars at ~0.12 m, end posts.
    'ornamental': same frame with cast lyre panels (back-to-back C-scrolls) between bars."""
    b = _B("railing_" + style)
    M = "AC_Iron"
    L = length / 2
    yc = 0.025
    b.box((-L, 0.0, h - 0.04), (L, 0.05, h), M, bevel=0.006)          # handrail
    b.box((-L, 0.012, 0.06), (L, 0.038, 0.10), M)                      # bottom rail
    b.box((-L, 0.015, h - 0.12), (L, 0.035, h - 0.10), M)               # intermediate rail
    for s in (-1, 1):
        b.box((s * L - 0.02, 0.005, 0.0), (s * L + 0.02, 0.045, h), M)
    if style == 'bars':
        n = max(2, int(round(length / 0.12)))
        for i in range(1, n):
            x = -L + length * i / n
            b.cyl(x, yc, 0.0, h - 0.04, 0.008, M, sides=6)
    elif style == 'ornamental':
        unit = 0.6
        n = max(1, int(round(length / unit)))
        uw = length / n
        fr = ((0, yc - 0.008, 0), (1, 0, 0), (0, 0, 1), (0, 1, 0))
        for i in range(n):
            x0 = -L + uw * i
            for k in (0.12, 0.88):
                b.cyl(x0 + uw * k, yc, 0.08, h - 0.10, 0.008, M, sides=6)
            if i:
                b.box((x0 - 0.012, 0.01, 0.06), (x0 + 0.012, 0.04, h - 0.04), M)
            cx = x0 + uw / 2
            # lyre: two back-to-back C scrolls + small S at top/bottom
            for s in (-1, 1):
                pts = _spiral(cx + s * 0.07, 0.62 * h, 0.01, 0.10, (0 if s > 0 else math.pi), 1.15, 30,
                              cw=(s < 0))
                pts = list(reversed(pts))
                tail = [pts[-1], (cx + s * 0.02, 0.45 * h), (cx + s * 0.09, 0.28 * h)]
                path = pts + _chaikin(tail, iters=2, closed=False)[1:]
                b.flat_bar(path, fr, 0.012, 0.016, M)
                lo = _spiral(cx + s * 0.11, 0.22 * h, 0.008, 0.05, (math.pi / 2), 1.0, 20, cw=(s > 0))
                b.flat_bar(lo, fr, 0.012, 0.016, M)
            b.box((cx - 0.02, 0.01, 0.40 * h), (cx + 0.02, 0.04, 0.44 * h), M, bevel=0.005)
            b.box((cx - 0.005, 0.015, 0.10), (cx + 0.005, 0.035, h - 0.12), M)
    else:
        raise ValueError(style)
    return b.finish()


def parapet(length=5.0, h=1.0, t=0.14):
    """Solid white loggia parapet (AC_Trim) along x = -length/2..length/2, y = 0..t: base band,
    panel joints and an overhanging coping."""
    b = _B("parapet")
    M = "AC_Trim"
    L = length / 2
    b.box((-L, 0.0, 0.0), (L, t + 0.02, 0.09), M, bevel=0.008)
    b.box((-L, 0.0, 0.09), (L, t, h - 0.07), M)
    b.box((-L - 0.03, -0.02, h - 0.07), (L + 0.03, t + 0.035, h), M, bevel=0.01, seg=2)
    n = max(1, int(round(length / 1.2)))
    for i in range(1, n):
        x = -L + length * i / n
        b.box((x - 0.006, t - 0.004, 0.09), (x + 0.006, t + 0.003, h - 0.07), M)
    return b.finish()


def oculus(d=0.9, ring=0.24, fill='louvre', hole=True):
    """Round window centred on x = z = 0 of the wall plane: moulded white ring (AC_Trim) on the
    face, round reveal liner, louvre slats (AC_Iron) or glazing (AC_Glass + AC_Frame)."""
    b = _B("oculus")
    M = "AC_Trim"
    ri, ro = d / 2, d / 2 + ring
    prof = ([(ri, -0.25)] if hole else []) + [(ri, 0.0), (ro, 0.0), (ro, 0.025), (ro - 0.02, 0.045), (ro - 0.06, 0.05),
            (ri + 0.10, 0.055), (ri + 0.08, 0.075), (ri + 0.05, 0.085), (ri + 0.025, 0.075),
            (ri + 0.01, 0.05), (ri - 0.0, 0.04)]
    # revolve (r, y) around the y axis: build around z then rotate z -> y
    rot = Matrix.Rotation(-math.pi / 2, 4, 'X')
    b.begin()
    segs = 40
    rings = []
    for (r, y) in prof:
        rings.append([b.v(rot @ Vector((r * math.cos(2 * math.pi * k / segs), r * math.sin(2 * math.pi * k / segs), y)))
                      for k in range(segs)])
    for i in range(len(rings) - 1):
        for k in range(segs):
            k1 = (k + 1) % segs
            b.f([rings[i][k], rings[i][k1], rings[i + 1][k1], rings[i + 1][k]], M)
    for fc in b._gf:
        c = fc.calc_center_median()
        radial = Vector((c.x, 0, c.z))
        if c.y < -0.001:
            if fc.normal.dot(-radial) < 0:
                fc.normal_flip()
    b.end()
    # make sure the front ring faces point outwards (+y or radially out)
    # hole=True: louvre/glass recessed in a real wall hole; hole=False: everything sits on the
    # wall face, inside the proud ring (for walls with no hole cut)
    yb = -0.24 if hole else 0.003
    ym = -0.12 if hole else 0.032
    if fill == 'louvre':
        b.lathe([(0, 0), (ri, 0), (ri, 0.01), (0, 0.01)], 24, "AC_Iron",
                xform=Matrix.Translation((0, yb, 0)) @ rot)
        n = 7
        for i in range(n):
            z = -ri + (i + 0.5) * (2 * ri / n)
            c = math.sqrt(max(0.0, ri * ri - z * z)) - 0.005
            m = Matrix.Translation((0, ym, z)) @ Matrix.Rotation(math.radians(35), 4, 'X')
            b.box((-c, -0.004, -0.04), (c, 0.004, 0.04), "AC_Iron", xform=m)
        b.box((-0.012, ym - 0.04, -ri), (0.012, ym + 0.04, ri), "AC_Iron")
    else:
        b.lathe([(0, 0), (ri, 0), (ri, 0.002), (0, 0.002)], 24, "AC_Glass",
                xform=Matrix.Translation((0, GLASS_Y if hole else 0.02, 0)) @ rot)
        b.box((-0.02, ym - 0.03, -ri), (0.02, ym + 0.03, ri), "AC_Frame")
        b.box((-ri, ym - 0.03, -0.02), (ri, ym + 0.03, 0.02), "AC_Frame")
    return b.finish()


def swag(length=0.95, sag=0.22, drop=0.42):
    """Stucco festoon (AC_Trim): a leafy garland hanging between two knots with ribbon drops
    at both ends. x = -length/2..length/2, z = 0 at the bottom of the drops, on the wall y >= 0."""
    b = _B("swag")
    M = "AC_Trim"
    L = length / 2
    zt = drop + 0.02
    n = 48
    pts, rad = [], []
    for i in range(n + 1):
        t = i / n
        x = -L + length * t
        z = zt - sag * math.sin(math.pi * t) ** 1.2
        r = 0.022 + 0.045 * math.sin(math.pi * t) ** 0.7
        r *= 1.0 + 0.25 * abs(math.sin(t * math.pi * 11))
        pts.append((x, r * 0.55, z))
        rad.append(r)
    b.tube(pts, rad, M, sides=8)
    for s in (-1, 1):
        x = s * L
        # rosette knot
        b.lathe([(0, 0), (0.04, 0), (0.035, 0.015), (0.015, 0.03), (0, 0.032)], 12, M,
                xform=Matrix.Translation((x, 0.0, zt + 0.01)) @ Matrix.Rotation(-math.pi / 2, 4, 'X'))
        # ribbon drop with beads, ending in a tassel
        dp, dr = [], []
        m = 16
        for i in range(m + 1):
            t = i / m
            dp.append((x + s * 0.01 * math.sin(t * 6), 0.012, zt - 0.02 - (drop - 0.06) * t))
            dr.append((0.014 + 0.012 * abs(math.sin(t * math.pi * 5))) * (1.0 - 0.3 * t))
        b.tube(dp, dr, M, sides=6)
        b.lathe([(0, 0), (0.022, 0.03), (0.012, 0.06), (0, 0.065)], 8, M,
                center=(x + s * 0.01 * math.sin(6), 0.012, 0.0))
    return b.finish()


# --------------------------------------------------------------------------------------------
# corner quoins and cartouche

def quoin_stack(n_courses=11, course_h=0.5, long=0.9, short=0.6, angle=90.0, proj=0.08, res=0.03,
                joint=0.02):
    """Vermiculated rusticated corner quoin (AC_Quoin) for a convex building corner at the
    origin. Face A is the wall plane y = 0 running along -x; face B runs from the corner at
    the given interior angle (90 deg: the plane x = 0 running along -y). Courses alternate
    long/short on each face; each block has chamfered joints and a worm-tracked
    (vermiculated) face as real displaced geometry (a few mm to 1.2 cm deep)."""
    b = _B("quoin_stack")
    M = "AC_Quoin"
    th = math.radians(angle)
    tA = Vector((-1.0, 0.0))
    tB = Vector((-math.cos(th), -math.sin(th)))  # 90 deg -> (0, -1)
    nA = Vector((0.0, 1.0))
    nB = Vector((-tB.y, tB.x))
    if nB.dot(Vector((1, 0))) < 0 and angle <= 180:
        nB = -nB
    # mitre point at the front corner
    # solve P = nA*proj + tA*s = nB*proj + tB*u
    A = Matrix(((tA.x, -tB.x), (tA.y, -tB.y)))
    rhs = nB * proj - nA * proj
    try:
        su = A.inverted() @ rhs
        P = nA * proj + tA * su.x
    except ValueError:
        P = nA * proj
    for i in range(n_courses):
        z0 = i * course_h + (joint / 2 if i else 0.0)
        z1 = (i + 1) * course_h - joint / 2
        LA, LB = (long, short) if i % 2 == 0 else (short, long)
        seed = i * 2.3
        a_end_f = tA * LA + nA * proj
        a_end_b = tA * LA
        b_end_f = tB * LB + nB * proj
        b_end_b = tB * LB
        # face A block: front from far end to the mitre point, back on the wall to the corner
        fr = [(a_end_f.x, a_end_f.y, z0), (P.x, P.y, z0), (P.x, P.y, z1), (a_end_f.x, a_end_f.y, z1)]
        bk = [(a_end_b.x, a_end_b.y, z0), (0, 0, z0), (0, 0, z1), (a_end_b.x, a_end_b.y, z1)]
        b.rock(fr, bk, M, mode='verm', seed=seed, chamfer=0.045, cell=res, groove=0.016,
               edges=(True, False, True, True), sides=(True, False, True, True),
               arris=(1, (nB.x, nB.y, 0.0)))
        fr = [(P.x, P.y, z0), (b_end_f.x, b_end_f.y, z0), (b_end_f.x, b_end_f.y, z1), (P.x, P.y, z1)]
        bk = [(0, 0, z0), (b_end_b.x, b_end_b.y, z0), (b_end_b.x, b_end_b.y, z1), (0, 0, z1)]
        b.rock(fr, bk, M, mode='verm', seed=seed + 50, chamfer=0.045, cell=res, groove=0.016,
               edges=(True, True, True, False), sides=(True, True, True, False),
               arris=(3, (nA.x, nA.y, 0.0)))
    return b.finish()


def _shield_outline(w, h):
    """Escutcheon of the 1947 cartouche: concave notched top corners, straight sides tapering
    a little, chamfered bottom. (x, z), CCW, z = 0 at the bottom."""
    hw = w / 2
    right = [(0.0, 0.0), (0.30 * hw, 0.0), (0.62 * hw, 0.05 * h), (0.95 * hw, 0.25 * h), (hw, 0.62 * h),
             (hw, 0.84 * h)]
    right += _arc(hw, h, 0.16 * h, -math.pi / 2, -math.pi, 6)[1:]   # concave notch
    right += [(0.70 * hw, 0.985 * h), (0.35 * hw, h)]
    left = [(-x, z) for (x, z) in reversed(right[1:])]
    pts = right + [(0.0, h * 1.01)] + left
    # remove duplicates
    out = []
    for p in pts:
        if not out or (Vector(p) - Vector(out[-1])).length > 1e-5:
            out.append(p)
    if (Vector(out[0]) - Vector(out[-1])).length < 1e-5:
        out.pop()
    return out


def _inset_poly(pts, d):
    """Offset a CCW polygon inwards by d (miter)."""
    n = len(pts)
    P = [Vector(p) for p in pts]
    out = []
    for i in range(n):
        a, c = P[i - 1], P[(i + 1) % n]
        t1 = (P[i] - a).normalized()
        t2 = (c - P[i]).normalized()
        n1 = Vector((-t1.y, t1.x))
        n2 = Vector((-t2.y, t2.x))
        m = n1 + n2
        if m.length < 1e-6:
            m = n1
        m = m.normalized()
        k = d / max(0.3, m.dot(n1))
        q = P[i] + m * k
        out.append((q.x, q.y))
    return out


def _star(cx, cz, ro, ri):
    pts = []
    for k in range(10):
        a = math.pi / 2 + k * math.pi / 5
        r = ro if k % 2 == 0 else ri
        pts.append((cx + r * math.cos(a), cz + r * math.sin(a)))
    return pts


def cartouche_1947():
    """The '1947' corner cartouche of the side buildings (AC_Trim), facing +y (the caller turns
    it 45 deg onto the corner arris): corniced pedestal, escutcheon with a raised rim, 5-point
    star and raised digits '1947' (Bfont text converted to mesh), ribbon bow on top. ~1.6 m."""
    b = _B("cartouche_1947")
    M = "AC_Trim"
    # pedestal: base block + moulded cornice slab
    b.box((-0.24, 0.0, 0.0), (0.24, 0.30, 0.30), M, bevel=0.01)
    b.box((-0.27, 0.0, 0.30), (0.27, 0.33, 0.34), M, bevel=0.008)
    b.box((-0.31, 0.0, 0.34), (0.31, 0.37, 0.39), M, bevel=0.01, seg=2)
    b.box((-0.40, 0.0, 0.39), (0.40, 0.46, 0.49), M, bevel=0.012, seg=2)
    b.box((-0.38, 0.0, 0.49), (0.38, 0.44, 0.52), M, bevel=0.006)
    # shield
    sw, sh = 0.66, 0.88
    z0 = 0.53
    sh_pts = [(x, z + z0) for (x, z) in _shield_outline(sw, sh)]
    inner = _inset_poly(sh_pts, 0.04)
    b.prism(inner, 0.0, 0.085, M)
    b.ring_prism(sh_pts, inner, 0.0, 0.11, M, bevel=0.008)
    # star
    b.prism(_star(0.0, z0 + 0.62 * sh, 0.10, 0.042), 0.08, 0.11, M, fan=(0.0, z0 + 0.62 * sh))
    # digits
    me = _text_mesh("1947", 0.20, 0.012)
    mtx = (Matrix.Translation((0.0, 0.085 + 0.012, z0 + 0.33 * sh)) @ Matrix.Rotation(math.pi, 4, 'Z')
           @ Matrix.Rotation(math.pi / 2, 4, 'X'))
    b.add_mesh(me, mtx, M)
    bpy.data.meshes.remove(me)
    # ribbon bow: central knot ring + two ribbons curling outwards
    zb = z0 + sh + 0.07
    rot = Matrix.Rotation(-math.pi / 2, 4, 'X')
    b.lathe([(0.025, -0.03), (0.06, -0.03), (0.065, 0.0), (0.06, 0.03), (0.025, 0.03), (0.02, 0.0)], 16, M,
            xform=Matrix.Translation((0, 0.06, zb)) @ rot, recalc=False)
    b.lathe([(0, -0.02), (0.03, -0.02), (0.03, 0.02), (0, 0.02)], 10, M,
            xform=Matrix.Translation((0, 0.06, zb)) @ rot)
    fr = ((0, 0.03, 0), (1, 0, 0), (0, 0, 1), (0, 1, 0))
    for s in (-1, 1):
        path = [(s * 0.05, zb + 0.02), (s * 0.12, zb + 0.05), (s * 0.20, zb + 0.02), (s * 0.24, zb - 0.04),
                (s * 0.30, zb - 0.05), (s * 0.35, zb - 0.01)]
        path = _chaikin(path, iters=3, closed=False)
        curl = _spiral(s * 0.33, zb + 0.015, 0.025, 0.003, (-math.pi / 2 if s > 0 else -math.pi / 2), 0.9, 12,
                       cw=(s < 0))
        path = path + curl[1:]
        n = len(path)
        wid = [0.05 * (1 - 0.6 * i / n) + 0.01 for i in range(n)]
        b.flat_bar(path, fr, wid, 0.03, M)
        loop = _spiral(s * 0.10, zb + 0.01, 0.045, 0.04, (math.pi if s > 0 else 0.0), 0.8, 16, cw=(s > 0))
        b.flat_bar(loop, fr, 0.03, 0.035, M)
    return b.finish()


# --------------------------------------------------------------------------------------------
# roof pieces

def chimney(w=0.9, d=0.6, h=2.0):
    """Red brick chimney stack (AC_Brick), base centre at the origin (sink it into the roof):
    corbelled cap with crenellated merlons at the corners and mid-sides, top corbel slab,
    flue mouths, small pyramidal sheet hood (AC_Zinc) on 4 posts."""
    b = _B("chimney")
    Bk = "AC_Brick"
    hx, hy = w / 2, d / 2
    h0 = h - 0.62
    b.box((-hx, -hy, 0.0), (hx, hy, h0), Bk)
    z = h0
    for e, t in ((0.04, 0.075), (0.08, 0.075)):
        b.box((-hx - e, -hy - e, z), (hx + e, hy + e, z + t), Bk, bevel=0.005)
        z += t
    # crenel band: recessed core + merlons
    cz0, cz1 = z, z + 0.24
    e = 0.05
    b.box((-hx - e + 0.04, -hy - e + 0.04, cz0), (hx + e - 0.04, hy + e - 0.04, cz1), Bk)
    m = 0.15
    ex = hx + 0.08
    ey = hy + 0.08
    for (x, y) in ((-1, -1), (1, -1), (1, 1), (-1, 1), (0, -1), (0, 1)):
        cx = x * (ex - m / 2)
        cy = y * (ey - m / 2)
        b.box((cx - m / 2, cy - m / 2, cz0), (cx + m / 2, cy + m / 2, cz1), Bk, bevel=0.005)
        # little stepped cap on each merlon
        b.box((cx - m / 2 - 0.015, cy - m / 2 - 0.015, cz1), (cx + m / 2 + 0.015, cy + m / 2 + 0.015, cz1 + 0.04),
              Bk, bevel=0.004)
    z = cz1 + 0.04
    b.box((-hx - 0.06, -hy - 0.06, z), (hx + 0.06, hy + 0.06, z + 0.07), Bk, bevel=0.005)
    z += 0.07
    # flues
    for s in (-1, 1):
        cx = s * w * 0.22
        b.box((cx - 0.09, -0.09, z - 0.005), (cx + 0.09, 0.09, z + 0.004), "AC_Iron")
    # posts and hood
    pz = z + 0.26
    for (x, y) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        px, py = x * (hx - 0.02), y * (hy - 0.02)
        b.box((px - 0.015, py - 0.015, z), (px + 0.015, py + 0.015, pz), "AC_Zinc")
    hx2, hy2 = hx + 0.1, hy + 0.1
    b.begin()
    base = [b.v(p) for p in ((-hx2, -hy2, pz), (hx2, -hy2, pz), (hx2, hy2, pz), (-hx2, hy2, pz))]
    base2 = [b.v(p) for p in ((-hx2, -hy2, pz + 0.025), (hx2, -hy2, pz + 0.025), (hx2, hy2, pz + 0.025),
                              (-hx2, hy2, pz + 0.025))]
    apex = b.v((0, 0, pz + 0.025 + 0.24))
    b.f(list(reversed(base)), "AC_Zinc")
    for i in range(4):
        j = (i + 1) % 4
        b.f([base[i], base[j], base2[j], base2[i]], "AC_Zinc")
        b.f([base2[i], base2[j], apex], "AC_Zinc")
    b.end(recalc=True)
    return b.finish()


def downpipe(h=12.0, d=0.12, standoff=0.06):
    """Galvanised downpipe (AC_Zinc) on the wall: hopper head at the top (z = h), 1 m sections
    with collars, outlet shoe at the bottom (z = 0, pointing out from the wall), iron bracket
    bands with wall pins (AC_Iron) every ~1.8 m. Pipe axis at y = standoff + d/2."""
    b = _B("downpipe")
    Z = "AC_Zinc"
    r = d / 2
    yc = standoff + r
    z_shoe = 0.35
    z_top = h - 0.45
    b.cyl(0.0, yc, z_shoe, z_top, r, Z, sides=12)
    z = z_shoe + 1.0
    while z < z_top - 0.2:
        b.cyl(0.0, yc, z - 0.05, z, r + 0.006, Z, sides=12)
        z += 1.0
    # shoe: a bend out from the wall
    pts = []
    for i in range(7):
        a = (math.pi / 3) * i / 6
        pts.append((0.0, yc + 0.25 * (1 - math.cos(a)), z_shoe - 0.25 * math.sin(a)))
    pts = [(0.0, yc, z_shoe + 0.01)] + pts
    b.tube(pts, [r] * len(pts), Z, sides=12, cap=False)
    last = Vector(pts[-1])
    dirv = (Vector(pts[-1]) - Vector(pts[-2])).normalized()
    b.tube([tuple(last), tuple(last + dirv * 0.1)], [r, r * 1.2], Z, sides=12)
    # hopper head: funnel + box
    b.lathe([(0, z_top - 0.02), (r, z_top - 0.02), (r, z_top + 0.05), (0.13, z_top + 0.25), (0.15, z_top + 0.45),
             (0.13, z_top + 0.45), (0.11, z_top + 0.27), (0, z_top + 0.27)], 12, Z, center=(0, yc, 0))
    # brackets
    z = z_shoe + 0.6
    while z < z_top:
        b.lathe([(r + 0.002, z - 0.02), (r + 0.012, z - 0.02), (r + 0.012, z + 0.02), (r + 0.002, z + 0.02)],
                12, "AC_Iron", center=(0, yc, 0), recalc=False)
        b.box((-0.01, -0.03, z - 0.012), (0.01, yc - r, z + 0.012), "AC_Iron")
        z += 1.8
    return b.finish()


# --------------------------------------------------------------------------------------------
# gates

# Right-hand gate pylon outline traced from refs/exterior/commons_Marshal_Novikov_str._1_gates.jpg
# (pixel units of a 2x crop, x right, y down; base top at y = 620, top at y = 45). '1' = sharp.
_PYLON_PX = [
    (100, 620, 1), (72, 580, 0), (52, 520, 0), (60, 460, 0), (92, 410, 0), (128, 382, 0), (150, 372, 1),
    (150, 285, 1), (232, 285, 1), (232, 202, 1), (204, 194, 1), (194, 160, 0), (200, 112, 0),
    (222, 70, 0), (262, 46, 0), (310, 47, 0), (345, 72, 0), (357, 115, 0), (356, 185, 0), (345, 245, 0),
    (322, 300, 0), (300, 328, 1), (270, 336, 1), (270, 408, 1), (238, 422, 0), (205, 448, 0),
    (186, 488, 0), (186, 545, 0), (200, 588, 0), (222, 620, 1),
]


def scroll_pylon(h=2.8, t=0.5, base_h=0.6):
    """Gate pylon (AC_Pylon): a slab t thick (centred on y = 0) with the stepped S profile, a
    big volute rolled at the top and a smaller curl at the bottom, on a base block. Base
    centre at the origin; it is the right-hand pylon seen from the street (mirror with
    scale x = -1 for the left one). The volute eyes carry raised spiral ribs on both faces."""
    b = _B("scroll_pylon")
    M = "AC_Pylon"
    body_h = h - base_h
    k = body_h / (620 - 45)
    cxp = 205.0
    pts = [((x - cxp) * k, base_h + (620 - y) * k) for (x, y, s) in _PYLON_PX]
    sharp = [bool(s) for (x, y, s) in _PYLON_PX]
    outline = _chaikin(pts, sharp, iters=3, closed=True)
    b.prism(outline, -t / 2, t / 2, M, axis='y', bevel=0.02, seg=2)
    # base block (slightly wider than the foot of the slab)
    xs = [p[0] for p in outline if p[1] < base_h + 0.5]
    bx0, bx1 = min(xs) - 0.08, max(xs) + 0.10
    b.box((bx0, -t / 2 - 0.08, 0.0), (bx1, t / 2 + 0.08, base_h), M, bevel=0.02, seg=2)
    # volute eyes: raised spirals on both faces
    top_c = ((272 - cxp) * k, base_h + (620 - 118) * k)
    bot_c = ((118 - cxp) * k, base_h + (620 - 470) * k)
    for s in (-1, 1):
        fr = ((0, s * t / 2, 0), (1, 0, 0), (0, 0, 1), (0, s, 0))
        b.flat_bar(_spiral(top_c[0], top_c[1], 0.02, 0.24, math.pi, 1.5, 48, cw=True), fr, 0.035, 0.018, M)
        b.flat_bar(_spiral(bot_c[0], bot_c[1], 0.015, 0.13, 0.0, 1.25, 36, cw=False), fr, 0.03, 0.015, M)
    return b.finish()


def wicket_block(length=3.8, h=4.5, arch_w=1.8, arch_h=3.0, t=0.7):
    """Gate wicket (калитка) block: ochre wall (AC_Wall) x = -length/2..length/2, thickness t
    centred on y = 0, with a semicircular arched passage arch_w x arch_h (clear), white arch
    surround, white band + moulded top cornice (AC_Trim) and a small hipped sheet roof
    (AC_Roof) above h. The clear opening must be >= 1.08 x 2.10 m (game rule)."""
    if arch_w < 1.08 or arch_h < 2.10:
        raise ValueError("wicket arch must be at least 1.08 m wide and 2.10 m high")
    b = _B("wicket_block")
    W, T = "AC_Wall", "AC_Trim"
    L = length / 2
    r = arch_w / 2
    zs = arch_h - r
    zb = h - 0.95          # top of the plain wall (band starts here)
    # front and back faces: radial strips between the arch and the outer rectangle
    n_arc = 24
    angs = [math.pi * i / n_arc for i in range(n_arc + 1)]
    ac = math.atan2(zb - zs, L)
    angs = sorted(set(angs + [ac, math.pi - ac]))
    arch = [(r * math.cos(a), zs + r * math.sin(a)) for a in angs]

    def to_rect(a):
        ca, sa = math.cos(a), math.sin(a)
        tx = L / abs(ca) if abs(ca) > 1e-9 else 1e9
        tz = (zb - zs) / sa if sa > 1e-9 else 1e9
        tt = min(tx, tz)
        return (ca * tt, zs + sa * tt)
    outer = [to_rect(a) for a in angs]
    for y, sgn in ((t / 2, 1), (-t / 2, -1)):
        b.begin()
        A = [b.v((p[0], y, p[1])) for p in arch]
        O = [b.v((p[0], y, p[1])) for p in outer]
        for i in range(len(arch) - 1):
            b.f([A[i], O[i], O[i + 1], A[i + 1]], W)
        # piers
        for sx in (1, -1):
            q = [b.v((sx * r, y, 0.0)), b.v((sx * L, y, 0.0)), b.v((sx * L, y, zs)), b.v((sx * r, y, zs))]
            b.f(q, W)
        for fc in b._gf:
            if fc.normal.y * sgn < 0:
                fc.normal_flip()
        b.end()
    # ends, top, intrados
    b.quad([(L, -t / 2, 0), (L, t / 2, 0), (L, t / 2, zb), (L, -t / 2, zb)][::-1], W)
    b.quad([(-L, -t / 2, 0), (-L, t / 2, 0), (-L, t / 2, zb), (-L, -t / 2, zb)], W)
    intr = [(r, 0.0)] + arch + [(-r, 0.0)]
    b.begin()
    F = [b.v((p[0], t / 2, p[1])) for p in intr]
    Bv = [b.v((p[0], -t / 2, p[1])) for p in intr]
    for i in range(len(intr) - 1):
        b.f([F[i], F[i + 1], Bv[i + 1], Bv[i]], W)
    for fc in b._gf:
        c = fc.calc_center_median()
        if fc.normal.dot(Vector((0, c.y, zs)) - c) < 0:
            fc.normal_flip()
    b.end()
    # arch surround on both faces
    for y0, y1 in ((t / 2, t / 2 + 0.04), (-t / 2 - 0.04, -t / 2)):
        b.prism(_arch_band(arch_w, arch_h, 0.0, 0.16), y0, y1, T, bevel=0.006)
    # upper body: white band, ochre frieze, cornice
    band = [(0.0, 0.0), (0.05, 0.0), (0.06, 0.02), (0.06, 0.10), (0.08, 0.12), (0.08, 0.15), (0.0, 0.17)]
    rect_cw = [(-L, -t / 2), (-L, t / 2), (L, t / 2), (L, -t / 2)]
    b.sweep_plan([(o, z + zb) for (o, z) in band], rect_cw, T, closed=True)
    b.box((-L, -t / 2, zb), (L, t / 2, h - 0.30), W)
    b.box((-L, -t / 2, zb + 0.17), (L, t / 2, h - 0.30), W)
    corn = [(0.0, 0.0), (0.03, 0.0), (0.05, 0.03), (0.09, 0.06), (0.10, 0.10), (0.18, 0.12), (0.18, 0.22),
            (0.20, 0.24), (0.20, 0.27), (0.0, 0.30)]
    b.sweep_plan([(o, z + h - 0.30) for (o, z) in corn], rect_cw, T, closed=True)
    b.box((-L, -t / 2, h - 0.30), (L, t / 2, h), T)
    # hipped roof
    rx, ry = L + 0.24, t / 2 + 0.24
    ridge = max(0.05, rx - ry)
    rh = ry * math.tan(math.radians(30))
    b.begin()
    z0 = h
    c = [b.v(p) for p in ((-rx, -ry, z0), (rx, -ry, z0), (rx, ry, z0), (-rx, ry, z0))]
    c2 = [b.v(p) for p in ((-rx, -ry, z0 + 0.03), (rx, -ry, z0 + 0.03), (rx, ry, z0 + 0.03), (-rx, ry, z0 + 0.03))]
    r0 = b.v((-ridge, 0, z0 + 0.03 + rh))
    r1 = b.v((ridge, 0, z0 + 0.03 + rh))
    b.f(list(reversed(c)), "AC_Roof")
    for i in range(4):
        j = (i + 1) % 4
        b.f([c[i], c[j], c2[j], c2[i]], "AC_Roof")
    b.f([c2[0], c2[1], r1, r0], "AC_Roof")
    b.f([c2[1], c2[2], r1], "AC_Roof")
    b.f([c2[2], c2[3], r0, r1], "AC_Roof")
    b.f([c2[3], c2[0], r0], "AC_Roof")
    b.end(recalc=True)
    return b.finish()


# --------------------------------------------------------------------------------------------
# square: fountain and flowerbed

def _fleur_outline_parts():
    """Fleur-de-lis as 2D parts (x, z), z = 0 at the bottom, 0.45 m tall, 0.38 m wide."""
    centre = [(0.0, 0.45), (0.045, 0.37), (0.07, 0.27), (0.065, 0.18), (0.035, 0.11), (-0.035, 0.11),
              (-0.065, 0.18), (-0.07, 0.27), (-0.045, 0.37)]
    centre = _chaikin(centre, [True, False, False, False, True, True, False, False, False], iters=3)
    side = [(0.04, 0.11), (0.09, 0.13), (0.15, 0.19), (0.185, 0.25), (0.19, 0.29), (0.165, 0.30),
            (0.15, 0.27), (0.17, 0.235), (0.135, 0.205), (0.09, 0.19), (0.06, 0.17)]
    side = _chaikin(side, [True] + [False] * 9 + [True], iters=3)
    foot = [(0.0, 0.0), (0.06, 0.02), (0.11, 0.0), (0.12, 0.03), (0.06, 0.07), (0.0, 0.065)]
    return centre, side, foot


def fountain():
    """White cast-stone two-tier fountain (AC_Stone), centred on x = y = 0: base plinth, fluted
    trumpet pedestal, wide gadrooned lower basin (Ø 1.9 m, rim at 1.25 m), baluster stem,
    small upper bowl and a fleur-de-lis finial; total ≈ 2.35 m."""
    b = _B("fountain")
    M = "AC_Stone"
    segs = 48

    def gad_low(i, a, r, z):
        if 0.92 < z < 1.17 and r > 0.5:
            return r * (1.0 + 0.035 * abs(math.cos(a * 10)))
        if 0.27 < z < 0.78 and r < 0.3:
            return r * (1.0 - 0.06 * (0.5 + 0.5 * math.cos(a * 16)))
        return r
    prof = [(0.0, 0.0), (0.52, 0.0), (0.52, 0.10), (0.47, 0.12), (0.44, 0.18), (0.40, 0.20),
            (0.36, 0.24), (0.30, 0.27), (0.22, 0.38), (0.17, 0.52), (0.155, 0.66), (0.15, 0.78),
            (0.18, 0.80), (0.18, 0.83), (0.15, 0.85), (0.20, 0.88), (0.36, 0.92), (0.55, 0.97),
            (0.72, 1.03), (0.84, 1.10), (0.92, 1.17), (0.955, 1.20), (0.965, 1.23), (0.965, 1.27),
            (0.95, 1.29), (0.90, 1.29), (0.88, 1.25), (0.80, 1.19), (0.55, 1.15), (0.0, 1.14)]
    b.lathe(prof, segs, M, mod=gad_low)
    # upper tier: baluster stem + small bowl

    def gad_up(i, a, r, z):
        if 1.68 < z < 1.80 and r > 0.15:
            return r * (1.0 + 0.05 * abs(math.cos(a * 8)))
        return r
    prof2 = [(0.0, 1.14), (0.13, 1.14), (0.13, 1.18), (0.09, 1.21), (0.07, 1.27), (0.085, 1.36), (0.10, 1.43),
             (0.085, 1.50), (0.06, 1.55), (0.065, 1.58), (0.09, 1.60), (0.07, 1.63), (0.12, 1.68), (0.22, 1.72),
             (0.29, 1.77), (0.32, 1.80), (0.325, 1.83), (0.31, 1.85), (0.27, 1.85), (0.20, 1.82),
             (0.0, 1.81)]
    b.lathe(prof2, 36, M, mod=gad_up)
    # finial neck
    b.lathe([(0.0, 1.81), (0.05, 1.81), (0.04, 1.86), (0.065, 1.88), (0.065, 1.91), (0.0, 1.91)], 16, M)
    centre, side, foot = _fleur_outline_parts()
    z0 = 1.90
    for rot in (0.0, math.pi / 2):
        mr = Matrix.Rotation(rot, 4, 'Z') @ Matrix.Translation((0, 0, z0))
        b.prism(centre, -0.04, 0.04, M, axis='y', xform=mr)
        for s in (-1, 1):
            pts = [(s * x, z) for (x, z) in side]
            b.prism(pts, -0.03, 0.03, M, axis='y', xform=mr)
            fpts = [(s * x, z) for (x, z) in foot]
            b.prism(fpts, -0.03, 0.03, M, axis='y', xform=mr)
    b.lathe([(0.0, z0 + 0.10), (0.085, z0 + 0.10), (0.085, z0 + 0.135), (0.0, z0 + 0.135)], 16, M)
    return b.finish()


def flowerbed_railing(radius=5.7, h=0.45, module=0.21):
    """Round flowerbed edge centred on x = y = 0: concrete kerb ring of straight slabs
    (AC_Concrete, 0.2 x 0.15 m) and on it a low white railing (AC_IronWhite) of flat-bar
    S-scrolls, alternately mirrored so pairs read as hearts, with thin top/bottom rails and
    posts every ~1.5 m. h is the railing height above the kerb."""
    b = _B("flowerbed_railing")
    K, I = "AC_Concrete", "AC_IronWhite"
    R = radius
    kh, kw = 0.15, 0.20
    # kerb: straight slabs (chords) with small joints, like the photo
    nk = int(round(2 * math.pi * R / 1.0))
    for i in range(nk):
        a0 = 2 * math.pi * i / nk + 0.002
        a1 = 2 * math.pi * (i + 1) / nk - 0.002
        ri, ro = R - kw / 2, R + kw / 2
        q = [(ri * math.cos(a0), ri * math.sin(a0)), (ro * math.cos(a0), ro * math.sin(a0)),
             (ro * math.cos(a1), ro * math.sin(a1)), (ri * math.cos(a1), ri * math.sin(a1))]
        b.prism(q, -0.10, kh, K, axis='z', bevel=0.012)
    # railing in unrolled coordinates (u along the circle, v radial, z up), then wrapped
    C = 2 * math.pi * R
    nm = int(round(C / module))
    m = C / nm
    zr0 = kh + 0.03
    hp = h - 0.03
    start_v = len(b.bm.verts)
    b.bm.verts.ensure_lookup_table()
    fr = ((0, -0.008, 0), (1, 0, 0), (0, 0, 1), (0, 1, 0))
    for i in range(nm):
        u0 = i * m
        mir = (i % 2 == 1)
        # S-scroll: upper curl at the left, lower curl at the right (mirrored on odd modules)
        r1 = min(0.25 * hp, 0.46 * m)
        cxA, czA = 0.30 * m, zr0 + hp - r1 - 0.012
        cxB, czB = 0.70 * m, zr0 + r1 + 0.012
        turns = 1.3
        # both curls clockwise from the eye; A ends at its rightmost point heading down,
        # B (walked backwards) starts at its leftmost point heading down -> an S
        sA = _spiral(cxA, czA, 0.01, r1, turns * 2 * math.pi, turns, 15, cw=True)
        sB = _spiral(cxB, czB, 0.01, r1, math.pi + turns * 2 * math.pi, turns, 15, cw=True)
        path = sA + list(reversed(sB))
        path = [(u0 + (m - x if mir else x), z) for (x, z) in path]
        b.flat_bar(path, fr, 0.007, 0.016, I)
    # rails (polyline tubes of square section via flat bars)
    for z in (zr0, zr0 + hp):
        path = [(C * k / 360, z) for k in range(361)]
        b.flat_bar(path, fr, 0.012, 0.016, I)
    npost = int(round(C / 1.5))
    for k in range(npost):
        u = C * k / npost
        b.box((u - 0.012, -0.012, kh - 0.05), (u + 0.012, 0.012, zr0 + hp + 0.05), I)
    b.bm.verts.ensure_lookup_table()
    for v in b.bm.verts[start_v:]:
        u, vr, z = v.co
        a = u / R
        rr = R + vr
        v.co = Vector((rr * math.cos(a), rr * math.sin(a), z))
    b.bm.normal_update()
    # the wrap preserves orientation (u -> angle CCW, v -> radius out), normals stay valid
    return b.finish()


# --------------------------------------------------------------------------------------------

# --------------------------------------------------------------------------------------------
# balconies, bay windows, vents

def slab(w=3.0, depth=1.0, t=0.25):
    """Balcony / loggia slab (AC_Trim, plastered) from the wall y = 0 out to y = depth,
    x = -w/2..w/2, z = 0..t, with a stepped drip edge on the three free sides."""
    b = _B("slab")
    M = "AC_Trim"
    W = w / 2
    b.box((-W + 0.04, -0.02, 0.0), (W - 0.04, depth - 0.04, 0.07), M, bevel=0.006)
    prof = [(0.0, 0.07), (0.015, 0.07), (0.03, 0.09), (0.03, t - 0.04), (0.04, t - 0.03), (0.04, t),
            (0.0, t)]
    b.box((-W, -0.02, 0.07), (W, depth - 0.0, t), M)
    # path runs -x side -> front -> +x side: the sweep normal points outwards
    b.sweep_plan(prof, [(-W, -0.02), (-W, depth), (W, depth), (W, -0.02)], M)
    return b.finish()


def railing_side(length=1.0, h=1.0, style='bars'):
    """Balcony railing running along +y from the wall (y = 0..length) at x = 0."""
    ob = balcony_railing(length, h, style)
    _xf(ob, Matrix.Translation((0, length / 2, 0)) @ Matrix.Rotation(math.pi / 2, 4, 'Z'))
    ob.name = "railing_side"
    return ob


def balcony(length=2.6, depth=1.0, style='bars', consoles=False, h=1.0, t=0.2):
    """Balcony: slab (top at z = 0, from y = 0 to depth) + railing on the three free sides;
    consoles (console_big) under it when asked."""
    parts = []
    s = slab(length, depth, t)
    _xf(s, Matrix.Translation((0, 0, -t)))
    parts.append(s)
    r = balcony_railing(length - 0.06, h, style)
    _xf(r, Matrix.Translation((0, depth - 0.06, 0)))
    parts.append(r)
    for sx in (-1, 1):
        rs = balcony_railing(depth - 0.08, h, 'bars')
        _xf(rs, Matrix.Translation((sx * (length / 2 - 0.05), (depth - 0.08) / 2, 0))
            @ Matrix.Rotation(math.pi / 2, 4, 'Z'))
        parts.append(rs)
    if consoles:
        for sx in (-1, 1):
            c = console_big(0.9, 0.32, min(depth - 0.1, 0.75))
            _xf(c, Matrix.Translation((sx * (length / 2 - 0.45), 0, -t - 0.9)))
            parts.append(c)
    return _join(parts, "balcony")


def bay_window(w=4.2, depth=1.1, h=8.0):
    """Polygonal 3-sided bay window (45 deg cheeks) standing on the wall plane: back corners
    (+-w/2, 0), front face (+-(w/2 - depth), depth). One glazed tier per storey (FLOOR_H): solid
    ochre spandrel to sill height, white corner posts, multi-light white casements, white
    band between storeys, moulded cornice and a sheet-metal top (AC_Zinc). z = 0 at its base."""
    from common import FLOOR_H
    b = _B("bay_window")
    T, Wm = "AC_Trim", "AC_Wall"
    hb = w / 2
    plan = [(-hb, 0.0), (-hb + depth, depth), (hb - depth, depth), (hb, 0.0)]
    body_h = h - 0.4
    n = max(1, int(round(body_h / FLOOR_H)))
    hs = body_h / n
    sill = 0.85
    faces = list(zip(plan[:-1], plan[1:]))
    for k in range(n):
        z0 = k * hs
        # solid spandrel up to the sill
        b.prism(plan, z0, z0 + sill, Wm, axis='z')
        # band at the top of each tier (not the last: the cornice is there)
        if k < n - 1:
            b.prism(plan, z0 + hs - 0.22, z0 + hs, T, axis='z')
            bp = [(0.0, 0.0), (0.05, 0.0), (0.05, 0.18), (0.0, 0.22)]
            b.sweep_plan([(o, z + z0 + hs - 0.22) for (o, z) in bp], _bay_path(plan), T)
        gz0, gz1 = z0 + sill, z0 + hs - (0.22 if k < n - 1 else 0.0)
        # sill band
        sp = [(0.0, -0.06), (0.06, -0.06), (0.06, 0.0), (0.0, 0.02)]
        b.sweep_plan([(o, z + gz0) for (o, z) in sp], _bay_path(plan), T)
        for (pa, pb) in faces:
            A, B2 = Vector((pa[0], pa[1])), Vector((pb[0], pb[1]))
            L = (B2 - A).length
            d = (B2 - A).normalized()
            nrm = Vector((d.y, -d.x))
            if nrm.y < -1e-6 or (abs(nrm.y) < 1e-6 and nrm.x * (A.x + B2.x) < 0):
                nrm = -nrm
            m = Matrix(((d.x, nrm.x, 0, (A.x + B2.x) / 2), (d.y, nrm.y, 0, (A.y + B2.y) / 2),
                        (0, 0, 1, 0), (0, 0, 0, 1)))
            post = 0.10
            start = len(b.bm.verts)
            cols = max(1, int(round((L - 2 * post) / 0.6)))
            _glazed_rect(b, -L / 2 + post, L / 2 - post, gz0, gz1, cols=cols, transom=0.45, fw=0.05,
                         y_face=0.0)
            # wall/posts at the ends of the face, full depth
            b.box((-L / 2 - 0.02, -0.20, gz0), (-L / 2 + post, 0.0, gz1), T)
            b.box((L / 2 - post, -0.20, gz0), (L / 2 + 0.02, 0.0, gz1), T)
            # back of the glass: dark interior
            b.quad([(-L / 2, -0.35, gz0), (L / 2, -0.35, gz0), (L / 2, -0.35, gz1), (-L / 2, -0.35, gz1)],
                   "AC_Iron")
            b.bm.verts.ensure_lookup_table()
            vv = b.bm.verts[start:]
            bmesh.ops.transform(b.bm, matrix=m, verts=list(vv))
    # cornice + top
    zc = body_h
    b.prism(plan, zc, zc + 0.4, T, axis='z')
    cp = [(0.0, 0.0), (0.03, 0.0), (0.05, 0.04), (0.10, 0.08), (0.10, 0.12), (0.17, 0.16), (0.17, 0.30),
          (0.20, 0.33), (0.20, 0.37), (0.0, 0.40)]
    b.sweep_plan([(o, z + zc) for (o, z) in cp], _bay_path(plan), T)
    top = [(-hb - 0.2, 0.0), (-hb + depth + 0.08, depth + 0.2), (hb - depth - 0.08, depth + 0.2), (hb + 0.2, 0.0)]
    b.prism(top, h, h + 0.02, "AC_Zinc", axis='z')
    return b.finish()


def _bay_path(plan):
    """Sweep path along the bay faces, extended 0.2 m into the wall at both ends."""
    a = Vector(plan[0])
    d0 = (Vector(plan[1]) - a).normalized()
    e = Vector(plan[-1])
    d1 = (e - Vector(plan[-2])).normalized()
    return [tuple(a - d0 * 0.2)] + [tuple(p) for p in plan] + [tuple(e + d1 * 0.2)]


def vent(s=0.3):
    """Small square wall vent s x s (no hole needed): white frame on the wall, dark recessed
    louvre plate (AC_Iron). Bottom-centre at the origin."""
    b = _B("vent")
    f = 0.05
    b.ring_prism(_opening(s, s, False, -f), _opening(s, s), 0.0, 0.025, "AC_Trim", bevel=0.005)
    b.quad([(-s / 2, 0.002, 0), (s / 2, 0.002, 0), (s / 2, 0.002, s), (-s / 2, 0.002, s)], "AC_Iron")
    n = max(3, int(s / 0.05))
    for i in range(n):
        z = (i + 0.5) * s / n
        m = Matrix.Translation((0, 0.012, z)) @ Matrix.Rotation(math.radians(40), 4, 'X')
        b.box((-s / 2, -0.002, -0.4 * s / n), (s / 2, 0.002, 0.4 * s / n), "AC_Iron", xform=m)
    return b.finish()


# --------------------------------------------------------------------------------------------
# object helpers + dispatcher for build_academgorodok.py

def _xf(ob, m):
    """Transform an object's mesh data in place (flips faces for mirroring matrices)."""
    me = ob.data
    me.transform(m)
    if m.to_3x3().determinant() < 0:
        bm = bmesh.new()
        bm.from_mesh(me)
        bmesh.ops.reverse_faces(bm, faces=bm.faces[:])
        bm.to_mesh(me)
        bm.free()
    me.update()
    return ob


def _join(parts, name):
    """Merge finished detail objects into one new object (materials remapped by name)."""
    bm = bmesh.new()
    names = []
    uv = bm.loops.layers.uv.new("UVMap")
    for ob in parts:
        me = ob.data
        tmp = bmesh.new()
        tmp.from_mesh(me)
        remap = []
        for m in me.materials:
            if m.name not in names:
                names.append(m.name)
            remap.append(names.index(m.name))
        for f in tmp.faces:
            f.material_index = remap[f.material_index] if remap else 0
        me2 = bpy.data.meshes.new("tmp_join")
        tmp.to_mesh(me2)
        tmp.free()
        bm.from_mesh(me2)
        bpy.data.meshes.remove(me2)
        bpy.data.objects.remove(ob)
        bpy.data.meshes.remove(me)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for n in names:
        me.materials.append(get_material(n))
    obj = bpy.data.objects.new(name, me)
    (bpy.context.collection or bpy.context.scene.collection).objects.link(obj)
    obj["tris"] = triangle_count(obj)
    return obj


def _shift(ob, x=0.0, y=0.0, z=0.0):
    return _xf(ob, Matrix.Translation((x, y, z)))


def make(name, **kw):
    """Dispatcher for build_academgorodok.py: returns a NEW object for `name`, or None.

    Origin = bottom-centre of the element on the wall plane, x along the wall, +y out of the
    wall, z up; window/door units fill an opening going into -y. Extra kwargs are ignored.
    The window/door units leave out their own reveal liner (the builder makes it)."""
    g = kw.get
    if name == "window":
        unit = g("unit", "double")
        if unit not in ('double', 'triple', 'french_door', 'palladian', 'shop', 'paired', 'arched'):
            unit = 'double'
        return window(g("w", 1.4), g("h", 1.9), unit, reveal=False)
    if name == "door":
        return door(g("w", 1.5), g("h", 2.6), g("unit", "double"), reveal=False)
    if name == "surround":
        return window_surround(g("w", 1.4), g("h", 1.9), g("style", "band"))
    if name == "steps":
        return steps(g("w", 2.0), g("n", 2), g("rise", 0.15), g("tread", 0.32))
    if name == "portal_dark":
        return portal_dark(g("w", 1.6), g("h", 2.6))
    if name == "portal_side":
        return portal_side(g("w", 1.3), g("h", 2.6), arched=g("arched", True))
    if name == "console_big":
        h = g("h", 1.4)
        ob = console_big(h, g("w", 0.4), g("depth", min(0.9, 0.65 * h)))
        return _shift(ob, z=-h)
    if name == "slab":
        return slab(g("w", 3.0), g("depth", 1.0), g("t", 0.25))
    if name == "pilaster":
        return pilaster(g("h", 3.5), g("w", 0.5))
    if name == "column":
        return column(g("h", 3.5), g("d", 0.26))
    if name == "parapet":
        return parapet(g("length", 4.0), g("h", 1.0))
    if name == "railing":
        return balcony_railing(g("length", 3.0), g("h", 1.0), g("style", "bars"))
    if name == "railing_side":
        return railing_side(g("length", 1.0), g("h", 1.0), g("style", "bars"))
    if name == "balcony":
        return balcony(g("length", 2.6), g("depth", 1.0), g("style", "bars"), g("consoles", False))
    if name == "bay_window":
        return bay_window(g("w", 4.2), g("depth", 1.1), g("h", 8.0))
    if name == "oculus":
        return oculus(g("d", 0.95), hole=g("hole", False))
    if name == "swag":
        L = g("length", 1.0)
        ob = swag(L, sag=0.23 * L, drop=0.45 * L)
        zs = [v.co.z for v in ob.data.vertices]
        return _shift(ob, z=-(min(zs) + max(zs)) / 2)
    if name == "modillion":
        dep = g("depth", 0.36)
        hgt = min(0.26, max(0.18, 0.28 * dep))
        return _shift(modillion(dep, 0.12, hgt), z=-hgt)
    if name == "rafter":
        hh = 0.18
        return _shift(rafter_tail(g("length", 1.1), 0.12, hh), z=-hh)
    if name == "chimney":
        return chimney(g("w", 0.9), g("d", 0.6), g("h", 2.0))
    if name == "downpipe":
        ob = downpipe(g("h", 12.0), 0.12, 0.06)
        return _shift(ob, y=-0.12)
    if name == "vent":
        return vent(g("s", 0.3))
    if name == "quoin":
        return quoin_stack(g("courses", 11), g("course_h", 0.5))
    if name == "cartouche":
        return cartouche_1947()
    if name == "wicket_block":
        return wicket_block(**{k: v for k, v in kw.items() if k in ("length", "h", "arch_w", "arch_h", "t")})
    if name == "scroll_pylon":
        ob = scroll_pylon(g("h", 2.8), g("t", 0.5))
        if g("flip", False):
            _xf(ob, Matrix.Scale(-1, 4, (1, 0, 0)))
            ob.name = "scroll_pylon_flip"
        return ob
    if name == "fountain":
        return fountain()
    if name == "flowerbed_railing":
        return flowerbed_railing(g("radius", 5.7))
    return None


ALL = [
    ("modillion", modillion), ("rafter_tail", rafter_tail),
    ("window_double", lambda: window(1.5, 1.95, 'double')),
    ("window_triple", lambda: window(1.9, 1.95, 'triple')),
    ("window_french_door", lambda: window(1.9, 2.6, 'french_door')),
    ("window_arched", lambda: window(1.8, 2.6, 'arched')),
    ("window_shop", lambda: window(2.0, 2.2, 'shop')),
    ("surround_band", lambda: window_surround(1.5, 1.95, 'band')),
    ("surround_ears", lambda: window_surround(1.3, 1.8, 'ears')),
    ("surround_hood", lambda: window_surround(1.3, 1.8, 'hood')),
    ("surround_arch", lambda: window_surround(1.8, 2.6, 'arch')),
    ("door_double", lambda: door(1.5, 2.6, 'double')),
    ("door_arched", lambda: door(1.2, 2.6, 'arched')),
    ("steps", steps), ("portal_dark", portal_dark), ("portal_side", portal_side),
    ("console_big", console_big), ("column", column), ("pilaster", pilaster),
    ("railing_bars", lambda: balcony_railing(3.0, 1.0, 'bars')),
    ("railing_ornamental", lambda: balcony_railing(3.0, 1.0, 'ornamental')),
    ("parapet", parapet), ("oculus", oculus), ("swag", swag),
    ("quoin_stack", quoin_stack), ("cartouche_1947", cartouche_1947), ("chimney", chimney),
    ("downpipe", downpipe), ("scroll_pylon", scroll_pylon), ("wicket_block", wicket_block),
    ("fountain", fountain), ("flowerbed_railing", flowerbed_railing),
]
