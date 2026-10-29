"""The ground floor of Novikova 1 as a 1950s district / scientists' library: a drop-in alternative to
gastronom.py (same interface, same shell). Pure Python (no bpy): build() returns a geom.Mesh.

Frame: the Novikova 1 HOUSE frame in metres, exactly as gastronom.py: x along the house (north +),
y across it (square-side inner wall face y = -6.75, outer face -7.25), ground floor z = 0.7,
ceiling z = 4.2.

Spaces (the shell is gastronom.py's, ported with library finishes):
  vestibule  x -2.45..2.45, y -6.75..-1.5: stone tiles, sage-green dado. The entrance (1.6 m) in the
             east wall; cloakroom counter + coats along the back wall; the issue desk (кафедра выдачи)
             faces the arch, the librarian sits with her back to the closed glazed door to the
             hairdresser's; two card-catalogue cabinets in the south-east corner; noticeboard and
             opening-hours board on the wall beside the arch; a board «Читальный зал» hangs under the arch.
  arch       x 2.45..3.0, y -4.8..-2.4, crown 3.0 m over the floor.
  hall       x 3.0..18.0, y -6.75..1.5: oak parquet. Tall open bookcases (2.4 m, moulded tops) along
             the back wall y = 1.5 (broken by three plant stands at the ends of the stack aisles), the
             end wall x = 18, the arch wall x = 3 and on the piers between the windows; four
             double-sided stacks; three reading tables (green baize, bentwood chairs, green banker's
             lamps) on the window axes; a round newspaper table and the periodicals rack by the
             hall door; a reading nook (two leather club chairs, a floor globe) at the north end.
             House plants everywhere: every window sill, the floor by the windows, stands, the tops
             of the bookcases (trailing tradescantia, spider plants).

Text: no words in the meshes. Sign boards are plain dark-green glass in brass frames; LABELS gives
(localization key, centre, size) of each board face and LABEL_NORMALS its facing.

Hooks for later gameplay:
  BOOK_SLOTS  one entry per shelf run (every shelf of every section): the front edge of the shelf at
              book-base height runs from (x0, y0) to (x1, y1) (for runs along the house x axis
              y0 == y1 == y); z = top of the shelf board, depth = usable depth behind the front edge,
              height = clear height to the shelf above, facing = (nx, ny) towards the reader.
  PLANTS      [(kind, x, y, z, yaw, seed, size)]: every plant (pot base at z). Built with
              houseplants.place() when that module exists, simple stand-ins otherwise.
  FOOTPRINTS  [(name, x0, x1, y0, y1)] of everything standing on the floor (filled by build()).

Run `python3 library.py [--audit]` for the self-check (triangles, materials, routes, door boxes).
"""
import importlib
import math
import random

from geom import Frame, Mesh, wall_with_holes
import gastronom as G
from gastronom import face_n, bx, tube, cyl, _Wall, _arch_pts, _tris, _openings, _dot
import furniture as F

# --- materials ---------------------------------------------------------------------------------------
LIBRARY_MATERIALS = {
    # name: (base colour sRGB hex, roughness, what it is)
    "AC_LParquet":      ("#8A5A32", 0.35, "waxed oak parquet of the hall, boards 7 x 40 cm on the diagonal of the UV"),
    "AC_LStone":        ("#B9A68A", 0.45, "vestibule floor: 15 x 15 cm metlakh tiles, beige and brick-red, thresholds"),
    "AC_LDado":         ("#5F7358", 0.35, "sage-green oil-painted dado to 1.2 m"),
    "AC_LWall":         ("#E4D9BC", 0.90, "warm cream distemper above the dado"),
    "AC_LPlaster":      ("#EEEADF", 0.90, "lime-white plaster: ceilings, cornices, reveals, ceiling roses, busts"),
    "AC_LPaintWhite":   ("#ECE8DD", 0.50, "white oil-painted joinery: architraves, window boards, radiators"),
    "AC_LWood":         ("#6E4426", 0.40, "varnished oak: bookcases, stacks, tables, catalogue cabinets"),
    "AC_LWoodDark":     ("#3E2516", 0.35, "dark stained wood: plinths, bentwood chairs, desk, shelf backs"),
    "AC_LBaize":        ("#2F5A3A", 0.90, "green baize / linoleum tops of the reading tables and the issue desk"),
    "AC_LLeather":      ("#4A2A1A", 0.45, "dark brown leather: club armchairs"),
    "AC_LUphRed":       ("#7E2A24", 0.85, "red upholstery and plush"),
    "AC_LShade":        ("#C9A060", 0.80, "silk lamp shade"),
    "AC_LBrass":        ("#B08D45", 0.30, "brass: frames, handles, lamp stems, rails (metallic)"),
    "AC_LChrome":       ("#C8CACC", 0.15, "nickel (metallic)"),
    "AC_LIron":         ("#2E2C2A", 0.60, "black iron: hooks, coat rails, stamp"),
    "AC_LBlack":        ("#121110", 0.15, "black carbolite / lacquer"),
    "AC_LOpal":         ("#FFF6E6", 0.30, "opal glass of the pendant globes and lamp bowls (emissive when lit)"),
    "AC_LGreenGlass":   ("#2E6B3E", 0.10, "green cased glass of the banker's lamps"),
    "AC_LGlass":        ("#DDEBEA", 0.03, "clear glass (transparent)"),
    "AC_LPaper":        ("#E9E2CC", 0.80, "paper: catalogue cards, pages, notices, coats' linings"),
    "AC_LNewsprint":    ("#CFC8B4", 0.85, "newsprint and magazine covers"),
    "AC_LDoor":         ("#4A3020", 0.45, "dark brown painted inner door leaves (back rooms, hairdresser's)"),
    "AC_LCork":         ("#A07A4E", 0.90, "cork noticeboard"),
    "AC_LBoard":        ("#1F3A2C", 0.35, "dark green reverse-painted glass sign boards (text is printed by the game)"),
    "AC_LCarpet":       ("#7E2A24", 0.95, "runner and rug field: red"),
    "AC_LCarpetBorder": ("#2B3550", 0.95, "runner and rug border: dark blue"),
    "AC_LGlobe":        ("#5B86A6", 0.40, "globe: ocean blue varnished paper"),
    # book spines (the library's colour comes from these)
    "AC_LBookMaroon":   ("#6E1F1C", 0.60, "book spines: maroon cloth"),
    "AC_LBookNavy":     ("#22304A", 0.60, "book spines: navy cloth"),
    "AC_LBookGreen":    ("#2E4A33", 0.60, "book spines: bottle-green cloth"),
    "AC_LBookBrown":    ("#6B4A2C", 0.60, "book spines: brown leather / cloth"),
    "AC_LBookOchre":    ("#A57A2E", 0.60, "book spines: ochre"),
    "AC_LBookCream":    ("#D8CBA8", 0.70, "book spines: cream paper"),
    "AC_LBookBlack":    ("#1E1C1A", 0.50, "book spines: black"),
    "AC_LBookGrey":     ("#6F7476", 0.65, "book spines: grey-blue"),
    "AC_LBookRed":      ("#A0332A", 0.55, "book spines: bright red"),
    "AC_LBookTeal":     ("#2F5E62", 0.60, "book spines: teal"),
    # stand-in plants (houseplants.py brings its own PLANT_MATERIALS)
    "AC_LPotClay":      ("#A9583A", 0.75, "terracotta flower pots"),
    "AC_LPotWhite":     ("#EDEAE0", 0.30, "white glazed ceramic pots and vases"),
    "AC_LSoil":         ("#3A2A1E", 0.95, "potting soil"),
    "AC_LLeaf":         ("#2F5A2A", 0.55, "dark green leaves (ficus, monstera, palms)"),
    "AC_LLeafLight":    ("#5E8A3A", 0.60, "light green leaves (ferns, geraniums)"),
    "AC_LLeafVar":      ("#8FA860", 0.60, "variegated / pale leaves (tradescantia, spider plant)"),
    "AC_LFlowerRed":    ("#C0302A", 0.60, "red flowers (geranium)"),
    "AC_LFlowerPink":   ("#B85C9A", 0.60, "pink-violet flowers (violets, begonias)"),
    "AC_LBark":         ("#5A4632", 0.80, "plant stems and trunks"),
}
GASTRONOM_MATERIALS = LIBRARY_MATERIALS      # alias: `import library as gastronom` works in the build
# also used: common.MATERIALS AC_Frame (door architraves), AC_Trim (reveal liners);
# furniture.FURNITURE_MATERIALS AC_FPaint_<name> (picture images, furniture.PAINTINGS); with houseplants.py
# present, its PLANT_MATERIALS are merged into LIBRARY_MATERIALS below.

# materials to smooth-shade (creases above furniture.SMOOTH_ANGLE kept), like furniture.SMOOTH_MATERIALS
SMOOTH_MATERIALS = {"AC_LLeather", "AC_LUphRed", "AC_LBrass", "AC_LChrome", "AC_LOpal", "AC_LGreenGlass",
                    "AC_LGlobe", "AC_LPotClay", "AC_LPotWhite", "AC_LShade"}

# furniture.py props are re-coloured into the library palette
FMAP = {
    "AC_FWalnut": "AC_LWoodDark", "AC_FOak": "AC_LWood", "AC_FBeech": "AC_LWood", "AC_FLeather": "AC_LLeather",
    "AC_FLeatherGreen": "AC_LBaize", "AC_FDamaskRed": "AC_LUphRed", "AC_FPlushGold": "AC_LShade",
    "AC_FLinen": "AC_LPaper", "AC_FLace": "AC_LPaper", "AC_FCurtainGreen": "AC_LBaize", "AC_FCurtainWine": "AC_LUphRed",
    "AC_FWool": "AC_LCarpet", "AC_FBrass": "AC_LBrass", "AC_FChrome": "AC_LChrome", "AC_FOpalGlass": "AC_LOpal",
    "AC_FGreenGlass": "AC_LGreenGlass", "AC_FEnamelWhite": "AC_LPaintWhite", "AC_FEnamelCream": "AC_LPaintWhite",
    "AC_FCastIron": "AC_LIron", "AC_FPorcelain": "AC_LPotWhite", "AC_FCeiling": "AC_LPlaster",
    "AC_FBookRed": "AC_LBookMaroon", "AC_FBookBlue": "AC_LBookNavy", "AC_FBookGreen": "AC_LBookGreen",
    "AC_FBookBrown": "AC_LBookBrown", "AC_FBookCream": "AC_LBookCream", "AC_FRugRed": "AC_LCarpet",
    "AC_FRugBlue": "AC_LCarpetBorder", "AC_FMirror": "AC_LGlass", "AC_FGlass": "AC_LGlass", "AC_FPaper": "AC_LPaper",
    "AC_FBlackLacquer": "AC_LBlack", "AC_FCanvas": "AC_LWoodDark",
}

WOOD, WOODD, BRASS, PAPER = "AC_LWood", "AC_LWoodDark", "AC_LBrass", "AC_LPaper"
BOOK_MATS = ["AC_LBookMaroon", "AC_LBookMaroon", "AC_LBookNavy", "AC_LBookNavy", "AC_LBookGreen", "AC_LBookGreen",
             "AC_LBookBrown", "AC_LBookBrown", "AC_LBookOchre", "AC_LBookCream", "AC_LBookCream", "AC_LBookBlack",
             "AC_LBookGrey", "AC_LBookRed", "AC_LBookTeal"]

# --- key dimensions (the shell is gastronom.py's) ----------------------------------------------------------
Z0, ZC, YI, YO = G.Z0, G.ZC, G.YI, G.YO
HALL, VEST = G.HALL, G.VEST
ARCH_Y, ARCH_TOP = G.ARCH_Y, G.ARCH_TOP
ENTRANCE, HALL_DOOR, SHOP_WINDOWS = G.ENTRANCE, G.HALL_DOOR, G.SHOP_WINDOWS
COLUMN = G.COLUMN
DOOR_LEAF_Y = G.DOOR_LEAF_Y
DADO = Z0 + 1.2
BACK_DOOR = (16.7, 17.8, Z0, Z0 + 2.15)     # to the back rooms; widened from the shop's 0.9 m to 1.1 m (game rule)
SIDE_DOOR = (-4.3, -3.0, Z0 + 2.4)          # closed glazed double door to the hairdresser's (x = -2.45 wall)
SILL_Z = SHOP_WINDOWS[0][1] + 0.006         # top of the inner window boards
ARCH_BOARD_Z = (Z0 + 2.51, Z0 + 2.75)       # the board hung under the arch (bottom, top)

# bookcases: (name, kind, centre x, centre y, yaw, width, depth, height, shelves)
#   furniture convention: built centred, front at -y; yaw turns the front to (sin yaw, -cos yaw)
CASE_D, CASE_H, CASE_N = 0.36, 2.4, 6
STACK_D, STACK_H, STACK_N = 0.6, 2.1, 5
STACK_X = (5.0, 7.3, 9.6, 11.9)
STACK_Y = (-1.85, -0.05)
PLANT_STANDS_X = (6.15, 10.75, 13.05)       # gaps in the back-wall run at the ends of the stack aisles


def _case_row():
    rows = []
    for k, (a, b) in enumerate(((3.0, 5.9), (6.4, 10.5), (11.0, 12.8), (13.3, 16.45))):
        rows.append(("back%d" % (k + 1), "wall", (a + b) / 2, HALL[3] - CASE_D / 2, 0.0, b - a, CASE_D, CASE_H, CASE_N))
    a, b = -2.1, HALL[3] - CASE_D
    rows.append(("south", "wall", HALL[0] + CASE_D / 2, (a + b) / 2, math.pi / 2, b - a, CASE_D, CASE_H, CASE_N))
    a, b = -3.4, 0.2
    rows.append(("north", "wall", HALL[1] - CASE_D / 2, (a + b) / 2, -math.pi / 2, b - a, CASE_D, CASE_H, CASE_N))
    for k, (a, b) in enumerate(((5.45, 6.9), (9.75, 11.1), (13.8, 15.15))):
        rows.append(("pier%d" % (k + 1), "wall", (a + b) / 2, YI + CASE_D / 2, math.pi, b - a, CASE_D, CASE_H, CASE_N))
    for k, x in enumerate(STACK_X):
        rows.append(("stack%d" % (k + 1), "stack", x, sum(STACK_Y) / 2, math.pi / 2, STACK_Y[1] - STACK_Y[0],
                     STACK_D, STACK_H, STACK_N))
    return rows


