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
Second pass (depth): at the foot an upturned rowboat, an oars rack, a rope coil and the water-temperature board;
on the deck a deck chair, the flag halyard, the lost-and-found board with its box and a lantern over the door; in the
cabin a shelf with a megaphone and a transistor, a whistle on its hook, the tide table. Things in `_extras`.
Third pass (2026-11, depth): at the foot a gear locker (rescue torpedo, throw line, aid kit) with a stand spot, a yellow
spine board on the stem, a practice ring post for the ring-throw game, a three-colour flag mast with a pointer, the tide
staff; in the cabin a lamp switch, a clock and the duty ledger (a text that grows by game day, one per tower); on the
deck a ship's bell and a west-facing coin binocular; staff spots. Things lg*/gear_locker etc., words in strings/x_p_lifeguard.json.
Ideas for later: a siren horn, a risk-flag halyard animation.
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
    _extras(B, z0)
    _third(B, z0)
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
    B.thing("cabin_radio", "machine", (0.95, 2.98, top + 0.14), 270.0, what="radio")
    # ---- the lookout's chair at the left window and a small table with the first-aid kit and a flask
    kit.rug(B, -0.7, 2.45, z0, 0.0, w=0.8, d=1.0, name="rug_5", seed=1)      # QA: was 1.5 m long and poked through the west wall
    ch = kit.chair(B, -0.82, 2.1, z0, 270.0, kind="wood", mat="CT_WoodLight")
    B.thing("lookout_chair", "seat", (-0.82, 2.1, z0 + 0.45), 270.0, height=0.45)
    tb = kit.table(B, -0.92, 2.95, z0, 90.0, w=0.5, d=0.45, h=0.75, kind="steel")
    tt = tb["top"]
    box(d, "CT_TileWhite", -1.05, -0.72, 2.8, 3.1, tt, tt + 0.22, faces="xXyYZ")                       # first-aid kit
    box(d, "CT_SteelRed", -0.72, -0.716, 2.9, 3.0, tt + 0.08, tt + 0.14, faces="X")
    box(d, "CT_SteelRed", -0.72, -0.716, 2.94, 2.96, tt + 0.05, tt + 0.17, faces="X")
    cyl(d, "CT_Steel", -1.0, 2.7, tt, tt + 0.2, 0.04, 6)                                               # flask
    B.thing("first_aid", "board", (-0.75, 2.95, tt + 0.12), 90.0, topic="health")
    B.thing("flask_tea", "machine", (-1.0, 2.7, tt + 0.12), 90.0, what="tea_flask")
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


def _boat(B, x, y, yaw):
    """An upturned rowing boat on the sand (0.84 x 2.3 x 0.44): hull loft, white gunwale band, keel strip."""
    p = kit._P(B, x, y, 0.0, yaw, "rowboat")
    ring = kit._ring_circle
    rings = [ring(0, 0, 1.15, 0.0, 14, rx=0.42, a0=0.0), ring(0, 0, 1.12, 0.12, 14, rx=0.40, a0=0.0),
             ring(0, 0, 0.95, 0.27, 14, rx=0.30, a0=0.0), ring(0, 0, 0.68, 0.40, 14, rx=0.12, a0=0.0)]
    p.loft("CT_PlasterBlue", rings, top=True)
    p.loft("CT_TileWhite", [ring(0, 0, 1.17, 0.0, 14, rx=0.44, a0=0.0), ring(0, 0, 1.17, 0.07, 14, rx=0.44, a0=0.0)],
           top=False)
    p.box("CT_WoodLight", -0.025, 0.025, -0.62, 0.62, 0.395, 0.44)
    p.box("CT_WoodLight", -0.2, 0.2, -0.04, 0.04, 0.0, 0.03)
    return p.done(solid=False)


def _oars(B, x, y, z):
    """Two oars leaning on the stem's left face, blades down (the stem face is at local x +0.18)."""
    p = kit._P(B, x, y, z, 0.0, "oars")
    for dy, lean in ((-0.13, 0.15), (0.13, 0.19)):
        a, b = (-0.05, dy, 0.0), (lean, dy, 1.9)
        p.rod("CT_WoodLight", a, b, 0.035, sides=6)
        k = 0.5 / 1.9
        c = (a[0] + (b[0] - a[0]) * k, dy, 0.5)
        for sgn in (1, -1):
            q = [(a[0], dy - sgn * 0.075, 0.0), (a[0], dy + sgn * 0.075, 0.0), (c[0], dy + sgn * 0.075, 0.5),
                 (c[0], dy - sgn * 0.075, 0.5)]
            p.face("CT_WoodLight", [(qx + sgn * 0.012, qy, qz) for qx, qy, qz in q][::sgn])
        p.rod("CT_SteelWhite", (a[0] + (b[0] - a[0]) * 0.8, dy, 1.52), (a[0] + (b[0] - a[0]) * 0.86, dy, 1.63), 0.05)
    return p.done(solid=False, footprint=(-0.12, 0.0, -0.3, 0.3))     # the feet only: the leaning tops are in the stem's zone


