"""Park furniture of the club's alley and the beach halt (docs/south.md): park_lamp, park_bench, flower_bed, litter_urn,
bike_rack, halt_shelter, drinking_fountain.

Built with the North's kit and atlas (blender/north/northkit.py, northtex.py; existing tiles and paint swatches only, no
words) in the style of props_places.py: its PB builder (faces wound outward on purpose), planar UVs at a fixed
metres-per-tile scale (long faces are cut into pieces no longer than one tile), deterministic seeds. Frame as everywhere:
metres, Z up, Blender -Y is the front, origin on the ground under the middle, transforms applied; posts and feet go
0.2..0.4 m under the ground.
    python3 blender/south/build_south.py props_alley [kind ...]
"""
import math
import random

from mathutils import Matrix, Vector

import northtex as T
from northkit import V
from props_places import (PB, beam, box, cbox, hexa, lathe, lerp, M, poly, pole, quad, rx, rz, tube)

# swatch colours used here: bag_green (park paint), enamel_white (globes), rust, steel, tin, black_paint, ground_dark,
# yellow_sign, red_paint, bag_orange, white_paint


def lathe_in(b, profile, c, seg, tile):
    """The inside of a surface of revolution: profile [(r, z)...] in any order, faces wound toward the axis / upward."""
    c = Vector(c)
    P = lambda r, z, a: c + Vector((math.cos(a) * r, math.sin(a) * r, z))
    for (ra, za), (rb, zb) in zip(profile, profile[1:]):
        dr, dz = rb - ra, zb - za
        nr, nz = dz, -dr
        if nz < -1e-9 or (abs(nz) <= 1e-9 and nr > 0):
            nr, nz = -nr, -nz
        for i in range(seg):
            a0, a1 = 2 * math.pi * i / seg, 2 * math.pi * (i + 1) / seg
            am = (a0 + a1) / 2
            q = [P(ra, za, a0), P(ra, za, a1), P(rb, zb, a1), P(rb, zb, a0)]
            if ra < 1e-6:
                q = [q[0], q[2], q[3]]
            elif rb < 1e-6:
                q = q[:3]
            poly(b, q, tile, out=V(nr * math.cos(am), nr * math.sin(am), nz))


def spike(b, base, tip, r, seg, tile, rot0=0.0):
    """An open cone (seg triangles) from a base circle of radius r about `base` up to `tip`: a blade of grass."""
    base, tip = Vector(base), Vector(tip)
    z = (tip - base).normalized()
    h = Vector((1, 0, 0)) if abs(z.x) < 0.9 else Vector((0, 1, 0))
    x = z.cross(h).normalized()
    y = z.cross(x).normalized()
    ring = [base + (x * math.cos(rot0 + 2 * math.pi * i / seg) + y * math.sin(rot0 + 2 * math.pi * i / seg)) * r for i in range(seg)]
    mid = (base + tip) / 2
    for i in range(seg):
        poly(b, [ring[i], ring[(i + 1) % seg], tip], tile, inside=mid)


def bipyramid(b, c, r, h, seg, tile, rot0=0.0):
    """A flower head: a closed double pyramid (2 * seg triangles), r across, h half-height."""
    c = Vector(c)
    ring = [c + V(math.cos(rot0 + 2 * math.pi * i / seg) * r, math.sin(rot0 + 2 * math.pi * i / seg) * r, 0) for i in range(seg)]
    top, bot = c + V(0, 0, h), c - V(0, 0, h * 0.6)
    for i in range(seg):
        poly(b, [ring[i], ring[(i + 1) % seg], top], tile, inside=c)
        poly(b, [ring[(i + 1) % seg], ring[i], bot], tile, inside=c)


def blk(b, x0, x1, y0, y1, zb, zt, tile, grain=None, tiles=None, skip=()):
    """A block between x0..x1, y0..y1 whose bottom zb and top zt are numbers or functions of y (a sloped soffit or top)."""
    fb = zb if callable(zb) else (lambda y, v=zb: v)
    ft = zt if callable(zt) else (lambda y, v=zt: v)
    P = [V(x0, y0, fb(y0)), V(x1, y0, fb(y0)), V(x1, y1, fb(y1)), V(x0, y1, fb(y1)),
         V(x0, y0, ft(y0)), V(x1, y0, ft(y0)), V(x1, y1, ft(y1)), V(x0, y1, ft(y1))]
    hexa(b, P, tile, grain=grain, tiles=tiles, skip=skip)


