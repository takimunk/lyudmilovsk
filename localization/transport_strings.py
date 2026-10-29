"""The cable-car metro's words (docs/transport.md), appended to Content/Localization/strings_<lang>.json in the same order
in all three tables. Run after a merge conflict in the strings to put them back:  python3 localization/transport_strings.py"""
import json
import os

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
LOC = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Localization")

ROWS = [
    # stations
    ("transport.station.aton", "Атон", "Aton", "アトン"),
    ("transport.station.academ", "Академгородок", "Akademgorodok", "アカデムゴロドク"),
    ("transport.station.zenit", "Зенит", "Zenith", "天頂"),
    ("transport.station.polar", "Полярная", "Polar Star", "北極星"),
    ("transport.station.mayak", "Маяк", "Lighthouse", "灯台"),
    ("transport.station.amber", "Янтарный берег", "Amber Shore", "琥珀海岸"),
    ("transport.station.horizon", "Горизонт", "Horizon", "地平線"),
    # lines
    ("transport.line.sun", "Солнечная линия", "Sun Line", "太陽線"),
    ("transport.line.sea", "Морская линия", "Sea Line", "海線"),
    # cabins: each its own bird (the sun line's are birds of the land, the sea line's of the sea)
    ("transport.cabin.c01", "Скворец", "Starling", "ムクドリ"),
    ("transport.cabin.c02", "Ласточка", "Swallow", "ツバメ"),
    ("transport.cabin.c03", "Стриж", "Swift", "アマツバメ"),
    ("transport.cabin.c04", "Зяблик", "Chaffinch", "ズアオアトリ"),
    ("transport.cabin.c05", "Иволга", "Oriole", "コウライウグイス"),
    ("transport.cabin.c06", "Чибис", "Lapwing", "タゲリ"),
    ("transport.cabin.c07", "Чайка", "Gull", "カモメ"),
    ("transport.cabin.c08", "Крачка", "Tern", "アジサシ"),
    ("transport.cabin.c09", "Баклан", "Cormorant", "ウ"),
    ("transport.cabin.c10", "Кулик", "Sandpiper", "シギ"),
    ("transport.cabin.c11", "Гага", "Eider", "ケワタガモ"),
    ("transport.cabin.c12", "Буревестник", "Petrel", "ミズナギドリ"),
    # the diagram
    ("transport.diagram.title", "Канатные дороги Людмиловска", "Lyudmilovsk Cable Cars", "リュドミロフスク・ロープウェイ"),
    ("transport.diagram.subtitle", "Схема линий · кабины ходят по вызову", "Route map · cabins run on call", "路線図・キャビンは呼び出し制"),
    ("transport.diagram.here", "Вы здесь", "You are here", "現在地"),
    ("transport.diagram.here_club", "Вы здесь · компьютерный клуб", "You are here · the computer club", "現在地・コンピュータークラブ"),
    ("transport.diagram.depot", "депо", "depot", "車庫"),
    ("transport.diagram.cabins", "кабины в пути", "cabins on the move", "運行中のキャビン"),
    ("transport.diagram.pick", "Выберите станцию: кабина приедет по вызову.", "Pick a station: a cabin will come for you.", "駅を選ぶと、キャビンが迎えに来ます。"),
    ("transport.diagram.look", "Наведите на станцию, чтобы узнать ожидание.", "Point at a station to see the wait.", "駅にカーソルを合わせると待ち時間が見られます。"),
    ("transport.diagram.called", "Кабина вызвана до станции «{0}». Ожидание: {1}.", "A cabin is called to {0}. Wait: {1}.", "{0}行きのキャビンを呼びました。待ち時間：{1}。"),
    ("transport.diagram.close", "E, Esc или щелчок вне схемы — закрыть", "E, Esc or a click outside the map: close", "E・Esc・図の外をクリック：閉じる"),
    # waits
    ("transport.wait.arriving", "прибывает", "arriving", "まもなく到着"),
    ("transport.wait.sec", "{0} с", "{0} s", "{0}秒"),
    ("transport.wait.min", "{0} мин", "{0} min", "{0}分"),
    ("transport.wait.called", "кабина через {0}", "cabin in {0}", "キャビンまで{0}"),
    ("transport.wait.on_call", "по вызову ≈ {0}", "on call ≈ {0}", "呼び出しで約{0}"),
    # boards and cabins
    ("transport.board.to", "→ {0}", "→ {0}", "→ {0}"),
    ("transport.cabin.to", "До станции «{0}»", "To {0}", "{0}行き"),
    ("transport.cabin.next", "Следующая: {0}", "Next: {0}", "次は{0}"),
    ("transport.cabin.doors", "Двери закрываются", "Doors closing", "ドアが閉まります"),
    ("transport.cabin.doors_open", "Двери открыты", "Doors open", "ドアが開いています"),
    ("transport.cabin.idle", "Ждёт пассажиров", "Waiting for passengers", "乗客待ち"),
    ("transport.cabin.how", "Вызов кабины — у табло на платформе", "Call a cabin at the platform's panel", "キャビンはホームの案内板で呼べます"),
    ("transport.cabin.depot", "В депо", "In the depot", "車庫"),
    ("transport.cabin.welcome", "Кабина «{0}» · до станции «{1}»", "Cabin {0} · to {1}", "キャビン「{0}」・{1}行き"),
    # the vestibule
    ("transport.vestibule.kiosk", "Касса", "Tickets", "きっぷ売り場"),
    # prompts
    ("transport.prompt.call", "E — схема и вызов кабины", "E: route map and call a cabin", "E：路線図・キャビンを呼ぶ"),
    ("transport.prompt.cabin_open", "E — открыть дверь кабины «{0}»", "E: open the door of cabin {0}", "E：キャビン「{0}」のドアを開ける"),
    ("transport.prompt.cabin_shut", "E — закрыть дверь кабины «{0}»", "E: shut the door of cabin {0}", "E：キャビン「{0}」のドアを閉める"),
    ("transport.prompt.seat_sit", "E — сесть", "E: sit down", "E：座る"),
    ("transport.prompt.seat_up", "E — встать", "E: stand up", "E：立つ"),
    ("transport.prompt.cabin_where", "E — куда ехать (схема)", "E: where to (route map)", "E：行き先（路線図）"),
    ("transport.prompt.board", "E — схема канатных дорог", "E: cable car route map", "E：ロープウェイ路線図"),
    ("transport.prompt.ride", "Кабина «{0}» → {1}", "Cabin {0} → {1}", "キャビン「{0}」→ {1}"),
    # announcements (spoken: localization/voice.json speaker "transport"; the same words as a note on screen)
    # the station halls' announcements (LyudStationFurnish.cpp: now and then while the player is in a hall)
    ("line.transport.hall_welcome.1", "Уважаемые пассажиры! Добро пожаловать на канатную дорогу Людмиловска. Посадка в кабины — по обе стороны платформы, у жёлтой линии.",
     "Dear passengers, welcome to the Lyudmilovsk cable car. Cabins board on both sides of the platform, at the yellow line.",
     "ご乗車のお客様、リュドミロフスク・ロープウェイへようこそ。キャビンへはホームの両側、黄色い線の内側からご乗車ください。"),
    ("line.transport.hall_cabin.1", "Внимание! Кабина подходит к станции. Пожалуйста, не заходите за жёлтую линию.",
     "Attention, please: a cabin is approaching the station. Please stay behind the yellow line.",
     "まもなくキャビンが到着します。黄色い線の内側でお待ちください。"),
    ("line.transport.hall_lift.1", "Лифт и лестница ведут на посадочную платформу. На лестнице, пожалуйста, держитесь за поручень.",
     "The lift and the stairs lead up to the boarding platform. Please hold the handrail on the stairs.",
     "エレベーターと階段は乗車ホームへ続いています。階段では手すりにおつかまりください。"),
    ("line.transport.next_aton.1", "Следующая станция — Атон. Компьютерный клуб.", "Next station: Aton. The computer club.", "次はアトン。コンピュータークラブ前です。"),
    ("line.transport.next_academ.1", "Следующая станция — Академгородок.", "Next station: Akademgorodok.", "次はアカデムゴロドクです。"),
    ("line.transport.next_zenit.1", "Следующая станция — Зенит. Переход на другую линию.", "Next station: Zenith. Change here for the other line.", "次は天頂。別の路線へお乗り換えいただけます。"),
    ("line.transport.next_polar.1", "Следующая станция — Полярная. Конечная.", "Next station: Polar Star, the end of the line.", "次は終点、北極星です。"),
    ("line.transport.next_mayak.1", "Следующая станция — Маяк. Конечная.", "Next station: Lighthouse, the end of the line.", "次は終点、灯台です。"),
    ("line.transport.next_amber.1", "Следующая станция — Янтарный берег. Выход к морю.", "Next station: Amber Shore. Exit to the sea.", "次は琥珀海岸。海へはこちらでお降りください。"),
    ("line.transport.next_horizon.1", "Следующая станция — Горизонт. Конечная.", "Next station: Horizon, the end of the line.", "次は終点、地平線です。"),
    ("line.transport.arrive_aton.1", "Атон.", "Aton.", "アトンです。"),
    ("line.transport.arrive_academ.1", "Академгородок.", "Akademgorodok.", "アカデムゴロドクです。"),
    ("line.transport.arrive_zenit.1", "Зенит.", "Zenith.", "天頂です。"),
    ("line.transport.arrive_polar.1", "Полярная. Просим освободить кабину.", "Polar Star. Please leave the cabin.", "北極星です。キャビンからお降りください。"),
    ("line.transport.arrive_mayak.1", "Маяк. Просим освободить кабину.", "Lighthouse. Please leave the cabin.", "灯台です。キャビンからお降りください。"),
    ("line.transport.arrive_amber.1", "Янтарный берег.", "Amber Shore.", "琥珀海岸です。"),
    ("line.transport.arrive_horizon.1", "Горизонт. Просим освободить кабину.", "Horizon. Please leave the cabin.", "地平線です。キャビンからお降りください。"),
]


def main():
    for i, lang in enumerate(("ru", "en", "ja")):
        p = os.path.join(LOC, "strings_%s.json" % lang)
        with open(p, encoding="utf-8") as f:
            d = json.load(f)
        for row in ROWS:
            d[row[0]] = row[1 + i]
        with open(p, "w", encoding="utf-8") as f:
            json.dump(d, f, ensure_ascii=False, indent=1)
            f.write("\n")
    print("transport strings: %d keys in ru/en/ja" % len(ROWS))


if __name__ == "__main__":
    main()
