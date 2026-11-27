"""The descent craft «Ласточка» (docs/spacecraft.md): clean game meshes made here from primitives, no generated models
(the project's rule for things you walk into: real wall thickness, boolean-cut openings, own UVs).

  blender -b -P blender/spacecraft/spacecraft_kit.py [-- --preview out_dir] [--strict]
  (or, with `pip install bpy`:  python3 blender/spacecraft/spacecraft_kit.py [--preview out_dir] [--strict])

  -> design/spacecraft/game/spacecraft_kit.glb           every piece, one mesh object each (SM_SC_*)
  -> unreal/Lyudmilovsk/Content/Spacecraft/kit.json      every measure, socket and pivot the game needs

Frames (Blender metres, Z up; UE gets (x, -y, z) * 100):
  the CABIN frame: origin = the middle of the cabin's floor, +X = forward (the panel and the big porthole), the hatch
  at -X, the hero's couch (couch 1) on +Y (his left... UE -Y). SM_SC_CabinOuter, CabinInner, Panel, Player and
  FireShell are built in it; every other cabin piece has its OWN frame (origin on its hinge or axis) and kit.json
  gives where it goes in the cabin frame.
  the STAGE frame: origin on the ice under the stage's axis (the foot pads' soles at z = 0), the hatch side at -X;
  kit.json "stage.cabin_seat" is where the cabin frame's origin stands in it.
KIT_NOTES.md beside this file lists the pieces, their material slots and every kit.json key.
Materials: tx_sc_* tile by the metre (kit.json "tiles"); SC_* are flat or drawn/replaced by the game.
"""
import contextlib
import json
import math
import os
import sys

import bpy            # (first: as a pip module bpy brings bmesh and mathutils)
import bmesh
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT_GLB = os.path.join(REPO, "design", "spacecraft", "game", "spacecraft_kit.glb")
OUT_JSON = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "Spacecraft", "kit.json")
LABELS_JSON = os.path.join(REPO, "tools", "spacecraft", "labels.json")
STRINGS_JSON = os.path.join(REPO, "tools", "spacecraft", "strings.json")
FONT = os.path.join(REPO, "fonts", "MPLUS1p-Bold.ttf")
PI = math.pi
TAU = 2 * math.pi

# --- measures (metres, cabin frame unless said) ----------------------------------------------------------------------
# the hull: a "headlight": a spherical heat shield, a cone, a flattened dome
R_IN0, R_IN1 = 1.30, 1.02      # inner radius at the floor and at the knee (inner diameter 2.6 m)
Z_KNEE = 1.55                  # where the inner cone becomes the dome
CEIL = 2.05                    # clear height in the middle
DOME_P_IN = 2.6                # the inner dome's superellipse power (flatter than a sphere: head room)
R_OUT0, R_OUT1 = 1.55, 1.22    # outer radius at the shield's rim and at the outer knee (outer diameter 3.1 m)
Z_RIM0, Z_RIM1 = -0.22, -0.06  # the shield's rim band
Z_APEX = -0.60                 # the shield's lowest point
Z_OKNEE = 1.60
Z_TOP = 2.40                   # overall height 3.0 m
DOME_P_OUT = 2.5
SHIELD_RS = (R_OUT0 ** 2 + (Z_RIM0 - Z_APEX) ** 2) / (2 * (Z_RIM0 - Z_APEX))    # the shield sphere's radius
S_OUT = math.atan2(R_OUT0 - R_OUT1, Z_OKNEE - Z_RIM1)   # the outer cone's lean from the vertical
S_IN = math.atan2(R_IN0 - R_IN1, Z_KNEE)
KICK = 0.125                   # painted kick band under the quilt
QUILT_ROWS, QUILT_COLS = 5, 24
QUILT_SLANT = math.hypot(Z_KNEE - KICK, (R_IN0 - R_IN1) * (Z_KNEE - KICK) / Z_KNEE)
QUILT = QUILT_SLANT / QUILT_ROWS                # one pillow, m (square in UV)
QUILT_RREF = QUILT * QUILT_COLS / TAU           # the radius the quilt's U is measured at: 24 pillows round, no seam
PILLOW = 0.025                 # how far a pillow's middle stands in from its seams

HATCH_AZ, HATCH_Z, HATCH_R = PI, 0.95, 0.60     # the side hatch: 1.20 m clear, its axis square to the outer cone
HATCH_T_OUT = 0.03             # its coaming's outer end (the door's seat) along the axis from the outer skin
DOOR_T, DOOR_R = 0.07, 0.655
HINGE_OFF = 0.76               # the hinge axis from the hatch axis
BIG_AZ, BIG_Z, BIG_R = 0.0, 1.30, 0.30          # the big porthole (0.60 clear)
SIDE_AZ, SIDE_Z, SIDE_R = PI / 2, 1.25, 0.18    # the side portholes (0.36 clear) at +-90 degrees
GLASS_DEPTH = 0.07             # a pane's middle under the outer skin, along its axis

# the instrument panel: a slanted face; its frame: origin at the face's bottom middle, U to the seated hero's right
# (-Y), V up the face, N out of the face toward him
PANEL_O = Vector((0.66, 0.0, 0.42))
PANEL_TILT = math.radians(18)
PANEL_W, PANEL_H = 1.40, 0.52
PANEL_ZB = 0.34                # the console's underside at the wall

# couches (own frame: origin on the floor under the pelvis, +X forward): where they stand, toed in to the panel
COUCH_AT = ((-0.05, 0.56), (-0.05, -0.56))
COUCH_YAW = (math.radians(-24), math.radians(24))
SEAT_PELVIS = Vector((0.0, 0.0, 0.42))
SEAT_EYE = Vector((-0.20, 0.0, 1.12))
STAND = Vector((-0.40, 0.0, 0.0))               # where the hero stands up inside (his feet)

# the record player's shelf (left wall) and the sleeve rack, the stowage net (right wall)
SHELF_AZ, SHELF_Z = math.radians(70), 0.74
PLAYER_RHO, PLAYER_TAU = 0.91, -0.06
RACK_AZ, RACK_Z = math.radians(84), 0.70
NET_AZ, NET_Z = math.radians(-66), 0.62

# the landing stage (stage frame)
SEAT_RING_R = 1.20             # the cradle ring the shield sits in
DECK_Z = 2.12
TRUSS_Z0, TRUSS_R = 1.00, 1.75
LADDER_FIRST, LADDER_STEP, LADDER_RUNGS = 0.25, 0.30, 9
PLATFORM_Z = LADDER_FIRST + LADDER_STEP * LADDER_RUNGS          # 2.95: where the next rung would be
CABIN_SEAT_Z = PLATFORM_Z - 0.20                                  # the platform 0.20 over the cabin's floor level
PAD_R = 3.05
LADDER_AT = (-2.05, -0.68)     # the rung line's middle; the climber is on its -Y side, facing +Y
LADDER_HALF, RUNG_R = 0.225, 0.016

CHUTE_ATTACH = Vector((0.0, 0.0, Z_TOP + 0.05))
MASS_KG = 2400.0               # the cabin afloat (for the waterline)

CAPSULE_R, CAPSULE_HH = 0.34, 0.96      # CharacterStandard.h (checked against the header in main)

# metres per texture tile (the game's materials use the same: kit.json "tiles")
TILE = {"tx_sc_floor": 0.5, "tx_sc_paint_white": 2.0, "tx_sc_paint_green": 2.0, "tx_sc_metal": 1.0, "tx_sc_foil": 1.0,
        "tx_sc_quilt": 2 * QUILT, "tx_sc_shield": 1.0, "tx_sc_rubber": 0.5, "tx_sc_fabric": 0.5, "tx_sc_net": 0.25}

PALETTE = {     # colour (sRGB), roughness, metallic, emission: only what the previews show; the game has its own
    "tx_sc_paint_white": ((0.86, 0.86, 0.82), 0.5, 0.0, 0.0),
    "tx_sc_paint_green": ((0.50, 0.60, 0.54), 0.55, 0.0, 0.0),
    "tx_sc_metal": ((0.56, 0.57, 0.58), 0.38, 0.85, 0.0),
    "tx_sc_foil": ((0.90, 0.66, 0.20), 0.28, 1.0, 0.0),
    "tx_sc_quilt": ((0.84, 0.81, 0.70), 0.9, 0.0, 0.0),
    "tx_sc_shield": ((0.22, 0.16, 0.12), 0.85, 0.0, 0.0),
    "tx_sc_rubber": ((0.06, 0.06, 0.06), 0.8, 0.0, 0.0),
    "tx_sc_floor": ((0.36, 0.40, 0.40), 0.75, 0.0, 0.0),
    "tx_sc_fabric": ((0.40, 0.42, 0.30), 0.9, 0.0, 0.0),
    "tx_sc_net": ((0.75, 0.72, 0.62), 0.9, 0.0, 0.0),
    "SC_dark": ((0.03, 0.03, 0.03), 0.6, 0.0, 0.0),
    "SC_red": ((0.72, 0.06, 0.04), 0.35, 0.0, 0.0),
    "SC_yellow": ((0.88, 0.68, 0.08), 0.5, 0.0, 0.0),
    "SC_glass": ((0.9, 0.95, 0.95), 0.03, 0.0, 0.0),
    "SC_porthole": ((0.9, 0.95, 1.0), 0.02, 0.0, 0.0),
    "SC_lamp": ((1.0, 0.9, 0.7), 0.4, 0.0, 4.0),
    "SC_dials": ((0.05, 0.05, 0.05), 0.5, 0.0, 0.0),
    "SC_digits": ((0.9, 0.35, 0.05), 0.4, 0.0, 2.0),
    "SC_bar": ((0.9, 0.5, 0.1), 0.4, 0.0, 1.5),
    "SC_crt": ((0.1, 0.8, 0.4), 0.2, 0.0, 1.5),
    "SC_lamps": ((0.9, 0.7, 0.2), 0.4, 0.0, 1.0),
    "SC_labels": ((0.05, 0.05, 0.05), 0.5, 0.0, 0.0),
    "SC_globe": ((0.08, 0.25, 0.55), 0.35, 0.0, 0.0),
    "SC_ball": ((0.2, 0.4, 0.7), 0.4, 0.0, 0.0),
    "SC_needle": ((0.95, 0.93, 0.85), 0.5, 0.0, 0.0),
    "SC_case": ((0.55, 0.16, 0.12), 0.7, 0.0, 0.0),
    "SC_record": ((0.02, 0.02, 0.02), 0.25, 0.0, 0.0),
    "SC_record_label": ((0.75, 0.2, 0.15), 0.6, 0.0, 0.0),
    "SC_sleeve": ((0.75, 0.6, 0.4), 0.7, 0.0, 0.0),
    "SC_paper": ((0.85, 0.82, 0.72), 0.85, 0.0, 0.0),
    "SC_book": ((0.2, 0.3, 0.5), 0.7, 0.0, 0.0),
    "SC_blanket": ((0.6, 0.25, 0.2), 0.95, 0.0, 0.0),
    "SC_cap": ((0.22, 0.27, 0.36), 0.8, 0.0, 0.0),
    "SC_fire": ((1.0, 0.5, 0.1), 0.5, 0.0, 2.0),
    "SC_chute": ((0.95, 0.45, 0.1), 0.8, 0.0, 0.0),
    "SC_cord": ((0.85, 0.85, 0.8), 0.8, 0.0, 0.0),
}
SEE_THROUGH = {"SC_glass": 0.06, "SC_porthole": 0.05, "tx_sc_net": 0.3}


def srgb_to_lin(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def material(name):
    m = bpy.data.materials.get(name)
    if m:
        return m
    col, rough, metal, emit = PALETTE[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    lin = [srgb_to_lin(c) for c in col]
    b.inputs["Base Color"].default_value = (*lin, 1.0)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    if emit > 0:
        b.inputs["Emission Color" if "Emission Color" in b.inputs else "Emission"].default_value = (*lin, 1.0)
        if "Emission Strength" in b.inputs:
            b.inputs["Emission Strength"].default_value = emit
    if name in SEE_THROUGH:        # (alpha, not refraction: lamps and the sun then light what is behind the glass)
        b.inputs["Alpha"].default_value = SEE_THROUGH[name]
    return m


# --- the hull's shape -------------------------------------------------------------------------------------------------
def r_in(z):
    """The inner wall's radius at a height (the seams' surface: pillows stand PILLOW in from it)."""
    if z <= Z_KNEE:
        return R_IN0 + (R_IN1 - R_IN0) * max(z, 0.0) / Z_KNEE
    t = min((z - Z_KNEE) / (CEIL - Z_KNEE), 1.0)
    return R_IN1 * max(1.0 - t ** DOME_P_IN, 0.0) ** (1.0 / DOME_P_IN)


def z_ceil(r):
    """The inner dome's height over a point r from the axis."""
    if r >= R_IN1:
        return Z_KNEE
    return Z_KNEE + (CEIL - Z_KNEE) * (1.0 - (r / R_IN1) ** DOME_P_IN) ** (1.0 / DOME_P_IN)


def r_out(z):
    if z <= Z_RIM0:
        h = max(z - Z_APEX, 0.0)
        return math.sqrt(max(2 * SHIELD_RS * h - h * h, 0.0))
    if z <= Z_RIM1:
        return R_OUT0
    if z <= Z_OKNEE:
        return R_OUT0 + (R_OUT1 - R_OUT0) * (z - Z_RIM1) / (Z_OKNEE - Z_RIM1)
    t = min((z - Z_OKNEE) / (Z_TOP - Z_OKNEE), 1.0)
    return R_OUT1 * max(1.0 - t ** DOME_P_OUT, 0.0) ** (1.0 / DOME_P_OUT)


def frame(o, x=None, y=None, z=None):
    """A 4x4 from an origin and two of its axes (the third is their cross product; the second given is squared up)."""
    if x is not None and z is not None:
        x = Vector(x).normalized()
        y = Vector(z).cross(x).normalized()
        z = x.cross(y)
    elif x is not None and y is not None:
        x = Vector(x).normalized()
        z = x.cross(Vector(y)).normalized()
        y = z.cross(x)
    else:
        z = Vector(z).normalized()
        x = Vector(y).cross(z).normalized()
        y = z.cross(x)
    m = Matrix.Identity(4)
    for i, a in enumerate((x, y, z, Vector(o))):
        m[0][i], m[1][i], m[2][i] = a.x, a.y, a.z
    return m


def polar(r, az, z=0.0):
    return Vector((r * math.cos(az), r * math.sin(az), z))


def wall_in(az, z, off=0.0):
    """A frame on the inner wall: X = into the cabin, square to the wall; Z = up the wall; Y = along it (level)."""
    n = Vector((-math.cos(az) * math.cos(S_IN), -math.sin(az) * math.cos(S_IN), -math.sin(S_IN)))
    up = Vector((-math.cos(az) * math.sin(S_IN), -math.sin(az) * math.sin(S_IN), math.cos(S_IN)))
    return frame(polar(r_in(z), az, z) + n * off, x=n, z=up)


def wall_out(az, z, off=0.0):
    """A frame on the outer cone: X = out of the hull, square to it; Z = up the cone; Y = along it."""
    n = Vector((math.cos(az) * math.cos(S_OUT), math.sin(az) * math.cos(S_OUT), math.sin(S_OUT)))
    up = Vector((-math.cos(az) * math.sin(S_OUT), -math.sin(az) * math.sin(S_OUT), math.cos(S_OUT)))
    return frame(polar(r_out(z), az, z) + n * off, x=n, z=up)


def dome_in(x, y, off=0.0):
    """A frame on the inner dome over (x, y): X = into the cabin (down), square to the dome; Z = toward +X of the
    cabin along the dome."""
    def P(a, b):
        return Vector((a, b, z_ceil(math.hypot(a, b))))
    e = 1e-3
    tx = (P(x + e, y) - P(x - e, y)).normalized()
    ty = (P(x, y + e) - P(x, y - e)).normalized()
    n = ty.cross(tx).normalized()           # down
    return frame(P(x, y) + n * off, x=n, z=tx)


def axis_frame(az, z):
    """An opening's frame: origin where its axis leaves the outer skin, Z = the axis outward, X = level."""
    w = wall_out(az, z)
    return frame(w.translation, x=w.col[1].xyz, z=w.col[0].xyz)


PANEL_U = Vector((0, -1, 0))
PANEL_V = Vector((math.sin(PANEL_TILT), 0, math.cos(PANEL_TILT)))
PM = frame(PANEL_O, x=PANEL_U, y=PANEL_V)      # panel coordinates (u, v, n) -> cabin
PANEL_N = PM.col[2].xyz.copy()


def rot_about(M, axis, deg, pivot=(0, 0, 0)):
    """M with a turn of deg about one of its own axes ('X', 'Y', 'Z') through a point of its own frame."""
    p = Vector(pivot)
    return M @ Matrix.Translation(p) @ Matrix.Rotation(math.radians(deg), 4, axis) @ Matrix.Translation(-p)


# --- a small mesh builder: faces per material into one bmesh (metre UVs unless given) ---------------------------------
def metre_uv(pts):
    n = Vector((0, 0, 0))
    for i in range(len(pts)):
        a, b = pts[i], pts[(i + 1) % len(pts)]
        n += Vector(((a.y - b.y) * (a.z + b.z), (a.z - b.z) * (a.x + b.x), (a.x - b.x) * (a.y + b.y)))
    ax = max(range(3), key=lambda k: abs(n[k]))
    if ax == 2:
        return [(p.x, p.y) for p in pts]
    if ax == 1:
        return [(p.x, p.z) for p in pts]
    return [(p.y, p.z) for p in pts]


class Builder:
    """Faces go in through the current frame (b.at(M)); metre UVs are taken in that frame, before it is applied.
    UVMap is the texture's; UVData is a second set the game's materials read a per-cell value by (lamps, digits)."""

    def __init__(self, mats=()):
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.uv2 = self.bm.loops.layers.uv.new("UVData")
        self.mats = list(mats)
        self.M = Matrix.Identity(4)

    @contextlib.contextmanager
    def at(self, M):
        old = self.M
        self.M = old @ M
        try:
            yield
        finally:
            self.M = old

    def mi(self, mat):
        if mat not in self.mats:
            self.mats.append(mat)
        return self.mats.index(mat)

    def face(self, pts, mat, uv=None, uv2=None):
        loc = [Vector(p) for p in pts]
        if uv is None:
            uv = metre_uv(loc)
        vs = [self.bm.verts.new(self.M @ p) for p in loc]
        try:
            f = self.bm.faces.new(vs)
        except ValueError:
            return None
        f.material_index = self.mi(mat)
        for lp, t in zip(f.loops, uv):
            lp[self.uv].uv = t
            lp[self.uv2].uv = uv2 or (0.0, 0.0)
        return f

    def hull(self, mat, pts):
        """The convex hull of some points, flat faces merged."""
        tb = bmesh.new()
        vs = [tb.verts.new(Vector(p)) for p in pts]
        r = bmesh.ops.convex_hull(tb, input=vs)
        junk = [g for g in r["geom_interior"] + r["geom_unused"] if isinstance(g, bmesh.types.BMVert)]
        if junk:
            bmesh.ops.delete(tb, geom=junk, context="VERTS")
        bmesh.ops.dissolve_limit(tb, angle_limit=math.radians(0.5), verts=tb.verts[:], edges=tb.edges[:])
        tb.normal_update()
        cen = sum((v.co for v in tb.verts), Vector()) / len(tb.verts)
        for f in tb.faces:
            p = [v.co.copy() for v in f.verts]
            if f.normal.dot(f.calc_center_median() - cen) < 0:
                p.reverse()
            self.face(p, mat)
        tb.free()

    def box(self, mat, c, s, rot=None, bevel=0.0, skip=()):
        """A box round c; bevel chamfers every edge; skip: faces to leave out ('-z', '+x' ...)."""
        R = (rot or Matrix.Identity(3)).to_4x4()
        R.translation = Vector(c)
        hx, hy, hz = s[0] / 2, s[1] / 2, s[2] / 2
        with self.at(R):
            if bevel > 0:
                b = min(bevel, hx * 0.98, hy * 0.98, hz * 0.98)
                pts = []
                for sx in (-1, 1):
                    for sy in (-1, 1):
                        for sz in (-1, 1):
                            pts += [(sx * (hx - b), sy * (hy - b), sz * hz), (sx * (hx - b), sy * hy, sz * (hz - b)),
                                    (sx * hx, sy * (hy - b), sz * (hz - b))]
                self.hull(mat, pts)
                return
            F = {"-z": [(-hx, -hy, -hz), (-hx, hy, -hz), (hx, hy, -hz), (hx, -hy, -hz)],
                 "+z": [(-hx, -hy, hz), (hx, -hy, hz), (hx, hy, hz), (-hx, hy, hz)],
                 "-y": [(-hx, -hy, -hz), (hx, -hy, -hz), (hx, -hy, hz), (-hx, -hy, hz)],
                 "+y": [(hx, hy, -hz), (-hx, hy, -hz), (-hx, hy, hz), (hx, hy, hz)],
                 "-x": [(-hx, hy, -hz), (-hx, -hy, -hz), (-hx, -hy, hz), (-hx, hy, hz)],
                 "+x": [(hx, -hy, -hz), (hx, hy, -hz), (hx, hy, hz), (hx, -hy, hz)]}
            for k, pts in F.items():
                if k not in skip:
                    self.face(pts, mat)

    def quad(self, mat, c, w, h, uv=None, uv2=None):
        """A rectangle in the frame's XY plane round c facing +Z; uv = (u0, v0, u1, v1) (v0 its bottom) or metres."""
        x, y, z = c
        pts = [(x - w / 2, y - h / 2, z), (x + w / 2, y - h / 2, z), (x + w / 2, y + h / 2, z), (x - w / 2, y + h / 2, z)]
        t = None
        if uv:
            u0, v0, u1, v1 = uv
            t = [(u0, v0), (u1, v0), (u1, v1), (u0, v1)]
        return self.face(pts, mat, t, uv2)

    def disc(self, mat, c, r, n=32, up=True, uv=None, r0=0.0):
        """A disc (or a ring from r0) in the frame's XY plane at c; uv None = metres, else (u0, v0, u1, v1) the
        square the circle of radius r is inscribed in."""
        x, y, z = c
        def P(rr, i):
            a = TAU * i / n
            return Vector((x + rr * math.cos(a), y + rr * math.sin(a), z))
        def T(p):
            if not uv:
                return (p.x, p.y)
            u0, v0, u1, v1 = uv
            return (u0 + (u1 - u0) * ((p.x - x) / (2 * r) + 0.5), v0 + (v1 - v0) * ((p.y - y) / (2 * r) + 0.5))
        if r0 <= 0:
            pts = [P(r, i) for i in range(n)]
            if not up:
                pts = pts[::-1]
            return self.face(pts, mat, [T(p) for p in pts])
        for i in range(n):
            q = [P(r0, i), P(r, i), P(r, i + 1), P(r0, i + 1)]
            if not up:
                q = q[::-1]
            self.face(q, mat, [T(p) for p in q])

    def tube(self, mat, c, r_in_, r_out_, z0, z1, n=32, inner=True, outer=True, top=None, bottom=None):
        """A pipe along the frame's Z from z0 to z1 round c: inner face, outer face, end rings (a material or None)."""
        cx, cy = c
        def ring(r, z):
            return [Vector((cx + r * math.cos(TAU * i / n), cy + r * math.sin(TAU * i / n), z)) for i in range(n + 1)]
        for r, face_in, on in ((r_in_, True, inner), (r_out_, False, outer)):
            if not on or r <= 0:
                continue
            a, b = ring(r, z0), ring(r, z1)
            arc = TAU * r / n
            for i in range(n):
                q = [a[i], a[i + 1], b[i + 1], b[i]]
                uv = [(i * arc, z0), ((i + 1) * arc, z0), ((i + 1) * arc, z1), (i * arc, z1)]
                if face_in:
                    q, uv = q[::-1], uv[::-1]
                self.face(q, mat, uv)
        for z, m, up in ((z1, top, True), (z0, bottom, False)):
            if not m:
                continue
            if r_in_ <= 0:
                self.disc(m, (cx, cy, z), r_out_, n=n, up=up)
                continue
            a, b = ring(r_in_, z), ring(r_out_, z)
            for i in range(n):
                q = [a[i], b[i], b[i + 1], a[i + 1]]
                self.face(q if up else q[::-1], m)

    def cyl(self, mat, a, b, r, n=12, caps=True, r2=None, cap_mat=None):
        """A rod from a to b (r at a, r2 at b)."""
        a, b = Vector(a), Vector(b)
        ax = b - a
        ln = ax.length
        z = ax / ln
        x = z.orthogonal().normalized()
        y = z.cross(x)
        r2 = r if r2 is None else r2
        ra, rb = [], []
        for i in range(n + 1):
            t = TAU * i / n
            d = x * math.cos(t) + y * math.sin(t)
            ra.append(a + d * r)
            rb.append(b + d * r2)
        arc = TAU * max(r, r2) / n
        for i in range(n):
            self.face([ra[i], ra[i + 1], rb[i + 1], rb[i]], mat, [(i * arc, 0), ((i + 1) * arc, 0), ((i + 1) * arc, ln), (i * arc, ln)])
        if caps:
            if r > 1e-6:
                self.face(list(reversed(ra[:-1])), cap_mat or mat)
            if r2 > 1e-6:
                self.face(rb[:-1], cap_mat or mat)

    def torus(self, mat, c, normal, r, t, n=24, m=8, a0=0.0, a1=TAU):
        c, nz = Vector(c), Vector(normal).normalized()
        x = nz.orthogonal().normalized()
        y = nz.cross(x)
        rings = []
        for i in range(n + 1):
            a = a0 + (a1 - a0) * i / n
            d = x * math.cos(a) + y * math.sin(a)
            rings.append([c + d * (r + t * math.cos(TAU * k / m)) + nz * t * math.sin(TAU * k / m) for k in range(m + 1)])
        su, sv = (a1 - a0) * r / n, TAU * t / m
        for i in range(n):
            for k in range(m):
                q = [rings[i][k], rings[i + 1][k], rings[i + 1][k + 1], rings[i][k + 1]]
                self.face(q, mat, [(i * su, k * sv), ((i + 1) * su, k * sv), ((i + 1) * su, (k + 1) * sv), (i * su, (k + 1) * sv)])

    def pipe(self, mat, pts, r, n=8, caps=True, closed=False):
        """A tube along a polyline (its rings bisect the corners)."""
        pts = [Vector(p) for p in pts]
        N = len(pts)
        seg = [(pts[(i + 1) % N] - pts[i]).normalized() for i in range(N if closed else N - 1)]
        x = seg[0].orthogonal().normalized()
        y = seg[0].cross(x)
        C = [(x * math.cos(TAU * k / n) + y * math.sin(TAU * k / n)) * r for k in range(n + 1)]     # round the first run
        def mitre(C_, a, t):
            return [c - a * (c.dot(t) / a.dot(t)) for c in C_]      # slid along the run onto the corner's bisecting plane
        rings = [None] * N
        if closed:
            rings[0] = [pts[0] + o for o in mitre(C, seg[0], (seg[-1] + seg[0]).normalized())]
        else:
            rings[0] = [pts[0] + c for c in C]
        for i in range(1, N):
            a = seg[i - 1]
            if i == N - 1 and not closed:
                rings[i] = [pts[i] + c for c in C]
                break
            b_ = seg[i]
            off = mitre(C, a, (a + b_).normalized())
            rings[i] = [pts[i] + o for o in off]
            C = [o - b_ * o.dot(b_) for o in off]
        arc, s = TAU * r / n, 0.0
        for i in range(N if closed else N - 1):
            ra, rb = rings[i], rings[(i + 1) % N]
            ln = (pts[(i + 1) % N] - pts[i]).length
            for k in range(n):
                self.face([ra[k], ra[k + 1], rb[k + 1], rb[k]], mat, [(k * arc, s), ((k + 1) * arc, s), ((k + 1) * arc, s + ln), (k * arc, s + ln)])
            s += ln
        if caps and not closed:
            self.face(list(reversed(rings[0][:-1])), mat)
            self.face(rings[-1][:-1], mat)

    def rev(self, mat, prof, n=32, inward=False, a0=0.0, a1=TAU, uv_ref=None):
        """A surface of revolution round the frame's Z: prof = [(r, z) ...] from the bottom up for outward faces;
        mat: one name or one per segment. UVs: metres round (at the band's middle radius, or uv_ref) x metres along."""
        s = 0.0
        for k in range(len(prof) - 1):
            (ra, za), (rb, zb) = prof[k], prof[k + 1]
            ln = math.hypot(rb - ra, zb - za)
            if ln < 1e-9:
                continue
            m = mat if isinstance(mat, str) else mat[k]
            rm = uv_ref or max((ra + rb) / 2, 1e-4)
            for i in range(n):
                t0, t1 = a0 + (a1 - a0) * i / n, a0 + (a1 - a0) * (i + 1) / n
                A0, A1 = polar(ra, t0, za), polar(ra, t1, za)
                B0, B1 = polar(rb, t0, zb), polar(rb, t1, zb)
                u0, u1 = t0 * rm, t1 * rm
                if ra < 1e-9:
                    q, uv = [A0, B1, B0], [((u0 + u1) / 2, s), (u1, s + ln), (u0, s + ln)]
                elif rb < 1e-9:
                    q, uv = [A0, A1, B0], [(u0, s), (u1, s), ((u0 + u1) / 2, s + ln)]
                else:
                    q, uv = [A0, A1, B1, B0], [(u0, s), (u1, s), (u1, s + ln), (u0, s + ln)]
                if inward:
                    q, uv = q[::-1], uv[::-1]
                self.face(q, m, uv)
            s += ln

    def sphere(self, mat, c, r, nu=24, nv=12, equirect=False, squash=(1, 1, 1)):
        """A UV sphere; equirect: u = (lon + 180) / 360 with lon 0 toward +X, v = latitude up; else metres."""
        c = Vector(c)
        for j in range(nv):
            la0, la1 = -PI / 2 + PI * j / nv, -PI / 2 + PI * (j + 1) / nv
            for i in range(nu):
                lo0, lo1 = -PI + TAU * i / nu, -PI + TAU * (i + 1) / nu
                def P(lo, la):
                    return c + Vector((r * squash[0] * math.cos(la) * math.cos(lo), r * squash[1] * math.cos(la) * math.sin(lo), r * squash[2] * math.sin(la)))
                if equirect:
                    def T(i_, j_):
                        return (i_ / nu, j_ / nv)
                else:
                    k = r * max(math.cos((la0 + la1) / 2), 0.05)
                    def T(i_, j_, k=k):
                        return ((-PI + TAU * i_ / nu) * k, (-PI / 2 + PI * j_ / nv) * r)
                q = [P(lo0, la0), P(lo1, la0), P(lo1, la1), P(lo0, la1)]
                uv = [T(i, j), T(i + 1, j), T(i + 1, j + 1), T(i, j + 1)]
                if j == 0:
                    q, uv = [q[0], q[2], q[3]], [(T(i, j)[0] / 2 + T(i + 1, j)[0] / 2, uv[0][1]), uv[2], uv[3]]
                elif j == nv - 1:
                    q, uv = [q[0], q[1], q[2]], [uv[0], uv[1], (uv[2][0] / 2 + uv[3][0] / 2, uv[2][1])]
                self.face(q, mat, uv)

    def to_object(self, name, smooth=45, weld=True):
        me = bpy.data.meshes.new(name)
        if weld:
            bmesh.ops.remove_doubles(self.bm, verts=self.bm.verts, dist=1e-5)
        self.bm.to_mesh(me)
        self.bm.free()
        for m in self.mats:
            me.materials.append(material(m))
        o = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(o)
        shade(o, smooth)
        return o


def shade(o, angle=45):
    """Smooth faces, sharp edges where the faces meet at more than the angle (None: flat)."""
    me = o.data
    for p in me.polygons:
        p.use_smooth = angle is not None
    if angle is not None:
        if hasattr(me, "set_sharp_from_angle"):
            me.set_sharp_from_angle(angle=math.radians(angle))
        else:                                   # before 4.1
            me.use_auto_smooth = True
            me.auto_smooth_angle = math.radians(angle)


def apply_mods(o):
    bpy.context.view_layer.objects.active = o
    for m in list(o.modifiers):
        bpy.ops.object.modifier_apply(modifier=m.name)


def boolean_cut(o, cutter):
    """o minus cutter (exact); the cut's faces keep the cutter's material slot (the same slot lists on both)."""
    m = o.modifiers.new("cut", "BOOLEAN")
    m.operation = "DIFFERENCE"
    m.solver = "EXACT"
    m.object = cutter
    if hasattr(m, "material_mode"):
        m.material_mode = "INDEX"
    apply_mods(o)
    bpy.data.objects.remove(cutter, do_unlink=True)


def join(objs, name):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    if len(objs) > 1:
        bpy.ops.object.join()
    bpy.ops.object.select_all(action="DESELECT")
    objs[0].name = name
    objs[0].data.name = name
    return objs[0]


def keep_faces(o, name, keep):
    """A copy of o with only the faces whose material name passes keep()."""
    c = o.copy()
    c.data = o.data.copy()
    c.name = c.data.name = name
    bpy.context.scene.collection.objects.link(c)
    names = [m.name for m in c.data.materials]
    bm = bmesh.new()
    bm.from_mesh(c.data)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if not keep(names[f.material_index])], context="FACES")
    bm.to_mesh(c.data)
    bm.free()
    return c


