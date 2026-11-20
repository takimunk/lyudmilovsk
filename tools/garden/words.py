"""The words of the allotment behind the commune house (backlog item 9, "An allotment"), ru / en / ja. Russian is the source.

Every entry is T(key, ru, en, ja), the same as tools/friends/content.py. The hero rents a bed from the house committee,
digs, sows, waters from the rain barrel, weeds, pulls the film down on cold nights; radish, dill, strawberries,
potatoes, tomatoes and sunflowers grow by the game's real calendar and weather. The harvest goes to the pocket, to the
market's control scales, or (six strawberries) to the neighbour Маарья Бернтсен, who cooks a jar of jam for the market's
contest. Calm: no meters, no numbers about growth, only words. The structure is tools/garden/data.py;
tools/garden/build_garden.py writes the string tables (the neighbour's bubbles become line.garden.*).

House style (localization skill): the hero is a teenage boy (ru agreement with him is masculine, ja 僕 in his diary);
Маарья is 77, kind, dry, a lifelong gardener: she says ты to him, ja soft and short. Names inside a sentence go after a
colon or stand alone, never into a Russian case ending. Placeholders {0} {1} {2} are the same in all three languages;
ja uses full-width ！？：「」. Bubbles (line.*): ru and en at most 100 characters, ja at most 48.
ICONS: one English sentence per item for its icon cell (localization/icon_grids_city.json style): no text on anything.
"""

STRINGS = {}
ICONS = {}


def T(key, ru, en, ja):
    assert key not in STRINGS, key
    STRINGS[key] = {"ru": ru, "en": en, "ja": ja}
    return key


# --- the pocket items --------------------------------------------------------------------------------------------------

T("item.spade.name", "Лопата", "Spade", "スコップ")
T("item.spade.desc", "Штыковая лопата с отполированным ладонями черенком. Земля за забором коммуны мягкая, копать одно удовольствие.",
  "A digging spade, its handle worn smooth by hands. The soil behind the commune house is soft: digging is a pleasure.",
  "柄が手でつるつるになった剣先スコップ。コミューン住宅の裏の土はやわらかくて、耕すのが楽しい。")
T("item.watering_can.name", "Лейка", "Watering can", "じょうろ")
T("item.watering_can.desc", "Жестяная лейка с дырчатым носиком. Набирается у дождевой бочки, а поёт, как маленький дождь.",
  "A tin watering can with a rose on the spout. You fill it at the rain barrel, and it sounds like a little shower.",
  "はす口のついたブリキのじょうろ。雨水のたるで水をくむ。小さな雨みたいな音がする。")
T("item.garden_compost.name", "Компост", "Compost", "たい肥")
T("item.garden_compost.desc", "Небольшой мешочек компоста из «Хозтоваров». Пахнет лесом после дождя. Вскопай его в грядку.",
  "A small sack of compost from the hardware shop. It smells of a wood after rain. Dig it into the bed.",
  "金物屋で買った小さなたい肥の袋。雨上がりの森のにおいがする。畑にすきこもう。")
T("item.seeds_radish.name", "Семена редиса", "Radish seeds", "ラディッシュの種")
T("item.seeds_radish.desc", "Бумажный пакетик с картинкой: красная редиска с хвостиком. Редис торопится, его можно ждать всего пару дней.",
  "A paper packet with a picture of a red radish on it. Radishes are in a hurry: you only wait a few days.",
  "赤いラディッシュの絵がついた紙袋。ラディッシュはせっかちで、待つのはほんの数日だ。")
T("item.seeds_dill.name", "Семена укропа", "Dill seeds", "ディルの種")
T("item.seeds_dill.desc", "Пакетик укропа. Срежешь — отрастёт снова, и так до трёх раз за лето.",
  "A packet of dill. Cut it and it grows back, up to three times a summer.",
  "ディルの種の袋。切ってもまた生えてくる。ひと夏に三回まで。")
T("item.seeds_sunflower.name", "Семена подсолнуха", "Sunflower seeds", "ヒマワリの種")
T("item.seeds_sunflower.desc", "Крупные полосатые семечки для посадки, не для лузганья. Хотя одно-то можно.",
  "Big striped seeds for sowing, not for nibbling. Though one won't hurt.",
  "まく用の大きなしま模様の種。食べる用ではない。でも、一つくらいなら。")
T("item.seed_potatoes.name", "Семенной картофель", "Seed potatoes", "種いも")
T("item.seed_potatoes.desc", "Сетка мелких картофелин с бледными ростками. Каждая просится в землю.",
  "A string bag of small potatoes with pale sprouts. Each one is asking to go in the ground.",
  "白い芽の出た小さなじゃがいもの網袋。どれも土に入りたがっている。")
