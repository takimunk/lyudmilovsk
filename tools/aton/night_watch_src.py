# -*- coding: utf-8 -*-
# Scratch generator of the lab adventure «Ночная вахта»: writes Content/AtonOS/adventure.json and tools/aton/strings/adventure.json.
import json, sys, collections
import os
ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
S = collections.OrderedDict()
NODES = collections.OrderedDict()
cur = [None, 0, 0, 0]

def put(key, ru, en, ja):
    assert key not in S, key
    S[key] = {"ru": ru, "en": en, "ja": ja}
    return key

def node(nid, set=None):
    cur[0] = nid; cur[1] = cur[2] = cur[3] = 0
    NODES[nid] = {"text": [], "choices": []}
    if set: NODES[nid]["set"] = set

def line(kind, ru, en, ja, cond=None):
    idx = {"t": 1, "c": 2, "s": 3}[kind]
    cur[idx] += 1
    key = "ui.aton.adv.%s.%s%d" % (cur[0], "" if kind == "t" else kind, cur[idx])
    put(key, ru, en, ja)
    return {"k": key, "if": cond} if cond else key

def t(ru, en, ja, cond=None):
    NODES[cur[0]]["text"].append(line("t", ru, en, ja, cond))

def say(ru, en, ja, cond=None):
    return ("say", ru, en, ja, cond)

def c(ru, en, ja, cond=None, says=(), set=None, go=None, end=None):
    ch = {"text": line("c", ru, en, ja)}
    if cond: ch["if"] = cond
    if says: ch["say"] = [line("s", *s[1:]) for s in says]
    if set: ch["set"] = set
    if go: ch["go"] = go
    if end: ch["end"] = end
    NODES[cur[0]]["choices"].append(ch)

put("ui.aton.adv.title", "НОЧНАЯ ВАХТА", "NIGHT WATCH", "夜の当直")
put("ui.aton.adv.end.dawn", "Утро", "Morning", "朝")
put("ui.aton.adv.end.keeper", "Вахту сдал, вахту принял", "Watch handed over, watch taken", "当直を渡し、当直を受ける")
put("ui.aton.adv.end.dark", "Темно", "Dark", "暗がり")

ALL = "knows_hand&heard&saw_edge&tea_done"
NOT_ALL = "!knows_hand|!heard|!saw_edge|!tea_done"

# ------------------------------------------------------------------ start
node("start")
t("НОЧНАЯ ВАХТА. Игра для одного дежурного и одной лампы. Написана в ночные смены, проверена там же.",
  "NIGHT WATCH. A game for one keeper and one lamp. Written on night shifts and tested on them too.",
  "夜の当直。当直ひとりとランプひとつのためのゲーム。夜勤中に書かれ、夜勤中に動作確認された。")
t("Вечер. Ты принимаешь вахту на посту «Фонарь»: каменная башня на берегу, наверху одна большая лампа. Задача простая — чтобы она горела до утра. Сменщика ты не застаёшь. Его никто никогда не застаёт.",
  "Evening. You take over the watch at the Lantern post: a stone tower on the shore with one great lamp at the top. The job is simple: it must burn until morning. You don't catch the day keeper. Nobody ever does.",
  "夕方。きみは「灯」の番所で当直を引き継ぐ。岸辺の石の塔、てっぺんに大きなランプがひとつ。仕事は単純で、朝まで灯しておくこと。昼の番人には会えない。会えた者はいない。")
t("На столе записка: «Лампа капризничает. Манометру не верь. Чай в жестянке. Если постучат — открой. Д.»",
  "A note on the desk: \"The lamp is being difficult. Don't trust the gauge. Tea is in the tin. If someone knocks, open. D.\"",
  "机にメモ。「ランプは気まぐれ。圧力計は信じるな。お茶は缶の中。ノックがあったら開けること。D」")
c("Сесть за стол", "Sit down at the desk", "机につく", go="watch")

# ------------------------------------------------------------------ watch room (hub)
node("watch")
t("Вахтенная комната. Стол с журналом, радио, керосинка с чайником. На стене гвоздь. Дверь наружу, лестница наверх, люк в подвал.",
  "The watch room. A desk with the log book, a radio, a paraffin stove with a kettle. A nail in the wall. A door outside, stairs up, a hatch to the cellar.",
  "当直室。日誌ののった机、無線機、やかんをのせた石油こんろ。壁に釘が一本。外への扉、上への階段、地下への揚げ戸。")
t("На гвозде висит железная штука с табличкой: «НЕ КОЛЕСО. Не трогать. Д.»",
  "On the nail hangs an iron thing with a tag: \"NOT A WHEEL. Do not touch. D.\"",
  "釘には鉄の何かが掛かっていて、札がついている。「車輪ではない。さわるな。D」", "!has_wheel")
t("Свет сверху, от лампы, идёт неровно. Или не идёт вовсе: отсюда не разобрать.",
  "The light from the lamp upstairs comes unevenly. Or doesn't come at all: hard to tell from here.",
  "上のランプからの光はむらがある。あるいは、まったく来ていない。ここからでは分からない。", "!lit")
t("По потолку ровно, раз в оборот, проходит свет лампы.",
  "The lamp's light crosses the ceiling evenly, once a turn.",
  "天井をランプの光が一回りごとに、きちんと横切っていく。", "lit")
t("В дверь стучат. Три раза, вежливо. Потом ещё три, так же вежливо.",
  "Someone is knocking at the door. Three times, politely. Then three more, just as politely.",
  "扉をたたく音がする。三回、ていねいに。それからまた三回、同じようにていねいに。", "lit&!opened")
c("Открыть журнал", "Open the log book", "日誌を開く", go="log")
c("Сесть к радио", "Sit down at the radio", "無線機の前にすわる", go="radio")
c("Подойти к чайнику", "Go to the kettle", "やかんのところへ行く", go="kettle")
c("Выдвинуть ящик стола", "Pull out the desk drawer", "机の引き出しを開ける", go="drawer")
c("Снять с гвоздя «не колесо»", "Take the \"not a wheel\" off the nail", "釘から「車輪ではない」を取る", "!has_wheel",
  says=[say("Снимаешь. Тяжёлое, круглое, со спицами и квадратной дырой посередине. Табличка права в одном: само по себе оно никуда не катится.",
            "You take it down. Heavy, round, with spokes and a square hole in the middle. The tag is right about one thing: on its own it rolls nowhere.",
            "外す。重くて丸く、輻があり、真ん中に四角い穴。札はひとつだけ正しい。これは、ひとりではどこへも転がらない。")],
  set=["has_wheel"])
c("Подняться по лестнице", "Go up the stairs", "階段を上がる", go="stairs")
c("Спуститься в подвал", "Go down to the cellar", "地下へ下りる", go="cellar")
c("Подойти к двери", "Go to the door", "扉のところへ行く", go="door")
c("Вздремнуть за столом", "Take a nap at the desk", "机でひと眠りする", "!lit", go="sleep")
c("Ждать утра", "Wait for morning", "朝を待つ", "lit", go="predawn",
  says=[say("Ты сидишь. Свет проходит по потолку — оборот, ещё оборот. Где-то на двухсотом ты перестаёшь считать, а где-то после этого потолок начинает сереть сам.",
            "You sit. The light crosses the ceiling: a turn, another turn. Around the two hundredth you stop counting, and some time after that the ceiling starts turning grey by itself.",
            "すわっている。光が天井を横切る。一回り、また一回り。二百回目あたりで数えるのをやめ、そのしばらくあとで、天井がひとりでに白みはじめる。")])

# ------------------------------------------------------------------ drawer
node("drawer")
t("В ящике: огрызок карандаша, спичечный коробок, жестянка с чаем и резинка, которая уже ничего не держит.",
  "In the drawer: a pencil stub, a matchbox, the tea tin and a rubber band that no longer holds anything.",
  "引き出しの中。ちびた鉛筆、マッチ箱、お茶の缶、それに、もう何も留めていない輪ゴム。", "!has_pencil")
t("В ящике: спичечный коробок, жестянка с чаем и резинка, которая уже ничего не держит.",
  "In the drawer: a matchbox, the tea tin and a rubber band that no longer holds anything.",
  "引き出しの中。マッチ箱、お茶の缶、それに、もう何も留めていない輪ゴム。", "has_pencil")
c("Взять карандаш", "Take the pencil", "鉛筆を取る", "!has_pencil",
  says=[say("Карандаш сточен с обоих концов. Кто-то очень много писал. Или очень много грыз.",
            "The pencil is worn down at both ends. Somebody wrote a great deal. Or chewed a great deal.",
            "鉛筆は両端からすり減っている。だれかがたくさん書いたのだ。あるいは、たくさんかじった。")],
  set=["has_pencil"])
c("Потрясти коробок", "Shake the matchbox", "マッチ箱を振る",
  says=[say("Гремит одна спичка. Открываешь: горелая. Её аккуратно положили обратно, как кладут вещь, которая ещё пригодится.",
            "One match rattles. You open it: burnt. It was put back carefully, the way you put back a thing that will still come in useful.",
            "一本だけ音がする。開けてみると、燃えさしだ。まだ使うものをしまうように、きちんと戻してある。")])
c("Открыть жестянку с чаем", "Open the tea tin", "お茶の缶を開ける",
  says=[say("Чай чёрный, крупный, пахнет дымом и сухой травой с солнечной стороны. На дне жестянки нацарапано: «на двоих».",
            "The tea is black, coarse, and smells of smoke and of dry grass from the sunny side. Scratched on the bottom of the tin: \"for two\".",
            "お茶は黒くて葉が大きく、煙と、日なたの干し草のにおいがする。缶の底に引っかき字で「二人ぶん」。")])
c("Задвинуть ящик", "Push the drawer shut", "引き出しを閉める", go="watch")

# ------------------------------------------------------------------ log book
node("log")
t("Вахтенный журнал. Толстый, в клеёнке, углы объедены. Между страниц торчит что-то тонкое — закладка.",
  "The log book. Thick, bound in oilcloth, the corners nibbled. Something thin sticks out between the pages: a bookmark.",
  "当直日誌。分厚く、油布の表紙で、角はかじられている。ページのあいだから細いものがのぞいている。しおりだ。", "!took_match")
