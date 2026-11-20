"""The words of the town's post (docs/letters.md), ru / en / ja. Russian is the source.

Every entry is T(key, ru, en, ja). tools/letters/build_letters.py writes them to
Content/Localization/letters/strings_<lang>.json and the structure to Content/Letters/letters.json.
Edit here, then rerun the builder; never edit the built files.

House style (localization skill, memory "localization-conventions"): the hero is a teenage boy (ru past tense masculine,
ja 僕); the townsfolk keep their own voice; names inside a sentence go after a colon or in quotes, never into a Russian
case ending; ja uses full-width ！？：（）「」.
"""

STRINGS = {}


def T(key, ru, en, ja):
    assert key not in STRINGS, key
    STRINGS[key] = {"ru": ru, "en": en, "ja": ja}
    return key


# --- the interface -------------------------------------------------------------------------------------------------

T("letters.postbox.prompt", "E — почтовый ящик", "E — postbox", "E：郵便ポスト")
T("letters.mailbox.prompt", "E — почтовый ящик клуба", "E — the club's mailbox", "E：クラブの郵便受け")
T("letters.mailbox.prompt_new", "E — забрать письма ({0})", "E — take the letters ({0})", "E：手紙を取る（{0}）")
T("letters.postbox.title", "Почтовый ящик", "Postbox", "郵便ポスト")
T("letters.mailbox.title", "Почтовый ящик клуба «Атон»", "The Aton club's mailbox", "クラブ「アトン」の郵便受け")
T("letters.postbox.collect", "Выемка писем в 17:00, кроме воскресенья.", "Collection at 17:00, except Sundays.",
  "取集は17時。日曜日を除く。")
T("letters.menu.card", "Написать открытку (в кармане: {0})", "Write a postcard ({0} in the pocket)", "はがきを書く（手持ち：{0}）")
T("letters.menu.pen", "Ответить Тоне на Белый Мыс", "Answer Tonya at White Cape", "白岬のトーニャに返事を書く")
T("letters.menu.reread", "Перечитать письма ({0})", "Read the letters again ({0})", "手紙を読み返す（{0}）")
T("letters.menu.leave", "Отойти", "Step away", "離れる")
T("letters.refuse.no_cards", "Открыток нет. Их можно взять в киоске у остановки.",
  "No postcards. The kiosks at the stops have them.", "はがきがない。停留所の売店でもらえる。")
T("letters.refuse.nobody", "Пока некому писать: со мной в городе ещё никто не знаком.",
  "Nobody to write to yet: nobody in town knows me.", "まだ書く相手がいない。町で僕を知っている人がいない。")
T("letters.refuse.empty", "В ящике пусто.", "The mailbox is empty.", "郵便受けは空だ。")
T("letters.step.to", "Кому?", "To whom?", "誰に？")
T("letters.step.picture", "Какую открытку?", "Which postcard?", "どのはがき？")
T("letters.step.opening", "С чего начать?", "How to begin?", "書き出しは？")
T("letters.step.wish", "Чем закончить?", "How to end?", "結びは？")
T("letters.step.preview", "Открытка", "The postcard", "はがき")
T("letters.step.answer", "Что рассказать Тоне?", "What to tell Tonya?", "トーニャに何を話そう？")
T("letters.step.answer_preview", "Письмо на Белый Мыс", "A letter to White Cape", "白岬への手紙")
T("letters.step.reread", "Письма", "Letters", "手紙")
T("letters.keys.choose", "1–9, A/D — выбрать, E — дальше, Esc — назад", "1–9, A/D — choose, E — next, Esc — back",
  "1〜9、A/D：選ぶ　E：次へ　Esc：戻る")
T("letters.keys.post", "E — опустить в ящик, Esc — назад", "E — drop it in the box, Esc — back", "E：ポストに入れる　Esc：戻る")
T("letters.keys.next", "E — дальше", "E — next", "E：次へ")
T("letters.keys.close", "E — закрыть", "E — close", "E：閉じる")
T("letters.more", "… ещё {0}", "… {0} more", "…ほか{0}")
T("letters.card.preview", "Открытка «{0}».\n\n{1} {2}\n\nКому: {3}\n{4}",
  "Postcard: \"{0}\".\n\n{1} {2}\n\nTo: {3}\n{4}", "はがき「{0}」。\n\n{1}{2}\n\n宛先：{3}\n{4}")
T("letters.answer.preview", "Дорогая Тоня!\n\n{0}\n\nПиши, я жду.", "Dear Tonya,\n\n{0}\n\nWrite back, I'm waiting.",
  "トーニャへ\n\n{0}\n\n返事を待ってるよ。")
T("letters.posted", "Открытка в ящике. Её отнесут {0}.", "The postcard is in the box. It goes out {0}.", "はがきをポストに入れた。配達は{0}。")
T("letters.posted_pen", "Письмо в ящике. До Белого Мыса оно идёт несколько дней.",
  "The letter is in the box. It takes a few days to reach White Cape.", "手紙をポストに入れた。白岬までは何日かかかる。")
for i, (ru, en, ja) in enumerate([("в понедельник", "on Monday", "月曜日"), ("во вторник", "on Tuesday", "火曜日"),
                                   ("в среду", "on Wednesday", "水曜日"), ("в четверг", "on Thursday", "木曜日"),
                                   ("в пятницу", "on Friday", "金曜日"), ("в субботу", "on Saturday", "土曜日"),
                                   ("в воскресенье", "on Sunday", "日曜日")]):
    T("letters.when.%d" % i, ru, en, ja)
T("letters.when.tomorrow", "завтра", "tomorrow", "明日")
T("letters.mail.new", "В почтовом ящике клуба новое письмо.", "A new letter in the club's mailbox.", "クラブの郵便受けに新しい手紙が届いた。")
T("letters.mail.new_n", "В почтовом ящике клуба новые письма: {0}.", "New letters in the club's mailbox: {0}.",
  "クラブの郵便受けに新しい手紙：{0}通。")
T("letters.letter.from", "От: {0}", "From: {0}", "{0}より")
T("letters.letter.from_place", "От: {0} ({1})", "From: {0} ({1})", "{0}より（{1}）")
T("letters.addr.flat", "{0}, д. {1}, кв. {2}", "{1} {0}, flat {2}", "{0}{1}番　{2}号室")
T("letters.addr.house", "{0}, д. {1}", "{1} {0}", "{0}{1}番")
T("letters.addr.academ", "Академгородок", "Academgorodok", "アカデムゴロドク")
T("letters.addr.academ_flat", "Академгородок, кв. {0}", "Academgorodok, flat {0}", "アカデムゴロドク　{0}号室")
for tid, ru, en, ja in [("T0", "Набережная", "Embankment", "海岸通り"), ("T1", "Нижний город", "Lower Town", "下町"),
                        ("T2", "Вторая терраса", "Second Terrace", "第二段丘"), ("T3", "Третья терраса", "Third Terrace", "第三段丘"),
                        ("T4", "Четвёртая терраса", "Fourth Terrace", "第四段丘"), ("T5", "Верхняя терраса", "Upper Terrace", "上段丘"),
                        ("B", "Пляж", "Beach", "浜辺"),
                        ("W1", "Набережная, у стены", "Embankment Wall", "海岸通り・壁の家"),
                        ("W2", "Нижний город, у стены", "Lower Town Wall", "下町・壁の家"),
                        ("W3", "Вторая терраса, у стены", "Second Terrace Wall", "第二段丘・壁の家"),
                        ("W4", "Третья терраса, у стены", "Third Terrace Wall", "第三段丘・壁の家"),
                        ("W5", "Четвёртая терраса, у стены", "Fourth Terrace Wall", "第四段丘・壁の家")]:
    T("letters.street.%s" % tid, ru, en, ja)
T("letters.diary.card", "Отправил открытку «{0}». Кому: {1}. «{2} {3}»", "Sent the postcard \"{0}\". To: {1}. \"{2} {3}\"",
  "はがき「{0}」を出した。宛先：{1}。「{2}{3}」")
T("letters.diary.answer", "Написал Тоне на Белый Мыс: «{0}»", "Wrote to Tonya at White Cape: \"{0}\"", "白岬のトーニャに書いた。「{0}」")
T("letters.diary.letter", "Письмо. От: {0}.", "A letter. From: {0}.", "手紙。{0}より。")
T("letters.know", "Теперь я знаю адрес: {0}", "Now I know the address: {0}", "住所がわかった：{0}")
T("letters.hint.answer", "Ответить Тоне можно у любого почтового ящика", "I can answer Tonya at any postbox", "トーニャへの返事はどのポストからでも出せる")
T("letters.from.council", "Горсовет Людмиловска", "Lyudmilovsk Town Council", "リュドミロフスク市役所")
T("letters.pen.who", "Тоня Лоцманова", "Tonya Lotsmanova", "トーニャ・ロツマノワ")
T("letters.pen.place", "Белый Мыс", "White Cape", "白岬")

# developer mode (docs/dev_mode.md): the group and the actions
T("dev.group.letters", "Почта", "Post", "郵便")
T("dev.action.letters.deliver", "Почта: доставить всё сейчас", "Post: deliver everything now", "郵便：今すぐ全部配達")
T("dev.action.letters.pen_next", "Почта: следующее письмо Тони", "Post: Tonya's next letter", "郵便：トーニャの次の手紙")
T("dev.action.letters.cards", "Почта: 5 открыток в карман", "Post: 5 postcards to the pocket", "郵便：はがき5枚を手持ちに")
T("dev.action.letters.know_all", "Почта: знаком со всеми", "Post: know everybody", "郵便：全員と知り合い")
T("dev.tp.letters_club_mailbox", "Почтовый ящик клуба", "The club's mailbox", "クラブの郵便受け")
T("dev.tp.letters_postbox_kiosk1", "Почтовый ящик у киоска 1", "Postbox by kiosk 1", "売店1のポスト")

# --- what goes on a postcard ----------------------------------------------------------------------------------------

PICTURES = [
    ("lighthouse", "Маяк на пирсе", "The lighthouse on the pier", "桟橋の灯台"),
    ("cablecar", "Канатная дорога над обрывом", "The cable car over the cliff", "崖の上のロープウェイ"),
    ("fountain", "Солнечный фонтан", "The Sun Fountain", "太陽の噴水"),
    ("promenade", "Набережная на закате", "The promenade at sunset", "夕暮れの海岸通り"),
    ("starlings", "Скворцы над морем", "Starlings over the sea", "海の上のムクドリ"),
    ("terraces", "Террасы Людмиловска", "The terraces of Lyudmilovsk", "リュドミロフスクの段丘"),
    ("sanatorium", "Санаторий «Корона»", "The Korona sanatorium", "サナトリウム「コロナ」"),
    ("aton", "Клуб «Атон» на утёсе", "The Aton club on the cliff", "崖の上のクラブ「アトン」"),
]
for pid, ru, en, ja in PICTURES:
    T("letters.pic.%s" % pid, ru, en, ja)

