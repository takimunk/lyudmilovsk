"""Wave Tower ("Башня-Волна"): the landmark of the town seen from the sea. An ORIGINAL design in the spirit
of 1950s-60s Brazilian modernist residential towers (Sao Paulo / Rio: continuous undulating horizontal
brise-soleil, azulejo-tiled cores and gables, a shopping arcade under a glazed base); not a copy of any building.

Plot 80 m (front, x) x 22 m deep (y), height 58 m.
Massing: a slab 12 m deep, 75 m long, whose plan is one gentle S-curve (yc(x) = 10.4 + 3 sin(2 pi x / 75)).
  * Ground floor (z 0..3.5): an ENTERABLE arcade 3.7 m deep along the whole curved front under the overhanging
    mezzanine, round columns, paving; behind it six hollow shops (glazed fronts with door holes, interior walls, floor,
    ceiling, counters), a through passage (blue-tiled) from the arcade to the back, and three lobbies of the
    lift/stair cores with street doors.
  * Mezzanine (z 3.5..6.5): glazed band of "public" windows (not enterable).
  * 16 flat storeys (storeys 2..17, floor-to-floor 3.0 m, z 6.5..54.5): the whole sea-facing front is wrapped by
    continuous white-concrete sun-breaker bands at every floor level that follow the S-curve and whose fronts
    undulate (phase shifts storey by storey, so the facade ripples). The window wall is set 1.3 m behind the bands;
    one window card per 2.5 m bay. Cores (3: x = -25, 0, 25) are clad in blue tile (CT_Tile) with narrow stair windows;
    the end gables are blue tile too, with side windows of the end flats.
  * Flats: each storey carries 12-14 flats of 2-4 bays (studios, 2-room, 3-room), all with 2-4 front windows and one
    small back window per bay; flat doors are at the nearest core.
  * Roof: three plant rooms over the cores, a water tank, a roof terrace (timber deck) under an undulating thin
    concrete canopy ("wave") on posts, a rail round the roof.
Enterable: arcade, passage, shops, lobbies. Spots: shop counters, arcade benches, a roof view point.
Ideas for later: shop signs with real text, cobogo screens in the passage, tiled mural on the gables, real lift cabs.
Material requests: none (uses CT_ConcreteWhite, CT_Tile, CT_Wood, CT_Neon...).
"""
import math
import random

from citykit import box, prism, DOOR_W, DOOR_H

INFO = {
    "name_ru": "Башня-Волна", "name_en": "Wave Tower",
    "style": "brazilian modernism", "use": "mixed",
    "inspired_by": "1950s-60s Sao Paulo / Rio residential towers with undulating brise-soleil, azulejo cores, "
                   "shopping arcades under glazed bases (Niemeyer / Reidy / Artigas era); original design",
}

N = 30                      # bays
STEP = 2.5
X0 = -N * STEP / 2          # -37.5
XS = [X0 + i * STEP for i in range(N + 1)]
DEPTH = 12.0
H_G, H_M, H_F = 3.5, 3.0, 3.0
Z_FLAT0 = 6.5
N_FLAT_ST = 16
Z_ROOF = Z_FLAT0 + H_F * N_FLAT_ST      # 54.5
ROOF_T = 0.4
CORES = [(4, 6, "core1"), (14, 16, "core2"), (24, 26, "core3")]
GROUPS = [(0, 4), (6, 14), (16, 24), (26, 30)]
UNITS = [(0, 4, "shop_a"), (4, 6, "core1"), (6, 9, "shop_b"), (9, 11, "passage"), (11, 14, "shop_c"),
         (14, 16, "core2"), (16, 20, "shop_d"), (20, 24, "shop_e"), (24, 26, "core3"), (26, 30, "shop_f")]


def yc(x):
    return 10.4 + 3.0 * math.sin(2 * math.pi * x / 75.0)


def yf(x):
    return yc(x) - DEPTH / 2


def yb(x):
    return yc(x) + DEPTH / 2


def slope(x):
    return 3.0 * 2 * math.pi / 75.0 * math.cos(2 * math.pi * x / 75.0)


def is_core(i):
    return any(a <= i < b for a, b, _ in CORES)


def _area(pts):
    return sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1] for i in range(len(pts)))


