#!/usr/bin/env python3
"""The people of the motor ship «Полдень» (docs/ship.md "People aboard"): her crew, the band, the tourists and the
townsfolk who take the weekend cruise, living aboard by the voyage's programme.

The ship is a building of the town (plot P-SHIP) standing far out at sea, so to the people's life (PeopleSim) she is
"island 4": a closed world with no way to or from the town. Everybody aboard is there on a "stay" (FPerson::StayCycle):
the crew from Friday 14:00 to Sunday 20:00, the passengers from the launch on Friday afternoon to Sunday evening, all of
them gone (the activity stay_gone, at the gangway) the rest of the week, and all the year out of season (the stay's
"when": the condition cruise_season the game switches). A townsperson who cruises is two records: the town's own record
gets the opposite stay ("patch": gone from the pier head on Friday afternoon till Sunday evening) and a twin c_<name>
with the same body lives aboard meanwhile. Two sets of six take turns, week and week about.

What it reads: blender/export/city/motor_ship.json (the ship's spots and things; what the export does not have yet is
taken from a stand-in made of the general arrangement's numbers, blender/city/buildings/_shipframe.py, and listed),
Content/City/city_plan.json (plot P-SHIP: where the ship stands), townsfolk.json and the town's names (read only).
What it writes: Content/PeopleData/ship_places.json, ship_activities.json, ship_people.json, the names
npc.ship_<id>.name|role and npc.c_<name>.name|role in Content/Localization/shippeople/strings_<lang>.json (other keys of those
tables are somebody else's and are kept), and its rows of Content/PeopleData/manners.json.

    python3 tools/ship/cruise_people.py            write what is stale
    python3 tools/ship/cruise_people.py --check    exit 1 when a file is not as this script makes it
    python3 tools/ship/cruise_people.py --doc F    also write the people's table (who, where, hour by hour) to F

tools/town/town_life.py is never rerun for this. The check: tools/peoplesim/run.sh cruise.
"""
import collections
import importlib.util
import json
import math
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
PD = os.path.join(CONTENT, "PeopleData")
EXPORT = os.path.join(ROOT, "blender", "export", "city", "motor_ship.json")
PLAN = os.path.join(CONTENT, "City", "city_plan.json")
FOLK = os.path.join(PD, "townsfolk.json")
TOWN_PLACES = os.path.join(PD, "town_places.json")
TOWN_STRINGS = os.path.join(CONTENT, "Localization", "town", "strings_%s.json")
OUT_PLACES = os.path.join(PD, "ship_places.json")
OUT_ACTS = os.path.join(PD, "ship_activities.json")
OUT_PEOPLE = os.path.join(PD, "ship_people.json")
MANNERS = os.path.join(PD, "manners.json")
STRINGS = os.path.join(CONTENT, "Localization", "shippeople", "strings_%s.json")
LANGS = ("ru", "en", "ja")
OD = collections.OrderedDict

PLOT = "P-SHIP"
ISLAND = 4
SEASON = "cruise_season"            # the people's-life condition the game switches (ULyudCruise): 15 April .. 15 November
# where the ship stands when the plan has no P-SHIP yet: the cruise stage, map (26000, 4000), terrace "B" over the sea
STAND_IN_FRONT, STAND_IN_YAW = (26000.0, 4000.0, -41.5), 0.0

_spec = importlib.util.spec_from_file_location("_shipframe", os.path.join(ROOT, "blender", "city", "buildings", "_shipframe.py"))
S = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(S)
ZD, ZC, ZB, ZA, ZBR = S.Z_D, S.Z_C, S.Z_B, S.Z_A, S.Z_BRIDGE
DECKS = (("D", ZD), ("C", ZC), ("B", ZB), ("A", ZA), ("BR", ZBR))

FRI, SAT, SUN = 4, 5, 6
VOYAGE = [FRI, SAT, SUN]


# ------------------------------------------------------------------------------------------------ the people
def T(*v):
    return OD(zip(("social", "active", "studious", "gamer", "nature", "sun"), v))


def P(key, ru, en, ja, role, age, female, occ, wake, bed, temper, likes, speed=None, smoker=False):
    return OD(key=key, name=(ru, en, ja), role=role, age=age, female=female, occ=occ, wake=wake, bed=bed, temper=temper,
              likes=likes, speed=speed, smoker=smoker)


# role words: ru (m, f), en, ja
ROLES = {
    "captain": (("капитан теплохода «Полдень»",) * 2, "master of the motor ship Polden", "「ポルデニ号」船長"),
    "mate": (("старший помощник капитана",) * 2, "chief mate", "一等航海士"),
    "bosun": (("боцман",) * 2, "bosun", "甲板長"),
    "purser": (("пассажирский помощник",) * 2, "purser", "パーサー"),
    "barman": (("буфетчик музыкального салона",) * 2, "barman of the music saloon", "サロンのバーテンダー"),
    "steward": (("стюард", "бортпроводница"), "stewardess", "客室係"),
    "cook": (("кок", "судовой повар"), "ship's cook", "船のコック"),
    "radio": (("начальник радиостанции",) * 2, "radio operator", "通信士"),
    "engineer": (("старший механик",) * 2, "chief engineer", "機関長"),
    "helmsman": (("рулевой матрос",) * 2, "helmsman", "操舵手"),
    "entertainer": (("культорганизатор",) * 2, "cruise entertainer", "催し物係"),
    "pianist": (("пианист, руководитель «Полуденного квартета»",) * 2, "pianist, leader of the Noon Quartet", "ピアニスト、「正午カルテット」のリーダー"),
    "bassist": (("контрабасист «Полуденного квартета»",) * 2, "double bass of the Noon Quartet", "「正午カルテット」のコントラバス"),
    "drummer": (("ударник «Полуденного квартета»",) * 2, "drummer of the Noon Quartet", "「正午カルテット」のドラム"),
    "reeds": (("кларнет и саксофон «Полуденного квартета»",) * 2, "clarinet and saxophone of the Noon Quartet", "「正午カルテット」のクラリネットとサックス"),
    "tourist": (("турист", "туристка"), "cruise passenger", "クルーズの乗客"),
    "passenger": (("пассажир теплохода", "пассажирка теплохода"), "cruise passenger", "クルーズの乗客"),
}

LOOK = {"ship_rail_look": 0.9, "ship_deck_walk": 0.8, "ship_bench": 0.5, "ship_saloon_sit": 0.4}
CREW = [
    P("ship_gromov", "Арсений Павлович Громов", "Arseny Pavlovich Gromov", "アルセーニー・グロモフ", "captain", 60, 0, "ship_captain", 7.0, 23.5,
      T(0.5, 0.5, 0.7, 0.3, 0.8, 0.6), {"ship_rail_look": 1.0, "ship_saloon_sit": 0.9, "ship_library_read": 0.6, "ship_deck_walk": 0.6}),
    P("ship_vais", "Ирина Сергеевна Вайс", "Irina Sergeyevna Vais", "イリーナ・ヴァイス", "mate", 38, 1, "ship_mate", 23.5, 16.5,
      T(0.4, 0.6, 0.9, 0.2, 0.7, 0.5), {"ship_rail_look": 0.9, "ship_library_read": 0.9, "ship_deck_walk": 0.5}),
    P("ship_taras", "Тарас Нечипоренко", "Taras Nechiporenko", "タラス・ネチポレンコ", "bosun", 52, 0, "ship_bosun", 6.5, 0.75,
      T(0.9, 0.8, 0.3, 0.7, 0.6, 0.6), {"ship_smoke": 2.6, "ship_deck_walk": 0.8, "ship_bar": 0.5, "ship_bench": 0.6}, smoker=True),
    P("ship_nonna", "Нонна Аркадьевна Лисицкая", "Nonna Arkadyevna Lisitskaya", "ノンナ・リシツカヤ", "purser", 47, 1, "ship_purser", 6.75, 23.5,
      T(0.9, 0.4, 0.6, 0.3, 0.4, 0.4), {"ship_saloon_sit": 0.8, "ship_bench": 0.5}),
    P("ship_givi", "Гиви Цулукидзе", "Givi Tsulukidze", "ギヴィ・ツルキゼ", "barman", 44, 0, "ship_barman", 9.0, 1.5,
      T(1.0, 0.5, 0.4, 0.7, 0.4, 0.6), {"ship_smoke": 2.4, "ship_rail_look": 0.6, "ship_bench": 0.5}, smoker=True),
    P("ship_zoya", "Зоя Ким", "Zoya Kim", "ゾーヤ・キム", "steward", 23, 1, "ship_steward", 6.25, 23.0,
      T(0.6, 0.7, 0.9, 0.3, 0.5, 0.6), {"ship_library_read": 1.2, "ship_deck_read": 1.0, "ship_rail_look": 0.6}),
    P("ship_pasha", "тётя Паша", "Aunt Pasha", "パーシャおばさん", "cook", 58, 1, "ship_cook", 5.5, 21.5,
      T(0.8, 0.5, 0.3, 0.2, 0.5, 0.5), {"ship_bench": 1.0, "ship_rail_look": 0.6}),
    P("ship_lev", "Лев Маркович Штерн", "Lev Markovich Shtern", "レフ・シュテルン", "radio", 66, 0, "ship_radio", 7.0, 23.75,
      T(0.6, 0.3, 0.8, 0.8, 0.4, 0.4), {"ship_library_read": 0.9, "ship_saloon_sit": 0.7, "ship_bench": 0.5}),
    P("ship_einar", "Эйнар Каск", "Einar Kask", "エイナル・カスク", "engineer", 57, 0, "ship_engineer", 6.5, 22.5,
      T(0.2, 0.5, 0.6, 0.3, 0.6, 0.4), {"ship_smoke": 2.6, "ship_rail_look": 0.9, "ship_bench": 0.4}, smoker=True),
    P("ship_vitya", "Витя Чиж", "Vitya Chizh", "ヴィーチャ・チーシ", "helmsman", 19, 0, "ship_helmsman", 7.0, 0.5,
      T(0.7, 0.9, 0.4, 0.7, 0.7, 0.7), {"ship_deck_walk": 0.9, "ship_rail_look": 0.8, "ship_bench": 0.5, "ship_bar": 0.4}),
    P("ship_rimma", "Римма Вольская", "Rimma Volskaya", "リンマ・ヴォリスカヤ", "entertainer", 35, 1, "ship_entertainer", 6.5, 23.75,
      T(1.0, 0.8, 0.5, 0.5, 0.5, 0.7), {"ship_saloon_sit": 0.9, "ship_deck_walk": 0.7, "ship_bar": 0.5}),
]
BAND = [
    P("ship_blum", "Савелий Блюм", "Saveliy Blum", "サヴェーリー・ブリューム", "pianist", 61, 0, "ship_musician", 9.5, 1.25,
      T(0.7, 0.3, 0.7, 0.8, 0.4, 0.4), {"ship_saloon_sit": 1.0, "ship_bar": 0.9, "ship_smoke": 2.2, "ship_library_read": 0.5}, smoker=True),
    P("ship_otar", "Отар", "Otar", "オタル", "bassist", 40, 0, "ship_musician", 9.5, 0.5,
      T(0.7, 0.5, 0.4, 0.5, 0.5, 0.6), {"ship_smoke": 2.6, "ship_bar": 1.0, "ship_rail_look": 0.6}, smoker=True),
    P("ship_kostya", "Костя Ёж", "Kostya Yozh", "コースチャ・ヨーシ", "drummer", 27, 0, "ship_musician", 9.5, 0.5,
      T(0.8, 0.8, 0.3, 0.8, 0.5, 0.7), {"ship_bar": 1.0, "ship_deck_walk": 0.8, "ship_deckchair": 0.8, "ship_smoke": 2.0}, smoker=True),
    P("ship_asta", "Аста Линд", "Asta Lind", "アスタ・リンド", "reeds", 31, 1, "ship_musician", 9.5, 0.5,
      T(0.6, 0.6, 0.7, 0.3, 0.8, 0.7), {"ship_rail_look": 1.0, "ship_deck_read": 0.9, "ship_deckchair": 0.7, "ship_bar": 0.5}),
]
TOURISTS = [
    P("ship_vera", "Вера Игнатьевна", "Vera Ignatyevna", "ヴェーラ・イグナーチエヴナ", "tourist", 68, 1, "ship_tourist", 6.25, 23.25,
      T(0.7, 0.5, 1.0, 0.7, 0.8, 0.5), {"ship_library_read": 1.3, "ship_rail_look": 0.9, "ship_bench": 0.6, "ship_deck_read": 0.7}, 1.05),
    P("ship_misha", "Миша", "Misha", "ミーシャ", "tourist", 26, 0, "ship_tourist", 7.0, 23.75,
      T(0.8, 0.7, 0.4, 0.5, 0.6, 0.7), {"ship_bench": 1.0, "ship_rail_look": 0.9, "ship_saloon_sit": 0.8, "ship_deck_walk": 0.8, "ship_bar": 0.5}),
    P("ship_olya", "Оля", "Olya", "オーリャ", "tourist", 24, 1, "ship_tourist", 7.0, 23.75,
      T(0.8, 0.7, 0.5, 0.3, 0.7, 0.8), {"ship_bench": 1.0, "ship_rail_look": 0.9, "ship_saloon_sit": 0.8, "ship_deck_walk": 0.8, "ship_deckchair": 0.6}),
    P("ship_albert", "Альберт", "Albert", "アルベルト", "tourist", 49, 0, "ship_tourist", 8.25, 1.25,
      T(0.8, 0.3, 0.6, 1.0, 0.3, 0.4), {"ship_smoke": 2.8, "ship_bar": 1.0, "ship_saloon_sit": 0.6}, smoker=True),
    P("ship_haruto", "Харуто Мори", "Haruto Mori", "森 春人", "tourist", 55, 0, "ship_tourist", 5.0, 22.25,
      T(0.4, 0.6, 0.8, 0.2, 1.0, 0.7), {"ship_rail_look": 1.6, "ship_deck_walk": 0.9, "ship_deckchair": 0.5, "ship_library_read": 0.5}),
    P("ship_senya", "Сеня", "Senya", "セーニャ", "tourist", 10, 0, "ship_tourist", 6.75, 22.0,
      T(0.7, 1.0, 0.5, 0.8, 0.8, 0.8), {"ship_deck_walk": 1.2, "ship_rail_look": 0.9, "ship_deckchair": 0.6}),
    P("ship_faina", "бабушка Фаина", "Granny Faina", "ファイーナおばあさん", "tourist", 67, 1, "ship_tourist", 6.5, 22.25,
      T(0.8, 0.4, 0.5, 0.3, 0.6, 0.6), {"ship_deckchair": 1.2, "ship_bench": 1.0, "ship_deck_read": 0.6, "ship_saloon_sit": 0.5}, 1.0),
    P("ship_inga", "Инга", "Inga", "インガ", "tourist", 33, 1, "ship_tourist", 5.0, 23.5,
      T(0.5, 0.8, 0.6, 0.3, 0.9, 0.9), {"ship_rail_look": 1.4, "ship_deck_walk": 1.3, "ship_bench": 0.4}),
]
# the townsfolk who cruise: two sets, week and week about (set 0 while FWorld::Week is even). Chosen (docs/ship.md):
# nobody with a duty from Friday afternoon to Sunday, a stay, a pet, an allotment, a book-club evening, a station trip
# or a line in a quest; couples go together. key (without t_), cabin
TWIN_SETS = [
    [("ozturk_karim", 4), ("ozturk_colette", 4), ("novotny_dmitry", 5), ("obrien_ananya", 5), ("navarro_tigran", 6), ("lund_parisa", 8)],
    [("kowalski_karim", 9), ("nakamura_shira", 9), ("zielinski_ines", 10), ("grabowski_akosua", 16), ("bakker_ursula", 17),
     ("wisniewski_jihoon", 18)],
]
# the tourists' cabins (7, 11, 15 are the hero's by his ticket: nobody's)
CABINS = {"ship_vera": 1, "ship_faina": 2, "ship_senya": 2, "ship_inga": 3, "ship_misha": 12, "ship_olya": 12, "ship_albert": 13,
          "ship_haruto": 14}
