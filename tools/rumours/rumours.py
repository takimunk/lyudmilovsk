#!/usr/bin/env python3
"""The talk of the town about the hero (docs/town_rumours.md, backlog item 22): which game events become rumours, how far
and how long each kind goes round, and the words the town uses for it outside the townsfolk's own lines (the paper's
column, a P.S. in the townsfolk's letters, the staff's answer to "what's the talk of the town?"). Russian is the source.

    python3 tools/rumours/rumours.py            # write
    python3 tools/rumours/rumours.py --check    # only check

Writes unreal/Lyudmilovsk/Content/PeopleData/rumours.json (read by ULyudTownRumours, model Private/People/TownRumours.h,
tested by tools/barksim/run.sh rumours) and unreal/Lyudmilovsk/Content/Localization/rumours/strings_<lang>.json.
The townsfolk's remarks (heard_of = <kind>) are in tools/barks/lines/15_rumours.txt (build_barks.py checks the kinds).
"""
import collections
import json
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
OUT = os.path.join(CONTENT, "PeopleData", "rumours.json")
OUT_STRINGS = os.path.join(CONTENT, "Localization", "rumours")
# the staff's topic goes through the interiors' pipeline (tools/city/interiors.py writes Localization/city and interiors.json)
OUT_CITY = os.path.join(ROOT, "tools", "city", "content", "x_rumours.json")
OUT_CITY_STRINGS = os.path.join(ROOT, "tools", "city", "content", "strings", "x_rumours.json")
LANGS = ("ru", "en", "ja")

# The kinds. hops: how far from those who saw it; life: game days (the day itself is day 0); pass: the chance a tie passes it
# on; seed: residents who know it at once though none of them was there (the train's passengers); news: the share of the town
# that hears it anyway the next day (the paper), news_today: the same day (the radio); once: the same detail is news only
# once ever; rank: which the paper's column and the letters pick first.
KINDS = collections.OrderedDict([
    ("cup",    {"hops": 4, "life": 5, "pass": 0.7, "news": 0.35, "rank": 9}),
    ("garden", {"hops": 3, "life": 5, "pass": 0.6, "news": 0.3, "rank": 8}),
    ("beat",   {"hops": 3, "life": 4, "pass": 0.6, "rank": 7}),
    ("cure",   {"hops": 3, "life": 5, "pass": 0.6, "seed": 4, "once": 1, "rank": 6}),
    ("job",    {"hops": 3, "life": 4, "pass": 0.6, "once": 1, "rank": 6}),
    ("tamed",  {"hops": 3, "life": 5, "pass": 0.55, "once": 1, "rank": 5}),
    ("radio",  {"hops": 1, "life": 3, "pass": 0.5, "news": 0.4, "news_today": 1, "rank": 5}),
    ("fine",   {"hops": 2, "life": 3, "pass": 0.55, "seed": 3, "rank": 4}),
    ("crows",  {"hops": 2, "life": 4, "pass": 0.55, "rank": 4}),
    ("catch",  {"hops": 2, "life": 3, "pass": 0.5, "rank": 3}),
    ("kite",   {"hops": 2, "life": 3, "pass": 0.5, "rank": 3}),
    ("tune",   {"hops": 2, "life": 3, "pass": 0.5, "rank": 3}),
    ("sing",   {"hops": 2, "life": 2, "pass": 0.5, "rank": 2}),
    ("film",   {"hops": 1, "life": 2, "pass": 0.5, "rank": 1}),
    ("dance",  {"hops": 2, "life": 3, "pass": 0.55, "rank": 4}),
])