def flat_patch(b, c, u, v, w, h, tile, out):
    """A small flat quad (a peeled patch, a chip) centred at c, w along u and h along v, facing `out`."""
    c, u, v = Vector(c), Vector(u), Vector(v)
    pts = [c - u * (w / 2) - v * (h / 2), c + u * (w / 2) - v * (h / 2), c + u * (w / 2) + v * (h / 2), c - u * (w / 2) + v * (h / 2)]
    poly(b, pts, tile, out=Vector(out))


# =========================================================================================================
# park_lamp
# =========================================================================================================

LAMP_Z0, LAMP_Z1 = -0.3, 3.62          # the post: buried 0.3 m, top
LAMP_R0, LAMP_R1 = 0.075, 0.045


def lamp_r(z):
    return lerp(LAMP_R0, LAMP_R1, (z - LAMP_Z0) / (LAMP_Z1 - LAMP_Z0))


def globe(b, c, r, seg=10):
    prof = [(0.0, -r), (r * 0.707, -r * 0.707), (r, 0.0), (r * 0.707, r * 0.707), (0.0, r)]
    lathe(b, prof, c, seg=seg, tile="enamel_white")


def park_lamp(variant):
    b = PB("park_lamp_v%d" % variant)
    rnd = random.Random(7 + variant)
    # cast foot (buried 0.3 m) and the post, tapered, painted park green
    lathe(b, [(0.17, LAMP_Z0), (0.17, 0.02), (0.14, 0.12), (0.10, 0.24), (0.074, 0.40)], V(0, 0, 0), seg=8, tile="metal")
    pole(b, V(0, 0, LAMP_Z0), V(0, 0, LAMP_Z1), LAMP_R0, LAMP_R1, seg=8, tile="bag_green", cap0=False, cap1=False)
    # peeled paint: rust and bare-steel patches on the flat faces of the post (the post is an octagon: face centres at (i+0.5)*45 deg)
    for k in range(7):
        i = rnd.randrange(8)
        a = (i + 0.5) * math.pi / 4
        z = rnd.uniform(0.6, 3.3)
        tile = ("rust", "metal")[k % 2]
        d = Vector((math.cos(a), math.sin(a), 0))
        t = Vector((-math.sin(a), math.cos(a), 0))
        flat_patch(b, d * (lamp_r(z) * math.cos(math.pi / 8) + 0.003) + V(0, 0, z), t, V(0, 0, 1),
                   rnd.uniform(0.025, 0.04), rnd.uniform(0.07, 0.2), tile, d)
    # a cast collar under the bracket
    lathe(b, [(0.046, 3.38), (0.074, 3.43), (0.074, 3.5), (0.05, 3.56)], V(0, 0, 0), seg=8, tile="metal")
    if variant == 0:
        # one globe under a conical shade at the end of a curved arm falling to the front (-Y)
        pts = [V(0, 0, 3.52), V(0, 0, 3.95)]
        for th in (30, 60, 90):
            t = math.radians(th)
            pts.append(V(0, -0.22 + 0.22 * math.cos(t), 3.95 + 0.22 * math.sin(t)))
        pts.append(V(0, -0.40, 4.15))
        tube(b, pts, 0.02, seg=5, tile="metal", caps=False, rscale=lambda i, n: 1.15 - 0.3 * i / (n - 1))
        cx, cy = 0.0, -0.42
        lathe(b, [(0.0, 4.2), (0.26, 4.03)], V(cx, cy, 0), seg=10, tile="metal")                 # the conical shade
        lathe(b, [(0.0, 4.03), (0.26, 4.03)], V(cx, cy, 0), seg=10, tile="enamel_white")         # its white underside
        globe(b, V(cx, cy, 3.855), 0.17)
        lamps = [[cx, cy, 3.855]]
        notes_arm = "one globe (diameter 0.34) under a conical shade (top z 4.20, diameter 0.52) on a curved arm that falls to the front (-Y)"
    else:
        # a T: the post ends in a cross bar along X curving up at both ends, a globe standing in a cup on each end
        pts = [V(-0.56, 0, 3.97), V(-0.44, 0, 3.80), V(-0.2, 0, 3.7), V(0, 0, 3.67), V(0.2, 0, 3.7), V(0.44, 0, 3.80), V(0.56, 0, 3.97)]
        tube(b, pts, 0.02, seg=5, tile="metal", caps=True)
        pole(b, V(0, 0, 3.5), V(0, 0, 3.69), 0.04, seg=6, tile="bag_green", cap0=False, cap1=False)
        lamps = []
        for sx in (-1, 1):
            x = sx * 0.5
            lathe(b, [(0.03, 3.84), (0.09, 3.92), (0.12, 4.0), (0.12, 4.04)], V(x, 0, 0), seg=8, tile="metal")
            globe(b, V(x, 0, 4.03), 0.17)
            lamps.append([x, 0.0, 4.03])
        notes_arm = "two globes (diameter 0.34 each) in cast cups on a T-bar 1.12 m wide along X (bar z 3.67..3.97)"
    lamps = [[round(v, 3) for v in p] for p in lamps]
    notes = ("late-Soviet park lamp post %.1f m high, origin on the ground under the post: cast iron foot (r 0.17, buried 0.3 m), tapered "
             "octagonal steel post painted park green (r 0.075 -> 0.045, top z %.2f), paint peeling to rust and bare steel, a cast collar, %s. "
             "No front (round post): the arm/bar points %s. Collision: box (collision_boxes: the post only, 0.34 m square to z 3.8; the "
             "arm and globes are out of reach). manifest `lamps` = the globes' centres [x, y, z] for future lights"
             % (4.2, LAMP_Z1, notes_arm, "to -Y (front)" if variant == 0 else "along X"))
    boxes = [cbox("post", (0, 0, 1.9), (0.34, 0.34, 3.8))]
    return b, {"notes": notes, "manifest": {"lamps": lamps, "collision_boxes": boxes}}