T("item.seedling_tomato.name", "Рассада помидоров", "Tomato seedlings", "トマトの苗")
T("item.seedling_tomato.desc", "Кустики в бумажных стаканчиках, с рынка. Листья пахнут летом, если их потереть.",
  "Little plants in paper cups, from the market. Rub a leaf and it smells of summer.",
  "市場で買った紙コップの苗。葉をこすると夏のにおいがする。")
T("item.seedling_strawberry.name", "Рассада клубники", "Strawberry runners", "イチゴの苗")
T("item.seedling_strawberry.desc", "Усы клубники с корешками, в стаканчике земли. Посадишь раз — будет плодоносить годами.",
  "Strawberry runners with roots, in a cup of soil. Plant them once and they fruit for years.",
  "根のついたイチゴのランナー。一度植えれば、何年も実をつける。")
T("item.radish.name", "Редиска", "Radishes", "ラディッシュ")
T("item.radish.desc", "Пучок редиски со своей грядки. Хрустит и чуть щиплет язык.",
  "A bunch of radishes from your own bed. Crunchy, with a little bite.",
  "自分の畑のラディッシュ一束。シャキッとして、少しピリッとする。")
T("item.dill.name", "Пучок укропа", "Bunch of dill", "ディルの束")
T("item.dill.desc", "Свежий укроп. Пахнет так, что хочется варёной картошки.",
  "Fresh dill. It smells so good you start wanting boiled potatoes.",
  "とれたてのディル。においをかぐと、ゆでたじゃがいもが食べたくなる。")
T("item.potatoes.name", "Картошка", "Potatoes", "じゃがいも")
T("item.potatoes.desc", "Молодая картошка, ещё в земле. Кожица слезает от одного прикосновения.",
  "New potatoes, still earthy. The skin comes off at a touch.",
  "まだ土のついた新じゃが。皮はさわるだけでむける。")
T("item.tomatoes.name", "Помидоры", "Tomatoes", "トマト")
T("item.tomatoes.desc", "Тёплые от солнца помидоры. С солью и чёрным хлебом лучше не придумаешь.",
  "Tomatoes still warm from the sun. With salt and black bread, nothing better.",
  "日なたであたたまったトマト。塩と黒パンがあれば、もう何もいらない。")
T("item.strawberries.name", "Клубника", "Strawberries", "イチゴ")
T("item.strawberries.desc", "Горсть клубники с грядки. Самая сладкая — та, что пряталась под листом.",
  "A handful of strawberries from the bed. The sweetest one was hiding under a leaf.",
  "畑のイチゴひとつかみ。いちばん甘いのは、葉の下にかくれていたやつ。")
T("item.jam_strawberry.name", "Банка клубничного варенья", "Jar of strawberry jam", "イチゴジャムのびん")
T("item.jam_strawberry.desc", "Варенье соседки по грядке, из твоей клубники. Крышка обвязана марлей, на бумажке написано от руки: «Клубника».",
  "Jam made by your neighbour from your strawberries. Gauze tied over the lid, a handwritten paper label: \"Strawberry\".",
  "となりの畑のおばあさんが君のイチゴで煮たジャム。ふたにガーゼ、手書きのラベルに「イチゴ」。")

# --- the crops as a status line names them -------------------------------------------------------------------------------

T("garden.crop.radish", "Редис", "Radish", "ラディッシュ")
T("garden.crop.dill", "Укроп", "Dill", "ディル")
T("garden.crop.strawberry", "Клубника", "Strawberries", "イチゴ")
T("garden.crop.potato", "Картофель", "Potatoes", "じゃがいも")
T("garden.crop.tomato", "Помидоры", "Tomatoes", "トマト")
T("garden.crop.sunflower", "Подсолнух", "Sunflower", "ヒマワリ")

# --- the stages: "{crop}: {stage}." ------------------------------------------------------------------------------------

T("garden.stage.0", "посеяно, пока ничего не видно", "sown, nothing showing yet", "まいたばかり、まだ何も見えない")
T("garden.stage.1", "проклюнулись всходы", "the first shoots are up", "芽が出てきた")
T("garden.stage.2", "молодые кустики", "young plants", "若い苗")
T("garden.stage.3", "подросло, вот-вот зацветёт", "grown, about to flower", "大きく育って、花が咲きそう")
T("garden.stage.4", "поспело", "ripe", "実った")
T("garden.stage.spoilt", "перестояло", "gone over", "とりごろを過ぎた")
T("garden.stage.dead", "погибло", "lost", "枯れてしまった")