OPENINGS = [
    ("hello", "Привет из Людмиловска!", "Greetings from Lyudmilovsk!", "リュドミロフスクから、こんにちは！"),
    ("morning", "Спасибо, что здороваетесь со мной по утрам.", "Thank you for saying hello to me in the mornings.",
     "毎朝あいさつしてくれて、ありがとうございます。"),
    ("sea", "Море сегодня было совсем зелёное, как бутылочное стекло.", "The sea was quite green today, like bottle glass.",
     "今日の海は、びんのガラスみたいに緑色でした。"),
    ("starlings", "Сегодня над пирсом летали скворцы, тысячи, будто дым.", "Starlings flew over the pier today, thousands, like smoke.",
     "今日、桟橋の上をムクドリが飛んでいました。何千羽も、煙みたいに。"),
    ("sunset", "Вчера закат был такой, что я пропустил ужин.", "Yesterday's sunset was so good I missed my supper.",
     "昨日の夕焼けがすごくて、夕ごはんを食べそこねました。"),
    ("thinking", "Просто вспомнил о вас и решил написать.", "I just thought of you and decided to write.",
     "ふと思い出して、書きたくなりました。"),
]
WISHES = [
    ("weather", "Пусть у вас всегда будет ясная погода.", "May you always have clear weather.", "いつも晴れますように。"),
    ("day", "Хорошего вам дня!", "Have a good day!", "よい一日を！"),
    ("see", "До встречи на набережной!", "See you on the promenade!", "海岸通りで会いましょう！"),
    ("care", "Берегите себя.", "Take care of yourself.", "お体に気をつけて。"),
    ("sun", "Пусть солнце светит вам в окно.", "May the sun shine in at your window.", "窓にお日さまが差しますように。"),
    ("tea", "Тёплого вам чая и долгих вечеров.", "Warm tea to you, and long evenings.", "あたたかいお茶と、長い夜を。"),
]
for pid, ru, en, ja in OPENINGS:
    T("letters.phrase.%s" % pid, ru, en, ja)
for pid, ru, en, ja in WISHES:
    T("letters.phrase.%s" % pid, ru, en, ja)

# --- the townsfolk answer a postcard ({0}: the postcard's picture) ---------------------------------------------------

REPLIES = {
    "kid": [
        T("letters.reply.kid.1",
          "Привет! Мне никто ещё не присылал открыток, а ты прислал. «{0}» висит у меня над кроватью. Я тебе нарисую ответ, когда научусь рисовать скворцов.",
          "Hi! Nobody ever sent me a postcard before, and you did. \"{0}\" is up over my bed. I'll draw you an answer when I can draw starlings.",
          "こんにちは！はがきをもらったの、はじめて。「{0}」はベッドの上にはってあるよ。ムクドリがかけるようになったら、絵でへんじをかくね。"),
        T("letters.reply.kid.2",
          "Мама сказала, что открытки пишут взрослые, а я сказала, что мне написали. Спасибо за «{0}»! Я её показывала всему классу.",
          "Mum said grown-ups write postcards, and I said one was written to me. Thank you for \"{0}\"! I showed it to the whole class.",
          "ママは「はがきは大人が書くもの」って言ったけど、わたしにも来たって言ったの。「{0}」をありがとう！クラスのみんなに見せたよ。"),
        T("letters.reply.kid.3",
          "Спасибо за открытку! Я знаю это место: «{0}». Там один раз чайка украла у меня булку. Пиши ещё.",
          "Thanks for the postcard! I know that place: \"{0}\". A gull stole my bun there once. Write again.",
          "はがきありがとう！その場所知ってる：「{0}」。あそこで一回カモメにパンを取られた。またかいてね。"),
    ],
    "teen": [
        T("letters.reply.teen.1",
          "Ну ты даёшь, настоящая открытка, с маркой и всем. «{0}» — классная. Я тоже напишу кому-нибудь, а то стыдно.",
          "Wow, a real postcard, stamp and all. \"{0}\" is cool. I'll write to somebody too, or I'll feel bad.",
          "まじか、本物のはがきだ。切手まで貼ってある。「{0}」、いいね。誰かに書こうかな、じゃないと悪い気がする。"),
        T("letters.reply.teen.2",
          "Открытка пришла. Сначала кажется: реклама, а читаешь — нет, мне. Спасибо. «{0}» теперь стоит за стеклом в шкафу.",
          "Got the postcard. Looks like an advert at first, then you read it. Thanks. \"{0}\" is behind the glass in my cupboard now.",
          "はがき届いた。最初は広告かと思ったけど、読んだ。ありがとう。「{0}」は棚のガラスの中に飾った。"),
        T("letters.reply.teen.3",
          "Привет. Открытка «{0}» дошла за один день, а мне казалось, почта ходит только к бабушкам. Встретимся у фонтана — расскажешь, откуда ты такой взялся.",
          "Hi. The postcard \"{0}\" came in a day, and I thought the post only went to grannies. See you by the fountain: tell me where you came from.",
          "やあ。「{0}」のはがき、一日で届いたよ。郵便っておばあちゃんにしか来ないと思ってた。噴水で会ったら、どこから来たのか教えて。"),
    ],
    "adult": [
        T("letters.reply.adult.1",
          "Спасибо за открытку! «{0}» теперь стоит у меня на холодильнике, рядом с квитанциями, и квитанции от этого стали не такими страшными.",
          "Thank you for the postcard! \"{0}\" is on my fridge now, next to the bills, and the bills look less frightening for it.",
          "はがきをありがとう！「{0}」は冷蔵庫の請求書の隣に貼りました。おかげで請求書が少しこわくなくなりました。"),
        T("letters.reply.adult.2",
          "Дорогой сосед! Открыток мне не присылали, кажется, с институтских времён. «{0}» — прекрасный выбор. Заходите в гости, если будете на нашей террасе.",
          "Dear neighbour! I don't think I've had a postcard since my student days. \"{0}\" is a lovely choice. Do drop in if you're on our terrace.",
          "ご近所さんへ。はがきをもらうのは学生のころ以来かもしれません。「{0}」、すてきな選び方です。うちの段丘に来たら寄ってくださいね。"),
        T("letters.reply.adult.3",
          "Ваша открытка «{0}» пришла, и полдня у меня было отличное настроение. На работе спросили, что случилось. Ответ простой: почта. Спасибо!",
          "Got your postcard \"{0}\" and went about pleased half the day. At work they asked what had happened. I said: the post. Thank you!",
          "「{0}」のはがきを受け取って、半日ずっとにこにこしていました。職場で何かあったのかと聞かれたので「郵便です」と答えました。ありがとう！"),
    ],
    "elder": [
        T("letters.reply.elder.1",
          "Здравствуй, внучек. Спасибо тебе за открытку «{0}». Она теперь стоит в серванте, у фотографий. Пусть стоит, она красивая.",
          "Hello, dear boy. Thank you for the postcard \"{0}\". I've put it in the glass cabinet with the photographs. Let it stand there: it's pretty.",
          "こんにちは、坊や。「{0}」のはがき、ありがとうね。写真と一緒に飾り棚に入れたよ。きれいだから、そこに置いておくよ。"),
        T("letters.reply.elder.2",
          "Молодой человек! Открытка «{0}» доставлена в полной исправности. В наше время писали чаще, и правильно делали. Жму руку.",
          "Young man! The postcard \"{0}\" arrived in perfect order. In our day people wrote more often, and they were right to. I shake your hand.",
          "若いの！「{0}」のはがき、無事に届いた。昔はもっとよく手紙を書いたものだ。それが正しかった。握手を送る。"),
        T("letters.reply.elder.3",
          "Спасибо, милый. Твою открытку «{0}» соседка слушала вслух два раза. Она тоже хочет. Приходи на нашу скамейку, у меня для тебя конфета.",
          "Thank you, dear. My neighbour has heard your postcard \"{0}\" read aloud twice. She wants one too. Come to our bench: I've a sweet for you.",
          "ありがとうね。「{0}」のはがき、お隣さんにも二回読んであげたよ。あの人も欲しがってる。うちのベンチにおいで、あめをあげるから。"),
    ],
}

# a townsperson who got a card says thank you in the street (shown over their head: the key without "line.")
THANKS_LINES = [
    T("line.letters.thanks.1", "Это ты мне открытку прислал? Спасибо!", "Was it you sent me that postcard? Thank you!",
      "はがきをくれたの、君？ありがとう！"),
    T("line.letters.thanks.2", "Открытку получили, стоит на самом видном месте.", "Got the postcard: it's standing in pride of place.",
      "はがき、届いたよ。いちばん目立つところに飾ってある。"),
    T("line.letters.thanks.3", "А, почтальон-любитель! Спасибо за открытку.", "Ah, the amateur postman! Thanks for the card.",
      "おや、郵便屋さん見習い！はがきをありがとう。"),
    T("line.letters.thanks.4", "Спасибо за весточку. Давно мне никто не писал.", "Thanks for the word. Nobody's written to me for ages.",
      "便りをありがとう。ずいぶん久しぶりに手紙をもらった。"),
    T("line.letters.thanks.5", "Твоя открытка дошла! Я теперь тоже хочу кому-нибудь написать.",
      "Your postcard came! Now I want to write to somebody too.", "君のはがき、届いたよ！今度は僕も誰かに書きたくなった。"),
    T("line.letters.thanks.6", "Спасибо тебе за открытку, золотой.", "Thank you for the postcard, my dear.", "はがきをありがとうね、いい子だね。"),
]

# --- the people of the open homes write back (their homes: tools/city/content/stories.json) ---------------------------
# id, the home plot, the thing their cards stand by (interiors.json), a townsperson who is them ("" none), the name,
# two replies (one per postcard, {0} the picture), the invitation, the line on seeing the card at theirs.

