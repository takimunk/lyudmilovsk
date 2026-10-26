"""The cable-car metro of Lyudmilovsk, resolved against the land (docs/transport.md).

The network is authored in unreal/Lyudmilovsk/Content/Transport/network.json: stations (where, how high
their boarding deck stands, which design), lines (the order of their stops, corner pylons between them,
the depot), the cabins. Everything that depends on the ground is worked out here, every time, from
tools/terrain.py's ground_z: so when the relief changes or a station moves, rerun this and the lifts
get shorter or longer, the pylons taller or lower, new pylons appear where the rope would scrape the
ground. Pure Python, no bpy or unreal.

  python3 tools/transport.py            -> Content/Transport/network_built.json (+ a report)
  python3 tools/transport.py --plot map.png   also draws the network over the map
  python3 tools/transport.py --check    exits 1 on any problem (rope too low, deck under ground, ...)

Frame: tools/world_map.py's map metres (X west, Y north, Z up, origin on the club's portal face).
A station's deck stands at a fixed height over the sea level of the map ("deck": metres), or right on
the ground ("deck": "ground", Aton on the club's hill). A station on two lines has a level per line,
LEVEL_H apart, both served by its lift and stair.

Each line is one closed loop of rope, the way a cabin goes round: out along the right-hand track from
the first stop to the last, round the last station's turn wheel, back along the other track, round
the first station's wheel. A cabin's place on its line is the distance s along the loop. In a station
the cabins leave the rope and roll slowly along a straight rail past the platform (a through station)
or round the turn (an end station).
"""
import argparse
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, ".."))
sys.path.insert(0, HERE)
import terrain as T  # noqa: E402
import world_map as W  # noqa: E402

DATA_DIR = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "Transport")
SOURCE = os.path.join(DATA_DIR, "network.json")
BUILT = os.path.join(DATA_DIR, "network_built.json")
LAYOUT = os.path.join(REPO, "blender", "export", "scene01_layout.json")

ON_GROUND_RISE = 5.0     # a station on the ground: its platform this much over the highest ground under it (the gallery passes under its track)
STOP_THROUGH = 5.5       # a through station: cabins stop this far along the line past its centre (the stair and lift are in the middle)
STOP_END = 2.5           # an end station: this far from its centre (before the turn wheel), on the platform's side
CORE_END = 7.0           # an end station's stair and lift: this far from its centre, away from the wheel
LEVEL_H = 6.5            # metres between the decks of a station on two lines
STATION_R = 9.0          # half size of a station's footprint (deck, tower, stair)
TURN_R = None            # turn wheel radius = track_gap / 2
STEP = 6.0               # metres between rope samples on a span
# the vestibule (docs/transport.md "Station layout and the vestibule connector"): beside the line on the ground, its
# connector this far off the line's axis, joined to the tower by a ground gallery under the track
VEST_OFF = 9.5
VEST_DEPTH = 22.0        # the footprint's size away from the connector (X < 0 in its frame), when vestibules.json has none
VEST_HALF = 11.0         # and across
VEST_HEIGHT = 16.0
GALLERY_CLEAR = 4.2      # deck over the gallery's floor at least (its roof 3.4 m, the cabin's floor, a margin)
VESTIBULES = os.path.join(DATA_DIR, "vestibules.json")


def load_source(path=SOURCE):
    with open(path) as f:
        return json.load(f)


def _ground(x, y):
    return T.ground_z(x, y)


def _ground_max(x, y, r):
    """Highest ground under a square of half size r (what a tower or deck must stand over)."""
    best = -1e9
    n = 4
    for i in range(-n, n + 1):
        for j in range(-n, n + 1):
            best = max(best, _ground(x + r * i / n, y + r * j / n))
    return best


def _ground_min(x, y, r):
    best = 1e9
    n = 3
    for i in range(-n, n + 1):
        for j in range(-n, n + 1):
            best = min(best, _ground(x + r * i / n, y + r * j / n))
    return best


def _unit(dx, dy):
    l = math.hypot(dx, dy) or 1.0
    return dx / l, dy / l


def _yaw_deg(dx, dy):
    """Unreal yaw of a map direction: map X is Unreal X, map Y is Unreal Y (same handedness)."""
    return math.degrees(math.atan2(dy, dx))


class Problems(list):
    def add(self, msg):
        self.append(msg)


