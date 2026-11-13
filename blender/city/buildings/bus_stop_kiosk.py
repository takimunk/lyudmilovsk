"""Bus-stop kiosk: a sculptural concrete shelter with a small glazed kiosk (ice cream, newspapers, sodas).

Original design in the spirit of the wild concrete bus stops of the 1960s-80s USSR (folded-plate roofs, big
mosaic "suns" on a wave wall): a tall wave-shaped back wall rises to a fin on the right, a folded
two-triangle concrete roof cantilevers from it and is propped by two outward-leaning piers; behind
the roof a mosaic sun (blue / white tile rings) decorates the wall over a wooden bench. At the right a small hollow
kiosk box (white tile, red fascia, serving window with a wooden shelf, side window, side door).

Plot 6 x 5 m, height 4.95 m. Not enterable (the seller's room is hollow, one door, no stairs).
Spots: bench (under the roof), counter (seller inside), stand (buyer at the window).
Window kinds: "public" (serving window + side window). No flats.
Ideas for later: a timetable board, a ticket-machine, a litter bin, an ice-cream sign (neon), tile variants.
Six plots use it (VARIANTS, one per kiosk: its trade, accent colour, sign): news, ice cream, tobacco, flowers,
cable-car information, repairs. Things, spots, windows, entrance and footprint are the same in every variant.
Second pass (2026-11): each trade has its own inside (seen through the hatch and the side door) and its own
bus-stop furniture (`_deeper`: drum litter bin everywhere, payphone at kiosk 1, drinking fountain at 5, weighing
machine at 3), extra things per trade (ids k<n>_*, words in strings/x_p_kiosk.json), a stop lamp under the roof.
Third pass (2026-11, depth): every kiosk gets a clock, a lamp switch for the stop's two lamps, a stamp pad of the
"bus-stop passport", a goods stand under the hatch (tins with atlas goods labels chosen by trade), a seat of its own
and per trade a wall paper stand (1), thermometer (2), lotto sheet (3), florist's calendar (4), departures stand (5),
key board and umbrella stand (6); staff spot for the seller. Words in strings/x_p_kiosk.json.
Requests: none.
"""
import math
import os

from citykit import box, prism, DOOR_H  # noqa: F401
import geom
import random
import furn_int3 as fu
import interior_kit as kit
from furn_int3 import Fr, lbox, cyl, blob

INFO = {
    "name_ru": "Остановка-киоск", "name_en": "Bus-stop Kiosk",
    "style": "late-soviet expressionism", "use": "public",
    "inspired_by": "Soviet concrete bus stops of the 1960s-80s (folded-plate roofs, mosaic panels, wave walls)",
}

TRADE = "news"                  # news | ice | tobacco | flowers | cable | repair
ACCENT = "CT_PlasterRed"        # fascia / awning colour
SIGN = "sign_books"             # atlas sign cell on the fascia (None: plain coloured fascia)
VARIANTS = {
    "P-KIOSK1": {"TRADE": "news", "ACCENT": "CT_PlasterRed", "SIGN": "sign_books"},
    "P-KIOSK2": {"TRADE": "ice", "ACCENT": "CT_PlasterBlue", "SIGN": "sign_milk"},
    "P-KIOSK3": {"TRADE": "tobacco", "ACCENT": "CT_PlasterGreen", "SIGN": None},
    "P-KIOSK4": {"TRADE": "flowers", "ACCENT": "CT_PlasterPink", "SIGN": None},
    "P-KIOSK5": {"TRADE": "cable", "ACCENT": "CT_PlasterYellow", "SIGN": "sign_till"},
    "P-KIOSK6": {"TRADE": "repair", "ACCENT": "CT_Leather", "SIGN": None},
}

SHELF_MATS = {
    "tobacco": ["CT_TileWhite", "CT_PlasterRed", "CT_PlasterGreen", "CT_TileWhite", "CT_PlasterYellow"],
    "flowers": ["CT_PlasterPink", "CT_PlasterYellow", "CT_TileWhite", "CT_PlasterGreen"],
    "cable": ["CT_PlasterYellow", "CT_TileWhite", "CT_PlasterBlue", "CT_TileWhite"],
    "repair": ["CT_Leather", "CT_Wood", "CT_Steel", "CT_Leather", "CT_PlasterRed"],
    "ice": ["CT_TileWhite", "CT_PlasterBlue", "CT_PlasterPink", "CT_PlasterYellow"],
}

# roof corner heights (top face), x -2.9..2.9, y 0.7..4.6
RX0, RX1, RY0, RY1 = -2.9, 2.9, 0.7, 4.6
ROOF_T = 0.28
Z_FL, Z_FR, Z_BR, Z_BL = 2.9, 3.4, 4.5, 3.6


def xz_prism(m, mat, poly, y0, y1, side_mat=None):
    """Solid shape: polygon in (x, z) (any winding) extruded between y0 < y1 (front face looks to -y)."""
    n = len(poly)
    area = sum(poly[i][0] * poly[(i + 1) % n][1] - poly[(i + 1) % n][0] * poly[i][1] for i in range(n))
    if area < 0:
        poly = poly[::-1]
    for a, b, c in geom.triangulate(poly):
        pa, pb, pc = poly[a], poly[b], poly[c]
        m.face(mat, [(pa[0], y0, pa[1]), (pb[0], y0, pb[1]), (pc[0], y0, pc[1])])
        m.face(mat, [(pa[0], y1, pa[1]), (pc[0], y1, pc[1]), (pb[0], y1, pb[1])])
    for k in range(n):
        (ax, az), (bx, bz) = poly[k], poly[(k + 1) % n]
        m.face(side_mat or mat, [(ax, y0, az), (ax, y1, az), (bx, y1, bz), (bx, y0, bz)][::-1][::-1])


def tri_slab(m, mat, p, t, hidden=()):
    """A triangular slab: top face through the 3 points p (x, y, z), thickness t downwards.
    hidden: indices k of edges (p[k], p[k+1]) whose side face is left out (shared with another slab)."""
    a, b, c = p
    nz = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
    if nz < 0:
        p = [a, c, b]
    top = list(p)
    bot = [(x, y, z - t) for x, y, z in p]
    m.face(mat, top)
    m.face(mat, bot[::-1])
    for k in range(3):
        k2 = (k + 1) % 3
        if (frozenset((tuple(top[k]), tuple(top[k2])))) in hidden:
            continue
        m.face(mat, [bot[k], bot[k2], top[k2], top[k]])


def roof_z(x, y):
    """Top height of the folded roof at (x, y): two planar triangles split on the FL-BR diagonal."""
    fl, fr, br, bl = (RX0, RY0, Z_FL), (RX1, RY0, Z_FR), (RX1, RY1, Z_BR), (RX0, RY1, Z_BL)
    # which side of the diagonal fl->br
    side = (br[0] - fl[0]) * (y - fl[1]) - (br[1] - fl[1]) * (x - fl[0])
    a, b, c = (fl, br, bl) if side > 0 else (fl, fr, br)
    d = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
    l1 = ((x - a[0]) * (c[1] - a[1]) - (y - a[1]) * (c[0] - a[0])) / d
    l2 = ((b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0])) / d
    return a[2] + l1 * (b[2] - a[2]) + l2 * (c[2] - a[2])


def _disc(m, cx, cz, y, rings):
    """Concentric mosaic rings on the plane y facing -y: rings = [(r0, r1, mat)]."""
    segs = 24
    for r0, r1, mat in rings:
        for k in range(segs):
            a0, a1 = 2 * math.pi * k / segs, 2 * math.pi * (k + 1) / segs
            P = lambda r, a: (cx + r * math.cos(a), y, cz + r * math.sin(a))
            if r0 == 0:
                m.face(mat, [P(0, 0), P(r1, a0), P(r1, a1)][::-1][::-1])
            else:
                m.face(mat, [P(r0, a0), P(r1, a0), P(r1, a1), P(r0, a1)])


