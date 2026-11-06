"""Clinic: a small two-storey white modernist polyclinic.

Original design in the spirit of 1960s European health-centre modernism (white plaster, ribbon windows, a glazed
stair tower with diagonal concrete fins, a cantilevered canopy, a red-cross panel): 28 x 12 m main block (storey 3.6 m)
plus a 3.6 x 2.4 m glass stair tower at the front-left corner (8.4 m), roof terrace parapet, a lift/plant box (9.9 m),
vertical brise-soleil fins over the upper windows of the right half.
Ground floor ENTERABLE: waiting/reception hall (17 x 11 m, entrance door 1.8 m two leaves under the canopy, big sidelights,
reception counter with a "counter" spot, 4 "bench" spots), a corridor and three doctors' rooms (each: desk + chair
("work"), patient chair ("seat"), couch), doors 1.2 x 2.2 m, CT_Interior walls, floor and ceilings. The upper floor is solid.
Spots: counter, 4 bench, 3 x work + 3 x seat. Window kinds: "public" (all), "stair" (tower). No flats.
Entrance: "main".
Ideas for later: a first-floor with consulting rooms, a pharmacy kiosk, an ambulance bay at the back, a waiting-room TV.
Requests: none.
"""
import math

from citykit import box, DOOR_H  # noqa: F401
import geom

INFO = {
    "name_ru": "Поликлиника", "name_en": "Clinic",
    "style": "modernism", "use": "public",
    "inspired_by": "1960s European health centres: white ribbon-window blocks with glazed stair towers",
}

H1, H2 = 3.4, 7.2          # ground clear height, wall top
SLAB = 0.2
Y0, Y1 = 4.0, 16.0
X0, X1 = -14.0, 14.0
T = 0.3


def _xz(m, mat, poly, y0, y1):
    geom.extrude_profile(m, mat, mat, poly, -y1, -y0, lambda u, v, t: (u, -t, v))


def _wall(B, p0, p1, mat, holes, z1=H2, kind="public", skin=True, cards=True, cols=2, skin_z1=H1):
    """A wall p0->p1 (outside on the right). holes: (a, b, h0, h1) with a, b the coordinates (x or y) along the wall.
    Adds window cards in the holes and an inner CT_Interior skin up to skin_z1 (the enterable ground floor)."""
    dx, dy = p1[0] - p0[0], p1[1] - p0[1]
    ax = 0 if abs(dx) > abs(dy) else 1
    c0 = p0[ax]

    def U(a, b, c0=c0):
        return tuple(sorted((abs(a - c0), abs(b - c0))))
    uh = []
    for a, b, h0, h1 in holes:
        u0, u1 = U(a, b)
        uh.append((u0, u1, h0, h1))
    F = B.facade(p0, p1, 0.0, z1, mat, holes=uh, depth=T, reveal_mat="CT_ConcreteWhite", back=False)
    if cards:
        for (u0, u1, h0, h1), (_, _, _, _) in zip(uh, holes):
            if h1 - h0 > 2.3 and h0 < 0.5 and kind == "public" and h1 < 2.5:
                continue                                  # a door hole: stays open
            B.facade_window(F, u0, u1, h0, h1, kind=kind, cols=cols if (u1 - u0) > 1.4 else 1, inset=0.12)
    if skin:
        L = math.hypot(dx, dy)
        n = (dy / L, -dx / L)                              # outward
        q0 = (p1[0] - n[0] * T, p1[1] - n[1] * T)
        q1 = (p0[0] - n[0] * T, p0[1] - n[1] * T)
        d0 = q0[ax]
        sh = []
        for a, b, h0, h1 in holes:
            if h0 < H1:
                sh.append(tuple(sorted((abs(a - d0), abs(b - d0)))) + (h0, min(h1, skin_z1)))
        B.facade(q0, q1, 0.0, skin_z1, "CT_Interior", holes=sh, depth=0.02, reveal_mat="CT_Interior", back=False)
    return F


