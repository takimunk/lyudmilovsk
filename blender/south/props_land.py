"""The Aton coast's land props (docs/south.md): the town's stele, the wayside spring, the coast trail's finger-posts and
notice boards, and the farm things of the valley (haystacks, hives, wattle fences, a scarecrow, an orchard ladder).

Built with the North's kit and atlas (blender/north/northkit.py, northtex.py: existing tiles only). Words are textures
typeset by tools/south_signs.py (textures/localized/<lang>/south_<id>.png); the GLBs carry material names only:
M_North_Atlas and M_South_Text_<id> (the Russian texture is the default, the game swaps the language with
LyudText::LoadTexture("South", "south_<id>")).

Conventions: metres, Z up, Blender -Y is the front, origin on the ground under the middle, deterministic seeds.
    python3 blender/south/build_south.py props_land [kind ...]
"""
import math
import os
import random
import sys

from mathutils import Matrix, Vector

import northkit as K
from northkit import Builder, V
import northtex as T

sys.path.insert(0, os.path.join(K.ROOT, "tools"))
import south_signs  # noqa: E402  (the finger-posts' words and directions: one table for the textures and the geometry)

TWO_PI = 2 * math.pi
TEXT_DIR = os.path.join(K.ROOT, "textures", "localized", "ru")
Z = Vector((0, 0, 1))


def rz(a):
    return Matrix.Rotation(a, 3, "Z")


def rx(a):
    return Matrix.Rotation(a, 3, "X")


def ry(a):
    return Matrix.Rotation(a, 3, "Y")


def lerp(a, b, t):
    return a + (b - a) * t


# =========================================================================================================
# helpers
# =========================================================================================================

def text_slot(b, tex_id):
    K.text_material("M_South_Text_%s" % tex_id, os.path.join(TEXT_DIR, "south_%s.png" % tex_id))
    return b.slot("M_South_Text_%s" % tex_id)


def quad_uv(b, pts, tile, grain=None, off=None, flatc=None, mat=0, smooth=False):
    """A planar polygon with the tile mapped flat on its own plane (metres per tile from northkit.TILE_M); grain = a
    direction the tile's U should follow (wood)."""
    pts = [Vector(p) for p in pts]
    if flatc:
        return b.face(pts, [T.swatch_uv(flatc)] * len(pts), mat=mat, smooth=smooth)
    n = (pts[1] - pts[0]).cross(pts[2] - pts[0])
    if n.length < 1e-12:
        n = (pts[2] - pts[0]).cross(pts[-1] - pts[0])
    n = n.normalized() if n.length > 1e-12 else Z.copy()
    e1 = None
    if grain is not None:
        g = Vector(grain)
        g = g - n * g.dot(n)
        if g.length > 1e-6:
            e1 = g.normalized()
    if e1 is None:
        e1 = (pts[1] - pts[0]).normalized()
    e2 = n.cross(e1)
    a = [(p - pts[0]).dot(e1) for p in pts]
    c = [(p - pts[0]).dot(e2) for p in pts]
    la, lb = max(a) - min(a), max(c) - min(c)
    off = off if off is not None else (b.rnd.random(), b.rnd.random())
    aniso = tile in ("wood", "wood_dark", "plank_edge", "bark", "stripes")
    uvs = [b.mapuv(tile, ai - min(a), ci - min(c), max(la, 1e-4), max(lb, 1e-4), off, aniso=aniso) for ai, ci in zip(a, c)]
    return b.face(pts, uvs, mat=mat, smooth=smooth)


# corner keys of a hexahedron: (ix, iy, iz) in {0, 1}
HEX_FACES = [
    [(1, 0, 0), (1, 1, 0), (1, 1, 1), (1, 0, 1)], [(0, 1, 0), (0, 0, 0), (0, 0, 1), (0, 1, 1)],
    [(1, 1, 0), (0, 1, 0), (0, 1, 1), (1, 1, 1)], [(0, 0, 0), (1, 0, 0), (1, 0, 1), (0, 0, 1)],
    [(0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1)], [(0, 1, 0), (1, 1, 0), (1, 0, 0), (0, 0, 0)],
]
HEX_NAMES = ["x+", "x-", "y+", "y-", "top", "bottom"]


def panel(b, pts, tile, grain=None):
    """A planar quad; when it is bigger than one tile window (northkit.TILE_M), it is cut into cells of one tile each so
    the texture repeats (the atlas tiles are periodic) instead of being stretched."""
    pts = [Vector(p) for p in pts]
    m = K.TILE_M.get(tile, 1.0)
    la, lb = (pts[1] - pts[0]).length, (pts[3] - pts[0]).length
    if len(pts) != 4 or (la <= m * 1.05 and lb <= m * 1.05) or tile in ("wood", "wood_dark", "plank_edge", "bark", "endgrain"):
        return quad_uv(b, pts, tile, grain=grain)
    nu, nv = max(1, math.ceil(la / m - 0.05)), max(1, math.ceil(lb / m - 0.05))
    us = [min(1.0, i * m / la) for i in range(nu)] + [1.0]
    vs = [min(1.0, j * m / lb) for j in range(nv)] + [1.0]
    P = [[pts[0].lerp(pts[1], u).lerp(pts[3].lerp(pts[2], u), v) for v in vs] for u in us]
    surf(b, P, tile, m=m, smooth=False, off=(0.0, 0.0))


def hexa(b, C, tile="ash", tiles=None, grain=None, flatc=None, skip=()):
    """A six-sided solid from 8 corners C[(ix, iy, iz)]; tiles = {face name: tile} (x+ x- y+ y- top bottom)."""
    for name, ks in zip(HEX_NAMES, HEX_FACES):
        if name in skip:
            continue
        t = (tiles or {}).get(name, tile)
        fc = flatc if not (tiles and name in tiles) else None
        if t in T.SWATCH:
            quad_uv(b, [C[k] for k in ks], t, flatc=t)
        elif fc:
            quad_uv(b, [C[k] for k in ks], t, flatc=fc)
        else:
            panel(b, [C[k] for k in ks], t, grain=grain)


def beam(b, p0, p1, w, h, tile="wood", up=None, flatc=None, tiles=None):
    """A rectangular beam from p0 to p1, w across (horizontal-ish), h along `up` (default: world Z made square to it)."""
    p0, p1 = Vector(p0), Vector(p1)
    d = (p1 - p0).normalized()
    u = Vector(up) if up is not None else Z.copy()
    if abs(u.normalized().dot(d)) > 0.98:
        u = Vector((0, 1, 0))
    s = d.cross(u).normalized()
    u = s.cross(d).normalized()
    C = {}
    for ix in (0, 1):
        for iy in (0, 1):
            for iz in (0, 1):
                C[(ix, iy, iz)] = (p0 if ix == 0 else p1) + s * ((iy - 0.5) * w) + u * ((iz - 0.5) * h)
    # remap so that hexa's x is along the beam
    hexa(b, C, tile, tiles=tiles, grain=d, flatc=flatc)


