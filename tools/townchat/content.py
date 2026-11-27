"""The data and the hero's side of a talk with a townsperson (docs/town_talk.md "A talk"): the rules, what an activity
done earlier is called in their day's news, which occupations are asked about how, the likes with lines, the questions
they ask back with his answers, the topics' titles (his questions) and the page's words, ru / en / ja.
Their own lines are the town talk's: tools/barks/lines/18_townchat.txt (sets with on = tc_*).
tools/townchat/build_town_chat.py turns this into Content/PeopleData/town_chat.json and Content/Localization/townchat/.
"""

RULES = {"topics_min": 2, "topics_max": 4, "kid_age": 12, "kid_shy_met": 2, "did_hours": 4.0, "ask_chance": 0.45}

# how often a topic is offered against the others that have a line now
WEIGHTS = {"day": 2.5, "work": 2.0, "festival": 3.0, "rumour": 3.0, "weather": 1.2, "hobby": 1.2, "kin": 1.2, "friend": 1.0, "town": 0.8}

# what they did before this -> the group of their day's news (on = tc_did_<group>); activities.json ids
DID = {
    "shop": ["shop", "shop_town", "supplies_town", "san_postcards"],
    "eat": ["eat_out", "coffee", "eat_diner"],
    "sea": ["sea_air", "swim_look", "sun_watch", "watch_sunrise", "boats_evening"],
    "walk": ["walk", "stroll", "night_walk", "jog", "san_promenade", "walk_dog", "walk_dog_late", "island_walk", "walk_aton", "lookout_view"],
    "bench": ["sit_bench", "read_bench", "read_paper", "feed_birds", "sit_mayak", "sit_belvedere"],
    "club": ["play_club", "snack_club", "eat_club"],
    "library": ["read_library", "book_club"],
    "fish": ["fish_shore", "mend_nets"],
    "fair": ["fair_visit", "gather_fair", "gather_mingle", "gather_watch", "gather_sit"],
    "culture": ["culture_evening", "cinema_day", "cinema_evening", "play_piano", "dominoes"],
    "sport": ["sport", "skate", "skate_busy", "walrus_swim", "banya_visit"],
    "garden": ["tend_allotment", "water_allotment", "tend_hives", "orchard_picnic"],
    "play": ["playground", "play_run", "fly_kite", "skip_stones", "play_brook"],
    "school": ["school", "school_town"],
    "clinic": ["clinic_visit", "san_cure", "cure_sanatorium_inhale"],
}

# occupation -> how the hero asks about it (the work topic's title); anything else: "work"
JOBS = {"pupil": "study", "student": "study", "pensioner": "retired", "homemaker": "home", "guest": "guest", "renter": "guest",
        "unemployed": "free"}

# the likes of townsfolk.json that have lines (on = tc_hobby_<like>)
HOBBIES = ["play_club", "night_walk", "watch_sunrise", "fish_shore", "stroll", "coffee", "eat_out", "swim_look", "fair_visit", "sea_air",
           "sit_bench", "read_library", "play_piano"]

# the questions they ask back: id, who asks (ages), his answers (ru, en, ja); theirs: on = tc_ask_<id>, tc_reply_<id>_<n>
ASKS = [
    {"id": "new", "min_age": 12, "answers": [
        ("Я из клуба на горе.", "I'm from the club up the hill.", "山の上のクラブから来ました。"),
        ("Да так, гуляю.", "Oh, just walking about.", "ちょっと散歩してるだけです。")]},
    {"id": "sea", "answers": [
        ("Был.", "I have.", "行きました。"),
        ("Ещё нет.", "Not yet.", "まだです。")]},
    {"id": "birds", "answers": [
        ("Правда.", "It's true.", "本当です。"),
        ("Больше люблю компьютеры.", "I like computers more.", "コンピューターのほうが好きです。")]},
    {"id": "like", "min_age": 16, "answers": [
        ("Нравится.", "I like it.", "気に入ってます。"),
        ("Пока привыкаю.", "Still getting used to it.", "まだ慣れてる途中です。")]},
    {"id": "eat", "min_age": 30, "answers": [
        ("Обедал.", "I have.", "食べました。"),
        ("Забыл.", "I forgot.", "忘れてました。")]},
    {"id": "game", "max_age": 17, "answers": [
        ("В стратегии.", "Strategy games.", "ストラテジーだよ。"),
        ("Больше смотрю, как другие играют.", "Mostly I watch the others play.", "人がやってるのを見てるほうが多いかな。")]},
]

