"""School: a low two-storey modernist school round a courtyard.

Original design in the spirit of mid-century European / Soviet standard-plan schools (Hans Scharoun's
Darmstadt school, Alvar Aalto's low halls, Soviet "series" schools with ribbon windows).
Layout (plot 70 x 60 m, building 60 x 50 m, front yard 6 m): a south wing along the street (entrance block in yellow
plaster with a deep covered entrance, a clock face on a bell block), west and east classroom wings, a north wing
with end classrooms and in the middle the ASSEMBLY HALL / GYM (brick, 16 x 28 m, saw-tooth roof with north glazing,
10.8 m high). Everything else is 2 storeys of 3.5 m, ribbon windows (concrete-white piers every 3.75 m), slab edges
standing 0.5 m proud as roofs. The courtyard (40 x 24 m) has benches, planters and a hall rear door.
Enterable (ground floor of the south wing): entrance hall (door 2.4 m, sidelights, benches), one classroom
(16 pupil "seat" spots + teacher "work" spot, desks, board) and a staff room ("table" spot with a table and chairs), with
floors, CT_Interior walls, ceilings and door gaps (1.2 x 2.2 m) in the partitions. Upper floors and the other wings
are solid (windows only show dark / lit glass). Courtyard: 6 "bench" spots. Window kind "public" everywhere.
Entrances: "main" at the street (2 leaves), "court" from the hall onto the courtyard.
Workarounds: the saw-tooth glazing cards stand 3 cm proud of the solid roof teeth (inset -0.03) because there is no hole.
Ideas for later: a bell, a sports field behind the hall, a staircase to the upper floor, murals on the hall wall.
Requests: none.
"""
import math

from citykit import box, prism, ring, DOOR_H  # noqa: F401
import geom
from furn_int2 import Furn

INFO = {
    "name_ru": "Школа", "name_en": "School",
    "style": "modernism", "use": "public",
    "inspired_by": "mid-century courtyard schools: Scharoun, Aalto, Soviet ribbon-window standard schools",
}

H1, H2 = 3.4, 7.0          # ground clear height, wall top
SLAB = 0.2
GRID = 3.75


def _bays(length, nom=GRID, pier=0.6, skip=()):
    n = max(1, round(length / nom))
    w = length / n
    out = []
    for i in range(n):
        if i in skip:
            continue
        out.append((i * w + pier / 2, (i + 1) * w - pier / 2))
    return out


def _yz(m, mat, side_mat, poly, x0, x1):
    geom.extrude_profile(m, mat, side_mat, poly, x0, x1, lambda u, v, t: (t, u, v))


def _spandrels(B, F, L, step, mat):
    """Coloured panels (5 cm proud) between the storeys' windows, one per bay, on facade F."""
    n = max(1, round(L / step))
    w = L / n
    wm = geom.Mesh()
    for i in range(n):
        box(wm, mat, i * w + 0.3, (i + 1) * w - 0.3, 0.0, 0.05, 3.05, 4.4)
    B.m.extend(F.apply(wm))


def _ribbon(B, p0, p1, mat, ground=True, skip_ground=(), extra=(), step=GRID, kind="public", acc=None):
    """Facade p0->p1 with ground and upper ribbon bays; returns F."""
    L = math.hypot(p1[0] - p0[0], p1[1] - p0[1])
    n = max(1, round(L / step))
    w = L / n
    holes = []
    for i in range(n):
        u0, u1 = i * w + 0.3, (i + 1) * w - 0.3
        if ground and i not in skip_ground:
            holes.append((u0, u1, 0.9, 3.0))
        holes.append((u0, u1, 4.5, 6.6))
    holes += list(extra)
    F = B.facade(p0, p1, 0.0, H2, mat, holes=holes, depth=0.3, reveal_mat="CT_ConcreteWhite", back=False)
    for h in holes:
        B.facade_window(F, h[0], h[1], h[2], h[3], kind=kind, cols=2, inset=0.12)
    if acc:
        _spandrels(B, F, L, step, acc)
    return F


def _roof(m, x0, x1, y0, y1, z=H2):
    box(m, "CT_ConcreteWhite", x0 - 0.5, x1 + 0.5, y0 - 0.5, y1 + 0.5, z, z + 0.3)


