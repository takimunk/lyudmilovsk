"""The sea railway of Lyudmilovsk, resolved against the land (docs/railway.md).

A single track comes out of a tunnel in the sea cliff just south of the club, crosses the beach path at a level
crossing, stops at the beach halt on piles over the water and goes straight out to sea along a low bank, its rails a
hand's breadth under the water at high tide, to a small island 2.3 km off (the island halt) and on, past the island,
to the horizon, where the train fades into the haze; then it comes back.

Authored in unreal/Lyudmilovsk/Content/Railway/railway.json; everything that depends on the ground (where the bank
stands on the beach, how deep the sea is under it, how the rails rise onto the island and out of the tunnel) is worked
out here from tools/terrain.py's ground_z every time:

  python3 tools/railway.py              -> Content/Railway/railway_built.json (+ a report)
  python3 tools/railway.py --plot a.png  also draws the line's profile and plan
  python3 tools/railway.py --check       exits 1 on a problem (rails under the beach, a hole outside the hood, ...)

Frame: tools/world_map.py's map metres (X west, Y north, Z up, origin on the club's portal face). The line runs along
+X (west) at a constant Y; s = distance along it from the tunnel's far end. Pure Python, no bpy or unreal.
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

DATA_DIR = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "Railway")
SOURCE = os.path.join(DATA_DIR, "railway.json")
BUILT = os.path.join(DATA_DIR, "railway_built.json")
LAYOUT = os.path.join(REPO, "blender", "export", "scene01_layout.json")

STEP = 4.0             # metres between samples of the centreline
RAIL_OVER_BALLAST = 0.33   # rail head over the ballast's top (rail 0.15 + sleeper 0.14 + bed)
BANK_TOP_HALF = 2.6    # the bank's crest each side of the centreline (ballast shoulder)
BANK_SLOPE = 1.5       # horizontal per vertical on its sides (armour stones)
SEABED_FAR = W.SEA_Z - 0.3 - 9.0   # the open Baltic past the map: ~9 m deep (world_map's sea bed)


def portal_face_y():
    """Unreal Y (cm) of the map's Y = 0, the club's portal face (as tools/transport.py finds it)."""
    try:
        with open(LAYOUT) as f:
            return json.load(f)["outside"]["portal_face_y"]
    except (OSError, KeyError, ValueError):
        with open(os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "UI", "map.json")) as f:
            return json.load(f)["frame"]["portal_face_y"]


def load(path=SOURCE):
    with open(path) as f:
        return json.load(f)


def seabed_z(x, y):
    """The ground (landscape) or sea bed under a map point; past the landscape the open sea's bed."""
    if T.GRID_X0 <= x <= T.GRID_X0 + T.GRID_NX - 2 and T.GRID_Y0 <= y <= T.GRID_Y0 + T.GRID_NY - 2:
        try:
            return T.ground_z(x, y)
        except (OSError, ValueError, KeyError):
            return W.height(x, y)      # no terrain built in this checkout yet: the macro shape
    return SEABED_FAR


def island_height(src, x, y):
    """The island's own ground over the sea (map Z), or None off it: a low mound of sand and turf with a rocky west
    end, flattened along the track. The Blender island (blender/railway/island.py) samples this same function."""
    isl = src["island"]
    dx, dy = (x - isl["x"]) / isl["rx"], (y - isl["y"]) / isl["ry"]
    r = math.hypot(dx, dy)
    # an irregular outline: a few lobes
    a = math.atan2(dy, dx)
    k = isl.get("seed", 1958)
    rr = 1.0 + 0.10 * math.sin(3 * a + k * 0.37) + 0.06 * math.sin(5 * a + k * 0.11) + 0.04 * math.sin(9 * a + 1.3)
    t = r / rr
    if t >= 1.25:
        return None
    sea = W.SEA_Z
    top = isl["top_over_sea"]
    # a shore apron (wet sand) out to t 1.25 under the water, a beach to t 1.0, the mound inside
    if t >= 1.0:
        return sea - 0.3 - (t - 1.0) / 0.25 * 3.0
    hump = (1.0 - t * t) ** 1.4
    knoll = math.exp(-((dx - 0.45) ** 2 + (dy + 0.25) ** 2) / 0.08) * 0.5   # a rocky knoll at the west end
    h = sea + 0.4 + top * (0.55 * hump + knoll) * min(1.0, (1.0 - t) / 0.18)
    # levelled along the track: the bank's crest on the island, and the halt's platform side a little under its deck
    ln = src["line"]
    level = sea + ln["island_rail_over_sea"] - 0.45
    d = abs(y - ln["y"])
    if d < 16.0:
        k = 1.0 if d < 7.5 else 1.0 - (d - 7.5) / 8.5
        k = k * k * (3 - 2 * k)
        h = h + (min(h, level) - h) * k if h > level else h
    return h


