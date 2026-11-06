"""The town of Lyudmilovsk: its urban plan (docs/city_plan.md). Pure Python, no bpy or unreal.

The town runs along the beach and climbs inland in big concrete terraces (Japanese hillside style): a
promenade over the beach, the lower town on the Academgorodok's level, then four terraces ~7 m apart, each
held by a retaining wall with flats built into it (windows that light up at night) and stairs along its face.

Everything here is in tools/world_map.py's frame (metres; X west = toward the sea, Y north, Z up). The plan is
laid out in the "terrace frame" (s, y): s = metres inland from the high-water line at that Y (world_map.inland),
y = map Y. A wall is a line s = wall_s(y); the terraces are the bands between walls.

  python3 tools/city_plan.py            # writes unreal/Lyudmilovsk/Content/City/city_plan.json
  python3 tools/city_plan.py --plot plan.png

Other threads read city_plan.json (plots for the buildings, spots for the life and the holidays, stairs and
wall dwellings for walking and lit windows). Ids are stable: change a plot's place freely, never reuse an id.
"""
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import world_map as W  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "unreal", "Lyudmilovsk", "Content", "City", "city_plan.json")

# --- the terraces ------------------------------------------------------------------------------------------

# The town's extent along the coast; the terraces fade into the land around them over CITY_FADE metres past it.
CITY_Y0, CITY_Y1 = 380.0, 1450.0
CITY_FADE = 45.0

# (id, name ru, name en, ground z). T1 is the Academgorodok's level (world_map.academ_z, -36.16): it stands on it.
TERRACES = [
    ("T0", "Набережная", "Embankment", -39.5),
    ("T1", "Нижний город", "Lower town", -36.2),
    ("T2", "Вторая терраса", "Second terrace", -29.0),
    ("T3", "Третья терраса", "Third terrace", -22.0),
    ("T4", "Четвёртая терраса", "Fourth terrace", -15.0),
    ("T5", "Верхняя терраса", "Upper terrace", -8.0),
]
# Past the upper terrace's first TOP_FLAT metres the ground rises on gently into the inland ridge.
TOP_FLAT = 220.0
TOP_RISE = 0.05

# The walls: W1 holds the lower town over the promenade, W2..W5 the terraces. (y, s) control points; between
# y 700 and 820 the walls swing seaward round the Academgorodok (its ground is T1, up to s ~540), so north of it
# the terraces come closer to the sea. The promenade's seawall to the beach is at s = SEAWALL_S.
SEAWALL_S = W.BEACH_W                 # 25: the beach's top
WALLS = [
    ("W1", [(300, 70.0), (1500, 70.0)]),
    ("W2", [(300, 560.0), (700, 560.0), (820, 330.0), (1500, 330.0)]),
    ("W3", [(300, 710.0), (700, 710.0), (820, 480.0), (1500, 480.0)]),
    ("W4", [(300, 860.0), (700, 860.0), (820, 680.0), (1500, 680.0)]),
    ("W5", [(300, 1010.0), (700, 1010.0), (820, 830.0), (1500, 830.0)]),
]
WALL_RAMP = 6.0      # the landscape climbs from the lower terrace to the upper over this many metres behind the
                     # wall's face; the wall block (WALL_DEPTH deep) hides it
WALL_DEPTH = 8.0
SEAWALL_H = W.BEACH_TOP


def _lin(pts, y):
    """Piecewise linear with smoothstep corners (walls are straight runs with gentle bends)."""
    if y <= pts[0][0]:
        return pts[0][1]
    for (y0, s0), (y1, s1) in zip(pts, pts[1:]):
        if y <= y1:
            t = (y - y0) / (y1 - y0)
            t = t * t * (3 - 2 * t)
            return s0 + (s1 - s0) * t
    return pts[-1][1]


def wall_s(i, y):
    """s of wall i's face (0 = W1) at map Y."""
    return _lin(WALLS[i][1], y)


def city_mask(y):
    """1 inside the town's Y extent, fading to 0 over CITY_FADE past it."""
    d = max(CITY_Y0 - y, y - CITY_Y1, 0.0)
    t = min(1.0, d / CITY_FADE)
    return 1.0 - t * t * (3 - 2 * t)


def terrace_at(x, y):
    """Index into TERRACES of the terrace under (x, y) (ignoring the fade at the town's ends); -1 = beach/sea."""
    s = W.inland(x, y)
    if s < SEAWALL_S:
        return -1
    k = 0
    for i in range(len(WALLS)):
        if s >= wall_s(i, y):
            k = i + 1
    return k


def terrace_z(x, y):
    """The town's ground at (x, y) before the fade: the terraces with their ramps behind each wall, None on the
    beach and the sea."""
    s = W.inland(x, y)
    if s < SEAWALL_S:
        return None
    z = TERRACES[0][3]
    if s < SEAWALL_S + 1.5:                         # the seawall's top edge
        z = TERRACES[0][3]
    for i in range(len(WALLS)):
        ws = wall_s(i, y)
        t = (s - ws - 0.4) / WALL_RAMP
        if t <= 0.0:
            break
        t = min(1.0, t)
        z = TERRACES[i][3] + (TERRACES[i + 1][3] - TERRACES[i][3]) * t
    top0 = wall_s(len(WALLS) - 1, y) + TOP_FLAT
    if s > top0:
        z += (s - top0) * TOP_RISE
    return z