def sweep(m, mat, xs, base, section, caps=True):
    """Solid swept along +x: section(x) -> [(dy, z)] (convex, same count), y = base(x) + dy."""
    secs = [[(base(x) + dy, z) for dy, z in section(x)] for x in xs]
    ccw = _area(secs[0]) > 0
    n = len(secs[0])
    for j in range(len(xs) - 1):
        for i in range(n):
            k = (i + 1) % n
            A = (xs[j], secs[j][i][0], secs[j][i][1])
            Bq = (xs[j + 1], secs[j + 1][i][0], secs[j + 1][i][1])
            C = (xs[j + 1], secs[j + 1][k][0], secs[j + 1][k][1])
            D = (xs[j], secs[j][k][0], secs[j][k][1])
            m.face(mat, [A, D, C, Bq] if ccw else [A, Bq, C, D])
    if caps:
        for j, sign in ((0, -1), (len(xs) - 1, 1)):
            pts = [(xs[j], y, z) for y, z in secs[j]]
            if (sign > 0) != ccw:
                pts = pts[::-1]
            m.face(mat, pts)


def fwin(B, F, u0, u1, z0, z1, **kw):
    """Window card in a hole of facade F (corrects citykit.facade_window's inward-facing corner order)."""
    pts = [F.to_building((u, 0.0, z)) for u, z in ((u0, z0), (u1, z0), (u1, z1), (u0, z1))]
    kw.setdefault("inset", 0.2)
    return B.window(pts, **kw)


def _compose(rnd, n):
    out = []
    while n > 0:
        opts = [s for s in (2, 3, 4) if s <= n and (n - s == 0 or n - s >= 2)]
        s = rnd.choice(opts)
        out.append(s)
        n -= s
    return out