BERTHS = {n: 2 for n in (1, 2, 3, 4, 11, 12, 13, 14)}
BERTHS.update({n: 4 for n in (5, 6, 7, 8, 9, 10, 15, 16, 17, 18, 19, 20)})
HERO_CABINS = (7, 11, 15)
# what no NPC ever takes (the hero's): his dining chair, his chair at the cards, and a few seats about the ship
HERO_CHAIR = "dining_chair_1"
HERO_FREE = {"dining_chair_1", "card_chair_1", "deck_chair_1", "deck_chair_2", "bar_stool_1", "saloon_seat_1", "saloon_seat_2",
             "saloon_seat_3", "saloon_seat_4", "library_chair_1", "prom_bench_1", "smoking_bench_1"}
HERO_TABLE = ("ship_vera", "ship_senya", "ship_faina")         # at the hero's table, on the chairs nearest to his
CAPTAINS_TABLE = ("ship_gromov", "ship_haruto", "ship_inga")    # and one chair of it left free (an invitation)
SMOKER_TWINS = ("navarro_tigran", "wisniewski_jihoon")


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f, object_pairs_hook=OD)


def dump(d):
    return json.dumps(d, ensure_ascii=False, indent=1) + "\n"


def r2(v):
    return [round(v[0], 2), round(v[1], 2), round(v[2], 2)]


# ------------------------------------------------------------------------------------------------ the ship's frame
class Ship:
    """Ship frame (x from the stern to the bow, y port negative, z over the waterline; _shipframe.py) -> the world."""

    def __init__(self):
        plan = load(PLAN)
        plot = next((p for p in plan["plots"] if p["id"] == PLOT), None)
        self.planned = plot is not None
        if plot:
            self.front, self.yaw = list(plot["front"]), plot["yaw"]
        else:
            f = STAND_IN_FRONT
            self.front, self.yaw = [f[0], f[1] + plan["portal_face_y"] / 100.0, f[2]], STAND_IN_YAW

    def world(self, p):
        """Ship point -> Unreal metres (tools/city/interiors.py local_to_world)."""
        x, y, z = S.p(*p)
        a = math.radians(self.yaw)
        return r2([self.front[0] + x * math.sin(a) - y * math.cos(a), self.front[1] - x * math.cos(a) - y * math.sin(a), self.front[2] + z])

    def wyaw(self, ship_yaw):
        """Ship yaw (0 to the bow, 90 to starboard) -> Unreal degrees (interiors.py world_yaw of the kit yaw)."""
        return round((self.yaw - S.yaw(ship_yaw)) % 360.0, 1)


def room_of(x, y, deck):
    for name, r in S.ROOMS.items():
        if r["deck"] == deck and r["x"][0] <= x <= r["x"][1] and r["y"][0] <= y <= r["y"][1]:
            return name
    return ""


# ------------------------------------------------------------------------------------------------ the export, the stand-in
class Item:
    """A spot or a seat/bed thing of the ship (ship frame). The export gives a seat's or a berth's point on its top (the
    things, the berths of decks C and D) or on the floor under it (the spots of decks B and A): the floor is the deck
    it stands on either way, the height what is left (the usual one for a point on the floor)."""

    def __init__(self, iid, kind, pos, yaw, height=None, thing=False, real=True, hold=None):
        self.id, self.kind, self.pos, self.yaw, self.thing, self.real = iid, kind, tuple(pos), yaw % 360.0, thing, real
        self.hold = hold or iid         # the city thing held while somebody is at it (a bench's, for each of its places)
        x, _y, z = self.pos
        self.deck = max((d for d in DECKS if d[1] <= z + 0.05), key=lambda d: d[1], default=DECKS[0])[0]
        usual = 0.45 if kind in ("seat", "bench") else 0.47 if kind == "bed" else 0.0
        if usual == 0.0:
            self.floor, self.height = z, 0.0        # where somebody stands: the point is the floor (the bandstand, the foredeck's ramp)
        else:
            surface = S.foredeck_z(x) if self.deck == "C" else dict(DECKS)[self.deck]
            self.floor = surface
            self.height = round(z - surface, 2) if z - surface > 0.06 else (height if height else usual)


def cabin_room(n):
    return "lux%d" % n if 11 <= n <= 14 else "cabin%d" % n


