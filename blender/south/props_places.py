"""Places on the Sambian coast (the south): belvedere, viewer, bird_hide, fisher_shed, net_rack, picnic_table.

Built with the North's kit and atlas (blender/north/northkit.py, northtex.py; no atlas change). Frame as everywhere: metres,
Z up, Blender -Y is the front, origin on the ground under the middle (each kind's notes say what "the middle" is), transforms
applied. Every face here is mapped by its own planar projection at a fixed metres-per-tile scale (long faces are cut into
pieces no longer than one tile, so nothing is stretched), and every face is wound outward explicitly, so the kit's
normal recalculation is skipped (thin sheets such as the nets keep both skins the right way round).
"""
import math
import random

import bmesh
from mathutils import Matrix, Vector

import northtex as T
from northkit import TILE_M, Builder, V

# metres one tile window covers (the kit's table, concrete = "ash", rope a little coarser for thin cords)
M = dict(TILE_M)
M.update({"ash": 1.0, "stone": 0.9, "rope": 0.3, "wood": 1.2, "wood_dark": 1.2, "endgrain": 0.35})
WOODS = ("wood", "wood_dark", "plank_edge", "bark")


def rz(a):
    return Matrix.Rotation(a, 3, "Z")


def rx(a):
    return Matrix.Rotation(a, 3, "X")


def ry(a):
    return Matrix.Rotation(a, 3, "Y")


def lerp(a, b, t):
    return a + (b - a) * t


class PB(Builder):
    """The kit's Builder, finishing without recalc_face_normals (faces here are wound outward on purpose)."""

    def finish(self, weld=0.0005):
        bm = self.bm
        bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=weld)
        bad = [f for f in bm.faces if f.calc_area() < 1e-9]
        if bad:
            bmesh.ops.delete(bm, geom=bad, context="FACES")
        bm.normal_update()
        for f in bm.faces:
            f.smooth = True
        for e in bm.edges:
            lf = e.link_faces
            if len(lf) != 2 or lf[0].normal.angle(lf[1].normal, 0.0) > math.radians(44):
                e.smooth = False
        self.fit_uvs()
        import bpy
        mesh = bpy.data.meshes.new(self.name)
        bm.to_mesh(mesh)
        mesh.update()
        return mesh


# =========================================================================================================
# mapping and primitives
# =========================================================================================================

def newell(P):
    n = Vector((0, 0, 0))
    for i in range(len(P)):
        a, c = P[i], P[(i + 1) % len(P)]
        n.x += (a.y - c.y) * (a.z + c.z)
        n.y += (a.z - c.z) * (a.x + c.x)
        n.z += (a.x - c.x) * (a.y + c.y)
    return n


def pmap(b, tile, pts, ua, va, m=None, off=None):
    """Planar UVs for pts on axes ua, va at m metres per tile window, offset at random inside the window; a face larger
    than the window is scaled down uniformly (callers cut long faces first)."""
    if tile in T.SWATCH:
        return [T.swatch_uv(tile)] * len(pts)
    u0, v0, du, dv = b.win(tile)
    m = m or M.get(tile, 1.0)
    A = [p.dot(ua) for p in pts]
    B = [p.dot(va) for p in pts]
    a0, b0 = min(A), min(B)
    la, lb = max(A) - a0, max(B) - b0
    s = min(1.0 / m, 0.97 / max(la, 1e-6), 0.97 / max(lb, 1e-6))
    off = off or (b.rnd.random(), b.rnd.random())
    ou, ov = off[0] * (1 - la * s), off[1] * (1 - lb * s)
    return [(u0 + (ou + (a - a0) * s) * du, v0 + (ov + (c - b0) * s) * dv) for a, c in zip(A, B)]


def poly(b, P, tile, inside=None, out=None, grain=None, m=None):
    """One planar polygon (any vertex count), wound so its normal points away from `inside` / along `out`."""
    P = [Vector(p) for p in P]
    n = newell(P)
    if n.length < 1e-12:
        return
    ctr = sum(P, Vector()) / len(P)
    ref = out if out is not None else (ctr - Vector(inside)) if inside is not None else None
    if ref is not None and n.dot(Vector(ref)) < 0:
        P, n = P[::-1], -n
    n.normalize()
    if grain is not None:
        g = Vector(grain)
        ua = g - n * g.dot(n)
        if ua.length < 1e-6:
            ua = max(((P[(i + 1) % len(P)] - P[i]) for i in range(len(P))), key=lambda e: e.length)
    else:
        ua = max(((P[(i + 1) % len(P)] - P[i]) for i in range(len(P))), key=lambda e: e.length)
    ua = (ua - n * ua.dot(n)).normalized()
    va = n.cross(ua).normalized()
    b.face(P, pmap(b, tile, P, ua, va, m))


def quad(b, P, tile, inside=None, out=None, grain=None, m=None):
    """A quad cut into pieces no larger than the tile's metres, each mapped planar (U along `grain` or the longest edge)."""
    P = [Vector(p) for p in P]
    n = newell(P)
    if n.length < 1e-12:
        return
    ctr = sum(P, Vector()) / 4
    ref = out if out is not None else (ctr - Vector(inside)) if inside is not None else None
    if ref is not None and n.dot(Vector(ref)) < 0:
        P, n = P[::-1], -n
    if tile in T.SWATCH:
        b.face(P, [T.swatch_uv(tile)] * 4)
        return
    # start so that P0 -> P1 runs along the grain (or the longest edge)
    if grain is not None:
        g = Vector(grain).normalized()
        k = max(range(4), key=lambda i: abs((P[(i + 1) % 4] - P[i]).normalized().dot(g)) if (P[(i + 1) % 4] - P[i]).length > 1e-9 else -1)
    else:
        k = max(range(4), key=lambda i: (P[(i + 1) % 4] - P[i]).length)
    P = P[k:] + P[:k]
    m = m or M.get(tile, 1.0)
    lu = max((P[1] - P[0]).length, (P[2] - P[3]).length)
    lv = max((P[3] - P[0]).length, (P[2] - P[1]).length)
    nu = max(1, int(math.ceil(lu / (0.95 * m) - 1e-6)))
    nv = max(1, int(math.ceil(lv / (0.95 * m) - 1e-6)))
    nn = newell(P).normalized()
    ua = (P[1] - P[0])
    if ua.length < 1e-9:
        ua = P[2] - P[3]
    ua = (ua - nn * ua.dot(nn)).normalized()
    va = nn.cross(ua).normalized()

    def Q(s, t):
        return P[0] * ((1 - s) * (1 - t)) + P[1] * (s * (1 - t)) + P[2] * (s * t) + P[3] * ((1 - s) * t)
    # cuts along U at a random phase, so neighbouring boards never show their texture seams in one line
    if nu > 1:
        r = b.rnd.uniform(0.15, 0.85)
        cu = [0.0] + [(i - r) / nu for i in range(1, nu + 1)] + [1.0]
    else:
        cu = [0.0, 1.0]
    cv = [j / nv for j in range(nv + 1)]
    for i in range(len(cu) - 1):
        for j in range(nv):
            c = [Q(cu[i], cv[j]), Q(cu[i + 1], cv[j]), Q(cu[i + 1], cv[j + 1]), Q(cu[i], cv[j + 1])]
            b.face(c, pmap(b, tile, c, ua, va, m))


def end_tile(tile):
    return "endgrain" if tile in ("wood", "plank_edge") else tile


def hexa(b, P8, tile, grain=None, tiles=None, skip=(), end=None):
    """A six-faced solid from 8 corners [b0 b1 b2 b3 t0 t1 t2 t3] (t_i above b_i). Faces across the grain take `end`
    (endgrain for wood); tiles = {'top'|'bottom'|'side'|'end': tile} overrides."""
    P = [Vector(p) for p in P8]
    c = sum(P, Vector()) / 8
    tiles = tiles or {}
    g = Vector(grain).normalized() if grain is not None else None
    endt = tiles.get("end", end or end_tile(tile))
    ms = [M.get(t, 1.0) for t in (tile, tiles.get("top", tile), tiles.get("bottom", tile), tiles.get("side", tile))
          if t not in T.SWATCH]
    mm = min(ms) if ms else 1.0
    faces = {"bottom": [0, 1, 2, 3], "top": [4, 5, 6, 7], "s0": [0, 1, 5, 4], "s1": [1, 2, 6, 5], "s2": [2, 3, 7, 6],
             "s3": [3, 0, 4, 7]}
    for nm, idx in faces.items():
        if nm in skip:
            continue
        Q = [P[i] for i in idx]
        n = newell(Q)
        if n.length < 1e-12:
            continue
        n.normalize()
        t = tiles.get(nm) or (tiles.get("top") if nm == "top" else tiles.get("bottom") if nm == "bottom" else tiles.get("side"))
        if g is not None and abs(n.dot(g)) > 0.7:
            t = endt
            quad(b, Q, t, inside=c, m=M.get(t, 1.0))
            continue
        t = t or tile
        quad(b, Q, t, inside=c, grain=g, m=mm if t not in T.SWATCH else None)


def box(b, c, size, tile="wood", rot=None, grain=None, tiles=None, skip=(), end=None):
    """Axis box of `size` centred at c, turned by `rot` about its centre; grain = axis index the U runs along
    (default: the longest)."""
    c = Vector(c)
    rot = rot if rot is not None else Matrix.Identity(3)
    hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
    if grain is None:
        grain = max(range(3), key=lambda i: size[i])
    loc = [(-hx, -hy, -hz), (hx, -hy, -hz), (hx, hy, -hz), (-hx, hy, -hz), (-hx, -hy, hz), (hx, -hy, hz), (hx, hy, hz), (-hx, hy, hz)]
    P = [c + rot @ Vector(p) for p in loc]
    ax = [Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))][grain]
    hexa(b, P, tile, grain=rot @ ax, tiles=tiles, skip=skip, end=end)


def beam(b, p0, p1, w, h, tile="wood", up=None, tiles=None, end=None, roll=0.0):
    """A rectangular member from p0 to p1 (centre line), w across, h 'up' (perpendicular to the axis, toward `up`)."""
    p0, p1 = Vector(p0), Vector(p1)
    ax = (p1 - p0)
    L = ax.length
    z = ax.normalized()
    upv = Vector(up) if up is not None else (Vector((0, 0, 1)) if abs(z.z) < 0.95 else Vector((0, 1, 0)))
    x = z.cross(upv).normalized()
    y = x.cross(z).normalized()
    if roll:
        R = Matrix.Rotation(roll, 3, z)
        x, y = R @ x, R @ y
    pts = []
    for (a, c) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        pts.append((a * w / 2, c * h / 2))
    P = [p0 + x * a + y * c for a, c in pts] + [p1 + x * a + y * c for a, c in pts]
    # order as [b0 b1 b2 b3 t0 t1 t2 t3]: the p0 loop is "bottom", the p1 loop "top"
    hexa(b, P, tile, grain=z, tiles=tiles, end=end)
    return L


def frame_of(z):
    h = Vector((0, 0, 1)) if abs(z.z) < 0.95 else Vector((1, 0, 0))
    x = z.cross(h).normalized()
    y = z.cross(x).normalized()
    return x, y


