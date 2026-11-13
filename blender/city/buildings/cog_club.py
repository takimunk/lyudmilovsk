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
import interior_kit as kit
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
    # ------------------------------------------------------------ tower, letters, details
    _tower(B)
    _letters(B)
    # ------------------------------------------------------------ furnishing
    _foyer(B)
    _reading(B)
    _buffet(B)
    _stack(B)
    _studio(B)
    _front_room(B)
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


# ------------------------------------------------------------------ interiors (furnished with interior_kit)
WARM = "#FFD9A8"
INNER = HW - WT                    # the inner face of a wing's side walls (p = +-2.65)


class Ctx:
    """A wing's own frame: a = metres from the drum centre along the axis, p = across (left of the axis > 0)."""

    def __init__(self, B, wid, z=0.0):
        self.B, self.z = B, z
        self.w = [w for w in WINGS if w["id"] == wid][0]
        self.u, self.v = wing_frame(self.w)
        self.ang = self.w["ang"]
        self.L = self.w["L"]

    def W(self, a, p):
        return wpt(self.w, a, p)

    def fy(self, fa, fp):
        return facing_yaw(fa * self.u[0] + fp * self.v[0], fa * self.u[1] + fp * self.v[1])

    def put(self, fn, a, p, face=(-1, 0), z=None, **kw):
        x, y = self.W(a, p)
        return fn(self.B, x, y, self.z if z is None else z, self.fy(*face), **kw)

    def along(self, fn, a, p, **kw):
        """A piece whose long side (local x) runs along the wing axis; its front faces -v."""
        x, y = self.W(a, p)
        return fn(self.B, x, y, self.z, self.ang, **kw)

    def spot(self, sid, kind, a, p, face):
        x, y = self.W(a, p)
        self.B.spot(sid, kind, (x, y, self.z), self.fy(*face))

    def thing(self, tid, kind, a, p, h, face, **props):
        x, y = self.W(a, p)
        self.B.thing(tid, kind, (x, y, self.z + h), self.fy(*face), **props)

    def wall_side(self, fn, a, side, zc=None, **kw):
        """A wall piece on the inner face of the side wall (side = +1 / -1), looking into the room."""
        x, y = self.W(a, side * (INNER + 0.0))
        extra = {} if zc is None else {"zc": zc}
        extra.update(kw)
        return fn(self.B, x, y, self.z, self.fy(0, -side), **extra)

    def wall_end(self, fn, p, zc=None, **kw):
        x, y = self.W(self.L - WT, p)
        extra = {} if zc is None else {"zc": zc}
        extra.update(kw)
        return fn(self.B, x, y, self.z, self.fy(-1, 0), **extra)


def _ring_pt(a, r):
    x, y = P(a, r)
    return x, y, yaw_at((x, y), (CX, CY))


