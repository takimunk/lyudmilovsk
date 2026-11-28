"""Ship frame <-> building frame for the motor ship «Полдень» (building type motor_ship), and the numbers of its
general arrangement, so the shell (motor_ship.py) and the furnishers (_ship_inside_cd.py, _ship_inside_ba.py) agree.

    import _shipframe as S

SHIP FRAME (metres; what R1 and DESIGN use, what ULyudCruise::StageTransform maps to the world)
    x   from the aftmost point of the stern (0) to the stem head (62.0): +x = toward the BOW
    y   0 on the centreline, PORT NEGATIVE, starboard positive
    z   0 at the WATERLINE (= the sea's high-water datum at the cruise stage), up
    ship yaw (degrees): the way a thing or a person FACES, 0 = toward the bow, 90 = to starboard, 180 = toward the
    stern, 270 (or -90) = to port. (Clockwise seen from above, like a compass with the bow as north.)
    NOTE x-bow / y-starboard / z-up is a LEFT-handed frame (as Unreal's). The kit's building frame is right-handed, so
    the map below mirrors y. Positions and yaws go through S.p / S.yaw; never hand-convert.

BUILDING FRAME (citykit: x along the front, y into the plot, z up, kit yaw 0 = facing -y, 90 = +x)
    bx = x - 31.0            the bow is at building +x, the stern at -x
    by = 5.0 - y             STARBOARD side is the "street" side (by small), port is at by = 10
    bz = z + Z0              Z0 = -1.5: the plot stands on terrace "B" (z -41.5), the sea datum is -43.0
    kit yaw = 90 - ship yaw

DECK LEVELS (ship z of the walking surface). The whole ship is lifted against R1 (which had the lower deck at
z -1.60, under water): the sea's water volume and the wave surface are not masked inside a hull, so every walkable
floor is ABOVE the waterline. R1's plan (x, y) numbers are unchanged; only z moved:
    Z_D 0.30   lower deck (hold cabins, crew, engine-room plates)       clear 2.30 (ceiling z 2.60)
    Z_C 2.80   main deck (vestibule, cabins 1-10, dining saloon, galley) clear 2.40
    Z_B 5.40   promenade deck (music saloon, bar, library, lux cabins)   clear 2.40
    Z_A 8.00   boat deck (boats, sun deck, officers' house, funnel)      clear 2.40
    Z_BRIDGE 10.60   bridge deck (wheelhouse, chart room, wings)         clear 2.30
Deck slabs are DECK_T = 0.20 thick: a ceiling is at (next deck z) - 0.20. Use S.ceil(deck).
The foredeck (x 46..62) ramps from Z_C up to Z_C + 1.0 at the stem: S.foredeck_z(x).

API (stable: append, never rename)
    S.p(x, y, z) -> (bx, by, bz)         S.p2(x, y) -> (bx, by)
    S.yaw(ship_yaw) -> kit yaw           S.unp(bx, by, bz) -> ship (x, y, z)
    S.half(x) -> hull half-breadth at main-deck level (the outer face of the side at that x), 0 outside 0..62
    S.deck_half(deck, x) -> half-breadth of the walkable deck plate of deck "D"|"C"|"B"|"A"|"BR" at x (0: no deck there)
    S.Z[deck], S.ceil(deck), S.floor(deck)
    S.ROOMS[name] = {"deck", "x": (x0, x1), "y": (y0, y1), "z": floor z, "note"}   (inner clear rectangle, ship frame)
    S.room(name) -> (x0, x1, y0, y1, z)
    S.box(m, mat, x0, x1, y0, y1, z0, z1, faces="all")   a box given in SHIP coordinates into mesh m (B.m / B.detail)
    S.poly(pts) -> building-frame CCW polygon of ship-frame 2D points
    S.thing(B, id, kind, (x, y, z), ship_yaw, **props), S.spot(B, id, kind, (x, y, z), ship_yaw),
    S.lamp(B, (x, y, z), **kw)           the citykit calls with ship coordinates
    S.fr(x, y, z, ship_yaw) -> (bx, by, bz, kit_yaw_degrees)   handy for interior_kit pieces
"""
import math

