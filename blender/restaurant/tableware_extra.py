"""The restaurant's second tableware batch: the weekday daily special, the festival menu and the kitchen props.

Not run on its own: tableware.py executes this file inside its own namespace (just before "the build"), so everything of
tableware.py (K, D, C, M, new, merge, xform, cut, finish, dollop, dill_bit, V_DINNER ...) is in scope here. It defines
X_TABLEWARE, X_FILLS, X_DISHES (same tuple shape as DISHES), X_RADIUS, X_HALF, X_CONSTS for the build and the json.

Dishes (food frame: origin at the vessel's inner floor centre, +fill_z):
  solyanka (SoupPlate, spoon, fill + stage_fill)   golubtsy / draniki / blini_caviar / fried_flounder (DinnerPlate, fork)
  shashlik (Board, fork)       sushi_set (Geta, chopsticks)      fest_ukha (ClayBowl, spoon, fill + stage_fill)
  fest_kulebyaka (DessertPlate, fork)   fest_sbiten (GlassMug, cup)   tea (TeaHolder, cup)
Kitchen props: FryPan (+PanFood), StockPot, Ladle, ServiceBell (+_Plunger), PlateStack, TicketRail, Cloche.
"""

R_ = random.Random


def sgnpow(v, e):
    return math.copysign(abs(v) ** e, v)


def superblob(c, size, e, mi, seg=10, rings=7, rz=0.0, seed=None, amp=0.0):
    """A rounded box (superellipsoid, exponent e: 1 = ellipsoid, 0.3 = cube-ish), poles on z, centred at c."""
    a, b, h = size[0] / 2, size[1] / 2, size[2] / 2
    rr = R_(seed) if seed is not None else None
    ph_ = [rr.uniform(0, 6.3) for _ in range(3)] if rr else [0, 0, 0]
    rings_ = [[(0.0, 0.0, -h)]]
    for k in range(1, rings):
        ph = PI - PI * k / rings
        cz, sr = math.cos(ph), math.sin(ph)
        ring = []
        for i in range(seg):
            th = 2 * PI * i / seg
            f = 1.0 + amp * (math.sin(2 * th + ph_[0]) * sr * 0.6 + 0.4 * math.sin(3 * th + ph_[1] + 2 * ph))
            rf = sr ** e
            ring.append((a * rf * sgnpow(math.cos(th), e) * f, b * rf * sgnpow(math.sin(th), e) * f, h * sgnpow(cz, e)))
        rings_.append(ring)
    rings_.append([(0.0, 0.0, h)])
    bm = new()
    D.loop_stack(bm, rings_, mi)
    return xform(bm, loc=c, rz=rz)


def x_poisson(n, rad, seed, mind, avoid=()):
    rr = R_(seed)
    pts = list(avoid)
    out = []
    for _ in range(6000):
        if len(out) >= n:
            break
        a, f = rr.uniform(0, 2 * PI), math.sqrt(rr.uniform(0.04, 1.0)) * rad
        p = (f * math.cos(a), f * math.sin(a))
        if all(math.hypot(p[0] - q[0], p[1] - q[1]) >= mind for q in pts):
            pts.append(p)
            out.append(p)
    return out


def x_cut_unit(parts, cutters):
    """parts: [(bmesh, inner material)] all cut by the same cutters."""
    return [cut(p, cutters, mi) for p, mi in parts]


def x_clip(pts, x0, x1, y0, y1):
    return D.clip_poly(pts, [(x0, y0), (x1, y0), (x1, y1), (x0, y1)])


def x_ellipse(a, b, n=40, rot=0.0):
    return [(a * math.cos(2 * PI * i / n), b * math.sin(2 * PI * i / n)) for i in range(n)]


def x_sprinkle_dill(parts, pts, seed):
    dl = new()
    K.sprinkle(dl, parts, pts, lambda b, p, n, r: dill_bit(b, p, n, r), seed)
    return dl


# =========================================================================================================================
# vessels: Geta, ClayBowl, GlassMug, TeaHolder
# =========================================================================================================================

GETA_HX, GETA_HY, GETA_FOOT, GETA_PLANK = 0.180, 0.075, 0.012, 0.012
GETA_TOP = GETA_FOOT + GETA_PLANK


class RectVessel(K.Vessel):
    """A flat rectangular top (the geta): rho is 1 at the edge of the rectangle, r_max 1."""

    def __init__(self, name, top, hx, hy):
        super().__init__(name, [(0.0, top), (2.0, top)], top, 1.0)
        self.hx, self.hy = hx, hy

    def rho(self, x, y):
        return max(abs(x) / self.hx, abs(y) / self.hy)


V_GETA = RectVessel("SM_RT_Geta", GETA_TOP, GETA_HX - 0.006, GETA_HY - 0.004)


def geta():
    bm = new()
    W, E = M("RS_T_Wood"), M("RS_T_WoodEdge")
    D.extrude(bm, rounded_rect(GETA_HX, GETA_HY, 0.012), GETA_FOOT, GETA_TOP, E, W, E)
    for sx in (-1, 1):
        C.bx(bm, E, sx * 0.100 - 0.013, sx * 0.100 + 0.013, -0.066, 0.066, 0.0, GETA_FOOT, faces="xXyYz")
    return bm


CL_PTS = [(0.032, 0.002), (0.036, 0.0), (0.043, 0.0), (0.044, 0.006), (0.056, 0.020), (0.070, 0.040), (0.0775, 0.056),
          (0.0790, 0.0625), (0.0780, 0.0650), (0.0745, 0.0650), (0.0735, 0.0615), (0.0725, 0.056), (0.064, 0.036), (0.050, 0.018),
          (0.034, 0.010), (0.0)]
CL_PTS = CL_PTS[:-1]
CL_IN_Z, CL_H, CL_R = 0.010, 0.065, 0.079
V_CLAY = K.Vessel("SM_RT_ClayBowl", [(0.0, 0.010), (0.034, 0.010), (0.050, 0.018), (0.064, 0.036), (0.0725, 0.056), (0.0735, 0.0615)],
                  0.010, 0.0725)


def clay_bowl():
    bm = new()
    CL, GZ = M("RS_T_Clay"), M("RS_T_ClayGlaze")
    mats = [CL, CL, CL, CL, CL, CL, GZ, GZ, GZ, GZ, GZ, GZ, GZ, GZ]
    D.lathe_fan(bm, CL_PTS, mats[:len(CL_PTS) - 1], 36, cap0=True, cap1=True)
    return bm


UKHA_H = 0.040
UKHA_FILL = [(0.033, 0.0), (0.048, 0.006), (0.062, 0.024), (0.0700, UKHA_H)]
UKHA_T = [1.0, 0.82, 0.62, 0.42, 0.24]
UKHA_XY = K.fill_xy(UKHA_FILL, V_CLAY)


def ukha_fill():
    bm = new()
    D.lathe_fan(bm, UKHA_FILL, [M("RS_F_Ukha")], 36, cap0=True, cap1=True)
    return bm


# ---- the glass mug (sbiten) ----------------------------------------------------------------------------------------------
MUG_H, MUG_IN_Z, MUG_FILL_H, MUG_SEG = 0.095, 0.012, 0.070, 20


def mug_ro(z):
    return 0.0375 + 0.0015 * z / MUG_H


def mug_ri(z):
    return mug_ro(z) - 0.0035


V_MUG = K.Vessel("SM_RT_GlassMug", [(0.0, MUG_IN_Z), (mug_ri(MUG_IN_Z), MUG_IN_Z), (mug_ri(MUG_H), MUG_H)], MUG_IN_Z, mug_ri(MUG_H))
MUG_FILL = [(mug_ri(MUG_IN_Z) - 0.0006, 0.0), (mug_ri(MUG_IN_Z + MUG_FILL_H) - 0.0006, MUG_FILL_H)]
MUG_HANDLE_Y = 0.0590


def glass_mug():
    bm = new()
    G, GB = M("RS_T_Glass"), M("RS_T_GlassBase")
    prof = [(mug_ri(0.010), 0.010), (mug_ro(0.010), 0.010), (mug_ro(0.092), 0.092), (mug_ro(0.095) + 0.0003, MUG_H),
            (mug_ri(MUG_H) + 0.0003, MUG_H), (mug_ri(MUG_IN_Z), MUG_IN_Z), (mug_ri(0.010), 0.010)]
    C.lathe(bm, prof, [G], MUG_SEG, cap0=False, cap1=False)
    D.lathe_fan(bm, [(mug_ro(0.0) - 0.0008, 0.0), (mug_ro(0.0008), 0.0008), (mug_ro(0.0065), 0.0065), (mug_ri(0.010) + 0.0006, 0.010),
                     (mug_ri(MUG_IN_Z) + 0.0004, MUG_IN_Z)], [GB], MUG_SEG, cap0=True, cap1=True)
    hp = D.spline([(0.0, 0.0365, 0.080), (0.0, 0.0490, 0.0840), (0.0, 0.0585, 0.0740), (0.0, 0.0590, 0.0500), (0.0, 0.0520, 0.0300),
                   (0.0, 0.0400, 0.0230), (0.0, 0.0368, 0.0245)], 3)
    D.sweep(bm, hp, 0.0036, G, 8)
    return bm


