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
from diner_common import M, bx, box, rbox, tube, lathe, sphere  # noqa: E402,F401
from building_util import oq, ybox  # noqa: E402,F401
import seating  # noqa: E402
import counter  # noqa: E402
import kitchen  # noqa: E402
from shell_kit import face_uv, sweep, sweep_plan, mitre_normals, bead, rrect, Loop, pipe  # noqa: E402,F401

sys.path.insert(0, os.path.join(C.ROOT, "tools"))
try:
    import world_map as W  # noqa: E402
    import terrain as T  # noqa: E402
except Exception as e:  # noqa: BLE001
    W = T = None
    print("DINER no map (%s): flat ground" % e)

LAYOUT = (os.path.join(os.environ["DINER_OUT"], "layout.json") if os.environ.get("DINER_OUT") else
          os.path.join(C.ROOT, "unreal", "Lyudmilovsk", "Content", "Diner", "layout.json"))     # read by ALyudDiner and people_map.py

from plan import *  # noqa: E402,F401,F403  (the shared measures)


# ---- walls -----------------------------------------------------------------------------------------------------------------

RC, CSEG = 0.45, 6           # the front-west corner's plan radius and its segments (the back-west corner meets the annex)
LIP = 0.07                   # the roof's overhang past the walls
NQ_ROOF, NQ_CEIL = 16, 20    # segments per quarter of the barrel's cross-section, outside and inside


def outline_edges():
    """The car's outer plan, counter-clockwise from the back-west corner: (points, kind of the edge leaving each point)."""
    pts = [(XW, YB), (XW, YF + RC)]
    kinds = ["west"]
    for i in range(1, CSEG):
        a = math.pi + math.pi / 2 * i / CSEG
        pts.append((XW + RC + RC * math.cos(a), YF + RC + RC * math.sin(a)))
        kinds.append("corner")
    pts.append((XW + RC, YF))
    kinds.append("corner")
    pts.append((XE, YF))
    kinds.append("front")
    for i in range(1, NOSE_SEG):
        a = -math.pi / 2 + math.pi * i / NOSE_SEG
        pts.append((XE + R * math.cos(a), R * math.sin(a)))
        kinds.append("nose")
    pts.append((XE, YB))
    kinds.append("nose")
    kinds.append("back")
    return pts, kinds


def outline():
    """The car's outer plan, counter-clockwise: [(x, y)]."""
    return outline_edges()[0]


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


# profiles (off, dz) for the sweeps of shell_kit
FRAME_PROF = [(0.0, 0.024), (0.016, 0.026), (0.027, 0.022), (0.033, 0.013), (0.035, 0.005), (0.020, 0.005), (0.020, -0.005),
              (0.035, -0.005), (0.033, -0.013), (0.027, -0.022), (0.016, -0.026), (0.0, -0.024)]   # a sash: rounded lips, a groove for the pane
BAR_PROF = [(0.011, -0.005), (0.017, -0.005), (0.017, -0.012), (0.014, -0.019), (0.006, -0.021), (-0.006, -0.021), (-0.014, -0.019),
            (-0.017, -0.012), (-0.017, -0.005), (-0.011, -0.005), (-0.011, 0.005), (-0.017, 0.005), (-0.017, 0.012), (-0.014, 0.019),
            (-0.006, 0.021), (0.006, 0.021), (0.014, 0.019), (0.017, 0.012), (0.017, 0.005), (0.011, 0.005)]   # a muntin with a groove each side
SILL_STEEL = [(0.0, 0.0), (0.05, -0.012), (0.054, -0.02), (0.05, -0.026), (0.0, -0.032)]              # the drip sill outside
SILL_CONC = [(0.0, 0.0), (0.07, -0.016), (0.075, -0.03), (0.07, -0.05), (0.0, -0.06)]
STOOL = ([(-0.04, -0.012)] + [(0.046 + 0.014 * math.cos(math.radians(a)), 0.002 + 0.014 * math.sin(math.radians(a))) for a in range(-90, 91, 30)]
         + [(-0.04, 0.016)])                                                                            # the mahogany stool, bull-nosed
FLUTE_PROF = bead(0.022, 6)
STRIP_PROF = [(0.0, -0.018), (0.008, -0.017), (0.014, -0.011), (0.0145, 0.0), (0.014, 0.011), (0.008, 0.017), (0.0, 0.018)]
SKIRT_PROF = [(0.0, -0.15), (0.012, -0.148), (0.03, -0.135), (0.036, -0.115), (0.036, 0.05), (0.028, 0.068), (0.012, 0.078), (0.0, 0.08)]
EAVE_PROF = [(0.0, -0.04), (0.012, -0.038), (0.024, -0.03), (0.03, -0.016), (0.03, 0.0), (0.0, 0.0)]
CORNICE = [(0.0, -0.075), (0.010, -0.073), (0.024, -0.062), (0.035, -0.04), (0.038, -0.02), (0.034, -0.006), (0.0, 0.0)]
CHAIR = [(0.0, -0.045), (0.012, -0.043), (0.025, -0.032), (0.03, -0.016), (0.027, -0.004), (0.014, 0.006), (0.0, 0.01)]
BASE = [(0.0, 0.0), (0.016, 0.0), (0.02, 0.012), (0.017, 0.085), (0.009, 0.098), (0.0, 0.1)]
RIB = [(0.0, -0.032), (0.018, -0.032), (0.026, -0.024), (0.026, 0.024), (0.018, 0.032), (0.0, 0.032)]
PLINTH = [(0.0, 0.0), (0.04, 0.0), (0.04, 0.33), (0.028, 0.36), (0.0, 0.37)]
COPING = [(0.0, 0.0), (0.0, 0.036), (-0.012, 0.05), (-0.128, 0.05), (-0.14, 0.036), (-0.14, 0.0)]


