"""Wave canopy: a free-form undulating white concrete marquee over a fair / promenade.

An original design in the spirit of 1950s Brazilian park marquees (a single thin sinuous slab on slender round
columns that shelters a public walk). ~120 m of canopy (arc length) follows a meandering curve (PARAMETERS
below), 8-12 m wide, a 0.2-0.3 m slab whose underside rolls between 4.2 and 5.4 m and whose edges curl up;
top 4.5 - 6.4 m. Carried by 28 slender round columns (r 0.14) staggered along both sides. Slab: closed top,
underside and edge band (CT_ConcreteWhite). Floor: a paving ribbon (CT_Paving) the length of the canopy.
Underneath: 5 coloured kiosk boxes (4.0 x 2.8 x 2.9 m, hollow, a back door each, a glazed counter hatch as a
"public" window, wooden shelf outside; spots "stall" inside and "counter" in front) and 12 benches ("bench").
The canopy itself is open: people walk and sit anywhere under it. No flats.

Plot 150 x 70 (fairground), max 8 m. The curve lies on y 13..57, x -52..52.

Ideas for later: hanging string lights, bunting, a stage kiosk, fairground rides in the clearings, glazed
roof lights, uplighting of the underside at night (needs an emissive strip material).
Material requests: none (CT_Plaster* for kiosks, CT_ConcreteWhite for the slab).
"""
import math
import random

from citykit import box, DOOR_W, geom
import interior_kit as kit
from _sheet import thick_sheet
import furn_int3 as fu
from furn_int3 import Fr as MFr, lbox, cyl, blob

INFO = {
    "name_ru": "Волнистый навес ярмарки", "name_en": "Wave Canopy",
    "style": "brazilian modernism", "use": "public",
    "inspired_by": "1950s Brazilian park marquees: a free-form concrete slab on slender columns over a promenade - original plan and section",
}

# ---- parameters
X_MIN, X_MAX = -52.0, 52.0
Y_MID, AMP, WAVE_LEN, PHASE = 35.0, 11.0, 78.0, 0.6
HEIGHT0, HEIGHT_AMP, HEIGHT_LEN = 5.0, 0.45, 31.0
N_ARC = 400


def centre_x(x):
    return Y_MID + AMP * math.sin(2 * math.pi * x / WAVE_LEN + PHASE)


def _curve():
    pts = []
    for i in range(N_ARC + 1):
        x = X_MIN + (X_MAX - X_MIN) * i / N_ARC
        pts.append((x, centre_x(x)))
    acc = [0.0]
    for a, b in zip(pts, pts[1:]):
        acc.append(acc[-1] + math.dist(a, b))
    return pts, acc


PTS, ACC = _curve()
LENGTH = ACC[-1]


def at(s):
    """(x, y, tx, ty) of the centreline at arc length s."""
    s = min(max(s, 0.0), LENGTH)
    lo, hi = 0, N_ARC
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if ACC[mid] <= s:
            lo = mid
        else:
            hi = mid
    f = (s - ACC[lo]) / ((ACC[hi] - ACC[lo]) or 1)
    a, b = PTS[lo], PTS[hi]
    dx, dy = b[0] - a[0], b[1] - a[1]
    d = math.hypot(dx, dy)
    return a[0] + dx * f, a[1] + dy * f, dx / d, dy / d


def half_width(s):
    return 5.0 + 1.0 * math.sin(2 * math.pi * s / 37.0 + 2.0) + 0.4 * math.sin(2 * math.pi * s / 17.0)


def z_top(s, t):
    return HEIGHT0 + HEIGHT_AMP * math.sin(2 * math.pi * s / HEIGHT_LEN + 1.0) + 0.3 * math.sin(2 * math.pi * s / 53.0) + 0.55 * t * t


def thick(s, t):
    return 0.30 - 0.11 * t ** 4


def surf(s, t):
    x, y, tx, ty = at(s)
    h = half_width(s)
    return x - ty * t * h, y + tx * t * h


class Fr:
    """A local frame: local (lx, ly) -> world; ey = the local +y direction."""
    def __init__(self, cx, cy, ex_x, ex_y):
        self.c = (cx, cy)
        self.ex = (ex_x, ex_y)
        self.ey = (-ex_y, ex_x)

    def p(self, lx, ly):
        return (self.c[0] + self.ex[0] * lx + self.ey[0] * ly, self.c[1] + self.ex[1] * lx + self.ey[1] * ly)

    def yaw(self, dx, dy):                 # yaw (deg) of a world direction vector, 0 = -y, 90 = +x
        return math.degrees(math.atan2(dx, -dy))

    def yaw_dir(self, lx, ly):
        return self.yaw(self.ex[0] * lx + self.ey[0] * ly, self.ex[1] * lx + self.ey[1] * ly)


def rect(fr, x0, x1, y0, y1):
    return [fr.p(x0, y0), fr.p(x1, y0), fr.p(x1, y1), fr.p(x0, y1)]


def _w3(fr, lx, ly, z):
    x, y = fr.p(lx, ly)
    return (x, y, z)


def _face_dir(m, mat, pts, want):
    """A quad whose normal points along `want` (a 3-vector)."""
    n = geom.cross(geom.sub(pts[1], pts[0]), geom.sub(pts[3], pts[0]))
    if sum(n[i] * want[i] for i in range(3)) < 0:
        pts = pts[::-1]
    m.face(mat, pts)


def awning(B, fr, mats, y0=-1.42, reach=0.95, z_top=2.52, z_out=2.18, hw=2.05, n=10, drop=0.17):
    """A striped shop awning over the serving hatch: slanted strips, a valance at the front edge, plain underside."""
    d = B.detail
    for i in range(n):
        x0 = -hw + 2 * hw * i / n
        x1 = x0 + 2 * hw / n
        mat = mats[i % 2]
        top = [_w3(fr, x0, y0, z_top), _w3(fr, x1, y0, z_top), _w3(fr, x1, y0 - reach, z_out), _w3(fr, x0, y0 - reach, z_out)]
        _face_dir(d, mat, top, (0.0, 0.0, 1.0))
        under = [(p[0], p[1], p[2] - 0.012) for p in top]
        _face_dir(d, "CT_PlasterGrey", under, (0.0, 0.0, -1.0))
        fy = y0 - reach
        val = [_w3(fr, x0, fy, z_out), _w3(fr, x1, fy, z_out), _w3(fr, x1, fy, z_out - drop), _w3(fr, x0, fy, z_out - drop)]
        out = fr.p(0, fy - 1.0)
        c0 = fr.p(0, fy)
        _face_dir(d, mat, val, (out[0] - c0[0], out[1] - c0[1], 0.0))
        val_b = [(p[0], p[1], p[2]) for p in val]
        out_in = (c0[0] - out[0], c0[1] - out[1], 0.0)
        _face_dir(d, "CT_PlasterGrey", [(p[0] - 0.004 * (out[0] - c0[0]), p[1] - 0.004 * (out[1] - c0[1]), p[2]) for p in val_b], out_in)
    # two thin tie rods from the roof slab to the awning front corners
    for sx in (-hw + 0.05, hw - 0.05):
        a = _w3(fr, sx, y0 - reach + 0.02, z_out - 0.02)
        b = _w3(fr, sx, y0 - 0.02, z_top + 0.35)
        fu.rod(d, "CT_Steel", a, b, 0.012, caps=False)


def bench(B, s, t, k):
    x, y, tx, ty = at(s)
    h = half_width(s)
    cx, cy = surf(s, t)
    fr = Fr(cx, cy, tx, ty)
    face = -1 if t > 0 else 1                      # sit facing the walk's centre line
    yaw = fr.yaw_dir(0, face)
    bn = kit.bench(B, cx, cy, 0.0, yaw, w=1.6, kind="park")
    px, py = fr.p(0, 0.5 * (-face))
    B.spot("bench%d" % k, "bench", (px, py, 0.0), yaw)
    B.thing("fair_bench_%d" % k, "seat", (cx, cy, 0.45), yaw, height=0.45)
    # a litter bin beside every second bench
    if k % 2 == 0:
        bx, by = fr.p(1.15, 0.0)
        cyl(B.detail, "CT_SteelRed", bx, by, 0.0, 0.62, 0.17, 8, top=False)
        cyl(B.detail, "CT_ConcreteDark", bx, by, 0.62, 0.65, 0.19, 8)