def _foyer(B):
    det = B.detail
    band(det, "CT_PlasterRed", ringpts(3.5), ringpts(3.2), 0.012)
    band(det, "CT_ConcreteDark", ringpts(5.95), ringpts(5.75), 0.012)
    # pendants under the first ring floor (the same five lamps as before)
    for a in (234, 306, 18, 162, 90):
        x, y = P(a, 4.4)
        kit.pendant(B, x, y, 4.0, drop=0.5, kind="opal", light=True, lumens=800, colour=WARM, radius=7.5)
    # benches facing the middle
    for i, a in enumerate((234, 306, 18)):
        x, y, yw = _ring_pt(a, 5.3)
        kit.bench(B, x, y, 0.0, yw, w=1.8, kind="park")
        B.spot("foyer_bench%d" % (i + 1), "bench", (x, y, 0.0), yw)
        B.thing("foyer_seat_%d" % (i + 1), "seat", (x, y, 0.45), yw, height=0.45)
    # cloakroom on facet 9 (162 deg): counter, coats behind, a hat stand
    cx, cy, cyaw = _ring_pt(162, 4.6)
    kit.shop_counter(B, cx, cy, 0.0, cyaw, w=1.9, d=0.6, h=1.05, top="CT_WoodLight", front="CT_PlasterRed", till=False)
    rx, ry, ryaw = _ring_pt(162, 5.8)
    kit.coat_rack(B, rx, ry, 0.0, ryaw, w=1.8, coats=0.8, seed=8)
    outw = (math.cos(math.radians(162)), math.sin(math.radians(162)))
    B.spot("cloak", "counter", (cx - outw[0] * 0.9, cy - outw[1] * 0.9, 0.0), facing_yaw(*outw))
    B.spot("cloak_staff", "work", (cx + outw[0] * 0.75, cy + outw[1] * 0.75, 0.0), facing_yaw(-outw[0], -outw[1]))
    B.thing("cloak", "counter", (cx, cy, 1.05), cyaw, role="clerk", service="cloakroom")
    sx, sy, syaw = _ring_pt(162, 5.9)
    kit.sign_plate(B, *P(162, 6.1), 0.0, yaw_at(P(162, 6.1), (CX, CY)), name="sign_cloakroom", w=1.3, zc=2.55)
    # plants
    for a, r, kind, h in ((250, 5.5, "ficus", 1.7), (290, 5.5, "ficus", 1.7), (100, 5.7, "monstera", 1.4),
                          (42, 5.7, "palm", 1.8)):
        x, y = P(a, r)
        kit.plant(B, x, y, 0.0, 0.0, kind=kind, pot="CT_PlasterRed" if a > 200 else "CT_TileWhite", h=h, seed=a)
    B.spot("foyer_stand1", "stand", (CX, CY - 4.2, 0.0), 180.0)
    B.spot("foyer_stand2", "stand", P(50, 4.4) + (0.0,), yaw_at(P(50, 4.4), (CX, CY)))
    B.spot("stair_up", "stand", P(-90, 3.4) + (0.0,), 180.0)
    # pictures on the wall facets (solid ones; every one different)
    pics = {36: ("poster", "poster_cons_0"), 72: ("poster", "poster_cons_1"), 108: ("poster", "poster_cons_2"),
            144: ("notice", None), 180: ("picture", "painting_sea"), 216: ("poster", "poster_sun_1"),
            324: ("poster", "poster_safety"), 0: ("poster", "poster_health")}
    for a, (kind, nm) in pics.items():
        px, py = P(a, 6.1)
        yw = yaw_at((px, py), (CX, CY))
        if kind == "poster":
            kit.poster(B, px, py, 0.0, yw, name=nm, w=0.9, zc=1.8)
        elif kind == "picture":
            kit.picture(B, px, py, 0.0, yw, name=nm, w=1.0, zc=1.75)
        else:
            kit.notice_board(B, px, py, 0.0, yw, w=1.1, zc=1.6, seed=5)
            B.thing("foyer_lost", "board", (px - math.cos(math.radians(a)) * 0.05, py - math.sin(math.radians(a)) * 0.05, 1.6),
                    yw, topic="lost")
    kit.wall_clock(B, *P(18 + 18 * 0 + 0, 6.1), 0.0, yaw_at(P(18, 6.1), (CX, CY)), zc=2.6, face="clock_0")
    # upper ring floors: benches
    for s in (1, 2):
        z = ZS[s]
        for a in (234, 306):
            x, y, yw = _ring_pt(a, 5.3)
            kit.bench(B, x, y, z, yw, w=1.8, kind="park")
            B.spot("ring%d_bench_%d" % (s, a), "bench", (x, y, z), yw)
            B.thing("ring%d_seat_%d" % (s, a), "seat", (x, y, z + 0.45), yw, height=0.45)
    for a in (270, 90):
        x, y = P(a, 4.6)
        B.lamp((x, y, ZS[2] + 3.3), lumens=800, colour=WARM, kind=PUB, radius=7.5) if a == 270 else None
    x, y = P(0, 4.6)
    B.lamp((x, y, ZS[1] + 3.4), lumens=800, colour=WARM, kind=PUB, radius=7.5)


