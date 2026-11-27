#!/usr/bin/env python3
"""The kites' town side in words (docs/kites.md "Shops", "Quests"), ru / en / ja, Russian the source:

  ITEMS   the three kites the shops got (delta, stunt, dragon) and the quests' developer teleport, into
          Content/Localization/strings_<lang>.json
  QUESTS  the quests "First lesson", "The kite in the poplar", "Figure eight": journal, objectives, spoken lines, the
          remarks of quest_scenes.json, into Content/Localization/quests/strings_<lang>.json

    python3 tools/kites/town.py           write
    python3 tools/kites/town.py --check   exit 1 when the tables are not as this script makes them

A key that is in a table already keeps its place and gets its text; a new one goes to the end. So this script and the
others that write these tables (tools/kites/strings.py: the hero's lines while flying, the two first kites) can be run
in any order. The quests' structure is hand-written data: Content/UI/quests.json (kite_lesson, kite_poplar,
kite_eight), Content/Dialogue/npc_dialogue.txt ([Korneev_ivan], [Kid_boy], [Guy_a]), Content/UI/quest_scenes.json.
The townsfolk's own lines are tools/barks/lines/13_kites.txt, the stamps tools/city/content/collections.json.
The spoken lines are not voiced yet (tools/gen_voice.py voices what is missing)."""
import collections
import json
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
LOC = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Localization")
LANGS = ("ru", "en", "ja")

# key: (ru, en, ja)
ITEMS = collections.OrderedDict([
    ("item.kite_delta.name", ("Змей-дельта", "Delta kite", "デルタ凧")),
    ("item.kite_delta.desc", (
        "Треугольное крыло из лёгкой ткани, без хвоста. Поднимается в самый слабый ветер, когда ромб ещё лежит на песке. В свежий ветер его лучше не доставать.",
        "A triangular wing of light cloth, no tail. It goes up in the lightest air, when a diamond still lies on the sand. Best left rolled up in a fresh wind.",
        "軽い布の三角の翼で、しっぽはない。ひし形がまだ砂の上に寝ているような弱い風でも上がる。風の強い日は出さないほうがいい。")),
    ("item.kite_stunt.name", ("Пилотажный змей", "Stunt kite", "スポーツカイト")),
    ("item.kite_stunt.desc", (
        "Крыло на двух стропах с двумя ручками. Потянешь левую, и он уходит влево: петли, восьмёрки, проходы над самым песком. Ему нужен ветер посвежее и открытый пляж.",
        "A wing on two lines with two handles. Pull the left one and it goes left: loops, figure eights, passes right over the sand. It wants a fresher wind and an open beach.",
        "二本の糸と二つのハンドルで操る翼。左を引けば左へ曲がる。宙返り、8の字、砂すれすれの低空飛行。強めの風と広い浜がいる。")),
    ("item.kite_dragon.name", ("Змей-дракон", "Dragon kite", "龍凧")),
    ("item.kite_dragon.desc", (
        "Расписная голова и очень длинный хвост из лёгкой ткани. Летает неторопливо, любит ровный ветер. Разматывать и сматывать его лучше вдвоём.",
        "A painted head and a very long tail of light cloth. It flies unhurriedly and likes a steady wind. Unrolling it and rolling it up is a job for two.",
        "絵を描いた頭と、軽い布のとても長いしっぽ。ゆったり飛び、安定した風を好む。広げるのも巻くのも二人がかり。")),
    ("dev.tp.kite_poplar", ("Тополь со змеем (Солнечная площадь)", "The poplar with the kite (Sun Square)", "凧のかかったポプラ（太陽広場）")),
])

