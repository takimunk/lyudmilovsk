"""Plain block: the town's ordinary, good-looking five-storey modernist housing slab (repeated on 6 plots).

38 m x 12 m x 15 m (+ stair bulkheads, 17.4 m). Two stair sections, each with its own entrance (A, B) in a brick
stair tower clad in warm brick with a concrete canopy, a lobby (3.2 x 3.2 m, hollow, lit window, interior
walls) behind the door and a stair window on every landing; 2 flats per landing per stair: 5 storeys
(0..4) x 4 = 20 flats. Each flat: two street windows, a recessed LOGGIA (1.4 m deep, pastel parapet colours chequered
over storeys) with a glazed door and a window, and two windows at the back. White plaster slabs,
slab edges stand 10 cm proud as horizontal lines, dark plinth, roof parapet, bulkheads over the stairs.

Frame: building front at y = 3 (a 3 m front strip for the pavement), back at y = 15, footprint 38.8 x 13.7
incl. slab edges and the canopies. Plots 40x30, 60x20, 50x20 all fit.
Enterable: the two entrance lobbies only (people walk in and vanish). Window kinds: flat, stair.
Ideas for later: washing lines on loggias, satellite dishes, bike racks, a shop in one ground flat.
Requests: none.
"""
from citykit import box, DOOR_W, DOOR_H  # noqa: F401

INFO = {
    "name_ru": "Жилой дом (пятиэтажка)", "name_en": "Plain Block",
    "style": "modernism", "use": "residential",
    "inspired_by": "late 1950s-70s European social housing slabs with loggias and brick stair towers",
}

ST = 3.0            # storey height (floor to floor)
WALLH = 2.75        # clear wall height; the slab is 0.25 on top
N = 5
HALF = 19.0
CORE_W = 3.6
YF, YB = 3.0, 15.0  # front and back outer faces
WT = 0.3
LOG_D = 1.4
CORES = [-9.5, 9.5]
ROOFZ = N * ST      # roof slab top, 15.0
ACC = ["CT_PlasterBlue", "CT_PlasterYellow", "CT_Plaster"]


