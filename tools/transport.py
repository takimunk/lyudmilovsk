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
DOOR_AHEAD = 0.47        # the cabin's door is this far ahead of its centre (kit.json cabin door x): a stop puts the door on the stop mark
LEVEL_H = 6.5            # metres between the decks of a station on two lines
STATION_R = 9.0          # half size of a station's footprint (deck, tower, stair)
TURN_R = None            # turn wheel radius = track_gap / 2
STEP = 6.0               # metres between rope samples on a span
# the station (docs/transport.md "Station layout v2"): the vestibule stands on its tower at deck height between the
# two tracks, which spread from track_gap to the vestibule's gap G over the station; the cabins stop at its two edges
VESTIBULES = os.path.join(DATA_DIR, "vestibules.json")
VEST_GAP = 24.0          # G when vestibules.json has none (metres between the two tracks along the vestibule)
VEST_LEN = 22.0          # the vestibule's edges' length along the line (the rail the cabins roll on)
EDGE_IN = 1.05           # a boarding edge stands this far inside its track (the cabin's sill is 1.007 m off its centre)
SPREAD_DEG = 6.0         # the line sheaves' deflection: the convergence pylon closes G to track_gap at this angle
WHEEL_RUN = 14.0         # an end station: the turn wheel this far past the vestibule's end (the end frame takes the turn)
TOWER_R = 2.7            # half the tower's core (the ground under it is levelled)


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
    global ln_all
    ln_all = src
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
        # the tower stands under the vestibule's middle (vestibules.json "tower_x" moves it along the top level's axis)
        v = _vest(s["style"])
        a0 = math.radians(s["axes"][s["levels"][-1]["line"]]) if s["levels"] else 0.0
        off = v.get("tower_x", 0.0)
        core = [round(s["at"][0] + math.cos(a0) * off, 3), round(s["at"][1] + math.sin(a0) * off, 3)]
        s["core"] = core
        s["core_ground"] = round(_ground_max(core[0], core[1], TOWER_R) + 0.15, 3)
        s["lift"] = round(max(0.0, s["levels"][-1]["z"] - s["core_ground"]), 3) if s["levels"] else 0.0

    for s in stations:
        v = _vest(s["style"])
        lid = s["levels"][-1]["line"] if s["levels"] else None
        s["vestibule"] = {"at": s["at"], "z": s["levels"][-1]["z"] if s["levels"] else s["deck"],
                          "yaw": s["axes"].get(lid, 0.0) if lid else 0.0, "gap": v["gap"], "length": v["length"]}
        s["geo"] = {}
        for ln in lines:
            if ln["id"] in s["lines"]:
                s["geo"][ln["id"]] = ln["geo"][s["id"]]

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


ln_all = None


def _station_axis(src, lid, sid):
    """The unit axis of line lid at station sid (the bisector of the way in and out), from network.json alone."""
    ln = next(l for l in src["lines"] if l["id"] == lid)
    st = {s["id"]: s for s in src["stations"]}
    stops = ln["stops"]
    i = stops.index(sid)
    pts = []
    for k in (i - 1, i + 1):
        if 0 <= k < len(stops):
            via = ln.get("via", {})
            # the nearest corner toward the neighbour, else the neighbour
            key, rev = ("%s-%s" % (sid, stops[k]), False) if k > i else ("%s-%s" % (stops[k], sid), True)
            c = via.get(key, [])
            p = (c[-1] if rev else c[0]) if c else st[stops[k]]["at"]
            pts.append((p, k > i))
    x, y = st[sid]["at"]
    dirs = [_unit(p[0] - x, p[1] - y) if fwd else _unit(x - p[0], y - p[1]) for p, fwd in pts]
    return _unit(sum(d[0] for d in dirs), sum(d[1] for d in dirs))


def _vest(style, line=None):
    """The vestibule's numbers for the rope (vestibules.json v2, UE cm; defaults when it has none): G, the edges'
    length, where the cabins' doors stop along an edge (from its middle), the tower along the axis. A line served by a
    deck on the vestibule's tower (tower.decks, Zenit's Sun line): that deck's numbers, centred on the tower's axis."""
    out = {"gap": VEST_GAP, "length": VEST_LEN, "stop_x": 0.0, "tower_x": 0.0, "on_tower": False}
    try:
        with open(VESTIBULES) as f:
            v = json.load(f)[style.lower()]
    except (OSError, KeyError, ValueError, TypeError):
        return out
    src = v
    for d in (v.get("tower") or {}).get("decks", []):
        if line and d.get("line") == line:
            src = d
            out["on_tower"] = True
    for k, key in (("gap", "track_gap"), ("length", "edge_length"), ("stop_x", "stop_x")):
        if isinstance(src.get(key), (int, float)):
            out[k] = src[key] / 100.0
    if isinstance(v.get("tower_x"), (int, float)):
        out["tower_x"] = v["tower_x"] / 100.0
    return out


