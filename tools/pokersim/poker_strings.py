#!/usr/bin/env python3
"""The words of the ship's poker (docs/ship.md "Poker"): every line in Russian (the original), English and Japanese, side
by side. Writes unreal/Lyudmilovsk/Content/Localization/poker/strings_<lang>.json; never edit those by hand.

    python3 tools/pokersim/poker_strings.py           # writes the three tables
    python3 tools/pokersim/poker_strings.py --check   # only checks that they are up to date (the suite)

Tells: poker.tell.<regular>.<strong|weak|bluff>.<0..5>, six a state; table talk: poker.talk.<regular>.<0..7>
(Private/Cruise/PokerModel.h: TellLines, TalkLines). A tell is one short line on the panel: keep it under ~90 letters.
"""
import collections
import json
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Localization", "poker", "strings_%s.json")
LANGS = ("ru", "en", "ja")

T = collections.OrderedDict()


def s(key, ru, en, ja):
    assert key not in T, key
    T[key] = (ru, en, ja)


# ---------------------------------------------------------------------------------------------------------------- the panel
s("poker.title", "Покер", "Poker", "ポーカー")
s("poker.title.table", "Покер · {0}", "Poker · {0}", "ポーカー · {0}")
s("poker.table.matches", "На интерес", "For matches", "マッチ棒で")
s("poker.table.roubles", "По маленькой", "Small stakes", "小さく賭けて")
s("poker.choose", "Во что играем?", "What shall we play for?", "何を賭ける？")
s("poker.choose.matches", "1 — на интерес: на спички, по {0} каждому. Проиграть нечего, кроме чести.",
  "1 — for matches: {0} each. Nothing to lose but your honour.",
  "1：マッチ棒で。一人{0}本。失うものは面目だけ。")
s("poker.choose.roubles", "2 — по маленькой: вход {0} руб., анте {1} руб., ставки {2} и {3} руб.",
  "2 — small stakes: {0} roubles to sit in, ante {1}, bets {2} and {3}.",
  "2：小さく賭けて。席料{0}ルーブル、アンティ{1}、賭け金{2}と{3}。")
s("poker.choose.cap", "      Больше {0} руб. за вечер здесь проиграть не дают.", "      Nobody is let lose more than {0} roubles a night here.",
  "　　　一晩の負けは{0}ルーブルまでと決まっている。")
s("poker.choose.keys", "1 / 2 — выбрать     H — правила     Esc — уйти", "1 / 2 — choose     H — rules     Esc — leave",
  "1／2：選ぶ　　H：ルール　　Esc：やめる")
s("poker.closed", "Стол собирается вечером, после десяти.", "The table gathers in the evening, after ten.",
  "テーブルに人が集まるのは夜、十時を過ぎてから。")
s("poker.cap", "На сегодня хватит: больше {0} руб. за вечер здесь не проигрывают. Можно на интерес.",
  "Enough for tonight: nobody loses more than {0} roubles an evening here. Matches are fine.",
  "今夜はここまで。ここでは一晩に{0}ルーブルより多くは負けられない。マッチ棒ならどうぞ。")
s("poker.no_money", "На вход нужно {0} руб. Можно на интерес, на спички.",
  "It takes {0} roubles to sit in. You can play for matches.", "席料は{0}ルーブル。マッチ棒でなら遊べる。")
s("poker.nobody", "Играть уже не с кем: стол разошёлся.", "Nobody left to play: the table has broken up.",
  "もう相手がいない。テーブルはお開きだ。")
s("poker.help.0", "Покер с обменом: правила", "Draw poker: the rules", "ドローポーカーのルール")
s("poker.help.1", "Все ставят анте и получают по пять карт. Круг ставок: пас, чек или уравнять, поставить или поднять.",
  "Everybody antes and gets five cards. A round of betting: fold, check or call, bet or raise.",
  "全員がアンティを出し、五枚ずつ配られる。賭けの一巡：降りる、チェックかコール、賭けるかレイズ。")
s("poker.help.2", "Потом каждый меняет до трёх карт, второй круг ставок (ставки вдвое крупнее) и вскрытие.",
  "Then everyone changes up to three cards, a second round (the bets are doubled) and the showdown.",
  "それから各自三枚まで交換し、二巡目（賭け金は倍）のあと、手を開く。")
s("poker.help.3", "По старшинству: пара, две пары, тройка, стрит (пять подряд), флеш (пять одной масти),",
  "From the lowest: a pair, two pair, three of a kind, a straight (five in a row), a flush (five of a suit),",
  "弱い順に：ワンペア、ツーペア、スリーカード、ストレート（五枚続き）、フラッシュ（同じマーク五枚）、")
s("poker.help.4", "фулл-хаус (тройка и пара), каре (четыре одинаковых), стрит-флеш, флеш-рояль.",
  "a full house (three and a pair), four of a kind, a straight flush, a royal flush.",
  "フルハウス（三枚と二枚）、フォーカード、ストレートフラッシュ、ロイヤルフラッシュ。")
s("poker.help.5", "Кому нечем ответить на ставку, идёт на все и спорит только за свою часть банка.",
  "Whoever cannot cover a bet goes all in and plays for his share of the pot only.",
  "賭け金が足りない人は全部賭け、ポットの自分の分だけを争う。")
s("poker.help.6", "Смотри не только в карты, но и на людей: каждый выдаёт себя по-своему.",
  "Watch the people, not only your cards: everyone gives himself away in his own fashion.",
  "カードだけでなく人も見ること。誰にでも、その人なりの癖がある。")
s("poker.you", "Ты", "You", "きみ")
s("poker.pot", "Банк: {0}", "Pot: {0}", "ポット：{0}")
s("poker.stack", "У тебя: {0}", "You have: {0}", "手持ち：{0}")
s("poker.bet", "ставка {0}", "bet {0}", "賭け {0}")
s("poker.dealer", "сдаёт", "dealer", "親")
s("poker.unit.matches", "{0} сп.", "{0} m.", "{0}本")
s("poker.unit.roubles", "{0} руб.", "{0} r.", "{0}ルーブル")
s("poker.hand_no", "Раздача {0}", "Hand {0}", "第{0}ゲーム")
s("poker.keys.bet", "2 — чек     3 — поставить {0}     H — правила", "2 — check     3 — bet {0}     H — rules",
  "2：チェック　　3：{0} 賭ける　　H：ルール")
s("poker.keys.bet_call", "1 — пас     2 — уравнять {0}     3 — поднять до {1}", "1 — fold     2 — call {0}     3 — raise to {1}",
  "1：降りる　　2：{0} でコール　　3：{1} までレイズ")
