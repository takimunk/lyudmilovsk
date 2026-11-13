"""Capsule tower: two concrete service cores with prefabricated living capsules plugged onto them,
each capsule one flat with one big round window. An original design in the spirit of 1970s Japanese
Metabolism (prefab capsules on cores), not a copy of any building."""
import math
import random

import furn_int3 as fu
import interior_kit as kit
from citykit import box, prism, ring, DOOR_W, DOOR_H
from _surrounds import add_surrounds

INFO = {
    "name_ru": "Капсульная башня", "name_en": "Capsule Tower",
    "style": "metabolism", "use": "residential",
    "inspired_by": "Japanese Metabolism of the early 1970s: prefabricated steel capsules bolted to concrete cores",
}

CAP_W, CAP_D, CAP_H = 2.3, 3.6, 2.5         # one capsule (a studio flat)
STOREY = 2.8
STOREYS = 13
CORE = 5.2                                    # each core is CORE x CORE
GAP = 2.4                                     # the bridge between the two cores
WIN_R = 0.62
# ---- per-plot variant parameters
SEED = 1972                                   # capsule layout (which capsules are missing, tilts)
CAPS = ("CT_Aluminium",)                      # capsule skin colours, picked at random per capsule
ACCENT = "CT_PlasterRed"                      # the lobby's upholstery colour
RESIDENT = "radio"                            # who lives in the caretaker's capsule: "radio" amateur | "student"
VARIANTS = {
    "P-CAPSULE": {"SEED": 1981, "CAPS": ("CT_Aluminium", "CT_Aluminium", "CT_PlasterRed"), "ACCENT": "CT_PlasterRed",
                  "RESIDENT": "radio"},
    "P-T5H1": {"SEED": 2007, "STOREYS": 15, "CAPS": ("CT_SteelWhite", "CT_SteelWhite", "CT_PlasterYellow", "CT_PlasterBlue"),
               "ACCENT": "CT_PlasterBlue", "RESIDENT": "student"},
}


def _round_window(B, cx, cz, face_y, out, flat, axis="y", fx=None):
    """A round window (a 16-gon card) on a capsule's face; out = +1/-1 the outward sign along the axis."""
    segs = 16
    pts = []
    for k in range(segs):
        a = 2 * math.pi * k / segs
        u, v = WIN_R * math.cos(a), WIN_R * math.sin(a)
        if axis == "y":
            pts.append((cx - u * out, face_y, cz + v))
        else:
            pts.append((fx, cx + u * out, cz + v))
    # the card: approximate the circle by its bounding quad (the window shader masks a circle by UV)
    if axis == "y":
        q = [(cx - WIN_R * out, face_y, cz - WIN_R), (cx + WIN_R * out, face_y, cz - WIN_R),
             (cx + WIN_R * out, face_y, cz + WIN_R), (cx - WIN_R * out, face_y, cz + WIN_R)]
    else:
        q = [(fx, cx + WIN_R * out, cz - WIN_R), (fx, cx - WIN_R * out, cz - WIN_R),
             (fx, cx - WIN_R * out, cz + WIN_R), (fx, cx + WIN_R * out, cz + WIN_R)]
    # bezel ring (detail) standing 6 cm off the face
    for k in range(segs):
        a0, a1 = 2 * math.pi * k / segs, 2 * math.pi * (k + 1) / segs
        r0, r1 = WIN_R, WIN_R + 0.12
        def P(r, a, o):
            u, v = r * math.cos(a), r * math.sin(a)
            if axis == "y":
                return (cx - u * out, face_y + out * o, cz + v)
            return (fx + out * o, cx + u * out, cz + v)
        B.detail.face("CT_SteelWhite", [P(r0, a0, 0.06), P(r0, a1, 0.06), P(r1, a1, 0.06), P(r1, a0, 0.06)][::-1])
        B.detail.face("CT_SteelWhite", [P(r0, a0, 0.0), P(r0, a1, 0.0), P(r0, a1, 0.06), P(r0, a0, 0.06)])
    return B.window(q, flat=flat, kind="flat", inset=0.05, frame=0, shape="round")


