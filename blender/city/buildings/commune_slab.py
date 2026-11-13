"""Commune slab ("дом-коммуна"): a constructivist residential slab on pilotis with a communal block, linked by a
glazed bridge. Original design in the spirit of the Soviet "transitional type" houses of the late 1920s (Ginzburg's
Narkomfin, Moscow collective housing), not a copy of any one building.

Plot 70 x 14 m, max 20 m. Everything is in one line along the street (x -35 .. 35):
  communal block  x -35..-22, y 1..13   two volumes: a glazed double-height hall (canteen / laundry / club room, ENTERABLE,
                                        6.4 m high) and a gym box above it that overhangs the street by 0.7 m
                                        (ribbon windows, flat roof).
  glazed bridge   x -22..-18, y 10..12  at level 2 (z 7.0 = the corridor floor of the slab), glass sides, flat roof.
  slab            x -18..31, y 1..12    49 x 11 m, 5 storeys: pilotis storey (z 0..3.5, round columns, partly open,
                                        a glazed lobby / bike room under the east end) + 4 floors at z 4, 7, 10, 13;
                                        roof terrace at z 16 (railing, timber pergola, glazed solarium shed).
  stair tower     x 31..35, y 0.5..12.5 red (CT_PlasterRed) hollow tower, 18.4 m, glass slits, the street door "main".
Skip-stop flats: 16 two-level flats (8 in floors 1-2 = "1-01".."1-08", 8 in floors 3-4 = "3-01".."3-08"). Access
  corridors run along the back on floors 2 and 4 only (z 7 and 13): at the back you see a continuous corridor
  window band on those floors and small bedroom/kitchen windows on floors 1 and 3. The front is a continuous ribbon
  window on every floor (white piers, grey spandrels). All flats use entrance "main" (the tower).
Entrances: "main" (tower, double leaves, 1.6 m), "hall" (communal hall, double leaves, 2.4 m, red canopy).
Enterable: the communal hall (floor CT_Floor, CT_Interior walls and ceiling). Spots: 6 "table", 1 "counter" (serving
  counter at the back, customers face +y = yaw 180), 5 "work" (laundry machines along the west wall, users face
  -x = yaw 270), 3 "bench" under the slab. Spot yaw = the direction the person using the spot faces (0 = -y,
  towards the street; 90 = +x).
Colours: off-white plaster, grey spandrels/soffits, red tower, canopy and cap, dark steel rails, timber pergola.
Ideas for later: stairs inside the hall to the gym, interior corridors, sliding red shutters, a cinema in the hall,
  hanging planters, roof garden beds, a wall newspaper board under the pilotis.
"""
import math

import interior_kit as kit

from citykit import box, prism, ring, DOOR_W
from _cons_util import wall, win, hface, rect
from furn_int2 import Furn

INFO = {
    "name_ru": "Дом-коммуна", "name_en": "Commune Slab",
    "style": "constructivism", "use": "mixed",
    "inspired_by": "Soviet transitional-type communal housing of 1928-30 (Ginzburg, Narkomfin; skip-stop corridors, "
                   "pilotis, ribbon windows, communal block on a bridge)",
}

SX0, SX1, SY0, SY1 = -18.0, 31.0, 1.0, 12.0
NB = 8
BW = (SX1 - SX0) / NB
FL = (4.0, 7.0, 10.0, 13.0)
ZB = 3.5            # underside of the slab
ZT = 15.6           # top of the walls (roof slab 15.6..16.0)
ZR = 16.0
CORR = (7.0, 13.0)
XB0, XB1 = -35.0, -22.0     # communal block
YB0, YB1 = 1.0, 13.0
HALL_H = 6.4
# ---- per-plot variant parameters
ACC_P = "CT_PlasterRed"      # accent plaster (stair tower)
ACC_S = "CT_SteelRed"        # accent steel (canopies, gym edge band)
MIRROR = False               # mirrored left-right (communal block on the east, stair tower on the west)
VARIANTS = {
    "P-T2H2": {},
    "P-T3H3": {"MIRROR": True, "ACC_P": "CT_PlasterBlue", "ACC_S": "CT_Steel"},
}


def fid_of(f, k):
    return ("1-%02d" if f < 8 else "3-%02d") % (k + 1)


def build(B):
    _build(B)
    if MIRROR:
        _mirror(B)
    _furnish_hall(B, MIRROR)          # after the mirror: atlas pictures and text must not be mirrored


