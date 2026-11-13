"""Promenade unit: a small shop or café room behind the promenade's seawall W1 (docs/city_plan.md, city_interiors.md).

The 32 units of W1 (city_plan.json promenade_units, ids W1-U01..) are bays of the wall's face 10 m wide with a door and a
shop window (tools/city_mesh.py draws the face; the doorway is open, the window keeps its glass card). This type is ONLY
the room behind that face, placed by unreal/scripts/import_city.py at each unit's "front" (the bay's middle on the face,
promenade level) with the unit's yaw, as a building on a plot:

  * the frame: x along the face (to the right of someone on the promenade looking at it), y into the wall, z 0 = the
    promenade (-39.5). The wall's face is y 0, its reveals 0.28 deep: the room's front wall stands behind them (y 0.28..0.40)
    with the same two holes, door x 2.8..4.0 (2.3 high), window x -4.0..2.0 (sill 0.45, head 2.65).
  * the room: 9.7 x 6.0 m inside (x -4.85..4.85, y 0.40..6.40), 2.98 m clear (floor top 0.02, ceiling 3.0, slab to 3.25
    under the block's top at 3.32; T1 is only 3.3 m over the promenade). The landscape under it stays at the promenade's
    level and steps up to T1 behind it (city_plan W1_RAMP_START, the block is 9.5 m deep: W1_DEPTH).
  * a signboard over the shop window outside (y -0.02, in front of the wall's face) where the atlas has the trade's word.

VARIANTS: one dressing per trade (the unit's "variant" in the plan, city_plan.UNIT_VARIANTS): cafe, fish, rental, workshop,
dumplings, souvenirs, bakery, photo, barber, books, beer, pasty -> promenade_unit__<variant>.json/.glb.
Each: things for the hero (B.thing: the counter with its shop or service and the staff role who serves, seats, shelves,
boards, the trade's machine), spots for the life sim (work spots "staff_<role>[_n]" where each role stands, "counter"
where a customer is served, "stand"/"seat" for visitors), one real lamp (a lit pendant) and unlit fixtures.
Collision: the shell (floor, walls, ceiling) and the kit's hidden cores of counters, shelves and tables.
"""
import math
import os

from citykit import box
import interior_kit as kit
from interior_kit import _P, WOOD, WOODL, STEEL, ALU, BRASS, LAQ

INFO = {
    "name_ru": "Лавка в подпорной стене", "name_en": "Promenade unit",
    "style": "1960s seaside shop fit-out", "use": "public",
    "inspired_by": "the small shops and cafés in the arcades of Soviet Black Sea and Baltic embankments",
}

VARIANT = "cafe"
VARIANTS = {v: {"VARIANT": v} for v in ("cafe", "fish", "rental", "workshop", "dumplings", "souvenirs", "bakery", "photo",
                                        "barber", "books", "beer", "pasty")}

XL, XR = -4.85, 4.85          # inside faces of the side walls
YF, YB = 0.40, 6.40           # inside faces of the front and back walls
REVEAL = 0.28                 # the wall's reveals (tools/city_mesh.py REVEAL): the room starts behind them
CEIL = 3.0
FLOOR = 0.02
DOOR = (2.8, 4.0, 2.3)        # x0, x1, head (city_plan.UNIT_OPENINGS door: u -3.4, 1.2 wide; x = -u)
WIN = (-4.0, 2.0, 0.45, 2.65)
DOOR_X = 3.4

# per variant: walls, dado (lower 1.3 m of the walls), floor
STYLE = {
    "cafe": ("CT_PlasterBlue", "CT_TileWhite", "CT_Floor"),
    "fish": ("CT_Plaster", "CT_Tile", "CT_Floor"),
    "rental": ("CT_PlasterYellow", "CT_WoodLight", "CT_Floor"),
    "workshop": ("CT_PlasterGrey", "CT_PlasterGreen", "CT_ConcreteDark"),
    "dumplings": ("CT_Plaster", "CT_TileWhite", "CT_Floor"),
    "souvenirs": ("CT_PlasterPink", "CT_WoodLight", "CT_Floor"),
    "bakery": ("CT_PlasterYellow", "CT_TileWhite", "CT_Floor"),
    "photo": ("CT_Plaster", "CT_PlasterGrey", "CT_Floor"),
    "barber": ("CT_PlasterGreen", "CT_TileWhite", "CT_Floor"),
    "books": ("CT_PlasterGreen", "CT_Wood", "CT_Floor"),
    "beer": ("CT_Brick", "CT_Wood", "CT_Floor"),
    "pasty": ("CT_PlasterPink", "CT_TileWhite", "CT_Floor"),
}
# the signboard over the shop window (atlas sign_* cells; none for trades the atlas has no word for)
SIGN = {"cafe": "sign_buffet", "beer": "sign_buffet", "dumplings": "sign_canteen", "pasty": "sign_canteen",
        "bakery": "sign_bread", "books": "sign_books", "barber": "sign_barber", "photo": "sign_photo"}


# ------------------------------------------------------------------------------------------------ the room
def _shell(B, wall, dado, floor):
    m, d = B.m, B.detail
    x0, x1 = XL - 0.1, XR + 0.1
    box(m, floor, x0, x1, REVEAL, YB + 0.15, -0.2, FLOOR, faces="Z")
    box(m, "CT_Plaster", x0, x1, REVEAL, YB + 0.15, CEIL, CEIL + 0.25)
    box(m, wall, x0, XL, REVEAL, YB + 0.15, FLOOR, CEIL)
    box(m, wall, XR, x1, REVEAL, YB + 0.15, FLOOR, CEIL)
    box(m, wall, XL, XR, YB, YB + 0.15, FLOOR, CEIL)
    # the front wall behind the face's reveals, with the door and the window
    holes = [(DOOR[0] - x0, DOOR[1] - x0, 0.0, DOOR[2]), (WIN[0] - x0, WIN[1] - x0, WIN[2], WIN[3])]
    B.facade((x0, REVEAL), (x1, REVEAL), 0.0, CEIL, wall, holes=holes, depth=YF - REVEAL, reveal_mat=wall, back=True)
    # the dado and a skirting (1 cm proud of the walls) and the window's inner sill
    zt = 1.3
    box(d, dado, XL, XL + 0.01, YF, YB, FLOOR, zt, faces="X")
    box(d, dado, XR - 0.01, XR, YF, YB, FLOOR, zt, faces="x")
    box(d, dado, XL, XR, YB - 0.01, YB, FLOOR, zt, faces="y")
    for a, b in ((XL, WIN[0]), (WIN[1], DOOR[0]), (DOOR[1], XR)):
        box(d, dado, a, b, YF, YF + 0.01, FLOOR, zt, faces="Y")
    box(d, dado, WIN[0], WIN[1], YF, YF + 0.01, FLOOR, WIN[2], faces="Y")
    box(d, WOOD, WIN[0] - 0.05, WIN[1] + 0.05, REVEAL, YF + 0.08, WIN[2] - 0.04, WIN[2] + 0.005, faces="yYxXZ")
    box(d, "CT_ConcreteDark", DOOR[0], DOOR[1], REVEAL, YF, -0.04, FLOOR + 0.002, faces="Z")   # the threshold strip
    B.entrance("door", (DOOR_X, 0.0, 0.0), 0.0, width=DOOR[1] - DOOR[0], height=DOOR[2], kind="street", leaves=0)
    B.footprint = [(x0, 0.0), (x1, 0.0), (x1, YB + 0.15), (x0, YB + 0.15)]
    B.height = CEIL + 0.25


def _sign(B, variant):
    name = SIGN.get(variant)
    if name:
        kit.sign_plate(B, (WIN[0] + WIN[1]) / 2, -0.004, 0.0, 0.0, name=name, w=2.2, zc=2.97)


def _lamp(B, x, y, lumens=1500, kind="opal"):
    kit.pendant(B, x, y, CEIL, drop=0.55, kind=kind, light=True, lumens=lumens, radius=8.0)


def _seat(B, tid, r, h=None, **props):
    x, y, z, yw = r["seats"][0]
    B.thing(tid, "seat", (x, y, z), yw, height=round(h if h is not None else z, 2), **props)


def _chairs(B, x, y, w, d, per_side, prefix, n_things=1, **kw):
    """Chairs round a table with a seat spot each and n_things seat things."""
    cs = kit.around_table(B, x, y, w, d, 0.0, per_side=per_side, **kw)
    for k, c in enumerate(cs):
        sx, sy, sz, syw = c["seats"][0]
        B.spot("%s_seat%d" % (prefix, k), "seat", (sx, sy, 0.0), syw)
        if k < n_things:
            _seat(B, "%s_chair%d" % (prefix, k + 1), c)
    return cs