T("garden.soil.dry", "Земля сухая.", "The soil is dry.", "土がかわいている。")
T("garden.soil.damp", "Земля влажная.", "The soil is damp.", "土はしっとりしている。")
T("garden.soil.wet", "Земля мокрая.", "The soil is wet.", "土がぬれている。")
T("garden.weeds.some", "Пробиваются сорняки.", "A few weeds are coming up.", "雑草が出はじめた。")
T("garden.weeds.many", "Всё заросло сорняками.", "Weeds everywhere.", "雑草だらけだ。")
T("garden.cover.down", "Плёнка опущена.", "The film is down.", "ビニールをかけてある。")

T("garden.look.grass", "Заросший кусок грядки. Сначала вскопать.", "An overgrown patch of the bed. It needs digging first.",
  "草ぼうぼうの区画。まずは耕さないと。")
T("garden.look.dug", "Вскопанная земля. Можно сеять.", "Dug soil, ready for sowing.", "耕した土。種をまける。")
T("garden.look.weedy", "Вскопано, но снова лезут сорняки.", "Dug, but the weeds are back.", "耕したけれど、また雑草が生えてきた。")
T("garden.look.crop", "{0}: {1}. {2}", "{0}: {1}. {2}", "{0}：{1}。{2}")

# --- prompts ---------------------------------------------------------------------------------------------------------

T("garden.prompt.dig", "E — вскопать", "E — dig", "E：耕す")
T("garden.prompt.sow", "E — посеять", "E — sow", "E：種をまく")
T("garden.prompt.water", "E — полить", "E — water", "E：水をやる")
T("garden.prompt.weed", "E — прополоть", "E — weed", "E：草をとる")
T("garden.prompt.pick", "E — собрать: {0}", "E — pick: {0}", "E：{0}を収穫する")
T("garden.prompt.clear", "E — выдернуть засохшее", "E — pull out the dead plants", "E：枯れた株をぬく")
T("garden.prompt.cover", "E — опустить плёнку", "E — pull the film down", "E：ビニールをかける")
T("garden.prompt.uncover", "E — скатать плёнку", "E — roll the film up", "E：ビニールを巻き上げる")
T("garden.prompt.fill", "E — набрать воды в лейку", "E — fill the watering can", "E：じょうろに水をくむ")
T("garden.prompt.rent", "E — взять свободную грядку у домкома ({0} руб.)", "E — rent the free bed from the house committee ({0} roubles)",
  "E：住宅委員会から空いた畑を借りる（{0}ルーブル）")
T("garden.prompt.no_spade", "Нужна лопата. Продаётся в «Хозтоварах».", "You need a spade. The hardware shop sells them.",
  "スコップがいる。金物屋で売っている。")
T("garden.prompt.no_seeds", "Сеять нечего. Семена — в «Хозтоварах» и на рынке.", "Nothing to sow. Seeds are at the hardware shop and the market.",
  "まくものがない。種は金物屋と市場にある。")
T("garden.prompt.no_can", "Нужна лейка. Продаётся в «Хозтоварах».", "You need a watering can. The hardware shop sells them.",
  "じょうろがいる。金物屋で売っている。")
T("garden.prompt.can_empty", "Лейка пустая. Набери воды у бочки.", "The can is empty. Fill it at the barrel.",
  "じょうろが空だ。たるで水をくもう。")
T("garden.prompt.barrel_ice", "Вода в бочке замёрзла.", "The water in the barrel has frozen over.", "たるの水がこおっている。")
T("garden.prompt.board", "E — прочитать объявление домкома", "E — read the committee's notice", "E：住宅委員会のお知らせを読む")
T("garden.prompt.jam_ask", "E — попросить сварить варенье: {0}", "E — ask for jam from your strawberries: {0}",
  "E：イチゴでジャムを頼む：{0}")
T("garden.prompt.contest", "E — сдать банку на конкурс варенья", "E — hand the jar in for the jam contest", "E：ジャムをコンテストに出す")
T("garden.prompt.sell", "E — сдать урожай на весы ({0} шт.) за {1} руб.", "E — hand in the harvest ({0} items) for {1} roubles",
  "E：収穫（{0}点）を検量はかりに出す：{1}ルーブル")
T("garden.prompt.scales_closed", "Весы закрыты. Откроются в {0}:00.", "The scales are closed. They open at {0}:00.",
  "検量はかりは閉まっている。{0}時に開く。")
