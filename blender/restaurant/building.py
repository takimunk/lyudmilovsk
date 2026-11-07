"""The restaurant «Янтарь» (Yantar, "Amber"): a Soviet-modernist seaside pavilion on the Baltic promenade, built as a clean
enterable game mesh (design/restaurant/building_notes.md).

A round, fully glazed 24-sided drum under a thin, wide, slightly upturned concrete disc on slender tapering columns, a solid
ochre-tiled kitchen block behind a straight chord wall, all on a low podium with steps: 1960s-70s Baltic resort modernism
(the round seaside cafes and restaurants of the Latvian/Estonian coast) crossed with Niemeyer's discs on thin legs.

  Blender -b -P blender/restaurant/building.py
      -> design/restaurant/game/SM_Rest_{Shell,Inside,Glass,Furniture,Lamps,Sign}.glb
         unreal/Lyudmilovsk/Content/Restaurant/layout.json, design/restaurant/building_notes.md

Frame (restaurant_common.py): metres, origin on the finished floor at the drum's centre, +x the sea, -y the main door,
z up. Every doorway is an ALyudDoor hole of 122 x 222 cm (clear 110 x 215).
"""
import json
import math
import os
import sys

import bmesh
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import restaurant_common as C  # noqa: E402
from restaurant_common import M, U, bx, tube, sphere, oq, lathe, lathe_p, Opening, wall, poly48, exit_dist, angles, strips  # noqa: E402

LAYOUT = os.path.join(C.ROOT, "unreal", "Lyudmilovsk", "Content", "Restaurant", "layout.json")
NOTES = os.path.join(C.ROOT, "design", "restaurant", "building_notes.md")

# ---- the drum ---------------------------------------------------------------------------------------------------------
R_MID = 10.0                      # the glass wall's mid plane (apothem of the 24-gon: segment centres at 15 deg * k)
WT = 0.16
R_OUT, R_IN = R_MID + WT / 2, R_MID - WT / 2
HALF = 7.5                        # half a segment, degrees
CEIL = 4.0
SILL_Z = 0.35                     # the solid concrete sill band
TRANSOM = (2.5, 2.58)
HEAD_Z = 3.55                     # the bronze head band 3.55..4.0 (the ribs land on it)
DOOR_HOLE = (1.22, 2.22)
# ---- the chord wall and the kitchen block -----------------------------------------------------------------------------
CHORD_X = -8.4
CT = 0.25
CX_HALL, CX_K = CHORD_X + CT / 2, CHORD_X - CT / 2
K_X0 = -14.4
KT = 0.25
K_ROOF, K_CEIL = 3.6, 3.3
PASS_Y = (-1.7, 1.7)
PASS_Z = (0.95, 2.05)
PASS_SHELF = 1.05
KDOOR_Y = 3.6
BACKDOOR_Y = -2.5
# ---- the podium, the roof ---------------------------------------------------------------------------------------------
POD_R = 13.6
SKIRT = -1.2
GROUND = -0.6
RISE, TREAD, NSTEP = 0.15, 0.35, 4
ROOF_R = 14.0
SOFFIT_EDGE = 4.2
BREAK_R = 10.4                    # the soffit is flat at 4.0 out to here, then rises to 4.2 at the edge
ROOF_TOP, EDGE_TOP, RIM_TOP, RIM_IN = 4.6, 4.55, 4.8, 13.75
OC_IN, OC_OUT, CURB = 1.0, 2.0, 4.75
COL_R = 12.4
RIB_IN_W, RIB_IN_BOT = 0.12, 3.65
RIB_OUT_W, RIB_OUT_END = 0.16, 13.8
# ---- the hall ---------------------------------------------------------------------------------------------------------
PLANTER_R, PLANTER_IN, COLUMN_R = 1.3, 1.0, 0.35
RING = (5.6, 5.75)                # the amber inlay ring (on the walkway between the inner and outer tables)
TABLE_Z = 0.75
SEAT_Z = 0.46
R_ROUND, R_SQUARE = 0.5, 0.35     # table top half sizes (the cloth hangs 0.15 down, 15 mm / 7 mm out)
BAR = (-7.3, -6.7, -5.2, -2.0, 1.05)
PASS_COUNTER = (-8.275, -7.93, -1.75, 1.75, 0.92)
CLOAK = (1.5, 2.9, -8.6, -8.15, 1.02)

SEG_C = [15.0 * k for k in range(-12, 12)]
HALL_PLANES = [(U(c), R_IN) for c in SEG_C] + [(Vector((-1, 0)), -CX_HALL)]
FP_PLANES = [(U(c), R_OUT) for c in SEG_C] + [(Vector((-1, 0)), -CX_K)]


def vtx(apo, deg):
    return U(deg) * (apo / math.cos(math.radians(HALF)))


# where the chord meets the drum: A (outer, on the kitchen face), B (inner, on the hall face)
KY = (R_OUT - CX_K * math.cos(math.radians(150))) / math.sin(math.radians(150))
BY = (R_IN - CX_HALL * math.cos(math.radians(150))) / math.sin(math.radians(150))
TH_A = math.degrees(math.atan2(KY, CX_K))
TH_B = math.degrees(math.atan2(BY, CX_HALL))
KIX0, KIX1, KIY = K_X0 + KT, CX_K, KY - KT          # the kitchen's inner box
POD_KX = -math.sqrt(POD_R ** 2 - KY ** 2)            # the podium's edge meets the kitchen's side walls here
TH_P = math.degrees(math.atan2(KY, POD_KX))
SEA_X = math.sqrt(POD_R ** 2 - 4.0 ** 2)             # the sea steps' top edge (8 m wide)
DOOR_YS = -math.sqrt(POD_R ** 2 - 1.2 ** 2)          # the door steps' top edge (2.4 m wide)
TH_SEA = math.degrees(math.atan2(4.0, SEA_X))
TH_FRONT = math.degrees(math.atan2(1.2, -DOOR_YS))


def hall_r(deg):
    return exit_dist(Vector((0, 0)), U(deg), HALL_PLANES)


def fp_r(deg):
    return exit_dist(Vector((0, 0)), U(deg), FP_PLANES)


def soffit_z(r):
    return CEIL if r <= BREAK_R else CEIL + (SOFFIT_EDGE - CEIL) * (r - BREAK_R) / (ROOF_R - BREAK_R)


# ---- the drum's glazing ------------------------------------------------------------------------------------------------

def drum(sh, gl):
    zs = [0.0, SILL_Z, TRANSOM[0], TRANSOM[1], HEAD_Z, CEIL, 2.22, 2.32]

    def cell(u, z, inner):
        return sh, M("RS_Concrete") if z < SILL_Z else M("RS_Bronze")

    def reveal(o, side):
        if side == "sill":
            return sh, M("RS_Granite") if o.kind == "door" else M("RS_Concrete")
        return sh, M("RS_Bronze")

    for c in SEG_C:
        if abs(c) > 150.5:
            continue                                   # behind the chord: the kitchen block
        p0, p1 = vtx(R_OUT, c - HALF), vtx(R_OUT, c + HALF)
        L = (p1 - p0).length
        d = (p1 - p0) / L
        n = Vector((d.y, -d.x))
        q0, q1 = vtx(R_IN, c - HALF), vtx(R_IN, c + HALF)
        ui = [(q0 - p0).dot(d), (q1 - p0).dot(d)]
        uo = [0.0, L]
        if abs(c) > 149.5:                             # cut by the chord wall: outer face to its kitchen face, inner to the hall face
            u_out = (CX_K - p0.x) / d.x
            u_in = (CX_HALL - (p0.x - n.x * WT)) / d.x
            if c > 0:
                uo[1], ui[1] = u_out, u_in
            else:
                uo[0], ui[0] = u_out, u_in
        a, b = max(uo[0], ui[0]) + 0.07, min(uo[1], ui[1]) - 0.07
        opens = []
        if abs(c) > 149.5:
            for z0, z1 in ((SILL_Z, TRANSOM[0]), (TRANSOM[1], HEAD_Z)):
                opens.append(Opening(a, b, z0, z1))
        elif c == -90.0:                               # the main door: sidelights, a transom, two panes over it
            um = L / 2
            opens.append(Opening(um - DOOR_HOLE[0] / 2, um + DOOR_HOLE[0] / 2, 0.0, DOOR_HOLE[1], "door"))
            opens.append(Opening(a, um - DOOR_HOLE[0] / 2 - 0.1, SILL_Z, DOOR_HOLE[1]))
            opens.append(Opening(um + DOOR_HOLE[0] / 2 + 0.1, b, SILL_Z, DOOR_HOLE[1]))
            opens.append(Opening(a, um - 0.05, 2.32, HEAD_Z))
            opens.append(Opening(um + 0.05, b, 2.32, HEAD_Z))
        else:                                          # two panes wide, a transom: four panes per segment
            um = L / 2
            for z0, z1 in ((SILL_Z, TRANSOM[0]), (TRANSOM[1], HEAD_Z)):
                opens.append(Opening(a, um - 0.05, z0, z1))
                opens.append(Opening(um + 0.05, b, z0, z1))
        wall(p0, p1, WT, opens, cell, reveal, glass=gl, uo=tuple(uo), ui=tuple(ui), zs=zs)