KIOSK_MATS = ["CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterPink", "CT_PlasterBlue"]
KIOSK_TRADE = ["coffee", "icecream", "press", "toys", "snacks"]
KIOSK_SIGN = {"coffee": "sign_buffet", "icecream": "sign_milk", "press": "sign_books", "toys": "sign_toys",
              "snacks": "sign_bread"}


def kiosk(B, s, side, k):
    x, y, tx, ty = at(s)
    h = half_width(s)
    off = side * (h - 2.4)
    cx, cy = x - ty * off, y + tx * off
    # local +y = away from the centre line
    ex = (tx, ty) if side > 0 else (-tx, -ty)
    fr = Fr(cx, cy, ex[0], ex[1])
    hw, hd, H = 2.0, 1.4, 2.9
    mat = KIOSK_MATS[k % len(KIOSK_MATS)]
    corners = [(-hw, -hd), (hw, -hd), (hw, hd), (-hw, hd)]     # CCW; edge0 = front (faces local -y)
    wp = [fr.p(*c) for c in corners]
    holes = [[(0.35, 3.65, 1.0, 2.45)], [], [(1.4, 1.4 + DOOR_W + 0.1, 0.0, 2.25)], []]
    F = {}
    for i in range(4):
        F[i] = B.facade(wp[i], wp[(i + 1) % 4], 0.0, H, mat, holes=holes[i], depth=0.2)
    # the serving hatch is left open (no glass): the stall-holder and the goods show from the walk
    # floor and roof
    B.slab([fr.p(-hw + 0.1, -hd + 0.1), fr.p(hw - 0.1, -hd + 0.1), fr.p(hw - 0.1, hd - 0.1), fr.p(-hw + 0.1, hd - 0.1)],
           0.0, 0.05, "CT_Floor")
    B.slab(rect(fr, -hw - 0.35, hw + 0.35, -hd - 0.45, hd + 0.35), H, H + 0.22, "CT_ConcreteWhite")
    # shelf outside the hatch
    B.slab(rect(fr, -1.7, 1.7, -hd - 0.45, -hd), 1.0, 1.06, "CT_Wood")
    for u in (-1.5, 1.5):
        B.slab(rect(fr, u - 0.04, u + 0.04, -hd - 0.4, -hd - 0.3), 0.0, 1.0, "CT_Steel")
    # door of the stall-holder (back wall, faces local +y)
    dx, dy = fr.ey
    doorp = fr.p(0.0 + (hw - 1.4 - (DOOR_W + 0.1) / 2), hd)
    B.entrance("kiosk%d" % k, (doorp[0], doorp[1], 0.0), fr.yaw(dx, dy), width=DOOR_W, kind="service")
    ip = fr.p(0, -0.2)
    B.spot("stall%d" % k, "stall", (ip[0], ip[1], 0.05), fr.yaw_dir(0, -1))
    cp = fr.p(0, -hd - 1.3)
    B.spot("counter%d" % k, "counter", (cp[0], cp[1], 0.0), fr.yaw_dir(0, 1))
    kiosk_interior(B, fr, k, random.Random(1960 + k))
    kiosk_outside(B, fr, k, mat)


def kiosk_outside(B, fr, k, mat):
    """The street side of a kiosk: sign over the hatch, striped awning, goods on the shelf, a board on the side wall."""
    trade = KIOSK_TRADE[(k - 1) % len(KIOSK_TRADE)]
    front = fr.yaw_dir(0, -1)
    sx, sy = fr.p(0, -1.4)
    kit.sign_plate(B, sx, sy, 0.0, front, name=KIOSK_SIGN[trade], w=1.9, zc=2.69)
    awning(B, fr, (mat, "CT_TileWhite"))
    top = 1.06
    # goods on the outside shelf (z = 1.06), each trade its own
    def at_(lx, ly=-1.62):
        return fr.p(lx, ly)
    rnd = random.Random(300 + k)
    if trade == "coffee":
        kit.glass(B, *at_(-1.3), top)
        for lx in (-0.9, -0.5, 0.45, 0.85):
            kit.cup(B, *at_(lx), top, yaw=rnd.uniform(0, 90))
        kit.teapot(B, *at_(1.3), top, yaw=front)
        kit.plates(B, *at_(0.0, -1.62), top, n=3)
    elif trade == "icecream":
        for lx in (-1.2, -0.7, -0.2):
            kit.glass(B, *at_(lx), top)
        kit.cup(B, *at_(0.5), top, mat="CT_PlasterPink")
        kit.cup(B, *at_(0.9), top, mat="CT_PlasterYellow")
        kit.bottles(B, *at_(1.35), top, n=2, seed=k)
        B.thing("ring_toss", "play", (*at_(1.35), top + 0.15), front, what="ring_toss")       # rings over the bottles
    elif trade == "press":
        kit.newspaper(B, *at_(-1.1), top, yaw=front + 8, seed=1)
        kit.newspaper(B, *at_(-0.4), top, yaw=front - 6, seed=2)
        kit.books_stack(B, *at_(0.5), top, yaw=front, n=4, seed=3)
        B.thing("lottery_tickets", "play", (*at_(0.5), top + 0.1), front, what="lottery")      # the lottery tickets
        kit.newspaper(B, *at_(1.1), top, yaw=front + 12, seed=4)
        B.thing("press_paper", "take", (*at_(-0.75), top + 0.03), front, item="daily_paper")
    elif trade == "toys":
        kit.globe(B, *at_(-1.2), top, yaw=front)
        B.thing("fortune_globe", "machine", (*at_(-1.2), top + 0.2), front, what="fortune")    # the fortune-telling globe
        kit.books_stack(B, *at_(-0.2), top, yaw=front + 90, n=3, seed=5)
        kit.vase(B, *at_(0.7), top, seed=6)
        kit.cup(B, *at_(1.25), top, mat="CT_PlasterBlue")
    else:
        kit.crate(B, *at_(-1.15), top, yaw=front, fill="CT_PlasterRed", seed=1)
        kit.crate(B, *at_(-0.5), top, yaw=front, fill="CT_PlasterYellow", seed=2)
        kit.kitchen_scales(B, *at_(0.5), top, yaw=front)
        kit.sack(B, *at_(1.2), top)
    # menu board on the side wall facing out along the walk, a clock over the door side
    side_ = 1 if k % 2 else -1
    mx, my = fr.p(side_ * 2.0, -0.35)
    kit.menu_board(B, mx, my, 0.0, fr.yaw_dir(side_, 0), name=("menu_0", "price_0", "menu_1", "price_1", "menu_0")[k - 1],
                   w=0.62, zc=1.65)
    B.thing("kiosk%d_menu" % k, "board", (mx, my, 1.65), fr.yaw_dir(side_, 0), topic="prices")
    # outdoor extras by trade
    if trade == "coffee":
        for sgn in (-1, 1):
            tx_, ty_ = fr.p(sgn * 2.6, -3.1)
            tt = kit.table_round(B, tx_, ty_, 0.0, r=0.36, h=1.0)
            kit.cup(B, tx_, ty_, tt["top"], yaw=sgn * 40)
            for a in (0.0, 180.0):
                ox, oy = fr.p(sgn * 2.6 + (0.6 if a == 0 else -0.6), -3.1)
                st = kit.bar_stool(B, ox, oy, 0.0, 0.0, h=0.72)
            B.thing("coffee_stool_%d" % (sgn + 2), "seat", (*fr.p(sgn * 2.6 + 0.6, -3.1), 0.72), front, height=0.72)
    elif trade == "press":
        for sgn in (-1, 1):
            rx, ry = fr.p(sgn * 2.0 + (-sgn) * 0.0, -0.6)
        mxr, myr = fr.p(-side_ * 2.0, -0.7)
        kit.magazine_rack(B, mxr, myr, 0.0, fr.yaw_dir(-side_, 0), w=0.9, seed=k)
        B.thing("press_rack", "shelf", (mxr, myr, 1.2), fr.yaw_dir(-side_, 0), topic="travel")
    elif trade == "toys":
        dc = kit.display_case(B, *fr.p(-side_ * 2.7, -0.6), 0.0, fr.yaw_dir(-side_, 0), w=1.0, d=0.5, goods="toys", seed=k)
        B.thing("shooting_gallery", "play", (*fr.p(-side_ * 2.7, -0.6), 0.9), fr.yaw_dir(-side_, 0),
                what="shooting_gallery")                                                        # the prizes' case
    elif trade == "snacks":
        kit.crate(B, *fr.p(2.55, -0.6), 0.0, fr.yaw_dir(1, 0), fill="CT_PlasterGreen", seed=7)
        kit.crate(B, *fr.p(2.55, -1.2), 0.0, fr.yaw_dir(1, 0), fill="CT_PlasterYellow", seed=8)
        kit.cardboard_box(B, *fr.p(2.55, -0.6), 0.38, fr.yaw_dir(1, 0), seed=9)
        B.thing("strongman", "sport", (*fr.p(2.55, -0.9), 0.6), fr.yaw_dir(1, 0), what="strongman")  # by the crates
    # the hatch counter thing: where the vendor serves
    hx, hy = fr.p(0, -1.55)
    B.thing("kiosk%d_counter" % k, "counter", (hx, hy, 1.1), front, role="vendor", service="shop", trade=trade)


