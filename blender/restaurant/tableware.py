"""The restaurant's tableware and dishes as clean low-poly game meshes (the diner's pipeline: blender/diner/tableware.py).

  Blender -b -P blender/restaurant/tableware.py [-- Name ...]   ->  design/restaurant/game/tableware/<mesh>.glb
                                                                     design/restaurant/tableware.json
  Blender -b -P blender/restaurant/pack_tableware.py            ->  design/restaurant/game/tableware_kit.glb
  Blender -b -P blender/restaurant/preview_tableware.py         ->  design/restaurant/previews/*.png

Names: tableware SM_RT_<Name> (+ SM_RT_<Name>_Glass), food SM_RF_<dish>_<k> (k = 0 full .. 4 the last remnant), liquid
columns SM_RF_<dish>_Fill, loads SM_RF_Load_<dish>_S / _B. Slots RS_T_* / RS_F_* (tableware_mats.py).

Frames (metres, x forward, y left, z up, exported y-up like the diner):
  plates, bowls, the board, the tray, the coupe, cups, glasses, the carafe, the basket   origin at the bottom centre
  food stages and fills          origin at the vessel's inner floor centre: put it at the vessel origin + fill_z
  fills                          z 0..fill_h; at level t the game scales Z by t and X, Y by fill_xy(t) (tableware.json)
  fork, knife, spoon, teaspoon, chopsticks   origin at the grip, tips / blade / bowl along +x
  fork and spoon loads           origin at the load point (TINE_X, 0, LOAD_Z) of the utensil (the diner's convention);
                                 teaspoon loads at (TEASPOON_X, 0, TEASPOON_LOAD_Z); chopstick loads at the tips' middle
                                 (CHOP_TIP_X, 0, CHOP_TIP_Z), hanging down; finger loads at the pinch point, food along +x
"""
import json
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dishes_common as K                    # noqa: E402
from dishes_common import C, D, M, PI, new, merge, xform, paint, setmat, rest, conform, cut  # noqa: E402

ROOT = K.ROOT
JSON_PATH = os.path.join(ROOT, "design", "restaurant", "tableware.json")
interp, smooth = D.interp, D.smoothstep


def lin(a, b, n):
    return [a + (b - a) * i / (n - 1) for i in range(n)]


# =========================================================================================================================
# vessels
# =========================================================================================================================

CH, AM = M("RS_T_China"), M("RS_T_Amber")

PLATE_PTS = [(0.0675, 0.0030), (0.0765, 0.0), (0.0900, 0.0), (0.1254, 0.0105), (0.1350, 0.0195), (0.1330, 0.0234),
             (0.1290, 0.0240), (0.1250, 0.0240), (0.1190, 0.0236), (0.1105, 0.0220), (0.1000, 0.0160), (0.0945, 0.0145),
             (0.0675, 0.0145)]


def plate_pts(R, sz):
    s = R / 0.135
    return [(r * s, z * sz) for r, z in PLATE_PTS]


def plate_vessel(name, R, sz):
    p = plate_pts(R, sz)
    top = p[-1][1]
    return K.Vessel(name, [(0.0, top)] + p[6:12][::-1], top, 0.125 * R / 0.135)


def plate(R=0.135, sz=1.0):
    bm = new()
    mats = [CH] * 12
    mats[6] = AM
    D.lathe_fan(bm, plate_pts(R, sz), mats, 44, cap0=True, cap1=True)
    return bm


V_DINNER = plate_vessel("SM_RT_DinnerPlate", 0.135, 1.0)
V_DESSERT = plate_vessel("SM_RT_DessertPlate", 0.100, 0.85)

SOUP_PTS = [(0.045, 0.003), (0.050, 0.0), (0.060, 0.0), (0.075, 0.007), (0.086, 0.020), (0.093, 0.032), (0.118, 0.039),
            (0.120, 0.0415), (0.1185, 0.0440), (0.1145, 0.0446), (0.1105, 0.0445), (0.094, 0.0408), (0.0885, 0.034),
            (0.081, 0.022), (0.070, 0.0125), (0.055, 0.0085), (0.045, 0.008)]
V_SOUP = K.Vessel("SM_RT_SoupPlate", [(0.0, 0.008), (0.045, 0.008), (0.055, 0.0085), (0.070, 0.0125), (0.081, 0.022),
                                      (0.0885, 0.034), (0.094, 0.0408), (0.1105, 0.0445)], 0.008, 0.087)


def soup_plate():
    bm = new()
    mats = [CH] * 16
    mats[9] = AM
    D.lathe_fan(bm, SOUP_PTS, mats, 44, cap0=True, cap1=True)
    return bm


RAMEN_PTS = [(0.030, 0.002), (0.034, 0.0), (0.041, 0.0), (0.042, 0.007), (0.047, 0.009), (0.068, 0.024), (0.083, 0.045),
             (0.0915, 0.066), (0.0950, 0.0825), (0.0945, 0.0850), (0.0915, 0.0850), (0.0905, 0.0835), (0.0875, 0.067),
             (0.079, 0.046), (0.064, 0.026), (0.044, 0.0135), (0.030, 0.012)]
V_RAMEN = K.Vessel("SM_RT_RamenBowl", [(0.0, 0.012), (0.030, 0.012), (0.044, 0.0135), (0.064, 0.026), (0.079, 0.046),
                                       (0.0875, 0.067), (0.0905, 0.0835)], 0.012, 0.0895)


def ramen_bowl():
    bm = new()
    G, R_, S = M("RS_T_Glaze"), M("RS_T_GlazeRim"), M("RS_T_Stoneware")
    mats = [S, S, S] + [G] * 13
    mats[8], mats[9], mats[10] = R_, R_, R_
    D.lathe_fan(bm, RAMEN_PTS, mats, 40, cap0=True, cap1=True)
    return bm


PASTA_PTS = [(0.050, 0.003), (0.056, 0.0), (0.068, 0.0), (0.086, 0.010), (0.096, 0.026), (0.134, 0.038), (0.140, 0.0415),
             (0.1385, 0.0450), (0.1340, 0.0458), (0.1300, 0.0457), (0.1000, 0.0405), (0.0930, 0.032), (0.0820, 0.0165),
             (0.0640, 0.0105), (0.0500, 0.0100)]
V_PASTA = K.Vessel("SM_RT_PastaPlate", [(0.0, 0.010), (0.050, 0.010), (0.064, 0.0105), (0.082, 0.0165), (0.093, 0.032),
                                        (0.100, 0.0405), (0.130, 0.0457)], 0.010, 0.091)


def pasta_plate():
    bm = new()
    mats = [CH] * 14
    mats[8] = AM
    D.lathe_fan(bm, PASTA_PTS, mats, 44, cap0=True, cap1=True)
    return bm


BOARD_RX, BOARD_RY, BOARD_H = 0.18, 0.11, 0.018
V_BOARD = K.Vessel("SM_RT_Board", [(0.0, BOARD_H), (0.174, BOARD_H)], BOARD_H, 0.170, aspect=BOARD_RY / BOARD_RX)


def board():
    bm = new()
    W, E = M("RS_T_Wood"), M("RS_T_WoodEdge")
    prof = [(0.172, 0.0), (0.1795, 0.0025), (0.180, 0.006), (0.180, 0.0145), (0.178, 0.0172), (0.174, BOARD_H)]
    D.lathe_fan(bm, prof, [E, E, E, E, E], 56, cap0=True, cap1=True)
    for f in bm.faces:                                   # the top fan is the board's face
        if f.calc_center_median().z > BOARD_H - 1e-5:
            f.material_index = W
    xform(bm, scale=(1, BOARD_RY / BOARD_RX, 1))
    return bm


COUPE_PTS = [(0.036, 0.0), (0.038, 0.0018), (0.0365, 0.0045), (0.014, 0.0085), (0.0075, 0.016), (0.0060, 0.030),
             (0.0060, 0.044), (0.0085, 0.050), (0.018, 0.055), (0.036, 0.065), (0.0485, 0.079), (0.0545, 0.092),
             (0.0560, 0.0975), (0.0548, 0.0988), (0.0535, 0.0975), (0.0520, 0.0920), (0.0465, 0.0800), (0.0345, 0.0680),
             (0.0170, 0.0610)]
V_COUPE = K.Vessel("SM_RT_Coupe", [(0.0, 0.061), (0.017, 0.061), (0.0345, 0.068), (0.0465, 0.080), (0.052, 0.092),
                                   (0.0535, 0.0975)], 0.061, 0.051)


def coupe():
    bm = new()
    D.lathe_fan(bm, COUPE_PTS, [M("RS_T_Steel")], 36, cap0=True, cap1=True)
    return bm


CUP_PTS = [(0.022, 0.0), (0.026, 0.0), (0.027, 0.004), (0.030, 0.006), (0.036, 0.025), (0.0385, 0.045), (0.0392, 0.053),
           (0.0395, 0.0575), (0.0392, 0.0603), (0.0380, 0.0608), (0.0368, 0.0600), (0.0365, 0.058), (0.0355, 0.045),
           (0.0330, 0.025), (0.0255, 0.010), (0.0200, 0.009)]
CUP_IN_Z, CUP_H, CUP_R = 0.009, 0.0608, 0.0395
V_CUP = K.Vessel("SM_RT_CoffeeCup", [(0.0, 0.009), (0.020, 0.009), (0.0255, 0.010), (0.0330, 0.025), (0.0355, 0.045),
                                     (0.0365, 0.058)], CUP_IN_Z, 0.0355)
COFFEE_PROF = [(0.0185, 0.0), (0.0245, 0.001), (0.0318, 0.016), (0.0343, 0.036), (0.0347, 0.041)]
SAUCER_TOP = 0.006


def coffee_cup():
    bm = new()
    mats = [CH] * 15
    mats[6] = AM
    D.lathe_fan(bm, CUP_PTS, mats, 32, cap0=True, cap1=True)
    hp = D.spline([(0.0, 0.0368, 0.051), (0.0, 0.0470, 0.0535), (0.0, 0.0545, 0.0465), (0.0, 0.0540, 0.0345),
                   (0.0, 0.0470, 0.0270), (0.0, 0.0340, 0.0225)], 3)
    D.sweep(bm, hp, 0.0034, CH, 8)
    return bm


def saucer():
    bm = new()
    pts = [(0.040, 0.002), (0.044, 0.0), (0.052, 0.0), (0.068, 0.008), (0.0720, 0.0125), (0.0712, 0.0145), (0.0690, 0.0150),
           (0.0660, 0.0148), (0.0600, 0.0128), (0.0480, 0.0080), (0.0340, 0.0062), (0.0300, SAUCER_TOP)]
    mats = [CH] * 11
    mats[6] = AM
    D.lathe_fan(bm, pts, mats, 40, cap0=True, cap1=True)
    return bm


def coffee_fill():
    bm = new()
    mats = [M("RS_F_Coffee")] * 4 + [M("RS_F_Crema")]
    D.lathe_fan(bm, COFFEE_PROF, mats, 32, cap0=True, cap1=True)
    return bm


# the tumbler: a Soviet faceted glass (16 facets); its thick bottom is the main mesh, the walls the _Glass
TUMBLER_H, TUMBLER_IN_Z, TUMBLER_FILL_H, TUMBLER_SEG = 0.105, 0.010, 0.080, 16


def tumbler_ro(z):
    return 0.0320 + 0.0035 * z / TUMBLER_H


def tumbler_ri(z):
    return tumbler_ro(z) - 0.0022


V_TUMBLER = K.Vessel("SM_RT_Tumbler", [(0.0, TUMBLER_IN_Z), (tumbler_ri(TUMBLER_IN_Z), TUMBLER_IN_Z),
                                       (tumbler_ri(TUMBLER_H), TUMBLER_H)], TUMBLER_IN_Z, tumbler_ri(TUMBLER_H))
TUMBLER_FILL = [(tumbler_ri(TUMBLER_IN_Z) - 0.0006, 0.0), (tumbler_ri(TUMBLER_IN_Z + 0.04) - 0.0006, 0.04),
                (tumbler_ri(TUMBLER_IN_Z + TUMBLER_FILL_H) - 0.0006, TUMBLER_FILL_H)]


def tumbler():
    bm = new()
    G, GB = M("RS_T_Glass"), M("RS_T_GlassBase")
    ro, ri = tumbler_ro, tumbler_ri
    prof = [(ri(0.006), 0.006), (ro(0.006), 0.006), (ro(0.084), 0.084), (ro(0.086) + 0.0009, 0.086), (ro(0.103) + 0.0009, 0.103),
            (ro(0.105) + 0.0002, 0.105), (ri(0.105) + 0.0003, 0.105), (ri(TUMBLER_IN_Z), TUMBLER_IN_Z), (ri(0.006), 0.006)]
    C.lathe(bm, prof, [G], TUMBLER_SEG, cap0=False, cap1=False)
    D.lathe_fan(bm, [(ro(0.0) - 0.0008, 0.0), (ro(0.0008), 0.0008), (ro(0.0055), 0.0055), (ri(0.0075) + 0.0006, 0.0075),
                     (ri(TUMBLER_IN_Z) + 0.0004, TUMBLER_IN_Z)], [GB], TUMBLER_SEG, cap0=True, cap1=True)
    return bm


def tumbler_fill(mat):
    bm = new()
    D.lathe_fan(bm, TUMBLER_FILL, [M(mat)], TUMBLER_SEG, cap0=True, cap1=True)
    return bm


CARAFE_IN_Z, CARAFE_FILL_H, CARAFE_H = 0.010, 0.125, 0.2268
V_CARAFE = K.Vessel("SM_RT_Carafe", [(0.0, CARAFE_IN_Z), (0.0425, CARAFE_IN_Z), (0.0425, 0.139)], CARAFE_IN_Z, 0.0425)
CARAFE_FILL = [(0.0418, 0.0), (0.0418, CARAFE_FILL_H)]