# ------------------------------------------------------------------------------------------------ own pieces
def _loaf_row(p, x0, x1, y0, y1, z, mats, step=0.15, round_=False):
    x = x0 + 0.02
    k = 0
    while x + step <= x1 + 1e-6:
        mt = mats[k % len(mats)]
        if round_:
            r = step * 0.42
            p.lathe(mt, x + step / 2, (y0 + y1) / 2, [(r, z), (r * 0.95, z + 0.04), (r * 0.5, z + 0.08), (0.0, z + 0.09)], 6)
        else:
            p.bev(mt, x + 0.008, x + step - 0.008, y0, y1, z, z + 0.11 + 0.02 * (k % 2), 0.04)
        x += step
        k += 1


def bread_rack(B, x, y, z=0.0, yaw=0.0, w=2.4, levels=4):
    """[wall] Open wooden bread racks with loaves (batons, rye bricks) and round loaves on top."""
    p = _P(B, x, y, z, yaw, "bread_rack")
    p.kind = "wall"
    d, h = 0.42, 1.9
    p.box(WOODL, -w / 2, w / 2, -0.015, 0.0, 0.0, h, f="y")
    for s in (-1, 1):
        xa = s * (w / 2 - 0.015)
        p.box(WOODL, xa - 0.015, xa + 0.015, -d, 0.0, 0.0, h, f="xXyZ")
    for k in range(levels):
        zb = 0.35 + k * (h - 0.35) / levels
        p.box(WOODL, -w / 2 + 0.03, w / 2 - 0.03, -d, -0.015, zb - 0.025, zb, f="yZz")
        p.box(WOODL, -w / 2 + 0.03, w / 2 - 0.03, -d - 0.015, -d, zb - 0.025, zb + 0.06, f="xXyYZ")     # a front lip
        mats = (["CT_WoodLight", "CT_PlasterYellow"], ["CT_WoodLight"], ["CT_Wood", "CT_WoodLight"], ["CT_PlasterYellow", "CT_WoodLight"])[k % 4]
        _loaf_row(p, -w / 2 + 0.03, w / 2 - 0.03, -d + 0.06, -0.06, zb, mats, round_=(k == levels - 1))
    p.box(WOODL, -w / 2 + 0.03, w / 2 - 0.03, -d, -0.015, 0.08, 0.1, f="yZ")
    mid = 0.35 + (h - 0.35) / 2
    p.core(-w / 2 + 0.03, w / 2 - 0.03, -d + 0.01, -0.02, mid - 0.02, mid - 0.006)
    return p.done(solid=True, top=h)


def tray_trolley(B, x, y, z=0.0, yaw=0.0, trays=5):
    """A baker's aluminium trolley with trays of buns."""
    p = _P(B, x, y, z, yaw, "tray_trolley")
    w, d, h = 0.6, 0.45, 1.5
    for sx in (-w / 2, w / 2):
        for sy in (-d / 2, d / 2):
            p.box(ALU, sx - 0.015, sx + 0.015, sy - 0.015, sy + 0.015, 0.06, h)
            p.cyl("CT_Rubber", sx, sy, 0.0, 0.06, 0.03, 5)
    for k in range(trays):
        zb = 0.3 + k * 0.26
        p.box(ALU, -w / 2 + 0.02, w / 2 - 0.02, -d / 2 + 0.01, d / 2 - 0.01, zb - 0.012, zb, f="xXyYZz")
        for i in range(3):
            for j in range(2):
                bx, by = -0.18 + i * 0.18, -0.09 + j * 0.18
                p.lathe("CT_PlasterYellow" if (i + j + k) % 3 else "CT_WoodLight", bx, by,
                        [(0.06, zb), (0.055, zb + 0.03), (0.03, zb + 0.05), (0.0, zb + 0.055)], 6)
    p.core(-w / 2 + 0.02, w / 2 - 0.02, -d / 2 + 0.02, d / 2 - 0.02, 0.3, 0.32)
    return p.done(solid=True, top=h)


def ice_bed(B, x, y, z=0.0, yaw=0.0, w=1.8, d=0.5, seed=0):
    """[prop] A sloping bed of crushed ice with fish on it (on a counter top)."""
    p = _P(B, x, y, z, yaw, "ice_bed")
    p.kind = "prop"
    p.hexa("CT_TileWhite", [(-w / 2, -d / 2, 0.0), (w / 2, -d / 2, 0.0), (w / 2, d / 2, 0.0), (-w / 2, d / 2, 0.0)],
           [(-w / 2, -d / 2, 0.03), (w / 2, -d / 2, 0.03), (w / 2, d / 2, 0.12), (-w / 2, d / 2, 0.12)], f="xXyYZ")
    rnd = kit._rng(seed, x, y, "fish")
    k = 0
    u = -w / 2 + 0.12
    while u < w / 2 - 0.12:
        for row in (-0.12, 0.1):
            L = rnd.uniform(0.28, 0.4)
            ht = 0.03 + (row + d / 2) / d * 0.09 + 0.004
            mat = ("CT_Aluminium", "CT_Aluminium", "CT_PlasterPink", "CT_Steel")[k % 4]
            out = [(u + L * 0.5 * math.cos(a), row + 0.055 * math.sin(a) * (1.0 if abs(math.cos(a)) < 0.9 else 0.4), 0)
                   for a in [k2 * math.pi / 3 for k2 in range(6)]]
            p.loft(mat, [[(a, b, ht) for a, b, _ in out], [(a, b, ht + 0.035) for a, b, _ in out]])
            p.face(mat, [(u - L * 0.5, row, ht + 0.02), (u - L * 0.5 - 0.08, row - 0.05, ht + 0.02), (u - L * 0.5 - 0.08, row + 0.05, ht + 0.02)])
            k += 1
        u += 0.22
    return p.done(top=0.12)


def chest_freezer(B, x, y, z=0.0, yaw=0.0, w=1.4):
    p = _P(B, x, y, z, yaw, "chest_freezer")
    d, h = 0.7, 0.85
    p.bev("CT_TileWhite", -w / 2, w / 2, -d / 2, d / 2, 0.08, h, 0.02)
    p.box("CT_Rubber", -w / 2 + 0.03, w / 2 - 0.03, -d / 2 + 0.03, d / 2 - 0.03, 0.0, 0.08, f="xXyY")
    p.box("CT_Steel", -w / 2 + 0.05, w / 2 - 0.05, -d / 2 - 0.01, -d / 2, h - 0.12, h - 0.09, f="y")
    p.box("CT_SteelRed", -0.1, 0.1, -d / 2 - 0.02, -d / 2, h - 0.2, h - 0.12, f="xXyZ")
    p.core(-w / 2 + 0.03, w / 2 - 0.03, -d / 2 + 0.03, d / 2 - 0.03, 0.1, h - 0.05)
    return p.done(solid=True, top=h)


def barrel(B, x, y, z=0.0, yaw=0.0, h=1.0, table=True):
    """An oak barrel; table=True: a round top on it (a beer bar's standing table)."""
    p = _P(B, x, y, z, yaw, "barrel")
    r = 0.3
    p.lathe(WOOD, 0, 0, [(r * 0.85, 0.0), (r * 0.97, h * 0.25), (r, h * 0.5), (r * 0.97, h * 0.75), (r * 0.85, h)], 10)
    for zz, rr in ((0.12, 0.91), (h - 0.12, 0.91)):
        p.lathe(STEEL, 0, 0, [(r * rr + 0.008, zz - 0.03), (r * rr + 0.008, zz + 0.03)], 10, top=False)
    top = h
    if table:
        p.cyl(WOODL, 0, 0, h, h + 0.04, 0.42, 10)
        top = h + 0.04
    p.core(-r * 0.6, r * 0.6, -r * 0.6, r * 0.6, 0.1, h - 0.1)
    return p.done(solid=True, top=top)


def barber_chair(B, x, y, z=0.0, yaw=0.0, mat="CT_Leather"):
    """A barber's chair on a chrome pedestal: seat, tilted back with a headrest, arms, footrest. The sitter faces yaw."""
    p = _P(B, x, y, z, yaw, "barber_chair")
    p.cyl(ALU, 0, 0.0, 0.0, 0.04, 0.28, 10)
    p.cyl(ALU, 0, 0.0, 0.04, 0.42, 0.055, 6)
    p.box(ALU, -0.22, 0.22, -0.2, 0.2, 0.42, 0.46)
    p.cushion(mat, -0.25, 0.25, -0.25, 0.22, 0.46, 0.58)
    p.hexa(mat, [(-0.25, 0.18, 0.58), (0.25, 0.18, 0.58), (0.25, 0.3, 0.58), (-0.25, 0.3, 0.58)],
           [(-0.24, 0.3, 1.12), (0.24, 0.3, 1.12), (0.24, 0.4, 1.12), (-0.24, 0.4, 1.12)])
    p.bev(mat, -0.12, 0.12, 0.31, 0.41, 1.14, 1.3, 0.02)
    p.rod(ALU, (0, 0.36, 1.12), (0, 0.36, 1.15), 0.03, 4)
    for s in (-1, 1):
        p.box(ALU, s * 0.27 - 0.015, s * 0.27 + 0.015, -0.12, -0.08, 0.46, 0.68)
        p.bev(mat, s * 0.27 - 0.04, s * 0.27 + 0.04, -0.22, 0.2, 0.68, 0.74, 0.015)
    p.rod(ALU, (0, -0.2, 0.42), (0, -0.42, 0.16), 0.03, 4)
    p.box(ALU, -0.2, 0.2, -0.5, -0.36, 0.13, 0.16)
    return p.done(seats=[(0.0, -0.02, 0.58, 0.0)])


