"""Draws the coastal food web (docs/ecosystem.md) as a picture: who eats whom, by trophic level.

    python3 tools/underwater/foodweb.py [--lang ru|en|ja] [out.png]

Builds and runs tools/underwater/run.sh eco -web for the web (nodes: trophic level, base biomass; links: base flows),
names from the localization tables (fish.<id>.name, eco.<id>.name). Circles: area ~ biomass (log), arrows prey -> predator,
width ~ the flow.
"""
import json
import math
import os
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))


def main():
    args = sys.argv[1:]
    lang = "ru"
    if "--lang" in args:
        i = args.index("--lang")
        lang = args[i + 1]
        del args[i:i + 2]
    out = args[0] if args else os.path.join(ROOT, "Saved", "Underwater", "foodweb_%s.png" % lang)
    txt = subprocess.run(["sh", os.path.join(ROOT, "tools", "underwater", "run.sh"), "eco", "-web"], capture_output=True, text=True, check=True).stdout
    nodes, links = {}, []
    for line in txt.splitlines():
        p = line.split()
        if p and p[0] == "N":
            nodes[p[1]] = dict(tl=float(p[2]), b=float(p[3]))
        elif p and p[0] == "L":
            links.append((p[1], p[2], float(p[3])))
    names = json.load(open(os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Localization", "strings_%s.json" % lang), encoding="utf-8"))

    def name(i):
        return names.get("fish.%s.name" % i.lower()) or names.get("eco.%s.name" % i.lower()) or i

    W, H, M = 3000, 1900, 120
    # rows by trophic level (bands of 0.45), ordered within a row by where their food is (a few barycentre sweeps)
    rows = {}
    for i, n in nodes.items():
        rows.setdefault(int(round((n["tl"] - 1.0) / 0.45)), []).append(i)
    levels = sorted(rows)
    tl_max = max(n["tl"] for n in nodes.values())
    x = {}
    for r in levels:
        for k, i in enumerate(sorted(rows[r])):
            x[i] = (k + 0.5) / len(rows[r])
    prey_of = {}
    for pred, prey, q in links:
        prey_of.setdefault(pred, []).append((prey, q))
    eaten_by = {}
    for pred, prey, q in links:
        eaten_by.setdefault(prey, []).append((pred, q))
    for sweep in range(8):
        for r in (levels if sweep % 2 == 0 else levels[::-1]):
            def bary(i):
                nb = prey_of.get(i, []) + (eaten_by.get(i, []) if sweep % 2 else [])
                tot = sum(math.sqrt(q) for _, q in nb)
                return sum(x[j] * math.sqrt(q) for j, q in nb) / tot if tot else x[i]
            order = sorted(rows[r], key=bary)
            for k, i in enumerate(order):
                x[i] = (k + 0.5) / len(order)
    pos = {}
    for i, n in nodes.items():
        y = H - M - (n["tl"] - 1.0) / (tl_max - 1.0) * (H - 2 * M - 60)
        pos[i] = (M + x[i] * (W - 2 * M), y)

    img = Image.new("RGB", (W, H), (18, 32, 44))
    d = ImageDraw.Draw(img, "RGBA")
    font_path = os.path.join(ROOT, "fonts", "MPLUS1p-Regular.ttf")
    font = ImageFont.truetype(font_path, 26)
    small = ImageFont.truetype(font_path, 22)
    big = ImageFont.truetype(os.path.join(ROOT, "fonts", "MPLUS1p-Bold.ttf"), 44)
    titles = {"ru": "Кто кого ест у берега Людмиловска", "en": "Who eats whom off Lyudmilovsk", "ja": "リュドミロフスク沿岸の食物網"}
    d.text((M, 30), titles.get(lang, titles["en"]), font=big, fill=(230, 236, 240))
    # trophic level guides
    for t in range(1, int(tl_max) + 1):
        y = H - M - (t - 1.0) / (tl_max - 1.0) * (H - 2 * M - 60)
        d.line([(40, y), (W - 40, y)], fill=(255, 255, 255, 22), width=2)
        d.text((40, y - 30), "%d" % t, font=small, fill=(150, 170, 185))

    def colour(i):
        tl = nodes[i]["tl"]
        if tl < 1.05:
            return (110, 190, 90) if i != "Detritus" else (150, 120, 90)
        t = min(1.0, (tl - 2.0) / 2.2)
        return (int(80 + 175 * t), int(170 - 60 * t), int(220 - 150 * t))

    qmax = max(q for _, _, q in links)
    for pred, prey, q in sorted(links, key=lambda l: l[2]):
        (x0, y0), (x1, y1) = pos[prey], pos[pred]
        w = 1 + 9 * math.sqrt(q / qmax)
        a = int(70 + 150 * math.sqrt(q / qmax))
        c = colour(prey)
        d.line([(x0, y0), (x1, y1)], fill=(c[0], c[1], c[2], a), width=int(w))
        # a small arrowhead short of the predator
        ang = math.atan2(y1 - y0, x1 - x0)
        r = 14 + 3 * math.log(1 + 40 * nodes[pred]["b"])
        hx, hy = x1 - math.cos(ang) * r, y1 - math.sin(ang) * r
        s = 8 + w
        d.polygon([(hx, hy), (hx - s * math.cos(ang - 0.4), hy - s * math.sin(ang - 0.4)), (hx - s * math.cos(ang + 0.4), hy - s * math.sin(ang + 0.4))],
                  fill=(c[0], c[1], c[2], min(255, a + 40)))
    boxes = []
    for i, (px, py) in pos.items():
        r = 14 + 3 * math.log(1 + 40 * nodes[i]["b"])
        c = colour(i)
        d.ellipse([px - r, py - r, px + r, py + r], fill=c + (255,), outline=(240, 240, 240), width=2)
    for i, (px, py) in sorted(pos.items(), key=lambda kv: kv[1][0]):
        r = 14 + 3 * math.log(1 + 40 * nodes[i]["b"])
        label = name(i)
        tw = d.textlength(label, font=font)
        # under the circle, or over it when that would cover another label
        for top in (py + r + 4, py - r - 40, py + r + 40, py - r - 76):
            box = (px - tw / 2 - 6, top, px + tw / 2 + 6, top + 34)
            if not any(box[0] < b[2] and b[0] < box[2] and box[1] < b[3] and b[1] < box[3] for b in boxes):
                break
        boxes.append(box)
        d.rectangle(box, fill=(10, 20, 28, 190))
        d.text((px - tw / 2, box[1]), label, font=font, fill=(235, 240, 244))
    os.makedirs(os.path.dirname(out), exist_ok=True)
    img.save(out)
    print(out)


if __name__ == "__main__":
    main()
