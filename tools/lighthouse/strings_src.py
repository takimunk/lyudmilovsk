"""Every word of the Mayak lighthouse (docs/lighthouse.md) in ru / en / ja: the keeper's name and lines, the hero's replies,
the prompts, the weather log's words, the quest lighthouse_paraffin, the paraffin item. tools/lighthouse/lighthouse.py
writes them to Content/Localization/lighthouse/strings_<lang>.json (never edit those by hand).

Spoken lines are line.<speaker>.<scene>.<n> (speaker vetrov_gordey; the hero's replies are line.hero.lh_<scene>.<n>);
Content/Dialogue/npc_dialogue.txt [Vetrov_gordey] strings them together. Japanese bubbles stay short.
"""

S = {}


def put(key, ru, en, ja):
    assert key not in S, key
    S[key] = (ru, en, ja)


# ---- the keeper ------------------------------------------------------------------------------------------------------
put("npc.vetrov_gordey.name", "Гордей Васильевич Ветров", "Gordey Vasilyevich Vetrov", "ゴルデイ・ヴァシリエヴィチ・ヴェトロフ")
put("npc.vetrov_gordey.role", "смотритель маяка «Маяк»", "keeper of the Mayak lighthouse", "マヤーク灯台の灯台守")

# the first talk of every day after the first ever (npc_dialogue "first" is said once: his greeting to a known face)
put("line.vetrov_gordey.first.1", "Ветров. Смотритель. По делу — говори. Без дела — смотри на море, оно тоже без дела.",
    "Vetrov. Keeper. If you've business, say it. If not, look at the sea. It hasn't any either.",
    "ヴェトロフだ。灯台守。用があるなら言え。ないなら海を見てろ。海も暇だ。")
put("line.hero.lh_first.2", "Я просто посмотреть.", "I only came to look.", "ちょっと見に来ただけです。")
put("line.vetrov_gordey.first.3", "Смотреть можно. Трогать — только перила.", "Looking's allowed. Touching: the railings only.",
    "見るのはいい。触っていいのは手すりだけだ。")

put("line.vetrov_gordey.lens.1", "Линзу делали в Париже, в девятьсот восьмом. Стеклу сто с лишним лет, а пыль на нём каждое утро новая.",
    "The lens was made in Paris in nineteen-oh-eight. The glass is over a hundred years old, and the dust on it is new every morning.",
    "レンズは1908年にパリで作られた。ガラスは百年以上前のものだが、埃は毎朝新しい。")
put("line.vetrov_gordey.log.1", "Журнал висит у будки, в жестяном ящике. Читать можно, писать нельзя. Почерк на этом берегу один — мой.",
    "The log hangs by the weather screen, in the tin box. You may read it, not write in it. There's one handwriting on this coast: mine.",
    "日誌は百葉箱の横のブリキの箱にある。読んでいいが書くな。この海岸の字は一種類だけ、俺のだ。")
put("line.vetrov_gordey.flash.1", "Огонь у нас белый, проблеск раз в десять секунд. Штурман по одному этому знает, что перед ним Людмиловск.",
    "Our light is white, one flash every ten seconds. A navigator knows from that alone it's Lyudmilovsk ahead.",
    "うちの灯は白、十秒に一回の閃光だ。航海士はそれだけでリュドミロフスクだと分かる。")
put("line.vetrov_gordey.fog.1", "В туман включаю ревун. Два гудка каждые полминуты. Чайки обижаются, суда — нет.",
    "In fog I switch on the horn. Two blasts every half minute. The gulls take offence. The ships don't.",
    "霧の日は霧笛を鳴らす。三十秒ごとに二回。カモメは怒るが、船は怒らない。")
put("line.vetrov_gordey.tower.1", "Наверх пускаю днём, с восьми до девяти вечера. Ночью наверху двое: я и огонь.",
    "Visitors go up by day, eight in the morning to nine at night. At night there are two of us up there: me and the light.",
    "上に上がれるのは朝八時から夜九時まで。夜、上にいるのは二人だけ。俺と灯だ。")
put("line.vetrov_gordey.town.1", "В город хожу по субботам. За керосином и за газетой. Остальное мне ветер приносит.",
    "I go to town on Saturdays. For paraffin and the paper. The wind brings me the rest.",
    "町へは土曜に行く。灯油と新聞を買いに。ほかのものは風が運んでくる。")