# Game events (ULyudQuests::Notify) -> a kind; the first rule that matches wins. target: "*", "prefix*" or the target itself;
# detail: the string key shown for {1} ({t} the target, {p} its part after '/'), "" none.
RULES = [
    {"event": "cup_won", "target": "*", "kind": "cup", "detail": "city.coll.cups.{t}"},
    {"event": "beat", "target": "npc.*", "kind": "beat", "detail": "{t}"},
    {"event": "do", "target": "garden_contest_place", "kind": "garden", "detail": ""},
    {"event": "stamp_set", "target": "san_course", "kind": "cure", "detail": ""},
    {"event": "stamp", "target": "work_book/*", "kind": "job", "detail": "rumour.job.{p}"},
    {"event": "bird_tamed", "target": "*", "kind": "tamed", "detail": "bird.{t}.name"},
    {"event": "radio", "target": "request_heard", "kind": "radio", "detail": ""},
    {"event": "fined", "target": "*", "kind": "fine", "detail": ""},
    {"event": "corvid_gift", "target": "*", "kind": "crows", "detail": ""},
    {"event": "fish_caught", "target": "*", "kind": "catch", "detail": "item.{t}.name"},
    {"event": "stamp", "target": "kites/*", "kind": "kite", "detail": ""},
    {"event": "play_tune", "target": "bonfire", "kind": "sing", "detail": ""},
    {"event": "play_tune", "target": "*", "kind": "tune", "detail": "music.tune.{t}"},
    {"event": "stamp", "target": "films/*", "kind": "film", "detail": ""},
    # a dance at the Vaults (docs/dances.md): the partners are regulars under names of their own (city.dnc.p.*), so their
    # "beat" is a dance, not a game won
    {"event": "beat", "target": "city.dnc.p.*", "kind": "dance", "detail": "{t}"},
]

SETTINGS = {
    "far_m": 200,        # friends whose homes are further apart than this hear it from each other a day later
    "big_group": 12,     # a workplace with more people than this (a school) ties each only to group_links of the others
    "group_links": 4,
    "max_rumours": 16,
    "fresh": [1.0, 0.8, 0.5, 0.3],   # how likely somebody brings it up, by its age in days
    "witness_m": 30,     # whoever is this close to the hero when it happens knows it at once
    "remark_every_s": 150,   # the game's pause between two remarks about the hero anywhere (calm and rare)
}

# The jobs of the work book (x_jobs.json), as a place: "you work {1}".
JOBS = {
    "kiosk": ("в газетном киоске", "at the newspaper kiosk", "新聞キオスクで"),
    "market": ("на рынке", "at the market", "市場で"),
    "lifeguard": ("на спасательной станции", "at the lifeguard station", "救助所で"),
    "lighthouse": ("на маяке", "at the lighthouse", "灯台で"),
    "cafe": ("в кафе", "at the café", "カフェで"),
    "cinema": ("в «Рассвете»", "at the Rassvet", "『ラスヴェート』で"),
}

