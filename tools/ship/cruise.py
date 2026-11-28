#!/usr/bin/env python3
"""The motor ship «Полдень»'s cruise: the source of its data (docs/ship.md).

    python3 tools/ship/cruise.py            writes the files below
    python3 tools/ship/cruise.py --check    fails (exit 1) when a file is stale or the data is wrong

Writes
    unreal/Lyudmilovsk/Content/Cruise/cruise.json                 season, voyage, islands, route, events, tickets, stage, points
    unreal/Lyudmilovsk/Content/Localization/ship/strings_<l>.json the cruise.* words (ru, en, ja) + tools/ship/strings/*.json
    unreal/Lyudmilovsk/Content/Dev/teleports/ship.json            developer mode's places aboard (group "cruise")

The rules are the engine-free model's (Private/Cruise/CruiseModel.h, checked by tools/cruisesim/run.sh): this file only
holds the numbers and the words. The chart frame is the map's metres carried out to sea (X west, Y north).

The stage (where the furnished ship stands, ship frame -> world) is not written twice: it is computed here from the city
plan's plot P-SHIP (Content/City/city_plan.json, placed like every plot: tools/city/interiors.py local_to_world) and the
modellers' ship frame (blender/city/buildings/_shipframe.py). Until the plot exists the design's numbers stand in
("source": "design"), and the check says so. Named points come from the building export (blender/export/city/
motor_ship.json: things and spots) on top of a few fixed ones.
"""
import collections
import glob
import json
import math
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
OUT_JSON = os.path.join(CONTENT, "Cruise", "cruise.json")
OUT_STRINGS = os.path.join(CONTENT, "Localization", "ship", "strings_%s.json")
OUT_DEV = os.path.join(CONTENT, "Dev", "teleports", "ship.json")
PLAN = os.path.join(CONTENT, "City", "city_plan.json")
EXPORT = os.path.join(ROOT, "blender", "export", "city", "motor_ship.json")
EXTRA_STRINGS = os.path.join(ROOT, "tools", "ship", "strings")
LANGS = ("ru", "en", "ja")

SEASON = {"from": "04-15", "to": "11-15"}
VOYAGE = {"weekday": 4, "board": "16:00", "sail": "18:00", "days": 2, "landing_hours": 2}
SIGHT_M = 4000
SEA_DATUM_M = -43.0          # the sea's high-water datum (docs/tides.md); the waterline of the ship frame
STAGE_DESIGN_MAP = (26000.0, 4000.0)   # DESIGN: used only until the plan has P-SHIP
PORTAL_FACE_Y_CM = 790.0

# id, kind, x, y, radius (holds all of it), height, keep_off (the route never comes nearer the centre), more
ISLANDS = [
    {"id": "home", "kind": "anchorage", "x": 2000, "y": 1500, "radius": 0, "height": 0, "keep_off": 0},
    {"id": "brothers", "kind": "stacks", "x": 12000, "y": 7000, "radius": 160, "height": 40, "keep_off": 300, "count": 3},
    {"id": "arch", "kind": "arch", "x": 28000, "y": 16000, "radius": 70, "height": 32, "keep_off": 250},
    {"id": "birds", "kind": "cliff_isle", "x": 45000, "y": 12000, "radius": 300, "height": 70, "keep_off": 600,
     "length": 600, "width": 260, "yaw": 20, "birds": True},
    {"id": "lighthouse", "kind": "lighthouse_isle", "x": 52000, "y": 30000, "radius": 70, "height": 38, "keep_off": 300,
     "light": True},
    {"id": "long", "kind": "long_isle", "x": 30000, "y": 38000, "radius": 1250, "height": 55, "keep_off": 450,
     "length": 2500, "width": 600, "yaw": 0, "bay": [30000, 38850]},
    {"id": "seals", "kind": "seal_rocks", "x": 11000, "y": 24000, "radius": 180, "height": 3, "keep_off": 350, "seals": True},
]

# The landing at the Far Light (Saturday 16:30-18:30): the real rock is the city plot P-FARLIGHT (tools/city_plan.py),
# standing "offset_m" (x to the bow, y to starboard) from the stage ship's midship point; the route below brings the ship
# to the chart point and the heading for which the seascape shows the chart island exactly there. The launch runs from
# lead_s after the event's beginning to last_s before its end; the bosun warns warn_s seconds before the end.
ASHORE = {"island": "lighthouse", "plot": "P-FARLIGHT", "offset_m": [-24.0, 331.0], "lead_s": 300, "last_s": 600,
          "warn_s": [1800, 1200, 780]}
FL_ANCHOR = (52000 - ASHORE["offset_m"][0], 30000 - ASHORE["offset_m"][1])     # the ship's midship point at anchor (heading 0)
FL_R = 600

# The belt: circles joined by their tangents (CruiseModel.h). home: the roads; via: a turning point (r = the turn's radius);
# pass: an island left abeam at r from its centre during its event, at "speed" m/s; orbit: round an island at r, "laps"
# whole turns besides the turn of the route, from its event's beginning to its end; anchor: the ship stops in the middle
# of this circle's arc for its event. The legs are long on purpose: the ship keeps a walking pace of 2-3 m/s through the
# water and the hours between two islands are open sea.
ROUTE = [
    {"kind": "home", "at": [2390, 1580], "r": 400},
    # the Brothers and the Eye are left to STARBOARD (the route bends to starboard at them): the stage ship never turns, so
    # the sun is always on her starboard side, and the sunset stands behind the Brothers, the sunrise in the Eye (W5)
    {"kind": "via", "at": [7000, 4800], "r": 500},
    {"kind": "pass", "island": "brothers", "r": 520, "event": "isl_brothers", "speed": 1.6},
    {"kind": "via", "at": [20000, 16000], "r": 600},
    {"kind": "via", "at": [34000, 30000], "r": 800},
    {"kind": "via", "at": [46000, 26000], "r": 800},
    {"kind": "pass", "island": "arch", "r": 380, "event": "isl_arch", "speed": 1.4},
    {"kind": "via", "at": [38000, 4500], "r": 800},
    {"kind": "via", "at": [52000, 3500], "r": 800},
    {"kind": "orbit", "island": "birds", "r": 800, "laps": 2, "event": "isl_birds"},
    {"kind": "via", "at": [60000, 13000], "r": 800},
    # the landing at the Far Light (ASHORE below): she passes the rock 3 km off, turns back round a shoal and comes up to
    # it from the west, to lie with her bow to +X exactly ASHORE["offset_m"] from the island's chart point. The two turning
    # points either side of the anchor circle mirror each other (same r, same way round), so the middle of its arc, where
    # she stops, is the circle's lowest point and her heading there is 0: tools/cruisesim checks it to a centimetre.
    {"kind": "via", "at": [44500, 32500], "r": 1000},
    {"kind": "via", "at": [FL_ANCHOR[0] - 3000, FL_ANCHOR[1] + FL_R + 600], "r": 800},
    {"kind": "anchor", "island": "lighthouse", "at": [FL_ANCHOR[0], FL_ANCHOR[1] + FL_R], "r": FL_R, "event": "isl_lighthouse",
     "ashore": True},
    {"kind": "via", "at": [FL_ANCHOR[0] + 3000, FL_ANCHOR[1] + FL_R + 600], "r": 800},
    {"kind": "via", "at": [58000, 43000], "r": 800},
    {"kind": "via", "at": [42000, 52000], "r": 800},
    {"kind": "anchor", "island": "long", "at": [30000, 39150], "r": 300, "event": "anchor"},
    {"kind": "via", "at": [13000, 46000], "r": 600},
    {"kind": "pass", "island": "seals", "r": 520, "event": "isl_seals", "speed": 1.5},
    {"kind": "via", "at": [24000, 21000], "r": 800},
    {"kind": "via", "at": [20000, 9000], "r": 600},
    {"kind": "via", "at": [19000, 2200], "r": 500},
]