T("garden.prompt.jam_wait", "Варенье ещё варится.", "The jam is still cooking.", "ジャムはまだ煮ているところ。")
T("garden.prompt.look", "{0}", "{0}", "{0}")

T("garden.sow.title", "Что посеять?", "What to sow?", "何をまこう？")
T("garden.sow.keys", "1–9 — посеять   Esc — назад", "1–9 — sow   Esc — back", "1〜9：まく　Esc：もどる")
T("garden.sow.none", "В карманах нет ни семян, ни рассады.", "No seeds or seedlings in my pockets.", "ポケットに種も苗もない。")

# --- toasts ----------------------------------------------------------------------------------------------------------

T("garden.toast.rented", "Грядка твоя. Домком записал тебя в тетрадку.", "The bed is yours. The house committee wrote you into its book.",
  "畑は君のものだ。住宅委員会のノートに名前が書かれた。")
T("garden.toast.no_money", "Не хватает денег: аренда {0} руб.", "Not enough money: the rent is {0} roubles.",
  "お金が足りない。借り賃は{0}ルーブル。")
T("garden.toast.dug", "Вскопано.", "Dug.", "耕した。")
T("garden.toast.sown", "Посеяно: {0}.", "Sown: {0}.", "まいた：{0}。")
T("garden.toast.watered", "Полито.", "Watered.", "水をやった。")
T("garden.toast.weeded", "Прополото.", "Weeded.", "草をとった。")
T("garden.toast.picked", "Собрано: {0} ({1}).", "Picked: {0} ({1}).", "収穫：{0}（{1}）。")
T("garden.toast.cleared", "Место расчищено.", "The spot is cleared.", "場所をあけた。")
T("garden.toast.covered", "Плёнка опущена. Ночью будет теплее.", "The film is down. It'll be warmer tonight.",
  "ビニールをかけた。夜は少しあたたかい。")
T("garden.toast.uncovered", "Плёнка скатана.", "The film is rolled up.", "ビニールを巻き上げた。")
T("garden.toast.filled", "Лейка полная.", "The can is full.", "じょうろがいっぱいになった。")
T("garden.toast.dead", "Не уберёг: {0}. Бывает и так.", "This one didn't make it: {0}. It happens.",
  "だめになってしまった：{0}。そういうこともある。")
T("garden.toast.spoilt", "Перестояло: {0}.", "Gone over: {0}.", "とりごろを過ぎた：{0}。")
T("garden.toast.jam_given", "Клубника отдана. Варенье сварит: {0}.", "Strawberries handed over. {0} will cook the jam.",
  "イチゴをわたした。ジャムを煮てくれる人：{0}。")
T("garden.toast.jam_ready", "На скамейке банка варенья и записка. От кого: {0}.", "A jar of jam on the bench, with a note. From: {0}.",
  "ベンチにジャムのびんとメモ。差出人：{0}。")
T("garden.toast.contest_entered", "Банка принята на конкурс. Жюри — в воскресенье.", "The jar is entered. The jury meets on Sunday.",
  "ジャムを受けつけてもらった。審査は日曜日。")
T("garden.toast.contest_already", "Твоя банка уже на конкурсе.", "Your jar is already in the contest.", "君のジャムはもう出してある。")
T("garden.toast.sold", "Урожай сдан: {0} руб.", "Harvest handed in: {0} roubles.", "収穫を引き取ってもらった：{0}ルーブル。")
T("garden.toast.friend_seeds", "Подарок от: {0}. {1}.", "A present from {0}: {1}.", "{0}からのおくりもの：{1}。")
T("garden.toast.compost", "Компост вскопан в грядку.", "The compost is dug into the bed.", "たい肥を畑にすきこんだ。")

T("garden.quality.1", "обычный", "ordinary", "ふつう")
T("garden.quality.2", "хороший", "good", "上等")
T("garden.quality.3", "отличный", "excellent", "極上")

# --- the diary -------------------------------------------------------------------------------------------------------

T("garden.diary.rented", "Взял грядку за домом-коммуной. Теперь у меня есть свой клочок земли.",
  "I rented a bed behind the commune house. Now I have a patch of earth of my own.",
  "コミューン住宅の裏に畑を借りた。僕だけの小さな土地ができた。")
T("garden.diary.first_harvest", "Первый урожай: {0}. Сам вырастил, сам и съем.", "My first harvest: {0}. I grew it myself, and I'll eat it myself.",
  "はじめての収穫：{0}。自分で育てて、自分で食べる。")