def _line(ln, st_by, st_built, D, probs):
    gap = D["track_gap"]
    stops = ln["stops"]
    via = ln.get("via", {})
    # centre line: station, corners, station, ...
    nodes = []
    for i, sid in enumerate(stops):
        s = st_built[sid]
        level = next(l["z"] for l in s["levels"] if l["line"] == ln["id"])
        v = _vest(s["style"], ln["id"])
        cx, cy = s["at"]
        if v["on_tower"]:
            # a deck on the vestibule's tower: the line's middle is the tower's axis (the top level's axis, tower_x)
            top = s["levels"][-1]["line"]
            a = _station_axis(ln_all, top, sid)
            cx, cy = cx + a[0] * v["tower_x"], cy + a[1] * v["tower_x"]
        nodes.append({"kind": "station", "id": sid, "x": cx, "y": cy,
                      "rope": level + D["rope_over_deck"], "deck": level, "gap": max(gap, v["gap"]),
                      "half_len": v["length"] / 2.0, "stop_x": v["stop_x"]})
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
    # fixed supports along the centre line: each station's two vestibule ends (the tracks G apart along the edges,
    # on the station's axis), past them a convergence pylon where the tracks are track_gap apart again (on the way to
    # the next support, SPREAD_DEG off it), and at an end station a turn wheel past its far end; each corner
    sup = []
    for i, n in enumerate(nodes):
        if n["kind"] != "station":
            sup.append({"x": n["x"], "y": n["y"], "z": None, "kind": "corner"})
            continue
        ax, ay = n["axis"]
        first, last = i == 0, i == len(nodes) - 1
        L, G = n["half_len"], n["gap"]
        run_out = (G - gap) / 2.0 / math.tan(math.radians(SPREAD_DEG))

        def end(e):
            return {"x": n["x"] + ax * e, "y": n["y"] + ay * e, "z": n["rope"], "kind": "station", "station": n["id"],
                    "end": "out" if e > 0 else "in", "half": G / 2.0, "axis": (ax, ay)}

        def converge(e, toward):
            # on the straight from the vestibule's end toward the neighbour, far enough for SPREAD_DEG
            ex, ey = n["x"] + ax * e, n["y"] + ay * e
            dx, dy = _unit(toward["x"] - ex, toward["y"] - ey)
            dist = math.hypot(toward["x"] - ex, toward["y"] - ey)
            d = min(run_out, dist * 0.45)
            if d < run_out - 0.5:
                probs.add("%s/%s: only %.0f m to close the tracks (%.0f wanted)" % (ln["id"], n["id"], d, run_out))
            return {"x": ex + dx * d, "y": ey + dy * d, "z": None, "kind": "converge", "station": n["id"]}

        def wheel(e):
            return {"x": n["x"] + ax * e, "y": n["y"] + ay * e, "z": n["rope"], "kind": "wheel", "station": n["id"],
                    "half": gap / 2.0, "axis": (ax, ay)}

        if first:
            sup += [wheel(-L - WHEEL_RUN), end(-L), end(L), converge(L, nodes[i + 1])]
        elif last:
            sup += [converge(-L, nodes[i - 1]), end(-L), end(L), wheel(L + WHEEL_RUN)]
        else:
            sup += [converge(-L, nodes[i - 1]), end(-L), end(L), converge(L, nodes[i + 1])]
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
    # auto pylons on every span between supports (a station's own supports, its ends, wheel and convergence, never)
    out = [sup[0]]
    for a, b in zip(sup, sup[1:]):
        if a.get("station") and a.get("station") == b.get("station"):
            out.append(b)
            continue
        out.extend(_span(a, b, D, probs, ln["id"]))
        out.append(b)
    sup = out
    pylons = []
    for i, s in enumerate(sup):
        if s["kind"] in ("station", "wheel"):
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
                       "kind": "corner" if s["kind"] == "corner" or turn > 2.0 else "line",
                       "converge": s["kind"] == "converge"})
    loop, stops_s, length, spans, geo = _loop(nodes, sup, D, ln["id"])
    stations = [{"id": n["id"], "yaw": round(_yaw_deg(*n["axis"]), 3), "deck": n["deck"], "rope": n["rope"]}
                for n in nodes if n["kind"] == "station"]
    return {"id": ln["id"], "name": ln["name"], "color": ln["color"], "number": ln.get("number", 0),
            "stops": stops, "depot": ln.get("depot", stops[-1]), "stations": stations,
            "pylons": pylons, "supports": [[round(s["x"], 3), round(s["y"], 3), round(s["z"], 3)] for s in sup],
            "loop": loop, "loop_length": round(length, 3), "stop_s": stops_s, "spans": spans, "geo": geo}


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
    """Support i moved sideways by side * its half gap (a station's own supports: square off the station's axis;
    others: track_gap / 2, mitred at corners). One side is +normal, the other -normal."""
    s = sup[i]
    if s.get("axis"):
        ax, ay = s["axis"]
        nx, ny = -ay, ax
        k = side * s["half"]
        return (s["x"] + nx * k, s["y"] + ny * k, s["z"])
    dirs = []
    if i > 0:
        dirs.append(_unit(s["x"] - sup[i - 1]["x"], s["y"] - sup[i - 1]["y"]))
    if i + 1 < len(sup):
        dirs.append(_unit(sup[i + 1]["x"] - s["x"], sup[i + 1]["y"] - s["y"]))
    tx, ty = _unit(sum(d[0] for d in dirs), sum(d[1] for d in dirs))
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
            # a station's own rail: between its two ends, and from an end to its turn wheel (no sag, no free span)
            rail = a.get("station") and a.get("station") == b.get("station") and a["kind"] in ("station", "wheel") \
                and b["kind"] in ("station", "wheel")
            span = math.hypot(pb[0] - pa[0], pb[1] - pa[1])
            n = 1 if rail else max(2, int(span / STEP))
            s_pa = None
            for m in range(n + 1):
                t = m / n
                z = pa[2] + (pb[2] - pa[2]) * t
                if not rail:
                    z -= 4.0 * t * (1.0 - t) * span * D["sag"]
                add((pa[0] + (pb[0] - pa[0]) * t, pa[1] + (pb[1] - pa[1]) * t, z))
                if m == 0:
                    s_pa = pts[-1][3]
            if not rail and span > 1.0:
                spans.append([round(s_pa, 3), round(pts[-1][3], 3)])
            if rail and a["kind"] == "station" and b["kind"] == "station":
                # along an edge: the door (DOOR_AHEAD in front of the cabin's middle) stops at the edge's stop mark
                # (stop_x from its middle, along the line's way out); an end station: only on the arriving edge
                nd = next(x for x in nodes if x["kind"] == "station" and x["id"] == a["station"])
                k_st = [x["id"] for x in nodes if x["kind"] == "station"]
                first, last = nd["id"] == k_st[0], nd["id"] == k_st[-1]
                arriving = (last and side > 0) or (first and side < 0)
                if (first or last) and not arriving:
                    continue
                mark = span / 2.0 + (nd["stop_x"] if side > 0 else -nd["stop_x"])
                stops.append({"station": a["station"], "dir": 0 if side > 0 else 1,
                              "s": round(s_pa + mark - DOOR_AHEAD, 3), "end": first or last})

    def turn(i, side_from):
        """Half circle round the turn wheel at support i, from track side_from to the other, through the far side."""
        s = sup[i]
        ax, ay = s["axis"]
        n = next(nd for nd in nodes if nd["kind"] == "station" and nd["id"] == s["station"])
        # the far side: away from the station's middle
        if (s["x"] - n["x"]) * ax + (s["y"] - n["y"]) * ay < 0:
            ax, ay = -ax, -ay
        nx, ny = -s["axis"][1], s["axis"][0]
        r = gap / 2.0
        steps = 12
        for m in range(steps + 1):
            a = math.pi * m / steps
            vx = side_from * nx * math.cos(a) + ax * math.sin(a)
            vy = side_from * ny * math.cos(a) + ay * math.sin(a)
            add((s["x"] + vx * r, s["y"] + vy * r, s["z"]))

    idx = list(range(len(sup)))
    # out: from the first station's wheel to the last one's, on side +1; round its wheel; back on side -1
    run(+1, idx)
    turn(len(sup) - 1, +1)
    run(-1, list(reversed(idx)))
    turn(0, -1)
    # close the loop
    first = pts[0]
    add((first[0], first[1], first[2]))
    length = pts[-1][3]
    pts[-1][3] = length
    loop = [[round(v, 3) for v in p] for p in pts]
    stops.sort(key=lambda s: s["s"])
    # per station: what the game builds (its gap, edges, stop mark, wheel, and the sheaves where the ropes bend)
    geo = {}
    for nd in nodes:
        if nd["kind"] != "station":
            continue
        g = {"centre": [round(nd["x"], 3), round(nd["y"], 3)], "yaw": round(_yaw_deg(*nd["axis"]), 3),
             "gap": round(nd["gap"], 3), "edge": round(nd["gap"] / 2.0 - EDGE_IN, 3), "length": round(nd["half_len"] * 2.0, 3),
             "stop_x": round(nd["stop_x"], 3), "rope": round(nd["rope"], 3), "wheel": None, "sheaves": []}
        for i, sp in enumerate(sup):
            if sp.get("station") != nd["id"]:
                continue
            if sp["kind"] == "wheel":
                g["wheel"] = [round(sp["x"], 3), round(sp["y"], 3), round(sp["z"], 3)]
                continue
            if i == 0 or i + 1 >= len(sup):
                continue
            for side in (1, -1):
                p0, p1, p2 = (_offset(sup, k, side, gap) for k in (i - 1, i, i + 1))
                d1 = _unit(p1[0] - p0[0], p1[1] - p0[1])
                d2 = _unit(p2[0] - p1[0], p2[1] - p1[1])
                ang = math.degrees(math.atan2(d1[0] * d2[1] - d1[1] * d2[0], d1[0] * d2[0] + d1[1] * d2[1]))
                g["sheaves"].append({"at": [round(p1[0], 3), round(p1[1], 3)], "z": round(sp["z"], 3), "side": side,
                                     "kind": sp["kind"], "turn_deg": round(ang, 2),
                                     "yaw": round(_yaw_deg(d1[0] + d2[0], d1[1] + d2[1]), 3)})
        geo[nd["id"]] = g
    return loop, stops, length, spans, geo


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