def height(x, y, natural):
    """The ground with the town: the terraces inside the town (blended into the natural land past its ends and
    never under the land's own hills), the natural macro shape elsewhere. natural = world_map's own height there."""
    m = city_mask(y)
    if m <= 0.0:
        return natural
    s = W.inland(x, y)
    if s < SEAWALL_S - 0.5:
        return natural
    z = terrace_z(x, y)
    if z is None:
        return natural
    z = max(z, natural) if natural > z + 2.0 and s > wall_s(len(WALLS) - 1, y) else z   # the ridge and the hills win
    return natural + (z - natural) * m


def to_map(s, y):
    return (W.shore_x(y) - s, y)


def seaward_yaw(y):
    """Unreal yaw (degrees) of a front facing the sea at Y: map +X is yaw 0, +Y is 90 (map X, Y are Unreal's)."""
    e = 1.0
    dx = (W.shore_x(y + e) - W.shore_x(y - e)) / (2 * e)     # along the coast: (dx, 1)
    # seaward normal of the along vector (dx, 1): (1, -dx)
    return round(math.degrees(math.atan2(-dx, 1.0)), 1)


def wall_yaw(i, y):
    """Yaw of wall i's face (facing seaward / down the terraces) at Y, following its bends."""
    e = 2.0
    x0, y0 = to_map(wall_s(i, y - e), y - e)
    x1, y1 = to_map(wall_s(i, y + e), y + e)
    ax, ay = x1 - x0, y1 - y0                       # along the wall, northward
    return round(math.degrees(math.atan2(-ax, ay)), 1)   # (ay, -ax): the normal on the sea side


# --- plots, spots, stairs ----------------------------------------------------------------------------------

# Building types (the buildings thread's catalog, blender/city/): key -> (ru, en, reference it is after).
BUILDING_TYPES = {
    "serpentine_slab": ("Змеевик", "Serpentine slab", "Brazilian modernism: a long curved slab on pilotis with brise-soleil following the contour (after Reidy's Pedregulho, Rio, 1947)"),
    "wave_tower": ("Волна", "Wave tower", "Brazilian modernism: a tall slab with wavy horizontal brise-soleil (after Niemeyer's Copan, São Paulo, 1966)"),
    "capsule_tower": ("Капсульная башня", "Capsule tower", "Japanese metabolism: capsule flats plugged onto two cores (after Kurokawa's Nakagin, Tokyo, 1972)"),
    "twin_cylinder_house": ("Дом-цилиндр", "Twin-cylinder house", "Constructivism: two interlocking cylinders with hexagonal windows (after Melnikov's house, Moscow, 1929)"),
    "commune_slab": ("Дом-коммуна", "Commune slab", "Constructivism: a slab on pilotis with ribbon windows and a roof garden (after Narkomfin, Moscow, 1930)"),
    "stacked_beam_tower": ("Дом-мост", "Stacked-beam tower", "Brutalism: crossed horizontal bars on cores over the slope (after Tbilisi's Ministry of Highways, 1975)"),
    "workers_club": ("Дом культуры", "Workers' club", "Constructivism: cantilevered halls over the street (after Melnikov's Rusakov Club, Moscow, 1928)"),
    "shell_market": ("Рынок-зонтики", "Shell market hall", "Hyperbolic-paraboloid concrete umbrellas (after Candela's market halls, Mexico)"),
    "culture_block": ("Спорт и культура", "Culture block", "Concrete towers with irregular window holes and sky bridges (after Bo Bardi's SESC Pompéia, São Paulo, 1982)"),
    "terrace_riser_housing": ("Ступенчатый дом", "Stepped riser housing", "Japanese hillside housing stepping up a terrace riser (after Rokko Housing, Kobe, Ando, 1983)"),
    "amber_pavilion": ("Янтарь", "Amber pavilion", "The restaurant thread's round glass pavilion under a concrete disc roof"),
    "hyperboloid_tower": ("Башня-гиперболоид", "Hyperboloid tower", "A steel lattice hyperboloid, the pier's lighthouse and water tank (after Shukhov's towers, 1896-1922)"),
    "wave_canopy": ("Марки́за", "Wave canopy", "A long free-form concrete canopy on thin columns over the fair (after Niemeyer's Marquise, Ibirapuera, 1954)"),
    "cube_houses": ("Кубические дома", "Cube houses", "Cubes tilted 45 degrees on hexagonal pylons, a row over a footpath (after Blom's Kubuswoningen, Rotterdam, 1984)"),
    "round_sanatorium": ("Санаторий-кольцо", "Round sanatorium", "A ring of cantilevered concrete cells round a courtyard (after Druzhba sanatorium, Yalta, 1985)"),
    "unite_block": ("Жилая единица", "Unité block", "A long brutalist slab on pilotis with a street of shops halfway up and a roof terrace (after Le Corbusier's Unité, Marseille, 1952)"),
    "stacked_boxes": ("Дом-кубики", "Stacked boxes", "Prefab boxes stacked in a terraced heap with roof gardens (after Safdie's Habitat 67, Montreal, 1967)"),
    "saucer_pavilion": ("Тарелка", "Saucer pavilion", "A round concrete saucer on a ring of legs (after Kyiv's Lybid 'flying saucer', 1971)"),
    "bus_stop_kiosk": ("Остановка-скульптура", "Sculptural kiosk", "Small concrete shelters and kiosks in wild shapes (after Soviet bus stops, 1960s-80s)"),
    "lifeguard_tower": ("Спасательная вышка", "Lifeguard tower", "A tall concrete lifeguard tower with a cabin on top"),
    "tidal_pool": ("Приливные бассейны", "Tidal pools", "Concrete sea pools set in the beach that fill at high water (after Siza's Leça pools, 1966)"),
    "school": ("Школа", "School", "A low modernist school with a courtyard and sports ground"),
    "clinic": ("Поликлиника", "Clinic", "A small two-storey modernist clinic"),
    "plain_block": ("Дом", "Plain block", "An ordinary 4-5 storey house (filler; the buildings thread may swap it for any type)"),
}