T("garden.diary.jam", "Соседка сварила варенье из моей клубники. Банка ещё тёплая.",
  "My neighbour cooked jam from my strawberries. The jar is still warm.",
  "となりのおばあさんが僕のイチゴでジャムを煮てくれた。びんがまだあたたかい。")
T("garden.diary.contest", "Конкурс варенья: {0}. Пахло клубникой на весь рынок.", "The jam contest: {0}. The whole market smelled of strawberries.",
  "ジャムのコンテスト：{0}。市場じゅうイチゴのにおいだった。")

# --- the committee's notice, the place ----------------------------------------------------------------------------------

T("garden.name", "Огороды дома-коммуны", "The commune house allotments", "コミューン住宅の家庭菜園")
T("garden.board.title", "Домовый комитет. Объявление", "House committee. Notice", "住宅委員会のお知らせ")
T("garden.board.text",
  "Сдаётся свободная грядка на сезон. Плата: {0} руб. Поливать только из бочки с дождевой водой. Собак на огороды не водить. "
  "Чужую клубнику не пробовать, даже «на спелость». Председатель огородной комиссии: М. Бернтсен.",
  "A free bed for rent for the season. Rent: {0} roubles. Water only from the rain barrel. No dogs in the allotments. "
  "Do not taste other people's strawberries, not even \"to check if they're ripe\". Chair of the allotment committee: M. Berntsen.",
  "空いた畑を一シーズン貸します。借り賃：{0}ルーブル。水やりは雨水のたるからのみ。犬の連れこみ禁止。"
  "よその畑のイチゴの「味見」も禁止。菜園委員長：M・ベルンツェン")

# --- the jury's letters ----------------------------------------------------------------------------------------------

T("garden.letter.from", "Жюри конкурса варенья, рынок", "The jam contest jury, the market", "市場ジャムコンテスト審査員一同")
T("garden.letter.contest.1",
  "Поздравляем! Ваше клубничное варенье заняло первое место. Смотрительница весов говорит, что оно густое как надо, "
  "продавщица мёда — что почти как мёд, а наш юный член жюри съел три ложки и попросил четвёртую. Премия: {0} руб., "
  "получить у весов.",
  "Congratulations! Your strawberry jam has taken first place. The keeper of the scales says it's just thick enough, the honey "
  "seller says it's nearly as good as honey, and our young juror ate three spoonfuls and asked for a fourth. Prize: {0} roubles, "
  "to be collected at the scales.",
  "おめでとうございます！あなたのイチゴジャムが一位になりました。はかり番は「ちょうどいい濃さ」、はちみつ売りは「はちみつに負けない」、"
  "子どもの審査員は三さじ食べて四さじ目をねだりました。賞金{0}ルーブルははかりのところで受け取ってください。")
T("garden.letter.contest.2",
  "Поздравляем! Ваше варенье — второе. Ягоды целые, сироп прозрачный; юный член жюри сказал, что первое было «чуть-чуть слаще», "
  "но вашу банку облизал дочиста. Премия: {0} руб., получить у весов.",
  "Congratulations! Your jam came second. Whole berries, clear syrup; our young juror said the winner was \"a tiny bit sweeter\", "
  "but licked your jar clean. Prize: {0} roubles, to be collected at the scales.",
  "おめでとうございます！あなたのジャムは二位です。実はまるごと、シロップはすきとおっていました。子どもの審査員は「一位がちょっとだけ甘い」"
  "と言いつつ、あなたのびんをきれいになめました。賞金{0}ルーブルははかりのところで。")
T("garden.letter.contest.3",
  "Поздравляем! Ваше варенье — третье. Продавщица мёда хвалила аромат, смотрительница весов — честный вес банки. Премия: {0} руб., "
  "получить у весов.",
  "Congratulations! Your jam came third. The honey seller praised its scent, the keeper of the scales the honest weight of the jar. "
  "Prize: {0} roubles, to be collected at the scales.",
  "おめでとうございます！あなたのジャムは三位です。はちみつ売りは香りを、はかり番はびんの正直な重さをほめていました。賞金{0}ルーブルははかりのところで。")
T("garden.letter.contest.none",
  "Спасибо за ваше варенье! В этот раз до призов не хватило совсем немного, но банку съели до донышка всем жюри. "
  "Приносите в следующий раз: клубника, говорят, на будущий год будет ещё лучше.",
  "Thank you for your jam! It fell just short of a prize this time, but the whole jury ate the jar down to the bottom. "
  "Do bring one next time: they say next year's strawberries will be even better.",
  "ジャムをありがとうございました！今回はあと一歩で入賞をのがしましたが、審査員みんなでびんの底まで食べました。"
  "来年もぜひ。イチゴはもっとよくなるそうです。")

