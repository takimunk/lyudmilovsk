# The films of the house of culture «Рассвет» (docs/cinema.md): public-domain silent shorts and early animation, shown as
# the newsreel before every session ("журнал перед фильмом"). Russian is the source of every word.
#
# Rule (the same as the library's books, memory "books-library"): every maker of the film (director, animator, producer
# who owned it) died 1955 or earlier and the film was published before 1929, so it is out of copyright in the USA (published
# before 1929), in the EU and Russia (life + 70; none of them was a Soviet WWII veteran, so no +4), in Japan (life + 70 since
# 2018; for films: 70 years after release, also over). The one country with a longer term is Mexico (life + 100): Lumière
# (d. 1948/1954), Méliès (d. 1938), Cohl (d. 1938), Chomón (d. 1929) and Porter (d. 1941) are not yet free there; this is
# written in SOURCES.md. Each copy is taken from Wikimedia Commons only when its page says Public domain (fetch_films.py
# checks the licence field and writes it to films.lock.json).
#
# "commons": search words for Commons' file namespace; "seconds": the length a real copy must have (a trailer or a fragment
# is refused). "lector": the lecturer's words over the film (the hall's old projectionist reads them, as lecturers did in
# the silent days, a benshi in Japan): at a share of the film's length (0..1), so any copy's timing fits. "laughs": shares
# of the film where the hall laughs. None of the chosen films has intertitles (the Lumières', Méliès', Cohl's, Chomón's and
# Porter's 1903 copy have none); the title card is ours ("city.film.card").