def surf(b, P, tile, m=None, mat=0, smooth=True, flatc=None, off=None, swap=False, vband=None):
    """A single-skin grid surface P[i][j] (i around / along, j up); each quad maps a window of the tile at m metres
    per tile, continuing from its neighbour where it fits (organic surfaces: hay, cloth, sacks)."""
    m = m or K.TILE_M.get(tile, 1.0)
    nu, nv = len(P), len(P[0])
    lu = [0.0]
    for i in range(1, nu):
        lu.append(lu[-1] + sum((P[i][j] - P[i - 1][j]).length for j in range(nv)) / nv)
    lv = [0.0]
    for j in range(1, nv):
        lv.append(lv[-1] + sum((P[i][j] - P[i][j - 1]).length for i in range(nu)) / nu)
    u0, v0, du, dv = b.win(tile)
    off = off if off is not None else (b.rnd.random(), b.rnd.random())
    for i in range(nu - 1):
        for j in range(nv - 1):
            q = [P[i][j], P[i + 1][j], P[i + 1][j + 1], P[i][j + 1]]
            if flatc:
                b.face(q, [T.swatch_uv(flatc)] * 4, mat=mat, smooth=smooth)
                continue
            wu = min(0.98, (lu[i + 1] - lu[i]) / m)
            wv = min(0.98, (lv[j + 1] - lv[j]) / m)
            su = (off[0] + lu[i] / m) % 1.0
            sv = (off[1] + lv[j] / m) % 1.0
            su = min(su, 1.0 - wu)
            sv = min(sv, 1.0 - wv)
            uv = [(su, sv), (su + wu, sv), (su + wu, sv + wv), (su, sv + wv)]
            if swap:
                uv = [(c, a) for a, c in uv]
            if vband:
                uv = [(a, vband[0] + c * (vband[1] - vband[0])) for a, c in uv]
            b.face(q, [(u0 + a * du, v0 + c * dv) for a, c in uv], mat=mat, smooth=smooth)


def revolve(fn, nu, nv):
    """Grid of points fn(angle, t) for angle round the full circle (nu + 1 columns, the last = the first) and t 0..1."""
    return [[Vector(fn(TWO_PI * (i % nu) / nu, j / (nv - 1))) for j in range(nv)] for i in range(nu + 1)]


def noise2(seed):
    r = random.Random(seed)
    ph = [r.uniform(0, TWO_PI) for _ in range(8)]

    def f(a, t):
        return (0.5 * math.sin(3 * a + ph[0] + 2.0 * t) + 0.3 * math.sin(5 * a + ph[1] - 3.0 * t) + 0.25 * math.sin(2 * a + ph[2])
                + 0.2 * math.sin(9 * a + ph[3] + 7 * t) + 0.15 * math.sin(13 * a + ph[4] - 5 * t))
    return f


def text_quad(b, c, ea, eb, w, h, slot, uvrect=(0, 0, 1, 1)):
    """One text plane: centre c, ea = the reading direction (texture U), eb = up (texture V); seen from ea x eb."""
    ea, eb = Vector(ea).normalized(), Vector(eb).normalized()
    u0, v0, u1, v1 = uvrect
    pts = [c - ea * w / 2 - eb * h / 2, c + ea * w / 2 - eb * h / 2, c + ea * w / 2 + eb * h / 2, c - ea * w / 2 + eb * h / 2]
    b.face(pts, [(u0, v0), (u1, v0), (u1, v1), (u0, v1)], mat=slot)


def stones_ring(b, c, r, n, size, seed, tile="stone"):
    rnd = random.Random(seed)
    for i in range(n):
        a = TWO_PI * i / n + rnd.uniform(-0.2, 0.2)
        s = size * rnd.uniform(0.8, 1.2)
        b.stone(Vector(c) + V(math.cos(a) * r, math.sin(a) * r, s * 0.18), (s, s * 0.85, s * 0.6), tile=tile, seed=seed * 31 + i, flatten=1.0,
                subdiv=2)


def gable_roof(b, cx, cz, length, depth, overhang=0.12, slope=33, y=0.0):
    a = math.radians(slope)
    half = depth / 2 / math.cos(a) + overhang
    for s in (-1, 1):
        c = V(cx, y + s * half * 0.5 * math.cos(a), cz - half * 0.5 * math.sin(a) - 0.015)
        b.box(c, (length, half, 0.032), "wood_dark", rot=rx(-s * a), grain=0)
    b.box(V(cx, y, cz + 0.005), (length + 0.02, 0.07, 0.05), "wood_dark")


# =========================================================================================================
# 1. town_stele
# =========================================================================================================

