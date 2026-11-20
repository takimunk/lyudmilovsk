#!/usr/bin/env python3
"""The sea railway as a passenger (docs/railway.md "Riding as a passenger", backlog item 21): the fare, the fine, the three
conductors (by day), the announcements. Russian is the source of every word.

    python3 tools/rail_passenger/passenger.py            # write
    python3 tools/rail_passenger/passenger.py --check    # only check

Writes unreal/Lyudmilovsk/Content/Railway/passenger.json (read by ULyudTrainPassenger, model Private/Railway/RailTicketModel.h,
checked by tools/railsim/run.sh tickets) and unreal/Lyudmilovsk/Content/Localization/rail/strings_<lang>.json.
"""
import collections
import json
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
OUT = os.path.join(CONTENT, "Railway", "passenger.json")
OUT_STRINGS = os.path.join(CONTENT, "Localization", "rail")
LANGS = ("ru", "en", "ja")

FARE = 5          # roubles, one ride, any distance (the ticket box at each halt)
FINE = 20         # a ride without a ticket, when the conductor fines
WALK_CMS = 85.0   # the conductor's pace down the aisle
START_AFTER = 12.0  # seconds after the doors shut before the conductor starts down the car

# The conductors: one a day (by the day's number), a crowd body (shared looks, as the townsfolk's) and a manner.
#   "warns": how many rides without a ticket get only a stern word before the fine (the strict one fines at once).
CONDUCTORS = [
    {"id": "valya", "model": "Korneeva_nina", "warns": 1},
    {"id": "timofeich", "model": "Crowd_12", "warns": 0},
    {"id": "sato", "model": "Sato_kei", "warns": 2},
]

# Every line a conductor says: ask (coming up to the hero), punch (a ticket punched), again (already punched this ride),
# warn (no ticket: a stern word), fine (no ticket: the fine taken, {0} roubles), broke (no ticket, no money for the fine).
LINES = {
    "valya": {
        "name": ("Кондуктор тётя Валя", "Conductor Auntie Valya", "車掌のワーリャおばさん"),
        "ask": ("Билетики, мои хорошие! Ваш, молодой человек?", "Tickets, my dears! Yours, young man?", "切符を拝見しますよ。お兄さんのは？"),
        "punch": ("Вот и пробила. Смотри в окно: сейчас чайки полетят.", "There, punched. Look out of the window: the gulls will come in a moment.",
                  "はい、切りました。窓の外を見てごらん。もうすぐカモメが来るから。"),
        "again": ("Пробит уже, вижу. Сиди, сиди.", "Already punched, I can see. Sit, sit.", "もう切ってあるね。座ってていいよ。"),
        "warn": ("Без билета? Ай-ай-ай. В этот раз прощу, а в кассу у платформы — пять рублей, не забудь.",
                 "No ticket? Tut-tut. I'll let it go this once, but the ticket box on the platform is five roubles: don't forget.",
                 "切符なし？あらあら。今回は見逃すけど、ホームの券売機で五ルーブルよ。忘れないで。"),
        "fine": ("Опять без билета. Ну, родной, штраф {0} рублей — квитанцию держи.", "No ticket again. Well, dear, a fine of {0} roubles: here's the receipt.",
                 "また切符なしね。罰金{0}ルーブル、はい領収書。"),
        "broke": ("И денег нет? Эх. На следующей выйдешь и купишь, договорились?", "And no money either? Oh dear. Get off at the next halt and buy one, agreed?",
                  "お金もないの？しょうがないね。次で降りて買うこと、いいね？"),
    },
    "timofeich": {
        "name": ("Кондуктор Тимофеич", "Conductor Timofeich", "車掌のチモフェイチ"),
        "ask": ("Предъявляем проездные документы.", "Travel documents, please.", "乗車券を拝見します。"),
        "punch": ("Порядок.", "In order.", "よろしい。"),
        "again": ("Компостирован. Следующий.", "Punched. Next.", "検札済み。次。"),
        "warn": ("Без билета. Чтоб я тебя больше так не видел.", "No ticket. Don't let me see you like this again.", "無賃乗車だな。二度とするなよ。"),
        "fine": ("Безбилетный проезд. Штраф {0} рублей. По правилам.", "Riding without a ticket. Fine: {0} roubles. Rules are rules.",
                 "無賃乗車。罰金{0}ルーブル。規則だ。"),
        "broke": ("Платить нечем? На следующей остановке — на выход.", "Nothing to pay with? Off at the next halt.", "払えない？次の駅で降りなさい。"),
    },
    "sato": {
        "name": ("Кондуктор Сато", "Conductor Sato", "車掌の佐藤さん"),
        "ask": ("Прошу прощения… Билет, пожалуйста.", "Excuse me… Your ticket, please.", "失礼します…切符を拝見します。"),
        "punch": ("Благодарю. Хорошей поездки к морю.", "Thank you. Have a good ride to the sea.", "ありがとうございます。海までよい旅を。"),
        "again": ("Уже проверено. Простите, что побеспокоил.", "Already checked. Sorry to bother you.", "確認済みでした。失礼しました。"),
        "warn": ("Билета нет?.. Ничего. Касса на платформе, в следующий раз, хорошо?",
                 "No ticket?.. That's all right. The ticket box is on the platform: next time, all right?",
                 "切符がない…？大丈夫です。券売機はホームにあります。次はお願いしますね。"),
        "fine": ("Мне очень жаль, но придётся взять штраф: {0} рублей.", "I'm very sorry, but I must take a fine: {0} roubles.",
                 "申し訳ありませんが、罰金{0}ルーブルをいただきます。"),
        "broke": ("Денег нет? Тогда, пожалуйста, сойдите на следующей и купите билет.", "No money? Then please get off at the next halt and buy a ticket.",
                  "お金がない？では次の駅で降りて切符を買ってください。"),
    },
}

