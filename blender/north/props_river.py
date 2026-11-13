"""River valley, hot spring and trail dressing (docs/north.md "River and spring dressing"): driftwood, river_boulder,
brow_bench, trail_marker, spring_bench, spring_steps, gauge_post, rowboat.

Frame as everywhere in the kit: metres, Z up, Blender -Y is the front, origin on the ground under the middle, transforms applied.
"""
import math
import random

from mathutils import Matrix, Vector
from mathutils.geometry import normal

import northtex as T
from northkit import Builder, V

DW = "bark"          # bleached driftwood: the atlas has no pale grey wood tile; the pale "bark" tile (creamy grey, dark cracks) is the closest


def rz(a):
    return Matrix.Rotation(a, 3, "Z")


def rx(a):
    return Matrix.Rotation(a, 3, "X")


def ry(a):
    return Matrix.Rotation(a, 3, "Y")


def lerp(a, b, t):
    return a + (b - a) * t


def smooth(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3 - 2 * t)


def rock(b, c, size, seed, subdiv=2, flatten=1.0, rough=0.16, rot=None, tile="stone"):
    """A rounded stone like Builder.stone (same shape rules) but with UVs that never leave the tile: one planar projection per
    face (dominant axis) scaled so the whole stone fits the tile window (Builder.stone wraps large stones across tile edges)."""
    import bmesh
    r = random.Random(seed)
    tmp = bmesh.new()
    bmesh.ops.create_icosphere(tmp, subdivisions=subdiv, radius=0.5)
    sx, sy, sz = size
    ph = [r.uniform(0, 6.28) for _ in range(6)]
    rotm = rot if rot is not None else rz(r.uniform(0, 6.28))
    for v in tmp.verts:
        d = v.co.normalized()
        k = 1.0 + rough * (math.sin(3 * d.x + ph[0]) * math.sin(2.3 * d.y + ph[1]) + 0.6 * math.sin(4 * d.z + ph[2]))
        v.co = V(d.x * 0.5 * sx * k, d.y * 0.5 * sy * k, d.z * 0.5 * sz * k)
        if v.co.z < -0.15 * sz and flatten:
            v.co.z = -0.15 * sz - (v.co.z + 0.15 * sz) * (1 - flatten)
    tmp.normal_update()
    u0, v0, du, dv = b.win(tile)
    ext = max(size) * (1 + rough) * 1.02
    for f in tmp.faces:
        n = f.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        ia, ib = [i for i in range(3) if i != ax]
        uvs = [(u0 + (0.02 + 0.96 * (v.co[ia] / ext + 0.5)) * du, v0 + (0.02 + 0.96 * (v.co[ib] / ext + 0.5)) * dv) for v in f.verts]
        b.face([c + rotm @ v.co for v in f.verts], uvs, smooth=True)
    tmp.free()


def root_tube(b, p0, direction, length, r0, droop, seg=4, tile=DW):
    """A root / branch: three points from p0 along `direction` (unit), sagging by `droop`, tapering to a thin tip."""
    d = Vector(direction).normalized()
    p0 = Vector(p0)
    pts = [p0, p0 + d * length * 0.5 - V(0, 0, droop * 0.4), p0 + d * length - V(0, 0, droop)]
    b.tube(pts, r0, seg=seg, tile=tile, caps=True, rscale=lambda i, n: [1.0, 0.62, 0.3][i])


def log_tube(b, pts, r0, r1, seg=6, tile=DW):
    n = len(pts)
    b.tube(pts, r0, seg=seg, tile=tile, caps=True, rscale=lambda i, nn: lerp(1.0, r1 / r0, i / (nn - 1)))


# =========================================================================================================
# driftwood
# =========================================================================================================

