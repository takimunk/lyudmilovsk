"""The sailing school's boathouse (эллинг) «ДЮСШ парусного спорта «Луч»», its interior props and the outdoor kit
(docs/yacht_school.md "Meshes (part C)"; footprint and doors after part B's site: 16.5 x 9 m, the gable and the ridge
to the sea, both doors in the north long wall, the classroom in the landward 5.3 m).

BUILDING FRAME: metres; origin on the finished floor (z = 0, the level of the sand at the boat door) in the middle of the
footprint; +X toward the SEA (the seaward gable at x = +8.25), -Y = the NORTH long wall (the park side, both doors),
+Z up. Unreal (export_yup): X = x, Y = -y, Z = z. So the seaward gable faces Unreal +X and the door wall Unreal +Y.
The sand falls 7 % toward the sea: ground(x) = -0.0727 (x + 0.85) (0 at the boat door, +0.54 at the landward gable,
-0.66 at the seaward one); the concrete plinth runs down to -0.85 everywhere and the sand covers what it must.

  SM_Sail_Boathouse          the shell: plinth, walls with real thickness and openings, concrete frame (pilasters, ring
                             beam), gables, roof, partitions, floors, the classroom and loft ceilings, trusses, the door
                             rail, the people's door areaway, aprons. collision exact (complex as simple)
  SM_Sail_Boathouse_Glass    the clerestory and window panes (clear glass, its own mesh like the diner's)
  SM_Sail_Boathouse_DoorLeaf one timber leaf of the sliding boat door (1.65 x 2.95 m), origin at its bottom middle, slides
                             along +-X (layout "sliding_doors")
  SM_Sail_Prop_<Name>        interior and outdoor props, each at its own origin (on the floor / ground, front = -Y)
  boathouse_layout.json      where everything goes (building frame): props, boats on racks, doors (place_doors.py
                             entries), sliding doors, lamps, rooms, people's spots, interactive things
"""
import json
import math
import os

from mathutils import Matrix, Vector

import boatspec as B
import boats as BT
from sailkit import Mesh, HERE, rz, frame_from

KIT = "sailing_house"
L, W = 16.5, 9.0
X0, X1, Y0, Y1 = -L / 2, L / 2, -W / 2, W / 2
WALL_H, RIDGE = 3.4, 5.6
T = 0.30           # outer walls
TP = 0.15          # partitions
PLINTH = -0.85
CLASS_X = X0 + 5.3           # classroom | hall partition (centre line)
WET_X = X1 - 3.0             # hall | changing room + wet store
BOAT_DOOR = (X0 + 7.4, 3.2, 2.95)        # centre x, width, height
PEOPLE_DOOR = (X0 + 2.5, 1.20, 2.18)     # centre x, hole w, hole h (clear 1.10 x 2.15 with the door's frame)
BAY = L / 6

CELLS = None


def ground(x):
    return -0.0727 * (x - BOAT_DOOR[0])


def cell(name, sheet="atlas"):
    global CELLS
    if CELLS is None:
        with open(os.path.join(HERE, "atlas_cells.json")) as f:
            CELLS = json.load(f)
    u, y, w, h = CELLS[sheet][name]
    e = 0.002
    return [(u + e, 1 - y - h + e), (u + w - e, 1 - y - h + e), (u + w - e, 1 - y - e), (u + e, 1 - y - e)]


def pic(m, c, n, w, h, name, slot="SLP_Atlas", sheet="atlas", lift=0.006):
    """A picture quad centred at c facing horizontal direction n, w x h, mapped to an atlas cell."""
    n = Vector(n).normalized()
    r = Vector((0, 0, 1)).cross(n)
    c = Vector(c) + n * lift
    up = Vector((0, 0, 1))
    pts = [c - r * w / 2 - up * h / 2, c + r * w / 2 - up * h / 2, c + r * w / 2 + up * h / 2, c - r * w / 2 + up * h / 2]
    return m.face(pts, slot, uvs=cell(name, sheet))


def picture_box(m, c, n, w, h, d, name, frame_slot="SL_WoodRaw", slot="SLP_Atlas", sheet="atlas"):
    """A board / frame (box d deep) with the picture on its front."""
    n = Vector(n).normalized()
    r = Vector((0, 0, 1)).cross(n)
    M = Matrix((r, n, Vector((0, 0, 1)))).transposed()
    m.box(Vector(c) - n * d / 2, (w, d, h), frame_slot, rot=M)
    pic(m, Vector(c) + n * 0.0005, n, w * 0.94, h * 0.92, name, slot, sheet, lift=0.001)


# ================================================================================================= the shell
def wall_x(m, y0, y1, x0, x1, z0, z1, openings, outer="y", s_out="CT_Plaster", s_in="CT_Interior", s_rev="CT_Concrete"):
    """A wall along X (thickness y0..y1) with openings [(xa, xb, za, zb)]; outer = side letter of the outside face."""
    ops = sorted(openings)
    xs = [x0]
    for xa, xb, za, zb in ops:
        xs += [xa, xb]
    xs.append(x1)
    sl = {outer: s_out, ("Y" if outer == "y" else "y"): s_in}
    for k in range(0, len(xs), 2):
        a, b = xs[k], xs[k + 1]
        m.bx(s_rev, a, b, y0, y1, z0, z1, slots=sl)
    for xa, xb, za, zb in ops:
        m.bx(s_rev, xa, xb, y0, y1, z0, za, slots=sl)
        m.bx(s_rev, xa, xb, y0, y1, zb, z1, slots=sl)


def wall_y(m, x0, x1, y0, y1, z0, z1, openings, outer="X", s_out="CT_Plaster", s_in="CT_Interior", s_rev="CT_Concrete"):
    ops = sorted(openings)
    ys = [y0]
    for ya, yb, za, zb in ops:
        ys += [ya, yb]
    ys.append(y1)
    sl = {outer: s_out, ("x" if outer == "X" else "X"): s_in}
    for k in range(0, len(ys), 2):
        m.bx(s_rev, x0, x1, ys[k], ys[k + 1], z0, z1, slots=sl)
    for ya, yb, za, zb in ops:
        m.bx(s_rev, x0, x1, ya, yb, z0, za, slots=sl)
        m.bx(s_rev, x0, x1, ya, yb, zb, z1, slots=sl)


def gable(m, x0, x1, half_w, z0, z1, slot_out, slot_in, out_sign):
    """A triangular gable from x0 to x1 (thickness) over y in +-half_w, z0 (eaves) to z1 (ridge)."""
    tri = [(-half_w, z0), (half_w, z0), (0.0, z1)]
    a = [m.bm.verts.new((x0, y, z)) for y, z in tri]
    b = [m.bm.verts.new((x1, y, z)) for y, z in tri]
    m.face([a[2], a[1], a[0]], slot_out if out_sign < 0 else slot_in)
    m.face([b[0], b[1], b[2]], slot_out if out_sign > 0 else slot_in)
    for i in range(3):
        j = (i + 1) % 3
        m.face([a[i], a[j], b[j], b[i]], "CT_Concrete")


ROOF_LO = 4.0                # roof soffit at the landward wall; it rises to RIDGE (5.6) at the seaward wall
ROOF_T = 0.24                # roof slab
OVER = 2.6                   # the roof oversails the seaward wall (over the starter's balcony)
BALC = (2.80, 1.60)          # the starter's balcony: floor (= the loft's floor), depth out from the seaward wall
STAIR_Y = (Y1 + 0.50, Y1 + 1.50)


def zu(x):
    """Roof soffit height at x (the mono-pitch rises toward the sea)."""
    return ROOF_LO + (RIDGE - ROOF_LO) * (x - X0) / L


def slab_xz(m, poly, y0, y1, s_front, s_back, s_edges="CT_Concrete"):
    """A prism of a polygon [(x, z)] (counter-clockwise with x right, z up) between y0 (front, faces -Y) and y1;
    s_edges: one slot or one per polygon edge."""
    a = [m.bm.verts.new((x, y0, z)) for x, z in poly]
    b = [m.bm.verts.new((x, y1, z)) for x, z in poly]
    m.face(a, s_front)
    m.face(b[::-1], s_back)
    n = len(poly)
    for i in range(n):
        j = (i + 1) % n
        s = s_edges[i] if isinstance(s_edges, (list, tuple)) else s_edges
        m.face([a[i], b[i], b[j], a[j]], s)


def slab_yz(m, poly, x0, x1, s_front, s_back, s_edges="CT_Concrete"):
    """A prism of a polygon [(y, z)] (counter-clockwise with y right, z up as seen from +X) between x1 (front, faces +X)
    and x0."""
    a = [m.bm.verts.new((x1, y, z)) for y, z in poly]
    b = [m.bm.verts.new((x0, y, z)) for y, z in poly]
    m.face(a, s_front)
    m.face(b[::-1], s_back)
    n = len(poly)
    for i in range(n):
        j = (i + 1) % n
        s = s_edges[i] if isinstance(s_edges, (list, tuple)) else s_edges
        m.face([a[i], b[i], b[j], a[j]], s)


def rail_run(m, pts, h=1.05, slot="CT_SteelWhite", post_step=1.1, rails=(0.38, 0.72)):
    """A steel railing along a polyline on its floor: posts, a top rail and intermediate rails."""
    pts = [Vector(p) for p in pts]
    for a, b in zip(pts, pts[1:]):
        n = max(1, int(round((b - a).length / post_step)))
        for k in range(n + 1):
            p = a + (b - a) * k / n
            m.tube(p, p + Vector((0, 0, h)), 0.022, slot, seg=8)
    m.pipe([p + Vector((0, 0, h)) for p in pts], 0.026, slot, seg=8)
    for z in rails:
        m.pipe([p + Vector((0, 0, z)) for p in pts], 0.012, slot, seg=6)


def _grime(m, pts_base, out, seed):
    """Splash staining: a thin jagged band on a wall's outside face from the sand up ~0.3-0.5 m; pts_base = [(x, y)]
    along the wall, out = the outward normal (3 mm proud)."""
    import random as _r
    r = _r.Random(seed)
    o = Vector(out) * 0.004
    for (xa, ya), (xb, yb) in zip(pts_base, pts_base[1:]):
        za, zb = ground(xa), ground(xb)
        ha, hb = r.uniform(0.22, 0.48), r.uniform(0.22, 0.48)
        quad = [Vector((xa, ya, za - 0.05)) + o, Vector((xb, yb, zb - 0.05)) + o, Vector((xb, yb, zb + hb)) + o, Vector((xa, ya, za + ha)) + o]
        f = m.face(quad, "SL_Grime")
        if f is not None and f.normal.dot(Vector(out)) < 0:
            f.normal_flip()


def _drift(m, line, out, width, seed):
    """Sand drifted against a wall: a strip from the wall foot (a heap 0.12-0.3 m high) out to the beach."""
    import random as _r
    r = _r.Random(seed)
    o = Vector(out)
    prev = None
    for (x, y) in line:
        h = r.uniform(0.10, 0.30)
        inner = Vector((x, y, ground(x) + h))
        mid = Vector((x, y, 0)) + o * width * 0.45
        mid.z = ground(mid.x) + h * 0.45
        outer = Vector((x, y, 0)) + o * width
        outer.z = ground(outer.x) - 0.04
        cur = (inner, mid, outer)
        if prev:
            for k in range(2):
                f = m.face([prev[k], cur[k], cur[k + 1], prev[k + 1]], "SL_Sand", smooth=True)
                if f is not None and f.normal.z < 0:
                    f.normal_flip()
        prev = cur


def _mosaic_panel(m, cx_y, z0, size, n=28):
    """The «Луч» mosaic on the seaward wall as real tesserae: n x n tiles (each its own little quad with its own patch of
    the mosaic picture, a few mm proud of the grout bed, slightly uneven), in a raised concrete surround."""
    import random as _r
    r = _r.Random(12)
    c = cell("mosaic")
    u0, v0 = c[0]
    u1, v1 = c[2]
    t = size / n
    gap = t * 0.10
    xw = X1 + 0.012
    m.face([(xw - 0.004, cx_y - size / 2 - 0.02, z0 - 0.02), (xw - 0.004, cx_y + size / 2 + 0.02, z0 - 0.02),
            (xw - 0.004, cx_y + size / 2 + 0.02, z0 + size + 0.02), (xw - 0.004, cx_y - size / 2 - 0.02, z0 + size + 0.02)], "CT_Concrete")
    for i in range(n):
        for j in range(n):
            ya = cx_y - size / 2 + i * t + gap / 2
            za = z0 + j * t + gap / 2
            d = r.uniform(0.0, 0.004)
            x = xw + d
            ua, ub = u0 + (u1 - u0) * (i + 0.18) / n, u0 + (u1 - u0) * (i + 0.82) / n
            va, vb = v0 + (v1 - v0) * (j + 0.18) / n, v0 + (v1 - v0) * (j + 0.82) / n
            tilt = r.uniform(-0.0015, 0.0015)
            m.face([(x + tilt, ya, za), (x - tilt, ya + t - gap, za), (x + tilt, ya + t - gap, za + t - gap), (x - tilt, ya, za + t - gap)],
                   "SLP_Atlas", uvs=[(ua, va), (ub, va), (ub, vb), (ua, vb)])
    # the concrete surround (a raised frame) and a little cill
    fw, fd = 0.16, 0.07
    m.bx("CT_Concrete", X1, X1 + fd, cx_y - size / 2 - fw, cx_y + size / 2 + fw, z0 - fw, z0)
    m.bx("CT_Concrete", X1, X1 + fd, cx_y - size / 2 - fw, cx_y + size / 2 + fw, z0 + size, z0 + size + fw)
    m.bx("CT_Concrete", X1, X1 + fd, cx_y - size / 2 - fw, cx_y - size / 2, z0, z0 + size)
    m.bx("CT_Concrete", X1, X1 + fd, cx_y + size / 2, cx_y + size / 2 + fw, z0, z0 + size)
    m.bx("CT_Concrete", X1, X1 + fd + 0.05, cx_y - size / 2 - fw - 0.04, cx_y + size / 2 + fw + 0.04, z0 - fw - 0.05, z0 - fw)


