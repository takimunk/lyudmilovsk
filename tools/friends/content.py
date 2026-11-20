"""The words of friendship with the townsfolk (backlog item 12, "Friends among the residents"), ru / en / ja. Russian is the source.

Every entry is T(key, ru, en, ja), the same as tools/letters/content.py. The townsfolk come to know the hero: greetings,
talk, quests, gifts and postcards bring them closer; a friend waves first, calls him by a nickname of their own, sits
with him on a bench, the ten open-home owners invite him to tea, close friends take him along on their family's weekend
outing, and the diary page "People" keeps what he knows of each. Calm: no meters, no romance, only lines.
The townsfolk's own spoken lines (greetings, waving, bench talk) are in tools/barks/lines/13_friends.txt.

House style (localization skill): the hero is a teenage boy (ru agreement with him is masculine, ja 僕 in his diary);
a townsperson's generic line is gender-neutral in Russian (no past tense agreeing with the speaker) and in Japanese
avoids 僕/俺/私 where it can; the open-home hosts are known people and keep their own voice (tools/city/content/strings/
homes.json and the letters). Names inside a sentence go after a colon or stand alone, never into a Russian case ending.
Placeholders {0} {1} {2} are the same in all three languages; ja uses full-width ！？：「」.
"""

STRINGS = {}


def T(key, ru, en, ja):
    assert key not in STRINGS, key
    STRINGS[key] = {"ru": ru, "en": en, "ja": ja}
    return key


# --- nicknames: what a friend calls the hero ("Привет, {0}!"), by the friend's persona -------------------------------
# hint: warm, elder, curious, kid, sea, birds, tidy, any. The form of address (ru nominative), en lower case unless a name.

NICKNAMES = [
    ("observer", "curious"),
    ("cabin_boy", "sea"),
    ("bird_boy", "birds"),
    ("neighbour", "any"),
    ("young_man", "elder"),
    ("professor", "birds"),
    ("naturalist", "tidy"),
    ("sunshine", "warm"),
    ("dear_boy", "elder"),
    ("captain", "kid"),
    ("binoculars", "kid"),
    ("sailor", "sea"),
    ("pal", "warm"),
    ("scout", "curious"),
    ("starling", "birds"),
    ("aton", "any"),
    ("colleague", "tidy"),
]
T("friends.nick.observer", "наблюдатель", "observer", "観察くん")
T("friends.nick.cabin_boy", "юнга", "ship's boy", "見習いくん")
T("friends.nick.bird_boy", "птичник", "bird boy", "鳥好きくん")
T("friends.nick.neighbour", "сосед", "neighbour", "ご近所さん")
T("friends.nick.young_man", "молодой человек", "young man", "若いの")
T("friends.nick.professor", "орнитолог", "professor", "鳥博士")
T("friends.nick.naturalist", "наш юннат", "young naturalist", "理科少年")
T("friends.nick.sunshine", "солнышко", "sunshine", "お日さま")
T("friends.nick.dear_boy", "голубчик", "dear boy", "坊や")
T("friends.nick.captain", "капитан", "captain", "キャプテン")
T("friends.nick.binoculars", "Бинокль", "Binoculars", "双眼鏡くん")
T("friends.nick.sailor", "морячок", "sailor", "水兵さん")
T("friends.nick.pal", "дружок", "pal", "相棒")
T("friends.nick.scout", "следопыт", "scout", "探検くん")
T("friends.nick.starling", "скворушка", "little starling", "ムクドリくん")
T("friends.nick.aton", "атоновец", "Aton boy", "アトンくん")
T("friends.nick.colleague", "коллега", "colleague", "同僚くん")

# --- gifts: the hero gives a pocket item ({0}: the nickname, where it's used) ----------------------------------------

T("friends.gift.loved.1", "Ой! Откуда ты знаешь, что я это обожаю? Вот спасибо!",
  "Oh! How did you know I adore this? Thank you!", "わあ！これ大好物なんだ、なんでわかったの？ありがとう！")
T("friends.gift.loved.2", "{0}, ну ты меня балуешь! Это же самое любимое.",
  "{0}, you do spoil me! This is my very favourite.", "{0}、うれしいなあ！これ、いちばん好きなものだよ。")
T("friends.gift.loved.3", "Вот это подарок! Прямо в сердце попал.",
  "Now that's a present! Straight to the heart.", "これはいい贈り物だ！心にずばり命中だよ。")
T("friends.gift.loved.4", "{0}, спасибо! Теперь весь день будет хороший, это точно.",
  "{0}, thank you! Now the whole day's going to be good, no doubt about it.", "{0}、ありがとう！今日は一日いい日になるね、まちがいない。")
T("friends.gift.loved.5", "Как ты угадал? Это ровно то, чего сегодня не хватало.",
  "How did you guess? It's exactly what today was missing.", "どうしてわかったの？今日ちょうど、これが足りなかったんだ。")
T("friends.gift.liked.1", "Спасибо! Очень приятно, правда.", "Thank you! That's really kind.", "ありがとう！ほんとにうれしいよ。")
T("friends.gift.liked.2", "{0}, ну зачем ты... Но спасибо, пригодится.",
  "{0}, you shouldn't have... But thank you, it'll come in handy.", "{0}、気をつかわなくていいのに…でもありがとう、助かるよ。")
T("friends.gift.liked.3", "Какой ты внимательный. Спасибо!", "How thoughtful of you. Thanks!", "よく気がつくね。ありがとう！")
T("friends.gift.liked.4", "О, спасибо, {0}! Сразу настроение лучше.",
  "Oh, thank you, {0}! That's cheered me up already.", "おお、ありがとう、{0}！それだけで元気が出た。")
T("friends.gift.liked.5", "Подарок? Мне? Ну надо же. Спасибо тебе.",
  "A present? For me? Well, I never. Thank you.", "プレゼント？もらっていいの？うれしいな、ありがとう。")
T("friends.gift.plain.1", "Спасибо... Это, честно говоря, не совсем по моей части. Но отдам тому, кто обрадуется.",
  "Thank you... It's not really my sort of thing, to be honest. But I'll pass it on to someone who'll be glad of it.",
  "ありがとう…正直、あまり縁がないものなんだけど。喜びそうな人に渡しておくね。")
T("friends.gift.plain.2", "Ой. Ну, спасибо. Главное ведь не что, а от кого.",
  "Oh. Well, thank you. It's not the what, it's the who.", "おや。うん、ありがとう。大事なのは何かじゃなくて、誰からかだよ。")
T("friends.gift.plain.3", "{0}, спасибо, что вспомнил обо мне. Пристрою в хорошие руки.",
  "{0}, thanks for thinking of me. I'll find it a good home.", "{0}、思い出してくれてありがとう。いい行き先を見つけるよ。")
T("friends.gift.plain.4", "Хм. Неожиданно. Но приятно, что ты обо мне подумал.",
  "Hm. Unexpected. But it's nice that you thought of me.", "ふむ。意外だな。でも、気にかけてくれたのはうれしいよ。")
T("friends.gift.again.1", "Опять подарок? Балуешь! Не каждый же день, {0}.",
  "Another present? You're spoiling me! Not every day, {0}.", "また贈り物？甘やかしすぎだよ！毎日じゃなくていいんだよ、{0}。")
T("friends.gift.again.2", "Нет-нет, на сегодня хватит. Оставь себе, завтра посмотрим.",
  "No, no, that's enough for today. Keep it, we'll see tomorrow.", "いやいや、今日はもう十分。取っておいて、また明日ね。")
T("friends.gift.again.3", "Ты сегодня уже дарил. Лучше просто посиди со мной минутку.",
  "You've already given me something today. Just sit with me a minute instead.", "今日はもうもらったよ。それより、ちょっとだけ一緒に座ってて。")

# who loves which pocket item (a hint is a persona, an age or a job group; sea: fisherman, fishmonger, lifeguard,
# lighthouse_keeper; artist: artist, musician, writer)
GIFTS = [
    ("chocolate", ["kid", "teen", "sweet", "warm"]),
    ("candy", ["kid", "sweet"]),
    ("cookies", ["kid", "elder", "sweet"]),
    ("gingerbread", ["elder", "sweet"]),
    ("pie", ["elder", "adult", "warm"]),
    ("bubliki", ["elder", "adult"]),
    ("vatrushka", ["elder", "kid", "sweet"]),
    ("pastry", ["adult", "sweet"]),
    ("donuts", ["kid", "teen", "sweet"]),
    ("cherries", ["kid", "adult", "artist"]),
    ("condensed_milk", ["kid", "teen", "sweet"]),
    ("hematogen", ["kid"]),
    ("lollipop", ["kid"]),
    ("candy_floss", ["kid"]),
    ("ice_cream", ["kid", "teen"]),
    ("eskimo", ["kid", "teen"]),
    ("coffee", ["adult", "artist", "reader"]),
    ("tea", ["elder", "warm", "reader"]),
    ("kvass", ["adult", "sea"]),
    ("smoked_fish", ["fisherman", "sea", "adult"]),
    ("dried_fish", ["fisherman", "sea"]),
    ("daily_paper", ["reader", "elder"]),
    ("magazine_science", ["curious", "reader", "teen"]),
    ("magazine_travel", ["sea", "curious", "reader"]),
    ("postcard_set", ["artist", "photographer", "warm"]),
    ("stamp_packet", ["curious", "elder", "tidy"]),
    ("notebook", ["artist", "tidy", "curious"]),
    ("adventure_book", ["kid", "teen", "reader"]),
    ("wool_ball", ["knitter"]),
    ("thread_spool", ["knitter", "tidy"]),
    ("buttons_card", ["knitter", "tidy"]),
    ("handkerchief", ["elder", "tidy"]),
    ("feed_grain", ["feeder"]),
    ("feed_peanuts", ["feeder", "kid"]),
    ("feed_rowan", ["feeder"]),
    ("bread_loaf", ["feeder", "elder"]),
    ("toy_duck", ["kid"]),
    ("spinning_top", ["kid"]),
    ("toy_car", ["kid"]),
    ("rubber_ball", ["kid"]),
    ("shell_box", ["artist", "kid", "sea"]),
    ("amber_charm", ["warm", "artist", "photographer"]),
    ("seeds", ["elder", "feeder", "tidy"]),
]