def build(B):
    rnd = random.Random(SEED)
    crnd = random.Random(SEED + 1)
    W = CORE * 2 + GAP
    x0 = -W / 2
    cores = [(x0, x0 + CORE), (x0 + CORE + GAP, x0 + W)]
    y0, y1 = 5.6, 5.6 + CORE                      # cores stand back from the street line: capsules + canopy in front
    top = STOREYS * STOREY + 2.0
    for cx0, cx1 in cores:
        prism(B.m, "CT_Concrete", [(cx0, y0), (cx1, y0), (cx1, y1), (cx0, y1)], 0, top)
        # stair windows: a slit up each core's street face
        for s in range(1, STOREYS):
            z = s * STOREY + 0.8
            u = (cx0 + cx1) / 2
            B.window([(u - 0.3, y0, z), (u + 0.3, y0, z), (u + 0.3, y0, z + 1.4), (u - 0.3, y0, z + 1.4)],
                     kind="stair", inset=0.02, frame=0.03)
    # bridges between the cores every storey (walkways to the capsules' doors)
    for s in range(1, STOREYS + 1):
        z = s * STOREY
        box(B.m, "CT_ConcreteDark", cores[0][1], cores[1][0], y0 + 1.5, y1 - 1.5, z - 0.25, z)
        B.railing([(cores[0][1], y0 + 1.5), (cores[1][0], y0 + 1.5)], z, h=1.1)
        B.railing([(cores[1][0], y1 - 1.5), (cores[0][1], y1 - 1.5)], z, h=1.1)
    # roof: the lift machine rooms
    box(B.m, "CT_ConcreteDark", cores[0][0] + 1, cores[0][1] - 1, y0 + 1, y1 - 1, top, top + 2.4)
    box(B.m, "CT_ConcreteDark", cores[1][0] + 1, cores[1][1] - 1, y0 + 1, y1 - 1, top, top + 2.4)
    # ground floor: a glazed lobby between and under the cores, a canopy over the door
    lob = (x0 - 1.0, x0 + W + 1.0, 2.0, y1 + 1.0)
    box(B.m, "CT_ConcreteDark", lob[0], lob[1], lob[2], lob[3], 3.6, 3.9)               # lobby roof slab
    for u in (lob[0] + 0.15, lob[1] - 0.15):
        box(B.m, "CT_Concrete", u - 0.15, u + 0.15, lob[2], lob[2] + 0.3, 0, 3.6)
    # lobby glazing: big shop cards along the street front with the door gap in the middle
    gx = [lob[0] + 0.3, -DOOR_W / 2 - 0.2, DOOR_W / 2 + 0.2, lob[1] - 0.3]
    for a, b in ((gx[0], gx[1]), (gx[2], gx[3])):
        n = max(1, int((b - a) / 1.8))
        for i in range(n):
            u0 = a + (b - a) * i / n
            u1 = a + (b - a) * (i + 1) / n
            B.window([(u0, lob[2], 0.1), (u1, lob[2], 0.1), (u1, lob[2], 3.5), (u0, lob[2], 3.5)],
                     kind="public", inset=0.0, frame=0.06)
    box(B.m, "CT_Steel", -2.0, 2.0, -0.6, lob[2], 3.3, 3.45)                                # canopy
    B.entrance("main", (0.0, lob[2], 0.0), 0.0, width=DOOR_W * 2, leaves=2, kind="street")
    # lobby furnishing: letter boxes on each core's street face, a doormat, plants, two lamps under the roof slab
    for (cx0_, cx1_) in cores:
        fu.mailboxes(B, fu.Fr((cx0_ + cx1_) / 2, y0, 0.0, 0.0), 0.0, 1.0, cols=6, rows=3)
    fu.doormat(B, fu.Fr(0.0, lob[2], 0.0, 0.0), 0.0, -0.6, size=(1.4, 0.7))
    for lx_ in (-3.0, 3.0):
        B.lamp((lx_, 3.6, 3.3), lumens=520, colour="#FFE0B0", kind="lobby", radius=6.0)
        box(B.detail, "CT_Steel", lx_ - 0.01, lx_ + 0.01, 3.59, 3.61, 3.38, 3.6, faces="xXyY")
    # capsules: on the outer faces of the cores, staggered, each a flat
    n_flat = 0
    sides = []
    for ci, (cx0, cx1) in enumerate(cores):
        sides.append(("front", ci, cx0, cx1))
        sides.append(("back", ci, cx0, cx1))
        sides.append(("outer", ci, cx0, cx1))
    for s in range(1, STOREYS + 1):
        zf = s * STOREY - 0.3
        k = 0
        for side, ci, cx0, cx1 in sides:
            if side in ("front", "back"):
                count = 2
                for i in range(count):
                    if rnd.random() < 0.12:
                        continue
                    jitter = rnd.uniform(-0.15, 0.15)
                    u = cx0 + 0.1 + i * (CAP_W + 0.4) + jitter
                    tilt = rnd.choice((0, 0, 0.35))
                    if side == "front":
                        ya, yb = y0 - CAP_D, y0
                        face, out = ya, -1
                    else:
                        ya, yb = y1, y1 + CAP_D
                        face, out = yb, 1
                    za = zf + tilt
                    box(B.m, crnd.choice(CAPS), u, u + CAP_W, ya, yb, za, za + CAP_H)
                    k += 1
                    fid = "%d-%02d" % (s, k)
                    B.flat(fid, storey=s, beds=1, door=((u + CAP_W / 2), y0 if side == "front" else y1, zf + 0.3),
                           entrance="main")
                    cz = za + CAP_H / 2 + 0.05
                    _round_window(B, u + CAP_W / 2, cz, face, out, fid, axis="y")
            else:
                # the outer side of each core: two capsules side by side along y, facing out along x
                xs = (cx0 - CAP_D, cx0) if ci == 0 else (cx1, cx1 + CAP_D)
                out = -1 if ci == 0 else 1
                for i in range(2):
                    if rnd.random() < 0.12:
                        continue
                    v = y0 + 0.1 + i * (CAP_W + 0.4)
                    za = zf + rnd.choice((0, 0, 0.35))
                    box(B.m, crnd.choice(CAPS), xs[0], xs[1], v, v + CAP_W, za, za + CAP_H)
                    k += 1
                    fid = "%d-%02d" % (s, k)
                    fx = xs[0] if ci == 0 else xs[1]
                    B.flat(fid, storey=s, beds=1, door=(cx0 if ci == 0 else cx1, v + CAP_W / 2, zf + 0.3), entrance="main")
                    _round_window(B, v + CAP_W / 2, za + CAP_H / 2 + 0.05, None, out, fid, axis="x", fx=fx)
        n_flat += k
    B.footprint = [(x0 - CAP_D, y0 - CAP_D), (x0 + W + CAP_D, y0 - CAP_D), (x0 + W + CAP_D, y1 + CAP_D), (x0 - CAP_D, y1 + CAP_D)]
    B.height = top + 2.4
    _lobby(B, lob, cores)
    B.note("%d capsule flats on %d storeys; capsules jut %.1f m over the street line at the front" % (n_flat, STOREYS, CAP_D - y0 if CAP_D > y0 else 0))
    add_surrounds(B, seed=6)