BOOKCASES = _case_row()

# reading tables on the window axes, the newspaper table, the nook
TABLE_L, TABLE_W, TABLE_H = 2.0, 0.9, 0.76
TABLE_Y = (-5.75, -3.75)
TABLES_X = [w[0] for w in SHOP_WINDOWS]
ROUND_TABLE = (5.9, -4.15)
NOOK = (15.25, -0.5)
GLOBE_XY = (13.75, -0.6)
ISSUE_DESK = (-0.1, -3.85)       # centre; its front faces the arch (+x)
CLOAK = (-1.25, -2.275, 2.3)     # centre x, centre y, length of the cloakroom counter
# the door from the vestibule to the middle stair's lobby (the central portal is the stair's street entrance
# too; NPC life thread, 2026-10-25): x range in the back wall, its top; an ALyudDoor fills it (DOORS)
LOBBY_DOOR = (0.17, 1.33, Z0 + 2.3)
CATALOGUES = [(-1.75, YI + 0.225, math.pi, 1.3), (VEST[0] + 0.225, -5.75, math.pi / 2, 1.0)]   # (x, y, yaw, width)
RACK = (HALL[0] + 0.18, -5.95)   # periodicals rack on the arch wall, south of the arch

# --- lights: 7 opal globe pendants + one point per reading table (two banker's lamps each) ------------
LAMP_Z = ZC - 0.95               # centre of the opal globes
LAMP_R = 0.16
LAMP_XY = [(0.6, -4.0), (5.9, -4.0), (9.6, -2.45), (13.4, -2.45), (16.4, -1.7), (6.15, 0.55), (10.75, 0.55)]
LIGHT_COLOUR = "#FFD9A8"
LIGHT_LUMENS = 1600.0
TABLE_LIGHT = ("#FFD2A0", 900.0)
LIGHTS = ([(x, y, LAMP_Z, LIGHT_COLOUR, LIGHT_LUMENS) for x, y in LAMP_XY]
          + [(x, sum(TABLE_Y) / 2, Z0 + TABLE_H + 0.3, TABLE_LIGHT[0], TABLE_LIGHT[1]) for x in TABLES_X])

LABELS = []          # (key, x, y, z, w, h): centre and size of the board face
LABEL_NORMALS = []   # (nx, ny, nz) per LABELS entry
FOOTPRINTS = []      # (name, x0, x1, y0, y1) of floor-standing furniture and plants (filled by build())


# --- houseplants.py (a colleague's module), re-checked on every build ---------------------------------
def houseplants():
    try:
        mod = importlib.import_module("houseplants")
        if all(hasattr(mod, a) for a in ("make", "info", "place")):
            return mod
    except Exception:  # noqa: BLE001
        pass
    return None


def _merge_plant_materials():
    hp = houseplants()
    if hp is not None:
        for k, v in getattr(hp, "PLANT_MATERIALS", {}).items():
            LIBRARY_MATERIALS.setdefault(k, v)
    return hp


# --- small helpers --------------------------------------------------------------------------------------
def remap(part):
    """furniture.py colours -> library palette (picture images AC_FPaint_* are kept)."""
    out = []
    for mt, i, uv in part.faces:
        if mt.startswith("AC_F") and not mt.startswith("AC_FPaint_"):
            mt = FMAP.get(mt, WOOD)
        out.append((mt, i, uv))
    part.faces = out
    return part


def put(m, part, x, y, z=Z0, yaw=0.0, name=None):
    """Adds a prop built in the furniture convention at (x, y, z) turned by yaw; name: record its
    floor footprint."""
    p = Frame(x, y, yaw, z).apply(remap(part))
    m.extend(p)
    low = [v for v in p.verts if v[2] < z + 1.9] if name else []
    if low:
        xs = [v[0] for v in low]
        ys = [v[1] for v in low]
        FOOTPRINTS.append((name, min(xs), max(xs), min(ys), max(ys)))
    return p


def fp(name, x0, x1, y0, y1):
    FOOTPRINTS.append((name, min(x0, x1), max(x0, x1), min(y0, y1), max(y0, y1)))


def two_sided(m, mat, pts):
    m.face(mat, list(pts))
    m.face(mat, list(reversed(pts)))


# --- shell (ported from gastronom.py with library finishes) -------------------------------------------
def finish_wall(m, w, length, holes, dado_mat="AC_LDado"):
    """Wall face (z Z0..ZC) with holes [(u0, u1, z0, z1)]: painted dado to DADO, distemper above, a
    dark wooden chair rail on the dado line between the holes."""
    loc = Mesh()
    wall_with_holes(loc, dado_mat, length, Z0, DADO, holes)
    wall_with_holes(loc, "AC_LWall", length, DADO, ZC, holes)
    cuts = sorted((h[0], h[1]) for h in holes if h[2] < DADO + 0.02 and h[3] > DADO - 0.03)
    u = 0.0
    for a, b in cuts + [(length, length)]:
        if a - u > 0.05:
            F.bx(loc, WOODD, u, a, 0.0, 0.022, DADO - 0.035, DADO + 0.015, faces="yzZxX")
        u = max(u, b)
    m.extend(loc.transformed(w))


def arch_spandrel(m, w, u0, u1, ztop, zhole, segs=12):
    pts, _ = _arch_pts(u0, u1, ztop, segs)
    loc = Mesh()
    for (a, az), (b, bz) in zip(pts, pts[1:]):
        face_n(loc, "AC_LWall", [(a, 0, az), (b, 0, bz), (b, 0, zhole), (a, 0, zhole)], (0, 1, 0),
               [(a, az), (b, bz), (b, zhole), (a, zhole)])
    m.extend(loc.transformed(w))


def arch_reveal(m, w, u0, u1, ztop, depth, segs=12):
    pts, zs = _arch_pts(u0, u1, ztop, segs)
    loc = Mesh()
    D = -depth
    cu = (u0 + u1) / 2
    for u, n in ((u0, (1, 0, 0)), (u1, (-1, 0, 0))):
        face_n(loc, "AC_LDado", [(u, 0, Z0), (u, D, Z0), (u, D, DADO), (u, 0, DADO)], n)
        face_n(loc, "AC_LPlaster", [(u, 0, DADO), (u, D, DADO), (u, D, zs), (u, 0, zs)], n)
    for (a, az), (b, bz) in zip(pts, pts[1:]):
        mid = ((a + b) / 2, 0, (az + bz) / 2)
        face_n(loc, "AC_LPlaster", [(a, 0, az), (b, 0, bz), (b, D, bz), (a, D, az)], (cu - mid[0], 0, zs - mid[2]))
    m.extend(loc.transformed(w))


def door_in_wall(m, w, u0, u1, z1, leaves=1, glazed=False, recess=0.08):
    """A closed panelled door in a hole of the wall w: plaster reveal, leaf, brass knobs, white
    architrave (AC_Frame) 0.12 m wide."""
    loc = Mesh()
    D = -recess
    face_n(loc, "AC_LPlaster", [(u0, 0, Z0), (u0, D, Z0), (u0, D, z1), (u0, 0, z1)], (1, 0, 0))
    face_n(loc, "AC_LPlaster", [(u1, 0, Z0), (u1, D, Z0), (u1, D, z1), (u1, 0, z1)], (-1, 0, 0))
    face_n(loc, "AC_LPlaster", [(u0, 0, z1), (u1, 0, z1), (u1, D, z1), (u0, D, z1)], (0, 0, -1))
    face_n(loc, "AC_LDoor", [(u0, D, Z0), (u1, D, Z0), (u1, D, z1), (u0, D, z1)], (0, 1, 0))
    face_n(loc, "AC_LStone", [(u0, 0, Z0), (u1, 0, Z0), (u1, D, Z0), (u0, D, Z0)], (0, 0, 1))
    lw = (u1 - u0) / leaves
    for k in range(leaves):
        a, b = u0 + k * lw, u0 + (k + 1) * lw
        mz = Z0 + 0.95
        bx(loc, "AC_LDoor", a + 0.1, b - 0.1, D, D + 0.02, Z0 + 0.15, mz - 0.1, faces="yXxzZY")
        if glazed:
            bx(loc, "AC_LGlass", a + 0.12, b - 0.12, D - 0.005, D + 0.005, mz + 0.05, z1 - 0.15)
        else:
            bx(loc, "AC_LDoor", a + 0.1, b - 0.1, D, D + 0.02, mz + 0.05, z1 - 0.15, faces="yXxzZY")
        hx = b - 0.08 if k == 0 else a + 0.08
        bx(loc, BRASS, hx - 0.015, hx + 0.015, D, D + 0.06, Z0 + 1.0, Z0 + 1.06)
    t, p = 0.12, 0.025
    bx(loc, "AC_Frame", u0 - t, u0, 0, p, Z0, z1 + t)
    bx(loc, "AC_Frame", u1, u1 + t, 0, p, Z0, z1 + t)
    bx(loc, "AC_Frame", u0, u1, 0, p, z1, z1 + t)
    m.extend(loc.transformed(w))


def cornice(m, x0, x1, y0, y1):
    prof = [(0.0, 0.32), (0.025, 0.32), (0.025, 0.28), (0.05, 0.26), (0.05, 0.22), (0.10, 0.17),
            (0.13, 0.17), (0.13, 0.13), (0.19, 0.09), (0.22, 0.09), (0.22, 0.05), (0.27, 0.035), (0.30, 0.0)]
    centre = ((x0 + x1) / 2, (y0 + y1) / 2, ZC - 1.5)

    def ring(d, dz):
        z = ZC - dz
        return [(x0 + d, y0 + d, z), (x1 - d, y0 + d, z), (x1 - d, y1 - d, z), (x0 + d, y1 - d, z)]

    rings = [ring(d, dz) for d, dz in prof]
    for ra, rb in zip(rings, rings[1:]):
        for k in range(4):
            j = (k + 1) % 4
            pts = [ra[k], ra[j], rb[j], rb[k]]
            c = [sum(p[i] for p in pts) / 4 for i in range(3)]
            face_n(m, "AC_LPlaster", pts, (centre[0] - c[0], centre[1] - c[1], centre[2] - c[2]))
    d = prof[-1][0]
    face_n(m, "AC_LPlaster", [(x0 + d, y0 + d, ZC), (x1 - d, y0 + d, ZC), (x1 - d, y1 - d, ZC), (x0 + d, y1 - d, ZC)],
           (0, 0, -1))


def floor(m, mat, x0, x1, y0, y1):
    pts = [(x0, y0, Z0), (x1, y0, Z0), (x1, y1, Z0), (x0, y1, Z0)]
    s = math.sqrt(0.5)
    uvs = [((x + y) * s, (y - x) * s) for x, y, _ in pts] if mat == "AC_LParquet" else [(x, y) for x, y, _ in pts]
    face_n(m, mat, pts, (0, 0, 1), uvs)


def walls(m):
    hx0, hx1, hy0, hy1 = HALL
    vx0, vx1, vy0, vy1 = VEST
    # hall east wall (window wall)
    w = _Wall((hx0, YI), (0.0, 1.0))
    holes = [(cx - wd / 2 - hx0, cx + wd / 2 - hx0, sill, sill + ht) for cx, sill, wd, ht in [HALL_DOOR] + SHOP_WINDOWS]
    finish_wall(m, w, hx1 - hx0, holes)
    # hall north end
    finish_wall(m, _Wall((hx1, hy0), (-1.0, 0.0)), hy1 - hy0, [])
    # hall back wall with the door to the back rooms
    w = _Wall((hx1, hy1), (0.0, -1.0))
    du0, du1 = hx1 - BACK_DOOR[1], hx1 - BACK_DOOR[0]
    finish_wall(m, w, hx1 - hx0, [(du0, du1, Z0, BACK_DOOR[3])])
    door_in_wall(m, w, du0, du1, BACK_DOOR[3], leaves=1)
    # hall south wall (x = 3) with the arch
    w = _Wall((hx0, hy1), (1.0, 0.0))
    a0, a1 = hy1 - ARCH_Y[1], hy1 - ARCH_Y[0]
    finish_wall(m, w, hy1 - hy0, [(a0, a1, Z0, ARCH_TOP)])
    arch_spandrel(m, w, a0, a1, ARCH_TOP, ARCH_TOP)
    arch_reveal(m, w, a0, a1, ARCH_TOP, hx0 - vx1)
    # vestibule east wall with the entrance
    w = _Wall((vx0, YI), (0.0, 1.0))
    cx, sill, wd, ht = ENTRANCE
    finish_wall(m, w, vx1 - vx0, [(cx - wd / 2 - vx0, cx + wd / 2 - vx0, sill, sill + ht)])
    # vestibule north wall (x = 2.45) with the arch
    w = _Wall((vx1, vy0), (-1.0, 0.0))
    b0, b1 = ARCH_Y[0] - vy0, ARCH_Y[1] - vy0
    finish_wall(m, w, vy1 - vy0, [(b0, b1, Z0, ARCH_TOP)])
    arch_spandrel(m, w, b0, b1, ARCH_TOP, ARCH_TOP)
    # vestibule back wall with the door to the stair's lobby (its leaf is an ALyudDoor)
    l0, l1, lz = LOBBY_DOOR
    finish_wall(m, _Wall((vx1, vy1), (0.0, -1.0)), vx1 - vx0, [(vx1 - l1, vx1 - l0, Z0, lz)])
    for x, n in ((l0, 1), (l1, -1)):            # plaster reveals through the 0.15 m wall
        face_n(m, "AC_LPlaster", [(x, vy1, Z0), (x, vy1 + 0.15, Z0), (x, vy1 + 0.15, lz), (x, vy1, lz)], (n, 0, 0))
    face_n(m, "AC_LPlaster", [(l0, vy1, lz), (l1, vy1, lz), (l1, vy1 + 0.15, lz), (l0, vy1 + 0.15, lz)], (0, 0, -1))
    # vestibule south wall with the glazed double door to the hairdresser's
    w = _Wall((vx0, vy1), (1.0, 0.0))
    d0, d1 = vy1 - SIDE_DOOR[1], vy1 - SIDE_DOOR[0]
    finish_wall(m, w, vy1 - vy0, [(d0, d1, Z0, SIDE_DOOR[2])])
    door_in_wall(m, w, d0, d1, SIDE_DOOR[2], leaves=2, glazed=True)
    # floors, ceilings
    floor(m, "AC_LParquet", hx0, hx1, hy0, hy1)
    floor(m, "AC_LStone", vx0, vx1, vy0, vy1)
    floor(m, "AC_LParquet", vx1, hx0, ARCH_Y[0], ARCH_Y[1])
    cornice(m, hx0, hx1, hy0, hy1)
    cornice(m, vx0, vx1, vy0, vy1)