def pole(b, p0, p1, r0, r1=None, seg=8, tile="wood", cap0=True, cap1=True, cap_tile=None, wobble=0.0, m=None, rot0=0.0):
    """A (tapered) round member, cut along its length into pieces of at most one tile, V continuous round the girth."""
    p0, p1 = Vector(p0), Vector(p1)
    r1 = r0 if r1 is None else r1
    ax = p1 - p0
    L = ax.length
    z = ax.normalized()
    x, y = frame_of(z)
    m = m or M.get(tile, 1.0)
    circ = 2 * math.pi * max(r0, r1)
    s = min(1.0 / m, 0.97 / circ)                       # tile units per metre
    n = max(1, int(math.ceil(L * s / 0.95 - 1e-6)))
    wob = [1.0 + (b.rnd.uniform(-wobble, wobble) if wobble else 0.0) for _ in range(seg)]
    ang = [rot0 + 2 * math.pi * i / seg for i in range(seg)]

    def ring(t):
        c = p0 + ax * t
        r = lerp(r0, r1, t)
        return [c + (x * math.cos(a) + y * math.sin(a)) * r * w for a, w in zip(ang, wob)], c
    rings = [ring(j / n) for j in range(n + 1)]
    sw = tile in T.SWATCH
    u0, v0, du, dv = b.win(tile) if not sw else (0, 0, 0, 0)
    for j in range(n):
        (ra, ca), (rb, cb) = rings[j], rings[j + 1]
        pl = L / n
        ou = b.rnd.random() * max(0.0, 1 - pl * s)
        ov = b.rnd.random() * max(0.0, 1 - circ * s)
        for i in range(seg):
            i2 = (i + 1) % seg
            q = [ra[i], ra[i2], rb[i2], rb[i]]
            nq = newell(q)
            mid = (ca + cb) / 2
            if nq.dot(sum(q, Vector()) / 4 - mid) < 0:
                q = q[::-1]
                rev = True
            else:
                rev = False
            if sw:
                uv = [T.swatch_uv(tile)] * 4
            else:
                va, vb = ov + circ * s * i / seg, ov + circ * s * (i + 1) / seg
                ua, ub = ou, ou + pl * s
                uv = [(u0 + ua * du, v0 + va * dv), (u0 + ua * du, v0 + vb * dv), (u0 + ub * du, v0 + vb * dv), (u0 + ub * du, v0 + va * dv)]
                if rev:
                    uv = uv[::-1]
            b.face(q, uv)
    ct = cap_tile or end_tile(tile)
    for on, (rr, cc), outv in ((cap0, rings[0], -z), (cap1, rings[-1], z)):
        if on:
            poly(b, rr, ct, out=outv, m=M.get(ct, 1.0))


def tube(b, pts, r, seg=5, tile="rope", m=None, caps=True, rscale=None):
    """A round tube along a polyline (parallel-transport frame), resampled so each piece spans at most one tile."""
    pts = [Vector(p) for p in pts]
    m = m or M.get(tile, 1.0)
    circ = 2 * math.pi * r
    s = min(1.0 / m, 0.97 / circ)
    maxl = 0.95 / s
    rs = [rscale(i, len(pts)) if rscale else 1.0 for i in range(len(pts))]
    P, Rr = [pts[0]], [rs[0]]
    for i in range(1, len(pts)):
        d = (pts[i] - pts[i - 1]).length
        k = max(1, int(math.ceil(d / maxl - 1e-6)))
        for j in range(1, k + 1):
            P.append(pts[i - 1].lerp(pts[i], j / k))
            Rr.append(lerp(rs[i - 1], rs[i], j / k))
    n = len(P)
    tans = [(P[min(n - 1, i + 1)] - P[max(0, i - 1)]).normalized() for i in range(n)]
    nrm, _ = frame_of(tans[0])
    rings = []
    for i in range(n):
        t = tans[i]
        nrm = nrm - t * nrm.dot(t)
        if nrm.length < 1e-6:
            nrm, _ = frame_of(t)
        nrm.normalize()
        bn = t.cross(nrm).normalized()
        rings.append([P[i] + (nrm * math.cos(2 * math.pi * k / seg) + bn * math.sin(2 * math.pi * k / seg)) * r * Rr[i] for k in range(seg)])
    sw = tile in T.SWATCH
    u0, v0, du, dv = b.win(tile) if not sw else (0, 0, 0, 0)
    for i in range(n - 1):
        pl = (P[i + 1] - P[i]).length
        ou = b.rnd.random() * max(0.0, 1 - pl * s)
        ov = b.rnd.random() * max(0.0, 1 - circ * s)
        mid = (P[i] + P[i + 1]) / 2
        for k in range(seg):
            k2 = (k + 1) % seg
            q = [rings[i][k], rings[i][k2], rings[i + 1][k2], rings[i + 1][k]]
            if sw:
                uv = [T.swatch_uv(tile)] * 4
            else:
                uv = [(u0 + ou * du, v0 + (ov + circ * s * k / seg) * dv), (u0 + ou * du, v0 + (ov + circ * s * (k + 1) / seg) * dv),
                      (u0 + (ou + pl * s) * du, v0 + (ov + circ * s * (k + 1) / seg) * dv), (u0 + (ou + pl * s) * du, v0 + (ov + circ * s * k / seg) * dv)]
            if newell(q).dot(sum(q, Vector()) / 4 - mid) < 0:
                q, uv = q[::-1], uv[::-1]
            b.face(q, uv)
    if caps:
        poly(b, rings[0], tile, out=-tans[0])
        poly(b, rings[-1], tile, out=tans[-1])


def lathe(b, profile, c, seg=8, tile="ash", m=None):
    """Surface of revolution about the vertical through c, profile [(r, z)...] bottom to top. UVs at m metres per tile:
    V runs along the profile and starts a new island (new random offset) whenever it would leave the window."""
    c = Vector(c)
    sw = tile in T.SWATCH
    u0, v0, du, dv = b.win(tile) if not sw else (0, 0, 0, 0)
    m = m or M.get(tile, 1.0)
    rmax = max(r for r, _ in profile)
    circ = 2 * math.pi * max(rmax, 1e-4)
    s = min(1.0 / m, 0.97 / circ)
    seglen = [math.hypot(profile[j + 1][0] - profile[j][0], profile[j + 1][1] - profile[j][1]) for j in range(len(profile) - 1)]
    s = min(s, 0.97 / max(max(seglen), 1e-6))
    islands, cur, acc = [], [], 0.0
    for j, L in enumerate(seglen):
        if cur and (acc + L) * s > 0.97:
            islands.append(cur)
            cur, acc = [], 0.0
        cur.append(j)
        acc += L
    islands.append(cur)
    Pp = lambda r, z, a: c + Vector((math.cos(a) * r, math.sin(a) * r, z))
    for isl in islands:
        tot = sum(seglen[j] for j in isl)
        ou = b.rnd.random() * max(0.0, 1 - circ * s)
        ov = b.rnd.random() * max(0.0, 1 - tot * s)
        vacc = 0.0
        for j in isl:
            (ra, za), (rb, zb) = profile[j], profile[j + 1]
            va, vb = ov + vacc * s, ov + (vacc + seglen[j]) * s
            vacc += seglen[j]
            for i in range(seg):
                a0, a1 = 2 * math.pi * i / seg, 2 * math.pi * (i + 1) / seg
                q = [Pp(ra, za, a0), Pp(ra, za, a1), Pp(rb, zb, a1), Pp(rb, zb, a0)]
                ua, ub = ou + circ * s * i / seg, ou + circ * s * (i + 1) / seg
                uv = [(ua, va), (ub, va), (ub, vb), (ua, vb)]
                if ra < 1e-6:
                    q, uv = [q[0], q[2], q[3]], [uv[0], uv[2], uv[3]]
                elif rb < 1e-6:
                    q, uv = q[:3], uv[:3]
                uv = [T.swatch_uv(tile)] * len(q) if sw else [(u0 + x * du, v0 + y * dv) for x, y in uv]
                am = (a0 + a1) / 2
                want = Vector((math.cos(am), math.sin(am), 0)) if (ra > 1e-6 and rb > 1e-6) else Vector((0, 0, -1 if ra < 1e-6 else 1))
                if ra > 1e-6 and rb > 1e-6 and abs(zb - za) < 1e-6:
                    want = Vector((0, 0, 1 if rb < ra else -1))
                if newell(q).dot(want) < 0:
                    q, uv = q[::-1], uv[::-1]
                b.face(q, uv)


def annulus(b, c, r_in, r_out, z0, z1, seg, tile="ash", rot0=0.0):
    """A flat ring (washer) about the vertical through c."""
    c = Vector(c)
    for i in range(seg):
        a0, a1 = rot0 + 2 * math.pi * i / seg, rot0 + 2 * math.pi * (i + 1) / seg
        d0, d1 = Vector((math.cos(a0), math.sin(a0), 0)), Vector((math.cos(a1), math.sin(a1), 0))
        P = [c + d0 * r_in + V(0, 0, z0), c + d0 * r_out + V(0, 0, z0), c + d1 * r_out + V(0, 0, z0), c + d1 * r_in + V(0, 0, z0)]
        P += [p + V(0, 0, z1 - z0) for p in P]
        hexa(b, P, tile)


def coil(b, c, r, loops, axis_x=True, rr=0.012, drop=0.0, tile="rope", seed=0):
    """A coil of rope hanging in the vertical plane through c (loops slightly fanned)."""
    rnd = random.Random(seed)
    for k in range(loops):
        pts = []
        off = rnd.uniform(-0.015, 0.015)
        rk = r * (1 + 0.06 * (k - loops / 2) / loops) + rnd.uniform(-0.01, 0.01)
        for i in range(13):
            a = 2 * math.pi * i / 12 + 0.2 * k
            sag = drop * (1 + math.cos(a)) * 0.5
            x = math.sin(a) * rk * 0.85
            z = -rk * (1 + math.cos(a)) - sag
            d = (k - (loops - 1) / 2) * rr * 2.1 + off
            pts.append(c + (V(x, d, z) if axis_x else V(d, x, z)))
        tube(b, pts, rr, seg=4, tile=tile, caps=False)


def floor_ring(n, r, k, z, rot0):
    a = rot0 + 2 * math.pi * k / n
    return V(r * math.cos(a), r * math.sin(a), z)


def band_boxes(a, z0, z1, nb, name):
    """Axis-aligned boxes whose union approximates a regular octagon with inradius a (flat sides facing +-X, +-Y)."""
    out = []
    edges = [-a + 2 * a * i / nb for i in range(nb + 1)]
    for i in range(nb):
        y0, y1 = edges[i], edges[i + 1]
        ym = abs((y0 + y1) / 2)
        hw = min(a, a * math.sqrt(2) - ym)
        out.append({"name": "%s_%d" % (name, i), "center": [0.0, round((y0 + y1) / 2, 3), round((z0 + z1) / 2, 3)],
                    "size": [round(2 * hw, 3), round(y1 - y0, 3), round(z1 - z0, 3)]})
    return out


def cbox(name, c, s, **rot):
    d = {"name": name, "center": [round(v, 3) for v in c], "size": [round(v, 3) for v in s]}
    for k, v in rot.items():
        d[k] = round(v, 3)
    return d


# =========================================================================================================
# belvedere
# =========================================================================================================

R_PL, R_ST, R_COL = 3.0, 3.38, 2.62            # vertex radii: plinth top, step, column centres
Z_FL, Z_ST, Z_FOOT = 0.25, 0.125, -0.3
OCT0 = math.radians(22.5)


def octp(r, k, z):
    return floor_ring(8, r, k, z, OCT0)