def chord(sh, ins):
    """The straight wall at x = -8.4: the pass and the kitchen door; its kitchen face is the outer face (-x)."""
    p0, p1 = Vector((CX_K, KY)), Vector((CX_K, -KY))
    L = 2 * KY
    uk = (KY - KIY, KY + KIY)
    opens = [Opening(KY - PASS_Y[1], KY - PASS_Y[0], PASS_Z[0], PASS_Z[1], "pass"),
             Opening(KY - KDOOR_Y - DOOR_HOLE[0] / 2, KY - KDOOR_Y + DOOR_HOLE[0] / 2, 0.0, DOOR_HOLE[1], "door")]

    def cell(u, z, inner):
        if inner:
            return ins, M("RS_Plaster")
        if z < K_CEIL:
            if uk[0] < u < uk[1]:
                return ins, M("RS_WallTile") if z < 1.8 else M("RS_Plaster")
            return None
        if z < K_ROOF:
            return None
        return sh, M("RS_Concrete")                     # over the kitchen roof, under the disc

    def reveal(o, side):
        if side == "sill":
            return ins, M("RS_Steel") if o.kind == "pass" else M("RS_Granite")
        return ins, M("RS_Plaster")

    wall(p0, p1, CT, opens, cell, reveal, uo=(0.0, L), ui=(KY - BY, KY + BY), zs=[0.0, 1.8, K_CEIL, K_ROOF, CEIL], us=uk)


def kitchen_block(sh, ins, gl):
    A, B_, C_, D_ = Vector((CX_K, KY)), Vector((K_X0, KY)), Vector((K_X0, -KY)), Vector((CX_K, -KY))
    win = 0.5
    wz = (2.55, 3.05)
    zs = [SKIRT, 0.0, 1.8, wz[0], wz[1], K_CEIL, 3.35, K_ROOF, 2.22]

    def out_mat(z):
        if z < 0:
            return M("RS_Granite")
        if z > 3.35:
            return M("RS_Concrete")
        return M("RS_AmberTile")

    def make_cell(buried):
        def cell(u, z, inner):
            if inner:
                if z > K_CEIL or z < 0:
                    return None
                return ins, M("RS_WallTile") if z < 1.8 else M("RS_Plaster")
            if z < 0 and buried(u):
                return None
            return sh, out_mat(z)
        return cell

    def reveal(o, side):
        if o.kind == "door" and side == "sill":
            return sh, M("RS_Granite")
        return sh, M("RS_Concrete")

    u_pod1 = CX_K - POD_KX                        # side walls: under the podium's top, no outer face below 0
    u_pod3 = POD_KX - K_X0
    land = (KY - (BACKDOOR_Y + 0.8), KY - (BACKDOOR_Y - 0.8))
    # north side wall (y = +KY), from the chord to the back
    opens = [Opening(u - win / 2, u + win / 2, wz[0], wz[1]) for u in (1.075, 2.075, 3.075, 4.075, 5.075)]
    wall(A, B_, KT, opens, make_cell(lambda u: u < u_pod1), reveal, glass=gl, ui=(0.0, A.x - KIX0), zs=zs, us=[u_pod1])
    # the back wall (x = -14.4): the staff door, windows over it
    ub = KY - BACKDOOR_Y
    opens = [Opening(ub - DOOR_HOLE[0] / 2, ub + DOOR_HOLE[0] / 2, 0.0, DOOR_HOLE[1], "door")]
    opens += [Opening(KY - y - win / 2, KY - y + win / 2, wz[0], wz[1]) for y in (4.5, 3.5, 2.5, 1.5, 0.5, -0.5, -1.5, -2.5, -3.5, -4.5)]
    wall(B_, C_, KT, opens, make_cell(lambda u: land[0] < u < land[1]), reveal, glass=gl, ui=(KT, 2 * KY - KT), zs=zs, us=list(land))
    # south side wall (y = -KY)
    opens = [Opening(u - win / 2, u + win / 2, wz[0], wz[1]) for u in (0.8, 1.8, 2.8, 3.8, 4.8)]
    wall(C_, D_, KT, opens, make_cell(lambda u: u > u_pod3), reveal, glass=gl, ui=(KT, CX_K - K_X0), zs=zs, us=[u_pod3])
    # roof, ceiling, floor
    oq(sh, [(K_X0, -KY, K_ROOF), (CX_K, -KY, K_ROOF), (CX_K, KY, K_ROOF), (K_X0, KY, K_ROOF)], M("RS_Roof"), (0, 0, 1))
    oq(ins, [(KIX0, -KIY, K_CEIL), (KIX1, -KIY, K_CEIL), (KIX1, KIY, K_CEIL), (KIX0, KIY, K_CEIL)], M("RS_Ceiling"), (0, 0, -1))
    oq(ins, [(KIX0, -KIY, 0.0), (KIX1, -KIY, 0.0), (KIX1, KIY, 0.0), (KIX0, KIY, 0.0)], M("RS_KitchenTile"), (0, 0, 1))
    # the yard landing and steps at the staff door
    steps(sh, Vector((K_X0, BACKDOOR_Y)), Vector((-1, 0)), 0.8, landing=1.2)


def steps(sh, o, e, hw, landing=0.0):
    """A flight of NSTEP steps (RISE x TREAD) going out along e from the edge point o at z=0, half width hw;
    an optional landing at z=0 first. The first riser (or the landing) closes the podium's edge."""
    t = Vector((-e.y, e.x))
    mi = M("RS_Granite")

    def P(s, w, z):
        p = o + e * s + t * w
        return (p.x, p.y, z)

    secs = []                                         # (s0, s1, top z)
    if landing > 0:
        secs.append((0.0, landing, 0.0))
    for i in range(NSTEP):
        s0 = landing + i * TREAD
        secs.append((s0, s0 + TREAD, -RISE * (i + 1)))
    zprev = 0.0
    for s0, s1, z in secs:
        if z < zprev - 1e-6:
            oq(sh, [P(s0, -hw, z), P(s0, hw, z), P(s0, hw, zprev), P(s0, -hw, zprev)], mi, (e.x, e.y, 0))      # riser
        oq(sh, [P(s0, -hw, z), P(s0, hw, z), P(s1, hw, z), P(s1, -hw, z)], mi, (0, 0, 1))                   # tread
        for w in (-hw, hw):
            oq(sh, [P(s0, w, SKIRT), P(s1, w, SKIRT), P(s1, w, z), P(s0, w, z)], mi, (t.x * w, t.y * w, 0))   # cheeks
        zprev = z
    s_end = secs[-1][1]
    oq(sh, [P(s_end, -hw, SKIRT), P(s_end, hw, SKIRT), P(s_end, hw, zprev), P(s_end, -hw, zprev)], mi, (e.x, e.y, 0))


