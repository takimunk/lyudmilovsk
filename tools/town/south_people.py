#!/usr/bin/env python3
"""The townsfolk on the Aton coast (docs/south.md "People", backlog item 37): old fishermen mending nets by the boat shed at
the brook's mouth in the morning and looking over the boats in the evening, a beekeeper at the old orchard's hives on warm
days, families at its picnic table at the weekend, birdwatchers in the hide when the starlings come in at dusk, walkers up
the shoulder trail to the belvedere and back, people sitting on the belvedere's benches, children at the footbridge's
stones and the spring after school. Nobody there at night; the rain sends them home (the life's own weather rule).

Like tools/town/town_activities.py this adds to the town generator's output and is idempotent (run it again after
tools/town/town_life.py, and after tools/south.py moved a thing or a trail):
    python3 tools/town/south_people.py            add or refresh
    python3 tools/town/south_people.py --check    exit 1 when the files are not as this script makes them

Reads Content/South/south.json (things, trails) and the meshes' manifests (blender/export/south|north/manifest_*.json:
"seats" [x, y, z, yaw] in the mesh's Blender frame, the hide's stair). Writes:
  Content/PeopleData/town_places.json   the places of PLACE_IDS at the end of the list
  Content/PeopleData/activities.json    the rows of ACTIVITIES_ROWS (after the last row, one a line)
  Content/PeopleData/townsfolk.json     the likes of LIKES (the fishermen, the beekeeper, the birdwatchers)

Frames: south.json is in map metres (docs/world_map.md, x west, y north); the town's places are the same metres with y moved
by the club's portal face (city_plan.json portal_face_y, cm), as town_life.py writes them. A thing's yaw is the Unreal yaw of
its front; a mesh's Blender (x, y) is its actor's local (x, -y) and the actor turns by yaw - front_yaw (import_north.py
spawn_mesh); a seat with Blender yaw s faces the thing's yaw - s (import_south.py _spots). A seat slot's "pos" is the floor
under the cushion (the cushion 0.45 over it), as everywhere in the life.
"""
import collections
import glob
import json
import math
import os
import sys
import zlib

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import terrain as T         # noqa: E402  (the built ground: the same heights the things stand on)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import town_activities as TA     # noqa: E402  (its places and rows stay last)

CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
FOLK = os.path.join(CONTENT, "PeopleData", "townsfolk.json")
PLACES = os.path.join(CONTENT, "PeopleData", "town_places.json")
ACTIVITIES = os.path.join(CONTENT, "PeopleData", "activities.json")
PLAN = os.path.join(CONTENT, "City", "city_plan.json")
SOUTH = os.path.join(CONTENT, "South", "south.json")
GARDEN = os.path.join(CONTENT, "Garden", "garden.json")
MANIFESTS = [os.path.join(ROOT, "blender", "export", d, "manifest*.json") for d in ("south", "north")]
OD = collections.OrderedDict

PLACE_IDS = ["S-ATON-SHED", "S-ATON-ORCHARD", "S-ATON-HIDE", "S-ATON-VIEW", "S-ATON-WALK", "S-ATON-BROOK"]
SEAT_H = 0.45

