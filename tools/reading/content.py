# -*- coding: utf-8 -*-
"""Every word of reading anywhere and the library's book club (docs/books.md "Reading anywhere", "The book club"), Russian first.

Written by hand; tools/reading/build_reading.py turns it into Content/Reading/reading.json and
Content/Localization/reading/strings_<lang>.json (never edit those by hand). Speech bubbles in Japanese: 48 characters at most.
"""

# ------------------------------------------------------------------------------------------------------------------ the club

CLUB = {
    "weekday": 3,            # Thursday (0 Monday)
    "from": 18.5,
    "to": 20.0,
    "place": "P-LIBRARY-CLUB",
    "order": ["librarian", "elder", "student", "programmer", "writer"],
    "hero_lines": 2,
}

# the book of each month, January first: works of tools/books/canon.txt with a text in Russian and English at least
BOOKS = ["faust", "queen_of_spades", "ibsen_doll", "time_machine", "two_cities", "white_nights",
         "twenty_thousand", "invisible_man", "rubaiyat", "little_princess", "dead_souls", "christmas_carol"]

# who sits in the circle (picked from the town by tools/reading/club_town.py): the role, how to find them, and how the
# others call them in the diary and the paper
ROLES = {
    "librarian": {"ru": "библиотекарь", "en": "librarian", "ja": "司書"},
    "elder": {"ru": "учительница на пенсии", "en": "retired teacher", "ja": "元教師"},
    "student": {"ru": "студент", "en": "student", "ja": "学生"},
    "programmer": {"ru": "программист", "en": "programmer", "ja": "プログラマー"},
    "writer": {"ru": "писатель", "en": "writer", "ja": "作家"},
}

