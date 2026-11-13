"""The North's boards and signs: their words (ru / en / ja, proposals not canon) and the textures typeset from them.

  python3 tools/north_signs.py --strings          add the north.sign.* / north.board.* keys to the three string tables
  python3 tools/north_signs.py [--lang ru,en,ja]  typeset textures/localized/<lang>/north_<id>.png (no image model: PIL
                                                  and the OFL fonts in fonts/, like tools/train_ads.py)
  python3 tools/north_signs.py --sheet out.png    one picture of all of them (ru, en, ja rows)

Textures (the default `ru` ones are what blender/north/build_north.py bakes into the GLBs, material M_North_Text_<id>;
LyudText::LoadTexture("North", "north_<id>") swaps the language in the game):
  board_reserve   2048 x 1024  the reserve's gate board: title, rules, map (props: board_reserve)
  board_camp      1024 x 1024  the camp board: duty rota, a note on the hot spring (board_camp)
  sign_hot_ground 1024 x 1024  yellow warning plate (sign_hot_ground)
  sign_post_spring / sign_post_cone / sign_post_camp   1024 x 512  two planks of a fingerpost (sign_post), 1024 x 256 each
"""
import argparse
import json
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, ".."))
LOC = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "Localization", "strings_%s.json")
OUT = os.path.join(REPO, "textures", "localized", "%s")
HEAD = os.path.join(REPO, "fonts", "DelaGothicOne-Regular.ttf")
BODY = os.path.join(REPO, "fonts", "MPLUS1p-Bold.ttf")
BODY_R = os.path.join(REPO, "fonts", "MPLUS1p-Regular.ttf")

STRINGS = {
    "north.sign.reserve": ("Заказник «Северный»", "Severny Reserve", "「セヴェルヌイ」自然保護区"),
    "north.sign.reserve.rules": ("Правила посещения", "Rules of the reserve", "利用上のきまり"),
    "north.sign.reserve.rule.1": ("Ходить только по тропам", "Keep to the trails", "歩道以外は歩かない"),
    "north.sign.reserve.rule.2": ("Горячая земля: шаг в сторону от тропы опасен", "Hot ground: stepping off the trail is dangerous",
                                  "高温地帯：道から外れると危険です"),
    "north.sign.reserve.rule.3": ("Медведи: продукты вешать на шест", "Bears: hang your food on the pole", "クマに注意：食料は柱につるす"),
    "north.sign.reserve.rule.4": ("Костры только в кострище", "Fires only in the fire ring", "火は焚き火場でのみ"),
    "north.sign.reserve.rule.5": ("Мусор уносить с собой", "Carry out your litter", "ごみは持ち帰る"),
    "north.sign.reserve.map": ("Схема троп", "Trail map", "歩道図"),
    "north.sign.reserve.map.gate": ("Ворота", "Gate", "ゲート"),
    "north.sign.reserve.map.camp": ("Кемпинг", "Campsite", "キャンプ場"),
    "north.sign.reserve.map.spring": ("Источник", "Hot spring", "温泉"),
    "north.sign.reserve.map.fumaroles": ("Долина фумарол", "Fumarole Valley", "噴気谷"),
    "north.sign.reserve.map.cone": ("Сопка", "The Cone", "火山丘"),
    "north.sign.reserve.map.here": ("Вы здесь", "You are here", "現在地"),
    "north.sign.hot_ground": ("ГОРЯЧАЯ ЗЕМЛЯ", "HOT GROUND", "高温注意"),
    "north.sign.hot_ground.sub": ("Пар. Не сходить с тропы", "Steam. Stay on the trail", "蒸気あり・道から出ない"),
    "north.sign.spring": ("Горячий источник", "Hot spring", "温泉"),
    "north.sign.spring.back": ("Кемпинг", "Campsite", "キャンプ場"),
    "north.sign.cone": ("Сопка", "The Cone", "火山丘"),
    "north.sign.cone.back": ("Долина фумарол", "Fumarole Valley", "噴気谷"),
    "north.sign.camp": ("Кемпинг", "Campsite", "キャンプ場"),
    "north.sign.camp.back": ("Ворота заказника", "Reserve gate", "保護区ゲート"),
    "north.board.camp.title": ("Лагерь", "Camp", "キャンプ場"),
    "north.board.camp.rota": ("Дежурство", "Duty rota", "当番"),
    "north.board.camp.rota.1": ("Пн, Ср: Вадим, Оля", "Mon, Wed: Vadim, Olya", "月・水：ヴァジム、オーリャ"),
    "north.board.camp.rota.2": ("Вт, Чт: Серёга, Лена", "Tue, Thu: Seryoga, Lena", "火・木：セリョーガ、レーナ"),
    "north.board.camp.rota.3": ("Пт: Миша, Катя", "Fri: Misha, Katya", "金：ミーシャ、カーチャ"),
    "north.board.camp.rota.4": ("Сб, Вс: все вместе", "Sat, Sun: everyone", "土・日：全員"),
    "north.board.camp.spring.title": ("Источник", "Hot spring", "温泉"),
    "north.board.camp.spring": ("Тёплый источник севернее лагеря. Купаться можно, мыло в воду не нести.",
                                "The warm spring is north of the camp. Bathing is fine; keep soap out of the water.",
                                "温かい温泉はキャンプ場の北にあります。入浴できますが、石けんは使わないでください。"),
}
LANGS = ("ru", "en", "ja")