def build(B):
    m, d = B.m, B.detail
    # ---- wave back wall (y 4.2..4.6): top curves from 3.5 at the left to a fin of 4.95 at the right
    prof = [(-2.95, 0.0), (2.95, 0.0)]
    n = 18
    for i in range(n + 1):
        t = i / n
        x = 2.95 - 5.9 * t
        s = t * t * (3 - 2 * t)
        z = 4.95 - 1.45 * s + 0.18 * math.sin(t * math.pi * 2)
        prof.append((x, z))
    xz_prism(m, "CT_Concrete", prof, 4.2, 4.6)
    # ---- mosaic sun on the wall, over the bench
    sy = 4.2 - 0.03
    cx, cz = -0.95, 1.7
    _disc(m, cx, cz, sy, [(0, 0.3, "CT_Tile"), (0.3, 0.62, "CT_TileWhite"), (0.62, 0.95, "CT_Tile"),
                          (0.95, 1.08, "CT_TileWhite")])
    # rays: blue/white tile blocks around the sun's upper half
    for k in range(7):
        a = math.pi * (0.0 + 1.0 * k / 6)
        r0, r1 = 1.2, 1.7 if k % 2 == 0 else 1.5
        w = 0.12
        ca, sa = math.cos(a), math.sin(a)
        p0 = (cx + r0 * ca, sy, cz + r0 * sa)
        p1 = (cx + r1 * ca, sy, cz + r1 * sa)
        nx, nz = -sa * w, ca * w
        m.face("CT_Tile" if k % 2 == 0 else "CT_TileWhite",
               [(p0[0] - nx, sy, p0[2] - nz), (p0[0] + nx, sy, p0[2] + nz), (p1[0] + nx, sy, p1[2] + nz), (p1[0] - nx, sy, p1[2] - nz)])
    # base tile band along the wall under the bench
    box(m, "CT_TileWhite", -2.9, 0.5, 4.17, 4.2, 0.0, 0.55)
    # ---- folded roof: two triangles meeting on the FL-BR diagonal
    fl, fr, br, bl = (RX0, RY0, Z_FL), (RX1, RY0, Z_FR), (RX1, RY1, Z_BR), (RX0, RY1, Z_BL)
    diag = frozenset(((fl), (br)))
    tri_slab(m, "CT_Concrete", [fl, fr, br], ROOF_T, hidden={frozenset((fl, br))})
    tri_slab(m, "CT_Concrete", [fl, br, bl], ROOF_T, hidden={frozenset((fl, br))})
    # red fascia stripe on the front edge of the roof
    # ---- leaning piers (foot inside, head outside under the roof)
    for foot0, foot1, head0, head1, yc in ((-2.6, -2.2, -2.95, -2.55, 1.0), (2.15, 2.55, 2.6, 3.0, 1.0)):
        zt = roof_z((head0 + head1) / 2, yc) - ROOF_T + 0.15
        xz_prism(m, "CT_Concrete", [(foot0, 0.0), (foot1, 0.0), (head1, zt), (head0, zt)], yc - 0.2, yc + 0.2)
    # ---- the kiosk: hollow box x 0.6..2.8, y 2.2..4.2, walls 0.15 thick
    kx0, kx1, ky0, ky1, kh = 0.6, 2.8, 2.2, 4.2, 2.45
    W = 0.15
    # front (serving window hole)
    F = B.facade((kx0, ky0), (kx1, ky0), 0.0, kh, "CT_TileWhite", holes=[(0.35, 1.95, 1.0, 2.15)], depth=W)
    B.facade_window(F, 0.35, 1.95, 1.0, 2.15, kind="public", cols=2, rows=1)
    # left (side window): outside faces -x, runs from back to front
    F2 = B.facade((kx0, ky1), (kx0, ky0), 0.0, kh, "CT_TileWhite", holes=[(0.55, 1.45, 1.0, 2.05)], depth=W)
    B.facade_window(F2, 0.55, 1.45, 1.0, 2.05, kind="public")
    # right (door hole)
    F3 = B.facade((kx1, ky0), (kx1, ky1), 0.0, kh, "CT_TileWhite", holes=[(0.9, 1.8, 0.0, 2.0)], depth=W)
    # (the door leaf comes from the game's door actor: the hole stays open so the room reads from outside)
    B.entrance("staff", (kx1, 3.55, 0.0), 90.0, width=0.9, height=2.0, kind="service")
    # the back wall of the kiosk is the concrete wall; floor and roof
    box(m, "CT_Floor", kx0 - W, kx1, ky0 - W, ky1, 0.0, 0.02, faces="Z")
    box(m, "CT_Concrete", kx0 - 0.35, kx1 + 0.2, ky0 - 0.35, ky1, kh, kh + 0.15)
    box(m, ACCENT, kx0 - 0.37, kx1 + 0.22, ky0 - 0.37, ky0 - 0.34, kh - 0.02, kh + 0.30, faces="all")
    # serving shelf under the window
    box(m, "CT_Wood", kx0 + 0.25, kx1 - 0.45, ky0 - 0.30, ky0, 0.96, 1.02)
    for u in (kx0 + 0.45, kx1 - 0.65):
        box(m, "CT_Steel", u, u + 0.05, ky0 - 0.28, ky0 - 0.03, 0.80, 0.96)
    # awning blades over the window (a striped canvas awning replaces them for ice cream and flowers)
    if TRADE not in ("ice", "flowers"):
        box(m, ACCENT, kx0 + 0.25, kx1 - 0.45, ky0 - 0.55, ky0 - 0.05, 2.22, 2.27)
    # litter bin / pillar tiling accent
    box(m, "CT_Steel", -2.95, -2.7, 0.2, 0.45, 0.0, 0.75)
    # ---- the kiosk's goods: counter, freezer, soda shelves, newspaper rack, lamp
    rnd = random.Random(1964)
    PACK = ["CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterGreen", "CT_TileWhite", "CT_PlasterPink"]
    box(m, "CT_WoodLight", kx0 + 0.15, kx1 - 0.15, ky0 + 0.15, ky0 + 0.7, 0.02, 0.92)          # the counter under the hatch
    box(d, "CT_ConcreteDark", kx0 + 0.15, kx1 - 0.15, ky0 + 0.13, ky0 + 0.72, 0.92, 0.96)
    # soda bottles in two rows on the counter, with a till and a cash tray
    for row, yy in enumerate((ky0 + 0.35, ky0 + 0.52)):
        for i in range(9):
            if TRADE in ("tobacco", "flowers", "cable", "repair") or (TRADE == "ice" and i % 3):
                continue
            cyl(d, PACK[(i + row) % 4], kx0 + 0.5 + i * 0.16, yy, 0.96, 0.96 + 0.2, 0.032, 5)
            cyl(d, "CT_Steel", kx0 + 0.5 + i * 0.16, yy, 1.16, 1.2, 0.012, 4)
    box(d, "CT_Steel", kx1 - 0.65, kx1 - 0.3, ky0 + 0.35, ky0 + 0.62, 0.96, 1.15)
    # ice-cream freezer under the left (side) window: an open white chest with ice-cream tubs
    box(m, {"repair": "CT_Wood", "cable": "CT_WoodLight", "tobacco": "CT_WoodLight"}.get(TRADE, "CT_TileWhite"),
        kx0 + 0.15, kx0 + 0.75, 3.05, 4.15, 0.02, 0.7)
    for j in range(5):
        for i in range(2):
            if TRADE != "ice":
                break
            box(d, rnd.choice(["CT_PlasterPink", "CT_PlasterBlue", "CT_PlasterYellow", "CT_TileWhite", "CT_PlasterGreen"]),
                kx0 + 0.22 + i * 0.25, kx0 + 0.42 + i * 0.25, 3.12 + j * 0.2, 3.26 + j * 0.2, 0.7, 0.88, faces="xXyYZ")
    # soda / snack shelves on the back wall beside the door
    fu.shelving(B, Fr(1.3, 4.2 - 0.0, 0), -0.55, 0.65, rnd, depth=0.3, tiers=4 if TRADE != "tobacco" else 6, zmax=2.2,
                z0=0.2, kind="box", mats=SHELF_MATS.get(TRADE, PACK), fill=0.95 if TRADE == "tobacco" else 0.85,
                hh=(0.06, 0.1) if TRADE == "tobacco" else (0.14, 0.34))
    # newspaper rack on the outside of the left wall near the back
    rx = kx0 - 0.04
    if TRADE == "news":
        box(d, "CT_Steel", rx - 0.4, rx, 3.65, 4.1, 0.0, 0.05)
        box(d, "CT_Steel", rx - 0.03, rx, 3.65, 4.1, 0.05, 1.55)
        for zz in (0.45, 0.85, 1.25):
            box(d, "CT_Steel", rx - 0.3, rx, 3.65, 4.1, zz, zz + 0.025)
            box(d, "CT_Steel", rx - 0.3, rx - 0.27, 3.65, 4.1, zz, zz + 0.12)
            for k in range(3):
                box(d, rnd.choice(["CT_TileWhite", "CT_PlasterYellow", "CT_PlasterPink", "CT_TileWhite"]),
                    rx - 0.26, rx - 0.04, 3.7 + 0.14 * k, 3.82 + 0.14 * k, zz + 0.025, zz + 0.07 + 0.015 * k, faces="xXyYZ")
    # one lamp
    B.lamp((1.7, 3.3, 2.2), lumens=480, colour="#FFD2A0", kind="public", radius=4.5)
    box(d, "CT_Steel", 1.69, 1.71, 3.29, 3.31, 2.28, kh, faces="xXyY")
    # ---- life
    B.spot("bench", "bench", (-1.25, 3.7, 0.0), 0.0)
    B.spot("counter", "counter", (1.5, 3.2, 0.0), 0.0)
    B.spot("stand", "stand", (1.4, 1.3, 0.0), 180.0)
    _furnish(B, rnd)
    _dress(B, rnd)
    _deeper(B, rnd)
    _third(B)
    B.footprint = [(-3.0, 0.0), (3.0, 0.0), (3.0, 5.0), (-3.0, 5.0)]
    B.height = 4.95
    if os.environ.get("KIT_CHECK"):
        print("KITCHECK", kit.check(B))


