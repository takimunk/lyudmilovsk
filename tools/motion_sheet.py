#!/usr/bin/env python3
"""Contact sheets, mp4s and motion numbers from unreal/scripts/shoot_motion.sh output.

  python3 tools/motion_sheet.py <Saved/Motion/<tag>>      -> sheet_<rec>.jpg, crop_<rec>.jpg, <rec>.mp4, numbers.txt

sheet_<rec>.jpg: every 3rd frame, 6 across (whole frames). crop_<rec>.jpg: 24 consecutive frames, the middle of the
picture at full size (a followed subject stays there): stutter, foot slide and pose pops show frame by frame.
numbers.txt, per recording and subject (s0 the main one), from the MOTION lines (cm, degrees, 30 frames a second):
  speed            mean ground speed of the actor, cm/s
  step cv          spread of the per-frame actor step (0 = even; stutter or pauses raise it), max/min step
  stalls           frames the actor stood still between moving frames (position not updated)
  yaw max          largest yaw change in one frame (snapping turns), deg
  mesh-actor jump  largest frame-to-frame change of the mesh's offset from the actor (smoothing pops), cm
  z pops           frame-to-frame actor z changes over 3 cm (kerbs / stairs / ground snapping)
  pose frozen      frames whose component-space end bone didn't move at all while the actor did (animation not
                   updated that frame: update-rate optimisation or a stalled tick); longest run
  foot slide       for people: mean / max world XY drift per frame of a planted foot (the lower foot, within 4 cm of
                   its lowest point over the recording), cm per frame
"""
import math
import os
import re
import shutil
import subprocess
import sys
from collections import defaultdict

from PIL import Image, ImageDraw

D = sys.argv[1]
FR = os.path.join(D, "frames")
FFMPEG = shutil.which("ffmpeg")


def recs():
    out = defaultdict(list)
    for f in sorted(os.listdir(FR)) if os.path.isdir(FR) else []:
        m = re.match(r"motion_[^_]+_(.+)_f(\d{4})\.png$", f)
        if m:
            out[m.group(1)].append((int(m.group(2)), f))
    return {k: sorted(v) for k, v in out.items()}