def _desk(d, x, y):
    """A pupil's desk (top towards +y of the chair) and chair, in B.detail."""
    box(d, "CT_Wood", x - 0.55, x + 0.55, y + 0.25, y + 0.75, 0.72, 0.76)
    for dx in (-0.5, 0.5):
        box(d, "CT_Steel", x + dx - 0.02, x + dx + 0.02, y + 0.28, y + 0.72, 0.0, 0.72)
    box(d, "CT_Wood", x - 0.22, x + 0.22, y - 0.22, y + 0.2, 0.44, 0.47)
    box(d, "CT_Steel", x - 0.22, x + 0.22, y - 0.22, y - 0.19, 0.47, 0.85)
    for dx in (-0.2, 0.2):
        box(d, "CT_Steel", x + dx - 0.015, x + dx + 0.015, y - 0.2, y + 0.18, 0.0, 0.44)


def _coat_rail(f, x, y, yaw, n, seed=0):
    """A wardrobe rail with hooks and coats hanging on the wall at (x, y) facing yaw (coats stick out of the wall)."""
    cols = ("CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterGreen", "CT_PlasterYellow", "CT_Steel", "CT_PlasterPink", "CT_Frame")

    def g(m):
        L = n * 0.32
        box(m, "CT_Wood", -L / 2, L / 2, -0.04, 0.0, 1.45, 1.55)
        for i in range(n):
            u = -L / 2 + 0.16 + i * 0.32
            if (i * 5 + seed) % 7 == 3:
                continue                                      # an empty hook
            h = 0.75 + 0.1 * ((i + seed) % 3)
            box(m, cols[(i * 3 + seed) % len(cols)], u - 0.12, u + 0.12, -0.17, -0.05, 1.5 - h, 1.5)
            box(m, "CT_Frame", u - 0.04, u + 0.04, -0.19, -0.14, 1.42, 1.5)       # a hood/collar
    f.put(g, x, y, yaw)


def _globe(f, x, y, z):
    def g(m):
        box(m, "CT_Wood", -0.09, 0.09, -0.09, 0.09, 0.0, 0.03)
        prism(m, "CT_Steel", ring(0, 0, 0.012, 4), 0.0, 0.1, top=False, bottom=False)
        for r, a, b in ((0.1, 0.1, 0.16), (0.16, 0.16, 0.22), (0.17, 0.22, 0.28), (0.12, 0.28, 0.33), (0.07, 0.33, 0.36)):
            prism(m, "CT_Tile" if r > 0.1 else "CT_PlasterBlue", ring(0, 0, r, 8), a, b, top=(r < 0.1), bottom=False)
    f.put(g, x, y, 0.0)


def _kettle(B, x, y, z):
    d = B.detail
    prism(d, "CT_Aluminium", ring(x, y, 0.09, 8), z, z + 0.2, top=True, bottom=False)
    box(d, "CT_SteelRed", x + 0.09, x + 0.13, y - 0.02, y + 0.02, z + 0.03, z + 0.18)