T("dev.group.garden", "Огород", "Allotment", "家庭菜園")

# --- Маарья's bubbles (line.garden.*): ru/en <= 100, ja <= 48 ---------------------------------------------------------

T("garden.need.dig.1", "Грядку сначала вскопай. Земля любит, когда её разбудят.", "Dig the bed first. Soil likes to be woken up.",
  "まずは耕しなさい。土は起こしてもらうのが好きなのよ。")
T("garden.need.dig.2", "Травой заросло. Лопату в руки, и через полчаса не узнаешь.", "It's all grass. Take the spade: in half an hour you won't know it.",
  "草だらけね。スコップを持てば、すぐ見ちがえるわ。")
T("garden.need.sow.1", "Вскопал — сей, пока земля рыхлая. Редис для начала хорош.", "Dug? Then sow while it's loose. Radish is good to start with.",
  "耕したら、やわらかいうちにまきなさい。最初はラディッシュがいいわ。")
T("garden.need.sow.2", "Пустая земля скучает. Посади хоть укроп.", "Bare soil gets bored. Plant some dill at least.",
  "空いた土はたいくつするの。ディルでもまいてごらん。")
T("garden.need.water.1", "Земля у тебя сухая. Лейка у бочки, не стесняйся.", "Your soil's dry. The can's by the barrel, help yourself.",
  "土がかわいてるよ。じょうろはたるのそば、遠慮しないで。")
T("garden.need.water.2", "Полей под корень, а не по листьям. Листья и так не жалуются.", "Water the roots, not the leaves. The leaves don't complain anyway.",
  "葉っぱじゃなくて根もとにね。葉っぱは文句言わないから。")
T("garden.need.weed.1", "Сорняки полезли. Дёргай, пока маленькие, потом не справишься.", "Weeds are coming up. Pull them while they're small.",
  "雑草が出てきたね。小さいうちにぬいておきなさい。")
T("garden.need.weed.2", "У тебя лебеда растёт лучше всего остального. Не дело.", "Your goosefoot is doing better than anything else. That won't do.",
  "君の畑、アカザがいちばん元気ね。それはこまるわ。")
T("garden.need.cover.1", "Ночью обещают холод. Опусти плёнку, не ленись.", "It'll be cold tonight. Pull the film down, don't be lazy.",
  "今夜は冷えるって。ビニールをかけておきなさい。")
T("garden.need.cover.2", "Укрой грядку на ночь. Мои помидоры уже под плёнкой.", "Cover your bed for the night. My tomatoes are already tucked in.",
  "夜は畑にビニールを。うちのトマトはもうかけたよ。")
T("garden.need.uncover.1", "Солнце вышло, сними плёнку, а то всё сварится.", "The sun's out: roll the film up, or it'll all cook.",
  "日が出たよ。ビニールを上げないと、むれちゃう。")
T("garden.need.uncover.2", "Днём под плёнкой баня. Пусть подышат.", "It's a sauna under that film by day. Let them breathe.",
  "昼のビニールの中はサウナよ。息をさせてあげて。")
T("garden.need.pick.1", "У тебя поспело. Собирай, не жди, пока перестоит.", "Yours is ripe. Pick it before it goes over.",
  "実ってるよ。とりごろを逃さないで。")
T("garden.need.pick.2", "Вон, созрело. Первый урожай самый вкусный, проверено.", "Look, it's ripe. The first harvest tastes best, I promise.",
  "ほら、実ってる。最初の収穫がいちばんおいしいのよ。")
T("garden.need.clear.1", "Засохшее выдерни, не жалей. Место пригодится.", "Pull out the dead ones, don't be sorry. You'll need the space.",
  "枯れた株はぬいてしまいなさい。場所が空くから。")
T("garden.need.clear.2", "Не уберёг? Со всеми бывает. Расчисти и посади заново.", "Lost it? Happens to everyone. Clear it and plant again.",
  "だめだった？だれにでもあるわ。片づけて植えなおそう。")

T("garden.season.0.1", "Весной не торопись. Земля должна прогреться, ей виднее.", "Don't hurry in spring. The soil has to warm up; it knows best.",
  "春はあせらないで。土があたたまるのを待つの。")
