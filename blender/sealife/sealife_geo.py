"""Sea-life geometry kit in pure numpy (no bpy): lofted tubes, domes, flat plates, eye caps, UV atlas.

Frame (same as the fish): head +X, back +Z, the animal's left side +Y, metres, origin on the body axis at half the
body length.  Every part keeps local uv coordinates in metres (a, b) inside its own texture rect; the Layout turns
them into atlas uvs, so LOD0 and LOD1 (rebuilt with fewer rings / sides) share one texture.
Mirrored halves / left copies / repeated instances (claws) reuse the texels of their original and carry mirror=True.
"""
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
FISH = os.path.join(os.path.dirname(HERE), "fish")
if FISH not in sys.path:
    sys.path.insert(0, FISH)

from fish_geo import curve, smoothstep, W, H  # noqa: E402  (monotone cubic keypoints, same atlas size)

PAD = 4          # px between rects (lint uv_pad)
INSET = 1.0      # px kept free inside each rect border
Y = np.array([0.0, 1.0, 0.0])


def fn(v):
    """keypoints [(s, v)], a scalar or a callable -> callable of s."""
    if callable(v):
        return v
    return curve(v)


def nrm(v):
    v = np.asarray(v, float)
    return v / np.maximum(np.linalg.norm(v, axis=-1, keepdims=True), 1e-30)


def spow(x, p):
    return np.sign(x) * np.abs(x) ** p


class Part:
    """name, kind ('body' | 'eye' | 'extra' -> slot Body, 'fin' -> slot Fins), V (n,3), F (m,3), uvl (n,2) local metres,
    rect_key, mirror (per face), B (paired-appendage flag), gadd (per-vertex addition to the flexibility),
    field(A, B) -> dict(valid, s, q, a, b, ...) used by the painter, paint dict, closed (expected watertight), ref (per-vertex
    inner reference point for the outward-normal check, or None)."""

    def __init__(self, name, kind, V, F, uvl, key, dims, field, paint, mirror=None, B=0.0, gadd=None, closed=True, ref=None):
        self.name, self.kind = name, kind
        self.V = np.asarray(V, float)
        self.F = np.asarray(F, int).reshape(-1, 3)
        self.uvl = np.asarray(uvl, float)
        self.rect_key = key
        self.dims = dims
        self.field = field
        self.paint = paint or {}
        self.mirror = np.zeros(len(self.F), bool) if mirror is None else np.asarray(mirror, bool)
        self.Bval = float(B)
        self.gadd = np.zeros(len(self.V)) if gadd is None else np.asarray(gadd, float)
        self.closed = closed
        self.ref = ref
        self.uv = None
        self.R = self.G = self.B = None
        self.minpx = 6
        self.dens = 1.0


def mirrored(p, name=None):
    """Copy of a part reflected y -> -y (winding flipped), same uvs, all faces flagged mirror."""
    q = Part(name or p.name + "_L", p.kind, p.V * [1, -1, 1], p.F[:, ::-1], p.uvl.copy(), p.rect_key, p.dims, p.field, p.paint,
             mirror=np.ones(len(p.F), bool), B=p.Bval, gadd=p.gadd.copy(), closed=p.closed,
             ref=None if p.ref is None else p.ref * [1, -1, 1])
    q.minpx, q.dens = p.minpx, p.dens
    return q


def instanced(p, name, M, t):
    """Copy transformed by 3x3 M and offset t (an instance sharing the texels; mirror-flagged)."""
    M = np.asarray(M, float)
    flip = np.linalg.det(M) < 0
    q = Part(name, p.kind, p.V @ M.T + t, p.F[:, ::-1] if flip else p.F, p.uvl.copy(), p.rect_key, p.dims, p.field, p.paint,
             mirror=np.ones(len(p.F), bool), B=p.Bval, gadd=p.gadd.copy(), closed=p.closed,
             ref=None if p.ref is None else p.ref @ M.T + t)
    q.minpx, q.dens = p.minpx, p.dens
    return q


def _orient_by_ref(V, F, ref):
    """Flip the winding of the whole face set when most normals point towards the reference points."""
    P = V[F]
    n = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0])
    cen = P.mean(1)
    r = ref[F].mean(1)
    d = np.einsum("ij,ij->i", n, cen - r)
    return F[:, ::-1] if (d < 0).sum() > (d > 0).sum() else F