def _mirror(B):
    """Mirror the finished building about x = 0 (x -> -x; faces re-wound, yaws negated)."""
    for mesh in (B.m, B.detail, B.win):
        mesh.verts = [(-x, y, z) for x, y, z in mesh.verts]
        mesh.faces = [(mt, idx[::-1], uv[::-1]) for mt, idx, uv in mesh.faces]
    for w in B.windows:
        w["centre"][0] = -w["centre"][0]
        w["normal"][0] = -w["normal"][0]
    for f in B.flats.values():
        if f["door"]:
            f["door"][0] = -f["door"][0]
    for e in B.entrances:
        e["pos"][0] = -e["pos"][0]
        e["yaw"] = (-e["yaw"]) % 360
    for sp in B.spots:
        sp["pos"][0] = -sp["pos"][0]
        sp["yaw"] = (-sp["yaw"]) % 360
    for l in B.lamps:
        l["pos"][0] = -l["pos"][0]
    B.footprint = [(-x, y) for x, y in B.footprint][::-1]


def _build(B):
    m, det = B.m, B.detail
    # ---------------------------------------------------------------- slab: soffit, roof, pilotis
    prism(m, "CT_ConcreteDark", rect(SX0, SX1, SY0, SY1), ZB, FL[0], top=False, bottom=True, side_mat="CT_Plaster")
    roof = rect(SX0 - 0.15, SX1 + 0.15, SY0 - 0.15, SY1 + 0.15)
    prism(m, "CT_Plaster", roof, ZT, ZR, top=False, bottom=True)
    hface(m, "CT_Roof", roof, ZR, True)
    hface(m, "CT_Paving", rect(SX0 - 0.5, SX1, SY0 - 0.5, SY1 + 0.5), 0.0, True)
    for k in range(NB):
        x = SX0 + k * BW
        for y in (2.5, 9.6):
            B.column_round(x, y, 0.0, ZB, 0.33, mat="CT_ConcreteWhite", segs=14)
    B.column_round(SX1 - 0.3, 2.5, 0.0, ZB, 0.33, mat="CT_ConcreteWhite", segs=14)
    # ---------------------------------------------------------------- slab: front wall (ribbon windows)
    p0, p1 = (SX0, SY0), (SX1, SY0)
    for f in FL:
        z0 = f - 0.5
        wall(B, p0, p1, z0, f + 0.85, "CT_Concrete", depth=0.4, mat_in="CT_Plaster")
        holes = [(k * BW + 0.2, (k + 1) * BW - 0.2, f + 0.85, f + 2.35) for k in range(NB)]
        wall(B, p0, p1, f + 0.85, f + 2.35, "CT_Plaster", holes=holes, depth=0.4, mat_in="CT_Plaster")
        top = f + 2.5 if f < FL[-1] else ZT
        wall(B, p0, p1, f + 2.35, top, "CT_Plaster", depth=0.4)
        for k in range(NB):
            win(B, p0, p1, k * BW + 0.2, (k + 1) * BW - 0.2, f + 0.85, f + 2.35, flat=fid_of(f, k), cols=3, rows=1,
                inset=0.22, frame=0.05)
    # ---------------------------------------------------------------- slab: back wall (corridor bands / small windows)
    q0, q1 = (SX1, SY1), (SX0, SY1)
    for f in FL:
        if f in CORR:
            holes = [(0.3, SX1 - SX0 - 0.3, f + 0.9, f + 2.1)]
            wall(B, q0, q1, f - 0.5, f + 2.5 if f < FL[-1] else ZT, "CT_Plaster", holes=holes, depth=0.4)
            nwin = 16
            w = (SX1 - SX0 - 0.6) / nwin
            for i in range(nwin):
                win(B, q0, q1, 0.3 + i * w + 0.03, 0.3 + (i + 1) * w - 0.03, f + 0.9, f + 2.1, kind="stair", cols=1,
                    rows=1, inset=0.2, frame=0.04)
        else:
            holes = []
            for k in range(NB):
                xm = SX0 + (k + 0.5) * BW
                u = SX1 - xm
                holes.append((u - 0.65, u + 0.65, f + 0.9, f + 1.9))
            wall(B, q0, q1, f - 0.5, f + 2.5, "CT_Plaster", holes=holes, depth=0.4)
            for k in range(NB):
                xm = SX0 + (k + 0.5) * BW
                u = SX1 - xm
                win(B, q0, q1, u - 0.65, u + 0.65, f + 0.9, f + 1.9, flat=fid_of(f, k), cols=2, rows=1, inset=0.2,
                    frame=0.05)
    # ---------------------------------------------------------------- slab: west end wall with the bridge door (floor 2)
    wall(B, (SX0, SY1), (SX0, SY0), ZB, ZT, "CT_Plaster", holes=[(0.3, 1.7, 7.0, 9.3)], depth=0.4)
    # corridor/flat doors
    for g, fz in ((0, 7.0), (1, 13.0)):
        for k in range(NB):
            B.flat(fid_of(4.0 if g == 0 else 10.0, k), 1 if g == 0 else 3, beds=2,
                   door=(SX0 + (k + 0.5) * BW, SY1 - 1.0, fz), entrance="main")
    # ---------------------------------------------------------------- lobby / bike room under the east end
    lx0, ly0, ly1 = SX0 + 6 * BW, 3.6, 9.0
    wall(B, (lx0, ly0), (SX1, ly0), 0.0, ZB, "CT_PlasterGrey",
         holes=[(0.5 + i * 4.0, 0.5 + i * 4.0 + 3.2, 0.4, 3.0) for i in range(3)], depth=0.35, mat_in="CT_Plaster")
    for i in range(3):
        win(B, (lx0, ly0), (SX1, ly0), 0.5 + i * 4.0, 0.5 + i * 4.0 + 3.2, 0.4, 3.0, kind="public", cols=3, rows=1,
            inset=0.15, frame=0.06)
    wall(B, (SX1, ly1), (lx0, ly1), 0.0, ZB, "CT_PlasterGrey", depth=0.35, mat_in="CT_Plaster")
    wall(B, (lx0, ly1), (lx0, ly0), 0.0, ZB, "CT_PlasterGrey", depth=0.35, mat_in="CT_Plaster")
    # ---------------------------------------------------------------- stair tower (red)
    tx0, tx1, ty0, ty1 = 31.0, 35.0, 0.5, 12.5
    TH = 17.8
    hface(m, "CT_Paving", rect(tx0, tx1, ty0, ty1), 0.0, True)
    wall(B, (tx0, ty0), (tx1, ty0), 0.0, TH, ACC_P,
         holes=[(0.5, 2.1, 0.0, 2.5), (2.7, 3.5, 0.5, 3.1), (2.7, 3.5, 4.6, 17.0)], depth=0.4, mat_in="CT_Plaster")
    win(B, (tx0, ty0), (tx1, ty0), 2.7, 3.5, 0.5, 3.1, kind="stair", rows=2, inset=0.2, frame=0.05)
    win(B, (tx0, ty0), (tx1, ty0), 2.7, 3.5, 4.6, 17.0, kind="stair", rows=8, inset=0.2, frame=0.05)
    wall(B, (tx1, ty0), (tx1, ty1), 0.0, TH, ACC_P,
         holes=[(5.4, 6.6, 0.5, 3.1), (5.4, 6.6, 4.6, 17.0)], depth=0.4, mat_in="CT_Plaster")
    win(B, (tx1, ty0), (tx1, ty1), 5.4, 6.6, 0.5, 3.1, kind="stair", rows=2, inset=0.2, frame=0.05)
    win(B, (tx1, ty0), (tx1, ty1), 5.4, 6.6, 4.6, 17.0, kind="stair", rows=8, inset=0.2, frame=0.05)
    wall(B, (tx1, ty1), (tx0, ty1), 0.0, TH, ACC_P, holes=[(1.5, 2.5, 4.6, 17.0)], depth=0.4,
         mat_in="CT_Plaster")
    win(B, (tx1, ty1), (tx0, ty1), 1.5, 2.5, 4.6, 17.0, kind="stair", rows=8, inset=0.2, frame=0.05)
    wall(B, (tx0, ty1), (tx0, ty0), 0.0, ZB, ACC_P, depth=0.4, mat_in="CT_Plaster")
    tr = rect(tx0 - 0.25, tx1 + 0.25, ty0 - 0.25, ty1 + 0.25)
    prism(m, "CT_Plaster", tr, TH, TH + 0.6, top=False, bottom=True)
    hface(m, "CT_Roof", tr, TH + 0.6, True)
    B.entrance("main", ((tx0 + 0.5 + tx0 + 2.1) / 2, ty0, 0.0), 0.0, width=1.6, height=2.5, kind="street", leaves=2)
    box(m, ACC_S, tx0 + 0.3, tx0 + 2.3, ty0 - 1.2, ty0, 3.0, 3.15)               # door canopy
    # ---------------------------------------------------------------- roof terrace
    inset = rect(SX0 + 0.3, SX1 - 0.3, SY0 + 0.3, SY1 - 0.3)
    B.railing(inset + [inset[0]], ZR, h=1.05, mat="CT_Steel", post=1.5)
    # pergola
    for x in (2.0, 6.0, 10.0, 14.0):
        for y in (3.0, 9.0):
            box(det, "CT_Steel", x - 0.07, x + 0.07, y - 0.07, y + 0.07, ZR, ZR + 2.6)
    for y in (3.0, 9.0):
        box(det, "CT_Steel", 1.9, 14.1, y - 0.1, y + 0.1, ZR + 2.6, ZR + 2.78)
    for i in range(21):
        x = 2.0 + i * 0.6
        box(det, "CT_Wood", x - 0.05, x + 0.05, 2.9, 9.1, ZR + 2.78, ZR + 2.86)
    box(det, "CT_Wood", 3.0, 8.0, 5.4, 6.6, ZR, ZR + 0.45)                               # a long bench/planter
    # solarium shed
    sx0, sx1, sy0, sy1 = 18.0, 26.0, 6.5, 10.0
    wall(B, (sx0, sy0), (sx1, sy0), ZR, ZR + 2.5, "CT_Plaster",
         holes=[(0.4, 3.8, 0.5, 2.2), (4.2, 7.6, 0.5, 2.2)], depth=0.3)
    for u0, u1 in ((0.4, 3.8), (4.2, 7.6)):
        win(B, (sx0, sy0), (sx1, sy0), u0, u1, ZR + 0.5, ZR + 2.2, kind="public", cols=3, rows=1, inset=0.15, frame=0.05)
    wall(B, (sx1, sy0), (sx1, sy1), ZR, ZR + 2.5, "CT_Plaster", depth=0.3)
    wall(B, (sx1, sy1), (sx0, sy1), ZR, ZR + 2.5, "CT_Plaster", depth=0.3)
    wall(B, (sx0, sy1), (sx0, sy0), ZR, ZR + 2.5, "CT_Plaster", depth=0.3)
    sr = rect(sx0 - 0.4, sx1 + 0.4, sy0 - 0.6, sy1 + 0.4)
    prism(m, "CT_PlasterGrey", sr, ZR + 2.5, ZR + 2.75, top=True, bottom=True)
    # ---------------------------------------------------------------- communal block
    _block(B)
    # ---------------------------------------------------------------- glazed bridge at level 2
    bx0, bx1, by0, by1 = XB1, SX0, 10.0, 12.0
    box(m, "CT_PlasterGrey", bx0 - 0.05, bx1 + 0.05, by0 - 0.1, by1 + 0.1, 6.6, 7.0)
    prism(m, "CT_Plaster", rect(bx0 - 0.1, bx1 + 0.1, by0 - 0.15, by1 + 0.15), 9.3, 9.6, top=False, bottom=True)
    hface(m, "CT_Roof", rect(bx0 - 0.1, bx1 + 0.1, by0 - 0.15, by1 + 0.15), 9.6, True)
    for yy, face in ((by0, -1), (by1, 1)):
        xs = (bx0, bx1) if face < 0 else (bx1, bx0)
        B.window([(xs[0], yy, 7.0), (xs[1], yy, 7.0), (xs[1], yy, 9.3), (xs[0], yy, 9.3)], kind="public", cols=4, rows=2,
                 inset=0.0, frame=0.06)
    for x in (bx0, bx1):
        for yy in (by0, by1):
            box(det, "CT_Frame", x - 0.06, x + 0.06, yy - 0.06, yy + 0.06, 7.0, 9.3)
    # ---------------------------------------------------------------- footprint / benches
    B.footprint = rect(XB0, 35.25, 0.3, 13.0)
    B.height = TH + 0.6
    for i, x in enumerate((-12.0, 0.0, 12.0)):
        box(det, "CT_Wood", x - 0.9, x + 0.9, 5.4, 5.85, 0.0, 0.45)
        box(det, "CT_Steel", x - 0.9, x - 0.8, 5.4, 5.85, 0.0, 0.45)
        box(det, "CT_Steel", x + 0.8, x + 0.9, 5.4, 5.85, 0.0, 0.45)
        B.spot("bench%d" % (i + 1), "bench", (x, 5.6, 0.0), 0.0)
    B.note("16 skip-stop flats, corridors on floors 2 and 4; communal hall enterable at the west end")