# Staff hints for the life thread: kind -> [(role, count, hours, days)].
STAFF = {
    "restaurant": [("cook", 2, [11, 23], "mon-sun"), ("waiter", 3, [11, 23], "mon-sun"), ("manager", 1, [10, 22], "tue-sun")],
    "cafe": [("barista", 2, [8, 21], "mon-sun"), ("cook", 1, [9, 18], "mon-sat")],
    "market": [("seller", 8, [7, 18], "mon-sun"), ("butcher", 1, [7, 15], "tue-sun"), ("fishmonger", 2, [6, 14], "mon-sat")],
    "shop": [("seller", 4, [9, 20], "mon-sat"), ("cashier", 2, [9, 20], "mon-sun")],
    "kiosk": [("seller", 1, [8, 22], "mon-sun")],
    "school": [("teacher", 8, [8, 15], "mon-fri"), ("director", 1, [8, 16], "mon-fri"), ("janitor", 1, [6, 18], "mon-sat")],
    "clinic": [("doctor", 3, [8, 17], "mon-fri"), ("nurse", 3, [8, 20], "mon-sat")],
    "club": [("projectionist", 1, [15, 23], "mon-sun"), ("dance_teacher", 1, [17, 21], "tue-sat"), ("clerk", 1, [12, 21], "mon-sun")],
    "culture": [("coach", 3, [7, 21], "mon-sat"), ("lifeguard", 2, [7, 21], "mon-sun"), ("cashier", 1, [7, 21], "mon-sun")],
    "hotel": [("receptionist", 2, [0, 24], "mon-sun"), ("doctor", 1, [9, 17], "mon-fri"), ("cleaner", 3, [8, 16], "mon-sat")],
    "office": [("clerk", 12, [9, 18], "mon-fri"), ("guard", 1, [18, 9], "mon-sun")],
    "lifeguard": [("lifeguard", 1, [10, 19], "mon-sun")],
    "fair": [("carousel_operator", 2, [12, 22], "fri-sun"), ("vendor", 6, [11, 22], "fri-sun")],
    "pier": [("lighthouse_keeper", 1, [18, 7], "mon-sun")],
    "workshop": [("master", 1, [10, 19], "mon-sat")],
}


def _plot(pid, kind, building, terrace, s, y, w, d, ru, en, yaw=None, height_max=None, notes=""):
    x, yy = to_map(s, y)
    z = TERRACES[terrace][3] if terrace >= 0 else W.BEACH_TOP
    yw = seaward_yaw(y) if yaw is None else yaw
    fx, fy = math.cos(math.radians(yw)), math.sin(math.radians(yw))
    return {"id": pid, "kind": kind, "staff": STAFF.get(kind),
            "front": [round(x + fx * d / 2, 2), round(yy + fy * d / 2, 2), round(z, 2)], "building": building, "terrace": TERRACES[terrace][0] if terrace >= 0 else "B",
            "s": s, "y_along": y, "map": [round(x, 2), round(yy, 2), round(z, 2)],
            "w": w, "d": d, "yaw": yw,
            "height_max": height_max, "name_ru": ru, "name_en": en, "notes": notes}