# ================================================================================================ interiors
def _bike(B, x, y, yaw=0.0, mat="CT_SteelRed"):
    """A leaning city bicycle (local: length along y, front wheel toward -y)."""
    p = kit._P(B, x, y, 0.0, yaw, "bicycle")
    p.kind = "prop"
    R = 0.33
    for yc in (-0.55, 0.55):
        ring_pts = [(0.0, yc + R * math.cos(2 * math.pi * k / 14), R + R * math.sin(2 * math.pi * k / 14)) for k in range(15)]
        p.tube("CT_Rubber", ring_pts, 0.04, 4)
        p.rod(kit.ALU, (0.0, yc, R), (0.0, yc, R + 0.001), 0.04, 4)
    p.rod(mat, (0.0, 0.55, R), (0.0, 0.05, 0.62), 0.03, 4)
    p.rod(mat, (0.0, 0.05, 0.62), (0.0, -0.4, 0.66), 0.03, 4)
    p.rod(mat, (0.0, 0.55, R), (0.0, 0.0, 0.3), 0.03, 4)
    p.rod(mat, (0.0, 0.0, 0.3), (0.0, 0.05, 0.62), 0.03, 4)
    p.rod(mat, (0.0, -0.4, 0.66), (0.0, -0.55, R), 0.03, 4)
    p.rod(kit.STEEL, (0.0, -0.42, 0.62), (0.0, -0.42, 0.98), 0.025, 4)
    p.rod(kit.STEEL, (-0.28, -0.42, 0.99), (0.28, -0.42, 0.99), 0.025, 4)
    p.rod(kit.LAQ, (0.0, 0.1, 0.62), (0.0, 0.1, 0.92), 0.025, 4)
    p.box(kit.LAQ, -0.06, 0.06, 0.02, 0.24, 0.92, 0.96)
    p.rod(kit.STEEL, (0.0, 0.0, 0.3), (0.0, -0.07, 0.3), 0.04, 4)
    return p.done()


