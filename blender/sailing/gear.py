"""Boat-park and water gear of the sailing school (docs/yacht_school.md "Meshes (part C)"):

  SM_Sail_Trolley_<Class>   launching trolley (galvanised frame, balloon wheels, padded bunks, handle). Origin on the ground
                            under the boat's origin; the boat sits at manifest "boat_origin" (its waterline-origin height)
  SM_Sail_Cover_<Class>     the winter / overnight cover over a de-rigged hull (sits on the hull: same origin as the boat)
  SM_Sail_SailBag           a rolled sail in its bag (2.4 m)
  SM_Sail_SparsRack         a free-standing rack for masts and booms (3 levels of arms)
  SM_Sail_Launch            the coach's aluminium launch («Казанка»-like, 4.6 m) with windscreen, wheel, benches, flag
                            staff, a megaphone and spare buoys aboard; origin at the waterline amidships, +X forward
  SM_Sail_Launch_Motor      the outboard («Вихрь»-like), origin on the steering axis at the transom clamp (yaw = steer,
                            pitch about +Y = tilt up)
  SM_Sail_Mark              race mark: orange inflatable cylinder 1.2 m (origin at the waterline)
  SM_Sail_Pin               the start line's pin buoy with a staff and an orange flag
  SM_Sail_Buoy              small yellow training buoy
  SM_Sail_MOB               man-overboard dummy: a lifebuoy ring with a dan pole and a flag
  SM_Sail_AnchorBlock       a concrete mooring block with an iron ring (sits on the sea bed)
  SM_Sail_Rope              1 m of 10 mm rope along +X from the origin (stretch it for sheets and lines)
  SM_Sail_Block             a single block, origin at its shackle (for sheets the engine draws)
"""
import math

from mathutils import Matrix, Vector

import boatspec as B
import boats as BT
from sailkit import Mesh, rz, ry, rx, tr, frame_from

KIT = "sailing_gear"


def wheel(m, c, r, w, seg=14):
    M = Matrix(((1, 0, 0), (0, 0, 1), (0, -1, 0))).transposed()      # lathe z -> -Y... axle along Y
    prof = [(0.0, -w / 2), (r * 0.55, -w / 2), (r * 0.85, -w / 2 - 0.004), (r, -w * 0.3), (r, w * 0.3),
            (r * 0.85, w / 2 + 0.004), (r * 0.55, w / 2), (0.0, w / 2)]
    m.lathe(prof, "SL_Rubber", seg=seg, c=c, axis=M)
    m.lathe([(0.0, -w / 2 - 0.006), (r * 0.42, -w / 2 - 0.006), (r * 0.42, w / 2 + 0.006), (0.0, w / 2 + 0.006)], "SL_White", seg=seg, c=c, axis=M)


