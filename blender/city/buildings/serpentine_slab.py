"""Serpentine slab: a very long, gently S-curved residential slab on pilotis. An original design in the spirit of
Brazilian modernism (Reidy's serpentine Pedregulho, Niemeyer's pilotis and curves, Rio brise-soleil and cobogó
screens, azulejo-clad stair towers) - not a copy of any building.

Size: ~250 m long (x) x 14 m deep (y) x ~26 m high. The plan is a polyline with an S bend: the heading of the
axis follows  CURVE_DEG * cos(2 pi s / L)  (a PARAMETER: CURVE_DEG = 0 gives a straight slab; the town plan can also
pass its own contour as PATH, a list of front-line points [(x, y)] one per structural bay). The origin is the middle
of the front line; the S bends it about +-1.4 m by default (bounding box ~17 m deep).
  * Open ground floor of pilotis (3.2 m): V-shaped legs along the front, round columns at the gallery side.
  * 7 storeys of 2.85 m over it; 30 flats a storey in 7.6 m structural bays, 3 (or 2) windows each, 210 flats.
  * The sunny front (street side) is covered end to end by a brise-soleil grid: a deep concrete fin every 1.9 m and a
    ledge at every floor. Faded blue / yellow spandrel panels behind some flats, white concrete elsewhere.
  * The back carries a gallery (access balcony) at every storey: solid white parapet topped with a band of cobogó
    (lattice of cement blocks) and the flat doors; the parapet continues over the roof.
  * 3 stair/lift cores, in blue glazed-tile mosaic with white bands and slit windows, rise through the whole slab and
    stand a vault-roofed stair head over the roof. Each core has a ground lobby with a door on the street side and
    one on the gallery side (the lobby is a hollow room with floor and ceiling and glazed side lights).
  * Roof terrace: low parapet and 11 curved concrete pergola pods (arched ribs with timber purlins).
Entrances: core<c>_front / core<c>_back (street). Enterable: the three ground lobbies (spots: bench). Flats are not.
Ideas for later: azulejo murals on the end walls, a bike store and kiosk under the pilotis, laundry on the galleries.
Material requests: none (CT_Tile, CT_TileWhite, CT_Cobogo, CT_Louvre cover it).
"""
import math
import random

import furn_int3 as fu
import interior_kit as kit
from citykit import (box, offset_polyline, band, extrude_profile, Frame, Mesh, DOOR_W, DOOR_H)
import geom

INFO = {
    "name_ru": "Змеевидный дом на опорах", "name_en": "Serpentine Slab",
    "style": "brazilian modernism", "use": "residential",
    "inspired_by": "Brazilian modernism: Reidy's serpentine Pedregulho, Niemeyer's pilotis, Rio brise-soleil, "
                   "cobogó screens, azulejo-tiled stair towers",
}

# ---- parameters (the town plan may change them before build())
LENGTH = 250.0
BAY = 7.6
CURVE_DEG = 1.5          # max heading deviation of the S bend; 0 = straight
PATH = None              # optional [(x, y)] front line, one point per bay boundary (overrides LENGTH/CURVE_DEG)

PIL = 3.2                # pilotis height
ST = 2.85                # storey height
NST = 7
BODY = 12.2              # flats' depth
GAL = 1.8                # gallery depth
FLAT_NO = 17             # the open flat of the first storey: 1-17 (Colette Bauer, a pensioner), bay 18
STAIR_W = 1.0            # the stair of the middle core, up from its lobby to the first gallery
WALL = 0.3
FIN_D = 0.85             # depth of the brise-soleil
FIN_PITCH = BAY / 4.0


# ------------------------------------------------------------------------------------------------ helpers
_FACES = {"x": (0, 2, 6, 4), "X": (1, 3, 7, 5), "y": (0, 1, 5, 4), "Y": (2, 3, 7, 6), "z": (0, 1, 3, 2), "Z": (4, 5, 7, 6)}


def hexa(m, mat, c, faces="xXyYzZ"):
    """A convex hexahedron from 8 corners (bit 0 = x, bit 1 = y, bit 2 = z); faces wound outward whatever the
    handedness of the mapping (checked against the centroid)."""
    cx = [sum(p[i] for p in c) / 8.0 for i in range(3)]
    for k in faces:
        q = [c[i] for i in _FACES[k]]
        n = geom.cross(geom.sub(q[1], q[0]), geom.sub(q[2], q[0]))
        fc = [sum(p[i] for p in q) / 4.0 for i in range(3)]
        if sum(n[i] * (fc[i] - cx[i]) for i in range(3)) < 0:
            q = q[::-1]
        m.face(mat, q)


def lbox(m, mat, F, u0, u1, y0, y1, z0, z1, faces="xXyYzZ"):
    c = [F.to_building((u1 if i & 1 else u0, y1 if i & 2 else y0, z1 if i & 4 else z0)) for i in range(8)]
    hexa(m, mat, c, faces)


def sub_facade(F, u0, u1):
    return (F.to_building((u0, 0, 0))[:2], F.to_building((u1, 0, 0))[:2])


def norm2(v):
    L = math.hypot(v[0], v[1]) or 1.0
    return (v[0] / L, v[1] / L)


def yaw_of(n):
    return math.degrees(math.atan2(n[0], -n[1]))


def plan_points():
    """Front line points FP (one per bay boundary), centred so that the middle is at the origin."""
    if PATH:
        pts = [tuple(p) for p in PATH]
    else:
        n = int(round(LENGTH / BAY))
        L = n * BAY
        th0 = math.radians(CURVE_DEG)
        pts = [(0.0, 0.0)]
        x = y = 0.0
        for i in range(n):
            s = (i + 0.5) * BAY
            th = th0 * math.cos(2 * math.pi * s / L)
            x += BAY * math.cos(th)
            y += BAY * math.sin(th)
            pts.append((x, y))
    k = len(pts) // 2
    cx = (pts[k][0] + pts[len(pts) - 1 - k][0]) / 2
    cy = (pts[k][1] + pts[len(pts) - 1 - k][1]) / 2
    return [(p[0] - cx, p[1] - cy) for p in pts]


def left_normal(a, b):
    d = norm2((b[0] - a[0], b[1] - a[1]))
    return (-d[1], d[0])


