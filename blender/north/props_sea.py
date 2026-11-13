"""Sea stacks (кекуры) for the north coast (docs/north.md "Sea stacks"): `sea_stack` v0..v3, columnar dark basalt on the kit's atlas.

Frame as everywhere in the kit: metres, Z up, origin on the water line (z = 0 is the sea at mid tide) under the middle, transforms applied.
The rock goes down to z = -3 so tides (+-1 m) and waves never show a bottom.

Built from polygonal columns (4..7 sides, ragged tops, a paler salt/barnacle band at the waterline, a little guano on some tops). Every face has its
own planar UVs, uniform in metres, fitted inside its tile window, so nothing wraps over a tile edge or stretches (see props_river.rock).
Tiles: 'stone_soot' (dark basalt), 'ash' (the pale speckled grey: salt band and guano). No atlas change.
"""
import math
import random

from mathutils import Matrix, Vector

from northkit import Builder, V
from props_river import rock

BASALT = "stone_soot"
SALT = "ash"
BOTTOM = -3.0
UV_M = 2.4            # metres of rock one tile window shows at full size; bigger faces shrink the texture uniformly (never stretch)


def lerp(a, b, t):
    return a + (b - a) * t


def pface(b, rnd, pts, tile, flat=False):
    """One face with planar UVs in the face's own plane (uniform scale, so no stretching), fitted inside the tile window."""
    n = Vector()
    for i, p in enumerate(pts):                              # Newell normal
        q = pts[(i + 1) % len(pts)]
        n += Vector(((p.y - q.y) * (p.z + q.z), (p.z - q.z) * (p.x + q.x), (p.x - q.x) * (p.y + q.y)))
    if n.length < 1e-9:
        return
    n.normalize()
    t = Vector((1, 0, 0)) if abs(n.z) > 0.9 else Vector((0, 0, 1)).cross(n).normalized()
    bv = n.cross(t)
    cs = [(p.dot(t), p.dot(bv)) for p in pts]
    mu, mv = min(c[0] for c in cs), min(c[1] for c in cs)
    lu, lv = max(c[0] for c in cs) - mu, max(c[1] for c in cs) - mv
    s = min(1.0, UV_M / max(lu, 1e-6), UV_M / max(lv, 1e-6))
    u0, v0, du, dv = b.win(tile)
    ou = rnd.random() * max(0.0, 1.0 - lu * s / UV_M)
    ov = rnd.random() * max(0.0, 1.0 - lv * s / UV_M)
    uvs = [(u0 + (ou + (c[0] - mu) * s / UV_M) * du, v0 + (ov + (c[1] - mv) * s / UV_M) * dv) for c in cs]
    b.face(pts, uvs, smooth=False)