def mug_fill():
    bm = new()
    D.lathe_fan(bm, MUG_FILL, [M("RS_F_Sbiten")], MUG_SEG, cap0=True, cap1=True)
    return bm


# ---- the tea glass in its podstakannik ------------------------------------------------------------------------------------
TG_Z0, TG_H, TG_IN_Z, TG_SEG, TG_FILL_H = 0.004, 0.068, 0.013, 12, 0.047
TH_H = 0.0628


def tg_ro(z):
    return 0.0285 + 0.0020 * (z - TG_Z0) / TG_H


def tg_ri(z):
    return tg_ro(z) - 0.0022


TG_TOP = TG_Z0 + TG_H
V_TEA = K.Vessel("SM_RT_TeaHolder", [(0.0, TG_IN_Z), (tg_ri(TG_IN_Z), TG_IN_Z), (tg_ri(TG_TOP), TG_TOP)], TG_IN_Z, tg_ri(TG_TOP))
TEA_FILL = [(tg_ri(TG_IN_Z) - 0.0006, 0.0), (tg_ri(TG_IN_Z + TG_FILL_H) - 0.0006, TG_FILL_H)]


def tea_holder():
    bm = new()
    G, GB, NI = M("RS_T_Glass"), M("RS_T_GlassBase"), M("RS_T_Nickel")
    ro, ri = tg_ro, tg_ri
    top = TG_TOP
    prof = [(ri(0.010), 0.010), (ro(0.010), 0.010), (ro(top - 0.001), top - 0.001), (ro(top) + 0.0003, top), (ri(top) + 0.0003, top),
            (ri(TG_IN_Z), TG_IN_Z), (ri(0.010), 0.010)]
    C.lathe(bm, prof, [G], TG_SEG, cap0=False, cap1=False)
    D.lathe_fan(bm, [(ro(TG_Z0) - 0.0008, TG_Z0), (ro(TG_Z0 + 0.0008), TG_Z0 + 0.0008), (ro(0.0085), 0.0085),
                     (ri(0.0115) + 0.0006, 0.0115), (ri(TG_IN_Z) + 0.0004, TG_IN_Z)], [GB], TG_SEG, cap0=True, cap1=True)
    # the nickel holder: a foot, an openwork-ish sleeve with two raised bands, its rim, and a ring handle
    sleeve = [(0.026, 0.0), (0.031, 0.0), (0.0335, 0.003), (0.0345, 0.006), (0.0332, 0.009), (0.0332, 0.0175), (0.0346, 0.0195),
              (0.0346, 0.0225), (0.0332, 0.0245), (0.0332, 0.0395), (0.0346, 0.0415), (0.0346, 0.0445), (0.0332, 0.0465),
              (0.0332, 0.0590), (0.0340, 0.0610), (0.0340, TH_H), (0.0321, TH_H), (0.0321, TG_Z0)]
    D.lathe_fan(bm, sleeve, [NI], 24, cap0=True, cap1=True)
    hp = D.spline([(0.0, 0.0334, 0.0560), (0.0, 0.0425, 0.0600), (0.0, 0.0510, 0.0530), (0.0, 0.0545, 0.0400), (0.0, 0.0510, 0.0285),
                   (0.0, 0.0425, 0.0205), (0.0, 0.0334, 0.0185)], 3)
    D.sweep(bm, hp, 0.0022, NI, 6)
    return bm


def tea_fill():
    bm = new()
    D.lathe_fan(bm, TEA_FILL, [M("RS_F_Tea")], TG_SEG, cap0=True, cap1=True)
    return bm


# =========================================================================================================================
# common soup floaters (solyanka, ukha)
# =========================================================================================================================

def x_soup_stage(k, V, FILL, FH, T, XY, kinds, counts, mk, cream, nd, seed, mind):
    t = T[k]
    h = t * FH
    rs = K.fill_k(XY, t) * FILL[-1][0]
    rs0 = XY[-1][1] * FILL[-1][0]
    parts = []
    cpos = (0.012, -0.010)
    avoid = [cpos] if cream else []
    pts = x_poisson(len(kinds), 0.78 * rs0, seed, mind, avoid)
    SC = M("RS_F_SourCream")
    if cream:
        Hd = [0.0105, 0.0090, 0.0068, 0.0048, 0.0030][k]
        Rd = [0.019, 0.018, 0.016, 0.014, 0.011][k]
        d = dollop(0, 0, -0.003, Rd, Hd + 0.003, SC, 5, 20)
        cuts = [K.ball_cutter((0.016, 0.006, 0.010), 0.010), K.ball_cutter((-0.012, 0.012, 0.010), 0.011),
                K.ball_cutter((0.002, -0.016, 0.008), 0.011), K.ball_cutter((-0.016, -0.008, 0.007), 0.010)][:k]
        d = cut(d, cuts, SC)
        parts.append(xform(d, loc=(cpos[0], cpos[1], max(h - 0.0005, 0.0035))))
    for i in range(min(counts[k], len(pts))):
        x, y = pts[i]
        lim = 0.80 * rs
        m = math.hypot(x, y)
        if m > lim:
            x, y = x * lim / m, y * lim / m
        b = mk(kinds[i], i)
        xform(b, loc=(x, y, h), rz=i * 1.7)
        lo = K.lowest_gap(b, V.floor)
        if lo < 0.0005:
            bmesh.ops.translate(b, vec=(0, 0, 0.0005 - lo), verts=b.verts)
        parts.append(b)
    dl = new()
    rr = R_(seed + 5)
    for i in range(nd[k]):
        a, f = rr.uniform(0, 2 * PI), math.sqrt(rr.uniform(0.0, 1.0)) * 0.8 * rs
        dill_bit(dl, (f * math.cos(a), f * math.sin(a), h + 0.0002), (0, 0, 1), rr)
    parts.append(dl)
    return finish(parts)


def x_soup_load(big, mat, kind_fn):
    bm = new()
    L = 0.0042 if big else 0.0030
    top = spoon_liquid(bm, L, M(mat))
    kind_fn(bm, top, big)
    return bm


# =========================================================================================================================
# 1. solyanka (SoupPlate): amber-red broth, olive halves, lemon, sausage, ham, a dollop of sour cream
# =========================================================================================================================

SOLY_T = [1.0, 0.78, 0.56, 0.36, 0.17]
SOLY_KINDS = ["ol", "sa", "ol", "lm", "sa", "ham", "ol", "sa", "ham", "ol", "sa"]
SOLY_COUNT = [11, 9, 7, 4, 2]


def solyanka_fill():
    bm = new()
    D.lathe_fan(bm, BORSCHT_FILL, [M("RS_F_Solyanka")], 40, cap0=True, cap1=True)
    return bm


def soly_item(kind, i):
    bm = new()
    if kind == "ol":
        D.ellipsoid(bm, (0, 0, 0.0008), 0.0075, 0.0058, 0.0046, M("RS_F_Olive"), 0.0, 10, 5)
        xform(bm, rx=0.25 * math.sin(i))
    elif kind == "sa":
        D.lathe_fan(bm, [(0.0085, -0.0020), (0.0085, 0.0028)], [M("RS_F_Sausage")], 14, cap0=True, cap1=True)
        xform(bm, rx=0.18 * math.cos(i * 2), ry=0.18 * math.sin(i))
        paint(bm, lambda c, n, m: M("RS_F_Ham") if n.z > 0.5 and math.hypot(c.x, c.y) < 0.004 else m)
    elif kind == "ham":
        C.box(bm, (0, 0, 0.0004), (0.020, 0.0052, 0.0038), M("RS_F_Ham"))
        xform(bm, ry=0.1)
    else:
        D.lathe_fan(bm, [(0.0150, -0.0014), (0.0150, 0.0014), (0.0128, 0.0014), (0.0030, 0.0014)],
                    [M("RS_F_LemonRind"), M("RS_F_LemonRind"), M("RS_F_Lemon")], 20, cap0=True, cap1=True)
        xform(bm, loc=(0, 0, 0.0004), rx=0.08)
    return bm


def solyanka(k):
    return x_soup_stage(k, V_SOUP, BORSCHT_FILL, BORSCHT_H, SOLY_T, BORSCHT_XY, SOLY_KINDS, SOLY_COUNT, soly_item, True,
                        [14, 11, 8, 5, 3], 71, 0.024)


def load_solyanka(big):
    def kf(bm, top, big):
        D.ellipsoid(bm, (0.004, 0.004, top - 0.0009), 0.0070, 0.0054, 0.0042, M("RS_F_Olive"), 0.3, 8, 4)
        if big:
            strip(bm, (-0.008, -0.004, top - 0.0004), (0.014, 0.0035, 0.0030), M("RS_F_Ham"), 0.4)
            b = soly_item("sa", 1)
            merge(bm, xform(b, loc=(-0.012, 0.007, top - 0.0004)))
        rr = R_(3)
        for i in range(3):
            dill_bit(bm, (-0.014 + 0.007 * i, 0.006 - 0.004 * i, top - 0.0006), (0, 0, 1), rr, 0.004, 0.0005)
    return x_soup_load(big, "RS_F_Solyanka", kf)