def kiosk_interior(B, fr, k, rnd):
    """Furnish a kiosk for its trade (floor z 0.05, inner space x +-1.8, y -1.2..1.2; hatch toward local -y, door in the
    back wall near x 0): a counter under the hatch, shelving on the back and side walls, a rug and a stool."""
    z0 = 0.05
    trade = KIOSK_TRADE[(k - 1) % len(KIOSK_TRADE)]
    toward_hatch = fr.yaw_dir(0, -1)
    into_room = fr.yaw_dir(0, 1)
    goods = {"coffee": "jars", "icecream": "boxes", "press": None, "toys": "toys", "snacks": "groceries"}[trade]
    # counter under the hatch (customer side toward the hatch)
    cx_, cy_ = fr.p(0, -0.88)
    cn = kit.shop_counter(B, cx_, cy_, z0, toward_hatch, w=3.4, d=0.65, front=KIOSK_MATS[k % 5], till=False)
    top = cn["top"]
    stand_x, stand_y = fr.p(-0.3, 0.05)
    # trade-specific counter dressing (z = counter top)
    if trade == "coffee":
        kit.samovar(B, *fr.p(-1.2, -0.85), top, yaw=toward_hatch)
        kit.teapot(B, *fr.p(-0.55, -0.85), top, yaw=toward_hatch)
        kit.cup(B, *fr.p(0.1, -0.95), top)
        kit.cup(B, *fr.p(0.35, -0.8), top, mat="CT_PlasterRed")
        kit.plates(B, *fr.p(0.95, -0.9), top, n=3)
        kit.cash_register(B, *fr.p(1.35, -0.8), top, toward_hatch + 180)
    elif trade == "icecream":
        for lx in (-1.3, -0.9, -0.5):
            kit.glass(B, *fr.p(lx, -0.9), top)
        kit.cup(B, *fr.p(0.1, -0.9), top, mat="CT_PlasterPink")
        kit.cup(B, *fr.p(0.5, -0.8), top, mat="CT_PlasterYellow")
        kit.cash_register(B, *fr.p(1.3, -0.8), top, toward_hatch + 180)
    elif trade == "press":
        for j, lx in enumerate((-1.3, -0.85, -0.4, 0.15)):
            kit.newspaper(B, *fr.p(lx, -0.85 + 0.03 * j), top, yaw=toward_hatch + 8 * j, seed=j)
        kit.books_stack(B, *fr.p(0.8, -0.9), top, yaw=toward_hatch, n=3, seed=k)
        kit.cash_register(B, *fr.p(1.35, -0.8), top, toward_hatch + 180)
    elif trade == "toys":
        kit.globe(B, *fr.p(-1.2, -0.85), top)
        kit.vase(B, *fr.p(-0.4, -0.85), top, seed=2)
        kit.books_stack(B, *fr.p(0.4, -0.9), top, yaw=toward_hatch + 20, n=3, seed=2)
        kit.cash_register(B, *fr.p(1.3, -0.8), top, toward_hatch + 180)
    else:
        kit.kitchen_scales(B, *fr.p(-1.1, -0.85), top, yaw=toward_hatch)
        kit.bottles(B, *fr.p(-0.4, -0.9), top, n=3, seed=k)
        kit.crate(B, *fr.p(0.5, -0.9), top, yaw=toward_hatch, fill="CT_PlasterRed", seed=k)
        kit.cash_register(B, *fr.p(1.35, -0.8), top, toward_hatch + 180)
    # shelving: back wall either side of the door, both side walls
    for lx in (-1.2, 1.2):
        bx, by = fr.p(lx, 1.2)
        if trade == "press":
            kit.bookshelf(B, bx, by, z0, toward_hatch, w=1.1, h=2.0, seed=lx, topic="library")
        else:
            kit.shop_shelf(B, bx, by, z0, toward_hatch, w=1.1, h=2.0, goods=goods, seed=k + int(lx * 5))
    for sgn in (-1, 1):
        bx, by = fr.p(sgn * 1.8, 0.1)
        yaw_in = fr.yaw_dir(-sgn, 0)
        if trade == "icecream" and sgn < 0:
            kit.kitchen_unit(B, bx, by, z0, yaw_in, modules="fridge,fridge", wall_cupboards=False)
        elif trade == "press":
            kit.magazine_rack(B, bx, by, z0, yaw_in, w=0.8, seed=k + sgn)
        elif trade == "toys" and sgn > 0:
            kit.display_case(B, bx - 0.0, by, z0, yaw_in, w=1.0, d=0.45, goods="toys", seed=k)
        else:
            kit.shop_shelf(B, bx, by, z0, yaw_in, w=0.9, h=1.9, goods=goods or "boxes", seed=k + 9 * sgn)
    # a rug and a stool for the stall-holder, a clock and a poster
    rx, ry = fr.p(0.2, 0.15)
    kit.rug(B, rx, ry, z0, toward_hatch, w=1.1, d=1.5, seed=k)
    kit.stool(B, *fr.p(-0.95, -0.05), z0, toward_hatch, mat="CT_PlasterBlue", h=0.55)
    kx, ky = fr.p(0.0, -1.2)
    kit.wall_clock(B, *fr.p(1.2, 1.2), z0, toward_hatch, zc=2.35)
    kit.poster(B, *fr.p(-1.2, 1.2), z0, toward_hatch, name=("poster_sun", "kids", "poster_cons", "kids", "poster_birds")[k - 1],
               w=0.5, zc=2.5)
    # lamp, a pendant under the roof slab (kiosk H = 2.9)
    lx, ly, _ = MFr(fr.c[0], fr.c[1], math.degrees(math.atan2(fr.ex[1], fr.ex[0])), z0).p(0.0, -0.1)
    B.lamp((lx, ly, 2.6), lumens=520, colour="#FFD2A0", kind="public", radius=5.5)
    lbox(B.detail, "CT_Steel", MFr(lx, ly), -0.01, 0.01, -0.01, 0.01, 2.68, 2.9, faces="xXyY")