CORRESPONDENTS = [
    dict(id="h_holm", home="P-T1H2", shelf="P-T1H2/flat_books_sea", person="",
         name=T("letters.who.holm", "Виктор Сергеевич Холм", "Viktor Sergeyevich Holm", "ヴィクトル・セルゲーエヴィチ・ホルム"),
         replies=[
             T("letters.home.holm.1",
               "Молодой человек!\n\nВаша открытка «{0}» принята на борт и поставлена между лоциями, где ей и место. Консьержка говорит, это вы записали мне прогноз в журнал. Почерк у вас, извините, как у юнги, но ветер записан верно.\n\nВ. С. Холм, штурман дальнего плавания",
               "Young man,\n\nYour postcard \"{0}\" has been taken aboard and stood between the pilot books, where it belongs. The concierge says it was you who wrote the forecast in my log. Your handwriting, forgive me, is a ship's boy's, but the wind is written down right.\n\nV. S. Holm, master mariner",
               "若者へ\n\n君のはがき「{0}」は乗船を許され、水路誌のあいだに立ててある。そこが似合いの場所だ。管理人の話では、航海日誌に天気予報を書いてくれたのは君だそうだね。字は見習い水夫だが、風向きは正しく書けている。\n\n遠洋航海士　V・S・ホルム"),
             T("letters.home.holm.2",
               "Юнга!\n\nВторая открытка, «{0}», — это уже переписка, а переписку я уважаю. Мой старый друг из Мурманска сорок лет играет со мной в шахматы открытками. Предлагаю и вам партию по почте. Мой ход: е2–е4. Можете не отвечать ходом, отвечайте погодой.\n\nВ. С. Х.",
               "Ship's boy,\n\nA second postcard, \"{0}\", makes a correspondence, and I respect a correspondence. An old friend of mine in Murmansk has played chess with me by postcard for forty years. I offer you a game by post too. My move: e2–e4. You needn't answer with a move: answer with the weather.\n\nV. S. H.",
               "見習い水夫くん\n\n二枚目のはがき「{0}」。これでもう文通だ。文通というものを私は尊重する。ムルマンスクの古い友人とは、四十年はがきでチェスを指している。君にも郵便での一局を申し込もう。私の手：e2–e4。手で答えなくていい、天気で答えなさい。\n\nV・S・H"),
         ],
         invite=T("letters.home.holm.invite",
                  "Юнга!\n\nПриглашаю на флотский чай. Дверь у меня, как известно, не заперта: на судне каюты не запирали. Приходите вечером, покажу, как пишется настоящий вахтенный журнал, и расскажу про Бискай в семьдесят четвёртом. Сахар не обещаю, обещаю сухари.\n\nВ. С. Холм",
                  "Ship's boy,\n\nI invite you to navy tea. My door, as you know, isn't locked: on board ship we never locked the cabins. Come in the evening: I'll show you how a real watch log is kept and tell you about Biscay in seventy-four. I don't promise sugar; I promise ship's biscuit.\n\nV. S. Holm",
                  "見習い水夫くん\n\n海軍式のお茶に招待しよう。ご存じのとおり、うちのドアに鍵はかかっていない。船では船室に鍵をかけなかったからね。夕方に来なさい。本物の航海日誌の書き方を見せて、七十四年のビスケー湾の話をしよう。砂糖は約束しないが、乾パンは約束する。\n\nV・S・ホルム"),
         shelf_note=T("letters.shelf.holm", "Между лоциями стоит моя открытка. Рядом с открыткой от «К.» из Мурманска.",
                      "My postcard stands between the pilot books. Next to the one from \"K.\" in Murmansk.",
                      "水路誌のあいだに僕のはがきが立っている。ムルマンスクの「K」からのはがきの隣に。")),
    dict(id="h_okada", home="P-T4H1", shelf="P-T4H1/flat_books_art", person="",
         name=T("letters.who.okada", "Лев Маркович Окада", "Lev Markovich Okada", "レフ・マルコヴィチ・オカダ"),
         replies=[
             T("letters.home.okada.1",
               "Дорогой мой зритель!\n\nОткрытка «{0}» пришла с утренней почтой, и я сразу поставил её к альбомам. Хороший художник её рисовал: свет не врёт. Асанов внизу говорит, вы смотрели мои окна долго и по-настоящему. Тысяча первое я напишу с вашей открыткой на подоконнике.\n\nЛ. М. Окада",
               "My dear viewer,\n\nThe postcard \"{0}\" came with the morning post, and I stood it by my albums at once. A good artist drew it: the light doesn't lie. Asanov downstairs says you looked at my windows long and truly. I'll paint the thousand-and-first with your card on the sill.\n\nL. M. Okada",
               "親愛なる観客へ\n\n「{0}」のはがきが朝の郵便で届いたので、すぐ画集の横に立てました。いい画家が描いたものです。光がうそをついていない。下のアサノフさんの話では、君は私の窓を長く、本気で見てくれたそうですね。千一枚目の窓は、君のはがきを窓辺に置いて描きます。\n\nL・M・オカダ"),
             T("letters.home.okada.2",
               "Мой друг!\n\nВторая открытка, «{0}». Я её прочитал Асе вслух. Ася — это моя жена, её давно нет, но я ей пишу каждый вечер и, как видите, читаю ей почту. Ей понравилось. Она всегда говорила, что открытки пишут люди, у которых ещё не кончилось лето.\n\nЛ. М.",
               "My friend,\n\nA second postcard, \"{0}\". I read it aloud to Asya. Asya is my wife; she's long gone, but I write to her every evening and, as you see, read her the post. She liked it. She always said postcards are written by people whose summer hasn't ended yet.\n\nL. M.",
               "友へ\n\n二枚目のはがき「{0}」。アーシャに読んで聞かせました。アーシャは妻です。もうずいぶん前にいなくなりましたが、毎晩手紙を書いていて、こうして郵便も読んであげるのです。気に入っていましたよ。はがきを書くのは、まだ夏が終わっていない人だと、彼女はいつも言っていました。\n\nL・M"),
         ],
         invite=T("letters.home.okada.invite",
                  "Мой друг!\n\nЗаходите, когда будет закат. Я поставлю второй мольберт и дам вам кисть: попробуйте написать наше окно хотя бы раз. Это не трудно. Трудно остановиться на первом. Асанов нальёт чаю в подстаканнике.\n\nЛ. М. Окада",
                  "My friend,\n\nCome by at sunset. I'll set up a second easel and give you a brush: try painting our window just once. It isn't hard. What's hard is stopping at the first. Asanov will pour tea in a glass holder.\n\nL. M. Okada",
                  "友へ\n\n夕焼けのころにいらっしゃい。イーゼルをもう一つ立てて、筆をお貸しします。一度でいいから、うちの窓を描いてごらんなさい。難しくはありません。難しいのは一枚目でやめることです。アサノフさんがグラスホルダーでお茶をいれてくれますよ。\n\nL・M・オカダ"),
         shelf_note=T("letters.shelf.okada", "У альбомов стоит моя открытка. Похоже, он её рисовал: на полях эскиз того же вида.",
                      "My postcard stands by the albums. It seems he's sketched it: the same view in the margin of a pad.",
                      "画集の横に僕のはがきが立っている。描いてみたらしい。スケッチ帳の余白に同じ景色がある。")),
    dict(id="h_singh", home="P-CAPSULE", shelf="P-CAPSULE/capsule_shelf", person="",
         name=T("letters.who.singh", "Ильмар Сингх", "Ilmar Singh", "イルマル・シン"),
         replies=[
             T("letters.home.singh.1",
               "Приём!\n\nОткрытка «{0}» получена, слышимость отличная. У радиолюбителей принято отвечать на связь карточкой, так что вот моя: «Капсульная башня, Людмиловск. Сигнал пять-девять. Оператор Ильмар». Повесьте её у себя. Теперь вы в моём журнале связей, под номером тысяча четыреста два.\n\nИльмар, комендант",
               "Come in!\n\nThe postcard \"{0}\" received, reception excellent. Radio hams answer a contact with a card, so here's mine: \"Capsule Tower, Lyudmilovsk. Signal five-nine. Operator Ilmar.\" Pin it up. You're in my contact log now, number one thousand four hundred and two.\n\nIlmar, the warden",
               "こちら応答！\n\nはがき「{0}」受信、感度良好。アマチュア無線家は交信のお礼にカードを送るのが決まりなので、これが私のカードです。「カプセルタワー、リュドミロフスク。信号５・９。オペレーター：イルマル」。どこかに貼っておいてください。君は私の交信ログの千四百二番になりました。\n\n管理人イルマル"),
             T("letters.home.singh.2",
               "Приём!\n\nВторая открытка, «{0}». Докладываю: в субботу в «Тарелке» был Гуннар. Двадцать лет мы говорили по радио через одну улицу и ни разу не виделись. Оказалось, он выше меня на голову и тоже пьёт чай без сахара. Это ваша работа. Конец связи, но не совсем.\n\nИльмар",
               "Come in!\n\nA second postcard, \"{0}\". I report: on Saturday Gunnar was at the Tarelka. Twenty years we talked by radio across one street and never once met. Turns out he's a head taller than me and also drinks his tea without sugar. That's your doing. Over and out, but not quite.\n\nIlmar",
               "こちら応答！\n\n二枚目のはがき「{0}」。報告します。土曜日、「タレルカ」にグンナルが来ました。通り一本をはさんで二十年無線で話していたのに、一度も会ったことがなかった。彼は私より頭ひとつ背が高く、やはり紅茶に砂糖を入れない人でした。君のおかげです。交信終了。でも、まだ終わりではありません。\n\nイルマル"),
         ],
         invite=T("letters.home.singh.invite",
                  "Приём!\n\nПриглашаю на ночное дежурство в эфире. В полночь проходимость лучше всего: бывает слышно корабли за Скагерраком и рыбаков с Лофотенов. Приходите в капсульную башню после одиннадцати. Наушники у меня есть вторые.\n\nИльмар",
                  "Come in!\n\nI invite you to a night watch on the air. Propagation is best at midnight: sometimes you can hear ships beyond the Skagerrak and fishermen off the Lofotens. Come to the Capsule Tower after eleven. I have a second pair of headphones.\n\nIlmar",
                  "こちら応答！\n\n夜の無線当番にご招待します。真夜中がいちばんよく聞こえます。スカゲラク海峡の向こうの船や、ロフォーテン諸島の漁師の声が聞こえることもある。十一時過ぎにカプセルタワーへ来てください。ヘッドホンはもう一組あります。\n\nイルマル"),
         shelf_note=T("letters.shelf.singh", "На полке со справочниками стоит моя открытка, а на ней карандашом: «QSO № 1402».",
                      "My postcard stands on the shelf of handbooks, with \"QSO No. 1402\" pencilled on it.",
                      "参考書の棚に僕のはがきが立っていて、鉛筆で「QSO 第1402号」と書いてある。")),
    dict(id="h_kravets", home="P-T5H1", shelf="P-T5H1/capsule_shelf", person="",
         name=T("letters.who.kravets", "Нора Кравец", "Nora Kravets", "ノーラ・クラヴェツ"),
         replies=[
             T("letters.home.kravets.1",
               "Привет!\n\nСпасибо за «{0}». Я поставила её на «Физику моря», и книга перестала пахнуть сыростью: открытка пахнет типографией. Задачу про полную воду я всё-таки решила. Ответ сошёлся с пирсом на шесть минут. Для первого курса — победа.\n\nНора",
               "Hi!\n\nThanks for \"{0}\". I stood it on Physics of the Sea, and the book stopped smelling of damp: the card smells of the printer's. I did solve the high-water problem in the end. My answer matched the pier to six minutes. For a first-year, that's a win.\n\nNora",
               "こんにちは！\n\n「{0}」をありがとう。『海の物理』の上に立てたら、本が湿気くさくなくなりました。はがきは印刷所のにおいがするから。満潮の問題、結局解けました。答えは桟橋と六分ずれただけ。一年生にしては勝利です。\n\nノーラ"),
             T("letters.home.kravets.2",
               "Привет ещё раз!\n\n«{0}» дошла. Мама прислала из дома посылку с сыром и шерстяными носками и спрашивает, есть ли у меня здесь друзья. Я ответила, что мне пишут открытки. Она успокоилась. Так что спасибо дважды.\n\nНора",
               "Hello again!\n\n\"{0}\" came. Mum sent a parcel from home with cheese and woollen socks and asks whether I have friends here. I answered that people write me postcards. She calmed down. So thank you twice.\n\nNora",
               "また、こんにちは！\n\n「{0}」が届きました。母が実家からチーズと毛糸の靴下の小包を送ってきて、そっちに友だちはいるのかと聞いてきました。はがきをくれる人がいるよ、と答えたら安心していました。だから、ありがとうを二回。\n\nノーラ"),
         ],
         invite=T("letters.home.kravets.invite",
                  "Привет!\n\nВ ближайшее полнолуние будет самая высокая вода месяца. Я посчитала время сама и хочу проверить на пирсе, с рейкой и фонариком. Нужен свидетель, который не будет смеяться, если я ошибусь. Приходи?\n\nНора",
                  "Hi!\n\nThe next full moon brings the highest water of the month. I've worked out the time myself and want to check it on the pier with a tide pole and a torch. I need a witness who won't laugh if I'm wrong. Will you come?\n\nNora",
                  "こんにちは！\n\n次の満月の日に、今月いちばんの満潮が来ます。時刻は自分で計算したので、桟橋で潮位ざおと懐中電灯を持って確かめたいの。まちがえても笑わない証人が必要です。来てくれる？\n\nノーラ"),
         shelf_note=T("letters.shelf.kravets", "На «Физике моря» стоит моя открытка, а под ней листок с расчётом прилива.",
                      "My postcard stands on Physics of the Sea, a tide calculation tucked under it.",
                      "『海の物理』の上に僕のはがきが立っていて、下に潮の計算の紙がはさんである。")),
    dict(id="h_okafor", home="P-T1H4", shelf="P-T1H4/studio_shelf_music", person="",
         name=T("letters.who.okafor", "Гуннар Окафор", "Gunnar Okafor", "グンナル・オカフォー"),
         replies=[
             T("letters.home.okafor.1",
               "Привет, помощник!\n\nОткрытку «{0}» поставил на полку «Для солнца», это моя лучшая полка. Ту музыку с катушки я нашёл: это был старый вальс, и играл его, оказывается, оркестр нашего же санатория. Полгода искал по всему миру то, что звучало за углом.\n\nГуннар, UA-Людмиловск-11",
               "Hello, helper!\n\nI've stood the postcard \"{0}\" on the \"For sun\" shelf, my best shelf. I found the music from the reel: an old waltz, played, it turns out, by our own sanatorium's band. Half a year I searched the whole world for what was playing round the corner.\n\nGunnar, UA-Lyudmilovsk-11",
               "やあ、助手くん！\n\nはがき「{0}」は「太陽のための」棚に立てた。いちばんいい棚だ。リールの音楽、見つけたよ。古いワルツで、演奏していたのはなんと、うちの町のサナトリウムの楽団だった。半年も世界じゅう探して、角を曲がったところで鳴っていたものだったんだ。\n\nグンナル　UA-リュドミロフスク-11"),
             T("letters.home.okafor.2",
               "Помощник!\n\nОткрытка «{0}» — уже вторая, значит, ты у меня постоянный корреспондент. С Ильмаром мы теперь ходим в «Тарелку» по субботам. Сидим за одним столом и молчим, как в эфире между сеансами. Хорошо молчим.\n\nГуннар",
               "Helper,\n\nThe postcard \"{0}\" is the second, so you're a regular correspondent of mine. Ilmar and I go to the Tarelka on Saturdays now. We sit at one table and say nothing, like on the air between sessions. A good sort of nothing.\n\nGunnar",
               "助手くん\n\n「{0}」で二枚目。これで君は常連の通信相手だ。イルマルとは土曜日に「タレルカ」へ行くようになった。同じテーブルにすわって、交信と交信のあいまみたいに黙っている。いい沈黙だよ。\n\nグンナル"),
         ],
         invite=T("letters.home.okafor.invite",
                  "Помощник!\n\nВ субботу в «Тарелке» в шесть. Ильмар, я и, если придёшь, ты. Принеси свою лучшую новость, мы с Ильмаром свои уже рассказали друг другу по радио.\n\nГуннар",
                  "Helper,\n\nSaturday at the Tarelka at six. Ilmar, me and, if you come, you. Bring your best news: Ilmar and I have already told each other ours over the radio.\n\nGunnar",
                  "助手くん\n\n土曜日の六時、「タレルカ」で。イルマルと僕と、来てくれるなら君。とっておきの話を持ってきてくれ。僕たちのは、もう無線で話しちゃったから。\n\nグンナル"),
         shelf_note=T("letters.shelf.okafor", "На полке «Для солнца» среди пластинок стоит моя открытка.",
                      "My postcard stands among the records on the \"For sun\" shelf.", "「太陽のための」棚のレコードのあいだに、僕のはがきが立っている。")),
    dict(id="h_tadesse", home="P-T1H3", shelf="P-T1H3/studio_art_shelf", person="t_tadesse_valentina",
         name=T("letters.who.tadesse", "Валентина Тадессе", "Valentina Tadesse", "ヴァレンチナ・タデッセ"),
         replies=[
             T("letters.home.tadesse.1",
               "Дорогой мальчик!\n\nСпасибо за «{0}». Хэмиш сказал, что на ней не хватает рыбы, и я дорисовала: теперь в углу плывёт маленькая камбала. Не обижайтесь, это у меня профессиональное. Открытка стоит в мастерской, рыбы в аквариуме на неё смотрят.\n\nВ. Тадессе",
               "Dear boy,\n\nThank you for \"{0}\". Hamish said it lacked a fish, so I drew one in: now a little flounder swims in the corner. Don't be offended: it's professional with me. The card stands in the studio, and the fish in the tank look at it.\n\nV. Tadesse",
               "やさしい坊やへ\n\n「{0}」をありがとう。ハミッシュが「魚が足りない」と言うので描き足しました。今は隅で小さなカレイが泳いでいます。気を悪くしないでね、職業病なの。はがきはアトリエに飾ってあって、水槽の魚たちが眺めています。\n\nV・タデッセ"),
             T("letters.home.tadesse.2",
               "Дорогой мой корреспондент!\n\nВторая открытка, «{0}». А у нас новость: Хэмиш поймал треску, первую за весь сезон, и не дал мне её написать, съел. Говорит, натюрморт должен быть в желудке. Пятьдесят лет вместе, а спорим, как на первом свидании.\n\nВаля",
               "My dear correspondent,\n\nA second postcard, \"{0}\". And we have news: Hamish caught a cod, the first all season, and wouldn't let me paint it: he ate it. He says a still life belongs in the stomach. Fifty years together, and we argue like on our first date.\n\nValya",
               "親愛なる文通相手へ\n\n二枚目のはがき「{0}」。こちらにも知らせがあります。ハミッシュが今季はじめてのタラを釣ったのに、私に描かせないで食べてしまいました。静物画は胃袋に入れるものだ、ですって。五十年いっしょにいて、まだ初デートのときみたいにけんかしています。\n\nワーリャ"),
         ],
         invite=T("letters.home.tadesse.invite",
                  "Дорогой мальчик!\n\nЗаходите в мастерскую посмотреть, как я пишу воду. Это лучше видно, чем рассказать. Хэмиш обещает уху, если клюнет, и бутерброды, если нет. Звонить не надо, заходите. Рыб не кормить.\n\nВ. Тадессе",
                  "Dear boy,\n\nCome to the studio and see how I paint water. It's better seen than told. Hamish promises fish soup if they bite and sandwiches if they don't. No need to ring: come in. Don't feed the fish.\n\nV. Tadesse",
                  "やさしい坊やへ\n\n水の描き方を見にアトリエへいらっしゃい。話すより見るほうが早いから。ハミッシュは、釣れたら魚のスープ、釣れなかったらサンドイッチを約束しています。呼び鈴は鳴らさずに、どうぞお入りなさい。魚にえさはあげないでね。\n\nV・タデッセ"),
         shelf_note=T("letters.shelf.tadesse", "На полке с альбомами стоит моя открытка. В углу на ней кто-то дорисовал камбалу.",
                      "My postcard stands on the shelf of albums. Somebody has drawn a flounder into its corner.",
                      "画集の棚に僕のはがきが立っている。隅に誰かがカレイを描き足してある。")),
    dict(id="h_tulegenova", home="P-UNITE", shelf="P-UNITE/tul_album", person="t_tulegenov_priya",
         name=T("letters.who.tulegenova", "Прия Тулегенова", "Priya Tulegenova", "プリヤ・トゥレゲノワ"),
         replies=[
             T("letters.home.tulegenova.1",
               "Здравствуйте!\n\nВаша открытка «{0}» уже в альбоме для Ширы, на отдельной странице, с подписью. Шира живёт далеко, в городе, где не бывает моря, и собирает чужие города по открыткам. Теперь у неё есть ваш взгляд на наш. Спасибо!\n\nПрия, кв. 5-12",
               "Hello!\n\nYour postcard \"{0}\" is already in the album for Shira, on a page of its own, with a caption. Shira lives far away, in a city with no sea, and collects other people's towns by postcard. Now she has your view of ours. Thank you!\n\nPriya, flat 5-12",
               "こんにちは！\n\nあなたのはがき「{0}」は、もうシーラのアルバムに入っています。一ページまるごと使って、説明もつけました。シーラは海のない遠い町に住んでいて、はがきでよその町を集めているの。これで、私たちの町をあなたの目で見られます。ありがとう！\n\nプリヤ　5-12号室"),
             T("letters.home.tulegenova.2",
               "Здравствуйте ещё раз!\n\nПришёл ответ от Ширы! Она пишет, что больше всего ей понравилась открытка «с серым морем в уголках», то есть ваша. А вторая, «{0}», едет к ней следующим письмом. Шира передаёт вам привет и вопрос: правда ли у нас видно солнце над морем по утрам?\n\nПрия",
               "Hello again!\n\nShira has answered! She writes that the card she liked best was the one \"with the grey sea in the corners\", that is, yours. And the second, \"{0}\", goes to her in the next letter. Shira sends you her greetings and a question: is it true that here you can see the sun over the sea in the mornings?\n\nPriya",
               "また、こんにちは！\n\nシーラから返事が来ました！いちばん気に入ったのは「すみに灰色の海があるはがき」、つまりあなたのだそうです。二枚目の「{0}」は次の手紙で送ります。シーラからよろしくと、それから質問です。この町では、朝、海の上にお日さまが見えるって本当？\n\nプリヤ"),
         ],
         invite=T("letters.home.tulegenova.invite",
                  "Здравствуйте!\n\nЗаходите вечером, с девяти до десяти, дверь синяя, на седьмом. Я покажу вам весь альбом для Ширы, от первой открытки до вашей. Будет чай с чак-чаком, мама прислала.\n\nПрия",
                  "Hello!\n\nCome in the evening, between nine and ten: the blue door on the seventh floor. I'll show you the whole album for Shira, from the first card to yours. There'll be tea with chak-chak: my mother sent some.\n\nPriya",
                  "こんにちは！\n\n夜の九時から十時のあいだにいらしてください。七階の青いドアです。シーラのアルバムを、最初のはがきからあなたのまで全部見せますね。お茶とチャクチャクがあります。母が送ってくれたの。\n\nプリヤ"),
         shelf_note=T("letters.shelf.tulegenova", "В альбоме для Ширы моя открытка на отдельной странице, с подписью мелким почерком.",
                      "In the album for Shira my postcard has a page of its own, captioned in small handwriting.",
                      "シーラのアルバムで、僕のはがきは一ページまるごと使われ、小さな字で説明がついている。")),
]