# The talk: for each book one line by each role, in CLUB["order"]; never past the first chapters. Then the hero's two lines.
# (ru, en, ja)
TALK = {
    "faust": [
        ("Январь — месяц «Фауста». Кто осилил «Пролог на небесах»?", "January is Faust's month. Who got through the Prologue in Heaven?", "一月は『ファウスト』。天上の序曲まで読めた人は？"),
        ("Я читала его ещё девочкой. Старый учёный в кабинете — это я теперь.", "I read it as a girl. The old scholar in his study, that's me now.", "少女の頃に読んだわ。書斎の老学者、今の私ね。"),
        ("Он же всё знает и всё равно скучает. Мне это слишком понятно.", "He knows everything and is still bored. I get that far too well.", "全部知ってるのに退屈してる。分かりすぎて怖い。"),
        ("Пудель, который оказался не пуделем. Лучший баг в литературе.", "A poodle that isn't a poodle. The best bug in literature.", "プードルじゃないプードル。文学史上最高のバグだ。"),
        ("«Остановись, мгновенье» — а мы ещё даже до сделки не дошли.", "\"Stay, moment\" — and we haven't even reached the bargain yet.", "「時よ止まれ」…まだ契約の場面にも着いてないのに。"),
    ],
    "queen_of_spades": [
        ("Февраль, Пушкин, «Пиковая дама». Коротко, зато какая метель.", "February, Pushkin, The Queen of Spades. Short, but what a blizzard.", "二月はプーシキン『スペードの女王』。短いけど吹雪がすごい。"),
        ("Германн всё рассчитал, кроме людей. Как многие мои ученики.", "Hermann worked out everything except people. Like many of my pupils.", "ゲルマンは人間以外を全部計算した。教え子にもいたわ。"),
        ("Три карты, и ни одной на ошибку. Жёстко.", "Three cards and not one to spare for a mistake. Harsh.", "三枚のカードで、失敗の余地なし。きつい。"),
        ("Он хочет алгоритм выигрыша. Я его понимаю, честно.", "He wants an algorithm for winning. I understand him, honestly.", "必勝アルゴリズムが欲しいんだ。正直分かる。"),
        ("Пушкин пишет так быстро, что страшно. Ни одного лишнего слова.", "Pushkin writes so fast it's frightening. Not one word too many.", "プーシキンは速すぎて怖い。無駄な言葉が一つもない。"),
    ],
    "ibsen_doll": [
        ("Март — «Кукольный дом». Пьеса, читайте вслух, так лучше.", "March: A Doll's House. It's a play, read it aloud, it's better.", "三月は『人形の家』。戯曲だから声に出して読んで。"),
        ("Нора с макаронами, которые ей запрещены. Мелочь, а сколько сказано.", "Nora and the macaroons she's not allowed. A trifle, and so much said.", "禁じられたマカロンを食べるノラ。小さいけど雄弁ね。"),
        ("Муж называет её белочкой. Мне уже неуютно, а это первый акт.", "Her husband calls her his squirrel. I'm uneasy already, in act one.", "夫は彼女をリスちゃんと呼ぶ。一幕目でもう居心地悪い。"),
        ("Сто сорок лет пьесе, а читается как переписка в мессенджере.", "The play is a hundred and forty years old and reads like a chat log.", "百四十年前の戯曲なのに、チャットのログみたいだ。"),
        ("Ибсен ставит один стул и одну тайну — и комната уже полна.", "Ibsen puts in one chair and one secret, and the room is full.", "イプセンは椅子一つと秘密一つで部屋を満たす。"),
    ],
    "time_machine": [
        ("Апрель — Уэллс, «Машина времени». Будет спор, чувствую.", "April: Wells, The Time Machine. There'll be an argument, I can tell.", "四月はウェルズ『タイム・マシン』。議論になりそう。"),
        ("Он рассказывает гостям за ужином. Я так рассказываю внукам.", "He tells it to his guests over supper. That's how I tell my grandchildren.", "夕食の客に語るのね。私が孫に話すみたいに。"),
        ("Четвёртое измерение на первых страницах — и никто не уходит.", "The fourth dimension on page one, and nobody leaves the table.", "最初のページで四次元。誰も席を立たない。"),
        ("Рычажок из слоновой кости и кварц. Хочу чертежи, а их нет.", "An ivory lever and quartz. I want the blueprints and there are none.", "象牙のレバーと水晶。設計図が欲しいのに無い。"),
        ("Самое смелое у Уэллса — что гости ему не верят. Очень по-людски.", "The boldest thing in Wells: his guests don't believe him. Very human.", "一番大胆なのは客が信じないこと。とても人間的だ。"),
    ],
    "two_cities": [
        ("Май — Диккенс. «Это было лучшее из времён…» Помните?", "May: Dickens. \"It was the best of times...\" Remember?", "五月はディケンズ。「最良の時代であり…」覚えてる？"),
        ("Первая фраза длиной в страницу. Мы такие в диктантах не давали.", "A first sentence a page long. We never set dictations like that.", "一文目が一ページ。こんな書き取りは出さなかったわ。"),
        ("Почтовая карета в тумане, и все друг друга боятся. Атмосфера.", "A mail coach in the fog and everyone afraid of everyone. Atmosphere.", "霧の中の郵便馬車、皆が互いを怖がる。雰囲気最高。"),
        ("«Воскрешён из мёртвых» — пароль у Диккенса лучше моих.", "\"Recalled to life\" — Dickens has better passwords than I do.", "「よみがえった」…僕のパスワードより気が利いてる。"),
        ("Диккенс платил за слово. Иногда это видно. Но туман того стоит.", "Dickens was paid by the word. Sometimes you can tell. The fog is worth it.", "ディケンズは語数で稼いだ。でも霧は見事だ。"),
    ],
    "white_nights": [
        ("Июнь, белые ночи — значит, «Белые ночи». Тут и ночей-то четыре.", "June, white nights, so White Nights. There are only four nights in it.", "六月は『白夜』。夜はたった四つだけど。"),
        ("Мечтатель, который знает все дома в лицо. Я тоже со своими здороваюсь.", "A dreamer who knows every house by sight. I greet mine too.", "どの家も顔見知りの夢想家。私も家に挨拶するわ。"),
        ("Он одинокий, но так смешно про это пишет. Это я в общаге.", "He's lonely but writes about it so funnily. That's me in the dorm.", "孤独なのに書き方が可笑しい。寮の僕だ。"),
        ("Петербург летом, когда не темнеет. Как наш июнь у моря.", "Petersburg in summer when it never gets dark. Like our June by the sea.", "暗くならない夏のペテルブルク。海辺の六月みたいだ。"),
        ("Достоевский ещё молодой и нежный. Таким его мало кто знает.", "Dostoevsky still young and tender. Few people know him like this.", "若くて優しいドストエフスキー。知る人は少ない。"),
    ],
    "twenty_thousand": [
        ("Июль, море, Жюль Верн. «Двадцать тысяч льё» — и все к воде.", "July, the sea, Jules Verne. Twenty Thousand Leagues, and everyone to the water.", "七月は海とヴェルヌ『海底二万里』。皆、水辺へ。"),
        ("Сначала все ищут чудовище. Учёные спорят, как у нас в учительской.", "First everyone hunts a monster. The scholars argue like our staffroom.", "まず怪物探し。学者の口論は職員室そっくり。"),
        ("Профессор с ассистентом, который всё классифицирует. Это я на практике.", "A professor and an assistant who classifies everything. That's me on fieldwork.", "何でも分類する助手。実習中の僕だ。"),
        ("Электричество в 1870 году как магия. Немо — первый инженер-хакер.", "Electricity in 1870 like magic. Nemo is the first hacker engineer.", "1870年の電気は魔法。ネモは最初のハッカー技師だ。"),
        ("Верн берёт таблицу рыб и делает из неё приключение. Учусь.", "Verne takes a table of fish and makes an adventure of it. I'm learning.", "魚の一覧表から冒険を作るヴェルヌ。勉強になる。"),
    ],
    "invisible_man": [
        ("Август — «Человек-невидимка». Начинается в снегу, в маленькой гостинице.", "August: The Invisible Man. It starts in snow, at a small inn.", "八月は『透明人間』。雪の小さな宿から始まる。"),
        ("Хозяйка гостиницы мне родная душа. Гость в бинтах, а ей лишь бы чай.", "The landlady is a kindred soul. A guest in bandages, and she fusses with tea.", "宿のおかみは同類ね。包帯の客にもお茶の心配。"),
        ("Весь городок сплетничает про одного человека. Знакомо, да?", "The whole village gossips about one man. Sound familiar?", "村中が一人の噂話。どこかで聞いたような？"),
        ("Оптика у Уэллса почти правдоподобна. Почти. Я считал.", "Wells's optics are almost plausible. Almost. I did the maths.", "ウェルズの光学はほぼ本当らしい。ほぼね。計算した。"),
        ("Комедия первые главы, а потом холодок по спине. Умеет.", "A comedy for the first chapters, then a chill down your spine. He knows how.", "序盤は喜劇、そのあと背筋が冷える。上手い。"),
    ],
    "rubaiyat": [
        ("Сентябрь — Хайям. По четверостишию каждый, кто какое выбрал?", "September: Khayyam. One quatrain each. Who chose which?", "九月はハイヤーム。一人一篇、どれを選んだ？"),
        ("Про кувшин из глины, который был человеком. Я его наизусть помню.", "The clay jug that was once a man. I know that one by heart.", "かつて人だった土の壺の詩。暗記してるわ。"),
        ("Вино, звёзды и «не грусти». Хайям — лучший старший брат.", "Wine, stars and \"don't be sad\". Khayyam is the best big brother.", "酒と星と「悲しむな」。最高の兄貴だ。"),
        ("Он же математик был, календарь считал. Это слышно в ритме.", "He was a mathematician, he worked out a calendar. You hear it in the rhythm.", "数学者で暦も作った人だ。リズムに出てる。"),
        ("Четыре строки, и вся жизнь. Мне бы так в одном абзаце.", "Four lines and a whole life. I wish I could do that in a paragraph.", "四行で人生まるごと。段落一つでやりたい。"),
    ],
    "little_princess": [
        ("Октябрь — «Маленькая принцесса». Добро пожаловать, новенькие!", "October: A Little Princess. Welcome, newcomers!", "十月は『小公女』。新しい方、ようこそ！"),
        ("Девочка в пансионе, папа далеко. Я сорок лет видела таких на первом сентября.", "A girl at boarding school, papa far away. I saw such girls every September for forty years.", "寄宿学校の少女、父は遠く。毎年九月に見た顔だわ。"),
        ("Сара придумывает истории, чтобы не плакать. Сильный ход.", "Sara makes up stories so she won't cry. Strong move.", "泣かないために物語を作るセーラ。強いね。"),
        ("Кукла Эмили, которая «оживает», когда никто не смотрит. Мне нравится.", "Emily the doll who \"comes alive\" when no one looks. I like it.", "誰も見ていない時に動く人形エミリー。好きだな。"),
        ("Бёрнетт пишет детскую книгу всерьёз. Это редкость.", "Burnett writes a children's book in earnest. That's rare.", "バーネットは児童書を本気で書く。珍しいことだ。"),
    ],
    "dead_souls": [
        ("Ноябрь — Гоголь, «Мёртвые души». Бричка въезжает в город…", "November: Gogol, Dead Souls. A britzka drives into town...", "十一月はゴーゴリ『死せる魂』。馬車が町へ…"),
        ("Два мужика спорят, доедет ли колесо до Москвы. Это лучшее начало на свете.", "Two peasants argue whether the wheel will make it to Moscow. Best opening in the world.", "車輪がモスクワまで持つか議論する農夫。最高の冒頭よ。"),
        ("Чичиков всем нравится, а что ему надо — никто не понял. Пока.", "Everyone likes Chichikov and nobody knows what he wants. Yet.", "チチコフは皆に好かれるけど、目的は謎。今のところ。"),
        ("Он покупает то, чего нет. Почти как стартап.", "He buys what doesn't exist. Almost like a startup.", "存在しない物を買う。ほぼスタートアップだ。"),
        ("Гоголь описывает трактир три страницы, и мне уже хочется есть. Магия.", "Gogol spends three pages on a tavern and I'm hungry. Magic.", "居酒屋の描写三ページで腹が減る。魔法だ。"),
    ],
    "christmas_carol": [
        ("Декабрь — «Рождественская песнь». Читаем до Рождества, не дольше!", "December: A Christmas Carol. We read it before Christmas, no later!", "十二月は『クリスマス・キャロル』。クリスマスまでに！"),
        ("«Марли был мёртв, это прежде всего». Как учили писать первую строку.", "\"Marley was dead, to begin with.\" That's how we taught a first line.", "「まずマーリーは死んでいた」。冒頭の手本ね。"),
        ("Скрудж запрещает клерку подбросить угля. Мне сразу стало холодно.", "Scrooge won't let his clerk have more coal. I froze at once.", "スクルージは石炭を足させない。こっちが凍えた。"),
        ("Дверной молоток превращается в лицо. Отличный глитч.", "The door knocker turns into a face. A great glitch.", "ノッカーが顔になる。最高のグリッチだ。"),
        ("Короткая, на один вечер. Диккенс знал, сколько длится зимний вечер.", "Short, one evening's worth. Dickens knew how long a winter evening is.", "短くて一晩分。冬の夜の長さを知ってたんだ。"),
    ],
}