# The paper's column «Говорят в городе» (the town page): the talk of the day before, the kinds the front page doesn't already
# print (cups, tamed birds and catches have their own front stories). {0} the detail.
PAPER_TITLE = ("ГОВОРЯТ В ГОРОДЕ", "THE TALK OF THE TOWN", "町のうわさ")
PAPER = {
    "beat": [
        ("Говорят, школьник обыграл одного из завсегдатаев. {0} просит реванша и уже расставляет фигуры.",
         "They say a schoolboy beat one of the regulars. {0} wants a rematch and is already setting up the pieces.",
         "少年が常連のひとりを負かしたそうだ。{0}は雪辱戦を望み、もう駒を並べている。"),
        ("В клубе обсуждают партию с мальчиком. {0} говорит, что такого проигрыша не было весь год, и это комплимент.",
         "The club is discussing a game with a boy. {0} says there hasn't been a loss like it all year, and means it as a compliment.",
         "クラブでは少年との対局の話でもちきり。{0}は「今年いちばんの負けっぷり」と言う。ほめ言葉だそうだ。"),
    ],
    "garden": [
        ("Конкурс огородов: среди призёров — школьник с участка у Террас. Соседи по грядкам просят поделиться секретом.",
         "The allotment contest: among the prize-winners is a schoolboy with a plot by the Terraces. His bed neighbours want the secret.",
         "菜園コンテスト：入賞者のひとりはテラス脇の区画の少年。隣の畑の人たちが秘訣を知りたがっている。"),
    ],
    "cure": [
        ("В санатории говорят о юном отдыхающем: прошёл курс до последней печати, ни одной процедуры не пропустил.",
         "The sanatorium talks of a young guest who saw his course through to the last stamp without missing a single treatment.",
         "サナトリウムでは若い保養客の話題：最後のスタンプまで療養を続け、治療を一度も休まなかった。"),
    ],
    "job": [
        ("У нас новый работник: школьник подрабатывает {0}. Говорят, справляется.",
         "A new worker in town: a schoolboy has taken a job {0}. They say he's managing.",
         "町に新しい働き手：少年が{0}アルバイトを始めた。なかなかの働きぶりだそうだ。"),
    ],
    "radio": [
        ("«Волна Людмиловска» сыграла заявку юного слушателя. Город слушал и подпевал.",
         "“Volna Lyudmilovska” played a young listener's request. The town listened and sang along.",
         "「ヴォルナ・リュドミロフスカ」が若い聴取者のリクエストを流した。町じゅうが聴き、口ずさんだ。"),
    ],
    "fine": [
        ("В поезде оштрафовали безбилетника. Говорят, школьник. Кассы на платформах работают, пять рублей.",
         "A fare dodger was fined on the train. A schoolboy, they say. The platform ticket boxes work: five roubles.",
         "列車で無賃乗車が罰金を取られた。少年だという話。ホームの券売機は動いている。五ルーブル。"),
    ],
    "crows": [
        ("Вороны, говорят, носят одному мальчику подарки: пуговицы, крышечки, блестящие ключи. Хозяев ключей просим не волноваться.",
         "The crows, they say, bring a certain boy presents: buttons, bottle caps, shiny keys. Owners of keys are asked not to worry.",
         "カラスがある少年に贈り物を運ぶそうだ。ボタン、王冠、光る鍵。鍵の持ち主はご心配なく。"),
    ],
    "kite": [
        ("Над обрывом видели змея выше всех. Запускал школьник; катушку, говорят, держал крепко.",
         "A kite higher than all the others was seen over the cliff. A schoolboy flew it; he held the reel tight, they say.",
         "崖の上で、どれよりも高い凧が見られた。揚げていたのは少年。糸巻きをしっかり握っていたそうだ。"),
    ],
    "tune": [
        ("По вечерам кто-то играет «{0}». Говорят, школьник учится сам, по слуху.",
         "Someone plays “{0}” in the evenings. A schoolboy teaching himself by ear, they say.",
         "夕方になると誰かが「{0}」を弾いている。少年が耳で覚えて独学しているそうだ。"),
    ],
    "sing": [
        ("У костра на пляже пели под гитару. Играл, говорят, мальчик из клуба.",
         "They sang to a guitar by the bonfire on the beach. A boy from the club played, they say.",
         "浜のたき火でギターに合わせて歌った。弾いていたのはクラブの少年だそうだ。"),
    ],
    "film": [
        ("В «Рассвете» на старой хронике смеялся весь зал, громче всех — школьник в третьем ряду.",
         "At the old newsreel in the Rassvet the whole hall laughed, loudest of all a schoolboy in the third row.",
         "『ラスヴェート』の古いニュース映画で客席じゅうが笑った。いちばん大きな声は三列目の少年。"),
    ],
    "dance": [
        ("На субботних танцах в «Сводах» новенький школьник не пропустил ни одного вальса. Партнёрша — {0} — говорит, что он ни разу не наступил ей на ногу.",
         "At the Saturday dance at the Vaults a new schoolboy didn't miss a single waltz. His partner, {0}, says he never once trod on her foot.",
         "土曜の「ヴォールト」のダンスで、新顔の少年がワルツを一曲も逃さなかった。相手の{0}によれば、一度も足を踏まなかったそうだ。"),
    ],
}