def prune_slots(o):
    """Drop material slots no face uses."""
    me = o.data
    used = sorted({p.material_index for p in me.polygons})
    mats = [me.materials[i] for i in used]
    remap = {old: new for new, old in enumerate(used)}
    idx = [remap[p.material_index] for p in me.polygons]
    me.materials.clear()
    for m in mats:
        me.materials.append(m)
    for p, i in zip(me.polygons, idx):
        p.material_index = i


def metre_uvs(o, only=None):
    me = o.data
    uv = me.uv_layers["UVMap"]
    for p in me.polygons:
        if only and me.materials[p.material_index].name not in only:
            continue
        pts = [me.vertices[me.loops[i].vertex_index].co.copy() for i in p.loop_indices]
        for i, t in zip(p.loop_indices, metre_uv(pts)):
            uv.data[i].uv = t


def tris(o):
    return sum(len(p.vertices) - 2 for p in o.data.polygons)


# --- the label atlas: plate ids, their rects on a 1024 atlas and their printed size ------------------------------------
LABEL_CLASSES = {"lamp": (164, 52), "inst": (228, 44), "sw": (164, 44), "mark": (100, 40), "wide": (228, 44),
                 "name": (500, 110), "data": (500, 170)}
LABEL_PX_PER_CM = {"lamp": 32.8}     # every other class prints at 41 px/cm
LAMP_IDS = ["power", "air", "gyro", "hatch", "armed", "engine", "sep", "zero_g", "burn", "comm", "blackout", "g_high",
            "hull_hot", "drogue", "main_chute", "alt_low", "jets", "splash", "beacon", "o2_low", "co2_high", "batt_low",
            "master_caution", "record"]
INST_IDS = ["globe", "clock", "range", "speed", "alt", "gmeter", "horizon", "press", "o2", "co2", "temp", "volts",
            "fuel", "hull", "crt", "lamps"]
SW_IDS = ["power", "air", "gyro", "seal", "arm", "launch", "light", "crt", "radio"]
TOGGLES = ["power", "air", "gyro", "seal", "arm", "light", "crt", "radio"]      # the 8 levers, left to right


def label_plates():
    """[(id, class)] in atlas order: tools/spacecraft/labels.json when it is there (its order is the lamps' order),
    else the same list from here."""
    if os.path.exists(LABELS_JSON):
        with open(LABELS_JSON, encoding="utf-8") as f:
            d = json.load(f)
        for k, v in d.get("classes", {}).items():
            LABEL_CLASSES[k] = (v["w"], v["h"])
        return [(k, v["class"]) for k, v in d["plates"].items()], d.get("gutter", 6)
    out = [("lamp_" + k, "lamp") for k in LAMP_IDS] + [("inst_" + k, "inst") for k in INST_IDS]
    out += [("sw_" + k, "sw") for k in SW_IDS] + [("mark_" + k, "mark") for k in ("on", "off", "open", "shut")]
    out += [("mark_chute", "wide"), ("mark_manual", "wide")]
    out += [("unit_" + k, "mark") for k in ("km", "kms", "ms", "g", "degc", "volt", "pct", "mmhg")]
    return out + [("nameplate", "name"), ("dataplate", "data")], 6


class Labels:
    """Shelf-packs the plates into a 1024 atlas; rect = [x, y, w, h] px from the top left."""
    SIZE = 1024

    def __init__(self):
        plates, gutter = label_plates()
        self.order = [p[0] for p in plates]
        self.cls = dict(plates)
        self.rect, self.used = {}, {}
        x = y = gutter
        row_h = 0
        for pid, c in sorted(plates, key=lambda p: (-LABEL_CLASSES[p[1]][1], -LABEL_CLASSES[p[1]][0], self.order.index(p[0]))):
            w, h = LABEL_CLASSES[c]
            if x + w + gutter > self.SIZE:
                x, y, row_h = gutter, y + row_h + gutter, 0
            if y + h + gutter > self.SIZE:
                raise SystemExit("SCKIT: the label plates do not fit a %d atlas" % self.SIZE)
            self.rect[pid] = [x, y, w, h]
            x += w + gutter
            row_h = max(row_h, h)
        self.lamps = [p for p in self.order if p.startswith("lamp_")]

    def size_m(self, pid):
        c = self.cls[pid]
        w, h = LABEL_CLASSES[c]
        k = LABEL_PX_PER_CM.get(c, 41.0) * 100
        return w / k, h / k

    def uv(self, pid):
        x, y, w, h = self.rect[pid]
        s = float(self.SIZE)
        return (x / s, 1 - (y + h) / s, (x + w) / s, 1 - y / s)

    def plate(self, b, pid, c, where, mat="SC_labels", uv2=None, rim=True):
        """A printed plate round c in the builder's frame's XY plane, standing 2.5 mm over it (its rim in SC_dark)."""
        w, h = self.size_m(pid)
        x, y, z = c
        if rim:
            b.box("SC_dark", (x, y, z - 0.00125), (w, h, 0.0025), skip=("+z", "-z"))
        b.quad(mat, (x, y, z), w, h, self.uv(pid), uv2)
        self.used.setdefault(pid, []).append({"piece": where, "frame_matrix": b.M.copy(), "c": Vector(c)})


LAB = None      # made in main


# --- the hull: one solid (outer skin, inner skin), the openings cut through it, then split into outside and inside ------
HULL_MATS = ["tx_sc_shield", "tx_sc_paint_white", "tx_sc_floor", "tx_sc_paint_green", "tx_sc_quilt", "tx_sc_metal"]
BIG_FLARE, SIDE_FLARE = math.tan(math.radians(16)), math.tan(math.radians(14))   # the portholes' reveals open inward
FLARE_T = -0.095              # where a reveal starts to open, along the axis (just inside the pane)
OPENINGS = [("hatch", HATCH_AZ, HATCH_Z, HATCH_R, 64, 0.0), ("big", BIG_AZ, BIG_Z, BIG_R, 48, BIG_FLARE),
            ("side_l", SIDE_AZ, SIDE_Z, SIDE_R, 32, SIDE_FLARE), ("side_r", -SIDE_AZ, SIDE_Z, SIDE_R, 32, SIDE_FLARE)]
BIG_W = 0.05        # the big porthole's collar's width
LINER = 0.004       # an opening's steel liner: the cut is this much wider than the clear radius


def axis_depth(az, z):
    """How far along an opening's axis the inner wall is under the outer skin."""
    A = axis_frame(az, z)
    t = 0.0
    while t < 1.0:
        p = A @ Vector((0, 0, -t))
        if math.hypot(p.x, p.y) <= r_in(p.z):
            return t
        t += 0.001
    return t


def hull():
    b = Builder(HULL_MATS)
    NO, NI = 64, 48
    # outside: the shield's cap and rim, the cone, the dome
    prof, mats = [], []
    for k in range(9):
        r = R_OUT0 * k / 8
        prof.append((r, Z_APEX + SHIELD_RS - math.sqrt(SHIELD_RS ** 2 - r * r)))
    mats += ["tx_sc_shield"] * 8
    prof.append((R_OUT0, Z_RIM1))
    mats.append("tx_sc_shield")
    for k in range(1, 5):
        prof.append((R_OUT0 + (R_OUT1 - R_OUT0) * k / 4, Z_RIM1 + (Z_OKNEE - Z_RIM1) * k / 4))
        mats.append("tx_sc_paint_white")
    for k in range(1, 11):
        t = PI / 2 * k / 10
        prof.append((R_OUT1 * math.cos(t) ** (2 / DOME_P_OUT) if k < 10 else 0.0, Z_OKNEE + (Z_TOP - Z_OKNEE) * math.sin(t) ** (2 / DOME_P_OUT)))
        mats.append("tx_sc_paint_white")
    b.rev(mats, prof, n=NO)
    # inside: the floor, the kick band, the quilt (pillows: their middles stand in from the seams), the dome
    b.rev("tx_sc_floor", [(0, 0), (0.45, 0), (0.9, 0), (R_IN0, 0)], n=NI, inward=True)
    b.rev("tx_sc_paint_green", [(R_IN0, 0), (r_in(KICK), KICK)], n=NI, inward=True)
    rows = QUILT_ROWS * 2
    def P(i, j):
        z = KICK + (Z_KNEE - KICK) * j / rows
        r = r_in(z) - (PILLOW if (i % 2 and j % 2) else 0.0)
        return polar(r, TAU * i / NI, z)
    for j in range(rows):
        for i in range(NI):
            b.face([P(i, j), P(i, j + 1), P(i + 1, j + 1), P(i + 1, j)], "tx_sc_quilt")
    prof = [(R_IN1, Z_KNEE)]
    for k in range(1, 11):
        t = PI / 2 * k / 10
        prof.append((R_IN1 * math.cos(t) ** (2 / DOME_P_IN) if k < 10 else 0.0, Z_KNEE + (CEIL - Z_KNEE) * math.sin(t) ** (2 / DOME_P_IN)))
    b.rev("tx_sc_paint_green", prof, n=NI, inward=True)
    o = b.to_object("hull", smooth=None)
    c = Builder(HULL_MATS)
    for _, az, z, r, n, fl in OPENINGS:
        with c.at(axis_frame(az, z)):
            c.cyl("tx_sc_metal", (0, 0, -0.62 if not fl else FLARE_T - 0.02), (0, 0, 0.45), r + LINER, n=n)
    boolean_cut(o, c.to_object("cut", smooth=None))
    c = Builder(HULL_MATS)
    for _, az, z, r, n, fl in OPENINGS:
        if fl:
            with c.at(axis_frame(az, z)):
                c.cyl("tx_sc_metal", (0, 0, -0.62), (0, 0, FLARE_T), r + LINER + fl * (FLARE_T + 0.62), n=n, r2=r + LINER)
    boolean_cut(o, c.to_object("cut", smooth=None))
    hull_uvs(o)
    outer = keep_faces(o, "outer", lambda m: m in ("tx_sc_shield", "tx_sc_paint_white"))
    inner = keep_faces(o, "inner", lambda m: m not in ("tx_sc_shield", "tx_sc_paint_white"))
    bpy.data.objects.remove(o, do_unlink=True)
    return outer, inner


def hull_uvs(o):
    """The cut hull's UVs, metres: flat-ish faces from above, steep ones unrolled round the axis (the quilt at one
    radius and from its own foot, so its pillows are QUILT square and meet round the back with no seam)."""
    me = o.data
    uv = me.uv_layers["UVMap"]
    for p in me.polygons:
        mat = me.materials[p.material_index].name
        pts = [me.vertices[me.loops[i].vertex_index].co.copy() for i in p.loop_indices]
        n, c = p.normal, p.center
        if mat == "tx_sc_metal":
            t = metre_uv(pts)
        elif abs(n.z) > 0.75:
            t = [(q.x, q.y) for q in pts]
        else:
            ac = math.atan2(c.y, c.x)
            if mat == "tx_sc_quilt":
                rr, k, z0 = QUILT_RREF, 1.0 / math.cos(S_IN), KICK
            else:
                rr, k, z0 = math.hypot(c.x, c.y), 1.0 / max(math.sqrt(1 - n.z * n.z), 0.3), 0.0
            t = []
            for q in pts:
                a = math.atan2(q.y, q.x) - ac
                a = (a + PI) % TAU - PI
                t.append(((ac + a) * rr, (q.z - z0) * k))
        for i, q in zip(p.loop_indices, t):
            uv.data[i].uv = q


def collar_end(az, z, r, w, flare):
    """Where a porthole's inner collar ends along its axis, and the reveal's radius there."""
    T = axis_depth(az, z)
    ri = r_in(z)
    t_in, re = -T, r
    for _ in range(4):
        re = r + flare * max(FLARE_T - t_in, 0.0)
        t_in = -(T + ri - math.sqrt(ri * ri - (re + w) ** 2) + 0.02)
    return t_in, re, T


def collar(b, az, z, r, w, t_out, inner_collar, n, bolts=12, flare=0.0):
    """An opening's steel liner and collars: a sleeve of clear radius r through the wall, standing t_out proud of the
    outer skin's nearest point, and (inner_collar) proud of the inner wall's too, its reveal opening inward by
    `flare` (a slope) from FLARE_T. Returns the inner end's depth."""
    A = axis_frame(az, z)
    ro = r_out(z)
    sag_o = ro - math.sqrt(ro * ro - (r + w) ** 2)
    t_in, re, T = collar_end(az, z, r, w, flare)
    if not inner_collar:
        t_in, re = -(T * 0.8), r
    with b.at(A):
        tb = FLARE_T if flare else t_in
        b.tube("tx_sc_metal", (0, 0), r, r + w, tb, t_out, n=n, outer=False, top="tx_sc_metal", bottom=None if flare else "tx_sc_metal")
        b.tube("tx_sc_metal", (0, 0), r, r + w, -sag_o - 0.03, t_out, n=n, inner=False)
        if flare:
            b.rev("tx_sc_metal", [(re, t_in), (r, tb)], n=n, inward=True)
            b.disc("tx_sc_metal", (0, 0, t_in), re + w, n=n, up=False, r0=re)
            for t in (-GLASS_DEPTH + 0.014, -GLASS_DEPTH - 0.014):
                b.torus("tx_sc_rubber", (0, 0, t), (0, 0, 1), r - 0.001, 0.006, n=n, m=6)
        if inner_collar:
            b.tube("tx_sc_metal", (0, 0), re, re + w, t_in, -T + 0.03, n=n, inner=False)
        b.torus("tx_sc_metal", (0, 0, t_out - 0.012), (0, 0, 1), r + w + 0.004, 0.008, n=n, m=6)
        for k in range(bolts):
            a = TAU * (k + 0.5) / bolts
            p = Vector(((r + w / 2) * math.cos(a), (r + w / 2) * math.sin(a), t_out))
            b.cyl("tx_sc_metal", p, p + Vector((0, 0, 0.006)), 0.009, n=6)
            if inner_collar:
                p = Vector(((re + w / 2) * math.cos(a), (re + w / 2) * math.sin(a), t_in))
                b.cyl("tx_sc_metal", p, p - Vector((0, 0, 0.005)), 0.007, n=6)
    return t_in


def hatch_frame():
    """The door's frame, shut: origin on the hinge axis, X = out of the hull along the hatch's axis, Z = the hinge axis
    (up the hull), Y = toward the door's middle."""
    w = wall_out(HATCH_AZ, HATCH_Z)
    d, yw = w.col[0].xyz, w.col[1].xyz
    return frame(w.translation + d * (HATCH_T_OUT + DOOR_T / 2) - yw * HINGE_OFF, x=d, z=w.col[2].xyz)