def column(m):
    x0, x1, y0, y1 = COLUMN
    bx(m, "AC_LDado", x0, x1, y0, y1, Z0, DADO, faces="xXyY")
    bx(m, "AC_LWall", x0, x1, y0, y1, DADO, ZC - 0.3, faces="xXyY")
    F.bx(m, WOODD, x0 - 0.02, x1 + 0.02, y0 - 0.02, y1 + 0.02, DADO - 0.035, DADO + 0.015, faces="xXyYzZ")
    bx(m, "AC_LPlaster", x0 - 0.04, x1 + 0.04, y0 - 0.04, y1 + 0.04, ZC - 0.3, ZC - 0.18, faces="xXyYz")
    bx(m, "AC_LPlaster", x0 - 0.09, x1 + 0.09, y0 - 0.09, y1 + 0.09, ZC - 0.18, ZC, faces="xXyYz")
    fp("column", x0, x1, y0, y1)


def opening_liners(m):
    """Inward-facing liners of the east-wall openings through the wall (as gastronom.opening_liners),
    with stone thresholds at the doors."""
    for (cx, sill, wd, ht), is_door in [(ENTRANCE, True), (HALL_DOOR, True)] + [(w, False) for w in SHOP_WINDOWS]:
        u0, u1, z0, z1 = cx - wd / 2, cx + wd / 2, sill, sill + ht
        face_n(m, "AC_Trim", [(u0, YO, z0), (u0, YI, z0), (u0, YI, z1), (u0, YO, z1)], (1, 0, 0))
        face_n(m, "AC_Trim", [(u1, YO, z0), (u1, YI, z0), (u1, YI, z1), (u1, YO, z1)], (-1, 0, 0))
        face_n(m, "AC_Trim", [(u0, YO, z1), (u1, YO, z1), (u1, YI, z1), (u0, YI, z1)], (0, 0, -1))
        if is_door:
            face_n(m, "AC_Trim", [(u0, YO, z0), (u1, YO, z0), (u1, DOOR_LEAF_Y, z0), (u0, DOOR_LEAF_Y, z0)], (0, 0, 1))
            face_n(m, "AC_LStone", [(u0, DOOR_LEAF_Y, Z0), (u1, DOOR_LEAF_Y, Z0), (u1, YI, Z0), (u0, YI, Z0)], (0, 0, 1))
            face_n(m, "AC_LStone", [(u0, DOOR_LEAF_Y, z0), (u1, DOOR_LEAF_Y, z0), (u1, DOOR_LEAF_Y, Z0),
                                    (u0, DOOR_LEAF_Y, Z0)], (0, -1, 0))
        else:
            face_n(m, "AC_Trim", [(u0, YO, z0), (u1, YO, z0), (u1, YI, z0), (u0, YI, z0)], (0, 0, 1))


def backs(m, t=0.15):
    """Outward plaster backs of the interior walls bordering unbuilt spaces (as gastronom.backs)."""
    hx0, hx1, hy0, hy1 = HALL
    vx0, vx1, vy0, vy1 = VEST
    l0, l1, lz = LOBBY_DOOR
    for pts, n in (
        ([(vx0 - t, vy0, Z0), (vx0 - t, vy1, Z0), (vx0 - t, vy1, ZC), (vx0 - t, vy0, ZC)], (-1, 0, 0)),
        ([(vx0, vy1 + t, Z0), (l0, vy1 + t, Z0), (l0, vy1 + t, ZC), (vx0, vy1 + t, ZC)], (0, 1, 0)),
        ([(l0, vy1 + t, lz), (l1, vy1 + t, lz), (l1, vy1 + t, ZC), (l0, vy1 + t, ZC)], (0, 1, 0)),
        ([(l1, vy1 + t, Z0), (vx1, vy1 + t, Z0), (vx1, vy1 + t, ZC), (l1, vy1 + t, ZC)], (0, 1, 0)),
        ([(vx1, vy1 + t, Z0), (hx0, vy1 + t, Z0), (hx0, vy1 + t, ZC), (vx1, vy1 + t, ZC)], (0, 1, 0)),
        ([(hx0 - t, vy1 + t, Z0), (hx0 - t, hy1 + t, Z0), (hx0 - t, hy1 + t, ZC), (hx0 - t, vy1 + t, ZC)], (-1, 0, 0)),
        ([(hx0 - t, hy1 + t, Z0), (hx1 + t, hy1 + t, Z0), (hx1 + t, hy1 + t, ZC), (hx0 - t, hy1 + t, ZC)], (0, 1, 0)),
        ([(hx1 + t, hy0, Z0), (hx1 + t, hy1 + t, Z0), (hx1 + t, hy1 + t, ZC), (hx1 + t, hy0, ZC)], (1, 0, 0)),
    ):
        face_n(m, "AC_LPlaster", pts, n)


def windows_inside(m):
    """Deep white window boards for the plants, low cast-iron radiators, wide white architraves on the
    inner face round the windows and both doors (like the street side: ref dima/shop.jpg)."""
    for cx, sill, wd, ht in SHOP_WINDOWS:
        bx(m, "AC_LPaintWhite", cx - wd / 2 - 0.06, cx + wd / 2 + 0.06, YI - 0.25, YI + 0.3, sill - 0.035, sill + 0.006)
        bx(m, "AC_LPaintWhite", cx - 0.75, cx + 0.75, YI + 0.02, YI + 0.12, Z0 + 0.1, sill - 0.1, faces="xXYZ")
        for k in range(13):
            fx = cx - 0.72 + k * 0.111
            bx(m, "AC_LPaintWhite", fx, fx + 0.06, YI + 0.12, YI + 0.16, Z0 + 0.12, sill - 0.12, faces="xXYZ")
        _architrave(m, cx - wd / 2, cx + wd / 2, sill - 0.035, sill + ht, 0.13, bottom=False)
    for cx, sill, wd, ht in (ENTRANCE, HALL_DOOR):
        _architrave(m, cx - wd / 2, cx + wd / 2, Z0, sill + ht, 0.22)


def _architrave(m, x0, x1, z0, z1, t, bottom=False):
    p = 0.03
    for a, b in ((x0 - t, x0), (x1, x1 + t)):
        bx(m, "AC_LPaintWhite", a, b, YI, YI + p, z0, z1 + t, faces="xXYZ")
    bx(m, "AC_LPaintWhite", x0 - t, x1 + t, YI, YI + p, z1, z1 + t, faces="xXYzZ")
    # a step in the profile
    for a, b in ((x0 - t * 0.35, x0), (x1, x1 + t * 0.35)):
        bx(m, "AC_LPaintWhite", a, b, YI + p, YI + p + 0.012, z0, z1 + t * 0.35, faces="xXYZ")
    bx(m, "AC_LPaintWhite", x0 - t * 0.35, x1 + t * 0.35, YI + p, YI + p + 0.012, z1, z1 + t * 0.35, faces="xXYz")


def board(m, key, x, y, z, w, h, n, frame=0.03):
    """Dark green glass board on a wall facing n = (nx, ny): centre (x, y, z) on the wall face."""
    nx, ny = n
    t = 0.02
    ux, uy = -ny, nx

    def P(a, b, c):
        return (x + ux * a + nx * b, y + uy * a + ny * b, z + c)

    face_n(m, "AC_LBoard", [P(-w / 2, t, -h / 2), P(w / 2, t, -h / 2), P(w / 2, t, h / 2), P(-w / 2, t, h / 2)], (nx, ny, 0))
    for a0, a1, c0, c1 in ((-w / 2 - frame, w / 2 + frame, h / 2, h / 2 + frame),
                           (-w / 2 - frame, w / 2 + frame, -h / 2 - frame, -h / 2),
                           (-w / 2 - frame, -w / 2, -h / 2, h / 2), (w / 2, w / 2 + frame, -h / 2, h / 2)):
        p0, p1 = P(a0, 0, c0), P(a1, t + 0.008, c1)
        bx(m, BRASS, p0[0], p1[0], p0[1], p1[1], p0[2], p1[2])
    if key:
        c = P(0, t + 0.001, 0)
        LABELS.append((key, round(c[0], 4), round(c[1], 4), round(c[2], 4), w, h))
        LABEL_NORMALS.append((float(nx), float(ny), 0.0))


# --- books ----------------------------------------------------------------------------------------------
def books(m, rnd, x0, x1, yf, depth, z, hmax, fill=1.0, eye=1.75):
    """A shelf of books, spines at the front line yf facing -y, between x0 and x1, standing on z.
    Cheap: each book is a spine (two quads with a ridge for the sets, so the volumes of a set read
    apart) and a top quad (left out above eye height); side faces only where the neighbour differs
    in height or there is a gap. Sets of collected works (same colour and height), single volumes,
    gaps, a leaning book at the end of a run, flat stacks."""
    items = []                                  # (xa, xb, h, mat, inset, depth)
    end = x0 + (x1 - x0) * fill - rnd.uniform(0.0, 0.05)
    x = x0 + rnd.uniform(0.0, 0.02)
    lean_at = None
    while x < end - 0.02:
        r = rnd.random()
        if r < 0.05 and items:                  # a gap (a borrowed book)
            x += rnd.uniform(0.04, 0.14)
            continue
        if r < 0.09 and end - x > 0.3:          # a flat stack
            w = rnd.uniform(0.17, 0.25)
            zz = z
            for _ in range(rnd.randint(2, 4)):
                t = rnd.uniform(0.02, 0.045)
                if zz + t > z + hmax:
                    break
                F.bx(m, rnd.choice(BOOK_MATS), x, x + w, yf + rnd.uniform(0.0, 0.02), yf + depth * 0.8, zz, zz + t, faces="yxXZ")
                zz += t
            x += w + 0.01
            continue
        if r < 0.45:                            # a set of collected works
            n, mat = rnd.randint(4, 14), rnd.choice(BOOK_MATS)
            bw, h = rnd.uniform(0.025, 0.04), hmax * rnd.uniform(0.62, 0.95)
            dd = depth * rnd.uniform(0.7, 0.92)
            for _ in range(n):
                if x + bw > end:
                    break
                items.append((x, x + bw - 0.003, h + rnd.uniform(-0.004, 0.004), mat, rnd.uniform(0.0, 0.004), dd, True))
                x += bw
        else:                                   # odd volumes
            for _ in range(rnd.randint(1, 5)):
                bw = rnd.uniform(0.015, 0.055)
                if x + bw > end:
                    break
                items.append((x, x + bw - 0.0015, hmax * rnd.uniform(0.5, 0.98), rnd.choice(BOOK_MATS),
                              rnd.uniform(0.0, 0.012), depth * rnd.uniform(0.6, 0.95), False))
                x += bw
        x += rnd.uniform(0.0, 0.004)
    if items and x1 - items[-1][1] > 0.12 and rnd.random() < 0.6:
        lean_at = items[-1]
    for k, (xa, xb, h, mat, ins, dd, rnd_sp) in enumerate(items):
        y0 = yf + ins
        if rnd_sp:                                                                              # rounded spine: a ridge
            xm, yr = (xa + xb) / 2, y0 - min(0.008, (xb - xa) * 0.3)
            m.face(mat, [(xa, y0, z), (xm, yr, z), (xm, yr, z + h), (xa, y0, z + h)])
            m.face(mat, [(xm, yr, z), (xb, y0, z), (xb, y0, z + h), (xm, yr, z + h)])
        else:
            m.face(mat, [(xa, y0, z), (xb, y0, z), (xb, y0, z + h), (xa, y0, z + h)])            # spine (faces -y)
        if z + h < eye:                                                                         # top (seen from above)
            m.face(mat, [(xa, y0, z + h), (xb, y0, z + h), (xb, y0 + dd, z + h), (xa, y0 + dd, z + h)])
        prev = items[k - 1] if k > 0 else None
        nxt = items[k + 1] if k + 1 < len(items) else None
        if prev is None or xa - prev[1] > 0.0045 or prev[2] < h - 0.006:
            F.face_n(m, mat, [(xa, y0, z), (xa, y0, z + h), (xa, y0 + dd, z + h), (xa, y0 + dd, z)], (-1, 0, 0))
        if nxt is None or nxt[0] - xb > 0.0045 or nxt[2] < h - 0.006:
            F.face_n(m, mat, [(xb, y0, z), (xb, y0 + dd, z), (xb, y0 + dd, z + h), (xb, y0, z + h)], (1, 0, 0))
    if lean_at is not None:                     # one book leaning on the last of the run
        xa, xb, h, mat, ins, dd, _ = lean_at
        a = rnd.uniform(0.25, 0.42)
        bw, hh = rnd.uniform(0.022, 0.035), h * rnd.uniform(0.85, 1.0)
        cx = xb + math.sin(a) * hh / 2 + math.cos(a) * bw / 2 + 0.002
        cz = z + math.cos(a) * hh / 2 + math.sin(a) * bw / 2
        F.obox(m, rnd.choice(BOOK_MATS), (cx, yf + 0.01 + dd / 2, cz), (math.cos(a), 0, math.sin(a)), (0, 1, 0),
               (-math.sin(a), 0, math.cos(a)), bw / 2, dd / 2, hh / 2, faces="xXyzZ")


# --- bookcases --------------------------------------------------------------------------------------------
PLINTH, CORN, SHELF_T, POST = 0.1, 0.12, 0.025, 0.03


