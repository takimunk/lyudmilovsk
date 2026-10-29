"""Overhead power lines of the Academgorodok (docs/academgorodok, docs/bird_life.md): concrete poles of the
1970s (the SV-95 kind: a tapered square post, a steel crossarm, four porcelain pin insulators) along the street
in front of the square and along the side houses' streets, with drops to the houses' walls. Swallows, starlings,
sparrows, pigeons and jackdaws sit on the wires.

  python3 tools/academ_wires.py            -> unreal/Lyudmilovsk/Content/BirdsData/wires.json (+ --plot PNG)

Read by ALyudWires (Source/Lyudmilovsk/Private/World/LyudWires.cpp: builds the poles and wires in the game) and by
tools/bird_habitats.py (each conductor becomes Wire perch sites). Frame of the file: Unreal metres (UE cm / 100),
like bird_habitats.json: X as the map, Y = portal face + map Y, Z up.
"""
import argparse
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "blender", "academgorodok"))
import world_map as W  # noqa: E402
import terrain as T  # noqa: E402
import plan as P  # noqa: E402
from geom import Frame  # noqa: E402

OUT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "BirdsData", "wires.json")

POLE_H = 8.5           # m over the ground (SV-95: 9.5 m, 1 m of it buried... 2 m in practice; 8.5 m standing)
ARM_Z = 8.15           # the crossarm under the top
WIRE_DZ = 0.17         # insulator: the wire lies this high over the crossarm
PHASES = (-0.75, -0.25, 0.25, 0.75)   # conductors across the crossarm (0.4 kV: three phases and the neutral)
SIDE_ROW = 15.9        # the side houses' street line: this far from the house's centre line (D/2 + 6 m)


def sag(span):
    """Aluminium conductor at autumn temperature: ~0.6 m in a 32 m span."""
    return 0.012 * span + 0.0002 * span * span


def house_frame(name):
    cx, cy, yaw, mirror = P.FRAMES[name]
    return Frame(cx, cy, math.radians(yaw), mirror=mirror)


def layout():
    """Poles and spans in the ensemble frame (x east, y north, metres): [(x, y)], [(i, j)], wall brackets [(x, y, z)]."""
    poles, spans, drops = [], [], []

    def pole(x, y):
        poles.append((x, y))
        return len(poles) - 1

    # A: the street in front of the square, on its edge, 32 m spans, none on the square's axis (the view to Novikova 1)
    street = [pole(P.SQUARE["x_end"] + 2.0, y) for y in (-112.0, -80.0, -48.0, -16.0, 16.0, 48.0, 80.0, 112.0)]
    spans += list(zip(street, street[1:]))
    # B, C: along the side houses' streets (their outer side, v+), joined to the street line through one pole
    for name, link_y in (("Novikova3", 80.0), ("Maksimova8", -80.0)):
        f = house_frame(name)
        row = [pole(*f((u, SIDE_ROW, 0.0))[:2]) for u in (42.0, 14.0, -14.0, -42.0)]
        # the row's end nearer the street line takes the link (u = +42 is the gate end, but the mirror twin turns it)
        a, b = poles[row[0]], poles[row[-1]]
        if math.hypot(a[0] - 82.0, a[1] - link_y) > math.hypot(b[0] - 82.0, b[1] - link_y):
            row.reverse()
        first = poles[row[0]]
        start = street[6] if link_y > 0 else street[1]
        sx, sy = poles[start]
        mid = pole((sx + first[0]) / 2.0, (sy + first[1]) / 2.0)
        spans += [(start, mid), (mid, row[0])] + list(zip(row, row[1:]))
        # drops to the house: from the two middle poles to brackets under its eaves on the street wall
        ez = P.eaves_z(P.S_FLOORS) - 0.8
        for k, u in ((1, 14.0), (2, -14.0)):
            x, y, _ = f((u, P.S_D / 2.0 + 0.25, 0.0))
            drops.append((row[k], (x, y, ez)))
    # Novikova 1: a drop to its north gable from the joint of line B
    n1 = house_frame("Novikova1")
    x, y, _ = n1((P.N1_L / 2.0 + 0.25, 0.0, 0.0))
    nb = min(range(len(poles)), key=lambda i: math.hypot(poles[i][0] - x, poles[i][1] - y))
    drops.append((nb, (x, y, P.eaves_z(P.N1_FLOORS) - 0.8)))
    return poles, spans, drops