# =========================================================================================================================
# 2. golubtsy (DinnerPlate): two cabbage rolls in tomato sauce, sour cream
# =========================================================================================================================

def loaf(L, W, H, mi, n=12, seg=14, seed=0, p=0.6):
    """A blunt-ended oval roll along x, flat underneath (bottom at z = 0, top at 2H)."""
    rr = R_(seed)
    ph = [rr.uniform(0, 6.3) for _ in range(2)]
    rings = [[(-L, 0.0, H)]]
    for i in range(1, n):
        a = PI * i / n
        x = -L * math.cos(a)
        s = math.sin(a) ** p
        ring = []
        for j in range(seg):
            th = 2 * PI * j / seg
            f = 1 + 0.05 * math.sin(2 * th + ph[0] + 3 * a) + 0.03 * math.sin(3 * th + ph[1])
            ring.append((x, W * s * f * math.cos(th), H + H * s * f * math.sin(th)))
        rings.append(ring)
    rings.append([(L, 0.0, H)])
    bm = new()
    K.skin(bm, rings, lambda b, i: mi)
    K.fix_normals(bm)
    if mi == M("RS_F_CabbageRoll"):
        paint(bm, lambda c, n, m: M("RS_F_Cabbage") if math.sin((c.x * 1.0 + c.y * 0.5) * 240) > 0.80 else m)
    return bm


GOL_L, GOL_W, GOL_H = 0.044, 0.0195, 0.0090
GOL_A = [(0.002, 0.028, 0.10), (-0.004, -0.028, -0.08)]
GOL_CUT = [[None, 0.018, -0.010, "gone", "gone"], [None, None, None, 0.018, -0.012]]
GOL_POOL = [0.070, 0.070, 0.064, 0.056, 0.044]


def gol_roll(seed, x0):
    parts = [(loaf(GOL_L, GOL_W, GOL_H, M("RS_F_CabbageRoll"), seed=seed), M("RS_F_Filling")),
             (dollop(0.002, 0.0, 0.0150, 0.0200, 0.0046, M("RS_F_Tomato"), seed + 3, 16), M("RS_F_Tomato"))]
    cuts = [K.halfspace_cutter(x0, rz=0.1 * (seed % 3 - 1))] if x0 is not None else []
    out = [cut(p, cuts, mi) for p, mi in parts]
    return [out[0], K.drop_bottom(out[1], 1.0)]


def golubtsy(k):
    parts = []
    pr = GOL_POOL[k]
    pool = new()
    D.dome(pool, 0, 0, 0.0004, blob(pr, 21, 0.35), POOL, 0.0034, M("RS_F_Tomato"), 28, bottom=True)
    K.drop_bottom(pool, 1.0)
    parts.append(pool)
    for r, (cx, cy, rot) in enumerate(GOL_A):
        x0 = GOL_CUT[r][k]
        if x0 == "gone":
            continue
        for b in gol_roll(4 + r * 3, x0):
            xform(b, loc=(cx, cy, 0.0026), rz=rot)
            parts.append(b)
    SC = M("RS_F_SourCream")
    Rd, Hd = [(0.016, 0.011), (0.0155, 0.010), (0.014, 0.0085), (0.012, 0.0065), (0.009, 0.0045)][k]
    parts.append(dollop(-0.068, 0.0, 0.0004, Rd, Hd, SC, 11, 18))
    dl = x_sprinkle_dill(parts[:], [(R_(5 + i).uniform(-0.05, 0.05), R_(9 + i).uniform(-0.04, 0.04)) for i in range(26)], 8)
    parts.append(dl)
    return finish(parts)


def load_golubtsy(big):
    b = loaf(0.012 if big else 0.0085, 0.0105, 0.0055, M("RS_F_CabbageRoll"), n=6, seg=10, seed=2)
    b = cut(b, [K.halfspace_cutter(0.0, rz=PI)], M("RS_F_Filling")) if False else b
    xform(b, loc=(0.0, 0, 0))
    rest(b, tine_floor, 0.0003)
    parts = [b]
    top = max(v.co.z for v in b.verts)
    parts.append(dollop(0.0, 0.0, top - 0.0020, 0.0070, 0.0045, M("RS_F_Tomato"), 2, 12))
    if big:
        parts.append(dollop(0.012, 0.0, top - 0.0030, 0.0045, 0.0042, M("RS_F_SourCream"), 3, 12))
    return finish(parts)


# =========================================================================================================================
# 3. draniki (DinnerPlate): three potato pancakes, sour cream on the last
# =========================================================================================================================

DR_R = 0.034
DR_POS = [(-0.035, 0.030, 0.4), (0.035, 0.030, 1.3), (0.0, -0.031, 2.2)]          # A (cream on it), B (near), C
DR_PROF = [(1, 0), (1, 0.45), (0.96, 0.80), (0.80, 0.95), (0.45, 1.0), (0, 1.02)]


def pancake(seed, R=DR_R, H=0.0100):
    bm = new()
    rr = R_(seed)
    p = [rr.uniform(0, 6.3) for _ in range(3)]
    rf = lambda th: R * (1 + 0.06 * math.sin(3 * th + p[0]) + 0.035 * math.sin(7 * th + p[1]) + 0.02 * math.sin(11 * th + p[2]))
    D.dome(bm, 0, 0, 0.0, rf, DR_PROF, H, M("RS_F_Draniki"), 22, bottom=True)
    paint(bm, lambda c, n, m: M("RS_F_DranikiDark") if n.z > 0.55 and math.sin(c.x * 430 + seed) * math.sin(c.y * 390 + 1.3) > 0.45 else m)
    return bm


def draniki(k):
    parts = []
    wp = [None, None, None]
    cut_b = [None, -0.00, "gone", "gone", "gone"][k]
    cut_c = [None, None, 0.005, "gone", "gone"][k]
    cut_a = [None, None, None, -0.035, -0.0505][k]
    cuts = [cut_a, cut_b, cut_c]
    pos = [(-0.035, 0.030, 0.4), (0.035, 0.030, 1.3), (0.0, -0.031, 2.2)]
    cutx = {0: cut_a, 1: None, 2: None}
    # B (index 1) is eaten first, then C (2), then A (0)
    state = {1: [None, 0.032, "gone", "gone", "gone"][k], 2: [None, None, -0.004, "gone", "gone"][k], 0: [None, None, None, -0.040, -0.0525][k]}
    cream = None
    for i in (0, 1, 2):
        cx, cy, rot = pos[i]
        st = state[i]
        if st == "gone":
            continue
        pc = pancake(10 + i)
        cs = [K.halfspace_cutter(st, rz=0.1)] if st is not None else []
        xform(pc, loc=(cx, cy, 0.0006), rz=0.0)
        if st is not None:
            pc = cut(pc, [K.halfspace_cutter(st, rz=0.15)], M("RS_F_PotatoIn"))
        parts.append(pc)
    SC = M("RS_F_SourCream")
    if state[0] != "gone":
        d = dollop(-0.030, 0.034, 0.0092, [0.0145, 0.0145, 0.0145, 0.0125, 0.0085][k], [0.0105, 0.0105, 0.0105, 0.009, 0.0065][k], SC, 7, 18)
        if state[0] is not None:
            d = cut(d, [K.halfspace_cutter(state[0], rz=0.15)], SC)
        parts.append(d)
    dl = x_sprinkle_dill(parts[:], [(R_(5 + i).uniform(-0.07, 0.07), R_(9 + i).uniform(-0.06, 0.06)) for i in range(34)], 8)
    parts.append(dl)
    return finish(parts)


def load_draniki(big):
    p = pancake(10)
    hw = 0.0125 if big else 0.0095
    x0 = 0.004 if big else 0.010
    p = cut(p, [K.halfspace_cutter(-x0, rz=PI), K.halfspace_cutter(hw, rz=PI / 2), K.halfspace_cutter(hw, rz=-PI / 2)], M("RS_F_PotatoIn"))
    lo, hi = K.bbox(p)
    xform(p, loc=(-(lo.x + hi.x) / 2, 0, 0))
    rest(p, tine_floor, 0.0003)
    top = max(v.co.z for v in p.verts)
    parts = [p, dollop(0.001, 0.0, top - 0.0015, 0.0052 if big else 0.0046, 0.0042, M("RS_F_SourCream"), 1, 12)]
    return finish(parts)


# =========================================================================================================================
# 4. fried_flounder (DinnerPlate): a whole pan-fried flounder, boiled potatoes with dill, lemon
# =========================================================================================================================

FF_A, FF_B, FF_C = 0.075, 0.053, (-0.012, 0.014)
FF_POT = [(-0.050, -0.072, 31), (-0.012, -0.080, 32), (0.026, -0.078, 33)]
FF_ORDER = ["RL", "RU", "FL", "FU"]                      # rear lower, rear upper, front lower, front upper (eaten from +x)
FF_STATE = [dict(RL=1, RU=1, FL=1, FU=1), dict(RL=0.5, RU=1, FL=1, FU=1), dict(RL=0, RU=0.5, FL=1, FU=1),
            dict(RL=0, RU=0, FL=0.5, FU=1), dict(RL=0, RU=0, FL=0, FU=0.4)]
