"""The smoking kit's pictures (docs/smoking.md), cut out of two Higgsfield grids and laid out for the game:

  design/smoking/pack_art_grid.png  4 printed panels without words (front, back, side, lid)  -> the pack's atlas
  design/smoking/icons_grid.png     2x2 inventory icons (pack, lighter, cigarette, butt)    -> textures/localized/ru/icon_<id>.jpg

The atlas (textures/localized/<lang>/cig_pack.png, 2048 px) follows the UV regions of blender/props/smoking/build_smoking.py
(PACK_UV, read from there): the art on each panel, then the words of strings_<lang>.json printed with the project's fonts
(label.cigarettes.brand, .warning in a framed white box on the front and the back, .count on the side), so the warning is
spelled exactly in every language. The art carries no words at all, so it is shared by the three languages.

  python3 tools/smoking_pack.py [lang ...]      (default ru en ja; the icons only once, for ru: they have no words)
  python3 tools/smoking_pack.py --uv            also draws the UV polygons (export/pack_uv.json) over the ru atlas
                                                -> blender/props/smoking/export/pack_uv_check.png
Then unreal/scripts/import_smoking.py (textures) and unreal/scripts/import_hall_assets.py (icons), tools/ui_diary_icons.py (sketches).
"""
import ast
import json
import os
import sys
import urllib.request

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DESIGN = os.path.join(ROOT, "design", "smoking")
STRINGS = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Localization", "strings_%s.json")
BUILD = os.path.join(ROOT, "blender", "props", "smoking", "build_smoking.py")
FONTS = os.path.join(ROOT, "fonts")
SIZE = 2048
BLUE = (22, 44, 86)
GOLD = (176, 138, 62)
INK = (20, 20, 20)


def pack_uv():
    """PACK_UV and the pack's sizes from the Blender builder (one source of truth), evaluated from its constants."""
    src = open(BUILD, encoding="utf-8").read()
    env = {"MM": 0.001}
    names = ("CIG_R", "CIG_L", "FILTER", "TIP", "PITCH", "WALL", "GAP", "COLLAR", "IN_W", "IN_D", "W", "D", "H", "K")
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.Assign) and isinstance(node.targets[0], ast.Name) and node.targets[0].id in names + ("PACK_UV",):
            exec(compile(ast.Module([node], []), BUILD, "exec"), env)
    return env["PACK_UV"]


def grid(name):
    png = os.path.join(DESIGN, name + ".png")
    with open(os.path.join(DESIGN, name + ".json"), encoding="utf-8") as f:
        spec = json.load(f)
    if not os.path.exists(png):
        print("downloading", spec["output"])
        urllib.request.urlretrieve(spec["output"], png)
    return spec, Image.open(png).convert("RGB")


def box(region):
    """A UV region (v up) as a pixel box (y down) in the atlas."""
    u0, v0, u1, v1 = region
    return (round(u0 * SIZE), round((1 - v1) * SIZE), round(u1 * SIZE), round((1 - v0) * SIZE))


def font(name, px):
    return ImageFont.truetype(os.path.join(FONTS, name), px)


def chunks(text):
    out, cur = [], ""
    for ch in text:
        cur += ch
        if ch in "はをにがでと、。":
            out.append(cur)
            cur = ""
    return out + ([cur] if cur else [])


def fit_text(draw, text, name, max_w, max_h, start):
    """The largest size (from start down) at which text fits in max_w x max_h, wrapped onto lines at spaces if needed."""
    px = start
    while px > 8:
        f = font(name, px)
        # text without spaces (Japanese) breaks after its particles, not inside a word
        words = text.split(" ") if " " in text else chunks(text)
        joiner = " " if " " in text else ""
        lines, cur = [], ""
        for w in words:
            t = (cur + joiner + w) if cur else w
            if draw.textlength(t, font=f) <= max_w:
                cur = t
            else:
                if cur:
                    lines.append(cur)
                cur = w
        lines.append(cur)
        h = len(lines) * px * 1.18
        if h <= max_h and all(draw.textlength(l, font=f) <= max_w for l in lines):
            return f, lines, px
        px -= 2
    return font(name, 8), [text], 8


def draw_lines(draw, f, lines, px, cx, cy, fill):
    h = len(lines) * px * 1.18
    y = cy - h / 2
    for l in lines:
        draw.text((cx, y + px * 0.59), l, font=f, fill=fill, anchor="mm")
        y += px * 1.18