def podium(sh):
    def pin(deg):
        a = abs(deg)
        if a <= TH_A + 1e-9:
            return fp_r(deg)
        return KY / abs(math.sin(math.radians(deg)))

    def pout(deg):
        if abs(deg) <= TH_SEA + 1e-9:
            return SEA_X / math.cos(math.radians(deg))
        if abs(deg + 90) <= TH_FRONT + 1e-9:
            return -DOOR_YS / abs(math.sin(math.radians(deg)))
        return POD_R

    def flat(deg):
        return abs(deg) <= TH_SEA + 1e-9 or abs(deg + 90) <= TH_FRONT + 1e-9

    ex = [TH_A, -TH_A, TH_P, -TH_P, TH_SEA, -TH_SEA, -90 + TH_FRONT, -90 - TH_FRONT]
    angs = angles(-TH_P, TH_P, 3.75, ex)
    ring_in = lambda a: tuple(U(a) * pin(a)) + (0.0,)  # noqa: E731
    ring_out = lambda a: tuple(U(a) * pout(a)) + (0.0,)  # noqa: E731
    strips(sh, angs, [ring_in, ring_out], [M("RS_Paving")], 1, closed=False)
    for a0, a1 in zip(angs, angs[1:]):
        if flat(a0) and flat(a1) and flat((a0 + a1) / 2):
            continue                                   # the steps close it
        q0, q1 = U(a0) * pout(a0), U(a1) * pout(a1)
        m = U((a0 + a1) / 2)
        oq(sh, [(q0.x, q0.y, SKIRT), (q1.x, q1.y, SKIRT), (q1.x, q1.y, 0.0), (q0.x, q0.y, 0.0)], M("RS_Granite"), (m.x, m.y, 0))
    steps(sh, Vector((0.0, DOOR_YS)), Vector((0, -1)), 1.2)
    steps(sh, Vector((SEA_X, 0.0)), Vector((1, 0)), 4.0)


# ---- the roof ---------------------------------------------------------------------------------------------------------

def rib(bm, deg, w, s0f, s1f, ztop, zbot, mi, start=False, end=False):
    d = U(deg)
    t = Vector((-d.y, d.x))
    side = {}
    for k in (-1, 1):
        o = t * (w * k)
        side[k] = (o, s0f(o, d), s1f(o, d))

    def P(k, s, z):
        p = side[k][0] + d * s
        return (p.x, p.y, z)
    for k in (-1, 1):
        o, s0, s1 = side[k]
        oq(bm, [P(k, s0, zbot(s0)), P(k, s1, zbot(s1)), P(k, s1, ztop(s1)), P(k, s0, ztop(s0))], mi, (t.x * k, t.y * k, 0))
    (_, a0, a1), (_, b0, b1) = side[-1], side[1]
    oq(bm, [P(-1, a0, zbot(a0)), P(-1, a1, zbot(a1)), P(1, b1, zbot(b1)), P(1, b0, zbot(b0))], mi, (0, 0, -1))
    if start:
        oq(bm, [P(-1, a0, zbot(a0)), P(1, b0, zbot(b0)), P(1, b0, ztop(b0)), P(-1, a0, ztop(a0))], mi, (-d.x, -d.y, 0))
    if end:
        oq(bm, [P(-1, a1, zbot(a1)), P(1, b1, zbot(b1)), P(1, b1, ztop(b1)), P(-1, a1, ztop(a1))], mi, (d.x, d.y, 0))


OUT_RIB_TOP = lambda s: CEIL + (4.25 - CEIL) * (s - 8.4) / (ROOF_R - 8.4)  # noqa: E731  (always above the soffit: hidden)
OUT_RIB_BOT = lambda s: 3.68 + 0.1046 * (s - R_OUT)  # noqa: E731
RIB_ANGLES = [15.0 + 30.0 * k for k in range(12)]
COL_ANGLES = [a for a in RIB_ANGLES if abs(((a + 180) % 360) - 180) < 160]     # not the two inside the kitchen block


def roof(sh, ins, gl):
    # the disc: edge, rim, top, the oculus curbs and well (lathe, 48 sides)
    lathe(sh, [(ROOF_R, SOFFIT_EDGE), (ROOF_R, RIM_TOP), (RIM_IN, RIM_TOP), (RIM_IN, EDGE_TOP), (BREAK_R, ROOF_TOP),
               (OC_OUT + 0.15, ROOF_TOP), (OC_OUT + 0.15, CURB), (OC_OUT, CURB), (OC_OUT, CEIL)],
          [M("RS_Concrete"), M("RS_Concrete"), M("RS_Concrete"), M("RS_RoofTop"), M("RS_RoofTop"), M("RS_Concrete"),
           M("RS_Concrete"), M("RS_Ceiling")], seg=48, cap0=False, cap1=False)
    lathe(sh, [(OC_IN, CEIL), (OC_IN, CURB), (OC_IN - 0.15, CURB), (OC_IN - 0.15, ROOF_TOP)],
          [M("RS_Ceiling"), M("RS_Concrete"), M("RS_Concrete"), M("RS_RoofTop")], seg=48, cap0=True, cap1=True)
    # the oculus glass: a ring between the curbs, 6 mm thick
    lathe(gl, [(OC_OUT, CURB - 0.047), (OC_IN, CURB - 0.047)], [M("RS_Glass")], seg=48, cap0=False, cap1=False)
    lathe(gl, [(OC_IN, CURB - 0.053), (OC_OUT, CURB - 0.053)], [M("RS_Glass")], seg=48, cap0=False, cap1=False)
    # the soffit outside the glass: flat to BREAK_R, then rising to the edge
    angs = angles(-180, 179.999, 7.5, [TH_A, -TH_A])
    r0 = lambda a: tuple(U(a) * fp_r(a)) + (CEIL,)  # noqa: E731
    r1 = lambda a: tuple(poly48(BREAK_R, a)) + (CEIL,)  # noqa: E731
    r2 = lambda a: tuple(poly48(ROOF_R, a)) + (SOFFIT_EDGE,)  # noqa: E731
    strips(sh, angs, [r0, r1, r2], [M("RS_Soffit"), M("RS_Soffit")], -1, closed=True)
    # the hall ceiling between the oculus and the glass / the chord wall
    angs = angles(-180, 179.999, 7.5, [TH_B, -TH_B])
    c0 = lambda a: tuple(poly48(OC_OUT, a)) + (CEIL,)  # noqa: E731
    c1 = lambda a: tuple(U(a) * hall_r(a)) + (CEIL,)  # noqa: E731
    strips(ins, angs, [c0, c1], [M("RS_Ceiling")], -1, closed=True)
    # ribs: 12 inside (from the oculus to the glass's head band or the chord wall), 10 under the overhang
    for a in RIB_ANGLES:
        rib(ins, a, RIB_IN_W, lambda o, d: OC_OUT, lambda o, d: exit_dist(o, d, HALL_PLANES),
            lambda s: CEIL, lambda s: RIB_IN_BOT, M("RS_Ceiling"), start=True)
        if a in COL_ANGLES:
            rib(sh, a, RIB_OUT_W, lambda o, d: exit_dist(o, d, FP_PLANES), lambda o, d: RIB_OUT_END,
                OUT_RIB_TOP, OUT_RIB_BOT, M("RS_Soffit"), end=True)


def columns(sh):
    for a in COL_ANGLES:
        p = U(a) * COL_R
        tube(sh, (p.x, p.y, 0.0), (p.x, p.y, OUT_RIB_BOT(COL_R) + 0.04), 0.10, M("RS_Concrete"), seg=16, caps=False, r1=0.14)


# ---- inside -----------------------------------------------------------------------------------------------------------

