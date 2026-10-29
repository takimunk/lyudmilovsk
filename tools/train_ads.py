"""The train's printed matter (docs/railway.md "The inside"): the advertisements and the notices, one picture per
language, composed here so every word is typeset from the string tables (never drawn by an image model).

  python3 tools/train_ads.py [--lang ru,en,ja] [--sheet out.jpg]

Art: design/railway/ads/gen/ads_a.png (a Higgsfield 4x3 grid of wordless Showa poster illustrations, pinned in
ads_a.json). Words: strings_<lang>.json, keys poster.train.<id>.title|line and sign.train.*. Fonts: fonts/ (OFL):
Dela Gothic One for headlines, M PLUS 1p for the rest, both cover Cyrillic, Latin and Japanese.

Out: one atlas per language, textures/localized/<lang>/train_atlas.jpg (4096 x 4096), imported by
unreal/scripts/import_railway.py as /Game/Railway/Localized/<lang>/T_RL_train_atlas (LyudText::LoadTexture("Railway",
...)): the car's inside has one material for all its print. Where each print sits in it (the same in every language):
design/railway/ads/atlas.json, read by blender/railway/rail_kit.py for the UVs. Kinds:
  hang_<id>   a hanging poster (中吊り), 1008 x 720, two to a hanger over the aisle
  frame_<id>  a framed card over the windows (窓上), 1008 x 362
  route       the line's map over the end door, 1008 x 256
  sticker_*   door, priority seat, no smoking, emergency stickers; plate_* the car number and maker's plates
"""
import argparse
import json
import os

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, ".."))
ART = os.path.join(REPO, "design", "railway", "ads", "gen", "ads_a.png")
STR = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "Localization", "strings_%s.json")
OUT = os.path.join(REPO, "textures", "localized", "%s")
HEAD = os.path.join(REPO, "fonts", "DelaGothicOne-Regular.ttf")
BODY = os.path.join(REPO, "fonts", "MPLUS1p-Bold.ttf")
BODY_R = os.path.join(REPO, "fonts", "MPLUS1p-Regular.ttf")

# the art grid's cells in reading order, and the colour of each ad's band (Showa print colours)
ADS = ["beach", "cable", "aton", "kiosk", "bakery", "amber", "starlings", "lectures", "sprats", "radio", "festival", "bath"]
BAND = {
    "beach": (24, 86, 142), "cable": (178, 38, 34), "aton": (62, 44, 30), "kiosk": (30, 70, 140), "bakery": (120, 52, 24),
    "amber": (22, 28, 74), "starlings": (176, 70, 32), "lectures": (34, 58, 46), "sprats": (28, 72, 120),
    "radio": (92, 50, 26), "festival": (18, 26, 62), "bath": (36, 96, 120),
}
PAPER = (240, 232, 214)
INK = (32, 28, 24)


def strings(lang):
    with open(STR % lang, encoding="utf-8") as f:
        return json.load(f)