def _stand(B, x, y, name, w=0.6, zc=1.5):
    """A board on two steel posts, facing the street (-y); the poster sits on its front face."""
    d = B.detail
    u0, v0, u1, v1 = kit.ATLAS.get(name, [0, 0, 1, 1])
    h = w * (v1 - v0) / max(1e-6, (u1 - u0))
    for sx in (x - w / 2 - 0.02, x + w / 2 + 0.02):
        box(d, "CT_Steel", sx - 0.03, sx + 0.03, y + 0.0, y + 0.06, 0.0, zc + h / 2 + 0.12)
    box(d, "CT_SteelWhite", x - w / 2 - 0.05, x + w / 2 + 0.05, y, y + 0.04, zc - h / 2 - 0.05, zc + h / 2 + 0.05)
    kit.poster(B, x, y, 0.0, 0.0, name=name, w=w, zc=zc)
    return h


def _furnish(B, rnd):
    kx0, kx1, ky0, ky1 = 0.6, 2.8, 2.2, 4.2
    # ---- the bench under the roof (back to the wall) with a newspaper left on it, a pot, a notice board above
    bn = kit.bench(B, -1.25, 3.95, 0.0, 0.0, w=2.0, kind="park")
    B.thing("shelter_bench", "seat", (-1.25, 3.95, 0.45), 0.0, height=0.45)
    kit.newspaper(B, -0.55, 3.78, 0.45, 12, seed=3)
    kit.plant(B, -2.65, 3.7, 0.0, 0.0, h=0.9, kind="cactus", pot="CT_PlasterYellow")
    kit.notice_board(B, -2.6, 4.2, 0.0, 0.0, w=0.55, zc=1.45)
    B.thing("stop_notices", "board", (-2.6, 4.15, 1.45), 0.0, topic="lost")
    # ---- the timetable on a stand at the kerb of the shelter
    _stand(B, -2.15, 1.75, "chart_timetable_bus", w=0.6, zc=1.5)
    B.thing("timetable", "board", (-2.15, 1.7, 1.5), 0.0, topic="timetable")
    # ---- the serving window: the goods of the trade on the shelf, pinned notices beside the glass
    sh = 1.02
    if TRADE == "news":
        for k, x in enumerate((1.1, 1.38, 1.66)):
            kit.newspaper(B, x, ky0 - 0.12, sh, (-10, 8, -4)[k], seed=k + 1)
        kit.bottles(B, 2.0, ky0 - 0.12, sh, 0.0, n=3, seed=5)
        kit.cup(B, 2.28, ky0 - 0.12, sh, 0.0, mat="CT_PlasterRed")
        for k, (nm, zc) in enumerate((("mag_2", 1.35), ("mag_5", 1.75))):
            kit.poster(B, 0.78, ky0, 0.0, 0.0, name=nm, w=0.26, zc=zc)
        kit.poster(B, 2.67, ky0, 0.0, 0.0, name="news_1", w=0.2, zc=1.6)
    else:
        notes = {"ice": ("poster_sun_1", "poster_sun_2"), "tobacco": ("paper_typed", "poster_health"),
                 "flowers": ("poster_sun_2", "notice_2"), "cable": ("chart_timetable_cable", "map_town"),
                 "repair": ("notice_2", "paper_letter")}[TRADE]
        kit.poster(B, 0.78, ky0, 0.0, 0.0, name=notes[0], w=0.26, zc=1.5)
        kit.poster(B, 2.67, ky0, 0.0, 0.0, name=notes[1], w=0.2, zc=1.6)
    pc = (1.75, ky0 - 0.12, sh)
    B.thing("counter", "counter", pc, 0.0, role="seller", service="newspapers", shop="kiosk_news")
    B.thing("postcards", "take", (2.15, ky0 - 0.12, sh + 0.03), 0.0, item="postcard")
    # ---- the side wall: the price list, the free paper rack
    kit.menu_board(B, kx0, 2.55, 0.0, 270.0, name={"news": "paper_typed", "ice": "menu_1", "tobacco": "paper_typed", "flowers": "notice_1",
                                                  "cable": "paper_typed", "repair": "notice_0"}[TRADE], w=0.4, zc=1.45)
    B.thing("price_board", "board", (kx0 - 0.03, 2.55, 1.45), 270.0, topic="prices")
    B.thing("paper_rack", "take", (0.36, 3.9, 1.0), 270.0, item="daily_paper")
    # ---- crates of soda bottles by the front corner and a stool for the seller's friend
    if TRADE in ("news", "ice", "tobacco"):
        kit.crate(B, 0.3, 2.5, 0.0, 0.0, w=0.5, d=0.34, h=0.28, fill=None)
        kit.bottles(B, 0.3, 2.5, 0.28, 0.0, n=3, seed=7)
        kit.crate(B, 0.3, 2.9, 0.0, 0.0, w=0.5, d=0.34, h=0.28, fill=None)
    kit.stool(B, 0.0, 3.4, 0.0, 90.0)


def _sign(B, kx0, kx1, ky0, kh):
    """The trade's sign: an atlas plate on the fascia (or a plain accent fascia)."""
    if SIGN:
        kit.sign_plate(B, (kx0 + kx1) / 2 - 0.08, ky0 - 0.37, 0.0, 0.0, name=SIGN, w=1.2, zc=kh + 0.14)


def _cone(d, x, y, z, mat, scoop="CT_PlasterPink", s=1.0):
    """An ice-cream cone with one scoop (z = where the tip rests... the cone's tip points down)."""
    cyl(d, "CT_WoodLight", x, y, z, z + 0.1 * s, 0.006 * s, 4)
    cyl(d, "CT_WoodLight", x, y, z + 0.1 * s, z + 0.2 * s, 0.03 * s, 6)
    blob(d, scoop, x, y, z + 0.25 * s, 0.045 * s, 6)


def _bucket(d, x, y, rnd, mats, n=7, h=0.32, r=0.13):
    """A steel bucket with a bunch of flowers (stems and heads)."""
    cyl(d, "CT_Steel", x, y, 0.0, h, r, 7)
    cyl(d, "CT_Leaf", x, y, h - 0.02, h + 0.12, r * 0.55, 6)
    for k in range(n):
        a = rnd.uniform(0, 6.28)
        rr = rnd.uniform(0.0, r * 0.6)
        fx, fy, fz = x + rr * math.cos(a), y + rr * math.sin(a), h + rnd.uniform(0.3, 0.45)
        box(d, "CT_Leaf", fx - 0.006, fx + 0.006, fy - 0.006, fy + 0.006, h + 0.1, fz, faces="xXyY")
        blob(d, rnd.choice(mats), fx, fy, fz, rnd.uniform(0.045, 0.06), 5)