def driftwood(variant):
    b = Builder("driftwood_v%d" % variant)
    rnd = random.Random(300 + variant)
    notes = ("bleached river driftwood for gravel bars, pale grey (atlas tile 'wood': no paler tile exists); ")
    if variant == 0:
        # one stripped trunk 6.2 m along X, root plate at -X
        n = 9
        R0, R1 = 0.24, 0.1
        x0, x1 = -2.95, 3.1
        pts = []
        for i in range(n):
            t = i / (n - 1)
            rr = lerp(R0, R1, t)
            pts.append(V(lerp(x0, x1, t), 0.22 * math.sin(t * 2.6) - 0.1, rr * 0.92 - 0.0 + 0.012 * math.sin(t * 7)))
        log_tube(b, pts, R0, R1, seg=6)
        # root plate: an upright disc of tangled roots
        b.cyl(V(-3.14, -0.1, 0.8), V(-2.84, -0.1, 0.8), 0.86, 0.7, seg=9, tile=DW, cap_tile="wood_dark", wobble=0.12)
        for k in range(8):
            a = 2 * math.pi * k / 8 + rnd.uniform(-0.2, 0.2)
            dirv = V(-0.55, math.cos(a) * 0.8, math.sin(a) * 0.8)
            start = V(-3.1, -0.1 + math.cos(a) * 0.5, 0.8 + math.sin(a) * 0.5)
            root_tube(b, start, dirv, rnd.uniform(0.5, 1.0), 0.12, 0.15 + 0.2 * (math.sin(a) < 0))
        for x, side in ((-0.7, 1), (0.9, -1), (2.0, 1)):
            t = (x - x0) / (x1 - x0)
            p = V(x, 0.22 * math.sin(t * 2.6) - 0.1, lerp(R0, R1, t) * 0.92)
            root_tube(b, p + V(0, side * 0.05, 0.08), V(0.3, side * 0.6, 0.7), rnd.uniform(0.3, 0.5), 0.04, 0.0, seg=4)
        notes += "a single stripped trunk 6.2 m long along X, root plate (upright, 1.5 m across) at -X, tip at +X"
    elif variant == 1:
        # a tangle of three logs: A on the ground, B and C lean on it
        yA, rA = 0.0, 0.2
        A = [V(-2.1 + 4.2 * i / 4, 0.12 * math.sin(i * 1.1), rA * 0.95 - 0.01) for i in range(5)]
        log_tube(b, A, rA, 0.15, seg=6)
        rB = 0.16
        cB = V(0.2, 0.0, 2 * rA + rB * 0.9)                   # B rests across A
        dB = V(math.cos(math.radians(55)), math.sin(math.radians(55)), 0)
        far = cB - dB * 1.6
        near = cB + dB * 1.8
        far.z = rB * 0.9
        near.z = rB * 0.9 + 0.05
        B = [far.lerp(near, i / 4) for i in range(5)]
        for i, p in enumerate(B):
            t = i / 4
            p.z = lerp(far.z, cB.z, smooth(min(1.0, t / 0.45))) if t < 0.45 else lerp(cB.z, 2 * rA * 0.55 + rB * 0.7, (t - 0.45) / 0.55)
        log_tube(b, B, rB, 0.1, seg=6)
        rC = 0.13
        cC = V(-1.15, 0.0, 2 * rA + rC * 0.85)
        dC = V(math.cos(math.radians(-38)), math.sin(math.radians(-38)), 0)
        C = []
        for i in range(5):
            t = i / 4
            p = cC + dC * lerp(-1.4, 1.5, t)
            p.z = lerp(cC.z, rC * 0.9, smooth(max(0.0, (t - 0.5) / 0.5))) if t > 0.5 else lerp(rC * 0.9 + 0.12, cC.z, t / 0.5)
            C.append(p)
        log_tube(b, C, rC, 0.085, seg=6)
        # broken limb stubs and a few roots at the cut ends
        for p0, d in ((A[0], V(-1, 0.3, 0.5)), (B[0], V(-0.5, -0.8, 0.3)), (C[0], V(-1, 0.4, 0.5)), (A[3], V(0.2, 0.9, 0.9)), (B[2], V(-0.7, 0.6, 0.8))):
            root_tube(b, p0 + V(0, 0, 0.02), d, rnd.uniform(0.35, 0.7), 0.05, 0.05, seg=4)
        for k in range(4):
            a = 2 * math.pi * k / 4 + 0.4
            root_tube(b, A[0] + V(0, 0, 0.0), V(-0.6, math.cos(a), 0.2 + math.sin(a) * 0.6), 0.6, 0.06, 0.1, seg=4)
        notes += "a tangle of three logs (4.2, 3.6 and 3.0 m) in a heap ~ 4.5 x 3.5 m, the two lighter ones resting across the first"
    else:
        # a short stump with splayed roots
        b.cyl(V(0, 0, -0.05), V(0, 0, 0.4), 0.58, 0.37, seg=8, tile=DW, cap_tile=DW, cap1=False)
        b.cyl(V(0, 0, 0.4), V(0, 0, 1.0), 0.37, 0.31, seg=8, tile=DW, cap_tile="endgrain", cap0=False)
        for k in range(3):                                   # splintered top
            a = 2 * math.pi * k / 3 + 0.5
            b.cyl(V(math.cos(a) * 0.2, math.sin(a) * 0.2, 0.97), V(math.cos(a) * 0.15, math.sin(a) * 0.15, 0.97 + 0.22 + 0.12 * k),
                  0.09, 0.0, seg=4, tile=DW, cap_tile=DW, wobble=0.05)
        for k in range(6):
            a = 2 * math.pi * k / 6 + rnd.uniform(-0.35, 0.35)
            c, s = math.cos(a), math.sin(a)
            Lr = rnd.uniform(0.6, 1.5)
            pts = [V(c * 0.3, s * 0.3, 0.32), V(c * (0.3 + Lr * 0.45), s * (0.3 + Lr * 0.45), 0.2), V(c * (0.3 + Lr * 0.75), s * (0.3 + Lr * 0.75), 0.08),
                   V(c * (0.3 + Lr), s * (0.3 + Lr), 0.015)]
            b.tube(pts, rnd.uniform(0.12, 0.2), seg=5, tile=DW, caps=True, rscale=lambda i, n: [1.0, 0.7, 0.42, 0.2][i])
        notes += "a short flood-torn stump 1 m high with six roots splayed ~ 1.8 m round"
    return b, {"notes": notes}


