"""Stacked beam tower («Дом-мост»): late-Soviet brutalism, an original "space city" house.

Four raw-concrete bars (9 m wide, 26-30 m long) lie crosswise on top of each other: bar 1 along x (storeys 1-2),
bar 2 along y (3-4), bar 3 along x (5-6) and a lighter top bar along y: the public viewing gallery. Between the
bars are 3 m air voids; three vertical cores of different height (38 / 25 / 17 m) pierce them, two slender
ground piers and a few shear piers carry the rest. The ground floor is open (pilotis hall) with a glazed,
ENTERABLE lobby around the main core: reception counter, benches, wooden lift cladding, the street door.

Size 30 x 28 x 39.4 m (plot 40 x 40). Flats: 6 residential storeys, ~5 m modules on both long faces, each flat has
a living ribbon window and a small kitchen window (2 windows). Bar ends carry stair slits. Bar undersides are
closed soffits (dark concrete), the gallery has a timber soffit. Flat doors are on the corridor inside the bar
(not enterable tonight); everyone uses entrance "main".

Spots: lobby bench x2, lobby counter, "view" in the top gallery (z 32.4: reached by the lift).
Ideas for later: lit gallery interior, outdoor stair from the pilotis hall up to the bar 1 roof terrace, murals.
Material requests: none.
"""
import math
import random

from citykit import box, prism, ring, DOOR_W, DOOR_H, Facade, Mesh, wall_with_holes
from furn_int2 import Furn

INFO = {
    "name_ru": "Дом-мост (балочная башня)", "name_en": "Stacked Beam Tower",
    "style": "brutalism", "use": "residential",
    "inspired_by": "1970s Soviet / Georgian 'space city' ideas (Tbilisi linear and bridge houses), Kenzo Tange's "
                   "Yamanashi cores, Moshe Safdie stacking; massing, proportions and details are our own",
}

T = 0.4            # slab thickness
WALL = 0.32
MOD = 5.0


def slab_box(m, x0, x1, y0, y1, z0, z1, top="CT_Concrete", bot="CT_ConcreteDark", side="CT_Concrete"):
    box(m, side, x0, x1, y0, y1, z0, z1, faces="xXyY")
    box(m, top, x0, x1, y0, y1, z0, z1, faces="Z")
    box(m, bot, x0, x1, y0, y1, z0, z1, faces="z")


def _u_range(F, rect):
    xs = (rect[0], rect[1])
    ys = (rect[2], rect[3])
    us = [(x - F.p0[0]) * F.d[0] + (y - F.p0[1]) * F.d[1] for x in xs for y in ys]
    return min(us), max(us)


