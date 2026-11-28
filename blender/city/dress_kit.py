"""Props for the swappable room dressing (B.dress, docs/city_dress.md): the small things that come and go with the days.

Pure Python on top of interior_kit's piece builder (same conventions: x, y on the floor or a surface, z = where it stands,
yaw 0 = the piece faces -y; [wall] pieces: (x, y) on the wall face, the piece stands out toward its facing). Every piece
here is solid=False (dressing never has collision) and returns the kit's dict (pos, top, size, foot...).

    import dress_kit as dk
    with B.dress("lobby_umbrellas", weather=("rain", "wet")):
        dk.umbrella_open(B, 1.0, 2.0, 0.0, 30, seed=1)
        dk.umbrella_stand(B, 2.0, 0.3, 0.0, 0, n=3, seed=2)

CATALOGUE
  umbrella_open     d1.0 x0.75 open umbrella drying on the floor, canopy tilted, handle up; mat = canopy
  umbrella_closed   0.1x0.1x0.9 [wall] a furled umbrella leaning against the wall
  umbrella_stand    d0.26 x0.5 a ceramic stand with n furled umbrellas (n 0..4)
  wall_hooks        w x0.3x1.9 [wall] a coat board: hooks, a hat shelf; coats (0..1 share of hooks), scarves, hats;
                    season "winter" (long coats, fur hats, scarves) | "autumn" (raincoats, caps) | "summer" (a light
                    jacket, a straw hat, a bag)
  coats_on_rail     the coats alone for a kit.coat_rack(... coats=0) at the same x, y, yaw, w (the cloakroom's hooks
                    at the kit's 0.24 m pitch): share 0..1 of the hooks taken, season winter | autumn | summer
  boots             0.32x0.3 a pair by a door: kind felt (valenki) | rubber (wellingtons) | galoshes | sandals | shoes
  doormat_wet       0.8x0.5 a rubber mat with a dark wet patch (rain days by the doors)
  jigsaw            0.62x0.46 [prop] a jigsaw on a table: the board, the picture done up to stage/stages, loose pieces,
                    the box lid with the picture; stage 0 = only the frame and the heap
  vase_wilted       [prop] the kit's vase with the flowers hanging and a few fallen petals round it
  plate_crumbs      [prop] a used plate: crumbs, a fork and a knife, a crumpled napkin
  book_face_down    [prop] an open book left face down (a tent)
  sledge            0.4x0.95x0.3 [wall] a child's wooden sledge leaning against the wall (winter lobbies)
  towel_hat         [prop] a folded striped towel and a straw hat on a chair or a bench (summer)
  school_bag        0.3x0.12x0.35 a satchel on the floor or hung on a desk hook (z = its bottom)
  mitten_pair       [prop] a pair of mittens drying on a radiator or a ledge
  chess_left        0.42x0.42 [prop] a game left mid-way on a table's board: the pieces in one of six real middle-game
                    positions (seed), two or three taken pieces stood beside the board; board=False when the table has its own
  dominoes_left     [prop] a game of dominoes left on a table: the snake of played tiles, each player's few tiles stood on
                    edge at their places (players 2..4), the face-down heap
  tea_in_holder     [prop] a tea glass in a metal holder (podstakannik) with the spoon left in it, a slice of lemon
  spectacles        [prop] reading glasses left folded on a table or a book
  exercise_books    [prop] two exercise books left open, a pencil case and a pencil (homework, a pupil's desk)
  class_register    [prop] the class register (a thick bound book), a duster and a stub of chalk on the teacher's desk
  ticket_stubs      [prop] torn cinema ticket stubs and a sweet wrapper on a seat or the floor after a show
  sheet_music       [prop] a book of music open on a stand or a lid, a loose sheet beside it
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import interior_kit as kit  # noqa: E402
from interior_kit import _P, _rng, _tilt, cell  # noqa: E402

CANOPIES = ["CT_PlasterRed", "CT_PlasterBlue", "CT_ConcreteDark", "CT_PlasterYellow", "CT_PlasterGreen", "CT_Leather",
            "CT_PlasterPink"]
COATS_WINTER = ["CT_ConcreteDark", "CT_Leather", "CT_Wood", "CT_SteelRed", "CT_Rubber", "CT_Leather", "CT_PlasterBlue"]
COATS_AUTUMN = ["CT_Leather", "CT_PlasterYellow", "CT_Wood", "CT_ConcreteDark", "CT_PlasterGreen", "CT_SteelRed"]


def _rot_since(p, n0, ax, deg, pivot):
    """Rotate the vertices added since n0 about the local axis 'x' or 'y' through pivot (3 floats)."""
    a = math.radians(deg)
    ca, sa = math.cos(a), math.sin(a)
    px, py, pz = pivot
    out = []
    for (x, y, z) in p.m.verts[n0:]:
        x, y, z = x - px, y - py, z - pz
        if ax == "x":
            y, z = y * ca - z * sa, y * sa + z * ca
        else:
            x, z = x * ca + z * sa, -x * sa + z * ca
        out.append((x + px, y + py, z + pz))
    p.m.verts[n0:] = out


def _canopy(p, mat, r, h, ribs=8, mat2=None):
    """An umbrella canopy round +z: apex at (0, 0, h), rim at z 0 (slightly scalloped), ribs as thin rods, both sides."""
    rim = []
    for k in range(ribs):
        a = 2 * math.pi * k / ribs
        rim.append((r * math.cos(a), r * math.sin(a), 0.0))
    apex = (0.0, 0.0, h)
    for k in range(ribs):
        a0, a1 = rim[k], rim[(k + 1) % ribs]
        mid_a = 2 * math.pi * (k + 0.5) / ribs
        sag = (r * 0.93 * math.cos(mid_a), r * 0.93 * math.sin(mid_a), h * 0.08)     # the cloth sags between the ribs
        m = mat2 if (mat2 and k % 2) else mat
        p.face(m, [a0, a1, apex])
        p.face(m, [apex, a1, a0])                                                     # the underside
        p.face(m, [a0, sag, a1])
        p.face(m, [a1, sag, a0])
        p.rod("CT_Steel", (a0[0] * 0.98, a0[1] * 0.98, 0.004), (0, 0, h - 0.01), 0.006, 3)
    p.cyl("CT_Steel", 0, 0, h - 0.005, h + 0.035, 0.008, 5)                          # the ferrule on top


def umbrella_open(B, x, y, z=0.0, yaw=0.0, mat=None, seed=0, solid=False):
    """An open umbrella left to dry on the floor: the canopy rests on its rim, tipped over, the handle sticking up and out."""
    p = _P(B, x, y, z, yaw, "umbrella_open")
    rnd = _rng(seed, x, y, "umbrella")
    mt = mat or rnd.choice(CANOPIES)
    mt2 = rnd.choice([None, None, "CT_TileWhite"]) if mt != "CT_ConcreteDark" else None
    n0 = len(p.m.verts)
    r, h = 0.48, 0.26
    _canopy(p, mt, r, h, 8, mt2)
    # the shaft from the apex down through the middle and out under the rim, the crook handle at its end
    p.rod("CT_Steel", (0, 0, h), (0, 0, -0.55), 0.012, 5)
    p.tube("CT_Wood", [(0, 0, -0.55), (0, 0, -0.66), (0.03, 0, -0.71), (0.07, 0, -0.7), (0.085, 0, -0.65)], 0.022, 5)
    # tip it over: it lies on its rim with the shaft pointing up and away (about 65 degrees)
    _rot_since(p, n0, "x", 115.0, (0.0, 0.0, 0.0))
    # after the turn the lowest rim point must sit on the floor
    zmin = min(v[2] for v in p.m.verts[n0:])
    p.m.verts[n0:] = [(vx, vy, vz - zmin) for (vx, vy, vz) in p.m.verts[n0:]]
    return p.done(top=None)


def umbrella_closed(B, x, y, z=0.0, yaw=0.0, mat=None, seed=0, solid=False):
    """[wall] A furled umbrella leaning back against the wall (its tip on the floor 0.22 m out)."""
    p = _P(B, x, y, z, yaw, "umbrella_closed")
    p.kind = "wall"
    rnd = _rng(seed, x, y, "umbrella_c")
    mt = mat or rnd.choice(CANOPIES)
    n0 = len(p.m.verts)
    _furled(p, mt, 0.0, 0.0, 0.0)
    lean = math.degrees(math.atan2(0.2, 0.9))
    _rot_since(p, n0, "x", -lean, (0.0, 0.0, 0.0))
    p.m.verts[n0:] = [(vx, vy - 0.22, vz) for (vx, vy, vz) in p.m.verts[n0:]]
    return p.done()


def _furled(p, mt, cx, cy, z0, L=0.92):
    """A furled umbrella standing on its tip at (cx, cy, z0): the tip, the wrapped cloth (a long 8-sided spindle with a
    strap), the shaft and the crook."""
    p.cyl("CT_Steel", cx, cy, z0, z0 + 0.08, 0.004, 4, r1=0.006)
    p.lathe(mt, cx, cy, [(0.006, z0 + 0.08), (0.03, z0 + 0.25), (0.036, z0 + 0.5), (0.03, z0 + 0.62), (0.008, z0 + 0.66)], 8,
            top=True)
    p.cyl(mt, cx, cy, z0 + 0.38, z0 + 0.41, 0.038, 8)                                  # the strap round the folds
    p.rod("CT_Steel", (cx, cy, z0 + 0.66), (cx, cy, z0 + L - 0.1), 0.012, 5)
    p.tube("CT_Wood", [(cx, cy, z0 + L - 0.1), (cx, cy, z0 + L), (cx + 0.035, cy, z0 + L + 0.045),
                       (cx + 0.075, cy, z0 + L + 0.02), (cx + 0.08, cy, z0 + L - 0.03)], 0.022, 5)


def umbrella_stand(B, x, y, z=0.0, yaw=0.0, n=3, mat="CT_TileWhite", seed=0, solid=False):
    """A ceramic umbrella stand with n furled umbrellas leaning out of it (n 0 = empty)."""
    p = _P(B, x, y, z, yaw, "umbrella_stand")
    rnd = _rng(seed, x, y, "ustand")
    p.lathe(mat, 0, 0, [(0.11, 0.0), (0.13, 0.03), (0.13, 0.46), (0.12, 0.5)], 10, top=False, a0=math.pi / 10)
    p.lathe(mat, 0, 0, [(0.115, 0.5), (0.105, 0.47), (0.105, 0.05)], 10, top=False, a0=math.pi / 10)    # inner wall
    p.cyl("CT_ConcreteDark", 0, 0, 0.05, 0.052, 0.104, 10)                                               # the bottom
    for k in range(max(0, min(4, n))):
        n0 = len(p.m.verts)
        a = 2 * math.pi * k / max(1, n) + rnd.uniform(-0.3, 0.3)
        _furled(p, rnd.choice(CANOPIES), 0.0, 0.0, 0.05, L=rnd.uniform(0.82, 0.95))
        _rot_since(p, n0, "y", rnd.uniform(7, 13), (0.0, 0.0, 0.05))
        # turn the leaning umbrella about z by a
        ca, sa = math.cos(a), math.sin(a)
        p.m.verts[n0:] = [(vx * ca - vy * sa + 0.035 * ca, vx * sa + vy * ca + 0.035 * sa, vz) for (vx, vy, vz) in p.m.verts[n0:]]
    return p.done(top=0.5, pin=(-0.13, 0.13, -0.13, 0.13, 0.0, 1.0 if n else 0.5))


def _coat(p, mat, xa, L, puffy=False, hood=False):
    """A coat hanging from a hook at (xa, -0.06, 1.66): a body lofted through four rings (narrow rounded shoulders, a
    chest, a waist and a wider hem), a collar round the hook, sleeves hanging at the sides, buttons down the front."""
    top, hem = 1.62, 1.62 - L
    f = 1.15 if puffy else 1.0

    def ring(z, w, d0, d1):
        # an 8-point section: the front (-y) flatter, the back rounder
        return [(xa - w, d0 * 0.55, z), (xa - w * 0.7, d0, z), (xa + w * 0.7, d0, z), (xa + w, d0 * 0.55, z),
                (xa + w, d1 * 0.5, z), (xa + w * 0.6, d1, z), (xa - w * 0.6, d1, z), (xa - w, d1 * 0.5, z)]
    rings = [ring(hem, 0.23 * f, -0.2 * f, 0.05), ring(hem + L * 0.45, 0.19 * f, -0.17 * f, 0.04),
             ring(top - 0.2, 0.2 * f, -0.16 * f, 0.03), ring(top - 0.05, 0.15 * f, -0.12 * f, 0.0),
             ring(top, 0.07, -0.08, -0.02)]
    rings = [list(reversed(r_)) for r_ in rings]                       # CCW seen from above
    p.loft(mat, rings, top=True, bottom=True)
    p.lathe(mat, xa, -0.08, [(0.06, top - 0.03), (0.075, top + 0.01), (0.04, top + 0.05)], 6)    # collar round the hook
    for s in (-1, 1):                                                                         # the sleeves
        p.rod(mat, (xa + s * 0.17 * f, -0.06, top - 0.07), (xa + s * 0.2 * f, -0.08, top - 0.62), 0.085 * f, 6)
        p.cyl("CT_ConcreteDark" if mat != "CT_ConcreteDark" else "CT_PlasterGrey", xa + s * 0.2 * f, -0.08, top - 0.66,
              top - 0.62, 0.044 * f, 6)                                                      # the cuff
    bm = "CT_ConcreteDark" if puffy else "CT_Brass"
    yb = -0.16 * f - 0.006
    for k in range(4 if L > 0.8 else 3):                                                     # buttons down the front
        p.box(bm, xa - 0.012, xa + 0.012, yb - 0.008, yb + 0.004, top - 0.26 - k * 0.17, top - 0.236 - k * 0.17)
    if hood:
        p.lathe(mat, xa, 0.02, [(0.0, top - 0.05), (0.1, top - 0.02), (0.1, top + 0.08), (0.0, top + 0.13)], 6)


def wall_hooks(B, x, y, z=0.0, yaw=0.0, w=1.4, coats=0.75, season="winter", seed=0, solid=False):
    """[wall] A coat board with hooks and a hat shelf over it, dressed for the season (see the catalogue)."""
    p = _P(B, x, y, z, yaw, "wall_hooks")
    p.kind = "wall"
    rnd = _rng(seed, x, y, "hooks_" + season)
    hw = w / 2
    p.bev("CT_Wood", -hw, hw, -0.025, 0.0, 1.62, 1.76, 0.008)                                     # the hook board
    p.bev("CT_Wood", -hw - 0.05, hw + 0.05, -0.27, 0.0, 1.86, 1.885, 0.006)                       # the hat shelf
    for s in (-1, 1):
        p.hexa("CT_Wood", [(s * (hw - 0.02) - 0.012, -0.22, 1.86), (s * (hw - 0.02) + 0.012, -0.22, 1.86),
                           (s * (hw - 0.02) + 0.012, 0.0, 1.86), (s * (hw - 0.02) - 0.012, 0.0, 1.86)],
               [(s * (hw - 0.02) - 0.012, -0.02, 1.74), (s * (hw - 0.02) + 0.012, -0.02, 1.74),
                (s * (hw - 0.02) + 0.012, 0.0, 1.74), (s * (hw - 0.02) - 0.012, 0.0, 1.74)])
    pal = COATS_WINTER if season == "winter" else COATS_AUTUMN
    xa = -hw + 0.16
    k = 0
    while xa < hw - 0.12:
        p.rod("CT_Brass", (xa, -0.02, 1.69), (xa, -0.08, 1.66), 0.012, 4)
        p.rod("CT_Brass", (xa, -0.08, 1.66), (xa, -0.09, 1.69), 0.01, 4)
        if rnd.random() < coats:
            if season == "summer":
                if k % 2 == 0:
                    _coat(p, rnd.choice(["CT_PlasterYellow", "CT_TileWhite", "CT_PlasterBlue"]), xa, 0.62)
                else:                                                           # a string bag hanging off the hook
                    p.rod("CT_Leather", (xa, -0.08, 1.66), (xa, -0.1, 1.3), 0.012, 3)
                    p.lathe("CT_PlasterGreen", xa, -0.1, [(0.02, 1.0), (0.13, 1.08), (0.12, 1.26), (0.04, 1.3)], 6)
            elif season == "winter":
                _coat(p, rnd.choice(pal), xa, rnd.uniform(0.85, 1.1), puffy=rnd.random() < 0.5, hood=rnd.random() < 0.3)
                if rnd.random() < 0.6:                                          # a scarf over the shoulders
                    sc = rnd.choice(["CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterGreen", "CT_TileWhite"])
                    p.box(sc, xa - 0.06, xa + 0.06, -0.25, -0.215, 1.15, 1.6)
            else:
                _coat(p, rnd.choice(pal), xa, rnd.uniform(0.8, 1.0), hood=rnd.random() < 0.5)
        k += 1
        xa += 0.3
    # hats on the shelf
    hx = -hw + 0.2
    while hx < hw - 0.15:
        if rnd.random() < 0.65:
            if season == "winter":                                              # an ushanka / fur hat
                hm = rnd.choice(["CT_Wood", "CT_ConcreteDark", "CT_PlasterGrey"])
                p.lathe(hm, hx, -0.13, [(0.1, 1.885), (0.11, 1.93), (0.095, 1.99), (0.0, 2.02)], 8)
                for s in (-1, 1):
                    p.box(hm, hx + s * 0.1 - 0.015, hx + s * 0.1 + 0.015, -0.2, -0.06, 1.89, 1.97)
            elif season == "summer":                                            # a straw hat
                p.cyl("CT_WoodLight", hx, -0.13, 1.885, 1.89, 0.17, 10)
                p.lathe("CT_WoodLight", hx, -0.13, [(0.09, 1.89), (0.085, 1.96), (0.0, 1.975)], 10)
                p.cyl("CT_PlasterRed", hx, -0.13, 1.892, 1.912, 0.091, 10, top=False)
            else:                                                               # a cap
                hm = rnd.choice(["CT_PlasterGrey", "CT_PlasterBlue", "CT_Leather"])
                p.lathe(hm, hx, -0.12, [(0.095, 1.885), (0.09, 1.93), (0.0, 1.96)], 8)
                p.hexa(hm, [(hx - 0.08, -0.32, 1.886), (hx + 0.08, -0.32, 1.886), (hx + 0.08, -0.2, 1.886), (hx - 0.08, -0.2, 1.886)],
                       [(hx - 0.08, -0.32, 1.895), (hx + 0.08, -0.32, 1.895), (hx + 0.08, -0.2, 1.9), (hx - 0.08, -0.2, 1.9)])
        hx += 0.32
    return p.done(front=0.7)


def coats_on_rail(B, x, y, z=0.0, yaw=0.0, w=2.4, share=0.8, season="winter", seed=0, solid=False):
    """The coats of a cloakroom rail (kit.coat_rack placed with coats=0 at the same x, y, z, yaw, w): one coat on `share`
    of its hooks, the hook pitch and height of the kit (first hook at -w/2 + 0.12, every 0.24 m, hanging from z 1.65)."""
    p = _P(B, x, y, z, yaw, "coats_on_rail")
    rnd = _rng(seed, x, y, "rail_" + season)
    hw = w / 2
    pal = COATS_WINTER if season == "winter" else COATS_AUTUMN if season == "autumn" else \
        ["CT_PlasterYellow", "CT_TileWhite", "CT_PlasterBlue"]
    xa = -hw + 0.12
    while xa < hw - 0.1:
        if rnd.random() < share:
            L = rnd.uniform(0.85, 1.1) if season == "winter" else rnd.uniform(0.6, 0.9)
            _coat(p, rnd.choice(pal), xa, L, puffy=season == "winter" and rnd.random() < 0.5)
        xa += 0.24
    if not p.m.faces:                                   # never an empty set: at least one coat at the first hook
        _coat(p, pal[0], -hw + 0.12, 0.9)
    return p.done()


def boots(B, x, y, z=0.0, yaw=0.0, kind="rubber", mat=None, seed=0, solid=False):
    """A pair by a door (the toes look -y)."""
    p = _P(B, x, y, z, yaw, "boots_" + kind)
    rnd = _rng(seed, x, y, "boots")
    for s in (-1, 1):
        cx = s * 0.075 + rnd.uniform(-0.01, 0.01)
        if kind == "felt":                                                       # valenki: grey felt, rubber galosh soles
            m = mat or rnd.choice(["CT_PlasterGrey", "CT_ConcreteWhite", "CT_Wood"])
            p.bev("CT_Rubber", cx - 0.05, cx + 0.05, -0.14, 0.12, 0.0, 0.03, 0.01)
            p.bev(m, cx - 0.048, cx + 0.048, -0.14, 0.03, 0.03, 0.1, 0.02)
            p.lathe(m, cx, 0.06, [(0.05, 0.02), (0.055, 0.1), (0.05, 0.42), (0.056, 0.45)], 8, top=False)
        elif kind == "rubber":
            m = mat or rnd.choice(["CT_PlasterGreen", "CT_PlasterYellow", "CT_PlasterRed", "CT_ConcreteDark"])
            p.bev("CT_Rubber", cx - 0.045, cx + 0.045, -0.13, 0.11, 0.0, 0.025, 0.008)
            p.bev(m, cx - 0.043, cx + 0.043, -0.13, 0.03, 0.025, 0.1, 0.02)
            p.lathe(m, cx, 0.06, [(0.048, 0.02), (0.05, 0.3), (0.055, 0.34)], 8, top=False)
        elif kind == "galoshes":
            m = mat or "CT_Rubber"
            p.bev(m, cx - 0.045, cx + 0.045, -0.14, 0.12, 0.0, 0.07, 0.02)
            p.box("CT_PlasterRed", cx - 0.035, cx + 0.035, -0.06, 0.1, 0.066, 0.07, f="Z")               # the red lining
        elif kind == "sandals":
            m = mat or rnd.choice(["CT_Leather", "CT_Wood"])
            p.bev(m, cx - 0.04, cx + 0.04, -0.12, 0.12, 0.0, 0.02, 0.006)
            for yy in (-0.07, 0.0):
                p.rod(m, (cx - 0.04, yy, 0.02), (cx + 0.04, yy, 0.02), 0.015, 4)
        else:                                                                    # town shoes
            m = mat or rnd.choice(["CT_Leather", "CT_ConcreteDark", "CT_Wood"])
            p.bev("CT_Rubber", cx - 0.045, cx + 0.045, -0.13, 0.13, 0.0, 0.02, 0.006)
            p.bev(m, cx - 0.043, cx + 0.043, -0.12, 0.12, 0.02, 0.09, 0.025)
    return p.done(top=None)


def doormat_wet(B, x, y, z=0.0, yaw=0.0, w=0.8, d=0.5, solid=False):
    """A ribbed rubber mat inside a door with a dark wet patch trodden into it (rain days)."""
    p = _P(B, x, y, z, yaw, "doormat_wet")
    p.box("CT_Rubber", -w / 2, w / 2, -d / 2, d / 2, 0.0, 0.012, f="xXyYZ")
    for k in range(int(w / 0.06)):
        xa = -w / 2 + 0.03 + k * 0.06
        p.box("CT_ConcreteDark", xa - 0.012, xa + 0.012, -d / 2 + 0.04, d / 2 - 0.04, 0.012, 0.016, f="xXyYZ")
    # the wet patch: a darker, shinier lozenge a hair over the ribs
    pts = [(math.cos(2 * math.pi * k / 10) * w * 0.3, math.sin(2 * math.pi * k / 10) * d * 0.32, 0.0175) for k in range(10)]
    p.face("CT_Glass", pts)
    return p.done(top=0.0175)


def jigsaw(B, x, y, z=0.0, yaw=0.0, stage=3, stages=6, picture="painting_sea", seed=0, solid=False):
    """[prop] A jigsaw on a table: a hardboard board with a frame, the picture done up to stage/stages (filled row by row
    from the bottom, the edge first), the loose pieces in a heap and the box lid with the whole picture."""
    p = _P(B, x, y, z, yaw, "jigsaw")
    p.kind = "prop"
    rnd = _rng(seed, x, y, "jigsaw%d" % stage)
    cn = cell(picture, rnd)
    W, D = 0.5, 0.36
    p.box("CT_Wood", -W / 2 - 0.02, W / 2 + 0.02, -D / 2 - 0.02, D / 2 + 0.02, 0.0, 0.004, f="xXyYZ")       # the board
    frac = max(0.0, min(1.0, stage / float(stages)))
    edge = 0.035
    # the edge is always done first (a frame of picture strips), then rows grow from the bottom (the -y side)
    u0, v0, u1, v1 = kit._uvrect(cn)

    def pic(xa, xb, ya, yb):
        a0, a1 = (xa + W / 2) / W, (xb + W / 2) / W
        b0, b1 = (ya + D / 2) / D, (yb + D / 2) / D
        uv = [(u0 + (u1 - u0) * a0, v0 + (v1 - v0) * b0), (u0 + (u1 - u0) * a1, v0 + (v1 - v0) * b0),
              (u0 + (u1 - u0) * a1, v0 + (v1 - v0) * b1), (u0 + (u1 - u0) * a0, v0 + (v1 - v0) * b1)]
        p.m.face(kit.ATLAS_SLOT, [(xa, ya, 0.007), (xb, ya, 0.007), (xb, yb, 0.007), (xa, yb, 0.007)], uv)
    if stage > 0:
        pic(-W / 2, W / 2, -D / 2, -D / 2 + edge)
        pic(-W / 2, W / 2, D / 2 - edge, D / 2)
        pic(-W / 2, -W / 2 + edge, -D / 2 + edge, D / 2 - edge)
        pic(W / 2 - edge, W / 2, -D / 2 + edge, D / 2 - edge)
        inner = (D - 2 * edge) * max(0.0, (frac * stages - 1) / max(1, stages - 1))
        if inner > 0.005:
            pic(-W / 2 + edge, W / 2 - edge, -D / 2 + edge, -D / 2 + edge + inner)
            # a ragged front: a few single pieces stuck on past the finished rows
            yb = -D / 2 + edge + inner
            for k in range(int(rnd.uniform(3, 7))):
                xa = rnd.uniform(-W / 2 + edge, W / 2 - edge - 0.03)
                if yb + 0.03 < D / 2 - edge:
                    pic(xa, xa + 0.03, yb, yb + 0.03)
    # the heap of loose pieces beside the board (grey backs and coloured faces)
    if stage < stages:
        for k in range(int(26 * (1.0 - frac)) + 4):
            px, py = W / 2 + 0.07 + rnd.uniform(-0.05, 0.05), rnd.uniform(-0.12, 0.12)
            a = rnd.uniform(0, math.pi)
            ca, sa = math.cos(a) * 0.014, math.sin(a) * 0.014
            hz = 0.005 + (k % 3) * 0.003
            q = [(px - ca + sa, py - sa - ca, hz), (px + ca + sa, py + sa - ca, hz), (px + ca - sa, py + sa + ca, hz),
                 (px - ca - sa, py - sa + ca, hz)]
            if k % 2:
                p.face("CT_PlasterGrey", q)
            else:
                ua = u0 + (u1 - u0) * rnd.uniform(0.1, 0.85)
                va = v0 + (v1 - v0) * rnd.uniform(0.1, 0.85)
                du, dv = (u1 - u0) * 0.06, (v1 - v0) * 0.08
                p.m.face(kit.ATLAS_SLOT, q, [(ua, va), (ua + du, va), (ua + du, va + dv), (ua, va + dv)])
    # the box lid, picture up, at the back left
    lx, ly = -W / 2 - 0.02, D / 2 + 0.11
    p.box("CT_PlasterBlue", lx, lx + 0.24, ly - 0.08, ly + 0.08, 0.0, 0.035, f="xXyY")
    p.card(cn, (lx + 0.01, ly - 0.07, 0.036), (lx + 0.23, ly - 0.07, 0.036), (lx + 0.23, ly + 0.07, 0.036), (lx + 0.01, ly + 0.07, 0.036))
    return p.done(top=0.04, cell=cn)


def vase_wilted(B, x, y, z=0.0, yaw=0.0, mat=None, seed=0, solid=False):
    """[prop] A vase whose flowers have gone over: the stems bent, the heads hanging brown, petals fallen round it."""
    p = _P(B, x, y, z, yaw, "vase_wilted")
    p.kind = "prop"
    rnd = _rng(seed, x, y, "vase_w")
    mt = mat or rnd.choice(["CT_PlasterBlue", "CT_TileWhite", "CT_Copper", "CT_PlasterRed"])
    p.lathe(mt, 0, 0, [(0.04, 0.0), (0.082, 0.07), (0.075, 0.14), (0.038, 0.205), (0.05, 0.24)], 8, top=False, a0=math.pi / 8)
    p.cyl("CT_ConcreteDark", 0, 0, 0.21, 0.215, 0.036, 6)
    for k in range(5):
        a = k * 1.3 + rnd.uniform(-0.2, 0.2)
        ca, sa = math.cos(a), math.sin(a)
        knee = (0.05 * ca, 0.05 * sa, 0.33)
        tip = (0.13 * ca, 0.13 * sa, 0.26 + rnd.uniform(-0.03, 0.02))
        p.tube("CT_Wood", [(0, 0, 0.2), knee, tip], 0.008, 3)
        p.lathe(rnd.choice(["CT_Wood", "CT_PlasterGrey", "CT_Leather"]), tip[0], tip[1],
                [(0.0, tip[2] + 0.015), (0.025, tip[2]), (0.0, tip[2] - 0.02)], 5)
    for k in range(6):                                                           # fallen petals
        a, r = rnd.uniform(0, 2 * math.pi), rnd.uniform(0.11, 0.2)
        cx, cy = r * math.cos(a), r * math.sin(a)
        b = rnd.uniform(0, math.pi)
        dx, dy = math.cos(b) * 0.012, math.sin(b) * 0.012
        p.face(rnd.choice(["CT_PlasterPink", "CT_PlasterRed", "CT_PlasterYellow"]),
               [(cx - dx, cy - dy, 0.002), (cx + dy * 0.6, cy - dx * 0.6, 0.002), (cx + dx, cy + dy, 0.002),
                (cx - dy * 0.6, cy + dx * 0.6, 0.002)])
    return p.done(top=0.24)


def plate_crumbs(B, x, y, z=0.0, yaw=0.0, seed=0, solid=False):
    """[prop] A used plate: crumbs, a fork and a knife crossed on it, a crumpled paper napkin beside."""
    p = _P(B, x, y, z, yaw, "plate_crumbs")
    p.kind = "prop"
    rnd = _rng(seed, x, y, "crumbs")
    p.lathe("CT_TileWhite", 0, 0, [(0.07, 0.0), (0.11, 0.012), (0.075, 0.008), (0.0, 0.007)], 10, top=False, a0=math.pi / 10)
    for k in range(9):
        a, r = rnd.uniform(0, 2 * math.pi), rnd.uniform(0.0, 0.06)
        cx, cy = r * math.cos(a), r * math.sin(a)
        s = rnd.uniform(0.003, 0.007)
        p.box("CT_WoodLight", cx - s, cx + s, cy - s, cy + s, 0.008, 0.008 + s)
    p.rod("CT_Aluminium", (-0.07, -0.05, 0.012), (0.08, 0.04, 0.012), 0.008, 4)
    p.rod("CT_Aluminium", (-0.06, 0.05, 0.012), (0.085, -0.03, 0.012), 0.007, 4)
    nx, ny = 0.17, 0.04
    p.lathe("CT_Paper", nx, ny, [(0.03, 0.0), (0.04, 0.015), (0.025, 0.03), (0.0, 0.035)], 6)
    return p.done(top=0.012)


def book_face_down(B, x, y, z=0.0, yaw=0.0, mat=None, seed=0, solid=False):
    """[prop] An open book left face down: two covers in a low tent, the pages' edges showing."""
    p = _P(B, x, y, z, yaw, "book_face_down")
    p.kind = "prop"
    rnd = _rng(seed, x, y, "book_fd")
    m = mat or rnd.choice(["CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterGreen", "CT_Leather", "CT_PlasterYellow"])
    w, d, h = 0.13, 0.2, 0.035
    # the two covers: each quad CCW seen from above-outside, extruded 3 mm in (slab extrudes behind its face)
    p.slab(m, (0, -d / 2, h), (w, -d / 2, 0.003), (w, d / 2, 0.003), (0, d / 2, h), 0.003)
    p.slab(m, (-w, -d / 2, 0.003), (0, -d / 2, h), (0, d / 2, h), (-w, d / 2, 0.003), 0.003)
    for s in (-1, 1):
        p.hexa("CT_Paper", [(s * 0.005, -d / 2 + 0.006, 0.0), (s * (w - 0.006), -d / 2 + 0.006, 0.0),
                            (s * (w - 0.006), d / 2 - 0.006, 0.0), (s * 0.005, d / 2 - 0.006, 0.0)],
               [(s * 0.005, -d / 2 + 0.006, h - 0.006), (s * (w - 0.006), -d / 2 + 0.006, 0.0005),
                (s * (w - 0.006), d / 2 - 0.006, 0.0005), (s * 0.005, d / 2 - 0.006, h - 0.006)])
    return p.done(top=h)


