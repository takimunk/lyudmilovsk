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
import interior_kit as kit

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


def _ribbon(B, p0, p1, mat, ground=True, skip_ground=(), extra=(), step=GRID, kind="public", acc=None, doors=()):
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
    wins = list(holes)
    holes = holes + list(doors)
    F = B.facade(p0, p1, 0.0, H2, mat, holes=holes, depth=0.3, reveal_mat="CT_ConcreteWhite", back=False)
    for h in wins:
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
    f.panel(7.35, 7.0, 1.7, 0.9, 1.2, 270.0, mat="CT_PlasterYellow", frame="CT_Wood")
    for i, (cx, mat) in enumerate(((-5.5, "CT_PlasterBlue"), (5.5, "CT_PlasterRed"))):
        f.panel(cx, 15.7, 1.9, 1.7, 1.1, 0.0, mat=mat, frame="CT_Frame")                # murals above the back benches
        f.panel(cx + (1.3 if cx > 0 else -1.3) * 0.0, 15.7, 2.8, 0.7, 0.45, 0.0, mat="CT_PlasterYellow", frame=None)
    kit.rug(B, 0.0, 11.0, 0.02, 0.0, w=2.6, d=6.0, seed=3)                          # QA: the hall floor was bare
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
    f.border(-18.6, -7.65, 6.3, 14.3, w=0.3, mat="CT_ConcreteDark")
    for x in (-15.5, -11.5):
        for y in (8.4, 12.0):
            B.lamp((x, y, 3.1), lumens=900, colour=C, kind="public", radius=7)
    # ------------------------------------------------------------ staff room  x 7.65..14.85, y 6.3..14.3
    f.border(7.65, 14.85, 6.3, 14.3, w=0.3, mat="CT_ConcreteDark")
    for x, y in ((9.6, 8.6), (13.0, 8.6), (11.2, 12.2)):
        B.lamp((x, y, 3.1), lumens=800, colour=C, kind="public", radius=7)


# ---------------------------------------------------------------- hollow rooms in the wings and the gym
def _skin_x(B, x, ya, yb, holes, up, mat="CT_Interior", z1=H1):
    """Interior skin on the wall plane x between y = ya..yb; the room is on +x when up (heading +y), else on -x.
    holes: [(ylo, yhi, h0, h1)] in building y."""
    hs = []
    for lo, hi, h0, h1 in holes:
        lo, hi = max(lo, ya), min(hi, yb)
        if hi - lo < 0.25:
            continue
        hs.append((lo - ya, hi - ya, h0, h1) if up else (yb - hi, yb - lo, h0, h1))
    p0, p1 = ((x, ya), (x, yb)) if up else ((x, yb), (x, ya))
    B.facade(p0, p1, 0.0, z1, mat, holes=hs, depth=0.02, reveal_mat=mat, back=False)


def _skin_y(B, y, xa, xb, holes, right, mat="CT_Brick", z1=H1):
    """Interior skin on the wall plane y between x = xa..xb; the room is on +y when `right` is False (heading -x)."""
    hs = []
    for lo, hi, h0, h1 in holes:
        lo, hi = max(lo, xa), min(hi, xb)
        if hi - lo < 0.25:
            continue
        hs.append((xb - hi, xb - lo, h0, h1) if not right else (lo - xa, hi - xa, h0, h1))
    p0, p1 = ((xb, y), (xa, y)) if not right else ((xa, y), (xb, y))
    B.facade(p0, p1, 0.0, z1, mat, holes=hs, depth=0.02, reveal_mat=mat, back=False)


def _ribbon_bays_y(y_from, y_to, step, ground_skip=()):
    """Ground window bays [(ylo, yhi)] of a ribbon facade running between two y values (any direction)."""
    L = abs(y_to - y_from)
    n = max(1, round(L / step))
    w = L / n
    s = 1 if y_to > y_from else -1
    out = []
    for i in range(n):
        a, b = y_from + s * (i * w + 0.3), y_from + s * ((i + 1) * w - 0.3)
        out.append((min(a, b), max(a, b)))
    return out


def _wing_rooms(B, sgn, doors):
    """Hollow the ground floor of a side wing between y 15.7 and 40: skins, floor, ceiling. doors: [door centre y].
    Returns (x0, x1) of the interior."""
    m = B.m
    x0, x1 = (-29.7, -20.3) if sgn < 0 else (20.3, 29.7)
    Y0, Y1 = 15.7, 40.0
    box(m, "CT_Plaster", x0, x1, Y0, Y1, H1 + SLAB, H2)
    box(m, "CT_Interior", x0, x1, Y0, Y1, H1, H1 + SLAB, faces="all")
    box(m, "CT_Floor", x0, x1, Y0, Y1, 0.0, 0.02, faces="Z")
    box(m, "CT_Interior", x0, x1, Y0, Y0 + 0.06, 0.0, H1)
    box(m, "CT_Interior", x0, x1, Y1 - 0.06, Y1, 0.0, H1)
    win = [(lo, hi, 0.9, 3.0) for lo, hi in _ribbon_bays_y(50.0 if sgn < 0 else 6.0, 6.0 if sgn < 0 else 50.0, 3.7)]
    cy = [(lo, hi, 0.9, 3.0) for lo, hi in _ribbon_bays_y(Y0 + 0.3 if False else 16.0, 40.0, GRID)]
    cy = [h for h in cy if not any(abs((h[0] + h[1]) / 2 - d) < 2.0 for d in doors)]
    cy += [(d - 1.0, d + 1.0, 0.0, 2.4) for d in doors]
    if sgn < 0:
        _skin_x(B, x0, Y0, Y1, win, True)          # outer wall, room on +x
        _skin_x(B, x1, Y0, Y1, cy, False)          # courtyard wall, room on -x
    else:
        _skin_x(B, x1, Y0, Y1, win, False)
        _skin_x(B, x0, Y0, Y1, cy, True)
    return x0, x1