def resolve(src=None, log=print):
    """network.json -> the built network (dict, map metres)."""
    src = src or load_source()
    D = dict(src["defaults"])
    gap = D["track_gap"]
    probs = Problems()
    st_by = {s["id"]: s for s in src["stations"]}
    lines_at = {}
    for ln in src["lines"]:
        for sid in ln["stops"]:
            lines_at.setdefault(sid, []).append(ln["id"])

    # --- stations: ground, deck, levels, lift ------------------------------------------------
    stations = []
    for s in src["stations"]:
        x, y = s["at"]
        g = round(_ground(x, y), 3)
        gmax = _ground_max(x, y, STATION_R)
        gmin = _ground_min(x, y, STATION_R)
        on_ground = s.get("deck") == "ground"
        if on_ground:
            deck = round(gmax + ON_GROUND_RISE, 3)
        else:
            deck = float(s["deck"])
            if deck < gmax + D["min_lift"]:
                probs.add("%s: deck %.1f is only %.1f m over the ground (%.1f); raised to %.1f"
                          % (s["id"], deck, deck - gmax, gmax, gmax + D["min_lift"]))
                deck = round(gmax + D["min_lift"], 3)
        lines = lines_at.get(s["id"], [])
        levels = [{"line": lid, "z": round(deck + k * LEVEL_H, 3)} for k, lid in enumerate(lines)]
        top = levels[-1]["z"] if levels else deck
        stations.append({
            "id": s["id"], "name": s["name"], "style": s.get("style", "saucer"), "at": [x, y],
            "ground": g, "ground_min": round(gmin, 3), "ground_max": round(gmax, 3),
            "on_ground": on_ground, "deck": deck, "levels": levels,
            "lift": round(max(0.0, top - g), 3), "lines": lines,
            "schema": s.get("schema"),
        })
    st_built = {s["id"]: s for s in stations}

    # --- lines -------------------------------------------------------------------------------
    lines = []
    for ln in src["lines"]:
        lines.append(_line(ln, st_by, st_built, D, probs))
    for s in stations:
        s["axes"] = {}
        s["ends"] = {}
        for ln in lines:
            for k, st in enumerate(ln["stations"]):
                if st["id"] == s["id"]:
                    s["axes"][ln["id"]] = st["yaw"]
                    # an end station's platform lies on the line's side of the wheel: -1 the last stop, +1 the first
                    s["ends"][ln["id"]] = -1 if k == len(ln["stations"]) - 1 else (1 if k == 0 else 0)
        # the stair and lift: in the middle, or at an end station CORE_END from the wheel along its platform
        core = [s["at"][0], s["at"][1]]
        for lid, e in s["ends"].items():
            if e and len(s["lines"]) == 1:
                a = math.radians(s["axes"][lid])
                core = [round(s["at"][0] + e * CORE_END * math.cos(a), 3), round(s["at"][1] + e * CORE_END * math.sin(a), 3)]
        s["core"] = core
        # the tower's and the gallery's floor 15 cm over the highest ground under them (the terrain never shows
        # through it): the tower's square and the strip square off the first level's axis either way, to VEST_OFF
        g = _ground_max(core[0], core[1], 2.7)
        if s["levels"]:
            a0 = math.radians(s["axes"][s["levels"][0]["line"]])
            nx, ny = -math.sin(a0), math.cos(a0)
            for k in range(-int(VEST_OFF), int(VEST_OFF) + 1):
                for w in (-1.7, 0.0, 1.7):
                    g = max(g, _ground(core[0] + nx * k + math.cos(a0) * w, core[1] + ny * k + math.sin(a0) * w))
        s["core_ground"] = round(g + 0.15, 3)
        s["lift"] = round(max(0.0, s["levels"][-1]["z"] - s["core_ground"]), 3) if s["levels"] else 0.0

    for s in stations:
        _vestibule(s, st_by[s["id"]], lines, probs)

    face_y = None
    try:
        with open(LAYOUT) as f:
            face_y = json.load(f)["outside"]["portal_face_y"]
    except (OSError, KeyError, ValueError):
        # the committed layout has no "outside": the UI's map frame keeps the same number (tools/ui_map.py)
        try:
            with open(os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "UI", "map.json")) as f:
                face_y = json.load(f)["frame"]["portal_face_y"]
        except (OSError, KeyError, ValueError):
            probs.add("no portal_face_y (scene01_layout.json outside, Content/UI/map.json frame): Unreal positions can't be placed")

    board = None
    if "club_board" in src:
        bx, by = src["club_board"]["at"]
        fx, fy = src["club_board"].get("facing", [0.0, 1.0])
        board = {"at": [bx, by], "z": round(_ground(bx, by), 3), "yaw": _yaw_deg(fx, fy)}

    built = {
        "comment": "Made by tools/transport.py from network.json and the terrain: do not edit, rerun it.",
        "frame": {"units": "map metres; Unreal cm = (X*100, portal_face_y + Y*100, Z*100)", "portal_face_y": face_y},
        "defaults": D, "level_h": LEVEL_H,
        "stations": stations, "lines": lines, "cabins": src["cabins"], "club_board": board,
        "problems": list(probs),
    }
    for p in probs:
        log("transport: " + p)
    return built