def town_stele(variant):
    b = Builder("town_stele")
    rnd = random.Random(1975)
    # podium
    b.box(V(0, 0.05, 0.15), (2.3, 1.3, 0.3), "stone", tiles={"top": "ash"})
    b.box(V(0, 0.05, 0.315), (2.1, 1.1, 0.03), "ash")
    # the main blade: leans back 0.35 m over its 5.2 m, the top cut on a slant (5.5 m at the left, 5.05 at the right)
    zb = 0.33
    lean = 0.35
    def blade(x0, x1, ztl, ztr, y0, y1, xt0, xt1):
        C = {}
        for ix, (xb, xt, zt) in enumerate(((x0, xt0, ztl), (x1, xt1, ztr))):
            for iy, yy in enumerate((y0, y1)):
                C[(ix, iy, 0)] = V(xb, yy, zb)
                C[(ix, iy, 1)] = V(xt, yy + lean * (zt - zb) / 5.2, zt)
        hexa(b, C, "ash", tiles={"bottom": "stone"})
    blade(-0.62, 0.62, 5.5, 5.05, -0.22, 0.22, -0.52, 0.5)
    # the stepped companion slab on the left, lower and set back
    blade(-1.02, -0.62, 3.55, 3.4, -0.1, 0.18, -0.98, -0.62)
    # and a low third step on the right
    blade(0.62, 0.86, 1.7, 1.62, -0.06, 0.16, 0.62, 0.84)
    # the blade's front plane frame (for the sun)
    up_t = V(0, lean, 5.2).normalized()
    n = V(0, -5.2, lean).normalized()
    ex = V(1, 0, 0)
    org = V(0, -0.22, zb)

    def F(x, s, o):
        return org + ex * x + up_t * s + n * o
    sc = 4.05                        # the sun's centre along the blade
    # rays: 16, long and short, cast metal painted gold and orange, standing proud of the blade
    for i in range(16):
        a = TWO_PI * i / 16 + math.pi / 16
        ca, sa = math.cos(a), math.sin(a)
        r0, r1 = 0.66, (1.32 if i % 2 == 0 else 1.06)
        w0, w1 = 0.10, 0.025
        px = V(-sa, ca, 0)            # perpendicular in the plane coords (x, s)
        pts = []
        for (r, w) in ((r0, w0), (r1, w1)):
            for sgn in (-1, 1):
                pts.append((ca * r + px.x * w * sgn / 2, sc + sa * r + px.y * w * sgn / 2))
        col = "yellow_sign" if i % 2 == 0 else "tin"
        C = {(0, 0, 0): F(pts[0][0], pts[0][1], 0.02), (0, 1, 0): F(pts[1][0], pts[1][1], 0.02),
             (1, 0, 0): F(pts[2][0], pts[2][1], 0.02), (1, 1, 0): F(pts[3][0], pts[3][1], 0.02)}
        for k in list(C):
            C[(k[0], k[1], 1)] = C[k] + n * 0.04
        hexa(b, C, col, tiles={nm: col for nm in HEX_NAMES})
    # the disc: a mosaic of tesserae in rings (yellow heart, gold and orange, an orange-red rim), on a steel rim
    rings = [(0.0, 0.14, 8, ("yellow_sign",) * 6 + ("white_paint",)), (0.14, 0.27, 14, ("yellow_sign",) * 5 + ("bag_orange",)),
             (0.27, 0.39, 20, ("yellow_sign", "yellow_sign", "bag_orange")), (0.39, 0.51, 26, ("bag_orange", "bag_orange", "yellow_sign")),
             (0.51, 0.62, 32, ("bag_orange",) * 4 + ("red_paint", "rust"))]
    od = 0.075
    for (ra, rb_, k, cols) in rings:
        for i in range(k):
            a0, a1 = TWO_PI * i / k, TWO_PI * (i + 1) / k
            c = rnd.choice(cols)
            if ra == 0:
                pts = [F(0, sc, od), F(math.cos(a0) * rb_, sc + math.sin(a0) * rb_, od), F(math.cos(a1) * rb_, sc + math.sin(a1) * rb_, od)]
            else:
                pts = [F(math.cos(a0) * ra, sc + math.sin(a0) * ra, od), F(math.cos(a0) * rb_, sc + math.sin(a0) * rb_, od),
                       F(math.cos(a1) * rb_, sc + math.sin(a1) * rb_, od), F(math.cos(a1) * ra, sc + math.sin(a1) * ra, od)]
            quad_uv(b, pts, c, flatc=c)
    seg = 32
    for i in range(seg):
        a0, a1 = TWO_PI * i / seg, TWO_PI * (i + 1) / seg
        R = 0.64
        q = [F(math.cos(a0) * R, sc + math.sin(a0) * R, 0.0), F(math.cos(a1) * R, sc + math.sin(a1) * R, 0.0),
             F(math.cos(a1) * R, sc + math.sin(a1) * R, od + 0.005), F(math.cos(a0) * R, sc + math.sin(a0) * R, od + 0.005)]
        quad_uv(b, q, "metal")
        q2 = [F(math.cos(a0) * R, sc + math.sin(a0) * R, od + 0.005), F(math.cos(a1) * R, sc + math.sin(a1) * R, od + 0.005),
              F(math.cos(a1) * 0.62, sc + math.sin(a1) * 0.62, od + 0.005), F(math.cos(a0) * 0.62, sc + math.sin(a0) * 0.62, od + 0.005)]
        quad_uv(b, q2, "steel", flatc="steel")
    # the name bar: a horizontal concrete beam on two square legs, in front of the blade; the name in raised letters
    bz0, bz1 = 1.52, 2.14
    by0, by1 = -0.46, -0.13
    bw = 4.3
    b.box(V(0, (by0 + by1) / 2, (bz0 + bz1) / 2), (bw, by1 - by0, bz1 - bz0), "ash", tiles={"bottom": "stone"})
    b.box(V(0, (by0 + by1) / 2 - 0.01, bz1 + 0.03), (bw + 0.08, by1 - by0 + 0.06, 0.06), "ash")      # drip cap
    for sx in (-1, 1):
        b.box(V(sx * 1.92, (by0 + by1) / 2, bz0 / 2), (0.24, 0.24, bz0), "ash", tiles={"bottom": "stone"})
        b.box(V(sx * 1.92, (by0 + by1) / 2, 0.06), (0.42, 0.42, 0.12), "stone")
    slot = text_slot(b, "stele")
    text_quad(b, V(0, by0 - 0.002, (bz0 + bz1) / 2), V(1, 0, 0), V(0, 0, 1), 4.12, 4.12 / 8.0, slot)
    # the flower bed in front: a concrete curb, dark soil, rows of salvia (red) and marigolds (orange, yellow)
    fx, fy0, fy1 = 2.0, -1.55, -0.75
    ct, ch = 0.12, 0.32
    b.box(V(0, fy0 + ct / 2, ch / 2), (2 * fx, ct, ch), "ash")
    b.box(V(0, fy1 - ct / 2, ch / 2), (2 * fx, ct, ch), "ash")
    for sx in (-1, 1):
        b.box(V(sx * (fx - ct / 2), (fy0 + fy1) / 2, ch / 2), (ct, fy1 - fy0 - 2 * ct, ch), "ash")
    sz = ch - 0.06
    quad_uv(b, [V(-fx + ct, fy0 + ct, sz), V(fx - ct, fy0 + ct, sz), V(fx - ct, fy1 - ct, sz), V(-fx + ct, fy1 - ct, sz)], "ground_dark",
            flatc="ground_dark")
    rows = [(fy0 + 0.24, ("bag_orange", "yellow_sign")), (fy0 + 0.52, ("red_paint",))]
    for ri, (yy, cols) in enumerate(rows):
        x = -fx + 0.26
        k = 0
        while x < fx - 0.22:
            px, py = x + rnd.uniform(-0.03, 0.03), yy + rnd.uniform(-0.04, 0.04)
            h = rnd.uniform(0.14, 0.22) if ri == 1 else rnd.uniform(0.08, 0.13)
            r = rnd.uniform(0.09, 0.12)
            b.cyl(V(px, py, sz - 0.01), V(px, py, sz + h * 0.6), r, r * 0.7, seg=5, flatc="bag_green", cap0=False)
            c = cols[k % len(cols)]
            if ri == 1:
                b.cyl(V(px, py, sz + h * 0.5), V(px, py, sz + h + 0.06), 0.035, 0.012, seg=4, flatc=c, cap0=False)
            else:
                b.cyl(V(px, py, sz + h * 0.6), V(px, py, sz + h * 0.6 + 0.04), r * 0.75, r * 0.55, seg=5, flatc=c, cap0=False)
            x += rnd.uniform(0.2, 0.26)
            k += 1
    return b, {"notes": "concrete stele 5.5 m high by the track: blade leaning back with a mosaic sun (r 0.64 m, rays to 1.32 m), the name "
                        "bar (4.3 m wide, top 2.14 m) on two legs, a flower bed in front (y -1.55..-0.75); text slot M_South_Text_stele "
                        "(texture south_stele.png, 8:1) on the bar's front face, one quad with UVs 0..1, 2 mm in front of the concrete",
               "manifest": {"text_slots": [{"id": "stele", "material": "M_South_Text_stele", "texture": "south_stele",
                                            "key": "south.sign.stele.name"}]}}


# =========================================================================================================
# 2. spring_trough
# =========================================================================================================

