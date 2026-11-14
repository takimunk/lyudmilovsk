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
    _deepen(B, lob, cores)
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
    _pram(B, -6.8, 4.75, 0.0)
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
        f(kit.poster, ix1, 5.0, 270.0, name="paper_drawing", w=0.22, zc=1.5)
        f(kit.poster, ix1, 5.25, 270.0, name="paper_typed", w=0.2, zc=1.5)
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


# ================================================================================================ round 2: depth pass
LIFT_S7 = 7                                  # the bridge storey with the first lift stop (the top stop is STOREYS)


def _lift_door(B, x, y, z, yaw):
    """[wall] A lift door set in a core's wall plane through (x, y): steel frame, two leaves, a call plate with a red
    button, a floor lamp over the door. yaw = the way the wall's face looks (0 = -y, 90 = +x, 270 = -x)."""
    p = kit._P(B, x, y, z, yaw, "lift_door")
    p.kind = "wall"
    p.box("CT_Steel", -0.58, 0.58, -0.05, 0.0, 0.0, 2.2, f="xXyZ")
    p.box("CT_SteelWhite", -0.48, -0.005, -0.075, -0.05, 0.0, 2.1, f="xXyZ")
    p.box("CT_SteelWhite", 0.005, 0.48, -0.075, -0.05, 0.0, 2.1, f="xXyZ")
    p.box("CT_Steel", -0.04, 0.04, -0.09, -0.075, 0.0, 2.1, f="xXyZ")
    p.box("CT_ConcreteWhite", 0.66, 0.74, -0.03, 0.0, 1.0, 1.2, f="xXyZz")
    p.box("CT_SteelRed", 0.685, 0.715, -0.045, -0.03, 1.07, 1.13, f="xXyZz")
    p.box("CT_Opal", -0.12, 0.12, -0.035, 0.0, 2.28, 2.38, f="xXyZz")
    return p.done()


def _switch_plate(B, x, y, z, yaw, zc=1.3):
    """[wall] A two-key light switch on a bakelite plate."""
    p = kit._P(B, x, y, z, yaw, "switch_plate")
    p.kind = "wall"
    p.box("CT_ConcreteWhite", -0.06, 0.06, -0.015, 0.0, zc - 0.06, zc + 0.06, f="xXyZz")
    for u in (-0.022, 0.022):
        p.box("CT_Steel", u - 0.012, u + 0.012, -0.03, -0.015, zc - 0.03, zc + 0.03, f="xXyZz")
    return p.done()


def _key_board(B, x, y, z, yaw, zc=1.35):
    """[wall] A wooden key board: two rows of brass hooks with keys on brass tags."""
    p = kit._P(B, x, y, z, yaw, "key_board")
    p.kind = "wall"
    p.box("CT_Wood", -0.25, 0.25, -0.03, 0.0, zc - 0.2, zc + 0.2, f="xXyZz")
    p.box("CT_PlasterGreen", -0.22, 0.22, -0.034, -0.03, zc + 0.12, zc + 0.17, f="xXyZz")
    for r_, zz in enumerate((zc + 0.02, zc - 0.12)):
        for k in range(5):
            u = -0.18 + k * 0.09
            p.rod("CT_Brass", (u, -0.03, zz), (u, -0.06, zz), 0.012, 4)
            if (k + r_) % 4 != 3:
                p.box("CT_Brass", u - 0.012, u + 0.012, -0.066, -0.058, zz - 0.07, zz - 0.012, f="xXyZz")
    return p.done()


def _umbrella_stand(B, x, y, z=0.0):
    p = kit._P(B, x, y, z, 0.0, "umbrella_stand")
    p.kind = "prop"
    p.cyl("CT_SteelRed", 0, 0, 0.0, 0.5, 0.17, 8, r1=0.2)
    for k, (a, b, c) in enumerate(((0.05, 0.02, 0.15), (-0.06, 0.0, -0.1), (0.0, -0.06, 0.05))):
        p.rod("CT_ConcreteDark" if k != 1 else "CT_PlasterBlue", (0.0, 0.0, 0.3), (a, b, 0.95 + c * 0.3), 0.03, 4)
    p.rod("CT_Wood", (0.05, 0.02, 0.95), (0.05, 0.02, 1.0), 0.045, 5)
    return p.done(footprint=(-0.2, 0.2, -0.2, 0.2))