def _vest_box(style):
    """The vestibule's footprint box in its connector frame (metres): x0, x1, y0, y1, height."""
    try:
        with open(VESTIBULES) as f:
            v = json.load(f)["styles"][style]
        pts = v["footprint"]
        xs = [p[0] / 100.0 for p in pts]
        ys = [p[1] / 100.0 for p in pts]
        return min(xs), max(xs), min(ys), max(ys), v.get("height", VEST_HEIGHT * 100) / 100.0
    except (OSError, KeyError, ValueError, TypeError):
        return -VEST_DEPTH, 0.0, -VEST_HALF, VEST_HALF, VEST_HEIGHT


def _vestibule(s, src, lines, probs):
    """Where the station's vestibule stands: beside the first level's line, the connector VEST_OFF + gap off its axis
    at the tower's ground, +X of its frame toward the tower. The side: network.json's or the one with the flatter
    ground and nothing over it."""
    if not s["levels"]:
        return
    lid = s["levels"][0]["line"]
    a = math.radians(s["axes"][lid])
    ax, ay = math.cos(a), math.sin(a)
    nx, ny = -ay, ax                                    # left of the axis
    want = src.get("vestibule", {})
    gap = float(want.get("gap", 0.0))
    x0, x1, y0, y1, height = _vest_box(s["style"])
    cx, cy = s["core"]
    choices = []
    for side in ([want["side"]] if "side" in want else [1, -1]):
        px, py = cx + nx * side * (VEST_OFF + gap), cy + ny * side * (VEST_OFF + gap)
        yaw = _yaw_deg(-nx * side, -ny * side)          # toward the tower
        fx, fy = -nx * side, -ny * side                 # the frame's +X (map)
        gx, gy = -fy, fx                                # its +Y
        ground = []
        for i in range(7):
            for j in range(7):
                u = x0 + (x1 - x0) * i / 6.0
                v = y0 + (y1 - y0) * j / 6.0
                ground.append(_ground(px + fx * u + gx * v, py + fy * u + gy * v))
        # ropes over the footprint (another line crossing): their lowest height there
        low = None
        for ln in lines:
            for q in ln["loop"]:
                dx, dy = q[0] - px, q[1] - py
                u, v = dx * fx + dy * fy, dx * gx + dy * gy
                if x0 - 2.0 < u < x1 + 2.0 and y0 - 2.0 < v < y1 + 2.0:
                    low = q[2] if low is None else min(low, q[2])
        floor = max(s["core_ground"], max(ground) + 0.1)
        score = (max(ground) - min(ground)) + (floor - s["core_ground"]) * 2.0
        if low is not None and low - 4.5 < floor + height:
            score += 1000.0
        choices.append((score, side, px, py, yaw, floor, min(ground), max(ground), low))
    score, side, px, py, yaw, floor, gmin, gmax, low = min(choices)
    deck = s["levels"][0]["z"]
    if deck - s["core_ground"] < GALLERY_CLEAR:
        probs.add("%s: the deck is only %.1f m over the gallery (needs %.1f)" % (s["id"], deck - s["core_ground"], GALLERY_CLEAR))
    if low is not None and low - 4.5 < floor + height:
        probs.add("%s: a rope runs %.1f m over the vestibule's floor (it is up to %.0f m tall)" % (s["id"], low - floor, height))
    if gmax - gmin > 6.0:
        probs.add("%s: the ground under the vestibule falls %.1f m (a tall podium)" % (s["id"], gmax - gmin))
    s["vestibule"] = {"at": [round(px, 3), round(py, 3)], "z": round(floor, 3), "yaw": round(yaw, 3), "side": side,
                      "ground_min": round(gmin, 3), "ground_max": round(gmax, 3), "box": [x0, x1, y0, y1, height]}


