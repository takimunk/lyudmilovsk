"""Stacked beam tower («Дом-мост»): late-Soviet brutalism, an original "space city" house.

Four raw-concrete bars (9 m wide, 26-30 m long) lie crosswise on top of each other: bar 1 along x (storeys 1-2),
bar 2 along y (3-4), bar 3 along x (5-6) and a lighter top bar along y: the public viewing gallery. Between the
bars are 3 m air voids; three vertical cores of different height (38 / 25 / 17 m) pierce them, two slender
ground piers and a few shear piers carry the rest. The ground floor is open (pilotis hall) with a glazed,
ENTERABLE lobby around the main core: reception counter, benches, wooden lift cladding, the street door.

Size 30 x 28 x 39.4 m (plot 40 x 40). Flats: 6 residential storeys, ~5 m modules on both long faces, each flat has
a living ribbon window and a small kitchen window (2 windows). Bar ends carry stair slits. Bar undersides are
closed soffits (dark concrete), the gallery has a timber soffit. Flat doors are on the corridor inside the bar
(not enterable tonight); everyone uses entrance "main".

Spots: lobby bench x2, lobby counter, "view" in the top gallery (z 32.4: reached by the lift).
Ideas for later: lit gallery interior, outdoor stair from the pilotis hall up to the bar 1 roof terrace, murals.
Material requests: none.
"""
import math
import random

from citykit import box, prism, ring, DOOR_W, DOOR_H, Facade, Mesh, wall_with_holes
import interior_kit as kit
import _homekit as hk

INFO = {
    "name_ru": "Дом-мост (балочная башня)", "name_en": "Stacked Beam Tower",
    "style": "brutalism", "use": "residential",
    "inspired_by": "1970s Soviet / Georgian 'space city' ideas (Tbilisi linear and bridge houses), Kenzo Tange's "
                   "Yamanashi cores, Moshe Safdie stacking; massing, proportions and details are our own",
}

T = 0.4            # slab thickness
WALL = 0.32
MOD = 5.0


def slab_box(m, x0, x1, y0, y1, z0, z1, top="CT_Concrete", bot="CT_ConcreteDark", side="CT_Concrete"):
    box(m, side, x0, x1, y0, y1, z0, z1, faces="xXyY")
    box(m, top, x0, x1, y0, y1, z0, z1, faces="Z")
    box(m, bot, x0, x1, y0, y1, z0, z1, faces="z")


def _u_range(F, rect):
    xs = (rect[0], rect[1])
    ys = (rect[2], rect[3])
    us = [(x - F.p0[0]) * F.d[0] + (y - F.p0[1]) * F.d[1] for x in xs for y in ys]
    return min(us), max(us)


