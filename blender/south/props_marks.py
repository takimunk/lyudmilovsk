"""Landmarks of the Aton coast (the south): lighthouse, bunker, shipwreck, erratic.

Built with the North's kit and atlas (blender/north/northkit.py, northtex.py; no atlas change, no words). Frame as everywhere:
metres, Z up, Blender -Y is the front, origin on the ground under the middle (each kind's notes say what "the middle" is),
transforms applied. Wood is mapped as in props_places (hexa/quad: planar pieces no longer than one tile, U along the grain).
Concrete and stone are mapped "world-continuous": every face is projected on its own plane at a fixed metres-per-tile
scale with one offset for the whole mesh and cut along the tile's period lines, so neighbouring faces of one wall (or of
one boulder seen along one axis) carry one unbroken texture instead of a patchwork of randomly offset islands; the atlas
tiles are periodic, so a cut costs only the tile's 6 px inset. A face whose projection would come out mirrored is
flipped in U. Faces are wound outward explicitly (PB skips the kit's normal recalculation).
"""
import math
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

import northkit as K
import northtex as T
from northkit import V
from props_places import PB, beam, box, cbox, hexa, newell, pole, tube

M = {"ash": 1.0, "stone": 1.2, "stone_soot": 0.9, "metal": 0.6, "nylon_olive": 2.0, "canvas_green": 2.0}


def rz(a):
    return Matrix.Rotation(a, 3, "Z")


def ry(a):
    return Matrix.Rotation(a, 3, "Y")


def rx(a):
    return Matrix.Rotation(a, 3, "X")


def lerp(a, b, t):
    return a + (b - a) * t


# =========================================================================================================
# world-continuous mapping
# =========================================================================================================

def _clip(poly, k, val, keep_ge):
    """Sutherland-Hodgman against the line uv[k] = val; poly = [(uv, p3)...]."""
    out = []
    n = len(poly)
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        ina = (a[0][k] >= val) if keep_ge else (a[0][k] <= val)
        inb = (b[0][k] >= val) if keep_ge else (b[0][k] <= val)
        if ina:
            out.append(a)
        if ina != inb:
            t = (val - a[0][k]) / (b[0][k] - a[0][k])
            uv = (lerp(a[0][0], b[0][0], t), lerp(a[0][1], b[0][1], t))
            out.append((uv, a[1].lerp(b[1], t)))
    return out


def _area2(pts):
    return sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1] for i in range(len(pts))) / 2


def wface(b, P, tile, raw=None, m=None, sub=(0.0, 0.0, 1.0, 1.0)):
    """One convex planar face P (wound outward) with world-continuous UVs. raw = per-vertex coordinates in metres (default:
    the projection on the two axes the normal is not dominant in); cut along every multiple of m; sub = the part of the tile
    window used (u0, v0, u1, v1 in 0..1), e.g. to keep off the cloth tiles' middle seam."""
    P = [Vector(p) for p in P]
    if tile in T.SWATCH:
        b.face(P, [T.swatch_uv(tile)] * len(P))
        return
    n = newell(P)
    if n.length < 1e-12:
        return
    m = m or M.get(tile, 1.0)
    if raw is None:
        ax = max(range(3), key=lambda i: abs(n[i]))
        ia, ib = [(1, 2), (0, 2), (0, 1)][ax]
        raw = [(p[ia], p[ib]) for p in P]
    ou, ov = b.uvoff
    R = [((r[0] + ou) / m, (r[1] + ov) / m) for r in raw]
    if _area2(R) < 0:                                   # would be mirrored seen from outside: flip U
        R = [(-r[0], r[1]) for r in R]
    u0, v0, du, dv = b.win(tile)
    poly = list(zip(R, P))
    us, vs = [r[0] for r in R], [r[1] for r in R]
    for i in range(math.floor(min(us)), math.ceil(max(us))):
        pa = _clip(_clip(poly, 0, i, True), 0, i + 1, False)
        if len(pa) < 3:
            continue
        for j in range(math.floor(min(vs)), math.ceil(max(vs))):
            pc = _clip(_clip(pa, 1, j, True), 1, j + 1, False)
            if len(pc) < 3:
                continue
            # drop repeated points the clipping can leave
            q = [pc[0]]
            for e in pc[1:]:
                if (e[1] - q[-1][1]).length > 1e-7:
                    q.append(e)
            if len(q) > 2 and (q[0][1] - q[-1][1]).length < 1e-7:
                q.pop()
            if len(q) < 3:
                continue
            uvs = [(u0 + (sub[0] + (e[0][0] - i) * (sub[2] - sub[0])) * du, v0 + (sub[1] + (e[0][1] - j) * (sub[3] - sub[1])) * dv) for e in q]
            b.face([e[1] for e in q], uvs)


def earclip(P):
    """Triangles (index triples) of a simple planar polygon, wound as P."""
    n = newell(P)
    ax = max(range(3), key=lambda i: abs(n[i]))
    ia, ib = [(1, 2), (0, 2), (0, 1)][ax]
    Q = [(p[ia], p[ib]) for p in P]
    sgn = 1 if _area2(Q) > 0 else -1
    idx = list(range(len(P)))
    tris = []

    def cross(o, a, c):
        return (a[0] - o[0]) * (c[1] - o[1]) - (a[1] - o[1]) * (c[0] - o[0])
    guard = 0
    while len(idx) > 3 and guard < 1000:
        guard += 1
        for k in range(len(idx)):
            i0, i1, i2 = idx[k - 1], idx[k], idx[(k + 1) % len(idx)]
            a, c, d = Q[i0], Q[i1], Q[i2]
            if cross(a, c, d) * sgn <= 1e-12:
                continue
            if any(j not in (i0, i1, i2) and cross(a, c, Q[j]) * sgn > 0 and cross(c, d, Q[j]) * sgn > 0 and cross(d, a, Q[j]) * sgn > 0
                   for j in idx):
                continue
            tris.append((i0, i1, i2))
            idx.pop(k)
            break
    tris.append(tuple(idx))
    return tris


def wpoly(b, P, tile, out=None, m=None, sub=(0.0, 0.0, 1.0, 1.0), raw=None):
    """A planar polygon (concave allowed), wound along `out`, ear-clipped, each piece mapped by wface."""
    P = [Vector(p) for p in P]
    if out is not None and newell(P).dot(Vector(out)) < 0:
        P = P[::-1]
        raw = raw[::-1] if raw else None
    if len(P) == 3 or _convex(P):
        wface(b, P, tile, raw=raw, m=m, sub=sub)
        return
    for t in earclip(P):
        wface(b, [P[i] for i in t], tile, raw=[raw[i] for i in t] if raw else None, m=m, sub=sub)


def _convex(P):
    n = newell(P)
    for i in range(len(P)):
        a, c, d = P[i - 1], P[i], P[(i + 1) % len(P)]
        if (c - a).cross(d - c).dot(n) < -1e-9:
            return False
    return True


def wbox(b, c, size, tile="ash", rot=None, skip=(), tiles=None):
    """A box with world-continuous faces; skip / tiles by face name: top bottom px nx py ny."""
    c = Vector(c)
    rot = rot if rot is not None else Matrix.Identity(3)
    hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
    C = lambda x, y, z: c + rot @ Vector((x * hx, y * hy, z * hz))
    faces = {"top": [(-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)], "bottom": [(-1, 1, -1), (1, 1, -1), (1, -1, -1), (-1, -1, -1)],
             "px": [(1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1)], "nx": [(-1, 1, -1), (-1, -1, -1), (-1, -1, 1), (-1, 1, 1)],
             "py": [(1, 1, -1), (-1, 1, -1), (-1, 1, 1), (1, 1, 1)], "ny": [(-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)]}
    for nm, ks in faces.items():
        if nm in skip:
            continue
        wface(b, [C(*k) for k in ks], (tiles or {}).get(nm, tile))