def _desk_bell(B, x, y, z, yaw=0.0):
    p = kit._P(B, x, y, z, yaw, "desk_bell")
    p.kind = "prop"
    p.cyl("CT_Brass", 0, 0, 0.0, 0.015, 0.06, 10)
    p.lathe("CT_Brass", 0, 0, [(0.05, 0.015), (0.045, 0.04), (0.02, 0.055), (0.004, 0.06)], 10)
    p.rod("CT_Steel", (0, 0, 0.058), (0, 0, 0.075), 0.012, 4)
    return p.done()


def _ledger(B, x, y, z, yaw=0.0):
    """[prop] A big house ledger lying open on a desk."""
    p = kit._P(B, x, y, z, yaw, "ledger")
    p.kind = "prop"
    p.box("CT_Leather", -0.22, 0.22, -0.16, 0.16, 0.0, 0.02)
    p.box("CT_Paper", -0.2, -0.004, -0.14, 0.14, 0.02, 0.04)
    p.box("CT_Paper", 0.004, 0.2, -0.14, 0.14, 0.02, 0.04)
    p.card_top("paper_typed", -0.19, -0.01, -0.13, 0.13, 0.041)
    p.card_top("paper_typed", 0.01, 0.19, -0.13, 0.13, 0.041)
    p.rod("CT_PlasterRed", (0.15, -0.1, 0.045), (0.28, -0.06, 0.047), 0.01, 4)
    return p.done()


def _tape_recorder(B, x, y, z, yaw, reel=True):
    """[prop] A reel-to-reel tape recorder (reel=True, 0.4 x 0.28 x 0.16) or a cassette recorder with a handle."""
    p = kit._P(B, x, y, z, yaw, "tape_recorder")
    p.kind = "prop"
    if reel:
        p.bev("CT_Lacquer", -0.2, 0.2, -0.14, 0.14, 0.0, 0.1, 0.012)
        p.box("CT_Aluminium", -0.19, 0.19, -0.13, 0.13, 0.1, 0.108, f="xXyYZ")
        for sx in (-0.09, 0.09):
            p.cyl("CT_Aluminium", sx, 0.03, 0.108, 0.118, 0.092, 10)
            p.cyl("CT_Rubber", sx, 0.03, 0.118, 0.14, 0.066, 10)
            p.cyl("CT_Steel", sx, 0.03, 0.14, 0.15, 0.014, 6)
        p.box("CT_Steel", -0.06, 0.06, -0.1, -0.06, 0.108, 0.14)
        p.tube("CT_Rubber", [(-0.09, -0.03, 0.125), (-0.075, -0.08, 0.125), (0.075, -0.08, 0.125), (0.09, -0.03, 0.125)], 0.006, 3)
        for sx in (-0.12, -0.04, 0.04, 0.12):
            p.rod("CT_Steel", (sx, -0.14, 0.05), (sx, -0.17, 0.05), 0.032, 6)
        p.box("CT_Brass", -0.18, -0.06, -0.141, -0.14, 0.075, 0.09, f="xXyZz")
    else:
        p.bev("CT_SteelWhite", -0.17, 0.17, -0.08, 0.08, 0.0, 0.1, 0.01)
        p.box("CT_Glass", -0.07, 0.07, -0.05, 0.03, 0.1, 0.106, f="xXyYZ")
        p.card_front("grille", -0.15, -0.075, 0.015, 0.075, -0.082)
        for k in range(5):
            p.box("CT_Steel", -0.06 + k * 0.03, -0.04 + k * 0.03, -0.075, -0.045, 0.1, 0.112, f="xXyYZ")
        p.box("CT_SteelRed", 0.04, 0.056, -0.075, -0.045, 0.1, 0.113, f="xXyYZ")
        p.tube("CT_Steel", [(-0.12, 0.0, 0.1), (-0.12, 0.0, 0.17), (0.12, 0.0, 0.17), (0.12, 0.0, 0.1)], 0.014, 4)
    return p.done(footprint=(-0.2, 0.2, -0.14, 0.14))