# --- tea at home: the ten open-home owners (their homes: tools/city/content/strings/homes.json) ----------------------
# welcome: the table set, as the hero sees it; q1..q3: what he asks (tied to the flat's things); a1..a3: the answers
# ({0}: the nickname); bye: at the door ({0}); invite: the letter ({0}: the day phrase, {1}: the time).

HOSTS = [dict(id=h, name="friends.host.%s.name" % h, topics=3)
         for h in ["holm", "okada", "singh", "kravets", "okafor", "tadesse", "okonkwo", "santos", "girma", "aoki"]]

# Viktor Sergeyevich Holm: a retired master mariner, a flat like a cabin; the storm painting "Ladoga, Biscay, 1974",
# chess by post with "K." of Murmansk, the watch log of the shore, the waltz record "To Nina, Riga, 1961".
T("friends.host.holm.name", "Виктор Сергеевич Холм", "Viktor Sergeyevich Holm", "ヴィクトル・セルゲーエヴィチ・ホルム")
T("friends.tea.holm.welcome",
  "Холм ставит на стол два стакана в подстаканниках и миску сухарей. Чай он заваривает крепко, по-штурмански, и поворачивает стол так, чтобы обоим было видно восток. «Сахара на борту нет. Есть сухари и прогноз».",
  "Holm sets two glasses in metal holders on the table and a bowl of ship's biscuit. He brews the tea strong, the navigator's way, and turns the table so that both of you can see the east. \"No sugar aboard. There's biscuit and there's the forecast.\"",
  "ホルムは金属のホルダーに入れたグラスを二つと、乾パンの鉢を置く。お茶は航海士流に濃くいれ、ふたりとも東が見えるようにテーブルの向きを変える。「砂糖は積んでいない。乾パンと天気予報ならある」")
T("friends.tea.holm.q1", "Про шторм на картине", "About the storm in the painting", "絵の嵐のこと")
T("friends.tea.holm.q2", "Про шахматы с «К.» из Мурманска", "About the chess with \"K.\"", "「K」とのチェスのこと")
T("friends.tea.holm.q3", "Про пластинку «Нине»", "About the record \"To Nina\"", "「ニーナへ」のレコード")
T("friends.tea.holm.a1",
  "Бискай, семьдесят четвёртый. Трое суток волна выше мачты, а «Ладога» шла и шла, старая калоша. Я тогда стоял на руле и молился, хотя был комсомольцем. «Все живы» — это я приписал, когда вернулся. Самая важная строчка в моей жизни, {0}.",
  "Biscay, seventy-four. Three days of waves higher than the mast, and the Ladoga kept going and going, the old tub. I was at the wheel and I prayed, though I was in the Komsomol. \"All alive\": I wrote that in when we got home. The most important line of my life, {0}.",
  "七十四年のビスケー湾だ。三日三晩、波はマストより高く、それでも古ぼけた「ラドガ」は進みつづけた。私は舵を握って祈っていた。青年同盟員だったのにね。「全員無事」と書き足したのは、帰ってからだ。人生でいちばん大事な一行だよ、{0}。")
T("friends.tea.holm.a2",
  "Костя. Мы вместе учились в мореходке. Сорок лет играем по почте, по ходу в месяц, а иногда и в два. Последняя открытка пришла давно, и я каждый день смотрю в ящик. Он не болен, нет. Просто думает. Костя всегда долго думал над ферзём.",
  "Kostya. We were at the naval college together. Forty years we've played by post, a move a month, sometimes one in two. His last card came a long while ago, and I look in the box every day. He isn't ill, no. He's thinking. Kostya always took his time over the queen.",
  "コースチャだ。商船学校の同期でね。四十年、郵便で指している。一か月に一手、ときには二か月に一手。最後のはがきはだいぶ前で、毎日ポストをのぞいている。病気じゃない、ちがう。考えているだけだ。コースチャはクイーンのことになると、いつも長考するんだ。")
T("friends.tea.holm.a3",
  "Нина. Мы танцевали в Риге, в шестьдесят первом, а через год я ушёл в первый рейс. Она тридцать лет ждала меня на берегу, а я всё приходил и уходил. Теперь я на берегу насовсем, а ждать некому. Поэтому я и смотрю восход: хоть кто-то приходит вовремя.",
  "Nina. We danced in Riga in sixty-one, and a year later I went off on my first voyage. For thirty years she waited for me on shore, and I kept coming and going. Now I'm ashore for good, and there's nobody to wait for. That's why I watch the sunrise: at least somebody turns up on time.",
  "ニーナだ。六十一年にリガで踊って、その一年後に私は最初の航海に出た。三十年、彼女は岸で待っていてくれたのに、私は帰っては出ていくばかりだった。いまは私がずっと岸にいるが、待つ相手がいない。だから日の出を見るんだ。せめて誰かが時間どおりに来るからね。")
T("friends.tea.holm.bye", "Вахту сдал, {0}. Заходи, дверь не заперта: на судне каюты не запирали.",
  "Watch handed over, {0}. Drop in again: the door isn't locked. On board ship we never locked the cabins.",
  "当直交代だ、{0}。また寄りなさい。鍵はかけていない。船では船室に鍵をかけなかったからね。")
T("friends.tea.holm.invite",
  "Юнга!\n\nПриглашаю на флотский чай {0}, к {1}. Склянки пробьют, и заходите: дверь, как известно, не заперта. Расскажу, как пишется настоящий вахтенный журнал. Сахар не обещаю, обещаю сухари.\n\nВ. С. Холм",
  "Ship's boy,\n\nI invite you to navy tea {0}, at {1}. When the bell strikes, come in: the door, as you know, isn't locked. I'll tell you how a real watch log is kept. I don't promise sugar; I promise ship's biscuit.\n\nV. S. Holm",
  "見習い水夫くん\n\n{0}、{1}に海軍式のお茶に招待しよう。時鐘が鳴ったら入ってきなさい。ご存じのとおり、ドアに鍵はかかっていない。本物の航海日誌の書き方を話してあげよう。砂糖は約束しないが、乾パンは約束する。\n\nV・S・ホルム")

# Lev Markovich Okada: an old painter who paints the same window, No. 1112; the sunsets in layers, the woman turned to
# the window, letters to Asya (his late wife), the exhibition at the Rassvet, 1979, tubes labelled in big letters.
T("friends.host.okada.name", "Лев Маркович Окада", "Lev Markovich Okada", "レフ・マルコヴィチ・オカダ")
T("friends.tea.okada.welcome",
  "Окада сдвигает на край стола тюбики «ЖЁЛТЫЙ» и «ЕЩЁ ЖЁЛТЫЙ» и ставит две чашки: одну с синим ободком, другую с отбитой ручкой — её он берёт себе. На блюдце сушки и долька лимона. «Ася всегда клала лимон. Я тоже кладу, хотя не люблю».",
  "Okada pushes the tubes marked YELLOW and MORE YELLOW to the edge of the table and sets out two cups: one with a blue rim, one with a chipped handle, which he keeps for himself. On the saucer, dry rings and a slice of lemon. \"Asya always put lemon in. So do I, though I don't like it.\"",
  "オカダは「きいろ」と「もっときいろ」の絵の具をテーブルの端に寄せて、カップを二つ置く。青い縁のものと、取っ手の欠けたもの。欠けたほうは自分用だ。小皿には小さな輪のパンとレモンがひと切れ。「アーシャはいつもレモンを入れた。私も入れる。好きじゃないんだがね」")
T("friends.tea.okada.q1", "Зачем писать одно окно 1112 раз?", "Why paint one window 1112 times?", "なぜ同じ窓を千百十二枚？")
T("friends.tea.okada.q2", "Про женщину на портрете у окна", "About the woman at the window", "窓辺の女の人の絵のこと")
T("friends.tea.okada.q3", "Про выставку в «Рассвете», 1979", "About the Rassvet show, 1979", "七九年の「ラスヴェト」展")
T("friends.tea.okada.a1",
  "Потому что оно каждый раз другое, {0}. Сегодня море с прозеленью, завтра будет как олово. Я пишу не окно, я пишу день, который в нём стоял. А ещё... однажды солнце сядет иначе, и я хочу в этот момент быть у окна с кистью в руке.",
  "Because it's different every time, {0}. Today the sea has a green tinge; tomorrow it'll be like tin. I don't paint the window, I paint the day that stood in it. And besides... one day the sun will set differently, and I want to be at the window with a brush in my hand.",
  "毎回ちがうからだよ、{0}。今日の海は少し緑がかっていて、明日はスズみたいな色になる。私が描くのは窓じゃない、そこにあった一日なんだ。それにね…いつか太陽がちがう沈み方をする日が来る。そのとき、筆を持って窓辺にいたいんだよ。")
T("friends.tea.okada.a2",
  "Это Ася. Я написал её со спины, потому что она не любила позировать: «Лёва, я тебе не стог сена». Лицо я помню и так. А свет на щеке тогда был такой, что я боялся пошевелиться. Сорок лет прошло, а он всё ещё там.",
  "That's Asya. I painted her from behind because she didn't like sitting: \"Lyova, I'm not a haystack.\" I remember her face without a picture. But the light on her cheek that day was such that I was afraid to move. Forty years have gone by, and it's still there.",
  "それはアーシャだ。後ろ姿なのは、モデルになるのを嫌がったからでね。「レーヴァ、私は干し草の山じゃないのよ」。顔は描かなくても覚えている。でもあの日の頬の光ときたら、動くのがこわいくらいだった。四十年たっても、まだそこにある。")