def sledge(B, x, y, z=0.0, yaw=0.0, mat="CT_WoodLight", solid=False):
    """[wall] A child's wooden sledge stood on end against the wall: two bent steel runners, slats, a red pull cord."""
    p = _P(B, x, y, z, yaw, "sledge")
    p.kind = "wall"
    n0 = len(p.m.verts)
    for s in (-1, 1):
        xr = s * 0.17
        p.tube("CT_SteelRed", [(xr, -0.47, 0.0), (xr, 0.38, 0.0), (xr, 0.47, 0.06), (xr, 0.45, 0.14)], 0.02, 4)
        for yy in (-0.32, 0.0, 0.3):
            p.rod("CT_SteelRed", (xr, yy, 0.0), (xr, yy, 0.17), 0.018, 4)
    for k in range(5):
        yy = -0.4 + k * 0.19
        p.bev(mat, -0.2, 0.2, yy - 0.04, yy + 0.04, 0.17, 0.19, 0.005)
    p.tube("CT_PlasterRed", [(-0.15, 0.45, 0.14), (0.0, 0.52, 0.0), (0.15, 0.45, 0.14)], 0.01, 3)
    # stand it on its back end against the wall: turn about x so +y goes up, then lean it 12 degrees off the wall
    _rot_since(p, n0, "x", 90.0, (0.0, 0.0, 0.0))
    _rot_since(p, n0, "x", -12.0, (0.0, 0.0, 0.0))
    vs = p.m.verts[n0:]
    zmin = min(v[2] for v in vs)
    ymax = max(v[1] for v in vs)
    p.m.verts[n0:] = [(vx, vy - ymax - 0.01, vz - zmin) for (vx, vy, vz) in vs]
    return p.done()