def trolley(b):
    s = B.BOATS[b]
    m = Mesh("SM_Sail_Trolley_%s" % BT.CLASS_NAME[b])
    loa = s["loa"]
    track = {"luch": 1.30, "optimist": 1.05, "cadet": 1.20}[b]
    wr = 0.22 if b != "optimist" else 0.18
    kz = min(v for _, v in s["keel_z"])
    hc = 0.42 if b != "optimist" else 0.36           # cradle top above the ground (where the keel rests)
    z_boat = hc - kz
    # axle and the frame (galvanised tube)
    ax_x = -0.10 * loa
    m.tube((ax_x, -track / 2, wr), (ax_x, track / 2, wr), 0.02, "SL_Galv", seg=8)
    for sg in (1, -1):
        wheel(m, (ax_x, sg * track / 2, wr), wr, 0.20 if b != "optimist" else 0.16)
    fz = wr + 0.05
    xa, xf = -0.36 * loa, 0.30 * loa
    for sg in (1, -1):
        y = sg * 0.30
        m.pipe([(xa, y, fz + 0.04), (ax_x, y, fz), (xf, y * 0.4, fz + 0.02)], 0.02, "SL_Galv", seg=8)
    m.pipe([(xf, -0.12, fz + 0.02), (xf + 0.5, 0, fz + 0.12), (xf + 1.0, 0, 0.75)], 0.022, "SL_Galv", seg=8)
    m.pipe([(xf, 0.12, fz + 0.02), (xf + 0.5, 0, fz + 0.12)], 0.022, "SL_Galv", seg=8)
    m.tube((xf + 1.0, -0.25, 0.75), (xf + 1.0, 0.25, 0.75), 0.02, "SL_Galv", seg=8)
    m.tube((xf + 1.0, -0.22, 0.75), (xf + 1.0, 0.22, 0.75), 0.026, "SL_Black", seg=8)
    for x in (xa, ax_x, xf * 0.9):
        m.tube((x, -0.30, fz + 0.03), (x, 0.30, fz + 0.03), 0.018, "SL_Galv", seg=8)
    # padded bunks shaped to the hull bottom at two stations
    for xs in (-0.22 * loa, 0.18 * loa):
        sec = BT.section(b, xs)
        for sg in (1, -1):
            pts = [(xs, sg * y, z + z_boat - 0.035) for y, z in sec if y < 0.42]
            for p in pts[:-1]:
                pass
            pts2 = [p for p in pts if p[2] < z_boat + kz + 0.25]
            if len(pts2) >= 2:
                for a, c in zip(pts2, pts2[1:]):
                    m.beam(a, c, 0.07, 0.05, "SL_Canvas", up=(0, 0, 1))
                for p in (pts2[0], pts2[-1]):
                    m.tube((p[0], p[1], fz + 0.03), (p[0], p[1], p[2] - 0.02), 0.014, "SL_Galv", seg=6)
    # a bow-stop post with a winch strap hook
    bx = loa / 2 - 0.25
    bz = B.sheer(b, bx) + z_boat - 0.20
    m.pipe([(xf + 0.5, 0, fz + 0.12), (bx + 0.1, 0, bz)], 0.02, "SL_Galv", seg=8)
    m.box((bx + 0.08, 0, bz), (0.06, 0.12, 0.10), "SL_Rubber")
    m.closed = False
    return m, {"boat_origin": [0.0, 0.0, round(z_boat, 3)], "note": "the boat rests with its origin at boat_origin"}


def cover(b):
    """A tarpaulin over the hull from gunwale to gunwale (de-rigged, the mast off), with a skirt and tie-downs."""
    s = B.BOATS[b]
    m = Mesh("SM_Sail_Cover_%s" % BT.CLASS_NAME[b])
    xs = BT.stations(s, 16)
    rows = []
    for k, x in enumerate(xs):
        hb = B.half_beam(b, x) + 0.03
        sz = B.sheer(b, x)
        t = abs(k / (len(xs) - 1) - 0.5) * 2
        ridge = 0.10 + 0.06 * (1 - t * t)
        row = []
        for j in range(13):
            a = -1 + 2 * j / 12
            y = a * hb
            z = sz + 0.02 + ridge * math.cos(a * math.pi / 2) ** 0.8 - 0.015 * math.sin(j * 1.7 + k)
            if abs(a) > 0.99:
                z = sz - 0.12
                y = a * (hb + 0.01)
            row.append((x, y, z))
        rows.append(row)
    # ends: pull the first/last row in and down over the transoms
    for idx, dx in ((0, -0.04), (-1, 0.04)):
        rows[idx] = [(p[0] + dx, p[1] * 0.96, p[2] - 0.05) for p in rows[idx]]
    m.quad_grid(rows, "SL_CanvasBlue" if b == "luch" else "SL_Canvas", smooth=True)
    # end flaps down over the transoms
    for idx, sg in ((0, -1), (-1, 1)):
        top = rows[idx]
        bot = [(p[0] + sg * 0.01, p[1], max(p[2] - 0.20, B.sheer(b, p[0]) - 0.15)) for p in top]
        Vg, f = m.quad_grid([top, bot] if sg > 0 else [bot, top], "SL_CanvasBlue" if b == "luch" else "SL_Canvas")
    # tie-down ropes under the hull at three stations
    kz = min(v for _, v in s["keel_z"])
    for x in (xs[3], xs[8], xs[12]):
        hb = B.half_beam(b, x) + 0.04
        sec = BT.section(b, x)
        pts = [(x, -y - 0.012, z - 0.012) for y, z in reversed(sec)] + [(x, y + 0.012, z - 0.012) for y, z in sec[1:]]
        m.pipe(pts, 0.005, "SL_Rope", seg=4)
    m.closed = False
    return m