# =========================================================================================================
# park_bench
# =========================================================================================================

BENCH_HALF = 0.9              # centre of each cast end; ends 0.1 thick -> 1.9 m over all
END_PROFILE = [(-0.26, -0.2), (-0.14, -0.2), (-0.14, 0.30), (0.19, 0.30), (0.19, -0.2), (0.33, -0.2), (0.33, 0.40), (0.44, 0.88),
               (0.35, 0.88), (0.27, 0.45), (0.27, 0.41), (-0.26, 0.41)]


def _signed_area(prof):
    return 0.5 * sum(prof[i][0] * prof[(i + 1) % len(prof)][1] - prof[(i + 1) % len(prof)][0] * prof[i][1] for i in range(len(prof)))


def prism_x2(b, x0, x1, prof, tile):
    """As prism_x but with the edge normals from the polygon's winding (right for concave profiles)."""
    if _signed_area(prof) < 0:
        prof = prof[::-1]
    n = len(prof)
    poly(b, [V(x0, y, z) for y, z in prof], tile, out=V(-1, 0, 0))
    poly(b, [V(x1, y, z) for y, z in prof], tile, out=V(1, 0, 0))
    for i in range(n):
        (ya, za), (yb, zb) = prof[i], prof[(i + 1) % n]
        nrm = V(0, zb - za, -(yb - ya))            # counter-clockwise in (y, z): the right-hand normal points outward
        quad(b, [V(x0, ya, za), V(x1, ya, za), V(x1, yb, zb), V(x0, yb, zb)], tile, out=nrm)