def towel_hat(B, x, y, z=0.0, yaw=0.0, seed=0, solid=False):
    """[prop] A folded striped beach towel and a straw hat on top (summer: the sanatorium, the beach café)."""
    p = _P(B, x, y, z, yaw, "towel_hat")
    p.kind = "prop"
    rnd = _rng(seed, x, y, "towel")
    c1 = rnd.choice(["CT_PlasterBlue", "CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterGreen"])
    for k in range(5):
        p.box(c1 if k % 2 == 0 else "CT_TileWhite", -0.2 + k * 0.08, -0.12 + k * 0.08, -0.14, 0.14, 0.0, 0.05,
              f="xXyYZ" if k in (0, 4) else "yYZ")
    p.cyl("CT_WoodLight", 0.04, 0.02, 0.05, 0.055, 0.17, 10)
    p.lathe("CT_WoodLight", 0.04, 0.02, [(0.09, 0.055), (0.085, 0.12), (0.0, 0.135)], 10)
    p.cyl(rnd.choice(["CT_PlasterRed", "CT_PlasterBlue"]), 0.04, 0.02, 0.057, 0.077, 0.091, 10, top=False)
    return p.done(top=0.135)


def school_bag(B, x, y, z=0.0, yaw=0.0, mat=None, seed=0, solid=False):
    """A satchel standing on its bottom (z), flap at the front (-y), shoulder straps behind."""
    p = _P(B, x, y, z, yaw, "school_bag")
    rnd = _rng(seed, x, y, "satchel")
    m = mat or rnd.choice(["CT_Leather", "CT_PlasterRed", "CT_PlasterBlue", "CT_Wood"])
    p.bev(m, -0.15, 0.15, -0.05, 0.06, 0.0, 0.33, 0.02, 0.02)
    p.hexa(m, [(-0.15, -0.065, 0.16), (0.15, -0.065, 0.16), (0.15, -0.05, 0.16), (-0.15, -0.05, 0.16)],
           [(-0.15, -0.065, 0.335), (0.15, -0.065, 0.335), (0.15, -0.05, 0.335), (-0.15, -0.05, 0.335)])
    p.box("CT_Brass", -0.02, 0.02, -0.072, -0.065, 0.17, 0.2)
    for s in (-1, 1):
        p.tube(m, [(s * 0.09, 0.06, 0.3), (s * 0.1, 0.09, 0.2), (s * 0.09, 0.065, 0.05)], 0.035, 4)
    p.rod(m, (-0.05, 0.0, 0.335), (0.05, 0.0, 0.335), 0.015, 4)
    return p.done(top=0.335)


