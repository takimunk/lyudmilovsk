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
    f(kit.fire_extinguisher, -1.7, 6.14, 0.0)
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
    f(kit.telephone, 3.7, 12.9, 200.0, z=1.1)
    f(kit.radio, 5.9, 12.95, 180.0, z=1.1)
    f(kit.cup, 5.4, 12.5, 0.0, z=1.1)
    f(kit.table_lamp, 4.3, 12.85, 0.0, z=1.1, shade="CT_PlasterGreen")
    f(kit.lockers, 5.85, 14.68, 0.0, n=3, mat="CT_PlasterGreen", solid=False)
    # ---- east side: mailboxes and the board between the windows, prams and bikes, a visitors' table
    f(hk.mailboxes, 6.68, 9.0, 270.0, cols=6, rows=5)
    hk.pram(B, 6.0, 5.6, 80.0, mat="CT_PlasterBlue")
    hk.bike(B, 6.3, 7.0, 0.0)
    f(kit.hat_stand, 3.6, 5.4, 0.0)
    f(kit.plant, 6.0, 10.4, 0.0, kind="fern", pot="CT_PlasterYellow", h=1.0, seed=14)
    f(kit.plant, 3.4, 11.0, 0.0, kind="ficus", pot="CT_PlasterGreen", h=1.5, seed=16)
    f(kit.magazine_rack, 0.0, 14.68, 0.0, w=0.9)
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
    B.thing("gallery_binoculars", "machine", t3["pos"][:2] + (z + 1.2,), 180.0, what="binoculars")
    B.thing("gallery_chess", "play", ct["pos"][:2] + (z + 0.78,), 0.0, what="chess")
    B.thing("gallery_board_coast", "board", (4.17, 6.0, z + 1.6), 270.0, topic="view")
    B.thing("gallery_board_birds", "board", (4.17, 9.0, z + 1.6), 270.0, topic="view")
    B.thing("gallery_board_town", "board", (-4.17, 15.0, z + 1.6), 90.0, topic="view")
    B.thing("gallery_picture_sea", "board", (4.17, 12.0, z + 1.6), 270.0, topic="painting")
    B.thing("gallery_picture_night", "board", (-4.17, 21.0, z + 1.6), 90.0, topic="painting")
    B.thing("gallery_picture_lighthouse", "board", (4.17, 27.0, z + 1.6), 270.0, topic="painting")
    H.check("beam-gallery")


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