def plots():
    """The plots: where each building stands. w is along the terrace (the frontage), d inland; yaw is the way the
    front faces (seaward unless said); map is the plot's centre at ground level."""
    P = []
    a = P.append
    # T0, the embankment: the pier, the kiosks, the beach's buildings
    a(_plot("P-PIER", "pier", "hyperboloid_tower", 0, -120.0, 905.0, 14, 14, "Пирс и маяк", "Pier and lighthouse",
            height_max=36, notes="the pier runs from the promenade (s 25) out to s -140 (12 m wide deck, ~1.5 m over the "
                                 "high water); the tower stands on its head; on the town's main axis Y 905"))
    for k, y in enumerate((470, 640, 800, 1020, 1180, 1330)):
        a(_plot("P-KIOSK%d" % (k + 1), "kiosk", "bus_stop_kiosk", 0, 50.0, float(y), 6, 5, "Киоск %d" % (k + 1),
                "Kiosk %d" % (k + 1), height_max=5, notes="ice cream, newspapers, sodas, rent of loungers"))
    a(_plot("P-LIFEGUARD1", "lifeguard", "lifeguard_tower", -1, 10.0, 860.0, 4, 4, "Спасательная вышка", "Lifeguard tower",
            height_max=9))
    a(_plot("P-LIFEGUARD2", "lifeguard", "lifeguard_tower", -1, 10.0, 1160.0, 4, 4, "Спасательная вышка 2", "Lifeguard tower 2",
            height_max=9))
    a(_plot("P-TIDALPOOL", "pool", "tidal_pool", -1, -15.0, 1270.0, 70, 40, "Морские бассейны", "Tidal pools",
            notes="in the beach and the flat, s -35..5; the pools fill at high water (docs/tides.md)"))
    # T1, the lower town
    a(_plot("P-SQUARE", "square", None, 1, 150.0, 905.0, 130, 110, "Площадь Солнца", "Sun Square",
            notes="the main square on the town's axis; the fountain in its middle, a stage on its east side "
                  "(holidays), benches round it; open, no building"))
    a(_plot("P-RESTAURANT", "restaurant", "amber_pavilion", 1, 105.0, 770.0, 40, 40, "Ресторан «Янтарь»",
            "Amber restaurant", height_max=10, notes="the restaurant thread's own building (a round glass pavilion under "
                                                    "a concrete disc roof, branch wt/restaurant), looking over the promenade"))
    a(_plot("P-CAFE", "cafe", "saucer_pavilion", 1, 110.0, 1040.0, 26, 26, "Кафе «Тарелка»", "Saucer café", height_max=10))
    a(_plot("P-MARKET", "market", "shell_market", 1, 250.0, 860.0, 50, 40, "Рынок", "Market hall", height_max=10,
            notes="grocery, fish and fruit stalls under the umbrellas; the town's shop"))
    a(_plot("P-STORE", "shop", "plain_block", 1, 250.0, 960.0, 45, 30, "Универмаг", "Department store", height_max=14,
            notes="two storeys of shop with flats over; the buildings thread may give it a type"))
    a(_plot("P-FAIR", "fair", "wave_canopy", 1, 140.0, 1210.0, 150, 70, "Ярмарка", "Fairground", height_max=8,
            notes="stalls, a carousel and a small Ferris wheel under and round the wavy canopy; holidays"))
    a(_plot("P-PARK", "park", None, 1, 255.0, 1220.0, 300, 120, "Приморский парк", "Seaside park",
            notes="lawns, the pond, alleys, a playground, benches; trees by the plant system; open"))
    a(_plot("P-CLUB2", "club", "workers_club", 1, 260.0, 1060.0, 35, 30, "Дом культуры «Рассвет»", "Rassvet culture house",
            height_max=18, notes="cinema hall, dance hall, a hobby room; the cinema of the town"))
    for k, (s, y) in enumerate(((120.0, 450.0), (220.0, 450.0), (125.0, 560.0), (130.0, 680.0), (230.0, 1380.0))):
        a(_plot("P-T1H%d" % (k + 1), "residential", ("cube_houses", "plain_block", "twin_cylinder_house", "plain_block",
                                                     "plain_block")[k], 1, s, y, 40, 30, "Дом T1-%d" % (k + 1),
                "House T1-%d" % (k + 1), height_max=18))
    # T2
    a(_plot("P-SERPENTINE", "residential", "serpentine_slab", 2, 455.0, 1000.0, 260, 16, "Дом-змеевик", "Serpentine house",
            height_max=26, notes="follows the contour in front of W3 (its curve along the wall); the street runs under it "
                                 "on pilotis; a wall stair goes through its middle"))
    a(_plot("P-SCHOOL", "school", "school", 2, 395.0, 1300.0, 70, 60, "Школа № 1", "School No. 1", height_max=12))
    a(_plot("P-CLINIC", "clinic", "clinic", 2, 380.0, 860.0, 40, 30, "Поликлиника", "Clinic", height_max=10))
    a(_plot("P-T2H1", "residential", "cube_houses", 2, 400.0, 1150.0, 60, 25, "Кубические дома", "Cube houses", height_max=14))
    a(_plot("P-T2H2", "residential", "commune_slab", 2, 640.0, 520.0, 70, 14, "Дом-коммуна", "Commune house", height_max=20,
            notes="south part of T2, behind the Academgorodok"))
    # T3
    a(_plot("P-WAVE", "residential", "wave_tower", 3, 570.0, 1100.0, 80, 22, "Дом «Волна»", "Wave house", height_max=60,
            notes="the town's landmark seen from the sea"))
    a(_plot("P-T3H1", "residential", "twin_cylinder_house", 3, 560.0, 1300.0, 25, 20, "Дом-цилиндр", "Cylinder house",
            height_max=12))
    a(_plot("P-T3H2", "residential", "stacked_boxes", 3, 640.0, 1240.0, 60, 40, "Дом-кубики", "Stacked boxes",
            notes="heaps up against W4", height_max=28))
    a(_plot("P-T3SPORT", "culture", "culture_block", 3, 560.0, 860.0, 60, 50, "Спорткомплекс", "Sports and culture centre",
            height_max=40, notes="pool, gym, a sky bridge to the zenit station"))
    a(_plot("P-T3H3", "residential", "commune_slab", 3, 790.0, 520.0, 70, 14, "Дом-коммуна 2", "Commune house 2",
            height_max=20))
    # T4
    a(_plot("P-CAPSULE", "residential", "capsule_tower", 4, 740.0, 1180.0, 20, 20, "Капсульная башня", "Capsule tower",
            height_max=50))
    a(_plot("P-UNITE", "residential", "unite_block", 4, 760.0, 980.0, 130, 22, "Жилая единица", "Unité block", height_max=56,
            notes="shops street half way up, a roof terrace with a running track"))
    a(_plot("P-T4H1", "residential", "plain_block", 4, 930.0, 560.0, 60, 20, "Дом T4-1", "House T4-1", height_max=18))
    a(_plot("P-T4H2", "residential", "plain_block", 4, 750.0, 1380.0, 50, 20, "Дом T4-2", "House T4-2", height_max=18))
    # T5
    a(_plot("P-SANATORIUM", "hotel", "round_sanatorium", 5, 930.0, 1250.0, 60, 60, "Санаторий «Дружба»", "Druzhba sanatorium",
            height_max=30, notes="a hotel and sanatorium with the sea view over the whole town"))
    a(_plot("P-BRIDGEHOUSE", "office", "stacked_beam_tower", 5, 900.0, 950.0, 40, 40, "Дом-мост", "Bridge house",
            height_max=40, notes="offices; its top bar reaches out over W5 toward the sea"))
    a(_plot("P-T5H1", "residential", "capsule_tower", 5, 1080.0, 560.0, 20, 20, "Капсульная башня 2", "Capsule tower 2",
            height_max=50))
    # set-pieces of stepped housing against walls
    a(_plot("P-RISER1", "residential", "terrace_riser_housing", 2, 335.0, 1100.0, 40, 18, "Ступенчатый дом", "Stepped house",
            notes="in front of W2, climbing it to T2's edge (the plot straddles the wall: s 330..350)"))
    a(_plot("P-RISER2", "residential", "terrace_riser_housing", 4, 685.0, 900.0, 40, 18, "Ступенчатый дом 2", "Stepped house 2",
            notes="straddles W4"))
    return P