T("friends.tea.okada.a3",
  "Моя единственная выставка. Фамилию набрали последней и мелко, на открытие пришли одиннадцать человек, и десять из них были Ася. Она одна стояла у каждой картины по десять минут. Это она обвела меня в каталоге красным. Говорила: «Чтобы ты сам себя нашёл».",
  "My only exhibition. They printed my name last and small; eleven people came to the opening, and ten of them were Asya. She stood in front of each painting for ten minutes on her own. It was she who ringed my name in red in the catalogue. She said: \"So you can find yourself.\"",
  "私のたった一度の展覧会だ。名前は最後に小さく印刷されて、初日に来たのは十一人。そのうち十人分はアーシャだった。ひとりで一枚ごとに十分ずつ立って見てくれたからね。カタログの私の名前を赤で囲んだのも彼女だ。「自分で自分を見つけられるように」と言ってね。")
T("friends.tea.okada.bye", "Спасибо, что посидел, {0}. Сегодня допишу тысяча сто тринадцатое: с тобой за столом оно будет теплее.",
  "Thank you for sitting with me, {0}. Tonight I'll finish number eleven hundred and thirteen: with you at the table it'll come out warmer.",
  "いっしょに座ってくれてありがとう、{0}。今夜は千百十三枚目を仕上げるよ。君がいたから、きっとあたたかい色になる。")
T("friends.tea.okada.invite",
  "Мой друг!\n\nПриходите на чай {0}, к {1}. Как раз начнётся свет, ради которого я пишу. Чашка с синим ободком ждёт вас, лимон тоже. Если опоздаете, не беда: окно никуда не денется.\n\nЛ. М. Окада",
  "My friend,\n\nCome to tea {0}, at {1}. That's just when the light I paint for begins. The cup with the blue rim is waiting for you, and so is the lemon. If you're late, no matter: the window isn't going anywhere.\n\nL. M. Okada",
  "友へ\n\n{0}、{1}にお茶にいらっしゃい。ちょうど、私が描きたい光が始まるころです。青い縁のカップとレモンが待っています。遅れてもかまいません。窓はどこへも行きませんから。\n\nL・M・オカダ")

# Ilmar Singh: the warden of the Capsule Tower, a radio ham; twenty years on the air with Gunnar (UA-Lyudmilovsk-11),
# 1 km away; the rounds notebook ("11th floor: somebody crying, left a chocolate"), the solstice mark on the round window.
T("friends.host.singh.name", "Ильмар Сингх", "Ilmar Singh", "イルマル・シン")
T("friends.tea.singh.welcome",
  "Ильмар освобождает место между паяльником и журналом связей и ставит две эмалированные кружки. Чай без сахара, к нему сушки из ящика, где лежат завёрнутые в газету лампы. Над столом тихо шипит эфир, как чайник, который никогда не закипает.",
  "Ilmar clears a space between the soldering iron and the contact log and sets out two enamel mugs. Tea without sugar, and dry rings from the drawer where the valves lie wrapped in newspaper. Over the table the air hisses softly, like a kettle that never boils.",
  "イルマルははんだごてと交信ログのあいだを片づけて、ホーローのマグを二つ置く。紅茶に砂糖はなし。お茶うけは、新聞紙にくるんだ真空管がしまってある引き出しから出てきた小さな輪のパン。机の上では無線が、決して沸かないやかんのように静かにシューシュー鳴っている。")
T("friends.tea.singh.q1", "Про Гуннара из UA-11", "About Gunnar, UA-11", "UA-11のグンナルのこと")
T("friends.tea.singh.q2", "Про тетрадь «Обходы»", "About the rounds notebook", "「見回り」ノートのこと")
T("friends.tea.singh.q3", "Про риску на круглом окне", "About the mark on the window", "丸窓の印のこと")
T("friends.tea.singh.a1",
  "Гуннар — самый близкий мне человек на свете, а живёт в километре. Двадцать лет мы говорили только в эфире: так проще, никто не видит, как ты смущаешься. Пластинки для него до сих пор перевязаны бечёвкой. Отдать что-то лично — это смелость совсем другого рода, {0}.",
  "Gunnar is the closest person in the world to me, and he lives a kilometre away. For twenty years we talked only on the air: it's easier, nobody sees you get flustered. The records for him are still tied up with string. Handing something over in person takes a different kind of courage altogether, {0}.",
  "グンナルは、世界でいちばん近しい人です。なのに住まいは一キロ先。二十年、無線でしか話してこなかった。そのほうが楽なんです。照れているのを見られないから。彼にあげるレコードは、いまもひもで縛ったまま。手渡しするには、まるで別の勇気がいるんですよ、{0}。")
T("friends.tea.singh.a2",
  "Комендант обходит башню каждый вечер. Лампы, двери, краны — это просто. А иногда за дверью кто-то плачет, и тогда я не стучу. Оставляю шоколадку и иду дальше. Утром фантик бывает в мусорке у лифта. Значит, обход прошёл хорошо.",
  "A warden goes round the tower every evening. Lamps, doors, taps: that's easy. But sometimes somebody's crying behind a door, and then I don't knock. I leave a chocolate and walk on. In the morning the wrapper's in the bin by the lift. That means the round went well.",
  "管理人は毎晩、塔を見回ります。電球、ドア、蛇口、そこまでは簡単。でもときどき、ドアの向こうで誰かが泣いている。そういうときはノックしません。チョコレートを置いて先へ行く。朝、エレベーターの横のごみ箱に包み紙があれば、見回りはうまくいったということです。")
T("friends.tea.singh.a3",
  "В день летнего солнцестояния солнце садится ровно в эту риску. Я жду его с часами, как позывной в эфире. Двенадцать лет подряд оно не опоздало ни на минуту. Если бы все мои собеседники были такими пунктуальными, {0}...",
  "On midsummer day the sun sets exactly into that mark. I wait for it with a watch, like a call sign on the air. Twelve years running, it hasn't been a minute late. If only everybody I talk to were that punctual, {0}...",
  "夏至の日、太陽はちょうどあの印のところに沈みます。無線の呼び出し符号を待つみたいに、時計を持って待つんです。十二年続けて、一分も遅れたことがない。話し相手がみんな、あれくらい時間に正確ならね、{0}…")
T("friends.tea.singh.bye", "Конец связи, {0}. Но не совсем: частота 7055, если что.",
  "Over and out, {0}. But not quite: frequency 7055, if you need me.", "交信終了、{0}。でも完全にじゃない。何かあったら7055で。")
T("friends.tea.singh.invite",
  "Приём!\n\nПриглашаю на чай в капсульную башню {0}, к {1}. Поднимайтесь на мой этаж, дверь будет приоткрыта, эфир включён. Кружек у меня ровно две, и вторая с этого дня ваша.\n\nИльмар",
  "Come in!\n\nI invite you to tea at the Capsule Tower {0}, at {1}. Come up to my floor: the door will be ajar and the radio on. I have exactly two mugs, and from today the second one is yours.\n\nIlmar",
  "こちら応答！\n\n{0}、{1}にカプセルタワーでお茶をどうぞ。私の階まで上がってきてください。ドアは少し開けて、無線はつけておきます。マグはちょうど二つ。今日から二つめはあなたのものです。\n\nイルマル")

# Nora Kravets: a student of oceanography from a mountain village; the high-water problem, the paper cranes (one per day
# by the sea), the record of her mountains' songs, mum's letters and parcels.
T("friends.host.kravets.name", "Нора Кравец", "Nora Kravets", "ノーラ・クラヴェツ")
T("friends.tea.kravets.welcome",
  "Нора сдвигает учебники и ставит две разные кружки: одну с гербом её посёлка, другую общажную, с отбитым краем. Чай травяной, мамин, из посылки, к нему тонкие ломтики горного сыра. «Пахнет домом. Ну, моим».",
  "Nora shoves her textbooks aside and sets out two odd mugs: one with her village's crest, the other from the hostel, with a chipped rim. The tea is herbal, her mum's, from the parcel, and there are thin slices of mountain cheese. \"It smells of home. Well, mine.\"",
  "ノーラは教科書をどけて、ちぐはぐなマグを二つ置く。ひとつは故郷の村の紋章入り、もうひとつは寮のもので縁が欠けている。お茶は小包で届いたお母さんのハーブティー、それに薄く切った山のチーズ。「家のにおいがするでしょ。まあ、私の家のね」")
T("friends.tea.kravets.q1", "Про задачу о полной воде", "About the high-water problem", "満潮の問題のこと")
T("friends.tea.kravets.q2", "Про бумажных журавликов", "About the paper cranes", "折り鶴のこと")
T("friends.tea.kravets.q3", "Про пластинку с песнями гор", "About the mountain songs", "山の歌のレコード")
T("friends.tea.kravets.a1",
  "Луна тянет воду, солнце тоже тянет, а наш берег ещё и поворачивает её, как ложка в стакане. Три раза я решала неправильно и один раз чуть не заплакала над тетрадью. А потом пошла на пирс и посмотрела на воду. Оказалось, иногда надо просто посмотреть, а уже потом считать.",
  "The moon pulls the water, the sun pulls it too, and our coast turns it round besides, like a spoon in a glass. Three times I got it wrong, and once I nearly cried over the exercise book. Then I went to the pier and looked at the water. It turns out sometimes you just have to look first and count afterwards.",
  "月が水を引っぱって、太陽も引っぱって、そのうえこの海岸がスプーンでかき混ぜるみたいに水を回すの。三回まちがえて、一回はノートの前で泣きそうになった。それで桟橋へ行って、水を見てきたの。ときには、まず見て、それから計算すればいいんだって、わかった。")