def build(B):
    rnd = random.Random(1958)
    zt = Z_ROOF
    chord = lambda i, d: (XS[i], yf(XS[i]) + d)
    front_line = lambda d: [chord(i, d) for i in range(N + 1)]
    back_line = [(XS[i], yb(XS[i])) for i in range(N + 1)]

    # ---------------------------------------------------------------- slabs
    arc = front_line(-1.8) + [(XS[i], yf(XS[i]) + 2.5) for i in range(N, -1, -1)]
    prism(B.m, "CT_Paving", arc, -0.4, 0.0, top=True, bottom=True, side_mat="CT_ConcreteDark")
    inner = front_line(2.5) + back_line[::-1]
    prism(B.m, "CT_Floor", inner, -0.4, 0.0, top=True, bottom=True, side_mat="CT_ConcreteDark")
    mezz = front_line(0.0) + back_line[::-1]
    prism(B.m, "CT_ConcreteDark", mezz, 3.2, 3.5, top=True, bottom=True)
    prism(B.m, "CT_Roof", mezz, zt, zt + ROOF_T, top=True, bottom=True, side_mat="CT_ConcreteWhite")

    # ---------------------------------------------------------------- brise-soleil bands (front)
    fine = [X0 + k * (N * STEP) / 120.0 for k in range(121)]
    for j in range(N_FLAT_ST + 2):
        zs = H_G + H_F * j
        top = 0.75
        ph = 0.55 * j

        def sec(x, zs=zs, ph=ph, top=top):
            out = 1.6 + 0.32 * math.sin(2 * math.pi * x / 11.0 + ph)
            return [(0.2, zs - 0.35), (-out, zs - 0.35), (-out, zs + top), (0.2, zs + 0.25)]
        sweep(B.m, "CT_ConcreteWhite", fine, yf, sec)

    # ---------------------------------------------------------------- flats layout
    flats = []                       # (storey, n, a, b)
    seg_flat = {}                    # (storey, seg) -> fid
    for s in range(2, 2 + N_FLAT_ST):
        n = 0
        for ga, gb in GROUPS:
            a = ga
            for size in _compose(rnd, gb - ga):
                n += 1
                fid = "%d-%02d" % (s, n)
                flats.append((s, n, a, a + size, fid))
                for i in range(a, a + size):
                    seg_flat[(s, i)] = fid
                a += size

    # ---------------------------------------------------------------- front facade (upper, mezz + flats)
    for i in range(N):
        x0, x1 = XS[i], XS[i + 1]
        p0, p1 = (x0, yf(x0)), (x1, yf(x1))
        L = math.dist(p0, p1)
        holes = []
        core = is_core(i)
        first_core = core and (i == min(a for a, b, _ in CORES if a <= i < b))
        if not core:
            holes.append((0.25, L - 0.25, 4.2, 6.15))
        elif first_core:
            holes.append((0.8, L - 0.8, 4.4, 6.0))
        for s in range(2, 2 + N_FLAT_ST):
            zs = Z_FLAT0 + H_F * (s - 2)
            if not core:
                holes.append((0.25, L - 0.25, zs + 0.7, zs + 2.55))
            elif first_core:
                holes.append((0.8, L - 0.8, zs + 0.8, zs + 2.3))
        F = B.facade(p0, p1, 3.5, zt, "CT_Tile" if core else "CT_ConcreteWhite", holes=holes, depth=0.3)
        if not core:
            fwin(B, F, 0.25, L - 0.25, 4.2, 6.15, kind="public", cols=2, rows=1, frame=0.06)
        elif first_core:
            fwin(B, F, 0.8, L - 0.8, 4.4, 6.0, kind="stair", frame=0.05)
        for s in range(2, 2 + N_FLAT_ST):
            zs = Z_FLAT0 + H_F * (s - 2)
            if not core:
                fwin(B, F, 0.25, L - 0.25, zs + 0.7, zs + 2.55, flat=seg_flat[(s, i)], kind="flat", cols=2, rows=1, frame=0.05)
            elif first_core:
                fwin(B, F, 0.8, L - 0.8, zs + 0.8, zs + 2.3, kind="stair", frame=0.05)

    # ---------------------------------------------------------------- back facade (upper + ground)
    passage_segs = set(range(9, 11))
    for i in range(N):
        x0, x1 = XS[i], XS[i + 1]
        p0, p1 = (x1, yb(x1)), (x0, yb(x0))
        L = math.dist(p0, p1)
        core = is_core(i)
        first_core = core and (i == min(a for a, b, _ in CORES if a <= i < b))
        holes = []
        for s in range(2, 2 + N_FLAT_ST):
            zs = Z_FLAT0 + H_F * (s - 2)
            if not core:
                holes.append((0.6, L - 0.6, zs + 0.95, zs + 2.35))
            elif first_core:
                holes.append((0.85, L - 0.85, zs + 0.8, zs + 2.3))
        F = B.facade(p0, p1, 3.5, zt, "CT_Tile" if core else "CT_Plaster", holes=holes, depth=0.3)
        for s in range(2, 2 + N_FLAT_ST):
            zs = Z_FLAT0 + H_F * (s - 2)
            if not core:
                fwin(B, F, 0.6, L - 0.6, zs + 0.95, zs + 2.35, flat=seg_flat[(s, i)], kind="flat", cols=2, rows=1, frame=0.05)
            elif first_core:
                fwin(B, F, 0.85, L - 0.85, zs + 0.8, zs + 2.3, kind="stair", frame=0.05)
        if i not in passage_segs:
            B.facade(p0, p1, 0.0, 3.5, "CT_ConcreteWhite", depth=0.3)

    # ---------------------------------------------------------------- end gables (blue tile, side windows)
    for xe, left in ((X0, True), (-X0, False)):
        p0, p1 = ((xe, yb(xe)), (xe, yf(xe))) if left else ((xe, yf(xe)), (xe, yb(xe)))
        L = DEPTH
        holes = []
        for s in range(2, 2 + N_FLAT_ST):
            zs = Z_FLAT0 + H_F * (s - 2)
            holes.append((L / 2 - 0.8, L / 2 + 0.8, zs + 0.9, zs + 2.4))
        F = B.facade(p0, p1, 0.0, zt, "CT_Tile", holes=holes, depth=0.3)
        for s in range(2, 2 + N_FLAT_ST):
            zs = Z_FLAT0 + H_F * (s - 2)
            fid = seg_flat[(s, 0 if left else N - 1)]
            fwin(B, F, L / 2 - 0.8, L / 2 + 0.8, zs + 0.9, zs + 2.4, flat=fid, kind="flat", cols=2, rows=1, frame=0.05)

    # ---------------------------------------------------------------- flats data
    for s, n, a, b, fid in flats:
        zs = Z_FLAT0 + H_F * (s - 2)
        best = None
        for ca, cb, cid in CORES:
            for edge in (a, b):
                for ce in (ca, cb):
                    d = abs(XS[edge] - XS[ce])
                    if best is None or d < best[0]:
                        best = (d, XS[edge], cid)
        xd = best[1]
        B.flat(fid, storey=s, beds={2: 1, 3: 2, 4: 3}[b - a], door=(xd, yc(xd), zs + 0.1), entrance=best[2])

    # ---------------------------------------------------------------- arcade columns, benches, signs
    for i in range(0, N + 1, 2):
        B.column_round(XS[i], yf(XS[i]) - 0.65, 0.0, 3.2, 0.22, mat="CT_ConcreteWhite", segs=12)
    for bx in (-33.0, 17.5):
        by = yf(bx) + 1.2
        box(B.detail, "CT_Wood", bx - 0.9, bx + 0.9, by, by + 0.45, 0.4, 0.5)
        box(B.detail, "CT_Steel", bx - 0.8, bx - 0.7, by + 0.05, by + 0.4, 0.0, 0.4)
        box(B.detail, "CT_Steel", bx + 0.7, bx + 0.8, by + 0.05, by + 0.4, 0.0, 0.4)
        B.spot("arcade_bench_%d" % int(bx), "bench", (bx, by + 0.7, 0.0), 0.0)

    # ---------------------------------------------------------------- ground units
    for a, b, name in UNITS:
        if name == "passage":
            continue
        core = name.startswith("core")
        for i in range(a, b):
            x0, x1 = XS[i], XS[i + 1]
            p0, p1 = (x0, yf(x0) + 2.5), (x1, yf(x1) + 2.5)
            L = math.dist(p0, p1)
            door_seg = a + (b - a - 1) // 2 if not core else a
            holes = []
            if i == door_seg:
                holes += [(L / 2 - DOOR_W / 2 - 0.05, L / 2 + DOOR_W / 2 + 0.05, 0.0, 2.3),
                          (L / 2 - DOOR_W / 2 - 0.05, L / 2 + DOOR_W / 2 + 0.05, 2.5, 3.05)]
            else:
                holes.append((0.2, L - 0.2, 0.45 if not core else 0.7, 3.05 if not core else 2.8))
            F = B.facade(p0, p1, 0.0, 3.2, "CT_Plaster" if not core else "CT_Tile", holes=holes, depth=0.3)
            kind = "stair" if core else "public"
            if i == door_seg:
                fwin(B, F, L / 2 - DOOR_W / 2 - 0.05, L / 2 + DOOR_W / 2 + 0.05, 2.5, 3.05, kind=kind, frame=0.04, inset=0.15)
                xm = (x0 + x1) / 2
                B.entrance("shop_door_" + name if not core else name, (xm, yf(xm) + 2.5, 0.0),
                           round(math.degrees(math.atan(slope(xm))), 1), width=DOOR_W, height=DOOR_H,
                           kind="street", leaves=1)
                if not core:  # blade sign on a rod
                    box(B.detail, "CT_Steel", xm - 0.02, xm + 0.02, yf(xm) + 1.0, yf(xm) + 1.9, 3.12, 3.2)
                    box(B.detail, "CT_Neon", xm - 0.05, xm + 0.05, yf(xm) + 1.1, yf(xm) + 1.9, 2.55, 3.05)
            else:
                fwin(B, F, 0.2, L - 0.2, 0.45 if not core else 0.7, 3.05 if not core else 2.8, kind=kind, cols=2 if L > 2 else 1, rows=1, frame=0.05, inset=0.15)
        if not core:
            xm = (XS[a] + XS[b]) / 2
            ycn = yb(xm) - 4.2
            box(B.m, "CT_Wood", xm - 1.6, xm + 1.6, ycn, ycn + 0.7, 0.0, 1.0)
            B.spot(name + "_counter", "counter", (xm, ycn - 0.1, 0.0), 0.0)
    # partitions between units (the passage sides are blue tile)
    bounds = sorted({a for a, b, _ in UNITS} | {b for a, b, _ in UNITS})
    for k in bounds:
        if k in (0, N):
            continue
        x = XS[k]
        names = {n: (a, b) for a, b, n in UNITS}
        mat = "CT_Tile" if any(n == "passage" and (a == k or b == k) for a, b, n in UNITS) else "CT_Interior"
        box(B.m, mat, x - 0.15, x + 0.15, yf(x) + 2.7, yb(x) - 0.2, 0.0, 3.2)

    # ---------------------------------------------------------------- roof
    ring = front_line(0.4)[:] + [(XS[-1] - 0.4, yf(XS[-1]) + 0.4)]
    pts = [(XS[0] + 0.4, yf(XS[0]) + 0.4)] + front_line(0.4)[1:-1] + [(XS[-1] - 0.4, yf(XS[-1]) + 0.4),
           (XS[-1] - 0.4, yb(XS[-1]) - 0.4)] + [(XS[i], yb(XS[i]) - 0.4) for i in range(N - 1, 0, -1)] + \
          [(XS[0] + 0.4, yb(XS[0]) - 0.4), (XS[0] + 0.4, yf(XS[0]) + 0.4)]
    B.railing(pts, zt + ROOF_T, h=1.05, post=1.5)
    zr = zt + ROOF_T
    # plant rooms over the cores
    for ca, cb, cid in CORES:
        xm = (XS[ca] + XS[cb]) / 2
        h = 2.9 if cid != "core2" else 2.5
        box(B.m, "CT_ConcreteWhite", xm - 3.2, xm + 3.2, yc(xm) - 3.0, yc(xm) + 3.0, zr, zr + h)
        box(B.m, "CT_Steel", xm - 3.3, xm + 3.3, yc(xm) - 3.1, yc(xm) + 3.1, zr + h, zr + h + 0.12)
        # louvre band on the sea side
        for k in range(8):
            xx = xm - 2.8 + k * 0.8
            box(B.detail, "CT_Louvre", xx, xx + 0.1, yc(xm) - 3.08, yc(xm) - 3.0, zr + 0.6, zr + h - 0.4)
    B.column_round(-12.0, yc(-12.0) + 1.0, zr, zr + 2.6, 1.6, mat="CT_ConcreteDark", segs=16)
    prism(B.m, "CT_Steel", [(-12.0 + 1.7 * math.cos(2 * math.pi * k / 16), yc(-12.0) + 1.0 + 1.7 * math.sin(2 * math.pi * k / 16)) for k in range(16)], zr + 2.6, zr + 2.75, top=True, bottom=True)
    # terrace deck and wave canopy
    tf = [6.0 + k * 28.0 / 56 for k in range(57)]
    sweep(B.m, "CT_Wood", tf, yc, lambda x: [(-4.2, zr), (4.2, zr), (4.2, zr + 0.1), (-4.2, zr + 0.1)][::-1] if False else [(-4.2, zr), (4.2, zr), (4.2, zr + 0.1), (-4.2, zr + 0.1)])
    cz = lambda x: 56.85 + 0.35 * math.sin(2 * math.pi * x / 12.0)
    cf = [8.0 + k * 24.0 / 96 for k in range(97)]
    sweep(B.m, "CT_ConcreteWhite", cf, yc, lambda x: [(-3.2, cz(x) - 0.08), (3.2, cz(x) - 0.08), (3.2, cz(x) + 0.12), (-3.2, cz(x) + 0.12)])
    for px in (10.0, 16.0, 22.0, 28.0):
        for dy in (-2.9, 2.9):
            B.column_round(px, yc(px) + dy, zr + 0.1, cz(px) - 0.05, 0.12, mat="CT_Steel", segs=8, m=B.detail)
    for bx in (13.0, 25.0):
        box(B.detail, "CT_Wood", bx - 1.0, bx + 1.0, yc(bx) - 1.8, yc(bx) - 1.35, zr + 0.5, zr + 0.6)
        B.spot("roof_bench_%d" % int(bx), "bench", (bx, yc(bx) - 1.0, zr + 0.1), 0.0)
    B.spot("roof_view", "view", (20.0, yf(20.0) + 0.9, zr), 0.0)

    # ---------------------------------------------------------------- footprint, height
    right = [(XS[i], yf(XS[i]) - 1.5) for i in range(N + 1)]
    B.footprint = right + [(XS[-1], yb(XS[-1]))] + [(XS[i], yb(XS[i])) for i in range(N - 1, -1, -1)]
    B.height = zr + 2.9 + 0.12
    B.note("%d flats on %d storeys, S-curved slab with undulating brise-soleil bands" % (len(flats), N_FLAT_ST))