def lathe_w(b, profile, c=(0, 0, 0), seg=24, rot0=0.0, m=None):
    """Surface of revolution about the vertical through c. profile = [(r, z, tile)...], the tile of each point naming the
    band from it to the next; walk the outline bottom -> outward -> up -> inward over the top (outward normals). Sloped and
    vertical bands are mapped unrolled (U = angle x r, V = length along the profile), flat ones in world XY."""
    c = Vector(c)
    s = [0.0]
    for j in range(len(profile) - 1):
        s.append(s[-1] + math.hypot(profile[j + 1][0] - profile[j][0], profile[j + 1][1] - profile[j][1]))
    Pp = lambda r, z, a: c + Vector((math.cos(a) * r, math.sin(a) * r, z))
    for j in range(len(profile) - 1):
        (ra, za, tl), (rb, zb, _) = profile[j], profile[j + 1]
        for i in range(seg):
            a0, a1 = rot0 + 2 * math.pi * i / seg, rot0 + 2 * math.pi * (i + 1) / seg
            q = [Pp(ra, za, a0), Pp(ra, za, a1), Pp(rb, zb, a1), Pp(rb, zb, a0)]
            raw = [(a0 * ra, s[j]), (a1 * ra, s[j]), (a1 * rb, s[j + 1]), (a0 * rb, s[j + 1])]
            flat = abs(zb - za) < 1e-6
            if ra < 1e-6:
                q, raw = [q[0], q[2], q[3]], [raw[0], raw[2], raw[3]]
            elif rb < 1e-6:
                q, raw = q[:3], raw[:3]
            if flat:
                raw = None
            wface(b, q, tl, raw=raw, m=m)


def ring(b, c, r_in, r_out, z0, z1, seg, tile, rot0=0.0):
    """A flat ring of `seg` straight pieces (a rail, a cornice), no inner end faces."""
    c = Vector(c)
    for i in range(seg):
        a0, a1 = rot0 + 2 * math.pi * i / seg, rot0 + 2 * math.pi * (i + 1) / seg
        d0, d1 = V(math.cos(a0), math.sin(a0), 0), V(math.cos(a1), math.sin(a1), 0)
        P = [c + d0 * r_in + V(0, 0, z0), c + d0 * r_out + V(0, 0, z0), c + d1 * r_out + V(0, 0, z0), c + d1 * r_in + V(0, 0, z0)]
        P += [p + V(0, 0, z1 - z0) for p in P]
        hexa(b, P, tile, skip=("s0", "s2"))


def chunk(b, c, size, tile, R, rnd):
    """A broken lump: a box with jittered corners, each side as two triangles."""
    hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
    C = {}
    for ix in (-1, 1):
        for iy in (-1, 1):
            for iz in (-1, 1):
                j = V(rnd.uniform(-0.25, 0.25) * hx, rnd.uniform(-0.25, 0.25) * hy, rnd.uniform(-0.2, 0.2) * hz)
                C[(ix, iy, iz)] = c + R @ (V(ix * hx, iy * hy, iz * hz) + j)
    faces = [[(-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)], [(-1, 1, -1), (1, 1, -1), (1, -1, -1), (-1, -1, -1)],
             [(1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1)], [(-1, 1, -1), (-1, -1, -1), (-1, -1, 1), (-1, 1, 1)],
             [(1, 1, -1), (-1, 1, -1), (-1, 1, 1), (1, 1, 1)], [(-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)]]
    for ks in faces:
        q = [C[k] for k in ks]
        wface(b, [q[0], q[1], q[2]], tile)
        wface(b, [q[0], q[2], q[3]], tile)


def glass_slot(b, name, color):
    """A material slot of its own (not an atlas tile): the game makes the material, the GLB carries the name only."""
    if name not in bpy.data.materials:
        K._plain(name, color)
    return b.slot(name)


def mk(name):
    b = PB(name)
    b.uvoff = (b.rnd.uniform(0, 3), b.rnd.uniform(0, 3))
    return b


# =========================================================================================================
# lighthouse
# =========================================================================================================

LH_SEG = 32
Z_PL = 0.3                       # plinth top
Z_TOP, R_BASE, R_TOP = 19.2, 2.3, 1.55
Z_SOCLE = 0.9
Z_GAL = 20.0                     # gallery deck top
Z_GL0, Z_GL1 = 20.92, 22.5       # glazing
NL = 12                          # lantern sides


def r_tower(z):
    return lerp(R_BASE, R_TOP, (z - Z_SOCLE) / (Z_TOP - Z_SOCLE))