def shell():
    """Soviet seaside modernism of the 60s-70s: an exposed concrete portal frame (pilasters on the door side, deep
    columns on the south, upstand beams over the roof), a mono-pitch roof rising toward the sea and oversailing it on a
    free-standing portal, a continuous clerestory band under the eaves of both long walls, a gridded glazed upper wall
    to the sea over the mosaic, the starter's balcony (the loft's floor carried out) with an external steel stair on the
    south side. Footprint, wall and door positions, floor and the room split as part B's site."""
    m = Mesh("SM_Sail_Boathouse")
    g = Mesh("SM_Sail_Boathouse_Glass")
    g.closed = False
    m.closed = False
    bx_, bw, bh = BOAT_DOOR
    px, pw, ph = PEOPLE_DOOR
    # ---- plinth ring (dark concrete), floor slab
    m.bx("CT_ConcreteDark", X0 - 0.03, X1 + 0.03, Y0 - 0.03, Y1 + 0.03, PLINTH, -0.20)
    m.bx("CT_ConcreteDark", X0 - 0.03, X1 + 0.03, Y0 - 0.03, Y0 + T, -0.20, 0.0)
    m.bx("CT_ConcreteDark", X0 - 0.03, X1 + 0.03, Y1 - T, Y1 + 0.03, -0.20, 0.0)
    m.bx("CT_ConcreteDark", X0 - 0.03, X0 + T, Y0 + T, Y1 - T, -0.20, 0.0)
    m.bx("CT_ConcreteDark", X1 - T, X1 + 0.03, Y0 + T, Y1 - T, -0.20, 0.0)
    m.bx("CT_Floor", X0 + T, X1 - T, Y0 + T, Y1 - T, -0.20, 0.0)
    # ---- long walls: solid to 3.05 (the doors in the north one, the classroom's windows in the south one), the
    # continuous clerestory band 3.05 .. 3.75, the upper wall up to the sloping soffit
    band = (3.05, 3.75)
    north_ops = [(bx_ - bw / 2, bx_ + bw / 2, 0.0, bh), (px - pw / 2, px + pw / 2, 0.0, ph)]
    south_ops = [(a, a + 1.5, 0.95, 2.05) for a in (X0 + 0.9, X0 + 3.1)]
    wall_x(m, Y0, Y0 + T, X0, X1, 0.0, band[0], north_ops, outer="y")
    wall_x(m, Y1 - T, Y1, X0, X1, 0.0, band[0], south_ops, outer="Y")
    up = [(X0, band[1]), (X1, band[1]), (X1, zu(X1)), (X0, zu(X0))]
    slab_xz(m, up, Y0, Y0 + T, "CT_Plaster", "CT_Interior")
    slab_xz(m, up, Y1 - T, Y1, "CT_Interior", "CT_Plaster")
    for yy, sg in ((Y0 + T * 0.45, -1), (Y1 - T * 0.45, 1)):
        g.bx("SL_Glass", X0 + T, X1 - T, yy - 0.004, yy + 0.004, band[0], band[1])
        m.bx("CT_Frame", X0 + T, X1 - T, yy - 0.05, yy + 0.05, band[0], band[0] + 0.06)
        m.bx("CT_Frame", X0 + T, X1 - T, yy - 0.05, yy + 0.05, band[1] - 0.06, band[1])
        nm = int((L - 2 * T) / 1.1)
        for k in range(nm + 1):
            x = X0 + T + (L - 2 * T) * k / nm
            m.bx("CT_Frame", x - 0.03, x + 0.03, yy - 0.05, yy + 0.05, band[0], band[1])
    for xa, xb, za, zb in south_ops:
        yy = Y1 - T * 0.4
        g.bx("SL_Glass", xa, xb, yy - 0.004, yy + 0.004, za, zb)
        for x in (xa + (xb - xa) / 3, xa + 2 * (xb - xa) / 3):
            m.bx("CT_Frame", x - 0.025, x + 0.025, yy - 0.04, yy + 0.04, za, zb)
        m.bx("CT_Frame", xa, xb, yy - 0.04, yy + 0.04, za, za + 0.05)
        m.bx("CT_Frame", xa, xb, yy - 0.04, yy + 0.04, zb - 0.05, zb)
        m.bx("CT_Concrete", xa - 0.05, xb + 0.05, Y1, Y1 + 0.08, za - 0.08, za)      # cill
    # ---- landward wall: its two small windows, up to the low soffit
    wall_y(m, X0, X0 + T, Y0 + T, Y1 - T, 0.0, WALL_H, [(-1.6, -0.6, 2.5, 3.1), (0.6, 1.6, 2.5, 3.1)], outer="x")
    m.bx("CT_Concrete", X0, X0 + T, Y0 + T, Y1 - T, WALL_H, zu(X0 + T), slots={"x": "CT_Plaster", "X": "CT_Interior"})
    for ya, yb, za, zb in [(-1.6, -0.6, 2.5, 3.1), (0.6, 1.6, 2.5, 3.1)]:
        g.bx("SL_Glass", X0 + 0.12 - 0.004, X0 + 0.12 + 0.004, ya, yb, za, zb)
    # ---- seaward wall: solid under the balcony (the mosaic), a gridded glazed wall above it with the balcony door
    zb0, bd = BALC
    wall_y(m, X1 - T, X1, Y0 + T, Y1 - T, 0.0, zb0 - 0.18, [], outer="X")
    _mosaic_panel(m, 0.0, 0.25, 2.25)
    xg = X1 - T * 0.5
    door_y = (-0.55, 0.55)
    ny = 9
    ys = [Y0 + T + (W - 2 * T) * k / ny for k in range(ny + 1)]
    zs = [zb0, zb0 + 1.15, zb0 + 2.1, zu(X1)]
    for y in ys:
        m.bx("SL_PaintSteel", xg - 0.06, xg + 0.06, y - 0.035, y + 0.035, zb0, zu(X1))
    for y in door_y:
        m.bx("SL_PaintSteel", xg - 0.07, xg + 0.07, y - 0.045, y + 0.045, zb0, zb0 + 2.1)
    for z in zs:
        m.bx("SL_PaintSteel", xg - 0.06, xg + 0.06, Y0 + T, Y1 - T, z - 0.035, z + 0.035)
    for a, b in zip(ys, ys[1:]):
        for za, zz in zip(zs, zs[1:]):
            if za < zb0 + 2.0 and a < door_y[1] and b > door_y[0]:
                # the balcony door's opening (the door leaf: layout "doors" -> balcony); glass round it
                for y0_, y1_ in ((a, door_y[0]), (door_y[1], b)):
                    if y1_ - y0_ > 0.05:
                        g.bx("SL_Glass", xg - 0.004, xg + 0.004, y0_, y1_, za, zz)
                continue
            g.bx("SL_Glass", xg - 0.004, xg + 0.004, a, b, za, zz)
    m.bx("CT_Concrete", X1 - T, X1, Y0 + T, Y1 - T, zu(X1) - 0.02, zu(X1) + 0.02)
    # ---- the starter's balcony: the loft floor carried out to the sea, wrapping round to the stair's landing
    m.bx("CT_Concrete", X1 - T, X1 + bd, Y0 + 0.45 - 0.6, STAIR_Y[1] + 0.05, zb0 - 0.18, zb0, slots={"Z": "CT_Paving"})
    for y in (-3.0, -1.0, 1.0, 3.0, Y1 + 1.0):
        m.bx("CT_Concrete", X1, X1 + bd - 0.1, y - 0.1, y + 0.1, zb0 - 0.40, zb0 - 0.18)
    yb0, yb1 = Y0 + 0.45 - 0.6 + 0.05, STAIR_Y[1]
    rail_run(m, [(X1, yb0, zb0), (X1 + bd - 0.05, yb0, zb0), (X1 + bd - 0.05, yb1, zb0), (X1 - T, yb1, zb0)])
    # ---- the external stair up the south side to the balcony (steel stringers, precast treads, two handrails)
    nst = 17
    x_top = X1 - T
    x_bot = x_top - (nst - 1) * 0.28
    z_bot = ground(x_bot)
    rise = (zb0 - z_bot) / nst
    for k in range(nst - 1):
        xa = x_bot + k * 0.28
        top = z_bot + rise * (k + 1)
        m.bx("CT_Concrete", xa - 0.02, xa + 0.28, STAIR_Y[0], STAIR_Y[1], top - 0.05, top)
    for y in (STAIR_Y[0] - 0.02, STAIR_Y[1] + 0.02):
        m.beam((x_bot - 0.3, y, z_bot - 0.05), (x_top, y, zb0 - 0.08), 0.03, 0.24, "SL_PaintSteel", up=(0, 0, 1))
        rail_run(m, [(x_bot, y, z_bot + rise), (x_top, y, zb0)], h=0.95, slot="SL_PaintSteel", post_step=0.9, rails=(0.45,))
    m.bx("CT_Concrete", x_bot - 0.45, x_bot + 0.1, STAIR_Y[0] - 0.1, STAIR_Y[1] + 0.1, z_bot - 0.3, z_bot + 0.02)
    # ---- the exposed concrete frame: pilasters (north: shallow, clear of the sliding leaves), columns (south), upstand
    # beams over the roof, downstand beams inside; the free-standing portal under the oversail
    for k in range(7):
        x = min(max(X0 + k * BAY, X0 + 0.15), X1 - 0.15)
        top = zu(x) + ROOF_T
        m.bx("CT_Concrete", x - 0.15, x + 0.15, Y0 - 0.10, Y0, -0.2, top)
        m.bx("CT_Concrete", x - 0.15, x + 0.15, Y1, Y1 + 0.38, -0.2, top)
        m.bx("CT_Concrete", x - 0.12, x + 0.12, Y0 - 0.10, Y1 + 0.38, top, top + 0.45)
        m.bx("CT_Concrete", x - 0.12, x + 0.12, Y0 + T, Y1 - T, zu(x) - 0.42, zu(x))
    xf = X1 + OVER - 0.30
    for y0_, y1_ in ((Y0 - 0.40, Y0 - 0.10), (Y1 + 0.08, Y1 + 0.38)):
        m.bx("CT_Concrete", xf - 0.15, xf + 0.15, y0_, y1_, ground(xf) - 0.3, zu(xf) - 0.42)
        m.bx("CT_ConcreteDark", xf - 0.35, xf + 0.35, y0_ - 0.2, y1_ + 0.2, ground(xf) - 0.5, ground(xf) + 0.05)
    # (no solids overlap here: interpenetrating boxes shade black under the sharp-edge split normals)
    m.bx("CT_Concrete", xf - 0.12, xf + 0.12, Y0 - 0.35, Y1 + 0.45, zu(xf) + ROOF_T, zu(xf) + ROOF_T + 0.45)
    m.bx("CT_Concrete", xf - 0.15, xf + 0.15, Y0 - 0.40, Y1 + 0.38, zu(xf) - 0.42, zu(xf))
    # ---- the roof: one slab from the landward eaves to the oversail, a low parapet, the deep seaward fascia (the sign)
    xa_, xe = X0 - 0.5, X1 + OVER
    ya_, ye = Y0 - 0.50, Y1 + 0.60
    slab_xz(m, [(xa_, zu(xa_)), (xe, zu(xe)), (xe, zu(xe) + ROOF_T), (xa_, zu(xa_) + ROOF_T)], ya_, ye, "CT_Concrete", "CT_Concrete",
            ["CT_Concrete", "CT_Concrete", "CT_Roof", "CT_Concrete"])
    for y0_, y1_ in ((ya_, ya_ + 0.15), (ye - 0.15, ye)):
        slab_xz(m, [(xa_, zu(xa_) + ROOF_T), (xe, zu(xe) + ROOF_T), (xe, zu(xe) + ROOF_T + 0.30), (xa_, zu(xa_) + ROOF_T + 0.30)],
                y0_, y1_, "CT_Concrete", "CT_Concrete")
    # between the side parapets (through them its faces would lie in theirs)
    m.bx("CT_Concrete", xa_, xa_ + 0.15, ya_ + 0.15, ye - 0.15, zu(xa_) + ROOF_T, zu(xa_) + ROOF_T + 0.20)
    fz0, fz1 = zu(xe) - 0.50, zu(xe) + ROOF_T + 0.42
    m.bx("CT_Concrete", xe, xe + 0.14, ya_ - 0.02, ye + 0.02, fz0, fz1)
    m.bx("CT_SteelWhite", xe + 0.14, xe + 0.18, -3.05, 3.05, fz0 + 0.12, fz0 + 0.78)
    m.picture([(xe + 0.185, -3.0, fz0 + 0.16), (xe + 0.185, 3.0, fz0 + 0.16), (xe + 0.185, 3.0, fz0 + 0.74), (xe + 0.185, -3.0, fz0 + 0.74)],
              "SLP_Sign_Name")
    # gutter on the low (landward) eaves and two downpipes to the sand with their shoes
    gz_ = zu(xa_) + 0.02
    m.bx("CT_Steel", xa_ - 0.16, xa_, ya_, ye, gz_ - 0.05, gz_ + 0.10)
    for y in (ya_ + 0.25, ye - 0.25):
        gx_ = xa_ - 0.08
        m.tube((gx_, y, gz_), (gx_, y, ground(gx_) + 0.25), 0.05, "CT_Steel", seg=8)
        m.pipe([(gx_, y, ground(gx_) + 0.27), (gx_, y, ground(gx_) + 0.10), (gx_ - 0.25, y, ground(gx_) + 0.06)], 0.05, "CT_Steel", seg=8)
        m.bx("CT_Concrete", gx_ - 0.65, gx_ - 0.15, y - 0.15, y + 0.15, ground(gx_ - 0.4) - 0.05, ground(gx_ - 0.4) + 0.03)
    # ---- partitions: classroom | hall (to the soffit), hall | changing room + wet store (to the loft)
    cx0, cx1 = CLASS_X - TP / 2, CLASS_X + TP / 2
    wall_y(m, cx0, cx1, Y0 + T, Y1 - T, 0.0, WALL_H, [(2.5 - 0.6, 2.5 + 0.6, 0.0, 2.18)], outer="X", s_out="CT_Interior")
    m.bx("CT_Interior", cx0, cx1, Y0 + T, Y1 - T, WALL_H, zu(cx1))
    wx0, wx1 = WET_X - TP / 2, WET_X + TP / 2
    loft = 2.70
    wall_y(m, wx0, wx1, Y0 + T, Y1 - T, 0.0, loft, [(-2.25 - 0.6, -2.25 + 0.6, 0.0, 2.18), (2.25 - 0.6, 2.25 + 0.6, 0.0, 2.18)],
           outer="x", s_out="CT_Interior")
    wall_x(m, -TP / 2, TP / 2, wx1, X1 - T, 0.0, loft, [], outer="y", s_out="CT_Interior")
    # ceilings: the classroom's (flat, 3.2), the loft over the small rooms (its floor timber) with a rail
    m.bx("CT_Interior", X0 + T, cx0, Y0 + T, Y1 - T, 3.20, 3.28, slots={"Z": "CT_Wood"})
    m.bx("CT_Wood", wx0, X1 - T, Y0 + T, Y1 - T, loft, loft + 0.10, slots={"z": "CT_Interior"})
    m.bx("CT_Steel", wx0 - 0.02, wx0 + 0.02, Y0 + T, Y1 - T, loft + 1.0, loft + 1.04)
    for y in (-3.5, -1.5, 0.5, 2.5, 4.0):
        m.bx("CT_Steel", wx0 - 0.02, wx0 + 0.02, y - 0.02, y + 0.02, loft + 0.10, loft + 1.0)
    # classroom floor: timber boards on the slab
    m.bx("CT_WoodLight", X0 + T, cx0, Y0 + T, Y1 - T, 0.0, 0.02)
    # ---- the hall: a lighting rail along the middle and two timber drying rails across (the sails hang from them),
    # hung from the downstand beams; paint and oil stains on the floor
    m.tube((cx1 + 0.2, 0.0, WALL_H - 0.25), (wx0 - 0.2, 0.0, WALL_H - 0.25), 0.025, "CT_Steel", seg=8)
    for k in range(2, 6):
        x = X0 + k * BAY
        if cx1 < x < wx0:
            m.tube((x, 0.0, WALL_H - 0.25), (x, 0.0, zu(x) - 0.42), 0.008, "CT_Steel", seg=5)
    for k in (3, 4):
        x = X0 + k * BAY + 0.10
        m.tube((x, Y0 + T + 0.3, WALL_H - 0.30), (x, Y1 - T - 0.3, WALL_H - 0.30), 0.04, "CT_Wood", seg=8)
        for y in (-3.0, 0.0, 3.0):
            m.tube((x, y, WALL_H - 0.30), (x - 0.10, y, zu(x) - 0.42), 0.007, "CT_Steel", seg=5)
    import random as _r
    rr = _r.Random(21)
    for (sx, sy, sr) in ((1.0, -0.6, 0.55), (3.3, -0.2, 0.7), (4.6, 1.8, 0.35), (-0.8, 2.6, 0.5), (2.2, 2.9, 0.3), (0.2, -2.6, 0.4)):
        pts = []
        for k in range(14):
            a = 2 * math.pi * k / 14
            rad = sr * rr.uniform(0.55, 1.0)
            pts.append((sx + math.cos(a) * rad, sy + math.sin(a) * rad * 0.75, 0.0015))
        m.face(pts, "SL_Stain")
        for k in range(4):
            dx, dy = rr.uniform(-1.2, 1.2) * sr, rr.uniform(-1.2, 1.2) * sr
            m.face([(sx + dx + 0.05 * math.cos(2 * math.pi * j / 6), sy + dy + 0.04 * math.sin(2 * math.pi * j / 6), 0.0016) for j in range(6)], "SL_Stain")
    # ---- the boat door: its overhead rail (a steel channel on brackets with rollers), the guide at the foot, lamps
    ry_ = Y0 - 0.20
    m.bx("SL_PaintSteel", bx_ - bw - 0.25, bx_ + bw + 0.25, ry_ - 0.05, ry_ + 0.05, bh + 0.10, bh + 0.24)
    for x in (bx_ - bw - 0.15, bx_ - bw / 2 - 0.2, bx_ + bw / 2 + 0.2, bx_ + bw + 0.15):
        m.bx("SL_PaintSteel", x - 0.03, x + 0.03, Y0 - 0.20, Y0, bh + 0.12, bh + 0.22)
    m.bx("CT_Steel", bx_ - bw - 0.2, bx_ + bw + 0.2, Y0 - 0.24, Y0 - 0.16, -0.02, 0.0)
    for lx, ly, lz in ((bx_, Y0 - 0.14, bh + 0.55), (px, Y0 - 0.45, ph + 0.21), (X1 + 0.06, 0.95, zb0 + 2.35)):
        m.bx("SL_Dark", lx - 0.10, lx + 0.10, ly - 0.08, ly + 0.08, lz + 0.02, lz + 0.10)
        m.sphere((lx, ly, lz), 0.07, "SL_Lamp", seg=10, rings=6)
    # ---- aprons: the boat door's (flush with the floor) and the people's door areaway (sunk into the sand, two steps)
    m.bx("CT_Paving", bx_ - bw / 2 - 0.6, bx_ + bw / 2 + 0.6, Y0 - 2.0, Y0, -0.20, 0.0)
    ax0, ax1 = px - 0.9, px + 0.9
    m.bx("CT_Paving", ax0, ax1, Y0 - 1.5, Y0, -0.20, 0.0)
    gz = ground(px)
    for k, (ya, z) in enumerate(((Y0 - 1.5, gz / 2), (Y0 - 1.8, gz))):
        m.bx("CT_Concrete", ax0, ax1, ya - 0.3, ya, -0.20, z)
    for xx in (ax0 - 0.15, ax1):
        m.bx("CT_Concrete", xx, xx + 0.15, Y0 - 2.1, Y0, -0.20, gz + 0.1)
    # a thin cantilevered canopy over the people's door, the door sign, the painted timetable board
    m.bx("CT_Concrete", px - 1.0, px + 1.0, Y0 - 1.0, Y0, ph + 0.27, ph + 0.37)
    m.bx("SL_PaintSteel", px - 1.0, px + 1.0, Y0 - 1.02, Y0 - 0.98, ph + 0.22, ph + 0.40)
    m.picture([(px + 0.95, Y0 - 0.005, 1.35), (px + 1.45, Y0 - 0.005, 1.35), (px + 1.45, Y0 - 0.005, 1.75), (px + 0.95, Y0 - 0.005, 1.75)],
              "SLP_Sign_Door")
    picture_box(m, (X0 + 1.2, Y0 - 0.0, 1.55), (0, -1, 0), 1.10, 1.10, 0.04, "timetable", frame_slot="SL_PaintSteel")
    # ---- weather: splash staining along the wall feet, sand drifted against the south and landward walls
    _grime(m, [(X0 + k * 0.55, Y1) for k in range(31)], (0, 1, 0), 1)
    _grime(m, [(X0, Y1 - k * 0.5) for k in range(19)], (-1, 0, 0), 2)
    _grime(m, [(X1, Y0 + k * 0.5) for k in range(19)], (1, 0, 0), 3)
    _grime(m, [(X0 + k * 0.5, Y0) for k in range(5)], (0, -1, 0), 4)
    _drift(m, [(X0 + 0.2 + k * 0.6, Y1 + 0.4) for k in range(12)], (0, 1, 0), 1.6, 5)
    _drift(m, [(X0 - 0.02, Y1 - 0.2 - k * 0.6) for k in range(15)][::-1], (-1, 0, 0), 1.4, 6)
    return m, g


