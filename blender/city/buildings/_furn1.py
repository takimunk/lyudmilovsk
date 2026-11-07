"""Furniture/props helpers for enterable interiors (low poly, plain Python): rotated boxes, cylinders, chairs,
potted plants, rows of goods, clothes racks, mannequins. Used by saucer_pavilion, department_store, workers_club.
All solids are closed on the sides/top; bottoms are skipped (they stand on a floor)."""
import math

from citykit import box, prism


def _rot(cx, cy, hx, hy, ang):
    c, s = math.cos(ang), math.sin(ang)
    return [(cx + c * x - s * y, cy + s * x + c * y) for x, y in ((-hx, -hy), (hx, -hy), (hx, hy), (-hx, hy))]


def rbox(m, mat, cx, cy, z0, z1, hx, hy, ang=0.0, top=True, bottom=False):
    """Box centred (cx, cy), half sizes hx (local x) hy (local y), local x rotated by ang radians."""
    prism(m, mat, _rot(cx, cy, hx, hy, ang), z0, z1, top=top, bottom=bottom)


def cyl(m, mat, x, y, z0, z1, r, segs=8, r1=None, top=True, bottom=False):
    """Vertical cylinder / cone frustum (r at z0, r1 at z1)."""
    r1 = r if r1 is None else r1
    pts0 = [(x + r * math.cos(2 * math.pi * k / segs), y + r * math.sin(2 * math.pi * k / segs)) for k in range(segs)]
    pts1 = [(x + r1 * math.cos(2 * math.pi * k / segs), y + r1 * math.sin(2 * math.pi * k / segs)) for k in range(segs)]
    for k in range(segs):
        k2 = (k + 1) % segs
        m.face(mat, [(pts0[k][0], pts0[k][1], z0), (pts0[k2][0], pts0[k2][1], z0),
                     (pts1[k2][0], pts1[k2][1], z1), (pts1[k][0], pts1[k][1], z1)])
    if top and r1 > 1e-6:
        m.face(mat, [(px, py, z1) for px, py in pts1])
    if bottom:
        m.face(mat, [(px, py, z0) for px, py in reversed(pts0)])


def blob(m, mat, x, y, z, r, segs=7, sq=1.0):
    """A low-poly ball (bipyramid with two rings), centre z, radius r, vertical squash sq."""
    rings = [(-0.55, 0.78), (0.0, 1.0), (0.55, 0.78)]
    pts = [[(x + r * f * math.cos(2 * math.pi * k / segs), y + r * f * math.sin(2 * math.pi * k / segs), z + r * h * sq)
            for k in range(segs)] for h, f in rings]
    bot, topp = (x, y, z - r * sq), (x, y, z + r * sq)
    for k in range(segs):
        k2 = (k + 1) % segs
        m.face(mat, [bot, pts[0][k], pts[0][k2]][::-1] if False else [bot, pts[0][k2], pts[0][k]])
        for a in range(2):
            m.face(mat, [pts[a][k], pts[a][k2], pts[a + 1][k2], pts[a + 1][k]])
        m.face(mat, [pts[2][k], pts[2][k2], topp])


def chair(m, x, y, z, fx, fy, seat="CT_SteelRed", frame="CT_Steel", s=1.0):
    """A chair at (x, y) whose sitter faces direction (fx, fy)."""
    n = math.hypot(fx, fy) or 1.0
    ang = math.atan2(fy / n, fx / n)         # local +x = facing direction
    c, sn = math.cos(ang), math.sin(ang)

    def at(u, v):
        return x + c * u - sn * v, y + sn * u + c * v
    for u, v in ((-0.19, -0.19), (-0.19, 0.19), (0.19, -0.19), (0.19, 0.19)):
        px, py = at(u * s, v * s)
        rbox(m, frame, px, py, z, z + 0.43 * s, 0.018, 0.018, ang)
    px, py = at(0, 0)
    rbox(m, seat, px, py, z + 0.43 * s, z + 0.47 * s, 0.22 * s, 0.22 * s, ang)
    px, py = at(-0.2 * s, 0)
    rbox(m, seat, px, py, z + 0.47 * s, z + 0.9 * s, 0.02 * s, 0.2 * s, ang)


def stool(m, x, y, z, h=0.7, r=0.19, seat="CT_SteelRed", frame="CT_Steel"):
    cyl(m, frame, x, y, z, z + h, 0.025, 5, top=False)
    cyl(m, frame, x, y, z, z + 0.03, r * 0.8, 8)
    cyl(m, seat, x, y, z + h, z + h + 0.06, r, 8)


def plant(m, x, y, z, h=1.4, pot="CT_PlasterRed", leaf="CT_Grass", r=0.38):
    """Pot (frustum) + trunk + two leafy blobs."""
    cyl(m, pot, x, y, z, z + 0.42, 0.24, 8, r1=0.3)
    cyl(m, "CT_Wood", x, y, z + 0.4, z + h * 0.6, 0.03, 4, top=False)
    blob(m, leaf, x, y, z + h * 0.62, r, 7, 0.9)
    blob(m, leaf, x + 0.12, y - 0.1, z + h * 0.62 + 0.38, r * 0.7, 6, 0.9)