# =========================================================================================================
# river_boulder
# =========================================================================================================

def river_boulder(variant):
    b = Builder("river_boulder_v%d" % variant)
    if variant == 0:
        spec = [((0, 0), (1.8, 1.35, 1.4), 0.0, 1), ((0.95, -0.7), (0.55, 0.5, 0.5), 0.0, 1)]
        note = "one big rounded boulder 1.8 x 1.35 m (0.9 m high) and a cobble beside it"
    elif variant == 1:
        spec = [((-0.25, 0.15), (1.0, 0.85, 0.95), 0.0, 1), ((0.55, -0.35), (0.7, 0.62, 0.7), 0.0, 1), ((0.3, 0.55), (0.55, 0.5, 0.55), 0.0, 1)]
        note = "a cluster of three boulders (1.0, 0.7 and 0.55 m), about 1.5 m across"
    else:
        spec = [((-0.4, 0.0), (0.7, 0.62, 0.65), 0.0, 1), ((0.25, -0.15), (0.6, 0.55, 0.55), 0.0, 1), ((0.0, 0.5), (0.5, 0.45, 0.5), 0.0, 1)]
        note = "a spill of three cobbles (0.7, 0.6 and 0.5 m), about 1.3 m across"
    for i, ((x, y), sz, _, sd) in enumerate(spec):
        rock(b, V(x, y, sz[2] * 0.12), sz, seed=10 * variant + i, subdiv=2, flatten=1.0, rough=0.16, rot=rz(0.9 * i + variant))
    return b, {"notes": note + "; grey andesite (atlas 'stone' tile), bottoms flat and just below z 0"}


# =========================================================================================================
# brow_bench
# =========================================================================================================

def plank_x(b, c, size, tile="wood", **kw):
    b.box(c, size, tile, tiles={"x": "endgrain", "top": tile}, grain=0, **kw)


