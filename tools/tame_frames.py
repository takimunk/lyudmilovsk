#!/usr/bin/env python3
"""Turns the screenshots of an offscreen befriending recording (unreal/scripts/record_tame.py) into a contact sheet and
an animation, without ffmpeg (Pillow only).

    python3 tools/tame_frames.py <out_dir> <name> [--since <unix time>] [--every N] [--cols 6]

Takes unreal/Lyudmilovsk/Saved/Screenshots/*/ScreenShot*.png newer than --since in time order, writes
<out_dir>/<name>.webp (animated, 5 fps, 640 px wide) and <out_dir>/<name>_sheet.jpg (every N-th frame, numbered).
"""
import argparse
import glob
import os

from PIL import Image, ImageDraw

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out_dir")
    ap.add_argument("name")
    ap.add_argument("--since", type=float, default=0.0)
    ap.add_argument("--every", type=int, default=10)
    ap.add_argument("--cols", type=int, default=6)
    ap.add_argument("--width", type=int, default=640)
    a = ap.parse_args()
    shots = sorted((p for p in glob.glob(os.path.join(ROOT, "unreal/Lyudmilovsk/Saved/Screenshots/*/ScreenShot*.png"))
                    if os.path.getmtime(p) >= a.since), key=os.path.getmtime)
    if not shots:
        raise SystemExit("no screenshots")
    os.makedirs(a.out_dir, exist_ok=True)
    frames = []
    for p in shots:
        im = Image.open(p).convert("RGB")
        h = round(im.height * a.width / im.width)
        frames.append(im.resize((a.width, h), Image.LANCZOS))
    anim = os.path.join(a.out_dir, a.name + ".webp")
    frames[0].save(anim, save_all=True, append_images=frames[1:], duration=200, loop=0, quality=70)
    picks = list(range(0, len(frames), a.every))
    tw, th = 320, round(frames[0].height * 320 / frames[0].width)
    rows = (len(picks) + a.cols - 1) // a.cols
    sheet = Image.new("RGB", (tw * a.cols, th * rows), (0, 0, 0))
    d = ImageDraw.Draw(sheet)
    for k, i in enumerate(picks):
        x, y = (k % a.cols) * tw, (k // a.cols) * th
        sheet.paste(frames[i].resize((tw, th), Image.LANCZOS), (x, y))
        d.text((x + 6, y + 4), "%d  %.1fs" % (i, i * 0.2), fill=(255, 255, 0))
    sheet_path = os.path.join(a.out_dir, a.name + "_sheet.jpg")
    sheet.save(sheet_path, quality=85)
    print(anim)
    print(sheet_path, len(frames), "frames")


if __name__ == "__main__":
    main()