def sheet(name, frames):
    pick = frames[::3]
    if not pick:
        return
    im0 = Image.open(os.path.join(FR, pick[0][1]))
    w, h = im0.size
    tw = 320
    th = int(h * tw / w)
    cols = 6
    rows = (len(pick) + cols - 1) // cols
    S = Image.new("RGB", (cols * tw, rows * th), (0, 0, 0))
    dr = ImageDraw.Draw(S)
    for k, (n, f) in enumerate(pick):
        im = Image.open(os.path.join(FR, f)).convert("RGB").resize((tw, th), Image.LANCZOS)
        x, y = (k % cols) * tw, (k // cols) * th
        S.paste(im, (x, y))
        dr.text((x + 4, y + 3), "f%d" % n, fill=(255, 255, 0))
    S.save(os.path.join(D, "sheet_%s.jpg" % name), quality=72)
    # consecutive crops from the middle
    cw, ch = min(300, w // 3), min(300, h // 2)
    start = max(0, len(frames) // 2 - 12)
    run = frames[start:start + 24]
    C = Image.new("RGB", (8 * cw, 3 * ch), (0, 0, 0))
    dr = ImageDraw.Draw(C)
    for k, (n, f) in enumerate(run):
        im = Image.open(os.path.join(FR, f)).convert("RGB")
        box = ((w - cw) // 2, (h - ch) // 2 + int(h * 0.08), (w + cw) // 2, (h + ch) // 2 + int(h * 0.08))
        C.paste(im.crop(box), ((k % 8) * cw, (k // 8) * ch))
        dr.text(((k % 8) * cw + 4, (k // 8) * ch + 3), "f%d" % n, fill=(255, 255, 0))
    C.save(os.path.join(D, "crop_%s.jpg" % name), quality=75)


def mp4(name, frames):
    if not FFMPEG or not frames:
        return
    tmp = os.path.join(D, "_seq_" + name)
    shutil.rmtree(tmp, ignore_errors=True)
    os.makedirs(tmp)
    for k, (n, f) in enumerate(frames):
        os.link(os.path.join(FR, f), os.path.join(tmp, "%05d.png" % k))
    subprocess.run([FFMPEG, "-loglevel", "error", "-y", "-framerate", "30", "-i", os.path.join(tmp, "%05d.png"),
                    "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "20", os.path.join(D, name + ".mp4")])
    shutil.rmtree(tmp, ignore_errors=True)


def vec(s):
    return tuple(float(x) for x in s.split(","))


def parse():
    """{rec: {subject: [(frame, fields)]}}"""
    out = defaultdict(lambda: defaultdict(list))
    path = os.path.join(D, "motion.txt")
    if not os.path.exists(path):
        return out
    for line in open(path, encoding="utf-8", errors="replace"):
        m = re.match(r"MOTION (\S+) f(\d{4}) (.*)$", line.strip())
        if not m:
            continue
        rec, n, rest = m.group(1), int(m.group(2)), m.group(3)
        for part in rest.split(" | "):
            toks = part.split()
            if not toks or toks[0].startswith("cam="):
                continue
            label = toks[0]
            fields = {}
            for t in toks[1:]:
                if "=" in t:
                    k, v = t.split("=", 1)
                    fields[k] = v
            out[rec][label].append((n, fields))
    return out


def stats(rows):
    rows = [(n, f) for n, f in rows if "a" in f]
    if len(rows) < 10:
        return "too few rows (%d)" % len(rows)
    A = [vec(f["a"]) for _, f in rows]
    steps = [math.hypot(A[i][0] - A[i - 1][0], A[i][1] - A[i - 1][1]) for i in range(1, len(A))]
    mean = sum(steps) / len(steps)
    out = []
    out.append("speed %.0f cm/s" % (mean * 30))
    steps = steps or [0.0]
    if mean > 0.3:
        sd = math.sqrt(sum((s - mean) ** 2 for s in steps) / len(steps))
        stalls = sum(1 for i in range(1, len(steps) - 1) if steps[i] < 0.05 * mean and steps[i - 1] > 0.3 * mean)
        big = [i + 1 for i, s in enumerate(steps) if s > 1.8 * mean]
        out.append("step cv %.2f (min %.2f max %.2f cm/frame) stalls %d jumps>1.8x %d%s" % (
            sd / mean, min(steps), max(steps), stalls, len(big), (" at f" + ",".join(str(rows[i][0]) for i in big[:8])) if big else ""))
    yaws = [float(f.get("yaw", 0)) for _, f in rows]
    dy = [abs((yaws[i] - yaws[i - 1] + 180) % 360 - 180) for i in range(1, len(yaws))]
    out.append("yaw max %.1f deg/frame (mean %.2f)" % (max(dy), sum(dy) / len(dy)))
    if "m" in rows[0][1]:
        M = [vec(f["m"]) for _, f in rows]
        off = [(M[i][0] - A[i][0], M[i][1] - A[i][1], M[i][2] - A[i][2]) for i in range(len(A))]
        dj = [math.dist(off[i], off[i - 1]) for i in range(1, len(off))]
        out.append("mesh-actor jump max %.1f cm (mean %.2f)" % (max(dj), sum(dj) / len(dj)))
    dz = [A[i][2] - A[i - 1][2] for i in range(1, len(A))]
    zp = [(rows[i + 1][0], d) for i, d in enumerate(dz) if abs(d) > 3.0]
    out.append("z pops %d%s" % (len(zp), (" (" + ", ".join("f%d %+.0f" % x for x in zp[:6]) + ")") if zp else ""))
    if "cs" in rows[0][1]:
        cs = [vec(f["cs"]) for _, f in rows]
        frozen = [i for i in range(1, len(cs)) if math.dist(cs[i], cs[i - 1]) < 0.005]
        run, best = 0, 0
        for i in range(1, len(cs)):
            run = run + 1 if i in frozen else 0
            best = max(best, run)
        out.append("pose frozen %d/%d frames (longest run %d)" % (len(frozen), len(cs) - 1, best))
    for foot in ("foot_l", "foot_r"):
        if foot not in rows[0][1]:
            continue
        # height of the foot over the actor's own point (the ground under it moves on stairs and slopes)
        # walking frames only (a seated or standing body is judged by the other figures)
        idx = [i for i in range(1, len(rows)) if foot in rows[i][1] and steps[i - 1] > 0.4 * mean and mean > 0.5]
        if len(idx) < 10:
            continue
        P = [vec(rows[i][1][foot]) for i in idx]
        AA = [A[i] for i in idx]
        H = [P[i][2] - AA[i][2] for i in range(len(P))]
        zmin = sorted(H)[len(H) // 20]   # the 5th percentile: a stray low frame does not set the ground
        slide = [math.hypot(P[i][0] - P[i - 1][0], P[i][1] - P[i - 1][1]) for i in range(1, len(P))
                 if idx[i] == idx[i - 1] + 1 and H[i] < zmin + 4.0 and H[i - 1] < zmin + 4.0]
        if slide:
            out.append("%s planted %d frames: slide mean %.2f max %.2f cm/frame (lift %.0f cm)" % (
                foot, len(slide), sum(slide) / len(slide), max(slide), max(H) - zmin))
    # the ball of the foot (the ankle rolls ~12 cm forward heel to toe even on a perfect walk): planted = within 1.5 cm
    # of its lowest height over the actor's point during the walking frames, and lower than the other ball
    if "ball_l" in rows[0][1] and mean > 0.5:
        idx = [i for i in range(1, len(rows)) if "ball_l" in rows[i][1] and "ball_r" in rows[i][1] and steps[i - 1] > 0.4 * mean]
        if len(idx) >= 20:
            BL = {i: vec(rows[i][1]["ball_l"]) for i in idx}
            BR = {i: vec(rows[i][1]["ball_r"]) for i in idx}
            hl = {i: BL[i][2] - A[i][2] for i in idx}
            hr = {i: BR[i][2] - A[i][2] for i in idx}
            spd, plants = [], {"l": [], "r": []}
            for side, B, h, ho in (("l", BL, hl, hr), ("r", BR, hr, hl)):
                hmin = min(h.values())
                # runs of consecutive planted frames; a run counts from 3 frames, without its first and last frame
                runs, cur = [], []
                for i in idx:
                    if h[i] < hmin + 1.5 and h[i] < ho[i] and (not cur or cur[-1] == i - 1):
                        cur.append(i)
                    else:
                        if len(cur) >= 3:
                            runs.append(cur)
                        cur = [i] if (h[i] < hmin + 1.5 and h[i] < ho[i]) else []
                if len(cur) >= 3:
                    runs.append(cur)
                for run in runs:
                    plants[side].append(B[run[len(run) // 2]])
                    inner = run[1:-1]
                    for k in range(1, len(inner)):
                        spd.append(math.hypot(B[inner[k]][0] - B[inner[k - 1]][0], B[inner[k]][1] - B[inner[k - 1]][1]) * 30.0)
            strides = [math.hypot(p[k][0] - p[k - 1][0], p[k][1] - p[k - 1][1]) for p in plants.values() for k in range(1, len(p))]
            if spd:
                spd.sort()
                out.append("BALL slide mean %.1f p95 %.1f cm/s (%d planted frame pairs) body %.0f cm/s stride %s cm" % (
                    sum(spd) / len(spd), spd[min(len(spd) - 1, int(len(spd) * 0.95))], len(spd), mean * 30,
                    ("%.0f (n%d)" % (sum(strides) / len(strides), len(strides))) if strides else "?"))
    if "pelvis" in rows[0][1]:
        PV = [vec(f["pelvis"]) for _, f in rows if "pelvis" in f]
        ps = [math.hypot(PV[i][0] - PV[i - 1][0], PV[i][1] - PV[i - 1][1]) for i in range(1, len(PV))]
        pz = [abs(PV[i][2] - PV[i - 1][2]) for i in range(1, len(PV))]
        if ps:
            k = max(range(len(ps)), key=lambda i: ps[i])
            pm = sum(ps) / len(ps)
            out.append("PELVIS step max %.1f cm/frame at f%d (mean %.2f), accel max %.1f cm/frame2, z step max %.1f" % (
                ps[k], rows[k + 1][0], pm, max(abs(ps[i] - ps[i - 1]) for i in range(1, len(ps))) if len(ps) > 1 else 0.0, max(pz)))
    return "; ".join(out)


def main():
    R = recs()
    for name, frames in R.items():
        sheet(name, frames)
        mp4(name, frames)
    data = parse()
    lines = []
    for rec in sorted(set(data) | set(R)):
        lines.append("== %s: %d frames" % (rec, len(R.get(rec, []))))
        for label in sorted(data.get(rec, {})):
            rows = data[rec][label]
            last = rows[-1][1]
            what = " ".join("%s=%s" % (k, last[k]) for k in ("state", "sp", "act") if k in last)
            lines.append("  %s %s: %s" % (label, what, stats(rows)))
    open(os.path.join(D, "numbers.txt"), "w").write("\n".join(lines) + "\n")
    print("\n".join(lines))


main()