# the hero's lines when he joins (read the book to its end): two, one after the other
HERO = {
    "faust": [("Мне Мефистофель понравился больше всех. Он хотя бы честно скучает.", "I liked Mephistopheles best. At least he's honest about being bored.", "メフィストが一番好き。退屈に正直だから。"),
              ("А «Остановись, мгновенье» я теперь говорю у моря на закате.", "And now I say \"Stay, moment\" by the sea at sunset.", "今は夕暮れの海で「時よ止まれ」って言ってる。")],
    "queen_of_spades": [("Я дочитал ночью, и потом долго не мог уснуть.", "I finished it at night and couldn't sleep for ages.", "夜に読み終えて、しばらく眠れなかった。"),
                        ("Мне кажется, графиня подмигнула не только Германну.", "I think the countess winked at more than just Hermann.", "伯爵夫人はゲルマンだけに目配せしたんじゃないと思う。")],
    "ibsen_doll": [("Я читал на скамейке и чуть не крикнул на весь парк.", "I read it on a bench and nearly shouted across the park.", "ベンチで読んで、公園中に叫びそうになった。"),
                   ("Хлопок двери в конце — лучший звук, который я читал.", "The door slamming at the end is the best sound I've ever read.", "最後のドアの音は、読んだ中で最高の音。")],
    "time_machine": [("Я теперь смотрю на часы на площади и думаю, куда бы поехал.", "Now I look at the clock on the square and think where I'd go.", "広場の時計を見て、どこへ行こうか考えちゃう。"),
                     ("Мне было страшно за Уину. Очень.", "I was scared for Weena. Really scared.", "ウィーナが心配でたまらなかった。")],
    "two_cities": [("Мадам Дефарж со своим вязанием — страшнее любого злодея.", "Madame Defarge and her knitting are scarier than any villain.", "編み物のドファルジュ夫人はどんな悪役より怖い。"),
                   ("Последнюю страницу я читал два раза. Вслух, тихо.", "I read the last page twice. Aloud, quietly.", "最後のページは二回読んだ。小さな声で。")],
    "white_nights": [("Я читал её на набережной в июне, и было светло до конца.", "I read it on the promenade in June, and it was light till the end.", "六月の遊歩道で読んだら、最後まで明るかった。"),
                     ("Мне жалко Мечтателя. И всё равно он счастливее многих.", "I'm sorry for the Dreamer. And he's still happier than many.", "夢想家が気の毒。でも多くの人より幸せだと思う。")],
    "twenty_thousand": [("Я теперь смотрю на море и считаю, сколько это — двадцать тысяч льё.", "Now I look at the sea and work out how far twenty thousand leagues is.", "海を見て二万里がどれくらいか計算してる。"),
                        ("Консель, который классифицирует даже в опасности, — мой герой.", "Conseil, who keeps classifying things even in danger, is my hero.", "危機でも分類するコンセイユが僕のヒーロー。")],
    "invisible_man": [("Я весь вечер оглядывался в пустой улице. Спасибо, Уэллс.", "I kept looking round in empty streets all evening. Thanks, Wells.", "一晩中、誰もいない通りで振り返ってた。"),
                      ("Мне его жалко в конце, хоть он и сам виноват.", "I felt sorry for him at the end, though it was his own fault.", "最後は気の毒だった。自業自得だけど。")],
    "rubaiyat": [("Я выписал одно четверостишие в дневник. Про розы и про пыль.", "I copied one quatrain into my diary. About roses and dust.", "一篇を日記に写した。薔薇と塵の詩。"),
                 ("Читал на скамейке, а над головой были те самые звёзды.", "I read it on a bench, with those very stars overhead.", "ベンチで読んだら、頭上にあの星があった。")],
    "little_princess": [("Сара мне понравилась тем, что не перестаёт быть вежливой.", "I liked Sara because she never stops being polite.", "セーラが礼儀を失わないところが好き。"),
                        ("Чердак с крысой Мельхиседеком — я бы там пожил.", "The attic with Melchisedec the rat — I'd live there.", "ネズミのメルキセデクがいる屋根裏、住んでみたい。")],
    "dead_souls": [("Я смеялся вслух в поезде, на меня оглядывались.", "I laughed out loud on the train and people turned round.", "電車で声を出して笑って、振り向かれた。"),
                   ("Плюшкин — это страшно и грустно сразу. Как так можно написать?", "Plyushkin is frightening and sad at once. How can anyone write that?", "プリューシキンは怖くて悲しい。どう書けるんだ？")],
    "christmas_carol": [("Я дочитал в один вечер, как вы и говорили.", "I finished it in one evening, just as you said.", "言われた通り、一晩で読み終えた。"),
                        ("После книги хочется кому-нибудь занести угля. Или пирога.", "After the book you want to bring someone coal. Or a pie.", "読んだ後、誰かに石炭か、パイを届けたくなる。")],
}