def carafe():
    bm = new()
    G, GB = M("RS_T_Glass"), M("RS_T_GlassBase")
    prof = [(0.0420, 0.006), (0.0450, 0.006), (0.045, 0.140), (0.040, 0.158), (0.028, 0.172), (0.019, 0.182), (0.017, 0.192),
            (0.017, 0.214), (0.0205, 0.2235), (0.0212, 0.2262), (0.0192, CARAFE_H), (0.0148, 0.218), (0.0145, 0.192),
            (0.0165, 0.181), (0.0255, 0.170), (0.0375, 0.156), (0.0425, 0.139), (0.0425, CARAFE_IN_Z), (0.0420, 0.006)]
    C.lathe(bm, prof, [G], 28, cap0=False, cap1=False)
    D.lathe_fan(bm, [(0.0442, 0.0), (0.0449, 0.0008), (0.0449, 0.0055), (0.0432, 0.0075), (0.0430, CARAFE_IN_Z)], [GB], 28,
                cap0=True, cap1=True)
    return bm


def carafe_fill():
    bm = new()
    D.lathe_fan(bm, CARAFE_FILL, [M("RS_F_Water")], 28, cap0=True, cap1=True)
    return bm


# ---- the bread basket: an oval wicker basket with a linen napkin in it --------------------------------------------------
BK_H, BK_RIM_Z, BK_RIM_R, BK_IN_Z = 0.064, 0.066, 0.0058, 0.010
BK_OUT0, BK_OUT1 = (0.098, 0.060), (0.125, 0.085)        # outer semi-axes at the bottom and the top
BK_IN0, BK_IN1 = (0.090, 0.052), (0.119, 0.079)          # inner semi-axes at the floor (BK_IN_Z) and the top
BK_RIM = (0.122, 0.082)


def basket_inner_z(x, y):
    """Height of the basket's inner surface (floor or wall) under (x, y); None outside the top opening."""
    def f(s):
        a = BK_IN0[0] + s * (BK_IN1[0] - BK_IN0[0])
        b = BK_IN0[1] + s * (BK_IN1[1] - BK_IN0[1])
        return (x / a) ** 2 + (y / b) ** 2 - 1
    if f(0.0) <= 0:
        return BK_IN_Z
    if f(1.0) > 0:
        return None
    lo, hi = 0.0, 1.0
    for _ in range(30):
        m = (lo + hi) / 2
        if f(m) > 0:
            lo = m
        else:
            hi = m
    return BK_IN_Z + hi * (BK_H - BK_IN_Z)


_RIM_PTS = [(BK_RIM[0] * math.cos(2 * PI * i / 720), BK_RIM[1] * math.sin(2 * PI * i / 720)) for i in range(720)]


def napkin_top(x, y):
    """The basket napkin's upper surface (absolute z in the basket frame): on the inner surface, over the rim, then hanging
    down outside it (clearances keep the rim inside even between grid samples)."""
    zs = []
    zi = basket_inner_z(x, y)
    if zi is not None:
        zs.append(zi + 0.0040)
    d = min(math.hypot(x - px, y - py) for px, py in _RIM_PTS)
    if math.hypot(x / BK_RIM[0], y / BK_RIM[1]) < 1:
        d = -d
    top = BK_RIM_Z + BK_RIM_R + 0.0040                      # flat over the rim: no fold of the grid dips into it
    if abs(d) < BK_RIM_R + 0.006:
        zs.append(top)
    if d > 0:
        zs.append(max(0.0016, top - 1.7 * max(0.0, d - BK_RIM_R - 0.006)))
    return max(zs) if zs else 0.0016