def lighthouse(variant):
    b = mk("lighthouse")
    rnd = random.Random(61)
    seam = math.pi / 2                                    # unroll seams on the back (+Y)
    # ---- plinth (concrete, bedded 0.5) and the stone socle ---------------------------------------------------
    lathe_w(b, [(3.1, -0.5, "ash"), (3.1, Z_PL, "ash"), (2.0, Z_PL, "ash")], seg=LH_SEG, rot0=seam)
    lathe_w(b, [(2.42, Z_PL - 0.05, "stone"), (2.42, Z_SOCLE, "ash"), (r_tower(Z_SOCLE), Z_SOCLE, "white_paint")], seg=LH_SEG, rot0=seam)
    # ---- the shaft: white with two red bands, the corbelled gallery ------------------------------------------
    zs = [Z_SOCLE, 4.6, 8.2, 10.4, 13.8, 16.0, Z_TOP]
    tl = ["white_paint", "white_paint", "red_paint", "white_paint", "red_paint", "white_paint"]
    prof = [(r_tower(z), z, tl[i] if i < len(tl) else "white_paint") for i, z in enumerate(zs)]
    prof[-1] = (R_TOP, Z_TOP, "white_paint")
    prof += [(2.38, 19.75, "white_paint"), (2.38, Z_GAL, "ash"), (1.2, Z_GAL, "ash")]
    lathe_w(b, prof, seg=LH_SEG, rot0=seam)
    # ---- windows up the shaft: dark pane in a white surround proud of the wall -----------------------------
    for zc, a in ((6.4, -math.pi / 2 + 0.25), (12.0, math.pi / 2 - 0.3), (17.3, -math.pi / 2 - 0.2)):
        r = r_tower(zc)
        R = rz(a + math.pi / 2)                           # local -Y = outward
        d = V(math.cos(a), math.sin(a), 0)
        t = V(-math.sin(a), math.cos(a), 0)
        box(b, d * (r - 0.08) + V(0, 0, zc), (0.42, 0.2, 0.78), "dark", rot=R)
        for sx in (-1, 1):
            box(b, d * (r - 0.065) + t * (sx * 0.25) + V(0, 0, zc), (0.08, 0.25, 0.78), "white_paint", rot=R)
        box(b, d * (r - 0.065) + V(0, 0, zc + 0.43), (0.58, 0.25, 0.08), "white_paint", rot=R)
        box(b, d * (r - 0.05) + V(0, 0, zc - 0.43), (0.66, 0.3, 0.08), "white_paint", rot=R)
    # ---- door on the front (-Y): jambs, lintel, threshold, a plank door; two steps off the plinth ----------
    for sx in (-1, 1):
        box(b, V(sx * 0.53, -2.27, (Z_PL + 2.4) / 2), (0.16, 0.56, 2.4 - Z_PL), "white_paint")
    box(b, V(0, -2.27, 2.6), (1.22, 0.56, 0.4), "white_paint", grain=0)
    box(b, V(0, -2.3, Z_PL + 0.03), (0.9, 0.5, 0.06), "ash", grain=0)
    nb = 4
    for i in range(nb):
        x0 = -0.445 + 0.89 * i / nb
        box(b, V(x0 + 0.89 / nb / 2, -2.45 + rnd.uniform(-0.004, 0.004), (Z_PL + 0.06 + 2.4) / 2), (0.89 / nb - 0.006, 0.05, 2.4 - Z_PL - 0.06),
            "wood_dark", grain=2)
    for zz in (0.75, 1.95):
        box(b, V(0, -2.488, zz), (0.8, 0.025, 0.1), "wood_dark", grain=0)
    box(b, V(0.32, -2.51, 1.3), (0.04, 0.03, 0.14), "black_paint")
    wbox(b, V(0, -3.175, -0.05), (1.6, 0.55, 0.5), "ash")          # tread z 0.2, y -3.45..-2.9 (into the plinth)
    wbox(b, V(0, -3.625, -0.1), (1.6, 0.35, 0.4), "ash", skip=("py",))   # tread z 0.1, y -3.8..-3.45
    # ---- keeper's annex on +X: whitewashed, stone socle, flat concrete roof, door on -Y, windows ------------
    AX0, AX1, AY = 1.2, 4.6, 1.5
    ZA = 2.7
    wbox(b, V((AX0 + AX1) / 2, 0, (0.45 + ZA) / 2), (AX1 - AX0, 2 * AY, ZA - 0.45), "white_paint", skip=("top", "bottom", "nx"))
    wbox(b, V((AX0 + AX1 + 0.03) / 2, 0, (-0.3 + 0.45) / 2), (AX1 - AX0 + 0.03, 2 * AY + 0.06, 0.75), "stone", skip=("bottom", "nx"),
         tiles={"top": "ash"})
    wbox(b, V((1.0 + AX1 + 0.2) / 2, 0, ZA + 0.1), (AX1 + 0.2 - 1.0, 2 * AY + 0.4, 0.2), "ash", skip=("nx",))
    # door
    box(b, V(3.8, -AY - 0.03, (0.25 + 2.2) / 2), (0.86, 0.06, 1.95), "wood_dark", grain=2)
    for sx in (-1, 1):
        box(b, V(3.8 + sx * 0.49, -AY - 0.04, (0.25 + 2.3) / 2), (0.12, 0.08, 2.05), "white_paint")
    box(b, V(3.8, -AY - 0.04, 2.36), (1.1, 0.08, 0.12), "white_paint", grain=0)
    box(b, V(3.55, -AY - 0.075, 1.15), (0.03, 0.03, 0.12), "black_paint")
    wbox(b, V(3.8, -AY - 0.22, 0.03), (1.0, 0.44, 0.4), "ash")      # door step, tread z 0.23
    # windows: -Y wall and +X wall
    for c, R in ((V(2.45, -AY, 1.45), rz(0.0)), (V(AX1, 0.2, 1.45), rz(math.pi / 2))):
        n = R @ V(0, -1, 0)
        box(b, c + n * 0.01, (0.6, 0.06, 0.8), "dark", rot=R)
        box(b, c + n * 0.04, (0.04, 0.03, 0.76), "white_paint", rot=R)                 # mullion (front 0.055)
        box(b, c + n * 0.035, (0.58, 0.03, 0.04), "white_paint", rot=R)               # transom (front 0.05)
        for sx in (-1, 1):
            box(b, c + n * 0.04 + (R @ V(sx * 0.33, 0, 0)), (0.06, 0.08, 0.8), "white_paint", rot=R)
        box(b, c + n * 0.04 + V(0, 0, 0.43), (0.72, 0.08, 0.06), "white_paint", rot=R)
        box(b, c + n * 0.07 + V(0, 0, -0.44), (0.76, 0.14, 0.05), "white_paint", rot=R)
    # ---- gallery rail -----------------------------------------------------------------------------------------
    NP = 24
    for k in range(NP):
        a = 2 * math.pi * (k + 0.5) / NP
        p = V(math.cos(a) * 2.28, math.sin(a) * 2.28, 0)
        pole(b, p + V(0, 0, Z_GAL - 0.02), p + V(0, 0, Z_GAL + 1.0), 0.022, seg=5, tile="black_paint", cap0=False, cap1=False)
    ring(b, (0, 0, 0), 2.245, 2.315, Z_GAL + 1.0, Z_GAL + 1.06, 32, "black_paint")
    ring(b, (0, 0, 0), 2.262, 2.298, Z_GAL + 0.5, Z_GAL + 0.53, 32, "black_paint")
    # ---- the hatch in the deck, on the front (-Y) between the drum and the rail: an iron coaming round a dark hole, the lid
    # standing open against the drum, two grab stiles curving up out of it ------------------------------------------
    HY = -1.765                                           # the hatch's centre (deck z Z_GAL)
    HO, HI, HH = 0.62, 0.44, 0.07                         # coaming outer / hole width, coaming height
    for sy in (-1, 1):
        box(b, V(0, HY + sy * (HI + 0.09) / 2, Z_GAL + HH / 2), (HO, 0.09, HH), "black_paint", grain=0, skip=("bottom",))
        box(b, V(sy * (HI + 0.09) / 2, HY, Z_GAL + HH / 2), (0.09, HI, HH), "black_paint", grain=1, skip=("bottom",))
    wface(b, [V(-HI / 2, HY - HI / 2, Z_GAL + 0.012), V(HI / 2, HY - HI / 2, Z_GAL + 0.012), V(HI / 2, HY + HI / 2, Z_GAL + 0.012),
              V(-HI / 2, HY + HI / 2, Z_GAL + 0.012)], "dark")
    hinge = V(0, HY + HO / 2, Z_GAL + HH)                 # the lid's hinge line: the drum-side edge
    phi = math.radians(107.0)                             # opened past the vertical so it leans on the drum
    lu = V(0, -math.cos(phi), math.sin(phi))              # along the lid, from the hinge to its free edge
    ln = V(1, 0, 0).cross(lu)                             # across the lid's thickness (toward the rail side)
    LR = Matrix(((1, lu.x, ln.x), (0, lu.y, ln.y), (0, lu.z, ln.z)))
    box(b, hinge + lu * (HO / 2) + ln * 0.0225, (HO, HO, 0.045), "metal", rot=LR, grain=0)
    for s_ in (0.17, 0.46):
        box(b, hinge + lu * s_ + ln * 0.05, (HO - 0.04, 0.07, 0.01), "black_paint", rot=LR, grain=0)
    box(b, hinge + lu * 0.52 + ln * 0.062, (0.12, 0.03, 0.02), "black_paint", rot=LR, grain=0)         # the lid's handle
    for sx in (-1, 1):
        box(b, hinge + V(sx * 0.2, 0.0, 0.0) + ln * 0.0225 + lu * 0.0, (0.07, 0.05, 0.05), "black_paint")   # hinge knuckles
    for sx in (-1, 1):                                    # the stiles: straight out of the hole, bent toward the rail at the top
        pts = []
        for i in range(10):
            h = 0.9 * i / 9
            pts.append(V(sx * 0.17, HY - 0.05 - 0.17 * max(0.0, (h - 0.5) / 0.4) ** 2, Z_GAL + 0.012 + h))
        tube(b, pts, 0.017, seg=6, tile="metal")
    pole(b, V(-0.17, HY - 0.05, Z_GAL + 0.4), V(0.17, HY - 0.05, Z_GAL + 0.4), 0.012, seg=6, tile="metal")      # one rung
    # ---- lantern: drum, glazing with bars, dark cap with ball and vane ---------------------------------------
    rl0 = math.pi / NL
    lathe_w(b, [(1.25, Z_GAL - 0.05, "white_paint"), (1.25, 20.82, "white_paint"), (1.33, 20.86, "white_paint"),
                (1.33, Z_GL0, "white_paint"), (0.9, Z_GL0, "white_paint"), (0.0, Z_GL0, "white_paint")], seg=LH_SEG, rot0=seam)
    rv = 1.14                                             # glazing vertex radius
    gslot = glass_slot(b, "M_South_LanternGlass", (0.02, 0.03, 0.035, 1))
    for k in range(NL):
        a0, a1 = rl0 + 2 * math.pi * k / NL, rl0 + 2 * math.pi * (k + 1) / NL
        p0, p1 = V(math.cos(a0) * rv, math.sin(a0) * rv, 0), V(math.cos(a1) * rv, math.sin(a1) * rv, 0)
        # one pane = one quad in the glass slot, UV 0..1; inset a hair at each side (under the bars) so the panes' own
        # vertices stay apart and every pane shades flat
        e0, e1 = (p1 - p0).normalized() * 0.004, (p0 - p1).normalized() * 0.004
        b.face([p0 + e0 + V(0, 0, Z_GL0), p1 + e1 + V(0, 0, Z_GL0), p1 + e1 + V(0, 0, Z_GL1), p0 + e0 + V(0, 0, Z_GL1)],
               [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)], mat=gslot)
        pv = V(math.cos(a0) * (rv + 0.02), math.sin(a0) * (rv + 0.02), 0)
        beam(b, pv + V(0, 0, Z_GL0 + 0.03), pv + V(0, 0, Z_GL1 - 0.03), 0.055, 0.055, "black_paint", up=V(math.cos(a0), math.sin(a0), 0))
    for z0, z1 in ((Z_GL0, Z_GL0 + 0.06), (21.68, 21.72), (Z_GL1 - 0.06, Z_GL1)):
        ring(b, (0, 0, 0), rv * math.cos(math.pi / NL) - 0.01, rv + 0.035, z0, z1, NL, "black_paint", rot0=rl0)
    lathe_w(b, [(0.0, Z_GL1, "black_paint"), (0.9, Z_GL1, "black_paint"), (1.36, Z_GL1 + 0.04, "black_paint"), (1.36, Z_GL1 + 0.14, "black_paint"),
                (0.75, 23.05, "black_paint"), (0.2, 23.42, "black_paint"), (0.0, 23.46, "black_paint")], seg=24)
    ball = [(0.0, 23.38, "black_paint")] + [(math.sin(math.pi * i / 6) * 0.2, 23.58 - math.cos(math.pi * i / 6) * 0.2, "black_paint") for i in range(1, 6)] + [(0.0, 23.78, "black_paint")]
    lathe_w(b, ball, seg=12)
    pole(b, V(0, 0, 23.75), V(0, 0, 24.45), 0.022, seg=6, tile="black_paint")
    box(b, V(0.24, 0, 24.2), (0.5, 0.015, 0.2), "black_paint", grain=0)               # the vane
    box(b, V(-0.12, 0, 24.2), (0.22, 0.02, 0.025), "black_paint", grain=0)            # its counterweight arm
    pole(b, V(-0.24, 0, 24.2), V(-0.3, 0, 24.2), 0.03, 0.0, seg=6, tile="black_paint")
    for a in (0.0, math.pi / 2, math.pi, 3 * math.pi / 2):                            # the compass cross (no letters)
        d = V(math.cos(a), math.sin(a), 0)
        pole(b, d * 0.02 + V(0, 0, 23.95), d * 0.26 + V(0, 0, 23.95), 0.01, seg=4, tile="black_paint")
    # ---- collision ------------------------------------------------------------------------------------------
    boxes = [cbox("plinth", (0, 0, (-0.5 + Z_PL) / 2), (5.6, 5.6, Z_PL + 0.5)),
             cbox("step_0", (0, -3.175, -0.05), (1.6, 0.55, 0.5)), cbox("step_1", (0, -3.625, -0.1), (1.6, 0.35, 0.4)),
             cbox("door", (0, -2.27, 1.35), (1.22, 0.56, 2.1)),
             cbox("annex", ((AX0 + AX1) / 2 + 0.1, 0, (-0.3 + ZA + 0.2) / 2), (AX1 - AX0 + 0.2, 2 * AY + 0.4, ZA + 0.5))]
    edges = [Z_PL, 4.0, 8.0, 12.0, 16.0, Z_TOP]
    for i in range(len(edges) - 1):
        s = 1.8 * r_tower(max(edges[i], Z_SOCLE))
        boxes.append(cbox("tower_%d" % i, (0, 0, (edges[i] + edges[i + 1]) / 2), (s, s, edges[i + 1] - edges[i])))
    # the deck is walkable: its top (Z_GAL) is the collision top in the ring between the drum and the rail, the hatch is part
    # of it (no hole); the rail is a ring of small posts up to its top (importer: axis-aligned boxes only), the lantern's
    # two crossed boxes (an octagon-ish drum) go down to the deck
    boxes.append(cbox("gallery_deck", (0, 0, (Z_TOP + Z_GAL) / 2), (4.7, 4.7, Z_GAL - Z_TOP)))
    for k in range(32):
        a = 2 * math.pi * (k + 0.5) / 32
        boxes.append(cbox("rail_%d" % k, (math.cos(a) * 2.28, math.sin(a) * 2.28, Z_GAL + 0.53), (0.3, 0.3, 1.06)))
    boxes.append(cbox("lantern_x", (0, 0, (Z_GAL + 23.5) / 2), (2.5, 1.44, 23.5 - Z_GAL)))
    boxes.append(cbox("lantern_y", (0, 0, (Z_GAL + 23.5) / 2), (1.44, 2.5, 23.5 - Z_GAL)))
    lamp = [0.0, 0.0, round((Z_GL0 + Z_GL1) / 2, 3)]
    notes = ("the «Маяк» lighthouse for the cliff top: a whitewashed round masonry tower tapering from r %.2f (z %.1f) to r %.2f "
             "(z %.1f) with two red bands (z 8.2..10.4 and 13.8..16.0), on a stone socle (r 2.42, to z %.1f) and a round concrete plinth "
             "r 3.1 (top z %.1f, bedded to z -0.5, nothing else under ground); three small windows up the shaft; a corbelled gallery "
             "(deck z %.1f, r 2.38) with a black rail (top z %.2f); a lantern room (drum to z %.2f, 12-sided glazing, one quad per pane in the slot M_South_LanternGlass (UV 0..1), "
             "z %.2f..%.2f with black bars; the lantern floor and the cap's underside are closed discs, nothing opaque inside between "
             "them: the lens (kind lighthouse_lens) is a separate mesh at the lamp point), a black cap, a ball and a wind vane (top z 24.45). Origin on the ground under the "
             "tower's axis. Front (-Y): a plank door (0.9 x 2.0 m, closed, not enterable) between white jambs on the plinth and two "
             "steps off the plinth (treads z 0.2 and 0.1, out to y -3.8). A keeper's annex on +X (x %.1f..%.1f, y +-%.1f, flat "
             "concrete roof z %.1f, its door on -Y at x 3.8, windows on -Y and +X). The gallery is walkable: a hatch in the deck on "
             "the front (-Y; 0.62 m iron coaming round a dark hole at x 0, y -1.765, the lid standing open against the drum, two "
             "iron stiles curving up 0.9 m as hand holds). Collision: box (collision_boxes, axis-aligned, mesh space): plinth, "
             "steps, door surround, annex, the tower as a stack of squares (side 1.8 r), the deck (top z %.1f, one slab, the hatch "
             "is no hole), 32 rail posts (0.3 m, to z %.2f; gaps under 0.2 m), the lantern as two crossed boxes from the deck up. "
             "Manifest points (mesh frame, metres; yaw = degrees counter-clockwise seen from above, 0 = toward -Y the front): "
             "lamp = lens = the lantern's centre; hatch = where the hero stands on the deck by the hatch, facing outward; "
             "gallery_view = a standing point on the deck on the sea side (map +X = west = the sea, so with the thing's yaw 140 "
             "the sea is toward mesh (0.643, 0.766)), facing the sea; door_stand = on the first step in front of the door, facing it"
             % (R_BASE, Z_SOCLE, R_TOP, Z_TOP, Z_SOCLE, Z_PL, Z_GAL, Z_GAL + 1.06, Z_GL0, Z_GL0, Z_GL1, AX0, AX1, AY, ZA + 0.2, Z_GAL, Z_GAL + 1.06))
    return b, {"notes": notes, "manifest": {"lamp": lamp, "lens": list(lamp), "gallery_z": Z_GAL,
                                             "hatch": {"x": 0.8, "y": HY, "z": Z_GAL, "yaw": 0.0},
                                             "gallery_view": {"x": 1.125, "y": 1.34, "z": Z_GAL, "yaw": 140.0},
                                             "door_stand": {"x": 0.0, "y": -2.98, "z": 0.2, "yaw": 180.0}, "annex_door": {"x": 3.8, "y": -1.62, "z": 0.23, "yaw": 0.0}, "door": {"x": [-0.45, 0.45], "z": [Z_PL, 2.4], "y": -2.5},
                                             "plinth_depth": 0.5, "collision_boxes": boxes}}