def mitten_pair(B, x, y, z=0.0, yaw=0.0, mat=None, seed=0, solid=False):
    """[prop] A pair of knitted mittens laid out to dry (on a radiator top or a window ledge)."""
    p = _P(B, x, y, z, yaw, "mittens")
    p.kind = "prop"
    rnd = _rng(seed, x, y, "mittens")
    m = mat or rnd.choice(["CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterGreen", "CT_PlasterYellow"])
    for s in (-1, 1):
        cx = s * 0.07
        p.bev(m, cx - 0.045, cx + 0.045, -0.06, 0.08, 0.0, 0.02, 0.008, 0.02)
        p.bev(m, cx + s * 0.04, cx + s * 0.07, -0.02, 0.03, 0.0, 0.018, 0.006, 0.01)        # the thumb
        p.box("CT_TileWhite", cx - 0.045, cx + 0.045, -0.085, -0.06, 0.0, 0.018)            # the cuff
    return p.done(top=0.02)


# ------------------------------------------------------------------------------------------------ traces of a visit
# (B.dress(..., visit=...): what a townsperson leaves behind; docs/city_dress.md "Traces of a visit")

# Six middle-game positions (white's pieces upper case, black lower; rank 8 first, as in FEN): real games' middles, so a
# player's eye reads "somebody stopped here", not a scatter. Kings castled or in the middle, a few pieces off.
CHESS_MIDGAMES = (
    "r1bq1rk1/pp2bppp/2n1pn2/3p4/2PP4/2N1PN2/PP3PPP/R2QKB1R",
    "r2q1rk1/1b2bppp/p2ppn2/1p6/3NP3/1BN1B3/PPP2PPP/R2Q1RK1",
    "2r2rk1/pp1q1ppp/4pn2/3p4/3P4/2PB1Q2/P4PPP/R4RK1",
    "r4rk1/1pp1qppp/p1np1n2/4p3/2B1P1b1/2NP1N2/PPP1QPPP/R4RK1",
    "3r1rk1/p4ppp/1pn1p3/2q5/2P5/P3P3/1B3PPP/2RQ1RK1",
    "6k1/pp3ppp/2p5/3r4/3P4/2P1R3/PP3PPP/6K1",
)
_CHESS_H = {"p": 0.032, "r": 0.04, "n": 0.045, "b": 0.05, "q": 0.058, "k": 0.065}


