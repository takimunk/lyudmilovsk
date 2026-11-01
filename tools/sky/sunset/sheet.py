#!/usr/bin/env python3
"""Builds the sunset comparison sheet from an offscreen run of unreal/scripts/shoot_sunset.py.

  tools/sky/sunset/sheet.py <log> <screenshot dir> <out.png> [tile width]

The log's SUNSETSHOT lines name the shots in order; the screenshots written after the run started are taken in the
same order (by modification time). Shots named row__col go into a grid (rows: profiles, columns: weathers); the rest
(town views, sunrises) into a strip under it. Each tile is captioned with the profile the game printed.
"""
import os
import re
import sys

from PIL import Image, ImageDraw, ImageFont


def font(size):
    for f in ("/System/Library/Fonts/Supplemental/Arial.ttf", "/System/Library/Fonts/Helvetica.ttc"):
        if os.path.exists(f):
            return ImageFont.truetype(f, size)
    return ImageFont.load_default()


def main():
    log, shots_dir, out = sys.argv[1:4]
    tw = int(sys.argv[4]) if len(sys.argv) > 4 else 400
    th = tw * 9 // 16
    names, captions = [], {}
    for line in open(log, errors="replace"):
        m = re.search(r"SUNSETSHOT (\S+) \| (.*)", line)
        if m and m.group(1) not in ("calendar", "script"):
            names.append(m.group(1))
            s = re.search(r"SUNSET (sunset|sunrise) (\w+)( \(spectacular\))? hue ([\d.]+) int ([\d.]+) aero ([\d.]+)", m.group(2))
            a = re.search(r"air overcast ([\d.]+) broken ([\d.]+) clean ([\d.]+) haze ([\d.]+)", m.group(2))
            alt = re.search(r"sun alt (-?[\d.]+)", m.group(2))
            cap = "%s%s h%.2f i%.2f a%.2f" % (s.group(2), "*" if s.group(3) else "", float(s.group(4)), float(s.group(5)), float(s.group(6))) if s else "neutral"
            if a:
                cap += " | ov%.1f br%.1f cl%.1f hz%.1f" % tuple(float(v) for v in a.groups())
            if alt:
                cap += " | %.1f°" % float(alt.group(1))
            captions[m.group(1)] = cap
    # the run's screenshots: the last len(names) by time, oldest first
    files = sorted((os.path.join(shots_dir, f) for f in os.listdir(shots_dir) if f.lower().endswith(".png")), key=os.path.getmtime)
    files = files[-len(names):]
    if len(files) != len(names):
        print("only %d screenshots for %d shots" % (len(files), len(names)))
    shot = dict(zip(names, files))

    rows, cols, extra = [], [], []
    for n in names:
        if "__" in n and not n.startswith(("town_", "sunrise_")):
            r, c = n.split("__")
            rows.append(r) if r not in rows else None
            cols.append(c) if c not in cols else None
        else:
            extra.append(n)
    lw, hh, cap_h = 150, 34, 18
    per = max(1, len(cols))
    ex_rows = (len(extra) + per - 1) // per
    W = lw + per * tw
    H = hh + len(rows) * (th + cap_h) + (hh + ex_rows * (th + cap_h) if extra else 0)
    sheet = Image.new("RGB", (W, H), (18, 18, 20))
    d = ImageDraw.Draw(sheet)
    f_big, f_small = font(20), font(12)
    for j, c in enumerate(cols):
        d.text((lw + j * tw + 8, 8), c.replace("+", " +").replace("-", " -") + "°", fill=(230, 230, 230), font=f_big)

    def tile(n, x, y):
        if n in shot:
            im = Image.open(shot[n]).convert("RGB").resize((tw, th), Image.LANCZOS)
            sheet.paste(im, (x, y))
        d.text((x + 4, y + th + 2), captions.get(n, ""), fill=(200, 200, 200), font=f_small)

    for i, r in enumerate(rows):
        y = hh + i * (th + cap_h)
        d.text((8, y + th // 2 - 10), r, fill=(240, 220, 200), font=f_big)
        for j, c in enumerate(cols):
            tile(r + "__" + c, lw + j * tw, y)
    if extra:
        y0 = hh + len(rows) * (th + cap_h)
        d.text((8, y0 + 8), "town (across the light) and sunrises", fill=(230, 230, 230), font=f_big)
        for k, n in enumerate(extra):
            x = lw + (k % per) * tw
            y = y0 + hh + (k // per) * (th + cap_h)
            tile(n, x, y)
            d.text((x + 4, y + 4), n.split("__")[0], fill=(255, 255, 255), font=f_small)
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    sheet.save(out)
    print("sheet %s: %d rows x %d cols + %d more, %dx%d" % (out, len(rows), len(cols), len(extra), W, H))


if __name__ == "__main__":
    main()