def _th(B, tid, kind, r, yaw, h=None, **props):
    """Declare a thing at the middle of a kit result r (or a point)."""
    pos = r["pos"] if isinstance(r, dict) else r
    if h is not None:
        pos = (pos[0], pos[1], h)
    B.thing(tid, kind, pos, yaw, **props)


def _seat_thing(B, tid, r, i=0):
    s = r["seats"][i]
    B.thing(tid, "seat", (s[0], s[1], s[2]), s[3], height=round(s[2], 2))


def _classroom(B, IY1):
    K = kit
    for r, y in enumerate((7.6, 8.9, 10.2, 11.5)):
        for cc, cx in enumerate((-16.0, -13.1, -10.2)):
            K.school_desk(B, cx, y, 0.0, 180.0, seed=r * 3 + cc)
            for j, s in enumerate((-0.28, 0.28)):
                K.chair(B, cx + s, y - 0.55, 0.0, 0.0, kind="school")
                B.spot("seat_%d_%d" % (r, 2 * cc + j), "seat", (cx + s, y - 0.55, 0.0), 0.0)
            if cc != 1:
                B.thing("seat_%d_%d" % (r, 2 * cc), "craft", (cx, y, 0.76), 180.0, what="school_desk")
    K.blackboard(B, -13.3, IY1, 0.0, 0.0, w=3.6, h=1.15, zb=0.9)
    td = K.teacher_desk(B, -13.3, 12.9, 0.0, 0.0)
    K.globe(B, -12.8, 12.9, td["top"], 0.0)
    B.spot("teacher", "work", (-13.3, 13.7, 0.0), 0.0)
    B.thing("teacher", "craft", (-13.3, IY1 - 0.1, 1.5), 0.0, what="blackboard")
    for x in (-15.6, -11.0):
        K.picture(B, x, IY1, 0.0, 0.0, name="painting_portrait", w=0.4, zc=2.65) if x < -13 else K.picture(B, x, IY1, 0.0, 0.0, name="painting_abstract", w=0.5, zc=2.65)
    K.wall_map(B, -18.6, 11.4, 0.0, 90.0, w=1.2, h=0.9, zc=1.65, name="map_coast")
    K.wall_map(B, -18.6, 12.8, 0.0, 90.0, w=1.0, h=0.5, zc=1.65, name="map_world")
    for i in range(2):
        K.bookshelf(B, -18.6, 8.2 + i * 1.0, 0.0, 90.0, w=1.0, h=1.9, levels=4, topic="encyclopaedia", seed=70 + i)
    B.thing("class_shelf", "shelf", (-18.4, 8.7, 1.2), 90.0, topic="science")
    K.wall_clock(B, -7.65, 10.0, 0.0, 270.0, zc=2.6, r=0.2)
    K.poster(B, -7.65, 8.4, 0.0, 270.0, name="poster_health", w=0.7, zc=1.6)
    for x in (-16.9, -13.1, -9.4):
        K.radiator(B, x, 6.32, 0.0, 180.0, w=1.2)
    K.plant(B, -18.0, 13.6, 0.0, 0.0, kind="ficus", pot="CT_PlasterGreen", h=1.1)
    K.plant(B, -8.3, 6.9, 0.0, 0.0, kind="fern", pot="CT_PlasterYellow", h=0.8)


def _staff_room(B):
    K = kit
    K.rug(B, 11.2, 10.4, 0.0, 0.0, w=3.4, d=2.6, name="rug_3")
    K.table(B, 11.2, 10.4, 0.0, 0.0, w=2.8, d=1.1)
    chs = K.around_table(B, 11.2, 10.4, 2.8, 1.1, 0.0, per_side=(3, 3, 0, 0), kind="wood", mat="CT_PlasterBlue")
    for j, (px, cmat) in enumerate(((10.4, "CT_PlasterYellow"), (11.3, "CT_PlasterBlue"), (12.0, "CT_PlasterRed"))):
        K.cup(B, px, 10.3 + 0.12 * (j % 2), 0.75, 0.0, mat=cmat)
    K.teapot(B, 11.8, 10.55, 0.75, 0.0)
    K.newspaper(B, 10.5, 10.6, 0.75, 15.0, seed=3)
    B.spot("staff_table", "table", (11.2, 9.15, 0.0), 0.0)
    _seat_thing(B, "staff_seat_1", chs[0])
    ku = K.kitchen_unit(B, 12.6, 13.95, 0.0, 0.0, modules="fridge,cupboard,sink,stove,cupboard")
    K.samovar(B, 13.9, 14.0, 0.9, 0.0)
    K.cup(B, 12.0, 13.9, 0.9, 0.0)
    K.sofa(B, 14.4, 7.9, 0.0, 270.0, seats=2, mat="CT_PlasterBlue", solid=False)
    K.coffee_table(B, 13.3, 7.9, 0.0, 90.0, w=0.9, d=0.5)
    K.floor_lamp(B, 14.4, 6.8, 0.0, 0.0)
    B.thing("staff_sofa", "seat", (14.45, 7.9, 0.45), 270.0, height=0.45)
    for i in range(2):
        K.bookshelf(B, 7.65, 8.4 + i * 1.0, 0.0, 90.0, w=1.0, h=1.9, levels=4, topic="library", seed=80 + i)
    B.thing("staff_shelf", "shelf", (7.85, 8.9, 1.2), 90.0, topic="local")
    K.notice_board(B, 7.65, 10.6, 0.0, 90.0, w=0.8, zc=1.6)
    B.thing("staff_board", "board", (7.7, 10.6, 1.6), 90.0, topic="timetable")
    K.filing_cabinet(B, 14.8, 12.3, 0.0, 270.0, drawers=4)
    K.plant(B, 8.4, 6.9, 0.0, 0.0, kind="ficus", pot="CT_PlasterRed", h=1.2)
    K.wall_clock(B, 14.85, 10.4, 0.0, 270.0, zc=2.5, r=0.18)


