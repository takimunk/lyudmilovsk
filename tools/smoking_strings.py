"""Adds the smoking kit's words (docs/smoking.md) to the three string tables, same keys in the same order, at the end.
Re-running replaces the values. python3 tools/smoking_strings.py"""
import json
import os
from collections import OrderedDict

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
T = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Localization", "strings_%s.json")

ROWS = [
    ("item.cigarettes.name", "Сигареты «Скворец»", "Skvorets cigarettes", "タバコ「スクヴォレツ」"),
    ("item.cigarettes.desc", "Пачка на десять сигарет с фильтром. На пачке честно написано, что курение вредит здоровью.",
     "A pack of ten filter cigarettes. The pack says it honestly: smoking harms your health.",
     "フィルター付き十本入りのパック。喫煙は健康を害すると正直に書いてある。"),
    ("item.lighter.name", "Зажигалка", "Lighter", "ライター"),
    ("item.lighter.desc", "Оранжевая пластмассовая зажигалка с колёсиком.", "An orange plastic lighter with a spark wheel.", "火打ち車の付いたオレンジ色のプラスチックライター。"),
    ("label.cigarettes.brand", "Скворец", "Skvorets", "スクヴォレツ"),
    ("label.cigarettes.warning", "Курение вредит вашему здоровью", "Smoking harms your health", "喫煙はあなたの健康を害します"),
    ("label.cigarettes.count", "10 сигарет с фильтром", "10 filter cigarettes", "フィルター付き 10本"),
    ("ui.smoke.hint_unlit", "ЛКМ — прикурить   ПКМ — убрать в пачку", "LMB — light it   RMB — put it back", "左クリック — 火をつける   右クリック — 箱に戻す"),
    ("ui.smoke.hint_lit", "ЛКМ — к губам, держать — затянуться   ПКМ — потушить и выбросить",
     "LMB — to the lips, hold — take a drag   RMB — put it out and throw it away", "左クリック — 口元へ、長押し — 吸う   右クリック — 消して捨てる"),
    ("ui.smoke.no_lighter", "Нечем прикурить", "Nothing to light it with", "火がない"),
    ("ui.smoke.take_pack", "[E] Взять сигареты и зажигалку", "[E] Take the cigarettes and the lighter", "[E] タバコとライターを取る"),
    ("ui.smoke.take_lighter", "[E] Взять зажигалку", "[E] Take the lighter", "[E] ライターを取る"),
    ("ui.smoke.empty", "Пачка пуста", "The pack is empty", "箱は空だ"),
    # the hero's cough: never shown, only voiced (MiniMax reads the tags as sounds)
    ("line.hero.cough.1", "(coughs) (coughs) Кха… кха-кха.", "(coughs) (coughs) Ugh… (coughs)", "(coughs) (coughs) ゴホッ…ゴホゴホ。"),
    ("line.hero.cough.2", "(coughs) Кх-кх… (coughs) (gasps) Ох…", "(coughs) Khh… (coughs) (gasps) Oh…", "(coughs) ケホッ…(coughs) (gasps) うっ…"),
    ("line.hero.cough.3", "(coughs) (coughs) (coughs) Гадость…", "(coughs) (coughs) (coughs) Gross…", "(coughs) (coughs) (coughs) まずい…"),
]

for i, lang in enumerate(("ru", "en", "ja")):
    with open(T % lang, encoding="utf-8") as f:
        d = json.load(f, object_pairs_hook=OrderedDict)
    for row in ROWS:
        d[row[0]] = row[1 + i]
    with open(T % lang, "w", encoding="utf-8") as f:
        f.write(json.dumps(d, ensure_ascii=False, indent=1) + "\n")
print(len(ROWS), "keys")