def _tape_boxes(B, x, y, z, yaw, n=5, cols=None):
    """[prop] A row of tape boxes standing on their spines (0.04 apart)."""
    p = kit._P(B, x, y, z, yaw, "tape_boxes")
    p.kind = "prop"
    cols = cols or ("CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterGreen", "CT_Paper", "CT_PlasterRed")
    for k in range(n):
        p.box(cols[k % len(cols)], k * 0.04, k * 0.04 + 0.036, -0.08, 0.08, 0.0, 0.17 - (k % 3) * 0.01, f="xXyYZ")
    return p.done(footprint=(0.0, n * 0.04, -0.08, 0.08))


def _alarm_clock(B, x, y, z, yaw=0.0):
    p = kit._P(B, x, y, z, yaw, "alarm_clock")
    p.kind = "prop"
    p.cyl("CT_SteelRed", 0, 0, 0.012, 0.11, 0.055, 10)
    p.card_front("clock_2", -0.045, 0.045, 0.03, 0.12, -0.056)
    for sx in (-0.03, 0.03):
        p.cyl("CT_Brass", sx, 0.0, 0.11, 0.135, 0.022, 6)
    p.rod("CT_Steel", (-0.035, 0, 0.012), (-0.045, 0, 0.0), 0.012, 4)
    p.rod("CT_Steel", (0.035, 0, 0.012), (0.045, 0, 0.0), 0.012, 4)
    return p.done()


def _fold_table(B, x, y, z, yaw):
    """[wall] A fold-down table flap on a hinge board, held by two arms (0.7 x 0.42, top 0.75 m)."""
    p = kit._P(B, x, y, z, yaw, "fold_table")
    p.kind = "wall"
    p.box("CT_Wood", -0.4, 0.4, -0.025, 0.0, 0.66, 0.84, f="xXyZz")
    p.bev("CT_WoodLight", -0.35, 0.35, -0.43, -0.025, 0.72, 0.75, 0.008)
    for sx in (-0.3, 0.3):
        p.rod("CT_Steel", (sx, -0.03, 0.34), (sx, -0.38, 0.71), 0.022, 4)
    p.box("CT_Brass", -0.36, 0.36, -0.03, -0.015, 0.74, 0.76, f="xXyZz")
    p.core(-0.33, 0.33, -0.41, -0.03, 0.722, 0.742)
    return p.done(solid=True, top=0.75)


def _hooks(B, x, y, z, yaw, radio=True):
    """[wall] A coat board with three brass hooks: a coat and headphones (radio) / a scarf and a rucksack (student)."""
    p = kit._P(B, x, y, z, yaw, "hooks")
    p.kind = "wall"
    p.box("CT_Wood", -0.15, 0.15, -0.025, 0.0, 1.6, 1.8, f="xXyZz")
    for u in (-0.08, 0.0, 0.08):
        p.rod("CT_Brass", (u, -0.025, 1.7), (u, -0.07, 1.7), 0.014, 4)
    if radio:
        p.cushion("CT_PlasterGreen", -0.14, 0.04, -0.065, -0.03, 1.05, 1.66, 0.012)
        p.tube("CT_Steel", [(0.09, -0.07, 1.69), (0.08, -0.09, 1.58), (0.14, -0.09, 1.58), (0.13, -0.07, 1.69)], 0.012, 4)
        p.cyl("CT_Leather", 0.08, -0.09, 1.55, 1.6, 0.03, 6)
        p.cyl("CT_Leather", 0.14, -0.09, 1.55, 1.6, 0.03, 6)
    else:
        p.cushion("CT_PlasterRed", -0.03, 0.03, -0.065, -0.03, 1.15, 1.68, 0.008)
        p.cushion("CT_PlasterBlue", -0.13, -0.06, -0.065, -0.03, 1.3, 1.67, 0.008)
        p.cushion("CT_Leather", 0.04, 0.14, -0.1, -0.03, 1.2, 1.68, 0.015)
    return p.done()