def _reading(B):
    c = Ctx(B, "reading")
    # rugs
    c.along(kit.rug, 10.4, 0.0, w=6.2, d=2.8, name="rug_5")
    c.along(kit.rug, 15.2, 0.0, w=2.2, d=4.6, name="rug_0")
    # long reading tables, three chairs a side
    for ti, a in enumerate((8.6, 11.9)):
        r = c.along(kit.reading_table, a, 0.0, w=2.6, d=1.0, h=0.76, lamps=2, light=False)
        for off in (-0.8, 0.0, 0.8):
            for sgn in (-1, 1):
                x, y = c.W(a + off, sgn * 0.88)
                yw = c.fy(0, -sgn)
                kit.chair(B, x, y, 0.0, yw, kind="wood")
                B.spot("read_t%d_%d_%d" % (ti, int(off * 10), sgn), "seat", (x, y, 0.0), yw)
        sx, sy = c.W(a, 0.88)
        B.thing("read_table_%d_a" % (ti + 1), "seat", (sx, sy, 0.45), c.fy(0, -1), height=0.45)
        sx, sy = c.W(a, -0.88)
        B.thing("read_table_%d_b" % (ti + 1), "seat", (sx, sy, 0.45), c.fy(0, 1), height=0.45)
        c.put(kit.books_stack, a - 0.8, 0.0, z=0.76, n=3, seed=ti)
        c.put(kit.newspaper, a + 0.8, 0.25, z=0.76, seed=ti)
    # chess tables
    for ci, p in enumerate((-1.3, 1.3)):
        x, y = c.W(15.3, p)
        kit.chess_table(B, x, y, 0.0, c.ang, seed=ci)
        for sgn in (-1, 1):
            cx, cy = c.W(15.3 + sgn * 0.78, p)
            yw = c.fy(-sgn, 0)
            kit.chair(B, cx, cy, 0.0, yw, kind="wood")
            B.spot("chess%d_%d" % (ci, sgn), "seat", (cx, cy, 0.0), yw)
        B.thing("chess_%d" % ci, "play", (x, y, 0.72), c.fy(-1, 0), what="chess")
    # bookshelves along both walls (topics!)
    topics = {1: ["local", "birds", "sea", "science"], -1: ["poetry", "art", "chess", "engineering"]}
    for sgn in (-1, 1):
        for k, a in enumerate((8.0, 9.7, 11.4, 13.1)):
            tp = topics[sgn][k]
            x, y = c.W(a, sgn * INNER)
            kit.bookshelf(B, x, y, 0.0, c.fy(0, -sgn), w=1.5, h=2.2, topic=("library", "old", "collected", "modern", "paperback", "encyclopaedia")[(k + 2 * (sgn > 0)) % 6], seed=k + 4 * (sgn > 0))
            B.thing("stack_%s_%d" % ("l" if sgn > 0 else "r", k + 1), "shelf", (x + c.v[0] * -sgn * 0.2, y + c.v[1] * -sgn * 0.2, 1.3),
                    c.fy(0, -sgn), topic=tp)
    # librarian desk and chair at the end, facing the drum
    c.put(kit.lending_desk, 15.7, 0.0, w=1.6, d=0.7)
    kit.chair(B, *c.W(16.4, 0.0), 0.0, c.fy(-1, 0), kind="school")
    c.spot("librarian", "work", 16.4, 0.0, (-1, 0))
    c.thing("library_desk", "counter", 15.7, 0.0, 1.0, (-1, 0), role="clerk", service="lending")
    c.put(kit.table_lamp, 15.7, 0.3, z=1.0)
    # card catalogues by the door
    c.put(kit.card_catalogue, 7.5, 2.1, face=(0, -1))
    c.put(kit.card_catalogue, 7.5, -2.1, face=(0, 1))
    c.thing("card_catalogue", "shelf", 7.5, 2.1, 1.1, (0, -1), topic="local")
    # boards, plants, wall decor
    c.wall_side(kit.notice_board, 14.8, 1, zc=1.7, w=1.0, seed=2)
    c.wall_side(kit.picture, 14.8, -1, zc=1.7, name="painting_sea", w=0.9)
    c.wall_side(kit.wall_clock, 7.2, 1, zc=3.2, face="clock_3")
    c.wall_end(kit.sign_plate, 0.0, zc=3.4, name="sign_library", w=2.2)
    c.wall_end(kit.sign_plate, 1.9, zc=2.6, name="sign_quiet", w=0.9)
    sx, sy = c.W(c.L - WT, 1.0)
    B.thing("club_notices", "board", (sx, sy, 1.7), c.fy(0, -1), topic="club")
    for sgn in (-1, 1):
        c.put(kit.plant, 16.4, sgn * 2.0, kind="ficus", pot="CT_PlasterBlue", h=1.7, seed=sgn)
    # pendants on long cords from the 8.1 m ceiling (same five lamps)
    for a, pp in ((8.4, 0.0), (11.2, 0.0), (14.0, -1.3), (14.0, 1.3), (16.0, 0.0)):
        x, y = c.W(a, pp)
        kit.pendant(B, x, y, 8.1, drop=4.5, kind="green", light=True, lumens=900, colour=WARM, radius=7.5)
    x, y = c.W(15.3, -1.3)
    B.lamp((x, y, 3.3), lumens=500, colour="#FFE2B5", kind=PUB, radius=5.0)
    B.spot("read_stand", "stand", c.W(6.9, 0.0) + (0.0,), c.fy(1, 0))