def _pram(B, x, y, yaw=0.0, mat="CT_PlasterBlue"):
    """A pram: a hooded body on a chassis with four wheels and a handle (local: handle toward +y)."""
    p = kit._P(B, x, y, 0.0, yaw, "pram")
    p.kind = "prop"
    for sx in (-0.26, 0.26):
        for yc in (-0.34, 0.34):
            pts = [(sx, yc + 0.14 * math.cos(2 * math.pi * k / 10), 0.14 + 0.14 * math.sin(2 * math.pi * k / 10)) for k in range(11)]
            p.tube("CT_Rubber", pts, 0.035, 4)
            p.rod(kit.ALU, (sx, yc, 0.14), (sx * 0.8, yc, 0.4), 0.02, 4)
    p.bev(mat, -0.27, 0.27, -0.46, 0.46, 0.4, 0.62, 0.03, 0.04)
    p.cushion("CT_TileWhite", -0.24, 0.24, -0.4, 0.36, 0.6, 0.66, 0.02)
    p.cushion(mat, -0.27, 0.27, -0.46, -0.04, 0.62, 0.98, 0.04)
    p.tube(kit.STEEL, [(-0.25, 0.42, 0.6), (-0.25, 0.62, 0.95), (0.25, 0.62, 0.95), (0.25, 0.42, 0.6)], 0.025, 4)
    return p.done()