def park_bench(variant):
    b = PB("park_bench_v%d" % variant)
    rnd = random.Random(11 + variant)
    worn = variant == 1
    for sx in (-1, 1):
        prism_x2(b, sx * BENCH_HALF - 0.05, sx * BENCH_HALF + 0.05, END_PROFILE, "ash")
    paint = "bag_green"
    LS = 1.88
    # seat: four slats (z 0.41..0.45), the paint worn through to bare wood on some in variant 1
    ys = [-0.18, -0.06, 0.06, 0.18]
    seat_tiles = [paint, paint, paint, paint] if not worn else ["wood", paint, "wood", paint]
    for y, t in zip(ys, seat_tiles):
        box(b, V(rnd.uniform(-0.004, 0.004), y, 0.43), (LS, 0.105, 0.04), t, grain=0, rot=rz(rnd.uniform(-0.004, 0.004)))
    # back: three slats leaning with the cast struts (front plane of the struts: y = 0.27 + (z - 0.45) * 0.186)
    th = math.atan(0.08 / 0.43)
    back_z = [0.57, 0.69, 0.81]
    back_tiles = [paint, paint, paint] if not worn else [paint, "wood", "wood"]
    for k, (z, t) in enumerate(zip(back_z, back_tiles)):
        if worn and k == 1:
            continue                                   # the missing slat (v1): the middle one of the back
        yc = 0.27 + (z - 0.45) * (0.08 / 0.43) - 0.0145
        box(b, V(0, yc, z), (LS, 0.025, 0.095), t, grain=0, rot=rx(-th))
    # peeled patches on the painted slats' top faces (bare wood chips and rust)
    chips = [(-0.55, 0.18, 0.8), (0.3, -0.18, 1.0), (0.7, 0.06, 0.7)] if not worn else [(-0.7, -0.06, 1.0), (0.4, 0.18, 1.2), (0.62, -0.06, 0.8), (-0.2, 0.18, 0.9)]
    for k, (x, y, s) in enumerate(chips):
        if seat_tiles[ys.index(y)] == "wood":
            continue
        flat_patch(b, V(x, y, 0.4535), V(1, 0, 0), V(0, 1, 0), 0.15 * s, 0.05, ("wood", "rust")[k % 2], V(0, 0, 1))
    seats = [[round(x, 3), -0.02, 0.45, 0.0] for x in (-0.56, 0.0, 0.56)]
    notes = ("park bench 1.9 m long (x) x 0.7 m deep (y -0.26..0.44 at the top of the back) x 0.88 m high, origin on the ground under the "
             "middle: two cast concrete ends (0.1 thick, x +-0.9, feet buried to z -0.2), four seat slats (top z 0.45, painted park green) "
             "and three leaning back slats (z 0.52..0.86), back on +Y, the sitter faces -Y; 3 seats at x -0.56 / 0 / 0.56 (z 0.45, yaw 0 = "
             "facing -Y). %s Collision: exact"
             % ("Variant 1: the middle back slat missing and the paint worn through to bare wood on several slats." if worn
                else "Variant 0: whole, a few small peeled patches."))
    return b, {"seats": seats, "notes": notes}


# =========================================================================================================
# flower_bed
# =========================================================================================================

def flower_bed(variant):
    b = PB("flower_bed")
    rnd = random.Random(21)
    seg = 12
    # the bowl: a short foot (r 0.30, buried 0.2 m), a flaring bowl to the rim (r 0.8, top z 0.58) and the rim's flat top
    lathe(b, [(0.30, -0.2), (0.30, 0.12), (0.50, 0.20), (0.72, 0.34), (0.80, 0.50), (0.80, 0.58), (0.68, 0.58)], V(0, 0, 0), seg=seg, tile="ash")
    # the earth, a low mound inside the rim
    lathe(b, [(0.0, 0.67), (0.36, 0.64), (0.68, 0.58)], V(0, 0, 0), seg=seg, tile="ground_dark")
    # tufts: a stem of leaf-blades and a flower head on top, in rings from the centre out (taller in the middle)
    heads = ["yellow_sign", "red_paint", "bag_orange", "enamel_white", "red_paint", "yellow_sign"]
    n = 0
    for ring, (rad, count, hgt) in enumerate(((0.0, 1, 0.34), (0.2, 5, 0.27), (0.42, 8, 0.21), (0.6, 10, 0.16))):
        for i in range(count):
            a = 2 * math.pi * i / count + rnd.uniform(-0.2, 0.2) + ring
            r = rad + rnd.uniform(-0.03, 0.03)
            ground = lerp(0.67, 0.585, min(1.0, r / 0.68))
            h = hgt * rnd.uniform(0.85, 1.15)
            lean = rnd.uniform(0.0, 0.05) * (1 if r > 0.1 else 0)
            base = V(math.cos(a) * r, math.sin(a) * r, ground - 0.02)
            tip = base + V(math.cos(a) * lean, math.sin(a) * lean, h + 0.02)
            spike(b, base, tip, 0.022, 3, "bag_green", rot0=a)
            if n % 5 != 4:                                             # one in five is just green
                bipyramid(b, tip, 0.05, 0.035, 5, heads[n % len(heads)], rot0=a)
            n += 1
    notes = ("round concrete flower bowl 1.6 m across (rim r 0.8, rim top z 0.58) on a short round foot (r 0.30, buried 0.2 m), origin on the "
             "ground under the middle; dark earth mounded inside the rim and %d low tufts (green blades, flower heads in yellow, red, orange "
             "and white, the tallest in the middle, highest flower head z 1.06); round, no front; Collision: box" % n)
    return b, {"notes": notes}