FF_POTS = [3, 3, 2, 1, 1]
FF_PROF = ((1, 0), (1, 0.30), (0.92, 0.72), (0.60, 0.96), (0, 1.0))


def ff_flesh(poly, h, mi=None, z0=0.0035):
    bm = new()
    D.pillow(bm, poly, z0, h, M("RS_F_Fish"), FF_PROF)
    paint(bm, lambda c, n, m: m)
    return bm


def ff_bones():
    bm = new()
    BO = M("RS_F_Bone")
    z = 0.0058
    C.tube(bm, (-0.060, 0, z), (0.062, 0, z), 0.0016, BO, 5)
    for i in range(-4, 5):
        x = i * 0.0125
        w = FF_B * 0.80 * math.sqrt(max(0.0, 1 - (x / FF_A) ** 2))
        for s in (1, -1):
            C.tube(bm, (x, s * 0.002, z), (x + 0.004, s * w, z - 0.0016), 0.0008, BO, 5)
    return bm


def fried_flounder(k):
    parts = []
    full = x_ellipse(FF_A, FF_B, 44)
    # the lower skin (what the bones lie on) with the frill of fins
    base = new()
    D.pillow(base, x_ellipse(FF_A * 1.07, FF_B * 1.10, 44), 0.0006, 0.0050, M("RS_F_FishSkinDark"), ((1, 0), (1, 0.5), (0.9, 0.9), (0.5, 1.0), (0, 1.0)))
    fin = new()
    D.pillow(fin, [(0.070, -0.012), (0.104, -0.024), (0.110, 0.0), (0.104, 0.024), (0.070, 0.012)], 0.0030, 0.0035, M("RS_F_FishFin"))
    tail = ff_flesh(x_clip(full, 0.058, 1, -1, 1), 0.0075)
    head = ff_flesh(x_clip(full, -1, -0.058, -1, 1), 0.0125)
    eyes = new()
    for s in (1, -1):
        K.sphere_blob(eyes, (-0.066, s * 0.010, 0.0035 + 0.0125 * 1.0 + 0.001), 0.0030, M("RS_F_Pepper"), 6, 4)
    quarters = {"RL": x_clip(full, 0.0035, 0.0525, -1, -0.0045), "RU": x_clip(full, 0.0035, 0.0525, 0.0045, 1),
                "FL": x_clip(full, -0.0525, -0.0035, -1, -0.0045), "FU": x_clip(full, -0.0525, -0.0035, 0.0045, 1)}
    # (the part the bite takes: its corner towards +x)
    bites = {"RL": (0.050, -0.030, 0.026), "RU": (0.050, 0.030, 0.026), "FL": (-0.005, -0.035, 0.026), "FU": (-0.005, 0.034, 0.026)}
    present = [x_clip(full, 0.058, 1, -1, 1), x_clip(full, -1, -0.058, -1, 1)]
    present += [quarters[key] for key in FF_ORDER if FF_STATE[k][key] == 1]
    base.normal_update()

    def inside(p, poly):
        n_, ok = len(poly), False
        for i in range(n_):
            (x1, y1), (x2, y2) = poly[i], poly[(i + 1) % n_]
            if (y1 > p.y) != (y2 > p.y) and p.x < x1 + (p.y - y1) * (x2 - x1) / (y2 - y1):
                ok = not ok
        return ok

    part = []                                # bitten quarters: only what the bite left (outside the bite circle) is covered
    for key in FF_ORDER:
        st_ = FF_STATE[k][key]
        if 0 < st_ < 1:
            bx_, by_, br_ = bites[key]
            part.append((quarters[key], bx_, by_, br_ * (0.85 if st_ == 0.5 else 1.15) + 0.004))

    def covered(p):
        if any(inside(p, q) for q in present):
            return True
        return any(inside(p, q) and math.hypot(p.x - bx_, p.y - by_) > r_ for q, bx_, by_, r_ in part)

    bmesh.ops.delete(base, geom=[f for f in base.faces if f.normal.z > 0.5 and (covered(f.calc_center_median()) or any(covered(v.co) for v in f.verts))], context="FACES")
    fish = [base, K.drop_bottom(fin, 1.0), K.drop_bottom(tail, 1.0), K.drop_bottom(head, 1.0), eyes, ff_bones()]
    for key in FF_ORDER:
        st = FF_STATE[k][key]
        if st == 0:
            continue
        q = ff_flesh(quarters[key], 0.0115)
        if st < 1:
            bx, by, br = bites[key]
            q = cut(q, [K.bite_cutter(bx, by, br * (0.85 if st == 0.5 else 1.15), seed=3)], M("RS_F_FishIn"))
        fish.append(K.drop_bottom(q, 1.0))
    for p in fish:
        xform(p, loc=(FF_C[0], FF_C[1], 0.0), rz=0.12)
        parts.append(p)
    for i in range(FF_POTS[k]):
        x, y, sd = FF_POT[i]
        p = potato(sd, 0.0135)
        setmat(p, M("RS_F_PotatoIn"))
        rest(p, lambda xx, yy: 0.0, 0.0)
        if k == 4:
            p = cut(p, [K.halfspace_cutter(0.0, rz=sd)], M("RS_F_Potato"))
        xform(p, loc=(x, y, 0.0006), rz=sd * 0.7)
        parts.append(p)
    pots = [p.copy() for p in parts[len(fish):]]
    if pots:
        parts.append(x_sprinkle_dill(pots, [(R_(i).uniform(-0.06, 0.04), R_(i + 40).uniform(-0.09, -0.065)) for i in range(18)], 4))
        for p in pots:
            p.free()
    lem = lemon_wedge((0.062, -0.040, 0.0006), PI / 2 + 0.30, 0.019, 0.015)
    parts.append(lem)
    return finish(parts)


def load_fried_flounder(big):
    bm = new()
    poly = rounded_rect(0.0090 if not big else 0.0115, 0.0060 if not big else 0.0075, 0.0035, 3)
    D.pillow(bm, poly, 0.0, 0.0070 if not big else 0.0085, M("RS_F_FishIn"), FF_PROF, zb=lambda x, y: D.tine_z(x) + 0.0003)
    paint(bm, lambda c, n, m: M("RS_F_Fish") if n.z > 0.75 and c.x < 0.002 else m)
    if big:
        p = potato(5, 0.0085)
        setmat(p, M("RS_F_PotatoIn"))
        xform(p, loc=(0.013, 0.0, 0.0))
        rest(p, tine_floor, 0.0003)
        merge(bm, p)
    return bm


# =========================================================================================================================
# 5. shashlik (Board): pork pieces on a long skewer over lavash, onion rings, tomato halves
# =========================================================================================================================

SH_Z = 0.0053
SH_PX = [-0.095 + 0.040 * i for i in range(6)]
SH_PIECES = [6, 5, 4, 2, 1]
SH_ONION = [5, 5, 4, 3, 1]
SH_TOM = [2, 2, 2, 1, 0]
LAV_HX, LAV_HY, LAV_TOP = 0.125, 0.052, 0.0030


def lav_top(x, y):
    e = min(1.0, (1 - abs(x) / LAV_HX) * 10, (1 - abs(y) / LAV_HY) * 10)
    return 0.0006 + LAV_TOP * 0.55 + 0.0013 * math.sin(x * 55 + y * 20) * (0.5 + 0.5 * e) + 0.0007 * math.sin(y * 90 - x * 30) + (0.0016 if e > 0.9 else 0.0004 + 0.0012 * e)


def lavash():
    bm = new()
    LV, LT = M("RS_F_Lavash"), M("RS_F_LavashToast")
    K.slab_grid(bm, 24, 8, lambda u, v: ((u - 0.5) * 2 * LAV_HX, (v - 0.5) * 2 * LAV_HY), lambda x, y: lav_top(x, y),
                lambda x, y: 0.0003, lambda i, j, top: LT if top and (i % 6 == 2 and j % 3 == 1) else LV)
    return bm


def pork_piece(i):
    bm = superblob((0, 0, 0), (0.033 - 0.002 * (i % 2), 0.030, 0.025), 0.55, M("RS_F_Pork"), 10, 7, rz=0.2 * (i - 2), seed=40 + i, amp=0.05)
    paint(bm, lambda c, n, m: M("RS_F_PorkChar") if math.sin(c.x * 410 + i * 3) * math.sin(c.y * 380 + c.z * 200) > 0.55 else m)
    return bm


def onion_ring(R, r=0.0016):
    bm = new()
    K.torus(bm, (0, 0, 0), R, r, M("RS_F_Onion"), 14, 4)
    return bm


def tomato_half(R=0.018):
    bm = new()
    prof, hf = D.cap_prof(90, 4)
    D.dome(bm, 0, 0, 0.0, lambda th: R, prof, R * 0.85, M("RS_F_Tomato"), 14, bottom=True)
    dill = new()
    K.sphere_blob(dill, (0, 0, R * 0.85 + 0.0005), 0.0030, M("RS_F_Dill"), 6, 3)
    merge(bm, dill)
    return bm