L = 62.0                    # length overall
BEAM = 10.0
X0 = 31.0                   # building x of ship x: bx = x - X0
Y0 = 5.0                    # building y of the centreline: by = Y0 - y
Z0 = -1.5                   # building z of the waterline (plot z -41.5 on terrace B, sea datum -43.0)

DECK_T = 0.20               # deck slab thickness
Z_KEEL = -2.80
Z_D = 0.30
Z_C = 2.80
Z_B = 5.40
Z_A = 8.00
Z_BRIDGE = 10.60
Z_WHEELHOUSE_TOP = 12.90
Z_FUNNEL_TOP = 13.40
Z_ENGINE_PLATES = Z_D       # the engine-room floor (seen from the gallery on deck C)
Z = {"D": Z_D, "C": Z_C, "B": Z_B, "A": Z_A, "BR": Z_BRIDGE}
_ABOVE = {"D": Z_C, "C": Z_B, "B": Z_A, "A": Z_BRIDGE, "BR": Z_WHEELHOUSE_TOP + DECK_T}

DOOR_W, DOOR_H = 1.10, 2.10          # every door hole is at least this clear
WALL_T = 0.10                        # inner bulkheads (centred on the line given in WALLS)
SIDE_T = 0.15                        # the ship's side / deckhouse outer walls (inside face = outer face - SIDE_T)
RAIL_H = 1.10

FOREDECK_X0, FOREDECK_RISE = 46.0, 1.0
BULWARK_H = 1.10


def floor(deck):
    return Z[deck]


def ceil(deck):
    return _ABOVE[deck] - DECK_T


def foredeck_z(x):
    """Walking surface of the main deck forward: flat Z_C to x 46, then a ramp up to Z_C + 1.0 at the stem."""
    if x <= FOREDECK_X0:
        return Z_C
    return Z_C + FOREDECK_RISE * (x - FOREDECK_X0) / (L - FOREDECK_X0)


# ---------------------------------------------------------------------------------------------- frame
def p(x, y, z=0.0):
    return (x - X0, Y0 - y, z + Z0)


def p2(x, y):
    return (x - X0, Y0 - y)


def unp(bx, by, bz=0.0):
    return (bx + X0, Y0 - by, bz - Z0)


def yaw(ship_yaw):
    return 90.0 - ship_yaw


def fr(x, y, z, ship_yaw=0.0):
    bx, by, bz = p(x, y, z)
    return bx, by, bz, yaw(ship_yaw)


def poly(pts):
    """Ship-frame 2D points -> building-frame polygon, counter-clockwise."""
    q = [p2(x, y) for x, y in pts]
    a = sum(q[i][0] * q[(i + 1) % len(q)][1] - q[(i + 1) % len(q)][0] * q[i][1] for i in range(len(q)))
    return q if a > 0 else q[::-1]


def box(m, mat, x0, x1, y0, y1, z0, z1, faces="all"):
    """An axis-aligned box given in ship coordinates. faces: letters among xXyYzZ in SHIP axes (x = the face looking
    aft, X = toward the bow, y = to port, Y = to starboard, z down, Z up)."""
    from geom import box as _box
    x0, x1 = sorted((x0, x1))
    y0, y1 = sorted((y0, y1))
    z0, z1 = sorted((z0, z1))
    if faces != "all":
        faces = faces.translate(str.maketrans("yY", "Yy"))
    _box(m, mat, x0 - X0, x1 - X0, Y0 - y1, Y0 - y0, z0 + Z0, z1 + Z0, faces=faces)


def thing(B, tid, kind, pos, ship_yaw=0.0, **props):
    B.thing(tid, kind, p(*pos), yaw(ship_yaw), **props)


