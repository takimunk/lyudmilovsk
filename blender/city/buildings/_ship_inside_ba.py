"""Motor ship «Полдень»: furniture, things and people-spots of the PROMENADE DECK B, the BOAT DECK A and the BRIDGE.

    from _ship_inside_ba import furnish_ba
    furnish_ba(B, S)        # B = the citykit Building of motor_ship.py, S = the ship-frame helper (_shipframe.py)

Everything here is laid out in the SHIP frame of R1 (x from the stern 0..62 to the bow, y port negative / starboard
positive, z from the waterline) and goes through S into the building frame. Headings: FWD 0, STBD 90, AFT 180, PORT 270
(the way a piece / a person / a thing FACES); `_K.Y` turns a heading into the kit's yaw whatever the helper does.
The shell (walls, decks, doors, windows, stairs, rails, funnel, boats, masts) is motor_ship.py's; nothing here cuts it.
Local pieces (double bass, drum kit, ship's wheel, telegraph ...) are written in the interior kit's style (kit._P: front
= -y, origin on the floor / on the wall) and live here, the kit itself is not edited.
"""
import math

import interior_kit as kit
from interior_kit import _P, _ax, _torus, _line, _post, _soft, _pad, _rrect, _ring_rect, _ring_circle

WOOD, WOODL, STEEL, ALU, BRASS, LAQ = kit.WOOD, kit.WOODL, kit.STEEL, kit.ALU, kit.BRASS, kit.LAQ
WHITE, PAPER, RED, GLASS, OPAL, RUBBER, BAIZE = ("CT_SteelWhite", "CT_Paper", "CT_SteelRed", "CT_Glass", "CT_Opal",
                                                 "CT_Rubber", "CT_Baize")
FWD, STBD, AFT, PORT = 0.0, 90.0, 180.0, 270.0

# deck levels of R1 5.1 (the helper's own numbers win when it has them)
ZB, ZA, ZBR = 3.80, 6.50, 9.20


# ================================================================================================ small helpers
def _disc(p, mat, cx, zc, r, y, segs=12, a0=0.0):
    """A disc in the plane y facing -y (the piece's front)."""
    p.face(mat, [(cx + r * math.cos(a0 + 2 * math.pi * k / segs), y, zc + r * math.sin(a0 + 2 * math.pi * k / segs))
                 for k in range(segs)])


def _sector(p, mat, cx, zc, r, y, a0, a1, n=3, r0=0.0):
    """A sector (clock angles in degrees: 0 = up, clockwise seen from the front) of a dial in the plane y facing -y."""
    def pt(rr, a):
        t = math.radians(90.0 - a)
        return (cx + rr * math.cos(t), y, zc + rr * math.sin(t))
    for k in range(n):
        b0, b1 = a0 + (a1 - a0) * k / n, a0 + (a1 - a0) * (k + 1) / n
        if r0 <= 0:
            p.face(mat, [pt(0, 0), pt(r, b1), pt(r, b0)])
        else:
            p.face(mat, [pt(r0, b0), pt(r0, b1), pt(r, b1), pt(r, b0)])


def _hand(p, mat, cx, zc, r, y, ang, w=0.012):
    """A dial hand (clock angle in degrees) lying on the plane y."""
    t = math.radians(90.0 - ang)
    p.rod(mat, (cx, y, zc), (cx + r * math.cos(t), y, zc + r * math.sin(t)), w, 3)


def _sphere(p, mat, x, y, z, r, segs=8):
    p.lathe(mat, x, y, [(r * 0.5, z - r * 0.87), (r * 0.87, z - r * 0.5), (r, z), (r * 0.87, z + r * 0.5),
                        (r * 0.5, z + r * 0.87)], segs, top=True, bottom=True)


# ================================================================================================ saloon pieces
def back_bar(B, x, y, z=0.0, yaw=0.0, w=2.8, d=0.36, seed=5, solid=True):
    """[wall] Mirror back-bar w x0.36x2.1: a veneered cabinet with a brass-edged top, a mirror in a frame behind two glass
    shelves of bottles. 'top' = the cabinet top (the coffee machine stands on it)."""
    p = _P(B, x, y, z, yaw, "back_bar")
    p.kind = "wall"
    rnd = kit._rng(seed, x, y, "backbar")
    hw = w / 2
    p.box(WOOD, -hw, hw, -d, 0.0, 0.08, 0.9, f="xXyZ")
    p.box(LAQ, -hw + 0.03, hw - 0.03, -d + 0.03, 0.0, 0.0, 0.08, f="xXy")
    p.box(BRASS, -hw - 0.01, hw + 0.01, -d - 0.015, -d + 0.02, 0.9, 0.915, f="xXyYZz")
    n = max(2, int(round(w / 0.7)))
    for k in range(n):                                   # door panels
        a = -hw + 0.04 + k * (w - 0.08) / n
        b = a + (w - 0.08) / n - 0.03
        p.box(WOODL, a, b, -d - 0.006, -d, 0.16, 0.82, f="xXyZz")
        p.box(BRASS, b - 0.05, b - 0.03, -d - 0.02, -d - 0.006, 0.47, 0.53, f="xXyZz")
    # mirror in a frame, two glass shelves on brass pegs
    p.box(WOOD, -hw, hw, -0.03, 0.0, 0.915, 2.1, f="xXyZ")
    p.box("CT_Aluminium", -hw + 0.07, hw - 0.07, -0.034, -0.03, 1.0, 2.02, f="y")
    for zb in (1.3, 1.68):
        p.box(GLASS, -hw + 0.06, hw - 0.06, -0.24, -0.035, zb - 0.012, zb, f="xXyZz")
        for sx in (-hw + 0.12, 0.0, hw - 0.12):
            p.rod(BRASS, (sx, -0.035, zb - 0.02), (sx, -0.22, zb - 0.02), 0.012, 3)
        kit._goods(p, "bottles", -hw + 0.08, hw - 0.08, -0.22, -0.05, zb, zb + 0.33, rnd, cull=True)
    kit._goods(p, "bottles", -hw + 0.9, hw - 0.1, -0.2, -0.05, 0.915, 1.25, rnd, cull=True)
    p.box(BRASS, -hw, hw, -0.05, 0.0, 2.1, 2.13, f="xXyZz")
    p.core(-hw + 0.02, hw - 0.02, -d + 0.02, -0.02, 0.1, 0.88)
    return p.done(solid=solid, top=0.915, on=(-hw + 0.4, -d / 2 - 0.02, 0.915), front=0.5)


def coffee_machine(B, x, y, z=0.0, yaw=0.0, solid=False):
    """[prop] Lever espresso machine 0.5x0.32x0.42: a chrome body, a brass boiler dome, two group heads with levers,
    a drip tray, a stack of cups on the top."""
    p = _P(B, x, y, z, yaw, "coffee_machine")
    p.kind = "prop"
    p.bev(ALU, -0.25, 0.25, -0.12, 0.16, 0.0, 0.3, 0.02, 0.02)
    p.box(STEEL, -0.22, 0.22, -0.2, -0.12, 0.0, 0.03, f="xXyZ")
    p.lathe(BRASS, 0.0, 0.03, [(0.11, 0.3), (0.1, 0.36), (0.06, 0.41), (0.0, 0.425)], 8)
    for sx in (-0.12, 0.12):
        _ax(p, ALU, (sx, 0.0, 0.2), "y", -0.19, -0.12, 0.035, segs=6)
        p.rod(LAQ, (sx, -0.17, 0.2), (sx + 0.02, -0.2, 0.44), 0.016, 4)
        p.box(LAQ, sx - 0.035, sx + 0.035, -0.22, -0.14, 0.12, 0.15, f="xXyYZz")
    _disc(p, PAPER, 0.0, 0.2, 0.04, -0.121, 8)
    for k, sx in enumerate((-0.17, -0.1)):
        p.cyl("CT_TileWhite", sx, 0.08, 0.3, 0.36 + 0.03 * k, 0.03, 6)
    return p.done()


