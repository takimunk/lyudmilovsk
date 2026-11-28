"""The hero's own room in the club (docs/hero_room.md, backlog item 19).

Not a town building: a small room dug into the hill beside the club's hall, behind a doorway in the hall's left wall,
opposite the owner's booth. It is built with the town's kit so that its furniture, its bed and its things work like every
other interior (docs/city_interiors.md), but nothing places it on a plot: the game spawns its meshes at the club
(ULyudHeroRoom, tools/home/home.py writes where), and tools/city/extra_places.json gives its things to interiors.py.

Frame: origin = the middle of the doorway's threshold on the HALL's face of the wall, x along that wall (to the right
seen from the hall, toward the corridor), y into the room, z 0 = the hall's floor. The hall's wall is 0.5 m thick
(y 0..0.5): the module lines the opening with a wooden casing and builds the room behind it.

    x -3.6 .. 0.9, y 0.56 .. 3.7 (4.5 x 3.1 m), ceiling 2.6 m; the door x -0.6 .. 0.6, 2.2 m high.

What stands there: a bed in the far left corner (the hero sleeps until an hour he chooses), a bedside table with a
lamp, a desk with a chair and a record player, a pin board over the desk, open shelves and a wall shelf, a radiator, a
rug, a map of the coast, a clock, a small table with a hot plate (cooking: docs/kitchen.md), an empty flower pot in
the corner and Maru's basket by the door.

Places for the hero's own things (things with `place`; ULyudHeroRoom puts what he owns on them, docs/hero_room.md):
    pin_1..6    cards on the pin board (postcards, prints)
    frame_1..3  nails for something framed (the diploma, a big print)
    shelf_1..6  the open shelves; top_1..2 the wall shelf (cups, records, books, finds)
    pot         the flower pot (a plant that grows)
Requests: none.
"""
import math

from citykit import box
import interior_kit as kit

INFO = {
    "name_ru": "Комната героя", "name_en": "The hero's room",
    "style": "a room of one's own", "use": "residential",
    "inspired_by": "a caretaker's room behind a house-of-culture foyer; a student's corner",
}

X0, X1 = -3.6, 0.9
Y0, Y1 = 0.56, 3.7          # the room's inner faces (the hall's wall is y 0 .. 0.5, its lining 0.5 .. 0.56)
H = 2.6
DOOR_HW, DOOR_H = 0.6, 2.2  # the hole in the hall's wall (ULyudHeroRoom cuts the same one: Content/Home/home.json)
CASE = 0.04                 # the casing's boards: clear opening 1.12 x 2.16 (the standard asks 1.08 x 2.10)

WALL, FLOOR, CEIL = "CT_Interior", "CT_Floor", "CT_ConcreteWhite"


def _shell(B):
    m, d = B.m, B.detail
    box(m, FLOOR, X0 - 0.2, X1 + 0.2, 0.5, Y1 + 0.2, -0.2, 0.0, faces="Z")
    box(m, CEIL, X0 - 0.2, X1 + 0.2, 0.5, Y1 + 0.2, H, H + 0.2, faces="z")
    box(m, WALL, X0 - 0.2, X1 + 0.2, Y1, Y1 + 0.2, 0.0, H, faces="y")
    box(m, WALL, X0 - 0.2, X0, 0.5, Y1, 0.0, H, faces="X")
    box(m, WALL, X1, X1 + 0.2, 0.5, Y1, 0.0, H, faces="x")
    # the lining on the back of the hall's wall, round the door
    box(m, WALL, X0, -DOOR_HW, 0.5, Y0, 0.0, H, faces="YX")
    box(m, WALL, DOOR_HW, X1, 0.5, Y0, 0.0, H, faces="Yx")
    box(m, WALL, -DOOR_HW, DOOR_HW, 0.5, Y0, DOOR_H, H, faces="Yz")
    # a painted skirting and a picture rail all round (detail: nothing to bump into)
    for (ax, ay, bx, by) in ((X0, Y1 - 0.015, X1, Y1), (X0, Y0, X0 + 0.015, Y1), (X1 - 0.015, Y0, X1, Y1),
                             (X0, Y0, -DOOR_HW - 0.09, Y0 + 0.015), (DOOR_HW + 0.09, Y0, X1, Y0 + 0.015)):
        box(d, "CT_Wood", ax, bx, ay, by, 0.0, 0.09)
        box(d, "CT_WoodLight", ax, bx, ay, by, 2.02, 2.05)
    # the casing through the wall: jambs, head, a threshold board; architraves on both faces
    y0, y1 = -0.02, Y0 + 0.02
    box(m, "CT_Wood", -DOOR_HW, -DOOR_HW + CASE, y0, y1, 0.0, DOOR_H)
    box(m, "CT_Wood", DOOR_HW - CASE, DOOR_HW, y0, y1, 0.0, DOOR_H)
    box(m, "CT_Wood", -DOOR_HW, DOOR_HW, y0, y1, DOOR_H - CASE, DOOR_H)
    box(m, "CT_Wood", -DOOR_HW + CASE, DOOR_HW - CASE, 0.0, Y0, 0.0, 0.012, faces="xXyYZ")
    for ya, yb in ((-0.03, 0.0), (Y0, Y0 + 0.03)):
        box(d, "CT_Wood", -DOOR_HW - 0.08, -DOOR_HW, ya, yb, 0.0, DOOR_H + 0.08)
        box(d, "CT_Wood", DOOR_HW, DOOR_HW + 0.08, ya, yb, 0.0, DOOR_H + 0.08)
        box(d, "CT_Wood", -DOOR_HW, DOOR_HW, ya, yb, DOOR_H, DOOR_H + 0.08)