S = [
    # the ticket box ({0} fare, {1} the halt's name)
    ("rail.ticket.prompt", "[E] Купить билет — {0} ₽", "[E] Buy a ticket — {0} ₽", "[E] 切符を買う — {0}₽"),
    ("rail.ticket.bought", "Билет на поезд: {0} ₽", "Train ticket: {0} ₽", "電車の切符：{0}₽"),
    ("rail.ticket.no_money", "Не хватает денег на билет ({0} ₽)", "Not enough money for a ticket ({0} ₽)", "切符代が足りない（{0}₽）"),
    ("rail.ticket.have", "Билет уже есть. [E] Ещё один — {0} ₽", "You have a ticket. [E] Another — {0} ₽", "切符は持っている。[E] もう一枚 — {0}₽"),
    ("rail.fine.paid", "Штраф: {0} ₽", "Fine: {0} ₽", "罰金：{0}₽"),
    # the conductor: {0} the name, {1} the line
    ("rail.conductor.says", "{0}: «{1}»", "{0}: “{1}”", "{0}：「{1}」"),
    # announcements ({0} a halt's name)
    ("rail.announce.next", "Следующая остановка — «{0}».", "Next stop: {0}.", "次は{0}です。"),
    ("rail.announce.arrive", "Остановка «{0}». Двери открываются.", "{0}. The doors are opening.", "{0}です。ドアが開きます。"),
    ("rail.announce.doors", "Осторожно, двери закрываются.", "Mind the doors, please.", "ドアが閉まります。ご注意ください。"),
    ("rail.announce.last", "Поезд дальше не идёт. Просьба освободить вагоны.", "This train terminates here. Please leave the cars.",
     "この電車はここまでです。お降りください。"),
    ("rail.announce.title", "Объявление в вагоне", "In the car", "車内放送"),
    # the item
    ("item.rail_ticket.name", "Билет на поезд", "Train ticket", "電車の切符"),
    ("item.rail_ticket.desc", "Картонный билетик морской железной дороги, одна поездка. Кондуктор пробьёт его в вагоне.",
     "A little card ticket of the sea railway, one ride. The conductor punches it in the car.",
     "海の鉄道の硬券。一回乗車用。車内で車掌が切る。"),
    ("item.rail_ticket_punched.name", "Пробитый билет", "Punched ticket", "切った切符"),
    ("item.rail_ticket_punched.desc", "Билет с дырочкой от компостера: поездка к морю была.",
     "A ticket with the punch's hole: the ride to the sea happened.", "鋏の穴があいた切符。海への旅の記念。"),
    ("dev.action.rail.conductor", "Поезд: кондуктор идёт к герою сейчас", "Train: the conductor comes to the hero now",
     "電車：車掌を今すぐ主人公のところへ"),
]


def main():
    check = "--check" in sys.argv
    errors = []
    table = collections.OrderedDict()

    def put(k, ru, en, ja):
        if k in table:
            errors.append("twice: " + k)
        table[k] = {"ru": ru, "en": en, "ja": ja}

    for row in S:
        put(*row)
    kinds = ("ask", "punch", "again", "warn", "fine", "broke")
    for c in CONDUCTORS:
        L = LINES[c["id"]]
        put("rail.conductor.%s.name" % c["id"], *L["name"])
        for k in kinds:
            put("rail.conductor.%s.%s" % (c["id"], k), *L[k])
    for k, v in table.items():
        ph = sorted(re.findall(r"\{\d+\}", v["ru"]))
        for l in LANGS:
            if not v[l].strip() or sorted(re.findall(r"\{\d+\}", v[l])) != ph:
                errors.append("string %s: %s empty or placeholders differ" % (k, l))
    for c in CONDUCTORS:
        if "{0}" not in LINES[c["id"]]["fine"][0]:
            errors.append("%s: the fine line needs {0}" % c["id"])
    data = collections.OrderedDict([
        ("about", "Written by tools/rail_passenger/passenger.py (docs/railway.md \"Riding as a passenger\"); never edit by hand."),
        ("fare", FARE), ("fine", FINE), ("walk_cms", WALK_CMS), ("start_after", START_AFTER),
        ("conductors", [collections.OrderedDict([("id", c["id"]), ("model", c["model"]), ("warns", c["warns"]),
                                                 ("name", "rail.conductor.%s.name" % c["id"])]
                                                + [(k, "rail.conductor.%s.%s" % (c["id"], k)) for k in kinds]) for c in CONDUCTORS]),
    ])
    if errors:
        print("\n".join("passenger.py: " + e for e in errors), file=sys.stderr)
        return 1
    outs = [(OUT, json.dumps(data, ensure_ascii=False, indent=1) + "\n")]
    for l in LANGS:
        outs.append((os.path.join(OUT_STRINGS, "strings_%s.json" % l),
                     json.dumps(collections.OrderedDict((k, v[l]) for k, v in table.items()), ensure_ascii=False, indent=1) + "\n"))
    stale = []
    for path, text in outs:
        old = open(path, encoding="utf-8").read() if os.path.exists(path) else None
        if old != text:
            stale.append(os.path.relpath(path, ROOT))
            if not check:
                os.makedirs(os.path.dirname(path), exist_ok=True)
                open(path, "w", encoding="utf-8").write(text)
    if check and stale:
        print("passenger.py: out of date: " + ", ".join(stale), file=sys.stderr)
        return 1
    print("passenger.py: %d conductors, %d strings%s" % (len(CONDUCTORS), len(table), "" if check else ", wrote %d" % len(stale)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