def spot(B, sid, kind, pos, ship_yaw=0.0):
    B.spot(sid, kind, p(*pos), yaw(ship_yaw))


def lamp(B, pos, **kw):
    B.lamp(p(*pos), **kw)


# ---------------------------------------------------------------------------------------------- hull plan
# half-breadth of the hull at main-deck level (R1 5.2; the stern is rounded off to a cruiser stern in plan)
_PLAN = [(0.0, 0.0), (0.15, 1.05), (0.5, 1.95), (1.0, 2.75), (2.0, 3.6), (4.0, 4.2), (8.0, 4.7), (14.0, 5.0),
         (40.0, 5.0), (43.0, 4.7), (46.0, 4.3), (50.0, 3.6), (54.0, 2.7), (58.0, 1.5), (60.5, 0.6), (62.0, 0.0)]


def _pchip(tab, x):
    """Monotone cubic through tab [(x, y)]."""
    n = len(tab)
    if x <= tab[0][0]:
        return tab[0][1]
    if x >= tab[-1][0]:
        return tab[-1][1]
    k = 0
    while tab[k + 1][0] < x:
        k += 1
    h = [tab[i + 1][0] - tab[i][0] for i in range(n - 1)]
    d = [(tab[i + 1][1] - tab[i][1]) / h[i] for i in range(n - 1)]

    def slope(i):
        if i == 0:
            return d[0]
        if i == n - 1:
            return d[-1]
        if d[i - 1] * d[i] <= 0:
            return 0.0
        w1, w2 = 2 * h[i] + h[i - 1], h[i] + 2 * h[i - 1]
        return (w1 + w2) / (w1 / d[i - 1] + w2 / d[i])
    m0, m1 = slope(k), slope(k + 1)
    t = (x - tab[k][0]) / h[k]
    h00 = 2 * t ** 3 - 3 * t ** 2 + 1
    h10 = t ** 3 - 2 * t ** 2 + t
    h01 = -2 * t ** 3 + 3 * t ** 2
    h11 = t ** 3 - t ** 2
    return h00 * tab[k][1] + h10 * h[k] * m0 + h01 * tab[k + 1][1] + h11 * h[k] * m1


# The plan is made of straight chords between these stations (walls, slabs, rails and the hull loft all use the same
# stations, so their edges meet exactly). Between x 14 and 40 the side is straight at y +-5.0.
STATIONS = [0.0, 0.15, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 10.0, 11.6, 13.2, 14.0, 14.8,
            40.0, 40.2, 41.8, 43.2, 44.6, 46.0, 47.0, 48.0, 49.0, 50.0, 51.0, 52.0, 53.0, 54.0, 55.0, 56.0, 57.0, 58.0,
            59.0, 60.0, 60.5, 61.0, 61.5, 62.0]
_HALF = [(x, _pchip(_PLAN, x)) for x in STATIONS]


def half(x):
    """Half-breadth of the hull at main-deck level at ship x: the OUTER face of the side (0 outside 0..62).
    Piecewise linear over STATIONS."""
    if x < 0.0 or x > L:
        return 0.0
    for (xa, ha), (xb, hb) in zip(_HALF, _HALF[1:]):
        if x <= xb:
            return ha + (hb - ha) * (x - xa) / (xb - xa)
    return 0.0


# deck plates: (x aft, x fwd, round-off length aft, round-off length fwd, max half-breadth)
_DECKS = {"D": (8.0, 50.0, 0.0, 0.0, 4.75), "C": (0.0, 62.0, 0.0, 0.0, 5.0), "B": (2.0, 48.0, 0.0, 3.0, 5.0),
          "A": (6.0, 46.0, 1.2, 3.0, 5.0), "BR": (34.0, 43.0, 0.0, 0.0, 3.0)}