# id, day (1 Friday, 2 Saturday, 3 Sunday), from, to (None: a moment), where
EVENTS = [
    ("boarding", 1, "16:00", "18:00", "gangway"),
    ("departure", 1, "18:00", None, "deck"),
    ("drill", 1, "18:30", "19:00", "boat_deck"),
    ("dinner1", 1, "19:30", "20:30", "dining"),
    ("isl_brothers", 1, "20:15", "20:45", "deck"),
    ("concert1", 1, "21:00", "23:00", "saloon"),
    ("cards1", 1, "22:00", "01:00", "saloon"),
    ("lights_out", 1, "23:00", None, "cabin"),
    ("isl_arch", 2, "05:15", "05:45", "deck"),
    ("wake", 2, "07:00", None, "cabin"),
    ("exercise", 2, "07:15", "07:45", "boat_deck"),
    ("breakfast2", 2, "08:00", "09:00", "dining"),
    ("isl_birds", 2, "10:00", "11:30", "deck"),
    ("deckgames", 2, "11:30", "13:00", "boat_deck"),
    ("lunch2", 2, "13:00", "14:00", "dining"),
    ("quiet", 2, "14:00", "15:00", "cabin"),
    ("bridge_tour", 2, "15:00", "16:00", "bridge"),
    ("tea2", 2, "16:00", "16:30", "dining"),
    ("isl_lighthouse", 2, "16:30", "18:30", "deck"),
    ("captains_dinner", 2, "19:00", "20:30", "dining"),
    ("concert2", 2, "20:30", "23:30", "saloon"),
    ("cards2", 2, "22:00", "01:00", "saloon"),
    ("anchor", 3, "00:00", "06:00", "deck"),
    ("wake", 3, "07:00", None, "cabin"),
    ("breakfast3", 3, "08:00", "09:00", "dining"),
    ("quiz", 3, "10:00", "10:45", "saloon"),
    ("isl_seals", 3, "10:45", "11:15", "deck"),
    ("lunch3", 3, "13:00", "14:00", "dining"),
    ("diplomas", 3, "15:00", "15:30", "saloon"),
    ("home_in_sight", 3, "16:30", None, "deck"),
    ("arrival", 3, "18:00", None, "deck"),
    ("landing", 3, "18:00", "20:00", "gangway"),
]

TICKETS = [
    {"item": "cruise_ticket_hold", "class": "hold", "cabin": 15, "price": 90},
    {"item": "cruise_ticket", "class": "std", "cabin": 7, "price": 150},
    {"item": "cruise_ticket_lux", "class": "lux", "cabin": 11, "price": 300},
]

# ------------------------------------------------------------------------------------------------ words
R = "Внимание! Говорит радиоузел теплохода «Полдень». "
E = "Attention, please. This is the motor ship Polden's radio room. "
J = "お知らせします。こちらは客船ポルデニ号の放送室です。"

