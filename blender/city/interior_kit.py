"""Interior furniture kit of the town buildings (pure Python on top of geom/citykit, no bpy: test with python3).

    import interior_kit as kit
    r = kit.chair(B, 2.0, 5.0, yaw=90, kind="tube", mat="CT_PlasterRed")
    B.thing("chair_1", "seat", r["pos"], 90, height=r["height"])

Convention (same as B.spot / B.thing / furn_int2): a piece is placed at a floor point (x, y, z) with yaw in degrees:
0 = the piece FACES -y (the street: a chair's sitter, a shelf's open front, a counter's customer side, a poster's
picture look toward -y), 90 = faces +x, 180 = +y, 270 = -x. Any yaw works (the piece is rotated as a whole).
  * free-standing pieces: (x, y) = the middle of the footprint;
  * wall pieces (marked [wall]): (x, y) = the point on the wall face at the middle of the piece's back; the piece
    stands out of the wall toward its facing. Pictures and signs take zc (height of their middle).
  * props (marked [prop]): (x, y, z) = where they stand, z = the top of the surface (use the "top" a table returned).
Every piece returns a dict: pos (middle of the object: what B.thing wants), yaw, top (height of its top surface or
None), size (w, d, h), front (a floor point 0.6 m in front: where a user stands), seats [(x, y, z, yaw)] for
seating, plus piece-specific points (spots, lamps, places on a counter...). All in building coordinates.

Geometry goes to B.detail (no collision). solid=True (the default for pieces that block walking: counters, shelves,
tables, beds, pianos...; docs/city_interiors.md) also puts ONE thin box into B.m, hidden INSIDE a visible board or
body of the piece (a tabletop, a middle shelf board, the body of a counter), so the game capsule stops at it and
nothing extra is ever seen. Pass solid=False when the piece sits somewhere no one walks.
UVs: metres, projected onto each face's own plane (never stretched); CT_Atlas faces map one atlas cell (upright,
unmirrored seen from the front). Lights: only pendant()/lamps with light=True call B.lamp (the lamp budget!).

CATALOGUE  (name: footprint w x d x h, m; what it is; notable options)
 seating
  chair            0.44x0.44x0.86  kind wood (bentwood-ish) | tube (cantilever steel) | school (plywood on steel)
  stool            d0.36 x0.45     three-legged wooden stool
  bar_stool        d0.40 x0.75     chrome pedestal, footring, red seat
  armchair         0.78x0.80x0.80  mid-century: wooden arms, splayed legs, two cushions
  sofa             w x0.85x0.80    seats=2|3, cushions, wooden plinth or legs
  bench            w x0.40x0.45    kind park (slats, back) | gym (plain plank) | sauna
  cinema_row       n*0.55x0.70     n folding seats, shared armrests, row number plate
  waiting_row      n*0.52x0.60     moulded seats on a steel beam
  bean_bag         d0.80 x0.55     slumped soft bag
  zabuton          0.55x0.55x0.07  Japanese floor cushion
 tables
  table            w x d x0.75     square/rect, tapered legs; kind wood | steel
  table_round      d x0.75         pedestal table (cafe)
  reading_table    w x1.0x0.75     long library table with green-shaded lamps (lamps=n, light=False)
  coffee_table     1.0x0.55x0.42   rounded top, hairpin legs
  school_desk      1.10x0.50x0.76  two-seat Soviet desk (slanted lid, shelf), returns 2 seats (chairs separate)
  office_desk      1.4x0.7x0.75    drawer pedestal, typewriter / papers / phone (options)
  drafting_table   1.2x0.9x1.0     tilted board with a drawing on a trestle
  chabudai         d0.9 x0.33      low round Japanese table
  restaurant_table w x d x0.75     cloth to the floor, place settings for `places`, vase
  chess_table      0.7x0.7x0.72    board inlay and a game in progress
  pingpong_table   2.74x1.53x0.76  table tennis, net, lines
  billiard_table   2.6x1.45x0.82   baize, cushions, pockets, balls
 storage
  bookshelf        w x0.32x2.0     [wall] books as blocks mapped on atlas spines; seed, levels, topic -> spine set
  magazine_rack    0.9x0.35x1.4    [wall] slanted ledges with magazine covers
  card_catalogue   0.9x0.45x1.3    library card drawers on a stand
  wardrobe         1.0x0.58x2.0    [wall] two doors, cornice, legs
  filing_cabinet   0.47x0.62x1.32  [wall] four steel drawers
  lockers          n*0.38x0.45x1.85 [wall] steel lockers with vents
  display_case     1.2x0.6x1.0     open-framed glass case (no panes: goods visible) with goods
  shop_shelf       w x0.45x2.0     [wall] goods = groceries|boxes|jars|fabric|shoes|toys|records|hardware|bottles|pharmacy
  clothes_rail     1.5x0.55x1.6    chrome rail with garments on hangers
  shoe_rack        1.0x0.4x1.1     [wall] slanted shelves with pairs of shoes
  coat_rack        w x0.6x1.75     cloakroom rail on posts with coats, numbered hooks
  safe             0.6x0.6x0.9     [wall] steel safe with dial
 counters (customer side = front; staff stand behind: returns "staff", "customer", "on" = a point on the top)
  shop_counter     w x0.7x1.05     body, overhanging top, till
  bar_counter      w x0.65x1.05    top, footrail, beer taps; back=True adds a back bar with bottles
  reception_desk   w x0.75x1.10    raised front ledge, lower work top, bell
  ticket_booth     1.6x1.3x2.3     a kiosk with a window and a sign (sign=atlas cell)
  lending_desk     w x0.8x1.0      library desk with a stamp and returned books
  pharmacy_counter w x0.65x1.05    glazed display front, back wall of white jars (back=True)
  market_stall     1.8x0.9x0.85    trestle table with crates of produce, posts and a striped canopy (canopy=True)
 home
  bed              single 0.95x2.05 | double 1.65x2.1  headboard, mattress, blanket, pillows
  bunk_bed         0.95x2.05x1.75  steel, ladder, two mattresses
  futon            1.0x2.1x0.12    folded quilt, pillow (Japanese)
  bedside_table    0.45x0.4x0.55   drawer, optional table lamp
  kitchen_unit     0.6x0.6x0.9 per module  modules "cupboard,stove,sink,fridge" (string or list) along x
  samovar          [prop] d0.3 x0.5 brass samovar with teapot crown
  tv_set           0.7x0.45x1.05   wooden TV on legs, screen = atlas cell
  radio            [prop] 0.5x0.22x0.3 valve radio with dial
  floor_lamp       d0.4 x1.6       shade, brass stem; light=False
  table_lamp       [prop] d0.25 x0.45 light=False
  wall_clock       [wall] d0.32    zc
  mirror           [wall] 0.6x1.2  zc, frame
  rug              w x d           atlas rug_* cell (round=True for rug_round_*)
  curtains         [wall] w x h    two drapes and a rod (mat)
  radiator         [wall] w x0.1x0.6 cast-iron fins
  plant            kind ficus | monstera | palm | cactus | fern, pot colour
  aquarium         1.0x0.4x1.25    on a cabinet, back picture, fish
  hat_stand        d0.45 x1.8      bentwood coat stand
 culture, leisure
  piano            1.5x0.6x1.25    upright, keyboard from atlas, music sheet, returns seat point for a stool
  grand_piano      1.5x1.9x1.0     lid propped up, keyboard, three legs
  stage            w x d x h       low platform with steps and side curtains (curtain=True)
  cinema_screen    [wall] w x h    frame, black masking, picture = atlas cell (screen_*)
  projector        [prop] 0.55x0.3x0.45 film projector with two reels
  easel            0.6x0.6x1.75    tripod easel with a canvas (atlas cell)
  potters_wheel    0.8x0.6x0.6     wheel, tray, seat stool
  sewing_table     1.0x0.5x0.75    treadle table with a black machine
  loom             1.2x1.0x1.5     frame loom with woven cloth (atlas fabric)
  globe            [prop] d0.35 x0.5 on a stand (or floor=True on a tall stand)
  telescope        0.8x0.8x1.5     on a tripod
  record_player    [prop] 0.45x0.35x0.15 with a record
  jukebox          0.75x0.6x1.55   arched top, glowing panels, sleeves
  arcade           0.65x0.8x1.75   cabinet, slanted screen (screen_arcade_*), control panel
  photo_booth      1.0x1.3x2.1     booth with curtain, sign, stool inside
  table_football   1.4x0.75x0.9    rods, little players
  dart_board       [wall] d0.45    cabinet with doors open, throw line
  slide_projector  [prop] 0.3x0.3x0.15 carousel projector
 sport, health
  wall_bars        [wall] w x0.15x2.6 Swedish ladder
  barbell_rack     1.2x0.6x1.2     stand with a loaded barbell and plates
  bench_press      1.8x1.2x1.25    bench with uprights and bar
  punching_bag     [ceiling hung] d0.35 x1.0 (top_z = ceiling)
  vaulting_horse   1.6x0.4x1.1     leather body on legs
  mats             w x d x0.08     gym mats (n stacked)
  basketball_hoop  [wall] backboard, ring, net (rim at 3.05 by default; ring_z)
  exercise_bike    1.0x0.5x1.15    stationary bike
  medical_couch    1.9x0.65x0.7    leather couch with headrest, paper sheet
  eye_chart        [wall] lit box with the eye chart
  scales           0.5x0.4x1.6     medical column scales with height rod
  screen_divider   1.6x0.4x1.7     three-panel folding screen (fabric atlas)
  dentist_chair    1.6x0.8x1.3     chair, lamp arm, spittoon, tray
 school
  blackboard       [wall] w x1.2   chalk writing (board_*), chalk tray, frame
  teacher_desk     1.3x0.7x0.78    desk on a low podium, register, globe option
  wall_map         [wall] w x h    rolled map on a stick (map_*)
  skeleton         0.5x0.4x1.75    on a stand
  lab_bench        w x0.75x0.9     tiled top, sink, flasks and burners
 walls and decor (all [wall], zc = height of the middle)
  picture          w x h  framed atlas cell (painting_* etc.), frame mat
  poster           w x h  unframed paper on the wall (any atlas cell)
  notice_board     w x h  cork board with notes (notice_*)
  banner           w x h  cloth banner (cell) on a pole
  mosaic_panel     w x h  tile_* panel set into a slim frame
  wall_shelf       w      a shelf on brackets (returns "top" for props)
  sign_plate       w x h  sign_* plate (also hung over counters: hang=True)
  menu_board       w x h  menu_*/price_* board
  fire_extinguisher       red, on a bracket
  wall_phone              payphone with handset and dial
  pendant          [ceiling] (x, y, ceiling z) a lamp on a cord; light=False: only when the caller asks it calls B.lamp
  ceiling_fan      [ceiling] four blades
 props ([prop]: z = top of a surface)
  books_stack, newspaper, cup, teapot, plates, bottles, vase, typewriter, telephone, cash_register, kitchen_scales,
  crate, sack, cardboard_box, tools, chess_set, paint_jars, glass, lamp_green (the reading lamp alone)
 layout helpers
  row(fn, B, x0, y0, x1, y1, n, yaw, **kw)    n pieces evenly between two points
  grid(fn, B, x0, y0, nx, ny, dx, dy, yaw, **kw)
  around_table(B, x, y, table_w, table_d, yaw, chair=chair, per_side=(1, 1, 0, 0), **kw)  chairs facing a table
  Room(B, poly or rect, z, seed): keep(path rects/doors) / place(fn, ...) / along(side, fns, ...) / scatter(...)
  check(B) -> list of warnings: pieces overlapping each other, entrances' clear zone, spots inside solid pieces
  cell(name_or_group, rnd) -> an atlas cell name;  ATLAS: {name: [u0, v0, u1, v1]}

Adding a piece: write `def name(B, x, y, z=0.0, yaw=0.0, solid=..., **opts)`; build in the local frame with
p = _P(B, x, y, z, yaw, "name") (front = -y, origin at the floor point / wall point), p.box/bev/cyl/loft/rod/card...,
p.core(...) for the hidden collision board, and return p.done(...). Keep the triangle budget (p.tris), give parts a
few mm of offset (never coplanar faces facing the same way), add a line to the catalogue above and to
kit_showroom.py, look at the shots.
"""
import json
import math
import os
import random

from citykit import Mesh, geom

HERE = os.path.dirname(os.path.abspath(__file__))
ATLAS_FILE = os.path.join(HERE, "atlas_cells.json")
try:
    with open(ATLAS_FILE) as _f:
        ATLAS = {k: v["uv"] for k, v in json.load(_f)["cells"].items()}
except (OSError, ValueError, KeyError):            # no atlas yet: a grey corner of the texture
    ATLAS = {}
ATLAS_SLOT = "CT_Atlas"

WOOD, WOODL = "CT_Wood", "CT_WoodLight"
STEEL, ALU, BRASS, LAQ = "CT_Steel", "CT_Aluminium", "CT_Brass", "CT_Lacquer"
FABRICS = ["CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterGreen", "CT_PlasterYellow", "CT_PlasterPink", "CT_Leather"]
PAINTS = ["CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterGreen", "CT_TileWhite", "CT_SteelRed",
          "CT_PlasterPink", "CT_Paper"]


def cell(name, rnd=None):
    """An atlas cell by exact name, or one of a group (prefix 'poster' -> poster_*), chosen with rnd."""
    if name in ATLAS:
        return name
    pre = name if name.endswith("_") else name + "_"
    names = sorted(k for k in ATLAS if k.startswith(pre))
    if not names:
        return name
    return (rnd or random.Random(len(name))).choice(names)


def _uvrect(name):
    return ATLAS.get(name, [0.0, 0.0, 0.002, 0.002])


def _rng(seed, x, y, extra=""):
    return random.Random("%s|%.2f|%.2f|%s" % (seed, x, y, extra))


# ------------------------------------------------------------------------------------------------ piece builder
def _plane_uv(pts):
    """Metre UVs projected onto the face's own plane (u horizontal along the face, v up the face)."""
    n = geom.norm(geom.cross(geom.sub(pts[1], pts[0]), geom.sub(pts[2], pts[0])))
    if abs(n[2]) > 0.95:
        u, v = (1.0, 0.0, 0.0), (0.0, 1.0 if n[2] > 0 else -1.0, 0.0)
    else:
        u = geom.norm(geom.cross((0.0, 0.0, 1.0), n))
        v = geom.cross(n, u)
    return [(p[0] * u[0] + p[1] * u[1] + p[2] * u[2], p[0] * v[0] + p[1] * v[1] + p[2] * v[2]) for p in pts]


def _ring_rect(x0, x1, y0, y1, z, c=0.0):
    """Rectangle (chamfered corners when c > 0) CCW seen from above, at z."""
    if c <= 0:
        return [(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)]
    return [(x0 + c, y0, z), (x1 - c, y0, z), (x1, y0 + c, z), (x1, y1 - c, z), (x1 - c, y1, z), (x0 + c, y1, z),
            (x0, y1 - c, z), (x0, y0 + c, z)]


def _ring_circle(x, y, r, z, segs=8, rx=None, a0=0.0):
    rx = r if rx is None else rx
    return [(x + rx * math.cos(a0 + 2 * math.pi * k / segs), y + r * math.sin(a0 + 2 * math.pi * k / segs), z)
            for k in range(segs)]