def bench_soviet(b, cx, cy, zf, L=1.6, face=-1):
    """Park bench: two cast-concrete side frames, three seat slats and two back slats (the back on the +Y side when face = -1)."""
    zs = zf + 0.45
    rnd = b.rnd
    for sx in (-1, 1):
        x = cx + sx * (L / 2 - 0.18)
        box(b, V(x, cy, zf + 0.2), (0.09, 0.42, 0.40), "ash", grain=2)                      # leg slab
        P = [V(x - 0.045, cy + 0.12, zs - 0.06), V(x + 0.045, cy + 0.12, zs - 0.06), V(x + 0.045, cy + 0.21, zs - 0.06), V(x - 0.045, cy + 0.21, zs - 0.06)]
        P += [V(x - 0.045, cy + 0.2, zs + 0.42), V(x + 0.045, cy + 0.2, zs + 0.42), V(x + 0.045, cy + 0.27, zs + 0.42), V(x - 0.045, cy + 0.27, zs + 0.42)]
        hexa(b, P, "ash", grain=V(0, 0.15, 1))                                                  # back support
    for i, y in enumerate((-0.135, -0.005, 0.125)):
        box(b, V(cx + rnd.uniform(-0.01, 0.01), cy + y, zs - 0.02), (L, 0.115, 0.04), "wood", grain=0,
            rot=rz(rnd.uniform(-0.006, 0.006)))
    for z in (zs + 0.16, zs + 0.33):
        t = (z - zs + 0.06) / 0.48
        yb = cy + lerp(0.12, 0.2, t) - 0.022
        box(b, V(cx, yb, z), (L - 0.04, 0.035, 0.11), "wood", grain=0, rot=rx(-0.16))
    return [[round(cx - 0.4, 3), round(cy - 0.02, 3), round(zs, 3), 0.0], [round(cx + 0.4, 3), round(cy - 0.02, 3), round(zs, 3), 0.0]]


def belvedere(variant):
    b = PB("belvedere")
    rnd = random.Random(1)
    # ---- plinth and step: a stepped octagon, flat sides facing +-X / +-Y -------------------------------------
    rings = [0.0, 1.05, 1.95, R_PL]
    for k in range(8):
        k1 = k + 1
        poly(b, [V(0, 0, Z_FL), octp(rings[1], k, Z_FL), octp(rings[1], k1, Z_FL)], "stone", out=V(0, 0, 1))
        # the sun: sixteen alternating rays of dark and pale concrete
        a, c = octp(rings[1], k, Z_FL), octp(rings[1], k1, Z_FL)
        A, C = octp(rings[2], k, Z_FL), octp(rings[2], k1, Z_FL)
        mi, Mi = (a + c) / 2, (A + C) / 2
        quad(b, [a, A, Mi, mi], "stone", out=V(0, 0, 1))
        quad(b, [mi, Mi, C, c], "ash", out=V(0, 0, 1))
        quad(b, [A, octp(R_PL, k, Z_FL), octp(R_PL, k1, Z_FL), C], "ash", out=V(0, 0, 1))
        quad(b, [octp(R_PL, k, Z_ST), octp(R_PL, k1, Z_ST), octp(R_PL, k1, Z_FL), octp(R_PL, k, Z_FL)], "stone", inside=V(0, 0, 0))
        quad(b, [octp(R_PL, k, Z_ST), octp(R_ST, k, Z_ST), octp(R_ST, k1, Z_ST), octp(R_PL, k1, Z_ST)], "ash", out=V(0, 0, 1))
        quad(b, [octp(R_ST, k, Z_FOOT), octp(R_ST, k1, Z_FOOT), octp(R_ST, k1, Z_ST), octp(R_ST, k, Z_ST)], "stone", inside=V(0, 0, 0))
    poly(b, [octp(R_ST, k, Z_FOOT) for k in range(8)], "ground_dark", out=V(0, 0, -1))
    # ---- columns ---------------------------------------------------------------------------------------------
    Z_CAP, Z_RB0, Z_RB1 = 2.70, 2.82, 3.10
    for k in range(8):
        p = octp(R_COL, k, 0)
        a = OCT0 + math.pi / 4 * k
        box(b, V(p.x, p.y, Z_FL + 0.075), (0.36, 0.36, 0.15), "ash", rot=rz(a), grain=0)
        box(b, V(p.x, p.y, Z_FL + 0.175), (0.28, 0.28, 0.05), "ash", rot=rz(a), grain=0)
        rr = 0.092 + rnd.uniform(-0.003, 0.003)
        pole(b, V(p.x, p.y, Z_FL + 0.2), V(p.x, p.y, Z_CAP - 0.04), rr, rr * 0.9, seg=10, tile="white_paint", cap0=False, cap1=False)
        box(b, V(p.x, p.y, Z_CAP - 0.02), (0.24, 0.24, 0.04), "white_paint", rot=rz(a), grain=0)
        box(b, V(p.x, p.y, (Z_CAP + Z_RB0) / 2), (0.34, 0.34, Z_RB0 - Z_CAP), "white_paint", rot=rz(a), grain=0)
    # ---- ring beam -------------------------------------------------------------------------------------------
    ri, ro = 2.40, 2.86
    for k in range(8):
        P = [octp(ri, k, Z_RB0), octp(ro, k, Z_RB0), octp(ro, k + 1, Z_RB0), octp(ri, k + 1, Z_RB0)]
        P += [p + V(0, 0, Z_RB1 - Z_RB0) for p in P]
        hexa(b, P, "white_paint", skip=("s0", "s2"))
        # a thin drip moulding round the outside
        Q = [octp(ro, k, Z_RB1 - 0.06), octp(ro + 0.05, k, Z_RB1 - 0.06), octp(ro + 0.05, k + 1, Z_RB1 - 0.06), octp(ro, k + 1, Z_RB1 - 0.06)]
        Q += [p + V(0, 0, 0.06) for p in Q]
        hexa(b, Q, "white_paint", skip=("s0", "s2"))
    # ---- the sun-ray roof: sixteen ribs from a hub ring, over the ring beam, out past it -------------------
    hub_c = V(0, 0, 0)
    annulus(b, hub_c, 0.40, 0.62, 3.47, 3.68, 16, "white_paint", rot0=math.pi / 16)
    ca = math.cos(math.pi / 8)
    for i in range(16):
        phi = math.pi / 8 * i
        d = V(math.cos(phi), math.sin(phi), 0)
        w_ = V(-math.sin(phi), math.cos(phi), 0)
        f = 1.0 if i % 2 else ca                  # odd rays run to the vertices (columns), even ones to the side middles
        r_in, r_out = ri * f, (ro + 0.05) * f
        r_tip = r_out + (0.42 if i % 2 else 0.30)
        prof = [(0.58, 3.49, 3.64, 0.07), (r_in, 3.10, 3.27, 0.085), (r_out, 3.10, 3.25, 0.085), (r_tip, 3.14, 3.19, 0.045)]
        for j in range(3):
            (ra, zba, zta, wa), (rb, zbb, ztb, wb) = prof[j], prof[j + 1]
            P = [d * ra + w_ * (-wa / 2) + V(0, 0, zba), d * rb + w_ * (-wb / 2) + V(0, 0, zbb), d * rb + w_ * (wb / 2) + V(0, 0, zbb), d * ra + w_ * (wa / 2) + V(0, 0, zba)]
            P += [d * ra + w_ * (-wa / 2) + V(0, 0, zta), d * rb + w_ * (-wb / 2) + V(0, 0, ztb), d * rb + w_ * (wb / 2) + V(0, 0, ztb), d * ra + w_ * (wa / 2) + V(0, 0, zta)]
            sk = ()
            if j == 0:
                sk = ("s1",)
            elif j == 1:
                sk = ("s1", "s3")
            else:
                sk = ("s3",)
            hexa(b, P, "white_paint", grain=d, skip=sk)
    # a hoop at r 1.6 between the ribs
    for i in range(16):
        a0, a1 = math.pi / 8 * i, math.pi / 8 * (i + 1)
        rr = 1.6
        p0 = V(math.cos(a0) * rr, math.sin(a0) * rr, 3.33)
        p1 = V(math.cos(a1) * rr, math.sin(a1) * rr, 3.33)
        e = (p1 - p0).normalized()
        beam(b, p0 + e * 0.045, p1 - e * 0.045, 0.07, 0.08, "white_paint")
    # ---- balustrade on the five front sides (3..7), the three back sides open ----------------------------
    bal_sides = (3, 4, 5, 6, 7)
    Z_CURB, Z_RAIL0, Z_RAIL1 = Z_FL + 0.12, Z_FL + 0.85, Z_FL + 0.95
    prof = [(0.0, 0.0), (0.055, 0.0), (0.055, 0.05), (0.032, 0.09), (0.06, 0.26), (0.064, 0.33), (0.036, 0.6), (0.03, 0.64),
            (0.055, 0.68), (0.055, Z_RAIL0 - Z_CURB), (0.0, Z_RAIL0 - Z_CURB)]
    missing = (6, 2)
    for k in bal_sides:
        P0, P1 = octp(R_COL, k, 0), octp(R_COL, k + 1, 0)
        L = (P1 - P0).length
        e = (P1 - P0).normalized()
        ang = math.atan2(e.y, e.x)
        mid = (P0 + P1) / 2
        box(b, V(mid.x, mid.y, (Z_FL + Z_CURB) / 2), (L - 0.30, 0.22, Z_CURB - Z_FL), "ash", rot=rz(ang), grain=0)
        box(b, V(mid.x, mid.y, (Z_RAIL0 + Z_RAIL1) / 2), (L - 0.19, 0.19, Z_RAIL1 - Z_RAIL0), "white_paint", rot=rz(ang), grain=0)
        nb = 7
        for j in range(nb):
            if (k, j) == missing:
                continue
            t = (j + 0.5) / nb
            p = P0.lerp(P1, lerp(0.12, 0.88, t))
            lathe(b, prof, V(p.x, p.y, Z_CURB), seg=6, tile="white_paint")
    # ---- benches -----------------------------------------------------------------------------------------
    seats = []
    for bxp in (-0.95, 0.95):
        seats += bench_soviet(b, bxp, -0.45, Z_FL)
    # ---- collision -------------------------------------------------------------------------------------------
    a_pl = R_PL * math.cos(math.pi / 8)
    a_st = R_ST * math.cos(math.pi / 8)
    boxes = band_boxes(a_pl, Z_FOOT, Z_FL, 9, "plinth") + band_boxes(a_st, Z_FOOT, Z_ST, 9, "step")
    for k in range(8):
        p = octp(R_COL, k, 0)
        boxes.append(cbox("column_%d" % k, (p.x, p.y, (Z_FL + Z_RB0) / 2), (0.3, 0.3, Z_RB0 - Z_FL)))
    ai = R_COL * math.cos(math.pi / 8)
    zc, hc = (Z_FL + Z_RAIL1) / 2, Z_RAIL1 - Z_FL
    boxes.append(cbox("rail_west", (-ai, 0, zc), (0.22, 2.0, hc)))
    boxes.append(cbox("rail_east", (ai, 0, zc), (0.22, 2.0, hc)))
    boxes.append(cbox("rail_front", (0, -ai, zc), (2.0, 0.22, hc)))
    for k, nm in ((4, "rail_front_west"), (6, "rail_front_east")):
        P0, P1 = octp(R_COL, k, 0), octp(R_COL, k + 1, 0)
        for j in range(4):
            p = P0.lerp(P1, (j + 0.5) / 4)
            sd = abs((P1 - P0).x) / 4 + 0.15
            boxes.append(cbox("%s_%d" % (nm, j), (p.x, p.y, zc), (sd, sd, hc)))
    for i, bxp in enumerate((-0.95, 0.95)):
        boxes.append(cbox("bench_%d" % i, (bxp, -0.45, (Z_FL + Z_FL + 0.45) / 2), (1.6, 0.42, 0.45)))
        boxes.append(cbox("bench_back_%d" % i, (bxp, -0.24, Z_FL + 0.66), (1.6, 0.1, 0.42)))
    notes = ("late-Soviet resort rotunda on a lookout: octagonal concrete plinth (vertex radius 3.0 m, flat sides facing +-X/+-Y), walking "
             "surface z %.2f with a step all round at z %.3f (out to vertex radius %.2f; foundation to z %.1f); floor paved as a sun "
             "(a dark centre and sixteen alternating rays); eight slender round concrete columns (centres on vertex radius %.2f) carry an "
             "octagonal ring beam (z %.2f..%.2f) and an open roof of sixteen sun-ray ribs from a hub ring (oculus r 0.40, top z 3.68) that "
             "run out past the ring as short rays: the sun falls through; a balustrade (rail top z %.2f, one baluster missing) on the five "
             "front sides (-Y half and the +-X sides), the three back sides (+Y) open as the entrance; two park benches inside facing the "
             "view (-Y), 4 seats. Collision: box (collision_boxes, axis-aligned, mesh space): the plinth and the step as stacks of bands, "
             "the columns, the balustrade (the two diagonal sides as four boxes each), the benches; no collision on the roof (out of "
             "reach) and nothing in the entrance" % (Z_FL, Z_ST, R_ST, Z_FOOT, R_COL, Z_RB0, Z_RB1, Z_RAIL1))
    return b, {"seats": seats, "notes": notes,
               "manifest": {"floor_z": Z_FL, "step_z": Z_ST, "entrance": "+Y (back sides open)", "view": "-Y",
                            "rail_top_z": Z_RAIL1, "collision_boxes": boxes}}


