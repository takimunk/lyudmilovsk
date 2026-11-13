"""Camp furniture and kit: table, food pole, clothes line, outhouse."""
import math
import random

from mathutils import Matrix, Vector

from northkit import Builder, V
from props import TWO_PI, catenary, guy, lerp, rx, ry, rz, smooth


def cup(b, c, color="enamel_white", rim="enamel_blue", handle_dir=1.0, r=0.04, h=0.09):
    prof = [(0, 0.0), (r * 0.93, 0.0), (r * 1.03, 0.012), (r * 1.06, h), (r * 0.98, h + 0.002), (r * 0.9, h), (r * 0.9, 0.014), (0, 0.014)]
    b.lathe([(p[0], p[1]) for p in prof], seg=12, center=c, flatc=color)
    ring = [c + V(math.cos(t) * r * 1.02, math.sin(t) * r * 1.02, h + 0.001) for t in [TWO_PI * i / 12 for i in range(13)]]
    b.tube(ring, 0.0028, seg=4, flatc=rim, caps=False)
    hp = [c + V(handle_dir * (r * 1.04 + 0.026 * math.sin(t)), 0, h * 0.5 + 0.03 * math.cos(t)) for t in [math.pi * i / 6 - math.pi / 2 for i in range(7)]]
    b.tube(hp, 0.0045, seg=4, flatc=color, caps=False)


def camp_table(variant):
    b = Builder("camp_table")
    rnd = random.Random(8)
    zt = 0.75
    # top planks
    for i, y in enumerate((-0.3, -0.1, 0.1, 0.3)):
        b.box(V(rnd.uniform(-0.01, 0.01), y, zt - 0.019 + rnd.uniform(-0.001, 0.001)), (1.8 + rnd.uniform(-0.02, 0.0), 0.19, 0.037), "wood",
              rot=rz(rnd.uniform(-0.004, 0.004)), tiles={"x": "endgrain", "top": "wood"})
    for sx in (-1, 1):
        b.box(V(sx * 0.72, 0, zt - 0.06), (0.07, 0.8, 0.05), "wood_dark")
        for sy in (-1, 1):
            b.box(V(sx * 0.72, sy * 0.3, 0.355), (0.07, 0.07, 0.71), "wood_dark")
        b.box(V(sx * 0.72, 0, 0.24), (0.05, 0.6, 0.045), "wood_dark")
    b.box(V(0, 0, 0.27), (1.44, 0.05, 0.05), "wood_dark")
    # benches on trestles
    seats = []
    for sy in (-1, 1):
        y = sy * 0.68
        for i, dy in enumerate((-0.07, 0.07)):
            b.box(V(0.0, y + dy * 1.0, 0.43), (1.8 + rnd.uniform(-0.03, 0.0), 0.125, 0.04), "wood", tiles={"x": "endgrain"})
        for sx in (-1, 1):
            b.box(V(sx * 0.7, y, 0.2), (0.05, 0.22, 0.4), "wood_dark")
            b.box(V(sx * 0.7, y + sy * 0.0, 0.03), (0.07, 0.34, 0.05), "wood_dark")
        for x in (-0.55, 0.0, 0.55):
            seats.append([x, y, 0.45, 180.0 if sy < 0 else 0.0])
    # things on the table: two enamel mugs, a pot with a lid, a tin of condensed milk (no readable label)
    cup(b, V(-0.5, -0.14, zt), handle_dir=1.0)
    cup(b, V(-0.28, 0.2, zt), rim="enamel_blue", handle_dir=-1.0, r=0.042, h=0.095)
    pot = [(0, 0.0), (0.105, 0.0), (0.11, 0.01), (0.112, 0.125), (0.108, 0.128), (0, 0.128)]
    b.lathe(pot, seg=14, center=V(0.28, 0.02, zt), tile="metal")
    b.lathe([(0.114, 0.126), (0.114, 0.132), (0.085, 0.152), (0.02, 0.166), (0.02, 0.172), (0.0, 0.175)], seg=14, center=V(0.28, 0.02, zt), tile="metal")
    b.tube([V(0.28 - 0.112, 0.02, zt + 0.1)] + [V(0.28 + math.cos(t) * 0.118, 0.02 + 0.0, zt + 0.1 + math.sin(t) * 0.0) for t in (math.pi,)]
           + [V(0.28 - 0.14, 0.02, zt + 0.1)], 0.005, seg=4, tile="metal", caps=False)
    b.tube([V(0.28 + 0.112, 0.02, zt + 0.1), V(0.28 + 0.15, 0.02, zt + 0.1)], 0.006, seg=4, tile="metal")
    b.cyl(V(0.62, 0.17, zt), V(0.62, 0.17, zt + 0.1), 0.038, 0.038, seg=10, tile="label", cap_tile="label", smooth=True)
    b.cyl(V(0.62, 0.17, zt + 0.1), V(0.62, 0.17, zt + 0.104), 0.0385, 0.0385, seg=10, flatc="tin", cap0=False)
    b.cyl(V(0.62, 0.17, zt - 0.0), V(0.62, 0.17, zt + 0.004), 0.0385, 0.0385, seg=10, flatc="tin", cap0=False, cap1=False)
    # a spoon and a knife
    b.box(V(0.05, -0.22, zt + 0.006), (0.17, 0.018, 0.006), flatc="steel", rot=rz(0.4))
    b.box(V(-0.05, 0.28, zt + 0.006), (0.2, 0.014, 0.005), flatc="steel", rot=rz(-0.2))
    return b, {"seats": seats, "notes": "table 1.8 x 0.8, top z 0.75, long along X; two benches (z 0.45), 6 seats; same position/yaw as tarp_shelter"}


