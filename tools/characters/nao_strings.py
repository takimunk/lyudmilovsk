"""Nao's names and lines in ru/en/ja, appended to strings_<lang>.json in the same order (rerun-safe: existing keys are overwritten in place)."""
import json
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
STRINGS = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Localization", "strings_%s.json")

ROWS = [
    ("npc.nao.name", "Нао", "Nao", "ナオ"),
    ("npc.nao.role", "стоит у выхода", "standing by the exit", "出口のそばに立っている"),
    ("line.nao.first.1", "Вышел. Я так и думала, что сегодня кто-нибудь выйдет.",
     "You came out. I had a feeling someone would come out today.", "出てきたね。今日は誰か出てくると思ってた。"),
    ("line.hero.nao_first.2", "Ты кого-то ждёшь?", "Are you waiting for someone?", "誰か待ってるの？"),
    ("line.nao.first.3", "Нет. Я просто стою там, где начинается снаружи.",
     "No. I just stand where the outside begins.", "ううん。外が始まるところに立ってるだけ。"),
    ("line.nao.first.4", "Меня зовут Нао. Если забудешь, ничего, я напомню.",
     "I'm Nao. If you forget, that's fine, I'll remind you.", "わたしはナオ。忘れても大丈夫、また教えてあげる。"),
    ("line.nao.again1.1", "Море сегодня такое же, как вчера. Или это я такая же.",
     "The sea is the same as yesterday. Or maybe I am.", "海は昨日と同じ。それとも、同じなのはわたしのほう？"),
    ("line.hero.nao_again2.1", "Красивая серёжка.", "Nice earring.", "そのピアス、きれいだね。"),
    ("line.nao.again2.2", "Это лошадь. Она всё время бежит и никуда не убегает.",
     "It's a horse. It's always running and never gets anywhere.", "馬だよ。ずっと走ってるのに、どこにも行かないの。"),
    ("line.nao.again3.1", "Внутри все смотрят в экраны. А я лучше постою здесь, на свету.",
     "Inside, everyone looks at screens. I'd rather stand out here, in the light.", "中ではみんな画面を見てる。わたしはここで、光の中に立っていたい。"),
    ("line.nao.again4.1", "Если долго стоять у двери, начинаешь слышать, как она дышит.",
     "If you stand by a door long enough, you start hearing it breathe.", "ドアのそばに長く立っていると、ドアが息をしているのが聞こえてくるの。"),
    ("line.nao.sun_day.1", "Тепло. Солнце сегодня как будто смотрит прямо на нас.",
     "It's warm. As if the sun were looking right at us today.", "あたたかい。今日の太陽は、まっすぐわたしたちを見てるみたい。"),
    ("line.nao.sun_low.1", "Подожди, не уходи. Оно сейчас коснётся воды.",
     "Wait, don't go. It's about to touch the water.", "待って、まだ行かないで。もうすぐ水にふれるから。"),
    ("line.nao.sun_night.1", "Ночью его не видно, но я знаю, где оно. Как будто за дверью.",
     "At night you can't see it, but I know where it is. Like it's behind a door.", "夜は見えないけど、どこにあるかはわかる。ドアの向こうにいるみたいに。"),
    ("line.nao.sun_cloudy.1", "Облака. Ничего, оно всё равно там. Я чувствую.",
     "Clouds. It's fine, it's still up there. I can feel it.", "雲だね。でも大丈夫、ちゃんとそこにいる。感じるの。"),
]


def main():
    for i, lang in enumerate(("ru", "en", "ja")):
        path = STRINGS % lang
        s = json.load(open(path, encoding="utf-8"))
        for row in ROWS:
            s[row[0]] = row[1 + i]
        with open(path, "w", encoding="utf-8") as f:
            json.dump(s, f, ensure_ascii=False, indent=1)
            f.write("\n")


if __name__ == "__main__":
    main()