# =========================================================================================================
# viewer
# =========================================================================================================

def viewer(variant):
    b = PB("viewer")
    # concrete pad (bedded 0.15 m) and the post with a flared foot
    box(b, V(0, 0.2, -0.06), (0.8, 1.2, 0.18), "ash", grain=1)
    pole(b, V(0, 0, -0.3), V(0, 0, 1.30), 0.06, 0.055, seg=10, tile="metal", cap0=False)
    lathe(b, [(0.0, 0.03), (0.17, 0.03), (0.17, 0.06), (0.12, 0.09), (0.07, 0.2), (0.0, 0.2)], V(0, 0, 0), seg=12, tile="metal")
    for k in range(4):                                     # anchor bolts
        a = math.pi / 4 + k * math.pi / 2
        pole(b, V(math.cos(a) * 0.13, math.sin(a) * 0.13, 0.06), V(math.cos(a) * 0.13, math.sin(a) * 0.13, 0.085), 0.014, seg=6, tile="steel")
    # coin box on the post (+Y face: the slot and a return cup)
    box(b, V(0, 0.105, 1.08), (0.17, 0.11, 0.22), "enamel_blue")
    box(b, V(0, 0.163, 1.13), (0.07, 0.006, 0.04), "tin")
    box(b, V(0, 0.168, 1.13), (0.035, 0.006, 0.006), "black_paint")
    box(b, V(0, 0.175, 1.0), (0.08, 0.03, 0.035), "tin")
    # turntable and yoke
    pole(b, V(0, 0, 1.30), V(0, 0, 1.36), 0.085, seg=12, tile="steel")
    box(b, V(0, 0, 1.38), (0.46, 0.12, 0.04), "enamel_blue", grain=0)
    for sx in (-1, 1):
        box(b, V(sx * 0.215, 0, 1.47), (0.03, 0.11, 0.14), "enamel_blue", grain=2)
        pole(b, V(sx * 0.2, 0, 1.53), V(sx * 0.245, 0, 1.53), 0.03, seg=8, tile="steel")
    # the binocular body: a chamfered block 0.36 x 0.36 x 0.22 along Y
    zc = 1.53
    hw, hh, ch = 0.18, 0.11, 0.05
    outline = [(-hw + ch, -hh), (hw - ch, -hh), (hw, -hh + ch), (hw, hh - ch), (hw - ch, hh), (-hw + ch, hh), (-hw, hh - ch), (-hw, -hh + ch)]
    y0, y1 = -0.17, 0.19
    pf = [V(x, y0, zc + z) for x, z in outline]
    pb = [V(x, y1, zc + z) for x, z in outline]
    for i in range(8):
        i2 = (i + 1) % 8
        quad(b, [pf[i], pf[i2], pb[i2], pb[i]], "enamel_blue", inside=V(0, 0, zc))
    poly(b, pf, "enamel_blue", out=V(0, -1, 0))
    poly(b, pb, "enamel_blue", out=V(0, 1, 0))
    # sun hood over the objectives, the objectives, the eyepiece visor and the eyepieces
    box(b, V(0, -0.215, zc + 0.115), (0.38, 0.11, 0.012), "enamel_blue", rot=rx(-0.12), grain=0)
    for sx in (-1, 1):
        x = sx * 0.085
        pole(b, V(x, y0 + 0.01, zc), V(x, y0 - 0.06, zc), 0.072, seg=14, tile="black_paint")
        pole(b, V(x, y0 - 0.058, zc), V(x, y0 - 0.0625, zc), 0.056, seg=14, tile="hem", cap0=False)
        pole(b, V(x * 0.39, y1 + 0.05, zc + 0.02), V(x * 0.39, y1 + 0.105, zc + 0.02), 0.021, seg=10, tile="rubber")
        pole(b, V(x * 0.39, y1 + 0.104, zc + 0.02), V(x * 0.39, y1 + 0.107, zc + 0.02), 0.012, seg=8, tile="dark", cap0=False)
        # aiming handles
        tube(b, [V(sx * 0.18, 0.16, zc - 0.02), V(sx * 0.27, 0.2, zc - 0.03), V(sx * 0.31, 0.24, zc - 0.04)], 0.014, seg=6, tile="steel")
        pole(b, V(sx * 0.27, 0.2, zc - 0.03), V(sx * 0.315, 0.245, zc - 0.04), 0.02, seg=8, tile="rubber")
    box(b, V(0, y1 + 0.025, zc + 0.02), (0.26, 0.05, 0.11), "rubber", grain=0)
    # the children's step behind the post: a steel tread on two flat legs, bolted to the pad
    box(b, V(0, 0.36, 0.215), (0.46, 0.26, 0.03), "metal", grain=0)
    for sx in (-1, 1):
        box(b, V(sx * 0.2, 0.36, 0.115), (0.03, 0.22, 0.17), "metal", grain=1)
    box(b, V(0, 0.36, 0.045), (0.44, 0.24, 0.03), "metal", grain=0)
    notes = ("coin-operated tower viewer (stationary binoculars) on a steel post with a flared foot, on a concrete pad 0.8 x 1.2 m "
             "(bedded 0.15 m); turning head with a blue chamfered body, objectives and sun hood toward the front (-Y) = where it looks, "
             "eyepieces at z 1.55 on the +Y side, coin box on the post's +Y face (z 1.08), a children's step (tread z 0.23) behind "
             "the post at y 0.23..0.49; the viewer stands at y ~ 0.45..0.6 facing -Y")
    return b, {"notes": notes, "manifest": {"eyepiece": [0.0, round(y1 + 0.107, 3), round(zc + 0.02, 3)], "look_dir": [0, -1, 0],
                                             "stand_at": [0.0, 0.62, 0.03]}}


# =========================================================================================================
# bird_hide
# =========================================================================================================

HF = 2.6                      # floor top
HX, HY = 1.4, 1.2             # cabin half-size (outside of the boards)
Y_L = 2.225                   # landing edge (stair top)
RISE, NR = 0.2, 13
GOING = 0.265


def zr(y):
    """Underside of the rafters: the roof falls to the front (-Y)."""
    return HF + 1.975 + (y / HY) * 0.225