def deck_half(deck, x):
    """Half-breadth of the deck plate of a deck at ship x (0 where the deck does not reach). Decks B and A follow the
    hull plan and are rounded off in plan at the fore end; the bridge deck is 3.0 wide with wings to 5.0 at x 39..41.5;
    deck D is the inside of the hull (hull - 0.25)."""
    xa, xf, ra, rf, hm = _DECKS[deck]
    if x < xa - 1e-9 or x > xf + 1e-9:
        return 0.0
    if deck == "BR":
        return 5.0 if 39.0 <= x <= 41.5 else 3.0
    h = min(half(x), hm)
    if deck == "D":
        return max(0.0, min(hull_half(x, Z_D) - 0.25, hm))
    if rf and x > xf - rf:
        t = (x - (xf - rf)) / rf
        h = min(h, min(half(xf - rf), hm) * math.sqrt(max(0.0, 1.0 - t * t)))
    if ra and x < xa + ra:
        t = ((xa + ra) - x) / ra
        h = min(h, half(xa + ra) * (0.75 + 0.25 * math.sqrt(max(0.0, 1.0 - t * t))))
    return h


# ---------------------------------------------------------------------------------------------- rooms (R1 section 5)
def _r(deck, x0, x1, y0, y1, note=""):
    return {"deck": deck, "x": (x0, x1), "y": (y0, y1), "z": Z[deck], "note": note}