def _hall_things(B):
    K = kit
    K.poster(B, 7.35, 10.9, 0.0, 270.0, name="news_0", w=1.5, zc=1.55)
    B.thing("wall_newspaper", "board", (7.3, 10.9, 1.55), 270.0, topic="wallpaper")
    B.thing("hall_timetable", "board", (7.3, 7.0, 1.7), 270.0, topic="timetable")
    B.thing("trophy_case", "board", (6.9, 8.4, 1.3), 270.0, topic="exhibit")
    B.thing("hall_mural_a", "board", (-5.5, 15.65, 1.9), 180.0, topic="painting")
    B.thing("hall_mural_b", "board", (5.5, 15.65, 1.9), 180.0, topic="painting")
    B.thing("hall_wardrobe_bench", "seat", (-6.85, 9.15, 0.45), 90.0, height=0.45)
    B.thing("hall_back_bench_1", "seat", (-5.5, 14.8, 0.45), 180.0, height=0.45)
    B.thing("hall_back_bench_2", "seat", (5.5, 14.8, 0.45), 180.0, height=0.45)


def _bio_room(B, x0, x1):
    K = kit
    Y0, Y1 = 15.76, 27.85
    box(B.m, "CT_Interior", x0, x1, Y1, Y1 + 0.3, 0.0, H1)                      # partition to the music room
    K.blackboard(B, -25.0, 15.76, 0.0, 180.0, w=3.6, h=1.15, zb=0.9)
    td = K.teacher_desk(B, -25.0, 17.1, 0.0, 180.0)
    B.spot("bio_teacher", "work", (-25.0, 16.3, 0.0), 180.0)
    B.thing("bio_blackboard", "craft", (-25.0, 15.9, 1.5), 180.0, what="blackboard")
    sk = K.skeleton(B, -28.9, 16.5, 0.0, 180.0)
    B.thing("bio_skeleton", "board", (-28.9, 16.5, 1.3), 180.0, topic="exhibit")
    K.poster(B, -27.6, 15.76, 0.0, 180.0, name="chart_anatomy", w=0.9, zc=1.6)
    B.thing("bio_chart", "board", (-27.6, 15.9, 1.6), 180.0, topic="health")
    K.poster(B, -22.6, 15.76, 0.0, 180.0, name="chart_birds", w=0.6, zc=1.6)
    aq = K.aquarium(B, -21.2, 16.2, 0.0, 180.0, w=1.0)
    B.thing("bio_aquarium", "screen", (-21.2, 16.2, 1.0), 180.0, what="aquarium")
    for j, cx in enumerate((-27.3, -23.1)):
        for i, y in enumerate((19.2, 21.8, 24.4)):
            lb = K.lab_bench(B, cx, y, 0.0, 180.0, w=2.0, seed=j * 3 + i)
            for s in (-0.5, 0.5):
                st = K.stool(B, cx + s, y + 0.75, 0.0, 0.0)
                if i == 0 and j == 0 and s < 0:
                    _seat_thing(B, "bio_seat_1", st)
            if i == 0:
                B.thing("bio_bench_%d" % j, "craft", (cx, y, 0.9), 180.0, what="workbench")
    for i in range(2):
        K.shop_shelf(B, -28.6 + i * 1.9, Y1, 0.0, 0.0, w=1.8, goods="jars", seed=i)
    K.display_case(B, -24.6, Y1 - 0.35, 0.0, 0.0, w=1.2, d=0.6, goods="bottles")
    B.thing("bio_shelf", "shelf", (-28.0, Y1 - 0.2, 1.2), 0.0, topic="medicine")
    K.plant(B, -22.0, 27.3, 0.0, 0.0, kind="ficus", pot="CT_PlasterGreen", h=1.1)
    K.plant(B, -20.9, 27.2, 0.0, 0.0, kind="fern", pot="CT_PlasterYellow", h=0.8)
    K.wall_clock(B, -22.0, 15.76, 0.0, 180.0, zc=2.7, r=0.2)
    K.pendant(B, -25.0, 19.0, H1, drop=0.5, kind="cone")
    B.lamp((-25.0, 19.0, 3.0), lumens=1400, colour="#FFF0DC", kind="public", radius=11.0, fixture=False)


def _music_room(B, x0, x1):
    K = kit
    Y0, Y1 = 28.15, 39.94
    pn = K.piano(B, -26.0, Y1 - 0.34, 0.0, 0.0)
    st = K.stool(B, pn["seat"][0], pn["seat"][1], 0.0, 180.0)
    B.thing("music_piano", "play", (-26.0, Y1 - 0.34, 1.0), 0.0, what="piano")
    _seat_thing(B, "music_stool", st)
    K.blackboard(B, -22.8, Y1, 0.0, 0.0, w=2.2, h=1.15, zb=0.9, writing="board_2")
    B.spot("music_teacher", "work", (-23.0, 38.6, 0.0), 0.0)
    cs = []
    for r_, y in enumerate((31.6, 32.9, 34.2)):
        for c_, x in enumerate((-28.0, -26.9, -25.8, -24.7, -23.6)):
            cs.append(K.chair(B, x, y, 0.0, 180.0, kind="school"))
    _seat_thing(B, "music_chair_1", cs[7])
    _seat_thing(B, "music_chair_2", cs[2])
    for i in range(2):
        K.bookshelf(B, -29.2 + i * 1.0, Y0, 0.0, 180.0, w=1.0, h=1.9, levels=4, topic="modern", seed=90 + i)
    B.thing("music_shelf", "shelf", (-28.7, Y0 + 0.2, 1.2), 180.0, topic="music")
    for i, nm in enumerate(("sleeve_0", "sleeve_2", "sleeve_5")):
        K.poster(B, -26.8 + i * 0.75, Y0, 0.0, 180.0, name=nm, w=0.5, zc=1.7)
    t = K.table(B, -22.2, Y0 + 0.4, 0.0, 180.0, w=1.0, d=0.5)
    K.record_player(B, -22.4, Y0 + 0.4, 0.75, 180.0)
    K.radio(B, -21.9, Y0 + 0.4, 0.75, 180.0)
    B.thing("music_player", "machine", (-22.4, Y0 + 0.4, 0.85), 180.0, what="record_player")
    K.plant(B, -29.0, 38.9, 0.0, 0.0, kind="palm", pot="CT_PlasterRed", h=1.0)
    K.wall_clock(B, -26.0, Y1, 0.0, 0.0, zc=2.7, r=0.2)
    K.pendant(B, -25.0, 34.0, H1, drop=0.5, kind="cone")
    B.lamp((-25.0, 34.0, 3.0), lumens=900, colour="#FFF0DC", kind="public", radius=9.0, fixture=False)