# =========================================================================================================
# litter_urn
# =========================================================================================================

def litter_urn(variant):
    b = PB("litter_urn")
    seg = 10
    prof = [(0.21, -0.15), (0.21, 0.04), (0.16, 0.08), (0.15, 0.20), (0.24, 0.38), (0.27, 0.52), (0.26, 0.64), (0.29, 0.70)]
    lathe(b, prof, V(0, 0, 0), seg=seg, tile="ash")
    lathe(b, [(0.29, 0.70), (0.2, 0.70)], V(0, 0, 0), seg=seg, tile="ash")                   # the rim's flat top
    lathe_in(b, [(0.2, 0.70), (0.19, 0.60)], V(0, 0, 0), seg, "ash")                          # the inside wall
    lathe(b, [(0.0, 0.5), (0.19, 0.5)], V(0, 0, 0), seg=seg, tile="black_paint")             # the dark of the inside
    notes = ("concrete urn-shaped litter bin 0.7 m high (foot r 0.21 buried 0.15 m, belly r 0.27 at z 0.52, rim r 0.29 at z 0.70, mouth r 0.20 "
             "with a dark bottom), origin on the ground under the middle; round, no front; Collision: box")
    return b, {"notes": notes}


# =========================================================================================================
# bike_rack
# =========================================================================================================

def bike_rack(variant):
    b = PB("bike_rack")
    xs = [-0.9, -0.45, 0.0, 0.45, 0.9]
    ZB = 0.09
    for sx in (-1, 1):                                              # concrete feet, buried to z -0.3
        box(b, V(sx * 1.1, 0, -0.08), (0.2, 0.56, 0.44), "ash", grain=2, tiles={"top": "ash", "side": "ash"})
    for sy in (-1, 1):                                              # two long steel tubes between the feet carry the hoops
        tube(b, [V(-1.12, sy * 0.21, ZB), V(1.12, sy * 0.21, ZB)], 0.02, seg=4, tile="metal", caps=True)
    for x in xs:                                                    # five inverted-U hoops (inside width 0.42, top z 0.71)
        pts = [V(x, -0.21, ZB), V(x, -0.21, 0.53), V(x, -0.15, 0.67), V(x, -0.07, 0.70), V(x, 0.07, 0.70), V(x, 0.15, 0.67), V(x, 0.21, 0.53), V(x, 0.21, ZB)]
        tube(b, pts, 0.018, seg=4, tile="metal", caps=True)
    notes = ("steel hoop rack for five bicycles, 2.4 m long along X (x -1.2..1.2), 0.56 m deep, origin on the ground under the middle: five "
             "inverted-U hoops (0.42 m wide, top z 0.72, 0.45 m apart, x -0.9..0.9) welded to two steel tubes along X at z 0.09, which end "
             "in two concrete feet (0.2 x 0.56 m, top z 0.14, buried to z -0.3). A bicycle's front wheel goes between two hoops. "
             "Symmetric front to back (no front; yaw freely). Collision: box")
    return b, {"notes": notes}


# =========================================================================================================
# halt_shelter
# =========================================================================================================

def zr(y):
    """The roof deck's underside: 2.40 at the front (y -1) falling to 2.00 at the back (y +1)."""
    return 2.40 - 0.20 * (y + 1.0)


HX, HY = 2.0, 1.0
PX, PY = 1.88, 0.88              # post centres
FL = 0.12                        # floor top