def _megaphone(B, x, y, z):
    p = kit._P(B, x, y, z, 0.0, "megaphone")
    p.lathe("CT_SteelWhite", 0, 0, [(0.03, 0.0), (0.04, 0.12), (0.115, 0.30)], segs=10)
    p.lathe("CT_SteelRed", 0, 0, [(0.116, 0.26), (0.118, 0.30)], segs=10)
    p.box("CT_Steel", -0.04, 0.04, -0.05, 0.05, 0.0, 0.04)
    return p.done(solid=False)


def _transistor(B, x, y, z, yaw):
    p = kit._P(B, x, y, z, yaw, "transistor")
    p.bev("CT_PlasterYellow", -0.09, 0.09, -0.035, 0.035, 0.0, 0.12, 0.01)
    p.box("CT_Steel", -0.07, 0.07, -0.037, -0.034, 0.05, 0.10, f="Y")
    p.box("CT_TileWhite", -0.03, 0.03, -0.037, -0.035, 0.07, 0.085, f="Y")
    p.rod("CT_Steel", (0.07, 0.0, 0.12), (0.12, 0.0, 0.42), 0.008)
    return p.done(solid=False)


def _whistle(B, x, y, z, yaw):
    """A whistle on a cord from a nail (wall piece, back on the wall face at (x, y), z = hook height)."""
    p = kit._P(B, x, y, z, yaw, "whistle")
    p.box("CT_Steel", -0.012, 0.012, -0.03, 0.0, -0.01, 0.012)
    p.rod("CT_SteelRed", (0.0, -0.03, 0.0), (-0.035, -0.035, -0.16), 0.006)
    p.rod("CT_SteelRed", (0.0, -0.03, 0.0), (0.035, -0.035, -0.16), 0.006)
    p.box("CT_Steel", -0.03, 0.03, -0.045, -0.025, -0.19, -0.165)
    p.cyl("CT_Steel", 0.0, -0.035, -0.215, -0.165, 0.016, 6)
    return p.done(solid=False)


def _cleat(B, px, py, z):
    p = kit._P(B, px, py, z, 0.0, "halyard")
    p.box("CT_Steel", -0.05, 0.05, -0.06, -0.03, 0.0, 0.025)
    p.box("CT_Steel", -0.045, -0.02, -0.075, -0.03, 0.0, 0.05)
    p.box("CT_Steel", 0.02, 0.045, -0.075, -0.03, 0.0, 0.05)
    p.rod("CT_PlasterRed", (-0.04, -0.02, 0.2), (-0.04, -0.07, 0.0), 0.012)
    return p.done(solid=False)