def _sill_things(B, x, y, z, radio=True):
    """Little things on the round window's sill: radio valves in a row (radio) / a jar of sea water and a pebble (student)."""
    p = kit._P(B, x, y, z, 0.0, "sill_things")
    p.kind = "prop"
    if radio:
        for k in range(3):
            p.lathe("CT_Glass", k * 0.07, 0, [(0.022, 0.0), (0.022, 0.07), (0.012, 0.11), (0.004, 0.115)], 6)
            p.cyl("CT_Brass", k * 0.07, 0, 0.0, 0.015, 0.024, 6)
    else:
        p.lathe("CT_Glass", 0, 0, [(0.04, 0.0), (0.045, 0.02), (0.045, 0.1), (0.035, 0.11)], 8)
        p.lathe("CT_PlasterBlue", 0, 0, [(0.038, 0.012), (0.042, 0.04), (0.042, 0.075), (0.0, 0.075)], 8)
        p.cyl("CT_Aluminium", 0, 0, 0.11, 0.125, 0.036, 8)
        p.lathe("CT_ConcreteDark", 0.1, 0.0, [(0.028, 0.0), (0.03, 0.015), (0.015, 0.025), (0.0, 0.026)], 6)
    return p.done(footprint=(-0.05, 0.25, -0.05, 0.05))


def _reel_box(B, x, y, z, yaw, reel=True):
    """[prop] A flat tape box with a string, left on a bench (reel box) / a cassette in its case."""
    p = kit._P(B, x, y, z, yaw, "reel_box")
    p.kind = "prop"
    if reel:
        p.box("CT_Paper", -0.1, 0.1, -0.1, 0.1, 0.0, 0.04)
        p.box("CT_PlasterRed", -0.1, 0.1, -0.1, -0.0995, 0.0, 0.04, f="yxX")
        p.card_top("paper_typed", -0.05, 0.05, -0.05, 0.05, 0.041)
        p.rod("CT_Leather", (-0.1, 0.0, 0.043), (0.1, 0.0, 0.043), 0.006, 4)
    else:
        p.box("CT_Glass", -0.055, 0.055, -0.035, 0.035, 0.0, 0.016)
        p.card_top("paper_typed", -0.04, 0.04, -0.02, 0.02, 0.0165)
    return p.done()