def mirror_console(B, x, y, z=0.0, yaw=0.0, w=1.0, seed=0):
    """[wall] A barber's place: a big mirror over a shelf with bottles, a comb glass and a towel."""
    r = kit.mirror(B, x, y, z, yaw, zc=1.55, w=w, h=1.05)
    kit.wall_shelf(B, x, y, z, yaw, w=w + 0.1, zt=0.95, props=False)
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    at = lambda u, v: (x + u * c - v * s, y + u * s + v * c)
    a = at(-w / 4, -0.11)
    kit.bottles(B, a[0], a[1], z + 0.95, yaw, n=3, seed=seed)
    b = at(w / 4, -0.11)
    kit.glass(B, b[0], b[1], z + 0.95, yaw)
    return r


def backdrop(B, x, y, z=0.0, yaw=0.0, w=2.8, mat="CT_PlasterBlue"):
    """[wall] A photographer's paper backdrop: two stands, a roll, the paper sweeping onto the floor."""
    p = _P(B, x, y, z, yaw, "backdrop")
    p.kind = "prop"            # the posing stool stands on its paper: not a floor piece for kit.check
    for s in (-1, 1):
        p.rod(STEEL, (s * w / 2, -0.25, 0.0), (s * w / 2, -0.25, 2.65), 0.035, 4)
        p.rod(STEEL, (s * w / 2 - 0.25, -0.25, 0.01), (s * w / 2 + 0.25, -0.25, 0.01), 0.03, 4)
    p.rod(mat, (-w / 2 + 0.05, -0.25, 2.55), (w / 2 - 0.05, -0.25, 2.55), 0.12, 8)
    a, b = -w / 2 + 0.12, w / 2 - 0.12
    sheet = [(-0.31, 2.5), (-0.31, 0.5), (-0.38, 0.25), (-0.6, 0.06), (-1.4, 0.004)]
    for (y0, z0), (y1, z1) in zip(sheet, sheet[1:]):
        p.face(mat, [(a, y0, z0), (b, y0, z0), (b, y1, z1), (a, y1, z1)][::-1])
    return p.done()


def camera_tripod(B, x, y, z=0.0, yaw=0.0):
    """A big studio camera on a wooden tripod; its lens looks toward yaw."""
    p = _P(B, x, y, z, yaw, "camera_tripod")
    top = 1.25
    for k in range(3):
        a = math.radians(90 + k * 120)
        p.rod(WOODL, (0.0, 0.0, top), (0.38 * math.cos(a), 0.38 * math.sin(a), 0.0), 0.03, 4)
    p.box(STEEL, -0.08, 0.08, -0.08, 0.08, top, top + 0.06)
    p.bev(WOOD, -0.13, 0.13, -0.05, 0.22, top + 0.06, top + 0.34, 0.01)
    p.box(LAQ, -0.09, 0.09, -0.22, -0.05, top + 0.1, top + 0.3, f="xXyZz")      # the bellows
    p.rod(BRASS, (0, -0.22, top + 0.2), (0, -0.3, top + 0.2), 0.09, 8, caps=True)
    p.rod(LAQ, (0, -0.3, top + 0.2), (0, -0.31, top + 0.2), 0.07, 8, caps=True)
    p.box(WOOD, -0.11, 0.11, 0.22, 0.24, top + 0.08, top + 0.32)
    return p.done(lens=(0.0, -0.3, top + 0.2))


def softbox(B, x, y, z=0.0, yaw=0.0):
    p = _P(B, x, y, z, yaw, "softbox")
    for k in range(3):
        a = math.radians(90 + k * 120)
        p.rod(STEEL, (0.0, 0.0, 0.6), (0.3 * math.cos(a), 0.3 * math.sin(a), 0.0), 0.02, 4)
    p.rod(STEEL, (0, 0, 0.6), (0, 0, 1.75), 0.025, 4)
    p.hexa("CT_Steel", [(-0.3, 0.05, 1.5), (0.3, 0.05, 1.5), (0.1, 0.3, 1.7), (-0.1, 0.3, 1.7)],
           [(-0.3, 0.05, 2.1), (0.3, 0.05, 2.1), (0.1, 0.3, 1.9), (-0.1, 0.3, 1.9)], f="xXYZz")
    p.face("CT_Opal", [(-0.3, 0.05, 1.5), (0.3, 0.05, 1.5), (0.3, 0.05, 2.1), (-0.3, 0.05, 2.1)][::-1])
    p.face("CT_Opal", [(-0.29, 0.049, 1.51), (0.29, 0.049, 1.51), (0.29, 0.049, 2.09), (-0.29, 0.049, 2.09)])
    return p.done()


def workbench(B, x, y, z=0.0, yaw=0.0, w=2.2):
    """[wall] A heavy workbench against the wall: a vice, a lower shelf with boxes, a pegboard of tools above."""
    p = _P(B, x, y, z, yaw, "workbench")
    p.kind = "wall"
    d, h = 0.7, 0.9
    p.bev(WOOD, -w / 2, w / 2, -d, -0.02, h - 0.07, h, 0.01)
    for sx in (-w / 2 + 0.06, w / 2 - 0.06):
        for sy in (-d + 0.06, -0.08):
            p.box(STEEL, sx - 0.03, sx + 0.03, sy - 0.03, sy + 0.03, 0.0, h - 0.07)
    p.box(WOODL, -w / 2 + 0.03, w / 2 - 0.03, -d + 0.04, -0.05, 0.2, 0.23)
    for k in range(4):
        p.box(("CT_SteelRed", "CT_PlasterBlue", "CT_PlasterYellow", "CT_Steel")[k], -w / 2 + 0.15 + k * 0.45,
              -w / 2 + 0.5 + k * 0.45, -d + 0.1, -0.12, 0.23, 0.4)
    # the vice on the front right corner
    xv = w / 2 - 0.3
    p.box("CT_PlasterBlue", xv - 0.1, xv + 0.1, -d - 0.12, -d + 0.1, h, h + 0.14)
    p.rod(STEEL, (xv, -d - 0.2, h + 0.07), (xv, -d - 0.12, h + 0.07), 0.025, 4)
    p.rod(STEEL, (xv - 0.12, -d - 0.2, h + 0.07), (xv + 0.12, -d - 0.2, h + 0.07), 0.015, 4)
    # pegboard with tools
    zb, zt = 1.05, 1.95
    p.box(WOODL, -w / 2 + 0.1, w / 2 - 0.1, -0.03, 0.0, zb, zt, f="yxXzZ")
    k = 0
    u = -w / 2 + 0.25
    while u < w / 2 - 0.25:
        kind = k % 4
        if kind == 0:      # a hammer
            p.box(WOOD, u - 0.012, u + 0.012, -0.06, -0.035, zb + 0.25, zb + 0.6)
            p.box(STEEL, u - 0.07, u + 0.07, -0.07, -0.035, zb + 0.6, zb + 0.65)
        elif kind == 1:    # a saw
            p.slab(ALU, (u - 0.1, -0.035, zb + 0.15), (u + 0.1, -0.035, zb + 0.15), (u + 0.1, -0.035, zb + 0.7),
                   (u - 0.1, -0.035, zb + 0.55), 0.002)
            p.box(WOOD, u - 0.06, u + 0.06, -0.05, -0.035, zb + 0.7, zb + 0.82)
        elif kind == 2:    # spanners
            for j in range(4):
                p.box(STEEL, u - 0.09 + j * 0.06, u - 0.07 + j * 0.06, -0.05, -0.035, zb + 0.3 - j * 0.03, zb + 0.7)
        else:              # pliers and screwdrivers
            for j in range(3):
                p.box(("CT_SteelRed", "CT_PlasterYellow", "CT_PlasterBlue")[j], u - 0.07 + j * 0.07, u - 0.05 + j * 0.07,
                      -0.05, -0.035, zb + 0.45, zb + 0.6)
                p.box(STEEL, u - 0.065 + j * 0.07, u - 0.055 + j * 0.07, -0.05, -0.04, zb + 0.3, zb + 0.45)
        u += 0.42
        k += 1
    p.core(-w / 2 + 0.02, w / 2 - 0.02, -d + 0.02, -0.04, h - 0.065, h - 0.01)
    return p.done(solid=True, top=h, staff=(0.0, -d - 0.45, 0.0))