def bird_hide(variant):
    b = PB("bird_hide")
    rnd = random.Random(21)
    # ---- stilts (0.15 sq, down to z -1.2), main beams, landing posts ---------------------------------------
    st = [(-1.25, -1.05), (0.0, -1.05), (1.25, -1.05), (-1.25, 1.05), (0.0, 1.05), (1.25, 1.05)]
    for (x, y) in st:
        box(b, V(x + rnd.uniform(-0.01, 0.01), y, (-1.2 + 2.22) / 2), (0.15, 0.15, 3.42), "wood_dark", grain=2,
            rot=rz(rnd.uniform(-0.03, 0.03)))
    for sx in (-1, 1):                                    # landing posts run up to carry the landing rail
        box(b, V(sx * 0.55, 2.15, (-1.2 + HF + 1.0) / 2), (0.14, 0.14, HF + 2.2), "wood_dark", grain=2)
    for y in (-1.05, 1.05):
        box(b, V(0, y, 2.32), (2.95, 0.16, 0.2), "wood_dark", grain=0)
    box(b, V(0, 2.15, 2.32), (1.25, 0.15, 0.2), "wood_dark", grain=0)
    for x in (-1.3, -0.65, 0.0, 0.65, 1.3):
        box(b, V(x, 0, 2.49), (0.08, 2.36, 0.14), "wood", grain=1)
    for x in (-0.42, 0.42):                               # landing joists
        box(b, V(x, (1.05 + 2.225) / 2, 2.49), (0.08, 1.2, 0.14), "wood", grain=1)
    # ---- cross-bracing ---------------------------------------------------------------------------------------
    z0b, z1b = 0.3, 2.1
    for y in (-1.05, 1.05):
        yo = y + (0.09 if y > 0 else -0.09)
        for xa, xb in ((-1.25, 0.0), (0.0, 1.25)):
            yy = yo + (0.025 if y > 0 else -0.025)
            beam(b, V(xa, yo, z0b), V(xb, yo, z1b), 0.12, 0.028, "wood", up=V(0, 1 if y > 0 else -1, 0))
            beam(b, V(xa, yy, z1b), V(xb, yy, z0b), 0.12, 0.028, "wood", up=V(0, 1 if y > 0 else -1, 0))
    for x in (-1.25, 1.25):
        xo = x + (0.09 if x > 0 else -0.09)
        xx = xo + (0.025 if x > 0 else -0.025)
        beam(b, V(xo, -1.05, z0b), V(xo, 1.05, z1b), 0.12, 0.028, "wood", up=V(1 if x > 0 else -1, 0, 0))
        beam(b, V(xx, -1.05, z1b), V(xx, 1.05, z0b), 0.12, 0.028, "wood", up=V(1 if x > 0 else -1, 0, 0))
    yl = 2.15 + 0.085
    beam(b, V(-0.55, yl, z0b), V(0.55, yl, z1b), 0.11, 0.028, "wood", up=V(0, 1, 0))
    beam(b, V(-0.55, yl + 0.028, z1b), V(0.55, yl + 0.028, z0b), 0.11, 0.028, "wood", up=V(0, 1, 0))
    for sx in (-1, 1):                                    # knee braces from the landing posts to the back beam
        beam(b, V(sx * 0.55, 2.08, 1.55), V(sx * 0.55, 1.1, 2.27), 0.1, 0.06, "wood", up=V(1, 0, 0))
    # ---- floor and landing boards ----------------------------------------------------------------------------
    nbd = 16
    for i in range(nbd):
        x0 = -1.38 + 2.76 * i / nbd
        box(b, V(x0 + 2.76 / nbd / 2, 0, HF - 0.02 + rnd.uniform(-0.002, 0.002)), (2.76 / nbd - 0.008, 2.36, 0.04), "wood", grain=1)
    for i in range(5):
        x0 = -0.6 + 1.2 * i / 5
        box(b, V(x0 + 0.12, (1.2 + Y_L) / 2, HF - 0.02), (0.232, Y_L - 1.2, 0.04), "wood", grain=1)
    # ---- walls: vertical boards with slits and the door opening ----------------------------------------------
    SL0, SL1 = HF + 1.0, HF + 1.4                          # slit band
    WB = HF - 0.22                                         # the boards come down over the floor's edge

    def board(x0, x1, y0, y1, zb, zt0, zt1, along_x):
        """A vertical board; its top slopes from zt0 to zt1 along y (side walls) or is flat."""
        if along_x:
            P = [V(x0, y0, zb), V(x1, y0, zb), V(x1, y1, zb), V(x0, y1, zb), V(x0, y0, zt0), V(x1, y0, zt0), V(x1, y1, zt0), V(x0, y1, zt0)]
        else:
            P = [V(x0, y0, zb), V(x1, y0, zb), V(x1, y1, zb), V(x0, y1, zb), V(x0, y0, zt0), V(x1, y0, zt0), V(x1, y1, zt1), V(x0, y1, zt1)]
        hexa(b, P, "wood", grain=V(0, 0, 1))
    bw = 0.175
    # front (y = -HY) and back (y = +HY)
    nfx = int(round(2 * (HX + 0.015) / bw))
    wfx = 2 * (HX + 0.015) / nfx
    for side in (-1, 1):
        yc = side * HY
        zt = zr(yc) + 0.12
        for i in range(nfx):
            x0 = -(HX + 0.015) + wfx * i + 0.002
            x1 = x0 + wfx - 0.004
            xm = (x0 + x1) / 2
            dz = rnd.uniform(-0.02, 0.0)
            dy = rnd.uniform(-0.003, 0.003)
            y0, y1 = yc - 0.015 + dy, yc + 0.015 + dy
            if side < 0 and abs(xm) < 1.2:                 # the long viewing slit
                board(x0, x1, y0, y1, WB + dz, SL0, SL0, True)
                board(x0, x1, y0, y1, SL1, zt, zt, True)
            elif side > 0 and abs(xm) < 0.44:              # the door opening
                board(x0, x1, y0, y1, HF + 1.95, zt, zt, True)
            else:
                board(x0, x1, y0, y1, WB + dz, zt, zt, True)
    nsy = int(round(2 * (HY - 0.015) / bw))
    wsy = 2 * (HY - 0.015) / nsy
    for side in (-1, 1):
        xc = side * HX
        for i in range(nsy):
            y0 = -(HY - 0.015) + wsy * i + 0.002
            y1 = y0 + wsy - 0.004
            ym = (y0 + y1) / 2
            dz = rnd.uniform(-0.02, 0.0)
            dx = rnd.uniform(-0.003, 0.003)
            x0, x1 = xc - 0.015 + dx, xc + 0.015 + dx
            if -0.8 < ym < 0.0:                            # the side slits
                board(x0, x1, y0, y1, WB + dz, SL0, SL0, False)
                board(x0, x1, y0, y1, SL1, zr(y0) + 0.12, zr(y1) + 0.12, False)
            else:
                board(x0, x1, y0, y1, WB + dz, zr(y0) + 0.12, zr(y1) + 0.12, False)
    # corner posts inside, the slit frames and the shelf
    for sx in (-1, 1):
        for sy in (-1, 1):
            box(b, V(sx * (HX - 0.06), sy * (HY - 0.06), (HF + zr(sy * (HY - 0.06))) / 2), (0.09, 0.09, zr(sy * (HY - 0.06)) - HF), "wood_dark", grain=2)
    box(b, V(0, -HY + 0.04, SL0 - 0.02), (2.5, 0.05, 0.04), "wood_dark", grain=0)            # sill (inside)
    box(b, V(0, -HY + 0.04, SL1 + 0.02), (2.5, 0.05, 0.04), "wood_dark", grain=0)            # head
    box(b, V(0, -HY - 0.045, SL0 - 0.02), (2.5, 0.06, 0.035), "wood_dark", grain=0)          # outer drip sill
    box(b, V(0, -HY + 0.2, SL0 - 0.06), (2.5, 0.27, 0.035), "wood", grain=0)                 # the shelf
    for x in (-1.0, 0.0, 1.0):
        P = [V(x - 0.02, -HY + 0.03, SL0 - 0.32), V(x + 0.02, -HY + 0.03, SL0 - 0.32), V(x + 0.02, -HY + 0.07, SL0 - 0.32), V(x - 0.02, -HY + 0.07, SL0 - 0.32),
             V(x - 0.02, -HY + 0.03, SL0 - 0.078), V(x + 0.02, -HY + 0.03, SL0 - 0.078), V(x + 0.02, -HY + 0.32, SL0 - 0.078), V(x - 0.02, -HY + 0.32, SL0 - 0.078)]
        hexa(b, P, "wood_dark", grain=V(0, 0, 1))                                            # brackets
    for sx in (-1, 1):
        box(b, V(sx * (HX - 0.04), -0.4, SL0 - 0.02), (0.05, 0.84, 0.04), "wood_dark", grain=1)
        box(b, V(sx * (HX - 0.04), -0.4, SL1 + 0.02), (0.05, 0.84, 0.04), "wood_dark", grain=1)
    # door frame
    for sx in (-1, 1):
        box(b, V(sx * 0.47, HY - 0.02, HF + 0.975), (0.06, 0.1, 1.95), "wood_dark", grain=2)
    box(b, V(0, HY - 0.02, HF + 1.975), (1.0, 0.1, 0.05), "wood_dark", grain=0)
    # ---- roof: rafters and boards running down the slope ----------------------------------------------------
    yf, yb = -HY - 0.42, HY + 0.32
    for x in (-1.32, -0.44, 0.44, 1.32):
        P = [V(x - 0.04, yf + 0.06, zr(yf + 0.06)), V(x + 0.04, yf + 0.06, zr(yf + 0.06)), V(x + 0.04, yb - 0.04, zr(yb - 0.04)), V(x - 0.04, yb - 0.04, zr(yb - 0.04))]
        P += [p + V(0, 0, 0.12) for p in P]
        hexa(b, P, "wood_dark", grain=V(0, 1, 0.19))
    nr = 13
    xw = HX + 0.26
    for i in range(nr):
        x0 = -xw + 2 * xw * i / nr + 0.004
        x1 = x0 + 2 * xw / nr - 0.008
        dz = rnd.uniform(-0.006, 0.006)
        zb0, zb1 = zr(yf) + 0.12 + dz, zr(yb) + 0.12 + dz
        P = [V(x0, yf, zb0), V(x1, yf, zb0), V(x1, yb, zb1), V(x0, yb, zb1)]
        P += [p + V(0, 0, 0.035) for p in P]
        hexa(b, P, "wood_dark", grain=V(0, 1, 0.19))
    for i in range(nr - 1):                                # cover battens over the joints
        x = -xw + 2 * xw * (i + 1) / nr
        P = [V(x - 0.025, yf, zr(yf) + 0.155), V(x + 0.025, yf, zr(yf) + 0.155), V(x + 0.025, yb, zr(yb) + 0.155), V(x - 0.025, yb, zr(yb) + 0.155)]
        P += [p + V(0, 0, 0.022) for p in P]
        hexa(b, P, "wood", grain=V(0, 1, 0.19), skip=("bottom",))
    # fascia boards
    for y in (yf, yb):
        box(b, V(0, y + (0.012 if y < 0 else -0.012), zr(y) + 0.1), (2 * xw, 0.024, 0.16), "wood_dark", grain=0)
    # ---- bench inside, facing the slit ------------------------------------------------------------------------
    zs = HF + 0.45
    by = -0.45
    for x in (-0.9, 0.0, 0.9):
        box(b, V(x, by, HF + 0.21), (0.06, 0.28, 0.42), "wood_dark", grain=2)
    for y in (-0.08, 0.08):
        box(b, V(0, by + y, zs - 0.02), (2.0, 0.15, 0.04), "wood", grain=0)
    box(b, V(0, by, HF + 0.1), (1.9, 0.05, 0.07), "wood_dark", grain=0)
    # ---- the stair ----------------------------------------------------------------------------------------------
    run = NR * GOING
    slope = RISE / GOING
    yfoot = Y_L + run

    def nose(y):
        return HF - (y - Y_L) * slope
    cosA = GOING / math.hypot(GOING, RISE)
    top_off, depth = 0.07, 0.27 / cosA
    zcut = 0.04
    for sx in (-1, 1):
        x0, x1 = sx * 0.42 - 0.025, sx * 0.42 + 0.025
        yt = lambda z: Y_L + (HF + top_off - z) / slope
        ybt = lambda z: Y_L + (HF + top_off - depth - z) / slope
        pts = [(Y_L, HF + top_off), (yt(zcut), zcut), (ybt(zcut), zcut), (Y_L, HF + top_off - depth)]
        P = [V(x0, y, z) for y, z in pts] + [V(x1, y, z) for y, z in pts]
        hexa(b, [P[0], P[1], P[2], P[3], P[4], P[5], P[6], P[7]], "wood", grain=V(0, GOING, -RISE))
    for k in range(1, NR):
        zt = HF - RISE * k
        yn = Y_L + GOING * k
        box(b, V(rnd.uniform(-0.005, 0.005), yn - GOING / 2 + 0.01, zt - 0.02), (0.84, GOING + 0.03, 0.04), "wood", grain=0)
    box(b, V(0, yfoot - 0.2, -0.1), (1.05, 0.62, 0.28), "ash", grain=0)                       # concrete sill under the feet
    # handrails: foot posts (dug in), a middle post on each stringer, sloped rails into the landing posts
    xr = 0.48
    for sx in (-1, 1):
        yp = yfoot - 0.32
        box(b, V(sx * xr, yp, (-0.45 + nose(yp) + 0.95) / 2), (0.08, 0.08, nose(yp) + 0.95 + 0.45), "wood_dark", grain=2)
        ym = (Y_L + yfoot) / 2
        zbot = nose(ym) - 0.22
        box(b, V(sx * xr, ym, (zbot + nose(ym) + 0.95) / 2), (0.07, 0.07, nose(ym) + 0.95 - zbot), "wood_dark", grain=2)
        y_a, y_b = 2.15, yfoot - 0.28
        beam(b, V(sx * xr, y_a, nose(y_a) + 0.95), V(sx * xr, y_b, nose(y_b) + 0.95), 0.06, 0.09, "wood", up=V(0, slope, 1))
        beam(b, V(sx * xr, y_a, nose(y_a) + 0.5), V(sx * xr, ym, nose(ym) + 0.5), 0.04, 0.08, "wood", up=V(0, slope, 1))
        beam(b, V(sx * xr, ym, nose(ym) + 0.5), V(sx * xr, y_b, nose(y_b) + 0.5), 0.04, 0.08, "wood", up=V(0, slope, 1))
        # landing rails from the wall to the landing post
        for zz, hh in ((HF + 0.95, 0.09), (HF + 0.5, 0.08)):
            box(b, V(sx * 0.55, (HY + 0.015 + 2.08) / 2, zz), (0.06, 2.08 - HY - 0.015, hh), "wood", grain=1)
    # ---- manifest -----------------------------------------------------------------------------------------------
    ang = math.degrees(math.atan2(RISE, GOING))
    length = math.hypot(run, HF) + 0.1
    thick = 0.3
    nrm = V(0, math.sin(math.radians(ang)), math.cos(math.radians(ang)))
    midp = V(0, (Y_L + yfoot) / 2, HF / 2) - nrm * (thick / 2 + 0.08)
    rail_len = math.hypot(2.15 - (yfoot - 0.28), slope * (yfoot - 0.28 - 2.15))
    rail_mid = V(0, (2.15 + yfoot - 0.28) / 2, nose((2.15 + yfoot - 0.28) / 2) + 0.7)
    boxes = [cbox("floor", (0, 0, HF - 0.1), (2.84, 2.44, 0.2)),
             cbox("landing", (0, (HY + Y_L) / 2, HF - 0.1), (1.25, Y_L - HY, 0.2)),
             cbox("stair", (0, midp.y, midp.z), (0.9, length, thick), rot_x_deg=-ang),
             cbox("wall_front", (0, -HY, HF + 0.875), (2.86, 0.06, 1.75)),
             cbox("wall_west", (-HX, 0, HF + 1.1), (0.06, 2.4, 2.2)),
             cbox("wall_east", (HX, 0, HF + 1.1), (0.06, 2.4, 2.2)),
             cbox("wall_back_west", (-0.9275, HY, HF + 1.1), (0.975, 0.06, 2.2)),
             cbox("wall_back_east", (0.9275, HY, HF + 1.1), (0.975, 0.06, 2.2)),
             cbox("door_head", (0, HY, HF + 2.075), (0.88, 0.06, 0.25)),
             cbox("roof", (0, (yf + yb) / 2, zr((yf + yb) / 2) + 0.1), (2 * xw, yb - yf, 0.2), rot_x_deg=math.degrees(math.atan2(0.45, 2 * HY))),
             cbox("shelf", (0, -HY + 0.2, SL0 - 0.06), (2.5, 0.27, 0.04)),
             cbox("bench", (0, by, HF + 0.225), (2.0, 0.3, 0.45)),
             cbox("rail_landing_west", (-0.55, (HY + 2.15) / 2, HF + 0.5), (0.08, 2.15 - HY, 1.0)),
             cbox("rail_landing_east", (0.55, (HY + 2.15) / 2, HF + 0.5), (0.08, 2.15 - HY, 1.0)),
             cbox("rail_stair_west", (-xr, rail_mid.y, rail_mid.z), (0.08, rail_len, 0.6), rot_x_deg=-ang),
             cbox("rail_stair_east", (xr, rail_mid.y, rail_mid.z), (0.08, rail_len, 0.6), rot_x_deg=-ang),
             cbox("braces_front", (0, -1.12, 1.25), (2.65, 0.1, 1.9)),
             cbox("braces_back", (0, 1.12, 1.25), (2.65, 0.1, 1.9)),
             cbox("braces_west", (-1.32, 0, 1.25), (0.1, 2.25, 1.9)),
             cbox("braces_east", (1.32, 0, 1.25), (0.1, 2.25, 1.9)),
             cbox("braces_landing", (0, 2.25, 1.25), (1.24, 0.1, 1.9))]
    for (x, y) in st + [(-0.55, 2.15), (0.55, 2.15)]:
        boxes.append(cbox("stilt", (x, y, 1.15), (0.16, 0.16, 2.3)))
    seats = [[-0.5, round(by, 3), round(zs, 3), 0.0], [0.5, round(by, 3), round(zs, 3), 0.0]]
    notes = ("ornithologists' hide on stilts at a reedbed's edge: timber cabin %.1f x %.1f m (origin on the ground under the cabin's "
             "middle), floor top z %.2f, vertical-board walls, a long viewing slit (x -1.2..1.2, z %.2f..%.2f) with a shelf inside on the "
             "front (-Y), shorter slits on both sides (y -0.8..0), a door opening 0.88 x 1.95 m (no leaf) in the back wall (+Y) onto a "
             "landing (y 1.2..%.3f) with rails; a straight open-riser stair (%d risers of %.2f, going %.3f, %.1f deg) runs down from the "
             "landing toward +Y to a concrete sill (foot at y %.2f); a board roof falling to the front (eaves z %.2f front, %.2f back); "
             "six cross-braced 0.15 m stilts plus two landing posts, all to z -1.2; a bench inside facing the slit with 2 seats. "
             "Collision: box (collision_boxes, mesh space; rot_x_deg right-handed about +X, negative tips the +Y end down): floor, "
             "landing, a sloped ramp box under the stair (top plane 0.08 under the nosing line), walls (the front closed up to the "
             "slit's top, the door opening free), roof, shelf, bench, rails, the braced stilt bays as thin slabs"
             % (2 * HX, 2 * HY, HF, SL0, SL1, Y_L, NR, RISE, GOING, ang, yfoot, zr(yf), zr(yb)))
    return b, {"seats": seats, "notes": notes,
               "manifest": {"floor_z": HF, "stair_top": [0.0, Y_L, HF], "stair_foot": [0.0, round(yfoot, 3), 0.0],
                            "stair_deg": round(ang, 2), "look_from": [0.0, round(by - 0.05, 3), round(HF + 1.2, 3)],
                            "slit": {"x": [-1.2, 1.2], "z": [SL0, SL1]}, "collision_boxes": boxes}}