def _chess_piece(p, mat, u, v, kind, s):
    """One piece of the kit's lathe set: a base, a waist, a head by kind (the knight a leaning block, the king a cross)."""
    h = _CHESS_H[kind]
    r = s * 0.32
    if kind == "n":
        p.lathe(mat, u, v, [(r, 0.0), (r * 0.8, 0.008), (r * 0.55, h * 0.45)], 4, top=True)
        p.bev(mat, u - r * 0.35, u + r * 0.35, v - r * 0.75, v + r * 0.35, h * 0.45, h, 0.004, 0.0)
        return
    prof = [(r, 0.0), (r * 0.85, 0.007), (r * 0.42, h * 0.55), (r * (0.62 if kind in "rq" else 0.5), h * 0.8),
            (r * (0.6 if kind == "r" else 0.2), h)]
    p.lathe(mat, u, v, prof, 4, top=True)
    if kind == "k":
        p.box(mat, u - 0.003, u + 0.003, v - 0.003, v + 0.003, h, h + 0.016)
        p.box(mat, u - 0.008, u + 0.008, v - 0.003, v + 0.003, h + 0.007, h + 0.012)


def chess_left(B, x, y, z=0.0, yaw=0.0, board=True, size=0.42, seed=0, solid=False):
    """[prop] A game of chess left half way: one of CHESS_MIDGAMES (by seed) on a board of `size` m (board=False: the
    table has its own squares; the pieces stand on its top at z), two or three taken pieces beside the board."""
    p = _P(B, x, y, z, yaw, "chess_left")
    p.kind = "prop"
    rnd = _rng(seed, x, y, "chess_left")
    fen = CHESS_MIDGAMES[seed % len(CHESS_MIDGAMES)]
    s = size * 0.9 / 8
    z0 = 0.0
    if board:
        z0 = 0.012
        p.box("CT_Wood", -size / 2, size / 2, -size / 2, size / 2, 0.0, z0, f="xXyYZ")
        for i in range(8):
            for j in range(8):
                if (i + j) % 2 == 0:
                    u, v = -size * 0.45 + i * s, -size * 0.45 + j * s
                    p.face("CT_WoodLight", [(u, v, z0 + 0.0005), (u + s, v, z0 + 0.0005), (u + s, v + s, z0 + 0.0005),
                                            (u, v + s, z0 + 0.0005)])
    left = {"w": [], "b": []}
    full = "rnbqkbnrpppppppp"
    for rank, row in enumerate(fen.split("/")):
        file_ = 0
        for ch in row:
            if ch.isdigit():
                file_ += int(ch)
                continue
            u = -size * 0.45 + (file_ + 0.5) * s
            v = size * 0.45 - (rank + 0.5) * s          # rank 8 at +y (black's side away from yaw's front)
            mat = "CT_TileWhite" if ch.isupper() else "CT_Lacquer"
            _chess_piece(p, mat, u, v, ch.lower(), s)
            left["w" if ch.isupper() else "b"].append(ch.lower())
            file_ += 1
    # two or three of the pieces that are off the board, stood by the board's edge on the taker's side
    for side, mat, vy in (("w", "CT_TileWhite", size / 2 + 0.04), ("b", "CT_Lacquer", -size / 2 - 0.04)):
        gone = list(full)
        for k in left[side]:
            if k in gone:
                gone.remove(k)
        gone = [k for k in gone if k != "k"]
        rnd.shuffle(gone)
        for n, kind in enumerate(gone[:3]):
            _chess_piece(p, mat, size / 2 - 0.03 - n * 0.045, vy if side == "b" else -vy, kind, s)
    return p.done(top=0.08)