QUESTS = collections.OrderedDict([
    # ---- "First lesson": Ivan Korneev, once the Laboratory's mechanic, teaches whoever comes with a kite of their own
    ("quest.kite_lesson.title", ("Первый урок", "First lesson", "はじめての授業")),
    ("quest.kite_lesson.desc", (
        "Иван Корнеев, бывший механик Лаборатории, клеит воздушных змеев с детства. Говорят, держать леску он учит всякого, кто придёт со своим змеем.",
        "Ivan Korneev, once the Laboratory's mechanic, has glued kites since he was a boy. They say he teaches anyone who turns up with a kite of their own to hold the line.",
        "研究所の元機械工、イワン・コルネーエフさんは子どもの頃から凧を作っている。自分の凧を持っていけば、誰にでも糸の持ち方を教えてくれるらしい。")),
    ("quest.kite_lesson.meet.log", (
        "У меня теперь есть воздушный змей. В городе говорят: кто не умеет запускать, идёт к Ивану Корнееву из Академгородка. По утрам он рыбачит с пляжа, а с трёх до половины шестого сидит на скамейке на площади.",
        "I have a kite now. In town they say: if you can't fly one, go to Ivan Korneev in the Academgorodok. In the mornings he fishes from the beach, and from three to half past five he sits on a bench on the square.",
        "凧を手に入れた。町では、揚げ方を知らないならアカデムゴロドクのイワン・コルネーエフさんのところへ行け、と言う。朝は浜で釣りをして、三時から五時半までは広場のベンチにいる。")),
    ("quest.kite_lesson.find_korneev.obj", ("Поговорить с Иваном Корнеевым", "Talk to Ivan Korneev", "イワン・コルネーエフさんと話す")),
    ("quest.kite_lesson.launch.log", (
        "Корнеев говорит: сначала читают ветер, а уже потом разматывают леску. У спасательной вышки висит полосатый рукав. Висит тряпкой, значит, змею рано. Стоит колом, значит, поздно. Вытянулся наполовину, значит, в самый раз. Встать спиной к ветру, подбросить и держать, не отпуская леску сразу.",
        "Korneev says you read the wind first and only then unwind the line. A striped sock hangs by the lifeguard tower. Hanging like a rag: too early for a kite. Standing out stiff: too late. Half filled: just right. Back to the wind, toss it up and hold, without letting the line out at once.",
        "コルネーエフさんいわく、まず風を読む。糸を出すのはそれから。監視塔のそばに縞の吹き流しがある。だらりと垂れていれば凧にはまだ早い。棒みたいにまっすぐなら、もう遅い。半分ふくらんでいれば、ちょうどいい。風に背を向けて立ち、放り上げて、すぐには糸を出さずにこらえる。")),
    ("quest.kite_lesson.up.obj", ("Запустить змея и удержать его в воздухе", "Get the kite up and keep it in the air", "凧を揚げて、空にとどめる")),
    ("quest.kite_lesson.climb.log", (
        "Летит. Теперь выше: Корнеев велел отпускать леску понемногу, пока змей тянет. Наверху ветер сильнее и ровнее, чем у песка. Провисла, значит, подобрать и ждать.",
        "It flies. Now higher: Korneev said to let the line out a little at a time, while the kite pulls. Up there the wind is stronger and steadier than down by the sand. If the line sags, take some in and wait.",
        "飛んだ。次はもっと高く。凧が引いているあいだ、糸を少しずつ出せとコルネーエフさんは言った。上のほうが、砂のそばより風は強くて安定している。糸がたるんだら、たぐって待つ。")),
    ("quest.kite_lesson.high.obj", ("Поднять змея на тридцать метров", "Take the kite up to thirty metres", "凧を三十メートルの高さまで揚げる")),
    ("quest.kite_lesson.land.log", (
        "Высоко. Отсюда он размером с почтовую марку. Осталось самое трудное, сказал Корнеев: вернуть. Сматывать ровно, без рывков, и посадить на песок, а не на сосну.",
        "High. From here it is the size of a postage stamp. The hardest part is left, Korneev said: getting it back. Wind in evenly, no jerks, and set it down on the sand, not on a pine.",
        "高い。ここから見ると切手くらいの大きさだ。いちばん難しいのが残っている、とコルネーエフさんは言った。連れて帰ることだ。むらなく巻き取り、ぐいと引かず、松の上ではなく砂の上に下ろす。")),
    ("quest.kite_lesson.down.obj", ("Смотать леску и посадить змея", "Wind in the line and land the kite", "糸を巻き取って凧を下ろす")),
    ("quest.kite_lesson.tell.log", (
        "Змей у меня в руках, целый. Расскажу Корнееву. Он на скамейке на площади Академгородка с трёх до половины шестого.",
        "The kite is in my hands, in one piece. I'll tell Korneev. He is on the bench on the Academgorodok's square from three to half past five.",
        "凧は無事に手の中にある。コルネーエフさんに話そう。三時から五時半まで、アカデムゴロドクの広場のベンチにいる。")),
    ("quest.kite_lesson.tell_korneev.obj", ("Рассказать Ивану Корнееву", "Tell Ivan Korneev", "イワン・コルネーエフさんに報告する")),
    ("quest.kite_lesson.done", (
        "Корнеев выслушал и спросил только одно: звенела ли леска. Звенела. Тогда он достал из сумки свёрток: дельта, сшитая из старого плаща. Сказал, что она летает в ветер, которого почти нет, и что такие дни тоже надо уметь любить.",
        "Korneev heard me out and asked only one thing: did the line hum. It did. Then he took a roll out of his bag: a delta sewn from an old raincoat. He said it flies in a wind that is hardly there, and that one has to learn to love such days too.",
        "コルネーエフさんは最後まで聞いて、ひとつだけたずねた。糸は鳴ったか、と。鳴った。すると鞄から包みを出した。古いレインコートで縫ったデルタ凧だ。ほとんどない風でも飛ぶ、そういう日も好きになれなきゃいかん、と言った。")),
    ("line.korneev_ivan.kite_hint.1", (
        "Змеев я клею с детства. В «Волне» теперь продают готовых, они тоже летают. Заведёшь своего, приходи. Научу держать.",
        "I've glued kites since I was a boy. The Wave sells ready-made ones now, and they fly too. Get one of your own and come by. I'll teach you to hold it.",
        "凧は子どもの頃から作っとる。今は「ヴォルナ」で出来合いを売っていて、あれもちゃんと飛ぶ。自分のを手に入れたら来なさい。持ち方を教えてやる。")),
    ("line.korneev_ivan.kite_meet.1", (
        "Змей? Покажи. Рейки ровные, ткань не тянет. Летать будет. Вопрос, будешь ли ты.",
        "A kite? Show me. The spars are straight, the cloth doesn't pull. It will fly. The question is whether you will.",
        "凧か。見せてごらん。骨はまっすぐ、布もつっぱっていない。こいつは飛ぶ。問題は、お前さんが飛ばせるかだ。")),
    ("line.hero.kite_meet.2", ("Я думал, его просто подбрасывают.", "I thought you just tossed it up.", "放り上げるだけだと思ってました。")),
    ("line.korneev_ivan.kite_meet.3", (
        "Подбрасывают шапку. А змея отдают ветру. Сначала посмотри на рукав у спасательной вышки: вытянулся наполовину, значит, в самый раз.",
        "A cap is what you toss up. A kite you hand over to the wind. Look at the sock by the lifeguard tower first: half filled means just right.",
        "放り上げるのは帽子だ。凧は風に渡すものだ。まず監視塔の吹き流しを見なさい。半分ふくらんでいれば、ちょうどいい。")),
    ("line.korneev_ivan.kite_meet.4", (
        "Встань к ветру спиной. Подбрось и держи, лески сразу не давай. Потянет, отпускай понемногу. Провиснет, подбери.",
        "Stand with your back to the wind. Toss it and hold; don't give it line at once. When it pulls, let out a little. When it sags, take some in.",
        "風に背を向けて立つ。放り上げたら、こらえる。すぐに糸をやるな。引いてきたら少しずつ出す。たるんだら、たぐる。")),
    ("line.korneev_ivan.kite_meet.5", (
        "Подними его метров на тридцать, а потом верни на песок целым. Вернуть труднее. Иди, пока дует.",
        "Take it up thirty metres or so, then bring it back to the sand in one piece. Bringing it back is harder. Go on, while it blows.",
        "三十メートルほど揚げて、それから無事に砂の上へ戻す。戻すほうが難しいぞ。行きなさい、風があるうちに。")),
    ("line.korneev_ivan.kite_wait.1", (
        "Ты ещё здесь? Ветер не ждёт. Рукав, спина к ветру, и не жадничай с леской.",
        "Still here? The wind doesn't wait. The sock, your back to the wind, and don't be greedy with the line.",
        "まだここにいるのか。風は待ってくれんぞ。吹き流し、風に背中、それから糸を欲張るな。")),
    ("line.korneev_ivan.kite_wait_calm.1", (
        "Если рукав висит тряпкой, сядь и подожди. После полудня с моря обычно тянет.",
        "If the sock hangs like a rag, sit down and wait. After noon there is usually a draw off the sea.",
        "吹き流しがだらりと垂れていたら、座って待ちなさい。昼を過ぎると、たいてい海から風が来る。")),
    ("line.korneev_ivan.kite_wait_gale.1", (
        "А если рукав стоит колом и песок летит в глаза, не разматывай. Ромб в такой ветер крутит, а леску рвёт.",
        "And if the sock stands out stiff and the sand flies in your eyes, don't unwind. A wind like that spins a diamond and snaps the line.",
        "吹き流しが棒みたいにのびて、砂が目に入るような日は、糸を出すな。そういう風だと、ひし形は回りだすし、糸は切れる。")),
    ("line.korneev_ivan.kite_dark.1", (
        "В темноте змея не запускают: не видно ни его, ни сосен. Приходи на пляж днём. Рукав, спина к ветру, леску понемногу.",
        "Nobody flies a kite in the dark: you can see neither it nor the pines. Come to the beach by day. The sock, your back to the wind, the line a little at a time.",
        "暗い中で凧は揚げんものだ。凧も松も見えんからな。昼間に浜へ行きなさい。吹き流し、風に背中、糸は少しずつ。")),
    ("line.korneev_ivan.kite_tell.1", (
        "Ну? По лицу вижу, что летал. Леска звенела?",
        "Well? I can see by your face that it flew. Did the line hum?",
        "どうだ。顔を見ればわかる、飛んだな。糸は鳴ったか。")),
    ("line.hero.kite_tell.2", (
        "Звенела. И в руку отдавало, как будто он живой.",
        "It did. And it tugged at my hand as if it were alive.",
        "鳴りました。手にびりびり来て、まるで生きてるみたいでした。")),
    ("line.korneev_ivan.kite_tell.3", (
        "Это он с тобой здоровался. Я своего первого запустил мальчишкой, из газеты и дранки. С тех пор ни разу не надоело.",
        "That was it saying hello to you. I flew my first as a boy, made of newspaper and lath. It has never once bored me since.",
        "それは凧のあいさつだ。わしが初めて揚げたのは子どもの頃、新聞紙と木っ端でこしらえたやつだ。それから一度も飽きたことがない。")),
    ("line.korneev_ivan.kite_tell.4", (
        "Держи. Дельта, сам шил из старого плаща. Она летает, когда ветра почти нет. Тихие дни тоже надо уметь любить.",
        "Here. A delta; I sewed it myself from an old raincoat. It flies when there is hardly any wind. One has to learn to love the quiet days too.",
        "ほら、持っていきなさい。デルタだ。古いレインコートで自分で縫った。風がほとんどない日でも飛ぶ。静かな日も、好きになれなきゃいかん。")),
    ("line.korneev_ivan.kite_tell.5", (
        "И вот ещё что. Тёмке из клуба я весной ромб склеил, а он посадил его на тополь у Солнечной площади и молчит. Сними, если руки дойдут.",
        "And one more thing. In spring I glued a diamond for Tyoma from the club; he put it in a poplar by Sun Square and keeps quiet about it. Get it down, if you find the time.",
        "それと、もうひとつ。クラブのチョーマに、春にひし形を作ってやったんだが、太陽広場のポプラに引っかけたまま黙っとる。手があいたら、下ろしてやってくれ。")),
    ("line.korneev_ivan.kite_after.1", (
        "В слабый ветер бери дельту, в свежий коробку. А в шторм сиди дома и клей нового.",
        "In light air take the delta, in a fresh wind the box. And in a storm stay at home and glue a new one.",
        "弱い風ならデルタ、強めの風なら箱凧だ。嵐の日は家にいて、新しいのを作りなさい。")),
    ("line.korneeva_nina.kite_glue.1", (
        "Это тебя Ваня учит змея пускать? У меня весь подоконник в его рейках. Пускай учит. Лишь бы клей со стола убрал.",
        "Is it you Vanya is teaching to fly a kite? My whole windowsill is covered in his spars. Let him teach. As long as he takes the glue off the table.",
        "ワーニャが凧揚げを教えてるっていうのは、あんたかい。うちの窓台はあの人の骨組みだらけだよ。教えるのはいいさ。テーブルの糊さえ片づけてくれればね。")),
    ("journal.heard.nina_kite_glue", (
        "У Нины Корнеевой весь подоконник в рейках для змеев. Она ворчит про клей, но рейки не убирает.",
        "Nina Korneeva's whole windowsill is covered in kite spars. She grumbles about the glue, but she doesn't move the spars.",
        "ニーナ・コルネーエワさんの窓台は凧の骨組みだらけだ。糊のことはぼやくのに、骨組みは片づけない。")),

    # ---- "The kite in the poplar": the kite Korneev glued for Tyoma has hung in a poplar by Sun Square since spring
    ("quest.kite_poplar.title", ("Змей на тополе", "The kite in the poplar", "ポプラの凧")),
    ("quest.kite_poplar.desc", (
        "Корнеев склеил Тёме из клуба змея. Змей с весны висит на тополе, а Тёма делает вид, что так и надо.",
        "Korneev glued a kite for Tyoma from the club. Since spring it has hung in a poplar, and Tyoma pretends that is how it should be.",
        "コルネーエフさんはクラブのチョーマに凧を作ってやった。凧は春からポプラにぶら下がったままで、チョーマはそれでいいという顔をしている。")),
    ("quest.kite_poplar.rescue.log", (
        "Тёмин змей застрял на тополе за Солнечной площадью, в ряду с южной стороны. Леска, говорят, свисает почти до земли. Взяться за неё и тянуть понемногу, с ветром, а не рывком: рванёшь, и змей уйдёт в ветки глубже.",
        "Tyoma's kite is stuck in a poplar behind Sun Square, in the row on the south side. The line, they say, hangs almost to the ground. Take hold of it and pull a little at a time, with the wind, not with a jerk: yank it and the kite goes deeper into the branches.",
        "チョーマの凧は、太陽広場の裏、南側の並木のポプラに引っかかっている。糸は地面近くまで垂れているそうだ。それをつかんで、風に合わせて少しずつ引く。ぐいと引いたら、凧は枝の奥へもぐってしまう。")),
    ("quest.kite_poplar.free.obj", ("Снять змея с дерева", "Get the kite down from the tree", "木から凧を下ろす")),
    ("quest.kite_poplar.return.log", (
        "Снял. Хвост помят, одна рейка треснула, но это чинится. Тёма в клубе: после школы он сидит за Сегой до самого вечера.",
        "Got it down. The tail is crumpled and one spar has cracked, but that can be mended. Tyoma is at the club: after school he sits at the Sega until evening.",
        "下ろした。しっぽはくしゃくしゃ、骨が一本割れているけど、直せる。チョーマはクラブにいる。学校のあとは夕方までずっとセガの前だ。")),
    ("quest.kite_poplar.tell_tyoma.obj", ("Рассказать Тёме", "Tell Tyoma", "チョーマに知らせる")),
    ("quest.kite_poplar.done", (
        "Тёма сказал, что змей и сам бы слетел, просто не успел. Потом полез в карман и отдал мне свою машинку, ту самую, которую никому не даёт. Змея он понесёт Корнееву чинить. Сам. Сказал, что не боится.",
        "Tyoma said the kite would have come down by itself, it just hadn't had the time. Then he dug in his pocket and gave me his toy car, the one he never lends to anyone. He will take the kite to Korneev to be mended. Himself. He says he isn't scared.",
        "チョーマは、凧はそのうち自分で下りてきたはずだ、まだその時じゃなかっただけ、と言った。それからポケットを探って、誰にも貸さないあのミニカーを僕にくれた。凧は自分でコルネーエフさんのところへ直しに持っていくそうだ。こわくない、と言っていた。")),
    ("line.kid_boy.kite_stuck.1", (
        "Дед Корнеев рассказал, да? Он не застрял. Он там… сохранён. Как игра.",
        "Grandpa Korneev told you, did he? It isn't stuck. It's… saved there. Like a game.",
        "コルネーエフのじいちゃんが言ったんでしょ。引っかかってるんじゃないよ。あそこに…セーブしてあるの。ゲームみたいに。")),
    ("line.hero.kite_stuck.2", ("С весны?", "Since spring?", "春から？")),
    ("line.kid_boy.kite_stuck.3", (
        "Ну, долгое сохранение. Я дёргал, честно. Он только глубже залез. А деду сказать не могу: он его три вечера клеил.",
        "Well, a long save. I did tug at it, honest. It only went in deeper. And I can't tell Grandpa: he spent three evenings gluing it.",
        "まあ、長いセーブ。引っぱったんだよ、ほんとに。でももっと奥に入っちゃった。じいちゃんには言えない。三晩もかけて作ってくれたんだ。")),
    ("line.kid_boy.kite_stuck.4", (
        "Тополь за Солнечной площадью, с южной стороны. Леска висит низко. Только не порви, ладно?",
        "The poplar behind Sun Square, on the south side. The line hangs low. Just don't tear it, all right?",
        "太陽広場の裏、南側のポプラ。糸は低いところまで垂れてる。破かないでよ、いい？")),
    ("line.kid_boy.kite_stuck_again.1", (
        "Снял? Нет? Я не тороплю, я на боссе. Тополь за Солнечной площадью. Только ты там не рви.",
        "Got it down? No? I'm not rushing you, I'm on the boss. The poplar behind Sun Square. Just don't tear it.",
        "下ろせた？まだ？急がせてないよ、今ボス戦だし。太陽広場の裏のポプラね。破かないでよ。")),
    ("line.kid_boy.kite_back.1", (
        "Правда снял? Целый? Покажи… Рейка треснула. Это не ты, это тополь.",
        "You really got it down? In one piece? Show me… A spar's cracked. That wasn't you, that was the poplar.",
        "ほんとに下ろしたの？こわれてない？見せて…骨が割れてる。君じゃないよ、ポプラのせい。")),
    ("line.hero.kite_back.2", (
        "Корнеев починит. Он, по-моему, и не сердится.",
        "Korneev will mend it. I don't think he is even cross.",
        "コルネーエフさんが直してくれるよ。怒ってもいないと思う。")),
    ("line.kid_boy.kite_back.3", (
        "Я сам отнесу. Скажу, что это было долгое сохранение. Он поймёт, он же механик.",
        "I'll take it myself. I'll say it was a long save. He'll understand, he's a mechanic.",
        "自分で持っていく。長いセーブだったって言う。わかってくれるよ、機械工だもん。")),
    ("line.kid_boy.kite_back.4", (
        "На. Это моя машинка. Я её никому не даю, даже Соне. Но ты же его достал.",
        "Here. It's my car. I don't give it to anyone, not even Sonya. But you did get it down.",
        "はい。ぼくのミニカー。誰にも貸さないんだ、ソーニャにも。でも、君は凧を取ってくれたから。")),
    ("line.kid_boy.kite_flies_again.1", (
        "Дед рейку заменил. Говорит, в субботу идём на пляж. Я, наверное, один уровень пропущу. Один.",
        "Grandpa changed the spar. He says we're going to the beach on Saturday. I'll probably skip one level. One.",
        "じいちゃんが骨を取り替えてくれた。土曜は浜に行くんだって。たぶんステージを一つ休む。一つだけ。")),
    ("line.kid_girl.kite_saved.1", (
        "Тёма говорит, змей сам слетел. А у него теперь машинки нет, я видела. Значит, не сам.",
        "Tyoma says the kite came down by itself. But his car is gone now, I saw. So not by itself.",
        "チョーマは凧が自分で下りてきたって言うの。でもミニカーがなくなってる、見たもん。だから自分でじゃない。")),
    ("journal.heard.sonya_kite_saved", (
        "Соня вычислила, что змей слетел не сам: у Тёмы пропала машинка. В клубе ничего не скроешь.",
        "Sonya worked out that the kite didn't come down by itself: Tyoma's car is gone. You can't hide anything at the club.",
        "ソーニャは、凧がひとりでに下りたんじゃないと見抜いた。チョーマのミニカーがなくなっているからだ。クラブでは何も隠せない。")),

    # ---- "Figure eight": Stas from the club dares the hero on a two-line stunt kite
    ("quest.kite_eight.title", ("Восьмёрка", "Figure eight", "8の字")),
    ("quest.kite_eight.desc", (
        "Стас из клуба летом гоняет на пляже пилотажного змея на двух стропах. Он уверен, что я на таком и петли не сделаю.",
        "Stas from the club spends his summers on the beach flying a two-line stunt kite. He is sure I couldn't even loop one.",
        "クラブのスタスは、夏は浜で二本糸のスポーツカイトを飛ばしている。僕には宙返りひとつできないと決めてかかっている。")),
    ("quest.kite_eight.dare.log", (
        "Тёма всем рассказал про тополь. Стас теперь зовёт меня «пилот» и говорит, что одна леска — это прогулка, а летают на двух. Он в клубе после пар, с двух и до ночи.",
        "Tyoma has told everyone about the poplar. Stas now calls me \"pilot\" and says one line is a stroll, and flying takes two. He is at the club after his lectures, from two until night.",
        "チョーマがポプラの話をみんなにした。スタスは僕を「パイロット」と呼ぶようになり、糸一本は散歩で、飛ぶなら二本だと言う。講義のあと、二時から夜までクラブにいる。")),
    ("quest.kite_eight.hear_stas.obj", ("Поговорить со Стасом", "Talk to Stas", "スタスと話す")),
    ("quest.kite_eight.fly.log", (
        "Стас отдал мне своего старого пилотажного: стропы перевязаны, но тянет как новый. Уговор такой: старт с помощником, петля, восьмёрка и проход над самым песком. Нужен открытый пляж и ветер посвежее, чем для ромба. Кто проспорит, с того «Байкал».",
        "Stas gave me his old stunt kite: the lines have been re-tied, but it pulls like new. The deal: a start with a helper, a loop, a figure eight and a pass right over the sand. It takes an open beach and a fresher wind than a diamond needs. The loser buys the Baikal.",
        "スタスが古いスポーツカイトをくれた。糸は結び直してあるけど、引きは新品同様だ。約束はこう。助手つきの発進、宙返り、8の字、砂すれすれの通過。広い浜と、ひし形より強めの風がいる。負けたほうが「バイカル」をおごる。")),
    ("quest.kite_eight.helper.obj", ("Запустить пилотажного змея с помощником", "Launch the stunt kite with a helper", "助手に持ってもらってスポーツカイトを発進させる")),
    ("quest.kite_eight.loop.obj", ("Сделать петлю", "Fly a loop", "宙返りをする")),
    ("quest.kite_eight.eight.obj", ("Нарисовать в небе восьмёрку", "Draw a figure eight in the sky", "空に8の字を描く")),
    ("quest.kite_eight.low.obj", ("Пройти над самым песком", "Make a pass right over the sand", "砂すれすれを通過する")),
    ("quest.kite_eight.tell.log", (
        "Петля, восьмёрка, проход над песком. Руки гудят до локтей, и я не уверен, что под конец управлял я, а не он. Стасу скажу, что всё так и было задумано.",
        "A loop, an eight, a pass over the sand. My arms hum up to the elbows, and I am not sure that by the end I was the one steering. I'll tell Stas it was all intended.",
        "宙返り、8の字、砂の上の通過。腕はひじまでじんじんしていて、最後のほうは僕が操っていたのか、凧のほうだったのか、自信がない。スタスには、全部ねらいどおりだったと言おう。")),
    ("quest.kite_eight.tell_stas.obj", ("Рассказать Стасу", "Tell Stas", "スタスに報告する")),
    ("quest.kite_eight.done", (
        "Стас сначала не поверил. Потом спросил, в какую сторону я вышел из восьмёрки, и поверил. Поставил мне «Байкал» и велел оставить змея себе: ему всё равно некогда, у него катки. По-моему, он врёт. По-моему, в субботу он придёт на пляж.",
        "At first Stas didn't believe me. Then he asked which way I came out of the eight, and believed. He stood me a Baikal and told me to keep the kite: he has no time anyway, he has matches to play. I think he is lying. I think he'll be on the beach on Saturday.",
        "スタスは最初、信じなかった。それから、8の字をどっち向きに抜けたか聞いて、信じた。「バイカル」をおごってくれて、凧は持っていていいと言った。どうせ暇がない、試合があるから、と。うそだと思う。土曜には浜に来ると思う。")),
    ("line.guy_a.kite_dare.1", (
        "О, пилот. Тёма рассказал. Змея с дерева снять, брат, это не летать. Это уборка.",
        "Oh, the pilot. Tyoma told us. Getting a kite out of a tree, bro, isn't flying. It's tidying up.",
        "お、パイロット。チョーマから聞いたぞ。木から凧を下ろすのは、飛ばすのとは違うからな。あれは片づけだ。")),
    ("line.hero.kite_dare.2", ("А летать — это как?", "So what is flying?", "じゃあ、飛ばすっていうのは？")),
    ("line.guy_a.kite_dare.3", (
        "На двух стропах. Левую тянешь, идёт влево, правую, вправо. Обе на себя, разгоняется. Отклик мгновенный, брат, пинга ноль.",
        "On two lines. Pull the left, it goes left; the right, right. Both towards you and it speeds up. Instant response, bro, zero ping.",
        "糸二本でやるんだ。左を引けば左、右を引けば右。両方引けば加速する。反応は一瞬、ピンはゼロだ。")),
    ("line.guy_a.kite_dare.4", (
        "Бери моего старого, он в чехле. Старт с помощником: кто-нибудь на пляже держит змея шагах в двадцати по ветру, ты натягиваешь стропы, он отпускает.",
        "Take my old one, it's in its sleeve. A start with a helper: somebody on the beach holds the kite twenty paces downwind, you take up the lines, they let go.",
        "おれの古いのを持ってけ、袋に入ってる。発進は助手つきだ。浜にいる誰かに風下二十歩で凧を持ってもらって、お前が糸を張ったら放してもらう。")),
    ("line.guy_a.kite_dare.5", (
        "Потом петля, восьмёрка и проход над самым песком. Сделаешь, с меня «Байкал». Нет, с тебя.",
        "Then a loop, a figure eight and a pass right over the sand. Do it and the Baikal's on me. Don't, and it's on you.",
        "それから宙返り、8の字、砂すれすれの通過。できたら「バイカル」はおれのおごり。できなきゃ、お前のおごりだ。")),
    ("line.guy_a.kite_fly_wait.1", (
        "Ещё не летал? Жди ветра посвежее. В слабый он просто упадёт и будет лежать, как я по утрам. Помощник, петля, восьмёрка, проход.",
        "Not flown yet? Wait for a fresher wind. In light air it just drops and lies there, like me in the mornings. A helper, a loop, an eight, a pass.",
        "まだ飛ばしてないのか。もう少し強い風を待て。弱い風だと落ちて寝てるだけだ、朝のおれみたいに。助手、宙返り、8の字、通過な。")),
    ("line.guy_a.kite_fly_tip.1", (
        "Совет: в восьмёрке не крути в одну сторону, стропы завьются. Петля влево, петля вправо. Как стрейф.",
        "A tip: in the eight don't keep turning one way, the lines wind up. A loop left, a loop right. Like strafing.",
        "アドバイス。8の字で同じ向きにばかり回すな、糸がねじれる。左に一回、右に一回。ストレイフと同じだ。")),
    ("line.guy_a.kite_fly_tip2.1", (
        "И не на пирсе. Ему нужен песок и место. На пирсе он найдёт фонарь. Они всегда находят.",
        "And not on the pier. It needs sand and room. On the pier it will find a lamp post. They always do.",
        "桟橋ではやるなよ。砂と広さがいる。桟橋だと街灯を見つける。あいつらは必ず見つけるんだ。")),
    ("line.guy_a.kite_lost.1", (
        "Потерял? Утопил? Ладно. В универмаге, в спорттоварах, такие лежат, пятьсот двадцать. Мой был последний бесплатный. Уговор тот же: помощник, петля, восьмёрка, проход.",
        "Lost it? Drowned it? All right. The department store's sports counter has them, five hundred and twenty. Mine was the last free one. Same deal: a helper, a loop, an eight, a pass.",
        "なくした？沈めた？まあいい。百貨店のスポーツ用品売り場にある、五百二十だ。ただでやれるのはおれのが最後。約束は同じ、助手、宙返り、8の字、通過。")),
    ("line.guy_a.kite_done.1", (
        "Сделал? Все три? Так. В какую сторону ты вышел из восьмёрки?",
        "Done? All three? Right. Which way did you come out of the eight?",
        "やったのか。三つとも？じゃあ聞く。8の字はどっち向きに抜けた？")),
    ("line.hero.kite_done.2", ("Вправо. Кажется. Он сам решил.", "To the right. I think. It decided for itself.", "右。たぶん。凧が自分で決めたんだ。")),
    ("line.guy_a.kite_done.3", (
        "Верю. Кто врёт, тот говорит «я решил». Держи «Байкал», уговор есть уговор.",
        "I believe you. The ones who lie say \"I decided\". Here's your Baikal, a deal's a deal.",
        "信じる。うそをつくやつは「おれが決めた」って言うからな。ほら「バイカル」。約束は約束だ。")),
    ("line.guy_a.kite_done.4", (
        "Змея оставь себе. Мне всё равно некогда, у меня катки. Только стропы суши, если намочишь.",
        "Keep the kite. I've no time anyway, I've got matches. Just dry the lines if you get them wet.",
        "凧はやるよ。どうせ暇がない、試合があるからな。糸をぬらしたら、ちゃんと乾かせよ。")),
    ("line.guy_a.kite_saturday.1", (
        "В субботу обещают умеренный с моря. Это я просто так говорю. Прогноз читал.",
        "They promise a moderate wind off the sea on Saturday. I'm only saying. I read the forecast.",
        "土曜は海からほどよい風の予報だ。別に、言ってみただけ。予報を見ただけだ。")),
])

