"""Appends the words of the station lobbies (docs/transport.md "Lobbies": the goods of the vending machines and kiosks, the
features' prompts) to the three string tables. Idempotent; rerun after a strings merge:  python3 tools/lobby_strings.py"""
import json
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DIR = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Localization")

ITEMS = {  # id: (ru name, ru desc, en name, en desc, ja name, ja desc)
 "water": ("Минеральная вода", "Холодная, чуть солоноватая, из горного источника. Стеклянная бутылка с крышкой.",
           "Mineral water", "Cold, faintly salty, from a mountain spring. A glass bottle with a cap.",
           "ミネラルウォーター", "冷たくて少し塩気のある山の湧き水。栓のついたガラス瓶。"),
 "coffee": ("Кофе", "Горячий, в бумажном стаканчике с крышкой. Пахнет жжёным сахаром.",
            "Coffee", "Hot, in a paper cup with a lid. Smells of burnt sugar.",
            "コーヒー", "ふた付きの紙コップに入った熱いコーヒー。焦がした砂糖の匂いがする。"),
 "tea": ("Чай в подстаканнике", "Крепкий, в гранёном стакане с железнодорожным подстаканником и ложечкой.",
         "Tea in a cup holder", "Strong tea in a faceted glass with a railway cup holder and a spoon.",
         "ホルダー入りの紅茶", "面取りグラスに入った濃い紅茶。鉄道風のホルダーとスプーン付き。"),
 "chips": ("Картофельные чипсы", "Хрусткие, в серебристо-рыжем пакете. Пакет надут воздухом больше, чем нужно.",
           "Potato chips", "Crunchy, in a silver-and-orange bag. The bag is puffed with more air than it needs.",
           "ポテトチップス", "銀とオレンジの袋に入ったパリパリのチップス。袋は必要以上に空気で膨らんでいる。"),
 "chocolate": ("Шоколад", "Плитка в синей фольге с золотой каймой. Тает в ладони быстрее, чем хотелось бы.",
               "Chocolate bar", "A bar in blue foil with a gold edge. Melts in the palm faster than you would like.",
               "板チョコ", "金の縁取りのある青いアルミ箔の板チョコ。手の中で思ったより早く溶ける。"),
 "cookies": ("Печенье", "Круглое сливочное, в бумажной упаковке с окошком. Крошится.",
             "Butter biscuits", "Round butter biscuits in a paper pack with a window. Crumbly.",
             "バタービスケット", "窓つきの紙包みに入った丸いバタービスケット。ぽろぽろ崩れる。"),
 "ice_cream": ("Мороженое", "Пломбир в вафельном рожке. Лучше съесть до того, как вы дойдёте до лифта.",
               "Ice cream", "Vanilla ice cream in a wafer cone. Best eaten before you reach the lift.",
               "アイスクリーム", "ワッフルコーンのバニラアイス。リフトに着く前に食べるのがいい。"),
 "pie": ("Пирожок", "Тёплый, с капустой и яйцом, на бумажной салфетке.",
         "Pie", "A warm pie with cabbage and egg, on a paper napkin.",
         "ピロシキ", "紙ナプキンにのった、キャベツと卵入りの温かいパイ。"),
 "seeds": ("Семечки", "Жареные подсолнечные семечки в бумажном кульке. Шелуху — в урну.",
           "Sunflower seeds", "Roasted sunflower seeds in a paper cone. The husks go in the bin.",
           "ひまわりの種", "紙の円錐に入った炒りひまわりの種。殻はごみ箱へ。"),
 "candy": ("Леденцы", "Фруктовые леденцы в целлофане. Один красный, один жёлтый, остальные слиплись.",
           "Fruit candies", "Hard fruit candies in cellophane. One red, one yellow, the rest stuck together.",
           "フルーツキャンディ", "セロファンに入ったフルーツ味のキャンディ。赤と黄が一つずつ、あとはくっついている。"),
 "magazine_science": ("Журнал «Космос и мы»", "Научно-популярный ежемесячник. На обложке ракета и Земля.",
                      "Magazine \"Cosmos and Us\"", "A monthly of popular science. A rocket and the Earth on the cover.",
                      "雑誌『宇宙とわたしたち』", "月刊の科学雑誌。表紙にはロケットと地球。"),
 "magazine_travel": ("Журнал «Берега»", "Путешествия по побережью: маяки, паромы, санатории.",
                     "Magazine \"Shores\"", "Journeys along the coast: lighthouses, ferries, sanatoriums.",
                     "雑誌『海岸』", "海岸の旅：灯台、フェリー、サナトリウム。"),
 "daily_paper": ("Вечерняя газета", "Свежая, пахнет типографской краской. Погода, расписание канатки, шахматная задача.",
                 "Evening paper", "Fresh, smells of ink. The weather, the cable-car timetable, a chess problem.",
                 "夕刊", "刷りたてでインクの匂いがする。天気、ロープウェイの時刻表、チェス問題。"),
 "postcard": ("Открытка", "Набережная с белой балюстрадой. На обороте пусто: можно написать.",
              "Postcard", "A promenade with a white balustrade. The back is empty: you could write on it.",
              "絵葉書", "白い欄干のある遊歩道。裏は空白で、何か書ける。"),
 "badge": ("Значок «Канатка»", "Эмалевый значок в виде красной кабинки. Коллекционный.",
           "Cable-car pin", "An enamel pin shaped like a red cabin. A collector's item.",
           "ロープウェイのバッジ", "赤いゴンドラの形のエナメルバッジ。コレクター向け。"),
 "amber_charm": ("Янтарная подвеска", "Капля балтийского янтаря на тонкой цепочке. Тёплая, если подержать в руке.",
                 "Amber pendant", "A drop of Baltic amber on a thin chain. Warm if you hold it.",
                 "琥珀のペンダント", "細い鎖につけたバルト海の琥珀のしずく。握ると温かい。"),
}
UI = {
 "ui.vending.pick": ("[1–{1}: выбор, сейчас {0}]", "[1–{1}: choose, now {0}]", "[1〜{1}：選択、現在{0}]"),
}

def main():
    for li, lang in enumerate(("ru", "en", "ja")):
        path = os.path.join(DIR, "strings_%s.json" % lang)
        with open(path, encoding="utf-8") as f:
            raw = f.read()
        d = json.loads(raw)
        add = {}
        for k, v in ITEMS.items():
            add["item.%s.name" % k] = v[li * 2]
            add["item.%s.desc" % k] = v[li * 2 + 1]
        for k, v in UI.items():
            add[k] = v[li]
        try:
            from lobby_strings_features import FEATURES
            for k, v in FEATURES.items():
                add[k] = v[li]
        except ImportError:
            pass
        new = {k: v for k, v in add.items() if d.get(k) != v}
        if not new:
            continue
        body = raw.rstrip()
        assert body.endswith("}")
        body = body[:-1].rstrip()
        rows = ",\n".join(" %s: %s" % (json.dumps(k, ensure_ascii=False), json.dumps(v, ensure_ascii=False)) for k, v in new.items() if k not in d)
        # a changed existing key is rewritten in place
        for k, v in new.items():
            if k in d:
                d[k] = v
        if any(k in d for k in new):
            with open(path, "w", encoding="utf-8") as f:
                json.dump(d, f, ensure_ascii=False, indent=1)
                f.write("\n")
            continue
        with open(path, "w", encoding="utf-8") as f:
            f.write(body + ",\n" + rows + "\n}\n")
        print(lang, len(new), "keys")

if __name__ == "__main__":
    main()
