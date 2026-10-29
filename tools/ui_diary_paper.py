"""The diary's paper (docs/diary.md): a page of an A4 planner in the hero's hands, cream offset paper with a faint dot grid
every 5 mm, a few fibres, the edges a touch yellower, the side at the spine shaded where the page curves into the binding.

  python3 tools/ui_diary_paper.py      writes textures/ui/diary_paper_left.png, textures/ui/diary_paper_right.png (1024 x 1448)
then unreal/scripts/import_diary_assets.py imports them as /Game/UI/Diary/T_PaperLeft, T_PaperRight.
Deterministic (seeded); the same picture every run.
"""
import os

import numpy as np
from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
W, H = 1024, 1448          # A4 at ~4.9 px/mm
MM = W / 210.0
PAPER = np.array([243, 237, 221], np.float32)
DOT = np.array([150, 160, 178], np.float32)


def page(right, seed=5):
    rng = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    col = np.tile(PAPER, (H, W, 1))
    # tooth of the paper: two scales of soft noise
    n1 = np.asarray(Image.fromarray((rng.random((H // 4, W // 4)) * 255).astype(np.uint8)).resize((W, H), Image.BICUBIC), np.float32) / 255
    n2 = rng.random((H, W)).astype(np.float32)
    col *= (0.985 + 0.02 * n1 + 0.012 * n2)[..., None]
    # fibres: a few short pale curls
    fib = Image.new("L", (W, H), 0)
    from PIL import ImageDraw
    d = ImageDraw.Draw(fib)
    for _ in range(140):
        x, y = rng.uniform(0, W), rng.uniform(0, H)
        a = rng.uniform(0, np.pi)
        L = rng.uniform(4, 14)
        pts = [(x + np.cos(a + 0.2 * np.sin(t)) * t, y + np.sin(a + 0.2 * np.sin(t)) * t) for t in np.linspace(0, L, 6)]
        d.line(pts, fill=int(rng.uniform(40, 90)), width=1)
    fib = np.asarray(fib.filter(ImageFilter.GaussianBlur(0.5)), np.float32) / 255
    col = col * (1 - 0.08 * fib[..., None])
    # the dot grid, 5 mm, inside the margins
    step = 5 * MM
    gx = np.abs(((xx - W / 2) / step + 0.5) % 1 - 0.5) * step
    gy = np.abs(((yy - H / 2) / step + 0.5) % 1 - 0.5) * step
    dot = np.clip(1.7 - np.hypot(gx, gy), 0, 1) * 0.5
    inside = (xx > 12 * MM) & (xx < W - 12 * MM) & (yy > 14 * MM) & (yy < H - 12 * MM)
    dot *= inside
    col = col * (1 - dot[..., None]) + DOT * dot[..., None]
    # yellower toward the edges, the binding side shaded
    ex = np.minimum(xx, W - 1 - xx) / W
    ey = np.minimum(yy, H - 1 - yy) / H
    age = np.clip(1 - np.minimum(ex * 9, ey * 9), 0, 1) ** 2
    col = col * (1 - 0.05 * age[..., None]) + np.array([214, 190, 140], np.float32) * 0.05 * age[..., None]
    spine = (xx / W) if right else (1 - xx / W)       # 0 at the spine
    shade = np.exp(-spine * 30) * 0.24 + np.exp(-spine * 8) * 0.06
    col *= (1 - shade)[..., None]
    out = np.clip(col, 0, 255).astype(np.uint8)
    return Image.fromarray(out, "RGB")


def main():
    out = os.path.join(ROOT, "textures", "ui")
    os.makedirs(out, exist_ok=True)
    for right, name in ((False, "diary_paper_left.png"), (True, "diary_paper_right.png")):
        p = os.path.join(out, name)
        page(right, seed=5 if right else 9).save(p, optimize=True)
        print("wrote", os.path.relpath(p, ROOT))


if __name__ == "__main__":
    main()