def hall_floor(ins):
    angs = angles(-180, 179.999, 7.5, [TH_B, -TH_B])
    rings = [lambda a, r=r: tuple(poly48(r, a)) + (0.0,) for r in (PLANTER_R, PLANTER_R + 0.2, RING[0], RING[1])]
    rings.append(lambda a: tuple(U(a) * hall_r(a)) + (0.0,))
    strips(ins, angs, rings, [M("RS_TerrazzoDark"), M("RS_Terrazzo"), M("RS_TerrazzoAmber"), M("RS_Terrazzo")], 1, closed=True)
    # the planter-bench ring round the central column: concrete side, walnut seat, flat soil
    lathe(ins, [(PLANTER_R, 0.0), (PLANTER_R, 0.45), (PLANTER_IN, 0.45), (PLANTER_IN, 0.40), (COLUMN_R, 0.40)],
          [M("RS_Concrete"), M("RS_Walnut"), M("RS_Concrete"), M("RS_Soil")], seg=48, cap0=False, cap1=False)
    lathe(ins, [(COLUMN_R, 0.40), (COLUMN_R, CEIL)], [M("RS_Concrete")], seg=24, cap0=False, cap1=False)


def pass_and_bar(ins):
    # the pass shelf through the chord wall (stainless, 0.45 deep), 1 cm off the jambs
    bx(ins, M("RS_Steel"), CHORD_X - 0.225, CHORD_X + 0.225, PASS_Y[0] + 0.01, PASS_Y[1] - 0.01, PASS_SHELF - 0.04, PASS_SHELF)
    # the heat-lamp bar over it in the hall, four lamps
    bx(ins, M("RS_Steel"), CX_HALL, CX_HALL + 0.38, -1.62, 1.62, 2.12, 2.2, faces="XyYzZ")
    for y in (-1.2, -0.4, 0.4, 1.2):
        x = CX_HALL + 0.13
        tube(ins, (x, y, 2.12), (x, y, 2.0), 0.008, M("RS_Steel"), seg=6, caps=False)
        lathe(ins, [(0.11, 1.86), (0.11, 1.9), (0.04, 2.0)], [M("RS_Steel")], seg=12, c=(x, y, 0), cap0=False, cap1=True)
        lathe(ins, [(0.09, 1.88), (0.09, 1.9)], [M("RS_HeatLamp")], seg=12, c=(x, y, 0), cap0=True, cap1=False)
    # the low counter under the pass on the hall side
    x0, x1, y0, y1, top = PASS_COUNTER
    bx(ins, M("RS_Walnut"), x0, x1 - 0.02, y0 + 0.02, y1 - 0.02, 0.0, top - 0.03, faces="XyY")
    bx(ins, M("RS_Steel"), x0, x1, y0, y1, top - 0.03, top, faces="XyYzZ")
    # the kitchen side of the pass: a stainless counter
    bx(ins, M("RS_Steel"), -9.2, CX_K, -1.8, 1.8, 0.0, 0.92, faces="xyYZ")
    # the bar along the chord wall
    x0, x1, y0, y1, top = BAR
    bx(ins, M("RS_Walnut"), x0, x1, y0, y1, 0.0, top - 0.04, faces="xXyY")
    bx(ins, M("RS_DarkWood"), x0 - 0.03, x1 + 0.04, y0 - 0.03, y1 + 0.03, top - 0.04, top)
    tube(ins, (x1 + 0.12, y0 + 0.1, 0.2), (x1 + 0.12, y1 - 0.1, 0.2), 0.022, M("RS_Bronze"), seg=8)
    for y in (y0 + 0.2, (y0 + y1) / 2, y1 - 0.2):
        tube(ins, (x1 + 0.12, y, 0.2), (x1 + 0.005, y, 0.2), 0.014, M("RS_Bronze"), seg=6, caps=False)
    # behind it: a low cupboard and shelves with bottles on the chord wall
    sy0, sy1 = -5.1, -2.1
    bx(ins, M("RS_Walnut"), CX_HALL, CX_HALL + 0.4, sy0, sy1, 0.0, 0.9, faces="XyYZ")
    for y in (sy0, sy1 - 0.02):
        bx(ins, M("RS_Walnut"), CX_HALL, CX_HALL + 0.28, y, y + 0.02, 0.9, 2.12, faces="XyYZ")
    k = 0
    for z in (1.3, 1.65, 2.0):
        bx(ins, M("RS_Walnut"), CX_HALL, CX_HALL + 0.28, sy0 + 0.02, sy1 - 0.02, z, z + 0.02, faces="XzZ")
    for z in (0.9, 1.32, 1.67, 2.02):
        if z > 2.0:
            continue
        for i in range(9):
            y = sy0 + 0.2 + i * 0.32
            k += 1
            h = 0.26 if k % 3 else 0.3
            mi = M("RS_Bottle") if k % 2 else M("RS_BottleAmber")
            lathe(ins, [(0.035, z), (0.035, z + h * 0.65), (0.012, z + h * 0.85), (0.012, z + h)], [mi], seg=8,
                  c=(CX_HALL + 0.14, y, 0), cap0=False, cap1=True)


def cloakroom(ins):
    x0, x1, y0, y1, top = CLOAK
    bx(ins, M("RS_Walnut"), x0, x1, y0, y1, 0.0, top - 0.04, faces="xXyY")
    bx(ins, M("RS_DarkWood"), x0 - 0.03, x1 + 0.03, y0 - 0.03, y1 + 0.03, top - 0.04, top)
    for x in (x0 + 0.1, x1 - 0.1):
        tube(ins, (x, -9.05, 0.0), (x, -9.05, 1.8), 0.02, M("RS_Bronze"), seg=8, caps=False)
        sphere(ins, (x, -9.05, 1.82), 0.03, M("RS_Bronze"), seg=8, rings=4)
    tube(ins, (x0 + 0.1, -9.05, 1.72), (x1 - 0.1, -9.05, 1.72), 0.015, M("RS_Bronze"), seg=8)


def kitchen_kit(ins):
    # the range against the back wall with four plates and a hood
    bx(ins, M("RS_Dark"), KIX0, KIX0 + 0.75, 0.4, 2.4, 0.0, 0.9, faces="XyYZ")
    for i in range(2):
        for j in range(2):
            lathe(ins, [(0.12, 0.9), (0.12, 0.915)], [M("RS_Steel")], seg=12, c=(KIX0 + 0.22 + i * 0.32, 0.9 + j * 1.0, 0), cap0=False, cap1=True)
    tube(ins, (KIX0 + 0.79, 0.6, 0.75), (KIX0 + 0.79, 2.2, 0.75), 0.012, M("RS_Steel"), seg=6)
    bx(ins, M("RS_Steel"), KIX0, KIX0 + 0.85, 0.3, 2.5, 1.95, 2.35, faces="XyYzZ")
    # the worktable in the middle
    bx(ins, M("RS_Steel"), -11.6, -10.4, -0.7, 0.7, 0.86, 0.9)
    bx(ins, M("RS_Steel"), -11.55, -10.45, -0.65, 0.65, 0.2, 0.22)
    for x in (-11.55, -10.45):
        for y in (-0.65, 0.65):
            tube(ins, (x, y, 0.0), (x, y, 0.86), 0.02, M("RS_Steel"), seg=6, caps=False)
    # the sink and the tall fridge on the north wall
    bx(ins, M("RS_Steel"), -13.6, -12.4, KIY - 0.64, KIY, 0.0, 0.9, faces="xXyZ")
    bx(ins, M("RS_Dark"), -13.3, -12.7, KIY - 0.55, KIY - 0.15, 0.9, 0.93, faces="xXyYZ")
    tube(ins, (-13.0, KIY - 0.08, 0.9), (-13.0, KIY - 0.08, 1.2), 0.012, M("RS_Steel"), seg=6, caps=False)
    tube(ins, (-13.0, KIY - 0.08, 1.2), (-13.0, KIY - 0.3, 1.2), 0.012, M("RS_Steel"), seg=6)
    bx(ins, M("RS_Enamel"), -12.1, -11.3, KIY - 0.7, KIY, 0.0, 2.0, faces="xXyZ")
    tube(ins, (-11.4, KIY - 0.73, 0.95), (-11.4, KIY - 0.73, 1.45), 0.014, M("RS_Steel"), seg=6)
    # shelves with pots on the south wall
    for x in (-13.0, -10.6):
        bx(ins, M("RS_Steel"), x - 0.02, x + 0.02, -KIY, -KIY + 0.4, 0.0, 2.0, faces="xXYZ")
    for z in (0.4, 1.0, 1.6):
        bx(ins, M("RS_Steel"), -12.98, -10.62, -KIY, -KIY + 0.4, z, z + 0.025, faces="YzZ")
        for i, (x, r, h) in enumerate(((-12.6, 0.13, 0.16), (-12.0, 0.11, 0.2), (-11.4, 0.14, 0.12), (-10.95, 0.1, 0.14))):
            if (i + int(z * 10)) % 4 == 3:
                continue
            lathe(ins, [(r, z + 0.025), (r, z + 0.025 + h)], [M("RS_Copper") if i % 2 else M("RS_Steel")], seg=12,
                  c=(x, -KIY + 0.2, 0), cap0=False, cap1=True)


