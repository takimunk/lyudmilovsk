"""Tents and the tarp: cloth with sag, taut guy lines, pegs. See props.py for conventions."""
import math
import random

from mathutils import Matrix, Vector

from northkit import Builder, V
from props import TWO_PI, catenary, guy, lerp, rx, ry, rz, smooth


def sink(z):
    """Fabric hems sink 2 cm into the ground so no gap shows on uneven terrain."""
    return z - 0.02 * (1.0 - smooth(z / 0.12))


# =========================================================================================================
# tent_dome
# =========================================================================================================

DOME_TILES = ("nylon_orange", "nylon_bluegrey", "nylon_olive")


def tent_dome(variant):
    b = Builder("tent_dome_v%d" % variant)
    a, H = 1.05, 1.3
    tile = DOME_TILES[variant]
    n = 20
    xs = [-a + 2 * a * i / n for i in range(n + 1)]
    ys = list(xs)
    DX, DY0 = 0.42, -0.315          # the door: |x| < DX and y < DY0 (grid lines coincide)
    opened = variant == 0

    def g(t):
        return max(0.0, 1.0 - abs(t / a) ** 3.0) ** 0.5

    def surf(x, y):
        z = H * g(x) * g(y)
        w = min(abs(x), abs(y)) / max(abs(x), abs(y), 1e-6)       # 1 on the pole diagonals, 0 mid-panel
        z *= 1.0 - 0.075 * (1.0 - w) * (1.0 - 0.3 * g(x) * g(y))
        z += 0.007 * math.sin(11.0 * x + 3.0 * y) * math.sin(9.0 * y - x) * (1.0 - w * 0.5)
        # panels pull in slightly toward the diagonals' poles
        return V(x * (1 - 0.0), y, sink(z))

    def in_door(i, j):
        return xs[i] >= -DX - 1e-6 and xs[i + 1] <= DX + 1e-6 and ys[j + 1] <= DY0 + 1e-6

    # grid(): u over x, v over y
    P = b.grid(lambda x, y: surf(x, y), xs, ys, tile=tile, thick=0.004, inner_flat="dark" if opened else None,
               inner_tile=None if opened else tile, skip=in_door if opened else None)
    # pole sleeves along both diagonals, over the fabric
    for sgn in (1, -1):
        pts = []
        for i in range(0, 21):
            t = -a * 0.97 + 2 * a * 0.97 * i / 20
            p = surf(t, sgn * t)
            pts.append(p + V(0, 0, 0.011))
        b.tube(pts, 0.012, seg=5, tile="nylon_olive", flatc="hem", caps=False)
    # door: zip arch (closed tents) or the flap swung open (v0)
    door_edge = [(-DX, -a), (-DX, DY0), (DX, DY0), (DX, -a)]
    if opened:
        us = [-DX + 2 * DX * i / 8 for i in range(9)]
        vs = [-a + (DY0 + a) * j / 7 for j in range(8)]
        hinge0 = surf(DX, -a)
        hinge1 = surf(DX, DY0)
        d = (hinge1 - hinge0).normalized()
        phi = math.radians(58)
        R = Matrix.Rotation(phi, 3, d)

        def flap(u, v):
            p = surf(u, v) + V(0, -0.004, 0.0)
            q = hinge0 + R @ (p - hinge0)
            # the flap bulges and sags a little as it is held open
            q.z -= 0.05 * math.sin((DX - u) / (2 * DX) * math.pi)
            q += V(0, -0.03 * math.sin((v + a) / (DY0 + a) * math.pi), 0)
            return q
        b.grid(flap, us, vs, tile=tile, thick=0.004, inner_flat="dark")
        # a stuff sack in the corner, the sleeping mat and the groundsheet
        b.box(V(0.0, 0.0, 0.006), (1.9, 1.9, 0.012), flatc="ground_dark")
        b.box(V(-0.38, 0.05, 0.03), (0.58, 1.75, 0.045), "mat")
        b.cyl(V(0.35, 0.6, 0.12), V(0.35, 0.05, 0.12), 0.12, 0.12, seg=8, tile="nylon_olive", flatc="bag_green")
    else:
        pts = []
        for k in range(0, 17):
            t = k / 16
            if t < 0.3:
                x, y = -DX, lerp(-a, DY0, t / 0.3)
            elif t < 0.7:
                x, y = lerp(-DX, DX, (t - 0.3) / 0.4), DY0
            else:
                x, y = DX, lerp(DY0, -a, (t - 0.7) / 0.3)
            pts.append(surf(x, y) + V(0, -0.006, 0.007))
        b.tube(pts, 0.006, seg=4, flatc="black_paint", caps=False)
        b.box(pts[8] + V(0, -0.008, -0.02), (0.035, 0.012, 0.05), flatc="black_paint")
    # guy lines from the pole sleeves' tape loops to pegs, pegs at the corners
    for sx in (-1, 1):
        for sy in (-1, 1):
            t = 0.72 * a
            p0 = surf(sx * t, sy * t)
            guy(b, p0, V(sx * 1.95, sy * 1.95, 0.0))
            b.box(p0 + V(0, 0, 0.0), (0.03, 0.03, 0.012), flatc="hem")
    # a small vent cap and corner webbing
    b.cyl(V(0, 0, H - 0.01), V(0, 0, H + 0.025), 0.07, 0.06, seg=8, tile=tile, cap_tile=tile)
    return b, {"notes": "two-pole dome, poles on the diagonals; v0 has the door flap swung open (inside dark, sleeping mat); front is -Y"}


