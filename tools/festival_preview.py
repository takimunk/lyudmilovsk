#!/usr/bin/env python3
"""Preview renders of the town festival decorations (headless Blender, python bpy).

A PREVIEW TOOL, not a game asset.  It ports the geometry of ALyudFestivalDecor::Build, the fireworks burst
(ALyudFireworks::Setup) and the sky lantern (ALyudSkyLanterns::Setup) from
unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Festival/LyudFestival.cpp, so the look can be judged without the editor.
Everything below works in UE space (cm, X forward, Y right, Z up, yaw degrees, 0 = +X) exactly like the C++; the
points are converted to Blender (m, right-handed: y is mirrored) only when a mesh is emitted.

    python festival_preview.py <scene> <out.png> [--blend out.blend] [--w 1600 --h 900 --samples 64]
    scenes: square_day, square_night, new_year_night, lantern_beach, lunar_new_year_night, halloween_night,
    tanabata_day, toro_nagashi_night, holi_day

The venues are the plan's (city_plan.json, git origin/town-life): the square S-SQUARE (radius 1900 cm, own stage
S-STAGE 5500 cm inland, yaw -4.7), the sea toward +X.  The square is put at the origin of a flat ground.
"""
import sys, math, argparse
import numpy as np
import bpy
from mathutils import Vector

# ----------------------------------------------------------------------------------------------------------------------
# palettes and materials (LyudFestival.cpp top, ALyudFestivalDecor::Mat)

PALETTE = [(0.80, 0.06, 0.05), (0.95, 0.70, 0.05), (0.05, 0.25, 0.75), (0.10, 0.55, 0.15), (0.90, 0.88, 0.82)]
FIRE = [(1.0, 0.62, 0.18), (1.0, 0.12, 0.08), (0.2, 1.0, 0.3), (0.25, 0.45, 1.0), (1.0, 0.95, 0.85), (0.75, 0.3, 1.0),
        (1.0, 0.35, 0.7)]

# emission strength = UE material Intensity * this (an eyeball: Blender's units are not UE's)
GLOW_K = 0.5

PAINT = {  # name -> (colour, roughness, metallic)
    'pole': ((0.18, 0.19, 0.2), 0.45, 0.6), 'rope': ((0.05, 0.05, 0.05), 0.6, 0), 'wood': ((0.32, 0.2, 0.11), 0.8, 0),
    'planks': ((0.42, 0.3, 0.19), 0.75, 0), 'canvas_a': ((0.75, 0.08, 0.06), 0.9, 0),
    'canvas_b': ((0.88, 0.86, 0.8), 0.9, 0), 'canvas_c': ((0.08, 0.38, 0.2), 0.9, 0),
    'backdrop': ((0.03, 0.04, 0.08), 0.95, 0), 'fir': ((0.02, 0.12, 0.05), 0.9, 0), 'straw': ((0.62, 0.5, 0.24), 0.95, 0),
    # placeholders (not in the C++)
    'ph_concrete': ((0.32, 0.32, 0.33), 0.9, 0), 'ph_pave': ((0.22, 0.21, 0.20), 0.95, 0),
    'ph_water': ((0.04, 0.12, 0.2), 0.12, 0), 'ph_ground': ((0.10, 0.14, 0.07), 1.0, 0),
    'ph_sand': ((0.62, 0.52, 0.36), 1.0, 0),
    # ALyudFestivalDecor::Mat, the newer kinds
    'gold': ((0.85, 0.6, 0.15), 0.35, 0.9), 'clay': ((0.45, 0.18, 0.08), 0.9, 0),
    'marigold_0': ((0.98, 0.62, 0.03), 0.85, 0), 'marigold_1': ((0.95, 0.42, 0.02), 0.85, 0),
    'pumpkin': ((0.85, 0.32, 0.03), 0.6, 0), 'bamboo': ((0.32, 0.45, 0.12), 0.55, 0),
    'leaf': ((0.12, 0.32, 0.05), 0.8, 0),
    # ALyudFirecrackers::Setup's own paints, ALyudSkyLanterns' float, ALyudColorPowder (rough 1)
    'cracker': ((0.75, 0.04, 0.03), 0.7, 0), 'fc_pole': ((0.18, 0.19, 0.2), 0.5, 0.6), 'fc_rope': ((0.05, 0.05, 0.05), 0.5, 0),
    'wood_float': ((0.3, 0.19, 0.1), 0.8, 0),
}
POWDER = [(0.95, 0.1, 0.45), (1.0, 0.75, 0.05), (0.1, 0.75, 0.2), (0.15, 0.35, 0.95), (0.6, 0.1, 0.85), (1.0, 0.4, 0.05)]
for _i, _c in enumerate(POWDER):
    PAINT['powder_%d' % _i] = (_c, 1.0, 0)
for _i, _c in enumerate(PALETTE):
    PAINT['flag_%d' % _i] = (_c, 0.85, 0)
    PAINT['balloon_%d' % _i] = (_c, 0.25, 0)


def glow_spec(name, night):
    """(colour, UE intensity) of a glow material at night (1) or by day (0): Mat() and SetLights()."""
    c, base, mn = (1.0, 0.72, 0.38), 6.0, 0.0
    idx = int(name[-1]) if name[-1].isdigit() else 0
    if name.startswith('glow_c'):
        c, base = FIRE[idx % 7], 8.0
    elif name.startswith('glow_paper'):
        c, base, mn = ((1.0, 0.25, 0.08) if idx % 2 else (1.0, 0.55, 0.15)), 3.0, 0.25
    elif name == 'glow_star':
        c, base, mn = (1.0, 0.85, 0.4), 25.0, 0.1
    elif name == 'glow_red':
        c, base, mn = (1.0, 0.08, 0.04), 4.0, 0.35
    elif name == 'glow_pumpkin':
        c, base = (1.0, 0.45, 0.08), 6.0
    elif name == 'glow_candle':     # NB: unreachable in the C++ too: 'glow_c' matches first (see the report)
        c, base = (1.0, 0.6, 0.25), 4.0
    elif name.startswith('glow_stage'):
        c = FIRE[idx % 7]
        W = 0.5 + 0.5 * math.sin(1.0 * 2.6 + idx * 0.9)   # the Tick() chase, frozen at one moment
        return c, (4.0 + 40.0 * W * W) if night else 0.0
    return c, base * (mn + (1.0 - mn) * night)


# ----------------------------------------------------------------------------------------------------------------------
# UE maths

def V(x=0.0, y=0.0, z=0.0):
    return np.array([x, y, z], dtype=float)


UP = V(0, 0, 1)