FILMS = [
    {
        "id": "lumiere_train",
        "title": ("Прибытие поезда на вокзал Ла-Сьота", "The Arrival of a Train at La Ciotat", "ラ・シオタ駅への列車の到着"),
        "maker": ("Огюст и Луи Люмьер", "Auguste and Louis Lumière", "オーギュスト＆ルイ・リュミエール"),
        "died": [1954, 1948], "year": 1896, "country": "FR",
        "commons": ["Arrivée d'un train en gare de La Ciotat", "Arrival of a Train at La Ciotat"],
        "seconds": [35, 120],
        "lector": [
            (0.08, ("Смотрите: вокзал маленького городка у моря. Сейчас покажется паровоз.",
                    "Look: the station of a little town by the sea. The engine will appear in a moment.",
                    "ご覧ください。海辺の小さな町の駅です。もうすぐ機関車が見えてきます。")),
            (0.30, ("Говорят, на первом показе зрители вскакивали с мест. Сидите спокойно: он остановится.",
                    "They say people jumped from their seats at the first showing. Sit still: it will stop.",
                    "最初の上映では客席から飛び上がった人もいたそうです。落ち着いて。ちゃんと止まりますから。")),
            (0.62, ("Дама в светлом ищет кого-то глазами. Никто из этих людей не знал, что их увидят через сто тридцать лет.",
                    "The lady in white looks for someone. None of these people knew they'd be watched a hundred and thirty years later.",
                    "白い服の婦人がだれかを探しています。百三十年後に見られるとは、だれも知りませんでした。")),
        ],
        "laughs": [0.34],
    },
    {
        "id": "lumiere_factory",
        "title": ("Выход рабочих с фабрики Люмьер в Лионе", "Workers Leaving the Lumière Factory in Lyon", "リュミエール工場の出口"),
        "maker": ("Луи Люмьер", "Louis Lumière", "ルイ・リュミエール"),
        "died": [1948], "year": 1895, "country": "FR",
        "commons": ["La Sortie de l'usine Lumière à Lyon", "Workers Leaving the Lumière Factory"],
        "seconds": [30, 120],
        "lector": [
            (0.10, ("Это самый первый фильм на свете. Обеденный перерыв, ворота фабрики открываются.",
                    "This is the very first film in the world. Lunch break, and the factory gates open.",
                    "これは世界で最初の映画です。昼休み、工場の門が開きます。")),
            (0.55, ("Велосипедист, собака, шляпки. Совсем как у нас на «Шестерне» после смены.",
                    "A cyclist, a dog, hats. Just like our people coming off a shift at the Cog.",
                    "自転車の人、犬、帽子。うちの「歯車」の交代時間とそっくりです。")),
        ],
        "laughs": [0.78],
    },
    {
        "id": "lumiere_sprinkler",
        "title": ("Политый поливальщик", "The Sprinkler Sprinkled", "水をかけられた散水夫"),
        "maker": ("Луи Люмьер", "Louis Lumière", "ルイ・リュミエール"),
        "died": [1948], "year": 1895, "country": "FR",
        "commons": ["L'Arroseur arrosé", "Arroseur arrose"],
        "seconds": [30, 120],
        "lector": [
            (0.08, ("Первая кинокомедия. Садовник поливает клумбы, а за его спиной — мальчишка.",
                    "The first film comedy. A gardener waters the beds, and behind him is a boy.",
                    "最初の喜劇映画です。庭師が花壇に水をまき、その後ろに少年がいます。")),
            (0.40, ("Мальчишка наступил на шланг. Сейчас будет…", "The boy has stepped on the hose. Now watch…",
                    "少年がホースを踏みました。さあ……")),
            (0.70, ("Догнал! Сто тридцать лет прошло, а мальчишки всё такие же.", "Caught him! A hundred and thirty years on, and boys are just the same.",
                    "捕まえた！百三十年たっても、男の子は変わりませんね。")),
        ],
        "laughs": [0.50, 0.74],
    },
    {
        "id": "lumiere_baby",
        "title": ("Завтрак младенца", "Baby's Meal", "赤ん坊の食事"),
        "maker": ("Луи Люмьер", "Louis Lumière", "ルイ・リュミエール"),
        "died": [1948], "year": 1895, "country": "FR",
        "commons": ["Repas de bébé", "Le Repas de bébé"],
        "seconds": [30, 120],
        "lector": [
            (0.10, ("Огюст Люмьер с женой кормят дочку. Первое семейное кино.",
                    "Auguste Lumière and his wife feed their little daughter. The first home movie.",
                    "オーギュスト・リュミエール夫妻が娘に食事をさせています。最初のホームムービーです。")),
            (0.60, ("Посмотрите на листья за ними: ветер. Зрители тогда удивлялись листьям больше, чем людям.",
                    "Look at the leaves behind them: the wind. Back then audiences marvelled at the leaves more than at the people.",
                    "後ろの木の葉をご覧ください。風です。当時の観客は人よりも木の葉に驚いたそうです。")),
        ],
        "laughs": [0.45],
    },
    {
        "id": "cohl_fantasmagorie",
        "title": ("Фантасмагория", "Fantasmagorie", "ファンタスマゴリー"),
        "maker": ("Эмиль Коль", "Émile Cohl", "エミール・コール"),
        "died": [1938], "year": 1908, "country": "FR",
        "commons": ["Fantasmagorie 1908 Cohl", "Fantasmagorie (1908)"],
        "seconds": [55, 150],
        "lector": [
            (0.06, ("Первый рисованный мультфильм. Мел на чёрной доске — и рука художника.",
                    "The first drawn cartoon. Chalk on a blackboard, and the artist's hand.",
                    "最初の手描きアニメーションです。黒板にチョーク、そして画家の手。")),
            (0.45, ("Человечек превращается во что угодно: в бутылку, в цветок, в слона. Попробуйте угнаться.",
                    "The little man turns into anything: a bottle, a flower, an elephant. Try to keep up.",
                    "小さな男が何にでも変わります。びん、花、象。ついていけますか。")),
        ],
        "laughs": [0.30, 0.82],
    },
    {
        "id": "melies_rubber_head",
        "title": ("Человек с резиновой головой", "The Man with the Rubber Head", "ゴム頭の男"),
        "maker": ("Жорж Мельес", "Georges Méliès", "ジョルジュ・メリエス"),
        "died": [1938], "year": 1901, "country": "FR",
        "commons": ["L'Homme à la tête en caoutchouc", "Man with the Rubber Head"],
        "seconds": [90, 300],
        "lector": [
            (0.08, ("Мельес был фокусником, а кино стало его лучшим фокусом. Учёный в лаборатории…",
                    "Méliès was a stage magician, and film became his best trick. A scientist in his laboratory…",
                    "メリエスは手品師で、映画は彼の最高の手品になりました。実験室の学者……")),
            (0.42, ("…надувает собственную голову мехами! Не пробуйте дома.", "…inflates his own head with a pair of bellows! Don't try it at home.",
                    "……ふいごで自分の頭をふくらませます！ご家庭ではまねしないように。")),
            (0.80, ("Помощник, конечно, перестарался.", "The assistant, of course, overdoes it.", "助手はもちろん、やりすぎです。")),
        ],
        "laughs": [0.48, 0.86],
    },
    {
        "id": "melies_moon",
        "title": ("Путешествие на Луну", "A Trip to the Moon", "月世界旅行"),
        "maker": ("Жорж Мельес", "Georges Méliès", "ジョルジュ・メリエス"),
        "died": [1938], "year": 1902, "country": "FR",
        "commons": ["Le Voyage dans la Lune 1902", "A Trip to the Moon 1902"],
        "seconds": [480, 1100],
        "lector": [
            (0.04, ("Учёные собрались на совет: профессор Барбенфуйе предлагает лететь на Луну.",
                    "The scientists meet in council: Professor Barbenfouillis proposes a trip to the Moon.",
                    "学者たちの会議。バルバンフイエ教授が月への旅を提案します。")),
            (0.20, ("Снаряд строят на крыше, отливают пушку. Как у Жюля Верна — книга есть у нас в библиотеке.",
                    "They build the shell on a rooftop and cast the cannon. As in Jules Verne: the book is in our library.",
                    "屋根の上で砲弾を作り、大砲を鋳造します。ジュール・ヴェルヌの小説そのままです。その本はうちの図書館にもありますよ。")),
            (0.32, ("Прямо в глаз! Этот кадр знает весь мир.", "Right in the eye! The whole world knows this shot.", "目に命中！世界中が知っている場面です。")),
            (0.50, ("Звёзды смотрят из облаков. А на Луне живут селениты — и они не рады гостям.",
                    "Stars peer out of the clouds. And on the Moon live the Selenites, who are not glad of guests.",
                    "雲の間から星がのぞきます。月にはセレナイトが住んでいて、客を歓迎しません。")),
            (0.80, ("Обратно — просто падают с края Луны. В море. Почти как у нас.",
                    "To get back they simply fall off the edge of the Moon. Into the sea. Almost like here.",
                    "帰りは月の端から落ちるだけ。海の中へ。まるでこの町のようです。")),
        ],
        "laughs": [0.33, 0.62, 0.84],
    },
    {
        "id": "chomon_hotel",
        "title": ("Электрический отель", "The Electric Hotel", "電気ホテル"),
        "maker": ("Сегундо де Шомон", "Segundo de Chomón", "セグンド・デ・チョモン"),
        "died": [1929], "year": 1908, "country": "ES",
        "commons": ["Hôtel électrique 1908", "El hotel eléctrico"],
        "seconds": [240, 720],
        "lector": [
            (0.06, ("Супруги приезжают в отель, где всё делает электричество. Чемоданы распаковываются сами.",
                    "A couple arrive at a hotel where electricity does everything. The suitcases unpack themselves.",
                    "夫婦が、何でも電気がやってくれるホテルに着きます。かばんがひとりでに開いて、荷物が片づいていきます。")),
            (0.40, ("Расчёска сама причёсывает, сапоги сами чистятся. Шомон снимал по кадру — сотни часов.",
                    "The comb combs by itself, the boots polish themselves. Chomón shot it frame by frame: hundreds of hours.",
                    "くしがひとりでに髪をとかし、靴がひとりでに磨かれます。チョモンは一コマずつ、何百時間もかけて撮りました。")),
            (0.78, ("А потом пьяный монтёр в машинном зале… ну, вы видите.", "And then a drunk electrician in the machine room… well, you can see.",
                    "それから機械室の酔っ払った電気係が……ご覧のとおりです。")),
        ],
        "laughs": [0.45, 0.82],
    },
    {
        "id": "porter_robbery",
        "title": ("Большое ограбление поезда", "The Great Train Robbery", "大列車強盗"),
        "maker": ("Эдвин Портер", "Edwin S. Porter", "エドウィン・S・ポーター"),
        "died": [1941], "year": 1903, "country": "US",
        "commons": ["The Great Train Robbery 1903", "Great Train Robbery (1903)"],
        "seconds": [480, 900],
        "lector": [
            (0.05, ("Первый вестерн. Бандиты связывают телеграфиста на станции.",
                    "The first western. Bandits tie up the telegraph operator at the station.",
                    "最初の西部劇です。強盗たちが駅の電信係を縛ります。")),
            (0.35, ("Схватка на крыше вагона. Тут, конечно, кукла вместо человека — присмотритесь.",
                    "A fight on the roof of the carriage. That's a dummy instead of a man, of course: look closely.",
                    "客車の屋根の上の格闘。もちろん人の代わりに人形です。よく見てください。")),
            (0.70, ("Девушка находит связанного телеграфиста, а в салуне танцы. Погоня!",
                    "A girl finds the tied-up operator, and there's dancing in the saloon. The chase!",
                    "娘が縛られた電信係を見つけ、酒場ではダンス。追跡です！")),
            (0.96, ("А этот кадр в те годы ставили то в начало, то в конец — как захочет киномеханик.",
                    "And in those days this shot went at the beginning or the end, as the projectionist liked.",
                    "この場面は当時、映写技師の好みで最初か最後に入れられました。")),
        ],
        "laughs": [0.36],
    },
]