# the topics' titles: his question to a grown-up (вы) and to somebody under 18 (ты)
TOPICS = {
    "day.morning": (("Какие у вас планы на день?", "Any plans for the day?", "今日のご予定は？"),
                    ("Что сегодня будешь делать?", "What are you up to today?", "今日は何するの？")),
    "day": (("Как ваш день?", "How's your day going?", "今日はどうですか？"),
            ("Как день?", "How's your day?", "今日はどう？")),
    "day.evening": (("Как прошёл день?", "How was your day?", "今日はどうでしたか？"),
                    ("Как день прошёл?", "How did the day go?", "今日はどうだった？")),
    "work.work": (("Как работа?", "How's work?", "お仕事はどうですか？"),
                  ("Как работа?", "How's work?", "仕事はどう？")),
    "work.study": (("Как учёба?", "How are your studies?", "勉強はどうですか？"),
                   ("Как дела в школе?", "How's school?", "学校はどう？")),
    "work.retired": (("Как поживаете?", "How are you keeping?", "お元気ですか？"),
                     ("Как поживаешь?", "How are you doing?", "元気？")),
    "work.home": (("Как дела дома?", "How are things at home?", "おうちのほうはどうですか？"),
                  ("Как дела дома?", "How are things at home?", "おうちはどう？")),
    "work.guest": (("Как вам у нас отдыхается?", "How are you enjoying your stay?", "こちらでの滞在はいかがですか？"),
                   ("Как тебе у нас?", "How do you like it here?", "ここはどう？")),
    "work.free": (("Чем сейчас занимаетесь?", "What keeps you busy these days?", "最近は何をされているんですか？"),
                  ("Чем сейчас занимаешься?", "What keeps you busy these days?", "最近は何してるの？")),
    "festival": (("Праздник сегодня, да?", "It's a holiday today, isn't it?", "今日はお祭りですね？"),
                 ("Ты на праздник?", "Off to the festival?", "お祭り行くの？")),
    "rumour": (("Что про меня говорят?", "What are people saying about me?", "ぼくのこと、何か聞きましたか？"),
               ("Что про меня говорят?", "What are people saying about me?", "ぼくのこと、何か聞いた？")),
    "weather": (("Ну и погода сегодня, а?", "Some weather today, eh?", "今日のお天気、どうですか？"),
                ("Как тебе погода?", "What do you make of the weather?", "今日の天気どう？")),
    "hobby": (("Что вы любите делать?", "What do you like to do?", "お好きなことは何ですか？"),
              ("Что ты любишь делать?", "What do you like to do?", "何するのが好き？")),
    "kin.kids": (("Как ваши дети?", "How are your children?", "お子さんはお元気ですか？"),
                 ("Как ваши дети?", "How are your children?", "お子さんはお元気ですか？")),
    "kin.parents": (("Как ваши родители?", "How are your parents?", "ご両親はお元気ですか？"),
                    ("Как твои родители?", "How are your mum and dad?", "お父さんとお母さんは元気？")),
    "kin.sibling": (("У вас есть брат или сестра?", "Do you have a brother or a sister?", "ごきょうだいはいますか？"),
                    ("У тебя есть брат или сестра?", "Do you have a brother or a sister?", "きょうだいはいるの？")),
    "kin.spouse": (("Как ваша семья?", "How's your family?", "ご家族はお元気ですか？"),
                   ("Как ваша семья?", "How's your family?", "ご家族はお元気ですか？")),
    "kin.mate": (("С кем вы живёте?", "Who do you live with?", "どなたと暮らしているんですか？"),
                 ("С кем ты живёшь?", "Who do you live with?", "誰と住んでるの？")),
    "friend": (("С кем вы тут дружите?", "Who are your friends round here?", "このあたりに仲のいい方はいますか？"),
               ("С кем ты дружишь?", "Who are your friends?", "仲のいい友だちは？")),
    "town": (("Что тут стоит посмотреть?", "What's worth seeing round here?", "このへんで見ておくといい所は？"),
             ("Что тут интересного?", "What's fun round here?", "このへんで面白い所ある？")),
}