t("Вахтенный журнал. Толстый, в клеёнке, углы объедены.",
  "The log book. Thick, bound in oilcloth, the corners nibbled.",
  "当直日誌。分厚く、油布の表紙で、角はかじられている。", "took_match")
t("У корешка торчит бахрома от вырванной страницы.",
  "At the spine, the fringe of a torn-out page sticks up.",
  "のどのところに、破り取られたページの端がけば立っている。", "tore")
c("Листать с начала", "Leaf through from the start", "はじめからめくる", go="log_old")
c("Открыть на закладке", "Open at the bookmark", "しおりのところを開く", "!took_match", go="log_mark")
c("Сделать запись", "Make an entry", "記入する", "has_pencil&read_old&!knows_hand", go="log_write")
c("Сделать запись", "Make an entry", "記入する", "!has_pencil&!knows_hand",
  says=[say("Писать нечем. Палец оставляет на странице только то, что и так на пальце.",
            "Nothing to write with. A finger leaves on the page only what was on the finger anyway.",
            "書くものがない。指でなぞっても、ページに残るのは指についていたものだけだ。")])
c("Сделать запись", "Make an entry", "記入する", "has_pencil&!read_old&!knows_hand",
  says=[say("Карандаш уже над страницей, но ты не знаешь, как здесь принято. Стоит сначала посмотреть, что писали до тебя.",
            "The pencil is already over the page, but you don't know how it's done here. Better look first at what they wrote before you.",
            "鉛筆はもうページの上にあるが、ここでの書き方を知らない。まず、前の人たちが何を書いたか見たほうがいい。")])
c("Перечитать свою запись", "Read your own entry again", "自分の記入を読み返す", "knows_hand",
  says=[say("«Вахту принял. Лампа капризничает». Запись как запись. Если закрыть её ладонью и открыть соседнюю, разницы не найти.",
            "\"Took the watch. The lamp is being difficult.\" An entry like any other. Cover it with your palm, uncover the one beside it, and there is no telling them apart.",
            "「当直を引き継いだ。ランプは気まぐれ」。ごくふつうの記入だ。手でかくして隣の記入を見ても、ちがいは見つからない。")])
c("Вырвать чистую страницу", "Tear out a blank page", "白いページを破り取る", "took_match&!has_match&!lit&!tore",
  says=[say("Вырываешь последнюю, чистую. Журнал вздыхает корешком. Страница сухая, старая: гореть будет хорошо.",
            "You tear out the last one, the blank one. The book sighs at the spine. The page is dry and old: it will burn well.",
            "いちばんうしろの白いページを破る。日誌が背でため息をつく。紙は古く乾いている。よく燃えるだろう。")],
  set=["tore"])
c("Закрыть журнал", "Close the log book", "日誌を閉じる", go="watch")

node("log_old", set=["read_old"])
t("Записи идут ночь за ночью, и каждая ночь одна: «Вахту принял. Лампа капризничала. Починил. Стучали. Открыл. Вахту сдал». Меняются только инициалы: А., Б., В., Г., Д.",
  "The entries go night after night, and every night is the same one: \"Took the watch. The lamp was difficult. Fixed it. Someone knocked. Opened. Handed over.\" Only the initials change: A., B., V., G., D.",
  "記入は夜ごとに続き、どの夜も同じひとつの夜だ。「当直を引き継いだ。ランプは気まぐれ。直した。ノックあり。開けた。引き継いだ」。変わるのは頭文字だけ。A、B、V、G、D。")
t("Инициалы разные. Почерк один: у буквы «у» везде один и тот же хвост, загнутый, как рыболовный крючок.",
  "The initials differ. The hand is one: everywhere the letter \"t\" ends in the same curl, like a fish hook.",
  "頭文字はちがう。筆跡はひとつだ。どこでも「ぐ」の字が、釣り針のように同じ形に曲がっている。")
c("Листать дальше", "Leaf on", "先をめくる",
  says=[say("«Гостья просила света в банку. Не дал. Напоил чаем». Через страницу: «Гостья просила света. Не дал. Напоил чаем». И ещё через страницу. Банка, судя по всему, одна и та же.",
            "\"The visitor asked for light in a jar. Didn't give any. Gave her tea.\" A page on: \"The visitor asked for light. Didn't give any. Gave her tea.\" And a page after that. The jar, by all accounts, is the same one.",
            "「客がびんに光をくれと言った。やらなかった。お茶を出した」。一枚あとにも「客が光をくれと言った。やらなかった。お茶を出した」。その次のページにも。びんは、どうやら同じびんだ。"),
        say("На полях карандашом: «Корабли за вахту: 0». И так на каждой странице, столбиком, как будто кто-то ждал, что цифра однажды изменится.",
            "In the margin, in pencil: \"Ships this watch: 0.\" And so on every page, in a column, as if somebody expected the figure to change one day.",
            "余白に鉛筆で「当直中の船　0」。どのページにも、縦にそろえて書いてある。いつか数字が変わるのを待っていたかのように。")])
c("Вернуться к обложке", "Go back to the cover", "表紙に戻る", go="log")

node("log_mark")
t("Журнал раскрывается сам. Закладкой служит спичка — целая, с серной головкой. Страницу под ней ела моль, и ела с разбором: остались одни концы строк.",
  "The book falls open by itself. The bookmark is a match: whole, with its head on. Moths have eaten the page under it, and eaten with discrimination: only the ends of the lines are left.",
  "日誌はひとりでに開く。しおりはマッチだ。頭のついた、まだ使える一本。その下のページは虫に食われている。それも選んで食べたらしく、行の終わりだけが残っている。")
t("«…не для них. …до края, а дальше не… …утром главное не забудь —» Дальше дыра аккуратной формы.",
  "\"…not for them. …as far as the edge, and beyond it no… …in the morning the main thing, don't forget to—\" After that, a neatly shaped hole.",
  "「……彼らのためではない。……ふちまでで、その先は……。……朝、いちばん大事なのは、忘れずに——」その先は、きれいな形の穴。")
c("Вынуть спичку", "Take the match out", "マッチを抜く",
  says=[say("Спичка одна. Это чувствуется сразу — как чувствуется последний кусок сахара.",
            "There is one match. You feel it at once, the way you feel the last lump of sugar.",
            "マッチは一本きり。それはすぐに分かる。最後の角砂糖が分かるのと同じように。")],
  set=["took_match", "has_match"], go="log")
c("Не трогать закладку", "Leave the bookmark alone", "しおりにはさわらない", go="log")

node("log_write", set=["knows_hand"])
t("Пишешь под вчерашней записью: «Вахту принял. Лампа капризничает». Карандаш идёт легко, как по накатанному.",
  "You write under yesterday's entry: \"Took the watch. The lamp is being difficult.\" The pencil goes easily, as if along a worn track.",
  "きのうの記入の下に書く。「当直を引き継いだ。ランプは気まぐれ」。鉛筆は、通いなれた道を行くようにすらすら進む。")
t("Потом смотришь на свою букву «у». Хвост у неё загнут, как рыболовный крючок. Смотришь на «у» строчкой выше. Потом ещё выше. Листать назад не обязательно.",
  "Then you look at your letter \"t\". It ends in a curl like a fish hook. You look at the \"t\" one line up. Then one more up. There is no need to leaf back.",
  "それから自分の「ぐ」の字を見る。釣り針のように曲がっている。一行上の「ぐ」を見る。さらにその上を見る。前のページまでめくる必要はない。")
c("Сравнить ещё раз", "Compare once more", "もう一度くらべる",
  says=[say("Пробуешь написать «у» по-другому, нарочно. Выходит по-другому. Отнимаешь карандаш — крючок на месте. Рука дописала его сама, из вежливости.",
            "You try writing the \"t\" differently, on purpose. It comes out different. You lift the pencil: the curl is there. The hand added it by itself, out of politeness.",
            "わざと、ちがう形の「ぐ」を書いてみる。ちがう形になる。鉛筆を離すと、釣り針はちゃんとある。手が、礼儀として自分で書き足したのだ。")])
c("Закрыть журнал", "Close the log book", "日誌を閉じる", go="watch")

# ------------------------------------------------------------------ radio
node("radio")
t("Радио: шкала, одна ручка, тангента на витом шнуре. На шкале карандашом отмечена одна волна и подписано: «берег».",
  "The radio: a dial, one knob, a handset on a coiled cord. One wavelength is marked on the dial in pencil and labelled \"shore\".",
  "無線機。目盛り、つまみがひとつ、巻いたコードの先に送話器。目盛りには鉛筆で一か所だけ印があり、「岸」と書いてある。")
c("Вызвать берег", "Call the shore", "岸を呼ぶ",
  says=[say("«Берег, я Фонарь. Приём». Секунду шипит. Потом из динамика: «Берег, я Фонарь. Приём». Голос твой. Интонация твоя. Даже вдох перед «приём» — твой.",
            "\"Shore, this is Lantern. Over.\" A second of hiss. Then from the speaker: \"Shore, this is Lantern. Over.\" The voice is yours. The tone is yours. Even the breath before \"over\" is yours.",
            "「岸、こちら灯。どうぞ」。一秒、雑音。それからスピーカーが言う。「岸、こちら灯。どうぞ」。きみの声だ。きみの調子だ。「どうぞ」の前の息つぎまで、きみのものだ。")])
c("Покрутить ручку", "Turn the knob", "つまみを回す",
  says=[say("На всех волнах одно и то же: море. На одной, кажется, чайник — но это твой, за спиной.",
            "The same on every wavelength: the sea. On one of them, it seems, a kettle; but that one is yours, behind your back.",
            "どの波長も同じ、海の音だ。ひとつだけ、やかんの音がする気がする。でもそれは、うしろにあるきみのやかんだ。")])
c("Молчать и слушать", "Keep quiet and listen", "だまって聞く", "lit&!heard", go="radio_listen")
c("Молчать и слушать", "Keep quiet and listen", "だまって聞く", "!lit",
  says=[say("Шипит. Пока лампа не горит как следует, радио, видимо, не о чем говорить.",
            "It hisses. Until the lamp burns properly, the radio apparently has nothing to talk about.",
            "雑音だけ。ランプがちゃんと灯るまでは、無線機にも話すことがないらしい。")])