T("garden.season.0.2", "Картошку сажают, когда зацветёт черёмуха. Так ещё моя мама говорила.", "Potatoes go in when the bird cherry flowers. My mother used to say so.",
  "じゃがいもはウワミズザクラが咲いたら。母の口ぐせよ。")
T("garden.season.0.3", "У моря весна поздняя. Зато лето потом доброе.", "Spring comes late by the sea. But then summer is kind.",
  "海辺の春はおそいの。そのかわり夏はやさしい。")
T("garden.season.1.1", "Летом поливай вечером, когда солнце ушло. Утром роса сама справится.", "In summer water in the evening, once the sun's gone. In the morning the dew does the work.",
  "夏の水やりは夕方に。朝は露がやってくれる。")
T("garden.season.1.2", "Клубника любит, когда её собирают. Чем чаще, тем щедрее.", "Strawberries like being picked. The more often, the more they give.",
  "イチゴはつんでもらうのが好き。つむほど実をつけるの。")
T("garden.season.1.3", "Укроп срезай сверху, корень оставь. Он ещё отрастёт.", "Cut the dill from the top, leave the root. It'll grow again.",
  "ディルは上だけ切って、根は残して。また生えるから。")
T("garden.season.2.1", "Осенью копай картошку в сухой день. Мокрая плохо лежит.", "Dig potatoes on a dry day in autumn. Wet ones don't keep.",
  "秋のいもほりは晴れた日に。ぬれたいもは長持ちしないの。")
T("garden.season.2.2", "С моря уже тянет холодом. Плёнку держи под рукой.", "There's a chill coming off the sea. Keep the film handy.",
  "海から冷たい風。ビニールは手もとにね。")
T("garden.season.2.3", "Клубнику на зиму укрой листьями. Ей под ними тепло.", "Tuck the strawberries in with leaves for winter. They're warm under there.",
  "冬にはイチゴを落ち葉でくるんで。あたたかいのよ。")
T("garden.season.3.1", "Зимой земля спит. И нам можно.", "In winter the soil sleeps. So can we.", "冬は土もお休み。私たちもね。")
T("garden.season.3.2", "Снег на грядке — лучшее одеяло. Не разгребай.", "Snow on the bed is the best blanket. Leave it.",
  "畑の雪はいちばんの毛布よ。どけないで。")
T("garden.season.3.3", "Зимой я читаю каталоги семян. Как романы, только с хорошим концом.", "In winter I read seed catalogues. Like novels, only with happy endings.",
  "冬は種のカタログを読むの。小説みたいなものよ、結末はいつもハッピーだけど。")
T("garden.frost.1", "Ночью будет заморозок. Опусти плёнку, а то утром плакать.", "Frost tonight. Pull the film down, or there'll be tears in the morning.",
  "今夜は霜がおりる。ビニールをかけないと、朝泣くよ。")
T("garden.frost.2", "Небо чистое, звёзды яркие — к морозу. Укрой грядку.", "Clear sky, bright stars: that means frost. Cover your bed.",
  "空が晴れて星がきれい。霜になるわ。畑をおおって。")
T("garden.hello.1", "Доброе утро, сосед! Я тут с пяти, а ты только проснулся.", "Morning, neighbour! I've been here since five, and you've only just woken up.",
  "おはよう、おとなりさん！私は五時からいるのに、君はいま起きたのね。")
T("garden.hello.2", "А, молодой огородник пришёл. Ну, здравствуй.", "Ah, the young gardener's here. Good morning.",
  "あら、若い畑仲間が来た。おはよう。")
T("garden.hello.3", "Утро какое, а? Самое время для грядок.", "What a morning, eh? Just the time for the beds.",
  "いい朝ね。畑にはちょうどいい時間。")
T("garden.jam.ask.1", "Давай свою клубнику. Вечером сварю, утром заберёшь.", "Give me your strawberries. I'll make the jam tonight, you'll have it by morning.",
  "イチゴをちょうだい。今夜煮て、朝にはできてるわ。")
T("garden.jam.ask.2", "Хорошая ягода. Сварю по маминому рецепту, без спешки.", "Good berries. I'll cook them my mother's way, no hurry.",
  "いいイチゴね。母のやり方で、ゆっくり煮るわ。")
T("garden.jam.ready.1", "Твоё варенье на скамейке. Пенку я сняла и съела, извини.", "Your jam's on the bench. I skimmed the foam and ate it, sorry.",
  "ジャムはベンチの上よ。泡はすくって私が食べちゃった、ごめんね。")
