"""The diner by the lower station: a 1940 Sterling Streamliner lunch car rebuilt as a clean, enterable game mesh (docs/diner.md).

Prototype: the Modern Diner, Pawtucket, Rhode Island (J. B. Judkins Co., Merrimac MA, 1940; designed by Roland Stickney; the
first diner on the US National Register, 1978): a red-and-cream porcelain-enamel car with stainless bands, shaped like a
streamlined locomotive with a rounded "bullet" end, the door in the middle of its long street side; inside a mahogany-trimmed
counter with stools, booths along the windows and the back bar with the coffee urns and the grill. Ours keeps that type at
game size: 11 stools, six four-seat booths, a curved bench in the nose, a self-service coffee table at the west end, a brick
kitchen annex behind (the staff's door), all doorways to CharacterStandard (clear >= 108 x 210 cm, ALyudDoor in each).

  Blender -b -P blender/diner/building.py           -> design/diner/game/SM_Diner_{Shell,Inside,Glass}.glb, unreal/Lyudmilovsk/Content/Diner/layout.json

Frame (diner_common.py): metres, origin on the floor at the middle of the street front's axis, x along the car (+x the nose),
y across (-y the street front), z up from the floor. tools/world_map.py diner() puts it on the map; the floor stands
FLOOR_OVER_GROUND over the highest ground under the car (terrain.ground_z), the steps reach the real ground.
"""
import json
import math
import os
import sys

import bmesh
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import diner_common as C  # noqa: E402
from diner_common import M, bx, box, tube, lathe, sphere  # noqa: E402

sys.path.insert(0, os.path.join(C.ROOT, "tools"))
try:
    import world_map as W  # noqa: E402
    import terrain as T  # noqa: E402
except Exception as e:  # noqa: BLE001
    W = T = None
    print("DINER no map (%s): flat ground" % e)

LAYOUT = os.path.join(C.ROOT, "unreal", "Lyudmilovsk", "Content", "Diner", "layout.json")     # read by ALyudDiner and people_map.py

# ---- the car --------------------------------------------------------------------------------------------------------------
WD = 5.4                    # outer width
TH = 0.12                   # wall thickness
YF, YB = -WD / 2, WD / 2    # street front, back (outer faces)
XW = -7.3                   # west end (outer face)
XE = 7.4                    # the nose's centre: straight sides end here, a half circle of radius WD/2 beyond
R = WD / 2
NOSE_SEG = 16
EAVE, CROWN = 2.45, 3.2     # outer wall top and roof crown over the floor
EAVE_IN, CROWN_IN = 2.36, 3.02
WALL_Z0 = -0.15             # the car's skirt under the floor line (the brick foundation below it)
SILL, HEAD = 0.95, 2.05     # the window band
FLOOR_OVER_GROUND = 0.7
DOOR_HOLE = (1.22, 2.22)    # CharacterStandard: clear 110 x 215 -> hole 120 x 218 (+2 cm each way)
DOOR_X = 0.0
KITCHEN_DOOR_X = -6.0
# the kitchen annex behind the west half
AX0, AX1, AY1 = -7.3, -2.0, 6.3
ATH = 0.2
A_TOP = 2.38
BACK_DOOR_X = -3.4

# ---- inside ----------------------------------------------------------------------------------------------------------------
YI_F, YI_B = YF + TH, YB - TH          # inner faces of the long walls
XI_W = XW + TH
COUNTER_X = (-4.6, 3.8)
COUNTER_Y = (0.38, 0.95)               # its body; the top overhangs 0.28..1.02
COUNTER_TOP = 1.02
STOOL_Y, STOOL_SEAT = 0.06, 0.76
STOOLS = [-4.0 + 0.72 * i for i in range(11)]
BOOTHS = [(-2.7, -0.9), (-4.5, -2.7), (-6.3, -4.5), (0.9, 2.7), (2.7, 4.5), (4.5, 6.3)]
BOOTH_Y = -1.76                         # the seats' aisle edge
BACKBAR_Y = 1.95                        # its front; the work aisle behind the counter is 0.95..1.95
BACKBAR_X = (-5.0, XE - 0.1)
BACKBAR_TOP = 0.92
GRILL_X = (4.4, 5.8)
URNS_X = (-4.25, -3.65)
MENU = (-1.6, 2.4, 1.42, 2.08)          # the menu board on the back wall: x0, x1, z0, z1
STATION = (-7.18, -6.62, -1.62, -0.42)  # the self-service coffee table against the west wall: x0, x1, y0, y1
STATION_TOP = 0.92
URN_SELF = (-6.94, -0.82)
NOSE_TABLES = [(XE + 1.35, -0.85), (XE + 1.35, 0.85)]


def oq(bm, pts, mi, want):
    """A face with its normal along want."""
    vs = [Vector(p) for p in pts]
    n = (vs[1] - vs[0]).cross(vs[2] - vs[0])
    if n.length < 1e-12 and len(vs) > 3:
        n = (vs[2] - vs[0]).cross(vs[3] - vs[0])
    if n.dot(Vector(want)) < 0:
        vs.reverse()
    f = bm.faces.new([bm.verts.new(v) for v in vs])
    f.material_index = mi
    return f


def ybox(bm, mi, c, size, yaw, faces="all"):
    box(bm, c, size, mi, rot=Matrix.Rotation(yaw, 3, "Z"), faces=faces)


# ---- walls -----------------------------------------------------------------------------------------------------------------

def outline():
    """The car's outer plan, counter-clockwise from the west end of the street front: [(x, y)]."""
    pts = [(XW, YF), (XE, YF)]
    for i in range(1, NOSE_SEG):
        a = -math.pi / 2 + math.pi * i / NOSE_SEG
        pts.append((XE + R * math.cos(a), R * math.sin(a)))
    pts += [(XE, YB), (XW, YB)]
    return pts


def offset_poly(pts, d):
    """The polygon moved inward by d (mitred corners); pts counter-clockwise."""
    n = len(pts)
    out = []
    for i in range(n):
        p0, p1, p2 = Vector(pts[i - 1]), Vector(pts[i]), Vector(pts[(i + 1) % n])
        d0, d1 = (p1 - p0).normalized(), (p2 - p1).normalized()
        n0, n1 = Vector((-d0.y, d0.x)), Vector((-d1.y, d1.x))        # inward normals (left of the walk)
        m = (n0 + n1).normalized()
        out.append(tuple(p1 + m * (d / max(m.dot(n0), 0.2))))
    return out


class Opening:
    def __init__(self, u0, u1, z0, z1, kind="window"):
        self.u0, self.u1, self.z0, self.z1, self.kind = u0, u1, z0, z1, kind