T("friends.tea.kravets.a2",
  "Один журавлик за каждый день у моря. Бабушка говорила: сложишь тысячу — сбудется желание. Я пока не решила, какое. Может, чтобы мама перестала волноваться. Может, чтобы море не надоело. Пока что оно мне не надоедает ни капельки, {0}.",
  "One crane for every day by the sea. Granny used to say: fold a thousand and your wish comes true. I haven't decided which wish yet. Maybe for Mum to stop worrying. Maybe for the sea never to get boring. So far it hasn't got boring one bit, {0}.",
  "海辺で過ごした一日ごとに一羽。千羽折ったら願いがかなうって、おばあちゃんが言ってた。何をお願いするかはまだ決めてないの。お母さんが心配しなくなるように、かな。海に飽きないように、かな。いまのところ、ちっとも飽きてないよ、{0}。")
T("friends.tea.kravets.a3",
  "Это наш хор, из посёлка. Мама поёт там вторым голосом, вон она, если очень прислушаться. Я ставлю пластинку, когда скучаю, а потом сразу кассету с морем. Получается, как будто горы спускаются к воде. Так, наверное, и выглядит мой дом теперь.",
  "That's our choir, from the village. Mum sings second voice; you can hear her if you listen really hard. I put the record on when I'm homesick, and then straight away the tape of the sea. It's as if the mountains come down to the water. I suppose that's what my home looks like now.",
  "村の合唱団なの。お母さんはアルトで歌ってる。すごく耳をすますと聞こえるよ。さみしいときはこのレコードをかけて、すぐに海を録音したカセットをかけるの。そうすると、山が海まで下りてくるみたい。いまの私の家って、きっとそんな形なんだと思う。")
T("friends.tea.kravets.bye", "Спасибо, что зашёл, {0}. Сегодня сложу журавлика с твоим именем.",
  "Thanks for coming, {0}. Tonight I'll fold a crane with your name on it.", "来てくれてありがとう、{0}。今夜はあなたの名前を書いた鶴を折るね。")
T("friends.tea.kravets.invite",
  "Привет!\n\nМама прислала новую посылку, и одной мне столько сыра не съесть. Приходи на чай {0}, к {1}. Обещаю не говорить про приливы. Ну, почти.\n\nНора",
  "Hi!\n\nMum sent another parcel, and I can't eat that much cheese on my own. Come to tea {0}, at {1}. I promise not to talk about tides. Well, almost.\n\nNora",
  "こんにちは！\n\nお母さんからまた小包が届いて、チーズがひとりじゃ食べきれないの。{0}、{1}にお茶しに来て。潮の話はしないって約束する。まあ、ほとんどね。\n\nノーラ")

# Gunnar Okafor: a radio ham (UA-Lyudmilovsk-11), records sorted by weather, the reel of 14 May with the unknown waltz,
# a striped sailor's scarf over his bed; "Capsule: tea with jam".
T("friends.host.okafor.name", "Гуннар Окафор", "Gunnar Okafor", "グンナル・オカフォー")
T("friends.tea.okafor.welcome",
  "Гуннар снимает наушники с подушки, сдувает пыль с двух кружек и открывает банку вишнёвого варенья. Чай он наливает под пластинку с полки «Для солнца». «На карте у меня так и написано: чай с вареньем — лучшая связь».",
  "Gunnar lifts the headphones off his pillow, blows the dust off two mugs and opens a jar of cherry jam. He pours the tea to a record from the \"For sun\" shelf. \"It's written on my map: tea with jam is the best connection.\"",
  "グンナルは枕の上のヘッドホンをどけて、マグ二つのほこりを吹き、サクランボのジャムのびんを開ける。「太陽のための」棚のレコードをかけながら、お茶をつぐ。「地図にも書いてあるだろ。ジャム入りの紅茶は最高の通信だって」")
T("friends.tea.okafor.q1", "Почему пластинки «по погоде»?", "Why records by weather?", "レコードを天気で分ける訳")
T("friends.tea.okafor.q2", "Про мелодию с 14 мая", "About the tune from 14 May", "五月十四日の曲のこと")
T("friends.tea.okafor.q3", "Про шарф-тельняшку над кроватью", "About the striped sailor's scarf", "しま模様のマフラー")
T("friends.tea.okafor.a1",
  "Потому что музыка — та же погода, {0}. Под дождь нужен Шопен, под ветер — марш, под солнце — что угодно, лишь бы с трубой. Если поставить не то, день пойдёт криво, проверено. «Для солнца» самая толстая полка: я оптимист, хоть и живу на Балтике.",
  "Because music is weather too, {0}. Rain wants Chopin, wind wants a march, sun wants anything at all, so long as there's a trumpet in it. Put on the wrong one and the day goes crooked: tried and tested. \"For sun\" is the fattest shelf: I'm an optimist, even living on the Baltic.",
  "音楽も天気みたいなものだからさ、{0}。雨にはショパン、風には行進曲、晴れには何でもいい、トランペットが入っていればね。合わないのをかけると一日がゆがむ。実証ずみだ。「太陽のための」棚がいちばん分厚いのは、バルト海に住んでいても楽天家だからさ。")
T("friends.tea.okafor.a2",
  "Полгода я искал её по всему миру: спрашивал Тикси, спрашивал Вальпараисо. А она играла в «Тарелке», в автомате, за углом. Так часто бывает: самое нужное звучит рядом, только помехи мешают расслышать. Теперь включаю её, когда Ильмар болеет.",
  "Half a year I searched the world for it: I asked Tiksi, I asked Valparaíso. And it was playing in the jukebox at the Tarelka, round the corner. That's how it often goes: the thing you need is playing close by, only the interference keeps you from hearing it. Now I put it on when Ilmar's ill.",
  "半年、世界じゅうを探した。ティクシにも、バルパライソにも聞いた。なのに、角を曲がった「タレルカ」のジュークボックスで鳴っていたんだ。よくあることさ。いちばん大事なものはすぐそばで鳴っているのに、雑音で聞こえない。いまはイルマルが寝こんだときに、あれをかけるんだ。")
T("friends.tea.okafor.a3",
  "Я десять лет был радистом на сухогрузе. Шарф подарил штурман, когда я списывался на берег: «Чтобы спалось, как в качку». На берегу мне долго не спалось совсем. Потом я поставил антенну, и мир опять стал качаться, как надо.",
  "I was a radio operator on a cargo ship for ten years. The navigator gave me the scarf when I signed off ashore: \"So you'll sleep like in a swell.\" Ashore I couldn't sleep at all for a long time. Then I put up an aerial, and the world started rocking again, the way it should.",
  "十年、貨物船の通信士だったんだ。陸に上がるとき、航海士がこのマフラーをくれた。「揺れているときみたいに眠れるように」ってね。陸では長いこと全然眠れなかった。それからアンテナを立てたら、世界がまた、ちょうどいい具合に揺れはじめた。")
T("friends.tea.okafor.bye", "Заходи ещё, {0}. В эфире — вежливо, в гостях — запросто.",
  "Come again, {0}. On the air, politely; as a guest, just as you are.", "また来いよ、{0}。無線ではていねいに、うちでは気楽にな。")
T("friends.tea.okafor.invite",
  "Помощник!\n\nОткрыл новую банку варенья, а без гостя она не открывается как следует. Приходи на чай {0}, к {1}. Пластинку выберем по погоде.\n\nГуннар",
  "Helper,\n\nI've opened a new jar of jam, and it never opens properly without a guest. Come to tea {0}, at {1}. We'll pick the record by the weather.\n\nGunnar",
  "助手くん\n\nジャムの新しいびんを開けたんだが、お客がいないと本当には開かないものでね。{0}、{1}にお茶に来てくれ。レコードは天気で選ぼう。\n\nグンナル")

# Valentina Tadesse: a painter of fish (79), with Hamish (77), who fishes; the aquarium with 54 names for 8 fish,
# the blank canvas "For the sun. When I dare", the portrait "Hamish. First date. Nothing biting", Hamish's lemon drops.
T("friends.host.tadesse.name", "Валентина Тадессе", "Valentina Tadesse", "ヴァレンチナ・タデッセ")
T("friends.tea.tadesse.welcome",
  "Валентина ставит чашки с нарисованными рыбками, у каждой своё имя на донышке. Хэмиш приносит из кухни бутерброды и жёлтую банку: «Это мои леденцы. Но гостям можно». Аквариум пускает пузыри, будто тоже хочет за стол.",
  "Valentina sets out cups with painted fish, each with its own name on the bottom. Hamish brings sandwiches from the kitchen and the yellow jar: \"These are my lemon drops. But guests may.\" The aquarium blows bubbles as if it wants to join the table too.",
  "ヴァレンチナが魚の絵のカップを並べる。どれも底にそれぞれの名前が書いてある。ハミッシュが台所からサンドイッチと黄色いびんを持ってくる。「これはぼくのドロップ。でもお客さんならいいよ」。水槽が泡を立てる。自分も席につきたいみたいに。")
T("friends.tea.tadesse.q1", "Почему у восьми рыб 54 имени?", "Why 54 names for eight fish?", "八匹に名前が五十四個？")
T("friends.tea.tadesse.q2", "Про чистый холст «Для солнца»", "About the canvas \"For the sun\"", "「太陽のための」画布")
T("friends.tea.tadesse.q3", "Про портрет с удочкой", "About the portrait with the rod", "釣りざおの肖像画のこと")
T("friends.tea.tadesse.a1",
  "Рыбы живут недолго, а имена я не выбрасываю. Гамлет был первый, ещё в шестидесятых. Теперь у нас Гамлет Седьмой, и характер у него, поверьте, фамильный. Остальные живут на картинах. Там им ничего не грозит, {0}.",
  "Fish don't live long, but I don't throw the names away. Hamlet was the first, back in the sixties. Now we have Hamlet the Seventh, and he has the family temper, believe me. The rest live on in the paintings. Nothing can happen to them there, {0}.",
  "魚は長く生きられないけれど、名前は捨てないの。最初のハムレットは六十年代。いまはハムレット七世で、性格はまちがいなく家系ゆずりよ。ほかの子たちは絵の中で暮らしている。そこなら何も心配ないから、{0}。")
