"""Cog club («Клуб-шестерня»): a workers' club made of a tall glazed cylindrical drum (foyer + spiral stair) with five
rectangular wing halls radiating from it like the teeth of a cog, at different heights. One wing is cantilevered over
the street entrance. A red stair tower stands behind the drum, big painted letters «КЛУБ» stand on a steel frame on
the drum roof. Original design in the spirit of the Moscow workers' clubs of the late 1920s (Melnikov, Golosov);
no Rusakov or Zuev club shape is used.

Plot 40 x 40 m, max 18 m. Drum centre (0, 19.5), 20 facets, circumradius 6.5 m, three storeys (floors at z 0, 4.4,
8.8; roof 13.2, red crown band to 13.7). Facet k is centred on 18k degrees (angle from +x, CCW; 270 = the street).
Wings (axis angle, 5.9 m wide, each opens onto 3 drum facets, door in the middle one):
  270 front   floor 8.8, roof 12.8, 17.8 m long, CANTILEVERED over the entrance (soffit 8.4 m): club room, ribbon
              windows, red end wall. Under it the porch and the drum's glazed street front with the main door.
  342 buffet  ground, roof 4.6, 16.5 m: buffet (furnished lightly), grey, big end window.
  54  stack   floors 0 / 4.4 / 8.8, roof 12.2, 15 m: club rooms (empty shell, lamps).
  126 studio  floors 0 / 4.4, roof 9.0, 15 m: grey, empty shell.
  198 reading ground, double height, roof 8.5, 17 m: READING / CHESS ROOM, furnished (tables, chairs, shelves, lamps).
Drum interior: spiral stair (54 steps, rise 0.163, going 0.30 at the walking line) round a central column from the
  floor to 8.8, ring floors at 4.4 and 8.8 with railings; ring doors into the wings at their floors.
Red stair tower (x +-1.8, y 27.6..34.9, 17.8 m) behind the drum, joined by a 1.5 m passage on the three floors; two
  straight flights per storey (14 steps) up to 8.8; service door in its east wall.
Roof frame on the drum: steel posts, braces and a white board with the red block letters «КЛУБ», read from the street.
Entrances: "main" (street, the glazed drum front under the cantilever, 1.7 m), "service" (tower, east wall).
Windows are all kind "public" (open hours), the tower slits "stair". Not residential: no flats.
Spots (yaw = direction the user faces): foyer benches, cloakroom "counter" + "work", "stand"; reading room "seat"s,
  "work" (librarian); buffet "table"/"seat", "counter", "work".
Ideas for later: a cinema hall in the 342 wing, furnished club rooms upstairs, a flag mast on the tower.
"""
import math

from citykit import box, prism, ring, band, band_down, ribbon_wall, quad_bar, DOOR_W
from _cons_util import wall, win, hface, rect
from _surrounds import add_surrounds
from furn_int2 import Furn
from furn_int3 import rod

INFO = {
    "name_ru": "Клуб «Шестерня»", "name_en": "Cog Club",
    "style": "constructivism", "use": "public",
    "inspired_by": "Moscow workers' clubs of 1925-30 (Melnikov, Golosov): a glazed cylinder of foyer and stair with "
                   "hall wings radiating at different heights, one cantilevered over the door, roof lettering, a red "
                   "stair tower; massing and details are our own",
}

CX, CY = 0.0, 19.5
R = 6.5
NF = 20                       # facets, facet k centred on 18 k degrees
ZS = (0.0, 4.4, 8.8)          # drum floors
ZROOF = 13.2
ZCROWN = 13.7
HW = R * math.sin(math.radians(27))          # wing half width
AV = R * math.cos(math.radians(27))          # axis distance of the wing wall start
APO = R * math.cos(math.radians(9))          # drum apothem
WT = 0.30
PUB = "public"

WINGS = [
    dict(id="front", ang=270, L=17.8, floors=[8.8], T=12.8, mat="CT_Plaster", end="CT_PlasterRed"),
    dict(id="buffet", ang=342, L=16.5, floors=[0.0], T=4.6, mat="CT_PlasterGrey", end="CT_PlasterGrey"),
    dict(id="stack", ang=54, L=15.0, floors=[0.0, 4.4, 8.8], T=12.2, mat="CT_Plaster", end="CT_PlasterGrey"),
    dict(id="studio", ang=126, L=15.0, floors=[0.0, 4.4], T=9.0, mat="CT_PlasterGrey", end="CT_Plaster"),
    dict(id="reading", ang=198, L=17.0, floors=[0.0], T=8.5, mat="CT_Plaster", end="CT_PlasterBlue"),
]


# ------------------------------------------------------------------ geometry helpers
def P(a, r):
    a = math.radians(a)
    return (CX + r * math.cos(a), CY + r * math.sin(a))


def ringpts(r, close=True):
    pts = [P(9 + 18 * j, r) for j in range(NF)]
    return pts + [pts[0]] if close else pts


def wing_frame(w):
    a = math.radians(w["ang"])
    return (math.cos(a), math.sin(a)), (-math.sin(a), math.cos(a))