# =========================================================================================================
# tent_ridge
# =========================================================================================================

RIDGE_TILES = ("canvas_khaki", "canvas_green")


def tent_ridge(variant):
    b = Builder("tent_ridge_v%d" % variant)
    w, L, H = 1.0, 2.8, 1.5
    tile = RIDGE_TILES[variant]
    ny = 28
    ys = [-L / 2 + L * i / ny for i in range(ny + 1)]
    ts = [i / 8 for i in range(9)]
    peg_sp = 0.7

    def slope(sx):
        def fn(y, t):
            sag = 0.04 * (1.0 - (2 * y / L) ** 2) * (1 - t) * 1.0
            lift = 0.055 * (math.sin(math.pi * (y + L / 2) / peg_sp) ** 2) * t ** 3
            wr = 0.010 * math.sin(8.0 * y + 2.0 * t) * math.sin(5.0 * t + y) * (1 - 0.5 * t)
            x = sx * (w * t + wr * 0.5)
            z = H * (1 - t) - sag + lift + wr
            return V(x, y, sink(max(z, 0.0)) if t > 0.85 else z)
        return fn
    for sx in (-1, 1):
        # the slope's u runs along y, v from the ridge down to the hem
        b.grid(slope(sx), ys, ts, tile=tile, thick=0.004, inner_tile=tile)
    # gables: back closed, front with two flaps tied back
    for end, door in ((1, False), (-1, True)):
        yE = end * L / 2
        for side in (-1, 1):
            xin = 0.0 if not door else 0.19

            def gab(u, v, side=side, xin=xin, end=end, door=door):
                xi = xin * (1 - v) if door else 0.0
                x = side * (xi + (w - xi) * u)
                zt = H * (1 - abs(x) / w)
                z = v * zt
                y = yE + end * 0.0
                if door:
                    y -= 0.07 * math.sin(math.pi * u) ** 1.5 * (1 - u) * 1.2 * (1.0 if end < 0 else 0)
                    y += (-0.012 * math.sin(14 * u + 5 * v)) if end < 0 else 0
                    z += 0.0
                return V(x, y, sink(z) if v < 0.1 else z)
            us = [i / 8 for i in range(9)]
            vs = [j / 8 for j in range(9)]
            b.grid(gab, us, vs, tile=tile, thick=0.004, inner_flat="dark" if door else None, inner_tile=None if door else tile)
    # ground sheet inside
    b.box(V(0, 0, 0.006), (1.9, 2.7, 0.012), flatc="ground_dark")
    b.box(V(-0.35, 0.1, 0.03), (0.6, 1.9, 0.045), "mat")
    # A-frame poles at both gables and the ridge pole between them
    for end in (-1, 1):
        y = end * (L / 2 + 0.07)
        for sx in (-1, 1):
            b.cyl(V(sx * 0.66, y, 0.0), V(-sx * 0.04, y, H + 0.1), 0.029, 0.024, seg=7, tile="wood", cap_tile="endgrain", wobble=0.04)
        b.cyl(V(0, y, H + 0.03), V(0, y, H + 0.1), 0.045, 0.045, seg=8, tile="rope", caps=False)
    b.cyl(V(0, -L / 2 - 0.4, H + 0.075), V(0, L / 2 + 0.4, H + 0.075), 0.026, 0.026, seg=7, tile="wood", cap_tile="endgrain", wobble=0.03)
    # guy ropes and pegs
    for end in (-1, 1):
        guy(b, V(0, end * (L / 2 + 0.07), H + 0.09), V(0, end * (L / 2 + 1.5), 0.0))
    for sx in (-1, 1):
        for k in range(5):
            y = -L / 2 + k * peg_sp
            p = V(sx * (w + 0.0), y, 0.01)
            b.peg(V(sx * (w + 0.13), y, 0.1), direction=(sx * 0.4, 0, -1), length=0.22)
            b.tube([p, V(sx * (w + 0.12), y, 0.07)], 0.003, seg=4, tile="rope", caps=False)
        for y in (-0.5, 0.5):
            p = V(sx * w * 0.55, y, H * 0.45 - 0.03)
            guy(b, p, V(sx * (w + 1.0), y * 1.5, 0.0))
    return b, {"notes": "old canvas A-frame ridge tent on wooden poles; front gable (-Y) has the flaps tied back, the back is closed"}