# =========================================================================================================
# bunker
# =========================================================================================================

BX, BY = 2.4, 1.9                 # body half-size (walls)
SX, SY = 2.5, 2.0                 # roof slab half-size
ZB0, ZR0, ZR1 = -0.8, 1.3, 1.6    # body bottom, slab bottom, slab top
Z_SOOT = 0.35                     # damp, dark band at the foot
Z_CHIP = 0.4                      # the broken corner starts here


def _wall(b, plane, a0, a1, z0, z1, coord):
    """A rectangle of wall in a vertical plane (plane 'ny' 'py' 'nx' 'px' at `coord`), the soot band below Z_SOOT."""
    out = {"ny": V(0, -1, 0), "py": V(0, 1, 0), "nx": V(-1, 0, 0), "px": V(1, 0, 0)}[plane]
    cuts = [z0] + [z for z in (Z_SOOT,) if z0 < z < z1] + [z1]
    for za, zb in zip(cuts, cuts[1:]):
        tile = "stone_soot" if zb <= Z_SOOT + 1e-6 else "ash"
        if plane in ("ny", "py"):
            P = [V(a0, coord, za), V(a1, coord, za), V(a1, coord, zb), V(a0, coord, zb)]
        else:
            P = [V(coord, a0, za), V(coord, a1, za), V(coord, a1, zb), V(coord, a0, zb)]
        wpoly(b, P, tile, out=out)


