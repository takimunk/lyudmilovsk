"""The words of the film camera's own interface (docs/photo.md), Russian first: the finder, the film, the studio's
counter, the darkroom, the album. Writes tools/photo/strings/ui.json; tools/photo/strings.py then builds the game's
tables. The studio's shop, the quest and the stamps are city words (tools/city/content/strings/photo.json).

    python3 tools/photo/strings_src.py"""
import json
import os

S = {}


def t(key, ru, en, ja):
    assert key not in S, key
    S[key] = {"ru": ru, "en": en, "ja": ja}


t("ui.photo.loaded", "Плёнка заряжена: 36 кадров.", "Film loaded: 36 frames.", "フィルムを入れた。36枚。")
t("ui.photo.no_film", "В аппарате нет плёнки.", "There is no film in the camera.", "カメラにフィルムがない。")
t("ui.photo.film_end", "Плёнка кончилась. R — перемотать.", "The film is finished. R to rewind.", "フィルムが終わった。Rで巻き戻す。")
t("ui.photo.rewind.nothing", "Перематывать нечего.", "Nothing to rewind.", "巻き戻すものがない。")
t("ui.photo.rewound", "Плёнка перемотана и вынута. Отснятых плёнок в кармане: {0}.", "The film is rewound and out. Exposed films in the pocket: {0}.",
  "フィルムを巻き戻して取り出した。撮影済みフィルム：{0}本。")
t("ui.photo.focus.inf", "∞", "∞", "∞")
t("ui.photo.focus.m", "{0} м", "{0} m", "{0} m")
t("ui.photo.finder", "резкость {0}   ·   кадр {1} из {2}", "focus {0}   ·   frame {1} of {2}", "ピント {0}   ·   {2}枚中{1}枚")
t("ui.photo.finder.no_film", "резкость {0}   ·   плёнки нет", "focus {0}   ·   no film", "ピント {0}   ·   フィルムなし")

t("ui.photo.studio.prints", "Отпечатки готовы: {0}. Они в альбоме (P).", "The prints are ready: {0}. They are in the album (P).", "プリントができた：{0}枚。アルバム（P）に入れた。")
t("ui.photo.studio.taken", "Плёнок сдано: {0}, {1} руб. Будет готово завтра к десяти утра.", "Films handed in: {0}, {1} roubles. Ready tomorrow by ten in the morning.",
  "フィルムを{0}本預けた。{1}ルーブル。明日の朝10時にできあがる。")
t("ui.photo.studio.no_money", "Проявка стоит {0} руб. Столько нет.", "Developing costs {0} roubles. You don't have that much.", "現像は{0}ルーブル。お金が足りない。")
t("ui.photo.studio.wait", "Плёнки ещё в работе. Будут готовы к десяти утра.", "The films are still being done. Ready by ten in the morning.", "フィルムはまだ現像中。朝10時にできあがる。")
t("ui.photo.studio.nothing", "Сдавать нечего: отснятых плёнок нет.", "Nothing to hand in: no exposed films.", "預けるものがない。撮影済みのフィルムがない。")

t("ui.photo.dark.nothing", "Проявлять нечего: отснятых плёнок нет.", "Nothing to develop: no exposed films.", "現像するものがない。撮影済みのフィルムがない。")
t("ui.photo.dark.bath.0", "Проявитель", "Developer", "現像液")
t("ui.photo.dark.bath.1", "Стоп-ванна", "Stop bath", "停止液")
t("ui.photo.dark.bath.2", "Фиксаж", "Fixer", "定着液")
t("ui.photo.dark.seconds", "{0} с   (нужно {1})", "{0} s   (needs {1})", "{0} 秒   （目安 {1}）")
t("ui.photo.dark.hint", "E — вынуть плёнку", "E — take the film out", "E：フィルムを引き上げる")
t("ui.photo.dark.done", "Плёнка проявлена, отпечатки высохли: {0}. Они в альбоме (P).", "The film is developed, the prints are dry: {0}. They are in the album (P).",
  "現像できた。乾いたプリントは{0}枚。アルバム（P）に入れた。")
t("ui.photo.dark.pale", "Плёнка проявлена кое-как, отпечатки вышли серые: {0}.", "The film came out so-so, the prints are grey: {0}.", "現像はいまひとつ。プリントは灰色っぽい：{0}枚。")
t("ui.photo.dark.ruined", "Плёнка испорчена: на ней ничего не вышло.", "The film is ruined: nothing came out on it.", "フィルムをだめにした。何も写っていない。")

t("ui.photo.album.title", "Альбом", "Album", "アルバム")
t("ui.photo.album.empty", "Пока ни одного отпечатка.", "Not a single print yet.", "プリントはまだ一枚もない。")
t("ui.photo.album.keys", "A, D — листать   ·   P — закрыть", "A, D — turn pages   ·   P — close", "A・D：ページをめくる   ·   P：閉じる")
t("ui.photo.album.page", "{0} / {1}", "{0} / {1}", "{0} / {1}")
t("ui.photo.album.caption", "{0}, {1}", "{0}, {1}", "{0}　{1}")
t("ui.photo.album.nowhere", "где-то в пути", "somewhere on the way", "どこかの道で")

t("dev.group.photo", "Фотоаппарат", "Camera", "カメラ")
t("dev.action.photo.give", "Дать фотоаппарат, 3 плёнки и 200 руб.", "Give a camera, 3 films and 200 roubles", "カメラ、フィルム3本、200ルーブルをもらう")
t("dev.action.photo.develop", "Проявить все плёнки сейчас", "Develop every film now", "すべてのフィルムを今すぐ現像")
t("dev.action.photo.album", "Открыть альбом", "Open the album", "アルバムを開く")

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "strings", "ui.json")
with open(out, "w", encoding="utf-8") as f:
    json.dump(S, f, ensure_ascii=False, indent=1, sort_keys=True)
    f.write("\n")
print("ui.json: %d keys" % len(S))