def trim_profile(w, h):
    """An architrave bar w wide (off, from the hole's edge outward), h proud of the wall."""
    return [(w, 0.0), (w, 0.012), (w - 0.007, h - 0.005), (w - 0.018, h), (0.012, h), (0.0, h - 0.008), (0.0, 0.0)]


CAR_STYLE = dict(frame=None, sill=SILL_STEEL, sill_mat=None, stool=None, trim_out=None, trim_in=None, tw_out=0.085, tw_in=0.07)


def car_style(**kw):
    st = dict(CAR_STYLE, frame=M("DN_Chrome"), sill_mat=M("DN_Steel"), stool=M("DN_Mahogany"), trim_out=M("DN_Steel"), trim_in=M("DN_Mahogany"))
    st.update(kw)
    return st


def wall(sh, p0, p1, q0, q1, opens, out_mat, in_mat, z0_out=WALL_Z0, z1_out=EAVE, z0_in=0.0, z1_in=EAVE_IN, reveal=None, glass=None,
         ubreaks=(), s_out=0.0, s_in=0.0, style=None):
    """A straight wall: outer face along p0 -> p1 (the outside on its right), inner face along q0 -> q1 (the inner polygon's
    edge). out_mat(u, z) / in_mat(u, z) pick each cell's material; openings cut through both with reveals, window sashes with a
    drip sill outside and a stool inside, a pane in the glass mesh, and architraves round the doors. s_out / s_in: the arc length
    of the outer / inner outline at the wall's start (the cells' u runs on, so a texture is continuous round the nose)."""
    p0, p1 = Vector(p0), Vector(p1)
    L = (p1 - p0).length
    d = (p1 - p0) / L
    n = Vector((d.y, -d.x))                       # outward
    t = (Vector(q0) - p0).dot(-n)                 # thickness
    ui0, ui1 = (Vector(q0) - p0).dot(d), (Vector(q1) - p0).dot(d)
    reveal = M("DN_Steel") if reveal is None else reveal
    st = style or car_style()
    d3, n3, Z3 = Vector((d.x, d.y, 0)), Vector((n.x, n.y, 0)), Vector((0, 0, 1))

    def P(u, z, inner):
        q = p0 + d * u - (n * t if inner else Vector((0, 0)))
        return (q.x, q.y, z)

    def Lp(u, z, w):                              # a point of the wall frame: u along, z up, w out of the outer face
        return Vector((p0.x + d.x * u + n.x * w, p0.y + d.y * u + n.y * w, z))

    def cell(pts, mi, want, inner):
        sb, du0 = (s_in, ui0) if inner else (s_out, 0.0)
        uvs = [(sb + (v[0] - p0.x) * d.x + (v[1] - p0.y) * d.y - du0, v[2]) for v in pts]
        face_uv(sh, pts, uvs, mi, want)

    def cells(inner):
        u_lo, u_hi = (ui0, ui1) if inner else (0.0, L)
        z_lo, z_hi = (z0_in, z1_in) if inner else (z0_out, z1_out)
        us = {u_lo, u_hi} | {min(max(u, u_lo), u_hi) for u in ubreaks}
        zs = {z_lo, z_hi}
        for o in opens:
            us.update((min(max(o.u0, u_lo), u_hi), min(max(o.u1, u_lo), u_hi)))
            zs.update((min(max(o.z0, z_lo), z_hi), min(max(o.z1, z_lo), z_hi)))
        for b in (SILL, HEAD, 0.0):
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
                cell([P(a, c, inner), P(b, c, inner), P(b, e, inner), P(a, e, inner)], mi, want, inner)

    cells(False)
    cells(True)

    def window(o):
        wc = -t / 2
        wd, ht = o.u1 - o.u0, o.z1 - o.z0
        sweep(sh, [Lp(o.u0, o.z0, wc), Lp(o.u1, o.z0, wc), Lp(o.u1, o.z1, wc), Lp(o.u0, o.z1, wc)],
              [d3 + Z3, -d3 + Z3, -d3 - Z3, d3 - Z3], [n3] * 4, FRAME_PROF, st["frame"], closed=True)
        zt = o.z1 - max(0.2, 0.26 * ht)                                              # the transom rail
        ai = 0.035                                                                  # the sash's inner edge: bars meet it, never overlap its groove
        sweep(sh, [Lp(o.u0 + ai, zt, wc), Lp(o.u1 - ai, zt, wc)], [Z3, Z3], [n3, n3], BAR_PROF, st["frame"], close_prof=True)
        um = (o.u0 + o.u1) / 2
        wide = wd > 0.7
        if wide:                                                                    # one muntin down the middle, in two pieces
            for za, zb in ((o.z0 + ai, zt - 0.017), (zt + 0.017, o.z1 - ai)):
                sweep(sh, [Lp(um, za, wc), Lp(um, zb, wc)], [d3, d3], [n3, n3], BAR_PROF, st["frame"], close_prof=True)
        if glass is not None:                                                       # the lights: 3 mm clear of every groove's bottom
            cols = [(o.u0 + 0.028, um - 0.014), (um + 0.014, o.u1 - 0.028)] if wide else [(o.u0 + 0.028, o.u1 - 0.028)]
            rows = [(o.z0 + 0.028, zt - 0.014), (zt + 0.014, o.z1 - 0.028)]
            for ua, ub in cols:
                for za, zb in rows:
                    for sg in (1, -1):
                        oq(glass, [Lp(ua, za, wc), Lp(ub, za, wc), Lp(ub, zb, wc), Lp(ua, zb, wc)], M("DN_Glass"), tuple(n3 * sg))
        sweep(sh, [Lp(o.u0 - 0.03, o.z0, 0.0), Lp(o.u1 + 0.03, o.z0, 0.0)], [n3, n3], [Z3, Z3], st["sill"], st["sill_mat"])    # drip sill
        sweep(sh, [Lp(o.u0 - 0.04, o.z0, -t), Lp(o.u1 + 0.04, o.z0, -t)], [-n3, -n3], [Z3, Z3], STOOL, st["stool"])          # the stool

    def door(o):
        for inner in (False, True):
            mat, w_, h_ = (st["trim_in"], st["tw_in"], 0.03) if inner else (st["trim_out"], st["tw_out"], 0.026)
            wb = -t if inner else 0.0
            W3 = -n3 if inner else n3
            sweep(sh, [Lp(o.u0, o.z0 + 0.22, wb), Lp(o.u0, o.z1, wb), Lp(o.u1, o.z1, wb), Lp(o.u1, o.z0 + 0.22, wb)],
                  [-d3, -d3 + Z3, d3 + Z3, d3], [W3] * 4, trim_profile(w_, h_), mat)
            for ue, sgn in ((o.u0, -1), (o.u1, 1)):                                  # a plinth block (kick-plate height) at each foot
                sweep(sh, [Lp(ue, o.z0, wb), Lp(ue, o.z0 + 0.22, wb)], [d3 * sgn] * 2, [W3] * 2, trim_profile(w_ + 0.014, h_ + 0.012), mat)

    for o in opens:
        # reveals: sill (unless at the bottom of both faces), head, jambs
        if o.z0 > z0_in + 1e-4 or o.kind == "door":
            oq(sh, [P(o.u0, o.z0, False), P(o.u1, o.z0, False), P(o.u1, o.z0, True), P(o.u0, o.z0, True)], reveal, (0, 0, 1))
        oq(sh, [P(o.u0, o.z1, False), P(o.u1, o.z1, False), P(o.u1, o.z1, True), P(o.u0, o.z1, True)], reveal, (0, 0, -1))
        for u, s in ((o.u0, 1), (o.u1, -1)):
            oq(sh, [P(u, o.z0, False), P(u, o.z0, True), P(u, o.z1, True), P(u, o.z1, False)], reveal, (d.x * s, d.y * s, 0))
        if o.kind == "window":
            window(o)
        elif o.kind == "door":
            door(o)
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