def _canteen(B, x0, x1):
    K = kit
    Y0, Y1 = 15.76, 39.94
    n = 0
    for ci, cx in enumerate((22.9, 26.7)):
        for y in (17.8, 20.8, 29.0, 32.0):
            K.table(B, cx, y, 0.0, 90.0, w=2.4, d=0.8, kind="steel")
            K.plates(B, cx, y - 0.4, 0.75, 0.0, n=2)
            K.plates(B, cx, y + 0.4, 0.75, 0.0, n=2)
            b1 = K.bench(B, cx - 0.65, y, 0.0, 90.0, w=2.4, kind="gym")
            b2 = K.bench(B, cx + 0.65, y, 0.0, 270.0, w=2.4, kind="gym")
            if n < 4:
                B.thing("canteen_seat_%d" % (n + 1), "seat", (cx - 0.65, y, 0.45), 90.0, height=0.45)
            n += 1
            B.spot("canteen_bench_%d" % n, "bench", (cx - 0.65, y, 0.0), 90.0)
    cn = K.serving_line(B, 25.0, 36.4, 0.0, 0.0, w=4.4)
    ct = K.table(B, 28.4, 36.4, 0.0, 0.0, w=0.8, d=0.6, kind="steel")           # the cashier's table at the end of the line
    K.cash_register(B, 28.4, 36.4, ct["top"], 0.0)
    B.thing("canteen_counter", "counter", (25.0, 36.2, 1.0), 0.0, shop="school_canteen", role="teacher")
    K.sign_plate(B, 25.0, 36.2, 0.0, 0.0, name="sign_canteen", w=1.6, zc=2.9, hang=True)
    K.kitchen_unit(B, 24.0, Y1 - 0.34, 0.0, 0.0, modules="fridge,cupboard,sink,stove,cupboard")
    K.samovar(B, 22.6, Y1 - 0.4, 0.0 + 0.9, 0.0)
    K.menu_board(B, 27.6, Y1, 0.0, 0.0, name="menu_0", w=0.9, zc=2.0)
    K.menu_board(B, 28.7, Y1, 0.0, 0.0, name="menu_1", w=0.8, zc=2.0)
    B.thing("canteen_menu", "board", (27.6, Y1 - 0.05, 2.0), 0.0, topic="prices")
    K.mosaic_panel(B, 24.0, Y0, 0.0, 180.0, name="tile_3", w=2.0, h=1.2, zc=1.7)       # QA: was over a window
    K.wall_clock(B, 29.7, 28.0, 0.0, 270.0, zc=2.6, r=0.2)
    K.plant(B, 21.0, 38.9, 0.0, 0.0, kind="ficus", pot="CT_PlasterYellow", h=1.2)
    K.plant(B, 29.0, 17.0, 0.0, 0.0, kind="palm", pot="CT_PlasterBlue", h=1.0)
    for y in (20.0, 31.0):
        K.pendant(B, 24.8, y, H1, drop=0.5, kind="cone")
    B.lamp((24.8, 20.0, 3.0), lumens=1000, colour="#FFF0DC", kind="public", radius=10.0, fixture=False)
    B.lamp((24.8, 31.0, 3.0), lumens=1000, colour="#FFF0DC", kind="public", radius=10.0, fixture=False)