def _furnish(B, IY0, IY1):
    """Round 2: the hall (wardrobe hooks, notice boards, trophy cabinet, benches), classroom and staff room dressing."""
    f = Furn(B)
    d = B.detail
    C = "#FFF0DC"
    # ------------------------------------------------------------ entrance hall  x -7.35..7.35, y 6.3..15.7
    for i, y in enumerate((7.9, 10.3)):
        _coat_rail(f, -7.35, y, 90.0, 6, seed=i * 2)               # on the west partition, coats face +x
    for i, y in enumerate((7.0, 9.15, 11.3)):
        f.bench(-6.85, y + 0.0, yaw=90.0, w=1.6)
        B.spot("hall_wardrobe_bench_%d" % i, "bench", (-6.85, y, 0.0), 90.0)
    f.cabinet(6.95, 8.4, w=1.6, d=0.5, h=1.9, yaw=270.0, mat="CT_Wood")             # trophy cabinet
    for k in range(3):                                                              # cups on its shelves
        for r in range(3):
            y = 7.85 + 0.45 * k + 0.1
            box(d, "CT_Copper" if (k + r) % 2 else "CT_Steel", 6.95 - 0.04, 6.95 + 0.04, y - 0.04, y + 0.04, 0.45 + r * 0.45, 0.45 + r * 0.45 + 0.14)
    f.panel(7.35, 10.9, 1.65, 1.4, 1.0, 270.0, mat="CT_PlasterGreen", frame="CT_Wood")        # notice boards
    f.panel(7.35, 7.0, 1.7, 0.9, 1.2, 270.0, mat="CT_PlasterYellow", frame="CT_Wood")
    for i, (cx, mat) in enumerate(((-5.5, "CT_PlasterBlue"), (5.5, "CT_PlasterRed"))):
        f.panel(cx, 15.7, 1.9, 1.7, 1.1, 0.0, mat=mat, frame="CT_Frame")                # murals above the back benches
        f.panel(cx + (1.3 if cx > 0 else -1.3) * 0.0, 15.7, 2.8, 0.7, 0.45, 0.0, mat="CT_PlasterYellow", frame=None)
    f.plant(-4.4, 7.0, h=1.5)
    f.plant(4.4, 7.0, h=1.5)
    f.plant(-6.7, 14.7, h=1.1, pot="CT_TileWhite")
    f.border(-7.35, 7.35, 6.3, 15.7, w=0.35, mat="CT_ConcreteDark")
    # doormat before the street door
    box(d, "CT_PlasterGrey", -1.0, 1.0, 6.3, 7.1, 0.0, 0.014, faces="Z")
    f.skirting([(-7.35, 6.3), (-1.2, 6.3)], h=0.12)
    f.skirting([(1.2, 6.3), (7.35, 6.3)], h=0.12)
    f.skirting([(7.35, 6.3), (7.35, 12.4)], h=0.12)
    f.skirting([(7.35, 13.6), (7.35, 15.7)], h=0.12)
    f.skirting([(7.35, 15.7), (1.2, 15.7)], h=0.12)
    f.skirting([(-1.2, 15.7), (-7.35, 15.7)], h=0.12)
    f.skirting([(-7.35, 15.7), (-7.35, 13.6)], h=0.12)
    f.skirting([(-7.35, 12.4), (-7.35, 6.3)], h=0.12)
    for x in (-4.0, 0.0, 4.0):
        for y in (8.8, 13.2):
            B.lamp((x, y, 3.1), lumens=800, colour=C, kind="public", radius=7)
    # ------------------------------------------------------------ classroom  x -18.6..-7.65, y 6.3..14.3
    for i, y in enumerate((7.9, 10.4)):
        f.shelf(-18.4, y, w=2.0, d=0.4, h=1.9, yaw=90.0, rows=4, seed=i * 3)                  # bookshelves on the west wall
    f.panel(-18.6, 12.7, 1.7, 1.7, 1.15, 90.0, mat="CT_PlasterGreen", frame="CT_Wood")        # wall map
    f.panel(-18.6, 12.7, 1.7, 1.2, 0.7, 90.0, mat="CT_TileWhite", frame=None, off=0.02)
    for i, x in enumerate((-16.2, -14.7, -13.2, -11.7)):                                        # portraits over the board
        f.panel(x, IY1, 2.75, 0.42, 0.55, 0.0, mat=("CT_Wood", "CT_Frame", "CT_PlasterGrey", "CT_Wood")[i], frame="CT_Steel")
    _globe(f, -16.6, 12.8, 0.76)
    box(d, "CT_PlasterGrey", -16.0, -11.0, IY1 - 0.1, IY1, 0.92, 0.96)                          # chalk ledge
    f.plant(-8.1, 6.9, h=1.2)
    f.border(-18.6, -7.65, 6.3, 14.3, w=0.3, mat="CT_ConcreteDark")
    for x in (-15.5, -11.5):
        for y in (8.4, 12.0):
            B.lamp((x, y, 3.1), lumens=900, colour=C, kind="public", radius=7)
    # ------------------------------------------------------------ staff room  x 7.65..14.85, y 6.3..14.3
    f.sofa(14.45, 7.9, yaw=270.0, w=2.0, mat="CT_PlasterBlue")
    f.round_table(14.0, 6.9, r=0.35, h=0.5, top="CT_Wood")
    f.counter(13.7, 13.5, 2.2, d=0.5, h=0.9, yaw=0.0)                                           # kitchenette
    _kettle(B, 13.1, 13.5, 0.9)
    f.cup(13.5, 13.4, 0.9)
    f.cup(13.65, 13.55, 0.9, mat="CT_PlasterRed")
    f.cup(11.2, 10.5, 0.77, mat="CT_PlasterYellow")
    f.cup(10.8, 10.3, 0.77)
    f.cup(11.6, 10.7, 0.77, mat="CT_PlasterBlue")
    f.panel(7.65, 8.0, 1.7, 1.4, 1.0, 90.0, mat="CT_PlasterGreen", frame="CT_Wood")             # timetable on the partition
    f.plant(8.3, 7.0, h=1.4)
    f.plant(14.35, 12.2, h=1.0, pot="CT_TileWhite")
    f.rug(9.6, 12.8, 9.2, 11.8, mat="CT_PlasterRed")
    f.border(7.65, 14.85, 6.3, 14.3, w=0.3, mat="CT_ConcreteDark")
    for x, y in ((9.6, 8.6), (13.0, 8.6), (11.2, 12.2)):
        B.lamp((x, y, 3.1), lumens=800, colour=C, kind="public", radius=7)