def menu_stand(sh):
    """A bronze lectern for the menu on the terrace right of the main door."""
    x, y = 2.0, -11.3
    tube(sh, (x, y, 0.0), (x, y, 1.02), 0.025, M("RS_Bronze"), seg=8, caps=False)
    lathe(sh, [(0.18, 0.0), (0.16, 0.02), (0.03, 0.03)], [M("RS_Bronze")], seg=12, c=(x, y, 0), cap0=False, cap1=True)
    rot = Matrix.Rotation(math.radians(-35), 3, "X")
    C.box(sh, (x, y, 1.1), (0.42, 0.3, 0.03), M("RS_Bronze"), rot=rot)
    return [x, y - 0.012, 1.1 + 0.02]


# ---- furniture --------------------------------------------------------------------------------------------------------

def chair(bm, at, yaw):
    """A 1960s moulded-plywood chair: seat front edge over `at`, the sitter facing yaw (deg); nothing over the seat."""
    s = C.mark(bm)
    bx(bm, M("RS_Walnut"), -0.42, 0.0, -0.21, 0.21, SEAT_Z - 0.03, SEAT_Z)
    # the curved back behind the seat's rear edge
    ys = [-0.21 + 0.105 * j for j in range(5)]
    xs = [-0.445 - 0.04 * (y / 0.21) ** 2 for y in ys]
    z0, z1, th = 0.62, 0.86, 0.02
    for j in range(4):
        a, b = (xs[j], ys[j]), (xs[j + 1], ys[j + 1])
        for off, want in ((0.0, 1), (-th, -1)):
            oq(bm, [(a[0] + off, a[1], z0), (b[0] + off, b[1], z0), (b[0] + off, b[1], z1), (a[0] + off, a[1], z1)], M("RS_Walnut"), (want, 0, 0))
        for z, want in ((z0, -1), (z1, 1)):
            oq(bm, [(a[0], a[1], z), (b[0], b[1], z), (b[0] - th, b[1], z), (a[0] - th, a[1], z)], M("RS_Walnut"), (0, 0, want))
    for j, w in ((0, -1), (4, 1)):
        oq(bm, [(xs[j], ys[j], z0), (xs[j] - th, ys[j], z0), (xs[j] - th, ys[j], z1), (xs[j], ys[j], z1)], M("RS_Walnut"), (0, w, 0))
    # legs: front ones straight, the rear ones running up behind the back
    for y in (-0.18, 0.18):
        tube(bm, (-0.04, y, 0.0), (-0.04, y, SEAT_Z - 0.03), 0.012, M("RS_DarkWood"), seg=6, caps=False, r1=0.016)
        tube(bm, (-0.39, y * 1.05, 0.0), (-0.47, y, 0.8), 0.012, M("RS_DarkWood"), seg=6, caps=False, r1=0.015)
    C.place(bm, s, Matrix.Translation((at[0], at[1], 0.0)) @ Matrix.Rotation(math.radians(yaw), 4, "Z"))


CHAIR_FOOT = [(-0.5, -0.23), (0.0, -0.23), (0.0, 0.23), (-0.5, 0.23)]


def table(bm, c, shape, yaw):
    s = C.mark(bm)
    if shape == "round":
        seg, ph, k = 24, 0.0, 1.0
        top, skirt, inner = R_ROUND, R_ROUND + 0.015, R_ROUND + 0.005
    else:
        seg, ph, k = 4, math.pi / 4, 1 / math.cos(math.pi / 4)
        top, skirt, inner = R_SQUARE, R_SQUARE + 0.007, R_SQUARE - 0.007
    hem = TABLE_Z - 0.15
    lathe_p(bm, [(inner * k, TABLE_Z - 0.018), (inner * k, hem), (skirt * k, hem), (skirt * k, TABLE_Z - 0.01), (top * k, TABLE_Z)],
            [M("RS_Linen")], seg=seg, cap0=True, cap1=True, phase=ph)
    tube(bm, (0, 0, 0.04), (0, 0, TABLE_Z - 0.018), 0.04, M("RS_Bronze"), seg=10, caps=False)
    lathe(bm, [(0.26, 0.0), (0.25, 0.025), (0.05, 0.045)], [M("RS_Bronze")], seg=16, cap0=False, cap1=True)
    C.place(bm, s, Matrix.Translation((c[0], c[1], 0.0)) @ Matrix.Rotation(math.radians(yaw), 4, "Z"))


ROUND_TABLES = [(0.0, True), (45.0, True), (-45.0, False), (90.0, False), (135.0, False), (-120.0, False)]
SQUARE_TABLES = [(30.0, True), (-30.0, True), (150.0, False), (-150.0, False)]
R_OUTER, R_INNER = 7.2, 4.0


def table_plan():
    """Tables, seats and serve points (the layout's tables, without the walk node)."""
    out = []
    i = 0
    for a, hero in ROUND_TABLES:
        i += 1
        c = U(a) * R_OUTER
        seats = []
        for j in range(4):
            dd = U(a + 90 * j)
            seats.append({"dir": a + 90 * j, "at": c + dd * (R_ROUND + 0.03), "yaw": a + 90 * j + 180,
                          "place": c + dd * (R_ROUND - 0.22)})
        out.append({"id": "t%d" % i, "c": c, "shape": "round", "size": [2 * R_ROUND, 2 * R_ROUND], "hero": hero, "yaw": a,
                    "seats": seats, "serve": c + U(a + 135) * (R_ROUND + 0.45), "a": a})
    for a, hero in SQUARE_TABLES:
        i += 1
        c = U(a) * R_INNER
        seats = []
        for j in (1, 3):
            dd = U(a + 90 * j)
            seats.append({"dir": a + 90 * j, "at": c + dd * (R_SQUARE + 0.022), "yaw": a + 90 * j + 180,
                          "place": c + dd * (R_SQUARE - 0.22)})
        out.append({"id": "t%d" % i, "c": c, "shape": "square", "size": [2 * R_SQUARE, 2 * R_SQUARE], "hero": hero, "yaw": a,
                    "seats": seats, "serve": c + U(a) * (R_SQUARE + 0.45), "a": a})
    return out


def furniture(fur, plan):
    for t in plan:
        table(fur, t["c"], t["shape"], t["yaw"])
        for s in t["seats"]:
            chair(fur, s["at"], s["yaw"])


# ---- lamps ------------------------------------------------------------------------------------------------------------

def ceiling_at(p):
    """The underside over a hall point: a rib's bottom if a rib passes over it."""
    for a in RIB_ANGLES:
        d = U(a)
        s = p.dot(d)
        if s > OC_OUT and abs(p.dot(Vector((-d.y, d.x)))) < RIB_IN_W + 0.08 and s < exit_dist(Vector((0, 0)), d, HALL_PLANES):
            return RIB_IN_BOT
    return CEIL