c("Спросить, который час", "Ask what the time is", "いま何時かたずねる", "heard",
  says=[say("«Берег, который час?» На этот раз ответ приходит раньше, чем ты договариваешь: «Тот же, что у тебя». И, подумав: «Он тут один».",
            "\"Shore, what's the time?\" This time the answer comes before you finish: \"The same as yours.\" And, after a thought: \"There's only the one here.\"",
            "「岸、いま何時だ」。こんどは言い終わる前に返事が来る。「そっちと同じ」。少し考えて、もうひとこと。「ここには、ひとつしかないから」")])
c("Встать от радио", "Get up from the radio", "無線機から離れる", go="watch")

node("radio_listen", set=["heard"])
t("Ты держишь тангенту и молчишь. Набираешь воздуха, чтобы сказать «Берег, я Фонарь». Из динамика твоим голосом: «Берег, я Фонарь». Ты ещё не сказал. Оно уже сказало.",
  "You hold the handset and keep quiet. You draw breath to say \"Shore, this is Lantern\". From the speaker, in your voice: \"Shore, this is Lantern.\" You haven't said it yet. It already has.",
  "送話器を持ったまま、だまっている。「岸、こちら灯」と言おうと息を吸う。スピーカーが、きみの声で言う。「岸、こちら灯」。きみはまだ言っていない。向こうはもう言った。")
t("Потом голос начинает считать — спокойно, как считают перед сном: «Десять. Девять. Восемь. Семь. Шесть. Пять…» На пяти помехи, будто кто-то накрыл микрофон ладонью.",
  "Then the voice begins to count, calmly, the way one counts before sleep: \"Ten. Nine. Eight. Seven. Six. Five…\" At five there is interference, as if someone had put a palm over the microphone.",
  "それから声は数えはじめる。眠る前に数えるように、おだやかに。「十。九。八。七。六。五……」五のところで雑音が入る。だれかがマイクを手のひらでおおったように。")
c("Досчитать самому", "Count the rest yourself", "つづきを自分で数える",
  says=[say("«Четыре. Три. Два. Один». На нуле ничего не происходит. Только лампа наверху гудит чуть ровнее, и ты замечаешь, что давно не слышишь собственного сердца — а оно, оказывается, никуда не торопится.",
            "\"Four. Three. Two. One.\" At zero nothing happens. Only the lamp upstairs hums a little more evenly, and you notice you haven't heard your own heart for a while; it turns out it is in no hurry.",
            "「四。三。二。一」。ゼロでは何も起こらない。ただ上のランプのうなりが少しなめらかになり、きみは、しばらく自分の心臓の音を聞いていなかったことに気づく。心臓は、べつに急いでいなかった。")],
  go="watch")
c("Выключить радио", "Switch the radio off", "無線機を切る",
  says=[say("Щёлкаешь выключателем. Шкала гаснет. «Пять», — договаривает динамик уже без питания, тихо, для порядка.",
            "You click the switch. The dial goes dark. \"Five,\" the speaker finishes, already without power, quietly, for the sake of order.",
            "スイッチを切る。目盛りの灯が消える。「五」と、電源の切れたスピーカーが小さく言い終える。けじめとして。")],
  go="watch")

# ------------------------------------------------------------------ kettle
node("kettle")
t("Керосинка горит маленьким синим огнём — похоже, её вообще не гасят. Чайник на ней холодный: огонь убавлен до того, что греет только сам себя.",
  "The stove burns with a small blue flame; it seems nobody ever puts it out. The kettle on it is cold: the flame is turned so low that it warms only itself.",
  "こんろは小さな青い火で燃えている。どうやら消されたことがない。上のやかんは冷たい。火は、自分をあたためるだけの弱さまで絞ってある。", "!tea_ready")
t("Керосинка горит. Чайник горячий и тихонько посвистывает носом, как человек, которому есть что сказать.",
  "The stove burns. The kettle is hot and whistles softly through its spout, like someone with something to say.",
  "こんろは燃えている。やかんは熱く、言いたいことのある人のように、口で小さく笛を鳴らしている。", "tea_ready")
c("Прибавить огня и заварить чай", "Turn the flame up and make tea", "火を強くしてお茶をいれる", "!tea_ready",
  says=[say("Прибавляешь огня, сыплешь в чайник из жестянки — на двоих, как написано. Через минуту в комнате пахнет дымом и летом.",
            "You turn the flame up and tip tea from the tin into the kettle: for two, as it says. A minute later the room smells of smoke and summer.",
            "火を強くし、缶のお茶をやかんに入れる。書いてあるとおり、二人ぶん。一分もすると、部屋は煙と夏のにおいになる。")],
  set=["tea_ready"])
c("Налить себе стакан", "Pour yourself a glass", "自分に一杯つぐ", "tea_ready",
  says=[say("Чай горячий и чуть горчит. Стакан греет ладони так, будто его только что вынесли с солнца.",
            "The tea is hot and slightly bitter. The glass warms your palms as if it had just been carried in out of the sun.",
            "お茶は熱く、少し苦い。グラスは、いま日なたから運んできたかのように手のひらをあたためる。")])
c("Зажечь страницу и бежать наверх", "Light the page and run upstairs", "紙に火をつけて上へ走る", "tore&!lit",
  says=[say("Скручиваешь страницу жгутом, суёшь край в синий огонь. Берётся сразу. Сорок ступеней бегом, ладонь домиком; огонёк ложится набок, но держится — ему, кажется, самому интересно, чем кончится.",
            "You twist the page into a spill and put its end into the blue flame. It catches at once. Forty steps at a run, your palm cupped; the little flame lies on its side but holds on. It seems curious itself how this will end.",
            "紙をこよりにして、端を青い火に入れる。すぐにつく。手のひらでかこって四十段をかけ上がる。小さな火は横倒しになるが、消えない。火のほうも、結末が気になるらしい。")],
  go="relight")
c("Отойти от керосинки", "Step away from the stove", "こんろから離れる", go="watch")

# ------------------------------------------------------------------ cellar
node("cellar")
t("Подвал. Бак с маслом во всю стену, на нём манометр. От бака вверх уходит труба, на трубе вентиль, рядом ручной насос с табличкой.",
  "The cellar. An oil tank the size of the wall, with a gauge on it. A pipe runs up from the tank, a valve on the pipe, and beside it a hand pump with a plate.",
  "地下室。壁いっぱいの油タンクに圧力計。タンクから上へ管がのび、管にはバルブ、その横に、札のついた手押しポンプ。")
t("У вентиля нет маховика: торчит голый квадратный шток.",
  "The valve has no handwheel: a bare square stem sticks out.",
  "バルブにはハンドルがない。四角い軸がむきだしで突き出ている。", "!valve")
t("Вентиль открыт. На нём сидит «не колесо» и, надо признать, выглядит как колесо.",
  "The valve is open. The \"not a wheel\" sits on it and, it must be admitted, looks like a wheel.",
  "バルブは開いている。「車輪ではない」がそこに収まっていて、認めるほかないが、車輪に見える。", "valve")
t("В трубе тихо идёт масло — вверх, к лампе.",
  "Oil moves quietly in the pipe: up, to the lamp.",
  "管の中を油が静かに流れていく。上へ、ランプへ。", "pumped")
c("Постучать по манометру", "Tap the gauge", "圧力計をたたく",
  says=[say("Стрелка стоит на «ПОЛНО». Стучишь — падает на ноль, лежит там, пока ты смотришь, и возвращается на «ПОЛНО», стоит отвернуться. Сменщик был прав: манометр врёт. Но врёт вежливо.",
            "The needle stands at FULL. You tap: it drops to zero, lies there while you look, and goes back to FULL the moment you turn away. The day keeper was right: the gauge lies. But it lies politely.",
            "針は「満」を指している。たたくとゼロに落ち、見ているあいだはそこに寝ていて、目をそらしたとたん「満」に戻る。昼の番人の言うとおり、圧力計はうそをつく。ただし、礼儀正しく。")])
c("Надеть «не колесо» на шток и открыть", "Fit the \"not a wheel\" on the stem and open", "「車輪ではない」を軸にはめて開ける", "has_wheel&!valve",
  says=[say("Квадратная дыра садится на квадратный шток, как будто они знакомы. Три оборота влево. В трубе что-то сглатывает.",
            "The square hole settles onto the square stem as if they knew each other. Three turns to the left. Something in the pipe swallows.",
            "四角い穴が四角い軸に、顔なじみのようにはまる。左へ三回。管の中で、何かがごくりとのみこむ。")],
  set=["valve"])
c("Повернуть шток пальцами", "Turn the stem with your fingers", "軸を指で回す", "!has_wheel&!valve",
  says=[say("Шток холодный, скользкий и поворачиваться не намерен. На него явно надевалось что-то круглое, со спицами. Что-то, что кто-то унёс и повесил подальше.",
            "The stem is cold, slippery and has no intention of turning. Something round with spokes obviously went on it. Something that somebody carried off and hung up out of the way.",
            "軸は冷たく、すべり、回る気はない。ここには明らかに、輻のある丸いものがはまっていた。だれかが持ち去って、遠くに掛けた何かが。")])
c("Качать насос", "Work the pump", "ポンプを押す", "valve&!pumped", go="pump")
c("Качать насос", "Work the pump", "ポンプを押す", "!valve",
  says=[say("Качаешь. Насос вздыхает, труба молчит. Масло стоит в баке и никуда не собирается, пока закрыт вентиль.",
            "You pump. The pump sighs, the pipe says nothing. The oil stays in the tank and isn't going anywhere while the valve is shut.",
            "押す。ポンプがため息をつき、管はだまっている。バルブが閉まっているかぎり、油はタンクの中でじっとしている。")])
c("Заглянуть в бак", "Look into the tank", "タンクをのぞく", go="tank")
c("Подняться в комнату", "Go up to the room", "部屋へ上がる", go="watch")