# event id -> (name ru, en, ja), (announcement ru, en, ja)
EVENT_WORDS = {
    "boarding": (("Посадка", "Boarding", "乗船"),
                 (R + "Начинается посадка. Катер ходит от головы пирса до шести часов. Пассажирский помощник выдаёт ключи от кают в вестибюле главной палубы.",
                  E + "Boarding has begun. The launch runs from the pier head until six o'clock. The purser hands out cabin keys in the vestibule on the main deck.",
                  J + "乗船を開始しました。桟橋の先端から六時までランチが出ます。客室の鍵は主甲板のロビーで事務長がお渡しします。")),
    "departure": (("Отход", "Departure", "出港"),
                  (R + "Отдать якорь… виноват, выбрать якорь! Три длинных гудка — мы уходим в рейс. Провожающих просим помахать с пирса.",
                   E + "Let go the anchor… I beg your pardon, weigh anchor! Three long blasts: we are under way. Those seeing us off are kindly asked to wave from the pier.",
                   J + "錨を下ろせ……失礼、錨を上げ！長音三回、出港いたします。お見送りの皆さま、桟橋から手を振ってください。")),
    "drill": (("Шлюпочная тревога (учебная)", "Lifeboat drill", "救命艇訓練"),
              (R + "Учебная шлюпочная тревога. Всем пассажирам собраться на шлюпочной палубе у шлюпки номер один. Нагрудники покажет боцман. Бежать не надо.",
               E + "Lifeboat drill. All passengers are to muster on the boat deck by boat number one. The bosun will show you the life jackets. There is no need to run.",
               J + "救命艇訓練を行います。乗客の皆さまはボートデッキの一号艇前にお集まりください。救命胴衣は甲板長がご説明します。走る必要はありません。")),
    "dinner1": (("Ужин", "Dinner", "夕食"),
                (R + "Ужин накрыт в столовой главной палубы. Сегодня котлеты по-флотски и компот. Приятного аппетита.",
                 E + "Dinner is served in the dining saloon on the main deck. Tonight: navy cutlets and stewed-fruit compote. Enjoy your meal.",
                 J + "主甲板の食堂に夕食のご用意ができました。今夜は海軍風カツレツとコンポートです。どうぞ召し上がれ。")),
    "isl_brothers": (("Каменные Братья", "The Stone Brothers", "石の兄弟岩"),
                     (R + "По правому борту — Каменные Братья. Три скалы, старший сорок метров. Солнце садится как раз между ними: фотографы, не зевайте.",
                      E + "On the starboard side: the Stone Brothers. Three stacks, the eldest forty metres tall. The sun sets right between them: photographers, look sharp.",
                      J + "右舷に「石の兄弟岩」が見えます。三つの岩柱で、いちばん上の兄は高さ四十メートル。夕日はちょうどその間に沈みます。写真を撮る方はお見逃しなく。")),
    "concert1": (("Вечер джаза", "Jazz evening", "ジャズの夕べ"),
                 (R + "В музыкальном салоне играет «Полуденный квартет» Савелия Блюма. Вход свободный, танцевать разрешается.",
                  E + "In the music saloon Savely Blum's Noon Quartet is playing. Admission is free, dancing is allowed.",
                  J + "音楽サロンでサヴェーリー・ブリュームの「正午カルテット」が演奏します。入場無料、ダンスもご自由に。")),
    "cards1": (("Карточный стол", "The card table", "カードテーブル"),
               (R + "В салоне составляется партия в покер. Играют на интерес и на копейки. Администрация за проигрыши не отвечает.",
                E + "A game of poker is being made up in the saloon. The stakes are small change and honour. The management takes no responsibility for losses.",
                J + "サロンでポーカーの卓が立ちます。賭けるのは小銭と名誉です。負けについて当船は責任を負いかねます。")),
    "lights_out": (("Отбой", "Lights out", "消灯"),
                   (R + "Двадцать три часа. В каютах отбой. Радиоузел желает вам спокойной ночи; вахта не спит.",
                    E + "Twenty-three hundred hours. Lights out in the cabins. The radio room wishes you a good night; the watch stays awake.",
                    J + "二十三時です。客室は消灯の時間です。放送室よりおやすみなさい。当直は起きております。")),
    "isl_arch": (("Игольное Ушко", "The Needle's Eye", "針の穴岩"),
                 (R + "Доброе утро тем, кто не спит. Слева по борту — Игольное Ушко. Если встать на баке, солнце взойдёт прямо в арке.",
                  E + "Good morning to those who are awake. On the port side: the Needle's Eye. From the foredeck you will see the sun rise right inside the arch.",
                  J + "起きている皆さま、おはようございます。左舷に「針の穴岩」が見えます。船首甲板に立つと、朝日がアーチの真ん中から昇ります。")),
    "wake": (("Подъём", "Reveille", "起床"),
             (R + "Доброе утро! Семь часов. Погода — как заказывали. Умывальники свободны, зарядка через пятнадцать минут.",
              E + "Good morning! It is seven o'clock. The weather is as ordered. The washbasins are free, morning exercise is in fifteen minutes.",
              J + "おはようございます。七時です。お天気はご注文どおり。洗面台は空いています。十五分後に朝の体操です。")),
    "exercise": (("Зарядка", "Morning exercise", "朝の体操"),
                 (R + "На шлюпочной палубе — утренняя зарядка. Проводит культорганизатор Римма Вольская. Ноги на ширине плеч!",
                  E + "Morning exercise on the boat deck, led by our entertainer Rimma Volskaya. Feet shoulder-width apart!",
                  J + "ボートデッキで朝の体操を行います。指導はレクリエーション係のリンマ・ヴォリスカヤ。足は肩幅に！")),
    "breakfast2": (("Завтрак", "Breakfast", "朝食"),
                   (R + "Завтрак в столовой: каша, яйцо, какао. Кто проспал зарядку — каша всё равно положена.",
                    E + "Breakfast in the dining saloon: porridge, an egg, cocoa. Those who slept through the exercise still get their porridge.",
                    J + "食堂で朝食です。おかゆ、卵、ココア。体操を寝過ごした方にも、おかゆはちゃんと出ます。")),
    "isl_birds": (("Остров Птичий", "Bird Island", "鳥の島"),
                  (R + "Подходим к острову Птичьему. Обойдём его дважды, малым ходом. Птиц просим не кормить котлетами. Бинокли — у пассажирского помощника.",
                   E + "We are coming up to Bird Island. We shall go round it twice, dead slow. Please do not feed the birds cutlets. Binoculars are with the purser.",
                   J + "まもなく「鳥の島」です。微速で二周します。鳥にカツレツを与えないでください。双眼鏡は事務長がお貸しします。")),
    "deckgames": (("Палубные игры", "Deck games", "デッキゲーム"),
                  (R + "На шлюпочной палубе — турнир по шаффлборду и кольцебросу. Запись у Риммы Вольской. Победителю — приз капитана.",
                   E + "On the boat deck: the shuffleboard and ring-toss tournament. Sign up with Rimma Volskaya. The winner gets the captain's prize.",
                   J + "ボートデッキでシャッフルボードと輪投げの大会を行います。申し込みはリンマ・ヴォリスカヤまで。優勝者には船長賞。")),
    "lunch2": (("Обед", "Lunch", "昼食"),
               (R + "Обед. Тётя Паша сварила борщ, и это не обсуждается. Столовая, главная палуба.",
                E + "Lunch. Aunt Pasha has made borscht, and that is not up for discussion. The dining saloon, main deck.",
                J + "昼食です。パーシャおばさんがボルシチを作りました。異論は受け付けません。主甲板の食堂へどうぞ。")),
    "quiet": (("Тихий час", "Quiet hour", "お昼寝の時間"),
              (R + "Тихий час. Шезлонги на шлюпочной палубе свободны. Оркестр отдыхает, чайки — нет.",
               E + "Quiet hour. The deck chairs on the boat deck are free. The band is resting; the gulls are not.",
               J + "お昼寝の時間です。ボートデッキのデッキチェアが空いています。楽団は休憩中、カモメは休みません。")),
    "bridge_tour": (("Экскурсия на мостик", "Tour of the bridge", "船橋見学"),
                    (R + "Желающих приглашают на мостик. Капитан Громов покажет штурвал, телеграф и карту. Штурвал не крутить, пока не разрешат.",
                     E + "Those who wish are invited to the bridge. Captain Gromov will show the wheel, the telegraph and the chart. Do not turn the wheel until you are told you may.",
                     J + "ご希望の方を船橋へご案内します。グロモフ船長が舵輪、テレグラフ、海図をお見せします。許可があるまで舵輪は回さないでください。")),
    "tea2": (("Чай", "Tea", "お茶の時間"),
             (R + "Чай с сушками в столовой. Самовар настоящий, прошу не сомневаться.",
              E + "Tea and sushki rings in the dining saloon. The samovar is a real one, have no doubt.",
              J + "食堂でお茶とスーシキをどうぞ。サモワールは本物です。ご安心を。")),
    "isl_lighthouse": (("Маяк Дальний", "The Far Light", "遠灯台"),
                       (R + "Становимся на якорь у маяка Дальнего, это последний огонь перед открытым морем. Желающих катер свезёт на скалу: стоим два часа. Последний катер обратно — в восемнадцать двадцать. Смотритель гостям рад, но виду не подаст.",
                        E + "We are anchoring off the Far Light, the last light before the open sea. The launch will take those who wish to the rock: we lie here two hours. The last launch back leaves at eighteen twenty. The keeper is glad of guests, though he will not show it.",
                        J + "外海の手前、最後の灯「遠灯台」の沖に錨を下ろします。ご希望の方はランチで岩へお渡しします。停泊は二時間。帰りの最終ランチは十八時二十分です。灯台守は客人を喜びますが、顔には出しません。")),
    "captains_dinner": (("Капитанский ужин", "The captain's dinner", "船長主催の夕食会"),
                        (R + "Капитанский ужин. Капитан Громов приглашает всех к столу. Форма одежды — какая есть, но причесаться.",
                         E + "The captain's dinner. Captain Gromov invites everyone to table. Dress as you are, but do comb your hair.",
                         J + "船長主催の夕食会です。グロモフ船長が皆さまを食卓にお招きします。服装はそのままで結構ですが、髪はとかしてきてください。")),
    "concert2": (("Большой вечер с танцами", "Gala evening with dancing", "ダンスの夕べ"),
                 (R + "Большой вечер в музыкальном салоне: «Полуденный квартет», танцы до половины двенадцатого. Дамы приглашают кавалеров, и наоборот.",
                  E + "Gala evening in the music saloon: the Noon Quartet, dancing until half past eleven. Ladies ask the gentlemen, and the other way round.",
                  J + "音楽サロンで大夜会です。「正午カルテット」の演奏で、十一時半までダンスを。ご婦人から殿方へ、殿方からご婦人へ、どうぞお誘いください。")),
    "cards2": (("Карточный стол", "The card table", "カードテーブル"),
               (R + "Карточный стол снова открыт. Инженер из Риги обещал дать отыграться.",
                E + "The card table is open again. The engineer from Riga has promised everyone a chance to win it back.",
                J + "カードテーブルが再び開きます。リガの技師殿が、皆さまに取り返す機会をくださるそうです。")),
    "anchor": (("Ночная стоянка у острова Долгого", "Night anchorage at Long Island", "長島での夜の停泊"),
               (R + "Становимся на якорь в бухте острова Долгого. Машина остановлена. Слышно, как дышит море. Спокойной ночи.",
                E + "We are anchoring in the bay of Long Island. The engine is stopped. You can hear the sea breathe. Good night.",
                J + "長島の入り江に錨を下ろします。機関は停止しました。海の息づかいが聞こえます。おやすみなさい。")),
    "breakfast3": (("Завтрак", "Breakfast", "朝食"),
                   (R + "Завтрак: сырники со сметаной. Тётя Паша просила передать, что добавки хватит всем.",
                    E + "Breakfast: curd fritters with sour cream. Aunt Pasha asked us to say there are seconds for everyone.",
                    J + "朝食はスメタナ添えのスィルニキです。パーシャおばさんから伝言です。おかわりは全員分あります。")),
    "quiz": (("Морская викторина", "Sea quiz", "海のクイズ"),
             (R + "В музыкальном салоне — морская викторина. Сколько узлов в миле? Ни одного: это ловушка. Остальные вопросы у Риммы.",
              E + "In the music saloon: the sea quiz. How many knots are there in a mile? None: that one is a trap. Rimma has the other questions.",
              J + "音楽サロンで海のクイズです。一海里は何ノット？答えはゼロ。これは引っかけ問題です。ほかの問題はリンマが出します。")),
    "isl_seals": (("Тюленьи камни", "The Seal Rocks", "アザラシ岩"),
                  (R + "Слева — Тюленьи камни. Идём тихо. Тюлени сегодня дома; просьба не свистеть, они обижаются.",
                   E + "To port: the Seal Rocks. We are going quietly. The seals are at home today; please do not whistle, they take offence.",
                   J + "左手に「アザラシ岩」です。静かに進みます。今日はアザラシが在宅中です。口笛はご遠慮ください。彼らは気を悪くします。")),
    "lunch3": (("Обед", "Lunch", "昼食"),
               (R + "Последний обед рейса: уха и макароны по-флотски. Столовая, главная палуба.",
                E + "The last lunch of the voyage: fish soup and navy-style macaroni. The dining saloon, main deck.",
                J + "この航海最後の昼食です。魚のスープ「ウハー」と海軍風マカロニ。主甲板の食堂へどうぞ。")),
    "diplomas": (("Вручение дипломов", "Diplomas", "修了証の授与"),
                 (R + "В музыкальном салоне капитан вручает дипломы морехода всем, кто дошёл до конца рейса. Не дошедших нет.",
                  E + "In the music saloon the captain is presenting seafarer's diplomas to all who have made it to the end of the voyage. Nobody has failed to.",
                  J + "音楽サロンで、航海を最後までやり遂げた皆さまに船長が「海の旅人」の修了証を授与します。脱落者はおりません。")),
    "home_in_sight": (("Людмиловск на горизонте", "Lyudmilovsk in sight", "リュドミーロフスクが見えます"),
                      (R + "Прямо по курсу показался Людмиловск. Кто первым увидит маяк на пирсе — тому компот.",
                       E + "Lyudmilovsk has come into sight dead ahead. Whoever spots the lighthouse on the pier first gets a compote.",
                       J + "正面にリュドミーロフスクが見えてきました。桟橋の灯台を最初に見つけた方にはコンポートを差し上げます。")),
    "arrival": (("Прибытие", "Arrival", "到着"),
                (R + "Мы на рейде Людмиловска. Якорь отдан. Экипаж благодарит вас за рейс.",
                 E + "We are on the Lyudmilovsk roads. The anchor is down. The crew thanks you for the voyage.",
                 J + "リュドミーロフスクの泊地に着きました。錨を下ろしました。乗組員一同、ご乗船に感謝いたします。")),
    "landing": (("Высадка", "Landing", "下船"),
                (R + "Катер у трапа. Высадка до восьми часов вечера. Ключи от кают просим сдать пассажирскому помощнику. До следующей пятницы!",
                 E + "The launch is at the gangway. Landing goes on until eight in the evening. Please hand your cabin keys to the purser. Until next Friday!",
                 J + "ランチが舷門に着きました。下船は夜八時までです。客室の鍵は事務長にお返しください。また来週の金曜日に！")),
}