# --- thank-you letters after a quest is done (quest id, who signs: a name key, the letter, days after) -----------------

THANKS = [
    ("waxwing", "npc.sorokin_oleg.name", T("letters.thanks.waxwing",
        "Здравствуй, наблюдатель!\n\nПишу тебе, потому что сказать вслух у меня не вышло. Сорок лет я ходил за свиристелями, а этот прилетел ко мне сам, к окну. Я понимаю, что это ты его привёл. Спасибо. Если увидишь стайку, хоть одну птицу, запиши день и место. Я проверю по старым журналам, те ли это.\n\nО. Сорокин",
        "Hello, observer,\n\nI'm writing because saying it aloud didn't come off. Forty years I walked after waxwings, and this one came to me itself, to my window. I understand it was you who brought it. Thank you. If you see a flock, even one bird, note the day and the place. I'll check the old logs to see whether they're the same ones.\n\nO. Sorokin",
        "観察者くんへ\n\n声に出しては言えなかったので、手紙を書く。四十年レンジャクを追いかけたが、あの一羽は自分から私の窓まで来てくれた。君が連れてきたのだとわかっている。ありがとう。群れを見たら、一羽でもいい、日付と場所を書きとめてくれ。古い記録と照らして、同じ鳥かどうか調べてみる。\n\nO・ソローキン"), 1),
    ("evening_paper", "npc.korneeva_nina.name", T("letters.thanks.evening_paper",
        "Сынок, здравствуй!\n\nГазету я прочитала всю, от погоды до кроссворда. Кроссворд не решила: там одни иностранные реки. Ваня говорит, раньше газету носили каждый вечер, и все во дворе знали, кто что вычитал. Теперь знаю я одна, и расскажу всем. Заходи за пирожками, с капустой будут в четверг.\n\nНина Корнеева",
        "Hello, sonny!\n\nI read the whole paper, from the weather to the crossword. I didn't solve the crossword: it's all foreign rivers. Vanya says the paper used to come every evening, and everybody in the yard knew who'd read what. Now I'm the only one who knows, and I'll tell everybody. Come for pies: cabbage ones on Thursday.\n\nNina Korneeva",
        "坊や、こんにちは！\n\n新聞は天気予報からクロスワードまで全部読んだよ。クロスワードは解けなかった。外国の川ばっかりなんだもの。ワーニャが言うには、昔は毎晩新聞が届いて、中庭のみんなが誰が何を読んだか知っていたんだって。今はあたしだけが知ってるから、みんなに話してあげる。ピロシキを食べにおいで。木曜日はキャベツのだよ。\n\nニーナ・コルネーエワ"), 1),
    ("crow_gift", "npc.sato_yuki.name", T("letters.thanks.crow_gift",
        "Привет!!!\n\nВорона сегодня принесла мне на подоконник фантик. Мама говорит, это подарок. Я ей тоже положила крошку от печенья. Я больше не боюсь ворон. Только чуть-чуть, когда они смотрят одним глазом. Спасибо тебе!!!\n\nЮки (8 лет)",
        "Hi!!!\n\nThe crow brought a sweet wrapper to my windowsill today. Mum says it's a present. I put a biscuit crumb out for her too. I'm not afraid of crows any more. Only a tiny bit when they look with one eye. Thank you!!!\n\nYuki (aged 8)",
        "こんにちは！！！\n\n今日カラスが、まどのところにあめのつつみ紙をもってきたよ。ママはプレゼントだって。わたしもクッキーのかけらをおいてあげた。もうカラスはこわくない。かた目でじっと見るときだけ、ちょっとこわい。ありがとう！！！\n\nユキ（8さい）"), 1),
    ("fisherman_cheese", "npc.lange_pyotr.name", T("letters.thanks.fisherman_cheese",
        "Парень.\n\nСыр был хороший. Съел весь в один вечер, с хлебом, на крыльце. Жена бы сказала, что так нельзя. Жены нет, так что можно. Червей бери, когда надо, банка под лодкой.\n\nП. Ланге",
        "Lad.\n\nThe cheese was good. Ate the lot in one evening, with bread, on the porch. My wife would have said you mustn't. No wife now, so you may. Take worms when you need them: the tin's under the boat.\n\nP. Lange",
        "坊主へ\n\nチーズはうまかった。ひと晩で全部、パンと一緒に玄関先で食った。女房がいたら、そんな食べ方はだめだと言っただろう。もういないから、いいんだ。ミミズが要るときは持っていけ。缶はボートの下だ。\n\nP・ランゲ"), 1),
    ("window_feeder", "npc.kimura_dasha.name", T("letters.thanks.window_feeder",
        "Привет, соавтор!\n\nВика моет посуду уже пятый день и делает вид, что ей нравится. А к кормушке теперь прилетают синицы, воробьи и один поползень, который ходит вниз головой. Я веду учёт в тетрадке. Если хочешь, приходи смотреть, у нас на кухне лучший вид на эксперимент.\n\nДаша",
        "Hi, co-author!\n\nVika has been washing the dishes for five days now and pretends she likes it. And now tits, sparrows and one nuthatch that walks upside down come to the feeder. I keep a count in an exercise book. Come and watch if you like: our kitchen has the best view of the experiment.\n\nDasha",
        "共同研究者さんへ\n\nヴィーカはもう五日もお皿を洗っていて、楽しいふりをしています。えさ台には今、シジュウカラとスズメと、さかさまに歩くゴジュウカラが一羽来ます。ノートに記録をつけているの。よかったら見に来て。うちの台所が、実験がいちばんよく見える場所だから。\n\nダーシャ"), 1),
    ("magpie_key", "npc.belov_misha.name", T("letters.thanks.magpie_key",
        "Привет, следователь!\n\nОфициально: сорока оправдана, ключ найден на магните, виноват я. Неофициально: она теперь каждое утро сидит у мастерской и ждёт полпеченья. Я ей купил целую пачку. Если что-нибудь пропадёт, приходи, будем расследовать вместе, теперь уже без подозреваемых.\n\nМиша",
        "Hi, detective!\n\nOfficially: the magpie is acquitted, the key was found on the magnet, I'm to blame. Unofficially: she sits by the workshop every morning now waiting for half a biscuit. I've bought her a whole packet. If anything goes missing, come and we'll investigate together, without suspects this time.\n\nMisha",
        "探偵くんへ\n\n公式発表：カササギは無罪、鍵は磁石にくっついていた、悪いのは僕。非公式：あいつは今、毎朝工房の前でクッキー半分を待っている。ひと箱買ってやったよ。また何かなくなったら来てくれ。今度は容疑者なしで一緒に捜査しよう。\n\nミーシャ"), 1),
    ("painter_views", "npc.nishimura_aya.name", T("letters.thanks.painter_views",
        "Здравствуй!\n\nЯ начала писать город по твоим рассказам. Получается не тот город, что за окном, а тот, что ты видел, и он, кажется, лучше. Скворцы вышли похожими на дым, как ты сказал. Если увидишь ещё что-нибудь, что стоит написать, приходи и расскажи. Ноги снова будут твои, кисти мои.\n\nАя",
        "Hello!\n\nI've begun painting the town from what you told me. It doesn't come out as the town outside my window but as the one you saw, and that one, I think, is better. The starlings came out like smoke, as you said. If you see anything else worth painting, come and tell me. The legs will be yours again, the brushes mine.\n\nAya",
        "こんにちは！\n\nあなたの話をもとに町を描きはじめました。窓の外の町じゃなくて、あなたが見た町になっていくの。たぶん、そっちのほうがいい町。ムクドリはあなたが言ったとおり、煙みたいになりました。ほかにも描く価値のあるものを見たら、話しに来てね。また足はあなた、筆は私で。\n\nアヤ"), 1),
    ("first_catch", "npc.lange_pyotr.name", T("letters.thanks.first_catch",
        "Парень.\n\nБлесну береги, она счастливая, я на неё первую треску взял в шестьдесят восьмом. Рыбу свою запомни: какая была, где, какой ветер. Потом их будет много, а первая одна.\n\nП. Ланге",
        "Lad.\n\nLook after the spoon: it's a lucky one, I took my first cod on it in sixty-eight. Remember your fish: what it was, where, which wind. There'll be many later, but only one first.\n\nP. Lange",
        "坊主へ\n\nあのスプーンは大事にしろ。縁起物だ。六十八年に、あれで初めてタラを釣った。お前の魚を覚えておけ。どんな魚で、どこで、どんな風だったか。この先たくさん釣れるだろうが、最初の一匹は一匹きりだ。\n\nP・ランゲ"), 1),
    ("soda_science", "npc.zotova_galina.name", T("letters.thanks.soda_science",
        "Уважаемый коллега!\n\nДокладываю: результаты вашей дегустации повторены в лаборатории хроматографом. Сиропов действительно четыре. Хроматограф подтвердил то, что вы определили языком за пять минут. Лев Аркадьевич предлагает внести вас в штат. Я против: вы слишком хороший прибор для нашей зарплаты.\n\nГ. Зотова",
        "Esteemed colleague,\n\nI report: the results of your tasting have been repeated in the laboratory with a chromatograph. There are indeed four syrups. The chromatograph confirmed what you determined with your tongue in five minutes. Lev Arkadyevich proposes putting you on the staff. I'm against it: you're too good an instrument for our pay.\n\nG. Zotova",
        "尊敬する同僚へ\n\n報告します。あなたの試飲結果を研究所のクロマトグラフで再現しました。シロップはたしかに四種類。あなたが舌で五分で判定したことを、機械が確認したわけです。レフ・アルカージエヴィチはあなたを職員にしようと言っています。私は反対です。うちの給料にはもったいない装置ですから。\n\nG・ゾートワ"), 1),
    ("hot_corn", "npc.guseva_valentina.name", T("letters.thanks.hot_corn",
        "Милый мой!\n\nИван так и не пришёл, но прислал с внуком записку: «Початок был как в семьдесят пятом. Клюёт плохо, зайду, когда перестанет». Я эту записку положила в кошелёк. Приходи на площадь, кукуруза теперь тебе за полцены, а соль бесплатно, как всем.\n\nВалентина Гусева",
        "My dear,\n\nIvan never came, but he sent a note with his grandson: \"The cob was like in seventy-five. Fish aren't biting much; I'll come when they stop.\" I've put the note in my purse. Come to the square: corn's half price for you now, and salt's free, as for everyone.\n\nValentina Guseva",
        "坊やへ\n\nイワンは結局来なかったけど、孫にメモを持たせてよこしたよ。「七十五年の味だった。あまり釣れないから、釣れなくなったら行く」。そのメモは財布にしまったよ。広場においで。トウモロコシは君には半額、塩はみんなと同じで無料だよ。\n\nワレンチナ・グセワ"), 1),
    ("lab_lunch", "npc.li_vika.name", T("letters.thanks.lab_lunch",
        "Привет, эксперт!\n\nВ субботу я сходила в «Чайку» сама. Уха действительно хорошая, а укроп, вопреки Мише, тоже. Я впервые за три года пообедала сидя и не глядя в журнал измерений. Оказывается, в обеде есть смысл. Спасибо, что проверил это раньше меня.\n\nВика",
        "Hi, expert!\n\nOn Saturday I went to the Chaika myself. The fish soup really is good, and the dill, in spite of Misha, is too. For the first time in three years I had lunch sitting down and not looking at the measurement log. It turns out there's a point to lunch. Thank you for checking that before me.\n\nVika",
        "専門家さんへ\n\n土曜日、自分で「チャイカ」に行ってきました。ウハーは本当においしかったし、ミーシャには悪いけど、ディルもおいしかった。三年ぶりに、測定ノートを見ずに座ってお昼を食べました。お昼ごはんって意味があるのね。私より先に確かめてくれて、ありがとう。\n\nヴィーカ"), 1),
    ("night_signal", "npc.valiev_rustam.name", T("letters.thanks.night_signal",
        "Уважаемый помощник!\n\nЯ пересчитал. Если огонёк был вашим, то вы стояли на два метра левее, чем мы договаривались, и это объясняет, почему он мигал: его закрывала сосна. Так что, вероятно, это были вы. Опыт засчитан. Следующий проведём в ясную ночь, с двумя фонариками и без сосны.\n\nР. Валиев",
        "Esteemed assistant,\n\nI've recalculated. If the light was yours, you stood two metres further left than we agreed, and that explains why it flickered: a pine was in the way. So it probably was you. The experiment counts. We'll do the next one on a clear night, with two torches and no pine.\n\nR. Valiev",
        "尊敬する助手どのへ\n\n計算し直しました。あの光があなたのものなら、あなたは約束より二メートル左に立っていたことになります。それで点滅していた理由も説明がつく。松の木がさえぎっていたのです。つまり、たぶんあなたでした。実験は成功とみなします。次は晴れた夜に、懐中電灯を二つ持って、松なしでやりましょう。\n\nR・ワリエフ"), 1),
    ("autumn_count", "npc.sorokin_oleg.name", T("letters.thanks.autumn_count",
        "Коллега!\n\nУчёт отправлен в общество, с двумя фамилиями в графе «наблюдатель». Они ответили, что наш участок двадцать лет числился пустым. Теперь не числится. Это немного, но это правда, а правда в орнитологии дороже всего.\n\nО. Сорокин",
        "Colleague,\n\nThe count has gone off to the society with two names in the \"observer\" column. They answered that our patch had been listed as empty for twenty years. Now it isn't. That's not much, but it's true, and in ornithology the truth is worth most of all.\n\nO. Sorokin",
        "同僚へ\n\n調査記録を学会に送った。「観察者」の欄には名前が二つ。返事によると、この区画は二十年間「記録なし」になっていたそうだ。今はもう違う。たいしたことではないが、本当のことだ。鳥類学では、本当のことが何より尊い。\n\nO・ソローキン"), 2),
]