def fair_board(B, s, t, tid, topic, posters=False):
    """A free-standing board on two wooden posts beside the walk; faces the centre line, both sides dressed."""
    cx, cy = surf(s, t)
    x, y, tx, ty = at(s)
    fr = Fr(cx, cy, tx, ty)
    face = -1 if t > 0 else 1
    yaw = fr.yaw_dir(0, face)
    for u in (-0.95, 0.95):
        B.slab(rect(fr, u - 0.06, u + 0.06, -0.06, 0.06), 0.0, 2.5, "CT_Wood")
    B.slab(rect(fr, -1.1, 1.1, -0.04, 0.04), 0.95, 2.4, "CT_WoodLight")
    B.slab(rect(fr, -1.15, 1.15, -0.07, 0.07), 2.4, 2.46, "CT_Wood")
    fx, fy = fr.p(0, face * 0.04)
    bx, by = fr.p(0, -face * 0.04)
    if posters:
        for u, nm, w_ in ((-0.55, "cinema", 0.62), (0.15, "poster_sun", 0.6), (0.78, "travel", 0.5)):
            px, py = fr.p(u, face * 0.04)
            kit.poster(B, px, py, 0.0, yaw, name=nm, w=w_, zc=1.62, seed=int(u * 10))
        kit.sign_plate(B, fx, fy, 0.0, yaw, name="sign_cinema", w=1.0, zc=2.2)
        kit.notice_board(B, bx, by, 0.0, yaw + 180, w=1.7, zc=1.65, seed=5)
    else:
        kit.notice_board(B, fx, fy, 0.0, yaw, w=1.7, zc=1.65, seed=3)
        kit.poster(B, bx, by, 0.0, yaw + 180, name="poster_cons", w=0.6, zc=1.65, seed=2)
        kit.poster(B, *fr.p(0.65, -face * 0.04), 0.0, yaw + 180, name="kids", w=0.5, zc=1.65, seed=3)
    B.thing(tid, "board", (fx, fy, 1.65), yaw, topic=topic)


def string_lights(B, rnd):
    """Festoons of bulbs under the canopy slab along two lines (t = +-0.32): lamps (kind lobby) every ~8 m, and a
    sagging cable with small CT_Neon beads between the supports every 3.5 m."""
    for li, t in enumerate((0.32, -0.32)):
        s = 3.0 + 4.0 * li
        k = 0
        while s < LENGTH - 3.0:
            x, y = surf(s, t)
            zu = z_top(s, t) - thick(s, t) - 0.04
            pts = []
            # a catenary from here to the next support (3.5 m along s)
            n_seg = 6
            ds = 3.5
            for i in range(n_seg + 1):
                ss = s + ds * i / n_seg
                if ss > LENGTH - 2.0:
                    break
                px, py = surf(ss, t)
                sag = 0.34 * 4 * (i / n_seg) * (1 - i / n_seg)
                pts.append((px, py, z_top(ss, t) - thick(ss, t) - 0.06 - sag))
            for a, b in zip(pts, pts[1:]):
                fu.rod(B.detail, "CT_Steel", a, b, 0.012, caps=False)
            for (px, py, pz) in pts[1:-1] if len(pts) > 2 else []:
                blob(B.detail, "CT_Opal", px, py, pz - 0.06, 0.05, 5)
            if k % 2 == 0 and pts:
                px, py, pz = pts[len(pts) // 2]
                B.lamp((px, py, pz - 0.12), lumens=170, colour="#FFD9A0", kind="lobby", radius=6.0, fixture=False)
                blob(B.detail, "CT_Opal", px, py, pz - 0.12, 0.09, 6)
            s += ds
            k += 1


# ======================================================================================================== fair games
FLAGS = ["CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterBlue", "CT_PlasterPink", "CT_PlasterGreen", "CT_TileWhite"]
LAMP_GAMES = []


def _gframe(s, t, face):
    """A game's frame: gf.p(u, w, z), u along the walk, w toward the customers (face +1: toward the walk's +t side)."""
    cx, cy = surf(s, t)
    x, y, tx, ty = at(s)
    fr = Fr(cx, cy, tx, ty)
    yaw = fr.yaw_dir(0, face)
    return MFr(cx, cy, yaw), yaw


def _wp(gf, u, w, z=0.0):
    return gf.p(u, w, z)


def _bar(m, mat, gf, a, b, wd=0.03, caps=True):
    fu.rod(m, mat, gf.p(*a), gf.p(*b), wd, caps=caps)


def _tri(m, mat, a, b, c, both=True):
    n = geom.cross(geom.sub(b, a), geom.sub(c, a))
    m.face(mat, [a, b, c])
    if both:
        m.face(mat, [a, c, b])


def _valance(B, gf, hw, w, z0, z1, n, mats=("CT_PlasterRed", "CT_TileWhite")):
    """Striped valance strips along u at the front plane w."""
    for i in range(n):
        u0 = -hw + 2 * hw * i / n
        u1 = u0 + 2 * hw / n
        lbox(B.detail, mats[i % len(mats)], gf, u0, u1, w, w + 0.035, z0, z1)


def _roof(B, gf, hw, w0, w1, z, mat="CT_ConcreteWhite"):
    lbox(B.detail, mat, gf, -hw, hw, w0, w1, z, z + 0.07)


def _parasol(B, gf, u, w, zt, r=1.15, zp=2.95, n=8, mats=("CT_PlasterRed", "CT_TileWhite")):
    """A striped parasol: a pole and n triangular gores."""
    _bar(B.detail, "CT_Wood", gf, (u, w, 0.0), (u, w, zp + 0.1), 0.05)
    for i in range(n):
        a0, a1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1) / n
        pa = gf.p(u + r * math.cos(a0), w + r * math.sin(a0), zt)
        pb = gf.p(u + r * math.cos(a1), w + r * math.sin(a1), zt)
        top = gf.p(u, w, zp)
        mat = mats[i % 2]
        # upper face is seen from the side: both sides drawn
        _face_dir(B.detail, mat, [pa, pb, top], (0.0, 0.0, 1.0)) if False else None
        B.detail.face(mat, [pa, pb, top])
        B.detail.face(mat, [pb, pa, top])


def _ring(d, mat, gf, u, w, z, r=0.085, segs=6, wd=0.016):
    pts = [gf.p(u + r * math.cos(2 * math.pi * k / segs), w + r * math.sin(2 * math.pi * k / segs), z) for k in range(segs)]
    for a, b in zip(pts, pts[1:] + pts[:1]):
        fu.rod(d, mat, a, b, wd, caps=False)


def _stand(B, gf, sid, u, w, face_back=True, kind="stand"):
    x, y, _ = gf.p(u, w)
    B.spot(sid, kind, (x, y, 0.0), gf.yaw + (180 if face_back else 0))


def bunting_line(B, s, t0=0.7, t1=-0.7, sag=0.8, n=7, seed=0):
    x0, y0 = surf(s, t0)
    x1, y1 = surf(s, t1)
    za = z_top(s, t0) - thick(s, t0) - 0.14
    zb = z_top(s, t1) - thick(s, t1) - 0.14
    segs = 6
    pts = []
    for i in range(segs + 1):
        f = i / segs
        tt = t0 + (t1 - t0) * f
        px, py = surf(s, tt)
        pz = za + (zb - za) * f - sag * 4 * f * (1 - f)
        pts.append((px, py, pz))
    for a, b in zip(pts, pts[1:]):
        fu.rod(B.detail, "CT_Steel", a, b, 0.01, caps=False)
    for (px, py, pz), zz in ((pts[0], za + 0.14), (pts[-1], zb + 0.14)):
        fu.rod(B.detail, "CT_Steel", (px, py, pz), (px, py, zz), 0.012, caps=False)
    dx, dy = x1 - x0, y1 - y0
    L = math.hypot(dx, dy)
    ux, uy = dx / L, dy / L
    for j in range(n):
        f = (j + 0.6) / n
        i = min(int(f * segs), segs - 1)
        g = f * segs - i
        px = pts[i][0] + (pts[i + 1][0] - pts[i][0]) * g
        py = pts[i][1] + (pts[i + 1][1] - pts[i][1]) * g
        pz = pts[i][2] + (pts[i + 1][2] - pts[i][2]) * g - 0.02
        hw_ = 0.1
        a = (px - ux * hw_, py - uy * hw_, pz)
        b = (px + ux * hw_, py + uy * hw_, pz)
        c = (px, py, pz - 0.28)
        _tri(B.detail, FLAGS[(j + seed) % len(FLAGS)], a, b, c)


