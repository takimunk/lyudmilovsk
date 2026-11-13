"""Lifeguard tower: a tall white concrete stem on a dark plinth carries a big cantilevered cabin deck.

Original design in the spirit of 1960s-70s Mediterranean / Baltic-resort concrete lifeguard and
watch towers: a slender 1.2 m square stem with red paint bands, four concrete corbels under a 3.8 m square
deck (railing all round), a glazed 2.6 m cabin with ribbon windows on all four sides (kind "public"),
a red pyramid roof, a ring-buoy on the front wall, a flag pole at the deck corner and a steel ladder
up the stem's front that enters the cabin through a floor hatch.

Plot 4 x 4 m, height 9.0 m. Deck at 5.6 m. Enterable: only the cabin (reached by the ladder; the door
hole in the back wall opens onto the deck).
Spots: "view" in the cabin looking to the street / the sea (yaw 0).
No flats, no entrances (the cabin door is a plain hole, 0.9 x 2.0 m).
Ideas for later: a rescue-boat rack at the foot, a siren horn, a risk-flag halyard animation.
Requests: none.
"""
import math
import os

from citykit import box, prism
import geom
import furn_int3 as fu
import interior_kit as kit
from furn_int3 import Fr, lbox, cyl

INFO = {
    "name_ru": "Спасательная вышка", "name_en": "Lifeguard Tower",
    "style": "resort modernism", "use": "public",
    "inspired_by": "concrete beach watch towers of the 1960s-70s Mediterranean and Baltic resorts",
}

CX, CY = 0.0, 2.0          # stem centre
STEM = 0.6                 # half width
DECK_Z0, DECK_Z1 = 5.4, 5.6
WALL_Z1 = 7.9


def _xz(m, mat, poly, y0, y1):
    """Polygon in (x, z) extruded over y0 < y1."""
    geom.extrude_profile(m, mat, mat, poly, -y1, -y0, lambda u, v, t: (u, -t, v))


def _yz(m, mat, poly, x0, x1):
    geom.extrude_profile(m, mat, mat, poly, x0, x1, lambda u, v, t: (t, u, v))