def spots():
    """Places where people gather, sit or watch: the life and the holidays use them by id."""
    S = []

    def a(sid, kind, s, y, terrace, ru, en, seats=0, yaw=None, notes=""):
        x, yy = to_map(s, y)
        z = TERRACES[terrace][3] if terrace >= 0 else W.BEACH_TOP - 0.5
        S.append({"id": sid, "kind": kind, "terrace": TERRACES[terrace][0] if terrace >= 0 else "B",
                  "map": [round(x, 2), round(yy, 2), round(z, 2)], "yaw": seaward_yaw(y) if yaw is None else yaw,
                  "seats": seats, "name_ru": ru, "name_en": en, "notes": notes})

    a("S-FOUNTAIN", "fountain", 150.0, 905.0, 1, "Фонтан «Солнце»", "Sun fountain", seats=24,
      notes="a round basin 18 m across; its rim is a bench")
    a("S-STAGE", "stage", 205.0, 905.0, 1, "Сцена на площади", "Square stage", yaw=0.0,
      notes="faces the square and the sea; holidays: concerts, the New Year tree")
    a("S-SQUARE", "square", 150.0, 905.0, 1, "Площадь Солнца", "Sun Square", seats=40)
    a("S-PIER-HEAD", "pier", -130.0, 905.0, 0, "Конец пирса", "Pier head", seats=6,
      notes="fishing, sunsets; fireworks are let off here on holidays")
    a("S-PROMENADE", "promenade", 45.0, 905.0, 0, "Променад", "Promenade",
      notes="the walk along T0, Y 380..1450; benches every 40 m (S-BENCH-P*)")
    for k, y in enumerate(range(420, 1441, 40)):
        a("S-BENCH-P%02d" % (k + 1), "bench", 62.0, float(y), 0, "Скамейка на набережной", "Promenade bench", seats=3,
          notes="backs to W1, facing the sea")
    a("S-BEACH", "beach", 8.0, 1000.0, -1, "Пляж", "Beach", seats=60, notes="loungers and towels Y 600..1400")
    a("S-BONFIRE", "bonfire", 5.0, 1120.0, -1, "Кострище", "Bonfire place", seats=20, notes="Ivan Kupala, summer nights")
    a("S-FAIR", "fair", 140.0, 1210.0, 1, "Ярмарка", "Fair", seats=30)
    a("S-PARK", "park", 255.0, 1220.0, 1, "Приморский парк", "Seaside park", seats=30)
    a("S-POND", "pond", 270.0, 1260.0, 1, "Пруд в парке", "Park pond", seats=10, notes="ducks; skating in winter")
    a("S-PLAYGROUND", "playground", 230.0, 1150.0, 1, "Детская площадка", "Playground", seats=8)
    for k, (s, y, t) in enumerate(((175.0, 870.0, 1), (175.0, 940.0, 1), (240.0, 1290.0, 1), (300.0, 1180.0, 1),
                                   (420.0, 1000.0, 2), (420.0, 1220.0, 2), (600.0, 960.0, 3), (720.0, 1050.0, 4),
                                   (200.0, 620.0, 1), (500.0, 500.0, 1))):
        a("S-BENCH-%02d" % (k + 1), "bench", s, y, t, "Скамейка", "Bench", seats=3,
          notes="the grannies' benches: by entrances, the square and the park")
    a("S-SUNRISE", "sunrise_point", 1060.0, 1100.0, 5, "Смотровая «Рассвет»", "Dawn lookout", seats=8, yaw=180.0,
      notes="the top terrace's east edge: dawn over the forest; also the whole town and the sea behind")
    a("S-SUNSET", "sunset_point", 870.0, 1100.0, 5, "Смотровая над городом", "Town lookout", seats=10,
      notes="the edge of W5: sunsets over the sea across the whole town")
    a("S-CASCADE", "fountain", 445.0, 760.0, 1, "Каскад", "Cascade", seats=12,
      notes="water steps down the bent walls W2-W5 beside the grand stair (ST-GRAND)")
    return S