PS_HEAD = ("P.S. ", "P.S. ", "追伸：")
# A P.S. in a townsperson's letter back to the hero (their reply to his postcard): the talk of the last days, any kind. {0} the detail.
PS = {
    "cup": ("А правда, что {0} теперь твой? Весь двор только об этом и говорит.",
            "Is it true {0} is yours now? The whole yard talks of nothing else.",
            "{0}、君のものになったって本当？近所じゅうその話でもちきりだよ。"),
    "garden": ("Слышали про твой огород и приз. Поздравляем! Пришли семян, если не жалко.",
               "We heard about your plot and the prize. Congratulations! Send some seeds if you can spare them.",
               "菜園の入賞、聞いたよ。おめでとう！よかったら種を分けて。"),
    "beat": ("Говорят, ты обыграл в клубе самого опытного. {0} до сих пор качает головой.",
             "They say you beat the most experienced player at the club. {0} is still shaking their head.",
             "クラブでいちばんの古株に勝ったんだって？{0}はまだ首をひねってるよ。"),
    "cure": ("Говорят, ты прошёл весь курс в санатории. Значит, теперь здоров за двоих.",
             "They say you finished the whole course at the sanatorium. So now you're healthy enough for two.",
             "サナトリウムの療養を全部終えたんだって？これで二人分元気だね。"),
    "job": ("Слышали, ты работаешь {0}. Молодец! Только про уроки не забывай.",
            "We heard you're working {0}. Well done! Just don't forget your lessons.",
            "{0}働いてるって聞いたよ。えらい！勉強も忘れずにね。"),
    "tamed": ("Говорят, у тебя новый друг с крыльями: {0}. Береги его.",
              "They say you have a new friend with wings: {0}. Take care of it.",
              "羽のある新しい友だちができたって？{0}だとか。大事にしてね。"),
    "radio": ("Мы слышали твою заявку по радио! Сделали погромче.",
              "We heard your request on the radio! We turned it up.",
              "ラジオで君のリクエスト、聴いたよ！音を大きくしたんだ。"),
    "fine": ("И ещё: говорят, тебя оштрафовали в поезде. Билет — пять рублей, не жадничай.",
             "And another thing: they say you got fined on the train. A ticket is five roubles, don't be stingy.",
             "それと、列車で罰金を取られたって？切符は五ルーブルだよ、けちらないで。"),
    "crows": ("Правда, что вороны носят тебе подарки? Если принесут нашу ложку, верни.",
              "Is it true the crows bring you presents? If they bring our spoon, send it back.",
              "カラスが贈り物を持ってくるって本当？うちのスプーンだったら返してね。"),
    "catch": ("Говорят, у тебя улов: {0}! Приходи как-нибудь, научишь.",
              "They say you've had a catch: {0}! Come round some time and teach us.",
              "{0}を釣ったんだって？今度来て、教えてよ。"),
    "kite": ("Видели твоего змея над обрывом. Выше всех!", "We saw your kite over the cliff. Higher than all the rest!",
             "崖の上の君の凧、見たよ。いちばん高かった！"),
    "tune": ("Говорят, ты играешь «{0}». Сыграешь нам, когда зайдёшь?", "They say you play “{0}”. Will you play it for us when you come?",
             "「{0}」が弾けるんだって？来たとき弾いてくれる？"),
    "sing": ("Слышали, как вы пели у костра. Жаль, нас там не было.", "We heard about the singing by the bonfire. A pity we weren't there.",
             "たき火のそばで歌ったんだって？行けなくて残念。"),
    "film": ("Видели тебя в «Рассвете». Как тебе старое кино?", "We saw you at the Rassvet. How did you like the old films?",
             "『ラスヴェート』で君を見かけたよ。古い映画はどうだった？"),
    "dance": ("Говорят, ты танцуешь в «Сводах», и партнёрша — {0}. Смотри, весь двор теперь знает.",
              "They say you dance at the Vaults, and your partner is {0}. Mind, the whole yard knows now.",
              "「ヴォールト」で踊ってるんだって？　相手は{0}。近所じゅうもう知ってるよ。"),
}