def strongman_tower(B, s=26.0, t=-0.4, face=1):
    gf, yaw = _gframe(s, t, face)
    d = B.detail
    lbox(B.m, "CT_Wood", gf, -0.8, 0.8, -0.8, 0.8, 0.0, 0.2)                       # plinth (solid)
    lbox(B.m, "CT_PlasterRed", gf, -0.34, 0.34, -0.30, -0.20, 0.2, 3.5)              # backboard (solid)
    for sg in (-1, 1):
        lbox(B.m, "CT_Wood", gf, sg * 0.34 - 0.04 + 0.04 * sg, sg * 0.34 + 0.04 + 0.04 * sg, -0.30, 0.0, 0.2, 3.5)
    lbox(d, "CT_PlasterYellow", gf, -0.5, 0.5, -0.4, 0.2, 3.5, 3.62)                  # cap
    lbox(d, "CT_PlasterRed", gf, -0.42, 0.42, -0.34, 0.14, 3.62, 3.7)
    _bar(d, "CT_Steel", gf, (0, -0.19, 0.3), (0, -0.19, 3.45), 0.014)                 # the puck's wire
    lbox(d, "CT_SteelRed", gf, -0.07, 0.07, -0.24, -0.12, 0.26, 0.38)                 # the puck, at rest
    for k in range(13):                                                              # scale
        z = 0.6 + 0.2 * k
        lbox(d, "CT_TileWhite", gf, 0.12 if k % 2 == 0 else 0.17, 0.3, -0.2, -0.192, z, z + 0.025)
    lbox(d, "CT_Brass", gf, -0.3, -0.12, -0.2, -0.185, 2.62, 2.7)                     # the record clip
    blob(d, "CT_Brass", *gf.p(0, -0.1, 3.33)[:2], gf.p(0, 0, 3.33)[2], 0.13, 6)       # the bell
    _bar(d, "CT_Steel", gf, (0, -0.1, 3.5), (0, -0.1, 3.42), 0.02)
    _bar(d, "CT_Steel", gf, (0, -0.1, 3.7), (0, -0.1, 4.15), 0.025)                   # flag pole
    _tri(d, "CT_PlasterRed", gf.p(0, -0.1, 4.15), gf.p(0, -0.1, 3.9), gf.p(0.5, -0.1, 4.03))
    for sg in (-1, 1):
        for k in range(8):
            x, y, z = gf.p(sg * 0.4, 0.04, 0.5 + 0.38 * k)
            blob(d, "CT_Opal", x, y, z, 0.05, 4)
    lbox(d, "CT_Leather", gf, -0.38, 0.38, 0.0, 0.55, 0.2, 0.3)                        # the pad
    lbox(d, "CT_Steel", gf, -0.04, 0.04, -0.15, 0.55, 0.3, 0.33)
    # the mallet, lying on the plinth
    _bar(d, "CT_Wood", gf, (0.56, 0.7, 0.24), (0.56, 0.06, 0.24), 0.045)
    lbox(d, "CT_Wood", gf, 0.42, 0.7, -0.1, 0.06, 0.2, 0.38)
    lbox(d, "CT_Steel", gf, 0.42, 0.7, -0.105, -0.095, 0.2, 0.38)
    lbox(d, "CT_Steel", gf, 0.42, 0.7, 0.055, 0.065, 0.2, 0.38)
    lbox(d, "CT_Brass", gf, -0.34, 0.34, 0.8, 0.82, 0.05, 0.16)                       # the record plaque
    B.lamp(gf.p(0, 0.5, 3.9), lumens=200, colour="#FFD2A0", kind="lobby", radius=6.0, fixture=False)
    B.thing("strongman_tower", "sport", gf.p(0, 0.2, 1.2), yaw, what="strongman")
    B.thing("strongman_record", "board", gf.p(0, 0.85, 0.55), yaw, topic="logbook")
    _stand(B, gf, "strongman_stand", 0.0, 1.5)
    _stand(B, gf, "strongman_stand_2", 1.3, 1.3)
    return gf


def shooting_booth(B, s=73.0, t=0.0, face=1):
    gf, yaw = _gframe(s, t, face)
    d = B.detail
    lbox(B.m, "CT_PlasterRed", gf, -1.6, 1.6, 0.62, 0.95, 0.0, 0.95)                  # counter (solid)
    lbox(d, "CT_Wood", gf, -1.68, 1.68, 0.55, 1.05, 0.95, 1.0)
    for sg in (-1, 1):                                                               # side walls (solid)
        lbox(B.m, "CT_PlasterBlue", gf, sg * 1.62 - 0.05, sg * 1.62 + 0.05, -0.85, 0.7, 0.0, 2.6)
    lbox(B.m, "CT_PlasterYellow", gf, -1.67, 1.67, -0.9, -0.8, 0.0, 2.6)              # back wall (solid)
    lbox(d, "CT_PlasterBlue", gf, -1.5, 1.5, -0.8, -0.78, 0.2, 2.45)                       # painted sky behind the targets
    for k in range(4):                                                                # clouds on it
        lbox(d, "CT_TileWhite", gf, -1.2 + 0.7 * k, -0.8 + 0.7 * k, -0.782, -0.775, 2.1 + 0.08 * (k % 2), 2.2 + 0.08 * (k % 2))
    _roof(B, gf, 1.9, -1.0, 1.2, 2.6)
    _valance(B, gf, 1.9, 1.17, 2.2, 2.6, 10)
    kit.sign_plate(B, *gf.p(0, 1.22)[:2], 0.0, yaw, name="sign_toys", w=1.7, zc=2.85)
    for sg in (-1, 1):
        lbox(d, "CT_Wood", gf, sg * 1.7 - 0.04, sg * 1.7 + 0.04, 1.0, 1.08, 0.0, 2.6)   # front posts
    # the chain of targets: a rail high at the back, ducks hanging from it
    _bar(d, "CT_Steel", gf, (-1.5, -0.55, 2.0), (1.5, -0.55, 2.0), 0.03)
    for k in range(7):
        u = -1.35 + 0.45 * k
        _bar(d, "CT_Steel", gf, (u, -0.55, 2.0), (u, -0.55, 1.78), 0.012)
        mat = ("CT_PlasterYellow", "CT_PlasterRed", "CT_PlasterBlue")[k % 3]
        lbox(d, mat, gf, u - 0.12, u + 0.12, -0.57, -0.53, 1.58, 1.78)
        blob(d, mat, *gf.p(u + 0.09, -0.55, 1.88)[:2], gf.p(0, 0, 1.88)[2], 0.06, 5)
    # a second row: bullseyes on a lower chain
    for k in range(5):
        u = -1.1 + 0.55 * k
        lbox(d, "CT_SteelRed", gf, u - 0.15, u + 0.15, -0.69, -0.66, 0.95, 1.25)
        lbox(d, "CT_TileWhite", gf, u - 0.1, u + 0.1, -0.695, -0.665, 1.0, 1.2)
        lbox(d, "CT_SteelRed", gf, u - 0.045, u + 0.045, -0.7, -0.67, 1.075, 1.125)
    # a little windmill target in the corner and the bell to ring
    _bar(d, "CT_Steel", gf, (1.25, -0.5, 0.95), (1.25, -0.5, 1.5), 0.03)
    for a in (0, 90):
        ca, sa = math.cos(math.radians(a + 25)), math.sin(math.radians(a + 25))
        lbox(d, "CT_PlasterRed", gf, 1.25 - 0.22 * ca - 0.03, 1.25 + 0.22 * ca + 0.03, -0.54, -0.5, 1.5 - 0.22 * sa - 0.03, 1.5 + 0.22 * sa + 0.03) if a == 0 else \
            lbox(d, "CT_PlasterYellow", gf, 1.25 - 0.03, 1.25 + 0.03, -0.54, -0.5, 1.28, 1.72)
    blob(d, "CT_Brass", *gf.p(-1.35, -0.5)[:2], gf.p(0, 0, 1.1)[2], 0.08, 6)
    # two rifles chained to the counter
    for u in (-0.55, 0.55):
        _bar(d, "CT_Steel", gf, (u, 0.55, 1.06), (u, -0.1, 1.12), 0.025)                # barrel
        lbox(d, "CT_Wood", gf, u - 0.04, u + 0.04, 0.5, 0.95, 1.0, 1.1)               # stock
        _bar(d, "CT_Steel", gf, (u, 0.85, 1.0), (u + 0.18, 0.88, 0.88), 0.008, caps=False)
        _bar(d, "CT_Steel", gf, (u + 0.18, 0.88, 0.88), (u + 0.32, 0.95, 1.18), 0.008, caps=False)
        lbox(d, "CT_Steel", gf, u + 0.3, u + 0.34, 0.93, 0.97, 1.0, 1.22)
    lbox(d, "CT_PlasterRed", gf, -0.15, 0.15, 0.7, 0.9, 1.0, 1.04)                     # pellets tin
    kit.shop_shelf(B, *gf.p(-1.57, 0.0)[:2], 0.0, yaw + 90 * (1 if True else 1), w=1.1, h=1.9, goods="toys", seed=3)
    B.lamp(gf.p(0, 0.2, 2.45), lumens=320, colour="#FFD2A0", kind="public", radius=6.0)
    B.thing("shooting_booth", "play", gf.p(0, 0.8, 1.15), yaw, what="shooting_gallery")
    _stand(B, gf, "gallery_stand", -0.5, 1.5)
    _stand(B, gf, "gallery_stand_2", 0.7, 1.6)
    return gf