def _tile(p, cx, cy, z, a, w=0.024, l=0.048, h=0.008, up=True, on_edge=False):
    """One domino: an ivory block (or its dark back when not up), pips as two dark dots; on_edge: stood up facing +y."""
    ca, sa = math.cos(a), math.sin(a)
    if on_edge:
        p.hexa("CT_TileWhite", [(cx - ca * l / 2, cy - sa * l / 2, z), (cx + ca * l / 2, cy + sa * l / 2, z),
                                (cx + ca * l / 2 - sa * h, cy + sa * l / 2 + ca * h, z), (cx - ca * l / 2 - sa * h, cy - sa * l / 2 + ca * h, z)],
               [(cx - ca * l / 2, cy - sa * l / 2, z + w), (cx + ca * l / 2, cy + sa * l / 2, z + w),
                (cx + ca * l / 2 - sa * h, cy + sa * l / 2 + ca * h, z + w), (cx - ca * l / 2 - sa * h, cy - sa * l / 2 + ca * h, z + w)])
        return

    def pt(u, v, zz):
        return (cx + ca * u - sa * v, cy + sa * u + ca * v, zz)
    mat = "CT_TileWhite" if up else "CT_ConcreteDark"
    p.hexa(mat, [pt(-l / 2, -w / 2, z), pt(l / 2, -w / 2, z), pt(l / 2, w / 2, z), pt(-l / 2, w / 2, z)],
           [pt(-l / 2, -w / 2, z + h), pt(l / 2, -w / 2, z + h), pt(l / 2, w / 2, z + h), pt(-l / 2, w / 2, z + h)])
    if up:
        for su in (-1, 1):
            d = 0.0035
            p.face("CT_ConcreteDark", [pt(su * l / 4 - d, -d, z + h + 0.0005), pt(su * l / 4 + d, -d, z + h + 0.0005),
                                       pt(su * l / 4 + d, d, z + h + 0.0005), pt(su * l / 4 - d, d, z + h + 0.0005)])