def card_things(B, x, y, z=0.0, yaw=0.0, solid=False):
    """[prop] A game left on the baize: a deck, five hands of cards face down, a saucer of matches (the stakes)."""
    p = _P(B, x, y, z, yaw, "card_things")
    p.kind = "prop"
    p.box(RED, -0.032, 0.032, -0.045, 0.045, 0.0, 0.022, f="xXyYZ")
    for k in range(5):
        a = math.radians(-90 + 72 * k)
        cx, cy = 0.4 * math.cos(a), 0.4 * math.sin(a)
        for j in range(3):
            b = a + math.radians(-22 + 22 * j)
            ux, uy = math.cos(b), math.sin(b)
            vx, vy = -uy, ux
            q = [(cx + ux * s * 0.045 + vx * t * 0.03 + j * 0.012 * vx, cy + uy * s * 0.045 + vy * t * 0.03 + j * 0.012 * vy,
                  0.002 + j * 0.0015) for s, t in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
            p.face(RED if j != 1 else "CT_PlasterBlue", q)
    p.lathe("CT_TileWhite", 0.16, 0.1, [(0.03, 0.0), (0.06, 0.012)], 8, top=False, bottom=True)
    for k in range(4):
        p.rod(WOODL, (0.13 + 0.01 * k, 0.08, 0.008), (0.19, 0.11 + 0.008 * k, 0.012), 0.004, 3)
    return p.done()


def double_bass(B, x, y, z=0.0, yaw=0.0, solid=False):
    """Double bass 0.68x0.5x1.85 on a floor stand, its front (strings) toward -y, leaning back a little."""
    p = _P(B, x, y, z, yaw, "double_bass")
    lean = 0.14                                          # y per metre of height

    def yo(zz):
        return lean * (zz - 0.15)
    prof = [(0.15, 0.14), (0.2, 0.26), (0.32, 0.33), (0.47, 0.335), (0.62, 0.26), (0.72, 0.215), (0.82, 0.24),
            (0.95, 0.265), (1.06, 0.22), (1.14, 0.1)]
    rings = [[(px, py + yo(zz), zz) for px, py, _ in _ring_rect(-hw, hw, -0.1, 0.1, zz, min(0.06, hw * 0.45))]
             for zz, hw in prof]
    p.loft(WOOD, rings, top=True, bottom=True)
    # neck, fingerboard, scroll, pegs, bridge, tailpiece, strings (one thin dark strip), end pin
    p.rod(WOOD, (0.0, yo(1.14) - 0.03, 1.12), (0.0, yo(1.75) - 0.06, 1.75), 0.05, 4)
    p.rod(LAQ, (0.0, yo(0.8) - 0.125, 0.8), (0.0, yo(1.72) - 0.09, 1.72), 0.045, 4)
    p.lathe(WOOD, 0.0, yo(1.8) - 0.06, [(0.03, 1.74), (0.045, 1.79), (0.03, 1.84), (0.0, 1.85)], 6)
    for k, zz in enumerate((1.72, 1.77)):
        p.rod(BRASS, (-0.07, yo(zz) - 0.06, zz), (0.07, yo(zz) - 0.06, zz), 0.014, 3)
    p.box(WOODL, -0.06, 0.06, yo(0.6) - 0.19, yo(0.6) - 0.1, 0.58, 0.6, f="xXyYZz")
    p.rod(LAQ, (0.0, yo(0.22) - 0.11, 0.22), (0.0, yo(0.48) - 0.125, 0.48), 0.06, 4)
    q = [(-0.022, yo(0.6) - 0.195, 0.6), (0.022, yo(0.6) - 0.195, 0.6), (0.022, yo(1.7) - 0.118, 1.7),
         (-0.022, yo(1.7) - 0.118, 1.7)]
    p.face(ALU, q)
    for sx in (-0.2, 0.2):                               # f-holes
        p.box(LAQ, sx - 0.012, sx + 0.012, yo(0.6) - 0.104, yo(0.6) - 0.1, 0.52, 0.7, f="y")
    p.rod(STEEL, (0.0, 0.0, 0.0), (0.0, 0.0, 0.15), 0.014, 3)
    # the stand: a U-yoke behind the body on a tripod
    p.rod(STEEL, (0.0, 0.3, 0.0), (0.0, yo(0.95) + 0.12, 0.95), 0.022, 4)
    p.rod(STEEL, (-0.24, yo(0.95) + 0.11, 0.95), (0.24, yo(0.95) + 0.11, 0.95), 0.02, 4)
    for sx in (-0.24, 0.24):
        p.rod(RUBBER, (sx, yo(0.95) + 0.11, 0.95), (sx, yo(0.95) - 0.02, 0.95), 0.024, 4)
        p.rod(STEEL, (0.0, 0.3, 0.0), (sx * 1.2, -0.18, 0.0), 0.02, 4)
    p.rod(STEEL, (0.0, 0.3, 0.0), (0.0, 0.5, 0.0), 0.02, 4)
    return p.done(front=0.7, footprint=(-0.34, 0.34, -0.2, 0.5), stand=(0.55, -0.35, 0.0))


def drum_kit(B, x, y, z=0.0, yaw=0.0, solid=False):
    """Small jazz drum kit 1.5x1.35x1.3: bass drum (front head toward -y, the audience), snare with a pair of brushes,
    a floor tom, a hi-hat, a ride cymbal and the drummer's round stool at +y ('seat')."""
    p = _P(B, x, y, z, yaw, "drum_kit")
    shell, head, rim = "CT_PlasterBlue", PAPER, ALU
    # bass drum on its side
    _ax(p, shell, (0.0, 0.0, 0.29), "y", -0.42, -0.06, 0.28, segs=12, caps=False)
    for ya, yb in ((-0.44, -0.42), (-0.06, -0.04)):
        _ax(p, rim, (0.0, 0.0, 0.29), "y", ya, yb, 0.295, segs=12, caps=False)
    _disc(p, head, 0.0, 0.29, 0.28, -0.43, 12, a0=math.pi / 12)
    p.face(head, [(0.28 * math.cos(-a), -0.05, 0.29 + 0.28 * math.sin(-a)) for a in
                  [math.pi / 12 + 2 * math.pi * k / 12 for k in range(12)]])
    _disc(p, "CT_PlasterYellow", 0.0, 0.29, 0.1, -0.432, 10)                 # the band's sun on the front head
    for sx in (-0.3, 0.3):
        p.rod(STEEL, (sx * 0.85, -0.38, 0.2), (sx * 1.15, -0.5, 0.0), 0.014, 3)
    p.box(STEEL, -0.05, 0.05, -0.04, 0.16, 0.0, 0.02, f="xXyYZ")              # pedal
    p.rod(STEEL, (0.0, 0.0, 0.02), (0.0, -0.035, 0.26), 0.014, 3)
    # snare on a tripod, brushes on it
    sx, sy = -0.36, 0.3
    p.cyl(ALU, sx, sy, 0.6, 0.73, 0.18, 10, top=False)
    p.cyl(head, sx, sy, 0.73, 0.735, 0.175, 10)
    p.cyl(STEEL, sx, sy, 0.0, 0.6, 0.014, 4, top=False)
    for k in range(3):
        a = math.radians(90 + 120 * k)
        p.rod(STEEL, (sx, sy, 0.22), (sx + 0.22 * math.cos(a), sy + 0.22 * math.sin(a), 0.0), 0.012, 3)
    for k, (dx, dy) in enumerate(((0.02, 0.0), (-0.03, 0.03))):
        a0 = (sx - 0.12 + dx, sy - 0.1 + dy, 0.742)
        a1 = (sx + 0.08 + dx, sy + 0.12 + dy, 0.742)
        p.rod(LAQ, a0, ((a0[0] + a1[0]) / 2, (a0[1] + a1[1]) / 2, 0.742), 0.012, 3)
        q = [((a0[0] + a1[0]) / 2, (a0[1] + a1[1]) / 2, 0.74 + 0.002 * k), (a1[0] + 0.04, a1[1] - 0.03, 0.74 + 0.002 * k),
             (a1[0] - 0.03, a1[1] + 0.04, 0.74 + 0.002 * k)]
        p.face(ALU, q)                                                         # the wire fan
    # floor tom
    tx, ty = 0.42, 0.34
    p.cyl(shell, tx, ty, 0.3, 0.66, 0.2, 10, top=False, bottom=True)
    p.cyl(head, tx, ty, 0.66, 0.665, 0.195, 10)
    p.cyl(rim, tx, ty, 0.64, 0.668, 0.208, 10, top=False)
    for k in range(3):
        a = math.radians(30 + 120 * k)
        p.rod(STEEL, (tx + 0.2 * math.cos(a), ty + 0.2 * math.sin(a), 0.5), (tx + 0.25 * math.cos(a), ty + 0.25 * math.sin(a), 0.0),
              0.012, 3)
    # hi-hat and ride
    hx, hy = -0.66, 0.02
    p.cyl(STEEL, hx, hy, 0.0, 0.98, 0.01, 4, top=False)
    for k in range(3):
        a = math.radians(30 + 120 * k)
        p.rod(STEEL, (hx, hy, 0.25), (hx + 0.2 * math.cos(a), hy + 0.2 * math.sin(a), 0.0), 0.012, 3)
    p.lathe(BRASS, hx, hy, [(0.17, 0.86), (0.03, 0.885)], 10, top=True, bottom=True)
    p.lathe(BRASS, hx, hy, [(0.03, 0.895), (0.17, 0.915)], 10, top=True, bottom=True)
    rx, ry = 0.62, -0.1
    p.cyl(STEEL, rx, ry, 0.0, 1.22, 0.01, 4, top=False)
    for k in range(3):
        a = math.radians(-30 + 120 * k)
        p.rod(STEEL, (rx, ry, 0.3), (rx + 0.22 * math.cos(a), ry + 0.22 * math.sin(a), 0.0), 0.012, 3)
    ring0 = [(rx + 0.24 * math.cos(2 * math.pi * k / 10), ry + 0.24 * math.sin(2 * math.pi * k / 10) * 0.96,
              1.2 + 0.06 * math.sin(2 * math.pi * k / 10)) for k in range(10)]
    ring1 = [(rx + 0.04 * math.cos(2 * math.pi * k / 10), ry + 0.04 * math.sin(2 * math.pi * k / 10), 1.235) for k in range(10)]
    p.loft(BRASS, [ring0, ring1], top=True, bottom=True)
    # stool
    p.cyl(STEEL, 0.0, 0.62, 0.0, 0.46, 0.02, 5, top=False)
    for k in range(3):
        a = math.radians(90 + 120 * k)
        p.rod(STEEL, (0.0, 0.62, 0.2), (0.2 * math.cos(a), 0.62 + 0.2 * math.sin(a), 0.0), 0.014, 3)
    p.cyl("CT_Leather", 0.0, 0.62, 0.46, 0.52, 0.16, 10)
    return p.done(front=0.9, seat=(0.0, 0.62, 0.52), footprint=(-0.75, 0.75, -0.5, 0.8))


def reed_stand(B, x, y, z=0.0, yaw=0.0, solid=False):
    """Clarinet and alto saxophone on one floor stand 0.45x0.4x0.85 (bells down on their pegs), front -y."""
    p = _P(B, x, y, z, yaw, "reed_stand")
    for k in range(3):
        a = math.radians(90 + 120 * k)
        p.rod(STEEL, (0.0, 0.0, 0.12), (0.22 * math.cos(a), 0.22 * math.sin(a), 0.0), 0.014, 3)
    p.cyl(STEEL, 0.0, 0.0, 0.1, 0.5, 0.012, 4, top=False)
    p.rod(STEEL, (-0.16, 0.0, 0.14), (0.16, 0.0, 0.14), 0.014, 3)
    # clarinet: black body, silver keys, upright on a peg
    cx = -0.16
    p.lathe(LAQ, cx, 0.0, [(0.04, 0.14), (0.022, 0.2), (0.015, 0.28), (0.014, 0.76), (0.016, 0.78), (0.01, 0.84)], 6)
    for zz in (0.36, 0.46, 0.56, 0.66):
        p.box(ALU, cx - 0.004, cx + 0.004, -0.022, -0.015, zz, zz + 0.05, f="xXyZz")
    # saxophone: bell up beside the body, the body leaning on the stand's yoke
    sx = 0.14
    _post(p, BRASS, (sx, 0.02, 0.2), (sx - 0.02, 0.1, 0.78), 0.05, 0.016, 6)
    p.lathe(BRASS, sx, -0.02, [(0.045, 0.14), (0.055, 0.2)], 6, top=False, bottom=True)        # the bow
    _post(p, BRASS, (sx + 0.03, -0.1, 0.2), (sx + 0.05, -0.16, 0.5), 0.045, 0.085, 8)             # the bell
    p.rod(BRASS, (sx - 0.02, 0.1, 0.78), (sx - 0.04, -0.02, 0.86), 0.022, 5)                       # crook
    p.rod(LAQ, (sx - 0.04, -0.02, 0.86), (sx - 0.05, -0.08, 0.84), 0.02, 4)                        # mouthpiece
    for zz in (0.32, 0.42, 0.52, 0.62):
        _ax(p, ALU, (sx - 0.005 - 0.03 * (zz - 0.2), -0.035 + 0.14 * (zz - 0.2), zz), "y", -0.01, 0.0, 0.016, segs=6)
    p.rod(STEEL, (0.0, 0.0, 0.5), (sx - 0.02, 0.13, 0.55), 0.014, 3)
    return p.done(front=0.6)


def music_stand(B, x, y, z=0.0, yaw=0.0, solid=False):
    """Folding music stand 0.5x0.4x1.25 with two sheets; the player stands at -y... reads from -y (front)."""
    p = _P(B, x, y, z, yaw, "music_stand")
    for k in range(3):
        a = math.radians(90 + 120 * k)
        p.rod(STEEL, (0.0, 0.0, 0.2), (0.2 * math.cos(a), 0.2 * math.sin(a), 0.0), 0.012, 3)
    p.cyl(STEEL, 0.0, 0.0, 0.18, 1.0, 0.009, 4, top=False)
    a_, b_ = (-0.24, -0.03, 0.93), (0.24, -0.03, 0.93)
    c_, d_ = (0.24, 0.05, 1.24), (-0.24, 0.05, 1.24)
    p.slab(STEEL, a_, b_, c_, d_, 0.006)
    p.box(STEEL, -0.24, 0.24, -0.07, -0.03, 0.925, 0.935, f="xXyYZz")
    p.card("paper_typed", (-0.21, -0.034, 0.95), (-0.01, -0.034, 0.95), (-0.01, 0.042, 1.22), (-0.21, 0.042, 1.22))
    p.card("paper_typed", (0.01, -0.034, 0.95), (0.21, -0.034, 0.95), (0.21, 0.042, 1.22), (0.01, 0.042, 1.22))
    return p.done(front=0.5)


def bandstand_floor(B, x, y, z=0.0, yaw=0.0, pts=(), h=0.2, solid=True):
    """The bandstand's low platform: pts = its outline in the LOCAL frame (CCW from above), a carpet-red top with a
    brass nosing all round, dark sides."""
    p = _P(B, x, y, z, yaw, "bandstand_floor")
    p.kind = "flat"
    r0 = [(u, v, 0.0) for u, v in pts]
    r1 = [(u, v, h - 0.02) for u, v in pts]
    r2 = [(u, v, h) for u, v in pts]
    p.loft(WOOD, [r0, r1], top=False)
    p.loft(BRASS, [r1, r2], top=False)
    p.face("CT_PlasterRed", r2)
    xs, ys = [u for u, _ in pts], [v for _, v in pts]
    if solid:                                            # the walkable body, hidden inside the visible platform
        cu, cv = sum(xs) / len(xs), sum(ys) / len(ys)
        poly = [p.fr((cu + (u - cu) * 0.985, cv + (v - cv) * 0.985, 0.0))[:2] for u, v in pts]
        area = sum(poly[i][0] * poly[(i + 1) % len(poly)][1] - poly[(i + 1) % len(poly)][0] * poly[i][1]
                   for i in range(len(poly)))
        if area < 0:
            poly = poly[::-1]
        kit.geom.prism(B.m, "CT_Steel", poly, z + 0.01, z + h - 0.004, top=True, bottom=True)
    return p.done(solid=False, top=h, footprint=(min(xs), max(xs), min(ys), max(ys)))


def radiogram(B, x, y, z=0.0, yaw=0.0, solid=True):
    """[wall] Radiogram 1.05x0.45x0.86: a veneered cabinet on tapered legs, cloth grille and a lit dial with knobs on the
    front, the lid raised over the record deck with a record on it. 'deck' = the turntable."""
    p = _P(B, x, y, z, yaw, "radiogram")
    p.kind = "wall"
    hw, d = 0.525, 0.45
    for sx in (-hw + 0.07, hw - 0.07):
        for sy in (-d + 0.06, -0.06):
            _post(p, WOOD, (sx * 1.04, sy, 0.0), (sx, sy, 0.22), 0.016, 0.026)
    p.bev(WOOD, -hw, hw, -d, 0.0, 0.2, 0.8, 0.012, 0.02, bottom=True)
    p.card_front("grille", -hw + 0.06, 0.08, 0.27, 0.72, -d - 0.004)
    p.box(LAQ, 0.12, hw - 0.05, -d - 0.006, -d, 0.5, 0.72, f="xXyZz")
    p.card_front("dial_radio", 0.14, hw - 0.07, 0.53, 0.7, -d - 0.008)
    for k in range(3):
        kit._knob(p, BRASS, 0.17 + k * 0.11, 0.38, 0.022, -d - 0.035, -d)
    # the well with the turntable (left), the raised lid behind it
    p.box(LAQ, -hw + 0.05, 0.05, -d + 0.05, -0.05, 0.8, 0.806, f="xXyYZ")
    p.cyl(RUBBER, -0.24, -d / 2, 0.806, 0.82, 0.15, 12)
    p.card_top("record_label", -0.29, -0.19, -d / 2 - 0.05, -d / 2 + 0.05, 0.8215)
    p.rod(ALU, (-0.03, -0.1, 0.83), (-0.16, -d / 2 - 0.06, 0.83), 0.014, 3)
    p.slab(WOOD, (-hw, -0.03, 0.8), (hw, -0.03, 0.8), (hw, -0.005, 1.22), (-hw, -0.005, 1.22), 0.02)
    p.core(-hw + 0.02, hw - 0.02, -d + 0.02, -0.02, 0.25, 0.78)
    return p.done(solid=solid, top=0.8, deck=(-0.24, -d / 2, 0.82), front=0.5, pin=(-hw, hw, -d, 0.0, 0.0, 1.22))


def wall_sconce(B, x, y, z=0.0, yaw=0.0, zc=1.9, solid=False):
    """[wall] Opal wall lamp: a brass back plate and arm, a pleated opal glass shade (glows; no real light)."""
    p = _P(B, x, y, z, yaw, "wall_sconce")
    p.kind = "wall"
    _disc(p, BRASS, 0.0, zc - 0.06, 0.055, -0.008, 8)
    p.rod(BRASS, (0.0, 0.0, zc - 0.06), (0.0, -0.11, zc - 0.06), 0.016, 4)
    p.cyl(BRASS, 0.0, -0.11, zc - 0.08, zc - 0.04, 0.03, 6)
    p.lathe(OPAL, 0.0, -0.11, [(0.045, zc - 0.04), (0.085, zc + 0.02), (0.075, zc + 0.12), (0.03, zc + 0.15)], 8)
    return p.done()


def barometer(B, x, y, z=0.0, yaw=0.0, zc=1.6, r=0.11, solid=False):
    """[wall] Ship's aneroid barometer: a brass case with a white dial, a scale arc, the needle and the brass set hand."""
    p = _P(B, x, y, z, yaw, "barometer")
    p.kind = "wall"
    p.box(WOOD, -r - 0.04, r + 0.04, -0.015, 0.0, zc - r - 0.04, zc + r + 0.04, f="xXyZz")
    _ax(p, BRASS, (0.0, 0.0, zc), "y", -0.06, -0.015, r, segs=12, caps=False)
    _disc(p, BRASS, 0.0, zc, r, -0.06, 12, a0=math.pi / 12)
    _disc(p, PAPER, 0.0, zc, r * 0.86, -0.0605, 12, a0=math.pi / 12)
    _sector(p, LAQ, 0.0, zc, r * 0.8, -0.061, -120, 120, n=8, r0=r * 0.72)
    _hand(p, LAQ, 0.0, zc, r * 0.7, -0.064, 35, 0.008)
    _hand(p, BRASS, 0.0, zc, r * 0.62, -0.068, -20, 0.006)
    kit._knob(p, BRASS, 0.0, zc, 0.012, -0.075, -0.06, 6)
    return p.done()


# ================================================================================================ lobby pieces
def glazed_bookcase(B, x, y, z=0.0, yaw=0.0, w=1.2, h=2.0, topic=None, seed=0, solid=True):
    """[wall] The ship's library case: the kit's bookshelf behind two glazed brass-framed doors (frames only, the glass is
    left to the eye), a cornice, a plinth with a drawer."""
    r = kit.bookshelf(B, x, y, z, yaw, w=w, h=h, topic=topic, seed=seed, solid=solid)
    p = _P(B, x, y, z, yaw, "bookcase_doors")
    p.kind = "wall"
    hw, yf = w / 2, -0.335
    for a, b in ((-hw + 0.01, -0.01), (0.01, hw - 0.01)):
        for (x0, x1, z0, z1) in ((a, b, 0.32, 0.36), (a, b, h - 0.1, h - 0.06), (a, a + 0.04, 0.36, h - 0.1),
                                 (b - 0.04, b, 0.36, h - 0.1)):
            p.box(WOOD, x0, x1, yf - 0.02, yf, z0, z1, f="xXyZz")
        p.box(GLASS, a + 0.04, b - 0.04, yf - 0.008, yf - 0.006, 0.36, h - 0.1, f="y")
    for sx in (-0.05, 0.05):
        kit._knob(p, BRASS, sx, 1.1, 0.014, yf - 0.045, yf - 0.02, 6)
    p.box(WOOD, -hw - 0.03, hw + 0.03, -0.37, 0.0, h, h + 0.05, f="xXyZz")
    p.done()
    return r


def ship_model_case(B, x, y, z=0.0, yaw=0.0, solid=True):
    """[wall] The builder's model of the ship in a brass-framed glass case 1.5x0.5x1.55 on a veneered stand: white hull
    with a green boot-topping, the long deckhouse, the funnel with its sun band, two masts."""
    p = _P(B, x, y, z, yaw, "ship_model_case")
    p.kind = "wall"
    hw, d, zt = 0.75, 0.5, 0.9
    p.box(WOOD, -hw, hw, -d, 0.0, 0.08, zt, f="xXyZ")
    p.box(LAQ, -hw + 0.04, hw - 0.04, -d + 0.04, 0.0, 0.0, 0.08, f="xXy")
    p.box(BAIZE, -hw + 0.03, hw - 0.03, -d + 0.03, -0.03, zt, zt + 0.004, f="Z")
    g0, g1 = zt, zt + 0.62
    for sx in (-hw + 0.012, hw - 0.012):
        for sy in (-d + 0.012, -0.012):
            p.rod(BRASS, (sx, sy, g0), (sx, sy, g1), 0.02, 4)
    for sy in (-d + 0.012, -0.012):
        p.rod(BRASS, (-hw, sy, g1), (hw, sy, g1), 0.02, 4)
    for sx in (-hw + 0.012, hw - 0.012):
        p.rod(BRASS, (sx, -d, g1), (sx, 0.0, g1), 0.02, 4)
    p.box(BRASS, -0.2, 0.2, -d - 0.004, -d, zt - 0.12, zt - 0.05, f="xXyZz")       # the name plate
    # the model: 1.16 long = 1:53
    yc, zk = -d / 2, zt + 0.07
    st = [(-0.58, 0.035), (-0.5, 0.075), (-0.3, 0.092), (0.2, 0.092), (0.4, 0.07), (0.52, 0.03), (0.58, 0.004)]
    for (xa, ha), (xb, hb) in zip(st, st[1:]):
        for (z0, z1, mat) in ((zk, zk + 0.025, "CT_PlasterGreen"), (zk + 0.025, zk + 0.085, "CT_TileWhite")):
            p.hexa(mat, [(xa, yc - ha, z0), (xb, yc - hb, z0), (xb, yc + hb, z0), (xa, yc + ha, z0)],
                   [(xa, yc - ha, z1), (xb, yc - hb, z1), (xb, yc + hb, z1), (xa, yc + ha, z1)], f="all")
    p.box("CT_TileWhite", -0.44, 0.3, yc - 0.065, yc + 0.065, zk + 0.085, zk + 0.135, f="xXyYZ")
    p.box("CT_TileWhite", -0.36, 0.27, yc - 0.06, yc + 0.06, zk + 0.135, zk + 0.18, f="xXyYZ")
    p.box("CT_TileWhite", 0.07, 0.24, yc - 0.05, yc + 0.05, zk + 0.18, zk + 0.225, f="xXyYZ")
    p.box(LAQ, -0.34, 0.25, yc - 0.0605, yc - 0.06, zk + 0.15, zk + 0.168, f="y")                  # the window band
    _post(p, "CT_TileWhite", (-0.12, yc, zk + 0.18), (-0.14, yc, zk + 0.29), 0.035, 0.03, 6)
    p.cyl("CT_PlasterYellow", -0.133, yc, zk + 0.235, zk + 0.26, 0.034, 6, top=False)
    for mx, mz in ((0.4, 0.3), (-0.22, 0.28)):
        p.rod(WOODL, (mx, yc, zk + 0.085), (mx - 0.01, yc, zk + mz), 0.008, 3)
    for sy in (-0.075, 0.075):
        p.box(WOODL, -0.3, -0.18, yc + sy - 0.012, yc + sy + 0.012, zk + 0.19, zk + 0.205, f="xXyYZ")
    for sx in (-0.4, 0.38):
        p.box(WOOD, sx - 0.012, sx + 0.012, yc - 0.06, yc + 0.06, zt + 0.004, zk + 0.03, f="xXyY")
    p.core(-hw + 0.02, hw - 0.02, -d + 0.02, -0.02, 0.1, zt - 0.02)
    return p.done(solid=solid, top=zt, front=0.5)


def honour_board(B, x, y, z=0.0, yaw=0.0, w=1.3, h=0.95, zc=1.55, solid=False):
    """[wall] «Доска почёта»: a red cloth board in a wooden frame with a brass header strip and two rows of framed
    portraits with a plate under each."""
    p = _P(B, x, y, z, yaw, "honour_board")
    p.kind = "wall"
    hw, z0, z1 = w / 2, zc - h / 2, zc + h / 2
    p.bev(WOOD, -hw - 0.04, hw + 0.04, -0.03, 0.0, z0 - 0.04, z1 + 0.04, 0.008)
    p.box("CT_PlasterRed", -hw, hw, -0.034, -0.03, z0, z1, f="y")
    p.box(BRASS, -hw * 0.55, hw * 0.55, -0.04, -0.034, z1 - 0.13, z1 - 0.05, f="xXyZz")
    cw = (w - 0.2) / 4
    for r_ in range(2):
        for c in range(4):
            cx = -hw + 0.1 + cw * (c + 0.5)
            cz = z0 + 0.1 + (h - 0.3) / 2 * (r_ + 0.5)
            ph = min((h - 0.3) / 2 - 0.1, cw * 1.2)
            pw = ph * 0.78
            p.box(WOODL, cx - pw / 2 - 0.012, cx + pw / 2 + 0.012, -0.042, -0.034, cz - ph / 2 - 0.012 + 0.03,
                  cz + ph / 2 + 0.012 + 0.03, f="xXyZz")
            p.card_front("painting_portrait", cx - pw / 2, cx + pw / 2, cz - ph / 2 + 0.03, cz + ph / 2 + 0.03, -0.044,
                         rect=(0.12 + 0.02 * c, 0.1, 0.88 - 0.02 * r_, 0.95))
            p.box(PAPER, cx - pw / 2, cx + pw / 2, -0.038, -0.034, cz - ph / 2 - 0.035, cz - ph / 2 + 0.002, f="xXyZz")
    return p.done()


def programme_board(B, x, y, z=0.0, yaw=0.0, w=1.0, h=0.8, zc=1.55, solid=False):
    """[wall] The daily programme: a baize board in a brass frame, the typed day's sheet, the route chart, two notes."""
    p = _P(B, x, y, z, yaw, "programme_board")
    p.kind = "wall"
    hw, z0, z1 = w / 2, zc - h / 2, zc + h / 2
    p.bev(BRASS, -hw - 0.03, hw + 0.03, -0.025, 0.0, z0 - 0.03, z1 + 0.03, 0.008)
    p.box(BAIZE, -hw, hw, -0.029, -0.025, z0, z1, f="y")
    p.card_front("paper_typed", -hw + 0.05, -hw + 0.05 + 0.4, z1 - 0.62, z1 - 0.06, -0.033)
    mh = kit._cell_h("map_coast", 0.42)
    mh = min(mh, 0.4)
    p.card_front("map_coast", hw - 0.48, hw - 0.06, z1 - 0.06 - mh, z1 - 0.06, -0.033)
    p.card_front("paper_letter", hw - 0.42, hw - 0.2, z0 + 0.05, z0 + 0.3, -0.033)
    for (px, pz) in ((-hw + 0.25, z1 - 0.07), (hw - 0.27, z1 - 0.07), (hw - 0.31, z0 + 0.29)):
        kit._knob(p, RED, px, pz, 0.008, -0.04, -0.033, 6)
    return p.done()


# ================================================================================================ cabin pieces
def berth(B, x, y, z=0.0, yaw=0.0, blanket="CT_PlasterBlue", solid=True):
    """Low ship's bed 0.85x1.9x0.78: a veneered box with two drawers, a lee board, mattress, folded-back blanket and a
    pillow; the head at +y."""
    p = _P(B, x, y, z, yaw, "berth")
    hw, hl = 0.425, 0.95
    p.box(WOOD, -hw, hw, -hl, hl, 0.05, 0.32, f="xXyYZ")
    p.box(LAQ, -hw + 0.03, hw - 0.03, -hl + 0.03, hl - 0.03, 0.0, 0.05, f="xXyY")
    for s in (-1, 1):
        for (a, b) in ((-hl + 0.1, -0.05), (0.05, hl - 0.1)):
            xa, xb = sorted((s * hw, s * (hw + 0.006)))
            p.box(WOODL, xa, xb, a, b, 0.1, 0.27, f="xXyYZz")
            xa, xb = sorted((s * (hw + 0.006), s * (hw + 0.02)))
            p.box(BRASS, xa, xb, (a + b) / 2 - 0.05, (a + b) / 2 + 0.05, 0.175, 0.195, f="xXyYZz")
    p.bev(WOOD, -hw, hw, hl - 0.04, hl, 0.32, 0.78, 0.012)                       # headboard
    p.box(WOOD, -hw, hw, -hl, -hl + 0.03, 0.32, 0.5, f="xXyYZ")                  # foot board
    for s in (-1, 1):                                                            # lee boards (low, open in the middle)
        p.box(WOOD, s * hw - (0.025 if s > 0 else 0.0), s * hw + (0.0 if s > 0 else 0.025), 0.35, hl - 0.04, 0.32, 0.47,
              f="xXyYZ")
    _soft(p, "CT_TileWhite", -hw + 0.03, hw - 0.03, -hl + 0.04, hl - 0.05, 0.32, 0.44, 0.012, 0.04, 1)
    _soft(p, blanket, -hw + 0.025, hw - 0.025, -hl + 0.035, 0.35, 0.43, 0.47, 0.01, 0.04, 1)
    p.box("CT_TileWhite", -hw + 0.03, hw - 0.03, 0.2, 0.37, 0.47, 0.478, f="xXyYZ")
    _soft(p, "CT_TileWhite", -0.28, 0.28, 0.48, hl - 0.08, 0.44, 0.54, 0.015, 0.06, 1)
    p.core(-hw + 0.03, hw - 0.03, -hl + 0.05, hl - 0.05, 0.1, 0.42)
    return p.done(solid=solid, top=0.47, seats=[(0.0, -0.1, 0.47, 0.0)], front=0.7)


def washbasin(B, x, y, z=0.0, yaw=0.0, solid=False):
    """[wall] Cabin washbasin niche 0.55x0.4x1.85: a white enamel basin on brackets with two brass taps, a splash-back of
    tiles, a glass shelf with a tumbler, a mirror in a brass frame and a towel on a rail. 'tap' = the taps."""
    p = _P(B, x, y, z, yaw, "washbasin")
    p.kind = "wall"
    p.box("CT_TileWhite", -0.3, 0.3, -0.012, 0.0, 0.7, 1.22, f="xXyZz")
    p.bev("CT_TileWhite", -0.26, 0.26, -0.4, -0.012, 0.72, 0.86, 0.02, 0.05, bottom=True)
    p.box("CT_PlasterGrey", -0.19, 0.19, -0.33, -0.09, 0.8605, 0.861, f="Z")
    for sx in (-0.12, 0.12):
        p.cyl(BRASS, sx, -0.06, 0.86, 0.93, 0.014, 5)
        p.rod(BRASS, (sx, -0.06, 0.92), (sx, -0.15, 0.91), 0.014, 4)
        p.rod(STEEL, (sx * 1.7, -0.012, 0.6), (sx * 1.7, -0.3, 0.74), 0.02, 3)
    p.cyl(ALU, 0.0, -0.2, 0.45, 0.72, 0.02, 5, top=False)
    p.box(GLASS, -0.24, 0.24, -0.12, -0.012, 1.24, 1.25, f="xXyYZz")
    p.cyl(GLASS, 0.14, -0.06, 1.25, 1.34, 0.03, 6)
    p.bev(BRASS, -0.24, 0.24, -0.03, 0.0, 1.32, 1.88, 0.008)
    p.box("CT_Aluminium", -0.215, 0.215, -0.034, -0.03, 1.345, 1.855, f="y")
    p.rod(BRASS, (0.34, -0.012, 1.0), (0.34, -0.2, 1.0), 0.014, 3)
    p.box("CT_PlasterYellow", 0.325, 0.355, -0.19, -0.04, 0.62, 1.0, f="xXyYz")
    return p.done(tap=(0.0, -0.12, 0.92), front=0.5)


# ================================================================================================ deck pieces
def ash_urn(B, x, y, z=0.0, yaw=0.0, solid=True):
    """Deck ash urn d0.34 x0.72: a white-painted cast pedestal with a brass sand bowl."""
    p = _P(B, x, y, z, yaw, "ash_urn")
    p.lathe(WHITE, 0, 0, [(0.17, 0.0), (0.17, 0.03), (0.08, 0.08), (0.07, 0.5), (0.09, 0.58)], 8, top=False)
    p.lathe(BRASS, 0, 0, [(0.09, 0.58), (0.16, 0.66), (0.165, 0.72), (0.14, 0.72)], 8, top=False)
    p.cyl("CT_Gravel", 0, 0, 0.69, 0.7, 0.142, 8)
    for a in (0.5, 2.4, 4.0):
        p.rod(PAPER, (0.05 * math.cos(a), 0.05 * math.sin(a), 0.7), (0.09 * math.cos(a), 0.09 * math.sin(a), 0.72), 0.008, 3)
    p.core(-0.04, 0.04, -0.04, 0.04, 0.1, 0.5)
    return p.done(solid=solid, top=0.72, front=0.5)


def ring_toss(B, x, y, z=0.0, yaw=0.0, solid=True):
    """Кольцеброс 0.9x0.75x1.25: a slanted varnished board on an A-frame with nine brass hooks on coloured score plates,
    rope rings hanging on three of them and three lying on the deck in front; 'throw' = the throwing mark 2.5 m out."""
    p = _P(B, x, y, z, yaw, "ring_toss")
    hw = 0.42

    def yb(zz):                                           # the board's front plane leans back
        return -0.12
    p.slab(WOOD, (-hw, yb(0.35), 0.35), (hw, yb(0.35), 0.35), (hw, yb(1.25), 1.25), (-hw, yb(1.25), 1.25), 0.05)
    p.slab(WHITE, (-hw + 0.04, yb(0.39) - 0.002, 0.39), (hw - 0.04, yb(0.39) - 0.002, 0.39),
           (hw - 0.04, yb(1.21) - 0.002, 1.21), (-hw + 0.04, yb(1.21) - 0.002, 1.21), 0.002, f="front")
    for s in (-1, 1):
        p.rod(WOOD, (s * (hw - 0.03), yb(1.2) + 0.03, 1.2), (s * (hw - 0.03), -0.22, 0.0), 0.04, 4)
        p.rod(WOOD, (s * (hw - 0.03), yb(1.1) + 0.03, 1.1), (s * (hw - 0.03), 0.5, 0.0), 0.04, 4)
    p.rod(WOOD, (-hw + 0.03, 0.5, 0.02), (hw - 0.03, 0.5, 0.02), 0.035, 4)
    cols = [RED, "CT_PlasterBlue", "CT_PlasterYellow"]
    hooks = []
    for r_ in range(3):
        for c in range(3):
            hx, hz = -0.26 + 0.26 * c, 0.52 + 0.27 * r_
            yy = yb(hz) - 0.004
            q = [(hx - 0.05, yb(hz - 0.05) - 0.005, hz - 0.05), (hx + 0.05, yb(hz - 0.05) - 0.005, hz - 0.05),
                 (hx + 0.05, yb(hz + 0.05) - 0.005, hz + 0.05), (hx - 0.05, yb(hz + 0.05) - 0.005, hz + 0.05)]
            p.face(cols[(r_ + c) % 3], q)
            p.rod(BRASS, (hx, yy, hz), (hx, yy - 0.06, hz + 0.01), 0.012, 4)
            p.rod(BRASS, (hx, yy - 0.06, hz + 0.01), (hx, yy - 0.07, hz + 0.05), 0.012, 4)
            hooks.append((hx, yy - 0.05, hz))
    for k in (1, 3, 8):
        hx, hy, hz = hooks[k]
        _torus(p, WOODL, (hx, hy, hz - 0.075), 0.075, 0.012, "y", 10, 4)
    for (rx, ry) in ((-0.2, -0.55), (0.12, -0.7), (0.3, -0.45)):
        _torus(p, WOODL, (rx, ry, 0.013), 0.075, 0.012, "z", 10, 4)
    p.core(-hw + 0.02, hw - 0.02, -0.112, -0.078, 0.4, 1.2)
    return p.done(solid=solid, front=1.0, footprint=(-hw, hw, -0.22, 0.52), throw=(0.0, -2.5, 0.0))


def lifejacket_chest(B, x, y, z=0.0, yaw=0.0, w=1.3, solid=True):
    """White lifejacket chest w x0.5x0.5 that doubles as a bench: slatted lid, a red band, brass hasp, rope handles."""
    p = _P(B, x, y, z, yaw, "lifejacket_chest")
    hw = w / 2
    for sx in (-hw + 0.06, hw - 0.06):
        p.box(WHITE, sx - 0.04, sx + 0.04, -0.22, 0.22, 0.0, 0.05, f="xXyY")
    p.box(WHITE, -hw, hw, -0.25, 0.25, 0.05, 0.44, f="xXyY")
    p.box(RED, -hw - 0.003, hw + 0.003, -0.253, 0.253, 0.2, 0.3, f="xXyY")
    p.bev(WOODL, -hw - 0.02, hw + 0.02, -0.27, 0.27, 0.44, 0.48, 0.01, 0.0, bottom=True)
    for k in range(5):
        yy = -0.22 + k * 0.11
        p.box(WOOD, -hw, hw, yy - 0.004, yy + 0.004, 0.4795, 0.481, f="Z")
    p.box(BRASS, -0.03, 0.03, -0.262, -0.25, 0.36, 0.46, f="xXyZz")
    for s in (-1, 1):
        p.rod(WOODL, (s * (hw + 0.012), -0.1, 0.33), (s * (hw + 0.03), 0.0, 0.28), 0.016, 3)
        p.rod(WOODL, (s * (hw + 0.03), 0.0, 0.28), (s * (hw + 0.012), 0.1, 0.33), 0.016, 3)
    p.core(-hw + 0.03, hw - 0.03, -0.22, 0.22, 0.1, 0.42)
    return p.done(solid=solid, top=0.48, seats=[(0.0, 0.0, 0.48, 0.0)])


def shuffleboard(B, x, y, z=0.0, yaw=0.0, w=1.8, d=8.0, solid=False):
    """[flat] Shuffleboard court painted on the planks, w across (x) by d along (y): the boundary, the two scoring
    triangles (10 / 8 8 / 7 7 and the «10 off» base), dead lines; eight discs (four red, four yellow) and two cues
    lying alongside. 'ends' = the two players' places, 'rack' = where the cues lie."""
    p = _P(B, x, y, z, yaw, "shuffleboard")
    p.kind = "flat"
    zz, lw, mat = 0.005, 0.04, "CT_TileWhite"
    a, b, e = -w / 2, w / 2, d / 2
    for (p0, p1) in (((a, -e), (b, -e)), ((b, -e), (b, e)), ((b, e), (a, e)), ((a, e), (a, -e))):
        _line(p, mat, p0, p1, lw, zz)
    for s in (-1, 1):
        base, apex = s * (e - 0.45), s * (e - 2.6)
        tw = w / 2 - 0.12
        _line(p, mat, (-tw, base), (0.0, apex), lw, zz)
        _line(p, mat, (tw, base), (0.0, apex), lw, zz)
        _line(p, mat, (-tw, base), (tw, base), lw, zz)
        _line(p, mat, (-tw, s * (e - 0.08)), (-tw, base), lw, zz)                 # the «10 off» box
        _line(p, mat, (tw, s * (e - 0.08)), (tw, base), lw, zz)
        for f in (0.36, 0.68):                                                   # 7 | 8 and 8 | 10 dividers
            yy = base + (apex - base) * f
            hw_ = tw * (1 - f)
            _line(p, mat, (-hw_, yy), (hw_, yy), lw, zz)
        _line(p, mat, (0.0, base), (0.0, base + (apex - base) * 0.68), lw, zz)   # the centre line of the 7s and 8s
        _line(p, mat, (a, s * (e - 3.3)), (b, s * (e - 3.3)), lw, zz)            # dead line
    # discs: a few played ones on the far triangle, the rest in a row at the near end
    spots = [(-0.3, e - 1.2, 0), (0.25, e - 1.75, 1), (0.05, e - 0.8, 0), (-0.5, -e + 0.25, 1), (-0.3, -e + 0.25, 1),
             (-0.1, -e + 0.25, 1), (0.3, -e + 0.25, 0), (0.5, -e + 0.25, 0)]
    for (dx, dy, c) in spots:
        m = RED if c == 0 else "CT_PlasterYellow"
        p.cyl(m, dx, dy, 0.0, 0.026, 0.076, 10)
        p.cyl(LAQ, dx, dy, 0.026, 0.028, 0.03, 6)
    for k, cx in enumerate((b + 0.3, b + 0.42)):
        y0, y1 = -e + 0.4 + 0.2 * k, -e + 2.0 + 0.2 * k
        p.rod(WOODL, (cx, y0, 0.02), (cx, y1, 0.02), 0.03, 5)
        for sx in (-0.07, 0.07):
            p.rod(WOODL, (cx, y1, 0.02), (cx + sx, y1 + 0.1, 0.02), 0.022, 4)
    return p.done(ends=[(0.0, -e - 0.6, 0.0), (0.0, e + 0.6, 0.0)], rack=(b + 0.36, -e + 1.2, 0.0),
                  footprint=(a, b, -e, e))


def muster_board(B, x, y, z=0.0, yaw=0.0, n=1, solid=False):
    """Muster station board 0.9x0.12x2.0 on two white posts: a red-framed board with the lifejacket instruction sheet,
    a big round number plate above it and a brass alarm bell under its little roof."""
    p = _P(B, x, y, z, yaw, "muster_board")
    for sx in (-0.4, 0.4):
        p.box(WHITE, sx - 0.025, sx + 0.025, -0.025, 0.025, 0.0, 1.85, f="xXyYZ")
    p.box(RED, -0.42, 0.42, -0.04, -0.025, 0.95, 1.8, f="xXyZz")
    p.box("CT_TileWhite", -0.38, 0.38, -0.044, -0.04, 0.99, 1.76, f="y")
    ph = min(0.62, kit._cell_h("poster_safety", 0.44))
    p.card_front("poster_safety", -0.34, 0.1, 1.38 - ph / 2, 1.38 + ph / 2, -0.048)
    p.card_front("paper_typed", 0.14, 0.34, 1.12, 1.4, -0.048)
    _disc(p, RED, 0.24, 1.6, 0.1, -0.048, 12)
    _disc(p, "CT_TileWhite", 0.24, 1.6, 0.075, -0.052, 12)
    for k in range(n):                                                           # the boat's number as tally bars
        p.box(LAQ, 0.228 + 0.03 * k - 0.015 * (n - 1), 0.252 + 0.03 * k - 0.015 * (n - 1), -0.056, -0.052, 1.55, 1.65, f="y")
    p.box(WHITE, -0.46, 0.46, -0.1, 0.06, 1.85, 1.88, f="xXyYZz")
    p.lathe(BRASS, -0.52, -0.02, [(0.07, 1.6), (0.06, 1.68), (0.03, 1.73), (0.0, 1.74)], 8, bottom=True)
    p.rod(STEEL, (-0.4, -0.02, 1.76), (-0.52, -0.02, 1.76), 0.014, 3)
    p.rod(STEEL, (-0.52, -0.02, 1.76), (-0.52, -0.02, 1.73), 0.014, 3)
    return p.done(front=0.8)


def striped_deck_chair(B, x, y, z=0.0, yaw=0.0, k=0, solid=False):
    """The kit's deck chair in one of the ship's canvas stripes (k picks it)."""
    mat, mat2 = (("CT_PlasterBlue", "CT_TileWhite"), ("CT_PlasterRed", "CT_TileWhite"), ("CT_PlasterYellow", "CT_TileWhite"),
                 ("CT_PlasterGreen", "CT_TileWhite"))[k % 4]
    return kit.deck_chair(B, x, y, z, yaw, mat=mat, mat2=mat2, frame=WOODL)


# ================================================================================================ radio room pieces
def transmitter_rack(B, x, y, z=0.0, yaw=0.0, solid=True):
    """[wall] Ship's radio station 1.1x0.45x1.75: a grey steel rack with the transmitter (meters, big tuning knobs, a
    control panel), the receiver with its lit dial under it and a loudspeaker grille."""
    p = _P(B, x, y, z, yaw, "transmitter_rack")
    p.kind = "wall"
    hw, d = 0.55, 0.45
    p.box("CT_PlasterGrey", -hw, hw, -d, 0.0, 0.0, 1.75, f="xXyZ")
    p.box(STEEL, -hw + 0.03, hw - 0.03, -d - 0.006, -d, 0.95, 1.7, f="xXyZz")
    ch = min(0.5, kit._cell_h("panel_control", 0.6))
    p.card_front("panel_control", -hw + 0.06, -hw + 0.66, 1.66 - ch, 1.66, -d - 0.01)
    for k in range(3):
        cz = 1.56 - 0.2 * k
        _disc(p, PAPER, hw - 0.2, cz, 0.065, -d - 0.008, 10)
        _hand(p, LAQ, hw - 0.2, cz - 0.03, 0.07, -d - 0.011, -40 + 35 * k, 0.006)
    for k in range(4):
        kit._knob(p, LAQ, -hw + 0.14 + 0.2 * k, 1.04, 0.04, -d - 0.04, -d - 0.006, 8)
    p.box(STEEL, -hw + 0.03, hw - 0.03, -d - 0.006, -d, 0.5, 0.9, f="xXyZz")
    p.card_front("dial_radio", -hw + 0.08, 0.0, 0.62, 0.84, -d - 0.01)
    p.card_front("grille", 0.08, hw - 0.08, 0.56, 0.86, -d - 0.01)
    for k in range(2):
        kit._knob(p, LAQ, -hw + 0.14 + 0.2 * k, 0.56, 0.028, -d - 0.035, -d - 0.006, 8)
    for k in range(6):
        p.box(LAQ, -hw + 0.08, hw - 0.08, -d - 0.002, -d, 0.1 + 0.055 * k, 0.125 + 0.055 * k, f="y")
    p.core(-hw + 0.02, hw - 0.02, -d + 0.02, -0.02, 0.1, 1.7)
    return p.done(solid=solid, top=1.75, front=0.6)


def morse_key(B, x, y, z=0.0, yaw=0.0, solid=False):
    """[prop] Morse key 0.09x0.16x0.07 on a dark base with headphones lying beside it and a message pad."""
    p = _P(B, x, y, z, yaw, "morse_key")
    p.kind = "prop"
    p.box(LAQ, -0.045, 0.045, -0.08, 0.08, 0.0, 0.016, f="xXyYZ")
    p.rod(BRASS, (0.0, 0.07, 0.04), (0.0, -0.07, 0.045), 0.014, 4)
    for sy in (0.03, 0.065):
        p.box(BRASS, -0.02, 0.02, sy - 0.008, sy + 0.008, 0.016, 0.05, f="xXyYZ")
    p.cyl(LAQ, 0.0, -0.075, 0.045, 0.07, 0.02, 8)
    # headphones
    hx = 0.26
    for sx in (-0.07, 0.07):
        _ax(p, LAQ, (0.0, 0.0, 0.045), "x", hx + sx - 0.015, hx + sx + 0.015, 0.042, segs=8)
    for (a, b) in (((hx - 0.07, 0.0, 0.085), (hx - 0.04, 0.0, 0.125)), ((hx - 0.04, 0.0, 0.125), (hx + 0.04, 0.0, 0.125)),
                   ((hx + 0.04, 0.0, 0.125), (hx + 0.07, 0.0, 0.085))):
        p.rod(STEEL, a, b, 0.012, 3)
    p.card_top("paper_typed", -0.33, -0.13, -0.12, 0.14, 0.003)
    return p.done()


def clock_24h(B, x, y, z=0.0, yaw=0.0, zc=1.9, r=0.19, solid=False):
    """[wall] The radio room's clock: a brass case, a white dial with the two RED three-minute silence sectors (minutes
    15-18 and 45-48: listen on 500 kHz) and the two green ones (00-03, 30-33), hour ticks, three hands."""
    p = _P(B, x, y, z, yaw, "clock_24h")
    p.kind = "wall"
    _ax(p, BRASS, (0.0, 0.0, zc), "y", -0.06, 0.0, r, segs=16, caps=False)
    _disc(p, BRASS, 0.0, zc, r, -0.06, 16, a0=math.pi / 16)
    _disc(p, PAPER, 0.0, zc, r * 0.9, -0.0615, 16, a0=math.pi / 16)
    for a0 in (90, 270):
        _sector(p, RED, 0.0, zc, r * 0.88, -0.063, a0, a0 + 18, n=2, r0=r * 0.2)
    for a0 in (0, 180):
        _sector(p, "CT_PlasterGreen", 0.0, zc, r * 0.88, -0.063, a0, a0 + 18, n=2, r0=r * 0.2)
    for k in range(12):
        _sector(p, LAQ, 0.0, zc, r * 0.88, -0.0635, k * 30 - 1.5, k * 30 + 1.5, n=1, r0=r * 0.74)
    _hand(p, LAQ, 0.0, zc, r * 0.5, -0.066, 305, 0.012)
    _hand(p, LAQ, 0.0, zc, r * 0.78, -0.069, 62, 0.009)
    _hand(p, RED, 0.0, zc, r * 0.82, -0.072, 200, 0.005)
    kit._knob(p, BRASS, 0.0, zc, 0.014, -0.078, -0.06, 6)
    return p.done()


# ================================================================================================ bridge pieces
def ships_wheel(B, x, y, z=0.0, yaw=0.0, solid=True):
    """Wooden steering wheel d1.0 on its pedestal (0.5x0.55x1.5): eight turned spokes with handles, a brass hub and
    rim band, the rudder-angle indicator on top of the column. The helmsman stands at -y ('stand') looking +y."""
    p = _P(B, x, y, z, yaw, "ships_wheel")
    p.lathe(WOOD, 0, 0.12, [(0.2, 0.0), (0.2, 0.06), (0.13, 0.1), (0.11, 0.8), (0.14, 0.86), (0.14, 1.12), (0.1, 1.16)], 8)
    p.lathe(BRASS, 0, 0.12, [(0.205, 0.06), (0.205, 0.075), (0.135, 0.115)], 8, top=False)
    zc, yw = 1.0, -0.1
    _torus(p, WOOD, (0.0, yw, zc), 0.36, 0.028, "y", 16, 5)
    _torus(p, BRASS, (0.0, yw - 0.03, zc), 0.36, 0.008, "y", 16, 3)
    for k in range(8):
        a = 2 * math.pi * k / 8 + math.pi / 8
        ca, sa = math.cos(a), math.sin(a)
        _post_xz(p, WOOD, (0.07 * ca, yw, zc + 0.07 * sa), (0.34 * ca, yw, zc + 0.34 * sa), 0.022, 0.018)
        _post_xz(p, WOOD, (0.385 * ca, yw, zc + 0.385 * sa), (0.5 * ca, yw, zc + 0.5 * sa), 0.014, 0.022)
    _ax(p, BRASS, (0.0, 0.0, zc), "y", yw - 0.05, yw + 0.04, 0.075, segs=10)
    _ax(p, STEEL, (0.0, 0.0, zc), "y", yw + 0.04, 0.02, 0.03, segs=6, caps=False)
    # rudder-angle indicator: a brass drum with a white arc dial facing the helmsman
    _ax(p, BRASS, (0.0, 0.0, 1.3), "y", 0.04, 0.16, 0.12, segs=12)
    p.box(BRASS, -0.04, 0.04, 0.07, 0.15, 1.14, 1.2, f="xXyY")
    _disc(p, PAPER, 0.0, 1.3, 0.1, 0.0385, 12)
    _sector(p, "CT_PlasterGreen", 0.0, 1.3, 0.095, 0.037, 0, 60, n=3, r0=0.07)
    _sector(p, RED, 0.0, 1.3, 0.095, 0.037, -60, 0, n=3, r0=0.07)
    _hand(p, LAQ, 0.0, 1.3, 0.085, 0.034, 8, 0.008)
    p.box(WOODL, -0.4, 0.4, -0.75, -0.25, 0.0, 0.03, f="xXyYZ")                  # the helmsman's grating
    for k in range(5):
        p.box(WOOD, -0.4, 0.4, -0.72 + k * 0.1, -0.69 + k * 0.1, 0.03, 0.036, f="xXyYZ")
    p.core(-0.07, 0.07, 0.05, 0.19, 0.12, 0.8)
    return p.done(solid=solid, front=0.5, stand=(0.0, -0.5, 0.0), hub=(0.0, yw, zc), footprint=(-0.4, 0.4, -0.2, 0.3))


def _post_xz(p, mat, a, b, ra, rb, sides=5):
    """A tapered round bar between two points lying in a plane y = const (a wheel spoke)."""
    d = geom_norm((b[0] - a[0], 0.0, b[2] - a[2]))
    u = (0.0, 1.0, 0.0)
    v = (d[2], 0.0, -d[0])
    ra_ = [(a[0] + (u[0] * math.cos(t) + v[0] * math.sin(t)) * ra, a[1] + math.cos(t) * ra, a[2] + v[2] * math.sin(t) * ra)
           for t in [2 * math.pi * k / sides for k in range(sides)]]
    rb_ = [(b[0] + (u[0] * math.cos(t) + v[0] * math.sin(t)) * rb, b[1] + math.cos(t) * rb, b[2] + v[2] * math.sin(t) * rb)
           for t in [2 * math.pi * k / sides for k in range(sides)]]
    cen = ((a[0] + b[0]) / 2, a[1], (a[2] + b[2]) / 2)
    for k in range(sides):
        j = (k + 1) % sides
        kit._outf(p, mat, [ra_[k], ra_[j], rb_[j], rb_[k]], cen)
    kit._outf(p, mat, rb_, cen)


def geom_norm(v):
    n = math.sqrt(sum(c * c for c in v)) or 1.0
    return tuple(c / n for c in v)


def telegraph(B, x, y, z=0.0, yaw=0.0, solid=True):
    """Brass engine-order telegraph 0.42x0.3x1.3: a turned pedestal, a drum head with a dial on each side (sectors for
    ПОЛНЫЙ / СРЕДНИЙ / МАЛЫЙ / САМЫЙ МАЛЫЙ / СТОП / ТОВСЬ ahead and astern: the red СТОП at the top), the order handle
    with a wooden grip standing at «малый вперёд» and the engine room's reply pointer. Operated from -y."""
    p = _P(B, x, y, z, yaw, "telegraph")
    p.lathe(BRASS, 0, 0, [(0.17, 0.0), (0.17, 0.03), (0.09, 0.07), (0.065, 0.2), (0.055, 0.78), (0.08, 0.84), (0.05, 0.9)], 10,
            top=False)
    zc, r = 1.06, 0.2
    _ax(p, BRASS, (0.0, 0.0, zc), "x", -0.085, 0.085, r, segs=16, caps=False)
    def pt(rr, phi, xx):                                 # phi: degrees from straight up toward +y (ahead)
        t = math.radians(phi)
        return (xx, rr * math.sin(t), zc + rr * math.cos(t))
    for s in (-1, 1):
        ref = (0.0, 0.0, zc)
        kit._outf(p, BRASS, [pt(r, 360.0 * k / 16 + 11.25, s * 0.085) for k in range(16)], ref)
        kit._outf(p, PAPER, [pt(r * 0.9, 360.0 * k / 16 + 11.25, s * 0.0875) for k in range(16)], ref)
        for k in range(11):                               # 5 orders ahead, СТОП (red, top), 5 astern
            a0 = -148.5 + 27 * k
            if k != 5 and k % 2:
                continue
            q = [pt(r * 0.5, a0 + 1, s * 0.089), pt(r * 0.86, a0 + 1, s * 0.089), pt(r * 0.86, a0 + 26, s * 0.089),
                 pt(r * 0.5, a0 + 26, s * 0.089)]
            kit._outf(p, RED if k == 5 else "CT_PlasterGrey", q, ref)
        p.rod(RED, pt(0.0, 0, s * 0.092), pt(r * 0.8, 54, s * 0.092), 0.012, 3)        # the engine room's reply
        p.rod(BRASS, pt(0.0, 0, s * 0.105), pt(r + 0.1, 54, s * 0.105), 0.02, 4)       # the order handle's arm
        _ax(p, BRASS, (0.0, 0.0, zc), "x", min(s * 0.085, s * 0.115), max(s * 0.085, s * 0.115), 0.03, segs=8)
    hy, hz = pt(r + 0.1, 54, 0.0)[1:]
    p.rod(WOOD, (-0.13, hy, hz), (0.13, hy, hz), 0.035, 6, caps=True)
    p.core(-0.034, 0.034, -0.034, 0.034, 0.22, 0.76)
    return p.done(solid=solid, front=0.5, handle=(0.0, hy, hz), footprint=(-0.2, 0.2, -0.2, 0.2))


def binnacle(B, x, y, z=0.0, yaw=0.0, solid=True):
    """Magnetic compass binnacle d0.5 x1.35: an octagonal teak column, the brass bowl hood with its window toward the
    helmsman (-y), the two iron correcting spheres (red to port, green to starboard) on brass arms, a little lamp."""
    p = _P(B, x, y, z, yaw, "binnacle")
    p.lathe(WOOD, 0, 0, [(0.2, 0.0), (0.2, 0.05), (0.15, 0.09), (0.14, 0.95), (0.19, 1.0), (0.19, 1.04)], 8, a0=math.pi / 8)
    p.lathe(BRASS, 0, 0, [(0.19, 1.04), (0.2, 1.1), (0.17, 1.22), (0.1, 1.31), (0.03, 1.34), (0.0, 1.345)], 10)
    q = [(-0.07, -0.19, 1.1), (0.07, -0.19, 1.1), (0.06, -0.14, 1.24), (-0.06, -0.14, 1.24)]
    p.face(GLASS, q)
    p.cyl(PAPER, 0, 0, 1.085, 1.09, 0.13, 10)
    for s, m in ((-1, RED), (1, "CT_PlasterGreen")):
        p.rod(BRASS, (s * 0.14, 0.0, 0.98), (s * 0.3, 0.0, 0.98), 0.03, 4)
        _sphere(p, m, s * 0.3, 0.0, 1.0, 0.085, 8)
    p.box(BRASS, -0.03, 0.03, 0.14, 0.22, 1.08, 1.2, f="xXyYZz")
    p.box(BRASS, -0.05, 0.05, -0.155, -0.14, 0.5, 0.62, f="xXyZz")
    p.core(-0.1, 0.1, -0.1, 0.1, 0.1, 1.0)
    return p.done(solid=solid, front=0.5, footprint=(-0.2, 0.2, -0.2, 0.2))


def radar_display(B, x, y, z=0.0, yaw=0.0, solid=True):
    """Radar display unit 0.55x0.6x1.25 («Дон»): a grey cabinet with a slanted top, the round screen under a rubber
    viewing hood (a green sweep glows inside), range knobs and a bearing ring."""
    p = _P(B, x, y, z, yaw, "radar_display")
    hw = 0.275
    p.box("CT_PlasterGrey", -hw, hw, -0.25, 0.3, 0.0, 0.85, f="xXyY")
    p.hexa("CT_PlasterGrey", [(-hw, -0.25, 0.85), (hw, -0.25, 0.85), (hw, 0.3, 0.85), (-hw, 0.3, 0.85)],
           [(-hw, -0.25, 0.9), (hw, -0.25, 0.9), (hw, 0.3, 1.08), (-hw, 0.3, 1.08)])
    sl = 0.18 / 0.55

    def zt(yy):
        return 0.9 + sl * (yy + 0.25)
    cy = 0.04
    ring0 = [(0.2 * math.cos(2 * math.pi * k / 12), cy + 0.2 * math.sin(2 * math.pi * k / 12),
              zt(cy + 0.2 * math.sin(2 * math.pi * k / 12)) + 0.004) for k in range(12)]
    ring1 = [(0.15 * math.cos(2 * math.pi * k / 12), cy - 0.05 + 0.15 * math.sin(2 * math.pi * k / 12),
              zt(cy) + 0.17 + 0.02 * math.sin(2 * math.pi * k / 12)) for k in range(12)]
    p.loft(RUBBER, [ring0, ring1], top=False)
    scr = [(0.17 * math.cos(2 * math.pi * k / 12), cy + 0.17 * math.sin(2 * math.pi * k / 12),
            zt(cy + 0.17 * math.sin(2 * math.pi * k / 12)) + 0.008) for k in range(12)]
    p.face(LAQ, scr)
    p.face("CT_Neon", [(0.0, cy, zt(cy) + 0.012), (0.15, cy + 0.03, zt(cy + 0.03) + 0.012),
                       (0.13, cy + 0.08, zt(cy + 0.08) + 0.012)])
    for k in range(4):
        kx = -0.2 + 0.13 * k
        p.cyl(LAQ, kx, -0.2, zt(-0.2), zt(-0.2) + 0.03, 0.022, 6)
    p.box(STEEL, -hw + 0.04, hw - 0.04, -0.256, -0.25, 0.2, 0.7, f="xXyZz")
    p.box(LAQ, -hw - 0.01, hw + 0.01, -0.26, 0.31, 0.0, 0.06, f="xXyY")
    p.core(-hw + 0.03, hw - 0.03, -0.2, 0.25, 0.1, 0.8)
    return p.done(solid=solid, front=0.5, top=1.25)


def voice_pipes(B, x, y, z=0.0, yaw=0.0, n=2, solid=False):
    """[wall] n brass voice pipes coming up the bulkhead and bending out to flared mouthpieces with whistle plugs on
    chains; engraved plates under them (engine room, captain)."""
    p = _P(B, x, y, z, yaw, "voice_pipes")
    p.kind = "wall"
    for k in range(n):
        sx = (k - (n - 1) / 2) * 0.2
        zt = 1.45 - 0.06 * (k % 2)
        p.cyl(BRASS, sx, -0.05, 0.0, zt, 0.025, 6, top=False)
        p.rod(BRASS, (sx, -0.05, zt - 0.01), (sx, -0.16, zt + 0.08), 0.05, 6)
        ring0 = [(sx + 0.03 * math.cos(2 * math.pi * j / 8), -0.16, zt + 0.08 + 0.03 * math.sin(2 * math.pi * j / 8)) for j in range(8)]
        ring1 = [(sx + 0.06 * math.cos(2 * math.pi * j / 8), -0.215, zt + 0.125 + 0.06 * math.sin(2 * math.pi * j / 8)) for j in range(8)]
        for j in range(8):
            jj = (j + 1) % 8
            kit._outf(p, BRASS, [ring0[j], ring0[jj], ring1[jj], ring1[j]], (sx, -0.3, zt + 0.2))
        kit._outf(p, WOOD, [(px, py + 0.012, pz - 0.01) for px, py, pz in ring1], (sx, 0.0, zt))
        p.box(BRASS, sx - 0.06, sx + 0.06, -0.006, 0.0, zt - 0.32, zt - 0.26, f="xXyZz")
        for zz in (0.4, 1.0):
            p.box(BRASS, sx - 0.04, sx + 0.04, -0.08, 0.0, zz, zz + 0.02, f="xXyZz")
    return p.done(front=0.5)


def flag_locker(B, x, y, z=0.0, yaw=0.0, w=1.2, zb=1.05, cols=8, rows=4, solid=False):
    """[wall] Signal flags locker w x0.26x0.72 hung at zb: varnished pigeon-holes with the rolled flags of the
    international code (colour ends showing) and a brass rail of lettered plates along the top."""
    p = _P(B, x, y, z, yaw, "flag_locker")
    p.kind = "wall"
    hw, d, h = w / 2, 0.26, 0.72
    p.box(WOOD, -hw, hw, -d, 0.0, zb, zb + h, f="xXZz")
    p.box(LAQ, -hw + 0.02, hw - 0.02, -0.02, -0.018, zb + 0.02, zb + h - 0.02, f="y")
    cw, ch = (w - 0.04) / cols, (h - 0.04) / rows
    for c in range(cols + 1):
        xx = -hw + 0.02 + c * cw
        p.box(WOOD, xx - 0.008, xx + 0.008, -d, -0.02, zb + 0.02, zb + h - 0.02, f="xXy")
    for r_ in range(rows + 1):
        zz = zb + 0.02 + r_ * ch
        p.box(WOOD, -hw + 0.02, hw - 0.02, -d, -0.02, zz - 0.008, zz + 0.008, f="yZz")
    cols_ = [RED, "CT_PlasterYellow", "CT_PlasterBlue", "CT_TileWhite", RED, "CT_PlasterBlue", "CT_PlasterYellow", LAQ]
    for r_ in range(rows):
        for c in range(cols):
            if (r_ * 5 + c * 3) % 11 == 0:
                continue                                                         # an empty hole: that flag is flying
            cx, cz = -hw + 0.02 + (c + 0.5) * cw, zb + 0.02 + (r_ + 0.5) * ch
            m1, m2 = cols_[(r_ * 3 + c) % 8], cols_[(r_ + c * 5 + 2) % 8]
            p.box(m1, cx - cw * 0.3, cx, -d + 0.03, -0.03, cz - ch * 0.28, cz + ch * 0.28, f="xXyZ")
            p.box(m2, cx, cx + cw * 0.3, -d + 0.03, -0.03, cz - ch * 0.28, cz + ch * 0.28, f="xXyZ")
    p.box(BRASS, -hw, hw, -d - 0.006, -d, zb + h - 0.03, zb + h, f="xXyZz")
    return p.done(front=0.5)


def chart_table(B, x, y, z=0.0, yaw=0.0, w=1.7, d=0.95, solid=True):
    """Chart table w x d x1.0: a chest of wide flat chart drawers with brass pulls, a fiddled top with the coast chart
    pinned out, parallel rulers, dividers, a pencil, the log book, the chronometer in its open gimballed box and a
    brass lamp on an arm. 'chart' = the middle of the chart, 'chrono' = the chronometer."""
    p = _P(B, x, y, z, yaw, "chart_table")
    hw, hd, h = w / 2, d / 2, 0.98
    p.box(WOOD, -hw, hw, -hd, hd, 0.08, h - 0.03, f="xXyY")
    p.box(LAQ, -hw + 0.04, hw - 0.04, -hd + 0.04, hd - 0.04, 0.0, 0.08, f="xXyY")
    p.bev(WOODL, -hw - 0.02, hw + 0.02, -hd - 0.03, hd + 0.01, h - 0.03, h, 0.008, bottom=True)
    for (x0, x1, y0, y1) in ((-hw - 0.02, hw + 0.02, hd - 0.012, hd + 0.01), (-hw - 0.02, -hw, -hd - 0.03, hd),
                             (hw, hw + 0.02, -hd - 0.03, hd)):
        p.box(WOOD, x0, x1, y0, y1, h, h + 0.025, f="xXyYZ")
    for k in range(5):                                                           # the drawers
        z0 = 0.12 + k * 0.165
        p.box(WOODL, -hw + 0.04, hw - 0.04, -hd - 0.008, -hd, z0, z0 + 0.145, f="xXyZz")
        for sx in (-hw * 0.5, hw * 0.5):
            p.box(BRASS, sx - 0.06, sx + 0.06, -hd - 0.024, -hd - 0.008, z0 + 0.06, z0 + 0.08, f="xXyZz")
    mh = min(d - 0.16, kit._cell_h("map_coast", w * 0.62))
    mw = mh / max(1e-6, kit._cell_h("map_coast", 1.0))
    cx = -hw + 0.1 + mw / 2
    p.card_top("map_coast", cx - mw / 2, cx + mw / 2, -mh / 2 - 0.02, mh / 2 - 0.02, h + 0.002)
    # parallel rulers (two ebony bars linked by brass arms), dividers, pencil
    for dy in (0.0, 0.07):
        p.box(LAQ, cx - 0.2, cx + 0.2, -0.18 + dy, -0.14 + dy, h + 0.004, h + 0.01, f="xXyYZ")
    for sx in (-0.12, 0.12):
        p.rod(BRASS, (cx + sx - 0.02, -0.16, h + 0.012), (cx + sx + 0.02, -0.09, h + 0.012), 0.008, 3)
    p.rod(BRASS, (cx + 0.22, 0.12, h + 0.008), (cx + 0.1, 0.2, h + 0.008), 0.008, 3)
    p.rod(BRASS, (cx + 0.22, 0.12, h + 0.008), (cx + 0.13, 0.24, h + 0.008), 0.008, 3)
    p.rod("CT_PlasterYellow", (cx - 0.3, 0.2, h + 0.006), (cx - 0.16, 0.26, h + 0.006), 0.008, 3)
    # log book, chronometer box, lamp
    bx = hw - 0.42
    p.box("CT_Leather", bx - 0.13, bx + 0.13, -hd + 0.06, -hd + 0.4, h, h + 0.03, f="xXyYZ")
    p.card_top("paper_typed", bx - 0.11, bx + 0.11, -hd + 0.08, -hd + 0.38, h + 0.032)
    kx, ky = hw - 0.18, 0.12
    p.box(WOOD, kx - 0.11, kx + 0.11, ky - 0.11, ky + 0.11, h, h + 0.12, f="xXyY")
    p.box(BAIZE, kx - 0.095, kx + 0.095, ky - 0.095, ky + 0.095, h + 0.05, h + 0.052, f="Z")
    p.cyl(BRASS, kx, ky, h + 0.052, h + 0.11, 0.075, 12, top=False)
    p.cyl(PAPER, kx, ky, h + 0.1, h + 0.105, 0.068, 12)
    p.rod(LAQ, (kx, ky, h + 0.107), (kx + 0.04, ky + 0.03, h + 0.107), 0.006, 3)
    p.rod(LAQ, (kx, ky, h + 0.107), (kx - 0.02, ky + 0.05, h + 0.107), 0.005, 3)
    p.slab(WOOD, (kx - 0.11, ky + 0.11, h + 0.12), (kx + 0.11, ky + 0.11, h + 0.12), (kx + 0.11, ky + 0.16, h + 0.33),
           (kx - 0.11, ky + 0.16, h + 0.33), 0.012)
    lx = -hw + 0.12
    p.cyl(BRASS, lx, hd - 0.1, h, h + 0.02, 0.05, 6)
    p.rod(BRASS, (lx, hd - 0.1, h), (lx, hd - 0.1, h + 0.42), 0.014, 4)
    p.rod(BRASS, (lx, hd - 0.1, h + 0.42), (lx + 0.3, hd - 0.3, h + 0.36), 0.014, 4)
    p.lathe(BRASS, lx + 0.3, hd - 0.3, [(0.085, h + 0.27), (0.05, h + 0.36), (0.02, h + 0.37)], 8)
    p.cyl(OPAL, lx + 0.3, hd - 0.3, h + 0.265, h + 0.27, 0.07, 8, top=False, bottom=True)
    p.core(-hw + 0.03, hw - 0.03, -hd + 0.03, hd - 0.03, 0.1, h - 0.04)
    return p.done(solid=solid, top=h, front=0.55, chart=(cx, -0.02, h), chrono=(kx, ky, h + 0.1), lamp=(lx + 0.3, hd - 0.3, h + 0.25))


def round_deck_table(B, x, y, z=0.0, yaw=0.0, r=0.4, solid=True):
    """Fixed round deck table d0.8 x0.72: white cast pedestal bolted to the deck, varnished slatted top with a brass rim."""
    p = _P(B, x, y, z, yaw, "round_deck_table")
    p.lathe(WHITE, 0, 0, [(0.2, 0.0), (0.2, 0.02), (0.05, 0.07), (0.04, 0.62), (0.12, 0.68)], 8, top=False)
    p.cyl(WOODL, 0, 0, 0.68, 0.715, r, 14, bottom=True)
    p.cyl(BRASS, 0, 0, 0.685, 0.722, r + 0.008, 14, top=False)
    for k in range(-3, 4):
        hx = math.sqrt(max(0.0, (r - 0.03) ** 2 - (k * 0.1) ** 2))
        p.box(WOOD, -hx, hx, k * 0.1 - 0.004, k * 0.1 + 0.004, 0.7155, 0.717, f="Z")
    p.core(-r * 0.6, r * 0.6, -r * 0.6, r * 0.6, 0.685, 0.71)
    return p.done(solid=solid, top=0.715, front=0.6)


def whistle_pull(B, x, y, ceiling, z=0.0, yaw=0.0, drop=0.75, solid=False):
    """[ceiling hung] The whistle pull: a cord from a brass eye in the deckhead down to a turned wooden toggle."""
    p = _P(B, x, y, ceiling, yaw, "whistle_pull")
    p.kind = "prop"
    p.cyl(BRASS, 0, 0, -0.03, 0.0, 0.03, 6, top=False, bottom=True)
    p.cyl(WOODL, 0, 0, -drop + 0.09, -0.03, 0.006, 4, top=False)
    _ax(p, WOOD, (0.0, 0.0, -drop + 0.07), "x", -0.07, 0.07, 0.018, segs=6)
    p.cyl(BRASS, 0, 0, -drop + 0.06, -drop + 0.1, 0.012, 5)
    return p.done(grip=(0.0, 0.0, -drop + 0.07))


def pelorus(B, x, y, z=0.0, yaw=0.0, solid=True):
    """Bearing repeater on a bridge wing, d0.3 x1.3: a white column, a brass bowl with a compass card and the sighting
    vane across it."""
    p = _P(B, x, y, z, yaw, "pelorus")
    p.lathe(WHITE, 0, 0, [(0.14, 0.0), (0.14, 0.03), (0.06, 0.07), (0.05, 1.05), (0.1, 1.1)], 8, top=False)
    p.lathe(BRASS, 0, 0, [(0.1, 1.1), (0.15, 1.16), (0.15, 1.24)], 10, top=False)
    p.cyl(PAPER, 0, 0, 1.2, 1.225, 0.14, 10)
    p.cyl(LAQ, 0, 0, 1.225, 1.228, 0.05, 8)
    p.box(BRASS, -0.15, 0.15, -0.012, 0.012, 1.24, 1.25, f="xXyYZz")
    for sx in (-0.14, 0.14):
        p.box(BRASS, sx - 0.006, sx + 0.006, -0.012, 0.012, 1.25, 1.36, f="xXyYZ")
    p.core(-0.03, 0.03, -0.03, 0.03, 0.1, 1.05)
    return p.done(solid=solid, top=1.25, front=0.5)


def soda_siphon(B, x, y, z=0.0, yaw=0.0, solid=False):
    """[prop] Soda siphon d0.1 x0.32: a blue glass bottle in a mesh, chrome head and lever."""
    p = _P(B, x, y, z, yaw, "soda_siphon")
    p.kind = "prop"
    p.lathe("CT_PlasterBlue", 0, 0, [(0.045, 0.0), (0.05, 0.02), (0.05, 0.17), (0.03, 0.22), (0.02, 0.25)], 8, top=False,
            bottom=True)
    p.lathe(ALU, 0, 0, [(0.022, 0.25), (0.028, 0.27), (0.022, 0.3), (0.0, 0.31)], 6)
    p.rod(ALU, (0.0, 0.0, 0.28), (0.0, -0.07, 0.27), 0.014, 4)
    p.rod(ALU, (0.0, 0.0, 0.29), (0.0, 0.06, 0.23), 0.012, 4)
    return p.done()


# ================================================================================================ the layout
class _K:
    """Ship-frame placing: positions (x, y, z ship) and headings (FWD 0, STBD 90, AFT 180, PORT 270) through S."""

    def __init__(self, B, S):
        self.B, self.S = B, S
        self.n_path = {}

    def P(self, x, y, z=0.0):
        return self.S.p(x, y, z)

    def Y(self, h):
        return self.S.yaw(h) % 360.0

    def H(self, kit_yaw):
        """Kit yaw -> ship heading."""
        return (90.0 - kit_yaw) % 360.0

    def put(self, fn, x, y, z, hdg=0.0, **kw):
        bx, by, bz = self.P(x, y, z)
        return fn(self.B, bx, by, bz, self.Y(hdg), **kw)

    def thing(self, tid, kind, x, y, z, h=0.0, **props):
        self.B.thing(tid, kind, self.P(x, y, z), self.Y(h), **props)

    def thing_at(self, tid, kind, pos, kit_yaw=0.0, dz=0.0, **props):
        self.B.thing(tid, kind, (pos[0], pos[1], pos[2] + dz), kit_yaw % 360.0, **props)

    def spot(self, sid, kind, x, y, z, h=0.0):
        self.B.spot(sid, kind, self.P(x, y, z), self.Y(h))

    def spot_at(self, sid, kind, pos, kit_yaw=0.0):
        self.B.spot(sid, kind, (pos[0], pos[1], pos[2]), kit_yaw % 360.0)

    def seat(self, tid, r, k=0, floor=None, kind="seat", spot_kind="seat", **props):
        """A seat thing + its people-spot from the k-th seat point of a kit piece (thing at the seat, spot on the floor)."""
        sx, sy, sz, syaw = r["seats"][k]
        fz = r["pos"][2] - r["size"][2] / 2 if floor is None else floor
        self.B.thing(tid, kind, (sx, sy, sz), syaw, height=round(sz - fz, 3), **props)
        self.B.spot(tid, spot_kind, (sx, sy, fz), syaw)

    def path(self, name, pts, z):
        """Path spots <name>_<n> along a polyline (ship x, y), heading along the way."""
        for i, (x, y) in enumerate(pts):
            j = min(i + 1, len(pts) - 1)
            i0 = j - 1 if j > 0 else 0
            h = math.degrees(math.atan2(pts[j][1] - pts[i0][1], pts[j][0] - pts[i0][0])) if j != i0 else 0.0
            self.spot("%s_%d" % (name, i + 1), "path", x, y, z, h % 360.0)

    def lamp(self, x, y, z, **kw):
        """No real lights here: the shell placed the ship's 24 (motor_ship.lamps). Kept as a no-op so the wished-for
        lamps stay written down next to their rooms (W3_report lists them)."""
        return None


def _saloon_half(x, inset=0.0):
    """Inner half-breadth of the music saloon at ship x (straight sides to x 42.5, then the semi-ellipse to x 45)."""
    b, a = 3.35 - inset, 2.5 - 0.15 - inset
    if x <= 42.5:
        return b
    t = (x - 42.5) / a
    return b * math.sqrt(max(0.0, 1.0 - t * t)) if t < 1.0 else 0.0


# Door holes and windows of the shell on these decks (ship frame). Defaults follow R1 / _shipframe.ROOMS; the shell's own
# tables (S.DOORS / S.WINDOWS, when _shipframe has them) win: see _shell().
CFG = {
    "lux_doors": {11: (12.35, 13.45), 12: (12.35, 13.45), 13: (15.45, 16.55), 14: (15.45, 16.55)},   # x spans, walls y +-0.75
    "lux11_windows_x": (13.0, 14.4),                        # 0.9 x 0.9, sill 0.9; one more in the aft wall at y 2.1
    "saloon_win_x": (34.0, 35.5, 39.2, 40.6, 41.9),         # side windows 1.1 wide, sill 0.85, head 1.85
    "saloon_door_x": (37.0, 38.2),
    "lobby_door_x": (31.6, 32.8),
    "radio_door": (-0.75, 34.5, 35.6),                      # the wall's y, x0, x1 (an open hole)
    "captain_door": (38.92, 0.0),                           # the leaf's face (looks aft)
    "boat1": (21.0, 28.5, 1),                               # x0, x1, side (+1 starboard): the forward starboard boat
}


def _shell(S):
    c = dict(CFG)
    c.update(getattr(S, "BA", {}) or {})
    return c


def furnish_ba(B, S):
    """Furniture, things and people-spots of decks B, A and the bridge (the contract of DESIGN.md)."""
    K = _K(B, S)
    C = _shell(S)
    zb, za, zr = S.Z["B"], S.Z["A"], S.Z["BR"]
    _saloon(K, C, zb, S.ceil("B") - zb)
    _lobby(K, C, zb, S.ceil("B") - zb)
    _lux(K, C, zb)
    _promenades(K, C, zb, S)
    _smoking_deck(K, C, zb, S)
    _boat_deck(K, C, za, S)
    _radio_room(K, C, za)
    _bridge(K, C, zr, S.ceil("BR") - zr)
    _marks(K, C, S)


# ------------------------------------------------------------------------------------------------ music saloon
def _saloon(K, C, z, ch):
    B = K.B
    wall = 3.35
    # ---- the bar: L-counter port aft, mirror back-bar on the port wall, five stools
    long_ = K.put(kit.bar_counter, 35.15, -1.975, z, STBD, w=2.8, d=0.55, taps=0, back=False, mat=WOOD, top="CT_Copper",
                  front="CT_Leather")
    K.put(kit.bar_counter, 36.275, -2.8, z, FWD, w=1.1, d=0.55, taps=0, back=False, mat=WOOD, top="CT_Copper",
          front="CT_Leather")
    bb = K.put(back_bar, 34.8, -wall, z, STBD, w=2.4)
    K.put(coffee_machine, 33.95, -wall + 0.19, z + 0.915, STBD)
    K.put(kit.cash_register, 35.75, -wall + 0.2, z + 0.915, STBD)
    top = z + 1.05 + 0.012
    K.put(kit.bottles, 36.3, -2.6, top, FWD, n=3, seed=3)
    K.put(kit.glass, 34.3, -1.95, top, STBD)
    K.put(kit.glass, 35.5, -1.9, top, STBD)
    K.put(kit.cup, 34.9, -1.9, top, STBD)
    K.put(soda_siphon, 35.05, -2.12, top, STBD)
    K.put(soda_siphon, 35.17, -2.1, top, STBD)
    K.put(kit.menu_board, 33.05, -2.75, z, FWD, w=0.5, zc=1.65)
    K.put(kit.rug, 35.45, -0.02, z + 0.002, FWD, w=1.3, d=3.6, name="rug")
    K.thing("bar_counter", "counter", 35.15, -1.975, z + 1.1, STBD, role="barman", service="bar")
    K.spot("staff_barman", "work", 35.0, -2.68, z, STBD)
    for i, sx in enumerate((34.05, 34.65, 35.25, 35.85, 36.4)):
        r = K.put(kit.bar_stool, sx, -1.28, z, PORT, mat="CT_Leather")
        K.seat("bar_stool_%d" % (i + 1), r, floor=K.P(0, 0, z)[2])
    K.spot("stand_bar_1", "stand", 33.5, -1.25, z, PORT)
    K.spot("stand_bar_2", "stand", 36.75, -0.6, z, PORT)
    del long_, bb

    # ---- the card table: green baize, a low brass lamp over it, five chairs
    cx, cy = 34.9, 2.2
    t = K.put(kit.table_round, cx, cy, z, FWD, r=0.65, mat=BAIZE, base=BRASS)
    K.put(card_things, cx, cy, z + 0.752, FWD)
    K.thing("card_table", "play", cx, cy, z + 0.8, PORT, what="poker")
    for i, ang in enumerate((270, 342, 54, 126, 198)):
        a = math.radians(ang)
        r = K.put(kit.chair, cx + 0.95 * math.cos(a), cy + 0.95 * math.sin(a), z, (ang + 180) % 360, kind="wood",
                  mat="CT_PlasterGreen")
        K.seat("card_chair_%d" % (i + 1), r, floor=K.P(0, 0, z)[2])
    K.put(kit.lamp_green, cx, cy, z + 0.752, PORT)          # the low brass lamp in the middle of the baize

    # ---- radiogram on the starboard wall, aft of the side door
    r = K.put(radiogram, 36.42, wall, z, PORT)
    K.thing_at("saloon_radiogram", "machine", r["deck"], r["yaw"], what="record_player")
    K.put(kit.books_stack, 36.75, wall - 0.2, z + 0.8, PORT, n=3, seed=2) if False else None

    # ---- the dance floor
    K.put(kit.parquet, 40.25, 0.0, z + 0.002, FWD, w=3.2, d=4.1)
    K.thing("dance_floor", "play", 40.25, 0.0, z + 0.15, AFT, what="dance")
    for i, (dx, dy) in enumerate(((-1.3, -0.8), (-1.3, 0.8), (-0.45, -0.25), (-0.45, 1.1), (0.45, -1.1), (0.45, 0.3),
                                  (1.3, -0.7), (1.3, 0.9))):
        K.spot("dance_%d" % (i + 1), "stand", 40.25 + dx, dy, z, (i * 135) % 360)

    # ---- 24 seats along the windows: two sofas a side, a row of tub chairs in front of them, two armchairs aft
    n = 0
    cols = ("CT_PlasterBlue", "CT_PlasterYellow")
    for s, h_in in ((1, PORT), (-1, STBD)):                # starboard seats first: saloon_seat_1.. on the starboard side
        for k, sx in enumerate((39.34, 41.51)):
            r = K.put(kit.sofa, sx, s * (wall - 0.42), z, h_in, seats=3, mat=cols[0] if s > 0 else "CT_PlasterRed")
            order = range(3) if True else ()
            for j in order:
                n += 1
                K.seat("saloon_seat_%d" % n, r, j, floor=K.P(0, 0, z)[2])
        for k, sx in enumerate((38.82, 39.55, 40.9, 41.63, 42.34)):
            r = K.put(kit.tub_chair, sx, s * 2.08, z, (FWD + s * -18) % 360, mat="CT_PlasterGreen" if k % 2 else "CT_Leather",
                      seat_mat=cols[1])
            n += 1
            K.seat("saloon_seat_%d" % n, r, floor=K.P(0, 0, z)[2])
        t = K.put(kit.table_round, 40.22, s * 2.1, z, FWD, r=0.27, h=0.5, mat=WOOD, base=BRASS)
        K.put(kit.cup, 40.2, s * 2.05, z + 0.5, FWD)
        r = K.put(kit.armchair, 37.2, s * 1.45, z, FWD, mat="CT_PlasterRed" if s > 0 else "CT_PlasterBlue")
        n += 1
        K.seat("saloon_seat_%d" % n, r, floor=K.P(0, 0, z)[2])
    K.put(kit.glass, 40.3, -2.15, z + 0.5, FWD)

    # ---- the bandstand in the curved front
    xs = (42.7, 43.2, 43.7, 44.1, 44.4, 44.64)
    half = [min(_saloon_half(x, 0.14), 3.2) for x in xs]
    cxp = 43.7
    ship = [(x, -h) for x, h in zip(xs, half)] + [(x, h) for x, h in reversed(list(zip(xs, half)))]
    # local frame of a piece facing AFT at (cxp, 0): local u = ship +y... taken from the helper, whatever its handedness
    o = K.P(cxp, 0.0, 0.0)
    ya = math.radians(K.Y(AFT))
    loc = []
    for (x, y) in ship:
        q = K.P(x, y, 0.0)
        dx, dy = q[0] - o[0], q[1] - o[1]
        loc.append((dx * math.cos(ya) + dy * math.sin(ya), -dx * math.sin(ya) + dy * math.cos(ya)))
    if sum(loc[i][0] * loc[(i + 1) % len(loc)][1] - loc[(i + 1) % len(loc)][0] * loc[i][1] for i in range(len(loc))) < 0:
        loc = loc[::-1]
    K.put(bandstand_floor, cxp, 0.0, z, AFT, pts=tuple(loc), h=0.2)
    zs = z + 0.2
    K.thing("bandstand", "screen", 42.72, 0.0, z + 1.0, AFT, what="band")
    # piano: its back to the port-forward curve, the pianist seen in profile from the room
    pr = K.put(kit.piano, 43.73, -2.58, zs, 135.0)
    K.thing_at("saloon_piano", "play", pr["pos"], pr["yaw"], what="piano")
    st = K.put(kit.stool, 43.06, -1.91, zs, 315.0, mat=LAQ, h=0.48)
    K.spot("band_piano", "work", 43.06, -1.91, zs, 315.0)
    dk = K.put(drum_kit, 43.95, 0.15, zs, AFT)
    K.spot_at("band_drums", "work", (dk["seat"][0], dk["seat"][1], K.P(0, 0, zs)[2]), K.Y(AFT))
    K.put(double_bass, 43.85, 1.95, zs, 205.0)
    K.spot("band_bass", "work", 43.4, 1.55, zs, AFT)
    K.put(reed_stand, 43.3, 0.95, zs, AFT)
    K.spot("band_reed", "work", 43.15, 0.45, zs, AFT)
    K.put(music_stand, 42.92, 0.62, zs, FWD)
    K.put(music_stand, 43.05, 1.95, zs, FWD)
    K.put(kit.mic_stand, 42.95, -0.45, zs, FWD)
    K.spot("bandstand", "mark", 43.5, 0.0, zs, AFT)
    K.spot("card_table", "mark", cx, cy, z, FWD)
    K.spot("staff_rimma", "work", 38.05, -0.45, z, FWD)
    del st

    # ---- walls: clock and barometer over the card table, pictures, opal wall lamps, curtains at the side windows
    ax = 33.05
    K.put(kit.wall_clock, ax, 1.55, z, FWD, zc=2.0, r=0.17, face="clock_1")
    K.thing("saloon_clock", "board", ax + 0.05, 1.55, z + 2.0, FWD, what="clock")
    r = K.put(barometer, ax, 2.25, z, FWD, zc=1.75)
    K.thing_at("saloon_barometer", "board", r["pos"], r["yaw"], what="barometer")
    K.put(kit.picture, ax, 2.95, z, FWD, name="painting_sea_calm", w=0.55, zc=1.7)
    K.put(kit.plant, 33.42, 3.0, z, FWD, kind="monstera", pot="CT_PlasterGreen", h=1.1)
    K.put(kit.picture, ax, -1.25, z, FWD, name="painting_lighthouse", w=0.5, zc=1.7)
    K.put(wall_sconce, ax, 1.0, z, FWD, zc=1.95)
    K.put(wall_sconce, ax, -0.98, z, FWD, zc=1.95)
    wins = C["saloon_win_x"]
    for s, h_in in ((1, PORT), (-1, STBD)):
        for wx in wins:
            if s < 0 and 33.5 < wx < 36.2:
                continue                                   # the back-bar stands there
            K.put(kit.curtains, wx, s * wall, z + 0.75, h_in, w=1.3, h=1.3, mat="CT_PlasterYellow", open_=0.7)
        mids = sorted(wins)
        for a, b in zip(mids, mids[1:]):
            mx = (a + b) / 2
            if C["saloon_door_x"][0] - 0.3 < mx < C["saloon_door_x"][1] + 0.3 or (s < 0 and mx < 36.4):
                continue
            K.put(wall_sconce, mx, s * wall, z, h_in, zc=1.95)
    # ---- the four real lights: over the bar, the card table (above), the dance floor, the bandstand
    K.lamp(35.2, -1.6, z + ch - 0.1, lumens=800, kind="public", radius=5.0)
    K.lamp(40.25, 0.0, z + ch - 0.1, lumens=1000, kind="public", radius=6.5)
    K.lamp(43.5, 0.0, z + ch - 0.1, lumens=700, colour="#FFE2B8", kind="public", radius=4.5)
    # paths: from the lobby door down the middle to the dance floor and out by the side doors
    K.path("path_saloon", [(33.6, 0.0), (36.0, 0.0), (37.9, 0.0), (37.8, -2.6), (37.8, 2.6)], z)


# ------------------------------------------------------------------------------------------------ stair lobby
def _lobby(K, C, z, ch):
    core = 1.55                                             # outer faces of the stair core walls
    # ---- library corner, port: two glazed cases on the stair core, armchairs under the windows, floor lamp, chess
    for i, (bx, topic, seed) in enumerate(((28.35, "sea", 11), (29.65, "classics", 12))):
        r = K.put(glazed_bookcase, bx, -core, z, PORT, w=1.2, topic=None, seed=seed)
        K.thing_at("library_case_%d" % (i + 1), "shelf", r["pos"], r["yaw"], topic=topic)
    for i, ax in enumerate((28.25, 29.45)):
        r = K.put(kit.armchair, ax, -2.93, z, STBD, mat="CT_Leather")
        K.seat("library_chair_%d" % (i + 1), r, floor=K.P(0, 0, z)[2])
    K.put(kit.floor_lamp, 28.85, -3.1, z, STBD, shade="CT_PlasterYellow")
    ct = K.put(kit.chess_table, 30.72, -2.85, z, STBD)
    K.thing_at("chess_table", "play", ct["pos"], ct["yaw"], dz=0.42, what="chess")
    for i, (sx, h) in enumerate(((30.15, FWD), (31.29, AFT))):
        r = K.put(kit.chair, sx, -2.85, z, h, kind="wood", mat="CT_Leather")
        K.seat("chess_seat_%d" % (i + 1), r, floor=K.P(0, 0, z)[2])
    g = K.put(kit.globe, 30.72, -1.82, z, PORT, floor=True)
    K.thing_at("library_globe", "board", g["pos"], g["yaw"], topic="globe")
    K.put(kit.rug, 28.85, -2.4, z + 0.002, FWD, w=1.0, d=2.4, name="rug")
    # ---- notice niche, starboard: the programme, the honour board, the model in its case
    r = K.put(programme_board, 28.3, core, z, STBD, zc=1.55)
    K.thing_at("programme_board", "board", r["pos"], r["yaw"], topic="programme")
    r = K.put(honour_board, 29.9, core, z, STBD, zc=1.55)
    K.thing_at("honour_board", "board", r["pos"], r["yaw"])
    r = K.put(ship_model_case, 32.95, 1.75, z, AFT)
    K.thing_at("ship_model", "board", r["pos"], r["yaw"], dz=0.25)
    K.put(wall_sconce, 29.1, core, z, STBD, zc=2.05)
    K.put(wall_sconce, 29.0, -core, z, PORT, zc=2.1) if False else None
    K.path("path_lobby", [(31.8, 0.0), (32.2, 2.6), (32.2, -2.6), (29.5, 2.5), (27.3, 2.45), (30.6, -2.2)], z)
    K.spot("stand_lobby_1", "stand", 28.9, 2.3, z, PORT)
    K.spot("stand_lobby_2", "stand", 31.9, 1.75, z, FWD)
    K.spot("stand_library_1", "stand", 28.95, -2.2, z, STBD)


# ------------------------------------------------------------------------------------------------ lux cabins
def _lux(K, C, z):
    B = K.B
    fz = K.P(0, 0, z)[2]
    x0, x1, y0, y1 = 12.15, 15.2, 0.8, 3.35
    # twin low beds, heads under the windows; a rug between the door and the beds
    ra = K.put(berth, x1 - 0.46, y1 - 0.96, z, PORT, blanket="CT_PlasterBlue")
    rb = K.put(berth, x1 - 1.56, y1 - 0.96, z, PORT, blanket="CT_PlasterBlue")
    for tag, r in (("a", ra), ("b", rb)):
        sx, sy, sz, syaw = r["seats"][0]
        B.thing("lux11_bed_%s" % tag, "bed", (sx, sy, sz), syaw, height=round(sz - fz, 3))
        B.spot("lux11_bed_%s" % tag, "bed", (sx, sy, fz), syaw)
    # writing desk under the aft window with its lamp, the chair, the washbasin by the door
    K.put(kit.table, x0 + 0.27, 2.72, z, FWD, w=1.05, d=0.5, mat=WOOD)
    K.put(kit.table_lamp, x0 + 0.2, 3.1, z + 0.75, FWD, shade="CT_PlasterYellow")
    K.put(kit.books_stack, x0 + 0.22, 2.42, z + 0.75, FWD, n=2, seed=4)
    K.put(kit.glass, x0 + 0.3, 2.78, z + 0.75, FWD)
    r = K.put(kit.chair, x0 + 0.72, 2.72, z, AFT, kind="wood", mat="CT_PlasterRed")
    K.seat("lux11_desk", r, floor=fz)
    wb = K.put(washbasin, x0, 1.4, z, FWD)
    K.thing_at("lux11_washbasin", "machine", wb["tap"], wb["yaw"], what="tap")
    wxs = C["lux11_windows_x"]
    for wx in wxs:
        K.put(kit.curtains, wx, y1, z + 0.82, PORT, w=1.05, h=1.1, mat="CT_PlasterGreen", open_=0.65)
    K.put(kit.curtains, x0, 2.1, z + 0.82, FWD, w=1.05, h=1.1, mat="CT_PlasterGreen", open_=0.65)
    K.thing("lux11_window", "machine", wxs[0], y1 - 0.06, z + 1.4, PORT, what="window")
    K.put(kit.rug, 12.95, 1.7, z + 0.002, FWD, w=0.9, d=1.3, name="rug")
    K.put(kit.picture, 14.3, y0, z, STBD, name="painting_sea_night", w=0.5, zc=1.65)
    K.put(wall_sconce, x1, 1.15, z, AFT, zc=1.85)
    # door things of the four lux cabins (the leaves are the shell's), berth spots behind the closed doors
    for n in (11, 12, 13, 14):
        a, b = C["lux_doors"][n]
        s = 1 if n % 2 else -1
        K.thing("cabin%d_door" % n, "door", (a + b) / 2, s * 0.67, z + 1.1, PORT if s > 0 else STBD, caption=str(n))
    for n, (cx1, s) in ((12, (15.2, -1)), (13, (18.45, 1)), (14, (18.45, -1))):
        K.spot("cabin%d_bed_a" % n, "bed", cx1 - 0.46, s * (y1 - 1.06), z, PORT if s > 0 else STBD)
        K.spot("cabin%d_bed_b" % n, "bed", cx1 - 1.56, s * (y1 - 1.06), z, PORT if s > 0 else STBD)
    K.path("path_b_aft", [(12.4, 0.0), (15.4, 0.0), (18.4, 0.0), (19.25, 0.0), (19.25, 2.6), (19.25, -2.6), (23.5, 2.45),
                          (26.6, 2.45)], z)


# ------------------------------------------------------------------------------------------------ side promenades
def _promenades(K, C, z, S):
    wall = 3.5
    n = 0
    for s, h_out in ((1, STBD), (-1, PORT)):
        for bx in (14.6, 23.6, 29.6):
            n += 1
            r = K.put(kit.bench, bx, s * (wall + 0.3), z, h_out, w=1.6, kind="park", mat=WOODL, frame=WHITE)
            mid = ((r["seats"][0][0] + r["seats"][1][0]) / 2, (r["seats"][0][1] + r["seats"][1][1]) / 2, r["seats"][0][2])
            K.B.thing("prom_bench_%d" % n, "seat", mid, r["yaw"], height=0.45)
            for tag, (sx, sy, sz, syaw) in zip("ab", r["seats"]):
                K.B.spot("prom_bench_%d_%s" % (n, tag), "bench", (sx, sy, K.P(0, 0, z)[2]), syaw)
        for k, lx in enumerate((12.9, 21.4, 27.4, 35.3, 41.2)):
            r = K.put(kit.lifebuoy, lx, s * wall, z, h_out, zc=1.45)
            if s > 0 and k == 2:
                K.thing_at("life_ring", "board", r["pos"], r["yaw"])
        K.lamp(27.0, s * 4.2, S.ceil("B") - 0.1, lumens=500, kind="public", radius=7.0)
        tag = "stbd" if s > 0 else "port"
        K.path("path_b_%s" % tag, [(x, s * 4.2) for x in (12.6, 17.0, 21.5, 26.0, 30.5, 35.0, 39.5, 42.3)], z)
        for k, rx in enumerate((16.6, 21.0, 26.0, 33.6, 39.6)):
            K.spot("rail_b_%s_%d" % (tag, k + 1), "stand", rx, s * 4.5, z, h_out)
    # forward observation walk
    K.path("path_b_bow", [(44.3, 3.9), (46.2, 2.3), (46.9, 0.0), (46.2, -2.3), (44.3, -3.9)], z)
    for k, (rx, ry, h) in enumerate(((47.2, 0.0, FWD), (46.75, 1.6, 30.0), (46.75, -1.6, 330.0))):
        K.spot("rail_b_bow_%d" % (k + 1), "stand", rx, ry, z, h)


# ------------------------------------------------------------------------------------------------ smoking deck
def _smoking_deck(K, C, z, S):
    fz = K.P(0, 0, z)[2]
    places = ((11.62, 2.25, AFT), (11.62, -2.25, AFT), (8.6, -2.72, STBD), (9.6, 4.25, PORT))
    for i, (bx, by, h) in enumerate(places):
        if i == 3:
            by = S.deck_half("B", bx) - 0.55
        r = K.put(kit.bench, bx, by, z, h, w=1.6, kind="park", mat=WOODL, frame=WHITE)
        mid = ((r["seats"][0][0] + r["seats"][1][0]) / 2, (r["seats"][0][1] + r["seats"][1][1]) / 2, r["seats"][0][2])
        K.B.thing("smoking_bench_%d" % (i + 1), "seat", mid, r["yaw"], height=0.45)
        for tag, (sx, sy, sz, syaw) in zip("ab", r["seats"]):
            K.B.spot("smoking_bench_%d_%s" % (i + 1, tag), "bench", (sx, sy, fz), syaw)
    for i, (ux, uy) in enumerate(((11.72, 1.1), (11.72, -1.1))):
        r = K.put(ash_urn, ux, uy, z, AFT)
        K.thing_at("ash_urn_%d" % (i + 1), "machine", r["pos"], r["yaw"], dz=0.4, what="ash_urn")
    for (tx, ty) in ((10.55, 2.25), (10.55, -2.0)):
        K.put(round_deck_table, tx, ty, z, FWD)
    K.put(kit.newspaper, 10.5, 2.3, z + 0.716, FWD) if hasattr(kit, "newspaper") else None
    r = K.put(ring_toss, 3.3, -1.75, z, FWD)
    K.thing("ring_toss", "play", 3.3 + 0.2, -1.75, z + 0.85, FWD, what="ring_toss")
    K.spot("ring_toss_throw", "stand", 5.8, -1.75, z, AFT)
    K.thing("stern_rail", "machine", 2.32, 0.4, z + 1.0, AFT, what="gull_feed")
    K.spot("stern_rail", "mark", 2.75, 0.4, z, AFT)
    for i, (rx, ry, h) in enumerate(((2.75, -0.6, AFT), (2.75, 1.1, AFT), (2.95, -2.6, 215.0), (5.0, -3.75, PORT),
                                     (8.6, 4.3, STBD), (10.9, 4.4, STBD))):
        if i >= 3:
            ry = math.copysign(S.deck_half("B", rx) - 0.5, ry)
        K.spot("rail_b_aft_%d" % (i + 1), "stand", rx, ry, z, h)
    K.spot("smoke_1", "stand", 9.3, 0.7, z, AFT)
    K.spot("smoke_2", "stand", 7.9, -1.0, z, 200.0)
    K.lamp(9.0, 0.0, S.ceil("B") - 0.1, lumens=450, kind="public", radius=6.0)
    K.path("path_b_stern", [(11.3, 0.0), (9.0, 0.3), (6.0, -0.6), (3.6, 0.3), (9.0, 3.6), (11.4, 4.2), (11.4, -4.45)], z)


# ------------------------------------------------------------------------------------------------ boat deck
def _boat_deck(K, C, z, S):
    fz = K.P(0, 0, z)[2]
    r = K.put(shuffleboard, 11.0, 0.0, z, FWD, w=1.8, d=8.0)
    K.thing("shuffleboard", "play", 11.0, 0.0, z + 0.1, AFT, what="shuffleboard")
    K.spot("shuffle_1", "stand", 6.75, 0.0, z, FWD)
    K.spot("shuffle_2", "stand", 15.6, 0.0, z, AFT)
    n = 0
    for s, xs in ((1, (8.3, 10.1, 11.9, 13.7)), (-1, (11.6, 13.2, 14.8, 16.4))):
        for cx in xs:
            n += 1
            rr = K.put(striped_deck_chair, cx, s * 2.45, z, PORT if s > 0 else STBD, k=n)
            sx, sy, sz, syaw = rr["seats"][0]
            K.B.thing("deck_chair_%d" % n, "bed", (sx, sy, sz), syaw, height=0.35)
            K.B.spot("deck_chair_%d" % n, "seat", (sx, sy, fz), syaw)
    for (cx, cy, h) in ((16.9, 1.7, AFT), (16.9, -1.9, AFT), (30.2, 2.6, STBD), (30.2, -2.6, PORT)):
        K.put(lifejacket_chest, cx, cy, z, h)
    for i, s in enumerate((1, -1)):
        rr = K.put(kit.bench, 23.5, s * 1.68, z, STBD if s > 0 else PORT, w=1.6, kind="park", mat=WOODL, frame=WHITE)
        mid = ((rr["seats"][0][0] + rr["seats"][1][0]) / 2, (rr["seats"][0][1] + rr["seats"][1][1]) / 2, rr["seats"][0][2])
        K.B.thing("funnel_bench_%d" % (i + 1), "seat", mid, rr["yaw"], height=0.45)
        for tag, (sx, sy, sz, syaw) in zip("ab", rr["seats"]):
            K.B.spot("funnel_bench_%d_%s" % (i + 1, tag), "bench", (sx, sy, fz), syaw)
    # the muster station by boat No. 1
    b0, b1, s = C["boat1"]
    mx = (b0 + b1) / 2
    K.thing("lifeboat_1", "board", mx, s * 3.75, z + 1.3, STBD if s > 0 else PORT)
    rr = K.put(muster_board, b1 + 0.75, s * 3.3, z, PORT if s > 0 else STBD, n=1)
    K.thing_at("muster_station", "board", rr["pos"], rr["yaw"], dz=0.45)
    for k in range(12):                                     # two ranks facing the boat, clear of the cowl vents
        mxk = b0 + 0.6 + (k % 6) * 1.1 if k < 6 else b0 + 0.9 + (k % 6) * 0.8
        K.spot("muster_%d" % (k + 1), "stand", mxk, s * (3.0 if k < 6 else 2.45), z, STBD if s > 0 else PORT)
    for k, (rx, s2) in enumerate(((7.4, 1), (9.5, 1), (19.6, 1), (19.6, -1), (33.95, 1), (33.95, -1))):
        hy = S.deck_half("A", rx) - 0.5
        K.spot("rail_a_%d" % (k + 1), "stand", rx, s2 * hy, z, STBD if s2 > 0 else PORT)
    for k, (ex, ey) in enumerate(((13.0, -0.3), (14.2, 1.0), (14.2, -1.6), (15.4, 0.2), (15.4, 1.5), (12.4, 1.2))):
        K.spot("exercise_%d" % (k + 1), "stand", ex, ey, z, AFT)
    K.spot("staff_rimma_deck", "work", 10.8, 0.3, z, FWD)
    K.path("path_a_stbd", [(7.2, 3.3), (10.5, 3.3), (19.5, 3.0), (20.6, 2.6), (26.5, 2.6), (30.0, 3.2), (33.6, 3.7)], z)
    K.path("path_a_port", [(11.3, -3.5), (16.0, -3.3), (19.5, -3.0), (20.6, -2.6), (26.5, -2.6), (30.0, -3.2), (33.6, -3.7)], z)
    K.path("path_a_mid", [(32.6, 2.3), (33.3, 0.0), (32.6, -2.3), (16.0, 0.0), (18.0, 2.4)], z)
    del r


def _radio_room(K, C, z):
    x0, x1, y0 = 34.15, 36.95, -2.85
    wy, da, db = C["radio_door"]
    # the operator's desk under the port windows, the station on the forward bulkhead, the clock between the windows
    K.put(kit.office_desk, 35.7, y0 + 0.37, z, STBD, w=1.5, d=0.7, typewriter_on=True, papers=True, phone=False)
    K.put(morse_key, 35.2, y0 + 0.45, z + 0.75, STBD)
    K.put(kit.chair, 35.7, y0 + 1.0, z, PORT, kind="wood", mat="CT_Leather")
    K.put(transmitter_rack, x1, -1.55, z, AFT)
    r = K.put(clock_24h, 35.8, y0, z, STBD, zc=2.08, r=0.16)
    K.thing_at("radio_clock", "board", r["pos"], r["yaw"], what="clock")
    K.put(kit.picture, x0, -1.75, z, FWD, name="map_world", w=0.9, zc=1.6)
    K.put(kit.filing_cabinet, x0, -2.5, z, FWD)
    K.spot("staff_radio", "work", 35.7, y0 + 1.0, z, PORT)
    mx = (da + db) / 2
    # the counter flap in the doorway: telegrams are handed in over it
    K.put(door_flap, mx, wy, z, STBD, w=db - da - 0.02)
    K.thing("radio_desk", "counter", mx, wy + 0.04, z + 1.12, STBD, role="radio", service="telegram")
    K.spot("stand_radio", "stand", mx, wy + 0.6, z, PORT)
    cx, cy = C["captain_door"]
    K.thing("captain_door", "door", cx - 0.02, cy, z + 1.1, AFT, caption="captain")
    K.path("path_a_house", [(33.7, 0.0), (35.05, 0.0), (38.3, 0.0)], z)


def door_flap(B, x, y, z=0.0, yaw=0.0, w=1.05, solid=True):
    """A counter flap across a doorway (w wide, 1.0 high, in the wall plane; its front = the customer's side): a
    panelled lower leaf, a brass-edged shelf, a desk bell and a pad of telegram forms."""
    p = _P(B, x, y, z, yaw, "door_flap")
    hw = w / 2
    p.box(WOOD, -hw, hw, -0.025, 0.025, 0.0, 0.98, f="xXyY")
    for (a, b) in ((-hw + 0.08, -0.04), (0.04, hw - 0.08)):
        p.box(WOODL, a, b, -0.031, -0.025, 0.12, 0.86, f="xXyZz")
    p.bev(WOOD, -hw, hw, -0.2, 0.12, 0.98, 1.01, 0.008, bottom=True)
    p.box(BRASS, -hw, hw, -0.206, -0.2, 0.985, 1.005, f="xXyZz")
    p.lathe(BRASS, hw - 0.2, -0.08, [(0.045, 1.01), (0.045, 1.03), (0.03, 1.06), (0.0, 1.07)], 6)
    p.card_top("paper_typed", -hw + 0.1, -hw + 0.3, -0.16, 0.08, 1.012)
    p.core(-hw + 0.01, hw - 0.01, -0.02, 0.02, 0.05, 0.97)
    return p.done(solid=solid, top=1.01, front=0.6)


# ------------------------------------------------------------------------------------------------ bridge
def _bridge(K, C, z, ch):
    B = K.B
    # ---- wheelhouse
    r = K.put(ships_wheel, 41.6, 0.0, z, AFT)
    K.thing_at("wheel", "machine", r["hub"], r["yaw"], what="helm")
    K.spot("staff_helmsman", "work", 41.02, 0.0, z, FWD)
    r = K.put(telegraph, 41.6, 1.1, z, AFT)
    K.thing_at("telegraph", "machine", (r["pos"][0], r["pos"][1], K.P(0, 0, z + 1.1)[2]), r["yaw"], what="telegraph")
    r = K.put(binnacle, 42.25, 0.0, z, AFT)
    K.thing_at("binnacle", "board", (r["pos"][0], r["pos"][1], K.P(0, 0, z + 1.15)[2]), r["yaw"])
    r = K.put(radar_display, 41.15, -1.65, z, AFT)
    K.thing_at("radar", "board", (r["pos"][0], r["pos"][1], K.P(0, 0, z + 1.1)[2]), r["yaw"])
    r = K.put(voice_pipes, 42.3, 0.75, z, AFT, n=2)
    K.thing_at("voice_pipes", "machine", (r["pos"][0], r["pos"][1], K.P(0, 0, z + 1.45)[2]), r["yaw"], what="voice_pipe")
    ax = 38.62                                              # the aft bulkhead's inner face
    r = K.put(flag_locker, ax, 0.2, z, FWD, w=1.2, zb=1.05)
    K.thing_at("flag_locker", "board", r["pos"], r["yaw"], topic="flags")
    K.put(kit.wall_clock, ax, 2.42, z, FWD, zc=1.95, r=0.15, face="clock_2")
    K.thing("wheelhouse_clock", "board", ax + 0.05, 2.42, z + 1.95, FWD, what="clock")
    r = K.put(barometer, ax, 2.42, z, FWD, zc=1.52)
    K.thing_at("wheelhouse_barometer", "board", r["pos"], r["yaw"], what="barometer")
    K.put(kit.stool, 40.3, 2.3, z, FWD, mat=WOOD, h=0.7) if False else None
    r = K.put(whistle_pull, 41.2, -0.75, z + ch, AFT, drop=ch - 1.75)
    K.thing_at("whistle_pull", "machine", r["grip"], r["yaw"], what="ship_whistle")
    for s in (1, -1):
        K.put(pelorus, 41.15, s * 4.45, z, AFT)
    K.spot("staff_captain", "work", 40.9, 1.75, z, FWD)
    K.spot("stand_bridge_1", "stand", 40.2, 0.9, z, FWD)
    K.spot("stand_bridge_2", "stand", 40.2, -0.7, z, FWD)
    K.spot("stand_bridge_3", "stand", 39.4, 0.3, z, FWD)
    K.lamp(40.6, 0.0, z + ch - 0.1, lumens=220, colour="#FFC9A0", kind="public", radius=4.5)
    # ---- chart room (port, aft of the wheelhouse, an open arch between)
    r = K.put(chart_table, 36.62, -1.5, z, FWD, w=1.7, d=0.95)
    K.thing_at("chart_table", "board", r["chart"], r["yaw"], topic="chart")
    K.thing_at("chronometer", "board", r["chrono"], r["yaw"], what="clock")
    K.spot("staff_mate", "work", 37.45, -1.5, z, AFT)
    K.put(kit.wall_shelf, 37.3, -2.85, z, STBD, w=0.9, zt=1.55, mat=WOOD) if False else None
    K.put(kit.stool, 37.95, -2.5, z, FWD, mat=WOOD, h=0.6)
    # ---- wings
    for s, name, h in ((1, "bridge_wing_stbd", STBD), (-1, "bridge_wing_port", PORT)):
        K.thing(name, "board", 40.25, s * 4.86, z + 1.12, h, topic="sea_view")
        K.spot("wing_%s_1" % ("stbd" if s > 0 else "port"), "stand", 40.25, s * 4.4, z, h)
        K.spot("wing_%s_2" % ("stbd" if s > 0 else "port"), "stand", 39.6, s * 4.3, z, h)
    K.path("path_bridge", [(39.2, 4.2), (40.1, 2.4), (40.1, 0.0), (40.1, -2.4), (39.2, -4.2), (38.0, -1.2)], z)
    del B


# ------------------------------------------------------------------------------------------------ named points
def _marks(K, C, S):
    """The engine's named points (ULyudCruise::GetShipPoint). bandstand, card_table (saloon) and stern_rail (smoking
    deck) are declared with their rooms. bow / funnel_top / gangway stand on the shell's and the main deck's geometry:
    furnish_ba runs last, so any of the three that nobody declared by then is declared here (never twice)."""
    have = {s_["id"] for s_ in K.B.spots}
    if "bow" not in have:
        K.spot("bow", "mark", 60.6, 0.0, S.foredeck_z(60.6), FWD)
    if "funnel_top" not in have:
        K.spot("funnel_top", "mark", 23.5, 0.0, S.Z_FUNNEL_TOP, FWD)
    if "gangway" not in have:
        K.spot("gangway", "mark", 31.1, 4.3, S.Z["C"], STBD)