put("line.vetrov_gordey.weather.1", "Барометр не врёт, врут прогнозы. Падает стрелка с утра — к вечеру ставь чайник и окно не открывай.",
    "The barometer doesn't lie; forecasts do. If the needle falls in the morning, put the kettle on by evening and keep the window shut.",
    "気圧計は嘘をつかない。嘘をつくのは予報だ。朝から針が下がったら、夕方には湯を沸かして窓を閉めておけ。")
put("line.vetrov_gordey.current.1", "Видишь полосу на воде, светлее остальной? Течение. Сорок лет она там, а лоции всё никак не напишут.",
    "See that strip on the water, paler than the rest? A current. Forty years it's been there, and the pilot books still don't say so.",
    "水面の、ほかより明るい筋が見えるか。潮の流れだ。四十年そこにあるのに、水路誌にはまだ載っていない。")
put("line.vetrov_gordey.clock.1", "Механизм заводится гирей, как ходики, только гиря сорок кило. Каждые четыре часа. Вот тебе и вся зарядка.",
    "The clockwork runs on a weight, like a cuckoo clock, only the weight is forty kilos. Every four hours. That's all the exercise I need.",
    "回転機械は振り子時計みたいに重りで動く。ただし重りは四十キロ。四時間ごとに巻く。運動はそれで足りる。")
put("line.vetrov_gordey.navy.1", "На флоте я был сигнальщиком. Всю жизнь мигал фонарём другим. Теперь мигаю всем сразу.",
    "In the navy I was a signalman. All my life I flashed a lamp at someone. Now I flash at everyone at once.",
    "海軍では信号手だった。一生、誰かに向けて灯を点滅させてきた。今はみんなに向けてだ。")
put("line.vetrov_gordey.ships.1", "Последний раз огонь видели с парохода в позапрошлом году. Это ничего. Маяк горит не для тех, кто есть, а для того, кто будет.",
    "The last time a ship saw the light was the year before last. Never mind. A lighthouse doesn't burn for those who are there. It burns for whoever comes.",
    "最後に船がこの灯を見たのは一昨年だ。構わない。灯台は今いる者のためじゃなく、いつか来る者のために灯る。")
put("line.vetrov_gordey.sun_day.1", "Днём маяк не нужен. Днём я обычный человек с тряпкой.",
    "By day nobody needs a lighthouse. By day I'm an ordinary man with a rag.", "昼間、灯台はいらない。昼の俺はただの雑巾持ちだ。")
put("line.vetrov_gordey.sun_low.1", "Скоро зажигать. Солнце садится, я поднимаюсь. Так и живём, по очереди.",
    "Time to light up soon. The sun goes down, I go up. That's how we live: taking turns.",
    "もうすぐ点灯だ。日が沈めば俺が上る。交代で暮らしてるのさ。")
put("line.vetrov_gordey.sun_night.1", "Слышишь, тикает? Это линза идёт. Пока тикает, можно спать всему берегу. Кроме меня.",
    "Hear it ticking? That's the lens going round. While it ticks, the whole coast can sleep. Except me.",
    "カチカチ聞こえるか。レンズが回ってる音だ。これが鳴ってる間は海岸じゅう眠っていい。俺以外はな。")
put("line.vetrov_gordey.sun_cloudy.1", "Облачно. Огонь сегодня увидят раньше обычного. Мне так спокойнее.",
    "Cloudy. They'll see the light earlier than usual today. I feel easier that way.", "曇りだな。今日はいつもより早く灯が見える。そのほうが落ち着く。")

# ---- the errand: paraffin and the paper (quest lighthouse_paraffin) -------------------------------------------------------
put("line.vetrov_gordey.paraffin_ask.1", "Стой. Ты в город ходишь? Вот и хорошо. У меня керосин на донышке, а нога вторую неделю не своя.",
    "Hold on. You go into town? Good. My paraffin's down to the bottom of the can, and my leg's not been my own for a fortnight.",
    "待て。町へは行くか。ならいい。灯油が底をつきかけてるのに、足がこの二週間言うことをきかん。")
put("line.hero.lh_paraffin_ask.2", "А огонь разве не электрический?", "Isn't the light electric?", "灯って電気じゃないんですか。")
put("line.vetrov_gordey.paraffin_ask.3", "Электрический. А если провод оборвёт? Запасная лампа керосиновая, и она должна быть полной. Это закон, а не прихоть.",
    "It is. And if a storm takes the line down? The spare lamp burns paraffin, and it has to be full. That's the rule, not a whim.",
    "電気だ。だが嵐で電線が切れたら？予備のランプは灯油で、満タンでなきゃならん。決まりだ、気まぐれじゃない。")