ROOMS = {
    # ---- lower deck D (z 0.30)
    "crew_quarters":   _r("D", 8.0, 18.0, -4.2, 4.2, "crew mess + 16 crew berths; stair from the galley lobby x 16.2-18, y -4.3..-3.0 (closed door: no player access needed)"),
    "engine_room":     _r("D", 18.0, 27.0, -4.4, 4.4, "plates at Z_D; diesel on the centreline x 20-26; seen from the gallery on deck C"),
    "d_lobby":         _r("D", 27.0, 33.0, -4.4, 4.4, "stair-foot lobby; stair core x 27-31, y +-1.5"),
    "baggage_room":    _r("D", 27.0, 33.0, -4.4, -1.5, "port of the stair core (closed door leaf in its wall y -1.5, x 31.5-32.6)"),
    "showers":         _r("D", 27.0, 33.0, 1.5, 4.4, "starboard of the stair core (open door in its wall y +1.5, x 31.5-32.6)"),
    "d_corridor":      _r("D", 33.0, 43.5, -0.75, 0.75, "hold-cabin corridor 1.4 wide"),
    "cabin15":         _r("D", 33.0, 36.5, 0.75, 4.4, "4-berth, starboard; FURNISHED hero cabin (hold class)"),
    "cabin17":         _r("D", 36.5, 40.0, 0.75, 4.4),
    "cabin19":         _r("D", 40.0, 43.5, 0.75, 4.05, "outer wall follows the hull: S.deck_half('D', x) = 4.75 at x 40, 4.09 at x 43.5"),
    "cabin16":         _r("D", 33.0, 36.5, -4.4, -0.75),
    "cabin18":         _r("D", 36.5, 40.0, -4.4, -0.75),
    "cabin20":         _r("D", 40.0, 43.5, -4.05, -0.75),
    "cargo_hold":      _r("D", 43.5, 50.0, -2.3, 2.3, "under the hatch; door thing hold_door in bulkhead x 43.5 at the corridor end"),
    # ---- main deck C (z 2.80)
    "aft_deck":        _r("C", 0.5, 8.0, -4.0, 4.0, "open mooring deck inside the bulwark; capstan x 3, y 0; stair up to deck B x 3.3-7.5, y +1.5..+2.8"),
    "dining_saloon":   _r("C", 8.0, 18.0, -4.55, 4.55, "40 seats; widens to +-4.85 from x 14; doors: aft double x 8 y +-0.8, to corridor x 18 y +1.6..+2.9, service x 18 y -3.6..-2.5"),
    "servery":         _r("C", 18.0, 20.0, -4.85, 1.5, "pantry behind the saloon; hatch to the saloon in bulkhead x 18, y -1.4..+1.4"),
    "galley":          _r("C", 20.0, 27.0, -4.85, -1.5, "half-door to the vestibule in bulkhead x 27, y -3.6..-2.5"),
    "engine_casing_c": _r("C", 20.0, 27.0, -1.5, 1.5, "open well down to the engine room; gallery x 22-25.5, y +0.3..+1.5; door on the starboard face x 23.0-24.1"),
    "c_stbd_corridor": _r("C", 18.0, 27.0, 1.5, 3.0, "1.5 wide, from the saloon door to the vestibule"),
    "c_wc":            _r("C", 18.0, 27.0, 3.0, 4.85, "men 18-21, women 21-24, linen 24-27 (closed doors)"),
    "vestibule":       _r("C", 27.0, 32.0, -4.85, 4.85, "reception; stair core x 27-31 y +-1.5; purser port x 27.3-30; kiosk starboard x 27.3-30; shell gangway doors both sides x 30.4-31.8"),
    "c_corridor":      _r("C", 32.0, 46.0, -0.80, 0.80, "cabin corridor 1.5 wide; weather door to the foredeck at x 46"),
    "cabin9":          _r("C", 32.0, 34.8, 0.80, 4.85), "cabin10": _r("C", 32.0, 34.8, -4.85, -0.80),
    "cabin7":          _r("C", 34.8, 37.6, 0.80, 4.85, "4-berth, starboard; FURNISHED hero cabin (standard class)"),
    "cabin8":          _r("C", 34.8, 37.6, -4.85, -0.80),
    "cabin5":          _r("C", 37.6, 40.4, 0.80, 4.85), "cabin6": _r("C", 37.6, 40.4, -4.85, -0.80),
    "cabin3":          _r("C", 40.4, 43.2, 0.80, 4.6), "cabin4": _r("C", 40.4, 43.2, -4.6, -0.80),
    "cabin1":          _r("C", 43.2, 46.0, 0.80, 4.2, "2-berth, starboard; FURNISHED"),
    "cabin2":          _r("C", 43.2, 46.0, -4.2, -0.80, "2-berth, port; FURNISHED"),
    "foredeck":        _r("C", 46.0, 61.0, -3.9, 3.9, "open, ramped (S.foredeck_z); hatch x 46.8-49.8 y +-1.4; foremast x 50.6; windlass x 55.5-57"),
    # ---- promenade deck B (z 5.40); deckhouse x 12..45, outer walls y +-3.5 (inside +-3.35)
    "smoking_deck":    _r("B", 2.3, 12.0, -3.5, 3.5, "open aft deck (to the rail: S.deck_half('B', x) - 0.15); stair down x 3.3-7.5 y +1.5..+2.8; stair up to deck A x 6.5-10.7, y -4.3..-3.05"),
    "prom_port":       _r("B", 12.0, 42.5, -4.85, -3.5, "side promenade 1.35 clear, under the boat deck"),
    "prom_stbd":       _r("B", 12.0, 42.5, 3.5, 4.85),
    "obs_walk":        _r("B", 45.0, 47.6, -2.5, 2.5, "forward observation walk round the saloon front"),
    "b_aft_corridor":  _r("B", 12.0, 18.5, -0.75, 0.75, "between the lux cabins; aft door x 12 to the smoking deck"),
    "lux11":           _r("B", 12.0, 15.25, 0.75, 3.35, "starboard aft; FURNISHED hero cabin (lux)"),
    "lux13":           _r("B", 15.25, 18.5, 0.75, 3.35), "lux12": _r("B", 12.0, 15.25, -3.35, -0.75),
    "lux14":           _r("B", 15.25, 18.5, -3.35, -0.75),
    "b_cross":         _r("B", 18.5, 20.0, -3.35, 3.35, "cross passage; side doors to both promenades"),
    "engine_casing_b": _r("B", 20.0, 27.0, -1.5, 1.5, "closed casing (ship model + honour board on its starboard face)"),
    "b_stbd_corridor": _r("B", 20.0, 27.0, 1.5, 3.35),
    "b_pantry":        _r("B", 20.0, 27.0, -3.35, -1.5, "steward's pantry + bar store (closed)"),
    "b_lobby":         _r("B", 27.0, 33.0, -3.35, 3.35, "stair lobby; stair core x 27-31 y +-1.5; library corner port, notice niche starboard; promenade doors x 31.6-32.8"),
    "library":         _r("B", 27.2, 31.5, -3.35, -1.6, "library corner (part of the lobby)"),
    "music_saloon":    _r("B", 33.0, 45.0, -3.35, 3.35, "front semi-ellipse from x 42.5 to the apex x 45.0; bar port aft x 33.3-36.8; card table starboard aft; dance floor x 37.5-42 y +-2; bandstand x 42.2-45; side doors x 37.0-38.2"),
    # ---- boat deck A (z 8.00)
    "sun_deck":        _r("A", 6.5, 18.0, -3.4, 3.4, "shuffleboard x 7-15 y +-0.9; stair opening from deck B x 6.5-10.7 y -4.3..-3.05"),
    "funnel":          _r("A", 21.0, 26.0, -1.3, 1.3, "funnel + casing top (solid)"),
    "stair_hood":      _r("A", 27.0, 32.3, -1.7, 1.7, "top of the main stair; doors both sides x 31.1-32.2"),
    "officers_house":  _r("A", 34.0, 43.0, -3.0, 3.0, "front curved (apex x 43); corridor y +-0.7 x 34-39"),
    "radio_room":      _r("A", 34.0, 37.0, -2.85, -0.75), "mate_cabin": _r("A", 37.0, 42.0, -2.85, -0.75),
    "captain_bedroom": _r("A", 34.0, 38.0, 0.75, 2.85), "captain_cabin": _r("A", 38.0, 42.0, 0.75, 2.85),
    # ---- bridge deck (z 10.60)
    "wheelhouse":      _r("BR", 38.5, 43.0, -2.85, 2.85, "front curved (apex x 43); wheel x 41.6 y 0; sliding doors to the wings x 39.5-40.7"),
    "chart_room":      _r("BR", 36.0, 38.5, -2.85, 0.0, "open arch to the wheelhouse"),
    "bridge_aft":      _r("BR", 34.0, 38.5, 0.0, 2.85, "open deck aft of the chart room (and x 34-36 port)"),
    "wing_port":       _r("BR", 39.0, 41.5, -4.85, -3.0), "wing_stbd": _r("BR", 39.0, 41.5, 3.0, 4.85),
}