# --- invitations to the holidays ({0}: the holiday's name) ------------------------------------------------------------

INVITE_GENERIC = T("letters.invite.generic",
    "Уважаемый житель!\n\nГорсовет Людмиловска приглашает вас на праздник: {0}. Приходите сами и приводите соседей. Будут музыка, угощение и хорошее настроение, а погоду мы заказали.\n\nС уважением, горсовет",
    "Dear resident,\n\nLyudmilovsk Town Council invites you to the holiday: {0}. Come yourself and bring your neighbours. There'll be music, treats and good cheer, and we've ordered the weather.\n\nYours, the Town Council",
    "市民のみなさまへ\n\nリュドミロフスク市役所より、お祭りのご案内です：{0}。ご近所の方もお誘いあわせのうえ、どうぞお越しください。音楽とごちそうと笑顔があります。お天気も注文しておきました。\n\n市役所")
INVITE_FESTIVALS = {
    "sea_festival": T("letters.invite.sea_festival",
        "Уважаемый житель!\n\nВ субботу праздник: {0}. На набережной будут гонки на вёслах, уха в больших котлах и концерт на пирсе. Смотрители маяка зажгут огонь раньше обычного, в честь всех, кто в море.\n\nГорсовет Людмиловска",
        "Dear resident,\n\nOn Saturday it's the holiday: {0}. On the promenade there'll be rowing races, fish soup in big cauldrons and a concert on the pier. The lighthouse keepers will light the lamp earlier than usual, in honour of all those at sea.\n\nLyudmilovsk Town Council",
        "市民のみなさまへ\n\n土曜日はお祭りです：{0}。海岸通りではボートレース、大鍋のウハー、桟橋でのコンサートがあります。灯台守は海にいるすべての人のために、いつもより早く灯をともします。\n\nリュドミロフスク市役所"),
    "lantern_night": T("letters.invite.lantern_night",
        "Уважаемый житель!\n\nСкоро праздник: {0}. Вечером на пляже будем запускать небесные фонарики. Напишите на своём желание и отпустите его над морем. Фонарики и спички выдаём бесплатно у костра.\n\nГорсовет Людмиловска",
        "Dear resident,\n\nThe holiday is coming: {0}. In the evening we'll be sending up sky lanterns on the beach. Write a wish on yours and let it go over the sea. Lanterns and matches are free by the bonfire.\n\nLyudmilovsk Town Council",
        "市民のみなさまへ\n\nもうすぐお祭りです：{0}。夕方、浜辺で天灯をあげます。ご自分の天灯に願いごとを書いて、海の上へ放してください。天灯とマッチはたき火のそばで無料でお配りします。\n\nリュドミロフスク市役所"),
    "first_snow": T("letters.invite.first_snow",
        "Уважаемый житель!\n\nВыпал первый снег, а значит, завтра праздник: {0}. На площади будет горячий чай из самовара, катание с горки и конкурс снеговиков. Варежки приносите свои.\n\nГорсовет Людмиловска",
        "Dear resident,\n\nThe first snow has fallen, so tomorrow is the holiday: {0}. On the square there'll be hot tea from the samovar, sledging and a snowman contest. Bring your own mittens.\n\nLyudmilovsk Town Council",
        "市民のみなさまへ\n\n初雪が降りました。明日はお祭りです：{0}。広場ではサモワールの熱いお茶、そり遊び、雪だるまコンテストがあります。手袋はご持参ください。\n\nリュドミロフスク市役所"),
    "winter_fair": T("letters.invite.winter_fair",
        "Уважаемый житель!\n\nПриглашаем на праздник: {0}. Ярмарочные ряды, глинтвейн для взрослых, сбитень для всех, пряники и ёлочные игрушки ручной работы. Приходите выбирать подарки.\n\nГорсовет Людмиловска",
        "Dear resident,\n\nWe invite you to the holiday: {0}. Fair stalls, mulled wine for grown-ups, hot honey drink for all, gingerbread and hand-made tree decorations. Come and choose your presents.\n\nLyudmilovsk Town Council",
        "市民のみなさまへ\n\nお祭りのご案内です：{0}。屋台が並び、大人にはホットワイン、みなさんにははちみつの温かい飲み物、しょうがクッキーや手作りのツリー飾りもあります。贈りものを選びにいらしてください。\n\nリュドミロフスク市役所"),
    "new_year": T("letters.invite.new_year",
        "Уважаемый житель!\n\nГорсовет поздравляет вас с наступающим праздником: {0}. В полночь на площади будет салют, а до полуночи — ёлка, музыка и Дед Мороз, который обещал не опаздывать. С новым счастьем!\n\nГорсовет Людмиловска",
        "Dear resident,\n\nThe Town Council wishes you joy of the coming holiday: {0}. At midnight there'll be fireworks on the square, and before midnight a tree, music and Grandfather Frost, who has promised not to be late. A happy new year!\n\nLyudmilovsk Town Council",
        "市民のみなさまへ\n\n市役所より、まもなくのお祭りのお祝いを申し上げます：{0}。真夜中には広場で花火、その前にはツリーと音楽と、遅刻しないと約束したマロース爺さんが待っています。新年おめでとうございます！\n\nリュドミロフスク市役所"),
    "halloween": T("letters.invite.halloween",
        "Уважаемый житель!\n\nСкоро праздник: {0}. Дети пойдут по домам за конфетами: держите угощение у двери. На набережной вечером будут тыквы с фонариками и страшные истории у костра. Не очень страшные.\n\nГорсовет Людмиловска",
        "Dear resident,\n\nThe holiday is coming: {0}. The children will go from door to door for sweets: keep treats by the door. In the evening on the promenade there'll be lantern pumpkins and scary stories by the bonfire. Not too scary.\n\nLyudmilovsk Town Council",
        "市民のみなさまへ\n\nもうすぐお祭りです：{0}。子どもたちがお菓子をもらいに家々をまわります。戸口にお菓子をご用意ください。夕方の海岸通りには、ちょうちんのかぼちゃと、たき火を囲んでのこわい話があります。それほどこわくはありません。\n\nリュドミロフスク市役所"),
}