def shashlik(k):
    parts = [lavash()]
    sk = new()
    ST_ = M("RS_T_Steel")
    path = [(-0.150, 0, 0.0022), (-0.128, 0, SH_Z), (0.125, 0, SH_Z), (0.150, 0, 0.0022)]
    D.sweep(sk, D.spline(path, 4), 0.0016, ST_, 6)
    K.torus(sk, (-0.158, 0.0, 0.0017), 0.0072, 0.0014, ST_, 14, 4)
    parts.append(sk)
    for i in range(SH_PIECES[k]):
        p = pork_piece(i)
        xform(p, loc=(SH_PX[i], 0.0, LAV_TOP + 0.0012 + 0.0125))
        parts.append(p)
    ons = [(-0.075, -0.037, 0.0), (-0.047, -0.040, 0.2), (-0.018, -0.037, 0.0), (-0.080, 0.037, 0.4), (-0.052, 0.040, 0.0)]
    for i in range(SH_ONION[k]):
        x, y, t = ons[i]
        o = onion_ring(0.0105 - 0.0008 * (i % 2))
        xform(o, loc=(x, y, LAV_TOP + 0.0036), rx=t * 0.2)
        parts.append(o)
    toms = [(0.060, 0.038), (0.098, -0.037)]
    for i in range(SH_TOM[k]):
        x, y = toms[i]
        t = tomato_half()
        if k == 3 and i == 0:
            t = cut(t, [K.halfspace_cutter(0.004, rz=0.5)], M("RS_F_Tomato"))
        xform(t, loc=(x, y, LAV_TOP + 0.0010), rz=i * 1.3)
        parts.append(t)
    return finish(parts)


def load_shashlik(big):
    bm = new()
    p = pork_piece(1 if big else 3)
    if not big:
        xform(p, scale=(0.80, 0.80, 0.80))
    xform(p, rz=0.0)
    rest(p, tine_floor, 0.0003)
    merge(bm, p)
    if big:
        o = onion_ring(0.0085)
        xform(o, loc=(0.0, 0.0, max(v.co.z for v in bm.verts) + 0.0008), rx=0.1)
        merge(bm, o)
    return bm


# =========================================================================================================================
# 6. sushi_set (Geta): 8 maki, 4 nigiri, ginger, wasabi, a soy dish
# =========================================================================================================================

MAKI_R, MAKI_H = 0.0145, 0.025
MAKI_FILL = ["RS_F_Salmon", "RS_F_Cucumber", "RS_F_Tuna", "RS_F_Salmon", "RS_F_Cucumber", "RS_F_Salmon", "RS_F_Tuna", "RS_F_Cucumber"]
MAKI_POS = [(-0.145 + 0.032 * i, 0.040) for i in range(4)] + [(-0.145 + 0.032 * i, -0.040) for i in range(4)]
NIG_POS = [(0.008, 0.026, 0.0), (0.056, 0.026, 0.1), (0.008, -0.026, 0.0), (0.056, -0.026, -0.1)]
NIG_TOP = ["RS_F_Salmon", "RS_F_Tuna", "RS_F_Tamago", "RS_F_Salmon"]
SUSHI_REMOVE = ["N0", "M0", "N1", "M4", "M1", "N2", "M5", "M2", "N3", "M6", "M3"]      # M7 is the last remnant
SUSHI_GONE = [0, 3, 6, 9, 11]


def maki(fill, up=True):
    bm = new()
    NO, RI = M("RS_F_Nori"), M("RS_F_Rice")
    H = MAKI_H
    D.lathe_fan(bm, [(MAKI_R, 0.0), (MAKI_R, H), (MAKI_R - 0.0022, H), (0.0050, H + 0.0006)],
                [NO, NO, RI], 16, cap0=True, cap1=True)
    # the filling: the apex fan (r < 0.005) takes the filling colour
    c = M(fill)
    top_c = H + 0.0006
    for f in bm.faces:
        if f.calc_center_median().z > top_c - 1e-5 and math.hypot(f.calc_center_median().x, f.calc_center_median().y) < 0.0045:
            f.material_index = c
    return bm


def nigiri(top):
    bm = new()
    rice = superblob((0, 0, 0.0075), (0.040, 0.018, 0.015), 0.6, M("RS_F_Rice"), 10, 6, seed=3, amp=0.03)
    merge(bm, rice)
    t = new()
    D.pillow(t, rounded_rect(0.0225, 0.0115, 0.0055, 3), 0.0128, 0.0070, M(top), ((1, 0), (1, 0.35), (0.9, 0.85), (0.6, 1.0), (0, 1.05)))
    if top == "RS_F_Salmon":
        paint(t, lambda c, n, m: M("RS_F_SalmonFat") if n.z > 0.6 and int((c.x + 0.05) / 0.006) % 2 else m)
    merge(bm, t)
    if top == "RS_F_Tamago":
        C.box(bm, (0.0, 0.0, 0.0107), (0.0065, 0.0215, 0.0207), M("RS_F_Nori"))
    return bm


def soy_dish():
    bm = new()
    D.lathe_fan(bm, [(0.016, 0.0), (0.021, 0.0015), (0.0275, 0.0095), (0.0285, 0.0125), (0.0262, 0.0125), (0.0245, 0.0096), (0.0185, 0.0052)],
                [M("RS_T_ChinaBlack")], 22, cap0=True, cap1=False)
    D.lathe_fan(bm, [(0.0220, 0.0072)], [M("RS_F_Soy")], 22, cap0=True, cap1=False)
    return bm


def sushi_set(k):
    parts = []
    gone = set(SUSHI_REMOVE[:SUSHI_GONE[k]])
    for i, (x, y) in enumerate(MAKI_POS):
        if "M%d" % i in gone:
            continue
        m = maki(MAKI_FILL[i])
        xform(m, loc=(x, y, 0.0), rz=0.3 * i)
        parts.append(m)
    for i, (x, y, r) in enumerate(NIG_POS):
        if "N%d" % i in gone:
            continue
        parts.append(xform(nigiri(NIG_TOP[i]), loc=(x, y, 0.0), rz=r))
    dish = new()
    D.lathe_fan(dish, [(0.016, 0.0), (0.021, 0.0015), (0.0275, 0.0095), (0.0285, 0.0125), (0.0262, 0.0125), (0.0245, 0.0096), (0.0185, 0.0052)],
                [M("RS_T_ChinaBlack")], 22, cap0=True, cap1=True)
    soy = new()
    D.lathe_fan(soy, [(0.0222, 0.0074)], [M("RS_F_Soy")], 22, cap0=True, cap1=False)
    # the soy's surface lies just inside the dish's wall
    parts.append(xform(dish, loc=(0.132, -0.032, 0.0)))
    parts.append(xform(soy, loc=(0.132, -0.032, 0.0)))
    f = [1.0, 1.0, 0.82, 0.6, 0.35][k]
    g = dollop(0.118, 0.036, 0.0, 0.020 * f, 0.013 * f, M("RS_F_Ginger"), 5, 14)
    parts.append(g)
    wf = 1.0 if k < 3 else 0.7
    parts.append(dollop(0.152, 0.040, 0.0, 0.0090 * wf, 0.0115 * wf, M("RS_F_Wasabi"), 6, 12))
    return finish(parts)


def load_sushi(big):
    if not big:
        m = maki("RS_F_Salmon")
        xform(m, ry=PI / 2)
        xform(m, loc=(-MAKI_H / 2 + 0.010, 0.0, -MAKI_R + 0.0025))
        return m
    n = nigiri("RS_F_Salmon")
    xform(n, loc=(0.004, 0.0, -0.0145 + 0.0035))
    return n


# =========================================================================================================================
# 7. blini_caviar (DinnerPlate): three folded blini, sour cream and red caviar on each
# =========================================================================================================================

BL_R = 0.052
BL_D = 0.012
BL_ANG = [math.radians(30), math.radians(270), math.radians(150)]          # A (eaten first), C, B
BL_CUT = [[None, 0.026, "gone", "gone", "gone"], [None, None, 0.028, "gone", "gone"], [None, None, None, 0.030, 0.014]]


def blin_unit(x0, seed):
    """One folded blin (apex at the origin, opening along +x) with sour cream and caviar; x > x0 taken away."""
    poly = D.sector(BL_R, -math.radians(45), math.radians(45), 10, apex=(0, 0))
    bl = new()
    D.pillow(bl, poly, 0.0, 0.0125, M("RS_F_Blin"), ((1, 0), (1, 0.42), (0.94, 0.85), (0.70, 1.0), (0, 1.08)))
    paint(bl, lambda c, n, m: M("RS_F_BlinSpot") if n.z > 0.6 and math.sin(c.x * 300 + seed) * math.sin(c.y * 310 + 1.3) > 0.5 else m)
    sc = dollop(0.019, -0.004, 0.0108, 0.0105, 0.0070, M("RS_F_SourCream"), seed, 14)
    cv = dollop(0.037, 0.003, 0.0100, 0.0125, 0.0085, M("RS_F_Caviar"), seed + 1, 14)
    cuts = [K.halfspace_cutter(x0, rz=0.12)] if x0 is not None else []
    parts = [cut(bl, cuts, M("RS_F_BlinIn")), K.drop_bottom(cut(sc, cuts, M("RS_F_SourCream")), 1.0),
             K.drop_bottom(cut(cv, cuts, M("RS_F_Caviar")), 1.0)]
    # beads on the caviar: rays from above onto its surface
    bd = new()
    rr = R_(seed)
    pts = [(0.037 + 0.011 * math.sqrt(rr.random()) * math.cos(a), 0.003 + 0.011 * math.sqrt(rr.random()) * math.sin(a))
           for a in [rr.uniform(0, 2 * PI) for _ in range(12)]]
    K.sprinkle(bd, [parts[2]], pts, lambda b, p, n, r: K.sphere_blob(b, (p.x, p.y, p.z + 0.0004), 0.0019, M("RS_F_CaviarBead"), 6, 4), seed)
    parts.append(bd)
    return parts