def _extras(B, z0):
    """The second pass: the foot of the tower, the deck's back and right side, and the cabin's walls."""
    d = B.detail
    # ---- at the foot: the upturned boat on the right, the oars on the stem's left face, a rope coil, the board
    _boat(B, 1.45, 2.0, 0.0)
    B.thing("rowboat", "machine", (1.45, 2.0, 0.34), 90.0, what="rowboat")
    _oars(B, -0.78, 2.0, 0.5)
    B.thing("oars", "machine", (-0.72, 2.0, 1.3), 270.0, what="oars")
    kit.rope_coil(B, 1.45, 3.5, 0.0, 20.0)
    B.thing("rope_coil", "machine", (1.45, 3.5, 0.12), 0.0, what="rope")
    kit.sandwich_board(B, -0.95, 0.55, 0.0, 0.0, name="board")
    B.thing("temp_board", "board", (-0.95, 0.55, 0.55), 0.0, topic="view")
    B.spot("boat", "stand", (1.45, 1.0, 0.0), 180.0)
    # ---- the deck: a deck chair on the right side, the flag halyard on the pole, the lost-and-found at the back
    dc = kit.deck_chair(B, 1.6, 2.1, z0, 0.0)
    B.thing("deck_chair", "seat", (1.6, 2.1, z0 + 0.35), 0.0, height=0.35)
    B.spot("deck_chair", "seat", (1.6, 2.1, z0), 0.0)
    _cleat(B, 1.8, 0.3, z0 + 1.0)
    fu.rod(d, "CT_PlasterRed", (1.77, 0.29, z0 + 1.2), (1.77, 0.17, 8.92), 0.01)
    B.thing("flag_halyard", "machine", (1.77, 0.3, z0 + 1.05), 90.0, what="halyard")
    B.spot("halyard", "stand", (1.55, 0.45, z0), 90.0)
    B.thing("deck_ring", "board", (0.0, 0.62, 6.05), 0.0, topic="rescue")
    kit.notice_board(B, -0.97, 3.3, z0, 180.0, w=0.42, h=0.3, zc=0.72)
    B.thing("lost_board", "board", (-0.97, 3.34, z0 + 0.72), 180.0, topic="lost")
    kit.crate(B, -1.55, 3.6, z0, 0.0, w=0.42, d=0.3, h=0.26, fill="CT_SteelRed")
    B.thing("lost_box", "machine", (-1.55, 3.6, z0 + 0.22), 180.0, what="lost_box")
    B.spot("lost_box", "stand", (-1.2, 3.55, z0), 270.0)
    cyl(d, "CT_SteelRed", -1.0, 3.62, z0, z0 + 0.1, 0.06, 7)                 # a child's bucket, still waiting
    # the lantern over the back door
    fu.rod(d, "CT_Steel", (0.0, 3.6, WALL_Z1), (0.0, 3.6, z0 + 2.0), 0.01)
    cyl(d, "CT_SteelWhite", 0.0, 3.6, z0 + 1.82, z0 + 1.98, 0.07, 6)
    cyl(d, "CT_Steel", 0.0, 3.6, z0 + 1.78, z0 + 1.82, 0.075, 6)
    B.lamp((0.0, 3.6, z0 + 1.9), lumens=260, colour="#FFD9A0", kind="public", radius=3.5)
    # ---- the cabin: a shelf on the left wall with the megaphone and a transistor, the whistle, the tide table
    kit.wall_shelf(B, -1.15, 1.6, z0, 90.0, w=0.5, zt=0.58, props=False)
    _megaphone(B, -1.0, 1.47, z0 + 0.58)
    _transistor(B, -1.0, 1.76, z0 + 0.58, 90.0)
    B.thing("megaphone", "machine", (-1.0, 1.47, z0 + 0.8), 90.0, what="megaphone")
    B.spot("megaphone", "stand", (-0.55, 1.65, z0), 270.0)
    _whistle(B, 0.4, 3.15, z0 + 1.55, 0.0)
    B.thing("whistle", "machine", (0.4, 3.1, z0 + 1.4), 0.0, what="whistle")
    kit.poster(B, 0.45, 0.85, z0, 180.0, name="chart_calendar", w=0.42, h=0.5, zc=0.55)
    B.thing("tide_table", "board", (0.45, 0.9, z0 + 0.55), 180.0, topic="tides")
    B.spot("tea", "stand", (-0.55, 2.7, z0), 270.0)