# =========================================================================================================
# fisher_shed
# =========================================================================================================

SX, SY = 1.6, 2.1               # half size of the walls
EAVE, RIDGE = 2.15, 3.2


def roof_z(x):
    return RIDGE - (RIDGE - EAVE) * abs(x) / SX


def fisher_shed(variant):
    b = PB("fisher_shed")
    rnd = random.Random(31)
    # ---- footing stones, sills and corner posts -------------------------------------------------------------
    for x in (-SX + 0.1, 0.0, SX - 0.1):
        for y in (-SY + 0.1, SY - 0.1):
            box(b, V(x, y, -0.05), (0.32, 0.3, 0.2), "stone", grain=0, rot=rz(rnd.uniform(-0.2, 0.2)))
    for sx in (-1, 1):
        box(b, V(sx * (SX - 0.08), 0, 0.12), (0.14, 2 * SY, 0.14), "wood_dark", grain=1)
    for sy in (-1, 1):
        box(b, V(0, sy * (SY - 0.08), 0.12), (2 * SX - 0.32, 0.14, 0.14), "wood_dark", grain=0)
    for sx in (-1, 1):
        for sy in (-1, 1):
            box(b, V(sx * (SX - 0.06), sy * (SY - 0.06), (0.19 + EAVE) / 2), (0.1, 0.1, EAVE - 0.19), "wood_dark", grain=2)
    # ---- clapboard walls: tarred planks, each lapping the one below -----------------------------------------
    ph, lap, th = 0.2, 0.03, 0.022
    nrow = int(math.ceil((EAVE - 0.08) / (ph - lap)))
    DX0, DX1 = -1.08, 1.08                               # door opening on the front

    def plank(p0, p1, z, hgt, nout, zt0=None, zt1=None):
        """A clapboard from p0 to p1 (bottom edge line at height z) leaning out at the bottom; nout = outward normal."""
        p0, p1 = Vector(p0), Vector(p1)
        e = (p1 - p0).normalized()
        n = Vector(nout)
        tilt = n * 0.012
        zt0 = z + hgt if zt0 is None else zt0
        zt1 = z + hgt if zt1 is None else zt1
        P = [p0 + V(0, 0, z) + tilt, p1 + V(0, 0, z) + tilt, p1 + V(0, 0, z) + tilt + n * th, p0 + V(0, 0, z) + tilt + n * th]
        P += [p0 + V(0, 0, zt0), p1 + V(0, 0, zt1), p1 + V(0, 0, zt1) + n * th, p0 + V(0, 0, zt0) + n * th]
        hexa(b, P, "wood_dark", grain=e, skip=("s0", "top"))          # the inside and the lapped top never show
    for r in range(nrow):
        z = 0.08 + r * (ph - lap)
        zt = min(z + ph, EAVE)
        if zt - z < 0.04:
            continue
        hgt = zt - z
        jit = rnd.uniform(-0.004, 0.004)
        # long walls (x = +-SX), outward +-X
        for sx in (-1, 1):
            x = sx * SX
            plank(V(x, -SY - 0.02, 0), V(x, SY + 0.02, 0), z + jit, hgt, V(sx, 0, 0))
        # back wall (y = +SY) and front wall (y = -SY) round the doors
        plank(V(SX + 0.034, SY, 0), V(-SX - 0.034, SY, 0), z + jit, hgt, V(0, 1, 0))
        if z >= 2.0 - 0.001 or zt <= 0.1:
            plank(V(-SX - 0.034, -SY, 0), V(SX + 0.034, -SY, 0), z, hgt, V(0, -1, 0))
        else:
            plank(V(-SX - 0.034, -SY, 0), V(DX0 - 0.06, -SY, 0), z, hgt, V(0, -1, 0))
            plank(V(DX1 + 0.06, -SY, 0), V(SX + 0.034, -SY, 0), z, hgt, V(0, -1, 0))
    # a full plank over the doors up to the eaves
    plank(V(-SX - 0.034, -SY, 0), V(SX + 0.034, -SY, 0), 1.97, EAVE - 1.97, V(0, -1, 0))
    # gables: planks cut to the roof line
    r = 0
    z = EAVE - 0.02
    while z < RIDGE - 0.12:
        hgt = ph
        zt = z + hgt
        xa = SX * (RIDGE - z) / (RIDGE - EAVE)              # half width at the plank's bottom
        xb = SX * (RIDGE - min(zt, RIDGE)) / (RIDGE - EAVE)
        for sy, n in ((-1, V(0, -1, 0)), (1, V(0, 1, 0))):
            y = sy * SY
            p0, p1 = V(-xa, y, 0), V(xa, y, 0)
            if sy > 0:
                p0, p1 = p1, p0
            # top corners pulled in to the roof line
            P0 = p0 + V(0, 0, z) + n * 0.012
            P1 = p1 + V(0, 0, z) + n * 0.012
            e = (p1 - p0).normalized()
            T0 = V(-xb * e.x / abs(e.x), y, zt)
            T1 = V(xb * e.x / abs(e.x), y, zt)
            P = [P0, P1, P1 + n * th, P0 + n * th, T0, T1, T1 + n * th, T0 + n * th]
            hexa(b, P, "wood_dark", grain=e, skip=("s0", "top"))
        z += ph - lap
        r += 1
    # a dark liner inside (seen only through the cracks round the doors) and the door casing
    box(b, V(0, 0, (0.06 + EAVE) / 2), (2 * SX - 0.03, 2 * SY - 0.03, EAVE - 0.06), "dark")
    for sx in (-1, 1):
        box(b, V(sx * (DX1 + 0.05), -SY - 0.045, 1.0), (0.1, 0.03, 2.0), "wood", grain=2)
    box(b, V(0, -SY - 0.045, 2.03), (2 * DX1 + 0.2, 0.03, 0.1), "wood", grain=0)
    # ---- doors (closed): vertical boards, ledges and a Z brace outside, strap hinges, hasp and padlock -------
    for side in (-1, 1):
        xa, xb = (DX0, -0.006) if side < 0 else (0.006, DX1)
        nbd = 5
        wd = (xb - xa) / nbd
        for i in range(nbd):
            x0 = xa + wd * i + 0.002
            box(b, V(x0 + wd / 2 - 0.002, -SY + 0.005, 1.0), (wd - 0.004, 0.03, 1.96), "wood_dark", grain=2)
        for zz in (0.3, 1.7):
            box(b, V((xa + xb) / 2, -SY - 0.03, zz), (xb - xa - 0.06, 0.03, 0.13), "wood", grain=0)
        za, zb = (0.38, 1.62) if side < 0 else (1.62, 0.38)
        beam(b, V(xa + 0.08, -SY - 0.025, za), V(xb - 0.08, -SY - 0.025, zb), 0.12, 0.024, "wood", up=V(0, -1, 0))
        for zz in (0.3, 1.7):                             # strap hinges from the outer edge
            xe = xa if side < 0 else xb
            box(b, V(xe - side * 0.22, -SY - 0.052, zz), (0.46, 0.008, 0.04), "black_paint", grain=0)
            pole(b, V(xe + side * 0.005, -SY - 0.03, zz - 0.06), V(xe + side * 0.005, -SY - 0.03, zz + 0.06), 0.014, seg=6, tile="black_paint")
    box(b, V(0.06, -SY - 0.05, 1.1), (0.2, 0.01, 0.04), "steel", grain=0)
    box(b, V(-0.03, -SY - 0.065, 1.1), (0.03, 0.02, 0.05), "steel", grain=2)
    box(b, V(-0.03, -SY - 0.075, 1.0), (0.06, 0.03, 0.06), "steel", grain=0)       # padlock
    tube(b, [V(-0.05, -SY - 0.075, 1.03), V(-0.05, -SY - 0.075, 1.07), V(-0.01, -SY - 0.075, 1.07), V(-0.01, -SY - 0.075, 1.03)], 0.005, seg=4, tile="steel")
    # ---- roof: board-and-batten on rafters, bargeboards, ridge board ----------------------------------------
    ov, yo = 0.28, 0.3
    rafter_ys = [-SY - 0.05, -SY / 2, 0.0, SY / 2, SY + 0.05]
    pitch = math.atan2(RIDGE - EAVE, SX)
    ux = math.cos(pitch)
    for y in rafter_ys:
        for sx in (-1, 1):
            p0 = V(sx * (SX + ov - 0.05), y, roof_z(SX + ov - 0.05) - 0.06)
            p1 = V(sx * 0.03, y, RIDGE - 0.06)
            beam(b, p0, p1, 0.07, 0.12, "wood_dark", up=V(-sx * math.sin(pitch), 0, ux))
    nb_ = 8
    for sx in (-1, 1):
        for i in range(nb_):
            y0 = -SY - yo + (2 * SY + 2 * yo) * i / nb_ + 0.004
            y1 = y0 + (2 * SY + 2 * yo) / nb_ - 0.008
            lo, hi = SX + ov, 0.0
            dz = rnd.uniform(-0.004, 0.004)
            base = [V(sx * lo, y0, roof_z(lo) - 0.06 * 0 + dz), V(sx * hi, y0, RIDGE + dz), V(sx * hi, y1, RIDGE + dz), V(sx * lo, y1, roof_z(lo) + dz)]
            top = [p + V(0, 0, 0.035) for p in base]
            hexa(b, base + top, "wood_dark", grain=V(-sx * ux, 0, math.sin(pitch)))
        for i in range(nb_ - 1):
            y = -SY - yo + (2 * SY + 2 * yo) * (i + 1) / nb_
            base = [V(sx * (SX + ov), y - 0.03, roof_z(SX + ov) + 0.035), V(sx * 0.0, y - 0.03, RIDGE + 0.035), V(sx * 0.0, y + 0.03, RIDGE + 0.035), V(sx * (SX + ov), y + 0.03, roof_z(SX + ov) + 0.035)]
            top = [p + V(0, 0, 0.025) for p in base]
            hexa(b, base + top, "wood", grain=V(-sx * ux, 0, math.sin(pitch)), skip=("bottom",))
    for sx in (-1, 1):                                    # ridge boards
        P = [V(0, -SY - yo - 0.02, RIDGE + 0.06), V(sx * 0.16, -SY - yo - 0.02, RIDGE + 0.06 - 0.16 * math.tan(pitch)), V(sx * 0.16, SY + yo + 0.02, RIDGE + 0.06 - 0.16 * math.tan(pitch)), V(0, SY + yo + 0.02, RIDGE + 0.06)]
        P += [p + V(0, 0, 0.03) for p in P]
        hexa(b, P, "wood_dark", grain=V(0, 1, 0))
    for y in (-SY - yo + 0.015, SY + yo - 0.015):         # bargeboards under the roof edge
        for sx in (-1, 1):
            beam(b, V(sx * (SX + ov), y, roof_z(SX + ov) - 0.07), V(0, y, RIDGE - 0.07), 0.03, 0.16, "wood_dark",
                 up=V(-sx * math.sin(pitch), 0, ux))
    # ---- lean-to rack on +X: two posts, a board roof against the wall (under the eaves), arms with oars -------
    xp = SX + 0.95
    x_w, x_o = SX + 0.07, xp + 0.25
    zl = lambda x: 1.70 + (x - x_w) * (1.48 - 1.70) / (x_o - x_w)     # top of the lean-to's beams
    for y in (-1.3, 1.3):
        box(b, V(xp, y, (-0.45 + zl(xp) - 0.1) / 2), (0.1, 0.1, zl(xp) - 0.1 + 0.45), "wood", grain=2)
    box(b, V(xp, 0, zl(xp) - 0.13), (0.1, 3.0, 0.1), "wood", grain=1)            # plate on the posts
    box(b, V(SX + 0.054, 0, 1.6), (0.04, 3.0, 0.1), "wood", grain=1)            # ledger on the wall
    for y in (-1.3, 0.0, 1.3):
        beam(b, V(x_w - 0.01, y, zl(x_w) - 0.04), V(x_o, y, zl(x_o) - 0.04), 0.06, 0.08, "wood")
    slope_l = (1.70 - 1.48) / (x_o - x_w)
    for i in range(6):
        y0 = -1.55 + 3.1 * i / 6 + 0.004
        y1 = y0 + 3.1 / 6 - 0.008
        base = [V(x_w, y0, zl(x_w)), V(x_o + 0.05, y0, zl(x_o + 0.05)), V(x_o + 0.05, y1, zl(x_o + 0.05)), V(x_w, y1, zl(x_w))]
        top = [p + V(0, 0, 0.03) for p in base]
        hexa(b, base + top, "wood", grain=V(1, 0, -slope_l))
    # arms from the wall to the posts, three oars lying on them along Y (blades at +Y)
    for zz in (0.9, 1.22):
        for y in (-1.22, 1.22):
            box(b, V((SX + 0.03 + xp - 0.05) / 2, y, zz), (xp - 0.05 - SX - 0.03, 0.05, 0.07), "wood_dark", grain=0)
    for x, zz, y0 in ((SX + 0.3, 0.9, -1.5), (SX + 0.62, 0.9, -1.42), (SX + 0.46, 1.22, -1.46)):
        zc = zz + 0.035 + 0.022
        pole(b, V(x, y0, zc), V(x, y0 + 2.2, zc), 0.022, 0.02, seg=6, tile="plank_edge")
        pole(b, V(x, y0 - 0.12, zc), V(x, y0 + 0.02, zc), 0.019, seg=6, tile="wood_dark")             # the grip
        box(b, V(x, y0 + 2.2 + 0.24, zc - 0.006), (0.15, 0.56, 0.018), "plank_edge", grain=1)          # the blade
    # a coil of rope on a peg on the front post of the rack
    pole(b, V(xp, -1.3 - 0.05, 1.5), V(xp, -1.3 - 0.17, 1.52), 0.014, seg=6, tile="plank_edge")
    coil(b, V(xp, -1.3 - 0.12, 1.53), 0.2, 4, axis_x=True, rr=0.014, drop=0.06, seed=3)
    # ---- bench by the west wall, facing west ------------------------------------------------------------------
    xb_ = -SX - 0.3
    for y in (-0.55, 0.55):
        box(b, V(xb_, y, (-0.3 + 0.41) / 2), (0.1, 0.24, 0.71), "wood_dark", grain=2)
    for x in (-0.07, 0.07):
        box(b, V(xb_ + x, rnd.uniform(-0.01, 0.01), 0.43), (0.135, 1.5, 0.045), "wood", grain=1)
    # a lifebuoy on the west wall over the bench (red and white bands)
    cz, cy = 1.45, 0.0
    pts, k = [], 16
    u0, v0, du, dv = b.win("stripes")
    R0, rr = 0.25, 0.055
    xc = -SX - 0.034 - rr
    ring = []
    for i in range(k):
        a = 2 * math.pi * i / k
        c = V(xc, cy + math.cos(a) * R0, cz + math.sin(a) * R0)
        rd = V(0, math.cos(a), math.sin(a))
        ring.append([c + (rd * math.cos(2 * math.pi * j / 6) + V(1, 0, 0) * math.sin(2 * math.pi * j / 6)) * rr for j in range(6)])
    for i in range(k):
        i2 = (i + 1) % k
        ua, ub = 0.02 + 0.96 * i / k, 0.02 + 0.96 * (i + 1) / k
        a = 2 * math.pi * (i + 0.5) / k
        cm = V(xc, cy + math.cos(a) * R0, cz + math.sin(a) * R0)
        for j in range(6):
            j2 = (j + 1) % 6
            q = [ring[i][j], ring[i2][j], ring[i2][j2], ring[i][j2]]
            uv = [(u0 + ua * du, v0 + (0.1 + 0.13 * j) * dv), (u0 + ub * du, v0 + (0.1 + 0.13 * j) * dv),
                  (u0 + ub * du, v0 + (0.1 + 0.13 * (j + 1)) * dv), (u0 + ua * du, v0 + (0.1 + 0.13 * (j + 1)) * dv)]
            if newell(q).dot(sum(q, Vector()) / 4 - cm) < 0:
                q, uv = q[::-1], uv[::-1]
            b.face(q, uv)
    pole(b, V(-SX - 0.01, cy, cz + R0 + 0.07), V(-SX - 0.12, cy, cz + R0 + 0.09), 0.012, seg=6, tile="plank_edge")
    seats = [[round(xb_, 3), -0.4, 0.45, 270.0], [round(xb_, 3), 0.4, 0.45, 270.0]]
    notes = ("small tarred-plank boat shed, walls %.1f x %.1f m (origin on the ground under the middle of the walls), clapboard of dark "
             "tarred planks on a sill over footing stones, gable roof of boards and cover battens with the ridge along Y (eaves z %.2f, "
             "ridge z %.2f, overhangs 0.28 / 0.3), closed double doors (2.16 x 1.96 m, Z-braced, strap hinges, hasp and padlock) on the "
             "front (-Y); a lean-to rack on the east side (+X, posts at x %.2f) with three oars and a coil of rope on a peg; a plank "
             "bench by the west wall (seat z 0.45, facing -X: seats yaw 270) under a red-and-white lifebuoy"
             % (2 * SX, 2 * SY, EAVE, RIDGE, xp))
    return b, {"seats": seats, "notes": notes, "manifest": {"door": {"x": [DX0, DX1], "z": [0.0, 1.96], "y": -SY}}}


