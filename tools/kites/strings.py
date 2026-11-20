#!/usr/bin/env python3
"""The kites' words (docs/kites.md) in ru / en / ja: the items, the hero's lines while flying, the dev panel. Russian is the
source. Writes them into Content/Localization/strings_<lang>.json (added or refreshed at the end; idempotent):
    python3 tools/kites/strings.py           write
    python3 tools/kites/strings.py --check   exit 1 when the tables are not as this script makes them
The stamp set's title lives with the city's words (tools/city/content/strings/collections.json, city.coll.kites.title)."""
import collections
import json
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
TABLE = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Localization", "strings_%s.json")

# key: (ru, en, ja)
WORDS = collections.OrderedDict([
    ("item.kite_diamond.name", ("Воздушный змей", "Kite", "凧")),
    ("item.kite_diamond.desc", ("Ромбовый змей с хвостом из бантиков, на катушке пятьдесят метров лески. Летает от лёгкого ветерка, но в сильный ветер его крутит.",
                                "A diamond kite with a tail of bows and fifty metres of line on the reel. It flies in a light breeze, but a strong wind sets it spinning.",
                                "蝶結びのしっぽがついたひし形の凧。糸巻きに糸が50メートル。そよ風でも上がるけど、強い風だとくるくる回ってしまう。")),
    ("item.kite_box.name", ("Коробчатый змей", "Box kite", "箱凧")),
    ("item.kite_box.desc", ("Две коробки из ткани на реечках. Тяжелее ромба, ему нужен ветер посильнее, зато он держится, когда ромбы уже крутит.",
                            "Two cloth boxes on slats. Heavier than a diamond, it wants a stronger wind, but it holds steady when the diamonds are spinning.",
                            "布の箱が二つ、細い棒に張ってある。ひし形より重くて強めの風がいるけど、ひし形が回りだす風でもしっかり飛ぶ。")),
    ("ui.kite.keys_wound", ("ЛКМ — подбросить змея · Q — убрать", "LMB — toss the kite up · Q — put it away", "左クリック：凧を放す・Q：しまう")),
    ("ui.kite.keys_up", ("ЛКМ — подтянуть · ПКМ — отпустить леску · колесо — по метру · Q — смотать",
                         "LMB — pull in · RMB — let the line out · wheel — a metre at a time · Q — wind it in",
                         "左クリック：たぐる・右クリック：糸を出す・ホイール：1メートルずつ・Q：巻き取る")),
    ("ui.kite.toss", ("Подбросил — ловит ветер!", "Up it goes, it's catching the wind!", "放した、風をつかんだ！")),
    ("ui.kite.toss_calm", ("Ветра почти нет… попробую.", "Hardly any wind… I'll give it a go.", "風がほとんどない…やってみよう。")),
    ("ui.kite.flying", ("Летит! Леска звенит в руке.", "It's flying! The line hums in my hand.", "飛んでる！糸が手の中で鳴ってる。")),
    ("ui.kite.fell", ("Упал. Подмотаю и попробую ещё.", "It came down. I'll wind in and try again.", "落ちちゃった。巻き取ってもう一回。")),
    ("ui.kite.fell_calm", ("Ветер стих — змей сел на землю.", "The wind dropped and the kite came down.", "風がやんで、凧が地面に下りた。")),
    ("ui.kite.spinning", ("Его крутит! Сматывай, внизу ветер слабее!", "It's spinning! Wind it in, the wind's weaker lower down!", "回ってる！巻き取らなきゃ、下のほうが風が弱い！")),
    ("ui.kite.steady", ("Выровнялся. Уф.", "It's steady again. Phew.", "持ち直した。ふう。")),
    ("ui.kite.winding", ("Сматываю леску…", "Winding the line in…", "糸を巻き取ってる…")),
    ("ui.kite.snagged", ("Зацепился! Змей застрял — не достать.", "It's caught! The kite's stuck up there, out of reach.", "引っかかった！凧が高いところに…もう取れない。")),
    ("ui.kite.snapped", ("Леска лопнула — змея унесло ветром.", "The line snapped and the wind took the kite away.", "糸が切れて、凧が風に飛ばされていった。")),
    ("ui.kite.sea", ("Упал в море. Всё, не вытащить.", "It fell in the sea. That's it, I can't get it back.", "海に落ちた。もう取り戻せない。")),
    ("dev.group.kite", ("Воздушные змеи", "Kites", "凧")),
    ("dev.action.kite.give", ("Выдать змеев (ромб и коробку)", "Give kites (diamond and box)", "凧をもらう（ひし形と箱）")),
    ("dev.action.kite.test", ("Десять змеев по ветру (бенч)", "Ten kites downwind (benchmark)", "風下に凧を10個（ベンチマーク）")),
    ("dev.tp.kite_beach", ("Пляж для змеев", "Kite beach", "凧揚げの浜")),
])


def build(lang_i, path):
    with open(path, encoding="utf-8") as f:
        d = json.load(f, object_pairs_hook=collections.OrderedDict)
    for k in WORDS:
        d.pop(k, None)
    for k, v in WORDS.items():
        d[k] = v[lang_i]
    return json.dumps(d, ensure_ascii=False, indent=1) + "\n"


def main():
    stale = []
    for i, lang in enumerate(("ru", "en", "ja")):
        path = TABLE % lang
        text = build(i, path)
        if open(path, encoding="utf-8").read() != text:
            stale.append(path)
            if "--check" not in sys.argv:
                with open(path, "w", encoding="utf-8") as f:
                    f.write(text)
                print("wrote", os.path.relpath(path, ROOT))
    if "--check" in sys.argv:
        for p in stale:
            print("kites/strings: %s is not up to date (python3 tools/kites/strings.py)" % os.path.relpath(p, ROOT))
        sys.exit(1 if stale else 0)
    print("kite words: %d keys in ru/en/ja" % len(WORDS))


if __name__ == "__main__":
    main()
