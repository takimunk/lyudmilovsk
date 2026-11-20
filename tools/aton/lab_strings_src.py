"""The words of the ATON OS lab programs' interface (docs/aton_lab.md), Russian first. Writes tools/aton/strings/lab.json;
tools/aton/strings.py then builds the game's tables.   python3 tools/aton/lab_strings_src.py"""
import json
import os

S = {}


def t(key, ru, en, ja):
    assert key not in S, key
    S[key] = {"ru": ru, "en": en, "ja": ja}


t("ui.aton.lab.os", "АТОН ОС · ЛАБ-2", "ATON OS · LAB-2", "アトンOS・ラボ2")
t("ui.aton.lab.leave", "Отойти", "Step away", "離れる")
t("ui.aton.lab.prompt", "E — сесть за терминал", "E — use the terminal", "E：端末を使う")
t("ui.aton.app.session", "Сеанс", "Session", "セッション")
t("ui.aton.app.labmail", "Почта ЛАБ-2", "LAB-2 mail", "ラボ2メール")
t("ui.aton.app.adventure", "Ночная вахта", "Night Watch", "夜の当直")
t("ui.aton.app.chess", "Шахматы", "Chess", "チェス")
t("ui.aton.app.weather", "Метеостанция", "Weather station", "気象台")
t("ui.aton.app.scope", "Телескоп", "Telescope", "望遠鏡")
t("ui.aton.term.app.adventure", "вахта,вахту,ночная вахта", "watch,night watch", "当直,夜の当直")
t("ui.aton.term.app.chess", "шахматы", "chess", "チェス")
t("ui.aton.term.app.weather", "погода,погоду,метеостанция", "weather,meteo", "天気,気象台")

t("ui.aton.lab.session.title", "Лаборатория № 2", "Laboratory No. 2", "第2研究室")
t("ui.aton.lab.session.net", "Сеть Академгородка. Узел «Библиотека».", "Academgorodok network. Node: Library.", "アカデムゴロドク網。ノード「図書館」。")
t("ui.aton.lab.session.user_staff", "Вы вошли как дежурная смена.", "You are logged in as the duty shift.", "当直班としてログイン中です。")
t("ui.aton.lab.session.open_staff", "Открыты почта смены и очередь на телескоп.", "The shift's mail and the telescope queue are open.", "当直班のメールと望遠鏡の予約が使えます。")
t("ui.aton.lab.session.to_mail", "Открыть почту", "Open the mail", "メールを開く")
t("ui.aton.lab.session.logout", "Выйти из смены", "Log out of the shift", "当直班からログアウト")
t("ui.aton.lab.session.user_guest", "Вы вошли как гость.", "You are logged in as a guest.", "ゲストとしてログイン中です。")
t("ui.aton.lab.session.open_guest", "Гостю открыты метеостанция, игры и общая почта. Остальное открывается паролем дежурной смены.",
  "A guest has the weather station, the games and the common mail. The rest opens with the duty shift's password.",
  "ゲストは気象台、ゲーム、共用メールを使えます。ほかは当直班のパスワードで開きます。")
t("ui.aton.lab.session.password", "Пароль смены (четыре цифры)", "Shift password (four digits)", "当直班のパスワード（数字4桁）")
t("ui.aton.lab.session.login", "Войти", "Log in", "ログイン")
t("ui.aton.lab.session.wrong", "Пароль не подошёл.", "Wrong password.", "パスワードが違います。")
t("ui.aton.lab.session.wrong_hint", "Пароль не подошёл. Если вы его забыли, посмотрите общую почту: вы не первый.",
  "Wrong password. If you have forgotten it, look in the common mail: you are not the first.",
  "パスワードが違います。忘れた方は共用メールをご覧ください。あなたが最初ではありません。")

t("ui.aton.lab.mail.box_guest", "Общая почта", "Common mail", "共用メール")
t("ui.aton.lab.mail.box_staff", "Дежурная смена", "Duty shift", "当直班")
t("ui.aton.lab.mail.locked", "Нужен пароль смены.", "Needs the shift password.", "当直班のパスワードが必要です。")
t("ui.aton.lab.mail.to_line", "Кому: {0}", "To: {0}", "宛先：{0}")

