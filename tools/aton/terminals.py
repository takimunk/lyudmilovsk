"""Where the lab network's terminals stand (docs/aton_lab.md): one in the scientists' library of the Academgorodok
(Novikova 1, ground floor; blender/academgorodok/library.py), at the hall end of the last stack, beside the globe.

    python3 tools/aton/terminals.py           -> Content/AtonOS/terminals.json, Content/Dev/teleports/aton.json
    python3 tools/aton/terminals.py --check   fail if they are stale or the desk no longer fits the room

terminals.json: Unreal metres like Content/Academgorodok/fountain.json (X as the map, Y = portal face + map Y, Z up);
"at" is the middle of the desk's front edge on the floor, "yaw" turns the actor's +X toward the person at the desk
(ALyudLabTerminal, Source/Lyudmilovsk/Private/AtonLab.cpp: the desk is 1.2 m wide and 0.62 m deep behind that edge).
The room's furniture comes from library.py itself (needs Blender's bpy, pip install bpy): the desk and the person's
place must stay clear of every footprint, so a moved bookcase fails the check instead of swallowing the terminal.
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
ACADEM = os.path.join(ROOT, "blender", "academgorodok")
sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.path.insert(0, ACADEM)
import world_map as W  # noqa: E402
import plan as P  # noqa: E402
from geom import Frame  # noqa: E402

CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
OUT = os.path.join(CONTENT, "AtonOS", "terminals.json")
OUT_TP = os.path.join(CONTENT, "Dev", "teleports", "aton.json")
DESK_W, DESK_D = 1.2, 0.62
SEAT = 1.0          # the floor kept clear in front of the desk (the stool, the person)
FLOOR = 0.7         # gastronom.Z0: the ground floor over the ensemble's ground

# (id, house, x of the desk's middle, y of its front edge, the way the person faces: +1 = the house's +y)
TERMINALS = [("library", "Novikova1", 12.835, -1.25, 1)]


def footprints():
    """library.py's furniture, [(name, x0, x1, y0, y1)] in the house frame; None without bpy."""
    try:
        import library
        library.build()
        return list(library.FOOTPRINTS), library.HALL
    except Exception as e:  # no bpy here: the place is then trusted as written
        print("terminals: library.py did not build (%s); the fit is not checked" % str(e)[:80])
        return None, None


def main():
    check = "--check" in sys.argv
    bad = 0
    try:
        with open(os.path.join(ROOT, "blender", "export", "scene01_layout.json")) as f:
            face_y = json.load(f)["outside"]["portal_face_y"]
    except (OSError, KeyError):
        face_y = 790.0
    prints, hall = footprints()
    terminals, teleports = [], []
    for tid, house, x, y, facing in TERMINALS:
        cx, cy, yaw_deg, mirror = P.FRAMES[house]
        frame = Frame(cx, cy, math.radians(yaw_deg), mirror=mirror)

        def to_map(hx, hy):
            ex, ey, _ = frame((hx, hy, 0.0))
            return W.academ_to_map(ex, ey)

        # the desk behind its front edge, and the floor the person takes in front of it
        desk = (x - DESK_W / 2, x + DESK_W / 2, min(y, y + facing * DESK_D), max(y, y + facing * DESK_D))
        seat = (x - 0.35, x + 0.35, min(y, y - facing * SEAT), max(y, y - facing * SEAT))
        if prints is not None:
            for name, x0, x1, y0, y1 in prints:
                for what, box in (("desk", desk), ("seat", seat)):
                    if box[0] < x1 and x0 < box[1] and box[2] < y1 and y0 < box[3]:
                        print("terminal %s: its %s stands in %s" % (tid, what, name)); bad += 1
            for what, box in (("desk", desk), ("seat", seat)):
                if box[0] < hall[0] or box[1] > hall[1] or box[2] < hall[2] or box[3] > hall[3]:
                    print("terminal %s: its %s is outside the hall" % (tid, what)); bad += 1
        mx, my = to_map(x, y)
        px, py = to_map(x, y - facing * 1.0)          # a metre in front: toward the person
        yaw = math.degrees(math.atan2(py - my, px - mx))
        z = round(W.academ_z() + FLOOR, 2)
        terminals.append({"id": tid, "at": [round(mx, 3), round(my + face_y / 100.0, 3), z], "yaw": round(yaw, 2)})
        sx, sy = to_map(x, y - facing * 1.3)
        teleports.append({"id": "aton_terminal" if tid == "library" else "aton_terminal_" + tid, "group": "aton",
                          "name": "dev.tp.aton_terminal", "x": round(sx, 2), "y": round(sy, 2), "z": round(z * 100.0, 1),
                          "yaw": round((yaw + 180.0 + 180.0) % 360.0 - 180.0, 1)})
    docs = [(OUT, {"about": "Written by tools/aton/terminals.py: Unreal metres, the desk's front edge on the floor; yaw turns +X to the person.",
                   "face_y_cm": face_y, "terminals": terminals}),
            (OUT_TP, {"teleports": teleports})]
    for path, doc in docs:
        text = json.dumps(doc, ensure_ascii=False, indent=1) + "\n"
        if check:
            if not os.path.isfile(path) or open(path, encoding="utf-8").read() != text:
                print("%s is stale: run python3 tools/aton/terminals.py" % os.path.relpath(path, ROOT)); bad += 1
        else:
            os.makedirs(os.path.dirname(path), exist_ok=True)
            open(path, "w", encoding="utf-8").write(text)
    print("terminals: %d, %d problems" % (len(terminals), bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