def _lobby_more(B, lob, cores):
    f = lambda fn, x, y, yaw=0.0, z=0.0, **kw: fn(B, x, y, z, yaw, **kw)
    # lift door in the passage between the cores (east core's west face x 1.2, the passage's far end)
    _lift_door(B, 1.2, 10.0, 0.0, 270)
    B.thing("lift_g", "door", (1.16, 10.0, 1.1), 270.0, to="lift")
    B.spot("lobby_lift", "stand", (0.6, 10.0, 0.0), 270.0)
    # lamp for the passage (it had none) and a switch on the west column
    B.lamp((0.0, 8.4, 3.3), lumens=420, colour="#FFE0B0", kind="lobby", radius=5.0)
    box(B.detail, "CT_Steel", -0.01, 0.01, 8.39, 8.41, 3.38, 3.6, faces="xXyY")
    _switch_plate(B, lob[0] + 0.15, lob[2] + 0.3, 0.0, 180.0, zc=1.3)
    B.thing("lobby_switch", "machine", (lob[0] + 0.15, lob[2] + 0.33, 1.3), 180.0, what="switch", switch=11)
    # key board and an umbrella stand by the door, the desk's ledger and bell, a magazine on the coffee table
    _key_board(B, 2.6, 5.6, 0.0, 0.0)
    B.thing("lobby_keys", "board", (2.6, 5.55, 1.35), 0.0, topic="keys")
    _umbrella_stand(B, 1.0, 2.5)
    B.thing("lobby_umbrellas", "board", (1.0, 2.5, 0.7), 0.0, topic="umbrellas")
    _ledger(B, 2.3, 3.88, 0.75, 8.0)
    B.thing("lobby_logbook", "board", (2.3, 3.88, 0.8), 180.0, topic="log")
    _desk_bell(B, 2.95, 4.2, 0.75)
    B.thing("lobby_bell", "board", (2.95, 4.2, 0.82), 180.0, topic="bell")
    f(kit.newspaper, -3.5, 4.1, 100.0, z=0.42, cellname="mag_3")
    B.thing("lobby_magazine", "take", (-3.5, 4.1, 0.45), 0.0, item="magazine_travel")
    # things on what is already there
    B.thing("lobby_mail_w", "board", (-3.8, 5.55, 1.45), 0.0, topic="mail")
    B.thing("lobby_mail_e", "board", (3.8, 5.55, 1.45), 0.0, topic="mail")
    B.thing("lobby_bike", "board", (-6.9, 4.1, 0.7), 270.0, topic="bike")
    B.thing("lobby_pram", "board", (-6.8, 4.75, 0.7), 90.0, topic="pram")
    B.thing("lobby_ficus", "board", (-6.2, 3.3, 0.9), 0.0, topic="plant")
    B.thing("lobby_fern", "board", (-2.5, 2.7, 0.6), 0.0, topic="plant")
    B.thing("lobby_monstera", "board", (3.4, 3.0, 0.8), 0.0, topic="plant")
    B.thing("lobby_palm", "board", (-0.8, 10.2, 0.9), 0.0, topic="plant")
    B.thing("lobby_hats_w", "board", (-1.2, 2.75, 1.4), 0.0, topic="hats")
    B.thing("lobby_hats_e", "board", (0.95, 5.2, 1.4), 0.0, topic="hats")
    B.thing("lobby_extinguisher", "board", (-1.2, 9.3, 1.2), 90.0, topic="safety")
    B.thing("lobby_street", "board", (-3.7, lob[2] + 0.15, 1.5), 180.0, topic="view")
    B.thing("lobby_news", "board", (1.75, 5.55, 2.05), 0.0, topic="news")
    # townsfolk: the waiting corner, the post boxes
    B.spot("lobby_wait_w", "seat", (-4.95, 3.9, 0.0), 90.0)
    B.spot("lobby_wait_e", "seat", (-2.45, 3.9, 0.0), 270.0)
    B.spot("lobby_post", "stand", (3.8, 5.0, 0.0), 180.0 + 180.0)


def _stop(B, z0, tag, top=False):
    """A lift stop on the bridge between the cores at deck height z0: the door in the west core's wall, a bench at the
    railing, a planter, two views, a lamp. Ids <tag>_*."""
    bx0, bx1, by0, by1 = cores_x0 + CORE, cores_x0 + CORE + GAP, 5.6 + 1.5, 5.6 + CORE - 1.5
    _lift_door(B, bx0, 8.1, z0, 90)
    B.thing("lift_%s" % tag, "door", (bx0 + 0.04, 8.1, z0 + 1.1), 90.0, to="lift")
    bn = kit.bench(B, 0.3, by0 + 0.35, z0, 0.0, w=1.2, kind="park")
    s = bn["seats"][0]
    B.thing("%s_bench" % tag, "seat", (s[0], s[1], s[2]), s[3], height=round(s[2] - z0, 2))
    kit.planter(B, bx1 - 0.3, 8.6, z0, 90.0, w=0.7, d=0.4, h=0.45, plants=3, seed=3)
    B.thing("%s_planter" % tag, "board", (bx1 - 0.3, 8.6, z0 + 0.8), 270.0, topic="plant")
    B.thing("%s_view_street" % tag, "board", (0.0, by0 + 0.03, z0 + 1.2), 180.0, topic="view")
    B.thing("%s_view_back" % tag, "board", (0.0, by1 - 0.03, z0 + 1.2), 0.0, topic="view")
    if top:
        box(B.detail, "CT_Steel", bx0, 0.02, 8.05, 8.15, z0 + 2.36, z0 + 2.4)
        box(B.detail, "CT_Steel", -0.02, 0.02, 8.05, 8.15, z0 + 2.38, z0 + 2.52)
    B.lamp((0.0, 8.1, z0 + 2.3), lumens=360, colour="#FFE0B0", kind="stair", radius=4.0)
    return bn