def wpt(w, a, p):
    u, v = wing_frame(w)
    return (CX + a * u[0] + p * v[0], CY + a * u[1] + p * v[1])


def wing_poly(w):
    """Wing footprint (CCW): from the drum vertex at -27 deg out along the right wall, round the end, back along the
    left wall to the vertex at +27 deg, then along the drum facets (-> bulging into the wing) back."""
    g = w["ang"]
    return [P(g - 27, R), wpt(w, w["L"], -HW), wpt(w, w["L"], HW), P(g + 27, R), P(g + 9, R), P(g - 9, R)]


def facing_yaw(fx, fy):
    return math.degrees(math.atan2(fx, -fy)) % 360


def yaw_at(frm, to):
    return facing_yaw(to[0] - frm[0], to[1] - frm[1])


def facets_of(w):
    m = round(w["ang"] / 18)
    return {(m - 1) % NF, m % NF, (m + 1) % NF}


def storeys_of(w):
    """Drum storeys whose facets are walled by this wing (the wing roof reaches into the storey's spandrel)."""
    f0 = w["floors"][0]
    lo = max(i for i, z in enumerate(ZS) if z <= f0 + 0.01) if f0 > 0 else 0
    hi = max(i for i, z in enumerate(ZS) if z - 0.4 < w["T"] + 0.01 and z - 0.4 < w["T"] - 0.05) if False else None
    n = 0
    for i, z in enumerate(ZS):
        if i >= lo and w["T"] > z + 0.5:
            n = i
    return list(range(lo, n + 1))


def build(B):
    m, det = B.m, B.detail
    covered = {}                                   # (facet, storey) -> wing
    doors = {}                                     # (facet, storey) -> True
    for w in WINGS:
        st = storeys_of(w)
        w["cov"] = st
        for k in facets_of(w):
            for s in st:
                covered[(k, s)] = w
        k0 = round(w["ang"] / 18) % NF
        for f in w["floors"]:
            if f in ZS:
                doors[(k0, ZS.index(f))] = w
    # ------------------------------------------------------------ ground: plinth, floors
    drum = ringpts(R, close=False)
    hface(m, "CT_Floor", drum, 0.0, True)
    prism(m, "CT_ConcreteDark", drum, -0.4, 0.0, top=False, bottom=False)
    for w in WINGS:
        poly = wing_poly(w)
        hface(m, "CT_Paving" if w["id"] == "front" else "CT_Floor", poly, 0.0, True)
        if w["id"] != "front":
            prism(m, "CT_ConcreteDark", poly, -0.4, 0.0, top=False, bottom=False)
    # ------------------------------------------------------------ drum: ring floors, ceiling, roof
    inner = ringpts(3.0)
    outer = ringpts(R)
    for s in (1, 2):
        z = ZS[s]
        band(m, "CT_Floor", outer, inner, z)
        band_down(m, "CT_Plaster", outer, inner, z - 0.4)
        ribbon_wall(m, "CT_Concrete", inner[::-1], z - 0.4, z)
    hface(m, "CT_Interior", drum, ZROOF - 0.4, False)
    hface(m, "CT_Roof", drum, ZROOF, True)
    # ------------------------------------------------------------ drum facet walls
    for k in range(NF):
        p0, p1 = P(18 * k - 9, R), P(18 * k + 9, R)
        L = math.dist(p0, p1)
        for s in range(3):
            z0 = 0.0 if s == 0 else ZS[s] - 0.4
            z1 = ZS[s + 1] - 0.4 if s < 2 else ZROOF - 0.4
            cov = covered.get((k, s))
            dr = doors.get((k, s))
            main = (k == 15 and s == 0)
            if main:
                holes = [(0.165, L - 0.165, 0.03, 2.55), (0.165, L - 0.165, 2.7, 3.9)]
                wall(B, p0, p1, z0, z1 + 0.0, "CT_Plaster", holes=holes, depth=WT, mat_in="CT_Plaster")
                win(B, p0, p1, 0.165, L - 0.165, 2.7, 3.9, kind=PUB, cols=3, rows=1, inset=0.15, frame=0.05)
            elif cov is not None:
                holes = [(L / 2 - 0.65, L / 2 + 0.65, ZS[s] + 0.03, ZS[s] + 2.5)] if dr is not None else []
                wall(B, p0, p1, z0, z1, "CT_Interior", holes=holes, depth=WT, mat_in="CT_Interior")
            else:
                wall(B, p0, p1, z0, ZS[s] + 0.55, "CT_Concrete", depth=WT, mat_in="CT_Plaster")
                h1 = ZS[s] + 3.9 if s < 2 else ZROOF - 0.5
                wall(B, p0, p1, ZS[s] + 0.55, z1, "CT_Plaster", holes=[(0.14, L - 0.14, ZS[s] + 0.6, h1)], depth=WT,
                     mat_in="CT_Plaster")
                win(B, p0, p1, 0.14, L - 0.14, ZS[s] + 0.6, h1, kind=PUB, cols=2, rows=3, inset=0.14, frame=0.045)
        wall(B, p0, p1, ZROOF - 0.4, ZCROWN, "CT_PlasterRed", depth=WT, mat_in="CT_Concrete")
    # a ring of red spandrel fillets? (drum piers get a thin concrete cap)
    # ------------------------------------------------------------ the stair: central column + spiral
    _stair(B)
    # ------------------------------------------------------------ wings
    for w in WINGS:
        _wing(B, w)
    # ------------------------------------------------------------ entrance
    B.entrance("main", (CX, CY - APO, 0.0), 0.0, width=1.7, height=2.55, kind="street", leaves=2)
    # canopy-less: the cantilever is the canopy; a red ground-line beam on the porch floor edge
    box(det, "CT_SteelRed", -1.2, 1.2, CY - APO - 0.05, CY - APO + 0.0, 0.0, 0.0 + 0.001, faces="Z") if False else None
    # ------------------------------------------------------------ tower, letters, details
    _tower(B)
    _letters(B)
    # ------------------------------------------------------------ furnishing
    _foyer(B)
    _reading(B)
    _buffet(B)
    _lamps_misc(B)
    # ------------------------------------------------------------ footprint, height, surrounds
    fp = []
    for i, w in enumerate(sorted(WINGS, key=lambda w: w["ang"])):
        pass
    order = sorted(WINGS, key=lambda w: w["ang"])             # 54, 126, 198, 270, 342
    for w in order:
        g = w["ang"]
        fp += [P(g - 27, R), wpt(w, w["L"], -HW), wpt(w, w["L"], HW), P(g + 27, R)]
        if w["id"] == "studio" and False:
            pass
        if w["id"] == "stack":
            fp += [(1.0, 27.6), (1.8, 27.6), (1.8, 34.9), (-1.8, 34.9), (-1.8, 27.6), (-1.0, 27.6)]
    # wing "stack" (54) comes first in angle order, the tower sits between 54 and 126
    B.footprint = fp
    B.height = 17.8
    add_surrounds(B, bounds=(-19.0, 19.0, 0.3, 12.5), max_lamps=0, seed=3)
    B.note("drum with spiral stair, 5 wings at different heights, front wing cantilevered over the entrance")


