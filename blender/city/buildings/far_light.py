"""The Far Light (маяк Дальний): the real rock the cruise lands on (docs/ship.md, "The landing at the Far Light").

A bare granite skerry about 150 x 75 m, 8 m high, 26 km out at sea: a concrete jetty on the lee side, a railed path up
to the red-and-white banded tower (28 m to the lamp: a ground room, a watch room, the lantern gallery outside), the
keeper's hut (one furnished room), a fog bell, a weather box, a flagpole, a walled vegetable patch, a rain-water tank,
bird ledges, a memorial plate. The whole island is this "building": the rock is the shell (exact collision), the hero
walks it.

IT MUST MATCH blender/ship/seascape_kit.py `lighthouse()`: the far-view copy the ship's seascape shows under way stands
exactly here when the ship anchors, and the game swaps one for the other. The same height function (skerry_height with
the same blobs and seed), the same tower profile and bands, the hut, the oil store, the mast and the jetty are used; the
real one adds paths (the rock is levelled along them), interiors and things. Change both or neither.

Frame: the kit's island frame is (kx, ky, kz): kx = the chart's X, ky = MINUS the chart's Y, kz from the sea's datum.
Building frame: bx = kx, by = ky + HALF_D, bz = kz - 1.5 (the plot P-FARLIGHT has yaw 90 and stands on terrace "B",
z -41.5 = datum + 1.5, like P-SHIP), so the plot's map point is the island's chart point: tools/city_plan.py puts it
FARLIGHT_OFFSET from the cruise stage, tools/ship/cruise.py ASHORE["offset_m"] is the same number, cruisesim checks it.

Links (fades, as the town's lighthouse): tower_stair_foot <-> watch_stair_top, watch_ladder <-> gallery_hatch,
jetty_gate <-> P-SHIP/launch_ashore.  No flats, no street entrance.
"""
import math
import os

import numpy as np

from citykit import box
import geom
import interior_kit as kit

import _rocknoise as SK  # the far-view copy's own noise and height function (a copy: see that file)

INFO = {
    "name_ru": "Маяк Дальний", "name_en": "The Far Light",
    "style": "sea mark", "use": "public",
    "inspired_by": "rock lighthouses of the Baltic and the White Sea: a banded tower, a keeper's hut, a jetty on the lee side",
}

HALF_D = 72.0
ZOFF = -1.5
X0, X1, Y0, Y1 = -110.0, 112.0, -70.0, 72.0        # the kit's grid
STEP = 1.5
T = (6.0, 2.0, 7.2)                                # the tower's foot (kit frame)
HUT = (-22.0, 6.0, 5.4)
HUT_ROT = math.radians(12.0)
J0 = (14.0, 30.0)
J1 = (24.0, 66.0)
JETTY_Z = 2.3
ROCK, ROCK_STEEP = "CT_ConcreteDark", "CT_Wet"

# the paths: polylines (kx, ky, kz); the rock is levelled 1.3 m either side of them
PATHS = {
    "jetty": [(14.0, 30.0, 2.3), (12.5, 22.0, 4.0), (10.5, 13.0, 6.2), (9.0, 8.5, 7.2)],
    "hut": [(1.0, 5.5, 7.2), (-8.0, 10.5, 6.2), (-16.0, 12.0, 5.5), (-22.8, 11.4, 5.4)],
    "patch": [(-22.8, 11.4, 5.4), (-29.0, 13.5, 4.9), (-35.0, 15.0, 4.6)],
    "bell": [(11.5, 0.0, 7.2), (20.0, -3.0, 6.0), (30.0, -9.5, 4.9), (40.0, -8.5, 4.8), (46.0, -8.0, 5.0)],
}
PATH_HALF = 1.3


def P(kx, ky, kz=0.0):
    return (kx, ky + HALF_D, kz + ZOFF)


def kyaw(deg):
    """A kit yaw (0 faces -by) from a compass-like angle in the kit frame: the direction (cos, sin) the thing FACES."""
    return (math.degrees(math.atan2(math.cos(math.radians(deg)), -math.sin(math.radians(deg)))) + 360.0) % 360.0


def face_yaw(dx, dy):
    """Kit yaw of a thing that faces along (dx, dy) in the building frame (0 = -y, 90 = +x)."""
    return round(math.degrees(math.atan2(dx, -dy)) % 360.0, 1)


# ------------------------------------------------------------------------------------------------ the rock
def _seg_dist(X, Y, a, b):
    ax, ay, az = a
    bx, by, bz = b
    dx, dy = bx - ax, by - ay
    t = np.clip(((X - ax) * dx + (Y - ay) * dy) / (dx * dx + dy * dy), 0.0, 1.0)
    return np.hypot(X - (ax + t * dx), Y - (ay + t * dy)), az + t * (bz - az)


def heights():
    xs = np.arange(X0, X1 + 1e-6, STEP)
    ys = np.arange(Y0, Y1 + 1e-6, STEP)
    X, Y = np.meshgrid(xs, ys, indexing="ij")
    blobs = [(0, 0, 70, 38, 7.5, 0.15), (-48, 10, 38, 26, 4.5, -0.3), (46, -8, 36, 22, 5.0, 0.4), (84, 18, 14, 10, 1.6, 0.0),
             (-88, -22, 12, 9, 1.2, 0.5), (20, 44, 16, 9, 1.0, 0.2)]
    H = SK.skerry_height(X, Y, blobs, 81, rough=0.7)
    seat = np.exp(-(((X - 6) / 9.0) ** 2 + ((Y - 2) / 9.0) ** 2))
    H = H * (1 - seat) + 7.4 * seat
    seat2 = np.exp(-(((X + 22) / 7.0) ** 2 + ((Y - 6) / 6.0) ** 2))
    H = H * (1 - seat2) + 5.6 * seat2
    # level ground: the tower's apron, the hut's floor and yard, the walled patch, the bell's knoll, then the paths
    def level(dist, z, flat, blend):
        nonlocal H
        k = 1.0 - SK.smooth(flat, flat + blend, dist)
        H = H * (1 - k) + z * k
    level(np.hypot(X - T[0], Y - T[1]), T[2], 6.6, 2.5)
    c, s = math.cos(HUT_ROT), math.sin(HUT_ROT)
    u = (X - HUT[0]) * c + (Y - HUT[1]) * s
    v = -(X - HUT[0]) * s + (Y - HUT[1]) * c
    level(np.maximum(np.abs(u) - 5.4, np.abs(v) - 3.9).clip(0), HUT[2], 0.01, 2.2)
    level(np.maximum(np.abs(u) - 5.0, np.abs(v - 5.2) - 2.6).clip(0), HUT[2], 0.01, 1.8)      # the yard before the door
    level(np.hypot(X + 37.0, Y - 16.5), 4.6, 4.2, 2.0)
    level(np.hypot(X - 46.0, Y + 8.0), 5.0, 2.6, 2.2)
    for pts in PATHS.values():
        for a, b in zip(pts, pts[1:]):
            d, z = _seg_dist(X, Y, a, b)
            k = 1.0 - SK.smooth(PATH_HALF, PATH_HALF + 1.6, d)
            H = H * (1 - k) + z * k
    return xs, ys, H