put("line.vetrov_gordey.paraffin_ask.4", "Бидон керосина из хозяйственного и свежую газету из киоска. Деньги отдам, не бойся.",
    "A can of paraffin from the hardware shop and today's paper from a kiosk. I'll pay you back, don't worry.",
    "金物屋で灯油を一缶、それと売店で今日の新聞を。金は返す、心配するな。")
put("line.vetrov_gordey.paraffin_hint.1", "Керосин в хозяйственном, газета в киоске. Маяк подождёт, а погода — нет.",
    "Paraffin at the hardware shop, the paper at a kiosk. The lighthouse can wait. The weather can't.",
    "灯油は金物屋、新聞は売店だ。灯台は待てるが、天気は待たない。")
put("line.vetrov_gordey.paraffin_hint.2", "Если в хозяйственном нет, спроси у продавщицы, она мне всегда оставляет. Скажи — для маяка.",
    "If they're out at the hardware shop, ask the woman at the counter. She always keeps some for me. Say it's for the lighthouse.",
    "金物屋になければ、店のおばさんに聞け。いつも取っておいてくれる。灯台用だと言えばいい。")
put("line.vetrov_gordey.paraffin_bring.1", "Принёс? Давай сюда. Ровно бидон, и газета ещё пахнет краской. Хорошо.",
    "You've brought it? Hand it over. A full can, and the paper still smells of ink. Good.",
    "持ってきたか。よこせ。一缶きっちり、新聞はまだインクの匂いがする。よし。")
put("line.vetrov_gordey.paraffin_bring.2", "Вот деньги. И вот что: наверх теперь заходи когда хочешь. Хоть ночью. Только меня не буди, если я на вахте сплю.",
    "Here's the money. And another thing: come up whenever you like now. At night too. Only don't wake me if I'm asleep on watch.",
    "ほら、金だ。それとな、これからはいつでも上がってこい。夜でもいい。ただ当直で寝てたら起こすなよ。")
put("line.hero.lh_paraffin_bring.3", "А разве на вахте спят?", "Do keepers sleep on watch?", "当直中に寝るんですか。")
put("line.vetrov_gordey.paraffin_bring.4", "Не спят. Поэтому и не буди.", "They don't. That's why you mustn't wake me.", "寝ない。だから起こすな。")
put("line.vetrov_gordey.paraffin_part.1", "Половину принёс, вижу. Керосин без газеты — это работа без новостей. Неси и вторую половину.",
    "Half of it, I see. Paraffin without the paper is work without news. Bring the other half.",
    "半分だな。新聞のない灯油は、ニュースのない仕事だ。残りも持ってこい。")
put("line.vetrov_gordey.paraffin_done.1", "Огонь полный, газета прочитана. В журнале так и записал: «Помощник был». Без имени, уж извини, место кончилось.",
    "The lamp's full, the paper's read. I wrote it in the log: 'A helper came.' No name, sorry; I ran out of room.",
    "ランプは満タン、新聞は読んだ。日誌に「手伝いが来た」と書いておいた。名前はない、欄が足りなくてな。")

put("quest.lighthouse_paraffin.title", "Керосин для маяка", "Paraffin for the lighthouse", "灯台の灯油")
put("quest.lighthouse_paraffin.desc", "Смотритель маяка на мысу за станцией «Маяк» не может дойти до города: у него кончается керосин для запасной лампы.",
    "The keeper of the lighthouse on the headland beyond Mayak station can't walk to town, and his spare lamp is running out of paraffin.",
    "マヤーク駅の先の岬にいる灯台守は町まで歩けない。予備ランプの灯油が切れかけている。")
put("quest.lighthouse_paraffin.ask.log", "Смотритель маяка, Ветров, просит купить ему керосин и свежую газету: у него болит нога, а запасная лампа должна быть полной.",
    "The keeper, Vetrov, asked me to buy him paraffin and today's paper: his leg hurts, and the spare lamp must be kept full.",
    "灯台守のヴェトロフに、灯油と今日の新聞を買ってきてくれと頼まれた。足が痛むのに、予備ランプは満タンにしておかなきゃならないらしい。")