def _sections(w):
    return max(1, int(round(w / 0.95)))


def case_slots(w, d, h, nshelf):
    """Shelf runs of an open case (furniture convention, front at -d/2): [(xa, xb, yf, z, depth, clear)]."""
    ztop = h - CORN
    pitch = (ztop - PLINTH) / nshelf
    n = _sections(w)
    sw = w / n
    out = []
    for k in range(n):
        a = -w / 2 + k * sw + POST / 2 + (POST / 2 if k == 0 else 0.0)
        b = -w / 2 + (k + 1) * sw - POST / 2 - (POST / 2 if k == n - 1 else 0.0)
        for i in range(nshelf):
            z = PLINTH + i * pitch + SHELF_T
            top = PLINTH + (i + 1) * pitch if i + 1 < nshelf else ztop - SHELF_T
            out.append((a, b, -d / 2, z, d - 0.03, top - z))
    return out


def case_half(rnd, w, d, h, nshelf, fill=0.95):
    """Open shelves (no cornice) facing -y: recessed plinth, dark back boards, posts with pilaster
    strips, shelves; books on every shelf."""
    m = Mesh()
    x0, x1, y0, y1 = -w / 2, w / 2, -d / 2, d / 2
    ztop = h - CORN
    n = _sections(w)
    sw = w / n
    F.bx(m, WOODD, x0 + 0.02, x1 - 0.02, y0 + 0.03, y1, 0.0, PLINTH, faces="yxXZ")
    F.bx(m, WOODD, x0, x1, y1 - 0.012, y1, PLINTH, ztop, faces="y")
    for k in range(n + 1):
        xa = min(max(x0 + k * sw - POST / 2, x0), x1 - POST)
        F.bx(m, WOOD, xa, xa + POST, y0, y1, 0.0 if k in (0, n) else PLINTH, ztop, faces="xXy")
        F.bx(m, WOOD, xa - 0.008, xa + POST + 0.008, y0 - 0.012, y0, PLINTH, ztop, faces="xXy")      # pilaster strip
    F.bx(m, WOOD, x0, x1, y0, y1, ztop - SHELF_T, ztop, faces="yz")
    F.bx(m, WOOD, x0, x1, y0 - 0.012, y0, ztop - 0.09, ztop, faces="yz")                              # frieze
    for (a, b, yf, z, depth, clear) in case_slots(w, d, h, nshelf):
        F.bx(m, WOOD, a, b, yf, y1 - 0.012, z - SHELF_T, z, faces="yzZ")
        books(m, rnd, a + 0.004, b - 0.004, yf + 0.012, depth - 0.02, z, clear - 0.025,
              fill=fill * rnd.uniform(0.8, 1.0) if rnd.random() < 0.3 else fill)
    return m


def wall_case(rnd, w, d, h, nshelf):
    m = case_half(rnd, w, d, h, nshelf)
    ztop = h - CORN
    F.moulded_top(m, WOOD, -w / 2, w / 2, -d / 2 - 0.012, d / 2, ztop, F.cornice_prof(CORN, 0.06), sides=3)
    F.bx(m, WOODD, -w / 2, w / 2, -d / 2 - 0.018, -d / 2 + 0.03, 0.0, 0.08, faces="yxXZ")             # skirting
    return m


def stack(rnd, w, d, h, nshelf):
    """Free-standing double-sided stack: two open halves back to back under one moulded top, solid
    end panels."""
    m = Mesh()
    m.extend(F.moved(case_half(rnd, w, d / 2, h, nshelf), 0.0, -d / 4))
    m.extend(F.moved(case_half(rnd, w, d / 2, h, nshelf), 0.0, d / 4, yaw=math.pi))
    ztop = h - CORN
    for s in (-1, 1):                                                                             # end panels
        x = s * (w / 2 + 0.006)
        F.bx(m, WOOD, x - 0.012, x + 0.012, -d / 2 - 0.012, d / 2 + 0.012, 0.0, ztop, faces="xXyYz")
    F.moulded_top(m, WOOD, -w / 2 - 0.018, w / 2 + 0.018, -d / 2 - 0.012, d / 2 + 0.012, ztop, F.cornice_prof(CORN, 0.05), sides=4)
    F.bx(m, WOODD, -w / 2 - 0.02, w / 2 + 0.02, -d / 2 - 0.018, d / 2 + 0.018, 0.0, 0.08, faces="yYxXZ")
    return m


def _case_slots_world(row):
    name, kind, cx, cy, yaw, w, d, h, nshelf = row
    f = Frame(cx, cy, yaw, Z0)
    runs = []
    if kind == "wall":
        runs = [(s, (0.0, 0.0, 0.0)) for s in case_slots(w, d, h, nshelf)]
    else:
        runs = ([(s, (0.0, -d / 4, 0.0)) for s in case_slots(w, d / 2, h, nshelf)]
                + [(s, (0.0, d / 4, math.pi)) for s in case_slots(w, d / 2, h, nshelf)])
    out = []
    for (a, b, yf, z, depth, clear), (ox, oy, ry) in runs:
        c, s_ = math.cos(ry), math.sin(ry)
        pa = (ox + a * c - yf * s_, oy + a * s_ + yf * c, z)
        pb = (ox + b * c - yf * s_, oy + b * s_ + yf * c, z)
        wa, wb = f(pa), f(pb)
        tot = yaw + ry
        out.append((name, wa, wb, depth, clear, (math.sin(tot), -math.cos(tot))))
    return out


def _book_slots():
    slots = []
    for row in BOOKCASES:
        for name, wa, wb, depth, clear, (nx, ny) in _case_slots_world(row):
            (x0, y0, z), (x1, y1, _) = wa, wb
            if (x1, y1) < (x0, y0):
                x0, y0, x1, y1 = x1, y1, x0, y0
            slots.append({"id": "lib.shelf.%d" % len(slots), "case": name,
                          "x0": round(x0, 4), "x1": round(x1, 4), "y0": round(y0, 4), "y1": round(y1, 4),
                          "y": round((y0 + y1) / 2, 4), "z": round(z, 4), "depth": round(depth, 4),
                          "height": round(clear, 4), "facing": (round(nx, 4) + 0.0, round(ny, 4) + 0.0)})
    return slots


BOOK_SLOTS = _book_slots()


def bookcases(m, rnd):
    for row in BOOKCASES:
        name, kind, cx, cy, yaw, w, d, h, nshelf = row
        r = random.Random(sum(map(ord, name)) * 7 + 3)
        part = wall_case(r, w, d, h, nshelf) if kind == "wall" else stack(r, w, d, h, nshelf)
        put(m, part, cx, cy, Z0, yaw, name=name)
    # plant stands (жардиньерки) in the gaps of the back run
    for x in PLANT_STANDS_X:
        s = Mesh()
        F.lathe(s, WOODD, [(0, 0), (0.16, 0), (0.16, 0.03), (0.06, 0.06), (0.035, 0.12), (0.03, 0.7), (0.05, 0.76),
                           (0.05, 0.8), (0.17, 0.82), (0.17, 0.85), (0, 0.85)], 10)
        put(m, s, x, HALL[3] - 0.25, Z0, 0.0, name="stand")


# --- vestibule ----------------------------------------------------------------------------------------------
def catalogue(rnd, w=1.3, d=0.45, cols=6, rows=8, pull=True):
    """Card-catalogue cabinet on a turned-leg stand: a grid of card drawers with small brass cup
    handles and label holders; one drawer pulled out with its cards."""
    m = Mesh()
    for sx in (-1, 1):
        for sy in (-1, 1):
            F.turned_leg(m, WOODD, sx * (w / 2 - 0.06), sy * (d / 2 - 0.06), 0.55, 0.024, 6, "taper")
    F.bx(m, WOODD, -w / 2 + 0.03, w / 2 - 0.03, -d / 2 + 0.03, d / 2 - 0.03, 0.47, 0.55, faces="xXyY")
    zb, zt = 0.55, 1.42
    F.bx(m, WOOD, -w / 2, w / 2, -d / 2, d / 2, zb, zt, faces="xXYz")
    F.bx(m, WOOD, -w / 2, w / 2, -d / 2 - 0.004, -d / 2 + 0.01, zb, zb + 0.03, faces="yZ")
    F.bx(m, WOOD, -w / 2, -w / 2 + 0.03, -d / 2 - 0.004, -d / 2 + 0.01, zb, zt, faces="yxX")
    F.bx(m, WOOD, w / 2 - 0.03, w / 2, -d / 2 - 0.004, -d / 2 + 0.01, zb, zt, faces="yxX")
    y0 = -d / 2 + 0.004
    F.face_n(m, WOODD, [(-w / 2 + 0.03, y0 + 0.006, zb + 0.03), (w / 2 - 0.03, y0 + 0.006, zb + 0.03),
                        (w / 2 - 0.03, y0 + 0.006, zt), (-w / 2 + 0.03, y0 + 0.006, zt)], (0, -1, 0))
    dw = (w - 0.06) / cols
    dh = (zt - zb - 0.03) / rows
    pulled = (rnd.randrange(cols), rnd.randrange(2, rows - 2)) if pull else None
    for c in range(cols):
        for r in range(rows):
            a = -w / 2 + 0.03 + c * dw
            z0 = zb + 0.03 + r * dh
            out = 0.24 if (c, r) == pulled else 0.0
            yf = y0 - out
            F.bx(m, WOOD, a + 0.004, a + dw - 0.004, yf - 0.012, yf + 0.006, z0 + 0.004, z0 + dh - 0.004, faces="yxXzZ")
            cx = a + dw / 2
            F.bx(m, BRASS, cx - 0.032, cx + 0.032, yf - 0.016, yf - 0.012, z0 + dh * 0.55, z0 + dh * 0.82, faces="y")
            F.face_n(m, PAPER, [(cx - 0.026, yf - 0.0165, z0 + dh * 0.58), (cx + 0.026, yf - 0.0165, z0 + dh * 0.58),
                                (cx + 0.026, yf - 0.0165, z0 + dh * 0.79), (cx - 0.026, yf - 0.0165, z0 + dh * 0.79)], (0, -1, 0))
            F.bx(m, BRASS, cx - 0.014, cx + 0.014, yf - 0.03, yf - 0.012, z0 + dh * 0.2, z0 + dh * 0.36, faces="xXyzZ")
            if out:
                F.bx(m, WOOD, a + 0.008, a + dw - 0.008, yf + 0.006, y0, z0 + 0.008, z0 + dh - 0.03, faces="xXZ")
                for k in range(22):
                    yy = yf + 0.02 + k * 0.01
                    F.bx(m, PAPER, a + 0.014, a + dw - 0.014, yy, yy + 0.002, z0 + 0.01, z0 + dh - 0.03 + (0.01 if k % 5 == 0 else 0.0),
                         faces="yZ")
    F.moulded_top(m, WOOD, -w / 2, w / 2, -d / 2 - 0.004, d / 2, zt, F.cornice_prof(0.05, 0.025, kind="thumb"), sides=3)
    return m


def abacus():
    """Счёты: a wooden frame with ten wires of ten beads (two dark beads in the middle of each)."""
    m = Mesh()
    W, D = 0.3, 0.22
    for s in (-1, 1):
        F.bx(m, WOOD, s * W / 2 - 0.012, s * W / 2 + 0.012, -D / 2, D / 2, 0.0, 0.025)
        F.bx(m, WOOD, -W / 2, W / 2, s * D / 2 - 0.012, s * D / 2 + 0.012, 0.0, 0.025)
    for k in range(10):
        y = -D / 2 + 0.02 + k * (D - 0.04) / 9
        F.bx(m, "AC_LIron", -W / 2, W / 2, y - 0.0015, y + 0.0015, 0.0115, 0.0145, faces="Z")
        left = 3 + (k * 7) % 5
        for j in range(10):
            x = -W / 2 + 0.03 + j * 0.019 + (0.05 if j >= left else 0.0)
            mat = "AC_LBlack" if j in (4, 5) else "AC_LUphRed" if j < left else WOOD
            F.bx(m, mat, x, x + 0.017, y - 0.009, y + 0.009, 0.005, 0.021, faces="xXyYZ")
    return m


def date_stamp():
    m = Mesh()
    F.bx(m, "AC_LIron", -0.03, 0.03, -0.02, 0.02, 0.0, 0.03)
    F.lathe(m, WOODD, [(0, 0.03), (0.012, 0.03), (0.012, 0.08), (0.022, 0.1), (0.018, 0.12), (0, 0.125)], 6)
    F.bx(m, "AC_LIron", 0.06, 0.15, -0.04, 0.04, 0.0, 0.012)                 # ink pad
    F.bx(m, "AC_LBookNavy", 0.065, 0.145, -0.035, 0.035, 0.012, 0.013, faces="Z")
    return m


def card_tray(rnd, L=0.36):
    """A long open wooden tray of catalogue / reader's cards."""
    m = Mesh()
    F.bx(m, WOOD, -0.065, 0.065, -L / 2, L / 2, 0.0, 0.07, faces="xXyYZ")
    F.bx(m, WOODD, -0.055, 0.055, -L / 2 + 0.01, L / 2 - 0.01, 0.06, 0.0605, faces="Z")
    for k in range(int((L - 0.04) / 0.012)):
        y = -L / 2 + 0.02 + k * 0.012
        top = 0.095 + (0.012 if rnd.random() < 0.15 else 0.0)
        F.bx(m, PAPER, -0.05, 0.05, y, y + 0.002, 0.03, top, faces="yYZ")
    return m