def rock(B, xs, ys, H):
    m = B.m
    floor = SK.SKIRT
    Hc = np.maximum(H, floor)
    nx, ny = len(xs), len(ys)
    for i in range(nx - 1):
        for j in range(ny - 1):
            h = (H[i, j], H[i + 1, j], H[i + 1, j + 1], H[i, j + 1])
            if max(h) <= floor:
                continue
            p = [P(xs[i], ys[j], Hc[i, j]), P(xs[i + 1], ys[j], Hc[i + 1, j]), P(xs[i + 1], ys[j + 1], Hc[i + 1, j + 1]),
                 P(xs[i], ys[j + 1], Hc[i, j + 1])]
            for tri in ((p[0], p[1], p[2]), (p[0], p[2], p[3])):
                n = geom.norm(geom.cross(geom.sub(tri[1], tri[0]), geom.sub(tri[2], tri[0])))
                if abs(n[0]) + abs(n[1]) + abs(n[2]) < 0.5:
                    continue
                wet = max(t[2] for t in tri) - ZOFF < 0.9
                m.face(ROCK_STEEP if (n[2] < 0.72 or wet) else ROCK, list(tri), [(t[0], t[1]) for t in tri])


def ground(xs, ys, H, kx, ky):
    """The rock's height (kit z) at a point: bilinear."""
    fx = (kx - xs[0]) / STEP
    fy = (ky - ys[0]) / STEP
    i = int(min(max(fx, 0), len(xs) - 2))
    j = int(min(max(fy, 0), len(ys) - 2))
    a, b = fx - i, fy - j
    return float(H[i, j] * (1 - a) * (1 - b) + H[i + 1, j] * a * (1 - b) + H[i + 1, j + 1] * a * b + H[i, j + 1] * (1 - a) * b)


# ------------------------------------------------------------------------------------------------ helpers
def lathe(m, mats, profile, segs=20, skip=(), inward=False, centre=None):
    """Rings of a profile [(r, kz)] round the tower's axis; skip = segment indices left open (a doorway)."""
    cx, cy = (T[0], T[1]) if centre is None else centre
    if isinstance(mats, str):
        mats = [mats] * (len(profile) - 1)
    for k in range(len(profile) - 1):
        (r0, z0), (r1, z1) = profile[k], profile[k + 1]
        for sgm in range(segs):
            if sgm in skip:
                continue
            a0 = 2 * math.pi * sgm / segs
            a1 = 2 * math.pi * (sgm + 1) / segs
            q = [P(cx + r0 * math.cos(a0), cy + r0 * math.sin(a0), z0), P(cx + r0 * math.cos(a1), cy + r0 * math.sin(a1), z0),
                 P(cx + r1 * math.cos(a1), cy + r1 * math.sin(a1), z1), P(cx + r1 * math.cos(a0), cy + r1 * math.sin(a0), z1)]
            if inward:
                q.reverse()
            if r1 < 1e-6:
                q = [q[0], q[1], q[2]] if not inward else [q[1], q[2], q[3]]
            L0 = r0 * (a1 - a0)
            hgt = math.hypot(z1 - z0, r1 - r0)
            uv = [(0, 0), (L0, 0), (L0, hgt), (0, hgt)]
            if inward:
                uv.reverse()
            m.face(mats[k], q, uv[:len(q)] if len(q) == 4 else None)


def disc(m, mat, r0, r1, kz, up=True, segs=20, centre=None):
    """A flat ring (r0 < r1) or a disc (r0 = 0) at kz."""
    cx, cy = (T[0], T[1]) if centre is None else centre
    for sgm in range(segs):
        a0 = 2 * math.pi * sgm / segs
        a1 = 2 * math.pi * (sgm + 1) / segs
        q = [P(cx + r1 * math.cos(a0), cy + r1 * math.sin(a0), kz), P(cx + r1 * math.cos(a1), cy + r1 * math.sin(a1), kz)]
        if r0 > 1e-6:
            q += [P(cx + r0 * math.cos(a1), cy + r0 * math.sin(a1), kz), P(cx + r0 * math.cos(a0), cy + r0 * math.sin(a0), kz)]
        else:
            q.append(P(cx, cy, kz))
        if not up:
            q.reverse()
        m.face(mat, q)


class Turned:
    """Build in a local frame (origin at a kit point, turned by rot about z), then put into the building."""

    def __init__(self, B, at, rot):
        self.B, self.at, self.rot = B, at, rot

    def __enter__(self):
        self.keep = (self.B.m, self.B.detail)
        self.B.m, self.B.detail = geom.Mesh(), geom.Mesh()
        return self

    def to(self, x, y, z=0.0):
        c, s = math.cos(self.rot), math.sin(self.rot)
        return P(self.at[0] + x * c - y * s, self.at[1] + x * s + y * c, self.at[2] + z)

    def yaw(self, local_yaw):
        """Kit yaw of something that has local kit yaw local_yaw in the turned frame."""
        return round((local_yaw - math.degrees(self.rot)) % 360.0, 1)

    def __exit__(self, *exc):
        fn = lambda p: self.to(p[0], p[1], p[2])
        m, d = self.B.m, self.B.detail
        self.B.m, self.B.detail = self.keep
        self.B.m.extend(m.transformed(fn))
        self.B.detail.extend(d.transformed(fn))
        return False


