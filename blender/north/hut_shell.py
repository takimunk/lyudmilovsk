"""The ranger's hut, shell (hut.glb): log walls seen from both sides, floor, ceiling, roof, porch, steps, footings, windows,
shutters and the fixed outside details. Built in three bmesh parts (main, ceiling, roof) so a preview can hide the roof and the
ceiling; export joins them into one mesh.

Plan (hut-local metres, -Y = front): the cabin is x -2.1..2.1, y -1.9..3.5 (4.2 x 5.4 m, 22 cm round logs, walls 0.22 thick);
the porch is y -3.5..-1.9; the floor top is F = 0.40 over the ground; the roof ridge runs along Y.
"""
import math

from mathutils import Vector

from hut_common import (F, Mesh, bx, box, cyl, lathe, prism, quad, tube_path, rot_x, rot_y, rot_z, about, softbox)

R = 0.11                      # log radius (22 cm logs)
PITCH = 0.19                  # log course pitch: logs overlap by 3 cm, so no daylight between courses
XO, YF, YB = 2.10, -1.90, 3.50
XI, YFI, YBI = XO - 2 * R, YF + 2 * R, YB - 2 * R          # inner faces: 1.88, -1.68, 3.28
YP = -3.50                    # porch front edge
CEIL = 2.70                   # ceiling underside
NK = 12                       # courses
PITCH_DEG = 30.0
T30 = math.tan(math.radians(PITCH_DEG))
C30 = math.cos(math.radians(PITCH_DEG))
S30 = math.sin(math.radians(PITCH_DEG))
EAVE_X = 2.5
ZU_EAVE = 2.67
ZR = ZU_EAVE + EAVE_X * T30           # underside of the ridge (4.113)
ROOF_Y = 3.8                          # the roof runs y -3.8..3.8


def zu(x):
    """Underside of the roof over x."""
    return ZR - abs(x) * T30


def zf(k):
    return F + 0.10 + PITCH * k       # front/back wall log centres


def zs(k):
    return F + 0.195 + PITCH * k      # side wall log centres (half a course higher: the corners interlock)


# the door, in the front wall: clear x 0..1, from the floor to the header
DOOR = dict(a0=0.0, a1=1.0, z0=F, z1=2.45)
# windows: front (x -1.4..-0.6), left side (y 0.15..0.95); sill and header follow the log courses
WIN_F = dict(a0=-1.4, a1=-0.6, z0=1.18, z1=2.11)
WIN_S = dict(a0=0.15, a1=0.95, z0=1.275, z1=2.195)
DOOR_CUT = (-0.03, 1.03, 0, 10)          # wall a-range cut out, courses k0..k1 removed (front wall)
WINF_CUT = (-1.43, -0.57, 4, 8)
WINS_CUT = (0.12, 0.98, 4, 8)


def wall_logs(m, wall, cuts):
    """All courses of one wall, cut where the openings are. wall: 'front' | 'back' | 'left' | 'right'."""
    ALONG = {"front": (0, YF + R), "back": (0, YB - R), "left": (1, -XO + R), "right": (1, XO - R)}
    axis, pos = ALONG[wall]
    out = {"front": Vector((0, -1, 0)), "back": Vector((0, 1, 0)), "left": Vector((-1, 0, 0)), "right": Vector((1, 0, 0))}[wall]
    lo, hi = (-XO - 0.2, XO + 0.2) if axis == 0 else (YF - 0.2, YB + 0.2)
    for k in range(NK):
        z = zf(k) if axis == 0 else zs(k)
        # the interval pieces of this course
        pieces = [(lo, hi)]
        for a0, a1, k0, k1 in cuts:
            if k0 <= k <= k1:
                nxt = []
                for p0, p1 in pieces:
                    if a1 <= p0 or a0 >= p1:
                        nxt.append((p0, p1))
                        continue
                    if a0 > p0:
                        nxt.append((p0, a0))
                    if a1 < p1:
                        nxt.append((a1, p1))
                pieces = nxt
        rr = R * (1.0 + 0.04 * (m.rng.random() - 0.5))
        for p0, p1 in pieces:
            if axis == 0:
                a, b = (p0, pos, z), (p1, pos, z)
            else:
                a, b = (pos, p0, z), (pos, p1, z)
            cyl(m, a, b, rr, rr * (1 + 0.03 * (m.rng.random() - 0.5)), "HUT_LogOut", seg=12, endmat="HUT_LogEnd", wob=0.04,
                tile=1.0, split=(out, "HUT_LogOut", "HUT_LogIn"), rot0=m.rng.random())