def issue_desk(rnd):
    """Кафедра выдачи: a panelled counter (1.05 m) for the readers on the -y side, the librarian's
    baize-topped working desk behind it with a banker's lamp, an abacus, the date stamp, card trays,
    a stack of returned books; her bentwood chair."""
    m = Mesh()
    w, d = 1.2, 0.75
    yf = -d / 2
    F.bx(m, WOODD, -w / 2, w / 2, yf + 0.02, yf + 0.06, 0.0, 0.08, faces="yxXZ")
    F.bx(m, WOOD, -w / 2, w / 2, yf, yf + 0.045, 0.08, 1.02, faces="yxXZ")
    for a, b in ((-w / 2 + 0.06, -0.02), (0.02, w / 2 - 0.06)):
        F.panel_door(m, WOOD, a, b, 0.16, 0.94, yf - 0.02, 0.02, 0.06)
    F.bx(m, WOOD, -w / 2 - 0.03, w / 2 + 0.03, yf - 0.06, yf + 0.3, 1.02, 1.05)                         # reader's ledge
    for s in (-1, 1):
        F.bx(m, WOOD, s * w / 2 - 0.03, s * w / 2, yf, d / 2, 0.0, 0.76, faces="xXyYZ")
    F.bx(m, WOOD, -w / 2, w / 2, yf + 0.045, d / 2, 0.725, 0.76, faces="yYZz")
    F.face_n(m, "AC_LBaize", [(-w / 2 + 0.04, yf + 0.32, 0.7605), (w / 2 - 0.04, yf + 0.32, 0.7605),
                              (w / 2 - 0.04, d / 2 - 0.04, 0.7605), (-w / 2 + 0.04, d / 2 - 0.04, 0.7605)], (0, 0, 1))
    # drawer pedestal on the librarian's side
    F.bx(m, WOODD, w / 2 - 0.45, w / 2 - 0.03, yf + 0.06, d / 2 - 0.01, 0.0, 0.725, faces="xY")
    for k in range(3):
        z0 = 0.06 + k * 0.22
        F.bx(m, WOOD, w / 2 - 0.43, w / 2 - 0.05, d / 2 - 0.01, d / 2 + 0.006, z0, z0 + 0.2, faces="YxXzZ")
        F.bx(m, BRASS, w / 2 - 0.27, w / 2 - 0.21, d / 2 + 0.006, d / 2 + 0.02, z0 + 0.13, z0 + 0.15, faces="YxXzZ")
    # on the reader's ledge: returned books; a card tray
    F.add(m, F.books_stack(rnd, 4), -0.32, yf + 0.1, 1.05, 0.2)
    F.add(m, card_tray(rnd, 0.3), 0.3, yf + 0.12, 1.05, math.pi / 2)
    # on the working top
    zt = 0.7605
    F.add(m, F.desk_lamp("banker"), -0.42, d / 2 - 0.2, zt, math.pi)
    F.add(m, abacus(), 0.05, 0.16, zt, math.pi + 0.12)
    F.add(m, date_stamp(), -0.2, 0.05, zt, 0.4)
    F.add(m, card_tray(rnd), 0.38, 0.12, zt)
    F.add(m, card_tray(rnd), 0.52, 0.12, zt)
    for k in range(3):                                                                               # reader's forms
        F.yawbox(m, PAPER, rnd.uniform(-0.25, 0.1), rnd.uniform(-0.05, 0.25), 0.15, 0.21, zt + 0.001 * k, zt + 0.001 * (k + 1),
                 rnd.uniform(-0.6, 0.6), faces="Z")
    F.add(m, F.bentwood_chair(rnd), 0.05, d / 2 + 0.3, 0.0, 0.0)
    return m


def counter(rnd, L=2.8, d=0.36, h=1.0):
    """Cloakroom counter (гардероб): a panelled oak barrier with a top, a lift-up flap at the left."""
    m = Mesh()
    F.bx(m, WOODD, -L / 2, L / 2, -d / 2 + 0.02, d / 2, 0.0, 0.08, faces="yxXZ")
    F.bx(m, WOOD, -L / 2, L / 2, -d / 2 + 0.02, d / 2, 0.08, h - 0.04, faces="yYxXZ")
    n = 4
    for k in range(n):
        a = -L / 2 + 0.06 + k * (L - 0.12) / n
        F.panel_door(m, WOOD, a + 0.02, a + (L - 0.12) / n - 0.02, 0.14, h - 0.12, -d / 2, 0.02, 0.06)
    F.bx(m, WOOD, -L / 2 - 0.03, L / 2 + 0.03, -d / 2 - 0.04, d / 2 + 0.02, h - 0.04, h)
    F.bx(m, WOOD, -L / 2 - 0.03, -L / 2 + 0.55, -d / 2 - 0.04, d / 2 + 0.02, h, h + 0.02)   # the flap, folded
    for k in range(6):                                                                     # numbered tokens on the top
        F.bx(m, BRASS, 0.5 + k * 0.07, 0.54 + k * 0.07, -0.05, -0.01, h, h + 0.003, faces="Z")
    return m


def noticeboard(rnd, w=1.1, h=0.85):
    """Cork noticeboard in an oak frame with pinned sheets (blank: the game may print on them).
    Back at +y, bottom at z=0."""
    m = Mesh()
    F.bx(m, WOOD, -w / 2, w / 2, 0.0, 0.03, 0.0, h, faces="yxXzZ")
    F.face_n(m, "AC_LCork", [(-w / 2 + 0.04, -0.001, 0.04), (w / 2 - 0.04, -0.001, 0.04), (w / 2 - 0.04, -0.001, h - 0.04),
                             (-w / 2 + 0.04, -0.001, h - 0.04)], (0, -1, 0))
    x = -w / 2 + 0.07
    while x < w / 2 - 0.2:
        sw, sh = rnd.uniform(0.15, 0.24), rnd.uniform(0.2, 0.3)
        z = rnd.uniform(0.07, h - 0.07 - sh)
        F.face_n(m, rnd.choice([PAPER, PAPER, "AC_LNewsprint"]), [(x, -0.003, z), (x + sw, -0.003, z), (x + sw, -0.003, z + sh), (x, -0.003, z + sh)],
                 (0, -1, 0))
        F.bx(m, "AC_LUphRed", x + sw / 2 - 0.006, x + sw / 2 + 0.006, -0.008, -0.003, z + sh - 0.02, z + sh - 0.008, faces="y")
        x += sw + rnd.uniform(0.03, 0.08)
    return m


def vestibule(m, rnd):
    vx0, vx1, vy0, vy1 = VEST
    # cloakroom: counter, coat racks with coats and hats on the back wall, a painting above
    cx, cy, L = CLOAK
    put(m, counter(rnd, L), cx, cy, Z0, 0.0, name="cloak_counter")
    for k, x in enumerate((-1.9, -0.9)):
        put(m, F.coat_rack(random.Random(31 + k), 0.95), x, vy1 - 0.16, Z0, 0.0, name="coats")
    # the issue desk facing the arch (+x)
    put(m, issue_desk(rnd), ISSUE_DESK[0], ISSUE_DESK[1], Z0, math.pi / 2, name="issue_desk")
    # card catalogues
    for k, (x, y, yaw, w) in enumerate(CATALOGUES):
        put(m, catalogue(random.Random(71 + k), w, pull=(k == 0)), x, y, Z0, yaw, name="catalogue")
    # noticeboard on the arch wall (facing -x), between the east wall and the hours board
    put(m, noticeboard(rnd), vx1 - 0.03, -6.0, Z0 + 0.95, -math.pi / 2)
    # runners: entrance -> desk, desk -> arch -> hall
    put(m, F.runner(random.Random(5), 1.65, 0.9), 0.0, -5.72, Z0, math.pi / 2)
    put(m, F.runner(random.Random(6), 3.75, 0.9), 2.33, ARCH_Y[0] + 1.2, Z0, 0.0)
    # a big oil painting over the coats
    put(m, F.picture(random.Random(8), 1.25, 0.8, "oil", "winter_village"), -1.4, vy1 - 0.015, Z0 + 2.45, 0.0)


# --- hall -------------------------------------------------------------------------------------------------
def open_book(rnd):
    m = Mesh()
    w, d = 0.16, 0.23
    mat = rnd.choice(BOOK_MATS)
    F.bx(m, mat, -w - 0.01, w + 0.01, -d / 2 - 0.005, d / 2 + 0.005, 0.0, 0.004, faces="xXyYZ")
    for s in (-1, 1):
        F.face_n(m, PAPER, [(0, -d / 2, 0.006), (s * w, -d / 2, 0.018), (s * w, d / 2, 0.018), (0, d / 2, 0.006)], (0, 0, 1))
        F.face_n(m, PAPER, [(s * w, -d / 2, 0.004), (s * w, d / 2, 0.004), (s * w, d / 2, 0.018), (s * w, -d / 2, 0.018)], (s, 0, 0))
    return m


def newspaper(rnd):
    m = Mesh()
    F.yawbox(m, "AC_LNewsprint", 0, 0, 0.42, 0.3, 0.0, 0.004, 0.0, faces="ZxXyY")
    return m


def reading_table(rnd):
    """A long reading table (2.0 x 0.9, along local x) with a green baize top, turned legs, two green
    banker's lamps on the centre line, three bentwood chairs a side, open books."""
    m = Mesh()
    L, W, H = TABLE_L, TABLE_W, TABLE_H
    F.cbox(m, WOODD, -L / 2, L / 2, -W / 2, W / 2, H - 0.045, H, 0.008)
    F.face_n(m, "AC_LBaize", [(-L / 2 + 0.05, -W / 2 + 0.05, H + 0.0008), (L / 2 - 0.05, -W / 2 + 0.05, H + 0.0008),
                              (L / 2 - 0.05, W / 2 - 0.05, H + 0.0008), (-L / 2 + 0.05, W / 2 - 0.05, H + 0.0008)], (0, 0, 1))
    F.bx(m, WOODD, -L / 2 + 0.06, L / 2 - 0.06, -W / 2 + 0.06, W / 2 - 0.06, H - 0.15, H - 0.045, faces="xXyY")
    for sx in (-1, 1):
        for sy in (-1, 1):
            F.turned_leg(m, WOODD, sx * (L / 2 - 0.09), sy * (W / 2 - 0.09), H - 0.045, 0.032, 6, "baluster")
    F.bx(m, WOODD, -L / 2 + 0.1, L / 2 - 0.1, -0.02, 0.02, 0.12, 0.16, faces="yYzZ")                      # stretcher
    for k, x in enumerate((-0.5, 0.5)):
        F.add(m, F.desk_lamp("banker"), x, 0.04 if k else -0.04, H, 0.0 if k else math.pi)
    for side in (-1, 1):
        for k, x in enumerate((-0.6, 0.0, 0.6)):
            jitter = rnd.uniform(-0.05, 0.12)
            F.add(m, F.bentwood_chair(rnd), x + rnd.uniform(-0.04, 0.04), side * (W / 2 + 0.17 + jitter), 0.0,
                  (0.0 if side > 0 else math.pi) + rnd.uniform(-0.12, 0.12))
            if rnd.random() < 0.45:
                F.add(m, open_book(rnd), x, side * (W / 2 - 0.2), H + 0.001, (0.0 if side < 0 else math.pi) + rnd.uniform(-0.2, 0.2))
            elif rnd.random() < 0.3:
                F.add(m, F.books_stack(rnd, 2), x + 0.15, side * (W / 2 - 0.15), H + 0.001)
    return m


def round_table(rnd):
    """Round newspaper table on a pedestal with three bentwood chairs and today's papers."""
    m = Mesh()
    F.lathe(m, WOODD, [(0, 0), (0.3, 0), (0.3, 0.03), (0.08, 0.08), (0.05, 0.2), (0.07, 0.4), (0.05, 0.6), (0.06, 0.72),
                       (0.5, 0.72), (0.5, 0.76), (0, 0.76)], 16)
    for k in range(3):
        a = math.pi / 2 + k * 2 * math.pi / 3 + 0.3
        F.add(m, F.bentwood_chair(rnd), 0.72 * math.cos(a), 0.72 * math.sin(a), 0.0, a - math.pi / 2)
    for k in range(3):
        F.add(m, newspaper(rnd), rnd.uniform(-0.15, 0.15), rnd.uniform(-0.15, 0.15), 0.761 + 0.004 * k, rnd.uniform(0, 3))
    return m


def periodicals_rack(rnd, w=1.2, d=0.36, h=1.55):
    """Periodicals rack: sloping display shelves with magazines, a rail at the top with newspaper
    holders (подшивки) hanging over it."""
    m = Mesh()
    for s in (-1, 1):
        F.bx(m, WOOD, s * w / 2 - 0.025, s * w / 2, -d / 2, d / 2, 0.0, h)
    F.bx(m, WOODD, -w / 2, w / 2, d / 2 - 0.012, d / 2, 0.08, h - 0.05, faces="y")
    F.bx(m, WOODD, -w / 2, w / 2, -d / 2 + 0.02, d / 2, 0.0, 0.08, faces="yZ")
    a = math.radians(62)
    ax, az = (0, math.cos(a), math.sin(a)), (0, -math.sin(a), math.cos(a))
    for k in range(4):
        z0 = 0.15 + k * 0.3
        c = (0.0, -d / 2 + 0.06 + 0.12 * math.cos(a), z0 + 0.12 * math.sin(a))
        F.obox(m, WOOD, c, (1, 0, 0), ax, az, w / 2 - 0.025, 0.13, 0.008)
        F.bx(m, WOOD, -w / 2 + 0.025, w / 2 - 0.025, -d / 2 + 0.02, -d / 2 + 0.045, z0 - 0.01, z0 + 0.04)      # lip
        for j in range(3):
            mx = -w / 2 + 0.2 + j * 0.39
            mc = tuple(c[i] + az[i] * 0.013 + ax[i] * rnd.uniform(-0.01, 0.02) for i in range(3))
            F.obox(m, rnd.choice(BOOK_MATS + ["AC_LNewsprint", "AC_LNewsprint"]), (mx, mc[1], mc[2]), (1, 0, 0), ax, az,
                   0.105, 0.14, 0.004)
    zr = h - 0.06
    F.bx(m, WOOD, -w / 2, w / 2, -0.02, 0.02, zr - 0.02, zr + 0.02)
    for j in range(3):                                                                    # newspaper holders
        x = -w / 2 + 0.22 + j * 0.38
        F.bx(m, WOODD, x - 0.2, x + 0.2, -0.04, -0.02, zr - 0.01, zr + 0.025, faces="yzZxX")
        F.bx(m, "AC_LNewsprint", x - 0.18, x + 0.18, -0.045, -0.04, zr - 0.5, zr - 0.01, faces="yxXz")
    return m