def _buffet(B):
    c = Ctx(B, "buffet")
    c.along(kit.rug, 10.0, 0.0, w=5.0, d=3.2, name="rug_2")
    for i, (a, p) in enumerate(((8.8, -1.4), (8.8, 1.4), (11.4, -1.4), (11.4, 1.4))):
        x, y = c.W(a, p)
        kit.table_round(B, x, y, 0.0, 0.0, r=0.55, h=0.75, mat="CT_WoodLight")
        B.spot("buffet_t%d" % i, "table", (x, y, 0.0), 0.0)
        for k in range(3):
            ang = math.radians(90 + 120 * k)
            cx, cy = x + 0.95 * math.cos(ang), y + 0.95 * math.sin(ang)
            yw = yaw_at((cx, cy), (x, y))
            kit.chair(B, cx, cy, 0.0, yw, kind="tube", mat="CT_PlasterRed" if i % 2 else "CT_PlasterBlue")
            B.spot("buffet_s%d_%d" % (i, k), "seat", (cx, cy, 0.0), yw)
        kit.cup(B, x - 0.1, y, 0.75, 0.0)
        kit.vase(B, x + 0.12, y + 0.08, 0.75, 0.0, seed=i)
        cx, cy = x + 0.95 * math.cos(math.radians(90)), y + 0.95
        B.thing("buffet_seat_%d" % (i + 1), "seat", (cx, cy, 0.45), yaw_at((cx, cy), (x, y)), height=0.45)
    c.put(kit.bar_counter, 14.3, 0.0, w=3.4, d=0.65, h=1.05, taps=0, back=True, seed=2, front="CT_PlasterRed", top="CT_WoodLight")
    c.put(kit.samovar, 13.9, -1.2, z=1.05)
    for k in range(3):
        c.put(kit.cup, 14.0, 0.6 + 0.2 * k, z=1.05)
    c.put(kit.plates, 14.0, -0.2, z=1.05, n=3)
    c.put(kit.cash_register, 14.4, 1.2, z=1.05)
    c.spot("buffet_counter", "counter", 13.5, 0.0, (1, 0))
    c.spot("buffet_staff", "work", 15.1, 0.0, (-1, 0))
    c.thing("buffet_counter", "counter", 14.3, 0.0, 1.05, (-1, 0), role="clerk", service="bar", shop="club_buffet")
    c.wall_end(kit.menu_board, 2.1, zc=1.9, name="menu", w=0.7)
    c.wall_end(kit.sign_plate, -1.0, zc=2.9, name="sign_canteen", w=1.6)
    for sgn in (-1, 1):
        c.put(kit.plant, 15.9, sgn * 2.2, kind="fern" if sgn > 0 else "ficus", pot="CT_PlasterYellow", h=1.2, seed=sgn)
    for a in (9.5, 13.5):
        x, y = c.W(a, 0.0)
        kit.pendant(B, x, y, 4.2, drop=0.9, kind="cone", light=True, lumens=800, colour=WARM, radius=7.0)