def door_leaf():
    """One leaf of the sliding boat door: exposed painted steel, a framed leaf with pressed horizontal ribs, hung from the
    overhead rail on two rollers (same size and origin as before: bottom middle, slides along x)."""
    m = Mesh("SM_Sail_Boathouse_DoorLeaf")
    w, h, t = BOAT_DOOR[1] / 2 + 0.05, BOAT_DOOR[2] + 0.02, 0.06
    m.bx("SL_PaintSteel", -w / 2, w / 2, -t / 2, t / 2, 0.0, h)
    fr = 0.08
    for (xa, xb, za, zb) in ((-w / 2 + fr, w / 2 - fr, 0.0, fr), (-w / 2 + fr, w / 2 - fr, h - fr, h), (-w / 2, -w / 2 + fr, 0.0, h), (w / 2 - fr, w / 2, 0.0, h),
                             (-w / 2 + fr, w / 2 - fr, h * 0.55 - fr / 2, h * 0.55 + fr / 2)):
        m.bx("SL_PaintSteel", xa, xb, -t / 2 - 0.03, -t / 2, za, zb)
    for k in range(13):
        z = fr + 0.06 + k * (h * 0.55 - fr - 0.12) / 12
        m.bx("SL_PaintSteel", -w / 2 + fr, w / 2 - fr, -t / 2 - 0.015, -t / 2, z - 0.015, z + 0.015)
    # a row of small wired-glass lights in the upper part
    for k in range(3):
        xc = -w / 2 + fr + (w - 2 * fr) * (k + 0.5) / 3
        m.bx("SL_Glass", xc - 0.18, xc + 0.18, -t / 2 - 0.004, -t / 2 - 0.002, h * 0.62, h * 0.80)
        m.bx("SL_Dark", xc - 0.20, xc + 0.20, -t / 2 - 0.01, -t / 2, h * 0.60, h * 0.62)
        m.bx("SL_Dark", xc - 0.20, xc + 0.20, -t / 2 - 0.01, -t / 2, h * 0.80, h * 0.82)
    m.bx("CT_Steel", -0.05, 0.05, -t / 2 - 0.06, -t / 2 - 0.03, 1.0, 1.25)
    for x in (-w / 2 + 0.2, w / 2 - 0.2):
        m.bx("CT_Steel", x - 0.03, x + 0.03, -0.03, 0.03, h, h + 0.13)
        m.lathe([(0.0, -0.02), (0.05, -0.02), (0.05, 0.02), (0.0, 0.02)], "SL_Black", seg=10, c=(x, 0.0, h + 0.16),
                axis=Matrix(((1, 0, 0), (0, 0, 1), (0, -1, 0))).transposed())
    m.closed = False
    m.add_box_collision((0, 0, h / 2), (w, t + 0.06, h))
    return m


# ================================================================================================= props
def P(name):
    m = Mesh("SM_Sail_Prop_" + name)
    m.closed = False
    return m


def prop_opti_rack():
    """Three-high rack for optimists (hulls upside down on padded arms), 2.4 m long (the boats lie across it)."""
    m = P("OptiRack")
    for x in (-0.9, 0.9):
        for y in (-0.55, 0.55):
            m.bx("SL_WoodPaint", x - 0.05, x + 0.05, y - 0.05, y + 0.05, 0.0, 2.25)
        for z in (0.35, 1.05, 1.75):
            m.bx("SL_WoodPaint", x - 0.05, x + 0.05, -0.62, 0.62, z - 0.05, z + 0.05)
            m.bx("SL_Canvas", x - 0.06, x + 0.06, -0.50, 0.50, z + 0.05, z + 0.08)
        m.bx("SL_WoodPaint", x - 0.05, x + 0.05, -0.62, 0.62, 2.20, 2.28)
    for y in (-0.55, 0.55):
        m.bx("SL_WoodPaint", -0.95, 0.95, y - 0.04, y + 0.04, 2.20, 2.28)
    m.add_box_collision((0, 0, 1.14), (2.0, 1.3, 2.28))
    return m


def prop_mast_bracket():
    m = P("MastBracket")
    m.bx("SL_Galv", -0.03, 0.03, -0.02, 0.0, -0.25, 0.25)
    for z in (-0.15, 0.10):
        m.bx("SL_Galv", -0.02, 0.02, -0.40, 0.0, z - 0.015, z + 0.015)
        m.bx("SL_Galv", -0.02, 0.02, -0.40, -0.37, z + 0.015, z + 0.07)
    return m


def prop_workbench():
    m = P("Workbench")
    m.bx("SL_WoodRaw", -1.0, 1.0, -0.4, 0.4, 0.82, 0.90)
    for x in (-0.92, 0.92):
        for y in (-0.33, 0.33):
            m.bx("SL_WoodRaw", x - 0.04, x + 0.04, y - 0.04, y + 0.04, 0.0, 0.82)
    m.bx("SL_WoodRaw", -0.95, 0.95, -0.36, 0.36, 0.15, 0.18)
    # a vice, a plane, a mallet, offcuts, a clamp
    m.bx("SL_Iron", -0.98, -0.80, -0.47, -0.33, 0.80, 0.98)
    m.bx("SL_Iron", -0.95, -0.83, -0.52, -0.47, 0.84, 0.95)
    m.bx("SL_WoodRaw", 0.10, 0.36, -0.10, -0.03, 0.90, 0.96)
    m.bx("SL_Iron", 0.18, 0.28, -0.09, -0.04, 0.96, 0.99)
    m.tube((0.5, 0.1, 0.92), (0.8, 0.15, 0.92), 0.012, "SL_WoodRaw", seg=6)
    m.box((0.5, 0.1, 0.94), (0.06, 0.12, 0.07), "SL_WoodRaw")
    for k in range(3):
        m.bx("SL_WoodRaw", -0.5 + k * 0.12, -0.42 + k * 0.12, 0.05, 0.35, 0.90, 0.92 + 0.02 * k)
    m.add_box_collision((0, 0, 0.45), (2.0, 0.8, 0.9))
    return m


def prop_tool_board():
    m = P("ToolBoard")
    picture_box(m, (0, 0, 1.45), (0, -1, 0), 1.2, 1.2, 0.02, "toolboard")
    # real tools on the painted outlines
    m.bx("SL_Iron", -0.47, -0.43, -0.05, -0.02, 1.25, 1.75)
    m.bx("SL_WoodRaw", -0.49, -0.41, -0.06, -0.02, 1.25, 1.42)
    m.bx("SL_Iron", -0.29, -0.27, -0.05, -0.03, 1.36, 1.72)
    m.bx("SL_Iron", -0.16, -0.12, -0.05, -0.02, 1.30, 1.78)
    m.tube((0.25, -0.04, 1.62), (0.40, -0.04, 1.62), 0.006, "SL_RopeRed", seg=5)
    BT.torus(m, (0.32, -0.04, 1.62), 0.10, 0.012, "SL_Rope", seg=16, rs=4, M=Matrix(((1, 0, 0), (0, 0, 1), (0, -1, 0))).transposed())
    m.bx("SL_WoodRaw", -0.35, 0.35, -0.05, -0.03, 1.12, 1.16)
    return m