node("pump")
t("На насосе табличка: «Десять качков. Считать вслух. С десяти». Почему с десяти — не сказано. Берёшься за рычаг.",
  "The plate on the pump: \"Ten strokes. Count aloud. From ten.\" Why from ten, it doesn't say. You take hold of the lever.",
  "ポンプの札。「十回押すこと。声に出して数える。十から」。なぜ十からなのかは書いていない。レバーを握る。")
t("«Десять». Рычаг идёт туго. «Девять. Восемь». Легче. «Семь. Шесть». Ещё легче; с каждым качком в голове тоже как будто убывает. «Пять…» И ты не помнишь, что идёт после пяти.",
  "\"Ten.\" The lever goes stiffly. \"Nine. Eight.\" Easier. \"Seven. Six.\" Easier still; with every stroke something seems to lessen in your head too. \"Five…\" And you can't remember what comes after five.",
  "「十」。レバーは重い。「九。八」。軽くなる。「七。六」。さらに軽い。ひと押しごとに、頭の中のものも減っていく気がする。「五……」そして、五の次が何だったか思い出せない。")
c("Начать заново, с десяти", "Start again, from ten", "十からやり直す",
  says=[say("Со второго раза доходишь до нуля. На нуле рычаг проваливается, как ступенька, которой нет, и в трубе начинает идти масло. Что ты сбрасывал вместе с числами, ты уже не помнишь. Наверное, неважное.",
            "The second time you get to zero. At zero the lever drops away like a step that isn't there, and oil starts moving in the pipe. What you were shedding along with the numbers, you no longer remember. Nothing important, probably.",
            "二度目はゼロまで行ける。ゼロでレバーが、あるはずの段がなかったときのようにすとんと落ち、管を油が流れはじめる。数といっしょに何を手放していたのか、もう思い出せない。たぶん、大したものではない。")],
  set=["pumped"], go="cellar")
c("Продолжить с пяти, наугад", "Carry on from five, by guess", "五から、あてずっぽうで続ける",
  says=[say("«Четыре?..» Рычаг соглашается. «Три. Два. Один». На нуле он проваливается, как ступенька, которой нет, и в трубе начинает идти масло. Числа, оказывается, помнят себя сами.",
            "\"Four?..\" The lever agrees. \"Three. Two. One.\" At zero it drops away like a step that isn't there, and oil starts moving in the pipe. Numbers, it turns out, remember themselves.",
            "「四……？」レバーが同意する。「三。二。一」。ゼロで、あるはずの段がなかったときのようにすとんと落ち、管を油が流れはじめる。数というものは、自分で自分を覚えているらしい。")],
  set=["pumped"], go="cellar")

node("tank")
t("Откидываешь люк на баке. Масла — до краёв: чёрное, гладкое, неподвижное. Лампа никогда не оставалась без масла. Она оставалась без того, кто откроет вентиль.",
  "You throw back the hatch on the tank. Oil to the brim: black, smooth, still. The lamp was never short of oil. It was short of someone to open the valve.",
  "タンクのふたをはね上げる。油はふちまである。黒く、なめらかで、動かない。ランプに油が足りなかったことは一度もない。足りなかったのは、バルブを開ける者だ。")
t("В масле отражается твоё лицо. Оно спокойнее, чем ты ожидал.",
  "Your face is reflected in the oil. It is calmer than you expected.",
  "油にきみの顔が映っている。思っていたより落ちついた顔だ。")
c("Помахать отражению", "Wave to the reflection", "映った顔に手を振る",
  says=[say("Отражение машет в ответ — вовремя, ни на секунду не опоздав. После радио это даже немного обидно.",
            "The reflection waves back: on time, not a second late. After the radio this is almost a little hurtful.",
            "映った顔も手を振り返す。一秒も遅れず、ぴったりに。無線機のあとでは、少しさびしいくらいだ。")])
c("Закрыть люк", "Close the hatch", "ふたを閉める", go="cellar")

# ------------------------------------------------------------------ stairs, window
node("stairs")
t("Винтовая лестница. Сорок ступеней, если верить цифре, выбитой на нижней. На полпути узкое окно.",
  "A winding stair. Forty steps, if you believe the number cut into the bottom one. Halfway up, a narrow window.",
  "らせん階段。いちばん下の段に刻まれた数字を信じるなら、四十段。途中に細い窓がある。")
c("Подняться к лампе", "Climb to the lamp", "ランプまで上がる", go="gallery")
c("Считать ступени", "Count the steps", "段を数える",
  says=[say("Вверх выходит сорок. Вниз — тридцать девять. Пробуешь ещё раз: сорок и тридцать девять. Одна ступенька, видимо, существует только в одну сторону.",
            "Going up it comes to forty. Going down, thirty-nine. You try again: forty and thirty-nine. One step, apparently, exists in one direction only.",
            "上りは四十。下りは三十九。もう一度やっても、四十と三十九。どうやら一段だけ、片方向にしか存在しない。")])
c("Подойти к окну", "Go to the window", "窓に寄る", go="window")
c("Спуститься в комнату", "Go down to the room", "部屋へ下りる", go="watch")

node("window")
t("Окно выходит на море. Стекло толстое, зеленоватое, с пузырьком воздуха, который старше тебя.",
  "The window looks out on the sea. The glass is thick, greenish, with an air bubble in it that is older than you.",
  "窓は海に面している。ガラスは厚く、緑がかり、きみより年上の気泡がひとつ入っている。")
t("За стеклом темно так ровно, что непонятно, далеко это или прямо за рамой.",
  "Beyond the glass it is dark so evenly that you can't tell whether it's far away or right behind the frame.",
  "ガラスの向こうは、むらなく暗い。遠いのか、枠のすぐ外なのか、見分けがつかない。", "!lit")
t("Раз в оборот по воде проходит луч, и вода под ним на секунду становится водой: с рябью, с пеной, с чайкой, которая спит и качается.",
  "Once a turn the beam crosses the water, and for a second the water under it becomes water: with ripples, with foam, with a gull that sleeps and rocks.",
  "一回りごとに光の帯が水の上を通り、その下で水は一秒だけ水になる。さざ波があり、泡があり、眠ったままゆれるカモメがいる。", "lit")
c("Протереть стекло рукавом", "Wipe the glass with your sleeve", "そででガラスをふく",
  says=[say("Стекло становится чище. То, что за ним, — нет: оно и было чистое.",
            "The glass gets cleaner. What is behind it doesn't: it was clean already.",
            "ガラスはきれいになる。その向こうのものは変わらない。もともと、きれいだったのだ。")])
c("Вернуться на лестницу", "Go back to the stairs", "階段に戻る", go="stairs")

# ------------------------------------------------------------------ lamp room
node("gallery")
t("Ламповая. Посередине лампа в человеческий рост за толстыми линзами, вокруг — стёкла от пола до потолка и дверь на балкон.",
  "The lamp room. In the middle the lamp, tall as a person, behind thick lenses; around it, glass from floor to ceiling and a door to the balcony.",
  "ランプ室。真ん中に、厚いレンズにかこまれた人の背丈ほどのランプ。まわりは床から天井までガラスで、バルコニーへの扉がある。")
t("Лампа не горит. Судя по тёплому стеклу, погасла она ровно тогда, когда ты ступил на последнюю ступеньку: дождалась свидетеля.",
  "The lamp is out. Judging by the warm glass, it went out exactly when you set foot on the last step: it waited for a witness.",
  "ランプは消えている。ガラスのあたたかさからすると、きみが最後の段に足をのせた、ちょうどそのときに消えたのだ。見届ける者を待っていたらしい。", "!lit")
t("Лампа горит ровно, без копоти, и медленно поворачивает свой свет над водой и берегом.",
  "The lamp burns evenly, without soot, and slowly turns its light over the water and the shore.",
  "ランプはすすも出さず、むらなく燃え、水と岸の上でゆっくりと光を回している。", "lit")
t("Одна створка окна открыта. Тянет сквозняком; на раме сидят мотыльки и ждут, когда будет на что лететь.",
  "One window sash is open. There's a draught; moths sit on the frame, waiting for something to fly at.",
  "窓がひとつ開いている。すきま風が入り、枠には蛾がとまって、飛んでいく先ができるのを待っている。", "!shut")
c("Постучать по манометру лампы", "Tap the lamp's gauge", "ランプの圧力計をたたく",
  says=[say("«ПОЛНО». Стучишь — «ПОЛНО». Этот манометр даже не притворяется, что думает.",
            "FULL. You tap: FULL. This gauge doesn't even pretend to think about it.",
            "「満」。たたいても「満」。この圧力計は、考えるふりさえしない。")])
c("Закрыть створку", "Close the sash", "窓を閉める", "!shut",
  says=[say("Закрываешь, поворачиваешь шпингалет. Сквозняк пропадает. Мотыльки остаются снаружи и смотрят через стекло без обиды.",
            "You close it and turn the catch. The draught is gone. The moths stay outside and look through the glass without resentment.",
            "閉めて、掛け金を回す。すきま風が止まる。蛾は外に残り、うらみもせずガラスごしにこちらを見ている。")],
  set=["shut"])
c("Подкрутить фитиль", "Turn up the wick", "芯を上げる", "!pumped&!lit",
  says=[say("Фитиль выходит сухой, как прошлогодняя трава. Зажигать нечего: масла в лампе нет, что бы ни показывал манометр. Масло где-то ниже.",
            "The wick comes up dry as last year's grass. There is nothing to light: no oil in the lamp, whatever the gauge shows. The oil is somewhere lower down.",
            "芯は去年の草のように乾いている。火をつけるものがない。圧力計が何を指していようと、ランプに油はない。油はどこか下にある。")])
c("Чиркнуть спичкой", "Strike the match", "マッチをする", "has_match&pumped&!lit&shut",
  says=[say("Чиркаешь о подоконник. Спичка думает секунду и загорается. Воздух в ламповой стоит неподвижно, и огонёк стоит в нём ровно, как нарисованный.",
            "You strike it on the sill. The match thinks for a second and lights. The air in the lamp room stands still, and the little flame stands straight in it as if drawn.",
            "窓台でする。マッチは一秒考えてから、つく。ランプ室の空気は動かず、小さな火は、絵にかいたようにまっすぐ立っている。")],
  go="relight")