T("friends.tea.tadesse.a2",
  "Пятьдесят лет я пишу рыб, потому что рыбы не спрашивают, похоже ли. А солнце спросит. Внучка просит: «Бабушка, нарисуй солнце». Грунт уже пожелтел от ожидания. Хэмиш говорит, что холст сам скоро станет солнцем, без меня.",
  "Fifty years I've painted fish, because fish don't ask whether it's a likeness. The sun will. My granddaughter asks: \"Granny, paint the sun.\" The ground on the canvas has gone yellow with waiting. Hamish says it'll turn into the sun by itself soon, without me.",
  "五十年、魚を描いてきたのは、魚は似ているかなんて聞かないから。でも太陽は聞いてくる。孫は「おばあちゃん、太陽を描いて」って言うの。下地はもう待ちくたびれて黄ばんでしまった。ハミッシュは、そのうち私がいなくても勝手に太陽になるって言うのよ。")
T("friends.tea.tadesse.a3",
  "Наше первое свидание. Он позвал меня на рыбалку, три часа не клевало, а он всё улыбался, будто клюёт. Я тогда подумала: с таким человеком не страшно и пятьдесят лет без улова. Так и вышло. Хэмиш, не слушай, это я не тебе.",
  "Our first date. He invited me fishing; nothing bit for three hours, and he kept smiling as if something was biting. I thought then: with a man like that, even fifty years without a catch wouldn't be frightening. And that's how it turned out. Hamish, don't listen, I'm not talking to you.",
  "最初のデートよ。釣りに誘われて、三時間まったく釣れないのに、この人はずっと、かかったみたいににこにこしていた。そのとき思ったの。こんな人となら、五十年釣れなくてもこわくないって。そのとおりになった。ハミッシュ、聞かないで。あなたに言ってるんじゃないの。")
T("friends.tea.tadesse.bye", "Заходи, {0}, когда захочешь. Звонить не надо. И рыб не кормить!",
  "Come whenever you like, {0}. No need to ring. And don't feed the fish!", "いつでもいらっしゃい、{0}。呼び鈴はいらないわ。それから、魚にえさはあげないでね！")
T("friends.tea.tadesse.invite",
  "Дорогой мальчик!\n\nЖдём вас на чай в мастерской {0}, к {1}. Хэмиш обещает уху, если клюнет, и бутерброды, если нет. Леденцы он тоже обещает, но тут я ему не верю.\n\nВ. Тадессе",
  "Dear boy,\n\nWe'll expect you to tea in the studio {0}, at {1}. Hamish promises fish soup if they bite and sandwiches if they don't. He promises lemon drops too, but there I don't believe him.\n\nV. Tadesse",
  "やさしい坊やへ\n\n{0}、{1}にアトリエでお茶をしましょう。ハミッシュは、釣れたら魚のスープ、釣れなかったらサンドイッチを約束しています。ドロップも約束していますけれど、それは信用していません。\n\nV・タデッセ")

# Yasmin Okonkwo: a barber on the promenade (37); her husband Mads, a potter, works away two weeks in three; their son
# Emiliano (8) made a clay whale for the wheel, "fire it when you come"; the portrait "Yasmin at work", the calendar.
T("friends.host.okonkwo.name", "Ясмин Оконкво", "Yasmin Okonkwo", "ヤスミン・オコンクウォ")
T("friends.tea.okonkwo.welcome",
  "Ясмин ставит на стол глиняные чашки, кривоватые и все разные: Мадс лепил, Эмилиано помогал. На тарелке печенье в форме китов, у одного два хвоста. Эмилиано садится рядом и строго следит, чтобы я взял именно этого.",
  "Yasmin sets clay cups on the table, a little lopsided and all different: Mads made them, Emiliano helped. On the plate, whale-shaped biscuits, one with two tails. Emiliano sits down beside me and watches sternly to make sure I take that one.",
  "ヤスミンが、少しゆがんだ、ひとつずつ形のちがう陶器のカップを並べる。マスが作って、エミリアーノが手伝ったものだ。お皿にはクジラの形のクッキー。一枚だけ尾が二本ある。エミリアーノがとなりに座って、僕がちゃんとそれを取るか、真剣に見張っている。")
T("friends.tea.okonkwo.q1", "Про кита на гончарном круге", "About the whale on the wheel", "ろくろの上のクジラ")
T("friends.tea.okonkwo.q2", "Про портрет «Ясмин на работе»", "About \"Yasmin at work\"", "「仕事中のヤスミン」の絵")
T("friends.tea.okonkwo.q3", "Сколько дней до субботы?", "How many days to Saturday?", "土曜日まであと何日？")
T("friends.tea.okonkwo.a1",
  "Кит ждёт папу, а мы ждём вместе с китом. Я поливаю его каждое утро, как цветок, чтобы не треснул. Эмилиано хотел ему второй хвост, Мадс сказал «ладно, пусть два». Когда обожгут, он будет самым кривым и самым любимым китом на свете.",
  "The whale is waiting for Dad, and we wait along with the whale. I water it every morning like a flower so it won't crack. Emiliano wanted it to have a second tail; Mads said, \"All right, let it have two.\" Once it's fired, it'll be the most crooked and most loved whale in the world.",
  "クジラはパパを待っていて、私たちもクジラといっしょに待っているの。ひびが入らないように、毎朝お花みたいに水をやってる。エミリアーノが尾をもう一本ほしがって、マスは「いいよ、二本にしよう」って。焼き上がったら、世界でいちばんゆがんでいて、いちばん愛されるクジラになるわ。")
T("friends.tea.okonkwo.a2",
  "Мадс писал его, пока я стригла его самого. Я велела не шевелиться, а он шевелил кистью. Похоже вышло, правда? Он не художник, он просто долго смотрел. Мне никто никогда не дарил ничего лучше, {0}.",
  "Mads painted it while I was cutting his hair. I told him to keep still, and he kept his brush moving. It's a good likeness, isn't it? He's no artist, he just looked for a long time. Nobody has ever given me anything better, {0}.",
  "マスが描いたの。私があの人の髪を切っているあいだにね。動かないでって言ったのに、筆だけは動かしてた。似てるでしょう？画家じゃないの、ただ長いこと見ていただけ。あれよりすてきな贈り物はもらったことがないわ、{0}。")
T("friends.tea.okonkwo.a3",
  "Эмилиано зачёркивает клетки каждый вечер, а иногда и утром, на всякий случай. Две недели — это триста тридцать шесть часов, Мадс их тоже считает. Ничего, мы привыкли. Зато когда он приезжает, у нас праздник на целые выходные, и глина на всех полах.",
  "Emiliano crosses off a square every evening, and sometimes in the morning too, just in case. Two weeks is three hundred and thirty-six hours; Mads counts them too. Never mind, we're used to it. But when he comes home, we have a holiday all weekend, and clay on every floor.",
  "エミリアーノは毎晩マス目を消すの。念のために朝も消すことがあるわ。二週間は三百三十六時間。マスも数えてる。平気よ、慣れてるから。そのかわり帰ってきたら週末じゅうお祭りで、床という床が粘土だらけ。")
T("friends.tea.okonkwo.bye", "Заходи ещё, {0}. И если надо подстричься — набережная, кресло у окна, без очереди.",
  "Come again, {0}. And if you need a haircut: the promenade, the chair by the window, no queue for you.",
  "また来てね、{0}。髪を切りたくなったら、遊歩道のお店の窓ぎわの椅子へ。あなたは並ばなくていいわ。")
T("friends.tea.okonkwo.invite",
  "Привет!\n\nЭмилиано велел написать, что приглашает «того, кто знает про кита», на чай {0}, к {1}. Печенье в форме китов уже в духовке, хвосты лепил он сам. Приходи в мастерскую, дверь открыта.\n\nЯсмин и Эмилиано",
  "Hi!\n\nEmiliano told me to write that he invites \"the one who knows about the whale\" to tea {0}, at {1}. The whale biscuits are in the oven already; he shaped the tails himself. Come to the studio: the door's open.\n\nYasmin and Emiliano",
  "こんにちは！\n\n「クジラのことを知ってる人」を、{0}、{1}のお茶に招待します、と書くようにエミリアーノに言われました。クジラのクッキーはもうオーブンの中。尾はあの子が自分で作りました。工房へどうぞ、ドアは開いています。\n\nヤスミンとエミリアーノ")

# The Santos family (names invented in homes.json only by initials: "E." and Vera): Vera and Emil, Lina (about 10),
# Timo (7), baby Noel; the Venus contest (the telescope in the attic), Timo's bird poster, the painting "Our first day
# by the sea". Vera answers.
T("friends.host.santos.name", "Вера и Эмиль Сантос", "Vera and Emil Santos", "ヴェラとエミル・サントス")
T("friends.tea.santos.welcome",
  "Вера ставит на кухонный стол четыре чашки, пятую для меня и бутылочку для Ноэля. Эмиль режет пирог на семь кусков: «Седьмой — Ноэлю, на вырост». Лина и Тимо шёпотом спорят, кто сядет рядом с гостем, и садятся оба.",
  "Vera sets four cups on the kitchen table, a fifth for me and a bottle for Noel. Emil cuts the pie into seven pieces: \"The seventh is Noel's, to grow into.\" Lina and Timo argue in whispers about who'll sit next to the guest, and both do.",
  "ヴェラが台所のテーブルにカップを四つ、僕の分の五つめ、それにノエルの哺乳びんを置く。エミルはパイを七つに切る。「七つめはノエルの分、大きくなったらね」。リナとティモは、どっちがお客さんの隣に座るかひそひそ言い合って、結局ふたりとも座る。")