def cell(i):
    g = Image.open(ART).convert("RGB")
    w, h = g.size
    r, c = divmod(i, 4)
    m = 4
    return g.crop((c * w // 4 + m, r * h // 3 + m, (c + 1) * w // 4 - m, (r + 1) * h // 3 - m))


def fit(draw, text, path, box_w, box_h, start):
    """The biggest size (from start down) at which text fits box_w x box_h on one line."""
    size = start
    while size > 10:
        f = ImageFont.truetype(path, size)
        x0, y0, x1, y1 = draw.textbbox((0, 0), text, font=f)
        if x1 - x0 <= box_w and y1 - y0 <= box_h:
            return f
        size -= 2
    return ImageFont.truetype(path, size)


def wrap2(draw, text, path, box_w, box_h, start):
    """One line if it fits big enough, else two lines split at the space nearest the middle."""
    f = fit(draw, text, path, box_w, box_h, start)
    if f.size >= start * 0.72 or " " not in text:
        return [text], f
    words = text.split(" ")
    best = None
    for k in range(1, len(words)):
        a, b = " ".join(words[:k]), " ".join(words[k:])
        d = abs(len(a) - len(b))
        if best is None or d < best[0]:
            best = (d, a, b)
    lines = [best[1], best[2]]
    f = min((fit(draw, s, path, box_w, box_h / 2 - 2, start) for s in lines), key=lambda x: x.size)
    return lines, f


def paper(img, amount=10):
    """A print's paper: a little grain and a soft vignette, so it doesn't read as a screen."""
    import random
    rnd = random.Random(7)
    noise = Image.effect_noise(img.size, 18).convert("L").filter(ImageFilter.GaussianBlur(0.6))
    tone = Image.merge("RGB", [noise] * 3)
    img = Image.blend(img, Image.composite(tone, img, Image.new("L", img.size, amount)), 0.5)
    vig = Image.new("L", img.size, 0)
    d = ImageDraw.Draw(vig)
    w, h = img.size
    d.rectangle((0, 0, w, h), fill=255)
    d.rectangle((w * 0.04, h * 0.06, w * 0.96, h * 0.94), fill=0)
    vig = vig.filter(ImageFilter.GaussianBlur(min(w, h) * 0.08))
    dark = Image.new("RGB", img.size, (60, 48, 36))
    _ = rnd
    return Image.composite(dark, img, vig.point(lambda v: int(v * 0.28)))


def centred(draw, lines, f, cx, cy, fill, spacing=1.12):
    hs = [draw.textbbox((0, 0), s, font=f) for s in lines]
    total = sum(b[3] - b[1] for b in hs) * spacing
    y = cy - total / 2
    for s, b in zip(lines, hs):
        draw.text((cx - (b[2] - b[0]) / 2 - b[0], y - b[1]), s, font=f, fill=fill)
        y += (b[3] - b[1]) * spacing


def hang(i, aid, S):
    W, H = 1008, 720
    img = Image.new("RGB", (W, H), PAPER)
    art = cell(i)
    aw, ah = W - 40, int(H * 0.66)
    sc = max(aw / art.width, ah / art.height)
    art = art.resize((int(art.width * sc) + 1, int(art.height * sc) + 1), Image.LANCZOS)
    art = art.crop(((art.width - aw) // 2, (art.height - ah) // 2, (art.width - aw) // 2 + aw, (art.height - ah) // 2 + ah))
    img.paste(art, (20, 20))
    d = ImageDraw.Draw(img)
    band = BAND[aid]
    y0 = 20 + ah + 10
    d.rectangle((20, y0, W - 20, H - 20), fill=band)
    title, line = S["poster.train.%s.title" % aid], S["poster.train.%s.line" % aid]
    tl, tf = wrap2(d, title, HEAD, W - 100, (H - 20 - y0) * 0.56, 78)
    centred(d, tl, tf, W / 2, y0 + (H - 20 - y0) * 0.36, PAPER)
    lf = fit(d, line, BODY, W - 120, (H - 20 - y0) * 0.26, 40)
    centred(d, [line], lf, W / 2, y0 + (H - 20 - y0) * 0.80, (236, 214, 160))
    return paper(img)


def frame(i, aid, S):
    W, H = 1008, 362
    img = Image.new("RGB", (W, H), PAPER)
    art = cell(i)
    aw, ah = int(W * 0.42), H - 28
    sc = max(aw / art.width, ah / art.height)
    art = art.resize((int(art.width * sc) + 1, int(art.height * sc) + 1), Image.LANCZOS)
    art = art.crop(((art.width - aw) // 2, (art.height - ah) // 2, (art.width - aw) // 2 + aw, (art.height - ah) // 2 + ah))
    img.paste(art, (14, 14))
    d = ImageDraw.Draw(img)
    band = BAND[aid]
    x0 = 14 + aw + 18
    d.rectangle((x0 - 4, 14, W - 14, 26), fill=band)
    d.rectangle((x0 - 4, H - 26, W - 14, H - 14), fill=band)
    title, line = S["poster.train.%s.title" % aid], S["poster.train.%s.line" % aid]
    bw = W - 14 - x0 - 20
    tl, tf = wrap2(d, title, HEAD, bw, (H - 60) * 0.55, 64)
    centred(d, tl, tf, x0 + bw / 2 + 6, 40 + (H - 60) * 0.32, band)
    ll, lf = wrap2(d, line, BODY, bw, (H - 60) * 0.32, 34)
    centred(d, ll, lf, x0 + bw / 2 + 6, 40 + (H - 60) * 0.80, INK)
    return paper(img, 8)


def route(S):
    W, H = 1008, 256
    img = Image.new("RGB", (W, H), (246, 244, 236))
    d = ImageDraw.Draw(img)
    green, orange = (24, 80, 56), (226, 114, 32)
    d.rectangle((0, 0, W, 64), fill=green)
    tf = fit(d, S["sign.train.route_title"], HEAD, W - 60, 48, 46)
    centred(d, [S["sign.train.route_title"]], tf, W / 2, 32, (250, 248, 240))
    stops = ["rail.stop.tunnel", "rail.stop.beach", "rail.stop.island", "rail.stop.far"]
    xs = [140, 390, 640, 880]
    y = 140
    d.line((xs[0], y, xs[-1], y), fill=green, width=12)
    for k, x in zip(stops, xs):
        d.ellipse((x - 18, y - 18, x + 18, y + 18), fill=(250, 248, 240), outline=green, width=7)
        f = fit(d, S[k], BODY, 220, 52, 44)
        centred(d, [S[k]], f, x, y + 62, INK)
    # the sea under the bank: a few waves between the beach and the island
    for x in range(420, 620, 28):
        d.arc((x, y + 18, x + 28, y + 34), 200, 340, fill=(60, 120, 170), width=3)
    d.rectangle((0, H - 10, W, H), fill=orange)
    return img


def sticker(kind, S):
    if kind == "fingers":
        W, H = 512, 512
        img = Image.new("RGB", (W, H), (250, 206, 40))
        d = ImageDraw.Draw(img)
        d.rounded_rectangle((10, 10, W - 10, H - 10), 40, outline=(30, 26, 20), width=10)
        # a hand by the door's edge, drawn: a mitten of a palm and fingers caught between two leaves
        d.rectangle((W / 2 - 8, 60, W / 2 + 8, 300), fill=(30, 26, 20))
        d.rounded_rectangle((120, 150, W / 2 - 14, 270), 30, fill=(250, 236, 220), outline=(30, 26, 20), width=6)
        for k in range(4):
            yy = 160 + k * 28
            d.rounded_rectangle((W / 2 - 40, yy, W / 2 + 46, yy + 22), 11, fill=(250, 236, 220), outline=(30, 26, 20), width=5)
        d.line((W / 2 + 60, 140, W / 2 + 110, 120), fill=(200, 30, 20), width=8)
        d.line((W / 2 + 60, 210, W / 2 + 120, 210), fill=(200, 30, 20), width=8)
        d.line((W / 2 + 60, 280, W / 2 + 110, 300), fill=(200, 30, 20), width=8)
        tl, tf = wrap2(d, S["sign.train.fingers"], HEAD, W - 70, 150, 64)
        centred(d, tl, tf, W / 2, 400, (30, 26, 20))
        return img
    if kind == "priority":
        W, H = 1008, 256
        img = Image.new("RGB", (W, H), (236, 120, 40))
        d = ImageDraw.Draw(img)
        d.rounded_rectangle((8, 8, W - 8, H - 8), 26, outline=(255, 248, 236), width=8)
        # three figures: an old man with a stick, a woman with a child, a person with a crutch (simple shapes)
        for k, x in enumerate((70, 160, 250)):
            d.ellipse((x - 18, 52, x + 18, 88), fill=(255, 248, 236))
            d.rounded_rectangle((x - 22, 92, x + 22, 170), 12, fill=(255, 248, 236))
            d.rectangle((x - 16, 168, x - 4, 210), fill=(255, 248, 236))
            d.rectangle((x + 4, 168, x + 16, 210), fill=(255, 248, 236))
            if k == 0:
                d.line((x + 30, 120, x + 40, 210), fill=(255, 248, 236), width=8)
            if k == 1:
                d.ellipse((x + 24, 120, x + 44, 140), fill=(255, 248, 236))
        tl, tf = wrap2(d, S["sign.train.priority"], HEAD, W - 370, 180, 72)
        centred(d, tl, tf, 330 + (W - 350) / 2, H / 2, (255, 248, 236))
        return img
    if kind == "no_smoking":
        W, H = 512, 512
        img = Image.new("RGB", (W, H), (250, 250, 246))
        d = ImageDraw.Draw(img)
        d.ellipse((40, 30, W - 40, H - 110), outline=(200, 30, 26), width=34)
        d.rectangle((140, 200, 360, 240), fill=(40, 36, 30))
        d.rectangle((360, 200, 384, 240), fill=(226, 120, 40))
        for k in range(3):
            d.arc((360 + k * 6, 130 - k * 24, 400 + k * 6, 200 - k * 24), 180, 360, fill=(120, 120, 120), width=5)
        d.line((112, 92, W - 112, H - 172), fill=(200, 30, 26), width=34)
        f = fit(d, S["sign.train.no_smoking"], HEAD, W - 60, 80, 64)
        centred(d, [S["sign.train.no_smoking"]], f, W / 2, H - 56, (40, 36, 30))
        return img
    if kind == "emergency":
        W, H = 512, 256
        img = Image.new("RGB", (W, H), (200, 30, 26))
        d = ImageDraw.Draw(img)
        d.rounded_rectangle((8, 8, W - 8, H - 8), 20, outline=(255, 248, 236), width=6)
        d.ellipse((30, 70, 140, 180), fill=(255, 248, 236))
        d.ellipse((58, 98, 112, 152), fill=(200, 30, 26))
        tl, tf = wrap2(d, S["sign.train.emergency"], HEAD, W - 190, 190, 44)
        centred(d, tl, tf, 160 + (W - 170) / 2, H / 2, (255, 248, 236))
        return img
    if kind == "lean":
        W, H = 1008, 160
        img = Image.new("RGB", (W, H), (250, 250, 246))
        d = ImageDraw.Draw(img)
        d.rectangle((0, 0, 30, H), fill=(226, 114, 32))
        f = fit(d, S["sign.train.door_hand"], BODY, W - 90, 90, 64)
        centred(d, [S["sign.train.door_hand"]], f, W / 2 + 15, H / 2, (40, 36, 30))
        return img
    raise ValueError(kind)


def plate(kind, S):
    W, H = 512, 160
    if kind == "number":
        img = Image.new("RGB", (W, H), (24, 70, 50))
        d = ImageDraw.Draw(img)
        d.rounded_rectangle((6, 6, W - 6, H - 6), 14, outline=(214, 206, 180), width=6)
        f = fit(d, S["sign.train.car_number"], HEAD, W - 60, H - 50, 100)
        centred(d, [S["sign.train.car_number"]], f, W / 2, H / 2, (240, 234, 214))
        return img
    img = Image.new("RGB", (W, H), (176, 150, 92))       # the maker's cast brass plate
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((6, 6, W - 6, H - 6), 30, outline=(96, 76, 40), width=8)
    for x in (30, W - 30):
        d.ellipse((x - 9, H / 2 - 9, x + 9, H / 2 + 9), fill=(96, 76, 40))
    tl, tf = wrap2(d, S["sign.train.maker"], BODY, W - 110, H - 50, 52)
    centred(d, tl, tf, W / 2, H / 2, (70, 54, 26))
    return img


def build(lang):
    S = strings(lang)
    out = OUT % lang
    os.makedirs(out, exist_ok=True)
    made = {}
    for i, aid in enumerate(ADS):
        made["hang_" + aid] = hang(i, aid, S)
        made["frame_" + aid] = frame(i, aid, S)
    made["route"] = route(S)
    for k in ("fingers", "priority", "no_smoking", "emergency", "lean"):
        made["sticker_" + k] = sticker(k, S)
    for k in ("number", "maker"):
        made["plate_" + k] = plate(k, S)
    atlas, rects = pack(made)
    atlas.save(os.path.join(out, "train_atlas.jpg"), quality=90)
    with open(os.path.join(REPO, "design", "railway", "ads", "atlas.json"), "w") as f:
        json.dump({"_doc": "Made by tools/train_ads.py: each print's rectangle in the atlas, pixels from the top left "
                   "(x0, y0, x1, y1) of a %d x %d image." % atlas.size, "size": list(atlas.size), "prints": rects}, f, indent=1)
    return made


ATLAS = 4096
PAD = 8


def pack(made):
    """Shelves, in a fixed order (the same rectangles in every language); a few pixels between prints so mips don't
    bleed one into the next."""
    atlas = Image.new("RGB", (ATLAS, ATLAS), (128, 128, 128))
    rects = {}
    x = y = shelf = 0
    for name, img in sorted(made.items(), key=lambda kv: (-kv[1].size[1], list(made).index(kv[0]))):
        w, h = img.size
        if x + w > ATLAS:
            x, y, shelf = 0, y + shelf + PAD, 0
        if y + h > ATLAS:
            raise SystemExit("train_ads: the atlas is full at " + name)
        atlas.paste(img, (x, y))
        rects[name] = [x, y, x + w, y + h]
        x += w + PAD
        shelf = max(shelf, h)
    return atlas, rects


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lang", default="ru,en,ja")
    ap.add_argument("--sheet", help="a contact sheet of the first language's prints")
    a = ap.parse_args()
    first = None
    for lang in a.lang.split(","):
        m = build(lang)
        first = first or m
        print("train_ads: %s %d prints" % (lang, len(m)))
    if a.sheet:
        names = list(first)
        cols, cw, ch = 5, 400, 300
        sheet = Image.new("RGB", (cols * cw, ((len(names) + cols - 1) // cols) * ch), (60, 60, 60))
        for k, n in enumerate(names):
            im = first[n].copy()
            im.thumbnail((cw - 10, ch - 10))
            sheet.paste(im, ((k % cols) * cw + 5, (k // cols) * ch + 5))
        sheet.save(a.sheet, quality=85)


if __name__ == "__main__":
    main()
