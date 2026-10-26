"""Puts the lobby pieces into a hall's furniture list (docs/transport.md "Lobbies"). Pure Python (no Blender), so vestibules.py
calls it from furnish() and it can also be run on its own on Content/Transport/vestibules.json:

  python3 blender/transport/lobby_layout.py

The layout table is Content/Transport/lobby.json "layout": per style a list of replacements (the first record of the item `from`
becomes the lobby piece `item`, moved toward the hall's middle if its back is deeper than the wall slot it took), and the hall's
"centre" piece (a chandelier, a pendulum, a lighthouse lens). Idempotent: a record already replaced is not found again.
"""
import json
import math
import os

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
LOBBY = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Transport", "lobby.json")
VEST = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Transport", "vestibules.json")
DEPTH = {"Bench": 0.6, "EggPair": 0.9}      # vestibules.py PATTERN: the depth of the wall slot a piece took (the item sits dep/2 + 4 cm off the wall)


def apply(style, furniture, table=None):
    """The furniture list (records {item, at (UE cm), yaw, ...}) with the lobby's pieces; returns (list, lobby dict or None)."""
    if table is None:
        with open(LOBBY, encoding="utf-8") as f:
            table = json.load(f)["layout"]
    spec = table.get(style)
    if not spec:
        return furniture, None
    out = [dict(r) for r in furniture]
    for rep in spec.get("replace", []):
        if any(r["item"] == rep["item"] for r in out):
            continue                                       # already done (idempotent)
        for r in out:
            if r["item"] == rep["from"]:
                dep = DEPTH.get(rep["from"], 0.6)
                shift = max(0.0, rep["back"] + 0.02 - (dep / 2 + 0.04)) * 100.0
                a = math.radians(r["yaw"])
                r["item"] = rep["item"]
                r["at"] = [round(r["at"][0] + math.cos(a) * shift, 1), round(r["at"][1] + math.sin(a) * shift, 1), r["at"][2]]
                if "stock" in rep:
                    r["stock"] = rep["stock"]
                r.pop("plant", None)
                r.pop("poster", None)
                break
    lobby = {"centre": spec["centre"]} if "centre" in spec else None
    return out, lobby


def place_centre(V):
    """The hall's middle is the glazed core (the stair and the lift), so a hanging piece goes into the ring round it: on the side of the core
    (one of the four) whose spot is farthest from the door, the stations' own spots and the furniture. Sets V["lobby"]["at"] (station frame, UE cm)."""
    lobby = V.get("lobby")
    if not lobby:
        return
    x0, y0, x1, y1 = V["floor_hole"]
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    room = V["room"]
    rc = room["centre"]
    obstacles = [d["at"] for d in V["doors"]] + [s["at"] for s in V["spots"].values()] + [f["at"] for f in V["furniture"]]
    best = None
    ex = V.get("core_exit", "")                                 # the side the core's stair comes out on: keep it free
    for dx, dy, half in ((1, 0, (x1 - x0) / 2), (-1, 0, (x1 - x0) / 2), (0, 1, (y1 - y0) / 2), (0, -1, (y1 - y0) / 2)):
        if ex and ex[1] == ("x" if dx else "y") and (ex[0] == "+") == (dx + dy > 0):
            continue
        p = (cx + dx * (half + 125), cy + dy * (half + 125))
        if math.hypot(p[0] - rc[0], p[1] - rc[1]) > room["radius"] - 120:
            continue
        score = min(math.hypot(p[0] - o[0], p[1] - o[1]) for o in obstacles)
        if best is None or score > best[0]:
            best = (score, p)
    if best:
        lobby["at"] = [round(best[1][0], 1), round(best[1][1], 1)]


def main():
    with open(VEST, encoding="utf-8") as f:
        text = f.read()
    d = json.loads(text)
    with open(LOBBY, encoding="utf-8") as f:
        table = json.load(f)["layout"]
    for style, V in d.items():
        if style.startswith("_"):
            continue
        V["furniture"], lobby = apply(style, V.get("furniture", []), table)
        if lobby:
            V["lobby"] = lobby
            place_centre(V)
        print(style, [r["item"] for r in V["furniture"]], lobby or "")
    with open(VEST, "w") as f:                                  # vestibules.py's own format
        json.dump(d, f, indent=1, sort_keys=True)


if __name__ == "__main__":
    main()