# ------------------------------------------------------------------ the spiral stair
def _stair(B):
    m, det = B.m, B.detail
    rise = ZS[1] / 27.0
    a0 = -90.0
    da = 7.5
    ri, ro = 0.40, 2.80
    # central column (concrete) up to the roof
    B.column_round(CX, CY, 0.0, ZROOF - 0.4, 0.36, mat="CT_ConcreteWhite", segs=12)
    for i in range(54):
        z = (i + 1) * rise
        pa, pb = a0 + da * i, a0 + da * (i + 1)
        poly = [P(pa, ri), P(pa, ro), P(pb, ro), P(pb, ri)]
        prism(m, "CT_WoodLight", poly, z - 0.30, z, top=True, bottom=True, side_mat="CT_ConcreteDark")
    # handrail on the outer edge, open at the two arrivals
    arrive = {26, 53}
    for i in range(54):
        near = any(0 <= a - i <= 1 or 0 <= i - a <= 4 for a in arrive) and False
        skip = any(a - 3 <= i <= a + 0 for a in arrive) or i in (27, 28)
        z = (i + 1) * rise
        pm = a0 + da * (i + 0.5)
        x, y = P(pm, 2.72)
        if skip:
            continue
        box(det, "CT_Steel", x - 0.02, x + 0.02, y - 0.02, y + 0.02, z, z + 0.95)
        if i + 1 < 54 and (i + 1) not in arrive and not (i + 1 in (27, 28)) and not (i + 1 >= 23 and i + 1 <= 26 and False):
            x2, y2 = P(a0 + da * (i + 1.5), 2.72)
            z2 = (i + 2) * rise
            rod(det, "CT_Steel", (x, y, z + 0.95), (x2, y2, z2 + 0.95), 0.04)
    # ring floor railings (the ring edge at r 3.0), open near the arrivals
    for s, arr_ang in ((1, a0 + da * 27), (2, a0 + da * 54)):
        z = ZS[s]
        for j in range(NF):
            ang0, ang1 = 9 + 18 * j, 27 + 18 * j
            mid = (ang0 + ang1) / 2
            d = (mid - arr_ang + 180) % 360 - 180
            if -40 <= d <= 14:
                continue
            B.railing([P(ang0, 3.05), P(ang1, 3.05)], z, h=1.05, mat="CT_Steel", post=1.0)
    # chandelier ring round the column
    prism(det, "CT_Opal", ring(CX, CY, 0.55, 12), 11.3, 11.9, top=True, bottom=True)
    B.lamp((CX, CY, 11.6), lumens=1200, colour="#FFD9A8", kind=PUB, radius=9.0, fixture=False)