def _pin_board(B, x, zc, w, h):
    """A cork board in a light wooden frame on the far wall; returns the cards' places (2 rows of 3)."""
    p = kit._P(B, x, Y1, 0.0, 0.0, "pin_board")
    p.kind = "wall"
    p.bev("CT_WoodLight", -w / 2 - 0.04, w / 2 + 0.04, -0.03, 0.0, zc - h / 2 - 0.04, zc + h / 2 + 0.04, 0.008)
    p.box("CT_Leather", -w / 2, w / 2, -0.034, -0.03, zc - h / 2, zc + h / 2, f="y")
    p.done()
    return [(x + (c - 1) * w / 3.0, Y1 - 0.045, zc + (0.5 - r) * h / 2.0) for r in range(2) for c in range(3)]


def _shelves(B, x, w=1.3, d=0.3, tops=(0.42, 0.84, 1.26)):
    """Shelves against the wall the door is in (facing +y): a low cupboard with two doors, and two open boards over it
    between the uprights. Returns the places on the cupboard's top and the boards, two each."""
    p = kit._P(B, x, Y0, 0.0, 180.0, "open_shelves")
    t = 0.025
    base, top = tops[0], tops[-1]
    p.box("CT_Wood", -w / 2, -w / 2 + t, -d, 0.0, 0.0, top, f="xXyYZ")
    p.box("CT_Wood", w / 2 - t, w / 2, -d, 0.0, 0.0, top, f="xXyYZ")
    p.box("CT_WoodLight", -w / 2 + t, w / 2 - t, -0.012, 0.0, base, top, f="y")
    # the cupboard: a plinth, two doors with brass knobs
    p.box("CT_Wood", -w / 2 + t, w / 2 - t, -d + 0.02, 0.0, 0.0, 0.06, f="y")
    for s in (-1, 1):
        a, b = sorted((s * 0.006, s * (w / 2 - t - 0.004)))
        p.bev("CT_WoodLight", a, b, -d, -d + 0.018, 0.065, base - t - 0.004, 0.004)
        p.box("CT_Brass", s * 0.05 - 0.008, s * 0.05 + 0.008, -d - 0.014, -d, 0.27, 0.286, f="xXyZz")
    for z in tops:
        over = 0.02 if z == top else -t
        p.bev("CT_Wood", -w / 2 - over, w / 2 + over, -d - (0.015 if z == top else 0.0), 0.0, z - t, z, 0.005, bottom=True)
    p.core(-w / 2 + 0.01, w / 2 - 0.01, -d + 0.025, -0.005, 0.01, base - 0.03)
    p.done(solid=True, top=top)
    # yaw 180: the piece's local x runs toward -x of the room, its front (-y local) looks into the room (+y)
    return [(x - s * w / 4.0, Y0 + d * 0.5, z) for z in tops for s in (-1, 1)]