c("Чиркнуть спичкой", "Strike the match", "マッチをする", "has_match&pumped&!lit&!shut",
  says=[say("Чиркаешь. Спичка загорается — и сквозняк из открытой створки снимает огонь с неё одним движением, как шляпу. Мотыльки на раме, кажется, разочарованы не меньше тебя.",
            "You strike it. The match lights, and the draught from the open sash takes the flame off it in one movement, like a hat. The moths on the frame seem no less disappointed than you.",
            "する。マッチはつく。開いた窓からのすきま風が、帽子でも取るようにひと息で火をさらっていく。枠の蛾も、きみに負けずがっかりしているようだ。"),
        say("Это была единственная. Огонь в башне теперь только один — внизу, под чайником. Донести бы его сюда на чём-нибудь, что горит.",
            "That was the only one. There is just one fire left in the tower now: downstairs, under the kettle. If only it could be carried up here on something that burns.",
            "あれが最後の一本だった。塔の中の火は、もうひとつだけ。下の、やかんの下だ。何か燃えるものにうつして、ここまで運べればいいのだが。")],
  set=["-has_match"])
c("Поискать, чем зажечь", "Look for something to light it with", "火をつけるものをさがす", "!took_match&!lit",
  says=[say("Карманы пусты. Спички на посту вроде бы водились: коробок в ящике стола. И ещё что-то тонкое торчало из журнала.",
            "Your pockets are empty. There used to be matches at the post, it seems: the box in the desk drawer. And something thin was sticking out of the log book.",
            "ポケットはからっぽ。番所にマッチはあったはずだ。机の引き出しの箱。それから、日誌から何か細いものがのぞいていた。")])
c("Поискать, чем зажечь", "Look for something to light it with", "火をつけるものをさがす", "took_match&!has_match&!lit",
  says=[say("Здесь — нечем. Огонь есть внизу, под чайником. Бумага горит. Бумаги на посту — один журнал.",
            "Up here, nothing. There is fire downstairs, under the kettle. Paper burns. The only paper at the post is the log book.",
            "ここには何もない。火は下の、やかんの下にある。紙は燃える。番所にある紙は、日誌だけだ。")])
c("Заглянуть в лампу", "Look into the lamp", "ランプをのぞきこむ", "lit", go="lens")
c("Выйти на балкон", "Go out on the balcony", "バルコニーに出る", go="balcony")
c("Спуститься в комнату", "Go down to the room", "部屋へ下りる", go="watch")

node("relight", set=["lit", "-has_match"])
t("Подносишь огонь к фитилю. Лампа берёт его не сразу — сначала как будто принюхивается. Потом внутри линз встаёт свет, слишком большой для такого маленького огня, и ты отворачиваешься, потому что смотреть на него нельзя.",
  "You bring the flame to the wick. The lamp doesn't take it at once: first it seems to sniff at it. Then a light rises inside the lenses, far too big for so small a flame, and you turn away, because it can't be looked at.",
  "火を芯に近づける。ランプはすぐには受けとらない。まず、においをかぐようにしている。それからレンズの内側に、こんな小さな火には大きすぎる光が立ち、きみは顔をそむける。見ていられないからだ。")
t("Свет уходит за стёкла и ложится на воду. За ним, как за рукой, проявляется берег: причал, крыши, окно, ещё окно. В одном уже кто-то ставит чайник — так, будто делал это весь вечер.",
  "The light goes out through the glass and lies down on the water. After it, as after a hand, the shore comes up: a pier, roofs, a window, another window. In one of them somebody is already putting a kettle on, as if they had been doing so all evening.",
  "光はガラスを抜けて、水の上に横たわる。そのあとを、手でなでたあとのように岸が浮かび上がる。桟橋、屋根、窓、また窓。そのひとつでは、もうだれかがやかんを火にかけている。夕方からずっとそうしていたように。")
c("Выйти на балкон", "Go out on the balcony", "バルコニーに出る", go="balcony")
c("Остаться у лампы", "Stay by the lamp", "ランプのそばに残る", go="gallery")
c("Спуститься в комнату", "Go down to the room", "部屋へ下りる", go="watch")

node("lens")
t("Через тёмное стёклышко на цепочке можно заглянуть за линзы. Огня там, если честно, почти нет: синий язычок с ноготь. Свет берётся не из него. Он как будто проходит сквозь огонёк откуда-то, где его много.",
  "Through the little dark glass on a chain you can look in behind the lenses. To be honest there is hardly any flame in there: a blue tongue the size of a fingernail. The light doesn't come from it. It seems to pass through the little flame from somewhere that has plenty.",
  "鎖についた黒い小さなガラスごしに、レンズの奥をのぞける。正直なところ、火はほとんどない。爪ほどの青い舌がひとつ。光はそこから出ているのではない。どこか、光のたくさんあるところから、小さな火を通り抜けてくるようだ。")
c("Смотреть дальше", "Keep looking", "見つづける",
  says=[say("Лицу тепло, как в полдень у стены. Когда отводишь глаза, перед ними висит круглое пятно и не спеша плывёт за взглядом — на стену, на пол, на твои руки.",
            "Your face is warm, as at noon by a wall. When you look away a round spot hangs before your eyes and drifts unhurriedly after your gaze: onto the wall, the floor, your hands.",
            "顔が、真昼の壁ぎわにいるようにあたたかい。目をそらすと、丸いしみが目の前に浮かび、視線のあとをゆっくりついてくる。壁へ、床へ、きみの手へ。")])
c("Отойти от лампы", "Step away from the lamp", "ランプから離れる", go="gallery")

# ------------------------------------------------------------------ balcony
node("balcony")
t("Балкон вокруг ламповой: железный настил, перила, ветер с моря.",
  "The balcony round the lamp room: iron decking, a rail, wind off the sea.",
  "ランプ室をぐるりとかこむバルコニー。鉄の床、手すり、海からの風。")
t("Темно. Города на берегу не видно — ни окна, ни фонаря. Его не то чтобы погасили. Его как будто убрали на ночь, как убирают бельё перед дождём.",
  "It is dark. The town on the shore can't be seen: not a window, not a street lamp. It isn't that it has been put out. It seems to have been taken in for the night, the way washing is taken in before rain.",
  "暗い。岸の町は見えない。窓のひとつも、街灯のひとつも。消されたというのではない。雨の前に洗濯物を取りこむように、夜のあいだ、しまわれたらしい。", "!lit")
t("Свет лампы идёт по кругу: вода, причал, крыши, снова вода. Город под ним виден весь — до последнего окна. Кажется, до последнего.",
  "The lamp's light goes round: water, pier, roofs, water again. The town beneath it can be seen whole, to the last window. To the last, it would seem.",
  "ランプの光がめぐる。水、桟橋、屋根、また水。その下で、町はすみずみまで見える。最後の窓まで。たぶん、最後まで。", "lit")
c("Проследить, где кончается свет", "Follow where the light ends", "光がどこで終わるかを目で追う", "lit", go="edge")
c("Ждать поезда", "Wait for the train", "列車を待つ", "lit", go="train")
c("Крикнуть в темноту", "Shout into the dark", "闇に向かってさけぶ", "!lit",
  says=[say("«Э-эй!» Эха нет. Чтобы было эхо, нужно, чтобы там что-то стояло.",
            "\"He-ey!\" No echo. For an echo, something has to be standing out there.",
            "「おーい！」こだまは返らない。こだまが返るには、向こうに何かが立っていなければならない。")])
c("Вернуться к лампе", "Go back to the lamp", "ランプのところへ戻る", go="gallery")

node("edge", set=["saw_edge"])
t("Ты ведёшь глазами за лучом, до самого его края. Край ровный, как у скатерти. По эту сторону — улица, забор, кошка на заборе. По ту — не темнота. Темнота — это когда что-то есть, но не видно. Там просто дальше не сделано.",
  "You follow the beam with your eyes to its very edge. The edge is straight, like a tablecloth's. On this side: a street, a fence, a cat on the fence. On the other, not darkness. Darkness is when something is there and can't be seen. Out there it simply hasn't been made any further.",
  "光の帯を、そのふちまで目で追う。ふちはテーブルかけのようにまっすぐだ。こちら側には、通り、へい、へいの上の猫。向こう側は、闇ではない。闇というのは、何かがあるのに見えないことだ。あそこはただ、その先が作られていない。")
t("Луч идёт дальше, и улица идёт с ним: дома встают перед светом и складываются за ним, без стука. Кошка на заборе существует пол-оборота. Вид у неё при этом совершенно спокойный.",
  "The beam moves on, and the street moves with it: houses stand up before the light and fold away behind it, without a sound. The cat on the fence exists for half a turn. It looks perfectly calm about it.",
  "光は進み、通りもいっしょに進む。家々は光の前で立ち上がり、光のうしろで音もなくたたまれる。へいの上の猫は、半回りのあいだだけ存在する。それでいて、まったく落ちついた顔をしている。")
c("Досмотреть оборот", "Watch the turn through", "一回りを見とどける",
  says=[say("Свет возвращается. Улица, забор, кошка. Она сидит ровно там же и так же, будто никуда не девалась. С её точки зрения, наверное, так и есть.",
            "The light comes back. Street, fence, cat. It sits in exactly the same place in the same way, as if it had never gone anywhere. From its point of view, that is probably so.",
            "光が戻ってくる。通り、へい、猫。猫はまったく同じ場所に同じ姿ですわっている。どこへも行かなかったように。猫にしてみれば、たぶんそのとおりなのだ。")])
c("Отойти от перил", "Step back from the rail", "手すりから離れる", go="balcony")

node("train")
t("Ждать недолго. Сначала по воде идёт дрожь, как по чаю в стакане на столике. Потом из-за мыса выходит поезд: два вагона, круглые фары. Он идёт по морю. Рельсы лежат под самой водой, и колёса режут её двумя тонкими усами.",
  "Not a long wait. First a shiver runs over the water, as over tea in a glass on a carriage table. Then a train comes out from behind the cape: two carriages, round headlamps. It runs across the sea. The rails lie just under the water, and the wheels cut it into two thin whiskers.",
  "長くは待たない。まず水の上を、車内のテーブルのグラスのお茶のようなふるえが走る。それから岬のかげから列車が出てくる。二両、丸い前照灯。列車は海の上を行く。線路は水のすぐ下にあり、車輪が水を二すじの細いひげに切っていく。")