def cabin_outer(skin):
    d = Builder()
    # the shield's rim ring and a belt where the cone starts
    d.tube("tx_sc_metal", (0, 0), R_OUT0 - 0.02, R_OUT0 + 0.012, Z_RIM1 - 0.035, Z_RIM1 + 0.02, n=64, inner=False,
           top="tx_sc_metal", bottom="tx_sc_metal")
    # the openings' collars; the hatch's has the hinge lugs
    collar(d, BIG_AZ, BIG_Z, BIG_R, BIG_W, 0.025, True, 48, flare=BIG_FLARE)
    for s in (1, -1):
        collar(d, s * SIDE_AZ, SIDE_Z, SIDE_R, 0.045, 0.022, True, 32, bolts=8, flare=SIDE_FLARE)
    collar(d, HATCH_AZ, HATCH_Z, HATCH_R, 0.07, HATCH_T_OUT, False, 64, bolts=0)
    H = hatch_frame()
    with d.at(H):
        for s in (-1, 1):
            d.hull("tx_sc_metal", [(x, y, s * 0.335 + e) for e in (-0.025, 0.025)
                                   for x, y in ((0.02, -0.07), (0.02, 0.03), (-0.04, 0.09), (-0.10, 0.09), (-0.10, -0.03), (-0.04, -0.07))])
        # a latch keeper opposite the hinge and a grab rail above the hatch
        d.box("tx_sc_metal", (-0.05, 2 * HINGE_OFF - 0.045, 0.0), (0.06, 0.05, 0.12), bevel=0.006)
        d.pipe("tx_sc_metal", [(-0.14, 0.50, 0.76), (0.0, 0.50, 0.76), (0.0, 1.02, 0.76), (-0.14, 1.02, 0.76)], 0.014, n=8)
    # thrusters: six small bells round the cone's foot, on fairings
    for az in (20, 100, 140, 220, 260, 340):
        W = wall_out(math.radians(az), 0.16)
        with d.at(frame(W.translation, x=W.col[1].xyz, z=W.col[0].xyz)):
            d.box("tx_sc_paint_white", (0, 0, 0.0), (0.16, 0.20, 0.05), bevel=0.012)
            d.rev("tx_sc_metal", [(0.026, 0.02), (0.05, 0.085)], n=12)
            d.rev("SC_dark", [(0.022, 0.03), (0.046, 0.085)], n=12, inward=True)
            d.disc("SC_dark", (0, 0, 0.03), 0.022, n=12)
            d.tube("tx_sc_metal", (0, 0), 0.046, 0.05, 0.084, 0.085, n=12, inner=False, outer=False, top="tx_sc_metal")
    # an umbilical plate on the rim and a row of four soft-landing jet ports just over it
    W = wall_out(math.radians(250), 0.42)
    with d.at(frame(W.translation, x=W.col[1].xyz, z=W.col[0].xyz)):
        d.box("tx_sc_metal", (0, 0, 0.006), (0.22, 0.16, 0.022), bevel=0.004)
        for k in range(3):
            d.cyl("SC_dark", (-0.06 + 0.06 * k, 0.0, 0.017), (-0.06 + 0.06 * k, 0.0, 0.027), 0.02, n=10)
    # the parachute container's lid on the dome, off the axis toward +Y, and the risers' fitting on the top
    lx, ly = polar(0.62, math.radians(100))[:2]
    def PO(a, b_):
        r = math.hypot(a, b_)
        t = (r / R_OUT1) ** DOME_P_OUT
        return Vector((a, b_, Z_OKNEE + (Z_TOP - Z_OKNEE) * max(1 - t, 0.0) ** (1 / DOME_P_OUT)))
    e = 1e-3
    nrm = (PO(lx + e, ly) - PO(lx - e, ly)).cross(PO(lx, ly + e) - PO(lx, ly - e)).normalized()
    Lf = frame(PO(lx, ly), y=(1, 0, 0), z=nrm)
    with d.at(Lf):
        d.rev(["tx_sc_metal", "tx_sc_metal", "tx_sc_paint_white", "tx_sc_paint_white"],
              [(0.40, -0.10), (0.40, 0.012), (0.37, 0.03), (0.2, 0.05), (0.0, 0.055)], n=40)
        for k in range(10):
            a = TAU * k / 10
            d.cyl("tx_sc_metal", (0.385 * math.cos(a), 0.385 * math.sin(a), 0.018), (0.385 * math.cos(a), 0.385 * math.sin(a), 0.03), 0.011, n=6)
        d.box("tx_sc_metal", (0, -0.40, 0.0), (0.16, 0.07, 0.07), bevel=0.008)
    lid = {"at": PO(lx, ly), "normal": nrm, "radius": 0.40}
    d.cyl("tx_sc_metal", (0, 0, Z_TOP - 0.02), (0, 0, Z_TOP + 0.03), 0.07, n=16)
    d.torus("tx_sc_metal", (0, 0, Z_TOP + 0.05), (0, 1, 0), 0.035, 0.012, n=16, m=6)
    # antennas: a whip on the dome's shoulder and a loop
    a0 = PO(*polar(0.85, math.radians(215))[:2])
    d.cyl("tx_sc_metal", a0 - Vector((0, 0, 0.05)), a0 + Vector((0, 0, 0.06)), 0.03, n=10)
    tip = a0 + Vector((-0.45, -0.32, 1.25))
    d.cyl("tx_sc_metal", a0 + Vector((0, 0, 0.05)), tip, 0.008, n=6, r2=0.004)
    a1 = PO(*polar(0.80, math.radians(-40))[:2])
    d.cyl("tx_sc_metal", a1 - Vector((0, 0, 0.05)), a1 + Vector((0, 0, 0.10)), 0.018, n=8)
    d.torus("tx_sc_metal", a1 + Vector((0, 0, 0.22)), (math.cos(math.radians(-40)), math.sin(math.radians(-40)), 0), 0.12, 0.009, n=20, m=6)
    o = join([skin, d.to_object("outer_details")], "SM_SC_CabinOuter")
    shade(o, 40)
    return o, lid


def glass(name, r):
    """A porthole's pane in its own frame: origin its middle, +X out of the hull, 16 mm thick; UV 0..1 across it."""
    b = Builder()
    with b.at(frame((0, 0, 0), x=(0, 1, 0), z=(1, 0, 0))):
        b.disc("SC_porthole", (0, 0, 0.008), r, n=40, uv=(0, 0, 1, 1))
        b.disc("SC_porthole", (0, 0, -0.008), r, n=40, up=False, uv=(0, 0, 1, 1))
    return b.to_object(name)


def glass_frame(az, z):
    A = axis_frame(az, z)
    w = wall_out(az, z)
    return frame(A @ Vector((0, 0, -GLASS_DEPTH)), x=w.col[0].xyz, z=w.col[2].xyz)


def hatch_door():
    """The door in its own frame (hatch_frame). Its inner face is quilted; the wheel is its own piece."""
    b = Builder()
    G = frame((-DOOR_T / 2, HINGE_OFF, 0), x=(0, 1, 0), z=(1, 0, 0))          # z out of the hull, 0 = the inner face
    Gi = frame((-DOOR_T / 2, HINGE_OFF, 0), x=(0, 1, 0), z=(-1, 0, 0))        # z into the cabin
    with b.at(G):
        b.tube("tx_sc_paint_white", (0, 0), 0.0, DOOR_R, 0.0, DOOR_T, n=64, inner=False)
        b.rev("tx_sc_paint_white", [(DOOR_R, DOOR_T), (0.56, DOOR_T + 0.028), (0.28, DOOR_T + 0.045), (0.0, DOOR_T + 0.05)], n=64)
        b.torus("tx_sc_rubber", (0, 0, 0.0), (0, 0, 1), DOOR_R - 0.012, 0.008, n=64, m=6)
        # an equalising valve and a pull handle outside
        b.cyl("tx_sc_metal", (0.0, -0.30, DOOR_T + 0.03), (0.0, -0.30, DOOR_T + 0.075), 0.035, n=12)
        b.pipe("tx_sc_metal", [(0.42, -0.13, DOOR_T + 0.02), (0.42, -0.13, DOOR_T + 0.10), (0.42, 0.13, DOOR_T + 0.10), (0.42, 0.13, DOOR_T + 0.02)], 0.013, n=8)
    with b.at(Gi):
        b.disc("tx_sc_paint_green", (0, 0, 0), DOOR_R, n=64, r0=0.52)
        b.rev("tx_sc_quilt", [(0.52, 0.0), (0.50, 0.018), (0.26, 0.03), (0.0, 0.032)], n=64)
        b.cyl("tx_sc_metal", (0, 0, 0.03), (0, 0, 0.10), 0.022, n=12)
        b.cyl("tx_sc_metal", (0, 0, 0.025), (0, 0, 0.045), 0.06, n=16)
        # four dogs round the rim
        for k in range(4):
            a = TAU * (k + 0.5) / 4
            b.box("tx_sc_metal", (0.562 * math.cos(a), 0.562 * math.sin(a), 0.014), (0.05, 0.09, 0.024),
                  Matrix.Rotation(a + PI / 2, 3, "Z"), bevel=0.005)
    with b.at(Gi @ Matrix.Rotation(PI, 4, "Z")):       # x to the right and y up for whoever looks at it from inside
        LAB.plate(b, "mark_open", (-0.05, 0.585, 0.0025), "SM_SC_Hatch")
        LAB.plate(b, "mark_shut", (0.05, 0.585, 0.0025), "SM_SC_Hatch")
    # the hinge: a barrel and two arms onto the door
    b.cyl("tx_sc_metal", (0, 0, -0.31), (0, 0, 0.31), 0.028, n=12)
    for s in (-1, 1):
        b.box("tx_sc_metal", (DOOR_T / 2 + 0.012, 0.19, s * 0.24), (0.045, 0.40, 0.06), bevel=0.008)
    wheel = Builder()
    wheel.torus("SC_red", (0, 0, 0), (1, 0, 0), 0.16, 0.014, n=32, m=8)
    for k in range(3):
        a = TAU * k / 3 + 0.4
        wheel.cyl("tx_sc_metal", (0, 0, 0), (0, 0.155 * math.cos(a), 0.155 * math.sin(a)), 0.009, n=6, caps=False)
    wheel.cyl("tx_sc_metal", (-0.012, 0, 0), (0.016, 0, 0), 0.032, n=12)
    wheel_at = frame((-DOOR_T / 2 - 0.10, HINGE_OFF, 0), x=(-1, 0, 0), z=(0, 0, 1))
    return b.to_object("SM_SC_Hatch", smooth=40), wheel.to_object("SM_SC_HatchWheel"), wheel_at


def fire_shell():
    """What the plasma is drawn on: a cone 0.10 outside the cabin's, both faces; U round (0..1), V up along it."""
    b, b2 = Builder(), Builder()
    n, rows = 48, 8
    z0, z1, off = Z_RIM0 - 0.10, Z_OKNEE + 0.15, 0.10
    ra, rb = R_OUT0 + off + 0.02, R_OUT1 + off - 0.03
    for j in range(rows):
        for i in range(n):
            def P(i_, j_):
                t = j_ / rows
                return polar(ra + (rb - ra) * t, TAU * i_ / n, z0 + (z1 - z0) * t)
            q = [P(i, j), P(i + 1, j), P(i + 1, j + 1), P(i, j + 1)]
            uv = [(i / n, j / rows), ((i + 1) / n, j / rows), ((i + 1) / n, (j + 1) / rows), (i / n, (j + 1) / rows)]
            b.face(q, "SC_fire", uv)
            b2.face(q[::-1], "SC_fire", uv[::-1])
    o = join([b.to_object("fire_out", smooth=180), b2.to_object("fire_in", smooth=180)], "SM_SC_FireShell")
    return o, {"z0": z0, "z1": z1, "r0": ra, "r1": rb}


# --- the cabin's inside -------------------------------------------------------------------------------------------------
def zf(M):
    """A wall or dome frame (X = the normal) as one whose Z is the normal (for rods, discs, turned shapes)."""
    return frame(M.translation, x=M.col[1].xyz, z=M.col[0].xyz)


SHELF_F = frame((0, 0, SHELF_Z), x=polar(1, SHELF_AZ), z=(0, 0, 1))        # (rho, tau, height over the shelf)
PLAYER_F = SHELF_F @ frame((PLAYER_RHO, PLAYER_TAU, 0), x=(-1, 0, 0), z=(0, 0, 1))   # the player: +X = its front
RACK_F = wall_in(RACK_AZ, RACK_Z)
NET_F = wall_in(NET_AZ, NET_Z)
CHUTE_F = dome_in(0.12, 0.42)
LAMPS_AT = ((0.26, 0.40), (0.26, -0.40), (-0.80, 0.0))
FANS_AT = ((math.radians(38), 1.42), (math.radians(-38), 1.42))


def cabin_inner(skin):
    d = Builder()
    info = {}
    # ribs up the wall between the pillows, a hoop where the wall meets the dome (it stops at the big porthole)
    L = QUILT_SLANT + KICK / math.cos(S_IN)
    for az in (35, 63, 117, 140, -35, -63, -117, -140):
        with d.at(wall_in(math.radians(az), 0.0)):
            d.box("tx_sc_paint_green", (0.011, 0, L / 2), (0.034, 0.045, L), bevel=0.006)
    t_in, re, _ = collar_end(BIG_AZ, BIG_Z, BIG_R, BIG_W, BIG_FLARE)        # the hoop ends inside the big porthole's collar
    dz = (Z_KNEE - (axis_frame(BIG_AZ, BIG_Z) @ Vector((0, 0, t_in))).z) / math.cos(S_OUT)
    gap = math.asin(math.sqrt(max((re + BIG_W / 2) ** 2 - dz * dz, 1e-4)) / R_IN1)
    # buttons where the quilt's seams cross (not where an opening or its collar is)
    holes = [(axis_frame(az, z), collar_end(az, z, r, 0.06, fl)[1] + 0.09) for _, az, z, r, _, fl in OPENINGS]
    for j in range(1, QUILT_ROWS):
        for i in range(QUILT_COLS):
            W = wall_in(TAU * i / QUILT_COLS, KICK + (Z_KNEE - KICK) * j / QUILT_ROWS)
            p = W.translation
            if any(((A.inverted() @ p).xy).length < rr for A, rr in holes):
                continue
            with d.at(zf(W)):
                d.rev("tx_sc_quilt", [(0.013, -0.002), (0.010, 0.004), (0.0, 0.0055)], n=6)
    d.torus("tx_sc_paint_green", (0, 0, Z_KNEE), (0, 0, 1), R_IN1 - 0.006, 0.022, n=56, m=8, a0=gap, a1=TAU - gap)
    # the kick band's top bead
    d.torus("tx_sc_metal", (0, 0, KICK), (0, 0, 1), r_in(KICK) - 0.004, 0.008, n=56, m=6)
    # cable runs: two cables round the dome with clips, drops down to the console and behind the couches
    zc = 1.72
    for k, (dz, dr) in enumerate(((0.0, 0.016), (0.03, 0.014))):
        rr = r_in(zc + dz) - dr
        d.torus("tx_sc_rubber", (0, 0, zc + dz), (0, 0, 1), rr, 0.011 - 0.002 * k, n=56, m=6)
    for k in range(12):
        a = TAU * (k + 0.5) / 12
        with d.at(frame(polar(r_in(zc + 0.015) - 0.012, a, zc + 0.015), x=polar(1, a + PI), z=(0, 0, 1))):
            d.box("tx_sc_metal", (0.004, 0, 0), (0.034, 0.03, 0.075), bevel=0.004)
    for az in (math.radians(27), math.radians(-125)):
        pts = []
        for k in range(9):
            z = zc - (zc - (0.9 if az > 0 else 0.03)) * k / 8
            pts.append(polar(r_in(z) - (0.03 if z < Z_KNEE else 0.016), az, z))
        d.pipe("tx_sc_rubber", pts, 0.010, n=6)
    # handrails: either side of the hatch, one overhead along the aisle, one under the right porthole
    for az in (HATCH_AZ - math.radians(46), HATCH_AZ + math.radians(46)):
        W = [wall_in(az, z, off) for z, off in ((0.72, -0.01), (0.76, 0.075), (1.30, 0.075), (1.34, -0.01))]
        d.pipe("tx_sc_metal", [w.translation for w in W], 0.014, n=8)
    for y in (-0.34, 0.34):
        d.pipe("tx_sc_metal", [dome_in(x, y, off).translation for x, off in ((-0.46, -0.01), (-0.42, 0.06), (-0.02, 0.06), (0.02, -0.01))], 0.014, n=8)
    az0, az1 = -SIDE_AZ - math.radians(15), -SIDE_AZ + math.radians(15)
    pts = [wall_in(az0, 0.93, -0.01).translation]
    pts += [wall_in(az0 + (az1 - az0) * k / 6, 0.93, 0.07).translation for k in range(7)]
    d.pipe("tx_sc_metal", pts + [wall_in(az1, 0.93, -0.01).translation], 0.014, n=8)
    # three cabin lamps on the dome: a base, a milky lens, a wire guard
    info["lamps"] = []
    for x, y in LAMPS_AT:
        D = dome_in(x, y)
        with d.at(zf(D)):
            d.cyl("tx_sc_metal", (0, 0, -0.03), (0, 0, 0.022), 0.075, n=20)
            d.rev("SC_lamp", [(0.062, 0.022), (0.055, 0.045), (0.03, 0.058), (0.0, 0.062)], n=20)
            for k in range(2):
                a = PI / 2 * k
                c, s = math.cos(a), math.sin(a)
                d.pipe("tx_sc_metal", [(0.07 * c, 0.07 * s, 0.02), (0.066 * c, 0.066 * s, 0.055), (0.03 * c, 0.03 * s, 0.072),
                                       (-0.03 * c, -0.03 * s, 0.072), (-0.066 * c, -0.066 * s, 0.055), (-0.07 * c, -0.07 * s, 0.02)], 0.004, n=5)
        info["lamps"].append({"lens": D @ Vector((0.045, 0, 0)), "light": D @ Vector((0.16, 0, 0)), "normal": D.col[0].xyz.copy()})
    # two fans on brackets: a ring, a wire guard, a hub and blades (still: the game may spin a blur disc at "fans")
    info["fans"] = []
    for az, z in FANS_AT:
        W = wall_in(az, z)
        aim = (Vector((-0.1, 0.0, 1.0)) - W.translation).normalized()
        c = W @ Vector((0.13, 0, 0))
        F = frame(c, y=W.col[2].xyz, z=aim)
        d.cyl("tx_sc_metal", W @ Vector((-0.01, 0, 0)), c - aim * 0.045, 0.012, n=8)
        with d.at(F):
            d.tube("tx_sc_paint_green", (0, 0), 0.078, 0.086, -0.03, 0.03, n=24, top="tx_sc_paint_green", bottom="tx_sc_paint_green")
            d.cyl("tx_sc_metal", (0, 0, -0.05), (0, 0, 0.018), 0.026, n=12)
            for k in range(4):
                a = TAU * k / 4 + 0.3
                c0, s0 = math.cos(a), math.sin(a)
                c1, s1 = math.cos(a + 0.75), math.sin(a + 0.75)
                q = [(0.024 * c0, 0.024 * s0, -0.008), (0.072 * c0, 0.072 * s0, -0.016), (0.072 * c1, 0.072 * s1, 0.016), (0.024 * c1, 0.024 * s1, 0.008)]
                d.face(q, "SC_dark")
                d.face(q[::-1], "SC_dark")
            for rr in (0.045, 0.078):
                d.torus("tx_sc_metal", (0, 0, 0.034), (0, 0, 1), rr, 0.003, n=24, m=4)
            for k in range(4):
                a = TAU * k / 4
                d.cyl("tx_sc_metal", (0, 0, 0.034), (0.08 * math.cos(a), 0.08 * math.sin(a), 0.034), 0.003, n=4, caps=False)
        info["fans"].append({"at": c.copy(), "axis": aim.copy(), "radius": 0.075})
    # the record player's shelf on the left wall: a board into the wall, a lip rail, two gussets
    with d.at(SHELF_F):
        rw = r_in(SHELF_Z) + 0.004
        t0, t1, rf = -0.34, 0.18, 0.72
        arc = [(math.sqrt(rw * rw - t * t), t) for t in [t0 + (t1 - t0) * k / 8 for k in range(9)]]
        top = [(rf, t0)] + arc + [(rf, t1)]
        d.face([(p[0], p[1], 0.0) for p in top], "tx_sc_paint_green")
        d.face([(p[0], p[1], -0.022) for p in reversed(top)], "tx_sc_paint_green")
        for (a, b_) in (((rf, t1), (rf, t0)), ((rf, t0), arc[0]), (arc[-1], (rf, t1))):
            d.face([(a[0], a[1], -0.022), (b_[0], b_[1], -0.022), (b_[0], b_[1], 0.0), (a[0], a[1], 0.0)], "tx_sc_paint_green")
        d.pipe("tx_sc_metal", [(arc[0][0] - 0.03, t0 + 0.012, 0.0), (arc[0][0] - 0.03, t0 + 0.012, 0.035), (rf + 0.012, t0 + 0.012, 0.035),
                               (rf + 0.012, t1 - 0.012, 0.035), (arc[-1][0] - 0.03, t1 - 0.012, 0.035), (arc[-1][0] - 0.03, t1 - 0.012, 0.0)], 0.006, n=6)
        for t in (t0 + 0.14, t1 - 0.14):
            d.cyl("tx_sc_metal", (rf + 0.012, t, 0.0), (rf + 0.012, t, 0.035), 0.005, n=6, caps=False)
        for t in (t0 + 0.06, t1 - 0.06):
            rb = math.sqrt(rw * rw - t * t)
            d.hull("tx_sc_metal", [(rf + 0.05, t - 0.006, -0.022), (rf + 0.05, t + 0.006, -0.022), (rb, t - 0.006, -0.022), (rb, t + 0.006, -0.022),
                                   (rb + 0.03, t - 0.006, -0.26), (rb + 0.03, t + 0.006, -0.26), (rb - 0.02, t - 0.006, -0.26), (rb - 0.02, t + 0.006, -0.26)])
    # the sleeve rack on the wall beside it: a tray, two posts and an elastic band across the sleeves
    with d.at(RACK_F):
        d.box("tx_sc_metal", (0.07, 0, -0.007), (0.17, 0.30, 0.014), bevel=0.003)
        d.box("tx_sc_metal", (0.152, 0, 0.012), (0.006, 0.30, 0.036), bevel=0.002)
        for s in (-1, 1):
            d.cyl("tx_sc_metal", (-0.03, s * 0.155, 0.17), (0.145, s * 0.155, 0.17), 0.005, n=6)
        d.box("tx_sc_rubber", (0.140, 0, 0.17), (0.004, 0.31, 0.03))
    # the stowage net on the right wall: a tray, a tube frame on standoffs, the net (tx_sc_net: a cutout texture)
    with d.at(NET_F):
        d.box("tx_sc_metal", (0.055, 0, -0.007), (0.16, 0.52, 0.014), bevel=0.003)
        d.box("tx_sc_metal", (0.132, 0, 0.012), (0.006, 0.52, 0.036), bevel=0.002)
        d.pipe("tx_sc_metal", [(0.132, -0.25, 0.03), (0.132, 0.25, 0.03), (0.132, 0.25, 0.37), (0.132, -0.25, 0.37)], 0.007, n=6, closed=True)
        for s in (-1, 1):
            d.cyl("tx_sc_metal", (-0.04, s * 0.25, 0.37), (0.130, s * 0.25, 0.37), 0.006, n=6, caps=False)
        d.face([(0.132, -0.25, 0.03), (0.132, 0.25, 0.03), (0.132, 0.25, 0.37), (0.132, -0.25, 0.37)], "tx_sc_net")
    # the parachute handle's base on the dome over the hero's couch, its plates
    with d.at(CHUTE_F):
        d.box("tx_sc_metal", (0.0, 0, -0.03), (0.014, 0.17, 0.25), bevel=0.003)
        d.cyl("tx_sc_metal", (0.006, 0, 0), (0.03, 0, 0), 0.013, n=10)
        d.cyl("tx_sc_metal", (0.004, -0.06, 0.06), (0.004, 0.06, 0.06), 0.007, n=8)
    with d.at(CHUTE_F @ frame((0.007, 0, 0), x=(0, -1, 0), y=(0, 0, -1))):
        LAB.plate(d, "mark_chute", (0, 0.098, 0.0025), "SM_SC_CabinInner")
        LAB.plate(d, "mark_manual", (0, 0.084, 0.0025), "SM_SC_CabinInner")
    # a foot rail under the console
    d.pipe("tx_sc_metal", [(0.92, -0.62, 0.0), (0.86, -0.62, 0.10), (0.86, 0.62, 0.10), (0.92, 0.62, 0.0)], 0.014, n=8)
    o = join([skin, d.to_object("inner_details")], "SM_SC_CabinInner")
    shade(o, 40)
    quilt_seams(o)
    return o, info