# The newsreel before the session, by weekday (Monday 0): the films in order. A festival day shows the Moon.
PROGRAMME = {
    0: ["lumiere_factory", "lumiere_train"],
    1: ["lumiere_sprinkler", "lumiere_baby"],
    2: ["cohl_fantasmagorie", "melies_rubber_head"],
    3: ["chomon_hotel"],
    4: ["lumiere_train", "lumiere_sprinkler"],
    5: ["melies_moon"],
    6: ["porter_robbery"],
}
FESTIVAL = ["melies_moon"]

# Where the films are shown: the hall's screen in the building frame (blender/city/buildings/workers_club.py: kit.cinema_screen
# at (0, YD = 12.6) yaw 180, w 12.2, h 5.6, bottom 2.7; its picture card 6.2 cm in front of the wall). The picture is
# 4:3 on its middle, 2 cm in front of the card; the booth's portholes in the back wall (y 30) throw the beam.
CINEMAS = {
    "rassvet": {
        "plot": "P-CLUB2",
        "screen": {"pos": [0.0, 12.682, 5.5], "w": 7.33, "h": 5.5},
        "booth": [0.0, 29.7, 7.6],
        "hall": {"pos": [0.0, 21.0, 3.0], "radius": 16.0},
        "acts": ["cinema_week", "cinema"],
    },
}