def wbox(m, wall_axis, mat, a0, a1, t0, t1, z0, z1, **kw):
    """A box by wall coordinates: a along the wall, t through it."""
    if wall_axis == 0:
        bx(m, mat, a0, a1, t0, t1, z0, z1, **kw)
    else:
        bx(m, mat, t0, t1, a0, a1, z0, z1, **kw)


def opening(m, wall_axis, o, t_out, t_in, window, shutters=True, side=1):
    """Lining, casings (both faces), sill, and for a window the four-pane sash and the plank shutters.
    t_out/t_in: the wall's outer/inner face coordinates; side: +1 when outside is toward +t, -1 toward -t."""
    a0, a1, z0, z1 = o["a0"], o["a1"], o["z0"], o["z1"]
    tl, th = min(t_out, t_in), max(t_out, t_in)
    g = 0 if wall_axis == 0 else 1       # the along-wall axis index, for the grain
    P = "HUT_PlankOut"
    L = 0.03
    # lining across the whole wall thickness: jambs, header, sill
    wbox(m, wall_axis, P, a0 - L, a0, tl, th, z0, z1 + L, grain=2)
    wbox(m, wall_axis, P, a1, a1 + L, tl, th, z0, z1 + L, grain=2)
    wbox(m, wall_axis, P, a0, a1, tl, th, z1, z1 + L, grain=g)
    if window:
        wbox(m, wall_axis, P, a0, a1, tl, th, z0 - L, z0, grain=g)
    # casings: flat boards 8 cm wide on both faces, over the ends of the logs
    for face_t, out in ((t_out, True), (t_in, False)):
        d = side if out else -side
        c0 = face_t + d * 0.0 - 0.0
        tt0, tt1 = (c0 + d * 0.03, c0 - d * 0.02)
        tt0, tt1 = min(tt0, tt1), max(tt0, tt1)
        lo = z0 - (0.0 if not window else 0.02)
        wbox(m, wall_axis, P, a0 - 0.11, a0 - L, tt0, tt1, lo, z1 + 0.11, grain=2)
        wbox(m, wall_axis, P, a1 + L, a1 + 0.11, tt0, tt1, lo, z1 + 0.11, grain=2)
        wbox(m, wall_axis, P, a0 - 0.11, a1 + 0.11, tt0, tt1, z1 + L, z1 + 0.11, grain=g)
        if window:
            # the sill board (outside: a drip shelf 10 cm out; inside: the window board 6 cm in)
            proj = 0.10 if out else 0.06
            s0, s1 = face_t, face_t + d * (0.03 + proj)
            s0, s1 = min(s0, s1), max(s0, s1)
            wbox(m, wall_axis, P, a0 - 0.12, a1 + 0.12, s0 - (0.0 if d > 0 else 0.0), s1, z0 - 0.055, z0 - 0.02, grain=g)
    if window:
        tm = (t_out + t_in) / 2
        S = "HUT_Linen"
        w, h = a1 - a0, z1 - z0
        ts = (tm - 0.02, tm + 0.02)
        fw = 0.045
        wbox(m, wall_axis, S, a0, a0 + fw, *ts, z0, z1, grain=2)
        wbox(m, wall_axis, S, a1 - fw, a1, *ts, z0, z1, grain=2)
        wbox(m, wall_axis, S, a0 + fw, a1 - fw, *ts, z0, z0 + fw, grain=g)
        wbox(m, wall_axis, S, a0 + fw, a1 - fw, *ts, z1 - fw, z1, grain=g)
        am, zm = (a0 + a1) / 2, (z0 + z1) / 2
        wbox(m, wall_axis, S, am - 0.015, am + 0.015, *ts, z0 + fw, z1 - fw, grain=2)
        wbox(m, wall_axis, S, a0 + fw, a1 - fw, *ts, zm - 0.015, zm + 0.015, grain=g)
        for (pa0, pa1) in ((a0 + fw, am - 0.015), (am + 0.015, a1 - fw)):
            for (pz0, pz1) in ((z0 + fw, zm - 0.015), (zm + 0.015, z1 - fw)):
                wbox(m, wall_axis, "HUT_Glass", pa0 - 0.004, pa1 + 0.004, tm - 0.003, tm + 0.003, pz0 - 0.004, pz1 + 0.004)
        if shutters:
            for sgn in (-1, 1):
                if sgn < 0:
                    sa0, sa1 = a0 - 0.11 - 0.05 - 0.42, a0 - 0.11 - 0.05
                else:
                    sa0, sa1 = a1 + 0.11 + 0.05, a1 + 0.11 + 0.05 + 0.42
                tb0 = t_out + side * 0.035
                tb1 = t_out + side * 0.07
                tb0, tb1 = min(tb0, tb1), max(tb0, tb1)
                zlo, zhi = z0 - 0.02, z1 + 0.06
                n = 3
                bw = (sa1 - sa0 - 0.02 * (n - 1)) / n
                for i in range(n):
                    wbox(m, wall_axis, P, sa0 + i * (bw + 0.02), sa0 + i * (bw + 0.02) + bw, tb0, tb1, zlo, zhi, grain=2)
                bt0, bt1 = (tb1, tb1 + side * 0.022) if side > 0 else (tb0 - 0.022, tb0)
                for zc in (zlo + 0.10, zhi - 0.10):
                    wbox(m, wall_axis, P, sa0, sa1, bt0, bt1, zc - 0.04, zc + 0.04, grain=g)
                # two black hinge straps on the batten at the casing side
                hx0, hx1 = (sa1 - 0.30, sa1) if sgn < 0 else (sa0, sa0 + 0.30)
                for zc in (zlo + 0.10, zhi - 0.10):
                    ht0, ht1 = (bt1, bt1 + side * 0.006) if side > 0 else (bt0 - 0.006, bt0)
                    wbox(m, wall_axis, "HUT_Black", hx0, hx1, ht0, ht1, zc - 0.015, zc + 0.015, grain=g)