def _lobby(B, lob, cores):
    """The glazed lobby as a house hall: boards, mailboxes (already on the core faces), a concierge's desk, a waiting
    corner, a book swap, a pram and a bike, plants, a radio; and the caretaker's capsule on the east end."""
    A = ACCENT
    f = lambda fn, x, y, yaw=0.0, z=0.0, **kw: fn(B, x, y, z, yaw, **kw)
    yw = lob[2]                                   # the street front line of the lobby
    # ---- west half of the front strip: waiting corner
    f(kit.rug, -3.7, 3.9, 90.0, w=1.8, d=2.4, name="rug_2")
    f(kit.coffee_table, -3.7, 3.9, 90.0, w=0.8, d=0.5)
    f(kit.newspaper, -3.85, 3.9, 80.0, z=0.42, cellname="news_0")
    f(kit.vase, -3.5, 3.9, 0.0, z=0.42)
    a1 = f(kit.armchair, -5.0, 3.9, 90.0, mat=A, solid=False)
    a2 = f(kit.armchair, -2.4, 3.9, 270.0, mat="CT_PlasterGreen", solid=False)
    f(kit.floor_lamp, -4.9, 4.75, 0.0)
    bn = f(kit.bench, -5.6, 5.28, 0.0, w=1.4, kind="park")
    f(kit.plant, -6.2, 3.3, 0.0, kind="ficus", pot="CT_PlasterBlue", h=1.2, seed=2)
    f(kit.plant, -2.5, 2.7, 0.0, kind="fern", pot="CT_PlasterYellow", h=0.8, seed=3)
    f(kit.hat_stand, -1.2, 2.75, 0.0)
    _bike(B, -6.9, 4.1, 90.0)
    _pram(B, -6.6, 4.9, 0.0)
    # west core face (y = 5.6, room toward -y): lost-and-found board, clock, picture
    nb1 = f(kit.notice_board, -5.6, 5.6, 0.0, w=1.0, zc=1.55, seed=11)
    f(kit.wall_clock, -5.6, 5.6, 0.0, zc=2.5, face="clock_1")
    f(kit.picture, -1.95, 5.6, 0.0, name="painting_sea_calm", w=0.6, zc=1.7)
    tb = f(kit.table, -1.95, 5.3, 0.0, w=0.8, d=0.4, h=0.75)
    f(kit.radio, -2.05, 5.3, 0.0, z=0.75)
    f(kit.vase, -1.7, 5.3, 0.0, z=0.75)
    # ---- east half: the concierge's post in front of the mailboxes
    cd = f(kit.office_desk, 2.4, 4.0, 0.0, w=1.2, d=0.6, typewriter_on=False, papers=True, phone=True)
    cc = f(kit.chair, 2.4, 4.6, 0.0, kind="wood", mat=A)
    f(kit.table_lamp, 1.95, 3.95, 0.0, z=0.75, shade="CT_PlasterGreen")
    f(kit.cup, 2.75, 4.05, 0.0, z=0.75)
    f(kit.hat_stand, 0.95, 5.2, 0.0)
    nb2 = f(kit.notice_board, 1.75, 5.6, 0.0, w=0.8, zc=1.55, seed=12)
    f(kit.sign_plate, 1.75, 5.6, 0.0, name="sign_quiet", w=0.5, zc=2.35)
    f(kit.plant, 3.4, 3.0, 0.0, kind="monstera", pot="CT_PlasterRed", h=1.1, seed=4)
    # ---- the passage between the cores
    sh = f(kit.bookshelf, 1.2, 6.5, 270.0, w=0.9, h=1.7, levels=4, topic="travel", seed=5)
    ph = f(kit.wall_phone, 1.2, 7.7, 270.0)
    f(kit.picture, 1.2, 8.7, 270.0, name="painting_dunes", w=0.7, zc=1.7)
    nb3 = f(kit.notice_board, -1.2, 6.6, 90.0, w=0.9, zc=1.5, seed=13)
    f(kit.poster, -1.2, 7.8, 90.0, name="poster_cons_1", w=0.6, zc=1.6)
    f(kit.fire_extinguisher, -1.2, 9.3, 90.0)
    f(kit.plant, -0.8, 10.2, 0.0, kind="palm", pot="CT_PlasterGreen", h=1.4, seed=6)
    f(kit.wall_clock, -1.2, 8.7, 90.0, zc=2.1, face="clock_2")

    B.spot("lobby_bench", "bench", bn["seats"][0][:2] + (0.0,), bn["seats"][0][3]) if bn["seats"] else \
        B.spot("lobby_bench", "bench", (-5.6, 4.95, 0.0), 0.0)
    B.spot("lobby_desk", "seat", cc["seats"][0][:2] + (0.0,), cc["seats"][0][3]) if cc["seats"] else \
        B.spot("lobby_desk", "seat", (2.4, 4.6, 0.0), 0.0)
    # ---- things
    def seat_thing(tid, r, i=0):
        s = r["seats"][i]
        B.thing(tid, "seat", (s[0], s[1], s[2]), s[3], height=round(s[2], 2))
    seat_thing("lobby_bench_seat", bn)
    seat_thing("lobby_armchair_w", a1)
    seat_thing("lobby_armchair_e", a2)
    seat_thing("lobby_desk_chair", cc)
    B.thing("lobby_board_lost", "board", (-5.6, 5.55, 1.55), 0.0, topic="lost")
    B.thing("lobby_board_club", "board", (1.75, 5.55, 1.55), 0.0, topic="club")
    B.thing("lobby_board_house", "board", (-1.15, 6.6, 1.5), 90.0, topic="rules")
    B.thing("lobby_swap_shelf", "shelf", (1.25, 6.5, 1.3), 270.0, topic="travel")
    B.thing("lobby_payphone", "machine", (1.2, 7.7, 1.4), 270.0, what="payphone")
    B.thing("lobby_radio", "machine", (-2.05, 5.3, 1.0), 0.0, what="jukebox")
    B.thing("lobby_picture_sea", "board", (-1.95, 5.55, 1.7), 0.0, topic="painting")
    B.thing("lobby_picture_dunes", "board", (1.15, 8.7, 1.7), 270.0, topic="painting")
    B.thing("lobby_paper", "take", (-3.85, 3.9, 0.45), 0.0, item="daily_paper")
    _capsule(B)