# =========================================================================================================
# tent_tunnel
# =========================================================================================================

def tent_tunnel(variant):
    b = Builder("tent_tunnel_v0")
    W, H, L = 0.8, 1.2, 4.0
    tile = "nylon_yellow"
    hoops = (-1.1, 0.0, 1.1)
    ny = 44
    ys = [-L / 2 + L * i / ny for i in range(ny + 1)]
    nt = 20
    ths = [math.pi * i / nt for i in range(nt + 1)]

    def kscale(y):
        if y < -1.5:
            q = (y + 1.5) / 0.5
            return math.sqrt(max(0.0, 1.0 - q * q))
        if y > 1.45:
            q = (y - 1.45) / 0.55
            return math.sqrt(max(0.0, 1.0 - q * q)) * 0.85 + 0.15 * (1 - q)
        return 1.0

    def sagf(y):
        # fabric pulls in between the hoops
        if y < hoops[0] or y > hoops[-1]:
            return 0.0
        seg = 1.1
        t = ((y - hoops[0]) % seg) / seg
        return math.sin(math.pi * t) ** 2

    def prof(th, y):
        k = kscale(y)
        c = 1.0 - 0.05 * sagf(y)
        x = W * math.cos(th) * k * c
        z = H * (math.sin(th) ** 0.82) * k * c
        z += 0.006 * math.sin(7 * y + 3 * th) * math.sin(th)
        return V(x, y, sink(z) if z < 0.12 else z)

    # the tip rings at the ends collapse to a point: drop the last row to keep the cells valid
    ysx = [y for y in ys if kscale(y) > 0.03]
    b.grid(lambda th, y: prof(th, y), ths, ysx, tile=tile, thick=0.004, inner_tile=tile)
    # hoop sleeves
    for yh in hoops:
        pts = [prof(th, yh) + V(0, 0, 0) for th in [math.pi * i / 24 for i in range(1, 24)]]
        pts = [p + (V(p.x, 0, p.z - 0.0).normalized() * 0.011 if p.length > 0 else V()) for p in pts]
        b.tube(pts, 0.012, seg=5, tile="nylon_yellow", flatc="hem", caps=False)
    # door zip: a D on the front cap
    yz = -1.72
    pts = [prof(th, yz) + V(0, -0.006, 0) for th in [0.35 + (math.pi - 0.7) * i / 20 for i in range(21)]]
    b.tube(pts, 0.006, seg=4, flatc="black_paint", caps=False)
    pts = [V(0.0, -1.74, 0.05 + 0.002 * i) for i in range(2)]
    # groundsheet
    b.box(V(0, 0, 0.006), (1.5, 3.7, 0.012), flatc="ground_dark")
    # guys: ends and the three hoops
    top_front = prof(math.pi / 2, -1.7)
    guy(b, top_front + V(0, -0.01, 0.0), V(0, -3.05, 0.0))
    guy(b, prof(math.pi / 2, 1.7) + V(0, 0.01, 0), V(0, 3.0, 0.0))
    for yh in hoops:
        for sx in (-1, 1):
            p0 = prof(math.pi / 2 - sx * 0.95, yh)
            guy(b, p0, V(sx * (W + 1.15), yh + 0.25 * (1 if yh >= 0 else -1), 0.0))
    for sx in (-1, 1):
        for yy in (-1.6, -0.55, 0.55, 1.6):
            b.peg(V(sx * (W + 0.1), yy, 0.1), direction=(sx * 0.4, 0, -1), length=0.2)
    return b, {"notes": "three-hoop tunnel tent, yellow fly, front (-Y) has the D door (zipped), rear end tapers lower"}