# The staff talk: a topic every counter offers while the town talks about the hero (tools/city/content/talk.json topic_sets
# "town_talk", added to every act with topics by interiors.py). {detail} is the rumour's detail. The topic's title first.
STAFF_TOPIC = ("Что говорят в городе?", "What's the talk of the town?", "町では何が話題？")
STAFF_INTRO = ("Что говорят? Да про тебя, между прочим.", "What's the talk? You, as it happens.", "話題？実は君のことだよ。")
STAFF = {
    "cup": ("{detail} — у школьника! Покупатели всё спрашивают, правда ли.",
            "{detail} won by a schoolboy! The customers keep asking if it's true.",
            "{detail}を少年が取ったって！お客さんがみんな本当かって聞くんだ。"),
    "garden": ("Говорят, твой огород взял приз. Тут уже спрашивали, не продаёшь ли рассаду.",
               "They say your plot won a prize. People have already asked here whether you sell seedlings.",
               "君の菜園が入賞したって話だよ。苗を売ってないかって、もう聞かれたよ。"),
    "beat": ("Говорят, ты вчера обыграл одного из наших. {detail} теперь каждый день тренируется.",
             "They say you beat one of our lot. {detail} practises every day now.",
             "うちの常連のひとりに勝ったんだって？{detail}はあれから毎日練習してるよ。"),
    "cure": ("Говорят, ты весь курс в санатории прошёл, до последней печати. Вот это выдержка.",
             "They say you did the whole course at the sanatorium, to the last stamp. That's stamina.",
             "サナトリウムの療養、最後のスタンプまでやり通したんだって？たいした根気だ。"),
    "job": ("Говорят, ты подрабатываешь {detail}. Если у нас место освободится, скажу.",
            "They say you've got a job {detail}. If a place comes up here, I'll tell you.",
            "{detail}働いてるって聞いたよ。うちに空きが出たら知らせるね。"),
    "tamed": ("Говорят, у тебя с руки ест птица: {detail}. Мои внуки теперь тоже семечки носят.",
              "They say a bird eats from your hand: {detail}. My grandchildren carry seeds about now too.",
              "君の手から鳥が食べるんだって？{detail}だとか。うちの孫まで種を持ち歩いてるよ。"),
    "radio": ("Слышали твою заявку по радио! Мы тут сделали погромче, покупатели подпевали.",
              "We heard your request on the radio! We turned it up in here and the customers sang along.",
              "ラジオで君のリクエスト、聴いたよ！店で音を上げたら、お客さんが口ずさんでた。"),
    "fine": ("Говорят, тебя в поезде оштрафовали. Тимофеич, небось? У него не забалуешь.",
             "They say you got fined on the train. Timofeich, I bet? You can't get anything past him.",
             "列車で罰金を取られたって？チモフェイチだろう。あの人はごまかせないよ。"),
    "crows": ("Говорят, вороны тебе подарки таскают. Ты им скажи, что у нас ложки считаны.",
              "They say the crows bring you presents. Tell them our spoons are counted.",
              "カラスが君に贈り物を運ぶんだって？うちのスプーンは数えてあるって言っといて。"),
    "catch": ("Говорят, у тебя улов: {detail}. Рыбаки с пирса уже ревнуют.",
              "They say you've had a catch: {detail}. The anglers on the pier are jealous already.",
              "{detail}を釣ったって話だよ。桟橋の釣り人たちがもうやきもちを焼いてる。"),
    "kite": ("Видели твоего змея над обрывом? Да все видели. Выше маяка, говорят.",
             "Did we see your kite over the cliff? Everyone did. Higher than the lighthouse, they say.",
             "崖の上の凧？みんな見たよ。灯台より高かったってさ。"),
    "tune": ("Говорят, ты играешь «{detail}». У нас тут тоже пианино было, да расстроилось.",
             "They say you play “{detail}”. We had a piano here too, but it went out of tune.",
             "「{detail}」が弾けるんだって？うちにもピアノがあったけど、調子が狂っちゃって。"),
    "sing": ("Говорят, вчера у костра пели под твою гитару. Жаль, у меня была смена.",
             "They say people sang to your guitar by the bonfire. A pity I was on shift.",
             "たき火で君のギターに合わせて歌ったんだって？仕事で行けなくて残念。"),
    "film": ("Говорят, ты ходил в «Рассвет» на старую хронику. Ну и как поезд, не страшно?",
             "They say you went to the old newsreel at the Rassvet. Well, the train: was it scary?",
             "『ラスヴェート』の古いニュース映画を見たんだって？で、列車は怖かった？"),
    "dance": ("Говорят, ты вчера на танцах был кавалером что надо. {detail} так и сказала подружкам.",
              "They say you were quite the gentleman at the dance. {detail} told her friends so.",
              "昨日のダンスでなかなかの騎士ぶりだったって？　{detail}が友だちにそう話してたよ。"),
}