def build(B):
    m = B.m
    ZT = ROOFZ - 0.25
    # ---- flats: (id nn, core x, outward direction, entrance, section)
    flats = []
    for si, cx in enumerate(CORES):
        ent = "AB"[si]
        flats.append((cx - CORE_W / 2, -1, ent))
        flats.append((cx + CORE_W / 2, +1, ent))
    spans = [(-HALF, -11.3), (-7.7, 0.0), (0.0, 7.7), (11.3, HALF)]
    loggias = [(-HALF, -16.2), (-2.8, 0.0), (0.0, 2.8), (16.2, HALF)]

    # ---- front facades per flat span (white) and core (brick)
    for k, ((xa, xb), (xc, dr, ent)) in enumerate(zip(spans, flats)):
        nn = k + 1
        holes, cards = [], []
        for s in range(N):
            fl = s * ST
            fid = "%d-%02d" % (s, nn)
            B.flat(fid, storey=s, beds=2, door=(CORES[k // 2], 8.0, fl + 0.05), entrance=ent)
            for (u0, u1) in ((0.5, 2.3), (2.9, 4.1)):
                xs = sorted((xc + dr * u0, xc + dr * u1))
                h = (xs[0] - xa, xs[1] - xa, fl + 0.9, fl + 2.2)
                holes.append(h)
                cards.append((h, fid, 2 if u1 - u0 > 1.5 else 1))
            lx0, lx1 = loggias[k]
            holes.append((lx0 - xa, lx1 - xa, fl, fl + WALLH))
        F = B.facade((xa, YF), (xb, YF), 0.0, ZT, "CT_Plaster", holes=holes, depth=WT, reveal_mat="CT_Plaster")
        for (u0, u1, z0, z1), fid, cols in cards:
            B.facade_window(F, u0, u1, z0, z1, flat=fid, cols=cols, inset=0.12)
    for ci, cx in enumerate(CORES):
        xa, xb = cx - CORE_W / 2, cx + CORE_W / 2
        holes = [(0.3, 1.6, 0.0, DOOR_H), (2.0, 3.3, 0.9, 2.2)]
        for s in range(1, N):
            holes.append((1.2, 2.4, s * ST + 1.0, s * ST + 2.2))
        F = B.facade((xa, YF), (xb, YF), 0.0, ZT, "CT_Brick", holes=holes, depth=WT, reveal_mat="CT_Brick")
        B.facade_window(F, 2.0, 3.3, 0.9, 2.2, kind="stair", inset=0.12)
        for s in range(1, N):
            B.facade_window(F, 1.2, 2.4, s * ST + 1.0, s * ST + 2.2, kind="stair", inset=0.12)
        B.entrance("AB"[ci], (xa + 0.95, YF, 0.0), 0.0, width=1.3, height=DOOR_H, kind="street")
        # canopy with two slim steel posts
        box(m, "CT_ConcreteWhite", cx - 2.0, cx + 2.0, 1.4, YF, 2.5, 2.7)
        for px in (cx - 1.85, cx + 1.85):
            box(m, "CT_Steel", px - 0.04, px + 0.04, 1.5, 1.58, 0.0, 2.5)
        # lobby: floor, side walls, back wall, bulkhead over the stairs
        box(m, "CT_Floor", cx - 1.6, cx + 1.6, YF + WT, 6.5, 0.0, 0.02, faces="Z")
        for x0, x1 in ((cx - 1.8, cx - 1.6), (cx + 1.6, cx + 1.8)):
            box(m, "CT_Interior", x0, x1, YF + WT, 6.7, 0.0, WALLH)
        box(m, "CT_Interior", cx - 1.8, cx + 1.8, 6.5, YB - WT, 0.0, WALLH)
        box(m, "CT_Plaster", cx - 1.8, cx + 1.8, YF + WT, YB - WT, WALLH, ZT)
        box(m, "CT_PlasterGrey", cx - 1.7, cx + 1.7, 8.5, 14.4, ROOFZ + 0.5, ROOFZ + 2.4)
    # ---- mass behind the front facades
    cuts = sorted(set([-HALF, HALF] + [x for l in loggias for x in l] + [c + d * CORE_W / 2 for c in CORES for d in (-1, 1)]))
    for xa, xb in zip(cuts, cuts[1:]):
        mid = (xa + xb) / 2
        in_core = any(abs(mid - c) < CORE_W / 2 for c in CORES)
        in_log = any(l[0] < mid < l[1] for l in loggias)
        if in_core:
            continue                      # built with the lobby
        if in_log:
            box(m, "CT_Plaster", xa, xb, 4.7, YB - WT, 0.0, ZT)
        else:
            box(m, "CT_Plaster", xa, xb, YF + WT, YB - WT, 0.0, ZT)
    # ---- loggias: back walls with a door-window and a window; parapets; floors
    for k, (lx0, lx1) in enumerate(loggias):
        nn = k + 1
        holes, cards = [], []
        for s in range(N):
            fl = s * ST
            fid = "%d-%02d" % (s, nn)
            for (u0, u1, h0, h1) in ((0.25, 1.05, fl + 0.12, fl + 2.2), (1.45, 2.55, fl + 0.9, fl + 2.2)):
                holes.append((u0, u1, h0, h1))
                cards.append((u0, u1, h0, h1, fid))
            mat = ACC[(s + k) % 3]
            box(m, mat, lx0, lx1, YF, YF + 0.25, fl, fl + 1.0)
        F = B.facade((lx0, YF + LOG_D), (lx1, YF + LOG_D), 0.0, ZT, "CT_Plaster", holes=holes, depth=WT, reveal_mat="CT_Plaster")
        for (u0, u1, h0, h1, fid) in cards:
            B.facade_window(F, u0, u1, h0, h1, flat=fid, cols=2 if u1 - u0 > 1 else 1, inset=0.12)
        box(m, "CT_Paving", lx0, lx1, YF, YF + LOG_D, 0.0, 0.1, faces="Z")
    box(m, "CT_ConcreteWhite", -0.08, 0.08, YF, YF + LOG_D, 0.0, ZT)           # divider of the central loggia
    # ---- back facade
    holes, cards = [], []
    for k, (xa, xb) in enumerate(spans):
        nn = k + 1
        for s in range(N):
            fl = s * ST
            fid = "%d-%02d" % (s, nn)
            for f in (0.27, 0.73):
                xcn = xa + (xb - xa) * f
                u0, u1 = HALF - (xcn + 0.7), HALF - (xcn - 0.7)
                holes.append((u0, u1, fl + 0.9, fl + 2.2))
                cards.append((u0, u1, fl + 0.9, fl + 2.2, fid))
    for cx in CORES:
        for s in range(N):
            u0, u1 = HALF - (cx + 0.6), HALF - (cx - 0.6)
            holes.append((u0, u1, s * ST + 1.0, s * ST + 2.2))
            cards.append((u0, u1, s * ST + 1.0, s * ST + 2.2, None))
    F = B.facade((HALF, YB), (-HALF, YB), 0.0, ZT, "CT_Plaster", holes=holes, depth=WT, reveal_mat="CT_Plaster")
    for u0, u1, h0, h1, fid in cards:
        B.facade_window(F, u0, u1, h0, h1, flat=fid, kind=None if fid else "stair", cols=2 if fid else 1, inset=0.12)
    # ---- slab edges proud, roof, parapet, end plates, plinth
    for s in range(1, N + 1):
        box(m, "CT_ConcreteWhite", -HALF - 0.4, HALF + 0.4, YF - 0.1, YB + 0.1, s * ST - 0.25, s * ST)
    for sgn in (-1, 1):
        x0, x1 = (HALF, HALF + 0.3) if sgn > 0 else (-HALF - 0.3, -HALF)
        box(m, "CT_Plaster", x0, x1, YF, YB, 0.0, ZT)
    t = 0.2
    box(m, "CT_ConcreteWhite", -HALF - 0.4, HALF + 0.4, YF - 0.1, YF - 0.1 + t, ROOFZ, ROOFZ + 0.5)
    box(m, "CT_ConcreteWhite", -HALF - 0.4, HALF + 0.4, YB + 0.1 - t, YB + 0.1, ROOFZ, ROOFZ + 0.5)
    box(m, "CT_ConcreteWhite", -HALF - 0.4, -HALF - 0.4 + t, YF - 0.1 + t, YB + 0.1 - t, ROOFZ, ROOFZ + 0.5)
    box(m, "CT_ConcreteWhite", HALF + 0.4 - t, HALF + 0.4, YF - 0.1 + t, YB + 0.1 - t, ROOFZ, ROOFZ + 0.5)
    box(m, "CT_Roof", -HALF - 0.4 + t, HALF + 0.4 - t, YF - 0.1 + t, YB + 0.1 - t, ROOFZ - 0.01, ROOFZ + 0.02, faces="Z")
    for xa, xb in spans:
        box(m, "CT_ConcreteDark", xa, xb, YF - 0.05, YF, 0.0, 0.5, faces="yxX")
    B.footprint = [(-HALF - 0.4, 1.4), (HALF + 0.4, 1.4), (HALF + 0.4, YB + 0.1), (-HALF - 0.4, YB + 0.1)]
    B.height = ROOFZ + 2.4
    B.note("20 flats, 2 stairs, 2 lobbies; loggia pairs meet in the middle")