ISLAND_NAMES = {
    "home": ("рейд Людмиловска", "the Lyudmilovsk roads", "リュドミーロフスク泊地"),
    "brothers": ("Каменные Братья", "the Stone Brothers", "石の兄弟岩"),
    "arch": ("Игольное Ушко", "the Needle's Eye", "針の穴岩"),
    "birds": ("остров Птичий", "Bird Island", "鳥の島"),
    "lighthouse": ("маяк Дальний", "the Far Light", "遠灯台"),
    "long": ("остров Долгий", "Long Island", "長島"),
    "seals": ("Тюленьи камни", "the Seal Rocks", "アザラシ岩"),
}

STRINGS = {
    "cruise.ship.name": ("теплоход «Полдень»", "the motor ship Polden", "客船ポルデニ号"),
    "cruise.speaker.radio": ("Радиоузел теплохода «Полдень»", "The Polden's radio room", "ポルデニ号 放送室"),
    "cruise.speaker.purser": ("Нонна Аркадьевна, пассажирский помощник", "Nonna Arkadyevna, the purser", "事務長 ノンナ・アルカージエヴナ"),
    "cruise.speaker.bosun": ("Боцман Тарас Нечипоренко", "Bosun Taras Nechiporenko", "甲板長 タラス・ネチポレンコ"),
    "cruise.say.board.hold": (
        "Добро пожаловать на борт! Каюта пятнадцатая, нижняя палуба: по трапу вниз и направо. Вот ключ, не утопите. Ужин в половине восьмого.",
        "Welcome aboard! Cabin fifteen, lower deck: down the stairs and to the right. Here is your key, do not drop it in the sea. Dinner is at half past seven.",
        "ご乗船ありがとうございます。お部屋は十五号室、下の甲板です。階段を下りて右へ。鍵をどうぞ、海に落とさないでくださいね。夕食は七時半です。"),
    "cruise.say.board.std": (
        "Добро пожаловать на борт! Каюта седьмая, здесь же, на главной палубе: по коридору в нос, правый борт. Вот ключ. Ужин в половине восьмого.",
        "Welcome aboard! Cabin seven, here on the main deck: along the corridor toward the bow, starboard side. Here is your key. Dinner is at half past seven.",
        "ご乗船ありがとうございます。お部屋は七号室、この主甲板です。廊下を船首のほうへ、右舷側。鍵をどうぞ。夕食は七時半です。"),
    "cruise.say.board.lux": (
        "Добро пожаловать на борт! У вас люкс, одиннадцатая каюта: прогулочная палуба, в корму. Вот ключ. Капитан просил кланяться. Ужин в половине восьмого.",
        "Welcome aboard! You have a de luxe cabin, number eleven: the promenade deck, aft. Here is your key. The captain sends his compliments. Dinner is at half past seven.",
        "ご乗船ありがとうございます。特等の十一号室でございます。遊歩甲板の船尾側です。鍵をどうぞ。船長がよろしくと申しておりました。夕食は七時半です。"),
    "cruise.say.board.late": (
        "А, вот и вы! Каюта номер {0}, ключ держите. Программа на доске в вестибюле.",
        "Ah, there you are! Cabin number {0}, here is the key. The programme is on the board in the vestibule.",
        "あら、いらっしゃい。お部屋は{0}号室、鍵をどうぞ。予定表はロビーの掲示板にあります。"),
    "cruise.say.ashore": (
        "Приехали! Последний катер, все на берег. Ключ от каюты — мне. В пятницу приходите опять, место найдём.",
        "Here we are! Last launch, all ashore. The cabin key goes to me. Come again on Friday, we will find you a berth.",
        "着いたぞ！最後のランチだ、全員上陸。客室の鍵はこっちへ。金曜日にまた来な、寝床は見つけてやる。"),
    "cruise.say.ashore_over": (
        "Эй, на борту! Рейс-то давно кончился, а он всё спит. На берег, на берег! Катер ждать не будет.",
        "Ahoy there aboard! The voyage ended long ago and he is still asleep. Ashore with you, ashore! The launch will not wait.",
        "おーい、そこの人！航海はとっくに終わったのに、まだ寝てるのか。上陸、上陸！ランチは待っちゃくれないぞ。"),
    "cruise.say.ashore_early": (
        "Сходите? Ну, дело хозяйское. Билет пропал, ключ — мне. Попутного ветра!",
        "Going ashore? Well, it is your own business. The ticket is spent, the key goes to me. Fair winds!",
        "降りるのか？まあ、好きにしな。切符は無効だ、鍵はこっちへ。いい風を！"),
    "cruise.say.overboard": (
        "Человек за бортом! Стоп машина! …Круг бросили, вытащили. Живой? Марш сушиться. И через леера больше не перегибаться!",
        "Man overboard! Stop engine! …Threw the ring, hauled him out. Alive? Off you go and dry out. And no more leaning over the rails!",
        "人が落ちたぞ！機関停止！……浮き輪を投げて、引き上げた。生きてるか？さっさと乾かしてこい。もう手すりから身を乗り出すんじゃないぞ！"),
    "cruise.say.stowaway": (
        "А вы как здесь? Без билета на борту не положено. Ну да ладно, осмотритесь — и на берег.",
        "And how did you get here? Nobody is allowed aboard without a ticket. Oh well, have a look round, and then ashore with you.",
        "おや、どうやってここへ？切符なしの乗船は禁止だ。まあいい、ひと回り見たら上陸するんだな。"),
    "cruise.say.ashore_warn.1": (
        "На скале! Восемнадцать ноль-ноль. Катер уходит через двадцать минут. Подтягивайтесь к причалу, не спеша.",
        "Ahoy on the rock! Eighteen hundred. The launch leaves in twenty minutes. Make your way to the jetty, no hurry.",
        "岩の皆さん！十八時だ。ランチは二十分後に出る。ゆっくりでいい、桟橋へ集まってくれ。"),
    "cruise.say.ashore_warn.2": (
        "На скале! Десять минут до последнего катера. Кто на галерее маяка — спускайтесь, ступеньки считать не обязательно.",
        "Ahoy on the rock! Ten minutes to the last launch. Whoever is up on the lighthouse gallery, come down, and never mind counting the steps.",
        "岩の皆さん！最終ランチまであと十分。灯台の回廊にいる人は下りてきな。段を数えなくていいから。"),
    "cruise.say.ashore_warn.3": (
        "Последний катер! Три минуты. Кого не досчитаюсь — приду за тем сам, и это хуже.",
        "Last launch! Three minutes. Anyone I find missing I shall come and fetch myself, and that is worse.",
        "最終ランチだ！あと三分。数が合わなかったら、わしが自分で迎えに行く。そっちのほうが怖いぞ。"),
    "cruise.say.ashore_fetch": (
        "Вот он где! Теплоход ждать не нанимался. В катер — и чтобы я тебя на скалах больше не искал. Смотрителю поклон передал?",
        "So there you are! The ship is not paid to wait. Into the launch with you, and let me not go hunting for you on the rocks again. Did you give the keeper my regards?",
        "こんなところにいたか！船は待つために雇われてるんじゃないぞ。ランチに乗れ。もう岩の上を探し回らせるなよ。灯台守によろしく言ったか？"),
    "cruise.say.ashore_swim": (
        "Куда?! Вода восемь градусов, а до теплохода триста метров. Вылезай на причал, пловец. Катер довезёт.",
        "Where to?! The water is eight degrees and the ship is three hundred metres off. Out you come onto the jetty, swimmer. The launch will take you.",
        "どこへ行く！水温は八度、船まで三百メートルだ。桟橋に上がれ、泳ぎ屋さん。ランチが送ってやる。"),
    "cruise.ui.map.farlight": ("Маяк Дальний", "The Far Light", "遠灯台"),
    "dev.tp.farlight_jetty": ("Маяк Дальний: причал", "The Far Light: the jetty", "遠灯台：桟橋"),
    "dev.tp.farlight_hut": ("Маяк Дальний: у домика смотрителя", "The Far Light: by the keeper's hut", "遠灯台：灯台守の小屋の前"),
    "dev.tp.farlight_gallery": ("Маяк Дальний: галерея маяка", "The Far Light: the lantern gallery", "遠灯台：灯室の回廊"),
    "dev.action.cruise.landing_now": ("Круиз: высадка на маяк (суббота 16:40, на скалу)", "Cruise: the landing at the Far Light (Saturday 16:40, onto the rock)", "クルーズ：遠灯台に上陸（土曜16:40、岩へ）"),
    "cruise.ui.next": ("Далее по программе: {0} — через {1} мин.", "Next on the programme: {0}, in {1} min.", "次の予定：{0}（{1}分後）"),
    "cruise.ui.next_none": ("Теплоход «Полдень» сейчас не в рейсе.", "The Polden is not on a voyage now.", "ポルデニ号はただいま航海中ではありません。"),
    "cruise.ui.map.roads": ("Рейд: теплоход «Полдень»", "The roads: the motor ship Polden", "泊地：客船ポルデニ号"),
    "cruise.ui.at_sea": ("В рейсе на теплоходе «Полдень»", "At sea aboard the Polden", "ポルデニ号で航海中"),
    "cruise.ui.phase.laid": ("на зимнем отстое", "laid up for the winter", "冬季係船中"),
    "cruise.ui.phase.port": ("на рейде", "on the roads", "泊地に停泊中"),
    "cruise.ui.phase.boarding": ("идёт посадка", "boarding", "乗船中"),
    "cruise.ui.phase.underway": ("на ходу", "under way", "航行中"),
    "cruise.ui.phase.circling": ("обходит остров", "rounding an island", "島を周回中"),
    "cruise.ui.phase.anchored": ("на якоре", "at anchor", "停泊中"),
    "cruise.ui.phase.landing": ("идёт высадка", "landing", "下船中"),
    "cruise.ui.class.hold": ("третий класс (трюмная каюта)", "third class (a hold cabin)", "三等（船倉の客室）"),
    "cruise.ui.class.std": ("второй класс", "second class", "二等"),
    "cruise.ui.class.lux": ("люкс", "de luxe", "特等"),
    "dev.group.cruise": ("Теплоход", "Motor ship", "客船"),
    "dev.tp.ship_gangway": ("Теплоход: вестибюль у трапа", "Ship: the vestibule by the gangway", "客船：舷門のロビー"),
    "dev.tp.ship_saloon": ("Теплоход: музыкальный салон", "Ship: the music saloon", "客船：音楽サロン"),
    "dev.tp.ship_dining": ("Теплоход: столовая", "Ship: the dining saloon", "客船：食堂"),
    "dev.tp.ship_bridge": ("Теплоход: мостик", "Ship: the bridge", "客船：船橋"),
    "dev.tp.ship_boat_deck": ("Теплоход: шлюпочная палуба", "Ship: the boat deck", "客船：ボートデッキ"),
    "dev.tp.ship_smoking_deck": ("Теплоход: кормовая палуба", "Ship: the aft (smoking) deck", "客船：船尾甲板"),
    "dev.tp.ship_foredeck": ("Теплоход: бак", "Ship: the foredeck", "客船：船首甲板"),
    "dev.tp.ship_cabin_hold": ("Теплоход: каюта 15 (трюм)", "Ship: cabin 15 (hold)", "客船：十五号室（船倉）"),
    "dev.tp.ship_cabin_std": ("Теплоход: каюта 7", "Ship: cabin 7", "客船：七号室"),
    "dev.tp.ship_cabin_lux": ("Теплоход: люкс 11", "Ship: de luxe cabin 11", "客船：特等十一号室"),
    "dev.action.cruise.board_now": ("Круиз: билет, пятница 17:30, к катеру", "Cruise: a ticket, Friday 17:30, to the launch", "クルーズ：切符、金曜17:30、ランチへ"),
    "dev.action.cruise.next_event": ("Круиз: к следующему событию", "Cruise: on to the next event", "クルーズ：次の予定へ"),
    "dev.action.cruise.ashore": ("Круиз: на берег", "Cruise: ashore", "クルーズ：上陸"),
    "dev.action.cruise.report": ("Круиз: отчёт в лог", "Cruise: report to the log", "クルーズ：ログに報告"),
    "dev.action.cruise.my_cabin": ("Круиз: к своей каюте", "Cruise: to my cabin", "クルーズ：自分の客室へ"),
    "dev.action.cruise.ticket_box": ("Круиз: к кассе на пирсе", "Cruise: to the ticket box on the pier", "クルーズ：桟橋の切符売り場へ"),
    "dev.action.cruise.ticket_hold": ("Круиз: билет в трюм", "Cruise: a hold ticket", "クルーズ：船倉の切符"),
    "dev.action.cruise.ticket_lux": ("Круиз: билет люкс", "Cruise: a de luxe ticket", "クルーズ：特等の切符"),
}