def bread_basket():
    bm = new()
    W, WD, LI, LS = M("RS_T_Wicker"), M("RS_T_WickerDark"), M("RS_T_Linen"), M("RS_T_LinenStripe")
    seg = 36

    def oval(a, b, z):
        return [(a * math.cos(2 * PI * i / seg), b * math.sin(2 * PI * i / seg), z) for i in range(seg)]

    rings = [[(0, 0, 0.0)], oval(BK_OUT0[0] - 0.006, BK_OUT0[1] - 0.006, 0.0)]
    nb = 7
    for k in range(nb + 1):
        t = k / nb
        z = 0.004 + t * (BK_H - 0.004)
        rings.append(oval(BK_OUT0[0] + t * (BK_OUT1[0] - BK_OUT0[0]), BK_OUT0[1] + t * (BK_OUT1[1] - BK_OUT0[1]), z))
    rings.append(oval(BK_IN1[0], BK_IN1[1], BK_H))
    # (no inner wall and floor: the napkin, corner to corner wider than the opening, covers all of it)

    def weave(band, i):
        if band <= 1:
            return WD
        return W if (band + (i // 2)) % 2 else WD

    K.skin(bm, rings, weave)
    path = [(BK_RIM[0] * math.cos(2 * PI * i / 48), BK_RIM[1] * math.sin(2 * PI * i / 48), BK_RIM_Z) for i in range(48)]
    rim = new()
    K.ring_tube(rim, path, BK_RIM_R, WD, 6)
    paint(rim, lambda c, n, m: W if int((math.atan2(c.y / BK_RIM[1], c.x / BK_RIM[0]) + PI) / (2 * PI) * 28 +
                                           (2 if c.z > BK_RIM_Z else 0)) % 2 else WD)
    merge(bm, rim)
    # the napkin: a square of linen with a red border, corner to corner along x and y, draped over the rim
    S, n = 0.235, 20
    ca, sa = math.cos(PI / 4), math.sin(PI / 4)

    def pos(u, v):
        a, b = (u - 0.5) * S, (v - 0.5) * S
        return (a * ca - b * sa, a * sa + b * ca)

    def nm(i, j, top):
        edge = min(i, j, n - 1 - i, n - 1 - j)
        return LS if edge == 1 else LI

    nap = K.slab_grid(new(), n, n, pos, lambda x, y: napkin_top(x, y), lambda x, y: napkin_top(x, y) - 0.0016, nm)
    # its underside inside the basket is never seen (and would lie along the wicker): only the hanging flaps keep one
    nap.normal_update()
    dead = [f for f in nap.faces if f.normal.z < 0 and basket_inner_z(*f.calc_center_median().xy) is not None]
    bmesh.ops.delete(nap, geom=dead, context="FACES")
    merge(bm, nap)
    return bm


def tray():
    bm = new()
    pts = [(0.190, 0.0), (0.1965, 0.0015), (0.2000, 0.0110), (0.2005, 0.0165), (0.1990, 0.0182), (0.1968, 0.0172),
           (0.1950, 0.0045), (0.1890, 0.0026)]
    D.lathe_fan(bm, pts, [M("RS_T_Steel")], 56, cap0=True, cap1=True)
    return bm


TRAY_R, TRAY_H, TRAY_TOP = 0.2005, 0.0182, 0.0026


# =========================================================================================================================
# cutlery and table things
# =========================================================================================================================

ST = M("RS_T_Steel")
TEASPOON_S = 0.72


def fork():
    return setmat(D.fork(), ST)


def spoon():
    return setmat(D.spoon(), ST)


def teaspoon():
    return xform(setmat(D.spoon(), ST), scale=TEASPOON_S)


KNIFE_X0, KNIFE_X1 = -0.072, 0.142


def knife():
    bm = new()
    path = [(x, 0.0, 0.0) for x in lin(KNIFE_X0, 0.037, 9)]
    rad = lambda s: interp([(0, 0.0040), (0.06, 0.0060), (0.3, 0.0072), (0.75, 0.0062), (0.93, 0.0048), (1, 0.0042)], s)
    h = new()
    D.sweep(h, path, rad, ST, 10)
    xform(h, scale=(1, 1, 0.62))
    xform(h, loc=(0, 0, 0.0072 * 0.62))
    merge(bm, h)

    def spine(x):
        if x < 0.105:
            return interp([(0.030, 0.0040), (0.046, 0.0084), (0.105, 0.0084)], x)
        return -0.0040 + 0.0124 * math.sqrt(max(0.0, 1 - ((x - 0.105) / 0.0372) ** 2))

    def edge(x):
        return interp([(0.030, -0.0040), (0.046, -0.0076), (0.112, -0.0076), (0.1417, -0.0042)], x)

    xs = lin(0.030, 0.1415, 16)
    zc = lambda x: 0.0045 - 0.0028 * smooth((x - 0.030) / 0.09)
    D.cells(bm, xs, [-1, -0.5, 0, 0.5, 1], lambda x: max(0.0002, (spine(x) - edge(x)) / 2), lambda x, y: zc(x) + 0.0009, 0.0018, ST,
            yshift=lambda x: (spine(x) + edge(x)) / 2)
    return bm


CHOP_X0, CHOP_TIP_X, CHOP_GAP = -0.085, 0.145, 0.0056
CHOP_R0, CHOP_R1 = 0.0036, 0.0012


def chopsticks():
    bm = new()
    LQ, BA = M("RS_T_Lacquer"), M("RS_T_Bamboo")
    for sgn in (1, -1):
        xs = lin(CHOP_X0, CHOP_TIP_X, 9)
        pts = []
        for x in xs:
            s = (x - CHOP_X0) / (CHOP_TIP_X - CHOP_X0)
            r = CHOP_R0 + (CHOP_R1 - CHOP_R0) * s
            pts.append((x, sgn * (CHOP_GAP / 2 + CHOP_R1 + 0.0075 * (1 - s)), r))
        st = new()
        D.sweep(st, pts, lambda s: CHOP_R0 + (CHOP_R1 - CHOP_R0) * s, LQ, 6)
        paint(st, lambda c, n, m: LQ if c.x < 0.050 else BA)
        merge(bm, st)
    return bm


def table_napkin():
    """A folded linen napkin, 0.20 x 0.11, with an amber-red stripe near one end."""
    bm = new()
    LI, LS = M("RS_T_Linen"), M("RS_T_LinenStripe")
    nx, ny = 20, 10

    def pos(u, v):
        return ((u - 0.5) * 0.20, (v - 0.5) * 0.11)

    def top(x, y):
        u, v = x / 0.20 + 0.5, y / 0.11 + 0.5
        e = min(1.0, min(u, 1 - u, v, 1 - v) * 14) ** 0.5
        h = 0.0078 if x < 0.048 else 0.0056
        return 0.0003 + h * (0.45 + 0.55 * e)

    K.slab_grid(bm, nx, ny, pos, top, lambda x, y: 0.0, lambda i, j, t: LS if i in (16, 17) and t else LI)
    return bm


MENU_X, MENU_Y, MENU_H = 0.24, 0.32, 0.012


def menu_card():
    bm = new()
    LE, PA, CO, GO = M("RS_T_Leather"), M("RS_T_Paper"), M("RS_T_MenuCover"), M("RS_T_Gold")
    X, Y, H, tc = MENU_X / 2, MENU_Y / 2, MENU_H, 0.0030
    q = lambda pts, mi: C.quad(bm, pts, mi)
    q([(-X, -Y, 0), (-X, Y, 0), (X, Y, 0), (X, -Y, 0)], LE)                      # back
    q([(-X, -Y, H), (X, -Y, H), (X, Y, H), (-X, Y, H)], CO)                      # the front cover (picture slot)
    q([(-X, -Y, 0), (-X, -Y, H), (-X, Y, H), (-X, Y, 0)], LE)                    # the spine
    for z0, z1 in ((0, tc), (H - tc, H)):                                          # the covers' edges
        q([(X, -Y, z0), (X, Y, z0), (X, Y, z1), (X, -Y, z1)], LE)
        q([(X, Y, z0), (-X, Y, z0), (-X, Y, z1), (X, Y, z1)], LE)
        q([(-X, -Y, z0), (X, -Y, z0), (X, -Y, z1), (-X, -Y, z1)], LE)
    # the page block's edges, flush between the covers (a cream band round three sides)
    q([(X, -Y, tc), (X, Y, tc), (X, Y, H - tc), (X, -Y, H - tc)], PA)
    q([(X, Y, tc), (-X, Y, tc), (-X, Y, H - tc), (X, Y, H - tc)], PA)
    q([(-X, -Y, tc), (X, -Y, tc), (X, -Y, H - tc), (-X, -Y, H - tc)], PA)
    # the gold emblem: a ring with a Y in it, raised 0.8 mm, in the upper half of the cover
    cx, cy, zg = 0.0, 0.050, H + 0.0008
    n = 36
    ro, ri = 0.024, 0.0212
    O = [(cx + ro * math.cos(2 * PI * i / n), cy + ro * math.sin(2 * PI * i / n)) for i in range(n)]
    I = [(cx + ri * math.cos(2 * PI * i / n), cy + ri * math.sin(2 * PI * i / n)) for i in range(n)]
    for i in range(n):
        j = (i + 1) % n
        q([(O[i][0], O[i][1], zg), (O[j][0], O[j][1], zg), (I[j][0], I[j][1], zg), (I[i][0], I[i][1], zg)], GO)
        q([(O[i][0], O[i][1], H), (O[j][0], O[j][1], H), (O[j][0], O[j][1], zg), (O[i][0], O[i][1], zg)], GO)
        q([(I[j][0], I[j][1], H), (I[i][0], I[i][1], H), (I[i][0], I[i][1], zg), (I[j][0], I[j][1], zg)], GO)
    w, jy, L, a = 0.0016, cy + 0.002, 0.0135, math.radians(38)
    yb = cy - 0.016
    d = (math.sin(a), math.cos(a))
    nr = (math.cos(a), -math.sin(a))
    Er = (cx + L * d[0], jy + L * d[1])
    El = (cx - L * d[0], jy + L * d[1])
    ytop = (cx, jy + w / math.sin(a))
    Y_ = [(cx - w, yb), (cx + w, yb), (cx + w, jy - w * math.tan(a / 2) * 0.0 - 0.0005),
          (Er[0] + nr[0] * w, Er[1] + nr[1] * w), (Er[0] - nr[0] * w, Er[1] - nr[1] * w), ytop,
          (El[0] + nr[0] * w, El[1] - nr[1] * w), (El[0] - nr[0] * w, El[1] + nr[1] * w), (cx - w, jy - 0.0005)]
    K.prism_layers(bm, Y_, [H, zg], lambda i, b: GO, GO, GO)
    # (its bottom fan lies on the cover, facing down: delete it)
    dead = [f for f in bm.faces if f.material_index == GO and abs(f.calc_center_median().z - H) < 1e-6 and f.normal.z < -0.5]
    bm.normal_update()
    dead = [f for f in bm.faces if f.material_index == GO and abs(f.calc_center_median().z - H) < 1e-6 and f.normal.z < -0.5]
    bmesh.ops.delete(bm, geom=dead, context="FACES")
    return bm


SP_GAP = 0.044


def salt_pepper():
    bm = new()
    for sgn, body in ((1, CH), (-1, M("RS_T_ChinaBlack"))):
        b = new()
        D.lathe_fan(b, [(0.0140, 0.0), (0.0158, 0.0015), (0.0165, 0.006), (0.0160, 0.040), (0.0150, 0.047), (0.0138, 0.049)],
                    [body], 24, cap0=True, cap1=True)
        D.lathe_fan(b, [(0.0150, 0.0475), (0.0157, 0.0485), (0.0157, 0.0560), (0.0130, 0.0625), (0.0070, 0.0655), (0.0030, 0.0662)],
                    [ST], 24, cap0=True, cap1=True)
        merge(bm, xform(b, loc=(0, sgn * SP_GAP / 2, 0)))
    return bm


# =========================================================================================================================
# food helpers
# =========================================================================================================================

def tine_floor(x, y):
    return D.tine_z(x)


def ts_floor(x, y):
    s = TEASPOON_S
    return s * (D.spoon_zin(D.TINE_X + x / s, y / s) - D.LOAD_Z)


def spoon_floor(x, y):
    return D.spoon_zin(D.TINE_X + x, y) - D.LOAD_Z


def finish(parts):
    out = new()
    for p in parts:
        if p is not None and p.faces:
            merge(out, p)
    K.fan_ngons(out)
    return out


def dill_bit(bm, p, n, r, L=0.0045, rad=0.0006):
    a = r.uniform(0, PI)
    d = Vector((math.cos(a), math.sin(a), 0)) * L / 2
    c = Vector(p) + Vector(n) * (rad * 0.9)
    C.tube(bm, c - d, c + d + Vector((0, 0, r.uniform(-0.0006, 0.0006))), rad, M("RS_F_Dill"), 4)


def herb_sprig(bm, base, ang, mi, L=0.026, z=0.0, lift=0.0007):
    """A flat little sprig lying on a surface at height z: a stem and paired leaflets."""
    b = Vector((base[0], base[1], 0.0))
    d = Vector((math.cos(ang), math.sin(ang), 0))
    s = Vector((-d.y, d.x, 0))
    C.tube(bm, b + Vector((0, 0, z + lift)), b + d * L + Vector((0, 0, z + lift + 0.001)), 0.0006, mi, 4)
    for k in range(1, 6):
        p = b + d * (L * k / 6) + Vector((0, 0, z + lift + 0.0002 * k))
        for sg in (1, -1):
            C.tube(bm, p, p + (s * sg * 0.006 + d * 0.003) * (1 - k / 8), 0.00045, mi, 4)


def cube(bm, c, a, mi, rz=0.0, rx=0.0):
    b = new()
    C.box(b, (0, 0, 0), (a, a, a), mi)
    merge(bm, xform(b, loc=c, rz=rz, rx=rx))


def pea(bm, c, r, mi=None):
    K.sphere_blob(bm, c, r, M("RS_F_Pea") if mi is None else mi, 6, 4)


def blob(R, seed, amp=1.0):
    rr = random.Random(seed)
    p = [rr.uniform(0, 6.3) for _ in range(3)]
    return lambda th: R * (1 + amp * (0.12 * math.sin(2 * th + p[0]) + 0.08 * math.sin(3 * th + p[1]) + 0.05 * math.sin(5 * th + p[2])))


DOLLOP = [(1, 0), (1, .3), (.84, .68), (.55, .9), (.25, 1.0), (0, 1.08)]
POOL = [(1, 0), (1, .5), (.85, .9), (.5, 1.0), (0, 1.0)]


def dollop(cx, cy, z0, R, H, mi, seed, seg=18, prof=DOLLOP):
    bm = new()
    D.dome(bm, cx, cy, z0, blob(R, seed), prof, H, mi, seg, bottom=True)
    return bm


def spoon_liquid(bm, L, mi):
    """The surface of a liquid in the spoon's bowl, L deep (as the diner's soup load)."""
    rmax = math.sqrt(L / 0.005)
    seg = 22

    def ring(f, z):
        return [(D.SP_A * rmax * f * math.cos(2 * PI * i / seg), D.SP_B * rmax * f * math.sin(2 * PI * i / seg), z) for i in range(seg)]

    zin = lambda rho: 0.005 * rho * rho
    rings = [[(0, 0, 0.0002)], ring(0.5, zin(0.5 * rmax) + 0.0002), ring(0.85, zin(0.85 * rmax) + 0.0002), ring(1.0, zin(rmax) + 0.0002),
             ring(1.0, L + 0.0004), ring(0.85, L + 0.0008), ring(0.5, L + 0.0012), [(0, 0, L + 0.0014)]]
    D.loop_stack(bm, rings, mi)
    return L + 0.0014


def strip(bm, c, size, mi, rz, tilt=0.0):
    b = new()
    C.box(b, (0, 0, 0), size, mi)
    merge(bm, xform(b, loc=c, rz=rz, ry=tilt))


# =========================================================================================================================
# 1. borscht (SoupPlate): the liquid is SM_RF_borscht_Fill; the stages are what floats on it
# =========================================================================================================================

BORSCHT_FILL = [(0.0435, 0.0), (0.0530, 0.0005), (0.0680, 0.0045), (0.0790, 0.014), (0.0830, 0.020)]
BORSCHT_H = 0.020
BORSCHT_T = [1.0, 0.76, 0.54, 0.33, 0.14]
BORSCHT_XY = K.fill_xy(BORSCHT_FILL, V_SOUP)


def borscht_fill():
    bm = new()
    D.lathe_fan(bm, BORSCHT_FILL, [M("RS_F_Borscht")], 40, cap0=True, cap1=True)
    return bm


def borscht(k):
    t = BORSCHT_T[k]
    h = t * BORSCHT_H
    rs = K.fill_k(BORSCHT_XY, t) * BORSCHT_FILL[-1][0]          # the surface's radius at this stage
    SC, BE, CA, PO = M("RS_F_SourCream"), M("RS_F_Beet"), M("RS_F_Cabbage"), M("RS_F_PotatoIn")
    parts = []
    # the sour cream: scooped and melting into the soup
    Hd = [0.0105, 0.0090, 0.0068, 0.0048, 0.0030][k]
    Rd = [0.020, 0.019, 0.017, 0.015, 0.012][k]
    d = dollop(0, 0, -0.003, Rd, Hd + 0.003, SC, 5, 20)
    cuts = [K.ball_cutter((0.016, 0.006, 0.010), 0.010), K.ball_cutter((-0.012, 0.012, 0.010), 0.011),
            K.ball_cutter((0.002, -0.016, 0.008), 0.011), K.ball_cutter((-0.016, -0.008, 0.007), 0.010)][:k]
    d = cut(d, cuts, SC)
    parts.append(xform(d, loc=(0.012, -0.010, max(h - 0.0005, 0.0035))))
    # vegetables poking out of the soup
    rr = random.Random(41)
    veg = []
    for i in range(14):
        a, f = rr.uniform(0, 2 * PI), math.sqrt(rr.uniform(0.15, 1.0))
        veg.append((a, f, rr.uniform(0, PI), rr.random()))
    nveg = [12, 9, 7, 4, 2][k]
    for i, (a, f, rot, kind) in enumerate(veg[:nveg]):
        x, y = 0.82 * rs * f * math.cos(a), 0.82 * rs * f * math.sin(a)
        if math.hypot(x - 0.012, y + 0.010) < 0.024:
            continue
        b = new()
        if i % 3 == 2:
            C.box(b, (0, 0, 0), (0.0200, 0.0042, 0.0016), CA)
        elif i % 5 == 4:
            C.box(b, (0, 0, 0), (0.0075, 0.0075, 0.0060), PO)
        else:
            C.box(b, (0, 0, 0), (0.0220, 0.0032, 0.0030), BE)
        xform(b, loc=(x, y, h + 0.0002), rz=rot, ry=0.12 * (kind - 0.5))
        lo = K.lowest_gap(b, V_SOUP.floor)
        if lo < 0.0005:
            bmesh.ops.translate(b, vec=(0, 0, 0.0005 - lo), verts=b.verts)
        parts.append(b)
    # dill: chopped bits on the surface, a sprig on the cream
    dl = new()
    nd = [14, 11, 8, 5, 3][k]
    for i in range(nd):
        a, f = rr.uniform(0, 2 * PI), math.sqrt(rr.uniform(0.0, 1.0)) * 0.8 * rs
        dill_bit(dl, (f * math.cos(a), f * math.sin(a), h + 0.0002), (0, 0, 1), rr)
    if k < 3:
        herb_sprig(dl, (0.004, -0.004), 0.6, M("RS_F_Dill"), 0.022, h + Hd + 0.0001)
    parts.append(dl)
    return finish(parts)


def load_borscht(big):
    bm = new()
    L = 0.0042 if big else 0.0030
    top = spoon_liquid(bm, L, M("RS_F_Borscht"))
    strip(bm, (0.002, 0.002, top - 0.0004), (0.016 if big else 0.012, 0.0028, 0.0024), M("RS_F_Beet"), 0.35)
    if big:
        strip(bm, (-0.009, -0.004, top - 0.0003), (0.013, 0.0035, 0.0014), M("RS_F_Cabbage"), -0.5)
        rr = random.Random(3)
        for i in range(3):
            dill_bit(bm, (-0.014 + 0.007 * i, 0.006 - 0.004 * i, top - 0.0006), (0, 0, 1), rr, 0.004, 0.0005)
    return bm


# =========================================================================================================================
# 2. pelmeni (PastaPlate): twelve dumplings, sour cream and dill
# =========================================================================================================================

def dumpling(c, ang, floor=None, seed=0):
    bm = new()
    DO = M("RS_F_Dough")
    D.ellipsoid(bm, (0, 0, 0.0088), 0.0158, 0.0118, 0.0084, DO, 0.0, 12, 5)
    D.ellipsoid(bm, (0, -0.0014, 0.0038), 0.0182, 0.0146, 0.0031, DO, 0.0, 12, 4)
    xform(bm, loc=c, rz=ang)
    if floor:
        rest(bm, floor, 0.0006)
    return bm


PEL_POS = [(0.0, 0.0, 0.3)]
PEL_POS += [(0.031 * math.cos(math.radians(30 + 120 * i)), 0.031 * math.sin(math.radians(30 + 120 * i)),
             math.radians(30 + 120 * i) + PI / 2 + 0.3 * (i - 1)) for i in range(3)]
PEL_POS += [(0.058 * math.cos(math.radians(45 * i + 8)), 0.058 * math.sin(math.radians(45 * i + 8)),
             math.radians(45 * i + 8) + PI / 2 + (0.2 if i % 2 else -0.15)) for i in range(8)]
# eaten from the near (+x) side: outer ring first, the middle last
PEL_ORDER = [4, 5, 11, 6, 10, 7, 9, 1, 8, 3, 2, 0]
PEL_LEFT = [12, 10, 7, 4, 1]


def pelmeni(k):
    parts = []
    gone = PEL_ORDER[:12 - PEL_LEFT[k]]
    bitten = PEL_ORDER[12 - PEL_LEFT[k]] if k > 0 else None
    for i, (x, y, a) in enumerate(PEL_POS):
        if i in gone:
            continue
        p = dumpling((x, y, 0), a, V_PASTA.floor)
        if i == bitten:
            out = Vector((x, y)).normalized() if (x or y) else Vector((1, 0))
            p = cut(p, [K.bite_cutter(x + out.x * 0.023, y + out.y * 0.023, 0.0140, seed=i)], M("RS_F_Filling"))
        parts.append(p)
    SC = M("RS_F_SourCream")
    if k < 4:
        d = dollop(0.005, -0.004, 0.0125, [0.017, 0.016, 0.014, 0.012][k], [0.010, 0.009, 0.008, 0.006][k], SC, 9)
        cuts = [K.ball_cutter((0.020, -0.002, 0.020), 0.008), K.ball_cutter((0.010, 0.012, 0.021), 0.008),
                K.ball_cutter((-0.008, -0.014, 0.020), 0.009)][:k]
        parts.append(cut(d, cuts, SC))
    else:
        parts.append(conform(dollop(0.030, -0.022, 0.0004, 0.011, 0.0028, SC, 4, 16, POOL), V_PASTA.floor))
    # chopped dill over everything still there
    dl = new()
    rr = random.Random(77)
    pts = [(0.070 * math.sqrt(rr.random()) * math.cos(a), 0.070 * math.sqrt(rr.random()) * math.sin(a))
           for a in [rr.uniform(0, 2 * PI) for _ in range(34)]]
    K.sprinkle(dl, parts, pts, lambda b, p, n, r: dill_bit(b, p, n, r), 7)
    parts.append(dl)
    return finish(parts)


def load_pelmeni(big):
    parts = [dumpling((0.004, 0, 0), PI / 2 + 0.1, tine_floor)]
    if big:
        parts.append(dumpling((-0.024, 0.001, 0), PI / 2 - 0.15, tine_floor))
        parts.append(dollop(-0.004, 0.0, 0.0125, 0.006, 0.004, M("RS_F_SourCream"), 2, 12))
    return finish(parts)


# =========================================================================================================================
# 3. baltic_fish (DinnerPlate): sprats on rye, a fan of smoked salmon, lemon, red onion rings
# =========================================================================================================================

def rounded_rect(hx, hy, r, n=4):
    pts = []
    for cx, cy, a0 in ((hx - r, -hy + r, -PI / 2), (hx - r, hy - r, 0), (-hx + r, hy - r, PI / 2), (-hx + r, -hy + r, PI)):
        for i in range(n + 1):
            a = a0 + (PI / 2) * i / n
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def rye_toast(hx=0.040, hy=0.025, h=0.010):
    bm = new()
    D.extrude(bm, rounded_rect(hx, hy, 0.007), 0.0, h, M("RS_F_RyeCrust"), M("RS_F_Rye"), M("RS_F_Rye"))
    return bm


def sprat(L=0.040, w=0.0065, hh=0.0045):
    bm = new()
    D.ellipsoid(bm, (0, 0, 0), hh, w, L, M("RS_F_Sprat"), 0.0, 8, 8)
    xform(bm, ry=PI / 2)
    paint(bm, lambda c, n, m: M("RS_F_SpratDark") if n.z > 0.45 else M("RS_F_Sprat"))
    tail = new()
    D.extrude(tail, [(-L + 0.003, 0.0), (-L - 0.009, -0.0055), (-L - 0.007, 0.0), (-L - 0.009, 0.0055)][::1], -0.0005, 0.0005,
              M("RS_F_SpratDark"), M("RS_F_SpratDark"), M("RS_F_SpratDark"))
    return [bm, tail]


def toast_with_sprats(c, rot):
    """Rye toast (local x along its length) with three sprats along it; returns (toast, [sprats])."""
    t = xform(rye_toast(), loc=(c[0], c[1], 0.0006), rz=rot)
    fishes = []
    for j, (yo, flip, a) in enumerate(((-0.0135, 1, 0.06), (0.0, -1, -0.04), (0.0135, 1, 0.03))):
        for f in sprat():
            xform(f, rz=(0 if flip > 0 else PI) + a)
            xform(f, loc=(0.002 * j, yo, 0.0006 + 0.010 + 0.0045 - 0.0012))
            xform(f, loc=(c[0], c[1], 0), rz=rot)
            fishes.append(f)
    return t, fishes


SALMON_P = (-0.004, -0.030)
SALMON_A = [math.radians(a) for a in (-162, -138, -114, -90, -66)]


def salmon_slices(n_have, plate_floor):
    """The fan of smoked salmon slices draped one over the other; returns the list (first = lowest)."""
    out = []
    for k, a in enumerate(SALMON_A):
        cx, cy = SALMON_P[0] + 0.028 * math.cos(a), SALMON_P[1] + 0.028 * math.sin(a)
        ca, sa = math.cos(a), math.sin(a)
        prev = [o.copy() for o in out]
        acc = new()
        for p in prev:
            merge(acc, p)
        from mathutils.bvhtree import BVHTree
        bvh = BVHTree.FromBMesh(acc) if acc.faces else None

        def base(x, y):
            z = plate_floor(x, y) + 0.0006
            if bvh:
                hit = bvh.ray_cast(Vector((x, y, 0.2)), Vector((0, 0, -1)))
                if hit[0] is not None:
                    z = max(z, hit[0].z + 0.0005)
            return z

        def pos(u, v, cx=cx, cy=cy, ca=ca, sa=sa):
            lx = (u - 0.5) * 0.066
            wf = 0.0185 * max(0.0, 1 - (lx / 0.035) ** 2) ** 0.45 + 0.004
            ly = (v - 0.5) * 2 * wf
            return (cx + lx * ca - ly * sa, cy + lx * sa + ly * ca)

        cache = {}

        def zb(x, y):
            key = (round(x, 7), round(y, 7))
            if key not in cache:
                cache[key] = base(x, y) + 0.0009 * math.sin((x * ca + y * sa) * 140 + k)
            return cache[key]

        s = new()
        K.slab_grid(s, 12, 3, pos, lambda x, y: zb(x, y) + 0.0028, zb,
                    lambda i, j, top: M("RS_F_SalmonFat") if i % 3 == 1 and top else M("RS_F_Salmon"))
        acc.free()
        out.append(s)
    return out[:n_have]


def lemon_wedge(c, rot, L=0.026, R=0.021):
    bm = new()
    rings = [[(-L, 0, 0.0)]]
    for x in lin(-L, L, 9)[1:-1]:
        s = (1 - (x / L) ** 2) ** 0.55
        ring = [(x, 0.0, 0.0)] + [(x, R * s * math.cos(t), R * s * math.sin(t)) for t in lin(0, PI / 3, 6)]
        rings.append(ring)
    rings.append([(L, 0, 0.0)])
    RI, FL = M("RS_F_LemonRind"), M("RS_F_Lemon")
    K.skin(bm, rings, lambda b, i: RI if 1 <= i <= 5 else FL)
    K.fix_normals(bm)
    return xform(bm, loc=c, rz=rot)


def baltic_fish(k, load=False):
    fl = V_DINNER.floor
    parts = []
    # what is still there: (toast2, toast2 bitten, toast1, toast1 bitten, salmon slices, onion on slice 1)
    st = [dict(t2=1, t2b=0, t1=1, t1b=0, sal=5), dict(t2=1, t2b=1, t1=1, t1b=0, sal=4), dict(t2=0, t2b=0, t1=1, t1b=0, sal=3),
          dict(t2=0, t2b=0, t1=1, t1b=1, sal=2), dict(t2=0, t2b=0, t1=0, t1b=0, sal=1)][k]
    for key, c, rot, bite in (("t1", (-0.032, 0.034), 1.30, (-0.021, 0.075)), ("t2", (0.022, 0.036), 1.85, (0.011, 0.0765))):
        if not st[key]:
            continue
        t, fishes = toast_with_sprats(c, rot)
        cuts = [K.bite_cutter(bite[0], bite[1], 0.019, seed=3)] if st[key + "b"] else []
        parts.append(cut(t, cuts, M("RS_F_Rye")))
        for f in fishes:
            parts.append(cut(f, cuts, M("RS_F_Sprat")))
    sal = salmon_slices(5, fl)
    for i, s in enumerate(sal):
        if i < st["sal"]:
            if k == 4 and i == 0:
                s = cut(s, [K.bite_cutter(SALMON_P[0] + 0.056 * math.cos(SALMON_A[0]), SALMON_P[1] + 0.056 * math.sin(SALMON_A[0]), 0.016)],
                        M("RS_F_Salmon"))
            parts.append(s)
        else:
            s.free()
    # red onion rings on the salmon
    from mathutils.bvhtree import BVHTree
    acc = new()
    for p in parts:
        merge(acc, p.copy())
    bvh = BVHTree.FromBMesh(acc)
    for (ox, oy, R, need) in ((-0.028, -0.042, 0.0105, 1), (-0.004, -0.058, 0.0085, 4)):
        if st["sal"] < need:
            continue
        zs = []
        for i in range(12):
            a = 2 * PI * i / 12
            hit = bvh.ray_cast(Vector((ox + R * math.cos(a), oy + R * math.sin(a), 0.2)), Vector((0, 0, -1)))
            zs.append(hit[0].z if hit[0] is not None else fl(ox, oy))
        o = new()
        K.torus(o, (ox, oy, max(zs) + 0.0010), R, 0.0011, M("RS_F_Onion"), 14, 4)
        parts.append(o)
    acc.free()
    parts.append(lemon_wedge((0.070, -0.004, 0.0006), PI / 2 + 0.25))
    if st["t1"]:
        dl = new()
        herb_sprig(dl, (-0.038, 0.016), 1.3, M("RS_F_Dill"), 0.024, 0.0006 + 0.010 + 0.0085)
        parts.append(dl)
    return finish(parts)


def load_baltic(big):
    parts = []
    if not big:
        t = rye_toast(0.013, 0.010, 0.008)
        rest(t, tine_floor, 0.0003)
        parts.append(t)
        top = max(v.co.z for v in t.verts)
        for f in sprat(0.014, 0.0055, 0.0038):
            xform(f, loc=(0.002, 0.0, top + 0.0030))
            parts.append(f)
    else:
        s = new()
        K.slab_grid(s, 6, 3, lambda u, v: ((u - 0.5) * 0.034, (v - 0.5) * 0.024),
                    lambda x, y: D.tine_z(x) + 0.0005 + 0.0045 * math.sin((x + 0.017) / 0.034 * PI) ** 2 + 0.0028,
                    lambda x, y: D.tine_z(x) + 0.0005 + 0.0045 * math.sin((x + 0.017) / 0.034 * PI) ** 2,
                    lambda i, j, top: M("RS_F_SalmonFat") if (i + j) % 3 == 0 and top else M("RS_F_Salmon"))
        parts.append(s)
        o = new()
        K.torus(o, (0.002, 0.003, D.tine_z(0.002) + 0.0005 + 0.0045 + 0.0028 + 0.0008), 0.0070, 0.0009, M("RS_F_Onion"), 12, 4)
        parts.append(o)
    return finish(parts)


# =========================================================================================================================
# 4. ramen (RamenBowl): broth SM_RF_ramen_Fill; noodles, chashu, egg, nori, naruto, spring onion
# =========================================================================================================================

RAMEN_FILL_H = 0.054
RAMEN_FILL = [(0.0285, 0.0), (0.0425, 0.0015), (0.0625, 0.014), (0.0775, 0.034), (0.0856, RAMEN_FILL_H)]
RAMEN_T = [1.0, 0.86, 0.70, 0.54, 0.38]
RAMEN_XY = K.fill_xy(RAMEN_FILL, V_RAMEN)


def ramen_fill():
    bm = new()
    D.lathe_fan(bm, RAMEN_FILL, [M("RS_F_Broth")], 40, cap0=True, cap1=True)
    return bm


def chashu(r=0.026):
    bm = new()
    F, P = M("RS_F_ChashuFat"), M("RS_F_Chashu")
    s = r / 0.026
    prof = [(0.0245 * s, 0.0), (0.0262 * s, 0.0012), (0.0262 * s, 0.0050), (0.0245 * s, 0.0062), (0.0170 * s, 0.0064),
            (0.0135 * s, 0.0064), (0.0060 * s, 0.0064)]
    D.lathe_fan(bm, prof, [F, F, F, P, F, P], 16, cap0=True, cap1=True)
    xform(bm, loc=(0, 0, -0.0032))
    return bm


def egg_half(rx=0.0215, ry=0.0165, rz=0.0155, seg=12):
    bm = new()
    B, W, Y = M("RS_F_EggShellBrown"), M("RS_F_EggWhite"), M("RS_F_Yolk")

    def ring(f, z):
        return [(rx * f * math.cos(2 * PI * i / seg), ry * f * math.sin(2 * PI * i / seg), z) for i in range(seg)]

    rings = [[(0, 0, -rz)]]
    for a in (35, 60, 78, 90):
        t = math.radians(a)
        rings.append(ring(math.sin(t), -rz * math.cos(t)))
    rings += [ring(0.60, 0.0), ring(0.52, 0.0016), ring(0.30, 0.0036), [(0, 0, 0.0042)]]
    K.skin(bm, rings, lambda b, i: B if b < 4 else (W if b == 4 else Y))
    K.fix_normals(bm)
    return bm


def naruto():
    bm = new()
    W, P = M("RS_F_Naruto"), M("RS_F_NarutoPink")
    seg = 16
    r = lambda th, f: 0.0150 * f * (1 + 0.06 * math.sin(8 * th))

    def ring(f, z):
        return [(r(2 * PI * i / seg, f) * math.cos(2 * PI * i / seg), r(2 * PI * i / seg, f) * math.sin(2 * PI * i / seg), z) for i in range(seg)]

    rings = [[(0, 0, -0.002)], ring(1, -0.002), ring(1, 0.002), ring(0.72, 0.002), ring(0.55, 0.002), ring(0.36, 0.002), [(0, 0, 0.002)]]
    K.skin(bm, rings, lambda b, i: P if b in (3, 4) else W)
    K.fix_normals(bm)
    return bm


def noodle_strands(n, h, rs, seed=19):
    """Wavy noodle strands round the broth's surface h (the fixed list: a stage keeps the first n)."""
    bm = new()
    rr = random.Random(seed)
    NO = M("RS_F_Noodle")
    for j in range(16):
        cx, cy = rr.uniform(-0.012, 0.004), rr.uniform(-0.002, 0.016)
        ph0, d = rr.uniform(0, 2 * PI), rr.choice((-1, 1))
        span = rr.uniform(1.2 * PI, 1.9 * PI)
        r0 = rr.uniform(0.012, 0.040)
        fa, pa, fz, pz = rr.uniform(1.0, 2.0), rr.uniform(0, 6.3), rr.uniform(1.5, 3.0), rr.uniform(0, 6.3)
        if j >= n:
            continue
        pts = []
        for i in range(10):
            s = i / 9
            ph = ph0 + d * span * s
            rho = r0 + 0.010 * math.sin(2 * PI * fa * s + pa)
            x, y = cx + rho * math.cos(ph), cy + rho * math.sin(ph)
            lim = 0.80 * rs
            m = math.hypot(x, y)
            if m > lim:
                x, y = x * lim / m, y * lim / m
            pts.append((x, y, h + 0.0012 + 0.0024 * math.sin(2 * PI * fz * s + pz)))
        D.sweep(bm, pts, 0.0016, NO, 4)
    return bm


def ramen(k):
    t = RAMEN_T[k]
    h = t * RAMEN_FILL_H
    rs = K.fill_k(RAMEN_XY, t) * RAMEN_FILL[-1][0]
    parts = [noodle_strands([16, 13, 10, 7, 4][k], h, rs)]
    # chashu A (front right) and B (right), leaning on the wall
    for key, (cx, cy), when, bite_at in (("A", (0.034, -0.032), (0, 1, 2), 2), ("B", (0.046, 0.010), (0, 1), 1)):
        if k not in when:
            continue
        c = chashu()
        a = math.atan2(cy, cx)
        xform(c, ry=-0.36)
        xform(c, loc=(cx, cy, h + 0.0005), rz=a)
        if k == bite_at:
            c = cut(c, [K.bite_cutter(cx - 0.018 * math.cos(a), cy - 0.018 * math.sin(a), 0.013, seed=5)], M("RS_F_Chashu"))
        parts.append(c)
    # the egg halves
    for (cx, cy, rot, when, bite_at) in ((-0.034, -0.034, 0.75, (0, 1, 2, 3), 3), (-0.006, -0.052, 0.25, (0, 1), None)):
        if k not in when:
            continue
        e = egg_half()
        xform(e, rx=-0.15)
        xform(e, loc=(cx, cy, h + 0.0035), rz=rot)
        if k == bite_at:
            e = cut(e, [K.bite_cutter(cx + 0.016, cy - 0.006, 0.013, seed=8)], M("RS_F_EggWhite"))
        parts.append(e)
    # nori, standing at the back against the wall
    if k < 3:
        nr = new()
        K.slab_grid(nr, 3, 3, lambda u, v: (u * 0.062, (v - 0.5) * 0.052), lambda x, y: 0.0004, lambda x, y: -0.0004,
                    lambda i, j, top: M("RS_F_Nori"))
        xform(nr, ry=-(PI / 2 - 0.24))
        a = math.radians(152)
        xform(nr, loc=(0.048 * math.cos(a), 0.048 * math.sin(a), h - 0.026), rz=a)
        # bring it inside the wall where needed
        parts.append(nr)
    if k < 2:
        parts.append(xform(naruto(), loc=(0.010, 0.046, h + 0.0016), rz=0.4, rx=0.06))
    # spring onion rings over everything, floating where they miss
    sur = new()
    D.lathe_fan(sur, [(rs * 0.97, h)], [0], 24, cap0=True, cap1=False)
    on = new()
    rr = random.Random(61)
    pts = []
    for i in range(18):
        a, f = rr.uniform(0, 2 * PI), 0.75 * rs * math.sqrt(rr.random())
        pts.append((f * math.cos(a), f * math.sin(a)))
    pts = pts[:[18, 14, 10, 7, 5][k]]

    def onion(b, p, n, r):
        K.torus(b, (p.x, p.y, p.z + 0.0010), 0.0034, 0.0008, M("RS_F_SpringOnion"), 7, 3, tilt=(r.uniform(-0.3, 0.3), r.uniform(-0.3, 0.3)))

    K.sprinkle(on, parts + [sur], pts, onion, 4)
    sur.free()
    parts.append(on)
    return finish(parts)


def noodle_hang(bm, xo, la, lb, sw, rr):
    """One noodle over the chopsticks' pinch: two legs hanging down on either side (y), its top at the origin."""
    pts = [(xo + sw * 1.4, -0.0042, -la), (xo + sw, -0.0034, -la * 0.6), (xo + sw * 0.4, -0.0026, -0.014), (xo, -0.0018, -0.004),
           (xo, 0.0, 0.0012), (xo, 0.0018, -0.004), (xo - sw * 0.3, 0.0026, -0.014), (xo - sw * 0.8, 0.0034, -lb * 0.6),
           (xo - sw * 1.1, 0.0042, -lb)]
    D.sweep(bm, D.spline(pts, 1), 0.0016, M("RS_F_Noodle"), 4)


def load_ramen(big):
    bm = new()
    rr = random.Random(12 if big else 11)
    n = 5 if big else 4
    for j in range(n):
        noodle_hang(bm, -0.007 + 0.0035 * j + rr.uniform(-0.001, 0.001), rr.uniform(0.050, 0.085), rr.uniform(0.045, 0.080),
                    rr.uniform(-0.006, 0.006), rr)
    if big:
        c = chashu(0.022)
        c = cut(c, [K.halfspace_cutter(0.004)], M("RS_F_Chashu"))
        xform(c, rx=PI / 2)
        xform(c, loc=(0.004, 0.0062, -0.012))
        merge(bm, c)
    return bm


# =========================================================================================================================
# 5. pasta (PastaPlate): tagliatelle in a cream sauce, parmesan
# =========================================================================================================================

def pasta_mound(rho, Hm):
    return Hm * max(0.0, 1 - (rho / 0.052) ** 2) ** 0.8 + 0.004


PASTA_LOOPS = 14


def pasta(k):
    fl = V_PASTA.floor
    CR, PA = M("RS_F_Cream"), M("RS_F_Pasta")
    parts = []
    pool = dollop(0, 0, 0.0004, [0.060, 0.057, 0.052, 0.046, 0.040][k], 0.0048, CR, 21, 26, POOL)
    conform(pool, fl, 0.0005)
    if k >= 2:
        pool = cut(pool, [K.bite_cutter(0.055, -0.020, 0.016, seed=2)] + ([K.bite_cutter(0.045, 0.030, 0.014, seed=4)] if k >= 3 else []), CR)
    parts.append(K.drop_bottom(pool, 0.002))
    Hc = [0.022, 0.017, 0.012, 0.007, 0.0][k]
    if Hc > 0:
        parts.append(K.drop_bottom(dollop(0.002, -0.002, 0.003, 0.034 - 0.003 * k, Hc, CR, 8, 18), 0.004))
    Hm = [0.030, 0.025, 0.019, 0.013, 0.008][k]
    rr = random.Random(5)
    nl = [14, 11, 8, 5, 2][k]
    for j in range(PASTA_LOOPS):
        rho0 = 0.046 - 0.0024 * j
        ph0, d = rr.uniform(0, 2 * PI), rr.choice((-1, 1))
        span = rr.uniform(1.5 * PI, 2.1 * PI)
        fa, pa, fz, pz = rr.uniform(1, 2.2), rr.uniform(0, 6.3), rr.uniform(1.5, 3), rr.uniform(0, 6.3)
        if j >= nl:
            continue
        pts, nrm = [], []
        for i in range(16):
            s = i / 15
            ph = ph0 + d * span * s
            rho = max(0.006, rho0 + 0.007 * math.sin(2 * PI * fa * s + pa))
            z = pasta_mound(rho, Hm) + 0.0016 * math.sin(2 * PI * fz * s + pz) + 0.0010 * j / PASTA_LOOPS
            pts.append((rho * math.cos(ph), rho * math.sin(ph), z))
            slope = -Hm * 0.8 * 2 * rho / 0.052 ** 2 * max(0.05, 1 - (rho / 0.052) ** 2) ** (-0.2)
            n = Vector((-slope * math.cos(ph), -slope * math.sin(ph), 1.0)).normalized()
            nrm.append(n)
        rb = new()
        K.ribbon(rb, pts, nrm, 0.0068, 0.0012, PA)
        conform(rb, fl, 0.0006)
        parts.append(rb)
    # parmesan shavings and grated, pepper, parsley
    gar = new()
    rg = random.Random(31)
    pts = [(0.040 * math.sqrt(rg.random()) * math.cos(a), 0.040 * math.sqrt(rg.random()) * math.sin(a)) for a in
           [rg.uniform(0, 2 * PI) for _ in range(40)]]

    def g(b, p, n, r):
        u = r.random()
        if u < 0.18:
            sh = new()
            C.box(sh, (0, 0, 0), (r.uniform(0.010, 0.014), r.uniform(0.006, 0.009), 0.0010), M("RS_F_Parmesan"))
            merge(b, xform(sh, loc=(p.x, p.y, p.z + 0.0004), rz=r.uniform(0, PI), ry=r.uniform(-0.25, 0.25)))
        elif u < 0.62:
            cube(b, (p.x, p.y, p.z + 0.0003), 0.0018, M("RS_F_Parmesan"), r.uniform(0, PI), r.uniform(0, 1))
        elif u < 0.82:
            cube(b, (p.x, p.y, p.z + 0.0002), 0.0011, M("RS_F_Pepper"), r.uniform(0, PI), r.uniform(0, 1))
        else:
            cube(b, (p.x, p.y, p.z + 0.0002), 0.0022, M("RS_F_Parsley"), r.uniform(0, PI), 0.0)

    K.sprinkle(gar, parts, pts, g, 9)
    parts.append(gar)
    return finish(parts)


def twirl(turns, rho, span, seed):
    bm = new()
    rr = random.Random(seed)
    zc = -0.0006
    npt = int(turns * 10)
    pts, nrm = [], []
    for i in range(npt + 1):
        s = i / npt
        a = -span / 2 + span * s
        psi = 2 * PI * turns * s + 0.4
        r = rho * (0.82 + 0.18 * math.sqrt(max(0.0, 1 - (2 * a / span) ** 2)))
        pts.append((a + 0.0006 * math.sin(psi * 3), r * math.cos(psi), zc + r * math.sin(psi)))
        nrm.append(Vector((0, math.cos(psi), math.sin(psi))))
    # the tail end hangs a little
    K.ribbon(bm, pts, nrm, 0.0062, 0.0012, M("RS_F_Pasta"))
    D.dome(bm, -0.001, 0.0, zc + rho + 0.0003, lambda th: 0.0058 * (1 + 0.12 * math.sin(3 * th)), [(1, 0), (1, .3), (.75, .8), (.4, 1.0), (0, 1.1)],
           0.0032, M("RS_F_Cream"), 12)
    cube(bm, (0.002, 0.002, zc + rho + 0.0036), 0.0016, M("RS_F_Parmesan"), 0.4, 0.3)
    return bm


def load_pasta(big):
    return twirl(3.2, 0.0112, 0.022, 2) if big else twirl(2.0, 0.0092, 0.014, 1)


# =========================================================================================================================
# 6. steak (DinnerPlate): grilled steak, roast potatoes, green beans, pepper sauce
# =========================================================================================================================

STEAK_C, STEAK_ROT = (-0.016, 0.012), 0.25
STEAK_CUTS = [None, 0.036, 0.014, -0.010, -0.033]


def steak_body():
    bm = new()
    rf = lambda th: 1.0 / math.sqrt((math.cos(th) / 0.060) ** 2 + (math.sin(th) / 0.043) ** 2) * (1 + 0.05 * math.sin(2 * th + 0.4) + 0.03 * math.sin(3 * th + 1.1))
    prof = [(1, 0), (1, 0.4), (0.975, 0.74), (0.93, 0.9), (0.86, 0.97), (0.76, 1.0)] + [(f, 1.0 + 0.04 * (1 - f / 0.76)) for f in (0.56, 0.36, 0.16)] + [(0, 1.04)]
    D.dome(bm, 0, 0, 0.0, rf, prof, 0.021, M("RS_F_Steak"), 28, bottom=True)
    ca, sa = math.cos(0.7), math.sin(0.7)
    for n_ in range(-6, 7):
        for off in (0.0, 0.0042):
            p = n_ * 0.0165 + off
            g = bm.verts[:] + bm.edges[:] + bm.faces[:]
            bmesh.ops.bisect_plane(bm, geom=g, plane_co=(p * ca, p * sa, 0), plane_no=(ca, sa, 0))
    paint(bm, lambda c, n, m: M("RS_F_GrillMark") if n.z > 0.8 and ((c.x * ca + c.y * sa) % 0.0165) < 0.0042 else m)
    return bm


def potato(seed, R=0.0145):
    rr = random.Random(seed)
    p = [rr.uniform(0, 6.3) for _ in range(4)]
    bm = K.sphere_blob(new(), (0, 0, 0), R, M("RS_F_Potato"), 12, 7,
                       noise=lambda th, ph: 1 + 0.10 * math.sin(2 * th + p[0]) * math.sin(ph) + 0.07 * math.sin(3 * th + p[1]) + 0.05 * math.cos(2 * ph + p[2]),
                       sz=0.72)
    return bm


def bean(bm, a, b, rr):
    mid = ((a[0] + b[0]) / 2 + rr.uniform(-0.003, 0.003), (a[1] + b[1]) / 2 + rr.uniform(-0.003, 0.003), (a[2] + b[2]) / 2 + 0.001)
    pts = D.spline([a, mid, b], 3)
    D.sweep(bm, pts, lambda s: 0.0031 * (0.55 + 0.45 * math.sin(PI * min(1.0, max(0.0, s * 1.15 - 0.075))) ** 0.4), M("RS_F_Bean"), 6)


POT_POS = [(0.050, -0.047, 3), (0.069, -0.018, 4), (0.026, -0.068, 5), (0.068, 0.020, 6)]


def steak(k):
    fl = V_DINNER.floor
    parts = []
    s = steak_body()
    if STEAK_CUTS[k] is not None:
        s = cut(s, [K.halfspace_cutter(STEAK_CUTS[k], rz=0.12 * (k % 2) - 0.06)], M("RS_F_SteakIn"))
    xform(s, loc=(STEAK_C[0], STEAK_C[1], 0.0006), rz=STEAK_ROT)
    parts.append(K.drop_bottom(s, 0.0015))   # its underside lies on the plate (and over the sauce's pepper)
    # potatoes: state per stage (1 full, 0.5 cut in half, 0 gone)
    state = [[1, 1, 1, 1], [0.5, 1, 1, 1], [0, 1, 1, 1], [0, 0, 0.5, 1], [0, 0, 0, 0.5]][k]
    for (x, y, sd), st_ in zip(POT_POS, state):
        if st_ == 0:
            continue
        p = potato(sd)
        rest(p, lambda xx, yy: 0.0, 0.0)
        if st_ == 0.5:
            p = cut(p, [K.halfspace_cutter(0.0, rz=sd)], M("RS_F_PotatoIn"))
        xform(p, loc=(x, y, 0.0006), rz=sd * 0.7)
        parts.append(p)
    # green beans
    rr = random.Random(8)
    nb = [7, 6, 4, 3, 1][k]
    b = new()
    a = 0.38
    d = Vector((math.cos(a), math.sin(a), 0))
    s_ = Vector((-d.y, d.x, 0))
    c0 = Vector((-0.032, -0.052, 0))
    lay = [(-0.009, 0), (-0.003, 0), (0.003, 0), (0.009, 0), (-0.006, 1), (0.000, 1), (0.006, 1)]
    for i in range(nb):
        off, lv = lay[i]
        L = rr.uniform(0.025, 0.031)
        p0 = c0 + s_ * off - d * L + d * rr.uniform(-0.004, 0.004)
        p1 = c0 + s_ * off + d * L
        z = 0.0006 + 0.0031 + lv * 0.0052
        bean(b, (p0.x, p0.y, z), (p1.x, p1.y, z), rr)
    parts.append(b)
    # pepper sauce
    sc = dollop(-0.060, 0.016, 0.0004, 0.024, 0.0036, M("RS_F_Gravy"), 13, 22, POOL)
    if k >= 2:
        sc = cut(sc, [K.bite_cutter(-0.078, 0.010, 0.013, seed=1)] + ([K.bite_cutter(-0.064, 0.040, 0.012, seed=6)] if k >= 3 else []) +
                 ([K.bite_cutter(-0.050, -0.006, 0.012, seed=7)] if k >= 4 else []), M("RS_F_Gravy"))
    parts.append(sc)
    pep = new()
    rg = random.Random(4)
    K.sprinkle(pep, [sc.copy()], [(-0.060 + rg.uniform(-0.02, 0.02), 0.016 + rg.uniform(-0.02, 0.02)) for _ in range(14)],
               lambda bb, p, n, r: cube(bb, (p.x, p.y, p.z + 0.0002), 0.0016, M("RS_F_Pepper"), r.uniform(0, 3), r.uniform(0, 1)), 2)
    parts.append(pep)
    return finish(parts)


def load_steak(big):
    bm = new()
    if not big:
        poly = [(-0.008, -0.0075), (0.0085, -0.0070), (0.0090, 0.0072), (-0.0080, 0.0078)]
        D.extrude(bm, poly, lambda x, y: D.tine_z(x) + 0.0003, lambda x, y: D.tine_z(x) + 0.0003 + 0.0150, M("RS_F_SteakIn"),
                  M("RS_F_Steak"), M("RS_F_SteakIn"))
        paint(bm, lambda c, n, m: M("RS_F_GrillMark") if n.z > 0.8 and abs(c.x - 0.002) < 0.0022 else (M("RS_F_Steak") if n.y > 0.8 else m))
    else:
        p = potato(5, 0.0125)
        p = cut(p, [K.halfspace_cutter(0.0, rz=PI / 2, ry=0.0)], M("RS_F_PotatoIn"))
        rest(p, tine_floor, 0.0003)
        merge(bm, p)
    return bm


# =========================================================================================================================
# 7. khachapuri (Board): Adjarian boat, cheese, egg, butter; eaten with the hands, tips first
# =========================================================================================================================

KH_L, KH_W, KH_P, KH_H = 0.150, 0.068, 0.85, 0.036


def lens_r(th):
    c, s = abs(math.cos(th)), abs(math.sin(th))
    lo, hi = 0.0, KH_L
    for _ in range(40):
        m = (lo + hi) / 2
        g = KH_W * max(0.0, 1 - (m * c / KH_L) ** 2) ** KH_P - m * s
        if g > 0:
            lo = m
        else:
            hi = m
    return lo


_LENS = {}


def lens_rf(th):
    key = round(th % (2 * PI), 6)
    if key not in _LENS:
        _LENS[key] = lens_r(th)
    return _LENS[key]


KH_PROF = [(1.0, 0.0), (1.0, 0.12), (0.985, 0.35), (0.95, 0.62), (0.90, 0.85), (0.84, 0.97), (0.78, 1.0), (0.72, 0.9),
           (0.67, 0.7), (0.63, 0.5), (0.60, 0.38), (0.5, 0.31), (0.3, 0.30), (0, 0.30)]


def kh_tip_lift(bm):
    for v in bm.verts:
        t = smooth((abs(v.co.x) - 0.075) / 0.075)
        v.co.z *= 1 + 0.40 * t


def kh_bread():
    bm = new()
    D.dome(bm, 0, 0, 0.0, lens_rf, KH_PROF, KH_H, M("RS_F_Bread"), 48, bottom=True)
    kh_tip_lift(bm)
    return bm


def kh_cheese():
    bm = new()
    D.dome(bm, 0, 0, 0.0104, lambda th: 0.665 * lens_rf(th), [(1, 0), (1, .35), (.92, .75), (.75, .95), (.45, 1.0), (0, 1.03)],
           0.0165, M("RS_F_Cheese"), 48, bottom=True)
    paint(bm, lambda c, n, m: M("RS_F_CheeseBrown") if n.z > 0.6 and (math.sin(c.x * 260) * math.sin(c.y * 310 + 1) > 0.55) else m)
    return bm


KH_CUTS = [[], [(0.205, 0.0, 0.075)], [(0.205, 0.0, 0.075), (-0.205, 0.004, 0.072)],
           [(0.180, 0.0, 0.075), (-0.190, 0.004, 0.075), (-0.040, -0.098, 0.036)],
           [(0.142, 0.0, 0.085), (-0.150, 0.004, 0.085), (-0.040, -0.098, 0.036), (0.010, 0.097, 0.040)]]


def khachapuri(k):
    parts = []
    cuts = [K.bite_cutter(x, y, r, seed=i) for i, (x, y, r) in enumerate(KH_CUTS[k])]
    parts.append(cut(kh_bread(), [c.copy() for c in cuts], M("RS_F_Crumb")))
    parts.append(K.drop_bottom(cut(kh_cheese(), [c.copy() for c in cuts], M("RS_F_Cheese")), 0.0112))
    if k <= 1:
        w = dollop(-0.004, 0.002, 0.0262, 0.026, 0.0030, M("RS_F_EggWhite"), 17, 22, POOL)
        parts.append(w)
        y = new()
        D.cap(y, (-0.004, 0.002, 0.0274), 0.0185, M("RS_F_Yolk"), alpha=96, n=4, seg=18, sz=0.62, bottom=True)
        parts.append(y)
    elif k <= 3:
        parts.append(dollop(-0.002, 0.000, 0.0250, [0, 0, 0.030, 0.022][k], 0.0042, M("RS_F_YolkMix"), 23, 22, DOLLOP))
    if k == 0:
        b = new()
        D.pillow(b, rounded_rect(0.011, 0.0095, 0.002, 2), 0.0, 0.0068, M("RS_F_Butter"))
        parts.append(xform(b, loc=(0.030, -0.012, 0.0250), rz=0.5, ry=-0.12))
    elif k == 1:
        b = new()
        D.pillow(b, rounded_rect(0.008, 0.007, 0.002, 2), 0.0, 0.0035, M("RS_F_Butter"))
        parts.append(xform(b, loc=(0.031, -0.012, 0.0262), rz=0.6))
    for c in cuts:
        c.free()
    return finish(parts)


def load_khachapuri(big):
    rx = 0.026 if big else 0.018
    b = new()
    D.ellipsoid(b, (rx, 0, 0.0), rx, 0.0125, 0.0095, M("RS_F_Bread"), 0.0, 10, 6)
    b = cut(b, [K.box_cutter((rx, -0.0230, 0.0), (0.2, 0.04, 0.2), rz=0.08)], M("RS_F_Crumb"))
    ch = new()
    D.ellipsoid(ch, (1.78 * rx, 0.002, 0.0015), 0.0105, 0.0128, 0.0085, M("RS_F_Cheese"), 0.0, 10, 6)
    if big:
        D.ellipsoid(ch, (1.78 * rx + 0.002, 0.003, 0.0085), 0.0050, 0.0055, 0.0018, M("RS_F_YolkMix"), 0.4, 8, 4)
    return finish([b, ch])


# =========================================================================================================================
# 8. olivier (DinnerPlate): a moulded dome with peas and a parsley leaf
# =========================================================================================================================

OL_PROF = [(0.0435, 0.0), (0.0450, 0.003), (0.0452, 0.018), (0.0435, 0.026), (0.0385, 0.032), (0.0290, 0.0365), (0.0160, 0.0390),
           (0.0060, 0.0398)]
OL_CUTS = [[], [((0.040, -0.012, 0.030), 0.020)], [((0.040, -0.012, 0.030), 0.020), ((0.030, 0.022, 0.028), 0.020)],
           [((0.040, -0.012, 0.030), 0.020), ((0.030, 0.022, 0.028), 0.020), ((0.008, -0.010, 0.038), 0.026), ((0.028, 0.0, 0.010), 0.020)],
           [((0.040, -0.012, 0.030), 0.020), ((0.030, 0.022, 0.028), 0.020), ((0.008, -0.010, 0.038), 0.026), ((0.028, 0.0, 0.010), 0.020),
            ((-0.010, 0.012, 0.040), 0.030), ((0.000, -0.026, 0.020), 0.022), ((0.012, 0.004, 0.0), 0.022), ((-0.010, 0.030, 0.012), 0.016)]]
BITS = ["RS_F_Carrot", "RS_F_Pea", "RS_F_Pickle", "RS_F_EggWhite", "RS_F_Sausage", "RS_F_Carrot", "RS_F_Pea"]


def ol_bit(bm, p, n, r):
    kind = BITS[int(r.random() * len(BITS))]
    c = Vector(p) + Vector(n) * 0.0006
    if kind == "RS_F_Pea":
        pea(bm, c, 0.0030)
    else:
        cube(bm, c, r.uniform(0.0038, 0.0048), M(kind), r.uniform(0, PI), r.uniform(0, 1.2))


def olivier(k):
    parts = []
    base = new()
    D.lathe_fan(base, OL_PROF, [M("RS_F_Olivier")], 28, cap0=True, cap1=True)
    xform(base, loc=(0, 0, 0.0006))
    TMP = M("RS_F_Cream")
    balls = OL_CUTS[k]
    inside = lambda p, m=0.0: any((Vector(p) - Vector(c)).length < r + m for c, r in balls)
    base = cut(base, [K.ball_cutter(c, r) for c, r in balls], TMP)
    # bits on the outer surface (a fixed list) and on the scooped faces
    rr = random.Random(13)
    bits = new()
    for i in range(46):
        th = rr.uniform(0, 2 * PI)
        u = rr.random()
        if u < 0.72:
            z = rr.uniform(0.002, 0.034)
            r = interp([(z_, r_) for r_, z_ in OL_PROF], z)
            p = Vector((r * math.cos(th), r * math.sin(th), z + 0.0006))
            n = Vector((math.cos(th), math.sin(th), 0.25)).normalized()
        else:
            r = math.sqrt(rr.random()) * 0.030
            p = Vector((r * math.cos(th), r * math.sin(th), interp([(r_, z_) for r_, z_ in OL_PROF[::-1]], r) + 0.0006))
            n = Vector((0, 0, 1))
        sub = random.Random(rr.random())
        if not inside(p, 0.001):
            ol_bit(bits, p, n, sub)
    cutf = [f for f in base.faces if f.material_index == TMP]
    rc = random.Random(29 + k)
    for f in cutf:
        if f.calc_area() > 1.5e-5 and rc.random() < min(0.9, f.calc_area() / 6e-5):
            ol_bit(bits, f.calc_center_median(), f.normal, rc)
        f.material_index = M("RS_F_Olivier")
    conform(bits, V_DINNER.floor, 0.0003)
    parts += [K.drop_bottom(base, 0.002), bits]
    top = 0.0398 + 0.0006
    if not inside((0, 0, top), 0.004):
        lf = new()
        poly = [(0.013 * (0.55 + 0.45 * abs(math.cos(1.5 * a))) * math.cos(a), 0.010 * (0.55 + 0.45 * abs(math.cos(1.5 * a))) * math.sin(a))
                for a in lin(0, 2 * PI, 25)[:-1]]
        K.prism_layers(lf, poly, [0.0, 0.0008], lambda i, b: M("RS_F_Parsley"), M("RS_F_Parsley"), M("RS_F_Parsley"))
        C.tube(lf, (-0.010, 0, 0.0004), (-0.020, -0.004, 0.0004), 0.0006, M("RS_F_Parsley"), 4)
        parts.append(xform(lf, loc=(0.002, 0.004, top + 0.0003), rz=0.6, ry=0.05))
        for i in range(4):
            a = 2 * PI * i / 4 + 0.3
            p = (0.016 * math.cos(a), 0.016 * math.sin(a))
            zz = interp([(r_, z_) for r_, z_ in OL_PROF[::-1]], 0.016) + 0.0006 + 0.0026
            if not inside((p[0], p[1], zz), 0.002):
                pp = new()
                pea(pp, (p[0], p[1], zz), 0.0032)
                parts.append(pp)
    return finish(parts)


def load_olivier(big):
    bm = new()
    R = 0.0125 if big else 0.0095
    b = K.sphere_blob(new(), (0, 0, R * 0.55), R, M("RS_F_Olivier"), 12, 6, noise=lambda th, ph: 1 + 0.08 * math.sin(3 * th) * math.sin(ph), sz=0.6)
    rest(b, tine_floor, 0.0002)
    merge(bm, b)
    rr = random.Random(5 if big else 4)
    for i in range(6 if big else 4):
        th = 2 * PI * i / (6 if big else 4) + 0.3
        ph = 0.9
        p = Vector((R * 0.85 * math.sin(ph) * math.cos(th), R * 0.85 * math.sin(ph) * math.sin(th), 0))
        p.z = D.tine_z(p.x) + 0.0002 + R * 0.6 * (0.55 + 0.6 * math.cos(ph))
        ol_bit(bm, p, Vector((math.sin(ph) * math.cos(th), math.sin(ph) * math.sin(th), math.cos(ph))), random.Random(rr.random()))
    return bm


# =========================================================================================================================
# 9. medovik (DessertPlate): a honey cake slice, layers on the cut faces
# =========================================================================================================================

MED_APEX, MED_R, MED_HALF = (0.050, 0.0), 0.095, 0.33
MED_CUT = [None, (0.030, 0.15), (0.010, -0.12), (-0.010, 0.10), (-0.028, -0.08)]


def med_levels(n_cake, cake=0.0047, cream=0.0015, top=0.0015):
    zs = [0.0]
    for i in range(n_cake):
        zs.append(zs[-1] + cake)
        if i < n_cake - 1:
            zs.append(zs[-1] + cream)
    zs.append(zs[-1] + top)
    return zs


def med_prism(poly, zs, arcs, zb=None):
    bm = new()
    HC, CC, CR = M("RS_F_HoneyCake"), M("RS_F_CakeCream"), M("RS_F_CakeCrumbs")
    nb = len(zs) - 1

    def side(i, b):
        if arcs[i] or b == nb - 1:
            return CR
        return HC if b % 2 == 0 else CC

    K.prism_layers(bm, poly, zs, side, CR, HC, zb)
    return bm


def medovik(k):
    poly = D.sector(MED_R, PI - MED_HALF, PI + MED_HALF, 9, apex=MED_APEX)
    if MED_CUT[k]:
        c, sl = MED_CUT[k]
        clipper = [(-1, -1), (c - sl * 1, -1), (c + sl * 1, 1), (-1, 1)]
        poly = D.clip_poly(poly, clipper)
    on_arc = lambda p: abs(math.hypot(p[0] - MED_APEX[0], p[1] - MED_APEX[1]) - MED_R) < 1e-5
    arcs = [on_arc(poly[i]) and on_arc(poly[(i + 1) % len(poly)]) for i in range(len(poly))]
    zs = [z + 0.0006 for z in med_levels(8)]
    parts = [med_prism(poly, zs, arcs)]
    # garnish on the plate: two berries and a mint leaf
    g = new()
    for (x, y) in ((-0.034, 0.046), (-0.024, 0.052)):
        K.sphere_blob(g, (x, y, 0.0006 + 0.0048), 0.0048, M("RS_F_Berry"), 10, 6)
    lf = new()
    poly2 = [(0.012 * math.cos(a), 0.0055 * math.sin(a) * (1 - 0.25 * math.cos(a))) for a in lin(0, 2 * PI, 17)[:-1]]
    K.prism_layers(lf, poly2, [0.0, 0.0008], lambda i, b: M("RS_F_Mint"), M("RS_F_Mint"), M("RS_F_Mint"))
    merge(g, xform(lf, loc=(-0.046, 0.034, 0.0010), rz=2.2, ry=0.06))
    parts.append(g)
    return finish(parts)


def load_medovik(big):
    L = 0.024 if big else 0.016
    hw = 0.0085 if big else 0.0065
    poly = [(-L / 2, -hw), (L / 2, -hw * 0.45), (L / 2, hw * 0.45), (-L / 2, hw)]
    zs = med_levels(4 if big else 3, 0.0040, 0.0013, 0.0012)
    return med_prism(poly, zs, [False, False, False, True], zb=lambda x, y: D.tine_z(x) + 0.0003)


# =========================================================================================================================
# 10. plombir (Coupe): three scoops, a fan wafer, berry sauce
# =========================================================================================================================

PL_R = 0.0225
PL_C = [(0.0185 * math.cos(math.radians(a)), 0.0185 * math.sin(math.radians(a)), 0.0235) for a in (-30, 90, 210)]


def scoop_noise(i):
    return lambda th, ph: 1 + 0.045 * math.sin(5 * th + i) * math.sin(3 * ph) + 0.025 * math.sin(9 * th + 2 * i) * math.sin(ph) ** 2


def scoop(i):
    bm = K.sphere_blob(new(), PL_C[i], PL_R, M("RS_F_Plombir"), 16, 9, noise=scoop_noise(i), sz=0.94)
    conform(bm, V_COUPE.floor, 0.0008)
    return bm


def scoop_sauce(i):
    """An open shell of berry sauce over the scoop's top, its lower edge running in drips."""
    bm = new()
    c, R, nz = PL_C[i], PL_R, scoop_noise(i)
    seg = 16
    rings = [[(c[0], c[1], c[2] + R * 0.94 * nz(0, 0) + 0.0011)]]
    for kk in range(1, 5):
        ring = []
        for j in range(seg):
            th = 2 * PI * j / seg
            pe = math.radians(48 + 18 * math.sin(3 * th + i) + (14 if j % 4 == 1 else 0))
            ph = pe * kk / 4
            f = nz(th, ph) * R + 0.0011
            ring.append((c[0] + f * math.sin(ph) * math.cos(th), c[1] + f * math.sin(ph) * math.sin(th), c[2] + f * 0.94 * math.cos(ph)))
        rings.append(ring)
    D.loop_stack(bm, rings[::-1], M("RS_F_BerrySauce"))
    return bm


PL_CUTS = [[], [("A", (0.0, 0.0, 0.016), 0.012)],
           [("A", (0.004, 0.0, 0.006), 0.020), ("W", None, 0.016)],
           [("A", (0.0, 0.0, 0.0), 0.030), ("B", (0.0, 0.0, 0.012), 0.018), ("W", None, 0.08)],
           [("A", (0.0, 0.0, 0.0), 0.030), ("B", (0.0, 0.0, 0.0), 0.032), ("C", (0.0, 0.0, 0.016), 0.019), ("W", None, 0.08)]]
WAFER_A = -(PI / 2 - 0.30)
WAFER_TOP = Vector((0.004 + 0.064 * math.cos(WAFER_A) * math.cos(PI + 0.6), -0.004 + 0.064 * math.cos(WAFER_A) * math.sin(PI + 0.6),
                    0.016 - 0.064 * math.sin(WAFER_A)))


def wafer():
    bm = new()
    W, WD = M("RS_F_Wafer"), M("RS_F_WaferDark")
    K.slab_grid(bm, 6, 4, lambda u, v: (u * 0.064, (v - 0.5) * (0.014 + 0.030 * u)), lambda x, y: 0.0021, lambda x, y: -0.0021,
                lambda i, j, top: W if (i + j) % 2 else WD)
    xform(bm, ry=WAFER_A)
    xform(bm, loc=(0.004, -0.004, 0.016), rz=PI + 0.6)
    return bm


def plombir(k):
    parts = []
    balls = []
    for who, off, r in PL_CUTS[k]:
        if who == "W":
            balls.append(((WAFER_TOP.x, WAFER_TOP.y, WAFER_TOP.z), r))
        else:
            c = PL_C["ABC".index(who)]
            balls.append(((c[0] + off[0], c[1] + off[1], c[2] + off[2]), r))
    wb = [(c, r) for c, r in balls if (Vector(c) - WAFER_TOP).length < 0.001]
    balls = [(c, r) for c, r in balls if (Vector(c) - WAFER_TOP).length >= 0.001]
    inside = lambda p: any((Vector(p) - Vector(c)).length < r for c, r in balls)
    for i in range(3):
        s = cut(scoop(i), [K.ball_cutter(c, r) for c, r in balls], M("RS_F_Plombir"))
        if s.faces:
            parts.append(s)
            sc = K.cull_inside(scoop_sauce(i), inside)
            parts.append(sc)
            top = Vector(PL_C[i]) + Vector((0.002, 0.001, PL_R * 0.94 + 0.0011 + 0.0040))
            if not inside(top):
                parts.append(K.sphere_blob(new(), top, 0.0042, M("RS_F_Berry"), 8, 5))
        else:
            s.free()
    w = cut(wafer(), [K.ball_cutter(c, r) for c, r in wb], M("RS_F_Wafer"))
    if w.faces:
        parts.append(w)
    if k < 3:
        lf = new()
        poly = [(0.010 * math.cos(a), 0.0048 * math.sin(a) * (1 - 0.25 * math.cos(a))) for a in lin(0, 2 * PI, 15)[:-1]]
        K.prism_layers(lf, poly, [0.0, 0.0007], lambda i, b: M("RS_F_Mint"), M("RS_F_Mint"), M("RS_F_Mint"))
        c = PL_C[1]
        parts.append(xform(lf, loc=(c[0] - 0.006, c[1] + 0.004, c[2] + PL_R * 0.94 * 0.93 + 0.0012), rz=0.9, ry=0.35))
    if k == 4:
        pu = dollop(-0.004, -0.006, 0.0003, 0.026, 0.0035, M("RS_F_Plombir"), 7, 22, POOL)
        conform(pu, V_COUPE.floor, 0.0006)
        parts.append(pu)
        ptop = max(v.co.z for v in pu.verts)
        parts.append(dollop(-0.010, -0.009, ptop - 0.0012, 0.0080, 0.0022, M("RS_F_BerrySauce"), 3, 14, POOL))
    return finish(parts)


def load_plombir(big):
    bm = new()
    R = 0.0105 if big else 0.0080
    b = K.sphere_blob(new(), (0, 0, R * 0.55), R, M("RS_F_Plombir"), 12, 7, noise=scoop_noise(2), sz=0.78)
    conform(b, ts_floor, 0.0003)
    merge(bm, b)
    if big:
        D.dome(bm, 0.001, 0.0, R * 0.55 + R * 0.78 - 0.0012, lambda th: 0.0050 * (1 + 0.15 * math.sin(3 * th)),
               [(1, 0), (1, .3), (.7, .8), (0, 1.0)], 0.0016, M("RS_F_BerrySauce"), 12)
    return bm


# =========================================================================================================================
# 11. syrniki (DinnerPlate): three cheese pancakes, sour cream, jam
# =========================================================================================================================

SY_PROF = [(0.0280, 0.0), (0.0305, 0.0018), (0.0315, 0.0060), (0.0310, 0.0110), (0.0295, 0.0145), (0.0260, 0.0166), (0.0180, 0.0172),
           (0.0080, 0.0175)]
SY_POS = [(0.038 * math.cos(math.radians(a)), 0.038 * math.sin(math.radians(a))) for a in (100, 220, 340)]


def syrnik(seed):
    bm = new()
    D.lathe_fan(bm, SY_PROF, [M("RS_F_Syrnik")], 24, cap0=True, cap1=True)
    b = blob(1.0, seed, 0.35)
    for v in bm.verts:
        th = math.atan2(v.co.y, v.co.x)
        f = b(th)
        v.co.x *= f
        v.co.y *= f
    paint(bm, lambda c, n, m: M("RS_F_SyrnikCrust") if abs(n.z) > 0.55 else M("RS_F_Syrnik"))
    return bm


def syrniki(k):
    parts = []
    SI = M("RS_F_SyrnikIn")
    # per syrnik: "full", "notch", "half", "quarter", None
    st = [["full", "full", "full"], ["full", "full", "notch"], ["full", "notch", "half"], ["notch", "half", None],
          ["quarter", None, None]][k]
    for i, ((x, y), s) in enumerate(zip(SY_POS, st)):
        if s is None:
            continue
        p = syrnik(i + 3)
        a = math.atan2(-y, -x) + 0.0          # cuts face the plate's centre... and the eater's side
        a = -0.4 + 0.3 * i
        if s == "notch":
            p = cut(p, [K.box_cutter((0.030 * math.cos(a), 0.030 * math.sin(a), 0.0), (0.040, 0.020, 0.1), rz=a)], SI)
        elif s == "half":
            p = cut(p, [K.halfspace_cutter(0.0, rz=a)], SI)
        elif s == "quarter":
            p = cut(p, [K.halfspace_cutter(0.0, rz=a), K.halfspace_cutter(0.0, rz=a + PI / 2)], SI)
        xform(p, loc=(x, y, 0.0006))
        parts.append(p)
    SC = M("RS_F_SourCream")
    d = dollop(0.050, 0.044, 0.0004, 0.0165, 0.0115, SC, 33, 20)
    d = cut(d, [K.ball_cutter((0.040, 0.034, 0.012), 0.009), K.ball_cutter((0.058, 0.040, 0.014), 0.010),
                K.ball_cutter((0.048, 0.052, 0.008), 0.012), K.ball_cutter((0.050, 0.044, 0.004), 0.016)][:k], SC)
    parts.append(d)
    j = dollop(-0.066, 0.016, 0.0004, 0.0125, 0.0072, M("RS_F_Jam"), 34, 18)
    j = cut(j, [K.ball_cutter((-0.058, 0.010, 0.008), 0.008), K.ball_cutter((-0.068, 0.022, 0.007), 0.009),
                K.ball_cutter((-0.066, 0.014, 0.0), 0.011), K.ball_cutter((-0.064, 0.016, 0.0), 0.016)][:max(0, k - 1)], M("RS_F_Jam"))
    if j.faces:
        parts.append(j)
    for (x, y) in ((-0.060, 0.040), (-0.052, 0.050)):
        parts.append(K.sphere_blob(new(), (x, y, 0.0006 + 0.0046), 0.0046, M("RS_F_Berry"), 10, 6))
    return finish(parts)


def load_syrniki(big):
    p = syrnik(4)
    hw = 0.012 if big else 0.009
    x0 = 0.002 if big else 0.008
    p = cut(p, [K.halfspace_cutter(-x0, rz=PI), K.halfspace_cutter(hw, rz=PI / 2), K.halfspace_cutter(hw, rz=-PI / 2)], M("RS_F_SyrnikIn"))
    lo, hi = K.bbox(p)
    xform(p, loc=(-(lo.x + hi.x) / 2, 0, 0))
    rest(p, tine_floor, 0.0003)
    top = max(v.co.z for v in p.verts)
    parts = [p]
    if big:
        parts.append(dollop(0.002, 0.0, top - 0.0015, 0.0055, 0.0040, M("RS_F_Jam"), 2, 12))
    else:
        parts.append(dollop(0.001, 0.0, top - 0.0015, 0.0050, 0.0042, M("RS_F_SourCream"), 1, 12))
    return finish(parts)


# =========================================================================================================================
# 15. bread (BreadBasket): rye and white slices standing in a row, leaning back
# =========================================================================================================================

BR_LEAN, BR_T = 0.45, 0.012
BR_X0, BR_DX = -0.074, (BR_T + 0.0060) / math.cos(0.45)
BR_N = 6
BR_LEFT = [6, 5, 4, 2, 1]


def slice_poly(white):
    pts = []
    hw = 0.045 if white else 0.043
    if white:                       # a loaf: arched top
        pts += [(hw - 0.006 + 0.006 * math.cos(a), 0.006 + 0.006 * math.sin(a)) for a in lin(-PI / 2, 0, 3)]
        pts += [(hw * math.cos(a), 0.050 + 0.030 * math.sin(a)) for a in lin(0, PI, 11)]
        pts += [(-hw + 0.006 + 0.006 * math.cos(a), 0.006 + 0.006 * math.sin(a)) for a in lin(PI, 1.5 * PI, 3)]
    else:                           # a brick: square with a slightly domed top
        pts += [(hw - 0.005 + 0.005 * math.cos(a), 0.005 + 0.005 * math.sin(a)) for a in lin(-PI / 2, 0, 3)]
        pts += [(hw - 0.005 + 0.005 * math.cos(a), 0.072 + 0.005 * math.sin(a)) for a in lin(0, PI / 2, 3)]
        pts += [(x, 0.077 + 0.003 * math.cos(x / hw * PI / 2)) for x in lin(hw - 0.012, -hw + 0.012, 5)]
        pts += [(-hw + 0.005 + 0.005 * math.cos(a), 0.072 + 0.005 * math.sin(a)) for a in lin(PI / 2, PI, 3)]
        pts += [(-hw + 0.005 + 0.005 * math.cos(a), 0.005 + 0.005 * math.sin(a)) for a in lin(PI, 1.5 * PI, 3)]
    out = []
    for p in pts:
        if not out or (abs(p[0] - out[-1][0]) > 1e-6 or abs(p[1] - out[-1][1]) > 1e-6):
            out.append(p)
    if abs(out[0][0] - out[-1][0]) < 1e-6 and abs(out[0][1] - out[-1][1]) < 1e-6:
        out.pop()
    return out


def bread_slice(white):
    bm = new()
    cr, cu = (M("RS_F_WhiteCrust"), M("RS_F_WhiteCrumb")) if white else (M("RS_F_RyeCrust"), M("RS_F_Rye"))
    D.extrude(bm, slice_poly(white), -BR_T / 2, BR_T / 2, cr, cu, cu)
    return bm


def bread(k):
    parts = []
    fl = lambda x, y: napkin_top(x, y) - BK_IN_Z
    for i in range(BR_LEFT[k]):
        s = bread_slice(i % 2 == 1)
        # slice frame (x across, y up, z thickness) -> basket: x <- thickness, y <- across, z <- up; then lean back (-x)
        m = Matrix(((0, 0, 1, 0), (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 0, 1)))
        xform(s, mat=m)
        xform(s, ry=-BR_LEAN)
        xform(s, loc=(BR_X0 + BR_DX * i, 0.002 * (i % 2), 0.0), rz=0.105 if i % 2 else -0.105)
        rest(s, fl, 0.0006)
        parts.append(s)
    return finish(parts)


def load_bread(big):
    s = bread_slice(False)
    xform(s, rz=-PI / 2)                    # across -> -x..: the slice's height along +x
    lo, hi = K.bbox(s)
    xform(s, loc=(-lo.x, 0, 0))
    s = cut(s, [K.bite_cutter(hi.x - lo.x + 0.004, 0.006, 0.020, -0.1, 0.1, seed=2)], M("RS_F_Rye"))
    xform(s, rx=0.0)
    return s


# =========================================================================================================================
# the build
# =========================================================================================================================

# ---- the second batch (daily special, festival menu, kitchen props): tableware_extra.py, run in this namespace ----------
_XP = os.path.join(os.path.dirname(os.path.abspath(__file__)), "tableware_extra.py")
exec(compile(open(_XP).read(), _XP, "exec"), globals())

TABLEWARE = {
    "SM_RT_DinnerPlate": lambda: plate(0.135, 1.0), "SM_RT_DessertPlate": lambda: plate(0.100, 0.85),
    "SM_RT_SoupPlate": soup_plate, "SM_RT_RamenBowl": ramen_bowl, "SM_RT_PastaPlate": pasta_plate, "SM_RT_Board": board,
    "SM_RT_Coupe": coupe, "SM_RT_CoffeeCup": coffee_cup, "SM_RT_Saucer": saucer, "SM_RT_Tumbler": tumbler,
    "SM_RT_Carafe": carafe, "SM_RT_BreadBasket": bread_basket, "SM_RT_Tray": tray, "SM_RT_Fork": fork, "SM_RT_Knife": knife,
    "SM_RT_Spoon": spoon, "SM_RT_TeaSpoon": teaspoon, "SM_RT_Chopsticks": chopsticks, "SM_RT_Napkin": table_napkin,
    "SM_RT_MenuCard": menu_card, "SM_RT_SaltPepper": salt_pepper,
}
FILLS = {
    "SM_RF_borscht_Fill": borscht_fill, "SM_RF_ramen_Fill": ramen_fill, "SM_RF_coffee_Fill": coffee_fill,
    "SM_RF_mors_Fill": lambda: tumbler_fill("RS_F_Mors"), "SM_RF_water_Fill": lambda: tumbler_fill("RS_F_Water"),
    "SM_RF_carafe_Fill": carafe_fill,
}
K.FLAT |= {"SM_RT_Tumbler", "SM_RT_Tumbler_Glass", "SM_RF_mors_Fill", "SM_RF_water_Fill"}

# dish: (vessel, stage builder, load builder, utensil, fill mesh, fill profile, fill_h, stage levels)
DISHES = {
    "borscht": (V_SOUP, borscht, load_borscht, "spoon", "SM_RF_borscht_Fill", BORSCHT_FILL, BORSCHT_H, BORSCHT_T),
    "pelmeni": (V_PASTA, pelmeni, load_pelmeni, "fork", None, None, None, None),
    "baltic_fish": (V_DINNER, baltic_fish, load_baltic, "fork", None, None, None, None),
    "ramen": (V_RAMEN, ramen, load_ramen, "chopsticks", "SM_RF_ramen_Fill", RAMEN_FILL, RAMEN_FILL_H, RAMEN_T),
    "pasta": (V_PASTA, pasta, load_pasta, "fork", None, None, None, None),
    "steak": (V_DINNER, steak, load_steak, "knife_fork", None, None, None, None),
    "khachapuri": (V_BOARD, khachapuri, load_khachapuri, "fingers", None, None, None, None),
    "olivier": (V_DINNER, olivier, load_olivier, "fork", None, None, None, None),
    "medovik": (V_DESSERT, medovik, load_medovik, "fork", None, None, None, None),
    "plombir": (V_COUPE, plombir, load_plombir, "teaspoon", None, None, None, None),
    "syrniki": (V_DINNER, syrniki, load_syrniki, "fork", None, None, None, None),
    "coffee": (V_CUP, None, None, "cup", "SM_RF_coffee_Fill", COFFEE_PROF, COFFEE_PROF[-1][1], None),
    "mors": (V_TUMBLER, None, None, "cup", "SM_RF_mors_Fill", TUMBLER_FILL, TUMBLER_FILL_H, None),
    "water": (V_TUMBLER, None, None, "cup", "SM_RF_water_Fill", TUMBLER_FILL, TUMBLER_FILL_H, None),
    "carafe_water": (V_CARAFE, None, None, "cup", "SM_RF_carafe_Fill", CARAFE_FILL, CARAFE_FILL_H, None),
    "bread": (None, bread, load_bread, "fingers", None, None, None, None),
}
V_BASKET = K.Vessel("SM_RT_BreadBasket", [(0.0, BK_IN_Z), (0.090, BK_IN_Z)], BK_IN_Z, 0.125, aspect=1.0)
TABLEWARE.update(X_TABLEWARE)
FILLS.update(X_FILLS)
DISHES.update(X_DISHES)
LIQUID = {"borscht", "ramen", "solyanka", "fest_ukha"}
CHECK = {}


def check_food(dish, k, bm):
    V = DISHES[dish][0]
    if dish == "bread":
        worst, n, out = 0.0, 0, 0
        for v in bm.verts:
            d = napkin_top(v.co.x, v.co.y) - BK_IN_Z - v.co.z
            if d > 0.0003:
                n += 1
                worst = max(worst, d)
        res = (worst, n, out)
    else:
        res = V.check(bm)
    CHECK["%s_%d" % (dish, k)] = res
    print("CHECK %-14s %d  sink %.4f m (%d verts)  beyond rim %d" % (dish, k, res[0], res[1], res[2]), flush=True)


def build(names):
    want = set(names)
    allw = not want
    for name, fn in TABLEWARE.items():
        if allw or name in want or "tableware" in want or ("new" in want and name in X_NEW):
            K.export(fn(), name)
    for name, fn in FILLS.items():
        if allw or name in want or "fills" in want or ("new" in want and name in X_NEW):
            K.export(fn(), name)
    for dish, (V, stage, load, *_r) in DISHES.items():
        if not (allw or dish in want or ("new" in want and dish in X_NEW)):
            continue
        if stage:
            for k in range(5):
                bm = stage(k)
                check_food(dish, k, bm)
                K.export(bm, "SM_RF_%s_%d" % (dish, k))
        if load:
            K.export(load(False), "SM_RF_Load_%s_S" % dish)
            K.export(load(True), "SM_RF_Load_%s_B" % dish)


def write_json():
    pt_d, pt_s = V_DINNER.fill_z, V_DESSERT.fill_z
    consts = {
        "DINNER_PLATE_R": 0.135, "DINNER_PLATE_TOP": pt_d, "DINNER_PLATE_FLAT_R": 0.0945,
        "DESSERT_PLATE_R": 0.100, "DESSERT_PLATE_TOP": pt_s,
        "SOUP_PLATE_R": 0.120, "SOUP_PLATE_H": 0.0446, "SOUP_PLATE_IN_Z": V_SOUP.fill_z, "SOUP_PLATE_WELL_R": 0.0885,
        "RAMEN_BOWL_R": 0.095, "RAMEN_BOWL_H": 0.085, "RAMEN_BOWL_IN_Z": V_RAMEN.fill_z, "RAMEN_BOWL_IN_R": 0.0905,
        "PASTA_PLATE_R": 0.140, "PASTA_PLATE_H": 0.0458, "PASTA_PLATE_IN_Z": V_PASTA.fill_z, "PASTA_PLATE_WELL_R": 0.093,
        "BOARD_LX": 2 * BOARD_RX, "BOARD_LY": 2 * BOARD_RY, "BOARD_H": BOARD_H,
        "COUPE_R": 0.056, "COUPE_H": 0.0988, "COUPE_IN_Z": V_COUPE.fill_z, "COUPE_IN_R": 0.0535,
        "CUP_R": CUP_R, "CUP_H": CUP_H, "CUP_HALF_H": CUP_H / 2, "CUP_IN_Z": CUP_IN_Z, "CUP_IN_R": 0.0365,
        "CUP_FILL_H": COFFEE_PROF[-1][1], "CUP_HANDLE_Y": 0.0545,
        "SAUCER_R": 0.072, "SAUCER_TOP": SAUCER_TOP,
        "TUMBLER_R": tumbler_ro(TUMBLER_H), "TUMBLER_H": TUMBLER_H, "TUMBLER_HALF_H": TUMBLER_H / 2, "TUMBLER_IN_Z": TUMBLER_IN_Z,
        "TUMBLER_IN_R": tumbler_ri(TUMBLER_H), "TUMBLER_FILL_H": TUMBLER_FILL_H,
        "CARAFE_R": 0.045, "CARAFE_H": CARAFE_H, "CARAFE_HALF_H": CARAFE_H / 2, "CARAFE_IN_Z": CARAFE_IN_Z,
        "CARAFE_FILL_H": CARAFE_FILL_H, "CARAFE_NECK_R": 0.017,
        "BASKET_LX": 2 * BK_RIM[0] + 2 * BK_RIM_R, "BASKET_LY": 2 * BK_RIM[1] + 2 * BK_RIM_R, "BASKET_H": BK_RIM_Z + BK_RIM_R,
        "BASKET_IN_Z": BK_IN_Z,
        "TRAY_R": TRAY_R, "TRAY_H": TRAY_H, "TRAY_TOP": TRAY_TOP,
        "TINE_X": D.TINE_X, "LOAD_Z": D.LOAD_Z, "FORK_LEN": 0.19, "GRIP_FROM_END": 0.06,
        "SPOON_X": D.TINE_X, "SPOON_LOAD_Z": D.LOAD_Z, "SPOON_LEN": 0.181,
        "TEASPOON_SCALE": TEASPOON_S, "TEASPOON_X": D.TINE_X * TEASPOON_S, "TEASPOON_LOAD_Z": D.LOAD_Z * TEASPOON_S,
        "TEASPOON_LEN": 0.181 * TEASPOON_S,
        "KNIFE_LEN": KNIFE_X1 - KNIFE_X0, "KNIFE_GRIP_FROM_END": -KNIFE_X0, "KNIFE_BLADE_X0": 0.030, "KNIFE_TIP_X": KNIFE_X1,
        "CHOP_LEN": CHOP_TIP_X - CHOP_X0, "CHOP_GRIP_FROM_END": -CHOP_X0, "CHOP_TIP_X": CHOP_TIP_X, "CHOP_TIP_Z": CHOP_R1,
        "CHOP_TIP_GAP": CHOP_GAP,
        "NAPKIN_LX": 0.20, "NAPKIN_LY": 0.11, "NAPKIN_H": 0.0081,
        "MENU_LX": MENU_X, "MENU_LY": MENU_Y, "MENU_H": MENU_H,
        "SALT_PEPPER_R": 0.0165, "SALT_PEPPER_H": 0.0662, "SALT_PEPPER_GAP": SP_GAP,
        "FINGER_LOAD": "origin = the pinch between thumb and index tips, food along +x, +z up",
    }
    consts.update(x_consts())
    dishes = {}
    for dish, (V, stage, load, ut, fill, fprof, fh, levels) in DISHES.items():
        vname = V.name if V else "SM_RT_BreadBasket"
        Vv = V or V_BASKET
        e = {"vessel": vname, "fill_z": round(Vv.fill_z, 5), "fill": fill, "fill_h": round(fh, 5) if fh else None, "utensil": ut,
             "load_s": ("SM_RF_Load_%s_S" % dish) if load else None, "load_b": ("SM_RF_Load_%s_B" % dish) if load else None,
             "stages": 5 if stage else 0, "radius": round({"SM_RT_DinnerPlate": 0.135, "SM_RT_DessertPlate": 0.100,
                                                          "SM_RT_SoupPlate": 0.120, "SM_RT_RamenBowl": 0.095,
                                                          "SM_RT_PastaPlate": 0.140, "SM_RT_Board": BOARD_RX, "SM_RT_Coupe": 0.056,
                                                          "SM_RT_CoffeeCup": CUP_R, "SM_RT_Tumbler": tumbler_ro(TUMBLER_H),
                                                          "SM_RT_Carafe": 0.045, "SM_RT_BreadBasket": BK_RIM[0] + BK_RIM_R, **X_RADIUS}[vname], 4)}
        if ut == "cup":
            e["half_height"] = round(({"SM_RT_CoffeeCup": CUP_H / 2, "SM_RT_Tumbler": TUMBLER_H / 2, "SM_RT_Carafe": CARAFE_H / 2, **X_HALF})[vname], 5)
        if fill:
            e["fill_xy"] = K.fill_xy(fprof, Vv)
        if levels:
            e["stage_fill"] = levels
        if vname == "SM_RT_CoffeeCup":
            e["saucer"], e["saucer_z"] = "SM_RT_Saucer", SAUCER_TOP
        if dish in ("bread", "khachapuri"):
            e["load_frame"] = "fingers"
        dishes[dish] = e
    consts["dishes"] = dishes
    os.makedirs(os.path.dirname(JSON_PATH), exist_ok=True)
    with open(JSON_PATH, "w") as f:
        json.dump(consts, f, indent=2)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    C.reset()
    build(argv)
    write_json()
    tp = os.path.join(ROOT, "design", "restaurant", "game", "tableware_tris.json")
    old = json.load(open(tp)) if os.path.isfile(tp) else {}
    old.update(K.TRIS)
    with open(tp, "w") as f:
        json.dump(dict(sorted(old.items())), f, indent=1)
    print("RS done", len(K.TRIS), "meshes", flush=True)


main()