def rot_matrix(pitch=0.0, yaw=0.0, roll=0.0):
    """FRotationMatrix: columns are the X, Y, Z axes."""
    sp, cp = math.sin(math.radians(pitch)), math.cos(math.radians(pitch))
    sy, cy = math.sin(math.radians(yaw)), math.cos(math.radians(yaw))
    sr, cr = math.sin(math.radians(roll)), math.cos(math.radians(roll))
    X = V(cp * cy, cp * sy, sp)
    Y = V(sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp)
    Z = V(-(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp)
    return np.stack([X, Y, Z], axis=1)


class Xf:
    """FTransform(rotation, location)."""
    def __init__(self, rot=None, loc=None):
        self.R = np.eye(3) if rot is None else rot
        self.t = V() if loc is None else np.asarray(loc, float)

    def pos(self, p):
        return self.R @ np.asarray(p, float) + self.t


def yaw_vec(deg):
    return V(math.cos(math.radians(deg)), math.sin(math.radians(deg)), 0)


def yaw_of(v):
    return math.degrees(math.atan2(v[1], v[0]))


def norm2d(v):
    v = V(v[0], v[1], 0)
    n = np.linalg.norm(v)
    return v / n if n > 1e-8 else v


def cross(a, b):
    return np.cross(a, b)


def make_from_z(z):
    """FRotationMatrix::MakeFromZ: a frame whose Z is z."""
    z = z / np.linalg.norm(z)
    up = V(1, 0, 0) if abs(z[2]) > 0.999 else V(0, 0, 1)
    y = cross(z, up)
    y /= np.linalg.norm(y)
    x = cross(y, z)
    return np.stack([x, y, z], axis=1)


class Rand:
    """Rand01 (xorshift32), the seeded generator the decor uses."""
    def __init__(self, seed):
        self.s = (seed | 1) & 0xFFFFFFFF

    def __call__(self):
        s = self.s
        s ^= (s << 13) & 0xFFFFFFFF
        s ^= s >> 17
        s ^= (s << 5) & 0xFFFFFFFF
        self.s = s
        return (s & 0xFFFFFF) / float(0x1000000)


def hang(a, b, sag, t):
    return a + (b - a) * t - V(0, 0, 4.0 * sag * t * (1.0 - t))


# ----------------------------------------------------------------------------------------------------------------------
# the mesh kit (FLyudMeshKit): triangle soup per material, in UE cm

class Kit:
    def __init__(self):
        self.tris = {}

    def _add(self, m, pts):
        self.tris.setdefault(m, []).append(pts)

    def tri(self, m, a, b, c):
        self._add(m, (a, b, c))

    def quad(self, m, a, b, c, d):
        self._add(m, (a, b, c))
        self._add(m, (a, c, d))

    def box(self, m, T, h, top=None):
        hx, hy, hz = h

        def P(x, y, z):
            return T.pos(V(x * hx, y * hy, z * hz))
        q = self.quad
        q(top or m, P(-1, -1, 1), P(1, -1, 1), P(1, 1, 1), P(-1, 1, 1))
        q(m, P(-1, 1, -1), P(1, 1, -1), P(1, -1, -1), P(-1, -1, -1))
        q(m, P(1, -1, -1), P(1, 1, -1), P(1, 1, 1), P(1, -1, 1))
        q(m, P(-1, 1, -1), P(-1, -1, -1), P(-1, -1, 1), P(-1, 1, 1))
        q(m, P(1, 1, -1), P(-1, 1, -1), P(-1, 1, 1), P(1, 1, 1))
        q(m, P(-1, -1, -1), P(1, -1, -1), P(1, -1, 1), P(-1, -1, 1))

    def box_in(self, m, F, centre, size, top=None):
        self.box(m, Xf(F.R, F.pos(centre)), np.asarray(size, float) * 0.5, top)

    def prism(self, m, F, poly, z0, z1):
        n = len(poly)

        def P(i, z):
            p = poly[i % n]
            return F.pos(V(p[0], p[1], z))
        for i in range(1, n - 1):
            self.tri(m, P(0, z1), P(i, z1), P(i + 1, z1))
            self.tri(m, P(0, z0), P(i + 1, z0), P(i, z0))
        for i in range(n):
            self.quad(m, P(i, z0), P(i + 1, z0), P(i + 1, z1), P(i, z1))

    def rod(self, m, a, b, r, n=6, caps=False):
        a, b = np.asarray(a, float), np.asarray(b, float)
        d = b - a
        if np.linalg.norm(d) < 1e-4:
            return
        R = make_from_z(d)
        ang = [2 * math.pi * i / n for i in range(n + 1)]
        ra = [a + R @ V(math.cos(t) * r, math.sin(t) * r, 0) for t in ang]
        rb = [b + R @ V(math.cos(t) * r, math.sin(t) * r, 0) for t in ang]
        for i in range(n):
            self.quad(m, ra[i], ra[i + 1], rb[i + 1], rb[i])
            if caps:
                self.tri(m, a, ra[i + 1], ra[i])
                self.tri(m, b, rb[i], rb[i + 1])

    def lathe(self, F, profile, mats, n=8):
        """profile: [(r, z)], one material per segment."""
        for k in range(len(profile) - 1):
            m = mats[min(k, len(mats) - 1)]
            (r0, z0), (r1, z1) = profile[k], profile[k + 1]
            for i in range(n):
                a0, a1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1) / n

                def p(r, z, a):
                    return F.pos(V(math.cos(a) * r, math.sin(a) * r, z))
                self.quad(m, p(r0, z0, a0), p(r0, z0, a1), p(r1, z1, a1), p(r1, z1, a0))


def ball(kit, mat, centre, R, H, sides=8):
    """Ball(): an ellipsoid of radius R and height H centred on `centre`."""
    N = 6
    prof = [(math.sin(math.pi * i / N) * R, H * 0.5 - math.cos(math.pi * i / N) * H * 0.5) for i in range(N + 1)]
    kit.lathe(Xf(None, centre - V(0, 0, H * 0.5)), prof, [mat], sides)


# ----------------------------------------------------------------------------------------------------------------------
# ALyudFestivalDecor::Build

class Venue:
    def __init__(self, name, center, radius, yaw, sea_dir, plan_stage=None, plan_stalls=False):
        self.name, self.center, self.radius, self.yaw = name, center, radius, yaw
        self.sea_dir = sea_dir
        self.stage_at = plan_stage
        self.plan_stage = plan_stage is not None
        self.plan_stalls = plan_stalls


class DecorResult:
    def __init__(self):
        self.solid, self.glow = Kit(), Kit()
        self.lights = []          # (pos, colour, UE candela)
        self.tops = []


def ground_at(p):
    return V(p[0], p[1], 0.0)    # flat ground