def _line(ln, st_by, st_built, D, probs):
    gap = D["track_gap"]
    half_plat = D["platform_len"] / 2.0
    stops = ln["stops"]
    via = ln.get("via", {})
    # centre line: station, corners, station, ...
    nodes = []
    for i, sid in enumerate(stops):
        s = st_built[sid]
        level = next(l["z"] for l in s["levels"] if l["line"] == ln["id"])
        nodes.append({"kind": "station", "id": sid, "x": s["at"][0], "y": s["at"][1],
                      "rope": level + D["rope_over_deck"], "deck": level})
        if i + 1 < len(stops):
            for c in via.get("%s-%s" % (sid, stops[i + 1]), []):
                nodes.append({"kind": "corner", "x": c[0], "y": c[1]})
            for c in reversed(via.get("%s-%s" % (stops[i + 1], sid), [])):
                nodes.append({"kind": "corner", "x": c[0], "y": c[1]})
    # station axes: the bisector of the way in and out (an end station: its one way)
    for i, n in enumerate(nodes):
        if n["kind"] != "station":
            continue
        dirs = []
        if i > 0:
            dirs.append(_unit(n["x"] - nodes[i - 1]["x"], n["y"] - nodes[i - 1]["y"]))
        if i + 1 < len(nodes):
            dirs.append(_unit(nodes[i + 1]["x"] - n["x"], nodes[i + 1]["y"] - n["y"]))
        ax, ay = _unit(sum(d[0] for d in dirs), sum(d[1] for d in dirs))
        n["axis"] = (ax, ay)
        if len(dirs) == 2:
            turn = math.degrees(math.acos(max(-1.0, min(1.0, dirs[0][0] * dirs[1][0] + dirs[0][1] * dirs[1][1]))))
            if turn > 75.0:
                probs.add("%s/%s: the line turns %.0f degrees in the station (more than 75)" % (ln["id"], n["id"], turn))
    # fixed supports along the centre line: each station's two platform ends, each corner
    sup = []
    for i, n in enumerate(nodes):
        if n["kind"] == "station":
            ax, ay = n["axis"]
            first, last = i == 0, i == len(nodes) - 1
            ends = []
            if not first:
                ends.append(-half_plat)
            if not last:
                ends.append(half_plat)
            if first:
                ends = [half_plat]
            if last:
                ends = [-half_plat]
            for e in ends:
                sup.append({"x": n["x"] + ax * e, "y": n["y"] + ay * e, "z": n["rope"], "kind": "station",
                            "station": n["id"], "end": "out" if e > 0 else "in"})
        else:
            sup.append({"x": n["x"], "y": n["y"], "z": None, "kind": "corner"})
    # corner heights: on the straight line between the stations' ropes, over the clearance
    for i, s in enumerate(sup):
        if s["z"] is not None:
            continue
        a = next(j for j in range(i - 1, -1, -1) if sup[j]["kind"] == "station")
        b = next(j for j in range(i + 1, len(sup)) if sup[j]["kind"] == "station")
        da = _dist_along(sup, a, i)
        db = _dist_along(sup, i, b)
        z = sup[a]["z"] + (sup[b]["z"] - sup[a]["z"]) * da / max(da + db, 1e-6)
        g = _ground(s["x"], s["y"])
        s["z"] = min(max(z, g + D["pylon_min"]), g + D["pylon_max"])
    # auto pylons on every span between supports (station ends of one station are one rail: skip)
    out = [sup[0]]
    for a, b in zip(sup, sup[1:]):
        if a["kind"] == "station" and b["kind"] == "station" and a["station"] == b["station"]:
            out.append(b)
            continue
        out.extend(_span(a, b, D, probs, ln["id"]))
        out.append(b)
    sup = out
    pylons = []
    for i, s in enumerate(sup):
        if s["kind"] == "station":
            continue
        g = _ground(s["x"], s["y"])
        # the line's direction here (corners: the bisector)
        p, q = sup[i - 1], sup[i + 1]
        d1 = _unit(s["x"] - p["x"], s["y"] - p["y"])
        d2 = _unit(q["x"] - s["x"], q["y"] - s["y"])
        bis = _unit(d1[0] + d2[0], d1[1] + d2[1])
        turn = math.degrees(math.acos(max(-1.0, min(1.0, d1[0] * d2[0] + d1[1] * d2[1]))))
        h = s["z"] - g
        if h > D["pylon_max"] + 0.01:
            probs.add("%s: pylon at (%.0f, %.0f) is %.1f m tall (max %.0f)" % (ln["id"], s["x"], s["y"], h, D["pylon_max"]))
        pylons.append({"x": round(s["x"], 3), "y": round(s["y"], 3), "ground": round(g, 3), "top": round(s["z"], 3),
                       "height": round(h, 3), "yaw": round(_yaw_deg(*bis), 3), "turn": round(turn, 2),
                       "kind": "corner" if s["kind"] == "corner" or turn > 2.0 else "line"})
    loop, stops_s, length, spans = _loop(nodes, sup, D, ln["id"])
    stations = [{"id": n["id"], "yaw": round(_yaw_deg(*n["axis"]), 3), "deck": n["deck"], "rope": n["rope"]}
                for n in nodes if n["kind"] == "station"]
    return {"id": ln["id"], "name": ln["name"], "color": ln["color"], "number": ln.get("number", 0),
            "stops": stops, "depot": ln.get("depot", stops[-1]), "stations": stations,
            "pylons": pylons, "supports": [[round(s["x"], 3), round(s["y"], 3), round(s["z"], 3)] for s in sup],
            "loop": loop, "loop_length": round(length, 3), "stop_s": stops_s, "spans": spans}


