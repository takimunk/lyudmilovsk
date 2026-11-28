"""The diner's seats (docs/diner.md): the counter's stools, the six booths along the street windows (two benches, a table, the
jukebox selector) and the curved bench with its two round tables in the nose. Every seat the hero or a customer can sit on is
listed by build() for layout.json ("hero_seats").

  build(parts) adds the geometry to parts["inside"] (benches, tables: collision), parts["stools"] (no collision),
  parts["small"] (small fittings, no collision) and returns {"hero_seats": [...]}.

The quality rebuild (2026-11): everything is built from a few generators on bmesh --
  sweep()   a closed 2D profile swept along a path (rings of vertices, own metre UVs, auto-oriented); with the shrinking of the
            profile near both ends of the path it gives a rounded block (cushions, mahogany frames, rails) straight or curved;
  tuft()    a vertically channel-tufted back cushion (real ridges, pinched seams, rounded top and bottom);
  path_tube() piping and trims along a polyline;
  bench()   one bench (toe-kick, base, partition, cap rail, seat cushion with a front roll and welt, tufted back) built through a
            place(u, z, s) function: u from the back inwards, z up, s along the bench; the booth benches are straight, the nose
            bench is the same thing curved round the nose.
"""
import math
import os
import sys

import bmesh  # noqa: F401
from mathutils import Matrix, Vector  # noqa: F401

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import diner_common as C  # noqa: E402,F401
from diner_common import M, bx, box, rbox, tube, lathe, sphere, prism  # noqa: E402,F401
from plan import *  # noqa: E402,F401,F403
from building_util import oq, ybox  # noqa: E402,F401

TABLE_Z = 0.765                  # the table tops' surface
SEAT_Z = 0.46                    # the benches' cushion top
STOOL_TOP = STOOL_SEAT + 0.045   # the stools' cushion top
BENCH_DEPTH = 0.56               # from the back to the cushion's front edge


# ---- generators -------------------------------------------------------------------------------------------------------------

def _sv(faces):
    """Signed volume of a closed set of faces (positive when the normals point outwards)."""
    v = 0.0
    for f in faces:
        vs = [lp.vert.co for lp in f.loops]
        for i in range(1, len(vs) - 1):
            v += vs[0].dot(vs[i].cross(vs[i + 1]))
    return v


def sweep(bm, prof, mi, ts, place, along=False, loop=False, caps=True, vertex_v=False):
    """Sweeps the closed profile prof [(a, b), ...] (metres) through the path parameters ts: ring k has the vertices
    place(a, b, ts[k]). Own metre UVs: round the profile and along the path (along=True: u along the path, v round the
    profile: grain and ribs that run along it). loop: the last ring joins the first (no caps). Returns the new faces."""
    C.ensure_uv(bm)
    n = len(prof)
    nr = len(ts)
    rings = [[bm.verts.new(place(a, b, t)) for a, b in prof] for t in ts]
    # u: round the profile (its first ring's edge lengths); v: along the path by the mean step of the ring's vertices (so a
    # rounded end that moves the profile inwards as it advances does not stretch the texture, a curved path does not drift)
    s_ = [0.0]
    for i in range(n):
        p_, q_ = prof[i], prof[(i + 1) % n]
        s_.append(s_[-1] + math.hypot(q_[0] - p_[0], q_[1] - p_[1]))
    us = [s_] * nr
    cum = [0.0]
    for k in range(1, nr + (1 if loop else 0)):
        ra, rb = rings[k - 1], rings[k % nr]
        cum.append(cum[-1] + sum((rb[i].co - ra[i].co).length for i in range(n)) / n)
    vs = [[c] * n for c in cum]
    vo = None
    if vertex_v:             # straight tracks (the tufted back): the flat back faces measure each vertex's own way
        vo = [[0.0] * n for _ in range(nr)]
        for k in range(1, nr):
            for i in range(n):
                vo[k][i] = vo[k - 1][i] + (rings[k][i].co - rings[k - 1][i].co).length
    vh = [[(vs[k][i] + vo[k][i]) / 2 for i in range(n)] for k in range(nr)] if vo is not None else None
    last = vs[-1] if loop else None
    new = []
    for k in range(nr - (0 if loop else 1)):
        r0, r1 = rings[k], rings[(k + 1) % nr]
        k1 = (k + 1) % nr
        for i in range(n):
            j = (i + 1) % n
            f = bm.faces.new((r0[i], r0[j], r1[j], r1[i]))
            f.material_index = mi
            src = vh if vo is not None else vs
            va = (src[k][i], src[k][j], (last[j] if (loop and k1 == 0) else src[k1][j]), (last[i] if (loop and k1 == 0) else src[k1][i]))
            uu = (us[k][i], us[k][i + 1], us[k1][i + 1], us[k1][i])
            uv = tuple(zip(uu, va))
            if along:
                uv = tuple((b, a) for a, b in uv)
            C.own_uv(bm, f, uv)
            new.append(f)
    if caps and not loop:
        for ring in (rings[0][::-1], rings[-1]):
            f = bm.faces.new(ring)
            f.material_index = mi
            new.append(f)
    if _sv(new) < 0:
        bmesh.ops.reverse_faces(bm, faces=new)
    return new