def blini_caviar(k):
    parts = []
    for i, ang in enumerate(BL_ANG):
        x0 = BL_CUT[i][k]
        if x0 == "gone":
            continue
        ca, sa = math.cos(ang), math.sin(ang)
        for p in blin_unit(x0, 4 + 3 * i):
            xform(p, loc=(BL_D * ca, BL_D * sa, 0.0006), rz=ang)
            parts.append(p)
    return finish(parts)


def load_blini(big):
    bm = new()
    poly = rounded_rect(0.0085 if not big else 0.0115, 0.0075 if not big else 0.0090, 0.0035, 3)
    D.pillow(bm, poly, 0.0, 0.0090, M("RS_F_Blin"), ((1, 0), (1, 0.4), (0.9, 0.85), (0.6, 1.0), (0, 1.06)), zb=lambda x, y: D.tine_z(x) + 0.0003)
    top = max(v.co.z for v in bm.verts)
    merge(bm, dollop(-0.002, 0.0, top - 0.0015, 0.0050, 0.0042, M("RS_F_SourCream"), 1, 12))
    if big:
        merge(bm, dollop(0.004, 0.0, top - 0.0015, 0.0050, 0.0045, M("RS_F_Caviar"), 2, 12))
    return bm


# =========================================================================================================================
# 8. fest_ukha (ClayBowl): salmon chunks, potato, dill in a golden broth
# =========================================================================================================================

UKHA_KINDS = ["sa", "po", "sa", "po", "sa", "po", "sa", "po", "di", "di"]
UKHA_COUNT = [10, 8, 6, 4, 2]


def ukha_item(kind, i):
    bm = new()
    if kind == "sa":
        s = superblob((0, 0, 0.0012), (0.025, 0.018, 0.013), 0.55, M("RS_F_Salmon"), 10, 6, seed=60 + i, amp=0.05)
        paint(s, lambda c, n, m: M("RS_F_SalmonFat") if n.z > 0.5 and int((c.x + 0.05) / 0.005) % 2 else m)
        merge(bm, s)
    elif kind == "po":
        merge(bm, superblob((0, 0, 0.0008), (0.016, 0.016, 0.014), 0.5, M("RS_F_PotatoIn"), 8, 5, seed=70 + i, amp=0.04))
    else:
        herb_sprig(bm, (-0.012, 0.0), 0.3, M("RS_F_Dill"), 0.026, 0.0004)
    return bm


def fest_ukha(k):
    return x_soup_stage(k, V_CLAY, UKHA_FILL, UKHA_H, UKHA_T, UKHA_XY, UKHA_KINDS, UKHA_COUNT, ukha_item, False,
                        [18, 14, 10, 6, 3], 83, 0.032)


def load_ukha(big):
    def kf(bm, top, big):
        rr = R_(4)
        for i in range(3):
            dill_bit(bm, (-0.012 + 0.008 * i, 0.004 - 0.004 * i, top - 0.0006), (0, 0, 1), rr, 0.004, 0.0005)
        if big:
            merge(bm, superblob((0.002, 0.0, top - 0.0004 + 0.0030), (0.019, 0.013, 0.0090), 0.55, M("RS_F_Salmon"), 8, 5, seed=1))
            merge(bm, superblob((-0.011, 0.002, top - 0.0004 + 0.0025), (0.012, 0.012, 0.0075), 0.5, M("RS_F_PotatoIn"), 8, 5, seed=2))
    return x_soup_load(big, "RS_F_Ukha", kf)


# =========================================================================================================================
# 9. fest_kulebyaka (DessertPlate): a slice of the layered fish, rice and egg pie
# =========================================================================================================================

KB_APEX, KB_R, KB_HALF = (0.050, 0.0), 0.088, 0.30
KB_CUT = [None, (0.026, 0.12), (0.006, -0.10), (-0.014, 0.09), (-0.034, -0.07)]
KB_ZS = [0.0, 0.0032, 0.0112, 0.0202, 0.0282]
KB_TOP = 0.0120


def kb_layers(poly, arcs, z0=0.0006):
    bm = new()
    PA, RI, SA, EG, YO = M("RS_F_Pastry"), M("RS_F_KbRice"), M("RS_F_Salmon"), M("RS_F_EggWhite"), M("RS_F_Yolk")
    mats = [PA, RI, SA, EG]
    nb = len(KB_ZS) - 1

    def side(i, b):
        if arcs[i]:
            return PA
        return mats[b]

    K.prism_layers(bm, poly, [z + z0 for z in KB_ZS], side, PA, PA)
    # the egg layer carries a yolk stripe on the cut faces
    zy0, zy1 = z0 + KB_ZS[3] + 0.0028, z0 + KB_ZS[3] + 0.0052
    paint(bm, lambda c, n, m: YO if m == EG and zy0 < c.z < zy1 and abs(n.z) < 0.5 else m)
    return bm


def fest_kulebyaka(k):
    poly = D.sector(KB_R, PI - KB_HALF, PI + KB_HALF, 10, apex=KB_APEX)
    if KB_CUT[k]:
        c, sl = KB_CUT[k]
        poly = D.clip_poly(poly, [(-1, -1), (c - sl, -1), (c + sl, 1), (-1, 1)])
    on_arc = lambda p: abs(math.hypot(p[0] - KB_APEX[0], p[1] - KB_APEX[1]) - KB_R) < 1e-5
    arcs = [on_arc(poly[i]) and on_arc(poly[(i + 1) % len(poly)]) for i in range(len(poly))]
    lay = kb_layers(poly, arcs)
    lay.normal_update()
    zt = 0.0006 + KB_ZS[-1]
    bmesh.ops.delete(lay, geom=[f for f in lay.faces if f.normal.z > 0.9 and abs(f.calc_center_median().z - zt) < 1e-5], context="FACES")
    parts = [lay]
    top = new()
    D.pillow(top, poly, 0.0006 + KB_ZS[-1], KB_TOP, M("RS_F_Pastry"), ((1, 0), (1, 0.55), (0.93, 0.88), (0.62, 1.0), (0, 1.05)), bottom=True)
    paint(top, lambda c, n, m: M("RS_F_PastryGlaze") if n.z > 0.5 else m)
    parts.append(K.drop_bottom(top, 1.0))
    dl = new()
    herb_sprig(dl, (0.040, -0.048), PI, M("RS_F_Dill"), 0.028, 0.0)
    parts.append(dl)
    return finish(parts)


def load_kulebyaka(big):
    L = 0.024 if big else 0.016
    hw = 0.0085 if big else 0.0065
    poly = [(-L / 2, -hw), (L / 2, -hw * 0.45), (L / 2, hw * 0.45), (-L / 2, hw)]
    zb = lambda x, y: D.tine_z(x) + 0.0003
    bm = new()
    zs = [0.0, 0.0018, 0.0052, 0.0090, 0.0124]
    PA, RI, SA, EG = M("RS_F_Pastry"), M("RS_F_KbRice"), M("RS_F_Salmon"), M("RS_F_EggWhite")
    K.prism_layers(bm, poly, zs, lambda i, b: PA if i == 3 else [PA, RI, SA, EG][b], PA, PA, zb)
    top = new()
    D.pillow(top, poly, zs[-1], 0.0050, M("RS_F_PastryGlaze"), ((1, 0), (1, 0.5), (0.9, 0.88), (0.6, 1.0), (0, 1.05)), zb=zb)
    merge(bm, top)
    return bm


# =========================================================================================================================
# kitchen props
# =========================================================================================================================

PAN_FLOOR = 0.005
PAN_R, PAN_H = 0.1285, 0.0455
V_PAN = K.Vessel("SM_RT_FryPan", [(0.0, 0.005), (0.095, 0.005), (0.103, 0.010), (0.118, 0.030), (0.1255, 0.0445)], 0.005, 0.1255)
PAN_HANDLE = 0.200