t("ui.aton.advui.hint", "Выберите строку мышью.", "Pick a line with the mouse.", "行をマウスで選んでください。")
t("ui.aton.advui.broken", "Лента с игрой не читается.", "The game tape cannot be read.", "ゲームのテープが読めません。")
t("ui.aton.advui.intro", "НОЧНАЯ ВАХТА. Игра сотрудников Лаборатории № 2.", "NIGHT WATCH. A game by the staff of Laboratory No. 2.", "夜の当直。第2研究室の職員によるゲーム。")
t("ui.aton.advui.resumed", "(Вахта продолжается. Ход {0}.)", "(The watch goes on. Move {0}.)", "（当直は続く。{0}手目。）")
t("ui.aton.advui.ending", "Концовка: {0}", "Ending: {0}", "結末：{0}")
t("ui.aton.advui.found", "Найдено концовок: {0} из {1}", "Endings found: {0} of {1}", "見つけた結末：{1}のうち{0}")
t("ui.aton.advui.again", "Заступить на вахту снова", "Take the watch again", "もう一度当直に就く")

t("ui.aton.chess.resumed", "Партия продолжается, ход {0}.", "The game goes on, move {0}.", "対局は続きます。{0}手目。")
t("ui.aton.chess.level", "Уровень", "Level", "レベル")
t("ui.aton.chess.new", "Новая партия", "New game", "新しい対局")
t("ui.aton.chess.undo", "Взять ход назад", "Take back", "待った")
t("ui.aton.chess.won", "Мат. Вы победили.", "Checkmate. You won.", "チェックメイト。あなたの勝ちです。")
t("ui.aton.chess.lost", "Мат. Машина победила.", "Checkmate. The machine won.", "チェックメイト。機械の勝ちです。")
t("ui.aton.chess.stalemate", "Пат. Ничья.", "Stalemate. A draw.", "ステイルメイト。引き分けです。")
t("ui.aton.chess.draw", "Ничья.", "A draw.", "引き分けです。")
t("ui.aton.chess.thinking", "Машина думает…", "The machine is thinking…", "機械が考えています…")
t("ui.aton.chess.your_move", "Ваш ход. Вы играете белыми.", "Your move. You play white.", "あなたの番です。あなたは白です。")
t("ui.aton.chess.your_move_check", "Шах. Ваш ход.", "Check. Your move.", "チェック。あなたの番です。")
t("ui.aton.chess.score", "Уровень {0}: побед {1}, ничьих {2}, поражений {3}", "Level {0}: won {1}, drawn {2}, lost {3}", "レベル{0}：{1}勝 {2}分 {3}敗")

t("ui.aton.wx.title", "Метеостанция ЛАБ-2. Мачта на крыше.", "LAB-2 weather station. The mast on the roof.", "ラボ2気象台。屋上のマスト。")
t("ui.aton.wx.offline", "Мачта не отвечает.", "The mast does not answer.", "マストが応答しません。")
t("ui.aton.wx.at", "Показания на {0}, {1}", "Readings for {0}, {1}", "{0} {1} の観測値")
t("ui.aton.wx.temp", "Температура воздуха", "Air temperature", "気温")
t("ui.aton.wx.dew", "Точка росы", "Dew point", "露点")
t("ui.aton.wx.humidity", "Влажность", "Humidity", "湿度")
t("ui.aton.wx.pressure", "Давление", "Pressure", "気圧")
t("ui.aton.wx.wind", "Ветер", "Wind", "風")
t("ui.aton.wx.cloud", "Облачность", "Cloud cover", "雲量")
t("ui.aton.wx.precip", "Осадки", "Precipitation", "降水")
t("ui.aton.wx.visibility", "Видимость", "Visibility", "視程")
t("ui.aton.wx.sea", "Вода в море", "Sea water", "海水温")
t("ui.aton.wx.snow", "Снег", "Snow depth", "積雪")
t("ui.aton.wx.flux", "Поток от Солнца", "Flux from the Sun", "太陽からの放射")
t("ui.aton.wx.kp", "Геомагнитный индекс Kp", "Geomagnetic index Kp", "地磁気指数Kp")
t("ui.aton.wx.deg", "{0} °C", "{0} °C", "{0} °C")
t("ui.aton.wx.rising", "растёт", "rising", "上昇")
t("ui.aton.wx.falling", "падает", "falling", "下降")
t("ui.aton.wx.steady", "ровно", "steady", "安定")
t("ui.aton.wx.hpa", "{0} гПа, {1}", "{0} hPa, {1}", "{0} hPa、{1}")
t("ui.aton.wx.wind_value", "{0}, {1} м/с, {2} б.", "{0}, {1} m/s, force {2}", "{0}、{1} m/s、風力{2}")
for i, (ru, en, ja) in enumerate([("северный", "north", "北"), ("северо-восточный", "north-east", "北東"), ("восточный", "east", "東"),
                                  ("юго-восточный", "south-east", "南東"), ("южный", "south", "南"), ("юго-западный", "south-west", "南西"),
                                  ("западный", "west", "西"), ("северо-западный", "north-west", "北西")]):
    t("ui.aton.wx.dir.%d" % i, ru, en, ja)