s("poker.keys.draw", "A / D — выбрать карту     W — отметить на обмен (до трёх)     E — менять",
  "A / D — pick a card     W — mark it to change (up to three)     E — change",
  "A／D：カードを選ぶ　　W：交換の印（三枚まで）　　E：交換する")
s("poker.keys.next", "E — следующая раздача     S / Esc — встать из-за стола", "E — next hand     S / Esc — get up from the table",
  "E：次のゲーム　　S／Esc：席を立つ")
s("poker.keys.wait", "Ходят другие…  (E — быстрее)", "The others are playing…  (E — faster)", "ほかの人の番…（E：早送り）")
s("poker.keys.common", "H — правила     Esc — встать из-за стола", "H — rules     Esc — get up from the table", "H：ルール　　Esc：席を立つ")
s("poker.keys.deal_watch", "Q — следить за руками сдающего", "Q — watch the dealer's hands", "Q：親の手もとを見張る")
s("poker.keys.over", "E / Esc — встать из-за стола", "E / Esc — get up from the table", "E／Esc：席を立つ")

s("poker.act.check", "{0}: чек.", "{0} checks.", "{0}：チェック。")
s("poker.act.bet", "{0}: ставлю {1}", "{0} bets {1}", "{0}：{1} 賭ける。")
s("poker.act.call", "{0}: уравниваю ({1}).", "{0} calls ({1}).", "{0}：コール（{1}）。")
s("poker.act.raise", "{0}: поднимаю! ({1})", "{0} raises! ({1})", "{0}：レイズ！（{1}）")
s("poker.act.fold", "{0}: пас.", "{0} folds.", "{0}：降りる。")
s("poker.act.allin", "{0} — на все!", "{0} is all in!", "{0}は全部賭けた！")
s("poker.act.draw", "{0} меняет карт: {1}.", "{0} changes {1}.", "{0}は{1}枚交換。")
s("poker.act.pat", "{0} не меняет ни одной.", "{0} stands pat.", "{0}は一枚も替えない。")
s("poker.act.you.check", "Ты: чек.", "You check.", "きみ：チェック。")
s("poker.act.you.bet", "Ты ставишь {1}", "You bet {1}", "きみは {1} 賭けた。")
s("poker.act.you.call", "Ты уравниваешь ({1}).", "You call ({1}).", "きみはコール（{1}）。")
s("poker.act.you.raise", "Ты поднимаешь ({1}).", "You raise ({1}).", "きみはレイズ（{1}）。")
s("poker.act.you.fold", "Ты пасуешь.", "You fold.", "きみは降りた。")
s("poker.act.you.draw", "Ты меняешь карт: {1}.", "You change {1}.", "きみは{1}枚交換。")
s("poker.act.you.pat", "Ты не меняешь ни одной.", "You stand pat.", "きみは一枚も替えない。")

s("poker.log.deal", "Сдаёт {0}.", "{0} deals.", "親は{0}。")
s("poker.log.you_deal", "Сдаёшь ты.", "You deal.", "きみが親だ。")
s("poker.log.ante", "Анте: по {0}", "Ante: {0} each", "アンティ：一人 {0}。")
s("poker.log.show", "{0}: {1}.", "{0}: {1}.", "{0}：{1}。")
s("poker.log.you_show", "У тебя {0}.", "You have {0}.", "きみの手は{0}。")
s("poker.log.win", "{0} забирает банк: {1}", "{0} takes the pot: {1}", "{0}がポットを取った：{1}。")
s("poker.log.you_win", "Банк твой: {0}!", "The pot is yours: {0}!", "ポットはきみのものだ：{0}！")
s("poker.log.split", "Банк делят.", "The pot is split.", "ポットは山分け。")
s("poker.log.left", "{0} выходит из игры: ставить больше нечего.", "{0} is out of the game: nothing left to bet.", "{0}は勝負を降りた。もう賭けるものがない。")
s("poker.log.goes", "{0} встаёт из-за стола: пора.", "{0} gets up from the table: time to go.", "{0}は席を立った。もう時間だ。")
s("poker.log.joins", "{0} подсаживается к столу.", "{0} sits in.", "{0}がテーブルに加わった。")
s("poker.log.says", "{0}: {1}", "{0}: {1}", "{0}：{1}")
s("poker.log.your_turn", "Твоё слово.", "Your call.", "きみの番だ。")
s("poker.log.your_draw", "Обмен: отметь карты, которые сбрасываешь.", "The draw: mark the cards you throw away.",
  "交換だ。捨てるカードに印をつけよう。")
s("poker.mark.out", "ушёл", "gone", "退席")
s("poker.mark.fold", "пас", "folded", "降りた")
s("poker.mark.allin", "на все", "all in", "全賭け")
s("poker.mark.drew", "обмен: {0}", "changed {0}", "{0}枚交換")

s("poker.sharp.suspect", "Опять у Альберта после обмена полный дом. Который раз за вечер…",
  "Albert's draw has come out a full house again. How many times tonight…",
  "アルベルトは交換でまたフルハウス。今夜これで何度目だ…。")
s("poker.sharp.suspect2", "Посмотреть бы на его руки, когда он сдаёт.", "If only one could watch his hands while he deals.",
  "彼が配るとき、手もとを見ていられたら。")
s("poker.sharp.caught", "Есть! Альберт ногтем метит уголок туза.", "Got him! Albert is nicking the corner of an ace with his nail.",
  "見えた！アルベルトがエースの角に爪で印をつけている。")
s("poker.sharp.caught2", "«Инженер из Риги» бледнеет и бочком выходит из салона. Раздача отменяется, ставки возвращают.",
  "The \"engineer from Riga\" turns pale and sidles out of the saloon. The hand is void, the antes go back.",
  "「リガの技師」は青ざめ、そっとサロンを出ていく。このゲームは無効、アンティは返される。")
s("poker.sharp.missed", "Ты не сводишь глаз с рук Альберта. Сдаёт чисто — на этот раз.",
  "You keep your eyes on Albert's hands. A clean deal — this time.", "アルベルトの手から目を離さない。今回は——きれいな配り方だ。")
s("poker.sharp.clean", "Ты следишь за руками. {0}: сдача чистая.", "You watch the hands. {0}: a clean deal.", "手もとを見張る。{0}：きれいな配り方だ。")
s("poker.sharp.not_now", "Сейчас смотреть не на что: следить надо, пока сдают.",
  "Nothing to see now: the moment to watch is while the cards are dealt.", "今は見るものがない。見張るならカードを配っている間だ。")

