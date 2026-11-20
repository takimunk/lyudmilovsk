#!/usr/bin/env python3
"""The town's own black poplars (docs/festivals.md "Poplar fluff"): the trees whose down fills the air in June.

Rows of big black poplars behind the park avenue's maples, behind the two maple rows of Sun Square and a clump by the
pond, placed on open ground only: clear of every paved box, prop, seat, tree, flower bed and water of the public spaces
(Content/City/public_spaces.json) and of the city plan's plots, walls and stairs (Content/City/city_plan.json). Reads those
two files only (no terrain), so it runs anywhere; rerun it after tools/public_spaces.py or tools/city_plan.py moved things.

Writes Content/City/poplars.json: {"trees": [{"species", "at": [Unreal cm], "age", "variant", "yaw"}]}, planted by
ULyudPoplarFluffSubsystem as one AProceduralForest (the procedural plant system, like the public spaces' own trees).

  python3 tools/town_poplars.py            writes the file
  python3 tools/town_poplars.py --check    exits 1 if the file is not what the plans give now
"""
import json
import math
import os
import random
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CITY = os.path.join(ROOT, "unreal/Lyudmilovsk/Content/City")
OUT = os.path.join(CITY, "poplars.json")


def load(name):
    with open(os.path.join(CITY, name), encoding="utf-8") as f:
        return json.load(f)