def ring_toss_table(B, s=34.0, t=0.0, face=1):
    gf, yaw = _gframe(s, t, face)
    d = B.detail
    lbox(B.m, "CT_PlasterBlue", gf, -1.15, 1.15, 0.0, 0.85, 0.0, 0.66)
    lbox(d, "CT_Wood", gf, -1.22, 1.22, -0.05, 0.92, 0.66, 0.7)
    lbox(d, "CT_TileWhite", gf, -1.3, 1.3, 1.3, 1.36, 0.0, 0.015)                       # the throwing line
    # a field of little prize stands, some with a ring on
    k = 0
    for row, w in enumerate((0.15, 0.5)):
        for col in range(6):
            u = -0.95 + 0.38 * col + (0.19 if row else 0.0)
            if u > 1.0:
                continue
            x, y, z = gf.p(u, w, 0.7)
            mat = FLAGS[(col + row) % 5]
            cyl(d, mat, x, y, 0.7, 0.88, 0.07, 6)
            if (col + row) % 3 == 0:
                _ring(d, "CT_SteelRed" if row else "CT_Brass", gf, u, w, 0.885, r=0.09)
            k += 1
    # the rings: a peg with a stack of rings
    x, y, _ = gf.p(1.0, 0.7)
    _bar(d, "CT_Wood", gf, (1.0, 0.7, 0.7), (1.0, 0.7, 0.95), 0.03)
    for j in range(3):
        _ring(d, ("CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterGreen")[j], gf, 1.0, 0.7, 0.72 + 0.04 * j, r=0.1)
    # the prize shelf behind
    kit.shop_shelf(B, *gf.p(0, -0.95)[:2], 0.0, yaw, w=2.4, h=2.2, goods="toys", seed=7)
    for sg in (-1, 1):                                                               # posts and roof
        lbox(d, "CT_Wood", gf, sg * 1.3 - 0.04, sg * 1.3 + 0.04, 0.95, 1.03, 0.0, 2.55)
    _roof(B, gf, 1.5, -1.1, 1.1, 2.55)
    _valance(B, gf, 1.5, 1.07, 2.2, 2.55, 8, mats=("CT_PlasterBlue", "CT_TileWhite"))
    lbox(d, "CT_PlasterYellow", gf, -0.6, 0.6, 1.07, 1.11, 2.62, 2.9)                   # a name board over the roof
    kit.poster(B, *gf.p(0, 1.11)[:2], 0.0, yaw, name="kids_2", w=0.5, h=0.24, zc=2.76)
    kit.crate(B, *gf.p(-1.25, 0.4)[:2], 0.7, yaw + 90, fill="CT_PlasterYellow", seed=3)
    B.thing("ring_table", "play", gf.p(0, 0.3, 0.95), yaw, what="ring_toss")
    B.thing("prize_shelf", "shelf", gf.p(0, -0.6, 1.4), yaw, topic="kids")
    _stand(B, gf, "ring_stand", 0.0, 1.75)
    _stand(B, gf, "ring_stand_2", 1.1, 1.8)
    return gf


def lottery_drum(B, s=40.0, t=0.25, face=-1):
    gf, yaw = _gframe(s, t, face)
    d = B.detail
    lbox(B.m, "CT_PlasterRed", gf, -0.55, 0.55, -0.4, 0.4, 0.0, 0.78)
    lbox(d, "CT_TileWhite", gf, -0.6, 0.6, -0.45, 0.45, 0.78, 0.82)
    for sg in (-1, 1):                                                               # drum cradle
        _bar(d, "CT_Wood", gf, (sg * 0.45, -0.3, 0.82), (sg * 0.45, 0.0, 1.12), 0.05)
        _bar(d, "CT_Wood", gf, (sg * 0.45, 0.3, 0.82), (sg * 0.45, 0.0, 1.12), 0.05)
    cz = 1.17
    r, n = 0.34, 8
    ring0 = [gf.p(-0.4, r * math.cos(2 * math.pi * k / n), cz + r * math.sin(2 * math.pi * k / n)) for k in range(n)]
    ring1 = [gf.p(0.4, r * math.cos(2 * math.pi * k / n), cz + r * math.sin(2 * math.pi * k / n)) for k in range(n)]
    for k in range(n):
        fu.rod(d, "CT_PlasterRed", ring0[k], ring1[k], 0.03, caps=False)
        for ring in (ring0, ring1):
            fu.rod(d, "CT_Brass", ring[k], ring[(k + 1) % n], 0.035, caps=False)
    _bar(d, "CT_Steel", gf, (-0.55, 0.0, cz), (0.62, 0.0, cz), 0.03)                    # axle
    _bar(d, "CT_Steel", gf, (0.62, 0.0, cz), (0.62, 0.24, cz + 0.12), 0.02)             # crank
    blob(d, "CT_PlasterRed", *gf.p(0.62, 0.26)[:2], cz + 0.12, 0.04, 5)
    for k in range(8):                                                               # tickets inside
        a = k * 2.399
        lbox(d, FLAGS[k % 6] if k % 3 else "CT_Paper", gf, -0.25 + 0.5 * ((k * 5) % 7) / 7, -0.25 + 0.5 * ((k * 5) % 7) / 7 + 0.06,
             0.1 * math.cos(a) - 0.04, 0.1 * math.cos(a) + 0.04, cz - 0.2 + 0.05 * (k % 4), cz - 0.17 + 0.05 * (k % 4))
    lbox(d, "CT_PlasterYellow", gf, -0.25, 0.1, 0.1, 0.34, 0.82, 0.88)                  # a tray with the won tickets
    for j in range(5):
        lbox(d, "CT_Paper", gf, -0.22 + 0.06 * j, -0.17 + 0.06 * j, 0.14, 0.3, 0.88, 0.9)
    _parasol(B, gf, 0.0, -0.55, 2.35, r=1.1, zp=2.85)
    kit.bottles(B, *gf.p(-0.45, 0.2)[:2], 0.82, n=2, seed=4)
    B.lamp(gf.p(0, 0.2, 2.4), lumens=200, colour="#FFC890", kind="public", radius=5.0, fixture=False)
    B.thing("lottery_drum", "play", gf.p(0, 0.4, 1.17), yaw, what="lottery")
    _stand(B, gf, "lottery_stand", 0.0, 1.5)
    return gf