# The kinds' short names (the dev panel and lyud.Rumours.Status).
NAMES = {
    "cup": ("кубок", "a cup won", "優勝"), "garden": ("приз за огород", "a garden prize", "菜園の入賞"),
    "beat": ("победа над завсегдатаем", "a regular beaten", "常連に勝利"), "cure": ("курс в санатории", "the sanatorium course", "療養の修了"),
    "job": ("подработка", "a job", "アルバイト"), "tamed": ("птица с руки", "a bird tamed", "鳥がなついた"),
    "radio": ("заявка на радио", "a radio request", "ラジオのリクエスト"), "fine": ("штраф в поезде", "a fine on the train", "列車で罰金"),
    "crows": ("подарки ворон", "the crows' presents", "カラスの贈り物"), "catch": ("улов", "a catch", "釣果"),
    "kite": ("змей", "a kite", "凧"), "tune": ("мелодия", "a tune", "曲"), "sing": ("песни у костра", "songs by the fire", "たき火の歌"),
    "film": ("старое кино", "the old films", "古い映画"),
    "dance": ("танцы в «Сводах»", "the dance at the Vaults", "「ヴォールト」のダンス"),
}

DEV = {
    "dev.action.rumour.cup": ("Слух: кубок у героя", "Rumour: the hero won a cup", "うわさ：主人公が優勝"),
    "dev.action.rumour.beat": ("Слух: обыграл завсегдатая", "Rumour: a regular beaten", "うわさ：常連に勝利"),
    "dev.action.rumour.status": ("Слухи: что говорят", "Rumours: the talk now", "うわさ：いまの話題"),
    "dev.action.rumour.clear": ("Слухи: забыть все", "Rumours: forget them all", "うわさ：すべて忘れる"),
}


def build():
    strings = collections.OrderedDict()

    def put(key, words):
        assert len(words) == 3 and all(w.strip() for w in words), key
        assert key not in strings, key
        strings[key] = words

    for k, w in NAMES.items():
        put("rumour.kind.%s" % k, w)
    for k, w in JOBS.items():
        put("rumour.job.%s" % k, w)
    put("rumour.paper.title", PAPER_TITLE)
    for k, variants in PAPER.items():
        for i, w in enumerate(variants, 1):
            put("rumour.paper.%s.%d" % (k, i), w)
    for k, w in PS.items():
        put("rumour.ps.%s" % k, (PS_HEAD[0] + w[0], PS_HEAD[1] + w[1], PS_HEAD[2] + w[2]))
    for k, w in DEV.items():
        put(k, w)
    data = collections.OrderedDict()
    data["_doc"] = "The talk of the town about the hero (docs/town_rumours.md). Written by tools/rumours/rumours.py: edit there."
    data["kinds"] = KINDS
    data["rules"] = RULES
    data.update(SETTINGS)
    data["paper"] = {k: len(v) for k, v in PAPER.items()}
    data["ps"] = sorted(PS)
    data["staff"] = sorted(STAFF)
    return data, strings


def staff():
    """The staff's topic for the interiors (content and strings, the answers in the staff talk's style: ru a dash, en quotes, ja 「」)."""
    def said(w):
        return {"ru": "— " + w[0], "en": "\"" + w[1] + "\"", "ja": "「" + w[2] + "」"}
    words = collections.OrderedDict()
    words["_doc"] = ("The staff's topic «Что говорят в городе?» (the talk of the town about the hero, docs/town_rumours.md). "
                     "Written by tools/rumours/rumours.py: edit there. {detail}: the rumour's detail (ULyudCityThings::LineText).")
    words["city.ask.town_talk"] = {"ru": STAFF_TOPIC[0], "en": STAFF_TOPIC[1], "ja": STAFF_TOPIC[2]}
    words["city.ask.town_talk.intro"] = said(STAFF_INTRO)
    lines = ["city.ask.town_talk.intro"]
    for k in KINDS:
        words["city.ask.town_talk.%s" % k] = said(STAFF[k])
        lines.append({"k": "city.ask.town_talk.%s" % k, "rumour": k})
    content = collections.OrderedDict()
    content["_doc"] = ("The staff's topic about the talk of the town (docs/town_rumours.md): offered by every counter with staff talk "
                       "while something about the hero goes round. Written by tools/rumours/rumours.py: edit there.")
    content["topic_sets"] = {"town_talk": [{"id": "town_talk", "k": "city.ask.town_talk", "everywhere": True, "rumour": "any", "lines": lines}]}
    return content, words