# =========================================================================================================
# net_rack
# =========================================================================================================

def net_sheet(b, fn, nu, nv, thick=0.006, tile="rope", m=0.34):
    """A thin closed shell from fn(s, t) (s, t in 0..1): two skins and a rim, each cell mapped on its own."""
    P = [[Vector(fn(i / (nu - 1), j / (nv - 1))) for j in range(nv)] for i in range(nu)]

    def nrm(i, j):
        du_ = P[min(nu - 1, i + 1)][j] - P[max(0, i - 1)][j]
        dv_ = P[i][min(nv - 1, j + 1)] - P[i][max(0, j - 1)]
        n = du_.cross(dv_)
        return n.normalized() if n.length > 1e-12 else Vector((0, 0, 1))
    N = [[nrm(i, j) for j in range(nv)] for i in range(nu)]
    O = [[P[i][j] + N[i][j] * thick / 2 for j in range(nv)] for i in range(nu)]
    I = [[P[i][j] - N[i][j] * thick / 2 for j in range(nv)] for i in range(nu)]
    for i in range(nu - 1):
        for j in range(nv - 1):
            nc = (N[i][j] + N[i + 1][j + 1]).normalized()
            q = [O[i][j], O[i + 1][j], O[i + 1][j + 1], O[i][j + 1]]
            poly(b, q, tile, out=nc, m=m)
            q = [I[i][j], I[i + 1][j], I[i + 1][j + 1], I[i][j + 1]]
            poly(b, q, tile, out=-nc, m=m)
    for i in range(nu - 1):
        for j, sgn in ((0, -1), (nv - 1, 1)):
            outv = (P[i][j] - P[i][j - sgn]) if True else None
            poly(b, [O[i][j], O[i + 1][j], I[i + 1][j], I[i][j]], tile, out=outv, m=m)
    for j in range(nv - 1):
        for i, sgn in ((0, -1), (nu - 1, 1)):
            outv = P[i][j] - P[i - sgn][j]
            poly(b, [O[i][j], O[i][j + 1], I[i][j + 1], I[i][j]], tile, out=outv, m=m)
    return P