def add_strings():
    for li, lang in enumerate(LANGS):
        path = LOC % lang
        with open(path, encoding="utf-8") as f:
            d = json.load(f)
        n = 0
        for k, v in STRINGS.items():
            if k not in d:
                d[k] = v[li]
                n += 1
        with open(path, "w", encoding="utf-8") as f:
            json.dump(d, f, ensure_ascii=False, indent=1)
            f.write("\n")
        print("%s: +%d keys" % (lang, n))


def S(lang):
    i = LANGS.index(lang)
    return lambda k: STRINGS[k][i]


def font(path, size):
    return ImageFont.truetype(path, size)


def wrap(draw, text, fnt, width):
    """Lines of text no wider than width (words, or single characters for Japanese)."""
    toks = text.split(" ") if " " in text else list(text)
    sep = " " if " " in text else ""
    lines, cur = [], ""
    for t in toks:
        trial = (cur + sep + t) if cur else t
        if draw.textlength(trial, font=fnt) <= width or not cur:
            cur = trial
        else:
            lines.append(cur)
            cur = t
    if cur:
        lines.append(cur)
    return lines


def fit(draw, text, path, box_w, box_h, start, minimum=18, lines_max=3, lh=1.18):
    """The biggest size at which text wraps into <= lines_max lines inside the box."""
    size = start
    while size > minimum:
        f = font(path, size)
        ls = wrap(draw, text, f, box_w)
        if len(ls) <= lines_max and len(ls) * size * lh <= box_h:
            return f, ls, size
        size -= 2
    f = font(path, minimum)
    return f, wrap(draw, text, f, box_w), minimum


def para(draw, xy, text, path, box_w, box_h, start, fill, anchor="l", lines_max=3, lh=1.18, minimum=18):
    f, ls, size = fit(draw, text, path, box_w, box_h, start, minimum, lines_max, lh)
    x, y = xy
    for ln in ls:
        w = draw.textlength(ln, font=f)
        xx = x if anchor == "l" else x + (box_w - w) / 2 if anchor == "c" else x + box_w - w
        draw.text((xx, y), ln, font=f, fill=fill)
        y += size * lh
    return y