def deck_chair(B, x, y, z=0.0, yaw=0.0, canvas="CT_PlasterRed"):
    """An open beach deck chair (wooden frame, a canvas sling); the sitter faces yaw."""
    p = _P(B, x, y, z, yaw, "deck_chair")
    for s in (-0.28, 0.28):
        p.rod(WOODL, (s, -0.55, 0.0), (s, 0.35, 0.95), 0.035, 4)
        p.rod(WOODL, (s * 0.92, -0.25, 0.38), (s * 0.92, 0.45, 0.0), 0.03, 4)
        p.rod(WOODL, (s * 1.05, -0.55, 0.38), (s * 1.05, -0.05, 0.38), 0.03, 4)
    p.rod(WOODL, (-0.28, -0.5, 0.06), (0.28, -0.5, 0.06), 0.03, 4)
    p.rod(WOODL, (-0.28, 0.3, 0.9), (0.28, 0.3, 0.9), 0.03, 4)
    p.rod(WOODL, (-0.27, -0.42, 0.36), (0.27, -0.42, 0.36), 0.03, 4)
    sling = [(-0.42, 0.36), (-0.12, 0.2), (0.08, 0.36), (0.3, 0.88)]
    for (y0, z0), (y1, z1) in zip(sling, sling[1:]):
        q = [(-0.25, y0, z0), (0.25, y0, z0), (0.25, y1, z1), (-0.25, y1, z1)]
        p.face(canvas, q)
        p.face(canvas, [(a, b, c - 0.004) for a, b, c in q][::-1])
    return p.done(seats=[(0.0, -0.08, 0.3, 0.0)])


def bicycle(B, x, y, z=0.0, yaw=0.0, mat="CT_SteelRed"):
    """A rental bicycle standing on its wheels, pointing toward yaw."""
    p = _P(B, x, y, z, yaw, "bicycle")
    r, zc = 0.33, 0.34
    for yc in (-0.53, 0.53):
        pts = [(0.0, yc + r * math.cos(2 * math.pi * k / 12), zc + r * math.sin(2 * math.pi * k / 12)) for k in range(13)]
        p.tube("CT_Rubber", pts, 0.035, 3)
        p.rod(ALU, (0.0, yc - r, zc), (0.0, yc + r, zc), 0.008, 3)
        p.rod(ALU, (0.0, yc, zc - r), (0.0, yc, zc + r), 0.008, 3)
    rear, crank, seat, head0, head1, front = ((0, 0.53, zc), (0, 0.02, 0.3), (0, 0.17, 0.82), (0, -0.4, 0.62), (0, -0.43, 0.82),
                                              (0, -0.53, zc))
    for a, b in ((rear, crank), (crank, seat), (seat, rear), (seat, head1), (crank, head0), (head0, head1), (head0, front)):
        p.rod(mat, a, b, 0.03, 4)
    p.rod(STEEL, (0, 0.17, 0.82), (0, 0.2, 0.92), 0.022, 4)
    p.bev("CT_Leather", -0.07, 0.07, 0.12, 0.33, 0.92, 0.96, 0.01)
    p.rod(STEEL, (0, -0.43, 0.82), (0, -0.4, 0.95), 0.022, 4)
    p.rod(STEEL, (-0.27, -0.36, 0.97), (0.27, -0.36, 0.97), 0.022, 4)
    p.rod(ALU, (0.0, 0.08, 0.3), (0.0, -0.06, 0.3), 0.06, 6)
    p.box(ALU, -0.11, 0.11, 0.38, 0.72, 0.7, 0.72)          # the carrier
    return p.done()


def oars(B, x, y, z=0.0, yaw=0.0, n=4, w=1.2):
    """[wall] Oars (and a boat hook) leaning on the wall, blades up."""
    p = _P(B, x, y, z, yaw, "oars")
    p.kind = "wall"
    for k in range(n):
        u = -w / 2 + (k + 0.5) * w / n
        p.rod(WOODL, (u, -0.32, 0.0), (u + 0.06, -0.05, 2.35), 0.045, 6)
        bu, bv = u + 0.06, -0.05
        p.slab("CT_PlasterRed" if k % 2 else WOODL, (bu - 0.08, bv - 0.006, 1.75), (bu + 0.08, bv - 0.006, 1.75),
               (bu + 0.075, bv - 0.006, 2.4), (bu - 0.075, bv - 0.006, 2.4), 0.012)
    return p.done()