# activities.json rows, one a line. Low bases: the fishermen, the beekeeper and the birdwatchers come by their likes; the
# walk, the benches, the picnic and the brook draw anybody now and then (the coast is a long way up from the terraces).
ACTIVITIES_ROWS = [
    '{"id": "mend_nets", "places": ["aton_shed"], "slots": ["shed_bench", "net_rack"], "pose": "sit", "minutes": [60, 150], "hours": [6, 11], "daylight": 1, "ages": [40, 200], "temp": [-3, 40], "meets": {"mind": 0.3, "nature": 0.7, "social": 0.15}, "costs": {"hunger": 0.04}, "base": -2.4, "trait": "nature"}',
    '{"id": "boats_evening", "places": ["aton_shed"], "slots": ["boat"], "pose": "look", "minutes": [25, 60], "hours": [16, 19.5], "ages": [40, 200], "temp": [-3, 40], "meets": {"nature": 0.8, "sun": 0.5}, "base": -2.4, "trait": "nature"}',
    '{"id": "tend_hives", "places": ["aton_orchard"], "slots": ["hive"], "pose": "stand", "minutes": [30, 80], "hours": [9, 17], "daylight": 1, "temp": [13, 40], "meets": {"nature": 0.9, "mind": 0.3, "sun": 0.3}, "costs": {"tired": 0.06}, "base": -2.6, "trait": "nature"}',
    '{"id": "orchard_picnic", "places": ["aton_orchard"], "slots": ["picnic"], "pose": "sit", "minutes": [40, 90], "hours": [11, 17.5], "days": [5, 6], "daylight": 1, "temp": [15, 40], "meets": {"hunger": 0.5, "social": 0.7, "nature": 0.6, "sun": 0.3}, "base": -0.7, "trait": "social", "talk": 1}',
    '{"id": "watch_starlings", "places": ["aton_hide"], "slots": ["hide_seat"], "pose": "sit", "minutes": [30, 60], "ages": [10, 200], "temp": [-6, 40], "meets": {"nature": 1.2, "mind": 0.5, "sun": 0.4}, "base": -1.9, "trait": "nature", "sunset": 1}',
    '{"id": "sit_belvedere", "places": ["aton_view"], "slots": ["bench"], "pose": "sit", "minutes": [15, 40], "hours": [8, 20.5], "daylight": 1, "temp": [3, 40], "meets": {"nature": 0.9, "sun": 0.7, "social": 0.15}, "base": -0.3, "day_more": {"5": 0.3, "6": 0.4}, "trait": "sun", "talk": 1}',
    '{"id": "walk_aton", "places": ["aton_walk"], "slots": ["trail"], "pose": "walk", "minutes": [45, 80], "hours": [9, 19], "daylight": 1, "ages": [8, 200], "temp": [3, 40], "meets": {"nature": 1.0, "sun": 0.35, "fun": 0.15}, "costs": {"hunger": 0.06, "tired": 0.04}, "base": -0.2, "day_more": {"5": 0.35, "6": 0.3}, "trait": "nature", "wander": 1, "round": 1, "wander_pause": [6, 25]}',
    '{"id": "play_brook", "places": ["aton_brook"], "slots": ["stone", "trough"], "pose": "look", "minutes": [20, 45], "hours": [14, 18.5], "days": [0, 1, 2, 3, 4, 5], "daylight": 1, "ages": [6, 13], "temp": [8, 40], "meets": {"fun": 0.9, "nature": 0.7}, "costs": {"tired": 0.05, "hunger": 0.06}, "base": 0.1, "trait": "active", "wander": 1, "wander_m": [2, 60], "wander_pause": [30, 120], "pace": 1.4}',
]
ACT_IDS = [json.loads(r)["id"] for r in ACTIVITIES_ROWS]
OWN_LIKES = ("mend_nets", "boats_evening", "tend_hives", "watch_starlings")

# who: the old fishermen (men of 58 and over who like the shore), one beekeeper (a pensioner fond of nature), a few
# birdwatchers (the studious who love the open air); the same people while the town is the same
FISHERMEN, BEEKEEPERS, BIRDERS = 3, 1, 8
# people who already have a pastime of their own somewhere (the allotment's beds, the piano, the sanatorium) are left to it
BUSY_LIKES = {"tend_allotment", "water_allotment", "play_piano", "san_cure", "dominoes"}
# the walk: every this many metres along the trail a spot (the round goes up and back down by them)
WALK_STEP = 16.0
# the brook: the stones within this of the footbridge, and the spring's trough
BROOK_R = 70.0


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f, object_pairs_hook=OD)


def dump(d):
    return json.dumps(d, ensure_ascii=False, indent=1) + "\n"