TABLES = (("strings_%s.json", ITEMS), (os.path.join("quests", "strings_%s.json"), QUESTS))


def build(path, words, lang_i):
    with open(path, encoding="utf-8") as f:
        text = f.read()
    d = json.loads(text, object_pairs_hook=collections.OrderedDict)
    for k, v in words.items():
        d[k] = v[lang_i]      # (an OrderedDict keeps a known key where it is)
    return text, json.dumps(d, ensure_ascii=False, indent=1) + "\n"


def main():
    check = "--check" in sys.argv
    stale = []
    for name, words in TABLES:
        for k, v in words.items():
            if len(v) != 3 or not all(s.strip() for s in v):
                sys.exit("kites/town: %s needs ru, en, ja" % k)
        for i, lang in enumerate(LANGS):
            path = os.path.join(LOC, name % lang)
            old, new = build(path, words, i)
            if old != new:
                stale.append(path)
                if not check:
                    with open(path, "w", encoding="utf-8") as f:
                        f.write(new)
                    print("wrote", os.path.relpath(path, ROOT))
    if check:
        for p in stale:
            print("kites/town: %s is not up to date (python3 tools/kites/town.py)" % os.path.relpath(p, ROOT))
        sys.exit(1 if stale else 0)
    print("kites, the town's words: %d item keys, %d quest keys in ru/en/ja" % (len(ITEMS), len(QUESTS)))


if __name__ == "__main__":
    main()