def life_ring(B, x, y, z=0.0, yaw=0.0, zc=1.8, r=0.3):
    """[wall] A red and white life ring on the wall."""
    p = _P(B, x, y, z, yaw, "life_ring")
    p.kind = "wall"
    for k in range(12):
        a0, a1 = 2 * math.pi * k / 12, 2 * math.pi * (k + 1) / 12
        p.rod("CT_PlasterRed" if (k // 3) % 2 else "CT_TileWhite", (r * math.cos(a0), -0.06, zc + r * math.sin(a0)),
              (r * math.cos(a1), -0.06, zc + r * math.sin(a1)), 0.1, 4)
    return p.done()


def pots(B, x, y, z=0.0, yaw=0.0, n=2):
    """[prop] Big aluminium pots on a stove."""
    p = _P(B, x, y, z, yaw, "pots")
    p.kind = "prop"
    for k in range(n):
        u = -0.15 * (n - 1) + k * 0.3
        h = 0.3 if k % 2 == 0 else 0.22
        p.cyl(ALU, u, 0.0, 0.0, h, 0.13, 8)
        p.cyl(ALU, u, 0.0, h, h + 0.02, 0.135, 8)
        p.rod(STEEL, (u, 0.0, h + 0.02), (u, 0.0, h + 0.05), 0.03, 4)
    return p.done(top=0.3)


def pastries(B, x, y, z=0.0, yaw=0.0, n=5, mat="CT_PlasterYellow", seed=0):
    """[prop] A dish of half-moon pastries (chebureki, pirozhki) or dumplings."""
    p = _P(B, x, y, z, yaw, "pastries")
    p.kind = "prop"
    p.cyl("CT_TileWhite", 0.0, 0.0, 0.0, 0.015, 0.2, 10)
    for k in range(n):
        a = 2 * math.pi * k / n
        cx, cy = 0.09 * math.cos(a), 0.09 * math.sin(a)
        half = [(cx + 0.08 * math.cos(a + math.pi / 2 + t * math.pi / 4), cy + 0.08 * math.sin(a + math.pi / 2 + t * math.pi / 4))
                for t in range(5)]
        p.loft(mat, [[(u, v, 0.015) for u, v in half], [(u, v, 0.04) for u, v in half]])
    return p.done(top=0.04)


def tall_table(B, x, y, z=0.0, yaw=0.0, r=0.38):
    """A stand-up table (canteens, beer bars)."""
    return kit.table_round(B, x, y, z, yaw, r=r, h=1.08, mat="CT_TileWhite", base=STEEL)


# ------------------------------------------------------------------------------------------------ the twelve trades
def cafe(B):
    """Кафе «Чайка»: a bar with a coffee machine and a back bar, four tables by the window, the café's menu."""
    bar = kit.bar_counter(B, 2.3, 4.55, 0.0, 0.0, w=3.2, taps=2, front="CT_PlasterBlue", back=False, seed=1)
    kit.shop_shelf(B, 2.3, YB, 0.0, 0.0, w=2.8, goods="bottles", levels=4, mat=WOOD, seed=1)
    kit.cup(B, 1.4, 4.5, bar["top"], 0.0)
    kit.cup(B, 1.6, 4.5, bar["top"], 20.0)
    kit.teapot(B, 3.4, 4.6, bar["top"], 0.0)
    B.thing("cafe_bar", "counter", (2.3, 4.55, 1.05), 0.0, shop="seagull_cafe", service="bar", role="barista")
    B.spot("counter", "counter", (3.3, 3.75, 0.0), 180.0)
    B.spot("staff_barista_1", "work", (1.7, 5.35, 0.0), 0.0)
    B.spot("staff_barista_2", "work", (2.9, 5.35, 0.0), 0.0)
    for k, sx in enumerate((1.0, 1.8)):
        st = kit.bar_stool(B, sx, 3.8, 0.0, 180.0, mat="CT_PlasterBlue")
        if k == 0:
            _seat(B, "cafe_stool", st, 0.75)
    # the kitchen corner (the cook) and the café's tables
    kit.kitchen_unit(B, -3.3, YB, 0.0, 0.0, modules="cupboard,stove,sink", wall_cupboards=True)
    pots(B, -3.3, 6.1, 0.9, 0.0, n=1)
    B.spot("staff_cook", "work", (-3.3, 5.45, 0.0), 180.0)
    for i, (tx, ty) in enumerate(((-3.6, 1.55), (-1.4, 1.55), (-3.6, 3.65), (-1.4, 3.65))):
        t = kit.table_round(B, tx, ty, 0.0, 0.0, r=0.4)
        _chairs(B, tx, ty, 0.8, 0.8, (1, 1, 0, 0), "table%d" % (i + 1), kind="tube", mat="CT_PlasterBlue")
        kit.vase(B, tx, ty, t["top"], 0.0, seed=i) if i % 2 == 0 else kit.cup(B, tx + 0.1, ty, t["top"], 0.0)
    kit.menu_board(B, -0.6, YB, 0.0, 0.0, name="menu_1", w=0.8, zc=1.65)
    B.thing("cafe_menu", "board", (-0.6, YB - 0.04, 1.65), 0.0, topic="prices")
    kit.picture(B, XL, 2.6, 0.0, 90.0, name="painting_sea_calm", w=1.0, zc=1.75)
    B.thing("cafe_picture", "board", (XL + 0.04, 2.6, 1.75), 90.0, topic="painting")
    kit.poster(B, XR, 3.3, 0.0, 270.0, name="poster_sun_1", w=0.6, zc=1.7)
    kit.plant(B, -4.45, 4.95, 0.0, 0.0, h=1.3, kind="palm")
    kit.plant(B, 0.0, 5.95, 0.0, 0.0, h=1.1, kind="ficus", pot="CT_TileWhite")
    kit.wall_clock(B, XR, 4.6, 0.0, 270.0, zc=2.35, face="clock_1")
    _lamp(B, 0.0, 3.0)
    kit.pendant(B, 2.3, 4.3, CEIL, drop=0.7, kind="cone")


def fish(B):
    """Рыбная лавка: a counter with fish on crushed ice and scales, tinned fish behind, crates and a freezer."""
    c = kit.shop_counter(B, 1.0, 4.4, 0.0, 0.0, w=3.0, front="CT_PlasterBlue", top="CT_TileWhite")
    ice_bed(B, 0.35, 4.38, c["top"], 0.0, w=1.6, d=0.5, seed=2)
    kit.kitchen_scales(B, 1.75, 4.35, c["top"], 0.0)
    B.thing("fish_counter", "counter", (1.0, 4.4, 1.1), 0.0, shop="fish_shop", service="fishmonger", role="fishmonger")
    B.spot("counter", "counter", (1.0, 3.55, 0.0), 180.0)
    B.spot("staff_fishmonger_1", "work", (0.3, 5.2, 0.0), 0.0)
    B.spot("staff_fishmonger_2", "work", (1.8, 5.2, 0.0), 0.0)
    kit.shop_shelf(B, 1.0, YB, 0.0, 0.0, w=2.2, goods="groceries", seed=3)
    B.thing("fish_tins", "shelf", (1.0, YB - 0.25, 1.3), 0.0, topic="sea")
    chest_freezer(B, XL + 0.36, 2.6, 0.0, 90.0, w=1.4)
    for k, (cx, cy, cz) in enumerate(((-4.3, 5.85, 0.0), (-3.7, 5.85, 0.0), (-4.3, 5.85, 0.3), (-3.0, 5.9, 0.0))):
        kit.crate(B, cx, cy, cz, 0.0, fill=("CT_Aluminium", "CT_Steel", "CT_Aluminium", "CT_PlasterPink")[k], seed=k)
    barrel(B, -1.6, 5.85, 0.0, 0.0, h=0.8, table=False)
    # crates of the morning's catch on ice by the window, a hanging steelyard over the counter's end
    for k, cx in enumerate((-3.6, -2.9, -2.2)):
        kit.crate(B, cx, 1.05, 0.0, 0.0, fill="CT_TileWhite", seed=10 + k)
        ice_bed(B, cx, 1.05, 0.3, 0.0, w=0.5, d=0.34, seed=20 + k)
    kit.shop_shelf(B, XR, 5.4, 0.0, 270.0, w=1.2, goods="jars", levels=4, seed=12)
    kit.picture(B, XL, 4.5, 0.0, 90.0, name="painting_sea_storm", w=1.0, zc=1.8)
    B.thing("fish_picture", "board", (XL + 0.04, 4.5, 1.8), 90.0, topic="painting")
    kit.wall_map(B, XR, 3.6, 0.0, 270.0, name="map_coast", w=1.4, h=1.0, zc=1.75)
    B.thing("fish_map", "board", (XR - 0.04, 3.6, 1.75), 270.0, topic="view")
    life_ring(B, -0.75, YB, 0.0, 0.0, zc=1.9)
    kit.sign_plate(B, 1.0, 4.4, 0.0, 0.0, name="sign_till", w=0.8, zc=2.55, hang=True)
    _lamp(B, 0.5, 3.4, kind="cone")
    kit.pendant(B, -3.0, 2.6, CEIL, drop=0.55, kind="cone")


def rental(B):
    """Прокат: bicycles, deck chairs and oars to hire, a counter with the keys, the hire rules on the wall."""
    c = kit.shop_counter(B, 2.9, 4.65, 0.0, 0.0, w=2.0, front="CT_PlasterYellow", till=False)
    kit.newspaper(B, 2.6, 4.6, c["top"], 10.0, seed=4)
    kit.telephone(B, 3.5, 4.7, c["top"], 0.0)
    B.thing("rental_counter", "counter", (2.9, 4.65, 1.1), 0.0, service="rentals", role="renter")
    B.spot("counter", "counter", (2.9, 3.85, 0.0), 180.0)
    B.spot("staff_renter", "work", (2.9, 5.45, 0.0), 0.0)
    for k, bx in enumerate((-4.4, -3.8, -3.2)):
        bicycle(B, bx, 4.6, 0.0, 0.0, mat=("CT_SteelRed", "CT_PlasterBlue", "CT_PlasterGreen")[k])
    for k, dx in enumerate((-3.3, -2.1, -0.9)):
        dc = deck_chair(B, dx, 1.55, 0.0, 0.0, canvas=("CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow")[k])
        if k == 1:
            _seat(B, "rental_deckchair", dc, 0.3)
    oars(B, -1.4, YB, 0.0, 0.0, n=4, w=1.3)
    life_ring(B, 0.3, YB, 0.0, 0.0, zc=1.85)
    kit.mats(B, -1.4, 4.4, 0.0, 0.0, w=0.9, d=0.6, n=4, mat="CT_PlasterBlue")
    kit.notice_board(B, XR, 3.0, 0.0, 270.0, w=1.2, zc=1.6)
    B.thing("rental_rules", "board", (XR - 0.04, 3.0, 1.6), 270.0, topic="rules")
    kit.poster(B, XL, 2.4, 0.0, 90.0, name="travel_2", w=0.6, zc=1.7)
    kit.wall_clock(B, 2.9, YB, 0.0, 0.0, zc=2.3, face="clock_0")
    # QA: the back-right corner and the right wall were bare
    kit.plant(B, 4.45, 5.9, 0.0, 0.0, h=1.3, kind="ficus")
    kit.picture(B, XR, 5.0, 0.0, 270.0, name="painting_sea_calm", w=0.8, zc=1.7)
    kit.rug(B, 1.0, 2.9, FLOOR, 0.0, w=2.2, d=1.3, name="rug_1", seed=3)
    _lamp(B, -0.8, 3.3, kind="cone")


def workshop(B):
    """Мастерская: a repair counter, a workbench with a pegboard of tools, radios and a TV waiting, hardware shelves."""
    c = kit.shop_counter(B, 2.7, 3.35, 0.0, 0.0, w=2.0, front="CT_PlasterGreen", top="CT_WoodLight", till=False)
    kit.radio(B, 2.4, 3.35, c["top"], 0.0)
    kit.wall_clock(B, 3.6, YB, 0.0, 0.0, zc=2.2, face="clock_3")
    B.thing("repair_counter", "counter", (2.7, 3.35, 1.1), 0.0, service="repair", role="repairman")
    B.spot("counter", "counter", (2.7, 2.55, 0.0), 180.0)
    B.spot("staff_repairman", "work", (2.7, 4.15, 0.0), 0.0)
    wb = workbench(B, -0.4, YB, 0.0, 0.0, w=2.4)
    kit.tools(B, -1.1, 6.0, wb["top"], 0.0)
    kit.radio(B, 0.2, 6.05, wb["top"], 15.0)
    B.thing("workbench", "craft", (-0.4, 6.05, 0.95), 0.0, what="workbench")
    B.spot("bench", "work", (-0.4, 5.25, 0.0), 180.0)
    kit.stool(B, 0.4, 5.3, 0.0, 180.0)
    kit.shop_shelf(B, XL, 2.4, 0.0, 90.0, w=1.8, goods="hardware", seed=5)
    kit.shop_shelf(B, XL, 4.5, 0.0, 90.0, w=1.8, goods="boxes", seed=6)
    B.thing("workshop_manuals", "shelf", (XL + 0.25, 4.5, 1.3), 90.0, topic="engineering")
    kit.tv_set(B, 3.9, 5.85, 0.0, 0.0)
    kit.cardboard_box(B, 3.0, 6.05, 0.0, 0.0, seed=7)
    kit.cardboard_box(B, 3.0, 6.05, 0.35, 10.0, seed=8)
    kit.poster(B, XR, 2.4, 0.0, 270.0, name="poster_safety", w=0.6, zc=1.7)
    B.thing("workshop_poster", "board", (XR - 0.04, 2.4, 1.7), 270.0, topic="rules")
    kit.fire_extinguisher(B, XR, 1.2, 0.0, 270.0)
    # QA: the front-left of the room was an empty floor
    kit.plant(B, -4.45, 0.95, 0.0, 0.0, h=1.1, kind="fern")
    kit.rug(B, -1.6, 2.4, FLOOR, 0.0, w=2.0, d=1.2, name="rug_2", seed=5)
    _lamp(B, 0.6, 3.6, kind="cone")
    kit.pendant(B, -0.4, 5.6, CEIL, drop=0.8, kind="cone")


def dumplings(B):
    """Пельменная: a self-service counter with a tray rail, the stoves and pots behind, stand-up tables, the menu."""
    c = kit.shop_counter(B, 1.4, 4.3, 0.0, 0.0, w=3.4, front="CT_TileWhite", top="CT_Aluminium", till=True)
    p = _P(B, 1.4, 4.3, 0.0, 0.0, "tray_rail")
    p.kind = "prop"
    for zz in (0.85, 0.9):
        p.rod(ALU, (-1.7, -0.62, zz), (1.7, -0.62, zz), 0.025, 6)
    for xx in (-1.5, 0.0, 1.5):
        p.rod(ALU, (xx, -0.36, 0.86), (xx, -0.62, 0.86), 0.02, 4)
    p.done()
    pastries(B, 0.6, 4.3, c["top"], 0.0, n=6, mat="CT_TileWhite")
    kit.plates(B, 0.0, 4.3, c["top"], 0.0, n=5)
    B.thing("dumpling_counter", "counter", (1.4, 4.3, 1.1), 0.0, shop="dumplings", service="bar", role="cook")
    B.spot("counter", "counter", (1.4, 3.3, 0.0), 180.0)
    B.spot("staff_cook", "work", (0.8, 5.1, 0.0), 0.0)
    B.spot("staff_waiter", "work", (2.3, 5.1, 0.0), 0.0)
    kit.kitchen_unit(B, 1.4, YB, 0.0, 0.0, modules="stove,cupboard,sink,stove", wall_cupboards=False)
    pots(B, 0.5, 6.1, 0.9, 0.0, n=2)
    pots(B, 2.3, 6.1, 0.9, 0.0, n=1)
    for i, (tx, ty) in enumerate(((-3.8, 1.5), (-1.7, 1.5), (-3.8, 3.4), (-1.7, 3.4))):
        t = tall_table(B, tx, ty, 0.0, 0.0)
        pastries(B, tx, ty, t["top"], 0.0, n=5, mat="CT_TileWhite") if i % 2 == 0 else kit.glass(B, tx, ty, t["top"], 0.0)
        B.spot("table%d" % (i + 1), "stand", (tx, ty - 0.6, 0.0), 180.0)
    kit.menu_board(B, -2.6, YB, 0.0, 0.0, name="menu_0", w=0.8, zc=1.7)
    B.thing("dumpling_menu", "board", (-2.6, YB - 0.04, 1.7), 0.0, topic="prices")
    kit.poster(B, XL, 3.4, 0.0, 90.0, name="poster_cons_1", w=0.6, zc=1.7)
    kit.wall_shelf(B, XL, 2.4, 0.0, 90.0, w=1.0, zt=1.45)
    kit.coat_rack(B, XR - 0.35, 3.0, 0.0, 270.0, w=1.2)
    kit.shop_shelf(B, XL, 4.6, 0.0, 90.0, w=1.2, goods="bottles", levels=4, seed=14)
    kit.radiator(B, -1.2, YB, 0.0, 0.0, w=1.0)
    kit.plant(B, -4.4, 5.85, 0.0, 0.0, h=1.1, kind="fern")
    kit.plant(B, 4.45, 5.9, 0.0, 0.0, h=1.2, kind="ficus")      # QA: back-right corner was empty
    kit.sign_plate(B, 1.4, 4.3, 0.0, 0.0, name="sign_till", w=0.8, zc=2.55, hang=True)
    _lamp(B, -0.3, 3.2)


def souvenirs(B):
    """Сувениры: glass display cases of trinkets, shelves of boxes and dolls, a postcard rack, sea paintings."""
    c = kit.shop_counter(B, 2.8, 4.5, 0.0, 0.0, w=2.0, front="CT_PlasterPink")
    B.thing("souvenir_counter", "counter", (2.8, 4.5, 1.1), 0.0, shop="souvenirs", service="shop", role="seller")
    B.spot("counter", "counter", (2.8, 3.65, 0.0), 180.0)
    B.spot("staff_seller", "work", (2.8, 5.3, 0.0), 0.0)
    for k, cx in enumerate((-3.4, -1.6)):
        kit.display_case(B, cx, 2.8, 0.0, 0.0, w=1.2, goods="toys", seed=k)
        B.spot("case%d" % (k + 1), "stand", (cx, 2.1, 0.0), 180.0)
    B.thing("souvenir_case", "board", (-3.4, 2.8, 1.0), 0.0, topic="store")
    kit.shop_shelf(B, -3.2, YB, 0.0, 0.0, w=1.6, goods="boxes", seed=9)
    kit.shop_shelf(B, -1.4, YB, 0.0, 0.0, w=1.4, goods="toys", seed=10)
    kit.magazine_rack(B, XL, 4.6, 0.0, 90.0, w=0.9)
    B.thing("postcards", "shelf", (XL + 0.2, 4.6, 1.0), 90.0, topic="travel")
    kit.picture(B, XR, 2.5, 0.0, 270.0, name="painting_lighthouse", w=0.9, zc=1.7)
    B.thing("souvenir_picture", "board", (XR - 0.04, 2.5, 1.7), 270.0, topic="painting")
    kit.picture(B, XR, 3.7, 0.0, 270.0, name="painting_dunes", w=0.7, zc=1.7)
    kit.picture(B, 1.2, YB, 0.0, 0.0, name="painting_sea_night", w=0.8, zc=1.8)
    kit.shop_shelf(B, 3.3, YB, 0.0, 0.0, w=1.2, goods="boxes", seed=11, levels=4)
    kit.plant(B, 0.2, 5.9, 0.0, 0.0, h=1.0, kind="cactus")
    _lamp(B, -0.8, 3.2)


def bakery(B):
    """Булочная: a counter with the till, bread racks behind and along the wall, a baker's trolley, a table by the window."""
    c = kit.shop_counter(B, 0.8, 3.9, 0.0, 0.0, w=3.0, front="CT_PlasterYellow", top="CT_TileWhite")
    p = _P(B, 0.3, 3.9, c["top"], 0.0, "bun_tray")
    p.kind = "prop"
    p.box(ALU, -0.45, 0.45, -0.2, 0.2, 0.0, 0.012)
    _loaf_row(p, -0.43, 0.43, -0.15, 0.15, 0.012, ["CT_PlasterYellow", "CT_WoodLight"], step=0.14, round_=True)
    p.done()
    B.thing("bakery_counter", "counter", (0.8, 3.9, 1.1), 0.0, shop="bakery", service="shop", role="seller")
    B.spot("counter", "counter", (0.8, 3.05, 0.0), 180.0)
    B.spot("staff_seller", "work", (0.8, 4.7, 0.0), 0.0)
    bread_rack(B, 0.6, YB, 0.0, 0.0, w=2.8)
    B.thing("bread_rack", "board", (0.6, YB - 0.25, 1.2), 0.0, topic="store")
    bread_rack(B, XL, 4.4, 0.0, 90.0, w=1.6, levels=3)
    tray_trolley(B, 3.9, 5.7, 0.0, 270.0)
    B.spot("staff_baker", "work", (3.2, 5.7, 0.0), 90.0)
    kit.menu_board(B, 3.2, YB, 0.0, 0.0, name="price_1", w=0.7, zc=1.7)
    B.thing("bread_prices", "board", (3.2, YB - 0.04, 1.7), 0.0, topic="prices")
    kit.table(B, -3.2, 1.5, 0.0, 0.0, w=0.8, d=0.6, mat="CT_WoodLight")
    _chairs(B, -3.2, 1.5, 0.8, 0.6, (0, 0, 1, 1), "table1", kind="wood")
    kit.cup(B, -3.3, 1.5, 0.75, 0.0)
    kit.sack(B, -1.5, 5.95, 0.0, 0.0)
    kit.sack(B, -2.0, 6.0, 0.0, 30.0)
    # QA: the left wall by the window table was bare
    kit.plant(B, -4.5, 2.9, 0.0, 0.0, h=1.1, kind="fern")
    kit.poster(B, XL, 2.2, 0.0, 90.0, name="travel_1", w=0.6, zc=1.7)
    kit.picture(B, XR, 3.0, 0.0, 270.0, name="painting_still_life", w=0.8, zc=1.7)
    kit.wall_clock(B, XR, 4.6, 0.0, 270.0, zc=2.3, face="clock_2")
    _lamp(B, -0.4, 3.0)


def photo(B):
    """Фотоателье: a studio corner (backdrop, posing stool, the camera on its tripod, a softbox), sample portraits, the
    photographer's desk and a photo booth."""
    backdrop(B, -3.0, YB, 0.0, 0.0, w=2.9)
    st = kit.stool(B, -3.0, 5.0, 0.0, 0.0, h=0.5, mat=WOOD)
    B.thing("photo_stool", "seat", (-3.0, 5.0, 0.5), 0.0, height=0.5, what="portrait")
    camera_tripod(B, -3.0, 2.5, 0.0, 180.0)
    B.spot("staff_photographer_camera", "work", (-3.0, 2.0, 0.0), 180.0)
    softbox(B, -1.1, 4.2, 0.0, 293.0)
    softbox(B, -4.5, 3.4, 0.0, 137.0)
    d = kit.reception_desk(B, 1.6, 3.5, 0.0, 0.0, w=1.8, front="CT_PlasterGrey")
    kit.telephone(B, 1.2, 3.65, 0.75, 0.0)
    kit.books_stack(B, 2.0, 3.7, 0.75, 0.0, n=2, seed=3)
    B.thing("photo_desk", "counter", (1.6, 3.5, 1.1), 0.0, service="reception", role="photographer")
    B.spot("counter", "counter", (1.6, 2.65, 0.0), 180.0)
    B.spot("staff_photographer", "work", (1.6, 4.35, 0.0), 0.0)
    pb = kit.photo_booth(B, 4.15, 5.35, 0.0, 270.0)
    B.thing("photo_booth", "machine", (4.15, 5.35, 1.2), 270.0, what="photo_booth")
    for k, (py, pz) in enumerate(((2.2, 1.95), (3.1, 1.95), (2.2, 1.2), (3.1, 1.2))):
        kit.picture(B, XR, py, 0.0, 270.0, name="painting_portrait", w=0.5, zc=pz, frame=("CT_Wood", "CT_Lacquer")[k % 2])
    B.thing("photo_samples", "board", (XR - 0.04, 2.65, 1.6), 270.0, topic="exhibit")
    kit.picture(B, 1.4, YB, 0.0, 0.0, name="painting_portrait", w=0.7, zc=1.75, frame="CT_Brass")
    kit.picture(B, 2.4, YB, 0.0, 0.0, name="painting_sea_calm", w=0.6, zc=1.75)
    kit.plant(B, 0.2, 5.95, 0.0, 0.0, h=1.2, kind="monstera")
    kit.hat_stand(B, 4.4, 2.3, 0.0, 0.0)
    # QA: the middle and the left wall were bare
    kit.wall_clock(B, -0.6, YB, 0.0, 0.0, zc=2.3, face="clock_2")
    kit.picture(B, XL, 1.4, 0.0, 90.0, name="painting_dunes", w=0.7, zc=1.7, frame="CT_Brass")
    kit.picture(B, XL, 5.4, 0.0, 90.0, name="painting_lighthouse", w=0.8, zc=1.7)
    kit.rug(B, 0.6, 2.0, FLOOR, 0.0, w=2.0, d=1.2, name="rug_4", seed=8)
    kit.plant(B, 4.45, 0.95, 0.0, 0.0, h=1.0, kind="fern")
    _lamp(B, 0.6, 3.0)
    kit.pendant(B, -3.0, 4.2, CEIL, drop=0.5, kind="cone")


def barber(B):
    """Парикмахерская: two barber's chairs before mirrors, a wash basin, a waiting bench with magazines, a coat stand."""
    for k, cy in enumerate((2.2, 4.3)):
        mirror_console(B, XL, cy, 0.0, 90.0, w=1.0, seed=k)
        bc = barber_chair(B, XL + 1.0, cy, 0.0, 270.0, mat=("CT_Leather", "CT_PlasterRed")[k])
        _seat(B, "barber_chair_%d" % (k + 1), bc, 0.58, what="haircut")
        B.spot("staff_barber_%d" % (k + 1), "work", (XL + 1.75, cy - 0.25, 0.0), 270.0)
    kit.kitchen_unit(B, -3.6, YB, 0.0, 0.0, modules="sink,cupboard", wall_cupboards=False)
    kit.mirror(B, -3.9, YB, 0.0, 0.0, zc=1.6, w=0.5, h=0.7)
    wr = kit.waiting_row(B, 1.4, 6.05, 0.0, 0.0, n=3, mat="CT_PlasterRed")
    m = wr["seats"][1]
    B.thing("barber_waiting", "seat", (m[0], m[1], m[2]), m[3], height=0.45)
    for k, s in enumerate(wr["seats"]):
        B.spot("wait%d" % k, "seat", (s[0], s[1], 0.0), s[3])
    ct = kit.coffee_table(B, 1.4, 5.05, 0.0, 0.0, w=1.0)
    kit.newspaper(B, 1.2, 5.05, ct["top"], 15.0, seed=6)
    kit.newspaper(B, 1.6, 5.05, ct["top"], -25.0, seed=7)
    kit.magazine_rack(B, 3.3, YB, 0.0, 0.0, w=0.8)
    B.thing("barber_magazines", "shelf", (3.3, YB - 0.2, 1.0), 0.0, topic="local")
    kit.poster(B, XR, 3.2, 0.0, 270.0, name="painting_portrait", w=0.5, zc=1.7)
    kit.notice_board(B, XR, 4.5, 0.0, 270.0, w=0.9, zc=1.6)
    B.thing("barber_prices", "board", (XR - 0.04, 4.5, 1.6), 270.0, topic="prices")
    kit.hat_stand(B, 4.4, 2.4, 0.0, 0.0)
    kit.plant(B, -0.4, 0.85, 0.0, 0.0, h=1.0, kind="fern", pot="CT_TileWhite")
    # QA: the room's middle and the back wall were bare
    kit.rug(B, 1.4, 5.0, FLOOR, 0.0, w=2.0, d=1.3, name="rug_1", seed=9)
    kit.picture(B, -1.2, YB, 0.0, 0.0, name="painting_sea_calm", w=0.7, zc=1.7)
    kit.wall_clock(B, XR, 5.6, 0.0, 270.0, zc=2.3, face="clock_1")
    kit.plant(B, 4.45, 5.9, 0.0, 0.0, h=1.2, kind="palm")
    _lamp(B, -2.6, 3.2)
    kit.pendant(B, 1.4, 4.4, CEIL, drop=0.5)


BOOK_TOPICS = (("sea", "old"), ("birds", "library"), ("local", "encyclopaedia"), ("travel", "modern"))


def books(B):
    """Книжный развал: shelves by topic round the walls, a sale table piled with books, a reading armchair, the counter."""
    for k, (topic, spines) in enumerate(BOOK_TOPICS):
        x = -4.3 + k * 0.95
        kit.bookshelf(B, x, YB, 0.0, 0.0, w=0.9, h=2.1, topic=spines, seed=k)
        B.thing("shelf_%s" % topic, "shelf", (x, YB - 0.16, 1.3), 0.0, topic=topic)
    for k, (topic, spines, y) in enumerate((("poetry", "collected", 2.0), ("kids", "kids", 2.95), ("cooking", "paperback", 3.9),
                                             ("science", "library", 4.85))):
        kit.bookshelf(B, XL, y, 0.0, 90.0, w=0.9, h=2.1, topic=spines, seed=10 + k)
        B.thing("shelf_%s" % topic, "shelf", (XL + 0.16, y, 1.3), 90.0, topic=topic)
    t = kit.table(B, -1.6, 2.4, 0.0, 0.0, w=1.6, d=0.8, mat="CT_Wood")
    for k, (dx, dy) in enumerate(((-0.55, -0.15), (-0.2, 0.15), (0.2, -0.1), (0.55, 0.15))):
        kit.books_stack(B, -1.6 + dx, 2.4 + dy, t["top"], 20.0 * k, n=3 + k % 3, seed=20 + k)
    B.thing("sale_table", "shelf", (-1.6, 2.4, 0.9), 0.0, topic="poetry_sale")
    B.spot("sale_table", "stand", (-1.6, 1.75, 0.0), 180.0)
    ac = kit.armchair(B, 0.4, 5.6, 0.0, 0.0, mat="CT_PlasterGreen")
    _seat(B, "reading_chair", ac, 0.45)
    kit.floor_lamp(B, 1.05, 5.95, 0.0, 0.0)
    kit.rug(B, 0.4, 5.0, 0.0, 0.0, w=1.6, d=1.2, name="rug_3", seed=2)
    c = kit.shop_counter(B, 3.1, 4.3, 0.0, 0.0, w=1.6, front="CT_PlasterGreen", top="CT_WoodLight")
    kit.books_stack(B, 2.7, 4.3, c["top"], 10.0, n=3, seed=31)
    B.thing("book_counter", "counter", (3.1, 4.3, 1.1), 0.0, shop="book_stall", service="shop", role="bookseller")
    B.spot("counter", "counter", (3.1, 3.45, 0.0), 180.0)
    B.spot("staff_bookseller", "work", (3.1, 5.1, 0.0), 0.0)
    kit.bookshelf(B, 3.3, YB, 0.0, 0.0, w=1.6, h=2.1, topic="old", seed=40)
    kit.poster(B, XR, 3.0, 0.0, 270.0, name="poster_birds_1", w=0.6, zc=1.7)
    B.thing("bird_poster", "board", (XR - 0.04, 3.0, 1.7), 270.0, topic="exhibit")
    _lamp(B, -1.0, 3.6, kind="green")


def beer(B):
    """Пивной бар: a long bar with taps and a back bar, bar stools, barrels as standing tables, a dart board."""
    bar = kit.bar_counter(B, 1.7, 4.55, 0.0, 0.0, w=3.6, taps=4, front="CT_Leather", back=False, seed=5)
    kit.shop_shelf(B, 1.7, YB, 0.0, 0.0, w=3.2, goods="bottles", levels=4, mat=WOOD, seed=5)
    for k in range(3):
        kit.glass(B, 0.6 + k * 0.25, 4.45, bar["top"], 0.0)
    B.thing("beer_bar", "counter", (1.7, 4.55, 1.05), 0.0, shop="beer_bar", service="bar", role="bartender")
    B.spot("counter", "counter", (3.2, 3.75, 0.0), 180.0)
    B.spot("staff_bartender_1", "work", (1.1, 5.35, 0.0), 0.0)
    B.spot("staff_bartender_2", "work", (2.4, 5.35, 0.0), 0.0)
    for k, sx in enumerate((0.3, 1.1, 1.9)):
        st = kit.bar_stool(B, sx, 3.8, 0.0, 180.0, mat="CT_Leather")
        B.spot("stool%d" % k, "seat", (sx, 3.8, 0.0), 180.0)
        if k == 1:
            _seat(B, "beer_stool", st, 0.75)
    for k, (bx, by) in enumerate(((-3.6, 1.5), (-1.5, 1.5), (-3.6, 3.3))):
        b = barrel(B, bx, by, 0.0, 0.0, h=1.0)
        kit.glass(B, bx + 0.12, by, b["top"], 0.0)
        B.spot("barrel%d" % (k + 1), "stand", (bx, by - 0.65, 0.0), 180.0)
    kit.dart_board(B, XL, 5.3, 0.0, 90.0)
    B.thing("darts", "play", (XL + 0.08, 5.3, 1.73), 90.0, what="darts")
    kit.picture(B, XR, 3.0, 0.0, 270.0, name="painting_sea_night", w=0.8, zc=1.75)
    kit.poster(B, -1.2, YB, 0.0, 0.0, name="poster_cons_3", w=0.6, zc=1.7)
    B.thing("beer_poster", "board", (-1.2, YB - 0.04, 1.7), 0.0, topic="rules")
    kit.coat_rack(B, XR - 0.35, 4.9, 0.0, 270.0, w=1.2)
    _lamp(B, -0.8, 3.2, lumens=1200, kind="green")
    kit.pendant(B, 1.7, 4.3, CEIL, drop=0.7, kind="green")


def pasty(B):
    """Чебуречная: a counter with a fryer and a dish of chebureki, the stoves behind, stand-up tables, the menu."""
    c = kit.shop_counter(B, 1.4, 4.3, 0.0, 0.0, w=3.0, front="CT_PlasterYellow", top="CT_Aluminium")
    pastries(B, 0.7, 4.3, c["top"], 0.0, n=6, mat="CT_WoodLight")
    kit.glass(B, 1.2, 4.2, c["top"], 0.0)
    B.thing("pasty_counter", "counter", (1.4, 4.3, 1.1), 0.0, shop="pasty_bar", service="shop", role="seller")
    B.spot("counter", "counter", (1.4, 3.45, 0.0), 180.0)
    B.spot("staff_seller", "work", (2.1, 5.1, 0.0), 0.0)
    B.spot("staff_cook", "work", (0.5, 5.1, 0.0), 0.0)
    kit.kitchen_unit(B, 1.4, YB, 0.0, 0.0, modules="stove,stove,cupboard,sink", wall_cupboards=True)
    # the deep fryer: a steel box with a dark pan of oil and a basket
    p = _P(B, 0.8, 6.1, 0.9, 0.0, "fryer")
    p.kind = "prop"
    p.box(ALU, -0.25, 0.25, -0.22, 0.22, 0.0, 0.18, f="xXyY")
    p.box("CT_Lacquer", -0.22, 0.22, -0.19, 0.19, 0.0, 0.15, f="Z")
    p.box(STEEL, -0.12, 0.12, -0.1, 0.1, 0.12, 0.2, f="xXyYz")
    p.rod(LAQ, (0.0, -0.1, 0.2), (0.0, -0.3, 0.3), 0.025, 4)
    p.done()
    for i, (tx, ty) in enumerate(((-3.8, 1.5), (-1.7, 1.5), (-2.8, 3.4))):
        t = tall_table(B, tx, ty, 0.0, 0.0)
        pastries(B, tx, ty, t["top"], 0.0, n=3, mat="CT_WoodLight")
        B.spot("table%d" % (i + 1), "stand", (tx, ty - 0.6, 0.0), 180.0)
    kit.menu_board(B, -2.6, YB, 0.0, 0.0, name="menu_0", w=0.8, zc=1.7)
    B.thing("pasty_menu", "board", (-2.6, YB - 0.04, 1.7), 0.0, topic="prices")
    kit.wall_shelf(B, XL, 4.8, 0.0, 90.0, w=1.0, zt=1.5, props=False)
    kit.bottles(B, XL + 0.11, 4.8, 1.5, 90.0, n=4, seed=3)
    kit.poster(B, XR, 3.2, 0.0, 270.0, name="poster_sun_2", w=0.6, zc=1.7)
    kit.wall_phone(B, XR, 4.6, 0.0, 270.0)
    kit.shop_shelf(B, XL, 2.6, 0.0, 90.0, w=1.4, goods="bottles", levels=4, seed=13)
    kit.poster(B, XL, 3.9, 0.0, 90.0, name="poster_cons_2", w=0.55, zc=1.7)
    kit.sign_plate(B, 1.4, 4.3, 0.0, 0.0, name="sign_till", w=0.8, zc=2.55, hang=True)
    kit.radiator(B, -1.0, YB, 0.0, 0.0, w=1.0)
    kit.plant(B, 4.45, 2.4, 0.0, 0.0, h=1.2, kind="ficus")
    B.thing("payphone", "machine", (XR - 0.1, 4.6, 1.45), 270.0, what="payphone")
    _lamp(B, -0.4, 3.2, kind="cone")


DRESS = {"cafe": cafe, "fish": fish, "rental": rental, "workshop": workshop, "dumplings": dumplings, "souvenirs": souvenirs,
         "bakery": bakery, "photo": photo, "barber": barber, "books": books, "beer": beer, "pasty": pasty}


def build(B):
    wall, dado, floor = STYLE[VARIANT]
    _shell(B, wall, dado, floor)
    _sign(B, VARIANT)
    DRESS[VARIANT](B)
    B.note("promenade unit room, variant %s: placed at each W1 unit whose plan variant is %s" % (VARIANT, VARIANT))
    if os.environ.get("KIT_CHECK"):
        print("KITCHECK", VARIANT, kit.check(B))