def floor_globe():
    """A library floor globe on a turned three-legged stand with a brass meridian."""
    m = Mesh()
    for k in range(3):
        a = 2 * math.pi * k / 3
        F.tube(m, WOODD, (0.27 * math.cos(a), 0.27 * math.sin(a), 0.0), (0.12 * math.cos(a), 0.12 * math.sin(a), 0.55), 0.018, 0.014, 5)
    F.lathe(m, WOODD, [(0.0, 0.5), (0.3, 0.55), (0.3, 0.58), (0.0, 0.58)], 16)
    F.lathe(m, WOOD, [(0.27, 0.58), (0.3, 0.58), (0.3, 0.6), (0.27, 0.6)], 16)
    F.sphere(m, "AC_LGlobe", (0, 0, 0.88), 0.26, 16, 10)
    pts = [(0.28 * math.sin(t), 0.0, 0.88 - 0.28 * math.cos(t)) for t in [math.pi * k / 12 for k in range(13)]]
    F.chain(m, BRASS, [(p[0] * math.cos(0.41), p[0] * math.sin(0.41), p[2]) for p in pts], 0.007, 4)
    return m


def bust():
    """Plaster bust of a scientist on a small plinth (top of the bookcases), front -y."""
    m = Mesh()
    F.bx(m, "AC_LPlaster", -0.1, 0.1, -0.09, 0.09, 0.0, 0.08)
    F.lathe(m, "AC_LPlaster", [(0, 0.08), (0.17, 0.08), (0.17, 0.12), (0.14, 0.2), (0.08, 0.24), (0.05, 0.27), (0, 0.27)], 10, sy=0.6)
    F.sphere(m, "AC_LPlaster", (0, 0, 0.36), 0.095, 10, 6, 1.18)
    return m


def nook(m, rnd):
    x, y = NOOK
    put(m, F.rug(random.Random(9), 2.6, 1.5), x, y - 0.1, Z0, 0.0)
    for s in (-1, 1):
        put(m, F.leather_armchair(rnd), x + s * 0.7, y, Z0, s * 0.25, name="armchair")
    t = Mesh()
    F.lathe(t, WOODD, [(0, 0), (0.2, 0), (0.2, 0.02), (0.04, 0.05), (0.03, 0.55), (0.3, 0.56), (0.3, 0.6), (0, 0.6)], 14)
    t.extend(F.moved(F.table_lamp(None), 0.08, 0.06, 0.6))
    t.extend(F.moved(F.books_stack(rnd, 3), -0.12, -0.05, 0.6, 0.3))
    put(m, t, x, y + 0.1, Z0, 0.0, name="nook_table")
    put(m, floor_globe(), GLOBE_XY[0], GLOBE_XY[1], Z0, 0.3, name="globe")


def hall(m, rnd):
    hx0, hx1, hy0, hy1 = HALL
    for k, x in enumerate(TABLES_X):
        put(m, reading_table(random.Random(100 + k)), x, sum(TABLE_Y) / 2, Z0, math.pi / 2, name="reading_table")
    put(m, round_table(rnd), ROUND_TABLE[0], ROUND_TABLE[1], Z0, 0.2, name="newspaper_table")
    put(m, periodicals_rack(rnd), RACK[0], RACK[1], Z0, math.pi / 2, name="periodicals")
    nook(m, rnd)
    # runners along the main aisle (y -2.9..-2.0) and down the back aisle
    put(m, F.runner(random.Random(11), 12.9, 0.9), 10.8, -2.45, Z0, 0.0)
    put(m, F.runner(random.Random(12), 9.0, 0.8), 9.0, 0.58, Z0, 0.0)
    # wall clock over the periodicals rack (x = 3 wall), portraits and paintings above the bookcases
    put(m, F.wall_clock(), hx0 + 0.065, RACK[1], Z0 + 1.75, math.pi / 2)
    for k, x in enumerate((4.45, 8.45, 14.8)):
        put(m, F.picture(random.Random(20 + k), 0.5, 0.62, "portrait", "family_portrait"), x, hy1 - 0.015, Z0 + 2.55, 0.0)
    put(m, F.picture(random.Random(24), 1.0, 0.62, "oil", "birch_grove"), hx1 - 0.015, -1.6, Z0 + 2.55, -math.pi / 2)
    put(m, F.picture(random.Random(25), 0.95, 0.62, "oil", "harbour_1950s"), hx0 + 0.015, -0.4, Z0 + 2.55, math.pi / 2)
    put(m, F.picture(random.Random(26), 0.8, 0.62, "print", "engraving_monastery"), 11.9, hy1 - 0.015, Z0 + 2.55, 0.0)
    # busts on the bookcase tops
    put(m, bust(), 3.18, -1.4, Z0 + CASE_H, math.pi / 2)
    put(m, bust(), hx1 - 0.18, -2.9, Z0 + CASE_H, -math.pi / 2)


# --- lamps ------------------------------------------------------------------------------------------------
def lamps(m):
    for x, y in LAMP_XY:
        F.lathe(m, "AC_LPlaster", [(0, ZC - 0.07), (0.07, ZC - 0.07), (0.1, ZC - 0.045), (0.17, ZC - 0.03), (0.24, ZC - 0.012), (0.26, ZC)], 12, (x, y, 0.0))
        F.lathe(m, BRASS, [(0, ZC - 0.06), (0.06, ZC - 0.06), (0.07, ZC - 0.02), (0.07, ZC), (0, ZC)], 10, (x, y, 0.0))
        F.tube(m, BRASS, (x, y, ZC - 0.06), (x, y, LAMP_Z + LAMP_R + 0.05), 0.009, 0.009, 6)
        F.lathe(m, BRASS, [(0, LAMP_Z + LAMP_R - 0.01), (0.075, LAMP_Z + LAMP_R - 0.01), (0.065, LAMP_Z + LAMP_R + 0.04),
                           (0.02, LAMP_Z + LAMP_R + 0.06), (0, LAMP_Z + LAMP_R + 0.06)], 12, (x, y, 0.0))
        F.sphere(m, "AC_LOpal", (x, y, LAMP_Z), LAMP_R, 14, 8)


# --- house plants ----------------------------------------------------------------------------------------
# stand-in sizes at size = 1: (canopy radius, height, pot radius)
PLANT_BASE = {
    "palm": (0.6, 1.7, 0.2), "ficus": (0.4, 1.6, 0.17), "monstera": (0.55, 1.1, 0.2), "dracaena": (0.4, 1.5, 0.13),
    "sansevieria": (0.12, 0.5, 0.09), "aloe": (0.18, 0.35, 0.07), "cactus": (0.07, 0.3, 0.06),
    "geranium": (0.2, 0.38, 0.08), "violet": (0.13, 0.14, 0.06), "chlorophytum": (0.3, 0.3, 0.08),
    "begonia": (0.2, 0.32, 0.08), "fern": (0.32, 0.4, 0.1), "tradescantia": (0.25, 0.15, 0.08),
}
# PLANTS use houseplants.KINDS names; without houseplants.py each kind falls back to a stand-in
STANDIN_OF = {"ficus_elastica": "ficus", "ficus_benjamina": "ficus", "lemon": "ficus", "dieffenbachia": "monstera",
              "african_violet": "violet", "asparagus": "fern", "aspidistra": "sansevieria"}


def _hp_kind(hp, kind):
    return kind if hp is not None and kind in getattr(hp, "KINDS", {}) else None


def info(kind, size=1.0):
    """(radius, height) of a plant as placed (houseplants.info when available)."""
    hp = houseplants()
    if _hp_kind(hp, kind):
        return hp.info(kind, size)
    r, h, _ = PLANT_BASE[STANDIN_OF.get(kind, kind)]
    return (r * size, h * size)


def pot_radius(kind, size=1.0):
    hp = houseplants()
    if _hp_kind(hp, kind) and hasattr(hp, "pot_info"):
        return hp.pot_info(kind, size)[0]
    return PLANT_BASE[STANDIN_OF.get(kind, kind)][2] * size


def _pot(m, r, h, mat):
    F.lathe(m, mat, [(0, 0), (r * 0.72, 0), (r, h), (r * 1.08, h), (r * 1.08, h + 0.025), (r * 0.97, h + 0.025),
                     (r * 0.97, h - 0.012)], 8)
    F.lathe(m, "AC_LSoil", [(r * 0.97, h - 0.012), (0, h - 0.012)], 8)
    return h - 0.012


def _leaf(m, mat, b, az, el, L, W, droop=0.0, twist=0.0):
    """A two-sided pentagonal leaf from b, azimuth az, elevation el, length L, width W; droop pulls
    the tip down."""
    ce = math.cos(el)
    d = (ce * math.cos(az), ce * math.sin(az), math.sin(el))
    s = (-math.sin(az) * math.cos(twist), math.cos(az) * math.cos(twist), math.sin(twist))

    def P(t, o, dz):
        return tuple(b[i] + d[i] * L * t + s[i] * o for i in range(2)) + (b[2] + d[2] * L * t - dz,)

    pts = [P(0, -W * 0.15, 0), P(0.45, -W / 2, droop * 0.25), P(1.0, 0, droop), P(0.45, W / 2, droop * 0.25), P(0, W * 0.15, 0)]
    two_sided(m, mat, pts)