def fry_pan():
    bm = new()
    CA = M("RS_T_Cast")
    prof = [(0.088, 0.0), (0.0950, 0.0), (0.1000, 0.0035), (0.1170, 0.0275), (0.1262, 0.0430), (0.1285, 0.0455), (0.1262, 0.0460),
            (0.1245, 0.0450), (0.1225, 0.0435), (0.1180, 0.0315), (0.1030, 0.0105), (0.0950, 0.0050)]
    D.lathe_fan(bm, prof, [CA], 32, cap0=True, cap1=True)
    # a pouring lip opposite the handle is skipped; the handle: a tapering flat-oval bar with a hanging hole's boss
    hp = D.spline([(0.116, 0.0, 0.0315), (0.150, 0.0, 0.0330), (0.200, 0.0, 0.0370), (0.260, 0.0, 0.0420), (0.322, 0.0, 0.0460)], 3)
    h = new()
    D.sweep(h, hp, lambda s: 0.0105 - 0.0045 * s, CA, 8)
    xform(h, scale=(1, 1, 0.62))
    xform(h, loc=(0, 0, 0.0315 * (1 - 0.62) + 0.0))
    merge(bm, h)
    return bm


def pan_food():
    parts = []
    spots = [(-0.028, 0.038, 0.3), (0.034, -0.030, -0.5)]
    for i, (x, y, r) in enumerate(spots):
        b = new()
        rf = lambda th: 1.0 / math.sqrt((math.cos(th) / 0.040) ** 2 + (math.sin(th) / 0.028) ** 2) * (1 + 0.05 * math.sin(3 * th + i))
        D.dome(b, 0, 0, 0.0, rf, ((1, 0), (1, 0.45), (0.95, 0.80), (0.80, 0.96), (0.45, 1.0), (0, 1.03)), 0.0155, M("RS_F_Cutlet"), 22, bottom=True)
        paint(b, lambda c, n, m: M("RS_F_CutletDark") if n.z > 0.5 and math.sin(c.x * 350 + i) * math.sin(c.y * 320) > 0.35 else (M("RS_F_CutletDark") if n.z < 0.4 and c.z < 0.003 else m))
        xform(b, loc=(x, y, 0.0006), rz=r)
        parts.append(b)
    on = new()
    for (x, y, a) in ((0.0, 0.0, 0.3), (-0.050, -0.040, 1.2), (0.062, 0.050, 2.4), (0.020, 0.075, 0.6), (-0.072, 0.020, 2.0)):
        s = new()
        C.box(s, (0, 0, 0), (0.016, 0.0034, 0.0022), M("RS_F_Onion"))
        merge(on, xform(s, loc=(x, y, 0.0012), rz=a))
    parts.append(on)
    return finish(parts)


STOCK_R, STOCK_H = 0.150, 0.235


def stock_pot():
    bm = new()
    AL, BK = M("RS_T_Aluminium"), M("RS_T_Black")
    body = [(0.130, 0.0), (0.1500, 0.004), (0.1515, 0.014), (0.1500, STOCK_H - 0.002), (0.1530, STOCK_H), (0.1500, STOCK_H),
            (0.1440, STOCK_H - 0.002), (0.1440, 0.012), (0.1380, 0.008)]
    D.lathe_fan(bm, body, [AL], 36, cap0=True, cap1=True)
    # the lid: a flange resting on the rim, a shallow dome and a black knob
    lid = [(0.1560, STOCK_H), (0.1570, STOCK_H + 0.003), (0.1490, STOCK_H + 0.008), (0.1250, STOCK_H + 0.0165), (0.0900, STOCK_H + 0.0230),
           (0.0500, STOCK_H + 0.0262), (0.0200, STOCK_H + 0.0272), (0.0160, STOCK_H + 0.0268)]
    D.lathe_fan(bm, lid, [AL], 36, cap0=True, cap1=True)
    knob = [(0.0080, STOCK_H + 0.0268), (0.0080, STOCK_H + 0.0330), (0.0150, STOCK_H + 0.0380), (0.0170, STOCK_H + 0.0440), (0.0120, STOCK_H + 0.0490),
            (0.0040, STOCK_H + 0.0505)]
    D.lathe_fan(bm, knob, [BK], 14, cap0=True, cap1=True)
    for s in (1, -1):
        hp = D.spline([(0.0, s * 0.1490, STOCK_H - 0.020), (0.0, s * 0.1650, STOCK_H - 0.014), (0.0, s * 0.1830, STOCK_H - 0.022),
                       (0.0, s * 0.1830, STOCK_H - 0.040), (0.0, s * 0.1650, STOCK_H - 0.048), (0.0, s * 0.1490, STOCK_H - 0.042)], 3)
        D.sweep(bm, hp, 0.0065, M("RS_T_Black"), 6)
    return bm


LADLE_BOWL_R, LADLE_BOWL_H = 0.050, 0.040


def ladle():
    bm = new()
    ST_ = M("RS_T_Steel")
    outer = [(0.012, 0.0), (0.024, 0.003), (0.037, 0.011), (0.046, 0.024), (0.0500, 0.040)]
    prof = outer + [(0.0486, 0.0405), (0.0485, 0.040)] + [(r - 0.0016, z + 0.0016) for r, z in outer[::-1][1:]] + [(0.0, 0.0016)]
    prof = [(0.0120, 0.0), (0.0240, 0.0030), (0.0370, 0.0110), (0.0460, 0.0240), (0.0500, 0.0400), (0.0484, 0.0408), (0.0470, 0.0400),
            (0.0432, 0.0262), (0.0346, 0.0132), (0.0224, 0.0050)]
    D.lathe_fan(bm, prof, [ST_], 24, cap0=True, cap1=True)
    hp = D.spline([(0.044, 0.0, 0.0405), (0.100, 0.0, 0.0640), (0.180, 0.0, 0.0950), (0.255, 0.0, 0.1100), (0.300, 0.0, 0.1050), (0.318, 0.0, 0.0950)], 3)
    D.sweep(bm, hp, 0.0042, ST_, 6)
    return bm


BELL_ZREST = 0.0305
BELL_RECESS = 0.0050


def service_bell():
    bm = new()
    CR = M("RS_T_Chrome")
    prof = [(0.0405, 0.0), (0.0430, 0.0), (0.0430, 0.0040), (0.0412, 0.0065), (0.0345, 0.0075), (0.0338, 0.0125), (0.0312, 0.0195), (0.0255, 0.0250),
            (0.0175, 0.0285), (0.0125, 0.0302), (0.0115, BELL_ZREST), (0.0105, BELL_ZREST), (0.0105, BELL_ZREST - BELL_RECESS)]
    D.lathe_fan(bm, prof, [CR], 28, cap0=True, cap1=True)
    return bm


def service_bell_plunger():
    bm = new()
    CR = M("RS_T_Chrome")
    prof = [(0.0030, -0.0045), (0.0030, 0.0010), (0.0085, 0.0010), (0.0092, 0.0040), (0.0085, 0.0075), (0.0060, 0.0090), (0.0030, 0.0095)]
    D.lathe_fan(bm, prof, [CR], 16, cap0=True, cap1=True)
    return bm


def plate_dz():
    top_ = [(0.0945, 0.0145), (0.1, 0.016), (0.1105, 0.022), (0.119, 0.0236), (0.125, 0.024), (0.129, 0.024), (0.133, 0.0234)]
    bot_ = [(0.09, 0.0), (0.1254, 0.0105), (0.135, 0.0195)]
    return max(D.interp(top_, r) - D.interp(bot_, r) for r in lin(0.095, 0.133, 60)) + 0.0004


PLATE_STACK_DZ = plate_dz()


def plate_stack_mesh():
    bm = new()
    dz = PLATE_STACK_DZ
    low = PLATE_PTS[2:-1]
    for i in range(8):
        b = new()
        if i == 7:
            mats = [CH] * 12
            mats[6] = AM
            D.lathe_fan(b, PLATE_PTS, mats, 30, cap0=True, cap1=True)
        else:
            mats = [CH] * (len(low) - 1)
            mats[4] = AM
            D.lathe_fan(b, low, mats, 30, cap0=True, cap1=True)
        merge(bm, xform(b, loc=(0, 0, dz * i), rz=0.31 * i))
    return bm


RAIL_L = 0.600
RAIL_BAR_Y, RAIL_BAR_Z = 0.032, 0.050
TICKETS = [(-0.205, 0.150, 0.025), (-0.070, 0.175, -0.020), (0.075, 0.125, 0.030), (0.200, 0.160, -0.015)]