def rail_profile(src):
    """Rail-head Z along the line: a function of x."""
    ln = src["line"]
    isl = src["island"]
    sea_rail = W.SEA_Z - ln["rail_sea_below"]
    portal_x, portal_z, g = ln["portal_x"], ln["rail_portal_z"], ln["grade"]
    isl_rail = W.SEA_Z + ln["island_rail_over_sea"]
    ig = ln["island_grade"]
    half = isl["rx"] * 0.55     # the level stretch on the island round the halt

    def z(x):
        if x <= portal_x:
            return portal_z
        zz = max(sea_rail, portal_z - g * (x - portal_x))
        d = abs(x - isl["x"]) - half
        zi = isl_rail - max(0.0, d) * ig
        return max(zz, zi)
    return z


PLATFORM_EDGE, PLATFORM_W, PLATFORM_UP = 1.55, 3.2, 1.10   # ULyudRailwaySubsystem's PlatformEdge, PlatformWidth, PlatformOverRail
RAMP_GRADE = 0.17


def ground_at(src, x, y):
    g = seabed_z(x, y)
    ih = island_height(src, x, y)
    return max(g, ih) if ih is not None else g


def halt_access(src, st, rz, y):
    """How people get onto a halt's platform: a ramp down off its land end along the line to the beach (on piles over
    the water's edge), or straight in from the ground behind it (on the island, where the ground stands at the deck).
    entrance = the ramp's foot / the step off the back edge, map metres."""
    yc = y + st["side"] * (PLATFORM_EDGE + PLATFORM_W / 2)
    yb = y + st["side"] * (PLATFORM_EDGE + PLATFORM_W + 1.2)
    xm = st["x"] - 8.0
    deck_m = rz(xm) + PLATFORM_UP
    if deck_m - ground_at(src, xm, yb) < 0.6:
        g = ground_at(src, xm, yb)
        return {"entrance": [round(xm, 2), round(yb, 2), round(g, 2)], "ramp": None}
    x0 = st["platform"][0]
    deck = rz(x0) + PLATFORM_UP
    # walk the ramp down toward the tunnel until it meets the ground
    x = x0
    z = deck
    while x > x0 - 40.0:
        x -= 0.5
        z -= 0.5 * RAMP_GRADE
        if z <= ground_at(src, x, yc) + 0.05:
            break
    g = ground_at(src, x, yc)
    return {"entrance": [round(x - 0.8, 2), round(yc, 2), round(g, 2)],
            "ramp": {"top": [x0, round(yc, 2), round(deck, 3)], "foot": [round(x, 2), round(yc, 2), round(g, 3)]}}


def build(src):
    ln = src["line"]
    y = ln["y"]
    x0, x1 = ln["tunnel_start_x"], ln["end_x"]
    rz = rail_profile(src)
    pts = []
    n = int(math.ceil((x1 - x0) / STEP))
    for i in range(n + 1):
        x = min(x0 + i * STEP, x1)
        g = seabed_z(x, y)
        ih = island_height(src, x, y)
        if ih is not None:
            g = max(g, ih)
        pts.append({"s": round(x - x0, 3), "x": round(x, 3), "y": y, "rail": round(rz(x), 3), "ground": round(g, 3)})

    def s_of(x):
        return round(x - x0, 3)

    stops = []
    for st in src["stops"]:
        stops.append(dict(st, s=s_of(st["x"]), rail=round(rz(st["x"]), 3),
                          platform_s=[s_of(st["platform"][0]), s_of(st["platform"][1])], **halt_access(src, st, rz, y)))
    cr = src["crossing"]
    crossing = dict(cr, s=s_of(cr["x"]), y=y, rail=round(rz(cr["x"]), 3), ground=round(seabed_z(cr["x"], y), 3))

    # the tunnel: its mouth (headwall) at portal_x facing the sea (+X), the bore runs east (-X) "depth" metres to a dark
    # end; a hood of concrete covers the cliff's foot over the bore, as far in as the ground stands under its roof
    tu = src["tunnel"]
    px = ln["portal_x"]
    half_out = tu["width"] / 2 + 1.0
    hood_back = px - tu["hood"]
    holes = []
    for i in range(int(math.floor(hood_back)) + 1, int(math.floor(px))):
        for j in range(int(math.floor(y - half_out)) + 1, int(math.ceil(y + half_out)) - 1):
            holes.append([i, j])
    tunnel = dict(tu, mouth_x=px, y=y, rail=ln["rail_portal_z"], back_x=round(px - tu["depth"], 3),
                  hood_back_x=hood_back, outer_half=half_out,
                  ground_at_mouth=round(seabed_z(px + 0.5, y), 3),
                  ground_over_hood_back=round(seabed_z(hood_back, y), 3))

    # where the bank meets the water at high tide (for the wake and the splashes)
    wet = [p["s"] for p in pts if p["rail"] - RAIL_OVER_BALLAST < W.SEA_Z and p["ground"] < W.SEA_Z]
    built = {
        "_doc": "Generated by tools/railway.py from railway.json; do not edit.",
        "sea_z": W.SEA_Z,
        "frame": {"to_unreal": "(X*100, portal_face_y + Y*100, Z*100)", "portal_face_y": portal_face_y()},
        "line": dict(ln, length=round(x1 - x0, 3), step=STEP, rail_over_ballast=RAIL_OVER_BALLAST,
                     bank_top_half=BANK_TOP_HALF, bank_slope=BANK_SLOPE,
                     portal_s=s_of(px), wet_from_s=wet[0] if wet else None),
        "points": pts,
        "stops": stops,
        "crossing": crossing,
        "tunnel": tunnel,
        "terrain_holes": holes,
        "island": dict(src["island"], halt_s=s_of(src["stops"][-1]["x"])),
        "catenary": src["catenary"],
        "train": src["train"],
    }
    return built


