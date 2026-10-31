"""A classic 1980s steel road racer (Soviet "Start-Shosse" / Colnago-era Italian lugged diamond frame) and a Soviet
wheel rack, made here from primitives only (no generated meshes): the game's bike kit.

  /Applications/Blender.app/Contents/MacOS/Blender -b -P blender/bikes/build_bike.py [-- --preview out_dir]

  -> design/bikes/game/bike_kit.glb                    every part, one mesh object each (SM_Bike_*, SM_BikeRack)
  -> unreal/Lyudmilovsk/Content/Bikes/bike.json        pivots and sockets the game reads (cm, Unreal frame)

Frame: Blender metres, X forward (the way the bike rides), Y to the bike's LEFT, Z up; origin on the ground directly under
the bottom bracket. The bike stands upright in the XZ plane (drivetrain on the right = -Y). Every part's mesh is exported
relative to its own pivot (object origin = pivot, identity rotation, scale 1) so the game can spin / steer it about the
pivot. UE = (x*100, -y*100, z*100) cm.

Glitch checks: blender/railway/kit_lint.py on every part as assembled (must report 0), plus our own: non-manifold edges,
zero-area faces, zero-area UVs.
"""
import json
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

V = Vector
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT_GLB = os.path.join(REPO, "design", "bikes", "game", "bike_kit.glb")
OUT_JSON = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "Bikes", "bike.json")

# --- the bike's measures (metres, degrees) ---------------------------------------------------------------------------
TYRE_R = 0.334                  # tyre outer radius (622 mm bead seat + 23 mm tyre)
AXLE_Z = TYRE_R
BB_Z = 0.27                     # bottom bracket centre height (BB drop 6.4 cm)
AXLE_R = V((-0.405, 0.0, AXLE_Z))       # rear axle centre
AXLE_F = V((0.595, 0.0, AXLE_Z))        # front axle centre (wheelbase 1.00 m)
BB = V((0.0, 0.0, BB_Z))
SEAT_ANG = 73.5
HEAD_ANG = 73.0
RAKE = 0.045                    # fork offset
CRANK = 0.170
Q_PEDAL = 0.155                 # pedal centre |y|
SEAT_DIR = V((-math.cos(math.radians(SEAT_ANG)), 0.0, math.sin(math.radians(SEAT_ANG))))   # up along the seat tube
HEAD_DIR = V((-math.cos(math.radians(HEAD_ANG)), 0.0, math.sin(math.radians(HEAD_ANG))))   # up along the steering axis
HEAD_FWD = V((math.sin(math.radians(HEAD_ANG)), 0.0, math.cos(math.radians(HEAD_ANG))))    # perpendicular, pointing forward
SEAT_TUBE_CC = 0.560            # BB centre to the top-tube junction along the seat tube
SEAT_TUBE_END = 0.600           # the seat lug's top
SADDLE_H = 0.740                # BB centre to the saddle top along the seat tube line
TOP_Z = BB_Z + SEAT_TUBE_CC * SEAT_DIR.z                      # horizontal top tube centre height
# the steering axis passes RAKE behind the front axle, perpendicular to itself
_Q = AXLE_F - HEAD_FWD * RAKE
HEAD_X = _Q.x + HEAD_DIR.x * ((TOP_Z - _Q.z) / HEAD_DIR.z)    # where the top tube meets the head tube axis
HEAD_J = V((HEAD_X, 0.0, TOP_Z))
HEAD_LEN = 0.150
P_TOP = HEAD_J + HEAD_DIR * 0.045       # head tube top end (on the axis)
P_LOW = HEAD_J - HEAD_DIR * 0.105       # head tube lower end = steering pivot (crown race)
SEAT_TOP_PT = BB + SEAT_DIR * SADDLE_H  # saddle top centre
Y_CHAIN = -0.0510               # the chain plane (big ring, middle cog) -- right = -Y
COG_TEETH = [14, 16, 18, 21, 24, 28]   # outermost (smallest) first
COG_DY = 0.0050                 # spacing
COG_Y0 = Y_CHAIN - 2 * COG_DY   # the smallest cog's y; the chain runs on index 2
RING_TEETH = (52, 42)
RING_Y = (Y_CHAIN, Y_CHAIN + 0.0075)    # big, small
REAR_OLD = 0.130                # rear dropout inner spacing
FRONT_OLD = 0.100
PITCH = 0.0127

WHEEL_SEGS = 64
BAR_Z0 = 0.925                  # handlebar top centre line height
POST_TOP_S = 0.706              # seat post clamp centre along the seat tube line from BB

PALETTE = {
    # name: (base colour sRGB 0..1, roughness, metallic)
    "Bike_Paint": ((0.55, 0.04, 0.05), 0.35, 0.0),
    "Bike_Accent": ((0.93, 0.92, 0.88), 0.4, 0.0),
    "Bike_Chrome": ((0.82, 0.83, 0.85), 0.15, 1.0),
    "Bike_Alu": ((0.72, 0.73, 0.75), 0.35, 1.0),
    "Bike_Steel": ((0.30, 0.30, 0.32), 0.4, 1.0),
    "Bike_Black": ((0.03, 0.03, 0.03), 0.6, 0.0),
    "Bike_Tyre": ((0.02, 0.02, 0.02), 0.8, 0.0),
    "Bike_Gumwall": ((0.55, 0.38, 0.20), 0.75, 0.0),
    "Bike_Tape": ((0.88, 0.86, 0.78), 0.9, 0.0),
    "Bike_Hood": ((0.42, 0.26, 0.14), 0.7, 0.0),
    "Bike_Saddle": ((0.20, 0.10, 0.05), 0.5, 0.0),
    "Bike_Strap": ((0.04, 0.03, 0.03), 0.6, 0.0),
    "Bike_Chain": ((0.30, 0.30, 0.31), 0.35, 1.0),
    "Bike_Bottle": ((0.92, 0.92, 0.90), 0.4, 0.0),
    "Rack_Steel": ((0.52, 0.54, 0.56), 0.55, 0.8),
}