def rrect(a0, a1, b0, b1, rad=(0, 0, 0, 0), n=3):
    """A rounded rectangle, counter-clockwise from the bottom-left corner; rad: radii (a0b0, a1b0, a1b1, a0b1)."""
    out = []
    for (ca, cb), r, st, sa, sb in (((a0, b0), rad[0], 180, 1, 1), ((a1, b0), rad[1], 270, -1, 1),
                                    ((a1, b1), rad[2], 0, -1, -1), ((a0, b1), rad[3], 90, 1, -1)):
        if r <= 0:
            out.append((ca, cb))
            continue
        ctr = (ca + sa * r, cb + sb * r)
        for k in range(n + 1):
            ang = math.radians(st + 90.0 * k / n)
            out.append((ctr[0] + r * math.cos(ang), ctr[1] + r * math.sin(ang)))
    return out


def ends(L, r, step=None, n=3):
    """Path parameters 0..L with rings that round both ends (quarter circles of radius r, n steps) and, when step is given,
    intermediate rings about step apart."""
    k = [r * (1 - math.cos(math.pi / 2 * i / n)) for i in range(n + 1)]
    mid = []
    if step:
        m = max(1, int(round((L - 2 * r) / step)))
        mid = [r + (L - 2 * r) * i / m for i in range(1, m)]
    return k + mid + [L - x for x in reversed(k)]


def inset_of(t, L, r):
    d = min(t, L - t)
    if d >= r:
        return 0.0
    c = 1 - d / r
    return r * (1 - math.sqrt(max(0.0, 1 - c * c)))


def bar(bm, mi, prof, place, s0, s1, r_end, step=None, n_end=3, along=True):
    """A rounded block: the (u, z) profile prof swept along s0..s1 of place(u, z, s), its ends rounded by radius r_end."""
    us, zs = [p[0] for p in prof], [p[1] for p in prof]
    cu, cz = (min(us) + max(us)) / 2, (min(zs) + max(zs)) / 2
    hu, hz = (max(us) - min(us)) / 2, (max(zs) - min(zs)) / 2
    L = s1 - s0

    def pl(a, b, t):
        i = inset_of(t, L, r_end)
        return place(cu + (a - cu) * (1 - i / hu), cz + (b - cz) * (1 - i / hz), s0 + t)
    return sweep(bm, prof, mi, ends(L, r_end, step, n_end), pl, along=along)


def path_tube(bm, pts, r, mi, seg=6, loop=False, ref=(0, 0, 1), phase=0.0):
    """Piping / a trim: a tube of radius r along the polyline pts."""
    pts = [Vector(p) for p in pts]
    n = len(pts)
    fr = []
    for i in range(n):
        t = (pts[(i + 1) % n] - pts[i - 1]) if loop else (pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)])
        t.normalize()
        rv = Vector(ref)
        nv = rv - t * rv.dot(t)
        if nv.length < 1e-4:
            nv = Vector((1, 0, 0)) - t * t.x
        nv.normalize()
        fr.append((nv, t.cross(nv)))
    prof = [(r * math.cos(2 * math.pi * k / seg + phase), r * math.sin(2 * math.pi * k / seg + phase)) for k in range(seg)]
    return sweep(bm, prof, mi, list(range(n)), lambda a, b, t: pts[int(t)] + fr[int(t)][0] * a + fr[int(t)][1] * b,
                 along=True, loop=loop, caps=not loop)