def _dist_along(sup, i, j):
    return sum(math.hypot(sup[k + 1]["x"] - sup[k]["x"], sup[k + 1]["y"] - sup[k]["y"]) for k in range(i, j))


def _sagged(a, b, t, sag):
    span = math.hypot(b["x"] - a["x"], b["y"] - a["y"])
    return a["z"] + (b["z"] - a["z"]) * t - 4.0 * t * (1.0 - t) * span * sag


def _need(p, q, t, a, b, D):
    """Clearance the rope must keep at t of span p-q: min_clearance, eased off near a station whose rope
    hangs lower over its own ground (a station on the ground: the rope comes in at deck height)."""
    need = D["min_clearance"]
    x, y = p["x"] + (q["x"] - p["x"]) * t, p["y"] + (q["y"] - p["y"]) * t
    for s in (a, b):
        if s["kind"] != "station":
            continue
        c0 = s["z"] - _ground(s["x"], s["y"])
        d = math.hypot(x - s["x"], y - s["y"])
        need = min(need, c0 + d * 0.2)
    return need


def _span(a, b, D, probs, lid):
    """Pylons between supports a and b so the sagging rope keeps min_clearance over the ground
    (and no span is longer than max_span)."""
    pts = [a, b]
    for _ in range(40):
        worst = None
        for p, q in zip(pts, pts[1:]):
            span = math.hypot(q["x"] - p["x"], q["y"] - p["y"])
            n = max(2, int(span / STEP))
            for k in range(1, n):
                t = k / n
                x, y = p["x"] + (q["x"] - p["x"]) * t, p["y"] + (q["y"] - p["y"]) * t
                c = _sagged(p, q, t, D["sag"]) - _ground(x, y) - _need(p, q, t, a, b, D)
                if c < 0.0 and (worst is None or c < worst[0]):
                    worst = (c, x, y, p, q, t)
        if worst is None:
            # no low rope: split the longest span if it is too long
            p, q = max(zip(pts, pts[1:]), key=lambda pq: math.hypot(pq[1]["x"] - pq[0]["x"], pq[1]["y"] - pq[0]["y"]))
            if math.hypot(q["x"] - p["x"], q["y"] - p["y"]) <= D["max_span"]:
                break
            worst = (0.0, (p["x"] + q["x"]) / 2, (p["y"] + q["y"]) / 2, p, q, 0.5)
        _, x, y, p, q, t = worst
        g = _ground(x, y)
        straight = p["z"] + (q["z"] - p["z"]) * t
        z = max(straight, g + D["min_clearance"] + 3.0, g + D["pylon_min"])
        z = min(z, g + D["pylon_max"])
        new = {"x": x, "y": y, "z": z, "kind": "line"}
        pts.insert(pts.index(q), new)
    else:
        probs.add("%s: could not keep the rope over the ground between (%.0f,%.0f) and (%.0f,%.0f)"
                  % (lid, a["x"], a["y"], b["x"], b["y"]))
    # final check
    for p, q in zip(pts, pts[1:]):
        span = math.hypot(q["x"] - p["x"], q["y"] - p["y"])
        n = max(2, int(span / STEP))
        for k in range(1, n):
            t = k / n
            x, y = p["x"] + (q["x"] - p["x"]) * t, p["y"] + (q["y"] - p["y"]) * t
            c = _sagged(p, q, t, D["sag"]) - _ground(x, y)
            if c < _need(p, q, t, a, b, D) - 0.5:
                probs.add("%s: rope only %.1f m over the ground at (%.0f, %.0f)" % (lid, c, x, y))
                break
    return pts[1:-1]