class Coast:
    """south.json's things and trails, the manifests' rows, and the turn into the town's frame."""

    def __init__(self):
        self.data = load(SOUTH)
        self.dy = load(PLAN)["portal_face_y"] / 100.0
        self.rows = {}
        self.front = {}
        for pat in MANIFESTS:
            for f in sorted(glob.glob(pat)):
                m = load(f)
                for r in m.get("meshes", []):
                    self.rows[(r["kind"], int(r.get("variant", 0)))] = r
                    self.front[(r["kind"], int(r.get("variant", 0)))] = float(m.get("front_yaw", 90.0))

    def things(self, kind):
        return [t for t in self.data["things"] if t["kind"] == kind]

    def thing(self, tid):
        t = next((t for t in self.data["things"] if t["id"] == tid), None)
        if t is None:
            sys.exit("south_people: %s is not a thing in south.json (python3 tools/south.py)" % tid)
        return t

    def row(self, t):
        r = self.rows.get((t["kind"], int(t.get("variant", 0)))) or self.rows.get((t["kind"], 0))
        if r is None:
            sys.exit("south_people: no manifest row for %s (python3 blender/south/build_south.py)" % t["kind"])
        return r

    def ground(self, t):
        return float(t["z"]) if t.get("z") is not None else T.ground_z(t["x"], t["y"])

    def at(self, x, y, z):
        """Map metres -> the town's frame."""
        return [round(x, 2), round(y + self.dy, 2), round(z, 2)]

    def local(self, t, bx, by, bz=0.0):
        """A point of a thing's mesh (Blender frame) in map metres."""
        r = math.radians(float(t.get("yaw", 0.0)) - self.front.get((t["kind"], int(t.get("variant", 0))), 90.0))
        lx, ly = bx, -by
        return (t["x"] + lx * math.cos(r) - ly * math.sin(r), t["y"] + lx * math.sin(r) + ly * math.cos(r), self.ground(t) + bz)

    def seats(self, t, kind):
        """Seat slots of a thing (its manifest's "seats")."""
        out = []
        for s in self.row(t).get("seats") or []:
            x, y, z = self.local(t, s[0], s[1], s[2] - SEAT_H)
            yaw = (float(t.get("yaw", 0.0)) - s[3]) % 360.0
            a = math.radians(yaw)
            out.append(OD([("kind", kind), ("pos", self.at(x, y, z)), ("yaw", round(yaw, 1)), ("height", SEAT_H),
                           ("approach", self.at(x + 0.55 * math.cos(a), y + 0.55 * math.sin(a), z))]))
        return out

    def stand_by(self, t, kind, ahead, side=0.0):
        """A standing slot in front of a thing (ahead m along its front, side m to its right), facing it."""
        yaw = math.radians(float(t.get("yaw", 0.0)))
        x = t["x"] + ahead * math.cos(yaw) - side * math.sin(yaw)
        y = t["y"] + ahead * math.sin(yaw) + side * math.cos(yaw)
        return OD([("kind", kind), ("pos", self.at(x, y, T.ground_z(x, y))), ("yaw", round((float(t.get("yaw", 0.0)) + 180.0) % 360.0, 1))])

    def trail(self, name):
        pts = self.data["trails"].get(name)
        if not pts:
            sys.exit("south_people: south.json has no trail %s" % name)
        return [(p[0], p[1]) for p in pts]


def resample(pts, step):
    """Points every step metres along a polyline, both ends kept."""
    out = [pts[0]]
    carry = 0.0
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        seg = math.hypot(bx - ax, by - ay)
        d = step - carry
        while d < seg:
            out.append((ax + (bx - ax) * d / seg, ay + (by - ay) * d / seg))
            d += step
        carry = seg - (d - step)
    if math.hypot(out[-1][0] - pts[-1][0], out[-1][1] - pts[-1][1]) > step * 0.35:
        out.append(pts[-1])
    else:
        out[-1] = pts[-1]
    return out


def heading(a, b):
    return round(math.degrees(math.atan2(b[1] - a[1], b[0] - a[0])) % 360.0, 1)


def place(pid, kind, c, pos, slots, yaw=0.0, **more):
    p = OD([("id", pid), ("kind", kind), ("pos", c.at(*pos)), ("yaw", yaw)])
    p.update(more)
    p["slots"] = slots
    return p


def shed(c):
    """The boat shed at the brook's mouth: its bench on the west wall, the two net racks, the two boats keel up."""
    t = c.things("fisher_shed")[0]
    slots = c.seats(t, "shed_bench")
    slots += [c.stand_by(r, "net_rack", 0.9) for r in c.things("net_rack")]
    slots += [c.stand_by(b, "boat", 1.6, 0.6) for b in c.things("rowboat")]
    return place("S-ATON-SHED", "aton_shed", c, (t["x"], t["y"], c.ground(t)), slots)