S = [
    ("city.film.card", "{0}. {1}, {2}", "{0}. {1}, {2}", "{0}　{1}（{2}年）"),
    ("city.film.newsreel", "Перед сеансом — киножурнал: {0}.", "Before the film, the newsreel: {0}.", "上映前のニュース映画：{0}。"),
    ("city.film.lector", "Киномеханик: «{0}»", "The projectionist: “{0}”", "映写技師：「{0}」"),
    ("city.film.leave", "E — встать и уйти", "E — get up and leave", "E — 席を立つ"),
    ("city.film.missing", "Плёнка киножурнала не пришла: сразу фильм.", "The newsreel's reel didn't arrive: straight to the film.",
     "ニュース映画のフィルムが届かなかったので、すぐ本編です。"),
    ("city.film.seen", "Старое кино: {0} из {1}", "Old films: {0} of {1}", "古い映画：{0}／{1}"),
    ("city.film.title", "Киножурнал «Рассвета»", "The Rassvet newsreel", "「ラスヴェート」のニュース映画"),
    ("item.ticket_stub.name", "Корешок билета", "Ticket stub", "映画の半券"),
    ("item.ticket_stub.desc", "Розовый корешок из «Рассвета», оторванный по линии. На обороте штамп: ряд и место.",
     "A pink stub from the Rassvet, torn along the line. A stamp on the back: row and seat.",
     "「ラスヴェート」のピンクの半券。ミシン目でちぎってある。裏に列と席のスタンプ。"),
    ("dev.action.cinema.play", "Кино: киножурнал дня в «Рассвете» сейчас (без сеанса)", "Cinema: the day's newsreel at the Rassvet now (no film)",
     "映画：今日のニュース映画を「ラスヴェート」で今すぐ上映（本編なし）"),
]