def food_pole(variant):
    b = Builder("food_pole")
    r = random.Random(4)
    b.cyl(V(0, 0, -0.1), V(0, 0, 5.0), 0.075, 0.045, seg=8, tile="bark", cap_tile="endgrain", wobble=0.03, cap0=False)
    bar_z = 4.78
    b.cyl(V(-0.85, 0, bar_z), V(1.0, 0, bar_z), 0.035, 0.03, seg=7, tile="bark", cap_tile="endgrain", wobble=0.03)
    b.cyl(V(0, 0, bar_z - 0.09), V(0, 0, bar_z + 0.09), 0.062, 0.062, seg=10, tile="rope", caps=False)
    # bag hoisted at the far end of the bar: rope over the bar, down to the bag, the other end tied to the pole
    bx = 0.85
    b.tube([V(bx, 0, 4.0), V(bx, 0, bar_z + 0.02)], 0.006, seg=4, tile="rope", caps=False)
    over = [V(bx + 0.03 * math.sin(t), 0, bar_z + 0.02 + 0.02 * math.cos(t)) for t in [math.pi * i / 6 for i in range(1, 6)]]
    b.tube([V(bx, 0, bar_z + 0.045)] + over + [V(bx + 0.035, 0, bar_z), V(0.08, -0.03, 1.55)], 0.006, seg=4, tile="rope", caps=False)
    bag = [(0.0, 0.0), (0.085, 0.005), (0.135, 0.12), (0.145, 0.26), (0.135, 0.38), (0.105, 0.43), (0.06, 0.45), (0.07, 0.49), (0.075, 0.56), (0.0, 0.57)]
    b.lathe(bag, seg=12, center=V(bx, 0, 3.45), flatc="bag_orange")
    b.cyl(V(bx, 0, 4.03), V(bx, 0, 4.0), 0.014, 0.014, seg=6, flatc="carabiner")
    b.tube([V(bx - 0.02, 0, 4.0), V(bx - 0.02, 0, 4.06), V(bx + 0.02, 0, 4.06), V(bx + 0.02, 0, 4.0)], 0.005, seg=4, flatc="carabiner", caps=False)
    b.box(V(0.08, -0.03, 1.5), (0.04, 0.03, 0.1), "wood_dark")           # the cleat the rope ties to
    # guys and a ring of stones round the foot
    for a in (60, 180, 300):
        a = math.radians(a)
        guy(b, V(math.cos(a) * 0.05, math.sin(a) * 0.05, 4.3), V(math.cos(a) * 3.0, math.sin(a) * 3.0, 0.0), sag=0.0)
    for i in range(8):
        a = TWO_PI * i / 8 + 0.3
        sz = r.uniform(0.17, 0.26)
        b.stone(V(math.cos(a) * 0.28, math.sin(a) * 0.28, sz * 0.15), (sz * 1.2, sz, sz * 0.8), tile="stone", seed=i, flatten=1.0)
    return b, {"notes": "5 m pole, crossbar at z 4.78, orange food bag hoisted at x 0.85 (z 3.45..4.0), three guys; collision: the pole only"}