def weathered(img, rng_seed=3, amount=14):
    """Light grain and stains over a painted board so it is not a flat print."""
    import random
    r = random.Random(rng_seed)
    px = img.load()
    w, h = img.size
    d = ImageDraw.Draw(img, "RGBA")
    for _ in range(int(w * h / 2500)):
        x, y = r.randrange(w), r.randrange(h)
        d.line([x, y, x + r.randrange(8, 60), y + r.randrange(-1, 2)], fill=(0, 0, 0, r.randrange(4, 14)), width=1)
    for _ in range(40):
        x, y = r.randrange(w), r.randrange(h)
        rad = r.randrange(20, 90)
        d.ellipse([x - rad, y - rad // 2, x + rad, y + rad // 2], fill=(20, 15, 10, r.randrange(3, 9)))
    return img


# ---- the reserve's board -------------------------------------------------------------------------------------------

def map_panel(d, box, t):
    x0, y0, x1, y1 = box
    sys.path.insert(0, HERE)
    import north
    paper = (232, 224, 198)
    d.rectangle(box, fill=paper, outline=(60, 50, 38), width=6)
    pad = 26
    wx0, wx1 = -760.0, 280.0           # map X (west positive) shown, screen runs the other way: east to the right
    wy0, wy1 = 2330.0, 3980.0
    sx = (x1 - x0 - 2 * pad) / (wx1 - wx0)
    sy = (y1 - y0 - 2 * pad - 40) / (wy1 - wy0)
    k = min(sx, sy)

    def P(x, y):
        return (x1 - pad - (x - wx0) * k, y1 - pad - (y - wy0) * k)
    # sea in the west (left), the meadows, the Cone (only the trail's landmarks: the land itself is still being reshaped)
    d.rectangle([x0 + 6, y0 + 6, P(150, 0)[0] + 0, y1 - 6], fill=(150, 184, 200)) if P(150, 0)[0] > x0 + 6 else None
    sea_x = P(135.0, 0)[0]
    d.rectangle([x0 + 6, y0 + 6, sea_x, y1 - 6], fill=(150, 184, 200))
    for i in range(0, 24):
        xx = x0 + 6 + i * (sea_x - x0) / 24
        d.line([xx, y0 + 6, xx + 5, y1 - 6], fill=(165, 196, 210), width=1)
    cone = P(*north.CONE)
    d.ellipse([cone[0] - 205 * k, cone[1] - 205 * k, cone[0] + 205 * k, cone[1] + 205 * k], fill=(196, 170, 140), outline=(120, 96, 76), width=2)
    d.ellipse([cone[0] - 52 * k, cone[1] - 52 * k, cone[0] + 52 * k, cone[1] + 52 * k], fill=(120, 96, 82))
    # the trail
    names = ["north_gate", "north_camp", "north_spring", "north_fumaroles", "north_breach"]
    pts = [P(*north.ANCHORS[n]) for n in names]
    for a, b in zip(pts, pts[1:]):
        dist = math.hypot(b[0] - a[0], b[1] - a[1])
        n = int(dist / 14)
        for i in range(n):
            if i % 2 == 0:
                d.line([a[0] + (b[0] - a[0]) * i / n, a[1] + (b[1] - a[1]) * i / n,
                        a[0] + (b[0] - a[0]) * (i + 1) / n, a[1] + (b[1] - a[1]) * (i + 1) / n], fill=(150, 40, 28), width=6)
    lab = font(BODY, 34)
    labs = [("north.sign.reserve.map.gate", pts[0], (18, 0)), ("north.sign.reserve.map.camp", pts[1], (-18, -24)),
            ("north.sign.reserve.map.spring", pts[2], (-18, -22)), ("north.sign.reserve.map.fumaroles", pts[3], (20, -10)),
            ("north.sign.reserve.map.cone", pts[4], (-14, -50))]
    for key, p, (ox, oy) in labs:
        d.ellipse([p[0] - 9, p[1] - 9, p[0] + 9, p[1] + 9], fill=(150, 40, 28), outline=(30, 20, 10), width=3)
        text = t(key)
        w = d.textlength(text, font=lab)
        tx = p[0] + ox if ox >= 0 else p[0] + ox - w
        tx = min(max(tx, x0 + 12), x1 - w - 12)
        d.text((tx, p[1] + oy - 14), text, font=lab, fill=(40, 30, 20), stroke_width=4, stroke_fill=paper)
    here = pts[0]
    d.polygon([(here[0], here[1] + 26), (here[0] - 14, here[1] + 56), (here[0] + 14, here[1] + 56)], fill=(30, 90, 170))
    f = font(BODY, 30)
    text = t("north.sign.reserve.map.here")
    d.text((max(x0 + 12, here[0] - d.textlength(text, font=f) / 2), here[1] + 60), text, font=f, fill=(30, 90, 170), stroke_width=4, stroke_fill=paper)
    title = font(BODY, 36)
    d.text((x0 + 18, y0 + 12), t("north.sign.reserve.map"), font=title, fill=(40, 30, 20), stroke_width=4, stroke_fill=paper)


def board_reserve(lang):
    t = S(lang)
    img = Image.new("RGB", (2048, 1024), (28, 62, 48))
    d = ImageDraw.Draw(img)
    d.rectangle([14, 14, 2034, 1010], outline=(226, 214, 176), width=10)
    d.rectangle([34, 34, 2014, 990], outline=(226, 214, 176), width=3)
    para(d, (80, 60), t("north.sign.reserve"), HEAD, 1100, 260, 96, (238, 226, 188), lines_max=3, lh=1.1)
    d.line([80, 292, 1160, 292], fill=(238, 226, 188), width=4)
    f = font(BODY, 52)
    d.text((80, 318), t("north.sign.reserve.rules"), font=f, fill=(240, 196, 70))
    y = 400
    for i in range(1, 6):
        d.ellipse([84, y + 16, 108, y + 40], fill=(240, 196, 70))
        y = para(d, (130, y), t("north.sign.reserve.rule.%d" % i), BODY, 1000, 110, 46, (238, 226, 188), lines_max=2, lh=1.12, minimum=30) + 14
    map_panel(d, (1240, 60, 1990, 960), t)
    return weathered(img)


# ---- camp board --------------------------------------------------------------------------------------------------

def paper_sheet(img, box, angle=0.0, color=(238, 232, 214)):
    x0, y0, x1, y1 = box
    sheet = Image.new("RGBA", (x1 - x0, y1 - y0), color + (255,))
    return sheet


def board_camp(lang):
    t = S(lang)
    img = Image.new("RGB", (1024, 1024), (96, 76, 56))
    d = ImageDraw.Draw(img)
    for i in range(0, 1024, 128):
        d.line([0, i, 1024, i], fill=(66, 50, 36), width=3)
    # sheet 1: the duty rota
    d.rectangle([40, 36, 984, 500], fill=(240, 236, 218), outline=(120, 110, 90), width=3)
    d.text((76, 52), t("north.board.camp.title"), font=font(HEAD, 72), fill=(150, 30, 24))
    d.line([72, 150, 950, 150], fill=(40, 40, 40), width=3)
    d.text((76, 168), t("north.board.camp.rota"), font=font(BODY, 46), fill=(30, 30, 90))
    y = 244
    for i in range(1, 5):
        d.text((90, y), t("north.board.camp.rota.%d" % i), font=fit(d, t("north.board.camp.rota.%d" % i), BODY_R, 840, 60, 44, lines_max=1, minimum=26)[0], fill=(30, 30, 30))
        d.line([76, y + 56, 950, y + 56], fill=(170, 170, 170), width=1)
        y += 64
    # sheet 2: the hot spring note, pinned a little crooked
    note = Image.new("RGBA", (900, 440), (246, 226, 150, 255))
    nd = ImageDraw.Draw(note)
    nd.text((30, 22), t("north.board.camp.spring.title"), font=font(HEAD, 64), fill=(150, 30, 24))
    nd.line([28, 112, 872, 112], fill=(60, 50, 30), width=3)
    para(nd, (30, 132), t("north.board.camp.spring"), BODY, 840, 290, 46, (40, 36, 26), lines_max=5, lh=1.25, minimum=26)
    note = note.rotate(-1.6, expand=True, resample=Image.BICUBIC)
    img.paste(note, (50, 540), note)
    for p in ((512, 36), (150, 540), (860, 548)):
        d.ellipse([p[0] - 10, p[1] - 10, p[0] + 10, p[1] + 10], fill=(170, 30, 30), outline=(60, 10, 10), width=2)
    return weathered(img)


# ---- hot ground sign ---------------------------------------------------------------------------------------------

def sign_hot_ground(lang):
    t = S(lang)
    img = Image.new("RGB", (1024, 1024), (232, 188, 28))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([12, 12, 1012, 1012], radius=70, outline=(24, 22, 20), width=22)
    # warning triangle with steam wavy lines and a boot over cracked ground
    cx, top, base = 512, 70, 520
    tri = [(cx, top), (cx - 330, base), (cx + 330, base)]
    d.polygon(tri, fill=(236, 196, 40), outline=(24, 22, 20))
    d.line(tri + [tri[0]], fill=(24, 22, 20), width=26, joint="curve")
    for i, x in enumerate((cx - 100, cx, cx + 100)):
        pts = []
        for s in range(40):
            yy = 430 - s * 7
            pts.append((x + math.sin(s * 0.45 + i) * 22, yy))
        d.line(pts, fill=(24, 22, 20), width=18, joint="curve")
    d.polygon([(cx - 190, 470), (cx + 190, 470), (cx + 210, 500), (cx - 210, 500)], fill=(24, 22, 20))
    d.line([(cx - 120, 500), (cx - 80, 470), (cx - 40, 500), (cx + 20, 470), (cx + 60, 500)], fill=(236, 196, 40), width=7)
    para(d, (50, 585), t("north.sign.hot_ground"), HEAD, 924, 190, 118, (24, 22, 20), anchor="c", lines_max=2, lh=1.05)
    d.line([120, 790, 904, 790], fill=(24, 22, 20), width=6)
    para(d, (60, 815), t("north.sign.hot_ground.sub"), BODY, 904, 170, 70, (24, 22, 20), anchor="c", lines_max=2, lh=1.1, minimum=34)
    return weathered(img, amount=8)


# ---- finger post -------------------------------------------------------------------------------------------------

def sign_post(lang, key):
    t = S(lang)
    img = Image.new("RGB", (1024, 512), (150, 120, 84))
    d = ImageDraw.Draw(img)
    for row, (text, right) in enumerate(((t("north.sign." + key), True), (t("north.sign." + key + ".back"), False))):
        y0 = row * 256
        d.rectangle([0, y0, 1024, y0 + 256], fill=(166, 134, 96) if row == 0 else (150, 124, 92))
        d.rectangle([0, y0 + 8, 1024, y0 + 14], fill=(110, 86, 60))
        d.rectangle([0, y0 + 242, 1024, y0 + 248], fill=(110, 86, 60))
        f, ls, size = fit(d, text, HEAD, 700, 190, 110, lines_max=1, minimum=40)
        w = d.textlength(ls[0], font=f)
        # the arrow tip is on the right for the first plank, left for the second: text runs away from it
        x = 70 if right else 1024 - 70 - w
        d.text((x, y0 + 128 - size * 0.62), ls[0], font=f, fill=(240, 230, 200), stroke_width=2, stroke_fill=(60, 44, 28))
        # a carved arrow glyph
        ax = 930 if right else 94
        s = 1 if right else -1
        d.polygon([(ax - 28 * s, y0 + 100), (ax + 6 * s, y0 + 100), (ax + 6 * s, y0 + 78), (ax + 46 * s, y0 + 128), (ax + 6 * s, y0 + 178), (ax + 6 * s, y0 + 156), (ax - 28 * s, y0 + 156)], fill=(240, 230, 200))
    return weathered(img)


TEXTURES = {
    "board_reserve": board_reserve,
    "board_camp": board_camp,
    "sign_hot_ground": sign_hot_ground,
    "sign_post_spring": lambda lang: sign_post(lang, "spring"),
    "sign_post_cone": lambda lang: sign_post(lang, "cone"),
    "sign_post_camp": lambda lang: sign_post(lang, "camp"),
}


def render_all(langs):
    for lang in langs:
        os.makedirs(OUT % lang, exist_ok=True)
        for name, fn in TEXTURES.items():
            p = os.path.join(OUT % lang, "north_%s.png" % name)
            fn(lang).save(p, optimize=True)
            print(p)


def sheet(path):
    cells = []
    for lang in LANGS:
        row = [TEXTURES[n](lang).resize((512, 512 if n != "board_reserve" else 256)) for n in TEXTURES]
        cells.append(row)
    W = sum(c.size[0] for c in cells[0])
    H = 512
    out = Image.new("RGB", (W, H * 3), (20, 20, 20))
    for r, row in enumerate(cells):
        x = 0
        for c in row:
            out.paste(c, (x, r * H))
            x += c.size[0]
    out.save(path)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--strings", action="store_true")
    ap.add_argument("--lang", default="ru,en,ja")
    ap.add_argument("--sheet")
    a = ap.parse_args()
    if a.strings:
        add_strings()
    elif a.sheet:
        sheet(a.sheet)
    else:
        render_all(a.lang.split(","))