def sail_bag():
    m = Mesh("SM_Sail_SailBag")
    L, r = 2.4, 0.12
    M = Matrix(((0, 1, 0), (0, 0, 1), (1, 0, 0))).transposed()
    prof = [(0.0, -L / 2), (r * 0.8, -L / 2), (r, -L / 2 + 0.05)] + [(r * (1 + 0.03 * math.sin(k)), -L / 2 + 0.05 + (L - 0.1) * k / 10) for k in range(1, 10)] + [(r, L / 2 - 0.05), (r * 0.8, L / 2), (0.0, L / 2)]
    m.lathe(prof, "SL_CanvasBlue", seg=14, c=(0, 0, r), axis=M)
    for x in (-0.8, 0.0, 0.8):
        m.lathe([(r + 0.004, x - 0.025), (r + 0.006, x - 0.025), (r + 0.006, x + 0.025), (r + 0.004, x + 0.025), (r + 0.004, x - 0.025)],
                "SL_Strap", seg=14, c=(0, 0, r), axis=M, cap0=False, cap1=False)
    m.closed = False
    m.add_box_collision((0, 0, r), (L, 2 * r, 2 * r))
    return m


def spars_rack():
    m = Mesh("SM_Sail_SparsRack")
    L = 4.0
    for x in (-L / 2 + 0.2, 0.0, L / 2 - 0.2):
        m.bx("SL_WoodPaint", x - 0.05, x + 0.05, -0.05, 0.05, 0.0, 2.0)
        m.bx("SL_WoodPaint", x - 0.05, x + 0.05, -0.40, 0.40, 0.0, 0.08)
        for z in (0.6, 1.1, 1.6):
            m.bx("SL_WoodPaint", x - 0.04, x + 0.04, -0.38, 0.05, z - 0.03, z + 0.03)
            m.bx("SL_WoodPaint", x - 0.04, x + 0.04, -0.38, -0.34, z + 0.03, z + 0.10)
    m.bx("SL_WoodPaint", -L / 2 + 0.15, L / 2 - 0.15, -0.03, 0.03, 1.92, 1.98)
    m.closed = False
    m.add_box_collision((0, -0.15, 1.0), (L - 0.3, 0.5, 2.0))
    return m


# ------------------------------------------------------------------------------------------------ the coach's launch
LAUNCH = {
    "title": "Launch", "loa": 4.6, "beam": 1.70, "crew": 3,
    "sheer_z": [(0.0, 0.42), (0.5, 0.50), (0.85, 0.62), (1.0, 0.70)],
    "keel_z": [(0.0, -0.18), (0.4, -0.22), (0.75, -0.16), (0.92, 0.02), (1.0, 0.30)],
    "half_beam": [(0.0, 0.74), (0.45, 0.85), (0.70, 0.78), (0.88, 0.50), (1.0, 0.03)],
    "chine_frac": 0.82, "chine_up": 0.18, "deadrise": 0.14, "round": 0.0,
    "bow": "point", "deck": "open", "crown": 0.03,
}


def launch():
    B.BOATS["_launch"] = LAUNCH
    try:
        return _launch()
    finally:
        del B.BOATS["_launch"]