def fail(msg):
    print("cruise.py: " + msg)
    sys.exit(1)


def clock(text):
    h, m = text.split(":")
    return int(h) * 3600 + int(m) * 60


def build_events():
    sail = clock(VOYAGE["sail"])
    out = []
    for eid, day, a, b, where in EVENTS:
        if eid not in EVENT_WORDS:
            fail("event %s has no words" % eid)
        t0 = (day - 1) * 86400 + clock(a) - sail
        t1 = t0 + 600 if b is None else (day - 1) * 86400 + clock(b) - sail + (86400 if clock(b) <= clock(a) else 0)
        ev = collections.OrderedDict([("id", eid), ("day", day), ("from", a)])
        if b is not None:
            ev["to"] = b
        ev.update([("t0", t0), ("t1", t1), ("announce", "cruise.announce." + eid), ("name", "cruise.event.%s.name" % eid),
                   ("where", where)])
        out.append(ev)
    out.sort(key=lambda e: e["t0"])
    return out


def build_strings():
    tables = {l: collections.OrderedDict() for l in LANGS}

    def put(key, words):
        for i, l in enumerate(LANGS):
            if key in tables[l]:
                fail("string %s twice" % key)
            if not words[i].strip():
                fail("string %s is empty in %s" % (key, l))
            tables[l][key] = words[i]
    for eid, (name, announce) in EVENT_WORDS.items():
        put("cruise.event.%s.name" % eid, name)
        put("cruise.announce." + eid, announce)
        # developer mode: an action per event (ULyudCruise registers "cruise.jump_<id>")
        put("dev.action.cruise.jump_" + eid, ["Круиз: к событию «%s»" % name[0], "Cruise: jump to \"%s\"" % name[1], "クルーズ：「%s」へ" % name[2]])
    for iid, words in ISLAND_NAMES.items():
        put("cruise.island.%s.name" % iid, words)
    for key, words in STRINGS.items():
        put(key, words)
    # other parts' words for the ship table (W4_contract.md section 5)
    for path in sorted(glob.glob(os.path.join(EXTRA_STRINGS, "*.json"))):
        with open(path, encoding="utf-8") as f:
            extra = json.load(f)
        for key, words in extra.items():
            if key.startswith("cruise."):
                fail("%s: %s: the cruise.* keys are tools/ship/cruise.py's own" % (os.path.basename(path), key))
            if not (isinstance(words, dict) and all(isinstance(words.get(l), str) and words[l].strip() for l in LANGS)):
                fail("%s: %s needs ru, en and ja" % (os.path.basename(path), key))
            put(key, [words[l] for l in LANGS])
    return tables