def standin(kind, seed, size=1.0):
    """Simple low-poly house plant (pot base on z=0), until houseplants.py exists."""
    rnd = random.Random(seed)
    m = Mesh()
    k = size
    kind = STANDIN_OF.get(kind, kind)
    R, H, pr = PLANT_BASE[kind]
    if kind == "palm":
        zs = _pot(m, pr * k, 0.32 * k, "AC_LPotClay")
        F.tube(m, "AC_LBark", (0, 0, zs), (0, 0, zs + 0.45 * k), 0.035 * k, 0.025 * k, 5)
        top = (0, 0, zs + 0.45 * k)
        for j in range(10):
            a = j * 2.4 + rnd.uniform(-0.2, 0.2)
            el = rnd.uniform(0.55, 1.1)
            L = rnd.uniform(0.75, 1.05) * k
            for t0, t1, w in ((0.0, 0.5, 0.22), (0.5, 1.0, 0.16)):          # two segments, arching
                e0 = el - 0.9 * t0
                b = (top[0] + math.cos(a) * L * t0 * math.cos(el * 0.7), top[1] + math.sin(a) * L * t0 * math.cos(el * 0.7),
                     top[2] + L * t0 * math.sin(el) - 0.35 * L * t0 * t0)
                _leaf(m, "AC_LLeaf" if j % 3 else "AC_LLeafLight", b, a, e0 - 0.35, L * 0.52, w * k, droop=0.08 * k)
    elif kind == "ficus":
        zs = _pot(m, pr * k, 0.3 * k, "AC_LPotWhite")
        F.tube(m, "AC_LBark", (0, 0, zs), (0.03 * k, 0, zs + 1.15 * k), 0.025 * k, 0.012 * k, 5)
        for j in range(30):
            t = rnd.uniform(0.25, 1.0)
            c = (0.03 * k * t, 0, zs + 1.15 * k * t)
            a = rnd.uniform(0, 2 * math.pi)
            _leaf(m, "AC_LLeaf", c, a, rnd.uniform(-0.2, 0.5), rnd.uniform(0.2, 0.3) * k * (1.25 - 0.5 * t), 0.11 * k, 0.03 * k)
    elif kind == "monstera":
        zs = _pot(m, pr * k, 0.3 * k, "AC_LPotClay")
        for j in range(9):
            a = j * 2.4 + rnd.uniform(-0.3, 0.3)
            L = rnd.uniform(0.45, 0.85) * k
            el = rnd.uniform(0.6, 1.2)
            tip = (math.cos(a) * L * math.cos(el), math.sin(a) * L * math.cos(el), zs + L * math.sin(el))
            F.tube(m, "AC_LLeafLight", (0, 0, zs), tip, 0.008 * k, 0.006 * k, 3)
            # big heart leaf: a hexagon hanging out from the stalk tip
            lw, ll = 0.34 * k, 0.4 * k
            ca, sa = math.cos(a), math.sin(a)
            pts = [(0, 0), (0.25, -lw / 2), (0.75, -lw * 0.55), (1.0, 0), (0.75, lw * 0.55), (0.25, lw / 2)]
            two_sided(m, "AC_LLeaf", [(tip[0] + ca * u * ll - sa * o, tip[1] + sa * u * ll + ca * o, tip[2] - 0.25 * u * ll)
                                      for u, o in pts])
    elif kind == "dracaena":
        zs = _pot(m, pr * k, 0.45 * k, "AC_LPotWhite")
        for j, hgt in enumerate((1.0, 0.75, 0.55)):
            ox, oy = rnd.uniform(-0.04, 0.04), rnd.uniform(-0.04, 0.04)
            top = (ox, oy, zs + hgt * k)
            F.tube(m, "AC_LBark", (0, 0, zs), top, 0.02 * k, 0.015 * k, 5)
            for i in range(10):
                _leaf(m, "AC_LLeafLight" if i % 2 else "AC_LLeaf", top, i * 0.63 + j, rnd.uniform(0.2, 1.1),
                      rnd.uniform(0.3, 0.45) * k, 0.05 * k, droop=0.1 * k)
    elif kind == "sansevieria":
        zs = _pot(m, pr * k, 0.14 * k, "AC_LPotClay")
        for j in range(7):
            a = j * 0.9
            b = (0.03 * math.cos(a) * k, 0.03 * math.sin(a) * k, zs)
            _leaf(m, "AC_LLeaf" if j % 2 else "AC_LLeafVar", b, a, 1.35 + rnd.uniform(-0.1, 0.1),
                  rnd.uniform(0.32, 0.5) * k, 0.06 * k, twist=rnd.uniform(-0.6, 0.6))
    elif kind == "aloe":
        zs = _pot(m, pr * k, 0.09 * k, "AC_LPotClay")
        for j in range(9):
            a, el = j * 2.4, rnd.uniform(0.6, 1.3)
            L = rnd.uniform(0.18, 0.28) * k
            F.tube(m, "AC_LLeafLight", (0, 0, zs), (math.cos(a) * L * math.cos(el), math.sin(a) * L * math.cos(el), zs + L * math.sin(el)),
                   0.018 * k, 0.0, 3)
    elif kind == "cactus":
        zs = _pot(m, pr * k, 0.08 * k, "AC_LPotClay")
        F.tube(m, "AC_LLeaf", (0, 0, zs), (0, 0, zs + 0.2 * k), 0.04 * k, 0.035 * k, 7)
        F.sphere(m, "AC_LLeaf", (0, 0, zs + 0.2 * k), 0.035 * k, 7, 3)
        if rnd.random() < 0.6:
            F.tube(m, "AC_LLeaf", (0.03 * k, 0, zs + 0.1 * k), (0.07 * k, 0, zs + 0.18 * k), 0.018 * k, 0.015 * k, 5, cap1="AC_LFlowerPink")
    elif kind == "geranium":
        zs = _pot(m, pr * k, 0.1 * k, "AC_LPotClay")
        for j in range(5):
            a = j * 1.3
            F.sphere(m, "AC_LLeafLight", (0.06 * math.cos(a) * k, 0.06 * math.sin(a) * k, zs + rnd.uniform(0.07, 0.15) * k),
                     0.075 * k, 6, 3, 0.7)
        for j in range(3):
            a = j * 2.1 + 0.5
            c = (0.07 * math.cos(a) * k, 0.07 * math.sin(a) * k, zs + rnd.uniform(0.22, 0.28) * k)
            F.tube(m, "AC_LLeafLight", (0, 0, zs + 0.1 * k), c, 0.004, 0.004, 3)
            F.sphere(m, "AC_LFlowerRed", c, 0.04 * k, 6, 3)
    elif kind == "violet":
        zs = _pot(m, pr * k, 0.07 * k, "AC_LPotWhite")
        for j in range(8):
            _leaf(m, "AC_LLeaf", (0, 0, zs + 0.01), j * 0.785, 0.25, 0.09 * k, 0.06 * k, droop=0.02 * k)
        for j in range(4):
            a = j * 1.57 + 0.4
            F.sphere(m, "AC_LFlowerPink", (0.025 * math.cos(a) * k, 0.025 * math.sin(a) * k, zs + 0.05 * k), 0.016 * k, 5, 3)
    elif kind == "chlorophytum":
        zs = _pot(m, pr * k, 0.1 * k, "AC_LPotClay")
        for j in range(16):
            _leaf(m, "AC_LLeafVar" if j % 2 else "AC_LLeafLight", (0, 0, zs), j * 0.393 + rnd.uniform(-0.1, 0.1),
                  rnd.uniform(0.3, 0.9), rnd.uniform(0.2, 0.32) * k, 0.025 * k, droop=0.14 * k)
        for j in range(2):                                                          # runners with plantlets
            a = rnd.uniform(0, 2 * math.pi)
            e = (math.cos(a) * 0.3 * k, math.sin(a) * 0.3 * k, zs - 0.12 * k)
            F.tube(m, "AC_LLeafVar", (0, 0, zs + 0.05), e, 0.003, 0.003, 3)
            for i in range(5):
                _leaf(m, "AC_LLeafVar", e, i * 1.26, 0.5, 0.07 * k, 0.015 * k, droop=0.02)
    elif kind == "begonia":
        zs = _pot(m, pr * k, 0.1 * k, "AC_LPotWhite")
        for j in range(6):
            _leaf(m, "AC_LLeaf" if j % 2 else "AC_LUphRed", (0, 0, zs + 0.03), j * 1.05, rnd.uniform(0.2, 0.6), 0.16 * k, 0.12 * k, 0.03 * k)
        for j in range(4):
            a = j * 1.6
            F.sphere(m, "AC_LFlowerPink", (0.05 * math.cos(a) * k, 0.05 * math.sin(a) * k, zs + 0.2 * k), 0.025 * k, 5, 3)
    elif kind == "fern":
        zs = _pot(m, pr * k, 0.14 * k, "AC_LPotClay")
        for j in range(14):
            a = j * 0.45 + rnd.uniform(-0.1, 0.1)
            el = rnd.uniform(0.3, 1.1)
            _leaf(m, "AC_LLeafLight", (0, 0, zs), a, el, rnd.uniform(0.3, 0.45) * k, 0.09 * k, droop=0.2 * k)
    elif kind == "tradescantia":
        # pot on a shelf top near the front (-y) edge; strands hang over the edge towards -y
        zs = _pot(m, pr * k, 0.09 * k, "AC_LPotClay")
        for j in range(9):
            a = math.pi * 1.5 + rnd.uniform(-1.0, 1.0)
            L = rnd.uniform(0.35, 0.8) * k
            p = (math.cos(a) * pr * k, math.sin(a) * pr * k, zs + 0.02)
            out = rnd.uniform(0.12, 0.22)
            path = [p, (p[0] + math.cos(a) * 0.05, -out * 0.6, zs + 0.03), (p[0] + math.cos(a) * 0.07, -out, zs - 0.08)]
            path += [(path[-1][0] + rnd.uniform(-0.03, 0.03), -out - 0.02 * i, zs - 0.08 - L * i / 5) for i in range(1, 6)]
            for i, (q0, q1) in enumerate(zip(path, path[1:])):
                F.tube(m, "AC_LLeafVar", q0, q1, 0.003, 0.003, 3)
                if i % 1 == 0:
                    _leaf(m, "AC_LLeafVar" if (i + j) % 3 else "AC_LUphRed", q1, rnd.uniform(0, 2 * math.pi), -0.2, 0.05 * k, 0.03 * k)
        for j in range(5):
            _leaf(m, "AC_LLeafVar", (0, 0, zs), j * 1.25, 0.4, 0.08 * k, 0.035 * k)
    return m


def plant_mesh(kind, seed, size=1.0):
    hp = houseplants()
    if _hp_kind(hp, kind):
        try:
            return hp.make(kind, seed, size)
        except Exception:  # noqa: BLE001
            pass
    return standin(kind, seed, size)


def overhang(mesh, front, back=None):
    """Bends a plant standing on a shelf top over its front edge: in the plant's own frame the edge is
    at y = -front; whatever reaches beyond it curls down (trailing shoots hang down the case front);
    shoots running back past y = back are stopped at the wall."""
    def f(p):
        x, y, z = p
        if y < -front:
            o = -front - y
            y, z = -front - 0.18 * o, z - 2.2 * o
        if back is not None and y > back:
            y = back
        return (x, y, z)
    return mesh.transformed(f)


PLANT_EDGES = {}     # PLANTS index -> (front, back): plants on a shelf / case top whose shoots hang over the front


def _plants():
    """PLANTS: (kind, x, y, z, yaw, seed, size), houseplants.KINDS names. Deterministic. About 60 plants
    within a ~60k triangle share of the budget."""
    rnd = random.Random(1952)
    out = []
    PLANT_EDGES.clear()
    seed = [100]

    def P(kind, x, y, z, yaw=None, size=1.0, edge=None):
        seed[0] += 1
        if edge is not None:
            PLANT_EDGES[len(out)] = edge
        out.append((kind, round(x, 4), round(y, 4), round(z, 4), round(rnd.uniform(0, 2 * math.pi) if yaw is None else yaw, 4),
                    seed[0], round(size, 3)))

    # every window sill: a back row in the reveal, a front row on the deep window board, one tall plant
    back_pool = ["sansevieria", "aspidistra", "aloe", "cactus", "asparagus", "geranium", "chlorophytum", "sansevieria"]
    front_pool = ["african_violet", "geranium", "begonia", "african_violet", "cactus", "geranium"]
    tall = ["sansevieria", "dracaena", "aspidistra"]
    for i, (cx, sill, wd, ht) in enumerate(SHOP_WINDOWS):
        c = cx + rnd.uniform(-0.08, 0.08)
        P(tall[i], c, YI - 0.12, SILL_Z, size=0.75 if tall[i] == "dracaena" else 1.25)
        for u in (-0.75, -0.42, 0.42, 0.75):
            P(rnd.choice(back_pool), cx + u + rnd.uniform(-0.03, 0.03), YI - 0.11, SILL_Z, size=rnd.uniform(0.9, 1.1))
        for u in (-0.55, 0.0, 0.55):
            P(rnd.choice(front_pool), cx + u + rnd.uniform(-0.03, 0.03), YI + 0.16, SILL_Z, size=rnd.uniform(0.95, 1.15))
    # big floor plants by the windows and in the corners
    for kind, x, y, s in (("ficus_elastica", 7.13, -6.3, 1.1), ("dracaena", 9.58, -6.32, 0.95), ("monstera", 11.28, -6.25, 0.85),
                          ("ficus_elastica", 13.63, -6.3, 1.0), ("ficus_elastica", 15.33, -6.3, 1.15), ("palm", 17.68, -6.3, 1.15),
                          ("ficus_elastica", 1.75, -6.38, 1.15), ("palm", 2.0, -1.9, 0.95)):
        P(kind, x, y, Z0, size=s)
    # plant stands at the ends of the stack aisles
    for x, kind in zip(PLANT_STANDS_X, ("asparagus", "chlorophytum", "begonia")):
        P(kind, x, HALL[3] - 0.25, Z0 + 0.85, size=1.2)
    # trailing plants on the bookcase tops (shoots hang down the case front)
    tops = {"back1": [0.9], "back2": [-1.2, 1.4], "back4": [-1.0], "north": [1.0], "south": [-0.6], "pier2": [-0.3],
            "stack2": [0.3]}
    for name, kind, cx, cy, yaw, w, d, h, nshelf in BOOKCASES:
        for j, u in enumerate(tops.get(name, [])):
            x, y, z = Frame(cx, cy, yaw, Z0)((u, -d / 2 + 0.12, h))
            kd = ("tradescantia", "asparagus", "tradescantia", "chlorophytum")[(j + len(name)) % 4]
            P(kd, x, y, z, yaw=yaw, size=rnd.uniform(0.95, 1.1), edge=(0.12 + 0.07, d - 0.12 - 0.01 if kind == "wall" else None))
    # on the tables, the desk, the counter and the catalogues
    for x in TABLES_X:
        P("african_violet", x, sum(TABLE_Y) / 2, Z0 + TABLE_H, size=1.0)
    P("african_violet", ISSUE_DESK[0] + 0.1, ISSUE_DESK[1] + 0.5, Z0 + 1.05)      # on the reader's ledge
    P("geranium", CLOAK[0] + 0.9, CLOAK[1] + 0.02, Z0 + 1.02)
    x, y, yaw, w = CATALOGUES[0]
    P("tradescantia", *Frame(x, y, yaw, Z0)((w / 2 - 0.2, -0.1, 1.47)), yaw=yaw, edge=(0.15, 0.3))
    P("aloe", *Frame(x, y, yaw, Z0)((-0.35, 0.0, 1.47)))
    x, y, yaw, w = CATALOGUES[1]
    P("cactus", *Frame(x, y, yaw, Z0)((0.3, 0.0, 1.47)))
    return out


PLANTS = _plants()


# The plants are AProceduralPlant in Unreal (the plants thread's indoor species, Dima: every plant goes
# through our plant system): build() leaves them out of the mesh and procedural_plants() lists them.
# PROCEDURAL = False brings back the modelled ones (houseplants.py) for Blender previews.
PROCEDURAL = True
SPECIES = {"ficus_elastica": "RubberPlant", "ficus_benjamina": "WeepingFig", "lemon": "Lemon", "dracaena": "Dracaena",
           "monstera": "Monstera", "sansevieria": "SnakePlant", "aspidistra": "Aspidistra", "chlorophytum": "SpiderPlant",
           "tradescantia": "Tradescantia", "geranium": "Geranium", "begonia": "Begonia", "african_violet": "AfricanViolet",
           "palm": "KentiaPalm", "aloe": "Aloe", "cactus": "Cactus", "dieffenbachia": "Dieffenbachia",
           "asparagus": "AsparagusFern"}


def plants(m):
    for idx, (kind, x, y, z, yaw, seed, size) in enumerate(PLANTS):
        if abs(z - Z0) < 1e-6 or (abs(z - Z0 - 0.85) < 1e-6 and abs(y - (HALL[3] - 0.25)) < 1e-6):
            r = pot_radius(kind, size) + 0.05                  # floor plants: the pot is the footprint
            fp("plant", x - r, x + r, y - r, y + r)
        if PROCEDURAL:
            continue
        part = plant_mesh(kind, seed, size)
        if idx in PLANT_EDGES:
            part = overhang(part, *PLANT_EDGES[idx])
        m.extend(Frame(x, y, yaw, z).apply(part))


# the library's own doorways for ALyudDoor (plan.py hands them to interiors.add_door): Novikova 1's frame
DOORS = [{"at": ((LOBBY_DOOR[0] + LOBBY_DOOR[1]) / 2, VEST[3] + 0.075, Z0), "out": (0.0, -1.0),
          "w": LOBBY_DOOR[1] - LOBBY_DOOR[0], "h": LOBBY_DOOR[2] - Z0, "wall": 0.15, "leaves": 1, "open": False}]


def procedural_plants():
    """PLANTS for AProceduralPlant (Novikova 1's frame): species, the pot's base (the plant snaps down
    onto the shelf or floor within 30 cm), yaw in degrees, seed, age 4-6 by size, pot diameter in cm."""
    out = []
    for kind, x, y, z, yaw, seed, size in PLANTS:
        out.append({"species": SPECIES[kind], "at": [x, y, z + 0.03], "yaw": math.degrees(yaw), "seed": seed,
                    "age": round(min(6.0, max(4.0, 4.0 + 2.0 * (size - 0.8) / 0.5)), 2),
                    "pot_cm": round(200.0 * pot_radius(kind, size), 1)})
    return out