class _P:
    """One piece under construction, in its local frame: origin = the placement point, front = -y, z up."""

    def __init__(self, B, x, y, z, yaw, name):
        self.B, self.name = B, name
        self.x, self.y, self.z, self.yaw = x, y, z, yaw
        self.fr = geom.Frame(x, y, math.radians(yaw), oz=z)
        self.m = Mesh()
        self.cores = []
        self.kind = "floor"

    # ---------------------------------------------------------------- world points
    def w(self, u, v, h=0.0):
        p = self.fr((u, v, h))
        return (round(p[0], 3), round(p[1], 3), round(p[2], 3))

    # ---------------------------------------------------------------- primitives (local)
    def face(self, mat, pts):
        self.m.face(mat, pts)

    def loft(self, mat, rings, top=True, bottom=False):
        """Side quads between consecutive rings (each CCW from above, same count), optional caps."""
        n = len(rings[0])
        for a, b in zip(rings, rings[1:]):
            for i in range(n):
                j = (i + 1) % n
                q = [a[i], a[j], b[j], b[i]]
                if math.dist(q[0], q[1]) < 1e-6 and math.dist(q[2], q[3]) < 1e-6:
                    continue
                if math.dist(q[0], q[1]) < 1e-6:
                    q = [q[0], q[2], q[3]]
                elif math.dist(q[2], q[3]) < 1e-6:
                    q = [q[0], q[1], q[2]]
                self.m.face(mat, q)
        if top and len(rings[-1]) >= 3 and _span(rings[-1]) > 1e-6:
            self.m.face(mat, list(rings[-1]))
        if bottom and _span(rings[0]) > 1e-6:
            self.m.face(mat, list(reversed(rings[0])))

    def box(self, mat, x0, x1, y0, y1, z0, z1, f="xXyYZ"):
        geom.box(self.m, mat, x0, x1, y0, y1, z0, z1, faces=f)

    def bev(self, mat, x0, x1, y0, y1, z0, z1, b=0.012, c=0.0, bottom=False):
        """A board / top with a chamfered top edge (b) and optionally chamfered vertical corners (c)."""
        b = min(b, (z1 - z0) * 0.6, (x1 - x0) * 0.3, (y1 - y0) * 0.3)
        r0 = _ring_rect(x0, x1, y0, y1, z0, c)
        r1 = _ring_rect(x0, x1, y0, y1, z1 - b, c)
        r2 = _ring_rect(x0 + b, x1 - b, y0 + b, y1 - b, z1, max(0.0, c - b * 0.4) if c else 0.0)
        if len(r2) != len(r0):
            r2 = _ring_rect(x0 + b, x1 - b, y0 + b, y1 - b, z1, 1e-4)
        self.loft(mat, [r0, r1, r2], top=True, bottom=bottom)

    def cushion(self, mat, x0, x1, y0, y1, z0, z1, puff=0.02):
        """A soft block: inset at the bottom and the top, full in the middle (8 sides, pillowy)."""
        h = z1 - z0
        c = min(0.04, (x1 - x0) * 0.2, (y1 - y0) * 0.2)
        rings = [_ring_rect(x0 + puff, x1 - puff, y0 + puff, y1 - puff, z0, c),
                 _ring_rect(x0, x1, y0, y1, z0 + h * 0.35, c),
                 _ring_rect(x0, x1, y0, y1, z0 + h * 0.7, c),
                 _ring_rect(x0 + puff * 1.5, x1 - puff * 1.5, y0 + puff * 1.5, y1 - puff * 1.5, z1, c * 0.6)]
        self.loft(mat, rings, top=True, bottom=False)

    def cyl(self, mat, x, y, z0, z1, r, segs=8, r1=None, top=True, bottom=False, a0=None):
        r1 = r if r1 is None else r1
        a0 = math.pi / segs if a0 is None else a0
        self.loft(mat, [_ring_circle(x, y, r, z0, segs, a0=a0), _ring_circle(x, y, r1, z1, segs, a0=a0)],
                  top=top and r1 > 1e-6, bottom=bottom)

    def lathe(self, mat, x, y, prof, segs=8, top=True, bottom=False):
        """A body of revolution: prof = [(r, z), ...] from the bottom up."""
        self.loft(mat, [_ring_circle(x, y, r, z, segs) for r, z in prof], top=top, bottom=bottom)

    def rod(self, mat, a, b, w=0.03, sides=4, caps=False):
        """A bar of `sides` faces between local points a and b (4: square of side w; 3: triangle; 5+: round, diameter w)."""
        d = geom.norm(geom.sub(b, a))
        ref = (0, 0, 1) if abs(d[2]) < 0.9 else (1, 0, 0)
        u = geom.norm(geom.cross(d, ref))
        v = geom.cross(d, u)
        if sides == 4:
            angs, rad = [math.pi / 4 + k * math.pi / 2 for k in range(4)], w / 2 * math.sqrt(2)
        elif sides == 3:
            angs, rad = [math.pi / 2 + k * 2 * math.pi / 3 for k in range(3)], w * 0.58
        else:
            angs, rad = [math.pi / sides + k * 2 * math.pi / sides for k in range(sides)], w / 2
        ra = [tuple(a[i] + (u[i] * math.cos(t) + v[i] * math.sin(t)) * rad for i in range(3)) for t in angs]
        rb = [tuple(b[i] + (u[i] * math.cos(t) + v[i] * math.sin(t)) * rad for i in range(3)) for t in angs]
        cen = tuple((a[i] + b[i]) / 2 for i in range(3))
        n = len(ra)
        quads = [[ra[k], ra[(k + 1) % n], rb[(k + 1) % n], rb[k]] for k in range(n)]
        if caps:
            quads += [ra[::-1], rb]
        for q in quads:
            nn = geom.cross(geom.sub(q[1], q[0]), geom.sub(q[2], q[0]))
            mid = tuple(sum(p[i] for p in q) / len(q) for i in range(3))
            if sum(nn[i] * (mid[i] - cen[i]) for i in range(3)) < 0:
                q = q[::-1]
            self.m.face(mat, q)

    def tube(self, mat, pts, w=0.025, sides=3):
        for a, b in zip(pts, pts[1:]):
            self.rod(mat, a, b, w, sides)

    def hexa(self, mat, bot, top, f="all"):
        """A sheared box from 4 bottom and 4 top points (any winding)."""
        area = sum(bot[i][0] * bot[(i + 1) % 4][1] - bot[(i + 1) % 4][0] * bot[i][1] for i in range(4))
        if area < 0:
            bot = [bot[0], bot[3], bot[2], bot[1]]
            top = [top[0], top[3], top[2], top[1]]
        if f == "all" or "Z" in f:
            self.m.face(mat, list(top))
        if f == "all" or "z" in f:
            self.m.face(mat, list(reversed(bot)))
        for i in range(4):
            j = (i + 1) % 4
            self.m.face(mat, [bot[i], bot[j], top[j], top[i]])

    def slab(self, mat, p0, p1, p2, p3, t, f="all"):
        """A thin slab: the quad p0..p3 (CCW seen from its front) extruded t BACKWARD (behind its face)."""
        n = geom.norm(geom.cross(geom.sub(p1, p0), geom.sub(p3, p0)))
        back = [tuple(p[i] - n[i] * t for i in range(3)) for p in (p0, p1, p2, p3)]
        front = [p0, p1, p2, p3]
        self.m.face(mat, front)
        if f == "all":
            self.m.face(mat, back[::-1])
        for i in range(4):
            j = (i + 1) % 4
            self.m.face(mat, [back[i], back[j], front[j], front[i]])

    # ---------------------------------------------------------------- atlas cards
    def card(self, name, bl, br, tr, tl, rect=None):
        """An atlas quad: corners bottom-left, bottom-right, top-right, top-left SEEN FROM ITS FRONT (CCW).
        rect: (a0, b0, a1, b1) fractions of the cell to use (default the whole cell)."""
        u0, v0, u1, v1 = _uvrect(name)
        if rect:
            a0, b0, a1, b1 = rect
            u0, u1, v0, v1 = u0 + (u1 - u0) * a0, u0 + (u1 - u0) * a1, v0 + (v1 - v0) * b0, v0 + (v1 - v0) * b1
        self.m.face(ATLAS_SLOT, [bl, br, tr, tl], [(u0, v0), (u1, v0), (u1, v1), (u0, v1)])

    def card_front(self, name, x0, x1, z0, z1, y, rect=None):
        """A card in the plane y facing -y (the piece's front)."""
        self.card(name, (x0, y, z0), (x1, y, z0), (x1, y, z1), (x0, y, z1), rect)

    def card_back(self, name, x0, x1, z0, z1, y, rect=None):
        self.card(name, (x1, y, z0), (x0, y, z0), (x0, y, z1), (x1, y, z1), rect)

    def card_side(self, name, y0, y1, z0, z1, x, facing=1, rect=None):
        """A card in the plane x facing +x (facing=1, picture right = +y) or -x."""
        if facing > 0:
            self.card(name, (x, y0, z0), (x, y1, z0), (x, y1, z1), (x, y0, z1), rect)
        else:
            self.card(name, (x, y1, z0), (x, y0, z0), (x, y0, z1), (x, y1, z1), rect)

    def card_top(self, name, x0, x1, y0, y1, z, rect=None, turn=0):
        """A card lying flat facing up; the picture's top edge toward +y (turn=1: toward -x, 2: -y, 3: +x)."""
        c = [(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)]
        c = c[turn % 4:] + c[:turn % 4]
        self.card(name, *c, rect=rect)

    def card_tilt(self, name, x0, x1, ya, za, yb, zb, rect=None):
        """A card from the lower edge (y=ya, z=za) up to (yb, zb), facing toward -y/up (a slanted lid or screen)."""
        self.card(name, (x0, ya, za), (x1, ya, za), (x1, yb, zb), (x0, yb, zb), rect)

    # ---------------------------------------------------------------- collision and finishing
    def core(self, x0, x1, y0, y1, z0, z1):
        """The hidden collision box (local), into B.m when the piece is solid. Keep it INSIDE a visible solid part."""
        self.cores.append((x0, x1, y0, y1, z0, z1))

    @property
    def tris(self):
        return self.m.tri_count()

    def done(self, solid=False, top=None, front=0.6, seats=(), **extra):
        m = self.m
        # Dima's rule (geom.coplanar_offsets): of overlapping faces in one plane facing the same way the smaller
        # goes 4 mm over the larger (a label on a drawer, a picture on its board), so nothing z-fights
        geom.separate_coplanar(m, 0.004)
        for k, (mat, idx, uv) in enumerate(m.faces):
            if mat != ATLAS_SLOT:
                m.faces[k] = (mat, idx, _plane_uv([m.verts[i] for i in idx]))
        world = self.fr.apply(m)
        self.B.detail.extend(world)
        if solid:
            for (x0, x1, y0, y1, z0, z1) in self.cores:
                poly = [self.fr((u, v, 0))[:2] for u, v in ((x0, y0), (x1, y0), (x1, y1), (x0, y1))]
                geom.prism(self.B.m, "CT_Steel", poly, self.z + z0, self.z + z1, top=True, bottom=True)
        xs = [p[0] for p in m.verts] or [0]
        ys = [p[1] for p in m.verts] or [0]
        zs = [p[2] for p in m.verts] or [0]
        x0, x1, y0, y1, z0, z1 = min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)
        foot = [self.w(u, v)[:2] for u, v in ((x0, y0), (x1, y0), (x1, y1), (x0, y1))]
        out = {"name": self.name, "pos": self.w((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), "yaw": self.yaw,
               "top": None if top is None else round(self.z + top, 3), "size": (round(x1 - x0, 3), round(y1 - y0, 3),
                                                                               round(z1 - z0, 3)),
               "front": self.w((x0 + x1) / 2, y0 - front, 0.0), "seats": [], "tris": m.tri_count(),
               "foot": foot, "solid": bool(solid and self.cores)}
        for (u, v, h, dyaw) in seats:
            p = self.w(u, v, h)
            out["seats"].append((p[0], p[1], p[2], (self.yaw + dyaw) % 360))
        if out["seats"]:
            out["height"] = round(seats[0][2], 3)
        for k, val in extra.items():
            if isinstance(val, tuple) and len(val) == 3 and all(isinstance(t, (int, float)) for t in val):
                out[k] = self.w(*val)
            elif isinstance(val, list) and val and isinstance(val[0], tuple) and len(val[0]) == 3:
                out[k] = [self.w(*t) for t in val]
            else:
                out[k] = val
        items = getattr(self.B, "kit_items", None)
        if items is None:
            items = []
            self.B.kit_items = items
        items.append({"name": self.name, "foot": foot, "z0": self.z + z0, "z1": self.z + z1, "kind": self.kind,
                      "solid": out["solid"]})
        return out


def _span(ring):
    xs = [p[0] for p in ring]
    ys = [p[1] for p in ring]
    return (max(xs) - min(xs)) * (max(ys) - min(ys))


def _legs(p, mat, xs, ys, z1, w=0.035, taper=0.7, sides=4):
    """Four tapered legs at the corners (xs, ys), floor to z1 (no caps)."""
    for x in xs:
        for y in ys:
            p.cyl(mat, x, y, 0.0, z1, w / 2 * taper, sides, r1=w / 2, top=False, a0=math.pi / 4)


# ================================================================================================ SEATING
def chair(B, x, y, z=0.0, yaw=0.0, kind="wood", mat=None, frame=None, solid=False, seed=0):
    """A chair; the sitter faces yaw. kind wood | tube | school."""
    p = _P(B, x, y, z, yaw, "chair")
    h = 0.45
    if kind == "tube":
        fm = frame or ALU
        mt = mat or "CT_PlasterRed"
        for s in (-0.2, 0.2):
            p.tube(fm, [(s, 0.17, 0.012), (s, -0.21, 0.012), (s, -0.21, h - 0.06), (s, 0.15, h - 0.05),
                        (s, 0.22, 0.86)], 0.024, 3)
        p.box(mt, -0.22, 0.22, -0.23, 0.18, h - 0.04, h, f="xXyYZz")
        p.hexa(mt, [(-0.22, 0.16, h + 0.08), (0.22, 0.16, h + 0.08), (0.22, 0.2, h + 0.08), (-0.22, 0.2, h + 0.08)],
               [(-0.22, 0.2, 0.84), (0.22, 0.2, 0.84), (0.22, 0.24, 0.84), (-0.22, 0.24, 0.84)])
    elif kind == "school":
        fm = frame or STEEL
        mt = mat or WOODL
        p.box(mt, -0.2, 0.2, -0.2, 0.18, h - 0.025, h, f="xXyYZz")
        for s in (-0.18, 0.18):
            p.rod(fm, (s, -0.17, 0.0), (s, -0.16, h - 0.025), 0.022, 3)
            p.rod(fm, (s, 0.15, 0.0), (s, 0.17, 0.82), 0.022, 3)
            p.rod(fm, (s, -0.16, 0.12), (s, 0.155, 0.12), 0.018, 3)
        p.hexa(mt, [(-0.2, 0.15, 0.6), (0.2, 0.15, 0.6), (0.2, 0.175, 0.6), (-0.2, 0.175, 0.6)],
               [(-0.2, 0.17, 0.8), (0.2, 0.17, 0.8), (0.2, 0.195, 0.8), (-0.2, 0.195, 0.8)])
    else:
        mt = mat or WOOD
        p.bev(mt, -0.21, 0.21, -0.2, 0.19, h - 0.035, h, 0.012)
        for s in (-0.17, 0.17):
            p.rod(mt, (s, -0.16, 0.0), (s, -0.15, h - 0.035), 0.032, 4)
            p.rod(mt, (s, 0.14, 0.0), (s, 0.17, 0.88), 0.032, 4)
        for zz in (0.62, 0.8):
            p.hexa(mt, [(-0.19, 0.155 + (zz - 0.45) * 0.07, zz - 0.05), (0.19, 0.155 + (zz - 0.45) * 0.07, zz - 0.05),
                        (0.19, 0.18 + (zz - 0.45) * 0.07, zz - 0.05), (-0.19, 0.18 + (zz - 0.45) * 0.07, zz - 0.05)],
                   [(-0.19, 0.16 + (zz - 0.45) * 0.07, zz + 0.04), (0.19, 0.16 + (zz - 0.45) * 0.07, zz + 0.04),
                    (0.19, 0.185 + (zz - 0.45) * 0.07, zz + 0.04), (-0.19, 0.185 + (zz - 0.45) * 0.07, zz + 0.04)])
    return p.done(solid=False, seats=[(0.0, 0.0, h, 0.0)])


def stool(B, x, y, z=0.0, yaw=0.0, mat=WOOD, h=0.45, solid=False):
    p = _P(B, x, y, z, yaw, "stool")
    p.cyl(mat, 0, 0, h - 0.035, h, 0.17, 8, r1=0.16)
    for k in range(3):
        a = 2 * math.pi * k / 3 + 0.5
        p.rod(mat, (0.2 * math.cos(a), 0.2 * math.sin(a), 0.0), (0.11 * math.cos(a), 0.11 * math.sin(a), h - 0.035), 0.03, 3)
    return p.done(seats=[(0, 0, h, 0.0)])


def bar_stool(B, x, y, z=0.0, yaw=0.0, mat="CT_PlasterRed", h=0.75, solid=False):
    p = _P(B, x, y, z, yaw, "bar_stool")
    p.cyl(ALU, 0, 0, 0.0, 0.03, 0.2, 5, r1=0.18)
    p.cyl(ALU, 0, 0, 0.03, h - 0.06, 0.03, 4, top=False)
    p.lathe(mat, 0, 0, [(0.08, h - 0.07), (0.18, h - 0.04), (0.17, h)], 7)
    ring = _ring_circle(0, 0, 0.16, 0.3, 4)
    for a, b in zip(ring, ring[1:] + ring[:1]):
        p.rod(ALU, a, b, 0.018, 3)
    return p.done(seats=[(0, 0, h, 0.0)])


def armchair(B, x, y, z=0.0, yaw=0.0, mat="CT_PlasterGreen", wood=WOOD, solid=True):
    p = _P(B, x, y, z, yaw, "armchair")
    for sx in (-0.33, 0.33):
        for sy, lean in ((-0.3, -0.04), (0.3, 0.05)):
            p.rod(wood, (sx * 1.08, sy + lean, 0.0), (sx, sy, 0.3), 0.04, 4)
    p.bev(wood, -0.37, 0.37, -0.36, 0.36, 0.26, 0.3, 0.01)
    p.cushion(mat, -0.3, 0.3, -0.36, 0.28, 0.3, 0.44)
    p.hexa(mat, [(-0.3, 0.2, 0.36), (0.3, 0.2, 0.36), (0.3, 0.36, 0.36), (-0.3, 0.36, 0.36)],
           [(-0.3, 0.3, 0.8), (0.3, 0.3, 0.8), (0.3, 0.42, 0.8), (-0.3, 0.42, 0.8)])
    for s in (-1, 1):
        xa = s * 0.345
        p.rod(wood, (xa, 0.28, 0.3), (xa, 0.3, 0.6), 0.035, 4)
        p.rod(wood, (xa, -0.3, 0.3), (xa, -0.3, 0.58), 0.035, 4)
        p.bev(wood, xa - 0.04, xa + 0.04, -0.38, 0.34, 0.58, 0.61, 0.008)
    p.core(-0.28, 0.28, -0.3, 0.25, 0.31, 0.42)
    return p.done(solid=solid, seats=[(0, -0.05, 0.44, 0.0)])


def sofa(B, x, y, z=0.0, yaw=0.0, seats=3, mat="CT_PlasterBlue", wood=WOOD, solid=True):
    p = _P(B, x, y, z, yaw, "sofa")
    sw = 0.62
    W = seats * sw + 0.3
    hw = W / 2
    for sx in (-hw + 0.08, hw - 0.08):
        for sy in (-0.34, 0.34):
            p.rod(wood, (sx * 1.02, sy * 1.05, 0.0), (sx, sy, 0.16), 0.04, 4)
    p.bev(wood, -hw, hw, -0.4, 0.42, 0.15, 0.2, 0.01)
    for s in (-1, 1):
        p.cushion(mat, s * hw - (0.15 if s > 0 else 0), s * hw + (0.15 if s < 0 else 0), -0.4, 0.42, 0.2, 0.6, 0.015)
    p.box(mat, -hw + 0.15, hw - 0.15, 0.24, 0.42, 0.2, 0.42, f="yZ")
    seat_pts = []
    for k in range(seats):
        a = -hw + 0.15 + k * sw
        p.cushion(mat, a + 0.005, a + sw - 0.005, -0.38, 0.24, 0.2, 0.43, 0.014)
        p.hexa(mat, [(a + 0.01, 0.12, 0.42), (a + sw - 0.01, 0.12, 0.42), (a + sw - 0.01, 0.32, 0.42), (a + 0.01, 0.32, 0.42)],
               [(a + 0.01, 0.24, 0.82), (a + sw - 0.01, 0.24, 0.82), (a + sw - 0.01, 0.4, 0.82), (a + 0.01, 0.4, 0.82)])
        seat_pts.append((a + sw / 2, -0.08, 0.43, 0.0))
    p.core(-hw + 0.05, hw - 0.05, -0.35, 0.38, 0.21, 0.4)
    return p.done(solid=solid, seats=seat_pts)


def bench(B, x, y, z=0.0, yaw=0.0, w=1.6, kind="park", mat=WOOD, frame=STEEL, solid=False):
    p = _P(B, x, y, z, yaw, "bench")
    hw = w / 2
    if kind == "gym":
        p.bev(mat, -hw, hw, -0.14, 0.14, 0.4, 0.45, 0.01)
        for sx in (-hw + 0.15, hw - 0.15):
            p.box(mat, sx - 0.02, sx + 0.02, -0.12, 0.12, 0.0, 0.4, f="xXyY")
        return p.done(seats=[(0, 0, 0.45, 0.0)])
    if kind == "sauna":
        for k in range(4):
            p.box(WOODL, -hw, hw, -0.2 + k * 0.105, -0.2 + k * 0.105 + 0.09, 0.42, 0.45)
        for sx in (-hw + 0.1, hw - 0.1):
            p.box(WOODL, sx - 0.03, sx + 0.03, -0.18, 0.2, 0.0, 0.42, f="xXyY")
        return p.done(seats=[(0, 0, 0.45, 0.0)])
    for k in range(4):
        y0 = -0.2 + k * 0.1
        p.bev(mat, -hw, hw, y0, y0 + 0.085, 0.41, 0.45, 0.008)
    for k in range(2):
        zz = 0.58 + k * 0.14
        p.hexa(mat, [(-hw, 0.2 + (zz - 0.5) * 0.15, zz), (hw, 0.2 + (zz - 0.5) * 0.15, zz), (hw, 0.225 + (zz - 0.5) * 0.15, zz),
                     (-hw, 0.225 + (zz - 0.5) * 0.15, zz)],
               [(-hw, 0.21 + (zz - 0.5) * 0.15, zz + 0.1), (hw, 0.21 + (zz - 0.5) * 0.15, zz + 0.1),
                (hw, 0.235 + (zz - 0.5) * 0.15, zz + 0.1), (-hw, 0.235 + (zz - 0.5) * 0.15, zz + 0.1)])
    for sx in (-hw + 0.12, hw - 0.12):
        p.tube(frame, [(sx, -0.22, 0.0), (sx, -0.18, 0.41), (sx, 0.2, 0.41), (sx, 0.24, 0.86)], 0.04, 4)
        p.rod(frame, (sx, 0.2, 0.0), (sx, 0.2, 0.41), 0.04, 4)
    return p.done(seats=[(-w / 4, -0.03, 0.45, 0.0), (w / 4, -0.03, 0.45, 0.0)])


def cinema_row(B, x, y, z=0.0, yaw=0.0, n=6, mat="CT_PlasterRed", frame=STEEL, number=None, solid=True):
    """n folding cinema seats side by side (x), facing yaw; seats spaced 0.55 m."""
    p = _P(B, x, y, z, yaw, "cinema_row")
    sp = 0.55
    W = n * sp
    x0 = -W / 2
    for k in range(n + 1):
        xa = x0 + k * sp
        p.box(frame, xa - 0.025, xa + 0.025, -0.2, 0.25, 0.0, 0.6, f="xXyYZ")
        p.bev(WOOD, xa - 0.035, xa + 0.035, -0.28, 0.2, 0.6, 0.64, 0.008)
    seats = []
    for k in range(n):
        a = x0 + k * sp + 0.03
        b = a + sp - 0.06
        p.hexa(mat, [(a, -0.25, 0.36), (b, -0.25, 0.36), (b, 0.12, 0.4), (a, 0.12, 0.4)],
               [(a, -0.25, 0.45), (b, -0.25, 0.45), (b, 0.12, 0.47), (a, 0.12, 0.47)], f="xXyYZz")
        p.hexa(mat, [(a, 0.12, 0.4), (b, 0.12, 0.4), (b, 0.24, 0.4), (a, 0.24, 0.4)],
               [(a, 0.2, 0.98), (b, 0.2, 0.98), (b, 0.3, 0.98), (a, 0.3, 0.98)])
        seats.append((a + (b - a) / 2, -0.06, 0.45, 0.0))
    p.core(x0 + 0.03, -x0 - 0.03, 0.16, 0.22, 0.42, 0.9)
    if number is not None and "sign_till" in ATLAS:
        pass
    return p.done(solid=solid, seats=seats)


def waiting_row(B, x, y, z=0.0, yaw=0.0, n=4, mat="CT_PlasterYellow", frame=STEEL, solid=False):
    p = _P(B, x, y, z, yaw, "waiting_row")
    sp = 0.52
    W = n * sp
    p.box(frame, -W / 2, W / 2, -0.04, 0.04, 0.3, 0.36)
    for sx in (-W / 2 + 0.2, W / 2 - 0.2):
        p.tube(frame, [(sx, -0.25, 0.0), (sx, 0.0, 0.3)], 0.04, 4)
        p.tube(frame, [(sx, 0.25, 0.0), (sx, 0.0, 0.3)], 0.04, 4)
    seats = []
    for k in range(n):
        a = -W / 2 + k * sp + 0.02
        b = a + sp - 0.04
        p.hexa(mat, [(a, -0.24, 0.42), (b, -0.24, 0.42), (b, 0.18, 0.4), (a, 0.18, 0.4)],
               [(a, -0.24, 0.45), (b, -0.24, 0.45), (b, 0.18, 0.43), (a, 0.18, 0.43)])
        p.hexa(mat, [(a, 0.18, 0.4), (b, 0.18, 0.4), (b, 0.21, 0.4), (a, 0.21, 0.4)],
               [(a, 0.26, 0.84), (b, 0.26, 0.84), (b, 0.29, 0.84), (a, 0.29, 0.84)])
        p.box(frame, a + 0.2, a + 0.28, -0.04, 0.04, 0.36, 0.405, f="xXyY")
        seats.append(((a + b) / 2, -0.04, 0.45, 0.0))
    return p.done(seats=seats)


def bean_bag(B, x, y, z=0.0, yaw=0.0, mat="CT_PlasterYellow", solid=False):
    p = _P(B, x, y, z, yaw, "bean_bag")
    prof = [(0.36, 0.0), (0.42, 0.12), (0.4, 0.3), (0.3, 0.42), (0.16, 0.48)]
    rings = []
    for r, h in prof:
        rg = _ring_circle(0, 0.03, r, h, 10)
        rings.append([(px, py + (0.06 if py > 0.03 else 0) * (h / 0.48), pz + (0.06 * max(0.0, py) / 0.4 if h > 0.2 else 0))
                      for px, py, pz in rg])
    p.loft(mat, rings, top=True)
    return p.done(seats=[(0, -0.08, 0.35, 0.0)])


def zabuton(B, x, y, z=0.0, yaw=0.0, mat="CT_PlasterBlue", solid=False):
    p = _P(B, x, y, z, yaw, "zabuton")
    p.cushion(mat, -0.27, 0.27, -0.27, 0.27, 0.0, 0.07, 0.02)
    return p.done(seats=[(0, 0, 0.07, 0.0)])


# ================================================================================================ TABLES
def table(B, x, y, z=0.0, yaw=0.0, w=1.2, d=0.8, h=0.75, mat=WOODL, legs=None, kind="wood", solid=True):
    p = _P(B, x, y, z, yaw, "table")
    lm = legs or (mat if kind == "wood" else STEEL)
    p.bev(mat, -w / 2, w / 2, -d / 2, d / 2, h - 0.035, h, 0.012, bottom=True)
    lw = 0.045 if kind == "wood" else 0.03
    _legs(p, lm, (-w / 2 + 0.06, w / 2 - 0.06), (-d / 2 + 0.06, d / 2 - 0.06), h - 0.035, lw, 0.65 if kind == "wood" else 1.0)
    p.core(-w / 2 + 0.01, w / 2 - 0.01, -d / 2 + 0.01, d / 2 - 0.01, h - 0.03, h - 0.008)
    return p.done(solid=solid, top=h)


def table_round(B, x, y, z=0.0, yaw=0.0, r=0.45, h=0.75, mat=WOODL, base=STEEL, solid=True):
    p = _P(B, x, y, z, yaw, "table_round")
    p.cyl(mat, 0, 0, h - 0.03, h, r, 10)
    p.cyl(base, 0, 0, 0.02, h - 0.03, 0.035, 6, top=False)
    p.cyl(base, 0, 0, 0.0, 0.025, 0.24, 6, r1=0.2)
    s = r * 0.62
    p.core(-s, s, -s, s, h - 0.025, h - 0.006)
    return p.done(solid=solid, top=h)


def lamp_green(B, x, y, z=0.0, yaw=0.0, light=False, solid=False):
    """[prop] The library's green-shaded brass lamp (z = desk top)."""
    p = _P(B, x, y, z, yaw, "lamp_green")
    p.kind = "prop"
    p.cyl(BRASS, 0, 0.04, 0.0, 0.025, 0.07, 6, r1=0.06)
    p.rod(BRASS, (0, 0.04, 0.02), (0, 0.04, 0.36), 0.015, 3)
    p.rod(BRASS, (0, 0.04, 0.36), (0, -0.02, 0.37), 0.015, 3)
    rings = [[(px, py - 0.02, pz) for px, py, pz in _ring_circle(0, 0, 0.0, 0.0, 6, 0.0)]]
    hw, d = 0.17, 0.09
    shade = [[(-hw, -d, 0.3), (hw, -d, 0.3), (hw, d, 0.3), (-hw, d, 0.3)],
             [(-hw * 0.95, -0.03, 0.4), (hw * 0.95, -0.03, 0.4), (hw * 0.95, 0.03, 0.4), (-hw * 0.95, 0.03, 0.4)]]
    _ = rings
    p.loft("CT_Baize", shade, top=True)
    p.face("CT_Opal", [(-hw + 0.01, d - 0.01, 0.301), (hw - 0.01, d - 0.01, 0.301), (hw - 0.01, -d + 0.01, 0.301),
                       (-hw + 0.01, -d + 0.01, 0.301)])
    if light:
        B.lamp(p.w(0, 0, 0.25), lumens=220, colour="#FFD9A0", kind="public", radius=3.0, fixture=False)
    return p.done(top=0.4)


def reading_table(B, x, y, z=0.0, yaw=0.0, w=2.4, d=1.0, h=0.75, lamps=2, light=False, mat=WOOD, solid=True):
    """A long reading table (seats on both long sides) with green lamps down the middle."""
    r = table(B, x, y, z, yaw, w, d, h, mat, solid=solid)
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    lp = []
    for k in range(lamps):
        u = -w / 2 + w * (k + 0.5) / lamps
        lx, ly = x + u * c, y + u * s
        lamp_green(B, lx, ly, z + h, yaw, light=light)
        lp.append((round(lx, 3), round(ly, 3), round(z + h + 0.3, 3)))
    seats = []
    n = max(1, int(w / 0.6))
    for k in range(n):
        u = -w / 2 + w * (k + 0.5) / n
        for side, dy in ((1, -d / 2 - 0.3), (-1, d / 2 + 0.3)):
            px, py = x + u * c - dy * s, y + u * s + dy * c
            seats.append((round(px, 3), round(py, 3), z, (yaw + (0 if side > 0 else 180) + 180) % 360))
    r["lamps"] = lp
    r["chairs"] = seats        # (x, y, z, yaw) for chair(): facing the table
    r["name"] = "reading_table"
    return r


def coffee_table(B, x, y, z=0.0, yaw=0.0, w=1.0, d=0.55, h=0.42, mat=WOOD, solid=True):
    p = _P(B, x, y, z, yaw, "coffee_table")
    p.bev(mat, -w / 2, w / 2, -d / 2, d / 2, h - 0.03, h, 0.01, 0.12, bottom=True)
    for sx in (-w / 2 + 0.12, w / 2 - 0.12):
        p.tube(STEEL, [(sx - 0.04, -d / 2 + 0.1, 0.0), (sx, -d / 2 + 0.12, h - 0.03), (sx, d / 2 - 0.12, h - 0.03),
                       (sx - 0.04, d / 2 - 0.1, 0.0)], 0.015, 3)
    p.core(-w / 2 + 0.05, w / 2 - 0.05, -d / 2 + 0.05, d / 2 - 0.05, h - 0.025, h - 0.006)
    return p.done(solid=solid, top=h)


def school_desk(B, x, y, z=0.0, yaw=0.0, mat=WOODL, frame=STEEL, solid=True, seed=0):
    """A two-seat desk; pupils sit at -y (in front of it... no: BEHIND it facing yaw). The pupils face yaw, the desk is
    in front of them: their chairs go 0.45 m behind (returned as 'chairs')."""
    p = _P(B, x, y, z, yaw, "school_desk")
    w, d = 1.1, 0.5
    # the desk: the pupil sits at +y and looks toward -y over a lid sloping down toward him
    p.hexa(mat, [(-w / 2, -d / 2, 0.74), (w / 2, -d / 2, 0.74), (w / 2, d / 2, 0.70), (-w / 2, d / 2, 0.70)],
           [(-w / 2, -d / 2, 0.765), (w / 2, -d / 2, 0.765), (w / 2, d / 2, 0.725), (-w / 2, d / 2, 0.725)])
    p.box(mat, -w / 2 + 0.03, w / 2 - 0.03, -d / 2 + 0.03, d / 2 - 0.06, 0.6, 0.62)
    p.box(mat, -w / 2 + 0.03, w / 2 - 0.03, -d / 2 + 0.02, -d / 2 + 0.035, 0.42, 0.738, f="yY")
    for sx in (-w / 2 + 0.05, w / 2 - 0.05):
        p.tube(frame, [(sx, -d / 2 + 0.03, 0.0), (sx, -d / 2 + 0.03, 0.72)], 0.03, 4)
        p.tube(frame, [(sx, d / 2 - 0.03, 0.0), (sx, d / 2 - 0.03, 0.7)], 0.03, 4)
        p.rod(frame, (sx, -d / 2 + 0.03, 0.08), (sx, d / 2 - 0.03, 0.08), 0.025, 3)
    rnd = _rng(seed, x, y, "desk")
    if rnd.random() < 0.7:
        p.box("CT_Paper", -0.35, -0.1, -0.05, 0.12, 0.74, 0.745)
    p.core(-w / 2 + 0.02, w / 2 - 0.02, -d / 2 + 0.03, d / 2 - 0.08, 0.602, 0.618)
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    chairs = []
    for u in (-0.27, 0.27):
        dv = d / 2 + 0.28
        chairs.append((round(x + u * c - dv * s, 3), round(y + u * s + dv * c, 3), z, yaw))
    return p.done(solid=solid, top=0.74, chairs=chairs)


def office_desk(B, x, y, z=0.0, yaw=0.0, w=1.4, d=0.7, mat=WOOD, typewriter_on=True, papers=True, phone=True,
                solid=True, seed=0):
    """A desk; the worker sits at +y facing yaw... the desk FACES yaw (visitors at -y), the worker sits behind it
    (returned 'chair': place a chair there facing yaw+180)."""
    p = _P(B, x, y, z, yaw, "office_desk")
    h = 0.75
    p.bev(mat, -w / 2, w / 2, -d / 2, d / 2, h - 0.035, h, 0.01, 0.0, bottom=True)
    p.box(mat, w / 2 - 0.45, w / 2 - 0.03, -d / 2 + 0.03, d / 2 - 0.03, 0.03, h - 0.035)
    for k in range(3):
        z0 = 0.06 + k * 0.21
        p.box(mat, w / 2 - 0.43, w / 2 - 0.05, d / 2 - 0.03, d / 2 - 0.02, z0, z0 + 0.19, f="Y")
        p.box(BRASS, w / 2 - 0.28, w / 2 - 0.2, d / 2 - 0.02, d / 2 - 0.005, z0 + 0.12, z0 + 0.135, f="xXYZz")
    p.box(mat, -w / 2 + 0.03, -w / 2 + 0.06, -d / 2 + 0.03, d / 2 - 0.03, 0.0, h - 0.035, f="xXyY")
    p.box(mat, -w / 2 + 0.06, w / 2 - 0.45, -d / 2 + 0.03, -d / 2 + 0.05, 0.3, h - 0.035, f="yY")
    p.core(-w / 2 + 0.02, w / 2 - 0.02, -d / 2 + 0.02, d / 2 - 0.02, h - 0.03, h - 0.008)
    r = p.done(solid=solid, top=h)
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    at = lambda u, v: (x + u * c - v * s, y + u * s + v * c)
    rnd = _rng(seed, x, y, "office")
    if typewriter_on:
        tx, ty = at(-0.1, 0.05)
        typewriter(B, tx, ty, z + h, yaw + 180)
    if papers:
        px, py = at(-w / 2 + 0.25, -0.05)
        newspaper(B, px, py, z + h, yaw + rnd.uniform(-20, 20), cellname="paper_typed")
    if phone:
        qx, qy = at(w / 2 - 0.25, 0.1)
        telephone(B, qx, qy, z + h, yaw + 160)
    cx_, cy_ = at(-0.1, d / 2 + 0.35)
    r["chair"] = (round(cx_, 3), round(cy_, 3), z, (yaw + 180) % 360)
    return r


def drafting_table(B, x, y, z=0.0, yaw=0.0, mat=WOODL, solid=True):
    p = _P(B, x, y, z, yaw, "drafting_table")
    for sx in (-0.5, 0.5):
        p.rod(STEEL, (sx, -0.35, 0.0), (sx, 0.0, 0.82), 0.035, 4)
        p.rod(STEEL, (sx, 0.35, 0.0), (sx, 0.0, 0.82), 0.035, 4)
    p.rod(STEEL, (-0.5, 0.0, 0.3), (0.5, 0.0, 0.3), 0.03, 4)
    a = math.radians(25)
    y0, z0 = -0.42, 0.82
    y1, z1 = y0 + 0.85 * math.cos(a), z0 + 0.85 * math.sin(a)
    p.slab(mat, (-0.6, y0, z0), (0.6, y0, z0), (0.6, y1, z1), (-0.6, y1, z1), 0.025)
    off = 0.004
    ny, nz = -math.sin(a) * off, math.cos(a) * off
    p.card("paper_drawing", (-0.42, y0 + 0.1 + ny, z0 + 0.1 * math.tan(a) + nz), (0.42, y0 + 0.1 + ny, z0 + 0.1 * math.tan(a) + nz),
           (0.42, y0 + 0.7 + ny, z0 + 0.7 * math.tan(a) + nz), (-0.42, y0 + 0.7 + ny, z0 + 0.7 * math.tan(a) + nz))
    p.box(WOOD, -0.5, 0.5, y0 - 0.03, y0 + 0.005, z0 - 0.01, z0 + 0.03)
    p.core(-0.5, 0.5, -0.02, 0.02, 0.29, 0.31)
    return p.done(solid=solid, top=None, front=0.5)


def chabudai(B, x, y, z=0.0, yaw=0.0, r=0.45, mat=WOOD, solid=False):
    p = _P(B, x, y, z, yaw, "chabudai")
    p.cyl(mat, 0, 0, 0.3, 0.33, r, 12)
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        p.box(mat, r * 0.55 * math.cos(a) - 0.025, r * 0.55 * math.cos(a) + 0.025, r * 0.55 * math.sin(a) - 0.025,
              r * 0.55 * math.sin(a) + 0.025, 0.0, 0.3, f="xXyY")
    return p.done(top=0.33)


def restaurant_table(B, x, y, z=0.0, yaw=0.0, w=0.9, d=0.9, places=4, cloth="CT_TileWhite", mat=WOOD, solid=True, seed=0):
    p = _P(B, x, y, z, yaw, "restaurant_table")
    h = 0.75
    hw, hd = w / 2, d / 2
    # cloth: the top, then a skirt flaring to the floor
    rings = [_ring_rect(-hw - 0.08, hw + 0.08, -hd - 0.08, hd + 0.08, 0.12, 0.02),
             _ring_rect(-hw - 0.03, hw + 0.03, -hd - 0.03, hd + 0.03, h - 0.02, 0.02),
             _ring_rect(-hw - 0.01, hw + 0.01, -hd - 0.01, hd + 0.01, h, 0.02)]
    p.loft(cloth, rings, top=True)
    _legs(p, mat, (-hw + 0.08, hw - 0.08), (-hd + 0.08, hd - 0.08), 0.12, 0.04)
    p.core(-hw + 0.02, hw - 0.02, -hd + 0.02, hd - 0.02, h - 0.05, h - 0.03)
    rnd = _rng(seed, x, y, "rest")
    sides = [(0, -hd + 0.15, 0), (0, hd - 0.15, 180), (-hw + 0.15, 0, 270), (hw - 0.15, 0, 90)][:places]
    for (u, v, a) in sides:
        p.cyl("CT_TileWhite", u, v, h, h + 0.012, 0.12, 10, r1=0.13)
        p.cyl("CT_Glass", u + 0.14 * math.cos(math.radians(a)), v + 0.14 * math.sin(math.radians(a)), h, h + 0.11, 0.028, 6, r1=0.035)
        k = 1 if a in (0, 180) else 0
        for sgn in (-1, 1):
            du, dv = (0.16 * sgn, 0) if k else (0, 0.16 * sgn)
            p.box(ALU, u + du - (0.01 if k else 0.08), u + du + (0.01 if k else 0.08), v + dv - (0.08 if k else 0.01),
                  v + dv + (0.08 if k else 0.01), h, h + 0.006, f="Z")
    p.lathe("CT_TileWhite", 0, 0, [(0.035, h), (0.05, h + 0.06), (0.02, h + 0.14), (0.03, h + 0.18)], 6, top=False)
    for k in range(3):
        a = k * 2.1 + rnd.uniform(0, 1)
        p.lathe(rnd.choice(["CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterPink"]), 0.025 * math.cos(a), 0.025 * math.sin(a),
                [(0.0, h + 0.2), (0.03, h + 0.23), (0.0, h + 0.26)], 5)
    seats = []
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    for (u, v, a) in sides:
        ou, ov = u * 1.0 + (0 if a in (0, 180) else (0.45 if u > 0 else -0.45)), v + (0 if a in (90, 270) else (0.45 if v > 0 else -0.45))
        seats.append((round(x + ou * c - ov * s, 3), round(y + ou * s + ov * c, 3), z, (yaw + a + 180) % 360))
    return p.done(solid=solid, top=h, chairs=seats)


def chess_table(B, x, y, z=0.0, yaw=0.0, mat=WOOD, solid=True, seed=0):
    p = _P(B, x, y, z, yaw, "chess_table")
    h = 0.72
    p.bev(mat, -0.35, 0.35, -0.35, 0.35, h - 0.04, h, 0.01, 0.02, bottom=True)
    p.card_top("chess_board", -0.25, 0.25, -0.25, 0.25, h + 0.002)
    p.cyl(mat, 0, 0, 0.02, h - 0.04, 0.05, 6, top=False)
    p.box(mat, -0.25, 0.25, -0.04, 0.04, 0.0, 0.04)
    p.box(mat, -0.04, 0.04, -0.25, 0.25, 0.0, 0.035)
    p.core(-0.33, 0.33, -0.33, 0.33, h - 0.035, h - 0.008)
    r = p.done(solid=solid, top=h)
    chess_set(B, x, y, z + h, yaw, seed=seed, board=0.5)
    return r


def pingpong_table(B, x, y, z=0.0, yaw=0.0, mat="CT_PlasterGreen", solid=True):
    p = _P(B, x, y, z, yaw, "pingpong_table")
    L, W, h = 2.74, 1.525, 0.76
    p.bev(mat, -L / 2, L / 2, -W / 2, W / 2, h - 0.03, h, 0.006, bottom=True)
    for (a0, a1, b0, b1) in ((-L / 2 + 0.005, L / 2 - 0.005, -W / 2 + 0.006, -W / 2 + 0.026),
                             (-L / 2 + 0.005, L / 2 - 0.005, W / 2 - 0.026, W / 2 - 0.006),
                             (-L / 2 + 0.006, -L / 2 + 0.026, -W / 2 + 0.03, W / 2 - 0.03),
                             (L / 2 - 0.026, L / 2 - 0.006, -W / 2 + 0.03, W / 2 - 0.03),
                             (-L / 2 + 0.03, L / 2 - 0.03, -0.0015, 0.0015)):
        p.box("CT_TileWhite", a0, a1, b0, b1, h, h + 0.002, f="Z")
    p.box(STEEL, -0.004, 0.004, -W / 2 - 0.1, W / 2 + 0.1, h, h + 0.1525, f="xXyYZ")
    for sx in (-L / 2 + 0.3, L / 2 - 0.3):
        for sy in (-W / 2 + 0.15, W / 2 - 0.15):
            p.cyl(STEEL, sx, sy, 0.0, h - 0.03, 0.025, 4, top=False)
    p.core(-L / 2 + 0.02, L / 2 - 0.02, -W / 2 + 0.02, W / 2 - 0.02, h - 0.025, h - 0.006)
    return p.done(solid=solid, top=h)


def billiard_table(B, x, y, z=0.0, yaw=0.0, mat=WOOD, solid=True, seed=0):
    p = _P(B, x, y, z, yaw, "billiard_table")
    L, W, h = 2.6, 1.45, 0.82
    p.bev(mat, -L / 2, L / 2, -W / 2, W / 2, 0.6, h - 0.04, 0.02, 0.06, bottom=True)
    p.box("CT_Baize", -L / 2 + 0.12, L / 2 - 0.12, -W / 2 + 0.12, W / 2 - 0.12, h - 0.06, h - 0.035, f="Z")
    for (a0, a1, b0, b1) in ((-L / 2 + 0.06, L / 2 - 0.06, -W / 2 + 0.03, -W / 2 + 0.12),
                             (-L / 2 + 0.06, L / 2 - 0.06, W / 2 - 0.12, W / 2 - 0.03),
                             (-L / 2 + 0.03, -L / 2 + 0.12, -W / 2 + 0.12, W / 2 - 0.12),
                             (L / 2 - 0.12, L / 2 - 0.03, -W / 2 + 0.12, W / 2 - 0.12)):
        p.bev(mat, a0, a1, b0, b1, h - 0.04, h, 0.012)
    for u in (-L / 2 + 0.13, 0.0, L / 2 - 0.13):
        for v in (-W / 2 + 0.13, W / 2 - 0.13):
            p.cyl(LAQ, u, v, h - 0.059, h - 0.0385, 0.055, 6)
    for sx in (-L / 2 + 0.25, 0.0, L / 2 - 0.25):
        for sy in (-W / 2 + 0.2, W / 2 - 0.2):
            p.cyl(mat, sx, sy, 0.0, 0.6, 0.07, 6, r1=0.09, top=False)
    rnd = _rng(seed, x, y, "billiard")
    for k in range(9):
        u, v = rnd.uniform(-0.9, 0.9), rnd.uniform(-0.45, 0.45)
        p.lathe(rnd.choice(["CT_TileWhite", "CT_PlasterRed", "CT_PlasterYellow", LAQ]), u, v,
                [(0.0, h - 0.035), (0.03, h - 0.007), (0.0, h + 0.022)], 5)
    p.core(-L / 2 + 0.05, L / 2 - 0.05, -W / 2 + 0.05, W / 2 - 0.05, 0.62, h - 0.08)
    return p.done(solid=solid, top=h)


# ================================================================================================ STORAGE
SPINE_TOPICS = {"library": 0, "kids": 1, "paperback": 2, "old": 3, "encyclopaedia": 4, "collected": 5, "modern": 6}


def _books_row(p, x0, x1, y0, y1, zb, zmax, rnd, spines=None, gap=0.35):
    """Blocks of books standing on a board from x0 to x1 (front y0, back y1), at most zmax tall."""
    x = x0 + rnd.uniform(0.0, 0.03)
    strip_m = 0.5                       # metres of books one spine strip shows across
    while x < x1 - 0.06:
        if rnd.random() < 0.08:
            x += rnd.uniform(0.05, 0.14)
            continue
        if rnd.random() < 0.06 and x1 - x > 0.3:           # a lying pile
            hh = 0.0
            for k in range(rnd.randint(2, 4)):
                t = rnd.uniform(0.03, 0.05)
                w = rnd.uniform(0.17, 0.24)
                p.box(rnd.choice(["CT_PlasterRed", "CT_PlasterBlue", "CT_Wood", "CT_PlasterGreen"]), x, x + w,
                      y0 + 0.02 + rnd.uniform(0, 0.02), y1 - 0.02, zb + hh, zb + hh + t, f="xXyZ")
                hh += t
            x += 0.27
            continue
        w = min(rnd.uniform(0.09, 0.26), x1 - x - 0.005)
        h = min(zmax - zb, rnd.uniform(0.18, 0.29))
        sp = spines or "spine_%02d" % rnd.randrange(40)
        if spines and not spines.startswith("spine_"):
            sp = cell(spines, rnd)
        frac = w / strip_m
        a0 = rnd.uniform(0.0, max(0.0, 1.0 - frac))
        yf = y0 + rnd.uniform(0.0, 0.025)
        p.card_front(sp, x, x + w, zb, zb + h, yf, rect=(a0, 0.0, min(1.0, a0 + frac), 1.0))
        p.box("CT_Paper", x + 0.004, x + w - 0.004, yf + 0.004, y1 - 0.01, zb, zb + h - 0.004, f="Z")
        p.box("CT_Wood", x, x + w, yf, y1 - 0.01, zb, zb + h, f="xX")
        x += w + rnd.uniform(0.0, 0.008)


def bookshelf(B, x, y, z=0.0, yaw=0.0, w=0.9, h=2.0, d=0.32, levels=5, mat=WOOD, topic=None, seed=0, fill=0.92,
              solid=True):
    """[wall] Open bookshelf full of books. topic: library|kids|paperback|old|encyclopaedia|collected|modern -> spine
    sets (each set is spine_NN with NN % 7 == the set), or an atlas group/cell for every block."""
    p = _P(B, x, y, z, yaw, "bookshelf")
    p.kind = "wall"
    t = 0.022
    p.box(mat, -w / 2, w / 2, -0.012, 0.0, 0.0, h, f="y")                       # back panel (front face only)
    p.bev(mat, -w / 2 - 0.01, w / 2 + 0.01, -d - 0.01, 0.0, h - 0.025, h, 0.008)    # top
    for s in (-1, 1):
        xa = s * (w / 2 - t / 2)
        p.box(mat, xa - t / 2, xa + t / 2, -d, -0.0, 0.0, h - 0.025, f="xXy")
    p.box(mat, -w / 2 + t, w / 2 - t, -d + 0.03, -d + 0.045, 0.0, 0.08, f="y")       # plinth
    pitch = (h - 0.1 - 0.025) / levels
    rnd = _rng(seed, x, y, "books")
    spine_set = SPINE_TOPICS.get(topic) if topic else None
    for k in range(levels):
        zb = 0.08 + k * pitch
        p.box(mat, -w / 2 + t, w / 2 - t, -d + 0.005, -0.012, zb - t, zb, f="yZz")
        if rnd.random() < fill:
            if spine_set is not None:
                sp = "spine_%02d" % ((rnd.randrange(6) * 7 + spine_set) % 40)
            elif topic:
                sp = topic
            else:
                sp = None
            _books_row(p, -w / 2 + t + 0.01, w / 2 - t - 0.01, -d + 0.02, -0.015, zb, zb + pitch - 0.035, rnd, sp)
    mid = 0.08 + (levels // 2) * pitch
    p.core(-w / 2 + t + 0.005, w / 2 - t - 0.005, -d + 0.01, -0.015, mid - t + 0.004, mid - 0.004)
    return p.done(solid=solid, top=h, front=0.7)


def magazine_rack(B, x, y, z=0.0, yaw=0.0, w=0.9, mat=WOODL, rows=4, solid=True, seed=0):
    p = _P(B, x, y, z, yaw, "magazine_rack")
    p.kind = "wall"
    h = 0.35 + rows * 0.27
    for s in (-1, 1):
        xa = s * (w / 2 - 0.012)
        p.hexa(mat, [(xa - 0.012, -0.35, 0), (xa + 0.012, -0.35, 0), (xa + 0.012, 0, 0), (xa - 0.012, 0, 0)],
               [(xa - 0.012, -0.12, h), (xa + 0.012, -0.12, h), (xa + 0.012, 0, h), (xa - 0.012, 0, h)])
    p.box(mat, -w / 2 + 0.024, w / 2 - 0.024, -0.012, 0.0, 0.0, h, f="y")
    rnd = _rng(seed, x, y, "mags")
    for k in range(rows):
        zb = 0.3 + k * 0.27
        yb = -0.35 + (zb / h) * 0.23 + 0.01
        p.box(mat, -w / 2 + 0.024, w / 2 - 0.024, yb - 0.02, yb + 0.06, zb - 0.02, zb, f="yZ")
        p.box(mat, -w / 2 + 0.024, w / 2 - 0.024, yb - 0.02, yb - 0.005, zb, zb + 0.05, f="yYZ")
        n = int((w - 0.08) / 0.21)
        for j in range(n):
            xa = -w / 2 + 0.05 + j * 0.21 + rnd.uniform(-0.01, 0.01)
            m = cell("mag", rnd)
            p.card(m, (xa, yb + 0.005, zb + 0.003), (xa + 0.18, yb + 0.005, zb + 0.003), (xa + 0.18, yb + 0.07, zb + 0.24),
                   (xa, yb + 0.07, zb + 0.24))
    p.core(-w / 2 + 0.03, w / 2 - 0.03, -0.2, -0.05, 0.4, h - 0.2)
    return p.done(solid=solid, top=h)


def card_catalogue(B, x, y, z=0.0, yaw=0.0, cols=4, rows=6, mat=WOODL, solid=True):
    p = _P(B, x, y, z, yaw, "card_catalogue")
    w = cols * 0.2 + 0.06
    h0, h1 = 0.45, 0.45 + rows * 0.13 + 0.04
    d = 0.45
    for sx in (-w / 2 + 0.05, w / 2 - 0.05):
        for sy in (-d / 2 + 0.05, d / 2 - 0.05):
            p.cyl(mat, sx, sy, 0.0, h0, 0.02, 4, r1=0.025, top=False)
    p.bev(mat, -w / 2, w / 2, -d / 2, d / 2, h0, h1, 0.012, bottom=True)
    for c in range(cols):
        for r in range(rows):
            a = -w / 2 + 0.03 + c * 0.2 + 0.008
            zb = h0 + 0.02 + r * 0.13 + 0.008
            p.box(mat, a, a + 0.184, -d / 2 - 0.006, -d / 2, zb, zb + 0.114, f="y")
            p.box(BRASS, a + 0.07, a + 0.114, -d / 2 - 0.014, -d / 2 - 0.006, zb + 0.07, zb + 0.09, f="yZz")
            p.box("CT_Paper", a + 0.06, a + 0.124, -d / 2 - 0.0075, -d / 2 - 0.006, zb + 0.03, zb + 0.06, f="y")
    p.core(-w / 2 + 0.03, w / 2 - 0.03, -d / 2 + 0.03, d / 2 - 0.03, h0 + 0.05, h1 - 0.05)
    return p.done(solid=solid, top=h1)


def wardrobe(B, x, y, z=0.0, yaw=0.0, w=1.0, h=2.0, d=0.58, mat=WOOD, solid=True):
    p = _P(B, x, y, z, yaw, "wardrobe")
    p.kind = "wall"
    for sx in (-w / 2 + 0.05, w / 2 - 0.05):
        for sy in (-d + 0.05, -0.05):
            p.cyl(mat, sx, sy, 0.0, 0.1, 0.02, 4, r1=0.025, top=False)
    p.box(mat, -w / 2, w / 2, -d, 0.0, 0.1, h - 0.06, f="xXyY")
    p.bev(mat, -w / 2 - 0.03, w / 2 + 0.03, -d - 0.03, 0.0, h - 0.06, h, 0.02)
    p.box(mat, -w / 2, w / 2, -d, 0.0, 0.098, 0.1, f="z")
    for s in (-1, 1):
        a, b = (-w / 2 + 0.02, -0.005) if s < 0 else (0.005, w / 2 - 0.02)
        p.box(mat, a, b, -d - 0.012, -d, 0.13, h - 0.09, f="xXyZz")
        p.box(mat, a + 0.06, b - 0.06, -d - 0.017, -d - 0.012, 0.25, h - 0.25, f="xXyZz")
        p.box(BRASS, s * 0.04 - 0.008, s * 0.04 + 0.008, -d - 0.04, -d - 0.012, 1.0, 1.15, f="xXyZz")
    p.core(-w / 2 + 0.02, w / 2 - 0.02, -d + 0.02, -0.02, 0.15, h - 0.1)
    return p.done(solid=solid, top=h)


def filing_cabinet(B, x, y, z=0.0, yaw=0.0, drawers=4, mat="CT_PlasterGreen", solid=True):
    p = _P(B, x, y, z, yaw, "filing_cabinet")
    p.kind = "wall"
    w, d = 0.47, 0.62
    h = 0.05 + drawers * 0.31 + 0.02
    p.bev(mat, -w / 2, w / 2, -d, 0.0, 0.0, h, 0.01)
    for k in range(drawers):
        zb = 0.05 + k * 0.31
        p.box(mat, -w / 2 + 0.02, w / 2 - 0.02, -d - 0.012, -d, zb + 0.01, zb + 0.3, f="xXyZz")
        p.box(ALU, -0.08, 0.08, -d - 0.035, -d - 0.012, zb + 0.2, zb + 0.225, f="xXyZz")
        p.box("CT_Paper", -0.05, 0.05, -d - 0.013, -d - 0.012, zb + 0.235, zb + 0.27, f="y")
    p.core(-w / 2 + 0.02, w / 2 - 0.02, -d + 0.02, -0.02, 0.1, h - 0.05)
    return p.done(solid=solid, top=h)


def lockers(B, x, y, z=0.0, yaw=0.0, n=4, mat="CT_PlasterBlue", solid=True):
    p = _P(B, x, y, z, yaw, "lockers")
    p.kind = "wall"
    lw, d, h = 0.38, 0.45, 1.85
    W = n * lw
    p.box(STEEL, -W / 2, W / 2, -d + 0.02, 0.0, 0.0, 0.1, f="xXy")
    p.bev(mat, -W / 2, W / 2, -d, 0.0, 0.1, h, 0.01)
    for k in range(n):
        a = -W / 2 + k * lw + 0.012
        b = a + lw - 0.024
        p.box(mat, a, b, -d - 0.01, -d, 0.13, h - 0.03, f="xXyZz")
        for j in range(4):
            zz = h - 0.25 + j * 0.035
            p.box(STEEL, a + 0.08, b - 0.08, -d - 0.011, -d - 0.01, zz, zz + 0.012, f="y")
        p.box(ALU, b - 0.07, b - 0.04, -d - 0.03, -d - 0.01, 0.95, 1.07, f="xXyZz")
        p.box("CT_Paper", a + 0.12, b - 0.12, -d - 0.011, -d - 0.01, 1.35, 1.41, f="y")
    p.core(-W / 2 + 0.02, W / 2 - 0.02, -d + 0.02, -0.02, 0.15, h - 0.05)
    return p.done(solid=solid, top=h)


def _goods(p, kind, x0, x1, y0, y1, zb, zmax, rnd):
    """Goods on one board (x0..x1 along it, y0 front, y1 back), never above zmax."""
    room = zmax - zb
    x = x0 + 0.01
    if kind in ("records",):
        while x < x1 - 0.06:
            n = rnd.randint(3, 8)
            for k in range(n):
                tl = 0.02 + k * 0.004
                xa = x + k * 0.012
                p.hexa("CT_Paper", [(xa, y0 + 0.02, zb), (xa + 0.01, y0 + 0.02, zb), (xa + 0.01, y1 - 0.02, zb),
                                                             (xa, y1 - 0.02, zb)],
                       [(xa + tl * 3, y0 + 0.02, zb + 0.31), (xa + tl * 3 + 0.01, y0 + 0.02, zb + 0.31),
                        (xa + tl * 3 + 0.01, y1 - 0.02, zb + 0.31), (xa + tl * 3, y1 - 0.02, zb + 0.31)], f="xXZ")
            xs = x + n * 0.012 + 0.06
            sc = cell("sleeve", rnd)
            p.card_front(sc, x, x + min(0.31, x1 - x), zb, zb + min(0.31, room - 0.01), y0 + 0.005)
            x = xs + 0.25
        return
    while x < x1 - 0.05:
        if kind == "pharmacy" and rnd.random() < 0.6:
            w = rnd.uniform(0.07, 0.14)
            h = min(room - 0.02, rnd.uniform(0.05, 0.12))
            if x + w > x1:
                break
            p.box("CT_TileWhite", x, x + w, y0 + 0.02, y0 + 0.12, zb, zb + h, f="xXZ")
            p.card_front(cell("label", rnd), x, x + w, zb, zb + h, y0 + 0.02)
            x += w + 0.01
        elif kind == "groceries" or kind == "jars" or kind == "pharmacy":
            r = rnd.uniform(0.035, 0.05) if kind != "pharmacy" else 0.035
            h = min(room - 0.02, rnd.uniform(0.09, 0.16))
            lab = cell("label", rnd)
            n = rnd.randint(2, 4)
            for k in range(n):
                cx = x + r + k * (2 * r + 0.006)
                if cx + r > x1:
                    break
                cy = y0 + r + 0.02
                for j in range(1):
                    if kind == "groceries":
                        _tin(p, cx, cy + j * (2 * r + 0.01), zb, h, r, lab)
                    else:
                        p.cyl("CT_TileWhite" if kind == "pharmacy" else "CT_Glass", cx, cy + j * (2 * r + 0.01), zb, zb + h, r, 5, top=False)
                        p.card_front(lab, cx - r * 0.75, cx + r * 0.75, zb + h * 0.25, zb + h * 0.7, cy + j * (2 * r + 0.01) - r - 0.002)
                        p.cyl(rnd.choice(["CT_PlasterRed", "CT_TileWhite", ALU]), cx, cy + j * (2 * r + 0.01), zb + h, zb + h + 0.02, r * 0.8, 5)
            x += n * (2 * r + 0.006) + 0.03
        elif kind in ("boxes", "hardware", "toys"):
            w = rnd.uniform(0.1, 0.24)
            h = min(room - 0.02, rnd.uniform(0.08, 0.22))
            dd = min(y1 - y0 - 0.03, rnd.uniform(0.12, 0.3))
            if x + w > x1:
                break
            if kind == "toys" and rnd.random() < 0.35:
                rr = min(h, w) * 0.45
                p.lathe(rnd.choice(PAINTS), x + w / 2, y0 + 0.1, [(0, zb), (rr, zb + rr), (0, zb + 2 * rr)], 6)
            else:
                mat = rnd.choice(PAINTS)
                p.box(mat, x, x + w, y0 + 0.02, y0 + 0.02 + dd, zb, zb + h, f="xXYZ")
                p.card_front(cell("label", rnd), x, x + w, zb, zb + h, y0 + 0.02)
            x += w + 0.015
        elif kind == "fabric":
            w = rnd.uniform(0.07, 0.1)
            h = min(room - 0.02, 0.55)
            p.box("CT_Paper", x + 0.004, x + w - 0.004, y0 + 0.05, y1 - 0.02, zb, zb + h - 0.004, f="xXYZ")
            p.card_front(cell("fabric", rnd), x, x + w, zb, zb + h, y0 + 0.03)
            x += w + 0.008
        elif kind == "shoes":
            mat = rnd.choice(["CT_Leather", LAQ, "CT_PlasterRed", "CT_Wood", "CT_TileWhite"])
            for k in range(2):
                xa = x + k * 0.1
                p.hexa(mat, [(xa, y0 + 0.03, zb), (xa + 0.085, y0 + 0.03, zb), (xa + 0.085, y0 + 0.28, zb), (xa, y0 + 0.28, zb)],
                       [(xa + 0.005, y0 + 0.05, zb + 0.05), (xa + 0.08, y0 + 0.05, zb + 0.05), (xa + 0.08, y0 + 0.2, zb + 0.07),
                        (xa + 0.005, y0 + 0.2, zb + 0.07)], f="xXyYZ")
                p.box(mat, xa + 0.005, xa + 0.08, y0 + 0.19, y0 + 0.28, zb, zb + 0.11)
            x += 0.26
        elif kind == "bottles":
            mat = rnd.choice(["CT_Glass", "CT_PlasterGreen", "CT_Copper", "CT_TileWhite"])
            lab = cell("label", rnd)
            for k in range(rnd.randint(2, 5)):
                cx = x + 0.04 + k * 0.08
                if cx > x1 - 0.04:
                    break
                hh = min(room - 0.02, 0.3)
                _bottle(p, cx, y0 + 0.08, zb, hh, mat, lab)
            x += 0.42
        else:
            x = x1


def _tin(p, cx, cy, zb, h, r, lab):
    """A tin: 6-sided can, the label wrapped round (one sixth of the cell per side)."""
    segs = 6
    pts = [(cx + r * math.cos(math.pi / segs + 2 * math.pi * k / segs), cy + r * math.sin(math.pi / segs + 2 * math.pi * k / segs))
           for k in range(segs)]
    for k in range(segs):
        (ax, ay), (bx, by) = pts[k], pts[(k + 1) % segs]
        a0 = ((k + 2) % segs) / segs
        p.card(lab, (ax, ay, zb), (bx, by, zb), (bx, by, zb + h), (ax, ay, zb + h), rect=(a0, 0.0, a0 + 1.0 / segs, 1.0))
    p.face(ALU, [(x, y, zb + h) for x, y in pts])


def _bottle(p, cx, cy, zb, h, mat, lab=None):
    p.lathe(mat, cx, cy, [(0.033, zb), (0.033, zb + h * 0.55), (0.011, zb + h * 0.78), (0.011, zb + h)], 5, top=True)
    if lab:
        p.card_front(lab, cx - 0.026, cx + 0.026, zb + h * 0.18, zb + h * 0.45, cy - 0.035)


def shop_shelf(B, x, y, z=0.0, yaw=0.0, w=1.2, h=2.0, d=0.45, levels=5, goods="groceries", mat=WOODL, seed=0, sign=None,
               solid=True):
    """[wall] Shop shelving with goods: groceries | boxes | jars | fabric | shoes | toys | records | hardware | bottles |
    pharmacy. sign: an atlas cell (sign_*) on a fascia on top."""
    p = _P(B, x, y, z, yaw, "shop_shelf")
    p.kind = "wall"
    p.box(mat, -w / 2, w / 2, -0.015, 0.0, 0.0, h, f="y")
    for s in (-1, 1):
        xa = s * (w / 2 - 0.012)
        p.box(mat, xa - 0.012, xa + 0.012, -d, 0.0, 0.0, h, f="xXyZ")
    p.box(mat, -w / 2 + 0.024, w / 2 - 0.024, -d, -0.015, 0.0, 0.12, f="yZ")
    pitch = (h - 0.12) / levels
    if goods == "fabric":
        levels = max(2, levels // 2)
        pitch = (h - 0.12) / levels
    rnd = _rng(seed, x, y, "goods" + goods)
    for k in range(levels):
        zb = 0.12 + k * pitch
        if k:
            p.box(mat, -w / 2 + 0.024, w / 2 - 0.024, -d + 0.005, -0.015, zb - 0.02, zb, f="yZz")
            p.box("CT_Paper", -w / 2 + 0.03, w / 2 - 0.03, -d + 0.004, -d + 0.005, zb - 0.02, zb - 0.002, f="y") \
                if goods in ("groceries", "boxes", "jars") else None
        if goods != "hardware" or k < levels - 1:
            _goods(p, goods, -w / 2 + 0.03, w / 2 - 0.03, -d + 0.01, -0.02, zb, zb + pitch - 0.03, rnd)
    p.bev(mat, -w / 2 - 0.01, w / 2 + 0.01, -d - 0.01, 0.0, h, h + 0.025, 0.006)
    if goods == "hardware":
        zb = 0.12 + (levels - 1) * pitch
        for k in range(int((w - 0.1) / 0.14)):
            xa = -w / 2 + 0.1 + k * 0.14
            p.rod(STEEL, (xa, -0.02, zb + 0.3), (xa, -0.18, zb + 0.3), 0.008, 3)
            p.box(rnd.choice([STEEL, "CT_SteelRed", WOOD, ALU]), xa - 0.02, xa + 0.02, -0.16, -0.14, zb + 0.05, zb + 0.29)
    if sign:
        p.box(mat, -w / 2, w / 2, -d - 0.01, -d + 0.01, h + 0.025, h + 0.25, f="xXyYZ")
        p.card_front(cell(sign), -w / 2 + 0.03, w / 2 - 0.03, h + 0.04, h + 0.235, -d - 0.012)
    mid = 0.12 + (levels // 2) * pitch
    p.core(-w / 2 + 0.03, w / 2 - 0.03, -d + 0.01, -0.02, mid - 0.016, mid - 0.004)
    return p.done(solid=solid, top=h, front=0.7)


def display_case(B, x, y, z=0.0, yaw=0.0, w=1.2, d=0.6, h=1.0, goods="toys", mat=WOOD, seed=0, solid=True):
    """A counter-height case; its glass is implied (open frame), goods visible on the inner shelf."""
    p = _P(B, x, y, z, yaw, "display_case")
    p.bev(mat, -w / 2, w / 2, -d / 2, d / 2, 0.0, 0.62, 0.01)
    fr = ALU
    for sx in (-w / 2 + 0.01, w / 2 - 0.01):
        for sy in (-d / 2 + 0.01, d / 2 - 0.01):
            p.box(fr, sx - 0.01, sx + 0.01, sy - 0.01, sy + 0.01, 0.62, h, f="xXyY")
    for sy in (-d / 2 + 0.01, d / 2 - 0.01):
        p.box(fr, -w / 2, w / 2, sy - 0.01, sy + 0.01, h - 0.02, h)
    for sx in (-w / 2 + 0.01, w / 2 - 0.01):
        p.box(fr, sx - 0.01, sx + 0.01, -d / 2 + 0.02, d / 2 - 0.02, h - 0.02, h)
    p.box("CT_Baize", -w / 2 + 0.03, w / 2 - 0.03, -d / 2 + 0.03, d / 2 - 0.03, 0.62, 0.625, f="Z")
    rnd = _rng(seed, x, y, "case")
    _goods(p, goods, -w / 2 + 0.05, w / 2 - 0.05, -d / 2 + 0.05, d / 2 - 0.05, 0.625, h - 0.05, rnd)
    p.core(-w / 2 + 0.02, w / 2 - 0.02, -d / 2 + 0.02, d / 2 - 0.02, 0.1, 0.55)
    return p.done(solid=solid, top=h)


def clothes_rail(B, x, y, z=0.0, yaw=0.0, w=1.5, n=None, seed=0, solid=False):
    p = _P(B, x, y, z, yaw, "clothes_rail")
    hw = w / 2
    for sx in (-hw, hw):
        p.rod(ALU, (sx, 0, 0.05), (sx, 0, 1.6), 0.03, 4)
        p.rod(ALU, (sx, -0.25, 0.04), (sx, 0.25, 0.04), 0.035, 4)
    p.rod(ALU, (-hw, 0, 1.6), (hw, 0, 1.6), 0.025, 4)
    rnd = _rng(seed, x, y, "rail")
    n = n or int((w - 0.15) / 0.075)
    for k in range(n):
        xa = -hw + 0.1 + k * (w - 0.2) / max(1, n - 1)
        mat = rnd.choice(FABRICS[:5] + ["CT_PlasterGrey", "CT_Paper"])
        L = rnd.uniform(0.6, 1.05)
        p.hexa(mat, [(xa - 0.012, -0.21, 1.52 - L), (xa + 0.012, -0.21, 1.52 - L), (xa + 0.012, 0.21, 1.52 - L), (xa - 0.012, 0.21, 1.52 - L)],
               [(xa - 0.015, -0.17, 1.52), (xa + 0.015, -0.17, 1.52), (xa + 0.015, 0.17, 1.52), (xa - 0.015, 0.17, 1.52)])
        p.rod(ALU, (xa, -0.17, 1.53), (xa, 0, 1.59), 0.008, 3)
        p.rod(ALU, (xa, 0.17, 1.53), (xa, 0, 1.59), 0.008, 3)
    return p.done()


def shoe_rack(B, x, y, z=0.0, yaw=0.0, w=1.0, rows=3, mat=WOODL, seed=0, solid=False):
    p = _P(B, x, y, z, yaw, "shoe_rack")
    p.kind = "wall"
    rnd = _rng(seed, x, y, "shoes")
    for s in (-1, 1):
        xa = s * (w / 2 - 0.012)
        p.box(mat, xa - 0.012, xa + 0.012, -0.4, 0.0, 0.0, 0.3 + rows * 0.27, f="xXyZ")
    for k in range(rows):
        zb = 0.08 + k * 0.27
        p.hexa(mat, [(-w / 2 + 0.024, -0.38, zb), (w / 2 - 0.024, -0.38, zb), (w / 2 - 0.024, -0.02, zb + 0.08), (-w / 2 + 0.024, -0.02, zb + 0.08)],
               [(-w / 2 + 0.024, -0.38, zb + 0.015), (w / 2 - 0.024, -0.38, zb + 0.015), (w / 2 - 0.024, -0.02, zb + 0.095),
                (-w / 2 + 0.024, -0.02, zb + 0.095)])
        p.box(mat, -w / 2 + 0.024, w / 2 - 0.024, -0.395, -0.38, zb, zb + 0.05)
        x0 = -w / 2 + 0.05
        while x0 < w / 2 - 0.25:
            mt = rnd.choice(["CT_Leather", LAQ, "CT_PlasterRed", "CT_TileWhite", WOOD])
            for j in range(2):
                xa = x0 + j * 0.1
                p.hexa(mt, [(xa, -0.36, zb + 0.016), (xa + 0.085, -0.36, zb + 0.016), (xa + 0.085, -0.08, zb + 0.08), (xa, -0.08, zb + 0.08)],
                       [(xa, -0.36, zb + 0.06), (xa + 0.085, -0.36, zb + 0.06), (xa + 0.085, -0.14, zb + 0.18), (xa, -0.14, zb + 0.18)], f="xXyYZ")
            x0 += 0.24
    return p.done(top=0.3 + rows * 0.27)


def coat_rack(B, x, y, z=0.0, yaw=0.0, w=2.4, coats=0.7, seed=0, solid=True):
    """Cloakroom rail (cloakroom attendant at +y... the public at -y): posts, rail, numbered hooks, coats."""
    p = _P(B, x, y, z, yaw, "coat_rack")
    hw = w / 2
    n_posts = max(2, int(w / 1.2) + 1)
    for k in range(n_posts):
        xa = -hw + k * w / (n_posts - 1)
        p.rod(STEEL, (xa, 0, 0.0), (xa, 0, 1.75), 0.04, 4)
        p.box(STEEL, xa - 0.15, xa + 0.15, -0.2, 0.2, 0.0, 0.02)
    p.rod(BRASS, (-hw, 0, 1.72), (hw, 0, 1.72), 0.03, 4)
    p.rod(BRASS, (-hw, 0, 1.35), (hw, 0, 1.35), 0.025, 4)
    rnd = _rng(seed, x, y, "coats")
    k = 0
    xa = -hw + 0.12
    while xa < hw - 0.1:
        p.rod(BRASS, (xa, 0, 1.72), (xa, -0.06, 1.65), 0.01, 3)
        if rnd.random() < coats:
            mat = rnd.choice(["CT_PlasterGrey", "CT_Leather", "CT_ConcreteDark", "CT_PlasterBlue", LAQ, "CT_Wood"])
            L = rnd.uniform(0.75, 1.1)
            p.hexa(mat, [(xa - 0.2, -0.25, 1.66 - L), (xa + 0.2, -0.25, 1.66 - L), (xa + 0.2, 0.05, 1.66 - L), (xa - 0.2, 0.05, 1.66 - L)],
                   [(xa - 0.17, -0.17, 1.62), (xa + 0.17, -0.17, 1.62), (xa + 0.17, 0.0, 1.62), (xa - 0.17, 0.0, 1.62)])
            p.lathe(mat, xa, -0.1, [(0.08, 1.6), (0.1, 1.64), (0.05, 1.7)], 5)
        k += 1
        xa += 0.24
    p.core(-hw, hw, -0.01, 0.01, 0.3, 1.7)
    return p.done(solid=solid, front=0.8)


def safe(B, x, y, z=0.0, yaw=0.0, mat="CT_PlasterGreen", solid=True):
    p = _P(B, x, y, z, yaw, "safe")
    p.kind = "wall"
    p.box(STEEL, -0.28, 0.28, -0.56, -0.04, 0.0, 0.08, f="xXyY")
    p.bev(mat, -0.3, 0.3, -0.6, 0.0, 0.08, 0.92, 0.02, 0.02)
    p.box(mat, -0.25, 0.25, -0.615, -0.6, 0.14, 0.86, f="xXyZz")
    p.slab(ALU, (-0.06, -0.635, 0.56), (0.06, -0.635, 0.56), (0.06, -0.635, 0.68), (-0.06, -0.635, 0.68), 0.02)
    p.slab(BRASS, (0.12, -0.63, 0.4), (0.2, -0.63, 0.4), (0.2, -0.63, 0.43), (0.12, -0.63, 0.43), 0.015)
    p.core(-0.28, 0.28, -0.58, -0.02, 0.1, 0.9)
    return p.done(solid=solid, top=0.92)


# ================================================================================================ COUNTERS
def _counter_body(p, w, d, h, mat, top, front=None, kick=0.08):
    p.box("CT_ConcreteDark", -w / 2 + 0.03, w / 2 - 0.03, -d / 2 + kick, d / 2, 0.0, kick, f="xXy")
    p.box(mat, -w / 2, w / 2, -d / 2, d / 2, kick, h - 0.04, f="xXyY")
    p.bev(top, -w / 2 - 0.02, w / 2 + 0.02, -d / 2 - 0.05, d / 2 + 0.01, h - 0.04, h, 0.008, 0.01)
    if front:
        p.box(front, -w / 2 + 0.06, w / 2 - 0.06, -d / 2 - 0.012, -d / 2, kick + 0.06, h - 0.12, f="xXyZz")
    p.core(-w / 2 + 0.02, w / 2 - 0.02, -d / 2 + 0.02, d / 2 - 0.02, kick + 0.02, h - 0.06)


def shop_counter(B, x, y, z=0.0, yaw=0.0, w=2.0, d=0.7, h=1.05, mat=WOOD, top="CT_TileWhite", front="CT_PlasterRed",
                 till=True, solid=True):
    """Customers at -y (the front), the shop assistant behind at +y ('staff'). 'on' = a free point on the top."""
    p = _P(B, x, y, z, yaw, "shop_counter")
    _counter_body(p, w, d, h, mat, top, front)
    r = p.done(solid=solid, top=h, staff=(0.0, d / 2 + 0.45, 0.0), customer=(0.0, -d / 2 - 0.45, 0.0),
               on=(-w / 4, 0.0, h))
    if till:
        c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        cash_register(B, x + w * 0.25 * c, y + w * 0.25 * s, z + h, yaw)
    return r


def bar_counter(B, x, y, z=0.0, yaw=0.0, w=3.0, d=0.65, h=1.05, mat=WOOD, top=BRASS, front="CT_Leather", taps=3,
                back=True, seed=0, solid=True):
    p = _P(B, x, y, z, yaw, "bar_counter")
    _counter_body(p, w, d, h, mat, WOOD, front)
    p.box(top, -w / 2 - 0.02, w / 2 + 0.02, -d / 2 - 0.06, -d / 2 + 0.02, h, h + 0.012, f="xXyYZ")
    p.rod(BRASS, (-w / 2 + 0.05, -d / 2 - 0.15, 0.22), (w / 2 - 0.05, -d / 2 - 0.15, 0.22), 0.04, 4)
    for xa in (-w / 2 + 0.1, 0.0, w / 2 - 0.1):
        p.rod(BRASS, (xa, -d / 2 - 0.15, 0.22), (xa, -d / 2, 0.22), 0.025, 3)
    for k in range(taps):
        xa = -0.3 + k * 0.3 / max(1, taps - 1) * 2 if taps > 1 else 0.0
        p.cyl(BRASS, xa, 0.05, h, h + 0.32, 0.025, 6)
        p.rod(BRASS, (xa, 0.05, h + 0.3), (xa, -0.05, h + 0.28), 0.02, 3)
        p.box(rnd_mat(k), xa - 0.02, xa + 0.02, 0.04, 0.06, h + 0.32, h + 0.44)
    if back:
        rnd = _rng(seed, x, y, "bar")
        yb = d / 2 + 1.0
        p.box(mat, -w / 2, w / 2, yb, yb + 0.45, 0.0, 0.9, f="xXyZ")
        for k in range(2):
            zb = 1.25 + k * 0.4
            p.box(WOOD, -w / 2, w / 2, yb + 0.25, yb + 0.45, zb - 0.025, zb)
            _goods(p, "bottles", -w / 2 + 0.05, w / 2 - 0.05, yb + 0.25, yb + 0.45, zb, zb + 0.36, rnd)
        p.core(-w / 2 + 0.02, w / 2 - 0.02, yb + 0.02, yb + 0.43, 0.05, 0.85)
    return p.done(solid=solid, top=h, staff=(0.0, d / 2 + 0.45, 0.0), customer=(0.0, -d / 2 - 0.5, 0.0), on=(0.6, -0.1, h))


def rnd_mat(k):
    return ["CT_PlasterRed", "CT_PlasterYellow", "CT_TileWhite", "CT_PlasterGreen"][k % 4]


def reception_desk(B, x, y, z=0.0, yaw=0.0, w=2.2, d=0.75, mat=WOODL, front="CT_PlasterBlue", solid=True):
    p = _P(B, x, y, z, yaw, "reception_desk")
    p.box(mat, -w / 2, w / 2, -d / 2, -d / 2 + 0.25, 0.0, 1.06, f="xXyY")
    p.bev(WOOD, -w / 2 - 0.02, w / 2 + 0.02, -d / 2 - 0.06, -d / 2 + 0.27, 1.06, 1.1, 0.01)
    p.box(front, -w / 2 + 0.08, w / 2 - 0.08, -d / 2 - 0.012, -d / 2, 0.15, 0.95, f="xXyZz")
    p.box(mat, -w / 2, w / 2, -d / 2 + 0.25, d / 2, 0.0, 0.72, f="xXY")
    p.bev(mat, -w / 2, w / 2, -d / 2 + 0.25, d / 2, 0.72, 0.75, 0.008)
    p.lathe(BRASS, -w / 4, -d / 2 + 0.08, [(0.045, 1.1), (0.045, 1.12), (0.035, 1.15), (0.0, 1.165)], 6)
    p.core(-w / 2 + 0.02, w / 2 - 0.02, -d / 2 + 0.02, d / 2 - 0.02, 0.2, 0.7)
    return p.done(solid=solid, top=1.1, staff=(0.0, d / 2 + 0.45, 0.0), customer=(0.0, -d / 2 - 0.45, 0.0),
                  on=(-w / 4, -d / 2 + 0.1, 1.1))


def ticket_booth(B, x, y, z=0.0, yaw=0.0, w=1.6, d=1.3, h=2.3, mat="CT_PlasterRed", sign="sign_till", solid=True):
    p = _P(B, x, y, z, yaw, "ticket_booth")
    # walls with a window in the front
    wx0, wx1, wz0, wz1 = -0.4, 0.4, 1.0, 1.6
    p.box(mat, -w / 2, w / 2, -d / 2, -d / 2 + 0.06, 0.0, wz0, f="xXyZ")
    p.box(mat, -w / 2, w / 2, -d / 2, -d / 2 + 0.06, wz1, h - 0.1, f="xXyz")
    p.box(mat, -w / 2, wx0, -d / 2, -d / 2 + 0.06, wz0, wz1, f="xXy")
    p.box(mat, wx1, w / 2, -d / 2, -d / 2 + 0.06, wz0, wz1, f="xXy")
    p.box(mat, -w / 2, -w / 2 + 0.06, -d / 2 + 0.06, d / 2, 0.0, h - 0.1, f="xX")
    p.box(mat, w / 2 - 0.06, w / 2, -d / 2 + 0.06, d / 2, 0.0, h - 0.1, f="xX")
    p.box(mat, -w / 2, w / 2, d / 2 - 0.06, d / 2, 0.0, h - 0.1, f="Yy")
    p.bev("CT_ConcreteWhite", -w / 2 - 0.08, w / 2 + 0.08, -d / 2 - 0.12, d / 2 + 0.08, h - 0.1, h, 0.02, bottom=True)
    p.box(WOOD, wx0, wx1, -d / 2 - 0.15, -d / 2 + 0.25, wz0 - 0.04, wz0, f="all")
    for xa in (wx0, wx1):
        p.box(ALU, xa - 0.02, xa + 0.02, -d / 2 - 0.02, -d / 2 + 0.06, wz0, wz1)
    p.box(ALU, wx0, wx1, -d / 2 - 0.02, -d / 2 + 0.06, wz1 - 0.03, wz1)
    p.card_front(cell(sign), -w / 2 + 0.1, w / 2 - 0.1, h - 0.45, h - 0.15, -d / 2 - 0.004)
    p.box(WOOD, -0.5, 0.5, -d / 2 + 0.06, -d / 2 + 0.5, 0.72, 0.75)
    p.core(-w / 2 + 0.01, w / 2 - 0.01, -d / 2 + 0.01, -d / 2 + 0.05, 0.1, 0.95)
    p.core(-w / 2 + 0.01, w / 2 - 0.01, d / 2 - 0.05, d / 2 - 0.01, 0.1, h - 0.2)
    p.core(-w / 2 + 0.01, -w / 2 + 0.05, -d / 2 + 0.06, d / 2 - 0.06, 0.1, h - 0.2)
    p.core(w / 2 - 0.05, w / 2 - 0.01, -d / 2 + 0.06, d / 2 - 0.06, 0.1, h - 0.2)
    return p.done(solid=solid, top=None, staff=(0.0, 0.0, 0.0), customer=(0.0, -d / 2 - 0.45, 0.0), window=(0.0, -d / 2, 1.3))


def lending_desk(B, x, y, z=0.0, yaw=0.0, w=2.4, d=0.8, mat=WOOD, front="CT_PlasterGreen", seed=0, solid=True):
    p = _P(B, x, y, z, yaw, "lending_desk")
    _counter_body(p, w, d, 1.0, mat, WOODL, front)
    p.box(mat, w / 2 - 0.8, w / 2, d / 2, d / 2 + 0.5, 0.0, 0.72, f="xXyYZ")
    rnd = _rng(seed, x, y, "lend")
    p.cyl(LAQ, -0.4, 0.0, 1.0, 1.06, 0.03, 6)
    p.box(WOOD, -0.45, -0.35, -0.04, 0.04, 1.06, 1.12, f="xXyYZ")
    zb = 1.0
    for k in range(rnd.randint(3, 6)):
        t = rnd.uniform(0.025, 0.045)
        p.box(rnd.choice(["CT_PlasterRed", "CT_PlasterBlue", WOOD, "CT_PlasterGreen", "CT_Leather"]), 0.3 + rnd.uniform(-0.02, 0.02),
              0.52, -0.12, 0.05, zb, zb + t, f="xXyYZ")
        zb += t
    return p.done(solid=solid, top=1.0, staff=(0.0, d / 2 + 0.45, 0.0), customer=(0.0, -d / 2 - 0.45, 0.0), on=(-0.8, 0.0, 1.0))


def pharmacy_counter(B, x, y, z=0.0, yaw=0.0, w=2.4, d=0.65, back=True, seed=0, solid=True):
    p = _P(B, x, y, z, yaw, "pharmacy_counter")
    _counter_body(p, w, d, 1.05, "CT_TileWhite", "CT_PlasterGreen", None)
    for k in range(int(w / 0.6)):
        xa = -w / 2 + 0.06 + k * 0.6
        p.box(ALU, xa, xa + 0.48, -d / 2 - 0.012, -d / 2, 0.45, 0.95, f="y")
        p.box("CT_Glass", xa + 0.02, xa + 0.46, -d / 2 - 0.013, -d / 2 - 0.012, 0.47, 0.93, f="y")
    r_ = None
    if back:
        rnd = _rng(seed, x, y, "pharm")
        yb = d / 2 + 0.95
        p.box("CT_TileWhite", -w / 2, w / 2, yb + 0.35, yb + 0.37, 0.0, 2.2, f="y")
        for k in range(5):
            zb = 0.9 + k * 0.28
            p.box("CT_TileWhite", -w / 2, w / 2, yb, yb + 0.35, zb - 0.02, zb, f="xXyZz")
            _goods(p, "pharmacy", -w / 2 + 0.03, w / 2 - 0.03, yb + 0.01, yb + 0.34, zb, zb + 0.25, rnd)
        p.box("CT_TileWhite", -w / 2, w / 2, yb, yb + 0.35, 0.0, 0.88, f="xXyZ")
        p.core(-w / 2 + 0.02, w / 2 - 0.02, yb + 0.02, yb + 0.33, 0.1, 0.85)
    _ = r_
    return p.done(solid=solid, top=1.05, staff=(0.0, d / 2 + 0.45, 0.0), customer=(0.0, -d / 2 - 0.45, 0.0), on=(0.5, 0.0, 1.05))


def market_stall(B, x, y, z=0.0, yaw=0.0, w=1.8, d=0.9, produce=None, canopy=True, seed=0, solid=True):
    p = _P(B, x, y, z, yaw, "market_stall")
    h = 0.8
    p.bev(WOODL, -w / 2, w / 2, -d / 2, d / 2, h - 0.04, h, 0.01, bottom=True)
    for sx in (-w / 2 + 0.1, w / 2 - 0.1):
        p.rod(WOOD, (sx, -d / 2 + 0.08, 0.0), (sx, 0.0, h - 0.04), 0.04, 4)
        p.rod(WOOD, (sx, d / 2 - 0.08, 0.0), (sx, 0.0, h - 0.04), 0.04, 4)
    rnd = _rng(seed, x, y, "stall")
    produce = produce or ["CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterGreen", "CT_Leaf", "CT_PlasterPink"]
    n = int(w / 0.45)
    for k in range(n):
        xa = -w / 2 + 0.05 + k * (w - 0.1) / n
        cw = (w - 0.1) / n - 0.03
        lift = 0.06 if k % 2 else 0.0
        _crate(p, xa + cw / 2, -0.12, h + lift, cw, 0.4, 0.14, WOODL, rnd.choice(produce), rnd)
        if lift:
            p.box(WOOD, xa + 0.02, xa + cw - 0.02, -0.25, 0.0, h, h + lift, f="xXyYZ")
    p.box("CT_Paper", w / 2 - 0.3, w / 2 - 0.05, 0.2, 0.4, h, h + 0.005, f="Z")
    if canopy:
        for sx in (-w / 2 + 0.02, w / 2 - 0.02):
            p.rod(STEEL, (sx, d / 2 - 0.03, h), (sx, d / 2 - 0.03, 2.3), 0.035, 4)
            p.rod(STEEL, (sx, -d / 2 - 0.2, 2.05), (sx, d / 2 - 0.03, 2.3), 0.025, 3)
        nst = int(w / 0.3)
        for k in range(nst):
            a = -w / 2 + k * w / nst
            b = a + w / nst
            mat = "CT_PlasterRed" if k % 2 else "CT_TileWhite"
            p.slab(mat, (a, -d / 2 - 0.25, 2.05), (b, -d / 2 - 0.25, 2.05), (b, d / 2, 2.33), (a, d / 2, 2.33), 0.01)
            p.box(mat, a, b, -d / 2 - 0.26, -d / 2 - 0.25, 1.88, 2.05, f="yY")
    p.core(-w / 2 + 0.02, w / 2 - 0.02, -d / 2 + 0.02, d / 2 - 0.02, h - 0.035, h - 0.008)
    return p.done(solid=solid, top=h, staff=(0.0, d / 2 + 0.45, 0.0), customer=(0.0, -d / 2 - 0.45, 0.0))


def _crate(p, cx, cy, zb, w, dd, h, mat, fill, rnd):
    t = 0.015
    p.box(mat, cx - w / 2, cx + w / 2, cy - dd / 2, cy + dd / 2, zb, zb + 0.015)
    p.box(mat, cx - w / 2, cx - w / 2 + t, cy - dd / 2, cy + dd / 2, zb, zb + h, f="xXyYZ")
    p.box(mat, cx + w / 2 - t, cx + w / 2, cy - dd / 2, cy + dd / 2, zb, zb + h, f="xXyYZ")
    for sy in (cy - dd / 2, cy + dd / 2 - t):
        p.box(mat, cx - w / 2 + t, cx + w / 2 - t, sy, sy + t, zb + 0.02, zb + h * 0.45, f="yYZ")
        p.box(mat, cx - w / 2 + t, cx + w / 2 - t, sy, sy + t, zb + h * 0.55, zb + h, f="yYZ")
    if fill:
        n = 3
        for i in range(n):
            for j in range(2):
                fx = cx - w / 2 + t + (i + 0.5) * (w - 2 * t) / n + rnd.uniform(-0.01, 0.01)
                fy = cy - dd / 4 + j * dd / 2
                r = min((w - 2 * t) / n, dd / 2) * 0.5
                p.lathe(fill, fx, fy, [(r * 0.9, zb + h - r * 0.5), (r, zb + h), (0.0, zb + h + r * 0.8)], 5)
        p.box(fill, cx - w / 2 + t + 0.004, cx + w / 2 - t - 0.004, cy - dd / 2 + t + 0.004, cy + dd / 2 - t - 0.004, zb + 0.015,
              zb + h - 0.01, f="Z")


# ================================================================================================ HOME
def bed(B, x, y, z=0.0, yaw=0.0, size="single", mat=WOOD, blanket="CT_PlasterBlue", solid=True):
    """The head of the bed is at +y (against a wall when yaw = 0... put y = wall - length/2)."""
    p = _P(B, x, y, z, yaw, "bed")
    W = 0.95 if size == "single" else 1.65
    L = 2.05 if size == "single" else 2.1
    hw, hl = W / 2, L / 2
    _legs(p, mat, (-hw + 0.04, hw - 0.04), (-hl + 0.04, hl - 0.04), 0.15, 0.05)
    p.box(mat, -hw, hw, -hl, hl, 0.15, 0.3, f="xXyYZ")
    p.bev(mat, -hw - 0.02, hw + 0.02, hl - 0.04, hl + 0.01, 0.0, 0.95, 0.015)
    p.bev(mat, -hw - 0.02, hw + 0.02, -hl - 0.01, -hl + 0.03, 0.0, 0.55, 0.015)
    p.cushion("CT_Paper", -hw + 0.02, hw - 0.02, -hl + 0.04, hl - 0.05, 0.3, 0.46, 0.01)
    p.cushion(blanket, -hw - 0.03, hw + 0.03, -hl + 0.02, hl * 0.35, 0.3, 0.5, 0.012)
    np_ = 1 if size == "single" else 2
    for k in range(np_):
        a = -hw + 0.08 + k * (W - 0.16) / np_
        p.cushion("CT_TileWhite", a, a + (W - 0.16) / np_ - 0.04, hl - 0.48, hl - 0.08, 0.46, 0.58, 0.02)
    p.core(-hw + 0.02, hw - 0.02, -hl + 0.04, hl - 0.06, 0.17, 0.29)
    return p.done(solid=solid, top=0.5, seats=[(0.0, -0.2, 0.48, 0.0)])


def bunk_bed(B, x, y, z=0.0, yaw=0.0, mat=STEEL, blanket="CT_PlasterGreen", solid=True):
    p = _P(B, x, y, z, yaw, "bunk_bed")
    hw, hl = 0.47, 1.02
    for sx in (-hw, hw):
        for sy in (-hl, hl):
            p.rod(mat, (sx, sy, 0.0), (sx, sy, 1.75), 0.04, 4)
    for zb in (0.32, 1.32):
        for sx in (-hw, hw):
            p.rod(mat, (sx, -hl, zb), (sx, hl, zb), 0.035, 4)
        for sy in (-hl, hl):
            p.rod(mat, (-hw, sy, zb), (hw, sy, zb), 0.035, 4)
        p.box(WOODL, -hw + 0.02, hw - 0.02, -hl + 0.02, hl - 0.02, zb - 0.02, zb, f="Zz")
        p.cushion("CT_Paper", -hw + 0.03, hw - 0.03, -hl + 0.03, hl - 0.03, zb, zb + 0.14, 0.01)
        p.cushion(blanket, -hw + 0.01, hw - 0.01, -hl + 0.02, hl * 0.3, zb + 0.02, zb + 0.17, 0.01)
        p.cushion("CT_TileWhite", -hw + 0.1, hw - 0.1, hl - 0.4, hl - 0.06, zb + 0.14, zb + 0.24, 0.02)
    for sy in (-hl, hl):
        p.rod(mat, (-hw, sy, 0.75), (hw, sy, 0.75), 0.03, 4)
        p.rod(mat, (-hw, sy, 1.6), (hw, sy, 1.6), 0.03, 4)
    p.rod(mat, (-hw, -0.3, 1.55), (-hw, 0.6, 1.55), 0.03, 4)
    for k in range(4):
        zz = 0.55 + k * 0.25
        p.rod(mat, (hw + 0.005, -hl + 0.3, zz), (hw + 0.005, -hl + 0.6, zz), 0.025, 3)
    p.core(-hw + 0.03, hw - 0.03, -hl + 0.03, hl - 0.03, 0.29, 0.31)
    return p.done(solid=solid, top=0.46, seats=[(0.0, -0.2, 0.46, 0.0)], upper=(0.0, 0.0, 1.46))


def futon(B, x, y, z=0.0, yaw=0.0, mat="CT_Paper", quilt="CT_PlasterBlue", solid=False):
    p = _P(B, x, y, z, yaw, "futon")
    p.cushion(mat, -0.5, 0.5, -1.05, 1.05, 0.0, 0.08, 0.015)
    p.cushion(quilt, -0.52, 0.52, -1.0, 0.45, 0.08, 0.16, 0.015)
    p.cushion("CT_TileWhite", -0.25, 0.25, 0.6, 0.95, 0.08, 0.18, 0.03)
    return p.done(top=0.12)


def bedside_table(B, x, y, z=0.0, yaw=0.0, mat=WOOD, lamp=True, light=False, solid=False):
    p = _P(B, x, y, z, yaw, "bedside_table")
    _legs(p, mat, (-0.19, 0.19), (-0.16, 0.16), 0.2, 0.03)
    p.bev(mat, -0.22, 0.22, -0.2, 0.2, 0.2, 0.55, 0.01)
    p.box(mat, -0.19, 0.19, -0.212, -0.2, 0.36, 0.52, f="y")
    p.box(BRASS, -0.04, 0.04, -0.225, -0.212, 0.43, 0.45, f="xXyZz")
    r = p.done(top=0.55)
    if lamp:
        table_lamp(B, x, y, z + 0.55, yaw, light=light)
    return r


KITCHEN = ("cupboard", "stove", "sink", "fridge")


def kitchen_unit(B, x, y, z=0.0, yaw=0.0, modules="cupboard,stove,sink,cupboard,fridge", mat="CT_TileWhite",
                 top="CT_PlasterGrey", wall_cupboards=True, solid=True):
    """[wall] A kitchen run along x (0.6 m per module, centred), its back on the wall."""
    p = _P(B, x, y, z, yaw, "kitchen_unit")
    p.kind = "wall"
    mods = modules.split(",") if isinstance(modules, str) else list(modules)
    W = 0.6 * len(mods)
    x0 = -W / 2
    for k, m in enumerate(mods):
        a = x0 + k * 0.6
        b = a + 0.6
        if m == "fridge":
            p.bev(mat, a + 0.01, b - 0.01, -0.62, 0.0, 0.0, 1.7, 0.03, 0.04)
            p.box(ALU, b - 0.08, b - 0.06, -0.66, -0.62, 1.05, 1.4, f="xXyZz")
            p.box("CT_PlasterGrey", a + 0.03, b - 0.03, -0.625, -0.62, 1.15, 1.16, f="y")
            continue
        p.box("CT_ConcreteDark", a, b, -0.55, -0.0, 0.0, 0.1, f="y")
        p.box(mat, a, b, -0.58, 0.0, 0.1, 0.86, f="xXy" if k in (0, len(mods) - 1) else "y")
        p.box(top, a, b, -0.62, 0.0, 0.86, 0.9, f="xXyZ")
        if m == "stove":
            p.box(LAQ, a + 0.04, b - 0.04, -0.585, -0.58, 0.18, 0.62, f="y")
            p.box(ALU, a + 0.08, b - 0.08, -0.6, -0.585, 0.66, 0.68, f="xXyZz")
            for i in range(2):
                for j in range(2):
                    p.cyl(LAQ, a + 0.17 + i * 0.26, -0.45 + j * 0.25, 0.9, 0.91, 0.08, 6)
            for i in range(4):
                p.box(LAQ, a + 0.11 + i * 0.12, a + 0.14 + i * 0.12, -0.6, -0.58, 0.77, 0.8, f="xXyZz")
        elif m == "sink":
            p.box(ALU, a + 0.08, b - 0.08, -0.5, -0.12, 0.88, 0.902, f="Z")
            p.rod(ALU, (a + 0.3, -0.06, 0.9), (a + 0.3, -0.06, 1.12), 0.025, 4)
            p.rod(ALU, (a + 0.3, -0.06, 1.12), (a + 0.3, -0.25, 1.1), 0.02, 3)
            p.box(mat, a + 0.03, b - 0.03, -0.585, -0.58, 0.14, 0.82, f="y")
        else:
            p.box(mat, a + 0.03, b - 0.03, -0.585, -0.58, 0.14, 0.82, f="y")
            p.box(ALU, a + 0.25, b - 0.25, -0.6, -0.585, 0.74, 0.76, f="xXyZz")
        if wall_cupboards:
            p.bev(mat, a, b, -0.33, 0.0, 1.45, 2.15, 0.01)
            p.box(mat, a + 0.03, b - 0.03, -0.342, -0.33, 1.48, 2.12, f="y")
            p.box(ALU, a + 0.25, b - 0.25, -0.357, -0.342, 1.5, 1.52, f="xXyZz")
    p.core(x0 + 0.02, -x0 - 0.02, -0.56, -0.02, 0.15, 0.84)
    return p.done(solid=solid, top=0.9)


def samovar(B, x, y, z=0.0, yaw=0.0, solid=False):
    p = _P(B, x, y, z, yaw, "samovar")
    p.kind = "prop"
    p.lathe(BRASS, 0, 0, [(0.09, 0.0), (0.06, 0.05), (0.06, 0.08), (0.13, 0.12), (0.15, 0.25), (0.12, 0.36), (0.06, 0.38),
                          (0.05, 0.42)], 8, top=True)
    p.lathe("CT_TileWhite", 0, 0, [(0.06, 0.42), (0.09, 0.47), (0.06, 0.52), (0.02, 0.53)], 6)
    p.rod(BRASS, (0.0, -0.14, 0.18), (0.0, -0.22, 0.15), 0.025, 3)
    for s in (-1, 1):
        p.rod(LAQ, (s * 0.14, 0, 0.3), (s * 0.2, 0, 0.28), 0.02, 3)
    return p.done(top=0.53)


def tv_set(B, x, y, z=0.0, yaw=0.0, screen="screen_tv_news", mat=WOOD, solid=False):
    p = _P(B, x, y, z, yaw, "tv_set")
    for sx in (-0.28, 0.28):
        for sy in (-0.15, 0.15):
            p.rod(LAQ, (sx * 1.1, sy * 1.2, 0.0), (sx, sy, 0.45), 0.025, 3)
    p.bev(mat, -0.35, 0.35, -0.22, 0.25, 0.45, 1.0, 0.03, 0.03)
    p.box(LAQ, -0.25, 0.22, -0.23, -0.22, 0.52, 0.93, f="y")
    p.card_front(screen, -0.23, 0.2, 0.54, 0.91, -0.232)
    for k in range(3):
        p.box(ALU, 0.255, 0.31, -0.24, -0.22, 0.85 - k * 0.09, 0.88 - k * 0.09, f="xXyZz")
    p.box("CT_Copper", 0.25, 0.32, -0.225, -0.22, 0.53, 0.6, f="y")
    p.rod(ALU, (-0.05, 0.1, 1.0), (-0.2, 0.15, 1.35), 0.008, 3)
    p.rod(ALU, (0.05, 0.1, 1.0), (0.2, 0.15, 1.35), 0.008, 3)
    return p.done(top=1.0, screen=(0.0, -0.23, 0.72))


def radio(B, x, y, z=0.0, yaw=0.0, mat=WOOD, solid=False):
    p = _P(B, x, y, z, yaw, "radio")
    p.kind = "prop"
    p.bev(mat, -0.25, 0.25, -0.11, 0.11, 0.0, 0.3, 0.03, 0.02)
    p.card_front("grille", -0.21, 0.02, 0.1, 0.27, -0.113)
    p.card_front("dial_radio", 0.04, 0.22, 0.17, 0.26, -0.113)
    for k in range(2):
        p.box(LAQ, 0.07 + k * 0.1, 0.11 + k * 0.1, -0.125, -0.11, 0.05, 0.09, f="xXyZz")
    return p.done(top=0.3)


def floor_lamp(B, x, y, z=0.0, yaw=0.0, shade="CT_Paper", light=False, solid=False):
    p = _P(B, x, y, z, yaw, "floor_lamp")
    p.cyl(LAQ, 0, 0, 0.0, 0.03, 0.17, 8, r1=0.15)
    p.rod(BRASS, (0, 0, 0.03), (0, 0, 1.3), 0.025, 4)
    p.lathe(shade, 0, 0, [(0.24, 1.25), (0.15, 1.6)], 8, top=False)
    p.lathe("CT_Opal", 0, 0, [(0.14, 1.59), (0.0, 1.595)], 8, top=False)
    if light:
        B.lamp(p.w(0, 0, 1.4), lumens=350, colour="#FFD9A8", kind="public", radius=4.0, fixture=False)
    return p.done()


def table_lamp(B, x, y, z=0.0, yaw=0.0, shade="CT_PlasterYellow", light=False, solid=False):
    p = _P(B, x, y, z, yaw, "table_lamp")
    p.kind = "prop"
    p.lathe("CT_TileWhite", 0, 0, [(0.07, 0.0), (0.08, 0.06), (0.04, 0.2), (0.015, 0.24)], 6, top=False)
    p.rod(BRASS, (0, 0, 0.23), (0, 0, 0.3), 0.012, 3)
    p.lathe(shade, 0, 0, [(0.13, 0.26), (0.08, 0.42)], 8, top=False)
    p.lathe("CT_Opal", 0, 0, [(0.075, 0.415), (0.0, 0.42)], 8, top=False)
    if light:
        B.lamp(p.w(0, 0, 0.33), lumens=180, colour="#FFD9A8", kind="public", radius=3.0, fixture=False)
    return p.done(top=0.42)


def wall_clock(B, x, y, z=0.0, yaw=0.0, zc=2.2, r=0.17, face=None, solid=False):
    p = _P(B, x, y, z, yaw, "wall_clock")
    p.kind = "wall"
    rg0 = [(px, -0.0, pz) for px, pz, _ in [(r * math.cos(a), zc + r * math.sin(a), 0) for a in
                                            [2 * math.pi * k / 12 for k in range(12)]]]
    rg1 = [(px, -0.05, pz) for px, _, pz in rg0]
    for a, b, c_, d_ in zip(rg0, rg0[1:] + rg0[:1], rg1[1:] + rg1[:1], rg1):
        p.face(LAQ, [a, b, c_, d_])
    s = r * 0.9
    p.card_front(face or "clock_0", -s, s, zc - s, zc + s, -0.05)
    return p.done()


def mirror(B, x, y, z=0.0, yaw=0.0, zc=1.45, w=0.6, h=1.0, frame=WOOD, solid=False):
    p = _P(B, x, y, z, yaw, "mirror")
    p.kind = "wall"
    p.box(frame, -w / 2, w / 2, -0.03, 0.0, zc - h / 2, zc + h / 2, f="xXyZz")
    p.box(ALU, -w / 2 + 0.05, w / 2 - 0.05, -0.036, -0.03, zc - h / 2 + 0.05, zc + h / 2 - 0.05, f="y")
    return p.done()


def rug(B, x, y, z=0.0, yaw=0.0, w=2.0, d=3.0, name="rug", round=False, seed=0, solid=False):
    """A rug: an atlas rug cell (rug_* / rug_round_*), 6 mm above the floor. Long side of the cell along y."""
    p = _P(B, x, y, z, yaw, "rug")
    p.kind = "flat"
    rnd = _rng(seed, x, y, "rug")
    zz = 0.006
    if round:
        cn = cell("rug_round", rnd) if name == "rug" else name
        n = 16
        r = w / 2
        u0, v0, u1, v1 = _uvrect(cn)
        cu, cv, ru, rv = (u0 + u1) / 2, (v0 + v1) / 2, (u1 - u0) / 2, (v1 - v0) / 2
        pts = [(r * math.cos(2 * math.pi * k / n), r * math.sin(2 * math.pi * k / n), zz) for k in range(n)]
        p.m.face(ATLAS_SLOT, pts, [(cu + ru * math.cos(2 * math.pi * k / n), cv + rv * math.sin(2 * math.pi * k / n)) for k in range(n)])
        ring_ = [(px, py, 0.0) for px, py, _ in pts]
        for a, b, c_, d_ in zip(ring_, ring_[1:] + ring_[:1], pts[1:] + pts[:1], pts):
            p.face("CT_PlasterRed", [a, b, c_, d_])
    else:
        cn = cell("rug", rnd) if name == "rug" else name
        if cn.startswith("rug_round"):
            cn = "rug_0"
        p.card_top(cn, -w / 2, w / 2, -d / 2, d / 2, zz, turn=0 if d >= w else 1)
        p.box("CT_ConcreteDark", -w / 2, w / 2, -d / 2, d / 2, 0.0, zz - 0.001, f="xXyY")
    return p.done()


def curtains(B, x, y, z=0.0, yaw=0.0, w=1.6, h=2.4, mat="CT_PlasterRed", open_=0.55, solid=False):
    """[wall] Two drapes on a rod, gathered to the sides (open_ = how much of the width is open)."""
    p = _P(B, x, y, z, yaw, "curtains")
    p.kind = "wall"
    p.rod(BRASS, (-w / 2 - 0.15, -0.1, h), (w / 2 + 0.15, -0.1, h), 0.025, 4)
    dw = w * (1 - open_) / 2 + 0.12
    for s in (-1, 1):
        x_out, x_in = s * (w / 2 + 0.12), s * (w / 2 + 0.12 - dw)
        folds = 5
        pts = []
        for k in range(folds * 2 + 1):
            t = k / (folds * 2)
            pts.append((x_out + (x_in - x_out) * t, -0.1 + (0.04 if k % 2 else -0.03)))
        if s > 0:
            pts = pts[::-1]
        for (ax, ay), (bx, by) in zip(pts, pts[1:]):
            p.face(mat, [(ax, ay, 0.02), (bx, by, 0.02), (bx, by, h - 0.05), (ax, ay, h - 0.05)])
            p.face(mat, [(bx, by + 0.002, 0.02), (ax, ay + 0.002, 0.02), (ax, ay + 0.002, h - 0.05), (bx, by + 0.002, h - 0.05)])
    return p.done()


def radiator(B, x, y, z=0.0, yaw=0.0, w=1.0, h=0.6, mat="CT_SteelWhite", solid=False):
    p = _P(B, x, y, z, yaw, "radiator")
    p.kind = "wall"
    n = int(w / 0.08)
    for k in range(n):
        a = -w / 2 + k * w / n
        p.bev(mat, a + 0.005, a + w / n - 0.005, -0.12, -0.04, 0.12, 0.12 + h, 0.012)
    p.box(mat, -w / 2, w / 2, -0.09, -0.07, 0.17, 0.17 + h - 0.1, f="yZz")
    for sx in (-w / 2 + 0.1, w / 2 - 0.1):
        p.rod(mat, (sx, -0.08, 0.12), (sx, -0.08, 0.0), 0.03, 4)
    return p.done(top=0.12 + h)


def plant(B, x, y, z=0.0, yaw=0.0, kind="ficus", pot="CT_PlasterRed", h=1.3, seed=0, solid=False):
    """kind ficus | monstera | palm | cactus | fern. h = overall height."""
    p = _P(B, x, y, z, yaw, "plant")
    rnd = _rng(seed, x, y, "plant" + kind)
    small = kind in ("fern", "cactus")
    pr = 0.12 if small else 0.2
    ph = 0.2 if small else 0.34
    p.lathe(pot, 0, 0, [(pr * 0.75, 0.0), (pr, ph), (pr * 1.06, ph + 0.02)], 8, top=False)
    p.cyl("CT_ConcreteDark", 0, 0, ph - 0.02, ph - 0.01, pr * 0.98, 8)
    if kind == "ficus":
        p.rod(WOOD, (0, 0, ph), (0.03, 0.02, h * 0.8), 0.035, 3)
        for k in range(7):
            a = k * 2.4 + rnd.uniform(-0.3, 0.3)
            zc = ph + (h - ph) * (0.45 + 0.08 * (k % 4))
            rr = (h - ph) * 0.2
            cx, cy = 0.18 * math.cos(a) * (h / 1.3), 0.18 * math.sin(a) * (h / 1.3)
            p.lathe("CT_Leaf", cx, cy, [(0.0, zc - rr * 0.8), (rr, zc - rr * 0.2), (rr * 0.85, zc + rr * 0.4), (0.0, zc + rr)], 6)
    elif kind == "monstera":
        for k in range(7):
            a = 2 * math.pi * k / 7 + rnd.uniform(-0.2, 0.2)
            L = rnd.uniform(0.45, 0.75) * h / 1.3
            tip = (math.cos(a) * L * 0.75, math.sin(a) * L * 0.75, ph + L * rnd.uniform(0.7, 1.0))
            p.rod("CT_Leaf", (0, 0, ph), tip, 0.015, 3)
            # a big split leaf: two triangles folded along the midrib
            ca, sa = math.cos(a), math.sin(a)
            ln, wd = 0.34 * h / 1.3, 0.2 * h / 1.3
            base = tip
            end = (tip[0] + ca * ln, tip[1] + sa * ln, tip[2] - 0.12)
            side = (-sa * wd, ca * wd)
            mid = ((base[0] + end[0]) / 2, (base[1] + end[1]) / 2, (base[2] + end[2]) / 2 + 0.03)
            for sgn in (-1, 1):
                q = (mid[0] + side[0] * sgn, mid[1] + side[1] * sgn, mid[2] - 0.05)
                tri = [base, end, q] if sgn > 0 else [base, q, end]
                p.face("CT_Leaf", tri)
                p.face("CT_Leaf", [tri[0], tri[2], tri[1]])
    elif kind == "palm":
        p.rod(WOOD, (0, 0, ph), (0.0, 0.0, h * 0.62), 0.05, 4)
        for k in range(8):
            a = 2 * math.pi * k / 8 + rnd.uniform(-0.2, 0.2)
            top = (0, 0, h * 0.62)
            L = 0.55 * h / 1.3
            mid = (math.cos(a) * L * 0.5, math.sin(a) * L * 0.5, h * 0.62 + 0.25 * h / 1.3)
            end = (math.cos(a) * L, math.sin(a) * L, h * 0.62 - 0.05)
            side = (-math.sin(a) * 0.08, math.cos(a) * 0.08)
            for a_, b_ in ((top, mid), (mid, end)):
                q0 = (a_[0] - side[0], a_[1] - side[1], a_[2])
                q1 = (b_[0] - side[0] * 0.6, b_[1] - side[1] * 0.6, b_[2])
                q2 = (b_[0] + side[0] * 0.6, b_[1] + side[1] * 0.6, b_[2])
                q3 = (a_[0] + side[0], a_[1] + side[1], a_[2])
                p.face("CT_Leaf", [q0, q1, q2, q3])
                p.face("CT_Leaf", [q3, q2, q1, q0])
    elif kind == "cactus":
        p.lathe("CT_Leaf", 0, 0, [(0.06, ph - 0.01), (0.07, ph + 0.2), (0.05, ph + 0.32), (0.0, ph + 0.36)], 7)
        p.lathe("CT_Leaf", 0.08, 0, [(0.0, ph + 0.1), (0.035, ph + 0.13), (0.03, ph + 0.22), (0.0, ph + 0.25)], 5)
    else:   # fern
        for k in range(9):
            a = 2 * math.pi * k / 9
            L = 0.35
            base = (0, 0, ph)
            end = (math.cos(a) * L, math.sin(a) * L, ph + 0.05)
            mid = (math.cos(a) * L * 0.5, math.sin(a) * L * 0.5, ph + 0.25)
            side = (-math.sin(a) * 0.05, math.cos(a) * 0.05)
            for a_, b_ in ((base, mid), (mid, end)):
                q = [(a_[0] - side[0], a_[1] - side[1], a_[2]), (b_[0] - side[0], b_[1] - side[1], b_[2]),
                     (b_[0] + side[0], b_[1] + side[1], b_[2]), (a_[0] + side[0], a_[1] + side[1], a_[2])]
                p.face("CT_Leaf", q)
                p.face("CT_Leaf", q[::-1])
    return p.done()


def aquarium(B, x, y, z=0.0, yaw=0.0, w=1.0, mat=WOOD, solid=True, seed=0):
    p = _P(B, x, y, z, yaw, "aquarium")
    p.bev(mat, -w / 2, w / 2, -0.2, 0.2, 0.0, 0.75, 0.012)
    p.box(mat, -w / 2 + 0.04, w / 2 - 0.04, -0.212, -0.2, 0.08, 0.7, f="y")
    p.card_front("aquarium_back", -w / 2 + 0.02, w / 2 - 0.02, 0.76, 1.18, 0.15)
    p.box("CT_Gravel", -w / 2 + 0.02, w / 2 - 0.02, -0.18, 0.15, 0.75, 0.8, f="Z")
    for sx in (-w / 2 + 0.01, w / 2 - 0.01):
        for sy in (-0.19, 0.19):
            p.box(LAQ, sx - 0.01, sx + 0.01, sy - 0.01, sy + 0.01, 0.75, 1.2, f="xXyY")
    p.box(LAQ, -w / 2, w / 2, -0.2, 0.2, 1.2, 1.25)
    rnd = _rng(seed, x, y, "fish")
    for k in range(5):
        fx, fy, fz = rnd.uniform(-w / 2 + 0.1, w / 2 - 0.1), rnd.uniform(-0.12, 0.1), rnd.uniform(0.85, 1.12)
        p.hexa(rnd.choice(["CT_PlasterYellow", "CT_PlasterRed", "CT_SteelRed"]), [(fx - 0.04, fy - 0.004, fz - 0.015), (fx + 0.04, fy - 0.004, fz - 0.015),
                                                                                   (fx + 0.04, fy + 0.004, fz - 0.015), (fx - 0.04, fy + 0.004, fz - 0.015)],
               [(fx - 0.04, fy - 0.004, fz + 0.015), (fx + 0.04, fy - 0.004, fz + 0.015), (fx + 0.04, fy + 0.004, fz + 0.015),
                (fx - 0.04, fy + 0.004, fz + 0.015)])
    p.core(-w / 2 + 0.02, w / 2 - 0.02, -0.18, 0.18, 0.1, 0.7)
    return p.done(solid=solid, top=1.25)


def hat_stand(B, x, y, z=0.0, yaw=0.0, mat=WOOD, solid=False):
    p = _P(B, x, y, z, yaw, "hat_stand")
    p.rod(mat, (0, 0, 0.0), (0, 0, 1.8), 0.04, 4)
    for k in range(3):
        a = 2 * math.pi * k / 3
        p.rod(mat, (0, 0, 0.35), (0.25 * math.cos(a), 0.25 * math.sin(a), 0.0), 0.03, 3)
    for k in range(4):
        a = 2 * math.pi * k / 4 + 0.4
        p.tube(mat, [(0, 0, 1.62), (0.12 * math.cos(a), 0.12 * math.sin(a), 1.68), (0.17 * math.cos(a), 0.17 * math.sin(a), 1.78)], 0.02, 3)
    return p.done()


# ================================================================================================ CULTURE, LEISURE
def piano(B, x, y, z=0.0, yaw=0.0, mat=LAQ, solid=True):
    """Upright piano, keyboard toward -y. 'seat' = where a piano stool goes."""
    p = _P(B, x, y, z, yaw, "piano")
    w = 1.48
    p.bev(mat, -w / 2, w / 2, -0.32, 0.0, 0.0, 1.25, 0.015, 0.0)
    p.box(mat, -w / 2, w / 2, -0.62, -0.32, 0.65, 0.72, f="xXyZz")
    p.box(mat, -w / 2, -w / 2 + 0.05, -0.62, -0.32, 0.72, 0.82, f="xXyYZ")
    p.box(mat, w / 2 - 0.05, w / 2, -0.62, -0.32, 0.72, 0.82, f="xXyYZ")
    p.card_top("keys_piano", -w / 2 + 0.06, w / 2 - 0.06, -0.62, -0.47, 0.722)
    p.box(mat, -w / 2 + 0.05, w / 2 - 0.05, -0.47, -0.32, 0.72, 0.76, f="yZ")
    for sx in (-w / 2 + 0.06, w / 2 - 0.06):
        p.cyl(mat, sx, -0.58, 0.0, 0.65, 0.025, 6, r1=0.03, top=False)
    p.hexa(mat, [(-0.25, -0.34, 0.85), (0.25, -0.34, 0.85), (0.25, -0.32, 0.85), (-0.25, -0.32, 0.85)],
           [(-0.25, -0.31, 1.1), (0.25, -0.31, 1.1), (0.25, -0.29, 1.1), (-0.25, -0.29, 1.1)])
    p.card("paper_typed", (-0.2, -0.345, 0.87), (0.0, -0.345, 0.87), (0.0, -0.32, 1.08), (-0.2, -0.32, 1.08))
    p.card("paper_typed", (0.0, -0.345, 0.87), (0.2, -0.345, 0.87), (0.2, -0.32, 1.08), (0.0, -0.32, 1.08))
    for sx in (-0.1, 0.1):
        p.box(BRASS, sx - 0.015, sx + 0.015, -0.4, -0.3, 0.05, 0.065, f="xXyYZ")
    p.core(-w / 2 + 0.02, w / 2 - 0.02, -0.3, -0.02, 0.1, 1.2)
    return p.done(solid=solid, top=1.25, seat=(0.0, -0.95, 0.0), front=0.9)


def grand_piano(B, x, y, z=0.0, yaw=0.0, mat=LAQ, solid=True):
    """Grand piano: keyboard at -y (the pianist sits there facing +y... the piano FACES -y), lid open."""
    p = _P(B, x, y, z, yaw, "grand_piano")
    # outline of the case (top view), keyboard edge along y = -0.85
    outline = [(-0.74, -0.85), (0.74, -0.85), (0.74, -0.2), (0.66, 0.35), (0.42, 0.75), (0.15, 1.0), (-0.2, 1.05),
               (-0.55, 0.9), (-0.74, 0.5)]
    r0 = [(u, v, 0.62) for u, v in outline]
    r1 = [(u, v, 0.98) for u, v in outline]
    p.loft(mat, [r0, r1], top=True, bottom=True)
    p.box(mat, -0.74, 0.74, -1.08, -0.85, 0.66, 0.72, f="xXyZz")
    p.card_top("keys_piano", -0.68, 0.68, -1.06, -0.86, 0.722)
    p.box(mat, -0.74, -0.68, -1.08, -0.85, 0.72, 0.8, f="xXyZ")
    p.box(mat, 0.68, 0.74, -1.08, -0.85, 0.72, 0.8, f="xXyZ")
    # lid: hinged on the -x side, propped open on a stick
    lid = [(u, v) for u, v in outline]
    ang = math.radians(32)
    hinge = -0.74

    def L(u, v, t=0.0):
        du = u - hinge
        return (hinge + du * math.cos(ang) - t * math.sin(ang), v, 0.985 + du * math.sin(ang) + t * math.cos(ang))
    top_ = [L(u, v, 0.02) for u, v in lid]
    bot_ = [L(u, v, 0.0) for u, v in lid]
    p.face(mat, top_)
    p.face(mat, bot_[::-1])
    for a, b, c_, d_ in zip(bot_, bot_[1:] + bot_[:1], top_[1:] + top_[:1], top_):
        p.face(mat, [a, b, c_, d_])
    p.rod(mat, (0.45, 0.3, 0.98), L(0.45, 0.3, 0.0), 0.015, 3)
    for (u, v) in ((-0.62, -0.7), (0.62, -0.7), (-0.1, 0.85)):
        p.cyl(mat, u, v, 0.0, 0.62, 0.05, 6, r1=0.065, top=False)
    p.box(BRASS, -0.15, 0.15, 0.0, 0.06, 0.05, 0.08, f="xXyYZ")
    p.rod(mat, (0.0, 0.03, 0.08), (0.0, 0.03, 0.62), 0.03, 4)
    p.core(-0.7, 0.7, -0.8, 0.3, 0.66, 0.95)
    return p.done(solid=solid, top=0.98, seat=(0.0, -1.4, 0.0), front=0.9)


def stage(B, x, y, z=0.0, yaw=0.0, w=6.0, d=3.0, h=0.5, mat=WOOD, curtain="CT_PlasterRed", curtain_h=3.2, solid=True):
    """A low stage; its front edge faces yaw (audience at -y). Steps at the left front corner."""
    p = _P(B, x, y, z, yaw, "stage")
    p.box("CT_ConcreteDark", -w / 2, w / 2, -d / 2, d / 2, 0.0, h - 0.04, f="xXyY")
    p.bev(mat, -w / 2 - 0.03, w / 2 + 0.03, -d / 2 - 0.03, d / 2, h - 0.04, h, 0.01)
    n = int(round((h - 0.04) / 0.16))
    for k in range(n):
        zz = (k + 1) * h / (n + 1)
        p.box(mat, -w / 2 + 0.2, -w / 2 + 1.1, -d / 2 - 0.3 * (n - k), -d / 2 - 0.3 * (n - k - 1) - 0.03, 0.0, zz)
    if curtain:
        for s in (-1, 1):
            x_out = s * w / 2
            x_in = s * (w / 2 - 0.9)
            pts = []
            for k in range(9):
                t = k / 8
                pts.append((x_out + (x_in - x_out) * t, -d / 2 + 0.25 + (0.06 if k % 2 else -0.04)))
            if s > 0:
                pts = pts[::-1]
            for (ax, ay), (bx, by) in zip(pts, pts[1:]):
                p.face(curtain, [(ax, ay, h), (bx, by, h), (bx, by, h + curtain_h), (ax, ay, h + curtain_h)])
                p.face(curtain, [(bx, by + 0.002, h), (ax, ay + 0.002, h), (ax, ay + 0.002, h + curtain_h), (bx, by + 0.002, h + curtain_h)])
        p.box(curtain, -w / 2, w / 2, -d / 2 + 0.15, -d / 2 + 0.3, h + curtain_h - 0.5, h + curtain_h, f="all")
    for k in range(n):
        zz = (k + 1) * h / (n + 1)
        p.core(-w / 2 + 0.22, -w / 2 + 1.08, -d / 2 - 0.3 * (n - k) + 0.02, -d / 2 - 0.3 * (n - k - 1) - 0.05, 0.0, zz - 0.01)
    p.core(-w / 2 + 0.02, w / 2 - 0.02, -d / 2 + 0.02, d / 2 - 0.02, 0.0, h - 0.045)
    return p.done(solid=solid, top=h, front=1.0)


def cinema_screen(B, x, y, z=0.0, yaw=0.0, w=6.0, h=3.0, zb=1.2, picture="screen_film_0", solid=False):
    p = _P(B, x, y, z, yaw, "cinema_screen")
    p.kind = "wall"
    p.box(LAQ, -w / 2 - 0.35, w / 2 + 0.35, -0.05, 0.0, zb - 0.3, zb + h + 0.3, f="xXyZz")
    p.box("CT_Paper", -w / 2, w / 2, -0.06, -0.05, zb, zb + h, f="y")
    p.card_front(picture, -w / 2 + 0.05, w / 2 - 0.05, zb + 0.05, zb + h - 0.05, -0.062)
    return p.done(screen=(0.0, -0.06, zb + h / 2))


def projector(B, x, y, z=0.0, yaw=0.0, solid=False):
    """[prop] A film projector; it projects toward -y."""
    p = _P(B, x, y, z, yaw, "projector")
    p.kind = "prop"
    p.bev("CT_PlasterGreen", -0.12, 0.12, -0.2, 0.2, 0.0, 0.28, 0.02, 0.02)
    p.rod(LAQ, (0.0, -0.2, 0.16), (0.0, -0.32, 0.16), 0.07, 4)
    for v, zz in ((-0.12, 0.45), (0.14, 0.43)):
        p.rod(ALU, (0.0, v, 0.28), (0.0, v, zz - 0.12), 0.02, 3)
        p.card_side("film_reel_side", v - 0.14, v + 0.14, zz - 0.14, zz + 0.14, 0.03, 1)
        p.card_side("film_reel_side", v - 0.14, v + 0.14, zz - 0.14, zz + 0.14, -0.03, -1)
    return p.done(top=0.28)


def easel(B, x, y, z=0.0, yaw=0.0, canvas=None, mat=WOODL, seed=0, solid=False):
    p = _P(B, x, y, z, yaw, "easel")
    for sx in (-0.28, 0.28):
        p.rod(mat, (sx, -0.1, 0.0), (sx * 0.2, 0.02, 1.75), 0.03, 4)
    p.rod(mat, (0.0, 0.55, 0.0), (0.0, 0.05, 1.5), 0.03, 4)
    p.box(mat, -0.32, 0.32, -0.25, -0.03, 0.8, 0.83)          # ledge, reaching back to the front legs
    a = math.radians(8)
    w, hh = 0.5, 0.65
    z0 = 0.83
    y0 = -0.2                                                  # canvas stands on the ledge in front of the legs
    yt, zt = y0 + hh * math.sin(a), z0 + hh * math.cos(a)
    p.slab(mat, (-w / 2, y0 + 0.02, z0), (w / 2, y0 + 0.02, z0), (w / 2, yt + 0.02, zt), (-w / 2, yt + 0.02, zt), 0.02)
    rnd = _rng(seed, x, y, "canvas")
    cn = canvas or rnd.choice(["canvas_blank", cell("painting", rnd)])
    p.card(cn, (-w / 2 + 0.01, y0 + 0.016, z0 + 0.01), (w / 2 - 0.01, y0 + 0.016, z0 + 0.01),
           (w / 2 - 0.01, yt + 0.016 - 0.0, zt - 0.01), (-w / 2 + 0.01, yt + 0.016, zt - 0.01))
    for k in range(3):
        p.cyl(rnd.choice(PAINTS), -0.2 + k * 0.06, -0.228, 0.83, 0.86, 0.018, 5)
    return p.done(seat=(0.0, -0.9, 0.0))


def potters_wheel(B, x, y, z=0.0, yaw=0.0, solid=False):
    p = _P(B, x, y, z, yaw, "potters_wheel")
    p.bev("CT_SteelWhite", -0.3, 0.3, -0.25, 0.3, 0.0, 0.42, 0.02, 0.04)
    p.lathe("CT_SteelWhite", 0, 0, [(0.26, 0.42), (0.28, 0.5), (0.24, 0.52)], 10, top=False)
    p.cyl(ALU, 0, 0, 0.45, 0.5, 0.15, 10)
    p.lathe("CT_PlasterPink", 0, 0, [(0.07, 0.5), (0.09, 0.56), (0.06, 0.64), (0.07, 0.68)], 8, top=False)
    p.box(STEEL, -0.1, 0.1, 0.3, 0.42, 0.0, 0.03)
    return p.done(seat=(0.0, 0.55, 0.0), top=0.5)


def sewing_table(B, x, y, z=0.0, yaw=0.0, mat=WOOD, solid=True):
    """Treadle sewing table; the seamstress sits at -y facing +y... the table FACES -y (her side)."""
    p = _P(B, x, y, z, yaw, "sewing_table")
    p.bev(mat, -0.5, 0.5, -0.25, 0.25, 0.71, 0.75, 0.01, bottom=True)
    for sx in (-0.45, 0.45):
        p.hexa(STEEL, [(sx - 0.015, -0.22, 0.0), (sx + 0.015, -0.22, 0.0), (sx + 0.015, 0.22, 0.0), (sx - 0.015, 0.22, 0.0)],
               [(sx - 0.015, -0.12, 0.71), (sx + 0.015, -0.12, 0.71), (sx + 0.015, 0.12, 0.71), (sx - 0.015, 0.12, 0.71)])
    p.slab(STEEL, (-0.3, -0.2, 0.1), (0.3, -0.2, 0.1), (0.3, 0.05, 0.12), (-0.3, 0.05, 0.12), 0.02)
    p.rod(STEEL, (0.3, 0.0, 0.4), (0.45, 0.0, 0.4), 0.02, 3)
    # the machine: base, column, arm, head (black with a gold band)
    p.bev(LAQ, -0.2, 0.2, -0.08, 0.08, 0.75, 0.8, 0.008)
    p.box(LAQ, 0.12, 0.19, -0.05, 0.05, 0.8, 1.02, f="xXyYZ")
    p.box(LAQ, -0.18, 0.19, -0.045, 0.045, 0.95, 1.04, f="xXyYZz")
    p.box(LAQ, -0.2, -0.13, -0.05, 0.05, 0.84, 1.04, f="xXyYz")
    p.box(BRASS, -0.1, 0.1, -0.046, -0.045, 0.98, 1.0, f="y")
    p.card_side("film_reel_side", -0.06, 0.06, 0.92, 1.04, 0.192, 1)
    p.core(-0.48, 0.48, -0.23, 0.23, 0.715, 0.745)
    return p.done(solid=solid, top=0.75, seat=(0.0, -0.6, 0.0))


def loom(B, x, y, z=0.0, yaw=0.0, mat=WOODL, cloth="fabric_2", solid=True):
    p = _P(B, x, y, z, yaw, "loom")
    for sx in (-0.55, 0.55):
        p.box(mat, sx - 0.03, sx + 0.03, -0.45, -0.39, 0.0, 0.9, f="xXyY")
        p.box(mat, sx - 0.03, sx + 0.03, 0.39, 0.45, 0.0, 1.5, f="xXyYZ")
        p.box(mat, sx - 0.03, sx + 0.03, -0.45, 0.45, 0.85, 0.9)
    p.rod(mat, (-0.55, -0.42, 0.88), (0.55, -0.42, 0.88), 0.05, 4)
    p.rod(mat, (-0.55, 0.42, 1.45), (0.55, 0.42, 1.45), 0.05, 4)
    p.card(cell(cloth), (-0.48, -0.4, 0.9), (0.48, -0.4, 0.9), (0.48, 0.0, 0.92), (-0.48, 0.0, 0.92))
    for k in range(24):
        xa = -0.46 + k * 0.04
        p.rod("CT_Paper", (xa, 0.0, 0.92), (xa, 0.42, 1.43), 0.004, 3)
    p.box(mat, -0.52, 0.52, 0.1, 0.14, 0.95, 1.25, f="xXyYZz")
    p.core(-0.55, 0.55, -0.44, 0.44, 0.86, 0.89)
    return p.done(solid=solid, seat=(0.0, -0.75, 0.0), top=0.9)


def globe(B, x, y, z=0.0, yaw=0.0, floor=False, solid=False):
    """[prop] A globe on a stand (floor=True: a tall floor stand, z = floor)."""
    p = _P(B, x, y, z, yaw, "globe")
    p.kind = "prop"
    zb = 0.0
    if floor:
        for k in range(3):
            a = 2 * math.pi * k / 3
            p.rod(WOOD, (0.25 * math.cos(a), 0.25 * math.sin(a), 0.0), (0.0, 0.0, 0.6), 0.03, 3)
        zb = 0.55
    p.cyl(WOOD, 0, 0, zb, zb + 0.03, 0.09, 6)
    p.rod(BRASS, (0, 0, zb + 0.03), (0, 0, zb + 0.12), 0.015, 3)
    r = 0.15
    zc = zb + 0.12 + r + 0.01
    n = 10
    u0, v0, u1, v1 = _uvrect("map_world")
    rings = []
    for j in range(7):
        phi = -math.pi / 2 + math.pi * j / 6
        rings.append([(r * math.cos(phi) * math.cos(2 * math.pi * k / n), r * math.cos(phi) * math.sin(2 * math.pi * k / n),
                       zc + r * math.sin(phi)) for k in range(n + 1)])
    for j in range(6):
        for k in range(n):
            a, b, c_, d_ = rings[j][k], rings[j][k + 1], rings[j + 1][k + 1], rings[j + 1][k]
            uva = [(u0 + (u1 - u0) * (k / n), v0 + (v1 - v0) * (j / 6)), (u0 + (u1 - u0) * ((k + 1) / n), v0 + (v1 - v0) * (j / 6)),
                   (u0 + (u1 - u0) * ((k + 1) / n), v0 + (v1 - v0) * ((j + 1) / 6)), (u0 + (u1 - u0) * (k / n), v0 + (v1 - v0) * ((j + 1) / 6))]
            if j == 0:
                p.m.face(ATLAS_SLOT, [a, c_, d_], [uva[0], uva[2], uva[3]])
            elif j == 5:
                p.m.face(ATLAS_SLOT, [a, b, c_], uva[:3])
            else:
                p.m.face(ATLAS_SLOT, [a, b, c_, d_], uva)
    p.tube(BRASS, [(r * 1.08 * math.cos(t), 0.0, zc + r * 1.08 * math.sin(t)) for t in
                   [-math.pi / 2 + math.pi * k / 6 for k in range(7)]], 0.012, 3)
    return p.done(top=zc + r)


def telescope(B, x, y, z=0.0, yaw=0.0, solid=False):
    """A telescope on a tripod, aimed up toward -y (out of the window it faces)."""
    p = _P(B, x, y, z, yaw, "telescope")
    for k in range(3):
        a = 2 * math.pi * k / 3 + math.pi / 2
        p.rod(WOODL, (0.38 * math.cos(a), 0.38 * math.sin(a), 0.0), (0.0, 0.0, 1.2), 0.03, 3)
    p.cyl(STEEL, 0, 0, 1.18, 1.26, 0.05, 6)
    a = math.radians(35)
    c0 = (0.0, 0.25, 1.3 - 0.25 * math.tan(a))
    c1 = (0.0, -0.55, 1.3 + 0.55 * math.tan(a))
    p.rod("CT_SteelWhite", c0, c1, 0.1, 4, caps=True)
    p.rod(LAQ, c1, (0.0, c1[1] - 0.06, c1[2] + 0.04), 0.13, 4, caps=True)
    p.rod(LAQ, c0, (0.0, c0[1] + 0.08, c0[2] - 0.055), 0.035, 4, caps=True)
    return p.done(eye=(0.0, 0.4, 1.15))


def record_player(B, x, y, z=0.0, yaw=0.0, mat=WOOD, solid=False):
    p = _P(B, x, y, z, yaw, "record_player")
    p.kind = "prop"
    p.bev(mat, -0.22, 0.22, -0.17, 0.17, 0.0, 0.1, 0.01)
    p.cyl(ALU, -0.04, 0.0, 0.1, 0.11, 0.15, 12)
    r = 0.145
    u0, v0, u1, v1 = _uvrect("record_label")
    cu, cv, ru, rv = (u0 + u1) / 2, (v0 + v1) / 2, (u1 - u0) / 2, (v1 - v0) / 2
    n = 12
    pts = [(-0.04 + r * math.cos(2 * math.pi * k / n), r * math.sin(2 * math.pi * k / n), 0.115) for k in range(n)]
    p.m.face(ATLAS_SLOT, pts, [(cu + ru * math.cos(2 * math.pi * k / n), cv + rv * math.sin(2 * math.pi * k / n)) for k in range(n)])
    p.rod(ALU, (0.17, 0.12, 0.13), (0.06, -0.06, 0.125), 0.012, 3)
    p.cyl(ALU, 0.17, 0.12, 0.1, 0.14, 0.02, 6)
    return p.done(top=0.1)


def jukebox(B, x, y, z=0.0, yaw=0.0, mat=WOOD, solid=True):
    p = _P(B, x, y, z, yaw, "jukebox")
    w, d = 0.76, 0.6
    p.bev(mat, -w / 2, w / 2, -d / 2, d / 2, 0.0, 1.15, 0.02, 0.05)
    arc = [(-w / 2 * math.cos(math.pi * k / 8), 1.15 + 0.38 * math.sin(math.pi * k / 8)) for k in range(9)]
    for (a, za), (b, zb_) in zip(arc, arc[1:]):
        p.face(mat, [(a, -d / 2, za), (b, -d / 2, zb_), (b, d / 2, zb_), (a, d / 2, za)])
    front = [(0.0, 1.15)] + arc
    for (a, za), (b, zb_) in zip(arc, arc[1:]):
        p.face(mat, [(0.0, -d / 2, 1.15), (a, -d / 2, za), (b, -d / 2, zb_)][::-1] if a < b else [(0.0, -d / 2, 1.15), (b, -d / 2, zb_), (a, -d / 2, za)])
        p.face(mat, [(0.0, d / 2, 1.15), (b, d / 2, zb_), (a, d / 2, za)][::-1] if a < b else [(0.0, d / 2, 1.15), (a, d / 2, za), (b, d / 2, zb_)])
    _ = front
    p.box("CT_Opal", -w / 2 + 0.03, -w / 2 + 0.09, -d / 2 - 0.012, -d / 2, 0.1, 1.15, f="xXyZz")
    p.box("CT_Opal", w / 2 - 0.09, w / 2 - 0.03, -d / 2 - 0.012, -d / 2, 0.1, 1.15, f="xXyZz")
    p.box("CT_Neon", -w / 2 + 0.12, w / 2 - 0.12, -d / 2 - 0.01, -d / 2, 1.18, 1.25, f="xXyZz")
    p.card_front("sleeve_1", -0.26, -0.01, 0.8, 1.05, -d / 2 - 0.004)
    p.card_front("sleeve_6", 0.01, 0.26, 0.8, 1.05, -d / 2 - 0.004)
    p.card_front("grille", -0.26, 0.26, 0.15, 0.62, -d / 2 - 0.004)
    p.box(ALU, -0.2, 0.2, -d / 2 - 0.06, -d / 2, 0.68, 0.74, f="xXyZz")
    p.core(-w / 2 + 0.02, w / 2 - 0.02, -d / 2 + 0.02, d / 2 - 0.02, 0.1, 1.1)
    return p.done(solid=solid, top=1.5)


def arcade(B, x, y, z=0.0, yaw=0.0, screen=None, mat="CT_PlasterBlue", seed=0, solid=True):
    p = _P(B, x, y, z, yaw, "arcade")
    w = 0.66
    side = [(0.4, 0.0), (0.4, 1.78), (-0.05, 1.78), (-0.1, 1.55), (-0.25, 1.2), (-0.4, 1.0), (-0.4, 0.88), (-0.2, 0.86),
            (-0.2, 0.0)]
    for sgn in (-1, 1):
        xa = sgn * w / 2

        def T(v, h, t, xa=xa):
            return (xa + t, v, h)
        poly = [(v, h) for v, h in side]
        geom.extrude_profile(p.m, mat, mat, poly, -0.02 if sgn < 0 else 0.0, 0.0 if sgn < 0 else 0.02,
                             lambda u, v, t, xa=xa: (xa + t, u, v))
    # front surfaces between the sides
    iw = w / 2 - 0.0
    p.box(LAQ, -iw, iw, -0.2, 0.38, 0.0, 0.86, f="y")
    p.face(LAQ, [(-iw, -0.05, 1.78), (iw, -0.05, 1.78), (iw, 0.4, 1.78), (-iw, 0.4, 1.78)])
    p.card("panel_control", (-iw, -0.4, 1.0), (iw, -0.4, 1.0), (iw, -0.25, 1.2), (-iw, -0.25, 1.2))
    p.face(LAQ, [(iw, 0.4, 0.0), (-iw, 0.4, 0.0), (-iw, 0.4, 1.78), (iw, 0.4, 1.78)])
    p.face(LAQ, [(-iw, -0.4, 1.0), (iw, -0.4, 1.0), (iw, -0.4, 0.88), (-iw, -0.4, 0.88)][::-1])
    rnd = _rng(seed, x, y, "arcade")
    sc = screen or cell("screen_arcade", rnd)
    p.card(sc, (-iw + 0.05, -0.25, 1.2), (iw - 0.05, -0.25, 1.2), (iw - 0.05, -0.1, 1.55), (-iw + 0.05, -0.1, 1.55))
    p.face("CT_Neon", [(-iw, -0.1, 1.55), (iw, -0.1, 1.55), (iw, -0.05, 1.78), (-iw, -0.05, 1.78)])
    p.face(LAQ, [(-iw, -0.2, 0.86), (iw, -0.2, 0.86), (iw, -0.4, 0.88), (-iw, -0.4, 0.88)])
    for k in range(3):
        p.cyl(rnd_mat(k), -0.1 + k * 0.08, -0.33, 1.0 + 0.0, 1.03, 0.017, 6)
    p.rod(LAQ, (0.18, -0.33, 1.0), (0.18, -0.33, 1.1), 0.02, 4)
    p.core(-iw + 0.01, iw - 0.01, -0.18, 0.38, 0.05, 1.7)
    return p.done(solid=solid, top=1.78, screen=(0.0, -0.18, 1.38), front=0.5)


def photo_booth(B, x, y, z=0.0, yaw=0.0, mat="CT_PlasterYellow", curtain="CT_PlasterRed", solid=True):
    p = _P(B, x, y, z, yaw, "photo_booth")
    w, d, h = 1.0, 1.3, 2.1
    p.box(mat, -w / 2, -w / 2 + 0.05, -d / 2, d / 2, 0.0, h, f="xXy")
    p.box(mat, w / 2 - 0.05, w / 2, -d / 2, d / 2, 0.0, h, f="xXy")
    p.box(mat, -w / 2, w / 2, d / 2 - 0.05, d / 2, 0.0, h, f="yY")
    p.bev(mat, -w / 2 - 0.02, w / 2 + 0.02, -d / 2 - 0.02, d / 2 + 0.02, h, h + 0.08, 0.015)
    p.box(mat, -w / 2 + 0.05, w / 2 - 0.05, -d / 2, -d / 2 + 0.05, h - 0.4, h, f="yYz")
    p.card_front("sign_photo", -0.4, 0.4, h - 0.32, h - 0.12, -d / 2 - 0.003)
    p.rod(ALU, (-w / 2, -d / 2 + 0.1, h - 0.42), (w / 2, -d / 2 + 0.1, h - 0.42), 0.02, 3)
    pts = [(-w / 2 + 0.05 + k * 0.1, -d / 2 + 0.1 + (0.03 if k % 2 else -0.02)) for k in range(5)]
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        p.face(curtain, [(ax, ay, 0.15), (bx, by, 0.15), (bx, by, h - 0.42), (ax, ay, h - 0.42)])
        p.face(curtain, [(bx, by + 0.002, 0.15), (ax, ay + 0.002, 0.15), (ax, ay + 0.002, h - 0.42), (bx, by + 0.002, h - 0.42)])
    p.card_front("screen_tv_test", -0.2, 0.2, 1.2, 1.5, d / 2 - 0.052)
    p.cyl(ALU, 0.0, 0.2, 0.0, 0.5, 0.03, 6)
    p.cyl("CT_PlasterRed", 0.0, 0.2, 0.5, 0.55, 0.17, 8)
    p.core(-w / 2 + 0.01, -w / 2 + 0.04, -d / 2 + 0.02, d / 2 - 0.02, 0.1, h - 0.1)
    p.core(w / 2 - 0.04, w / 2 - 0.01, -d / 2 + 0.02, d / 2 - 0.02, 0.1, h - 0.1)
    p.core(-w / 2 + 0.02, w / 2 - 0.02, d / 2 - 0.04, d / 2 - 0.01, 0.1, h - 0.1)
    return p.done(solid=solid, seat=(0.0, 0.2, 0.55), front=0.6)


def table_football(B, x, y, z=0.0, yaw=0.0, mat=WOOD, solid=True):
    p = _P(B, x, y, z, yaw, "table_football")
    L, W = 1.2, 0.7
    p.box(mat, -L / 2, L / 2, -W / 2, W / 2, 0.72, 0.9, f="xXyY")
    p.box("CT_Baize", -L / 2 + 0.03, L / 2 - 0.03, -W / 2 + 0.03, W / 2 - 0.03, 0.74, 0.745, f="Z")
    for (a0, a1, b0, b1) in ((-L / 2, L / 2, -W / 2, -W / 2 + 0.03), (-L / 2, L / 2, W / 2 - 0.03, W / 2),
                             (-L / 2, -L / 2 + 0.03, -W / 2 + 0.03, W / 2 - 0.03), (L / 2 - 0.03, L / 2, -W / 2 + 0.03, W / 2 - 0.03)):
        p.box(mat, a0, a1, b0, b1, 0.9, 0.92, f="Z")
    for sx in (-L / 2 + 0.06, L / 2 - 0.06):
        for sy in (-W / 2 + 0.06, W / 2 - 0.06):
            p.box(mat, sx - 0.04, sx + 0.04, sy - 0.04, sy + 0.04, 0.0, 0.72, f="xXyY")
    for k in range(6):
        xa = -0.45 + k * 0.18
        p.rod(ALU, (xa, -W / 2 - 0.15, 0.85), (xa, W / 2 + 0.15, 0.85), 0.015, 3)
        for j in range(3 if k in (2, 3) else 2):
            ya = -0.18 + j * (0.36 / max(1, (2 if k in (2, 3) else 1)))
            p.box("CT_PlasterRed" if k % 2 else "CT_PlasterBlue", xa - 0.012, xa + 0.012, ya - 0.015, ya + 0.015, 0.76, 0.87)
    p.core(-L / 2 + 0.02, L / 2 - 0.02, -W / 2 + 0.02, W / 2 - 0.02, 0.73, 0.88)
    return p.done(solid=solid, top=0.92)


def dart_board(B, x, y, z=0.0, yaw=0.0, zc=1.73, solid=False):
    p = _P(B, x, y, z, yaw, "dart_board")
    p.kind = "wall"
    p.box(WOOD, -0.3, 0.3, -0.06, 0.0, zc - 0.32, zc + 0.32, f="xXyZz")
    for s in (-1, 1):
        p.slab(WOOD, (s * 0.3, -0.06, zc - 0.32), (s * 0.58, -0.2 if s > 0 else -0.2, zc - 0.32),
               (s * 0.58, -0.2, zc + 0.32), (s * 0.3, -0.06, zc + 0.32), 0.015) if s > 0 else \
            p.slab(WOOD, (s * 0.58, -0.2, zc - 0.32), (s * 0.3, -0.06, zc - 0.32), (s * 0.3, -0.06, zc + 0.32),
                   (s * 0.58, -0.2, zc + 0.32), 0.015)
    p.card_front("dartboard", -0.23, 0.23, zc - 0.23, zc + 0.23, -0.062)
    return p.done(throw=(0.0, -2.6, 0.0))


def slide_projector(B, x, y, z=0.0, yaw=0.0, solid=False):
    p = _P(B, x, y, z, yaw, "slide_projector")
    p.kind = "prop"
    p.bev("CT_PlasterGrey", -0.13, 0.13, -0.15, 0.15, 0.0, 0.12, 0.015, 0.02)
    p.cyl(LAQ, 0.0, 0.03, 0.12, 0.16, 0.13, 10)
    p.rod(LAQ, (0.0, -0.15, 0.06), (0.0, -0.22, 0.06), 0.06, 4, caps=True)
    return p.done(top=0.16)


# ================================================================================================ SPORT, HEALTH
def wall_bars(B, x, y, z=0.0, yaw=0.0, w=0.9, h=2.6, mat=WOODL, solid=False):
    p = _P(B, x, y, z, yaw, "wall_bars")
    p.kind = "wall"
    for sx in (-w / 2, w / 2):
        p.box(mat, sx - 0.03, sx + 0.03, -0.14, -0.02, 0.0, h, f="xXyYZ")
    n = int(h / 0.15)
    for k in range(1, n):
        p.rod(mat, (-w / 2 + 0.03, -0.09, k * 0.15), (w / 2 - 0.03, -0.09, k * 0.15), 0.035, 4)
    return p.done()


def _plates(p, x, y, zc, axis_x=True, n=2):
    for k in range(n):
        off = 0.0 + k * 0.05
        r = 0.22 - k * 0.04
        if axis_x:
            p.rod(LAQ, (x + off, y, zc), (x + off + 0.04, y, zc), r * 2, 8, caps=True)
        else:
            p.rod(LAQ, (x, y + off, zc), (x, y + off + 0.04, zc), r * 2, 8, caps=True)


def barbell_rack(B, x, y, z=0.0, yaw=0.0, solid=True):
    p = _P(B, x, y, z, yaw, "barbell_rack")
    for sx in (-0.5, 0.5):
        p.box(STEEL, sx - 0.03, sx + 0.03, -0.25, 0.25, 0.0, 0.04)
        p.box(STEEL, sx - 0.025, sx + 0.025, -0.025, 0.025, 0.04, 1.1, f="xXyYZ")
        p.box(STEEL, sx - 0.025, sx + 0.025, -0.1, 0.0, 1.0, 1.03, f="xXyYZ")
    p.rod(ALU, (-1.0, -0.06, 1.06), (1.0, -0.06, 1.06), 0.03, 4, caps=True)
    for s in (-1, 1):
        _plates(p, s * 0.72 - (0.09 if s > 0 else 0.0) + (0.0 if s > 0 else 0.0), -0.06, 1.06, True, 2)
    p.core(-0.52, 0.52, -0.02, 0.02, 0.1, 1.0)
    return p.done(solid=solid)


def bench_press(B, x, y, z=0.0, yaw=0.0, solid=True):
    p = _P(B, x, y, z, yaw, "bench_press")
    p.cushion("CT_Leather", -0.15, 0.15, -0.6, 0.6, 0.38, 0.45, 0.01)
    for sy in (-0.5, 0.5):
        p.box(STEEL, -0.03, 0.03, sy - 0.03, sy + 0.03, 0.0, 0.38, f="xXyY")
        p.box(STEEL, -0.2, 0.2, sy - 0.03, sy + 0.03, 0.0, 0.04)
    for sx in (-0.45, 0.45):
        p.box(STEEL, sx - 0.025, sx + 0.025, 0.45, 0.5, 0.0, 1.2, f="xXyYZ")
    p.rod(ALU, (-1.0, 0.43, 1.12), (1.0, 0.43, 1.12), 0.03, 4, caps=True)
    for s in (-1, 1):
        _plates(p, s * 0.75 - (0.09 if s > 0 else 0.0), 0.43, 1.12, True, 2)
    p.core(-0.13, 0.13, -0.58, 0.58, 0.39, 0.44)
    return p.done(solid=solid, seats=[(0.0, -0.2, 0.45, 0.0)])


def punching_bag(B, x, y, z=0.0, yaw=0.0, top_z=3.0, solid=False):
    p = _P(B, x, y, z, yaw, "punching_bag")
    p.lathe("CT_Leather", 0, 0, [(0.0, 0.75), (0.17, 0.82), (0.17, 1.7), (0.12, 1.78)], 8, top=True)
    p.rod(STEEL, (0, 0, 1.78), (0, 0, top_z), 0.012, 3)
    p.box(STEEL, -0.08, 0.08, -0.08, 0.08, top_z - 0.02, top_z - 0.005, f="xXyYz")
    return p.done()


def vaulting_horse(B, x, y, z=0.0, yaw=0.0, solid=True):
    p = _P(B, x, y, z, yaw, "vaulting_horse")
    p.cushion("CT_Leather", -0.8, 0.8, -0.18, 0.18, 0.88, 1.12, 0.02)
    p.box(WOOD, -0.6, 0.6, -0.15, 0.15, 0.8, 0.9, f="xXyY")
    for sx in (-0.55, 0.55):
        for sy in (-1, 1):
            p.rod(WOOD, (sx * 1.12, sy * 0.3, 0.0), (sx, sy * 0.1, 0.82), 0.06, 4)
    p.core(-0.75, 0.75, -0.15, 0.15, 0.9, 1.08)
    return p.done(solid=solid, top=1.12)


def mats(B, x, y, z=0.0, yaw=0.0, w=1.0, d=2.0, n=1, mat="CT_PlasterBlue", solid=False):
    p = _P(B, x, y, z, yaw, "mats")
    p.kind = "flat"
    for k in range(n):
        dz = k * 0.08
        dx = (k % 2) * 0.02
        p.cushion(mat if k % 2 == 0 else "CT_Leather", -w / 2 + dx, w / 2 + dx, -d / 2, d / 2, dz, dz + 0.08, 0.01)
    return p.done(top=n * 0.08)


def basketball_hoop(B, x, y, z=0.0, yaw=0.0, ring_z=3.05, solid=False):
    p = _P(B, x, y, z, yaw, "basketball_hoop")
    p.kind = "wall"
    for sx in (-0.5, 0.5):
        p.rod(STEEL, (sx, 0.0, ring_z + 0.2), (sx * 0.4, -0.6, ring_z + 0.3), 0.04, 4)
    p.box("CT_TileWhite", -0.9, 0.9, -0.64, -0.6, ring_z - 0.15, ring_z + 0.9)
    p.box("CT_SteelRed", -0.3, 0.3, -0.645, -0.64, ring_z, ring_z + 0.45, f="y")
    p.box("CT_TileWhite", -0.27, 0.27, -0.646, -0.645, ring_z + 0.03, ring_z + 0.42, f="y")
    ring = [(0.23 * math.cos(2 * math.pi * k / 10), -0.64 - 0.25 + 0.23 * math.sin(2 * math.pi * k / 10), ring_z) for k in range(10)]
    for a, b in zip(ring, ring[1:] + ring[:1]):
        p.rod("CT_SteelRed", a, b, 0.018, 3)
    low = [(px * 0.6, -0.89 + (py + 0.89) * 0.6, ring_z - 0.4) for px, py, _ in ring]
    for a, b, c_, d_ in zip(ring, ring[1:] + ring[:1], low[1:] + low[:1], low):
        p.face("CT_Paper", [d_, c_, b, a])
        p.face("CT_Paper", [a, b, c_, d_])
    return p.done(ring=(0.0, -0.89, ring_z))


def exercise_bike(B, x, y, z=0.0, yaw=0.0, mat="CT_SteelRed", solid=False):
    """The rider faces -y."""
    p = _P(B, x, y, z, yaw, "exercise_bike")
    for sy in (-0.45, 0.4):
        p.box(STEEL, -0.25, 0.25, sy - 0.03, sy + 0.03, 0.0, 0.05)
    p.rod(mat, (0.0, -0.42, 0.05), (0.0, -0.3, 1.0), 0.05, 4)
    p.rod(mat, (0.0, 0.38, 0.05), (0.0, 0.0, 0.4), 0.05, 4)
    p.rod(mat, (0.0, 0.0, 0.4), (0.0, -0.36, 0.5), 0.05, 4)
    p.rod(mat, (0.0, 0.0, 0.4), (0.0, 0.12, 0.85), 0.04, 4)
    p.cushion(LAQ, -0.1, 0.1, 0.05, 0.3, 0.85, 0.92, 0.01)
    p.rod(STEEL, (-0.22, -0.3, 1.02), (0.22, -0.3, 1.02), 0.03, 4)
    for s in (-1, 1):
        p.rod(LAQ, (s * 0.22, -0.3, 1.02), (s * 0.24, -0.18, 1.1), 0.03, 4)
    p.rod(LAQ, (-0.03, -0.36, 0.5), (0.03, -0.36, 0.5), 0.36, 4, caps=True)
    return p.done(seats=[(0.0, 0.17, 0.92, 0.0)])


def medical_couch(B, x, y, z=0.0, yaw=0.0, solid=True):
    p = _P(B, x, y, z, yaw, "medical_couch")
    _legs(p, ALU, (-0.85, 0.85), (-0.27, 0.27), 0.6, 0.035, 1.0)
    p.box(ALU, -0.9, 0.9, -0.3, 0.3, 0.58, 0.6)
    p.cushion("CT_Leather", -0.92, 0.6, -0.32, 0.32, 0.6, 0.7, 0.01)
    p.hexa("CT_Leather", [(0.6, -0.32, 0.6), (0.95, -0.32, 0.6), (0.95, 0.32, 0.6), (0.6, 0.32, 0.6)],
           [(0.6, -0.32, 0.7), (0.92, -0.32, 0.85), (0.92, 0.32, 0.85), (0.6, 0.32, 0.7)])
    p.box("CT_Paper", -0.88, 0.55, -0.25, 0.25, 0.7, 0.703, f="Z")
    p.core(-0.88, 0.88, -0.28, 0.28, 0.61, 0.68)
    return p.done(solid=solid, top=0.7, seats=[(0.0, -0.1, 0.7, 0.0)])


def eye_chart(B, x, y, z=0.0, yaw=0.0, zc=1.6, solid=False):
    p = _P(B, x, y, z, yaw, "eye_chart")
    p.kind = "wall"
    p.box("CT_SteelWhite", -0.36, 0.36, -0.12, 0.0, zc - 0.5, zc + 0.5, f="xXyZz")
    p.card_front("chart_eye", -0.32, 0.32, zc - 0.45, zc + 0.45, -0.122)
    return p.done()


def scales(B, x, y, z=0.0, yaw=0.0, solid=False):
    """Medical column scales with a height rod; the patient faces -y standing on it."""
    p = _P(B, x, y, z, yaw, "scales")
    p.bev("CT_SteelWhite", -0.22, 0.22, -0.2, 0.25, 0.0, 0.08, 0.01, 0.02)
    p.box("CT_Rubber", -0.18, 0.18, -0.17, 0.12, 0.08, 0.085, f="Z")
    p.box("CT_SteelWhite", -0.04, 0.04, 0.17, 0.25, 0.08, 1.35, f="xXyYZ")
    p.box("CT_SteelWhite", -0.15, 0.15, 0.1, 0.2, 1.1, 1.3)
    p.box(ALU, -0.13, 0.13, 0.095, 0.1, 1.2, 1.24, f="y")
    p.rod(ALU, (0.0, 0.2, 1.35), (0.0, 0.2, 2.0), 0.02, 4)
    p.box(ALU, -0.01, 0.01, -0.05, 0.2, 1.85, 1.865)
    return p.done()


def screen_divider(B, x, y, z=0.0, yaw=0.0, mat=None, solid=False):
    p = _P(B, x, y, z, yaw, "screen_divider")
    pts = [(-0.75, 0.15), (-0.25, -0.05), (0.25, 0.15), (0.75, -0.05)]
    fab = mat or "fabric_3"
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        for zz in (0.04, 1.66):
            p.rod(WOOD, (ax, ay, zz), (bx, by, zz), 0.03, 4)
        p.rod(WOOD, (ax, ay, 0.0), (ax, ay, 1.7), 0.035, 4)
        if fab in ATLAS:
            p.card(fab, (ax, ay, 0.06), (bx, by, 0.06), (bx, by, 1.64), (ax, ay, 1.64))
            p.card(fab, (bx, by, 0.06), (ax, ay, 0.06), (ax, ay, 1.64), (bx, by, 1.64))
        else:
            p.face(fab, [(ax, ay, 0.06), (bx, by, 0.06), (bx, by, 1.64), (ax, ay, 1.64)])
            p.face(fab, [(bx, by, 0.06), (ax, ay, 0.06), (ax, ay, 1.64), (bx, by, 1.64)])
    p.rod(WOOD, (0.75, -0.05, 0.0), (0.75, -0.05, 1.7), 0.035, 4)
    return p.done()


def dentist_chair(B, x, y, z=0.0, yaw=0.0, mat="CT_PlasterGreen", solid=True):
    """The patient faces yaw (lying back toward +y)."""
    p = _P(B, x, y, z, yaw, "dentist_chair")
    p.cyl("CT_SteelWhite", 0, 0.1, 0.0, 0.04, 0.3, 8, r1=0.28)
    p.cyl("CT_SteelWhite", 0, 0.1, 0.04, 0.4, 0.08, 6)
    p.cushion(mat, -0.27, 0.27, -0.25, 0.3, 0.4, 0.52, 0.015)
    p.hexa(mat, [(-0.27, 0.25, 0.48), (0.27, 0.25, 0.48), (0.27, 0.35, 0.48), (-0.27, 0.35, 0.48)],
           [(-0.25, 0.55, 1.08), (0.25, 0.55, 1.08), (0.25, 0.65, 1.08), (-0.25, 0.65, 1.08)])
    p.cushion(mat, -0.12, 0.12, 0.52, 0.66, 1.08, 1.2, 0.01)
    p.slab(mat, (-0.22, -0.7, 0.2), (0.22, -0.7, 0.2), (0.24, -0.25, 0.45), (-0.24, -0.25, 0.45), 0.05)
    for s in (-1, 1):
        p.box(mat, s * 0.27 - 0.04, s * 0.27 + 0.04, -0.15, 0.25, 0.62, 0.66)
        p.rod(ALU, (s * 0.27, 0.0, 0.52), (s * 0.27, 0.0, 0.62), 0.03, 3)
    p.rod("CT_SteelWhite", (0.45, 0.4, 0.0), (0.45, 0.4, 1.7), 0.06, 4)
    p.rod("CT_SteelWhite", (0.45, 0.4, 1.7), (0.0, -0.2, 1.75), 0.04, 4)
    p.lathe("CT_SteelWhite", 0.0, -0.2, [(0.0, 1.6), (0.14, 1.66), (0.12, 1.75)], 8, top=True)
    p.face("CT_Opal", [(px, py, 1.605) for px, py, _ in reversed(_ring_circle(0.0, -0.2, 0.1, 0, 8))])
    p.lathe("CT_TileWhite", -0.5, 0.0, [(0.03, 0.0), (0.04, 0.8), (0.14, 0.86), (0.12, 0.92)], 8, top=False)
    p.core(-0.25, 0.25, -0.22, 0.28, 0.41, 0.5)
    return p.done(solid=solid, seats=[(0.0, 0.0, 0.52, 0.0)])


# ================================================================================================ SCHOOL
def blackboard(B, x, y, z=0.0, yaw=0.0, w=2.4, h=1.15, zb=0.9, writing=None, seed=0, solid=False):
    p = _P(B, x, y, z, yaw, "blackboard")
    p.kind = "wall"
    rnd = _rng(seed, x, y, "board")
    p.box(WOOD, -w / 2 - 0.05, w / 2 + 0.05, -0.04, 0.0, zb - 0.05, zb + h + 0.05, f="xXyZz")
    p.card_front(writing or cell("board", rnd), -w / 2, w / 2, zb, zb + h, -0.042)
    p.box(WOOD, -w / 2, w / 2, -0.12, -0.04, zb - 0.07, zb - 0.045)
    p.box(WOOD, -w / 2, w / 2, -0.125, -0.115, zb - 0.045, zb - 0.025, f="xXyY")
    for k in range(3):
        p.box("CT_TileWhite", -0.4 + k * 0.1, -0.33 + k * 0.1, -0.1, -0.09, zb - 0.045, zb - 0.035, f="xXyYZ")
    p.box("CT_Wood", 0.5, 0.65, -0.11, -0.06, zb - 0.045, zb - 0.015, f="xXyYZ")
    return p.done()


def teacher_desk(B, x, y, z=0.0, yaw=0.0, podium=True, solid=True):
    """The teacher's desk FACES the class (yaw), the teacher sits behind it at +y."""
    zz = 0.0
    if podium:
        p0 = _P(B, x, y, z, yaw, "podium")
        p0.kind = "flat"
        p0.bev(WOOD, -1.2, 1.2, -0.7, 0.9, 0.0, 0.16, 0.012)
        p0.core(-1.18, 1.18, -0.68, 0.88, 0.0, 0.15)
        p0.done(solid=solid)
        zz = 0.16
    r = office_desk(B, x, y, z + zz, yaw, w=1.3, d=0.7, typewriter_on=False, papers=True, phone=False, solid=solid)
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    books_stack(B, x + 0.4 * c - 0.1 * s, y + 0.4 * s + 0.1 * c, z + zz + 0.75, yaw + 10, n=3)
    r["name"] = "teacher_desk"
    return r


def wall_map(B, x, y, z=0.0, yaw=0.0, w=1.6, h=1.2, zc=1.7, name="map_coast", solid=False):
    p = _P(B, x, y, z, yaw, "wall_map")
    p.kind = "wall"
    p.rod(WOOD, (-w / 2 - 0.05, -0.03, zc + h / 2), (w / 2 + 0.05, -0.03, zc + h / 2), 0.03, 4, caps=True)
    p.rod(WOOD, (-w / 2 - 0.05, -0.03, zc - h / 2), (w / 2 + 0.05, -0.03, zc - h / 2), 0.035, 4, caps=True)
    p.card_front(cell(name), -w / 2, w / 2, zc - h / 2 + 0.015, zc + h / 2 - 0.015, -0.025)
    p.rod("CT_Paper", (0.0, -0.03, zc + h / 2), (0.0, -0.01, zc + h / 2 + 0.25), 0.006, 3)
    return p.done()


def skeleton(B, x, y, z=0.0, yaw=0.0, solid=False):
    p = _P(B, x, y, z, yaw, "skeleton")
    bone = "CT_Paper"
    p.cyl(STEEL, 0, 0.15, 0.0, 0.03, 0.2, 6)
    p.rod(STEEL, (0, 0.15, 0.03), (0, 0.15, 1.6), 0.02, 3)
    p.rod(STEEL, (0, 0.15, 1.6), (0, 0.02, 1.62), 0.02, 3)
    p.lathe(bone, 0, 0, [(0.0, 1.5), (0.07, 1.56), (0.09, 1.65), (0.07, 1.72), (0.0, 1.75)], 6)
    p.rod(bone, (0, 0.02, 1.0), (0, 0.02, 1.5), 0.03, 4)
    for k in range(5):
        zz = 1.18 + k * 0.055
        p.tube(bone, [(-0.12, -0.02, zz), (-0.14, 0.05, zz - 0.02), (0, 0.06, zz - 0.03), (0.14, 0.05, zz - 0.02), (0.12, -0.02, zz)], 0.015, 3)
    p.rod(bone, (-0.18, 0.02, 1.42), (0.18, 0.02, 1.42), 0.025, 3)
    p.lathe(bone, 0, 0.02, [(0.1, 0.92), (0.16, 0.98), (0.1, 1.06)], 6, top=False)
    for s in (-1, 1):
        p.tube(bone, [(s * 0.18, 0.02, 1.42), (s * 0.22, 0.02, 1.13), (s * 0.24, -0.03, 0.85)], 0.025, 3)
        p.tube(bone, [(s * 0.09, 0.02, 0.95), (s * 0.1, 0.0, 0.5), (s * 0.1, 0.03, 0.08)], 0.03, 3)
        p.box(bone, s * 0.1 - 0.04, s * 0.1 + 0.04, -0.08, 0.05, 0.03, 0.07)
    return p.done()


def lab_bench(B, x, y, z=0.0, yaw=0.0, w=2.0, d=0.75, seed=0, solid=True):
    p = _P(B, x, y, z, yaw, "lab_bench")
    p.box("CT_PlasterGrey", -w / 2, w / 2, -d / 2, d / 2, 0.0, 0.86, f="xXyY")
    p.bev(LAQ, -w / 2 - 0.02, w / 2 + 0.02, -d / 2 - 0.03, d / 2, 0.86, 0.9, 0.006)
    p.box("CT_Paper", -w / 2, w / 2, d / 2 - 0.15, d / 2, 0.9, 1.25, f="yY")
    p.box(LAQ, -w / 2, w / 2, d / 2 - 0.25, d / 2, 1.25, 1.27)
    p.box("CT_TileWhite", w / 2 - 0.5, w / 2 - 0.15, -0.15, 0.15, 0.88, 0.902, f="Z")
    p.rod(ALU, (w / 2 - 0.32, 0.25, 0.9), (w / 2 - 0.32, 0.25, 1.2), 0.02, 3)
    p.rod(ALU, (w / 2 - 0.32, 0.25, 1.2), (w / 2 - 0.32, 0.05, 1.15), 0.02, 3)
    rnd = _rng(seed, x, y, "lab")
    for k in range(5):
        u = -w / 2 + 0.2 + k * 0.25
        kind = k % 3
        if kind == 0:
            p.lathe("CT_Glass", u, 0.0, [(0.06, 0.9), (0.06, 0.95), (0.02, 1.05), (0.018, 1.12)], 6)
        elif kind == 1:
            p.lathe("CT_Glass", u, 0.0, [(0.035, 0.9), (0.035, 1.05)], 6, top=False)
            p.cyl(rnd.choice(["CT_PlasterBlue", "CT_PlasterGreen", "CT_PlasterRed"]), u, 0.0, 0.902, 0.98, 0.03, 6)
        else:
            p.cyl(BRASS, u, 0.0, 0.9, 0.92, 0.05, 6)
            p.cyl(BRASS, u, 0.0, 0.92, 1.05, 0.012, 4)
        p.box(rnd.choice(PAINTS), u - 0.05, u + 0.05, d / 2 - 0.22, d / 2 - 0.12, 1.27, 1.27 + rnd.uniform(0.08, 0.14))
    p.core(-w / 2 + 0.02, w / 2 - 0.02, -d / 2 + 0.02, d / 2 - 0.02, 0.1, 0.84)
    return p.done(solid=solid, top=0.9)


# ================================================================================================ WALLS AND DECOR
def picture(B, x, y, z=0.0, yaw=0.0, name="painting", w=0.7, h=None, zc=1.6, frame=WOOD, mat=None, seed=0, solid=False):
    """[wall] A framed picture of an atlas cell (name or group). h defaults to the cell's aspect."""
    p = _P(B, x, y, z, yaw, "picture")
    p.kind = "wall"
    cn = cell(name, _rng(seed, x, y, "pic"))
    if h is None:
        u0, v0, u1, v1 = _uvrect(cn)
        h = w * (v1 - v0) / max(1e-6, (u1 - u0))
    f = 0.045
    # frame as 4 bevelled bars
    for (a0, a1, b0, b1) in ((-w / 2 - f, w / 2 + f, zc - h / 2 - f, zc - h / 2), (-w / 2 - f, w / 2 + f, zc + h / 2, zc + h / 2 + f),
                             (-w / 2 - f, -w / 2, zc - h / 2, zc + h / 2), (w / 2, w / 2 + f, zc - h / 2, zc + h / 2)):
        p.box(frame, a0, a1, -0.035, 0.0, b0, b1, f="xXyZz")
    p.card_front(cn, -w / 2, w / 2, zc - h / 2, zc + h / 2, -0.018)
    return p.done(cell=cn)


def poster(B, x, y, z=0.0, yaw=0.0, name="poster", w=0.6, h=None, zc=1.55, seed=0, solid=False):
    """[wall] An unframed poster (paper 4 mm off the wall)."""
    p = _P(B, x, y, z, yaw, "poster")
    p.kind = "wall"
    cn = cell(name, _rng(seed, x, y, "poster"))
    if h is None:
        u0, v0, u1, v1 = _uvrect(cn)
        h = w * (v1 - v0) / max(1e-6, (u1 - u0))
    p.card_front(cn, -w / 2, w / 2, zc - h / 2, zc + h / 2, -0.004)
    return p.done(cell=cn)


def notice_board(B, x, y, z=0.0, yaw=0.0, w=1.2, h=None, zc=1.5, seed=0, solid=False):
    p = _P(B, x, y, z, yaw, "notice_board")
    p.kind = "wall"
    cn = cell("notice", _rng(seed, x, y, "notice"))
    if h is None:
        u0, v0, u1, v1 = _uvrect(cn)
        h = w * (v1 - v0) / max(1e-6, (u1 - u0))
    p.box(WOODL, -w / 2 - 0.04, w / 2 + 0.04, -0.03, 0.0, zc - h / 2 - 0.04, zc + h / 2 + 0.04, f="xXyZz")
    p.card_front(cn, -w / 2, w / 2, zc - h / 2, zc + h / 2, -0.032)
    return p.done(cell=cn)


def banner(B, x, y, z=0.0, yaw=0.0, name="poster_sun_2", w=2.0, h=None, zc=2.6, solid=False):
    p = _P(B, x, y, z, yaw, "banner")
    p.kind = "wall"
    cn = cell(name)
    if h is None:
        u0, v0, u1, v1 = _uvrect(cn)
        h = w * (v1 - v0) / max(1e-6, (u1 - u0))
    p.rod(WOOD, (-w / 2 - 0.08, -0.06, zc + h / 2 + 0.02), (w / 2 + 0.08, -0.06, zc + h / 2 + 0.02), 0.03, 4, caps=True)
    for s in (-1, 1):
        p.rod("CT_Paper", (s * w / 2, -0.06, zc + h / 2 + 0.02), (s * w * 0.2, -0.01, zc + h / 2 + 0.3), 0.006, 3)
    p.card_front(cn, -w / 2, w / 2, zc - h / 2, zc + h / 2, -0.06)
    p.card_back(cn, -w / 2, w / 2, zc - h / 2, zc + h / 2, -0.058)
    return p.done(cell=cn)


def mosaic_panel(B, x, y, z=0.0, yaw=0.0, name="tile", w=1.2, h=1.2, zc=1.5, seed=0, solid=False):
    p = _P(B, x, y, z, yaw, "mosaic_panel")
    p.kind = "wall"
    cn = cell(name, _rng(seed, x, y, "mosaic"))
    p.box(ALU, -w / 2 - 0.02, w / 2 + 0.02, -0.02, 0.0, zc - h / 2 - 0.02, zc + h / 2 + 0.02, f="xXyZz")
    p.card_front(cn, -w / 2, w / 2, zc - h / 2, zc + h / 2, -0.022)
    return p.done(cell=cn)


def wall_shelf(B, x, y, z=0.0, yaw=0.0, w=0.9, zt=1.5, mat=WOOD, props=True, seed=0, solid=False):
    p = _P(B, x, y, z, yaw, "wall_shelf")
    p.kind = "wall"
    p.bev(mat, -w / 2, w / 2, -0.22, 0.0, zt - 0.025, zt, 0.006)
    for sx in (-w / 2 + 0.12, w / 2 - 0.12):
        p.hexa(STEEL, [(sx - 0.01, -0.005, zt - 0.2), (sx + 0.01, -0.005, zt - 0.2), (sx + 0.01, 0.0, zt - 0.2), (sx - 0.01, 0.0, zt - 0.2)],
               [(sx - 0.01, -0.17, zt - 0.025), (sx + 0.01, -0.17, zt - 0.025), (sx + 0.01, 0.0, zt - 0.025), (sx - 0.01, 0.0, zt - 0.025)])
    r = p.done(top=zt)
    if props:
        rnd = _rng(seed, x, y, "wshelf")
        c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        at = lambda u, v: (x + u * c - v * s, y + u * s + v * c)
        a = at(-w / 4, -0.1)
        books_stack(B, a[0], a[1], z + zt, yaw + 90, n=rnd.randint(3, 5), seed=seed)
        b = at(w / 4, -0.1)
        vase(B, b[0], b[1], z + zt, yaw, seed=seed)
    return r


def sign_plate(B, x, y, z=0.0, yaw=0.0, name="sign_library", w=1.2, h=None, zc=2.4, hang=False, solid=False):
    """[wall] A sign plate (hang=True: hung from the ceiling at zc, both faces)."""
    p = _P(B, x, y, z, yaw, "sign_plate")
    p.kind = "wall"
    cn = cell(name)
    if h is None:
        u0, v0, u1, v1 = _uvrect(cn)
        h = w * (v1 - v0) / max(1e-6, (u1 - u0))
    if hang:
        p.box("CT_Steel", -w / 2, w / 2, -0.012, 0.012, zc - h / 2, zc + h / 2, f="xXZz")
        p.card_front(cn, -w / 2, w / 2, zc - h / 2, zc + h / 2, -0.013)
        p.card_back(cn, -w / 2, w / 2, zc - h / 2, zc + h / 2, 0.013)
        for sx in (-w / 2 + 0.1, w / 2 - 0.1):
            p.rod(STEEL, (sx, 0.0, zc + h / 2), (sx, 0.0, zc + h / 2 + 0.6), 0.008, 3)
    else:
        p.box("CT_Steel", -w / 2, w / 2, -0.02, 0.0, zc - h / 2, zc + h / 2, f="xXyZz")
        p.card_front(cn, -w / 2 + 0.005, w / 2 - 0.005, zc - h / 2 + 0.005, zc + h / 2 - 0.005, -0.022)
    return p.done(cell=cn)


def menu_board(B, x, y, z=0.0, yaw=0.0, name="menu", w=0.7, zc=1.8, seed=0, solid=False):
    p = _P(B, x, y, z, yaw, "menu_board")
    p.kind = "wall"
    cn = cell(name, _rng(seed, x, y, "menu"))
    u0, v0, u1, v1 = _uvrect(cn)
    h = w * (v1 - v0) / max(1e-6, (u1 - u0))
    p.box(WOOD, -w / 2 - 0.04, w / 2 + 0.04, -0.03, 0.0, zc - h / 2 - 0.04, zc + h / 2 + 0.04, f="xXyZz")
    p.card_front(cn, -w / 2, w / 2, zc - h / 2, zc + h / 2, -0.032)
    return p.done(cell=cn)


def fire_extinguisher(B, x, y, z=0.0, yaw=0.0, solid=False):
    p = _P(B, x, y, z, yaw, "fire_extinguisher")
    p.kind = "wall"
    p.box(STEEL, -0.06, 0.06, -0.02, 0.0, 0.85, 1.05, f="xXyZz")
    p.lathe("CT_SteelRed", 0, -0.1, [(0.07, 0.55), (0.08, 0.6), (0.08, 1.05), (0.05, 1.12), (0.02, 1.15)], 8)
    p.rod(LAQ, (0.0, -0.1, 1.15), (0.0, -0.1, 1.22), 0.03, 4)
    p.tube(LAQ, [(0.0, -0.1, 1.2), (0.08, -0.12, 1.15), (0.09, -0.15, 0.7)], 0.015, 3)
    p.card_front("label_17", -0.05, 0.05, 0.75, 0.88, -0.181)
    return p.done()


def wall_phone(B, x, y, z=0.0, yaw=0.0, mat="CT_PlasterRed", zc=1.45, solid=False):
    p = _P(B, x, y, z, yaw, "wall_phone")
    p.kind = "wall"
    p.bev(mat, -0.13, 0.13, -0.14, 0.0, zc - 0.25, zc + 0.15, 0.015, 0.02)
    p.card_front("dial_phone", -0.07, 0.07, zc - 0.12, zc + 0.02, -0.142)
    p.box(LAQ, -0.12, -0.08, -0.2, -0.14, zc - 0.18, zc + 0.08)
    p.box(LAQ, -0.13, -0.07, -0.21, -0.14, zc + 0.05, zc + 0.11)
    p.box(LAQ, -0.13, -0.07, -0.21, -0.14, zc - 0.21, zc - 0.15)
    p.box(ALU, 0.05, 0.1, -0.145, -0.14, zc + 0.05, zc + 0.1, f="y")
    return p.done()


def pendant(B, x, y, ceiling, drop=0.6, z=0.0, yaw=0.0, kind="opal", light=False, lumens=700, colour="#FFD9A8",
            lamp_kind="public", radius=6.0, solid=False):
    """[ceiling] A hanging lamp at (x, y) under the ceiling height `ceiling`: kind opal (globe) | cone (enamel shade) |
    green (library). light=True: also a real lamp (B.lamp, counts against the lamp budget)."""
    p = _P(B, x, y, ceiling, yaw, "pendant")
    p.kind = "ceiling"
    p.rod(STEEL, (0, 0, 0.0), (0, 0, -drop + 0.1), 0.008, 3)
    p.cyl(STEEL, 0, 0, -0.02, 0.0, 0.05, 6, top=False, bottom=True)
    zb = -drop
    if kind == "opal":
        p.lathe("CT_Opal", 0, 0, [(0.0, zb - 0.1), (0.12, zb - 0.02), (0.13, zb + 0.06), (0.06, zb + 0.13), (0.02, zb + 0.15)], 8)
    elif kind == "green":
        p.lathe("CT_Baize", 0, 0, [(0.22, zb), (0.12, zb + 0.08), (0.03, zb + 0.13)], 8, top=True)
        p.lathe("CT_Opal", 0, 0, [(0.2, zb + 0.005), (0.0, zb + 0.01)], 8, top=False)
    else:
        p.lathe("CT_SteelWhite", 0, 0, [(0.2, zb), (0.08, zb + 0.15), (0.03, zb + 0.18)], 8, top=True)
        p.lathe("CT_Opal", 0, 0, [(0.18, zb + 0.005), (0.0, zb + 0.02)], 8, top=False)
    if light:
        B.lamp((x, y, ceiling - drop), lumens=lumens, colour=colour, kind=lamp_kind, radius=radius, fixture=False)
    return p.done()


def ceiling_fan(B, x, y, ceiling, z=0.0, yaw=0.0, solid=False):
    p = _P(B, x, y, ceiling, yaw, "ceiling_fan")
    p.kind = "ceiling"
    p.rod(STEEL, (0, 0, 0.0), (0, 0, -0.35), 0.03, 4)
    p.cyl("CT_SteelWhite", 0, 0, -0.45, -0.33, 0.1, 8, bottom=True)
    for k in range(4):
        a = math.pi / 2 * k + math.radians(yaw)
        c, s = math.cos(a), math.sin(a)
        q = [(c * 0.1 - s * 0.05, s * 0.1 + c * 0.05), (c * 0.7 - s * 0.07, s * 0.7 + c * 0.07),
             (c * 0.7 + s * 0.07, s * 0.7 - c * 0.07), (c * 0.1 + s * 0.05, s * 0.1 - c * 0.05)]
        q = q if (q[1][0] - q[0][0]) * (q[2][1] - q[0][1]) - (q[1][1] - q[0][1]) * (q[2][0] - q[0][0]) > 0 else q[::-1]
        p.loft(WOOD, [[(u, v, -0.41) for u, v in q], [(u, v, -0.4) for u, v in q]], top=True, bottom=True)
    return p.done()


# ================================================================================================ SMALL PROPS
def books_stack(B, x, y, z=0.0, yaw=0.0, n=4, seed=0, solid=False):
    p = _P(B, x, y, z, yaw, "books_stack")
    p.kind = "prop"
    rnd = _rng(seed, x, y, "stack")
    zz = 0.0
    for k in range(n):
        t = rnd.uniform(0.025, 0.045)
        w, d = rnd.uniform(0.15, 0.22), rnd.uniform(0.2, 0.27)
        a = rnd.uniform(-0.2, 0.2)
        c, s = math.cos(a), math.sin(a)
        pts = [(u * c - v * s, u * s + v * c) for u, v in ((-w / 2, -d / 2), (w / 2, -d / 2), (w / 2, d / 2), (-w / 2, d / 2))]
        mat = rnd.choice(["CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterGreen", WOOD, "CT_Leather", "CT_PlasterYellow"])
        p.loft(mat, [[(u, v, zz) for u, v in pts], [(u, v, zz + t) for u, v in pts]], top=True)
        zz += t + 0.001
    return p.done(top=zz)


def newspaper(B, x, y, z=0.0, yaw=0.0, cellname=None, seed=0, solid=False):
    p = _P(B, x, y, z, yaw, "newspaper")
    p.kind = "prop"
    cn = cellname or cell("news", _rng(seed, x, y, "news"))
    p.card_top(cn, -0.14, 0.14, -0.19, 0.19, 0.003)
    p.box("CT_Paper", -0.14, 0.14, -0.19, 0.19, 0.0, 0.002, f="xXyY")
    return p.done(top=0.003)


def cup(B, x, y, z=0.0, yaw=0.0, mat="CT_TileWhite", saucer=True, solid=False):
    p = _P(B, x, y, z, yaw, "cup")
    p.kind = "prop"
    if saucer:
        p.cyl(mat, 0, 0, 0.0, 0.01, 0.06, 8, r1=0.07)
    p.lathe(mat, 0, 0, [(0.03, 0.01), (0.042, 0.07)], 7, top=False)
    p.cyl("CT_Wood", 0, 0, 0.06, 0.062, 0.039, 7) if True else None
    p.rod(mat, (0.042, 0.0, 0.055), (0.06, 0.0, 0.035), 0.012, 3)
    return p.done(top=0.07)


def glass(B, x, y, z=0.0, yaw=0.0, solid=False):
    p = _P(B, x, y, z, yaw, "glass")
    p.kind = "prop"
    p.lathe("CT_Glass", 0, 0, [(0.028, 0.0), (0.034, 0.11)], 6, top=False)
    return p.done(top=0.11)


def teapot(B, x, y, z=0.0, yaw=0.0, mat="CT_TileWhite", solid=False):
    p = _P(B, x, y, z, yaw, "teapot")
    p.kind = "prop"
    p.lathe(mat, 0, 0, [(0.05, 0.0), (0.08, 0.04), (0.08, 0.09), (0.05, 0.13), (0.02, 0.15)], 8)
    p.rod(mat, (0.07, 0.0, 0.05), (0.14, 0.0, 0.12), 0.02, 3)
    p.tube(mat, [(-0.07, 0.0, 0.11), (-0.12, 0.0, 0.1), (-0.12, 0.0, 0.05), (-0.075, 0.0, 0.04)], 0.014, 3)
    return p.done(top=0.15)


def plates(B, x, y, z=0.0, yaw=0.0, n=4, mat="CT_TileWhite", solid=False):
    p = _P(B, x, y, z, yaw, "plates")
    p.kind = "prop"
    for k in range(n):
        p.cyl(mat, 0, 0, k * 0.015, k * 0.015 + 0.012, 0.1, 10, r1=0.12)
    return p.done(top=n * 0.015)


def bottles(B, x, y, z=0.0, yaw=0.0, n=3, seed=0, solid=False):
    p = _P(B, x, y, z, yaw, "bottles")
    p.kind = "prop"
    rnd = _rng(seed, x, y, "bottles")
    for k in range(n):
        _bottle(p, -0.08 * (n - 1) / 2 + k * 0.08, 0.0, 0.0, rnd.uniform(0.24, 0.32),
                rnd.choice(["CT_Glass", "CT_PlasterGreen", "CT_Copper"]), cell("label", rnd))
    return p.done(top=0.3)


def vase(B, x, y, z=0.0, yaw=0.0, mat=None, flowers=True, seed=0, solid=False):
    p = _P(B, x, y, z, yaw, "vase")
    p.kind = "prop"
    rnd = _rng(seed, x, y, "vase")
    mt = mat or rnd.choice(["CT_PlasterBlue", "CT_TileWhite", "CT_Copper", "CT_PlasterRed"])
    p.lathe(mt, 0, 0, [(0.05, 0.0), (0.08, 0.08), (0.04, 0.2), (0.05, 0.24)], 7, top=False)
    p.cyl("CT_ConcreteDark", 0, 0, 0.21, 0.215, 0.04, 6)
    if flowers:
        for k in range(5):
            a = k * 1.3
            tip = (0.08 * math.cos(a), 0.08 * math.sin(a), 0.36 + 0.05 * (k % 3))
            p.rod("CT_Leaf", (0, 0, 0.2), tip, 0.008, 3)
            p.lathe(rnd.choice(["CT_PlasterRed", "CT_PlasterYellow", "CT_TileWhite", "CT_PlasterPink"]), tip[0], tip[1],
                    [(0.0, tip[2] - 0.02), (0.035, tip[2]), (0.0, tip[2] + 0.025)], 5)
    return p.done(top=0.24)


def typewriter(B, x, y, z=0.0, yaw=0.0, mat="CT_PlasterGreen", solid=False):
    """[prop] The typist sits at -y (yaw = the way the typist looks... the machine faces the typist)."""
    p = _P(B, x, y, z, yaw, "typewriter")
    p.kind = "prop"
    p.hexa(mat, [(-0.2, -0.17, 0.0), (0.2, -0.17, 0.0), (0.2, 0.15, 0.0), (-0.2, 0.15, 0.0)],
           [(-0.2, -0.17, 0.04), (0.2, -0.17, 0.04), (0.2, 0.15, 0.13), (-0.2, 0.15, 0.13)])
    p.card_tilt("typewriter_keys", -0.17, 0.17, -0.165, 0.042, 0.0, 0.09)
    p.rod(LAQ, (-0.25, 0.12, 0.15), (0.25, 0.12, 0.15), 0.06, 4, caps=True)
    p.card("paper_typed", (-0.1, 0.13, 0.16), (0.1, 0.13, 0.16), (0.1, 0.2, 0.36), (-0.1, 0.2, 0.36))
    p.card("paper_typed", (0.1, 0.132, 0.16), (-0.1, 0.132, 0.16), (-0.1, 0.202, 0.36), (0.1, 0.202, 0.36), rect=(0.0, 0.0, 0.05, 0.05))
    return p.done(top=0.15)


def telephone(B, x, y, z=0.0, yaw=0.0, mat=LAQ, solid=False):
    p = _P(B, x, y, z, yaw, "telephone")
    p.kind = "prop"
    p.hexa(mat, [(-0.1, -0.11, 0.0), (0.1, -0.11, 0.0), (0.1, 0.1, 0.0), (-0.1, 0.1, 0.0)],
           [(-0.08, -0.09, 0.07), (0.08, -0.09, 0.07), (0.07, 0.07, 0.1), (-0.07, 0.07, 0.1)])
    p.card_tilt("dial_phone", -0.05, 0.05, -0.09, 0.07, 0.01, 0.087)
    p.rod(mat, (-0.12, 0.05, 0.12), (0.12, 0.05, 0.12), 0.035, 4, caps=True)
    for s in (-1, 1):
        p.rod(mat, (s * 0.11, 0.05, 0.1), (s * 0.11, 0.05, 0.14), 0.05, 4, caps=True)
    return p.done(top=0.14)


def cash_register(B, x, y, z=0.0, yaw=0.0, mat="CT_Brass", solid=False):
    """[prop] The till; the cashier stands at +y (behind), customers see the price display at -y."""
    p = _P(B, x, y, z, yaw, "cash_register")
    p.kind = "prop"
    p.bev(mat, -0.2, 0.2, -0.22, 0.2, 0.0, 0.12, 0.01)
    p.hexa(mat, [(-0.18, -0.15, 0.12), (0.18, -0.15, 0.12), (0.18, 0.18, 0.12), (-0.18, 0.18, 0.12)],
           [(-0.18, -0.15, 0.32), (0.18, -0.15, 0.32), (0.18, -0.02, 0.2), (-0.18, -0.02, 0.2)])
    p.card("typewriter_keys", (0.16, -0.02, 0.205), (-0.16, -0.02, 0.205), (-0.16, -0.14, 0.316), (0.16, -0.14, 0.316))
    p.box(mat, -0.1, 0.1, -0.17, -0.12, 0.32, 0.42)
    p.card_front("cash_display", -0.08, 0.08, 0.34, 0.41, -0.172)
    p.card_back("cash_display", -0.08, 0.08, 0.34, 0.41, -0.118)
    p.rod(LAQ, (0.21, 0.0, 0.18), (0.27, 0.0, 0.24), 0.02, 3)
    return p.done(top=0.42)


def kitchen_scales(B, x, y, z=0.0, yaw=0.0, solid=False):
    p = _P(B, x, y, z, yaw, "kitchen_scales")
    p.kind = "prop"
    p.bev("CT_SteelWhite", -0.15, 0.15, -0.1, 0.1, 0.0, 0.1, 0.015, 0.02)
    p.cyl(ALU, -0.06, 0.0, 0.1, 0.12, 0.08, 8, r1=0.1)
    p.box("CT_SteelWhite", 0.05, 0.14, -0.04, 0.04, 0.1, 0.32)
    p.card_front("clock_2", 0.06, 0.13, 0.22, 0.29, -0.042)
    return p.done(top=0.12)


def crate(B, x, y, z=0.0, yaw=0.0, w=0.55, d=0.38, h=0.3, fill="CT_PlasterRed", seed=0, solid=False):
    p = _P(B, x, y, z, yaw, "crate")
    p.kind = "prop"
    _crate(p, 0.0, 0.0, 0.0, w, d, h, WOODL, fill, _rng(seed, x, y, "crate"))
    return p.done(top=h)


def sack(B, x, y, z=0.0, yaw=0.0, mat="CT_Paper", solid=False):
    p = _P(B, x, y, z, yaw, "sack")
    p.kind = "prop"
    rings = [_ring_rect(-0.2, 0.2, -0.13, 0.13, 0.0, 0.05), _ring_rect(-0.22, 0.22, -0.16, 0.16, 0.2, 0.06),
             _ring_rect(-0.17, 0.17, -0.12, 0.12, 0.5, 0.05), _ring_rect(-0.06, 0.06, -0.04, 0.04, 0.62, 0.02)]
    p.loft(mat, rings, top=True)
    return p.done(top=0.62)


def cardboard_box(B, x, y, z=0.0, yaw=0.0, w=0.5, d=0.4, h=0.35, seed=0, solid=False):
    p = _P(B, x, y, z, yaw, "cardboard_box")
    p.kind = "prop"
    p.box("CT_PlasterYellow", -w / 2, w / 2, -d / 2, d / 2, 0.0, h)
    p.card_front(cell("label", _rng(seed, x, y, "box")), -w * 0.3, w * 0.3, h * 0.35, h * 0.8, -d / 2 - 0.002)
    p.box("CT_Paper", -w / 2 - 0.002, w / 2 + 0.002, -0.025, 0.025, h, h + 0.002, f="Z")
    return p.done(top=h)


def tools(B, x, y, z=0.0, yaw=0.0, solid=False):
    """[prop] A toolbox with a hammer and a saw beside it."""
    p = _P(B, x, y, z, yaw, "tools")
    p.kind = "prop"
    p.bev("CT_SteelRed", -0.25, 0.25, -0.1, 0.1, 0.0, 0.18, 0.01)
    p.rod(LAQ, (-0.1, 0.0, 0.22), (0.1, 0.0, 0.22), 0.02, 3)
    p.rod(LAQ, (-0.1, 0.0, 0.18), (-0.1, 0.0, 0.22), 0.015, 3)
    p.rod(LAQ, (0.1, 0.0, 0.18), (0.1, 0.0, 0.22), 0.015, 3)
    p.rod(WOOD, (0.3, -0.1, 0.012), (0.3, 0.18, 0.012), 0.024, 4)
    p.box(STEEL, 0.25, 0.35, 0.16, 0.19, 0.0, 0.03)
    p.slab(ALU, (-0.35, -0.12, 0.002), (-0.3, -0.12, 0.002), (-0.3, 0.25, 0.002), (-0.35, 0.25, 0.002), 0.002)
    return p.done(top=0.22)


def chess_set(B, x, y, z=0.0, yaw=0.0, board=0.5, seed=0, solid=False):
    """[prop] Chess pieces mid-game on a board of `board` m (put chess_board under it or use chess_table)."""
    p = _P(B, x, y, z, yaw, "chess_set")
    p.kind = "prop"
    rnd = _rng(seed, x, y, "chess")
    s = board * 0.88 / 8
    taken = set()
    for k in range(14):
        i, j = rnd.randrange(8), rnd.randrange(8)
        if (i, j) in taken:
            continue
        taken.add((i, j))
        u, v = -board * 0.44 + (i + 0.5) * s, -board * 0.44 + (j + 0.5) * s
        mat = "CT_TileWhite" if j < 4 else LAQ
        hh = 0.035 if k % 3 else 0.06
        p.lathe(mat, u, v, [(s * 0.3, 0.003), (s * 0.16, hh * 0.7), (0.0, hh + 0.01)], 4)
    return p.done(top=0.07)


def paint_jars(B, x, y, z=0.0, yaw=0.0, n=5, seed=0, solid=False):
    p = _P(B, x, y, z, yaw, "paint_jars")
    p.kind = "prop"
    rnd = _rng(seed, x, y, "jars")
    for k in range(n):
        u = -0.05 * (n - 1) + k * 0.1
        p.cyl("CT_Glass", u, 0.0, 0.0, 0.08, 0.035, 6)
        p.cyl(rnd.choice(PAINTS), u, 0.0, 0.08, 0.09, 0.037, 6)
    p.rod(WOOD, (0.0, 0.08, 0.005), (0.22, 0.12, 0.005), 0.008, 3)
    return p.done(top=0.09)


# ================================================================================================ LAYOUT HELPERS
def row(fn, B, x0, y0, x1, y1, n, yaw=0.0, z=0.0, **kw):
    """n pieces evenly from (x0, y0) to (x1, y1) inclusive."""
    out = []
    for k in range(n):
        t = k / (n - 1) if n > 1 else 0.5
        out.append(fn(B, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, z, yaw, **kw))
    return out


def grid(fn, B, x0, y0, nx, ny, dx, dy, yaw=0.0, z=0.0, **kw):
    """nx x ny pieces from (x0, y0) with steps dx, dy (building axes)."""
    return [fn(B, x0 + i * dx, y0 + j * dy, z, yaw, **kw) for j in range(ny) for i in range(nx)]


def around_table(B, x, y, table_w, table_d, yaw=0.0, z=0.0, chair_fn=None, per_side=(1, 1, 0, 0), gap=0.12, **kw):
    """Chairs facing a table centred at (x, y) (table rotated by yaw): per_side = (front -y, back +y, left -x, right +x)
    chairs on each side. Returns the chair dicts."""
    chair_fn = chair_fn or chair
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    out = []
    sides = [((0, -1), table_w, 180.0), ((0, 1), table_w, 0.0), ((-1, 0), table_d, 90.0), ((1, 0), table_d, 270.0)]
    for (dirn, L, face), n in zip(sides, per_side):
        for k in range(n):
            t = (k + 0.5) / n - 0.5
            if dirn[1]:
                u, v = t * L, dirn[1] * (table_d / 2 + gap + 0.15)
            else:
                u, v = dirn[0] * (table_w / 2 + gap + 0.15), t * L
            # a chair at local +v faces the table, i.e. toward -v: kit yaw 0 faces -y, so +v side faces 0, -v side 180
            fy = (yaw + {180.0: 180.0, 0.0: 0.0, 90.0: 90.0, 270.0: 270.0}[face]) % 360
            if dirn[1] < 0:
                fy = (yaw + 180.0) % 360
            elif dirn[1] > 0:
                fy = yaw % 360
            elif dirn[0] < 0:
                fy = (yaw + 90.0) % 360
            else:
                fy = (yaw + 270.0) % 360
            out.append(chair_fn(B, x + u * c - v * s, y + u * s + v * c, z, fy, **kw))
    return out


def _rect_poly(x0, y0, x1, y1):
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def _overlap(a, b, margin=0.0):
    """Separating-axis test of two convex polygons (2D); margin > 0 shrinks both."""
    for poly in (a, b):
        n = len(poly)
        for i in range(n):
            ex, ey = poly[(i + 1) % n][0] - poly[i][0], poly[(i + 1) % n][1] - poly[i][1]
            ax, ay = -ey, ex
            L = math.hypot(ax, ay) or 1.0
            ax, ay = ax / L, ay / L
            pa = [px * ax + py * ay for px, py in a]
            pb = [px * ax + py * ay for px, py in b]
            if max(pa) - margin <= min(pb) + margin or max(pb) - margin <= min(pa) + margin:
                return False
    return True


def _inside(pt, poly):
    x, y = pt
    c = False
    n = len(poly)
    for i in range(n):
        (ax, ay), (bx, by) = poly[i], poly[(i + 1) % n]
        if (ay > y) != (by > y) and x < ax + (y - ay) * (bx - ax) / (by - ay):
            c = not c
    return c


class Room:
    """A room's floor (a rectangle (x0, y0, x1, y1) or a CCW polygon) for laying furniture out deterministically.
        R = kit.Room(B, (0, 2, 12, 9), z=0.0, seed=3)
        R.keep_door((6, 2), 0)            # keep 1.4 x 1.6 m clear inside a door at (6, 2) facing yaw 0 (outward -y)
        R.keep(4, 2, 8, 9)                # keep a walkway clear
        R.place(kit.bookshelf, 0.0, 5.0, 90, w=1.0)      # anything; its footprint is remembered
        R.along("n", [kit.bookshelf] * 4, w=0.9, gap=0.1)  # pieces in a row against a wall side (n = +y wall)
        R.scatter([kit.plant], 3)         # up to 3 pieces at free seeded spots
    The walls are the polygon's edges; along() puts wall pieces with their backs on the wall, facing into the room."""

    def __init__(self, B, area, z=0.0, seed=0, margin=0.1):
        self.B, self.z = B, z
        if len(area) == 4 and not isinstance(area[0], (tuple, list)):
            x0, y0, x1, y1 = area
            self.poly = _rect_poly(x0, y0, x1, y1)
        else:
            self.poly = [tuple(p) for p in area]
        self.rnd = random.Random("room|%s|%s" % (seed, self.poly[0]))
        self.kept = []
        self.used = []
        self.margin = margin
        xs = [p[0] for p in self.poly]
        ys = [p[1] for p in self.poly]
        self.box = (min(xs), min(ys), max(xs), max(ys))

    def keep(self, x0, y0, x1, y1):
        self.kept.append(_rect_poly(min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1)))

    def keep_door(self, pos, yaw, w=1.4, d=1.6):
        """Clear zone inside a doorway: pos on the wall, yaw = the door's OUTWARD direction (B.entrance convention)."""
        a = math.radians(yaw)
        ox, oy = math.sin(a), -math.cos(a)              # outward
        px, py = -oy, ox
        x, y = pos[0], pos[1]
        pts = [(x + px * s * w / 2 - ox * t, y + py * s * w / 2 - oy * t) for s, t in ((-1, 0), (1, 0), (1, d), (-1, d))]
        self.kept.append(pts if sum(pts[i][0] * pts[(i + 1) % 4][1] - pts[(i + 1) % 4][0] * pts[i][1] for i in range(4)) > 0 else pts[::-1])

    def free(self, foot):
        if not all(_inside(p, self.poly) or any(math.dist(p, q) < 0.05 for q in self.poly) for p in foot):
            return False
        return not any(_overlap(foot, k, 0.01) for k in self.kept + self.used)

    def place(self, fn, x, y, yaw=0.0, force=True, **kw):
        r = fn(self.B, x, y, self.z, yaw, **kw)
        if r.get("foot"):
            self.used.append(r["foot"])
        return r

    def along(self, side, fns, start=None, gap=0.1, offset=0.0, **kw):
        """Wall pieces along side n (+y wall), s (-y), e (+x), w (-x) of the room's bounding box, from `start`
        (default the wall's start) with `gap` between them; each faces into the room. Pieces that would land on a
        kept zone are skipped (their slot stays empty). fns: functions or (fn, kwargs) pairs; kw: common options
        (w = width hint used for spacing; default 1.0)."""
        x0, y0, x1, y1 = self.box
        yaw = {"n": 0.0, "s": 180.0, "e": 270.0, "w": 90.0}[side]
        out = []
        pos = start if start is not None else ((x0 if side in "ns" else y0) + 0.05)
        for f in fns:
            fn, extra = (f if isinstance(f, tuple) else (f, {}))
            opts = dict(kw)
            opts.update(extra)
            wd = opts.get("w", 1.0)
            c = pos + wd / 2
            if side == "n":
                x, y = c, y1 - offset
            elif side == "s":
                x, y = c, y0 + offset
            elif side == "e":
                x, y = x1 - offset, c
            else:
                x, y = x0 + offset, c
            probe = _P(Building_stub(), x, y, 0.0, yaw, "probe")
            fp = [probe.w(u, v)[:2] for u, v in ((-wd / 2, -0.6), (wd / 2, -0.6), (wd / 2, -0.02), (-wd / 2, -0.02))]
            if any(_overlap(fp, k, 0.01) for k in self.kept):
                pos += wd + gap
                continue
            r = fn(self.B, x, y, self.z, yaw, **opts)
            self.used.append(r["foot"])
            out.append(r)
            pos += wd + gap
        return out

    def scatter(self, fns, n, tries=60, wall=False, **kw):
        """Up to n pieces at seeded free spots (random yaw in 90 degree steps). Returns the placed dicts."""
        out = []
        x0, y0, x1, y1 = self.box
        for k in range(n):
            fn = fns[k % len(fns)]
            for _ in range(tries):
                x, y = self.rnd.uniform(x0 + 0.5, x1 - 0.5), self.rnd.uniform(y0 + 0.5, y1 - 0.5)
                yaw = self.rnd.choice((0.0, 90.0, 180.0, 270.0))
                tmp = Building_stub()
                r = fn(tmp, x, y, self.z, yaw, **kw)
                if self.free(r["foot"]):
                    r = fn(self.B, x, y, self.z, yaw, **kw)
                    self.used.append(r["foot"])
                    out.append(r)
                    break
        return out


class Building_stub:
    """Throw-away Building for probing a piece's footprint (nothing reaches the real building)."""

    def __init__(self):
        self.m, self.detail = Mesh(), Mesh()
        self.lamps = []

    def lamp(self, *a, **k):
        pass


def check(B, door_clear=(1.2, 1.4)):
    """Warnings about the kit's pieces in B: overlapping floor pieces, pieces in an entrance's clear zone
    (door_clear = width, depth inside the door), spots standing inside a solid piece. Returns a list of strings."""
    items = [it for it in getattr(B, "kit_items", []) if it["kind"] in ("floor", "wall")]
    out = []
    for i in range(len(items)):
        for j in range(i + 1, len(items)):
            a, b = items[i], items[j]
            if a["z1"] <= b["z0"] + 0.02 or b["z1"] <= a["z0"] + 0.02:
                continue
            if _overlap(a["foot"], b["foot"], 0.03):
                out.append("overlap: %s at %s and %s at %s" % (a["name"], _c(a["foot"]), b["name"], _c(b["foot"])))
    w, d = door_clear
    for e in B.entrances:
        a = math.radians(e["yaw"])
        ox, oy = math.sin(a), -math.cos(a)
        px, py = -oy, ox
        x, y = e["pos"][0], e["pos"][1]
        zone = [(x + px * s * w / 2 - ox * t, y + py * s * w / 2 - oy * t) for s, t in ((-1, 0.3), (1, 0.3), (1, d + 0.3), (-1, d + 0.3))]
        if sum(zone[i][0] * zone[(i + 1) % 4][1] - zone[(i + 1) % 4][0] * zone[i][1] for i in range(4)) < 0:
            zone = zone[::-1]
        for it in items:
            if it["z0"] > e["pos"][2] + 2.0 or it["z1"] < e["pos"][2]:
                continue
            if _overlap(it["foot"], zone, 0.0):
                out.append("blocks entrance %s: %s at %s" % (e["id"], it["name"], _c(it["foot"])))
    for s in B.spots:
        for it in items:
            if it["solid"] and _inside(s["pos"][:2], it["foot"]) and abs(s["pos"][2] - it["z0"]) < 1.0:
                out.append("spot %s inside solid %s" % (s["id"], it["name"]))
    return out


def _c(foot):
    return "(%.1f, %.1f)" % (sum(p[0] for p in foot) / len(foot), sum(p[1] for p in foot) / len(foot))


# every placeable piece by name (kit_showroom.py walks this)
PIECES = {f.__name__: f for f in (
    chair, stool, bar_stool, armchair, sofa, bench, cinema_row, waiting_row, bean_bag, zabuton,
    table, table_round, reading_table, coffee_table, school_desk, office_desk, drafting_table, chabudai, restaurant_table,
    chess_table, pingpong_table, billiard_table,
    bookshelf, magazine_rack, card_catalogue, wardrobe, filing_cabinet, lockers, display_case, shop_shelf, clothes_rail,
    shoe_rack, coat_rack, safe,
    shop_counter, bar_counter, reception_desk, ticket_booth, lending_desk, pharmacy_counter, market_stall,
    bed, bunk_bed, futon, bedside_table, kitchen_unit, samovar, tv_set, radio, floor_lamp, table_lamp, wall_clock, mirror,
    rug, curtains, radiator, plant, aquarium, hat_stand,
    piano, grand_piano, stage, cinema_screen, projector, easel, potters_wheel, sewing_table, loom, globe, telescope,
    record_player, jukebox, arcade, photo_booth, table_football, dart_board, slide_projector,
    wall_bars, barbell_rack, bench_press, punching_bag, vaulting_horse, mats, basketball_hoop, exercise_bike, medical_couch,
    eye_chart, scales, screen_divider, dentist_chair,
    blackboard, teacher_desk, wall_map, skeleton, lab_bench,
    picture, poster, notice_board, banner, mosaic_panel, wall_shelf, sign_plate, menu_board, fire_extinguisher, wall_phone,
    pendant, ceiling_fan,
    books_stack, newspaper, cup, glass, teapot, plates, bottles, vase, typewriter, telephone, cash_register,
    kitchen_scales, crate, sack, cardboard_box, tools, chess_set, paint_jars, lamp_green)}

BUDGET = {"chair": 80, "stool": 80, "bar_stool": 80, "table": 60, "table_round": 60, "bookshelf": 400, "piano": 500,
          "grand_piano": 500}
