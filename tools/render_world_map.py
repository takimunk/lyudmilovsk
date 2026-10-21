"""Top-down picture of the outdoor map (tools/world_map.py): relief, beach, sea, town, forests,
the path with its alley, the cable car and the starlings.

  python3 tools/render_world_map.py [out.png]
"""
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import world_map as W  # noqa: E402

PX = 1.5   # metres per pixel


def main(out):
    w = int((W.X_MAX - W.X_MIN) / PX)
    h = int((W.Y_MAX - W.Y_MIN) / PX)

    def px(x, y):   # west (+X) on the left, north (+Y) up
        return (W.X_MAX - x) / PX, (W.Y_MAX - y) / PX

    img = Image.new("RGB", (w, h))
    pix = img.load()
    for j in range(h):
        y = W.Y_MAX - (j + 0.5) * PX
        for i in range(w):
            x = W.X_MAX - (i + 0.5) * PX
            z = W.height(x, y)
            if z < W.SEA_Z:
                k = max(0.0, min(1.0, (W.SEA_Z - z) / 9.0))
                c = (int(90 - 50 * k), int(140 - 60 * k), int(170 - 40 * k))
            elif W.is_beach(x, y):
                c = (222, 205, 160)
            else:
                nx, ny, nz = W.normal(x, y, 1.5)
                shade = max(0.35, min(1.2, 0.75 + 0.9 * (nx * 0.5 + ny * 0.5)))
                t = max(0.0, min(1.0, (z - W.SEA_Z) / 70.0))
                base = (int(110 + 40 * t), int(150 + 20 * t), int(80 + 10 * t))
                c = tuple(min(255, int(v * shade)) for v in base)
            pix[i, j] = c
    d = ImageDraw.Draw(img)
    for x, y, hx, hy, mix, *_ in W.forests():     # forest patches (trees only where the ground allows)
        col = {"pine": (45, 85, 45), "dune": (90, 120, 70), "wet": (70, 105, 60), "town": (95, 130, 75)}[mix]
        d.rectangle((*px(x + hx, y + hy), *px(x - hx, y - hy)), outline=col, width=3)
    houses = W.town()
    for x, y, yaw, hw, hd, fl, kind in houses:
        ca, sa = math.cos(yaw), math.sin(yaw)
        pts = []
        for u, v in ((-hw / 2, -hd / 2), (hw / 2, -hd / 2), (hw / 2, hd / 2), (-hw / 2, hd / 2)):
            pts.append(px(x + u * ca - v * sa, y + u * sa + v * ca))
        col = {"house": (170, 70, 50), "block": (150, 150, 155), "kirche": (90, 60, 50)}[kind]
        d.polygon(pts, fill=col)
    path = [px(x, y) for x, y in W.path_points()]
    d.line(path, fill=(200, 200, 195), width=3)
    for x, y, sp, *_ in W.alley_trees():
        a, b = px(x, y)
        d.ellipse((a - 3, b - 3, a + 3, b + 3), fill=(30, 80, 30) if sp == "Maple" else (20, 60, 20))
    cc = W.cable_car()
    d.line([px(*cc["upper"]), px(*cc["lower"])], fill=(200, 30, 30), width=3)
    for t in cc["towers"]:
        a, b = px(*t)
        d.ellipse((a - 3, b - 3, a + 3, b + 3), fill=(60, 60, 60))
    for k in ("upper", "lower"):
        a, b = px(*cc[k])
        d.rectangle((a - 6, b - 4, a + 6, b + 4), fill=(200, 30, 30))
    a, b = px(0, 0)
    d.rectangle((a - 6, b - 2, a + 6, b + 14), fill=(40, 40, 40))   # the club, dug in behind the portal
    sx, sy = W.starlings()
    a, b = px(sx, sy)
    d.ellipse((a - 60, b - 35, a + 60, b + 35), outline=(30, 30, 30), width=2)
    try:
        font = ImageFont.truetype("/System/Library/Fonts/Supplemental/Arial.ttf", 22)
    except OSError:
        font = ImageFont.load_default()
    labels = [((0, -25), "Клуб"), ((sx, sy - 40), "Скворцы над морем"), (W.TOWN_CENTER, "Людмиловск"),
              ((cc["lower"][0] + 60, cc["lower"][1] - 120), "Канатка"), ((-70, 60), "Аллея"),
              ((W.shore_x(-200) - 15, -200), "Пляж"), ((700, 1500), "Балтийское море")]
    for (x, y), text in labels:
        a, b = px(x, y)
        d.text((a, b), text, fill=(15, 15, 15), font=font, anchor="mm")
    # scale bar and north arrow
    d.rectangle((30, h - 40, 30 + 500 / PX, h - 34), fill=(20, 20, 20))
    d.text((30, h - 66), "500 м", fill=(20, 20, 20), font=font)
    d.text((w - 60, 40), "С ↑", fill=(20, 20, 20), font=font)
    img.save(out)
    print(out, img.size)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "world_map.png")