t("В вагонах горит тёплый свет. В одном окне пассажир — один на весь поезд. Он смотрит на лампу. Проезжая, поднимает руку.",
  "A warm light burns in the carriages. In one window there is a passenger: one for the whole train. He is looking at the lamp. As he passes, he raises a hand.",
  "車内にはあたたかい灯がともっている。窓のひとつに乗客がいる。列車じゅうで、ひとりきり。ランプを見ている。通りすぎるとき、片手を上げる。")
c("Помахать в ответ", "Wave back", "手を振り返す",
  says=[say("Машешь. Пассажир кивает, как кивают знакомому, с которым каждый день ездят в одну сторону. Поезд уходит за край света — в прямом смысле — и не гремит там, а просто перестаёт.",
            "You wave. The passenger nods, the way one nods to an acquaintance one rides the same way with every day. The train passes beyond the edge of the light, in the plain sense, and doesn't rumble on out there but simply stops being.",
            "手を振る。乗客は、毎日同じ方向に乗り合わせる顔なじみにするように、うなずく。列車は光のふちの向こうへ出ていく。文字どおりに。その先でごとごと鳴ることもなく、ただ、やむ。")],
  go="balcony")
c("Не махать", "Don't wave", "手を振らない",
  says=[say("Не машешь. Пассажир держит руку поднятой до самого края света — терпеливо, без упрёка. По расписанию, приклеенному к перилам, обратный поезд «по возможности».",
            "You don't wave. The passenger keeps his hand raised right to the edge of the light: patiently, without reproach. According to the timetable glued to the rail, the return train runs \"when possible\".",
            "手は振らない。乗客は光のふちまで、手を上げたままでいる。しんぼう強く、責めもせずに。手すりにはられた時刻表によれば、戻りの列車は「可能なかぎり」。")],
  go="balcony")

# ------------------------------------------------------------------ door, visitor
node("door")
t("Дверь наружу: дубовая, с засовом. На косяке зарубки — кто-то отмечал не то рост, не то ночи.",
  "The door outside: oak, with a bolt. Notches on the jamb: somebody was marking either height or nights.",
  "外への扉。樫の木で、かんぬきがついている。柱に刻み目がある。背丈を記したのか、夜を数えたのか。")
t("За дверью тихо. Так тихо бывает не снаружи, а внутри закрытой коробки.",
  "It is quiet behind the door. Quiet not the way it is outdoors but the way it is inside a closed box.",
  "扉の向こうは静かだ。外の静けさではなく、ふたをした箱の中の静けさ。", "!lit")
t("Стучат. Три раза. Пауза — ровно такая, чтобы успеть подойти.",
  "Knocking. Three times. A pause, exactly long enough to reach the door.",
  "ノック。三回。それから間。扉まで歩いていくのに、ちょうど足りるだけの。", "lit&!opened")
t("Дверь открыта. Гостья стоит на пороге и ждёт, не переступая.",
  "The door is open. The visitor stands on the threshold and waits without stepping over it.",
  "扉は開いている。客は敷居のところに立ち、またがずに待っている。", "opened&!tea_done")
c("Отодвинуть засов и открыть", "Draw the bolt and open", "かんぬきを外して開ける", "lit&!opened", set=["opened"], go="visitor",
  says=[say("Засов идёт легко: его смазывали чаще, чем всё остальное на посту.",
            "The bolt slides easily: it has been oiled more often than anything else at the post.",
            "かんぬきは軽く動く。番所のどこよりも、ここにはよく油がさしてある。")])
c("Выйти к гостье", "Go out to the visitor", "客のところへ出る", "opened&!tea_done", go="visitor")
c("Выйти наружу", "Go outside", "外に出る", "!lit", go="shore_dark")
c("Выйти на берег", "Go out on the shore", "岸に出る", "tea_done", go="shore_night")
c("Вернуться к столу", "Go back to the desk", "机に戻る", go="watch")

node("visitor")
t("На пороге старушка в платке, маленькая, сухая. В руках у неё стеклянная банка с крышкой, пустая и очень чистая. «Мне бы света, — говорит она. — В банку. Немножко. До утра».",
  "On the threshold is an old woman in a headscarf, small and dry. In her hands a glass jar with a lid, empty and very clean. \"I'd like some light,\" she says. \"In the jar. Just a little. To last till morning.\"",
  "敷居に、スカーフをかぶった小さなやせたおばあさんがいる。手には、ふたつきのガラスびん。からっぽで、とてもきれいだ。「光を少し、もらえないかね」と言う。「このびんに。ほんの少し。朝までのぶん」")
c("«Свет не выдаётся»", "\"Light isn't handed out\"", "「光はお渡しできません」",
  says=[say("Она кивает без обиды. «Все так говорят. В первую ночь». Банку при этом не убирает.",
            "She nods without offence. \"They all say that. On the first night.\" She doesn't put the jar away, though.",
            "おばあさんは気を悪くもせずうなずく。「みんなそう言うよ。最初の晩はね」。それでもびんは引っこめない。")])
c("«Откуда вы?»", "\"Where are you from?\"", "「どちらからいらしたんですか」",
  says=[say("«Оттуда, куда достаёт, — она показывает подбородком на берег под лучом. — Дальше меня не бывает». Говорит она это так, как говорят «я живу за мостом».",
            "\"From as far as it reaches.\" She points with her chin at the shore under the beam. \"Beyond that there's none of me.\" She says it the way people say \"I live past the bridge\".",
            "「とどくところからだよ」と、あごで光の下の岸をさす。「その先に、わたしはいないのさ」。「橋の向こうに住んでるよ」と言うのと同じ調子で。")])
c("Позвать на чай", "Invite her in for tea", "お茶にさそう", "tea_ready", go="tea")
c("Позвать на чай", "Invite her in for tea", "お茶にさそう", "!tea_ready",
  says=[say("Она заглядывает тебе за плечо. «У тебя чайник холодный. Ты поставь. Я подожду. Я умею».",
            "She looks past your shoulder. \"Your kettle's cold. You put it on. I'll wait. I know how.\"",
            "おばあさんはきみの肩ごしに中をのぞく。「やかんが冷たいじゃないか。火にかけておいで。待ってるから。待つのは得意だよ」")])
c("Вернуться в комнату", "Go back into the room", "部屋に戻る", go="watch")

node("tea")
t("Она садится на край стула, банку ставит на стол, под свет. Чай пьёт из блюдца, мелко, и каждый раз, когда луч проходит по потолку, поднимает глаза и ждёт, пока пройдёт.",
  "She sits on the edge of the chair and puts the jar on the desk, under the light. She drinks her tea from the saucer in small sips, and each time the beam crosses the ceiling she raises her eyes and waits for it to pass.",
  "おばあさんはいすの端にすわり、びんを机の、光の当たるところに置く。お茶は受け皿から少しずつ飲む。光が天井を横切るたびに目を上げ、通りすぎるまで待つ。")
t("«Ты думаешь, ты её сторожишь, — говорит она. — Лампу не сторожат. Возле неё греются. Мы все. Ты тоже». Потом, уже допив: «А утром ты что делаешь?»",
  "\"You think you're guarding it,\" she says. \"Nobody guards a lamp. You warm yourself beside it. All of us. You too.\" Then, her tea finished: \"And what do you do in the morning?\"",
  "「あんたは、あれの番をしているつもりだろう」とおばあさんは言う。「ランプの番なんて、だれもしないよ。そばであたたまるのさ。みんなね。あんたも」。飲み終えてから、こう聞く。「で、朝になったら、あんたは何をするんだい」")
c("«Сдаю вахту и ложусь спать»", "\"Hand over the watch and go to bed\"", "「引き継いで、寝ます」",
  says=[say("«Каждый раз, — соглашается она. — Каждый раз сдаёшь и ложишься. А ты хоть раз видел, кому сдаёшь?»",
            "\"Every time,\" she agrees. \"Every time you hand over and go to bed. Have you ever once seen who you hand over to?\"",
            "「毎回ね」とおばあさんはうなずく。「毎回、引き継いで寝る。で、だれに引き継いでいるのか、一度でも見たことがあるかい」"),
        say("Она встаёт, поправляет платок и уходит, не прощаясь, как уходят до завтра. Банка остаётся на столе. В ней, на самом дне, немного света — столько, сколько натекло за чаем.",
            "She gets up, straightens her scarf and leaves without a goodbye, the way people leave till tomorrow. The jar stays on the desk. In it, at the very bottom, is a little light: as much as ran in over tea.",
            "おばあさんは立ち上がり、スカーフを直し、さよならも言わずに出ていく。あしたまた来る人のように。びんは机に残る。いちばん底に、少しだけ光がたまっている。お茶のあいだに流れこんだぶんだけ。")],
  set=["tea_done"], go="watch")
c("«Не помню»", "\"I don't remember\"", "「おぼえていません」",
  says=[say("«Вот и я, — говорит она с облегчением. — Ни одного утра не помню. Значит, что-то там хорошее: плохое бы запомнилось».",
            "\"Nor do I,\" she says with relief. \"Not one morning do I remember. So there must be something good there: the bad would have stuck.\"",
            "「わたしもだよ」と、おばあさんはほっとしたように言う。「朝のことは、ひとつもおぼえていない。ということは、きっといいものがあるんだ。悪いことならおぼえているはずだからね」"),
        say("Она встаёт, поправляет платок и уходит, не прощаясь, как уходят до завтра. Банка остаётся на столе. В ней, на самом дне, немного света — столько, сколько натекло за чаем.",
            "She gets up, straightens her scarf and leaves without a goodbye, the way people leave till tomorrow. The jar stays on the desk. In it, at the very bottom, is a little light: as much as ran in over tea.",
            "おばあさんは立ち上がり、スカーフを直し、さよならも言わずに出ていく。あしたまた来る人のように。びんは机に残る。いちばん底に、少しだけ光がたまっている。お茶のあいだに流れこんだぶんだけ。")],
  set=["tea_done"], go="watch")