def prop_paint_tins():
    m = P("PaintTins")
    for k, (x, y, r, h) in enumerate(((0, 0, 0.09, 0.2), (0.22, 0.05, 0.09, 0.2), (0.1, 0.22, 0.07, 0.14), (0.32, 0.25, 0.05, 0.1), (-0.15, 0.2, 0.09, 0.2))):
        m.lathe([(0.0, 0.0), (r, 0.0), (r, h), (r * 0.94, h + 0.005), (0.0, h + 0.005)], "SL_PaintTin", seg=12, c=(x, y, 0))
        m.lathe([(r + 0.001, h * 0.25), (r + 0.002, h * 0.25), (r + 0.002, h * 0.75), (r + 0.001, h * 0.75), (r + 0.001, h * 0.25)],
                ["SL_Red", "SL_Blue", "SL_White", "SL_Green", "SL_Yellow"][k], seg=12, c=(x, y, 0), cap0=False, cap1=False)
    m.bx("SL_WoodRaw", 0.30, 0.55, -0.12, -0.08, 0.0, 0.02)
    return m


def prop_trestle():
    m = P("Trestle")
    m.bx("SL_WoodRaw", -0.45, 0.45, -0.06, 0.06, 0.66, 0.74)
    for x in (-0.38, 0.38):
        for sg in (1, -1):
            m.beam((x, sg * 0.30, 0.0), (x, sg * 0.03, 0.68), 0.06, 0.06, "SL_WoodRaw", up=(1, 0, 0))
    m.add_box_collision((0, 0, 0.37), (0.9, 0.7, 0.74))
    return m


def prop_hull_frame():
    """A cadet hull being built upside down on two trestles: plywood frames, hog, chine stringers, part planked."""
    b = "cadet"
    s = B.BOATS[b]
    m = P("HullFrame")
    xs = [-1.55, -1.0, -0.45, 0.10, 0.65, 1.15, 1.55]
    sheer0 = max(B.sheer(b, x) for x in xs)
    for x in xs:
        sec = BT.section(b, x)
        # upside down: z' = sheer0 - z (the sheer on the trestles, the keel up)
        pts = [(x, -y, sheer0 - z) for y, z in reversed(sec)] + [(x, y, sheer0 - z) for y, z in sec[1:]]
        for a, c in zip(pts, pts[1:]):
            m.beam(a, c, 0.018, 0.07, "SL_WoodRaw", up=(1, 0, 0))
    hog = [(x, 0, sheer0 - B.lerp_keys(s["keel_z"], BT.u_of(s, x)) + 0.02) for x in xs]
    m.pipe(hog, 0.025, "SL_Wood", seg=4)
    for side in (1, -1):
        for frac in (0.25, 0.55, 1.0):
            pts = []
            for x in xs:
                sec = BT.section(b, x)
                k = min(len(sec) - 1, int(round(frac * (len(sec) - 1))))
                y, z = sec[k]
                pts.append((x, side * y, sheer0 - z))
            m.pipe(pts, 0.014, "SL_Wood", seg=4)
    # two bottom planks on, port side
    for x0, x1 in ((-1.55, 0.10), (0.10, 1.15)):
        s0, s1 = BT.section(b, x0), BT.section(b, x1)
        q = [(x0, 0.0, sheer0 - s0[0][1] + 0.03), (x1, 0.0, sheer0 - s1[0][1] + 0.03),
             (x1, s1[2][0], sheer0 - s1[2][1] + 0.03), (x0, s0[2][0], sheer0 - s0[2][1] + 0.03)]
        m.face(q, "SL_FoilWood")
        m.face(q[::-1], "SL_FoilWood")
    return m


def prop_blackboard():
    m = P("Blackboard")
    picture_box(m, (0, 0, 1.55), (0, -1, 0), 2.6, 1.3, 0.04, "blackboard", frame_slot="SL_Dark")
    m.bx("SL_WoodRaw", -1.3, 1.3, -0.12, 0.0, 0.86, 0.89)
    for k in range(4):
        m.bx("SL_White", -0.9 + k * 0.08, -0.85 + k * 0.08, -0.08, -0.06, 0.89, 0.90)
    m.add_box_collision((0, -0.04, 1.5), (2.6, 0.12, 1.4))
    return m


def prop_desk():
    """A two-seat school desk with its bench (the classroom has six: twelve pupils)."""
    m = P("Desk")
    m.bx("CT_WoodLight", -0.6, 0.6, -0.22, 0.22, 0.72, 0.75)
    m.bx("CT_WoodLight", -0.58, 0.58, -0.20, 0.18, 0.60, 0.62)
    for x in (-0.55, 0.55):
        m.bx("SL_Iron", x - 0.02, x + 0.02, -0.25, 0.25, 0.0, 0.03)
        m.bx("SL_Iron", x - 0.02, x + 0.02, -0.02, 0.02, 0.0, 0.72)
        m.bx("SL_Iron", x - 0.02, x + 0.02, 0.20, 0.64, 0.0, 0.03)
        m.bx("SL_Iron", x - 0.02, x + 0.02, 0.42, 0.46, 0.0, 0.43)
    m.bx("CT_WoodLight", -0.6, 0.6, 0.30, 0.58, 0.43, 0.46)
    m.bx("CT_WoodLight", -0.6, 0.6, 0.60, 0.63, 0.55, 0.80)
    m.add_box_collision((0, 0.2, 0.4), (1.2, 0.9, 0.8))
    return m


def prop_coach_desk():
    m = P("CoachDesk")
    m.bx("CT_WoodLight", -0.7, 0.7, -0.35, 0.35, 0.74, 0.77)
    m.bx("CT_WoodLight", 0.25, 0.68, -0.33, 0.33, 0.0, 0.74)
    m.bx("CT_WoodLight", -0.68, -0.62, -0.33, 0.33, 0.0, 0.74)
    m.bx("CT_WoodLight", -0.62, 0.25, 0.30, 0.33, 0.3, 0.74)
    for z in (0.2, 0.45, 0.66):
        m.bx("SL_Brass", 0.24, 0.25, -0.05, 0.05, z - 0.01, z + 0.01)
    # a journal, a stopwatch, a whistle
    m.bx("SL_Blue", -0.3, 0.0, -0.15, 0.1, 0.77, 0.79)
    m.lathe([(0.0, 0.0), (0.03, 0.0), (0.03, 0.012), (0.0, 0.014)], "SL_Steel", seg=10, c=(0.25, 0.1, 0.77))
    m.add_box_collision((0, 0, 0.385), (1.4, 0.7, 0.77))
    return m


def prop_chair():
    m = P("Chair")
    m.bx("CT_WoodLight", -0.21, 0.21, -0.21, 0.21, 0.44, 0.47)
    for x in (-0.18, 0.18):
        for y in (-0.18, 0.18):
            m.bx("CT_WoodLight", x - 0.018, x + 0.018, y - 0.018, y + 0.018, 0.0, 0.44)
        m.bx("CT_WoodLight", x - 0.018, x + 0.018, 0.17, 0.21, 0.47, 0.88)
    m.bx("CT_WoodLight", -0.2, 0.2, 0.18, 0.21, 0.70, 0.86)
    m.add_box_collision((0, 0, 0.44), (0.42, 0.42, 0.88))
    return m


def prop_knot_board():
    m = P("KnotBoard")
    picture_box(m, (0, 0, 1.5), (0, -1, 0), 0.9, 0.9, 0.03, "knotboard")
    return m


def prop_flag_chart():
    m = P("FlagChart")
    picture_box(m, (0, 0, 1.55), (0, -1, 0), 0.8, 0.8, 0.01, "flagchart", frame_slot="SL_WoodRaw")
    return m


def prop_chart():
    m = P("SeaChart")
    picture_box(m, (0, 0, 1.6), (0, -1, 0), 1.4, 0.7, 0.01, "chart", frame_slot="SL_Dark")
    return m


def cup(m, c, h, slot="SL_Brass", seg=14):
    x, y, z = c
    prof = [(0.0, 0.0), (h * 0.28, 0.0), (h * 0.28, h * 0.08), (h * 0.08, h * 0.12), (h * 0.06, h * 0.40), (h * 0.12, h * 0.46),
            (h * 0.30, h * 0.70), (h * 0.36, h * 0.98), (h * 0.33, h), (h * 0.20, h * 0.75), (0.0, h * 0.72)]
    m.lathe(prof, slot, seg=seg, c=c)
    for sg in (1, -1):
        m.pipe([(x + sg * h * 0.30, y, z + h * 0.85), (x + sg * h * 0.48, y, z + h * 0.80), (x + sg * h * 0.42, y, z + h * 0.60),
                (x + sg * h * 0.22, y, z + h * 0.56)], h * 0.025, slot, seg=5)


def prop_cup():
    m = P("Cup")
    m.bx("SL_Dark", -0.06, 0.06, -0.06, 0.06, 0.0, 0.05)
    cup(m, (0, 0, 0.05), 0.28)
    return m


def pennant(m, c, name, w=0.22, h=0.30):
    x, y, z = c
    m.tube((x - w / 2 - 0.02, y, z), (x + w / 2 + 0.02, y, z), 0.005, "SL_Brass", seg=5)
    q = [(x - w / 2, y - 0.002, z - h), (x + w / 2, y - 0.002, z - h), (x + w / 2, y - 0.002, z), (x - w / 2, y - 0.002, z)]
    pts = [(x - w / 2, y - 0.002, z), (x + w / 2, y - 0.002, z), (x, y - 0.002, z - h)]
    uv = cell(name)
    tri_uv = [uv[3], uv[2], ((uv[0][0] + uv[1][0]) / 2, uv[0][1])]
    m.face(pts[::-1], "SLP_Atlas", uvs=tri_uv[::-1])
    m.face([(p[0], y + 0.002, p[2]) for p in pts], "SL_Red")
    del q


def prop_pennant():
    m = P("Pennant")
    pennant(m, (0, 0, 0.30), "pennant0")
    return m


def prop_trophy_shelf():
    m = P("TrophyShelf")
    for z in (1.0, 1.45, 1.9):
        m.bx("CT_WoodLight", -0.9, 0.9, -0.25, 0.0, z - 0.02, z + 0.02)
    for x in (-0.9, 0.9):
        m.bx("CT_WoodLight", x - 0.02, x + 0.02, -0.25, 0.0, 0.95, 1.95)
    for k, (x, z, h) in enumerate(((-0.6, 1.02, 0.30), (-0.2, 1.02, 0.22), (0.25, 1.02, 0.36), (0.6, 1.02, 0.20), (-0.45, 1.47, 0.18), (0.4, 1.47, 0.25))):
        cup(m, (x, -0.12, z), h, slot="SL_Brass" if k % 3 else "SL_Steel", seg=12)
    for k in range(4):
        pennant(m, (-0.6 + k * 0.4, -0.005, 2.30), "pennant%d" % k)
    m.add_box_collision((0, -0.12, 1.45), (1.8, 0.25, 1.0))
    return m


def prop_samovar_table():
    m = P("KettleTable")
    m.bx("CT_WoodLight", -0.35, 0.35, -0.3, 0.3, 0.70, 0.73)
    for x in (-0.31, 0.31):
        for y in (-0.26, 0.26):
            m.bx("CT_WoodLight", x - 0.02, x + 0.02, y - 0.02, y + 0.02, 0.0, 0.70)
    m.lathe([(0.0, 0.0), (0.10, 0.0), (0.08, 0.04), (0.06, 0.06), (0.14, 0.10), (0.15, 0.30), (0.12, 0.36), (0.05, 0.38),
             (0.05, 0.44), (0.07, 0.46), (0.0, 0.47)], "SL_Brass", seg=16, c=(-0.1, 0, 0.73))
    m.tube((0.03, 0, 0.88), (0.12, 0, 0.86), 0.012, "SL_Brass", seg=6)
    m.lathe([(0.0, 0.0), (0.05, 0.0), (0.06, 0.08), (0.0, 0.085)], "SL_White", seg=10, c=(-0.1, 0, 1.2))
    for k in range(3):
        m.lathe([(0.0, 0.0), (0.035, 0.0), (0.04, 0.08), (0.036, 0.08), (0.03, 0.006), (0.0, 0.006)], "SL_Glass", seg=10, c=(0.15, -0.15 + k * 0.1, 0.73))
    m.add_box_collision((0, 0, 0.6), (0.7, 0.6, 1.2))
    return m


def prop_wall_clock():
    m = P("WallClock")
    m.lathe([(0.0, 0.0), (0.17, 0.0), (0.17, 0.05), (0.0, 0.05)], "SL_Dark", seg=20, axis=Matrix(((1, 0, 0), (0, 0, -1), (0, 1, 0))).transposed())
    m.face([(-0.15, -0.051, -0.15), (0.15, -0.051, -0.15), (0.15, -0.051, 0.15), (-0.15, -0.051, 0.15)], "SLP_Atlas", uvs=cell("clock"))
    return m


def prop_barometer():
    m = P("Barometer")
    m.lathe([(0.0, 0.0), (0.13, 0.0), (0.13, 0.04), (0.0, 0.04)], "SL_Brass", seg=20, axis=Matrix(((1, 0, 0), (0, 0, -1), (0, 1, 0))).transposed())
    m.face([(-0.11, -0.041, -0.11), (0.11, -0.041, -0.11), (0.11, -0.041, 0.11), (-0.11, -0.041, 0.11)], "SLP_Atlas", uvs=cell("barometer"))
    m.bx("CT_WoodLight", -0.17, 0.17, -0.0, 0.02, -0.17, 0.17)
    return m


def prop_anemometer():
    m = P("AnemometerDisplay")
    m.bx("SL_Dark", -0.16, 0.16, -0.08, 0.0, -0.09, 0.09)
    m.face([(-0.13, -0.081, -0.06), (0.13, -0.081, -0.06), (0.13, -0.081, 0.07), (-0.13, -0.081, 0.07)], "SLP_Atlas", uvs=cell("anemometer"))
    return m


def prop_radio():
    m = P("Radio")
    m.bx("CT_WoodLight", -0.25, 0.25, -0.12, 0.12, 0.0, 0.26)
    m.face([(-0.23, -0.121, 0.03), (0.23, -0.121, 0.03), (0.23, -0.121, 0.23), (-0.23, -0.121, 0.23)], "SLP_Atlas", uvs=cell("radio"))
    for x in (0.12, 0.19):
        m.tube((x, -0.12, 0.06), (x, -0.15, 0.06), 0.018, "SL_Dark", seg=10)
    m.tube((0.2, 0.05, 0.26), (0.05, 0.08, 0.75), 0.003, "SL_Steel", seg=4)
    return m