t("ui.aton.wx.oktas", "{0} из 8", "{0} of 8", "8分の{0}")
for i, (ru, en, ja) in enumerate([("нет", "none", "なし"), ("морось", "drizzle", "霧雨"), ("дождь", "rain", "雨"), ("ливень", "showers", "にわか雨"),
                                  ("гроза", "thunderstorm", "雷雨"), ("град", "hail", "雹"), ("снежная крупа", "graupel", "霰"),
                                  ("мокрый снег", "sleet", "みぞれ"), ("снег", "snow", "雪"), ("снежные заряды", "snow showers", "にわか雪"),
                                  ("ледяная морось", "freezing drizzle", "着氷性の霧雨"), ("ледяной дождь", "freezing rain", "着氷性の雨")]):
    t("ui.aton.wx.precip.%d" % i, ru, en, ja)
t("ui.aton.wx.precip_rate", "{0}, {1} мм/ч", "{0}, {1} mm/h", "{0}、{1} mm/h")
t("ui.aton.wx.km", "{0} км", "{0} km", "{0} km")
t("ui.aton.wx.m", "{0} м", "{0} m", "{0} m")
t("ui.aton.wx.cm", "{0} см", "{0} cm", "{0} cm")
t("ui.aton.wx.wm2", "{0} Вт/м²", "{0} W/m²", "{0} W/m²")
t("ui.aton.wx.forecast", "Прогноз", "Forecast", "予報")
t("ui.aton.wx.forecast_line", "{0} °C, осадки: {1}, облака {2} из 8", "{0} °C, precipitation: {1}, cloud {2} of 8", "{0} °C、降水：{1}、雲量8分の{2}")
t("ui.aton.wx.footer", "Показания идут как есть. За погоду лаборатория не отвечает.", "Readings are given as they are. The laboratory is not responsible for the weather.",
  "観測値はそのままお伝えします。天気について研究室は責任を負いません。")

t("ui.aton.scope.title", "Очередь на телескоп", "Telescope queue", "望遠鏡の予約")
t("ui.aton.scope.locked", "Очередь на телескоп ведёт дежурная смена. Нужен пароль смены.", "The telescope queue is kept by the duty shift. It needs the shift password.",
  "望遠鏡の予約は当直班が管理しています。当直班のパスワードが必要です。")
t("ui.aton.scope.rules", "Четыре окна за ночь. За вами может быть одно окно. Наблюдение идёт само; запись появится в журнале, если небо было чистым.",
  "Four slots a night. One slot may be yours. The observation runs by itself; the record appears in the journal if the sky was clear.",
  "一晩に四枠。あなたの枠は一つまで。観測は自動で行われ、空が晴れていれば記録が日誌に載ります。")
t("ui.aton.scope.night", "Ночь на {0}", "Night to {0}", "{0} にかけての夜")
t("ui.aton.scope.target.moon", "Луна", "The Moon", "月")
t("ui.aton.scope.target.evening_star", "Вечерняя звезда", "The evening star", "宵の明星")
t("ui.aton.scope.target.giant", "Планета-гигант", "The giant planet", "巨大惑星")
t("ui.aton.scope.target.rings", "Планета с кольцами", "The ringed planet", "環のある惑星")
t("ui.aton.scope.target.double", "Двойная звезда", "The double star", "二重星")
t("ui.aton.scope.target.field14", "Поле № 14", "Field No. 14", "第14区画")
t("ui.aton.scope.yours", "за вами", "yours", "あなたの枠")
t("ui.aton.scope.unbook", "Отказаться", "Give up", "取り消す")
t("ui.aton.scope.staff.1", "Н. Оямада", "N. Oyamada", "N・小山田")
t("ui.aton.scope.staff.2", "Л. Абебе", "L. Abebe", "L・アベベ")
t("ui.aton.scope.staff.3", "Т. Ильвес", "T. Ilves", "T・イルヴェス")
t("ui.aton.scope.staff.4", "М. Керимова", "M. Kerimova", "M・ケリモワ")
t("ui.aton.scope.staff.5", "Р. Халворсен", "R. Halvorsen", "R・ハルヴォルセン")
t("ui.aton.scope.unused", "прошло пустым", "passed unused", "使われずに過ぎた")
t("ui.aton.scope.free", "свободно", "free", "空き")
t("ui.aton.scope.book", "Записаться", "Book", "予約する")
t("ui.aton.scope.journal", "Журнал ваших наблюдений", "Journal of your observations", "あなたの観測日誌")
t("ui.aton.scope.journal_empty", "Пока пусто.", "Empty so far.", "まだ何もありません。")
t("ui.aton.scope.result.moon", "Луна. Терминатор резкий, тени в кратерах длинные. Всё на своих местах, и от этого спокойно.",
  "The Moon. The terminator is sharp, the crater shadows are long. Everything is in its place, and that is calming.",
  "月。明暗の境はくっきりして、クレーターの影が長い。すべてがあるべき場所にあって、それで落ち着く。")
