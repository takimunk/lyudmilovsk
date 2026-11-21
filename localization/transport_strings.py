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
    ("transport.prompt.call", "E — схема и вызов кабины", "E — route map and call a cabin", "E：路線図・キャビンを呼ぶ"),
    ("transport.prompt.cabin_open", "E — открыть дверь кабины «{0}»", "E — open the door of cabin {0}", "E：キャビン「{0}」のドアを開ける"),
    ("transport.prompt.cabin_shut", "E — закрыть дверь кабины «{0}»", "E — shut the door of cabin {0}", "E：キャビン「{0}」のドアを閉める"),
    ("transport.prompt.seat_sit", "E — сесть", "E — sit down", "E：座る"),
    ("transport.prompt.seat_up", "E — встать", "E — stand up", "E：立つ"),
    ("transport.prompt.cabin_where", "E — куда ехать (схема)", "E — where to (route map)", "E：行き先（路線図）"),
    ("transport.prompt.board", "E — схема канатных дорог", "E — cable car route map", "E：ロープウェイ路線図"),
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
    # halls v3 (docs/transport.md "Halls v3"): the buffet, the attendant, their greetings
    ("ui.shop.title_station_buffet", "Буфет станции «{0}»", "Station buffet, {0}", "{0}駅のビュッフェ"),
    ("transport.hall.attendant_prompt", "E — спросить дежурную по станции", "E — ask the station attendant", "E：駅員にたずねる"),
    ("line.transport.barista.hello.1", "Здравствуйте! Кофе, чай, пирожки — всё свежее.", "Hello! Coffee, tea, pies, all fresh.", "いらっしゃいませ！コーヒー、紅茶、ピロシキ、全部できたてですよ。"),
    ("line.transport.barista.hello.2", "Заходите, пока кабина едет, — успеете выпить чаю.", "Come in, you've time for a tea before the cabin comes.", "キャビンが来るまでにお茶を一杯どうぞ。"),
    ("line.transport.barista.hello.3", "Пирожки с капустой только из печи.", "Cabbage pies, straight from the oven.", "キャベツのピロシキ、焼きたてです。"),
    ("line.transport.attendant.hello.1", "Добрый день. Кабины ходят по вызову, кнопка у перрона.", "Good day. Cabins come when called: the button is by the platform.", "こんにちは。キャビンは呼び出し制です。ボタンはホームの横にあります。"),
    ("line.transport.attendant.hello.2", "Не стойте у края, пожалуйста.", "Please keep back from the edge.", "端に近づかないでください。"),
    ("line.transport.attendant.hello.3", "Если что-то нужно — спрашивайте.", "If you need anything, just ask.", "何かあればお声がけください。"),
    ("line.transport.attendant.help.1", "Вот схема линий. Мы здесь, где огонёк.", "Here's the route map. We're where the light is.", "こちらが路線図です。光っているところが現在地です。"),
    ("line.transport.attendant.help.2", "Пересадка на другую линию — на Зените.", "Change for the other line at Zenith.", "別の路線へのお乗り換えは天頂駅です。"),
    ("line.transport.attendant.help.3", "К морю — до Янтарного берега, оттуда пешком.", "For the sea, ride to Amber Shore and walk from there.", "海へは琥珀海岸駅で降りて、そこから歩いてください。"),
    # station life (docs/transport.md "Station life"): the wind stop in the cabin, the view on each hop, the halls' words
    ("line.transport.wind_stop.1", "Внимание! Из-за сильного ветра движение кабин приостановлено. Кабина стоит на станции, двери открыты. Пожалуйста, ожидайте.", "Attention: cabins are stopped because of strong wind. This cabin will wait at the station with its doors open. Please wait.", "お知らせします。強風のため、キャビンの運転を見合わせています。このキャビンはドアを開けたまま駅で待機します。しばらくお待ちください。"),
    ("line.transport.wind_go.1", "Ветер стих. Движение кабин возобновляется. Приятной поездки!", "The wind has dropped and the cabins are running again. Have a pleasant ride!", "風がおさまりました。運転を再開します。どうぞ快適な旅を！"),
    ("line.transport.view_aton_academ.1", "Слева внизу — Людмиловская бухта. Впереди — крыши Академгородка.", "Down on the left is Lyudmilovsk Bay. Ahead are the roofs of Akademgorodok.", "左手の下にリュドミロフスク湾。前方にアカデムゴロドクの屋根が見えます。"),
    ("line.transport.view_academ_aton.1", "Справа — Людмиловская бухта. Впереди, на холме, — компьютерный клуб «Атон».", "On the right is Lyudmilovsk Bay. Ahead on the hill is the Aton computer club.", "右手にリュドミロフスク湾。前方の丘の上にコンピュータークラブ「アトン」があります。"),
    ("line.transport.view_academ_zenit.1", "Слева — бухта. Под нами — террасы города.", "On the left is the bay. Below us are the town's terraces.", "左手に湾。眼下に町の段々が広がります。"),
    ("line.transport.view_zenit_academ.1", "Справа — бухта. Внизу — Академгородок и его институты.", "On the right is the bay. Below is Akademgorodok with its institutes.", "右手に湾。眼下にアカデムゴロドクと研究所があります。"),
    ("line.transport.view_zenit_polar.1", "Слева — открытое море. Впереди — северные холмы.", "On the left is the open sea. Ahead are the northern hills.", "左手に外海。前方に北の丘が見えます。"),
    ("line.transport.view_polar_zenit.1", "Справа — море. Впереди — город на террасах.", "On the right is the sea. Ahead is the town on its terraces.", "右手に海。前方に段々の町が見えます。"),
    ("line.transport.view_mayak_amber.1", "Позади — маяк на обрыве. Слева — пляж и море.", "Behind us is the lighthouse on the cliff. On the left are the beach and the sea.", "後ろに崖の上の灯台。左手に砂浜と海が見えます。"),
    ("line.transport.view_amber_mayak.1", "Справа — море. Впереди — маяк на обрыве.", "On the right is the sea. Ahead is the lighthouse on the cliff.", "右手に海。前方に崖の上の灯台が見えます。"),
    ("line.transport.view_amber_zenit.1", "Под нами — набережная и пляж Янтарного берега. Впереди — город на террасах.", "Below us are the promenade and the beach of Amber Shore. Ahead is the town on its terraces.", "眼下に琥珀海岸の遊歩道と砂浜。前方に段々の町が見えます。"),
    ("line.transport.view_zenit_amber.1", "Впереди — море и Янтарный берег.", "Ahead are the sea and Amber Shore.", "前方に海と琥珀海岸が見えます。"),
    ("line.transport.view_zenit_horizon.1", "Позади — море. Впереди — холмы и лес у Горизонта.", "The sea is behind us. Ahead are the hills and the forest by Horizon.", "後ろに海。前方に地平線駅のそばの丘と森が見えます。"),
    ("line.transport.view_horizon_zenit.1", "Впереди — море и весь город на террасах.", "Ahead are the sea and the whole town on its terraces.", "前方に海と段々の町全体が見えます。"),
    ("line.transport.wait.any.1", "Давно ждёте?", "Have you been waiting long?", "長く待ってますか？"),
    ("line.transport.wait.any.2", "Кабинка по вызову, кто-нибудь нажал кнопку?", "They come when called. Has anyone pressed the button?", "呼び出し制ですよね。誰かボタン押しました？"),
    ("line.transport.wait.any.3", "Каждый день так езжу, уже привычно.", "I ride like this every day. I'm used to it.", "毎日これで通ってるから、もう慣れました。"),
    ("line.transport.wait.any.4", "Пирожок бы взять, пока ждём.", "I could get a pie while we wait.", "待ってる間にピロシキでも買おうかな。"),
    ("line.transport.wait.wind.1", "Опять ветер, кабинки встанут?", "Windy again. Will the cabins stop?", "また風だ。キャビン止まるかな？"),
    ("line.transport.wait.wind.2", "Чувствуете, как качает?", "Can you feel it swaying?", "揺れてるの、わかります？"),
    ("line.transport.wait.wind.3", "Если ещё поднимется, линию остановят.", "If it gets any stronger, they'll stop the line.", "これ以上強くなったら、運転止めますよ。"),
    ("line.transport.wait.wind.4", "В такой ветер я бы лучше пешком.", "In wind like this I'd rather walk.", "こんな風なら歩いた方がいいかも。"),
    ("line.transport.wait.stopped.1", "Ну вот, встали. Ветер.", "There, they've stopped. The wind.", "ほら、止まった。風のせいだ。"),
    ("line.transport.wait.stopped.2", "Говорят, переждать надо минут двадцать.", "They say we'll have to wait twenty minutes or so.", "二十分くらい待つしかないらしいよ。"),
    ("line.transport.wait.stopped.3", "Хорошо хоть в буфете чай есть.", "At least the buffet has tea.", "せめて売店にお茶があってよかった。"),
    ("line.transport.wait.stopped.4", "Как стихнет — поедем. Дежурная сказала.", "We'll go once it drops. The attendant said so.", "風がやんだら動くって、駅員さんが言ってた。"),
    ("line.transport.wait.morning.1", "Только бы на работу не опоздать.", "I just hope I'm not late for work.", "仕事に遅れないといいけど。"),
    ("line.transport.wait.morning.2", "Утром всегда народ.", "It's always busy in the mornings.", "朝はいつも混むね。"),
    ("line.transport.wait.morning.3", "Доброе утро! Вы тоже в Академгородок?", "Good morning! Going to Akademgorodok too?", "おはようございます！あなたもアカデムゴロドクへ？"),
    ("line.transport.wait.morning.4", "Кофе бы ещё один.", "I could do with another coffee.", "もう一杯コーヒーが欲しいな。"),
    ("line.transport.wait.evening.1", "Наконец домой.", "Home at last.", "やっと家に帰れる。"),
    ("line.transport.wait.evening.2", "Кабинки ходят до полуночи, успеем.", "The cabins run till midnight. We'll make it.", "キャビンは夜中まで動いてるから、間に合うよ。"),
    ("line.transport.wait.evening.3", "Какой длинный день.", "What a long day.", "長い一日だった。"),
    ("line.transport.wait.evening.4", "Вечером тут тихо, хорошо.", "It's nice and quiet here in the evening.", "夜はここ、静かでいいね。"),
    ("line.transport.attendant.regular.1", "О, опять вы! Как всегда, в Академгородок?", "Oh, you again! Akademgorodok, as usual?", "あ、またいらっしゃいましたね！いつものアカデムゴロドクですか？"),
    ("line.transport.attendant.regular.2", "Доброе утро! Кабинка будет через пару минут.", "Good morning! Your cabin will be here in a couple of minutes.", "おはようございます！キャビンはあと二、三分で来ますよ。"),
    ("line.transport.attendant.regular.3", "Как здоровье? Давно вас не было видно.", "How are you keeping? We haven't seen you for a while.", "お元気でしたか？しばらくお見かけしませんでしたね。"),
    ("line.transport.attendant.regular.4", "Сегодня ветрено, держитесь за поручень.", "It's windy today. Hold on to the rail.", "今日は風が強いので、手すりにおつかまりください。"),
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