def spring_trough(variant):
    b = Builder("spring_trough")
    rnd = random.Random(533)
    W, D, H = 1.7, 0.55, 1.4
    yf = -0.18                    # the wall core's front face
    # the core (behind the stones, closes every gap), slightly battered
    C = {}
    for ix, x in enumerate((-W / 2 + 0.05, W / 2 - 0.05)):
        for iy, y in enumerate((yf, yf + D)):
            for iz, z in enumerate((-0.05, H - 0.12)):
                C[(ix, iy, iz)] = V(x * (1 - 0.05 * iz), y + 0.06 * iz * (1 - iy), z)
    hexa(b, C, "stone")
    # the niche: a dark recess with a flat shelf stone, left of the pipe
    nx, nz, nw, nh = -0.45, 1.0, 0.26, 0.26
    yfz = lambda z: yf + 0.06 * z / (H - 0.17)
    quad_uv(b, [V(nx - nw / 2, yfz(nz) - 0.004, nz), V(nx + nw / 2, yfz(nz) - 0.004, nz), V(nx + nw / 2, yfz(nz + nh) - 0.004, nz + nh),
                V(nx - nw / 2, yfz(nz + nh) - 0.004, nz + nh)], "stone_soot")
    b.box(V(nx, yfz(nz) - 0.08, nz - 0.025), (nw + 0.12, 0.2, 0.05), "stone")
    # courses of fieldstones on the front and the two ends; skip the niche and the pipe's hole
    courses = [0.1, 0.29, 0.47, 0.66, 0.85, 1.04, 1.22]
    for ci, z in enumerate(courses):
        x = -W / 2 + 0.02 + (0.0 if ci % 2 else 0.17) + rnd.uniform(-0.04, 0.04)
        if ci % 2 == 0:
            x -= 0.17
        while x < W / 2 - 0.1:
            w = rnd.uniform(0.32, 0.5)
            w = min(w, W / 2 - x + 0.02)
            cx = x + w / 2
            if w > 0.14 and not (abs(cx - nx) < nw / 2 + 0.08 and nz - 0.08 < z < nz + nh + 0.04) and not (abs(cx) < 0.14 and abs(z - 0.86) < 0.12) \
                    and not (ci == 0 and abs(cx) < 0.42):
                h = rnd.uniform(0.17, 0.22)
                yy = yfz(z) - 0.02 - rnd.uniform(0.0, 0.02)
                b.stone(V(cx, yy, z + rnd.uniform(-0.015, 0.015)), (w * 1.0, 0.13, h), tile="stone", seed=ci * 100 + int(x * 100), flatten=0.0,
                        subdiv=2, rot=rz(rnd.uniform(-0.06, 0.06)), rough=0.1)
            x += w * rnd.uniform(0.9, 0.97)
    for sx in (-1, 1):
        for ci, z in enumerate(courses[:-1]):
            b.stone(V(sx * (W / 2 - 0.06), yf + D / 2 + rnd.uniform(-0.05, 0.05), z), (0.2, D * 0.9, 0.24), tile="stone", seed=500 + ci * 2 + sx,
                    subdiv=1, rough=0.15)
    # cap stones, mossy
    x = -W / 2 + 0.05
    k = 0
    while x < W / 2 - 0.1:
        w = rnd.uniform(0.3, 0.45)
        b.stone(V(x + w / 2, yf + D / 2 + 0.02, H - 0.08), (w, D + 0.08, 0.16), tile="stone", seed=700 + k, subdiv=2, rough=0.12)
        b.stone(V(x + w / 2 + rnd.uniform(-0.05, 0.05), yf + D / 2 + rnd.uniform(-0.1, 0.1), H - 0.02), (w * 0.85, D * 0.75, 0.07),
                tile="nylon_olive", seed=800 + k, subdiv=1, rough=0.25)
        x += w * 0.9
        k += 1
    for (mx, mz, ms) in ((-0.7, 0.2, 0.3), (0.62, 0.45, 0.24), (-0.2, 0.06, 0.34), (0.3, 0.1, 0.26)):
        b.stone(V(mx, yf - 0.07, mz), (ms, 0.08, ms * 0.55), tile="nylon_olive", seed=int(mx * 100 + 900), subdiv=1, rough=0.3)
    # the iron pipe: out of the wall, a short downturn at the mouth
    pz = 0.86
    b.cyl(V(0, yf + 0.05, pz), V(0, -0.56, pz), 0.024, 0.024, seg=8, tile="metal", cap_tile="metal", cap0=False, cap1=False)
    b.cyl(V(0, -0.56, pz), V(0, -0.6, pz - 0.035), 0.024, 0.022, seg=8, tile="metal", cap_tile="metal", cap0=False)
    b.cyl(V(0, yf + 0.0, pz), V(0, yf - 0.05, pz), 0.05, 0.05, seg=8, flatc="rust")          # the collar where it leaves the wall
    spout = [0.0, -0.605, round(pz - 0.06, 3)]
    # the trough: hewn stone, walls 0.09 thick, the front lip lower in the middle with a little channel
    tx, ty0, ty1, th = 0.5, -0.98, -0.32, 0.46
    tw = 0.09
    b.box(V(0, (ty0 + ty1) / 2, 0.06), (2 * tx + 0.03, ty1 - ty0 + 0.03, 0.12), "stone")
    b.box(V(0, ty1 - tw / 2, th / 2), (2 * tx, tw, th), "stone")
    for sx in (-1, 1):
        b.box(V(sx * (tx - tw / 2), (ty0 + ty1) / 2, th / 2), (tw, ty1 - ty0 - 2 * tw, th), "stone")
    lip_w, lip_z = 0.16, th - 0.05
    for sx in (-1, 1):
        x0, x1 = lip_w / 2, tx
        b.box(V(sx * (x0 + x1) / 2, ty0 + tw / 2, th / 2), (x1 - x0, tw, th), "stone")
    b.box(V(0, ty0 + tw / 2, lip_z / 2), (lip_w, tw, lip_z), "stone")
    b.box(V(0, ty0 - 0.04, lip_z - 0.02), (lip_w, 0.08, 0.04), "stone")
    # the trough's floor inside, dark and wet
    quad_uv(b, [V(-tx + tw, ty0 + tw, 0.121), V(tx - tw, ty0 + tw, 0.121), V(tx - tw, ty1 - tw, 0.121), V(-tx + tw, ty1 - tw, 0.121)], "stone_soot")
    basin = [0.0, round((ty0 + ty1) / 2, 3), round(lip_z - 0.01, 3), round(min(tx - tw, (ty1 - ty0) / 2 - tw), 3)]
    # the stepping stone in front, and a few cobbles where the overflow runs off
    b.stone(V(0.05, -1.42, 0.03), (0.62, 0.48, 0.14), tile="stone", seed=41, flatten=1.0, subdiv=2, rough=0.08)
    for i, (cx, cy) in enumerate(((-0.25, -1.08), (0.22, -1.12), (-0.05, -1.13), (0.55, -0.85), (-0.62, -0.8))):
        s = rnd.uniform(0.12, 0.2)
        b.stone(V(cx, cy, 0.02), (s, s * 0.8, s * 0.5), tile="stone", seed=60 + i, flatten=1.0, subdiv=2)
    # the tin mug on a chain from a hook right of the pipe
    hx, hz = 0.36, 1.02
    hy = yfz(hz) - 0.09
    b.cyl(V(hx, yfz(hz) - 0.0, hz), V(hx, hy, hz), 0.008, 0.008, seg=5, flatc="rust")
    b.cyl(V(hx, hy, hz), V(hx, hy, hz + 0.03), 0.008, 0.008, seg=5, flatc="rust")
    links = 7
    for i in range(links):
        z0 = hz - i * 0.035
        if i % 2 == 0:
            b.box(V(hx, hy, z0 - 0.017), (0.004, 0.016, 0.03), flatc="steel")
        else:
            b.box(V(hx, hy, z0 - 0.017), (0.016, 0.004, 0.03), flatc="steel")
    mz1 = hz - links * 0.035 - 0.01
    mx = hx + 0.02
    b.cyl(V(mx + 0.05, hy, mz1 - 0.1), V(mx + 0.05, hy, mz1), 0.042, 0.045, seg=10, flatc="tin", cap1=False)
    b.cyl(V(mx + 0.05, hy, mz1 - 0.003), V(mx + 0.05, hy, mz1 - 0.098), 0.039, 0.042, seg=10, flatc="dark", cap0=False, cap1=True)
    b.tube([V(mx + 0.006, hy, mz1 - 0.02), V(mx - 0.012, hy, mz1 - 0.035), V(mx - 0.012, hy, mz1 - 0.065), V(mx + 0.006, hy, mz1 - 0.08)],
           0.005, seg=4, tile="metal", caps=False, flatc="tin")
    b.box(V(mx + 0.005, hy, mz1 + 0.002), (0.012, 0.004, 0.012), flatc="steel")
    return b, {"notes": "wayside spring: a dry-stone head wall 1.7 x 0.55 m, 1.4 m high (front face y -0.18), an iron pipe at z 0.86, a "
                        "hewn trough (1.0 x 0.66 m, rim 0.46, a lower lip in the middle of its front), a stepping stone, a tin mug on a "
                        "chain, a niche with a shelf; spout = where the water leaves the pipe, basin = [x, y, water z, half width] of "
                        "the water in the trough (inner box 0.82 x 0.48 m): no water in the mesh",
               "manifest": {"spout": spout, "basin": basin, "basin_box": [round(2 * (tx - tw), 3), round(ty1 - ty0 - 2 * tw, 3)]}}


# =========================================================================================================
# 3. trail_sign
# =========================================================================================================

PL_A0, PL_A1, PL_TIP, PL_H, PL_T = 0.05, 0.87, 0.1, 0.2, 0.035