def build():
    poles, spans, drops = layout()
    ground = W.academ_z()

    def to_map(p):
        return W.academ_to_map(p[0], p[1])

    def gz(mx, my):
        return T.ground_z(mx, my) if W.academ_flat(mx, my) < 1.0 else ground

    # each pole's crossarm lies across the line: the mean direction of its spans
    dirs = [[] for _ in poles]
    for i, j in spans:
        (ax, ay), (bx, by) = to_map(poles[i]), to_map(poles[j])
        d = math.atan2(by - ay, bx - ax)
        dirs[i].append(d)
        dirs[j].append(d)
    out_poles = []
    for k, p in enumerate(poles):
        mx, my = to_map(p)
        ds = dirs[k] or [0.0]
        cx = sum(math.cos(2 * d) for d in ds)
        cy = sum(math.sin(2 * d) for d in ds)
        along = 0.5 * math.atan2(cy, cx)                 # mean of undirected angles
        # an end or corner pole leans on a strut against the pull of its wires
        strut = None
        if len(ds) == 1 or (len(ds) == 2 and abs(math.sin(ds[0] - ds[1])) > 0.35):
            sx = sum(math.cos(d) for d in ds)
            sy = sum(math.sin(d) for d in ds)
            n = math.hypot(sx, sy)
            if n > 1e-3:
                strut = [-sx / n, -sy / n]               # on the far side from the wires' pull
        out_poles.append({"at": [mx, my, gz(mx, my)], "along": math.degrees(along), "strut": strut})

    def arm_point(k, side):
        p = out_poles[k]
        a = math.radians(p["along"]) + math.pi / 2
        x, y, z = p["at"]
        return [x + math.cos(a) * side, y + math.sin(a) * side, z + ARM_Z + WIRE_DZ]

    wires = []
    for i, j in spans:
        # keep the phases from crossing: match the crossarms' sides by direction
        ai = math.radians(out_poles[i]["along"]) + math.pi / 2
        aj = math.radians(out_poles[j]["along"]) + math.pi / 2
        flip = -1.0 if math.cos(ai - aj) < 0 else 1.0
        for side in PHASES:
            a, b = arm_point(i, side), arm_point(j, side * flip)
            wires.append({"a": a, "b": b, "sag": sag(math.dist(a[:2], b[:2]))})
    brackets = []
    for k, (bx, by, bz) in drops:
        mx, my = to_map((bx, by))
        z = gz(mx, my) + bz
        brackets.append({"at": [mx, my, z]})
        x, y, _ = out_poles[k]["at"]
        for side in (-0.15, 0.15):                        # a two-wire service drop
            a = arm_point(k, side * 3.0)
            wires.append({"a": a, "b": [mx + side, my, z], "sag": sag(math.dist(a[:2], (mx, my))) * 0.7})
    return out_poles, wires, brackets


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=OUT)
    ap.add_argument("--plot", default=None, help="PNG of the layout over the ensemble")
    a = ap.parse_args()
    poles, wires, brackets = build()
    try:
        with open(os.path.join(ROOT, "blender", "export", "scene01_layout.json")) as f:
            face_y = json.load(f)["outside"]["portal_face_y"]
    except (OSError, KeyError):
        face_y = 790.0
    fy = face_y / 100.0

    def ue(p):
        return [round(p[0], 3), round(p[1] + fy, 3), round(p[2], 3)]

    doc = {"version": 1, "frame": "unreal_metres", "face_y_cm": face_y, "pole_height": POLE_H, "arm_z": ARM_Z,
           "poles": [{"at": ue(p["at"]), "along": round(p["along"], 2), "strut": p["strut"]} for p in poles],
           "wires": [{"a": ue(w["a"]), "b": ue(w["b"]), "sag": round(w["sag"], 3)} for w in wires],
           "brackets": [{"at": ue(b["at"])} for b in brackets]}
    with open(a.out, "w") as f:
        json.dump(doc, f, indent=1)
    print("academ_wires: %d poles, %d wires, %d wall brackets -> %s" % (len(poles), len(wires), len(brackets), a.out))
    if a.plot:
        plot(poles, wires, a.plot)


def plot(poles, wires, path):
    from PIL import Image, ImageDraw
    ox, oy, _ = W.academ()
    s, w, h = 3.0, 900, 900

    def px(x, y):
        return (w / 2 - (x - ox) * s, h / 2 - (y - oy) * s)     # map X grows west: draw west on the left

    im = Image.new("RGB", (w, h), (235, 232, 222))
    d = ImageDraw.Draw(im)
    for name in P.FRAMES:
        f = house_frame(name)
        L, D = (P.N1_L, P.N1_D) if name == "Novikova1" else (P.S_L, P.S_D)
        pts = [W.academ_to_map(*f((u, v, 0))[:2]) for u, v in ((-L / 2, -D / 2), (L / 2, -D / 2), (L / 2, D / 2), (-L / 2, D / 2))]
        d.polygon([px(*p) for p in pts], fill=(190, 160, 140))
    with open(os.path.join(ROOT, "blender", "export", "academgorodok", "layout.json")) as f:
        lay = json.load(f)
    for tx, ty, *_ in lay["trees"]:
        x, y = W.academ_to_map(tx, ty)
        d.ellipse([px(x, y)[0] - 9, px(x, y)[1] - 9, px(x, y)[0] + 9, px(x, y)[1] + 9], outline=(40, 120, 40))
    for g in lay["garden"]:
        (cx, cy), (hx, hy) = g["centre"], g["half"]
        pts = [W.academ_to_map(cx + a * hx, cy + b * hy) for a, b in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        d.polygon([px(*p) for p in pts], outline=(40, 140, 40))
    cc = W.cable_car()
    for (ax, ay, _), (bx, by, _) in zip(cc["supports"], cc["supports"][1:]):
        d.line([px(ax, ay), px(bx, by)], fill=(200, 0, 0), width=2)
    for wr in wires:
        d.line([px(*wr["a"][:2]), px(*wr["b"][:2])], fill=(30, 30, 30), width=1)
    for p in poles:
        x, y, _ = p["at"]
        d.ellipse([px(x, y)[0] - 3, px(x, y)[1] - 3, px(x, y)[0] + 3, px(x, y)[1] + 3], fill=(60, 60, 200))
    sx, sy = W.STATION_DOWN
    d.rectangle([px(sx, sy)[0] - 8, px(sx, sy)[1] - 8, px(sx, sy)[0] + 8, px(sx, sy)[1] + 8], outline=(200, 0, 0))
    im.save(path)
    print("plot ->", path)


if __name__ == "__main__":
    main()