def goods_row(m, rnd, p0, p1, z, depth, hmax, mats, hmin=0.12, wmin=0.14, wmax=0.32, gap=0.02, density=1.0):
    """Boxes (jars/packs/shoes) standing in a row from p0 to p1 (x, y), depth across the row, random heights/colours."""
    L = math.hypot(p1[0] - p0[0], p1[1] - p0[1])
    ang = math.atan2(p1[1] - p0[1], p1[0] - p0[0])
    ux, uy = (p1[0] - p0[0]) / L, (p1[1] - p0[1]) / L
    t = 0.04
    while t < L - wmin:
        w = rnd.uniform(wmin, wmax)
        w = min(w, L - t - 0.02)
        if rnd.random() < density:
            h = rnd.uniform(hmin, hmax)
            cx, cy = p0[0] + ux * (t + w / 2), p0[1] + uy * (t + w / 2)
            rbox(m, rnd.choice(mats), cx, cy, z, z + h, w / 2, depth / 2, ang)
        t += w + gap


def shelf_unit(d, rnd, p0, p1, z, height, depth, levels, mats, board="CT_Wood", hmax=0.34, density=0.92):
    """A shelving unit along p0->p1 (the front looks to the left of the direction): back board, end uprights, boards
    and goods on each. Returns nothing. Everything in `d` (no collision) - make a thin solid separately if needed."""
    L = math.hypot(p1[0] - p0[0], p1[1] - p0[1])
    ang = math.atan2(p1[1] - p0[1], p1[0] - p0[0])
    ux, uy = (p1[0] - p0[0]) / L, (p1[1] - p0[1]) / L
    nx, ny = -uy, ux                                 # front side
    mid = ((p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2)
    rbox(d, "CT_Frame", mid[0] - nx * (depth / 2 - 0.015), mid[1] - ny * (depth / 2 - 0.015), z, z + height,
         L / 2, 0.015, ang)
    for k in range(0, int(L / 1.5) + 2):
        u = min(k * 1.5, L)
        rbox(d, board, p0[0] + ux * u, p0[1] + uy * u, z, z + height, 0.025, depth / 2 - 0.02, ang,
             ) if False else None
    for lv in range(levels):
        zb = z + 0.1 + lv * (height - 0.14) / levels
        rbox(d, board, mid[0], mid[1], zb, zb + 0.035, L / 2, depth / 2, ang)
        goods_row(d, rnd, (p0[0] - nx * (depth / 2 - 0.1), p0[1] - ny * (depth / 2 - 0.1)),
                  (p1[0] - nx * (depth / 2 - 0.1), p1[1] - ny * (depth / 2 - 0.1)), zb + 0.035, depth - 0.2,
                  min(hmax, (height - 0.14) / levels - 0.06), mats, density=density)
    for u in (0.0, L):
        rbox(d, board, p0[0] + ux * u, p0[1] + uy * u, z, z + height, 0.02, depth / 2, ang)


def clothes_rack(d, rnd, x, y, z, length, ang, mats, h=1.55):
    """A garment rail: two feet+posts, a rail, hanging garments (thin boxes)."""
    ux, uy = math.cos(ang), math.sin(ang)
    for s in (-1, 1):
        px, py = x + ux * s * (length / 2 - 0.05), y + uy * s * (length / 2 - 0.05)
        rbox(d, "CT_Steel", px, py, z, z + 0.03, 0.03, 0.3, ang + math.pi / 2)
        rbox(d, "CT_Steel", px, py, z, z + h, 0.015, 0.015, ang)
    rbox(d, "CT_Steel", x, y, z + h, z + h + 0.025, length / 2, 0.015, ang)
    t = -length / 2 + 0.12
    while t < length / 2 - 0.1:
        cx, cy = x + ux * t, y + uy * t
        gh = rnd.uniform(0.7, 1.0)
        rbox(d, rnd.choice(mats), cx, cy, z + h - gh, z + h - 0.02, 0.03, 0.2, ang + rnd.uniform(-0.15, 0.15))
        t += 0.09


def mannequin(d, x, y, z, mat, skin="CT_Plaster", h=1.75, ang=0.0):
    cyl(d, "CT_Steel", x, y, z, z + 0.03, 0.22, 8)
    cyl(d, "CT_Steel", x, y, z, z + 0.9, 0.012, 4, top=False)
    cyl(d, mat, x, y, z + 0.5, z + 1.38, 0.2, 8, r1=0.13)           # dress / torso
    cyl(d, skin, x, y, z + 1.38, z + 1.5, 0.05, 6)
    blob(d, skin, x, y, z + 1.6, 0.1, 6, 1.2)


def till(d, x, y, z, ang=0.0):
    """A cash register on a desk top at height z facing local -y... (screen toward the seller side +local y)."""
    rbox(d, "CT_Steel", x, y, z, z + 0.12, 0.2, 0.17, ang)
    ox, oy = -math.sin(ang) * 0.06, math.cos(ang) * 0.06
    rbox(d, "CT_Frame", x + ox, y + oy, z + 0.12, z + 0.3, 0.1, 0.02, ang)
    rbox(d, "CT_Neon", x + ox * 0.5, y + oy * 0.5, z + 0.13, z + 0.16, 0.12, 0.05, ang) if False else None