def outer_trims(sh, lo, kinds, out, iF, u_door, iB, kitchen_u):
    """What stands on the car's outside: three stainless half-round flutes round the red panels, the base skirt rail, the eave
    moulding, and the porcelain panels' joint strips every 1.2 m. They stop at the door's architrave and at the annex."""
    hw = DOOR_HOLE[0] / 2
    pad = 0.085 + 0.014 + 0.012
    door_block = (lo.cum[iF] + u_door - hw - pad, lo.cum[iF] + u_door + hw + pad)
    annex_a = lo.cum[iB] + kitchen_u[0]
    pieces = lo.complement([door_block, (annex_a, lo.L)], wrap=False)
    steel = M("DN_Steel")
    for a, b in pieces:
        path = lo.walk(a, b)
        for z in (0.22, 0.52, 0.82):
            sweep_plan(sh, path, z, FLUTE_PROF, steel, side=1, s0=a)
        sweep_plan(sh, path, 0.0, SKIRT_PROF, steel, side=1, s0=a)
    sweep_plan(sh, lo.walk(0.0, annex_a), EAVE, EAVE_PROF, steel, side=1)
    # joint strips: straight walls every 1.2 m about the door (front) or from the end (west, back); the nose every second facet
    N = mitre_normals(out, True, 1)
    pts = [Vector(p) for p in out]

    def strip(x, y, nrm, tan):
        sweep(sh, [(x, y, 0.08), (x, y, SILL)], [(nrm[0], nrm[1], 0)] * 2, [(tan[0], tan[1], 0)] * 2, STRIP_PROF, steel)
    xd = DOOR_X
    for k in range(6):
        for sg in (-1, 1):
            x = xd + sg * (hw + 0.65 + 1.2 * k)
            if XW + RC + 0.3 < x < XE - 0.25:
                strip(x, YF, (0, -1), (1, 0))
    for k in range(4):
        strip(XW, YB - 0.9 - 1.2 * k, (-1, 0), (0, -1))
    for k in range(8):
        u = 0.6 + 1.2 * k
        if u < kitchen_u[0] - 0.3:
            strip(XE - u, YB, (0, 1), (-1, 0))
    j0 = kinds.index("nose")
    for j in range(2, NOSE_SEG - 1, 2):
        i = j0 + j
        tn = (pts[(i + 1) % len(pts)] - pts[i - 1]).normalized()
        strip(pts[i].x, pts[i].y, (N[i].x / N[i].length, N[i].y / N[i].length), (tn.x, tn.y))