def bar_rail(B, pts, h=1.05, solid=True):
    """An iron rail along kit points (kx, ky, kz): posts and two bars; in the shell (it stops the hero) when solid."""
    m = B.m if solid else B.detail
    for a, b in zip(pts, pts[1:]):
        L = math.hypot(b[0] - a[0], b[1] - a[1])
        n = max(1, int(round(L / 1.6)))
        for k in range(n + 1):
            f = k / n
            x, y, z = a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f, a[2] + (b[2] - a[2]) * f
            px, py, pz = P(x, y, z)
            box(m, "CT_Steel", px - 0.03, px + 0.03, py - 0.03, py + 0.03, pz - 0.3, pz + h)
        ux, uy = (b[0] - a[0]) / L, (b[1] - a[1]) / L
        nx, ny = -uy * 0.025, ux * 0.025
        for zz in (h - 0.05, h * 0.5):
            q0 = P(a[0], a[1], a[2] + zz)
            q1 = P(b[0], b[1], b[2] + zz)
            # a square bar as four sloping quads
            c = [(nx, ny, -0.025), (nx, ny, 0.025), (-nx, -ny, 0.025), (-nx, -ny, -0.025)]
            for k in range(4):
                o0, o1 = c[k], c[(k + 1) % 4]
                m.face("CT_Steel", [(q0[0] + o0[0], q0[1] + o0[1], q0[2] + o0[2]), (q1[0] + o0[0], q1[1] + o0[1], q1[2] + o0[2]),
                                    (q1[0] + o1[0], q1[1] + o1[1], q1[2] + o1[2]), (q0[0] + o1[0], q0[1] + o1[1], q0[2] + o1[2])])


def offset_path(pts, side):
    out = []
    for k, p in enumerate(pts):
        a = pts[max(k - 1, 0)]
        b = pts[min(k + 1, len(pts) - 1)]
        L = math.hypot(b[0] - a[0], b[1] - a[1])
        out.append((p[0] - (b[1] - a[1]) / L * side, p[1] + (b[0] - a[0]) / L * side, p[2]))
    return out


# ------------------------------------------------------------------------------------------------ the tower
DOOR_SEG = 9                 # of 20: the doorway looks along 171 degrees (toward the hut)
WATCH_Z = 18.6               # the watch room's floor over the tower's foot
GALLERY_Z = 22.75