def column(b, rnd, c, r, H, n=6, taper=0.84, jit=0.3, tilt=(0.0, 0.0), band=1.1, lean=(0.0, 0.0), guano=0.0, bottom=BOTTOM,
           xf=None, pts_out=None, beam=False):
    """A basalt column: centre c = (x, y), mean radius r, top at H over the water (ragged by jit, tilted by `tilt` = metres of rise per metre
    toward +X / +Y), leaning by `lean` (m per m of height over the water). beam: a horizontal one (built along local Z, no waterline band)
    and moved by `xf`."""
    ph = rnd.uniform(0, 6.28)
    ang = [ph + 2 * math.pi * (i + rnd.uniform(-0.12, 0.12)) / n for i in range(n)]
    rad = [r * rnd.uniform(0.88, 1.1) for _ in range(n)]
    ha = [H + rnd.uniform(-jit, jit) + (tilt[0] * math.cos(ang[i]) + tilt[1] * math.sin(ang[i])) * r for i in range(n)]
    hc = H + rnd.uniform(-jit, jit) * 0.6
    minh = min(ha)

    def P(i, z, k=None):
        t = (z - bottom) / max(H - bottom, 1e-6)
        k_ = lerp(1.0, taper, min(1.0, max(0.0, t)))
        p = V(c[0] + math.cos(ang[i]) * rad[i] * k_, c[1] + math.sin(ang[i]) * rad[i] * k_, z)
        if not beam:
            zz = max(z, 0.0)
            p = V(p.x + lean[0] * zz, p.y + lean[1] * zz, p.z)
        if xf:
            p = xf(p)
        if pts_out is not None:
            pts_out.append(p)
        return p

    levels = [bottom]
    bt = None
    if beam:
        if minh > 2.2:
            levels.append(bottom + (minh - bottom) * 0.5)
    elif minh > 0.5:
        bt = min(band * rnd.uniform(0.8, 1.3), 0.45 * minh)
        levels += [-0.6, bt]
        z = bt
        while z + 2.5 < minh - 0.8:
            z += rnd.uniform(2.1, 2.7)
            levels.append(z)
    for k in range(len(levels) - 1):
        z0, z1 = levels[k], levels[k + 1]
        tile = SALT if (bt is not None and z0 >= -0.6 - 1e-6 and z1 <= bt + 1e-6) else BASALT
        for i in range(n):
            j = (i + 1) % n
            pface(b, rnd, [P(i, z0), P(j, z0), P(j, z1), P(i, z1)], tile)
    zl = levels[-1]
    top = [P(i, ha[i]) for i in range(n)]
    for i in range(n):                                        # last course up to the ragged top
        j = (i + 1) % n
        pface(b, rnd, [P(i, zl), P(j, zl), top[j], top[i]], BASALT)
    axis = (xf(V(0, 0, 1)) - xf(V(0, 0, 0))).normalized() if xf else V(0, 0, 1)
    ctr = sum(top, Vector()) / n + axis * (hc - sum(ha) / n)      # the top's centre point, raised or sunk along the column's axis
    for i in range(n):
        j = (i + 1) % n
        tile = SALT if rnd.random() < guano else BASALT
        pface(b, rnd, [top[i], top[j], ctr], tile)
    bot = [P(i, bottom) for i in range(n)][::-1]
    pface(b, rnd, bot, BASALT)


def stack_group(b, rnd, cols, lean=(0.0, 0.0), grp=None, groups=None):
    for spec in cols:
        x, y, r, H = spec[:4]
        kw = dict(spec[4]) if len(spec) > 4 else {}
        pts = []
        column(b, rnd, (x, y), r, H, lean=kw.pop("lean", lean), pts_out=pts, **kw)
        if groups is not None:
            groups.setdefault(grp, []).extend(pts)


def boxes_from(groups, inset=0.15):
    out = []
    for name, pts in groups.items():
        pts = [p for p in pts if p.z >= -0.7]
        if not pts:                                      # never above the water (the arch's sill)
            continue
        zlo = min(p.z for p in pts)
        mn = V(min(p.x for p in pts), min(p.y for p in pts), -1.0 if zlo < -0.5 else zlo)      # a lintel's box starts at its underside
        mx = V(max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts))
        mn += V(inset, inset, 0.0)
        mx -= V(inset, inset, 0.0)
        if mx.x - mn.x < 0.3 or mx.y - mn.y < 0.3:
            continue
        out.append({"name": name, "center": [round((mn.x + mx.x) / 2, 3), round((mn.y + mx.y) / 2, 3), round((mn.z + mx.z) / 2, 3)],
                    "size": [round(mx.x - mn.x, 3), round(mx.y - mn.y, 3), round(mx.z - mn.z, 3)]})
    return out