def lamps(lam, plan):
    lights = []
    for t in plan:                                    # an opal globe over every table on a wire
        c = t["c"]
        top = ceiling_at(c)
        lathe(lam, [(0.06, top - 0.04), (0.06, top + 0.01)], [M("RS_Bronze")], seg=12, c=(c.x, c.y, 0), cap0=True, cap1=False)
        tube(lam, (c.x, c.y, top - 0.04), (c.x, c.y, 2.33), 0.005, M("RS_Bronze"), seg=6, caps=False)
        lathe(lam, [(0.04, 2.31), (0.03, 2.35)], [M("RS_Bronze")], seg=12, c=(c.x, c.y, 0), cap0=False, cap1=True)
        sphere(lam, (c.x, c.y, 2.15), 0.17, M("RS_Opal"), seg=16, rings=8)
        lights.append({"at": [c.x, c.y, 2.15], "lumens": 600, "colour": "#FFE2B8", "kind": "inside", "table": t["id"]})
    for a in (0.0, 90.0, 180.0, 270.0):               # four opal drums on the ceiling
        p = U(a) * 3.0
        lathe(lam, [(0.3, CEIL - 0.09), (0.3, CEIL + 0.01)], [M("RS_Opal")], seg=24, c=(p.x, p.y, 0), cap0=True, cap1=False)
        lights.append({"at": [p.x, p.y, CEIL - 0.25], "lumens": 1500, "colour": "#FFE8C8", "kind": "inside"})
    out_k = 0
    for a in range(-150, 181, 30):                   # downlights in the overhang (not over the kitchen roof)
        if a == 180:
            continue
        p = U(a) * 11.6
        z = soffit_z(11.6)
        lathe(lam, [(0.1, z - 0.03), (0.1, z + 0.03)], [M("RS_Opal")], seg=12, c=(p.x, p.y, 0), cap0=True, cap1=False)
        if a in (-90, -60, -120, 0, 60, 120, 90, -30):
            out_k += 1
            lights.append({"at": [p.x, p.y, z - 0.1], "lumens": 400, "colour": "#FFD8A0", "kind": "outside"})
    for y in (-2.2, 2.2):                             # the kitchen's opal bars
        bx(lam, M("RS_Opal"), -11.6, -10.4, y - 0.08, y + 0.08, K_CEIL - 0.07, K_CEIL + 0.01, faces="xXyYz")
        lights.append({"at": [-11.0, y, K_CEIL - 0.2], "lumens": 1500, "colour": "#F4F1EA", "kind": "kitchen"})
    return lights


# ---- the sign ---------------------------------------------------------------------------------------------------------
LW, LH, LS, GAP = 0.62, 0.9, 0.16, 0.16
SIGN_Y = (-13.90, -13.79)          # the letters' front and back faces
SIGN_Z0 = 4.98
FRAME_Y = (-13.765, -13.705)


def _r(x0, x1, z0, z1):
    return [(x0, z0), (x1, z0), (x1, z1), (x0, z1)]


def letter(ch):
    W, H, s = LW, LH, LS
    if ch == "Я":
        return [_r(W - s, W, 0, H), _r(s, W - s, H - s, H), _r(0, s, 0.45 * H, H), _r(s, W - s, 0.45 * H, 0.45 * H + s),
                [(0, 0), (0.2, 0), (W - s, 0.45 * H), (W - s - 0.2, 0.45 * H)]]
    if ch == "Н":
        return [_r(0, s, 0, H), _r(W - s, W, 0, H), _r(s, W - s, H / 2 - s / 2, H / 2 + s / 2)]
    if ch == "Т":
        return [_r(0, W, H - s, H), _r(W / 2 - s / 2, W / 2 + s / 2, 0, H - s)]
    if ch == "А":
        return [_r(0, s, 0, H - s), _r(W - s, W, 0, H - s), [(0, H - s), (W, H - s), (W - 0.1, H), (0.1, H)],
                _r(s, W - s, 0.38 * H, 0.38 * H + s)]
    if ch == "Р":
        return [_r(0, s, 0, H), _r(s, W - s, H - s, H), _r(W - s, W, 0.45 * H, H), _r(s, W - s, 0.45 * H, 0.45 * H + s)]
    if ch == "Ь":
        return [_r(0, s, 0, H), _r(s, W - s, 0, s), _r(W - s, W, 0, 0.55 * H), _r(s, W - s, 0.55 * H - s, 0.55 * H)]
    raise ValueError(ch)


STEM_X = {"Я": LW - LS / 2, "Н": LS / 2, "Т": LW / 2, "А": LS / 2, "Р": LS / 2, "Ь": LS / 2}


def sign(sg, sh):
    word = "ЯНТАРЬ"
    total = len(word) * LW + (len(word) - 1) * GAP
    x0 = -total / 2
    yf, yb = SIGN_Y
    for i, ch in enumerate(word):
        ox = x0 + i * (LW + GAP)
        for poly in letter(ch):
            pts = [(ox + x, SIGN_Z0 + z) for x, z in poly]
            cx = sum(p[0] for p in pts) / len(pts)
            cz = sum(p[1] for p in pts) / len(pts)
            oq(sg, [(x, yf, z) for x, z in pts], M("RS_SignGlow"), (0, -1, 0))
            oq(sg, [(x, yb, z) for x, z in pts], M("RS_SignGlow"), (0, 1, 0))
            for (ax, az), (bx_, bz) in zip(pts, pts[1:] + pts[:1]):
                nx, nz = bz - az, -(bx_ - ax)
                if nx * ((ax + bx_) / 2 - cx) + nz * ((az + bz) / 2 - cz) < 0:
                    nx, nz = -nx, -nz
                oq(sg, [(ax, yf, az), (bx_, yf, bz), (bx_, yb, bz), (ax, yb, az)], M("RS_SignGlow"), (nx, 0, nz))
        # a standoff from the stem to the frame's middle bar
        sx = ox + STEM_X[ch]
        bx(sh, M("RS_Steel"), sx - 0.015, sx + 0.015, yb, FRAME_Y[0], 5.405, 5.455, faces="xXzZ")
    # the thin steel frame standing on the roof rim
    fx = total / 2 + 0.19
    fy0, fy1 = FRAME_Y
    bx(sh, M("RS_Steel"), -fx, fx, fy0, fy1, RIM_TOP, RIM_TOP + 0.06, faces="xXyYZ")
    bx(sh, M("RS_Steel"), -fx, fx, fy0, fy1, 5.94, 6.0)
    bx(sh, M("RS_Steel"), -fx, fx, fy0, fy1, 5.40, 5.46)
    for x in (-fx + 0.03, 0.0, fx - 0.03):
        for z0, z1 in ((RIM_TOP + 0.06, 5.40), (5.46, 5.94)):
            bx(sh, M("RS_Steel"), x - 0.025, x + 0.025, fy0 + 0.005, fy1 - 0.005, z0, z1, faces="xXyY")
    return [0.0, (yf + yb) / 2, SIGN_Z0 + LH / 2]


# ---- 2D checks ----------------------------------------------------------------------------------------------------------

def _seg_dist(p, a, b):
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.dot(ab), 1e-12)))
    return (a + ab * t - p).length


def _cross(a, b):
    return a.x * b.y - a.y * b.x


def _segs_cross(a, b, c, d):
    d1, d2 = _cross(b - a, c - a), _cross(b - a, d - a)
    d3, d4 = _cross(d - c, a - c), _cross(d - c, b - c)
    return d1 * d2 < 0 and d3 * d4 < 0


def _inside(p, poly):
    s = None
    for a, b in zip(poly, poly[1:] + poly[:1]):
        c = _cross(b - a, p - a)
        if abs(c) < 1e-12:
            continue
        if s is None:
            s = c > 0
        elif (c > 0) != s:
            return False
    return True


def seg_obstacle(a, b, ob):
    """Distance from segment ab to an obstacle ('circle', c, r) or ('poly', [Vector])."""
    if ob[0] == "circle":
        return _seg_dist(ob[1], a, b) - ob[2]
    poly = ob[1]
    if _inside(a, poly) or _inside(b, poly):
        return 0.0
    best = 1e9
    for c, d in zip(poly, poly[1:] + poly[:1]):
        if _segs_cross(a, b, c, d):
            return 0.0
        best = min(best, _seg_dist(c, a, b), _seg_dist(d, a, b), _seg_dist(a, c, d), _seg_dist(b, c, d))
    return best