# ------------------------------------------------------------------ wings
def _ribbon(Ls, spans, u0, u1, seg=3.0, pier=0.45):
    """Window holes along a wall of length Ls: for each (z0, z1) a row of windows between u0 and u1."""
    n = max(1, int(round((u1 - u0 + pier) / (seg + pier))))
    wlen = (u1 - u0 - pier * (n - 1)) / n
    out = []
    for z0, z1 in spans:
        for i in range(n):
            a = u0 + i * (wlen + pier)
            out.append((a, a + wlen, z0, z1))
    return out


def _wing(B, w):
    m, det = B.m, B.detail
    g, L, T = w["ang"], w["L"], w["T"]
    fl = w["floors"]
    zlo = 0.0 if fl[0] == 0 else fl[0] - 0.4
    poly = wing_poly(w)
    # floors above ground, ceiling and roof
    for f in fl:
        if f > 0:
            hface(m, "CT_Floor", poly, f, True)
            hface(m, "CT_Plaster", poly, f - 0.4, False)
    hface(m, "CT_Interior", poly, T - 0.4, False)
    hface(m, "CT_Roof", poly, T, True)
    # window spans per wing
    side_len = L - AV
    tops = fl[1:] + [T]
    if w["id"] == "reading":
        spans = [(0.9, 3.5), (5.4, 7.6)]
    elif w["id"] == "buffet":
        spans = [(0.9, 3.6)]
    else:
        spans = [(f + 0.9, min(f + 3.1, t - 0.65)) for f, t in zip(fl, tops)]
    p_r0, p_r1 = P(g - 27, R), wpt(w, L, -HW)
    p_e0, p_e1 = p_r1, wpt(w, L, HW)
    p_l0, p_l1 = p_e1, P(g + 27, R)
    # side walls
    for (a, b) in ((p_r0, p_r1), (p_l0, p_l1)):
        Ls = math.dist(a, b)
        holes = _ribbon(Ls, spans, 0.9 if a is p_r0 else 0.5, Ls - (0.5 if a is p_r0 else 0.9))
        wall(B, a, b, zlo, T, w["mat"], holes=holes, depth=WT, mat_in="CT_Interior")
        for (u0, u1, z0, z1) in holes:
            win(B, a, b, u0, u1, z0, z1, kind=PUB, cols=3 if u1 - u0 > 2.5 else 2, rows=1, inset=0.16, frame=0.05)
    # end wall
    Le = math.dist(p_e0, p_e1)
    if w["id"] == "reading":
        eh = [(0.9, Le - 0.9, 0.9, 7.6)]
        ec, er = 4, 5
    elif w["id"] == "buffet":
        eh = [(0.7, Le - 0.7, 0.9, 3.6)]
        ec, er = 4, 1
    elif w["id"] == "front":
        eh = [(0.5, Le - 0.5, 9.7, 12.1)]
        ec, er = 5, 1
    else:
        eh = [(0.9, Le - 0.9, f + 0.9, min(f + 3.1, t - 0.65)) for f, t in zip(fl, tops)]
        ec, er = 3, 1
    wall(B, p_e0, p_e1, zlo, T, w["end"], holes=eh, depth=WT, mat_in="CT_Interior")
    for (u0, u1, z0, z1) in eh:
        win(B, p_e0, p_e1, u0, u1, z0, z1, kind=PUB, cols=ec, rows=er, inset=0.16, frame=0.05)
    # coping on the roof edge (red for the front wing)
    cop = "CT_SteelRed" if w["id"] == "front" else "CT_Concrete"
    path = [p_r0, p_r1, p_e1, p_l1]
    offs = _offset_edge(path, 0.12)
    for i in range(3):
        quad_bar(det, cop, path[i], path[i + 1], offs[i][0], offs[i][1], T, T + 0.22)
    # the front wing: deep fascia under the end and red soffit line, two concrete fins on the sides
    if w["id"] == "front":
        for sgn in (-1, 1):
            a = wpt(w, L - 0.0, sgn * (HW + 0.0))
        # a long red band under the end wall's ribbon, and fascia fins
        a, b = p_e0, p_e1
        quad_bar(det, "CT_SteelRed", a, b, _shift(a, b, 0.14)[0], _shift(a, b, 0.14)[1], 8.55, 9.35)
    # roof furniture
    if w["id"] == "buffet":
        c = wpt(w, 10.0, 0.0)
        box(m, "CT_Concrete", c[0] - 0.7, c[0] + 0.7, c[1] - 0.7, c[1] + 0.7, T, T + 1.0)
        box(det, "CT_Steel", c[0] - 0.8, c[0] + 0.8, c[1] - 0.8, c[1] + 0.8, T + 1.0, T + 1.08)
    if w["id"] == "studio":
        for a_ in (8.5, 10.5, 12.5):
            c = wpt(w, a_, 0.0)
            box(m, "CT_Concrete", c[0] - 0.5, c[0] + 0.5, c[1] - 0.5, c[1] + 0.5, T, T + 0.7)
    # door label: bench-high steel kick plate at the end of the stack wing
    # lamps in the empty wings
    for f in fl[:1] if w["id"] in ("stack", "studio", "front") else []:
        c = wpt(w, 10.5, 0.0)
        B.lamp((c[0], c[1], f + 3.4), lumens=700, colour="#FFD9A8", kind=PUB, radius=7.0)