def plank(b, z, yaw_deg, tex_id, seed):
    """An arrow plank from the post's axis outward; words on both faces (texture: upper half tip-right, lower tip-left)."""
    a = math.radians(yaw_deg)
    d = V(math.sin(a), -math.cos(a), 0)
    nr = V(d.y, -d.x, 0)              # the face whose viewer sees the tip on the right
    rnd = random.Random(seed)
    tilt = rnd.uniform(-0.012, 0.012)
    up = (Z + d * tilt).normalized()
    out = [(PL_A0, -PL_H / 2), (PL_A1 - PL_TIP, -PL_H / 2), (PL_A1, 0.0), (PL_A1 - PL_TIP, PL_H / 2), (PL_A0, PL_H / 2)]
    base = V(0, 0, z)
    P = lambda aa, bb, s: base + d * aa + up * bb + nr * (s * PL_T / 2)
    slot = text_slot(b, tex_id)
    L = PL_A1 - PL_A0

    def tuv(aa, bb, right):
        v = (bb + PL_H / 2) / PL_H * 0.5
        return ((aa - PL_A0) / L, 0.5 + v) if right else ((PL_A1 - aa) / L, v)
    cen = (PL_A0 + L * 0.45, 0.0)
    for side, right in ((1, True), (-1, False)):
        for i in range(len(out)):
            p0, p1 = out[i], out[(i + 1) % len(out)]
            tri = [cen, p0, p1] if side == 1 else [cen, p1, p0]
            pts = [P(q[0], q[1], side) for q in tri]
            nrm = (pts[1] - pts[0]).cross(pts[2] - pts[0])
            if nrm.dot(nr * side) < 0:
                tri = tri[::-1]
                pts = pts[::-1]
            b.face(pts, [tuv(q[0], q[1], right) for q in tri], mat=slot)
    for i in range(len(out)):
        p0, p1 = out[i], out[(i + 1) % len(out)]
        quad_uv(b, [P(p0[0], p0[1], -1), P(p1[0], p1[1], -1), P(p1[0], p1[1], 1), P(p0[0], p0[1], 1)], "wood_dark", grain=d)
    # an iron strap round the post holding the plank's root
    for dz in (-0.06, 0.06):
        c = base + d * 0.1 + up * dz
        beam(b, c - d * 0.1, c - d * 0.02, PL_T + 0.012, 0.025, flatc="steel")
    return d


def trail_sign(variant):
    b = Builder("trail_sign_v%d" % variant)
    rnd = random.Random(70 + variant)
    b.cyl(V(0, 0, -0.15), V(0, 0, 2.32), 0.068, 0.06, seg=10, tile="wood", cap_tile="endgrain", wobble=0.02, cap0=False, cap1=False)
    # a little four-sided cap against the rain
    b.cyl(V(0, 0, 2.32), V(0, 0, 2.42), 0.1, 0.0, seg=4, tile="wood_dark", cap_tile="wood_dark", cap1=False)
    rows = south_signs.PLANKS[variant]
    zs = [2.12, 1.86, 1.6][:len(rows)]
    planks = []
    for n, ((name, dist, yaw), z) in enumerate(zip(rows, zs), start=1):
        tid = "sign_%d_%d" % (variant, n)
        plank(b, z, yaw, tid, seed=variant * 10 + n)
        planks.append({"id": tid, "material": "M_South_Text_" + tid, "texture": "south_" + tid, "yaw_deg": yaw, "z": z,
                       "text_keys": ["south.sign.trail." + name, "south.sign.trail.dist." + dist]})
    stones_ring(b, (0, 0, 0), 0.19, 6, 0.2, seed=variant + 3)
    return b, {"notes": "trail finger-post 2.4 m, %d arrow planks (0.82 x 0.2 m, words on both faces, never mirrored); planks[].yaw_deg "
                        "= where each tip points, degrees counter-clockwise from above, 0 = the post's front (-Y), 90 = +X; turn the "
                        "post so the planks point at their places" % len(rows),
               "manifest": {"planks": planks}}


# =========================================================================================================
# 4. info_board
# =========================================================================================================

def text_plate(b, w, h, c, tex_id, depth=0.015, back_tile="wood", edge_tile="wood_dark"):
    """A flat panel facing -Y whose front carries the words (the whole texture), back and edges from the atlas."""
    slot = text_slot(b, tex_id)
    yf = c.y - depth / 2
    text_quad(b, V(c.x, yf, c.z), V(1, 0, 0), V(0, 0, 1), w, h, slot)
    C = {}
    for ix, x in enumerate((c.x - w / 2, c.x + w / 2)):
        for iy, y in enumerate((yf, c.y + depth / 2)):
            for iz, z in enumerate((c.z - h / 2, c.z + h / 2)):
                C[(ix, iy, iz)] = V(x, y, z)
    hexa(b, C, edge_tile, tiles={"y+": back_tile}, skip=("y-",))


def info_board(variant):
    b = Builder("info_board_v%d" % variant)
    rnd = random.Random(90 + variant)
    tex = ("board_trail", "board_reeds")[variant]
    px = 0.74
    for sx in (-1, 1):
        b.box(V(sx * px, 0, 1.05), (0.1, 0.1, 2.3), "wood_dark", rot=rz(rnd.uniform(-0.02, 0.02)))
        b.box(V(sx * px, 0, -0.12), (0.12, 0.12, 0.2), "wood_dark")
    # the back boards
    z0, z1 = 0.86, 1.98
    nb = 6
    hb = (z1 - z0) / nb
    for i in range(nb):
        b.box(V(0, 0.0, z0 + hb * (i + 0.5)), (1.38 + rnd.uniform(-0.01, 0.01), 0.025, hb - 0.006), "wood", grain=0,
              tiles={"x": "endgrain", "top": "wood"})
    cz = 1.42
    pw, ph = 1.3, 0.65
    text_plate(b, pw, ph, V(0, -0.0205, cz), tex)
    for (cx_, cz_, sx_, sz_) in ((0, cz + ph / 2 + 0.03, pw + 0.12, 0.06), (0, cz - ph / 2 - 0.03, pw + 0.12, 0.06),
                                 (-pw / 2 - 0.03, cz, 0.06, ph), (pw / 2 + 0.03, cz, 0.06, ph)):
        b.box(V(cx_, -0.026, cz_), (sx_, 0.03, sz_), "wood_dark")
    # a little shelf under the panel (for a leaflet box or a pine cone left by a child)
    b.box(V(0, -0.07, z0 - 0.02), (1.3, 0.14, 0.03), "wood", grain=0)
    for sx in (-1, 1):
        b.box(V(sx * 0.5, -0.04, z0 - 0.08), (0.03, 0.08, 0.1), "wood_dark")
    gable_roof(b, 0, 2.3, 1.85, 0.62, overhang=0.12)
    for sx in (-1, 1):
        b.box(V(sx * px, 0, 2.2), (0.12, 0.62, 0.06), "wood_dark")
    stones_ring(b, (-px, 0, 0), 0.13, 4, 0.17, seed=variant * 7 + 1)
    stones_ring(b, (px, 0, 0), 0.13, 4, 0.17, seed=variant * 7 + 2)
    return b, {"notes": "roofed timber notice board 1.6 m wide (2.45 m high); the panel (1.3 x 0.65 m, M_South_Text_%s, 2:1, "
                        "texture south_%s.png) faces -Y, 5 mm in front of the back boards" % (tex, tex),
               "manifest": {"text_slots": [{"id": tex, "material": "M_South_Text_" + tex, "texture": "south_" + tex,
                                            "keys": "south.board.%s.*" % tex.split("_")[1]}]}}


# =========================================================================================================
# 5. hay_stack
# =========================================================================================================

HAY_BAND = (0.77, 0.98)          # the knot-free band of the pale pine tile: straw, its grain running down the stack