def _gym(B):
    K = kit
    X0, X1, Y0, Y1 = -13.7, 13.7, 40.3, 55.7
    box(B.m, "CT_Floor", X0, X1, Y0, Y1, 0.0, 0.02, faces="Z")
    lines = "CT_TileWhite"
    for (a, b, c, d_) in ((-11.0, 11.0, 42.5, 42.6), (-11.0, 11.0, 53.4, 53.5), (-11.0, -10.9, 42.5, 53.5),
                          (10.9, 11.0, 42.5, 53.5), (-0.05, 0.05, 42.5, 53.5)):
        box(B.detail, lines, a, b, c, d_, 0.02, 0.025, faces="Z")
    for (a, b, c, d_) in ((-11.0, -8.0, 45.0, 45.08), (-11.0, -8.0, 50.9, 50.98), (-8.08, -8.0, 45.0, 51.0),
                          (8.0, 11.0, 45.0, 45.08), (8.0, 11.0, 50.9, 50.98), (8.0, 8.08, 45.0, 51.0)):
        box(B.detail, lines, a, b, c, d_, 0.02, 0.025, faces="Z")
    K.basketball_hoop(B, X0, 48.0, 0.0, 90.0)
    K.basketball_hoop(B, X1, 48.0, 0.0, 270.0)
    B.thing("gym_hoop_w", "sport", (X0 + 0.5, 48.0, 3.0), 90.0, what="basketball")
    B.thing("gym_hoop_e", "sport", (X1 - 0.5, 48.0, 3.0), 270.0, what="basketball")
    B.thing("gym_court", "sport", (0.0, 47.0, 0.3), 180.0, what="track")
    st = K.stage(B, 0.0, Y1 - 1.0, 0.0, 0.0, w=6.0, d=2.0, h=0.5)
    B.thing("gym_stage", "screen", (0.0, Y1 - 1.2, 1.0), 0.0, what="stage")
    for i, x in enumerate((-9.2, -8.2, -7.2, 7.2, 8.2, 9.2)):
        K.wall_bars(B, x, Y1, 0.0, 0.0, w=0.9)
    B.thing("gym_wall_bars_w", "sport", (-8.2, Y1 - 0.1, 1.3), 0.0, what="wall_bars")
    B.thing("gym_wall_bars_e", "sport", (8.2, Y1 - 0.1, 1.3), 0.0, what="wall_bars")
    K.wall_clock(B, 0.0, Y1, 0.0, 0.0, zc=5.2, r=0.45)
    for x in (-6.0, 6.0):
        K.vaulting_horse(B, x, 44.0, 0.0, 0.0)
        K.mats(B, x, 45.6, 0.0, 0.0, w=2.0, d=1.2, n=2, mat="CT_PlasterBlue")
    K.mats(B, X0 + 1.0, 53.2, 0.0, 90.0, w=1.2, d=2.0, n=3, mat="CT_PlasterRed")
    for x in (-4.2, 4.2):
        b = K.bench(B, x, Y0 + 0.4, 0.0, 180.0, w=2.2, kind="gym")
    B.thing("gym_bench_w", "seat", (-4.2, Y0 + 0.4, 0.45), 180.0, height=0.45)
    B.thing("gym_bench_e", "seat", (4.2, Y0 + 0.4, 0.45), 180.0, height=0.45)
    K.lockers(B, 9.6, Y0, 0.0, 180.0, n=4, mat="CT_PlasterBlue")
    K.barbell_rack(B, 12.2, 51.5, 0.0, 270.0)
    B.thing("gym_barbell", "sport", (12.2, 51.5, 1.0), 270.0, what="barbell")
    K.exercise_bike(B, -12.2, 41.8, 0.0, 90.0)
    K.exercise_bike(B, -12.2, 43.0, 0.0, 90.0)
    B.thing("gym_bike", "sport", (-12.2, 42.4, 0.9), 90.0, what="exercise_bike")
    K.fire_extinguisher(B, 13.7, 41.2, 0.0, 270.0)
    B.lamp((-6.0, 47.0, 7.4), lumens=1800, colour="#FFF0DC", kind="public", radius=16.0)
    B.lamp((6.0, 47.0, 7.4), lumens=1800, colour="#FFF0DC", kind="public", radius=16.0)


# ---------------------------------------------------------------- second pass: depth (props, new things, spots)
def _bell_piece(m):
    for r, a, b in ((0.055, 0.0, 0.022), (0.047, 0.022, 0.05), (0.034, 0.05, 0.074), (0.02, 0.074, 0.09)):
        prism(m, "CT_Brass", ring(0, 0, r, 10), a, b, top=(r < 0.03), bottom=False)
    prism(m, "CT_Wood", ring(0, 0, 0.011, 6), 0.09, 0.17, top=True, bottom=False)


def _microscope_piece(m):
    box(m, "CT_Frame", -0.07, 0.07, -0.09, 0.09, 0.0, 0.02)
    box(m, "CT_Steel", -0.012, 0.012, 0.05, 0.075, 0.02, 0.29)
    box(m, "CT_Steel", -0.012, 0.012, -0.005, 0.075, 0.26, 0.29)
    prism(m, "CT_Frame", ring(0, -0.005, 0.021, 8), 0.14, 0.3, top=True, bottom=False)
    prism(m, "CT_Steel", ring(0, -0.005, 0.012, 6), 0.3, 0.35, top=True, bottom=False)
    prism(m, "CT_Steel", ring(0, -0.005, 0.01, 6), 0.1, 0.14, top=False, bottom=True)
    box(m, "CT_Steel", -0.05, 0.05, -0.06, 0.04, 0.085, 0.095)
    prism(m, "CT_Glass", ring(0, -0.005, 0.026, 8), 0.04, 0.05, top=True, bottom=False)


def _metronome_piece(m):
    box(m, "CT_Wood", -0.055, 0.055, -0.045, 0.045, 0.0, 0.04)
    box(m, "CT_Wood", -0.044, 0.044, -0.035, 0.035, 0.04, 0.1)
    box(m, "CT_Wood", -0.031, 0.031, -0.024, 0.024, 0.1, 0.18)
    box(m, "CT_Steel", -0.004, 0.004, -0.03, -0.026, 0.06, 0.24)
    box(m, "CT_Brass", -0.012, 0.012, -0.034, -0.026, 0.15, 0.17)


def _scoreboard_piece(m):
    box(m, "CT_Frame", -0.95, 0.95, -0.09, 0.0, 0.0, 0.85)
    box(m, "CT_PlasterGreen", -0.9, 0.9, -0.1, -0.09, 0.05, 0.8)
    for cx in (-0.6, -0.28, 0.28, 0.6):
        for (a, b, c, e) in ((-0.07, -0.04, 0.22, 0.58), (0.04, 0.07, 0.22, 0.58), (-0.07, 0.07, 0.55, 0.58),
                             (-0.07, 0.07, 0.38, 0.41), (-0.07, 0.07, 0.22, 0.25)):
            box(m, "CT_TileWhite", cx + a, cx + b, -0.11, -0.1, c, e)
    box(m, "CT_PlasterYellow", -0.03, 0.03, -0.11, -0.1, 0.52, 0.56)
    box(m, "CT_PlasterYellow", -0.03, 0.03, -0.11, -0.1, 0.3, 0.34)
    box(m, "CT_Steel", -0.5, 0.5, -0.11, -0.095, 0.06, 0.1)