def densify(pts, maxlen, closed=True):
    out = []
    n = len(pts)
    for i in range(n if closed else n - 1):
        p, q = Vector(pts[i]), Vector(pts[(i + 1) % n])
        m = max(1, int(math.ceil((q - p).length / maxlen)))
        out += [tuple(p + (q - p) * (k / m)) for k in range(m)]
    if not closed:
        out.append(tuple(pts[-1]))
    return out


def tuft(bm, mi, place, a0, W, z0, z1, n_ch, n_pt=5, T=0.065, T0=0.014, rb=0.03, rt=0.06, back_step=None, u0=0.085):
    """A back cushion with vertical channels: lateral a0..a0+W (along s), z0..z1, n_ch channels bulging T over a back plane
    at u0 and pinched to T0 at the seams; the bottom and the top roll away (radii rb, rt)."""
    w = W / n_ch
    front = []
    for c in range(n_ch):
        for p in range(n_pt):
            f = p / n_pt
            front.append((a0 + c * w + f * w, T0 + (T - T0) * math.sin(math.pi * f) ** 0.55))
    front.append((a0 + W, T0))
    back = [(a0 + W, 0.0)]
    if back_step:
        m = int(math.ceil(W / back_step))
        back += [(a0 + W * i / m, 0.0) for i in range(m - 1, 0, -1)]
    back.append((a0, 0.0))
    prof = front + back
    zl = [(z0 + rb - rb * math.sin(math.pi / 2 * k / 2), math.cos(math.pi / 2 * k / 2)) for k in range(2, -1, -1)]
    zl += [(z1 - rt * (1 - math.sin(math.pi / 2 * 0.85 * k / 4)) / (1 - math.sin(math.pi / 2 * 0.85)) * 0 - rt + rt * math.sin(math.pi / 2 * 0.85 * k / 4) + rt * 0.0, math.cos(math.pi / 2 * 0.85 * k / 4)) for k in range(0, 5)]

    def pl(a, b, k):
        z, sc = zl[int(k)]
        bb = b if b <= 0 else T0 + (b - T0) * sc
        return place(u0 + bb, z, a)
    return sweep(bm, prof, mi, list(range(len(zl))), pl, vertex_v=True)


def bench(bm, small, place, L, hole=0.0, step=None, nose=False):
    """One bench along s = 0..L: toe-kick, mahogany base, partition at the back, cap rail, seat cushion with a front roll and
    welt, tufted back cushion. Returns the front-edge u."""
    mah, vin, dark, chrome = M("DN_Mahogany"), M("DN_Vinyl"), M("DN_Dark"), M("DN_Chrome")
    # the mahogany below the seat
    bar(bm, dark, rrect(0.087, 0.49, 0.0015, 0.100), place, 0.022, L - 0.045, 0.004, step, 2)
    bar(bm, mah, rrect(0.087, 0.535, 0.098, 0.372, (0.006, 0.012, 0.012, 0.006), 2), place, 0.016, L - 0.025, 0.012, step, 2)
    # the back: partition, cap rail and a chrome line on the rail's face
    bar(bm, mah, rrect(0.004, 0.090, 0.0015, 1.075, (0.008, 0.008, 0.008, 0.008), 2), place, 0.002, L - 0.004, 0.010, step, 2)
    bar(bm, mah, rrect(0.003, 0.158, 1.066, 1.124, (0.010, 0.020, 0.020, 0.010), 3 if not nose else 2), place, 0.0, L, 0.016, step, 3)
    ln = densify([(0.1585, 0.032), (0.1585, L - 0.032)], 0.2, closed=False)
    path_tube(bm, [place(u, 1.095, s) for u, s in ln], 0.0045, chrome, seg=6, phase=math.pi / 6)
    # the seat cushion
    bar(bm, vin, rrect(0.087, BENCH_DEPTH, 0.368, SEAT_Z, (0.010, 0.030, 0.042, 0.008), 4 if not nose else 3), place, 0.003, L, 0.02,
        step, 3, along=False)
    loop = densify(rrect(0.185, 0.50, 0.04, L - 0.03, (0.035,) * 4, 3), 0.2)
    path_tube(bm, [place(u, SEAT_Z + 0.001, s) for u, s in loop], 0.0055, vin, seg=6, loop=True)
    # the tufted back
    W = L - 0.016
    nch = max(1, int(round(W / (0.134 if not nose else 0.17))))
    tuft(bm, vin, place, 0.006, W, 0.455, 1.070, nch, 5 if not nose else 4, back_step=(0.2 if step else None))
    return BENCH_DEPTH