def hay_stack(variant):
    b = Builder("hay_stack_v%d" % variant)
    nz = noise2(100 + variant)
    hay = "plank_edge"
    if variant == 0:
        prof = [(1.25, 0.0), (1.42, 0.3), (1.44, 0.7), (1.3, 1.2), (1.0, 1.75), (0.62, 2.2), (0.28, 2.55), (0.08, 2.72)]
        lean = V(0.06, 0.04, 0)

        def fn(a, t):
            k = t * (len(prof) - 1)
            i = min(int(k), len(prof) - 2)
            f = k - i
            r = lerp(prof[i][0], prof[i + 1][0], f)
            z = lerp(prof[i][1], prof[i + 1][1], f)
            r *= 1.0 + 0.08 * nz(a, t) * (1 - t * 0.5)
            return V(math.cos(a) * r, math.sin(a) * r, z) + lean * (z / 2.7)
        P = revolve(fn, 22, 12)
        surf(b, P, hay, m=0.7, swap=True, vband=HAY_BAND)
        # the pole through the top
        b.cyl(V(0, 0, 2.2), V(0.075, 0.05, 3.0), 0.045, 0.035, seg=6, tile="wood", cap_tile="endgrain", cap0=False)
        # loose hay round the foot, a skirt lying on the ground
        Q = revolve(lambda a, t: V(math.cos(a) * lerp(1.3, 1.62 + 0.22 * nz(a, 3.0), t), math.sin(a) * lerp(1.3, 1.58 + 0.22 * nz(a, 3.0), t),
                                   lerp(0.12, 0.005, t)), 22, 2)
        surf(b, Q, hay, m=0.7, swap=True, vband=HAY_BAND)
        # twisted hay ropes thrown over the top, with weights (wooden pegs) at the ends
        for k in range(3):
            a = k * TWO_PI / 3 + 0.4
            pts = [fn(a, t / 6.0) * 1.015 + V(0, 0, 0.0) for t in range(1, 7)]
            b.tube(pts, 0.018, seg=4, tile="rope", caps=False)
        note = "conical haystack round a pole, 2.9 m across, the pole's top at 3.0 m"
        manifest = {"body": [2.9, 2.9, 3.0]}
    else:
        def fn(a, t):
            # a low oblong rick: 2.6 x 1.9 m, 1.7 m high, rounded top
            rx_, ry_ = 1.3, 0.95
            zt = 1.7
            prof_r = [1.0, 1.06, 1.05, 0.95, 0.75, 0.42, 0.0]
            prof_z = [0.0, 0.25, 0.6, 1.0, 1.35, 1.6, zt]
            k = t * (len(prof_r) - 1)
            i = min(int(k), len(prof_r) - 2)
            f = k - i
            r = lerp(prof_r[i], prof_r[i + 1], f)
            z = lerp(prof_z[i], prof_z[i + 1], f)
            r *= 1.0 + 0.05 * nz(a, t)
            return V(math.cos(a) * rx_ * r, math.sin(a) * ry_ * r, z)
        P = revolve(fn, 20, 9)
        surf(b, P, hay, m=0.7, swap=True, vband=HAY_BAND)
        Q = revolve(lambda a, t: V(math.cos(a) * lerp(1.3, 1.55 + 0.15 * nz(a, 3.0), t), math.sin(a) * lerp(0.96, 1.2 + 0.15 * nz(a, 3.0), t),
                                   lerp(0.08, 0.005, t)), 20, 2)
        surf(b, Q, hay, m=0.7, swap=True, vband=HAY_BAND)
        # the tarp over the top: an old blue tarpaulin, hanging to about 1.0 m, hem wavy
        hem = noise2(7)

        def tarp(u, v):
            a = u
            p = fn(a, 1.0 - v * 0.58)
            c = V(0, 0, p.z)
            r = (p - c)
            r.z = 0
            out = p + r.normalized() * 0.035 if r.length > 1e-6 else p
            out.z += 0.03 + (0.04 * hem(a, 1.0) * v)
            return out
        us = [TWO_PI * i / 24 for i in range(25)]
        vs = [i / 6 for i in range(7)]
        b.grid(tarp, us, vs, tile="tarp_blue", thick=0.006, wrap_u=False, smooth=True)
        # stones on ropes weighing the tarp's edge
        for k in range(6):
            a = TWO_PI * k / 6 + 0.25
            p = tarp(a, 1.0)
            r = V(p.x, p.y, 0).normalized()
            s0 = p + r * 0.03
            s1 = s0 + V(0, 0, -0.42) + r * 0.08
            b.tube([s0, s1], 0.008, seg=4, tile="rope", caps=False)
            b.stone(s1 + V(0, 0, -0.08) + r * 0.02, (0.2, 0.17, 0.16), tile="stone", seed=200 + k, subdiv=2)
        note = "low rick 2.6 x 1.9 m, 1.75 m high, under an old blue tarp weighed down by six stones on ropes"
        manifest = {}
    return b, {"notes": note, "manifest": manifest}


# =========================================================================================================
# 6. beehive (Dadant)
# =========================================================================================================

def beehive(variant):
    b = Builder("beehive_v%d" % variant)
    rnd = random.Random(300 + variant)
    BL = "tarp_blue"
    OC = "nylon_yellow"
    bodies = [(0.31, BL), (0.17, OC)] if variant == 0 else [(0.31, OC), (0.17, BL), (0.17, BL)]
    W, D = 0.5, 0.46
    # stand: four legs and two rails
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.box(V(sx * 0.22, sy * 0.2, 0.19), (0.06, 0.06, 0.4), "wood_dark", rot=rz(rnd.uniform(-0.05, 0.05)))
        b.box(V(sx * 0.22, 0, 0.37), (0.07, 0.52, 0.06), "wood_dark")
    for sy in (-1, 1):
        b.box(V(0, sy * 0.2, 0.15), (0.5, 0.04, 0.05), "wood_dark")
    # bottom board with the landing board
    z = 0.4
    b.box(V(0, -0.03, z + 0.03), (W + 0.02, D + 0.1, 0.06), "wood")
    quad_uv(b, [V(-W / 2 + 0.01, -0.03 - (D + 0.1) / 2, z), V(W / 2 - 0.01, -0.03 - (D + 0.1) / 2, z),
                V(W / 2 - 0.01, -D / 2 - 0.09, z + 0.055), V(-W / 2 + 0.01, -D / 2 - 0.09, z + 0.055)], "wood")
    z += 0.06
    # the entrance slot along the bottom of the brood box front
    for i, (h, tile) in enumerate(bodies):
        jitter = (rnd.uniform(-0.006, 0.006), rnd.uniform(-0.004, 0.004))
        b.box(V(jitter[0], jitter[1], z + h / 2), (W, D, h - 0.004), tile, tiles={"top": "wood", "bottom": "wood"}, grain=0)
        # hand holds on the sides
        for sx in (-1, 1):
            b.box(V(sx * (W / 2 + 0.001), jitter[1], z + h * 0.62), (0.004, 0.14, 0.03), flatc="dark")
        if i == 0:
            b.box(V(0, -D / 2 - 0.001, z + 0.012), (0.3, 0.004, 0.018), flatc="dark")
            b.box(V(0, -D / 2 - 0.004, z + 0.024), (0.34, 0.006, 0.006), flatc="steel")    # the entrance reducer strip
        z += h
    # the roof: a telescoping lid clad in tin
    rz_ = z
    ra = rnd.uniform(-0.03, 0.03) if variant == 1 else 0.0
    rot = rz(ra)
    b.box(V(0, 0, rz_ + 0.05), (W + 0.06, D + 0.06, 0.1), "wood", rot=rot, tiles={"top": "metal"}, grain=0, flatc=None)
    b.box(V(0, 0, rz_ + 0.104), (W + 0.07, D + 0.07, 0.01), "metal", rot=rot, flatc="tin")
    for sx in (-1, 1):
        b.box(V(0, 0, rz_ + 0.02) + rot @ V(sx * (W / 2 + 0.031), 0, 0), (0.004, D + 0.06, 0.04), flatc="tin", rot=rot)
    if variant == 1:
        b.stone(V(0.05, 0.03, rz_ + 0.16), (0.24, 0.18, 0.12), tile="stone", seed=17, flatten=1.0, subdiv=2)
    # a little ladder of grass at the stand's feet: no; a flat stone under each leg
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.box(V(sx * 0.22, sy * 0.2, 0.0), (0.14, 0.13, 0.04), "stone", rot=rz(rnd.uniform(-0.3, 0.3)))
    note = ("Dadant hive on a stand, the entrance on the front (-Y): v0 a pale blue brood box and an ochre super (top %.2f m); "
            "v1 an ochre brood box, two blue supers and a stone on the lid" % (rz_ + 0.11))
    return b, {"notes": note, "manifest": {"entrance": [0.0, round(-D / 2 - 0.01, 3), 0.475]}}