def _launch():
    b = "_launch"
    s = B.BOATS[b]
    m = Mesh("SM_Sail_Launch")
    xs = BT.stations(s, 22)
    rows = [BT.ring(b, x) for x in xs]
    m.quad_grid(rows, "SL_AluDull", smooth=True)
    t = 0.008
    inner = []
    xi0, xi1 = xs[0] + t, xs[-1] - 0.02
    xi = [min(max(x, xi0), xi1) for x in xs]
    xi[0], xi[-1] = xi0, xi1
    for x in xi:
        r = BT.ring(b, x, inset=t)
        sz = B.sheer(b, x)
        r[0] = (r[0][0], r[0][1], sz)
        r[-1] = (r[-1][0], r[-1][1], sz)
        inner.append(r)
    m.quad_grid(inner, "SL_AluDull", flip=True, smooth=True)
    m.face(rows[0], "SL_AluDull")
    m.face(list(reversed(inner[0])), "SL_AluDull")
    m.face(list(reversed(rows[-1])), "SL_AluDull")
    m.face(inner[-1], "SL_AluDull")
    n = len(xs)
    for k in range(n - 1):
        for side in (-1, 0):
            o0, o1 = rows[k][side], rows[k + 1][side]
            i0, i1 = inner[k][side], inner[k + 1][side]
            m.face([o0, o1, i1, i0] if side == -1 else [o0, i0, i1, o1], "SL_AluDull")
    for k, sg in ((0, 1), (n - 1, -1)):
        o_st, o_po, i_st, i_po = rows[k][0], rows[k][-1], inner[k][0], inner[k][-1]
        m.face([o_st, o_po, i_po, i_st] if sg > 0 else [o_st, i_st, i_po, o_po], "SL_AluDull")
    # rubbing strake, spray rails (chines), rivet lines are in the texture budget: skip
    sp = [(x, B.half_beam(b, x) + 0.01, B.sheer(b, x) - 0.03) for x in xs[1:-1]]
    m.pipe(sp, 0.02, "SL_Rubber", seg=6)
    m.pipe([(x, -y, z) for x, y, z in sp], 0.02, "SL_Rubber", seg=6)
    # foredeck (aluminium) and the windscreen
    fx = 0.95
    pts = [(x, B.half_beam(b, x) - 0.01) for x in [fx + (s["loa"] / 2 - 0.03 - fx) * k / 6 for k in range(7)]]
    poly = pts + [(p[0], -p[1]) for p in reversed(pts)]
    BT._plate_follow(m, b, poly, 0.01, "SL_White")
    wz = B.sheer(b, fx)
    hb = B.half_beam(b, fx) - 0.04
    ws = []
    for k in range(9):
        a = -1 + 2 * k / 8
        ws.append((fx + 0.12 * (1 - a * a), a * hb))
    for (x0, y0), (x1, y1) in zip(ws, ws[1:]):
        m.face([(x0, y0, wz + 0.02), (x1, y1, wz + 0.02), (x1 - 0.12, y1, wz + 0.40), (x0 - 0.12, y0, wz + 0.40)][::-1], "SL_Glass")
        m.face([(x0, y0, wz + 0.02), (x1, y1, wz + 0.02), (x1 - 0.12, y1, wz + 0.40), (x0 - 0.12, y0, wz + 0.40)], "SL_Glass")
        m.beam((x0 - 0.12, y0, wz + 0.41), (x1 - 0.12, y1, wz + 0.41), 0.02, 0.02, "SL_AluDull")
    # benches, the console with the wheel, floorboards
    for x, w in ((0.45, 0.30), (-0.70, 0.30), (-1.85, 0.32)):
        z = B.sheer(b, x) - 0.12
        hb = B.half_beam(b, x) - 0.05
        m.bx("SL_WoodPaint", x - w / 2, x + w / 2, -hb, hb, z - 0.015, z + 0.015)
    cz = B.lerp_keys(s["keel_z"], 0.6) + 0.10
    m.bx("SL_White", 0.55, 0.85, -0.62, -0.20, cz, B.sheer(b, 0.7) + 0.05)
    wc = Vector((0.52, -0.41, B.sheer(b, 0.7) + 0.02))
    M = Matrix(((0, 0, 1), (0, 1, 0), (-1, 0, 0))).transposed() @ Matrix.Rotation(math.radians(-25), 3, "Y")
    from boats import torus
    torus(m, wc, 0.16, 0.012, "SL_Black", seg=16, rs=5, M=Matrix(((0, 1, 0), (0, 0, 1), (1, 0, 0))).transposed() @ Matrix.Rotation(math.radians(25), 3, "X"))
    m.tube(wc, wc + Vector((0.12, 0, 0.05)), 0.012, "SL_Black", seg=6)
    for dy in (-0.25, 0.0, 0.25):
        m.bx("SL_WoodRaw", -2.0, 0.6, dy - 0.08, dy + 0.08, cz - 0.03, cz - 0.01)
    # transom pad for the motor, the stern flag staff with a red flag, cleats
    xt = -s["loa"] / 2
    m.bx("SL_WoodPaint", xt - 0.02, xt + 0.05, -0.20, 0.20, B.sheer(b, xt) - 0.20, B.sheer(b, xt) + 0.01)
    fy = 0.55
    m.tube((xt + 0.08, fy, B.sheer(b, xt)), (xt + 0.08, fy, B.sheer(b, xt) + 1.4), 0.012, "SL_White", seg=6)
    m.face([(xt + 0.08, fy, B.sheer(b, xt) + 1.38), (xt - 0.42, fy, B.sheer(b, xt) + 1.36),
            (xt - 0.42, fy, B.sheer(b, xt) + 1.04), (xt + 0.08, fy, B.sheer(b, xt) + 1.06)], "SL_Red")
    m.face([(xt + 0.08, fy, B.sheer(b, xt) + 1.06), (xt - 0.42, fy, B.sheer(b, xt) + 1.04),
            (xt - 0.42, fy, B.sheer(b, xt) + 1.36), (xt + 0.08, fy, B.sheer(b, xt) + 1.38)], "SL_Red")
    for x in (1.7, xt + 0.25):
        for sg in (1, -1):
            BT.cleat(m, x, sg * (B.half_beam(b, x) - 0.05), B.sheer(b, x), L=0.10)
    # megaphone on the bench, two spare marks (deflated: folded) and buoys on the floor
    mz = B.sheer(b, -0.7) - 0.105
    m.lathe([(0.0, 0.0), (0.03, 0.0), (0.04, 0.05), (0.13, 0.32), (0.135, 0.33), (0.0, 0.33)], "SL_White", seg=14,
            c=(-0.62, 0.30, mz + 0.14), axis=Matrix(((0, 1, 0), (0, 0, 1), (1, 0, 0))).transposed() @ Matrix.Rotation(math.radians(-90), 3, "X"))
    m.bx("SL_Black", -0.78, -0.70, 0.27, 0.33, mz, mz + 0.12)
    for p in ((-1.3, 0.35), (-1.45, 0.05)):
        m.sphere((p[0], p[1], cz + 0.14), 0.16, "SL_Yellow", seg=12, rings=8)
    m.bx("SL_Orange", -1.7, -1.2, -0.45, -0.15, cz, cz + 0.08)
    m.closed = False
    return m