def build(B):
    m, d = B.m, B.detail
    # ================================================================== south wing (street)
    SY0, SY1 = 6.0, 16.0
    # ---- facades: left (white) incl. classroom, hall (yellow), right (white) incl. staff room
    FL = _ribbon(B, (-30, SY0), (-7.5, SY0), "CT_Plaster", acc="CT_PlasterBlue")
    FR = _ribbon(B, (7.5, SY0), (30, SY0), "CT_Plaster", acc="CT_PlasterBlue")
    hx = [(-7.0, -1.9, 0.3, 3.0), (1.9, 7.0, 0.3, 3.0)]
    hu = [(a + 7.5, b + 7.5, c, e) for a, b, c, e in hx]
    door = (-1.2 + 7.5, 1.2 + 7.5, 0.0, 2.4)
    up = [(i * 3.75 + 0.3 + 0.0, (i + 1) * 3.75 - 0.3, 4.5, 6.6) for i in range(4)]
    holes = hu + [door] + up
    FH = B.facade((-7.5, SY0), (7.5, SY0), 0.0, H2, "CT_PlasterYellow", holes=holes, depth=0.3,
                  reveal_mat="CT_ConcreteWhite", back=False)
    for h in hu + up:
        B.facade_window(FH, h[0], h[1], h[2], h[3], kind="public", cols=3 if h[1] - h[0] > 3 else 2, inset=0.12)
    B.entrance("main", (0.0, SY0, 0.0), 0.0, width=2.4, height=2.4, kind="street", leaves=2)
    # ---- covered entrance: canopy, columns, fascia, bell block with clock
    box(m, "CT_ConcreteWhite", -9.0, 9.0, 0.4, SY0, 3.9, 4.2)
    box(m, "CT_PlasterRed", -9.0, 9.0, 0.4, 0.5, 3.6, 4.5, faces="all")
    for cx in (-8.2, -4.8, 4.8, 8.2):
        B.column_round(cx, 1.4, 0.0, 3.9, 0.2, segs=14)
    box(m, "CT_ConcreteWhite", -3.0, 3.0, SY0 - 0.1, SY0 + 2.6, H2, 9.6)
    box(m, "CT_PlasterRed", -3.0, 3.0, SY0 - 0.1, SY0 + 2.6, 9.6, 9.8)
    cy, cz, cr = SY0 - 0.14, 8.3, 0.9
    segs = 28
    for k in range(segs):
        a0, a1 = 2 * math.pi * k / segs, 2 * math.pi * (k + 1) / segs
        d.face("CT_TileWhite", [(cr * math.cos(a0), cy, cz + cr * math.sin(a0)), (cr * math.cos(a1), cy, cz + cr * math.sin(a1)),
                                (0, cy, cz)][::-1][::-1])
        d.face("CT_Steel", [(cr * math.cos(a0), cy + 0.01, cz + cr * math.sin(a0)),
                            (cr * math.cos(a1), cy + 0.01, cz + cr * math.sin(a1)),
                            ((cr + 0.1) * math.cos(a1), cy + 0.01, cz + (cr + 0.1) * math.sin(a1)),
                            ((cr + 0.1) * math.cos(a0), cy + 0.01, cz + (cr + 0.1) * math.sin(a0))][::-1][::-1])
    box(d, "CT_Steel", -0.04, 0.04, cy - 0.03, cy, cz, cz + 0.65)
    box(d, "CT_Steel", 0.0, 0.55, cy - 0.03, cy, cz - 0.04, cz + 0.04)
    # ---- mass of the south wing, rooms hollow on the ground floor
    # rooms: classroom x -18.6..-7.65, staff 7.65..14.85, hall -7.35..7.35; ground clear 3.4
    IY0, IY1 = SY0 + 0.3, 14.3
    solid = [(-29.7, -18.9), (14.85 + 0.3, 29.7)]
    for x0, x1 in solid:
        box(m, "CT_Plaster", x0, x1, SY0 + 0.3, SY1 - 0.3, 0.0, H2)
    # walls around the classroom (west wall) and staff room (east wall), partitions at -7.5 and 7.5
    box(m, "CT_Interior", -18.9, -18.6, IY0, 15.7, 0.0, H1)
    box(m, "CT_Interior", 14.85, 15.15, IY0, 15.7, 0.0, H1)
    box(m, "CT_Interior", -18.9, -7.35, IY1, IY1 + 0.3, 0.0, H1)            # classroom back wall (board)
    box(m, "CT_Interior", 7.35, 15.15, IY1, IY1 + 0.3, 0.0, H1)             # staff back wall
    box(m, "CT_Plaster", -18.9, -7.35, IY1 + 0.3, 15.7, 0.0, H1)             # filler behind
    box(m, "CT_Plaster", 7.35, 15.15, IY1 + 0.3, 15.7, 0.0, H1)
    for sx in (-1, 1):                                                       # partitions with door gaps
        xa, xb = (7.35, 7.65) if sx > 0 else (-7.65, -7.35)
        gap0, gap1 = 12.4, 13.6
        box(m, "CT_Interior", xa, xb, IY0, gap0, 0.0, H1)
        box(m, "CT_Interior", xa, xb, gap1, 15.7, 0.0, H1)
        box(m, "CT_Interior", xa, xb, gap0, gap1, 2.3, H1)
    # hall side walls (inside the partitions are shared) and back wall pieces handled by the facade below
    # ceilings (interior) and everything above
    box(m, "CT_Interior", -18.9, 15.15, IY0, 15.7, H1, H1 + SLAB, faces="all")
    box(m, "CT_Plaster", -18.9, 15.15, IY0, 15.7, H1 + SLAB, H2)
    # floors
    box(m, "CT_Floor", -18.6, 14.85, IY0, 15.7, 0.0, 0.02, faces="Z")
    # interior skins of the room fronts (CT_Interior), holes = ground bays
    def skin(xa, xb, bays_x):
        holes = []
        for (a, b) in bays_x:
            if a > xa and b < xb:
                holes.append((xb - b, xb - a, 0.9, 3.0))
        B.facade((xb, IY0), (xa, IY0), 0.0, H1, "CT_Interior", holes=holes, depth=0.02, reveal_mat="CT_Interior", back=False)
    gl = [(-30 + i * 3.75 + 0.3, -30 + (i + 1) * 3.75 - 0.3) for i in range(6)]
    skin(-18.6, -7.65, gl)
    gr = [(7.5 + i * 3.75 + 0.3, 7.5 + (i + 1) * 3.75 - 0.3) for i in range(6)]
    skin(7.65, 14.85, gr)
    # hall interior skins: front (sidelights + door) and back (door to the courtyard), CT_Interior
    B.facade((7.35, IY0), (-7.35, IY0), 0.0, H1, "CT_Interior",
             holes=[(0.35, 5.45, 0.3, 3.0), (9.25, 14.35, 0.3, 3.0), (6.15, 8.55, 0.0, 2.4)],
             depth=0.02, reveal_mat="CT_Interior", back=False)
    B.facade((-7.35, 15.7), (7.35, 15.7), 0.0, H1, "CT_Interior", holes=[(6.15, 8.55, 0.0, 2.4)],
             depth=0.02, reveal_mat="CT_Interior", back=False)
    # south wing back (courtyard side): bays, door in the middle
    # ground bays of the back side exclude the door zone [-3.75, 3.75] (u from x=20: u = 20 - x)
    holes = []
    n = round(40 / GRID)
    w = 40 / n
    for i in range(n):
        x0, x1 = 20 - (i + 1) * w, 20 - i * w          # bay in x
        u0, u1 = i * w + 0.3, (i + 1) * w - 0.3
        if abs((x0 + x1) / 2) > 2.9:
            holes.append((u0, u1, 0.9, 3.0))
        holes.append((u0, u1, 4.5, 6.6))
    holes.append((20 - 1.2, 20 + 1.2, 0.0, 2.4))
    FB = B.facade((20, SY1), (-20, SY1), 0.0, H2, "CT_Plaster", holes=holes, depth=0.3, reveal_mat="CT_ConcreteWhite", back=False)
    for h in holes:
        if h[3] - h[2] < 2.3:
            B.facade_window(FB, h[0], h[1], h[2], h[3], kind="public", cols=2, inset=0.12)
    _spandrels(B, FB, 40.0, GRID, "CT_PlasterYellow")
    B.entrance("court", (0.0, SY1, 0.0), 180.0, width=2.4, height=2.4, kind="street", leaves=2)
    # hall rear wall pieces inside: the hall interior is y 6.3..15.7, its end is the back facade (inner face hidden)
    # ---- classroom: desks, board, teacher
    for r, y in enumerate((7.6, 8.9, 10.2, 11.5)):
        for c, x in enumerate((-16.4, -14.0, -11.6, -9.2)):
            _desk(d, x, y)
            B.spot("seat_%d_%d" % (r, c), "seat", (x, y - 0.1, 0.0), 180.0)
    box(d, "CT_Steel", -16.0, -11.0, IY1 - 0.04, IY1, 1.0, 2.2)
    box(d, "CT_Wood", -17.0, -14.5, 12.5, 13.1, 0.72, 0.76)
    box(d, "CT_Steel", -16.98, -16.94, 12.52, 12.58, 0.0, 0.72)
    box(d, "CT_Steel", -14.56, -14.52, 12.52, 12.58, 0.0, 0.72)
    B.spot("teacher", "work", (-15.7, 13.6, 0.0), 0.0)
    # ---- staff room
    tx, ty = 11.2, 10.5
    box(d, "CT_Wood", tx - 1.4, tx + 1.4, ty - 0.55, ty + 0.55, 0.72, 0.77)
    for dx in (-1.3, 1.3):
        for dy in (-0.45, 0.45):
            box(d, "CT_Steel", tx + dx - 0.03, tx + dx + 0.03, ty + dy - 0.03, ty + dy + 0.03, 0.0, 0.72)
    for dx in (-0.8, 0.0, 0.8):
        for sy in (-1, 1):
            cy_ = ty + sy * 0.95
            box(d, "CT_Wood", tx + dx - 0.22, tx + dx + 0.22, cy_ - 0.22, cy_ + 0.22, 0.44, 0.48)
            box(d, "CT_Steel", tx + dx - 0.22, tx + dx + 0.22, cy_ + sy * 0.2 - 0.015, cy_ + sy * 0.2 + 0.015, 0.48, 0.88)
    box(d, "CT_Wood", 8.0, 14.6, IY1 - 0.5, IY1, 0.0, 1.9)                  # cupboards along the back wall
    B.spot("staff_table", "table", (tx, ty, 0.0), 0.0)
    # ---- hall benches
    for x in (-5.5, 5.5):
        box(d, "CT_Wood", x - 1.0, x + 1.0, 14.6, 15.05, 0.42, 0.47)
        for u in (x - 0.9, x + 0.8):
            box(d, "CT_Steel", u, u + 0.1, 14.65, 15.0, 0.0, 0.42)
        B.spot("hall_bench_%d" % (x > 0), "bench", (x, 14.8, 0.0), 180.0)
    _furnish(B, IY0, IY1)
    _roof(m, -30, 30, SY0, SY1)
    # ================================================================== west and east wings
    for sgn in (-1, 1):
        xo, xi = 30 * sgn, 20 * sgn
        mx0, mx1 = sorted((xo - sgn * 0.3, xi + sgn * 0.3))
        box(m, "CT_Plaster", mx0, mx1, SY1 - 0.3, 40.0, 0.0, H2)
        box(m, "CT_Plaster", mx0, mx1, 40.0, 49.7, 0.0, H2)
        mat = "CT_Plaster"
        if sgn < 0:
            _ribbon(B, (xo, 50.0), (xo, SY0), mat, step=3.7, acc="CT_PlasterBlue")          # outer, faces -x
            _ribbon(B, (xi, SY1), (xi, 40.0), mat, acc="CT_PlasterYellow")                    # courtyard side, faces +x
        else:
            _ribbon(B, (xo, SY0), (xo, 50.0), mat, step=3.7, acc="CT_PlasterBlue")
            _ribbon(B, (xi, 40.0), (xi, SY1), mat, acc="CT_PlasterYellow")
        _roof(m, min(xo, xi), max(xo, xi), SY1 - 0.3, 49.7)
    # ================================================================== north wing and hall
    for sgn in (-1, 1):
        x0, x1 = sorted((14.0 * sgn, 30.0 * sgn))
        box(m, "CT_Plaster", x0 + (0.3 if sgn < 0 else 0), x1 - (0.3 if sgn > 0 else 0), 40.3, 49.7, 0.0, H2)
        # back (north) facade faces +y
        if sgn < 0:
            _ribbon(B, (-14.0, 50.0), (-30.0, 50.0), "CT_PlasterGrey", step=3.7)
        else:
            _ribbon(B, (30.0, 50.0), (14.0, 50.0), "CT_PlasterGrey", step=3.7)
    # courtyard side of the north wing end bays (x 14..20 / -20..-14), faces -y
    _ribbon(B, (-20.0, 40.0), (-14.0, 40.0), "CT_Plaster", step=3.0)
    _ribbon(B, (14.0, 40.0), (20.0, 40.0), "CT_Plaster", step=3.0)
    _roof(m, -30, -14, 40.0, 49.7)
    _roof(m, 14, 30, 40.0, 49.7)
    # hall block 28 x 16, walls to 8.7, saw-tooth roof to 10.8
    HZ = 8.7
    box(m, "CT_Brick", -13.7, 13.7, 40.3, 55.7, 0.0, HZ)
    hh = []
    n = 7
    w = 28 / n
    for i in range(n):
        hh.append((i * w + 0.5, (i + 1) * w - 0.5, 4.5, 7.5))
    # courtyard front (faces -y), from x -14 to 14; a tall door-less glazed foyer strip + high windows
    FHf = B.facade((-14.0, 40.0), (14.0, 40.0), 0.0, HZ, "CT_Brick", holes=hh, depth=0.3, reveal_mat="CT_ConcreteWhite", back=False)
    for h in hh:
        B.facade_window(FHf, *h, kind="public", cols=3, inset=0.12)
    # hall sides y 50..56 and back y=56
    F = B.facade((-14.0, 56.0), (-14.0, 50.0), 0.0, HZ, "CT_Brick", holes=[(1.4, 3.4, 4.5, 7.5), (4.6, 6.6, 4.5, 7.5)], depth=0.3, reveal_mat="CT_ConcreteWhite", back=False)
    for h in [(1.4, 3.4, 4.5, 7.5), (4.6, 6.6, 4.5, 7.5)]:
        B.facade_window(F, *h, kind="public", cols=2, inset=0.12)
    F = B.facade((14.0, 50.0), (14.0, 56.0), 0.0, HZ, "CT_Brick", holes=[(1.4, 3.4, 4.5, 7.5), (4.6, 6.6, 4.5, 7.5)], depth=0.3, reveal_mat="CT_ConcreteWhite", back=False)
    for h in [(1.4, 3.4, 4.5, 7.5), (4.6, 6.6, 4.5, 7.5)]:
        B.facade_window(F, *h, kind="public", cols=2, inset=0.12)
    bk = [(i * 4.0 + 0.5, (i + 1) * 4.0 - 0.5, 4.5, 7.5) for i in range(7)]
    F = B.facade((14.0, 56.0), (-14.0, 56.0), 0.0, HZ, "CT_Brick", holes=bk, depth=0.3, reveal_mat="CT_ConcreteWhite", back=False)
    for h in bk:
        B.facade_window(F, *h, kind="public", cols=2, inset=0.12)
    # saw-tooth roof: 4 teeth along y (each 4 m), ramps rise to +y, vertical faces glazed facing +y
    # build CCW polygon: bottom left->right (y 40->56), then back along the top from y=56 to 40
    top = [(56.0, HZ)]
    for k in range(3, -1, -1):
        ya = 40.0 + 4 * k
        top.append((ya + 4.0, 10.8))
        top.append((ya, HZ))
    poly = [(40.0, HZ - 0.3), (56.0, HZ - 0.3)] + top
    # remove duplicate points (consecutive equal)
    clean = []
    for p in poly:
        if not clean or abs(p[0] - clean[-1][0]) > 1e-6 or abs(p[1] - clean[-1][1]) > 1e-6:
            clean.append(p)
    if abs(clean[-1][0] - clean[0][0]) < 1e-6 and abs(clean[-1][1] - clean[0][1]) < 1e-6:
        clean.pop()
    _yz(m, "CT_ConcreteWhite", "CT_ConcreteWhite", clean, -14.0, 14.0)
    for k in range(4):
        ya = 40.0 + 4 * k + 4.0
        for (a, b) in ((-13.4, -4.5), (-4.2, 4.2), (4.5, 13.4)):
            B.window([(b, ya, 9.0), (a, ya, 9.0), (a, ya, 10.6), (b, ya, 10.6)], kind="public", cols=3, inset=-0.03, frame=0.05)
    # courtyard: benches along the wings and planters
    for i, (x, y, yaw) in enumerate(((-19.2, 22.0, 90.0), (-19.2, 34.0, 90.0), (19.2, 22.0, 270.0), (19.2, 34.0, 270.0),
                                      (-6.0, 40.0 - 0.9, 0.0), (6.0, 40.0 - 0.9, 0.0))):
        if abs(yaw) in (90.0, 270.0):
            sx = 1 if yaw == 90.0 else -1
            box(d, "CT_Wood", x - 0.2 if sx > 0 else x - 0.2, x + 0.25, y - 1.0, y + 1.0, 0.42, 0.47)
            box(m, "CT_Concrete", x - 0.2, x + 0.25, y - 0.9, y - 0.7, 0.0, 0.42)
            box(m, "CT_Concrete", x - 0.2, x + 0.25, y + 0.7, y + 0.9, 0.0, 0.42)
        else:
            box(d, "CT_Wood", x - 1.0, x + 1.0, y - 0.25, y + 0.2, 0.42, 0.47)
            box(m, "CT_Concrete", x - 0.9, x - 0.7, y - 0.2, y + 0.15, 0.0, 0.42)
            box(m, "CT_Concrete", x + 0.7, x + 0.9, y - 0.2, y + 0.15, 0.0, 0.42)
        B.spot("court_bench_%d" % i, "bench", (x, y, 0.0), yaw)
    for x in (-12.0, 12.0):
        box(m, "CT_ConcreteWhite", x - 3.0, x + 3.0, 26.0, 27.2, 0.0, 0.6)
        box(m, "CT_Grass", x - 2.85, x + 2.85, 26.15, 27.05, 0.55, 0.62, faces="Z")
    B.footprint = [(-30.5, 0.4), (30.5, 0.4), (30.5, 55.8), (-30.5, 55.8)]
    B.height = 10.8