def _chair(d, x, y, face):
    """A chair at (x, y) whose back is opposite the facing direction face (+1 = faces +y, -1 = faces -y)."""
    box(d, "CT_Wood", x - 0.22, x + 0.22, y - 0.22, y + 0.22, 0.44, 0.48)
    box(d, "CT_Steel", x - 0.22, x + 0.22, y - face * 0.22 - 0.015, y - face * 0.22 + 0.015, 0.48, 0.88)
    for dx in (-0.2, 0.2):
        for dy in (-0.2, 0.2):
            box(d, "CT_Steel", x + dx - 0.015, x + dx + 0.015, y + dy - 0.015, y + dy + 0.015, 0.0, 0.44)


def build(B):
    m, d = B.m, B.detail
    IX0, IX1, IY0, IY1 = X0 + T, X1 - T, Y0 + T, Y1 - T
    # ================================================================ facades (holes in world x / y)
    bay = (X1 - (-10.4)) / 7
    upfront = []
    for i in range(7):
        a, b = -10.4 + i * bay + 0.3, -10.4 + (i + 1) * bay - 0.3
        if i == 1:
            continue                                       # the red-cross panel hangs here
        upfront.append((a, b, 4.5, 6.6))
    front = [(-5.9, -4.1, 0.0, 2.4), (-10.0, -6.5, 0.3, 3.0), (-3.5, 2.8, 0.3, 3.0),
             (4.3, 6.1, 0.9, 2.6), (7.8, 9.6, 0.9, 2.6), (11.3, 13.1, 0.9, 2.6)] + upfront
    _wall(B, (-10.4, Y0), (X1, Y0), "CT_Plaster", front)
    B.entrance("main", (-5.0, Y0, 0.0), 0.0, width=1.8, height=2.4, kind="street", leaves=2)
    # back (faces +y), from x = 14 to -14
    back = [(x - 0.9, x + 0.9, 0.9, 2.6) for x in (-11.0, -7.0, -3.0, 4.5, 8.0, 11.5)]
    back += [(-13.0 + i * 3.857 + 0.3, -13.0 + (i + 1) * 3.857 + 0.3 - 0.6 + 0.0, 4.5, 6.6) for i in range(7)]
    _wall(B, (X1, Y1), (X0, Y1), "CT_Plaster", [(a, b, h0, h1) for a, b, h0, h1 in back])
    # right end (faces +x)
    _wall(B, (X1, Y0), (X1, Y1), "CT_Plaster", [(6.0, 8.0, 0.9, 2.6), (12.0, 14.0, 0.9, 2.6), (6.0, 8.0, 4.5, 6.6), (12.0, 14.0, 4.5, 6.6)])
    # left end (faces -x), from y = 16 to 4
    _wall(B, (X0, Y1), (X0, Y0), "CT_Plaster", [(5.5, 8.5, 0.9, 2.6), (10.5, 13.5, 0.9, 2.6), (5.5, 8.5, 4.5, 6.6), (10.5, 13.5, 4.5, 6.6)])
    # ================================================================ glazed stair tower (front-left)
    TH = 8.1
    box(m, "CT_ConcreteWhite", -13.7, -10.4, 1.9, Y0 + T, 0.0, TH)
    box(m, "CT_ConcreteWhite", -14.3, -10.1, 1.3, 4.3, TH, TH + 0.3)
    F = B.facade((-14.0, 1.6), (-10.4, 1.6), 0.0, TH, "CT_ConcreteWhite", holes=[(0.45, 3.15, 0.15, 7.9)], depth=T,
                 reveal_mat="CT_ConcreteWhite", back=False)
    B.facade_window(F, 0.45, 3.15, 0.15, 7.9, kind="stair", cols=3, rows=4, inset=0.12)
    F = B.facade((-14.0, Y0), (-14.0, 1.6), 0.0, TH, "CT_ConcreteWhite", holes=[(0.4, 2.0, 0.15, 7.9)], depth=T,
                 reveal_mat="CT_ConcreteWhite", back=False)
    B.facade_window(F, 0.4, 2.0, 0.15, 7.9, kind="stair", cols=2, rows=4, inset=0.12)
    # diagonal fins across the front glazing (stair flight silhouette) and landing bands
    for k, xs in enumerate((-13.6, -12.4, -11.2)):
        _xz(m, "CT_ConcreteWhite", [(xs, 0.15), (xs + 0.4, 0.15), (xs + 1.2, 7.9), (xs + 0.8, 7.9)], 1.35, 1.5)
    for z in (3.6, 7.2):
        box(m, "CT_ConcreteWhite", -14.0, -10.4, 1.35, 1.6, z - 0.1, z + 0.1)
    # ================================================================ interior of the ground floor
    # floors / ceiling / upper storey
    box(m, "CT_Floor", IX0, IX1, IY0, IY1, 0.0, 0.02, faces="Z")
    box(m, "CT_Interior", IX0, IX1, IY0, IY1, H1, H1 + SLAB)
    box(m, "CT_Plaster", IX0, IX1, IY0, IY1, H1 + SLAB, H2)
    # partition hall | rooms (x 3.3..3.6), gap to the corridor at y 12.4..13.6
    box(m, "CT_Interior", 3.3, 3.6, IY0, 12.4, 0.0, H1)
    box(m, "CT_Interior", 3.3, 3.6, 13.6, IY1, 0.0, H1)
    box(m, "CT_Interior", 3.3, 3.6, 12.4, 13.6, 2.3, H1)
    # corridor wall behind the rooms (y 11..11.3) with three door gaps, dividers between rooms
    cx = (5.2, 8.7, 12.15)
    xs = [3.6] + [v for c in cx for v in (c - 0.6, c + 0.6)] + [IX1]
    box(m, "CT_Interior", xs[0], xs[1], 11.0, 11.3, 0.0, H1)
    box(m, "CT_Interior", xs[2], xs[3], 11.0, 11.3, 0.0, H1)
    box(m, "CT_Interior", xs[4], xs[5], 11.0, 11.3, 0.0, H1)
    box(m, "CT_Interior", xs[6], xs[7], 11.0, 11.3, 0.0, H1)
    for c in cx:
        box(m, "CT_Interior", c - 0.6, c + 0.6, 11.0, 11.3, 2.3, H1)
    for x in (6.8, 10.3):
        box(m, "CT_Interior", x, x + 0.3, IY0, 11.3, 0.0, H1)
    # ---- reception hall furniture
    box(d, "CT_Wood", -9.5, -3.5, 11.3, 12.0, 0.0, 1.1)
    box(d, "CT_Frame", -9.5, -3.5, 11.3, 12.0, 1.1, 1.14)
    B.spot("reception", "counter", (-6.5, 12.7, 0.0), 0.0)
    benches = [(-12.7, 7.2, 90.0, "x"), (-12.7, 9.4, 90.0, "x"), (-8.0, 8.0, 0.0, "y"), (-1.0, 8.0, 0.0, "y")]
    for i, (x, y, yaw, ax) in enumerate(benches):
        if ax == "x":
            box(d, "CT_Wood", x - 0.25, x + 0.2, y - 0.9, y + 0.9, 0.42, 0.47)
            box(m, "CT_Steel", x - 0.2, x + 0.15, y - 0.85, y - 0.75, 0.0, 0.42)
            box(m, "CT_Steel", x - 0.2, x + 0.15, y + 0.75, y + 0.85, 0.0, 0.42)
        else:
            box(d, "CT_Wood", x - 0.9, x + 0.9, y - 0.25, y + 0.2, 0.42, 0.47)
            box(m, "CT_Steel", x - 0.85, x - 0.75, y - 0.2, y + 0.15, 0.0, 0.42)
            box(m, "CT_Steel", x + 0.75, x + 0.85, y - 0.2, y + 0.15, 0.0, 0.42)
        B.spot("wait_%d" % i, "bench", (x, y, 0.0), yaw)
    # ---- doctors' rooms
    for i, c in enumerate(cx):
        box(d, "CT_Wood", c - 0.8, c + 0.8, 7.6, 8.3, 0.72, 0.76)
        for dx in (-0.75, 0.75):
            box(d, "CT_Steel", c + dx - 0.03, c + dx + 0.03, 7.65, 8.25, 0.0, 0.72)
        _chair(d, c, 8.9, -1)                               # the doctor faces -y
        _chair(d, c, 6.8, +1)                               # the patient faces +y
        box(d, "CT_Wood", c - 1.0, c - 0.4, 9.8, 10.9, 0.0, 0.5)  # couch
        box(d, "CT_FrameWhite", c + 0.8, c + 1.4, 10.4, 10.9, 0.0, 1.8)               # cabinet
        B.spot("doctor_%d" % i, "work", (c, 9.3, 0.0), 0.0)
        B.spot("patient_%d" % i, "seat", (c, 6.4, 0.0), 180.0)
    # ================================================================ canopy, cross panel, roof, fins
    box(m, "CT_ConcreteWhite", -8.5, -1.5, 0.8, Y0, 3.0, 3.25)
    box(m, "CT_PlasterRed", -8.5, -1.5, 0.75, 0.85, 2.9, 3.5)
    for px in (-8.2, -1.8):
        B.column_round(px, 1.3, 0.0, 3.0, 0.1, mat="CT_Steel", segs=10)
    cxp, czp = -5.17, 5.4
    box(m, "CT_TileWhite", cxp - 1.1, cxp + 1.1, Y0 - 0.06, Y0, czp - 1.1, czp + 1.1, faces="yxXzZ")
    box(m, "CT_SteelRed", cxp - 0.75, cxp + 0.75, Y0 - 0.1, Y0 - 0.06, czp - 0.25, czp + 0.25, faces="yxXzZ")
    box(m, "CT_SteelRed", cxp - 0.25, cxp + 0.25, Y0 - 0.1, Y0 - 0.06, czp - 0.75, czp + 0.75, faces="yxXzZ")
    # roof slab (proud), parapet, plant box
    box(m, "CT_ConcreteWhite", X0 - 0.4, X1 + 0.4, Y0 - 0.4, Y1 + 0.4, H2, H2 + 0.3)
    t = 0.2
    for (a, b, c, e) in ((X0 - 0.4, X1 + 0.4, Y0 - 0.4, Y0 - 0.4 + t), (X0 - 0.4, X1 + 0.4, Y1 + 0.4 - t, Y1 + 0.4),
                         (X0 - 0.4, X0 - 0.4 + t, Y0 - 0.4 + t, Y1 + 0.4 - t), (X1 + 0.4 - t, X1 + 0.4, Y0 - 0.4 + t, Y1 + 0.4 - t)):
        box(m, "CT_ConcreteWhite", a, b, c, e, H2 + 0.3, H2 + 0.8)
    box(m, "CT_Roof", X0 - 0.2, X1 + 0.2, Y0 - 0.2, Y1 + 0.2, H2 + 0.29, H2 + 0.31, faces="Z")
    box(m, "CT_PlasterGrey", 4.0, 8.0, 9.0, 13.0, H2 + 0.3, 9.9)
    # brise-soleil fins before the upper windows of the right half
    x = 0.6
    while x < 13.8:
        box(d, "CT_Louvre", x, x + 0.1, Y0 - 0.5, Y0 - 0.02, 4.1, 6.9)
        x += 0.9
    B.footprint = [(X0 - 0.4, 0.75), (X1 + 0.4, 0.75), (X1 + 0.4, Y1 + 0.4), (X0 - 0.4, Y1 + 0.4)]
    B.height = 9.9