# ------------------------------------------------------------------ paths
class Path:
    """Smooth centripetal Catmull-Rom path through control points, parametrised by normalised arc length s."""

    def __init__(self, ctrl, dense=400):
        C = np.asarray(ctrl, float)
        if len(C) == 2:
            C = np.array([C[0], (2 * C[0] + C[1]) / 3, (C[0] + 2 * C[1]) / 3, C[1]])
        P = np.concatenate([[2 * C[0] - C[1]], C, [2 * C[-1] - C[-2]]])
        pts = []
        for i in range(1, len(P) - 2):
            p0, p1, p2, p3 = P[i - 1], P[i], P[i + 1], P[i + 2]
            for u in np.linspace(0, 1, dense // (len(C) - 1), endpoint=False):
                u2, u3 = u * u, u * u * u
                pts.append(0.5 * ((2 * p1) + (-p0 + p2) * u + (2 * p0 - 5 * p1 + 4 * p2 - p3) * u2 + (-p0 + 3 * p1 - 3 * p2 + p3) * u3))
        pts.append(C[-1])
        pts = np.array(pts)
        seg = np.linalg.norm(np.diff(pts, axis=0), axis=1)
        cum = np.concatenate([[0], np.cumsum(seg)])
        self.length = cum[-1]
        self.s = cum / cum[-1]
        self.pts = pts

    def at(self, s):
        s = np.asarray(s, float)
        return np.stack([np.interp(s, self.s, self.pts[:, k]) for k in range(3)], -1)

    def tangent(self, s):
        e = 1e-3
        return nrm(self.at(np.clip(s + e, 0, 1)) - self.at(np.clip(s - e, 0, 1)))


def rm_frames(T, up0):
    """Rotation-minimising frames along tangents T (n,3) from an initial up vector: returns ups (n,3)."""
    ups = np.zeros_like(T)
    u = np.asarray(up0, float) - T[0] * np.dot(up0, T[0])
    u = nrm(u)
    ups[0] = u
    for i in range(1, len(T)):
        v = np.cross(T[i - 1], T[i])
        sn = np.linalg.norm(v)
        if sn > 1e-9:
            v /= sn
            ang = math.atan2(sn, np.dot(T[i - 1], T[i]))
            u = u * math.cos(ang) + np.cross(v, u) * math.sin(ang) + v * np.dot(v, u) * (1 - math.cos(ang))
        u = nrm(u - T[i] * np.dot(u, T[i]))
        ups[i] = u
    return ups


def ring_s(nr, ends, cluster=0.5, extra=()):
    """Ring positions along s.  apex ends keep the endpoints for the apex vertex."""
    a0 = ends[0] == "apex"
    a1 = ends[1] == "apex"
    k = nr + int(a0) + int(a1)
    u = np.linspace(0, 1, k)
    s = (1 - cluster) * u + cluster * (0.5 - 0.5 * np.cos(np.pi * u))
    s = s[int(a0): len(s) - int(a1)]
    if len(extra):
        ex = np.asarray(extra, float)
        s = s[np.abs(s[:, None] - ex[None, :]).min(1) > 0.005]     # base rings too close to a boundary ring would make slivers
        s = np.concatenate([s, ex])
        s = s[(s > 1e-4 if a0 else s >= 0) & (s < 1 - 1e-4 if a1 else s <= 1)]
        s = np.sort(s)
        keep = np.concatenate([[True], np.diff(s) > 2e-4])
        s = s[keep]
    return np.sort(s)


# ------------------------------------------------------------------ the kit
class Kit:
    def __init__(self, spec, lod):
        self.spec = spec
        self.L = spec["length"]
        self.Lr = spec.get("r_len", self.L)            # length that maps x -> R (t)
        self.lod = lod
        self.seed = spec["seed"]
        fl = spec.get("flex", [(0, 0.0), (1, 0.1)])
        self.flex = fn(fl)

    def pick(self, a, b):
        return a if self.lod == 0 else b

    # -------------------------------------------------------------- tube
    def tube(self, name, ctrl, w, top=None, bot=None, n=2.0, m=6, nr=10, ends=("apex", "apex"), half=True,
             kind="body", key=None, paint=None, up=(0, 0, 1), roll=0.0, B=0.0, gadd=None, mod=None, shift=None,
             cluster=0.5, extra_s=(), deform=None, minpx=6, dens=1.0, closed=None):
        """Lofted tube along a smooth path.
        half=True: bilaterally symmetric body whose path lies in the XZ plane; the +Y half is real, the -Y half mirrors its texels.
        half=False: an appendage with a full ring of m sides (own texels all round, seam duplicated).
        w/top/bot: half-width / height above / below the path (keypoints over s); n superellipse exponent (keypoints ok).
        ends: 'apex' (closes to a point at the path end), 'edge' (profile w = 0 there: a sharp edge), 'open'.
        mod(s) -> (fw, fh) multiplies the section (segment grooves, saw-tooth tergites); shift(s): offset along up.
        deform(V, s, q) -> V lets a species bend single vertices (q = 0 top .. 0.5 side .. 1 bottom on the +side)."""
        path = Path(ctrl)
        Lp = path.length
        wf = fn(w)
        tf = fn(top if top is not None else w)
        bf = fn(bot if bot is not None else (top if top is not None else w))
        nf = fn(n)
        S = ring_s(nr, ends, cluster, extra_s)
        C = path.at(S)
        T = path.tangent(S)
        if half:
            ups = nrm(np.cross(Y, T))
            sides = np.broadcast_to(Y, T.shape).copy()
        else:
            dense = np.linspace(0, 1, 200)
            ud = rm_frames(path.tangent(dense), np.asarray(up, float))
            ups = nrm(np.stack([np.interp(S, dense, ud[:, k]) for k in range(3)], -1))
            ups = nrm(ups - T * np.einsum("ij,ij->i", ups, T)[:, None])
            sides = np.cross(T, ups)
            if roll:
                r = np.radians(roll)
                ups, sides = ups * math.cos(r) + sides * math.sin(r), sides * math.cos(r) - ups * math.sin(r)
        ww, tt, bb, nn = wf(S), tf(S), bf(S), nf(S)
        if mod is not None:
            fw, fh = mod(S)
            ww, tt, bb = ww * fw, tt * fh, bb * fh
        if shift is not None:
            C = C + ups * fn(shift)(S)[:, None]
        M = m                       # half: segments per half ring; full ring: number of sides
        th = np.pi * np.arange(m + 1) / m if half else 2 * np.pi * np.arange(M + 1) / M
        nr_ = len(S)
        ring = np.zeros((nr_, len(th), 3))
        for i in range(nr_):
            p = 2.0 / nn[i]
            ct, st = np.cos(th), np.sin(th)
            zl = np.where(ct >= 0, tt[i], bb[i]) * spow(ct, p)
            yl = ww[i] * spow(st, p)
            ring[i] = C[i] + ups[i] * zl[:, None] + sides[i] * yl[:, None]
        q = th / (np.pi if half else 2 * np.pi)
        if deform is not None:
            ring = deform(ring, S, q)
        # arc length around each ring (uv b)
        arcs = np.concatenate([np.zeros((nr_, 1)), np.cumsum(np.linalg.norm(np.diff(ring, axis=1), axis=2), axis=1)], 1)
        arcmax = arcs[:, -1]
        # vertices
        Vs, uvl, ref, sv = [], [], [], []
        idx = np.zeros((nr_, len(th)), int)
        k = 0
        for i in range(nr_):
            for j in range(len(th)):
                idx[i, j] = k
                Vs.append(ring[i, j])
                uvl.append((S[i] * Lp, arcs[i, j]))
                ref.append(C[i])
                sv.append(S[i])
                k += 1
        F, Fm = [], []
        nh = len(th) - 1
        for i in range(nr_ - 1):
            for j in range(nh):
                a, b, c, d = idx[i, j], idx[i, j + 1], idx[i + 1, j + 1], idx[i + 1, j]
                F += [(a, b, c), (a, c, d)]
        apex = {}
        for e, si, ii in ((0, 0.0, 0), (1, 1.0, nr_ - 1)):
            if ends[e] != "apex":
                continue
            P = path.at(si)
            apex[e] = k
            Vs.append(P)
            uvl.append((si * Lp, arcmax[ii] * 0.5))
            ref.append(C[ii])
            sv.append(si)
            k += 1
            for j in range(nh):
                a, b = idx[ii, j], idx[ii, j + 1]
                F.append((apex[e], b, a) if e == 0 else (apex[e], a, b))
        Vs, uvl, ref, sv = np.array(Vs), np.array(uvl), np.array(ref), np.array(sv)
        F = np.array(F)
        for e, ii in ((0, 0), (1, nr_ - 1)):
            if e in apex:
                ref[apex[e]] = C[ii]
        F = _orient_by_ref(Vs, F, ref)
        mir = np.zeros(len(F), bool)
        if half:
            # -Y half: mirror vertices j = 1 .. m-1 (top / bottom seam shared), plus shared apexes
            mapv = {}
            Vm, uvm, refm, svm = [], [], [], []
            for i in range(nr_):
                for j in range(1, m):
                    mapv[idx[i, j]] = k
                    Vm.append(ring[i, j] * [1, -1, 1])
                    uvm.append(uvl[idx[i, j]])
                    refm.append(ref[idx[i, j]] * [1, -1, 1])
                    svm.append(S[i])
                    k += 1
            Fm = np.array([[mapv.get(int(v), int(v)) for v in f] for f in F])[:, ::-1]
            Vs = np.concatenate([Vs, Vm])
            uvl = np.concatenate([uvl, uvm])
            ref = np.concatenate([ref, refm])
            sv = np.concatenate([sv, svm])
            mir = np.concatenate([mir, np.ones(len(Fm), bool)])
            F = np.concatenate([F, Fm])
        dims = (Lp, max(arcmax.max(), 1e-6))
        Sx = np.concatenate([[0.0] if ends[0] == "apex" else [], S, [1.0] if ends[1] == "apex" else []])
        Ax = np.concatenate([[0.0] if ends[0] == "apex" else [], arcmax, [0.0] if ends[1] == "apex" else []])

        def field(A, Bc, Lp=Lp, Sx=Sx, Ax=Ax):
            s = A / Lp
            am = np.interp(s, Sx, Ax)
            qq = Bc / np.maximum(am, 1e-9)
            valid = (s >= -1e-6) & (s <= 1 + 1e-6) & (qq <= 1.0 + 1e-6)
            return dict(valid=valid, s=np.clip(s, 0, 1), q=np.clip(qq, 0, 1), a=A, b=Bc)
        g = np.zeros(len(Vs)) if gadd is None else fn(gadd)(sv)
        p = Part(name, kind, Vs, F, uvl, key or name, dims, field, paint, mirror=mir, B=B, gadd=g,
                 closed=(ends[0] != "open" and ends[1] != "open") if closed is None else closed, ref=ref)
        p.minpx, p.dens = minpx, dens
        p.path = path
        return p

    # -------------------------------------------------------------- eye cap
    def eye(self, name, C, r, axis, phimax=1.9, nlat=3, nseg=8, scale=(1, 1, 1), key=None, paint=None, up=(0, 0, 1),
            B=0.0, minpx=10, dens=2.0, kind="eye"):
        """Spherical / ellipsoidal cap around `axis` (the pupil at the pole), open rim sunk into the head or stalk.
        uv: azimuthal equidistant disc (no folds) so the painter can draw a pupil / facets by the radius."""
        n = nrm(axis)
        ax = nrm(np.asarray(up, float) - n * np.dot(up, n))
        bx = np.cross(n, ax)
        pts = [(0.0, 0.0, 1.0)]
        uv = [(0.0, 0.0)]
        for i in range(1, nlat + 1):
            phi = phimax * i / nlat
            for k in range(nseg):
                az = 2 * math.pi * k / nseg
                pts.append((math.sin(phi) * math.cos(az), math.sin(phi) * math.sin(az), math.cos(phi)))
                uv.append(((i / nlat) * math.cos(az), (i / nlat) * math.sin(az)))
        pts = np.array(pts) * np.asarray(scale, float)
        V = np.asarray(C, float) + (pts[:, 0:1] * ax + pts[:, 1:2] * bx + pts[:, 2:3] * n) * r
        R0 = r * phimax
        uvl = (np.array(uv) + 1) * R0
        F = []
        for k in range(nseg):
            F.append((0, 1 + k, 1 + (k + 1) % nseg))
        for i in range(1, nlat):
            for k in range(nseg):
                a, b = 1 + (i - 1) * nseg + k, 1 + (i - 1) * nseg + (k + 1) % nseg
                c, d = 1 + i * nseg + (k + 1) % nseg, 1 + i * nseg + k
                F += [(a, d, c), (a, c, b)]
        F = np.array(F)
        ref = np.broadcast_to(np.asarray(C, float), V.shape).copy()
        F = _orient_by_ref(V, F, ref)

        def field(A, Bc, R0=R0):
            x, y = A / R0 - 1, Bc / R0 - 1
            rho = np.sqrt(x * x + y * y)
            return dict(valid=rho <= 1.0, s=np.clip(rho, 0, 1), q=(np.arctan2(y, x) / (2 * np.pi)) % 1.0, a=A, b=Bc)
        p = Part(name, kind, V, F, uvl, key or name, (2 * R0, 2 * R0), field, paint, B=B, closed=False, ref=ref)
        p.minpx, p.dens = minpx, dens
        return p

    # -------------------------------------------------------------- dome (crab carapace)
    def dome(self, name, outline, cx, ztop, zbot, nring=6, m=12, key=None, paint=None, paint_bot=None, ringpow=0.8,
             kind="body", dens=1.0):
        """Closed lens over a star-shaped outline (control points of the +Y half, front (y=0) round to back (y=0)).
        ztop(rho, u), zbot(rho, u): heights over the normalised radius rho (0 centre .. 1 rim) and u (0 front .. 1 back along the outline).
        Top and bottom surfaces get their own rects (planar projections), the -Y half mirrors them; they meet at a sharp rim."""
        pth = Path(np.array([(x, y, 0.0) for x, y in outline]))
        u = np.linspace(0, 1, m + 1)
        O = pth.at(u)[:, :2]
        O[0, 1] = O[-1, 1] = 0.0
        cen = np.array([cx, 0.0])
        rr = np.linspace(0, 1, nring + 1)[1:] ** ringpow
        rr[-1] = 1.0
        parts = []
        xs = O[:, 0]
        xmax, xmin = max(xs.max(), cx), min(xs.min(), cx)
        ymax = O[:, 1].max()
        ang = np.arctan2(O[:, 1], O[:, 0] - cx)          # 0 .. pi, monotone for a star-shaped outline
        rad = np.linalg.norm(O - cen, axis=1)
        for surf, zf, pa in (("top", ztop, paint), ("bot", zbot, paint_bot or paint)):
            Vs, uvl, F = [], [], []
            idx = np.zeros((nring + 1, m + 1), int)
            Vs.append((cx, 0.0, float(zf(np.array(0.0), np.array(0.5)))))
            uvl.append((xmax - cx, 0.0))
            idx[0, :] = 0
            k = 1
            for i, r in enumerate(rr, start=1):
                for j in range(m + 1):
                    p2 = cen + (O[j] - cen) * r
                    z = float(zf(np.array(r), np.array(u[j])))
                    idx[i, j] = k
                    Vs.append((p2[0], p2[1], z))
                    uvl.append((xmax - p2[0], p2[1]))
                    k += 1
            for i in range(nring):
                for j in range(m):
                    a, b, c, d = idx[i, j], idx[i, j + 1], idx[i + 1, j + 1], idx[i + 1, j]
                    if i == 0:
                        F.append((a, c, d))
                    else:
                        F += [(a, b, c), (a, c, d)]
            Vs, uvl, F = np.array(Vs), np.array(uvl), np.array(F)
            # outward: top normals +Z, bottom -Z
            P = Vs[F]
            nz = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0])[:, 2]
            want = 1 if surf == "top" else -1
            if (nz * want < 0).sum() > (nz * want > 0).sum():
                F = F[:, ::-1]
            # mirror half: vertices with y > 0
            mapv = {}
            Vm, uvm = [], []
            for v in range(len(Vs)):
                if Vs[v, 1] > 1e-12:
                    mapv[v] = len(Vs) + len(Vm)
                    Vm.append(Vs[v] * [1, -1, 1])
                    uvm.append(uvl[v])
            Fm = np.array([[mapv.get(int(v), int(v)) for v in f] for f in F])[:, ::-1]
            V2 = np.concatenate([Vs, Vm])
            U2 = np.concatenate([uvl, uvm])
            F2 = np.concatenate([F, Fm])
            mir = np.concatenate([np.zeros(len(F), bool), np.ones(len(Fm), bool)])

            def field(A, Bc, cx=cx, xmax=xmax, ang=ang, rad=rad, xr=xmax - xmin):
                x = xmax - A
                y = Bc
                a = np.arctan2(y, x - cx)
                rim = np.interp(a, ang, rad)
                rho = np.sqrt((x - cx) ** 2 + y ** 2) / np.maximum(rim, 1e-9)
                return dict(valid=rho <= 1.0 + 1e-6, s=np.clip(A / xr, 0, 1), q=np.clip(rho, 0, 1), a=A, b=Bc, y=y)
            cref = np.array([cx, 0.0, 0.5 * (float(ztop(np.array(0.0), np.array(0.5))) + float(zbot(np.array(0.0), np.array(0.5))))])
            p = Part("%s_%s" % (name, surf), kind, V2, F2, U2, "%s_%s" % (key or name, surf), (xmax - xmin, ymax), field, pa,
                     mirror=mir, closed=False, ref=np.broadcast_to(cref, V2.shape).copy())
            p.dens = dens
            p.lens_group = name
            parts.append(p)
        return parts

    # -------------------------------------------------------------- flat plate (Fins slot)
    def plate(self, name, ctrl, shape, nu=4, nv=2, normal=(0, 0, 1), key=None, paint=None, B=0.0, gadd=None, cup=0.0,
              margin=1.12, floor=0.25, kind="fin", minpx=8, dens=1.0):
        """Flat card along a path (base -> tip) with half-width `shape` keypoints over s; its outline is cut by the
        texture alpha (painter: inside |v| <= shape(s)).  The card is shape*margin wide, at least floor*max wide."""
        path = Path(ctrl)
        Lp = path.length
        sh = fn(shape)
        su = np.linspace(0, 1, nu + 1)
        T = path.tangent(su)
        nn = np.asarray(normal, float)
        sides = nrm(np.cross(nn, T))
        dense = np.linspace(0, 1, 101)
        hmax = sh(dense).max()
        hw = np.maximum(sh(su) * margin, floor * hmax)
        C = path.at(su)
        Vs, uvl, idx = [], [], np.zeros((nu + 1, nv + 1), int)
        k = 0
        for i in range(nu + 1):
            for j in range(nv + 1):
                v = -1 + 2 * j / nv
                P = C[i] + sides[i] * v * hw[i]
                if cup:
                    P = P + nn * cup * hmax * (v * v - 0.5)
                Vs.append(P)
                uvl.append((su[i] * Lp, (v * hw[i]) + hmax * margin))
                idx[i, j] = k
                k += 1
        F = []
        for i in range(nu):
            for j in range(nv):
                a, b, c, d = idx[i, j], idx[i, j + 1], idx[i + 1, j + 1], idx[i + 1, j]
                F += [(a, b, c), (a, c, d)]
        F = np.array(F)
        P_ = np.array(Vs)[F]
        nz = np.cross(P_[:, 1] - P_[:, 0], P_[:, 2] - P_[:, 0]) @ nn
        if (nz < 0).sum() > (nz > 0).sum():
            F = F[:, ::-1]
        sv = np.repeat(su, nv + 1)

        def field(A, Bc, Lp=Lp, hm=hmax * margin):
            s = np.clip(A / Lp, 0, 1)
            off = Bc - hm
            vv = off / np.maximum(sh(s), 1e-9)
            return dict(valid=np.ones(A.shape, bool), s=s, q=np.clip(0.5 + 0.5 * vv, -1, 2), v=vv, a=A, b=Bc)
        g = np.zeros(len(Vs)) if gadd is None else fn(gadd)(sv)
        p = Part(name, kind, np.array(Vs), F, np.array(uvl), key or name, (Lp, 2 * hmax * margin), field, paint, B=B, gadd=g,
                 closed=False, ref=None)
        p.minpx, p.dens = minpx, dens
        p.is_plate = True
        return p

    def pair(self, p):
        """The part (built on the +Y side) and its mirrored -Y copy."""
        return [p, mirrored(p)]

    # -------------------------------------------------------------- finish
    def finish(self, parts):
        for p in parts:
            x = p.V[:, 0]
            p.R = np.clip(0.5 - x / self.Lr, 0, 1)
            p.G = np.clip(self.flex(p.R) + p.gadd, 0, 1)
            p.B = np.full(len(p.V), p.Bval)
        return parts