# ------------------------------------------------------------------ outside
node("shore_dark")
t("Ты выходишь. Под ногой есть ступенька — ровно одна, та, на которую ты наступил. Про следующую ничего нельзя сказать заранее.",
  "You go out. There is a step under your foot: exactly one, the one you have stepped on. Nothing can be said in advance about the next.",
  "外に出る。足の下に段がある。ちょうど一段、いま踏んだ段だけ。次の段については、前もって何も言えない。")
t("Ни моря, ни берега не слышно. За спиной жёлтая щель двери. Впереди даже не темно — впереди никак.",
  "Neither sea nor shore can be heard. Behind you is the yellow crack of the door. Ahead it isn't even dark; ahead it isn't anything.",
  "海の音も岸の音も聞こえない。うしろには扉の黄色いすきま。前は暗いのですらない。前は、何でもない。")
c("Идти дальше", "Walk on", "先へ歩く", go="dark_walk")
c("Вернуться в башню", "Go back into the tower", "塔に戻る", go="watch")

node("dark_walk")
t("Шаг. Под ним есть на что встать. Ещё шаг. Щель двери за спиной стала тонкой, как нитка. Ты замечаешь, что перестал чувствовать, какие у тебя руки: длинные, короткие, чьи.",
  "A step. There is something under it to stand on. Another step. The crack of the door behind you has grown thin as a thread. You notice you have stopped feeling what your hands are like: long, short, whose.",
  "一歩。その下には、立てるものがある。もう一歩。うしろの扉のすきまは糸のように細くなった。自分の手がどんな手なのか、感じられなくなっているのに気づく。長いのか、短いのか、だれのものなのか。")
c("Ещё дальше", "Further still", "もっと先へ", end="dark",
  says=[say("Ещё шаг. Нитка за спиной рвётся без звука. Ты идёшь, и некому сказать, далеко ли ты ушёл и ты ли это идёшь. Лампа в эту ночь не горела. На берегу этого никто не заметил: замечать было некому.",
            "One more step. The thread behind you breaks without a sound. You walk, and there is nobody to say how far you have gone, or whether it is you walking. The lamp did not burn that night. Nobody on the shore noticed: there was nobody to notice.",
            "もう一歩。うしろの糸が音もなく切れる。きみは歩く。どこまで来たのか、歩いているのがきみなのか、言ってくれる者はいない。その夜、ランプは灯らなかった。岸では、だれも気づかなかった。気づく者がいなかったからだ。"),
        say("КОНЕЦ. Вахта не сдана.", "THE END. The watch was not handed over.", "おわり。当直は引き継がれなかった。")])
c("Назад, на ощупь", "Back, feeling your way", "手さぐりで戻る",
  says=[say("Поворачиваешься и идёшь на нитку света. Ступеньки под ногами появляются неохотно, но появляются. В башне пахнет керосином, и ты удивляешься, как это хорошо.",
            "You turn and walk toward the thread of light. The steps appear under your feet reluctantly, but they appear. The tower smells of paraffin, and you are surprised at how good that is.",
            "向きを変え、光の糸をめざして歩く。足の下に段は、しぶしぶながら現れる。塔の中は石油のにおいがして、それがこんなにいいものかと、きみはおどろく。")],
  go="watch")

node("shore_night")
t("Берег под лучом. Камни, причал, у причала в воду уходят рельсы и блестят под ней, как две мокрые нитки. От двери к городу идут следы маленьких ног.",
  "The shore under the beam. Stones, a pier; by the pier rails go down into the water and shine beneath it like two wet threads. Small footprints lead from the door toward the town.",
  "光の下の岸。石、桟橋。桟橋のわきで線路が水に入り、水の下で二本のぬれた糸のように光っている。扉から町のほうへ、小さな足あとが続いている。")
c("Потрогать рельс", "Touch the rail", "線路にさわる",
  says=[say("Опускаешь руку в воду. Рельс тёплый — как перила, на которых весь день лежало солнце.",
            "You put your hand in the water. The rail is warm, like a handrail the sun has lain on all day.",
            "水に手を入れる。線路はあたたかい。一日じゅう日が当たっていた手すりのように。")])
c("Пойти по следам", "Follow the footprints", "足あとをたどる",
  says=[say("Следы идут ровно, не торопясь, и кончаются посреди дороги — там, где сейчас край луча. Когда луч возвращается, следов на шаг больше.",
            "The footprints go evenly, unhurried, and end in the middle of the road: where the edge of the beam is now. When the beam comes back, there is one footprint more.",
            "足あとは急がず、きちんと続き、道の真ん中で終わる。いま光のふちがあるところで。光が戻ってくると、足あとは一歩ぶん増えている。")])
c("Оглянуться на башню", "Look back at the tower", "塔をふり返る",
  says=[say("Снаружи башня меньше, чем изнутри. Сорок ступеней в неё не поместились бы. Тридцать девять — может быть.",
            "From outside the tower is smaller than from inside. Forty steps wouldn't fit in it. Thirty-nine, perhaps.",
            "外から見る塔は、中から見るより小さい。四十段は入りそうにない。三十九段なら、あるいは。")])
c("Вернуться в башню", "Go back into the tower", "塔に戻る", go="watch")

node("sleep")
t("Ты кладёшь голову на журнал. Клеёнка прохладная. Сверху не идёт никакого света, и от этого в комнате очень спокойно — подозрительно спокойно, как в доме, из которого все ушли.",
  "You lay your head on the log book. The oilcloth is cool. No light at all comes from above, and that makes the room very peaceful: suspiciously peaceful, like a house everyone has left.",
  "日誌に頭をのせる。油布はひんやりしている。上からは少しも光が来ず、そのせいで部屋はとても安らかだ。みんなが出ていった家のように、あやしいほど安らかだ。")
c("Закрыть глаза", "Close your eyes", "目を閉じる", end="dark",
  says=[say("Ты закрываешь глаза и считаешь, чтобы уснуть: десять, девять, восемь… До нуля ты не доходишь. Лампа в эту ночь не горела. Утром некому было проверить, наступило ли утро.",
            "You close your eyes and count yourself to sleep: ten, nine, eight… You don't get to zero. The lamp did not burn that night. In the morning there was nobody to check whether morning had come.",
            "目を閉じ、眠るために数える。十、九、八……。ゼロまではたどりつかない。その夜、ランプは灯らなかった。朝になっても、朝が来たかどうかを確かめる者はいなかった。"),
        say("КОНЕЦ. Вахта не сдана.", "THE END. The watch was not handed over.", "おわり。当直は引き継がれなかった。")])
c("Встать", "Get up", "立ち上がる", go="watch",
  says=[say("Нет. Сначала лампа. Щека успела запомнить узор клеёнки.",
            "No. The lamp first. Your cheek has had time to learn the pattern of the oilcloth.",
            "だめだ。まずランプ。ほおは、もう油布の模様をおぼえてしまった。")])

# ------------------------------------------------------------------ morning
node("predawn")
t("Предрассветный час. Окно сереет. Свет лампы на потолке стал бледнее — не потому что она слабеет, а потому что у неё появился соперник.",
  "The hour before dawn. The window turns grey. The lamp's light on the ceiling has grown paler: not because the lamp is weakening, but because it has a rival now.",
  "夜明け前。窓が白んでくる。天井のランプの光はうすくなった。ランプが弱ったからではない。競う相手が現れたからだ。")
t("Журнал лежит открытый на сегодняшней ночи. Осталось расписаться.",
  "The log book lies open at tonight. All that's left is to sign.",
  "日誌は今夜のページを開いたまま置いてある。あとは署名するだけだ。")
t("У тебя чувство, что за ночь ты чего-то не досмотрел или не дослушал. Но ночь уже не переспросишь.",
  "You have the feeling there was something you didn't see through or hear out tonight. But you can't ask the night to repeat itself.",
  "この夜のうちに、何かを見とどけなかったか、聞きとどけなかった気がする。けれど、夜に聞き返すことは、もうできない。", NOT_ALL)
t("Банка на столе светится со дна. Наверху гудит лампа. И тебе впервые за ночь не хочется просто расписаться.",
  "The jar on the desk glows from its bottom. The lamp hums upstairs. And for the first time tonight you don't feel like simply signing.",
  "机のびんは底から光っている。上ではランプがうなっている。そしてこの夜はじめて、ただ署名して終わりにする気になれない。", ALL)
c("Расписаться в журнале", "Sign the log book", "日誌に署名する", go="signoff")
c("Подняться к лампе с банкой", "Go up to the lamp with the jar", "びんを持ってランプへ上がる", ALL, go="lastlamp")

node("signoff")
t("Ты пишешь: «Лампа капризничала. Починил. Стучали. Открыл. Вахту сдал». И ставишь инициал. Рука выводит «Д.» раньше, чем ты успеваешь подумать, какой он у тебя.",
  "You write: \"The lamp was difficult. Fixed it. Someone knocked. Opened. Handed over.\" And you put your initial. The hand writes \"D.\" before you have time to think what yours is.",
  "きみは書く。「ランプは気まぐれ。直した。ノックあり。開けた。引き継いだ」。そして頭文字を入れる。自分の頭文字が何だったか考えるより先に、手が「D」と書いている。")
t("В конце журнала не хватает страницы. Ты решаешь об этом не писать.",
  "A page is missing at the end of the log book. You decide not to write about that.",
  "日誌のうしろのページが一枚足りない。それについては書かないことにする。", "tore")
c("Перечитать записку сменщика", "Read the day keeper's note again", "昼の番人のメモを読み返す",
  says=[say("«Лампа капризничает. Манометру не верь. Чай в жестянке. Если постучат — открой. Д.» Хвост у «у» загнут, как рыболовный крючок. Ты кладёшь записку обратно на то же место. Она ещё пригодится.",
            "\"The lamp is being difficult. Don't trust the gauge. Tea is in the tin. If someone knocks, open. D.\" Every \"t\" ends in a curl like a fish hook. You put the note back in the same place. It will come in useful yet.",
            "「ランプは気まぐれ。圧力計は信じるな。お茶は缶の中。ノックがあったら開けること。D」。「ぐ」の字は釣り針のように曲がっている。メモを同じ場所に戻す。まだ使うものだから。")])