def _dress(B, rnd):
    """Everything that tells the trade at a glance, outside and on the shelf (no new things, no new lamps)."""
    d = B.detail
    kx0, kx1, ky0, ky1, kh = 0.6, 2.8, 2.2, 4.2, 2.45
    sh = 1.02
    r = random.Random(7 + len(TRADE))
    _sign(B, kx0, kx1, ky0, kh)
    if TRADE == "news":
        kit.magazine_rack(B, 0.56, 3.5, 0.0, 270.0, w=0.8, seed=2, solid=False)
        # a bundle of newspapers tied up by the front corner
        kit.newspaper(B, 0.35, 1.95, 0.0, 15.0, seed=9)
        kit.newspaper(B, 0.35, 1.95, 0.03, -10.0, seed=10)
        kit.newspaper(B, 0.35, 1.95, 0.06, 5.0, seed=11)
        kit.sandwich_board(B, 0.1, 1.5, 0.0, 12.0, name="news_0")
    elif TRADE == "ice":
        kit.awning(B, 1.7, ky0 - 0.02, 0.0, 0.0, w=1.9, proj=0.6, zt=2.35, drop=0.3, mat=ACCENT, mat2="CT_TileWhite")
        kit.ice_cream_chest(B, -0.15, 1.9, 0.0, 0.0, solid=False)
        kit.sandwich_board(B, 0.55, 1.2, 0.0, -10.0, name="menu_1")
        for k in range(3):
            _cone(d, 1.2 + 0.4 * k, ky0 - 0.12, sh, "CT_WoodLight", ["CT_PlasterPink", "CT_TileWhite", "CT_PlasterYellow"][k])
        # a big cone sign on a pole at the kiosk's front corner: waffle cone and three scoops
        cx, cy, z0 = 2.9, 1.75, 1.5
        cyl(d, "CT_Steel", cx, cy, 0.0, z0, 0.02, 4)
        cyl(d, "CT_Steel", cx, cy, 0.0, 0.03, 0.12, 8, bottom=True)
        cyl(d, "CT_WoodLight", cx, cy, z0, z0 + 0.04, 0.015, 4)
        cyl(d, "CT_WoodLight", cx, cy, z0 + 0.04, z0 + 0.3, 0.12, 8, bottom=True)
        for k, mt in enumerate(("CT_PlasterPink", "CT_TileWhite", "CT_PlasterYellow")):
            blob(d, mt, cx + 0.02 * (k % 2), cy, z0 + 0.38 + 0.13 * k, 0.12 - 0.01 * k, 8)
    elif TRADE == "tobacco":
        # a lighter, neat counter-top display: a steel frame with packs and lighters, two small jars of matches
        box(d, "CT_Steel", 1.0, 1.4, ky0 - 0.26, ky0 - 0.06, sh, sh + 0.02)
        for k in range(8):
            box(d, ["CT_TileWhite", "CT_PlasterRed", "CT_PlasterGreen", "CT_PlasterYellow"][k % 4],
                1.03 + 0.045 * k, 1.07 + 0.045 * k, ky0 - 0.22, ky0 - 0.18, sh + 0.02, sh + 0.09, faces="xXyYZ")
        for k in range(5):
            box(d, "CT_SteelRed" if k % 2 else "CT_PlasterBlue", 1.05 + 0.07 * k, 1.09 + 0.07 * k, ky0 - 0.12, ky0 - 0.09,
                sh + 0.02, sh + 0.08, faces="xXyYZ")
        cyl(d, "CT_Glass", 1.65, ky0 - 0.14, sh, sh + 0.1, 0.04, 6)
        cyl(d, "CT_Wood", 1.8, ky0 - 0.14, sh, sh + 0.06, 0.03, 6)
        # an ash stand at the kerb of the kiosk
        cyl(d, "CT_Steel", 0.35, 1.85, 0.0, 0.8, 0.02, 4)
        cyl(d, "CT_Steel", 0.35, 1.85, 0.8, 0.88, 0.1, 8, bottom=True)
        cyl(d, "CT_Steel", 0.35, 1.85, 0.0, 0.03, 0.14, 8, bottom=True)
        kit.sandwich_board(B, 0.1, 1.3, 0.0, 10.0, name="poster_health")
    elif TRADE == "flowers":
        kit.awning(B, 1.7, ky0 - 0.02, 0.0, 0.0, w=1.9, proj=0.6, zt=2.35, drop=0.3, mat=ACCENT, mat2="CT_TileWhite")
        fm = ["CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterPink", "CT_TileWhite", "CT_PlasterBlue"]
        for k, (bx, by) in enumerate(((0.28, 1.95), (0.58, 1.55), (0.05, 1.5), (2.2, 1.75), (2.5, 1.5), (2.85, 1.85))):
            _bucket(d, bx, by, r, fm, n=8)
        for k in range(3):
            kit.vase(B, 1.15 + 0.4 * k, ky0 - 0.12, sh, 0.0, seed=k + 20)
        kit.planter(B, 1.4, 1.0, 0.0, 0.0, w=1.2, d=0.4, h=0.35, seed=3, solid=False)
        kit.plant(B, 0.15, 2.45, 0.0, 0.0, kind="fern", pot="CT_PlasterYellow", h=0.7, seed=4)
        kit.plant(B, 3.0, 2.3, 0.0, 0.0, kind="ficus", pot="CT_PlasterRed", h=1.2, seed=5)
        # watering can by the door side
        wx, wy = 0.8, 1.75
        cyl(d, "CT_SteelWhite", wx, wy, 0.0, 0.22, 0.09, 8, top=False)
        box(d, "CT_SteelWhite", wx - 0.1, wx + 0.1, wy - 0.015, wy + 0.015, 0.22, 0.24)
        for t in range(4):
            box(d, "CT_SteelWhite", wx - 0.2 - 0.03 * t, wx - 0.1 - 0.03 * t, wy - 0.015, wy + 0.015, 0.1 + 0.08 * t, 0.13 + 0.08 * t)
        cyl(d, "CT_SteelWhite", wx - 0.31, wy, 0.4, 0.43, 0.035, 6)
        box(d, "CT_SteelWhite", wx + 0.08, wx + 0.12, wy - 0.015, wy + 0.015, 0.0, 0.3)
        box(d, "CT_SteelWhite", wx - 0.1, wx + 0.1, wy - 0.015, wy + 0.015, 0.3, 0.34)
    elif TRADE == "cable":
        # timetable and route boards at the kiosk front, a ticket window sign and a model cabin on the roof
        kit.notice_stand(B, -0.1, 1.55, 0.0, 0.0, w=0.8, h=0.6, front="chart_timetable_cable", back="map_town", seed=1)
        kit.sandwich_board(B, 3.0, 1.3, 0.0, -8.0, name="travel_0")
        kit.poster(B, 1.7, ky0, 0.0, 0.0, name="sign_till", w=0.3, zc=2.05)
        for k in range(4):
            box(d, "CT_Paper", 1.15 + 0.12 * k, 1.25 + 0.12 * k, ky0 - 0.2, ky0 - 0.08, sh, sh + 0.004 + 0.003 * k)
        cx, cy, z0 = 2.3, ky0 - 0.12, sh
        box(d, "CT_Steel", cx - 0.01, cx + 0.01, cy - 0.01, cy + 0.01, z0, z0 + 0.04, faces="xXyY")
        box(d, ACCENT, cx - 0.1, cx + 0.1, cy - 0.06, cy + 0.06, z0 + 0.04, z0 + 0.19)
        box(d, "CT_Glass", cx - 0.085, cx + 0.085, cy - 0.065, cy - 0.06, z0 + 0.09, z0 + 0.16, faces="y")
        box(d, "CT_TileWhite", cx - 0.11, cx + 0.11, cy - 0.07, cy + 0.07, z0 + 0.19, z0 + 0.205)
        box(d, "CT_Steel", cx - 0.008, cx + 0.008, cy - 0.008, cy + 0.008, z0 + 0.205, z0 + 0.28, faces="xXyY")
    elif TRADE == "repair":
        kit.shoe_rack(B, 0.56, 3.75, 0.0, 270.0, w=0.8, rows=3, seed=6, solid=False)
        # key board on the left wall: a wooden board with brass keys on hooks
        box(d, "CT_Wood", kx0 - 0.03, kx0, 3.7, 4.04, 1.15, 1.65)
        for row in range(3):
            for k in range(4):
                box(d, "CT_Brass", kx0 - 0.045, kx0 - 0.03, 3.75 + 0.07 * k, 3.78 + 0.07 * k, 1.25 + 0.14 * row, 1.36 + 0.14 * row)
        # a tray of watches and a few pairs of shoes on the serving shelf
        box(d, "CT_Wood", 1.3, 1.57, ky0 - 0.28, ky0 - 0.06, sh, sh + 0.02)
        for k in range(6):
            cyl(d, ["CT_Brass", "CT_Steel", "CT_TileWhite"][k % 3], 1.36 + 0.08 * (k % 3), ky0 - 0.2 + 0.1 * (k // 3), sh + 0.02, sh + 0.035, 0.028, 8)
        for k in range(2):
            box(d, "CT_Leather", 1.7 + 0.25 * k, 1.95 + 0.25 * k - 0.05, ky0 - 0.24, ky0 - 0.06, sh, sh + 0.09, faces="xXyYZ")
        # a gooseneck lamp over the tiny workbench inside (an unlit fixture: the kiosk keeps its single lamp)
        kit.table_lamp(B, 2.35, 2.55, 0.92, 0.0, shade="CT_SteelRed", light=False)
        kit.tools(B, 1.35, 2.5, 0.92, 0.0)
        # a big brass key on the fascia instead of a sign
        cx, cz, yk = 1.6, kh + 0.14, ky0 - 0.37 - 0.012
        for k in range(8):
            a0, a1 = 2 * math.pi * k / 8, 2 * math.pi * (k + 1) / 8
            for rr0, rr1 in ((0.05, 0.11),):
                B.detail.face("CT_Brass", [(cx - 0.45 + rr0 * math.cos(a0), yk, cz + rr0 * math.sin(a0)),
                                           (cx - 0.45 + rr1 * math.cos(a0), yk, cz + rr1 * math.sin(a0)),
                                           (cx - 0.45 + rr1 * math.cos(a1), yk, cz + rr1 * math.sin(a1)),
                                           (cx - 0.45 + rr0 * math.cos(a1), yk, cz + rr0 * math.sin(a1))])
        box(d, "CT_Brass", cx - 0.34, cx + 0.45, yk - 0.015, yk, cz - 0.02, cz + 0.02, faces="xXyYZz")
        for k in range(3):
            box(d, "CT_Brass", cx + 0.2 + 0.08 * k, cx + 0.26 + 0.08 * k, yk - 0.015, yk, cz - 0.1, cz - 0.02, faces="xXyYZz")
        kit.wall_clock(B, 2.67, ky0, 0.0, 0.0, zc=1.75, r=0.1)


# ====================================================================================== second pass
def _drum_bin(B, x, y, h=0.82, r=0.19):
    """A street litter bin: a steel drum on a post, a rim, a dished lid with a slot and a paint stripe."""
    p = kit._P(B, x, y, 0.0, 0.0, "drum_bin")
    p.kind = "prop"
    p.cyl("CT_Steel", 0, 0, 0.0, 0.05, r * 0.9, 8, bottom=True)
    p.cyl("CT_Steel", 0, 0, 0.05, 0.28, 0.02, 4)
    p.cyl("CT_SteelRed", 0, 0, 0.28, h - 0.08, r, 10, bottom=True)
    p.cyl("CT_SteelWhite", 0, 0, 0.5, 0.56, r + 0.004, 10, top=False)
    p.cyl("CT_Steel", 0, 0, h - 0.08, h - 0.02, r + 0.02, 10, bottom=True)
    p.box("CT_ConcreteDark", -0.07, 0.07, -r - 0.02, -r + 0.005, h - 0.2, h - 0.15, f="xXyYZ")
    p.cyl("CT_Steel", 0, 0, h - 0.02, h + 0.01, r - 0.04, 10)
    return p.done()


def _payphone(B, x, y, yaw):
    """A street payphone: steel post, a red hood with a dial, a coin slot, a directory shelf, a rain lip."""
    p = kit._P(B, x, y, 0.0, yaw, "payphone")
    p.kind = "prop"
    p.box("CT_Steel", -0.05, 0.05, -0.04, 0.06, 0.0, 1.15)
    p.box("CT_Steel", -0.16, 0.16, -0.14, 0.14, 0.0, 0.03, f="xXyYZ")
    p.bev("CT_PlasterRed", -0.17, 0.17, -0.17, 0.12, 1.05, 1.62, 0.02, 0.025)
    p.box("CT_PlasterRed", -0.19, 0.19, -0.22, -0.1, 1.6, 1.64)
    p.card_front("dial_phone", -0.1, 0.1, 1.2, 1.4, -0.175)
    p.box("CT_Steel", -0.045, 0.045, -0.178, -0.172, 1.43, 1.5, f="y")
    p.box("CT_Lacquer", -0.14, -0.1, -0.215, -0.17, 1.25, 1.5)
    p.box("CT_Lacquer", -0.145, -0.095, -0.225, -0.17, 1.45, 1.52)
    p.box("CT_Lacquer", -0.145, -0.095, -0.225, -0.17, 1.2, 1.27)
    p.box("CT_Wood", -0.18, 0.18, -0.26, -0.12, 0.95, 0.98)
    p.box("CT_Paper", -0.12, 0.12, -0.22, -0.14, 0.98, 1.03, f="xXyYZ")
    return p.done()


def _fountain(B, x, y, yaw):
    """A drinking fountain: a concrete column, a shallow stone basin, a brass spout and a push button."""
    p = kit._P(B, x, y, 0.0, yaw, "fountain")
    p.kind = "prop"
    p.cyl("CT_Concrete", 0, 0, 0.0, 0.08, 0.22, 8, bottom=True)
    p.cyl("CT_Concrete", 0, 0, 0.08, 0.78, 0.13, 8)
    p.cyl("CT_ConcreteDark", 0, 0, 0.78, 0.9, 0.3, 10, r1=0.28, bottom=True)
    p.cyl("CT_Steel", 0, 0, 0.9, 0.905, 0.22, 10)
    p.box("CT_Brass", -0.015, 0.015, 0.1, 0.13, 0.9, 1.0, f="xXyYZ")
    p.rod("CT_Brass", (0.0, 0.115, 1.0), (0.0, -0.02, 1.0), 0.03, 5)
    p.cyl("CT_Brass", 0.0, -0.1, 0.78, 0.84, 0.03, 5, r1=0.025)
    p.box("CT_PlasterBlue", -0.14, 0.14, 0.28, 0.3, 0.62, 0.9, f="y")
    return p.done()


def _weigh_machine(B, x, y, yaw):
    """A street weighing machine: a cast column with a big dial on top, a ticket slot and a ribbed platform."""
    p = kit._P(B, x, y, 0.0, yaw, "weigh_machine")
    p.kind = "prop"
    p.bev("CT_Steel", -0.24, 0.24, -0.28, 0.2, 0.0, 0.06, 0.01, 0.03)
    p.box("CT_Rubber", -0.2, 0.2, -0.26, 0.0, 0.06, 0.075, f="Z")
    for k in range(5):
        p.box("CT_Steel", -0.19, 0.19, -0.24 + 0.05 * k, -0.22 + 0.05 * k, 0.075, 0.085)
    p.bev("CT_PlasterGreen", -0.15, 0.15, 0.05, 0.2, 0.06, 1.0, 0.012, 0.02)
    p.bev("CT_PlasterGreen", -0.2, 0.2, 0.0, 0.24, 1.0, 1.3, 0.02, 0.04)
    p.cyl("CT_Steel", 0, 0.0, 1.3, 1.34, 0.17, 10)
    p.card_front("clock_2", -0.15, 0.15, 1.02, 1.28, -0.002)
    p.box("CT_Steel", -0.07, 0.07, 0.04, 0.052, 0.62, 0.66, f="y")
    p.box("CT_Brass", -0.04, 0.04, 0.0, 0.01, 1.3, 1.31)
    return p.done()


def _wrap_table(B, x, y, yaw):
    """The florist's wrapping bench: a table with a kraft-paper roll on a holder, ribbon spools, scissors, a half-wrapped bunch."""
    p = kit._P(B, x, y, 0.0, yaw, "wrap_table")
    p.kind = "prop"
    p.box("CT_Wood", -0.5, 0.5, -0.25, 0.25, 0.8, 0.84)
    for sx in (-0.45, 0.45):
        for sy in (-0.2, 0.2):
            p.box("CT_Wood", sx - 0.025, sx + 0.025, sy - 0.025, sy + 0.025, 0.0, 0.8, f="xXyY")
    p.box("CT_Wood", -0.46, 0.46, -0.22, 0.22, 0.2, 0.22)
    # roll of kraft paper on a bar between two uprights
    for sx in (-0.45, 0.2):
        p.box("CT_Wood", sx, sx + 0.025, 0.12, 0.15, 0.84, 1.1, f="xXyY")
    p.rod("CT_Steel", (-0.45, 0.135, 1.04), (0.225, 0.135, 1.04), 0.015, 4)
    # the roll itself: an x-axis cylinder drawn as a prism rotated by hand
    n = 9
    pts = [(0.12 * math.cos(2 * math.pi * k / n), 1.04 + 0.12 * math.sin(2 * math.pi * k / n)) for k in range(n)]
    for k in range(n):
        (ay, az), (by, bz) = pts[k], pts[(k + 1) % n]
        p.face("CT_WoodLight", [(-0.4, 0.135 + ay, az), (0.17, 0.135 + ay, az), (0.17, 0.135 + by, bz), (-0.4, 0.135 + by, bz)][::-1])
    p.face("CT_WoodLight", [(0.17, 0.135 + a, b) for a, b in pts])
    p.face("CT_WoodLight", [(-0.4, 0.135 + a, b) for a, b in pts][::-1])
    # sheet pulled across the bench
    p.box("CT_WoodLight", -0.4, 0.17, -0.2, 0.02, 0.84, 0.845, f="Z")
    # ribbon spools, scissors, a bunch lying wrapped
    for k, mt in enumerate(("CT_PlasterRed", "CT_PlasterPink", "CT_PlasterYellow")):
        p.cyl(mt, 0.28 + 0.07 * k, -0.15, 0.84, 0.9, 0.032, 6, bottom=True)
    p.box("CT_Steel", 0.25, 0.4, -0.05, -0.04, 0.84, 0.846)
    p.box("CT_Steel", 0.25, 0.4, -0.02, -0.01, 0.84, 0.846)
    for k in range(5):
        a = -0.35 + 0.05 * k
        p.rod("CT_Leaf", (-0.38, -0.12 + 0.02 * k, 0.87), (-0.12, -0.1 + 0.02 * k, 0.88), 0.012, 4)
        p.face(["CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterPink", "CT_TileWhite", "CT_PlasterRed"][k],
               [(a - 0.03, -0.14, 0.88), (a + 0.03, -0.14, 0.88), (a + 0.03, -0.14, 0.93), (a - 0.03, -0.14, 0.93)])
    return p.done()


def _pipe_rack(B, x, y, yaw):
    """[wall] A tobacconist's pipe rack: a varnished board with two rails and six pipes resting bowl up, a brass plate."""
    p = kit._P(B, x, y, 0.0, yaw, "pipe_rack")
    p.kind = "wall"
    p.box("CT_Wood", -0.24, 0.24, -0.02, 0.0, 1.0, 1.55)
    for zz in (1.2, 1.4):
        p.box("CT_WoodLight", -0.22, 0.22, -0.07, -0.02, zz - 0.012, zz)
        p.box("CT_WoodLight", -0.22, 0.22, -0.07, -0.062, zz, zz + 0.03)
    for k in range(6):
        px = -0.19 + 0.076 * k
        zb = 1.2 if k % 2 == 0 else 1.4
        p.rod("CT_Lacquer", (px, -0.045, zb + 0.015), (px + 0.02, -0.05, zb + 0.15), 0.012, 4)
        p.cyl("CT_Wood" if k % 3 else "CT_Brass", px + 0.02, -0.05, zb + 0.14, zb + 0.2, 0.021, 6, bottom=True)
    p.box("CT_Brass", -0.08, 0.08, -0.025, -0.02, 1.05, 1.1, f="y")
    return p.done()


def _key_machine(B, x, y, z, yaw=0.0):
    """A key-duplicating machine: cast body, a clamp vice, a grinding wheel with a guard, a tray of blanks."""
    p = kit._P(B, x, y, z, yaw, "key_machine")
    p.kind = "prop"
    p.bev("CT_SteelRed", -0.17, 0.17, -0.11, 0.11, 0.0, 0.14, 0.012, 0.02)
    p.box("CT_Steel", -0.1, 0.1, -0.12, -0.05, 0.14, 0.2, f="xXyYZ")
    p.cyl("CT_Aluminium", 0.0, 0.0, 0.14, 0.17, 0.001, 3)
    n = 10
    for k in range(n):
        a0, a1 = 2 * math.pi * k / n, 2 * math.pi * (k + 1) / n
        r = 0.07
        p.face("CT_Steel", [(0.1 + r * math.cos(a0) * 0.2, -0.07, 0.17 + r * math.sin(a0)), (0.1 + r * math.cos(a1) * 0.2, -0.07, 0.17 + r * math.sin(a1)),
                           (0.1 + r * math.cos(a1) * 0.2, -0.04, 0.17 + r * math.sin(a1)), (0.1 + r * math.cos(a0) * 0.2, -0.04, 0.17 + r * math.sin(a0))])
    p.box("CT_Aluminium", -0.07, 0.07, -0.1, -0.02, 0.2, 0.215)
    p.box("CT_Brass", -0.05, 0.05, -0.095, -0.07, 0.215, 0.225, f="xXyYZ")
    p.box("CT_Steel", -0.15, -0.09, 0.0, 0.1, 0.14, 0.15)
    for k in range(4):
        p.box("CT_Brass", -0.145 + 0.014 * k, -0.135 + 0.014 * k, 0.02 + 0.015 * k, 0.09, 0.15, 0.16, f="xXyYZ")
    return p.done()


def _clock_row(B, x, y, yaw, zc, ys, r=0.12):
    for k, yy in enumerate(ys):
        kit.wall_clock(B, x, yy, 0.0, yaw, zc=zc + (0.05 if k % 2 else -0.03), r=r - 0.01 * (k % 2), face="clock_%d" % (k % 4))


def _ticket_rolls(B, x, y, z, rnd):
    """A cable-car ticket dispenser: a steel frame with three paper rolls and a hand punch on a chain."""
    p = kit._P(B, x, y, z, 0.0, "ticket_rolls")
    p.kind = "prop"
    p.box("CT_Steel", -0.16, 0.16, -0.06, 0.06, 0.0, 0.015)
    for k, mt in enumerate(("CT_PlasterYellow", "CT_PlasterBlue", "CT_TileWhite")):
        cx = -0.1 + 0.1 * k
        p.cyl(mt, cx, 0.0, 0.015, 0.1, 0.035, 8)
        p.box("CT_Steel", cx - 0.004, cx + 0.004, -0.045, 0.045, 0.1, 0.108)
    p.box("CT_Steel", -0.16, -0.14, -0.05, 0.05, 0.015, 0.12, f="xXyYZ")
    p.box("CT_Steel", 0.14, 0.16, -0.05, 0.05, 0.015, 0.12, f="xXyYZ")
    p.rod("CT_Steel", (0.0, 0.1, 0.0), (0.0, 0.18, 0.0), 0.01, 4)
    return p.done()


def _deeper(B, rnd):
    """Second pass: the trade's inside, the stop's furniture, one lamp, the extra things and spots."""
    d = B.detail
    kx0, kx1, ky0, ky1, kh = 0.6, 2.8, 2.2, 4.2, 2.45
    sh = 1.02
    r = random.Random(900 + len(TRADE))
    # ---- the stop itself: a drum bin (every kiosk), a lamp under the roof
    _drum_bin(B, -0.42, 2.5)
    B.thing("stop_bin", "board", (-0.42, 2.5, 0.8), 0.0, topic="bin")
    B.lamp((-1.5, 2.6, 2.75), lumens=420, colour="#FFE2B8", kind="public", radius=4.0)
    box(d, "CT_Steel", -1.505, -1.495, 2.595, 2.605, 2.76, 2.95, faces="xXyY")
    B.spot("wait", "stand", (-1.9, 2.4, 0.0), 0.0)
    if TRADE == "news":
        # a payphone against the pier-side, the crossword board, a bundle of morning papers inside
        _payphone(B, -2.72, 2.7, 90.0)
        B.thing("k1_payphone", "machine", (-2.72, 2.7, 1.3), 90.0, what="payphone")
        B.spot("phone", "stand", (-2.1, 2.7, 0.0), 270.0)
        box(d, "CT_Wood", 0.02, 0.44, 4.1, 4.2, 0.95, 1.7)
        box(d, "CT_Floor", 0.05, 0.41, 4.085, 4.1, 1.02, 1.62, faces="y")
        kit.poster(B, 0.23, 4.085, 0.0, 0.0, name="paper_typed", w=0.3, zc=1.33)
        for k in range(5):
            box(d, "CT_Paper", 0.07 + 0.065 * k, 0.11 + 0.065 * k, 4.075, 4.085, 1.54, 1.58, faces="y")
        B.thing("k1_crossword", "board", (0.23, 4.08, 1.35), 0.0, topic="crossword")
        for k in range(3):
            kit.newspaper(B, 1.0 + 0.28 * k, 3.4, 0.96, (k * 31) % 40 - 20, seed=40 + k)
        B.thing("k1_bench_paper", "craft", (-0.55, 3.78, 0.5), 0.0, what="pencil")
    elif TRADE == "ice":
        # wafer cones in a tube, a scoop jar, a thermometer, a bell on the hatch, the day's flavours on the pole sign
        for k in range(3):
            cyl(d, "CT_WoodLight", 2.3 + 0.07 * k, 2.55, 0.96, 1.2, 0.03, 6)
        cyl(d, "CT_Glass", 1.15, 2.6, 0.96, 1.1, 0.07, 8)
        cyl(d, "CT_Steel", 1.15, 2.6, 1.1, 1.13, 0.075, 8)
        for k in range(3):
            blob(d, ["CT_PlasterPink", "CT_TileWhite", "CT_PlasterYellow"][k], 1.15 + 0.5 * k, 2.8, 0.995, 0.04, 6)
            cyl(d, "CT_Steel", 1.15 + 0.5 * k, 2.8, 0.96, 0.99, 0.045, 6, bottom=True)
        cyl(d, "CT_Brass", 1.05, ky0 - 0.12, sh, sh + 0.03, 0.045, 8)
        cyl(d, "CT_Brass", 1.05, ky0 - 0.12, sh + 0.03, sh + 0.05, 0.015, 6)
        B.thing("k2_bell", "machine", (1.05, ky0 - 0.12, sh + 0.04), 0.0, what="bell")
        box(d, "CT_SteelWhite", 2.57, 2.62, ky0 - 0.04, ky0 - 0.02, 1.2, 1.58, faces="xXyYZ")
        box(d, "CT_PlasterRed", 2.578, 2.612, ky0 - 0.041, ky0 - 0.039, 1.25, 1.55, faces="y")
        B.thing("k2_flavours", "board", (0.55, 1.2, 0.55), 0.0, topic="flavours")
        B.thing("k2_cone_sign", "board", (2.9, 1.75, 1.9), 0.0, topic="coneSign")
        # a parasol-free shade: a crate stool for the queue and a tin of spoons on the freezer
        kit.crate(B, 3.0, 2.6, 0.0, 0.0, w=0.4, d=0.3, h=0.3, fill="CT_PlasterBlue", seed=2)
        kit.crate(B, 3.0, 2.6, 0.3, 0.0, w=0.4, d=0.3, h=0.2, fill="CT_PlasterYellow", seed=3)
        B.spot("queue", "stand", (1.4, 1.1, 0.0), 180.0)
        _fountain(B, -2.72, 2.6, 90.0)
        B.thing("k2_fountain", "machine", (-2.72, 2.6, 1.0), 90.0, what="fountain")
        B.spot("drink", "stand", (-2.1, 2.6, 0.0), 270.0)
    elif TRADE == "tobacco":
        _pipe_rack(B, kx1 + 0.0, 2.7, 90.0)
        B.thing("k3_pipes", "board", (kx1 + 0.04, 2.7, 1.3), 90.0, topic="pipes")
        cyl(d, "CT_Glass", 0.93, ky0 - 0.14, sh, sh + 0.16, 0.06, 8)
        for k in range(6):
            box(d, ["CT_PlasterRed", "CT_PlasterYellow", "CT_TileWhite"][k % 3], 0.9 + 0.02 * (k % 3), 0.92 + 0.02 * (k % 3),
                ky0 - 0.16 + 0.03 * (k // 3), ky0 - 0.13 + 0.03 * (k // 3), sh + 0.01 + 0.045 * (k % 3), sh + 0.04 + 0.045 * (k % 3), faces="xXyYZ")
        cyl(d, "CT_Steel", 0.93, ky0 - 0.14, sh + 0.16, sh + 0.175, 0.062, 8)
        B.thing("k3_jar", "board", (0.93, ky0 - 0.14, sh + 0.1), 0.0, topic="matches")
        _weigh_machine(B, -1.9, 2.45, 0.0)
        B.thing("k3_scales", "machine", (-1.9, 2.45, 1.2), 0.0, what="scales")
        B.spot("weigh", "stand", (-1.9, 1.9, 0.0), 0.0)
    elif TRADE == "flowers":
        _wrap_table(B, -1.0, 1.8, 0.0)
        B.thing("k4_wrap", "craft", (-1.0, 1.8, 0.95), 0.0, what="wrap")
        B.spot("wrap", "stand", (-1.0, 1.25, 0.0), 180.0)
        # a bucket stand of three tiers by the right corner and a hand cart with a basket of sunflowers
        fm = ["CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterPink", "CT_TileWhite", "CT_PlasterBlue"]
        box(d, "CT_WoodLight", 2.9, 3.25, 1.05, 1.5, 0.0, 0.04, faces="xXyYZ")
        box(d, "CT_WoodLight", 2.9, 3.25, 1.05, 1.5, 0.3, 0.33, faces="xXyYZ")
        for sy in (1.07, 1.48):
            box(d, "CT_Wood", 3.2, 3.24, sy - 0.015, sy + 0.015, 0.0, 0.62, faces="xXyY")
        _bucket(d, 3.05, 1.18, r, fm, n=7, h=0.28, r=0.1)
        _bucket(d, 3.05, 1.38, r, fm, n=7, h=0.28, r=0.1)
        # sunflowers in a tin tub: tall stems, big discs facing the street
        for k in range(5):
            fx, fy = -0.55 + 0.12 * k, 2.75 + 0.05 * (k % 2)
            fz = 0.75 + 0.1 * (k % 3)
            box(d, "CT_Leaf", fx - 0.008, fx + 0.008, fy - 0.008, fy + 0.008, 0.2, fz, faces="xXyY")
            cyl(d, "CT_PlasterYellow", fx, fy, fz, fz + 0.015, 0.075, 8, bottom=True)
            cyl(d, "CT_Leather", fx, fy, fz + 0.015, fz + 0.025, 0.035, 6)
        cyl(d, "CT_Steel", -0.35, 2.77, 0.0, 0.22, 0.17, 8, top=False)
        kit.poster(B, 0.23, 4.2 - 0.02, 0.0, 0.0, name="paper_letter", w=0.3, zc=1.45)
        box(d, "CT_PlasterPink", 0.03, 0.43, 4.17, 4.19, 1.15, 1.8, faces="xXyYZz")
        B.thing("k4_lilac", "board", (0.23, 4.15, 1.45), 0.0, topic="lilac")
        B.thing("k4_watering", "machine", (0.8, 1.75, 0.3), 0.0, what="watering")
        # ribbons and dried bunches hung under the awning edge
        for k in range(6):
            box(d, ["CT_PlasterRed", "CT_PlasterPink", "CT_PlasterYellow"][k % 3], 0.95 + 0.3 * k, 0.97 + 0.3 * k, ky0 - 0.65, ky0 - 0.64,
                1.95 - 0.04 * (k % 3), 2.3, faces="xXyY")
    elif TRADE == "cable":
        # the route diagram (a wide board of the lines), the ticket rolls on the hatch, a drinking fountain and a lost-things box
        kit.poster(B, 0.23, 4.2 - 0.02, 0.0, 0.0, name="chart_timetable_cable", w=0.42, zc=1.5)
        box(d, "CT_SteelWhite", 0.0, 0.46, 4.17, 4.19, 1.2, 1.82, faces="xXyYZz")
        B.thing("k5_diagram", "board", (0.23, 4.15, 1.5), 0.0, topic="route")
        _ticket_rolls(B, 0.98, ky0 - 0.14, sh, r)
        B.thing("k5_rolls", "machine", (0.98, ky0 - 0.14, sh + 0.1), 0.0, what="punch")
        _fountain(B, -2.72, 2.6, 90.0)
        B.thing("k5_fountain", "machine", (-2.72, 2.6, 1.0), 90.0, what="fountain")
        B.spot("drink", "stand", (-2.1, 2.6, 0.0), 270.0)
        # a glass-fronted box with a lone mitten, a thermos, a hat on the left wall: "lost in the cabins"
        box(d, "CT_Wood", kx0 - 0.1, kx0, 3.1, 3.9, 0.22, 0.95)
        box(d, "CT_Glass", kx0 - 0.105, kx0 - 0.1, 3.14, 3.86, 0.26, 0.91, faces="x")
        box(d, "CT_PlasterRed", kx0 - 0.095, kx0 - 0.03, 3.2, 3.38, 0.28, 0.37, faces="xXyYZ")
        cyl(d, "CT_PlasterBlue", kx0 - 0.07, 3.55, 0.28, 0.46, 0.045, 6, bottom=True)
        box(d, "CT_Leather", kx0 - 0.095, kx0 - 0.03, 3.62, 3.85, 0.28, 0.35, faces="xXyYZ")
        B.thing("k5_lost", "board", (kx0 - 0.1, 3.5, 0.6), 270.0, topic="lostcabin")
        # a thick book on a chain on a lectern post at the kiosk's right corner: "Passengers' questions"
        box(d, "CT_Steel", 2.9, 2.94, 1.98, 2.02, 0.0, 1.0, faces="xXyY")
        box(d, "CT_Steel", 2.8, 3.04, 1.92, 2.08, 0.0, 0.03, faces="xXyYZ")
        box(d, "CT_Wood", 2.84, 3.0, 1.9, 2.1, 1.0, 1.03)
        box(d, "CT_Leather", 2.85, 2.99, 1.93, 2.07, 1.03, 1.07, faces="xXyYZ")
        box(d, "CT_Paper", 2.86, 2.98, 1.94, 2.06, 1.07, 1.075, faces="Z")
        B.thing("k5_ledger", "craft", (2.92, 2.0, 1.12), 0.0, what="ledger")
        # a cabin-shaped timetable lamp-box over the hatch (unlit fixture)
        box(d, ACCENT, 0.9, 2.5, ky0 - 0.12, ky0 - 0.02, kh - 0.2, kh - 0.05, faces="xXyYZ")
    elif TRADE == "repair":
        _key_machine(B, 1.07, ky0 - 0.16, sh, 0.0)
        B.thing("k6_keycut", "craft", (1.07, ky0 - 0.16, sh + 0.12), 0.0, what="keycut")
        # the bench inside: a radio under repair with its back off, a shoe on a last, a jar of screws, a lens loupe
        kit.radio(B, 1.05, 3.45, 0.7, 90.0, solid=False)
        kit.radio(B, 0.36, 3.65, 1.1, 270.0, mat="CT_PlasterRed", solid=False)
        B.thing("k6_radios", "machine", (0.36, 3.65, 1.26), 270.0, what="radio")
        kit.radio(B, 1.05, 3.9, 0.7, 90.0, mat="CT_Lacquer", solid=False)
        cyl(d, "CT_Glass", 1.2, 3.15, 0.7, 0.78, 0.04, 6)
        box(d, "CT_Leather", 1.22, 1.32, 3.7, 3.85, 0.7, 0.76)
        # shelf of radios and tins over the bench, left inner wall
        box(d, "CT_Wood", 0.76, 0.97, 3.1, 4.1, 1.55, 1.57)
        # a column of clocks on a board on the back wall, each showing its own time
        box(d, "CT_Wood", 0.0, 0.46, 4.16, 4.19, 1.28, 2.3, faces="xXyYZz")
        for k, zc in enumerate((1.5, 1.8, 2.1)):
            kit.wall_clock(B, 0.23, 4.16, 0.0, 0.0, zc=zc, r=0.1, face="clock_%d" % (k + 1))
        B.thing("k6_clocks", "board", (0.23, 4.1, 1.8), 0.0, topic="clocks")
        box(d, "CT_WoodLight", 0.0, 0.22, 4.0, 4.16, 1.0, 1.02)
        B.thing("k6_shoes", "board", (kx0 - 0.1, 3.75, 0.6), 270.0, topic="shoes")
        # the pocket watch on a velvet pad on a little ledge under the clocks: the one that ticks loudly
        box(d, "CT_PlasterRed", 0.02, 0.2, 4.02, 4.14, 1.02, 1.032, faces="xXyYZ")
        cyl(d, "CT_Brass", 0.11, 4.08, 1.032, 1.05, 0.04, 8)
        box(d, "CT_TileWhite", 0.095, 0.125, 4.06, 4.1, 1.051, 1.053, faces="Z")
        B.thing("k6_watch", "machine", (0.1, 4.08, 1.1), 0.0, what="pocketwatch")


# ====================================================================================== third pass
GOODS_BY_TRADE = {            # atlas goods labels label_NN = GOODS[NN % 30]: chosen by what the trade really keeps
    "news": (21, 22, 14, 13, 5), "ice": (23, 9, 29, 4, 14), "tobacco": (15, 14, 5, 15, 13),
    "flowers": (11, 10, 5, 11, 10), "cable": (5, 6, 13, 14, 29), "repair": (20, 19, 18, 20, 19),
}


def _label_tin(B, x, y, z, num, mat, h=0.12):
    """A tin with an atlas goods label on its front face (toward -y)."""
    box(B.detail, mat, x - 0.043, x + 0.043, y - 0.035, y + 0.035, z, z + h, faces="xXyYZ")
    kit.poster(B, x, y - 0.035, 0.0, 0.0, name="label_%02d" % num, w=0.075, zc=z + h / 2)


def _goods_stand(B, nums, mats, x0=1.15, x1=2.0, ky0=2.2):
    """A three-tier wooden display under the serving shelf, tins with labels of the trade's goods."""
    d = B.detail
    y0, y1 = ky0 - 0.30, ky0 - 0.02
    for xs in (x0, x1 - 0.03):
        box(d, "CT_Wood", xs, xs + 0.03, y0, y1, 0.0, 0.69, faces="xXyYZ")
    for zz in (0.04, 0.34, 0.64):
        box(d, "CT_WoodLight", x0, x1, y0, y1, zz, zz + 0.025, faces="xXyYZz")
    n = 7
    for tier, zz in enumerate((0.065, 0.365, 0.665)):
        for k in range(n):
            num = nums[(k + tier) % len(nums)] + (30 if (k + tier) % 2 else 0)
            _label_tin(B, x0 + 0.075 + k * 0.1117, y0 + 0.1, zz, num, mats[(k + tier) % len(mats)], h=0.12 if tier < 2 else 0.1)


def _switch_plate(B, x, ky0, z):
    d = B.detail
    box(d, "CT_TileWhite", x - 0.04, x + 0.04, ky0 - 0.014, ky0, z - 0.06, z + 0.06, faces="xXyYZz")
    box(d, "CT_Steel", x - 0.012, x + 0.012, ky0 - 0.026, ky0 - 0.014, z - 0.02, z + 0.035, faces="xXyYZz")


def _stamp_station(B, x, ky0, z):
    """A little steel shelf at the hatch's side with a red ink pad and a wooden stamp on a chain."""
    d = B.detail
    box(d, "CT_Steel", x - 0.08, x + 0.08, ky0 - 0.11, ky0, z, z + 0.012, faces="xXyYZz")
    box(d, "CT_Steel", x - 0.07, x - 0.065, ky0 - 0.1, ky0 - 0.01, z - 0.05, z, faces="xXyY")
    box(d, "CT_Steel", x + 0.065, x + 0.07, ky0 - 0.1, ky0 - 0.01, z - 0.05, z, faces="xXyY")
    box(d, "CT_PlasterRed", x - 0.065, x + 0.0, ky0 - 0.09, ky0 - 0.03, z + 0.012, z + 0.03, faces="xXyYZ")
    cyl(d, "CT_Wood", x + 0.04, ky0 - 0.055, z + 0.012, z + 0.085, 0.017, 6)
    box(d, "CT_Wood", x + 0.015, x + 0.065, ky0 - 0.08, ky0 - 0.03, z + 0.012, z + 0.03, faces="xXyYZ")
    fu.rod(d, "CT_Steel", (x + 0.04, ky0 - 0.055, z + 0.085), (x + 0.07, ky0 - 0.01, z + 0.16), 0.004, caps=False)


def _thermometer(B, x, y, yaw, z0=1.25):
    """[wall] A big street thermometer: white plate, red column, ticks, a bulb at the foot."""
    p = kit._P(B, x, y, 0.0, yaw, "thermometer")
    p.kind = "wall"
    p.box("CT_SteelWhite", -0.07, 0.07, -0.02, 0.0, z0, z0 + 0.62)
    p.box("CT_Glass", -0.012, 0.012, -0.03, -0.02, z0 + 0.1, z0 + 0.55)
    p.box("CT_SteelRed", -0.008, 0.008, -0.03, -0.021, z0 + 0.1, z0 + 0.38)
    p.cyl("CT_SteelRed", 0.0, -0.026, z0 + 0.05, z0 + 0.1, 0.026, 6)
    for k in range(8):
        p.box("CT_Steel", 0.02, 0.05 if k % 2 == 0 else 0.04, -0.024, -0.02, z0 + 0.12 + 0.055 * k, z0 + 0.128 + 0.055 * k, f="y")
    return p.done()


def _umbrella_stand(B, x, y):
    """A steel pot with three umbrellas awaiting repair (closed, tags tied to the handles)."""
    p = kit._P(B, x, y, 0.0, 0.0, "umbrella_stand")
    p.kind = "prop"
    p.cyl("CT_Steel", 0, 0, 0.0, 0.5, 0.14, 8, bottom=True)
    p.cyl("CT_ConcreteDark", 0, 0, 0.5, 0.505, 0.12, 8)
    for k, (dx, dy, mt, lean) in enumerate(((-0.05, 0.02, "CT_Lacquer", 0.04), (0.04, -0.03, "CT_PlasterBlue", -0.05),
                                           (0.0, 0.06, "CT_PlasterYellow", 0.02))):
        p.cyl(mt, dx, dy, 0.42, 1.0, 0.012, 6, r1=0.052)
        p.rod("CT_Steel", (dx, dy, 0.42), (dx + lean, dy, 1.22), 0.01, 4)
        p.box("CT_Wood", dx + lean - 0.045, dx + lean + 0.045, dy - 0.012, dy + 0.012, 1.22, 1.245)
        p.box("CT_Paper", dx - 0.02, dx + 0.02, dy - 0.014, dy - 0.01, 1.02 + 0.05 * k, 1.09 + 0.05 * k)
    return p.done()


def _third(B):
    """Third pass: the stop's clock, switch and stamp, the trade's goods stand, a seat of its own, the trade's boards."""
    d = B.detail
    kx0, kx1, ky0, ky1, kh = 0.6, 2.8, 2.2, 4.2, 2.45
    t = TRADE
    # ---- common to all six
    if t != "repair":
        kit.wall_clock(B, kx0, 3.2, 0.0, 270.0, zc=2.22, r=0.1, face="clock_%d" % (len(t) % 4))
        B.thing("stop_clock", "board", (kx0 - 0.08, 3.2, 2.22), 270.0, topic="clock")
    _switch_plate(B, 2.68, ky0, 1.33)
    B.thing("stop_switch", "machine", (2.68, ky0 - 0.04, 1.33), 0.0, what="switch", switch="room")
    _stamp_station(B, 2.68, ky0, 0.9)
    B.thing("stop_stamp", "machine", (2.68, ky0 - 0.06, 0.98), 0.0, what="stamp")
    mats = {"news": ["CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow"], "ice": ["CT_TileWhite", "CT_PlasterBlue", "CT_PlasterPink"],
            "tobacco": ["CT_PlasterGreen", "CT_TileWhite", "CT_PlasterRed"], "flowers": ["CT_PlasterPink", "CT_TileWhite", "CT_PlasterYellow"],
            "cable": ["CT_PlasterYellow", "CT_TileWhite", "CT_PlasterBlue"], "repair": ["CT_Steel", "CT_PlasterRed", "CT_Leather"]}[t]
    _goods_stand(B, GOODS_BY_TRADE[t], mats)
    B.spot("staff_seller", "work", (1.9, 3.25, 0.0), 0.0)
    # ---- the seat of its own, by trade
    if t == "news":
        _stand(B, -1.1, 1.3, "news_0", w=0.62, zc=1.45)
        B.thing("k1_wallpaper", "board", (-1.1, 1.26, 1.45), 0.0, topic="paper")
    elif t == "ice":
        B.thing("k2_crate_seat", "seat", (3.0, 2.6, 0.5), 0.0, height=0.5)
        B.spot("seat_k2", "seat", (3.0, 2.6, 0.0), 0.0)
        _thermometer(B, kx1, 2.55, 90.0)
        B.thing("k2_thermo", "board", (kx1 + 0.04, 2.55, 1.55), 90.0, topic="thermo")
    elif t == "tobacco":
        kit.chair(B, -2.6, 3.2, 0.0, 90.0, kind="wood")
        B.thing("k3_chair", "seat", (-2.6, 3.2, 0.45), 90.0, height=0.45, cone=90)
        B.spot("seat_k3", "seat", (-2.6, 3.2, 0.0), 90.0)
        B.thing("k3_lotto", "board", (0.78, ky0 - 0.03, 1.5), 0.0, topic="lotto")
    elif t == "flowers":
        kit.bench(B, -2.65, 3.05, 0.0, 90.0, w=0.8, kind="park")
        B.thing("k4_bench", "seat", (-2.65, 3.05, 0.45), 90.0, height=0.45)
        B.spot("seat_k4", "seat", (-2.65, 3.05, 0.0), 90.0)
        kit.poster(B, 0.23, 4.18, 0.0, 0.0, name="chart_calendar", w=0.3, zc=2.1)
        B.thing("k4_calendar", "board", (0.23, 4.14, 2.1), 0.0, topic="calendar")
        B.thing("k4_bucket_sniff", "machine", (0.28, 1.95, 0.5), 0.0, what="sniff")
    elif t == "cable":
        kit.chair(B, -2.6, 3.2, 0.0, 90.0, kind="tube")
        B.thing("k5_chair", "seat", (-2.6, 3.2, 0.45), 90.0, height=0.45, cone=90)
        B.spot("seat_k5", "seat", (-2.6, 3.2, 0.0), 90.0)
        B.thing("k5_stand", "board", (-0.1, 1.5, 0.85), 0.0, topic="departures")
    elif t == "repair":
        kit.stool(B, -2.55, 3.2, 0.0, 90.0, mat="CT_Leather")
        B.thing("k6_stool", "seat", (-2.55, 3.2, 0.45), 90.0, height=0.45)
        B.spot("seat_k6", "seat", (-2.55, 3.2, 0.0), 90.0)
        _umbrella_stand(B, 2.95, 2.6)
        B.thing("k6_umbrellas", "machine", (2.95, 2.6, 0.7), 0.0, what="umbrellas")
        B.thing("k6_keyboard", "board", (kx0 - 0.06, 3.87, 1.4), 270.0, topic="keys")