def tower(B):
    m, d = B.m, B.detail
    z0 = T[2]
    bands = [(3.7, 0.0), (3.5, 4.4), (3.3, 8.8), (3.1, 13.2), (2.9, 17.6), (2.7, 22.0)]
    cols = ["CT_PlasterRed", "CT_ConcreteWhite", "CT_PlasterRed", "CT_ConcreteWhite", "CT_PlasterRed"]
    lathe(m, "CT_Concrete", [(4.3, z0 - 1.6), (4.3, z0)], skip=())
    disc(m, "CT_Concrete", 3.7, 4.3, z0)
    # the first band with the doorway (2.15 m high), the rest whole
    r_at = lambda h: 3.7 - h * (1.0 / 22.0)
    lathe(m, cols[0], [(3.7, z0), (r_at(2.15), z0 + 2.15)], skip=(DOOR_SEG,))
    lathe(m, cols[0], [(r_at(2.15), z0 + 2.15), (3.5, z0 + 4.4)])
    for k in range(1, 5):
        lathe(m, cols[k], [(bands[k][0], z0 + bands[k][1]), (bands[k + 1][0], z0 + bands[k + 1][1])])
    # the ground room: inner wall r 2.9, floor, ceiling 3.0; the doorway's jambs and head
    RI = 2.9
    lathe(m, "CT_Interior", [(RI, z0), (RI, z0 + 2.15)], skip=(DOOR_SEG,), inward=True)
    lathe(m, "CT_Interior", [(RI, z0 + 2.15), (RI, z0 + 3.0)], inward=True)
    disc(m, "CT_Floor", 0.0, 3.7, z0 + 0.0)
    disc(m, "CT_Interior", 0.0, RI, z0 + 3.0, up=False)
    a0 = 2 * math.pi * DOOR_SEG / 20
    a1 = 2 * math.pi * (DOOR_SEG + 1) / 20
    for a, flip in ((a0, False), (a1, True)):
        q = [P(T[0] + RI * math.cos(a), T[1] + RI * math.sin(a), z0), P(T[0] + 3.7 * math.cos(a), T[1] + 3.7 * math.sin(a), z0),
             P(T[0] + r_at(2.15) * math.cos(a), T[1] + r_at(2.15) * math.sin(a), z0 + 2.15), P(T[0] + RI * math.cos(a), T[1] + RI * math.sin(a), z0 + 2.15)]
        if flip:
            q.reverse()
        m.face("CT_ConcreteWhite", q)
    q = [P(T[0] + RI * math.cos(a0), T[1] + RI * math.sin(a0), z0 + 2.15), P(T[0] + r_at(2.15) * math.cos(a0), T[1] + r_at(2.15) * math.sin(a0), z0 + 2.15),
         P(T[0] + r_at(2.15) * math.cos(a1), T[1] + r_at(2.15) * math.sin(a1), z0 + 2.15), P(T[0] + RI * math.cos(a1), T[1] + RI * math.sin(a1), z0 + 2.15)]
    m.face("CT_ConcreteWhite", q)
    # the foot of the iron stair (drawn, not climbed: the links carry the hero), its newel
    for k in range(9):
        a = math.radians(300 - k * 22)
        px, py, pz = P(T[0] + 1.75 * math.cos(a), T[1] + 1.75 * math.sin(a), z0 + 0.2 * (k + 1))
        box(d, "CT_Steel", px - 0.42, px + 0.42, py - 0.42, py + 0.42, pz - 0.04, pz)
    px, py, pz = P(T[0], T[1], z0)
    box(m, "CT_Steel", px - 0.09, px + 0.09, py - 0.09, py + 0.09, pz, pz + 3.0)
    # the watch room under the gallery: floor at WATCH_Z, inner r 2.25
    RW = 2.25
    disc(m, "CT_Floor", 0.0, 2.95, z0 + WATCH_Z)
    disc(m, "CT_Interior", 0.0, 2.95, z0 + WATCH_Z - 0.2, up=False)
    lathe(m, "CT_Interior", [(RW, z0 + WATCH_Z), (RW, z0 + 21.9)], inward=True)
    disc(m, "CT_Interior", 0.0, RW, z0 + 21.9, up=False)
    box(m, "CT_Steel", px - 0.09, px + 0.09, py - 0.09, py + 0.09, pz + WATCH_Z, pz + 21.9)
    # the gallery: a deck, its rail as a solid band (the far copy draws it so), the lantern
    lathe(m, "CT_Steel", [(2.7, z0 + 22.0), (3.7, z0 + 22.5), (3.7, z0 + GALLERY_Z)])
    disc(m, "CT_Steel", 0.0, 3.7, z0 + GALLERY_Z)
    lathe(m, "CT_Steel", [(3.62, z0 + GALLERY_Z), (3.62, z0 + GALLERY_Z + 1.1)])
    lathe(m, "CT_Steel", [(3.56, z0 + GALLERY_Z), (3.56, z0 + GALLERY_Z + 1.1)], inward=True)
    disc(m, "CT_Steel", 3.56, 3.62, z0 + GALLERY_Z + 1.1)
    lathe(m, "CT_ConcreteWhite", [(2.0, z0 + GALLERY_Z), (2.0, z0 + 23.6)], segs=16)
    lathe(m, "CT_Glass", [(1.95, z0 + 23.6), (1.95, z0 + 26.3)], segs=16)
    lathe(m, "CT_Roof", [(2.25, z0 + 26.3), (1.6, z0 + 27.3), (0.5, z0 + 28.0), (0.12, z0 + 28.2), (0.12, z0 + 29.4), (0.0, z0 + 29.5)], segs=16)
    disc(m, "CT_Roof", 0.0, 2.25, z0 + 26.3, up=False, segs=16)
    for k in range(16):                                        # glazing bars
        a = 2 * math.pi * k / 16
        gx, gy, gz = P(T[0] + 1.97 * math.cos(a), T[1] + 1.97 * math.sin(a), z0 + 23.6)
        box(d, "CT_Steel", gx - 0.025, gx + 0.025, gy - 0.025, gy + 0.025, gz, gz + 2.7)
    lathe(d, "CT_Opal", [(0.55, z0 + 24.3), (0.75, z0 + 25.0), (0.55, z0 + 25.7), (0.0, z0 + 25.7)], segs=10)   # the lens
    lathe(d, "CT_Brass", [(0.3, z0 + 23.6), (0.3, z0 + 24.3)], segs=8)
    # small dark windows up the stem, as on the far copy
    for k, zz in enumerate((6.0, 11.0, 16.0, 20.0)):
        a = math.radians(40 + k * 95)
        r = 3.72 - 0.045 * zz
        wx, wy, wz = P(T[0] + r * math.cos(a), T[1] + r * math.sin(a), z0 + zz)
        box(d, "CT_Glass", wx - 0.22, wx + 0.22, wy - 0.22, wy + 0.22, wz, wz + 0.8)
    # lamps
    B.lamp(P(T[0] + 1.2, T[1] + 1.0, z0 + 2.6), 500, kind="public", radius=5.0)
    B.lamp(P(T[0] + 0.9, T[1] - 0.9, z0 + WATCH_Z + 2.8), 500, kind="public", radius=5.0)
    # --- things
    da = (a0 + a1) / 2
    ox, oy = math.cos(da), math.sin(da)
    B.thing("memorial_plate", "board", P(T[0] + 3.74 * math.cos(a1 + 0.22), T[1] + 3.74 * math.sin(a1 + 0.22), z0 + 1.5),
            face_yaw(math.cos(a1 + 0.22), math.sin(a1 + 0.22)))
    wx, wy, wz = P(T[0] + 3.7 * math.cos(a1 + 0.22), T[1] + 3.7 * math.sin(a1 + 0.22), z0 + 1.3)
    box(d, "CT_Brass", wx - 0.2, wx + 0.2, wy - 0.2, wy + 0.2, wz, wz + 0.4)
    # the stair links: foot (ground room, opposite the door) <-> watch room; watch room ladder <-> the gallery
    B.thing("tower_stair_foot", "door", P(T[0] - 1.7 * ox, T[1] - 1.7 * oy, z0 + 1.1), face_yaw(ox, oy),
            to="watch_stair_top")
    B.thing("watch_stair_top", "door", P(T[0] + 1.3, T[1] + 1.2, z0 + WATCH_Z + 1.1), face_yaw(-1.0, -1.0), to="tower_stair_foot")
    B.thing("watch_ladder", "door", P(T[0] - 1.5, T[1] + 1.0, z0 + WATCH_Z + 1.2), face_yaw(1.0, -0.6), to="gallery_hatch")
    lx, ly, lz = P(T[0] - 1.8, T[1] + 1.2, z0 + WATCH_Z)
    for sx in (-0.22, 0.22):
        box(d, "CT_Steel", lx + sx - 0.02, lx + sx + 0.02, ly - 0.02, ly + 0.02, lz, lz + 3.3)
    for k in range(10):
        box(d, "CT_Steel", lx - 0.22, lx + 0.22, ly - 0.015, ly + 0.015, lz + 0.3 * (k + 1), lz + 0.3 * (k + 1) + 0.03)
    # the watch room: the clockwork of the lens, the watch desk, a stool
    B.thing("lens_clockwork", "machine", P(T[0] + 0.0, T[1] - 1.5, z0 + WATCH_Z + 1.0), face_yaw(0.0, 1.0), what="clockwork")
    cx, cy, cz = P(T[0], T[1] - 1.75, z0 + WATCH_Z)
    box(m, "CT_Steel", cx - 0.35, cx + 0.35, cy - 0.25, cy + 0.25, cz, cz + 1.2)
    box(d, "CT_Brass", cx - 0.2, cx + 0.2, cy + 0.25, cy + 0.29, cz + 0.6, cz + 1.0)
    B.thing("watch_desk", "board", P(T[0] + 1.55, T[1] - 0.8, z0 + WATCH_Z + 0.9), face_yaw(-1.0, 0.5))
    kit.table(B, *P(T[0] + 1.6, T[1] - 0.85, z0 + WATCH_Z), yaw=face_yaw(-1.0, 0.5), w=0.9, d=0.5, solid=False)
    # the gallery (the hatch is on the hut's side; the view of the ship is on the jetty's side, +ky)
    B.thing("gallery_hatch", "door", P(T[0] - 2.75, T[1] - 0.4, z0 + GALLERY_Z + 0.9), face_yaw(-1.0, 0.0), to="watch_ladder")
    hx, hy, hz = P(T[0] - 2.75, T[1] - 0.4, z0 + GALLERY_Z)
    box(d, "CT_SteelRed", hx - 0.35, hx + 0.35, hy - 0.35, hy + 0.35, hz, hz + 0.05)
    B.thing("gallery_view", "board", P(T[0] + 0.6, T[1] + 3.5, z0 + GALLERY_Z + 1.1), face_yaw(0.0, -1.0), topic="sea_view")
    B.thing("lantern", "board", P(T[0] + 2.0, T[1] - 0.2, z0 + GALLERY_Z + 1.4), face_yaw(1.0, 0.0))
    B.thing("gallery_north", "board", P(T[0] + 0.3, T[1] - 3.5, z0 + GALLERY_Z + 1.1), face_yaw(0.0, 1.0))
    B.spot("gallery_stand", "stand", P(T[0] + 0.8, T[1] + 2.8, z0 + GALLERY_Z), face_yaw(0.0, 1.0))
    B.spot("farlight_gallery", "mark", P(T[0] + 0.8, T[1] + 2.8, z0 + GALLERY_Z), face_yaw(0.0, 1.0))