def sea_stack(variant):
    b = Builder("sea_stack_v%d" % variant)
    rnd = random.Random(900 + variant)
    groups = {}
    if variant == 0:
        lean = (0.075, -0.02)
        core = [(0.0, 0.0, 1.35, 13.4, dict(jit=0.7, tilt=(0.5, -0.3), guano=0.5, taper=0.82)),
                (1.45, 0.55, 1.0, 12.1, dict(jit=0.55, tilt=(-0.4, 0.2), guano=0.3, taper=0.84)),
                (0.2, -1.55, 1.1, 12.9, dict(jit=0.5, tilt=(0.2, 0.5), guano=0.3, taper=0.84)),
                (-1.3, 0.75, 1.05, 10.4, dict(jit=0.5, guano=0.2, taper=0.86))]
        mid = [(-1.55, -0.95, 0.95, 8.2, dict(jit=0.6, tilt=(-0.5, 0.0), taper=0.88)),
               (1.65, -1.0, 0.9, 6.8, dict(jit=0.5, taper=0.9)),
               (0.1, 1.75, 0.9, 9.0, dict(jit=0.6, tilt=(0.0, -0.5), taper=0.88))]
        low = [(-2.1, 0.0, 0.85, 3.4, dict(jit=0.4, guano=0.5, taper=0.95)),
               (1.1, -2.2, 0.8, 2.5, dict(jit=0.3, guano=0.5, taper=0.95)),
               (-0.6, 2.35, 0.85, 3.6, dict(jit=0.4, guano=0.5, taper=0.95)),
               (2.2, 0.9, 0.8, 3.0, dict(jit=0.4, guano=0.5, taper=0.95))]
        stack_group(b, rnd, core, lean, "core", groups)
        stack_group(b, rnd, mid, lean, "mid", groups)
        stack_group(b, rnd, low, lean, "low", groups)
        note = ("a tall lone basalt pillar: ~ 14 m over the water, ~ 5.5 m across at the waterline, leaning ~ 4 degrees toward +X, a broken ragged top with "
                "guano, a ring of lower columns and ledges round the foot")
    elif variant == 1:
        la, lb = (-0.05, 0.02), (0.065, -0.03)
        tooth_a = [(-2.4, 0.0, 1.2, 9.0, dict(jit=0.5, tilt=(0.3, 0.0), guano=0.4, taper=0.82)),
                   (-3.4, 0.8, 0.8, 7.0, dict(jit=0.5, taper=0.86)),
                   (-1.55, 0.95, 0.9, 5.6, dict(jit=0.5, tilt=(0.4, 0.3), taper=0.88)),
                   (-2.85, -1.05, 0.9, 6.3, dict(jit=0.5, taper=0.86))]
        tooth_b = [(2.7, 0.4, 0.95, 5.0, dict(jit=0.5, tilt=(-0.4, 0.2), guano=0.4, taper=0.84)),
                   (3.4, -0.4, 0.7, 3.8, dict(jit=0.4, taper=0.88)),
                   (2.0, -0.5, 0.7, 3.1, dict(jit=0.4, taper=0.9))]
        base = [(0.0, 0.0, 1.3, 2.7, dict(jit=0.4, guano=0.5, taper=0.95)),
                (-0.3, -1.45, 1.1, 1.8, dict(jit=0.3, guano=0.4, taper=0.95)),
                (0.4, 1.4, 1.1, 2.2, dict(jit=0.3, guano=0.5, taper=0.95)),
                (1.0, -0.1, 1.0, 3.3, dict(jit=0.4, guano=0.5, taper=0.95))]
        stack_group(b, rnd, tooth_a, la, "tooth_a", groups)
        stack_group(b, rnd, tooth_b, lb, "tooth_b", groups)
        stack_group(b, rnd, base, (0.0, 0.0), "base", groups)
        note = ("a pair of basalt teeth sharing a base: ~ 9 m (west one, x -2.4) and ~ 5 m (x +2.7), ~ 8 m across the pair, leaning slightly apart; "
                "low shared foot ledges 2..3 m between them, guano on the tops")
    elif variant == 2:
        pier_l = [(-3.5, 0.0, 1.4, 5.5, dict(jit=0.6, tilt=(0.4, 0.0), guano=0.3, taper=0.86)),
                  (-4.0, 0.95, 1.0, 4.6, dict(jit=0.5, taper=0.88)),
                  (-2.8, -1.15, 1.0, 4.1, dict(jit=0.5, taper=0.9)),
                  (-2.9, 1.25, 0.9, 3.4, dict(jit=0.4, guano=0.4, taper=0.92))]
        pier_r = [(3.3, 0.2, 1.3, 4.9, dict(jit=0.6, tilt=(-0.3, 0.3), guano=0.4, taper=0.86)),
                  (3.9, -0.85, 1.0, 3.7, dict(jit=0.5, taper=0.9)),
                  (2.7, 1.1, 1.0, 4.3, dict(jit=0.5, taper=0.88)),
                  (4.1, 0.95, 0.8, 2.5, dict(jit=0.4, guano=0.4, taper=0.94))]
        low = [(0.0, 0.0, 1.5, -1.8, dict(jit=0.15, taper=1.0)),                       # the sill under the arch, never above the lowest tide
               (-0.9, 1.75, 0.8, 1.3, dict(jit=0.3, guano=0.4, taper=0.95)),
               (1.0, -1.65, 0.8, 1.1, dict(jit=0.3, guano=0.4, taper=0.95))]
        stack_group(b, rnd, pier_l, (0.0, 0.0), "pier_w", groups)
        stack_group(b, rnd, pier_r, (0.0, 0.0), "pier_e", groups)
        stack_group(b, rnd, low[1:2], (0.0, 0.0), "low_n", groups)
        stack_group(b, rnd, low[2:], (0.0, 0.0), "low_s", groups)
        stack_group(b, rnd, low[:1], (0.0, 0.0), "sill", groups)
        # the lintel: a horizontal column laid across the piers at z ~ 3.5 (its axis X, bottom side ~ 2.5 m over the water)
        L = 6.4
        p0 = V(-3.2, 0.1, 3.5)
        rot = Matrix.Rotation(math.radians(90), 3, "Y") @ Matrix.Rotation(0.0, 3, "Z")        # local +Z -> world +X
        xf = lambda p: p0 + rot @ p                                                          # noqa: E731
        pts = []
        column(b, rnd, (0.0, 0.0), 1.0, L, n=6, taper=0.9, jit=0.25, guano=0.0, bottom=0.0, xf=xf, pts_out=pts, beam=True)
        groups["lintel"] = pts
        note = ("a low wide basalt stack ~ 9.5 m long along X and ~ 6 m high with a natural arch: two piers (5 and 5.5 m) joined by a lintel, the opening ~ 3.2 m wide "
                "and ~ 2.5 m high at the waterline, the sea passes through; guano on the tops. Collision boxes: two piers and the lintel (the opening stays free)")
    else:
        sk = [("A", -4.2, 0.8, [(0.0, 0.0, 0.9, 3.0, dict(jit=0.35, tilt=(0.3, 0.2), guano=0.4, taper=0.88)),
                                (0.85, -0.6, 0.7, 2.2, dict(jit=0.3, taper=0.9)), (-0.6, 0.75, 0.6, 1.5, dict(jit=0.25, guano=0.4, taper=0.92))]),
              ("B", -0.8, -1.2, [(0.0, 0.0, 0.8, 2.0, dict(jit=0.3, tilt=(-0.3, 0.0), guano=0.4, taper=0.9)),
                                 (0.75, 0.5, 0.6, 1.3, dict(jit=0.25, taper=0.92))]),
              ("C", 2.6, 1.4, [(0.0, 0.0, 0.8, 2.6, dict(jit=0.35, tilt=(0.2, -0.3), guano=0.4, taper=0.88)),
                               (-0.75, -0.5, 0.65, 1.6, dict(jit=0.25, taper=0.92)), (0.6, -0.75, 0.55, 1.1, dict(jit=0.2, guano=0.3, taper=0.94))]),
              ("D", 5.0, -0.6, [(0.0, 0.0, 0.7, 1.6, dict(jit=0.3, guano=0.4, taper=0.92)), (0.6, 0.5, 0.5, 1.0, dict(jit=0.2, taper=0.94))])]
        for name, ox, oy, cols in sk:
            cols = [(x + ox, y + oy, r, H, kw) for (x, y, r, H, kw) in cols]
            stack_group(b, rnd, cols, ((0.02 if ox > 0 else -0.02), 0.0), "skerry_" + name, groups)
        # awash rubble between them: tall ellipsoids whose tops just break the surface and whose bottoms stay under the lowest tide
        for i, (x, y, sz) in enumerate(((-2.4, -0.7, (1.7, 1.4, 2.3)), (1.0, 0.5, (1.5, 1.3, 2.2)), (3.9, 0.8, (1.3, 1.1, 2.1)), (-5.6, -0.6, (1.2, 1.0, 2.0)))):
            rock(b, V(x, y, -0.4), sz, seed=40 + i, subdiv=1, flatten=0.0, rough=0.1, tile=BASALT)
        note = ("four small basalt skerries 1.5..3 m high (A 3.0 m at x -4.2, B 2.0 m, C 2.6 m, D 1.6 m at x +5), spread ~ 11 m along X, "
                "with four awash boulders between them (tops 0..0.5 m); guano on some tops")
    boxes = boxes_from(groups)
    return b, {"notes": note + "; origin on the water line under the middle (z 0 = sea at mid tide), rock to z -3 so tides of +-1 m show no bottom; "
                              "dark basalt (atlas 'stone_soot') with a pale salt band to ~ 1 m (atlas 'ash'); round, no front; no seats",
               "manifest": {"collision_boxes": boxes}}