def stairs():
    """Stairs from one terrace up to the next: id, wall, bottom (on the lower terrace) and top (on the upper)
    points, map metres. Wall stairs run along the wall's face (two flights and a landing, 2.4 m wide), drawn by
    the wall build; the grand stairs go straight up the town's axes."""
    out = []
    # the seawall: steps from the promenade down to the beach every 80 m
    for k, y in enumerate(range(420, 1441, 80)):
        bx, by = to_map(SEAWALL_S - 4.0, float(y))
        tx, ty = to_map(SEAWALL_S + 1.0, float(y))
        out.append({"id": "ST-B%02d" % (k + 1), "wall": "SEA", "kind": "steps", "bottom": [round(bx, 2), by, W.BEACH_TOP],
                    "top": [round(tx, 2), ty, TERRACES[0][3]]})
    # wall stairs: every ~110 m along each wall, offset per wall so the way up zigzags
    for i, (wid, _) in enumerate(WALLS):
        y = CITY_Y0 + 30.0 + (i * 37.0) % 110.0
        n = 0
        while y < CITY_Y1 - 20.0:
            ws = wall_s(i, y)
            run = 13.0 if i > 0 else 7.0                  # the flight's length along the face
            bx, by = to_map(ws - 1.5, y - run / 2)
            tx, ty = to_map(ws + 1.0, y + run / 2)
            n += 1
            out.append({"id": "ST-%s-%02d" % (wid, n), "wall": wid, "kind": "wall_stair",
                        "bottom": [round(bx, 2), round(by, 2), TERRACES[i][3]],
                        "top": [round(tx, 2), round(ty, 2), TERRACES[i + 1][3]]})
            y += 110.0
    # the grand stair on the main axis Y 905 (W2..W5) and the cascade stair at the walls' bend
    for i in range(1, len(WALLS)):
        ws = wall_s(i, 905.0)
        bx, by = to_map(ws - 14.0, 905.0)
        tx, ty = to_map(ws + WALL_DEPTH, 905.0)
        out.append({"id": "ST-AXIS-%s" % WALLS[i][0], "wall": WALLS[i][0], "kind": "grand_stair", "width": 8.0,
                    "bottom": [round(bx, 2), by, TERRACES[i][3]], "top": [round(tx, 2), ty, TERRACES[i + 1][3]]})
    for i in range(1, len(WALLS)):
        ws = wall_s(i, 760.0)
        bx, by = to_map(ws - 14.0, 760.0)
        tx, ty = to_map(ws + WALL_DEPTH, 760.0)
        out.append({"id": "ST-GRAND-%s" % WALLS[i][0], "wall": WALLS[i][0], "kind": "grand_stair", "width": 10.0,
                    "bottom": [round(bx, 2), 760.0, TERRACES[i][3]], "top": [round(tx, 2), 760.0, TERRACES[i + 1][3]]})
    return out


DWELLING_W = 7.5          # a flat in the wall: 7.5 m of the face, two storeys (door + window below, two windows over)


def _near_stair(wid, y, st):
    for s in st:
        if s["wall"] == wid:
            y0, y1 = sorted((s["bottom"][1], s["top"][1]))
            if y0 - 4.0 <= y <= y1 + 4.0:
                return True
    return False