# ------------------------------------------------------------------------------------------------ the hut
def hut(B):
    """8.0 x 5.2 m outside, one room 7.4 x 4.6; the door in the +y wall (the jetty's side), three windows in the -y wall."""
    with Turned(B, HUT, HUT_ROT) as F:
        m, d = B.m, B.detail
        W, D, Hh = 4.0, 2.6, 2.7
        t = 0.3
        box(m, "CT_Concrete", -W - 0.15, W + 0.15, -D - 0.15, D + 0.15, -1.2, 0.0)             # the plinth
        box(m, "CT_Wood", -W + t, W - t, -D + t, D - t, 0.0, 0.05, faces="Z")                    # the floor
        # walls: -y with three window holes, +y with the door, the ends
        for (x0, x1) in ((-W, -2.85), (-1.95, -0.25), (0.65, 2.15), (3.05, W)):
            box(m, "CT_ConcreteWhite", x0, x1, -D, -D + t, 0.0, Hh)
        for wx in (-2.4, 0.2, 2.6):
            box(m, "CT_ConcreteWhite", wx - 0.45, wx + 0.45, -D, -D + t, 0.0, 0.95)
            box(m, "CT_ConcreteWhite", wx - 0.45, wx + 0.45, -D, -D + t, 1.95, Hh)
            box(m, "CT_Glass", wx - 0.45, wx + 0.45, -D + 0.12, -D + 0.15, 0.95, 1.95)
            box(d, "CT_FrameWhite", wx - 0.02, wx + 0.02, -D + 0.08, -D + 0.19, 0.95, 1.95)
        box(m, "CT_ConcreteWhite", -W, -0.6, D - t, D, 0.0, Hh)
        box(m, "CT_ConcreteWhite", 0.6, W, D - t, D, 0.0, Hh)
        box(m, "CT_ConcreteWhite", -0.6, 0.6, D - t, D, 2.15, Hh)
        box(m, "CT_ConcreteWhite", -W, -W + t, -D + t, D - t, 0.0, Hh)
        box(m, "CT_ConcreteWhite", W - t, W, -D + t, D - t, 0.0, Hh)
        box(m, "CT_Interior", -W + t, W - t, -D + t, D - t, Hh - 0.08, Hh)                       # the ceiling
        box(d, "CT_Door", -0.62, -0.57, D, D + 0.95, 0.0, 2.1)                                    # the door, open
        # the roof: a red gable
        roof = [(-4.3, -2.9, Hh), (4.3, -2.9, Hh), (4.3, 2.9, Hh), (-4.3, 2.9, Hh), (-4.3, 0.0, 4.5), (4.3, 0.0, 4.5)]
        for idx in ((0, 1, 5, 4), (2, 3, 4, 5), (0, 4, 3), (1, 2, 5), (0, 3, 2, 1)):
            m.face("CT_SteelRed", [roof[i] for i in idx])
        box(m, "CT_Concrete", 1.6, 2.3, -0.4, 0.3, 3.4, 5.1)                                      # the chimney
        # --- the room. Local kit yaw 0 faces -y.
        Z = 0.05
        # the stove under the chimney, against the middle of the back... it stands free, a brick block with an iron door
        box(m, "CT_Brick", 1.45, 2.45, -0.55, 0.45, Z, 1.55)
        box(m, "CT_Brick", 1.7, 2.2, -0.3, 0.2, 1.55, Hh - 0.08)
        box(d, "CT_Steel", 1.42, 1.45, -0.3, 0.2, Z + 0.25, Z + 0.7)
        box(d, "CT_Steel", 1.55, 2.35, -0.45, 0.35, 1.55, 1.58)
        box(d, "CT_Aluminium", 1.75, 2.05, -0.25, 0.05, 1.58, 1.8)                                # the kettle
        # the bunk in the +x end
        kit.bed(B, 3.15, 0.9, Z, yaw=0.0, blanket="CT_PlasterGreen")
        # the table by the middle window, with the log book, a chair, the keeper's stool
        kit.table(B, 0.2, -1.6, Z, yaw=0.0, w=1.3, d=0.75)
        box(d, "CT_Leather", -0.15, 0.25, -1.75, -1.45, Z + 0.75, Z + 0.79)                        # the log book
        box(d, "CT_Paper", -0.13, 0.23, -1.73, -1.47, Z + 0.79, Z + 0.8)
        box(d, "CT_Brass", 0.55, 0.62, -1.5, -1.43, Z + 0.75, Z + 0.87)                           # an ink well... a lamp's foot
        kit.chair(B, 0.2, -0.85, Z, yaw=180.0)
        kit.stool(B, -1.0, -1.5, Z)
        # the radio set on its own bench in the -x end, the chess position pinned over it
        box(m, "CT_Wood", -3.65, -2.9, -1.2, 0.6, Z, Z + 0.78)
        box(d, "CT_Steel", -3.6, -3.2, -0.9, 0.0, Z + 0.78, Z + 1.18)
        box(d, "CT_Lacquer", -3.19, -3.18, -0.8, -0.1, Z + 0.86, Z + 1.1)
        for k in range(4):
            box(d, "CT_Brass", -3.185, -3.16, -0.75 + 0.2 * k, -0.69 + 0.2 * k, Z + 0.9, Z + 0.96)
        box(d, "CT_Lacquer", -3.5, -3.3, 0.15, 0.4, Z + 0.78, Z + 0.84)                            # the key
        box(d, "CT_Paper", -3.69, -3.68, -0.75, -0.2, Z + 1.45, Z + 2.0)                           # the chess diagram
        for i in range(4):
            for j in range(4):
                if (i + j) % 2 == 0:
                    box(d, "CT_Lacquer", -3.675, -3.67, -0.7 + 0.1125 * i, -0.7 + 0.1125 * (i + 1), Z + 1.5 + 0.1125 * j, Z + 1.5 + 0.1125 * (j + 1))
        # shelves of tins on the +y wall beside the door, the barometer by the door, nets drying in the corner
        for zz in (1.0, 1.45, 1.9):
            box(d, "CT_Wood", -3.3, -1.2, D - t - 0.25, D - t, Z + zz, Z + zz + 0.03)
            for k in range(9):
                cx = -3.2 + 0.22 * k + 0.03 * ((k * 7 + int(zz * 10)) % 3)
                box(d, ("CT_Aluminium", "CT_SteelRed", "CT_PlasterBlue", "CT_Copper")[(k + int(zz * 10)) % 4], cx, cx + 0.12, D - t - 0.2, D - t - 0.08,
                    Z + zz + 0.03, Z + zz + 0.17 + 0.03 * (k % 2))
        box(d, "CT_Wood", 0.95, 1.25, D - t - 0.04, D - t, Z + 1.45, Z + 1.75)
        box(d, "CT_Brass", 1.0, 1.2, D - t - 0.07, D - t - 0.04, Z + 1.5, Z + 1.7)
        for k in range(7):
            box(d, "CT_Louvre", 2.2 + 0.18 * k, 2.22 + 0.18 * k, D - t - 0.06, D - t - 0.03, Z + 0.5, Z + 2.2)
        for k in range(8):
            box(d, "CT_Louvre", 2.2, 3.3, D - t - 0.05, D - t - 0.03, Z + 0.55 + 0.22 * k, Z + 0.57 + 0.22 * k)
        box(d, "CT_Wood", 2.15, 3.4, D - t - 0.08, D - t, Z + 2.2, Z + 2.26)
        # the cat's bowl by the stove, carved birds on the window sill, a coat on a peg
        box(d, "CT_Aluminium", 0.95, 1.15, 0.5, 0.7, Z, Z + 0.06)
        for k, wx in enumerate((-2.6, -2.35, -2.15)):
            box(d, "CT_WoodLight", wx - 0.05, wx + 0.07, -D + 0.17, -D + 0.25, Z + 0.95, Z + 1.03 + 0.02 * k)
            box(d, "CT_WoodLight", wx + 0.05, wx + 0.1, -D + 0.19, -D + 0.23, Z + 1.01, Z + 1.07 + 0.02 * k)
        box(d, "CT_Rubber", -3.68, -3.6, 1.2, 1.7, Z + 0.9, Z + 1.8)
        B.lamp(F.to(0.0, 0.0, Z + 2.35), 700, kind="public", radius=6.0)
        # --- things (pos through F.to, yaw through F.yaw)
        B.thing("keeper_table", "counter", F.to(0.2, -1.25, Z + 0.9), F.yaw(180.0), role="keeper", service="farlight")
        B.thing("log_book", "shelf", F.to(0.05, -1.62, Z + 0.82), F.yaw(180.0), topic="farlight_log")
        B.thing("radio_set", "machine", F.to(-3.3, -0.45, Z + 1.0), F.yaw(90.0), what="radio")
        B.thing("chess_card", "board", F.to(-3.66, -0.47, Z + 1.75), F.yaw(90.0), topic="chess")
        B.thing("keeper_stove", "machine", F.to(1.45, -0.05, Z + 0.6), F.yaw(270.0), what="stove")
        B.thing("keeper_bunk", "board", F.to(2.65, 0.9, Z + 0.5), F.yaw(270.0))
        B.thing("tins_shelf", "shelf", F.to(-2.2, D - t - 0.15, Z + 1.45), F.yaw(0.0), topic="tins")
        B.thing("barometer", "machine", F.to(1.1, D - t - 0.06, Z + 1.6), F.yaw(0.0), what="barometer")
        B.thing("drying_nets", "board", F.to(2.75, D - t - 0.06, Z + 1.3), F.yaw(0.0))
        B.thing("cat_bowl", "board", F.to(1.05, 0.6, Z + 0.1), F.yaw(0.0))
        B.thing("carved_birds", "shelf", F.to(-2.35, -D + 0.22, Z + 1.02), F.yaw(180.0), topic="birds")
        B.spot("staff_keeper", "work", F.to(-0.35, -0.95, Z), F.yaw(0.0))
        B.spot("keeper_bed", "bed", F.to(3.15, 0.9, Z), F.yaw(0.0))
        B.spot("farlight_hut", "mark", F.to(0.0, 4.2, 0.0), F.yaw(0.0))
        # outside: the bench by the door, the rain-water tank at the +x end with its down pipe
        kit.bench(B, 2.3, D + 0.45, 0.0, yaw=180.0, w=1.6, kind="park", solid=True)
        B.thing("hut_bench", "seat", F.to(2.3, D + 0.45, 0.45), F.yaw(180.0), height=0.45)
        B.spot("hut_bench_1", "seat", F.to(1.9, D + 0.45, 0.0), F.yaw(180.0))
        B.spot("hut_bench_2", "seat", F.to(2.7, D + 0.45, 0.0), F.yaw(180.0))
        lathe_local = [(W + 1.0 + 0.75 * math.cos(2 * math.pi * k / 12), -1.2 + 0.75 * math.sin(2 * math.pi * k / 12)) for k in range(12)]
        geom.prism(m, "CT_Aluminium", lathe_local, -0.3, 1.7, top=True, bottom=False)
        box(d, "CT_Steel", W + 0.02, W + 0.3, -1.25, -1.15, 1.7, 2.6)
        box(d, "CT_Brass", W + 1.72, W + 1.8, -1.25, -1.15, 0.25, 0.33)
        B.thing("rain_tank", "machine", F.to(W + 1.76, -1.2, 0.6), F.yaw(90.0), what="tap")