def _stack(B):
    """Wing 54, three storeys: games (ground), model-makers (4.4), photo lab (8.8)."""
    # ---------------------------------------------------------------- ground: billiards + table tennis
    c = Ctx(B, "stack", 0.0)
    c.along(kit.rug, 8.6, 0.0, w=4.4, d=3.0, name="rug_4")
    c.along(kit.billiard_table, 8.6, 0.0)
    c.along(kit.pingpong_table, 12.4, 0.0)
    x, y = c.W(8.6, 0.0)
    B.thing("billiards_1", "play", (x, y, 0.82), c.ang, what="billiards")
    x, y = c.W(12.4, 0.0)
    B.thing("table_tennis_1", "play", (x, y, 0.76), c.ang, what="table_tennis")
    for k, (a, sgn) in enumerate(((7.2, 1), (10.5, 1), (10.5, -1), (14.0, -1))):
        x, y = c.W(a, sgn * (INNER - 0.25))
        kit.bench(B, x, y, 0.0, c.fy(0, -sgn), w=1.5, kind="park")
        B.thing("games_seat_%d" % (k + 1), "seat", (x, y, 0.45), c.fy(0, -sgn), height=0.45)
    c.wall_side(kit.poster, 13.0, 1, zc=1.8, name="poster_birds_2", w=0.9)
    c.wall_side(kit.notice_board, 9.2, -1, zc=1.7, w=1.1, seed=9)
    c.wall_side(kit.poster, 11.8, -1, zc=1.8, name="poster_sun_2", w=1.0)
    c.wall_end(kit.banner, 0.0, zc=2.1, name="poster_cons_3", w=1.3)
    c.wall_side(kit.wall_clock, 7.0, -1, zc=3.0, face="clock_2")
    c.put(kit.plant, 14.3, 1.9, kind="palm", pot="CT_PlasterBlue", h=1.9)
    for a in (8.6, 12.4):
        x, y = c.W(a, 0.0)
        kit.pendant(B, x, y, 4.0, drop=0.7, kind="green", light=False)
    c.thing("games_board", "board", 9.2, -INNER, 1.7, (0, 1), topic="club")
    # ---------------------------------------------------------------- 4.4: the model-makers' workshop
    z = 4.4
    c = Ctx(B, "stack", z)
    for k, a in enumerate((8.4, 11.6)):
        c.along(kit.table, a, 0.0, w=2.4, d=0.9, h=0.8, mat="CT_WoodLight", kind="wood")
        c.put(kit.tools, a - 0.5, 0.15, z=z + 0.8)
        c.put(kit.paint_jars, a + 0.5, -0.2, z=z + 0.8, n=5, seed=k)
        c.put(kit.books_stack, a + 0.1, 0.25, z=z + 0.8, n=2, seed=k + 2)
        c.put(kit.table_lamp, a - 0.9, -0.2, z=z + 0.8)
        for sgn in (-1, 1):
            x, y = c.W(a + 0.4 * sgn, 0.0)
            for s2 in (-1, 1):
                sx, sy = c.W(a + 0.45 * sgn, s2 * 0.85)
                kit.stool(B, sx, sy, z, 0.0)
        x, y = c.W(a, 0.0)
        B.thing("workbench_%d" % (k + 1), "craft", (x, y, z + 0.8), c.ang, what="workbench")
    c.put(kit.drafting_table, 13.9, 0.2, face=(-1, 0))
    x, y = c.W(13.9, 0.2)
    B.thing("drafting_board", "craft", (x, y, z + 0.9), c.fy(-1, 0), what="workbench")
    for k, a in enumerate((8.3, 10.8, 13.3)):
        x, y = c.W(a, INNER - 0.32)
        kit.display_case(B, x, y, z, c.fy(0, -1), w=1.4, d=0.5, goods="toys", seed=k)
    x, y = c.W(10.8, INNER - 0.32)
    B.thing("model_shelf", "shelf", (x, y, z + 1.0), c.fy(0, -1), topic="engineering")
    kit.lockers(B, *c.W(14.0, -INNER + 0.25), z, c.fy(0, 1), n=3)
    c.wall_side(kit.notice_board, 9.8, -1, zc=z + 1.7, w=1.2, seed=4)
    c.wall_side(kit.poster, 8.0, -1, zc=z + 1.8, name="poster_birds_1", w=0.8)
    c.wall_end(kit.wall_map, 0.0, zc=z + 1.8, name="map_coast", w=1.6)
    c.wall_side(kit.wall_clock, 7.0, 1, zc=z + 3.0, face="clock_1")
    # hanging model aeroplanes
    for k, (a, p) in enumerate(((9.5, 0.6), (12.8, -0.5))):
        _model_plane(B, *c.W(a, p), z + 3.3, c.ang + 30 * k)
    c.thing("workshop_board", "board", 9.8, -INNER, 1.7, (0, 1), topic="club")
    kit.pendant(B, *c.W(8.4, 0.0), z + 4.0, drop=0.6, kind="cone", light=False)
    kit.pendant(B, *c.W(11.6, 0.0), z + 4.0, drop=0.6, kind="cone", light=False)
    B.lamp((*c.W(10.5, 0.0), z + 3.5), lumens=700, colour=WARM, kind=PUB, radius=7.0)
    # ---------------------------------------------------------------- 8.8: the photo lab
    z = 8.8
    c = Ctx(B, "stack", z)
    x, y = c.W(8.3, INNER - 0.4)
    kit.lab_bench(B, x, y, z, c.fy(0, -1), w=2.4, d=0.75)
    B.thing("darkroom_bench", "craft", (x, y, z + 0.9), c.fy(0, -1), what="darkroom")
    x, y = c.W(11.2, INNER - 0.4)
    kit.lab_bench(B, x, y, z, c.fy(0, -1), w=2.4, d=0.75, seed=3)
    c.put(kit.photo_booth, 14.0, -1.6, face=(-1, 0))
    x, y = c.W(14.0, -1.6)
    B.thing("photo_booth", "machine", (x, y, z + 1.0), c.fy(-1, 0), what="photo_booth")
    c.along(kit.table, 9.0, -1.2, w=1.8, d=0.8, h=0.8, mat="CT_WoodLight")
    c.put(kit.slide_projector, 9.0, -1.2, z=z + 0.8)
    c.put(kit.cardboard_box, 9.9, -1.1, z=z + 0.8, w=0.35, d=0.3, h=0.12, seed=1)
    for sgn in (-1, 1):
        kit.stool(B, *c.W(9.0 + 0.5 * sgn, -0.55), z, 0.0)
    c.put(kit.screen_divider, 12.5, -0.4, face=(0, 1))
    # drying line with prints along the end wall side
    pa, pb = c.W(14.6, 2.0), c.W(14.6, -2.0)
    rod(B.detail, "CT_Steel", (pa[0], pa[1], z + 2.1), (pb[0], pb[1], z + 2.1), 0.01)
    for k in range(5):
        px, py = c.W(14.6, 1.6 - 0.8 * k)
    for k in range(4):
        c.wall_end(kit.poster, 1.5 - 1.0 * k, zc=1.75, name=("travel_0", "travel_1", "travel_3", "travel_4")[k], w=0.55)
    c.wall_end(kit.sign_plate, 0.0, zc=2.7, name="sign_photo", w=1.6)
    kit.pendant(B, *c.W(9.5, 0.0), z + 3.0, drop=0.6, kind="opal", light=False)
    B.lamp((*c.W(11.0, 0.0), z + 2.7), lumens=500, colour="#FFB090", kind=PUB, radius=6.0)
    c.thing("photo_board", "board", c.L - WT, 0.0, 1.6, (-1, 0), topic="exhibit")