# --- the pen friend: Tonya Lotsmanova, 14, the lighthouse keeper's daughter at White Cape far north ---------------------
# Her letters come one by one: each waits for the hero's answer, chosen from what he really did (its "ask"). {0} in a
# body is White Cape. The acknowledgement that opens her next letter names his answer ({0}: the bird, the fish, the place).

PEN = [
    dict(ask="bird",
         body=T("letters.pen.1.body",
            "Здравствуй, незнакомый наблюдатель!\n\nМеня зовут Тоня, мне четырнадцать, я живу на маяке {0}, мой папа здесь смотритель. В бюллетене Общества друзей птиц напечатали список юных наблюдателей, и от Людмиловска там был только один адрес: клуб «Атон». Я решила, что это ты.\n\nУ нас уже почти нет дня: солнце показывается над морем ненадолго, как будто проверить, всё ли на месте. Гаги собираются в стаи и уходят на юг, может быть, к вам. Я их считаю каждое утро с галереи маяка. Вчера насчитала триста двенадцать.\n\nНапиши мне, пожалуйста, какие птицы живут у вас. Я никогда не была на тёплом море и не знаю, кто там летает.\n\nТоня Лоцманова, маяк {0}",
            "Hello, unknown observer!\n\nMy name is Tonya, I'm fourteen, I live at the lighthouse on {0}, and my father is the keeper here. The Friends of Birds bulletin printed a list of young observers, and from Lyudmilovsk there was only one address: the Aton club. I decided it was you.\n\nWe have hardly any day left now: the sun shows over the sea for a little while, as if to check that everything's still there. The eiders are gathering in flocks and leaving for the south, maybe for yours. I count them every morning from the lighthouse gallery. Yesterday I counted three hundred and twelve.\n\nPlease write and tell me which birds live where you are. I've never been to a warm sea and don't know who flies there.\n\nTonya Lotsmanova, {0} lighthouse",
            "はじめまして、知らない観察者さん！\n\n私はトーニャ、十四歳です。{0}の灯台に住んでいて、父がここの灯台守です。「鳥の友の会」の会報に若い観察者の名簿がのっていて、リュドミロフスクからはひとつだけ、クラブ「アトン」の住所がありました。それがあなただと決めました。\n\nこちらはもうほとんど昼がありません。お日さまは、みんなちゃんといるか確かめるみたいに、ちょっとだけ海の上に顔を出します。ケワタガモが群れになって南へ渡っていきます。もしかしたら、そちらへ。毎朝、灯台の回廊から数えています。きのうは三百十二羽でした。\n\nそちらにはどんな鳥がいるのか、教えてください。あたたかい海には行ったことがないので、どんな鳥が飛んでいるのか知らないの。\n\n{0}灯台　トーニャ・ロツマノワ"),
         diary=T("letters.pen.1.diary", "Мне написала девочка с маяка на Белом Мысе, Тоня. Она считает гаг и спрашивает, какие птицы у нас.",
                 "A girl from the lighthouse at White Cape wrote to me: Tonya. She counts eiders and asks which birds we have.",
                 "白岬の灯台の女の子、トーニャから手紙が来た。ケワタガモを数えていて、こっちにどんな鳥がいるか知りたいそうだ。")),
    dict(ask="place",
         body=T("letters.pen.2.body",
            "Здравствуй!\n\nТвоё письмо пришло! Его везли поездом, потом машиной, потом на катере с почтой и хлебом. Папа сказал, что за такую дорогу письмо заслуживает чая, и мы читали его за чаем, вслух, два раза.\n\nУ нас началась полярная ночь. Это не так страшно, как звучит: днём бывает синее время, когда всё светится само, и снег, и море. А вчера было северное сияние, зелёное, с розовым краем, оно двигалось, как занавеска от сквозняка. Лампа маяка рядом с ним выглядела совсем маленькой.\n\nРасскажи, где ты любишь гулять. У нас гулять можно только от маяка до метеостанции и обратно, два километра, я знаю там каждый камень.\n\nТоня",
            "Hello!\n\nYour letter came! It went by train, then by car, then on the launch with the post and the bread. Dad said a letter that's come so far deserves tea, and we read it over tea, aloud, twice.\n\nThe polar night has begun here. It isn't as frightening as it sounds: in the daytime there's a blue time when everything glows by itself, the snow and the sea. And yesterday there were northern lights, green with a pink edge, moving like a curtain in a draught. The lighthouse lamp looked quite small beside them.\n\nTell me where you like to walk. Here you can only walk from the lighthouse to the weather station and back, two kilometres, and I know every stone.\n\nTonya",
            "こんにちは！\n\nあなたの手紙が届きました！汽車で、それから車で、それから郵便とパンを運ぶ船で。こんなに遠くから来た手紙にはお茶がふさわしいと父が言って、お茶を飲みながら声に出して二回読みました。\n\nこちらは極夜が始まりました。聞こえるほどこわくはありません。昼には、雪も海も自分で光っているような青い時間があります。きのうはオーロラが出ました。緑色で、ふちがピンク色で、すきま風に揺れるカーテンみたいに動いていました。そのそばでは、灯台の灯がとても小さく見えました。\n\nあなたはどこを散歩するのが好き？こちらで歩けるのは灯台から気象観測所までの往復二キロだけで、石の一つ一つまで知っています。\n\nトーニャ"),
         diary=T("letters.pen.2.diary", "Тоня пишет, что у них полярная ночь и было северное сияние. Спрашивает, где я гуляю.",
                 "Tonya writes that the polar night has begun there and they had northern lights. She asks where I walk.",
                 "トーニャの手紙：向こうは極夜が始まって、オーロラが出たそうだ。僕がどこを散歩するのか知りたがっている。")),
    dict(ask="fish",
         body=T("letters.pen.3.body",
            "Здравствуй!\n\nТри дня был шторм. Волны перелетали через мол, и маяк гудел, как бутылка, если в неё дуть. Папа не спал две ночи, а я носила ему чай наверх, сто двенадцать ступенек. После шторма на берег прилетели пуночки — маленькие, белые с коричневым, как будто снег научился летать.\n\nКогда встанет лёд, папа поведёт меня рыбачить в лунку. Он говорит, у них тут ловится навага, а если повезёт, то треска. Я ещё ни разу ничего не поймала, только варежку уронила в лунку.\n\nА ты рыбачишь? Что у вас ловится? Напиши, какую рыбу ты поймал сам.\n\nТоня",
            "Hello!\n\nThere was a storm for three days. The waves flew over the breakwater, and the lighthouse hummed like a bottle when you blow into it. Dad didn't sleep for two nights, and I carried tea up to him, a hundred and twelve steps. After the storm the snow buntings came to the shore: little, white and brown, as if the snow had learned to fly.\n\nWhen the ice sets, Dad will take me fishing through a hole. He says you catch navaga here and, if you're lucky, cod. I've never caught anything yet, only dropped a mitten down the hole.\n\nDo you fish? What's caught where you are? Write and tell me which fish you caught yourself.\n\nTonya",
            "こんにちは！\n\n三日間、嵐でした。波が防波堤を越えて飛んできて、灯台はびんに息を吹きこんだときみたいに鳴っていました。父は二晩眠らなくて、私は百十二段の階段を上ってお茶を運びました。嵐のあと、浜にユキホオジロが来ました。白と茶色の小さな鳥で、雪が飛び方を覚えたみたいです。\n\n氷がはったら、父が氷に穴をあけて釣りに連れていってくれます。ここではコマイが釣れて、運がよければタラも釣れるそうです。私はまだ一匹も釣ったことがなくて、穴に手袋を落としただけ。\n\nあなたは釣りをする？そちらでは何が釣れるの？自分で釣った魚のこと、教えて。\n\nトーニャ"),
         diary=T("letters.pen.3.diary", "У Тони три дня был шторм, после него прилетели пуночки. Она спрашивает, какую рыбу я поймал.",
                 "Tonya had a three-day storm, and snow buntings came after it. She asks what fish I've caught.",
                 "トーニャのところは三日間嵐で、そのあとユキホオジロが来たそうだ。僕が何を釣ったか聞いている。")),
    dict(ask="friend",
         body=T("letters.pen.4.body",
            "Здравствуй!\n\nУ меня есть друг. Его зовут Капитан, он ворон. Он живёт на крыше метеостанции и каждое утро прилетает к маяку проверить, не осталось ли чего от завтрака. Сначала он брал хлеб, только если я отходила на десять шагов. Потом на пять. Вчера он взял сухарь прямо с варежки и посмотрел на меня так, будто это он меня приручил, а не я его.\n\nПапа говорит, что вороны помнят лица много лет. Значит, Капитан будет помнить меня, даже когда я вырасту и уеду учиться.\n\nА у тебя есть друг среди птиц? Кто-нибудь берёт у тебя еду с руки?\n\nТоня",
            "Hello!\n\nI have a friend. His name is Captain, and he's a raven. He lives on the roof of the weather station and every morning flies over to the lighthouse to check whether anything's left from breakfast. At first he only took bread if I stepped ten paces away. Then five. Yesterday he took a rusk straight off my mitten and looked at me as if he had tamed me, not I him.\n\nDad says ravens remember faces for many years. So Captain will remember me even when I grow up and go away to study.\n\nDo you have a friend among the birds? Does anybody take food from your hand?\n\nTonya",
            "こんにちは！\n\n私には友だちがいます。名前はキャプテン、ワタリガラスです。気象観測所の屋根に住んでいて、毎朝、朝ごはんの残りがないか灯台まで確かめに来ます。はじめは私が十歩離れないとパンを取りませんでした。それが五歩になって、きのうは手袋の上から直接乾パンを取って、私が手なずけたんじゃなくて自分が私を手なずけたんだ、という顔で私を見ました。\n\nカラスは人の顔を何年も覚えているそうです。だからキャプテンは、私が大きくなって勉強しに町へ出ても、私を覚えているはず。\n\nあなたには鳥の友だちがいる？手から食べてくれる鳥はいる？\n\nトーニャ"),
         diary=T("letters.pen.4.diary", "Тоня подружилась с вороном Капитаном: он берёт сухарь с её варежки. Спрашивает, есть ли у меня друг среди птиц.",
                 "Tonya has made friends with a raven called Captain: he takes a rusk from her mitten. She asks if I have a friend among the birds.",
                 "トーニャはワタリガラスのキャプテンと仲よくなった。手袋から乾パンを取るそうだ。僕に鳥の友だちがいるか聞いている。")),
    dict(ask="bird",
         body=T("letters.pen.5.body",
            "Здравствуй!\n\nСлучилось чудо: к маяку прилетела белая чайка. Совсем белая, как фарфоровая, с чёрными лапками. Папа сказал, что их осталось очень мало и что за тридцать лет на маяке он видел такую четыре раза. Она посидела на перилах галереи минуты три, посмотрела на лампу и улетела в темноту. Я даже не успела позвать папу второй раз.\n\nТеперь я выхожу на галерею каждый час, хотя знаю, что она не вернётся. Так, наверное, и становятся орнитологами: один раз увидишь и потом ждёшь всю жизнь.\n\nА кого ты видел нового? Мне кажется, у вас каждый день кто-то новый.\n\nТоня",
            "Hello!\n\nA miracle happened: an ivory gull came to the lighthouse. Completely white, like porcelain, with black feet. Dad said there are very few of them left, and in thirty years at the lighthouse he's seen one four times. She sat on the gallery rail for about three minutes, looked at the lamp and flew off into the dark. I didn't even have time to call Dad a second time.\n\nNow I go out on the gallery every hour, though I know she won't come back. Maybe that's how people become ornithologists: you see something once and then wait all your life.\n\nWho have you seen that's new? It seems to me you have someone new every day.\n\nTonya",
            "こんにちは！\n\n奇跡が起きました。灯台にゾウゲカモメが来たの。陶器みたいに真っ白で、足だけ黒いカモメです。父の話では、もうとても数が少なくて、灯台で暮らした三十年で見たのは四回だけだそうです。回廊の手すりに三分くらいとまって、灯りを見てから暗やみへ飛んでいきました。父を二回目に呼ぶ間もありませんでした。\n\n戻ってこないとわかっているのに、今は一時間ごとに回廊へ出ています。鳥類学者って、きっとこうしてなるんですね。一度見たら、あとは一生待つの。\n\nあなたは新しく誰を見た？そちらでは毎日新しい鳥がいる気がします。\n\nトーニャ"),
         diary=T("letters.pen.5.diary", "К Тониному маяку прилетела белая чайка, папа видел такую всего четыре раза. Тоня спрашивает, кого нового видел я.",
                 "An ivory gull came to Tonya's lighthouse: her father has seen one only four times. Tonya asks who I've seen that's new.",
                 "トーニャの灯台にゾウゲカモメが来た。お父さんでも四回しか見たことがないそうだ。僕が新しく誰を見たか聞いている。")),
    dict(ask="place",
         body=T("letters.pen.6.body",
            "Здравствуй!\n\nЯ рисую карту нашего мыса. Настоящую, с масштабом и розой ветров, как у папы на стене. Я отметила маяк, метеостанцию, Тюлений камень, ручей, который летом поёт, а зимой молчит, и место, где Капитан прячет свои сокровища: там у него крышка от банки, ложка и синяя пуговица.\n\nКогда карта будет готова, я пошлю тебе копию. А ты пришли мне своё место, самое хорошее, я нарисую его на полях, со стрелкой: «Людмиловск, туда». Тогда на моей карте будет два края света, наш и твой.\n\nТоня",
            "Hello!\n\nI'm drawing a map of our cape. A real one, with a scale and a wind rose, like the one on Dad's wall. I've marked the lighthouse, the weather station, Seal Rock, the stream that sings in summer and is silent in winter, and the place where Captain hides his treasures: he has a jar lid there, a spoon and a blue button.\n\nWhen the map's ready I'll send you a copy. And you send me your place, the best one, and I'll draw it in the margin with an arrow: \"Lyudmilovsk, that way\". Then my map will have two ends of the earth, ours and yours.\n\nTonya",
            "こんにちは！\n\n岬の地図をかいています。父の部屋の壁にあるような、縮尺と風配図のついた本物の地図です。灯台、気象観測所、アザラシ岩、夏は歌って冬は黙る小川、それからキャプテンが宝物をかくしている場所をかきこみました。びんのふたと、スプーンと、青いボタンがあるの。\n\nできあがったら写しを送ります。あなたも、いちばんいい場所を教えて。余白に矢印つきでかきます。「リュドミロフスクはあっち」って。そうしたら私の地図には、地の果てが二つになる。私たちのと、あなたのと。\n\nトーニャ"),
         diary=T("letters.pen.6.diary", "Тоня рисует карту своего мыса и хочет нарисовать на полях моё самое хорошее место.",
                 "Tonya is drawing a map of her cape and wants to draw my best place in the margin.",
                 "トーニャは岬の地図をかいていて、僕のいちばんいい場所を余白にかきたいそうだ。")),
    dict(ask="bird",
         body=T("letters.pen.7.body",
            "Здравствуй!\n\nКарта готова, копию посылаю. Твоё место я нарисовала на полях, там, где у карты кончается север и начинается всё остальное.\n\nУ нас самая тёмная неделя в году. В школу я хожу по радио: учительница читает урок с Большой земли, а мы, четверо ребят с разных станций, отвечаем по очереди. Слышно плохо, зато весело. Папа говорит, что скоро повернёт к свету, и что он это чувствует коленями раньше, чем видит.\n\nЯ веду список птиц за зиму. Пока у меня девять видов. Пришли мне ещё одного, твоего, я впишу его отдельной строчкой: «видел мой друг на юге».\n\nТоня",
            "Hello!\n\nThe map is ready, and I'm sending you a copy. I drew your place in the margin, where the map's north ends and everything else begins.\n\nIt's the darkest week of the year here. I go to school by radio: the teacher reads the lesson from the mainland, and we, four children from different stations, answer in turn. It's hard to hear, but fun. Dad says it'll soon turn towards the light, and that he feels it in his knees before he sees it.\n\nI'm keeping a list of the winter's birds. So far I have nine species. Send me one more, yours, and I'll write it in on a line of its own: \"seen by my friend in the south\".\n\nTonya",
            "こんにちは！\n\n地図ができたので、写しを送ります。あなたの場所は、地図の北が終わって、ほかの全部が始まる余白にかきました。\n\nこちらは一年でいちばん暗い週です。学校には無線で通っています。先生が本土から授業を読んで、別々の観測所にいる四人の子どもが順番に答えるの。聞こえにくいけど、楽しいです。父は、もうすぐ光のほうへ向かう、目で見るより先にひざでわかる、と言っています。\n\n冬の鳥のリストをつけています。今のところ九種類。もう一種類、あなたの鳥を送って。別の行に書きます。「南の友だちが見た」って。\n\nトーニャ"),
         diary=T("letters.pen.7.diary", "Тоня прислала копию своей карты: моё место на полях. Она ведёт список зимних птиц и просит ещё одну, мою.",
                 "Tonya sent a copy of her map, with my place in the margin. She's keeping a list of winter birds and asks for one more, mine.",
                 "トーニャが地図の写しを送ってくれた。余白に僕の場所がある。冬の鳥のリストに、僕の鳥をもう一種類加えたいそうだ。")),
    dict(ask="",
         body=T("letters.pen.8.body",
            "Здравствуй, мой друг на юге!\n\nТвоя птица вписана в список десятой, отдельной строчкой, как обещала.\n\nПапа говорит, что солнце вернётся совсем скоро. В тот день, когда оно выглянет из-за моря, я поднимусь на самый верх маяка, к лампе, и помашу в твою сторону. Я посмотрела по карте: это прямо над Тюленьим камнем и дальше, далеко, через всю страну. Если в тот день у вас солнце будет светить в окно, знай, что это я машу.\n\nВесной гаги вернутся. Посчитай их у себя, когда они полетят на север, а я посчитаю их здесь, когда они прилетят. Так мы узнаем, сколько их долетело. Пиши мне. Всегда пиши.\n\nТвоя Тоня, маяк {0}\n\nP.S. В конверте перо Капитана. Он разрешил.",
            "Hello, my friend in the south!\n\nYour bird is written into the list as the tenth, on a line of its own, as I promised.\n\nDad says the sun will come back very soon. On the day it looks out from beyond the sea, I'll climb to the very top of the lighthouse, to the lamp, and wave your way. I looked on the map: it's right over Seal Rock and on, far away, across the whole country. If the sun shines in at your window that day, know that it's me waving.\n\nIn spring the eiders will come back. Count them where you are when they fly north, and I'll count them here when they arrive. That way we'll know how many made it. Write to me. Always write.\n\nYour Tonya, {0} lighthouse\n\nP.S. There's a feather of Captain's in the envelope. He said I could.",
            "南にいる友だちへ\n\nあなたの鳥は、約束どおり十番目に、別の行に書きこみました。\n\nもうすぐお日さまが戻ってくると父が言っています。海の向こうからお日さまが顔を出す日、私は灯台のいちばん上、灯りのところまで上って、あなたのほうへ手をふります。地図で調べたら、アザラシ岩の真上の、そのずっと先、国をまるごと越えたところでした。その日、あなたの窓にお日さまが差したら、それは私が手をふっているのだと思ってね。\n\n春にはケワタガモが帰ってきます。北へ飛んでいくときにそちらで数えてください。こちらに着いたら私が数えます。そうすれば、何羽たどりついたかわかる。手紙をください。ずっと書いてね。\n\n{0}灯台　あなたのトーニャ\n\n追伸　封筒にキャプテンの羽が入っています。本人の許可はとりました。"),
         diary=T("letters.pen.8.diary", "Тоня пишет: когда к ним вернётся солнце, она поднимется к лампе маяка и помашет в мою сторону. В конверте перо её ворона.",
                 "Tonya writes: when the sun comes back to them, she'll climb up to the lighthouse lamp and wave my way. Her raven's feather is in the envelope.",
                 "トーニャの手紙：お日さまが戻ったら灯台の灯りまで上って、僕のほうへ手をふるそうだ。封筒にはカラスの羽が入っていた。")),
]