# =========================================================================================================
# tarp_shelter
# =========================================================================================================

def tarp_shelter(variant):
    b = Builder("tarp_shelter")
    hx, hy = 2.0, 1.5                  # tarp 4 x 3
    zf, zb = 1.95, 2.55                # corner heights: front (-Y) lower, back higher: rain runs off the back
    corners = {(-1, -1): zf, (1, -1): zf, (-1, 1): zb, (1, 1): zb}
    nx, ny = 16, 12
    us = [-1 + 2 * i / nx for i in range(nx + 1)]
    vs = [-1 + 2 * j / ny for j in range(ny + 1)]
    rnd = random.Random(3)

    def fn(u, v):
        x, y = u * hx, v * hy
        zlin = lerp(lerp(zf, zb, (v + 1) / 2), lerp(zf, zb, (v + 1) / 2), 0.5)
        # a tarp hung on four corners: edges sag (catenary), the middle hangs lower than the corners' plane
        sag = 0.20 * (1 - u * u) * (0.5 + 0.5 * (1 - v * v)) + 0.17 * (1 - v * v) * (0.5 + 0.5 * (1 - u * u))
        # radial pleats at the corners where the pull concentrates
        d = [math.hypot(u - cu, v - cv) for cu in (-1, 1) for cv in (-1, 1)]
        dm = min(d)
        pleat = 0.030 * math.sin(math.atan2(v - (1 if v > 0 else -1), u - (1 if u > 0 else -1)) * 9.0) * math.exp(-dm * 3.0)
        wave = 0.022 * math.sin(5 * u + 2 * v) * math.sin(4 * v - u) + 0.012 * math.sin(11 * u - 3 * v)
        return V(x, y, zlin - sag + pleat + wave)
    b.grid(fn, us, vs, tile="tarp_blue", thick=0.003, inner_tile="tarp_blue")
    # four poles with the guy lines and the pegs
    for (sx, sy), z in corners.items():
        x, y = sx * hx, sy * hy
        b.cyl(V(x, y, -0.05), V(x, y, z + 0.1), 0.04, 0.032, seg=8, tile="bark", cap_tile="endgrain", wobble=0.04)
        eye = V(x, y, z - 0.01)
        b.cyl(eye + V(0, 0, 0.03), eye + V(0, 0, 0.045), 0.05, 0.05, seg=8, tile="rope", caps=False)
        out = V(sx, sy, 0).normalized()
        guy(b, V(x, y, z + 0.06), V(x + sx * 1.35, y + sy * 1.0, 0.0), sag=0.0)
    # a rope along the front edge's middle for a lantern hook
    b.tube([V(0, -hy, zf - 0.17), V(0, -hy, zf - 0.30)], 0.003, seg=4, tile="rope", caps=False)
    return b, {"notes": "4 x 3 m tarp on four poles, front (-Y) lower; it covers camp_table at the same position and yaw"}