def build(B):
    m, d = B.m, B.detail
    # ---- plinth, stem, red paint bands
    box(m, "CT_ConcreteDark", CX - 1.0, CX + 1.0, CY - 1.0, CY + 1.0, 0.0, 0.5)
    box(m, "CT_ConcreteWhite", CX - STEM, CX + STEM, CY - STEM, CY + STEM, 0.5, DECK_Z0, faces="xXyYZ")
    for z0, z1 in ((1.5, 2.0), (3.0, 3.5), (4.5, 5.0)):
        box(m, "CT_PlasterRed", CX - STEM - 0.02, CX + STEM + 0.02, CY - STEM - 0.02, CY + STEM + 0.02, z0, z1,
            faces="xXyY")
    # ---- corbels under the deck
    for sgn in (-1, 1):
        _xz(m, "CT_ConcreteWhite", [(sgn * STEM, 4.0), (sgn * 1.5, DECK_Z0), (sgn * STEM, DECK_Z0)][::sgn], CY - 0.2, CY + 0.2)
        _yz(m, "CT_ConcreteWhite", [(CY + sgn * STEM, 4.0), (CY + sgn * 1.5, DECK_Z0), (CY + sgn * STEM, DECK_Z0)][::sgn], -0.2, 0.2)
    # ---- deck (CT_Concrete) with a hatch for the ladder: x -0.4..0.4, y 1.2..1.9
    hx0, hx1, hy0, hy1 = -0.4, 0.4, 1.15, 1.88
    X0, X1, Y0, Y1 = -1.9, 1.9, 0.1, 3.9
    box(m, "CT_Concrete", X0, hx0, Y0, Y1, DECK_Z0, DECK_Z1)
    box(m, "CT_Concrete", hx1, X1, Y0, Y1, DECK_Z0, DECK_Z1)
    box(m, "CT_Concrete", hx0, hx1, Y0, hy0, DECK_Z0, DECK_Z1)
    box(m, "CT_Concrete", hx0, hx1, hy1, Y1, DECK_Z0, DECK_Z1)
    # ---- cabin: 2.6 m square, ribbon windows
    H = WALL_Z1
    sill, head = 6.5, 7.55
    def wall(p0, p1, holes):
        F = B.facade(p0, p1, DECK_Z1, H, "CT_Plaster", holes=holes, depth=0.15, reveal_mat="CT_FrameWhite")
        return F
    # front, faces -y (street / sea)
    holes = [(0.15, 1.2, sill, head), (1.4, 2.45, sill, head)]
    F = wall((-1.3, 0.7), (1.3, 0.7), holes)
    for u0, u1, *_ in holes:
        B.facade_window(F, u0, u1, sill, head, kind="public", cols=2, inset=0.1)
    holes = [(0.15, 2.45, sill, head)]
    F = wall((1.3, 0.7), (1.3, 3.3), holes)
    B.facade_window(F, 0.15, 2.45, sill, head, kind="public", cols=3, inset=0.1)
    # back: window, door, window
    holes = [(0.1, 0.8, sill, head), (1.0, 1.9, DECK_Z1, DECK_Z1 + 2.0), (2.05, 2.5, sill, head)]
    F = wall((1.3, 3.3), (-1.3, 3.3), holes)
    B.facade_window(F, 0.1, 0.8, sill, head, kind="public", inset=0.1)
    B.facade_window(F, 2.05, 2.5, sill, head, kind="public", inset=0.1)
    holes = [(0.15, 2.45, sill, head)]
    F = wall((-1.3, 3.3), (-1.3, 0.7), holes)
    B.facade_window(F, 0.15, 2.45, sill, head, kind="public", cols=3, inset=0.1)
    # ---- roof: eave slab + red pyramid
    box(m, "CT_ConcreteWhite", -1.8, 1.8, 0.2, 3.8, WALL_Z1, WALL_Z1 + 0.15)
    z0, za = WALL_Z1 + 0.15, 8.6
    a, b, c_, d_ = (-1.8, 0.2), (1.8, 0.2), (1.8, 3.8), (-1.8, 3.8)
    for p, q in ((a, b), (b, c_), (c_, d_), (d_, a)):
        m.face("CT_SteelRed", [(p[0], p[1], z0), (q[0], q[1], z0), (0.0, 2.0, za)])
    # ---- ring buoy on the front wall (alternating red / white segments)
    cxb, czb, y = 0.0, 6.05, 0.7 - 0.03
    for k in range(12):
        a0, a1 = 2 * math.pi * k / 12, 2 * math.pi * (k + 1) / 12
        P = lambda r, ang: (cxb + r * math.cos(ang), y, czb + r * math.sin(ang))
        d.face("CT_SteelRed" if k % 2 == 0 else "CT_SteelWhite", [P(0.15, a0), P(0.32, a0), P(0.32, a1), P(0.15, a1)])
    # ---- deck railing (closed loop, gap at the corner of the flag pole is fine)
    B.railing([(X0 + 0.05, Y0 + 0.05), (X1 - 0.05, Y0 + 0.05), (X1 - 0.05, Y1 - 0.05), (X0 + 0.05, Y1 - 0.05), (X0 + 0.05, Y0 + 0.05)],
              DECK_Z1, h=1.05, post=1.1)
    # ---- flag pole at the front-right deck corner with a flag
    px, py = 1.82, 0.16
    box(d, "CT_SteelWhite", px - 0.03, px + 0.03, py - 0.03, py + 0.03, DECK_Z1, 9.0)
    box(d, "CT_SteelRed", px - 0.95, px, py - 0.01, py + 0.01, 8.35, 8.65)
    box(d, "CT_SteelWhite", px - 0.95, px, py - 0.01, py + 0.01, 8.65, 8.95)
    # ---- steel ladder up the stem's front face (y = CY - STEM) from ground to the hatch
    ly = CY - STEM - 0.07
    for xx in (-0.25, 0.25):
        box(d, "CT_Steel", xx - 0.02, xx + 0.02, ly - 0.02, ly + 0.02, 0.5, DECK_Z1 + 0.9)
    z = 0.8
    while z < DECK_Z1 + 0.8:
        box(d, "CT_Steel", -0.25, 0.25, ly - 0.015, ly + 0.015, z, z + 0.03)
        z += 0.3
    # ---- the cabin's furniture (inner floor z = DECK_Z1; the hatch x -0.4..0.4, y 1.15..1.88 and the path from it to
    # the back door (x -0.6..0.3, y 3.3) stay clear)
    z0 = DECK_Z1
    _cabin(B, z0)
    # binoculars on a tripod by the front window
    bx, by = -0.75, 1.2
    top = z0 + 1.15
    for k in range(3):
        a = math.radians(90 + 120 * k)
        fu.rod(d, "CT_Steel", (bx, by, top), (bx + 0.28 * math.cos(a), by + 0.28 * math.sin(a), z0), 0.025)
    box(d, "CT_Steel", bx - 0.02, bx + 0.02, by - 0.02, by + 0.02, top, top + 0.1)
    box(d, "CT_ConcreteDark", bx - 0.12, bx + 0.12, by - 0.16, by + 0.06, top + 0.1, top + 0.22, faces="xXyYZ")
    for dx in (-0.06, 0.06):
        cyl(d, "CT_Steel", bx + dx, by - 0.2, top + 0.13, top + 0.2, 0.045, 6)
    B.spot("binoculars", "stand", (bx, by + 0.45, z0), 0.0)
    B.thing("binoculars", "machine", (bx, by - 0.1, top + 0.15), 0.0, what="binoculars")
    B.lamp((0.0, 2.2, z0 + 2.2), lumens=450, colour="#FFE8C8", kind="public", radius=4.5)
    box(d, "CT_Steel", -0.01, 0.01, 2.19, 2.21, z0 + 2.28, WALL_Z1, faces="xXyY")
    B.entrance("ladder", (0.0, ly - 0.05, 0.5), 0.0, width=0.5, height=2.0, kind="service")
    B.spot("view", "view", (0.0, 2.6, DECK_Z1), 0.0)
    B.spot("chair", "seat", (-0.85, 2.1, z0), 270.0)
    B.thing("view_cabin", "board", (0.0, 1.0, z0 + 1.5), 0.0, topic="view")
    B.footprint = [(-2.0, 0.0), (2.0, 0.0), (2.0, 4.0), (-2.0, 4.0)]
    B.height = 9.0
    if os.environ.get("KIT_CHECK"):
        print("KITCHECK", kit.check(B))