def _drape(b, x0, width, drop_f, drop_b, zl, tile, folds=0.015, thick=0.008, nx=7, y0=0.0):
    """A cloth hung over the line (at height zl): front hangs drop_f, back drop_b."""
    path = []
    n1 = 8
    for i in range(n1 + 1):
        path.append((-0.024, zl - drop_f + drop_f * i / n1 * 0.995 + 0.0))
    for k in range(1, 5):
        t = math.pi * k / 5
        path.append((-0.024 * math.cos(t), zl + 0.024 * math.sin(t)))
    for i in range(n1 + 1):
        path.append((0.024, zl - drop_b * i / n1 * 0.995 - 0.0 * 1))
    path = [p for p in path]
    us = [i / nx for i in range(nx + 1)]
    vs = list(range(len(path)))

    def fn(u, v):
        y, z = path[int(v)]
        x = x0 - width / 2 + u * width
        hang = max(0.0, (zl - z) / max(drop_f, drop_b))
        side = -1 if y < 0 else 1
        y2 = y + side * folds * math.sin(u * 13.0 + z * 4.0) * hang + side * 0.03 * hang * hang
        return V(x + folds * 0.4 * math.sin(z * 9.0 + u * 5.0) * hang, y0 + y2, z)
    b.grid(fn, us, vs, tile=tile, thick=thick, inner_tile=tile)


def clothes_line(variant):
    b = Builder("clothes_line")
    zt = 1.85
    px = 2.5
    for sx in (-1, 1):
        b.cyl(V(sx * px, 0, -0.1), V(sx * (px + 0.06), 0, zt), 0.04, 0.032, seg=7, tile="bark", cap_tile="endgrain", wobble=0.03, cap0=False)
        guy(b, V(sx * (px + 0.06), 0, zt - 0.05), V(sx * (px + 1.05), 0.2, 0.0))
    top = [V(-px - 0.06, 0, zt - 0.06), V(px + 0.06, 0, zt - 0.06)]
    sag = 0.2
    pts = catenary(top[0], top[1], sag, 22)
    b.tube(pts, 0.004, seg=4, tile="rope", caps=False)

    def zl(x):
        t = (x + px + 0.06) / (2 * px + 0.12)
        return zt - 0.06 - sag * 4 * t * (1 - t) + 0.0
    # two grey wool socks, a striped towel, a plaid shirt, pegs
    for x in (-1.7, -1.5):
        z0 = zl(x) - 0.03
        pth = [V(x, 0, z0), V(x, 0, z0 - 0.12), V(x + 0.005, -0.005, z0 - 0.24), V(x + 0.02, -0.015, z0 - 0.32)]
        b.tube(pth, 0.04, seg=7, tile="knit", rscale=lambda i, n: (0.9, 1.0, 0.95, 0.5)[i], aspect=0.55)
        b.box(V(x, 0.0, z0 - 0.0), (0.03, 0.045, 0.06), "wood_dark")
    _drape(b, 0.1, 0.55, 0.78, 0.62, zl(0.1) - 0.0, "towel")
    b.box(V(-0.12, 0.0, zl(-0.12) + 0.0), (0.012, 0.05, 0.075), "wood_dark")
    b.box(V(0.32, 0.0, zl(0.32) + 0.0), (0.012, 0.05, 0.075), "wood_dark")
    xs = 1.3
    _drape(b, xs, 0.5, 0.68, 0.64, zl(xs), "plaid", folds=0.02, thick=0.01)
    for sx in (-1, 1):
        x = xs + sx * 0.3
        p = [V(x, 0, zl(x) - 0.04), V(x + sx * 0.03, 0.0, zl(x) - 0.22), V(x + sx * 0.05, 0.0, zl(x) - 0.42), V(x + sx * 0.06, 0.0, zl(x) - 0.56)]
        b.tube(p, 0.05, seg=7, tile="plaid", rscale=lambda i, n: (1.0, 0.95, 0.85, 0.7)[i], aspect=0.5)
    for dx in (-0.22, 0.22):
        b.box(V(xs + dx, 0.0, zl(xs + dx)), (0.012, 0.05, 0.075), "wood_dark")
    return b, {"notes": "5 m rope between two poles, three items; no collision except (optionally) the poles"}