def build():
    spaces = load("public_spaces.json")       # frame: Unreal metres
    plan = load("city_plan.json")             # map metres: Unreal Y = map Y + face_y
    face = plan.get("portal_face_y", 790.0) / 100.0
    rng = random.Random(20270612)

    boxes = [(b["c"][0], b["c"][1], math.radians(b.get("yaw", 0.0)), b["size"][0] * 0.5, b["size"][1] * 0.5) for b in spaces["boxes"]]
    points = []                               # (x, y, clearance m)
    for p in spaces["props"]:
        points.append((p["at"][0], p["at"][1], 3.0))
    for p in spaces["plants"]:
        points.append((p["at"][0], p["at"][1], 1.5))
    for t in spaces["trees"]:
        points.append((t["at"][0], t["at"][1], 6.5))
    for s in spaces["spots"]:
        points.append((s["pos"][0], s["pos"][1], 3.0))
        for slot in s.get("slots", []):
            points.append((slot["pos"][0], slot["pos"][1], 2.0))
    for w in spaces["waters"]:
        r = w.get("r", w.get("rect", 4.0))
        points.append((w["at"][0], w["at"][1], (max(r) if isinstance(r, list) else r) + 4.0))
    for f in spaces["fountains"]:
        points.append((f["at"][0], f["at"][1], 14.0))
    for s in plan["spots"]:
        points.append((s["map"][0], s["map"][1] + face, 4.0))
    for s in plan["stairs"]:
        for k in ("bottom", "top"):
            points.append((s[k][0], s[k][1] + face, 6.0))
    for l in plan["lamps"]:
        points.append((l["foot"][0], l["foot"][1] + face, 3.0))
    plots = [(p["map"][0], p["map"][1] + face, math.radians(p.get("yaw", 0.0)), p.get("d", 10) * 0.5, p.get("w", 10) * 0.5) for p in plan["plots"]
             if p.get("kind") not in ("park", "square", "fair", "pier", "promenade", "beach")]     # the open places are not houses
    walls = [[(x, y + face) for x, y in w["line"]] for w in plan["walls"]]

    def in_rect(x, y, cx, cy, yaw, hx, hy, margin):
        dx, dy = x - cx, y - cy
        lx = dx * math.cos(yaw) + dy * math.sin(yaw)
        ly = -dx * math.sin(yaw) + dy * math.cos(yaw)
        return abs(lx) < hx + margin and abs(ly) < hy + margin

    def seg_dist(x, y, a, b):
        ax, ay = a
        bx, by = b
        vx, vy = bx - ax, by - ay
        t = max(0.0, min(1.0, ((x - ax) * vx + (y - ay) * vy) / max(1e-9, vx * vx + vy * vy)))
        return math.hypot(x - ax - vx * t, y - ay - vy * t)

    def free(x, y, placed):
        if any(in_rect(x, y, cx, cy, yaw, hx, hy, 2.5) for cx, cy, yaw, hx, hy in boxes):
            return False
        # (a plot's rectangle is tried both ways round: its yaw says where its front looks, not which side is the long one)
        if any(in_rect(x, y, cx, cy, yaw, max(hx, hy), max(hx, hy), 6.0) for cx, cy, yaw, hx, hy in plots):
            return False
        if any(math.hypot(x - px, y - py) < r for px, py, r in points):
            return False
        if any(seg_dist(x, y, line[i], line[i + 1]) < 7.0 for line in walls for i in range(len(line) - 1)):
            return False
        return all(math.hypot(x - t[0], y - t[1]) > 9.0 for t in placed)

    trees = []

    def plant(x, y, z, age):
        if free(x, y, trees):
            trees.append((x, y, z, age))
            return True
        return False

    maples = [t["at"] for t in spaces["trees"] if t["species"] == "Maple"]
    # 1. the park avenue: its maples stand in pairs 10 m apart along the path; a poplar 8 m behind every other one
    by_y = {}
    for m in maples:
        by_y.setdefault(round(m[1], 1), []).append(m)
    for y in sorted(by_y):
        pair = sorted(by_y[y])
        if len(pair) != 2 or abs(pair[1][0] - pair[0][0] - 10.0) > 0.5 or int(round(y)) % 24 > 12:
            continue
        plant(pair[0][0] - 8.0, y + 6.0, pair[0][2], rng.uniform(38, 60))
        plant(pair[1][0] + 8.0, y + 6.0, pair[1][2], rng.uniform(38, 60))
    # 2. Sun Square: behind each of its two rows of maples (same Y, 13 m apart), a row of poplars on the outer side
    for y, row in by_y.items():
        if len(row) < 6:
            continue
        mid = sum(m[1] for m in maples if len(by_y.get(round(m[1], 1), [])) >= 6) / max(1, sum(1 for m in maples if len(by_y.get(round(m[1], 1), [])) >= 6))
        out = -9.0 if y < mid else 9.0
        for i, m in enumerate(sorted(row)):
            if i % 2 == 0:
                plant(m[0] + 6.5, y + out, m[2], rng.uniform(45, 70))
    # 3. by the pond: a clump on its landward side, old ones
    for w in spaces["waters"]:
        if w["id"] != "pond":
            continue
        r = w["r"] if isinstance(w["r"], list) else [w["r"], w["r"]]
        for k in range(14):
            ang = math.radians(150 + k * 9 + rng.uniform(-3, 3))
            dist = 1.0 + rng.uniform(0.25, 0.8)
            plant(w["at"][0] + math.cos(ang) * r[0] * dist + rng.uniform(-2, 2) - 6.0, w["at"][1] + math.sin(ang) * r[1] * dist, -36.2, rng.uniform(50, 80))
    out = []
    for x, y, z, age in trees:
        out.append({"species": "BlackPoplar", "at": [round(x * 100.0, 1), round(y * 100.0, 1), round(z * 100.0, 1)], "age": round(age, 1),
                    "variant": rng.randrange(3), "yaw": round(rng.uniform(0, 360), 1)})
    return {"_doc": "The town's black poplars (tools/town_poplars.py; Unreal cm). Planted by ULyudPoplarFluffSubsystem.", "trees": out}


def main():
    doc = build()
    text = json.dumps(doc, ensure_ascii=False, indent=1) + "\n"
    if "--check" in sys.argv:
        old = open(OUT, encoding="utf-8").read() if os.path.exists(OUT) else ""
        if old != text:
            print("poplars.json is stale: run tools/town_poplars.py")
            return 1
        print("poplars.json up to date (%d trees)" % len(doc["trees"]))
        return 0
    with open(OUT, "w", encoding="utf-8") as f:
        f.write(text)
    print("%d black poplars -> %s" % (len(doc["trees"]), os.path.relpath(OUT, ROOT)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