# ------------------------------------------------------------------------------------------------ the jetty and the yard
def jetty(B, xs, ys, H):
    jl = math.hypot(J1[0] - J0[0], J1[1] - J0[1])
    jr = math.atan2(J1[1] - J0[1], J1[0] - J0[0])
    with Turned(B, (J0[0], J0[1], 0.0), jr) as F:
        m, d = B.m, B.detail
        box(m, "CT_Concrete", -2.5, jl, -1.6, 1.6, 1.9, JETTY_Z)
        box(m, "CT_Concrete", -3.0, 3.0, -1.6, 1.6, 0.2, 1.9)                 # the root, bedded on the rock
        for k in range(7):
            for side in (-1.2, 1.2):
                box(m, "CT_Wood", k * jl / 6.3 - 0.2, k * jl / 6.3 + 0.2, side - 0.2, side + 0.2, SK.SKIRT, 1.9)
        box(m, "CT_ConcreteDark", jl - 0.25, jl, -1.6, 1.6, JETTY_Z, JETTY_Z + 0.18)      # a kerb at the head
        box(m, "CT_ConcreteDark", 2.0, jl, -1.6, -1.45, JETTY_Z, JETTY_Z + 0.18)          # and along the open side
        for bx_ in (jl * 0.55, jl * 0.9):
            box(m, "CT_Steel", bx_ - 0.14, bx_ + 0.14, -1.25, -0.97, JETTY_Z, JETTY_Z + 0.42)     # bollards
        # the landing steps for the launch, halfway out on the open side
        for k in range(4):
            box(m, "CT_Concrete", jl * 0.7, jl * 0.7 + 2.2, -1.6 - 0.35 * (k + 1), -1.6 - 0.35 * k, 0.4, JETTY_Z - 0.28 * (k + 1))
        gate = F.to(jl * 0.74, -1.25, JETTY_Z + 1.0)
        B.thing("jetty_gate", "door", gate, F.yaw(0.0), to="P-SHIP/launch_ashore")
        box(d, "CT_SteelRed", jl * 0.7 - 0.03, jl * 0.7 + 0.03, -1.5, -1.44, JETTY_Z, JETTY_Z + 1.3)
        box(d, "CT_SteelWhite", jl * 0.7 - 0.3, jl * 0.7 + 0.3, -1.49, -1.46, JETTY_Z + 1.3, JETTY_Z + 1.6)
        B.thing("jetty_bollard", "board", F.to(jl * 0.9, -1.1, JETTY_Z + 0.4), F.yaw(0.0))
        B.spot("farlight_jetty", "mark", F.to(jl * 0.62, 0.3, JETTY_Z), F.yaw(90.0))
        B.spot("jetty_stand_1", "stand", F.to(jl * 0.85, 0.6, JETTY_Z), F.yaw(270.0))
        B.spot("jetty_stand_2", "stand", F.to(jl * 0.4, 0.5, JETTY_Z), F.yaw(0.0))
    # the rail along the jetty's +side and up the path (iron, in the shell)
    c, s = math.cos(jr), math.sin(jr)
    side = [(J0[0] + u * c - 1.52 * s, J0[1] + u * s + 1.52 * c, JETTY_Z) for u in (jl * 0.02, jl * 0.5, jl - 0.3)]
    bar_rail(B, side)
    bar_rail(B, offset_path(PATHS["jetty"], -PATH_HALF - 0.1))
    bar_rail(B, offset_path(PATHS["jetty"], PATH_HALF + 0.1)[1:])
    # concrete treads on the path where it is steepest (looks; the rock under them is the walking surface)
    for pts in (PATHS["jetty"],):
        for a, b in zip(pts, pts[1:]):
            L = math.hypot(b[0] - a[0], b[1] - a[1])
            n = int(L / 1.2)
            for k in range(n):
                f = (k + 0.5) / n
                x, y, z = a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f, a[2] + (b[2] - a[2]) * f
                px, py, pz = P(x, y, z)
                box(B.detail, "CT_Concrete", px - 0.85, px + 0.85, py - 0.22, py + 0.22, pz - 0.2, pz + 0.035)