PEN_NUDGE = T("letters.pen.nudge",
    "Здравствуй!\n\nТы получил моё письмо? Почта у нас ходит, когда хочет: катер ждёт погоды, погода ждёт ветра. Если не получил, я напишу ещё раз. Если получил, просто напиши хоть два слова, я буду знать, что ты есть.\n\nТоня",
    "Hello!\n\nDid you get my letter? Our post comes when it likes: the launch waits for the weather, the weather waits for the wind. If you didn't get it, I'll write again. If you did, just write two words, and I'll know you're there.\n\nTonya",
    "こんにちは！\n\n私の手紙、届いた？こちらの郵便は気まぐれです。船は天気を待って、天気は風を待っているの。届いていなかったら、もう一度書きます。届いていたら、ひとことでいいから書いて。あなたがいるってわかるように。\n\nトーニャ")

PEN_ACKS = {
    "bird": [
        T("letters.pen.ack.bird.1", "Ты пишешь, что видел: {0}! Я нашла эту птицу в папином определителе и обвела карандашом. У нас таких нет, теперь буду знать, что они есть у тебя.",
          "You write that you saw this: {0}! I found it in Dad's field guide and circled it in pencil. We don't have them here; now I'll know they're there with you.",
          "あなたが見たのは：{0}！父の図鑑で見つけて、鉛筆で丸をつけました。こちらにはいない鳥だから、あなたのところにいるって覚えておくね。"),
        T("letters.pen.ack.bird.2", "Про птицу, которую ты видел ({0}), я рассказала Капитану. Он сделал вид, что ему всё равно, но слушал.",
          "I told Captain about the bird you saw ({0}). He pretended not to care, but he listened.",
          "あなたが見た鳥（{0}）のこと、キャプテンに話しました。興味ないふりをしていたけど、ちゃんと聞いていました。"),
    ],
    "fish": [
        T("letters.pen.ack.fish.1", "Твоя рыба, {0}, — настоящий улов! Папа сказал, что ты молодец и что первую рыбу надо записывать в журнал. Я записала твою в свой.",
          "Your fish, {0}, is a real catch! Dad said well done and that a first fish must go in the log. I wrote yours in mine.",
          "あなたの魚、{0}、本物の獲物ね！父が「よくやった、最初の魚は日誌に書くものだ」って。私の日誌に、あなたのを書いておきました。"),
        T("letters.pen.ack.fish.2", "Ты поймал рыбу: {0}. А я пока поймала только варежку. Но лёд уже почти встал, так что жди новостей.",
          "You caught a fish: {0}. And I've only caught a mitten so far. But the ice has nearly set, so wait for news.",
          "あなたは魚を釣った：{0}。私はまだ手袋しか釣れていません。でも、もうすぐ氷がはるから、知らせを待っていてね。"),
    ],
    "place": [
        T("letters.pen.ack.place.1", "Я представила твоё место: {0}. Закрыла глаза и представила, как там пахнет морем, только тёплым. Получилось почти.",
          "I imagined your place: {0}. I shut my eyes and imagined the smell of the sea there, only warm. I almost managed it.",
          "あなたの場所を思いうかべました：{0}。目をつぶって、そこの海のにおいを想像したの。あたたかい海のにおい。もう少しでわかりそうでした。"),
        T("letters.pen.ack.place.2", "Твоё место ({0}) теперь есть и на моей карте, на полях, со стрелкой.",
          "Your place ({0}) is on my map now too, in the margin, with an arrow.", "あなたの場所（{0}）は、もう私の地図の余白にも、矢印つきでのっています。"),
    ],
    "friend": [
        T("letters.pen.ack.friend.1", "Значит, у тебя тоже есть друг с крыльями: {0}! Капитан, когда я ему прочитала, каркнул два раза. Я думаю, это значит «уважаю».",
          "So you have a winged friend too: {0}! When I read it to Captain, he cawed twice. I think that means \"respect\".",
          "あなたにも翼のある友だちがいるのね：{0}！キャプテンに読んで聞かせたら、二回鳴きました。「尊敬する」って意味だと思う。"),
        T("letters.pen.ack.friend.2", "Твой друг, {0}, ест у тебя с ладони. Значит, у нас с тобой одинаковые руки: им доверяют.",
          "Your friend, {0}, eats from your palm. So you and I have the same kind of hands: they're trusted.",
          "あなたの友だち、{0}は、手のひらから食べるのね。私たちは同じ手を持っているんだね。信頼される手を。"),
    ],
    "none": [
        T("letters.pen.ack.none.1", "Ты пишешь, что пока ничего особенного. А мне кажется, у тебя каждый день особенный, просто ты привык.",
          "You write that nothing special has happened yet. But I think every day is special where you are: you're just used to it.",
          "まだ特別なことはないって書いてあったけど、そちらでは毎日が特別なんだと思う。あなたが慣れているだけで。"),
        T("letters.pen.ack.none.2", "Спасибо, что ответил, даже когда нечего рассказать. Это тоже важно: значит, письмо дошло.",
          "Thank you for answering even when there's nothing to tell. That matters too: it means the letter got there.",
          "話すことがなくても返事をくれて、ありがとう。それも大事なこと。手紙が届いたってわかるから。"),
    ],
}