# ------------------------------------------------------------------ helpers for species
def saw(at, amp=0.08, groove=0.06, gw=0.012, power=1.0):
    """Segment modifier for tube(mod=...): within each segment [at_k, at_k+1] the section grows towards its rear edge
    (overlapping tergites), with a groove at each boundary.  Returns (fw, fh) multipliers."""
    at = np.asarray(at, float)

    def f(s):
        s = np.asarray(s, float)
        fw = np.ones_like(s)
        k = np.clip(np.searchsorted(at, s, side="right") - 1, 0, len(at) - 2)
        inside = (s >= at[0]) & (s <= at[-1])
        frac = np.clip((s - at[k]) / np.maximum(at[k + 1] - at[k], 1e-9), 0, 1)
        fw = fw + inside * amp * (frac ** power - 0.5)
        for a in at[1:-1]:
            fw = fw - groove * np.exp(-((s - a) / gw) ** 2)
        return fw, fw
    return f


def seg_rings(at, eps=0.004):
    """Extra ring positions so the segment boundaries are resolved."""
    out = []
    for a in at:
        out += [a - eps, a + eps * 0.6]
    return [o for o in out if 0 < o < 1]


def rotz(deg):
    a = math.radians(deg)
    return np.array([[math.cos(a), -math.sin(a), 0], [math.sin(a), math.cos(a), 0], [0, 0, 1]])