def _basket(B, x, y):
    """Maru's basket: a low woven oval with a folded blanket."""
    p = kit._P(B, x, y, 0.0, 0.0, "basket")
    ring = lambda rx, ry, z: [(rx * math.cos(2 * math.pi * k / 12), ry * math.sin(2 * math.pi * k / 12), z) for k in range(12)]
    p.loft("CT_WoodLight", [ring(0.3, 0.22, 0.0), ring(0.36, 0.27, 0.1), ring(0.37, 0.28, 0.2)], top=False)
    p.loft("CT_Wood", [ring(0.37, 0.28, 0.2), ring(0.385, 0.295, 0.215), ring(0.35, 0.26, 0.215)], top=False)
    p.loft("CT_WoodLight", [ring(0.35, 0.26, 0.215), ring(0.33, 0.245, 0.1), ring(0.28, 0.2, 0.05)], top=False)
    kit._soft(p, "CT_PlasterRed", -0.25, 0.25, -0.18, 0.18, 0.04, 0.13, 0.02, 0.06, 2, 0.7)
    p.done(footprint=(-0.38, 0.38, -0.29, 0.29))


def _pot(B, x, y):
    p = kit._P(B, x, y, 0.0, 0.0, "pot")
    p.lathe("CT_PlasterRed", 0, 0, [(0.2, 0.0), (0.2, 0.012), (0.12, 0.02)], 10, top=True, a0=math.pi / 10)   # the saucer
    p2 = kit._P(B, x, y, 0.015, 0.0, "pot")
    kit._pot(p2, "CT_PlasterRed", 0.16, 0.28)
    p.done()
    p2.done()
    return 0.015 + 0.28


def _hot_plate(B, x, y, z, yaw):
    """A one-ring electric hot plate: a white enamel body on rubber feet, the dark ring, a red knob, a cord (0.26 x 0.26 x 0.08)."""
    p = kit._P(B, x, y, z, yaw, "hot_plate")
    p.kind = "prop"
    p.bev("CT_SteelWhite", -0.13, 0.13, -0.13, 0.13, 0.012, 0.07, b=0.008, c=0.02)
    for sx in (-0.1, 0.1):
        for sy in (-0.1, 0.1):
            p.cyl("CT_Rubber", sx, sy, 0.0, 0.012, 0.012, 5)
    p.cyl("CT_Steel", 0.0, 0.01, 0.07, 0.076, 0.095, 12)
    p.cyl("CT_ConcreteDark", 0.0, 0.01, 0.076, 0.082, 0.085, 12)
    p.cyl("CT_Steel", 0.0, 0.01, 0.082, 0.084, 0.03, 8)
    p.cyl("CT_SteelRed", 0.085, -0.134, 0.03, 0.05, 0.014, 6, top=True)
    p.rod("CT_Rubber", (-0.09, 0.13, 0.03), (-0.2, 0.2, 0.004), 0.008, 3)
    return p.done(top=0.084)