def _shift(a, b, d):
    """The edge a->b moved d to its right (outside)."""
    dx, dy = b[0] - a[0], b[1] - a[1]
    L = math.hypot(dx, dy)
    nx, ny = dy / L * d, -dx / L * d
    return (a[0] + nx, a[1] + ny), (b[0] + nx, b[1] + ny)


def _offset_edge(path, d):
    return [_shift(path[i], path[i + 1], d) for i in range(len(path) - 1)]


# ------------------------------------------------------------------ red tower
TX0, TX1, TY0, TY1, TH = -1.8, 1.8, 27.6, 34.9, 17.8


def _tower(B):
    m, det = B.m, B.detail
    # the passage from the drum (facet 5, y 25.92) to the tower south wall
    yd = CY + APO
    for z in (0.0, 4.4, 8.8):
        if z > 0:
            box(m, "CT_Concrete", -0.75, 0.75, yd, TY0 + 0.1, z - 0.4, z)
        else:
            hface(m, "CT_Floor", rect(-0.75, 0.75, yd, TY0 + 0.1), 0.0, True)
    for sx in (-1, 1):
        x0, x1 = (0.75, 1.0) if sx > 0 else (-1.0, -0.75)
        box(m, "CT_PlasterRed", x0, x1, yd, TY0, 0.0, ZROOF - 0.4, faces="xXyYZ" if False else "all")
    box(m, "CT_PlasterRed", -1.0, 1.0, yd, TY0, ZROOF - 0.4, ZROOF, faces="all")
    for z in (4.4, 8.8):
        box(m, "CT_Interior", -0.75, 0.75, yd, TY0, z + 3.9, z + 4.0, faces="z") if False else None
    # tower walls
    cw = (TX0, TY0), (TX1, TY0), (TX1, TY1), (TX0, TY1)
    # south: link doors
    wall(B, cw[0], cw[1], 0.0, TH, "CT_PlasterRed",
         holes=[(1.15, 2.45, z + 0.03, z + 2.4) for z in (0.0, 4.4, 8.8)], depth=WT, mat_in="CT_Interior")
    # east: service door and slits
    east = [(0.45, 1.65, 0.03, 2.3)]
    slits_e = [(2.6, 3.2, z + 0.9, z + 3.4) for z in (0.0, 4.4, 8.8)] + [(5.2, 5.8, z + 0.9, z + 3.4) for z in (0.0, 4.4, 8.8)]
    wall(B, cw[1], cw[2], 0.0, TH, "CT_PlasterRed", holes=east + [(a, b, c, d) for a, b, c, d in slits_e], depth=WT,
         mat_in="CT_Interior")
    for (a, b, c, d) in slits_e:
        win(B, cw[1], cw[2], a, b, c, d, kind="stair", cols=1, rows=3, inset=0.16, frame=0.04)
    # north: tall glazed slit
    wall(B, cw[2], cw[3], 0.0, TH, "CT_PlasterRed", holes=[(1.4, 2.2, 1.0, 16.4)], depth=WT, mat_in="CT_Interior")
    win(B, cw[2], cw[3], 1.4, 2.2, 1.0, 16.4, kind="stair", cols=1, rows=9, inset=0.16, frame=0.05)
    # west: slits
    slits_w = [(2.6, 3.2, z + 0.9, z + 3.4) for z in (0.0, 4.4, 8.8)] + [(5.2, 5.8, z + 0.9, z + 3.4) for z in (0.0, 4.4, 8.8)]
    wall(B, cw[3], cw[0], 0.0, TH, "CT_PlasterRed", holes=slits_w, depth=WT, mat_in="CT_Interior")
    for (a, b, c, d) in slits_w:
        win(B, cw[3], cw[0], a, b, c, d, kind="stair", cols=1, rows=3, inset=0.16, frame=0.04)
    # floor, roof, landings, flights
    hface(m, "CT_Floor", rect(TX0 + WT, TX1 - WT, TY0 + WT, TY1 - WT), 0.0, True)
    tr = rect(TX0 - 0.15, TX1 + 0.15, TY0 - 0.15, TY1 + 0.15)
    prism(m, "CT_Concrete", tr, TH - 0.4, TH, top=False, bottom=True)
    hface(m, "CT_Roof", tr, TH, True)
    box(det, "CT_Steel", -1.0, 1.0, TY0 + 1.0, TY0 + 1.06, TH, min(TH + 0.9, 18.0))           # beacon mast
    ix0, ix1, iy0, iy1 = TX0 + WT, TX1 - WT, TY0 + WT, TY1 - WT
    y_flight0 = 29.5
    n, going = 14, 0.28
    rise = 4.4 / 28.0
    for s in (0, 1):
        zb = 4.4 * s
        # west flight up (+y), east flight back (-y)
        for i in range(n):
            top = zb + (i + 1) * rise
            ya, yb = y_flight0 + i * going, y_flight0 + (i + 1) * going
            box(m, "CT_Concrete", ix0, -0.1, ya, yb, zb, top)
        ztop = zb + n * rise
        box(m, "CT_Concrete", ix0, ix1, y_flight0 + n * going, iy1, ztop - 0.4, ztop)         # north landing
        for i in range(n):
            top = ztop + (i + 1) * rise
            ya, yb = y_flight0 + (n - 1 - i) * going, y_flight0 + (n - i) * going
            box(m, "CT_Concrete", 0.1, ix1, ya, yb, ztop, top)
        zf = zb + 4.4
        box(m, "CT_Concrete", ix0, ix1, iy0, y_flight0, zf - 0.4, zf)                       # south landing / floor
    B.railing([(-0.05, y_flight0 + 0.1), (-0.05, y_flight0 + n * going)], 0.0, h=0.0001, mat="CT_Steel") if False else None
    # service door
    B.entrance("service", (TX1, TY0 + 1.05, 0.0), 90.0, width=1.2, height=2.3, kind="service", leaves=1)
    B.lamp((0.0, 28.7, 3.5), lumens=500, colour="#FFD9A8", kind="stair", radius=6.0)