T("friends.tea.santos.q1", "Кто же первым увидел Венеру?", "So who saw Venus first?", "金星を最初に見たのは？")
T("friends.tea.santos.q2", "Про плакат с птицами", "About the bird poster", "鳥のポスターのこと")
T("friends.tea.santos.q3", "Про картину «Первый день у моря»", "About \"Our first day by the sea\"", "「海での最初の日」の絵")
T("friends.tea.santos.a1",
  "Лина! Кто-то оставил ей в тетради подсказку, и она клянётся, что не знает кто. Тимо требует пересмотра, Эмиль моет посуду и ворчит, что судья подкуплен. Судья, между прочим, я. И я никому не скажу, что почерк в тетради мальчишеский, {0}.",
  "Lina! Somebody left a hint in her exercise book, and she swears she doesn't know who. Timo demands a recount, and Emil does the dishes, grumbling that the judge was bribed. The judge, by the way, is me. And I won't tell anyone that the handwriting in the book is a boy's, {0}.",
  "リナよ！誰かがノートにヒントを書いてくれたんですって。本人は誰だか知らないって言い張ってる。ティモはやり直しを要求して、エミルは審判が買収されたってぼやきながら皿洗い。ちなみに審判は私。ノートの字が男の子の字だったことは、誰にも言わないわ、{0}。")
T("friends.tea.santos.a2",
  "Это Тимо. Он отмечает каждую птицу и злится, что альбатросы у нас не летают. Ещё он нарисовал там солнечную птицу, которая приснилась. Теперь он думает, что ты её найдёшь, раз ты из птичьего клуба. Не разочаровывай его сразу, ладно?",
  "That's Timo. He ticks off every bird and is cross that albatrosses don't fly here. He also drew a sun bird there that he saw in a dream. Now he thinks you'll find it, since you're from the bird club. Don't let him down straight away, will you?",
  "ティモよ。鳥を見るたびに印をつけて、ここにはアホウドリが飛んでこないって怒ってるの。夢で見た太陽の鳥まで描いてある。あなたは鳥のクラブの人だから、見つけてくれるって思いこんでるのよ。すぐにがっかりさせないであげてね。")
T("friends.tea.santos.a3",
  "Мы приехали сюда с одним чемоданом и двумя детьми, а Ноэля ещё не было. Весь первый день просидели на берегу и никуда не пошли, даже квартиру смотреть. Эмиль нарисовал это через год, по памяти. Говорит, тогда было солнечнее. По-моему, просто мы были моложе.",
  "We came here with one suitcase and two children; Noel wasn't born yet. We sat on the shore the whole first day and went nowhere, not even to see the flat. Emil painted it a year later, from memory. He says it was sunnier then. I think we were just younger.",
  "スーツケースひとつと子ども二人でここへ来たの。ノエルはまだいなかった。最初の日はずっと浜辺に座って、部屋を見にも行かなかった。エミルが一年後に、思い出して描いたの。あのころはもっと晴れてたって言うけど、私たちが若かっただけだと思うわ。")
T("friends.tea.santos.bye", "Приходи ещё, {0}! Только в следующий раз ты моешь посуду: таковы правила дома.",
  "Come again, {0}! Only next time you do the dishes: house rules.", "また来てね、{0}！ただし次はあなたが皿洗い。それがうちの決まりよ。")
T("friends.tea.santos.invite",
  "Здравствуй!\n\nВсе Сантосы (их пятеро, считая Ноэля) приглашают тебя на чай с пирогом {0}, к {1}. Тимо приготовил вопросы про птиц, Лина — про Венеру. Обувь можно не снимать, у нас её и так пять пар.\n\nВера и Эмиль",
  "Hello!\n\nAll the Santoses (five of them, counting Noel) invite you to tea and pie {0}, at {1}. Timo has questions ready about birds, Lina about Venus. No need to take your shoes off: we've five pairs in the hall as it is.\n\nVera and Emil",
  "こんにちは！\n\nサントス家一同（ノエルを入れて五人）より、{0}、{1}のお茶とパイにご招待します。ティモは鳥の質問を、リナは金星の質問を用意しています。靴は脱がなくてけっこう。玄関にはもう五足ありますから。\n\nヴェラとエミル")

# Daisuke Girma: sells apples at the market (38), watches sunspots through a filter every day; his wife Nino works away
# (home on Fridays), their son Tomohiko, "Tomo" (8), drew the sun on the night chart; the still life with the teapot.
T("friends.host.girma.name", "Дайсукэ Гирма", "Daisuke Girma", "大輔・ギルマ")
T("friends.tea.girma.welcome",
  "Дайсукэ ставит на стол чайник — тот самый, с натюрморта, — и режет яблоки, красные с жёлтым боком. Томо приносит третью чашку и ставит её на мамино место: «Это на пятницу. Но пусть пока стоит».",
  "Daisuke puts the teapot on the table, the very one from the still life, and slices apples, red with a yellow cheek. Tomo brings a third cup and sets it at his mum's place: \"That's for Friday. But it can stand there for now.\"",
  "大輔が、静物画に描かれていたあのティーポットをテーブルに置き、黄色いほっぺの赤いリンゴを切る。智彦が三つめのカップを持ってきて、ママの席に置く。「これは金曜日の分。でも、いまから置いとくの」")
T("friends.tea.girma.q1", "Про солнце на ночной карте", "About the sun on the night chart", "夜の星図の太陽のこと")
T("friends.tea.girma.q2", "Про тетрадь «Пятна»", "About the \"Spots\" notebook", "「黒点」ノートのこと")
T("friends.tea.girma.q3", "Когда приезжает мама?", "When does Tomo's mum come home?", "ママはいつ帰るの？")
T("friends.tea.girma.a1",
  "Томо было пять. Я сказал, что ночью солнца нет, и он обиделся и нарисовал его прямо посреди созвездий. Я потом полночи думал: а ведь он прав. Оно есть всегда, просто с другой стороны. Так и с теми, кто далеко, {0}.",
  "Tomo was five. I told him there's no sun at night, and he got upset and drew it right in the middle of the constellations. Afterwards I lay awake half the night thinking: he's right, you know. It's always there, just on the other side. It's like that with people who are far away too, {0}.",
  "智彦が五歳のときだ。夜は太陽がないよって言ったら、むくれて星座のまんなかに描いたんだ。あとで夜中まで考えた。あの子が正しいって。太陽はいつもある、ただ反対側にあるだけだ。遠くにいる人も同じだよ、{0}。")
T("friends.tea.girma.a2",
  "Три года ни одного пропуска. Утром, до рынка, пять минут у телескопа, с фильтром — Томо проверяет. Пятна приходят и уходят, как покупатели. Нино я пишу каждый вечер, сколько их было. Она говорит, это самые странные любовные письма на свете.",
  "Three years without missing a day. In the morning, before the market, five minutes at the telescope, with the filter: Tomo checks. The spots come and go, like customers. Every evening I write to Nino how many there were. She says they're the strangest love letters in the world.",
  "三年間、一日も欠かしていない。朝、市場へ行く前に五分だけ望遠鏡をのぞく。フィルターつきでね。智彦が確認するんだ。黒点はお客さんみたいに来ては去っていく。毎晩、その数をニノに書き送っている。世界でいちばん変わったラブレターだって、彼女は言うよ。")
T("friends.tea.girma.a3",
  "В пятницу вечером, поездом. Томо в этот день рисует с утра, а я стараюсь не опоздать с рынка: на календаре так и написано. Вчера там появилась ещё одна звёздочка, совсем маленькая. Никто в семье не признаётся. Ну, пусть будет.",
  "On Friday evening, by train. That day Tomo draws from the morning on, and I try not to be late back from the market: it says so on the calendar. Yesterday another little star appeared there, a tiny one. Nobody in the family owns up to it. Well, let it stay.",
  "金曜日の夜、汽車でね。その日、智彦は朝から絵を描いて、私は市場から遅れないようにする。カレンダーにもそう書いてある。昨日、そこに小さな星がひとつ増えていた。家族の誰も、自分だとは言わない。まあ、そのままにしておこう。")
T("friends.tea.girma.bye", "Приходи, {0}, и в ясный день загляни в телескоп. Только с фильтром, Томо проследит.",
  "Come again, {0}, and on a clear day have a look through the telescope. Only with the filter: Tomo will make sure.",
  "また来なさい、{0}。晴れた日には望遠鏡ものぞいてごらん。フィルターつきでね。智彦が見張っているから。")
T("friends.tea.girma.invite",
  "Здравствуй!\n\nПриходи к нам на чай {0}, к {1}. Будут яблоки с рынка и, если небо позволит, свежие пятна на солнце. Томо говорит, что нарисует тебя, но только если будешь сидеть смирно.\n\nДайсукэ и Томо",
  "Hello!\n\nCome to us for tea {0}, at {1}. There'll be apples from the market and, if the sky allows, fresh spots on the sun. Tomo says he'll draw you, but only if you sit still.\n\nDaisuke and Tomo",
  "こんにちは！\n\n{0}、{1}にうちへお茶を飲みにおいで。市場のリンゴと、空がよければ、できたての太陽の黒点もあるよ。じっと座っていられるなら絵を描いてあげる、と智彦が言っている。\n\n大輔と智彦")

