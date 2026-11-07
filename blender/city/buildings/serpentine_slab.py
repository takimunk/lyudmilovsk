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
        # soffit over the pilotis
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
                F = B.facade(p0, p1, z0, z1, mat, holes=holes, depth=WALL, back=False)
                # back facade of this flat with the gallery door and a kitchen window
                fid = new_flat(j + 1)
                du0 = 0.9 if (i + j) % 2 == 0 else Lb - 0.9 - DOOR_W
                bholes = [(du0, du0 + DOOR_W, z0, z0 + DOOR_H)]
                kw = (3.3, 4.9) if du0 < 3 else (2.7, 4.3)
                bholes.append((kw[0], kw[1], z0 + 1.0, z0 + 2.1))
                # the back facade goes from q1 to q0 (outside faces +y); u runs from q1
                Fb = B.facade(q1, q0, z0, z1, "CT_ConcreteWhite", holes=[(Lb - h[1], Lb - h[0], h[2], h[3]) for h in bholes],
                              depth=WALL, back=False)
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
            _core_bay(B, i, core_id, p0, p1, q0, q1, zs, rnd, new_flat)

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
        _v_column(m, q, d, nl)
        if not (i in cores or (i - 1) in cores):
            pass
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


def _v_column(m, q, d, nl):
    """A V-shaped column: a narrow foot on the ground spreading to two tops under the slab (2 legs of one V)."""
    def pt(cx, cy, dd, nn, a, b, z):
        return (cx + d[0] * (dd + a) + nl[0] * (nn + b), cy + d[1] * (dd + a) + nl[1] * (nn + b), z)
    for sgn in (-1, 1):
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


def _core_bay(B, i, cid, p0, p1, q0, q1, zs, rnd, new_flat):
    """A stair/lift core bay above the pilotis: blue tile mosaic with white bands and slit windows."""
    Lb = math.dist(p0, p1)
    ZR = zs[-1]
    for j in range(NST):
        z0 = zs[j]
        for side in ("front", "back"):
            a, b = (p0, p1) if side == "front" else (q1, q0)
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
    B.spot("lobby_bench_%d" % cid, "bench", (cx[0], cx[1], 0.0), yaw_of((-nl[0], -nl[1])))
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