T("garden.jam.ready.2", "Варенье готово. Неси на конкурс, не стыдно.", "The jam's ready. Take it to the contest, it's nothing to be shy of.",
  "ジャムができたよ。コンテストに出しても恥ずかしくない。")
T("garden.praise.1", "Вот это урожай! Я в твои годы так не умела.", "Now that's a harvest! I couldn't do that at your age.",
  "いい収穫ね！君の年のころ、私はこうはいかなかった。")
T("garden.praise.2", "Хорошо растёт у тебя. Значит, рука лёгкая.", "Things grow well for you. You've got a light hand.",
  "よく育ってるね。君には育てる才能があるのよ。")
T("garden.gift.1", "Слышно, ты грядку взял? Вот, держи семян, у меня лишние.", "I hear you've got a bed? Here, have some seeds, I've got spares.",
  "畑を始めたんだって？種をどうぞ、余ってるから。")
T("garden.gift.2", "Это тебе на грядку. Пусть хорошо взойдёт.", "These are for your bed. May they come up well.",
  "畑にどうぞ。よく芽が出ますように。")

# --- icon cells (one English sentence per item, no text anywhere) ----------------------------------------------------

ICONS = {
    "spade": "a well-used Soviet digging spade with a pointed steel blade dusted with dark soil and a pale wooden handle worn smooth, lying diagonally",
    "watering_can": "a green-painted tin watering can with a long spout ending in a round perforated rose, a few scuffs on the paint",
    "garden_compost": "a small tied sack of coarse brown paper, its top folded open to show dark crumbly compost",
    "seeds_radish": "a plain paper seed packet with a printed picture of red round radishes with green tops and white tips, no letters",
    "seeds_dill": "a plain paper seed packet with a printed picture of feathery green dill and yellow umbrella flowers, no letters",
    "seeds_sunflower": "a plain paper seed packet with a printed picture of a big yellow sunflower head, a few striped seeds spilled beside it, no letters",
    "seed_potatoes": "a string net bag of small yellow-brown seed potatoes with short pale sprouts",
    "seedling_tomato": "a young tomato seedling with fuzzy serrated leaves in a plain brown paper cup of dark soil",
    "seedling_strawberry": "a small strawberry plant with three-lobed leaves and a trailing runner in a plain brown paper cup of soil",
    "radish": "a tied bunch of five glossy red radishes with green leafy tops and thin white tails",
    "dill": "a fresh bunch of feathery green dill tied with a thin strip of twine",
    "potatoes": "a small heap of six new potatoes with thin papery skins and a little dark earth still on them",
    "tomatoes": "four ripe red ribbed garden tomatoes with green star-shaped stalks, one with a drop of dew",
    "strawberries": "a handful of ripe red garden strawberries with green caps in a small birch-bark basket",
    "jam_strawberry": "a glass jar of dark red strawberry jam with whole berries visible, a square of white gauze tied over the lid with string, a blank paper label",
}

# --- added with the engine side: compost, the contest's place in the diary, developer mode ------------------------------
T("garden.prompt.compost", "E — закопать компост", "E — dig the compost in", "E：たい肥をすきこむ")
T("garden.place.1", "первое место", "first place", "一位")
T("garden.place.2", "второе место", "second place", "二位")
T("garden.place.3", "третье место", "third place", "三位")
T("garden.place.none", "без места, зато с благодарностью", "no place, but a thank-you", "入賞はなし、でもお礼の言葉")
T("dev.action.garden.kit", "Огород: грядка, лопата, лейка, семена", "Garden: the bed, a spade, a can, seeds", "畑：区画、スコップ、じょうろ、種")
T("dev.action.garden.grow_day", "Огород: прошли сутки", "Garden: a day goes by", "畑：一日たつ")
T("dev.action.garden.report", "Огород: отчёт в лог", "Garden: report to the log", "畑：ログに報告")
T("dev.tp.garden_bed", "Огород дома-коммуны", "The commune house allotment", "コミューン住宅の畑")
# the crops' names in the plant system (PlantSpeciesLibrary::DisplayName: plant.<species lower case>.name)
T("plant.gardenradish.name", "Редис", "Radish", "ラディッシュ")
T("plant.gardendill.name", "Укроп", "Dill", "ディル")
T("plant.gardenstrawberry.name", "Клубника садовая", "Garden strawberry", "イチゴ")
T("plant.gardenpotato.name", "Картофель", "Potato", "ジャガイモ")
T("plant.gardentomato.name", "Томат", "Tomato", "トマト")
T("plant.gardensunflower.name", "Подсолнечник", "Sunflower", "ヒマワリ")