def outhouse(variant):
    b = Builder("outhouse")
    rnd = random.Random(21)
    W, D = 1.1, 1.1
    zf, zr = 2.12, 1.94                      # wall tops front (-Y) and rear
    th = 0.022

    def h(y):
        return lerp(zf, zr, (y + D / 2) / D)
    # corner posts
    for sx in (-1, 1):
        for sy in (-1, 1):
            hh = h(sy * D / 2)
            b.box(V(sx * (W / 2 - 0.035), sy * (D / 2 - 0.035), hh / 2 - 0.02), (0.07, 0.07, hh + 0.04), "wood_dark", grain=2)
    # side and back boards (vertical, small gaps)
    nb = 8
    bw = (W - 0.14) / nb
    for sx in (-1, 1):
        for i in range(nb):
            y = -D / 2 + 0.07 + bw * (i + 0.5)
            hh = h(y)
            b.box(V(sx * (W / 2 - th / 2), y, hh / 2 + 0.01), (th + rnd.uniform(-0.002, 0.003), bw - 0.006, hh - 0.0), "wood", grain=2)
    for i in range(nb):
        x = -W / 2 + 0.07 + bw * (i + 0.5)
        hh = zr
        b.box(V(x, D / 2 - th / 2, hh / 2 + 0.01), (bw - 0.006, th + rnd.uniform(-0.002, 0.003), hh), "wood", grain=2)
    # front: two narrow boards each side of the door and a header
    for sx in (-1, 1):
        for k, wb in enumerate((0.115, 0.115)):
            x = sx * (0.36 + 0.07 + k * 0.125 - 0.0 + 0.0)
            if abs(x) < W / 2 - 0.03:
                b.box(V(x, -D / 2 + th / 2, zf / 2 + 0.01), (wb - 0.006, th, zf), "wood", grain=2)
    for i in range(6):
        x = -0.36 + 0.72 * (i + 0.5) / 6
        b.box(V(x, -D / 2 + th / 2, (1.88 + zf) / 2), (0.72 / 6 - 0.006, th, zf - 1.88), "wood", grain=2)
    # roof: boards sloping down to the back, a drip board, a tin ridge strip
    ang = math.atan2(zf - zr, D)
    for i in range(8):
        x = -0.72 + 1.44 * (i + 0.5) / 8
        b.box(V(x, 0.0 - 0.02, (zf + zr) / 2 + 0.065), (1.44 / 8 - 0.004, 1.42, 0.028), "wood_dark", rot=rx(-ang), grain=1)
    b.box(V(0, -0.74, zf + 0.025 - 0.0), (1.46, 0.03, 0.06), "wood_dark")
    # floor and a stone doorstep
    b.box(V(0, 0, 0.012), (1.0, 1.0, 0.024), "wood_dark")
    b.stone(V(0.0, -0.9, 0.04), (0.7, 0.45, 0.16), tile="stone", seed=2, flatten=1.0)
    # inside: seat box with a hole, lid leaning on the wall, a roll of paper on a nail
    sy0 = 0.26
    zs = 0.46
    for (cx, cy, sx_, sy_) in ((-0.34, sy0, 0.22, 0.46), (0.34, sy0, 0.22, 0.46), (0.0, sy0 + 0.17, 0.46, 0.12), (0.0, sy0 - 0.17, 0.46, 0.12)):
        b.box(V(cx, cy, zs - 0.02), (sx_, sy_, 0.04), "wood")
    b.box(V(0, sy0, zs / 2 - 0.02), (0.9, 0.46, zs - 0.04), "wood_dark")
    b.box(V(0, sy0, zs - 0.03), (0.22, 0.22, 0.003), flatc="dark")
    b.box(V(0.18, D / 2 - 0.05, 1.25), (0.42, 0.03, 0.46), "wood", rot=rx(-0.12))
    b.cyl(V(-0.35, D / 2 - 0.06, 1.2), V(-0.35, D / 2 - 0.13, 1.2), 0.055, 0.055, seg=8, flatc="white_paint")
    # the door: a plate with a heart cut in it, ajar, hinged at the left
    dw, dh = 0.72, 1.86
    heart = []
    for i in range(22):
        t = TWO_PI * i / 22
        hx = 16 * math.sin(t) ** 3
        hy = 13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)
        heart.append((hx * 0.0105, 1.42 + hy * 0.0105 - 0.1))
    heart = heart[::-1]
    outer = [(-dw / 2, 0.0), (dw / 2, 0.0), (dw / 2, dh), (-dw / 2, dh)]
    # an extra outline vertex pair keeps the fill well shaped
    outer = [(-dw / 2, 0.0), (0.0, 0.0), (dw / 2, 0.0), (dw / 2, dh * 0.5), (dw / 2, dh), (0.0, dh), (-dw / 2, dh), (-dw / 2, dh * 0.5)]
    ang = math.radians(-32)            # opens outward (toward -Y)
    # build the door in its own frame and rotate about the hinge (left edge)
    from northkit import Builder as _B
    tmp = _B("door")
    tmp.slab_hole(outer, heart, V(dw / 2, 0, 0), (V(1, 0, 0), V(0, 0, 1), V(0, 1, 0)), 0.03, tile="wood", swap=True)
    for gx in (-0.27, -0.09, 0.09, 0.27):
        for (z0, z1) in ((0.06, 1.0), (1.72, 1.82)):
            tmp.box(V(dw / 2 + gx, -0.016, (z0 + z1) / 2), (0.007, 0.004, z1 - z0), flatc="black_paint")
    for z in (0.35, 1.55):                           # two cross battens on the inside
        tmp.box(V(dw / 2, 0.028, z), (dw - 0.06, 0.026, 0.1), "wood_dark")
    tmp.box(V(dw / 2 + 0.28, -0.026, 0.95), (0.05, 0.02, 0.012), flatc="steel")                    # latch
    # merge the door's faces into the main builder, rotated about the hinge
    R = rz(ang)
    pivot = V(-0.36, -D / 2 - 0.012, 0)
    for f in tmp.bm.faces:
        pts = [pivot + R @ v.co for v in f.verts]
        uvs = [lp[tmp.uv].uv.copy() for lp in f.loops]
        b.face(pts, uvs, mat=0)
    # hinges
    for z in (0.3, 1.5):
        b.box(pivot + V(0.0, -0.0, z), (0.05, 0.05, 0.1), flatc="black_paint")
    return b, {"notes": "plank outhouse 1.1 x 1.1, shed roof sloping to the back, heart cut in the door, door ajar 32 degrees; exact collision (walls); the door stands open"}
