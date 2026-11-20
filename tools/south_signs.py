"""The Aton coast's land signs (blender/south/props_land.py): their words (ru / en / ja, proposals, not story canon) and
the textures typeset from them.

  python3 tools/south_signs.py --strings          add the south.sign.stele.* / south.sign.trail.* / south.board.* keys to the
                                                  three string tables (only missing keys; an existing key is never changed)
  python3 tools/south_signs.py [--lang ru,en,ja]  typeset textures/localized/<lang>/south_<id>.png (PIL and the OFL fonts in
                                                  fonts/, like tools/north_signs.py: no image model)
  python3 tools/south_signs.py --sheet out.png    one picture of all of them (ru, en, ja)

Textures (the `ru` ones are what blender/south/build_south.py bakes into the GLBs as material M_South_Text_<id>; the game
swaps the language with LyudText::LoadTexture("South", "south_<id>")):
  stele             2048 x 256   the town's name on the stele's bar (town_stele)
  sign_<v>_<n>      1024 x 512   plank n (1..3) of the trail finger-post variant v (trail_sign v0..v3): the upper half is
                                 the plank seen with its tip to the right, the lower half seen with its tip to the left
                                 (each plank carries words on both faces, never mirrored)
  board_trail       2048 x 1024  «Тропа Атона» notice (info_board v0)
  board_reeds       2048 x 1024  «Камыши. Тихо: птицы» at the bird hide (info_board v1)
"""
import argparse
import json
import math
import os
import random
import zlib

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, ".."))
LOC = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "Localization", "south", "strings_%s.json")   # the South's own tables
OUT = os.path.join(REPO, "textures", "localized", "%s")
HEAD = os.path.join(REPO, "fonts", "DelaGothicOne-Regular.ttf")
BODY = os.path.join(REPO, "fonts", "MPLUS1p-Bold.ttf")
BODY_R = os.path.join(REPO, "fonts", "MPLUS1p-Regular.ttf")