# ------------------------------------------------------------------------------------------------ build
def build(B):
    rnd = random.Random(1957)
    m, det = B.m, B.detail
    FP = plan_points()
    N = len(FP) - 1
    BP = offset_polyline(FP, BODY)
    GP = offset_polyline(FP, BODY + GAL)
    ZR = PIL + NST * ST                         # roof level
    cores = sorted({N // 6, N // 2, N - 1 - N // 6})
    zs = [PIL + ST * j for j in range(NST + 1)]
    flat_count = {}
    flat_ctx = {}
    bays_done = 0

    def new_flat(storey):
        flat_count[storey] = flat_count.get(storey, 0) + 1
        return "%d-%02d" % (storey, flat_count[storey])

    def nearest_core(i):
        return min(range(len(cores)), key=lambda c: abs(cores[c] - i))

    # ---------------------------------------------------------------- roof, soffit, galleries
    for i in range(N):
        a, b, c_, d = FP[i], FP[i + 1], BP[i + 1], BP[i]
        # roof of body and gallery (one slab over both, overhanging the gallery)
        m.face("CT_Roof", [(a[0], a[1], ZR), (b[0], b[1], ZR), (GP[i + 1][0], GP[i + 1][1], ZR), (GP[i][0], GP[i][1], ZR)])
        # soffit over the pilotis (the middle core's has the stairwell hole up to the first gallery)
        if i == cores[1]:
            _soffit_with_hole(m, a, b, c_, d)
        else:
            m.face("CT_Concrete", [(a[0], a[1], PIL), (d[0], d[1], PIL), (c_[0], c_[1], PIL), (b[0], b[1], PIL)])
        # roof slab edge towards the gallery (underside of the roof slab over the gallery)
        m.face("CT_ConcreteWhite", [(BP[i][0], BP[i][1], ZR - 0.22), (BP[i + 1][0], BP[i + 1][1], ZR - 0.22),
                                    (GP[i + 1][0], GP[i + 1][1], ZR - 0.22), (GP[i][0], GP[i][1], ZR - 0.22)][::-1])
        # gallery slabs, one at every storey floor (z = zs[j]) including the one over the pilotis
        for j in range(NST):
            z = zs[j]
            poly = [BP[i], BP[i + 1], GP[i + 1], GP[i]]
            geom.prism(m, "CT_ConcreteWhite", poly, z - 0.22, z, top=False, bottom=True)
            m.face("CT_Paving", [(p[0], p[1], z) for p in poly])
    # outer edge of the roof slab over the gallery
    for i in range(N):
        m.face("CT_ConcreteWhite", [(GP[i][0], GP[i][1], ZR - 0.22), (GP[i + 1][0], GP[i + 1][1], ZR - 0.22),
                                    (GP[i + 1][0], GP[i + 1][1], ZR), (GP[i][0], GP[i][1], ZR)][::-1])

    # ---------------------------------------------------------------- bays
    for i in range(N):
        p0, p1 = FP[i], FP[i + 1]
        q0, q1 = BP[i], BP[i + 1]
        is_core = i in cores
        Lb = math.dist(p0, p1)
        core_id = nearest_core(i)
        # ---- front facade, storey by storey
        if not is_core:
            fins_u = [k * FIN_PITCH for k in range(0, 5)]
            for j in range(NST):
                z0, z1 = zs[j], zs[j] + ST
                r = rnd.random()
                mat = "CT_ConcreteWhite" if r < 0.62 else ("CT_PlasterBlue" if r < 0.77 else ("CT_PlasterYellow" if r < 0.89 else "CT_PlasterPink"))
                if rnd.random() < 0.55:
                    wins = [(0.45, 2.65), (3.05, 4.65), (5.05, Lb - 0.45)]
                else:
                    wins = [(0.6, 3.4), (4.2, Lb - 0.6)]
                holes = [(u0, u1, z0 + 0.85, z0 + 2.15) for u0, u1 in wins]
                mine = (j == 0 and flat_count.get(1, 0) + 1 == FLAT_NO)      # Colette Bauer's flat: open, with an inner skin
                F = B.facade(p0, p1, z0, z1, mat, holes=holes, depth=WALL, back=mine)
                # back facade of this flat with the gallery door and a kitchen window
                fid = new_flat(j + 1)
                du0 = 0.9 if (i + j) % 2 == 0 else Lb - 0.9 - DOOR_W
                bholes = [(du0, du0 + DOOR_W, z0, z0 + DOOR_H)]
                kw = (3.3, 4.9) if du0 < 3 else (2.7, 4.3)
                bholes.append((kw[0], kw[1], z0 + 1.0, z0 + 2.1))
                # the back facade goes from q1 to q0 (outside faces +y); u runs from q1
                Fb = B.facade(q1, q0, z0, z1, "CT_ConcreteWhite", holes=[(Lb - h[1], Lb - h[0], h[2], h[3]) for h in bholes],
                              depth=WALL, back=mine)
                if mine:
                    flat_ctx.update(bay=i, wins=wins, du0=du0)
                door_u = Lb - (du0 + DOOR_W / 2)
                dp = Fb.to_building((door_u, -0.6, z0))
                B.flat(fid, storey=j + 1, beds=1 + (i + j) % 3, door=dp, entrance="core%d_back" % core_id)
                for u0, u1 in wins:
                    B.facade_window(F, u0, u1, z0 + 0.85, z0 + 2.15, flat=fid, cols=2, rows=1)
                B.facade_window(Fb, Lb - bholes[1][1], Lb - bholes[1][0], z0 + 1.0, z0 + 2.1, flat=fid)
                B.facade_window(Fb, Lb - bholes[0][1], Lb - bholes[0][0], z0 + 0.0 + 0.0, z0 + DOOR_H, flat=fid, cols=1, rows=1) if False else None
            # brise-soleil grid: continuous vertical fins + a ledge at every floor line
            Ff = geom.Facade(p0, p1)
            for u in fins_u:
                if u == 0.0 and i > 0 and (i - 1) not in cores:
                    continue                    # shared with the previous bay
                uu = min(max(u, 0.05), Lb - 0.05)
                lbox(det, "CT_Louvre", Ff, uu - 0.05, uu + 0.05, 0.0, FIN_D, PIL, ZR + 0.9, faces="xXyZ")
            for z in zs:
                lbox(det, "CT_Louvre", Ff, 0.0, Lb, 0.0, FIN_D, z - 0.14, z, faces="yzZ")
        else:
            _core_bay(B, i, core_id, p0, p1, q0, q1, zs, rnd, new_flat, door=(i == cores[1]))

        # ---- ground floor of a core bay: the lobby (hollow room with floor, ceiling = the soffit)
        if is_core:
            _lobby(B, i, core_id, p0, p1, q0, q1, rnd)

        # ---- gallery parapet and cobogó band at every storey (back side, outside = +y)
        for j in range(NST):
            _gallery_parapet(B, GP[i], GP[i + 1], zs[j], rnd)
        # ---- roof parapets (front and gallery edge)
        for (a, b_, mat_front) in ((p0, p1, True), (GP[i + 1], GP[i], False)):
            B.facade(a, b_, ZR, ZR + 0.9, "CT_ConcreteWhite", depth=0.25, back=True)
            nl = left_normal(a, b_) if mat_front else left_normal(b_, a)
            t = 0.25
            inner = [(a[0] + nl[0] * t * (1 if mat_front else 1), a[1] + nl[1] * t), (b_[0] + nl[0] * t, b_[1] + nl[1] * t)]
            band(m, "CT_ConcreteWhite", [a, b_], inner, ZR + 0.9)

    if flat_ctx:
        _bauer_flat(B, FP, BP, GP, flat_ctx, zs)
    _gallery(B, FP, cores, zs)

    # ---------------------------------------------------------------- end walls
    for end in (0, 1):
        if end == 0:
            pa, pb = BP[0], FP[0]           # outward -x/along -d
            Fe = B.facade(GP[0], FP[0], PIL, ZR + 0.9, "CT_ConcreteWhite", depth=WALL, back=True)
            nrm = Fe.n
        else:
            Fe = B.facade(FP[N], GP[N], PIL, ZR + 0.9, "CT_ConcreteWhite", depth=WALL, back=True)
            nrm = Fe.n
        # a tiled mural panel (blue/white) on the end wall
        L = Fe.length
        for k in range(5):
            h0 = PIL + 1.5 + k * 3.6
            lbox(det, "CT_Tile" if k % 2 == 0 else "CT_TileWhite", Fe, L * 0.25, L * 0.75, 0.0, 0.03, h0, h0 + 2.6, faces="yX")

    # ---------------------------------------------------------------- pilotis
    for i in range(N + 1):
        d = norm2((FP[min(i + 1, N)][0] - FP[max(i - 1, 0)][0], FP[min(i + 1, N)][1] - FP[max(i - 1, 0)][1]))
        nl = (-d[1], d[0])
        q = (FP[i][0] + nl[0] * 1.5, FP[i][1] + nl[1] * 1.5)
        _v_column(m, q, d, nl, legs=(-1,) if i in cores else (1,) if (i - 1) in cores else (-1, 1))
        bx, by = BP[i][0] - nl[0] * 0.7, BP[i][1] - nl[1] * 0.7
        B.column_round(bx, by, 0.0, PIL, 0.3, mat="CT_ConcreteWhite", segs=10)
        # slender round posts at the gallery edge carrying the roof canopy
        B.column_round(GP[i][0] - nl[0] * 0.2, GP[i][1] - nl[1] * 0.2, PIL, ZR - 0.22, 0.11, mat="CT_ConcreteWhite", segs=8)

    # ---------------------------------------------------------------- roof: pergola pods, vault heads
    pods = [b for b in (2, 3, 8, 9, 13, 14, 19, 20, 24, 25, 30) if b < N and b not in cores]
    for b in pods:
        _pergola(B, FP[b], FP[b + 1], ZR, rnd)

    B.footprint = _outline(FP, GP)
    B.height = ZR + 2.4 + 0.4
    # spots: lobby benches handled in _lobby; benches under the pilotis
    for i in (3, 10, 21, 29):
        if i < N:
            mid = ((FP[i][0] + FP[i + 1][0]) / 2, (FP[i][1] + FP[i + 1][1]) / 2)
            nl = left_normal(FP[i], FP[i + 1])
            pos = (mid[0] + nl[0] * 5.0, mid[1] + nl[1] * 5.0, 0.0)
            bx0, by0 = pos[0], pos[1]
            box(det, "CT_Wood", bx0 - 1.0, bx0 + 1.0, by0 - 0.25, by0 + 0.25, 0.42, 0.48)
            box(det, "CT_Concrete", bx0 - 0.9, bx0 - 0.7, by0 - 0.2, by0 + 0.2, 0.0, 0.42)
            box(det, "CT_Concrete", bx0 + 0.7, bx0 + 0.9, by0 - 0.2, by0 + 0.2, 0.0, 0.42)
            B.spot("pilotis_bench_%d" % i, "bench", (bx0, by0, 0.0), yaw_of(left_normal(FP[i], FP[i + 1])))
    B.note("%d flats on %d storeys, %d bays of %.1f m, S bend %.1f deg (axis offset +-%.1f m)" % (
        len(B.flats), NST, N, BAY, CURVE_DEG, max(abs(p[1]) for p in FP)))


def _outline(FP, GP):
    pts = list(FP) + list(reversed(GP))
    return pts


def _v_column(m, q, d, nl, legs=(-1, 1)):
    """A V-shaped column: a narrow foot on the ground spreading to two tops under the slab (2 legs of one V).
    legs: which legs to draw (-1 = toward -d, +1 = toward +d); a core's lobby walls keep only the outer leg (QA: the
    inner one stood inside the lobby)."""
    def pt(cx, cy, dd, nn, a, b, z):
        return (cx + d[0] * (dd + a) + nl[0] * (nn + b), cy + d[1] * (dd + a) + nl[1] * (nn + b), z)
    for sgn in legs:
        foot = [pt(q[0], q[1], 0, 0, a * 0.2, b * 0.3, 0.0) for b in (-1, 1) for a in (-1, 1)]
        topc = [pt(q[0], q[1], sgn * 1.05, 0, a * 0.28, b * 0.4, PIL) for b in (-1, 1) for a in (-1, 1)]
        hexa(m, "CT_ConcreteWhite", foot + topc, faces="xXyY")
    # note: bit order is (x = a, y = b, z) -> first 4 corners are the foot, last 4 the top


def _gallery_parapet(B, a, b, z, rnd):
    """Parapet of one gallery bay (a -> b along the slab): solid white lower part, cobogó lattice above."""
    d = norm2((b[0] - a[0], b[1] - a[1]))
    nl = (-d[1], d[0])
    L = math.dist(a, b)
    # lower solid wall 0.14 thick, standing just inside the slab edge
    def P(u, off, zz):
        return (a[0] + d[0] * u + nl[0] * off, a[1] + d[1] * u + nl[1] * off, zz)
    F = geom.Facade(b, a)            # outside faces +y (walk b -> a)
    # lower wall: outer face + inner face + top
    lbox(B.m, "CT_ConcreteWhite", geom.Facade(b, a), 0.0, L, 0.0, 0.0 + 0.001, z, z + 0.45, faces="y")
    B.m.face("CT_ConcreteWhite", [P(0, -0.14, z + 0.45), P(L, -0.14, z + 0.45), P(L, -0.14, z), P(0, -0.14, z)][::-1]) if False else None
    # inner face of the lower wall (faces -y into the gallery)
    B.m.face("CT_ConcreteWhite", [P(0, -0.14, z), P(L, -0.14, z), P(L, -0.14, z + 0.45), P(0, -0.14, z + 0.45)])
    # top of the low wall
    B.m.face("CT_ConcreteWhite", [P(0, -0.14, z + 0.45), P(L, -0.14, z + 0.45), P(L, 0.0, z + 0.45), P(0, 0.0, z + 0.45)])
    # opaque back of the cobogó band
    B.m.face("CT_ConcreteDark", [P(0, -0.07, z + 0.45), P(L, -0.07, z + 0.45), P(L, -0.07, z + 1.1), P(0, -0.07, z + 1.1)])
    B.m.face("CT_ConcreteDark", [P(L, -0.07, z + 0.45), P(0, -0.07, z + 0.45), P(0, -0.07, z + 1.1), P(L, -0.07, z + 1.1)])
    # lattice of blocks (vertical bars with a cap rail)
    n = max(4, int(L / 0.5))
    pitch = L / n
    Fo = geom.Facade(b, a)
    for k in range(n):
        u0 = L - (k + 0.5) * pitch - pitch * 0.25
        lbox(B.detail, "CT_Cobogo", Fo, u0, u0 + pitch * 0.5, 0.0, 0.1, z + 0.45, z + 1.1, faces="yxXZ")
    lbox(B.detail, "CT_Cobogo", Fo, 0.0, L, -0.04, 0.12, z + 1.1, z + 1.18, faces="yYzZ")
    # note: the Facade's outward is +y; local y -0.14.. is inside


def _core_bay(B, i, cid, p0, p1, q0, q1, zs, rnd, new_flat, door=False):
    """A stair/lift core bay above the pilotis: blue tile mosaic with white bands and slit windows."""
    Lb = math.dist(p0, p1)
    ZR = zs[-1]
    for j in range(NST):
        z0 = zs[j]
        for side in ("front", "back"):
            a, b = (p0, p1) if side == "front" else (q1, q0)
            if door and j == 0 and side == "back":
                _core_back_door(B, a, b, Lb, z0)
                continue
            # white spandrel band, blue field with 2 slit windows, white cap band
            B.facade(a, b, z0, z0 + 0.85, "CT_TileWhite", depth=WALL, back=False)
            slit = [(1.5, 2.1, z0 + 0.85, z0 + 2.2), (Lb - 2.1, Lb - 1.5, z0 + 0.85, z0 + 2.2)]
            if side == "back":
                # the door from the gallery into the stair, in the middle, glazed (stair kind)
                slit = [(1.0, 1.0 + 1.3, z0 + 0.0 + 0.85, z0 + 0.85 + 0.0)] if False else slit
            F = B.facade(a, b, z0 + 0.85, z0 + 2.6, "CT_Tile", holes=[(u0, u1, h0, h1) for u0, u1, h0, h1 in slit], depth=WALL, back=False)
            for u0, u1, h0, h1 in slit:
                B.facade_window(F, u0, u1, h0, h1, kind="stair", cols=1, rows=2)
            B.facade(a, b, z0 + 2.6, z0 + ST, "CT_TileWhite", depth=WALL, back=False)
    # stair head on the roof: tile box with a shallow concrete vault over it
    Ff = geom.Facade(p0, p1)
    zb = ZR + 2.4
    lbox(B.m, "CT_Tile", Ff, 1.0, Lb - 1.0, -(BODY - 1.5), -1.5, ZR, zb, faces="xXyY")
    n = 10
    prof = [(zb, 1.0), (zb, Lb - 1.0)] + [(zb + 0.4 * math.sin(math.pi * k / n), Lb - 1.0 - (Lb - 2.0) * k / n) for k in range(1, n)]
    yaw = math.atan2(p1[1] - p0[1], p1[0] - p0[0])
    mm = Mesh()
    extrude_profile(mm, "CT_ConcreteWhite", "CT_ConcreteWhite", prof, 1.5, BODY - 1.5, lambda a, b, t: (b, t, a))
    B.m.extend(Frame(p0[0], p0[1], yaw).apply(mm))
    # a slit window in the head
    B.facade_window  # (cards omitted: the head is a blind box)


def _lobby(B, i, cid, p0, p1, q0, q1, rnd):
    Lb = math.dist(p0, p1)
    m = B.m
    # front wall (tile) with a double door in the middle and glazed side lights
    dw = DOOR_W * 2
    mid = Lb / 2
    holes_f = [(mid - dw / 2, mid + dw / 2, 0.0 + 0.0001, DOOR_H), (0.5, 2.3, 0.3, 2.6), (Lb - 2.3, Lb - 0.5, 0.3, 2.6)]
    Ff = B.facade(p0, p1, 0.0, PIL, "CT_Tile", holes=holes_f, depth=WALL, back=True)
    for h in holes_f[1:]:
        B.facade_window(Ff, h[0], h[1], h[2], h[3], kind="public", cols=2, rows=1)
    B.entrance("core%d_front" % cid, Ff.to_building((mid, 0.0, 0.0)), yaw_of(Ff.n), dw, DOOR_H, "street", leaves=2)
    # back wall (gallery side): single door and one side light
    holes_b = [(mid - DOOR_W / 2, mid + DOOR_W / 2, 0.0 + 0.0001, DOOR_H), (Lb - 2.3, Lb - 0.7, 0.3, 2.6)]
    Fb = B.facade(q1, q0, 0.0, PIL, "CT_Tile", holes=holes_b, depth=WALL, back=True)
    B.facade_window(Fb, holes_b[1][0], holes_b[1][1], 0.3, 2.6, kind="public", cols=2, rows=1)
    B.entrance("core%d_back" % cid, Fb.to_building((mid, 0.0, 0.0)), yaw_of(Fb.n), DOOR_W, DOOR_H, "street")
    # side walls of the lobby (towards the open pilotis), tile
    B.facade(q0, p0, 0.0, PIL, "CT_Tile", depth=WALL, back=True)
    B.facade(p1, q1, 0.0, PIL, "CT_Tile", depth=WALL, back=True)
    # floor
    m.face("CT_Floor", [(p0[0], p0[1], 0.01), (p1[0], p1[1], 0.01), (q1[0], q1[1], 0.01), (q0[0], q0[1], 0.01)])
    # a canopy over the street door: a thin white slab
    lbox(B.detail, "CT_ConcreteWhite", Ff, mid - 1.6, mid + 1.6, 0.0, 1.4, 2.65, 2.8, faces="yYzZxX")
    cx = ((p0[0] + p1[0] + q0[0] + q1[0]) / 4, (p0[1] + p1[1] + q0[1] + q1[1]) / 4)
    nl = left_normal(p0, p1)
    # furnishing: letter boxes on the left wall, a doormat inside the front door, a plant, a lamp
    tx, ty = (p1[0] - p0[0]) / Lb, (p1[1] - p0[1]) / Lb
    D = math.dist(p0, q0)
    yw_side = math.degrees(math.atan2(tx, -ty))              # out of the left wall, into the lobby (along +t)
    fu.mailboxes(B, fu.Fr(p0[0] + tx * WALL, p0[1] + ty * WALL, yw_side, 0.01), WALL + 1.5, 0.95, cols=5, rows=3)
    yw_u = math.degrees(math.atan2(ty, tx))                   # u along the front wall
    fu.doormat(B, fu.Fr(p0[0] + tx * mid + nl[0] * (WALL + 0.5), p0[1] + ty * mid + nl[1] * (WALL + 0.5), yw_u, 0.01), 0.0, 0.0)
    fu.plant(B, p1[0] - tx * (WALL + 0.45) + nl[0] * (D - WALL - 0.6), p1[1] - ty * (WALL + 0.45) + nl[1] * (D - WALL - 0.6), 0.01, 0.9, "CT_PlasterBlue")
    B.lamp((cx[0], cx[1], PIL - 0.35), lumens=480, colour="#FFE0B0", kind="lobby", radius=5.5)
    for sg in (-1, 1):      # QA: the 12 m deep lobby was dark at both ends (bench, desk)
        B.lamp((cx[0] + nl[0] * sg * 3.9, cx[1] + nl[1] * sg * 3.9, PIL - 0.35), lumens=330, colour="#FFE0B0", kind="lobby", radius=4.5)
    _dress_lobby(B, i, cid, p0, p1, nl, Lb, D)
    B.spot("lobby_view_%d" % cid, "view", (cx[0] + nl[0] * 2, cx[1] + nl[1] * 2, 0.0), yaw_of(nl))


def _pergola(B, a, b, zr, rnd):
    """A pod of curved concrete ribs (arches across the roof) with timber purlins, standing on bay a -> b."""
    L = math.dist(a, b)
    yaw = math.atan2(b[1] - a[1], b[0] - a[0])
    v0, v1 = 1.5, 10.8
    H = 2.0
    th = 0.22
    for k, u in enumerate((0.9, L / 2, L - 0.9)):
        n = 12
        outer = [(v0 + (v1 - v0) * t / n, zr + 0.9 * 0 + H * math.sin(math.pi * t / n) ** 0.8) for t in range(n + 1)]
        inner = [(v, z - th) for v, z in reversed(outer)]
        poly = outer + inner
        # drop the degenerate ends: the arch feet touch the roof
        poly = [(v, max(z, zr)) for v, z in poly]
        # remove duplicate points
        clean = []
        for p in poly:
            if not clean or math.hypot(p[0] - clean[-1][0], p[1] - clean[-1][1]) > 1e-4:
                clean.append(p)
        while len(clean) > 3 and math.hypot(clean[0][0] - clean[-1][0], clean[0][1] - clean[-1][1]) <= 1e-4:
            clean.pop()          # the closing point repeats the first: a zero-length side, a degenerate face
        mm = Mesh()
        extrude_profile(mm, "CT_ConcreteWhite", "CT_ConcreteWhite", clean, -0.1, 0.1, lambda aa, bb, t: (t, aa, bb))
        fr = Frame(a[0] + math.cos(yaw) * u, a[1] + math.sin(yaw) * u, yaw)
        B.detail.extend(fr.apply(mm))
    # purlins along the bay across the ribs
    for v in (v0 + 1.5, (v0 + v1) / 2, v1 - 1.5):
        t = (v - v0) / (v1 - v0)
        z = zr + H * math.sin(math.pi * t) ** 0.8
        mm = Mesh()
        box(mm, "CT_Wood", 0.1, L - 0.1, v - 0.06, v + 0.06, z - 0.02, z + 0.1)
        fr = Frame(a[0], a[1], yaw)
        B.detail.extend(fr.apply(mm))


# ------------------------------------------------------------------------------------------------ lobby furnishing
class _Fr:
    """Lobby frame: origin = front wall's left end p0, u along the front wall, v into the building."""

    def __init__(self, o, eu, ev):
        self.o, self.eu, self.ev = o, eu, ev

    def p(self, u, v):
        return (self.o[0] + u * self.eu[0] + v * self.ev[0], self.o[1] + u * self.eu[1] + v * self.ev[1])

    def yaw(self, fu_, fv_):
        dx = fu_ * self.eu[0] + fv_ * self.ev[0]
        dy = fu_ * self.eu[1] + fv_ * self.ev[1]
        return math.degrees(math.atan2(dx, -dy)) % 360.0


def _wheel(m, mat, c, u, r, n=8, w=0.03):
    pts = [(c[0] + u[0] * r * math.cos(2 * math.pi * k / n), c[1] + u[1] * r * math.cos(2 * math.pi * k / n),
            c[2] + r * math.sin(2 * math.pi * k / n)) for k in range(n)]
    for k in range(n):
        fu.rod(m, mat, pts[k], pts[(k + 1) % n], w)


def _bike(B, x, y, dirv, mat="CT_PlasterRed"):
    """A leaning city bicycle (detail) lying along the unit vector dirv."""
    u = dirv
    P3 = lambda s, z: (x + u[0] * s, y + u[1] * s, z)
    m = B.detail
    for s in (-0.55, 0.55):
        _wheel(m, "CT_Steel", P3(s, 0.33), u, 0.33)
    fu.rod(m, mat, P3(-0.55, 0.33), P3(-0.1, 0.55), 0.035)
    fu.rod(m, mat, P3(-0.1, 0.55), P3(0.4, 0.62), 0.035)
    fu.rod(m, mat, P3(0.4, 0.62), P3(0.55, 0.33), 0.035)
    fu.rod(m, mat, P3(-0.1, 0.55), P3(0.05, 0.3), 0.035)
    fu.rod(m, mat, P3(0.4, 0.62), P3(0.4, 0.95), 0.03)
    hb = P3(0.4, 0.95)
    fu.rod(m, "CT_Steel", (hb[0] - u[1] * 0.28, hb[1] + u[0] * 0.28, 0.95), (hb[0] + u[1] * 0.28, hb[1] - u[0] * 0.28, 0.95), 0.03)
    sd = P3(-0.17, 0.77)
    fu.rod(m, "CT_Leather", (sd[0] - u[0] * 0.12, sd[1] - u[1] * 0.12, 0.77), (sd[0] + u[0] * 0.12, sd[1] + u[1] * 0.12, 0.77), 0.07)
    fu.rod(m, mat, P3(-0.1, 0.55), P3(-0.17, 0.74), 0.03)


def _pram(B, x, y, front, mat="CT_PlasterBlue"):
    """A pram (detail) whose front looks along the unit vector `front`."""
    f = front
    s_ = (-f[1], f[0])
    P3 = lambda s, t, z: (x + s_[0] * s + f[0] * t, y + s_[1] * s + f[1] * t, z)
    m = B.detail
    geom.prism(m, mat, [P3(du, dv, 0)[:2] for du, dv in ((-0.27, -0.45), (0.27, -0.45), (0.27, 0.45), (-0.27, 0.45))], 0.38, 0.62, top=True, bottom=True)
    geom.prism(m, "CT_Paper", [P3(du, dv, 0)[:2] for du, dv in ((-0.27, -0.45), (0.27, -0.45), (0.27, -0.05), (-0.27, -0.05))], 0.62, 0.86, top=True, bottom=False)
    for du, dv in ((-0.3, -0.35), (0.3, -0.35), (-0.3, 0.35), (0.3, 0.35)):
        _wheel(m, "CT_Steel", P3(du, dv, 0.17), f, 0.17, n=8, w=0.03)
    fu.rod(m, "CT_Steel", P3(-0.25, -0.45, 0.6), P3(-0.25, -0.75, 1.0), 0.03)
    fu.rod(m, "CT_Steel", P3(0.25, -0.45, 0.6), P3(0.25, -0.75, 1.0), 0.03)
    fu.rod(m, "CT_Steel", P3(-0.25, -0.75, 1.0), P3(0.25, -0.75, 1.0), 0.03)


def _dress_lobby(B, i, cid, p0, p1, nl, Lb, D):
    """One lobby design (7.6 x 12.2 m, doors mid front and mid back, mailboxes on the left wall near the front) with
    seeded differences per core: which wall carries the concierge desk and which the waiting corner, pictures,
    colours, plants and one extra (bike / pram / aquarium)."""
    r = random.Random(7000 + cid * 31)
    tx, ty = (p1[0] - p0[0]) / Lb, (p1[1] - p0[1]) / Lb
    F = _Fr(p0, (tx, ty), nl)
    pre = "core%d_" % cid
    Z = 0.01
    mid = Lb / 2
    yb = D - 0.3
    sa = 1 if cid % 2 == 0 else -1               # side A (desk, radio): +1 left wall, -1 right wall
    sb = -sa

    def wall(s, v):
        """(x, y, yaw) of a point on the inner face of the left (s=+1) / right (s=-1) wall at depth v, facing in."""
        u = WALL if s > 0 else Lb - WALL
        return F.p(u, v), F.yaw(s, 0)

    def at(s, off, v):                           # a point `off` m in from the wall
        u = (WALL + off) if s > 0 else (Lb - WALL - off)
        return F.p(u, v)

    pal = ["CT_PlasterBlue", "CT_PlasterGreen", "CT_PlasterYellow", "CT_PlasterRed", "CT_PlasterPink"]
    r.shuffle(pal)
    # runner between the doors
    x, y = F.p(mid, 6.2)
    kit.rug(B, x, y, Z, F.yaw(0, 1), w=1.4, d=8.0, name="rug_%d" % (cid + 1), seed=cid)
    # --- side A: house notice board over a bench, pictures, radio shelf, concierge desk, plant
    (px, py), fy = wall(sa, 4.0)
    kit.notice_board(B, px, py, Z, fy, w=1.0, zc=1.75, seed=7 + cid)
    B.thing(pre + "notices", "board", (*at(sa, 0.05, 4.0), 1.75), fy, topic=("lost", "club", "lost")[cid % 3])
    bx_, by_ = at(sa, 0.25, 4.0)
    kit.bench(B, bx_, by_, Z, fy, w=1.6, kind="park")
    B.thing(pre + "bench_a", "seat", (bx_, by_, 0.46), fy, height=0.45)
    B.spot("lobby_bench_%d" % cid, "bench", (bx_, by_, 0.0), fy)
    names = ["painting_sea_calm", "painting_dunes", "painting_pines", "painting_lighthouse", "painting_abstract", "painting_portrait"]
    r.shuffle(names)
    (px, py), fy = wall(sa, 5.8)
    kit.picture(B, px, py, Z, fy, name=names[0], w=0.8, zc=1.75)
    (px, py), fy = wall(sa, 7.0)
    ws_x, ws_y = px, py
    kit.wall_shelf(B, ws_x, ws_y, Z, fy, w=0.7, zt=1.2, props=False)
    rx, ry = at(sa, 0.14, 7.0)
    kit.radio(B, rx, ry, 1.2, fy)
    B.thing(pre + "radio", "machine", (rx, ry, 1.35), fy, what="jukebox")
    dx, dy = at(sa, 0.95, 9.0)
    d = kit.office_desk(B, dx, dy, Z, F.yaw(-sa, 0), w=1.4, d=0.7)
    cx_, cy_ = at(sa, 0.3, 9.0)
    kit.chair(B, cx_, cy_, Z, F.yaw(-sa, 0), kind="wood")
    lx_, ly_ = at(sa, 0.95, 9.45)
    kit.table_lamp(B, lx_, ly_, d["top"] + Z, 0.0)
    B.thing(pre + "concierge_desk", "counter", (dx, dy, 1.0), F.yaw(-sa, 0), role="concierge", service="reception")
    px_, py_ = at(sa, 0.6, 10.9)
    kit.plant(B, px_, py_, Z, 0.0, kind=("ficus", "fern", "ficus")[cid % 3], pot=pal[0], h=1.4, seed=cid)
    # --- side B: board over a bench near the front, sofa corner with coffee table, armchair, lamp
    (px, py), fy = wall(sb, 2.6)
    kit.picture(B, px, py, Z, fy, name=names[1], w=0.9, zc=1.75)
    (px, py), fy = wall(sb, 6.0)
    sfx, sfy = at(sb, 0.45, 6.0)
    kit.sofa(B, sfx, sfy, Z, fy, seats=2, mat=pal[1])
    B.thing(pre + "sofa", "seat", (sfx, sfy, 0.46), fy, height=0.45)
    kit.picture(B, px, py, Z, fy, name=names[2], w=1.0, zc=1.8)
    x, y = at(sb, 1.7, 6.0)
    ct = kit.coffee_table(B, x, y, Z, fy)
    kit.newspaper(B, x, y, ct["top"] + Z, 0.0, seed=cid)
    kit.cup(B, x + 0.1, y + 0.1, ct["top"] + Z, 0.0)
    if cid == 1:                                   # the left wall's back half is the stair now
        x, y = F.p(2.15, 7.9)
        ay = F.yaw(-0.35, -1.0)
        kit.armchair(B, x, y, Z, ay, mat=pal[2])
        B.thing(pre + "armchair", "seat", (x, y, 0.46), ay, height=0.45)
        x, y = F.p(2.85, 9.0)
    else:
        x, y = at(sb, 0.75, 8.3)
        kit.armchair(B, x, y, Z, F.yaw(-sb, 0.5), mat=pal[2])
        B.thing(pre + "armchair", "seat", (x, y, 0.46), F.yaw(-sb, 0.5), height=0.45)
        x, y = at(sb, 0.4, 9.4)
    kit.floor_lamp(B, x, y, Z, 0.0)
    (px, py), fy = wall(sb, 4.0)
    kit.poster(B, px, py, Z, fy, name=("poster_birds_0", "poster_cons_1", "poster_sun_1")[cid % 3], w=0.6, zc=1.7)
    B.thing(pre + "club_board", "board", (*at(sb, 0.05, 4.0), 1.7), fy, topic="club")
    # --- back wall: pictures, clock (the rear door is in the middle)
    for k, uu in enumerate((mid + 1.4, mid + 2.5)):
        x, y = F.p(uu, yb)
        kit.picture(B, x, y, Z, F.yaw(0, -1), name=names[3 + k], w=0.7, zc=1.7)
    x, y = F.p(mid, yb)
    kit.wall_clock(B, x, y, Z, F.yaw(0, -1), zc=2.8, face="clock_%d" % (cid % 4))
    # --- front wall: hat stand and a mirror
    x, y = F.p(mid + (-1.9 if cid == 0 else 1.7), 0.75)      # lobby 0 keeps the right corner for bicycles
    kit.hat_stand(B, x, y, Z, 0.0)
    if cid != 0:                                 # lobby 0 parks bicycles in this corner
        x, y = at(sb, 0.5, 1.8)                  # QA: the front half of the lobby was an empty floor
        kit.plant(B, x, y, Z, 0.0, kind=("fern", "ficus", "fern")[cid % 3], pot=pal[3], h=1.1, seed=cid + 5)
    # --- one extra per core
    if cid == 0:
        x, y = at(sb, 0.55, 10.6)
        _bike(B, x, y, F.ev, mat=pal[4])
    elif cid == 1:
        # QA (textured renders): the pram's handle stood inside the wall at the back corner, a dark bar hanging on it.
        # The pram now parks near the front under the club board, hood to the door, well clear of every wall.
        x, y = F.p(1.15, 4.1)
        kit.pram(B, x, y, Z, F.yaw(0, -1), mat=pal[4])
        B.thing(pre + "pram", "machine", (x, y, 0.7), F.yaw(0, -1), what="pram")
        _stair(B, F, Lb, D)
        B.thing(pre + "stairs", "door", (*F.p(0.8, 7.4), 1.0), F.yaw(0, -1), to="gallery")
    else:
        x, y = at(sb, 0.3, 10.5)
        kit.aquarium(B, x, y, Z, F.yaw(-sb, 0), w=1.0, seed=3)
        B.thing(pre + "aquarium", "screen", (x, y, 1.0), F.yaw(-sb, 0), what="aquarium")
    _lobby_extras(B, cid, F, Lb, D, sa, sb, Z, pre, mid, at)


# ================================================================================================ round 2: the stair, the flat, the gallery
# Colette Bauer (74, a pensioner of the town's data, home P-SERPENTINE/1-17) lives in flat 1-17, bay 18 of the first
# storey. The way up: the middle lobby's stair (left wall, back half) climbs through a hole in the soffit and a small
# shaft to a door in the core's back wall onto the first gallery; the gallery runs on to her door.
HOLE = (0.3, 1.5, 9.2, 11.9)       # stairwell hole in the middle core's soffit: u0, u1 (from the bay's p0), v0, v1 (from the front line)


def _bil(a, b, c, d, u, v, Lb, D, z):
    s, t = u / Lb, v / D
    return ((1 - s) * (1 - t) * a[0] + s * (1 - t) * b[0] + s * t * c[0] + (1 - s) * t * d[0],
            (1 - s) * (1 - t) * a[1] + s * (1 - t) * b[1] + s * t * c[1] + (1 - s) * t * d[1], z)


def _soffit_with_hole(m, a, b, c, d):
    Lb, D = math.dist(a, b), math.dist(a, d)
    u0, u1, v0, v1 = HOLE
    for ua, ub, va, vb in ((0, Lb, 0, v0), (0, u0, v0, D), (u1, Lb, v0, D), (u0, u1, v1, D)):
        m.face("CT_Concrete", [_bil(a, b, c, d, ua, va, Lb, D, PIL), _bil(a, b, c, d, ua, vb, Lb, D, PIL),
                               _bil(a, b, c, d, ub, vb, Lb, D, PIL), _bil(a, b, c, d, ub, va, Lb, D, PIL)])


def _core_back_door(B, a, b, Lb, z0):
    """The middle core's back on the first storey: blue tile with its slit windows and a door (the stair from the lobby comes
    out here onto the gallery); the white spandrel band is drawn as an overlay either side of the door."""
    ud = (Lb - WALL - STAIR_W, Lb - WALL)               # the facade runs from q1: the stair is at the p0 end
    slit = [(1.5, 2.1, z0 + 0.85, z0 + 2.2), (Lb - 2.1, Lb - 1.5, z0 + 0.85, z0 + 2.2)]
    F = B.facade(a, b, z0, z0 + 2.6, "CT_Tile", holes=slit + [(ud[0], ud[1], z0, z0 + 2.2)], depth=WALL, back=False)
    for u0, u1, h0, h1 in slit:
        B.facade_window(F, u0, u1, h0, h1, kind="stair", cols=1, rows=2)
    for ua, ub in ((0.0, ud[0]), (ud[1], Lb)):
        lbox(B.detail, "CT_TileWhite", F, ua, ub, 0.0, 0.02, z0, z0 + 0.85, faces="yXx")
    B.facade(a, b, z0 + 2.6, z0 + ST, "CT_TileWhite", depth=WALL, back=False)


def _fbox(m, mat, F, u0, u1, v0, v1, z0, z1, faces="xXyYzZ"):
    """A box in a bay's frame (u along the front line, v into the plot)."""
    c = []
    for i in range(8):
        x, y = F.p(u1 if i & 1 else u0, v1 if i & 2 else v0)
        c.append((x, y, z1 if i & 4 else z0))
    hexa(m, mat, c, faces)


def _p3(F, u, v, z):
    x, y = F.p(u, v)
    return (x, y, z)


def _bay_frame(FP, i):
    p0, p1 = FP[i], FP[i + 1]
    Lb = math.dist(p0, p1)
    return _Fr(p0, ((p1[0] - p0[0]) / Lb, (p1[1] - p0[1]) / Lb), left_normal(p0, p1)), Lb


def _stair(B, F, Lb, D):
    """The stair of the middle lobby: left wall, from v = 7.58 up to the back wall (16 steps of 0.2 x 0.27), solid to the floor;
    the shaft above the soffit hole (walls, a ceiling, a lamp); a handrail on the open side."""
    m = B.m
    z0, top, n = 0.01, PIL, 16
    ve = D - WALL
    g = 0.27
    for k in range(n):
        zt = z0 + (top - z0) * (k + 1) / n
        _fbox(m, "CT_Concrete", F, WALL, WALL + STAIR_W, ve - (n - k) * g, ve - (n - k - 1) * g, z0, zt, faces="xXyYZ")
    u0, u1, v0, v1 = HOLE
    zc = PIL + 2.65
    t = 0.1
    _fbox(m, "CT_ConcreteWhite", F, 0.0, u1 + t, v0 - t, v0, PIL, zc, faces="xXyY")          # front of the shaft
    _fbox(m, "CT_ConcreteWhite", F, u1, u1 + t, v0, v1, PIL, zc, faces="xXyY")               # its open side
    _fbox(m, "CT_ConcreteWhite", F, 0.0, WALL, v0, v1, PIL, zc, faces="XyY")                  # the lobby's left wall carried up
    _fbox(m, "CT_ConcreteWhite", F, 0.0, u1 + t, v0 - t, v1, zc, zc + 0.12, faces="zZxXyY")  # ceiling of the shaft
    # handrail on the open side, in the detail mesh
    d = B.detail
    ur = WALL + STAIR_W + 0.04
    va, vb = ve - n * g + 0.1, ve - 0.15
    za, zb = z0 + (top - z0) * 1.0 / n + 0.9, top + 0.9
    fu.rod(d, "CT_Steel", _p3(F, ur, va, za), _p3(F, ur, vb, zb), 0.022)
    fu.rod(d, "CT_Steel", _p3(F, ur, va, za - 0.4), _p3(F, ur, vb, zb - 0.4), 0.015)
    for k in range(0, n, 3):
        v = ve - (n - k - 0.5) * g
        zt = z0 + (top - z0) * (k + 1) / n
        fu.rod(d, "CT_Steel", _p3(F, ur, v, zt), _p3(F, ur, v, zt + 0.9 + 0.0), 0.014)
    B.lamp(_p3(F, 0.9, 10.6, zc - 0.3), lumens=420, colour="#FFE0B0", kind="public", radius=5.0)


def _bauer_flat(B, FP, BP, GP, ctx, zs):
    """Flat 1-17: the home of Colette Bauer, a retired net-mender (the harbour net loft), owl, fish keeper. Hall with the
    door from the gallery; kitchen at the back; bedroom and living room along the street front. Everything inside the bay."""
    i = ctx["bay"]
    F, Lb = _bay_frame(FP, i)
    D = math.dist(FP[i], BP[i])
    m, d = B.m, B.detail
    Z0 = zs[0]
    Zf = Z0 + 0.01
    ZC = Z0 + 2.65
    OH = Z0 + 2.1
    u0, u1 = 0.2, Lb - 0.2
    v0, v1 = WALL, D - WALL
    E, W, S, N = F.yaw(1, 0), F.yaw(-1, 0), F.yaw(0, -1), F.yaw(0, 1)

    def at(u, v):
        return F.p(u, v)

    # ---- floors, ceiling, party walls
    quad = [_p3(F, u0, v0, Zf), _p3(F, u1, v0, Zf), _p3(F, u1, v1, Zf), _p3(F, u0, v1, Zf)]
    m.face("CT_Floor", quad)
    m.face("CT_TileWhite", [_p3(F, 2.97, 7.82, Zf + 0.004), _p3(F, u1, 7.82, Zf + 0.004), _p3(F, u1, v1, Zf + 0.004), _p3(F, 2.97, v1, Zf + 0.004)])
    m.face("CT_ConcreteWhite", [_p3(F, u0, v0, ZC), _p3(F, u0, v1, ZC), _p3(F, u1, v1, ZC), _p3(F, u1, v0, ZC)])
    _fbox(m, "CT_ConcreteWhite", F, 0.0, u0, v0, v1, Z0, ZC, faces="XyYZ")
    _fbox(m, "CT_ConcreteWhite", F, u1, Lb, v0, v1, Z0, ZC, faces="xyYZ")
    # ---- partitions: bedroom | living (u 2.85), hall+bedroom | kitchen+living (v 7.7), hall | kitchen (u 2.85)
    T = 0.12
    _fbox(m, "CT_PlasterGreen", F, 2.85, 2.85 + T, v0, 7.7 + T, Z0, ZC)
    _fbox(m, "CT_PlasterYellow", F, u0, 0.25, 7.7, 7.7 + T, Z0, ZC)
    _fbox(m, "CT_PlasterYellow", F, 1.2, 2.85, 7.7, 7.7 + T, Z0, ZC)
    _fbox(m, "CT_PlasterYellow", F, 0.25, 1.2, 7.7, 7.7 + T, OH, ZC)
    _fbox(m, "CT_PlasterBlue", F, 2.97, 5.3, 7.7, 7.7 + T, Z0, ZC)
    _fbox(m, "CT_PlasterBlue", F, 6.3, u1, 7.7, 7.7 + T, Z0, ZC)
    _fbox(m, "CT_PlasterBlue", F, 5.3, 6.3, 7.7, 7.7 + T, OH, ZC)
    _fbox(m, "CT_PlasterGreen", F, 2.85, 2.85 + T, 7.7 + T, 9.2, Z0, ZC)
    _fbox(m, "CT_PlasterGreen", F, 2.85, 2.85 + T, 10.2, v1, Z0, ZC)
    _fbox(m, "CT_PlasterGreen", F, 2.85, 2.85 + T, 9.2, 10.2, OH, ZC)

    # ---- lamps (living room, kitchen)
    B.lamp(_p3(F, 4.8, 3.6, ZC - 0.3), lumens=520, colour="#FFD9A8", kind="public", radius=8.0)
    B.lamp(_p3(F, 3.7, 9.6, ZC - 0.3), lumens=440, colour="#FFE8C8", kind="public", radius=5.5)

    # ---- living room
    kit.rug(B, *at(5.2, 4.3), Zf, N, w=2.0, d=3.2, name="rug_4", seed=4)
    ac = kit.aquarium(B, *at(Lb - 0.2 - 0.2, 2.4), Zf, W, w=1.0, seed=7)
    B.thing("bauer_aquarium", "screen", _p3(F, Lb - 0.45, 2.4, Zf + 1.0), W, what="aquarium")
    x, y = at(Lb - 0.2 - 0.2, 1.3)                  # the little table by the tank: a saucer of beach finds
    st = kit.table(B, x, y, Zf, W, w=0.45, d=0.4, h=0.62)
    kit.plates(B, x, y, st["top"], 0.0, n=1)
    for k, mt in enumerate(("CT_PlasterGreen", "CT_PlasterBlue", "CT_ConcreteWhite")):
        _fbox(d, mt, F, Lb - 0.45 + 0.04 * k - 0.04, Lb - 0.45 + 0.04 * k - 0.01, 1.3 - 0.04 + 0.03 * k, 1.3 - 0.01 + 0.03 * k,
              st["top"] + 0.012, st["top"] + 0.03, faces="xXyYZ")
    B.thing("bauer_dish", "machine", (x, y, st["top"] + 0.1), W, what="beach_finds")
    x, y = at(5.7, 2.4)
    kit.armchair(B, x, y, Zf, E, mat="CT_PlasterRed")
    B.thing("bauer_armchair", "seat", (x, y, Zf + 0.45), E, height=0.45)
    x, y = at(Lb - 0.2 - 0.43, 5.35)
    kit.sofa(B, x, y, Zf, W, seats=2, mat="CT_PlasterGreen")
    B.thing("bauer_sofa", "seat", (x, y, Zf + 0.45), W, height=0.45)
    x, y = at(5.75, 5.35)
    ct = kit.coffee_table(B, x, y, Zf, W)
    kit.teapot(B, x, y, ct["top"], 0.0)
    kit.newspaper(B, x + 0.25 * F.ev[0], y + 0.25 * F.ev[1], ct["top"], 0.0, seed=4)
    x, y = at(3.4, 5.35)
    kit.tv_set(B, x, y, Zf, E)
    B.thing("bauer_tv", "screen", (x, y, Zf + 0.7), E, what="tv")
    x, y = at(2.97, 1.9)
    kit.bookshelf(B, x, y, Zf, E, w=1.0, topic="sea", seed=3)
    B.thing("bauer_shelf", "shelf", (x + F.eu[0] * 0.2, y + F.eu[1] * 0.2, Zf + 1.3), E, topic="sea")
    x, y = at(2.97, 3.7)
    kit.wall_shelf(B, x, y, Zf, E, w=0.7, zt=1.2, props=False)
    rx, ry = at(3.12, 3.7)
    kit.radio(B, rx, ry, Zf + 1.2, E)
    B.thing("bauer_radio", "machine", (rx, ry, Zf + 1.35), E, what="radio")
    x, y = at(3.55, 7.7)
    kit.picture(B, x, y, Zf, S, name="painting_portrait", w=0.7, zc=1.65)
    B.thing("bauer_photos", "board", (x, y, Zf + 1.65), S, topic="photos")
    x, y = at(4.55, 7.7)
    kit.wall_clock(B, x, y, Zf, S, zc=2.1, face="clock_2")
    B.thing("bauer_clock", "machine", (x, y, Zf + 2.1), S, what="clock")
    x, y = at(7.05, 6.85)
    kit.floor_lamp(B, x, y, Zf, 0.0)
    for cu, cw in ((3.85, 1.6), (6.1, 1.9), (1.55, 2.0)):
        x, y = at(cu, v0)
        kit.curtains(B, x, y, Zf, N, w=cw, h=2.3, mat="CT_PlasterYellow" if cu < 2 else "CT_PlasterRed")
    x, y = at(4.3, 0.75)
    kit.plant(B, x, y, Zf, 0.0, kind="fern", pot="CT_PlasterBlue", h=0.9, seed=2)
    x, y = at(6.1, v0)
    B.thing("bauer_window", "board", (x, y, Zf + 1.5), N, topic="view")
    # ---- kitchen
    x, y = at(4.4, v1)
    kit.kitchen_unit(B, x, y, Zf, S, modules="cupboard,sink,stove,fridge", wall_cupboards=False)
    B.thing("bauer_sink", "machine", _p3(F, 4.1, v1 - 0.25, Zf + 1.0), S, what="sink")
    B.thing("bauer_stove", "machine", _p3(F, 4.7, v1 - 0.25, Zf + 1.0), S, what="stove")
    x, y = at(6.5, 10.0)
    tb = kit.table(B, x, y, Zf, N, w=0.8, d=0.8)
    kit.samovar(B, x, y, tb["top"], N)
    B.thing("bauer_samovar", "machine", (x, y, Zf + 1.05), N, what="samovar")
    xa, ya = at(5.75, 10.0)
    kit.chair(B, xa, ya, Zf, E, kind="wood")
    B.thing("bauer_kitchen_seat", "seat", (xa, ya, Zf + 0.45), E, height=0.45)
    xb, yb = at(6.5, 11.05)
    kit.chair(B, xb, yb, Zf, S, kind="wood")
    # ---- hall
    x, y = at(2.35, 11.3)
    kit.hat_stand(B, x, y, Zf, 0.0)
    x, y = at(0.2, 10.2)
    kit.shoe_rack(B, x, y, Zf, E, w=0.8)
    x, y = at(0.2, 9.0)
    kit.mirror(B, x, y, Zf, E, zc=1.5, w=0.55, h=0.9)
    x, y = at(2.85, 8.5)
    kit.wall_phone(B, x, y, Zf, W, zc=1.45)
    x, y = at(1.45, 11.3)
    kit.rug(B, x, y, Zf, S, w=0.9, d=0.6, name="rug_1", seed=1)
    # ---- bedroom
    x, y = at(2.33, 6.65)
    kit.bed(B, x, y, Zf, S, size="single", blanket="CT_PlasterGreen")
    B.thing("bauer_bed", "bed", (x, y, Zf + 0.5), S, rest="private", height=0.5)
    x, y = at(1.55, 7.45)
    kit.bedside_table(B, x, y, Zf, S, lamp=True)
    x, y = at(0.2, 3.6)
    kit.wardrobe(B, x, y, Zf, E, w=1.2)
    x, y = at(1.5, 1.4)
    kit.loom(B, x, y, Zf, N)
    B.thing("bauer_net", "craft", (x, y, Zf + 1.0), N, what="net_mending")
    x, y = at(1.4, 4.4)
    kit.rug(B, x, y, Zf, N, w=1.5, d=2.4, name="rug_5", seed=5)
    x, y = at(0.2, 5.7)
    kit.picture(B, x, y, Zf, E, name="painting_lighthouse", w=0.6, zc=1.7)
    # ---- the door from the gallery (a caption for the hero standing at the threshold)
    B.thing("bauer_door", "door", _p3(F, 1.45 + 0.0, D - 0.1, Z0 + 1.1), N, to="flat")
    # ---- townsfolk: Colette's places
    B.spot("bauer_sofa_spot", "bench", (*at(Lb - 1.25, 5.35), Zf), W)
    B.spot("bauer_window_spot", "view", (*at(6.1, 1.0), Zf), S)
    B.spot("bauer_table_spot", "table", (*at(5.75, 10.0), Zf), E)
    B.spot("bauer_aquarium_spot", "view", (*at(5.0, 2.4), Zf), E)


def _lobby_extras(B, cid, F, Lb, D, sa, sb, Z, pre, mid, at):
    """Round 2: what makes each lobby its own (mailboxes with a thing, a light switch, and per lobby: bicycles and a
    dominoes table / the stair and the pram / a chess table and a planter wall)."""
    d = B.detail
    # light switch on the pier beside the front door
    x0, y0 = F.p(5.1, WALL)
    _fbox(d, "CT_ConcreteWhite", F, 5.06, 5.14, WALL, WALL + 0.015, Z + 1.24, Z + 1.36, faces="yXxzZ")
    _fbox(d, "CT_Steel", F, 5.085, 5.115, WALL + 0.015, WALL + 0.025, Z + 1.28, Z + 1.32, faces="yXxzZ")
    B.thing(pre + "switch", "machine", (x0, y0, 1.3), F.yaw(0, 1), what="switch", switch="hall")
    # the letter boxes on the left wall get their thing
    mx, my = F.p(WALL + 0.02, 1.8)
    B.thing(pre + "mail", "board", (mx, my, 1.45), F.yaw(1, 0), topic="mail")
    sbu = lambda off: (WALL + off) if sb > 0 else (Lb - WALL - off)
    if cid == 0:
        bx, by = F.p(6.45, 1.0)
        kit.bicycle(B, bx, by, Z, F.yaw(1, 0), kind="stand", mat="CT_SteelRed")
        bx, by = F.p(6.45, 1.0)
        B.thing(pre + "bikes", "machine", (bx, by, 0.8), F.yaw(-1, 0), what="bike_rack")
        # the dominoes table: two chairs, a stool, the tiles and a cup of cold tea
        tx, ty = F.p(sbu(1.5), 3.9)
        tb = kit.table(B, tx, ty, Z, F.yaw(0, 1), w=0.8, d=0.8)
        for cu, cv, dy in ((0.0, -0.75, F.yaw(0, 1)), (0.0, 0.75, F.yaw(0, -1)), (-0.85, 0.0, F.yaw(1, 0))):
            cx, cy = tx + F.eu[0] * cu + F.ev[0] * cv, ty + F.eu[1] * cu + F.ev[1] * cv
            kit.chair(B, cx, cy, Z, dy, kind="wood")
        for k in range(7):
            ux, vx = -0.22 + k * 0.07, 0.04 * ((-1) ** k)
            _fbox(d, "CT_ConcreteDark", F, sbu(1.5) + ux - 0.03, sbu(1.5) + ux + 0.03, 3.9 + vx - 0.055, 3.9 + vx + 0.055,
                  tb["top"], tb["top"] + 0.014, faces="xXyYZ")
        kit.cup(B, tx + F.eu[0] * 0.2 + F.ev[0] * 0.25, ty + F.eu[1] * 0.2 + F.ev[1] * 0.25, tb["top"], 0.0)
        B.thing(pre + "dominoes", "play", (tx, ty, tb["top"]), F.yaw(0, 1), what="dominoes")
        B.spot("lobby_table_0", "table", (tx - F.eu[0] * 0.85, ty - F.eu[1] * 0.85, 0.0), F.yaw(1, 0))
    elif cid == 2:
        tx, ty = F.p(sbu(1.5), 3.9)
        tb = kit.chess_table(B, tx, ty, Z, F.yaw(0, 1), seed=cid)
        for cv, dy in ((-0.7, F.yaw(0, 1)), (0.7, F.yaw(0, -1))):
            kit.chair(B, tx + F.ev[0] * cv, ty + F.ev[1] * cv, Z, dy, kind="wood")
        B.thing(pre + "chess", "play", (tx, ty, tb["top"]), F.yaw(0, 1), what="chess")
        B.spot("lobby_table_2", "table", (tx + F.ev[0] * -0.7, ty + F.ev[1] * -0.7, 0.0), F.yaw(0, 1))
        px, py = F.p(1.9, D - WALL - 0.3)
        kit.planter(B, px, py, Z, F.yaw(0, -1), w=1.5, d=0.45, h=0.5, seed=cid)
        B.thing(pre + "plants", "machine", (px, py, 0.7), F.yaw(0, -1), what="watering_can")



def _cat(B, x, y, z, yaw, mat="CT_ConcreteDark"):
    """A grey cat with a white forepaw, sitting, looking along yaw (kit convention)."""
    f = fu.Fr(x, y, yaw, z)
    d = B.detail
    fu.lbox(d, mat, f, -0.07, 0.07, -0.12, 0.12, 0.0, 0.17)           # body
    fu.lbox(d, mat, f, -0.05, 0.05, -0.17, -0.06, 0.15, 0.26)         # head (forward = -w here: w grows toward -y at yaw 0)
    for sx in (-0.035, 0.035):
        fu.lbox(d, mat, f, sx - 0.012, sx + 0.012, -0.15, -0.11, 0.26, 0.295)   # ears
    fu.lbox(d, "CT_ConcreteWhite", f, -0.045, 0.0, -0.15, -0.1, 0.0, 0.05)       # the white forepaw
    fu.rod(d, mat, f.p(0.0, 0.12, 0.04), f.p(0.12, 0.2, 0.08), 0.02)             # the tail along the floor
    fu.rod(d, mat, f.p(0.12, 0.2, 0.08), f.p(0.14, 0.3, 0.2), 0.018)


def _gallery(B, FP, cores, zs):
    """The first storey's long gallery between the middle core's door and flat 1-17: two benches, a neighbour's cat, washing,
    flower boxes, a view over the parapet, a lamp, the mat and pots at Colette's door."""
    Z = zs[0]
    c = cores[1]
    d = B.detail
    VW = BODY + 0.23                                  # bench back against the flats' wall
    VP = BODY + GAL - 0.14                            # inner face of the parapet
    Fc, Lc = _bay_frame(FP, c)
    F1, L1 = _bay_frame(FP, c + 1)
    F2, L2 = _bay_frame(FP, c + 2)
    # bench by the core door
    x, y = Fc.p(5.1, VW)
    kit.bench(B, x, y, Z, Fc.yaw(0, 1), w=1.5, kind="park")
    B.thing("gallery_bench_core", "seat", (x, y, Z + 0.46), Fc.yaw(0, 1), height=0.45)
    B.spot("gallery_bench_core_spot", "bench", (*Fc.p(5.1, VW + 0.45), Z), Fc.yaw(0, 1))
    # bench with the cat, in front of flat 1-16
    x, y = F1.p(2.4, VW)
    kit.bench(B, x, y, Z, F1.yaw(0, 1), w=1.5, kind="park")
    B.thing("gallery_bench", "seat", (x, y, Z + 0.46), F1.yaw(0, 1), height=0.45)
    B.spot("gallery_bench_spot", "bench", (*F1.p(2.4, VW + 0.45), Z), F1.yaw(0, 1))
    cx, cy = F1.p(2.95, VW + 0.02)
    _cat(B, cx, cy, Z + 0.45, F1.yaw(0, 1))
    B.thing("gallery_cat", "machine", (cx, cy, Z + 0.62), F1.yaw(0, 1), what="gallery_cat")
    # washing between two poles by the parapet
    va = VP - 0.15
    for u in (3.7, 7.1):
        fu.rod(d, "CT_Steel", _p3(F1, u, va, Z), _p3(F1, u, va, Z + 2.15), 0.022)
    fu.rod(d, "CT_Steel", _p3(F1, 3.7, va, Z + 2.12), _p3(F1, 7.1, va, Z + 2.12), 0.008)
    for u, w_, h_, mt in ((3.95, 0.5, 0.75, "CT_PlasterBlue"), (4.6, 0.42, 0.6, "CT_ConcreteWhite"), (5.2, 0.55, 0.85, "CT_PlasterYellow"),
                          (5.85, 0.3, 0.45, "CT_PlasterRed"), (6.4, 0.5, 0.7, "CT_ConcreteWhite")):
        _fbox(d, mt, F1, u, u + w_, va - 0.015, va + 0.015, Z + 2.12 - h_, Z + 2.12, faces="yYxXz")
    wx, wy = F1.p(5.2, va)
    B.thing("gallery_washing", "machine", (wx, wy, Z + 1.7), F1.yaw(0, -1), what="clothesline")
    # Colette's door: mat, a pot, flower boxes under the kitchen window and on the parapet, a lamp
    x, y = F2.p(1.45, BODY + 0.45)
    kit.rug(B, x, y, Z + 0.01, F2.yaw(0, 1), w=0.9, d=0.55, name="rug_2", seed=2)
    x, y = F2.p(0.45, BODY + 0.3)
    kit.plant(B, x, y, Z, 0.0, kind="cactus", pot="CT_PlasterRed", h=0.5, seed=8)
    x, y = F2.p(4.1, BODY + 0.3)
    kit.planter(B, x, y, Z, F2.yaw(0, 1), w=1.5, d=0.3, h=0.4, seed=9, solid=False)
    B.thing("gallery_flowers", "machine", (x, y, Z + 0.62), F2.yaw(0, 1), what="flower_boxes")
    x, y = F2.p(6.5, VP - 0.15)
    kit.planter(B, x, y, Z, F2.yaw(0, 1), w=1.3, d=0.3, h=0.4, seed=11, solid=False)
    B.lamp(_p3(F2, 1.45, BODY + 0.55, Z + 2.5), lumens=300, colour="#FFD9A8", kind="lobby", radius=5.0)
    x, y = F2.p(4.9, VP)
    B.thing("gallery_view", "board", (x, y, Z + 1.25), F2.yaw(0, -1), topic="view")
    B.spot("gallery_view_spot", "view", (*F2.p(4.9, VP - 0.5), Z), F2.yaw(0, 1))