def stand_in():
    """Every spot the people need, from the general arrangement's numbers (R1 section 5, _shipframe.ROOMS): what stands
    in for the export while the modellers have not made a spot (ship frame; seats: the seat's top)."""
    out = []

    def add(iid, kind, x, y, z, yaw=0.0, h=None):
        out.append(Item(iid, kind, (x, y, z), yaw, h, False, False))

    add("gangway", "door", 31.1, 4.75, ZC, 90)
    add("captains_table", "board", 9.75, 0.0, ZC + 0.75)
    for sid, x, y, z, yaw in (("staff_captain", 40.6, 0.9, ZBR, 0), ("staff_mate", 40.4, -1.0, ZBR, 0), ("staff_helmsman", 41.0, 0.0, ZBR, 0),
                              ("staff_purser", 28.6, -3.7, ZC, 90), ("staff_barman", 34.2, -2.6, ZB, 60), ("staff_steward", 17.2, -1.9, ZC, 180),
                              ("staff_cook", 23.5, -3.4, ZC, 270), ("staff_bosun", 52.5, 1.6, S.foredeck_z(52.5), 0),
                              ("staff_radio", 35.4, -1.9, ZA, 270), ("staff_engineer", 23.8, 0.9, ZC, 270), ("staff_rimma", 37.2, 2.6, ZB, 250),
                              ("band_piano", 43.2, -1.6, ZB + 0.2, 200), ("band_bass", 43.3, 1.5, ZB + 0.2, 160),
                              ("band_drums", 44.0, 0.0, ZB + 0.2, 180), ("band_reed", 42.7, 0.8, ZB + 0.2, 180)):
        add(sid, "work", x, y, z, yaw)
    # the dining saloon: ten tables of four, the captain's aft on the centreline (table 10)
    tables = [(x, y) for x in (15.6, 12.6) for y in (-3.4, -1.75, 1.75, 3.4)] + [(9.7, -2.6), (9.75, 0.0)]
    for k, (tx, ty) in enumerate(tables):
        for j, (dx, dy, yaw) in enumerate(((0.7, 0.0, 180), (-0.7, 0.0, 0), (0.0, 0.62, 270), (0.0, -0.62, 90))):
            add("dining_chair_%d" % (k * 4 + j + 1), "seat", tx + dx, ty + dy, ZC + 0.45, yaw)
    # the music saloon: armchairs along the sides, stools at the bar, the card table, the dance floor
    for i in range(24):
        side = -1 if i % 2 else 1
        add("saloon_seat_%d" % (i + 1), "seat", 37.3 + (i // 2) * 0.42, side * (2.95 - 0.5 * ((i // 2) % 2)), ZB + 0.42, 270 if side > 0 else 90, 0.42)
    for i in range(5):
        add("bar_stool_%d" % (i + 1), "seat", 34.0 + i * 0.62, -1.35, ZB + 0.75, 180 + 90, 0.75)
    for i in range(5):
        a = math.radians(72 * i)
        add("card_chair_%d" % (i + 1), "seat", 34.9 + 1.0 * math.cos(a), 2.3 + 1.0 * math.sin(a), ZB + 0.45, math.degrees(math.atan2(-math.sin(a), -math.cos(a))) % 360)
    for i in range(8):
        add("dance_%d" % (i + 1), "stand", 38.3 + (i % 4) * 1.0, -1.0 + (i // 4) * 2.0, ZB)
    add("bar_stand_1", "stand", 36.9, -1.4, ZB, 240)
    for i in range(2):
        add("library_chair_%d" % (i + 1), "seat", 28.4 + i * 1.6, -2.6, ZB + 0.42, 0, 0.42)
    for i in range(4):
        add("smoking_bench_%d" % (i + 1), "seat", 10.9 - 2.3 * (i // 2), (-1 if i % 2 else 1) * (2.9 if i < 2 else 0.9), ZB + 0.45, 180)
    for i in range(6):
        add("prom_bench_%d" % (i + 1), "seat", 16.0 + (i // 2) * 9.0, (-1 if i % 2 else 1) * 3.75, ZB + 0.45, 270 if i % 2 else 90)
    for i in range(2):
        add("funnel_bench_%d" % (i + 1), "seat", 23.5, (-1 if i else 1) * 1.65, ZA + 0.45, 270 if i else 90)
    for i in range(8):
        add("deck_chair_%d" % (i + 1), "bed", 8.5 + (i // 2) * 2.4, (-1 if i % 2 else 1) * 2.5, ZA + 0.35, 180, 0.35)
    for i in range(12):
        add("muster_%d" % (i + 1), "stand", 21.5 + (i // 2) * 1.2, (-1 if i % 2 else 1) * 2.9, ZA, 270 if i % 2 else 90)
    # the rails: both promenades, the smoking deck's stern, the observation walk, the boat deck
    rails = [(x, s * 4.75, ZB, 90 if s > 0 else 270) for x in (14.0, 20.0, 26.0, 34.5, 40.5) for s in (1, -1)]
    rails += [(2.9, -1.2, ZB, 180), (2.9, 1.0, ZB, 180), (47.4, -1.0, ZB, 0), (47.4, 1.0, ZB, 0)]
    rails += [(x, s * (3.25 if 10.5 < x < 29 else 4.75), ZA, 90 if s > 0 else 270) for x in (8.0, 19.8, 30.5, 33.0) for s in (1, -1)]
    for i, (x, y, z, yaw) in enumerate(rails):
        add("rail_%d" % (i + 1), "stand", x, y, z, yaw)
    # the berths: lower ones along the side, the upper (Pullman) ones over them; the crew's sixteen aft on deck D
    for n in sorted(BERTHS):
        x0, x1, y0, y1, z = S.room(cabin_room(n))
        side = 1 if y0 > 0 else -1
        ya, yb = (y0 + 0.55, y1 - 0.55) if side > 0 else (y1 - 0.55, y0 + 0.55)
        for b in range(BERTHS[n]):
            lux = 11 <= n <= 14
            x = (x0 + 0.75, x1 - 0.75)[b % 2] if lux else (x0 + x1) / 2 + 0.25
            y = (ya + yb) / 2 if lux else (ya, yb)[b % 2]
            add("%s_bed_%s" % ("lux%d" % n if lux else "cabin%d" % n, "abcd"[b]), "bed", x, y, z + (0.5 if b < 2 else 1.45), 0, 0.5 if b < 2 else 1.45)
    for i in range(16):
        add("crew_berth_%d" % (i + 1), "bed", 9.2 + (i // 4) * 2.2, (-3.5, -1.6, 1.6, 3.5)[i % 4], ZD + (0.5 if i % 2 == 0 else 0.5), 0, 0.5)
    return out


def read_export():
    """The ship's spots and seat/bed things as Items in the ship frame; ([], note) while there is no export."""
    if not os.path.exists(EXPORT):
        return [], "no blender/export/city/motor_ship.json yet"
    ex = load(EXPORT)
    out, seen = [], set()
    for t in ex.get("things", []):
        props = t.get("props") or {}
        if t["kind"] in ("seat", "bed"):
            h = float(props.get("height", 0.45 if t["kind"] == "seat" else 0.5))
            out.append(Item(t["id"], t["kind"], S.unp(*t["pos"]), 90.0 - t.get("yaw", 0.0), h, True))
            seen.add(t["id"])
        elif t["id"] in ("gangway", "captains_table") or re.match(r"(cabin|lux)\d+_door$", t["id"]):
            out.append(Item(t["id"], t["kind"], S.unp(*t["pos"]), 90.0 - t.get("yaw", 0.0), 0.0, True))
    for s in ex.get("spots", []):
        if s["id"] in seen:
            continue        # (a seat's spot: the thing has its height)
        out.append(Item(s["id"], s["kind"], S.unp(*s["pos"]), 90.0 - s.get("yaw", 0.0)))
    return out, ""


class Fittings:
    """What the people use aboard, sorted out of the export (and the stand-in for what it lacks)."""

    def __init__(self):
        real, self.note = read_export()
        fake = stand_in()
        self.standin = []           # what was taken from the stand-in
        self.all = real
        ids = {i.id: i for i in real}
        fake_ids = {i.id: i for i in fake}

        def one(iid):
            if iid not in ids:
                self.standin.append(iid)
            return ids.get(iid) or fake_ids[iid]

        def numbered(prefix, n, kinds=("seat", "bed", "bench")):
            got = []
            for k in range(1, n + 1):
                iid = "%s%d" % (prefix, k)
                if iid in ids and ids[iid].kind in kinds:
                    got.append(ids[iid])
                else:
                    self.standin.append(iid)
                    got.append(fake_ids[iid])
            return got

        self.gangway = one("gangway")
        self.captains_table = one("captains_table")
        self.staff = {k: one(k) for k in fake_ids if k.startswith("staff_") or k.startswith("band_")}
        self.dining = numbered("dining_chair_", 40)
        self.saloon = numbered("saloon_seat_", 24)
        self.stools = numbered("bar_stool_", 5)
        self.cards = numbered("card_chair_", 5)
        self.library = numbered("library_chair_", 2)
        # the chess table's two chairs beside them, when the modellers gave them: more places to read
        self.library += [ids[c] for c in ("chess_seat_1", "chess_seat_2") if c in ids and ids[c].kind == "seat"]
        def places_of(benches):
            """A bench's places (the export's spots <bench>_a, _b), each holding the bench; else the bench itself."""
            got = []
            for b in benches:
                own = [i for i in real if re.match(re.escape(b.id) + r"_[a-c]$", i.id)]
                got += [Item(i.id, "seat", (i.pos[0], i.pos[1], b.pos[2]), b.yaw, thing=b.thing, hold=b.id) for i in own] or [b]
            return got

        self.smoking = places_of(numbered("smoking_bench_", 4))
        self.prom = places_of(numbered("prom_bench_", 6))
        self.funnel = places_of(numbered("funnel_bench_", 2))
        self.deckchairs = numbered("deck_chair_", 8)
        # stand spots, by where they are
        stands = [i for i in real if i.kind == "stand"]

        def stands_where(pred, fallback, least):
            got = [i for i in stands if pred(i)]
            if len(got) < least:
                self.standin.append("%s (%d of %d stand spots)" % (fallback, len(got), least))
                got = [i for i in fake if i.id.startswith(fallback)]
            return got

        def in_room(i, *rooms):
            return room_of(i.pos[0], i.pos[1], i.deck) in rooms

        self.dance = stands_where(lambda i: "dance" in i.id or (in_room(i, "music_saloon") and 37.3 <= i.pos[0] <= 42.1 and abs(i.pos[1]) <= 2.1
                                                                and "bar" not in i.id), "dance_", 6)
        self.muster = stands_where(lambda i: "muster" in i.id, "muster_", 10)
        self.barstand = stands_where(lambda i: "bar" in i.id and i.deck == "B", "bar_stand_", 1)
        used = {id(i) for i in self.dance + self.muster + self.barstand}
        self.rails = stands_where(lambda i: id(i) not in used and re.match(r"(rail_|stand_bow_|stand_aft_)", i.id), "rail_", 10)
        self.exercise = [i for i in stands if i.id.startswith("exercise_")]
        self.wings = [i for i in stands if re.match(r"(wing_|stand_bridge_)", i.id)]
        self.smoke_stands = [i for i in stands if i.id.startswith("smoke_")]
        self.rimma_deck = ids.get("staff_rimma_deck")
        # berths by the room they are in
        beds = [i for i in real if i.kind == "bed" and not i.id.startswith("deck_chair")]
        self.berths = {}
        for n in sorted(BERTHS):
            got = sorted((i for i in beds if room_of(i.pos[0], i.pos[1], i.deck) == cabin_room(n)), key=lambda i: (i.height > 1.0, i.id))
            if len(got) < min(BERTHS[n], 2):
                if n not in HERO_CABINS:
                    self.standin.append("cabin %d berths" % n)
                got = [i for i in fake if re.match(r"(cabin|lux)%d_bed_" % n, i.id)]
            self.berths[n] = got
        crew = sorted((i for i in beds if i.deck == "D" and i.pos[0] < 18.5), key=lambda i: (len(i.id), i.id))
        if len(crew) < 15:
            self.standin.append("crew berths (%d of 16)" % len(crew))
            crew = [i for i in fake if i.id.startswith("crew_berth_")]
        self.crew_berths = crew
        self.doors = {int(re.search(r"\d+", i.id).group()): i for i in real if re.match(r"(cabin|lux)\d+_door$", i.id)}
        self.paths = [i for i in real if i.kind == "path"]


# ------------------------------------------------------------------------------------------------ the ways aboard
class Ways:
    """The ways people walk aboard: a tree of points from the gangway (ship frame). A slot's way in is the tree's way
    to the node of its room, then along that node's line to abreast of the slot, then to the slot."""

    def __init__(self, fit):
        g = fit.gangway.pos
        sg = 1.0 if g[1] >= 0 else -1.0
        self.sg = sg
        self.parent, self.pt, self.line = {}, {}, {}

        def node(name, parent, x, y, z):
            self.parent[name], self.pt[name] = parent, (round(x, 2), round(y, 2), round(z, 2))
            return name

        def chain(prefix, parent, pts):
            """A line of nodes one after another: slots hang off the nearest point of it."""
            names = []
            for k, (x, y, z) in enumerate(pts):
                parent = node("%s%d" % (prefix, k), parent, x, y, z)
                names.append(parent)
            for n in names:
                self.line[n] = names
            return names[-1]

        def stair(prefix, parent, z0, z1):
            """The main stair's two flights between decks at z0 and z1 (_shipframe.STAIRWELL): starboard up, port down."""
            up = z1 > z0
            ya, yb = (0.75, -0.75) if up else (-0.75, 0.75)
            zm = (z0 + z1) / 2
            a = node(prefix + "a", parent, 30.9, ya, z0)
            b = node(prefix + "b", a, 28.0, ya, zm)
            c = node(prefix + "c", b, 28.0, yb, zm)
            return node(prefix + "d", c, 30.9, yb, z1)

        # deck C: the vestibule (the gangway door, the strip before the stair), the corridors off it
        node("G", None, g[0], sg * 4.2, ZC)
        node("Vg", "G", 31.5, sg * 2.3, ZC)
        node("C0", "Vg", 31.5, 0.0, ZC)
        node("Vo", "C0", 31.5, -sg * 2.3, ZC)
        vs, vp = ("Vg", "Vo") if sg > 0 else ("Vo", "Vg")
        chain("VP", vp, [(29.0, -2.3, ZC)])                        # by the purser's desk (port)
        chain("VS", vs, [(29.0, 2.3, ZC)])                         # by the kiosk (starboard)
        chain("KC", "VS0", [(26.5, 2.25, ZC), (18.7, 2.25, ZC)])   # the starboard corridor aft to the dining saloon's door
        node("EG", "KC0", 23.55, 2.25, ZC)                         # (abreast of the casing's door: the engine gallery)
        chain("EGL", "EG", [(23.55, 0.9, ZC)])
        node("DN", "KC1", 17.3, 2.25, ZC)
        chain("AI", "DN", [(16.5, 0.0, ZC), (13.5, 0.0, ZC), (11.0, 0.0, ZC)])     # the saloon's middle aisle
        node("AIa", "AI2", 9.2, 1.9, ZC)                           # round the captain's table to the aft doors
        node("AIb", "AIa", 8.6, 0.0, ZC)
        chain("MD", "AIb", [(7.2, 0.0, ZC), (5.0, 0.0, ZC), (5.0, -2.2, ZC)])      # the aft mooring deck
        node("SVD", "AI0", 17.3, -3.05, ZC)                        # the service door, the servery, the galley
        chain("SV", "SVD", [(19.0, -3.05, ZC)])
        chain("GA", "SV0", [(20.8, -3.05, ZC), (26.0, -3.05, ZC)])
        # the crew's stair down from the servery to their quarters on deck D
        node("QS", "SV0", 17.9, -3.65, ZC)
        node("QF", "QS", 16.3, -3.65, ZD)
        chain("QQ", "QF", [(16.0, -2.0, ZD), (12.0, 0.0, ZD), (9.0, 0.0, ZD)])
        chain("CF", "C0", [(32.6, 0.0, ZC), (45.6, 0.0, ZC)])      # the cabin corridor forward
        chain("FD", "CF1", [(46.6, 0.0, ZC), (46.5, 2.4, S.foredeck_z(46.5)), (50.5, 2.3, S.foredeck_z(50.5)),
                            (54.0, 0.0, S.foredeck_z(54.0)), (58.0, 0.0, S.foredeck_z(58.0))])
        # deck D by the main stair: the hold cabins' corridor
        d = stair("SD", "C0", ZC, ZD)
        node("D0", d, 31.8, 0.0, ZD)
        chain("DF", "D0", [(33.2, 0.0, ZD), (43.2, 0.0, ZD)])
        # deck B: the lobby, the saloon, the library corner, the promenades, the lux cabins, the smoking deck
        b = stair("SB", "C0", ZC, ZB)
        node("B0", b, 31.8, 0.0, ZB)
        chain("SA", "B0", [(33.7, 0.0, ZB), (37.0, 0.0, ZB), (41.9, 0.0, ZB)])
        chain("BS", "SA2", [(42.6, 0.0, ZB + 0.2), (43.6, 0.0, ZB + 0.2)])     # up onto the bandstand
        node("LB", "B0", 31.8, -2.4, ZB)
        chain("LI", "LB", [(29.4, -2.4, ZB)])
        node("LN", "B0", 31.8, 2.4, ZB)
        node("PP", "LB", 32.2, -4.2, ZB)
        node("PS", "LN", 32.2, 4.2, ZB)
        chain("PPA", "PP", [(31.0, -4.2, ZB), (12.4, -4.2, ZB)])
        chain("PSA", "PS", [(31.0, 4.2, ZB), (12.4, 4.2, ZB)])
        chain("PPF", "PP", [(33.4, -4.2, ZB), (42.4, -4.2, ZB), (45.7, -2.9, ZB), (47.0, 0.0, ZB)])
        chain("PSF", "PS", [(33.4, 4.2, ZB), (42.4, 4.2, ZB), (45.7, 2.9, ZB)])
        chain("SM", "PSA1", [(11.4, 4.2, ZB), (9.0, 3.6, ZB), (9.0, 0.3, ZB), (6.0, -0.6, ZB), (3.6, 0.3, ZB)])
        chain("BC", "LN", [(27.4, 2.4, ZB), (19.25, 2.4, ZB), (19.25, 0.0, ZB)])
        chain("BL", "BC2", [(18.3, 0.0, ZB), (12.4, 0.0, ZB)])     # between the lux cabins
        # deck A: out of the stair hood by its side doors; aft to the sun deck, forward to the officers' house, the bridge
        a = stair("SA_", b, ZB, ZA)
        node("A0", a, 31.65, 0.0, ZA)
        node("AP", "A0", 31.65, -2.5, ZA)
        node("AS", "A0", 31.65, 2.5, ZA)
        chain("APA", "AP", [(30.0, -3.2, ZA), (26.5, -2.6, ZA), (20.6, -2.6, ZA), (19.5, -3.0, ZA), (16.0, -3.3, ZA), (11.3, -3.5, ZA)])
        chain("ASA", "AS", [(30.0, 3.2, ZA), (26.5, 2.6, ZA), (20.6, 2.6, ZA), (19.5, 3.0, ZA), (10.5, 3.3, ZA), (7.2, 3.3, ZA)])
        chain("AM", "ASA3", [(18.0, 2.4, ZA), (16.0, 0.0, ZA), (7.6, 0.0, ZA)])     # onto the sun deck's middle (the shuffleboard court)
        node("AH", "AS", 33.3, 2.5, ZA)
        node("AH0", "AH", 33.3, 0.0, ZA)
        chain("HO", "AH0", [(34.4, 0.0, ZA), (38.6, 0.0, ZA)])     # the officers' corridor
        chain("AF", "AH", [(33.4, 3.75, ZA), (34.8, 3.75, ZA)])
        node("BRS", "AF1", 39.0, 3.75, ZBR)                         # the starboard stair to the bridge wing
        chain("WS", "BRS", [(40.1, 3.9, ZBR)])
        chain("WH", "WS0", [(40.1, 1.9, ZBR), (40.1, -1.9, ZBR)])   # the wheelhouse, door to door
        chain("WP", "WH1", [(40.1, -3.9, ZBR)])
        chain("CH", "WH1", [(38.0, -1.6, ZBR)])                     # the chart room
        self.cabin_node = {}
        self.fit = fit

    def way(self, name):
        out = []
        while name:
            out.append(self.pt[name])
            name = self.parent[name]
        return out[::-1]

    def cabin(self, n):
        """The node just inside cabin n's door (made on first use: off its corridor, abreast of the door)."""
        if n in self.cabin_node:
            return self.cabin_node[n]
        x0, x1, y0, y1, z = S.room(cabin_room(n))
        side = 1.0 if y0 + y1 > 0 else -1.0
        door = self.fit.doors.get(n)
        dx = min(max(door.pos[0], x0 + 0.4), x1 - 0.4) if door else (x1 - 0.7 if 11 <= n <= 14 else x0 + 0.7)
        cor = "BL" if 11 <= n <= 14 else "DF" if n >= 15 else "CF"
        line = self.line[cor + "0"]
        a, b = self.pt[line[0]], self.pt[line[-1]]
        lo, hi = min(a[0], b[0]), max(a[0], b[0])
        cx = min(max(dx, lo), hi)
        at = "c%d_at" % n
        # the corridor's line is walked from its first node: abreast of the door hangs off whichever end is nearer the stair
        self.parent[at], self.pt[at] = line[0], (round(cx, 2), a[1], z)
        inn = "c%d_in" % n
        self.parent[inn], self.pt[inn] = at, (round(dx, 2), round(side * 1.55, 2), z)
        self.cabin_node[n] = inn
        return inn

    def anchor(self, item):
        """The node (or line of nodes) a slot at this item hangs off, by its deck and where it is."""
        x, y, _z = item.pos
        dk = item.deck
        room = room_of(x, y, dk)
        m = re.match(r"(?:cabin|lux)(\d+)$", room)
        if m:
            return self.cabin(int(m.group(1)))
        if dk == "D":
            return "QQ0" if x < 18.5 else "DF0" if x > 33.0 else "D0"
        if dk == "C":
            if x < 8.0:
                return "MD0"
            if x < 18.0:
                return "AI0" if x >= 11.0 else "AIa"
            if x < 20.0 and y < 1.5:
                return "SV0"
            if x < 27.0:
                return "GA0" if y < -1.5 else "EGL0" if y < 1.5 else "KC0"
            if x < 32.0:
                return "VP0" if y < -1.5 else "VS0" if y > 1.5 else "C0"
            return "CF0" if x < 46.0 else "FD0"
        if dk == "B":
            if x < 12.0:
                return "SM0"
            if abs(y) >= 3.45 or x > 45.0:
                aft = x < 32.2
                if x > 45.0:
                    return "PPF0" if y < 0.6 else "PSF0"
                return ("PPA0" if aft else "PPF0") if y < 0 else ("PSA0" if aft else "PSF0")
            if x < 18.5:
                return "BL0"
            if x < 27.0:
                return "BC0"
            if x < 33.0:
                return "LI0" if y < -1.5 else "LN" if y > 1.5 else "B0"
            return "BS0" if (x > 42.2 and item.floor > ZB + 0.1) else "SA0"
        if dk == "A":
            if 34.0 <= x <= 43.0 and abs(y) < 3.0:
                return "HO0"
            if x > 33.0:
                return "AF0"
            if x < 19.0 and abs(y) < 2.0:
                return "AM0"
            return "APA0" if y < 0 else "ASA0"
        if y > 3.0:
            return "WS0"
        if y < -3.0:
            return "WP0"
        return "CH0" if (x < 38.5 and y < 0.0) else "WH0"

    def inside(self, item, approach=None):
        """The way in to an item from the gangway: [points] (ship frame), the last one where they stand before it."""
        name = self.anchor(item)
        end = tuple(approach or (item.pos[0], item.pos[1], item.floor))
        line = self.line.get(name)
        if not line:
            return self.way(name) + [end]
        pts = [self.pt[n] for n in line]
        # the nearest point of the line
        best = None
        for k in range(len(pts) - 1):
            a, b = pts[k], pts[k + 1]
            dx, dy = b[0] - a[0], b[1] - a[1]
            L2 = dx * dx + dy * dy
            t = 0.0 if L2 == 0 else max(0.0, min(1.0, ((end[0] - a[0]) * dx + (end[1] - a[1]) * dy) / L2))
            q = (a[0] + dx * t, a[1] + dy * t, a[2] + (b[2] - a[2]) * t)
            d = math.hypot(end[0] - q[0], end[1] - q[1])
            if best is None or d < best[0]:
                best = (d, k, t, q)
        if best is None:
            return self.way(line[0]) + [end]
        _d, k, t, q = best
        out = self.way(line[k])
        if t > 0.999:
            out.append(pts[k + 1])
        elif t > 0.001:
            out.append(tuple(round(v, 2) for v in q))
        return out + [end]


# ------------------------------------------------------------------------------------------------ places
class Places:
    def __init__(self, ship, fit, ways):
        self.ship, self.fit, self.ways = ship, fit, ways
        self.door = ship.world(ways.pt["G"])
        self.first = ship.world(ways.pt["Vg"])
        self.places = []

    def place(self, pid, kind, slots, **more):
        p = OD([("id", pid), ("kind", kind), ("door", list(self.door)), ("yaw", self.ship.wyaw(0)), ("island", ISLAND)])
        p.update(more)
        p["slots"] = slots
        p["roles"] = []
        p["building"] = PLOT
        p["path"] = [list(self.first)]
        self.places.append(p)
        return p

    def slot(self, item, kind, tag=None, hold=True, **more):
        """A slot at an item: the floor under it, its height, the way in from the gangway; the city thing held while
        somebody is at it."""
        w = self.ship.world((item.pos[0], item.pos[1], item.floor))
        way = [self.ship.world(q) for q in self.ways.inside(item)]
        s = OD([("kind", kind)])
        if tag:
            s["tag"] = tag
        s["pos"] = w
        s["yaw"] = self.ship.wyaw(item.yaw)
        s["height"] = round(item.height, 2)
        s["approach"] = list(way[-1])
        if hold and item.thing:
            s["thing"] = "%s/%s" % (PLOT, item.hold)
        s.update(more)
        s["path"] = [list(q) for q in way[2:-1]]       # (after the door and the place's own first point)
        return s


def tables_of(fit):
    """The dining chairs sorted into tables: the hero's (his chair and the three nearest), the captain's (the four
    nearest the thing captains_table), then the rest four by four, nearest together."""
    chairs = {i.id: i for i in fit.dining}

    def near(p, pool, n):
        return sorted(pool, key=lambda c: (math.hypot(chairs[c].pos[0] - p[0], chairs[c].pos[1] - p[1]), int(c.split("_")[-1])))[:n]

    left = sorted(chairs, key=lambda c: int(c.split("_")[-1]))
    hero = [HERO_CHAIR] + near(chairs[HERO_CHAIR].pos, [c for c in left if c != HERO_CHAIR], 3)
    left = [c for c in left if c not in hero]
    cap = near(fit.captains_table.pos, left, 4)
    left = [c for c in left if c not in cap]
    rest = []
    while left:
        t = [left[0]] + near(chairs[left[0]].pos, left[1:], 3)
        rest.append(t)
        left = [c for c in left if c not in t]
    return hero, cap, rest


# ------------------------------------------------------------------------------------------------ activities
def act(aid, kinds, slots, pose, minutes, meets=None, **more):
    a = OD([("id", aid), ("places", list(kinds)), ("slots", list(slots)), ("pose", pose), ("minutes", list(minutes))])
    if meets:
        a["meets"] = OD(meets)
    a.update(more)
    return a


# a duty aboard never sends anybody off to eat or to rest (FWorld::Interrupt): each gives a little of both
DUTY = (("hunger", 0.05), ("tired", 0.03))


def duty_act(aid, kinds, slots, pose, extra=(), **more):
    return act(aid, kinds, slots, pose, [30, 30], DUTY + tuple(extra), free=0, rain="ok", **more)


def staff_act(aid, pose="stand", extra=(), kind="ship_work"):
    return duty_act(aid, [kind], ["staff"], pose, extra, staff=1)


def activities():
    A = []
    # the meals: at their own chair (a duty; one activity a meal: the life finds a duty by its activity)
    for m in ("cruise_dinner", "cruise_breakfast", "cruise_lunch", "cruise_tea", "cruise_gala_dinner", "crew_breakfast", "crew_lunch", "crew_supper"):
        A.append(act(m, ["ship_dining"], ["staff"], "eat", [30, 30], (("hunger", 2.6), ("social", 0.4), ("tired", 0.12)), free=0, staff=1))
    # the programme (duties)
    A += [
        duty_act("cruise_settle", ["home"], ["armchair"], "sit", (("tired", 0.2),)),
        duty_act("cruise_pack", ["home"], ["armchair"], "sit"),
        duty_act("cruise_departure", ["ship_deck"], ["rail"], "look", (("nature", 0.6), ("sun", 0.3))),
        duty_act("cruise_drill", ["ship_deck"], ["muster"], "stand", (("social", 0.4),)),
        duty_act("cruise_exercise", ["ship_deck"], ["exercise"], "stand", (("fun", 0.4),)),
        duty_act("cruise_deckgames", ["ship_deck"], ["games"], "stand", (("fun", 0.9), ("social", 0.5))),
        duty_act("cruise_bridge_tour", ["ship_deck"], ["wing"], "look", (("mind", 0.6),)),
        duty_act("cruise_concert", ["ship_saloon"], ["saloon_seat"], "sit", (("fun", 0.8), ("social", 0.4))),
        duty_act("cruise_concert2", ["ship_saloon"], ["saloon_seat"], "sit", (("fun", 0.8), ("social", 0.4))),
        duty_act("cruise_dance", ["ship_saloon"], ["dance"], "stand", (("fun", 1.0), ("social", 0.6))),
        duty_act("cruise_quiz", ["ship_saloon"], ["saloon_seat"], "sit", (("mind", 0.6), ("fun", 0.5))),
        duty_act("cruise_diplomas", ["ship_saloon"], ["saloon_seat"], "sit", (("fun", 0.5), ("social", 0.5))),
        duty_act("cruise_library", ["ship_library"], ["read_seat"], "read", (("mind", 0.7),)),
        duty_act("cruise_cards", ["ship_cards"], ["staff"], "sit", (("fun", 0.9), ("social", 0.6)), staff=1),
    ]
    for isl in ("brothers", "arch", "birds", "lighthouse", "seals", "home"):
        A.append(duty_act("cruise_isl_" + isl, ["ship_deck"], ["rail"], "look", (("nature", 0.9), ("sun", 0.3))))
    # the crew's posts (duties at P-SHIP, a slot of their role: FWorld::StaffOnShift)
    for aid, pose in (("ship_watch_am", "stand"), ("ship_watch_pm", "stand"), ("ship_watch_night", "stand"), ("ship_watch_day", "stand"),
                      ("ship_helm_am", "stand"), ("ship_helm_eve", "stand"), ("ship_bosun_post", "stand"), ("ship_bosun_gangway", "stand"),
                      ("ship_bosun_drill", "stand"), ("ship_purser_desk", "stand"), ("ship_bar_shift", "stand"), ("ship_steward_am", "stand"),
                      ("ship_steward_day", "stand"), ("ship_galley", "stand"), ("ship_radio", "sit"), ("ship_engine", "stand"),
                      ("ship_rimma_welcome", "stand"), ("ship_rimma_drill", "stand"), ("ship_rimma_exercise", "stand"),
                      ("ship_rimma_games", "stand"), ("ship_rimma_concert", "stand"), ("ship_rimma_concert2", "stand"),
                      ("ship_rimma_quiz", "stand"), ("ship_rimma_diplomas", "stand"),
                      ("band_concert", "sit"), ("band_rehearsal", "sit"), ("band_dinner_set", "sit"), ("band_concert2", "sit"),
                      # (the pianist's are his own, so that the manners can put his hands on the keys: manners.json "doing")
                      ("piano_concert", "sit"), ("piano_rehearsal", "sit"), ("piano_dinner_set", "sit"), ("piano_concert2", "sit")):
        A.append(staff_act(aid, pose))
    A += [
        duty_act("ship_bosun_round", ["ship_round"], ["stand"], "walk", wander=1, round=1, wander_pause=[60, 150]),
        duty_act("ship_cabin_round", ["ship_corridor"], ["stand"], "walk", wander=1, round=1, wander_pause=[90, 200]),
    ]
    # free time aboard (chosen; only the ship's people can: every place of these kinds is on island 4, FWorld::Score)
    A += [
        act("ship_deck_walk", ["ship_deck"], ["rail", "walk_spot"], "walk", [20, 45], (("nature", 0.7), ("sun", 0.4), ("fun", 0.2)),
            costs=OD([("tired", 0.03)]), base=0.1, trait="active", hours=[6, 23], wander=1, wander_m=[4, 70], wander_pause=[40, 120]),
        act("ship_rail_look", ["ship_deck"], ["rail"], "look", [10, 25], (("nature", 0.9), ("sun", 0.5)), base=0.15, trait="nature", hours=[5, 23.5]),
        act("ship_bench", ["ship_deck"], ["deck_bench"], "sit", [20, 50], (("tired", 0.15), ("nature", 0.4), ("social", 0.3), ("sun", 0.3)),
            base=0.05, trait="social", hours=[7, 23], talk=1),
        act("ship_deckchair", ["ship_deck"], ["deckchair"], "sit", [30, 70], (("tired", 0.3), ("sun", 0.7), ("nature", 0.3)), base=0.0,
            trait="sun", hours=[9, 19], daylight=1),
        act("ship_deck_read", ["ship_deck"], ["deckchair", "deck_bench"], "read", [30, 70], (("mind", 0.5), ("sun", 0.4)), base=-0.2,
            trait="studious", hours=[9, 19], daylight=1),
        act("ship_smoke", ["ship_deck"], ["smoke"], "sit", [8, 14], (("fun", 0.5), ("social", 0.3)), base=-2.0, trait="social", hours=[6.5, 25]),
        act("ship_bar", ["ship_saloon"], ["bar_stool"], "sit", [20, 45], (("fun", 0.6), ("social", 0.7), ("hunger", 0.3)), base=-0.2,
            trait="social", hours=[10.2, 24.8], ages=[18, 200]),
        act("ship_saloon_sit", ["ship_saloon"], ["saloon_seat"], "sit", [20, 50], (("social", 0.6), ("fun", 0.3), ("tired", 0.15)), base=0.0,
            trait="social", hours=[9, 23.5], talk=1),
        act("ship_library_read", ["ship_library"], ["read_seat"], "read", [30, 80], (("mind", 0.8), ("tired", 0.05)), base=-0.1,
            trait="studious", hours=[8, 23]),
    ]
    return A


# ------------------------------------------------------------------------------------------------ the day's programme
def D(activity, days, a, b, place=None, role=None, stay_day=None):
    d = OD([("activity", activity)])
    if place:
        d["place"] = place
    if role:
        d["role"] = role
    d["hours"] = [a, b]
    d["days"] = list(days)
    if stay_day is not None:
        d["stay_day"] = stay_day
    return d


DIN = PLOT + "-DINING"
CARDS = PLOT + "-CARDS"


def meals(seat, breakfast=(8.0, 9.0), lunch=(13.0, 14.0), tea=(16.0, 16.5), dinner=(19.5, 20.5), gala=(19.0, 20.5)):
    out = []
    for aid, days, h in (("cruise_dinner", [FRI], dinner), ("cruise_breakfast", [SAT, SUN], breakfast), ("cruise_lunch", [SAT, SUN], lunch),
                         ("cruise_tea", [SAT], tea), ("cruise_gala_dinner", [SAT], gala)):
        if h:
            out.append(D(aid, days, h[0], h[1], DIN, seat))
    return out


def crew_meals(seat, breakfast=None, lunch=None, supper=None, days=(SAT, SUN), supper_days=(FRI, SAT)):
    out = []
    if breakfast:
        out.append(D("crew_breakfast", days, breakfast, breakfast + 0.5, DIN, seat))
    if lunch:
        out.append(D("crew_lunch", days, lunch, lunch + 0.55, DIN, seat))
    if supper:
        out.append(D("crew_supper", supper_days, supper, supper + 0.55, DIN, seat))
    return out


def crew_duties(key, seat):
    """The crew's and the band's fixed hours (the earlier a duty stands in the list, the sooner it wins an hour both claim)."""
    W = PLOT
    if key == "ship_gromov":
        # the captain: the forenoon and the dog watches on the bridge, the captain's table at the passengers' dinners
        return (meals(seat, breakfast=None, lunch=None, tea=None) + crew_meals(seat, 7.25, 12.25)
                + [D("ship_watch_am", VOYAGE, 8.0, 12.1, W, "captain"), D("ship_watch_pm", VOYAGE, 16.0, 18.95, W, "captain")])
    if key == "ship_vais":
        # the chief mate: the night (the anchor watch too) and the afternoon; asleep from half past four till half past eleven
        return (crew_meals(seat, 8.2, 11.3) + [D("ship_watch_night", VOYAGE, 23.75, 8.1, W, "mate"), D("ship_watch_day", VOYAGE, 12.0, 16.1, W, "mate")])
    if key == "ship_vitya":
        return (crew_meals(seat, 7.2, 12.3, 17.6)
                + [D("ship_helm_am", [SAT, SUN], 8.0, 12.1, W, "helmsman"), D("ship_helm_eve", VOYAGE, 18.45, 0.15, W, "helmsman")])
    if key == "ship_taras":
        return (crew_meals(seat, 7.0, 12.1, 17.45, supper_days=(SAT,))
                + [D("ship_bosun_gangway", [FRI], 15.8, 18.1, W, "bosun_gangway"), D("ship_bosun_drill", [FRI], 18.4, 19.05, W, "bosun_muster"),
                   D("ship_bosun_post", [SAT, SUN], 8.0, 12.0, W, "bosun"), D("ship_bosun_round", [SAT, SUN], 13.0, 17.3),
                   D("cruise_cards", VOYAGE, 22.0, 0.5, CARDS, "card_seat_2")])
    if key == "ship_nonna":
        return crew_meals(seat, 7.0) + [D("ship_purser_desk", VOYAGE, 8.0, 23.0, W, "purser")]
    if key == "ship_givi":
        return crew_meals(seat, 9.2) + [D("ship_bar_shift", VOYAGE, 10.0, 1.0, W, "barman")]
    if key == "ship_zoya":
        return [D("ship_steward_am", VOYAGE, 7.0, 9.45, W, "steward"), D("ship_cabin_round", [SAT, SUN], 9.5, 11.0),
                D("ship_steward_day", VOYAGE, 11.05, 21.0, W, "steward")]
    if key == "ship_pasha":
        return [D("ship_galley", VOYAGE, 6.0, 20.0, W, "cook")]
    if key == "ship_lev":
        return crew_meals(seat, 7.2) + [D("ship_radio", VOYAGE, 8.0, 20.0, W, "radio"), D("cruise_cards", VOYAGE, 22.0, 23.5, CARDS, "card_seat_3")]
    if key == "ship_einar":
        return crew_meals(seat, 7.0) + [D("ship_engine", VOYAGE, 8.0, 20.0, W, "engineer")]
    if key == "ship_rimma":
        return (meals(seat) + [
            D("ship_rimma_welcome", [FRI], 15.9, 18.2, W, "entertainer_gangway"), D("ship_rimma_drill", [FRI], 18.4, 19.05, W, "entertainer_muster"),
            D("ship_rimma_concert", [FRI], 20.9, 23.0, W, "entertainer"), D("ship_rimma_exercise", [SAT], 7.2, 7.8, W, "entertainer_deck"),
            D("ship_rimma_games", [SAT], 11.4, 13.0, W, "entertainer_deck"), D("ship_rimma_concert2", [SAT], 20.55, 23.5, W, "entertainer"),
            D("ship_rimma_quiz", [SUN], 9.9, 10.8, W, "entertainer"), D("ship_rimma_diplomas", [SUN], 14.9, 15.6, W, "entertainer")])
    # the band: the two concerts, Saturday's rehearsal, three of them the soft music at the captain's dinner
    role = {"ship_blum": "band_piano", "ship_otar": "band_bass", "ship_kostya": "band_drums", "ship_asta": "band_reed"}[key]
    b = "piano_" if key == "ship_blum" else "band_"
    out = [D("cruise_drill", [FRI], 18.5, 19.0), D(b + "concert", [FRI], 20.8, 23.0, W, role), D(b + "rehearsal", [SAT], 11.0, 12.5, W, role)]
    if key == "ship_kostya":
        out += meals(seat, breakfast=None, tea=None, gala=(19.0, 20.3)) + [D(b + "concert2", [SAT], 20.4, 23.5, W, role)]
    else:
        out += (meals(seat, breakfast=None, tea=None, gala=None) + crew_meals(seat, supper=17.9, supper_days=(SAT,))
                + [D(b + "dinner_set", [SAT], 18.85, 20.5, W, role), D(b + "concert2", [SAT], 20.5, 23.5, W, role)])
    out += crew_meals(seat, 9.6)
    if key == "ship_blum":
        out.append(D("cruise_cards", VOYAGE, 23.65, 1.0, CARDS, "card_seat_4"))
    return out


# who of the passengers goes to what (the rest of the time is their own): a passenger's flags
#   brothers arch exercise birds games tour lighthouse concert quiz seals home | concert till, concert2 till
PAX = {
    "ship_vera":   dict(brothers=1, exercise=1, birds=1, tour=1, lighthouse=1, quiz=1, seals=1, home=1, concert=21.9, concert2=21.9, library=1,
                        cards=("card_seat_4", 22.0, 23.0)),
    "ship_misha":  dict(brothers=1, birds=1, games=1, tour=1, lighthouse=1, quiz=1, seals=1, home=1, concert=22.9, concert2=23.4, dance=1),
    "ship_olya":   dict(brothers=1, birds=1, games=1, tour=1, lighthouse=1, quiz=1, seals=1, home=1, concert=22.9, concert2=23.4, dance=1),
    "ship_albert": dict(lighthouse=1, concert=21.9, concert2=21.9, cards=("card_seat_5", 22.0, 1.0)),
    "ship_haruto": dict(brothers=2, arch=1, birds=2, tour=1, lighthouse=2, seals=2, home=1, concert=None, concert2=21.75, quiz=1),
    "ship_senya":  dict(brothers=1, exercise=1, birds=1, games=1, tour=1, lighthouse=1, quiz=1, seals=1, home=1, concert=21.7, concert2=21.7),
    "ship_faina":  dict(brothers=1, exercise=1, birds=1, games=1, tour=1, lighthouse=1, quiz=1, seals=1, home=1, concert=21.7, concert2=21.7),
    "ship_inga":   dict(brothers=2, arch=1, birds=2, games=1, lighthouse=2, seals=2, home=1, concert=22.5, concert2=23.0, quiz=1),
}
TWIN_PAX = [
    dict(brothers=1, exercise=1, birds=1, lighthouse=1, quiz=1, seals=1, home=1, concert=22.5, concert2=22.9),
    dict(brothers=1, exercise=1, birds=1, tour=1, lighthouse=1, quiz=1, seals=1, home=1, concert=22.5, concert2=22.9),
    dict(birds=1, games=1, lighthouse=1, quiz=1, home=1, concert=22.9, concert2=23.2),
    dict(brothers=1, birds=1, games=1, tour=1, lighthouse=1, seals=1, home=1, concert=22.9, concert2=23.2),
    dict(brothers=1, birds=1, games=1, lighthouse=1, quiz=1, seals=1, home=1, concert=22.9, concert2=23.3),
    dict(exercise=1, birds=1, tour=1, lighthouse=1, quiz=1, seals=1, home=1, concert=22.4, concert2=22.8),
]


def pax_duties(key, seat, f, arrive, leave):
    """A passenger's voyage: aboard and to the cabin, on deck as she sails, the drill, the meals, the islands, the
    programme, the evenings (the first that claims an hour has it)."""
    out = [D("cruise_settle", [FRI], arrive, min(arrive + 0.45, 17.8), stay_day=0), D("cruise_departure", [FRI], 17.85, 18.4),
           D("cruise_drill", [FRI], 18.5, 19.0)]
    if f.get("brothers") == 2:
        out += meals(seat, dinner=(19.5, 20.2))[:1] + [D("cruise_isl_brothers", [FRI], 20.2, 20.8)]
    else:
        out += meals(seat)[:1] + ([D("cruise_isl_brothers", [FRI], 20.5, 20.8)] if f.get("brothers") else [])
    out += meals(seat)[1:]
    if f.get("cards"):
        tag, a, b = f["cards"]
        out.append(D("cruise_cards", VOYAGE, a, b, CARDS, tag))
    if f.get("concert"):
        out.append(D("cruise_concert", [FRI], 21.0, f["concert"]))
    if f.get("arch"):
        out.append(D("cruise_isl_arch", [SAT], 5.25, 5.8))
    if f.get("exercise"):
        out.append(D("cruise_exercise", [SAT], 7.25, 7.75))
    if f.get("library"):
        out.append(D("cruise_library", [SAT, SUN], 9.05, 9.9))
    if f.get("birds"):
        out.append(D("cruise_isl_birds", [SAT], 10.0, 11.5 if f["birds"] == 2 else 11.0))
    if f.get("games"):
        out.append(D("cruise_deckgames", [SAT], 11.5, 12.9))
    if f.get("tour"):
        out.append(D("cruise_bridge_tour", [SAT], 15.0, 15.9))
    if f.get("lighthouse"):
        out.append(D("cruise_isl_lighthouse", [SAT], 16.55, 17.5 if f["lighthouse"] == 2 else 17.2))
    if f.get("dance"):
        out.append(D("cruise_dance", [SAT], 21.0, 22.5))
    if f.get("concert2"):
        # those who dance sit down after the dancing (one duty of an activity a person: the hours never overlap)
        out.append(D("cruise_concert2", [SAT], 22.55 if f.get("dance") else 20.55, f["concert2"]))
    if f.get("quiz"):
        out.append(D("cruise_quiz", [SUN], 10.0, 10.75))
    if f.get("seals"):
        out.append(D("cruise_isl_seals", [SUN], 10.75, 11.25))
    out.append(D("cruise_diplomas", [SUN], 15.0, 15.5))
    if f.get("home"):
        out.append(D("cruise_isl_home", [SUN], 16.5, 17.1))
    out.append(D("cruise_pack", [SUN], 17.5, leave))
    return out


def stay(from_day, arrive, leave, cycle=7):
    return OD([("cycle", cycle), ("from", from_day), ("days", 2), ("arrive", arrive), ("leave", leave), ("when", SEASON)])


def person(p, home, household, seat, duties, st, mesh=None, body=None, town=False, friends=(), twin_of=None, occupation=None):
    o = OD([("key", p["key"])])
    if town:
        o["town"] = 1
    o["mesh"] = mesh or "Town/R_" + p["key"]
    o["body"] = body or p["key"][0].upper() + p["key"][1:]
    if twin_of:
        o["twin_of"] = twin_of
    o["age"] = p["age"]
    o["female"] = p["female"]
    o["occupation"] = occupation or p["occ"]
    o["household"] = household
    o["home"] = home
    o["wake"] = p["wake"]
    o["bed"] = p["bed"]
    o["spread"] = 0.0
    if p.get("speed"):
        o["walk_speed"] = p["speed"]
    o["temper"] = OD(p["temper"])
    o["stay"] = st
    o["duties"] = duties
    o["friends"] = list(friends)
    o["likes"] = OD(p["likes"])
    o["dining_chair"] = seat
    return o


# ------------------------------------------------------------------------------------------------ build
def build(want_doc=False):
    ship = Ship()
    fit = Fittings()
    ways = Ways(fit)
    PL = Places(ship, fit, ways)
    folk = {p["key"]: p for p in load(FOLK)["people"]}
    town_names = [load(TOWN_STRINGS % lang) for lang in LANGS]

    # ---- who sits where
    hero_t, cap_t, rest_t = tables_of(fit)
    chair = {}
    for k, c in zip(HERO_TABLE, hero_t[1:]):
        chair[k] = c
    for k, c in zip(CAPTAINS_TABLE, cap_t):
        chair[k] = c
    free_chairs = [HERO_CHAIR, cap_t[3]]
    groups = [["ship_misha", "ship_olya", "ship_albert"], ["twin0", "twin1", "twin2", "twin3"], ["twin4", "twin5", "ship_rimma"],
              ["ship_blum", "ship_otar", "ship_kostya", "ship_asta"], ["ship_vais", "ship_lev", "ship_einar", "ship_nonna"],
              ["ship_taras", "ship_vitya", "ship_givi"], ["ship_zoya", "ship_pasha"]]
    if len(rest_t) < len(groups):
        sys.exit("cruise_people: %d dining tables besides the hero's and the captain's, %d needed" % (len(rest_t), len(groups)))
    for g, t in zip(groups, rest_t):
        for k, c in zip(g, t):
            chair[k] = c
        free_chairs += t[len(g):]
    for t in rest_t[len(groups):]:
        free_chairs += t

    def seat_tag(c):
        return "seat_" + c.split("_")[-1]

    # ---- the twins
    twins = []          # (set, n, original, cabin)
    for si, group in enumerate(TWIN_SETS):
        for n, (name, cabin) in enumerate(group):
            o = folk.get("t_" + name)
            if o is None:
                sys.exit("cruise_people: t_%s is not in townsfolk.json" % name)
            for d in o.get("duties", []):
                days = d.get("days", list(range(7)))
                if "stay" in o or SAT in days or SUN in days or (FRI in days and d["hours"][1] > 16.2):
                    sys.exit("cruise_people: t_%s has a duty on the cruise's days (%s): choose somebody else" % (name, d["activity"]))
            twins.append((si, n, o, cabin))

    # ---- homes: the passengers' cabins, the crew's berths
    used_cabins = sorted(set(CABINS.values()) | {c for _s, _n, _o, c in twins})
    for n in used_cabins:
        if n in HERO_CABINS:
            sys.exit("cruise_people: cabin %d is the hero's" % n)
        slots = [PL.slot(b, "bed") for b in fit.berths[n]]
        low = fit.berths[n][0]
        # (the edge of the lower berth to sit on: no chair's thing is held, the desk stays the hero's to look at)
        slots.append(PL.slot(Item(low.id + "_edge", "seat", (low.pos[0], low.pos[1], low.floor + 0.45), low.yaw + 90.0, 0.45), "armchair", hold=False))
        PL.place("%s/c-%02d" % (PLOT, n), "flat", slots, private=1)
    crew_all = CREW + BAND
    crew_home = {}
    for k, p in enumerate(crew_all):
        b = fit.crew_berths[k]
        pid = "%s/k-%02d" % (PLOT, k + 1)
        PL.place(pid, "flat", [PL.slot(b, "bed"),
                               PL.slot(Item(b.id + "_edge", "seat", (b.pos[0], b.pos[1], b.floor + 0.45), b.yaw + 90.0, 0.45), "armchair", hold=False)],
                 private=1)
        crew_home[p["key"]] = pid

    # ---- the posts (P-SHIP itself: FWorld::StaffOnShift(P-SHIP, role) is what the counters ask)
    st = fit.staff
    muster_lead = fit.muster[0]
    g = ways.pt["Vg"]
    sun = Item("lead_deck", "stand", (14.6, 0.0, ZA), 180.0)
    posts = [("captain", st["staff_captain"]), ("mate", st["staff_mate"]), ("helmsman", st["staff_helmsman"]), ("purser", st["staff_purser"]),
             ("barman", st["staff_barman"]), ("steward", st["staff_steward"]), ("cook", st["staff_cook"]), ("bosun", st["staff_bosun"]),
             ("radio", st["staff_radio"]), ("engineer", st["staff_engineer"]), ("entertainer", st["staff_rimma"]),
             ("band_piano", st["band_piano"]), ("band_bass", st["band_bass"]), ("band_drums", st["band_drums"]), ("band_reed", st["band_reed"]),
             ("bosun_gangway", Item("post_gangway", "stand", (g[0] - 0.9, g[1] + ways.sg * 1.2, ZC), 90.0 if ways.sg > 0 else 270.0)),
             ("entertainer_gangway", Item("post_welcome", "stand", (g[0] + 0.3, g[1] - ways.sg * 0.9, ZC), 90.0 if ways.sg > 0 else 270.0)),
             ("bosun_muster", Item("post_muster", "stand", (muster_lead.pos[0] - 1.6, muster_lead.pos[1] * 0.55, ZA), 0.0)),
             ("entertainer_muster", Item("post_muster2", "stand", (muster_lead.pos[0] - 1.6, -muster_lead.pos[1] * 0.55, ZA), 0.0)),
             ("entertainer_deck", fit.rimma_deck or sun)]
    work = PL.place(PLOT, "ship_work", [PL.slot(i, "staff", tag, hold=False) for tag, i in posts])
    work["roles"] = [t for t, _i in posts]

    # ---- the dining saloon: a place a chair, by its number
    din_slots = []
    for c in fit.dining:
        if c.id in (HERO_CHAIR, cap_t[3]):
            continue        # the hero's own chair and the free one at the captain's table: nobody's place
        din_slots.append(PL.slot(c, "staff", seat_tag(c.id)))
    PL.place(DIN, "ship_dining", din_slots)

    # ---- the card table (chair 1 is the hero's)
    PL.place(CARDS, "ship_cards", [PL.slot(c, "staff", "card_seat_%s" % c.id.split("_")[-1]) for c in fit.cards if c.id not in HERO_FREE])

    # ---- the music saloon, the library corner
    sal = [PL.slot(c, "saloon_seat") for c in fit.saloon if c.id not in HERO_FREE]
    sal += [PL.slot(c, "bar_stool") for c in fit.stools if c.id not in HERO_FREE]
    sal += [PL.slot(c, "bar_stand") for c in fit.barstand]
    sal += [PL.slot(c, "dance") for c in fit.dance]
    PL.place(PLOT + "-SALOON", "ship_saloon", sal)
    PL.place(PLOT + "-LIBRARY", "ship_library", [PL.slot(c, "read_seat") for c in fit.library if c.id not in HERO_FREE])

    # ---- the open decks (under the sky: "open")
    deck = [PL.slot(c, "rail") for c in fit.rails]
    deck += [PL.slot(c, "deck_bench") for c in fit.prom + fit.funnel if c.hold not in HERO_FREE]
    deck += [PL.slot(c, "smoke") for c in fit.smoking if c.hold not in HERO_FREE]
    deck += [PL.slot(c, "deckchair") for c in fit.deckchairs if c.id not in HERO_FREE]
    deck += [PL.slot(c, "muster") for c in fit.muster]
    # (everybody musters: a second rank, on the other side of the funnel from the export's spots, for the rest)
    deck += [PL.slot(Item("muster_b%d" % k, "stand", (c.pos[0], -c.pos[1], c.pos[2]), 180.0 - c.yaw), "muster") for k, c in enumerate(fit.muster)
             if not any(abs(o.pos[0] - c.pos[0]) < 0.5 and abs(o.pos[1] + c.pos[1]) < 0.5 for o in fit.muster)]
    # where the morning exercise stands (the sun deck, facing the entertainer), the deck games' players (round the
    # shuffleboard court), the bridge tour (the wings and the wheelhouse's back), a walk's turning points
    deck += [PL.slot(c, "exercise") for c in fit.exercise]
    deck += [PL.slot(Item("exercise_x%d" % k, "stand", (12.4 + (k % 3) * 1.3, (-1.7, 1.7)[k // 3 % 2], ZA), 180.0), "exercise")
             for k in range(max(0, 8 - len(fit.exercise)))]
    deck += [PL.slot(Item("games_%d" % k, "stand", (8.0 + (k % 6) * 1.5, (-1.7 if k < 6 else 1.7), ZA), 90.0 if k < 6 else 270.0), "games")
             for k in range(12)]
    deck += [PL.slot(c, "wing") for c in fit.wings]
    deck += [PL.slot(Item("wing_x%d" % k, "stand", (39.45 + (k % 2) * 0.55, (3.55 if k < 2 else -3.55), ZBR), 90.0 if k < 2 else 270.0), "wing")
             for k in range(4)]
    for k, (x, y, z) in enumerate(((46.9, 0.0, ZB), (12.6, 4.2, ZB), (12.6, -4.2, ZB), (25.0, 4.2, ZB), (25.0, -4.2, ZB), (38.0, 4.2, ZB),
                                   (38.0, -4.2, ZB), (18.0, 3.1, ZA), (18.0, -3.1, ZA), (28.0, 2.7, ZA), (28.0, -2.7, ZA))):
        deck.append(PL.slot(Item("walk_%d" % k, "stand", (x, y, z), 90.0 if y > 0 else 270.0), "walk_spot", capacity=3))
    PL.place(PLOT + "-DECK", "ship_deck", deck, open=1)

    # ---- the bosun's round of the decks, the stewardess's of the cabin corridors (in their order)
    rnd = [(52.0, 2.2, S.foredeck_z(52.0)), (46.9, 0.6, ZB), (38.0, 4.2, ZB), (20.0, 4.2, ZB), (6.0, -0.6, ZB), (20.0, -4.2, ZB),
           (12.0, 3.3, ZA), (24.0, 2.6, ZA), (28.0, -2.7, ZA), (5.0, -1.2, ZC)]
    PL.place(PLOT + "-ROUND", "ship_round", [PL.slot(Item("round_%d" % k, "stand", q, 0.0), "stand") for k, q in enumerate(rnd)], open=1)
    cor = [(34.0, 0.0, ZC), (39.0, 0.0, ZC), (44.6, 0.0, ZC), (41.0, 0.0, ZD), (35.0, 0.0, ZD), (16.8, 0.0, ZB), (13.6, 0.0, ZB), (29.0, 2.3, ZC)]
    PL.place(PLOT + "-CORRIDORS", "ship_corridor", [PL.slot(Item("cor_%d" % k, "stand", q, 0.0), "stand") for k, q in enumerate(cor)])

    # ---- the people
    people, names = [], OD()
    everybody = []      # (key, kind, record) for the doc

    def name_of(p, lang_i):
        role = ROLES[p["role"]]
        return p["name"][lang_i], (role[0][p["female"]], role[1], role[2])[lang_i]

    gone_owner = []
    for p in crew_all:
        key = p["key"]
        rec = person(p, crew_home[key], key, chair[key], crew_duties(key, seat_tag(chair[key])), stay(FRI, 14.0, 20.0))
        people.append(rec)
        gone_owner.append(key)
        everybody.append((key, "band" if p in BAND else "crew", rec, p))
    households = {"ship_faina": "ship_faina", "ship_senya": "ship_faina", "ship_misha": "ship_misha", "ship_olya": "ship_misha"}
    friends = {"ship_faina": ["ship_senya", "ship_vera"], "ship_senya": ["ship_faina"], "ship_misha": ["ship_olya"], "ship_olya": ["ship_misha"],
               "ship_vera": ["ship_faina", "ship_haruto"], "ship_haruto": ["ship_vera", "ship_inga"], "ship_inga": ["ship_haruto"],
               "ship_albert": []}
    for k, p in enumerate(TOURISTS):
        key = p["key"]
        arrive, leave = round(16.2 + 0.15 * k, 2), round(18.3 + 0.12 * k, 2)
        if key == "ship_senya":
            arrive, leave = round(16.2 + 0.15 * (k + 1), 2), round(18.3 + 0.12 * (k + 1), 2)     # with his grandmother
        rec = person(p, "%s/c-%02d" % (PLOT, CABINS[key]), households.get(key, key), chair[key],
                     pax_duties(key, seat_tag(chair[key]), PAX[key], arrive, leave), stay(FRI, arrive, leave), friends=friends[key])
        people.append(rec)
        gone_owner.append(key)
        everybody.append((key, "tourist", rec, p))
    patches, pier_owner = [], []
    for si, n, o, cabin in twins:
        name = o["key"][2:]
        key = "c_" + name
        ck = chair["twin%d" % n]
        arrive, leave = round(16.75 + 0.1 * n, 2), round(18.2 + 0.15 * n, 2)
        tw = dict(key=key, age=o["age"], female=o["female"], occ="cruise_passenger", wake=min(max(o["wake"], 6.0), 7.0),
                  bed=23.25 if o["age"] > 60 else 23.6, temper=o.get("temper") or T(0.5, 0.5, 0.5, 0.5, 0.5, 0.5),
                  likes=OD(LOOK, **({"ship_smoke": 2.4} if name in SMOKER_TWINS else {}), **({"ship_deckchair": 0.8} if o["age"] > 60 else {"ship_bar": 0.6})),
                  speed=o.get("walk_speed"), role="passenger", name=tuple(t["npc.%s.name" % o["key"]] for t in town_names))
        mate = [("c_" + q["key"][2:]) for s2, _n2, q, c2 in twins if s2 == si and c2 == cabin and q is not o]
        rec = person(tw, "%s/c-%02d" % (PLOT, cabin), "cruise_" + o["household"] if mate else "cruise_" + name, ck,
                     pax_duties(key, seat_tag(ck), TWIN_PAX[n], arrive, leave), stay(FRI + 7 * si, arrive, leave, 14),
                     mesh=o["mesh"], body=o.get("body", o["mesh"]), town=True, friends=mate, twin_of=o["key"])
        people.append(rec)
        gone_owner.append(key)
        everybody.append((key, "twin %s" % "AB"[si], rec, tw))
        # the town's own record: gone from the pier head on Friday afternoon (a quarter of an hour before the twin comes
        # aboard: the launch) till Sunday evening (a quarter of an hour after the twin goes ashore); here out of season
        patches.append(OD([("key", o["key"]), ("patch", 1),
                           ("stay", OD([("cycle", 14), ("from", SUN + 7 * si), ("days", 12), ("arrive", round(leave + 0.25, 2)),
                                        ("leave", round(arrive - 0.25, 2)), ("when", SEASON), ("off", "here")])),
                           ("duties", [D("cruise_board", list(range(7)), round(arrive - 0.95, 2), round(arrive - 0.25, 2), "S-PIER-CRUISE", "cruise_passenger",
                                         stay_day=12)])]))
        pier_owner.append(o["key"])
    for key, _kind, rec, p in everybody:
        for i, lang in enumerate(LANGS):
            nm, role = name_of(p, i)
            names.setdefault(lang, OD())["npc.%s.name" % key] = nm
            names[lang]["npc.%s.role" % key] = role

    # ---- where they are while gone: at the gangway (aboard), at the pier head (the town's records)
    gw = fit.gangway.pos
    shore = [OD([("kind", "stand"), ("pos", ship.world((gw[0] - 1.2 + 0.08 * (k % 30), ways.sg * 4.1, ZC))), ("yaw", ship.wyaw(0)), ("height", 0.0),
                 ("owner", key)]) for k, key in enumerate(gone_owner)]
    PL.places.append(OD([("id", "S-SHIP-SHORE"), ("kind", "guest_platform"), ("pos", list(PL.door)), ("yaw", ship.wyaw(0)), ("island", ISLAND), ("open", 1),
                         ("slots", shore), ("roles", [])]))
    head = next(p for p in load(TOWN_PLACES)["places"] if p["id"] == "S-PIER-HEAD")
    hx, hy, hz = head["pos"]
    pier = [OD([("kind", "stand"), ("pos", [round(hx - 0.4 - 1.1 * (k % 2), 2), round(hy - 5.0 + 0.9 * k, 2), hz]), ("yaw", head.get("yaw", 0.0)),
                ("height", 0.0), ("owner", key)]) for k, key in enumerate(pier_owner)]
    PL.places.append(OD([("id", "S-PIER-CRUISE"), ("kind", "guest_platform"), ("pos", [round(hx - 0.9, 2), hy, hz]), ("yaw", head.get("yaw", 0.0)),
                         ("island", 0), ("open", 1), ("slots", pier), ("roles", [])]))
    acts = activities()
    # (waiting for the launch at the pier head: the town's own record, only in season)
    acts.append(act("cruise_board", ["guest_platform"], ["stand"], "stand", [30, 30], (("social", 0.2),), free=0, rain="ok", staff=1, when=SEASON))

    out = {}
    doc = ("The motor ship «Полдень» as the people's life sees her (tools/ship/cruise_people.py, docs/ship.md): island 4, no way to or "
           "from the town. P-SHIP: the crew's posts (FWorld::StaffOnShift); P-SHIP-DINING: a place a chair; cabins: private flats; "
           "S-SHIP-SHORE: where the ship's people are while gone; S-PIER-CRUISE (island 0): where the cruising townsfolk wait for the launch.")
    out[OUT_PLACES] = dump(OD([("_doc", doc), ("standin", sorted(set(fit.standin))), ("planned", 1 if ship.planned else 0),
                               ("free", OD([("dining", free_chairs), ("other", sorted(HERO_FREE))])), ("places", PL.places)]))
    out[OUT_ACTS] = "{\n  \"_doc\": \"What the motor ship's people do (tools/ship/cruise_people.py): appended to activities.json's by FWorld::Load.\",\n  \"activities\": [\n" \
        + ",\n".join("    " + json.dumps(a, ensure_ascii=False) for a in acts) + "\n  ]\n}\n"
    out[OUT_PEOPLE] = "{\n  \"_doc\": \"The motor ship's crew, band and tourists (story people: talked to with E, body = their dialogue section), the cruising townsfolk's twins (town: 1, twin_of) and, with patch: 1, the stay and the pier duty added to those townsfolk's own records (tools/ship/cruise_people.py).\",\n  \"people\": [\n" \
        + ",\n".join("    " + json.dumps(p, ensure_ascii=False) for p in people + patches) + "\n  ]\n}\n"
    # the names: a table of their own (Localization/shippeople: LyudText merges every folder; the ship's own table is
    # written whole by tools/ship/cruise.py)
    for lang in LANGS:
        path = STRINGS % lang
        sd = load(path) if os.path.exists(path) else OD()
        for k in [k for k in sd if re.match(r"npc\.(ship_|c_)", k)]:
            del sd[k]
        sd.update(names[lang])
        out[path] = dump(sd)
    out[MANNERS] = manners_text()
    if want_doc:
        out["__doc__"] = people_doc(everybody, chair, free_chairs, cap_t, hero_t, fit, ship, twins)
    return out


# ------------------------------------------------------------------------------------------------ manners
MANNER_ROWS = [
    '{"id": "piano", "poses": ["sit"], "doing": ["piano_concert", "piano_concert2", "piano_rehearsal", "piano_dinner_set"], "force": 1}',
    '{"id": "smoke", "poses": ["sit", "stand", "talk"], "doing": ["ship_smoke"], "force": 1, "prop": "cigarette"}',
    '{"id": "dominoes", "poses": ["sit"], "doing": ["cruise_cards"], "seconds": [2.4, 3.2], "weight": 5.0, "cooldown": 14}',
    '{"id": "binoculars", "poses": ["look"], "doing": ["cruise_isl_brothers", "cruise_isl_arch", "cruise_isl_birds", "cruise_isl_lighthouse", "cruise_isl_seals", "cruise_isl_home", "ship_rail_look"], "seconds": [10, 30], "weight": 2.5, "cooldown": 40, "prop": "binoculars"}',
    '{"id": "photo", "poses": ["look", "stand"], "doing": ["cruise_isl_brothers", "cruise_isl_arch", "cruise_isl_birds", "cruise_isl_lighthouse", "cruise_isl_seals", "cruise_departure"], "sun_up": 1, "seconds": [6, 12], "weight": 2.0, "cooldown": 60, "prop": "camera"}',
    '{"id": "point", "poses": ["look", "stand"], "doing": ["cruise_isl_brothers", "cruise_isl_arch", "cruise_isl_birds", "cruise_isl_lighthouse", "cruise_isl_seals", "cruise_isl_home"], "seconds": [2, 4], "weight": 1.5, "cooldown": 50}',
    '{"id": "shade_eyes", "poses": ["look", "stand"], "doing": ["cruise_isl_birds", "cruise_isl_lighthouse", "cruise_isl_seals", "cruise_isl_home", "cruise_departure", "ship_rail_look", "ship_deck_walk"], "sun_up": 1, "seconds": [4, 10], "weight": 1.2, "cooldown": 70}',
    '{"id": "wave", "poses": ["look", "stand"], "doing": ["cruise_departure", "cruise_isl_home"], "seconds": [3, 6], "weight": 3.0, "cooldown": 40}',
    '{"id": "sway", "poses": ["stand"], "doing": ["cruise_dance"], "seconds": [20, 60], "weight": 9.0, "cooldown": 4}',
    '{"id": "sway", "poses": ["sit"], "doing": ["band_concert", "band_concert2", "band_rehearsal", "band_dinner_set"], "seconds": [15, 45], "weight": 6.0, "cooldown": 6}',
    '{"id": "clap", "poses": ["sit"], "doing": ["cruise_concert", "cruise_concert2", "cruise_diplomas"], "seconds": [4, 8], "weight": 1.2, "cooldown": 180}',
    '{"id": "clap", "poses": ["stand"], "doing": ["cruise_deckgames"], "seconds": [3, 6], "weight": 1.5, "cooldown": 90}',
    '{"id": "stretch", "poses": ["stand"], "doing": ["cruise_exercise", "ship_rimma_exercise"], "seconds": [4, 6], "weight": 9.0, "cooldown": 8}',
    '{"id": "hands_hips", "poses": ["stand"], "doing": ["cruise_exercise", "ship_rimma_exercise", "ship_bosun_post", "ship_bosun_drill"], "seconds": [6, 12], "weight": 3.0, "cooldown": 20}',
    '{"id": "drink", "poses": ["sit"], "doing": ["ship_bar", "cruise_tea"], "seconds": [40, 110], "weight": 5.0, "cooldown": 60, "prop": "cup"}',
    '{"id": "newspaper", "poses": ["sit"], "doing": ["ship_bench", "ship_deckchair", "ship_saloon_sit"], "habit": "reader", "seconds": [120, 400], "weight": 2.0, "cooldown": 600, "prop": "newspaper"}',
    '{"id": "doze", "poses": ["sit"], "doing": ["ship_deckchair"], "hours": [12, 17.5], "seconds": [60, 240], "weight": 2.5, "cooldown": 600}',
    '{"id": "hands_back", "poses": ["stand", "look", "walk"], "doing": ["ship_watch_am", "ship_watch_pm", "ship_watch_night", "ship_watch_day", "ship_bosun_round"], "seconds": [20, 80], "weight": 3.0, "cooldown": 30}',
    '{"id": "check_watch", "poses": ["stand", "look"], "doing": ["ship_watch_am", "ship_watch_pm", "ship_watch_night", "ship_watch_day", "ship_bosun_gangway", "ship_rimma_welcome"], "seconds": [3, 4], "weight": 1.0, "cooldown": 120}',
    '{"id": "carry_bag", "poses": ["walk", "stand", "look"], "doing": ["cruise_settle", "cruise_board"], "force": 1, "prop": "suitcase"}',
    '{"id": "torch", "poses": ["stand", "walk", "look"], "doing": ["ship_bosun_round"], "sun_up": 0, "force": 1, "prop": "torch"}',
]
MANNER_MARK = ', "from": "cruise_people"}'


def manners_text():
    """manners.json with the ship's rows (marked "from": "cruise_people") before the reactions at its end."""
    lines = [l for l in open(MANNERS, encoding="utf-8").read().split("\n") if MANNER_MARK not in l]
    at = next(i for i, l in enumerate(lines) if '"id": "wave", "react": 1' in l)
    while at > 0 and not lines[at - 1].strip():
        at -= 1
    rows = ["    " + r[:-1] + MANNER_MARK + "," for r in MANNER_ROWS]
    lines[at:at] = rows
    text = "\n".join(lines)
    json.loads(text)
    return text


# ------------------------------------------------------------------------------------------------ the people's table
def hm(h):
    h = h % 24.0
    return "%02d:%02d" % (int(h), int(round((h - int(h)) * 60)) % 60)


WHERE = {
    "ship_watch_am": "bridge (wheelhouse)", "ship_watch_pm": "bridge (wheelhouse)", "ship_watch_night": "bridge (wheelhouse)",
    "ship_watch_day": "bridge (wheelhouse)", "ship_helm_am": "at the wheel", "ship_helm_eve": "at the wheel",
    "ship_bosun_post": "foredeck (staff_bosun)", "ship_bosun_gangway": "at the gangway", "ship_bosun_drill": "muster station (leads the drill)",
    "ship_bosun_round": "round of the decks", "ship_purser_desk": "purser's desk", "ship_bar_shift": "behind the bar",
    "ship_steward_am": "dining hatch", "ship_steward_day": "dining hatch", "ship_cabin_round": "cabin corridors", "ship_galley": "galley",
    "ship_radio": "radio desk", "ship_engine": "engine gallery", "ship_rimma_welcome": "vestibule, meeting passengers",
    "ship_rimma_drill": "muster station", "ship_rimma_exercise": "sun deck (leads the exercise)", "ship_rimma_games": "sun deck (deck games)",
    "ship_rimma_concert": "saloon (staff_rimma)", "ship_rimma_concert2": "saloon (staff_rimma)", "ship_rimma_quiz": "saloon (leads the quiz)",
    "ship_rimma_diplomas": "saloon (diplomas)", "band_concert": "bandstand", "band_rehearsal": "bandstand (rehearsal)",
    "band_dinner_set": "bandstand (dinner music)", "band_concert2": "bandstand", "piano_concert": "bandstand (piano)",
    "piano_rehearsal": "bandstand (rehearsal)", "piano_dinner_set": "bandstand (dinner music)", "piano_concert2": "bandstand (piano)", "cruise_settle": "cabin (just aboard)", "cruise_pack": "cabin (packing)",
    "cruise_departure": "at a rail", "cruise_drill": "muster station", "cruise_exercise": "sun deck (exercise)",
    "cruise_deckgames": "sun deck (deck games)", "cruise_bridge_tour": "bridge wings (tour)", "cruise_concert": "saloon seat",
    "cruise_concert2": "saloon seat", "cruise_dance": "dance floor", "cruise_quiz": "saloon seat (quiz)", "cruise_diplomas": "saloon seat (diplomas)",
    "cruise_library": "library chair", "cruise_cards": "card table", "cruise_dinner": "dining chair", "cruise_breakfast": "dining chair",
    "cruise_lunch": "dining chair", "cruise_tea": "dining chair", "cruise_gala_dinner": "dining chair", "crew_breakfast": "dining chair",
    "crew_lunch": "dining chair", "crew_supper": "dining chair",
}


def day_line(rec, day):
    """One day's fixed hours of a person, in the order of the clock."""
    segs = []
    wake, bed = rec["wake"], rec["bed"]
    for d in rec["duties"]:
        if day not in d["days"]:
            continue
        a, b = d["hours"]
        where = WHERE.get(d["activity"]) or ("at a rail (%s)" % d["activity"].replace("cruise_isl_", "") if d["activity"].startswith("cruise_isl_") else d["activity"])
        if b < a:
            segs.append((a, 24.0, where))
            segs.append((0.0, b, where))        # (the small hours of the same weekday: the night before's)
        else:
            segs.append((a, b, where))
    segs.sort()
    return "; ".join("%s-%s %s" % (hm(a), hm(b), w) for a, b, w in segs) + " | sleeps %s-%s" % (hm(bed), hm(wake))


def people_doc(everybody, chair, free_chairs, cap_t, hero_t, fit, ship, twins):
    L = []
    L.append("# W10: the people aboard «Полдень» (generated by `python3 tools/ship/cruise_people.py --doc <this file>`; the data is")
    L.append("# `Content/PeopleData/ship_people.json`, `ship_places.json`, `ship_activities.json`)")
    L.append("")
    L.append("## Keys, bodies, talk")
    L.append("The 23 named crew / band / tourists are story people (NOT `town`): talked to with E, dialogue section = `body` = `Ship_<id>`")
    L.append("(`[Ship_gromov]`, ...). Their `mesh` is the generated resident `Town/R_ship_<id>` (blender/residents): until the Mac builds and")
    L.append("imports those models they LIVE UNSEEN (the life runs, the counters are staffed, but there is no body to press E on).")
    L.append("The twins `c_<name>` are townsfolk (`town: 1`): the pooled body of their town record, barks, no E.")
    L.append("Names: `npc.<key>.name` / `.role` in `Content/Localization/shippeople/strings_<lang>.json`.")
    L.append("")
    L.append("| key | body = section | ru | en | ja | age | what | cabin / berth | dining chair |")
    L.append("|---|---|---|---|---|---|---|---|---|")
    for key, kind, rec, p in everybody:
        L.append("| %s | %s | %s | %s | %s | %d | %s | %s | %s |" % (key, rec["body"] if not rec.get("town") else "(townsperson: no E)", p["name"][0], p["name"][1],
                                                                 p["name"][2], rec["age"], kind + ": " + ROLES[p["role"]][1], rec["home"].split("/")[-1],
                                                                 rec["dining_chair"]))
    L.append("")
    L.append("## Left free for the hero (no NPC ever takes them)")
    L.append("- Dining: `%s` (his table: %s on `%s`). The captain's table is `%s`: Громов, Харуто Мори, Инга" % (
        HERO_CHAIR, ", ".join(HERO_TABLE), "`, `".join(hero_t[1:]), "`, `".join(cap_t)))
    L.append("  and `%s` left free (an invitation). Free chairs in all: %s." % (cap_t[3], ", ".join("`%s`" % c for c in free_chairs)))
    L.append("- Cards: `card_chair_1`. Bodies at the table: Альберт 22:00-01:00 (chair 5), Тарас 22:00-00:30 (chair 2), Лев 22:00-23:30 (chair 3),")
    L.append("  Вера 22:00-23:00 then Блюм 23:40-01:00 (chair 4). Гиви stays behind the bar till 01:00 (the counter), so he is not a body at the table.")
    L.append("- Cabins 7, 11, 15 (no NPC). Also never used by NPCs: %s." % ", ".join("`%s`" % c for c in sorted(HERO_FREE) if c not in (HERO_CHAIR, "card_chair_1")))
    L.append("")
    L.append("## Twins")
    for si in (0, 1):
        L.append("- Set %s (weeks with FWorld::Week %s): %s." % ("AB"[si], ("even", "odd")[si], ", ".join(
            "%s (%s, %d, cabin %d)" % (o["key"], o.get("occupation", ""), o["age"], c) for s, _n, o, c in twins if s == si)))
    L.append("  Town record: waits at the pier head (`S-PIER-CRUISE`) from ~15:50, gone 16:30-17:00 Friday .. back on the pier 18:30-19:15 Sunday.")
    L.append("")
    L.append("## StaffOnShift(\"P-SHIP\", role)")
    L.append("`purser` Нонна 08:00-23:00 (Fri from ~14:10); `barman` Гиви 10:00-01:00 (Fri from ~14:10); `steward` Зоя 07:00-21:00 except")
    L.append("09:30-11:00 Sat/Sun (cabins); `cook` тётя Паша 06:00-20:00; `radio` Лев 08:00-20:00; also `captain`, `mate`, `helmsman` (bridge),")
    L.append("`bosun` (foredeck, Sat/Sun 08-12), `engineer` 08-20, `entertainer` (concerts, quiz, diplomas), `band_piano|bass|drums|reed`.")
    L.append("Out of the voyage (Sun 20:00 - Fri 14:00) and out of season nobody is aboard.")
    L.append("")
    L.append("## Hour by hour (fixed hours; between them they choose: decks, rails, benches, deck chairs, bar, saloon, library, smoking deck, cabin)")
    for day, title in ((FRI, "Friday (crew aboard 14:00, passengers 16:12-17:15, sails 18:00)"), (SAT, "Saturday"), (SUN, "Sunday (anchors 18:00, passengers ashore 18:12-19:10, crew 20:00)")):
        L.append("")
        L.append("### %s" % title)
        for key, kind, rec, p in everybody:
            if kind == "twin B":
                continue
            L.append("- **%s** (%s): %s" % (p["name"][0], key, day_line(rec, day)))
    L.append("")
    L.append("Twins of set B keep the same programme as set A's (their own chairs' numbers and cabins above).")
    L.append("")
    L.append("## Data notes")
    L.append("- Stand-in used for (the export lacks them, numbers from _shipframe.ROOMS / R1): %s." % (", ".join(sorted(set(fit.standin))) or "nothing"))
    L.append("- Plot P-SHIP in city_plan.json: %s." % ("yes" if ship.planned else "NOT YET (the stand-in cruise stage map (26000, 4000) is used)"))
    L.append("- Season: the people's-life condition `cruise_season` (the game: `UPeopleLifeSubsystem::SetCondition`); off = nobody aboard.")
    return "\n".join(L) + "\n"


def main():
    doc_path = sys.argv[sys.argv.index("--doc") + 1] if "--doc" in sys.argv else None
    out = build(doc_path is not None)
    doc = out.pop("__doc__", None)
    stale = [p for p, t in out.items() if not os.path.exists(p) or open(p, encoding="utf-8").read() != t]
    if "--check" in sys.argv:
        for p in stale:
            print("cruise_people: %s is not up to date (python3 tools/ship/cruise_people.py)" % os.path.relpath(p, ROOT))
        sys.exit(1 if stale else 0)
    for p in stale:
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "w", encoding="utf-8") as f:
            f.write(out[p])
        print("wrote", os.path.relpath(p, ROOT))
    if doc_path:
        with open(doc_path, "w", encoding="utf-8") as f:
            f.write(doc)
    places = json.loads(out[OUT_PLACES])
    people = json.loads(out[OUT_PEOPLE])["people"]
    print("cruise people: %d aboard (%d crew, %d band, %d tourists, %d twins in two sets), %d places, %d slots%s"
          % (len([p for p in people if not p.get("patch")]), len(CREW), len(BAND), len(TOURISTS), sum(len(s) for s in TWIN_SETS), len(places["places"]),
             sum(len(p["slots"]) for p in places["places"]), "" if places["planned"] else "; P-SHIP is not in city_plan.json yet (stand-in stage)"))
    if places["standin"]:
        print("cruise people: stand-in positions for %d things the export lacks: %s" % (len(places["standin"]), ", ".join(places["standin"][:12])
                                                                                       + (" ..." if len(places["standin"]) > 12 else "")))


if __name__ == "__main__":
    main()