# ------------------------------------------------------------------ letters on the roof frame
FONT = {
    "К": ["X...X", "X..X.", "X.X..", "XX...", "X.X..", "X..X.", "X...X"],
    "Л": ["..XXX", "..X.X", "..X.X", ".X..X", ".X..X", "X...X", "X...X"],
    "У": ["X...X", "X...X", ".X.X.", "..X..", "..X..", ".X...", "X...."],
    "Б": ["XXXXX", "X....", "XXXX.", "X...X", "X...X", "X...X", "XXXX."],
}


def _letters(B):
    m, det = B.m, B.detail
    yf = CY - 1.6
    zb, zt = 13.75, 17.55
    # posts, rails, board, braces
    for x in (-5.4, 5.4):
        box(det, "CT_Steel", x - 0.1, x + 0.1, yf - 0.1, yf + 0.1, ZROOF, zt + 0.15)
        rod(det, "CT_Steel", (x, yf, zt - 0.2), (x * 0.85, yf + 2.6, ZROOF), 0.08)
        rod(det, "CT_Steel", (x, yf, zt - 0.2), (x * 0.85, yf - 1.8, ZROOF + 0.4), 0.08)
    box(det, "CT_Steel", -5.5, 5.5, yf - 0.1, yf + 0.1, zt, zt + 0.15)
    box(det, "CT_Steel", -5.5, 5.5, yf - 0.1, yf + 0.1, zb - 0.15, zb)
    box(det, "CT_Plaster", -5.3, 5.3, yf - 0.04, yf + 0.04, zb, zt)
    cell = 0.44
    gap = 0.45
    word = "КЛУБ"
    total = 4 * 5 * cell + 3 * gap
    x = -total / 2
    z_top = zt - 0.25
    for ch in word:
        rows = FONT[ch]
        for r, line in enumerate(rows):
            c = 0
            while c < 5:
                if line[c] == "X":
                    c2 = c
                    while c2 + 1 < 5 and line[c2 + 1] == "X":
                        c2 += 1
                    xa, xb = x + c * cell, x + (c2 + 1) * cell
                    za, zb2 = z_top - (r + 1) * cell, z_top - r * cell
                    box(det, "CT_SteelRed", xa, xb, yf - 0.14, yf - 0.04, za, zb2, faces="yxXzZ")
                    c = c2 + 1
                else:
                    c += 1
        x += 5 * cell + gap


# ------------------------------------------------------------------ interiors
WARM = "#FFD9A8"