def aluminium_loop(bm, poly, z0, z1, mi):
    """A ribbed aluminium band round a table top whose outer edge is poly (counter-clockwise (x, y)): the band is 12 mm wide
    (2 mm of it under the top), u along the band, v across it, 3 mm proud of the top's surface z1."""
    n = len(poly)
    off = []
    for i in range(n):
        p, q, r = Vector(poly[i - 1]), Vector(poly[i]), Vector(poly[(i + 1) % n])
        e1, e2 = (q - p).normalized(), (r - q).normalized()
        n1, n2 = Vector((e1.y, -e1.x)), Vector((e2.y, -e2.x))
        off.append((n1 + n2) / (1 + n1.dot(n2)))
    core = [Vector(poly[i]) - off[i] * 0.010 for i in range(n)]
    zi, zt = z1 - 0.0275, z1 + 0.003
    prof = [(0.0, zi), (0.0045, z0), (0.0075, z0), (0.0095, z0 + 0.0012), (0.0105, z0 + 0.003), (0.0105, zt - 0.003),
            (0.0095, zt - 0.0012), (0.0075, zt), (0.0045, zt), (0.0, z1)]          # the lip stands 3 mm over the top, a chamfer to it
    sweep(bm, prof, mi, list(range(n)),
          lambda a, b, k: Vector((core[int(k)].x + off[int(k)].x * a, core[int(k)].y + off[int(k)].y * a, b)),
          along=True, loop=True, caps=False)
    return [tuple(c) for c in core]


def table(bm, poly, cx, cy):
    """A diner table: Formica top in a ribbed aluminium edge on a chrome pedestal with a cast flared foot; poly: the outer
    outline of the top (ccw)."""
    z0, z1 = TABLE_Z - 0.0275, TABLE_Z
    core = aluminium_loop(bm, poly, z0 - 0.0035, z1, M("DN_Aluminum"))
    bm.faces.new([bm.verts.new((x, y, z1)) for x, y in core]).material_index = M("DN_Formica")           # the sides sit inside the band
    bm.faces.new([bm.verts.new((x, y, z0)) for x, y in core][::-1]).material_index = M("DN_Formica")
    lathe(bm, [(0.20, 0.002), (0.20, 0.010), (0.186, 0.020), (0.14, 0.032), (0.09, 0.050), (0.058, 0.090), (0.040, 0.150),
               (0.036, 0.220), (0.036, 0.690), (0.050, 0.700), (0.090, 0.714), (0.110, 0.722), (0.110, z0 - 0.002)],
          M("DN_Chrome"), seg=20, c=(cx, cy, 0.0), cap0=False)