s("poker.end.left", "Ты встаёшь из-за стола.", "You get up from the table.", "きみは席を立った。")
s("poker.end.broke", "Играть больше не на что.", "Nothing left to play with.", "もう賭けるものがない。")
s("poker.end.empty", "Играть больше не с кем: все разошлись.", "Nobody left to play: everyone has gone.", "もう相手がいない。みんな引き上げた。")
s("poker.end.hands", "«На сегодня всё», — колоду убирают в коробку. Спокойной ночи.",
  "\"That will do for tonight\" — the deck goes back in its box. Good night.", "「今夜はここまで」——カードは箱にしまわれた。おやすみ。")
s("poker.end.clock", "Второй час ночи. Боцман гасит лампу над столом: отбой.",
  "Past one in the morning. The bosun puts out the lamp over the table: lights out.", "夜中の一時過ぎ。甲板長がテーブルの上の灯りを消す。消灯だ。")
s("poker.end.up", "Вечер в плюсе: +{0} руб.", "Up on the evening: +{0} roubles.", "今夜は勝ち：＋{0}ルーブル。")
s("poker.end.down", "Вечер в минусе: −{0} руб.", "Down on the evening: −{0} roubles.", "今夜は負け：−{0}ルーブル。")
s("poker.end.even", "При своих.", "Broke even.", "差し引きゼロ。")
s("poker.end.matches", "Спички возвращаешь в коробок: было {0}, стало {1}.", "The matches go back in the box: you had {0}, you leave with {1}.",
  "マッチ棒を箱に戻す。{0}本が{1}本になった。")

s("poker.hand.high", "старшая карта", "high card", "ハイカード")
s("poker.hand.pair", "пара", "a pair", "ワンペア")
s("poker.hand.two_pair", "две пары", "two pair", "ツーペア")
s("poker.hand.three", "тройка", "three of a kind", "スリーカード")
s("poker.hand.straight", "стрит", "a straight", "ストレート")
s("poker.hand.flush", "флеш", "a flush", "フラッシュ")
s("poker.hand.full_house", "фулл-хаус", "a full house", "フルハウス")
s("poker.hand.four", "каре", "four of a kind", "フォーカード")
s("poker.hand.straight_flush", "стрит-флеш", "a straight flush", "ストレートフラッシュ")
s("poker.hand.royal", "флеш-рояль", "a royal flush", "ロイヤルフラッシュ")

s("dev.action.poker.open", "Покер: сесть за стол (все в сборе)", "Poker: sit at the table (everyone in)", "ポーカー：席につく（全員そろい）")
s("dev.action.poker.sharp", "Покер: режим шулера (квест)", "Poker: the card sharp's mode (quest)", "ポーカー：いかさま師モード（クエスト）")

# -------------------------------------------------------------------------------------------------------------- the regulars
# The names are a fallback: npc.ship_<id>.name of the people's tables wins when it exists.
s("poker.name.ship_albert", "Альберт", "Albert", "アルベルト")
s("poker.name.ship_givi", "Гиви", "Givi", "ギヴィ")
s("poker.name.ship_vera", "Вера Игнатьевна", "Vera Ignatyevna", "ヴェーラ先生")
s("poker.name.ship_taras", "Тарас", "Taras", "タラス")
s("poker.name.ship_blum", "Блюм", "Blum", "ブリューム")
s("poker.name.ship_lev", "Лев Маркович", "Lev Markovich", "レフ・マルコヴィチ")

s("poker.style.ship_albert", "«Инженер из Риги». Играет редко и крупно. По лицу почти ничего не прочтёшь.",
  "The \"engineer from Riga\". Plays few hands, and big. His face tells you next to nothing.",
  "「リガの技師」。めったに勝負しないが、出るときは大きい。顔からはほとんど何も読めない。")
s("poker.style.ship_givi", "Бармен, после смены. Играет почти любую карту. Чем хуже карта, тем длиннее история.",
  "The barman, off his shift. Plays almost any hand. The worse the cards, the longer the story.",
  "仕事上がりのバーテンダー。ほとんどどんな手でも勝負する。手が悪いほど、話が長くなる。")
s("poker.style.ship_vera", "Учительница географии. Осторожна и всё замечает.",
  "A geography teacher. Careful, and misses nothing.", "地理の先生。慎重で、何ひとつ見逃さない。")
s("poker.style.ship_taras", "Боцман. Смелый. Блефует часто, а врать не умеет.",
  "The bosun. Bold. Bluffs a lot and cannot lie to save his life.", "甲板長。大胆。よくはったりをかけるが、うそは下手だ。")
s("poker.style.ship_blum", "Пианист. Играет по слуху: то гениально, то мимо нот.",
  "The pianist. Plays by ear: now a stroke of genius, now a wrong note.", "ピアニスト。耳で弾くように賭ける。天才的なときも、音を外すときもある。")
s("poker.style.ship_lev", "Радист. Считает шансы до третьего знака.",
  "The radio operator. Works the odds out to the third decimal.", "通信士。確率を小数第三位まで計算する。")