def _duffel_piece(m):
    box(m, "CT_PlasterBlue", -0.3, 0.3, -0.13, 0.13, 0.0, 0.25)
    box(m, "CT_PlasterBlue", -0.26, 0.26, -0.11, 0.11, 0.25, 0.29)
    box(m, "CT_Leather", -0.3, 0.3, -0.135, -0.125, 0.12, 0.17)
    box(m, "CT_Leather", -0.31, -0.28, -0.14, 0.14, 0.0, 0.26)
    box(m, "CT_Leather", 0.28, 0.31, -0.14, 0.14, 0.0, 0.26)
    box(m, "CT_Leather", -0.14, 0.14, -0.04, 0.04, 0.29, 0.31)
    box(m, "CT_TileWhite", -0.12, 0.12, -0.145, -0.135, 0.19, 0.23)
    box(m, "CT_PlasterRed", 0.18, 0.28, 0.05, 0.12, 0.25, 0.3)           # the sole of a sneaker sticks out


def _trolley_piece(m):
    for z in (0.22, 0.78):
        box(m, "CT_Steel", -0.4, 0.4, -0.25, 0.25, z, z + 0.025)
        box(m, "CT_Steel", -0.4, 0.4, -0.25, -0.235, z + 0.025, z + 0.06)
    for sx in (-0.37, 0.37):
        for sy in (-0.22, 0.22):
            box(m, "CT_Steel", sx - 0.012, sx + 0.012, sy - 0.012, sy + 0.012, 0.05, 0.8)
            prism(m, "CT_Rubber", ring(sx, sy, 0.04, 8), 0.0, 0.05, top=True, bottom=False)
    box(m, "CT_Steel", -0.37, 0.37, 0.24, 0.26, 0.98, 1.0)
    box(m, "CT_Steel", -0.38, -0.36, 0.22, 0.26, 0.8, 1.0)
    box(m, "CT_Steel", 0.36, 0.38, 0.22, 0.26, 0.8, 1.0)


def _herbarium_piece(m, k=0):
    leaf = ("CT_PlasterGreen", "CT_PlasterYellow", "CT_Leaf", "CT_PlasterRed")[k % 4]
    box(m, "CT_Wood", -0.2, 0.2, -0.03, 0.0, 2.44, 2.96)
    box(m, "CT_Paper", -0.17, 0.17, -0.034, -0.03, 2.47, 2.93)
    box(m, leaf, -0.006, 0.006, -0.038, -0.034, 2.5, 2.88)
    for i in range(3):
        box(m, leaf, 0.0, 0.1 - 0.02 * i, -0.038, -0.034, 2.6 + i * 0.1, 2.625 + i * 0.1)
        box(m, leaf, -0.1 + 0.02 * i, 0.0, -0.038, -0.034, 2.65 + i * 0.1, 2.675 + i * 0.1)


def _stand_piece(m):
    box(m, "CT_Steel", -0.22, 0.22, -0.02, 0.02, 0.0, 0.02)
    box(m, "CT_Steel", -0.02, 0.02, -0.22, 0.22, 0.0, 0.02)
    box(m, "CT_Steel", -0.01, 0.01, -0.01, 0.01, 0.02, 1.05)
    box(m, "CT_Steel", -0.24, 0.24, -0.05, -0.02, 1.0, 1.04)
    box(m, "CT_Steel", -0.24, 0.24, 0.0, 0.015, 1.0, 1.3)
    box(m, "CT_Paper", -0.2, 0.2, -0.004, 0.0, 1.03, 1.27)


def _bag_piece(m, mat="CT_PlasterRed"):
    box(m, mat, -0.14, 0.14, -0.05, 0.05, 0.0, 0.22)
    box(m, "CT_Leather", -0.15, 0.15, -0.056, -0.04, 0.12, 0.24)
    box(m, "CT_Leather", -0.1, -0.08, -0.06, 0.06, 0.2, 0.22)