def bunker(variant):
    b = mk("bunker")
    rnd = random.Random(71)
    # ---- the body: front with the embrasure, back with the doorway, the broken corner at +X -Y ---------------
    EX0, EX1, EZ0, EZ1 = 0.7, 0.24, (0.68, 0.98), (0.79, 0.91)        # embrasure: outer and inner half-widths, z ranges
    yi = -BY + 0.6
    CH = [(2.1, -BY), (2.16, -1.79), (2.26, -1.67), (BX, -1.6)]        # the chip's outline (from the front to the side)
    _wall(b, "ny", -BX, -EX0, ZB0, ZR0, -BY)
    NOTCH = [(1.6, -SY), (1.76, -1.8), (1.97, -1.52), (2.2, -1.24), (SX, -1.05)]
    yx = lerp(NOTCH[-2][1], NOTCH[-1][1], (BX - NOTCH[-2][0]) / (NOTCH[-1][0] - NOTCH[-2][0]))   # notch line meets x = BX
    xy = lerp(NOTCH[0][0], NOTCH[1][0], (-BY - NOTCH[0][1]) / (NOTCH[1][1] - NOTCH[0][1]))       # and y = -BY
    _wall(b, "ny", EX0, xy, ZB0, ZR0, -BY)
    _wall(b, "ny", xy, CH[0][0], ZB0, Z_CHIP, -BY)
    _wall(b, "ny", CH[0][0], BX, ZB0, Z_CHIP, -BY)
    _wall(b, "ny", -EX0, EX0, ZB0, EZ0[0], -BY)
    _wall(b, "ny", -EX0, EX0, EZ0[1], ZR0, -BY)
    # the embrasure's splayed reveals and its dark back
    o = [V(-EX0, -BY, EZ0[0]), V(EX0, -BY, EZ0[0]), V(EX0, -BY, EZ0[1]), V(-EX0, -BY, EZ0[1])]
    i_ = [V(-EX1, yi, EZ1[0]), V(EX1, yi, EZ1[0]), V(EX1, yi, EZ1[1]), V(-EX1, yi, EZ1[1])]
    ctr = V(0, (yi - BY) / 2, (EZ0[0] + EZ0[1]) / 2)
    for k in range(4):                                   # the reveals face the opening's axis
        q = [o[k], o[(k + 1) % 4], i_[(k + 1) % 4], i_[k]]
        cq = sum(q, Vector()) / 4
        wpoly(b, q, "ash", out=V(0, cq.y, ctr.z) - cq)
    wpoly(b, i_, "dark", out=V(0, -1, 0))
    # side walls
    _wall(b, "nx", -BY, BY, ZB0, ZR0, -BX)
    _wall(b, "px", -BY, CH[-1][1], ZB0, Z_CHIP, BX)
    _wall(b, "px", CH[-1][1], yx, ZB0, Z_CHIP, BX)
    _wall(b, "px", yx, BY, ZB0, ZR0, BX)
    # the back (+Y) with the doorway: two steps down inside the wall, a dark liner 0.6 m in
    DX, DZ = 0.45, 1.1
    yl = BY - 0.6
    _wall(b, "py", -BX, -DX, ZB0, ZR0, BY)
    _wall(b, "py", DX, BX, ZB0, ZR0, BY)
    _wall(b, "py", -DX, DX, DZ, ZR0, BY)
    _wall(b, "py", -DX, DX, ZB0, 0.0, BY)
    steps = [(BY, 0.0), (BY - 0.16, 0.0), (BY - 0.16, -0.17), (BY - 0.38, -0.17), (BY - 0.38, -0.34), (yl, -0.34)]
    for sx in (-1, 1):
        P = [V(sx * DX, y, z) for y, z in steps] + [V(sx * DX, yl, DZ), V(sx * DX, BY, DZ)]
        wpoly(b, P, "ash", out=V(-sx, 0, 0))
    wpoly(b, [V(-DX, yl, DZ), V(DX, yl, DZ), V(DX, BY, DZ), V(-DX, BY, DZ)], "ash", out=V(0, 0, -1))
    for (ya, za), (yb, zb) in zip(steps, steps[1:]):
        q = [V(-DX, ya, za), V(DX, ya, za), V(DX, yb, zb), V(-DX, yb, zb)]
        wpoly(b, q, "stone_soot", out=V(0, 0, 1) if abs(za - zb) < 1e-6 else V(0, 1, 0))
    wpoly(b, [V(-DX, yl, -0.34), V(DX, yl, -0.34), V(DX, yl, DZ), V(-DX, yl, DZ)], "dark", out=V(0, 1, 0))
    # bottom (closed for the exact collision)
    wpoly(b, [V(-BX, -BY, ZB0), V(BX, -BY, ZB0), V(BX, BY, ZB0), V(-BX, BY, ZB0)], "stone_soot", out=V(0, 0, -1))
    # ---- the broken corner: a chipped edge from z 0.4 up, the slab's corner gone ---------------------------
    chz = [ZR0 - 0.06, ZR0 - 0.14, ZR0 - 0.03, ZR0 - 0.1]              # ragged top of the chip line
    for k in range(3):
        (xa, ya), (xb, yb) = CH[k], CH[k + 1]
        wpoly(b, [V(xa, ya, Z_CHIP), V(xb, yb, Z_CHIP), V(xb, yb, chz[k + 1]), V(xa, ya, chz[k])], "stone", out=V(1, -1, 0))
    wpoly(b, [V(CH[0][0], -BY, Z_CHIP), V(BX, -BY, Z_CHIP), V(BX, CH[-1][1], Z_CHIP)] + [V(x, y, Z_CHIP) for x, y in CH[1:-1][::-1]],
          "stone", out=V(0, 0, 1))
    # the exposed top of the body inside the slab's notch (z ZR0 at the notch line, falling to the chip's ragged line)
    top = [V(xy, -BY, ZR0), V(CH[0][0], -BY, chz[0])] + [V(x, y, z) for (x, y), z in zip(CH[1:], chz[1:])] + [V(BX, yx, ZR0)]
    top += [V(x, y, ZR0) for x, y in NOTCH[1:-1][::-1]]
    # this top is not planar (the ragged line): fan it from its middle
    cm = sum(top, Vector()) / len(top)
    cm.z = ZR0 - 0.04
    for k in range(len(top)):
        t3 = [top[k], top[(k + 1) % len(top)], cm]
        wpoly(b, t3, "stone", out=V(0, 0, 1))
    # the front wall next to the chip: from x xy to the chip, up to the ragged line (a triangle-ish strip)
    wpoly(b, [V(xy, -BY, Z_CHIP), V(CH[0][0], -BY, Z_CHIP), V(CH[0][0], -BY, chz[0]), V(xy, -BY, ZR0)], "ash", out=V(0, -1, 0))
    wpoly(b, [V(BX, CH[-1][1], Z_CHIP), V(BX, yx, Z_CHIP), V(BX, yx, ZR0), V(BX, CH[-1][1], chz[-1])], "ash", out=V(1, 0, 0))
    # ---- the roof slab ---------------------------------------------------------------------------------------
    outline = [(-SX, -SY)] + NOTCH + [(SX, SY), (-SX, SY)]
    wpoly(b, [V(x, y, ZR1) for x, y in outline], "stone", out=V(0, 0, 1))
    wpoly(b, [V(x, y, ZR0) for x, y in outline], "ash", out=V(0, 0, -1))
    for k in range(len(outline)):
        (xa, ya), (xb, yb) = outline[k], outline[(k + 1) % len(outline)]
        broken = 1 <= k <= len(NOTCH) - 1
        q = [V(xa, ya, ZR0), V(xb, yb, ZR0), V(xb, yb, ZR1), V(xa, ya, ZR1)]
        wpoly(b, q, "stone" if broken else "ash", out=V(yb - ya, -(xb - xa), 0))
    # the visor over the embrasure
    wbox(b, V(0, -BY - 0.07, 1.06), (1.9, 0.14, 0.12), "ash", skip=("py",))
    # wing walls by the doorway, holding the earth back
    for sx in (-1, 1):
        wbox(b, V(sx * 0.82, BY + 0.45, (-0.4 + 0.45) / 2), (0.24, 0.9, 0.85), "ash", skip=("ny",), tiles={"top": "stone"})
    # ---- rust: rebar stubs out of the break, a bent one, and broken chunks at the corner's foot --------------
    bars = [((1.7, -1.86, 1.48), (0.22, -0.1, 0.06)), ((1.88, -1.6, 1.42), (0.18, -0.16, 0.12)), ((2.08, -1.33, 1.5), (0.25, -0.05, 0.0)),
            ((2.3, -1.16, 1.4), (0.12, -0.22, -0.05)), ((2.2, -1.72, 1.12), (0.2, -0.14, 0.18)), ((2.34, -1.62, 0.8), (0.17, -0.1, -0.02))]
    for (p, d) in bars:
        p, d = V(*p), V(*d)
        q = p + d
        tube(b, [p - d * 0.4, p + d * 0.5, q + V(0, 0, rnd.uniform(-0.08, 0.05))], 0.011, seg=4, tile="metal", m=0.6)
    for k, (x, y, s) in enumerate(((2.75, -2.25, 0.34), (3.05, -1.7, 0.26), (2.55, -2.65, 0.2))):
        R = rz(rnd.uniform(0, 3)) @ rx(rnd.uniform(-0.3, 0.3))
        chunk(b, V(x, y, s * 0.22), (s * 1.3, s, s * 0.8), "stone" if k else "ash", R, rnd)
    # ---- moss: low cushions on the roof and on the wing walls (each a fan just above the concrete) -----------
    patches = [(-1.5, 1.2, 0.75), (0.4, 1.4, 0.5), (-1.9, -1.2, 0.45), (0.9, -0.2, 0.35), (-0.4, -1.6, 0.3)]
    for k, (x, y, r) in enumerate(patches):
        n = 11
        pts = []
        for i in range(n):
            a = 2 * math.pi * i / n
            rr = r * (0.7 + 0.3 * math.sin(3 * a + k) + rnd.uniform(-0.08, 0.08))
            px = max(-SX + 0.04, min(SX - 0.04, x + math.cos(a) * rr))
            py = max(-SY + 0.04, min(SY - 0.04, y + math.sin(a) * rr * 0.8))
            pts.append(V(px, py, ZR1 + 0.012))
        cpt = V(x, y, ZR1 + 0.035)
        for i in range(n):
            wpoly(b, [pts[i], pts[(i + 1) % n], cpt], "nylon_olive", out=V(0, 0, 1), sub=(0.04, 0.04, 0.46, 0.96))
    notes = ("WW2 concrete pillbox half sunk into the cliff top: body %.1f x %.1f m (walls), roof slab %.1f x %.1f m, slab top z %.1f "
             "(walkable roof), the body goes down to z %.1f under ground. Front (-Y, toward the sea): a horizontal embrasure 1.4 x 0.3 m "
             "(z 0.68..0.98) splaying in to a dark back 0.6 m inside, a visor over it. Back (+Y): a doorway 0.9 x 1.1 m whose sill is "
             "at ground level, two steps down inside the wall (treads z -0.17, -0.34) to a dark liner 0.6 m in (not enterable), low "
             "wing walls either side (to y %.2f). The +X -Y corner is broken: the slab's corner is gone and the wall's edge chipped "
             "from z 0.4 up, rusty rebar stubs stick out, broken chunks lie at its foot (out to x 3.2, y -2.75). Damp dark band to "
             "z 0.35, lichen-grey roof with moss cushions. Collision: exact"
             % (2 * BX, 2 * BY, 2 * SX, 2 * SY, ZR1, ZB0, BY + 0.9))
    return b, {"notes": notes, "manifest": {"roof_z": ZR1, "embrasure": {"x": [-EX0, EX0], "z": list(EZ0), "y": -BY},
                                             "doorway": {"x": [-DX, DX], "z": [0.0, DZ], "y": BY}, "depth": -ZB0}}