def _foyer(B):
    f = Furn(B)
    det = B.detail
    # inlaid floor ring, pendants under the first ring floor
    band(det, "CT_PlasterRed", ringpts(3.5), ringpts(3.2), 0.012)
    band(det, "CT_ConcreteDark", ringpts(5.95), ringpts(5.75), 0.012)
    for a in (234, 306, 18, 162, 90):
        x, y = P(a, 4.4)
        f.pendant(x, y, 4.0, lumens=800, colour=WARM, radius=7.5, drop=0.5)
    # benches facing the glass
    for i, a in enumerate((234, 306, 18)):
        x, y = P(a, 5.3)
        yw = yaw_at((x, y), (CX, CY))
        f.bench(x, y, yaw=yw, w=1.8, back=True)
        B.spot("foyer_bench%d" % (i + 1), "bench", (x, y, 0.0), yw)
    # cloakroom counter on facet 9 (162 deg)
    cx, cy = P(162, 4.55)
    outw = (math.cos(math.radians(162)), math.sin(math.radians(162)))
    f.counter(cx, cy, 2.4, d=0.6, h=1.05, yaw=yaw_at((cx, cy), (CX, CY)) + 0.0, mat="CT_Wood", top="CT_Steel",
              front="CT_PlasterRed")
    B.spot("cloak", "counter", (cx - outw[0] * 0.9, cy - outw[1] * 0.9, 0.0), facing_yaw(*outw))
    sx, sy = cx + outw[0] * 0.95, cy + outw[1] * 0.95
    B.spot("cloak_staff", "work", (sx, sy, 0.0), facing_yaw(-outw[0], -outw[1]))
    for j in (-1, 1):                                            # hooks rail behind the counter on the wall
        px, py = P(162 + j * 4.5, 6.08)
        box(det, "CT_Wood", px - 0.4, px + 0.4, py - 0.05, py + 0.05, 1.7, 1.78) if False else None
    # plants, standing spots
    for a in (250, 290):
        x, y = P(a, 5.5)
        f.plant(x, y, h=1.7)
    for a in (150, 30):
        x, y = P(a + 0.0, 5.7)
        f.plant(x, y, h=1.4, pot="CT_TileWhite")
    B.spot("foyer_stand1", "stand", (CX + 0.0, CY - 4.2, 0.0), 0.0 if False else 180.0)
    B.spot("foyer_stand2", "stand", P(50, 4.4) + (0.0,), yaw_at(P(50, 4.4), (CX, CY)))
    B.spot("stair_up", "stand", P(-90, 3.4) + (0.0,), 180.0)
    # posters on the solid facets
    for i, a in enumerate((36, 72, 108, 144, 180, 216, 324, 0)):
        x, y = P(a, 6.1)
        yw = yaw_at((x, y), (CX, CY))
        f.panel(x, y, 1.75, 1.3, 1.6, yaw=yw, mat=("CT_PlasterRed", "CT_PlasterYellow", "CT_Steel", "CT_PlasterBlue")[i % 4],
                frame="CT_Frame")
    # upper ring: benches and lamps
    for s in (1, 2):
        z = ZS[s]
        for a in (234, 306):
            x, y = P(a, 5.3)
            yw = yaw_at((x, y), (CX, CY))
            Furn(B, z0=z).bench(x, y, yaw=yw, w=1.8, back=True)
            B.spot("ring%d_bench_%d" % (s, a), "bench", (x, y, z), yw)
    for a in (270, 90):
        x, y = P(a, 4.6)
        B.lamp((x, y, ZS[2] + 3.3), lumens=800, colour=WARM, kind=PUB, radius=7.5) if a == 270 else None
    x, y = P(0, 4.6)
    B.lamp((x, y, ZS[1] + 3.4), lumens=800, colour=WARM, kind=PUB, radius=7.5)