def bar(B, rect, z0, storeys, axis, mat, first_storey, cores, cnt, soffit="CT_ConcreteDark", end_mat=None,
        top_mat="CT_Gravel", gallery=False, rnd=None):
    x0, x1, y0, y1 = rect
    H = storeys * 3.0 if not gallery else 4.0
    z1 = z0 + H
    slab_box(B.m, x0, x1, y0, y1, z0, z0 + T, top="CT_Floor" if gallery else "CT_Concrete", bot=soffit)
    slab_box(B.m, x0 - 0.0, x1, y0, y1, z1 - T, z1, top=top_mat, bot="CT_ConcreteDark")
    if storeys == 2:
        slab_box(B.m, x0, x1, y0, y1, z0 + 3.0 - T / 2, z0 + 3.0 + T / 2)
    edges = [((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)), ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0))]
    long_idx = (0, 2) if axis == "x" else (1, 3)
    wz0, wz1 = z0 + T, z1 - T
    # flat counters per storey
    for ei, (p0, p1) in enumerate(edges):
        L = math.hypot(p1[0] - p0[0], p1[1] - p0[1])
        F = Facade(p0, p1)
        holes, wins, fins = [], [], []
        if ei in long_idx and not gallery:
            n = int(L // MOD)
            margin = (L - n * MOD) / 2
            for k in range(n):
                ua = margin + k * MOD
                ub = ua + MOD
                blocked = any(_ov(_u_range(F, c), (ua, ub)) for c in cores)
                if not blocked:
                    for s in range(storeys):
                        zb = z0 + s * 3.0
                        h = (zb + 1.0, zb + 2.4)
                        hv = (zb + 0.9, zb + 2.4)
                        holes.append((ua + 0.55, ua + 3.55, h[0], h[1]))
                        holes.append((ua + 3.95, ua + 4.65, hv[0], hv[1]))
                        wins.append((k, s, (ua + 0.55, ua + 3.55, h[0], h[1]), (ua + 3.95, ua + 4.65, hv[0], hv[1]), ua, ub))
                else:
                    for s in range(storeys):
                        zb = z0 + s * 3.0
                        holes.append((ua + 2.2, ua + 2.8, zb + 0.8, zb + 2.4))
                        wins.append((None, s, (ua + 2.2, ua + 2.8, zb + 0.8, zb + 2.4), None, ua, ub))
                fins.append(ua)
            fins.append(margin + n * MOD)
        elif ei in long_idx and gallery:
            n = int(L // 3.0)
            m0 = (L - n * 3.0) / 2
            for k in range(n):
                ua = m0 + k * 3.0
                holes.append((ua + 0.4, ua + 2.6, z0 + 0.9, z1 - 0.9))
                wins.append((None, 0, (ua + 0.4, ua + 2.6, z0 + 0.9, z1 - 0.9), None, ua, ua + 3.0))
        else:
            # end wall: stair slits per storey, one big opening for the gallery
            if gallery:
                holes.append((1.0, L - 1.0, z0 + 0.9, z1 - 0.9))
                wins.append((None, 0, holes[-1], None, 0, L))
            else:
                for s in range(storeys):
                    zb = z0 + s * 3.0
                    for uc in (L * 0.3, L * 0.7):
                        holes.append((uc - 0.3, uc + 0.3, zb + 0.8, zb + 2.4))
                        wins.append((None, s, holes[-1], None, 0, 0))
        wmat = end_mat if (end_mat and ei not in long_idx) else mat
        F2 = B.facade(p0, p1, wz0, wz1, wmat, holes=holes, depth=WALL)
        for (k, s, h1, h2, ua, ub) in wins:
            if gallery or k is None:
                B.facade_window(F2, h1[0], h1[1], h1[2], h1[3], kind="public" if gallery else "stair", inset=0.18)
            else:
                storey = first_storey + s
                key = (storey, ei, k)
                if key not in cnt["ids"]:
                    cnt[storey] = cnt.get(storey, 0) + 1
                    fid = "%d-%02d" % (storey, cnt[storey])
                    cnt["ids"][key] = fid
                    mid = F2.to_building(((ua + ub) / 2, -(x1 - x0 if axis == "y" else y1 - y0) / 2, h1[2] - 0.9))
                    B.flat(fid, storey=storey, beds=rnd.choice((1, 2, 2, 3)), door=mid, entrance="main")
                fid = cnt["ids"][key]
                B.facade_window(F2, h1[0], h1[1], h1[2], h1[3], flat=fid, cols=3, rows=1, inset=0.18)
                B.facade_window(F2, h2[0], h2[1], h2[2], h2[3], flat=fid, inset=0.18)
        # fins and slab brows (outside relief)
        lm = Mesh()
        for ua in fins:
            box(lm, "CT_Concrete", ua - 0.13, ua + 0.13, -0.05, 0.5, wz0, wz1)
        if ei in long_idx and not gallery:
            for zz in ([z0 + 3.0] if storeys == 2 else []):
                box(lm, "CT_ConcreteDark", 0, L, -0.05, 0.22, zz - 0.22, zz + 0.22)
        B.m.extend(F2.apply(lm))
        lm2 = Mesh()
    return z1


def _ov(a, b):
    return a[0] < b[1] - 0.1 and a[1] > b[0] + 0.1


def _furnish_lobby(B, lx0, lx1, ly0, ly1, lh):
    """The main lobby as the hall of a big house (inner x -6.7..6.7, y 3.3..14.7, ceiling 4.2): reception desk with
    pigeonholes, mailboxes, house boards on the walls and the core faces, benches under the windows, a waiting corner,
    a chess table, a pram and a bike, plants, a radio. Six lamps as before (the building's only lobby lights)."""
    ix0, ix1, iy0, iy1 = lx0 + 0.3, lx1 - 0.3, ly0 + 0.3, ly1 - 0.3
    CORE = (-2.5, 2.5, 6.15, 11.5)

    def inside(x, y):
        return (ix0 < x < ix1 and iy0 < y < iy1 and not (CORE[0] - 0.02 < x < CORE[1] + 0.02 and CORE[2] - 0.02 < y < CORE[3] + 0.02))
    H = hk.Home(B, inside)
    f = H.f
    W = "#FFE0B8"
    # ---- entrance: runner and mat, benches under the front windows, boards and a clock between them
    f(kit.rug, 0.0, 4.9, 0.0, w=1.7, d=3.1, name="rug_0")
    f(kit.rug, 0.0, 3.75, 0.0, w=1.3, d=0.8, name="rug_5")
    bw = f(kit.bench, -5.0, 4.5, 0.0, w=2.0, kind="park", solid=True)
    be = f(kit.bench, 5.0, 4.5, 0.0, w=2.0, kind="park", solid=True)
    f(kit.plant, -3.0, 4.2, 0.0, kind="palm", pot="CT_PlasterRed", h=1.7, seed=11)
    f(kit.plant, 3.0, 3.9, 0.0, kind="ficus", pot="CT_TileWhite", h=1.6, seed=12)
    f(kit.picture, -6.68, 5.8, 90.0, name="painting_sea_storm", w=0.7, zc=1.8)
    # ---- the lift front (wood cladding on the core, y 6.15): sign, call button, clock
    f(kit.sign_plate, 0.0, 6.14, 0.0, name="sign_quiet", w=0.7, zc=2.9, hang=False)
    f(kit.wall_clock, 1.6, 6.14, 0.0, zc=2.6, face="clock_2")
    box(B.detail, "CT_Steel", 1.5, 1.6, 6.1, 6.15, 1.0, 1.3)
    box(B.detail, "CT_SteelRed", 1.53, 1.57, 6.07, 6.1, 1.1, 1.14)
    f(kit.fire_extinguisher, -2.25, 6.14, 0.0)
    # ---- west side: waiting corner, boards, benches, chess
    f(kit.rug, -4.4, 7.4, 0.0, w=2.6, d=2.0, name="rug_2")
    a1 = f(kit.armchair, -5.4, 7.4, 90.0, mat="CT_PlasterGreen")
    a2 = f(kit.armchair, -3.4, 7.4, 270.0, mat="CT_PlasterRed")
    f(kit.coffee_table, -4.4, 7.4, 90.0, w=0.8, d=0.5)
    f(kit.newspaper, -4.4, 7.3, 0.0, z=0.42, cellname="news_0")
    f(kit.cup, -4.4, 7.6, 0.0, z=0.42)
    f(kit.floor_lamp, -5.9, 6.4, 0.0)
    bn = f(kit.bench, -6.35, 9.0, 90.0, w=1.5, kind="park")
    f(kit.notice_board, -6.68, 9.0, 90.0, w=1.2, zc=1.75, seed=41)
    f(kit.wall_clock, -6.68, 9.0, 90.0, zc=2.75, face="clock_1")
    bnw = f(kit.bench, -6.35, 13.6, 90.0, w=1.4, kind="park")
    ct = f(kit.chess_table, -4.4, 12.2, 0.0)
    c1 = f(kit.chair, -4.4, 11.45, 180.0, kind="wood", mat="CT_PlasterBlue")
    c2 = f(kit.chair, -4.4, 12.95, 0.0, kind="wood", mat="CT_PlasterYellow")
    f(kit.plant, -3.2, 13.8, 0.0, kind="monstera", pot="CT_PlasterBlue", h=1.3, seed=13)
    f(kit.notice_board, -5.95, 14.68, 0.0, w=0.9, zc=1.7, seed=42)
    # ---- core faces: house rules west, a map and a bookshelf, the swap shelf north
    rules = f(kit.notice_board, -2.52, 9.0, 270.0, w=1.0, zc=1.6, seed=43)
    f(kit.sign_plate, -2.52, 9.0, 270.0, name="sign_quiet", w=0.5, zc=2.45)
    f(kit.wall_map, 2.52, 9.0, 90.0, w=1.5, h=1.1, zc=1.7, name="map_town")
    f(kit.poster, 2.52, 7.6, 90.0, name="travel_2", w=0.6, zc=1.7)
    f(kit.poster, 2.52, 10.4, 90.0, name="travel_4", w=0.6, zc=1.7)
    sh = f(kit.bookshelf, 0.0, 11.52, 180.0, w=1.2, h=1.8, levels=5, topic="local", seed=15)
    ph = f(kit.wall_phone, -1.5, 11.52, 180.0)
    # ---- reception: desk with the clerk's chair, pigeonholes behind, a till lamp and a radio
    rd = f(kit.reception_desk, 4.8, 12.6, 0.0, w=3.0, front="CT_SteelRed")
    ck = f(kit.chair, 4.8, 13.45, 0.0, kind="wood", mat="CT_PlasterRed")
    f(kit.telephone, 3.7, 12.8, 200.0, z=0.75)
    f(kit.radio, 5.9, 12.8, 180.0, z=0.75)
    f(kit.cup, 5.4, 12.65, 0.0, z=0.75)
    f(kit.table_lamp, 4.3, 12.8, 0.0, z=0.75, shade="CT_PlasterGreen")
    f(kit.lockers, 5.85, 14.68, 0.0, n=3, mat="CT_PlasterGreen", solid=False)
    # ---- east side: mailboxes and the board between the windows, prams and bikes, a visitors' table
    f(hk.mailboxes, 6.68, 9.0, 270.0, cols=6, rows=5)
    hk.pram(B, 6.0, 5.6, 80.0, mat="CT_PlasterBlue")
    hk.bike(B, 6.3, 7.0, 0.0)
    f(kit.hat_stand, 3.6, 5.4, 0.0)
    f(kit.plant, 6.0, 10.4, 0.0, kind="fern", pot="CT_PlasterYellow", h=1.0, seed=14)
    f(kit.plant, 3.4, 11.0, 0.0, kind="ficus", pot="CT_PlasterGreen", h=1.5, seed=16)
    f(kit.magazine_rack, 0.0, 14.68, 0.0, w=0.9)
    _deepen_lobby(B, H, f, rd, ck, ct, c2, sh)
    # ---- lights: the building's six lobby lamps (pendants with a real lamp)
    for x, y, lm in ((-4.2, 5.0, 800), (4.2, 5.0, 800), (0.0, 4.6, 800), (-4.4, 11.5, 800), (4.8, 10.0, 800), (0.0, 13.2, 800)):
        kit.pendant(B, x, y, lh, drop=0.3, light=True, lumens=lm, colour=W, radius=7.0, kind="cone")
    # ---- spots (ids kept) on the new seats and desk
    B.spot("lobby_bench_n", "bench", bn["seats"][0][:2] + (0.0,), bn["seats"][0][3])
    B.spot("lobby_bench_nw", "bench", bnw["seats"][0][:2] + (0.0,), bnw["seats"][0][3])
    B.spot("lobby_bench_w", "bench", bw["seats"][0][:2] + (0.0,), bw["seats"][0][3])
    B.spot("lobby_bench_e", "bench", be["seats"][0][:2] + (0.0,), be["seats"][0][3])
    B.spot("lobby_counter", "counter", rd["customer"][:2] + (0.0,), 180.0)
    # ---- things
    H.seat("lobby_bench_west", bw)
    H.seat("lobby_bench_east", be)
    H.seat("lobby_bench_wall", bn)
    H.seat("lobby_bench_corner", bnw)
    H.seat("lobby_armchair_green", a1)
    H.seat("lobby_armchair_red", a2)
    H.seat("lobby_chess_chair", c1)
    H.seat("lobby_clerk_chair", ck)
    B.thing("lobby_reception", "counter", rd["on"][:3], 180.0, role="concierge", service="reception")
    B.thing("lobby_chess", "play", ct["pos"][:2] + (0.78,), 0.0, what="chess")
    B.thing("lobby_board_lost", "board", (-6.68, 9.0, 1.75), 90.0, topic="lost")
    B.thing("lobby_board_club", "board", (-5.95, 14.65, 1.7), 0.0, topic="club")
    B.thing("lobby_board_rules", "board", (-2.5, 9.0, 1.6), 270.0, topic="rules")
    B.thing("lobby_board_map", "board", (2.5, 9.0, 1.7), 90.0, topic="view")
    B.thing("lobby_picture_storm", "board", (-6.66, 5.8, 1.8), 90.0, topic="painting")
    B.thing("lobby_swap_shelf", "shelf", sh["pos"][:2] + (1.3,), 180.0, topic="local")
    B.thing("lobby_magazines", "shelf", (0.0, 14.5, 1.0), 0.0, topic="travel")
    B.thing("lobby_payphone", "machine", ph["pos"][:2] + (1.4,), 180.0, what="payphone")
    B.thing("lobby_radio", "machine", (5.9, 12.95, 1.25), 180.0, what="jukebox")
    B.thing("lobby_paper", "take", (-4.4, 7.3, 0.45), 0.0, item="daily_paper")
    B.thing("lobby_lift", "door", (0.0, 6.14, 1.2), 0.0, to="roof")
    H.check("beam-lobby")


def _furnish_gallery(B, z=32.4):
    """The top gallery (inner x -4.18..4.18, y 3.32..29.68, floor 32.4, glazed on both long sides and at both ends):
    telescopes at the south end, wall benches facing the windows, pictures on the piers, a chess table, plants."""
    def inside(x, y):
        return -4.18 < x < 4.18 and 3.32 < y < 29.68
    H = hk.Home(B, inside)
    f = H.f
    # pictures and charts on the piers between the windows (piers y 5.6..6.4, 8.6..9.4 ... on both walls)
    piers = [6.0 + 3.0 * k for k in range(8)]
    cells = ["map_coast", "chart_birds", "painting_sea_calm", "map_town", "poster_birds_1", "painting_sea_night",
             "chart_calendar", "painting_lighthouse"]
    for k, py in enumerate(piers):
        f(kit.picture if cells[k].startswith("painting") else kit.poster, 4.17, py, 270.0, z=z, name=cells[k], w=0.55, zc=1.6)
        c2 = cells[(k + 3) % 8]
        f(kit.picture if c2.startswith("painting") else kit.poster, -4.17, py, 90.0, z=z, name=c2, w=0.55, zc=1.6)
    # benches facing the windows, pairs between plants
    benches = []
    for k, by in enumerate((8.0, 12.0, 16.0 + 0.0, 20.0, 24.0, 27.5)):
        for sx, yw in ((3.55, 90.0), (-3.55, 270.0)):
            if k == 2:
                continue
            b = f(kit.bench, sx, by, yw, z=z, w=2.0, kind="park", solid=True)
            benches.append(b)
    f(kit.plant, 3.35, 10.0, 0.0, z=z, kind="palm", pot="CT_PlasterRed", h=1.8, seed=21)
    f(kit.plant, -3.35, 10.0, 0.0, z=z, kind="palm", pot="CT_PlasterRed", h=1.8, seed=22)
    f(kit.plant, 3.55, 22.0, 0.0, z=z, kind="ficus", pot="CT_TileWhite", h=1.7, seed=23)
    f(kit.plant, -3.55, 22.0, 0.0, z=z, kind="ficus", pot="CT_TileWhite", h=1.7, seed=24)
    f(kit.plant, 3.2, 4.2, 0.0, z=z, kind="fern", pot="CT_PlasterYellow", h=1.0, seed=25)
    f(kit.plant, -3.2, 29.0, 0.0, z=z, kind="fern", pot="CT_PlasterYellow", h=1.0, seed=26)
    # the south end: two telescopes on the glass, a rug
    f(kit.rug, 0.0, 5.0, 0.0, z=z, w=2.6, d=2.2, name="rug_round_0", round=True)
    t1 = f(kit.telescope, -1.5, 4.5, 0.0, z=z)
    t2 = f(kit.telescope, 1.5, 4.5, 0.0, z=z)
    # the north end: binoculars on a stand, a reading table with newspapers
    t3 = f(kit.telescope, 0.0, 29.0, 180.0, z=z)
    f(kit.rug, 0.0, 26.0, 0.0, z=z, w=2.4, d=2.4, name="rug_round_1", round=True)
    ct = f(kit.chess_table, 0.0, 26.0, 0.0, z=z)
    c1 = f(kit.chair, 0.0, 25.25, 180.0, z=z, kind="wood", mat="CT_PlasterBlue")
    c2 = f(kit.chair, 0.0, 26.75, 0.0, z=z, kind="wood", mat="CT_PlasterRed")
    # the middle: a long table of guide books with a lamp, near the view spot but clear of it
    for y in (6.0, 11.0, 16.0, 21.0, 26.0, 29.0):
        kit.pendant(B, 0.0, y, z + 3.2, drop=0.4, light=True, lumens=700, colour="#FFE8C8", radius=8.0, kind="opal")
    H.seat("gallery_bench_e1", benches[0])
    H.seat("gallery_bench_w1", benches[1])
    H.seat("gallery_bench_e2", benches[2])
    H.seat("gallery_bench_w2", benches[3])
    H.seat("gallery_bench_e3", benches[6])
    H.seat("gallery_bench_w3", benches[7])
    H.seat("gallery_chess_chair", c1)
    B.thing("gallery_telescope_1", "machine", t1["pos"][:2] + (z + 1.2,), 0.0, what="telescope")
    B.thing("gallery_telescope_2", "machine", t2["pos"][:2] + (z + 1.2,), 0.0, what="telescope")
    B.thing("gallery_binoculars", "machine", t3["pos"][:2] + (z + 1.2,), 0.0, what="binoculars")
    B.thing("gallery_chess", "play", ct["pos"][:2] + (z + 0.78,), 0.0, what="chess")
    B.thing("gallery_board_coast", "board", (4.17, 6.0, z + 1.6), 270.0, topic="view")
    B.thing("gallery_board_birds", "board", (4.17, 9.0, z + 1.6), 270.0, topic="view")
    B.thing("gallery_board_town", "board", (-4.17, 15.0, z + 1.6), 90.0, topic="view")
    B.thing("gallery_picture_sea", "board", (4.17, 12.0, z + 1.6), 270.0, topic="painting")
    B.thing("gallery_picture_night", "board", (-4.17, 21.0, z + 1.6), 90.0, topic="painting")
    B.thing("gallery_picture_lighthouse", "board", (4.17, 27.0, z + 1.6), 270.0, topic="painting")
    _deepen_gallery(B, H, f, z, ct, c2)
    H.check("beam-gallery")


# ====================================================================================== round 2: depth pass
def _lift_leaf(B, cx, cy, z, yaw):
    """A lift door set in a wall plane through (cx, cy): steel frame, two leaves, a call button, a floor lamp above.
    yaw 0: the wall's face looks to -y (the viewer stands at smaller y); yaw 180: it looks to +y. Drawing only."""
    def bx(u0, u1, v0, v1, z0, z1, mat):
        if yaw == 0:
            box(B.detail, mat, cx + u0, cx + u1, cy - v1, cy - v0, z + z0, z + z1)
        else:
            box(B.detail, mat, cx - u1, cx - u0, cy + v0, cy + v1, z + z0, z + z1)
    bx(-0.58, 0.58, 0.0, 0.05, 0.0, 2.2, "CT_Steel")
    bx(-0.48, -0.005, 0.05, 0.075, 0.0, 2.1, "CT_SteelWhite")
    bx(0.005, 0.48, 0.05, 0.075, 0.0, 2.1, "CT_SteelWhite")
    bx(-0.04, 0.04, 0.075, 0.09, 0.0, 2.1, "CT_Steel")
    bx(0.66, 0.74, 0.0, 0.03, 1.0, 1.2, "CT_ConcreteWhite")
    bx(0.685, 0.715, 0.03, 0.045, 1.07, 1.13, "CT_SteelRed")
    bx(-0.12, 0.12, 0.0, 0.03, 2.28, 2.38, "CT_Opal")


def _umbrella_stand(B, x, y, z=0.0):
    p = kit._P(B, x, y, z, 0.0, "umbrella_stand")
    p.kind = "prop"
    p.cyl("CT_SteelRed", 0, 0, 0.0, 0.5, 0.17, 8, r1=0.2)
    for k, (a, b, c) in enumerate(((0.05, 0.02, 0.15), (-0.06, 0.0, -0.1), (0.0, -0.06, 0.05))):
        p.rod("CT_ConcreteDark" if k != 1 else "CT_PlasterBlue", (0.0, 0.0, 0.3), (a, b, 0.95 + c * 0.3), 0.03, 4)
    p.rod("CT_Wood", (0.05, 0.02, 0.95), (0.05, 0.02, 1.0), 0.045, 5)
    return p.done()


def _wind_gauge(B, x, y, z, yaw):
    """[wall] A brass-ringed wind dial (the cup anemometer is on the roof; the dial comes down the core): a back plate,
    a clock-like face, a thin pointer and a little brass plaque. Faces toward -y at yaw 0."""
    p = kit._P(B, x, y, z, yaw, "wind_gauge")
    p.kind = "wall"
    p.box("CT_Wood", -0.3, 0.3, -0.04, 0.0, 1.18, 1.82, f="xXyZz")
    r = 0.2
    rg0 = [(r * math.cos(2 * math.pi * k / 14), -0.04, 1.5 + r * math.sin(2 * math.pi * k / 14)) for k in range(14)]
    rg1 = [(a, -0.075, c) for a, _, c in rg0]
    for a, b, c_, d_ in zip(rg0, rg0[1:] + rg0[:1], rg1[1:] + rg1[:1], rg1):
        p.face("CT_Brass", [a, b, c_, d_])
    s_ = r * 0.88
    p.card_front("clock_3", -s_, s_, 1.5 - s_, 1.5 + s_, -0.078)
    p.rod("CT_SteelRed", (0.0, -0.085, 1.5), (0.1, -0.085, 1.62), 0.012, 4)
    p.box("CT_Brass", -0.1, 0.1, -0.052, -0.04, 1.2, 1.26, f="xXyZz")
    return p.done()


def _lectern(B, x, y, z, yaw):
    """A visitors' book on a slim lectern: base, column, a sloping top with an open book. Reader at -y at yaw 0."""
    p = kit._P(B, x, y, z, yaw, "lectern")
    p.kind = "floor"
    p.box("CT_WoodLight", -0.22, 0.22, -0.2, 0.2, 0.0, 0.04)
    p.box("CT_Wood", -0.07, 0.07, -0.07, 0.07, 0.04, 0.98)
    ring_lo = [(-0.3, -0.24, 1.0), (0.3, -0.24, 1.0), (0.3, 0.24, 1.16), (-0.3, 0.24, 1.16)]
    ring_hi = [(a, b, c + 0.035) for a, b, c in ring_lo]
    p.loft("CT_Wood", [ring_lo, ring_hi], top=True, bottom=True)
    p.card_tilt("paper_00" if "paper_00" in kit.ATLAS else "news_0", -0.22, -0.005, -0.2, 1.045, 0.2, 1.19)
    p.card_tilt("paper_01" if "paper_01" in kit.ATLAS else "news_0", 0.005, 0.22, -0.2, 1.045, 0.2, 1.19)
    p.core(-0.07, 0.07, -0.07, 0.07, 0.04, 0.98)
    return p.done(solid=False, top=1.1, footprint=(-0.3, 0.3, -0.25, 0.25))


def _deepen_lobby(B, H, f, rd, ck, ct, c2, sh):
    """Round 2, the lobby lived in: two lift doors on the wooden front (the old caption thing lobby_lift is the left-hand
    door of the pair, lift_g is the second car), the lamp switch by the street door, the house board with the running
    story, the post, pram, bicycle, hats and plants as things, the desk's clerk and guard, the second chess chair."""
    _lift_leaf(B, 0.0, 6.15, 0.0, 0)
    _lift_leaf(B, -1.35, 6.15, 0.0, 0)
    B.thing("lift_g", "door", (-1.35, 6.11, 1.1), 0.0, to="lift")
    _umbrella_stand(B, 1.3, 3.55)
    box(B.detail, "CT_SteelWhite", -1.04, -0.96, 3.30, 3.33, 1.22, 1.38)
    B.thing("lobby_switch", "machine", (-1.0, 3.34, 1.3), 180.0, what="switch", switch="hall")
    f(kit.notice_board, 1.7, 11.52, 180.0, w=0.8, zc=1.6, seed=44)
    B.thing("lobby_board_story", "board", (1.7, 11.5, 1.6), 180.0, topic="house")
    B.thing("lobby_mail", "board", (6.66, 9.0, 1.2), 270.0, topic="mail")
    B.thing("lobby_pram", "board", (6.0, 5.6, 0.6), 260.0, topic="pram")
    B.thing("lobby_bike", "board", (6.3, 7.0, 0.8), 0.0, topic="bike")
    B.thing("lobby_hats", "board", (3.6, 5.4, 1.2), 0.0, topic="hats")
    B.thing("lobby_plants", "board", (3.0, 3.9, 0.9), 0.0, topic="plants")
    on = rd["on"]
    B.thing("lobby_desk_clerk", "counter", (on[0] - 0.9, on[1], on[2]), 180.0, role="clerk", service="reception")
    B.thing("lobby_desk_guard", "counter", (on[0] + 0.9, on[1], on[2]), 180.0, role="guard", service="reception")
    H.seat("lobby_chess_chair2", c2)
    B.spot("lobby_clerk_post", "work", (ck["seats"][0][0], ck["seats"][0][1], 0.0), 0.0)
    B.spot("lobby_chess_n", "seat", c2["seats"][0][:2] + (0.0,), c2["seats"][0][3])
    B.spot("lobby_mail_stand", "stand", (5.9, 9.0, 0.0), 90.0)
    B.spot("lobby_board_stand", "stand", (1.7, 10.7, 0.0), 0.0)
    B.spot("lobby_plants_stand", "stand", (3.0, 4.7, 0.0), 180.0)


def _deepen_gallery(B, H, f, z, ct, c2):
    """Round 2, the gallery as the place people come up for: the lift door on the core, light switches, the clock and the
    wind gauge on the north piers, the visitors' book on a lectern, a winter garden bed and a tea corner on the long
    walls' empty bay, a map of what one sees, the opponent's chair, the window with a view, the spots."""
    _lift_leaf(B, 0.0, 6.5, z, 0)
    B.thing("lift_gallery", "door", (0.0, 6.46, z + 1.1), 0.0, to="lift")
    # switches on the south and north piers
    for tid, sx, sy, yw in (("gallery_switch_s", 3.85, 3.34, 180.0), ("gallery_switch_n", -3.85, 29.64, 0.0)):
        box(B.detail, "CT_SteelWhite", sx - 0.04, sx + 0.04, min(sy, sy + (0.03 if yw == 180.0 else -0.03)),
            max(sy, sy + (0.03 if yw == 180.0 else -0.03)), z + 1.22, z + 1.38)
        B.thing(tid, "machine", (sx, sy + (0.04 if yw == 180.0 else -0.04), z + 1.3), yw, what="switch", switch=15)
    f(kit.wall_clock, -3.85, 29.64, 0.0, z=z, zc=2.35, face="clock_1")
    _wind_gauge(B, 3.85, 29.66, z, 0.0)
    B.thing("gallery_wind_gauge", "board", (3.85, 29.58, z + 1.5), 0.0, topic="wind")
    # the visitors' book
    lc = _lectern(B, 2.85, 5.7, z, 270.0)
    B.thing("gallery_guestbook", "shelf", (2.85, 5.7, z + 1.12), 270.0, topic="guestbook")
    # the winter garden: a long bed on the east wall at the gap of the benches, a tea corner opposite
    f(kit.planter, 3.8, 16.0, 90.0, z=z, w=2.8)
    f(kit.palm, 3.5, 14.0, 0.0, z=z, h=2.1)
    f(kit.plant, 3.5, 18.2, 0.0, z=z, kind="monstera", pot="CT_PlasterGreen", h=1.1, seed=27)
    f(kit.plant, 3.15, 16.9, 0.0, z=z, kind="cactus", pot="CT_PlasterYellow", h=0.7, seed=28)
    B.thing("gallery_garden", "board", (3.62, 16.0, z + 0.8), 270.0, topic="garden")
    f(kit.rug, -3.1, 16.0, 0.0, z=z, w=1.8, d=1.8, name="rug_6")
    tt = f(kit.table_round, -3.2, 16.0, 0.0, z=z, r=0.4, h=0.75)
    sm = kit.samovar(B, -3.2, 16.0, tt["top"], 0.0)
    kit.cup(B, -3.45, 15.85, tt["top"], 0.0)
    kit.cup(B, -2.95, 16.2, tt["top"], 0.0)
    ta = f(kit.chair, -3.2, 15.2, 180.0, z=z, kind="wood", mat="CT_PlasterRed")
    tb = f(kit.chair, -3.2, 16.8, 0.0, z=z, kind="wood", mat="CT_PlasterGreen")
    B.thing("gallery_tea", "machine", (-3.2, 16.0, tt["top"] + 0.2), 90.0, what="samovar")
    H.seat("gallery_tea_chair_s", ta)
    H.seat("gallery_tea_chair_n", tb)
    # the map of what one sees on the core's east face
    f(kit.wall_map, 2.52, 9.4, 90.0, z=z, w=1.5, h=1.1, zc=1.7, name="map_coast")
    B.thing("gallery_map_view", "board", (2.5, 9.4, z + 1.7), 90.0, topic="view")
    # the window at the south end: a thing to look out of
    B.thing("gallery_window_sea", "board", (0.0, 3.36, z + 1.6), 180.0, topic="view")
    # the opponent's chair and spots for the visitors
    H.seat("gallery_chess_chair2", c2)
    B.spot("gallery_chess_n", "seat", c2["seats"][0][:2] + (z,), c2["seats"][0][3])
    for sid, sx, sy, yw in (("gallery_scope_a", -1.5, 5.4, 0.0), ("gallery_scope_b", 1.5, 5.4, 0.0),
                            ("gallery_glasses", 0.0, 28.2, 180.0), ("gallery_book_stand", 2.2, 5.7, 90.0),
                            ("gallery_garden_stand", 2.9, 16.0, 90.0), ("gallery_gauge_stand", 3.85, 28.8, 180.0),
                            ("gallery_window_stand", 0.0, 4.2, 0.0)):
        B.spot(sid, "view" if "stand" not in sid else "stand", (sx, sy, z), yw)
    B.spot("gallery_tea_s", "seat", (-3.2, 15.2, z), 180.0)
    B.spot("gallery_tea_n", "seat", (-3.2, 16.8, z), 0.0)


def _flat_wing(B, Z):
    """Round 2: the lift stop of the fourth storey (bar 2, floor z=Z) on the core's north face, a 1.1 m corridor east of
    the bar's axis, and flat 4-03, the home of Rohan Hargreaves, whose door stands open. Shell only: inner walls, floor,
    ceiling; windows, flats and entrances of the export are untouched."""
    zt = Z + 2.4
    # corridor walls
    box(B.m, "CT_Interior", -0.65, -0.55, 11.5, 29.68, Z, zt, faces="xX")
    box(B.m, "CT_Interior", 0.55, 0.65, 11.5, 24.0, Z, zt, faces="xX")
    box(B.m, "CT_Interior", 0.55, 0.65, 24.95, 29.68, Z, zt, faces="xX")
    box(B.m, "CT_Interior", 0.55, 0.65, 24.0, 24.95, Z + 2.05, zt, faces="xX")
    # the flat's walls: south and north partitions between the corridor wall and the bar's east wall
    box(B.m, "CT_Interior", 0.65, 4.18, 23.5, 23.6, Z, zt, faces="yY")
    box(B.m, "CT_Interior", 0.65, 4.18, 28.4, 28.5, Z, zt, faces="yY")
    # floor overlays and ceiling
    box(B.m, "CT_Floor", -0.55, 0.55, 11.5, 29.68, Z, Z + 0.005, faces="Z")
    box(B.m, "CT_Wood", 0.65, 4.18, 23.6, 28.4, Z, Z + 0.006, faces="Z")
    for x0, x1, y0, y1 in ((-0.55, 0.55, 11.5, 29.68), (0.65, 4.18, 23.6, 28.4)):
        B.m.face("CT_Interior", [(x0, y0, zt - 0.001), (x1, y0, zt - 0.001), (x1, y1, zt - 0.001), (x0, y1, zt - 0.001)][::-1])
    # the door frame (the door itself stands open against the corridor wall)
    box(B.detail, "CT_Wood", 0.5, 0.7, 23.94, 24.0, Z, Z + 2.07)
    box(B.detail, "CT_Wood", 0.5, 0.7, 24.95, 25.01, Z, Z + 2.07)
    box(B.detail, "CT_Wood", 0.5, 0.7, 23.94, 25.01, Z + 2.07, Z + 2.13)
    box(B.detail, "CT_Wood", 0.5, 0.52, 25.01, 25.9, Z, Z + 2.0)         # the leaf, swung open along the corridor wall
    box(B.detail, "CT_Brass", 0.495, 0.5, 25.6, 25.64, Z + 1.0, Z + 1.04)
    # the lift stop on the core's north face
    _lift_leaf(B, 0.0, 11.5, Z, 180)
    B.thing("lift_s4", "door", (0.0, 11.54, Z + 1.1), 180.0, to="lift")
    box(B.detail, "CT_Steel", -0.56, -0.5, 11.5, 11.56, Z + 2.4, Z + 2.5)
    # corridor: numbers, a notice and a bench-less coat hook
    kit.picture(B, -0.54, 18.0, Z, 270.0 - 180.0, name="painting_lighthouse", w=0.7, zc=1.6)
    kit.poster(B, -0.54, 14.5, Z, 270.0 - 180.0, name="chart_birds", w=0.55, zc=1.6)
    kit.fire_extinguisher(B, -0.54, 22.0, Z, 270.0 - 180.0)
    kit.plant(B, -0.1, 28.9, Z, 0.0, kind="fern", pot="CT_PlasterYellow", h=0.9, seed=29)
    for ly, lr in ((16.5, 8.0), (26.5, 8.0)):
        B.lamp((0.0, ly, zt - 0.3), lumens=520, colour="#FFE0B8", kind="public", radius=lr)
    _rohan_flat(B, Z)


def _rohan_flat(B, Z):
    """Flat 4-03 (x 0.65..4.18, y 23.6..28.4; the ribbon window and the kitchen window on the east wall): the home of
    Rohan Hargreaves, 78, who spent thirty-one years laying roads on slopes for the Ministry of Highways. A drafting table
    with the road he never built, a wind log, an armchair at the window, a shelf of engineering books, letters, a kettle."""
    f = lambda fn, x, y, yaw=0.0, **kw: fn(B, x, y, Z, yaw, **kw)
    # the window side: armchair looking out, a floor lamp, a low table, a globe
    f(kit.rug, 2.5, 25.8, 0.0, w=2.2, d=2.6, name="rug_3")
    ar = f(kit.armchair, 3.55, 25.2, 90.0, mat="CT_PlasterGreen")
    f(kit.floor_lamp, 3.75, 24.3, 0.0)
    ctb = f(kit.coffee_table, 3.5, 26.2, 90.0)
    kit.cup(B, 3.45, 26.1, ctb["top"], 0.0)
    kit.books_stack(B, 3.55, 26.45, ctb["top"], 0.0, n=2, seed=6)
    gl = kit.globe(B, 3.75, 27.0, Z, 0.0, floor=True)
    # the west wall: the drafting table with a stool, the map above it, the bed under a painting
    dt = f(kit.drafting_table, 1.3, 25.7, 90.0)
    f(kit.stool, 2.15, 25.7, 90.0)
    kit.lamp_green(B, 1.15, 25.2, Z + 0.92, 0.0)
    kit.books_stack(B, 1.2, 26.25, Z + 0.92, 0.0, n=2, seed=7)
    f(kit.wall_map, 0.66, 25.7, 90.0, w=1.3, h=0.9, zc=1.8, name="map_town")
    f(kit.bed, 1.22, 27.35, 0.0, size="single", blanket="CT_PlasterBlue")
    bt = f(kit.bedside_table, 1.95, 28.1, 0.0, lamp=False)
    kit.radio(B, 1.95, 28.15, bt["top"], 0.0)
    f(kit.picture, 0.66, 27.4, 90.0, name="painting_sea_calm", w=0.8, zc=1.75)
    # the south wall: shoes and coats by the door, a wardrobe, the bookshelf of engineering
    f(kit.hat_stand, 1.1, 23.95, 0.0)
    f(kit.shoe_rack, 1.9, 23.63, 180.0, w=0.7)
    f(kit.wardrobe, 2.7, 23.65, 180.0, w=1.0)
    bs = f(kit.bookshelf, 3.55, 23.62, 180.0, w=1.1, h=1.9, levels=5, topic="engineering", seed=61)
    f(kit.picture, 2.2, 23.6, 180.0, name="painting_dunes", w=0.5, zc=1.7)
    # the north wall: a kitchenette under the small window
    ku = f(kit.kitchen_unit, 3.25, 28.1, 0.0, modules="cupboard,stove,sink", wall_cupboards=False)
    kit.teapot(B, 2.7, 28.1, Z + 0.9, 0.0)
    kit.plates(B, 3.7, 28.1, Z + 0.9, 0.0, n=2)
    f(kit.wall_clock, 2.2, 28.4, 0.0, zc=2.0, face="clock_2")
    # light: a lamp in the room
    B.lamp((2.4, 26.0, Z + 2.1), lumens=480, colour="#FFD9A8", kind="lobby", radius=6.0)
    # things
    B.thing("hg_armchair", "seat", (3.55, 25.2, Z + 0.45), 90.0, height=0.45)
    B.thing("hg_diary", "shelf", (1.25, 25.4, Z + 0.95), 90.0, topic="diary")
    B.thing("hg_log", "shelf", (1.2, 26.25, Z + 0.95), 90.0, topic="windlog")
    B.thing("hg_letters", "shelf", (3.5, 26.3, Z + 0.5), 270.0, topic="letters")
    B.thing("hg_roadmap", "board", (0.66, 25.7, Z + 1.8), 90.0, topic="roads")
    B.thing("hg_photo", "board", (0.66, 27.4, Z + 1.75), 90.0, topic="photos")
    B.thing("hg_globe", "board", (3.75, 27.0, Z + 1.0), 0.0, topic="globe")
    B.thing("hg_books", "shelf", (3.55, 23.6, Z + 1.3), 180.0, topic="engineering")
    B.thing("hg_kettle", "machine", (3.25, 28.1, Z + 1.0), 0.0, what="stove")
    B.thing("hg_radio", "machine", (1.95, 28.15, Z + 0.7), 0.0, what="radio")
    B.thing("hg_bed", "bed", (1.22, 27.35, Z + 0.5), 0.0, rest="private", height=0.5)
    B.thing("hg_door", "door", (0.62, 24.5, Z + 1.1), 90.0, to="flat")
    B.thing("hg_switch", "machine", (0.62, 24.1, Z + 1.3), 90.0, what="switch", switch="room")
    B.spot("hargreaves_chair", "seat", (3.55, 25.2, Z), 90.0)
    B.spot("hargreaves_desk", "seat", (2.15, 25.7, Z), 270.0)
    B.spot("hargreaves_kitchen", "stand", (3.25, 27.4, Z), 180.0)
    B.spot("hargreaves_window", "view", (3.3, 26.7, Z), 90.0)


def build(B):
    rnd = random.Random(1974)
    cnt = {"ids": {}}
    # cores: (name, cx, cy, top)
    cores = [("A", 0.0, 9.0, 37.2), ("B", -9.0, 23.0, 17.2), ("C", 9.0, 23.0, 24.6)]
    R = 2.5
    # bars: rect, z0
    b1 = (-15.0, 15.0, 18.5, 27.5)
    b2 = (-4.5, 4.5, 2.0, 30.0)
    b3 = (-14.0, 14.0, 4.5, 13.5)
    b4 = (-4.5, 4.5, 3.0, 30.0)
    z1b, z2b, z3b, z4b = 5.0, 14.0, 23.0, 32.0
    solids = [(b1, z1b, z1b + 6), (b2, z2b, z2b + 6), (b3, z3b, z3b + 6), (b4, z4b, z4b + 4)]
    core_rects = {n: (cx - R, cx + R, cy - R, cy + R) for n, cx, cy, t in cores}
    bar(B, b1, z1b, 2, "x", "CT_Concrete", 1, [core_rects["B"], core_rects["C"]], cnt, end_mat="CT_PlasterRed", rnd=rnd)
    bar(B, b2, z2b, 2, "y", "CT_Concrete", 3, [core_rects["A"]], cnt, rnd=rnd)
    bar(B, b3, z3b, 2, "x", "CT_Concrete", 5, [core_rects["A"]], cnt, rnd=rnd)
    bar(B, b4, z4b, 1, "y", "CT_ConcreteWhite", 0, [], cnt, soffit="CT_Wood", gallery=True, top_mat="CT_Roof", rnd=rnd)
    # piers
    slab_box(B.m, -2.8, 2.8, 20.5, 25.5, z1b + 6, z2b, top="CT_Concrete", bot="CT_ConcreteDark")
    for sx in (-11.0, 11.0):
        slab_box(B.m, sx - 1.2, sx + 1.2, 7.0, 11.0, 0.0, z3b, top="CT_Concrete", bot="CT_ConcreteDark")
    slab_box(B.m, -0.9, 0.9, 22.0, 27.0, z2b + 6, z4b, top="CT_Concrete", bot="CT_ConcreteDark")
    # cores with stair slits
    for n, cx, cy, top in cores:
        box(B.m, "CT_Concrete", cx - R, cx + R, cy - R, cy + R, 0.0, top, faces="xXyYz")
        box(B.m, "CT_ConcreteDark", cx - R - 0.3, cx + R + 0.3, cy - R - 0.3, cy + R + 0.3, top, top + 0.4)
        faces = [("y", cx, cy - R), ("Y", cx, cy + R), ("x", cx - R, cy), ("X", cx + R, cy)]
        for side, fx, fy in faces:
            z = 3.0 if n == "A" else 7.0 if n == "B" else 4.0
            zs = []
            zz = 6.0
            while zz + 1.6 < top - 0.8:
                zs.append(zz)
                zz += 3.0
            for zz in zs:
                px, py = (fx, fy)
                if side in "yY":
                    pts = [(px, py + (-0.05 if side == "y" else 0.05))]
                inside = any(r[0] - 0.1 < px < r[1] + 0.1 and r[2] - 0.1 < py < r[3] + 0.1 and za - 0.3 < zz + 0.8 < zb + 0.3
                             for r, za, zb in solids if not (n == "A" and False))
                # a slit's centre point is outside a bar if the slit is in a void
                inside = any((r[0] - 0.4 < px < r[1] + 0.4 and r[2] - 0.4 < py < r[3] + 0.4) and (za - 0.6 < zz + 0.8 and zz < zb + 0.6)
                             for r, za, zb in solids)
                if inside or (n == "A" and zz < 5.0):
                    continue
                w = 0.28
                c0, c1 = zz, zz + 1.6
                if side == "y":
                    q = [(cx - w, fy, c0), (cx + w, fy, c0), (cx + w, fy, c1), (cx - w, fy, c1)]
                elif side == "Y":
                    q = [(cx + w, fy, c0), (cx - w, fy, c0), (cx - w, fy, c1), (cx + w, fy, c1)]
                elif side == "x":
                    q = [(fx, cy + w, c0), (fx, cy - w, c0), (fx, cy - w, c1), (fx, cy + w, c1)]
                else:
                    q = [(fx, cy - w, c0), (fx, cy + w, c0), (fx, cy + w, c1), (fx, cy - w, c1)]
                B.window(q, kind="stair", inset=0.04, frame=0.04)
    # roof machine room on the main core, tank on core C
    box(B.m, "CT_ConcreteDark", -2.0, 2.0, 7.2, 10.8, 37.6, 39.4)
    box(B.m, "CT_Steel", -1.0, 1.0, 7.8, 10.2, 39.4, 39.6)
    box(B.m, "CT_ConcreteDark", 7.5, 10.5, 21.5, 24.5, 25.0, 26.2)
    # ----------------------------------------------------------------- the lobby (enterable) around core A
    lx0, lx1, ly0, ly1, lh = -7.0, 7.0, 3.0, 15.0, 4.2
    box(B.m, "CT_Floor", lx0, lx1, ly0, ly1, -0.3, 0.0, faces="Z")
    box(B.m, "CT_ConcreteDark", lx0 - 0.3, lx1 + 0.3, ly0 - 0.3, ly1 + 0.3, -0.3, 0.0, faces="xXyYz")
    # roof slab with interior ceiling
    box(B.m, "CT_Concrete", lx0 - 0.4, lx1 + 0.4, ly0 - 1.0, ly1 + 0.4, lh, lh + 0.4, faces="xXyYZ")
    box(B.m, "CT_ConcreteDark", lx0 - 0.4, lx1 + 0.4, ly0 - 1.0, ly1 + 0.4, lh, lh + 0.4, faces="z")
    # ceiling inside (just below the slab)
    B.m.face("CT_Interior", [(lx0, ly0, lh - 0.001), (lx1, ly0, lh - 0.001), (lx1, ly1, lh - 0.001), (lx0, ly1, lh - 0.001)][::-1])
    lob = [((lx0, ly0), (lx1, ly0)), ((lx1, ly0), (lx1, ly1)), ((lx1, ly1), (lx0, ly1)), ((lx0, ly1), (lx0, ly0))]
    lobholes = [
        [(0.6, 5.6, 0.35, 3.7), (7 - DOOR_W, 7 + DOOR_W, 0.0, 2.45), (8.4, 13.4, 0.35, 3.7)],
        [(1.5, 5.0, 0.5, 3.6), (7.0, 10.5, 0.5, 3.6)],
        [(2.0, 6.0, 0.6, 3.4), (8.0, 12.0, 0.6, 3.4)],
        [(1.5, 5.0, 0.5, 3.6), (7.0, 10.5, 0.5, 3.6)],
    ]
    lobwin = [[True, False, True], [True, True], [True, True], [True, True]]
    # holes list is in walking order (u); windows for each hole flagged
    for ei, (p0, p1) in enumerate(lob):
        F = B.facade(p0, p1, 0.0, lh, "CT_Concrete", holes=lobholes[ei], depth=0.3, back=False)
        L = F.length
        for hi, h in enumerate(lobholes[ei]):
            if lobwin[ei][hi]:
                B.facade_window(F, h[0], h[1], h[2], h[3], kind="public", cols=3 if ei == 0 else 2, rows=1, inset=0.15)
        # inner skin facing the room: walk the reversed edge (p1 -> p0) so the right side is the room
        q0 = _toward(p0, p1, 0.3, lob, ei)
        q1 = _toward(p1, p0, 0.3, lob, ei, rev=True)
        Fi = Facade(q1, q0)
        mi = Mesh()
        ih = [(L - h[1] + 0.0, L - h[0] + 0.0, h[2], h[3]) for h in lobholes[ei]]
        # inner skin length differs slightly (corner overlap): same inner positions, so rebuild holes in its own u
        Li = Fi.length
        ih = [(Li - (h[1] - 0.3), Li - (h[0] - 0.3), h[2], h[3]) for h in lobholes[ei]]
        ih = [(max(0.0, a), min(Li, b), c, d) for a, b, c, d in ih]
        wall_with_holes(mi, "CT_Interior", Li, 0.0, lh, ih)
        B.m.extend(Fi.apply(mi))
    B.entrance("main", (0.0, ly0, 0.0), 0.0, width=DOOR_W * 2, leaves=2, kind="street")
    # lobby furniture: wood-clad lift front on the core, counter
    box(B.m, "CT_Wood", -2.5, 2.5, 6.15, 6.5, 0.0, 3.2, faces="xXyYZ")
    _furnish_lobby(B, lx0, lx1, ly0, ly1, lh)       # (reception desk, benches and spots now come from the kit)
    _furnish_gallery(B, z4b + 0.4)
    _flat_wing(B, z2b + 3.2)
    # canopy over the street door
    box(B.m, "CT_Wood", -3.0, 3.0, 0.0, 3.0, 3.35, 3.6, faces="xXyYZz")
    B.spot("view", "view", (0.0, 16.0, z4b + 0.4), 0.0)
    # pilotis hall paving
    B.footprint = [(-15.0, 2.0), (15.0, 2.0), (15.0, 30.0), (-15.0, 30.0)]
    B.height = 39.6
    nf = len(B.flats)
    B.note("%d flats on 6 storeys, 4 crossing bars; main lobby + gallery 'view' spot at z=%.1f" % (nf, z4b + 0.4))


def _toward(p, q, d, poly, ei, rev=False):
    """Inner corner point of the wall p->q moved inward by d (rectangle CCW)."""
    xs = [a[0] for a in (poly[0][0], poly[1][0], poly[2][0], poly[3][0])]
    ys = [a[1] for a in (poly[0][0], poly[1][0], poly[2][0], poly[3][0])]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    x = p[0] + (d if p[0] < cx else -d)
    y = p[1] + (d if p[1] < cy else -d)
    return (x, y)


from _wallhang import hung  # noqa: E402  (QA r9: nothing hangs over the glass)
build = hung(build)