def check(built, src):
    problems = []
    pts = built["points"]
    tu = built["tunnel"]
    for p in pts:
        if p["x"] > tu["mouth_x"] + 0.5 and p["x"] < 1300 and p["rail"] - RAIL_OVER_BALLAST < p["ground"] - 0.25 \
                and p["ground"] > W.SEA_Z:
            problems.append("ballast under the beach at x %.0f (rail %.2f ground %.2f)" % (p["x"], p["rail"], p["ground"]))
            break
    if tu["ground_at_mouth"] > tu["rail"] + 0.2:
        problems.append("ground at the mouth %.2f over the rail %.2f: the portal needs a cutting" % (tu["ground_at_mouth"], tu["rail"]))
    roof = tu["rail"] + tu["height"]
    if tu["ground_over_hood_back"] < roof + 0.5:
        problems.append("the hood's back (x %.1f) is not buried: ground %.2f, roof %.2f" % (tu["hood_back_x"], tu["ground_over_hood_back"], roof))
    cr = built["crossing"]
    if cr["rail"] - cr["ground"] > 1.6:
        problems.append("the crossing stands %.1f m over the beach (ramps too steep)" % (cr["rail"] - cr["ground"]))
    return problems


def report(built):
    ln = built["line"]
    print("railway: %.2f km, %d samples, tunnel mouth at s %.0f, wet from s %s" % (ln["length"] / 1000, len(built["points"]), ln["portal_s"], ln["wet_from_s"]))
    for st in built["stops"]:
        print("  stop %-7s s %7.1f  rail %.2f (sea %+.2f)" % (st["id"], st["s"], st["rail"], st["rail"] - W.SEA_Z))
    cr = built["crossing"]
    print("  crossing s %.1f rail %.2f ground %.2f (%.2f m up)" % (cr["s"], cr["rail"], cr["ground"], cr["rail"] - cr["ground"]))
    tu = built["tunnel"]
    print("  tunnel mouth x %.1f ground %.2f, hood back x %.1f ground %.2f (roof %.2f), %d hole quads"
          % (tu["mouth_x"], tu["ground_at_mouth"], tu["hood_back_x"], tu["ground_over_hood_back"], tu["rail"] + tu["height"], len(built["terrain_holes"])))


def plot(built, path):
    from PIL import Image, ImageDraw
    pts = [p for p in built["points"] if p["x"] < 2700]
    w, h = 1800, 500
    img = Image.new("RGB", (w, h), (250, 248, 240))
    d = ImageDraw.Draw(img)
    xs = [p["x"] for p in pts]
    xa, xb = min(xs), max(xs)
    za, zb = W.SEA_Z - 12, W.SEA_Z + 12

    def P(x, z):
        return ((x - xa) / (xb - xa) * (w - 40) + 20, h - 20 - (z - za) / (zb - za) * (h - 40))
    d.line([P(xa, W.SEA_Z), P(xb, W.SEA_Z)], fill=(60, 110, 200), width=2)
    d.line([P(p["x"], p["ground"]) for p in pts], fill=(150, 110, 60), width=2)
    d.line([P(p["x"], p["rail"]) for p in pts], fill=(30, 30, 30), width=2)
    for st in built["stops"]:
        d.text(P(st["x"], st["rail"] + 2), st["id"], fill=(200, 0, 0))
    img.save(path)
    print("plot ->", path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--plot")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--out", default=BUILT)
    a = ap.parse_args()
    src = load()
    built = build(src)
    probs = check(built, src)
    with open(a.out, "w") as f:
        json.dump(built, f, separators=(",", ":"))
    report(built)
    for p in probs:
        print("PROBLEM:", p)
    if a.plot:
        plot(built, a.plot)
    if a.check and probs:
        sys.exit(1)


if __name__ == "__main__":
    main()