# =========================================================================================================
# 7. wattle_fence
# =========================================================================================================

def wattle_fence(variant):
    b = Builder("wattle_fence_v%d" % variant)
    rnd = random.Random(400 + variant)
    L = 4.0
    ns = 10
    xs = [-L / 2 + 0.06 + (L - 0.12) * i / (ns - 1) + (rnd.uniform(-0.04, 0.04) if 0 < i < ns - 1 else 0) for i in range(ns)]
    gap = (0.15, 1.15) if variant == 1 else None       # the broken stretch (x range), upper courses gone

    def broken(x, z):
        return gap is not None and gap[0] < x < gap[1] and z > 0.42 + 0.25 * math.sin((x - gap[0]) * 2.6)
    # stakes: split poles, pointed, uneven heights; in v1 one is snapped and one leans
    stake_r = 0.032
    for i, x in enumerate(xs):
        top = 1.12 + rnd.uniform(-0.05, 0.1)
        lean = V(rnd.uniform(-0.03, 0.03), rnd.uniform(-0.03, 0.03), 0)
        if gap and gap[0] < x < gap[1]:
            if i % 2 == 0:
                top = 0.62
            else:
                lean = V(0.12, -0.22, 0)
        b.cyl(V(x, 0, -0.2), V(x, 0, top) + lean, stake_r, stake_r * 0.85, seg=6, tile="bark", cap_tile="endgrain", cap0=False, cap1=False,
              wobble=0.08)
        b.cyl(V(x, 0, top) + lean, V(x, 0, top + 0.07) + lean * 1.06, stake_r * 0.85, 0.004, seg=6, tile="wood", cap0=False, cap1=False)
    # the weave: courses of rod bundles passing in front of and behind the stakes, alternating
    zc = [0.09 + 0.066 * k for k in range(15)]
    rr = 0.027
    for k, z in enumerate(zc):
        pts = []
        segs = []
        x_prev = None
        for i in range(ns):
            side = 1 if (i + k) % 2 == 0 else -1
            zj = z + rnd.uniform(-0.012, 0.012)
            p = V(xs[i], side * (stake_r + rr * 0.9), zj)
            if i > 0:
                m = (pts[-1] + p) / 2
                m.y = 0.0
                m.z += rnd.uniform(-0.01, 0.01)
                pts.append(m)
            pts.append(p)
        # rods run a little past the end stakes
        pts.insert(0, pts[0] + V(-0.09, -pts[0].y * 0.3, rnd.uniform(-0.02, 0.02)))
        pts.append(pts[-1] + V(0.08, -pts[-1].y * 0.3, rnd.uniform(-0.02, 0.02)))
        # cut the broken stretch out (ragged ends), keep the pieces
        run = []
        for p in pts:
            if broken(p.x, p.z):
                if len(run) > 1:
                    segs.append(run)
                run = []
            else:
                run.append(p)
        if len(run) > 1:
            segs.append(run)
        for s in segs:
            b.tube(s, rr, seg=4, tile="wood", caps=True, aspect=0.8)
    if gap:
        # rods pulled out of the gap hang down in front, one lies on the grass
        b.tube([V(0.25, -0.06, 0.85), V(0.5, -0.12, 0.62), V(0.7, -0.2, 0.33)], 0.014, seg=4, tile="wood")
        b.tube([V(1.12, 0.05, 0.75), V(0.9, -0.05, 0.55), V(0.72, -0.12, 0.42)], 0.012, seg=4, tile="wood")
        b.tube([V(0.1, -0.5, 0.012), V(0.7, -0.62, 0.014), V(1.3, -0.58, 0.012)], 0.012, seg=4, tile="wood")
    # the binding rod twisted along the top
    tz = zc[-1] + 0.07
    top = []
    for i in range(41):
        x = -L / 2 + 0.02 + (L - 0.04) * i / 40
        top.append(V(x, 0.03 * math.sin(i * 1.7), tz + 0.01 * math.sin(i * 0.9)))
    run, segs = [], []
    for p in top:
        if broken(p.x, p.z):
            if len(run) > 1:
                segs.append(run)
            run = []
        else:
            run.append(p)
    if len(run) > 1:
        segs.append(run)
    for s in segs:
        b.tube(s, 0.012, seg=4, tile="wood")
    note = "woven wattle fence 4 m along X, 1.1 m high (stakes to 1.25 m), ten stakes" + (
        "; a broken stretch at x 0.15..1.15 where the upper courses are gone (one stake snapped, one leaning)" if gap else "")
    return b, {"notes": note, "manifest": {"axis": "x", "length": L}}


# =========================================================================================================
# 8. scarecrow
# =========================================================================================================