def yard(B, xs, ys, H):
    m, d = B.m, B.detail
    g = lambda x, y: ground(xs, ys, H, x, y)
    # the fog bell on its frame at the seaward knoll, its clockwork striker in a box
    bx_, by_, bz_ = P(46.0, -8.0, 5.0)
    for sx in (-0.7, 0.7):
        box(m, "CT_Steel", bx_ + sx - 0.05, bx_ + sx + 0.05, by_ - 0.05, by_ + 0.05, bz_ - 0.3, bz_ + 2.4)
    box(m, "CT_Steel", bx_ - 0.75, bx_ + 0.75, by_ - 0.05, by_ + 0.05, bz_ + 2.4, bz_ + 2.5)
    poly = lambda r: [(bx_ + r * math.cos(2 * math.pi * k / 10), by_ + r * math.sin(2 * math.pi * k / 10)) for k in range(10)]
    geom.prism(d, "CT_Brass", poly(0.38), bz_ + 1.45, bz_ + 1.75, top=False, bottom=True)
    geom.prism(d, "CT_Brass", poly(0.3), bz_ + 1.75, bz_ + 2.1, top=True, bottom=False)
    geom.prism(d, "CT_Brass", poly(0.06), bz_ + 2.1, bz_ + 2.4, top=False, bottom=False)
    box(m, "CT_SteelRed", bx_ + 0.9, bx_ + 1.4, by_ - 0.25, by_ + 0.25, bz_ - 0.3, bz_ + 1.1)
    box(d, "CT_Brass", bx_ + 1.1, bx_ + 1.2, by_ + 0.25, by_ + 0.33, bz_ + 0.7, bz_ + 0.8)
    B.thing("fog_bell", "machine", (bx_, by_ + 0.1, bz_ + 1.6), 0.0, what="bell")
    B.thing("bell_clockwork", "machine", (bx_ + 1.15, by_ + 0.3, bz_ + 0.8), 0.0, what="clockwork")
    B.spot("bell_stand", "stand", (bx_ - 0.3, by_ + 1.4, bz_), 180.0)
    # the oil store (as on the far copy), locked
    with Turned(B, (30.0, -6.0, 4.6), math.radians(-20)) as F:
        box(B.m, "CT_Concrete", -2.0, 2.0, -1.5, 1.5, -1.5, 2.2)
        box(B.detail, "CT_Door", -0.45, 0.45, 1.5, 1.54, 0.2, 2.0)
        B.thing("oil_store", "board", F.to(0.0, 1.56, 1.2), F.yaw(180.0))
    # the weather box on four legs beside the path to the hut
    wx, wy = -9.5, 13.6
    px, py, pz = P(wx, wy, g(wx, wy))
    for sx in (-0.3, 0.3):
        for sy in (-0.25, 0.25):
            box(m, "CT_SteelWhite", px + sx - 0.03, px + sx + 0.03, py + sy - 0.03, py + sy + 0.03, pz - 0.3, pz + 1.4)
    box(m, "CT_FrameWhite", px - 0.4, px + 0.4, py - 0.33, py + 0.33, pz + 1.4, pz + 2.05)
    for k in range(5):
        box(d, "CT_Louvre", px - 0.42, px + 0.42, py - 0.35, py + 0.35, pz + 1.47 + 0.11 * k, pz + 1.5 + 0.11 * k)
    box(d, "CT_FrameWhite", px - 0.48, px + 0.48, py - 0.4, py + 0.4, pz + 2.05, pz + 2.1)
    B.thing("weather_box", "board", (px, py - 0.36, pz + 1.7), 0.0, topic="weather")
    # the flagpole (the far copy's mast), with a storm-signal yard
    fx, fy = -34.0, 14.0
    px, py, pz = P(fx, fy, g(fx, fy))
    geom.prism(m, "CT_SteelWhite", [(px + 0.09 * math.cos(2 * math.pi * k / 6), py + 0.09 * math.sin(2 * math.pi * k / 6)) for k in range(6)], pz - 0.5, pz + 9.0,
               top=True, bottom=False)
    box(d, "CT_SteelWhite", px - 0.9, px + 0.9, py - 0.025, py + 0.025, pz + 7.0, pz + 7.05)
    box(d, "CT_SteelRed", px + 0.1, px + 1.0, py - 0.01, py + 0.01, pz + 8.2, pz + 8.8)
    box(m, "CT_Concrete", px - 0.3, px + 0.3, py - 0.3, py + 0.3, pz - 0.5, pz + 0.25)
    B.thing("flagpole", "board", (px, py + 0.3, pz + 1.3), 180.0)
    # the walled vegetable patch: dry-stone walls 0.8 m, a gap toward the path, three beds
    cx, cy = -37.0, 16.5
    px, py, pz = P(cx, cy, 4.6)
    for (x0, x1, y0, y1) in ((-2.6, 2.6, -2.2, -1.9), (-2.6, 2.6, 1.9, 2.2), (-2.6, -2.3, -1.9, 1.9), (2.3, 2.6, -1.9, -0.6), (2.3, 2.6, 0.6, 1.9)):
        box(m, "CT_Stone", px + x0, px + x1, py + y0, py + y1, pz - 0.5, pz + 0.8)
    for k, yy in enumerate((-1.3, 0.0, 1.3)):
        box(d, "CT_Roof", px - 1.9, px + 1.6, py + yy - 0.4, py + yy + 0.4, pz - 0.1, pz + 0.12)
        for j in range(8):
            lx = px - 1.7 + 0.45 * j
            box(d, ("CT_Leaf", "CT_Grass", "CT_Leaf")[k], lx - 0.1, lx + 0.1, py + yy - 0.12, py + yy + 0.12, pz + 0.12, pz + 0.3 + 0.05 * ((j + k) % 3))
    B.thing("veg_patch", "board", (px + 2.45, py, pz + 0.7), 90.0)
    B.spot("patch_stand", "stand", (px + 3.4, py, pz), 270.0)
    # bird ledges: looked at from the end of the bell path
    B.thing("bird_ledge", "board", P(49.5, -7.0, g(49.5, -7.0) + 0.6), 90.0, topic="birds")
    # people's standing places along the paths (the passengers ashore)
    for k, (x, y, yw) in enumerate(((10.0, 16.0, 200.0), (3.5, 8.5, 20.0), (-12.0, 12.6, 180.0), (-17.5, 13.5, 160.0), (22.0, -2.2, 0.0), (36.0, -7.6, 180.0))):
        B.spot("ashore_%d" % (k + 1), "stand", P(x, y, g(x, y)), yw)


def build(B):
    xs, ys, H = heights()
    rock(B, xs, ys, H)
    tower(B)
    hut(B)
    jetty(B, xs, ys, H)
    yard(B, xs, ys, H)
    B.footprint = [(X0, 0.0), (X1, 0.0), (X1, 2 * HALF_D), (X0, 2 * HALF_D)]
    B.height = T[2] + 29.5 + ZOFF
    B.note("the island's chart point is the plot's map point; kit frame -> building frame: (kx, ky + 72, kz - 1.5)")
    B.note("must match blender/ship/seascape_kit.py lighthouse() (the far-view copy swapped for this one at anchor)")
    if os.environ.get("KIT_CHECK"):
        print(kit.check(B))