def _offset(sup, i, side, gap):
    """Support i moved sideways by side*gap/2 (right of the way out = +1), mitred at corners."""
    s = sup[i]
    dirs = []
    if i > 0:
        dirs.append(_unit(s["x"] - sup[i - 1]["x"], s["y"] - sup[i - 1]["y"]))
    if i + 1 < len(sup):
        dirs.append(_unit(sup[i + 1]["x"] - s["x"], sup[i + 1]["y"] - s["y"]))
    tx, ty = _unit(sum(d[0] for d in dirs), sum(d[1] for d in dirs))
    # right of (tx, ty) in a frame with X west and Y north, seen from above with north up and west left:
    # (tx, ty) -> right = (ty, -tx)?  X west means the map is mirrored against a usual (east, north)
    # frame, so right of the direction is (-ty, tx)... keep it simple: one side is +normal, the other -normal
    nx, ny = -ty, tx
    cos_half = max(0.3, abs(dirs[0][0] * tx + dirs[0][1] * ty)) if dirs else 1.0
    k = side * gap / 2.0 / cos_half
    return (s["x"] + nx * k, s["y"] + ny * k, s["z"])


def _loop(nodes, sup, D, lid):
    """The loop a cabin rides: points [x, y, z, s] (rope height) and where it stops."""
    gap = D["track_gap"]
    pts = []
    stops = []
    spans = []          # free spans of rope between supports, [s0, s1] (the game swings and loads them)

    def add(p):
        if pts:
            l = pts[-1]
            d = math.dist(p, l[:3])
            if d < 1e-3:
                return
            pts.append([p[0], p[1], p[2], l[3] + d])
        else:
            pts.append([p[0], p[1], p[2], 0.0])

    def run(side, order):
        for k in range(len(order) - 1):
            i, j = order[k], order[k + 1]
            a, b = sup[i], sup[j]
            pa, pb = _offset(sup, i, side, gap), _offset(sup, j, side, gap)
            same_station = a["kind"] == "station" and b["kind"] == "station" and a["station"] == b["station"]
            span = math.hypot(pb[0] - pa[0], pb[1] - pa[1])
            n = 1 if same_station else max(2, int(span / STEP))
            s_pa = None
            for m in range(n + 1):
                t = m / n
                z = pa[2] + (pb[2] - pa[2]) * t
                if not same_station:
                    z -= 4.0 * t * (1.0 - t) * span * D["sag"]
                add((pa[0] + (pb[0] - pa[0]) * t, pa[1] + (pb[1] - pa[1]) * t, z))
                if m == 0:
                    s_pa = pts[-1][3]
            if not same_station and span > 1.0:
                spans.append([round(s_pa, 3), round(pts[-1][3], 3)])
            if same_station:
                # the stop: STOP_THROUGH past the station's centre along the line, whichever way the cabin goes
                along = span / 2.0 + STOP_THROUGH if side > 0 else span / 2.0 - STOP_THROUGH
                stops.append({"station": a["station"], "dir": 0 if side > 0 else 1,
                              "s": round(s_pa + along, 3), "end": False})

    def turn(i, side_from, facing_out):
        """Half circle round station node i's turn wheel, beyond its platform end, from track side_from to
        the other; the stop is at its apex."""
        s = sup[i]
        n = next(nd for nd in nodes if nd["kind"] == "station" and nd["id"] == s["station"])
        ax, ay = n["axis"]
        if not facing_out:
            ax, ay = -ax, -ay
        cx, cy = n["x"], n["y"]          # the wheel turns round the station's centre
        nx, ny = -n["axis"][1], n["axis"][0]
        r = gap / 2.0
        steps = 12
        # the stop: on the way in, STOP_END before the centre (the wheel turns round the centre)
        stops.append({"station": n["id"], "dir": 0 if side_from > 0 else 1,
                      "s": round(pts[-1][3] + D["platform_len"] / 2.0 - STOP_END, 3), "end": True})
        # start angle: at +side normal, sweeping through the far axis
        for m in range(steps + 1):
            a = math.pi * m / steps
            # vector = side_from*normal*cos(a) + far_axis*sin(a)
            vx = side_from * nx * math.cos(a) + ax * math.sin(a) * 1.0
            vy = side_from * ny * math.cos(a) + ay * math.sin(a) * 1.0
            add((cx + vx * r, cy + vy * r, n["rope"]))

    idx = list(range(len(sup)))
    # out: first station's platform end ... last station's platform end, on side +1
    run(+1, idx)
    turn(len(sup) - 1, +1, True)
    run(-1, list(reversed(idx)))
    turn(0, -1, False)
    # close the loop
    first = pts[0]
    add((first[0], first[1], first[2]))
    length = pts[-1][3]
    pts[-1][3] = length
    loop = [[round(v, 3) for v in p] for p in pts]
    stops.sort(key=lambda s: s["s"])
    return loop, stops, length, spans