def jukebox(bm, cx, y0, z0=0.795, z1=0.945):
    """A Seeburg wall-o-matic on the wall y0 (it protrudes into +y): chrome case, a deep window with title cards under a glass
    pane (the layers are 12+ mm apart, the owner's z-fight rule), a control block with the coin slot and push buttons."""
    ch, pa, da, rd = M("DN_Chrome"), M("DN_Paper"), M("DN_Dark"), M("DN_RedPlastic")
    rbox(bm, ch, cx - 0.15, cx + 0.15, y0 + 0.003, y0 + 0.034, z0, z1, r=0.012, seg=3)
    rbox(bm, ch, cx + 0.052, cx + 0.140, y0 + 0.020, y0 + 0.062, z0 + 0.012, z1 - 0.012, r=0.007, seg=2)
    wx0, wx1, wz0, wz1 = cx - 0.135, cx + 0.035, z0 + 0.020, z1 - 0.020
    bx(bm, da, wx0, wx1, y0 + 0.030, y0 + 0.047, wz0, wz1)                       # the dark back of the window
    nrow = 5
    rh = (wz1 - wz0 - 0.012) / nrow
    for r_ in range(nrow):
        zr = wz0 + 0.006 + r_ * rh
        for c in range(3):
            xc = wx0 + 0.008 + c * 0.052
            bx(bm, rd if (r_ * 2 + c) % 5 == 0 else pa, xc, xc + 0.049, y0 + 0.044, y0 + 0.062, zr + 0.002, zr + rh - 0.002)  # a title card
    bx(bm, M("DN_Glass"), wx0, wx1, y0 + 0.078, y0 + 0.081, wz0, wz1)             # the glass pane
    for x0, x1, a, b in ((wx0 - 0.014, wx1 + 0.014, wz1, wz1 + 0.014), (wx0 - 0.014, wx1 + 0.014, wz0 - 0.014, wz0)):  # bezel
        bx(bm, ch, x0, x1, y0 + 0.030, y0 + 0.090, a, b)
    for x0, x1 in ((wx0 - 0.014, wx0), (wx1, wx1 + 0.014)):
        bx(bm, ch, x0, x1, y0 + 0.030, y0 + 0.090, wz0 - 0.014, wz1 + 0.014)
    # the coin slot and the push buttons
    rx, yb = cx + 0.096, y0 + 0.062
    bx(bm, da, rx - 0.014, rx + 0.014, yb - 0.001, yb + 0.002, z1 - 0.036, z1 - 0.031)
    for i, zz in enumerate((z1 - 0.066, z1 - 0.089, z1 - 0.112)):
        for j, xx in enumerate((rx - 0.022, rx + 0.022)):
            tube(bm, (xx, yb - 0.001, zz), (xx, yb + 0.009, zz), 0.0065, rd if (i + j) % 2 == 0 else M("DN_Bakelite"), seg=8)


def hook(bm, x, y, z):
    """A chrome double coat hook on the partition's aisle end at (x, y); it points into +y."""
    ch = M("DN_Chrome")
    tube(bm, (x, y - 0.001, z), (x, y + 0.006, z), 0.021, ch, seg=10)
    tube(bm, (x, y + 0.004, z), (x, y + 0.050, z), 0.0065, ch, seg=6, caps=False)
    tube(bm, (x, y + 0.050, z), (x, y + 0.078, z + 0.034), 0.0065, ch, seg=6, caps=False)
    sphere(bm, (x, y + 0.078, z + 0.034), 0.0105, ch, seg=8, rings=4)


# ---- the stools -------------------------------------------------------------------------------------------------------------

def stool(bm, x, y):
    ch = M("DN_Chrome")
    c = (x, y, 0.0)
    # the bolted flange, the column with its step and the swivel cup
    lathe(bm, [(0.172, 0.002), (0.172, 0.011), (0.165, 0.014), (0.115, 0.014), (0.075, 0.034), (0.050, 0.075), (0.040, 0.130),
               (0.040, 0.420), (0.034, 0.426), (0.034, 0.600), (0.042, 0.622), (0.060, 0.646), (0.082, 0.668)],
          ch, seg=16, c=c, cap0=False, cap1=False)
    for k in range(3):
        a = 2 * math.pi * (k + 0.25) / 3
        lathe(bm, [(0.0085, 0.013), (0.0085, 0.0165), (0.0065, 0.0200), (0.0030, 0.0218)], ch, seg=8,
              c=(x + 0.138 * math.cos(a), y + 0.138 * math.sin(a), 0.0), cap0=False)
    # the ribbed skirt under the seat and the chrome band round the cushion
    p0, p1 = Vector((0.05, 0.636)), Vector((0.19, 0.700))
    d = (p1 - p0)
    ln = d.length
    d /= ln
    nrm = Vector((d.y, -d.x))
    prof = [tuple(p0)]
    for t in (0.72,):
        for dt, h in ((-0.06, 0.0), (0.0, 0.0075), (0.06, 0.0)):
            q = p0 + d * ln * (t + dt) + nrm * h
            prof.append(tuple(q))
    prof += [tuple(p1), (0.199, 0.707), (0.199, 0.719), (0.2025, 0.722), (0.199, 0.725), (0.199, 0.742), (0.1965, 0.745)]
    lathe(bm, prof, ch, seg=24, c=c, cap0=False, cap1=False)
    # the thick red vinyl cushion with its rolled edge (the roll covers the band's top)
    lathe(bm, [(0.196, 0.7445), (0.205, 0.749), (0.2095, 0.760), (0.2095, 0.778), (0.2065, 0.791), (0.198, 0.7995), (0.178, 0.8030),
               (0.12, 0.8042), (0.07, STOOL_TOP), (0.05, STOOL_TOP)], M("DN_Vinyl"), seg=28, c=c, cap0=False)