TELLS = {
    "ship_albert": {
        "strong": [
            ("Альберт кладёт карты рубашкой вверх и больше в них не заглядывает.", "Albert lays his cards face down and does not look at them again.", "アルベルトはカードを伏せて置き、もう見ようともしない。"),
            ("Альберт поправляет манжету и смотрит на банк, а не на тебя.", "Albert straightens a cuff and looks at the pot, not at you.", "アルベルトは袖口を直し、きみではなくポットを見ている。"),
            ("Альберт ставит ровной стопкой, не торопясь.", "Albert puts his bet down in a neat stack, in no hurry.", "アルベルトは急がず、きちんとそろえて賭け金を置く。"),
            ("Альберт чуть улыбается и просит принести боржоми.", "Albert smiles faintly and asks for a mineral water.", "アルベルトはかすかに笑い、ミネラルウォーターを頼む。"),
            ("Альберт постукивает ногтем по рубашке верхней карты.", "Albert taps a fingernail on the back of his top card.", "アルベルトは一番上のカードの背を爪でこつこつ叩く。"),
            ("Альберт откидывается на спинку стула: «Ну-с, молодой человек?»", "Albert leans back in his chair: \"Well, young man?\"", "アルベルトは椅子の背にもたれる。「さて、お若い方？」"),
        ],
        "weak": [
            ("Альберт долго разглядывает свои карты, будто чертёж.", "Albert studies his cards for a long time, like a blueprint.", "アルベルトは設計図でも見るように、長いこと手札を眺めている。"),
            ("Альберт вздыхает: «Не идёт карта инженеру».", "Albert sighs: \"The cards don't love an engineer.\"", "アルベルトはため息をつく。「技師にはカードが来ないものでね」"),
            ("Альберт молча крутит на пальце перстень.", "Albert turns the ring on his finger and says nothing.", "アルベルトは黙って指輪をくるくる回している。"),
            ("Альберт пересчитывает свою стопку.", "Albert counts his stack over.", "アルベルトは自分の持ち分を数え直している。"),
            ("Альберт рассеянно смотрит в тёмный иллюминатор.", "Albert gazes absently at the dark porthole.", "アルベルトはぼんやりと暗い舷窓を見ている。"),
            ("Альберт трёт переносицу: «В Риге так не сдают».", "Albert rubs the bridge of his nose: \"They don't deal like this in Riga.\"", "アルベルトは鼻筋をこする。「リガではこんな配り方はしませんよ」"),
        ],
        "bluff": [
            ("Альберт ставит быстро, почти небрежно, и сразу закуривает.", "Albert bets fast, almost carelessly, and lights up at once.", "アルベルトは無造作なほど素早く賭け、すぐに煙草に火をつける。"),
            ("Альберт смотрит тебе прямо в глаза и не моргает.", "Albert looks you straight in the eye and does not blink.", "アルベルトはまばたきもせず、きみの目をまっすぐ見ている。"),
            ("Альберт вдруг становится очень любезен: «Ваше слово, прошу».", "Albert is suddenly very polite: \"Your call, if you please.\"", "アルベルトは急にていねいになる。「どうぞ、あなたの番です」"),
            ("Альберт одним пальцем двигает ставку на середину стола.", "Albert pushes his bet to the middle of the table with one finger.", "アルベルトは指一本で賭け金をテーブルの真ん中へ押し出す。"),
            ("Альберт насвистывает что-то из оперетты.", "Albert whistles something from an operetta.", "アルベルトはオペレッタの一節を口笛で吹いている。"),
            ("Альберт улыбается одними губами.", "Albert smiles with his lips only.", "アルベルトは口もとだけで笑っている。"),
        ],
    },
    "ship_givi": {
        "strong": [
            ("Гиви замолкает на полуслове и смотрит в карты.", "Givi stops in mid-sentence and looks at his cards.", "ギヴィは話の途中で黙りこみ、手札を見つめる。"),
            ("Гиви придвигает стул поближе к столу.", "Givi pulls his chair closer to the table.", "ギヴィは椅子をテーブルに引き寄せる。"),
            ("Гиви перестаёт шутить и аккуратно складывает карты.", "Givi stops joking and squares his cards carefully.", "ギヴィは冗談をやめ、カードをていねいにそろえる。"),
            ("Гиви говорит коротко: «Играю».", "Givi says just one word: \"In.\"", "ギヴィは短く言う。「やる」"),
            ("У Гиви подрагивают усы: он очень старается не улыбаться.", "Givi's moustache twitches: he is trying very hard not to smile.", "ギヴィの口ひげがぴくぴくしている。笑うまいと必死だ。"),
            ("Гиви накрывает карты ладонью, как блюдце с чаевыми.", "Givi covers his cards with his palm, like a saucer of tips.", "ギヴィはチップの小皿でも守るように、手札を手のひらで覆う。"),
        ],
        "weak": [
            ("Гиви рассказывает про Батуми и не смотрит в карты.", "Givi is telling a story about Batumi and not looking at his cards.", "ギヴィはバトゥミの話をしていて、手札を見もしない。"),
            ("Гиви вспоминает, как его дядя проиграл в нарды осла.", "Givi recalls how his uncle lost a donkey at backgammon.", "ギヴィは、おじさんがバックギャモンでロバを取られた話を始める。"),
            ("Гиви объясняет, каким должен быть настоящий кофе по-турецки.", "Givi explains what real Turkish coffee ought to be.", "ギヴィは本物のトルココーヒーとは何かを説明している。"),
            ("Гиви спрашивает, все ли пробовали его лимонад с тархуном.", "Givi asks whether everyone has tried his tarragon lemonade.", "ギヴィは、タラゴンのレモネードをみんな飲んだかと聞いて回る。"),
            ("Гиви заводит историю про шторм у Поти — третью за вечер.", "Givi starts on the storm off Poti — his third story tonight.", "ギヴィはポチ沖の嵐の話を始める。今夜三つ目の話だ。"),
            ("Гиви громче всех смеётся собственной шутке.", "Givi laughs at his own joke louder than anyone.", "ギヴィは自分の冗談に、誰よりも大きな声で笑う。"),
        ],
        "bluff": [
            ("Гиви поднимает тост за смелых и ставит, не договорив.", "Givi proposes a toast to the brave and bets before he has finished it.", "ギヴィは「勇者に乾杯」と言いかけ、言い終わらないうちに賭ける。"),
            ("Гиви рассказывает анекдот, а сам косится, кто как смотрит.", "Givi tells a joke, watching sideways who looks how.", "ギヴィは小話をしながら、みんなの顔色を横目でうかがっている。"),
            ("Гиви ставит и тут же спрашивает, не принести ли кому чаю.", "Givi bets and at once asks if anyone would like some tea.", "ギヴィは賭けるなり、誰かお茶はいらないかと聞く。"),
            ("Гиви клянётся мамой, что карта у него — во какая!", "Givi swears on his mother that his hand is this good!", "ギヴィは母親に誓って「こんなにいい手だ！」と言い張る。"),
            ("Гиви говорит слишком быстро и забывает, с чего начал.", "Givi talks too fast and forgets where he began.", "ギヴィは早口になり、何の話だったか自分で忘れている。"),
            ("Гиви хлопает ладонью по столу: «Э, была не была!»", "Givi slaps the table: \"Eh, here goes nothing!\"", "ギヴィはテーブルをぱんと叩く。「ええい、ままよ！」"),
        ],
    },
    "ship_vera": {
        "strong": [
            ("Вера Игнатьевна снимает очки и кладёт их рядом с картами.", "Vera Ignatyevna takes off her glasses and lays them by her cards.", "ヴェーラ先生は眼鏡を外し、手札の横に置く。"),
            ("Вера Игнатьевна выпрямляет спину, как у доски.", "Vera Ignatyevna straightens her back, as at the blackboard.", "ヴェーラ先生は黒板の前に立つときのように背筋を伸ばす。"),
            ("Вера Игнатьевна смотрит на тебя поверх очков: «Ну что ж».", "Vera Ignatyevna looks at you over her glasses: \"Well then.\"", "ヴェーラ先生は眼鏡ごしにきみを見る。「さて、と」"),
            ("Вера Игнатьевна ставит сразу, не пересчитывая.", "Vera Ignatyevna bets at once, without counting.", "ヴェーラ先生は数え直しもせず、すぐに賭ける。"),
            ("Вера Игнатьевна складывает карты ровным веером, как контурные.", "Vera Ignatyevna fans her cards as evenly as outline maps.", "ヴェーラ先生は白地図をそろえるように、手札をきれいな扇にする。"),
            ("Вера Игнатьевна чуть заметно кивает сама себе.", "Vera Ignatyevna gives herself the smallest of nods.", "ヴェーラ先生は自分にだけわかるほど小さくうなずく。"),
        ],
        "weak": [
            ("Вера Игнатьевна поджимает губы и раскладывает карты по порядку.", "Vera Ignatyevna purses her lips and puts her cards in order.", "ヴェーラ先生は口を結び、手札を順番に並べ直す。"),
            ("Вера Игнатьевна вздыхает: «Двойка мне за такую раздачу».", "Vera Ignatyevna sighs: \"A bottom mark for a deal like this.\"", "ヴェーラ先生はため息をつく。「こんな手じゃ落第点ね」"),
            ("Вера Игнатьевна долго протирает очки краем платка.", "Vera Ignatyevna spends a long time wiping her glasses on her shawl.", "ヴェーラ先生はショールの端で、長いこと眼鏡をふいている。"),
            ("Вера Игнатьевна шёпотом пересчитывает банк.", "Vera Ignatyevna counts the pot in a whisper.", "ヴェーラ先生は小声でポットを数えている。"),
            ("Вера Игнатьевна поправляет шаль и смотрит на часы.", "Vera Ignatyevna adjusts her shawl and looks at the clock.", "ヴェーラ先生はショールを直し、時計に目をやる。"),
            ("Вера Игнатьевна говорит «посмотрим, посмотрим» без всякой уверенности.", "Vera Ignatyevna says \"we shall see, we shall see\" with no conviction at all.", "ヴェーラ先生は「まあ、見てみましょう」と言うが、まるで自信がなさそうだ。"),
        ],
        "bluff": [
            ("Вера Игнатьевна ставит строгим голосом, будто объявляет контрольную.", "Vera Ignatyevna bets in a stern voice, as if announcing a test.", "ヴェーラ先生は抜き打ちテストを告げるような厳しい声で賭ける。"),
            ("Вера Игнатьевна вдруг спрашивает, какая столица у Парагвая.", "Vera Ignatyevna suddenly asks what the capital of Paraguay is.", "ヴェーラ先生はいきなり「パラグアイの首都は？」と聞く。"),
            ("Вера Игнатьевна слишком старательно выравнивает свою ставку.", "Vera Ignatyevna lines her bet up a little too carefully.", "ヴェーラ先生は賭け金を、少しばかりていねいすぎるほどそろえる。"),
            ("Вера Игнатьевна смотрит не на тебя, а куда-то в угол салона.", "Vera Ignatyevna is looking not at you but into a corner of the saloon.", "ヴェーラ先生はきみではなく、サロンの隅のほうを見ている。"),
            ("Вера Игнатьевна теребит дужку очков.", "Vera Ignatyevna fidgets with the arm of her glasses.", "ヴェーラ先生は眼鏡のつるをいじっている。"),
            ("У Веры Игнатьевны розовеют щёки.", "Vera Ignatyevna's cheeks turn pink.", "ヴェーラ先生の頬がほんのり赤くなる。"),
        ],
    },
    "ship_taras": {
        "strong": [
            ("Тарас гудит: «Ну, держитесь за леера!»", "Taras booms: \"Hold on to the rails, all hands!\"", "タラスがどら声で言う。「さあ、手すりにつかまってろ！」"),
            ("Тарас потирает ладони, большие, как лопаты.", "Taras rubs his hands, each as big as a shovel.", "タラスはシャベルのように大きな手をこすり合わせる。"),
            ("Тарас довольно крякает и подкручивает ус.", "Taras grunts with pleasure and twirls his moustache.", "タラスは満足そうにうなり、口ひげをひねる。"),
            ("Тарас ставит и спокойно откидывается назад: стул скрипит.", "Taras bets and leans back calmly: the chair creaks.", "タラスは賭けて、ゆったりと後ろにもたれる。椅子がきしむ。"),
            ("Тарас весело подмигивает тебе.", "Taras gives you a cheerful wink.", "タラスは陽気にきみに片目をつぶってみせる。"),
            ("Тарас мурлычет «Раскинулось море широко».", "Taras hums an old sailors' song under his breath.", "タラスは古い船乗りの歌を鼻歌でうたっている。"),
        ],
        "weak": [
            ("Тарас чешет в затылке и сопит.", "Taras scratches the back of his head and breathes heavily.", "タラスは頭の後ろをかき、鼻を鳴らす。"),
            ("Тарас ворчит: «Не карта, а швабра».", "Taras grumbles: \"That's not a hand, that's a deck mop.\"", "タラスがぼやく。「こりゃカードじゃねえ、デッキブラシだ」"),
            ("Тарас хмуро вяжет узел на обрывке шкерта.", "Taras glumly ties a knot in a scrap of line.", "タラスはむっつりと、細引きの切れ端に結び目を作っている。"),
            ("Тарас заглядывает в карты ещё раз: вдруг поменялись.", "Taras looks at his cards again, in case they have changed.", "タラスはもう一度手札をのぞく。変わっているかもしれないと思って。"),
            ("Тарас отодвигает карты и смотрит в подволок.", "Taras pushes his cards away and looks up at the deckhead.", "タラスは手札を押しやり、天井を見上げる。"),
            ("Тарас бурчит: «Ладно, поглядим, чего там».", "Taras mutters: \"All right, let's see what's what.\"", "タラスがぶつぶつ言う。「まあいい、どうなるか見てやる」"),
        ],
        "bluff": [
            ("Тарас ставит так громко, что вздрагивает стол.", "Taras bets so loudly that the table jumps.", "タラスの賭け方があまりに荒っぽくて、テーブルが揺れる。"),
            ("У Тараса краснеют уши.", "Taras's ears are turning red.", "タラスの耳が赤くなっていく。"),
            ("Тарас говорит: «У меня тут ого-го!» — и не знает, куда деть руки.", "Taras says \"I've got a whopper here!\" and does not know what to do with his hands.", "タラスは「こっちはすげえ手だぞ！」と言うが、手のやり場に困っている。"),
            ("Тарас смотрит в сторону и свистит мимо нот.", "Taras looks away and whistles out of tune.", "タラスはそっぽを向いて、調子外れの口笛を吹く。"),
            ("Тарас кашляет в кулак и торопит: «Ну, ходи, ходи».", "Taras coughs into his fist and hurries you: \"Go on, go on.\"", "タラスはこぶしに咳をして急かす。「ほら、早くしろ」"),
            ("Тарас сжимает карты так, что они гнутся.", "Taras grips his cards so hard that they bend.", "タラスはカードが曲がるほど強く握りしめている。"),
        ],
    },
    "ship_blum": {
        "strong": [
            ("Блюм барабанит пальцами по столу мажорный аккорд.", "Blum drums a major chord on the table with his fingers.", "ブリュームは指でテーブルに長調の和音を叩く。"),
            ("Блюм напевает что-то быстрое, в ритме фокстрота.", "Blum hums something quick, in foxtrot time.", "ブリュームはフォックストロットのリズムで、何か速い曲を口ずさむ。"),
            ("Блюм улыбается картам, как старым знакомым.", "Blum smiles at his cards as at old friends.", "ブリュームは古い友だちに会ったように手札にほほえむ。"),
            ("Блюм ставит не глядя: «Пусть будет форте».", "Blum bets without looking: \"Let it be forte.\"", "ブリュームは見もせずに賭ける。「ここはフォルテで」"),
            ("Блюм поправляет бабочку.", "Blum straightens his bow tie.", "ブリュームは蝶ネクタイを直す。"),
            ("Блюм говорит неизвестно кому: «Маэстро, туш!»", "Blum says to nobody in particular: \"A fanfare, maestro!\"", "ブリュームは誰にともなく言う。「マエストロ、ファンファーレを！」"),
        ],
        "weak": [
            ("Пальцы Блюма играют на краю стола что-то в миноре.", "Blum's fingers play something in a minor key on the edge of the table.", "ブリュームの指がテーブルの端で、短調の何かを弾いている。"),
            ("Блюм смотрит в карты, как в ноты с пятью бемолями.", "Blum looks at his cards like a score with five flats.", "ブリュームはフラットが五つもある楽譜を見るように手札を見ている。"),
            ("Блюм вздыхает: «Фальшивит сегодня колода».", "Blum sighs: \"The deck is out of tune tonight.\"", "ブリュームはため息をつく。「今夜のカードは音が外れている」"),
            ("Блюм рассеянно слушает, как под палубой гудит машина.", "Blum listens absently to the engine humming below the deck.", "ブリュームは甲板の下でうなるエンジンの音を、ぼんやり聞いている。"),
            ("Блюм перекладывает карты то так, то эдак.", "Blum arranges his cards now this way, now that.", "ブリュームは手札をああ並べたり、こう並べたりしている。"),
            ("Блюм бормочет: «Пауза… Пауза — тоже музыка».", "Blum mutters: \"A rest… A rest is music too.\"", "ブリュームがつぶやく。「休符…休符もまた音楽だ」"),
        ],
        "bluff": [
            ("Блюм ставит широким жестом, будто берёт последний аккорд.", "Blum bets with a sweep of the arm, as if striking the last chord.", "ブリュームは最後の和音を鳴らすような大きな身ぶりで賭ける。"),
            ("Блюм начинает напевать и сбивается с мотива.", "Blum starts humming and loses the tune.", "ブリュームは鼻歌を始めるが、途中で節がわからなくなる。"),
            ("Блюм объявляет: «Импровизация!» — и двигает ставку.", "Blum announces \"Improvisation!\" and pushes his bet in.", "ブリュームは「即興演奏！」と宣言して賭け金を押し出す。"),
            ("Блюм смотрит в потолок и считает такты.", "Blum looks at the ceiling and counts bars.", "ブリュームは天井を見上げて小節を数えている。"),
            ("Блюм слишком долго поправляет бабочку.", "Blum takes far too long over his bow tie.", "ブリュームは蝶ネクタイを直すのに、やけに時間をかけている。"),
            ("Блюм говорит: «Сыграем крещендо» — и никому не смотрит в глаза.", "Blum says \"Let's have a crescendo\" and looks nobody in the eye.", "ブリュームは「クレッシェンドでいこう」と言うが、誰とも目を合わせない。"),
        ],
    },
    "ship_lev": {
        "strong": [
            ("Лев Маркович ставит сразу: видно, всё уже сосчитал.", "Lev Markovich bets at once: he has clearly done the sums.", "レフ・マルコヴィチはすぐに賭ける。もう計算は済んでいるらしい。"),
            ("Лев Маркович отстукивает пальцем: точка, тире, точка.", "Lev Markovich taps with a finger: dot, dash, dot.", "レフ・マルコヴィチが指で叩く。トン、ツー、トン。"),
            ("Лев Маркович кивает: «Шансы приличные».", "Lev Markovich nods: \"Decent odds.\"", "レフ・マルコヴィチはうなずく。「確率は悪くない」"),
            ("Лев Маркович убирает карандаш в нагрудный карман.", "Lev Markovich puts his pencil away in his breast pocket.", "レフ・マルコヴィチは鉛筆を胸ポケットにしまう。"),
            ("Лев Маркович складывает карты и кладёт сверху спичечный коробок.", "Lev Markovich squares his cards and sets a matchbox on top.", "レフ・マルコヴィチは手札をそろえ、上にマッチ箱をのせる。"),
            ("Лев Маркович смотрит на банк и довольно хмыкает.", "Lev Markovich looks at the pot and gives a satisfied grunt.", "レフ・マルコヴィチはポットを見て、満足げに鼻を鳴らす。"),
        ],
        "weak": [
            ("Лев Маркович что-то подсчитывает на полях газеты.", "Lev Markovich works something out in the margin of a newspaper.", "レフ・マルコヴィチは新聞の余白で何か計算している。"),
            ("Лев Маркович бормочет: «Один к пяти… нет, хуже».", "Lev Markovich mutters: \"One in five… no, worse.\"", "レフ・マルコヴィチがつぶやく。「五分の一…いや、もっと悪い」"),
            ("Лев Маркович снимает очки и трёт глаза.", "Lev Markovich takes off his glasses and rubs his eyes.", "レフ・マルコヴィチは眼鏡を外して目をこする。"),
            ("Лев Маркович отстукивает: три точки, три тире, три точки.", "Lev Markovich taps out: three dots, three dashes, three dots.", "レフ・マルコヴィチが叩く。トトト、ツーツーツー、トトト。"),
            ("Лев Маркович качает головой: «Помехи в эфире».", "Lev Markovich shakes his head: \"Static on the air.\"", "レフ・マルコヴィチは首を振る。「電波に雑音が入っている」"),
            ("Лев Маркович смотрит на банк, потом в карты, потом опять на банк.", "Lev Markovich looks at the pot, then at his cards, then at the pot again.", "レフ・マルコヴィチはポットを見て、手札を見て、またポットを見る。"),
        ],
        "bluff": [
            ("Лев Маркович ставит и тут же объясняет, почему это верно по теории.", "Lev Markovich bets and at once explains why theory says he is right.", "レフ・マルコヴィチは賭けるなり、なぜ理論上これが正しいかを説明し始める。"),
            ("Лев Маркович отстукивает что-то пальцем — и сбивается.", "Lev Markovich taps something out with a finger — and loses his place.", "レフ・マルコヴィチは指で何か打ち始めるが、途中で間違える。"),
            ("Лев Маркович говорит: «По теории вероятностей вам пора пасовать».", "Lev Markovich says: \"By the theory of probability it is time you folded.\"", "レフ・マルコヴィチが言う。「確率論によれば、あなたは降りるころですな」"),
            ("Лев Маркович слишком внимательно изучает рубашку своих карт.", "Lev Markovich studies the backs of his cards far too closely.", "レフ・マルコヴィチは自分のカードの背を、やけに熱心に調べている。"),
            ("Лев Маркович дважды подряд поправляет очки.", "Lev Markovich adjusts his glasses twice running.", "レフ・マルコヴィチは眼鏡を二度続けて直す。"),
            ("Лев Маркович сухо покашливает и ставит, не поднимая глаз.", "Lev Markovich gives a dry cough and bets without looking up.", "レフ・マルコヴィチは空咳をして、目を上げずに賭ける。"),
        ],
    },
}