def wall_dwellings():
    """The flats built into the terrace walls W2..W5 (W1 holds shops and cafés facing the promenade): id, wall,
    door (map, on the lower terrace in front of the face), windows [(id, map centre)], yaw of the face. Where a stair,
    a plot or the town's fade is, the face is blank."""
    st = stairs()
    pl = plots()
    out = []
    for i, (wid, _) in enumerate(WALLS):
        if i == 0:
            continue
        y = CITY_Y0 + 10.0
        n = 0
        while y < CITY_Y1 - DWELLING_W:
            yc = y + DWELLING_W / 2
            y += DWELLING_W
            if city_mask(yc) < 1.0 or _near_stair(wid, yc, st):
                continue
            ws = wall_s(i, yc)
            if any(abs(p["s"] - ws) < p["d"] / 2 + 4 and abs(p["y_along"] - yc) < p["w"] / 2 + 4 for p in pl):
                continue
            n += 1
            z0 = TERRACES[i][3]
            dx, dy = to_map(ws - 0.6, yc - 2.0)
            wins = []
            for k, (du, dz) in enumerate(((1.6, 1.5), (-2.2, 4.9), (1.6, 4.9))):
                wx, wy = to_map(ws, yc + du)
                wins.append({"id": "%s-D%03d-w%d" % (wid, n, k + 1), "map": [round(wx, 2), round(wy, 2), round(z0 + dz, 2)]})
            out.append({"id": "%s-D%03d" % (wid, n), "wall": wid, "terrace": TERRACES[i][0],
                        "door": [round(dx, 2), round(dy, 2), z0], "yaw": wall_yaw(i, yc), "windows": wins, "beds": 2})
    return out


def shops_in_seawall():
    """Units in W1 (the promenade's back wall, 3.3 m): small shops and cafés opening onto the promenade."""
    out = []
    kinds = ["cafe", "shop", "kiosk", "workshop", "cafe", "shop"]
    names = [("Кафе «Чайка»", "Seagull café"), ("Рыбная лавка", "Fish shop"), ("Прокат", "Rentals"),
             ("Мастерская", "Repair shop"), ("Пельменная", "Dumpling bar"), ("Сувениры", "Souvenirs")]
    n = 0
    for y in range(500, 1400, 75):
        if abs(y - 905) < 60:
            continue
        x, yy = to_map(WALLS[0][1][0][1] - 0.3, float(y))
        k = n % len(kinds)
        n += 1
        out.append({"id": "W1-U%02d" % n, "kind": kinds[k], "name_ru": names[k][0], "name_en": names[k][1],
                    "door": [round(x, 2), yy, TERRACES[0][3]], "yaw": wall_yaw(0, float(y)), "w": 10.0})
    return out


def links():
    """How the town connects to the rest of the map."""
    return [
        {"id": "L-CABLE-ACADEM", "kind": "cable_car", "to": "the club (Aton station on the spur)", "map": list(W.STATION_DOWN),
         "notes": "existing; the Academgorodok's station on T1"},
        {"id": "L-CABLE-ZENIT", "kind": "cable_car", "map": [-470.0, 905.0], "notes": "existing station 'zenit', on T3"},
        {"id": "L-CABLE-AMBER", "kind": "cable_car", "map": [95.0, 640.0], "notes": "existing station 'amber', on the embankment T0"},
        {"id": "L-CLUB-STAIR", "kind": "stair", "notes": "the old stair from the town's south end up the spur's north slope "
                                                          "to the club's alley (36 m); the public-spaces thread draws it",
         "bottom": [20.0, 300.0, TERRACES[1][3]], "top": [10.0, 175.0, 0.0]},
        {"id": "L-BEACH-RAIL", "kind": "path", "notes": "the beach south to the railway's beach halt (docs/railway.md)",
         "map": [W.shore_x(380.0) - 10.0, 380.0, W.BEACH_TOP]},
    ]


def ue(p, face_y):
    return [round(p[0] * 100.0, 1), round(face_y + p[1] * 100.0, 1), round(p[2] * 100.0, 1)]