def ship_frame():
    sys.path.insert(0, os.path.join(ROOT, "blender", "city", "buildings"))
    try:
        import _shipframe as S
        return S
    except Exception:  # the modellers' helper is not in this checkout yet
        return None
    finally:
        sys.path.pop(0)


def build_stage(S):
    """Ship frame (stern, centreline, waterline; x to the bow, y to starboard) -> Unreal: the origin (cm) and the yaw."""
    plot = None
    if os.path.exists(PLAN):
        with open(PLAN, encoding="utf-8") as f:
            plan = json.load(f)
        plot = next((p for p in plan["plots"] if p["id"] == "P-SHIP"), None)
    if plot is None or S is None:
        x, y = STAGE_DESIGN_MAP
        return {"world_cm": [x * 100.0, y * 100.0 + PORTAL_FACE_Y_CM, SEA_DATUM_M * 100.0], "yaw_deg": 0.0, "source": "design"}
    sys.path.insert(0, os.path.join(ROOT, "tools", "city"))
    try:
        import interiors as I
    finally:
        sys.path.pop(0)
    front = I.ue_m(plan, plot["front"])
    o = I.local_to_world(front, plot["yaw"], S.p(0.0, 0.0, 0.0))
    bow = I.local_to_world(front, plot["yaw"], S.p(1.0, 0.0, 0.0))
    stbd = I.local_to_world(front, plot["yaw"], S.p(0.0, 1.0, 0.0))
    fx, fy = bow[0] - o[0], bow[1] - o[1]
    rx, ry = stbd[0] - o[0], stbd[1] - o[1]
    # Unreal's right of a forward (fx, fy) is (-fy, fx): the ship frame must not come out mirrored
    if abs(rx + fy) > 1e-6 or abs(ry - fx) > 1e-6:
        fail("the ship frame comes out mirrored in the world: check _shipframe.p against interiors.local_to_world")
    return {"world_cm": [round(o[0] * 100.0, 1), round(o[1] * 100.0, 1), round(o[2] * 100.0, 1)],
            "yaw_deg": round(math.degrees(math.atan2(fy, fx)), 3), "source": "plan"}