# ------------------------------------------------------------------------------------------------ the librarian's advice

# "if you liked X, read Y": across languages (a work counts in whatever language he read it)
PAIRS = [
    ("little_princess", "alice"), ("alice", "looking_glass"), ("looking_glass", "prince_pauper"), ("prince_pauper", "captain_grant"),
    ("captain_grant", "twenty_thousand"), ("twenty_thousand", "earth_moon"), ("earth_moon", "time_machine"), ("time_machine", "invisible_man"),
    ("invisible_man", "frankenstein"), ("frankenstein", "dorian_gray"), ("dorian_gray", "faust"), ("faust", "divine_comedy"),
    ("white_nights", "poor_folk"), ("poor_folk", "crime_punishment"), ("crime_punishment", "karamazov"), ("overcoat", "nose_gogol"),
    ("nose_gogol", "dead_souls"), ("dead_souls", "inspector"), ("queen_of_spades", "onegin"), ("onegin", "fathers_sons"),
    ("fathers_sons", "first_love"), ("first_love", "lady_dog"), ("lady_dog", "darling"), ("darling", "cherry_orchard"),
    ("christmas_carol", "two_cities"), ("two_cities", "les_miserables"), ("ibsen_doll", "anna_karenina"), ("anna_karenina", "bovary"),
    ("bovary", "red_black"), ("rubaiyat", "gitanjali"), ("gitanjali", "book_of_tea"), ("book_of_tea", "walden"), ("walden", "sportsman"),
    ("hamlet", "king_lear"), ("romeo_juliet", "midsummer"), ("midsummer", "tempest"), ("iliad", "odyssey"), ("odyssey", "kalevala"),
    ("call_wild", "martin_eden"), ("kim", "call_wild"), ("ward6", "chekhov_stories"), ("carrots", "little_women"),
]
STARTERS = ["little_princess", "christmas_carol", "white_nights", "time_machine", "alice", "lady_dog", "queen_of_spades"]