def _block(B):
    m = B.m
    # floor, ceiling, overhanging upper slab
    prism(m, "CT_ConcreteDark", rect(XB0, XB1, YB0, YB1), -0.4, 0.0, top=False, bottom=False)
    hface(m, "CT_Floor", rect(XB0, XB1, YB0, YB1), 0.0, True)
    prism(m, "CT_Plaster", rect(XB0, XB1, 0.3, YB1), HALL_H, 7.0, top=False, bottom=True, side_mat="CT_Plaster")
    hface(m, "CT_Interior", rect(XB0, XB1, YB0, YB1), HALL_H - 0.012, False)   # 12 mm under the slab soffit: no z-fight
    hface(m, "CT_Roof", rect(XB0, XB1, 0.3, YB1), 7.0, True)
    # hall walls
    fp0, fp1 = (XB0, YB0), (XB1, YB0)
    holes = [(0.6, 4.4, 0.5, 5.6), (5.3, 7.7, 0.0, 2.7), (5.3, 7.7, 3.0, 5.6), (8.6, 12.4, 0.5, 5.6)]
    wall(B, fp0, fp1, 0.0, HALL_H, "CT_Plaster", holes=holes, depth=0.4, mat_in="CT_Interior")
    for u0, u1, z0, z1, rows in ((0.6, 4.4, 0.5, 5.6, 2), (5.3, 7.7, 3.0, 5.6, 1), (8.6, 12.4, 0.5, 5.6, 2)):
        win(B, fp0, fp1, u0, u1, z0, z1, kind="public", cols=4 if u1 - u0 > 3 else 2, rows=rows, inset=0.2, frame=0.06)
    wall(B, (XB1, YB0), (XB1, YB1), 0.0, HALL_H, "CT_Plaster", depth=0.4, mat_in="CT_Interior")
    wall(B, (XB1, YB1), (XB0, YB1), 0.0, HALL_H, "CT_Plaster", holes=[(1.0, 12.0, 4.0, 5.6)], depth=0.4,
         mat_in="CT_Interior")
    win(B, (XB1, YB1), (XB0, YB1), 1.0, 12.0, 4.0, 5.6, kind="public", cols=8, rows=1, inset=0.2, frame=0.05)
    wall(B, (XB0, YB1), (XB0, YB0), 0.0, HALL_H, "CT_Plaster", holes=[(2.0, 9.5, 3.0, 5.6)], depth=0.4,
         mat_in="CT_Interior")
    win(B, (XB0, YB1), (XB0, YB0), 2.0, 9.5, 3.0, 5.6, kind="public", cols=5, rows=1, inset=0.2, frame=0.05)
    # upper (gym) box: ribbon windows, flat roof
    UZ0, UZ1 = 7.0, 10.6
    up0, up1 = (XB0, 0.3), (XB1, 0.3)
    wall(B, up0, up1, UZ0, UZ0 + 0.9, "CT_PlasterGrey", depth=0.35)
    holes = [(0.5 + i * 3.1, 0.5 + i * 3.1 + 2.7, UZ0 + 0.9, UZ0 + 2.6) for i in range(4)]
    wall(B, up0, up1, UZ0 + 0.9, UZ0 + 2.6, "CT_Plaster", holes=holes, depth=0.35)
    for u0, u1, *_ in holes:
        win(B, up0, up1, u0, u1, UZ0 + 0.9, UZ0 + 2.6, kind="public", cols=3, rows=1, inset=0.18, frame=0.05)
    wall(B, up0, up1, UZ0 + 2.6, UZ1, "CT_Plaster", depth=0.35)
    wall(B, (XB1, 0.3), (XB1, 12.5), UZ0, UZ1, "CT_Plaster", holes=[(10.0, 11.4, 7.0, 9.3)], depth=0.35)
    wall(B, (XB1, 12.5), (XB0, 12.5), UZ0, UZ1, "CT_Plaster", depth=0.35)
    wall(B, (XB0, 12.5), (XB0, 0.3), UZ0, UZ1, "CT_Plaster", holes=[(2.5, 8.5, UZ0 + 0.9, UZ0 + 2.6)], depth=0.35)
    win(B, (XB0, 12.5), (XB0, 0.3), 2.5, 8.5, UZ0 + 0.9, UZ0 + 2.6, kind="public", cols=4, rows=1, inset=0.18, frame=0.05)
    ur = rect(XB0 - 0.15, XB1 + 0.15, 0.15, 12.65)
    prism(m, "CT_Plaster", ur, UZ1, UZ1 + 0.4, top=False, bottom=True)
    hface(m, "CT_Roof", ur, UZ1 + 0.4, True)
    # red canopy over the hall door, red edge band on the gym box
    box(m, ACC_S, -30.4, -26.6, -0.9, YB0, 3.0, 3.15)
    box(B.detail, ACC_S, XB0, XB1, 0.2, 0.34, UZ0 + 2.6, UZ0 + 2.9)
    B.entrance("hall", (-28.5, YB0, 0.0), 0.0, width=2.4, height=2.7, kind="street", leaves=2)