# the page's words
UI = {
    "townchat.prompt": ("E — поговорить: {0}", "E — talk to {0}", "E — {0}と話す"),
    "townchat.said": ("— {0}", "“{0}”", "「{0}」"),
    "townchat.entry": ("{0}. {1}", "{0}. {1}", "{0}. {1}"),
    "townchat.gift": ("Подарить что-нибудь…", "Give something…", "何か贈る…"),
    "townchat.keys": ("1–5, A / D — выбрать   E — спросить   Esc — попрощаться", "1–5, A / D — choose   E — ask   Esc — say goodbye",
                      "1–5・A / D — 選ぶ   E — たずねる   Esc — さようなら"),
    "townchat.keys_next": ("E — дальше", "E — go on", "E — つづき"),
    "townchat.keys_ask": ("1–2 — ответить", "1–2 — answer", "1–2 — 答える"),
    "townchat.keys_bye": ("E / Esc — идти дальше", "E / Esc — walk on", "E / Esc — 立ち去る"),
    "townchat.diary": ("Поговорил с жителем города: {0}.", "Had a talk with one of the townsfolk: {0}.", "町の人と話した：{0}。"),
    "dev.action.friends.talk_nearest": ("Поговорить с ближайшим жителем", "Talk to the nearest resident", "いちばん近い住民と話す"),
    "dev.action.friends.talk_report": ("Разговор: что скажет ближайший житель (в лог)", "Talk: what the nearest resident has to say (log)",
                                       "会話：いちばん近い住民の話題（ログ）"),
}

# every on = of 18_townchat.txt (tools/barks/build_barks.py reads this list)
KIN = ["son", "daughter", "mum", "dad", "sister", "brother", "wife", "husband", "mate"]
EVENTS = (["tc_hello", "tc_bye", "tc_no_work", "tc_no_hurry", "tc_no_shy", "tc_work", "tc_day", "tc_day_before_work", "tc_day_after_work",
           "tc_day_off", "tc_weather", "tc_festival", "tc_rumour", "tc_friend", "tc_town"]
          + ["tc_did_" + g for g in DID] + ["tc_kin_" + k for k in KIN] + ["tc_hobby_" + h for h in HOBBIES]
          + ["tc_ask_" + a["id"] for a in ASKS] + ["tc_reply_%s_%d" % (a["id"], n + 1) for a in ASKS for n in range(len(a["answers"]))])
# the events that must have a line for anybody at any moment (a set with no conditions)
ALWAYS = ["tc_hello", "tc_bye", "tc_no_work", "tc_no_hurry", "tc_no_shy", "tc_work", "tc_day", "tc_weather", "tc_town", "tc_friend", "tc_festival",
          "tc_rumour"] + ["tc_kin_" + k for k in KIN] + ["tc_did_" + g for g in DID] + ["tc_hobby_" + h for h in HOBBIES] \
    + ["tc_reply_%s_%d" % (a["id"], n + 1) for a in ASKS for n in range(len(a["answers"]))]
FORMS = ["nom", "gen", "dat", "acc", "ins", "pre"]

# given names of townsfolk written by hand (not in tools/town/names.json): id, sex, ru, en, ja
EXTRA_NAMES = [
    ("sonya", "f", "Соня", "Sonya", "ソーニャ"), ("boris", "m", "Борис", "Boris", "ボリス"), ("nikolai", "m", "Николай", "Nikolai", "ニコライ"),
    ("raisa", "f", "Раиса", "Raisa", "ライーサ"), ("tamara", "f", "Тамара", "Tamara", "タマーラ"), ("yuri", "m", "Юрий", "Yuri", "ユーリー"),
    ("haruo", "m", "Харуо", "Haruo", "春雄"), ("lidia", "f", "Лидия", "Lidia", "リディヤ"), ("asya", "f", "Ася", "Asya", "アーシャ"),
]