PEN_ANSWERS = {
    "bird": T("letters.pen.answer.bird", "Я видел птицу: {0}. Подобрался совсем близко и долго смотрел в бинокль.",
              "I saw a bird: {0}. I crept up quite close and watched through the binoculars for a long time.",
              "鳥を見たよ：{0}。すぐ近くまで寄って、双眼鏡で長いこと見ていた。"),
    "fish": T("letters.pen.answer.fish", "Я поймал рыбу: {0}! Сам, на удочку, на пирсе.",
              "I caught a fish: {0}! Myself, with a rod, on the pier.", "魚を釣ったよ：{0}！自分で、釣りざおで、桟橋から。"),
    "place": T("letters.pen.answer.place", "Моё самое хорошее место: {0}. Тебе бы там понравилось.",
               "My best place: {0}. You'd like it there.", "僕のいちばんいい場所：{0}。きっと気に入ると思う。"),
    "friend": T("letters.pen.answer.friend", "У меня есть друг с крыльями: {0}. Он ест у меня с ладони и прилетает, когда я зову.",
                "I have a winged friend: {0}. It eats from my palm and comes when I call.", "翼のある友だちがいるよ：{0}。手のひらから食べて、呼ぶと飛んでくる。"),
    "none": T("letters.pen.answer.none", "Пока ничего особенного. Но я смотрю в оба и обязательно напишу.",
              "Nothing special yet. But I'm keeping my eyes open and will write for sure.", "まだ特別なことはないよ。でも、よく見ているから、必ずまた書くね。"),
}