def quilt_seams(o):
    """The quilt's seams sharp (the pillows between them stay soft)."""
    me = o.data
    qi = {i for i, m in enumerate(me.materials) if m.name == "tx_sc_quilt"}
    bm = bmesh.new()
    bm.from_mesh(me)
    for e in bm.edges:
        if len(e.link_faces) == 2 and all(f.material_index in qi for f in e.link_faces) and \
                all(abs(math.hypot(v.co.x, v.co.y) - r_in(v.co.z)) < 0.002 for v in e.verts):
            e.smooth = False
    bm.to_mesh(me)
    bm.free()


# --- couches and what lies on the second one -----------------------------------------------------------------------------
PAN_A, PAN_B = Vector((-0.10, 0, 0.25)), Vector((0.36, 0, 0.33))      # the seat pan's line (side view), back to front
BACK_T = Vector((-0.40, 0, 1.02))                                     # the back's top


def couch():
    """A couch in its own frame: origin on the floor under the pelvis, +X forward. Tube frame, three back cushions,
    two seat cushions, a headrest, belts hanging at the sides."""
    b = Builder()
    hw = 0.235
    for s in (-1, 1):
        y = s * hw
        b.pipe("tx_sc_metal", [(0.40, y, 0.0), (PAN_B.x + 0.02, y, PAN_B.z - 0.03), (PAN_A.x, y, PAN_A.z - 0.03),
                               (BACK_T.x - 0.035, y, BACK_T.z + 0.02), (BACK_T.x - 0.03, y, BACK_T.z + 0.20)], 0.016, n=8)
        b.pipe("tx_sc_metal", [(PAN_A.x - 0.01, y, PAN_A.z - 0.03), (PAN_A.x - 0.06, y, 0.0)], 0.014, n=8)
        b.pipe("tx_sc_metal", [(-0.30, y, 0.72), (-0.44, y, 0.0)], 0.012, n=8)
        for x in (0.40, PAN_A.x - 0.06, -0.44):
            b.cyl("tx_sc_metal", (x, y, 0.0), (x, y, 0.012), 0.035, n=10)
        # a belt half hanging from the pan's side with its buckle
        b.box("tx_sc_fabric", (0.02, s * (hw + 0.021), 0.19), (0.045, 0.004, 0.22))
        b.box("tx_sc_metal", (0.02, s * (hw + 0.021), 0.07), (0.055, 0.012, 0.045), bevel=0.003)
    for (x, z) in ((PAN_B.x + 0.02, PAN_B.z - 0.03), (PAN_A.x, PAN_A.z - 0.03), (BACK_T.x - 0.035, BACK_T.z + 0.02), (BACK_T.x - 0.03, BACK_T.z + 0.20)):
        b.cyl("tx_sc_metal", (x, -hw, z), (x, hw, z), 0.012, n=8, caps=False)
    # cushions on a thin shell
    ex = (PAN_B - PAN_A).normalized()
    Fp = frame(PAN_A, x=ex, y=(0, 1, 0))
    with b.at(Fp):
        ln = (PAN_B - PAN_A).length
        b.box("tx_sc_metal", (ln / 2, 0, -0.005), (ln, 0.45, 0.012))
        for k in range(2):
            b.box("tx_sc_fabric", (ln * (0.25 + 0.5 * k), 0, 0.036), (ln / 2 - 0.008, 0.43, 0.07), bevel=0.02)
    eb = (BACK_T - PAN_A).normalized()
    Fb = frame(PAN_A, x=eb, z=(0, 1, 0))      # x up the back, y = forward of it
    with b.at(Fb):
        ln = (BACK_T - PAN_A).length
        b.box("tx_sc_metal", (ln / 2 + 0.03, -0.005, 0), (ln - 0.06, 0.012, 0.45))
        for k in range(3):
            b.box("tx_sc_fabric", (0.07 + (ln - 0.08) * (k + 0.5) / 3, 0.036, 0), ((ln - 0.08) / 3 - 0.008, 0.07, 0.43), bevel=0.02)
        b.box("tx_sc_fabric", (ln + 0.10, 0.045, 0), (0.17, 0.085, 0.26), bevel=0.025)
        for s in (-1, 1):
            b.box("tx_sc_fabric", (ln + 0.10, 0.068, s * 0.145), (0.15, 0.085, 0.04), Matrix.Rotation(s * 0.45, 3, "X"), bevel=0.016)
    return b.to_object("SM_SC_Couch", smooth=50)


def couch_frame(i):
    x, y = COUCH_AT[i]
    return Matrix.Translation((x, y, 0)) @ Matrix.Rotation(COUCH_YAW[i], 4, "Z")


def blanket_and_cap():
    """In the couch's frame, on its seat: a folded blanket and a peaked cap on it."""
    ex = (PAN_B - PAN_A).normalized()
    Fp = frame(PAN_A, x=ex, y=(0, 1, 0))
    b = Builder()
    with b.at(Fp):
        for k, (w, dx) in enumerate(((0.36, 0.0), (0.35, 0.006), (0.34, -0.004))):
            b.box("SC_blanket", (0.22 + dx, 0.01, 0.073 + 0.016 + 0.034 * k), (0.30, w, 0.032), bevel=0.014)
    blanket = b.to_object("SM_SC_Blanket", smooth=60)
    c = Builder()
    top = 0.073 + 0.034 * 3
    with c.at(Fp @ Matrix.Translation((0.20, -0.02, top)) @ Matrix.Rotation(0.5, 4, "Z")):
        c.rev(["SC_dark", "SC_cap", "SC_cap", "SC_cap"], [(0.088, 0.0), (0.090, 0.04), (0.118, 0.075), (0.105, 0.092), (0.0, 0.098)], n=24)
        c.disc("SC_dark", (0, 0, 0.0), 0.088, n=24, up=False)
        # the peak: a crescent out from the band's front, a little down; its top and its underside
        angs = [math.radians(-62 + 124 * k / 10) for k in range(11)]
        vis = [(0.088 * math.cos(a), 0.088 * math.sin(a)) for a in angs]
        rim = [(0.088 * math.cos(a) + 0.066 * math.cos(a * 90 / 62), 0.097 * math.sin(a)) for a in angs]
        for k in range(10):
            q = [(vis[k][0], vis[k][1], 0.016), (rim[k][0], rim[k][1], 0.004), (rim[k + 1][0], rim[k + 1][1], 0.004), (vis[k + 1][0], vis[k + 1][1], 0.016)]
            c.face(q, "tx_sc_rubber")
            c.face([(p[0], p[1], p[2] - 0.004) for p in reversed(q)], "tx_sc_rubber")
            c.face([(rim[k + 1][0], rim[k + 1][1], 0.0), (rim[k + 1][0], rim[k + 1][1], 0.004), (rim[k][0], rim[k][1], 0.004), (rim[k][0], rim[k][1], 0.0)], "tx_sc_rubber")
        c.cyl("SC_yellow", (0.089, -0.012, 0.03), (0.097, -0.012, 0.03), 0.012, n=8)
    cap = c.to_object("SM_SC_Cap", smooth=50)
    return blanket, cap


def thermos():
    """Own frame: origin the middle of its foot, Z up. 0.29 m tall."""
    b = Builder()
    b.rev(["tx_sc_metal", "tx_sc_paint_green", "tx_sc_metal", "tx_sc_metal", "tx_sc_metal", "tx_sc_metal", "tx_sc_metal"],
          [(0.044, 0.0), (0.046, 0.012), (0.046, 0.195), (0.036, 0.215), (0.040, 0.218), (0.040, 0.280), (0.034, 0.29), (0.0, 0.29)], n=20)
    b.disc("tx_sc_metal", (0, 0, 0), 0.044, n=20, up=False)
    b.pipe("tx_sc_metal", [(0.044, 0, 0.05), (0.075, 0, 0.06), (0.075, 0, 0.15), (0.044, 0, 0.16)], 0.006, n=6)
    return b.to_object("SM_SC_Thermos")


BOOK = (0.135, 0.200, 0.028)


def book():
    """Own frame: lying shut on its back cover, origin the middle of that cover, front cover up (+Z), spine at -X,
    the text's top toward +Y. SC_book: one picture, U 0..0.47 back, 0.47..0.53 spine, 0.53..1 front."""
    b = Builder()
    w, h, t = BOOK
    c = 0.003
    b.face([(-w / 2, -h / 2, t), (w / 2, -h / 2, t), (w / 2, h / 2, t), (-w / 2, h / 2, t)], "SC_book", [(0.53, 0), (1, 0), (1, 1), (0.53, 1)])
    b.face([(w / 2, -h / 2, 0), (-w / 2, -h / 2, 0), (-w / 2, h / 2, 0), (w / 2, h / 2, 0)], "SC_book", [(0, 0), (0.47, 0), (0.47, 1), (0, 1)])
    b.face([(-w / 2, -h / 2, 0), (-w / 2, -h / 2, t), (-w / 2, h / 2, t), (-w / 2, h / 2, 0)], "SC_book", [(0.47, 0), (0.53, 0), (0.53, 1), (0.47, 1)])
    b.box("SC_paper", (0.001, 0, t / 2), (w - 0.006, h - 0.008, t - 2 * c), skip=("-x", "+z", "-z"))
    for z0, z1 in ((0.0, c), (t - c, t)):       # the covers' edges
        b.box("SC_dark", (0, 0, (z0 + z1) / 2), (w, h, z1 - z0), skip=("+z", "-z", "-x") if z0 == 0 else ("+z", "-z", "-x"))
        b.face([(-w / 2, -h / 2, z1 if z0 == 0 else z0), (w / 2, -h / 2, z1 if z0 == 0 else z0), (w / 2, h / 2, z1 if z0 == 0 else z0), (-w / 2, h / 2, z1 if z0 == 0 else z0)][::(1 if z0 == 0 else -1)], "SC_dark")
    return b.to_object("SM_SC_Book", smooth=None)


# --- the instrument panel -------------------------------------------------------------------------------------------------
# everything in panel coordinates (u to the right, v up the face, n out of it), metres
# dials: id, (u, v), face radius, atlas rect [x, y, w, h] px on SC_dials (1024, from the top left), scale
#   scale: value min..max over the angles a_min..a_max, degrees clockwise from 12 o'clock as the hero sees it
DIALS = [
    {"id": "alt", "c": (-0.232, 0.385), "r": 0.065, "rect": [0, 0, 512, 512], "min": 0, "max": 10, "a_min": 0, "a_max": 360, "major": 1, "minor": 0.2,
     "needles": [{"id": "long", "per_rev": 10, "length": 0.90, "lift": 0.0025}, {"id": "short", "per_rev": 100, "length": 0.55, "lift": 0.0}],
     "note": "an altimeter: the scale 0..9 round the whole face; the long needle turns once per 10 units, the short once per 100 (the flight code picks the unit: 100 km in space, 1 km under the parachute)"},
    {"id": "gmeter", "c": (0.288, 0.385), "r": 0.055, "rect": [512, 0, 512, 512], "min": 0, "max": 6, "a_min": -135, "a_max": 135, "major": 1, "minor": 0.2},
    {"id": "press", "c": (-0.265, 0.20), "r": 0.032, "rect": [0, 512, 256, 256], "min": 0, "max": 1000, "a_min": -120, "a_max": 120, "major": 200, "minor": 50, "unit": "mmhg"},
    {"id": "o2", "c": (-0.157, 0.20), "r": 0.032, "rect": [256, 512, 256, 256], "min": 0, "max": 40, "a_min": -120, "a_max": 120, "major": 10, "minor": 2, "unit": "pct"},
    {"id": "co2", "c": (-0.049, 0.20), "r": 0.032, "rect": [512, 512, 256, 256], "min": 0, "max": 3, "a_min": -120, "a_max": 120, "major": 1, "minor": 0.2, "unit": "pct"},
    {"id": "temp", "c": (0.059, 0.20), "r": 0.032, "rect": [768, 512, 256, 256], "min": 0, "max": 40, "a_min": -120, "a_max": 120, "major": 10, "minor": 2, "unit": "degc"},
    {"id": "volts", "c": (0.167, 0.20), "r": 0.032, "rect": [0, 768, 256, 256], "min": 20, "max": 32, "a_min": -120, "a_max": 120, "major": 2, "minor": 0.5, "unit": "volt"},
    {"id": "fuel", "c": (0.275, 0.20), "r": 0.032, "rect": [256, 768, 256, 256], "min": 0, "max": 100, "a_min": -120, "a_max": 120, "major": 25, "minor": 5, "unit": "pct"},
]
DIAL_FACE_N, NEEDLE_N, GLASS_N = -0.016, -0.0105, -0.0015
GLOBE = {"c": (-0.047, 0.385), "r": 0.070, "depth": 0.090, "window": 0.066}
BALL = {"c": (0.130, 0.385), "r": 0.055, "depth": 0.072, "window": 0.050}
# seven-segment windows: id, the window's (u, v) middle, digits, cell (w, h), pitch, extra gap after these digits
DIGIT_W, DIGIT_H, DIGIT_PITCH = 0.017, 0.028, 0.0185
WINDOWS = [("clock", (-0.5435, 0.345), 6, (2, 4)), ("range", (-0.5435, 0.297), 6, ()), ("speed", (-0.5435, 0.249), 5, ())]
WINDOW_W, WINDOW_H = 0.135, 0.040
LAMP_COLS, LAMP_ROWS = 6, 4
LAMP_C = (-0.505, 0.447)
LAMP_PITCH = (0.055, 0.0235)
CRT = {"c": (0.487, 0.40), "w": 0.20, "h": 0.15}
BAR = {"c": (0.635, 0.40), "w": 0.028, "h": 0.15, "min": 0, "max": 2000}
SPEAKER = {"c": (0.52, 0.10), "w": 0.20, "h": 0.11}
TOGGLE_V = 0.085
TOGGLE_U = [-0.42, -0.35, -0.28, -0.21, -0.14, 0.14, 0.21, 0.28]
TOGGLE_N = 0.007               # the lever's pivot over the face
TOGGLE_ON, TOGGLE_OFF = 25.0, -25.0
LAUNCH = {"c": (0.0, 0.085), "base": 0.076, "button_r": 0.019, "travel": 0.006}


def panel_xyz(u, v, n=0.0):
    return PM @ Vector((u, v, n))


def digit_cells():
    """[(window id, index in the window, global index, (u, v))]: the cells right-aligned in their windows."""
    out, g = [], 0
    for wid, (cu, cv), count, gaps in WINDOWS:
        total = count * DIGIT_PITCH + 0.006 * len(gaps)
        u = cu + WINDOW_W / 2 - 0.008 - total + DIGIT_PITCH / 2
        for i in range(count):
            if i in gaps:
                u += 0.006
            out.append((wid, i, g, (u, cv)))
            u += DIGIT_PITCH
            g += 1
    return out


def lamp_cells():
    """[(index, row, col, (u, v))]: row 0 on top, the atlas' lamp order row by row."""
    out = []
    for r in range(LAMP_ROWS):
        for c in range(LAMP_COLS):
            out.append((r * LAMP_COLS + c, r, c, (LAMP_C[0] + (c - (LAMP_COLS - 1) / 2) * LAMP_PITCH[0],
                                                   LAMP_C[1] + ((LAMP_ROWS - 1) / 2 - r) * LAMP_PITCH[1])))
    return out