# =========================================================================================================
# shipwreck
# =========================================================================================================

HEEL = -0.2                       # rad about +X: the +Y side lies lower (planks still on it, half in the sand)
SINK = 0.18                       # whole hull lowered so the lowest parts are 0.5 m under


def hull_frame():
    Rh = rx(HEEL)
    return lambda p: Rh @ Vector(p) + V(0, 0, SINK)


def half_breadth(x):
    return 0.22 + 1.28 * max(0.0, 1 - (abs(x) / 4.45) ** 2.2) ** 0.55


def keel_z(x):
    return -0.36 + 0.3 * (abs(x) / 4.5) ** 3


def sheer_h(x):
    return 1.45 + 0.4 * (x / 4.5) ** 2


def S(x, t, side):
    """A point on the hull's outside: station x, t 0 (keel) .. 1 (gunwale), side +-1 (before the heel)."""
    th = t * math.pi / 2
    y = side * (0.1 + (half_breadth(x) - 0.1) * math.sin(th) ** 0.75)
    z = keel_z(x) + sheer_h(x) * (1 - math.cos(th)) + 0.15 * t
    return V(x, y, z)


def sweep(b, pts, normals, w, d, tile="wood_dark", along=None):
    """A rectangular member along pts: width w along `along` (default X), depth d from the line toward normals[i]."""
    rings = []
    for p, n in zip(pts, normals):
        ax = Vector(along) if along is not None else V(1, 0, 0)
        rings.append([p - ax * w / 2, p + ax * w / 2, p + ax * w / 2 + n * d, p - ax * w / 2 + n * d])
    last = len(rings) - 2
    for i in range(len(rings) - 1):
        sk = tuple(([] if i == 0 else ["bottom"]) + ([] if i == last else ["top"]))
        hexa(b, rings[i] + rings[i + 1], tile, grain=(pts[i + 1] - pts[i]), skip=sk)