def build(Vn, kinds, seed):
    out = DecorResult()
    K, Glow = out.solid, out.glow
    C, R = Vn.center, Vn.radius
    fwd = yaw_vec(Vn.yaw)
    b_stage, b_tree = 'stage' in kinds, 'tree' in kinds
    rng = Rand(seed)

    b_ring = any(k in kinds for k in ('bunting', 'lanterns', 'wreaths', 'red_lanterns', 'marigolds', 'balloons'))
    NPOLES, POLE_H = 6, 520.0
    b_line = Vn.name in ('promenade', 'beach', 'pier')
    b_pier = Vn.name == 'pier'
    along = norm2d(cross(UP, Vn.sea_dir))
    line_dir = Vn.sea_dir if b_pier else along
    line_from = (C - Vn.sea_dir * R * 3.2 + along * 180.0) if b_pier else (C - line_dir * R * 1.6)
    NLINE = 8
    tops = out.tops
    if b_ring:
        for i in range(NLINE if b_line else NPOLES):
            A = Vn.yaw + 30.0 + i * 360.0 / NPOLES
            base = (ground_at(line_from + line_dir * (i * R * 3.2 / (NLINE - 1))) if b_line
                    else ground_at(C + yaw_vec(A) * R * 0.95))
            K.rod('pole', base - V(0, 0, 30), base + V(0, 0, POLE_H), 6.0, 8, True)
            ball(K, 'pole', base + V(0, 0, POLE_H + 6), 9.0, 12.0)
            tops.append(base + V(0, 0, POLE_H - 10))
    strings = [(tops[i], tops[(i + 1) % len(tops)]) for i in range(len(tops) - (1 if b_line else 0))]
    if not b_line:
        strings += [(tops[i], tops[i + len(tops) // 2]) for i in range(len(tops) // 2)]
    ntops = len(tops)

    if 'bunting' in kinds:
        for si, (A, B_) in enumerate(strings):
            Ln = np.linalg.norm(B_ - A)
            sag = min(Ln * (0.06 if si < ntops else 0.09), 140.0 if si < ntops else 90.0)
            N = max(4, math.ceil(Ln / 150.0))
            prev = A
            for i in range(1, N + 1):
                p = hang(A, B_, sag, i / N)
                K.rod('rope', prev, p, 0.6, 4)
                prev = p
            NF = int(Ln // 42.0)
            for i in range(1, NF):
                t0, t1 = (i - 0.36) / NF, (i + 0.36) / NF
                p0, p1 = hang(A, B_, sag, t0), hang(A, B_, sag, t1)
                tip = (p0 + p1) * 0.5 - V(0, 0, 34) + V(rng() - 0.5, rng() - 0.5, 0) * 6.0
                m = 'flag_%d' % ((i + si) % 5)
                K.tri(m, p0, p1, tip)
                K.tri(m, p1, p0, tip)

    if 'lanterns' in kinds:
        for si, (A0, B0) in enumerate(strings):
            A, B_ = A0 - V(0, 0, 40), B0 - V(0, 0, 40)
            Ln = np.linalg.norm(B_ - A)
            sag = min(Ln * (0.08 if si < ntops else 0.11), 160.0 if si < ntops else 110.0)
            N = max(4, math.ceil(Ln / 150.0))
            prev = A
            for i in range(1, N + 1):
                p = hang(A, B_, sag, i / N)
                K.rod('rope', prev, p, 0.5, 4)
                prev = p
            NB = int(Ln // 55.0)
            for i in range(1, NB):
                p = hang(A, B_, sag, i / NB) - V(0, 0, 5)
                if i % 5 == 0:
                    K.rod('rope', p, p - V(0, 0, 18), 0.4, 4)
                    ball(Glow, 'glow_paper_%d' % ((i // 5) % 2), p - V(0, 0, 40), 17.0, 38.0, 10)
                else:
                    Glow.box('glow_c%d' % ((i + si) % 5), Xf(None, p), V(2.6, 2.6, 3.5))

    if 'wreaths' in kinds:
        for T in tops:
            In = norm2d(C - T)
            At = T - V(0, 0, 200) + In * 12.0
            prof = [(32.0 + math.cos(2 * math.pi * i / 8) * 9.0, math.sin(2 * math.pi * i / 8) * 9.0) for i in range(9)]
            face = make_from_z(In)
            K.lathe(Xf(face, At), prof, ['fir'], 12)
            K.box('flag_0', Xf(face, At - V(0, 0, 32) + In * 8.0), V(10, 4, 6))

    if 'flags' in kinds:
        for i in range(6):
            A = Vn.yaw + 180.0 + (i - 2.5) * 14.0
            base = ground_at(C + yaw_vec(A) * R * 1.05)
            H = 850.0
            K.rod('pole', base - V(0, 0, 40), base + V(0, 0, H), 5.0, 8, True)
            al = yaw_vec(A + 90.0)
            t0 = base + V(0, 0, H - 15)
            t1 = t0 - V(0, 0, 110)
            m = 'flag_%d' % (i % 5)
            off = V(al[1], -al[0], 0) * 12.0
            mid0, mid1 = t0 + al * 85 + off, t1 + al * 85 + off
            e0, e1 = t0 + al * 170, t1 + al * 170
            K.quad(m, t1, mid1, mid0, t0)
            K.quad(m, t0, mid0, mid1, t1)
            K.quad(m, mid1, e1, e0, mid0)
            K.quad(m, mid0, e0, e1, mid1)

    stage_dir = norm2d(Vn.stage_at - C) if Vn.plan_stage else fwd
    if b_stage and Vn.plan_stage:
        front = norm2d(C - Vn.stage_at)
        side = cross(UP, front)
        edge = ground_at(Vn.stage_at + front * 420.0)
        frot = rot_matrix(0, yaw_of(front), 0)
        for i in range(12):
            L = edge + side * (-550.0 + i * 100.0) + V(0, 0, 12)
            K.box('pole', Xf(frot, L - V(0, 0, 6)), V(10, 12, 6))
            Glow.box('glow_stage%d' % (i % 7), Xf(frot, L + V(0, 0, 6)), V(4, 9, 6))
        for y in (-350.0, 350.0):
            out.lights.append((Vn.stage_at + front * 500.0 + side * y + V(0, 0, 450), (1.0, 0.6, 0.3), 900.0))
    elif b_stage:
        S = ground_at(C + fwd * R * 0.65)
        F = Xf(rot_matrix(0, Vn.yaw + 180.0, 0), S)
        K.box_in('planks', F, V(0, 0, 55), V(500, 900, 110))
        K.box_in('planks', F, V(310, 0, 18), V(60, 180, 36))
        K.box_in('planks', F, V(280, 0, 55), V(60, 180, 36))
        K.box_in('planks', F, V(250, 0, 90), V(60, 180, 30))
        TH = 560.0
        for y in (-440.0, 440.0):
            for x in (-230.0, 230.0):
                K.rod('pole', F.pos(V(x, y, 110)), F.pos(V(x, y, 110 + TH)), 5.0, 6, True)
            K.rod('pole', F.pos(V(-230, y, 110 + TH)), F.pos(V(230, y, 110 + TH)), 4.0, 6, True)
        for x in (-230.0, 230.0):
            K.rod('pole', F.pos(V(x, -440, 110 + TH)), F.pos(V(x, 440, 110 + TH)), 4.0, 6, True)
        K.box_in('backdrop', F, V(-235, 0, 110 + TH * 0.5), V(4, 880, TH - 20))
        for i in range(10):
            L = F.pos(V(225, -405 + i * 90, 110 + TH - 18))
            Glow.box('glow_stage%d' % (i % 7), Xf(F.R, L), V(9, 9, 9))
        for y in (-300.0, 300.0):
            out.lights.append((F.pos(V(300, y, 500)), (1.0, 0.6, 0.3), 900.0))

    if 'stalls' in kinds and not Vn.plan_stalls:
        if b_stage or b_tree:
            away = yaw_of(-stage_dir)
            angles = [away - 55.0 + i * 27.5 for i in range(5)]
        else:
            angles = [Vn.yaw + i * 45.0 for i in range(8) if i != 4]
        for si, ang in enumerate(angles):
            d = yaw_vec(ang)
            base = ground_at(C + d * R * 0.78)
            F = Xf(rot_matrix(0, yaw_of(-d), 0), base)
            K.box_in('planks', F, V(0, 0, 50), V(90, 240, 100))
            K.box_in('canvas_b', F, V(2, 0, 101), V(96, 246, 3))
            for x in (-110.0, 50.0):
                for y in (-118.0, 118.0):
                    K.rod('wood', F.pos(V(x, y, 0)), F.pos(V(x, y, 250 if x < 0 else 225)), 3.5, 6, True)
            K.box_in('planks', F, V(-110, 0, 150), V(4, 236, 200))
            ca = 'canvas_a' if si % 2 else 'canvas_c'
            NS = 8
            for i in range(NS):
                y0, y1 = -130.0 + 260.0 * i / NS, -130.0 + 260.0 * (i + 1) / NS
                m = 'canvas_b' if i % 2 else ca
                a0, a1 = F.pos(V(-125, y0, 262)), F.pos(V(-125, y1, 262))
                b0, b1 = F.pos(V(75, y0, 228)), F.pos(V(75, y1, 228))
                K.quad(m, a0, b0, b1, a1)
                K.quad(m, a1, b1, b0, a0)
                dd = F.pos(V(78, (y0 + y1) * 0.5, 200))
                K.tri(m, b0, b1, dd)
                K.tri(m, b1, b0, dd)
            Glow.box('glow_warm', Xf(None, F.pos(V(-10, 0, 215))), V(5, 5, 6))

    if b_tree:
        if Vn.plan_stage:
            dist = np.linalg.norm(V(Vn.stage_at[0] - C[0], Vn.stage_at[1] - C[1], 0))
            want = 900.0 + (dist - 900.0) * 0.4
            td = min(max(want, 1150.0), max(1150.0, R * 0.72))
            ang = math.degrees(math.atan2(stage_dir[1], stage_dir[0]))
            snapped = math.radians(Vn.yaw + math.floor((ang - Vn.yaw) / 60.0 + 0.5) * 60.0)   # RoundToFloat
            T = ground_at(C + V(math.cos(snapped), math.sin(snapped), 0) * td)
        else:
            T = ground_at(C - fwd * R * 0.55)
        H = 950.0
        K.rod('wood', T, T + V(0, 0, 120), 16.0, 8, True)
        tiers = 6
        for i in range(tiers):
            z0 = 100.0 + i * (H - 150.0) / tiers
            z1 = z0 + (H - 100.0) / tiers * 1.5
            r0 = 260.0 * (1.0 - i / (tiers + 0.5))
            r1 = r0 * 0.18
            K.lathe(Xf(None, T), [(0, z0), (r0, z0 + 20), (r1, z1), (0, z1 + 5)], ['fir'], 14)
        for i in range(140):
            u = i / 140
            z = 130.0 + u * (H - 180.0)
            rad = 255.0 * (1.0 - u) * 0.92 + 15.0
            a = u * 2 * math.pi * 7
            Glow.box('glow_c%d' % (i % 5), Xf(None, T + V(math.cos(a) * rad, math.sin(a) * rad, z)), V(3.5, 3.5, 3.5))
        top = T + V(0, 0, H + 30)
        outv = norm2d(C - T)
        side = cross(UP, outv)
        for i in range(5):
            a0 = 2 * math.pi * i / 5
            a1, a2 = a0 + math.pi / 5, a0 - math.pi / 5

            def P(a, rr):
                return top + (side * math.sin(a) + UP * math.cos(a)) * rr
            for (p, q) in ((P(a1, 22), P(a0, 55)), (P(a0, 55), P(a2, 22))):
                Glow.tri('glow_star', top, p, q)
                Glow.tri('glow_star', top, q, p)
        out.lights.append((T + V(0, 0, 400) + outv * 300, (1.0, 0.75, 0.45), 900.0))

    if 'candles' in kinds:
        Ln = R * 2.4
        for off in (-260.0, 260.0):
            s = -Ln * 0.5
            while s <= Ln * 0.5:
                P = ground_at(C + along * s + Vn.sea_dir * off)
                K.box('canvas_b', Xf(rot_matrix(0, rng() * 90.0, 0), P + V(0, 0, 13)), V(9, 7, 13))
                Glow.box('glow_candle', Xf(None, P + V(0, 0, 8)), V(2.5, 2.5, 6))
                s += 130.0

    if 'birdhouses' in kinds:
        for i in range(6):
            d = yaw_vec(Vn.yaw + 15.0 + i * 60.0)
            base = ground_at(C + d * R * 0.6)
            face = yaw_of(-d)
            F = Xf(rot_matrix(0, face, 0), base)
            K.rod('wood', base - V(0, 0, 20), base + V(0, 0, 240), 4.0, 6, True)
            K.box_in('planks', F, V(0, 0, 260), V(22, 22, 32))
            K.prism('wood', Xf(rot_matrix(0, face, 90), base + V(0, 0, 291)), [(-16, -15), (16, -15), (0, 4)], -17.0, 17.0)
            K.box_in('backdrop', F, V(11.5, 0, 266), V(1, 6, 6))

    if 'red_lanterns' in kinds:
        for si, (A0, B0) in enumerate(strings):
            A, B_ = A0 - V(0, 0, 20), B0 - V(0, 0, 20)
            Ln = np.linalg.norm(B_ - A)
            sag = min(Ln * (0.07 if si < ntops else 0.1), 150.0 if si < ntops else 100.0)
            if 'bunting' not in kinds:
                N = max(4, math.ceil(Ln / 150.0))
                prev = A
                for i in range(1, N + 1):
                    p = hang(A, B_, sag, i / N)
                    K.rod('rope', prev, p, 0.6, 4)
                    prev = p
            NL = max(2, int(Ln // 340.0))
            for i in range(1, NL):
                p = hang(A, B_, sag, i / NL)
                mid = p - V(0, 0, 62)
                K.rod('rope', p, mid + V(0, 0, 26), 0.5, 4)
                ball(Glow, 'glow_red', mid, 30.0, 44.0, 12)
                K.rod('gold', mid + V(0, 0, 20), mid + V(0, 0, 27), 12.0, 10, True)
                K.rod('gold', mid - V(0, 0, 27), mid - V(0, 0, 20), 12.0, 10, True)
                K.rod('canvas_a', mid - V(0, 0, 27), mid - V(0, 0, 58), 2.2, 5, True)

    if 'marigolds' in kinds:
        for si in range(min(len(strings), ntops)):
            A, B_ = strings[si][0] - V(0, 0, 70), strings[si][1] - V(0, 0, 70)
            Ln = np.linalg.norm(B_ - A)
            sag = min(Ln * 0.14, 220.0)
            NB = int(Ln // 11.0)
            for i in range(NB + 1):
                p = hang(A, B_, sag, i / NB)
                rp, ry = rng() * 90.0, rng() * 90.0
                K.box('marigold_%d' % ((i // 9) % 2), Xf(rot_matrix(rp, ry, 0), p), V(6, 6, 6))
        for T in tops:
            base = T - V(0, 0, POLE_H - 10.0)
            In = norm2d(C - T)
            pot = base + In * 45.0
            K.rod('clay', pot, pot + V(0, 0, 32), 20.0, 10, True)
            for i in range(14):
                a = i * 2.39996
                rr = 6.0 + 10.0 * math.sqrt((i + 0.5) / 14)
                K.box('marigold_%d' % (i % 2), Xf(rot_matrix(0, a * 57.0, 0), pot + V(math.cos(a) * rr, math.sin(a) * rr, 38.0 + rng() * 6.0)),
                      V(5, 5, 5))

    if 'balloons' in kinds:
        for pi, top0 in enumerate(tops):
            T = top0 + V(0, 0, 16)
            for i in range(6):
                a = (i + pi) * 1.9
                x, y = math.cos(a) * 34.0, math.sin(a) * 34.0
                at = T + V(x, y, 70.0 + (i % 3) * 32.0 + rng() * 20.0)
                K.rod('rope', T, at - V(0, 0, 18), 0.3, 3)
                ball(K, 'balloon_%d' % ((i + pi) % 5), at, 15.0, 21.0, 10)

    if 'pumpkins' in kinds:
        NP = 18
        for i in range(NP):
            ang = Vn.yaw + 10.0 + i * 360.0 / NP
            side_ = norm2d(cross(UP, Vn.sea_dir))
            d = (Vn.sea_dir if i % 2 else -Vn.sea_dir) if b_line else yaw_vec(ang)
            if b_line:
                P = ground_at(C + side_ * (-R * 1.5 + (i // 2) * R * 3.0 / (NP // 2 - 1)) + d * 300.0)
            else:
                P = ground_at(C + d * R * (0.86 if i % 2 else 0.8))
            rr = 20.0 + rng() * 9.0
            H = rr * 1.35
            prof = [(math.sin(math.pi * j / 6) * rr, H * 0.5 - math.cos(math.pi * j / 6) * H * 0.5) for j in range(7)]
            K.lathe(Xf(rot_matrix(0, rng() * 30.0, 0), P), prof, ['pumpkin'], 10)
            K.rod('fir', P + V(0, 0, H - 3), P + V(0, 0, H + 8), 2.5, 5, True)
            In = -d
            face = rot_matrix(0, yaw_of(In), 0)
            F0 = P + In * (rr - 1.5) + V(0, 0, H * 0.55)
            sv = cross(UP, In)
            Glow.box('glow_pumpkin', Xf(face, F0 + sv * rr * 0.32), V(1, 3, 3))
            Glow.box('glow_pumpkin', Xf(face, F0 - sv * rr * 0.32), V(1, 3, 3))
            Glow.box('glow_pumpkin', Xf(face, F0 - V(0, 0, H * 0.25)), V(1, rr * 0.45, 2.2))

    if 'wish_tree' in kinds:
        line_w = norm2d(cross(UP, Vn.sea_dir))
        for i in range(5):
            base = ground_at(C + line_w * (-R * 1.2 + i * R * 0.6) - Vn.sea_dir * 250.0)
            lean = V(rng() - 0.5, rng() - 0.5, 0) * 50.0
            top = base + V(0, 0, 520) + lean
            K.rod('bamboo', base - V(0, 0, 30), top, 5.5, 8, True)
            for j in range(6):
                K.rod('bamboo', base + V(0, 0, 60 + j * 80) + lean * (j / 6.0), base + V(0, 0, 64 + j * 80) + lean * (j / 6.0), 6.5, 8, True)
            for j in range(9):
                a = j * 2.39996 + i
                fr = base + V(0, 0, 260 + j * 28) + lean * ((260.0 + j * 28.0) / 520.0)
                to = fr + V(math.cos(a) * 110.0, math.sin(a) * 110.0, 45.0)
                K.rod('bamboo', fr, to, 1.4, 4)
                for l in range(4):
                    lf = fr + (to - fr) * (0.35 + l * 0.2)
                    dr = V(math.cos(a + l), math.sin(a + l), -0.4)
                    dr /= np.linalg.norm(dr)
                    sd = cross(dr, UP)
                    sd = sd / np.linalg.norm(sd) * 4.0
                    K.tri('leaf', lf + sd, lf - sd, lf + dr * 32.0)
                    K.tri('leaf', lf - sd, lf + sd, lf + dr * 32.0)
                for w in range(3):
                    hk = fr + (to - fr) * (0.3 + w * 0.28)
                    fc = V(math.cos(a + 1.6), math.sin(a + 1.6), 0) * 3.5
                    t0 = hk - V(0, 0, 6)
                    t1 = t0 - V(0, 0, 22)
                    m = 'flag_%d' % ((j + w + i) % 5)
                    K.rod('rope', hk, t0, 0.2, 3)
                    K.quad(m, t1 - fc, t1 + fc, t0 + fc, t0 - fc)
                    K.quad(m, t0 - fc, t0 + fc, t1 + fc, t1 - fc)
    return out


# ----------------------------------------------------------------------------------------------------------------------
# fireworks and sky lanterns

def firework_burst(centre, radius, mat, kit):
    """ALyudFireworks::Setup burst mesh (110 streaks on a Fibonacci sphere, r 0.62..1 of 100 cm) scaled radius/100."""
    s = radius / 100.0
    N = 110
    for i in range(N):
        y = 1.0 - 2.0 * (i + 0.5) / N
        rr = math.sqrt(1.0 - y * y)
        phi = i * 2.39996
        d = V(math.cos(phi) * rr, math.sin(phi) * rr, y)
        kit.rod(mat, centre + d * 62.0 * s, centre + d * 100.0 * s, 0.55 * s, 3)


def sky_lantern(kit, centre, yaw, tilt_p, tilt_r, mat='glow_sky'):
    """ALyudSkyLanterns::Setup lathe: {20,0},{26,30},{28,58},{0,62}, 8 sides."""
    F = Xf(rot_matrix(tilt_p, yaw, tilt_r), centre)
    kit.lathe(F, [(20, 0), (26, 30), (28, 58), (0, 62)], [mat], 8)



def water_lantern(solid, glow, centre, yaw, tilt_p, tilt_r):
    """ALyudSkyLanterns::Setup bOnWater mesh: a wooden float, a lit paper box, a lid (centre = the component's origin)."""
    F = Xf(rot_matrix(tilt_p, yaw, tilt_r), centre)
    solid.box('wood_float', Xf(F.R, F.pos(V(0, 0, 2.5))), V(17, 17, 2.5))
    glow.box('glow_water', Xf(F.R, F.pos(V(0, 0, 18))), V(11, 11, 13))
    solid.box('wood_float', Xf(F.R, F.pos(V(0, 0, 32))), V(13, 13, 1.2))


def firecrackers(solid, glow, light_out, at, facing, rng):
    """ALyudFirecrackers::Setup (+ a few sparks frozen mid-pop, positions as Tick picks them)."""
    side = cross(UP, norm2d(facing))
    for y in (-180.0, 180.0):
        p = at + side * y
        solid.rod('fc_pole', p - V(0, 0, 30), p + V(0, 0, 420), 5.0, 8, True)
    solid.rod('fc_pole', at - side * 190 + V(0, 0, 410), at + side * 190 + V(0, 0, 410), 3.0, 6, True)
    for i in range(5):
        top = at + side * (-120.0 + i * 60.0) + V(0, 0, 405)
        for j in range(34):
            c = top - V(0, 0, 12 + j * 10)
            out = yaw_vec(j * 67.0) * 4.0
            solid.rod('cracker', c + out - V(0, 0, 3), c + out * 2.6 + V(0, 0, 2), 1.4, 4, True)
        solid.rod('fc_rope', top, top - V(0, 0, 350), 0.5, 3)
    n = 0
    while n < 7:
        ground = rng() < 0.4
        p = at + side * (-120.0 + int(rng() * 5.0) * 60.0) + V(0, 0, (8.0 + rng() * 30.0) if ground else (60.0 + rng() * 330.0))
        if ground:
            p = p + V(rng() - 0.5, rng() - 0.5, 0) * 240.0
        pit, yaw = rng() * 360.0, rng() * 360.0
        sc = 0.6 + rng() * 0.8
        R = rot_matrix(pit, yaw, 0)
        for i in range(6):
            d = yaw_vec(i * 60.0) * math.cos(math.radians(i * 30.0)) + V(0, 0, math.sin(math.radians(i * 30.0)))
            glow.rod('glow_spark', p + R @ (-d * 18 * sc), p + R @ (d * 18 * sc), 1.6 * sc, 3)
        n += 1
    light_out.append((at + V(0, 0, 200), (1.0, 0.7, 0.4), 9000.0 * 0.5))


def powder_puffs(kits, centre, radius, rng, count):
    """ALyudColorPowder: puffs frozen at mid-life (u = 0.5), a lumpy cloud of three balls each, coloured from Colors."""
    for i in range(count):
        life = 2.0 + rng() * 1.5
        size = 1.2 + rng() * 1.6
        a, d = rng() * 2 * math.pi, math.sqrt(rng()) * radius * 0.7
        p = centre + V(math.cos(a) * d, math.sin(a) * d, 120.0 + rng() * 80.0)
        drift = V(rng() - 0.5, rng() - 0.5, 0) * 60.0 + V(0, 0, 25)
        col = min(int(rng() * 6.0), 5)
        t = 0.5 * life
        grow = 1.0 - math.exp(-t / 0.25)
        scale = max(0.01, size * grow)        # u = 0.5 < 0.6: no thinning yet
        p = p + drift * t
        k = kits.setdefault(col, Kit())
        m = 'powder_%d' % col
        for (c, r, h, sd) in ((V(0, 0, 50), 50, 80, 10), (V(28, 12, 40), 36, 56, 8), (V(-22, -20, 62), 34, 50, 8)):
            ball(k, m, p + c * scale, r * scale, h * scale, sd)


# ----------------------------------------------------------------------------------------------------------------------
# Blender side

def B(p):
    """UE cm -> Blender m (y mirrored: UE is left-handed)."""
    return (p[0] / 100.0, -p[1] / 100.0, p[2] / 100.0)


_mats = {}


def material(name, night=1.0):
    key = (name, night)
    if key in _mats:
        return _mats[key]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    if name.startswith('glow') or name.startswith('fw_'):
        if name == 'glow_sky':
            c, inten = (1.0, 0.48, 0.14), 9.0
        elif name == 'glow_water':      # ALyudSkyLanterns water mode
            c, inten = (1.0, 0.62, 0.22), 6.0
        elif name == 'glow_spark':      # ALyudFirecrackers spark
            c, inten = (1.0, 0.8, 0.45), 40.0
        elif name.startswith('fw_'):
            c, inten = FIRE[int(name[3:])], 9.0 / GLOW_K
        else:
            c, inten = glow_spec(name, night)
        em = nt.nodes.new('ShaderNodeEmission')
        em.inputs['Color'].default_value = (*c, 1)
        em.inputs['Strength'].default_value = inten * GLOW_K
        nt.links.new(em.outputs[0], out.inputs[0])
    else:
        col, rough, metal = PAINT[name]
        bs = nt.nodes.new('ShaderNodeBsdfPrincipled')
        bs.inputs['Base Color'].default_value = (*col, 1)
        bs.inputs['Roughness'].default_value = rough
        bs.inputs['Metallic'].default_value = metal
        nt.links.new(bs.outputs[0], out.inputs[0])
    _mats[key] = m
    return m


def emit_kit(kit, name, night=1.0):
    for mat, tris in kit.tris.items():
        arr = np.array(tris, dtype=float).reshape(-1, 3)            # (n*3, 3) UE cm
        arr = np.stack([arr[:, 0], -arr[:, 1], arr[:, 2]], axis=1) / 100.0
        n = len(arr) // 3
        me = bpy.data.meshes.new('%s_%s' % (name, mat))
        me.vertices.add(len(arr))
        me.vertices.foreach_set('co', arr.ravel())
        me.loops.add(len(arr))
        me.loops.foreach_set('vertex_index', np.arange(len(arr)))
        me.polygons.add(n)
        me.polygons.foreach_set('loop_start', np.arange(n) * 3)
        me.update()
        me.validate()
        ob = bpy.data.objects.new('%s_%s' % (name, mat), me)
        ob.data.materials.append(material(mat, night))
        bpy.context.scene.collection.objects.link(ob)


def add_light_point(pos, colour, watts, radius=0.3):
    ld = bpy.data.lights.new('pl', 'POINT')
    ld.color = colour
    ld.energy = watts
    ld.shadow_soft_size = radius
    ob = bpy.data.objects.new('pl', ld)
    ob.location = B(pos)
    bpy.context.scene.collection.objects.link(ob)


def add_sun(elev, azim, strength, colour=(1, 1, 1)):
    ld = bpy.data.lights.new('sun', 'SUN')
    ld.energy = strength
    ld.color = colour
    ld.angle = math.radians(1.0)
    ob = bpy.data.objects.new('sun', ld)
    ob.rotation_euler = (math.radians(90 - elev), 0, math.radians(azim))
    bpy.context.scene.collection.objects.link(ob)


def plane(name, x0, x1, y0, y1, z, mat):
    k = Kit()
    p = [V(x0, y0, z), V(x1, y0, z), V(x1, y1, z), V(x0, y1, z)]
    k.quad(mat, p[0], p[1], p[2], p[3])
    emit_kit(k, name)


def set_camera(pos, target, lens=24.0):
    cam = bpy.data.cameras.new('cam')
    cam.lens = lens
    cam.clip_start, cam.clip_end = 0.1, 5000
    ob = bpy.data.objects.new('cam', cam)
    ob.location = B(pos)
    d = Vector(B(target)) - Vector(B(pos))
    ob.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    bpy.context.scene.collection.objects.link(ob)
    bpy.context.scene.camera = ob


def world(night, sun_elev=40.0):
    w = bpy.data.worlds.new('w')
    w.use_nodes = True
    nt = w.node_tree
    nt.nodes.clear()
    out = nt.nodes.new('ShaderNodeOutputWorld')
    bg = nt.nodes.new('ShaderNodeBackground')
    if night:
        # dark blue, a little lighter near the horizon
        tc = nt.nodes.new('ShaderNodeTexCoord')
        sep = nt.nodes.new('ShaderNodeSeparateXYZ')
        nt.links.new(tc.outputs['Generated'], sep.inputs[0])
        ramp = nt.nodes.new('ShaderNodeValToRGB')
        ramp.color_ramp.elements[0].position = 0.5
        ramp.color_ramp.elements[0].color = (0.030, 0.045, 0.10, 1)
        ramp.color_ramp.elements[1].position = 0.9
        ramp.color_ramp.elements[1].color = (0.004, 0.008, 0.03, 1)
        nt.links.new(sep.outputs['Z'], ramp.inputs[0])
        nt.links.new(ramp.outputs[0], bg.inputs[0])
    else:
        sky = nt.nodes.new('ShaderNodeTexSky')
        sky.sky_type = 'MULTIPLE_SCATTERING'
        sky.sun_elevation = math.radians(sun_elev)
        sky.sun_rotation = math.radians(200)
        sky.sun_disc = False
        bg.inputs['Strength'].default_value = 0.35
        nt.links.new(sky.outputs[0], bg.inputs[0])
    if night:
        bg.inputs['Strength'].default_value = 0.35
    nt.links.new(bg.outputs[0], out.inputs[0])
    bpy.context.scene.world = w


def setup_render(w, h, samples, night, exposure):
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'
    sc.cycles.device = 'CPU'
    sc.cycles.samples = samples
    sc.cycles.use_denoising = True
    sc.cycles.max_bounces = 6
    sc.render.resolution_x, sc.render.resolution_y = w, h
    sc.render.resolution_percentage = 100
    sc.render.image_settings.file_format = 'PNG'
    sc.view_settings.view_transform = 'AgX'
    sc.view_settings.exposure = exposure
    # a bit of glow round the lamps (night only)
    try:
        if not night:
            raise RuntimeError('day')
        tree = bpy.data.node_groups.new('comp', 'CompositorNodeTree')
        tree.interface.new_socket('Image', in_out='OUTPUT', socket_type='NodeSocketColor')
        rl = tree.nodes.new('CompositorNodeRLayers')
        gl = tree.nodes.new('CompositorNodeGlare')
        gl.inputs['Type'].default_value = 'Fog Glow'
        gl.inputs['Threshold'].default_value = 1.5
        gl.inputs['Size'].default_value = 0.6
        gl.inputs['Strength'].default_value = 0.8
        out = tree.nodes.new('NodeGroupOutput')
        tree.links.new(rl.outputs['Image'], gl.inputs['Image'])
        tree.links.new(gl.outputs['Image'], out.inputs[0])
        sc.render.compositor_device = 'CPU'
        sc.compositing_node_group = tree
    except Exception as e:
        print('no glare:', e)


# ----------------------------------------------------------------------------------------------------------------------
# the scenes

# city_plan.json (origin/town-life): S-SQUARE ue (1634, 91290), kind default radius 1900, yaw -4.7;
# S-STAGE ue (-3866, 91290), i.e. 5500 cm toward -X from the square.  The square is moved to the origin.
SQUARE_YAW = -4.7
STAGE_AT = V(-5500.0, 0.0, 0.0)
SEA = V(1, 0, 0)
FOUNT_R = 900.0     # the 18 m fountain basin


def square_venue():
    return Venue('square', V(), 1900.0, SQUARE_YAW, SEA, STAGE_AT, plan_stalls=False)


def add_square_placeholders():
    k = Kit()
    k.lathe(Xf(), [(0, 4), (1900, 4), (1900, 0)], ['ph_pave'], 64)         # the paved square
    emit_kit(k, 'plaza')
    f = Kit()                                                              # the fountain basin
    f.lathe(Xf(), [(FOUNT_R, 0), (FOUNT_R, 60), (FOUNT_R - 40, 60), (FOUNT_R - 40, 20)], ['ph_concrete'], 48)
    f.lathe(Xf(), [(FOUNT_R - 40, 48), (0, 48)], ['ph_water'], 48)
    f.lathe(Xf(), [(0, 48), (70, 48), (55, 300), (110, 330), (0, 330)], ['ph_concrete'], 16)
    emit_kit(f, 'fountain')
    s = Kit()                                                              # the plan's stage: 10 x 6 x 1.1 m
    s.box_in('ph_concrete', Xf(None, STAGE_AT), V(0, 0, 55), V(600, 1000, 110))
    s.box_in('backdrop', Xf(None, STAGE_AT), V(-290, 0, 110 + 200), V(20, 1000, 400))
    emit_kit(s, 'stage_ph')
    plane('ground', -80000, 80000, -80000, 80000, 0.0, 'ph_ground')
    plane('sea', 16000, 400000, -400000, 400000, -0.5, 'ph_water')


def fireworks_scene():
    fk = Kit()
    bursts = [(10000, -8500, 11500, 3600, 3), (14500, 2500, 14500, 4400, 1), (9000, 7500, 9500, 2800, 2),
              (18000, -4500, 15500, 4800, 5), (12500, 600, 10000, 2600, 0)]
    for (x, y, z, r, ci) in bursts:
        firework_burst(V(x, y, z), r, 'fw_%d' % ci, fk)
        add_light_point(V(x, y, z), FIRE[ci], 4e5, 20)
    emit_kit(fk, 'fireworks')


def fireworks_few():
    fk = Kit()
    for (x, y, z, r, ci) in [(11000, -4200, 9000, 3200, 1), (15000, 3000, 12500, 4000, 0), (12500, 7500, 8000, 2600, 4)]:
        firework_burst(V(x, y, z), r, 'fw_%d' % ci, fk)
        add_light_point(V(x, y, z), FIRE[ci], 1.2e5, 20)
    emit_kit(fk, 'fireworks')


NEW_SCENES = ('lunar_new_year_night', 'halloween_night', 'tanabata_day', 'toro_nagashi_night', 'holi_day')


def run_new(scene, out, blend, w, h, samples):
    night = scene.endswith('night')
    nv = 1.0 if night else 0.0
    extra_lights = []
    solid_extra, glow_extra = Kit(), Kit()
    if scene == 'lunar_new_year_night':
        v = square_venue()
        dec = build(v, ['red_lanterns', 'bunting', 'stalls'], 1634)
        d = norm2d(v.stage_at - v.center)                      # the "firecrackers" show's placement
        at = ground_at(v.center + d * v.radius * 0.55 + cross(UP, d) * v.radius * 0.3)
        firecrackers(solid_extra, glow_extra, extra_lights, at, -d, Rand(77))
        add_square_placeholders()
        world(True)
        add_sun(35, 140, 0.05, (0.45, 0.55, 1.0))
        fireworks_few()
        set_camera(V(-2300, 100, 200), V(2400, -1500, 1200), 20.0)
    elif scene == 'halloween_night':
        v = square_venue()
        dec = build(v, ['pumpkins', 'candles', 'bunting'], 1634)
        add_square_placeholders()
        world(True)
        add_sun(35, 140, 0.08, (0.45, 0.55, 1.0))
        set_camera(V(900, -1650, 90), V(1300, 300, 80), 22.0)
    elif scene == 'tanabata_day':
        v = Venue('promenade', V(), 1200.0, 0.0, SEA)
        dec = build(v, ['wish_tree', 'lanterns'], 7777)
        k = Kit()
        k.box('ph_pave', Xf(None, V(-100, 0, -10)), V(1200, 2600, 10))          # the promenade's paving
        emit_kit(k, 'promenade')
        plane('ground', -80000, 1100, -80000, 80000, -30.0, 'ph_ground')
        plane('sea', 1100, 400000, -400000, 400000, -80.0, 'ph_water')
        world(False, 38)
        add_sun(38, 200, 1.8, (1.0, 0.95, 0.85))
        set_camera(V(-950, -1450, 190), V(-250, -250, 330), 20.0)
    elif scene == 'toro_nagashi_night':
        v = Venue('beach', V(), 1500.0, 0.0, SEA)
        dec = build(v, ['lanterns', 'candles'], 4242)
        k = Kit()
        k.box('ph_sand', Xf(None, V(-30000, 0, -6)), V(31500, 60000, 14))       # the sand: top at +8, ends at x = 1500
        emit_kit(k, 'sand')
        plane('sea', 1000, 500000, -500000, 500000, 0.0, 'ph_water')
        world(True)
        add_sun(28, 60, 0.35, (0.55, 0.65, 1.0))
        # Show 'water_lanterns': at = centre + sea * max(R * 1.1, 1200); drift out at 22..38 cm/s, bobbing; 30 of the pool of 50
        at = v.center + SEA * max(v.radius * 1.1, 1200.0)
        side = norm2d(cross(UP, SEA))
        rng = Rand(31)
        for n in range(30):
            p = at + side * (rng() - 0.5) * 3000.0
            vel = SEA * (22.0 + rng() * 16.0) + side * (rng() - 0.5) * 10.0
            phase = rng() * 10.0
            t_end = 3.0 + rng() * 100.0
            tt = 0.0
            while tt < t_end:
                dt = min(0.5, t_end - tt)
                tt += dt
                p = p + vel * dt + V(math.sin(tt * 0.3 + phase), math.cos(tt * 0.25 + phase), 0) * 6.0 * dt
            p[2] = 0.0 + math.sin(tt * 1.3 + phase) * 4.0
            water_lantern(solid_extra, glow_extra, p, phase * 30.0 + tt * 2.0, math.sin(tt * 1.1 + phase) * 3.0, math.cos(tt * 0.9 + phase) * 3.0)
        set_camera(V(-700, -700, 130), V(2400, 500, 40), 20.0)
    else:   # holi_day
        v = Venue('park', V(), 1600.0, 0.0, SEA)
        dec = build(v, ['bunting', 'flags', 'marigolds'], 2468)
        plane('ground', -80000, 80000, -80000, 80000, 0.0, 'ph_ground')
        kits = {}
        powder_puffs(kits, V(), 1600.0, Rand(5150), 12)
        for ci, kk in kits.items():
            emit_kit(kk, 'powder%d' % ci, 0.0)
        world(False, 42)
        add_sun(42, 200, 1.8, (1.0, 0.95, 0.85))
        set_camera(V(1500, 1200, 170), V(-600, -300, 250), 18.0)
    emit_kit(dec.solid, 'decor', nv)
    emit_kit(dec.glow, 'lights', nv)
    emit_kit(solid_extra, 'show_solid', nv)
    emit_kit(glow_extra, 'show_glow', nv)
    for (pos, col, cd) in dec.lights:
        add_light_point(pos, col, cd * 0.12, 5.0)
    for (pos, col, cd) in extra_lights:
        add_light_point(pos, col, cd * 0.12, 5.0)
    setup_render(w, h, samples, night, {'halloween_night': 2.5, 'toro_nagashi_night': 1.0}.get(scene, 1.0 if night else -0.4))
    bpy.context.scene.render.filepath = out
    if blend:
        bpy.ops.wm.save_as_mainfile(filepath=blend)
    bpy.ops.render.render(write_still=True)


def run(scene, out, blend, w, h, samples):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if scene in NEW_SCENES:
        return run_new(scene, out, blend, w, h, samples)
    night = not scene.startswith('square_day')
    nv = 1.0 if night else 0.0
    if scene in ('square_day', 'square_day_stalls', 'square_night'):
        dec = build(square_venue(), ['bunting', 'flags', 'lanterns', 'stage', 'stalls'], 1634)
    elif scene == 'new_year_night':
        dec = build(square_venue(), ['tree', 'lanterns', 'wreaths', 'stage'], 1634)
    else:
        dec = build(Venue('beach', V(), 1500.0, 0.0, SEA), ['lanterns'], 4242)
    emit_kit(dec.solid, 'decor', nv)
    emit_kit(dec.glow, 'lights', nv)
    if scene != 'lantern_beach':
        add_square_placeholders()
    if scene in ('square_day', 'square_day_stalls'):
        world(False, 38)
        add_sun(38, 200, 1.8, (1.0, 0.95, 0.85))
        if scene == 'square_day_stalls':
            set_camera(V(-1100, -250, 190), V(1500, 250, 330), 17.0)
        else:
            set_camera(V(1650, 420, 170), V(-5500, 0, 330), 20.0)
    elif scene == 'square_night':
        world(True)
        add_sun(35, 140, 0.05, (0.45, 0.55, 1.0))
        fireworks_scene()
        # the stage's POV (the fireworks are over the sea behind the stage's audience): standing on the plan stage
        set_camera(V(-5650, 150, 280), V(1500, -250, 3400), 20.0)
    elif scene == 'new_year_night':
        world(True)
        add_sun(35, 140, 0.05, (0.45, 0.55, 1.0))
        set_camera(V(-250, -350, 170), V(-3400, 150, 560), 20.0)
    else:
        plane('sand', -60000, 3500, -60000, 60000, 0.0, 'ph_sand')
        plane('sea', 3500, 500000, -500000, 500000, -20.0, 'ph_water')
        world(True)
        add_sun(28, 60, 0.35, (0.55, 0.65, 1.0))
        # ~26 sky lanterns released from the beach, drifting out to sea (ALyudSkyLanterns::Tick, advanced to random ages)
        rng = Rand(99)
        sk = Kit()
        side = norm2d(cross(UP, SEA))
        n = 0
        while n < 26:
            p = side * (rng() - 0.5) * 3000.0 + SEA * (rng() - 0.5) * 500.0 + V(0, 0, 120)
            vel = V(0, 0, 55.0 + rng() * 45.0) + SEA * (40.0 + rng() * 60.0) + side * (rng() - 0.5) * 30.0
            phase = rng() * 10.0
            t = 5.0 + rng() * 190.0
            q = p + vel * t + V(math.sin(t * 0.4 + phase), math.cos(t * 0.33 + phase), 0) * 8.0 * t * 0.5
            if not (500.0 <= q[2] <= 15000.0):
                continue
            sky_lantern(sk, q, phase * 30.0, math.sin(t * 0.7 + phase) * 4, math.cos(t * 0.6 + phase) * 4)
            n += 1
        emit_kit(sk, 'skylanterns')
        set_camera(V(-5200, 300, 170), V(2500, -100, 3200), 15.0)
    for (pos, col, cd) in dec.lights:        # the C++ point lights (candela -> rough watts for Cycles)
        add_light_point(pos, col, cd * 0.12, 5.0)
    setup_render(w, h, samples, night, {'new_year_night': 1.0, 'lantern_beach': 0.5}.get(scene, 0.0 if night else -0.4))
    bpy.context.scene.render.filepath = out
    if blend:
        bpy.ops.wm.save_as_mainfile(filepath=blend)
    bpy.ops.render.render(write_still=True)


if __name__ == '__main__':
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
    ap = argparse.ArgumentParser()
    ap.add_argument('scene', choices=['square_day', 'square_day_stalls', 'square_night', 'new_year_night', 'lantern_beach'] + list(NEW_SCENES))
    ap.add_argument('out')
    ap.add_argument('--blend')
    ap.add_argument('--w', type=int, default=1600)
    ap.add_argument('--h', type=int, default=900)
    ap.add_argument('--samples', type=int, default=64)
    a = ap.parse_args(argv)
    run(a.scene, a.out, a.blend, a.w, a.h, a.samples)