def polys_overlap(p, q):
    for poly in (p, q):
        for a, b in zip(poly, poly[1:] + poly[:1]):
            ax = Vector((-(b - a).y, (b - a).x))
            pa = [ax.dot(v) for v in p]
            qa = [ax.dot(v) for v in q]
            if max(pa) < min(qa) - 1e-6 or max(qa) < min(pa) - 1e-6:
                return False
    return True


def rect(x0, x1, y0, y1):
    return [Vector((x0, y0)), Vector((x1, y0)), Vector((x1, y1)), Vector((x0, y1))]


def chair_poly(at, yaw):
    r = Matrix.Rotation(math.radians(yaw), 2)
    return [Vector(at) + r @ Vector(p) for p in CHAIR_FOOT]


def table_ob(t):
    if t["shape"] == "round":
        return ("circle", t["c"], R_ROUND + 0.015)
    r = Matrix.Rotation(math.radians(t["yaw"]), 2)
    h = R_SQUARE + 0.007
    return ("poly", [t["c"] + r @ Vector(p) for p in ((-h, -h), (h, -h), (h, h), (-h, h))])


def obstacles(plan):
    obs = []
    for t in plan:
        obs.append((t["id"], table_ob(t)))
        for k, s in enumerate(t["seats"]):
            obs.append(("%s chair %d" % (t["id"], k), ("poly", chair_poly(s["at"], s["yaw"]))))
    obs.append(("planter", ("circle", Vector((0, 0)), PLANTER_R)))
    obs.append(("bar", ("poly", rect(BAR[0] - 0.03, BAR[1] + 0.15, BAR[2] - 0.03, BAR[3] + 0.03))))
    obs.append(("bar back", ("poly", rect(CX_HALL, CX_HALL + 0.4, -5.1, -2.1))))
    obs.append(("pass counter", ("poly", rect(PASS_COUNTER[0], PASS_COUNTER[1], PASS_COUNTER[2], PASS_COUNTER[3]))))
    obs.append(("cloakroom", ("poly", rect(CLOAK[0] - 0.03, CLOAK[1] + 0.03, -9.1, CLOAK[3] + 0.03))))
    return obs


def walk_graph(plan):
    P = Vector((-7.55, 0.0))
    W = Vector((-6.15, -2.9))
    Dn = Vector((0.0, -8.6))
    K = Vector((CX_HALL + 0.85, KDOOR_Y))
    nodes = {"n0": P, "n1": W, "n2": Dn, "n3": K}
    serve_ang = {}
    for t in plan:
        serve_ang[t["id"]] = math.degrees(math.atan2(t["serve"].y, t["serve"].x))
    ring_ang = sorted(set([round(a, 4) for a in serve_ang.values()] + [180.0, -90.0]))
    ring = {}
    for i, a in enumerate(ring_ang):
        name = "n%d" % (4 + i)
        nodes[name] = U(a) * 5.4
        ring[a] = name
    edges = []
    names = [ring[a] for a in ring_ang]
    for a, b in zip(names, names[1:] + names[:1]):
        edges.append((a, b))
    k = 4 + len(ring_ang)
    for t in plan:
        name = "n%d" % k
        k += 1
        nodes[name] = t["serve"]
        t["node"] = name
        edges.append((ring[round(serve_ang[t["id"]], 4)], name))
    edges += [("n0", ring[180.0]), ("n0", "n3"), ("n1", ring[180.0]), ("n1", ring[-150.0]), ("n2", ring[-90.0])]
    # the kitchen door's node to the nearest ring nodes
    for a in sorted(ring_ang, key=lambda a: (U(a) * 5.4 - K).length)[:2]:
        edges.append(("n3", ring[a]))
    return nodes, edges


def check(plan, nodes, edges):
    obs = obstacles(plan)
    fails = []
    # tables vs chairs, chairs vs chairs
    items = []
    for t in plan:
        tob = table_ob(t)
        for s in t["seats"]:
            cp = chair_poly(s["at"], s["yaw"])
            items.append(cp)
            for t2 in plan:
                ob = table_ob(t2)
                if ob[0] == "circle":
                    if _inside(ob[1], cp) or min(_seg_dist(ob[1], a, b) for a, b in zip(cp, cp[1:] + cp[:1])) < ob[2]:
                        fails.append("chair of %s hits table %s" % (t["id"], t2["id"]))
                elif polys_overlap(cp, ob[1]):
                    fails.append("chair of %s hits table %s" % (t["id"], t2["id"]))
        del tob
    for i in range(len(items)):
        for j in range(i + 1, len(items)):
            if polys_overlap(items[i], items[j]):
                fails.append("chairs %d and %d overlap" % (i, j))
    # walls: every node 0.35 inside the hall
    for n, p in nodes.items():
        for u, a in HALL_PLANES:
            if p.dot(u) > a - 0.35:
                fails.append("node %s within 0.35 of the wall" % n)
                break
    worst = 1e9
    for a, b in edges:
        pa, pb = nodes[a], nodes[b]
        for name, ob in obs:
            d = seg_obstacle(pa, pb, ob)
            worst = min(worst, d)
            if d < 0.35:
                fails.append("edge %s-%s passes %.2f m from %s" % (a, b, d, name))
    return fails, worst


# ---- main -------------------------------------------------------------------------------------------------------------

def rnd(v):
    if isinstance(v, float):
        return round(v, 3) + 0.0
    if isinstance(v, Vector):
        return [round(x, 3) + 0.0 for x in v]
    if isinstance(v, (list, tuple)):
        return [rnd(x) for x in v]
    if isinstance(v, dict):
        return {k: rnd(x) for k, x in v.items()}
    return v