# ====================================================================================== third pass
def _gear_locker(B, x, y, yaw):
    """A rescue-gear locker 0.9 x 0.36 x 1.25: blue doors with a red cross, hasp, a torpedo buoy and a throw-line bag on top,
    fins hung on hooks at its side."""
    p = kit._P(B, x, y, 0.0, yaw, "gear_locker")
    p.kind = "prop"
    p.box("CT_ConcreteDark", -0.45, 0.45, -0.18, 0.18, 0.0, 0.08)
    p.box("CT_PlasterBlue", -0.44, 0.44, -0.17, 0.17, 0.08, 1.2)
    p.box("CT_Steel", -0.46, 0.46, -0.19, 0.19, 1.2, 1.24)
    p.box("CT_Steel", -0.006, 0.006, -0.176, -0.17, 0.1, 1.18, f="y")
    p.box("CT_TileWhite", -0.14, -0.04, -0.178, -0.17, 0.62, 0.9, f="y")
    p.box("CT_SteelRed", -0.115, -0.065, -0.1785, -0.1775, 0.67, 0.85, f="y")
    p.box("CT_SteelRed", -0.14, -0.04, -0.1785, -0.1775, 0.735, 0.785, f="y")
    p.box("CT_Brass", 0.02, 0.07, -0.19, -0.176, 0.6, 0.66, f="xXyYZz")
    p.box("CT_Brass", 0.02, 0.07, -0.19, -0.176, 0.35, 0.4, f="xXyYZz")
    for k in range(4):
        p.box("CT_Steel", -0.4 + 0.1 * k, -0.37 + 0.1 * k, -0.178, -0.172, 0.95, 1.15, f="y")
    # torpedo buoy lying on top, a bag of throw line beside it
    p.rod("CT_SteelRed", (-0.36, 0.0, 1.3), (0.12, 0.0, 1.3), 0.075, 8)
    p.rod("CT_TileWhite", (-0.12, 0.0, 1.3), (-0.03, 0.0, 1.3), 0.078, 8)
    p.box("CT_Steel", -0.34, -0.3, -0.02, 0.02, 1.24, 1.26)
    p.box("CT_Steel", 0.04, 0.08, -0.02, 0.02, 1.24, 1.26)
    p.cyl("CT_PlasterYellow", 0.31, 0.0, 1.24, 1.38, 0.09, 8, bottom=True)
    p.rod("CT_PlasterRed", (0.31, 0.0, 1.38), (0.2, 0.0, 1.31), 0.01, 3)
    # fins on hooks on the left side
    for k, zz in enumerate((0.8, 1.0)):
        p.box("CT_Steel", -0.47, -0.45, -0.02, 0.02, zz + 0.1, zz + 0.15, f="xXyYZ")
        p.box("CT_PlasterYellow" if k else "CT_PlasterBlue", -0.475, -0.455, -0.07 + 0.0, 0.07, zz - 0.2, zz + 0.1, f="xXyYZz")
    return p.done()


def _flag_mast(B, x, y):
    """A short mast with three colour discs (green, yellow, red) one over the other and a pointer plate showing the day's flag."""
    p = kit._P(B, x, y, 0.0, 0.0, "flag_mast")
    p.kind = "prop"
    p.cyl("CT_ConcreteDark", 0, 0, 0.0, 0.08, 0.17, 8, bottom=True)
    p.cyl("CT_SteelWhite", 0, 0, 0.08, 2.1, 0.035, 6)
    p.box("CT_SteelWhite", -0.2, 0.2, -0.045, 0.0, 0.95, 1.65)
    for k, mt in enumerate(("CT_PlasterGreen", "CT_PlasterYellow", "CT_SteelRed")):
        p.box(mt, -0.15, 0.1, -0.058, -0.045, 0.99 + 0.22 * k, 1.15 + 0.22 * k, f="xXyYZz")
    p.box("CT_Steel", 0.12, 0.19, -0.065, -0.045, 1.02, 1.1, f="xXyYZz")
    p.box("CT_SteelRed", -0.36, -0.03, -0.012, 0.012, 2.04, 2.3)       # the pennant at the head of the mast
    p.box("CT_SteelWhite", -0.36, -0.03, -0.012, 0.012, 1.76, 2.04)
    return p.done()


def _tide_staff(B, x, y, yaw):
    """A painted tide staff (water gauge): a 1.9 m board with alternating black and white decimetres and red metre marks."""
    p = kit._P(B, x, y, 0.0, yaw, "tide_staff")
    p.kind = "prop"
    p.box("CT_Steel", -0.03, 0.03, -0.03, 0.03, 0.0, 0.1, f="xXyY")
    p.box("CT_TileWhite", -0.06, 0.06, -0.025, 0.025, 0.1, 2.0)
    for k in range(9):
        p.box("CT_ConcreteDark", -0.06, 0.06, -0.0255, -0.025, 0.2 + 0.2 * k, 0.3 + 0.2 * k, f="y")
    p.box("CT_SteelRed", -0.06, 0.06, -0.0258, -0.025, 1.0, 1.04, f="y")
    p.box("CT_SteelRed", -0.06, 0.06, -0.0258, -0.025, 0.4, 0.43, f="y")
    return p.done()