def motor():
    m = Mesh("SM_Sail_Launch_Motor")
    # origin: on the steering axis at the clamp on the transom top; the leg goes down and the cowl sits above-aft
    m.bx("SL_Engine", -0.32, 0.02, -0.17, 0.17, 0.05, 0.42)                      # cowl
    m.bx("SL_Engine", -0.30, 0.0, -0.15, 0.15, 0.42, 0.47)
    m.bx("SL_White", -0.33, 0.03, -0.175, 0.175, 0.20, 0.23)                    # band
    m.bx("SL_Engine", -0.20, -0.05, -0.06, 0.06, -0.55, 0.06)                    # leg
    m.bx("SL_Engine", -0.32, 0.02, -0.11, 0.11, -0.40, -0.38)                    # cavitation plate
    M = Matrix(((0, 0, 1), (0, 1, 0), (-1, 0, 0))).transposed()
    m.lathe([(0.0, -0.06), (0.045, -0.06), (0.05, 0.12), (0.0, 0.18)], "SL_Engine", seg=10, c=(-0.12, 0, -0.52), axis=M)
    for k in range(3):     # propeller blades
        a = 2 * math.pi * k / 3
        c = Vector((-0.27, 0, -0.52))
        d = Vector((0, math.cos(a), math.sin(a)))
        m.beam(c, c + d * 0.13, 0.05, 0.008, "SL_Steel", up=(1, 0, 0))
    m.bx("SL_Steel", 0.0, 0.06, -0.10, 0.10, -0.12, 0.06)                         # clamp bracket
    m.tube((0.03, 0, 0.18), (0.55, 0, 0.30), 0.016, "SL_Black", seg=8)            # tiller arm
    m.tube((0.45, 0, 0.28), (0.58, 0, 0.31), 0.022, "SL_Black", seg=8)
    m.closed = False
    return m


def mark():
    m = Mesh("SM_Sail_Mark")
    r, H = 0.45, 1.20
    m.lathe([(0.0, -0.32), (0.10, -0.32), (r * 0.95, -0.28), (r, -0.22), (r, H - 0.32 - 0.05), (r * 0.9, H - 0.32), (0.0, H - 0.32)],
            "SL_Orange", seg=20, smooth=True)
    for z in (0.15, 0.55):
        m.lathe([(r + 0.002, z - 0.02), (r + 0.006, z - 0.02), (r + 0.006, z + 0.02), (r + 0.002, z + 0.02), (r + 0.002, z - 0.02)],
                "SL_White", seg=20, cap0=False, cap1=False)
    m.box((0, 0, -0.38), (0.05, 0.02, 0.10), "SL_Steel")
    m.closed = False
    return m