class _H:
    """Placement in the unmirrored plan (x -34.6..-22.4 hall at the west end); mirrors x and yaw for the east variant."""

    def __init__(self, B, mirror):
        self.B, self.mir = B, mirror

    def X(self, x):
        return -x if self.mir else x

    def Y(self, yaw):
        return (-yaw) % 360 if self.mir else yaw % 360

    def p(self, fn, x, y, yaw=0.0, z=0.0, **kw):
        return fn(self.B, self.X(x), y, z, self.Y(yaw), **kw)

    def thing(self, tid, kind, x, y, z, yaw, **props):
        self.B.thing(tid, kind, (self.X(x), y, z), self.Y(yaw), **props)

    def spot(self, sid, kind, x, y, yaw):
        self.B.spot(sid, kind, (self.X(x), y, 0.0), self.Y(yaw))

    def lamp(self, x, y, z, lumens=800, colour="#FFD2A0"):
        self.B.lamp((self.X(x), y, z), lumens=lumens, colour=colour, kind="public", radius=7, fixture=False)


def _washer(B, x, y, z=0.0, yaw=0.0, solid=True):
    """[wall] A front-loading laundry machine; the user stands in front (-y local)."""
    p = kit._P(B, x, y, z, yaw, "washer")
    p.kind = "wall"
    w, d, h = 0.62, 0.62, 0.95
    p.bev("CT_SteelWhite", -w / 2, w / 2, -d, 0.0, 0.0, h, 0.012)
    p.box("CT_SteelRed", -w / 2 + 0.04, w / 2 - 0.04, -d - 0.012, -d, 0.8, 0.9, f="y")
    p.box("CT_Steel", -0.06, 0.06, -d - 0.016, -d - 0.012, 0.84, 0.86, f="y")
    n = 10
    for rr, mat, dz in ((0.24, "CT_Aluminium", 0.012), (0.19, "CT_Steel", 0.02)):
        p.face(mat, [(rr * math.cos(2 * math.pi * k / n), -d - dz, 0.42 + rr * math.sin(2 * math.pi * k / n)) for k in range(n)])
    p.core(-w / 2 + 0.02, w / 2 - 0.02, -d + 0.02, -0.02, 0.1, h - 0.05)
    return p.done(solid=solid, top=h, front=0.8)