def net_rack(variant):
    L = 4.0 if variant == 0 else 6.0
    b = PB("net_rack_v%d" % variant)
    rnd = random.Random(40 + variant)
    ZR = 1.78
    xs = [-L / 2, L / 2] if variant == 0 else [-L / 2, 0.0, L / 2]
    for x in xs:                                         # X-trestles of peeled poles dug in 0.4 m
        x += rnd.uniform(-0.03, 0.03)
        for sy in (-1, 1):
            foot = V(x, sy * 0.48, 0.0)
            cross = V(x, 0.0, ZR - 0.07)
            d = (cross - foot).normalized()
            top = cross + d * 0.22
            pole(b, foot - d * 0.42, top, 0.055, 0.045, seg=7, tile="wood", wobble=0.06, rot0=sy * 0.3)
            # the lashing at the crossing
        pole(b, V(x - 0.07, 0, ZR - 0.07), V(x + 0.07, 0, ZR - 0.07), 0.07, seg=7, tile="rope", cap0=False, cap1=False)
    # the rail resting in the crotches
    pole(b, V(-L / 2 - 0.35, 0.0, ZR), V(L / 2 + 0.35, 0.0, ZR + 0.03), 0.05, 0.04, seg=7, tile="wood", wobble=0.04)
    # nets hung gathered over the rail: bunches of netting with gaps between, a few loose meshes across the gaps
    nets = [(-L / 2 + 0.25, L / 2 - 0.25)] if variant == 0 else [(-L / 2 + 0.2, -0.15), (0.15, L / 2 - 0.3)]
    R = 0.062                                              # wrap radius over the rail (rail r <= 0.052)

    def prof(lf, lb, yf, yb):
        """Cross-section: front hem -> up to the rail -> over it -> down to the back hem (11 points)."""
        out = []
        for k in (0.0, 0.4, 0.75):
            out.append((lerp(yf, -R, k) - math.sin(k * math.pi) * 0.03, lerp(ZR - lf, ZR, k)))
        for a in (0.0, 45.0, 90.0, 135.0, 180.0):
            a = math.radians(a)
            out.append((-math.cos(a) * R, ZR + math.sin(a) * R))
        for k in (0.75, 0.4, 0.0):
            out.append((lerp(yb, R, k) + math.sin(k * math.pi) * 0.03, lerp(ZR - lb, ZR, k)))
        return out
    for ni, (x0, x1) in enumerate(nets):
        ntile = ("nylon_olive", "nylon_bluegrey")[ni]
        x = x0
        edges = []
        while x < x1 - 0.1:
            w = min(rnd.uniform(0.12, 0.28), x1 - x)
            lf = 1.3 + rnd.uniform(-0.18, 0.12) - 0.08 * ni
            lb = 1.05 + rnd.uniform(-0.15, 0.12)
            yf = -(R + rnd.uniform(0.06, 0.2))
            yb = R + rnd.uniform(0.08, 0.22)
            bulge = rnd.uniform(0.02, 0.05) * (1 if rnd.random() < 0.5 else -1)
            pa, pb = prof(lf, lb, yf, yb), prof(lf + rnd.uniform(-0.1, 0.1), lb + rnd.uniform(-0.1, 0.1), yf + rnd.uniform(-0.04, 0.04), yb)
            xa, xb = x, x + w

            def fn(s_, t, xa=xa, xb=xb, pa=pa, pb=pb, bulge=bulge):
                j = int(round(t * 10))
                (ya, za), (yb_, zb) = pa[j], pb[j]
                hang = 1.0 - min(1.0, abs(j - 5) / 2.0)        # no bulge where it lies on the rail
                y = lerp(ya, yb_, s_) + math.sin(s_ * math.pi) * bulge * (1 - hang) * (1 if ya < 0 else -1)
                gather = 0.55 + 0.45 * (1 - hang) * (0.6 + 0.4 * (abs(j - 5) / 5))   # gathered narrow on the rail, splayed below
                xm = (xa + xb) / 2
                return (xm + (lerp(xa, xb, s_) - xm) * gather + math.sin(t * 9 + xa * 5) * 0.015 * (1 - hang), y, lerp(za, zb, s_))
            P = net_sheet(b, fn, 3, 11, thick=0.008, tile=ntile, m=3.0)
            c0 = (P[0][0] + P[2][0]) / 2
            pole(b, c0 + V(-0.035, -0.012, 0.0), c0 + V(0.035, -0.012, 0.0), 0.032, seg=6, tile="plank_edge", m=0.5)   # cork float
            c1 = (P[0][-1] + P[2][-1]) / 2
            pole(b, c1 + V(0, 0.01, 0.02), c1 + V(0, 0.01, -0.03), 0.014, seg=5, tile="metal")                          # lead weight
            edges.append((P[0], P[2]))
            x += w * 0.8 + rnd.uniform(0.0, 0.08)
        # loose meshes across the gaps: a few sagging, crossing strands each
        for (_, right), (left, _) in zip(edges[:-1], edges[1:]):
            for ja, jb in ((1, 2), (2, 1), (8, 9), (9, 8)):
                pa_, pb_ = right[ja], left[jb]
                mid = (pa_ + pb_) / 2 + V(0, 0, -0.03)
                tube(b, [pa_, mid, pb_], 0.004, seg=3, tile=ntile, caps=False)
    # two orange marker floats hanging from the rail ends
    for x, h in ((-L / 2 - 0.22, 0.55), (L / 2 + 0.24, 0.42)):
        zt = ZR + 0.01
        tube(b, [V(x, 0, zt + 0.05), V(x, 0, zt - h)], 0.006, seg=4, tile="rope", caps=False)
        lathe(b, [(0.0, -0.2), (0.07, -0.19), (0.1, -0.14), (0.11, -0.1), (0.1, -0.05), (0.07, -0.01), (0.0, 0.0)], V(x, 0, zt - h), seg=10, tile="bag_orange")
    notes = ("fishing nets hung to dry over a peeled-pole rail on %d X-trestles (poles dug in 0.4 m), rail at z %.2f, %.0f m long along X; "
             "%s hung gathered over the rail in bunches of netting (gaps between, loose meshes across them; a cork float on each bunch's "
             "front hem, a lead weight on the back), "
             "two orange marker floats at the rail ends; no collision; no front (yaw freely)"
             % (len(xs), ZR, L, "one net" if variant == 0 else "two nets"))
    return b, {"notes": notes}


# =========================================================================================================
# picnic_table
# =========================================================================================================

def picnic_table(variant):
    b = PB("picnic_table")
    rnd = random.Random(51)
    LT, ZT = 1.9, 0.76
    # five heavy top planks
    for i in range(5):
        y = -0.32 + 0.16 * i
        box(b, V(rnd.uniform(-0.01, 0.01), y, ZT - 0.03 + rnd.uniform(-0.002, 0.002)), (LT + rnd.uniform(-0.02, 0.0), 0.15, 0.06), "wood", grain=0,
            rot=rz(rnd.uniform(-0.006, 0.006)))
    for sx in (-1, 1):
        x = sx * 0.68
        box(b, V(x, 0, ZT - 0.11), (0.1, 0.78, 0.1), "wood_dark", grain=1)               # cleat under the top
        box(b, V(x, 0, 0.36), (0.1, 1.78, 0.1), "wood_dark", grain=1)                    # cross beam carrying the benches
        for sy in (-1, 1):
            box(b, V(x, sy * 0.24, (-0.35 + ZT - 0.16) / 2), (0.12, 0.12, ZT - 0.16 + 0.35), "wood_dark", grain=2)    # table post
            box(b, V(x, sy * 0.78, (-0.35 + 0.31) / 2), (0.11, 0.11, 0.66), "wood_dark", grain=2)                     # bench post
    box(b, V(0, 0, 0.36), (1.26, 0.09, 0.09), "wood_dark", grain=0)                    # stretcher
    for sx in (-1, 1):                                                                    # braces from the stretcher to the top
        beam(b, V(sx * 0.2, 0, 0.4), V(sx * 0.62, 0, ZT - 0.17), 0.07, 0.07, "wood_dark", up=V(0, 1, 0))
    seats = []
    for sy in (-1, 1):
        for j, dy in enumerate((-0.075, 0.075)):
            box(b, V(rnd.uniform(-0.01, 0.01), sy * 0.66 + dy, 0.43), (LT, 0.14, 0.05), "wood", grain=0, rot=rz(rnd.uniform(-0.005, 0.005)))
        for x in (-0.45, 0.45):
            seats.append([x, round(sy * 0.66, 3), 0.455, 180.0 if sy < 0 else 0.0])
    notes = ("heavy timber picnic table, top 1.9 x 0.8 m at z %.2f, long along X, two fixed benches (seat top z 0.455) at y -0.66 / "
             "+0.66 on cross beams; posts dug in to z -0.35; 4 seats facing the table" % ZT)
    return b, {"seats": seats, "notes": notes}


KINDS = {
    "belvedere": (belvedere, {"variants": [0], "collision": "box"}),
    "viewer": (viewer, {"variants": [0], "collision": "exact"}),
    "bird_hide": (bird_hide, {"variants": [0], "collision": "box"}),
    "fisher_shed": (fisher_shed, {"variants": [0], "collision": "exact"}),
    "net_rack": (net_rack, {"variants": [0, 1], "collision": "none"}),
    "picnic_table": (picnic_table, {"variants": [0], "collision": "exact"}),
}