def main():
    C.reset()
    sh, ins, gl, fur, lam, sg = (bmesh.new() for _ in range(6))
    drum(sh, gl)
    chord(sh, ins)
    kitchen_block(sh, ins, gl)
    podium(sh)
    roof(sh, ins, gl)
    columns(sh)
    hall_floor(ins)
    pass_and_bar(ins)
    cloakroom(ins)
    kitchen_kit(ins)
    menu = menu_stand(sh)
    sign_at = sign(sg, sh)
    plan = table_plan()
    furniture(fur, plan)
    lights = lamps(lam, plan)
    nodes, edges = walk_graph(plan)
    fails, worst = check(plan, nodes, edges)
    print("WALK %d nodes, %d edges, closest pass %.3f m: %s" % (len(nodes), len(edges), worst, "OK" if not fails else "%d FAILS" % len(fails)), flush=True)
    for f in fails:
        print("WALK FAIL", f, flush=True)
    tris = {}
    tris["SM_Rest_Shell"] = C.export(sh, "SM_Rest_Shell")
    tris["SM_Rest_Inside"] = C.export(ins, "SM_Rest_Inside", smooth=["RS_Bottle", "RS_BottleAmber", "RS_Copper"])
    tris["SM_Rest_Glass"] = C.export(gl, "SM_Rest_Glass")
    tris["SM_Rest_Furniture"] = C.export(fur, "SM_Rest_Furniture", smooth=["RS_Linen", "RS_Bronze", "RS_DarkWood"])
    tris["SM_Rest_Lamps"] = C.export(lam, "SM_Rest_Lamps", smooth=["RS_Opal", "RS_Bronze"])
    tris["SM_Rest_Sign"] = C.export(sg, "SM_Rest_Sign")
    tris["building"] = sum(tris.values())
    print("REST total", tris["building"], "tris", flush=True)
    # ---- layout.json ----------------------------------------------------------------------------------------------------
    tables = []
    for t in plan:
        tables.append({"id": t["id"], "at": [t["c"].x, t["c"].y, TABLE_Z], "shape": t["shape"], "size": t["size"], "hero": t["hero"],
                       "seats": [{"at": [s["at"].x, s["at"].y, 0.0], "yaw": ((s["yaw"] + 180) % 360) - 180, "height": SEAT_Z,
                                  "place": [s["place"].x, s["place"].y, TABLE_Z]} for s in t["seats"]],
                       "serve_at": [t["serve"].x, t["serve"].y, 0.0], "node": t["node"]})
    doors = [
        {"name": "front", "at": [0.0, -R_MID, 0.0], "out": [0, -1], "hole": list(DOOR_HOLE), "wall": WT, "leaves": 1, "hinge": "left",
         "frame": True, "open": False, "locked": False, "outside": True},
        {"name": "kitchen", "at": [CHORD_X, KDOOR_Y, 0.0], "out": [1, 0], "hole": list(DOOR_HOLE), "wall": CT, "leaves": 1, "hinge": "right",
         "frame": True, "open": False, "locked": False, "outside": False},
        {"name": "back", "at": [K_X0 + KT / 2, BACKDOOR_Y, 0.0], "out": [-1, 0], "hole": list(DOOR_HOLE), "wall": KT, "leaves": 1,
         "hinge": "left", "frame": True, "open": False, "locked": False, "outside": True},
    ]
    layout = {
        "_doc": "blender/restaurant/building.py: the restaurant's frame (metres; origin on the finished floor at the drum's centre, "
                "+x the sea, -y the main door, z up; yaw in degrees from +x toward +y). Doors: 'out' is the unit normal out of the "
                "room the door belongs to (front/back: out of the building; kitchen: out of the kitchen into the hall); "
                "place_doors.py swings the leaves that way.",
        "name": "Yantar",
        "prototype": "A 1960s-70s Soviet Baltic seaside restaurant pavilion - a round glazed drum under a thin upturned concrete disc "
                     "on slender columns (the resort cafes of the Latvian and Estonian coast) crossed with Niemeyer's floating discs.",
        "box": {"hall": {"r": 9.85, "z": [0.0, CEIL], "chord_x": CHORD_X}, "kitchen": [KIX0, KIX1, -KIY, KIY, 0.0, K_CEIL]},
        "parts": ["SM_Rest_Shell", "SM_Rest_Inside", "SM_Rest_Glass", "SM_Rest_Furniture", "SM_Rest_Lamps", "SM_Rest_Sign"],
        "doors": doors,
        "lights": lights,
        "tables": tables,
        "pass": {"shelf": [[CX_HALL + 0.025, y, PASS_SHELF] for y in (-1.2, -0.4, 0.4, 1.2)], "waiter": [nodes["n0"].x, nodes["n0"].y, 0.0],
                 "yaw": 180.0, "kitchen_shelf": [[CX_K - 0.025, y, PASS_SHELF] for y in (-1.2, -0.4, 0.4, 1.2)]},
        "cook": {"at": [-9.55, 0.0, 0.0], "yaw": 0.0},
        "waiter_home": {"at": [nodes["n1"].x, nodes["n1"].y, 0.0], "yaw": 0.0},
        "walk": {"nodes": {k: [v.x, v.y] for k, v in nodes.items()}, "edges": [list(e) for e in edges],
                 "named": {"pass": "n0", "waiter_home": "n1", "door_inside": "n2", "kitchen_door": "n3"}},
        "entrance": {"outside": [0.0, DOOR_YS - NSTEP * TREAD - 0.6, None], "inside": [0.0, -8.6, 0.0],
                     "back_outside": [K_X0 - 1.2 - NSTEP * TREAD - 0.6, BACKDOOR_Y, None], "back_inside": [KIX0 + 0.9, BACKDOOR_Y, 0.0]},
        "sign": {"at": sign_at},
        "menu_spot": menu,
        "ground_z": GROUND,
        "tris": tris,
    }
    os.makedirs(os.path.dirname(LAYOUT), exist_ok=True)
    with open(LAYOUT, "w", encoding="utf-8") as f:
        json.dump(rnd(layout), f, indent=1, ensure_ascii=False)
    print("REST layout", LAYOUT, flush=True)
    os.makedirs(os.path.dirname(NOTES), exist_ok=True)
    with open(NOTES, "w", encoding="utf-8") as f:
        f.write(notes(tris, len(nodes), len(edges), fails))
    print("REST A=(%.3f, %.3f) B=(%.3f, %.3f) podium/kitchen x %.3f" % (CX_K, KY, CX_HALL, BY, POD_KX), flush=True)


def notes(tris, nn, ne, fails):
    t = "\n".join("| %s | %d |" % (k, v) for k, v in tris.items())
    return """# Ресторан «Янтарь» (Yantar): the building

Built by `blender/restaurant/building.py` (helpers `restaurant_common.py`, which reuses `blender/diner/diner_common.py`);
the layout the game reads is `unreal/Lyudmilovsk/Content/Restaurant/layout.json`.

## Design

A Soviet-modernist seaside pavilion on the Baltic promenade, 1960s-70s: a round, fully glazed drum under a thin, wide,
slightly upturned concrete disc on slender tapering columns (Niemeyer's floating discs), a solid ochre-tiled kitchen block
behind a straight chord wall, all on a low granite podium with steps to the promenade (-y), the sea (+x, 8 m wide) and
the kitchen yard (-x). The name «ЯНТАРЬ» stands in constructivist block letters on a steel frame on the roof rim over
the main door.

## Sizes (metres, frame: origin on the floor at the drum's centre, +x sea, -y main door)

- Podium r 13.6, top z 0, skirt to -1.2 (ground about -0.6); steps 4 x 0.15 rise, 0.35 tread: door flight 2.4 wide,
  sea flight 8 wide, kitchen yard landing 1.6 x 1.2 then 4 steps.
- Drum: 24-gon, wall mid plane at apothem 10.0 (segment centres at 15 deg * k), 0.16 thick, ceiling 4.0. Each segment is
  two panes wide with a transom (2.5..2.58); a 0.35 concrete sill band, a 0.45 bronze head band (the ribs land on it).
- Chord wall x = -8.4 (0.25), kitchen block x -14.4..-8.4, y +-%.3f (outer; the drum's outer line meets the chord's kitchen
  face there), walls 0.25, roof 3.6, ceiling 3.3, ten + ten small windows high up, amber tile cladding.
- Pass y +-1.7, z 0.95..2.05, stainless shelf top 1.05 (0.45 deep), heat-lamp bar, low counter (0.92) under it in the hall.
- Doors (ALyudDoor holes 1.22 x 2.22): front (0, -10), kitchen (-8.4, 3.6), back (-14.275, -2.5).
- Disc roof r 14: soffit 4.0 inside the glass line, flat to r 10.4 then rising to 4.2 at the edge; top 4.6, edge 4.55
  with a 0.25 raised rim (4.8); oculus ring r 1.0..2.0 glazed at 4.7; 12 ribs inside, 10 under the overhang on 10
  tapering columns at r 12.4 (two of the twelve positions fall inside the kitchen block, which carries the disc there),
  a central column r 0.35 in a planter-bench ring r 1.3 (flat soil at 0.40, seat 0.45).
- Hall: terrazzo floor with a dark ring round the planter and an amber ring at r 5.6..5.75; bar y -5.2..-2.0 (1.05)
  with shelves behind; cloakroom counter by the main door; a menu lectern outside right of the door.
- Furniture: 6 round 4-seat tables (d 1.0, r 7.2 at 0, 45, -45, 90, 135, -120 deg) and 4 square 2-seat tables (0.7, r 4.0
  at +-30, +-150 deg), cloths 0.15 deep, 32 moulded-plywood chairs (seat 0.42 at 0.46). Hero tables: t1, t2, t7, t8.
- Walk graph: %d nodes, %d edges, 0.35 m clearance check: %s.

## Triangles

| mesh | tris |
|---|---|
%s
""" % (KY, nn, ne, "all clear" if not fails else "%d failures" % len(fails), t)


if __name__ == "__main__":
    main()