def shipwreck(variant):
    b = mk("shipwreck")
    rnd = random.Random(81)
    F = hull_frame()
    # ---- keel and keelson along X, rising at the ends ---------------------------------------------------------
    xs = [-4.2 + 8.35 * i / 12 for i in range(13)]
    kp = [F(V(x, 0, keel_z(x) - 0.32)) for x in xs]
    up = F(V(0, 0, 1)) - F(V(0, 0, 0))
    sweep(b, kp, [up] * len(kp), 0.2, 0.34, "wood_dark", along=F(V(0, 1, 0)) - F(V(0, 0, 0)))
    kq = [F(V(x, 0, keel_z(x) + 0.0)) for x in xs[1:-1]]
    sweep(b, kq, [up] * len(kq), 0.16, 0.24, "wood_dark", along=F(V(0, 1, 0)) - F(V(0, 0, 0)))
    # ---- stem post at +X: curving up and leaning over to -Y; sternpost at -X broken off ----------------------
    lean = rx(0.22) @ ry(0.08)
    base = V(4.15, 0, keel_z(4.15) - 0.2)
    sp = [base + lean @ (V(x, 0, z) - base) for x, z in ((4.15, keel_z(4.15) - 0.2), (4.38, 0.35), (4.56, 1.1), (4.66, 1.85), (4.7, 2.5))]
    sp = [F(p) for p in sp]
    nrm = []
    for i in range(len(sp)):
        tng = (sp[min(i + 1, len(sp) - 1)] - sp[max(i - 1, 0)]).normalized()
        yy = F(V(0, 1, 0)) - F(V(0, 0, 0))
        nrm.append(tng.cross(yy).normalized() * -1)
    sweep(b, sp, nrm, 0.2, 0.22, "wood_dark", along=F(V(0, 1, 0)) - F(V(0, 0, 0)))
    st = [F(V(x, 0, z)) for x, z in ((-4.2, keel_z(-4.2) - 0.2), (-4.36, 0.3), (-4.48, 0.95))]
    sweep(b, st, [F(V(1, 0, 0)) - F(V(0, 0, 0))] * 3, 0.2, 0.2, "wood_dark", along=F(V(0, 1, 0)) - F(V(0, 0, 0)))
    # ---- ribs: 9 pairs, uneven, some broken off low ----------------------------------------------------------
    stations = [-3.4 + 0.85 * i for i in range(9)]
    tops = {}
    for i, x in enumerate(stations):
        for side in (-1, 1):
            tmax = rnd.uniform(0.72, 1.0)
            if (i, side) in ((1, -1), (4, -1), (6, -1), (7, 1), (2, 1)):
                tmax = rnd.uniform(0.32, 0.5)                 # broken
            if (i, side) in ((5, -1),):
                continue                                       # gone
            tops[(i, side)] = tmax
            ts = [0.05 + (tmax - 0.05) * k / 6 for k in range(7)]
            pts = [F(S(x + rnd.uniform(-0.015, 0.015), t, side)) for t in ts]
            ns = []
            for k, t in enumerate(ts):
                tg = (F(S(x, t + 0.02, side)) - F(S(x, t - 0.02, side))).normalized()
                ax = F(V(1, 0, 0)) - F(V(0, 0, 0))
                n = ax.cross(tg).normalized()
                if n.dot(F(V(0, -side, 0.3)) - F(V(0, 0, 0))) < 0:      # inward (toward the boat's middle)
                    n = -n
                ns.append(n)
            sweep(b, pts, ns, 0.12, 0.14, "wood_dark")
    # ---- planks left on the +Y side (the low side), outside the ribs, some short -------------------------------
    bands = [(0.12, 0.25, -3.85, 3.7), (0.26, 0.38, -3.6, 2.7), (0.39, 0.5, -2.45, 3.2), (0.51, 0.6, -1.3, 1.8), (0.61, 0.7, 0.55, 3.05)]
    for (ta, tb, xa, xb) in bands:
        n = max(2, int(math.ceil((xb - xa) / 0.9)))
        xs_ = [xa + (xb - xa) * k / n for k in range(n + 1)]
        rings = []
        for x in xs_:
            A, B = S(x, ta, 1), S(x, tb, 1)
            tg = (B - A).normalized()
            out = V(1, 0, 0).cross(tg).normalized()
            if out.dot(V(0, 1, -0.3)) < 0:
                out = -out
            A, B = A + out * 0.005, B + out * 0.005
            rings.append([F(A), F(B), F(B + out * 0.045), F(A + out * 0.045)])
        last = len(rings) - 2
        for k in range(len(rings) - 1):
            sk = tuple(([] if k == 0 else ["bottom"]) + ([] if k == last else ["top"]))
            hexa(b, rings[k] + rings[k + 1], "wood", grain=F(V(1, 0, 0)) - F(V(0, 0, 0)), skip=sk)
    # a length of gunwale still on two full ribs of the high side
    full = [i for i in range(8) if tops.get((i, -1), 0) > 0.8 and tops.get((i + 1, -1), 0) > 0.8]
    if full:
        i0, i1 = full[0], full[0] + 1
        xg = [stations[i0] + (stations[i1] - stations[i0]) * k / 3 for k in range(4)]
        gp = [F(S(x, min(tops[(i0, -1)], tops[(i1, -1)]) - 0.03, -1) + V(0, 0.07, 0)) for x in xg]
        sweep(b, gp, [up] * len(gp), 0.14, 0.09, "wood", along=F(V(0, 1, 0)) - F(V(0, 0, 0)))
    # ---- iron: a rusty strap and ring bolt on the stem --------------------------
    for k, t in ((0, 0.45), (1, 0.62)):
        p = sp[1].lerp(sp[2], t)
        tg = (sp[2] - sp[1]).normalized()
        yy = F(V(0, 1, 0)) - F(V(0, 0, 0))
        box(b, p + nrm[1] * 0.11, (0.24, 0.24, 0.08), "metal", rot=Matrix((yy.cross(tg).normalized(), yy, tg)).transposed(), grain=0)
    rp = sp[2] + nrm[2] * 0.3
    pts = [rp + (nrm[2] * math.cos(2 * math.pi * i / 10) + (sp[3] - sp[2]).normalized() * math.sin(2 * math.pi * i / 10)) * 0.09 for i in range(11)]
    tube(b, pts, 0.016, seg=5, tile="metal", m=0.6, caps=False)
    pole(b, sp[2] + nrm[2] * 0.2, sp[2] + nrm[2] * 0.23, 0.022, seg=6, tile="metal")
    # ---- collision: the keel line, the planked side, the stem -------------------------------------------------
    kl = F(V(0, 0, keel_z(0)))
    boxes = [cbox("keel", (0.0, round(kl.y, 3), (-0.5 + 0.12) / 2), (8.6, 0.45, 0.62))]
    A, B = F(S(0, 0.12, 1)), F(S(0, 0.7, 1))
    ang = math.degrees(math.atan2(B.z - A.z, B.y - A.y))
    mid = (A + B) / 2
    boxes.append(cbox("planked_side", (-0.1, mid.y, mid.z), (7.4, (B - A).length, 0.25), rot_x_deg=ang))
    sm = (sp[0] + sp[-1]) / 2
    boxes.append(cbox("stem", (sm.x, sm.y, sm.z), (0.6, 0.35, (sp[-1] - sp[0]).length * 0.95)))
    notes = ("the ribs and keel of an old wooden fishing boat half buried in beach sand, about 9 m long along X (stem at +X, curving up "
             "to z ~2.4 and leaning over to -Y; the sternpost at -X broken off at z ~0.8), beam ~3 m, heeled %.0f degrees so the +Y "
             "side lies lower: 9 pairs of dark ribs of uneven height (five broken low, one gone), five strakes of grey planking left "
             "on the +Y side (partly running into the sand), a bit of gunwale on the high (-Y) side, a rusty strap, ring bolt on "
             "the stem. Origin on the sand under the middle; keel, rib feet and plank ends go down to z -0.5 (under the "
             "sand). No front (yaw freely). Collision: box (collision_boxes, mesh space; rot_x_deg right-handed about +X): the keel "
             "line (walkable along its top), the planked side as one tilted slab, the stem; nothing on the ribs, so one walks "
             "through between them" % abs(math.degrees(HEEL)))
    return b, {"notes": notes, "manifest": {"axis": "x", "buried_to_z": -0.5, "collision_boxes": boxes}}


# =========================================================================================================
# erratic
# =========================================================================================================

def _noise(seed, k=7):
    r = random.Random(seed)
    dirs = []
    for _ in range(k):
        v = V(r.gauss(0, 1), r.gauss(0, 1), r.gauss(0, 1)).normalized()
        dirs.append((v, r.uniform(1.2, 3.2), r.uniform(0, 6.28), r.uniform(0.4, 1.0)))
    return lambda d: sum(a * math.sin(f * d.dot(v) + ph) for v, f, ph, a in dirs) / sum(a for *_, a in dirs)


def _decimate(bm, ratio):
    """bm through Blender's collapse decimation (an evaluated modifier), back as a triangulated bmesh."""
    import bpy
    me = bpy.data.meshes.new("_dec")
    bm.to_mesh(me)
    ob = bpy.data.objects.new("_dec", me)
    bpy.context.scene.collection.objects.link(ob)
    mod = ob.modifiers.new("d", "DECIMATE")
    mod.ratio = ratio
    dg = bpy.context.evaluated_depsgraph_get()
    ev = ob.evaluated_get(dg)
    m2 = ev.to_mesh()
    out = bmesh.new()
    out.from_mesh(m2)
    ev.to_mesh_clear()
    bpy.data.objects.remove(ob)
    bpy.data.meshes.remove(me)
    bmesh.ops.triangulate(out, faces=list(out.faces))
    bm.free()
    return out