def build_main(m):
    """Walls, floor, porch, steps, footings, windows, the door's lining, exterior details."""
    # ---- footings (field stones) and the sill frame
    for (x, y, top) in [(-1.99, -1.79, 0.18), (1.99, -1.79, 0.18), (-1.99, 0.2, 0.18), (1.99, 0.2, 0.18), (-1.99, 1.8, 0.18),
                        (1.99, 1.8, 0.18), (-1.99, 3.39, 0.18), (1.99, 3.39, 0.18), (0.0, -1.79, 0.18), (0.0, 3.39, 0.18),
                        (-1.95, -3.35, 0.28), (1.95, -3.35, 0.28)]:
        s = 0.44 + 0.06 * m.rng.random()
        box(m, (x, y, (top - 0.10) / 2), (s, s * (0.9 + 0.2 * m.rng.random()), top + 0.10), "HUT_Stone",
            xf=about(rot_z(m.rng.uniform(-12, 12)), (x, y, 0)))
    P, PI = "HUT_PlankOut", "HUT_PlankIn"
    bx(m, P, -XO, XO, YF, YFI, 0.18, F, grain=0, endmat="HUT_LogEnd")           # sill timbers 22 x 22
    bx(m, P, -XO, XO, YBI, YB, 0.18, F, grain=0, endmat="HUT_LogEnd")
    bx(m, P, -XO, -XI, YFI, YBI, 0.18, F, grain=1, faces="xXzZ")
    bx(m, P, XI, XO, YFI, YBI, 0.18, F, grain=1, faces="xXzZ")
    # ---- the floor deck inside (painted boards) and the porch deck (weathered boards)
    bx(m, "HUT_FloorIn", -XI, XI, YFI, YBI, 0.28, F, grain=1, faces="zZ")
    bx(m, "HUT_PlankOut", -XI, XI, YFI, YBI, 0.28, F, grain=1, faces="xXyY")
    bx(m, P, -XO, XO, YP, YF, 0.28, F, grain=1, faces="xXyzZ")
    # steps: two risers of 13.3 cm to the deck
    bx(m, P, -0.25, 1.25, -4.10, -3.80, -0.02, 0.133, grain=0, faces="xXyZ")
    bx(m, P, -0.25, 1.25, -3.80, -3.50, -0.02, 0.267, grain=0, faces="xXyZ")
    # ---- log walls
    wall_logs(m, "front", [DOOR_CUT, WINF_CUT])
    wall_logs(m, "back", [])
    wall_logs(m, "left", [WINS_CUT])
    wall_logs(m, "right", [])
    # ---- the door's opening: lintel timber above, jamb linings, casings, a board over the leaf
    ytm = (YF + YFI) / 2
    d = DOOR
    bx(m, "HUT_PlankOut", d["a0"] - 0.14, d["a1"] + 0.14, YF, YFI, d["z1"], d["z1"] + 0.10, grain=0, endmat="HUT_LogEnd")
    opening(m, 0, dict(a0=d["a0"], a1=d["a1"], z0=d["z0"], z1=d["z1"] - 0.03), YF, YFI, window=False)
    bx(m, P, d["a0"], d["a1"], ytm - 0.015, ytm + 0.015, 2.365, d["z1"], grain=0)        # board above the leaf
    # ---- windows
    opening(m, 0, WIN_F, YF, YFI, window=True, side=-1)
    opening(m, 1, WIN_S, -XO, -XI, window=True, side=-1)
    # ---- porch: posts, front beam, rails, bench
    for sx in (-1, 1):
        bx(m, P, sx * 2.0 - 0.07, sx * 2.0 + 0.07, -3.42, -3.28, F, 2.62, grain=2)
    bx(m, P, -XO, XO, -3.50, -3.28, 2.62, 2.90, grain=0, endmat="HUT_LogEnd")
    bx(m, P, -XO, XO, -2.75, -2.60, 2.62, 2.78, grain=0, endmat="HUT_LogEnd")             # tie beam
    # railings: sides and front on both sides of the steps
    def rail(a, b, fixed, axis):
        # a, b: along-coordinates; fixed: the other; axis 0 = along x
        for zc, hh in ((F + 0.88, 0.07), (F + 0.30, 0.06)):
            if axis == 0:
                bx(m, P, a, b, fixed - 0.03, fixed + 0.03, zc, zc + hh, grain=0)
            else:
                bx(m, P, fixed - 0.03, fixed + 0.03, a, b, zc, zc + hh, grain=1)
        n = max(2, int(abs(b - a) / 0.22))
        for i in range(n + 1):
            t = a + (b - a) * i / n
            if i in (0, n):
                continue
            if axis == 0:
                bx(m, P, t - 0.018, t + 0.018, fixed - 0.018, fixed + 0.018, F + 0.37, F + 0.88, grain=2)
            else:
                bx(m, P, fixed - 0.018, fixed + 0.018, t - 0.018, t + 0.018, F + 0.37, F + 0.88, grain=2)
    rail(-3.28, -2.1, -2.03, 1)
    rail(-3.28, -2.1, 2.03, 1)
    rail(-2.0, -0.28, -3.35, 0)
    rail(1.28, 2.0, -3.35, 0)
    # bench against the front wall, right of the door
    bx(m, P, 1.18, 2.02, -2.28, -1.96, F + 0.44, F + 0.48, grain=0)
    for x in (1.26, 1.94):
        bx(m, P, x - 0.025, x + 0.025, -2.26, -1.98, F, F + 0.44, grain=2)
    # ---- rain barrel under the right eave
    lathe(m, [(0.25, 0.0), (0.285, 0.1), (0.31, 0.42), (0.285, 0.74), (0.25, 0.84)], "HUT_PlankOut", seg=16, c=(2.45, -0.6, 0.0), tile=1.2)
    for z in (0.12, 0.30, 0.56, 0.72):
        # hoop radius follows the staves' profile
        prof = [(0.25, 0.0), (0.285, 0.1), (0.31, 0.42), (0.285, 0.74), (0.25, 0.84)]
        rr = 0.25
        for (r0, z0), (r1, z1_) in zip(prof[:-1], prof[1:]):
            if z0 <= z <= z1_:
                rr = r0 + (r1 - r0) * (z - z0) / (z1_ - z0)
        lathe(m, [(rr + 0.004, z - 0.02), (rr + 0.012, z - 0.02), (rr + 0.012, z + 0.02), (rr + 0.004, z + 0.02)], "HUT_Iron", seg=16,
              c=(2.45, -0.6, 0.0), caps=(False, False), tile=0.5)
    lathe(m, [(0.0, 0.80), (0.235, 0.80)], "HUT_Black", seg=16, c=(2.45, -0.6, 0.0), caps=(False, False))   # the water
    # ---- ski rack on the right wall: bar on two brackets, two skis, two poles, a shovel
    bx(m, P, 2.08, 2.22, 0.98, 2.12, 1.52, 1.60, grain=1)
    for y in (1.05, 2.05):
        bx(m, P, 2.08, 2.24, y - 0.02, y + 0.02, 1.38, 1.60, grain=2)
    ang = -7.4
    for y in (1.28, 1.47):
        c = (2.30, y, 0.95)
        box(m, c, (0.014, 0.085, 1.9), P, grain=2, xf=about(rot_y(ang), c))
        box(m, (2.30, y, 0.95 + 0.0), (0.03, 0.05, 0.14), "HUT_Leather", xf=about(rot_y(ang), c))       # binding
        tip = (2.17, y, 1.88)
        box(m, tip, (0.014, 0.085, 0.12), P, grain=2, xf=about(rot_y(ang - 28), tip))                    # the upturned tip
    for y in (1.72, 1.82):
        cyl(m, (2.40, y, 0.02), (2.20, y, 1.55), 0.011, 0.011, "HUT_Zinc", seg=6, caps=(True, True), tile=0.5)
        cyl(m, (2.40, y, 0.06), (2.40, y, 0.065), 0.045, 0.045, "HUT_Leather", seg=8, caps=(True, True), tile=0.2)
    cyl(m, (2.24, 2.0, 1.45), (2.40, 2.0, 0.34), 0.016, 0.016, P, seg=6, tile=0.5)                       # shovel handle
    box(m, (2.405, 2.0, 0.20), (0.012, 0.26, 0.30), "HUT_Zinc", xf=about(rot_y(-7), (2.4, 2.0, 0.2)))   # blade
    # ---- the firewood lean-to against the back wall
    for x in (-0.35, 1.82):
        bx(m, P, x - 0.06, x + 0.06, 4.30, 4.42, 0.0, 1.98, grain=2)
    bx(m, P, -0.41, 1.88, YB, YB + 0.10, 2.20, 2.32, grain=0)
    sl = (-0.41, 1.88)
    sheet_c = ((sl[0] + sl[1]) / 2, YB + 0.55, 2.16)
    box(m, sheet_c, (sl[1] - sl[0] + 0.1, 1.15, 0.03), "HUT_Metal", xf=about(rot_x(-14), sheet_c))
    for i in range(8):
        for j in range(5):
            x = -0.20 + 0.16 * i + (0.02 if j % 2 else 0)
            z = 0.09 + 0.165 * j
            cyl(m, (x, YB + 0.06, z), (x, YB + 0.55, z), 0.07, 0.07 + 0.005 * m.rng.random(), "HUT_LogOut", seg=7, endmat="HUT_LogEnd",
                wob=0.1, tile=1.0, rot0=m.rng.random())
    cyl(m, (1.45, 4.05, 0.0), (1.45, 4.05, 0.42), 0.19, 0.19, "HUT_LogOut", seg=12, endmat="HUT_LogEnd", tile=1.0)   # chopping block
    cyl(m, (1.30, 3.95, 0.43), (1.25, 4.10, 0.55), 0.014, 0.014, P, seg=6, tile=0.5)                       # axe handle
    box(m, (1.28, 3.99, 0.50), (0.05, 0.14, 0.10), "HUT_Iron")
    # ---- weather post with a thermometer box (inside the pad), and the antenna's anchor pole
    px, py = 3.4, -2.4
    bx(m, P, px - 0.05, px + 0.05, py - 0.05, py + 0.05, 0.0, 2.0, grain=2)
    bx(m, "HUT_Linen", px - 0.17, px + 0.17, py - 0.30, py - 0.05, 1.50, 1.92, grain=2)
    for i in range(6):
        bx(m, "HUT_Linen", px - 0.19, px + 0.19, py - 0.325, py - 0.30, 1.52 + i * 0.065, 1.52 + i * 0.065 + 0.03, grain=0)
    box(m, (px, py - 0.18, 1.96), (0.40, 0.34, 0.03), "HUT_PlankOut", xf=about(rot_x(8), (px, py - 0.18, 1.96)))
    cyl(m, (-3.6, 2.2, 0.0), (-3.6, 2.2, 3.4), 0.04, 0.035, P, seg=8, endmat="HUT_LogEnd", tile=1.0)
    return dict(post=(px, py), anchor=(-3.6, 2.2, 3.4))