def halt_shelter(variant):
    b = PB("halt_shelter")
    rnd = random.Random(33)
    # floor: a concrete slab 4 x 2 m, top at z 0.12, foundation to z -0.3
    box(b, V(0, 0, (FL - 0.3) / 2), (2 * HX, 2 * HY, FL + 0.3), "ash")
    # four concrete posts 0.16 square, slanted top under the rafters, buried to z -0.3
    for sx in (-1, 1):
        for sy in (-1, 1):
            y = sy * PY
            blk(b, sx * PX - 0.08, sx * PX + 0.08, y - 0.08, y + 0.08, -0.3, lambda yy: zr(yy) - 0.12, "ash", grain=V(0, 0, 1))
        # mid posts of the half sides (timber 0.12)
        blk(b, sx * PX - 0.06, sx * PX + 0.06, -0.06, 0.06, FL, lambda yy: zr(yy) - 0.12, "wood_dark", grain=V(0, 0, 1))
    # wall plates under the rafters: front (an open header) and back, 0.1 x 0.12
    for y in (-PY, PY):
        blk(b, -HX + 0.0, HX - 0.0, y - 0.05, y + 0.05, lambda yy: zr(yy) - 0.12, lambda yy: zr(yy), "wood_dark", grain=V(1, 0, 0))
    # back wall: vertical boards between the posts (x -1.80..1.80), z FL .. under the back plate
    n = 18
    wb = 3.6 / n
    zt = zr(PY) - 0.12
    for i in range(n):
        x0 = -1.8 + i * wb
        th = 0.02 if i % 2 else 0.016
        blk(b, x0, x0 + wb, PY - th, PY + th, FL, zt, ("wood", "wood_dark")[(i + i // 3) % 2], grain=V(0, 0, 1))
    # the half sides: boards between the mid post (y 0.06) and the back post (y 0.80), tops sloping under the side rafter
    nb = 4
    wsb = (0.80 - 0.06) / nb
    for sx in (-1, 1):
        for i in range(nb):
            y0 = 0.06 + i * wsb
            blk(b, sx * PX - (0.02 if i % 2 else 0.016), sx * PX + (0.02 if i % 2 else 0.016), y0, y0 + wsb, FL, lambda yy: zr(yy) - 0.12, ("wood_dark", "wood")[(i + (sx > 0)) % 2], grain=V(0, 0, 1))
    # rafters along Y (x -1.88, -0.94, 0, 0.94, 1.88): 0.07 wide, 0.12 deep, front overhang to y -1.3, back to 1.15
    for x in (-PX, -PX / 2, 0.0, PX / 2, PX):
        blk(b, x - 0.035, x + 0.035, -1.3, 1.15, lambda yy: zr(yy) - 0.12, lambda yy: zr(yy), "wood", grain=V(0, 1, 0))
    # the roof: sheet iron over the deck boards, 4.4 x 2.45 m, 0.05 thick, falling to the back
    blk(b, -2.2, 2.2, -1.3, 1.15, lambda yy: zr(yy), lambda yy: zr(yy) + 0.05, "metal", grain=V(1, 0, 0), tiles={"top": "metal", "bottom": "wood", "side": "wood_dark"})
    # a bench along the back wall: three cast concrete supports and two plank slats, seat top z 0.57
    for x in (-1.6, 0.0, 1.6):
        box(b, V(x, 0.67, FL + 0.205), (0.12, 0.36, 0.41), "ash", grain=2)
    for y in (0.585, 0.775):
        box(b, V(0, y, 0.55), (3.5, 0.17, 0.04), "wood", grain=0, rot=rz(rnd.uniform(-0.002, 0.002)))
    seats = [[x, 0.68, 0.57, 0.0] for x in (-1.1, 0.0, 1.1)]
    boxes = [cbox("floor", (0, 0, (FL - 0.3) / 2), (4.0, 2.0, FL + 0.3)),
             cbox("wall_back", (0, 0.88, FL + 0.93), (3.9, 0.12, 1.86)),
             cbox("wall_west", (-PX, 0.43, FL + 0.93), (0.12, 0.74, 1.86)),
             cbox("wall_east", (PX, 0.43, FL + 0.93), (0.12, 0.74, 1.86)),
             cbox("post_front_west", (-PX, -PY, 1.0), (0.16, 0.16, 2.0)),
             cbox("post_front_east", (PX, -PY, 1.0), (0.16, 0.16, 2.0)),
             cbox("bench", (0, 0.67, FL + 0.225), (3.5, 0.36, 0.45)),
             cbox("roof", (0, -0.075, zr(-0.075) + 0.025), (4.4, math.hypot(2.45, 0.49), 0.05), rot_x_deg=-math.degrees(math.atan(0.2)))]
    notes = ("small open waiting shelter of a seaside railway halt, origin on the ground under the middle of the 4 x 2 m concrete slab (top "
             "z 0.12, foundation to z -0.3): four concrete posts, a back wall (+Y) and two half side walls (y 0.06..0.80, the front halves open) "
             "of vertical boards, five rafters and an iron mono-pitch roof 4.4 x 2.45 m falling to the back (+Y): eaves z 2.40 at the front "
             "(y -1) to 2.00 at the back (y +1), overhanging 0.3 m at the front; the front (-Y) is open. A bench along the back wall inside "
             "(plank slats, seat top z 0.57, three concrete supports), 3 seats at x -1.1 / 0 / 1.1 facing -Y. Collision: box (collision_boxes, "
             "mesh space; rot_x_deg right-handed about +X, negative tips the +Y end down): floor slab, back wall, the two half walls, "
             "the two front posts, bench, roof; the front stays open")
    return b, {"seats": seats, "notes": notes, "manifest": {"collision_boxes": boxes, "open_front": "-Y"}}


# =========================================================================================================
# drinking_fountain
# =========================================================================================================

def drinking_fountain(variant):
    b = PB("drinking_fountain")
    # plinth (buried to z -0.2), a square tapering column, a cap slab
    box(b, V(0, 0, -0.05), (0.44, 0.44, 0.3), "ash", grain=2)
    P = [V(-0.15, -0.15, 0.1), V(0.15, -0.15, 0.1), V(0.15, 0.15, 0.1), V(-0.15, 0.15, 0.1),
         V(-0.115, -0.115, 0.72), V(0.115, -0.115, 0.72), V(0.115, 0.115, 0.72), V(-0.115, 0.115, 0.72)]
    hexa(b, P, "ash", grain=V(0, 0, 1))
    box(b, V(0, 0, 0.76), (0.4, 0.4, 0.08), "ash", grain=0)
    # the steel bowl: shallow, rim r 0.2 at z 0.88, sitting on the cap
    lathe(b, [(0.07, 0.80), (0.17, 0.84), (0.2, 0.88)], V(0, 0, 0), seg=12, tile="steel")
    lathe(b, [(0.2, 0.88), (0.17, 0.88)], V(0, 0, 0), seg=12, tile="steel")
    lathe_in(b, [(0.17, 0.88), (0.06, 0.83)], V(0, 0, 0), 12, "tin")
    lathe(b, [(0.0, 0.832), (0.045, 0.832)], V(0, 0, 0), seg=8, tile="black_paint")           # the drain
    # the spout: a steel gooseneck from the back rim, tip over the bowl
    sp = [V(0, 0.15, 0.84), V(0, 0.15, 0.92), V(0, 0.13, 0.95), V(0, 0.085, 0.945), V(0, 0.05, 0.925)]
    tube(b, sp, 0.014, seg=5, tile="steel", caps=True)
    # a push button on the front of the column
    box(b, V(0, -0.128, 0.5), (0.05, 0.03, 0.05), "steel")
    notes = ("concrete pedestal drinking fountain 0.95 m high, origin on the ground under the middle: square plinth 0.44 m (buried 0.2 m), a "
             "column tapering 0.30 -> 0.23 m, a cap slab 0.40 m (z 0.72..0.80), a shallow steel bowl (rim r 0.2, z 0.88) with a dark drain and a "
             "steel gooseneck spout on the back rim (+Y) leaning over the bowl; a push button on the front (-Y) face of the column; use from "
             "-Y. manifest `spout` = the spout's tip [x, y, z]. Collision: box")
    return b, {"notes": notes, "manifest": {"spout": [0.0, 0.05, 0.925]}}


KINDS = {
    "park_lamp": (park_lamp, {"variants": [0, 1], "collision": "box"}),
    "park_bench": (park_bench, {"variants": [0, 1], "collision": "exact"}),
    "flower_bed": (flower_bed, {"variants": [0], "collision": "box"}),
    "litter_urn": (litter_urn, {"variants": [0], "collision": "box"}),
    "bike_rack": (bike_rack, {"variants": [0], "collision": "box"}),
    "halt_shelter": (halt_shelter, {"variants": [0], "collision": "box"}),
    "drinking_fountain": (drinking_fountain, {"variants": [0], "collision": "box"}),
}