def brow_bench(variant):
    b = Builder("brow_bench")
    L = 2.2
    zt = 0.46
    for sx in (-1, 1):
        x = sx * 0.88
        b.cyl(V(x, -0.34, 0.15), V(x, 0.4, 0.15), 0.15, 0.15, seg=7, tile="wood_dark", cap_tile="endgrain", wobble=0.03)     # sleeper log
        for y, top in ((-0.24, 0.41), (0.30, 1.0)):
            b.box(V(x, y, (0.1 + top) / 2), (0.13, 0.13, top - 0.1), "wood", grain=2, tiles={"top": "endgrain"})
        b.box(V(x, 0.03, 0.34), (0.1, 0.62, 0.09), "wood_dark", grain=1, tiles={"y": "endgrain"})              # bearer
    for y in (-0.115, 0.115):
        plank_x(b, V(0, y, zt - 0.04), (L, 0.225, 0.08), "wood")
    b.box(V(0, -0.24, 0.41 - 0.07), (L - 0.3, 0.07, 0.1), "wood_dark", grain=0, tiles={"x": "endgrain"})      # front apron
    for z in (0.66, 0.88):
        plank_x(b, V(0, 0.2575, z), (L - 0.04, 0.045, 0.17), "wood")
    return b, {"seats": [[-0.55, 0.0, zt, 0.0], [0.55, 0.0, zt, 0.0]],
               "notes": "heavy plank-and-log viewpoint bench 2.2 x 0.7 m, log sleepers, 2-plank seat top z %.2f, two-rail back (top z 0.97) on the +Y side; "
                        "seats face the front (-Y)" % zt}


# =========================================================================================================
# trail_marker
# =========================================================================================================

def trail_marker(variant):
    b = Builder("trail_marker")
    H = 1.2
    b.cyl(V(0.0, 0.0, -0.05), V(0.0, 0.0, H - 0.06), 0.062, 0.056, seg=7, tile="plank_edge", cap_tile="endgrain", cap0=False, cap1=False)
    b.cyl(V(0.0, 0.0, H - 0.06), V(0.0, 0.0, H), 0.056, 0.032, seg=7, tile="plank_edge", cap_tile="endgrain", cap0=False)   # chamfered top
    b.cyl(V(0.0, 0.0, 0.86), V(0.0, 0.0, 0.98), 0.0665, 0.0655, seg=7, flatc="red_paint")                                  # the painted ring
    st = [((0.17, -0.06), (0.30, 0.26, 0.17), 1), ((-0.14, -0.13), (0.26, 0.22, 0.16), 1), ((-0.05, 0.17), (0.28, 0.24, 0.16), 1),
          ((0.08, 0.10), (0.18, 0.16, 0.11), 0), ((-0.12, 0.03), (0.17, 0.15, 0.11), 0), ((0.06, -0.11), (0.16, 0.14, 0.10), 0)]
    zs = [0.085, 0.08, 0.08, 0.215, 0.22, 0.20]
    for i, ((x, y), sz, sd) in enumerate(st):
        rock(b, V(x, y, zs[i] * 0.78), sz, seed=50 + i, subdiv=1 + sd, flatten=0.0, rough=0.12)
    rock(b, V(0.0, -0.075, 0.3), (0.13, 0.11, 0.09), seed=60, subdiv=1, flatten=0.0, rough=0.1)
    return b, {"notes": "trail marker: peeled post 1.2 m with a red painted ring at z 0.86..0.98 and a small cairn of stones at its foot "
                        "(~ 0.7 m across, 0.36 m high); round, no front"}


# =========================================================================================================
# spring_bench
# =========================================================================================================