def prop_bell():
    m = P("ShipBell")
    m.bx("CT_WoodLight", -0.08, 0.08, -0.0, 0.03, -0.05, 0.35)
    m.tube((0, -0.0, 0.30), (0, -0.14, 0.30), 0.01, "SL_Brass", seg=6)
    m.lathe([(0.0, 0.0), (0.09, 0.0), (0.085, 0.02), (0.06, 0.10), (0.05, 0.17), (0.0, 0.18)], "SL_Brass", seg=16, c=(0, -0.14, 0.10))
    m.pipe([(0, -0.14, 0.10), (0, -0.14, -0.10)], 0.004, "SL_Rope", seg=4)
    return m


def prop_photo(k=0):
    m = P("Photo%d" % k)
    picture_box(m, (0, 0, 0), (0, -1, 0), 0.40, 0.30, 0.02, "photo%d" % k, frame_slot="SL_Dark")
    return m


def prop_drawing(k=0):
    """A child's drawing of a boat pinned to the wall (paper with four drawing pins; origin on the wall, front = -Y)."""
    m = P("Drawing%d" % k)
    ang = (-4, 3, -2, 5)[k] * math.pi / 180
    c, s = math.cos(ang), math.sin(ang)
    w, h = 0.42, 0.42
    pts = [(-w / 2 * c + h / 2 * s, -0.003, -w / 2 * s - h / 2 * c), (w / 2 * c + h / 2 * s, -0.003, w / 2 * s - h / 2 * c),
           (w / 2 * c - h / 2 * s, -0.003, w / 2 * s + h / 2 * c), (-w / 2 * c - h / 2 * s, -0.003, -w / 2 * s + h / 2 * c)]
    m.face(pts, "SLP_Atlas", uvs=cell("drawing%d" % k))
    m.face(pts[::-1], "SL_Paper")
    for p in pts:
        q = Vector(p) * 0.9
        m.sphere((q.x, -0.008, q.z), 0.008, ("SL_Red", "SL_Blue", "SL_Yellow", "SL_Green")[k], seg=6, rings=4)
    return m


def prop_locker():
    m = P("Lockers")
    for k in range(4):
        x0 = -0.8 + k * 0.4
        m.bx("SL_Green", x0 + 0.005, x0 + 0.395, -0.25, 0.25, 0.10, 1.85, slots={"y": "SL_Green"})
        for z in (1.6, 1.65, 1.7):
            m.bx("SL_Dark", x0 + 0.08, x0 + 0.32, -0.255, -0.25, z, z + 0.02)
        m.bx("SL_Steel", x0 + 0.33, x0 + 0.35, -0.27, -0.25, 1.0, 1.12)
    m.bx("SL_Dark", -0.8, 0.8, -0.25, 0.25, 0.0, 0.10)
    m.add_box_collision((0, 0, 0.93), (1.6, 0.5, 1.85))
    return m


def prop_bench():
    m = P("Bench")
    m.bx("CT_WoodLight", -0.9, 0.9, -0.16, 0.16, 0.42, 0.46)
    for x in (-0.8, 0.8):
        m.bx("CT_WoodLight", x - 0.03, x + 0.03, -0.14, 0.14, 0.0, 0.42)
    m.bx("CT_WoodLight", -0.75, 0.75, -0.03, 0.03, 0.12, 0.16)
    m.add_box_collision((0, 0, 0.23), (1.8, 0.32, 0.46))
    return m


def lifejacket(m, M=Matrix.Identity(4), worn=False):
    """A foam-block buoyancy aid (red, a white band): two front panels and the back, open at the front."""
    seg = 10
    pts = []
    for k in range(seg + 1):
        a = math.pi * 0.12 + (2 * math.pi - math.pi * 0.24) * k / seg
        pts.append((math.cos(a) * 0.17, math.sin(a) * 0.14))
    rows = []
    for z in (-0.30, -0.12, 0.08, 0.14):
        sc = 1.0 if z < 0.1 else 0.85
        rows.append([M @ Vector((x * sc, y * sc, z)) for x, y in pts])
    outer = [[tuple(v) for v in r] for r in rows]
    m.quad_grid(outer, "SL_Buoyancy", smooth=True, flip=True)
    inner_rows = []
    for z in (-0.30, -0.12, 0.08, 0.14):
        sc = (0.80 if z < 0.1 else 0.68)
        inner_rows.append([tuple(M @ Vector((x * sc, y * sc, z))) for x, y in pts])
    m.quad_grid(inner_rows, "SL_Buoyancy", smooth=True, flip=False)
    for i in range(len(rows) - 1):
        for j in (0, seg):
            q = [outer[i][j], outer[i + 1][j], inner_rows[i + 1][j], inner_rows[i][j]]
            m.face(q if j == 0 else q[::-1], "SL_Buoyancy")
    for r_o, r_i, sgn in ((outer[0], inner_rows[0], 1), (outer[-1], inner_rows[-1], -1)):
        for j in range(seg):
            q = [r_o[j], r_o[j + 1], r_i[j + 1], r_i[j]]
            m.face(q if sgn > 0 else q[::-1], "SL_Buoyancy")
    # white band and a buckle
    band = [tuple(M @ Vector((x * 1.01, y * 1.01, -0.18))) for x, y in pts]
    m.pipe(band, 0.012, "SL_White", seg=4)
    m.box(tuple(M @ Vector((0.175, 0.0, -0.18))), (0.02, 0.06, 0.04), "SL_Black")


def prop_lifejacket():
    m = P("LifeJacket")
    lifejacket(m, Matrix.Translation((0, 0, 0.30)))
    return m


def prop_lifejacket_worn():
    """The worn life jacket (item life_jacket): origin at the base of the neck, +X forward (the open front), sized for a
    1.75 m adult torso (chest 0.96 m round); attach to the neck/spine socket, scale for children (0.8)."""
    m = Mesh("SM_Sail_LifeJacket_Worn")
    m.closed = False
    lifejacket(m, Matrix.Translation((0, 0, -0.16)) @ Matrix.Scale(1.08, 4))
    return m


def prop_jacket_rail():
    m = P("JacketRail")
    m.tube((-1.0, 0, 1.75), (1.0, 0, 1.75), 0.016, "SL_Galv", seg=8)
    for x in (-1.0, 1.0):
        m.bx("SL_Galv", x - 0.02, x + 0.02, -0.02, 0.06, 1.65, 1.8)
    for k in range(6):
        x = -0.8 + k * 0.32
        m.pipe([(x, 0, 1.76), (x, -0.03, 1.70), (x, -0.06, 1.68)], 0.005, "SL_Steel", seg=4)
        lifejacket(m, Matrix.Translation((x, -0.10, 1.42)) @ Matrix.Rotation(math.radians(-90 + (k % 2) * 8), 4, "Z") @ Matrix.Scale(0.9, 4))
    m.add_box_collision((0, -0.12, 1.4), (2.1, 0.35, 0.8))
    return m


def prop_oar():
    m = P("Paddle")
    m.tube((0, 0, 0), (1.25, 0, 0), 0.016, "SL_WoodRaw", seg=8)
    m.lathe([(0.0, 0.0), (0.03, 0.0), (0.03, 0.10), (0.0, 0.11)], "SL_Black", seg=8, axis=Matrix(((0, 0, 1), (0, 1, 0), (-1, 0, 0))).transposed())
    m.box((1.40, 0, 0), (0.42, 0.15, 0.012), "SL_WoodRaw")
    return m


def prop_bailer():
    m = P("Bailer")
    m.lathe([(0.0, 0.0), (0.06, 0.005), (0.075, 0.07), (0.07, 0.075)], "SL_Yellow", seg=12, cap1=False)
    m.beam((0, 0, 0.06), (0.10, 0, 0.09), 0.025, 0.02, "SL_Yellow")
    return m


def prop_shelf():
    """Wet-store shelving: 2 m, four shelves, with folded sails, bailers and spare blocks on it."""
    m = P("Shelf")
    for x in (-1.0, 1.0):
        for y in (-0.22, 0.22):
            m.bx("SL_Galv", x - 0.02, x + 0.02, y - 0.02, y + 0.02, 0.0, 2.0)
    for z in (0.15, 0.65, 1.15, 1.65):
        m.bx("SL_WoodRaw", -1.0, 1.0, -0.24, 0.24, z - 0.015, z + 0.015)
    for k in range(4):
        m.bx("SL_White", -0.9 + k * 0.45, -0.55 + k * 0.45, -0.18, 0.18, 0.665, 0.78)
        m.bx("SL_CanvasBlue", -0.9 + k * 0.45, -0.6 + k * 0.45, -0.15, 0.15, 1.165, 1.32)
    for k in range(3):
        m.lathe([(0.0, 0.0), (0.06, 0.005), (0.075, 0.07), (0.07, 0.075)], "SL_Yellow", seg=10, c=(-0.7 + k * 0.2, 0, 0.165), cap1=False)
    m.add_box_collision((0, 0, 1.0), (2.0, 0.48, 2.0))
    return m


def prop_lamp():
    m = P("Lamp")
    m.tube((0, 0, 0), (0, 0, -0.8), 0.006, "SL_Dark", seg=6)
    m.lathe([(0.0, -0.8), (0.04, -0.8), (0.16, -0.95), (0.18, -1.0), (0.0, -1.0)], "SL_Green", seg=14)
    m.sphere((0, 0, -1.0), 0.07, "SL_Lamp", seg=10, rings=6)
    return m


# ---------------------------------------------------------------------------------------------------- outdoor
def prop_slip_board():
    """One duckboard section of the slip over the sand: 3.6 m wide (across), 1.2 m long (down the beach, along +X)."""
    m = P("SlipBoard")
    for y in (-1.6, 0.0, 1.6):
        m.bx("SL_WoodRaw", -0.6, 0.6, y - 0.05, y + 0.05, 0.0, 0.06)
    for k in range(8):
        x = -0.6 + 0.075 + k * 0.15
        m.bx("SL_WoodRaw", x - 0.06, x + 0.06, -1.8, 1.8, 0.06, 0.09)
    m.add_box_collision((0, 0, 0.045), (1.2, 3.6, 0.09))
    return m


def flag_mesh(fid, w=0.9, h=0.66):
    """A signal flag as a 6 x 4 grid (for the cloth wind WPO): origin at the hoist's top, flying along +X, both faces;
    UV0 into sail_flags.png, UV1 = (distance from the hoist 0..1, 0) like the festival cloth."""
    m = Mesh("SM_Sail_Flag_" + fid, uv1=True)
    m.closed = False
    with open(os.path.join(HERE, "atlas_cells.json")) as f:
        fc = json.load(f)["flags"][fid]
    u0, y0, cw, ch = fc
    nx, nz = 6, 4
    for side, yoff, flip in ((0, -0.001, False), (1, 0.001, True)):
        P_ = [[(w * j / nx, yoff, -h * i / nz) for j in range(nx + 1)] for i in range(nz + 1)]
        uvf = lambda i, j: (u0 + cw * j / nx, 1 - y0 - ch * i / nz)
        m.quad_grid(P_, "SLP_Flags", uvf=uvf, uv1f=lambda i, j: (j / nx, 0.0), flip=not flip, colf=lambda i, j: (0, 1, side, 1))
    for f in m.bm.faces:
        pass
    return m


def prop_signal_mast():
    """10 m signal mast with a yard (cross-tree) and halyards; flags hang from socket points listed in the layout."""
    m = P("SignalMast")
    m.lathe([(0.0, 0.0), (0.09, 0.0), (0.07, 9.8), (0.05, 10.0), (0.0, 10.0)], "SL_White", seg=12)
    m.sphere((0, 0, 10.05), 0.08, "SL_White", seg=10, rings=6)
    m.bx("SL_White", -0.05, 0.05, -1.6, 1.6, 7.5, 7.6)
    m.bx("CT_Concrete", -0.45, 0.45, -0.45, 0.45, -0.3, 0.12)
    for y in (-1.5, 1.5, 0.0):
        zt = 7.5 if y else 9.9
        m.pipe([(0.06 if y == 0 else 0.0, y, zt), (0.06, y * 0.15, 1.3)], 0.004, "SL_Rope", seg=4)
    # pin rail (a bar across the mast's foot with cleats) and the halyards made up on it in hanging coils
    m.bx("SL_Galv", 0.08, 0.12, -0.45, 0.45, 1.18, 1.24)
    m.bx("SL_Galv", 0.0, 0.10, -0.03, 0.03, 1.18, 1.24)
    Mv = Matrix(((0, 1, 0), (0, 0, 1), (1, 0, 0))).transposed()
    for k, y in enumerate((-0.33, -0.11, 0.11, 0.33)):
        BT.cleat(m, 0.12, y, 1.24, yaw=math.pi / 2, L=0.09)
        for j in range(3):
            BT.torus(m, (0.16, y, 1.02 - j * 0.004), 0.16 - j * 0.01, 0.006, "SL_Rope", seg=14, rs=4, M=Mv)
    # stays: from the masthead and the yard's ends down to three anchors in the sand (galvanised wire, bottle screws)
    for a in (0.0, 2.1, 4.2):
        ax, ay = math.cos(a) * 3.2, math.sin(a) * 3.2
        m.pipe([(0.0, 0.0, 9.6), (ax, ay, 0.15)], 0.005, "SL_Galv", seg=4)
        m.tube((ax * 0.97, ay * 0.97, 0.15), (ax * 0.97, ay * 0.97, 0.55), 0.012, "SL_Galv", seg=6)
        m.bx("CT_Concrete", ax - 0.2, ax + 0.2, ay - 0.2, ay + 0.2, -0.3, 0.12)
    for y in (-1.55, 1.55):
        m.pipe([(0.0, y, 7.5), (0.0, y * 1.2, 0.15)], 0.004, "SL_Galv", seg=4)
        m.bx("CT_Concrete", -0.15, 0.15, y * 1.2 - 0.15, y * 1.2 + 0.15, -0.3, 0.12)
    m.add_box_collision((0, 0, 5.0), (0.25, 0.25, 10.0))
    return m


def prop_windsock_pole():
    m = P("WindsockPole")
    m.tube((0, 0, 0), (0, 0, 2.0), 0.03, "SL_White", seg=8)
    m.lathe([(0.0, 1.95), (0.05, 1.95), (0.05, 2.05), (0.0, 2.05)], "SL_Black", seg=8)
    return m