def srgb_to_lin(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def material(name):
    m = bpy.data.materials.get(name)
    if m:
        return m
    col, rough, metal = PALETTE[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (*[srgb_to_lin(c) for c in col], 1.0)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    m.diffuse_color = (*[srgb_to_lin(c) for c in col], 1.0)
    return m


def pitch_radius(n):
    return PITCH / (2.0 * math.sin(math.pi / n))


# --- mesh builder -------------------------------------------------------------------------------------------------------
class MB:
    """One part: bmesh + material slots. Every face gets UVs (tubes / lathes cylindrical in metres, the rest box-projected
    in metres at the end)."""

    def __init__(self):
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.hu = self.bm.faces.layers.int.new("hasuv")
        self.mats = []

    def mi(self, name):
        if name not in self.mats:
            self.mats.append(name)
        return self.mats.index(name)

    def v(self, p):
        return self.bm.verts.new(p)

    def face(self, vs, mat, uvs=None):
        try:
            f = self.bm.faces.new(vs)
        except ValueError:
            return None
        f.material_index = self.mi(mat)
        if uvs is not None:
            for lp, uv in zip(f.loops, uvs):
                lp[self.uv].uv = uv
            f[self.hu] = 1
        return f

    # -- sweeps -------------------------------------------------------------------------------------------------------
    def sweep(self, pts, prof, mat, scale=None, ref=None, closed=False, caps=(True, True), fixed=False, mats=None):
        """Sweep a 2D profile (list of (u, v)) along a polyline. scale: float or per-point list."""
        P = [V(p) for p in pts]
        n, m = len(P), len(prof)
        if scale is None:
            scale = 1.0
        S = scale if isinstance(scale, (list, tuple)) else [scale] * n
        T = []
        for i in range(n):
            if closed:
                a, b = P[i] - P[i - 1], P[(i + 1) % n] - P[i]
            else:
                a = P[i] - P[i - 1] if i > 0 else P[1] - P[0]
                b = P[i + 1] - P[i] if i < n - 1 else P[i] - P[i - 1]
            t = a.normalized() + b.normalized()
            if t.length < 1e-9:
                t = b
            T.append(t.normalized())
        if ref is None:
            ref = V((0, 0, 1)) if abs(T[0].z) < 0.9 else V((1, 0, 0))
        ref = V(ref)
        Ns = []
        for i in range(n):
            src = ref if (fixed or i == 0) else Ns[-1]
            nn = src - T[i] * src.dot(T[i])
            if nn.length < 1e-6:
                nn = ref.cross(T[i])
            Ns.append(nn.normalized())
        Bs = [T[i].cross(Ns[i]) for i in range(n)]
        rings = [[self.v(P[i] + S[i] * (prof[k][0] * Ns[i] + prof[k][1] * Bs[i])) for k in range(m)] for i in range(n)]
        # profile perimeter coordinate for u (metres, at scale 1 -> use mean scale)
        per = [0.0]
        for k in range(m):
            a, b = prof[k], prof[(k + 1) % m]
            per.append(per[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
        cum = [0.0]
        for i in range(1, n):
            cum.append(cum[-1] + (P[i] - P[i - 1]).length)
        if closed:
            cum.append(cum[-1] + (P[0] - P[n - 1]).length)
        for i in range(n if closed else n - 1):
            i2 = (i + 1) % n
            sc = 0.5 * (S[i] + S[i2])
            for k in range(m):
                k2 = (k + 1) % m
                mt = mats[k] if mats else mat
                uvs = [(per[k] * sc, cum[i]), (per[k + 1] * sc, cum[i]), (per[k + 1] * sc, cum[i + 1]), (per[k] * sc, cum[i + 1])]
                self.face([rings[i][k], rings[i][k2], rings[i2][k2], rings[i2][k]], mt, uvs)
        if not closed:
            if caps[0]:
                self.face(rings[0][::-1], mat)
            if caps[1]:
                self.face(rings[-1], mat)
        return rings

    def tube(self, pts, r, n, mat, caps=(True, True), phase=0.0, ref=None, fixed=False):
        prof = [(math.cos(phase + 2 * math.pi * k / n), math.sin(phase + 2 * math.pi * k / n)) for k in range(n)]
        return self.sweep(pts, prof, mat, scale=r, ref=ref, caps=caps, fixed=fixed)

    def cyl(self, a, b, r, n, mat, r2=None, caps=(True, True), phase=0.0):
        r2 = r if r2 is None else r2
        return self.tube([a, b], [r, r2], n, mat, caps=caps, phase=phase)

    def lathe(self, prof, c, ax, n, mat, closed=False, ref=None, phase=0.0, mats=None):
        """Revolve a profile of (r, h) about the axis through c; r < 1e-9 points are poles."""
        ax = V(ax).normalized()
        c = V(c)
        if ref is None:
            ref = V((0, 0, 1)) if abs(ax.z) < 0.9 else V((1, 0, 0))
        e1 = (V(ref) - ax * V(ref).dot(ax)).normalized()
        e2 = ax.cross(e1)
        rings = []
        for (r, h) in prof:
            if r < 1e-9:
                rings.append(self.v(c + ax * h))
            else:
                rings.append([self.v(c + ax * h + r * (math.cos(phase + 2 * math.pi * k / n) * e1 +
                                                       math.sin(phase + 2 * math.pi * k / n) * e2)) for k in range(n)])
        cum = [0.0]
        for j in range(len(prof)):
            a, b = prof[j], prof[(j + 1) % len(prof)]
            cum.append(cum[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
        npr = len(prof)
        for j in range(npr if closed else npr - 1):
            j2 = (j + 1) % npr
            A, B = rings[j], rings[j2]
            ra, rb = prof[j][0], prof[j2][0]
            mt = mats[j] if mats else mat
            for k in range(n):
                k2 = (k + 1) % n
                ua, ub = k / n * 2 * math.pi, (k + 1) / n * 2 * math.pi
                if ra < 1e-9 and rb < 1e-9:
                    continue
                if ra < 1e-9:
                    self.face([A, B[k], B[k2]], mt, [((ua + ub) / 2 * rb, cum[j]), (ua * rb, cum[j + 1]), (ub * rb, cum[j + 1])])
                elif rb < 1e-9:
                    self.face([A[k], A[k2], B], mt, [(ua * ra, cum[j]), (ub * ra, cum[j]), ((ua + ub) / 2 * ra, cum[j + 1])])
                else:
                    self.face([A[k], A[k2], B[k2], B[k]], mt,
                              [(ua * ra, cum[j]), (ub * ra, cum[j]), (ub * rb, cum[j + 1]), (ua * rb, cum[j + 1])])
        return rings

    def sphere(self, c, r, n, mat, squash=1.0, ax=(0, 0, 1)):
        prof = [(0.0, -r * squash)] + [(r * math.sin(math.pi * i / n), -r * squash * math.cos(math.pi * i / n)) for i in range(1, n)] + [(0.0, r * squash)]
        return self.lathe(prof, c, ax, max(6, n * 2 // 2), mat, closed=False)

    def torus(self, c, ax, R, r, n, k, mat):
        prof = [(R + r * math.cos(2 * math.pi * i / k), r * math.sin(2 * math.pi * i / k)) for i in range(k)]
        return self.lathe(prof, c, ax, n, mat, closed=True)

    def box(self, c, ex, ey, ez, mat):
        """Oriented box: centre c, half-extent vectors ex, ey, ez."""
        c, ex, ey, ez = V(c), V(ex), V(ey), V(ez)
        sg = [(-1, -1, -1), (1, -1, -1), (1, 1, -1), (-1, 1, -1), (-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]
        vs = [self.v(c + ex * a + ey * b + ez * d) for a, b, d in sg]
        quads = [((0, 3, 2, 1), ex, ey), ((4, 5, 6, 7), ex, ey), ((0, 1, 5, 4), ex, ez), ((2, 3, 7, 6), ex, ez),
                 ((1, 2, 6, 5), ey, ez), ((3, 0, 4, 7), ey, ez)]
        for idx, a, b in quads:
            an, bn = a.normalized(), b.normalized()
            uvs = [((vs[i].co - c).dot(an), (vs[i].co - c).dot(bn)) for i in idx]
            self.face([vs[i] for i in idx], mat, uvs)
        return vs

    def slab(self, poly, o, e1, e2, e3, t0, t1, mat):
        """Extrude a 2D polygon (in the e1/e2 plane at origin o) from t0 to t1 along e3."""
        o, e1, e2, e3 = V(o), V(e1), V(e2), V(e3)
        A = [self.v(o + e1 * x + e2 * y + e3 * t0) for x, y in poly]
        B = [self.v(o + e1 * x + e2 * y + e3 * t1) for x, y in poly]
        n = len(poly)
        per = [0.0]
        for i in range(n):
            a, b = poly[i], poly[(i + 1) % n]
            per.append(per[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
        th = abs(t1 - t0)
        for i in range(n):
            j = (i + 1) % n
            self.face([A[i], A[j], B[j], B[i]], mat, [(per[i], 0), (per[i + 1], 0), (per[i + 1], th), (per[i], th)])
        self.face(A[::-1], mat)
        self.face(B, mat)
        return A, B

    def gear(self, cx, cz, y0, y1, N, r_root, r_tip, r_in, mat, phase=0.0):
        """Toothed ring in the XZ plane (axis along Y) from y0 to y1: 4 points a tooth; sector n-gons front and back."""
        d = 2 * math.pi / N
        outer = []
        for k in range(N):
            a0 = phase + k * d
            for (fa, rr) in ((0.0, r_root), (0.20, r_tip), (0.45, r_tip), (0.62, r_root)):
                a = a0 + fa * d
                outer.append((cx + rr * math.cos(a), cz + rr * math.sin(a)))
        inner = [(cx + r_in * math.cos(phase + k * d), cz + r_in * math.sin(phase + k * d)) for k in range(N)]
        Ov = [[self.v((x, y, z)) for (x, z) in outer] for y in (y0, y1)]
        Iv = [[self.v((x, y, z)) for (x, z) in inner] for y in (y0, y1)]
        for s in (0, 1):
            for k in range(N):
                k2 = (k + 1) % N
                ring = [Ov[s][4 * k + i] for i in range(4)] + [Ov[s][(4 * k + 4) % (4 * N)], Iv[s][k2], Iv[s][k]]
                self.face(ring if s == 1 else ring[::-1], mat)
        for i in range(4 * N):
            j = (i + 1) % (4 * N)
            self.face([Ov[0][i], Ov[0][j], Ov[1][j], Ov[1][i]], mat)
        for k in range(N):
            k2 = (k + 1) % N
            self.face([Iv[0][k], Iv[1][k], Iv[1][k2], Iv[0][k2]], mat)

    # -- finishing ----------------------------------------------------------------------------------------------------
    def to_object(self, name, pivot, smooth_deg=40.0):
        bm = self.bm
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        bm.normal_update()
        # metre box UVs for every face the builders left alone
        for f in bm.faces:
            if f[self.hu]:
                continue
            n = f.normal
            ax = max(range(3), key=lambda i: abs(n[i]))
            ia, ib = [i for i in range(3) if i != ax]
            for lp in f.loops:
                co = lp.vert.co
                lp[self.uv].uv = (co[ia], co[ib])
        lim = math.radians(smooth_deg)
        for e in bm.edges:
            if len(e.link_faces) == 2:
                e.smooth = e.calc_face_angle(0.0) < lim
            else:
                e.smooth = False
        for f in bm.faces:
            f.smooth = True
        pv = V(pivot)
        bmesh.ops.translate(bm, vec=-pv, verts=bm.verts)
        me = bpy.data.meshes.new(name)
        bm.to_mesh(me)
        bm.free()
        for mname in self.mats:
            me.materials.append(material(mname))
        o = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(o)
        o.location = pv
        return o


def hull2d(pts):
    pts = sorted(set(pts))
    if len(pts) <= 2:
        return pts

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lo, up = [], []
    for p in pts:
        while len(lo) >= 2 and cross(lo[-2], lo[-1], p) <= 0:
            lo.pop()
        lo.append(p)
    for p in reversed(pts):
        while len(up) >= 2 and cross(up[-2], up[-1], p) <= 0:
            up.pop()
        up.append(p)
    return lo[:-1] + up[:-1]


def circle_pts(cx, cy, r, n, a0=0.0):
    return [(cx + r * math.cos(a0 + 2 * math.pi * k / n), cy + r * math.sin(a0 + 2 * math.pi * k / n)) for k in range(n)]


def lerp(a, b, t):
    return a + (b - a) * t


def bez(p0, p1, p2, p3, n):
    out = []
    for i in range(n + 1):
        t = i / n
        u = 1 - t
        out.append(p0 * (u ** 3) + p1 * (3 * u * u * t) + p2 * (3 * u * t * t) + p3 * (t ** 3))
    return out


def rot_y(v, deg):
    """Rotate a vector about +Y by deg (Blender right-handed): +X turns toward -Z."""
    a = math.radians(deg)
    return V((v.x * math.cos(a) + v.z * math.sin(a), v.y, -v.x * math.sin(a) + v.z * math.cos(a)))


def mirror_y(p):
    return V((p.x, -p.y, p.z))


# --- the chain's path (also fixes where the rear derailleur's jockey wheels hang) -----------------------------------------
JOCKEY_N = 11
R_JOCKEY = pitch_radius(JOCKEY_N)
R_RING = pitch_radius(RING_TEETH[0])
COG_MID = COG_TEETH[2]
R_COG = pitch_radius(COG_MID)


def _tangent(c1, r1, s1, c2, r2, s2):
    """Directed tangent from circle 1 (orientation s1, +1 ccw) to circle 2 -> (point on 1, point on 2)."""
    D = (c2[0] - c1[0], c2[1] - c1[1])
    L = math.hypot(*D)
    k = s2 * r2 - s1 * r1
    al = math.atan2(D[1], D[0])
    ar = math.asin(max(-1.0, min(1.0, -k / L)))
    for phi in (al + ar, al + math.pi - ar):
        d = (math.cos(phi), math.sin(phi))
        m = (d[1], -d[0])
        p1 = (c1[0] + s1 * r1 * m[0], c1[1] + s1 * r1 * m[1])
        p2 = (c2[0] + s2 * r2 * m[0], c2[1] + s2 * r2 * m[1])
        if (p2[0] - p1[0]) * d[0] + (p2[1] - p1[1]) * d[1] > 0:
            return p1, p2
    raise ValueError("no tangent")


def chain_path(cage_len, step_deg=4.0):
    """The chain's centre line in the (x, z) plane: ring (cw) -> tension jockey (cw) -> guide jockey (ccw) -> cog (cw)."""
    cog = (AXLE_R.x, AXLE_R.z)
    d = R_COG + R_JOCKEY + 0.0060
    g_dir = V((-0.10, -1.0, 0)).normalized()
    G = (cog[0] + d * g_dir.x, cog[1] + d * g_dir.y)
    cdir = V((0.42, -0.91, 0)).normalized()
    T = (G[0] + cage_len * cdir.x, G[1] + cage_len * cdir.y)
    circles = [((0.0, BB_Z), R_RING, -1), (T, R_JOCKEY, -1), (G, R_JOCKEY, 1), (cog, R_COG, -1)]
    nc = len(circles)
    segs = []
    for i in range(nc):
        c1, r1, s1 = circles[i]
        c2, r2, s2 = circles[(i + 1) % nc]
        segs.append(_tangent(c1, r1, s1, c2, r2, s2))
    pts = []
    for i in range(nc):
        c, r, s = circles[i]
        p_in = segs[i - 1][1]
        p_out = segs[i][0]
        a0 = math.atan2(p_in[1] - c[1], p_in[0] - c[0])
        a1 = math.atan2(p_out[1] - c[1], p_out[0] - c[0])
        delta = (a1 - a0) % (2 * math.pi) if s > 0 else -((a0 - a1) % (2 * math.pi))
        steps = max(2, int(abs(math.degrees(delta)) / step_deg) + 1)
        for k in range(steps):
            a = a0 + delta * k / steps
            pts.append((c[0] + r * math.cos(a), c[1] + r * math.sin(a)))
    length = sum(math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]) for i in range(len(pts)))
    return pts, length, G, T


def solve_chain():
    """Pick the cage's reach so the loop is a whole, even number of pitches (the game scrolls links around it)."""
    lo, hi = 0.050, 0.110
    l_lo, l_hi = chain_path(lo)[1], chain_path(hi)[1]
    target = round(chain_path(0.068)[1] / (2 * PITCH)) * 2
    assert l_lo <= target * PITCH <= l_hi, (l_lo, l_hi, target)
    for _ in range(60):
        mid = 0.5 * (lo + hi)
        if chain_path(mid)[1] < target * PITCH:
            lo = mid
        else:
            hi = mid
    cage = 0.5 * (lo + hi)
    pts, length, G, T = chain_path(cage)
    return pts, length, G, T, target, cage


CHAIN_PTS, CHAIN_LEN, JOCKEY_G, JOCKEY_T, CHAIN_LINKS, CAGE_LEN = solve_chain()


# --- wheels ------------------------------------------------------------------------------------------------------------
def build_wheel(front):
    m = MB()
    c = AXLE_F if front else AXLE_R
    ax = V((0, 1, 0))
    n = WHEEL_SEGS
    ph = -math.pi / 2          # (the lathe's e1 is +Z: the first ring of vertices points straight up; offset so a vertex points down)
    # rim: polished alu box section with a brake track
    rim = [(0.2910, -0.0060), (0.2910, 0.0060), (0.2995, 0.0100), (0.3035, 0.0100), (0.3085, 0.0065), (0.3085, -0.0065),
           (0.3035, -0.0100), (0.2995, -0.0100)]
    m.lathe(rim, c, ax, n, "Bike_Alu", closed=True)
    # tyre: tan gumwall sidewalls, black tread
    a_, b_ = 0.013, 0.0115
    mid_r = TYRE_R - a_
    tp, tm = [], []
    np_ = 20
    for i in range(np_):
        t = 2 * math.pi * i / np_
        tp.append((mid_r + a_ * math.cos(t), b_ * math.sin(t)))
    for i in range(np_):
        t = 2 * math.pi * (i + 0.5) / np_
        th = abs(((math.degrees(t) + 180) % 360) - 180)       # 0 at the tread crown
        tm.append("Bike_Tyre" if (th < 52 or th > 160) else "Bike_Gumwall")
    m.lathe(tp, c, ax, n, "Bike_Tyre", closed=True, mats=tm)
    # hub
    if front:
        fl = (-0.0310, 0.0310)
        half_o = FRONT_OLD / 2
    else:
        fl = (-0.0265, 0.0340)
        half_o = REAR_OLD / 2
    hub = [(0.0, fl[0] - 0.0105), (0.0090, fl[0] - 0.0105), (0.0090, fl[0] - 0.0020), (0.0225, fl[0] - 0.0020), (0.0225, fl[0] + 0.0020),
           (0.0125, fl[0] + 0.0020), (0.0125, fl[1] - 0.0020), (0.0225, fl[1] - 0.0020), (0.0225, fl[1] + 0.0020),
           (0.0090, fl[1] + 0.0020), (0.0090, fl[1] + 0.0105), (0.0, fl[1] + 0.0105)]
    m.lathe(hub, c, ax, 24, "Bike_Alu", closed=False)
    # axle, locknuts, quick release
    y_end = (0.056 if front else 0.0735) + 0.0025
    m.cyl(c + V((0, -y_end, 0)), c + V((0, y_end, 0)), 0.0045, 10, "Bike_Steel")
    for s in (-1, 1):
        if not front and s < 0:
            continue                                  # (inside the freewheel body)
        yf = abs(fl[0 if s < 0 else 1])
        m.cyl(c + V((0, s * (yf + 0.0105), 0)), c + V((0, s * (yf + 0.0155), 0)), 0.0085, 6, "Bike_Steel")
        y_plate_in = half_o
        m.cyl(c + V((0, s * (yf + 0.0155), 0)), c + V((0, s * (y_plate_in - 0.0002), 0)), 0.0075, 10, "Bike_Steel")
    # the dropouts' outer faces sit at half_o + plate thickness; the QR body / nut sit outside them
    plate_out = (0.056 if front else 0.0735)
    cam_y0, cam_y1 = plate_out, plate_out + 0.0080
    m.cyl(c + V((0, -(plate_out + 0.0005), 0)), c + V((0, -(plate_out + 0.0085), 0)), 0.0095, 8, "Bike_Chrome")          # nut (right)
    m.cyl(c + V((0, cam_y0 + 0.0005, 0)), c + V((0, cam_y1, 0)), 0.0115, 12, "Bike_Chrome")                          # cam (left)
    m.cyl(c + V((0, -(plate_out + 0.0110), 0)), c + V((0, cam_y1 + 0.0100, 0)), 0.0022, 6, "Bike_Steel")             # skewer
    yl = cam_y1 + 0.0030
    if front:
        lever_pts = [c + V((-0.002, yl, 0.0)), c + V((-0.030, yl + 0.002, 0.008)), c + V((-0.070, yl + 0.004, 0.020))]
    else:
        lever_pts = [c + V((0.002, yl, 0.0)), c + V((0.004, yl + 0.002, 0.030)), c + V((0.000, yl + 0.004, 0.066))]
    lever_prof = [(-0.0016, -0.0050), (0.0016, -0.0050), (0.0016, 0.0050), (-0.0016, 0.0050)]
    m.sweep(lever_pts, lever_prof, "Bike_Chrome", ref=V((0, 1, 0)), fixed=True)
    m.sphere(lever_pts[-1] + (lever_pts[-1] - lever_pts[-2]).normalized() * 0.003, 0.0065, 6, "Bike_Black", squash=0.8)
    # spokes: 36, 3-cross, 4-sided
    n_sp = 36
    r_fl = 0.0215
    r_rim = 0.2975
    for side, y_f, y_r in ((-1, fl[0], -0.0030), (1, fl[1], 0.0030)):
        for j in range(18):
            th_h = (j * 20.0 + (10.0 if side < 0 else 0.0)) * math.pi / 180
            dirn = 1 if j % 2 == 0 else -1
            th_r = th_h + dirn * math.radians(60.0)
            ph_ = c + V((r_fl * math.cos(th_h), y_f, r_fl * math.sin(th_h)))
            pr_ = c + V((r_rim * math.cos(th_r), y_r, r_rim * math.sin(th_r)))
            m.tube([ph_, pr_], 0.00085, 4, "Bike_Steel", phase=math.pi / 4)
    # valve stem pointing in from the rim, with its cap
    va = math.radians(95)
    vb = c + V((0.2910 * math.cos(va), 0, 0.2910 * math.sin(va)))
    vdir = V((math.cos(va), 0, math.sin(va)))
    m.cyl(vb, vb - vdir * 0.032, 0.0030, 8, "Bike_Steel")
    m.cyl(vb - vdir * 0.032, vb - vdir * 0.036, 0.0036, 8, "Bike_Black")
    if not front:
        # freewheel body and the 6 cogs (the chain runs on index 2)
        body_r = 0.0210
        m.cyl(c + V((0, -0.0280, 0)), c + V((0, -0.0625, 0)), body_r, 14, "Bike_Steel")
        for i, N in enumerate(COG_TEETH):
            y = COG_Y0 + i * COG_DY
            R = pitch_radius(N)
            m.gear(c.x, c.z, y - 0.0008, y + 0.0008, N, R - 0.0040, R + 0.0035, body_r + 0.0003, "Bike_Steel", phase=i * 0.1)
    return m.to_object("SM_Bike_WheelF" if front else "SM_Bike_WheelR", c)


# --- crank, chainrings, pedals -----------------------------------------------------------------------------------------
def build_crank():
    m = MB()
    ey = V((0, 1, 0))
    ex, ez = V((1, 0, 0)), V((0, 0, 1))
    # BB spindle through the shell, both crank arms (right points +X, left -X)
    m.cyl(BB + V((0, -0.0700, 0)), BB + V((0, 0.0700, 0)), 0.0085, 12, "Bike_Steel")
    for side, ang in ((-1, 0.0), (1, math.pi)):
        pts = hull2d(circle_pts(0.0, 0.0, 0.0205, 16) + circle_pts(CRANK, 0.0, 0.0095, 14))
        # local polygon x along the arm
        c_, s_ = math.cos(ang), math.sin(ang)
        poly = [(p[0] * c_, p[1]) for p in pts]
        y0, y1 = (0.0625, 0.0725) if side > 0 else (-0.0725, -0.0625)
        m.slab(poly, BB, ex, ez, ey, y0, y1, "Bike_Alu")
        # pedal boss ring on the outer face and the crank bolt dust cap
        m.cyl(BB + V((0, side * 0.0725, 0)), BB + V((0, side * 0.0760, 0)), 0.0095, 10, "Bike_Chrome")
    # right side: boss, 5-arm spider (one slab), two chainrings, bolt heads, spacers
    m.cyl(BB + V((0, -0.0625, 0)), BB + V((0, -0.0590, 0)), 0.0170, 14, "Bike_Alu")
    outline = []
    arms = 5
    for k in range(arms):
        a = math.radians(-90.0 + 72.0 * k + 36.0)       # the arms straddle the right crank arm's direction
        outline += [(a - 0.26, 0.0230, None), (a - 0.075, 0.0675, None), (a + 0.075, 0.0675, None), (a + 0.26, 0.0230, None)]
        a_next = math.radians(-90.0 + 72.0 * (k + 1) + 36.0) - 0.26
        for q in range(1, 4):
            outline.append((lerp(a + 0.26, a_next, q / 4), 0.0230, None))
    poly = [(r * math.cos(a), r * math.sin(a)) for a, r, _ in outline]
    m.slab(poly, BB, ex, ez, ey, -0.0590, -0.0540, "Bike_Alu")
    for k in range(arms):
        a = math.radians(-90.0 + 72.0 * k + 36.0)
        bp = BB + V((0.0605 * math.cos(a), 0, 0.0605 * math.sin(a)))
        m.cyl(bp + V((0, -0.0590, 0)), bp + V((0, -0.0620, 0)), 0.0052, 8, "Bike_Chrome")
        m.cyl(bp + V((0, -0.0540, 0)), bp + V((0, -0.0405, 0)), 0.0042, 8, "Bike_Steel")
    for N, y in zip(RING_TEETH, RING_Y):
        R = pitch_radius(N)
        m.gear(0.0, BB_Z, y - 0.0008, y + 0.0008, N, R - 0.0040, R + 0.0035, 0.0500, "Bike_Alu" if N == RING_TEETH[1] else "Bike_Chrome")
    return m.to_object("SM_Bike_Crank", BB)


def build_pedal(right):
    m = MB()
    out = -1.0 if right else 1.0        # outboard direction (the right pedal is at -Y)
    c = V((0, 0, 0))
    # quill pedal: end plates, front and rear bars, spindle, pin
    for sx in (-1, 1):
        m.box((sx * 0.0365, 0, 0), (0.0035, 0, 0), (0, 0.0435, 0), (0, 0, 0.0105), "Bike_Alu")
    for s in (-1, 1):
        m.box((0, s * 0.0455, 0), (0.0400, 0, 0), (0, 0.0020, 0), (0, 0, 0.0105), "Bike_Alu")
    m.cyl(V((0, -out * 0.0825, 0)), V((0, -out * 0.0725, 0)), 0.0085, 10, "Bike_Steel")           # collar at the crank
    m.cyl(V((0, -out * 0.0725, 0)), V((0, out * 0.0435, 0)), 0.0055, 8, "Bike_Steel")
    m.cyl(V((0, out * 0.0455, 0)), V((0, out * 0.0500, 0)), 0.0065, 8, "Bike_Chrome")               # outer dust cap
    # toe clip: two flat steel straps from the front bar, curling up and forward, joined across the nose
    prof = [(-0.0055, -0.0011), (0.0055, -0.0011), (0.0055, 0.0011), (-0.0055, 0.0011)]
    path = [V((0.0400, 0, 0.0)), V((0.0700, 0, 0.0020)), V((0.0930, 0, 0.0130)), V((0.1060, 0, 0.0330)), V((0.1030, 0, 0.0520)),
            V((0.0900, 0, 0.0640))]
    for s in (-1, 1):
        pts = [p + V((0, s * 0.0345, 0)) for p in path]
        m.sweep(pts, prof, "Bike_Chrome", ref=V((0, 1, 0)), fixed=True)
    m.box((0.0900, 0, 0.0652), (0.0070, 0, 0), (0, 0.0402, 0), (0, 0, 0.0013), "Bike_Chrome")
    # leather strap: a loop in the YZ plane through the middle of the cage, with a buckle on the outer side
    loop = []
    hw, z0, z1, rc = 0.0385, -0.0170, 0.0780, 0.0110
    for (cy_, cz_, a0) in ((hw - rc, z0 + rc, -90), (hw - rc, z1 - rc, 0), (-hw + rc, z1 - rc, 90), (-hw + rc, z0 + rc, 180)):
        for q in range(7):
            a = math.radians(a0 + 90.0 * q / 6)
            loop.append(V((-0.0190, cy_ + rc * math.cos(a), cz_ + rc * math.sin(a))))
    sprof = [(-0.0090, -0.0016), (0.0090, -0.0016), (0.0090, 0.0016), (-0.0090, 0.0016)]
    m.sweep(loop, sprof, "Bike_Strap", ref=V((1, 0, 0)), fixed=True, closed=True)
    m.box((-0.0190, out * (hw + 0.0036), 0.045), (0.0120, 0, 0), (0, 0.0016, 0), (0, 0, 0.0100), "Bike_Chrome")
    return m.to_object("SM_Bike_PedalR" if right else "SM_Bike_PedalL", c)


# --- the chain ---------------------------------------------------------------------------------------------------------
def build_chain():
    m = MB()
    pts = CHAIN_PTS
    n = len(pts)
    P = [V((p[0], 0, p[1])) for p in pts]
    # arc-length U in link pitches, one seam
    s = [0.0]
    for i in range(1, n):
        s.append(s[-1] + (P[i] - P[i - 1]).length)
    s.append(s[-1] + (P[0] - P[-1]).length)
    L = s[-1]
    U = [x / PITCH * (CHAIN_LINKS * PITCH / L) for x in s]       # exactly CHAIN_LINKS at the seam
    W = 0.0075
    for yc in (Y_CHAIN - 0.0025, Y_CHAIN + 0.0025):
        y_a, y_b = yc - 0.00045, yc + 0.00045
        ring = []
        for i in range(n):
            t = (P[(i + 1) % n] - P[i - 1]).normalized()
            nrm = V((-t.z, 0, t.x))
            inn, out = P[i] - nrm * W / 2, P[i] + nrm * W / 2
            ring.append([m.v((inn.x, y_a, inn.z)), m.v((out.x, y_a, out.z)), m.v((out.x, y_b, out.z)), m.v((inn.x, y_b, inn.z))])
        for i in range(n):
            j = (i + 1) % n
            u0, u1 = U[i], U[i + 1]
            # faces: -y plate face, outer edge, +y plate face, inner edge
            m.face([ring[i][0], ring[i][1], ring[j][1], ring[j][0]], "Bike_Chain", [(u0, 0), (u0, 1), (u1, 1), (u1, 0)])
            m.face([ring[i][1], ring[i][2], ring[j][2], ring[j][1]], "Bike_Chain", [(u0, 0.9), (u0, 1.0), (u1, 1.0), (u1, 0.9)])
            m.face([ring[i][2], ring[i][3], ring[j][3], ring[j][2]], "Bike_Chain", [(u0, 1), (u0, 0), (u1, 0), (u1, 1)])
            m.face([ring[i][3], ring[i][0], ring[j][0], ring[j][3]], "Bike_Chain", [(u0, 0.0), (u0, 0.1), (u1, 0.1), (u1, 0.0)])
    return m.to_object("SM_Bike_Chain", V((0, 0, 0)))


# --- tube helpers for the frame ----------------------------------------------------------------------------------------
def sleeve(m, a, b, r, n=12, phase=0.0, mat="Bike_Chrome", r_end=None):
    """A thin chrome lug sleeve over a tube: r is the tube's radius; the sleeve stands 2.3 mm proud (>= the linter's 2 mm)."""
    ro = r + 0.0023
    r_e = ro if r_end is None else r_end
    m.cyl(a, b, ro, n, mat, r2=r_e, phase=phase)


def band(m, a, b, r, n=12, mat="Bike_Accent"):
    """A pinstripe ring, turned half a facet so its flats clear the tube's corners by >= 0.5 mm."""
    ph = math.pi / n
    rb = r / math.cos(math.pi / n) + 0.0006
    m.cyl(a, b, rb, n, mat, phase=ph)


def tube_at(a, b, t):
    return a + (b - a) * t


def stay_y(x, side, x0=-0.025, x1=-0.392, y0=0.0245, y1=0.0682):
    return side * lerp(y0, y1, (x - x0) / (x1 - x0))




def build_frame():
    m = MB()
    up = V((0, 0, 1))
    r_tt, r_dt, r_st, r_ht = 0.0127, 0.0143, 0.0143, 0.0158
    # BB shell and cups
    m.cyl(BB + V((0, -0.0340, 0)), BB + V((0, 0.0340, 0)), 0.0175, 14, "Bike_Paint")
    for s in (-1, 1):
        m.cyl(BB + V((0, s * 0.0340, 0)), BB + V((0, s * 0.0385, 0)), 0.0195, 14, "Bike_Alu")
    # main tubes
    st_end = BB + SEAT_DIR * SEAT_TUBE_END
    m.tube([BB, st_end], r_st, 12, "Bike_Paint")
    tt_s = BB + SEAT_DIR * SEAT_TUBE_CC
    m.tube([tt_s, HEAD_J], r_tt, 12, "Bike_Paint")
    dt_h = P_LOW + HEAD_DIR * 0.032
    m.tube([BB, dt_h], r_dt, 12, "Bike_Paint")
    m.tube([P_LOW, P_TOP], r_ht, 12, "Bike_Paint")
    # lugs
    sleeve(m, P_TOP - HEAD_DIR * 0.034, P_TOP - HEAD_DIR * 0.003, r_ht, 12)
    sleeve(m, P_LOW + HEAD_DIR * 0.003, P_LOW + HEAD_DIR * 0.040, r_ht, 12)
    sleeve(m, tt_s + SEAT_DIR * -0.060, st_end - SEAT_DIR * 0.003, r_st, 12)                       # seat lug
    sleeve(m, HEAD_J + V((-0.052, 0, 0)), HEAD_J + V((-0.006, 0, 0)), r_tt, 12)
    sleeve(m, dt_h + (BB - dt_h).normalized() * 0.006, dt_h + (BB - dt_h).normalized() * 0.060, r_dt, 12)
    sleeve(m, BB + (dt_h - BB).normalized() * 0.014, BB + (dt_h - BB).normalized() * 0.050, r_dt, 12)
    # pinstripes: each side of the lugs on the top tube / down tube / seat tube
    tdir = V((1, 0, 0))
    band(m, HEAD_J + V((-0.066, 0, 0)), HEAD_J + V((-0.060, 0, 0)), r_tt)
    band(m, HEAD_J + V((-0.076, 0, 0)), HEAD_J + V((-0.072, 0, 0)), r_tt)
    band(m, tt_s + V((0.088, 0, 0)), tt_s + V((0.094, 0, 0)), r_tt)
    band(m, tt_s + V((0.100, 0, 0)), tt_s + V((0.104, 0, 0)), r_tt)
    ddir = (dt_h - BB).normalized()
    dlen = (dt_h - BB).length
    band(m, dt_h - ddir * 0.074, dt_h - ddir * 0.068, r_dt)
    band(m, dt_h - ddir * 0.084, dt_h - ddir * 0.080, r_dt)
    band(m, BB + ddir * 0.066, BB + ddir * 0.072, r_dt)
    band(m, BB + ddir * 0.078, BB + ddir * 0.082, r_dt)
    band(m, BB + SEAT_DIR * 0.064, BB + SEAT_DIR * 0.070, r_st)
    band(m, BB + SEAT_DIR * 0.076, BB + SEAT_DIR * 0.080, r_st)
    # seat binder ear
    sb = BB + SEAT_DIR * 0.585 + V((-1, 0, 0)) * 0.0215
    m.cyl(sb + V((0, -0.0150, 0)), sb + V((0, 0.0150, 0)), 0.0042, 8, "Bike_Chrome")
    m.box(sb + V((0.0035, 0, 0)), (0.0080, 0, 0), (0, 0.0100, 0), (0, 0, 0.0060), "Bike_Chrome")
    # chain stays and seat stays, dropouts
    for side in (-1, 1):
        cs0 = V((-0.025, side * 0.0245, BB_Z))
        cs1 = V((-0.392, side * 0.0682, AXLE_Z + 0.004))
        n_st = 8
        pts = [cs0 + (cs1 - cs0) * (i / n_st) for i in range(n_st + 1)]
        rr = [lerp(0.0095, 0.0055, i / n_st) for i in range(n_st + 1)]
        m.tube(pts, rr, 10, "Bike_Paint")
        band(m, cs0 + (cs1 - cs0) * 0.10, cs0 + (cs1 - cs0) * 0.115, lerp(0.0095, 0.0055, 0.1) , 10)
        ss0 = BB + SEAT_DIR * 0.535 + V((0, side * 0.0105, 0))
        ss1 = V((-0.392, side * 0.0682, AXLE_Z + 0.017))
        pts = [ss0 + (ss1 - ss0) * (i / 10) for i in range(11)]
        rr = [lerp(0.0080, 0.0055, i / 10) for i in range(11)]
        m.tube(pts, rr, 10, "Bike_Paint")
        band(m, ss0 + (ss1 - ss0) * 0.16, ss0 + (ss1 - ss0) * 0.175, lerp(0.0080, 0.0055, 0.16), 10)
        # dropout plates (chrome) with the right-hand derailleur hanger
        m.box(V((AXLE_R.x - 0.001, side * 0.0693, AXLE_Z + 0.004)), (0.0125, 0, 0), (0, 0.0042, 0), (0, 0, 0.0170), "Bike_Chrome")
    # rear brake bridge and caliper mount
    br_x = BB.x - 0.0 + (-0.2327)
    br_z = 0.645
    m.tube([V((br_x, -0.0300, br_z)), V((br_x, 0.0300, br_z))], 0.0075, 10, "Bike_Paint")
    # hanger ear on the right dropout (behind the axle)
    m.box(V((AXLE_R.x - 0.0245, -0.0693, AXLE_Z + 0.002)), (0.0110, 0, 0), (0, 0.0042, 0), (0, 0, 0.0100), "Bike_Chrome")
    # left chain stay: stand bracket (a clamp ring + hinge ear)
    # seat post (fluted), head, saddle rails and the leather saddle
    post_top = BB + SEAT_DIR * POST_TOP_S
    n_fl = 16
    fl_prof = [(math.cos(2 * math.pi * k / n_fl) * (1.0 if k % 2 == 0 else 0.93), math.sin(2 * math.pi * k / n_fl) * (1.0 if k % 2 == 0 else 0.93)) for k in range(n_fl)]
    m.sweep([st_end, BB + SEAT_DIR * 0.690], fl_prof, "Bike_Alu", scale=0.0128)
    m.cyl(BB + SEAT_DIR * 0.690, BB + SEAT_DIR * 0.702, 0.0155, 12, "Bike_Alu")
    m.cyl(post_top + V((0, -0.0210, 0)), post_top + V((0, 0.0210, 0)), 0.0085, 12, "Bike_Alu")
    for s in (-1, 1):
        m.cyl(post_top + V((0, s * 0.0210, 0)), post_top + V((0, s * 0.0250, 0)), 0.0070, 8, "Bike_Chrome")
    saddle_geo = saddle(m)
    # front derailleur: braze-on clamp on the seat tube, cage over the big ring's chain
    fd = BB + SEAT_DIR * 0.150
    m.cyl(fd - SEAT_DIR * 0.007, fd + SEAT_DIR * 0.007, r_st + 0.0025, 12, "Bike_Alu")
    cage_x = fd.x - 0.004
    for yc in (FD_YO, FD_YI):
        m.box(V((cage_x, yc, 0.386)), (0.016, 0, 0), (0, 0.00075, 0), (0, 0, 0.0200), "Bike_Alu")
    m.box(V((cage_x, Y_CHAIN - 0.0005, 0.4075)), (0.0160, 0, 0), (0, 0.0095, 0), (0, 0, 0.0020), "Bike_Alu")
    m.box(V((fd.x, -0.0345, 0.4130)), (0.0100, 0, 0), (0, 0.0180, 0), (0, 0, 0.0025), "Bike_Alu")
    # rear derailleur
    rear_derailleur(m)
    # rear brake
    caliper(m, V((br_x + 0.0130, 0, br_z)), pad_z=rim_pad_z(br_x + 0.0130, AXLE_R))
    # down-tube friction shifters
    for side in (-1, 1):
        base = dt_h - ddir * 0.250
        yb = side * (r_dt + 0.0060)
        m.cyl(base + V((0, side * (r_dt - 0.002), 0)), base + V((0, yb + side * 0.004, 0)), 0.0090, 10, "Bike_Chrome")
        m.cyl(base + V((0, yb + side * 0.004, 0)), base + V((0, yb + side * 0.008, 0)), 0.0060, 10, "Bike_Black")
        lever_dir = (V((1, 0, 0)) * 0.55 + V((0, 0, 1)) * 0.83).normalized()
        lp = [base + V((0, yb + side * 0.0100, 0)), base + V((0, yb + side * 0.0100, 0)) + lever_dir * 0.050,
              base + V((0, yb + side * 0.0100, 0)) + lever_dir * 0.070 + V((0.004, 0, 0.0))]
        m.tube(lp, 0.0030, 8, "Bike_Chrome")
        m.sphere(lp[-1] + lever_dir * 0.004, 0.0062, 6, "Bike_Black")
    # bottle cage and bottle on the down tube
    bottle(m, dt_h, ddir)
    # stand bracket on the left chain stay
    stand_bracket(m)
    return m.to_object("SM_Bike_Frame", V((0, 0, 0))), saddle_geo


FD_YO = Y_CHAIN - 0.0066
FD_YI = Y_CHAIN + 0.0056


def rim_pad_z(x, axle):
    dx = x - axle.x
    return axle.z + math.sqrt(0.3035 ** 2 - dx * dx)


def saddle(m):
    """Leather saddle (27 cm, nose forward) with chrome rails; returns the key points for bike.json."""
    mid = V((BB.x + SEAT_DIR.x * SADDLE_H, 0.0, BB.z + SEAT_DIR.z * SADDLE_H))
    ztop = mid.z
    st = [(-0.135, 0.060), (-0.122, 0.071), (-0.098, 0.081), (-0.068, 0.085), (-0.032, 0.076), (0.0, 0.062), (0.036, 0.046),
          (0.072, 0.031), (0.100, 0.021), (0.120, 0.016), (0.135, 0.0125)]

    def z_top(u):
        return ztop + 0.008 * max(0.0, (-u - 0.04) / 0.095) ** 2 + 0.013 * max(0.0, (u - 0.04) / 0.095) ** 2
    n = 16
    rings = []
    for u, hw in st:
        h = 0.0085 if u < 0.1 else 0.0065
        zt = z_top(u)
        zc = zt - h
        ring = []
        for k in range(n):
            t = 2 * math.pi * k / n
            cs, sn = math.cos(t), math.sin(t)
            yy = hw * math.copysign(abs(cs) ** 0.55, cs)
            zz = zc + h * math.copysign(abs(sn) ** 0.65, sn)
            ring.append(m.v((mid.x + u, yy, zz)))
        rings.append(ring)
    for i in range(len(st) - 1):
        for k in range(n):
            k2 = (k + 1) % n
            m.face([rings[i][k], rings[i][k2], rings[i + 1][k2], rings[i + 1][k]], "Bike_Saddle")
    m.face(rings[0][::-1], "Bike_Saddle")
    m.face(rings[-1], "Bike_Saddle")
    # rails: one closed wire under the shell, nose to tail
    def rz(u):
        return z_top(u) - 0.0235
    path = []
    for (u, y) in ((0.108, 0.0030), (0.070, 0.0110), (0.030, 0.0145), (-0.030, 0.0145), (-0.070, 0.0300), (-0.103, 0.0460), (-0.118, 0.0360), (-0.124, 0.0100)):
        path.append((u, y))
    full = [(u, y) for (u, y) in path] + [(u, -y) for (u, y) in reversed(path)]
    pts = [V((mid.x + u, y, rz(u))) for (u, y) in full]
    m.tube(pts, 0.0025, 8, "Bike_Chrome")
    sit = V((mid.x - 0.065, 0.0, z_top(-0.065)))
    nose = V((mid.x + 0.135, 0.0, z_top(0.135)))
    return {"mid": mid, "sit": sit, "nose": nose}


def rear_derailleur(m):
    G, T = JOCKEY_G, JOCKEY_T
    for c in (G, T):
        R = R_JOCKEY
        m.gear(c[0], c[1], Y_CHAIN - 0.0008, Y_CHAIN + 0.0008, JOCKEY_N, R - 0.0030, R + 0.0022, 0.0075, "Bike_Black")
    # cage plates: hull of the pivot, guide and tension circles, 1.5 mm thick, 2.4 mm clear of the chain
    pivot = (AXLE_R.x - 0.023, AXLE_Z - 0.056)
    pts = hull2d(circle_pts(pivot[0], pivot[1], 0.0095, 10) + circle_pts(G[0], G[1], 0.0150, 12) + circle_pts(T[0], T[1], 0.0150, 12))
    for yo, yi in ((FD_YO - 0.00075, FD_YO + 0.00075), (FD_YI - 0.00075, FD_YI + 0.00075)):
        m.slab(pts, V((0, 0, 0)), V((1, 0, 0)), V((0, 0, 1)), V((0, 1, 0)), yo, yi, "Bike_Alu")
    for c in (G, T):
        m.cyl(V((c[0], Y_CHAIN - 0.0100, c[1])), V((c[0], Y_CHAIN + 0.0100, c[1])), 0.0035, 8, "Bike_Steel")
    # hanger bolt, body under the hanger ear, two parallelogram links to the cage pivot
    hang_y = -0.0693
    body = V((AXLE_R.x - 0.0300, -0.0765, AXLE_Z - 0.0280))
    m.cyl(V((AXLE_R.x - 0.0295, hang_y - 0.0042, AXLE_Z + 0.0020)), V((AXLE_R.x - 0.0295, hang_y - 0.0085, AXLE_Z + 0.0020)), 0.0052, 8, "Bike_Chrome")
    m.box(body, (0.0070, 0, 0), (0, 0.0045, 0), (0, 0, 0.0200), "Bike_Alu")
    for dz, dz2 in ((0.011, 0.012), (-0.002, -0.001)):
        a = V((body.x, -0.0720 + 0.0006, body.z + dz))
        b = V((pivot[0] + 0.001, FD_YO - 0.0010, pivot[1] + dz2 + 0.004))
        m.tube([a, b], 0.0030, 8, "Bike_Alu")
    m.cyl(V((pivot[0], Y_CHAIN - 0.0100, pivot[1])), V((pivot[0], Y_CHAIN + 0.0100, pivot[1])), 0.0050, 8, "Bike_Steel")


def caliper(m, piv, pad_z, front=False):
    """Single-pivot side-pull caliper: a bolt along X at piv, two flat arms curling down round the tyre to rubber pads on the
    rim's brake track (pad centres at pad_z)."""
    prof = [(-0.0065, -0.0014), (0.0065, -0.0014), (0.0065, 0.0014), (-0.0065, 0.0014)]
    for s in (-1, 1):
        zp = piv.z
        arm = [V((piv.x, s * 0.0045, zp + 0.0010)), V((piv.x, s * 0.0110, zp - 0.0020)), V((piv.x, s * 0.0180, zp - 0.0100)),
               V((piv.x, s * 0.0180, pad_z + 0.0200)), V((piv.x, s * 0.0180, pad_z + 0.0040))]
        m.sweep(arm, prof, "Bike_Chrome", ref=V((1, 0, 0)), fixed=True)
        m.box(V((piv.x, s * 0.0135, pad_z)), (0.0085, 0, 0), (0, 0.0030, 0), (0, 0, 0.0100), "Bike_Black")        # pad
    m.cyl(piv + V((-0.0105, 0, 0)), piv + V((0.0115, 0, 0)), 0.0048, 10, "Bike_Chrome")
    m.cyl(piv + V((0.0115, 0, 0)), piv + V((0.0150, 0, 0)), 0.0070, 8, "Bike_Chrome")


def bottle(m, dt_h, ddir):
    top = V((-ddir.z, 0, ddir.x))                  # the down tube's upper-rear side
    if top.z < 0:
        top = -top
    r_dt = 0.0143
    s0 = 0.150
    rb = 0.0365
    axis_off = r_dt + rb + 0.0075
    # backing plate on the tube
    m.box(BB + ddir * (s0 + 0.080) + top * (r_dt + 0.0010), ddir * 0.080, V((0, 0.0100, 0)), top * 0.0010, "Bike_Alu")
    # bottle: 73 mm x 19 cm, tapered shoulder, nozzle cap
    c0 = BB + ddir * s0 + top * axis_off
    prof = [(0.0, 0.0), (0.0300, 0.0), (rb - 0.0020, 0.0015), (rb, 0.0060), (rb, 0.1450), (rb - 0.0040, 0.1600), (0.0200, 0.1700),
            (0.0140, 0.1760), (0.0140, 0.1780), (0.0100, 0.1800), (0.0100, 0.1880), (0.0, 0.1880)]
    m.lathe(prof, c0, ddir, 20, "Bike_Bottle", closed=False, ref=V((0, 1, 0)))
    # cage: two wire rings round the bottle and legs down to the plate
    for h, hl in ((0.045, 0.045), (0.125, 0.125)):
        cc = c0 + ddir * h
        m.torus(cc, ddir, rb + 0.0020, 0.0016, 20, 6, "Bike_Alu")
        for yy in (-1, 1):
            a = BB + ddir * (s0 + h) + top * (r_dt + 0.0030) + V((0, yy * 0.006, 0))
            b = cc + top * (-(rb + 0.0020) * math.cos(math.radians(40))) + V((0, yy * (rb + 0.0020) * math.sin(math.radians(40)), 0))
            m.tube([a, b], 0.0016, 6, "Bike_Alu")


def stand_bracket(m):
    ts = STAND_TS
    c = STAND_STAY_PT
    d = (STAND_CS1 - STAND_CS0).normalized()
    r_stay = STAND_STAY_R
    m.cyl(c - d * 0.010, c + d * 0.010, r_stay + 0.0036, 10, "Bike_Alu")
    m.cyl(c + V((0, r_stay + 0.0010, 0)), STAND_HINGE + V((0, -0.0100, 0)), 0.0055, 8, "Bike_Alu")


# --- the steering assembly: fork, headset, quill stem, drop bars, tape, levers, hoods, front caliper ------------------------
Q_LEN = (BAR_Z0 - P_TOP.z) / HEAD_DIR.z
Q_TOP = P_TOP + HEAD_DIR * Q_LEN
BAR_X = Q_TOP.x + 0.100                        # handlebar clamp x
BAR_W = 0.210                                  # half width at the drops (42 cm centre to centre)
BEND_R = 0.070                                 # drop 14 cm
PLAN_R = 0.020
BEND_CX = BAR_X + PLAN_R                       # x of the vertical bend's circle centre
BEND_CZ = BAR_Z0 - BEND_R
HOOD_A0 = 80.0                                 # where the hood sits on the bend (degrees round the bend, 90 = top)
BAR_R, TAPE_R = 0.0115, 0.0145
CROWN_C = P_LOW - HEAD_DIR * 0.026


def bar_left():
    """Centre line of the left half (y >= 0): clamp, tops, plan bend, the round vertical bend, the drop. Returns points and
    index markers."""
    pts, mark = [], {}
    # tops: straight from the centre out to the plan bend
    ys = [0.0, 0.04, 0.075, 0.11, 0.15, 0.19 - 0.0]
    for y in ys:
        pts.append(V((BAR_X, y, BAR_Z0)))
    mark["y075"] = 2
    cy = BAR_W - PLAN_R
    for q in range(1, 6):
        a = math.radians(180.0 - 90.0 * q / 6)
        pts.append(V((BAR_X + PLAN_R + PLAN_R * math.cos(a), cy + PLAN_R * math.sin(a), BAR_Z0)))
    mark["arc"] = {}
    for a in range(90, -91, -5):
        r = math.radians(a)
        pts.append(V((BEND_CX + BEND_R * math.cos(r), BAR_W, BEND_CZ + BEND_R * math.sin(r))))
        mark["arc"][a] = len(pts) - 1
    bot = pts[-1]
    ang = math.radians(8.0)
    for q in range(1, 7):
        d = 0.15 * q / 6
        pts.append(bot + V((-math.cos(ang) * d, 0.0, math.sin(ang) * d)))
    mark["end"] = len(pts) - 1
    return pts, mark


def hood_geometry():
    bar, mark = bar_left()
    p = bar[mark["arc"][int(HOOD_A0)]]
    h0 = p + V((0, 0, 0.004))
    sweep_v = V((0.090, 0.0, 0.004))
    return h0, sweep_v


def build_steer():
    m = MB()
    ax = HEAD_DIR
    # headset: lower cup (just under the head tube), upper cup + locknut
    m.cyl(P_LOW, P_LOW - ax * 0.012, 0.0195, 14, "Bike_Chrome")
    m.cyl(P_TOP, P_TOP + ax * 0.014, 0.0195, 14, "Bike_Chrome")
    m.cyl(P_TOP + ax * 0.014, P_TOP + ax * 0.022, 0.0170, 6, "Bike_Chrome")
    # quill + stem neck, clamp barrel on the bar
    m.cyl(P_TOP + ax * 0.022, Q_TOP, 0.0110, 12, "Bike_Alu")
    m.cyl(Q_TOP - ax * 0.010, Q_TOP + ax * 0.014, 0.0150, 12, "Bike_Alu")
    neck_end = V((BAR_X - 0.012, 0.0, BAR_Z0))
    m.cyl(Q_TOP + V((0.004, 0, 0)), neck_end, 0.0115, 12, "Bike_Alu")
    m.cyl(Q_TOP + ax * 0.0140, Q_TOP + ax * 0.0220, 0.0070, 8, "Bike_Chrome")
    m.cyl(V((BAR_X, -0.0200, BAR_Z0)), V((BAR_X, 0.0200, BAR_Z0)), 0.0170, 14, "Bike_Alu")
    m.cyl(V((BAR_X + 0.0160, 0, BAR_Z0)), V((BAR_X + 0.0245, 0, BAR_Z0)), 0.0048, 8, "Bike_Chrome")
    # handlebar: one chrome tube, wider at the clamp
    left, mark = bar_left()
    full = [mirror_y(p) for p in reversed(left[1:])] + left
    rad = []
    for p in full:
        ay = abs(p.y)
        rad.append(0.0130 if ay < 0.045 else lerp(0.0130, BAR_R, min(1.0, (ay - 0.045) / 0.030)))
    m.tube(full, rad, 12, "Bike_Chrome")
    # bar tape (cloth) from 7.5 cm out, up to the hoods, and from under the hoods to the plugs
    for side in (-1, 1):
        L = [V((p.x, side * p.y, p.z)) for p in left]
        i0, i1 = mark["y075"], mark["arc"][75]
        m.tube(L[i0:i1 + 1], TAPE_R, 12, "Bike_Tape")
        j0 = mark["arc"][60]
        seg = L[j0:mark["end"] + 1]
        dend = (seg[-1] - seg[-2]).normalized()
        seg[-1] = seg[-1] - dend * 0.006
        m.tube(seg, TAPE_R, 12, "Bike_Tape")
        m.cyl(seg[-1], seg[-1] + dend * 0.008, 0.0142, 12, "Bike_Black")
    # brake levers: rubber hoods on the bends and the lever blades
    h0, hv = hood_geometry()
    hood_hh = [0.0200, 0.0208, 0.0211, 0.0206, 0.0196, 0.0176, 0.0142, 0.0102]
    for side in (-1, 1):
        pts = [V((h0.x + hv.x * t, side * BAR_W, h0.z + hv.z * t + 0.003 * math.sin(math.pi * t))) for t in [i / 7 for i in range(8)]]
        prof = [(1.08 * math.cos(2 * math.pi * k / 14), math.sin(2 * math.pi * k / 14)) for k in range(14)]
        m.sweep(pts, prof, "Bike_Hood", scale=hood_hh, ref=V((0, 1, 0)), fixed=True)
        # lever blade: a flat alu strap round the front of the bend
        blade = []
        for a in range(38, -58, -8):
            r = math.radians(a)
            blade.append(V((BEND_CX + 0.105 * math.cos(r), side * BAR_W, BEND_CZ + 0.105 * math.sin(r))))
        bprof = [(-0.0055, -0.0016), (0.0055, -0.0016), (0.0055, 0.0016), (-0.0055, 0.0016)]
        m.sweep(blade, bprof, "Bike_Alu", ref=V((0, 1, 0)), fixed=True)
        m.sphere(blade[-1], 0.0065, 6, "Bike_Black", squash=0.8)
    # fork: crown (chrome), blades (paint) with chrome sleeves at the crown and chrome tips with dropout plates
    m.cyl(CROWN_C + ax * 0.014, CROWN_C - ax * 0.022, 0.0190, 14, "Bike_Chrome")
    m.cyl(CROWN_C - ax * 0.004 + V((0, -0.0300, 0)), CROWN_C - ax * 0.004 + V((0, 0.0300, 0)), 0.0125, 12, "Bike_Chrome")
    d3 = V((math.sin(math.radians(26.0)), 0, -math.cos(math.radians(26.0))))
    for side in (-1, 1):
        top = CROWN_C + V((0, side * 0.0360, 0))
        e_end = AXLE_F + V((0, side * 0.0535, 0)) - d3 * 0.010
        tip_top = e_end - d3 * 0.080
        p1 = top - ax * 0.12
        p2 = tip_top - d3 * 0.10
        cps = [top, V((p1.x, lerp(top.y, tip_top.y, 0.33), p1.z)), V((p2.x, lerp(top.y, tip_top.y, 0.67), p2.z)), tip_top]
        path = bez(*cps, 18) + [e_end]
        rr = [lerp(0.0120, 0.0075, min(1.0, i / 18)) for i in range(19)] + [0.0075]
        m.tube(path, rr, 12, "Bike_Paint")
        m.tube([path[0] + (path[0] - path[1]).normalized() * 0.003] + path[:4], [rr[0] + 0.0032] + [rr[i] + 0.0032 for i in range(4)], 12, "Bike_Chrome")
        m.cyl(e_end - d3 * 0.058, e_end + d3 * 0.003, 0.0098, 12, "Bike_Chrome")
        m.box(AXLE_F + V((0.0, side * 0.0530, 0.008)), (0.0125, 0, 0), (0, 0.0030, 0), (0, 0, 0.0200), "Bike_Chrome")
    # front caliper on the crown
    zc = 0.678
    px = CROWN_C.x + 0.0190 * HEAD_FWD.x + 0.012
    m.cyl(V((CROWN_C.x, 0, zc)), V((px - 0.0105, 0, zc)), 0.0042, 8, "Bike_Chrome")
    caliper(m, V((px, 0, zc)), pad_z=rim_pad_z(px, AXLE_F), front=True)
    return m.to_object("SM_Bike_Steer", P_LOW)


# --- the kickstand --------------------------------------------------------------------------------------------------------
def build_stand():
    m = MB()
    H = STAND_HINGE
    axis = STAND_AXIS
    m.cyl(H - axis * 0.0090, H + axis * 0.0090, 0.0052, 10, "Bike_Alu")
    m.tube([H, STAND_PAD_TOP], 0.0055, 8, "Bike_Alu")
    m.cyl(STAND_PAD_TOP, STAND_FOOT_BOT, 0.0140, 14, "Bike_Black")
    return m.to_object("SM_Bike_Stand", H)


# --- the rack --------------------------------------------------------------------------------------------------------------
RACK_PITCH = 0.34
RACK_SLOTS = [(k - 2) * RACK_PITCH for k in range(5)]


def build_rack():
    m = MB()
    r = 0.0110
    hoops = [(k - 2.5) * RACK_PITCH for k in range(6)]
    prof_pts = []
    xh, zh, rc = 0.30, 0.35, 0.130
    prof_pts.append((-xh, r))
    prof_pts.append((-xh, zh - rc))
    for q in range(1, 6):
        a = math.radians(180 - 90 * q / 6)
        prof_pts.append((-xh + rc + rc * math.cos(a), zh - rc + rc * math.sin(a)))
    for q in range(0, 6):
        a = math.radians(90 - 90 * q / 5)
        prof_pts.append((xh - rc + rc * math.cos(a), zh - rc + rc * math.sin(a)))
    prof_pts.append((xh, r))
    for y in hoops:
        m.tube([V((x, y, z)) for x, z in prof_pts], r, 10, "Rack_Steel")
    L = hoops[-1] + 0.05
    for x in (-xh, xh):
        m.tube([V((x, -L, r)), V((x, L, r))], r, 10, "Rack_Steel")
    return m.to_object("SM_BikeRack", V((0, 0, 0)))


# --- checks --------------------------------------------------------------------------------------------------------------------
def own_checks(objs):
    bad = 0
    for o in objs:
        me = o.data
        bm = bmesh.new()
        bm.from_mesh(me)
        nm = sum(1 for e in bm.edges if len(e.link_faces) != 2)
        zero = sum(1 for f in bm.faces if f.calc_area() < 1e-10)
        uvl = bm.loops.layers.uv.active
        zuv = 0
        for f in bm.faces:
            uv = [l[uvl].uv for l in f.loops]
            a = abs(sum((uv[0] - uv[i]).cross(uv[0] - uv[i + 1]) for i in range(1, len(uv) - 1))) / 2
            if a < 1e-12:
                zuv += 1
        # inside-out islands: signed volume of each connected piece must be positive
        seen, inv = set(), 0
        for f0 in bm.faces:
            if f0.index in seen:
                continue
            stack, vol = [f0], 0.0
            seen.add(f0.index)
            while stack:
                f = stack.pop()
                vs = [v.co for v in f.verts]
                for i in range(1, len(vs) - 1):
                    vol += vs[0].dot(vs[i].cross(vs[i + 1])) / 6.0
                for e in f.edges:
                    for g in e.link_faces:
                        if g.index not in seen:
                            seen.add(g.index)
                            stack.append(g)
            if vol < 0:
                inv += 1
        rot_ok = o.rotation_euler.to_matrix() == Matrix.Identity(3) and tuple(o.scale) == (1.0, 1.0, 1.0)
        print("OWNCHECK %-16s non-manifold edges %d, zero-area faces %d, zero-area UVs %d, inside-out pieces %d, identity xform %s" %
              (o.name, nm, zero, zuv, inv, rot_ok))
        bad += nm + zero + zuv + inv + (0 if rot_ok else 1)
        bm.free()
    return bad


def tri_count(o):
    return sum(len(p.vertices) - 2 for p in o.data.polygons)


# --- assembly + previews -------------------------------------------------------------------------------------------------------
def pose(parts, crank_deg=0.0, steer_deg=0.0, roll_deg=0.0):
    """Put the parts where the game would: crank turned, pedals level at the crank ends, bars turned about the steering axis."""
    for o in parts.values():
        o.rotation_mode = "XYZ"
        o.rotation_euler = (0, 0, 0)
    steer = parts["SM_Bike_Steer"]
    steer.rotation_mode = "AXIS_ANGLE"
    steer.rotation_axis_angle = (math.radians(steer_deg), HEAD_DIR.x, HEAD_DIR.y, HEAD_DIR.z)
    cr = parts["SM_Bike_Crank"]
    cr.rotation_euler = (0, math.radians(crank_deg), 0)
    pr = BB + rot_y(V((CRANK, 0, 0)), crank_deg) + V((0, -Q_PEDAL, 0))
    pl = BB + rot_y(V((-CRANK, 0, 0)), crank_deg) + V((0, Q_PEDAL, 0))
    parts["SM_Bike_PedalR"].location = pr
    parts["SM_Bike_PedalL"].location = pl
    bpy.context.view_layer.update()


def setup_render(res=(1600, 1000)):
    scn = bpy.context.scene
    engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    scn.render.engine = next((e for e in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT") if e in engines), "BLENDER_WORKBENCH")
    scn.render.resolution_x, scn.render.resolution_y = res
    scn.render.film_transparent = False
    world = bpy.data.worlds.new("w")
    world.use_nodes = True
    nt = world.node_tree
    bg = nt.nodes["Background"]
    try:
        sky = nt.nodes.new("ShaderNodeTexSky")
        sky.sky_type = "NISHITA"
        sky.sun_elevation = math.radians(50)
        sky.sun_rotation = math.radians(30)
        nt.links.new(sky.outputs["Color"], bg.inputs["Color"])
        bg.inputs["Strength"].default_value = 0.9
    except Exception:
        bg.inputs[0].default_value = (0.62, 0.68, 0.74, 1)
        bg.inputs[1].default_value = 1.0
    scn.world = world
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 3.0
    sun.rotation_euler = (math.radians(55), math.radians(5), math.radians(40))
    scn.collection.objects.link(sun)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scn.collection.objects.link(cam)
    scn.camera = cam
    cam.data.clip_start, cam.data.clip_end = 0.05, 50
    gm = bpy.data.materials.new("floor")
    gm.use_nodes = True
    gm.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.30, 0.30, 0.29, 1)
    gm.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.9
    me = bpy.data.meshes.new("floor")
    bm = bmesh.new()
    vs = [bm.verts.new(p) for p in ((-6, -6, -0.002), (6, -6, -0.002), (6, 6, -0.002), (-6, 6, -0.002))]
    bm.faces.new(vs)
    bm.to_mesh(me)
    bm.free()
    me.materials.append(gm)
    fl = bpy.data.objects.new("floor", me)
    scn.collection.objects.link(fl)
    return cam, fl


def shot(cam, name, out_dir, loc, look, lens, hide=()):
    scn = bpy.context.scene
    cam.location = V(loc)
    cam.rotation_euler = (V(look) - V(loc)).to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = lens
    for o in bpy.data.objects:
        if o.type == "MESH" and o.name != "floor":
            o.hide_render = o.name in hide
    scn.render.filepath = os.path.join(out_dir, name + ".png")
    bpy.ops.render.render(write_still=True)
    return scn.render.filepath


def previews(parts, rack, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    cam, floor = setup_render()
    names = list(parts)
    out = []
    rk = ("SM_BikeRack",)
    pose(parts)
    mid = (0.10, 0.0, 0.52)
    out.append(shot(cam, "bike_side_right", out_dir, (0.10, -3.4, 0.52), mid, 42, rk))
    out.append(shot(cam, "bike_side_left", out_dir, (0.10, 3.4, 0.52), mid, 42, rk))
    out.append(shot(cam, "bike_three_quarter_front", out_dir, (2.55, -1.95, 1.05), (0.12, 0.0, 0.50), 40, rk))
    out.append(shot(cam, "bike_top", out_dir, (0.10, 0.0, 3.7), (0.10, 0.0, 0.0), 40, rk))
    out.append(shot(cam, "closeup_drivetrain", out_dir, (-0.15, -0.95, 0.42), (-0.15, 0.0, 0.29), 48, rk))
    out.append(shot(cam, "closeup_drivetrain_rear", out_dir, (-0.62, -0.62, 0.34), (-0.40, -0.05, 0.28), 55, rk))
    out.append(shot(cam, "closeup_cockpit", out_dir, (1.15, -0.95, 1.18), (0.50, 0.0, 0.90), 44, rk))
    out.append(shot(cam, "closeup_cockpit_top", out_dir, (0.45, 0.0, 1.75), (0.48, 0.0, 0.9), 50, rk))
    out.append(shot(cam, "closeup_front_hub", out_dir, (0.80, -0.42, 0.46), (0.595, 0.0, 0.334), 50, rk))
    out.append(shot(cam, "closeup_saddle", out_dir, (-0.60, -0.85, 1.22), (-0.21, 0.0, 0.93), 50, rk))
    out.append(shot(cam, "closeup_pedal", out_dir, (0.40, -0.55, 0.30), (0.17, -0.155, 0.27), 60, rk))
    pose(parts, crank_deg=60.0, steer_deg=20.0)
    out.append(shot(cam, "bike_posed_crank60_bars20", out_dir, (2.55, -1.95, 1.05), (0.12, 0.0, 0.50), 40, rk))
    out.append(shot(cam, "bike_posed_top", out_dir, (0.10, 0.0, 3.7), (0.10, 0.0, 0.0), 40, rk))
    # rolled 7 degrees left onto its kickstand: parent everything to an empty at the origin (the X axis through the contact points)
    pose(parts, crank_deg=60.0)
    emp = bpy.data.objects.new("roll", None)
    bpy.context.scene.collection.objects.link(emp)
    for o in parts.values():
        o.parent = emp
    emp.rotation_euler = (math.radians(-PARK_ROLL), 0, 0)
    bpy.context.view_layer.update()
    out.append(shot(cam, "bike_on_kickstand_front", out_dir, (3.4, 0.35, 0.55), (0.0, 0.0, 0.45), 40, rk))
    out.append(shot(cam, "bike_on_kickstand_rear", out_dir, (-2.4, 2.1, 0.60), (-0.15, 0.1, 0.35), 40, rk))
    out.append(shot(cam, "closeup_kickstand", out_dir, (-0.65, 0.9, 0.35), (-0.25, 0.1, 0.18), 45, rk))
    for o in parts.values():
        o.parent = None
    bpy.data.objects.remove(emp, do_unlink=True)
    # the stand swung up about its hinge (the angle and axis written to bike.json): it must lie along the left chain stay
    pose(parts)
    st = parts["SM_Bike_Stand"]
    st.rotation_mode = "AXIS_ANGLE"
    st.rotation_axis_angle = (math.radians(STAND_UP_ANGLE), STAND_AXIS.x, STAND_AXIS.y, STAND_AXIS.z)
    bpy.context.view_layer.update()
    out.append(shot(cam, "closeup_kickstand_folded", out_dir, (-0.45, 0.62, 0.55), (-0.28, 0.06, 0.31), 50, rk))
    out.append(shot(cam, "bike_stand_folded_left", out_dir, (-0.25, 2.2, 0.45), (-0.25, 0.0, 0.30), 50, rk))
    pose(parts)
    # the rack with a wheel in a slot
    rack.hide_render = False
    wf = parts["SM_Bike_WheelF"]
    for o in bpy.data.objects:
        if o.type == "MESH" and o.name != "floor":
            o.hide_render = o.name not in ("SM_BikeRack", "SM_Bike_WheelF")
    wf.location = V((0.0, RACK_SLOTS[1], TYRE_R))
    bpy.context.view_layer.update()
    scn = bpy.context.scene
    cam.location = V((1.9, -2.4, 1.0))
    cam.rotation_euler = (V((0, 0, 0.2)) - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = 38
    scn.render.filepath = os.path.join(out_dir, "rack_with_wheel.png")
    bpy.ops.render.render(write_still=True)
    out.append(scn.render.filepath)
    wf.location = AXLE_F
    return out


# --- kickstand geometry (hinge on the left chain stay behind the BB; modelled DOWN) -------------------------------------------------
PARK_ROLL = 7.0
STAND_X = -0.165
STAND_CS0 = V((-0.025, 0.0245, BB_Z))
STAND_CS1 = V((-0.392, 0.0682, AXLE_Z + 0.004))
STAND_TS = (STAND_X - STAND_CS0.x) / (STAND_CS1.x - STAND_CS0.x)
STAND_STAY_PT = STAND_CS0 + (STAND_CS1 - STAND_CS0) * STAND_TS
STAND_STAY_R = lerp(0.0095, 0.0055, STAND_TS)
STAND_HINGE = STAND_STAY_PT + V((0, 0.0345, 0))
_r7 = math.radians(PARK_ROLL)
STAND_FOOT_BOT = V((STAND_HINGE.x - 0.045, 0.150, 0.150 * math.tan(_r7)))     # the foot's underside centre: on the ground when rolled 7 deg left
_DN = V((0, math.sin(_r7), -math.cos(_r7)))                                    # world "down" in the bike's frame when rolled
STAND_PAD_TOP = STAND_FOOT_BOT - _DN * 0.008
STAND_DIR_DOWN = (STAND_PAD_TOP - STAND_HINGE).normalized()
STAND_DIR_UP = (STAND_CS1 - STAND_CS0).normalized()                           # folded: along the chain stay, pointing back
STAND_AXIS = STAND_DIR_DOWN.cross(STAND_DIR_UP).normalized()
STAND_UP_ANGLE = math.degrees(math.acos(max(-1.0, min(1.0, STAND_DIR_DOWN.dot(STAND_DIR_UP)))))


def ue(p, nd=3):
    return [round(p.x * 100, nd) + 0.0, round(-p.y * 100, nd) + 0.0, round(p.z * 100, nd) + 0.0]


def ue_dir(p, nd=5):
    return [round(p.x, nd) + 0.0, round(-p.y, nd) + 0.0, round(p.z, nd) + 0.0]


def world_verts(o):
    mw = o.matrix_world
    return [mw @ v.co for v in o.data.vertices]


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    prev = argv[argv.index("--preview") + 1] if "--preview" in argv else None
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    frame, saddle_geo = build_frame()
    steer = build_steer()
    wheel_f = build_wheel(True)
    wheel_r = build_wheel(False)
    crank = build_crank()
    pedal_r = build_pedal(True)
    pedal_l = build_pedal(False)
    chain = build_chain()
    stand = build_stand()
    rack = build_rack()
    bike = [frame, steer, wheel_f, wheel_r, crank, pedal_r, pedal_l, chain, stand]
    parts = {o.name: o for o in bike}
    pose(parts)
    # --- checks -----------------------------------------------------------------------------------------------------------
    sys.path.insert(0, os.path.join(REPO, "blender", "railway"))
    import kit_lint
    problems = kit_lint.lint(bike, "bike")
    problems += kit_lint.lint([rack], "rack")
    bad = own_checks(bike + [rack])
    zmin_f = min(v.z for v in world_verts(wheel_f))
    zmin_r = min(v.z for v in world_verts(wheel_r))
    print("TYRES lowest point: front z=%.7f  rear z=%.7f" % (zmin_f, zmin_r))
    roll = Matrix.Rotation(-_r7, 4, "X")
    z_stand = min((roll @ v).z for v in world_verts(stand))
    z_all = min((roll @ v).z for o in bike if o is not stand for v in world_verts(o))
    print("STAND rolled %.1f deg left: foot lowest z=%.6f; everything else lowest z=%.6f (tyres touch within the section's curvature)" % (PARK_ROLL, z_stand, z_all))
    tris = {o.name: tri_count(o) for o in bike}
    total = sum(tris.values())
    for k, v in tris.items():
        print("TRIS %-18s %6d" % (k, v))
    print("TRIS %-18s %6d" % ("TOTAL (no rack)", total))
    print("TRIS %-18s %6d" % (rack.name, tri_count(rack)))
    print("CHAIN loop %.4f m = %d pitches, cage reach %.4f" % (CHAIN_LEN, CHAIN_LINKS, CAGE_LEN))
    print("LINT total problems: %d, own-check problems: %d" % (len(problems), bad))
    shots = []
    if prev:
        shots = previews(parts, rack, prev)
        for p in shots:
            print("SHOT", p)
        pose(parts)
    # --- export ---------------------------------------------------------------------------------------------------------------
    saved = {o.name: o.location.copy() for o in bike + [rack]}
    os.makedirs(os.path.dirname(OUT_GLB), exist_ok=True)
    os.makedirs(os.path.dirname(OUT_JSON), exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    for o in bike + [rack]:
        o.location = (0, 0, 0)
        o.rotation_euler = (0, 0, 0)
        o.select_set(True)
    bpy.ops.export_scene.gltf(filepath=OUT_GLB, use_selection=True, export_format="GLB", export_yup=True, export_apply=True,
                              export_image_format="NONE")
    for o in bike + [rack]:
        o.location = saved[o.name]
    pose(parts)
    # --- bike.json -------------------------------------------------------------------------------------------------------------
    h0, hv = hood_geometry()
    hh_mid = 0.5 * (0.0206 + 0.0196)
    hood_pt = {}
    for side, key in ((1, "l"), (-1, "r")):
        hood_pt[key] = V((h0.x + hv.x * 0.5, side * BAR_W, h0.z + hv.z * 0.5 + 0.003 + hh_mid))
    grip = hv.normalized()
    bot = V((BEND_CX, BAR_W, BAR_Z0 - 2 * BEND_R))
    drop_dir = V((-math.cos(math.radians(8.0)), 0, math.sin(math.radians(8.0))))
    drop_pt = bot + drop_dir * 0.05
    allv = [v for o in bike for v in world_verts(o)]
    bmin = V((min(v.x for v in allv), min(v.y for v in allv), min(v.z for v in allv)))
    bmax = V((max(v.x for v in allv), max(v.y for v in allv), max(v.z for v in allv)))
    ue_min = [round(bmin.x * 100, 2), round(-bmax.y * 100, 2), round(bmin.z * 100, 2)]
    ue_max = [round(bmax.x * 100, 2), round(-bmin.y * 100, 2), round(bmax.z * 100, 2)]
    pv = {o.name: ue(o.location) for o in bike}
    pv[rack.name] = ue(V((0, 0, 0)))
    data = {
        "version": 1,
        "units": "cm, Unreal frame: X forward, Y right, Z up, origin on the ground under the bottom bracket",
        "_doc": "Made by blender/bikes/build_bike.py. Every part's mesh is relative to its pivot (pivot given here at rest: crank angle 0, bars straight, stand down).",
        "parts": {k: {"pivot": v} for k, v in pv.items()},
        "wheel_radius": TYRE_R * 100,
        "axle_front": ue(AXLE_F),
        "axle_rear": ue(AXLE_R),
        "bb": ue(BB),
        "crank_length": CRANK * 100,
        "crank_note": "SM_Bike_Crank turns about the Y axis through bb; at angle 0 the right arm points +X, the left -X. Pedalling turns the right arm's end down (-Z) as it passes forward. The pedals are separate parts that stay level at the crank ends.",
        "pedal_r": {"y": round(Q_PEDAL * 100, 3)},
        "pedal_l": {"y": round(-Q_PEDAL * 100, 3)},
        "steer_axis": {"point": ue(P_LOW), "dir": ue_dir(HEAD_DIR)},
        "saddle_top": ue(saddle_geo["sit"]),
        "saddle_nose": ue(saddle_geo["nose"]),
        "hoods": {"l": ue(hood_pt["l"]), "r": ue(hood_pt["r"]), "grip_axis_l": ue_dir(grip), "grip_axis_r": ue_dir(grip)},
        "hood_radius": round(hh_mid * 100, 2),
        "bar_tops": {"l": ue(V((BAR_X, 0.100, BAR_Z0))), "r": ue(V((BAR_X, -0.100, BAR_Z0))), "radius": round(TAPE_R * 100, 2)},
        "drops": {"l": ue(drop_pt), "r": ue(mirror_y(drop_pt)), "radius": round(TAPE_R * 100, 2)},
        "stand": {
            "hinge": ue(STAND_HINGE),
            "axis": None,
            "up_angle_deg": round(STAND_UP_ANGLE, 3),
            "park_roll_deg": PARK_ROLL,
            "leg_dir_down": ue_dir(STAND_DIR_DOWN),
            "leg_dir_up": ue_dir(STAND_DIR_UP),
            "foot": ue(STAND_FOOT_BOT),
            "note": "Rotate SM_Bike_Stand about its pivot (the hinge) by +up_angle_deg about 'axis' with FQuat(axis, angle) to fold it up along the left chain stay; rolling the bike park_roll_deg to the LEFT about the X axis through the two tyre contact points puts the foot on the ground.",
        },
        "chain": {"pitch": PITCH * 100, "links": CHAIN_LINKS, "length_m": round(CHAIN_LEN, 5), "plane_y": round(-Y_CHAIN * 100, 3)},
        "rack": {"slots": [[0.0, round(-y * 100, 2), 0.0] for y in RACK_SLOTS], "pitch": RACK_PITCH * 100, "length": 170.0},
        "bounds": {"min": ue_min, "max": ue_max},
    }
    # the stand axis in UE components: the plain-arithmetic cross product of the UE-frame leg directions (what FQuat wants)
    dn_ue, up_ue = V(ue_dir(STAND_DIR_DOWN)), V(ue_dir(STAND_DIR_UP))
    ax_ue = dn_ue.cross(up_ue).normalized()
    data["stand"]["axis"] = [round(c, 5) for c in ax_ue]
    with open(OUT_JSON, "w") as f:
        json.dump(data, f, indent=1)
    print("BIKE: %d parts + rack, %d tris -> %s" % (len(bike), total, OUT_GLB))
    print(json.dumps(data, indent=1))
    if "--strict" in argv and (problems or bad):
        raise SystemExit("BIKE: glitch findings")


main()