def _model_plane(B, x, y, z, yaw):
    """A little hanging model aeroplane (fuselage, wings, tail) on two threads, built from boxes."""
    det = B.detail
    a = math.radians(yaw)
    ux, uy = math.cos(a), math.sin(a)
    vx, vy = -math.sin(a), math.cos(a)

    def pt(u, v):
        return x + u * ux + v * vx, y + u * uy + v * vy

    def blk(u0, u1, v0, v1, z0, z1, mat):
        pts = [pt(u0, v0), pt(u1, v0), pt(u1, v1), pt(u0, v1)]
        poly = [(p[0], p[1]) for p in pts]
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        box(det, mat, min(xs), max(xs), min(ys), max(ys), z + z0, z + z1)
    # axis-aligned approximation (boxes are world-aligned); good enough for a 40 cm toy
    blk(-0.2, 0.2, -0.03, 0.03, 0.0, 0.05, "CT_Plaster")
    blk(-0.03, 0.03, -0.3, 0.3, 0.01, 0.03, "CT_PlasterRed")
    blk(-0.2, -0.14, -0.1, 0.1, 0.01, 0.025, "CT_PlasterRed")
    for dx in (-0.1, 0.1):
        px, py = pt(dx, 0.0)
        rod(det, "CT_Steel", (px, py, z + 0.05), (px, py, z + 0.9), 0.006)