def _bell(B, x, y, z, yaw):
    """[wall] A ship's bell hung from a bracket (back on the wall at (x, y), z = the bracket's height)."""
    p = kit._P(B, x, y, z, yaw, "ship_bell")
    p.kind = "wall"
    p.box("CT_Steel", -0.03, 0.03, -0.015, 0.0, 0.0, 0.32)
    p.box("CT_Steel", -0.015, 0.015, -0.2, 0.0, 0.3, 0.33)
    p.lathe("CT_Brass", 0.0, -0.17, [(0.0, 0.28), (0.04, 0.3), (0.06, 0.2), (0.1, 0.07), (0.115, 0.0)], segs=10)
    p.rod("CT_Steel", (0.0, -0.17, 0.3), (0.0, -0.17, 0.33), 0.01, 3)
    p.rod("CT_PlasterRed", (0.0, -0.17, 0.03), (0.0, -0.17, -0.2), 0.01, 3)
    p.cyl("CT_PlasterRed", 0.0, -0.17, -0.27, -0.2, 0.025, 6)
    return p.done()


def _third(B, z0):
    """Third pass: the foot's gear, the flag mast and the throwing post, the cabin's switch and clock, ledger, bell, binoculars."""
    d = B.detail
    # ---- the foot: the gear locker behind the plinth (opening toward +y), the spine board on the stem, the ring post
    _gear_locker(B, 0.0, 3.25, 180.0)
    B.thing("gear_locker", "machine", (0.0, 3.4, 0.75), 180.0, what="gear")
    B.spot("gear", "stand", (0.0, 3.85, 0.0), 0.0)
    box(d, "CT_PlasterYellow", -0.668, -0.64, 2.3, 2.6, 0.5, 2.1, faces="xXyYZ")
    for zz in (0.9, 1.4, 1.85):
        box(d, "CT_Leather", -0.672, -0.664, 2.295, 2.605, zz, zz + 0.05, faces="xXyYZ")
    for k, zz in enumerate((0.7, 1.15, 1.6)):
        box(d, "CT_ConcreteDark", -0.672, -0.664, 2.34, 2.4, zz, zz + 0.1, faces="X")
        box(d, "CT_ConcreteDark", -0.672, -0.664, 2.5, 2.56, zz, zz + 0.1, faces="X")
    B.thing("spine_board", "machine", (-0.68, 2.45, 1.3), 270.0, what="spine_board")
    kit.ring_buoy(B, -1.55, 3.2, 0.0, 0.0)
    B.thing("ring_post", "machine", (-1.55, 3.0, 1.2), 0.0, what="ring_toss")
    B.spot("ring_throw", "stand", (-1.5, 1.2, 0.0), 180.0)
    _flag_mast(B, -1.65, 0.55)
    B.thing("flag_mast", "board", (-1.65, 0.5, 1.3), 0.0, topic="flagstate")
    _tide_staff(B, 1.93, 0.75, 270.0)
    B.thing("tide_staff", "board", (1.9, 0.75, 1.25), 270.0, topic="tidestaff")
    # ---- the cabin: the lamp switch by the door, the clock, the duty ledger on the front wall
    box(d, "CT_TileWhite", 0.51, 0.59, 3.136, 3.15, z0 + 0.66, z0 + 0.78, faces="xXyYZz")
    box(d, "CT_Steel", 0.538, 0.562, 3.124, 3.136, z0 + 0.70, z0 + 0.745, faces="xXyYZz")
    B.thing("lg_switch", "machine", (0.55, 3.1, z0 + 0.72), 0.0, what="switch", switch="room")
    B.thing("lg_clock", "board", (-0.15, 3.1, z0 + 2.15), 0.0, topic="clock")
    kit.notice_board(B, 1.0, 0.85, z0, 180.0, w=0.3, h=0.4, zc=0.62, seed=4)
    B.thing("duty_ledger", "board", (1.0, 0.9, z0 + 0.62), 180.0, topic="ledger")
    B.spot("staff_lifeguard", "work", (-0.3, 2.5, z0), 0.0)
    # ---- the deck: the ship's bell on the cabin's right wall, a coin binocular on the left rail looking west
    _bell(B, 1.3, 1.5, z0 + 1.35, 90.0)
    B.thing("deck_bell", "machine", (1.45, 1.5, z0 + 1.55), 90.0, what="bell")
    B.spot("staff_lifeguard_2", "work", (0.2, 0.55, z0), 0.0)
    kit.telescope(B, -1.45, 2.3, z0, 270.0)
    B.thing("deck_binoc_w", "machine", (-1.5, 2.3, z0 + 1.2), 270.0, what="binoculars")
    B.spot("binoc_w", "stand", (-1.0, 2.3, z0), 90.0)