def report(b):
    lines = []
    for s in b["stations"]:
        lines.append("%-8s ground %6.1f  deck %6.1f  lift %5.1f m  levels %s  %s" % (
            s["id"], s["ground"], s["deck"], s["lift"], [l["line"] for l in s["levels"]], s["style"]))
    for ln in b["lines"]:
        lines.append("line %s: %s, loop %.0f m, %d pylons (%s), stops %s" % (
            ln["id"], " - ".join(ln["stops"]), ln["loop_length"], len(ln["pylons"]),
            ", ".join("%.0f" % p["height"] for p in ln["pylons"]),
            ", ".join("%s%s@%.0f" % (s["station"], "*" if s["end"] else "", s["s"]) for s in ln["stop_s"])))
    for p in b["problems"]:
        lines.append("PROBLEM " + p)
    return "\n".join(lines)


def plot(b, out):
    from PIL import Image, ImageDraw
    PX = 2.0
    w = int((W.X_MAX - W.X_MIN) / PX)
    h = int((W.Y_MAX - W.Y_MIN) / PX)

    def px(x, y):
        return (W.X_MAX - x) / PX, (W.Y_MAX - y) / PX

    img = Image.new("RGB", (w, h))
    pix = img.load()
    for j in range(h):
        y = W.Y_MAX - (j + 0.5) * PX
        for i in range(w):
            x = W.X_MAX - (i + 0.5) * PX
            z = _ground(x, y)
            if z < W.SEA_Z:
                pix[i, j] = (70, 110, 140)
            else:
                t = max(0.0, min(1.0, (z - W.SEA_Z) / 70.0))
                pix[i, j] = (int(90 + 120 * t), int(110 + 100 * t), int(70 + 80 * t))
    d = ImageDraw.Draw(img)
    for ln in b["lines"]:
        col = tuple(int(ln["color"][k:k + 2], 16) for k in (1, 3, 5))
        d.line([px(p[0], p[1]) for p in ln["loop"]], fill=col, width=2)
        for p in ln["pylons"]:
            x, y = px(p["x"], p["y"])
            r = 4 if p["kind"] == "corner" else 2
            d.ellipse((x - r, y - r, x + r, y + r), fill=(250, 250, 250) if p["kind"] == "corner" else (30, 30, 30))
    for s in b["stations"]:
        x, y = px(*s["at"])
        d.ellipse((x - 7, y - 7, x + 7, y + 7), outline=(255, 255, 255), width=3)
        d.text((x + 9, y - 6), "%s %.0f/%.0f" % (s["id"], s["deck"], s["lift"]), fill=(255, 255, 255))
    img.save(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=BUILT)
    ap.add_argument("--plot")
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    b = resolve()
    with open(a.out, "w") as f:
        json.dump(b, f, indent=1, ensure_ascii=False)
        f.write("\n")
    print(report(b))
    if a.plot:
        plot(b, a.plot)
    if a.check and b["problems"]:
        sys.exit(1)


if __name__ == "__main__":
    main()