def build_ceiling(m):
    bx(m, "HUT_PlankIn", -XI - 0.06, XI + 0.06, YFI - 0.06, YBI + 0.06, CEIL, CEIL + 0.04, grain=0)
    for y in (-0.9, 0.7, 2.3):
        bx(m, "HUT_PlankIn", -XI, XI, y - 0.07, y + 0.07, CEIL - 0.14, CEIL, grain=0, endmat="HUT_LogEnd")
    # tin plate round the flue where it passes the ceiling
    bx(m, "HUT_Zinc", -1.43, -1.07, 2.74, 3.10, CEIL - 0.012, CEIL, faces="zZ")


def build_roof(m):
    """Roof sheets (corrugated, one solid per slope), ridge cap, gable boards, wall plates, flue, mast, wire."""
    ys0, ys1 = -ROOF_Y, ROOF_Y
    pitch = 0.1
    N = int(round((ys1 - ys0) / pitch)) * 4
    L = EAVE_X / C30
    for side in (-1, 1):
        d = Vector((side * C30, 0, -S30))
        nrm = Vector((side * S30, 0, C30))
        org = Vector((0, 0, ZR))

        def P_(y, s, w):
            return org + Vector((0, y, 0)) + d * s + nrm * w

        ys = [ys0 + (ys1 - ys0) * i / N for i in range(N + 1)]
        ws = [0.04 + 0.016 * math.cos(2 * math.pi * y / pitch) for y in ys]
        top0 = [m.vert(P_(y, 0, w)) for y, w in zip(ys, ws)]
        topL = [m.vert(P_(y, L, w)) for y, w in zip(ys, ws)]
        refp = org + d * (L / 2) - nrm * 2.0
        for i in range(N):
            u0, u1 = ys[i] / 2.0, ys[i + 1] / 2.0
            m.face([top0[i], top0[i + 1], topL[i + 1], topL[i]], "HUT_Metal", uvs=[(u0, 0), (u1, 0), (u1, L / 2.0), (u0, L / 2.0)], ref=refp)
        # underside (boards, seen from the porch)
        b00, b01, bL1, bL0 = m.vert(P_(ys0, 0, 0)), m.vert(P_(ys1, 0, 0)), m.vert(P_(ys1, L, 0)), m.vert(P_(ys0, L, 0))
        m.face([b00, b01, bL1, bL0], "HUT_PlankOut", ref=org + d * (L / 2) + nrm * 2.0, grain=Vector((0, 1, 0)))
        # ridge end and eave end: the corrugated profile polygon
        for s, ring in ((0, top0), (L, topL)):
            c1 = m.vert(P_(ys1, s, 0))
            c0 = m.vert(P_(ys0, s, 0))
            cen = org + d * s
            refc = cen - d * 2.0 if s > 0 else cen + d * 2.0
            m.face(list(ring) + [c1, c0], "HUT_Metal", ref=refc, tile=2.0)
        # the two side edges (y ends)
        for y, ring_i in ((ys0, 0), (ys1, N)):
            a = m.vert(P_(y, 0, 0))
            b = m.vert(P_(y, L, 0))
            m.face([a, top0[ring_i], topL[ring_i], b], "HUT_Metal", ref=P_(y + (1.0 if y == ys0 else -1.0), L / 2, 0.02), tile=2.0)
    # ridge cap
    def ztop(x):
        return ZR - abs(x) * T30 + 0.055
    prism(m, [(-0.26, ztop(0.26) + 0.004), (0.26, ztop(0.26) + 0.004), (0.0, ztop(0.0) + 0.075)], 1, -ROOF_Y - 0.04, ROOF_Y + 0.04,
          "HUT_Metal", tile=2.0)
    # gable boards (vertical planks) at the back wall, the front wall and the porch front
    for (y0, y1) in ((YB - 0.16, YB - 0.06), (YF + 0.06, YF + 0.16), (YP - 0.02, YP + 0.04)):
        base = 2.60 if y0 > YF else 2.78
        poly = [(-XO, base), (XO, base), (XO, zu(XO) + 0.012), (0.0, ZR + 0.012), (-XO, zu(XO) + 0.012)]
        prism(m, poly, 1, y0, y1, "HUT_PlankOut", grain=2)
    # wall plates carry the rafters over the side walls and the porch
    for sx in (-1, 1):
        for (y0, y1, zb) in ((YP + 0.2, YF, 2.62), (YF, YB, 2.74)):       # over the porch lower, over the room above the ceiling
            if sx > 0:
                poly = [(XI, zb), (XO, zb), (XO, zu(XO) + 0.012), (XI, zu(XI) + 0.012)]
            else:
                poly = [(-XO, zb), (-XI, zb), (-XI, zu(XI) + 0.012), (-XO, zu(XO) + 0.012)]
            prism(m, poly, 1, y0, y1, "HUT_PlankOut", grain=1)
    # stove pipe from the ceiling through the roof, with a cap
    fx, fy = -1.25, 2.92
    cyl(m, (fx, fy, CEIL), (fx, fy, 4.88), 0.065, 0.065, "HUT_Iron", seg=12, caps=(False, True), tile=0.5)
    for z in (3.0, 4.3):
        cyl(m, (fx, fy, z), (fx, fy, z + 0.05), 0.08, 0.08, "HUT_Iron", seg=12, tile=0.5)
    zr = zu(fx) + 0.05
    lathe(m, [(0.17, zr - 0.02), (0.075, zr + 0.11)], "HUT_Zinc", seg=12, c=(fx, fy, 0), caps=(False, False))       # roof flashing
    for i in range(3):
        a = 2 * math.pi * i / 3
        cyl(m, (fx + 0.06 * math.cos(a), fy + 0.06 * math.sin(a), 4.88), (fx + 0.06 * math.cos(a), fy + 0.06 * math.sin(a), 4.98), 0.007, 0.007,
            "HUT_Iron", seg=5, tile=0.3)
    lathe(m, [(0.0, 5.06), (0.16, 4.98)], "HUT_Iron", seg=12, c=(fx, fy, 0), caps=(False, False))
    # antenna mast on the ridge, a wire to the anchor pole
    mx, my = 0.0, 2.2
    zt = ZR + 0.12
    box(m, (mx, my, zt + 0.005), (0.22, 0.22, 0.03), "HUT_Zinc")
    cyl(m, (mx, my, zt), (mx, my, zt + 3.3), 0.025, 0.018, "HUT_Zinc", seg=8, tile=0.5)
    ax, ay, az = -3.6, 2.2, 3.35
    top = Vector((mx, my, zt + 3.25))
    end = Vector((ax, ay, az))
    pts = []
    n = 10
    for i in range(n + 1):
        t = i / n
        p = top.lerp(end, t)
        p.z -= 0.35 * 4 * t * (1 - t)
        pts.append(p)
    tube_path(m, pts, 0.004, "HUT_Wire", seg=5)
    box(m, (ax, ay, az + 0.02), (0.08, 0.08, 0.05), "HUT_Enamel")                # insulator
    cyl(m, (mx, my, zt + 2.4), (mx + 0.35, my, zt + 2.4), 0.009, 0.009, "HUT_Zinc", seg=5, tile=0.5)     # a crossbar of the dipole
    return dict(flue_top=(fx, fy, 4.88), mast_top=(mx, my, zt + 3.3))


def build():
    main, ceil, roof = Mesh("hut_main"), Mesh("hut_ceiling"), Mesh("hut_roof")
    info = build_main(main)
    build_ceiling(ceil)
    info.update(build_roof(roof))
    return main.finish(), ceil.finish(), roof.finish(), info
