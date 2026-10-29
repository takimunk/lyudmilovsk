"""Contact sheet of a shoot.sh run: tools/sky/rare/sheet.py Saved/RareSky/<tag> [cols] -> <tag>/sheet.png"""
import os
import sys

from PIL import Image, ImageDraw

d = sys.argv[1]
cols = int(sys.argv[2]) if len(sys.argv) > 2 else 3
files = sorted(f for f in os.listdir(d) if f.endswith(".png") and f[:2].isdigit())
w, h = 640, 360
sheet = Image.new("RGB", (w * cols, (h + 18) * ((len(files) + cols - 1) // cols)), (20, 20, 20))
dr = ImageDraw.Draw(sheet)
for i, f in enumerate(files):
    im = Image.open(os.path.join(d, f)).convert("RGB").resize((w, h), Image.LANCZOS)
    x, y = (i % cols) * w, (i // cols) * (h + 18)
    sheet.paste(im, (x, y + 18))
    dr.text((x + 4, y + 3), f[:-4], fill=(230, 230, 230))
sheet.save(os.path.join(d, "sheet.png"))
print(os.path.join(d, "sheet.png"))