# ---- booths -----------------------------------------------------------------------------------------------------------------

def booth(parts, xa, xb, seats):
    bi, bs = parts["inside"], parts["small"]
    y0 = YI_F + 0.003
    L = BOOTH_Y - y0 - 0.002                     # the benches' length along the wall (s = 0 at the wall)
    for xs, sg in ((xa, 1), (xb, -1)):
        place = lambda u, z, s, xs=xs, sg=sg: Vector((xs + sg * u, y0 + s, z))  # noqa: E731
        bench(bi, bs, place, L)
        if sg == 1:
            hook(bs, xs + 0.047 * sg, y0 + L - 0.004, 1.0)
    tx0, tx1 = xa + 0.62, xb - 0.62
    ty0, ty1 = YI_F + 0.006, BOOTH_Y - 0.06
    cx, cy = (tx0 + tx1) / 2, (ty0 + ty1) / 2
    table(bi, rrect(tx0, tx1, ty0, ty1, (0.05,) * 4, 4), cx, cy)
    jukebox(bs, cx, YI_F)
    for xs, sg in ((xa, 1), (xb, -1)):
        for f in (0.25, 0.75):
            s = L * f
            seats.append({"kind": "booth", "at": [round(xs + sg * BENCH_DEPTH, 3), round(y0 + s, 3), 0.0], "yaw": 0.0 if sg > 0 else 180.0,
                          "seat_z": SEAT_Z,
                          "table": [round(tx0 + 0.25 if sg > 0 else tx1 - 0.25, 3), round(y0 + s, 3), TABLE_Z]})


# ---- the nose ---------------------------------------------------------------------------------------------------------------

NOSE_RW = R * math.cos(math.pi / (2 * NOSE_SEG)) - TH - 0.003        # the inner wall's flat faces are nearest to the centre here
NOSE_A0 = math.radians(-60.0)
NOSE_RREF = NOSE_RW - 0.25
NOSE_L = NOSE_RREF * math.radians(120.0)


def nose_place(u, z, s):
    r = NOSE_RW - u
    a = NOSE_A0 + s / NOSE_RREF
    return Vector((XE + r * math.cos(a), r * math.sin(a), z))


def nose_bench(parts, seats):
    bench(parts["inside"], parts["small"], nose_place, NOSE_L, step=0.2, nose=True)
    for x, y in NOSE_TABLES:
        table(parts["inside"], [(x + 0.34 * math.cos(2 * math.pi * k / 28), y + 0.34 * math.sin(2 * math.pi * k / 28)) for k in range(28)], x, y)
    for deg in (-50, -30, -10, 10, 30, 50):
        a = math.radians(deg)
        s = (a - NOSE_A0) * NOSE_RREF
        p = nose_place(BENCH_DEPTH, 0.0, s)
        tc = min(NOSE_TABLES, key=lambda t: (t[0] - p.x) ** 2 + (t[1] - p.y) ** 2)
        d = Vector((p.x - tc[0], p.y - tc[1])).normalized()
        yaw = (deg + 180.0 + 180.0) % 360.0 - 180.0
        seats.append({"kind": "bench", "at": [round(p.x, 3), round(p.y, 3), 0.0], "yaw": round(yaw, 1), "seat_z": SEAT_Z,
                      "table": [round(tc[0] + d.x * 0.09, 3), round(tc[1] + d.y * 0.09, 3), TABLE_Z]})


def build(parts):
    for bm in parts.values():
        C.ensure_uv(bm)
    seats = []
    for x in STOOLS:
        stool(parts["stools"], x, STOOL_Y)
        seats.append({"kind": "stool", "at": [round(x, 3), round(STOOL_Y + 0.19, 3), 0.0], "yaw": 90.0, "seat_z": STOOL_TOP,
                      "table": [round(x, 3), 0.46, COUNTER_TOP], "foot_z": 0.20})
    for xa, xb in BOOTHS:
        booth(parts, min(xa, xb), max(xa, xb), seats)
    nose_bench(parts, seats)
    return {"hero_seats": seats}