# ------------------------------------------------------------------ atlas
class Layout:
    """Shelf-packs one rect per rect_key at a common texel density (px per metre, per-key `dens` boost and `minpx` floor)."""

    def __init__(self, parts):
        req = {}
        for p in parts:
            if p.rect_key in req:
                continue
            if p.mirror.all():
                continue
            req[p.rect_key] = (p.dims[0], p.dims[1], p.minpx, p.dens)
        for p in parts:            # keys only present as mirror copies (should not happen)
            if p.rect_key not in req:
                req[p.rect_key] = (p.dims[0], p.dims[1], p.minpx, p.dens)
        self.req = req
        lo, hi = 1.0, 1e7
        best = None
        for _ in range(60):
            mid = math.sqrt(lo * hi)
            r = self._pack(mid)
            if r is None:
                hi = mid
            else:
                lo, best = mid, r
        self.fs = lo
        self.rects = best
        self.scale = {}
        for k, (wm, hm, mp, de) in req.items():
            x0, y0, w, h = self.rects[k]
            self.scale[k] = ((w - 2 * INSET) / wm, (h - 2 * INSET) / hm)

    def _size(self, fs, wm, hm, mp, de):
        return int(max(math.ceil(wm * fs * de + 2 * INSET), mp)), int(max(math.ceil(hm * fs * de + 2 * INSET), mp))

    def _pack(self, fs):
        items = sorted(((k,) + self._size(fs, *v) for k, v in self.req.items()), key=lambda t: (-t[2], -t[1]))
        x, y, shelf = 2, 2, 0
        out = {}
        for k, w, h in items:
            if w > W - 4 or h > H - 4:
                return None
            if x + w > W - 2:
                x, y = 2, y + shelf + PAD
                shelf = 0
            if y + h > H - 2:
                return None
            out[k] = (x, y, w, h)
            x += w + PAD
            shelf = max(shelf, h)
        return out

    def apply(self, parts):
        for p in parts:
            x0, y0, w, h = self.rects[p.rect_key]
            sx, sy = self.scale[p.rect_key]
            px = x0 + INSET + p.uvl[:, 0] * sx
            py = y0 + INSET + p.uvl[:, 1] * sy
            p.uv = np.stack([px / W, 1 - py / H], 1)
        return parts