TALK = {
    "ship_albert": [
        ("«В Риге, знаете, играют тише».", "\"In Riga, you know, they play more quietly.\"", "「リガではですね、もっと静かにやるものです」"),
        ("«Я, собственно, инженер. Карты — так, отдых для ума».", "\"I am an engineer, strictly speaking. Cards are just a rest for the mind.\"", "「私は本来、技師でして。カードは頭の休憩ですよ」"),
        ("«Молодой человек далеко пойдёт. Если вовремя остановится».", "\"The young man will go far. If he stops in time.\"", "「この若い方は出世しますよ。引き際さえ心得ていれば」"),
        ("«Колода старовата. Сменить бы… Впрочем, не настаиваю».", "\"The deck is rather worn. One might change it… But I don't insist.\"", "「このカードは少々くたびれていますな。替えては…いや、無理にとは」"),
        ("«Позвольте, я сниму».", "\"Allow me to cut.\"", "「失礼、私がカットしましょう」"),
        ("«Везение — тоже расчёт. Только чужой».", "\"Luck is calculation too. Somebody else's.\"", "「運もまた計算のうちです。ただし、他人の計算ですが」"),
        ("«Чудесный вечер. Море спокойное, публика доверчивая».", "\"A lovely evening. A calm sea, a trusting public.\"", "「すばらしい夜だ。海は穏やか、お客は人がいい」"),
        ("«Не люблю играть на спички. Но ради компании — извольте».", "\"I dislike playing for matches. But for the company's sake — very well.\"", "「マッチ棒の勝負は好みませんが、皆さんのためなら、まあ」"),
    ],
    "ship_givi": [
        ("«Кто проиграл — тому завтра коктейль за счёт бара. Молочный!»", "\"Whoever loses gets a cocktail on the house tomorrow. A milk one!\"", "「負けた人には明日、店のおごりでカクテルだ。ミルクのやつ！」"),
        ("«У нас в Батуми так сдавал один парикмахер. Плохо кончил».", "\"A barber in Batumi used to deal like that. He came to a bad end.\"", "「バトゥミにそういう配り方をする床屋がいたよ。ろくな最期じゃなかった」"),
        ("«Слушай, дорогой, не смотри так в карты — дырку прожжёшь».", "\"Listen, my dear, don't stare at the cards like that — you'll burn a hole.\"", "「なあ君、そんなに手札をにらむなよ。穴があくぞ」"),
        ("«Весь день я наливаю людям, а вечером люди меня обыгрывают».", "\"All day I pour for people, and in the evening people clean me out.\"", "「昼はずっと人に注いでやって、夜はその人たちに巻き上げられる」"),
        ("«Э, с такой картой только чай пить».", "\"Eh, a hand like this is good for drinking tea and nothing else.\"", "「ええい、こんな手じゃお茶でも飲むしかないな」"),
        ("«Сейчас бы хачапури, а не эти ваши двойки».", "\"What I want now is a khachapuri, not these twos of yours.\"", "「今ほしいのはハチャプリだよ、こんな2のカードじゃなくて」"),
        ("«Море любит смелых. А кого любят карты — не знаю».", "\"The sea loves the brave. Who the cards love, I don't know.\"", "「海は勇者を愛する。カードが誰を愛するのかは、知らないね」"),
        ("«Тихо, тихо, сдают! Гиви молчит. Гиви уже молчит».", "\"Hush, hush, they're dealing! Givi is silent. Givi is already silent.\"", "「しーっ、配ってるぞ！ギヴィは黙る。ギヴィはもう黙ってる」"),
    ],
    "ship_vera": [
        ("«Сдавайте внимательнее, вы не на базаре».", "\"Deal more carefully, you are not at the market.\"", "「もっと注意して配ってください。市場じゃないんですから」"),
        ("«Сорок лет в школе. Я вижу, когда списывают и когда блефуют».", "\"Forty years in a school. I can see who is copying and who is bluffing.\"", "「学校に四十年いました。カンニングもはったりも、見ればわかります」"),
        ("«Завтра Птичий остров. Не засиживайтесь».", "\"Bird Island tomorrow. Don't sit up too late.\"", "「明日は鳥の島ですよ。夜ふかしはいけません」"),
        ("«Не сутулься за столом. И не показывай карты соседу».", "\"Don't slouch at the table. And don't show your cards to your neighbour.\"", "「背中を丸めないの。それから、隣の人に手札を見せないこと」"),
        ("«Покер — та же география: надо знать, где находишься».", "\"Poker is geography too: you must know where you stand.\"", "「ポーカーも地理と同じ。自分が今どこにいるかを知ることです」"),
        ("«Хорошая игра. Садись, пять».", "\"Well played. Sit down, top marks.\"", "「いい勝負でした。座ってよろしい、満点です」"),
        ("«В моё время дети в этот час спали. Впрочем, каникулы».", "\"In my day children were asleep at this hour. Still, it is the holidays.\"", "「私の若いころ、子どもはこの時間には寝ていたものです。まあ、休暇中ですしね」"),
        ("«Кто тасует? Тасуйте как следует».", "\"Who is shuffling? Shuffle properly.\"", "「切っているのは誰？ちゃんと切ってくださいね」"),
    ],
    "ship_taras": [
        ("«Эх, сейчас как поставлю — мало не покажется!»", "\"Ha, wait till I bet — you'll know all about it!\"", "「ようし、今度おれが賭けたら、ただじゃすまねえぞ！」"),
        ("«Мы в Мурманске на тушёнку играли. Вот были ставки!»", "\"In Murmansk we played for tinned stew. Now those were stakes!\"", "「ムルマンスクじゃ缶詰の肉を賭けたもんだ。あれこそ勝負ってやつよ！」"),
        ("«Сдавай веселей, вахта не ждёт!»", "\"Deal lively there, the watch won't wait!\"", "「さっさと配れ、当直は待っちゃくれねえぞ！」"),
        ("«Не дрейфь, юнга. Карта как волна: то под тебя, то на тебя».", "\"Steady, lad. Cards are like a wave: now under you, now over you.\"", "「びびるな、坊主。カードは波と同じだ。持ち上げもすりゃ、かぶさりもする」"),
        ("«Кто так тасует? Дай сюда, это тебе не узлы вязать».", "\"Who shuffles like that? Give it here, it's not like tying knots.\"", "「なんだその切り方は。貸してみろ、結び目を作るのとはわけが違うんだ」"),
        ("«Капитану ни слова, что я тут до часу сижу».", "\"Not a word to the captain that I sit here till one.\"", "「おれが一時までここにいること、船長には内緒だぞ」"),
        ("«После отбоя чтоб тихо у меня. Ну, почти тихо».", "\"After lights out I want it quiet here. Well, nearly quiet.\"", "「消灯のあとは静かにしろよ。まあ、だいたい静かにな」"),
        ("«Хорошо сидим, братцы».", "\"This is the life, lads.\"", "「いい晩じゃねえか、兄弟」"),
    ],
    "ship_blum": [
        ("«Весь вечер играю: сначала на рояле, теперь на нервах».", "\"I've been playing all evening: first the piano, now on my nerves.\"", "「今夜はずっと弾きっぱなしだ。最初はピアノ、今は自分の神経をね」"),
        ("«Карты как джаз: правила знаешь, а играешь всё равно по слуху».", "\"Cards are like jazz: you know the rules and play by ear all the same.\"", "「カードはジャズと同じ。決まりは知っていても、結局は耳で弾く」"),
        ("«Тише, пианиссимо… Кто сдаёт?»", "\"Softly, pianissimo… Whose deal is it?\"", "「静かに、ピアニッシモで…。親は誰かな？」"),
        ("«Молодой человек, вы были на концерте? Отар сегодня в ударе».", "\"Young man, were you at the concert? Otar was on form tonight.\"", "「お若い方、演奏は聴いたかね？今夜のオタルは絶好調だった」"),
        ("«В Одессе я проиграл за вечер фрак. Потом играл в свитере».", "\"In Odessa I lost my tailcoat in one evening. I played in a jumper after that.\"", "「オデッサでは一晩で燕尾服を取られてね。そのあとはセーターで弾いたよ」"),
        ("«Эту раздачу я назвал бы “Блюз пустого кармана”».", "\"I would call that hand the Empty Pocket Blues.\"", "「今のゲームに題をつけるなら『からっぽポケットのブルース』だ」"),
        ("«Сдавайте в ритме вальса: раз, два, три».", "\"Deal in waltz time: one, two, three.\"", "「ワルツのリズムで配ってくれたまえ。いち、に、さん」"),
        ("«Аплодисменты победителю. Жидкие, но искренние».", "\"Applause for the winner. Thin, but sincere.\"", "「勝者に拍手を。まばらだが、心からのだよ」"),
    ],
    "ship_lev": [
        ("«Флеш-рояль приходит раз на 650 тысяч раздач. Жду сорок лет».", "\"A royal flush comes once in 650,000 deals. I have waited forty years.\"", "「ロイヤルフラッシュは六十五万回に一回。私は四十年待っています」"),
        ("«Пара приходит почти в каждой второй раздаче. Не радуйся ей слишком».", "\"A pair comes nearly every other deal. Don't be too glad of one.\"", "「ワンペアはほぼ二回に一回は来ます。あまり喜ばないことです」"),
        ("«Карты — это статистика. А люди — помехи».", "\"Cards are statistics. People are the static.\"", "「カードは統計です。人間のほうが雑音ですな」"),
        ("«В рубке тихо, эфир чистый. Можно ещё раздачу».", "\"All quiet in the radio room, the air is clear. One more hand, then.\"", "「通信室は静かで、電波もきれいです。もう一ゲームできますな」"),
        ("«Я записываю. К концу рейса скажу, кто из вас блефует чаще».", "\"I keep notes. By the end of the voyage I'll say which of you bluffs most.\"", "「記録をつけています。航海の終わりに、誰が一番はったりをかけるか発表しましょう」"),
        ("«Точка, тире… Простите, привычка».", "\"Dot, dash… Forgive me, a habit.\"", "「トン、ツー…失礼、癖でして」"),
        ("«Смелость — величина постоянная. Меняется только карта».", "\"Courage is a constant. Only the cards vary.\"", "「勇気は定数です。変わるのはカードだけ」"),
        ("«Кто выигрывает первую раздачу, редко выигрывает вечер. Наблюдение».", "\"He who wins the first hand seldom wins the evening. An observation.\"", "「最初のゲームに勝つ人は、めったにその晩を制しません。観察の結果です」"),
    ],
}

for who, states in TELLS.items():
    for state in ("strong", "weak", "bluff"):
        assert len(states[state]) == 6, (who, state)
        for n, line in enumerate(states[state]):
            s("poker.tell.%s.%s.%d" % (who, state, n), *line)
for who, lines in TALK.items():
    assert len(lines) == 8, who
    for n, line in enumerate(lines):
        s("poker.talk.%s.%d" % (who, n), *line)


def main():
    check = "--check" in sys.argv
    stale = 0
    for i, lang in enumerate(LANGS):
        text = json.dumps(collections.OrderedDict((k, v[i]) for k, v in T.items()), ensure_ascii=False, indent=1) + "\n"
        path = OUT % lang
        old = open(path, encoding="utf-8").read() if os.path.exists(path) else None
        if check:
            stale += old != text
        elif old != text:
            os.makedirs(os.path.dirname(path), exist_ok=True)
            open(path, "w", encoding="utf-8").write(text)
    if check and stale:
        print("FAIL: %d of Content/Localization/poker/strings_*.json are stale: run tools/pokersim/poker_strings.py" % stale)
        return 1
    print("poker strings: %d keys in %d languages%s" % (len(T), len(LANGS), " (up to date)" if check else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