def orchard(c):
    """The old orchard: a place in front of each hive (the beekeeper), the picnic table's four seats."""
    hives = c.things("beehive")
    slots = [c.stand_by(h, "hive", 0.85) for h in hives]
    t = c.things("picnic_table")[0]
    slots += c.seats(t, "picnic")
    return place("S-ATON-ORCHARD", "aton_orchard", c, (t["x"], t["y"], c.ground(t)), slots)


def hide(c):
    """The bird hide on its stilts: in at the stair's foot, up to the landing, along the bench to a seat at the slit. Under the
    sky as far as the weather goes ("open")."""
    t = c.things("bird_hide")[0]
    r = c.row(t)
    foot = c.local(t, *r["stair_foot"])
    top = c.local(t, *r["stair_top"])
    seats = c.seats(t, "hide_seat")
    for s in seats:
        s["approach"] = [s["pos"][0], s["pos"][1], s["pos"][2]]
        # from the landing through the door and over to the seat (inside the hide, 0.6 m behind the bench)
        back = c.local(t, 0.0, 0.3, r["floor_z"])
        s["path"] = [c.at(*back)]
    return OD([("id", "S-ATON-HIDE"), ("kind", "aton_hide"), ("door", c.at(foot[0], foot[1], T.ground_z(foot[0], foot[1]))),
               ("yaw", round((float(t.get("yaw", 0.0)) + 180.0) % 360.0, 1)), ("open", 1), ("slots", seats), ("roles", []),
               ("path", [c.at(*top)])])


def view(c):
    """The belvedere's four benches and the brow benches by it and on the shoulder's last bend (under the birch)."""
    b = c.things("belvedere")[0]
    slots = c.seats(b, "bench")
    for t in c.things("brow_bench"):
        if math.hypot(t["x"] - b["x"], t["y"] - b["y"]) < 30.0 or math.hypot(t["x"] + 165.0, t["y"] - 265.0) < 25.0:
            slots += c.seats(t, "bench")
    return place("S-ATON-VIEW", "aton_view", c, (b["x"], b["y"], c.ground(b)), slots)


def walk(c):
    """Up the shoulder trail from the town's end to the alley, out to the belvedere, and back down the same way: spots every
    WALK_STEP m in that order (a round goes from spot to spot, so the walkers keep to the trail)."""
    up = list(reversed(c.trail("town")))           # the trail runs from the alley's end down to the town
    out = c.trail("belvedere")
    if math.hypot(up[-1][0] - out[0][0], up[-1][1] - out[0][1]) > 3.0:
        sys.exit("south_people: the belvedere trail does not start where the town trail ends")
    line = resample(up + out[1:], WALK_STEP)
    there_back = line + list(reversed(line))[1:-1]
    slots = []
    for i, p in enumerate(there_back):
        q = there_back[(i + 1) % len(there_back)]
        slots.append(OD([("kind", "trail"), ("pos", c.at(p[0], p[1], T.ground_z(*p))), ("yaw", heading(p, q))]))
    return place("S-ATON-WALK", "aton_walk", c, (line[0][0], line[0][1], T.ground_z(*line[0])), slots)


def brook(c):
    """The children's brook: the stones by the footbridge, the spring's trough."""
    br = c.data["bridge"]
    slots = []
    for t in c.things("river_boulder"):
        if math.hypot(t["x"] - br["x"], t["y"] - br["y"]) < BROOK_R:
            s = c.stand_by(t, "stone", 1.1)
            slots.append(s)
    for t in c.things("spring_trough"):
        slots += [c.stand_by(t, "trough", 1.0, -0.5), c.stand_by(t, "trough", 1.0, 0.5)]
    if len(slots) < 3:
        sys.exit("south_people: hardly any stones by the footbridge (%d)" % len(slots))
    return place("S-ATON-BROOK", "aton_brook", c, (br["x"], br["y"], T.ground_z(br["x"], br["y"])), slots)


def pick(people, n, fit, salt):
    """n people who fit, the same while the town is the same."""
    cand = sorted((p for p in people if fit(p)), key=lambda p: zlib.crc32((salt + p["key"]).encode()))
    if len(cand) < n:
        sys.exit("south_people: only %d fit for %s" % (len(cand), salt))
    return cand[:n]