def build_points(S):
    """Named points in ship-frame cm: fixed ones from the general arrangement, then every thing and spot of the export."""
    z = S.Z if S else {"D": 0.3, "C": 2.8, "B": 5.4, "A": 8.0, "BR": 10.6}
    funnel = S.Z_FUNNEL_TOP if S else 13.4
    pts = collections.OrderedDict()

    def put(name, x, y, zz):
        pts[name] = [round(x * 100.0, 1), round(y * 100.0, 1), round(zz * 100.0, 1)]
    put("stern", 0.0, 0.0, z["C"])
    put("bow", 61.0, 0.0, z["C"] + 1.0)
    put("midship", 31.0, 0.0, z["C"])
    put("funnel_top", 23.5, 0.0, funnel)
    put("aft_deck", 4.5, -1.5, z["C"])
    put("smoking_deck", 8.0, 0.0, z["B"])
    put("foredeck", 52.5, 2.2, z["C"] + 0.4)
    put("gangway", 31.1, 3.4, z["C"])
    put("vestibule", 31.4, 2.4, z["C"])
    put("dining", 13.0, 0.0, z["C"])
    put("saloon", 39.5, 0.0, z["B"])
    put("bandstand", 43.5, 0.0, z["B"])
    put("card_table", 35.0, 2.2, z["B"])
    put("stern_rail", 2.6, 0.0, z["B"])
    put("boat_deck", 12.0, 0.0, z["A"])
    put("bridge", 40.3, 0.0, z["BR"])
    put("cabin_hold", 34.7, 1.4, z["D"])
    put("cabin_std", 36.2, 1.5, z["C"])
    put("cabin_lux", 13.6, 1.4, z["B"])
    source = "design"
    if S is not None and os.path.exists(EXPORT):
        with open(EXPORT, encoding="utf-8") as f:
            ex = json.load(f)
        source = "export"
        for row in list(ex.get("spots", [])) + list(ex.get("things", [])):
            x, y, zz = S.unp(*row["pos"])
            if row["id"] in pts or row in ex.get("things", []):
                put(row["id"], x, y, zz)
    return pts, source