def dominoes_left(B, x, y, z=0.0, yaw=0.0, players=4, seed=0, solid=False):
    """[prop] A game of dominoes left on the table: a snake of played tiles turning at the ends, each player's last tiles
    stood on edge at their side (players at -y, +x, +y, -x), the face-down heap ("базар") at a corner."""
    p = _P(B, x, y, z, yaw, "dominoes_left")
    p.kind = "prop"
    rnd = _rng(seed, x, y, "dominoes")
    n = 9 + seed % 6
    cx, cy, a = -0.05 * (n // 2), 0.0, 0.0
    for k in range(n):
        _tile(p, cx, cy, 0.0, a)
        step = 0.05
        if k in (n // 2 + 2, 2) and rnd.random() < 0.7:
            a += math.pi / 2 * (1 if k > 2 else -1)
            step = 0.037
        cx, cy = cx + math.cos(a) * step, cy + math.sin(a) * step
    seats = ((0.0, -0.28, 0.0), (0.3, 0.0, math.pi / 2), (0.0, 0.28, math.pi), (-0.3, 0.0, -math.pi / 2))[:max(2, min(4, players))]
    for sx, sy, sa in seats:
        for j in range(rnd.randint(2, 4)):
            u = (j - 1.5) * 0.028
            _tile(p, sx + math.cos(sa) * u, sy + math.sin(sa) * u, 0.0, sa, on_edge=True)
    for j in range(rnd.randint(3, 5)):
        _tile(p, 0.24 + rnd.uniform(-0.03, 0.03), -0.2 + j * 0.03, 0.0, rnd.uniform(0, math.pi), up=False)
    return p.done(top=0.03)


def tea_in_holder(B, x, y, z=0.0, yaw=0.0, lemon=True, seed=0, solid=False):
    """[prop] A tea glass in a metal holder (podstakannik): the openwork band, the handle, the spoon left standing in it,
    the tea a third drunk; a slice of lemon on the saucer-less table beside it."""
    p = _P(B, x, y, z, yaw, "tea_holder")
    p.kind = "prop"
    p.lathe("CT_Brass", 0, 0, [(0.03, 0.0), (0.037, 0.008), (0.035, 0.05), (0.038, 0.058)], 8, top=False, a0=math.pi / 8)
    p.lathe("CT_Glass", 0, 0, [(0.033, 0.05), (0.036, 0.105)], 8, top=False, a0=math.pi / 8)
    p.cyl("CT_Wood", 0, 0, 0.07, 0.072, 0.034, 8, a0=math.pi / 8)
    p.tube("CT_Brass", [(0.036, 0.0, 0.052), (0.06, 0.0, 0.048), (0.06, 0.0, 0.016), (0.036, 0.0, 0.012)], 0.008, 3)
    p.rod("CT_Aluminium", (0.006, 0.01, 0.03), (0.02, 0.025, 0.14), 0.005, 3)
    if lemon:
        p.cyl("CT_PlasterYellow", 0.085, -0.03, 0.0, 0.006, 0.022, 7)
    return p.done(top=0.12)


def spectacles(B, x, y, z=0.0, yaw=0.0, solid=False):
    """[prop] Reading glasses left folded: two round lenses in a thin frame, the temples folded behind."""
    p = _P(B, x, y, z, yaw, "spectacles")
    p.kind = "prop"
    for s in (-1, 1):
        p.cyl("CT_Glass", s * 0.028, 0.0, 0.004, 0.006, 0.022, 8)
        p.lathe("CT_Lacquer", s * 0.028, 0.0, [(0.024, 0.002), (0.024, 0.009)], 8, top=False)
    p.rod("CT_Lacquer", (-0.007, 0.0, 0.008), (0.007, 0.0, 0.008), 0.004, 3)
    p.rod("CT_Lacquer", (-0.05, 0.004, 0.006), (0.04, 0.012, 0.003), 0.003, 3)
    p.rod("CT_Lacquer", (0.05, 0.008, 0.009), (-0.04, 0.016, 0.006), 0.003, 3)
    return p.done(top=0.012)


def exercise_books(B, x, y, z=0.0, yaw=0.0, seed=0, solid=False):
    """[prop] Homework left out: two exercise books (thin, the school's green and blue covers), one open on its ruled pages,
    a pencil case and a pencil."""
    p = _P(B, x, y, z, yaw, "exercise_books")
    p.kind = "prop"
    rnd = _rng(seed, x, y, "exbooks")
    # the open one: two pages side by side, a faint spine dip
    p.box(rnd.choice(["CT_PlasterGreen", "CT_PlasterBlue"]), -0.17, 0.17, -0.1, 0.1, 0.0, 0.002, f="xXyYZ")
    p.slab("CT_Paper", (-0.165, -0.095, 0.003), (-0.003, -0.095, 0.0025), (-0.003, 0.095, 0.0025), (-0.165, 0.095, 0.003), 0.001)
    p.slab("CT_Paper", (0.003, -0.095, 0.0025), (0.165, -0.095, 0.003), (0.165, 0.095, 0.003), (0.003, 0.095, 0.0025), 0.001)
    for k in range(6):                       # ruled lines, pencil grey
        v = -0.07 + k * 0.028
        p.face("CT_PlasterGrey", [(0.02, v, 0.0032), (0.15, v, 0.0032), (0.15, v + 0.002, 0.0032), (0.02, v + 0.002, 0.0032)])
    # the shut one, askew on the side
    a = rnd.uniform(-0.4, 0.4)
    ca, sa = math.cos(a), math.sin(a)
    q = [(-0.085, -0.1), (0.085, -0.1), (0.085, 0.1), (-0.085, 0.1)]
    ox, oy = 0.27, 0.03
    bot = [(ox + ca * u - sa * v, oy + sa * u + ca * v, 0.0) for u, v in q]
    p.hexa(rnd.choice(["CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterBlue"]), bot, [(b[0], b[1], 0.005) for b in bot])
    # the pencil case and a pencil across the open book
    p.bev("CT_PlasterRed", -0.12, 0.08, 0.13, 0.18, 0.0, 0.03, 0.008, 0.0)
    p.rod("CT_PlasterYellow", (0.03, -0.06, 0.008), (0.16, 0.02, 0.008), 0.007, 6)
    return p.done(top=0.03)


def class_register(B, x, y, z=0.0, yaw=0.0, seed=0, solid=False):
    """[prop] The class register (классный журнал): a thick bound book in a dark cloth cover with a pale label, a felt
    duster grey with chalk, a stub of chalk."""
    p = _P(B, x, y, z, yaw, "class_register")
    p.kind = "prop"
    p.bev("CT_PlasterBlue", -0.11, 0.11, -0.15, 0.15, 0.0, 0.03, 0.004, 0.0)
    p.box("CT_Paper", -0.105, 0.1, -0.145, 0.145, 0.003, 0.027, f="xXyY")
    p.box("CT_Paper", -0.05, 0.05, -0.02, 0.04, 0.03, 0.0305, f="Z")
    p.bev("CT_Wood", 0.17, 0.3, -0.05, 0.0, 0.0, 0.025, 0.004, 0.0)
    p.box("CT_PlasterGrey", 0.17, 0.3, -0.05, 0.0, 0.025, 0.04, f="xXyYZ")
    p.rod("CT_TileWhite", (0.2, 0.06, 0.006), (0.24, 0.07, 0.006), 0.011, 6)
    return p.done(top=0.04)


def ticket_stubs(B, x, y, z=0.0, yaw=0.0, n=3, wrapper=True, seed=0, solid=False):
    """[prop] Torn cinema ticket stubs (pale pink and blue slips) and a sweet wrapper twisted at the ends."""
    p = _P(B, x, y, z, yaw, "ticket_stubs")
    p.kind = "prop"
    rnd = _rng(seed, x, y, "stubs")
    for k in range(n):
        cx, cy = rnd.uniform(-0.12, 0.12), rnd.uniform(-0.08, 0.08)
        a = rnd.uniform(0, math.pi)
        ca, sa = math.cos(a), math.sin(a)
        q = [(-0.028, -0.014), (0.028, -0.014), (0.028, 0.014), (-0.028, 0.014)]
        p.face(rnd.choice(["CT_PlasterPink", "CT_PlasterBlue", "CT_Paper"]),
               [(cx + ca * u - sa * v, cy + sa * u + ca * v, 0.001 + 0.0004 * k) for u, v in q])
    if wrapper:
        wx, wy = rnd.uniform(-0.1, 0.1), rnd.uniform(-0.06, 0.06)
        p.lathe(rnd.choice(["CT_PlasterRed", "CT_Brass", "CT_PlasterGreen"]), wx, wy,
                [(0.0, 0.0), (0.012, 0.004), (0.0, 0.009)], 5)
    return p.done(top=0.01)


def sheet_music(B, x, y, z=0.0, yaw=0.0, tilt=0.0, seed=0, solid=False):
    """[prop] A book of music open flat (tilt > 0: leaning back that many degrees, on a stand), a loose sheet beside."""
    p = _P(B, x, y, z, yaw, "sheet_music")
    p.kind = "prop"
    t = math.radians(tilt)
    dz, dy = 0.24 * math.sin(t), 0.24 * math.cos(t)
    for x0, x1 in ((-0.17, -0.004), (0.004, 0.17)):
        p.slab("CT_Paper", (x0, 0.0, 0.002), (x1, 0.0, 0.002), (x1, dy, 0.002 + dz), (x0, dy, 0.002 + dz), 0.002)
        for k in range(5):           # the staves, dark lines across each page
            f = 0.15 + k * 0.17
            p.face("CT_ConcreteDark", [(x0 + 0.012, dy * f, 0.0045 + dz * f), (x1 - 0.012, dy * f, 0.0045 + dz * f),
                                       (x1 - 0.012, dy * f + 0.003, 0.0045 + dz * (f + 0.012)), (x0 + 0.012, dy * f + 0.003, 0.0045 + dz * (f + 0.012))])
    if tilt == 0.0:
        p.face("CT_Paper", [(0.2, -0.02, 0.001), (0.4, 0.01, 0.001), (0.38, 0.27, 0.001), (0.18, 0.24, 0.001)])
    return p.done(top=0.01 + dz)


PIECES = {f.__name__: f for f in (coats_on_rail, umbrella_open, umbrella_closed, umbrella_stand, wall_hooks, boots, doormat_wet, jigsaw,
                                   vase_wilted, plate_crumbs, book_face_down, sledge, towel_hat, school_bag, mitten_pair,
                                   chess_left, dominoes_left, tea_in_holder, spectacles, exercise_books, class_register,
                                   ticket_stubs, sheet_music)}