def room(name):
    r = ROOMS[name]
    return r["x"][0], r["x"][1], r["y"][0], r["y"][1], r["z"]


# stairs (ship frame): (deck from, deck to, x of the foot, x of the head, y0, y1)
STAIRWELL = (27.0, 31.0, -1.5, 1.5)          # main U-stair core, decks D..A: walls at y +-1.5 and x 27, open at the
#                                              fwd end x 31.0; the starboard flight goes UP (rising aft to the landing
#                                              x 27.05..29.04), the port flight comes up from the landing to the deck
STAIRS = {
    "aft_c_b":   ("C", "B", 7.5, 3.3, 1.5, 2.8),       # aft mooring deck -> smoking deck, rising aft
    "aft_b_a":   ("B", "A", 6.5, 10.7, -4.3, -3.05),    # smoking deck -> sun deck, rising forward (port side)
    "bridge_s":  ("A", "BR", 34.8, 39.0, 3.1, 4.4),    # boat deck -> starboard bridge wing
    "bridge_p":  ("A", "BR", 34.8, 39.0, -4.4, -3.1),  # boat deck -> port bridge wing
}


# ---------------------------------------------------------------------------------------------- the hull's form
# (appended 22:35) S.hull_point(u, z): a point of the starboard hull surface for the plan station u (0..62) at height
# z; S.hull_half(x, z): the hull's outer half-breadth at ship x and height z (0 where there is no hull);
# S.stem_x(z) / S.stern_x(z): the profile. At and above the main deck the hull's breadth is S.half(x).
def _sm(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3 - 2 * t)