def spring_bench(variant):
    b = Builder("spring_bench")
    L, D, zt = 1.3, 0.36, 0.40
    for sx in (-1, 1):
        for sy, top in ((-1, zt - 0.08), (1, 1.5)):
            b.box(V(sx * (L / 2 - 0.07), sy * 0.14, top / 2 - 0.0), (0.075, 0.075, top + 0.0), "wood_dark", grain=2, tiles={"top": "endgrain"})
    for y in (-0.086, 0.086):
        plank_x(b, V(0, y, zt - 0.03), (L, 0.165, 0.06), "wood")
    for sy in (-1, 1):
        plank_x(b, V(0, sy * 0.14, 0.17), (L - 0.14, 0.04, 0.07), "wood_dark")        # stretchers
    plank_x(b, V(0, 0.095, 1.28), (L, 0.03, 0.24), "wood")                               # the back board with the pegs, in front of the rear posts
    for x in (-0.4, 0.0, 0.4):
        b.cyl(V(x, 0.08, 1.3), V(x, -0.04, 1.34), 0.019, 0.016, seg=6, tile="plank_edge", cap_tile="plank_edge")
    return b, {"seats": [[-0.32, 0.0, zt, 0.0], [0.32, 0.0, zt, 0.0]],
               "notes": "low changing bench 1.3 x 0.36 m, seat top z %.2f, back board at z 1.16..1.40 on the +Y side with three coat pegs at x -0.4 / 0 / 0.4 "
                        "(z 1.3..1.34, pointing to -Y); seats face the front (-Y)" % zt}


# =========================================================================================================
# spring_steps
# =========================================================================================================

STEP_RUN, STEP_DROP, N_STEPS = 1.4, 0.9, 3
STEP_W = 1.2


def spring_steps(variant):
    b = Builder("spring_steps")
    rnd = random.Random(77)
    run_i, drop_i = STEP_RUN / N_STEPS, STEP_DROP / N_STEPS
    for i in range(N_STEPS):
        ztop = -drop_i * (i + 1)
        y0, y1 = -run_i * i, -run_i * (i + 1)                 # tread spans y1..y0 (it ends at the nosing y1)
        # a split log: flat face up, round side down (a half-round, 0.34 wide)
        rr = 0.2
        cy = (y0 + y1) / 2
        n = 6
        prof = []
        for k in range(n + 1):
            a = math.pi * k / n
            prof.append((-0.5 * (run_i + 0.04) * math.cos(a), -rr * 0.9 * math.sin(a)))     # outline in (y-ish, z) from -w/2 to +w/2 (round side below)
        outline = [(p[0], p[1]) for p in prof]
        b.prism(outline, V(0, cy, ztop), (V(0, 1, 0), V(0, 0, 1), V(1, 0, 0)), STEP_W, tile="wood", side_tile=lambda k: "wood", cap_tile="endgrain")
        for sx in (-1, 1):                                     # two stakes in front of each step
            x = sx * (STEP_W / 2 - 0.1)
            b.cyl(V(x, y1 - 0.06, ztop - 0.55), V(x, y1 - 0.06, ztop + 0.0), 0.05, 0.05, seg=6, tile="wood_dark", cap_tile="endgrain", wobble=0.03)
    # handrail on the +X side: three round posts and one sloped pole, continuing 0.35 m up the bank
    xr = STEP_W / 2 + 0.12
    slope = STEP_DROP / STEP_RUN
    top_y, bot_y = 0.35, -STEP_RUN - 0.05
    hz = lambda y: slope * y + 0.95 + 0.0                      # rail height along the chord (parallel to the nosing line)
    ys = [0.3, -0.45, -1.2]
    for y in ys:
        zrail = hz(y)
        b.cyl(V(xr, y, slope * y - 0.45), V(xr, y, zrail + 0.05), 0.05, 0.045, seg=6, tile="wood_dark",
              cap_tile="endgrain", wobble=0.03)
    b.cyl(V(xr, top_y, hz(top_y)), V(xr, bot_y, hz(bot_y)), 0.04, 0.04, seg=6, tile="wood", cap_tile="endgrain", wobble=0.02)
    # collision: one sloped box under the treads, top plane 0.15 m under the nosing line (mid-tread)
    ang = math.degrees(math.atan2(STEP_DROP, STEP_RUN))
    length = math.hypot(STEP_RUN, STEP_DROP)
    thick = 0.3
    cy, cz = -STEP_RUN / 2, -STEP_DROP / 2
    nrm = (0.0, math.sin(math.radians(ang)), math.cos(math.radians(ang)))      # up-facing normal of the slope plane
    mid = (0.0, cy - nrm[1] * (thick / 2 + 0.15), cz - nrm[2] * (thick / 2 + 0.15))
    boxes = [{"name": "slope", "center": [0.0, round(mid[1], 3), round(mid[2], 3)], "size": [STEP_W, round(length, 3), thick],
              "rot_x_deg": round(ang, 3)}]
    notes = ("three split-log steps %.1f m wide with a pole handrail on the +X side, leading down into a pool: origin = the top step's back edge at "
             "ground level (y 0, z 0); the steps descend %.1f m over %.1f m toward the front (-Y): treads at z -0.3 / -0.6 / -0.9, noses at y %.2f / %.2f / %.2f; "
             "the rail runs 0.35 m up the bank (+Y); collision is one sloped box (collision_boxes, mesh space, rot_x_deg is a right-handed "
             "rotation about Blender +X: positive tips the -Y end down; top plane 0.15 m below the nosing line); stakes reach z -1.45"
             % (STEP_W, STEP_DROP, STEP_RUN, -run_i, -2 * run_i, -3 * run_i))
    return b, {"notes": notes, "manifest": {"axis": "y", "run": STEP_RUN, "drop": STEP_DROP, "slope_deg": round(ang, 3), "collision_boxes": boxes}}