def ticket_rail():
    bm = new()
    ST_ = M("RS_T_Steel")
    # the back plate and the lower ledge: one L-shaped profile (y, z) run along x
    prof = [(0.0, 0.0), (0.018, 0.0), (0.018, 0.003), (0.003, 0.003), (0.003, 0.058), (0.0, 0.058)]
    lo = [bm.verts.new((-RAIL_L / 2, y, z)) for y, z in prof]
    hi = [bm.verts.new((RAIL_L / 2, y, z)) for y, z in prof]
    for i in range(len(prof)):
        j = (i + 1) % len(prof)
        bm.faces.new((lo[i], hi[i], hi[j], lo[j])).material_index = ST_
    cap0 = bm.faces.new(lo[::-1])
    cap1 = bm.faces.new(hi)
    bmesh.ops.triangulate(bm, faces=[cap0, cap1], quad_method="BEAUTY", ngon_method="BEAUTY")
    K.fix_normals(bm)
    C.tube(bm, (-RAIL_L / 2 + 0.004, RAIL_BAR_Y, RAIL_BAR_Z), (RAIL_L / 2 - 0.004, RAIL_BAR_Y, RAIL_BAR_Z), 0.0050, ST_, 10)
    for x in (-0.275, 0.0, 0.275):
        C.tube(bm, (x, 0.003, RAIL_BAR_Z), (x, RAIL_BAR_Y - 0.0048, RAIL_BAR_Z), 0.0042, ST_, 8)
    PA, TR, TI = M("RS_T_Paper"), M("RS_T_TicketRed"), M("RS_T_TicketInk")
    for i, (x, hgt, tilt) in enumerate(TICKETS):
        t = new()
        W, ny = 0.072, 16

        def mf(ii, jj, top, ny=ny):
            row = ny - 1 - jj                      # 0 = the top row
            if row < 2:
                return TR
            return TI if (row % 2 == 0 and row > 3 and (not top) and ii < 3 - (row % 3)) else PA

        K.slab_grid(t, 4, ny, lambda u, v, W=W, hgt=hgt: ((u - 0.5) * W, v * hgt), lambda x, y: 0.0007, lambda x, y: 0.0, mf)
        # (a grid over x and the height: the sheet's plane is x-z; stand it up)
        t2 = new()
        for f in t.faces:
            pass
        xform(t, mat=Matrix(((1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1))))
        xform(t, loc=(x, RAIL_BAR_Y + 0.0050 + 0.0006 + 0.0012 * i, RAIL_BAR_Z + 0.008 - hgt), rz=tilt * 0.3)
        merge(bm, t)
    return bm


CLOCHE_R, CLOCHE_H = 0.125, 0.1235


def cloche():
    bm = new()
    ST_ = M("RS_T_Steel")
    outer = [(0.1250, 0.0), (0.1256, 0.0035), (0.1210, 0.0300), (0.1060, 0.0610), (0.0760, 0.0840), (0.0400, 0.0940), (0.0160, 0.0975), (0.0040, 0.0985)]
    inner = [(0.0040, 0.0965), (0.0150, 0.0955), (0.0385, 0.0920), (0.0745, 0.0820), (0.1040, 0.0590), (0.1190, 0.0290), (0.1233, 0.0030), (0.1230, 0.0)]
    prof = outer + inner + [outer[0]]
    C.lathe(bm, prof, [ST_], 32, cap0=False, cap1=False)
    # (the lathe's ring at r = 0.004 on top is closed by the knob's foot)
    knob = [(0.0070, 0.0970), (0.0070, 0.1030), (0.0130, 0.1070), (0.0150, 0.1130), (0.0110, 0.1190), (0.0040, 0.1235)]
    D.lathe_fan(bm, knob, [M("RS_T_Black")], 14, cap0=True, cap1=True)
    return bm


# =========================================================================================================================
# registry
# =========================================================================================================================

X_TABLEWARE = {
    "SM_RT_Geta": geta, "SM_RT_ClayBowl": clay_bowl, "SM_RT_GlassMug": glass_mug, "SM_RT_TeaHolder": tea_holder,
    "SM_RT_FryPan": fry_pan, "SM_RT_PanFood": pan_food, "SM_RT_StockPot": stock_pot, "SM_RT_Ladle": ladle,
    "SM_RT_ServiceBell": service_bell, "SM_RT_ServiceBell_Plunger": service_bell_plunger, "SM_RT_PlateStack": plate_stack_mesh,
    "SM_RT_TicketRail": ticket_rail, "SM_RT_Cloche": cloche,
}
X_FILLS = {
    "SM_RF_solyanka_Fill": solyanka_fill, "SM_RF_fest_ukha_Fill": ukha_fill, "SM_RF_fest_sbiten_Fill": mug_fill,
    "SM_RF_tea_Fill": tea_fill,
}
X_DISHES = {
    "solyanka": (V_SOUP, solyanka, load_solyanka, "spoon", "SM_RF_solyanka_Fill", BORSCHT_FILL, BORSCHT_H, SOLY_T),
    "golubtsy": (V_DINNER, golubtsy, load_golubtsy, "fork", None, None, None, None),
    "draniki": (V_DINNER, draniki, load_draniki, "fork", None, None, None, None),
    "fried_flounder": (V_DINNER, fried_flounder, load_fried_flounder, "fork", None, None, None, None),
    "shashlik": (V_BOARD, shashlik, load_shashlik, "fork", None, None, None, None),
    "sushi_set": (V_GETA, sushi_set, load_sushi, "chopsticks", None, None, None, None),
    "blini_caviar": (V_DINNER, blini_caviar, load_blini, "fork", None, None, None, None),
    "fest_ukha": (V_CLAY, fest_ukha, load_ukha, "spoon", "SM_RF_fest_ukha_Fill", UKHA_FILL, UKHA_H, UKHA_T),
    "fest_kulebyaka": (V_DESSERT, fest_kulebyaka, load_kulebyaka, "fork", None, None, None, None),
    "fest_sbiten": (V_MUG, None, None, "cup", "SM_RF_fest_sbiten_Fill", MUG_FILL, MUG_FILL_H, None),
    "tea": (V_TEA, None, None, "cup", "SM_RF_tea_Fill", TEA_FILL, TG_FILL_H, None),
}
X_RADIUS = {"SM_RT_Geta": GETA_HX, "SM_RT_ClayBowl": CL_R, "SM_RT_GlassMug": mug_ro(MUG_H), "SM_RT_TeaHolder": 0.0346}
X_HALF = {"SM_RT_GlassMug": MUG_H / 2, "SM_RT_TeaHolder": TH_H / 2}
K.SMOOTH |= {"RS_T_Nickel", "RS_T_Clay", "RS_T_ClayGlaze", "RS_T_Cast", "RS_T_Aluminium", "RS_T_Chrome", "RS_T_Black"}
K.FLAT |= {"SM_RT_TeaHolder", "SM_RT_TeaHolder_Glass", "SM_RF_tea_Fill"}
X_NEW = set(X_TABLEWARE) | set(X_FILLS) | set(X_DISHES)


def x_consts():
    d = {
        "GETA_LX": 2 * GETA_HX, "GETA_LY": 2 * GETA_HY, "GETA_H": GETA_TOP, "GETA_TOP": GETA_TOP,
        "CLAY_BOWL_R": CL_R, "CLAY_BOWL_H": CL_H, "CLAY_BOWL_IN_Z": CL_IN_Z, "CLAY_BOWL_IN_R": 0.0735,
        "GLASS_MUG_R": mug_ro(MUG_H), "GLASS_MUG_H": MUG_H, "GLASS_MUG_HALF_H": MUG_H / 2, "GLASS_MUG_IN_Z": MUG_IN_Z,
        "GLASS_MUG_IN_R": mug_ri(MUG_H), "GLASS_MUG_FILL_H": MUG_FILL_H, "GLASS_MUG_HANDLE_Y": MUG_HANDLE_Y,
        "TEA_HOLDER_R": 0.0346, "TEA_HOLDER_H": TH_H, "TEA_HOLDER_GLASS_H": TG_TOP, "TEA_HOLDER_IN_Z": TG_IN_Z,
        "TEA_HOLDER_FILL_H": TG_FILL_H, "TEA_HOLDER_HANDLE_Y": 0.0545,
        "PAN_FOOD_Z": PAN_FLOOR, "FRY_PAN_R": PAN_R, "FRY_PAN_H": PAN_H, "FRY_PAN_HANDLE_LEN": PAN_HANDLE + 0.122,
        "FRY_PAN_IN_R": 0.1255,
        "STOCK_POT_R": 0.153, "STOCK_POT_H": STOCK_H, "STOCK_POT_LID_TOP": STOCK_H + 0.0505,
        "LADLE_LEN": 0.32, "LADLE_BOWL_R": LADLE_BOWL_R, "LADLE_ORIGIN": "bowl's bottom centre, handle rising along +x",
        "SERVICE_BELL_R": 0.043, "SERVICE_BELL_H": BELL_ZREST, "SERVICE_BELL_PLUNGER_Z": BELL_ZREST,
        "SERVICE_BELL_PLUNGER_TRAVEL": BELL_RECESS - 0.001,
        "PLATE_STACK_N": 8, "PLATE_STACK_PITCH": PLATE_STACK_DZ, "PLATE_STACK_R": 0.135,
        "PLATE_STACK_H": PLATE_STACK_DZ * 7 + 0.024,
        "TICKET_RAIL_LEN": RAIL_L, "TICKET_RAIL_BAR_Y": RAIL_BAR_Y, "TICKET_RAIL_BAR_Z": RAIL_BAR_Z, "TICKET_RAIL_DEPTH": 0.052,
        "TICKET_RAIL_ORIGIN": "centre of the back plate's back face (y = 0), z = 0 at its lower edge; tickets hang below",
        "CLOCHE_R": CLOCHE_R, "CLOCHE_H": CLOCHE_H, "CLOCHE_ORIGIN": "rim centre (z = 0 at the rim's lower edge)",
    }
    return d