def inner_trims(sh, li, inn, blocks_rail, blocks_base):
    """Inside: the mahogany cornice where the walls meet the ceiling (all round), the chair rail at the sill line (broken at
    windows and doors) and the dark cove base on the floor (broken at doors; its bottom edge 2 mm above the floor)."""
    mah, dark = M("DN_Mahogany"), M("DN_Dark")
    sweep_plan(sh, inn, EAVE_IN, CORNICE, mah, side=-1, closed=True)
    for a, b in li.complement(blocks_rail):
        sweep_plan(sh, li.walk(a, b), SILL, CHAIR, mah, side=-1, s0=a)
    for a, b in li.complement(blocks_base):
        sweep_plan(sh, li.walk(a, b), 0.002, BASE, dark, side=-1, s0=a)


def arc_profile(r, z0, z1, nq):
    """The barrel's quarter section (r, z) from the eave to the crown, and the arc length at each point."""
    pts = [(r * math.cos(math.pi / 2 * k / nq), z0 + (z1 - z0) * math.sin(math.pi / 2 * k / nq)) for k in range(nq + 1)]
    s = [0.0]
    for a, b in zip(pts, pts[1:]):
        s.append(s[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
    return pts, s


def barrel(sh, x0, x1, prof, s, mat_of, f):
    """The barrel over x0..x1: u = x, v = signed arc length from the crown (continuous over it); f +1 faces out, -1 in."""
    S = s[-1]
    for sg in (-1, 1):
        for k in range(len(prof) - 1):
            (r0, z0), (r1, z1) = prof[k], prof[k + 1]
            v0, v1 = sg * (S - s[k]), sg * (S - s[k + 1])
            face_uv(sh, [(x0, sg * r0, z0), (x1, sg * r0, z0), (x1, sg * r1, z1), (x0, sg * r1, z1)],
                    [(x0, v0), (x1, v0), (x1, v1), (x0, v1)], mat_of(k), (0, sg * (z1 - z0) * f, -(r1 - r0) * f))


def dome(sh, ring, nseg, prof, s, mat_of, f, cx):
    """A dome of nseg meridians from ring(k, j) (profile ring k, meridian j): azimuthal-equidistant UVs round the crown pole
    (u = cx + l cos(phi), v = l sin(phi), l the arc length to the crown), which continue the barrel's mapping at its end."""
    S = s[-1]
    for j in range(nseg):
        ph0 = -math.pi / 2 + math.pi * j / nseg
        ph1 = -math.pi / 2 + math.pi * (j + 1) / nseg
        phm = (ph0 + ph1) / 2
        for k in range(len(prof) - 1):
            (r0, z0), (r1, z1) = prof[k], prof[k + 1]
            l0, l1 = S - s[k], S - s[k + 1]
            q = [ring(k, j), ring(k, j + 1), ring(k + 1, j + 1), ring(k + 1, j)]
            uv = [(cx + l0 * math.cos(ph0), l0 * math.sin(ph0)), (cx + l0 * math.cos(ph1), l0 * math.sin(ph1)),
                  (cx + l1 * math.cos(ph1), l1 * math.sin(ph1)), (cx + l1 * math.cos(ph0), l1 * math.sin(ph0))]
            if r1 < 1e-6:
                q, uv = q[:3], uv[:3]
            want = (math.cos(phm) * (z1 - z0) * f, math.sin(phm) * (z1 - z0) * f, -(r1 - r0) * f)
            face_uv(sh, q, uv, mat_of(k), want)


def roof_and_ceiling(sh, out, inn):
    """The barrel roof (stainless fascia, painted metal above) over the straight part, a half dome over the nose, the cream gable at
    the west end, the soffit under the overhang, and inside the barrel ceiling with a mahogany rib every 1.22 m."""
    steel, roof, ceil, mah = M("DN_Steel"), M("DN_Roof"), M("DN_Ceiling"), M("DN_Mahogany")
    Rr = R + LIP
    rprof, rs = arc_profile(Rr, EAVE, CROWN, NQ_ROOF)
    fas = lambda k: steel if k < 2 else roof  # noqa: E731
    barrel(sh, XW - LIP, XE, rprof, rs, fas, 1)
    nseg = 32
    ring_r = lambda k, j: (XE + rprof[k][0] * math.cos(-math.pi / 2 + math.pi * j / nseg),       # noqa: E731
                           rprof[k][0] * math.sin(-math.pi / 2 + math.pi * j / nseg), rprof[k][1])
    dome(sh, ring_r, nseg, rprof, rs, fas, 1, XE)
    # the west gable: a cream face at the roof's west edge with a stainless bead round its arch
    arc = [(-r, z) for r, z in rprof] + [(r, z) for r, z in reversed(rprof)][1:]
    face_uv(sh, [(XW - LIP, y, z) for y, z in arc], None, M("DN_EnamelCream"), (-1, 0, 0))
    gp = [(XW - LIP, y, z) for y, z in arc]
    N, W = [], []
    for i in range(len(gp)):
        a, b = gp[max(i - 1, 0)], gp[min(i + 1, len(gp) - 1)]
        ty, tz = b[1] - a[1], b[2] - a[2]
        ln = math.hypot(ty, tz)
        N.append((0, tz / ln, -ty / ln))
        W.append((-1, 0, 0))
    sweep(sh, gp, N, W, [(0.03 + 0.016 * math.cos(math.radians(a)), 0.016 * math.sin(math.radians(a))) for a in range(180, -1, -30)], steel)
    # the soffit: the overhang's underside, from the walls out to the roof's edge
    dn = (0, 0, -1)
    sof = lambda pts: face_uv(sh, [(x, y, EAVE) for x, y in pts], None, steel, dn)  # noqa: E731
    sof([(XW + RC, YF), (XE, YF), (XE, YF - LIP), (XW + RC, YF - LIP)])
    sof([(XE, YB), (XW, YB), (XW, YB + LIP), (XE, YB + LIP)])
    sof([(XW, YB), (XW, YF + RC), (XW - LIP, YF + RC), (XW - LIP, YB)])
    sof([(XW, YB), (XW - LIP, YB), (XW - LIP, YB + LIP), (XW, YB + LIP)])
    C0 = (XW - LIP, YF - LIP)
    arcp = out[1:CSEG + 2]
    for a, b in zip(arcp, arcp[1:]):
        sof([C0, a, b])
    sof([C0, (XW - LIP, YF + RC), arcp[0]])
    sof([C0, arcp[-1], (XW + RC, YF - LIP)])
    B = lambda m: (XE + Rr * math.cos(-math.pi / 2 + math.pi * m / (2 * NOSE_SEG)), Rr * math.sin(-math.pi / 2 + math.pi * m / (2 * NOSE_SEG)))  # noqa: E731
    jn = CSEG + 2
    for j in range(NOSE_SEG):
        sof([out[jn + j], out[jn + j + 1], B(2 * j + 2), B(2 * j + 1), B(2 * j)])
    # ---- the ceiling ---------------------------------------------------------------------------------------------------------
    rc = R - TH
    cprof, cs = arc_profile(rc, EAVE_IN, CROWN_IN, NQ_CEIL)
    xe_in = inn[jn][0]                                       # where the inner straight walls end (the nose's first inner vertex)
    barrel(sh, XI_W, xe_in, cprof, cs, lambda k: ceil, -1)
    c0 = Vector((xe_in, 0.0))
    ring_c = lambda k, j: (c0.x + (inn[jn + j][0] - c0.x) * cprof[k][0] / rc, (inn[jn + j][1]) * cprof[k][0] / rc, cprof[k][1])  # noqa: E731
    dome(sh, ring_c, NOSE_SEG, cprof, cs, lambda k: ceil, -1, xe_in)
    gin = [(XI_W, y, z) for y, z in [(-r, z) for r, z in cprof] + [(r, z) for r, z in reversed(cprof)][1:]]
    face_uv(sh, gin, None, ceil, (1, 0, 0))
    # ribs: transverse every 1.22 m (the ceiling texture's panel joints), radial over the nose, a rosette at the crown
    hh = CROWN_IN - EAVE_IN

    def inward(a, sg_y=1.0, ph=None):
        """Unit inward normal of the ceiling at quarter angle a (the barrel: across y; the dome: about the meridian ph)."""
        ny, nz = math.cos(a) / rc, math.sin(a) / hh
        ln = math.hypot(ny, nz)
        if ph is None:
            return Vector((0, -sg_y * ny / ln, -nz / ln))
        return Vector((-math.cos(ph) * ny / ln, -math.sin(ph) * ny / ln, -nz / ln))
    ang = [math.pi / 2 * k / NQ_CEIL for k in range(NQ_CEIL + 1)]
    xs = [k * 1.22 for k in range(-5, 6)] + [xe_in - 0.034]
    for xc in xs:
        if xc > xe_in - 0.05:                                # the joint rib stops short of the crown rosette
            for sg in (-1, 1):
                ks = [k for k in range(NQ_CEIL + 1) if cprof[k][0] > 0.36]
                sweep(sh, [(xc, sg * cprof[k][0], cprof[k][1]) for k in ks], [inward(ang[k], float(sg)) for k in ks], [(1, 0, 0)] * len(ks), RIB, mah)
            continue
        P = [(xc, -r, z) for r, z in cprof] + [(xc, r, z) for r, z in reversed(cprof)][1:]
        Nn = [inward(a, -1.0) for a in ang] + [inward(a, 1.0) for a in reversed(ang)][1:]
        sweep(sh, P, Nn, [(1, 0, 0)] * len(P), RIB, mah)
    for j in range(2, NOSE_SEG - 1, 2):
        ph = -math.pi / 2 + math.pi * j / NOSE_SEG
        ks = range(1, NQ_CEIL - 1)
        P = [ring_c(k, j) for k in ks]
        Nn = [inward(ang[k], 1.0, ph) for k in ks]
        sweep(sh, P, Nn, [(-math.sin(ph), math.cos(ph), 0)] * len(P), RIB, mah)
    lathe(sh, [(0.3, CROWN_IN + 0.004), (0.3, CROWN_IN - 0.02), (0.24, CROWN_IN - 0.036), (0.0, CROWN_IN - 0.036)], mah, seg=24, c=(xe_in, 0, 0), cap0=False, cap1=False)


def build_shell(found_z, door_ground, back_ground):
    sh = bmesh.new()
    gl = bmesh.new()
    C.ensure_uv(sh)
    out, kinds = outline_edges()
    inn = offset_poly(out, TH)
    n = len(out)
    lo, li = Loop(out), Loop(inn)
    iF, iB = kinds.index("front"), kinds.index("back")
    u_door = DOOR_X - out[iF][0]
    hw = DOOR_HOLE[0] / 2
    kitchen_u = (XE - AX1, XE - AX0)
    blocks_rail, blocks_base = [], []
    for i in range(n):
        p0, p1 = out[i], out[(i + 1) % n]
        q0, q1 = inn[i], inn[(i + 1) % n]
        L = (Vector(p1) - Vector(p0)).length
        kind = kinds[i]
        opens = []
        kitchen = None
        ubreaks = ()
        style = car_style()
        if kind == "front":                           # the street front: the door and the windows
            opens.append(Opening(u_door - hw, u_door + hw, 0.0, DOOR_HOLE[1], "door"))
            opens += band_windows(0.2, u_door - hw - 0.35)
            opens += band_windows(u_door + hw + 0.35, L - 0.12)
        elif kind == "nose":                          # the nose: a window in every face
            opens.append(Opening(0.12, L - 0.12, SILL, HEAD))
        elif kind == "back":                          # the back: the kitchen door, windows over the east half's back bar
            u_k = XE - KITCHEN_DOOR_X
            opens.append(Opening(u_k - hw, u_k + hw, 0.0, DOOR_HOLE[1], "door"))
            opens += band_windows(0.25, XE - MENU[1] - 0.35, sill=1.45)
            kitchen = lambda u: kitchen_u[0] - 0.01 < u < kitchen_u[1] + 0.01  # noqa: E731
            ubreaks = kitchen_u
            style = car_style(trim_out=M("DN_EnamelCream"), tw_out=0.04)
        elif kind == "west":                          # the west end: one window
            opens += [Opening(1.3, 2.3, SILL, HEAD)]
        wall(sh, p0, p1, q0, q1, opens, car_out_mat(kitchen=kitchen), car_in_mat, glass=gl, ubreaks=ubreaks, s_out=lo.cum[i], s_in=li.cum[i],
             style=style)
        d = (Vector(p1) - Vector(p0)).normalized()
        ui0 = (Vector(q0) - Vector(p0)).dot(d)
        for o in opens:
            a, b = li.cum[i] + o.u0 - ui0, li.cum[i] + o.u1 - ui0
            if o.kind == "door":
                blocks_rail.append((a - 0.1, b + 0.1))
                blocks_base.append((a - 0.1, b + 0.1))
            elif o.z0 < SILL + 0.05:
                blocks_rail.append((a - 0.045, b + 0.045))
    outer_trims(sh, lo, kinds, out, iF, u_door, iB, kitchen_u)
    inner_trims(sh, li, inn, blocks_rail, blocks_base)
    roof_and_ceiling(sh, out, inn)
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
    annex(sh, gl, found_z, back_ground)
    # ---- the steps: a landing at the floor and steps down to the ground, at the front and the back doors ------------------
    steps(sh, DOOR_X, YF, -1, door_ground)
    steps(sh, BACK_DOOR_X, AY1, 1, back_ground)
    return sh, gl


def annex(sh, gl, found_z, back_ground):
    """A brick kitchen behind the west half of the car (flat roof under the car's eave), its outer walls x AX0..AX1, y YB..AY1:
    a concrete plinth band, painted steel windows with concrete sills, cream door trim and a concrete coping on the roof's edge."""
    ring = [(AX1, YB), (AX1, AY1), (AX0, AY1), (AX0, YB)]
    inner = [(AX1 - ATH, YB), (AX1 - ATH, AY1 - ATH), (AX0 + ATH, AY1 - ATH), (AX0 + ATH, YB)]
    brick = lambda u, z: M("DN_Brick")  # noqa: E731
    plaster = lambda u, z: M("DN_Plaster") if z > 1.2 else M("DN_WallTile")  # noqa: E731
    style = car_style(frame=M("DN_Dark"), sill=SILL_CONC, sill_mat=M("DN_Concrete"), stool=M("DN_WallTile"), trim_out=M("DN_EnamelCream"),
                      trim_in=M("DN_EnamelCream"), tw_out=0.085, tw_in=0.04)
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
             reveal=M("DN_Concrete"), glass=gl, style=style)
    conc = M("DN_Concrete")
    sweep_plan(sh, [(AX1, YB), (AX1, AY1), (AX0, AY1), (AX0, YB)], back_ground - 0.05, PLINTH, conc, side=1)
    # roof (top, a concrete lip over the walls) and ceiling
    oq(sh, [(AX0 - 0.05, YB, A_TOP), (AX1 + 0.05, YB, A_TOP), (AX1 + 0.05, AY1 + 0.05, A_TOP), (AX0 - 0.05, AY1 + 0.05, A_TOP)], M("DN_Roof"), (0, 0, 1))
    for a, b, want in (((AX0 - 0.05, AY1 + 0.05), (AX1 + 0.05, AY1 + 0.05), (0, 1, 0)), ((AX1 + 0.05, YB), (AX1 + 0.05, AY1 + 0.05), (1, 0, 0)),
                       ((AX0 - 0.05, YB), (AX0 - 0.05, AY1 + 0.05), (-1, 0, 0))):
        oq(sh, [(a[0], a[1], A_TOP - 0.1), (b[0], b[1], A_TOP - 0.1), (b[0], b[1], A_TOP), (a[0], a[1], A_TOP)], conc, want)
    sweep_plan(sh, [(AX1 + 0.05, YB), (AX1 + 0.05, AY1 + 0.05), (AX0 - 0.05, AY1 + 0.05), (AX0 - 0.05, YB)], A_TOP, COPING, conc, side=1)
    # the lip's underside outside the walls
    for poly in (((AX0 - 0.05, AY1), (AX1 + 0.05, AY1), (AX1 + 0.05, AY1 + 0.05), (AX0 - 0.05, AY1 + 0.05)),
                 ((AX1, YB), (AX1 + 0.05, YB), (AX1 + 0.05, AY1), (AX1, AY1)), ((AX0 - 0.05, YB), (AX0, YB), (AX0, AY1), (AX0 - 0.05, AY1))):
        oq(sh, [(x, y, A_TOP - 0.1) for x, y in poly], conc, (0, 0, -1))
    ci = A_TOP - 0.12
    oq(sh, [(AX0 + ATH, YB, ci), (AX1 - ATH, YB, ci), (AX1 - ATH, AY1 - ATH, ci), (AX0 + ATH, AY1 - ATH, ci)], M("DN_Ceiling"), (0, 0, -1))
    oq(sh, [(AX0 + ATH, YB, 0.0), (AX1 - ATH, YB, 0.0), (AX1 - ATH, AY1 - ATH, 0.0), (AX0 + ATH, AY1 - ATH, 0.0)], M("DN_KitchenTile"), (0, 0, 1))


def steps(sh, x, y_wall, s, ground):
    """A landing 1.1 m deep in front of a door at x on the wall y_wall (s: outward sign of y), then steps down to ground (z): one
    concrete block with a rounded nosing on every tread, chrome rails with returns to the wall and down into posts."""
    rise = max(0.0, -ground)
    k = max(1, int(math.ceil(rise / 0.19 - 1e-6))) if rise > 0.05 else 0
    hw = 0.95
    y0 = y_wall
    y1 = y_wall + s * 1.1
    bot = ground - 0.3
    h = rise / k if k else 0.0
    zl = -0.004                                      # the landing's top, a hair under the threshold
    # the side profile (distance out from the wall, z): convex front edges of the treads are rounded
    r_n = 0.03
    pts = []
    # build the polyline: landing edge (round), then for every tread: riser corner (sharp), nosing (round)
    pts.append(("sharp", (0.0, zl)))
    pts.append(("round", (1.1, zl)))
    z = zl
    for i in range(k):
        pts.append(("sharp", (1.1 + 0.3 * i, z - h)))
        z -= h
        pts.append(("round", (1.1 + 0.3 * (i + 1), z)))
    dmax = 1.1 + 0.3 * k
    poly = []
    for kind, (dd, zz) in pts:
        if kind == "round":
            for q in range(5):
                a = math.radians(90.0 * q / 4)
                poly.append((dd - r_n + r_n * math.sin(a), zz - r_n + r_n * math.cos(a)))
        else:
            poly.append((dd, zz))
    top = poly + [(dmax, bot)]
    conc = M("DN_Concrete")
    xa, xb = x - hw, x + hw
    for (d0, z0_), (d1, z1_) in zip(top, top[1:]):
        if math.hypot(d1 - d0, z1_ - z0_) < 1e-6:
            continue
        want = (0, s * (-(z1_ - z0_)), (d1 - d0))
        oq(sh, [(xa, y0 + s * d0, z0_), (xb, y0 + s * d0, z0_), (xb, y0 + s * d1, z1_), (xa, y0 + s * d1, z1_)], conc, want)
    side = top + [(0.0, bot)]
    for xs, wx in ((xa, -1), (xb, 1)):
        face_uv(sh, [(xs, y0 + s * dd, zz) for dd, zz in side], None, conc, (wx, 0, 0))
    # chrome hand rails: a return into the wall, a post at the landing's edge, and the rail bending down into its post
    chrome = M("DN_Chrome")
    if k:
        for xs in (x - hw + 0.06, x + hw - 0.06):
            ye = y0 + s * (dmax - 0.1)
            ze = 0.9 - rise
            gz = -rise - 0.004
            pipe(sh, [(xs, y0 + s * 0.012, 0.9), (xs, y1, 0.9), (xs, ye, ze), (xs, ye, gz)], 0.022, chrome, seg=10, bend=0.14)
            tube(sh, (xs, y0 - s * 0.01, 0.9), (xs, y0 + s * 0.022, 0.9), 0.05, chrome, seg=14)            # the wall rosette
            tube(sh, (xs, y1, zl), (xs, y1, 0.9 - 0.02), 0.018, chrome, seg=10, caps=False)
            for yf, zf in ((y1, zl), (ye, gz)):                                                              # post flanges (no underside)
                lathe(sh, [(0.04, zf), (0.04, zf + 0.012), (0.03, zf + 0.018), (0.0, zf + 0.018)], chrome, seg=14, c=(xs, yf, 0), cap0=False, cap1=False)
    return k


# ---- inside --------------------------------------------------------------------------------------------------------------

def signs(sh):
    """Picture quads (DN_Sign_<name>, their own slot) 16 mm off their faces; the roof sign in a rounded stainless frame on tube
    legs with base plates; the menu board and the coffee card in mahogany frames."""
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
    mah, steel, chrome = M("DN_Mahogany"), M("DN_Steel"), M("DN_Chrome")
    # the menu board: a backing panel and a rounded mahogany frame standing proud of the picture (edges 16 mm from it)
    bx(sh, mah, x0, x1, YI_B - 0.03, YI_B, z0, z1, faces="y")
    sweep(sh, [(x0 - 0.06, YI_B, z0 - 0.06), (x1 + 0.06, YI_B, z0 - 0.06), (x1 + 0.06, YI_B, z1 + 0.06), (x0 - 0.06, YI_B, z1 + 0.06)],
          [(1, 0, 1), (-1, 0, 1), (-1, 0, -1), (1, 0, -1)], [(0, -1, 0)] * 4,
          [(0.0, 0.0), (0.0, 0.052), (0.008, 0.062), (0.02, 0.066), (0.044, 0.066), (0.054, 0.058), (0.06, 0.05), (0.06, 0.0)], mah, closed=True)
    # the coffee card: a slim mahogany frame
    cx = XI_W
    sweep(sh, [(cx, -0.62 + 0.03, 1.2 - 0.03), (cx, -1.42 - 0.03, 1.2 - 0.03), (cx, -1.42 - 0.03, 1.6 + 0.03), (cx, -0.62 + 0.03, 1.6 + 0.03)],
          [(0, -1, 1), (0, 1, 1), (0, 1, -1), (0, -1, -1)], [(1, 0, 0)] * 4,
          [(0.0, 0.0), (0.0, 0.03), (0.006, 0.038), (0.016, 0.04), (0.026, 0.036), (0.03, 0.028), (0.03, 0.0)], mah, closed=True)
    # the roof sign: a rounded stainless frame all round, a dark core, tube legs on base plates
    fr = rrect(0.0, 0.08, -0.06, 0.06, 0.022, 3)
    sweep(sh, [(rx0 - 0.08, 0, rz0 - 0.08), (rx1 + 0.08, 0, rz0 - 0.08), (rx1 + 0.08, 0, rz1 + 0.08), (rx0 - 0.08, 0, rz1 + 0.08)],
          [(1, 0, 1), (-1, 0, 1), (-1, 0, -1), (1, 0, -1)], [(0, 1, 0)] * 4, fr, steel, closed=True, close_prof=True)
    bx(sh, M("DN_Dark"), rx0, rx1, -0.025, 0.025, rz0, rz1, faces="yY")
    for x in (rx0 + 0.4, rx1 - 0.4):
        tube(sh, (x, 0, CROWN - 0.02), (x, 0, rz0 - 0.06), 0.03, steel, seg=14, caps=False)
        lathe(sh, [(0.115, CROWN - 0.008), (0.115, CROWN + 0.012), (0.1, CROWN + 0.03), (0.0, CROWN + 0.03)], steel, seg=20, c=(x, 0, 0), cap0=False, cap1=False)
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
    parts = {"inside": bmesh.new(), "stools": bmesh.new(), "small": bmesh.new()}
    got = {}
    for mod in (seating, counter, kitchen):
        got.update(mod.build(parts))
    station, lights = got["coffee"], got["lights"]
    # the glass bits of the insides (sugar pourers, the pie case, gauges) go to their _Glass meshes
    C.export(sh, "SM_Diner_Shell", extra_mats=extra, split_glass=False, angle=40)
    tris_in = C.export(parts["inside"], "SM_Diner_Inside", angle=40)
    tris_small = C.export(parts["small"], "SM_Diner_Small", angle=40)
    tris_stools = C.export(parts["stools"], "SM_Diner_Stools", angle=40)
    C.export(gl, "SM_Diner_Glass", split_glass=False)
    # ---- the layout for the game (diner frame, metres) ------------------------------------------------------------------
    serving = []
    for i, x in enumerate(STOOLS):
        serving.append({"stool": [x, STOOL_Y, STOOL_SEAT], "plate": [x, 0.46, COUNTER_TOP], "stand": [x, STOOL_Y - 0.14, 0.0]})
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
        "parts": [p for p in ("SM_Diner_Shell", "SM_Diner_Inside", "SM_Diner_Inside_Glass", "SM_Diner_Small", "SM_Diner_Small_Glass",
                              "SM_Diner_Stools", "SM_Diner_Glass") if os.path.isfile(os.path.join(C.OUT, p + ".glb"))],
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
        "hero_seats": got.get("hero_seats", []),
        "tris": {"inside": tris_in, "small": tris_small, "stools": tris_stools},
    }
    os.makedirs(os.path.dirname(LAYOUT), exist_ok=True)
    with open(LAYOUT, "w") as f:
        json.dump(layout, f, indent=1)
    print("DINER floor %.2f (map), foundation %.2f, door steps rise %.2f, back %.2f" % (floor, found_z, -door_ground, -back_ground), flush=True)


GROUND = None

if __name__ == "__main__":
    main()