def pin():
    m = Mesh("SM_Sail_Pin")
    m.lathe([(0.0, -0.20), (0.22, -0.18), (0.26, 0.0), (0.22, 0.16), (0.0, 0.20)], "SL_Orange", seg=16)
    m.tube((0, 0, 0.18), (0, 0, 1.9), 0.018, "SL_White", seg=8)
    for sg in (1, -1):
        q = [(0.0, 0, 1.88), (0.0, 0, 1.52), (0.55, 0, 1.55), (0.55, 0, 1.86)]
        m.face(q if sg > 0 else q[::-1], "SL_Orange", col=(0, 1, 0, 1))
    m.closed = False
    return m


def buoy():
    m = Mesh("SM_Sail_Buoy")
    m.sphere((0, 0, 0.05), 0.22, "SL_Yellow", seg=16, rings=10)
    BT.torus(m, (0, 0, 0.30), 0.04, 0.008, "SL_Steel", seg=10, rs=4, M=Matrix(((1, 0, 0), (0, 0, 1), (0, -1, 0))).transposed())
    m.closed = False
    return m


def mob():
    m = Mesh("SM_Sail_MOB")
    BT.torus(m, (0, 0, 0.0), 0.30, 0.07, "SL_Orange", seg=20, rs=8)
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        c = Vector((math.cos(a) * 0.30, math.sin(a) * 0.30, 0))
        BT.torus(m, c, 0.073, 0.012, "SL_White", seg=10, rs=4,
                 M=Matrix(((-math.sin(a), math.cos(a), 0), (0, 0, 1), (math.cos(a), math.sin(a), 0))).transposed() @ Matrix.Identity(3))
    m.tube((0.45, 0, -0.6), (0.45, 0, 1.6), 0.015, "SL_Yellow", seg=6)
    m.lathe([(0.0, -0.05), (0.10, -0.05), (0.10, 0.25), (0.0, 0.25)], "SL_Orange", seg=12, c=(0.45, 0, -0.1))
    m.face([(0.45, 0, 1.6), (0.45, 0, 1.3), (0.85, 0, 1.33), (0.85, 0, 1.58)], "SL_Orange")
    m.face([(0.45, 0, 1.3), (0.45, 0, 1.6), (0.85, 0, 1.58), (0.85, 0, 1.33)], "SL_Orange")
    m.closed = False
    return m


def anchor_block():
    m = Mesh("SM_Sail_AnchorBlock")
    m.bx("CT_ConcreteDark", -0.30, 0.30, -0.30, 0.30, 0.0, 0.35)
    BT.torus(m, (0, 0, 0.42), 0.07, 0.014, "SL_Iron", seg=12, rs=5, M=Matrix(((1, 0, 0), (0, 0, 1), (0, -1, 0))).transposed())
    m.closed = False
    return m


def rope():
    m = Mesh("SM_Sail_Rope")
    m.tube((0, 0, 0), (1.0, 0, 0), 0.005, "SL_Rope", seg=6)
    return m


def single_block():
    m = Mesh("SM_Sail_Block")
    BT.block(m, (0, 0, -0.05), 0.03, axis=(0, 1, 0))
    m.closed = False
    return m


def build(reg):
    for b in ("luch", "optimist", "cadet"):
        tm, extra = trolley(b)
        reg.add(tm, KIT, collision="box", extra=extra)
        reg.add(cover(b), KIT)
    sb = sail_bag()
    reg.add(sb, KIT, collision="box")
    reg.add(spars_rack(), KIT, collision="box")
    reg.add(launch(), KIT, note="boat: no collision (the engine floats it)")
    reg.add(motor(), KIT, extra={"pivot_on_launch": [-2.32, 0.0, 0.43],
                                 "note": "steering axis at the transom clamp; yaw = steer, pitch = tilt"})
    for fn in (mark, pin, buoy, mob, anchor_block, rope, single_block):
        reg.add(fn(), KIT)