FRIENDS = {"opinions": 6, "reply_min": 4, "reply_max": 8}

# --------------------------------------------------------------------------------------------------------------- strings

S = {
    # reading wherever he sits, the light
    "reading.prompt.read": ("E — читать", "E — read", "E：読む"),
    "reading.dark": ("Слишком темно, чтобы читать. Включи фонарик (L) или сядь под фонарём.",
                     "Too dark to read. Switch on the headlamp (L) or sit under a street lamp.",
                     "暗くて読めない。ヘッドランプ(L)をつけるか、街灯の下へ。"),
    "reading.rain": ("Под дождём страницы промокнут. Почитаю под крышей.", "The rain would soak the pages. I'll read under a roof.",
                     "雨でページが濡れる。屋根の下で読もう。"),
    "reading.dusk_close": ("Стемнело: строчки уже не разобрать.", "It's got dark: I can't make out the lines any more.",
                           "暗くなって、もう字が読めない。"),
    "reading.rain_close": ("Закапал дождь — книгу под куртку.", "It's starting to rain: the book goes under my jacket.",
                           "雨だ。本を上着の中へ。"),
    "reading.dim": ("Сумерки: читать можно, но щуришься.", "Dusk: I can read, but I have to squint.", "薄暗い。読めるけど目を細める。"),
    # the reader's keys and the quote mode
    "reading.keys": ("A/D — листать · Q — оглавление · C — выписать цитату · +/− — шрифт · Esc — закрыть",
                     "A/D — turn pages · Q — contents · C — copy a quote · +/− — type size · Esc — close",
                     "A/D：めくる · Q：目次 · C：引用を書き写す · +/−：文字サイズ · Esc：閉じる"),
    "reading.quote.keys": ("W/S — другое предложение · E — выписать в дневник · C — отмена",
                           "W/S — another sentence · E — copy into the diary · C — cancel",
                           "W/S：別の文 · E：日記に書き写す · C：やめる"),
    "reading.quote.saved": ("Цитата выписана в дневник: «{0}»", "Quote copied into the diary: “{0}”", "日記に引用を書き写した：「{0}」"),
    "reading.quote.again": ("Эта цитата уже есть в дневнике.", "That quote is already in the diary.", "その引用はもう日記にある。"),
    "reading.quote.none": ("На этой странице нечего выписать.", "Nothing to copy on this page.", "このページには書き写せる文がない。"),
    # the diary's «Книги» pages
    "reading.diary.title": ("Книги", "Books", "本"),
    "reading.diary.stats": ("Дочитано книг: {0}. Прочитано страниц: {1}.", "Books finished: {0}. Pages read: {1}.", "読み終えた本：{0}冊。読んだページ：{1}。"),
    "reading.diary.longest": ("Самая длинная: «{0}», {1} стр.", "The longest: “{0}”, {1} pages.", "いちばん長い本：『{0}』{1}ページ。"),
    "reading.diary.reading_now": ("Сейчас читаю: «{0}» — {1}%.", "Reading now: “{0}”, {1}%.", "今読んでいる本：『{0}』{1}%。"),
    "reading.diary.quotes": ("Цитаты", "Quotes", "引用"),
    "reading.diary.quote_src": ("— «{0}», {1}", "— “{0}”, {1}", "——『{0}』、{1}"),
    "reading.diary.empty": ("Пока ничего не выписано. В книге нажми C и выбери предложение.",
                            "Nothing copied yet. In a book press C and choose a sentence.",
                            "まだ何も書き写していない。本の中でCを押して文を選ぼう。"),
    "reading.diary.club": ("Книжный клуб: был на {0} заседаниях.", "Book club: joined the talk {0} times.", "読書会：{0}回参加した。"),
    # the diary's journal lines
    "reading.note.session": ("Читал «{0}»: {1} стр.", "Read “{0}”: {1} pages.", "『{0}』を読んだ：{1}ページ。"),
    "reading.note.finished": ("Дочитал «{0}».", "Finished “{0}”.", "『{0}』を読み終えた。"),
    "reading.note.club": ("Был в книжном клубе: говорили о «{0}».", "Went to the book club: we talked about “{0}”.", "読書会に出た。『{0}』の話をした。"),
    "reading.note.recommend": ("Посоветовал «{0}» — {1}.", "Recommended “{0}” to {1}.", "『{0}』を{1}に勧めた。"),
    # the lending desk
    "reading.desk.advice": ("Библиотекарь советует: «{0}» ({1}) — раз тебе понравилась «{2}».",
                            "The librarian suggests “{0}” ({1}), since you liked “{2}”.",
                            "司書のおすすめ：『{0}』（{1}）。『{2}』が気に入ったなら。"),
    "reading.desk.advice_month": ("Книга месяца в клубе: «{0}» ({1}). Клуб — по четвергам в 18:30 в читальном зале.",
                                  "The club's book of the month: “{0}” ({1}). The club meets on Thursdays at 18:30 in the reading room.",
                                  "今月の読書会の本：『{0}』（{1}）。毎週木曜18:30、閲覧室で。"),
    "reading.desk.advice_start": ("Библиотекарь советует начать с «{0}» ({1}).", "The librarian suggests starting with “{0}” ({1}).",
                                  "司書のおすすめ：まずは『{0}』（{1}）から。"),
    "reading.desk.overdue": ("просрочена", "overdue", "延滞中"),
    "reading.desk.recommend": ("Посоветовать другу", "Recommend to a friend", "友だちに勧める"),
    "reading.recommend.done": ("Ты посоветовал «{0}» — {1}. Возьмёт в библиотеке.", "You recommended “{0}” to {1}. They'll borrow it.",
                               "『{0}』を{1}に勧めた。図書館で借りるそうだ。"),
    "reading.recommend.nobody": ("Посоветовать пока некому: сначала подружись с кем-нибудь в городе.",
                                 "No one to recommend it to yet: make a friend in town first.",
                                 "まだ勧める相手がいない。まず町で友だちを作ろう。"),
    "reading.recommend.all": ("Твои друзья уже читают то, что ты им посоветовал.", "Your friends are all reading what you recommended already.",
                              "友だちはみんな、勧めた本をもう読んでいる。"),
    # the overdue postcard and the friends' letters
    "reading.letter.from": ("Городская библиотека", "The town library", "町立図書館"),
    "reading.letter.overdue": ("Здравствуй! Книга «{0}» у тебя уже {1} дн. Никто не торопит, но её ждут и другие читатели. Загляни, продлим или примем. Библиотека, кафедра выдачи.",
                               "Hello! You've had “{0}” for {1} days now. No hurry, but other readers are waiting for it too. Drop in and we'll renew it or take it back. The library's lending desk.",
                               "こんにちは。『{0}』の貸出から{1}日たちました。急ぎませんが、待っている読者もいます。延長か返却にお立ち寄りください。図書館貸出カウンターより。"),
    "reading.friend.opinion.1": ("Взял «{0}», как ты советовал. Читал до двух ночи — спасибо и одновременно ну тебя!",
                                 "I borrowed “{0}” as you said. Read till two in the morning — thank you, and also how dare you!",
                                 "勧められた『{0}』を借りた。夜中の二時まで読んだよ。ありがとう、そしてひどい！"),
    "reading.friend.opinion.2": ("Дочитал «{0}». Не всё понял, но один кусок перечитал три раза. Обсудим на скамейке?",
                                 "Finished “{0}”. Didn't understand all of it, but I reread one part three times. Talk it over on a bench?",
                                 "『{0}』を読み終えた。全部は分からないけど、一か所は三回読んだ。ベンチで話そう？"),
    "reading.friend.opinion.3": ("«{0}» — честно, тяжеловато. Но конец того стоил. У тебя хороший вкус.",
                                 "“{0}” — honestly, a bit heavy going. But the ending was worth it. You have good taste.",
                                 "『{0}』は正直ちょっと重かった。でも結末は価値があった。いい趣味だね。"),
    "reading.friend.opinion.4": ("Читаю «{0}» вслух домашним по вечерам. Теперь все спрашивают, что дальше.",
                                 "I'm reading “{0}” aloud to my family in the evenings. Now they all ask what happens next.",
                                 "『{0}』を夜に家族へ読み聞かせてる。みんな続きをせがむんだ。"),
    "reading.friend.opinion.5": ("Спасибо за «{0}». Я давно ничего не читал, а тут не мог оторваться. Посоветуй ещё.",
                                 "Thanks for “{0}”. I hadn't read anything in ages and couldn't put it down. Recommend another.",
                                 "『{0}』をありがとう。久しぶりに夢中で読んだ。また教えて。"),
    "reading.friend.opinion.6": ("«{0}» мне не очень. Зато я нашёл в ней закладку из 1974 года. Тоже история!",
                                 "“{0}” wasn't really for me. But I found a bookmark from 1974 in it. That's a story too!",
                                 "『{0}』はあまり合わなかった。でも1974年のしおりが挟まってた。それも物語だね！"),
    # the club
    "reading.club.join": ("E — вступить в разговор о книге", "E — join the talk about the book", "E：本の話に加わる"),
    "reading.club.unread": ("Клуб обсуждает «{0}». Дочитай её — и сможешь вступить в разговор.",
                            "The club is talking about “{0}”. Finish it and you can join in.",
                            "読書会は『{0}』の話をしている。読み終えれば話に加われる。"),
    "reading.club.joined": ("Ты рассказал клубу о «{0}».", "You told the club about “{0}”.", "読書会で『{0}』について話した。"),
    "line.town.bookclub_welcome": ("Садись к нам! Ты тоже дочитал? Рассказывай.", "Come and sit with us! You've finished it too? Tell us.", "こっちに座って！読み終えたの？聞かせて。"),
    # the paper
    "reading.paper.title": ("Клуб читателей", "The readers' club", "読書会だより"),
    "reading.paper.book": ("Книга месяца — «{0}» ({1}). Клуб собирается по четвергам в 18:30 в читальном зале городской библиотеки, приходите со своими закладками.",
                           "Book of the month: “{0}” ({1}). The club meets on Thursdays at 18:30 in the reading room of the town library — bring your bookmarks.",
                           "今月の本：『{0}』（{1}）。読書会は毎週木曜18:30、町立図書館の閲覧室で。しおりを持ってどうぞ。"),
    "reading.paper.tonight": ("Сегодня вечером — заседание клуба. Библиотекарь обещает чай с сушками.",
                              "The club meets tonight. The librarian promises tea and bagels.",
                              "今夜は読書会。司書がお茶と菓子を用意するそうだ。"),
    "reading.paper.next": ("В следующем месяце читаем «{0}» ({1}).", "Next month we read “{0}” ({1}).", "来月の本は『{0}』（{1}）。"),
    # developer mode
    "dev.action.reading.quote": ("Выписать цитату (первое предложение страницы)", "Copy a quote (the page's first sentence)", "引用を書き写す（ページ最初の文）"),
    "dev.action.reading.finish_month": ("Дочитать книгу месяца", "Finish the book of the month", "今月の本を読了にする"),
    "dev.action.reading.club_now": ("Клуб: четверг 18:40", "Club: Thursday 18:40", "読書会：木曜18:40"),
    "dev.action.reading.overdue": ("Просрочить книги (8 дней)", "Make the loans overdue (8 days)", "貸出を延滞にする（8日）"),
    "dev.action.reading.recommend": ("Посоветовать книгу другу", "Recommend a book to a friend", "友だちに本を勧める"),
    "dev.tp.reading.club": ("Читальный зал, книжный клуб", "Reading room, the book club", "閲覧室・読書会"),
}