def build():
    face_y = None
    try:
        with open(os.path.join(HERE, "..", "blender", "export", "scene01_layout.json")) as f:
            face_y = json.load(f)["outside"]["portal_face_y"]
    except (OSError, KeyError, ValueError):
        pass
    data = {
        "about": "Lyudmilovsk's urban plan (tools/city_plan.py, docs/city_plan.md). map = world_map metres "
                 "(X west/sea, Y north, Z up); ue = Unreal cm (needs the portal face Y: portal_face_y). yaw = Unreal "
                 "degrees the front faces (0 = +X, the sea).",
        "portal_face_y": face_y,
        "city_y": [CITY_Y0, CITY_Y1],
        "terraces": [{"id": t[0], "name_ru": t[1], "name_en": t[2], "z": t[3]} for t in TERRACES],
        "walls": [{"id": wid, "below": TERRACES[i][0], "above": TERRACES[i + 1][0],
                   "height": round(TERRACES[i + 1][3] - TERRACES[i][3], 2), "depth": WALL_DEPTH,
                   "line": [[round(c, 2) for c in to_map(wall_s(i, y), y)] for y in range(int(CITY_Y0) - 40, int(CITY_Y1) + 41, 10)]}
                  for i, (wid, _) in enumerate(WALLS)],
        "building_types": {k: {"name_ru": v[0], "name_en": v[1], "after": v[2]} for k, v in BUILDING_TYPES.items()},
        "plots": plots(),
        "spots": spots(),
        "stairs": stairs(),
        "wall_dwellings": wall_dwellings(),
        "promenade_units": shops_in_seawall(),
        "links": links(),
    }
    if face_y is not None:
        for group in ("plots", "spots", "links"):
            for p in data[group]:
                if "map" in p and len(p["map"]) == 3:
                    p["ue"] = ue(p["map"], face_y)
        for d in data["wall_dwellings"]:
            d["door_ue"] = ue(d["door"], face_y)
            for w in d["windows"]:
                w["ue"] = ue(w["map"], face_y)
        for d in data["promenade_units"]:
            d["door_ue"] = ue(d["door"], face_y)
        for s in data["stairs"]:
            s["bottom_ue"], s["top_ue"] = ue(s["bottom"], face_y), ue(s["top"], face_y)
    return data


def plot_png(out, data):
    from PIL import Image, ImageDraw
    PX = 1.0
    x0, x1, y0, y1 = -1150.0, 330.0, 280.0, 1560.0
    w, h = int((x1 - x0) / PX), int((y1 - y0) / PX)
    img = Image.new("RGB", (w, h))
    pix = img.load()

    def px(x, y):
        return (x1 - x) / PX, (y1 - y) / PX
    for j in range(h):
        y = y1 - (j + 0.5) * PX
        for i in range(w):
            x = x1 - (i + 0.5) * PX
            z = W.height(x, y)
            if W.inland(x, y) < 0:
                pix[i, j] = (60, 100, 140)
            elif W.is_beach(x, y):
                pix[i, j] = (225, 210, 165)
            else:
                t = max(0.0, min(1.0, (z + 40.0) / 40.0))
                pix[i, j] = (int(110 + 120 * t), int(140 + 90 * t), int(100 + 80 * t))
    d = ImageDraw.Draw(img)
    for wall in data["walls"]:
        d.line([px(x, y) for x, y in wall["line"]], fill=(70, 70, 70), width=3)
    for p in data["plots"]:
        cx, cy, _ = p["map"]
        a = math.radians(p["yaw"])
        fx, fy = math.cos(a), math.sin(a)          # front (seaward-ish)
        ax, ay = -fy, fx                           # along
        hw, hd = p["w"] / 2, p["d"] / 2
        pts = [(cx + ax * u + fx * v, cy + ay * u + fy * v) for u, v in ((-hw, -hd), (hw, -hd), (hw, hd), (-hw, hd))]
        col = {"residential": (200, 120, 90), "square": (240, 230, 200), "park": (90, 170, 90), "fair": (230, 180, 60)}
        d.polygon([px(*q) for q in pts], outline=(30, 30, 30), fill=col.get(p["kind"], (170, 140, 200)))
        d.text(px(cx, cy), p["id"][2:], fill=(0, 0, 0))
    ax0, ax1, ay0, ay1 = W.ACADEM_RECT
    d.polygon([px(*W.academ_to_map(u, v)) for u, v in ((ax0, ay0), (ax1, ay0), (ax1, ay1), (ax0, ay1))],
              outline=(30, 30, 30), fill=(215, 190, 150))
    d.text(px(*W.academ_to_map(0, 0)), "ACADEMGORODOK", fill=(0, 0, 0))
    pr = [p for p in data["plots"] if p["id"] == "P-PIER"][0]
    d.line([px(*to_map(SEAWALL_S, 905.0)), px(*to_map(-140.0, 905.0))], fill=(120, 90, 60), width=12)
    for s in data["stairs"]:
        d.line([px(*s["bottom"][:2]), px(*s["top"][:2])], fill=(255, 255, 255), width=2)
    for dw in data["wall_dwellings"]:
        for wnd in dw["windows"]:
            q = px(*wnd["map"][:2])
            d.point(q, fill=(255, 230, 120))
    for s in data["spots"]:
        q = px(*s["map"][:2])
        d.ellipse([q[0] - 3, q[1] - 3, q[0] + 3, q[1] + 3], fill=(40, 90, 230))
    img.save(out)


def main(argv):
    data = build()
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(data, f, ensure_ascii=False, indent=1)
    print("wrote %s: %d plots, %d spots, %d stairs, %d wall dwellings (%d windows), %d promenade units" % (
        os.path.normpath(OUT), len(data["plots"]), len(data["spots"]), len(data["stairs"]), len(data["wall_dwellings"]),
        sum(len(d["windows"]) for d in data["wall_dwellings"]), len(data["promenade_units"])))
    if "--plot" in argv:
        plot_png(argv[argv.index("--plot") + 1], data)


if __name__ == "__main__":
    main(sys.argv[1:])