def panel():
    """The console: a body from the slanted face back into the wall, wells cut into the face for every instrument,
    then bezels, faces, windows, plates, switches' bases. Moving parts are their own pieces."""
    G, DK = "tx_sc_paint_green", "SC_dark"
    b = Builder([G, DK])
    x0, z0 = PANEL_O.x, PANEL_O.z
    top = PM @ Vector((0, PANEL_H, 0))
    x1, z1 = top.x, top.z
    hw = PANEL_W / 2
    ys = [-hw + PANEL_W * i / 14 for i in range(15)]
    def BT(y):
        R = r_in(z1) + 0.012
        return Vector((math.sqrt(R * R - y * y), y, z1))
    def BB(y):
        R = r_in(PANEL_ZB) + 0.012
        return Vector((math.sqrt(R * R - y * y), y, PANEL_ZB))
    def F0(y):
        return Vector((x0, y, z0))
    def F1(y):
        return Vector((x1, y, z1))
    for ya, yb in zip(ys, ys[1:]):
        b.face([F0(ya), F1(ya), F1(yb), F0(yb)], G)
        b.face([F1(ya), BT(ya), BT(yb), F1(yb)], G)
        b.face([F0(yb), BB(yb), BB(ya), F0(ya)], G)
        b.face([BT(ya), BB(ya), BB(yb), BT(yb)], G)
    b.face([F0(hw), F1(hw), BT(hw), BB(hw)], G)
    b.face([BB(-hw), BT(-hw), F1(-hw), F0(-hw)], G)
    body = b.to_object("panel_body", smooth=None)
    # the wells
    c = Builder([G, DK])
    with c.at(PM):
        for d in DIALS:
            c.cyl(DK, (*d["c"], DIAL_FACE_N - 0.003), (*d["c"], 0.02), d["r"] + 0.004, n=40)
        c.cyl(DK, (*GLOBE["c"], -(GLOBE["depth"] + GLOBE["r"] + 0.008)), (*GLOBE["c"], 0.02), GLOBE["r"] + 0.012, n=40)
        c.cyl(DK, (*BALL["c"], -(BALL["depth"] + BALL["r"] + 0.008)), (*BALL["c"], 0.02), BALL["r"] + 0.010, n=40)
        for _, (u, v), _, _ in WINDOWS:
            c.box(DK, (u, v, 0.0), (WINDOW_W, WINDOW_H, 0.018))
        c.box(DK, (*LAMP_C, 0.0), (LAMP_COLS * LAMP_PITCH[0] + 0.006, LAMP_ROWS * LAMP_PITCH[1] + 0.006, 0.012))
        c.box(DK, (*CRT["c"], 0.0), (CRT["w"] + 0.008, CRT["h"] + 0.008, 0.050))
        c.box(DK, (*BAR["c"], 0.0), (BAR["w"] + 0.006, BAR["h"] + 0.006, 0.016))
        c.box(DK, (*SPEAKER["c"], 0.0), (SPEAKER["w"], SPEAKER["h"], 0.030))
    boolean_cut(body, c.to_object("panel_cut", smooth=None))
    metre_uvs(body)

    d = Builder()
    M = "tx_sc_metal"
    # the hood: a black lip over the face's top edge and a mat on the console's top
    d.box("tx_sc_rubber", (x1 - 0.005, 0, z1 + 0.008), (0.12, PANEL_W + 0.04, 0.016), bevel=0.005)
    def screw(u, v, n=0.0, r=0.0042):
        d.cyl(M, (u, v, n), (u, v, n + 0.0025), r, n=8)
    with d.at(PM):
        # screws round the face's edge
        for k in range(8):
            for v in (0.012, PANEL_H - 0.012):
                screw(-hw + 0.012 + (PANEL_W - 0.024) * k / 7, v)
        for v in (0.18, 0.34):
            for u in (-hw + 0.012, hw - 0.012):
                screw(u, v)
        def flange(u, v, half, hole, top=0.003):
            """A square plate with a round hole (the instrument's own mounting plate)."""
            n = 40
            def sq(a):
                c_, s_ = math.cos(a), math.sin(a)
                k = half / max(abs(c_), abs(s_))
                return (u + c_ * k, v + s_ * k, top)
            for i in range(n):
                a0, a1 = TAU * i / n, TAU * (i + 1) / n
                d.face([(u + hole * math.cos(a0), v + hole * math.sin(a0), top), sq(a0), sq(a1), (u + hole * math.cos(a1), v + hole * math.sin(a1), top)], DK)
            cs = [(half, -half), (half, half), (-half, half), (-half, -half)]
            for (xa, ya), (xb, yb) in zip(cs, cs[1:] + cs[:1]):
                d.face([(u + xa, v + ya, 0), (u + xb, v + yb, 0), (u + xb, v + yb, top), (u + xa, v + ya, top)], DK)
        # dials: a square flange with four screws under a round black bezel; the face disc; a glass
        for dl in DIALS:
            u, v = dl["c"]
            r = dl["r"]
            big = r > 0.04
            if big:
                fl = r + 0.022
                flange(u, v, fl, r + 0.004)
                for su in (-1, 1):
                    for sv in (-1, 1):
                        screw(u + su * (fl - 0.008), v + sv * (fl - 0.008), 0.003)
            with d.at(Matrix.Translation((u, v, 0))):
                z = 0.003 if big else 0.0
                d.rev(DK, [(r + 0.014, z), (r + 0.012, z + 0.006), (r + 0.003, z + 0.0075), (r - 0.0015, z + 0.001), (r - 0.0015, -0.003)], n=40)
                if not big:
                    for k in range(3):
                        a = TAU * k / 3 + PI / 2
                        screw((r + 0.0085) * math.cos(a), (r + 0.0085) * math.sin(a), 0.0058, 0.003)
            d.disc("SC_dials", (u, v, DIAL_FACE_N), r + 0.003, n=40, uv=dial_uv(dl))
            d.disc("SC_glass", (u, v, GLASS_N), r + 0.003, n=32, uv=(0, 0, 1, 1))
        # the globe: a deep well, a bezel with six screws, a crosshair and a ring under the glass
        u, v = GLOBE["c"]
        with d.at(Matrix.Translation((u, v, 0))):
            r = GLOBE["window"]
            d.rev(DK, [(r + 0.03, 0.0), (r + 0.027, 0.008), (r + 0.006, 0.010), (r, 0.002), (r, -0.006), (GLOBE["r"] + 0.012, -0.006)], n=48)
            for k in range(6):
                a = TAU * k / 6
                screw((r + 0.018) * math.cos(a), (r + 0.018) * math.sin(a), 0.0088, 0.0036)
            for a in (0.0, PI / 2):
                ca, sa = math.cos(a), math.sin(a)
                d.box(M, (0, 0, -0.008), (2 * r + 0.004, 0.0016, 0.0016), Matrix.Rotation(a, 3, "Z"))
            d.torus(M, (0, 0, -0.008), (0, 0, 1), 0.016, 0.0009, n=20, m=4)
            d.disc("SC_glass", (0, 0, -0.003), r, n=40, uv=(0, 0, 1, 1))
        # the attitude ball: a well, a square flange, a bezel, the little fixed aircraft under the glass
        u, v = BALL["c"]
        fl = BALL["window"] + 0.026
        flange(u, v, fl, BALL["r"] + 0.010)
        for su in (-1, 1):
            for sv in (-1, 1):
                screw(u + su * (fl - 0.008), v + sv * (fl - 0.008), 0.003)
        with d.at(Matrix.Translation((u, v, 0))):
            r = BALL["window"]
            d.rev(DK, [(r + 0.018, 0.003), (r + 0.016, 0.009), (r + 0.005, 0.0105), (r, 0.003), (r, -0.006), (BALL["r"] + 0.010, -0.006)], n=40)
            for s in (-1, 1):
                d.box("SC_yellow", (s * 0.026, 0, -0.009), (0.024, 0.003, 0.002))
            d.box("SC_yellow", (0, 0, -0.009), (0.005, 0.005, 0.002))
            d.disc("SC_glass", (0, 0, -0.004), r, n=32, uv=(0, 0, 1, 1))
        # seven-segment windows: a thin frame, the cells (UVData: which of the 17 cells)
        cells = digit_cells()
        for _, (u, v), _, _ in WINDOWS:
            for (du, dv, w, h) in ((0, WINDOW_H / 2 + 0.0019, WINDOW_W + 0.012, 0.0038), (0, -WINDOW_H / 2 - 0.0019, WINDOW_W + 0.012, 0.0038),
                                   (WINDOW_W / 2 + 0.003, 0, 0.006, WINDOW_H), (-WINDOW_W / 2 - 0.003, 0, 0.006, WINDOW_H)):
                d.box(DK, (u + du, v + dv, 0.0015), (w, h, 0.003), bevel=0.001)
        for _, _, g, (u, v) in cells:
            d.quad("SC_digits", (u, v, -0.006), DIGIT_W, DIGIT_H, (0, 0, 1, 1), ((g + 0.5) / len(cells), 0.5))
        # the annunciator board: 24 lamps (UVMap: its caption on the label atlas; UVData: its cell of a 6 x 4 texture)
        bw, bh = LAMP_COLS * LAMP_PITCH[0] + 0.006, LAMP_ROWS * LAMP_PITCH[1] + 0.006
        for (du, dv, w, h) in ((0, bh / 2 + 0.003, bw + 0.012, 0.006), (0, -bh / 2 - 0.003, bw + 0.012, 0.006),
                               (bw / 2 + 0.003, 0, 0.006, bh), (-bw / 2 - 0.003, 0, 0.006, bh)):
            d.box(DK, (LAMP_C[0] + du, LAMP_C[1] + dv, 0.0015), (w, h, 0.003), bevel=0.001)
        for k in range(1, LAMP_COLS):
            d.box(M, (LAMP_C[0] + (k - LAMP_COLS / 2) * LAMP_PITCH[0], LAMP_C[1], -0.003), (0.003, bh, 0.006), skip=("-z",))
        for k in range(1, LAMP_ROWS):
            for j in range(LAMP_COLS):
                d.box(M, (LAMP_C[0] + (j - (LAMP_COLS - 1) / 2) * LAMP_PITCH[0], LAMP_C[1] + (k - LAMP_ROWS / 2) * LAMP_PITCH[1], -0.003),
                      (LAMP_PITCH[0] - 0.003, 0.003, 0.006), skip=("-z", "-x", "+x"))
        for i, r, c_, (u, v) in lamp_cells():
            pid = LAB.lamps[i]
            w, h = LAB.size_m(pid)
            d.quad("SC_lamps", (u, v, -0.0035), w, h, LAB.uv(pid), ((c_ + 0.5) / LAMP_COLS, 1 - (r + 0.5) / LAMP_ROWS))
            LAB.used.setdefault(pid, []).append({"piece": "SM_SC_Panel", "frame_matrix": d.M.copy(), "c": Vector((u, v, -0.0035))})
        # the CRT: a bezel of four bars, the screen (0..1)
        u, v = CRT["c"]
        w, h = CRT["w"], CRT["h"]
        for (du, dv, bw_, bh_) in ((0, h / 2 + 0.006, w + 0.036, 0.016), (0, -h / 2 - 0.006, w + 0.036, 0.016),
                                   (w / 2 + 0.008, 0, 0.016, h - 0.004), (-w / 2 - 0.008, 0, 0.016, h - 0.004)):
            d.box(DK, (u + du, v + dv, 0.004), (bw_, bh_, 0.008), bevel=0.003)
        for su in (-1, 1):
            for sv in (-1, 1):
                screw(u + su * (w / 2 + 0.010), v + sv * (h / 2 + 0.007), 0.008, 0.003)
        d.quad("SC_crt", (u, v, -0.022), w + 0.008, h + 0.008, (0, 0, 1, 1))
        # the hull bar
        u, v = BAR["c"]
        w, h = BAR["w"], BAR["h"]
        for (du, dv, bw_, bh_) in ((0, h / 2 + 0.005, w + 0.02, 0.008), (0, -h / 2 - 0.005, w + 0.02, 0.008),
                                   (w / 2 + 0.005, 0, 0.008, h + 0.002), (-w / 2 - 0.005, 0, 0.008, h + 0.002)):
            d.box(DK, (u + du, v + dv, 0.002), (bw_, bh_, 0.004), bevel=0.0015)
        d.quad("SC_bar", (u, v, -0.0055), w + 0.006, h + 0.006, (0, 0, 1, 1))
        # the speaker: a dark well behind eleven slats
        u, v = SPEAKER["c"]
        w, h = SPEAKER["w"], SPEAKER["h"]
        for k in range(11):
            d.box(M, (u, v - h / 2 + h * (k + 0.5) / 11, -0.003), (w + 0.004, 0.0045, 0.006), Matrix.Rotation(0.5, 3, "X"))
        for su in (-1, 1):
            d.box(G, (u + su * (w / 2 + 0.005), v, 0.002), (0.010, h + 0.02, 0.004), bevel=0.0015)
            for sv in (-1, 1):
                screw(u + su * (w / 2 + 0.005), v + sv * (h / 2 + 0.002), 0.004, 0.003)
        # the switches: a nut and a collar each (the lever is SM_SC_Toggle), guards on the five flight switches
        for i, u in enumerate(TOGGLE_U):
            d.cyl(M, (u, TOGGLE_V, 0), (u, TOGGLE_V, 0.003), 0.0105, n=6)
            d.cyl(M, (u, TOGGLE_V, 0.003), (u, TOGGLE_V, 0.009), 0.0062, n=10)
            if i < 5:
                for s in (-1, 1):
                    d.hull("SC_red" if i == 4 else M, [(u + s * 0.018 + e, TOGGLE_V + dv, n_) for e in (-0.0012, 0.0012)
                                                         for dv, n_ in ((-0.024, 0), (0.024, 0), (-0.016, 0.026), (0.016, 0.026))])
            LAB.plate(d, "sw_" + TOGGLES[i], (u, 0.028, 0.0025), "SM_SC_Panel")
            LAB.plate(d, "mark_on", (u, 0.122, 0.0025), "SM_SC_Panel")
            LAB.plate(d, "mark_off", (u, 0.050, 0.0025), "SM_SC_Panel")
        # ПУСК: a yellow base, a ring round the button, the hinge's lugs for the red cover
        u, v = LAUNCH["c"]
        s = LAUNCH["base"]
        d.box("SC_yellow", (u, v, 0.002), (s, s, 0.004), bevel=0.0015)
        d.tube(M, (u, v), LAUNCH["button_r"] + 0.001, LAUNCH["button_r"] + 0.007, 0.004, 0.011, n=24, top=M)
        for su in (-1, 1):
            d.box(M, (u + su * 0.0385, v + s / 2 - 0.004, 0.007), (0.006, 0.008, 0.008))
        LAB.plate(d, "sw_launch", (u, 0.028, 0.0025), "SM_SC_Panel")
        # name plates under the instruments, units, the craft's nameplate, the data plate
        for dl in DIALS:
            off = dl["r"] + (0.036 if dl["r"] > 0.04 else 0.024)
            LAB.plate(d, "inst_" + dl["id"], (dl["c"][0], dl["c"][1] - off, 0.0025), "SM_SC_Panel")
        LAB.plate(d, "inst_globe", (GLOBE["c"][0], GLOBE["c"][1] - GLOBE["window"] - 0.042, 0.0025), "SM_SC_Panel")
        LAB.plate(d, "inst_horizon", (BALL["c"][0], BALL["c"][1] - BALL["window"] - 0.038, 0.0025), "SM_SC_Panel")
        for (wid, (u, v), _, _), unit in zip(WINDOWS, (None, "unit_km", "unit_kms")):
            LAB.plate(d, "inst_" + wid, (u - WINDOW_W / 2 - 0.012 - LAB.size_m("inst_" + wid)[0] / 2, v, 0.0025), "SM_SC_Panel")
            if unit:
                LAB.plate(d, unit, (u + WINDOW_W / 2 + 0.012 + LAB.size_m(unit)[0] / 2, v, 0.0025), "SM_SC_Panel")
        LAB.plate(d, "inst_lamps", (LAMP_C[0], LAMP_C[1] - bh / 2 - 0.016, 0.0025), "SM_SC_Panel")
        LAB.plate(d, "inst_crt", (CRT["c"][0], CRT["c"][1] - CRT["h"] / 2 - 0.026, 0.0025), "SM_SC_Panel")
        LAB.plate(d, "inst_hull", (BAR["c"][0], BAR["c"][1] - BAR["h"] / 2 - 0.026, 0.0025), "SM_SC_Panel")
        LAB.plate(d, "unit_degc", (BAR["c"][0], BAR["c"][1] + BAR["h"] / 2 + 0.018, 0.0025), "SM_SC_Panel")
        LAB.plate(d, "nameplate", (0.52, 0.245, 0.0025), "SM_SC_Panel")
        for su in (-1, 1):
            screw(0.52 + su * 0.068, 0.245, 0.0, 0.003)
        LAB.plate(d, "dataplate", (-0.575, 0.105, 0.0025), "SM_SC_Panel")
    o = join([body, d.to_object("panel_details", smooth=None)], "SM_SC_Panel")
    shade(o, 40)
    return o


def dial_uv(dl):
    x, y, w, h = dl["rect"]
    return (x / 1024, 1 - (y + h) / 1024, (x + w) / 1024, 1 - y / 1024)


def needle():
    """The unit needle in its own frame: origin on its axis, the axis = +X toward the hero, pointing +Z, 1 long (the
    game scales it by kit.json "needle_scale" = the dial's radius x the needle's "length")."""
    b = Builder()
    t = 0.02
    # (only what the hero sees: the blade's and the tail's fronts and edges, the hub's front and rim; nothing behind)
    for edge in ([(0.024, 0.1005), (0.0, 1.0), (-0.024, 0.1005)], [(0.034, -0.24), (0.026, -0.1005), (-0.026, -0.1005), (-0.034, -0.24)]):
        b.face([(t, y, z) for y, z in edge], "SC_needle")
        for (ya, za), (yb, zb) in zip(edge, edge[1:] + edge[:1]):
            b.face([(0, ya, za), (t, ya, za), (t, yb, zb), (0, yb, zb)][::-1], "SC_needle")
    b.cyl("SC_dark", (0.0, 0, 0), (0.05, 0, 0), 0.10, n=12, caps=False)
    b.face([(0.05, 0.10 * math.cos(TAU * k / 12), 0.10 * math.sin(TAU * k / 12)) for k in range(12)], "SC_dark")
    return b.to_object("SM_SC_Needle", smooth=30)


def globe_and_ball():
    g = Builder()
    g.sphere("SC_globe", (0, 0, 0), GLOBE["r"], nu=32, nv=16, equirect=True)
    a = Builder()
    a.sphere("SC_ball", (0, 0, 0), BALL["r"], nu=28, nv=14, equirect=True)
    return g.to_object("SM_SC_Globe", smooth=180), a.to_object("SM_SC_Ball", smooth=180)


def toggle():
    """A lever in its own frame: origin on its pivot, the lever along +X (the game tilts it up or down about +Y)."""
    b = Builder()
    b.rev("tx_sc_metal", [(0.0028, 0.0), (0.0030, 0.012), (0.0042, 0.022), (0.0036, 0.026), (0.0, 0.027)], n=10)
    o = b.to_object("SM_SC_Toggle")
    o.data.transform(frame((0, 0, 0), x=(0, 1, 0), z=(1, 0, 0)))
    return o


def launch_parts():
    """The button (own frame: origin the middle of its foot, +X = out of the panel; it travels -X) and its red cover
    (own frame: origin on the hinge, the hinge along +Y, the cover hanging down -Z, +X out of the panel)."""
    b = Builder()
    r = LAUNCH["button_r"]
    with b.at(frame((0, 0, 0), x=(0, 1, 0), z=(1, 0, 0))):
        b.rev("SC_red", [(r, 0.0), (r, 0.010), (r - 0.003, 0.013), (0.0, 0.014)], n=24)
    c = Builder()
    s = LAUNCH["base"] - 0.008
    hgt, th = 0.030, 0.002
    def shell(x0, x1, y, z0, z1, flip):
        F = [[(x0, -y, z0), (x0, -y, z1), (x1, -y, z1), (x1, -y, z0)],          # -y side
             [(x0, y, z0), (x1, y, z0), (x1, y, z1), (x0, y, z1)],              # +y side
             [(x0, -y, z0), (x1, -y, z0), (x1, y, z0), (x0, y, z0)],            # the far end (-z)
             [(x0, -y, z1), (x0, y, z1), (x1, y, z1), (x1, -y, z1)],            # the hinge end
             [(x1, -y, z0), (x1, -y, z1), (x1, y, z1), (x1, y, z0)]]            # the top (+x)
        for q in F:
            c.face(q if flip else q[::-1], "SC_red")
    shell(0.0, hgt, s / 2, -s, 0.0, False)
    shell(0.0, hgt - th, s / 2 - th, -s + th, -th, True)
    for q in ([(0, -s / 2, -s), (0, -s / 2 + th, -s + th), (0, s / 2 - th, -s + th), (0, s / 2, -s)],
              [(0, s / 2, 0), (0, s / 2 - th, -th), (0, -s / 2 + th, -th), (0, -s / 2, 0)],
              [(0, -s / 2, 0), (0, -s / 2 + th, -th), (0, -s / 2 + th, -s + th), (0, -s / 2, -s)],
              [(0, s / 2, -s), (0, s / 2 - th, -s + th), (0, s / 2 - th, -th), (0, s / 2, 0)]):
        c.face(q, "SC_red")
    c.cyl("tx_sc_metal", (0.0, -s / 2 + 0.004, 0.0), (0.0, s / 2 - 0.004, 0.0), 0.003, n=8)
    return b.to_object("SM_SC_LaunchButton"), c.to_object("SM_SC_LaunchCover", smooth=30)


def chute_parts():
    """The parachute handle (own frame: origin where it leaves its base, +X = the pull, the bar along Y) and its cover
    (own frame: origin on the hinge, the hinge along +Y, the cover lying -Z, +X toward the cabin)."""
    b = Builder()
    b.cyl("tx_sc_metal", (0, 0, 0), (0.04, 0, 0), 0.006, n=8)
    b.cyl("SC_red", (0.04, -0.05, 0), (0.04, 0.05, 0), 0.011, n=12)
    for s in (-1, 1):
        b.sphere("SC_red", (0.04, s * 0.05, 0), 0.014, nu=10, nv=6)
    c = Builder()
    x1, y, z0 = 0.085, 0.07, -0.12
    th = 0.003
    def shell(x1_, y_, z0_, z1_, flip, mat):
        F = [[(0, -y_, z0_), (0, -y_, z1_), (x1_, -y_, z1_), (x1_, -y_, z0_)], [(0, y_, z0_), (x1_, y_, z0_), (x1_, y_, z1_), (0, y_, z1_)],
             [(0, -y_, z0_), (x1_, -y_, z0_), (x1_, y_, z0_), (0, y_, z0_)], [(0, -y_, z1_), (0, y_, z1_), (x1_, y_, z1_), (x1_, -y_, z1_)],
             [(x1_, -y_, z0_), (x1_, -y_, z1_), (x1_, y_, z1_), (x1_, y_, z0_)]]
        for q in F:
            c.face(q if flip else q[::-1], mat)
    shell(x1, y, z0, 0.0, False, "SC_red")
    shell(x1 - th, y - th, z0 + th, -th, True, "SC_red")
    for q in ([(0, -y, z0), (0, -y + th, z0 + th), (0, y - th, z0 + th), (0, y, z0)], [(0, y, 0), (0, y - th, -th), (0, -y + th, -th), (0, -y, 0)],
              [(0, -y, 0), (0, -y + th, -th), (0, -y + th, z0 + th), (0, -y, z0)], [(0, y, z0), (0, y - th, z0 + th), (0, y - th, -th), (0, y, 0)]):
        c.face(q, "SC_red")
    for k in range(3):
        c.box("SC_yellow", (x1 + 0.001, 0, z0 + 0.03 + 0.03 * k), (0.002, 2 * y - 0.02, 0.012), skip=("-x",))
    return b.to_object("SM_SC_ChuteHandle"), c.to_object("SM_SC_ChuteCover", smooth=30)


# --- the record player ------------------------------------------------------------------------------------------------------
PLAYER = {"d": 0.30, "w": 0.40, "h": 0.10, "lid_w": 0.36, "lid_t": 0.035, "lid_open": 75.0,
          "platter": (0.005, 0.05), "platter_r": 0.10, "pivot": (-0.095, -0.150), "arm": 0.185,
          "record_r": 0.125, "label_r": 0.045, "lead_in_r": 0.118, "run_out_r": 0.055}
DECK_Z_P = PLAYER["h"] + 0.004       # the deck plate's top over the shelf
PLATTER_TOP = 0.016                   # the platter's top over its own origin
PIVOT_H = 0.034                       # the arm's pivot over the deck


def arm_angle(rho):
    """The arm's angle at its pivot from the line to the spindle with the stylus rho from the spindle, degrees."""
    px, py = PLAYER["pivot"]
    cx, cy = PLAYER["platter"]
    D = math.hypot(cx - px, cy - py)
    L = PLAYER["arm"]
    return math.degrees(math.acos((L * L + D * D - rho * rho) / (2 * L * D)))


ARM_REST = 57.0


def player():
    """The portable player strapped to its shelf (cabin frame): a suitcase base with wooden cheeks, the deck, knobs,
    the open lid with its speaker, two straps. The platter, arm and record are their own pieces."""
    P = PLAYER
    b = Builder()
    with b.at(PLAYER_F):
        d, w, h = P["d"], P["w"], P["h"]
        b.box("SC_case", (0, 0, h / 2), (d, w - 0.04, h), bevel=0.008)
        for s in (-1, 1):
            b.box("SC_case", (0, s * (w / 2 - 0.011), h / 2 + 0.003), (d + 0.006, 0.022, h + 0.006), bevel=0.006)
        b.box("tx_sc_metal", (0, 0, h + 0.002), (d - 0.03, w - 0.07, 0.004), bevel=0.0015)
        # the spindle's boss, the arm's post and rest, two knobs, a handle on the front, corner caps
        cx, cy = P["platter"]
        b.cyl("tx_sc_metal", (cx, cy, h + 0.004), (cx, cy, DECK_Z_P + 0.004), 0.03, n=16)
        px, py = P["pivot"]
        b.cyl("tx_sc_metal", (px, py, h + 0.004), (px, py, DECK_Z_P + PIVOT_H - 0.006), 0.012, n=12)
        to_sp = Vector((cx - px, cy - py, 0)).normalized()
        rest = Matrix.Rotation(math.radians(-ARM_REST), 3, "Z") @ to_sp
        rp = Vector((px, py, 0)) + rest * (P["arm"] * 0.72)
        b.cyl("tx_sc_metal", (rp.x, rp.y, h + 0.004), (rp.x, rp.y, DECK_Z_P + PIVOT_H - 0.014), 0.005, n=8)
        b.box("tx_sc_metal", (rp.x, rp.y, DECK_Z_P + PIVOT_H - 0.014), (0.012, 0.018, 0.006), bevel=0.002)
        for ky in (-0.105, 0.125):
            with b.at(Matrix.Translation((0.125, ky, DECK_Z_P))):
                b.rev("SC_dark", [(0.013, 0.0), (0.012, 0.012), (0.0, 0.013)], n=14)
                b.box("SC_paper", (0.006, 0, 0.0132), (0.01, 0.002, 0.0006), skip=("-z",))
        b.pipe("tx_sc_rubber", [(d / 2, -0.05, 0.055), (d / 2 + 0.022, -0.05, 0.062), (d / 2 + 0.022, 0.05, 0.062), (d / 2, 0.05, 0.055)], 0.007, n=6)
        for s in (-1, 1):
            b.box("tx_sc_metal", (d / 2 + 0.0015, s * 0.05, 0.055), (0.003, 0.03, 0.022), bevel=0.001)
            b.box("tx_sc_metal", (d / 2 + 0.0015, s * 0.12, h - 0.012), (0.004, 0.03, 0.022), bevel=0.001)
        # the lid: hinged along the back's top edge, open; a round speaker in it
        hinge = Matrix.Translation((-d / 2, 0, h)) @ Matrix.Rotation(math.radians(-P["lid_open"]), 4, "Y")
        with b.at(hinge):
            # shut it would lie over the base: x from 0 to d, its inside facing -z; here turned up about the hinge
            lw, lt = P["lid_w"], P["lid_t"]
            b.box("SC_case", (d / 2, 0, lt / 2 + 0.002), (d, lw, lt), bevel=0.008)
            b.box("tx_sc_fabric", (d / 2, 0, 0.0005), (d - 0.03, lw - 0.03, 0.003), skip=("+z",))
            with b.at(frame((d / 2, 0, -0.001), x=(1, 0, 0), z=(0, 0, -1))):
                b.tube("tx_sc_metal", (0, 0), 0.085, 0.094, 0.0, 0.006, n=28, top="tx_sc_metal")
                b.disc("SC_dark", (0, 0, 0.003), 0.085, n=28)
                for k in range(-3, 4):
                    hw = math.sqrt(0.085 ** 2 - (k * 0.022) ** 2)
                    b.box("tx_sc_metal", (0, k * 0.022, 0.0065), (2 * hw, 0.003, 0.002), skip=("-z",))
            for s in (-1, 1):
                b.cyl("tx_sc_metal", (0, s * 0.10 - 0.02, 0), (0, s * 0.10 + 0.02, 0), 0.006, n=8)
        # the stay holding the lid
        st = hinge @ Vector((0.16, P["lid_w"] / 2 + 0.004, 0.004))
        b.pipe("tx_sc_metal", [(-d / 2 + 0.12, P["lid_w"] / 2 + 0.004, h), st], 0.003, n=5)
        # two straps over the cheeks down to the shelf, with buckles
        for s in (-1, 1):
            y = s * (w / 2 - 0.011)
            pts = [(d / 2 + 0.008, h + 0.0085), (-d / 2 - 0.008, h + 0.0085)]
            b.box("tx_sc_fabric", (0, y, h + 0.008), (d + 0.010, 0.026, 0.003))
            for sx in (-1, 1):
                b.box("tx_sc_fabric", (sx * (d / 2 + 0.0065), y, (h + 0.0095 + 0.004) / 2), (0.003, 0.026, h + 0.0095 - 0.004))
                b.box("tx_sc_metal", (sx * (d / 2 + 0.012), y, 0.004), (0.02, 0.034, 0.008), bevel=0.002)
            b.box("tx_sc_metal", (0.06, y, h + 0.011), (0.03, 0.034, 0.005), bevel=0.0015)
            _ = pts
    return b.to_object("SM_SC_Player", smooth=40)


def player_parts():
    """Platter (own frame: origin on its axis at the deck, Z up; it spins about Z), tonearm (origin on its pivot, the
    arm along +X to the stylus, Z up), record (origin the middle of its underside, Z up), sleeve (origin the middle
    of its foot, +X = its front, Z up)."""
    P = PLAYER
    b = Builder()
    r = P["platter_r"]
    b.rev(["tx_sc_metal", "tx_sc_metal", "tx_sc_metal", "tx_sc_rubber", "tx_sc_rubber"],
          [(0.03, 0.004), (r - 0.004, 0.004), (r, 0.006), (r, 0.013), (r - 0.003, PLATTER_TOP), (0.0, PLATTER_TOP)], n=40)
    b.cyl("tx_sc_metal", (0, 0, PLATTER_TOP - 0.004), (0, 0, PLATTER_TOP + 0.010), 0.0036, n=8, r2=0.003)
    platter = b.to_object("SM_SC_Platter")
    a = Builder()
    L = P["arm"]
    a.cyl("tx_sc_metal", (0, 0, -0.008), (0, 0, 0.012), 0.014, n=12)
    a.cyl("tx_sc_metal", (-0.045, 0, 0.004), (L - 0.02, 0, 0.004), 0.004, n=8, r2=0.003)
    a.cyl("SC_dark", (-0.07, 0, 0.004), (-0.035, 0, 0.004), 0.012, n=10)
    a.box("SC_case", (L - 0.008, 0, 0.0), (0.032, 0.018, 0.012), bevel=0.003)
    a.cyl("tx_sc_metal", (L, 0, -0.006), (L, 0, -0.016), 0.0012, n=4, r2=0.0003)
    a.pipe("tx_sc_metal", [(L + 0.008, 0, 0.004), (L + 0.008, -0.022, 0.006)], 0.002, n=4)
    arm = a.to_object("SM_SC_Tonearm")
    rc = Builder()
    R, rl, t = P["record_r"], P["label_r"], 0.002
    for z, up in ((t, True), (0.0, False)):
        rc.disc("SC_record", (0, 0, z), R, n=48, up=up, r0=rl, uv=(0, 0, 1, 1))
        rc.disc("SC_record_label", (0, 0, z), rl, n=32, up=up, uv=(0, 0, 1, 1))
    rc.tube("SC_record", (0, 0), 0, R, 0, t, n=48, inner=False)
    record = rc.to_object("SM_SC_Record", smooth=30)
    s = Builder()
    w, h, th = 0.26, 0.26, 0.004
    s.face([(th / 2, -w / 2, 0), (th / 2, w / 2, 0), (th / 2, w / 2, h), (th / 2, -w / 2, h)], "SC_sleeve", [(0, 0), (1, 0), (1, 1), (0, 1)])
    s.box("SC_paper", (0, 0, h / 2), (th, w, h), skip=("+x",))
    sleeve = s.to_object("SM_SC_Sleeve", smooth=None)
    return platter, arm, record, sleeve