STRINGS = {
    # the stele by the track: the town's name as the menu shows it in each language
    "south.sign.stele.name": ("ЛЮДМИЛОВСК", "LYUDMILOVSK", "リュドミロフスク"),
    # the Aton coast trail's finger-posts: one key per place, one per distance (each plank = name + distance)
    "south.sign.trail.town": ("Город", "Town", "市街地"),
    "south.sign.trail.lookout": ("Смотровая", "Lookout", "展望台"),
    "south.sign.trail.sea": ("К морю", "To the sea", "海へ"),
    "south.sign.trail.aton": ("Атон", "Aton", "アトン"),
    "south.sign.trail.spring": ("Родник", "Spring", "湧き水"),
    "south.sign.trail.promenade": ("Набережная", "Promenade", "海岸通り"),
    "south.sign.trail.hide": ("Скрадок", "Bird hide", "野鳥観察小屋"),
    "south.sign.trail.station_beach": ("Станция «Пляж»", "Beach station", "「海辺」駅"),
    "south.sign.trail.cliff": ("Обрыв", "Cliff", "崖"),
    "south.sign.trail.station_mayak": ("Станция «Маяк»", "Lighthouse station", "「灯台」駅"),
    "south.sign.trail.dist.60m": ("60 м", "60 m", "60m"),
    "south.sign.trail.dist.110m": ("110 м", "110 m", "110m"),
    "south.sign.trail.dist.0_2km": ("0,2 км", "0.2 km", "0.2km"),
    "south.sign.trail.dist.0_25km": ("0,25 км", "0.25 km", "0.25km"),
    "south.sign.trail.dist.0_3km": ("0,3 км", "0.3 km", "0.3km"),
    "south.sign.trail.dist.0_4km": ("0,4 км", "0.4 km", "0.4km"),
    "south.sign.trail.dist.0_5km": ("0,5 км", "0.5 km", "0.5km"),
    # the notice board at the trail's head
    "south.board.trail.title": ("Тропа Атона", "The Aton Trail", "アトンの小道"),
    "south.board.trail.1": ("От станции «Атон» на мысу тропа идёт по гребню отрога вниз, в долину.",
                            "From the Aton station on the headland, the trail follows the ridge down into the valley.",
                            "岬のアトン駅から、小道は尾根づたいに谷へ下ります。"),
    "south.board.trail.2": ("Спуск пологий, но после дождя на нём скользко.",
                            "The descent is gentle, but slippery after rain.",
                            "下りはゆるやかですが、雨のあとは滑りやすくなります。"),
    "south.board.trail.3": ("Внизу течёт ручей; родник с чистой водой выше по долине.",
                            "A brook runs at the bottom; a spring of clear water is further up the valley.",
                            "下には小川が流れ、谷の上手にはきれいな湧き水があります。"),
    "south.board.trail.4": ("В камышах у устья ручья по вечерам собираются скворцы.",
                            "In the reedbed by the brook's mouth, starlings gather at dusk.",
                            "河口のヨシ原には、夕暮れどきにムクドリが集まります。"),
    "south.board.trail.5": ("Пожалуйста, не сходите с настила.",
                            "Please keep to the boardwalk.",
                            "木道から外れないでください。"),
    # the notice board at the bird hide
    "south.board.reeds.title": ("Камыши. Тихо: птицы", "The reedbed. Quiet: birds", "ヨシ原。お静かに：野鳥がいます"),
    "south.board.reeds.1": ("Летом здесь гнездятся камышовки.",
                            "Reed warblers nest here in summer.",
                            "夏にはヨシキリがここで子育てをします。"),
    "south.board.reeds.2": ("По вечерам на ночлег слетаются скворцы, иногда тысячами.",
                            "At dusk starlings fly in to roost, sometimes in their thousands.",
                            "夕方にはムクドリがねぐらに集まります。数千羽になることもあります。"),
    "south.board.reeds.3": ("У скрадка говорите тихо.",
                            "Speak softly by the hide.",
                            "観察小屋のそばでは静かに話しましょう。"),
    "south.board.reeds.4": ("Не сходите с настила: в камышах гнёзда и топко.",
                            "Do not leave the boardwalk: there are nests in the reeds and the ground is soft.",
                            "木道から出ないでください。ヨシの中には巣があり、足もとはぬかるんでいます。"),
}
LANGS = ("ru", "en", "ja")

# the finger-post planks: variant -> [(place key, distance key, yaw_deg)]; yaw = the direction the plank's tip points,
# degrees counter-clockwise seen from above, 0 = toward the post's front (-Y), 90 = to the front viewer's right (+X).
# blender/south/props_land.py builds the planks from this table (one source for the words and the geometry).
# The table comes from the land (tools/south.py SIGNS: where each post stands and what each plank points at).
import south as _south  # noqa: E402

PLANKS = _south.sign_planks()


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
            elif d[k] != v[li]:
                print("%s: %s already exists with other text (left as it is): %r" % (lang, k, d[k]))
        with open(path, "w", encoding="utf-8") as f:
            json.dump(d, f, ensure_ascii=False, indent=1)
            f.write("\n")
        print("%s: +%d keys" % (lang, n))


def S(lang):
    """Words from the string table (the source of truth once added), falling back to this file's table."""
    i = LANGS.index(lang)
    try:
        with open(LOC % lang, encoding="utf-8") as f:
            table = json.load(f)
    except (OSError, ValueError):
        table = {}
    return lambda k: table.get(k, STRINGS[k][i])


def font(path, size):
    return ImageFont.truetype(path, size)