def _capsule(B):
    """The caretaker's capsule at ground level under the east capsule stack: one flat, a radio amateur's (or a
    student's), entered through the door in its west wall. Inner x 4.78..6.94, y 2.47..5.43, ceiling 2.3."""
    X0, X1, Y0, Y1, T = 4.66, 7.06, 2.35, 5.55, 0.12
    ZT = 2.42                                       # the roof (the capsules above start at 2.5)
    skin = ACCENT if ACCENT != "CT_PlasterBlue" else "CT_SteelWhite"
    ix0, ix1, iy0, iy1 = X0 + T, X1 - T, Y0 + T, Y1 - T
    radio_man = RESIDENT == "radio"
    # shell: floor, roof, back (north, against the core), east, south (street) with the round window, west with the door
    box(B.m, "CT_ConcreteDark", X0, X1, Y0, Y1, -0.06, 0.02, faces="xXyYZ")
    box(B.m, skin, X0, X1, Y0, Y1, 2.3, ZT, faces="xXyYZz")
    box(B.m, skin, X0, X1, Y1 - T, Y1, 0.02, 2.3)
    box(B.m, skin, X1 - T, X1, Y0, Y1 - T, 0.02, 2.3)
    # south wall with a square hole for the round window (centre x 5.35, z 1.45, half side 0.52)
    wx, wz, wr = 5.35, 1.45, 0.52
    box(B.m, skin, X0, wx - wr, Y0, Y0 + T, 0.02, 2.3)
    box(B.m, skin, wx + wr, X1 - T, Y0, Y0 + T, 0.02, 2.3)
    box(B.m, skin, wx - wr, wx + wr, Y0, Y0 + T, 0.02, wz - wr)
    box(B.m, skin, wx - wr, wx + wr, Y0, Y0 + T, wz + wr, 2.3)
    # west wall: door hole y 3.9..5.0 (1.1 wide, 2.2 high)
    box(B.m, skin, X0, X0 + T, Y0 + T, 3.9, 0.02, 2.3)
    box(B.m, skin, X0, X0 + T, 5.0, Y1 - T, 0.02, 2.3)
    box(B.m, skin, X0, X0 + T, 3.9, 5.0, 2.2, 2.3)
    # the round window: card in the wall thickness, four corner fillers outside, a bezel ring
    wy = Y0 - 0.004
    B.window([(wx - wr, Y0, wz - wr), (wx + wr, Y0, wz - wr), (wx + wr, Y0, wz + wr), (wx - wr, Y0, wz + wr)],
             kind="public", inset=0.06, frame=0, shape="round")
    r = wr
    for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        arc = [(wx + su * r * math.cos(math.pi / 2 * q / 4), wy, wz + sv * r * math.sin(math.pi / 2 * q / 4)) for q in range(5)]
        corner = (wx + su * r, wy, wz + sv * r)
        for q in range(4):
            tri = [corner, arc[q], arc[q + 1]]
            if _tri_faces(tri, (0, -1, 0)):
                B.detail.face("CT_SteelWhite", tri)
            else:
                B.detail.face("CT_SteelWhite", tri[::-1])
    seg = 16
    for k in range(seg):
        a0, a1 = 2 * math.pi * k / seg, 2 * math.pi * (k + 1) / seg
        P = lambda rr, a, o: (wx + rr * math.cos(a), Y0 - o, wz + rr * math.sin(a))
        B.detail.face("CT_SteelWhite", [P(r, a0, 0.0), P(r, a1, 0.0), P(r + 0.1, a1, 0.05), P(r + 0.1, a0, 0.05)][::-1])
    # lining of the room (detail, 2 cm into the room), floor boards
    box(B.detail, "CT_Plaster", ix0, ix1, iy1 - 0.02, iy1, 0.03, 2.28, faces="y")
    box(B.detail, "CT_Plaster", ix1 - 0.02, ix1, iy0, iy1 - 0.02, 0.03, 2.28, faces="x")
    box(B.detail, "CT_Plaster", ix0, ix1 - 0.02, iy0, iy0 + 0.02, 0.03, 2.28, faces="Y")
    box(B.detail, "CT_Plaster", ix0, ix0 + 0.02, iy0, 3.9, 0.03, 2.28, faces="X")
    box(B.detail, "CT_Plaster", ix0, ix0 + 0.02, 5.0, iy1 - 0.02, 0.03, 2.28, faces="X")
    box(B.detail, "CT_WoodLight", ix0, ix1, iy0, iy1, 0.02, 0.03, faces="Z")
    box(B.detail, "CT_Interior", ix0, ix1, iy0, iy1, 2.28, 2.3, faces="z")
    box(B.detail, "CT_WoodLight", ix0, ix1, iy0, iy1, 0.02, 0.03, faces="Z")
    ix0, ix1, iy1 = ix0 + 0.02, ix1 - 0.02, iy1 - 0.02            # wall pieces stand on the lining
    f = lambda fn, x, y, yaw=0.0, z=0.0, **kw: fn(B, x, y, z, yaw, **kw)
    # bed along the east wall, head north
    bd = f(kit.bed, 6.43, 4.4, 0.0, size="single", blanket="CT_PlasterGreen" if radio_man else "CT_PlasterRed")
    # desk under the window, chair, equipment
    dk = f(kit.office_desk, 5.35, 2.78, 0.0, w=1.1, d=0.5, typewriter_on=not radio_man, papers=True, phone=False, seed=3)
    ch = f(kit.chair, 5.35, 3.4, 0.0, kind="wood", mat="CT_PlasterYellow")
    f(kit.lamp_green, 5.0, 2.62, 180.0, z=0.75)
    if radio_man:
        f(kit.radio, 5.62, 2.6, 0.0, z=0.75)
        f(kit.telephone, 5.12, 2.9, 200.0, z=0.75)
        f(kit.cup, 5.78, 2.95, 0.0, z=0.75)
    else:
        f(kit.books_stack, 5.6, 2.62, 20.0, z=0.75, n=5, seed=2)
        f(kit.globe, 5.05, 2.62, 0.0, z=0.75)
        f(kit.cup, 5.75, 2.95, 0.0, z=0.75)
    # bookshelf on the west wall south of the door
    sh = f(kit.bookshelf, ix0, 3.38, 90.0, w=0.8, h=1.9, levels=5, topic="engineering" if radio_man else "science", seed=7)
    # north wall: a shelf with the record player, pictures
    ws = f(kit.wall_shelf, 5.3, iy1, 0.0, w=0.9, zt=1.25, props=False)
    rp = f(kit.record_player, 5.5, iy1 - 0.12, 0.0, z=1.25)
    f(kit.table_lamp, 5.0, iy1 - 0.12, 0.0, z=1.25, shade="CT_PlasterYellow")
    f(kit.picture, 5.3, iy1, 0.0, name="painting_lighthouse", w=0.5, zc=1.85)
    # east wall above the bed: a map / chart and cards
    if radio_man:
        f(kit.poster, ix1, 4.1, 270.0, name="map_world", w=1.1, zc=1.6)
        f(kit.poster, ix1, 5.0, 270.0, name="label_12", w=0.22, zc=1.5)
        f(kit.poster, ix1, 5.25, 270.0, name="label_31", w=0.22, zc=1.5)
        f(kit.poster, ix1, 3.3, 270.0, name="paper_letter", w=0.3, zc=1.7)
    else:
        f(kit.poster, ix1, 4.1, 270.0, name="chart_periodic", w=0.8, zc=1.65)
        f(kit.poster, ix1, 5.0, 270.0, name="poster_cons_3", w=0.45, zc=1.6)
        f(kit.poster, ix1, 3.5, 270.0, name="paper_drawing", w=0.35, zc=1.7)
    f(kit.rug, 5.4, 4.2, 0.0, w=1.0, d=1.5, name="rug_3")
    f(kit.plant, 4.95, 5.2, 0.0, kind="cactus", pot="CT_PlasterRed", h=0.6, seed=9)
    # fixture without light; the room is lit by the lobby and the round window
    kit.pendant(B, 5.9, 3.7, 2.28, drop=0.2, light=True, lumens=260, colour="#FFE0B0", radius=3.5)
    # things
    s = ch["seats"][0]
    B.thing("capsule_desk_chair", "seat", (s[0], s[1], s[2]), s[3], height=round(s[2], 2))
    B.thing("capsule_bed", "bed", (6.43, 4.4, 0.5), 270.0, height=0.5, rest="private")
    B.thing("capsule_shelf", "shelf", (ix0 + 0.15, 3.38, 1.3), 90.0, topic="engineering" if radio_man else "science")
    B.thing("capsule_records", "machine", (5.5, iy1 - 0.12, 1.3), 0.0, what="record_player")
    B.thing("capsule_window", "board", (wx, Y0 + 0.1, wz), 180.0, topic="view")
    if radio_man:
        B.thing("capsule_radio_set", "machine", (5.62, 2.6, 0.9), 180.0, what="radio_set")
        B.thing("capsule_map", "board", (ix1 - 0.05, 4.1, 1.6), 270.0, topic="travel")
    else:
        B.thing("capsule_study_desk", "craft", (5.35, 2.78, 0.85), 180.0, what="study_desk")
        B.thing("capsule_chart", "board", (ix1 - 0.05, 4.1, 1.65), 270.0, topic="wallpaper")
    B.spot("capsule_chair", "seat", (s[0], s[1], 0.0), s[3])
    B.note("caretaker's capsule at ground level (x %.2f..%.2f): %s" % (X0, X1, RESIDENT))


def _tri_faces(tri, out):
    u = [tri[1][i] - tri[0][i] for i in range(3)]
    v = [tri[2][i] - tri[0][i] for i in range(3)]
    n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
    return n[0] * out[0] + n[1] * out[1] + n[2] * out[2] > 0