def _reading(B):
    w = [w for w in WINGS if w["id"] == "reading"][0]
    f = Furn(B)
    u, v = wing_frame(w)
    L = w["L"]

    def W(a, p):
        return wpt(w, a, p)

    def fy(fa, fp):
        return facing_yaw(fa * u[0] + fp * v[0], fa * u[1] + fp * v[1])
    # floor border and rug
    def rugbox(a0, a1, p0, p1, mat, z=0.012):
        pts = [W(a0, p0), W(a1, p0), W(a1, p1), W(a0, p1)]
        # corners in CCW order (the frame is right-handed: a along u, p along v)
        for tri in ((0, 1, 2), (0, 2, 3)):
            B.detail.face(mat, [(pts[i][0], pts[i][1], z) for i in tri])
    rugbox(7.3, 13.4, -1.4, 1.4, "CT_PlasterRed")
    rugbox(14.2, 16.2, -2.4, 2.4, "CT_PlasterBlue")
    # long reading tables, chairs on both long sides
    for ti, a in enumerate((8.6, 11.9)):
        x, y = W(a, 0.0)
        f.table(x, y, w=2.6, d=1.0, h=0.76, top="CT_WoodLight", yaw=w["ang"])
        for off in (-0.8, 0.0, 0.8):
            for sgn in (-1, 1):
                cx, cy = W(a + off, sgn * 0.88)
                yw = fy(0, -sgn)                               # the sitter faces the table
                f.chair(cx, cy, yaw=yw, seat="CT_Wood")
                B.spot("read_t%d_%d_%d" % (ti, int(off * 10), sgn), "seat", (cx, cy, 0.0), yw)
        # a reading lamp on the table
        lx, ly = W(a, 0.0)
        box(B.detail, "CT_Steel", lx - 0.02, lx + 0.02, ly - 0.02, ly + 0.02, 0.76, 1.05)
        box(B.detail, "CT_Opal", lx - 0.1, lx + 0.1, ly - 0.1, ly + 0.1, 1.05, 1.15)
        for q in (-0.5, 0.5):                                  # papers and books
            bx, by = W(a + q * 1.6, 0.0)
            box(B.detail, ("CT_PlasterRed", "CT_PlasterBlue")[(ti + (q > 0)) % 2], bx - 0.18, bx + 0.18, by - 0.14, by + 0.14, 0.76, 0.79)
    # chess tables
    for ci, p in enumerate((-1.3, 1.3)):
        x, y = W(15.3, p)
        f.table(x, y, w=0.9, d=0.9, h=0.74, top="CT_WoodLight", yaw=w["ang"], leg="CT_Steel")
        for k in range(8):                                    # a 4x4 checker on the top
            for l in range(8):
                pass
        for i in range(4):
            for j in range(4):
                if (i + j) % 2:
                    c = W(15.3 - 0.3 + i * 0.15 + 0.075, p - 0.3 + j * 0.15 + 0.075)
                    box(B.detail, "CT_Steel", c[0] - 0.06, c[0] + 0.06, c[1] - 0.06, c[1] + 0.06, 0.74, 0.745)
        for sgn in (-1, 1):
            cx, cy = W(15.3 + sgn * 0.78, p)
            yw = fy(-sgn, 0)
            f.chair(cx, cy, yaw=yw, seat="CT_PlasterRed")
            B.spot("chess%d_%d" % (ci, sgn), "seat", (cx, cy, 0.0), yw)
        B.lamp((x, y, 3.3), lumens=500, colour="#FFE2B5", kind=PUB, radius=5.0) if ci == 0 else None
    # shelves along both long walls (facing the room)
    for sgn in (-1, 1):
        for k, a in enumerate((8.0, 9.7, 11.4, 13.1)):
            x, y = W(a, sgn * 2.38)
            f.shelf(x, y, w=1.6, d=0.4, h=2.2, yaw=fy(0, -sgn) + 0.0 if False else yaw_at((x, y), W(a, 0.0)) , rows=5, seed=k + 3 * (sgn > 0))
    # librarian desk and a newspaper stand at the end wall, facing the room (the drum)
    x, y = W(16.0, 0.0)
    f.desk(x, y, yaw=fy(-1, 0), w=1.6, d=0.7)
    B.spot("librarian", "work", W(16.55, 0.0) + (0.0,), fy(-1, 0))
    cx, cy = W(15.4, 0.0)
    f.chair(*W(16.55, 0.0), yaw=fy(-1, 0), seat="CT_PlasterBlue")
    for sgn in (-1, 1):
        x, y = W(16.4, sgn * 2.0)
        f.plant(x, y, h=1.7)
    # card catalogue cabinet and wall boards
    x, y = W(7.5, 2.4)
    f.cabinet(x, y, yaw=yaw_at((x, y), W(7.5, 0.0)))
    x, y = W(7.5, -2.4)
    f.cabinet(x, y, yaw=yaw_at((x, y), W(7.5, 0.0)))
    for sgn, mat in ((-1, "CT_PlasterYellow"), (1, "CT_PlasterRed")):
        x, y = W(14.3, sgn * 2.62)
        f.panel(x, y, 1.9, 1.6, 1.0, yaw=yaw_at((x, y), W(14.3, 0.0)), mat=mat, frame="CT_Wood")
    # pendant lamps on long cords from the 8.1 m ceiling
    for a in (8.4, 11.2, 14.0, 16.0):
        for p in ((0.0,) if a != 14.0 else (-1.3, 1.3)):
            x, y = W(a, p)
            f.pendant(x, y, 8.1, lumens=900, colour=WARM, radius=7.5, drop=4.5)
    # reading-room reading spots for passers-by
    B.spot("read_stand", "stand", W(6.9, 0.0) + (0.0,), fy(1, 0))


def _buffet(B):
    w = [w for w in WINGS if w["id"] == "buffet"][0]
    f = Furn(B)
    u, v = wing_frame(w)

    def W(a, p):
        return wpt(w, a, p)

    def fy(fa, fp):
        return facing_yaw(fa * u[0] + fp * v[0], fa * u[1] + fp * v[1])
    for i, (a, p) in enumerate(((9.0, -1.4), (9.0, 1.4), (12.0, -1.4), (12.0, 1.4))):
        x, y = W(a, p)
        f.round_table(x, y, r=0.55, h=0.75, top="CT_WoodLight")
        B.spot("buffet_t%d" % i, "table", (x, y, 0.0), 0.0)
        for k in range(3):
            ang = math.radians(90 + 120 * k)
            cx, cy = x + 0.95 * math.cos(ang), y + 0.95 * math.sin(ang)
            yw = yaw_at((cx, cy), (x, y))
            f.chair(cx, cy, yaw=yw, seat="CT_PlasterRed" if i % 2 else "CT_Wood")
            B.spot("buffet_s%d_%d" % (i, k), "seat", (cx, cy, 0.0), yw)
        f.cup(x, y, 0.75, mat="CT_PlasterBlue", h=0.16)
    x, y = W(15.5, 0.0)
    f.counter(x, y, 3.4, d=0.7, h=1.05, yaw=fy(-1, 0), mat="CT_Wood", top="CT_Steel", front="CT_PlasterRed")
    B.spot("buffet_counter", "counter", W(14.6, 0.0) + (0.0,), fy(1, 0))
    B.spot("buffet_staff", "work", W(16.2, 0.0) + (0.0,), fy(-1, 0))
    for a in (9.5, 13.5):
        x, y = W(a, 0.0)
        f.pendant(x, y, 4.2, lumens=800, colour=WARM, radius=7.0, drop=0.9)
    for sgn in (-1, 1):
        x, y = W(16.2, sgn * 2.2)
        f.plant(x, y, h=1.5)


def _lamps_misc(B):
    pass
