"""GIFs (and a contact sheet) from unreal/scripts/test_faces.py's frames: the FACESHOT log lines name each screenshot.

    python3 tools/face_gif.py <log> <screenshots newer than this file> <out dir>
"""
import glob
import os
import re
import sys

from PIL import Image

log, since, out = sys.argv[1:4]
names = re.findall(r"FACESHOT (\S+) (\S+) (\d+)", open(log, errors="ignore").read())
t0 = os.path.getmtime(since)
shots = sorted((f for f in glob.glob("unreal/Lyudmilovsk/Saved/Screenshots/**/*.png", recursive=True) if os.path.getmtime(f) > t0),
               key=os.path.getmtime)
os.makedirs(out, exist_ok=True)
groups = {}
for (who, what, n), f in zip(names, shots):
    groups.setdefault((who, what), []).append(f)
for (who, what), fs in groups.items():
    ims = [Image.open(f).convert("RGB").resize((320, 320)) for f in fs]
    ims[0].save(os.path.join(out, "%s_%s.gif" % (who, what)), save_all=True, append_images=ims[1:], duration=120, loop=0)
    sheet = Image.new("RGB", (160 * 10, 160 * ((len(ims) + 9) // 10)))
    for i, im in enumerate(ims):
        sheet.paste(im.resize((160, 160)), (160 * (i % 10), 160 * (i // 10)))
    sheet.save(os.path.join(out, "%s_%s_sheet.jpg" % (who, what)))
print(sorted(groups))