def _pennants(d, z, y, x0, x1, n, sag=0.09, drop=0.26):
    cols = ["CT_SteelRed", "CT_PlasterYellow", "CT_TileWhite", "CT_PlasterBlue", "CT_PlasterGreen"]
    w = (x1 - x0) / n
    prev = None
    for i in range(n):
        xa = x0 + i * w
        za = z - sag * math.sin(math.pi * (i + 0.5) / n)
        tri = [(xa + 0.02, y, za), (xa + w - 0.02, y, za), (xa + w / 2, y, za - drop)]
        d.face(cols[i % 5], [(px, py + 0.004, pz) for px, py, pz in tri])
        d.face(cols[i % 5], [(px, py - 0.004, pz) for px, py, pz in tri[::-1]])
        pt = (xa + w / 2, y, za)
        if prev:
            fu.rod(d, "CT_Steel", prev, pt, 0.008, caps=False)
        prev = pt


def _cabin(B, z0):
    """Inside the cabin (inner x -1.15..1.15, y 0.85..3.15; the hatch x +-0.4, y 1.15..1.88 and the way to the back
    door x -0.6..0.3 stay clear) and on the deck."""
    d = B.detail
    # ---- the logbook desk along the right wall
    dk = kit.office_desk(B, 0.87, 2.45, z0, 270.0, w=1.4, d=0.55, typewriter_on=False, papers=False, phone=False)
    top = dk["top"]
    kit.telephone(B, 0.9, 1.95, top, 250)
    box(d, "CT_PlasterBlue", 0.7, 1.0, 2.25, 2.5, top, top + 0.025, faces="xXyYZ")                      # the log
    box(d, "CT_TileWhite", 0.74, 0.96, 2.28, 2.46, top + 0.025, top + 0.027, faces="Z")
    kit.table_lamp(B, 1.0, 2.7, top, 0.0)
    kit.radio(B, 0.95, 2.98, top, 270.0)
    kit.cup(B, 0.75, 2.62, top, 0.0)
    B.thing("log_book", "board", (0.85, 2.38, top + 0.05), 270.0, topic="view")
    # ---- the lookout's chair at the left window and a small table with the first-aid kit and a flask
    kit.rug(B, -0.75, 2.25, z0, 90.0, w=1.0, d=1.5, name="rug_5", seed=1)
    ch = kit.chair(B, -0.82, 2.1, z0, 270.0, kind="wood", mat="CT_WoodLight")
    B.thing("lookout_chair", "seat", (-0.82, 2.1, z0 + 0.45), 270.0, height=0.45)
    tb = kit.table(B, -0.92, 2.95, z0, 90.0, w=0.5, d=0.45, h=0.75, kind="steel")
    tt = tb["top"]
    box(d, "CT_TileWhite", -1.05, -0.72, 2.8, 3.1, tt, tt + 0.22, faces="xXyYZ")                       # first-aid kit
    box(d, "CT_SteelRed", -0.72, -0.716, 2.9, 3.0, tt + 0.08, tt + 0.14, faces="X")
    box(d, "CT_SteelRed", -0.72, -0.716, 2.94, 2.96, tt + 0.05, tt + 0.17, faces="X")
    cyl(d, "CT_Steel", -1.0, 2.7, tt, tt + 0.2, 0.04, 6)                                               # flask
    B.thing("first_aid", "board", (-0.75, 2.95, tt + 0.12), 90.0, topic="health")
    # ---- a stool by the hatch, a stack of cork life belts, the clock over the door, pennants along the ceiling
    kit.stool(B, 0.72, 1.5, z0, 0.0)
    kit.wall_clock(B, -0.15, 3.15, z0, 0.0, zc=2.15, face="clock_3")
    _pennants(d, z0 + 2.2, 1.0, -1.1, 1.1, 8)
    kit.plant(B, 1.0, 1.05, z0, 0.0, h=0.7, kind="cactus", pot="CT_PlasterRed")
    # ---- the deck: a telescope at the front rail, the flags board and a rescue tube on the rail, a bench at the back
    ts = kit.telescope(B, 1.05, 0.5, z0, 0.0)
    B.thing("deck_telescope", "machine", (1.05, 0.55, z0 + 1.2), 0.0, what="telescope")
    for sx in (-1.5, -0.5):
        box(d, "CT_Steel", sx - 0.03, sx + 0.03, 0.2, 0.26, z0, z0 + 1.35, faces="xXyYZ")
    box(d, "CT_SteelWhite", -1.58, -0.42, 0.2, 0.25, z0 + 0.55, z0 + 1.28, faces="xXyYZz")
    kit.poster(B, -1.0, 0.25, z0, 180.0, name="poster_safety", w=0.62, zc=0.95)
    B.thing("flags_board", "board", (-1.0, 0.3, z0 + 0.95), 180.0, topic="rules")
    fu.rod(d, "CT_SteelRed", (1.2, 0.18, z0 + 0.75), (1.75, 0.18, z0 + 0.75), 0.11)                       # rescue tube
    fu.rod(d, "CT_SteelWhite", (1.43, 0.18, z0 + 0.75), (1.52, 0.18, z0 + 0.75), 0.115)
    kit.bench(B, 1.25, 3.62, z0, 180.0, w=1.1, kind="gym")
    B.thing("deck_bench", "seat", (1.25, 3.62, z0 + 0.45), 180.0, height=0.45)