# =========================================================================================================
# gauge_post
# =========================================================================================================

def gauge_post(variant):
    b = Builder("gauge_post")
    b.cyl(V(0, 0, -0.3), V(0, 0, 2.2), 0.04, 0.04, seg=6, flatc="steel", cap_tile="metal")
    b.box(V(0, 0, 2.2), (0.09, 0.09, 0.03), "metal", grain=0, flatc="steel")                           # cap plate
    yb = -0.04 - 0.012
    z0, z1 = 0.18, 2.0
    b.box(V(0, yb, (z0 + z1) / 2), (0.15, 0.024, z1 - z0), "metal", flatc="black_paint")
    n = 9
    h = (z1 - z0) / (2 * n)
    for k in range(n):
        zc = z0 + h * (2 * k + 1.5)
        b.box(V(0, yb - 0.012 - 0.0015, zc), (0.15, 0.007, h), "metal", flatc="white_paint")
    for z in (0.5, 1.7):
        b.box(V(0, -0.04 - 0.0, z), (0.11, 0.05, 0.035), "metal", flatc="steel")                       # bracket straps
    return b, {"notes": "hydrologist's staff gauge: steel post (round, r 0.04) 2.2 m above the ground (0.3 m more buried), the white-and-black striped board "
                        "(0.15 wide, z 0.18..2.0, nine 0.1 m white bands) on its front face (-Y): stand it with the board facing the channel"}


# =========================================================================================================
# rowboat
# =========================================================================================================