put("quest.lighthouse_paraffin.talk_keeper.obj", "Поговорить со смотрителем маяка", "Talk to the lighthouse keeper", "灯台守と話す")
put("quest.lighthouse_paraffin.fetch.log", "Керосин продают в хозяйственном, газету в киоске. Потом обратно на мыс, к маяку.",
    "Paraffin is sold at the hardware shop, the paper at a kiosk. Then back out to the headland.",
    "灯油は金物屋、新聞は売店で売っている。そのあと岬の灯台へ戻る。")
put("quest.lighthouse_paraffin.get_paraffin.obj", "Купить бидон керосина в хозяйственном", "Buy a can of paraffin at the hardware shop", "金物屋で灯油を一缶買う")
put("quest.lighthouse_paraffin.get_paper.obj", "Купить свежую газету", "Buy today's paper", "今日の新聞を買う")
put("quest.lighthouse_paraffin.bring.log", "Всё есть. Теперь отнести смотрителю, пока не стемнело.", "I have it all. Now to take it to the keeper before dark.",
    "全部そろった。暗くなる前に灯台守に届けよう。")
put("quest.lighthouse_paraffin.bring_paraffin.obj", "Отдать керосин Ветрову", "Give the paraffin to Vetrov", "ヴェトロフに灯油を渡す")
put("quest.lighthouse_paraffin.bring_paper.obj", "Отдать газету Ветрову", "Give the paper to Vetrov", "ヴェトロフに新聞を渡す")
put("quest.lighthouse_paraffin.done", "Ветров пустил меня на маяк в любое время. Даже ночью, когда наверху только он и огонь.",
    "Vetrov says I can go up the lighthouse any time. Even at night, when there's only him and the light up there.",
    "ヴェトロフはいつでも灯台に上がっていいと言ってくれた。夜でも。上にいるのが彼と灯だけの時間でも。")

put("item.paraffin.name", "Бидон керосина", "Can of paraffin", "灯油缶")
put("item.paraffin.desc", "Литровый жестяной бидон с керосином, крышка на цепочке. Пахнет так, что сразу вспоминаешь дачу.",
    "A one-litre tin can of paraffin, its cap on a little chain. The smell takes you straight back to the dacha.",
    "一リットルのブリキの灯油缶。ふたは鎖でつながっている。匂いだけで夏の別荘を思い出す。")

# ---- prompts and toasts ----------------------------------------------------------------------------------------------
put("ui.lh.up", "E — подняться на галерею", "E — climb up to the gallery", "E：回廊に上がる")
put("ui.lh.down", "E — спуститься", "E — go down", "E：下りる")
put("ui.lh.locked", "E — дверь маяка", "E — the lighthouse door", "E：灯台の扉")
put("ui.lh.log", "E — читать вахтенный журнал", "E — read the weather log", "E：観測日誌を読む")
put("lh.tower.locked", "Заперто. На двери мелом: «Посетители с 8 до 21. Смотритель».",
    "Locked. Chalked on the door: 'Visitors 8 to 21. The Keeper.'", "鍵がかかっている。扉にチョークで「見学8時〜21時　灯台守」。")

# ---- the weather log -------------------------------------------------------------------------------------------------
put("lh.log.head", "Вахтенный журнал маяка «Маяк». Огонь белый проблесковый, период 10 с. Ревун: 2 гудка, период 30 с.",
    "Watch log of the Mayak lighthouse. Light: white flashing, period 10 s. Horn: 2 blasts, period 30 s.",
    "マヤーク灯台 当直日誌。灯質：白色閃光 周期10秒。霧笛：2吹鳴 周期30秒。")
MONTHS = [("января", "January", "1月"), ("февраля", "February", "2月"), ("марта", "March", "3月"), ("апреля", "April", "4月"),
          ("мая", "May", "5月"), ("июня", "June", "6月"), ("июля", "July", "7月"), ("августа", "August", "8月"),
          ("сентября", "September", "9月"), ("октября", "October", "10月"), ("ноября", "November", "11月"),
          ("декабря", "December", "12月")]
for i, (ru, en, ja) in enumerate(MONTHS, 1):
    put("lh.log.date.%d" % i, "{0} %s" % ru, "%s {0}" % en, "%s{0}日" % ja)