def _depth(B):
    """Second pass: the canteen line, the duty desk and the bell, the lost box, the microscope, herbarium, scoreboard and
    the lost bag; new things (rules and acts in tools/city/content/x_p_school.json) and spots for the townsfolk."""
    K = kit
    f = Furn(B)
    # ---------------------------------------------------------------- canteen
    tt = K.table(B, 21.5, 36.2, 0.0, 0.0, w=0.8, d=0.5, kind="steel")           # trays and the tea urn before the line
    K.tray_stack(B, 21.25, 36.2, tt["top"], 0.0, n=6, mat="CT_PlasterRed")
    K.samovar(B, 21.8, 36.2, tt["top"], 0.0)
    B.thing("canteen_tea", "take", (21.8, 36.2, 1.2), 0.0, item="tea")
    f.put(_trolley_piece, 29.2, 23.0, 270.0)
    K.plates(B, 29.2, 22.8, 0.82, 0.0, n=5)
    K.plates(B, 29.2, 23.2, 0.82, 0.0, n=3, mat="CT_PlasterBlue")
    K.plates(B, 29.2, 23.0, 0.27, 0.0, n=6)
    B.thing("canteen_return", "machine", (29.1, 23.0, 0.95), 270.0, what="trolley")
    K.blackboard(B, 27.6, 15.76, 0.0, 180.0, w=1.4, h=0.8, zb=1.0, writing="board_3")
    B.thing("canteen_dayboard", "board", (27.6, 15.9, 1.4), 180.0, topic="menu_day")
    K.poster(B, 21.8, 15.76, 0.0, 180.0, name="poster_health", w=0.6, zc=1.7)
    K.fire_extinguisher(B, 29.6, 16.6, 0.0, 270.0)
    K.pendant(B, 25.0, 35.2, H1, drop=0.5, kind="cone")
    B.lamp((25.0, 35.2, 3.0), lumens=900, colour="#FFE8C8", kind="public", radius=8.0, fixture=False)
    B.spot("canteen_duty", "work", (25.0, 37.6, 0.0), 0.0)
    B.spot("canteen_queue_0", "stand", (23.6, 35.2, 0.0), 180.0)
    B.spot("canteen_queue_1", "stand", (25.4, 35.2, 0.0), 180.0)
    B.spot("canteen_tea_stand", "stand", (21.8, 35.4, 0.0), 180.0)
    B.spot("canteen_return_stand", "stand", (28.4, 23.0, 0.0), 270.0)
    # ---------------------------------------------------------------- entrance hall: duty desk, bell, lost box, concert poster
    dk = K.table(B, 2.9, 7.5, 0.0, 0.0, w=1.0, d=0.5)
    K.chair(B, 2.9, 8.2, 0.0, 0.0, kind="school")
    f.z0 = dk["top"]
    f.put(_bell_piece, 2.55, 7.5, 0.0)
    f.z0 = 0.0
    K.books_stack(B, 3.15, 7.52, dk["top"], 8.0, n=2)
    K.table_lamp(B, 3.4, 7.42, dk["top"], 0.0, light=True)
    B.thing("hall_handbell", "machine", (2.55, 7.5, dk["top"] + 0.1), 0.0, what="handbell")
    B.thing("hall_duty_desk", "board", (3.15, 7.5, dk["top"] + 0.1), 0.0, topic="duty")
    B.spot("hall_duty", "work", (2.9, 8.2, 0.0), 0.0)
    K.shoe_rack(B, -3.0, 15.7, 0.0, 0.0, w=1.0, rows=3, seed=4)
    K.crate(B, -3.0, 15.45, 1.1, 0.0, w=0.5, d=0.3, h=0.25, fill="CT_PlasterBlue", seed=6)
    B.thing("hall_lost_box", "board", (-3.0, 15.4, 0.9), 0.0, topic="lostbox")
    B.spot("hall_lost_stand", "stand", (-3.0, 14.4, 0.0), 180.0)
    K.poster(B, 7.35, 14.5, 0.0, 270.0, name="kids_1", w=0.8, zc=1.6)
    B.thing("hall_concert_poster", "board", (7.3, 14.5, 1.6), 270.0, topic="concert")
    B.thing("hall_coats", "board", (-7.3, 9.1, 1.2), 90.0, topic="coats")
    # ---------------------------------------------------------------- classroom: timetable, globe, satchels
    K.notice_board(B, -18.6, 7.1, 0.0, 90.0, w=0.8, zc=1.55, seed=2)
    B.thing("class_timetable", "board", (-18.5, 7.1, 1.55), 90.0, topic="lessons")
    B.thing("class_globe", "machine", (-12.8, 12.9, 1.15), 0.0, what="globe")
    for i, (bx, by, bm) in enumerate(((-14.55, 7.15, "CT_PlasterRed"), (-11.65, 8.6, "CT_PlasterBlue"), (-17.2, 10.0, "CT_PlasterGreen"),
                                      (-9.0, 11.4, "CT_Leather"), (-14.5, 11.1, "CT_PlasterYellow"))):
        f.put(lambda m, mat=bm: _bag_piece(m, mat), bx, by, 90.0 * (i % 3))
    # ---------------------------------------------------------------- biology room: microscope, herbarium, second lamp
    f.z0 = 0.91
    f.put(_microscope_piece, -24.3, 17.15, 180.0)
    f.z0 = 0.0
    B.thing("bio_microscope", "machine", (-24.3, 17.15, 1.15), 180.0, what="microscope")
    for k, hx in enumerate((-28.2, -27.2, -26.2, -25.2)):
        f.put(lambda m, kk=k: _herbarium_piece(m, kk), hx, 27.85, 0.0)
    B.thing("bio_herbarium", "board", (-26.7, 27.78, 2.7), 0.0, topic="herbarium")
    K.pendant(B, -25.0, 24.6, H1, drop=0.5, kind="cone")
    B.lamp((-25.0, 24.6, 3.0), lumens=1400, colour="#FFF0DC", kind="public", radius=11.0, fixture=False)
    B.spot("bio_scope_stand", "stand", (-24.3, 18.2, 0.0), 0.0)
    # ---------------------------------------------------------------- music room: stands, metronome, lamp
    f.z0 = 1.25
    f.put(_metronome_piece, -26.55, 39.7, 0.0)
    f.z0 = 0.0
    K.table_lamp(B, -25.5, 39.7, 1.25, 0.0, light=True)
    B.thing("music_metronome", "machine", (-26.55, 39.7, 1.4), 0.0, what="metronome")
    for x in (-27.4, -25.8, -24.2):
        f.put(_stand_piece, x, 35.0, 180.0)
    B.thing("music_blackboard", "craft", (-22.8, 39.85, 1.5), 0.0, what="blackboard")
    B.spot("music_stand_spot", "stand", (-26.55, 38.6, 0.0), 0.0)
    # ---------------------------------------------------------------- gym: scoreboard, the lost bag
    f.z0 = 3.6
    f.put(_scoreboard_piece, -13.7, 44.5, 90.0)
    f.z0 = 0.0
    B.thing("gym_scoreboard", "machine", (-13.5, 44.5, 4.0), 90.0, what="scoreboard")
    f.put(_duffel_piece, -13.0, 55.2, 20.0)
    B.thing("gym_bag", "craft", (-13.0, 55.2, 0.2), 20.0, what="bag")
    B.spot("gym_whistle", "work", (0.0, 41.6, 0.0), 180.0)
    B.spot("gym_bag_stand", "stand", (-12.2, 53.9, 0.0), 270.0)


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
    # ---- hall benches
    for x in (-5.5, 5.5):
        box(d, "CT_Wood", x - 1.0, x + 1.0, 14.6, 15.05, 0.42, 0.47)
        for u in (x - 0.9, x + 0.8):
            box(d, "CT_Steel", u, u + 0.1, 14.65, 15.0, 0.0, 0.42)
        B.spot("hall_bench_%d" % (x > 0), "bench", (x, 14.8, 0.0), 180.0)
    _furnish(B, IY0, IY1)
    _classroom(B, IY1)
    _staff_room(B)
    _hall_things(B)
    _roof(m, -30, 30, SY0, SY1)
    # ================================================================== west and east wings
    for sgn in (-1, 1):
        xo, xi = 30 * sgn, 20 * sgn
        mx0, mx1 = sorted((xo - sgn * 0.3, xi + sgn * 0.3))
        box(m, "CT_Plaster", mx0, mx1, 40.0, 49.7, 0.0, H2)
        mat = "CT_Plaster"
        door_y = [22.0, 34.0] if sgn < 0 else [26.0]
        x0, x1 = _wing_rooms(B, sgn, door_y)                  # ground floor hollow: biology + music (W), canteen (E)
        du = [(y - 16.0) if sgn < 0 else (40.0 - y) for y in door_y]
        dskip = tuple(int(u // 4) for u in du)
        dh = [(u - 1.0, u + 1.0, 0.0, 2.4) for u in du]
        if sgn < 0:
            _ribbon(B, (xo, 50.0), (xo, SY0), mat, step=3.7, acc="CT_PlasterBlue")          # outer, faces -x
            _ribbon(B, (xi, SY1), (xi, 40.0), mat, acc="CT_PlasterYellow", skip_ground=dskip, doors=dh)   # courtyard side
            B.entrance("bio", (xi, 22.0, 0.0), 90.0, width=2.0, height=2.4, kind="street", leaves=1)
            B.entrance("music", (xi, 34.0, 0.0), 90.0, width=2.0, height=2.4, kind="street", leaves=1)
            _bio_room(B, x0, x1)
            _music_room(B, x0, x1)
        else:
            _ribbon(B, (xo, SY0), (xo, 50.0), mat, step=3.7, acc="CT_PlasterBlue")
            _ribbon(B, (xi, 40.0), (xi, SY1), mat, acc="CT_PlasterYellow", skip_ground=dskip, doors=dh)
            B.entrance("canteen", (xi, 26.0, 0.0), 270.0, width=2.0, height=2.4, kind="street", leaves=1)
            _canteen(B, x0, x1)
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
    box(m, "CT_Brick", -14.0, -13.7, 40.3, 50.0, 0.0, HZ)                   # gym side walls shared with the wings
    box(m, "CT_Brick", 13.7, 14.0, 40.3, 50.0, 0.0, HZ)
    hh = []
    n = 7
    w = 28 / n
    for i in range(n):
        hh.append((i * w + 0.5, (i + 1) * w - 0.5, 4.5, 7.5))
    # courtyard front (faces -y), from x -14 to 14; a tall door-less glazed foyer strip + high windows
    FHf = B.facade((-14.0, 40.0), (14.0, 40.0), 0.0, HZ, "CT_Brick", holes=hh + [(12.8, 15.2, 0.0, 2.4)], depth=0.3, reveal_mat="CT_ConcreteWhite", back=False)
    for h in hh:
        B.facade_window(FHf, *h, kind="public", cols=3, inset=0.12)
    B.entrance("gym", (0.0, 40.0, 0.0), 0.0, width=2.4, height=2.4, kind="street", leaves=2)
    # hall sides y 50..56 and back y=56
    F = B.facade((-14.0, 56.0), (-14.0, 50.0), 0.0, HZ, "CT_Brick", holes=[(1.0, 2.8, 4.5, 7.5), (3.6, 5.4, 4.5, 7.5)], depth=0.3, reveal_mat="CT_ConcreteWhite", back=False)
    for h in [(1.0, 2.8, 4.5, 7.5), (3.6, 5.4, 4.5, 7.5)]:
        B.facade_window(F, *h, kind="public", cols=2, inset=0.12)
    F = B.facade((14.0, 50.0), (14.0, 56.0), 0.0, HZ, "CT_Brick", holes=[(0.6, 2.4, 4.5, 7.5), (3.2, 5.0, 4.5, 7.5)], depth=0.3, reveal_mat="CT_ConcreteWhite", back=False)
    for h in [(0.6, 2.4, 4.5, 7.5), (3.2, 5.0, 4.5, 7.5)]:
        B.facade_window(F, *h, kind="public", cols=2, inset=0.12)
    bk = [(i * 4.0 + 0.5, (i + 1) * 4.0 - 0.5, 4.5, 7.5) for i in range(7)]
    F = B.facade((14.0, 56.0), (-14.0, 56.0), 0.0, HZ, "CT_Brick", holes=bk, depth=0.3, reveal_mat="CT_ConcreteWhite", back=False)
    for h in bk:
        B.facade_window(F, *h, kind="public", cols=2, inset=0.12)
    # gym interior skins (brick), then the furniture
    _skin_y(B, 40.3, -13.7, 13.7, [(-14 + i * 4 + 0.5, -14 + (i + 1) * 4 - 0.5, 4.5, 7.5) for i in range(7)] + [(-1.2, 1.2, 0.0, 2.4)], False, z1=HZ)
    _skin_y(B, 55.7, -13.7, 13.7, [(14 - (i + 1) * 4 + 0.5, 14 - i * 4 - 0.5, 4.5, 7.5) for i in range(7)], True, z1=HZ)
    _skin_x(B, -13.7, 40.3, 55.7, [(56 - 5.4, 56 - 3.6, 4.5, 7.5), (56 - 2.8, 56 - 1.0, 4.5, 7.5)], True, mat="CT_Brick", z1=HZ)
    _skin_x(B, 13.7, 40.3, 55.7, [(50 + 0.6, 50 + 2.4, 4.5, 7.5), (50 + 3.2, 50 + 5.0, 4.5, 7.5)], False, mat="CT_Brick", z1=HZ)
    _gym(B)
    _depth(B)
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