def rowboat(variant):
    b = Builder("rowboat")
    LEN = 4.0
    ST = 14

    def wg(s):
        if s <= 0.42:
            return lerp(0.6, 0.72, smooth(s / 0.42))
        t = (s - 0.42) / 0.58
        return 0.72 * (1 - t ** 1.7) + 0.035 * t ** 1.7 + 0.0

    def wb(s):
        return max(0.03, 0.58 * wg(s))

    def zg(s):
        return 0.015 + 0.2 * (1 - (2 * s - 1) ** 2)

    def dep(s):
        return lerp(0.40, 0.54, smooth(s / 0.4)) if s < 0.4 else lerp(0.54, 0.24, ((s - 0.4) / 0.6) ** 1.5)

    OFF = (0.07, 0.035, 0.0)        # clinker laps: the upper strakes stand out over the lower ones

    def section(s):
        """Points (y, z) round the section: left gunwale -> left chine (three strakes, a lap step between each), the flat bottom,
        right chine -> right gunwale."""
        w_g, w_b, z_g = wg(s), wb(s), zg(s)
        z_b = z_g + dep(s)
        k = w_g / 0.72
        left = []
        for j in range(3):
            for t in (j / 3, (j + 1) / 3):
                left.append((-(lerp(w_g, w_b, t) + OFF[j] * k), lerp(z_g, z_b, t)))
        return left + [(-y, z) for (y, z) in left[::-1]]

    ss = [i / (ST - 1) for i in range(ST)]
    rings = []
    for s in ss:
        x = -LEN / 2 + LEN * s
        rings.append([V(x, y, z) for (y, z) in section(s)])
    nb = len(rings[0])                                          # 12 points; strip k joins point k to k+1 (the last joins back to the first)
    # strips: strake, lap step, strake, lap step, strake | bottom plank | strake, step, strake, step, strake | the hollow underside
    tiles = ["wood", "wood_dark", "wood_dark", "wood_dark", "wood", "wood_dark", "wood", "wood_dark", "wood_dark", "wood_dark", "wood", None]
    for i in range(ST - 1):
        for k in range(nb):
            k2 = (k + 1) % nb
            q = [rings[i][k], rings[i + 1][k], rings[i + 1][k2], rings[i][k2]]
            cen = V((q[0].x + q[2].x) / 2, 0.0, (rings[i][0].z + rings[i][5].z) / 2 + 0.2)
            nrm = (q[1] - q[0]).cross(q[3] - q[0])
            mid = sum(q, Vector()) / 4
            flipped = nrm.dot(mid - cen) < 0
            if flipped:
                q = q[::-1]
            tl = tiles[k]
            if tl is None:
                uvs = [T.swatch_uv("dark")] * 4
            else:
                u0, v0, du, dv = b.win(tl)
                band = k / nb
                ua, ub = 0.04 + 0.92 * ss[i], 0.04 + 0.92 * ss[i + 1]
                va, vb = 0.03 + band * 0.9, 0.03 + (band + 0.07) * 0.9
                uvs = [(u0 + ua * du, v0 + va * dv), (u0 + ub * du, v0 + va * dv), (u0 + ub * du, v0 + vb * dv), (u0 + ua * du, v0 + vb * dv)]
                if flipped:
                    uvs = uvs[::-1]
            b.face(q, uvs, smooth=False)
    # end caps: the stern transom and the stem
    for idx, outward in ((0, -1), (ST - 1, 1)):
        ring = rings[idx]
        poly = ring if normal([p for p in ring]).x * outward >= 0 else ring[::-1]
        tl = "wood_dark" if idx == 0 else "wood"
        u0, v0, du, dv = b.win(tl)
        ys = [p.y for p in poly]
        zs = [p.z for p in poly]
        wy, wz = max(max(ys) - min(ys), 0.05), max(max(zs) - min(zs), 0.05)
        uvs = [(u0 + (0.1 + 0.8 * (p.y - min(ys)) / wy) * du, v0 + (0.1 + 0.8 * (p.z - min(zs)) / wz) * dv) for p in poly]
        b.face(poly, uvs, smooth=False)
    # gunwale rails (outwale), the keel batten and chine battens: thin tubes on the hull
    def path(f, lo=0.0, hi=1.0, n=9):
        return [lo + (hi - lo) * i / (n - 1) for i in range(n)]
    for sg in (-1, 1):
        pts = [V(-LEN / 2 + LEN * s, sg * (wg(s) + 0.086 * wg(s) / 0.72), zg(s) + 0.012) for s in path(0, 0.0, 0.97, 7)]
        b.tube(pts, 0.026, seg=4, tile="wood_dark", caps=True)
        pts = [V(-LEN / 2 + LEN * s, sg * (wb(s) + 0.012), zg(s) + dep(s) - 0.01) for s in path(0, 0.02, 0.95, 7)]
        b.tube(pts, 0.022, seg=4, tile="wood_dark", caps=True)
    pts = [V(-LEN / 2 + LEN * s, 0.0, zg(s) + dep(s) + 0.012) for s in path(0, 0.0, 0.96, 7)]
    b.tube(pts, 0.03, seg=4, tile="wood_dark", caps=True)
    return b, {"notes": "old flat-bottomed wooden rowboat lying upturned on the bank, 4.0 m long along X (stern/transom at -X, bow at +X), 1.44 m beam, "
                        "flat bottom up at z ~ 0.5, gunwales touching the ground at both ends with a ~0.15 m hollow gap in the middle; "
                        "no front (use yaw to lay it along the bank)"}