def wall(sh, p0, p1, q0, q1, opens, out_mat, in_mat, z0_out=WALL_Z0, z1_out=EAVE, z0_in=0.0, z1_in=EAVE_IN, reveal=None, glass=None, ubreaks=()):
    """A straight wall: outer face along p0 -> p1 (the outside on its right), inner face along q0 -> q1 (the inner polygon's
    edge). out_mat(u, z) / in_mat(u, z) pick each cell's material; openings cut through both with reveals and a pane."""
    p0, p1 = Vector(p0), Vector(p1)
    L = (p1 - p0).length
    d = (p1 - p0) / L
    n = Vector((d.y, -d.x))                       # outward
    t = (Vector(q0) - p0).dot(-n)                 # thickness
    ui0, ui1 = (Vector(q0) - p0).dot(d), (Vector(q1) - p0).dot(d)
    reveal = M("DN_Steel") if reveal is None else reveal

    def P(u, z, inner):
        q = p0 + d * u - (n * t if inner else Vector((0, 0)))
        return (q.x, q.y, z)

    def cells(inner):
        u_lo, u_hi = (ui0, ui1) if inner else (0.0, L)
        z_lo, z_hi = (z0_in, z1_in) if inner else (z0_out, z1_out)
        us = {u_lo, u_hi} | {min(max(u, u_lo), u_hi) for u in ubreaks}
        zs = {z_lo, z_hi}
        for o in opens:
            us.update((min(max(o.u0, u_lo), u_hi), min(max(o.u1, u_lo), u_hi)))
            zs.update((min(max(o.z0, z_lo), z_hi), min(max(o.z1, z_lo), z_hi)))
        for b in (SILL, HEAD, 0.0, 0.12):
            if z_lo < b < z_hi:
                zs.add(b)
        us, zs = sorted(us), sorted(zs)
        for a, b in zip(us, us[1:]):
            if b - a < 1e-4:
                continue
            for c, e in zip(zs, zs[1:]):
                if e - c < 1e-4:
                    continue
                um, zm = (a + b) / 2, (c + e) / 2
                if any(o.u0 < um < o.u1 and o.z0 < zm < o.z1 for o in opens):
                    continue
                mi = (in_mat if inner else out_mat)(um, zm)
                want = (-n.x, -n.y, 0) if inner else (n.x, n.y, 0)
                oq(sh, [P(a, c, inner), P(b, c, inner), P(b, e, inner), P(a, e, inner)], mi, want)

    cells(False)
    cells(True)
    for o in opens:
        # reveals: sill (unless at the bottom of both faces), head, jambs
        if o.z0 > z0_in + 1e-4 or o.kind == "door":
            oq(sh, [P(o.u0, o.z0, False), P(o.u1, o.z0, False), P(o.u1, o.z0, True), P(o.u0, o.z0, True)], reveal, (0, 0, 1))
        oq(sh, [P(o.u0, o.z1, False), P(o.u1, o.z1, False), P(o.u1, o.z1, True), P(o.u0, o.z1, True)], reveal, (0, 0, -1))
        for u, s in ((o.u0, 1), (o.u1, -1)):
            oq(sh, [P(u, o.z0, False), P(u, o.z0, True), P(u, o.z1, True), P(u, o.z1, False)], reveal, (d.x * s, d.y * s, 0))
        if o.kind == "window" and glass is not None:
            m_ = t / 2
            q = lambda u, z: (p0.x + d.x * u - n.x * m_, p0.y + d.y * u - n.y * m_, z)  # noqa: E731
            oq(glass, [q(o.u0, o.z0), q(o.u1, o.z0), q(o.u1, o.z1), q(o.u0, o.z1)], M("DN_Glass"), (n.x, n.y, 0))
            oq(glass, [q(o.u0, o.z0), q(o.u1, o.z0), q(o.u1, o.z1), q(o.u0, o.z1)], M("DN_Glass"), (-n.x, -n.y, 0))
    return d, n, L


