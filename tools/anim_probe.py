#!/usr/bin/env python3
"""Figures of how people move from a game log with lyud.Anim.Probe N on (ANIMPROBE lines, PeopleLife.cpp).

    python3 tools/anim_probe.py <log> [<log2> ...]

Per log: over every person followed for at least a second
  body jerk     how unevenly the body itself travels: the 95th percentile and the worst of |acceleration| (m/s2) of the
                actor's point (a person walking steadily is near 0; a teleport or a stutter is tens)
  turn jerk     the same for the facing: deg/s2
  hips jerk     |acceleration| of the pelvis bone (the pose and the body together), m/s2
  foot slide    how far a foot on the ground (the lower one, within 4 cm of its lowest) travels along the ground per
                second while the person walks (cm/s; a planted foot stands still: 0)
  hand jerk     |acceleration| of the right hand, m/s2 (popping clips and gestures)
Compare a run with lyud.Anim.Smooth 0 against one with 1 at the same place and hour.
"""
import math
import sys
from collections import defaultdict


def pct(v, p):
    if not v:
        return 0.0
    v = sorted(v)
    return v[min(len(v) - 1, int(len(v) * p))]


def read(path):
    who = defaultdict(list)
    with open(path, errors="replace") as f:
        for line in f:
            i = line.find("ANIMPROBE ")
            if i < 0:
                continue
            a = line[i + 10:].split()
            if len(a) < 20:
                continue
            v = [float(x) for x in a]
            who[int(v[1])].append(v)
    return who


def accel(ts, ps):
    """|second difference| of points ps at times ts, in the points' units per s2."""
    out = []
    for k in range(1, len(ts) - 1):
        d0, d1 = ts[k] - ts[k - 1], ts[k + 1] - ts[k]
        if d0 <= 0 or d1 <= 0 or d0 > 0.2 or d1 > 0.2:
            continue
        a = [((ps[k + 1][j] - ps[k][j]) / d1 - (ps[k][j] - ps[k - 1][j]) / d0) / (0.5 * (d0 + d1)) for j in range(len(ps[k]))]
        out.append(math.sqrt(sum(x * x for x in a)))
    return out


def report(path):
    who = read(path)
    body, turn, hips, hand, slide = [], [], [], [], []
    n = 0
    for rows in who.values():
        if len(rows) < 30 or rows[-1][0] - rows[0][0] < 1.0:
            continue
        n += 1
        ts = [r[0] for r in rows]
        body += [a * 0.01 for a in accel(ts, [r[2:5] for r in rows])]
        hips += [a * 0.01 for a in accel(ts, [r[12:15] for r in rows])]
        hand += [a * 0.01 for a in accel(ts, [r[15:18] for r in rows])]
        yaw = [rows[0][5]]
        for r in rows[1:]:
            d = (r[5] - yaw[-1] + 180.0) % 360.0 - 180.0
            yaw.append(yaw[-1] + d)
        turn += accel(ts, [[y] for y in yaw])
        for side in (6, 9):
            low = min(r[side + 2] - r[4] for r in rows)
            for k in range(1, len(rows)):
                dt = ts[k] - ts[k - 1]
                walking = math.hypot(rows[k][2] - rows[k - 1][2], rows[k][3] - rows[k - 1][3]) / max(dt, 1e-4) > 30.0
                if 0 < dt < 0.2 and walking and rows[k][side + 2] - rows[k][4] < low + 4.0 and rows[k - 1][side + 2] - rows[k - 1][4] < low + 4.0:
                    slide.append(math.hypot(rows[k][side] - rows[k - 1][side], rows[k][side + 1] - rows[k - 1][side + 1]) / dt)
    print(f"{path}: {n} people")
    for name, v, unit in (("body jerk", body, "m/s2"), ("turn jerk", turn, "deg/s2"), ("hips jerk", hips, "m/s2"), ("hand jerk", hand, "m/s2"),
                          ("foot slide", slide, "cm/s")):
        print(f"  {name:10s} median {pct(v, 0.5):9.2f}  p95 {pct(v, 0.95):9.2f}  worst {max(v) if v else 0.0:9.2f} {unit}  ({len(v)} samples)")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    for p in sys.argv[1:]:
        report(p)