def _studio(B):
    """Wing 126: a small stage hall (ground) and a youth lounge (4.4)."""
    # ---------------------------------------------------------------- ground: stage and rows of chairs
    c = Ctx(B, "studio", 0.0)
    c.put(kit.stage, 13.4, 0.0, w=4.9, d=2.5, h=0.6, curtain_h=3.1)
    sx, sy = c.W(13.4, 0.0)
    B.thing("stage", "screen", (sx, sy, 0.7), c.fy(-1, 0), what="stage")
    B.thing("stage_dance", "sport", (sx, sy, 0.0), c.fy(-1, 0), what="dance")
    c.put(kit.piano, 10.2, 2.55, face=(0, -1))
    c.thing("stage_piano", "play", 10.2, 2.4, 1.0, (0, -1), what="piano")
    kit.stool(B, *c.W(10.2, 1.7), 0.0, 0.0)
    n = 0
    for ri, a in enumerate((7.4, 8.4, 9.4)):
        for p in (-1.9, -1.35, -0.8, 0.8, 1.35) + (() if ri == 2 else (1.9,)):
            x, y = c.W(a, p)
            kit.chair(B, x, y, 0.0, c.fy(1, 0), kind="tube", mat="CT_PlasterRed")
        for g, pm in enumerate((-1.35, 1.35)):
            if ri in (0, 2):
                x, y = c.W(a, pm)
                B.thing("hall_seats_%d_%d" % (ri + 1, g + 1), "seat", (x, y, 0.45), c.fy(1, 0), height=0.45)
    c.wall_side(kit.poster, 9.0, 1, zc=1.9, name="poster_cons_4", w=0.9)
    c.wall_side(kit.poster, 11.6, 1, zc=1.9, name="cinema_3", w=0.9)
    c.wall_side(kit.notice_board, 9.0, -1, zc=1.7, w=1.1, seed=12)
    c.wall_side(kit.fire_extinguisher, 7.4, -1)
    c.put(kit.plant, 6.3, 2.2, kind="ficus", pot="CT_PlasterRed", h=1.6)
    c.thing("stage_board", "board", 9.0, -INNER, 1.7, (0, 1), topic="club")
    for a in (8.0, 10.6):
        kit.pendant(B, *c.W(a, 0.0), 4.0, drop=0.6, kind="cone", light=False)
    # ---------------------------------------------------------------- 4.4: lounge
    z = 4.4
    c = Ctx(B, "studio", z)
    c.along(kit.rug, 9.5, 0.0, w=4.6, d=3.2, name="rug_1")
    c.put(kit.sofa, 9.5, 2.0, face=(0, -1), seats=3, mat="CT_PlasterGreen")
    x, y = c.W(9.5, 2.0)
    B.thing("lounge_sofa", "seat", (x, y, z + 0.45), c.fy(0, -1), height=0.45)
    c.put(kit.armchair, 11.4, -1.9, face=(-1, 1), mat="CT_PlasterRed")
    x, y = c.W(11.4, -1.9)
    B.thing("lounge_armchair", "seat", (x, y, z + 0.45), c.fy(-1, 1), height=0.45)
    c.along(kit.coffee_table, 9.5, 0.6, w=1.0, d=0.55)
    c.put(kit.newspaper, 9.4, 0.6, z=z + 0.42, seed=1)
    c.put(kit.cup, 9.8, 0.55, z=z + 0.42)
    c.along(kit.table_football, 13.2, 0.0)
    x, y = c.W(13.2, 0.0)
    B.thing("table_football", "play", (x, y, z + 0.9), c.ang, what="table_football")
    c.put(kit.arcade, 14.4, 2.1, face=(-1, -1), screen="screen_arcade_0")
    x, y = c.W(14.4, 2.1)
    B.thing("arcade_1", "machine", (x, y, z + 1.2), c.fy(-1, -1), what="arcade")
    c.put(kit.jukebox, 14.4, -2.1, face=(-1, 1))
    x, y = c.W(14.4, -2.1)
    B.thing("jukebox", "machine", (x, y, z + 1.2), c.fy(-1, 1), what="jukebox")
    c.put(kit.tv_set, 7.1, 1.5, face=(1, -0.2))
    x, y = c.W(7.1, 1.5)
    B.thing("lounge_tv", "screen", (x, y, z + 0.9), c.fy(1, -0.2), what="tv")
    c.wall_side(kit.poster, 11.8, 1, zc=1.8, name="travel_2", w=0.8)
    c.wall_end(kit.notice_board, 0.0, zc=1.8, w=1.2, seed=15)
    c.wall_side(kit.wall_clock, 12.5, -1, zc=3.0, face="clock_0")
    c.put(kit.plant, 6.9, -2.2, kind="monstera", pot="CT_PlasterBlue", h=1.5)
    c.thing("lounge_board", "board", c.L - WT, 0.0, 1.8, (-1, 0), topic="club")
    B.lamp((*c.W(10.0, 0.0), z + 3.4), lumens=700, colour=WARM, kind=PUB, radius=7.0)
    kit.pendant(B, *c.W(9.5, 0.0), z + 4.0, drop=0.6, kind="opal", light=False)