# {0} hour, {1} sky, {2} wind from, {3} m/s, {4} hPa, {5} the pressure's way, {6} temperature, {7} visibility
put("lh.log.reading", "{0} — {1}. Ветер {2}, {3} м/с. {4} гПа, {5}. {6}°. Видимость {7}.",
    "{0} — {1}. Wind {2}, {3} m/s. {4} hPa, {5}. {6}°. Visibility {7}.",
    "{0}　{1}。風 {2} {3}m/s。{4}hPa、{5}。{6}°。視程 {7}。")
put("lh.log.no_reading", "Наблюдений ещё не было.", "No readings yet today.", "まだ観測なし。")
put("lh.log.lamp", "Огонь погашен в {0}, зажжён в {1}.", "Light out at {0}, lit at {1}.", "消灯 {0}、点灯 {1}。")
put("lh.log.lamp_out", "Огонь погашен в {0}.", "Light out at {0}.", "消灯 {0}。")
put("lh.log.horn", "Туман. Ревун работал {0} ч.", "Fog. Horn sounded {0} h.", "霧。霧笛 {0}時間。")
SKY = [("ясно", "clear", "快晴"), ("малооблачно", "fair", "晴れ"), ("облачно", "cloudy", "くもり"), ("пасмурно", "overcast", "本曇り"),
       ("морось", "drizzle", "霧雨"), ("дождь", "rain", "雨"), ("ливень", "heavy rain", "大雨"), ("мокрый снег", "sleet", "みぞれ"),
       ("снег", "snow", "雪"), ("гроза", "thunderstorm", "雷雨"), ("туман", "fog", "霧"), ("дымка", "haze", "もや")]
for i, (ru, en, ja) in enumerate(SKY):
    put("lh.log.sky.%d" % i, ru, en, ja)
WIND = [("северный", "N", "北"), ("северо-восточный", "NE", "北東"), ("восточный", "E", "東"), ("юго-восточный", "SE", "南東"),
        ("южный", "S", "南"), ("юго-западный", "SW", "南西"), ("западный", "W", "西"), ("северо-западный", "NW", "北西")]
for i, (ru, en, ja) in enumerate(WIND):
    put("lh.log.wind.%d" % i, ru, en, ja)
put("lh.log.tend.fall", "падает", "falling", "下降")
put("lh.log.tend.steady", "без изменений", "steady", "変化なし")
put("lh.log.tend.rise", "растёт", "rising", "上昇")
VIS = [("менее 200 м", "under 200 m", "200m未満"), ("до 1 км", "under 1 km", "1km未満"), ("1–4 км", "1–4 km", "1〜4km"),
       ("4–10 км", "4–10 km", "4〜10km"), ("более 10 км", "over 10 km", "10km以上")]
for i, (ru, en, ja) in enumerate(VIS):
    put("lh.log.vis.%d" % i, ru, en, ja)