# Rimma Aoki: a seamstress; the linen dress with suns on the hem (for a market girl's graduation), the swatches named by
# weather (the yellow "as at seven in the evening"), the Chaika machine "Don't rush me", the patchwork quilt.
T("friends.host.aoki.name", "Римма Аоки", "Rimma Aoki", "リンマ・アオキ")
T("friends.tea.aoki.welcome",
  "Римма убирает со стола выкройки, втыкает иголку в подушечку и ставит две чашки из разных сервизов: «Ни одна не лишняя, каждая от кого-то». К чаю сухари с маком и мёд, жёлтый, «как в шесть утра».",
  "Rimma clears the patterns off the table, sticks her needle into the pincushion and sets out two cups from different sets: \"Not one too many: each is from somebody.\" With the tea, poppy-seed rusks and honey, yellow \"as at six in the morning\".",
  "リンマは型紙を片づけ、針を針山に刺して、ちがうセットのカップを二つ置く。「余計なものはひとつもないの。どれも誰かからのもの」。お茶うけはケシの実のラスクと、「朝六時みたいな」黄色のはちみつ。")
T("friends.tea.aoki.q1", "Про солнца на подоле", "About the suns on the hem", "すその太陽のこと")
T("friends.tea.aoki.q2", "Про одеяло из лоскутов", "About the patchwork quilt", "つぎはぎの布団のこと")
T("friends.tea.aoki.q3", "Про машинку «Чайка»", "About the Chaika machine", "ミシン「チャイカ」のこと")
T("friends.tea.aoki.a1",
  "Девочка с рынка принесла лён и попросила просто платье, к выпускному. Она не просила солнц. Но у неё такое лицо, будто она каждое утро встречает рассвет. Последнее солнце я дошила с тем жёлтым, «как в семь вечера». Кто-то положил образец на нужное место. Не знаешь кто, {0}?",
  "A girl from the market brought linen and asked for a plain dress, for her graduation. She didn't ask for suns. But she has the face of someone who meets the dawn every morning. I sewed the last sun with that yellow, \"as at seven in the evening\". Somebody laid the swatch in the right place. You don't know who, {0}?",
  "市場の女の子が亜麻布を持ってきて、卒業式用にふつうのワンピースを、と頼んだの。太陽なんて頼まれていない。でもあの子、毎朝夜明けを迎えているみたいな顔をしているのよ。最後の太陽は「夜七時みたいな」黄色で縫ったわ。誰かが見本をちょうどいい場所に置いてくれたの。誰だか知らない、{0}？")
T("friends.tea.aoki.a2",
  "Каждый лоскут от чьего-то платья: свадебного, школьного, того, в котором провожали. Я шила полгорода, и от каждого у меня остался кусочек. Под этим одеялом никогда не бывает одиноко. А жёлтый в середине — мой. Я всё ещё ищу, какой он на самом деле.",
  "Every patch is from somebody's dress: a wedding dress, a school dress, the one they wore to see someone off. I've sewn for half the town, and from each of them I kept a scrap. Under this quilt it's never lonely. And the yellow in the middle is mine. I'm still finding out what it really is.",
  "どの布切れも誰かの服のものよ。花嫁衣装、制服、誰かを見送ったときの服。町の半分の服を縫って、ひとりひとりから少しずつ残してあるの。この布団の下では、さみしくなることがない。まんなかの黄色は私の。それが本当はどんな色なのか、まだ探しているところ。")
T("friends.tea.aoki.a3",
  "«Чайка» старше меня, это мамина. Мама говорила: машинку нельзя торопить, и человека тоже. Я наклеила это на неё в год, когда всё делала бегом и всё переделывала. Теперь мы обе шьём медленно. И почти ничего не распарываем.",
  "The Chaika is older than me; it was Mum's. Mum used to say you mustn't rush a machine, or a person either. I stuck that note on it the year I did everything at a run and had to redo everything. Now we both sew slowly. And we hardly ever unpick a thing.",
  "「チャイカ」は私より年上なの。母のミシンよ。ミシンも人も急かしちゃいけない、と母は言っていた。何でも駆け足でやって、何でもやり直していた年に、あの貼り紙をしたの。いまは二人ともゆっくり縫う。ほどくことは、ほとんどなくなったわ。")
T("friends.tea.aoki.bye", "Заходи, {0}. Пуговицу пришить или рукав подшить — ко мне, без очереди.",
  "Drop in again, {0}. A button to sew on or a sleeve to take up: come to me, no queue.", "また寄ってね、{0}。ボタンつけでも袖上げでも、あなたなら順番待ちなしよ。")
T("friends.tea.aoki.invite",
  "Здравствуй!\n\nПлатье с солнцами готово и уехало к выпускнице, а у меня наконец свободный вечер. Приходи на чай {0}, к {1}. Мёд будет, сухари будут, машинку обещаю не включать.\n\nР. Аоки",
  "Hello!\n\nThe dress with the suns is finished and gone to its graduate, and at last I have a free evening. Come to tea {0}, at {1}. There'll be honey, there'll be rusks, and I promise not to switch on the machine.\n\nR. Aoki",
  "こんにちは！\n\n太陽のワンピースが仕上がって卒業生のところへ行き、やっと夜が空きました。{0}、{1}にお茶にいらっしゃい。はちみつもラスクもあります。ミシンはかけないと約束します。\n\nR・アオキ")

# the tea: shared words
T("friends.tea.served.1", "Чай разлит. Пар поднимается над чашками, и разговор начинается сам собой.",
  "The tea is poured. Steam rises over the cups, and the talk begins by itself.", "お茶がつがれた。湯気が立ちのぼり、話は自然に始まる。")
T("friends.tea.served.2", "Хозяин дома придвигает мне чашку и тарелку. Пахнет чаем и чужим уютным домом.",
  "My host moves a cup and a plate over to me. It smells of tea and of someone else's cosy home.", "家の人がカップとお皿をこちらへ寄せてくれる。お茶と、よその家のあたたかいにおいがする。")
T("friends.tea.served.3", "На столе чай и что-то к чаю. За окном шумит море, а здесь тихо и тепло.",
  "On the table, tea and something to go with it. Outside the sea murmurs; in here it's quiet and warm.", "テーブルにはお茶とお茶うけ。窓の外では海が鳴り、ここは静かであたたかい。")
T("friends.tea.prompt", "E — сесть пить чай", "E — sit down to tea", "E：お茶の席につく")
T("friends.tea.title", "Чаепитие: {0}", "Tea: {0}", "お茶の時間：{0}")
T("friends.tea.keys", "1–3 — спросить   Esc — встать из-за стола", "1–3 — ask   Esc — get up from the table", "1〜3：たずねる　Esc：席を立つ")
T("friends.tea.not_now", "Тебя ждут {0} к {1}.", "You're expected {0} at {1}.", "{0}の{1}に招かれている。")
T("friends.tea.diary", "Чай в гостях. Хозяин дома: {0}.", "Tea as a guest. The host: {0}.", "お茶によばれた。家の人：{0}。")

# --- day phrases ({0} in the invitations and the diary) ---------------------------------------------------------------

for i, (ru, en, ja) in enumerate([("в понедельник", "on Monday", "月曜日"), ("во вторник", "on Tuesday", "火曜日"),
                                   ("в среду", "on Wednesday", "水曜日"), ("в четверг", "on Thursday", "木曜日"),
                                   ("в пятницу", "on Friday", "金曜日"), ("в субботу", "on Saturday", "土曜日"),
                                   ("в воскресенье", "on Sunday", "日曜日")]):
    T("friends.day.in.%d" % i, ru, en, ja)
T("friends.day.today", "сегодня", "today", "今日")
T("friends.day.tomorrow", "завтра", "tomorrow", "明日")

# --- the weekend outing: a close friend's letter ({0}: day phrase, {1}: time, {2}: the sender's name) ----------------

T("friends.outing.invite.sea_air",
  "Здравствуй!\n\nМы всей семьёй идём {0} к морю, просто подышать и посмотреть на волны. Встречаемся на набережной в {1}. Пойдёшь с нами? Будет хорошо.\n\n{2}",
  "Hello!\n\nThe whole family is going down to the sea {0}, just to breathe the air and watch the waves. We meet on the promenade at {1}. Will you come with us? It'll be lovely.\n\n{2}",
  "こんにちは！\n\n{0}、家族みんなで海へ行きます。潮風を吸って、波をながめるだけ。{1}に遊歩道で待ち合わせ。いっしょに行かない？きっと気持ちいいよ。\n\n{2}")
T("friends.outing.invite.stroll",
  "Привет!\n\n{0} у нас семейная прогулка по террасам, медленная, с остановками на каждой скамейке. Выходим в {1}. Присоединяйся, без тебя уже как-то не то.\n\n{2}",
  "Hi!\n\n{0} we're having a family walk along the terraces: a slow one, stopping at every bench. We set off at {1}. Come along: it isn't quite the same without you now.\n\n{2}",
  "やあ！\n\n{0}は家族で段丘をお散歩。ベンチごとにひと休みする、のんびりしたやつ。出発は{1}。いっしょにおいでよ。君がいないと、もうなんだか物足りないんだ。\n\n{2}")
T("friends.outing.invite.fair_visit",
  "Здравствуй!\n\nМы всей семьёй собираемся на ярмарку {0}: карусель, сладкая вата, всё как положено. Ждём тебя у входа в {1}. Приходи, на карусели место найдётся.\n\n{2}",
  "Hello!\n\nThe whole family is off to the fair {0}: the carousel, candy floss, everything as it should be. We'll wait for you at the entrance at {1}. Do come: there's room on the carousel.\n\n{2}",
  "こんにちは！\n\n{0}、家族みんなで市に行きます。メリーゴーラウンドに綿あめ、お決まりのコース。{1}に入口で待ってるね。おいでよ、メリーゴーラウンドの席はちゃんとあるから。\n\n{2}")