def _front_room(B):
    """Wing 270 (floor 8.8, cantilevered): a meeting / lecture room."""
    z = 8.8
    c = Ctx(B, "front", z)
    c.along(kit.rug, 11.0, 0.0, w=6.4, d=3.4, name="rug_2")
    c.along(kit.table, 11.0, 0.0, w=4.4, d=1.0, h=0.76, mat="CT_WoodLight")
    n = 0
    for k, off in enumerate((-1.6, -0.55, 0.55, 1.6)):
        for sgn in (-1, 1):
            x, y = c.W(11.0 + off, sgn * 0.85)
            kit.chair(B, x, y, z, c.fy(0, -sgn), kind="wood")
    for end in (-1, 1):
        x, y = c.W(11.0 + end * 2.7, 0.0)
        kit.chair(B, x, y, z, c.fy(-end, 0), kind="wood")
    x, y = c.W(11.0, 0.9)
    B.thing("meeting_seat_1", "seat", (x, y, z + 0.45), c.fy(0, -1), height=0.45)
    x, y = c.W(11.0, -0.9)
    B.thing("meeting_seat_2", "seat", (x, y, z + 0.45), c.fy(0, 1), height=0.45)
    c.put(kit.samovar, 11.5, 0.0, z=z + 0.76)
    for k in range(4):
        c.put(kit.cup, 10.2 + 0.4 * k, 0.28 * (-1) ** k, z=z + 0.76)
    c.put(kit.books_stack, 9.7, -0.2, z=z + 0.76, n=3, seed=3)
    c.wall_end(kit.blackboard, 0.0, w=2.4, h=1.15, zb=0.9)
    x, y = c.W(c.L - WT, 0.0)
    B.thing("club_blackboard", "craft", (x, y, z + 1.5), c.fy(-1, 0), what="blackboard")
    c.wall_side(kit.bookshelf, 8.2, 1, w=1.2, h=2.0, topic="library", seed=21)
    x, y = c.W(8.2, INNER)
    B.thing("meeting_shelf", "shelf", (x, y, z + 1.3), c.fy(0, -1), topic="local")
    c.wall_side(kit.notice_board, 13.6, -1, zc=1.7, w=1.2, seed=22)
    c.wall_side(kit.picture, 14.0, 1, zc=1.8, name="painting_sea", w=0.9)
    c.wall_side(kit.wall_clock, 9.0, -1, zc=3.0, face="clock_3")
    c.put(kit.plant, 7.1, 2.1, kind="ficus", pot="CT_PlasterRed", h=1.6)
    c.put(kit.plant, 7.1, -2.1, kind="fern", pot="CT_PlasterGreen", h=0.8)
    c.put(kit.floor_lamp, 15.8, 2.0)
    x, y = c.W(13.6, -INNER)
    B.thing("meeting_board", "board", (x, y, z + 1.7), c.fy(0, 1), topic="club")
    B.lamp((*c.W(11.0, 0.0), z + 3.3), lumens=700, colour=WARM, kind=PUB, radius=7.0)
    kit.pendant(B, *c.W(11.0, 0.0), z + 3.8, drop=0.8, kind="cone", light=False)


def _lamps_misc(B):
    pass