REMARKS = {
    "gale.1": ("Шторм. Стёкла фонаря гудят, как струны. Механизм завёл дважды лишний раз.",
               "Gale. The lantern panes hum like strings. Wound the clockwork twice more than needed.",
               "暴風。灯室のガラスが弦のように鳴る。機械を余分に二度巻いた。"),
    "gale.2": ("Ветер сорвал ведро с крыльца. Ведро нашёл, крыльцо на месте.",
               "The wind took the bucket off the porch. Found the bucket. The porch is still here.",
               "風でバケツが玄関から飛んだ。バケツは見つけた。玄関は無事。"),
    "gale.3": ("Волна заходила на камни у мыса. Чайки сидят на земле, это к долгому.",
               "Waves breaking over the rocks at the point. The gulls sit on the ground: it'll last.",
               "岬の岩に波が打ち上げる。カモメが地面に座っている。長引くぞ。"),
    "thunder.1": ("Гроза прошла по морю южнее. Громоотвод проверил, цел.",
                  "A thunderstorm went by over the sea to the south. Checked the lightning rod: sound.",
                  "雷雨が南の海上を通過。避雷針を点検、異常なし。"),
    "thunder.2": ("Молния в воду за отмелью. Свет мигнул, лампа держала.",
                  "Lightning into the water beyond the shoal. The power flickered; the lamp held.",
                  "浅瀬の向こうの海に落雷。電気がちらついたがランプは持ちこたえた。"),
    "fog.1": ("Туман с моря, плотный. Ревун работал, чайки обиделись.",
              "Thick fog off the sea. The horn worked; the gulls took offence.", "海から濃霧。霧笛作動、カモメはご機嫌斜め。"),
    "fog.2": ("В тумане огня не видно и с крыльца. Значит, слушают ревун.",
              "In the fog you can't see the light even from the porch. So they listen to the horn.",
              "霧で玄関からも灯が見えない。だから霧笛を聞くんだ。"),
    "fog.3": ("Туман. Слышал чей-то мотор у мыса, ответил ревуном. Мотор ушёл вдоль берега.",
              "Fog. Heard an engine off the point, answered with the horn. The engine went off along the coast.",
              "霧。岬沖でエンジン音、霧笛で応答。エンジン音は岸沿いに去った。"),
    "ice.1": ("У берега лёд. Море молчит, только скрипит.", "Ice along the shore. The sea is quiet; it only creaks.",
              "岸に氷。海は黙り、きしむだけ。"),
    "ice.2": ("Лёд до самой отмели. Огонь горит для тех, кто придёт по весне.",
              "Ice out to the shoal. The light burns for whoever comes in spring.", "浅瀬まで氷。灯は春に来る者のために灯る。"),
    "frost.1": ("Мороз. Стёкла фонаря изнутри в узорах, протёр спиртом.",
                "Frost. The lantern panes patterned inside; wiped them with spirit.", "厳寒。灯室のガラスの内側に霜の模様、アルコールで拭く。"),
    "frost.2": ("Сильный мороз. Керосин в запасной лампе проверил, не загустел.",
                "Hard frost. Checked the paraffin in the spare lamp: not thickened.", "厳しい冷え込み。予備ランプの灯油を点検、凍結なし。"),
    "calm.1": ("Штиль, ясно. Огонь видно до самого горизонта, а смотреть некому.",
               "Calm and clear. You can see the light to the horizon, and nobody to see it.", "凪、快晴。灯は水平線まで見えるが、見る者はいない。"),
    "calm.2": ("Тихо. Слышно, как в городе лают собаки.", "Still. You can hear the dogs barking in town.",
               "静か。町で犬の吠える声が聞こえる。"),
    "calm.3": ("Звёзды. Насчитал три спутника и одну падающую. Желание записывать не положено.",
               "Stars. Counted three satellites and one shooter. Wishes are not to be entered in the log.",
               "星空。人工衛星三つ、流れ星一つ。願い事は日誌に書かない決まり。"),
    "plain.1": ("Происшествий нет.", "Nothing to report.", "異常なし。"),
    "plain.2": ("Линзу протёр. Механизм в порядке.", "Lens cleaned. Clockwork in order.", "レンズ清掃。機械異常なし。"),
    "plain.3": ("Приходили смотреть. Наверх поднимались двое, спускались двое.",
                "Visitors came to look. Two went up, two came down.", "見学あり。二人上がり、二人下りた。"),
    "plain.4": ("Покрасил перила на галерее. Вечером сохнут, утром трогать.",
                "Painted the gallery railings. Drying tonight; may be touched in the morning.", "回廊の手すりを塗装。今夜乾燥、触るのは朝から。"),
}
for k, (ru, en, ja) in REMARKS.items():
    put("lh.log.remark." + k, ru, en, ja)

# developer mode (F7): teleports, actions, the group (docs/dev_mode.md)
put("dev.group.lighthouse", "Маяк", "Lighthouse", "灯台")
put("dev.tp.lighthouse_base", "Маяк: у двери башни", "Lighthouse: the tower door", "灯台：塔の扉")
put("dev.tp.lighthouse_gallery", "Маяк: галерея", "Lighthouse: the gallery", "灯台：回廊")
put("dev.tp.lighthouse_log", "Маяк: метеобудка и журнал", "Lighthouse: weather screen and log", "灯台：百葉箱と日誌")
put("dev.tp.lighthouse_night_beach", "Маяк ночью с пляжа", "Lighthouse at night from the beach", "夜の灯台（浜辺から）")
put("dev.tp.lighthouse_night_town", "Маяк ночью с городского пирса", "Lighthouse at night from the town pier", "夜の灯台（町の桟橋から）")
put("dev.action.lighthouse.lamp", "Маяк: зажечь огонь", "Lighthouse: light the lamp", "灯台：灯をともす")
put("dev.action.lighthouse.fog", "Маяк: густой туман", "Lighthouse: thick fog", "灯台：濃霧")