def hull_plan(S):
    """The hull's outline at the main deck: [x, half-breadth] in cm from the stern to the stem (_shipframe.half)."""
    if S is None:
        return [[0.0, 0.0], [200.0, 360.0], [800.0, 470.0], [1400.0, 500.0], [4000.0, 500.0], [5000.0, 360.0], [5800.0, 150.0], [6200.0, 0.0]]
    return [[round(x * 100.0, 1), round(S.half(x) * 100.0, 1)] for x in S.STATIONS]


def build_dev(stage, pts):
    """Developer mode's places aboard: map metres + world z of a few points (a hand above the deck: the capsule's middle)."""
    yaw = math.radians(stage["yaw_deg"])
    ox, oy, oz = stage["world_cm"]

    def tp(tid, point, face):
        x, y, z = pts[point]
        wx = ox + x * math.cos(yaw) - y * math.sin(yaw)
        wy = oy + x * math.sin(yaw) + y * math.cos(yaw)
        return collections.OrderedDict([("id", tid), ("group", "cruise"), ("name", "dev.tp." + tid),
                                        ("x", round(wx / 100.0, 2)), ("y", round((wy - PORTAL_FACE_Y_CM) / 100.0, 2)),
                                        ("z", round(oz + z + 100.0, 1)), ("yaw", round((stage["yaw_deg"] + face) % 360.0, 1))])
    return {"teleports": [tp("ship_gangway", "vestibule", 180.0), tp("ship_saloon", "saloon", 0.0), tp("ship_dining", "dining", 180.0),
                          tp("ship_bridge", "bridge", 0.0), tp("ship_boat_deck", "boat_deck", 180.0),
                          tp("ship_smoking_deck", "smoking_deck", 180.0), tp("ship_foredeck", "foredeck", 0.0),
                          tp("ship_cabin_hold", "cabin_hold", 90.0), tp("ship_cabin_std", "cabin_std", 90.0),
                          tp("ship_cabin_lux", "cabin_lux", 90.0)]}


def build():
    islands = []
    for isl in ISLANDS:
        row = collections.OrderedDict(isl)
        row["name"] = "cruise.island.%s.name" % isl["id"]
        if isl["id"] not in ISLAND_NAMES:
            fail("island %s has no name" % isl["id"])
        islands.append(row)
    ids = {i["id"] for i in ISLANDS}
    events = build_events()
    ev_ids = {e["id"] for e in events}
    for stop in ROUTE:
        if stop.get("island") and stop["island"] not in ids:
            fail("route: unknown island %s" % stop["island"])
        if stop["kind"] in ("pass", "orbit", "anchor") and stop.get("event") not in ev_ids:
            fail("route: stop at %s needs an event" % stop.get("island"))
    S = ship_frame()
    stage = build_stage(S)
    points, points_source = build_points(S)
    data = collections.OrderedDict([
        ("about", "The motor ship Polden's cruise. Generated by tools/ship/cruise.py: do not edit."),
        ("season", SEASON), ("voyage", VOYAGE), ("sight_m", SIGHT_M), ("ramp_s", 300), ("wait_cap_min", 180),
        ("islands", islands), ("route", ROUTE), ("ashore", ASHORE), ("events", events), ("tickets", TICKETS),
        ("stage", stage), ("roads", {"sea_z_cm": SEA_DATUM_M * 100.0, "portal_face_y_cm": PORTAL_FACE_Y_CM}),
        ("hull", {"length_cm": 6200.0, "half_beam_cm": 500.0, "plan_cm": hull_plan(S)}),
        ("pier_fallback", {"map": [311.3, 905.0], "yaw_deg": 180.0}),
        ("points_source", points_source), ("points", points),
    ])
    files = {OUT_JSON: json.dumps(data, ensure_ascii=False, indent=1) + "\n",
             OUT_DEV: json.dumps(build_dev(stage, points), ensure_ascii=False, indent=1) + "\n"}
    for lang, table in build_strings().items():
        files[OUT_STRINGS % lang] = json.dumps(table, ensure_ascii=False, indent=1) + "\n"
    return files, stage, points_source


def main():
    check = "--check" in sys.argv[1:]
    files, stage, points_source = build()
    stale = []
    for path, text in files.items():
        old = None
        if os.path.exists(path):
            with open(path, encoding="utf-8") as f:
                old = f.read()
        if old == text:
            continue
        if check:
            stale.append(os.path.relpath(path, ROOT))
            continue
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            f.write(text)
        print("wrote " + os.path.relpath(path, ROOT))
    if stage["source"] != "plan":
        print("cruise.py: NOTE the stage is the design's (the city plan has no P-SHIP, or _shipframe.py is missing): rerun when it has")
    if points_source != "export":
        print("cruise.py: NOTE the points are the general arrangement's (no blender/export/city/motor_ship.json yet): rerun when it is built")
    if stale:
        print("cruise.py: STALE (run python3 tools/ship/cruise.py): " + ", ".join(stale))
        sys.exit(1)
    if check:
        print("cruise.py: check ok")


if __name__ == "__main__":
    main()