def _stops(B, cores):
    global cores_x0, cores_x1
    cores_x0, cores_x1 = cores[0][0], cores[1][1]
    z7 = LIFT_S7 * STOREY
    _stop(B, z7, "s7")
    B.thing("s7_box", "board", (0.3, 7.6, z7 + 0.62), 180.0, topic="box")
    _reel_box(B, 0.3, 7.6, z7 + 0.45, 12.0, reel=(RESIDENT == "radio"))
    zt = STOREYS * STOREY
    _stop(B, zt, "top", top=True)


def _capsule_more(B):
    """The capsule's second pass (ix0 4.80, ix1 6.92, iy0 2.49, iy1 5.41): tape recorder with tapes on a shelf over the bed
    head, an alarm clock, a fold-down table, hooks, the sill things, a switch."""
    radio = RESIDENT == "radio"
    ix0, ix1, iy0, iy1 = 4.80, 6.92, 2.49, 5.41
    f = lambda fn, x, y, yaw=0.0, z=0.0, **kw: fn(B, x, y, z, yaw, **kw)
    ws = f(kit.wall_shelf, 6.435, iy1, 0.0, w=0.95, zt=1.15, props=False)
    _tape_recorder(B, 6.64, iy1 - 0.16, 1.15, 0.0, reel=radio)
    _tape_boxes(B, 6.25, iy1 - 0.12, 1.15, 0.0, n=4 if radio else 3)
    _alarm_clock(B, 6.1, iy1 - 0.1, 1.15, 0.0)
    B.thing("capsule_tapes", "shelf", (6.62, 5.3, 1.3), 0.0, topic="tapes")
    B.thing("capsule_alarm", "board", (6.1, 5.3, 1.27), 0.0, topic="clock")
    _switch_plate(B, 5.86, iy1, 0.0, 0.0, zc=1.45)
    B.thing("capsule_switch", "machine", (5.86, 5.38, 1.45), 0.0, what="switch", switch=3)
    _fold_table(B, 6.45, iy0, 0.0, 180.0)
    f(kit.teapot, 6.3, 2.75, 0.75, 0.0)
    f(kit.cup, 6.62, 2.8, 0.75, 0.0)
    f(kit.cup, 6.5, 2.95, 0.75, 90.0, mat="CT_PlasterYellow")
    B.thing("capsule_table", "board", (6.45, 2.75, 0.85), 180.0, topic="table")
    _hooks(B, ix0, 5.2, 0.0, 90.0, radio=radio)
    B.thing("capsule_hooks", "board", (ix0 + 0.03, 5.2, 1.5), 90.0, topic="hooks")
    _sill_things(B, 5.04, 2.46, 0.93, radio=radio)
    B.thing("capsule_sill", "board", (5.15, 2.5, 1.02), 180.0, topic="sill")
    B.thing("capsule_phone", "board", (5.0, 2.95, 0.86), 200.0, topic="phone")
    B.spot("capsule_table_stand", "stand", (6.45, 3.3, 0.0), 0.0)


def _deepen(B, lob, cores):
    _lobby_more(B, lob, cores)
    _capsule_more(B)
    _stops(B, cores)
    for w in kit.check(B):
        print("capsule_tower kit.check:", w)