def wrap(draw, text, fnt, width):
    """Lines no wider than width: words, or characters for Japanese (never breaking before 、。」：)."""
    if " " in text:
        toks, sep = text.split(" "), " "
    else:
        toks, sep = [], ""
        for ch in text:
            if toks and ch in "、。」）：・ー":
                toks[-1] += ch
            elif toks and toks[-1] and toks[-1][-1] in "「（":
                toks[-1] += ch
            else:
                toks.append(ch)
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
    if not sep and len(lines) > 1 and len(lines[-1]) <= 3:
        # no orphan "す。" alone on the last line: carry a few characters down with it
        k = 4 - len(lines[-1])
        lines[-2], lines[-1] = lines[-2][:-k], lines[-2][-k:] + lines[-1]
    return lines


def fit(draw, text, path, box_w, box_h, start, minimum=18, lines_max=3, lh=1.18):
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


def weathered(img, seed=3, stains=40):
    """Light grain and stains over a painted board so it is not a flat print."""
    r = random.Random(seed)
    w, h = img.size
    d = ImageDraw.Draw(img, "RGBA")
    for _ in range(int(w * h / 2500)):
        x, y = r.randrange(w), r.randrange(h)
        d.line([x, y, x + r.randrange(8, 60), y + r.randrange(-1, 2)], fill=(0, 0, 0, r.randrange(4, 14)), width=1)
    for _ in range(stains):
        x, y = r.randrange(w), r.randrange(h)
        rad = r.randrange(20, 90)
        d.ellipse([x - rad, y - rad // 2, x + rad, y + rad // 2], fill=(20, 15, 10, r.randrange(3, 9)))
    return img


# ---- the stele's bar: raised cast letters on concrete ----------------------------------------------------------------

def stele(lang):
    t = S(lang)
    W, H = 2048, 256
    rnd = random.Random(11)
    img = Image.new("RGB", (W, H), (178, 174, 164))
    d = ImageDraw.Draw(img, "RGBA")
    # board-formed concrete: faint horizontal formwork lines, speckle, a darker rain streak here and there
    for y in range(0, H, 64):
        d.line([0, y, W, y], fill=(120, 116, 108, 60), width=2)
    for _ in range(9000):
        x, y = rnd.randrange(W), rnd.randrange(H)
        c = rnd.randrange(90, 200)
        d.point((x, y), fill=(c, c - 2, c - 8, 90))
    for _ in range(14):
        x = rnd.randrange(W)
        d.rectangle([x, 0, x + rnd.randrange(6, 30), H], fill=(80, 76, 70, rnd.randrange(8, 20)))
    text = t("south.sign.stele.name")
    f, ls, size = fit(d, text, HEAD, W - 200, H - 60, 190, minimum=60, lines_max=1)
    # letter spacing: draw glyph by glyph, spread to fill the bar
    glyphs = list(ls[0])
    widths = [d.textlength(g, font=f) for g in glyphs]
    gap = min(size * 0.28, max(0.0, (W - 200 - sum(widths)) / max(1, len(glyphs) - 1)))
    total = sum(widths) + gap * (len(glyphs) - 1)
    x = (W - total) / 2
    asc, desc = f.getmetrics()
    y = (H - asc) / 2 - size * 0.08
    if any(ord(g) > 0x3000 for g in glyphs):
        y = (H - size) / 2 - size * 0.12
    for g, w in zip(glyphs, widths):
        # raised letter: dark cast shadow down-right, a lit upper-left edge, the face in weathered bronze-painted metal
        d.text((x + 7, y + 9), g, font=f, fill=(58, 52, 44, 170))
        d.text((x - 2, y - 2), g, font=f, fill=(236, 214, 150))
        d.text((x, y), g, font=f, fill=(196, 150, 70))
        x += w + gap
    return weathered(img, seed=5, stains=10)


# ---- finger-post planks ----------------------------------------------------------------------------------------------

PLANK_BG = (40, 84, 64)
PLANK_INK = (238, 230, 204)


def plank_half(lang, name_key, dist_key, tip_right):
    t = S(lang)
    W, H = 1024, 256
    img = Image.new("RGB", (W, H), PLANK_BG)
    d = ImageDraw.Draw(img, "RGBA")
    # painted boards: a little grain, a thin cream border inset from the edge, worn at the ends
    rnd = random.Random(zlib.crc32(("%s%d" % (name_key, tip_right)).encode()))
    for _ in range(260):
        y = rnd.randrange(H)
        x = rnd.randrange(W)
        d.line([x, y, x + rnd.randrange(30, 200), y], fill=(0, 0, 0, rnd.randrange(8, 26)), width=1)
    tip = 128                       # the last 0.1 m of the 0.82 m plank is the arrow's point: no words there
    x0, x1 = (64, W - tip) if tip_right else (tip, W - 64)
    d.rectangle([x0 - 6, 18, x1 + 6, H - 18], outline=PLANK_INK + (220,), width=5)
    name = t("south.sign.trail." + name_key)
    dist = t("south.sign.trail.dist." + dist_key)
    fd = font(BODY, 58)
    dw = d.textlength(dist, font=fd)
    room = (x1 - x0) - dw - 70
    f, ls, size = fit(d, name, HEAD, room, 170, 104, lines_max=1, minimum=34)
    nw = d.textlength(ls[0], font=f)
    cy = H / 2
    if tip_right:
        nx, dx = x0 + 26, x1 - 22 - dw
    else:
        dx, nx = x0 + 22, x1 - 26 - nw
    asc, _ = f.getmetrics()
    d.text((nx, cy - asc * 0.56), ls[0], font=f, fill=PLANK_INK)
    asc2, _ = fd.getmetrics()
    d.text((dx, cy - asc2 * 0.52), dist, font=fd, fill=PLANK_INK)
    # the point: a darker painted end so the arrow reads at a distance
    if tip_right:
        d.rectangle([W - tip + 8, 0, W, H], fill=(30, 64, 48))
    else:
        d.rectangle([0, 0, tip - 8, H], fill=(30, 64, 48))
    return img


def sign_plank(lang, variant, n):
    name_key, dist_key, _yaw = PLANKS[variant][n - 1]
    img = Image.new("RGB", (1024, 512))
    img.paste(plank_half(lang, name_key, dist_key, True), (0, 0))
    img.paste(plank_half(lang, name_key, dist_key, False), (0, 256))
    return weathered(img, seed=variant * 10 + n, stains=12)


# ---- notice boards ---------------------------------------------------------------------------------------------------

BOARD_BG = (238, 232, 212)
INK = (40, 46, 40)
GREEN = (36, 86, 62)


def sun_emblem(d, cx, cy, r, col=(214, 150, 52)):
    """The coast's little emblem: a sun with rays over three waves."""
    for i in range(16):
        a = 2 * math.pi * i / 16
        l = r * (1.75 if i % 2 == 0 else 1.45)
        d.line([cx + math.cos(a) * r * 1.2, cy + math.sin(a) * r * 1.2, cx + math.cos(a) * l, cy + math.sin(a) * l], fill=col, width=max(3, int(r * 0.12)))
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=col)
    for k in range(3):
        y = cy + r * 2.1 + k * r * 0.42
        pts = [(cx - r * 2.0 + s * r * 0.1, y + math.sin(s * 0.75) * r * 0.12) for s in range(41)]
        d.line(pts, fill=(60, 110, 140), width=max(3, int(r * 0.1)))


def board_frame(d, W, H, title, t):
    d.rectangle([0, 0, W, 190], fill=GREEN)
    d.rectangle([14, 14, W - 14, H - 14], outline=GREEN, width=8)
    para(d, (90, 30), t(title), HEAD, W - 520, 140, 108, (244, 236, 210), lines_max=1, minimum=50)


def board_trail(lang):
    t = S(lang)
    W, H = 2048, 1024
    img = Image.new("RGB", (W, H), BOARD_BG)
    d = ImageDraw.Draw(img, "RGBA")
    board_frame(d, W, H, "south.board.trail.title", t)
    sun_emblem(d, W - 230, 82, 46, col=(236, 186, 80))
    y = 250
    for i in range(1, 6):
        d.ellipse([92, y + 22, 118, y + 48], fill=GREEN)
        y = para(d, (150, y), t("south.board.trail.%d" % i), BODY, W - 260, 160, 66, INK, lines_max=2, lh=1.2, minimum=34) + 26
    return weathered(img, seed=21, stains=24)


def starling(d, x, y, s, col=(34, 34, 40)):
    d.polygon([(x - s, y), (x - s * 0.2, y - s * 0.25), (x, y - s * 0.7), (x + s * 0.2, y - s * 0.25), (x + s, y - s * 0.1),
               (x + s * 0.2, y + s * 0.1), (x - s * 0.2, y + s * 0.12)], fill=col)


def board_reeds(lang):
    t = S(lang)
    W, H = 2048, 1024
    img = Image.new("RGB", (W, H), BOARD_BG)
    d = ImageDraw.Draw(img, "RGBA")
    board_frame(d, W, H, "south.board.reeds.title", t)
    # a strip of reeds along the bottom with a warbler on a stem, a murmuration over it on the right
    rnd = random.Random(4)
    for i in range(70):
        x = 1460 + i * 8.4 + rnd.uniform(-3, 3)
        top = H - 40 - rnd.uniform(160, 340)
        d.line([x, H - 30, x + rnd.uniform(-14, 14), top], fill=(150, 130, 80), width=4)
        if i % 5 == 0:
            d.ellipse([x - 7, top - 30, x + 7, top + 10], fill=(118, 86, 54))
    wx, wy = 1690, 760
    d.ellipse([wx - 34, wy - 18, wx + 30, wy + 20], fill=(150, 116, 70))
    d.ellipse([wx + 14, wy - 30, wx + 46, wy - 2], fill=(150, 116, 70))
    d.polygon([(wx + 44, wy - 18), (wx + 66, wy - 14), (wx + 44, wy - 10)], fill=(90, 70, 40))
    d.polygon([(wx - 30, wy), (wx - 80, wy + 22), (wx - 26, wy + 14)], fill=(130, 100, 60))
    for k in range(90):
        a = rnd.uniform(0, 2 * math.pi)
        rr = rnd.uniform(0, 1) ** 0.6
        x = 1760 + math.cos(a) * rr * 210 + rnd.uniform(-10, 10)
        y = 380 + math.sin(a) * rr * 90 - (x - 1760) * 0.15
        starling(d, x, y, rnd.uniform(7, 13))
    y = 250
    for i in range(1, 5):
        d.ellipse([92, y + 22, 118, y + 48], fill=GREEN)
        y = para(d, (150, y), t("south.board.reeds.%d" % i), BODY, 1250, 230, 64, INK, lines_max=3, lh=1.2, minimum=34) + 26
    return weathered(img, seed=22, stains=24)


TEXTURES = {"stele": stele, "board_trail": board_trail, "board_reeds": board_reeds}
for _v, _rows in PLANKS.items():
    for _n in range(1, len(_rows) + 1):
        TEXTURES["sign_%d_%d" % (_v, _n)] = (lambda v, n: (lambda lang: sign_plank(lang, v, n)))(_v, _n)


def render_all(langs):
    for lang in langs:
        os.makedirs(OUT % lang, exist_ok=True)
        for name, fn in TEXTURES.items():
            p = os.path.join(OUT % lang, "south_%s.png" % name)
            fn(lang).save(p, optimize=True)
            print(p)


def sheet(path):
    cols = []
    for lang in LANGS:
        ims = [TEXTURES[n](lang) for n in TEXTURES]
        ims = [im.resize((768, max(1, int(im.size[1] * 768 / im.size[0])))) for im in ims]
        cols.append(ims)
    H = sum(im.size[1] + 8 for im in cols[0])
    out = Image.new("RGB", (768 * 3 + 16, H), (20, 20, 20))
    for c, ims in enumerate(cols):
        y = 0
        for im in ims:
            out.paste(im, (c * 776, y))
            y += im.size[1] + 8
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
