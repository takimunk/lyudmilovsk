"""Contact sheet: python3 tools/characters/sheet.py out.png cols height img1 img2 ... (each scaled to the height)."""
import sys
from PIL import Image
out, cols, h = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
ims = [Image.open(p).convert("RGB") for p in sys.argv[4:]]
ims = [i.resize((int(i.width * h / i.height), h)) for i in ims]
w = max(i.width for i in ims)
rows = (len(ims) + cols - 1) // cols
s = Image.new("RGB", (w * cols, h * rows), (40, 40, 40))
for k, i in enumerate(ims):
    s.paste(i, ((k % cols) * w, (k // cols) * h))
s.save(out)