def _lerp_tab(tab, x):
    if x <= tab[0][0]:
        return tab[0][1]
    for (xa, ya), (xb, yb) in zip(tab, tab[1:]):
        if x <= xb:
            return ya + (yb - ya) * (x - xa) / (xb - xa)
    return tab[-1][1]


Z_SHEER_BOW = Z_C + FOREDECK_RISE          # 3.80: the deck at the stem


def stem_x(z):
    """The stem profile: x of the stem at height z (raked above water, a rounded forefoot under it)."""
    if z >= Z_SHEER_BOW:
        return L
    if z >= 0:
        return 60.4 + (L - 60.4) * z / Z_SHEER_BOW
    q = min(1.0, -z / -Z_KEEL)
    return 60.4 + (L - 60.4) * z / Z_SHEER_BOW - 2.4 * (1 - math.sqrt(max(0.0, 1 - q * q)))


def stern_x(z):
    """The stern profile (cruiser stern): x of the aftmost point at height z."""
    if z >= 1.8:
        return 0.0
    if z >= 0:
        q = 1 - z / 1.8
        return 2.2 * (1 - math.sqrt(max(0.0, 1 - q * q)))
    return 2.2 + 6.0 * (-z / -Z_KEEL) ** 0.8


_FWL = [(0, 0.55), (4, 0.78), (10, 0.92), (14, 0.97), (40, 0.97), (46, 0.86), (50, 0.72), (56, 0.48), (62, 0.30)]


def hull_point(u, z):
    """A point of the starboard hull surface: u = the plan station (0..62), z the height (ship frame)."""
    x = u + (stem_x(z) - L) * _sm((u - 48.0) / 14.0) + stern_x(z) * (1 - _sm(u / 20.0))
    f = _lerp_tab(_FWL, u)
    zs = foredeck_z(u)
    zfull = 0.15 + (zs - 0.15) * _sm((u - 38.0) / 16.0)        # the height where the side reaches its full breadth
    if u < 12:
        zfull = 0.15 + (zs - 0.15) * (1 - u / 12.0) * 0.8
    if z >= zfull:
        k = 1.0
    elif z >= 0:
        k = f + (1 - f) * _sm(z / zfull) ** 0.9
    else:
        pw = 1.5 + 1.3 * math.sin(math.pi * min(1.0, max(0.0, u / 62.0))) ** 2
        q = min(1.0, -z / -Z_KEEL)
        k = f * max(0.0, 1 - q ** pw) ** (1 / pw)
    return (x, half(u) * k, z)




def hull_half(x, z):
    """Half-breadth of the hull's outer surface at ship (x, z), z between the keel and the main deck."""
    lo, hi = 0.0, L
    if x <= hull_point(lo, z)[0] or x >= hull_point(hi, z)[0]:
        return 0.0
    for _ in range(40):
        mid = (lo + hi) / 2
        if hull_point(mid, z)[0] < x:
            lo = mid
        else:
            hi = mid
    return hull_point((lo + hi) / 2, min(z, foredeck_z(x)))[1]


# (appended 23:00) S.FITTINGS: {name: ship (x, y, z)} of the shell's deck fittings (ship_bell, windlass, cargo_hatch, capstan,
# gangway, bow, stern_rail, funnel_top, lifeboat_1..4, foremast, mainmast, searchlight, radar_scanner, life_rings: a list).
# Filled by motor_ship.build() before the furnishers run; empty when the module is imported alone.
FITTINGS = {}