def fortune_automaton(B, s=109.0, t=-0.4, face=1):
    gf, yaw = _gframe(s, t, face)
    d = B.detail
    lbox(B.m, "CT_PlasterBlue", gf, -0.55, 0.55, -0.4, 0.4, 0.0, 0.9)
    lbox(d, "CT_Brass", gf, -0.58, 0.58, -0.43, 0.43, 0.9, 0.93)
    lbox(d, "CT_Brass", gf, -0.58, 0.58, 0.4, 0.43, 0.0, 0.05)
    for u in (-0.3, 0.3):
        lbox(d, "CT_Brass", gf, u - 0.012, u + 0.012, 0.402, 0.41, 0.2, 0.84)             # gilt trim on the front
    lbox(d, "CT_SteelRed", gf, -0.08, 0.08, 0.402, 0.42, 0.62, 0.68)                    # the coin slot
    lbox(d, "CT_Steel", gf, -0.2, 0.2, 0.3, 0.44, 0.33, 0.37)                          # the card tray
    lbox(d, "CT_Paper", gf, -0.06, 0.1, 0.34, 0.42, 0.37, 0.375)
    for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):                                # glass case posts
        lbox(d, "CT_Wood", gf, sx * 0.5 - 0.03, sx * 0.5 + 0.03, sy * 0.36 - 0.03, sy * 0.36 + 0.03, 0.93, 2.25)
    lbox(d, "CT_PlasterBlue", gf, -0.6, 0.6, -0.46, 0.46, 2.25, 2.35)
    lbox(d, "CT_PlasterYellow", gf, -0.5, 0.5, -0.38, 0.38, 2.35, 2.45)
    blob(d, "CT_Brass", *gf.p(0, 0)[:2], 2.55, 0.1, 6)
    lbox(d, "CT_PlasterBlue", gf, -0.48, 0.48, -0.37, -0.33, 0.93, 2.25)                # back panel, a starry sky
    for k in range(9):
        blob(d, "CT_Brass", *gf.p(-0.4 + 0.1 * ((k * 5) % 9), -0.32)[:2], 1.5 + 0.09 * ((k * 4) % 8), 0.018, 4)
    lbox(d, "CT_PlasterRed", gf, -0.3, 0.3, -0.3, 0.12, 0.93, 1.1)                      # the fortune-teller's table
    cyl(d, "CT_PlasterRed", *gf.p(0, -0.05)[:2], 1.1, 1.38, 0.2, 8)                      # robe
    cyl(d, "CT_PlasterBlue", *gf.p(0, -0.05)[:2], 1.38, 1.6, 0.14, 8)
    blob(d, "CT_PlasterPink", *gf.p(0, -0.05)[:2], 1.72, 0.1, 6)                          # head
    cyl(d, "CT_PlasterYellow", *gf.p(0, -0.05)[:2], 1.78, 1.86, 0.11, 8)                 # turban
    blob(d, "CT_PlasterYellow", *gf.p(0, -0.05)[:2], 1.9, 0.08, 6)
    for sg in (-1, 1):                                                               # arms to the crystal ball
        _bar(d, "CT_PlasterBlue", gf, (sg * 0.16, -0.05, 1.56), (sg * 0.1, 0.18, 1.32), 0.07)
    blob(d, "CT_Opal", *gf.p(0, 0.2)[:2], 1.27, 0.09, 6)                                  # the crystal ball
    lbox(d, "CT_Brass", gf, -0.06, 0.06, 0.14, 0.26, 1.1, 1.17)
    for j in range(5):                                                               # a fan of cards on the table
        lbox(d, "CT_Paper", gf, -0.2 + 0.1 * j, -0.14 + 0.1 * j, 0.24, 0.3, 1.1, 1.103)
    B.lamp(gf.p(0, 0.15, 2.1), lumens=240, colour="#FFB090", kind="public", radius=5.0, fixture=False)
    B.thing("fortune_automaton", "machine", gf.p(0, 0.45, 1.3), yaw, what="fortune")
    _stand(B, gf, "fortune_stand", 0.0, 1.3)
    return gf


def weighing_chair(B, s=102.0, t=-0.4, face=1):
    gf, yaw = _gframe(s, t, face)
    d = B.detail
    lbox(B.m, "CT_PlasterGreen", gf, -0.5, 0.5, -0.55, 0.55, 0.0, 0.14)                   # platform
    lbox(d, "CT_Steel", gf, -0.52, 0.52, 0.55, 0.58, 0.0, 0.14)
    for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):                                # chair
        lbox(d, "CT_Wood", gf, sx * 0.22 - 0.02, sx * 0.22 + 0.02, sy * 0.18 - 0.02, sy * 0.18 + 0.02, 0.14, 0.58)
    lbox(d, "CT_Leather", gf, -0.27, 0.27, -0.23, 0.23, 0.58, 0.63)
    lbox(d, "CT_Leather", gf, -0.25, 0.25, -0.26, -0.2, 0.63, 1.2)
    for sg in (-1, 1):
        lbox(d, "CT_Wood", gf, sg * 0.28 - 0.025, sg * 0.28 + 0.025, -0.2, 0.2, 0.84, 0.88)
        lbox(d, "CT_Wood", gf, sg * 0.28 - 0.02, sg * 0.28 + 0.02, -0.2, -0.16, 0.63, 0.88)
    lbox(d, "CT_Wood", gf, -0.25, 0.25, 0.3, 0.42, 0.14, 0.2)                           # footrest
    lbox(d, "CT_SteelRed", gf, -0.09, 0.09, -0.52, -0.36, 0.14, 1.95)                   # the scale's post
    lbox(d, "CT_Steel", gf, -0.62, 0.62, -0.48, -0.4, 1.9, 1.96)                        # the beam
    lbox(d, "CT_Brass", gf, 0.25, 0.37, -0.5, -0.38, 1.84, 2.02)                        # the sliding weight
    lbox(d, "CT_Steel", gf, 0.5, 0.58, -0.48, -0.4, 1.7, 1.9)
    lbox(d, "CT_Paper", gf, -0.25, 0.25, -0.358, -0.35, 1.36, 1.84)
    kit.poster(B, *gf.p(0, -0.35)[:2], 0.0, yaw, name="clock_2", w=0.4, h=0.4, zc=1.62)
    lbox(d, "CT_Steel", gf, -0.01, 0.01, -0.34, -0.3, 1.58, 1.72)                       # the pointer
    _parasol(B, gf, 0.0, -0.8, 2.35, r=1.0, zp=2.8, mats=("CT_PlasterGreen", "CT_TileWhite"))
    B.thing("weighing_chair", "machine", gf.p(0, -0.15, 1.62), yaw, what="scales")
    B.thing("weighing_seat", "seat", gf.p(0, 0.0, 0.59), yaw, height=0.45)
    _stand(B, gf, "weighing_stand", 0.0, 1.5)
    # the Lost and Found basket beside the platform: an umbrella, a mitten
    kit.crate(B, *gf.p(1.0, 0.1)[:2], 0.0, yaw + 90, w=0.5, d=0.38, h=0.34, fill="CT_PlasterPink", seed=21)
    _bar(d, "CT_Wood", gf, (0.95, 0.05, 0.3), (0.8, 0.0, 0.62), 0.02)
    blob(d, "CT_PlasterBlue", *gf.p(1.1, 0.12)[:2], 0.38, 0.06, 5)
    B.thing("fair_lost_basket", "board", gf.p(1.0, 0.1, 0.5), yaw, topic="lost")
    return gf