# --- signs -------------------------------------------------------------------------------------------------
def signs(m):
    vx0, vx1, vy0, vy1 = VEST
    # vestibule
    board(m, "sign.academ.library.hours", vx1, -5.1, Z0 + 1.6, 0.4, 0.55, (-1, 0))
    board(m, "sign.academ.library.catalogue", -1.75, YI, Z0 + 2.15, 1.0, 0.3, (0, 1))
    board(m, "sign.academ.library.issue_desk", vx0, sum(SIDE_DOOR[:2]) / 2, Z0 + 2.8, 1.1, 0.32, (1, 0))
    board(m, "sign.academ.library.cloakroom", CLOAK[0], vy1, Z0 + 3.05, 1.0, 0.28, (0, -1))
    # hall
    board(m, "sign.academ.library.silence", COLUMN[0], sum(COLUMN[2:]) / 2, Z0 + 1.9, 0.42, 0.5, (-1, 0))
    board(m, "sign.academ.library.periodicals", HALL[0], RACK[1], Z0 + 2.75, 0.9, 0.26, (1, 0))
    board(m, "sign.academ.library.silence", HALL[1], -4.6, Z0 + 1.9, 0.42, 0.5, (-1, 0))
    # «Читальный зал» hung under the arch on two brass rods, face to the vestibule
    xa = (VEST[1] + HALL[0]) / 2
    yc = sum(ARCH_Y) / 2
    w, h, t = 1.1, ARCH_BOARD_Z[1] - ARCH_BOARD_Z[0], 0.025
    zc = sum(ARCH_BOARD_Z) / 2
    F.bx(m, "AC_LBoard", xa - t / 2, xa + t / 2, yc - w / 2, yc + w / 2, ARCH_BOARD_Z[0], ARCH_BOARD_Z[1])
    for y0, y1, z0, z1 in ((yc - w / 2 - 0.02, yc + w / 2 + 0.02, ARCH_BOARD_Z[1], ARCH_BOARD_Z[1] + 0.02),
                           (yc - w / 2 - 0.02, yc + w / 2 + 0.02, ARCH_BOARD_Z[0] - 0.02, ARCH_BOARD_Z[0]),
                           (yc - w / 2 - 0.02, yc - w / 2, ARCH_BOARD_Z[0], ARCH_BOARD_Z[1]),
                           (yc + w / 2, yc + w / 2 + 0.02, ARCH_BOARD_Z[0], ARCH_BOARD_Z[1])):
        F.bx(m, BRASS, xa - t / 2 - 0.006, xa + t / 2 + 0.006, y0, y1, z0, z1)
    r = 1.2
    for s in (-1, 1):
        y = yc + s * 0.45
        ztop = ARCH_TOP - r + math.sqrt(r * r - 0.45 ** 2) + 0.01
        F.tube(m, BRASS, (xa, y, ARCH_BOARD_Z[1] + 0.02), (xa, y, ztop), 0.006, 0.006, 4)
    LABELS.append(("sign.academ.library.reading_room", round(xa - t / 2 - 0.001, 4), round(yc, 4), round(zc, 4), w, h))
    LABEL_NORMALS.append((-1.0, 0.0, 0.0))


# --- build ------------------------------------------------------------------------------------------------
def build(with_backs=True):
    """Returns the library as a geom.Mesh in the Novikova 1 house frame (metres). Also fills LABELS,
    LABEL_NORMALS and FOOTPRINTS."""
    del LABELS[:]
    del LABEL_NORMALS[:]
    del FOOTPRINTS[:]
    _merge_plant_materials()
    rnd = random.Random(1951)
    m = Mesh()
    walls(m)
    opening_liners(m)
    if with_backs:
        backs(m)
    column(m)
    windows_inside(m)
    bookcases(m, rnd)
    vestibule(m, rnd)
    hall(m, rnd)
    lamps(m)
    plants(m)
    signs(m)
    return m


def clearances():
    """Clear width/height (m) of the passages the player uses (all >= 1.08 x 2.10)."""
    return {
        "entrance": (ENTRANCE[2], ENTRANCE[1] + ENTRANCE[3] - Z0),
        "hall_door": (HALL_DOOR[2], HALL_DOOR[1] + HALL_DOOR[3] - Z0),
        "arch": (ARCH_Y[1] - ARCH_Y[0], ARCH_BOARD_Z[0] - Z0),       # under the hanging board
        "back_door": (BACK_DOOR[1] - BACK_DOOR[0], BACK_DOOR[3] - BACK_DOOR[2]),
        "side_door": (SIDE_DOOR[1] - SIDE_DOOR[0], SIDE_DOOR[2] - Z0),
    }


# --- route and door-box check ----------------------------------------------------------------------------
ROUTE_W = 1.1          # clear walking width between furniture
DOOR_BOX_D = 1.2       # nothing standing this deep in front of a door / the arch ...
DOOR_BOX_SIDE = 0.2    # ... across the door width + 2 x this


def door_boxes():
    hx0, hx1, hy0, hy1 = HALL
    vx0, vx1, vy0, vy1 = VEST
    s, d = DOOR_BOX_SIDE, DOOR_BOX_D
    boxes = {}
    for name, (cx, _, wd, _) in (("entrance", ENTRANCE), ("hall_door", HALL_DOOR)):
        boxes[name] = (cx - wd / 2 - s, cx + wd / 2 + s, YI, YI + d)
    boxes["back_door"] = (BACK_DOOR[0] - s, BACK_DOOR[1] + s, hy1 - d, hy1)
    boxes["side_door"] = (vx0, vx0 + d, SIDE_DOOR[0] - s, SIDE_DOOR[1] + s)
    boxes["arch_vestibule"] = (vx1 - d, vx1, ARCH_Y[0] - s, ARCH_Y[1] + s)
    boxes["arch_hall"] = (hx0, hx0 + d, ARCH_Y[0] - s, ARCH_Y[1] + s)
    boxes["lobby_door"] = (LOBBY_DOOR[0] - s, LOBBY_DOOR[1] + s, vy1 - d, vy1)
    return boxes


ROUTE_TARGETS = {
    "arch": (2.72, -3.6), "hall_door": (4.35, YI + 0.6), "back_door": (17.25, HALL[3] - 0.6),
    "side_door": (VEST[0] + 0.6, -3.65), "lobby_door": ((LOBBY_DOOR[0] + LOBBY_DOOR[1]) / 2, VEST[3] - 0.6), "issue_desk": (0.85, -3.85), "cloakroom": (0.9, -2.97),
    "catalogue": (-1.2, -5.45), "periodicals": (3.95, -5.95), "pier1": (6.2, -5.83),
    "table1": (8.4, -3.15), "table2": (12.45, -3.15), "table3": (16.5, -3.15), "end_cases": (17.05, -1.5),
    "nook": (15.25, -1.45), "aisle1": (6.15, -0.9), "aisle2": (8.45, -0.9), "aisle3": (10.75, -0.9),
    "aisle4": (12.87, -0.9), "back_aisle": (9.0, 0.58), "south_cases": (3.95, -0.5),
}


def route_check(step=0.05):
    """Grid search for a ROUTE_W wide walking route from the entrance to every ROUTE_TARGETS point
    (a disc of radius ROUTE_W/2 that fits between the walls and the FOOTPRINTS), and the door boxes
    against the footprints. Returns {'routes': {name: bool}, 'door_boxes': [(door, thing)]}."""
    if not FOOTPRINTS:
        build()
    r = ROUTE_W / 2
    vx0, vx1, vy0, vy1 = VEST
    hx0, hx1, hy0, hy1 = HALL
    free_rects = [(vx0 + r, vx1 - r, vy0 + r, vy1 - r), (hx0 + r, hx1 - r, hy0 + r, hy1 - r),
                  (vx0 + r, hx1 - r, ARCH_Y[0] + r, ARCH_Y[1] - r)]
    nx = int((hx1 - vx0) / step) + 1
    ny = int((hy1 - vy0) / step) + 1
    blocked = bytearray(nx * ny)
    for i in range(nx):
        x = vx0 + i * step
        for j in range(ny):
            y = vy0 + j * step
            if not any(a <= x <= b and c <= y <= d for a, b, c, d in free_rects):
                blocked[i * ny + j] = 1
    for name, a, b, c, d in FOOTPRINTS:
        i0, i1 = max(0, int((a - r - vx0) / step)), min(nx - 1, int((b + r - vx0) / step) + 1)
        j0, j1 = max(0, int((c - r - vy0) / step)), min(ny - 1, int((d + r - vy0) / step) + 1)
        for i in range(i0, i1 + 1):
            x = vx0 + i * step
            dx = max(a - x, 0.0, x - b)
            for j in range(j0, j1 + 1):
                y = vy0 + j * step
                dy = max(c - y, 0.0, y - d)
                if dx * dx + dy * dy < r * r:
                    blocked[i * ny + j] = 1

    def cell(p):
        return int(round((p[0] - vx0) / step)), int(round((p[1] - vy0) / step))

    si, sj = cell((ENTRANCE[0], YI + 0.6))
    seen = bytearray(nx * ny)
    stack = [(si, sj)] if not blocked[si * ny + sj] else []
    for i, j in stack:
        seen[i * ny + j] = 1
    while stack:
        i, j = stack.pop()
        for di in (-1, 0, 1):
            for dj in (-1, 0, 1):
                a, b = i + di, j + dj
                if 0 <= a < nx and 0 <= b < ny and not seen[a * ny + b] and not blocked[a * ny + b]:
                    seen[a * ny + b] = 1
                    stack.append((a, b))
    routes = {}
    for name, p in ROUTE_TARGETS.items():
        i, j = cell(p)
        routes[name] = any(seen[(i + di) * ny + j + dj] for di in (-1, 0, 1) for dj in (-1, 0, 1)
                           if 0 <= i + di < nx and 0 <= j + dj < ny)
    bad = []
    for door, (a, b, c, d) in door_boxes().items():
        for name, x0, x1, y0, y1 in FOOTPRINTS:
            if x0 < b - 1e-3 and x1 > a + 1e-3 and y0 < d - 1e-3 and y1 > c + 1e-3:
                bad.append((door, name, (round(x0, 2), round(x1, 2), round(y0, 2), round(y1, 2))))
    return {"routes": routes, "door_boxes": bad}


# --- self-check: is the interior closed when seen from inside? (ported from gastronom.audit) ---------------
ENCLOSURE = {"AC_LDado", "AC_LWall", "AC_LPlaster", "AC_LParquet", "AC_LStone", "AC_LDoor", "AC_Trim"}
AUDIT_POINTS = G.AUDIT_POINTS


def audit(mesh=None, n_dirs=500, points=None, verbose=True):
    """Casts rays from interior points against the enclosure (walls, floors, ceilings, cornices,
    doors, liners). A ray is fine if the first face it hits faces the ray's origin, or if it leaves
    through an east-wall opening. Returns {'rays', 'escape', 'backface', 'opening', 'problems'}."""
    from geom import cross, norm, sub
    mesh = mesh or build()
    tris = _tris(mesh, ENCLOSURE)
    ops = _openings()
    pts = points or AUDIT_POINTS
    dirs = []
    ga = math.pi * (3 - math.sqrt(5))
    for i in range(n_dirs):
        z = 1 - 2 * (i + 0.5) / n_dirs
        r = math.sqrt(1 - z * z)
        dirs.append((r * math.cos(ga * i), r * math.sin(ga * i), z))
    res = {"rays": 0, "escape": 0, "backface": 0, "opening": 0, "problems": []}

    def cast(o, d):
        best, hit = 1e9, None
        for a, e1, e2, n, mat in tris:
            p = cross(d, e2)
            det = _dot(e1, p)
            if abs(det) < 1e-12:
                continue
            inv = 1.0 / det
            s = sub(o, a)
            u = _dot(s, p) * inv
            if u < 0.0 or u > 1.0:
                continue
            q = cross(s, e1)
            v = _dot(d, q) * inv
            if v < 0.0 or u + v > 1.0:
                continue
            t = _dot(e2, q) * inv
            if 1e-6 < t < best:
                best, hit = t, (n, mat)
        if d[1] < -1e-9:
            t_out = (YO - o[1]) / d[1]
            if t_out < best:
                x, z = o[0] + d[0] * t_out, o[2] + d[2] * t_out
                if any(x0 <= x <= x1 and z0 <= z <= z1 for x0, x1, z0, z1 in ops):
                    return "opening", None
        if hit is None:
            return "escape", None
        if _dot(hit[0], d) > 0:
            return "backface", (tuple(round(o[k] + d[k] * best, 3) for k in range(3)), hit[1])
        return "ok", None

    for o in pts:
        for d in dirs:
            res["rays"] += 1
            kind, info_ = cast(o, d)
            if kind in ("escape", "backface"):
                retry = [cast(o, norm((d[0] + j[0], d[1] + j[1], d[2] + j[2])))[0]
                         for j in ((1e-4, -7e-5, 3e-5), (-6e-5, 1e-4, -8e-5))]
                if any(r in ("ok", "opening") for r in retry):
                    kind = "ok"
            if kind == "opening":
                res["opening"] += 1
            elif kind != "ok":
                res[kind] += 1
                res["problems"].append((kind, o, tuple(round(c, 3) for c in d), info_))
    if verbose:
        print("audit: %d rays, %d out through openings, %d escape, %d backface" %
              (res["rays"], res["opening"], res["escape"], res["backface"]))
        for p in res["problems"][:20]:
            print("  ", p)
    return res


if __name__ == "__main__":
    import collections
    import sys
    mesh = build()
    print("triangles", mesh.tri_count(), "faces", len(mesh.faces), "verts", len(mesh.verts))
    by = collections.Counter()
    for mt, idx, _ in mesh.faces:
        by[mt] += len(idx) - 2
    print("tris by material", by.most_common())
    print("houseplants.py", "used" if houseplants() else "absent: stand-ins")
    print("labels", len(LABELS), sorted(set(l[0] for l in LABELS)))
    print("book slots", len(BOOK_SLOTS), "plants", len(PLANTS), "lights", len(LIGHTS))
    used = set(f[0] for f in mesh.faces)
    known = set(LIBRARY_MATERIALS) | {"AC_Frame", "AC_Trim"} | set(k for k in F.FURNITURE_MATERIALS if k.startswith("AC_FPaint_"))
    print("unknown materials", sorted(u for u in used if u not in known))
    print(clearances())
    rc = route_check()
    print("routes", {k: v for k, v in rc["routes"].items() if not v} or "all ok")
    print("door boxes", rc["door_boxes"] or "clear")
    if "--audit" in sys.argv:
        audit(mesh, n_dirs=600)