def warning_box(draw, rect, text):
    """A white box with a thick black frame and the warning in black, as packs print it."""
    x0, y0, x1, y1 = rect
    frame = max(4, (x1 - x0) // 45)
    draw.rectangle(rect, fill=(250, 250, 246))
    draw.rectangle(rect, outline=INK, width=frame)
    pad = frame * 3
    f, lines, px = fit_text(draw, text, "MPLUS1p-Bold.ttf", x1 - x0 - 2 * pad, y1 - y0 - 2 * pad, (y1 - y0) // 3)
    draw_lines(draw, f, lines, px, (x0 + x1) / 2, (y0 + y1) / 2, INK)


def atlas(lang, art):
    uv = pack_uv()
    with open(STRINGS % lang, encoding="utf-8") as f:
        words = json.load(f)
    w, h = art.size
    # the four cells of the art grid (not equal widths: found by eye on the generated sheet, see the sidecar)
    spec = json.load(open(os.path.join(DESIGN, "pack_art_grid.json"), encoding="utf-8"))
    cells = {k: tuple(round(c * (w if i % 2 == 0 else h)) for i, c in enumerate(v)) for k, v in spec["cells"].items()}
    img = Image.new("RGB", (SIZE, SIZE), (236, 226, 204))
    d = ImageDraw.Draw(img)

    def paste(region, cell, crop=None):
        b = box(uv[region])
        src = art.crop(cells[cell])
        if crop:
            sw, sh = src.size
            src = src.crop((round(crop[0] * sw), round(crop[1] * sh), round(crop[2] * sw), round(crop[3] * sh)))
        img.paste(src.resize((b[2] - b[0], b[3] - b[1]), Image.LANCZOS), b[:2])
        return b

    # the inside of the box and its rims: plain card (a piece of the lid's cell away from its emblem)
    paste("inner", "lid", (0.05, 0.02, 0.95, 0.25))
    paste("top", "lid", (0.0, 0.32, 1.0, 0.68))
    paste("bottom", "lid", (0.0, 0.75, 1.0, 0.98))
    front = paste("front", "front")
    back = paste("back", "back")
    side_r = paste("side_r", "side")
    side_l = paste("side_l", "side")
    # front: the name under the bird, the warning at the bottom (a third of the panel)
    fx0, fy0, fx1, fy1 = front
    fw, fh = fx1 - fx0, fy1 - fy0
    brand = words["label.cigarettes.brand"]
    f, lines, px = fit_text(d, brand, "DelaGothicOne-Regular.ttf", fw * 0.78, fh * 0.1, int(fh * 0.075))
    draw_lines(d, f, lines, px, fx0 + fw / 2, fy0 + fh * 0.6, BLUE)
    warning_box(d, (fx0 + round(fw * 0.06), fy0 + round(fh * 0.67), fx1 - round(fw * 0.06), fy1 - round(fh * 0.035)), words["label.cigarettes.warning"])
    # back: the warning again, bigger, in the calm blue middle
    bx0, by0, bx1, by1 = back
    bw, bh = bx1 - bx0, by1 - by0
    warning_box(d, (bx0 + round(bw * 0.07), by0 + round(bh * 0.40), bx1 - round(bw * 0.07), by0 + round(bh * 0.72)), words["label.cigarettes.warning"])
    # the sides: how many and what, written up the side
    for b in (side_r, side_l):
        sx0, sy0, sx1, sy1 = b
        sw, sh = sx1 - sx0, sy1 - sy0
        txt = Image.new("RGBA", (round(sh * 0.62), sw), (0, 0, 0, 0))
        td = ImageDraw.Draw(txt)
        f, lines, px = fit_text(td, words["label.cigarettes.count"], "MPLUS1p-Bold.ttf", txt.width * 0.95, txt.height * 0.36, int(sw * 0.3))
        draw_lines(td, f, lines, px, txt.width / 2, txt.height * 0.30, (236, 226, 204))
        txt = txt.rotate(90, expand=True)
        img.paste(txt, (sx0, sy0 + round(sh * 0.30)), txt)
    out = os.path.join(ROOT, "textures", "localized", lang)
    os.makedirs(out, exist_ok=True)
    path = os.path.join(out, "cig_pack.png")
    img.save(path, optimize=True)
    with open(os.path.join(out, "cig_pack.json"), "w", encoding="utf-8") as f:
        json.dump({"id": "cig_pack", "lang": lang, "art": "design/smoking/pack_art_grid.png (higgsfield, no words)",
                   "words": ["label.cigarettes.brand", "label.cigarettes.warning", "label.cigarettes.count"],
                   "fonts": ["DelaGothicOne-Regular.ttf", "MPLUS1p-Bold.ttf"], "made_by": "tools/smoking_pack.py"}, f, indent=1, ensure_ascii=False)
    print(lang, "atlas", os.path.relpath(path, ROOT))
    return img


def icons():
    spec, img = grid("icons_grid")
    out = os.path.join(ROOT, "textures", "localized", "ru")
    w, h = img.size
    for i, name in enumerate(spec["ids"]):
        if name not in spec["ship"]:
            continue
        c, r = i % 2, i // 2
        cell = img.crop((c * w // 2, r * h // 2, (c + 1) * w // 2, (r + 1) * h // 2))
        m = int(cell.width * 0.02)
        cell = cell.crop((m, m, cell.width - m, cell.height - m)).resize((512, 512), Image.LANCZOS)
        cell.save(os.path.join(out, "icon_%s.jpg" % name), quality=90)
        with open(os.path.join(out, "icon_%s.json" % name), "w", encoding="utf-8") as f:
            json.dump({"id": "icon_" + name, "lang": "ru", "service": "higgsfield", "grid": "design/smoking/icons_grid.png", "cell": [c, r]}, f, indent=1)
        print("icon", name)


def small_textures():
    """The cigarette's own little pictures, drawn here (no generation): the cork-pattern filter paper, the ember and ash, a soft
    puff of smoke (alpha) -> textures/smoking/*.png."""
    import random
    out = os.path.join(ROOT, "textures", "smoking")
    os.makedirs(out, exist_ok=True)
    rnd = random.Random(1951)
    # filter: warm cork orange with darker speckles; U round it, V along it; a thin gold line where the tipping paper ends (top)
    w = h = 256
    img = Image.new("RGB", (w, h), (196, 122, 52))
    d = ImageDraw.Draw(img)
    for _ in range(2600):
        x, y = rnd.randrange(w), rnd.randrange(h)
        r = rnd.choice((1, 1, 1, 2))
        c = rnd.choice(((150, 84, 30), (170, 98, 40), (214, 150, 80), (128, 70, 26)))
        for dx in (-w, 0, w):
            d.ellipse((x + dx - r, y - r, x + dx + r, y + r), fill=c)
    d.rectangle((0, 6, w, 10), fill=(212, 178, 96))
    img.save(os.path.join(out, "cig_filter.png"))
    # ember: R = char (0 ash .. 1 black), G = where it glows (cracks in the ash)
    img = Image.new("RGB", (w, h), (60, 0, 0))
    d = ImageDraw.Draw(img)
    for _ in range(900):
        x, y, r = rnd.randrange(w), rnd.randrange(h), rnd.randrange(2, 7)
        g = rnd.randrange(120, 255)
        d.ellipse((x - r, y - r, x + r, y + r), fill=(rnd.randrange(80, 255), g, 0))
    img.save(os.path.join(out, "cig_ember.png"))
    # a puff: a soft round cloud of overlapping blobs fading to the edge (alpha), white
    n = 128
    px = Image.new("L", (n, n), 0)
    d = ImageDraw.Draw(px)
    for _ in range(70):
        a = rnd.random() * 6.283
        rr = rnd.random() ** 0.7 * n * 0.28
        x, y = n / 2 + rr * __import__("math").cos(a), n / 2 + rr * __import__("math").sin(a)
        r = rnd.uniform(n * 0.08, n * 0.2)
        d.ellipse((x - r, y - r, x + r, y + r), fill=rnd.randrange(25, 60))
    from PIL import ImageFilter, ImageChops
    px = px.filter(ImageFilter.GaussianBlur(n * 0.06))
    mask = Image.radial_gradient("L").resize((n, n)).point(lambda v: max(0, 255 - int(v * 1.25)))
    px = ImageChops.multiply(px.point(lambda v: min(255, v * 3)), mask)
    puff = Image.merge("RGBA", (Image.new("L", (n, n), 255),) * 3 + (px,))
    puff.save(os.path.join(out, "smoke_puff.png"))
    print("small textures", os.path.relpath(out, ROOT))


def uv_check(img):
    data = json.load(open(os.path.join(ROOT, "blender", "props", "smoking", "export", "pack_uv.json")))
    im = img.copy()
    d = ImageDraw.Draw(im)
    colours = {"SM_CigPackBody": (255, 0, 0), "SM_CigPackLid": (0, 200, 0), "SM_CigPackCollar": (0, 120, 255)}
    for name, polys in data.items():
        for p in polys:
            pts = [(u * SIZE, (1 - v) * SIZE) for u, v in p]
            d.line(pts + pts[:1], fill=colours.get(name, (255, 0, 255)), width=3)
    path = os.path.join(ROOT, "blender", "props", "smoking", "export", "pack_uv_check.png")
    im.resize((1024, 1024), Image.LANCZOS).save(path)
    print("uv check", os.path.relpath(path, ROOT))


def main(args):
    langs = [a for a in args if not a.startswith("--")] or ["ru", "en", "ja"]
    _, art = grid("pack_art_grid")
    first = None
    for lang in langs:
        img = atlas(lang, art)
        first = first or img
    if "ru" in langs:
        icons()
    if "--uv" in args:
        uv_check(first)
    small_textures()


if __name__ == "__main__":
    main(sys.argv[1:])