def temper(p, k):
    return float((p.get("temper") or {}).get(k, 0.0))


def build():
    out = {}
    c = Coast()
    pd = load(PLACES)
    places = [p for p in pd["places"] if p["id"] not in PLACE_IDS]
    new = [shed(c), orchard(c), hide(c), view(c), walk(c), brook(c)]
    assert [p["id"] for p in new] == PLACE_IDS
    # before town_activities.py's places and rows, which that script keeps last: either script run again leaves the other's
    at = next((i for i, p in enumerate(places) if p["id"] in TA.PLACE_IDS), len(places))
    pd["places"] = places[:at] + new + places[at:]
    out[PLACES] = dump(pd)

    fd = load(FOLK)
    people = [p for p in fd["people"] if p.get("town")]
    for p in fd["people"]:
        for a in OWN_LIKES:
            (p.get("likes") or {}).pop(a, None)
    fishers = pick(people, FISHERMEN, lambda p: not p.get("female") and p["age"] >= 58 and p.get("occupation") in ("pensioner", "fisherman")
                   and "fish_shore" in (p.get("likes") or {}), "nets:")
    busy = {p["key"] for p in people if set(p.get("likes") or {}) & BUSY_LIKES or p.get("occupation") == "guest"}
    busy |= {b.get("keeper") for b in load(GARDEN).get("beds", [])}       # Maarja and the beds' keepers: the allotment's
    keeper = pick(people, BEEKEEPERS, lambda p: p.get("occupation") == "pensioner" and temper(p, "nature") >= 0.6
                  and p not in fishers and p["key"] not in busy, "bees:")
    birders = pick(people, BIRDERS, lambda p: p["age"] >= 14 and temper(p, "nature") >= 0.6 and temper(p, "studious") >= 0.5
                   and p not in fishers and p not in keeper, "birds:")
    for p in fishers:
        p.setdefault("likes", OD())["mend_nets"] = 2.9
        p["likes"]["boats_evening"] = 2.7
    for p in keeper:
        p.setdefault("likes", OD())["tend_hives"] = 3.6
    for p in birders:
        p.setdefault("likes", OD())["watch_starlings"] = 2.4
    out[FOLK] = dump(fd)

    # activities.json is written by hand, one activity a line: ours after the last row
    text = open(ACTIVITIES, encoding="utf-8").read()
    lines = [l for l in text.split("\n") if not any('"id": "%s"' % a in l for a in ACT_IDS)]
    first_ta = next((i for i, l in enumerate(lines) if any('"id": "%s"' % a in l for a in TA.ACT_IDS)), None)
    rows = ["    " + r + "," for r in ACTIVITIES_ROWS]
    if first_ta is not None:
        lines[first_ta:first_ta] = rows
    else:
        end = max(i for i, l in enumerate(lines) if l.strip() == "]")
        last = max(i for i in range(end) if lines[i].strip())
        if not lines[last].rstrip().endswith(","):
            lines[last] = lines[last].rstrip() + ","
        rows[-1] = rows[-1][:-1]
        lines[last + 1:end] = rows
    out[ACTIVITIES] = "\n".join(lines)
    json.loads(out[ACTIVITIES])
    who = {"fishermen": [p["key"] for p in fishers], "beekeeper": [p["key"] for p in keeper], "birdwatchers": [p["key"] for p in birders]}
    return out, new, who


def main():
    out, new, who = build()
    stale = [p for p, t in out.items() if open(p, encoding="utf-8").read() != t]
    if "--check" in sys.argv:
        for p in stale:
            print("south_people: %s is not up to date (python3 tools/town/south_people.py)" % os.path.relpath(p, ROOT))
        sys.exit(1 if stale else 0)
    for p in stale:
        with open(p, "w", encoding="utf-8") as f:
            f.write(out[p])
        print("wrote", os.path.relpath(p, ROOT))
    print("the Aton coast's people: %s" % ", ".join("%s %d" % (p["id"], len(p["slots"])) for p in new))
    for k, v in who.items():
        print("  %s: %s" % (k, ", ".join(v)))


if __name__ == "__main__":
    main()