c("Лечь спать", "Go to bed", "寝る", end="keeper",
  says=[say("Койка за печкой узкая и помнит форму твоей спины. Ты засыпаешь на счёте «семь».",
            "The bunk behind the stove is narrow and remembers the shape of your back. You fall asleep on the count of seven.",
            "ストーブのうしろの寝台はせまく、きみの背中の形をおぼえている。「七」まで数えたところで眠る。"),
        say("Вечер. Ты принимаешь вахту на посту «Фонарь». Сменщика ты не застаёшь. На столе записка знакомым почерком: «Лампа капризничает…»",
            "Evening. You take over the watch at the Lantern post. You don't catch the day keeper. On the desk is a note in a familiar hand: \"The lamp is being difficult…\"",
            "夕方。きみは「灯」の番所で当直を引き継ぐ。昼の番人には会えない。机には、見おぼえのある字のメモ。「ランプは気まぐれ……」"),
        say("КОНЕЦ. Вахту сдал. Вахту принял.", "THE END. Watch handed over. Watch taken.", "おわり。当直を渡した。当直を受けとった。")])

node("lastlamp")
t("Ламповая на рассвете. За стёклами небо над морем светлеет с одного края, и ты вдруг видишь: этот свет и свет лампы — одного цвета. Не похожего. Одного.",
  "The lamp room at dawn. Beyond the glass the sky over the sea is growing light from one edge, and suddenly you see it: that light and the lamp's light are the same colour. Not a similar one. The same.",
  "夜明けのランプ室。ガラスの向こうで、海の上の空が片方の端から明るくなっていく。そしてふいに分かる。あの光とランプの光は、同じ色だ。似た色ではない。同じ色だ。")
t("Кораблей за вахту: ноль. Кораблей не было ни разу. Лампа не для них. Она для берега: чтобы всю ночь было где стоять домам и кому пить чай. Она держит место до утра — как держат место в очереди для того, кто сейчас подойдёт.",
  "Ships this watch: none. There never were any ships. The lamp isn't for them. It is for the shore: so that all night the houses have somewhere to stand and someone is there to drink tea. It keeps the place until morning, the way you keep a place in a queue for someone who is just coming.",
  "当直中の船、ゼロ。船など一度も来なかった。ランプは船のためのものではない。岸のためのものだ。夜のあいだ、家々に立つ場所があり、お茶を飲むだれかがいるように。ランプは朝まで場所を取っておく。もうすぐ来る人のために、列の場所を取っておくように。")
t("И ты вспоминаешь, чем кончалась съеденная молью строчка. «Утром главное не забудь — погаси сам».",
  "And you remember how the moth-eaten line ended. \"In the morning the main thing, don't forget: put it out yourself.\"",
  "そして、虫に食われたあの行の終わりを思い出す。「朝、いちばん大事なのは、忘れずに——自分の手で消すこと」")
c("Погасить лампу самому", "Put the lamp out yourself", "自分の手でランプを消す", go="dawn_out")
c("Оставить гореть и спуститься", "Leave it burning and go down", "灯したまま下りる", go="signoff",
  says=[say("Рука не поднимается. Столько возни с ней за ночь — и гасить? Ты спускаешься. Лампа гудит тебе в спину ровно и, кажется, с пониманием.",
            "Your hand won't rise to it. All that trouble with it through the night, and now put it out? You go down. The lamp hums at your back evenly and, it seems, with understanding.",
            "手が上がらない。ひと晩あれだけ手を焼いて、それを消すのか。きみは下りる。背中でランプが、むらなく、そしてどうやら分かってくれた様子で、うなっている。")])

node("dawn_out")
t("Ты берёшься за винт фитиля и убавляешь — медленно, считая обороты. Десять. Девять. Восемь. С каждым оборотом что-то отпускает: манометр, вентиль, стук в дверь, сорок ступеней. Пять. Тебя самого становится меньше, и это не страшно. Три. Два. Один.",
  "You take hold of the wick screw and turn it down: slowly, counting the turns. Ten. Nine. Eight. With every turn something lets go: the gauge, the valve, the knock at the door, the forty steps. Five. There is less of you yourself, and it isn't frightening. Three. Two. One.",
  "芯のねじを握り、しぼっていく。ゆっくりと、回す数を数えながら。十。九。八。回すたびに何かが手を離れていく。圧力計、バルブ、扉のノック、四十の段。五。きみ自身も少なくなっていくが、こわくはない。三。二。一。")
t("Ноль. Лампа гаснет. И ничего не меняется: свет остаётся. Он просто больше не из лампы. Причал на месте. Крыши на месте. Кошка на заборе потягивается — вся, целиком, без всяких пол-оборотов.",
  "Zero. The lamp goes out. And nothing changes: the light stays. It is simply no longer from the lamp. The pier is in place. The roofs are in place. The cat on the fence stretches: all of it, the whole cat, with no half turns about it.",
  "ゼロ。ランプが消える。そして何も変わらない。光は残っている。ただ、もうランプからの光ではないだけだ。桟橋はある。屋根もある。へいの上の猫がのびをする。まるごと、全身で。半回りなどということはなしに。")
c("Постоять ещё", "Stand a little longer", "もう少し立っている",
  says=[say("Стёкла ламповой тёплые с обеих сторон. Мотыльки на раме складывают крылья: смена кончилась и у них.",
            "The lamp room's glass is warm on both sides. The moths on the frame fold their wings: their shift is over too.",
            "ランプ室のガラスは、内も外もあたたかい。枠の蛾が羽をたたむ。彼らの当直も終わったのだ。")])
c("Выйти наружу", "Go outside", "外に出る", end="dawn",
  says=[say("Ты спускаешься — тридцать девять ступеней — и выходишь за дверь, не расписавшись. Снаружи утро. На берегу стоят люди, немного, человек десять, и все смотрят в одну сторону — туда, откуда оно поднимается. Старушка в платке тоже там. Она не оборачивается, но подвигается, чтобы тебе было где встать.",
            "You go down, thirty-nine steps, and walk out of the door without signing. Outside it is morning. There are people standing on the shore, not many, ten or so, and all of them are looking one way: to where it is rising. The old woman in the headscarf is there too. She doesn't turn round, but she moves over so that you have somewhere to stand.",
            "きみは下りる。三十九段。そして署名をしないまま扉を出る。外は朝だ。岸に人が立っている。多くはない、十人ほど。みな同じほうを見ている。それがのぼってくるほうを。スカーフのおばあさんもいる。ふり返りはしないが、きみの立つ場所をあけてくれる。"),
        say("Банка у тебя в руках полна доверху. Ты снимаешь крышку. Ничего не происходит: то, что было в банке, и то, что снаружи, — одно и то же. Лицу тепло. Ты стоишь со всеми и не оглядываешься проверить, на месте ли башня.",
            "The jar in your hands is full to the top. You take off the lid. Nothing happens: what was in the jar and what is outside are one and the same. Your face is warm. You stand with the others and don't look back to check whether the tower is still there.",
            "手の中のびんは、口までいっぱいだ。ふたを取る。何も起こらない。びんの中にあったものと外にあるものは、同じひとつのものだから。顔があたたかい。きみはみんなといっしょに立ち、塔がまだあるかどうか、ふり返って確かめたりはしない。"),
        say("КОНЕЦ. Вахту сдал. Принимать некому — и не нужно.",
            "THE END. Watch handed over. There is nobody to take it, and no need.",
            "おわり。当直を渡した。受けとる者はいない。いなくていい。")])

# ------------------------------------------------------------------ write
for n in NODES.values():
    for ch in n["choices"]:
        pass
data = collections.OrderedDict()
data["_doc"] = "«Ночная вахта», the text adventure of the lab's ATON machines. Format: Source/Lyudmilovsk/Private/AtonOS/Lab/LabAdventure.h; words: tools/aton/strings/adventure.json; checks: tools/atonsim/adventure_test.cpp. Flags: has_pencil read_old took_match has_match knows_hand has_wheel valve pumped shut tore lit heard tea_ready opened tea_done saw_edge."
data["title"] = "ui.aton.adv.title"
data["start"] = "start"
data["best"] = "dawn"
data["nodes"] = NODES

def dump_nodes(d):
    out = ["{"]
    out.append(' "_doc": %s,' % json.dumps(d["_doc"], ensure_ascii=False))
    out.append(' "title": "%s",\n "start": "%s",\n "best": "%s",' % (d["title"], d["start"], d["best"]))
    out.append(' "nodes": {')
    items = list(d["nodes"].items())
    for i, (nid, n) in enumerate(items):
        out.append('  "%s": {' % nid)
        out.append('   "text": [%s],' % ", ".join(json.dumps(x, ensure_ascii=False) for x in n["text"]))
        if "set" in n:
            out.append('   "set": %s,' % json.dumps(n["set"]))
        out.append('   "choices": [')
        for j, ch in enumerate(n["choices"]):
            out.append('    %s%s' % (json.dumps(ch, ensure_ascii=False), "," if j + 1 < len(n["choices"]) else ""))
        out.append('   ]')
        out.append('  }%s' % ("," if i + 1 < len(items) else ""))
    out.append(' }')
    out.append('}')
    return "\n".join(out) + "\n"

open(ROOT + "/unreal/Lyudmilovsk/Content/AtonOS/adventure.json", "w", encoding="utf-8").write(dump_nodes(data))
with open(ROOT + "/tools/aton/strings/adventure.json", "w", encoding="utf-8") as f:
    f.write("{\n")
    items = list(S.items())
    for i, (k, v) in enumerate(items):
        f.write(' %s: {"ru": %s, "en": %s, "ja": %s}%s\n' % (json.dumps(k), json.dumps(v["ru"], ensure_ascii=False), json.dumps(v["en"], ensure_ascii=False), json.dumps(v["ja"], ensure_ascii=False), "," if i + 1 < len(items) else ""))
    f.write("}\n")
long = [(k, len(v["ru"])) for k, v in S.items() if (".c" in k.rsplit(".", 1)[1] and len(v["ru"]) > 60) or len(v["ru"]) > 330]
print(len(NODES), "nodes", len(S), "strings", sum(len(n["choices"]) for n in NODES.values()), "choices", "too long:", long)