def _boulder_bm(seed, radii, cz, rough=0.13, dent=None, size=None, top=None, ratio=0.5):
    """A rounded boulder: icosphere (1280 faces) pushed by low-frequency noise, flatter on top, scaled to `size` (x, y) and `top`
    (z of the highest point, with the foot at z -0.4), an optional hollow, then decimated to `ratio` (curvature-adaptive)."""
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=4, radius=1.0)
    n1, n2 = _noise(seed), _noise(seed + 1, 11)
    for v in bm.verts:
        d = v.co.normalized()
        k = 1 + rough * n1(d) + 0.035 * n2(d * 2.3)
        p = V(d.x * radii[0], d.y * radii[1], d.z * radii[2]) * k
        if p.z > 0:
            p.z *= 0.88
        v.co = p + V(0, 0, cz)
    if size:
        xs, ys = [v.co.x for v in bm.verts], [v.co.y for v in bm.verts]
        zmax = max(v.co.z for v in bm.verts)
        cx, cy = (max(xs) + min(xs)) / 2, (max(ys) + min(ys)) / 2
        sx, sy = size[0] / (max(xs) - min(xs)), size[1] / (max(ys) - min(ys))
        zb = -0.4
        sz = (top - zb) / (zmax - zb)
        for v in bm.verts:
            v.co = V((v.co.x - cx) * sx, (v.co.y - cy) * sy, zb + (v.co.z - zb) * sz)
    if dent:
        (dx, dy), rr, depth = dent
        zmax = max(v.co.z for v in bm.verts)
        for v in bm.verts:
            g = math.exp(-((v.co.x - dx) ** 2 + (v.co.y - dy) ** 2) / (rr * rr))
            if v.co.z > zmax - 0.9:
                v.co.z -= depth * g * min(1.0, (v.co.z - (zmax - 0.9)) / 0.5)
    # drop what lies well under the bedding before decimating (it is cut flat afterwards)
    under = [v for v in bm.verts if v.co.z < -0.75]
    if under:
        bmesh.ops.delete(bm, geom=under, context="VERTS")
    return _decimate(bm, ratio)


def _cut(bm, co, no, keep_positive):
    geom = list(bm.verts) + list(bm.edges) + list(bm.faces)
    bmesh.ops.bisect_plane(bm, geom=geom, dist=1e-5, plane_co=co, plane_no=no, clear_inner=keep_positive, clear_outer=not keep_positive)
    edges = [e for e in bm.edges if len(e.link_faces) == 1]
    bmesh.ops.holes_fill(bm, edges=edges, sides=0)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))


def _emit_rock(b, bm, xf=None, m=2.0):
    """Faces of bm into b (world-continuous stone), optionally moved by xf (a rigid motion)."""
    bm.normal_update()
    xf = xf or (lambda p: p)
    for f in bm.faces:
        P = [xf(v.co.copy()) for v in f.verts]
        if len(P) > 4:
            c = f.calc_center_median()
            wpoly(b, P, "stone", out=xf(c + f.normal) - xf(c), m=m)
        else:
            wface(b, P, "stone", m=m)


def _lichen(b, bm, centre, r, seed, xf=None, blobs=9):
    """A crust of lichen: a few irregular discs laid on the rock (vertices cast onto the surface, 15 mm proud), in the pale
    sage cloth tile (its left half, off the tile's middle seam), mottled at 0.7 m per tile."""
    from mathutils.bvhtree import BVHTree
    xf = xf or (lambda p: p)
    tree = BVHTree.FromBMesh(bm)
    rnd = random.Random(seed)
    centre = Vector(centre)
    hit = tree.find_nearest(centre)
    nrm0 = hit[1]
    t1 = nrm0.cross(V(0, 0, 1) if abs(nrm0.z) < 0.9 else V(1, 0, 0)).normalized()
    t2 = nrm0.cross(t1)
    spots = [(0, 1.0, 18)] + [(1, rnd.uniform(0.28, 0.45), 10) for _ in range(3)] + [(2, rnd.uniform(0.07, 0.16), 6) for _ in range(blobs)]
    for kind, fr_r, nseg in spots:
        rr = r * fr_r
        ang = rnd.uniform(0, 2 * math.pi)
        dist = 0.0 if kind == 0 else r * (rnd.uniform(0.85, 1.25) if kind == 1 else rnd.uniform(1.0, 1.8))
        c0 = hit[0] + (t1 * math.cos(ang) + t2 * math.sin(ang)) * dist
        ph = [rnd.uniform(0, 6.3) for _ in range(3)]

        def rim(i, fr):
            a = 2 * math.pi * i / nseg
            k = 0.8 + 0.12 * math.sin(3 * a + ph[0]) + 0.07 * math.sin(5 * a + ph[1]) + 0.05 * math.sin(9 * a + ph[2])
            return c0 + (t1 * math.cos(a) + t2 * math.sin(a)) * rr * fr * k
        rings = [[c0]] + [[rim(i, fr) for i in range(nseg)] for fr in ((0.55, 1.0) if kind == 0 else (1.0,))]

        def onto(p):
            q = tree.find_nearest(p)
            return q[0] + q[1] * 0.015 if q[0] is not None else p
        R = [[onto(p) for p in rg] for rg in rings]
        tris = []
        for i in range(nseg):
            i2 = (i + 1) % nseg
            tris.append([R[0][0], R[1][i], R[1][i2]])
            if len(R) > 2:
                tris.append([R[1][i], R[2][i], R[2][i2]])
                tris.append([R[1][i], R[2][i2], R[1][i2]])
        for t in tris:
            P = [xf(p) for p in t]
            wpoly(b, P, "canvas_green", out=xf(hit[0] + nrm0) - xf(hit[0]), m=0.7, sub=(0.04, 0.04, 0.46, 0.96))


def erratic(variant):
    b = mk("erratic_v%d" % variant)
    if variant == 0:
        bm = _boulder_bm(91, (2.25, 1.75, 1.75), 1.0, rough=0.12, size=(4.5, 3.5), top=2.8, dent=((0.35, 0.15), 0.55, 0.16), ratio=0.38)
        _cut(bm, V(0, 0, -0.4), V(0, 0, 1), True)
        _emit_rock(b, bm)
        _lichen(b, bm, V(-0.9, -1.55, 1.75), 0.55, 7)
        zt = max(v.co.z for v in bm.verts)
        bm.free()
        notes = ("a big glacial erratic (a Baltic \"Devil's stone\"): rounded granite 4.5 x 3.5 m, top z %.2f, with a shallow hollow on "
                 "top (near x 0.35, y 0.15) and a pale lichen crust on the front (-Y) shoulder; bedded 0.4 m (cut flat at z -0.4, "
                 "nothing else under ground). Origin on the ground under the middle. No real front (the lichen side, -Y, reads "
                 "best toward a path). Collision: exact" % zt)
        return b, {"notes": notes, "manifest": {"bedded": 0.4, "top_z": round(zt, 3)}}
    # v1: one stone split in two, the halves a walk-through gap apart, leaning a little away from each other
    GAP = 1.0
    tops = []
    for side in (-1, 1):
        bm = _boulder_bm(93, (1.55, 1.5, 1.3), 0.85, rough=0.11, size=(2.15, 2.9), top=2.25, ratio=0.32)
        _cut(bm, V(0, 0, -0.4), V(0, 0, 1), True)
        _cut(bm, V(0.0, 0, 0), V(1, 0.08, 0.05), side > 0)
        xs = [v.co.x for v in bm.verts]
        inner = min(xs) if side > 0 else max(xs)
        pivot = V(inner, 0, -0.4)
        R = rz(side * 0.06) @ ry(side * 0.07)

        def xf(p, R=R, side=side, pivot=pivot, inner=inner):
            return R @ (p - pivot) + V(side * GAP / 2, 0, -0.4)
        _emit_rock(b, bm, xf=xf)
        if side < 0:
            _lichen(b, bm, V(-0.6, -0.5, 2.0), 0.4, 9, xf=xf)
        tops.append(max(xf(v.co).z for v in bm.verts))
        bm.free()
    notes = ("a split glacial erratic: one granite boulder (about 3.5 m across overall, 2.9 m deep, top z %.2f) cracked along a "
             "plane into two halves standing %.1f m apart at the ground (the gap runs along Y through the origin), leaning a little "
             "away from each other: one can walk through. A lichen crust on the -X half's top. Bedded 0.4 m (cut flat at z -0.4). "
             "Origin on the ground in the middle of the gap. No front. Collision: exact (the gap stays open)" % (max(tops), GAP))
    return b, {"notes": notes, "manifest": {"bedded": 0.4, "gap": GAP, "gap_axis": "y"}}


KINDS = {
    "lighthouse": (lighthouse, {"variants": [0], "collision": "box"}),
    "bunker": (bunker, {"variants": [0], "collision": "exact"}),
    "shipwreck": (shipwreck, {"variants": [0], "collision": "box"}),
    "erratic": (erratic, {"variants": [0, 1], "collision": "exact"}),
}
