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

Variants (module VARIANTS = {plot id: {attribute: value}}, built as plain_block__<plot>): STAIRS (2 or 3 stair
sections, 38 or 57 m), ACC (loggia parapet colours), BALCONY ("panel" / "rail" steel rail over a low panel / "screen"
fins), ROOF ("flat" / "pergola" roof terrace with timber pergola / "pitched" light gable roof), TOWER and CANOPY
(colours per stair), SHOP + SHOP_COL (ground-floor flats nn that become a shop: shopfront window, sign board, a shop
door "S<nn>" in the loggia; those flats do not exist in that variant). Flat naming stays "<storey>-<nn>", nn counted
left to right.
"""
import math

import geom
import furn_int3 as fu
from citykit import box, DOOR_W, DOOR_H  # noqa: F401
from _surrounds import add_surrounds

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
# ---- per-plot variant parameters (see VARIANTS below; the defaults are the base block)
STAIRS = 2          # number of stair sections: 2 -> 38 m, 3 -> 57 m (flats per storey = 2 x STAIRS)
TOWER = ("CT_Brick",)                   # stair tower cladding, cycled per stair
CANOPY = ("CT_ConcreteWhite",)          # entrance canopy slab, cycled per stair
ROOF = "flat"       # "flat" | "pergola" (roof terrace with timber pergola) | "pitched" (light copper-green gable roof)
BALCONY = "panel"   # loggia front: "panel" (solid coloured parapet) | "rail" (low panel + steel rail) | "screen" (panel + fins)
SHOP = ()           # flat numbers (nn of "<storey>-<nn>") whose GROUND flat is a shop (that flat does not exist then)
SHOP_COL = "CT_PlasterRed"              # shop sign board colour
VARIANTS = {
    "P-T1H2": {"ACC": ["CT_PlasterGreen", "CT_Plaster", "CT_PlasterYellow"], "ROOF": "pergola",
               "CANOPY": ("CT_PlasterRed",)},
    "P-T1H4": {"ACC": ["CT_PlasterPink", "CT_Plaster", "CT_PlasterBlue"], "BALCONY": "rail", "ROOF": "pitched",
               "TOWER": ("CT_PlasterYellow",), "CANOPY": ("CT_SteelRed",)},
    "P-T1H5": {"ACC": ["CT_PlasterRed", "CT_Plaster", "CT_PlasterGrey"], "BALCONY": "screen", "SHOP": (3,),
               "TOWER": ("CT_PlasterGrey", "CT_Brick"), "SHOP_COL": "CT_PlasterBlue"},
    "P-T4H1": {"STAIRS": 3, "ACC": ["CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterGreen"], "ROOF": "pergola",
               "SHOP": (3, 4), "TOWER": ("CT_Brick", "CT_PlasterRed", "CT_Brick"), "CANOPY": ("CT_Steel",),
               "SHOP_COL": "CT_PlasterYellow"},
    "P-T4H2": {"ACC": ["CT_PlasterYellow", "CT_PlasterPink", "CT_PlasterGreen"], "BALCONY": "rail", "ROOF": "pitched",
               "SHOP": (1,), "TOWER": ("CT_PlasterBlue",), "CANOPY": ("CT_SteelRed",), "SHOP_COL": "CT_PlasterGreen"},
}


def build(B):
    global HALF, CORES
    HALF = 9.5 * STAIRS
    CORES = [9.5 * (2 * i - (STAIRS - 1)) for i in range(STAIRS)]
    m = B.m
    ZT = ROOFZ - 0.25
    # ---- flats: (id nn, core x, outward direction, entrance, section)
    flats = []
    for si, cx in enumerate(CORES):
        ent = "ABC"[si]
        flats.append((cx - CORE_W / 2, -1, ent))
        flats.append((cx + CORE_W / 2, +1, ent))
    spans, loggias = [], []
    for k in range(2 * STAIRS):
        dr = -1 if k % 2 == 0 else 1
        xs0 = CORES[k // 2] + dr * CORE_W / 2
        far = xs0 + dr * 7.7
        spans.append(tuple(sorted((xs0, far))))
        loggias.append(tuple(sorted((far - dr * 2.8, far))))
    shops = {(0, nn) for nn in SHOP}

    # ---- front facades per flat span (white) and core (brick)
    for k, ((xa, xb), (xc, dr, ent)) in enumerate(zip(spans, flats)):
        nn = k + 1
        holes, cards = [], []
        for s in range(N):
            fl = s * ST
            fid = "%d-%02d" % (s, nn)
            if (s, nn) in shops:
                xs = sorted((xc + dr * 0.4, xc + dr * 4.4))
                h = (xs[0] - xa, xs[1] - xa, 0.35, 2.5)
                holes.append(h)
                cards.append((h, None, 4))
                box(m, SHOP_COL, xs[0] - 0.1, xs[1] + 0.1, YF - 0.15, YF, 2.55, 2.95)
            else:
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
            B.facade_window(F, u0, u1, z0, z1, flat=fid, kind=None if fid else "public", cols=cols, inset=0.12)
    for ci, cx in enumerate(CORES):
        xa, xb = cx - CORE_W / 2, cx + CORE_W / 2
        holes = [(0.3, 1.6, 0.0, DOOR_H), (2.0, 3.3, 0.9, 2.2)]
        for s in range(1, N):
            holes.append((1.2, 2.4, s * ST + 1.0, s * ST + 2.2))
        F = B.facade((xa, YF), (xb, YF), 0.0, ZT, TOWER[ci % len(TOWER)], holes=holes, depth=WT, reveal_mat=TOWER[ci % len(TOWER)])
        B.facade_window(F, 2.0, 3.3, 0.9, 2.2, kind="stair", inset=0.12)
        for s in range(1, N):
            B.facade_window(F, 1.2, 2.4, s * ST + 1.0, s * ST + 2.2, kind="stair", inset=0.12)
        B.entrance("ABC"[ci], (xa + 0.95, YF, 0.0), 0.0, width=1.3, height=DOOR_H, kind="street")
        # canopy with two slim steel posts
        box(m, CANOPY[ci % len(CANOPY)], cx - 2.0, cx + 2.0, 1.4, YF, 2.5, 2.7)
        for px in (cx - 1.85, cx + 1.85):
            box(m, "CT_Steel", px - 0.04, px + 0.04, 1.5, 1.58, 0.0, 2.5)
        # lobby: floor, side walls, back wall, bulkhead over the stairs
        box(m, "CT_Floor", cx - 1.6, cx + 1.6, YF + WT, 6.5, 0.0, 0.02, faces="Z")
        for x0, x1 in ((cx - 1.8, cx - 1.6), (cx + 1.6, cx + 1.8)):
            box(m, "CT_Interior", x0, x1, YF + WT, 6.7, 0.0, WALLH)
        box(m, "CT_Interior", cx - 1.8, cx + 1.8, 6.5, YB - WT, 0.0, WALLH)
        box(m, "CT_Plaster", cx - 1.8, cx + 1.8, YF + WT, YB - WT, WALLH, ZT)
        box(m, "CT_PlasterGrey", cx - 1.7, cx + 1.7, 8.5, 14.4, ROOFZ + 0.5, ROOFZ + 2.4)
        # lobby furnishing: letter boxes on the left wall, a doormat inside the door, a plant, a lamp (dusk to dawn)
        fu.mailboxes(B, fu.Fr(cx - 1.6, YF + WT, 90.0, 0.02), 1.9, 0.95, cols=5, rows=3)
        fu.doormat(B, fu.Fr(cx - 0.85, YF + WT, 0.0, 0.02), 0.0, -0.45)
        fu.plant(B, cx + 1.2, 6.0, 0.02, 0.9, "CT_PlasterRed")
        B.lamp((cx, 4.9, 2.45), lumens=420, colour="#FFE0B0", kind="lobby", radius=5.0)
        box(B.detail, "CT_Steel", cx - 0.01, cx + 0.01, 4.89, 4.91, 2.53, WALLH, faces="xXyY")
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
            shop = (s, nn) in shops
            specs = ((0.25, 1.45, 0.1, 2.3), (1.6, 2.6, 0.9, 2.2)) if shop else \
                ((0.25, 1.05, fl + 0.12, fl + 2.2), (1.45, 2.55, fl + 0.9, fl + 2.2))
            for (u0, u1, h0, h1) in specs:
                holes.append((u0, u1, h0, h1))
                cards.append((u0, u1, h0, h1, "shop" if shop else fid))
            if shop:
                B.entrance("S%d" % nn, (lx0 + 0.85, YF + LOG_D, 0.1), 0.0, width=1.2, height=DOOR_H, kind="shop")
                continue
            mat = ACC[(s + k) % 3]
            if BALCONY == "rail":
                box(m, mat, lx0, lx1, YF, YF + 0.25, fl, fl + 0.45)
                B.railing([(lx0 + 0.05, YF + 0.12), (lx1 - 0.05, YF + 0.12)], fl + 0.45, h=0.65, post=0.9)
            elif BALCONY == "screen":
                box(m, mat, lx0, lx1, YF, YF + 0.25, fl, fl + 0.6)
                for i in range(int((lx1 - lx0) / 0.3)):
                    xf = lx0 + 0.15 + i * 0.3
                    box(B.detail, "CT_Louvre", xf - 0.03, xf + 0.03, YF, YF + 0.22, fl + 0.6, fl + 2.5)
            else:
                box(m, mat, lx0, lx1, YF, YF + 0.25, fl, fl + 1.0)
        F = B.facade((lx0, YF + LOG_D), (lx1, YF + LOG_D), 0.0, ZT, "CT_Plaster", holes=holes, depth=WT, reveal_mat="CT_Plaster")
        for (u0, u1, h0, h1, fid) in cards:
            B.facade_window(F, u0, u1, h0, h1, flat=None if fid == "shop" else fid, kind="public" if fid == "shop" else None,
                            cols=2 if u1 - u0 > 1 else 1, inset=0.12)
        box(m, "CT_Paving", lx0, lx1, YF, YF + LOG_D, 0.0, 0.1, faces="Z")
    for i in range(STAIRS - 1):
        xd = (CORES[i] + CORES[i + 1]) / 2
        box(m, "CT_ConcreteWhite", xd - 0.08, xd + 0.08, YF, YF + LOG_D, 0.0, ZT)   # divider of a central loggia
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
                cards.append((u0, u1, fl + 0.9, fl + 2.2, "shop" if (s, nn) in shops else fid))
    for cx in CORES:
        for s in range(N):
            u0, u1 = HALF - (cx + 0.6), HALF - (cx - 0.6)
            holes.append((u0, u1, s * ST + 1.0, s * ST + 2.2))
            cards.append((u0, u1, s * ST + 1.0, s * ST + 2.2, None))
    F = B.facade((HALF, YB), (-HALF, YB), 0.0, ZT, "CT_Plaster", holes=holes, depth=WT, reveal_mat="CT_Plaster")
    for u0, u1, h0, h1, fid in cards:
        B.facade_window(F, u0, u1, h0, h1, flat=None if fid in (None, "shop") else fid,
                        kind="public" if fid == "shop" else (None if fid else "stair"), cols=2 if fid else 1, inset=0.12)
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
    if ROOF == "pergola":
        px = (CORES[0] + CORES[-1]) / 2 if STAIRS == 2 else (CORES[0] + CORES[1]) / 2
        box(m, "CT_Wood", px - 4.2, px + 4.2, 5.0, 11.0, ROOFZ + 0.02, ROOFZ + 0.12, faces="Z")      # timber deck
        for ix in (-4.0, 4.0):
            for iy in (5.3, 10.7):
                box(B.detail, "CT_Wood", px + ix - 0.07, px + ix + 0.07, iy - 0.07, iy + 0.07, ROOFZ + 0.12, ROOFZ + 2.5)
        for iy in (5.3, 10.7):
            box(B.detail, "CT_Wood", px - 4.4, px + 4.4, iy - 0.08, iy + 0.08, ROOFZ + 2.5, ROOFZ + 2.7)
        for i in range(9):
            box(B.detail, "CT_Wood", px - 4.3 + i * 1.0, px - 4.3 + i * 1.0 + 0.1, 5.1, 10.9, ROOFZ + 2.7, ROOFZ + 2.8)
        box(m, "CT_Wood", px - 3.0, px - 1.0, 5.4, 6.0, ROOFZ + 0.12, ROOFZ + 0.55)                # bench / planter
        box(m, "CT_Grass", px + 1.0, px + 3.0, 5.4, 6.0, ROOFZ + 0.55, ROOFZ + 0.62, faces="Z")
        box(m, "CT_ConcreteWhite", px + 1.0, px + 3.0, 5.4, 6.0, ROOFZ + 0.12, ROOFZ + 0.55)
    elif ROOF == "pitched":
        geom.hip_roof(m, "CT_Copper", "CT_ConcreteWhite", "CT_ConcreteWhite", -HALF - 0.1, HALF + 0.1, YF + 0.1, YB - 0.1,
                      ROOFZ + 0.5, math.radians(8.0), 0.0, fascia_h=0.1, gable_x={"x", "X"})
    for xa, xb in spans:
        box(m, "CT_ConcreteDark", xa, xb, YF - 0.05, YF, 0.0, 0.5, faces="yxX")
    B.footprint = [(-HALF - 0.4, 1.4), (HALF + 0.4, 1.4), (HALF + 0.4, YB + 0.1), (-HALF - 0.4, YB + 0.1)]
    B.height = ROOFZ + (2.8 if ROOF == "pergola" else 2.4)
    B.note("%d flats, %d stairs, %d lobbies; loggia pairs meet in the middle" % (len(B.flats), STAIRS, STAIRS))
    add_surrounds(B, seed=3 * STAIRS + len(SHOP) + (ROOF == "pitched"))