t("ui.aton.scope.result.evening_star", "Вечерняя звезда. Узкий серп, очень яркий. Автомат трижды уменьшал выдержку.",
  "The evening star. A thin crescent, very bright. The automat cut the exposure three times.",
  "宵の明星。細い三日月形で、とても明るい。自動装置が露出を三度下げた。")
t("ui.aton.scope.result.giant", "Планета-гигант. Полосы видны, спутников насчитано четыре. Один из них отмечен в каталоге значком, без пояснения.",
  "The giant planet. The belts are visible, four moons counted. One of them carries a mark in the catalogue, with no explanation.",
  "巨大惑星。縞が見え、衛星は四つ数えられた。そのうち一つにはカタログで印が付いているが、説明はない。")
t("ui.aton.scope.result.rings", "Планета с кольцами. Кольца раскрыты широко. Автомат приписал: «красиво», хотя такого слова в его словаре нет.",
  "The ringed planet. The rings are wide open. The automat added: \"beautiful\", though that word is not in its dictionary.",
  "環のある惑星。環は大きく開いている。自動装置は「美しい」と書き添えた。その語は辞書にないはずなのに。")
t("ui.aton.scope.result.double", "Двойная звезда. Разделена уверенно: золотая и голубая. Расстояние между ними совпало с каталогом до последнего знака.",
  "The double star. Split cleanly: a gold one and a blue one. Their separation matches the catalogue to the last digit.",
  "二重星。はっきり分離できた。金色と青。離角はカタログと最後の桁まで一致した。")
t("ui.aton.scope.result.field14", "Поле № 14. Звёзд в кадре столько же, сколько в прошлый раз. И в позапрошлый. Ни одной больше, ни одной меньше.",
  "Field No. 14. As many stars in the frame as last time. And the time before. Not one more, not one fewer.",
  "第14区画。写った星の数は前回と同じ。その前とも同じ。一つも多くなく、一つも少なくない。")
t("ui.aton.scope.cloudy.1", "Облака. Купол не открывался.", "Cloud. The dome stayed shut.", "曇り。ドームは開かなかった。")
t("ui.aton.scope.cloudy.2", "Небо закрыто. Автомат подождал до рассвета и уснул.", "The sky was closed. The automat waited until dawn and went to sleep.", "空は閉ざされていた。自動装置は夜明けまで待って眠った。")

t("dev.group.aton", "АТОН ОС", "ATON OS", "アトンOS")
t("dev.action.aton.lab_profile", "Клубный компьютер: сеть ЛАБ-2 / обычный", "Club computer: LAB-2 network / normal", "クラブのPC：ラボ2網／通常")
t("dev.action.aton.lab_login", "ЛАБ-2: войти дежурной сменой", "LAB-2: log in as the duty shift", "ラボ2：当直班でログイン")
t("dev.action.aton.lab_reset", "ЛАБ-2: сбросить всё", "LAB-2: reset everything", "ラボ2：すべてリセット")
t("dev.tp.aton_terminal", "Терминал ЛАБ-2 в библиотеке", "LAB-2 terminal in the library", "図書館のラボ2端末")

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "strings", "lab.json")
with open(out, "w", encoding="utf-8") as f:
    json.dump(S, f, ensure_ascii=False, indent=1, sort_keys=True)
    f.write("\n")
print("lab.json: %d keys" % len(S))