def player_frames():
    P = PLAYER
    cx, cy = P["platter"]
    px, py = P["pivot"]
    platter = PLAYER_F @ Matrix.Translation((cx, cy, DECK_Z_P))
    record = platter @ Matrix.Translation((0, 0, PLATTER_TOP))
    to_sp = math.atan2(cy - py, cx - px)
    def arm(deg):
        return PLAYER_F @ Matrix.Translation((px, py, DECK_Z_P + PIVOT_H)) @ Matrix.Rotation(to_sp - math.radians(deg), 4, "Z")
    rack = [RACK_F @ Matrix.Translation((0.036 + 0.012 * (7 - k), 0, 0)) for k in range(8)]
    return {"platter": platter, "record": record, "arm": arm, "rack": rack}


# --- the landing stage (stage frame) -----------------------------------------------------------------------------------------
def stage():
    b = Builder()
    M, F, W = "tx_sc_metal", "tx_sc_foil", "tx_sc_paint_white"
    V = [math.radians(22.5 + 45 * k) for k in range(8)]
    lo = [polar(TRUSS_R, a, TRUSS_Z0) for a in V]
    hi = [polar(TRUSS_R, a, DECK_Z) for a in V]
    for k in range(8):
        k2 = (k + 1) % 8
        b.cyl(M, lo[k], hi[k], 0.04, n=8, caps=False)
        b.cyl(M, lo[k], lo[k2], 0.035, n=8, caps=False)
        b.cyl(M, hi[k], hi[k2], 0.035, n=8, caps=False)
        mid = math.degrees((V[k] + (V[k2] if k2 else V[k2] + TAU)) / 2) % 360
        leg_side = abs((mid - 45) % 90) < 1 or abs((mid - 45) % 90 - 90) < 1
        if leg_side:
            # foil over the bays the legs stand on (2 cm inside the tubes), a diagonal in front of it
            n = polar(1, math.radians(mid))
            q = [lo[k] - n * 0.03, lo[k2] - n * 0.03, hi[k2] - n * 0.03, hi[k] - n * 0.03]
            b.face(q, F)
            b.face([p - n * 0.008 for p in reversed(q)], F)       # its back, seen through the open bays
            b.cyl(M, lo[k], hi[k2], 0.022, n=6, caps=False)
        for p in (lo[k], hi[k]):
            b.sphere(M, p, 0.055, nu=8, nv=4)
    # the deck and the belly: foil octagons; the cradle ring the shield sits in on eight posts
    for k in range(8):          # (fans from the middle: no eight-sided face under the shield's apex)
        b.face([(0, 0, DECK_Z), hi[k], hi[(k + 1) % 8]], F)
        b.face([(0, 0, DECK_Z - 0.008), hi[(k + 1) % 8] - Vector((0, 0, 0.008)), hi[k] - Vector((0, 0, 0.008))], F)
    b.face([p for p in reversed(lo)], F)
    b.face([p + Vector((0, 0, 0.008)) for p in lo], F)
    ring_z = CABIN_SEAT_Z + (Z_APEX + SHIELD_RS - math.sqrt(SHIELD_RS ** 2 - SEAT_RING_R ** 2)) - 0.04
    b.torus(M, (0, 0, ring_z), (0, 0, 1), SEAT_RING_R, 0.04, n=40, m=8)
    for k in range(8):
        a = math.radians(45 * k)
        b.cyl(M, polar(SEAT_RING_R, a, DECK_Z), polar(SEAT_RING_R, a, ring_z), 0.03, n=8, caps=False)
        b.cyl(M, polar(TRUSS_R * math.cos(math.radians(22.5)) - 0.05, a, DECK_Z + 0.02), polar(SEAT_RING_R, a, ring_z - 0.02), 0.02, n=6)
    # four tanks in the open bays
    for k in range(4):
        a = math.radians(90 * k)
        c = polar(1.02, a, (TRUSS_Z0 + DECK_Z) / 2)
        b.sphere(W, c, 0.50, nu=20, nv=10)
        b.torus(M, c, (0, 0, 1), 0.502, 0.014, n=20, m=4)
    # the engine: a block under the belly and its bell
    b.tube(M, (0, 0), 0.138, 0.26, TRUSS_Z0 - 0.16, TRUSS_Z0, n=28, inner=False, bottom=M)
    bell = [(0.15, TRUSS_Z0 - 0.16), (0.22, TRUSS_Z0 - 0.30), (0.36, TRUSS_Z0 - 0.47), (0.47, TRUSS_Z0 - 0.60), (0.52, TRUSS_Z0 - 0.68)]
    b.rev(M, bell[::-1], n=28)                                                           # the outside
    b.rev("SC_dark", [(r - 0.012, z) for r, z in bell[::-1]], n=28, inward=True)       # the inside, facing the axis
    b.disc("SC_dark", (0, 0, bell[0][1]), bell[0][0] - 0.012, n=28, up=False)
    b.disc(M, (0, 0, bell[-1][1]), bell[-1][0], n=28, up=False, r0=bell[-1][0] - 0.012)
    # four legs: a hub over each pad, two main struts from the bay's lower corners, two from the upper, the pad
    pads = []
    for k in range(4):
        a = math.radians(45 + 90 * k)
        hub = polar(PAD_R - 0.08, a, 0.36)
        i0 = (2 * k) % 8
        i1 = (2 * k + 1) % 8
        for i in (i0, i1):
            b.cyl(M, lo[i], hub, 0.05, n=8, caps=False)
            mid = hi[i].lerp(hub, 0.55)
            b.cyl(M, hi[i], mid, 0.055, n=8)
            b.cyl(M, mid, hub, 0.035, n=8, caps=False)
            b.torus(M, mid, (hub - hi[i]), 0.055, 0.012, n=10, m=4)
        b.sphere(M, hub, 0.10, nu=12, nv=6)
        foot = polar(PAD_R, a, 0.0)
        b.cyl(M, hub, foot + Vector((0, 0, 0.10)), 0.045, n=8, caps=False)
        with b.at(Matrix.Translation(foot)):
            b.rev([M, M, M, F, F], [(0.0, 0.0), (0.34, 0.0), (0.42, 0.06), (0.40, 0.085), (0.12, 0.11), (0.0, 0.12)], n=20)
        pads.append(foot)
    # a dish on a mast at one corner, two small gas bottles and an instrument box on the deck's rim
    m0 = hi[0]
    mt = m0 + Vector((0.18, 0.08, 1.25))
    b.pipe(M, [m0, m0 + Vector((0.10, 0.04, 0.5)), mt], 0.025, n=8)
    aim = Vector((0.75, 0.35, 0.56)).normalized()
    with b.at(frame(mt, y=(0, 0, 1), z=aim)):
        dish = [(0.03, 0.0), (0.16, 0.022), (0.30, 0.075), (0.40, 0.135)]
        b.rev(W, dish, n=24, inward=True)                        # the bowl (seen from where it looks)
        b.rev(M, [(r, z - 0.006) for r, z in dish], n=24)        # its back
        b.disc(M, (0, 0, -0.006), 0.03, n=24, up=False)
        b.disc(W, (0, 0, 0.0), 0.03, n=24)
        for k in range(3):
            a = TAU * k / 3
            b.cyl(M, (0.28 * math.cos(a), 0.28 * math.sin(a), 0.066), (0, 0, 0.30), 0.006, n=5, caps=False)
        b.cyl(M, (0, 0, 0.27), (0, 0, 0.33), 0.03, n=10)
    for k, a in enumerate((math.radians(112), math.radians(-68))):
        c = polar(TRUSS_R - 0.22, a, DECK_Z + 0.17)
        b.sphere(W, c, 0.17, nu=14, nv=8)
        b.cyl(M, c - Vector((0, 0, 0.17)), c - Vector((0, 0, 0.12)), 0.08, n=10)
    with b.at(frame(polar(TRUSS_R - 0.30, math.radians(-22), DECK_Z), x=polar(1, math.radians(-22)), z=(0, 0, 1))):
        b.box(F, (0, 0, 0.13), (0.34, 0.5, 0.26), bevel=0.02)
        b.box(M, (0.172, 0, 0.13), (0.008, 0.3, 0.12), bevel=0.003)
    # the platform at the hatch: a plate on a tube frame, struts back to the truss
    px0, px1, py = -2.50, -1.56, 0.65
    pz = PLATFORM_Z
    b.box(M, ((px0 + px1) / 2, 0, pz - 0.015), (px1 - px0, 2 * py, 0.03), bevel=0.004)
    b.pipe(M, [(px0, -py, pz - 0.03), (px1, -py, pz - 0.03), (px1, py, pz - 0.03), (px0, py, pz - 0.03)], 0.03, n=8, closed=True)
    for s in (-1, 1):
        b.cyl(M, (px0 + 0.05, s * (py - 0.05), pz - 0.05), polar(TRUSS_R, PI - s * math.radians(22.5), DECK_Z), 0.03, n=8, caps=False)
        b.cyl(M, (px1 + 0.02, s * (py - 0.05), pz - 0.05), polar(TRUSS_R, PI - s * math.radians(22.5), DECK_Z), 0.03, n=8, caps=False)
    # a toe rail and a hand rail on the platform's +Y... no: the door swings over that side; a low kerb all round instead
    b.pipe(M, [(px0 + 0.02, py - 0.02, pz + 0.05), (px0 + 0.02, -py + 0.02, pz + 0.05)], 0.012, n=6)
    for y in (-py + 0.02, 0, py - 0.02):
        b.cyl(M, (px0 + 0.02, y, pz), (px0 + 0.02, y, pz + 0.05), 0.01, n=6, caps=False)
    # the ladder: on the platform's -Y side, the climber facing +Y (the ladder component's +X = -Y here)
    lx, ly = LADDER_AT
    top = pz + 1.0
    for s in (-1, 1):
        x = lx + s * LADDER_HALF
        b.pipe(M, [(x, ly, 0.02), (x, ly, top - 0.10), (x, ly + 0.07, top), (x, ly + 0.26, top), (x, ly + 0.33, top - 0.10), (x, ly + 0.33, pz)], 0.02, n=8)
        b.cyl(M, (x, ly, 0.0), (x, ly, 0.02), 0.06, n=10)
        b.cyl(M, (x, ly, pz - 0.04), (x, -py, pz - 0.04), 0.014, n=6, caps=False)
        b.cyl(M, (x, ly, 1.45), (x * 0.86, ly * 0.4 - 0.9, 1.25), 0.016, n=6, caps=False)
    for k in range(LADDER_RUNGS):
        z = LADDER_FIRST + LADDER_STEP * k
        b.cyl("SC_yellow", (lx - LADDER_HALF + 0.015, ly, z), (lx + LADDER_HALF - 0.015, ly, z), RUNG_R, n=8, caps=False)
    return b.to_object("SM_SC_Stage", smooth=40), {"pads": pads, "platform": (px0, px1, py, pz), "ring_z": ring_z}


# --- parachutes (own frame: origin at the cabin's riser fitting, hanging straight up +Z) -----------------------------------
def canopy(name, diameter, height, gores, seg, rows, vent, confluence, cord_r):
    """A round canopy: its skirt `height` over the origin, scalloped gores, a vent; lines from the skirt to a
    confluence point, a riser down to the origin. UV: U 0..1 round (one gore = 1/gores), V 0 at the skirt, 1 at the vent."""
    b = Builder()
    R = diameter * 0.36            # the inflated mouth's radius (a flat circle of that diameter, blown)
    H = diameter * 0.30            # the crown over the skirt
    n = gores * seg
    def P(i, j):
        t = j / rows                           # 0 the skirt, 1 the vent
        ang = (PI / 2) * (vent + (1 - vent) * (1 - t))
        r = R * math.sin(ang) ** 0.85
        z = height + H * math.cos(ang) ** 0.9
        f = (i % seg) / seg
        bulge = 1.0 + 0.045 * math.sin(PI * f) * (1 - t) ** 0.5 * (1 if True else 0)
        scallop = -0.03 * diameter * math.sin(PI * f) * max(0.0, 1 - 4 * t) * 0.0
        return polar(r * bulge, TAU * i / n, z + scallop + (0.02 * diameter * math.sin(PI * f) if j == 0 else 0.0))
    for j in range(rows):
        for i in range(n):
            q = [P(i, j), P(i + 1, j), P(i + 1, j + 1), P(i, j + 1)]
            uv = [(i / n, j / rows), ((i + 1) / n, j / rows), ((i + 1) / n, (j + 1) / rows), (i / n, (j + 1) / rows)]
            b.face(q, "SC_chute", uv)
    conf = Vector((0, 0, confluence))
    for g in range(gores):
        p = P(g * seg, 0)
        b.cyl("SC_cord", conf, p, cord_r, n=3, caps=False)
    b.cyl("SC_cord", (0, 0, 0), conf, cord_r * 2.5, n=5, caps=False)
    b.sphere("SC_cord", conf, cord_r * 5, nu=6, nv=4)
    return b.to_object(name, smooth=60), {"canopy_radius": R, "skirt_z": height, "crown_z": height + H, "confluence_z": confluence, "gores": gores}


# --- where everything goes; kit.json -------------------------------------------------------------------------------------
def ue(p):
    return [round(p[0] * 100, 2), round(-p[1] * 100, 2), round(p[2] * 100, 2)]


def ued(v):
    v = Vector(v).normalized()
    return [round(v[0], 5), round(-v[1], 5), round(v[2], 5)]


def uef(M):
    """A Blender frame as UE: the origin (cm), its axes as unit vectors and the same as a rotator [pitch, yaw, roll].
    (A mesh's own Y is mirrored on import, so the UE Y axis is minus the Blender one.)"""
    X, Y, Z = Vector(ued(M.col[0].xyz)), -Vector(ued(M.col[1].xyz)), Vector(ued(M.col[2].xyz))
    pitch = math.atan2(X.z, math.hypot(X.x, X.y))
    yaw = math.atan2(X.y, X.x)
    sy = Vector((-math.sin(yaw), math.cos(yaw), 0))
    roll = math.atan2(Z.dot(sy), Y.dot(sy))
    return {"at": ue(M.translation), "x": [round(c, 5) for c in X], "y": [round(c, 5) for c in Y], "z": [round(c, 5) for c in Z],
            "rot": [round(math.degrees(a), 3) for a in (pitch, yaw, roll)]}


def n_frame(u, v, n):
    """A frame on the panel whose +X is out of the face, +Z up the face, +Y to the right."""
    return PM @ Matrix.Translation((u, v, n)) @ frame((0, 0, 0), x=(0, 0, 1), z=(0, 1, 0))


def placements(wheel_at):
    """Every own-frame piece's frame(s) in the cabin frame."""
    H = hatch_frame()
    pf = player_frames()
    P = {
        "hatch": H, "hatch_open": rot_about(H, "Z", -110.0), "wheel_in_hatch": wheel_at,
        "glass_big": glass_frame(BIG_AZ, BIG_Z), "glass_side": [glass_frame(SIDE_AZ, SIDE_Z), glass_frame(-SIDE_AZ, SIDE_Z)],
        "couch": [couch_frame(0), couch_frame(1)],
        "thermos": NET_F @ Matrix.Translation((0.080, -0.13, 0.0)),
        "book": NET_F @ frame((0.042, 0.10, BOOK[1] / 2), x=(0, 1, 0), z=(1, 0, 0)),
        "globe": n_frame(*GLOBE["c"], -GLOBE["depth"]), "ball": n_frame(*BALL["c"], -BALL["depth"]),
        "toggle": [n_frame(u, TOGGLE_V, TOGGLE_N) for u in TOGGLE_U],
        "button": n_frame(*LAUNCH["c"], 0.004),
        "launch_cover": n_frame(LAUNCH["c"][0], LAUNCH["c"][1] + (LAUNCH["base"] - 0.008) / 2, 0.0045),
        "chute_handle": CHUTE_F @ Matrix.Translation((0.03, 0, 0)),
        "chute_cover": CHUTE_F @ Matrix.Translation((0.0075, 0, 0.06)),
        "platter": pf["platter"], "record": pf["record"], "arm": pf["arm"], "rack": pf["rack"],
    }
    P["needle"] = {}
    for d in DIALS:
        for nd in d.get("needles", [{"id": "main", "length": 0.90, "lift": 0.0}]):
            P["needle"][d["id"] + "." + nd["id"]] = (n_frame(*d["c"], NEEDLE_N + nd["lift"]), (d["r"] - 0.004) * nd["length"], d)
    return P


def needle_at(F, scale, deg):
    """The needle's frame at an angle (clockwise from 12 o'clock for the hero), scaled."""
    return rot_about(F, "X", -deg) @ Matrix.Scale(scale, 4)


def waterline():
    """Where the sea stands on the cabin afloat (MASS_KG in sea water): z in the cabin frame, the draft, the centre of
    buoyancy."""
    need = MASS_KG / 1025.0
    v = m = 0.0
    z, dz = Z_APEX, 0.001
    while v < need and z < Z_TOP:
        a = PI * r_out(z + dz / 2) ** 2
        v += a * dz
        m += a * dz * (z + dz / 2)
        z += dz
    return z, z - Z_APEX, m / v


def placed(o, M, name=None):
    """A copy of a piece with its mesh moved by M (what the lint and the checks look at)."""
    c = o.copy()
    c.data = o.data.copy()
    c.name = name or o.name + "_at"
    c.data.transform(M)
    bpy.context.scene.collection.objects.link(c)
    return c


def assemble(pieces, P, closed=True):
    """The cabin as the game puts it together (cabin frame): [(what, object)]."""
    out = [("static", pieces[k]) for k in ("outer", "inner", "panel", "player")]
    def add(what, key, M):
        out.append((what, placed(pieces[key], M)))
    add("door", "hatch", P["hatch"] if closed else P["hatch_open"])
    add("door", "wheel", (P["hatch"] if closed else P["hatch_open"]) @ P["wheel_in_hatch"])
    add("glass", "glass_big", P["glass_big"])
    for M in P["glass_side"]:
        add("glass", "glass_side", M)
    for M in P["couch"]:
        add("furniture", "couch", M)
    add("furniture", "blanket", P["couch"][1])
    add("furniture", "cap", P["couch"][1])
    add("prop", "thermos", P["thermos"])
    add("prop", "book", P["book"])
    add("part", "globe", P["globe"])
    add("part", "ball", P["ball"])
    for i, M in enumerate(P["toggle"]):
        add("part", "toggle", rot_about(M, "Y", -(TOGGLE_ON if i in (0, 5) else TOGGLE_OFF)))
    add("part", "button", P["button"])
    add("part", "launch_cover", P["launch_cover"])
    add("part", "chute_handle", P["chute_handle"])
    add("part", "chute_cover", P["chute_cover"])
    add("prop", "platter", P["platter"])
    add("prop", "record", P["record"])
    add("prop", "arm", P["arm"](arm_angle(0.09)))
    for M in P["rack"]:
        add("prop", "sleeve", M)
    for k, (F, s, d) in P["needle"].items():
        a = d["a_min"] + (d["a_max"] - d["a_min"]) * (0.37 if "short" not in k else 0.12)
        add("part", "needle", needle_at(F, s, a))
    return out


def capsule_checks(objs):
    """The character standard's capsule against what stands in the cabin (the door open): clearances in metres
    (negative = it hits). The seat's balls are checked against everything but the hero's own couch."""
    owner = []
    def tree(os_):
        verts, polys = [], []
        del owner[:]
        for o in os_:
            k = len(verts)
            verts += [tuple(o.matrix_world @ v.co) for v in o.data.vertices]
            polys += [tuple(k + i for i in p.vertices) for p in o.data.polygons]
            owner.extend([o.name] * len(o.data.polygons))
        return BVHTree.FromPolygons(verts, polys)
    couch1 = next(o for o in objs if o.name.startswith("SM_SC_Couch"))
    bvh_seat = tree([o for o in objs if o is not couch1])
    bvh = tree(objs)
    def clear(p, r, t=None):
        hit = (t or bvh).find_nearest(Vector(p))
        return (hit[3] if hit and hit[0] is not None else 9.0) - r
    def capsule(x, y, half, lift=0.12):
        zs, z = [], CAPSULE_R + lift
        while z <= 2 * half - CAPSULE_R + 1e-6:
            zs.append(z)
            z += 0.04
        return min(clear((x, y, z), CAPSULE_R) for z in zs)
    out = {}
    if os.environ.get("SCKIT_DEBUG"):
        for x in [-0.75 + 0.05 * k for k in range(14)]:
            best = (9.0, None, 0.0)
            z = CAPSULE_R + 0.12
            while z <= 2 * CAPSULE_HH - CAPSULE_R + 1e-6:
                hit = bvh.find_nearest(Vector((x, 0.0, z)))
                if hit[3] - CAPSULE_R < best[0]:
                    best = (hit[3] - CAPSULE_R, owner[hit[2]], z)
                z += 0.04
            print("SCKIT stand x=%.2f clearance %.3f (%s at z %.2f)" % (x, best[0], best[1], best[2]))
    out["stand"] = capsule(STAND.x, STAND.y, CAPSULE_HH)
    out["stand_5cm_around"] = min(capsule(STAND.x + 0.05 * math.cos(a), STAND.y + 0.05 * math.sin(a), CAPSULE_HH) for a in [TAU * k / 8 for k in range(8)])
    # through the hatch: a ball of the capsule's radius along the hatch's axis (he comes in bent), then crouched to
    # the standing point
    A = axis_frame(HATCH_AZ, HATCH_Z)
    out["hatch_axis_ball"] = min(clear(A @ Vector((0, 0, t)), CAPSULE_R) for t in [0.3 - 0.02 * k for k in range(40)])
    out["hatch_clear_radius"] = min(clear(A @ Vector((0, 0, t)), 0.0) for t in [0.02 - 0.02 * k for k in range(12)])
    x0 = (A @ Vector((0, 0, -0.5))).x
    out["crouched_path"] = min(capsule(x0 + (STAND.x - x0) * k / 10, 0.0, 0.62) for k in range(11))
    # sitting on couch 1: balls at the pelvis, chest and head
    C = couch_frame(0)
    out["seat_pelvis_ball_22cm"] = clear(C @ (SEAT_PELVIS + Vector((0.04, 0, 0.14))), 0.22, bvh_seat)
    out["seat_chest_ball_22cm"] = clear(C @ Vector((-0.07, 0, 0.82)), 0.22, bvh_seat)
    out["seat_head_ball_13cm"] = clear(C @ (SEAT_EYE + Vector((-0.02, 0, 0.0))), 0.13, bvh_seat)
    out["seat_knees_ball_12cm"] = min(clear(C @ Vector((0.48, s * 0.12, 0.50)), 0.12, bvh_seat) for s in (-1, 1))
    out["seat_feet_ball_10cm"] = min(clear(C @ Vector((0.66, s * 0.12, 0.17)), 0.10, bvh_seat) for s in (-1, 1))
    return {k: round(v, 3) for k, v in out.items()}


TWO_SIDED = ("SC_chute", "SC_cord", "SC_porthole", "SC_glass", "SC_fire", "tx_sc_net")     # the game draws these from both sides