def check(data, strings):
    problems = []
    kinds = set(KINDS)
    for r in RULES:
        if r["kind"] not in kinds:
            problems.append("rule %s: unknown kind %s" % (r["event"], r["kind"]))
    for k in kinds:
        if k not in NAMES:
            problems.append("kind %s has no name" % k)
        if not any(r["kind"] == k for r in RULES):
            problems.append("kind %s: no rule makes it" % k)
        if k not in PS:
            problems.append("kind %s: no P.S." % k)
        if k not in STAFF:
            problems.append("kind %s: no staff line" % k)
    for k in list(PAPER) + list(PS) + list(STAFF):
        if k not in kinds:
            problems.append("words for unknown kind %s" % k)
    detail_kinds = {r["kind"] for r in RULES if r["detail"]}
    for key, words in strings.items():
        m = re.match(r"^(rumour\.paper|rumour\.ps)\.([a-z]+)", key)
        if not m:
            continue
        kind = m.group(2)
        token = "{0}"
        for w in words:
            if token in w and kind not in detail_kinds:
                problems.append("%s uses %s but %s has no detail" % (key, token, kind))
            if "{" in w.replace(token, ""):
                problems.append("%s: a stray brace" % key)
        # Russian: the speaker never agrees in gender about themselves (the hero is a boy: his forms are masculine)
        if "(а)" in words[0] and not key.startswith("rumour.paper"):
            problems.append("%s: a (а) form in Russian: say it without the speaker's gender" % key)
    for k, words in STAFF.items():
        for w in words:
            if "{detail}" in w and k not in detail_kinds:
                problems.append("staff line %s uses {detail} but %s has no detail" % (k, k))
            if "{" in w.replace("{detail}", ""):
                problems.append("staff line %s: a stray brace" % k)
        if "(а)" in words[0]:
            problems.append("staff line %s: a (а) form in Russian" % k)
    for key, words in strings.items():
        if key.startswith("rumour.paper") and "(а)" in words[0]:
            problems.append("%s: a (а) form in Russian" % key)
    return problems


def main():
    data, strings = build()
    problems = check(data, strings)
    for p in problems:
        print("PROBLEM", p)
    if problems:
        sys.exit(1)
    if "--check" in sys.argv:
        print("ok: %d kinds, %d rules, %d strings" % (len(KINDS), len(RULES), len(strings)))
        return
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=1)
        f.write("\n")
    content, words = staff()
    for path, obj in ((OUT_CITY, content), (OUT_CITY_STRINGS, words)):
        with open(path, "w", encoding="utf-8") as f:
            json.dump(obj, f, ensure_ascii=False, indent=1)
            f.write("\n")
    os.makedirs(OUT_STRINGS, exist_ok=True)
    for i, lang in enumerate(LANGS):
        with open(os.path.join(OUT_STRINGS, "strings_%s.json" % lang), "w", encoding="utf-8") as f:
            json.dump(collections.OrderedDict((k, v[i]) for k, v in strings.items()), f, ensure_ascii=False, indent=1)
            f.write("\n")
    print("wrote %s and %s/strings_{ru,en,ja}.json: %d kinds, %d rules, %d strings" % (
        os.path.relpath(OUT, ROOT), os.path.relpath(OUT_STRINGS, ROOT), len(KINDS), len(RULES), len(strings)))


if __name__ == "__main__":
    main()