T("friends.outing.invite.feed_birds",
  "Привет!\n\n{0} мы всей семьёй идём кормить птиц. Хлеба набрали столько, что хватит на всех чаек побережья. Встречаемся в {1}. Кто лучше тебя расскажет, кто к нам прилетел?\n\n{2}",
  "Hi!\n\n{0} the whole family is going to feed the birds. We've got enough bread for every gull on the coast. We meet at {1}. Who better than you to tell us who's come to visit?\n\n{2}",
  "やあ！\n\n{0}、家族みんなで鳥にえさをやりに行きます。海岸じゅうのカモメに足りるくらいパンを持ったよ。待ち合わせは{1}。どんな鳥が来たか教えてくれるのに、君ほどの人はいないからね。\n\n{2}")
T("friends.outing.invite.eat_out",
  "Здравствуй!\n\n{0} мы всей семьёй обедаем в ресторане, у нас это маленький праздник. Стол заказан на {1}, и за ним есть место для тебя. Приходи, будем ждать.\n\n{2}",
  "Hello!\n\n{0} the whole family is having lunch at the restaurant: it's a small holiday for us. The table is booked for {1}, and there's a place at it for you. Do come, we'll be waiting.\n\n{2}",
  "こんにちは！\n\n{0}は家族みんなでレストランでお昼ごはん。うちではちょっとしたお祝いなんだ。{1}に予約してあって、君の席もあるよ。来てね、待ってるから。\n\n{2}")
T("friends.outing.invite.coffee",
  "Привет!\n\n{0} мы всей семьёй идём в кафе: взрослым кофе, детям какао, всем пирожные. Встречаемся в {1}. Посиди с нами, расскажешь, что нового у птиц.\n\n{2}",
  "Hi!\n\n{0} the whole family is going to the café: coffee for the grown-ups, cocoa for the children, cakes for everyone. We meet at {1}. Sit with us and tell us what's new with the birds.\n\n{2}",
  "やあ！\n\n{0}、家族みんなでカフェに行きます。大人はコーヒー、子どもはココア、みんなにケーキ。{1}に待ち合わせ。いっしょに座って、鳥たちの近況を聞かせてよ。\n\n{2}")
T("friends.outing.diary", "Выходные с семьёй друга. Кто позвал: {0}.", "A weekend with a friend's family. Who asked me: {0}.",
  "友だちの家族と週末をすごした。誘ってくれた人：{0}。")
T("friends.outing.toast", "{0} машет тебе: вся семья в сборе.", "{0} waves to you: the whole family's here.", "{0}が手をふっている。家族がそろった。")

# --- the diary page "People" (pencil notes about each friend) --------------------------------------------------------

T("friends.diary.title", "Люди", "People", "人びと")
T("friends.diary.empty", "Пока здесь пусто. Надо чаще здороваться: в этом городе все рано или поздно становятся знакомыми.",
  "Empty so far. I should say hello more often: in this town everyone becomes an acquaintance sooner or later.",
  "まだ何もない。もっとあいさつしよう。この町では、遅かれ早かれ誰とでも知り合いになる。")
T("friends.level.1", "знакомы", "acquaintances", "顔見知り")
T("friends.level.2", "приятели", "pals", "仲よし")
T("friends.level.3", "друзья", "friends", "友だち")
T("friends.level.4", "близкие друзья", "close friends", "親友")
T("friends.fact.home", "живёт: {0}", "lives: {0}", "住まい：{0}")
T("friends.fact.job", "{0}", "{0}", "{0}")
T("friends.fact.nick", "зовёт меня «{0}»", "calls me \"{0}\"", "僕の呼び名：「{0}」")
T("friends.fact.likes", "любит: {0}", "likes: {0}", "好きなもの：{0}")
T("friends.fact.cards", "открыток от меня: {0}", "postcards from me: {0}", "僕が出したはがき：{0}枚")
T("friends.fact.teas", "чаепитий: {0}", "teas together: {0}", "お茶によばれた回数：{0}")
T("friends.fact.outing", "по выходным с семьёй: {0}", "weekends with the family: {0}", "週末は家族と：{0}")
T("friends.fact.met", "знакомство: {0}", "met: {0}", "知り合った日：{0}")
T("friends.fact.family", "семья: {0}", "family: {0}", "家族：{0}")

# what they do, as a pencil note (gender-neutral in ru)
for job, ru, en, ja in [
        ("away", "работает в отъезде", "works away", "よそへ働きに出ている"),
        ("pensioner", "на пенсии", "retired", "年金暮らし"),
        ("pupil", "учится в школе", "at school", "学校に通っている"),
        ("fisherman", "рыбачит", "fishes for a living", "漁師"),
        ("writer", "пишет книги", "writes books", "本を書いている"),
        ("seller", "торгует", "sells things", "物売り"),
        ("student", "учится в институте", "a student", "大学生"),
        ("programmer", "пишет программы", "writes software", "プログラマー"),
        ("unemployed", "ищет работу", "looking for work", "仕事を探している"),
        ("homemaker", "ведёт дом", "keeps house", "家のことをしている"),
        ("musician", "играет музыку", "plays music", "音楽をしている"),
        ("artist", "рисует", "paints", "絵を描いている"),
        ("vendor", "торгует с лотка", "runs a stall", "屋台をやっている"),
        ("cook", "готовит в ресторане", "cooks at the restaurant", "料理人"),
        ("clerk", "служит в конторе", "works in an office", "事務の仕事"),
        ("barista", "варит кофе", "makes coffee", "バリスタ"),
        ("fishmonger", "торгует рыбой", "sells fish", "魚屋さん"),
        ("teacher", "учит детей", "teaches children", "先生"),
        ("waiter", "работает в зале ресторана", "waits at tables", "給仕の仕事"),
        ("renter", "сдаёт жильё", "lets rooms", "部屋を貸している"),
        ("lifeguard", "спасатель на пляже", "a lifeguard on the beach", "浜の監視員"),
        ("bartender", "работает за стойкой", "works behind the bar", "バーテンダー"),
        ("cashier", "работает на кассе", "works at a till", "レジ係"),
        ("barber", "стрижёт", "cuts hair", "床屋さん"),
        ("doctor", "лечит людей", "a doctor", "お医者さん"),
        ("cleaner", "наводит чистоту", "does the cleaning", "清掃の仕事"),
        ("coach", "тренирует", "coaches sport", "コーチ"),
        ("receptionist", "встречает гостей у стойки", "works at a front desk", "受付係"),
        ("nurse", "медсестра или медбрат", "a nurse", "看護師"),
        ("baker", "печёт хлеб", "bakes bread", "パン屋さん"),
        ("repairman", "чинит всё подряд", "mends things", "修理屋さん"),
        ("photographer", "фотографирует", "takes photographs", "写真家"),
        ("janitor", "смотрит за двором", "looks after the yard", "用務員"),
        ("manager", "руководит", "runs things", "支配人"),
        ("guard", "сторожит", "keeps guard", "警備の仕事"),
        ("bookseller", "продаёт книги", "sells books", "本屋さん"),
        ("projectionist", "крутит кино", "runs the film projector", "映写技師"),
        ("librarian", "работает в библиотеке", "works at the library", "図書館の人"),
        ("lighthouse_keeper", "смотрит за маяком", "keeps the lighthouse", "灯台守"),
        ("dance_teacher", "учит танцевать", "teaches dancing", "ダンスの先生"),
        ("carousel_operator", "катает на карусели", "runs the carousel", "メリーゴーラウンドの係"),
        ("caretaker", "смотрит за домом", "takes care of the building", "建物の管理人"),
        ("butcher", "торгует мясом", "sells meat", "肉屋さん"),
        ("choir_leader", "руководит хором", "leads a choir", "合唱の指揮者"),
        ("director", "директорствует", "a director", "所長さん"),
        ("guest", "отдыхает в санатории", "is staying at the sanatorium", "サナトリウムで療養中"),
]:
    T("friends.job.%s" % job, ru, en, ja)

# --- the interface ---------------------------------------------------------------------------------------------------

T("friends.gift.prompt", "Подарок для: {0}", "A present for: {0}", "{0}へのプレゼント")
T("friends.gift.title", "Что подарить?", "What to give?", "何をあげよう？")
T("friends.gift.keys", "1–9 — подарить   Esc — передумать", "1–9 — give   Esc — change my mind", "1〜9：あげる　Esc：やめる")
T("friends.gift.none", "В карманах нет ничего, что можно подарить.", "Nothing in my pockets to give.", "ポケットに、あげられるものが何もない。")
T("friends.gift.diary", "Подарок. Кому: {0}. Что: {1}.", "A present. To: {0}. What: {1}.", "プレゼント。相手：{0}。品物：{1}。")
T("friends.bench.toast", "{0} садится рядом.", "{0} sits down beside you.", "{0}がとなりに座った。")
T("friends.know.toast", "{0} теперь зовёт тебя «{1}».", "{0} calls you \"{1}\" now.", "{0}はこれから君を「{1}」と呼ぶ。")

# developer mode (docs/dev_mode.md): the group and the actions
T("dev.group.friends", "Друзья", "Friends", "友だち")
T("dev.action.friends.befriend", "Друзья: ближайший — близкий друг", "Friends: the nearest becomes a close friend", "友だち：いちばん近い人を親友に")
T("dev.action.friends.tea_now", "Друзья: чай сейчас (Тадессе)", "Friends: tea now (Tadesse)", "友だち：今すぐお茶（タデッセ）")
T("dev.action.friends.invite_all", "Друзья: все приглашения", "Friends: all the invitations", "友だち：招待状を全部")
T("dev.action.friends.report", "Друзья: отчёт в лог", "Friends: report to the log", "友だち：ログに報告")
T("dev.tp.friends_tea_tadesse", "Дом Тадессе: стол", "The Tadesse home: the table", "タデッセ家：テーブル")