def band_windows(u0, u1, w=0.82, pier=0.14, sill=SILL, head=HEAD):
    """Windows of about w filling u0..u1 with piers between them."""
    k = max(1, int((u1 - u0 + pier) // (w + pier)))
    ww = (u1 - u0 - pier * (k - 1)) / k
    return [Opening(u0 + i * (ww + pier), u0 + i * (ww + pier) + ww, sill, head) for i in range(k)]


def car_out_mat(x_of_u=None, kitchen=None):
    def f(u, z):
        if kitchen and kitchen(u):
            return M("DN_Plaster")
        if z < SILL:
            return M("DN_EnamelRed")
        if z < HEAD:
            return M("DN_Steel")                  # the stainless piers of the window band
        return M("DN_EnamelCream")
    return f


def car_in_mat(u, z):
    if z < SILL:
        return M("DN_WallTile")
    if z < HEAD:
        return M("DN_EnamelCream")
    return M("DN_Mahogany")


def flutes(bm, p0, p1, skip=()):
    """Three stainless flutes along the red lower panel, 8 mm off the face (never on it)."""
    p0, p1 = Vector(p0), Vector(p1)
    L = (p1 - p0).length
    d = (p1 - p0) / L
    n = Vector((d.y, -d.x))
    yaw = math.atan2(d.y, d.x)
    for z in (0.18, 0.5, 0.82):
        segs = [(0.0, L)]
        for a, b in skip:
            segs = [s for seg in segs for s in ((seg[0], min(seg[1], a)), (max(seg[0], b), seg[1])) if s[1] - s[0] > 0.05]
        for a, b in segs:
            c = p0 + d * ((a + b) / 2) + n * 0.014
            ybox(bm, M("DN_Steel"), (c.x, c.y, z), (b - a, 0.012, 0.045), yaw)


def build_shell(found_z, door_ground, back_ground):
    sh = bmesh.new()
    gl = bmesh.new()
    out = outline()
    inn = offset_poly(out, TH)
    n = len(out)
    kitchen_u = None
    for i in range(n):
        p0, p1 = out[i], out[(i + 1) % n]
        q0, q1 = inn[i], inn[(i + 1) % n]
        L = (Vector(p1) - Vector(p0)).length
        opens = []
        kitchen = None
        ubreaks = ()
        if i == 0:                                    # the street front: the door and the windows
            u_door = DOOR_X - XW
            opens.append(Opening(u_door - DOOR_HOLE[0] / 2, u_door + DOOR_HOLE[0] / 2, 0.0, DOOR_HOLE[1], "door"))
            opens += band_windows(0.3, u_door - DOOR_HOLE[0] / 2 - 0.35)
            opens += band_windows(u_door + DOOR_HOLE[0] / 2 + 0.35, L - 0.12)
            flutes(sh, p0, p1, skip=[(u_door - DOOR_HOLE[0] / 2 - 0.02, u_door + DOOR_HOLE[0] / 2 + 0.02)])
        elif 1 <= i <= NOSE_SEG:                      # the nose: a window in every face
            opens.append(Opening(0.12, L - 0.12, SILL, HEAD))
            flutes(sh, p0, p1)
        elif i == NOSE_SEG + 1:                       # the back: the kitchen door, windows over the east half's back bar
            u_k = XE - KITCHEN_DOOR_X
            opens.append(Opening(u_k - DOOR_HOLE[0] / 2, u_k + DOOR_HOLE[0] / 2, 0.0, DOOR_HOLE[1], "door"))
            opens += band_windows(0.25, XE - MENU[1] - 0.35, sill=1.45)
            kitchen_u = (XE - AX1, XE - AX0)
            kitchen = lambda u: kitchen_u[0] - 0.01 < u < kitchen_u[1] + 0.01  # noqa: E731
            flutes(sh, p0, p1, skip=[(kitchen_u[0], kitchen_u[1] + 0.2)])
            ubreaks = kitchen_u
        else:                                         # the west end: one window
            opens += [Opening(1.3, 2.3, SILL, HEAD)]
            flutes(sh, p0, p1)
        wall(sh, p0, p1, q0, q1, opens, car_out_mat(kitchen=kitchen), car_in_mat, glass=gl, ubreaks=ubreaks)
    # ---- the roof: a barrel over the straight part, a half dome over the nose, a gable at the west end --------------------
    prof = [(math.cos(a), math.sin(a)) for a in [math.pi / 2 * k / 8 for k in range(9)]]     # (c, s) quarter
    lip = 0.07

    def roof_pts(r_out, eave, crown):
        return [(r_out * c, eave + (crown - eave) * s) for c, s in prof]      # from the eave (r, eave) to the crown (0, crown)
    rp = roof_pts(R + lip, EAVE, CROWN)
    cp = roof_pts(R - TH, EAVE_IN, CROWN_IN)
    for pts, want_up, mi, x0 in ((rp, 1, M("DN_Roof"), XW - lip), (cp, -1, M("DN_Ceiling"), XI_W)):
        for side in (-1, 1):
            for (r0, z0_), (r1, z1_) in zip(pts, pts[1:]):
                want = (0, side * (r0 + r1) / 2 * want_up, ((z0_ + z1_) / 2 - EAVE + 1.0) * want_up)
                oq(sh, [(x0, side * r0, z0_), (XE, side * r0, z0_), (XE, side * r1, z1_), (x0, side * r1, z1_)], mi, want)
        segs = 24
        for j in range(segs):
            a0 = -math.pi / 2 + math.pi * j / segs
            a1 = -math.pi / 2 + math.pi * (j + 1) / segs
            for (r0, z0_), (r1, z1_) in zip(pts, pts[1:]):
                q = lambda r, a, z: (XE + r * math.cos(a), r * math.sin(a), z)  # noqa: E731
                am = (a0 + a1) / 2
                rm = (r0 + r1) / 2
                want = (math.cos(am) * rm * want_up, math.sin(am) * rm * want_up, ((z0_ + z1_) / 2 - EAVE + 1.0) * want_up)
                fpts = [q(r0, a0, z0_), q(r0, a1, z0_), q(r1, a1, z1_), q(r1, a0, z1_)]
                if r1 < 1e-6:
                    fpts = fpts[:3]
                oq(sh, fpts, mi, want)
    # the lip's underside (from the wall's face out to the lip), all round
    for i in range(n):
        p0, p1 = Vector(out[i]), Vector(out[(i + 1) % n])
        d = (p1 - p0).normalized()
        nn = Vector((d.y, -d.x))
        a, b = p0 + nn * lip, p1 + nn * lip
        if i == n - 1:                                # the west end: its lip runs out to the barrel's lip
            continue
        oq(sh, [(p0.x, p0.y, EAVE), (p1.x, p1.y, EAVE), (b.x, b.y, EAVE), (a.x, a.y, EAVE)], M("DN_Steel"), (0, 0, -1))
    # the west gable: the barrel's end over the wall top, outside and inside, and the lip's end
    garc = roof_pts(R, EAVE, CROWN - lip * 0.6)
    poly = [(XW, -r, z) for r, z in garc] + [(XW, r, z) for r, z in reversed(garc)][1:]
    oq(sh, poly, M("DN_EnamelCream"), (-1, 0, 0))
    gin = [(XI_W, -r, z) for r, z in cp] + [(XI_W, r, z) for r, z in reversed(cp)][1:]
    oq(sh, gin, M("DN_Ceiling"), (1, 0, 0))
    lp = [(XW - lip, -r, z) for r, z in rp] + [(XW - lip, r, z) for r, z in reversed(rp)][1:]
    inner_g = [(XW - lip, -r, z) for r, z in garc] + [(XW - lip, r, z) for r, z in reversed(garc)][1:]
    # the lip ring at the west end: quads between the roof's lip line and the gable's arc
    for a, b, c_, e in zip(lp, lp[1:], inner_g, inner_g[1:]):
        oq(sh, [a, b, e, c_], M("DN_Steel"), (-1, 0, 0))
    for a, c_ in ((lp[0], inner_g[0]), (lp[-1], inner_g[-1])):
        pass
    oq(sh, [(XW - lip, -R - lip, EAVE), (XW, -R - lip, EAVE), (XW, -R, EAVE), (XW - lip, -R, EAVE)], M("DN_Steel"), (0, 0, -1))
    oq(sh, [(XW - lip, R, EAVE), (XW, R, EAVE), (XW, R + lip, EAVE), (XW - lip, R + lip, EAVE)], M("DN_Steel"), (0, 0, -1))
    oq(sh, [(XW - lip, -R, EAVE), (XW, -R, EAVE), (XW, R, EAVE), (XW - lip, R, EAVE)], M("DN_Steel"), (0, 0, -1))
    # ---- the floor: a dark border round a light field --------------------------------------------------------------------
    fo = offset_poly(out, TH)
    fi = offset_poly(out, TH + 0.16)
    oq(sh, [(x, y, 0.0) for x, y in fi], M("DN_FloorTile"), (0, 0, 1))
    for i in range(len(fo)):
        j = (i + 1) % len(fo)
        oq(sh, [(fo[i][0], fo[i][1], 0.0), (fo[j][0], fo[j][1], 0.0), (fi[j][0], fi[j][1], 0.0), (fi[i][0], fi[i][1], 0.0)],
           M("DN_FloorDark"), (0, 0, 1))
    # under the car: the brick foundation set 6 cm in from the skirt, down into the ground
    fd = offset_poly(out, 0.06)
    for i in range(len(fd)):
        j = (i + 1) % len(fd)
        p0, p1 = Vector(fd[i]), Vector(fd[j])
        d = (p1 - p0).normalized()
        oq(sh, [(p0.x, p0.y, found_z), (p1.x, p1.y, found_z), (p1.x, p1.y, WALL_Z0), (p0.x, p0.y, WALL_Z0)], M("DN_Brick"), (d.y, -d.x, 0))
        po, qo = Vector(out[i]), Vector(out[j])
        oq(sh, [(p0.x, p0.y, WALL_Z0), (p1.x, p1.y, WALL_Z0), (qo.x, qo.y, WALL_Z0), (po.x, po.y, WALL_Z0)], M("DN_Dark"), (0, 0, -1))
    # ---- the kitchen annex ------------------------------------------------------------------------------------------------
    annex(sh, gl, found_z)
    # ---- the steps: a landing at the floor and steps down to the ground, at the front and the back doors ------------------
    steps(sh, DOOR_X, YF, -1, door_ground)
    steps(sh, BACK_DOOR_X, AY1, 1, back_ground)
    return sh, gl


def annex(sh, gl, found_z):
    """A brick kitchen behind the west half of the car (flat roof under the car's eave), its outer walls x AX0..AX1, y YB..AY1."""
    # outer outline (counter-clockwise), the car's back wall closes it
    ring = [(AX1, YB), (AX1, AY1), (AX0, AY1), (AX0, YB)]
    inner = [(AX1 - ATH, YB), (AX1 - ATH, AY1 - ATH), (AX0 + ATH, AY1 - ATH), (AX0 + ATH, YB)]
    brick = lambda u, z: M("DN_Brick")  # noqa: E731
    plaster = lambda u, z: M("DN_Plaster") if z > 1.2 else M("DN_WallTile")  # noqa: E731
    for i in range(3):
        p0, p1, q0, q1 = ring[i], ring[i + 1], inner[i], inner[i + 1]
        L = (Vector(p1) - Vector(p0)).length
        opens = []
        if i == 1:                                   # the back: the staff door
            u = AX1 - BACK_DOOR_X
            opens.append(Opening(u - DOOR_HOLE[0] / 2, u + DOOR_HOLE[0] / 2, 0.0, DOOR_HOLE[1], "door"))
            opens.append(Opening(L - 1.6, L - 0.6, 1.2, 1.9))
        if i == 2:
            opens.append(Opening(0.9, 1.9, 1.2, 1.9))
        wall(sh, p0, p1, q0, q1, opens, brick, plaster, z0_out=found_z, z1_out=A_TOP, z0_in=0.0, z1_in=A_TOP - 0.12,
             reveal=M("DN_Concrete"), glass=gl)
    # roof (top, a concrete coping lip) and ceiling
    oq(sh, [(AX0 - 0.05, YB, A_TOP), (AX1 + 0.05, YB, A_TOP), (AX1 + 0.05, AY1 + 0.05, A_TOP), (AX0 - 0.05, AY1 + 0.05, A_TOP)], M("DN_Roof"), (0, 0, 1))
    for a, b, want in (((AX0 - 0.05, AY1 + 0.05), (AX1 + 0.05, AY1 + 0.05), (0, 1, 0)), ((AX1 + 0.05, YB), (AX1 + 0.05, AY1 + 0.05), (1, 0, 0)),
                       ((AX0 - 0.05, YB), (AX0 - 0.05, AY1 + 0.05), (-1, 0, 0))):
        oq(sh, [(a[0], a[1], A_TOP - 0.1), (b[0], b[1], A_TOP - 0.1), (b[0], b[1], A_TOP), (a[0], a[1], A_TOP)], M("DN_Concrete"), want)
    # the coping's underside outside the walls
    for poly in (((AX0 - 0.05, AY1), (AX1 + 0.05, AY1), (AX1 + 0.05, AY1 + 0.05), (AX0 - 0.05, AY1 + 0.05)),
                 ((AX1, YB), (AX1 + 0.05, YB), (AX1 + 0.05, AY1), (AX1, AY1)), ((AX0 - 0.05, YB), (AX0, YB), (AX0, AY1), (AX0 - 0.05, AY1))):
        oq(sh, [(x, y, A_TOP - 0.1) for x, y in poly], M("DN_Concrete"), (0, 0, -1))
    ci = A_TOP - 0.12
    oq(sh, [(AX0 + ATH, YB, ci), (AX1 - ATH, YB, ci), (AX1 - ATH, AY1 - ATH, ci), (AX0 + ATH, AY1 - ATH, ci)], M("DN_Ceiling"), (0, 0, -1))
    oq(sh, [(AX0 + ATH, YB, 0.0), (AX1 - ATH, YB, 0.0), (AX1 - ATH, AY1 - ATH, 0.0), (AX0 + ATH, AY1 - ATH, 0.0)], M("DN_KitchenTile"), (0, 0, 1))


def steps(sh, x, y_wall, s, ground):
    """A landing 1.1 m deep in front of a door at x on the wall y_wall (s: outward sign of y), then steps down to ground (z)."""
    rise = max(0.0, -ground)
    k = max(1, int(math.ceil(rise / 0.19 - 1e-6))) if rise > 0.05 else 0
    hw = 0.95
    y0 = y_wall
    y1 = y_wall + s * 1.1
    bot = ground - 0.3
    # the landing: its top a hair under the threshold so the door's reveal and it never share a plane
    for poly, want in (([(x - hw, y0, -0.004), (x + hw, y0, -0.004), (x + hw, y1, -0.004), (x - hw, y1, -0.004)], (0, 0, 1)),):
        oq(sh, poly, M("DN_Concrete"), want)
    for xs, wx in ((x - hw, -1), (x + hw, 1)):
        oq(sh, [(xs, y0, bot), (xs, y1, bot), (xs, y1, -0.004), (xs, y0, -0.004)], M("DN_Concrete"), (wx, 0, 0))
    yy = y1
    for i in range(k):
        h = rise / k
        ztop = -h * (i + 1) - 0.004
        zprev = -h * i - 0.004
        yn = yy + s * 0.3
        oq(sh, [(x - hw, yy, ztop), (x + hw, yy, ztop), (x + hw, yy, zprev), (x - hw, yy, zprev)], M("DN_Concrete"), (0, s, 0))  # riser
        oq(sh, [(x - hw, yy, ztop), (x + hw, yy, ztop), (x + hw, yn, ztop), (x - hw, yn, ztop)], M("DN_Concrete"), (0, 0, 1))
        for xs, wx in ((x - hw, -1), (x + hw, 1)):
            oq(sh, [(xs, yy, bot), (xs, yn, bot), (xs, yn, ztop), (xs, yy, ztop)], M("DN_Concrete"), (wx, 0, 0))
        yy = yn
    oq(sh, [(x - hw, yy, bot), (x + hw, yy, bot), (x + hw, yy, -0.004 - rise), (x - hw, yy, -0.004 - rise)], M("DN_Concrete"), (0, s, 0))
    # chrome hand rails on both sides, along the steps
    if k:
        for xs in (x - hw + 0.06, x + hw - 0.06):
            top0 = (xs, y0 + s * 0.05, 0.9)
            top1 = (xs, y1, 0.9)
            top2 = (xs, yy - s * 0.1, 0.9 - rise)
            tube(sh, top0, top1, 0.022, M("DN_Chrome"), seg=8)
            tube(sh, top1, top2, 0.022, M("DN_Chrome"), seg=8)
            for p, zb in ((top0, -0.004), (top1, -0.004), (top2, -rise - 0.004)):
                tube(sh, (p[0], p[1], zb), (p[0], p[1], p[2] - 0.02), 0.018, M("DN_Chrome"), seg=8, caps=False)
    return k


# ---- inside --------------------------------------------------------------------------------------------------------------

def stool(bm, x, y):
    lathe(bm, [(0.17, 0.0), (0.17, 0.02), (0.06, 0.04)], M("DN_Chrome"), seg=16, c=(x, y, 0.0), cap0=False)
    tube(bm, (x, y, 0.04), (x, y, STOOL_SEAT - 0.06), 0.035, M("DN_Chrome"), seg=10, caps=False)
    lathe(bm, [(0.05, STOOL_SEAT - 0.07), (0.19, STOOL_SEAT - 0.05), (0.195, STOOL_SEAT - 0.02)], M("DN_Chrome"), seg=20, c=(x, y, 0.0), cap0=False, cap1=False)
    lathe(bm, [(0.195, STOOL_SEAT - 0.02), (0.19, STOOL_SEAT + 0.01), (0.15, STOOL_SEAT + 0.035), (0.0, STOOL_SEAT + 0.045)],
          M("DN_Vinyl"), seg=20, c=(x, y, 0.0), cap0=False)


def counter(bm):
    x0, x1 = COUNTER_X
    y0, y1 = COUNTER_Y
    # the body: a recessed dark kick, the stainless front, a mahogany rail under the top
    bx(bm, M("DN_Dark"), x0 + 0.05, x1 - 0.05, y0 + 0.06, y1, 0.0, 0.12, faces="xXyYZ")
    bx(bm, M("DN_Steel"), x0, x1, y0, y1, 0.12, 0.86, faces="xXyYz")
    bx(bm, M("DN_Mahogany"), x0, x1, y0 - 0.03, y1, 0.86, COUNTER_TOP - 0.04, faces="xXyYzZ")
    bx(bm, M("DN_Formica"), x0 - 0.04, x1 + 0.04, 0.28, 1.02, COUNTER_TOP - 0.04, COUNTER_TOP, faces="Z")
    bx(bm, M("DN_Chrome"), x0 - 0.04, x1 + 0.04, 0.28, 1.02, COUNTER_TOP - 0.04, COUNTER_TOP, faces="xXyYz")
    # the foot rail on posts
    tube(bm, (x0 + 0.1, 0.24, 0.2), (x1 - 0.1, 0.24, 0.2), 0.022, M("DN_Chrome"), seg=8)
    for xp in (x0 + 0.25, (x0 + x1) / 2, x1 - 0.25):
        tube(bm, (xp, 0.24, 0.18), (xp, y0 + 0.005, 0.18), 0.016, M("DN_Chrome"), seg=6, caps=False)
    # the staff side: open shelves under the top
    bx(bm, M("DN_Steel"), x0, x1, y1, y1 + 0.02, 0.12, 0.86, faces="Y")
    for z in (0.12, 0.5):
        bx(bm, M("DN_Steel"), x0 + 0.02, x1 - 0.02, y1 + 0.02, 1.0, z, z + 0.02, faces="Zz")
    # napkin holders, sugar pourers, salt and pepper every two stools; a cash register at the east end
    for i, x in enumerate(STOOLS[::2]):
        xx = x + 0.36
        if xx > x1 - 0.4:
            break
        bx(bm, M("DN_Steel"), xx - 0.06, xx + 0.06, 0.62, 0.68, COUNTER_TOP, COUNTER_TOP + 0.1, faces="xXyYZ")
        lathe(bm, [(0.03, COUNTER_TOP), (0.032, COUNTER_TOP + 0.09), (0.02, COUNTER_TOP + 0.11)], M("DN_Glass"), seg=10, c=(xx + 0.12, 0.65, 0), cap0=False)
        lathe(bm, [(0.02, COUNTER_TOP + 0.11), (0.012, COUNTER_TOP + 0.14), (0.0, COUNTER_TOP + 0.145)], M("DN_Chrome"), seg=10, c=(xx + 0.12, 0.65, 0), cap0=False)
        for k_, dx in enumerate((-0.15, -0.11)):
            lathe(bm, [(0.017, COUNTER_TOP), (0.017, COUNTER_TOP + 0.07), (0.0, COUNTER_TOP + 0.085)], M("DN_Glass") if k_ == 0 else M("DN_Glass"),
                  seg=8, c=(xx + dx, 0.65, 0), cap0=False)
    rx = x1 - 0.32
    bx(bm, M("DN_Dark"), rx - 0.2, rx + 0.2, 0.6, 0.98, COUNTER_TOP, COUNTER_TOP + 0.12, faces="xXyYZ")
    ybox(bm, M("DN_Chrome"), (rx, 0.72, COUNTER_TOP + 0.2), (0.38, 0.22, 0.14), 0.0)
    ybox(bm, M("DN_Dark"), (rx, 0.9, COUNTER_TOP + 0.3), (0.3, 0.06, 0.12), 0.0)


def booth(bm, xa, xb):
    """Two benches facing over a table, the backs at xa and xb, against the street front's inner face."""
    y0, y1 = YI_F, BOOTH_Y
    for xs, s in ((xa, 1), (xb, -1)):
        xback = xs + s * 0.09
        seat_out = xs + s * 0.55
        lo, hi = sorted((xs, xback))
        bx(bm, M("DN_Mahogany"), lo, hi, y0, y1, 0.0, 1.12, faces="xXYZ")                 # the back
        lo, hi = sorted((xback, seat_out))
        bx(bm, M("DN_Mahogany"), lo, hi, y0, y1 - 0.04, 0.0, 0.38, faces="xXY" if s > 0 else "xXY")
        bx(bm, M("DN_Vinyl"), lo, hi, y0, y1, 0.38, 0.46, faces="xXYZ")                    # the seat cushion
        # the back cushion facing the table
        bc0, bc1 = (xback, xback + s * 0.07)
        lo, hi = sorted((bc0, bc1))
        bx(bm, M("DN_Vinyl"), lo, hi, y0, y1 - 0.03, 0.5, 1.05, faces=("X" if s > 0 else "x") + "YZz")
    # the table: Formica top with a chrome edge on a pedestal, its long side at the wall
    tx0, tx1 = xa + 0.62, xb - 0.62
    bx(bm, M("DN_Formica"), tx0, tx1, y0, y1 - 0.08, 0.735, 0.765, faces="Z")
    bx(bm, M("DN_Chrome"), tx0, tx1, y0, y1 - 0.08, 0.735, 0.765, faces="xXYz")
    tube(bm, ((tx0 + tx1) / 2, (y0 + y1) / 2, 0.0), ((tx0 + tx1) / 2, (y0 + y1) / 2, 0.735), 0.04, M("DN_Chrome"), seg=10, caps=False)
    lathe(bm, [(0.22, 0.0), (0.2, 0.025), (0.04, 0.04)], M("DN_Chrome"), seg=16, c=((tx0 + tx1) / 2, (y0 + y1) / 2, 0.0), cap0=False)
    # a jukebox selector on the wall over the table, between the windows' sills
    cx = (tx0 + tx1) / 2
    bx(bm, M("DN_Chrome"), cx - 0.11, cx + 0.11, y0, y0 + 0.09, 0.8, 0.93, faces="xXYZz")


def nose_bench(bm):
    """A curved bench along the nose's inner wall (-60..60 degrees) and two round tables."""
    r_wall = R - TH
    for j in range(NOSE_SEG):
        a0 = -math.pi / 2 + math.pi * j / NOSE_SEG
        a1 = -math.pi / 2 + math.pi * (j + 1) / NOSE_SEG
        am = (a0 + a1) / 2
        if abs(am) > math.radians(62):
            continue
        for r0, r1, z0, z1, mi, keep in ((r_wall - 0.55, r_wall, 0.38, 0.46, M("DN_Vinyl"), "top"),
                                         (r_wall - 0.52, r_wall, 0.0, 0.38, M("DN_Mahogany"), "front"),
                                         (r_wall - 0.09, r_wall, 0.46, 1.1, M("DN_Mahogany"), "back")):
            q = lambda r, a, z: (XE + r * math.cos(a), r * math.sin(a), z)  # noqa: E731
            if keep == "top":
                oq(bm, [q(r0, a0, z1), q(r0, a1, z1), q(r1, a1, z1), q(r1, a0, z1)], mi, (0, 0, 1))
                oq(bm, [q(r0, a0, z0), q(r0, a1, z0), q(r0, a1, z1), q(r0, a0, z1)], mi, (-math.cos(am), -math.sin(am), 0))
            elif keep == "front":
                oq(bm, [q(r0, a0, z0), q(r0, a1, z0), q(r0, a1, z1), q(r0, a0, z1)], mi, (-math.cos(am), -math.sin(am), 0))
            else:
                oq(bm, [q(r0, a0, z0), q(r0, a1, z0), q(r0, a1, z1), q(r0, a0, z1)], mi, (-math.cos(am), -math.sin(am), 0))
                oq(bm, [q(r0, a0, z1), q(r0, a1, z1), q(r1, a1, z1), q(r1, a0, z1)], mi, (0, 0, 1))
        # the bench's open ends
    for am in (math.radians(-60), math.radians(60)):
        for r0, r1, z0, z1, mi in ((r_wall - 0.55, r_wall, 0.0, 0.46, M("DN_Mahogany")), (r_wall - 0.09, r_wall, 0.46, 1.1, M("DN_Mahogany"))):
            s = -1 if am < 0 else 1
            t = Vector((-math.sin(am), math.cos(am), 0)) * s
            a = Vector((XE + r0 * math.cos(am), r0 * math.sin(am), 0))
            b = Vector((XE + r1 * math.cos(am), r1 * math.sin(am), 0))
            oq(bm, [(a.x, a.y, z0), (b.x, b.y, z0), (b.x, b.y, z1), (a.x, a.y, z1)], mi, tuple(t))
    for x, y in NOSE_TABLES:
        lathe(bm, [(0.0, 0.765), (0.34, 0.765)], M("DN_Formica"), seg=24, c=(x, y, 0), cap0=False, cap1=False)
        lathe(bm, [(0.34, 0.765), (0.345, 0.75), (0.34, 0.735), (0.0, 0.735)], M("DN_Chrome"), seg=24, c=(x, y, 0), cap0=False, cap1=False)
        tube(bm, (x, y, 0.0), (x, y, 0.735), 0.04, M("DN_Chrome"), seg=10, caps=False)
        lathe(bm, [(0.22, 0.0), (0.2, 0.025), (0.04, 0.04)], M("DN_Chrome"), seg=16, c=(x, y, 0.0), cap0=False)


def back_bar(bm):
    x0, x1 = BACKBAR_X
    y0, y1 = BACKBAR_Y, YI_B
    bx(bm, M("DN_Dark"), x0, x1, y0 + 0.06, y1, 0.0, 0.1, faces="xXyZ")
    bx(bm, M("DN_Steel"), x0, x1, y0, y1, 0.1, BACKBAR_TOP - 0.03, faces="xXyz")
    bx(bm, M("DN_Steel"), x0 - 0.02, x1 + 0.02, y0 - 0.03, y1, BACKBAR_TOP - 0.03, BACKBAR_TOP, faces="xXyzZ")
    # cupboard doors (raised 6 mm off the front)
    n = int((x1 - x0) // 0.6)
    w = (x1 - x0) / n
    for i in range(n):
        a = x0 + i * w + 0.02
        bx(bm, M("DN_Steel"), a, a + w - 0.04, y0 - 0.016, y0, 0.14, BACKBAR_TOP - 0.08, faces="yxXzZ")
        tube(bm, (a + w / 2 - 0.08, y0 - 0.03, BACKBAR_TOP - 0.15), (a + w / 2 + 0.08, y0 - 0.03, BACKBAR_TOP - 0.15), 0.008, M("DN_Chrome"), seg=6)
    # the stainless backsplash up to the menu board and the window sills, with a mahogany shelf
    bx(bm, M("DN_Steel"), x0, x1, y1 - 0.015, y1, BACKBAR_TOP, 1.4, faces="y")
    bx(bm, M("DN_Mahogany"), x0, x1, y1 - 0.2, y1 - 0.015, 1.4, 1.43, faces="xXyzZ")
    # the grill with its hood
    gx0, gx1 = GRILL_X
    bx(bm, M("DN_Dark"), gx0, gx1, y0 + 0.02, y1 - 0.1, BACKBAR_TOP, BACKBAR_TOP + 0.03, faces="xXyYZ")
    bx(bm, M("DN_Steel"), gx0 - 0.1, gx1 + 0.1, y1 - 0.55, y1 - 0.015, 1.95, 2.25, faces="xXyzZ")
    # the urns: two tall stainless urns with brass-less chrome taps facing the counter
    for x in URNS_X:
        urn(bm, x, y1 - 0.3, BACKBAR_TOP, -1)
    # the pie case at the back bar's west end
    px = -2.9
    bx(bm, M("DN_Chrome"), px - 0.35, px + 0.35, y1 - 0.45, y1 - 0.1, BACKBAR_TOP, BACKBAR_TOP + 0.03, faces="xXyYZ")
    bx(bm, M("DN_Glass"), px - 0.34, px + 0.34, y1 - 0.44, y1 - 0.11, BACKBAR_TOP + 0.03, BACKBAR_TOP + 0.4, faces="xXyYZ")
    for z in (BACKBAR_TOP + 0.03, BACKBAR_TOP + 0.21):
        for k_, dx in enumerate((-0.17, 0.17)):
            lathe(bm, [(0.11, z + 0.002), (0.11, z + 0.02), (0.1, z + 0.08), (0.0, z + 0.085)], M("DN_Crust"), seg=16,
                  c=(px + dx, y1 - 0.27, 0), cap0=False)
    # mugs upside down on the shelf (the staff's)
    for i in range(6):
        lathe(bm, [(0.042, 1.43), (0.042, 1.525), (0.0, 1.527)], M("DN_China"), seg=12, c=(x0 + 0.35 + i * 0.12, y1 - 0.1, 0), cap0=False)


def urn(bm, x, y, z, face):
    """A stainless coffee urn standing at z on three short legs, its tap facing face (+1 +y, -1 -y; 2 +x)."""
    for k in range(3):
        a = 2 * math.pi * k / 3 + 0.5
        tube(bm, (x + 0.12 * math.cos(a), y + 0.12 * math.sin(a), z), (x + 0.12 * math.cos(a), y + 0.12 * math.sin(a), z + 0.12), 0.012, M("DN_Chrome"), seg=6, caps=False)
    lathe(bm, [(0.0, z + 0.12), (0.155, z + 0.12), (0.16, z + 0.16), (0.16, z + 0.58), (0.15, z + 0.62), (0.09, z + 0.66), (0.0, z + 0.67)],
          M("DN_Steel"), seg=24, c=(x, y, 0), cap0=False)
    sphere(bm, (x, y, z + 0.69), 0.03, M("DN_Dark"), seg=10, rings=5)
    # a glass gauge tube up its side
    gdir = Vector((0.0, face * 1.0, 0.0)) if face in (-1, 1) else Vector((1.0, 0.0, 0.0))
    side = Vector((gdir.y, -gdir.x, 0)) * 0.1
    g = Vector((x, y, 0)) + gdir * 0.162 + side
    tube(bm, (g.x, g.y, z + 0.2), (g.x, g.y, z + 0.55), 0.008, M("DN_Glass"), seg=6)
    # the tap: a spout out of the body, the lever on top
    t0 = Vector((x, y, 0)) + gdir * 0.16
    t1 = Vector((x, y, 0)) + gdir * 0.25
    tube(bm, (t0.x, t0.y, z + 0.24), (t1.x, t1.y, z + 0.24), 0.014, M("DN_Chrome"), seg=8)
    tube(bm, (t1.x, t1.y, z + 0.24), (t1.x, t1.y, z + 0.2), 0.012, M("DN_Chrome"), seg=8)
    ybox(bm, M("DN_Dark"), (t1.x, t1.y, z + 0.29), (0.025, 0.025, 0.08), 0.0)
    return (t1.x, t1.y, z + 0.2)


def coffee_station(bm):
    x0, x1, y0, y1 = STATION
    bx(bm, M("DN_Mahogany"), x0, x1, y0, y1, 0.0, STATION_TOP - 0.03, faces="XyYZ")
    bx(bm, M("DN_Formica"), x0, x1 + 0.02, y0 - 0.02, y1 + 0.02, STATION_TOP - 0.03, STATION_TOP, faces="Z")
    bx(bm, M("DN_Chrome"), x0, x1 + 0.02, y0 - 0.02, y1 + 0.02, STATION_TOP - 0.03, STATION_TOP, faces="XyYz")
    ux, uy = URN_SELF
    tap = urn(bm, ux, uy, STATION_TOP, 2)
    # the drip tray under the tap
    bx(bm, M("DN_Chrome"), tap[0] - 0.07, tap[0] + 0.07, tap[1] - 0.08, tap[1] + 0.08, STATION_TOP, STATION_TOP + 0.015, faces="xXyYZ")
    # a tray of clean mugs upside down, a sugar pourer and a creamer
    mx, my = -6.85, -1.33
    bx(bm, M("DN_Steel"), mx - 0.16, mx + 0.16, my - 0.2, my + 0.2, STATION_TOP, STATION_TOP + 0.012, faces="xXyYZ")
    for i in range(2):
        for j in range(3):
            lathe(bm, [(0.042, STATION_TOP + 0.012), (0.042, STATION_TOP + 0.107), (0.0, STATION_TOP + 0.109)], M("DN_China"), seg=12,
                  c=(mx - 0.07 + i * 0.14, my - 0.12 + j * 0.12, 0), cap0=False)
    lathe(bm, [(0.03, STATION_TOP), (0.032, STATION_TOP + 0.09), (0.02, STATION_TOP + 0.11)], M("DN_Glass"), seg=10, c=(-6.95, -0.5, 0), cap0=False)
    lathe(bm, [(0.02, STATION_TOP + 0.11), (0.012, STATION_TOP + 0.14), (0.0, STATION_TOP + 0.145)], M("DN_Chrome"), seg=10, c=(-6.95, -0.5, 0), cap0=False)
    return {"tap": [round(v, 4) for v in tap], "mug_under_tap": [round(tap[0], 4), round(tap[1], 4), STATION_TOP + 0.015],
            "mugs": [mx, my, STATION_TOP + 0.012], "stand": [x1 + 0.45, (y0 + y1) / 2, 0.0]}


def kitchen(bm):
    """Stove, sink, worktable, shelves and a fridge in the annex, clear of both doors' swing zones."""
    xi0, xi1, yi0, yi1 = AX0 + ATH, AX1 - ATH, YB, AY1 - ATH
    # the range against the east wall, a hood over it
    bx(bm, M("DN_Dark"), xi1 - 0.7, xi1, 3.0, 4.5, 0.0, 0.9, faces="xyYZ")
    for i in range(2):
        for j in range(2):
            lathe(bm, [(0.11, 0.9), (0.11, 0.92), (0.0, 0.92)], M("DN_Steel"), seg=12, c=(xi1 - 0.5 + i * 0.3, 3.4 + j * 0.65, 0), cap0=False)
    bx(bm, M("DN_Steel"), xi1 - 0.75, xi1, 2.95, 4.55, 1.85, 2.1, faces="xyYz")
    # the sink and the fridge along the back wall, west of the back door's clear zone
    bx(bm, M("DN_Steel"), xi0, xi0 + 1.2, yi1 - 0.6, yi1, 0.0, 0.9, faces="XyZ")
    bx(bm, M("DN_Dark"), xi0 + 0.35, xi0 + 0.85, yi1 - 0.5, yi1 - 0.1, 0.9, 0.92, faces="xXyYZ")
    bx(bm, M("DN_EnamelCream"), xi0 + 1.3, xi0 + 2.0, yi1 - 0.7, yi1, 0.0, 1.8, faces="xXyZ")
    tube(bm, (xi0 + 1.38, yi1 - 0.73, 1.0), (xi0 + 1.38, yi1 - 0.73, 1.5), 0.012, M("DN_Chrome"), seg=6)
    # shelves on the west wall
    for z in (1.0, 1.4, 1.8):
        bx(bm, M("DN_Steel"), xi0, xi0 + 0.35, 3.9, 5.3, z, z + 0.02, faces="XyYzZ")
    for k in range(5):
        lathe(bm, [(0.09, 1.02), (0.09, 1.2), (0.0, 1.2)], M("DN_Steel"), seg=12, c=(xi0 + 0.18, 4.05 + k * 0.28, 0), cap0=False)
    # a worktable in the middle
    bx(bm, M("DN_Steel"), -5.0, -3.9, 4.0, 4.7, 0.86, 0.9, faces="xXyYzZ")
    for x in (-4.95, -3.95):
        for y in (4.05, 4.65):
            tube(bm, (x, y, 0.0), (x, y, 0.86), 0.02, M("DN_Steel"), seg=6, caps=False)


def lamps(bm):
    """Opal globes on chrome stems along the crown, one in the nose, two in the kitchen, one over the door outside."""
    out = []
    for x in (-6.0, -3.6, -1.2, 1.2, 3.6, 6.0, XE + 1.2):
        top = CROWN_IN - 0.01 if x <= XE else EAVE_IN + (CROWN_IN - EAVE_IN) * math.sqrt(max(0.0, 1 - ((x - XE) / (R - TH)) ** 2)) - 0.01
        tube(bm, (x, 0, top), (x, 0, top - 0.25), 0.01, M("DN_Chrome"), seg=6, caps=False)
        lathe(bm, [(0.0, top - 0.24), (0.06, top - 0.25), (0.07, top - 0.28), (0.0, top - 0.285)], M("DN_Chrome"), seg=14, c=(x, 0, 0), cap0=False)
        sphere(bm, (x, 0, top - 0.4), 0.13, M("DN_Opal"), seg=16, rings=8)
        out.append({"at": [x, 0.0, top - 0.4], "lumens": 900, "colour": "#FFE2B8", "kind": "inside"})
    for x, y in ((-5.8, 3.9), (-3.5, 4.9)):
        z = A_TOP - 0.12
        bx(bm, M("DN_Opal"), x - 0.5, x + 0.5, y - 0.1, y + 0.1, z - 0.06, z, faces="xXyYz")
        out.append({"at": [x, y, z - 0.2], "lumens": 1100, "colour": "#F4F1EA", "kind": "kitchen"})
    # the gooseneck lamp over the front door (outside)
    tube(bm, (DOOR_X, YF + 0.0, 2.32), (DOOR_X, YF - 0.3, 2.42), 0.018, M("DN_Chrome"), seg=6, caps=False)
    lathe(bm, [(0.0, 2.44), (0.12, 2.36), (0.13, 2.33), (0.0, 2.34)], M("DN_EnamelRed"), seg=16, c=(DOOR_X, YF - 0.42, 0), cap0=False, cap1=False)
    sphere(bm, (DOOR_X, YF - 0.42, 2.335), 0.05, M("DN_Opal"), seg=10, rings=5)
    out.append({"at": [DOOR_X, YF - 0.42, 2.25], "lumens": 600, "colour": "#FFD8A0", "kind": "outside"})
    return out


def signs(sh):
    """Picture quads (DN_Sign_<name>, their own slot) 6 mm off their faces; the roof sign on a stainless frame."""
    quads = []
    # the name on the cream band over the west windows, the hours over the east ones
    for name, x0, x1 in (("name", -6.7, -0.95), ("band", 0.95, 6.9)):
        quads.append((name, [(x0, YF - 0.016, HEAD + 0.03), (x1, YF - 0.016, HEAD + 0.03), (x1, YF - 0.016, EAVE - 0.04), (x0, YF - 0.016, EAVE - 0.04)], (0, -1, 0)))
    # the menu board inside on the back wall, framed in mahogany
    x0, x1, z0, z1 = MENU
    quads.append(("menu", [(x0, YI_B - 0.046, z0), (x1, YI_B - 0.046, z0), (x1, YI_B - 0.046, z1), (x0, YI_B - 0.046, z1)], (0, -1, 0)))
    # the self-service card over the coffee table (west wall, inside)
    quads.append(("coffee", [(XI_W + 0.016, -0.62, 1.2), (XI_W + 0.016, -1.42, 1.2), (XI_W + 0.016, -1.42, 1.6), (XI_W + 0.016, -0.62, 1.6)], (1, 0, 0)))
    # the roof sign: a double-sided panel on the crown
    rz0, rz1, rx0, rx1 = CROWN + 0.25, CROWN + 1.0, -2.4, 2.4
    for side in (-1, 1):
        y = side * 0.046
        quads.append(("roof", [(rx0, y, rz0), (rx1, y, rz0), (rx1, y, rz1), (rx0, y, rz1)] if side < 0 else
                      [(rx1, y, rz0), (rx0, y, rz0), (rx0, y, rz1), (rx1, y, rz1)], (0, side, 0)))
    extra = sorted({"DN_Sign_" + q[0] for q in quads})
    base = len(C.MATS)
    for name, pts, want in quads:
        oq(sh, pts, base + extra.index("DN_Sign_" + name), want)
    # frames: mahogany round the menu, stainless round the roof panel and its legs
    bx(sh, M("DN_Mahogany"), x0 - 0.06, x1 + 0.06, YI_B - 0.03, YI_B, z0 - 0.06, z1 + 0.06, faces="yxXzZ")
    bx(sh, M("DN_Steel"), rx0 - 0.08, rx1 + 0.08, -0.04, 0.04, rz0 - 0.08, rz0, faces="all")
    bx(sh, M("DN_Steel"), rx0 - 0.08, rx1 + 0.08, -0.04, 0.04, rz1, rz1 + 0.08, faces="all")
    for x in (rx0 - 0.08, rx1):
        bx(sh, M("DN_Steel"), x, x + 0.08, -0.04, 0.04, rz0, rz1, faces="xXyY")
    bx(sh, M("DN_Dark"), rx0, rx1, -0.025, 0.025, rz0, rz1, faces="yY")
    for x in (rx0 + 0.4, rx1 - 0.4):
        tube(sh, (x, 0, CROWN - 0.02), (x, 0, rz0 - 0.08), 0.03, M("DN_Steel"), seg=8, caps=False)
    return extra


def ground_z_of(x, y):
    """The ground under a point of the diner's frame, relative to the floor (negative)."""
    return GROUND(x, y)


def main():
    global GROUND
    C.reset()
    # the floor over the highest ground under the car
    if W and T:
        corners = [(x, y) for x in (XW, 0.0, XE, XE + R) for y in (YF, 0.0, YB)] + [(AX0, AY1), (AX1, AY1)]
        gz = {c: T.ground_z(*W.diner_to_map(*c)) for c in corners}
        floor = max(gz.values()) + FLOOR_OVER_GROUND
        lowest = min(gz.values())
        GROUND = lambda x, y: T.ground_z(*W.diner_to_map(x, y)) - floor  # noqa: E731
    else:
        floor, lowest = 0.0, -FLOOR_OVER_GROUND
        GROUND = lambda x, y: -FLOOR_OVER_GROUND  # noqa: E731
    found_z = lowest - floor - 0.3
    door_ground = GROUND(DOOR_X, YF - 1.6)
    back_ground = GROUND(BACK_DOOR_X, AY1 + 1.6)
    sh, gl = build_shell(found_z, door_ground, back_ground)
    extra = signs(sh)
    ins = bmesh.new()
    counter(ins)
    for x in STOOLS:
        stool(ins, x, STOOL_Y)
    for xa, xb in BOOTHS:
        booth(ins, min(xa, xb), max(xa, xb))
    nose_bench(ins)
    back_bar(ins)
    station = coffee_station(ins)
    kitchen(ins)
    lights = lamps(ins)
    # the glass bits of the inside (sugar pourers, the pie case, gauges) go to the glass mesh with the panes
    C.export(sh, "SM_Diner_Shell", extra_mats=extra, split_glass=False)
    tris_in = C.export(ins, "SM_Diner_Inside", smooth=["DN_Chrome", "DN_China", "DN_Vinyl", "DN_Opal", "DN_Steel", "DN_Crust"])
    C.export(gl, "SM_Diner_Glass", split_glass=False)
    # ---- the layout for the game (diner frame, metres) ------------------------------------------------------------------
    serving = []
    for i, x in enumerate(STOOLS):
        serving.append({"stool": [x, STOOL_Y, STOOL_SEAT], "plate": [x, 0.58, COUNTER_TOP], "stand": [x, STOOL_Y - 0.45, 0.0]})
    seats = []
    for xa, xb in BOOTHS:
        lo, hi = min(xa, xb), max(xa, xb)
        for xs, yaw in ((lo + 0.33, 0.0), (hi - 0.33, 180.0)):
            for y in (YI_F + 0.3, BOOTH_Y - 0.25):
                seats.append({"at": [round(xs, 3), round(y, 3), 0.46], "yaw": yaw, "table": [round((lo + hi) / 2, 3), round((YI_F + BOOTH_Y - 0.08) / 2, 3), 0.765]})
    doors = [
        {"name": "front", "at": [DOOR_X, YF + TH / 2, 0.0], "out": [0, 1], "hole": list(DOOR_HOLE), "wall": TH, "leaves": 1, "hinge": "left",
         "frame": True, "open": False, "locked": False, "outside": True},
        {"name": "kitchen", "at": [KITCHEN_DOOR_X, YB - TH / 2, 0.0], "out": [0, 1], "hole": list(DOOR_HOLE), "wall": TH, "leaves": 1,
         "hinge": "right", "frame": True, "open": True, "locked": False, "outside": False},
        {"name": "back", "at": [BACK_DOOR_X, AY1 - ATH / 2, 0.0], "out": [0, -1], "hole": list(DOOR_HOLE), "wall": ATH, "leaves": 1,
         "hinge": "left", "frame": True, "open": False, "locked": False, "outside": True},
    ]
    layout = {
        "_doc": "blender/diner/building.py: the diner's frame (metres; x along the car, -y the street front, z over the floor); "
                "tools/world_map.py diner() / diner_to_map() put it on the map, floor_z is the floor's map height.",
        "prototype": "Modern Diner, Pawtucket RI (Sterling Streamliner, J. B. Judkins Co., 1940)",
        "floor_z": round(floor, 3),
        "parts": ["SM_Diner_Shell", "SM_Diner_Inside", "SM_Diner_Inside_Glass", "SM_Diner_Glass"],
        "box": {"inside": [XI_W, XE + R - TH, YI_F, YI_B, 0.0, CROWN_IN], "kitchen": [AX0 + ATH, AX1 - ATH, YB, AY1 - ATH, 0.0, A_TOP]},
        "doors": doors,
        "lights": lights,
        "counter": {"x": list(COUNTER_X), "top": COUNTER_TOP, "serving": serving},
        "booth_seats": seats,
        "nose_tables": [[x, y, 0.765] for x, y in NOSE_TABLES],
        "coffee": station,
        "menu_board": [(MENU[0] + MENU[1]) / 2, YI_B, (MENU[2] + MENU[3]) / 2],
        "staff": [
            {"name": "counter_w", "at": [-2.2, 1.45, 0.0], "yaw": -90.0},
            {"name": "counter_e", "at": [1.4, 1.45, 0.0], "yaw": -90.0},
            {"name": "register", "at": [COUNTER_X[1] - 0.32, 1.45, 0.0], "yaw": -90.0},
            {"name": "grill", "at": [(GRILL_X[0] + GRILL_X[1]) / 2, 1.35, 0.0], "yaw": 90.0},
            {"name": "stove", "at": [AX1 - ATH - 1.15, 3.75, 0.0], "yaw": 0.0},
        ],
        "entrance": {"outside": [DOOR_X, YF - 1.1 - 0.3 * 5, None], "inside": [DOOR_X, YF + 1.2, 0.0],
                     "back_outside": [BACK_DOOR_X, AY1 + 2.6, None], "back_inside": [BACK_DOOR_X, AY1 - 1.0, 0.0]},
        "tris_inside": tris_in,
    }
    os.makedirs(os.path.dirname(LAYOUT), exist_ok=True)
    with open(LAYOUT, "w") as f:
        json.dump(layout, f, indent=1)
    print("DINER floor %.2f (map), foundation %.2f, door steps rise %.2f, back %.2f" % (floor, found_z, -door_ground, -back_ground), flush=True)


GROUND = None

if __name__ == "__main__":
    main()