def prop_windsock():
    """The sock (turns about the pole's axis: origin on it at the swivel, the sock along +X; UV1.x = 0 at the mouth .. 1 at
    the tail for the WPO droop/flutter), orange-white bands."""
    m = Mesh("SM_Sail_Windsock", uv1=True)
    m.closed = False
    m.tube((0, 0, 0), (0.12, 0, 0), 0.01, "SL_Steel", seg=6)
    n, seg, Lk = 10, 12, 1.5
    rows = []
    for i in range(n + 1):
        t = i / n
        r = 0.20 * (1 - 0.55 * t)
        x = 0.12 + Lk * t
        rows.append([(x, r * math.cos(2 * math.pi * k / seg), -0.20 + r * math.sin(2 * math.pi * k / seg) + 0.2 * (1 - t) * 0 + (0.20 - r)) for k in range(seg)])
    for i in range(n):
        slot = "SL_Orange" if (i // 2) % 2 == 0 else "SL_White"
        m.quad_grid(rows[i:i + 2], slot, closed_u=True, smooth=True, uv1f=lambda a, b, i=i: ((i + a) / n, 0.0))
        m.quad_grid(rows[i:i + 2], slot, closed_u=True, smooth=True, flip=True, uv1f=lambda a, b, i=i: ((i + a) / n, 0.0))
    BT.torus(m, (0.12, 0, 0.0), 0.20, 0.008, "SL_Steel", seg=14, rs=4, M=Matrix(((0, 1, 0), (0, 0, 1), (1, 0, 0))).transposed())
    return m


def prop_hose_stand():
    m = P("HoseStand")
    m.tube((0, 0, 0), (0, 0, 1.0), 0.03, "SL_Galv", seg=8)
    m.bx("CT_Concrete", -0.25, 0.25, -0.25, 0.25, -0.2, 0.05)
    m.tube((0, 0, 0.9), (0.12, 0, 0.9), 0.012, "SL_Brass", seg=6)
    m.lathe([(0.0, 0.0), (0.04, 0.0), (0.04, 0.02), (0.0, 0.02)], "SL_Red", seg=8, c=(0, 0, 1.0))
    for k in range(4):
        BT.torus(m, (0, -0.05, 0.6 - k * 0.02), 0.22 - k * 0.01, 0.012, "SL_Green", seg=16, rs=4,
                 M=Matrix(((1, 0, 0), (0, 0, 1), (0, -1, 0))).transposed())
    m.bx("SL_Galv", -0.02, 0.02, -0.05, 0.0, 0.7, 0.85)
    return m


def prop_outdoor_bench():
    m = P("OutdoorBench")
    for x in (-0.8, 0.8):
        m.bx("CT_Concrete", x - 0.08, x + 0.08, -0.2, 0.2, 0.0, 0.40)
    for k in range(3):
        y = -0.15 + k * 0.15
        m.bx("CT_Wood", -1.0, 1.0, y - 0.06, y + 0.06, 0.40, 0.45)
    m.add_box_collision((0, 0, 0.225), (2.0, 0.45, 0.45))
    return m


def prop_notice_board():
    """Timetable / results board: a glazed case on two legs; SLP_Notice (UV 0..1 over the panel) is what the engine
    writes on (a render target or a localized texture, like the town's boards)."""
    m = P("NoticeBoard")
    for x in (-0.65, 0.65):
        m.bx("CT_Steel", x - 0.03, x + 0.03, -0.03, 0.03, 0.0, 2.1)
    m.bx("CT_Wood", -0.65, 0.65, -0.06, 0.04, 0.95, 2.05)
    m.picture([(-0.6, -0.065, 1.0), (0.6, -0.065, 1.0), (0.6, -0.065, 2.0), (-0.6, -0.065, 2.0)], "SLP_Notice")
    m.bx("CT_Steel", -0.70, 0.70, -0.12, 0.08, 2.05, 2.10)
    m.add_box_collision((0, 0, 1.05), (1.4, 0.2, 2.1))
    return m


def prop_bollard():
    m = P("Bollard")
    m.lathe([(0.0, 0.0), (0.13, 0.0), (0.11, 0.15), (0.10, 0.45), (0.14, 0.50), (0.14, 0.55), (0.0, 0.56)], "SL_Iron", seg=14)
    m.add_box_collision((0, 0, 0.28), (0.28, 0.28, 0.56))
    return m


def prop_winch():
    """A hand winch on a post (pulls trolleys up the slip): drum on a horizontal axle along Y, crank handle."""
    m = P("Winch")
    m.bx("CT_Concrete", -0.25, 0.25, -0.25, 0.25, -0.2, 0.15)
    for y in (-0.15, 0.15):
        m.bx("SL_Galv", -0.06, 0.06, y - 0.01, y + 0.01, 0.15, 0.85)
    M = Matrix(((1, 0, 0), (0, 0, 1), (0, -1, 0))).transposed()
    m.lathe([(0.0, -0.14), (0.12, -0.14), (0.12, -0.12), (0.07, -0.12), (0.07, 0.12), (0.12, 0.12), (0.12, 0.14), (0.0, 0.14)], "SL_Galv", seg=14, c=(0, 0, 0.7), axis=M)
    for k in range(5):
        BT.torus(m, (0, -0.10 + k * 0.05, 0.7), 0.075, 0.006, "SL_Rope", seg=12, rs=4, M=Matrix(((1, 0, 0), (0, 0, -1), (0, 1, 0))).transposed())
    m.pipe([(0, 0.16, 0.7), (0, 0.22, 0.7), (0.0, 0.22, 0.95), (0, 0.32, 0.95)], 0.012, "SL_Galv", seg=6)
    m.add_box_collision((0, 0, 0.45), (0.5, 0.5, 0.9))
    return m


def prop_park():
    """The boat park: a concrete slab 15 m (toward the sea, +X) x 14 m with painted trolley bays, its fence (1.5 m chain-link
    on posts) and a 3.6 m gate in the seaward fence at the boathouse's side (+Y edge = against the boathouse aisle).
    Origin at the slab's landward corner nearest the boathouse (x 0..15, y -14..0) on the slab top."""
    m = P("Park")
    m.bx("CT_Paving", 0, 15, -14, 0, -0.15, 0.0)
    # bay lines (paint strips 5 cm, 1 mm above the slab)
    for k in range(1, 6):
        x = k * 2.5
        m.bx("SL_White", x - 0.025, x + 0.025, -13.0, -4.6, 0.0, 0.004)
    m.bx("SL_White", 0.5, 14.5, -4.625, -4.575, 0.0, 0.004)
    # the bays' numbers painted at their heads, and puddles in the slab's low spots
    for k in range(1, 7):
        x = (k - 0.5) * 2.5
        m.face([(x - 0.35, -5.35, 0.005), (x + 0.35, -5.35, 0.005), (x + 0.35, -4.75, 0.005), (x - 0.35, -4.75, 0.005)],
               "SLP_Atlas", uvs=cell("bay%d" % k))
    import random as _r
    rr = _r.Random(8)
    for (px_, py_, pr) in ((3.2, -2.2, 0.9), (9.6, -1.4, 0.6), (12.8, -7.2, 0.5), (5.9, -13.2, 0.7), (1.0, -9.5, 0.4)):
        pts = []
        for k in range(16):
            a = 2 * math.pi * k / 16
            r_ = pr * rr.uniform(0.6, 1.0)
            pts.append((px_ + math.cos(a) * r_, py_ + math.sin(a) * r_ * 0.65, 0.003))
        m.face(pts, "SL_Puddle")
    # fence: posts every 2.5 m, top rail, a mesh panel (SL_Galv thin box: a texture would do it in the game)
    def fence(a, b, gate=None):
        a, b = Vector(a), Vector(b)
        d = b - a
        n = max(1, int(round(d.length / 2.5)))
        for k in range(n + 1):
            p = a + d * k / n
            m.bx("SL_Galv", p.x - 0.03, p.x + 0.03, p.y - 0.03, p.y + 0.03, -0.1, 1.5)
        for k in range(n):
            p0, p1 = a + d * k / n, a + d * (k + 1) / n
            mid = (p0 + p1) / 2
            if gate and (mid - Vector(gate[0])).length < gate[1] / 2:
                continue
            for z in (0.08, 0.78, 1.48):
                m.beam((p0.x, p0.y, z), (p1.x, p1.y, z), 0.03, 0.03, "SL_Galv")
            # welded-mesh panel: vertical bars every 0.2 m
            nb = int((p1 - p0).length / 0.2)
            for j in range(1, nb):
                q = p0 + (p1 - p0) * j / nb
                m.bx("SL_Galv", q.x - 0.006, q.x + 0.006, q.y - 0.006, q.y + 0.006, 0.08, 1.48)
    # seaward fence (x = 15) with the gate near y = 0 (the boathouse side: gate 3.6 m from y -3.6 to 0)
    for k in range(5):
        y0 = -14 + k * 2.08
        fence((15, y0, 0), (15, y0 + 2.08, 0))
    fence((0, -14, 0), (15, -14, 0))
    fence((0, -14, 0), (0, 0, 0))
    # gate leaves (open inward)
    for y0, y1 in ((-3.6, -1.8), (-1.8, 0.0)):
        m.beam((15, y0, 0.1), (15, y1, 0.1), 0.03, 0.03, "SL_Galv")
        m.beam((15, y0, 1.4), (15, y1, 1.4), 0.03, 0.03, "SL_Galv")
        m.beam((15, (y0 + y1) / 2, 0.75), (15, (y0 + y1) / 2 + 0.01, 0.75), 0.02, 1.3, "SL_Galv")
    m.bx("SL_Galv", 14.97, 15.03, -3.65, -3.55, -0.1, 1.6)
    picture_box(m, (0.02, -7.0, 1.1), (1, 0, 0), 0.9, 0.45, 0.02, "parkplate", frame_slot="SL_Galv")
    m.add_box_collision((7.5, -7.0, -0.075), (15.0, 14.0, 0.15))
    for c, s in (((15.0, -8.8, 0.75), (0.08, 10.4, 1.5)), ((7.5, -14.0, 0.75), (15.0, 0.08, 1.5)), ((0.0, -7.0, 0.75), (0.08, 14.0, 1.5))):
        m.add_box_collision(c, s)
    return m


def prop_rope_coil():
    """Coiled lines hanging on a wall peg (origin on the wall at the peg's foot, front = -Y): three coils of different
    rope and a tail."""
    m = P("RopeCoil")
    m.tube((0, 0, 0), (0, -0.14, 0.02), 0.018, "SL_WoodRaw", seg=8)
    Mv = Matrix(((1, 0, 0), (0, 0, 1), (0, -1, 0))).transposed()
    for k, (slot, R, dx) in enumerate((("SL_Rope", 0.20, -0.03), ("SL_RopeBlue", 0.17, 0.0), ("SL_RopeRed", 0.15, 0.03))):
        for j in range(4):
            # each turn a little off the last one's plane: turns in one plane fight for the depth buffer
            BT.torus(m, (dx + j * 0.006, -0.06 - k * 0.025 - j * 0.005, -R + 0.02), R - j * 0.006, 0.009, slot, seg=18, rs=5, M=Mv)
    m.pipe([(0.0, -0.08, -0.36), (0.02, -0.09, -0.6), (-0.03, -0.1, -0.8)], 0.009, "SL_Rope", seg=5)
    return m


def prop_buoy_net():
    """A string net bag of training buoys hanging from a hook (origin on the wall at the hook, front = -Y)."""
    m = P("BuoyNet")
    m.tube((0, 0, 0), (0, -0.10, 0.0), 0.012, "SL_Galv", seg=6)
    c = Vector((0, -0.30, -0.55))
    for k, (dx, dy, dz, slot) in enumerate(((-0.12, 0.02, -0.15, "SL_Orange"), (0.13, -0.02, -0.12, "SL_Yellow"),
                                             (0.0, 0.05, 0.05, "SL_Orange"), (-0.05, -0.12, -0.30, "SL_Yellow"))):
        m.sphere(c + Vector((dx, dy, dz)), 0.16, slot, seg=12, rings=8)
    for k in range(10):
        a = 2 * math.pi * k / 10
        pts = [(0, -0.10, 0.0)]
        for j in range(1, 8):
            t = j / 7
            r = 0.30 * math.sin(math.pi * min(1.0, t * 1.1))
            pts.append((c.x + math.cos(a) * r, c.y + math.sin(a) * r * 0.8, 0.0 - 0.95 * t))
        m.pipe(pts, 0.004, "SL_Rope", seg=4)
    for z in (-0.25, -0.5, -0.75):
        rr = 0.30 * math.sin(math.pi * min(1.0, (-z / 0.95) * 1.1))
        ring_ = [(c.x + math.cos(2 * math.pi * k / 14) * rr, c.y + math.sin(2 * math.pi * k / 14) * rr * 0.8, z) for k in range(15)]
        m.pipe(ring_, 0.004, "SL_Rope", seg=4)
    return m


def prop_stove():
    """The hall's cast-iron stove (буржуйка) on legs with its flue up and back into the wall (origin on the floor, front
    = -Y; the flue enters the wall behind at +Y 0.45, z 2.6)."""
    m = P("Stove")
    for x in (-0.22, 0.22):
        for y in (-0.15, 0.15):
            m.bx("SL_Iron", x - 0.03, x + 0.03, y - 0.03, y + 0.03, 0.0, 0.18)
    m.bx("SL_Iron", -0.28, 0.28, -0.22, 0.22, 0.18, 0.78)
    m.bx("SL_Iron", -0.31, 0.31, -0.25, 0.25, 0.78, 0.83)
    m.bx("SL_Dark", -0.15, 0.15, -0.235, -0.22, 0.32, 0.58)
    m.bx("SL_Brass", 0.10, 0.14, -0.25, -0.235, 0.43, 0.47)
    m.pipe([(0.0, 0.05, 0.83), (0.0, 0.05, 2.45), (0.0, 0.20, 2.60), (0.0, 0.47, 2.60)], 0.065, "SL_Iron", seg=10)
    m.bx("SL_Galv", -0.40, 0.40, -0.40, 0.40, -0.001 + 0.0, 0.004)
    m.bx("SL_Galv", -0.30, 0.30, 0.36, 0.40, 0.2, 1.6)
    for k in range(5):
        m.bx("SL_WoodRaw", 0.42 + k * 0.012, 0.52 + k * 0.012, -0.2 + k * 0.08, -0.14 + k * 0.08, 0.0, 0.40 - k * 0.04)
    m.add_box_collision((0, 0, 0.42), (0.62, 0.5, 0.84))
    return m


def prop_oar_rack():
    """A wall rack of oars and paddles (origin on the wall at the floor, front = -Y)."""
    m = P("OarRack")
    for z in (0.9, 1.9):
        m.bx("SL_WoodRaw", -0.8, 0.8, -0.08, 0.0, z - 0.04, z + 0.04)
        for k in range(7):
            x = -0.66 + k * 0.22
            m.bx("SL_WoodRaw", x - 0.015, x + 0.015, -0.14, -0.08, z, z + 0.06)
    for k in range(6):
        x = -0.55 + k * 0.22
        slot = "SL_Wood" if k % 2 else "SL_FoilWood"
        m.tube((x, -0.11, 0.05), (x + 0.02, -0.11, 2.35), 0.018, slot, seg=8)
        m.box((x + 0.002, -0.11, 0.35), (0.14, 0.012, 0.55), "SL_Red" if k % 3 == 0 else slot)
    return m


def prop_life_ring():
    """A life ring on its bracket (origin on the wall at the bracket, front = -Y): red with white bands and the grab line."""
    m = P("LifeRing")
    m.bx("SL_Galv", -0.03, 0.03, -0.08, 0.0, 0.25, 0.30)
    Mv = Matrix(((1, 0, 0), (0, 0, 1), (0, -1, 0))).transposed()
    seg = 24
    R, r = 0.30, 0.055
    for k in range(seg):
        a0, a1 = 2 * math.pi * k / seg, 2 * math.pi * (k + 1) / seg
        slot = "SL_White" if (k // 3) % 2 else "SL_Red"
        pts = [Vector((0, -0.12, 0)) + Mv @ Vector((math.cos(a) * R, math.sin(a) * R, 0)) for a in (a0, a1)]
        m.pipe(pts, r, slot, seg=8, caps=False)
    for k in range(4):
        a0, a1 = math.pi / 2 * k + 0.4, math.pi / 2 * (k + 1) - 0.4
        p0 = Vector((0, -0.12, 0)) + Mv @ Vector((math.cos(a0) * (R + r), math.sin(a0) * (R + r), 0))
        p1 = Vector((0, -0.12, 0)) + Mv @ Vector((math.cos(a1) * (R + r), math.sin(a1) * (R + r), 0))
        mid = (p0 + p1) / 2 + (p0 + p1 - Vector((0, -0.24, 0))).normalized() * 0.05
        m.pipe([p0, mid, p1], 0.006, "SL_Rope", seg=4)
    return m


def prop_flagstaff():
    """The club's flagstaff on the roof (origin at its foot): a 5 m tapered white pole with a truck, its halyard to a
    cleat; the flag hoists at layout flag_points staff_top."""
    m = P("Flagstaff")
    m.bx("CT_Concrete", -0.25, 0.25, -0.25, 0.25, 0.0, 0.25)
    m.lathe([(0.0, 0.25), (0.06, 0.25), (0.035, 5.2), (0.0, 5.2)], "SL_White", seg=10)
    m.sphere((0, 0, 5.25), 0.06, "SL_Brass", seg=10, rings=6)
    m.pipe([(0.05, 0, 5.15), (0.06, 0, 1.0)], 0.004, "SL_Rope", seg=4)
    m.pipe([(-0.05, 0, 5.15), (-0.06, 0, 1.0)], 0.004, "SL_Rope", seg=4)
    BT.cleat(m, 0.065, 0, 0.95, yaw=math.pi / 2, L=0.10)
    m.add_box_collision((0, 0, 2.6), (0.15, 0.15, 5.2))
    return m


def prop_bike_rack():
    """A bike rack by the park gate (origin on the ground, front = -Y): galvanised hoops on a rail, two children's bikes
    are the engine's business."""
    m = P("BikeRack")
    m.bx("SL_Galv", -1.6, 1.6, -0.03, 0.03, 0.0, 0.06)
    for k in range(6):
        x = -1.35 + k * 0.54
        pts = [(x, -0.30, 0.0)] + [(x, -0.30 * math.cos(math.pi * j / 8), 0.42 + 0.30 * math.sin(math.pi * j / 8)) for j in range(9)] + [(x, 0.30, 0.0)]
        m.pipe(pts, 0.022, "SL_Galv", seg=6)
    m.add_box_collision((0, 0, 0.36), (3.2, 0.66, 0.72))
    return m


PROPS = [prop_opti_rack, prop_mast_bracket, prop_workbench, prop_tool_board, prop_paint_tins, prop_trestle, prop_hull_frame,
         prop_blackboard, prop_desk, prop_coach_desk, prop_chair, prop_knot_board, prop_flag_chart, prop_chart, prop_cup,
         prop_pennant, prop_trophy_shelf, prop_samovar_table, prop_wall_clock, prop_barometer, prop_anemometer, prop_radio,
         prop_bell, prop_locker, prop_bench, prop_lifejacket, prop_jacket_rail, prop_oar, prop_bailer, prop_shelf, prop_lamp,
         prop_slip_board, prop_signal_mast, prop_windsock_pole, prop_hose_stand, prop_outdoor_bench, prop_notice_board,
         prop_bollard, prop_winch, prop_park, prop_rope_coil, prop_buoy_net, prop_stove, prop_oar_rack, prop_life_ring,
         prop_flagstaff, prop_bike_rack]


# ================================================================================================= layout
def layout():
    """Where everything goes, building frame (see the module doc). yaw: degrees about +Z (Blender, right-handed); a prop's
    front is its -Y, so yaw 0 faces -Y (north), 90 faces +X (the sea), 180 faces +Y, -90 faces -X. Unreal yaw = -yaw."""
    bx_, bw, bh = BOAT_DOOR
    px, pw, ph = PEOPLE_DOOR
    items = []

    def put(mesh, x, y, z=0.0, yaw=0.0, room="hall", interactive=None, **kw):
        e = {"mesh": mesh, "pos": [round(x, 3), round(y, 3), round(z, 3)], "yaw": yaw, "room": room}
        if interactive:
            e["interactive"] = interactive
        e.update(kw)
        items.append(e)
    # ---- classroom (x -8.25 .. -3.03): blackboard on the partition wall facing west... on the landward gable
    cx0 = X0 + T
    put("SM_Sail_Prop_Blackboard", cx0 + 0.02, 0.0, 0.02, yaw=90, room="classroom", interactive="blackboard")
    put("SM_Sail_Prop_CoachDesk", cx0 + 1.1, -2.2, 0.02, yaw=90, room="classroom")
    put("SM_Sail_Prop_Chair", cx0 + 0.55, -2.2, 0.02, yaw=90, room="classroom", seat="coach")
    for r in range(3):
        for c in range(2):
            put("SM_Sail_Prop_Desk", cx0 + 2.0 + r * 0.95, -1.2 + c * 2.4, 0.02, yaw=-90, room="classroom", seats=2)
    put("SM_Sail_Prop_KnotBoard", CLASS_X - TP / 2 - 0.0, -1.5, 0.02, yaw=-90, room="classroom", interactive="knot_board")
    put("SM_Sail_Prop_FlagChart", cx0 + 0.02, 2.4, 0.02, yaw=90, room="classroom", interactive="flag_chart")
    put("SM_Sail_Prop_SeaChart", cx0 + 0.02, -2.6, 0.30, yaw=90, room="classroom")
    put("SM_Sail_Prop_TrophyShelf", -6.0, Y1 - T, 0.02, yaw=0, room="classroom", interactive="trophies")
    put("SM_Sail_Prop_KettleTable", CLASS_X - 0.6, Y1 - T - 0.45, 0.02, yaw=0, room="classroom", interactive="samovar")
    put("SM_Sail_Prop_WallClock", CLASS_X - TP / 2, 0.8, 2.45, yaw=-90, room="classroom")
    put("SM_Sail_Prop_Barometer", CLASS_X - TP / 2, 0.2, 1.65, yaw=-90, room="classroom", interactive="barometer")
    put("SM_Sail_Prop_AnemometerDisplay", CLASS_X - TP / 2, 0.75, 1.65, yaw=-90, room="classroom", interactive="wind_display")
    put("SM_Sail_Prop_Radio", CLASS_X - 0.6, Y1 - T - 0.45, 0.75, yaw=0, room="classroom", interactive="radio")
    put("SM_Sail_Prop_ShipBell", X0 + 2.5 + 0.9, Y0 + T, 1.7, yaw=180, room="classroom", interactive="bell")
    for k in range(4):
        put("SM_Sail_Prop_Photo%d" % k, -7.6 + k * 0.6 + (0.4 if k > 1 else 0), Y1 - T, 1.9 + (k % 2) * 0.1, yaw=0, room="classroom")
    for k, (x, z) in enumerate(((-7.75, 1.55), (-7.25, 1.75), (-6.75, 1.5))):
        put("SM_Sail_Prop_Drawing%d" % k, x, Y0 + T, z, yaw=180, room="classroom", note="a pupil's drawing")
    put("SM_Sail_Prop_Drawing3", -3.34, Y1 - T, 1.6, yaw=0, room="classroom", note="a pupil's drawing")
    for k in range(3):
        put("SM_Sail_Prop_Lamp", -6.9 + k * 1.6, 0.0, 3.2, room="classroom")
    # ---- boat hall (x -2.88 .. 5.18)
    hx0, hx1 = CLASS_X + TP / 2, WET_X - TP / 2
    racks = []
    for k in range(2):
        x = hx0 + 1.4 + k * 2.35
        put("SM_Sail_Prop_OptiRack", x, Y1 - T - 0.75, yaw=0)
        racks.append(x)
    boats = []
    for x in racks:
        for lvl, z in enumerate((0.43, 1.13, 1.83)):
            boats.append({"mesh": "SM_Sail_Optimist_Hull", "pos": [round(x, 3), round(Y1 - T - 0.75, 3), round(z + 0.33, 3)],
                          "rot_deg": [180, 0, 90], "note": "upside down across the rack (bow to +Y)", "tint_index": lvl + (0 if x == racks[0] else 3)})
    # luches and a cadet on trolleys in the hall (summer: rigged outside; winter: here under covers)
    tz = {"luch": 0.57, "cadet": 0.54}
    for k, (b, x, y) in enumerate((("luch", 1.5, -1.6), ("luch", 1.5, 0.6), ("cadet", -0.8, 2.1))):
        cn = BT.CLASS_NAME[b]
        put("SM_Sail_Trolley_" + cn, x, y, yaw=0, kind="trolley")
        boats.append({"mesh": "SM_Sail_%s_Hull" % cn, "pos": [x, y, None], "on_trolley": "SM_Sail_Trolley_" + cn,
                      "rot_deg": [0, 0, 0], "winter_cover": "SM_Sail_Cover_" + cn})
    # masts on wall brackets high on the classroom partition (above its door, clear of the boat door), lying along +Y;
    # sails hanging to dry from the timber rails
    for y in (-3.4, 1.4):
        for z in (2.55, 2.95):
            put("SM_Sail_Prop_MastBracket", hx0, y, z, yaw=90)
    spars = []
    for k, z in enumerate((2.55, 2.95)):
        for j in range(2):
            spars.append({"mesh": "SM_Sail_Luch_Mast" if k else "SM_Sail_Cadet_Mast",
                          "pos": [round(hx0 + 0.12 + j * 0.12, 3), -4.05, round(z - 0.11, 3)], "rot_deg": [-90, 0, 0],
                          "note": "lying along +Y on the brackets"})
    # life about the hall: coiled lines on pegs, a net of buoys, the stove, the oar rack, the chart of the bay
    put("SM_Sail_Prop_RopeCoil", 1.15, Y0 + T, 1.95, yaw=180)
    put("SM_Sail_Prop_RopeCoil", 1.65, Y0 + T, 1.95, yaw=180)
    put("SM_Sail_Prop_RopeCoil", hx1, -1.0, 1.95, yaw=-90)
    put("SM_Sail_Prop_BuoyNet", 3.3, Y1 - T, 2.3, yaw=0)
    put("SM_Sail_Prop_Stove", 2.55, Y1 - T - 0.47, 0.0, yaw=0, interactive="stove")
    put("SM_Sail_Prop_OarRack", hx0, -1.2, 0.0, yaw=90)
    put("SM_Sail_Prop_SeaChart", hx1, 0.9, 0.9, yaw=-90, interactive="chart", note="the chart of the bay")
    hang = []
    # sails drying on the tie beams: the luff along the beam (local z -> +y), the sail hanging down (local -x -> -z)
    eul = Matrix(((0, 1, 0), (0, 0, 1), (1, 0, 0))).to_euler("XYZ")
    rot = [round(math.degrees(a), 2) for a in eul]
    for mesh, x, y in (("SM_Sail_Cadet_Main", X0 + 3 * BAY + 0.10, -2.0), ("SM_Sail_Cadet_Jib", X0 + 4 * BAY + 0.10, -1.4)):
        hang.append({"mesh": mesh, "pos": [round(x, 3), y, WALL_H - 0.30], "rot_deg": rot,
                     "hang": "drying on the tie beam: luff along the beam (+y), the sail hanging down"})
    put("SM_Sail_Prop_Workbench", hx1 - 1.2, Y0 + T + 0.45, yaw=180, interactive="workbench")
    put("SM_Sail_Prop_ToolBoard", hx1 - 1.2, Y0 + T, 0.0, yaw=180, interactive="tools")
    put("SM_Sail_Prop_PaintTins", hx1 - 0.4, Y0 + T + 0.35, 0.0)
    for x in (2.6, 4.1):
        put("SM_Sail_Prop_Trestle", x, -0.4, yaw=90)
    put("SM_Sail_Prop_HullFrame", 3.35, -0.4, 0.74, yaw=0, interactive="boat_build", note="the cadet being built, on the trestles")
    put("SM_Sail_SailBag", hx1 - 0.6, 3.6, 0.0, yaw=90)
    for k in range(4):
        put("SM_Sail_Prop_Lamp", hx0 + 1.0 + k * 2.0, 0.0, 3.1)
    # ---- changing room (x 5.33..7.95, y -4.2..-0.06) and wet store (y 0.06..4.2)
    put("SM_Sail_Prop_Lockers", 6.6, Y0 + T + 0.25, yaw=180, room="changing", interactive="locker")
    put("SM_Sail_Prop_Lockers", X1 - T - 0.25, -2.4, yaw=-90, room="changing", interactive="locker")
    put("SM_Sail_Prop_Bench", 6.6, -2.4, yaw=0, room="changing")
    put("SM_Sail_Prop_JacketRail", 6.6, -0.08, 0.0, yaw=0, room="changing", interactive="life_jacket")
    put("SM_Sail_Prop_Lamp", 6.6, -2.1, 2.7, room="changing")
    put("SM_Sail_Prop_Shelf", 6.6, Y1 - T - 0.25, yaw=0, room="wet_store")
    put("SM_Sail_Prop_Shelf", X1 - T - 0.25, 2.0, yaw=-90, room="wet_store")
    for k in range(4):
        put("SM_Sail_Prop_Paddle", 5.6 + k * 0.1, 0.25, 0.05, yaw=0, room="wet_store", rot_deg=[0, -80, 90])
    put("SM_Sail_Prop_Bailer", 6.0, 2.0, 0.0, room="wet_store")
    put("SM_Sail_Buoy", 6.9, 1.5, 0.22, room="wet_store")
    put("SM_Sail_MOB", 7.4, 1.0, 0.07, room="wet_store")
    put("SM_Sail_Prop_Lamp", 6.6, 2.1, 2.7, room="wet_store")
    # ---- the loft over the small rooms: sail bags
    for k in range(5):
        put("SM_Sail_SailBag", 6.6, -3.6 + k * 0.6 + (2.0 if k > 2 else 0), 2.80, yaw=0, room="loft")
    # ---- outside (sand height: ground(x)); the park on the north side, the slip down from its gate
    out = []

    def ext(mesh, x, y, z=None, yaw=0.0, **kw):
        e = {"mesh": mesh, "pos": [round(x, 3), round(y, 3), round(ground(x) if z is None else z, 3)], "yaw": yaw}
        e.update(kw)
        out.append(e)
    # park slab: its landward corner by the boathouse: x from -6.5 (15 m toward the sea ends at 8.5), aisle 3.6 m wide
    park_x0 = X1 - 15.0 + 0.25
    ext("SM_Sail_Prop_Park", park_x0, Y0 - 3.6, z=ground(park_x0 + 7.5), note="slab origin (landward corner at the aisle)")
    for k in range(6):
        b = ("optimist", "optimist", "luch", "luch", "cadet", "optimist")[k]
        cn = BT.CLASS_NAME[b]
        x = park_x0 + 1.25 + k * 2.5
        ext("SM_Sail_Trolley_" + cn, x, Y0 - 3.6 - 9.0, z=ground(park_x0 + 7.5), yaw=90, kind="trolley", boat=b,
            note="bow to the north (-Y), in a bay")
    gx = park_x0 + 15.0
    for k in range(6):
        ext("SM_Sail_Prop_SlipBoard", gx + 0.6 + k * 1.2, Y0 - 3.6 - 1.8, note="first of 19 sections down to 22.5 m; repeat +1.2 m in x at ground(x)")
    ext("SM_Sail_Prop_SignalMast", gx + 1.0, Y0 - 3.6 - 5.0, note="10 m; flags hoist at the yard ends (y +-1.5, z 7.5) and the top")
    xr = X1 + OVER - 0.5
    ext("SM_Sail_Prop_WindsockPole", xr, Y1 + 0.25, z=zu(xr) + ROOF_T,
        note="on the roof's seaward south corner; SM_Sail_Windsock at z +2.0 of it")
    ext("SM_Sail_Prop_Flagstaff", xr - 0.45, Y0 - 0.08, z=zu(xr - 0.45) + ROOF_T, note="on the roof's seaward north corner; flag at flag_points staff_top")
    ext("SM_Sail_Prop_HoseStand", bx_ + 2.6, Y0 - 1.2)
    ext("SM_Sail_Prop_HoseStand", gx - 0.6, Y0 - 3.6 - 3.4, z=ground(park_x0 + 7.5))
    ext("SM_Sail_Prop_LifeRing", X1, -2.6, z=1.55, yaw=90)
    ext("SM_Sail_Prop_LifeRing", X1 + BALC[1] - 0.05, 2.2, z=BALC[0] + 0.75, yaw=90)
    ext("SM_Sail_Prop_LifeRing", 3.0, STAIR_Y[1] + 0.02, z=ground(3.0) + 1.3, yaw=180)
    ext("SM_Sail_Prop_BikeRack", X0 - 1.5, -2.2, yaw=-90)
    ext("SM_Sail_Prop_OutdoorBench", 3.7, Y0 - 0.45, yaw=0, note="parents wait here, looking at the park")
    # spars resting outside on a rack at the park's head (masts along +X)
    zs_ = ground(park_x0 + 7.5)
    rx_, ry_ = park_x0 + 3.2, Y0 - 3.6 - 4.2
    ext("SM_Sail_SparsRack", rx_, ry_, z=zs_, yaw=180)
    for k, (mesh, zz) in enumerate((("SM_Sail_Luch_Mast", 0.665), ("SM_Sail_Cadet_Mast", 1.165), ("SM_Sail_Luch_Mast", 1.665))):
        ext(mesh, rx_ - 2.4, ry_ + 0.2, z=zs_ + zz, rot_deg=[0, 90, 0], note="lying along +X on the spars rack")
    ext("SM_Sail_Prop_NoticeBoard", px + 1.8, Y0 - 0.9, yaw=0, interactive="timetable")
    ext("SM_Sail_Prop_OutdoorBench", X1 + 2.0, -2.0, yaw=90)
    ext("SM_Sail_Prop_OutdoorBench", X1 + 2.0, 2.0, yaw=90)
    for k in range(3):
        ext("SM_Sail_Prop_Bollard", gx + 0.3, Y0 - 3.6 - 0.4 - k * 1.5)
    ext("SM_Sail_Prop_Winch", gx + 0.6, Y0 - 3.6 + 0.3, yaw=90)
    flags_at = {"yard_port": [gx + 1.0, Y0 - 3.6 - 5.0 + 1.5, 7.5], "yard_stbd": [gx + 1.0, Y0 - 3.6 - 5.0 - 1.5, 7.5],
                "top": [gx + 1.0, Y0 - 3.6 - 5.0, 9.9],
                "staff_top": [round(xr - 0.40, 3), round(Y0 - 0.08, 3), round(zu(xr - 0.45) + ROOF_T + 5.1, 3)]}
    doors = [
        {"name": "people", "at": [px, Y0 + T / 2, 0.0], "out": [0, 1], "hole": [pw, ph], "wall": T, "leaves": 1, "hinge": "left",
         "frame": True, "open": False, "locked": False, "outside": True},
        {"name": "class_hall", "at": [CLASS_X, 2.5, 0.0], "out": [1, 0], "hole": [1.2, 2.18], "wall": TP, "leaves": 1,
         "hinge": "right", "frame": True, "open": True, "locked": False},
        {"name": "changing", "at": [WET_X, -2.25, 0.0], "out": [1, 0], "hole": [1.2, 2.18], "wall": TP, "leaves": 1,
         "hinge": "left", "frame": True, "open": False, "locked": False},
        {"name": "wet_store", "at": [WET_X, 2.25, 0.0], "out": [1, 0], "hole": [1.2, 2.18], "wall": TP, "leaves": 1,
         "hinge": "right", "frame": True, "open": False, "locked": False},
        {"name": "balcony", "at": [X1 - T / 2, 0.0, BALC[0]], "out": [1, 0], "hole": [1.1, 2.1], "wall": T, "leaves": 1,
         "hinge": "left", "frame": True, "open": False, "locked": False, "outside": True,
         "note": "the loft's glazed door onto the starter's balcony (in the seaward glazed wall)"},
    ]
    sliding = [{"name": "boat_door", "mesh": "SM_Sail_Boathouse_DoorLeaf", "leaves": [
        {"closed": [round(bx_ - bw / 4 - 0.025, 3), round(Y0 - 0.20, 3), 0.0], "open_dx": round(-(bw / 2 + 0.05), 3)},
        {"closed": [round(bx_ + bw / 4 + 0.025, 3), round(Y0 - 0.20, 3), 0.0], "open_dx": round(bw / 2 + 0.05, 3)}],
        "hole": [bw, bh], "note": "leaves hang on the outside rail and slide apart along x"}]
    lamps = [{"pos": e["pos"][:2] + [e["pos"][2] - 1.0], "room": e["room"], "lumens": 1400 if e["room"] == "hall" else 900}
             for e in items if e["mesh"] == "SM_Sail_Prop_Lamp"]
    for p in ((bx_, Y0 - 0.14, bh + 0.55), (px, Y0 - 0.45, ph + 0.21), (X1 + 0.06, 0.95, BALC[0] + 2.35)):
        lamps.append({"pos": [round(v, 3) for v in p], "room": "outside", "lumens": 700, "note": "bulkhead lamp in the shell mesh"})
    rooms = {"classroom": [[X0 + T, Y0 + T, 0], [CLASS_X - TP / 2, Y1 - T, 3.2]],
             "balcony": [[X1 - T, Y0 - 0.1, BALC[0]], [X1 + BALC[1], STAIR_Y[1], BALC[0] + 2.4]],
             "hall": [[CLASS_X + TP / 2, Y0 + T, 0], [WET_X - TP / 2, Y1 - T, RIDGE]],
             "changing": [[WET_X + TP / 2, Y0 + T, 0], [X1 - T, -TP / 2, 2.7]],
             "wet_store": [[WET_X + TP / 2, TP / 2, 0], [X1 - T, Y1 - T, 2.7]],
             "loft": [[WET_X - TP / 2, Y0 + T, 2.8], [X1 - T, Y1 - T, RIDGE]]}
    spots = [{"id": "coach_board", "kind": "teach", "pos": [cx0 + 0.6, 0.6, 0.02], "yaw": 90},
             {"id": "bench_work", "kind": "work", "pos": [hx1 - 1.2, Y0 + T + 1.1, 0.0], "yaw": 180},
             {"id": "boat_build", "kind": "work", "pos": [3.35, 0.7, 0.0], "yaw": 180},
             {"id": "changing", "kind": "change", "pos": [6.6, -1.8, 0.0], "yaw": 0},
             {"id": "kettle", "kind": "tea", "pos": [CLASS_X - 0.6, Y1 - T - 1.1, 0.02], "yaw": 0},
             {"id": "deck_view", "kind": "watch", "pos": [round(X1 + 1.0, 3), -1.6, BALC[0]], "yaw": 90,
              "note": "the starter's balcony: the hero stands here to watch the races (the course is out to +x)"},
             {"id": "stair_foot", "kind": "path", "pos": [round(X1 - T - 16 * 0.28 - 0.4, 3), round((STAIR_Y[0] + STAIR_Y[1]) / 2, 3),
                                                          round(ground(X1 - T - 16 * 0.28 - 0.4), 3)], "yaw": 90}]
    for e in items:
        if e["mesh"] == "SM_Sail_Prop_Desk":
            for k in (-0.3, 0.3):
                spots.append({"id": "desk_%d" % len(spots), "kind": "pupil", "pos": [e["pos"][0] + 0.45, e["pos"][1] + k, 0.02], "yaw": 90,
                              "seat_z": 0.45})
    return {"_doc": ("Sailing-school boathouse layout (blender/sailing/boathouse.py). Building frame: metres, origin on the "
                     "floor in the middle of the 16.5 x 9 m footprint, +X to the sea, -Y the north (door) wall, +Z up; Unreal "
                     "X = x, Y = -y, Z = z (cm x100), yaw_unreal = -yaw. Props: mesh, pos, yaw (deg about +Z; a prop's "
                     "front is its -Y), room, interactive (a kind for the engine). boats: hull meshes placed on racks / "
                     "trolleys (rot_deg = Blender XYZ Euler degrees; pos z None = on its trolley's boat_origin). doors: "
                     "place_doors.py entries. sliding_doors: the boat door's two leaves (closed position, open offset in x). "
                     "outside: pos z = the sand (ground(x) = -0.0727 (x + 0.85)) unless given."),
            "footprint": {"x": [X0, X1], "y": [Y0, Y1], "wall_h": WALL_H, "ridge": RIDGE, "floor_z": 0.0,
                          "ground_slope": "-0.0727 per m toward +x, 0 at x = %.2f" % BOAT_DOOR[0],
                          "roof": {"kind": "mono-pitch rising to the sea", "soffit_z": [ROOF_LO, RIDGE], "slab": ROOF_T,
                                   "x": [round(X0 - 0.5, 3), round(X1 + OVER, 3)], "y": [round(Y0 - 0.5, 3), round(Y1 + 0.6, 3)]},
                          "balcony": {"floor_z": BALC[0], "x": [round(X1 - T, 3), round(X1 + BALC[1], 3)],
                                      "stair": {"y": list(STAIR_Y), "x": [round(X1 - T - 16 * 0.28, 3), round(X1 - T, 3)]}}},
            "props": items, "boats": boats, "spars": spars, "hanging_sails": hang, "doors": doors,
            "sliding_doors": sliding, "lamps": lamps, "rooms": rooms, "spots": spots, "outside": out, "flag_points": flags_at}


def build(reg):
    sh, gl = shell()
    reg.add(sh, KIT, collision="exact", closed=False)
    reg.add(gl, KIT, collision="none", closed=False)
    reg.add(door_leaf(), KIT, collision="box", closed=False)
    for fn in PROPS:
        m = fn()
        reg.add(m, KIT, collision="box" if m.collision else "none", closed=False)
    for k in range(1, 4):
        reg.add(prop_photo(k), KIT, closed=False)
    reg.objects  # noqa: B018
    reg.add(prop_photo(0), KIT, closed=False)
    for k in range(4):
        reg.add(prop_drawing(k), KIT, closed=False)
    reg.add(prop_lifejacket_worn(), KIT, closed=False)
    reg.add(prop_windsock(), KIT, closed=False)
    with open(os.path.join(HERE, "atlas_cells.json")) as f:
        fids = json.load(f)["flag_ids"]
    for fid in fids:
        reg.add(flag_mesh(fid), KIT, closed=False)
    lay = layout()
    with open(os.path.join(HERE, "boathouse_layout.json"), "w") as f:
        f.write(B.compact_json(lay) + "\n")
    return lay