def bar(B, rect, z0, storeys, axis, mat, first_storey, cores, cnt, soffit="CT_ConcreteDark", end_mat=None,
        top_mat="CT_Gravel", gallery=False, rnd=None):
    x0, x1, y0, y1 = rect
    H = storeys * 3.0 if not gallery else 4.0
    z1 = z0 + H
    slab_box(B.m, x0, x1, y0, y1, z0, z0 + T, top="CT_Floor" if gallery else "CT_Concrete", bot=soffit)
    slab_box(B.m, x0 - 0.0, x1, y0, y1, z1 - T, z1, top=top_mat, bot="CT_ConcreteDark")
    if storeys == 2:
        slab_box(B.m, x0, x1, y0, y1, z0 + 3.0 - T / 2, z0 + 3.0 + T / 2)
    edges = [((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)), ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0))]
    long_idx = (0, 2) if axis == "x" else (1, 3)
    wz0, wz1 = z0 + T, z1 - T
    # flat counters per storey
    for ei, (p0, p1) in enumerate(edges):
        L = math.hypot(p1[0] - p0[0], p1[1] - p0[1])
        F = Facade(p0, p1)
        holes, wins, fins = [], [], []
        if ei in long_idx and not gallery:
            n = int(L // MOD)
            margin = (L - n * MOD) / 2
            for k in range(n):
                ua = margin + k * MOD
                ub = ua + MOD
                blocked = any(_ov(_u_range(F, c), (ua, ub)) for c in cores)
                if not blocked:
                    for s in range(storeys):
                        zb = z0 + s * 3.0
                        h = (zb + 1.0, zb + 2.4)
                        hv = (zb + 0.9, zb + 2.4)
                        holes.append((ua + 0.55, ua + 3.55, h[0], h[1]))
                        holes.append((ua + 3.95, ua + 4.65, hv[0], hv[1]))
                        wins.append((k, s, (ua + 0.55, ua + 3.55, h[0], h[1]), (ua + 3.95, ua + 4.65, hv[0], hv[1]), ua, ub))
                else:
                    for s in range(storeys):
                        zb = z0 + s * 3.0
                        holes.append((ua + 2.2, ua + 2.8, zb + 0.8, zb + 2.4))
                        wins.append((None, s, (ua + 2.2, ua + 2.8, zb + 0.8, zb + 2.4), None, ua, ub))
                fins.append(ua)
            fins.append(margin + n * MOD)
        elif ei in long_idx and gallery:
            n = int(L // 3.0)
            m0 = (L - n * 3.0) / 2
            for k in range(n):
                ua = m0 + k * 3.0
                holes.append((ua + 0.4, ua + 2.6, z0 + 0.9, z1 - 0.9))
                wins.append((None, 0, (ua + 0.4, ua + 2.6, z0 + 0.9, z1 - 0.9), None, ua, ua + 3.0))
        else:
            # end wall: stair slits per storey, one big opening for the gallery
            if gallery:
                holes.append((1.0, L - 1.0, z0 + 0.9, z1 - 0.9))
                wins.append((None, 0, holes[-1], None, 0, L))
            else:
                for s in range(storeys):
                    zb = z0 + s * 3.0
                    for uc in (L * 0.3, L * 0.7):
                        holes.append((uc - 0.3, uc + 0.3, zb + 0.8, zb + 2.4))
                        wins.append((None, s, holes[-1], None, 0, 0))
        wmat = end_mat if (end_mat and ei not in long_idx) else mat
        F2 = B.facade(p0, p1, wz0, wz1, wmat, holes=holes, depth=WALL)
        for (k, s, h1, h2, ua, ub) in wins:
            if gallery or k is None:
                B.facade_window(F2, h1[0], h1[1], h1[2], h1[3], kind="public" if gallery else "stair", inset=0.18)
            else:
                storey = first_storey + s
                key = (storey, ei, k)
                if key not in cnt["ids"]:
                    cnt[storey] = cnt.get(storey, 0) + 1
                    fid = "%d-%02d" % (storey, cnt[storey])
                    cnt["ids"][key] = fid
                    mid = F2.to_building(((ua + ub) / 2, -(x1 - x0 if axis == "y" else y1 - y0) / 2, h1[2] - 0.9))
                    B.flat(fid, storey=storey, beds=rnd.choice((1, 2, 2, 3)), door=mid, entrance="main")
                fid = cnt["ids"][key]
                B.facade_window(F2, h1[0], h1[1], h1[2], h1[3], flat=fid, cols=3, rows=1, inset=0.18)
                B.facade_window(F2, h2[0], h2[1], h2[2], h2[3], flat=fid, inset=0.18)
        # fins and slab brows (outside relief)
        lm = Mesh()
        for ua in fins:
            box(lm, "CT_Concrete", ua - 0.13, ua + 0.13, -0.05, 0.5, wz0, wz1)
        if ei in long_idx and not gallery:
            for zz in ([z0 + 3.0] if storeys == 2 else []):
                box(lm, "CT_ConcreteDark", 0, L, -0.05, 0.22, zz - 0.22, zz + 0.22)
        B.m.extend(F2.apply(lm))
        lm2 = Mesh()
        box(lm2, "CT_ConcreteDark", -0.0, L, -0.05, 0.3, z1 - T - 0.25, z1 - T + 0.02) if False else None
    return z1


def _ov(a, b):
    return a[0] < b[1] - 0.1 and a[1] > b[0] + 0.1


def _furnish_lobby(B, lx0, lx1, ly0, ly1, lh):
    """Round 2: lobby (inner x -6.7..6.7, y 3.3..14.7, ceiling 4.2): reception dressing, mailboxes, benches, plants, lamps."""
    f = Furn(B)
    d = B.detail
    W = "#FFE0B8"
    ix0, ix1, iy0, iy1 = lx0 + 0.3, lx1 - 0.3, ly0 + 0.3, ly1 - 0.3
    # reception desk: fascia, monitor, bell, parcels, a lamp; the clerk's chair behind
    box(d, "CT_SteelRed", 3.25, 6.35, 12.17, 12.2, 0.15, 0.95, faces="y")
    box(d, "CT_Steel", 3.7, 4.2, 12.55, 12.9, 1.14, 1.45)
    box(d, "CT_Frame", 3.65, 4.25, 12.58, 12.62, 1.2, 1.5)
    f.cup(5.6, 12.4, 1.14, mat="CT_PlasterYellow", h=0.06)
    f.cup(5.95, 12.55, 1.14, mat="CT_TileWhite", h=0.12)
    box(d, "CT_Wood", 4.6, 5.1, 12.6, 12.9, 1.14, 1.28)
    f.chair(4.8, 13.7, yaw=0.0, seat="CT_PlasterRed")
    f.shelf(5.85, 14.5, w=1.3, d=0.35, h=2.0, yaw=0.0, rows=4, seed=3)                    # pigeonholes behind
    # mailboxes (the residents' post) on the east wall between the windows
    for r in range(6):
        for c in range(4):
            box(d, ("CT_Steel", "CT_Aluminium", "CT_SteelRed")[(r + c) % 3], 6.62, 6.7, 8.2 + c * 0.4, 8.2 + c * 0.4 + 0.36, 0.9 + r * 0.2, 0.9 + r * 0.2 + 0.17, faces="x")
    f.panel(ix0, 9.0, 1.7, 1.6, 1.0, 90.0, mat="CT_PlasterGreen", frame="CT_Wood")        # notice board on the west wall
    f.bench(-6.2, 9.0, yaw=90.0, w=1.6, back=False)
    B.spot("lobby_bench_n", "bench", (-6.2, 9.0, 0.0), 90.0)
    f.bench(-6.2, 13.6, yaw=90.0, w=1.4, back=False)
    B.spot("lobby_bench_nw", "bench", (-6.2, 13.6, 0.0), 90.0)
    # lift: call button, floor indicator, a carpet runner, doormat, plants
    box(d, "CT_Steel", 1.5, 1.6, 6.1, 6.15, 1.0, 1.3)
    box(d, "CT_SteelRed", 1.53, 1.57, 6.07, 6.1, 1.1, 1.14)
    box(d, "CT_PlasterRed", -1.0, 1.0, 3.3, 6.05, 0.0, 0.012, faces="Z")
    box(d, "CT_PlasterGrey", -1.1, 1.1, 3.3, 4.2, 0.012, 0.024, faces="Z")
    for px, py, pot in ((-3.0, 3.9, "CT_PlasterRed"), (3.0, 3.9, "CT_TileWhite"), (-6.1, 5.4, "CT_TileWhite"),
                        (6.0, 6.0, "CT_PlasterRed"), (-3.2, 14.2, "CT_PlasterRed"), (6.1, 14.3, "CT_TileWhite")):
        f.plant(px, py, h=1.9, pot=pot)
    f.border(ix0, ix1, iy0, iy1, w=0.4, mat="CT_ConcreteDark")
    for seg in (((ix0, iy1), (ix0, iy0)), ((ix0, iy0), (-1.1, iy0)), ((1.1, iy0), (ix1, iy0)), ((ix1, iy0), (ix1, iy1)),
                ((ix1, iy1), (ix0, iy1))):
        f.skirting(list(seg), h=0.14)
    for x, y in ((-4.2, 5.0), (4.2, 5.0), (0.0, 4.6), (-4.4, 11.5), (4.8, 10.0), (0.0, 13.2)):
        B.lamp((x, y, lh - 0.3), lumens=800, colour=W, kind="public", radius=7)


def build(B):
    rnd = random.Random(1974)
    cnt = {"ids": {}}
    # cores: (name, cx, cy, top)
    cores = [("A", 0.0, 9.0, 37.2), ("B", -9.0, 23.0, 17.2), ("C", 9.0, 23.0, 24.6)]
    R = 2.5
    # bars: rect, z0
    b1 = (-15.0, 15.0, 18.5, 27.5)
    b2 = (-4.5, 4.5, 2.0, 30.0)
    b3 = (-14.0, 14.0, 4.5, 13.5)
    b4 = (-4.5, 4.5, 3.0, 30.0)
    z1b, z2b, z3b, z4b = 5.0, 14.0, 23.0, 32.0
    solids = [(b1, z1b, z1b + 6), (b2, z2b, z2b + 6), (b3, z3b, z3b + 6), (b4, z4b, z4b + 4)]
    core_rects = {n: (cx - R, cx + R, cy - R, cy + R) for n, cx, cy, t in cores}
    bar(B, b1, z1b, 2, "x", "CT_Concrete", 1, [core_rects["B"], core_rects["C"]], cnt, end_mat="CT_PlasterRed", rnd=rnd)
    bar(B, b2, z2b, 2, "y", "CT_Concrete", 3, [core_rects["A"]], cnt, rnd=rnd)
    bar(B, b3, z3b, 2, "x", "CT_Concrete", 5, [core_rects["A"]], cnt, rnd=rnd)
    bar(B, b4, z4b, 1, "y", "CT_ConcreteWhite", 0, [], cnt, soffit="CT_Wood", gallery=True, top_mat="CT_Roof", rnd=rnd)
    # piers
    slab_box(B.m, -2.8, 2.8, 20.5, 25.5, z1b + 6, z2b, top="CT_Concrete", bot="CT_ConcreteDark")
    for sx in (-11.0, 11.0):
        slab_box(B.m, sx - 1.2, sx + 1.2, 7.0, 11.0, 0.0, z3b, top="CT_Concrete", bot="CT_ConcreteDark")
    slab_box(B.m, -0.9, 0.9, 22.0, 27.0, z2b + 6, z4b, top="CT_Concrete", bot="CT_ConcreteDark")
    slab_box(B.m, -3.2, 3.2, 20.0, 28.0, z2b + 6, z2b + 6.0 + 0.0001) if False else None
    # cores with stair slits
    for n, cx, cy, top in cores:
        box(B.m, "CT_Concrete", cx - R, cx + R, cy - R, cy + R, 0.0, top, faces="xXyYz")
        box(B.m, "CT_ConcreteDark", cx - R - 0.3, cx + R + 0.3, cy - R - 0.3, cy + R + 0.3, top, top + 0.4)
        faces = [("y", cx, cy - R), ("Y", cx, cy + R), ("x", cx - R, cy), ("X", cx + R, cy)]
        for side, fx, fy in faces:
            z = 3.0 if n == "A" else 7.0 if n == "B" else 4.0
            zs = []
            zz = 6.0
            while zz + 1.6 < top - 0.8:
                zs.append(zz)
                zz += 3.0
            for zz in zs:
                px, py = (fx, fy)
                if side in "yY":
                    pts = [(px, py + (-0.05 if side == "y" else 0.05))]
                inside = any(r[0] - 0.1 < px < r[1] + 0.1 and r[2] - 0.1 < py < r[3] + 0.1 and za - 0.3 < zz + 0.8 < zb + 0.3
                             for r, za, zb in solids if not (n == "A" and False))
                # a slit's centre point is outside a bar if the slit is in a void
                inside = any((r[0] - 0.4 < px < r[1] + 0.4 and r[2] - 0.4 < py < r[3] + 0.4) and (za - 0.6 < zz + 0.8 and zz < zb + 0.6)
                             for r, za, zb in solids)
                if inside or (n == "A" and zz < 5.0):
                    continue
                w = 0.28
                c0, c1 = zz, zz + 1.6
                if side == "y":
                    q = [(cx - w, fy, c0), (cx + w, fy, c0), (cx + w, fy, c1), (cx - w, fy, c1)]
                elif side == "Y":
                    q = [(cx + w, fy, c0), (cx - w, fy, c0), (cx - w, fy, c1), (cx + w, fy, c1)]
                elif side == "x":
                    q = [(fx, cy + w, c0), (fx, cy - w, c0), (fx, cy - w, c1), (fx, cy + w, c1)]
                else:
                    q = [(fx, cy - w, c0), (fx, cy + w, c0), (fx, cy + w, c1), (fx, cy - w, c1)]
                B.window(q, kind="stair", inset=0.04, frame=0.04)
    # roof machine room on the main core, tank on core C
    box(B.m, "CT_ConcreteDark", -2.0, 2.0, 7.2, 10.8, 37.6, 39.4)
    box(B.m, "CT_Steel", -1.0, 1.0, 7.8, 10.2, 39.4, 39.6)
    box(B.m, "CT_ConcreteDark", 7.5, 10.5, 21.5, 24.5, 25.0, 26.2)
    # ----------------------------------------------------------------- the lobby (enterable) around core A
    lx0, lx1, ly0, ly1, lh = -7.0, 7.0, 3.0, 15.0, 4.2
    box(B.m, "CT_Floor", lx0, lx1, ly0, ly1, -0.3, 0.0, faces="Z")
    box(B.m, "CT_ConcreteDark", lx0 - 0.3, lx1 + 0.3, ly0 - 0.3, ly1 + 0.3, -0.3, 0.0, faces="xXyYz")
    # roof slab with interior ceiling
    box(B.m, "CT_Concrete", lx0 - 0.4, lx1 + 0.4, ly0 - 1.0, ly1 + 0.4, lh, lh + 0.4, faces="xXyYZ")
    box(B.m, "CT_ConcreteDark", lx0 - 0.4, lx1 + 0.4, ly0 - 1.0, ly1 + 0.4, lh, lh + 0.4, faces="z")
    # ceiling inside (just below the slab)
    B.m.face("CT_Interior", [(lx0, ly1, lh), (lx1, ly1, lh), (lx1, ly0, lh), (lx0, ly0, lh)][::-1] if False else
             [(lx0, ly0, lh - 0.001), (lx1, ly0, lh - 0.001), (lx1, ly1, lh - 0.001), (lx0, ly1, lh - 0.001)][::-1])
    lob = [((lx0, ly0), (lx1, ly0)), ((lx1, ly0), (lx1, ly1)), ((lx1, ly1), (lx0, ly1)), ((lx0, ly1), (lx0, ly0))]
    lobholes = [
        [(0.6, 5.6, 0.35, 3.7), (7 - DOOR_W, 7 + DOOR_W, 0.0, 2.45), (8.4, 13.4, 0.35, 3.7)],
        [(1.5, 5.0, 0.5, 3.6), (7.0, 10.5, 0.5, 3.6)],
        [(2.0, 6.0, 0.6, 3.4), (8.0, 12.0, 0.6, 3.4)],
        [(1.5, 5.0, 0.5, 3.6), (7.0, 10.5, 0.5, 3.6)],
    ]
    lobwin = [[True, False, True], [True, True], [True, True], [True, True]]
    # holes list is in walking order (u); windows for each hole flagged
    for ei, (p0, p1) in enumerate(lob):
        F = B.facade(p0, p1, 0.0, lh, "CT_Concrete", holes=lobholes[ei], depth=0.3, back=False)
        L = F.length
        for hi, h in enumerate(lobholes[ei]):
            if lobwin[ei][hi]:
                B.facade_window(F, h[0], h[1], h[2], h[3], kind="public", cols=3 if ei == 0 else 2, rows=1, inset=0.15)
        # inner skin facing the room: walk the reversed edge (p1 -> p0) so the right side is the room
        q0 = _toward(p0, p1, 0.3, lob, ei)
        q1 = _toward(p1, p0, 0.3, lob, ei, rev=True)
        Fi = Facade(q1, q0)
        mi = Mesh()
        ih = [(L - h[1] + 0.0, L - h[0] + 0.0, h[2], h[3]) for h in lobholes[ei]]
        # inner skin length differs slightly (corner overlap): same inner positions, so rebuild holes in its own u
        Li = Fi.length
        ih = [(Li - (h[1] - 0.3), Li - (h[0] - 0.3), h[2], h[3]) for h in lobholes[ei]]
        ih = [(max(0.0, a), min(Li, b), c, d) for a, b, c, d in ih]
        wall_with_holes(mi, "CT_Interior", Li, 0.0, lh, ih)
        B.m.extend(Fi.apply(mi))
    B.entrance("main", (0.0, ly0, 0.0), 0.0, width=DOOR_W * 2, leaves=2, kind="street")
    # lobby furniture: wood-clad lift front on the core, counter
    box(B.m, "CT_Wood", -2.5, 2.5, 6.15, 6.5, 0.0, 3.2, faces="xXyYZ")
    box(B.m, "CT_Wood", 3.2, 6.4, 12.2, 13.0, 0.0, 1.1)
    box(B.m, "CT_Steel", 3.2, 6.4, 12.2, 13.0, 1.1, 1.14)                 # solid since round 2: people walk round it
    B.spot("lobby_counter", "counter", (4.8, 11.8, 0.0), 180.0)           # the customer faces +y, to the desk
    B.spot("lobby_bench_w", "bench", (-5.0, 4.5, 0.0), 0.0)
    B.spot("lobby_bench_e", "bench", (5.0, 4.5, 0.0), 0.0)
    for bx in (-5.5, 5.5):
        box(B.m, "CT_Wood", bx - 1.0, bx + 1.0, 4.1, 4.55, 0.0, 0.45)
    _furnish_lobby(B, lx0, lx1, ly0, ly1, lh)
    # canopy over the street door
    box(B.m, "CT_Wood", -3.0, 3.0, 0.0, 3.0, 3.35, 3.6, faces="xXyYZz")
    B.spot("view", "view", (0.0, 16.0, z4b + 0.4), 0.0)
    # pilotis hall paving
    box(B.m, "CT_Paving", -15, 15, 0.0, 30.0, -0.05, -0.01, faces="Z") if False else None
    B.footprint = [(-15.0, 2.0), (15.0, 2.0), (15.0, 30.0), (-15.0, 30.0)]
    B.height = 39.6
    nf = len(B.flats)
    B.note("%d flats on 6 storeys, 4 crossing bars; main lobby + gallery 'view' spot at z=%.1f" % (nf, z4b + 0.4))


def _toward(p, q, d, poly, ei, rev=False):
    """Inner corner point of the wall p->q moved inward by d (rectangle CCW)."""
    xs = [a[0] for a in (poly[0][0], poly[1][0], poly[2][0], poly[3][0])]
    ys = [a[1] for a in (poly[0][0], poly[1][0], poly[2][0], poly[3][0])]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    x = p[0] + (d if p[0] < cx else -d)
    y = p[1] + (d if p[1] < cy else -d)
    return (x, y)