def scarecrow(variant):
    b = Builder("scarecrow")
    rnd = random.Random(808)
    b.cyl(V(0, 0, -0.3), V(0.02, 0.01, 1.92), 0.04, 0.034, seg=6, tile="wood", cap0=False, cap_tile="endgrain", wobble=0.06)
    b.cyl(V(-0.72, 0.0, 1.47), V(0.72, 0.0, 1.5), 0.028, 0.026, seg=6, tile="wood", cap_tile="endgrain", wobble=0.05)
    nz = noise2(5)
    # the body: an old plaid shirt stuffed with straw, belted with rope
    def torso(a, t):
        z = lerp(0.88, 1.5, t)
        r = 0.17 + 0.03 * math.sin(t * math.pi) - 0.05 * t * t
        r *= 1 + 0.06 * nz(a, t)
        return V(math.cos(a) * r * 1.2, math.sin(a) * r * 0.8, z)
    surf(b, revolve(torso, 10, 5), "plaid", m=0.5)
    b.cyl(V(0, 0, 1.49), V(0, 0, 1.55), 0.13, 0.05, seg=8, tile="plaid", cap0=False, cap1=False)      # shoulders closing to the neck
    b.cyl(V(0, 0, 0.96), V(0, 0, 1.0), 0.2, 0.2, seg=10, tile="rope", aspect=0.68, cap0=False, cap1=False)
    # sleeves along the crossbar, sagging a little, straw tufts at the cuffs
    for sx in (-1, 1):
        pts = [V(sx * 0.16, 0, 1.45), V(sx * 0.38, 0, 1.46), V(sx * 0.6, 0.0, 1.44)]
        b.tube(pts, 0.075, seg=7, tile="plaid", caps=False, rscale=lambda i, n: 1.0 - 0.15 * i / (n - 1))
        b.cyl(V(sx * 0.6, 0, 1.44), V(sx * 0.75, 0.0, 1.38), 0.055, 0.1, seg=6, tile="rope", cap0=False)
    # trousers: two legs hanging from the waist, straw at the ankles
    for sx in (-1, 1):
        p0 = V(sx * 0.09, 0, 0.92)
        p1 = V(sx * 0.12 + rnd.uniform(-0.03, 0.03), rnd.uniform(-0.04, 0.04), 0.42)
        b.cyl(p0, p1, 0.085, 0.07, seg=7, tile="nylon_bluegrey", cap0=False, cap1=False)
        b.cyl(p1, p1 + V(sx * 0.02, 0, -0.12), 0.05, 0.085, seg=6, tile="rope", cap0=False)
    b.box(V(0, 0, 0.9), (0.34, 0.2, 0.08), "nylon_bluegrey")
    # the head: a stuffed sack tied at the neck, turned up a little toward the sky
    hc = V(0.02, -0.01, 1.76)
    tilt = rx(0.28) @ rz(0.2)

    def head(a, t):
        ph = lerp(-math.pi / 2, math.pi / 2, t)
        r = 0.15 * (1 + 0.05 * nz(a, t + 2))
        p = V(math.cos(a) * math.cos(ph) * r, math.sin(a) * math.cos(ph) * r * 0.92, math.sin(ph) * r * 1.1)
        return hc + tilt @ p
    surf(b, revolve(head, 10, 6), "rope", m=0.3)                # burlap
    b.cyl(V(0.01, 0, 1.55), V(0.02, -0.01, 1.64), 0.05, 0.06, seg=6, tile="rope", cap0=False, cap1=False)
    b.tube([V(0.02, 0, 1.6) + V(math.cos(a) * 0.065, math.sin(a) * 0.065, 0) for a in [TWO_PI * i / 6 for i in range(7)]], 0.008, seg=3,
           tile="rope", caps=False)
    # a red knit scarf
    b.tube([V(0.0, 0, 1.58) + V(math.cos(a) * 0.085, math.sin(a) * 0.075, 0) for a in [TWO_PI * i / 8 for i in range(9)]], 0.025, seg=5,
           tile="knit", caps=False)
    b.tube([V(0.06, -0.08, 1.58), V(0.09, -0.12, 1.45), V(0.1, -0.13, 1.3)], 0.022, seg=4, tile="knit", aspect=0.5)
    # the face: two button eyes and a stitched smile, on the sack's front (-Y side, turned with the head)
    fwd = tilt @ V(0, -1, 0)
    for ex in (-0.05, 0.05):
        p = hc + tilt @ V(ex, -0.135, 0.035)
        b.cyl(p, p + fwd * 0.012, 0.018, 0.018, seg=6, flatc="black_paint")
    sm = [hc + tilt @ V(x, -0.142 + abs(x) * 0.25, -0.04 + x * x * 3.0) for x in (-0.05, -0.025, 0.0, 0.025, 0.05)]
    b.tube(sm, 0.004, seg=3, tile="rope", caps=False, flatc="black_paint")
    # a wide straw hat, its brim tilted with the head
    hat_c = hc + tilt @ V(0, 0, 0.12)
    brim = [(0.33, -0.004), (0.32, 0.012), (0.12, 0.02), (0.11, 0.1), (0.07, 0.14), (0.0, 0.145)]
    under = [(0.0, -0.01), (0.33, -0.012)]
    b.lathe(under + brim, seg=14, center=hat_c, tile="rope", rot=tilt)
    b.lathe([(0.115, 0.02), (0.117, 0.05)], seg=14, center=hat_c, rot=tilt, flatc="sock_red")
    return b, {"notes": "scarecrow 1.9 m: a pole and a crossbar (arms 1.5 m wide at 1.47 m), plaid shirt, blue trousers, straw tufts, a "
                        "sack head turned up a little toward the sky, straw hat, red scarf; faces -Y"}


# =========================================================================================================
# 9. orchard_ladder
# =========================================================================================================

def orchard_ladder(variant):
    b = Builder("orchard_ladder")
    rnd = random.Random(909)
    foot_y, top_y, top_z = -0.62, 0.5, 2.75
    wb, wt = 0.56, 0.3
    rails = []
    for sx in (-1, 1):
        p0 = V(sx * wb / 2, foot_y, 0.0)
        p1 = V(sx * wt / 2, top_y, top_z)
        rails.append((p0, p1))
        beam(b, p0 - (p1 - p0).normalized() * 0.02, p1 + (p1 - p0).normalized() * 0.06, 0.04, 0.065, "wood", up=V(0, -1, 0.4))
    axis = (rails[0][1] - rails[0][0])
    Lr = axis.length
    n = 9
    for k in range(n):
        t = (k + 0.7) / (n + 0.4)
        a, c = rails[0][0].lerp(rails[0][1], t), rails[1][0].lerp(rails[1][1], t)
        if k == 6:
            # a rung that broke and was replaced by a nailed-on lath of fresh pine
            beam(b, a + V(-0.03, -0.04, 0), c + V(0.03, -0.04, 0), 0.022, 0.06, "plank_edge", up=V(0, -0.4, 1))
            continue
        if k == 3:
            # a split one, still there but cracked and sagging
            m = (a + c) / 2 + V(0, 0, -0.012)
            b.cyl(a, m, 0.017, 0.015, seg=6, tile="wood", cap_tile="wood")
            b.cyl(m, c, 0.015, 0.017, seg=6, tile="wood", cap_tile="wood")
            continue
        b.cyl(a, c, 0.017, 0.017, seg=6, tile="wood", cap_tile="endgrain", wobble=0.04)
    # the wicker basket at the foot, a few apples in it, one on the grass
    bc = V(0.62, -0.72, 0.0)
    prof = [(0.0, 0.0), (0.16, 0.0), (0.2, 0.06), (0.235, 0.26), (0.245, 0.27)]
    b.lathe(prof, seg=12, center=bc, tile="rope")
    b.cyl(bc + V(0, 0, 0.2), bc + V(0, 0, 0.215), 0.228, 0.231, seg=12, flatc="dark", cap0=False)        # full to the brim
    b.tube([bc + V(-0.22, 0, 0.26), bc + V(-0.16, 0, 0.45), bc + V(0.0, 0, 0.52), bc + V(0.16, 0, 0.45), bc + V(0.22, 0, 0.26)], 0.012, seg=4,
           tile="rope", caps=False)
    for i in range(7):
        a = TWO_PI * i / 7 + rnd.uniform(-0.2, 0.2)
        r = 0.1 if i else 0.0
        p = bc + V(math.cos(a) * r, math.sin(a) * r, 0.22 + rnd.uniform(0, 0.03))
        c = "red_paint" if i % 3 else "yellow_sign"
        b.lathe([(0.0, -0.035), (0.03, -0.028), (0.038, 0.0), (0.03, 0.028), (0.0, 0.032)], seg=6, center=p, flatc=c)
    pa = V(0.3, -0.98, 0.036)
    b.lathe([(0.0, -0.035), (0.03, -0.028), (0.038, 0.0), (0.03, 0.028), (0.0, 0.032)], seg=6, center=pa, flatc="red_paint")
    top = [0.0, round(top_y + 0.05, 3), top_z]
    return b, {"notes": "old tapering fruit-picking ladder leaning toward +Y (foot at y -0.62, top at y 0.5, z 2.75): stand it with its "
                        "back (+Y) against an apple tree's trunk or a fork; one rung replaced by a pine lath, one cracked; a wicker "
                        "basket with apples at its foot",
               "manifest": {"lean_toward": "+Y", "top": top}}


# =========================================================================================================

KINDS = {
    "town_stele": (town_stele, {"variants": [0], "collision": "exact"}),
    "spring_trough": (spring_trough, {"variants": [0], "collision": "exact"}),
    "trail_sign": (trail_sign, {"variants": [0, 1, 2, 3], "collision": "exact"}),
    "info_board": (info_board, {"variants": [0, 1], "collision": "exact"}),
    "hay_stack": (hay_stack, {"variants": [0, 1], "collision": "box"}),
    "beehive": (beehive, {"variants": [0, 1], "collision": "box"}),
    "wattle_fence": (wattle_fence, {"variants": [0, 1], "collision": "box"}),
    "scarecrow": (scarecrow, {"variants": [0], "collision": "none"}),
    "orchard_ladder": (orchard_ladder, {"variants": [0], "collision": "none"}),
}
