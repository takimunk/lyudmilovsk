"""Counters with real service (docs/counters.md, backlog item 27): queues at the town's counters, and the market's stalls.

  python3 tools/town/counters.py           rewrites the counters' slots in Content/PeopleData/town_places.json
  python3 tools/town/counters.py --check   fails if town_places.json is not what this script makes of it now

tools/town/town_life.py runs this at its end. Running it again changes nothing: it is idempotent.

- The market (P-MARKET). Its sellers used to stand at random points of the hall and its shoppers by the door, ten metres
  from any of them, so nobody was ever served (LyudBarks::AtCounter pairs a customer with a seller within 4 m). Now each
  seller stands at a stall's seller spot (shell_market.json stallN), and the shoppers queue at that stall's counter spot
  (counterN): the meat stall gets the butcher, two fish stalls get the fishmongers, and the sellers take the rest in
  their order.
- Every kiosk, shop and cafe whose shoppers stand at a counter (a visit slot within 2.5 m of a staff slot) gets a queue.
  That slot becomes q0, the one being served, and a line of spots goes back from it, away from the counter, 0.85 m apart.
  The slots carry "queue": <id> and "q": <n>. The people's life (PeopleSim) fills a line in order and moves people up as
  the front leaves; the hero standing at a spot holds it (FWorld::SetHeroAt).
"""
import json
import math
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
PLACES = os.path.join(CONTENT, "PeopleData", "town_places.json")
PLAN = os.path.join(CONTENT, "City", "city_plan.json")
MARKET_SHELL = os.path.join(ROOT, "blender", "export", "city", "shell_market.json")

SPACING = 0.85          # m between two in a line
NEAR_COUNTER = 2.5      # m: a visit slot this near a staff slot is at the counter
LINE = {"kiosk": 4, "shop": 4, "cafe": 3, "market": 3}   # spots in a line, the front included
# which of the market's stalls each trade gets (x_p_market.json: counter3 the old butcher, counter6 and counter11 the fish)
MARKET_STALLS = {"butcher": [3, 15], "fishmonger": [6, 11], "seller": [1, 2, 4, 5, 7, 9, 10, 14, 13, 12, 8, 16]}


def local_to_world(front, yaw, local):
    """Building frame (x along the front, y into the plot, z up; metres) -> Unreal metres (as tools/city/interiors.py)."""
    a = math.radians(yaw)
    x, y, z = local
    return [front[0] + x * math.sin(a) - y * math.cos(a), front[1] - x * math.cos(a) - y * math.sin(a), front[2] + z]


def world_yaw(plot_yaw, local_yaw):
    return round((plot_yaw - local_yaw) % 360, 1)


def r2(p):
    return [round(p[0], 2), round(p[1], 2), round(p[2], 2)]


def line_back(q0, away, n, yaw, queue):
    """q0 and the spots behind it, `away` the unit direction from the counter toward the shoppers."""
    out = []
    for i in range(n):
        p = [q0[0] + away[0] * SPACING * i, q0[1] + away[1] * SPACING * i, q0[2]]
        out.append({"kind": "visit", "pos": r2(p), "yaw": yaw, "queue": queue, "q": i})
    return out


def market(place, plan):
    """The market's sellers at their stalls, its shoppers in a line at each staffed stall's counter."""
    shell = json.load(open(MARKET_SHELL, encoding="utf-8"))
    plot = next(p for p in plan["plots"] if p["id"] == place["id"])
    front = [plot["front"][0], plot["front"][1] + plan["portal_face_y"] / 100.0, plot["front"][2]]
    spots = {s["id"]: s for s in shell["spots"]}
    staff = [s for s in place["slots"] if s["kind"] == "staff"]
    others = [s for s in place["slots"] if s["kind"] not in ("staff", "visit")]
    taken, slots, lines = set(), [], []
    for s in staff:
        role = s.get("tag") or "seller"
        stall = next((n for n in MARKET_STALLS.get(role, MARKET_STALLS["seller"]) if n not in taken), None)
        if stall is None:
            stall = next(n for n in range(1, 17) if n not in taken)
        taken.add(stall)
        st, co = spots["stall%d" % stall], spots["counter%d" % stall]
        sp = local_to_world(front, plot["yaw"], st["pos"])
        cp = local_to_world(front, plot["yaw"], co["pos"])
        slots.append({"kind": "staff", "tag": role, "pos": r2(sp), "yaw": world_yaw(plot["yaw"], st.get("yaw", 0))})
        d = [cp[0] - sp[0], cp[1] - sp[1]]
        n = math.hypot(*d) or 1.0
        lines += line_back(cp, [d[0] / n, d[1] / n], LINE["market"], world_yaw(plot["yaw"], co.get("yaw", 0)),
                           "%s/%d" % (place["id"], stall))
    place["slots"] = slots + lines + others


def queues(place):
    """A line from each visit slot at a counter."""
    n = LINE.get(place["kind"])
    if not n or place.get("shell"):
        return
    staff = [s for s in place["slots"] if s["kind"] == "staff" and "pos" in s]
    # drop the lines a previous run added, keep each line's front as the plain visit slot it was
    base = [dict((k, v) for k, v in s.items() if k not in ("queue", "q")) for s in place["slots"] if s.get("q", 0) == 0]
    out, k = [], 0
    for s in base:
        if s["kind"] != "visit" or "pos" not in s or not staff:
            out.append(s)
            continue
        near = min(staff, key=lambda t: math.hypot(t["pos"][0] - s["pos"][0], t["pos"][1] - s["pos"][1]))
        d = [s["pos"][0] - near["pos"][0], s["pos"][1] - near["pos"][1]]
        dist = math.hypot(*d)
        if dist > NEAR_COUNTER or dist < 0.2:
            out.append(s)
            continue
        line = line_back(s["pos"], [d[0] / dist, d[1] / dist], n, s.get("yaw", 0.0), "%s/%d" % (place["id"], k))
        for extra in ("capacity",):
            if extra in s:
                line[0][extra] = s[extra]
        out += line
        k += 1
    place["slots"] = out


def build(data, plan):
    for place in data["places"]:
        if place["id"] == "P-MARKET":
            market(place, plan)
        else:
            queues(place)
    return data


def dump(data):
    return json.dumps(data, ensure_ascii=False, indent=1) + "\n"


def main():
    old = open(PLACES, encoding="utf-8").read()
    data = build(json.loads(old), json.load(open(PLAN, encoding="utf-8")))
    text = dump(data)
    lines = sum(1 for p in data["places"] for s in p["slots"] if s.get("q") == 0)
    spots = sum(1 for p in data["places"] for s in p["slots"] if "q" in s)
    if "--check" in sys.argv:
        if text != old:
            print("ERROR: %s is not what tools/town/counters.py makes of it: run it" % os.path.relpath(PLACES, ROOT))
            sys.exit(1)
        print("counters: ok (%d queues, %d spots)" % (lines, spots))
        return
    with open(PLACES, "w", encoding="utf-8") as f:
        f.write(text)
    print("counters: %d queues, %d spots in %s" % (lines, spots, os.path.relpath(PLACES, ROOT)))


if __name__ == "__main__":
    main()