def backface_audit(objs, eyes, name, rays=12000):
    """Inverted normals: from each eye, rays every way; the first face a ray meets must face the eye (glass is
    looked through). Returns {object/material: [count, a place]} for what is seen from behind."""
    verts, polys, who, nrm = [], [], [], []
    for o in objs:
        mw = o.matrix_world
        r3 = mw.to_3x3()
        names = [m.name for m in o.data.materials]
        base = o.name.split("_at")[0]
        k = len(verts)
        verts += [tuple(mw @ v.co) for v in o.data.vertices]
        for p in o.data.polygons:
            mat = names[p.material_index] if names else ""
            if mat in ("SC_porthole", "SC_glass", "tx_sc_net"):
                continue
            polys.append(tuple(k + i for i in p.vertices))
            who.append((base, mat))
            nrm.append((r3 @ p.normal).normalized())
    bvh = BVHTree.FromPolygons(verts, polys)
    bad = {}
    ga = PI * (3 - math.sqrt(5))
    for eye in eyes:
        eye = Vector(eye)
        for i in range(rays):
            z = 1 - 2 * (i + 0.5) / rays
            r = math.sqrt(1 - z * z)
            d = Vector((r * math.cos(ga * i), r * math.sin(ga * i), z))
            loc, _, idx, dist = bvh.ray_cast(eye, d)
            if idx is None or who[idx][1] in TWO_SIDED:
                continue
            if nrm[idx].dot(d) > 0.02:
                e = bad.setdefault("%s/%s" % who[idx], [0, [round(c, 2) for c in loc]])
                e[0] += 1
    for k, (n, at) in sorted(bad.items(), key=lambda kv: -kv[1][0])[:25]:
        print("SCKIT backface %s: %s seen from behind by %d rays, e.g. at %s" % (name, k, n, at))
    return {k: v[0] for k, v in bad.items()}


def wall_pokes(objs):
    """Things that must be inside the cabin: how far their furthest vertex is through the inner wall (m; <= 0 is in)."""
    out = {}
    for o in objs:
        worst = -9.0
        for v in o.data.vertices:
            p = o.matrix_world @ v.co
            if p.z < 0.0 or p.z > CEIL:
                worst = max(worst, -p.z - 0.03, p.z - CEIL)       # (a leg's foot may go 3 cm into the floor)
                continue
            lim = r_in(p.z) - (PILLOW if KICK < p.z < Z_KNEE else 0.0)
            worst = max(worst, math.hypot(p.x, p.y) - lim)
        out[o.name] = round(worst, 4)
    return out


def write_kit(pieces, P, info, counts, checks):
    wl, draft, cb = waterline()
    C0 = couch_frame(0)
    eye = C0 @ SEAT_EYE
    look_at = Vector((0.92, 0.0, 1.02))
    A_h = axis_frame(HATCH_AZ, HATCH_Z)
    def porthole(az, z, r):
        A = axis_frame(az, z)
        T = axis_depth(az, z)
        d = A.col[2].xyz
        return {"centre": ue(A @ Vector((0, 0, -GLASS_DEPTH))), "normal": ued(d), "radius": r * 100,
                "glass": uef(glass_frame(az, z)),
                "look": {"camera": ue(A @ Vector((0, 0, -(T + 0.30)))), "direction": ued(d), "interact": ue(A @ Vector((0, 0, -(T + 0.08))))}}
    dials = []
    for d in DIALS:
        x, y, w, h = d["rect"]
        e = {k: d[k] for k in ("id", "min", "max", "a_min", "a_max", "major", "minor") if k in d}
        e.update({"centre": ue(panel_xyz(*d["c"], NEEDLE_N)), "uv": [d["c"][0] * 100, d["c"][1] * 100], "radius": d["r"] * 100,
                  "window_radius": (d["r"] - 0.0015) * 100, "atlas_rect": d["rect"], "atlas_circle_radius_px": w / 2,
                  "atlas_px_per_cm": round(w / 2 / ((d["r"] + 0.003) * 100), 3)})
        if "unit" in d:
            e["unit_plate"] = "unit_" + d["unit"]
        if "note" in d:
            e["note"] = d["note"]
        e["needles"] = []
        for k, (F, s, dd) in P["needle"].items():
            if dd is d:
                nd = {"id": k.split(".")[1], "frame": uef(F), "scale": round(s, 5)}
                for src in d.get("needles", []):
                    if src["id"] == nd["id"]:
                        nd["per_rev"] = src["per_rev"]
                e["needles"].append(nd)
        dials.append(e)
    cells = digit_cells()
    digits = {"cell_size": [DIGIT_W * 100, DIGIT_H * 100], "count": len(cells), "windows": []}
    for wid, (u, v), count, gaps in WINDOWS:
        digits["windows"].append({"id": wid, "digits": count, "first_index": next(g for w_, i, g, _ in cells if w_ == wid and i == 0),
                                  "window_centre": ue(panel_xyz(u, v, 0)), "window_size": [WINDOW_W * 100, WINDOW_H * 100],
                                  "gap_before": list(gaps),
                                  "cells": [ue(panel_xyz(*c, -0.006)) for w_, i, g, c in cells if w_ == wid]})
    lamps = [{"index": i, "id": LAB.lamps[i][5:], "row": r, "col": c, "at": ue(panel_xyz(u, v, -0.0035)),
              "data_uv": [round((c + 0.5) / LAMP_COLS, 4), round((r + 0.5) / LAMP_ROWS, 4)]} for i, r, c, (u, v) in lamp_cells()]
    labels = []
    for pid in LAB.order:
        w, h = LAB.size_m(pid)
        e = {"id": pid, "class": LAB.cls[pid], "rect": LAB.rect[pid], "size_cm": [round(w * 100, 3), round(h * 100, 3)], "placed": []}
        for use in LAB.used.get(pid, []):
            own = use["piece"] == "SM_SC_Hatch"
            e["placed"].append({"piece": use["piece"], "frame": "hatch" if own else "cabin", "at": ue(use["frame_matrix"] @ use["c"])})
        labels.append(e)
    toggles = []
    for i, (tid, F) in enumerate(zip(TOGGLES, P["toggle"])):
        toggles.append({"id": tid, "index": i, "guarded": i < 5, "off": uef(rot_about(F, "Y", -TOGGLE_OFF)), "on": uef(rot_about(F, "Y", -TOGGLE_ON)),
                        "interact": ue(F @ Vector((0.02, 0, 0)))})
    lx, ly = LADDER_AT
    px0, px1, py, pz = info["stage"]["platform"]
    seat = Vector((0, 0, CABIN_SEAT_Z))
    interact = [{"id": "sw_" + t["id"], "at": t["interact"]} for t in toggles]
    interact += [{"id": "launch", "at": ue(P["button"] @ Vector((0.03, 0, 0)))},
                 {"id": "light", "at": toggles[5]["interact"]},
                 {"id": "player", "at": ue(P["record"] @ Vector((0, 0, 0.03)))},
                 {"id": "sleeves", "at": ue(P["rack"][0] @ Vector((0.03, 0, 0.16)))},
                 {"id": "thermos", "at": ue(P["thermos"] @ Vector((0, 0, 0.15)))},
                 {"id": "book", "at": ue(P["book"] @ Vector((0, 0, 0.03)))},
                 {"id": "chute", "at": ue(P["chute_handle"] @ Vector((0.05, 0, 0)))},
                 {"id": "hatch_wheel", "at": ue(P["hatch"] @ P["wheel_in_hatch"] @ Vector((0.02, 0, 0)))},
                 {"id": "couch_1", "at": ue(C0 @ (SEAT_PELVIS + Vector((0.1, 0, 0.05))))},
                 {"id": "couch_2", "at": ue(couch_frame(1) @ (SEAT_PELVIS + Vector((0.1, 0, 0.05))))},
                 {"id": "porthole_big", "at": porthole(BIG_AZ, BIG_Z, BIG_R)["look"]["interact"]},
                 {"id": "porthole_left", "at": porthole(SIDE_AZ, SIDE_Z, SIDE_R)["look"]["interact"]},
                 {"id": "porthole_right", "at": porthole(-SIDE_AZ, SIDE_Z, SIDE_R)["look"]["interact"]}]
    bounds = {}
    for k, o in pieces.items():
        cs = [Vector(c) for c in o.bound_box]
        bounds[o.name] = {"min": [round(min(c[i] for c in cs) * 100, 1) for i in range(3)], "max": [round(max(c[i] for c in cs) * 100, 1) for i in range(3)]}
    kit = {
        "_doc": "Made by blender/spacecraft/spacecraft_kit.py (docs/spacecraft.md; the pieces and every key: blender/spacecraft/KIT_NOTES.md). "
                "UE centimetres. Unless a key says otherwise everything is in the CABIN frame: origin = the middle of the cabin's floor, "
                "+X forward (the panel and the big porthole), the hatch at -X, +Z up, UE +Y = the hero's right when seated (Blender -Y). "
                "A 'frame' is {at, x, y, z, rot}: where a piece's own origin goes, its own axes as unit vectors, and the same as "
                "[pitch, yaw, roll] degrees (FRotator). 'stage.*' is in the STAGE frame: origin on the ice under the stage's axis, "
                "the hatch side at -X; the cabin frame stands at stage.cabin_seat in it, unrotated.",
        "frame": "cabin", "units": "cm",
        "cabin": {
            "inner_radius_floor": R_IN0 * 100, "inner_radius_knee": R_IN1 * 100, "knee_z": Z_KNEE * 100, "ceiling_z": CEIL * 100,
            "outer_radius": R_OUT0 * 100, "top_z": Z_TOP * 100, "shield_apex_z": Z_APEX * 100, "shield_rim_z": [Z_RIM0 * 100, Z_RIM1 * 100],
            "shield_sphere_radius": round(SHIELD_RS * 100, 2), "height": (Z_TOP - Z_APEX) * 100,
            "bounds": {"min": ue((-R_OUT0, R_OUT0, Z_APEX)), "max": ue((R_OUT0, -R_OUT0, Z_TOP + 0.07))},
            "jets": [ue(polar(1.42, math.radians(45 + 90 * k), -0.30)) for k in range(4)],
            "thrusters": [{"at": ue(wall_out(math.radians(a), 0.16, 0.09).translation), "direction": ued(wall_out(math.radians(a), 0.16).col[0].xyz)}
                          for a in (20, 100, 140, 220, 260, 340)],
            "chute_lid": {"at": ue(info["lid"]["at"]), "normal": ued(info["lid"]["normal"]), "radius": info["lid"]["radius"] * 100},
        },
        "seat": {"couch_frames": [uef(M) for M in P["couch"]], "pelvis": ue(C0 @ SEAT_PELVIS), "eyes": ue(eye),
                 "look_direction": ued(look_at - eye), "look_at": ue(look_at), "couch_yaw": uef(C0)["rot"][1],
                 "pelvis_in_couch": ue(SEAT_PELVIS), "eyes_in_couch": ue(SEAT_EYE),
                 "second_couch_pelvis": ue(couch_frame(1) @ SEAT_PELVIS)},
        "stand": {"centre": ue(STAND), "inside_hatch": ue(Vector(((A_h @ Vector((0, 0, -0.5))).x, 0, 0))),
                  "outside_hatch": ue(A_h @ Vector((0, 0, 0.75)) - Vector((0, 0, 0.75))), "capsule": {"radius": CAPSULE_R * 100, "half_height": CAPSULE_HH * 100}},
        "hatch": {"centre": ue(A_h.translation), "axis_out": ued(A_h.col[2].xyz), "clear_radius": HATCH_R * 100, "door_radius": DOOR_R * 100,
                  "closed": uef(P["hatch"]), "open": uef(P["hatch_open"]), "swing_deg": 110.0, "hinge_axis": ued(P["hatch"].col[2].xyz),
                  "door_centre_in_hatch": ue((0, HINGE_OFF, 0)),
                  "wheel": {"in_hatch": uef(P["wheel_in_hatch"]), "closed": uef(P["hatch"] @ P["wheel_in_hatch"]),
                            "axis": ued((P["hatch"] @ P["wheel_in_hatch"]).col[0].xyz), "turns_to_open": 1.5},
                  "sill_z": round((A_h @ Vector((0, -HATCH_R, -axis_depth(HATCH_AZ, HATCH_Z)))).z * 100, 1)},
        "portholes": {"big": porthole(BIG_AZ, BIG_Z, BIG_R), "left": porthole(SIDE_AZ, SIDE_Z, SIDE_R), "right": porthole(-SIDE_AZ, SIDE_Z, SIDE_R)},
        "fire_shell": {k: round(v * 100, 2) for k, v in info["fire"].items()},
        "chute": {"attach": ue(CHUTE_ATTACH), "main": {k: (round(v * 100, 1) if isinstance(v, float) else v) for k, v in info["chute"].items()},
                  "drogue": {k: (round(v * 100, 1) if isinstance(v, float) else v) for k, v in info["drogue"].items()},
                  "handle": {"rest": uef(P["chute_handle"]), "pulled": uef(P["chute_handle"] @ Matrix.Translation((0.08, 0, 0))), "travel": 8.0},
                  "cover": {"closed": uef(P["chute_cover"]), "open": uef(rot_about(P["chute_cover"], "Y", -100.0)), "swing_deg": 100.0}},
        "lamps": [{"lens": ue(l["lens"]), "light": ue(l["light"]), "normal": ued(l["normal"])} for l in info["inner"]["lamps"]],
        "fans": [{"at": ue(f["at"]), "axis": ued(f["axis"]), "radius": f["radius"] * 100} for f in info["inner"]["fans"]],
        "panel": {
            "frame_doc": "origin = the face's bottom middle; u = right for the hero, v = up the face, n = out of the face; 'uv' values are cm in it",
            "origin": ue(PANEL_O), "u": ued(PANEL_U), "v": ued(PANEL_V), "n": ued(PANEL_N), "size": [PANEL_W * 100, PANEL_H * 100],
            "tilt_deg": math.degrees(PANEL_TILT), "centre": ue(panel_xyz(0, PANEL_H / 2)),
            "needle_doc": "a needle's tip at the angle a (degrees clockwise from 12 o'clock as the hero sees it) points along "
                          "cos(a) * panel.v + sin(a) * panel.u; a = a_min + (value - min) / (max - min) * (a_max - a_min). "
                          "SM_SC_Needle goes at needles[].frame (its +X = panel.n), scaled by needles[].scale, turned about its own X.",
            "dials_atlas": {"slot": "SC_dials", "size": 1024, "rect_doc": "[x, y, w, h] px from the top left; the face's circle is inscribed in "
                            "its rect (radius atlas_circle_radius_px = the dial's radius + 3 mm); the bezel shows it out to window_radius"},
            "dials": dials,
            "globe": {"frame": uef(P["globe"]), "radius": GLOBE["r"] * 100, "window_radius": GLOBE["window"] * 100,
                      "doc": "SM_SC_Globe: equirectangular, its own +X (lat 0, lon 0) toward the hero and +Z north at this frame; turn it "
                             "so the point under the craft, own direction (cos lat cos lon, -cos lat sin lon, sin lat) in UE axes, looks along the frame's x"},
            "ball": {"frame": uef(P["ball"]), "radius": BALL["r"] * 100, "window_radius": BALL["window"] * 100,
                     "doc": "SM_SC_Ball: equirectangular (V = pitch -90..90, U round), level flight = this frame"},
            "digits": digits,
            "digits_doc": "SC_digits: one quad per cell, UVMap 0..1 over the cell, UVData.x = (index + 0.5) / count: a count x 1 data "
                          "texture (or that many scalars) tells the material each cell's digit",
            "lamps": {"cols": LAMP_COLS, "rows": LAMP_ROWS, "cell_size": [round(c * 100, 3) for c in LAB.size_m(LAB.lamps[0])], "cells": lamps,
                      "doc": "SC_lamps: one quad per lamp. UVMap = its caption's rect on the label atlas (the material samples "
                             "T_SC_Labels_<lang> with it), UVData = its cell's middle in a cols x rows data texture (row 0 at V = 0 in UE): "
                             "one material instance and a 6 x 4 texture (or 24 scalars) light the lamps"},
            "bar": {"id": "hull", "centre": ue(panel_xyz(*BAR["c"], -0.0055)), "size": [(BAR["w"] + 0.006) * 100, (BAR["h"] + 0.006) * 100],
                    "min": BAR["min"], "max": BAR["max"], "doc": "SC_bar: UV 0..1, V = 0 (UE: 1) at the bottom"},
            "crt": {"centre": ue(panel_xyz(*CRT["c"], -0.022)), "size": [(CRT["w"] + 0.008) * 100, (CRT["h"] + 0.008) * 100], "normal": ued(PANEL_N),
                    "doc": "SC_crt: UV 0..1 over the screen"},
            "speaker": ue(panel_xyz(*SPEAKER["c"], 0)),
            "toggles": toggles, "toggle_doc": "SM_SC_Toggle at 'off' or 'on' (blend the two); index 5 is the cabin light's switch",
            "launch": {"button": uef(P["button"]), "pressed": uef(P["button"] @ Matrix.Translation((-LAUNCH["travel"], 0, 0))), "travel": LAUNCH["travel"] * 100,
                       "cover_closed": uef(P["launch_cover"]), "cover_open": uef(rot_about(P["launch_cover"], "Y", -110.0)), "cover_swing_deg": 110.0},
            "labels": {"slot": "SC_labels", "size": LAB.SIZE, "rect_doc": "[x, y, w, h] px from the top left; size_cm = the plate as printed",
                       "plates": labels},
        },
        "player": {"frame": uef(PLAYER_F), "platter": uef(P["platter"]), "platter_axis": ued((0, 0, 1)), "record": uef(P["record"]),
                   "record_radius": PLAYER["record_r"] * 100, "label_radius": PLAYER["label_r"] * 100,
                   "tonearm": {"pivot": ue(P["arm"](0).translation), "length": PLAYER["arm"] * 100,
                               "rest_deg": ARM_REST, "lead_in_deg": round(arm_angle(PLAYER["lead_in_r"]), 2), "run_out_deg": round(arm_angle(PLAYER["run_out_r"]), 2),
                               "lead_in_radius": PLAYER["lead_in_r"] * 100, "run_out_radius": PLAYER["run_out_r"] * 100,
                               "rest": uef(P["arm"](ARM_REST)), "lead_in": uef(P["arm"](arm_angle(PLAYER["lead_in_r"]))),
                               "run_out": uef(P["arm"](arm_angle(PLAYER["run_out_r"]))), "lift_deg": 4.0,
                               "doc": "the angle is at the pivot between the arm and the line to the spindle; the three frames are the arm "
                                      "at those angles (blend between lead_in and run_out as the side plays)"},
                   "sleeve_rack": [uef(M) for M in P["rack"]], "sleeve_size": [26.0, 26.0, 0.4]},
        "props": {"thermos": uef(P["thermos"]), "book": uef(P["book"]), "blanket_and_cap": uef(P["couch"][1])},
        "interact": interact,
        "afloat": {"mass_kg": MASS_KG, "waterline_z": round(wl * 100, 1), "draft": round(draft * 100, 1), "buoyancy_centre": ue((0, 0, cb)),
                   "hatch_sill_over_water": round(((A_h @ Vector((0, -HATCH_R, 0))).z - wl) * 100, 1)},
        "stage": {
            "frame": "stage", "cabin_seat": ue(seat), "ring_z": round(info["stage"]["ring_z"] * 100, 2), "deck_z": DECK_Z * 100,
            "pads": [ue(p) for p in info["stage"]["pads"]], "pad_radius": 42.0,
            "platform": {"min": ue((px0, py, pz)), "max": ue((px1, -py, pz)), "z": pz * 100},
            "ladder": {"at": ue((lx, ly, 0)), "yaw": 90.0, "facing_doc": "ULyudLadderComponent's frame: its +X (toward the climber) is the stage's UE +Y",
                       "rung_spacing": LADDER_STEP * 100, "first_rung_z": LADDER_FIRST * 100, "rung_count": LADDER_RUNGS,
                       "half_width": LADDER_HALF * 100, "rung_radius": RUNG_R * 100, "top_exit": [-45.0, 0.0, pz * 100],
                       "top_exit_stage": ue((lx, ly + 0.45, pz)), "rails_top_z": (pz + 1.0) * 100},
            "hatch_stand": ue((-1.95, 0.0, pz)), "engine_bell": ue((0, 0, TRUSS_Z0 - 0.68)), "bounds_radius": (PAD_R + 0.42) * 100,
            "height_with_cabin": (CABIN_SEAT_Z + Z_TOP + 0.07) * 100,
        },
        "tiles": {k: round(v * 100, 3) for k, v in TILE.items()},
        "quilt": {"pillow": round(QUILT * 100, 3), "doc": "tx_sc_quilt: one pillow is this square in UV; the seams are at its multiples"},
        "pieces": {o.name: {"triangles": counts[o.name], "bounds": bounds[o.name], "slots": [m.name for m in o.data.materials]} for o in pieces.values()},
        "triangles_total": sum(counts.values()),
        "checks": checks,
    }
    def tidy(v):
        if isinstance(v, float):
            v = round(v, 4)
            return 0.0 if v == 0 else v
        if isinstance(v, dict):
            return {k: tidy(x) for k, x in v.items()}
        if isinstance(v, (list, tuple)):
            return [tidy(x) for x in v]
        return v
    kit = tidy(kit)
    os.makedirs(os.path.dirname(OUT_JSON), exist_ok=True)
    with open(OUT_JSON, "w", encoding="utf-8") as f:
        json.dump(kit, f, indent=1, ensure_ascii=False)
        f.write("\n")
    return kit