def photographer(B, s=87.0, t=-0.2, face=1):
    gf, yaw = _gframe(s, t, face)
    d = B.detail
    kit.photo_backdrop(B, *gf.p(0, -1.5)[:2], 0.0, yaw, w=2.2, h=2.3, mat="CT_PlasterBlue")
    lbox(d, "CT_PlasterRed", gf, -0.22, 0.22, -0.8, -0.38, 0.4, 0.45)                    # the posing chair
    lbox(d, "CT_PlasterRed", gf, -0.22, 0.22, -0.82, -0.78, 0.45, 0.95)
    for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        lbox(d, "CT_Wood", gf, sx * 0.19 - 0.02, sx * 0.19 + 0.02, -0.59 + sy * 0.19 - 0.02, -0.59 + sy * 0.19 + 0.02, 0.0, 0.4)
    kit.camera_tripod(B, *gf.p(0, 1.9)[:2], 0.0, yaw + 180)
    kit.cardboard_box(B, *gf.p(0.9, 1.8)[:2], 0.0, yaw + 20, w=0.6, d=0.4, h=0.3, seed=11)
    kit.sandwich_board(B, *gf.p(-1.4, 0.5)[:2], 0.0, yaw, name="price_1", w=0.55)
    # a flash lamp on a pole
    _bar(d, "CT_Steel", gf, (-0.9, 1.5, 0.0), (-0.9, 1.5, 1.75), 0.03)
    lbox(d, "CT_Aluminium", gf, -1.05, -0.75, 1.42, 1.5, 1.65, 1.9)
    blob(d, "CT_Opal", *gf.p(-0.9, 1.4)[:2], 1.78, 0.07, 5)
    B.thing("photo_camera", "machine", gf.p(0, 1.9, 1.35), yaw, what="photographer")
    B.thing("photo_chair", "seat", gf.p(0, -0.6, 0.45), yaw + 180, height=0.45)
    _stand(B, gf, "photo_stand", 1.2, 1.0)
    return gf


def chalk_board(B, s=78.0, t=0.4, face=-1):
    gf, yaw = _gframe(s, t, face)
    d = B.detail
    lbox(d, "CT_Wood", gf, -0.68, 0.68, -0.09, -0.04, 0.6, 1.8)
    for sg in (-1, 1):
        _bar(d, "CT_Wood", gf, (sg * 0.58, -0.07, 1.75), (sg * 0.64, 0.42, 0.0), 0.05)
        _bar(d, "CT_Wood", gf, (sg * 0.58, -0.07, 1.75), (sg * 0.64, -0.52, 0.0), 0.04)
    lbox(d, "CT_ConcreteDark", gf, -0.62, 0.62, -0.04, 0.0, 0.7, 1.7)                 # the slate
    for k, ln in enumerate((0.9, 0.7, 1.0, 0.6, 0.8, 0.5)):                          # chalk lines: prizes and scores
        z = 1.55 - 0.15 * k
        lbox(d, "CT_TileWhite", gf, -0.52, -0.52 + ln * 0.9, 0.0, 0.004, z, z + 0.02)
        lbox(d, "CT_TileWhite", gf, 0.4, 0.5, 0.0, 0.004, z - 0.005, z + 0.03)
    lbox(d, "CT_Wood", gf, -0.66, 0.66, 0.0, 0.1, 0.64, 0.7)                          # the chalk tray
    for k in range(3):
        lbox(d, "CT_TileWhite", gf, -0.3 + 0.12 * k, -0.24 + 0.12 * k, 0.03, 0.05, 0.7, 0.72)
    lbox(d, "CT_PlasterRed", gf, -0.7, 0.7, -0.12, -0.03, 1.8, 1.86)
    B.thing("prize_board", "board", gf.p(0, 0.05, 1.2), yaw, topic="fair")
    _stand(B, gf, "chalk_stand", 0.0, 1.1)
    return gf


def lost_sandal(B, s=12.0, t=-0.35):
    """A child's sandal under the bench, a thing to find."""
    cx, cy = surf(s, t)
    x, y, tx, ty = at(s)
    fr = Fr(cx, cy, tx, ty)
    px, py = fr.p(0.5, 0.0)
    lbox(B.detail, "CT_PlasterRed", MFr(px, py, 20.0), -0.07, 0.07, -0.05, 0.1, 0.0, 0.04)
    lbox(B.detail, "CT_PlasterRed", MFr(px, py, 20.0), -0.07, 0.07, -0.05, -0.02, 0.04, 0.09)
    B.thing("fair_sandal", "board", (px, py, 0.1), 20.0, topic="lost")


def fair_games(B):
    strongman_tower(B)
    ring_toss_table(B)
    lottery_drum(B)
    shooting_booth(B)
    chalk_board(B)
    photographer(B)
    weighing_chair(B)
    fortune_automaton(B)
    lost_sandal(B)
    for i, s in enumerate((9, 21, 31, 43, 57, 69, 83, 95, 111)):
        bunting_line(B, float(s), seed=i)
    # people watch from the far side of the walk
    B.spot("fair_view_1", "view", (*surf(60.0, 0.1), 0.0), 0.0)
    B.spot("fair_view_2", "view", (*surf(30.0, 0.0), 0.0), 0.0)


def build(B):
    # canopy slab
    def f(u, v):
        s, t = u * LENGTH, 2 * v - 1
        x, y = surf(s, t)
        return (x, y, z_top(s, t))
    nu = int(LENGTH)
    thick_sheet(B.m, lambda u, v: f(u, v), nu, 10, lambda u, v: thick(u * LENGTH, 2 * v - 1),
                "CT_ConcreteWhite", "CT_ConcreteWhite", "CT_ConcreteWhite")
    # paving ribbon under it
    def fl(u, v):
        s, t = u * LENGTH, 2 * v - 1
        x, y = surf(s, t * 1.0)
        tx, ty = at(s)[2:]
        w = half_width(s) + 1.4
        xc, yc = at(s)[:2]
        return (xc - ty * t * w, yc + tx * t * w, 0.0)
    thick_sheet(B.m, fl, nu // 2, 8, lambda u, v: 0.3, "CT_Paving", "CT_ConcreteDark", "CT_ConcreteDark")
    # kiosks first (columns avoid them)
    kiosks = [(20.0, 1), (46.0, -1), (66.0, 1), (92.0, -1), (108.0, 1)]
    for k, (s, side) in enumerate(kiosks, 1):
        kiosk(B, s, side, k)
    # columns
    n = 0
    s = 4.0
    i = 0
    while s < LENGTH - 2.0:
        for side in (1, -1):
            if (i + (side > 0)) % 3 == 2 and False:
                continue
            ss = s + (3.5 if side < 0 else 0)
            if ss > LENGTH - 2:
                continue
            t = 0.62 * side
            if any(abs(ss - ks) < 4.2 and ksd == side for ks, ksd in kiosks):
                continue
            x, y = surf(ss, t)
            zu = z_top(ss, t) - thick(ss, t)
            B.column_round(x, y, 0.0, zu + 0.05, 0.14, "CT_ConcreteWhite", segs=10)
            n += 1
        s += 7.0
        i += 1
    string_lights(B, None)
    fair_games(B)
    fair_board(B, 52.0, -0.5, "fair_board", "fair")
    fair_board(B, 100.0, 0.5, "fair_programme", "cinema", posters=True)
    # benches
    k = 0
    for ss, t in [(12, -0.35), (30, 0.4), (38, -0.4), (56, 0.35), (60, -0.35), (79, -0.4), (84, 0.4), (100, 0.35), (116, -0.4), (8, 0.45)]:
        if ss < LENGTH - 3 and not any(abs(ss - ks) < 4.0 and ksd * t > 0 for ks, ksd in kiosks):
            k += 1
            bench(B, ss, t, k)
    # footprint: the canopy outline
    left = [surf(LENGTH * i / 60, 1.0) for i in range(61)]
    right = [surf(LENGTH * i / 60, -1.0) for i in range(61)]
    B.footprint = right + left[::-1]
    B.height = max(z_top(LENGTH * i / 200, 1.0) for i in range(201)) + 0.2
    for m_ in kit.check(B):
        print("KIT:", m_)
    B.note("canopy %.0f m long (arc), %d columns, %d kiosks, %d benches" % (LENGTH, n, len(kiosks), k))