def _furnish_hall(B, mirror=False):
    """Round 3 (interior kit): the communal hall (x -34.6..-22.4, y 1.4..12.6, ceiling 6.4; coordinates below are the
    west variant, H mirrors them): canteen with a serving line, laundry on the end wall, a reading nook, the house
    committee notice board by the door, a gym corner at the far end."""
    H = _H(B, mirror)
    d = B.detail
    WARM = "#FFD2A0"
    # ---- canteen: six tables in two rows, the aisle from the door (x -29.8..-27.2) stays free
    n = 0
    rows = (5.0, 7.9)
    for y in rows:
        for x in (-32.0, -29.0, -26.0):
            n += 1
            H.p(kit.restaurant_table, x, y, 0.0, w=1.2, d=0.9, places=4, cloth=("CT_PlasterRed", "CT_PlasterBlue")[n % 2], seed=n)
            seats = [(x, y - 0.75, 180.0), (x, y + 0.75, 0.0)]       # a chair at -y faces +y (yaw 180)
            if x < -30:
                seats.append((x - 0.9, y, 90.0))
            if x > -27:
                seats.append((x + 0.9, y, 270.0))
            for k, (cx, cy, face) in enumerate(seats):
                H.p(kit.chair, cx, cy, face, mat=None if k < 2 else "CT_PlasterRed")
            H.thing("canteen_table_%d" % n, "seat", x, y - 0.75, 0.45, 180.0, height=0.45)
            H.spot("table%d" % n, "table", x, y - 0.75, 180.0)
            kit.pendant(B, H.X(x), y, 6.4, drop=1.5, kind="cone", light=True, lumens=900, colour=WARM, radius=6.5)
    # ---- the serving line along the back wall: counter, pots, trays, kitchen run behind
    cx = -29.2
    cr = H.p(kit.shop_counter, cx, 10.7, 0.0, w=7.4, d=0.8, front="CT_PlasterRed", top="CT_Aluminium", till=False)
    H.spot("counter1", "counter", cx, 10.1, 180.0)
    H.thing("canteen_counter", "counter", cx, 10.7, 1.05, 0.0, role="clerk", shop="club_buffet", service="bar")
    for k, px in enumerate((-32.2, -30.6, -29.0, -27.4)):
        H.p(kit.samovar if k == 1 else kit.teapot, px, 10.7, 0.0, z=1.05)
    for k in range(4):
        H.p(kit.plates, -26.0 + 0.4 * (k % 2), 10.55 + 0.2 * (k // 2), 0.0, z=1.05, n=4)
    H.p(kit.cup, -33.0, 10.7, 0.0, z=1.05)
    H.p(kit.cup, -32.7, 10.8, 0.0, z=1.05)
    H.p(kit.kitchen_unit, -31.2, 12.58, 0.0, modules="fridge,cupboard,stove,sink,cupboard", wall_cupboards=False)
    H.p(kit.kitchen_unit, -27.0, 12.58, 0.0, modules="cupboard,stove,sink,cupboard", wall_cupboards=False)
    H.p(kit.sign_plate, -29.2, 12.6, 0.0, name="sign_canteen", w=3.2, zc=4.3)
    H.p(kit.menu_board, -33.0, 12.6, 0.0, name="menu_0", w=0.9, zc=2.6)
    H.p(kit.menu_board, -25.4, 12.6, 0.0, name="menu_1", w=0.9, zc=2.6)
    H.p(kit.wall_clock, -29.2, 12.6, 0.0, zc=3.2, r=0.28)
    H.thing("canteen_menu", "board", -33.0, 12.5, 2.6, 0.0, topic="prices")
    # ---- laundry on the west wall: five machines, a folding table with baskets
    for i in range(5):
        y = 4.2 + i * 1.1
        H.p(_washer, -34.6, y, 90.0)
        H.spot("work%d" % (i + 1), "work", -33.5, y, 270.0)
        H.thing("washer_%d" % (i + 1), "machine", -34.3, y, 0.9, 90.0, what="washing_machine")
    H.p(kit.table, -33.3, 9.5, 90.0, w=1.6, d=0.8, kind="steel", mat="CT_Aluminium")
    H.p(kit.crate, -33.3, 9.9, 0.0, z=0.75, w=0.6, d=0.42, h=0.2, fill="CT_PlasterYellow", seed=1)
    H.p(kit.crate, -33.3, 9.1, 0.0, z=0.75, w=0.6, d=0.42, h=0.2, fill="CT_TileWhite", seed=2)
    H.thing("laundry_table", "craft", -33.3, 9.5, 0.8, 90.0, what="workbench")
    H.p(kit.notice_board, -34.6, 3.6, 90.0, w=0.8, zc=1.8, seed=5)
    # ---- reading nook in the front-left corner: bookcases on the front wall, two armchairs, a coffee table
    for k, (bx, tp) in enumerate(((-33.9, "poetry"), (-32.8, "local"), (-31.7, "kids"))):
        r = H.p(kit.bookshelf, bx, 1.4, 180.0, w=1.05, h=2.1, levels=5, seed=k + 3)
        H.thing("hall_shelf_%d" % (k + 1), "shelf", bx, 1.7, 1.3, 180.0, topic=tp)
    H.p(kit.rug, -32.6, 2.75, 0.0, w=2.6, d=1.5, name="rug_3")
    H.p(kit.coffee_table, -32.6, 2.75, 0.0, w=0.8, d=0.5)
    H.p(kit.books_stack, -32.8, 2.75, 0.0, z=0.42, n=3, seed=2)
    H.p(kit.cup, -32.3, 2.8, 0.0, z=0.42)
    H.p(kit.newspaper, -32.6, 2.55, 20.0, z=0.42)
    H.thing("hall_newspaper", "take", -32.6, 2.55, 0.45, 0.0, item="daily_paper")
    H.p(kit.armchair, -33.8, 2.8, 90.0, mat="CT_PlasterGreen")
    H.p(kit.armchair, -31.4, 2.8, 270.0, mat="CT_PlasterRed")
    H.thing("hall_armchair_1", "seat", -33.8, 2.8, 0.45, 90.0, height=0.45)
    H.thing("hall_armchair_2", "seat", -31.4, 2.8, 0.45, 270.0, height=0.45)
    H.spot("reading_1", "seat", -33.8, 2.8, 90.0)
    H.spot("reading_2", "seat", -31.4, 2.8, 270.0)
    H.p(kit.floor_lamp, -30.9, 1.85)
    # ---- house committee: notice board and wall newspaper by the door, a bench to read them
    H.p(kit.notice_board, -25.2, 1.4, 180.0, w=1.5, zc=1.8, seed=6)
    H.p(kit.poster, -23.9, 1.4, 180.0, name="poster_cons_3", w=0.55, zc=1.8)
    H.p(kit.bench, -25.2, 2.15, 0.0, w=1.6, kind="park")
    H.thing("house_committee_board", "board", -25.2, 1.5, 1.8, 180.0, topic="rules")
    H.thing("house_committee_bench", "seat", -25.2, 2.15, 0.45, 0.0, height=0.45)
    H.p(kit.poster, -30.55, 1.4, 180.0, name="poster_health", w=0.5, zc=1.8)
    H.thing("house_committee_wallpaper", "board", -30.55, 1.5, 1.8, 180.0, topic="health")
    # ---- gym corner (east end): mats, wall bars, a pull-up frame, a bench
    H.p(kit.mats, -23.6, 9.6, 0.0, w=2.0, d=3.0, n=2)
    for k, by in enumerate((5.6, 6.6, 7.6)):
        H.p(kit.wall_bars, -22.4, by, 270.0, w=0.9, h=2.6)
    H.thing("gym_mat", "sport", -23.6, 9.6, 0.3, 270.0, what="wall_bars")
    H.p(kit.bench, -25.0, 11.9, 0.0, w=1.6, kind="gym")
    mx0, mx1 = sorted((H.X(-23.9), H.X(-22.8)))
    box(d, "CT_Steel", mx0 - 0.04, mx0 + 0.04, 3.1, 3.18, 0.0, 2.4)
    box(d, "CT_Steel", mx1 - 0.04, mx1 + 0.04, 3.1, 3.18, 0.0, 2.4)
    box(d, "CT_SteelRed", min(mx0, mx1), max(mx0, mx1), 3.1, 3.18, 2.32, 2.4)
    H.thing("gym_bar", "sport", -23.35, 3.1, 1.6, 180.0, what="barbell")
    H.spot("gym_mat", "stand", -23.6, 9.6, 270.0)
    H.spot("gym_bar", "work", -23.35, 2.6, 180.0)
    H.p(kit.banner, -22.4, 10.5, 270.0, name="poster_sun_1", w=1.4, zc=3.4)
    # ---- plants in the corners, a mat at the door, the lamps (4 besides the pendants)
    H.p(kit.plant, -34.0, 12.0, kind="palm", pot="CT_PlasterYellow", h=1.9)
    H.p(kit.plant, -23.0, 12.0, kind="ficus", pot="CT_TileWhite", h=1.9)
    H.p(kit.plant, -30.2, 2.1, kind="fern", pot="CT_PlasterRed", h=1.1)
    H.p(kit.fire_extinguisher, -22.4, 2.0, 270.0)
    box(d, "CT_PlasterGrey", min(H.X(-29.8), H.X(-27.2)), max(H.X(-29.8), H.X(-27.2)), 1.4, 2.5, 0.0, 0.014, faces="Z")
    for lx, ly in ((-23.4, 6.5), (-28.5, 3.0), (-31.0, 10.0), (-26.5, 10.0)):
        H.lamp(lx, ly, 5.0)