# --- previews (Cycles on the CPU: no GPU needed) ---------------------------------------------------------------------------
def preview_textures():
    """Stand-in pictures for the slots the game draws (dial faces from DIALS, plates from the label list with the
    Russian words when tools/spacecraft/strings.json is there): only to look at the kit, nothing is exported."""
    try:
        import numpy as np
        from PIL import Image, ImageDraw, ImageFont
    except ImportError:
        return
    def font(px):
        try:
            return ImageFont.truetype(FONT, px)
        except OSError:
            return ImageFont.load_default()
    def use(mat, im, emit=0.0):
        im = im.convert("RGBA").transpose(Image.FLIP_TOP_BOTTOM)
        img = bpy.data.images.new("prev_" + mat, im.width, im.height, alpha=True)
        img.pixels.foreach_set((np.asarray(im, dtype=np.float32) / 255.0).ravel())
        m = material(mat)
        nt = m.node_tree
        t = nt.nodes.new("ShaderNodeTexImage")
        t.image = img
        bs = nt.nodes.get("Principled BSDF")
        nt.links.new(t.outputs["Color"], bs.inputs["Base Color"])
        if emit:
            nt.links.new(t.outputs["Color"], bs.inputs["Emission Color" if "Emission Color" in bs.inputs else "Emission"])
            bs.inputs["Emission Strength"].default_value = emit
    # dials
    im = Image.new("RGB", (1024, 1024), (10, 10, 10))
    dr = ImageDraw.Draw(im)
    for d in DIALS:
        x, y, w, h = d["rect"]
        cx, cy, R = x + w / 2, y + h / 2, w / 2 * (d["r"] - 0.0035) / (d["r"] + 0.003)
        dr.ellipse([x + 2, y + 2, x + w - 2, y + h - 2], fill=(16, 17, 16))
        span = d["max"] - d["min"]
        k, n = 0, int(round(span / d["minor"]))
        full = abs(d["a_max"] - d["a_min"]) >= 360
        for k in range(n + (0 if full else 1)):
            val = d["min"] + k * d["minor"]
            a = math.radians(d["a_min"] + (d["a_max"] - d["a_min"]) * k / n)
            major = abs(val / d["major"] - round(val / d["major"])) < 1e-6
            r0 = R * (0.80 if major else 0.88)
            dr.line([cx + r0 * math.sin(a), cy - r0 * math.cos(a), cx + R * math.sin(a), cy - R * math.cos(a)], fill=(235, 232, 215), width=max(2, w // (90 if major else 170)))
            if major:
                txt = "%g" % (val if d["id"] != "press" else val / 100)
                f = font(int(w * 0.11))
                r1 = R * 0.64
                tw = dr.textlength(txt, font=f)
                dr.text((cx + r1 * math.sin(a) - tw / 2, cy - r1 * math.cos(a) - w * 0.065), txt, font=f, fill=(235, 232, 215))
    use("SC_dials", im)
    # labels (and the lamps, which show their captions)
    words = {}
    try:
        with open(STRINGS_JSON, encoding="utf-8") as f:
            st = json.load(f)
        with open(LABELS_JSON, encoding="utf-8") as f:
            for pid, v in json.load(f)["plates"].items():
                words[pid] = [st[k]["ru"] for k in v["keys"] if k in st]
    except (OSError, KeyError, ValueError):
        pass
    im = Image.new("RGB", (1024, 1024), (0, 0, 0))
    lm = Image.new("RGB", (1024, 1024), (0, 0, 0))
    dr, dl = ImageDraw.Draw(im), ImageDraw.Draw(lm)
    for pid in LAB.order:
        x, y, w, h = LAB.rect[pid]
        lines = words.get(pid) or [pid.split("_", 1)[-1].upper()]
        lamp = pid.startswith("lamp_")
        big = LAB.cls[pid] in ("name", "data")
        dr.rectangle([x, y, x + w - 1, y + h - 1], fill=(196, 192, 170) if big else (18, 18, 18), outline=(90, 90, 80))
        dl.rectangle([x, y, x + w - 1, y + h - 1], fill=(120, 70, 8) if LAB.lamps.index(pid) % 5 else (150, 20, 10)) if lamp else None
        fs = int(h / len(lines) * (0.62 if not big else 0.6))
        f = font(fs)
        for i, t in enumerate(lines):
            while dr.textlength(t, font=f) > w - 10 and fs > 8:
                fs -= 1
                f = font(fs)
            tw = dr.textlength(t, font=f)
            ty = y + h * (i + 0.5) / len(lines) - fs * 0.72
            dr.text((x + (w - tw) / 2, ty), t, font=f, fill=(20, 20, 20) if big else (232, 228, 205))
            if lamp:
                dl.text((x + (w - tw) / 2, ty), t, font=f, fill=(255, 225, 150))
    use("SC_labels", im)
    use("SC_lamps", lm, emit=1.2)
    # a digit, the bar, the CRT
    im = Image.new("RGB", (64, 104), (12, 3, 0))
    dr = ImageDraw.Draw(im)
    for seg in ([14, 8, 50, 14], [14, 48, 50, 54], [14, 88, 50, 94], [8, 14, 14, 48], [50, 14, 56, 48], [8, 54, 14, 88], [50, 54, 56, 88]):
        dr.rectangle(seg, fill=(255, 110, 20))
    use("SC_digits", im, emit=3.0)
    im = Image.new("RGB", (32, 256), (10, 8, 6))
    dr = ImageDraw.Draw(im)
    dr.rectangle([4, 150, 27, 250], fill=(255, 140, 30))
    for k in range(11):
        dr.line([0, 6 + k * 24.4, 8, 6 + k * 24.4], fill=(220, 220, 200))
    use("SC_bar", im, emit=1.5)
    im = Image.new("RGB", (256, 192), (2, 14, 8))
    dr = ImageDraw.Draw(im)
    dr.ellipse([20, 120, 60, 160], outline=(70, 255, 150), width=2)
    dr.ellipse([150, -40, 330, 140], outline=(70, 255, 150), width=2)
    dr.arc([30, 10, 260, 250], 180, 290, fill=(70, 255, 150), width=2)
    dr.ellipse([96, 40, 104, 48], fill=(200, 255, 220))
    dr.text((12, 170), "ПОЛЁТ  00:07:42", font=font(13), fill=(70, 255, 150))
    use("SC_crt", im, emit=2.5)
    # the globe, the ball, the canopy's stripes, a record's label, a sleeve, the book
    im = Image.new("RGB", (256, 128), (20, 70, 150))
    dr = ImageDraw.Draw(im)
    dr.ellipse([122, 54, 136, 64], fill=(90, 140, 60))
    for k in range(1, 8):
        dr.line([k * 32, 0, k * 32, 128], fill=(40, 95, 170))
    for k in range(1, 4):
        dr.line([0, k * 32, 256, k * 32], fill=(40, 95, 170))
    use("SC_globe", im)
    im = Image.new("RGB", (256, 128), (60, 120, 190))
    dr = ImageDraw.Draw(im)
    dr.rectangle([0, 64, 256, 128], fill=(90, 62, 40))
    dr.line([0, 64, 256, 64], fill=(240, 240, 230), width=2)
    for k in range(-3, 4):
        dr.line([0, 64 + k * 14, 256, 64 + k * 14], fill=(230, 230, 220))
    use("SC_ball", im)
    im = Image.new("RGB", (48, 4), (240, 238, 228))
    dr = ImageDraw.Draw(im)
    for k in range(0, 24, 2):
        dr.rectangle([k * 2, 0, k * 2 + 1, 4], fill=(235, 95, 20))
    use("SC_chute", im)
    im = Image.new("RGB", (128, 128), (170, 40, 30))
    dr = ImageDraw.Draw(im)
    dr.ellipse([10, 10, 118, 118], outline=(240, 220, 170), width=3)
    dr.ellipse([58, 58, 70, 70], fill=(10, 10, 10))
    dr.text((34, 30), "МЕЛОДИЯ", font=font(13), fill=(240, 220, 170))
    use("SC_record_label", im)
    im = Image.new("RGB", (128, 128), (40, 80, 110))
    dr = ImageDraw.Draw(im)
    dr.ellipse([24, 30, 104, 110], fill=(230, 180, 60))
    dr.rectangle([0, 0, 128, 22], fill=(225, 220, 200))
    use("SC_sleeve", im)


def preview(out_dir, pieces, P, info):
    """Renders: the craft on its stage, the cabin from the hatch, the seated view, the panel, the player, a look back,
    the plan from above, the cabin under its canopy."""
    try:
        from PIL import Image
    except ImportError:
        Image = None
    os.makedirs(out_dir, exist_ok=True)
    preview_textures()
    scn = bpy.context.scene
    scn.render.engine = "CYCLES"
    scn.cycles.samples = int(os.environ.get("SCKIT_SAMPLES", "40"))
    scn.cycles.max_bounces = 4
    only = [t for t in os.environ.get("SCKIT_SHOTS", "").split(",") if t]
    if os.environ.get("SCKIT_BACKFACE"):
        # a check, not a picture: every face seen from behind glows magenta (inverted normals show at once)
        for m in bpy.data.materials:
            if not m.use_nodes or m.name in SEE_THROUGH:
                continue
            nt = m.node_tree
            out = next(n for n in nt.nodes if n.type == "OUTPUT_MATERIAL")
            src = out.inputs["Surface"].links[0].from_socket
            geo, mix, em = nt.nodes.new("ShaderNodeNewGeometry"), nt.nodes.new("ShaderNodeMixShader"), nt.nodes.new("ShaderNodeEmission")
            em.inputs["Color"].default_value = (1, 0, 1, 1)
            em.inputs["Strength"].default_value = 3.0
            nt.links.new(geo.outputs["Backfacing"], mix.inputs["Fac"])
            nt.links.new(src, mix.inputs[1])
            nt.links.new(em.outputs["Emission"], mix.inputs[2])
            nt.links.new(mix.outputs["Shader"], out.inputs["Surface"])
    try:
        scn.cycles.use_denoising = True
    except Exception:
        pass
    scn.render.resolution_x, scn.render.resolution_y = 1000, 562
    scn.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("w")
    world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    scn.world = world
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scn.collection.objects.link(cam)
    scn.camera = cam
    cam.data.clip_start = 0.02
    temp = [cam]
    def inst(o, M):
        c = bpy.data.objects.new(o.name + "_i", o.data)
        c.matrix_world = M
        scn.collection.objects.link(c)
        temp.append(c)
        return c
    for o in pieces.values():
        o.hide_render = True
    def cabin_set(door_open, arm_deg=None):
        out = [inst(pieces[k], Matrix.Identity(4)) for k in ("outer", "inner", "panel", "player")]
        H = P["hatch_open"] if door_open else P["hatch"]
        out += [inst(pieces["hatch"], H), inst(pieces["wheel"], H @ P["wheel_in_hatch"]), inst(pieces["glass_big"], P["glass_big"])]
        out += [inst(pieces["glass_side"], M) for M in P["glass_side"]] + [inst(pieces["couch"], M) for M in P["couch"]]
        out += [inst(pieces["blanket"], P["couch"][1]), inst(pieces["cap"], P["couch"][1]), inst(pieces["thermos"], P["thermos"]), inst(pieces["book"], P["book"])]
        out += [inst(pieces["globe"], rot_about(rot_about(P["globe"], "Y", -18), "Z", 25)), inst(pieces["ball"], rot_about(rot_about(P["ball"], "X", 8), "Y", -10))]
        for i, M in enumerate(P["toggle"]):
            out.append(inst(pieces["toggle"], rot_about(M, "Y", -(TOGGLE_ON if i in (0, 1, 5) else TOGGLE_OFF))))
        out += [inst(pieces["button"], P["button"]), inst(pieces["launch_cover"], rot_about(P["launch_cover"], "Y", -35.0)),
                inst(pieces["chute_handle"], P["chute_handle"]), inst(pieces["chute_cover"], rot_about(P["chute_cover"], "Y", -40.0)),
                inst(pieces["platter"], P["platter"]), inst(pieces["record"], P["record"]),
                inst(pieces["arm"], P["arm"](arm_angle(0.09) if arm_deg is None else arm_deg))]
        out += [inst(pieces["sleeve"], M) for M in P["rack"]]
        for k, (F, s, d) in P["needle"].items():
            a = d["a_min"] + (d["a_max"] - d["a_min"]) * (0.37 if "short" not in k else 0.12)
            out.append(inst(pieces["needle"], needle_at(F, s, a)))
        return out
    def light(kind, loc, energy, color=(1, 1, 1), rot=None, size=0.1):
        L = bpy.data.objects.new("L", bpy.data.lights.new("L", kind))
        L.data.energy = energy
        L.data.color = color
        L.location = loc
        if kind == "POINT":
            L.data.shadow_soft_size = size
        if rot:
            L.rotation_euler = rot
        scn.collection.objects.link(L)
        temp.append(L)
        return L
    def shot(name, loc, look, lens=20, clip=0.02, up_hint=None):
        if only and not any(t in name for t in only):
            return
        name = "kit_" + name
        cam.location = loc
        q = (Vector(look) - Vector(loc)).to_track_quat("-Z", "Y")
        cam.rotation_euler = q.to_euler()
        cam.data.lens = lens
        cam.data.clip_start = clip
        path = os.path.join(out_dir, name + ".png")
        scn.render.filepath = path
        bpy.ops.render.render(write_still=True)
        if Image:
            im = Image.open(path).convert("RGB")
            im = im.quantize(colors=200, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG)
            im.save(path, optimize=True)
            if os.path.getsize(path) > 300_000:
                im = Image.open(path).convert("RGB").resize((880, 496), Image.LANCZOS).quantize(colors=128, method=Image.Quantize.MEDIANCUT)
                im.save(path, optimize=True)
    def clear_temp(keep):
        for o in list(temp):
            if o not in keep:
                bpy.data.objects.remove(o, do_unlink=True)
                temp.remove(o)
    # 1, 2: on the stage on Europa's ice: black sky, a low sun, a white ground
    bg.inputs[0].default_value = (0.004, 0.005, 0.008, 1)
    bg.inputs[1].default_value = 1.0
    cabin_set(True)
    inst(pieces["stage"], Matrix.Translation((0, 0, -CABIN_SEAT_Z)))
    gb = Builder()
    gb.disc("tx_sc_paint_white", (0, 0, -CABIN_SEAT_Z), 60.0, n=48)
    temp.append(gb.to_object("ground"))
    light("SUN", (0, 0, 10), 4.0, (1, 0.97, 0.92), (math.radians(62), 0, math.radians(-150)))
    light("SUN", (0, 0, 10), 0.5, (0.6, 0.7, 1.0), (math.radians(70), 0, math.radians(40)))
    for l in info["inner"]["lamps"]:
        light("POINT", l["light"], 30, (1.0, 0.85, 0.65))
    shot("01_on_stage", (-8.2, -7.4, -0.6), (0, 0, -0.4), 30)
    shot("02_hatch_ladder", (-5.4, -2.6, 1.3), (-1.2, -0.1, 0.4), 30)
    clear_temp([cam])
    # 3..7: inside, in flight: the planet ahead through the big porthole, the door shut
    bg.inputs[0].default_value = (0.0, 0.0, 0.002, 1)
    cabin_set(False)
    pl = inst(pieces["globe"], Matrix.Translation((70, 4, 22)) @ Matrix.Scale(520, 4))
    m = bpy.data.materials.new("prev_planet")
    m.use_nodes = True
    bs = m.node_tree.nodes["Principled BSDF"]
    bs.inputs["Base Color"].default_value = (0.03, 0.16, 0.45, 1)
    bs.inputs["Emission Color" if "Emission Color" in bs.inputs else "Emission"].default_value = (0.05, 0.22, 0.6, 1)
    bs.inputs["Emission Strength"].default_value = 0.6
    pl.data = pl.data.copy()
    pl.data.materials.clear()
    pl.data.materials.append(m)
    light("SUN", (0, 0, 10), 5.0, (1, 0.98, 0.95), (math.radians(55), 0, math.radians(200)))
    for l in info["inner"]["lamps"]:
        light("POINT", l["light"], 22, (1.0, 0.86, 0.68), size=0.06)
    A = axis_frame(HATCH_AZ, HATCH_Z)
    C0 = couch_frame(0)
    eye = C0 @ SEAT_EYE
    shot("03_from_hatch", A @ Vector((0, 0, -0.30)), (0.9, 0.0, 0.95), 13)
    shot("04_seated", eye, (0.92, 0.0, 1.02), 15)
    shot("05_panel", panel_xyz(-0.05, 0.27, 0.62), panel_xyz(-0.05, 0.25, 0), 22)
    shot("06_panel_left", panel_xyz(-0.38, 0.32, 0.30), panel_xyz(-0.40, 0.30, 0), 26)
    pc = PLAYER_F @ Vector((0, 0, 0.10))
    shot("07_player", pc + Vector((-0.55, -0.42, 0.42)), pc + Vector((0.02, 0.06, 0.02)), 22)
    shot("08_look_back", (0.55, -0.10, 1.25), (-0.9, 0.15, 0.65), 13)
    shot("09_plan", (0.0, 0.0, 6.0), (0.0, 0.0, 0.0), 52, clip=6.0 - 1.50)
    if os.environ.get("SCKIT_EXTRA"):       # close looks for checking, not kept
        shot("x1_chute_handle", eye + Vector((0.05, -0.1, 0.0)), CHUTE_F.translation, 40)
        shot("x2_hatch_plates", (-0.55, 0.0, 1.25), hatch_frame() @ Vector((-0.04, HINGE_OFF, 0.50)), 50)
        shot("x3_net", (0.05, -0.25, 1.05), NET_F @ Vector((0.1, 0, 0.18)), 24)
        shot("x4_couch2", (-0.35, 0.05, 1.25), couch_frame(1) @ Vector((0.2, 0, 0.45)), 24)
        shot("x5_left", (0.0, -0.2, 1.2), wall_in(SIDE_AZ, 1.15).translation, 16)
        shot("x6_floor", (-0.4, 0.0, 1.6), (0.75, 0.0, 0.2), 16)
    clear_temp([cam])
    # 10: under the canopy over the sea
    bg.inputs[0].default_value = (0.35, 0.55, 0.85, 1)
    cabin_set(False)
    inst(pieces["chute"], Matrix.Translation(CHUTE_ATTACH))
    inst(pieces["drogue"], Matrix.Translation(CHUTE_ATTACH + Vector((9.0, 3.0, 0))))
    light("SUN", (0, 0, 10), 4.0, (1, 0.97, 0.9), (math.radians(50), 0, math.radians(-140)))
    shot("10_chute", (-17, -24, -3.0), (2.0, 0, 9.5), 30)
    clear_temp([])
    for o in pieces.values():
        o.hide_render = False


def check_standard():
    """The capsule here must be CharacterStandard.h's."""
    import re
    path = os.path.join(REPO, "unreal", "Lyudmilovsk", "Source", "Lyudmilovsk", "Public", "CharacterStandard.h")
    try:
        src = open(path, encoding="utf-8").read()
    except OSError:
        return
    r = float(re.search(r"PlayerCapsuleRadius\s*=\s*([\d.]+)", src).group(1)) / 100
    hh = float(re.search(r"PlayerCapsuleHalfHeight\s*=\s*([\d.]+)", src).group(1)) / 100
    if abs(r - CAPSULE_R) > 1e-6 or abs(hh - CAPSULE_HH) > 1e-6:
        raise SystemExit("SCKIT: CharacterStandard.h's capsule is %.2f x %.2f, this script's %.2f x %.2f" % (r, hh, CAPSULE_R, CAPSULE_HH))


def main():
    global LAB
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    prev = argv[argv.index("--preview") + 1] if "--preview" in argv else None
    check_standard()
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    LAB = Labels()
    pieces, info = {}, {}
    skin_out, skin_in = hull()
    pieces["outer"], info["lid"] = cabin_outer(skin_out)
    pieces["inner"], info["inner"] = cabin_inner(skin_in)
    pieces["panel"] = panel()
    pieces["player"] = player()
    pieces["hatch"], pieces["wheel"], wheel_at = hatch_door()
    pieces["glass_big"] = glass("SM_SC_Glass_Big", BIG_R)
    pieces["glass_side"] = glass("SM_SC_Glass_Side", SIDE_R)
    pieces["fire"], info["fire"] = fire_shell()
    pieces["couch"] = couch()
    pieces["blanket"], pieces["cap"] = blanket_and_cap()
    pieces["thermos"] = thermos()
    pieces["book"] = book()
    pieces["needle"] = needle()
    pieces["globe"], pieces["ball"] = globe_and_ball()
    pieces["toggle"] = toggle()
    pieces["button"], pieces["launch_cover"] = launch_parts()
    pieces["chute_handle"], pieces["chute_cover"] = chute_parts()
    pieces["platter"], pieces["arm"], pieces["record"], pieces["sleeve"] = player_parts()
    pieces["stage"], info["stage"] = stage()
    pieces["chute"], info["chute"] = canopy("SM_SC_Chute", 9.0, 14.0, 24, 3, 9, 0.07, 3.2, 0.012)
    pieces["drogue"], info["drogue"] = canopy("SM_SC_Drogue", 1.8, 8.0, 8, 3, 5, 0.12, 5.6, 0.008)
    for o in pieces.values():
        prune_slots(o)
        if "UVData" in o.data.uv_layers and not any(m.name in ("SC_lamps", "SC_digits") for m in o.data.materials):
            o.data.uv_layers.remove(o.data.uv_layers["UVData"])
        o.data.uv_layers.active = o.data.uv_layers["UVMap"]
    bpy.context.view_layer.update()
    counts = {o.name: tris(o) for o in pieces.values()}
    P = placements(wheel_at)

    # glitch checks (blender/railway/kit_lint.py): the cabin as the game assembles it, the stage with the cabin's
    # outside on it, the pieces that are not in the cabin
    sys.path.insert(0, os.path.join(REPO, "blender", "railway"))
    import kit_lint
    asm = assemble(pieces, P)
    bpy.context.view_layer.update()
    lim = int(os.environ.get("SCKIT_LINT_LIMIT", "40"))
    problems = kit_lint.lint([o for _, o in asm], "cabin", limit=lim)
    st = placed(pieces["outer"], Matrix.Translation((0, 0, CABIN_SEAT_Z)))
    problems += kit_lint.lint([pieces["stage"], st], "stage")
    bpy.data.objects.remove(st, do_unlink=True)
    problems += kit_lint.lint([pieces["chute"]], "chute")
    problems += kit_lint.lint([pieces["drogue"]], "drogue")
    problems += kit_lint.lint([pieces["fire"]], "fire")
    # the character standard's capsule in the cabin (door shut: it stands inside), things that must be inside the wall
    solid = [o for what, o in asm if what in ("static", "furniture", "prop")]
    checks = {"capsule_clearance_m": capsule_checks(solid)}
    inside = [o for what, o in asm if what in ("furniture", "prop") or o.name.startswith(("SM_SC_Player", "SM_SC_Chute"))]
    pokes = wall_pokes(inside)
    checks["through_wall_m"] = {k: v for k, v in pokes.items() if v > 0.0}
    # inverted normals: what a ray from where the hero's eyes can be meets must face him
    C0 = couch_frame(0)
    inside_eyes = [C0 @ SEAT_EYE, STAND + Vector((0, 0, 1.62)), axis_frame(HATCH_AZ, HATCH_Z) @ Vector((0, 0, -0.3)), (0.35, 0.0, 0.30),
                   (0.25, 0.55, 1.25), (0.2, -0.5, 1.2), (-0.7, 0.0, 0.6), PLAYER_F @ Vector((0.42, 0.30, 0.50)), PLAYER_F @ Vector((0.5, -0.4, 0.25)), panel_xyz(0.0, 0.26, 0.35), panel_xyz(-0.45, 0.3, 0.2), panel_xyz(0.45, 0.2, 0.2)]
    back = backface_audit([o for _, o in asm], inside_eyes, "inside")
    outside_eyes = [polar(5.5, TAU * k / 8 + 0.2, zz) for k in range(8) for zz in (-1.6, 1.2, 4.5)] + [(0, 0, -2.0)]
    st = placed(pieces["stage"], Matrix.Translation((0, 0, -CABIN_SEAT_Z)))
    asm_open = assemble(pieces, P, closed=False)
    bpy.context.view_layer.update()
    back2 = backface_audit([o for _, o in asm_open] + [st], outside_eyes, "outside")
    for o in [o for _, o in asm_open if "_at" in o.name] + [st]:
        bpy.data.objects.remove(o, do_unlink=True)
    ch = [placed(pieces["chute"], Matrix.Identity(4)), placed(pieces["drogue"], Matrix.Translation((6, 0, 0)))]
    back3 = backface_audit(ch, [(-12, -9, 2), (10, 6, 22), (2, 3, 12), (0, 0, 30)], "chutes", rays=4000)
    for o in ch:
        bpy.data.objects.remove(o, do_unlink=True)
    checks["seen_from_behind"] = {k: v for k, v in {**back, **back2, **back3}.items() if v > 3}
    checks["lint_findings"] = len(problems)
    for _, o in asm:
        if o.name.endswith("_at") or "_at." in o.name:
            bpy.data.objects.remove(o, do_unlink=True)
    print("SCKIT checks: %s" % json.dumps(checks))
    total = sum(counts.values())
    for k, v in sorted(counts.items(), key=lambda kv: -kv[1]):
        print("SCKIT %-22s %7d tris" % (k, v))
    print("SCKIT total %d triangles (budget 150000)" % total)
    if "--strict" in argv and (problems or total > 150000 or checks["through_wall_m"] or checks["seen_from_behind"] or min(checks["capsule_clearance_m"].values()) < 0):
        raise SystemExit("SCKIT: %d glitch findings (KITLINT lines above), %d triangles, checks %s" % (len(problems), total, checks))

    os.makedirs(os.path.dirname(OUT_GLB), exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    for o in pieces.values():
        o.location = (0, 0, 0)
        o.select_set(True)
    kw = dict(filepath=OUT_GLB, use_selection=True, export_format="GLB", export_yup=True, export_apply=True, export_image_format="NONE")
    try:
        bpy.ops.export_scene.gltf(**kw)
    except TypeError:
        kw.pop("export_image_format")
        bpy.ops.export_scene.gltf(**kw)
    bpy.ops.object.select_all(action="DESELECT")
    write_kit(pieces, P, info, counts, checks)
    print("SCKIT wrote %s (%.0f KB) and %s, %d lint findings" % (OUT_GLB, os.path.getsize(OUT_GLB) / 1024, OUT_JSON, len(problems)))
    if prev:
        preview(prev, pieces, P, info)


main()