def build(B):
    _shell(B)
    R = kit.Room(B, (X0, Y0, X1, Y1), seed=19)
    R.keep_door((0.0, Y0), 0.0, w=1.4, d=1.5)

    # ---- the bed in the far left corner, its head to the far wall
    bx, by = X0 + 0.52, Y1 - 1.06
    R.place(kit.bed, bx, by, 0.0, size="single", blanket="CT_PlasterGreen")
    B.thing("bed", "bed", (bx, by, 0.5), 0.0, height=0.5, rest="home")
    R.place(kit.bedside_table, bx + 0.78, Y1 - 0.24, 0.0, light=True)

    # ---- the desk under the pin board, its chair, the record player
    dx, dy = -0.48, Y1 - 0.36
    R.place(kit.table, dx, dy, 0.0, w=1.35, d=0.66, h=0.75, mat="CT_Wood")
    R.place(kit.chair, dx - 0.15, dy - 0.62, 180.0, kind="wood", seed=2)
    B.thing("desk_chair", "seat", (dx - 0.15, dy - 0.62, 0.45), 180.0, height=0.45)
    kit.record_player(B, dx + 0.38, dy + 0.06, 0.75, 0.0)
    B.thing("record_player", "machine", (dx + 0.38, dy + 0.06, 0.85), 0.0, what="record_player")
    kit.books_stack(B, dx - 0.5, dy + 0.12, 0.75, 20.0, n=3, seed=5)
    kit.table_lamp(B, dx - 0.52, dy - 0.12, 0.75, 0.0, shade="CT_PlasterGreen")
    for k, pos in enumerate(_pin_board(B, dx, 1.62, 1.35, 0.84)):
        B.thing("pin_%d" % (k + 1), "board", pos, 0.0, place="card")

    # ---- between the bed and the desk: the radiator, the wall shelf for cups, the clock
    kit.radiator(B, -1.72, Y1, 0.0, 0.0, w=0.9, h=0.55)
    kit.wall_shelf(B, -1.72, Y1, 0.0, 0.0, w=0.9, zt=1.42, props=False)
    for k, sx in enumerate((-0.24, 0.24)):
        B.thing("top_%d" % (k + 1), "shelf", (-1.72 + sx, Y1 - 0.11, 1.42), 0.0, place="shelf")
    kit.wall_clock(B, -1.72, Y1, 0.0, 0.0, zc=2.2)

    # ---- the wall the door is in: open shelves, Maru's basket, the switch
    for k, pos in enumerate(_shelves(B, -2.55)):
        B.thing("shelf_%d" % (k + 1), "shelf", pos, 180.0, place="shelf")
    _basket(B, -1.25, Y0 + 0.36)
    B.thing("basket", "board", (-1.25, Y0 + 0.36, 0.2), 180.0, topic="basket")
    sp = kit._P(B, DOOR_HW + 0.19, Y0, 0.0, 180.0, "switch")
    sp.kind = "wall"
    sp.bev("CT_Paper", -0.04, 0.04, -0.012, 0.0, 1.26, 1.34, 0.004)
    sp.box("CT_Lacquer", -0.012, 0.012, -0.02, -0.012, 1.285, 1.315, f="xXyZz")
    sp.done()
    B.thing("switch", "machine", (DOOR_HW + 0.19, Y0 + 0.02, 1.3), 180.0, what="home_switch")

    # ---- nails for frames: two over the bed, one on the right wall; the map of the coast beside it
    for k, fy in enumerate((by - 0.55, by + 0.35)):
        B.thing("frame_%d" % (k + 1), "board", (X0 + 0.02, fy, 1.6), 90.0, place="frame")
    B.thing("frame_3", "board", (X1 - 0.02, 2.75, 1.6), 270.0, place="frame")
    for (nx, ny, yaw) in ((X0, by - 0.55, 90.0), (X0, by + 0.35, 90.0), (X1, 2.75, 270.0)):
        n = kit._P(B, nx, ny, 0.0, yaw, "nail")
        n.kind = "wall"
        n.rod("CT_Brass", (0.0, 0.0, 1.78), (0.0, -0.02, 1.785), 0.008, 4, caps=True)
        n.done()
    kit.poster(B, X1, 1.55, 0.0, 270.0, name="map_coast", w=0.8, zc=1.55)
    B.thing("coast_map", "board", (X1 - 0.02, 1.55, 1.55), 270.0, topic="home_map")

    # ---- the kitchen corner on the right wall, past the map: a small table, the hot plate (the cooking's own act
    # "hot_plate" finds it by what=hot_plate, docs/kitchen.md), a kettle's cup and a plate
    kx, ky = X1 - 0.27, 2.36
    kt = R.place(kit.table, kx, ky, 270.0, w=0.72, d=0.46, h=0.72, mat="CT_WoodLight")
    _hot_plate(B, kx + 0.02, ky - 0.14, kt["top"], 270.0)
    B.thing("hot_plate", "machine", (kx + 0.02, ky - 0.14, kt["top"] + 0.1), 270.0, what="hot_plate")
    kit.cup(B, kx - 0.05, ky + 0.2, kt["top"], 0.0)
    kit.plates(B, kx + 0.08, ky + 0.22, kt["top"], 0.0, n=2)

    # ---- the flower pot in the far right corner
    pz = _pot(B, X1 - 0.34, Y1 - 0.34)
    B.thing("pot", "craft", (X1 - 0.34, Y1 - 0.34, pz), 0.0, place="pot")

    # ---- the middle: a rug, the lamp over it
    kit.rug(B, -1.15, 1.9, 0.0, 90.0, w=1.5, d=2.3, name="rug", seed=7)
    kit.pendant(B, -1.3, 2.05, H, drop=0.5, kind="cone", light=True, lumens=850, colour="#FFD9A8", radius=6.5)

    B.entrance("door", (0.0, 0.0, 0.0), 0.0, width=2 * (DOOR_HW - CASE), height=DOOR_H - CASE, kind="street")
    B.spot("stand", "stand", (0.0, 1.4, 0.0), 180.0)
    B.footprint = [(X0 - 0.2, 0.0), (X1 + 0.2, 0.0), (X1 + 0.2, Y1 + 0.2), (X0 - 0.2, Y1 + 0.2)]
    B.height = H + 0.2
    B.note("door hole in the hall's wall: x -%.2f..%.2f, z 0..%.2f" % (DOOR_HW, DOOR_HW, DOOR_H))
