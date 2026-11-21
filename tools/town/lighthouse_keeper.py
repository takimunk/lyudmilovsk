#!/usr/bin/env python3
"""The Mayak lighthouse's keeper (docs/lighthouse.md "The keeper", backlog item 42): Gordey Vetrov, a named person of
people.json (he talks, his quest is lighthouse_paraffin), who lives in the keeper's annex by the tower on the headland beyond
the Mayak station. His day: puts the lamp out just after sunrise, the weather readings at 08, 14 and 20 at the weather screen
in the yard, polishes the lens in the morning, lights the lamp at dusk and keeps the evening watch in the lantern; between
them the bench by the yard's gate, his own stove, now and then a trip to the shop. The lamp's and the watch's hours follow
the sun (duty "sun": "set" | "rise": PeopleSim's SyncSunDuties), so he lights it at dusk in June as in December.

Like tools/town/south_people.py this adds to the generated town and is idempotent (run it after town_life.py and after
tools/south.py moved the lighthouse, its weather screen or its bench):
    python3 tools/town/lighthouse_keeper.py            add or refresh
    python3 tools/town/lighthouse_keeper.py --check    exit 1 when the files are not as this script makes them

Reads Content/South/south.json (the lighthouse, the meteo screen, the bench by the yard) and the meshes' manifests (the
tower's door_stand and annex_door, the screen's read_at). Writes:
  Content/PeopleData/town_places.json  PLACE_IDS (just before south_people's, town_night's and town_activities' places)
  Content/PeopleData/activities.json   ACTIVITIES_ROWS (likewise before theirs)
  Content/PeopleData/people.json       the keeper's line (after the last person)
The keeper's name and lines are in Content/Localization/lighthouse (tools/lighthouse/lighthouse.py builds them).
"""
import collections
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, ".."))
import south_people as SP        # noqa: E402  (the coast's frames; its places and rows stay after ours)
import terrain as T              # noqa: E402
import town_activities as TA     # noqa: E402
import town_night as TN          # noqa: E402

ROOT = SP.ROOT
PLACES = SP.PLACES
ACTIVITIES = SP.ACTIVITIES
PEOPLE = os.path.join(SP.CONTENT, "PeopleData", "people.json")
OD = collections.OrderedDict

KEY = "vetrov_gordey"
BODY = "Vetrov_gordey"
PLACE_IDS = ["S-MAYAK-HOME", "S-MAYAK-TOWER", "S-MAYAK-METEO", "S-MAYAK-BENCH"]
# the activities, one a line; each duty has its own (the life finds a person's duty by its activity: FWorld::DutyFor)
_TOWER = '"places": ["mayak_tower"], "slots": ["staff"], "pose": "hidden", "minutes": [60, 60], "free": 0, "staff": 1'
_SCREEN = '"places": ["mayak_meteo"], "slots": ["staff"], "pose": "look", "minutes": [10, 10], "free": 0, "staff": 1'
ACTIVITIES_ROWS = [
    '{"id": "lamp_out", %s, "meets": {"mind": 0.2}}' % _TOWER,
    '{"id": "lens_polish", %s, "meets": {"mind": 0.35, "fun": 0.1}}' % _TOWER,
    '{"id": "lamp_light", %s, "meets": {"mind": 0.3}}' % _TOWER,
    '{"id": "lamp_watch", %s, "meets": {"mind": 0.4, "fun": 0.15}}' % _TOWER,
    '{"id": "weather_obs_8", %s, "meets": {"nature": 0.3, "mind": 0.2}}' % _SCREEN,
    '{"id": "weather_obs_14", %s, "meets": {"nature": 0.3, "mind": 0.2}}' % _SCREEN,
    '{"id": "weather_obs_20", %s, "meets": {"nature": 0.3, "mind": 0.2}}' % _SCREEN,
    '{"id": "sit_mayak", "places": ["mayak_bench"], "slots": ["bench"], "pose": "sit", "minutes": [20, 50], "hours": [7, 21.5], '
    '"temp": [-8, 40], "meets": {"nature": 0.7, "sun": 0.5, "tired": 0.1}, "base": -3.0, "trait": "nature"}',
]
ACT_IDS = [json.loads(r)["id"] for r in ACTIVITIES_ROWS]
EVERY_DAY = [0, 1, 2, 3, 4, 5, 6]
# the keeper's day (hours, or offsets from sunrise/sunset for "sun"); the order matters where two overlap (FWorld::DutyNow
# takes the first): in June the lighting (21:25..22:25) runs into the watch, which then carries on
DUTIES = [
    ("lamp_out", "rise", [0.0, 0.5], "S-MAYAK-TOWER"),
    ("weather_obs_8", None, [8.0, 8.2], "S-MAYAK-METEO"),
    ("lens_polish", None, [9.0, 10.75], "S-MAYAK-TOWER"),
    ("weather_obs_14", None, [14.0, 14.2], "S-MAYAK-METEO"),
    ("lamp_light", "set", [-0.5, 0.5], "S-MAYAK-TOWER"),
    ("weather_obs_20", None, [20.0, 20.2], "S-MAYAK-METEO"),
    ("lamp_watch", None, [21.75, 23.25], "S-MAYAK-TOWER"),
]
# the ranger's manner of likes: a man of the headland who keeps away from the town's pastimes (people.json "likes")
LIKES = OD([("eat_out", -2.0), ("coffee", -1.0), ("eat_diner", -1.5), ("eat_club", -1.5), ("snack_club", -1.5), ("shop_town", -1.5),
            ("stroll", -1.0), ("swim_look", -1.0), ("culture_evening", -1.5), ("fair_visit", -1.0), ("sport", -1.5), ("night_walk", -1.0),
            ("island_walk", -1.0), ("read_library", -0.5), ("chat", -0.4), ("town", -1.0), ("play_club", -1.5), ("walk", -0.5),
            ("sit_bench", -0.8), ("stove_tea", 0.6), ("sit_mayak", 5.5), ("fish_shore", 0.6), ("supplies_town", 5.0)])


def tower(c):
    t = c.things("lighthouse")
    if not t:
        sys.exit("lighthouse_keeper: no lighthouse in south.json (python3 tools/south.py)")
    return t[0]


def point(c, t, key):
    """A manifest point {"x","y","z","yaw"} of a thing: map metres and the Unreal yaw it faces."""
    r = c.row(t)
    p = r.get(key)
    if not p:
        sys.exit("lighthouse_keeper: the %s manifest has no %s (python3 blender/south/build_south.py)" % (t["kind"], key))
    x, y, z = c.local(t, p["x"], p["y"], p.get("z", 0.0))
    return x, y, z, (float(t.get("yaw", 0.0)) - float(p.get("yaw", 0.0))) % 360.0


def ahead(x, y, yaw, d):
    a = math.radians(yaw)
    return x + d * math.cos(a), y + d * math.sin(a)


def home(c, t):
    """The keeper's annex: in at its door (a shell flat: he goes in and is not seen, as the town's flats), private."""
    x, y, z, yaw = point(c, t, "annex_door")
    ox, oy = ahead(x, y, yaw, 0.6)                       # the step in front of the door
    return OD([("id", "S-MAYAK-HOME"), ("kind", "flat"), ("door", c.at(ox, oy, T.ground_z(ox, oy))), ("yaw", round(yaw, 1)),
               ("island", 0), ("shell", 1), ("private", 1), ("building", "S-MAYAK"),
               ("slots", [OD([("kind", "bed"), ("tag", "bedroom")]), OD([("kind", "armchair"), ("tag", "living")]),
                          OD([("kind", "desk_seat"), ("tag", "living")]), OD([("kind", "kitchen_seat"), ("tag", "kitchen")]),
                          OD([("kind", "stove"), ("tag", "kitchen")])])])


def tower_place(c, t):
    """The tower: in at the door off the steps, his place on the stair inside (hidden: lighting, polishing, the watch)."""
    x, y, z, yaw = point(c, t, "door_stand")
    gz = T.ground_z(x, y) if z is None else z
    axis = c.local(t, 0.0, 0.0, 0.3)
    return OD([("id", "S-MAYAK-TOWER"), ("kind", "mayak_tower"), ("door", c.at(x, y, gz)), ("yaw", round((yaw + 180.0) % 360.0, 1)),
               ("island", 0), ("roles", []), ("path", [c.at(*axis)]),
               ("slots", [OD([("kind", "staff"), ("tag", "keeper"), ("pos", c.at(*axis)), ("yaw", round(yaw, 1)), ("height", 0.0)])])])


def meteo(c):
    s = c.things("meteo_screen")
    if not s:
        sys.exit("lighthouse_keeper: no meteo_screen in south.json (python3 tools/south.py)")
    x, y, z, yaw = point(c, s[0], "read_at")
    return OD([("id", "S-MAYAK-METEO"), ("kind", "mayak_meteo"), ("pos", c.at(x, y, z)), ("yaw", round(yaw, 1)), ("open", 1),
               ("slots", [OD([("kind", "staff"), ("tag", "keeper"), ("pos", c.at(x, y, z)), ("yaw", round(yaw, 1))])])])


def bench(c, t):
    """The park bench nearest the tower (by the yard's gate)."""
    b = min(c.things("park_bench"), key=lambda q: math.hypot(q["x"] - t["x"], q["y"] - t["y"]))
    if math.hypot(b["x"] - t["x"], b["y"] - t["y"]) > 25.0:
        sys.exit("lighthouse_keeper: no park bench within 25 m of the lighthouse")
    return SP.place("S-MAYAK-BENCH", "mayak_bench", c, (b["x"], b["y"], c.ground(b)), c.seats(b, "bench"))


def person_line():
    p = OD([("key", KEY), ("body", BODY), ("mesh", "Town/R_" + KEY), ("age", 67), ("occupation", "lighthouse_keeper"),
            ("household", "vetrov"), ("home", "S-MAYAK-HOME"),
            ("temper", OD([("social", 0.3), ("active", 0.6), ("studious", 0.7), ("gamer", 0.1), ("nature", 0.95), ("sun", 0.9)])),
            ("wake", 6.25), ("bed", 23.5), ("walk_speed", 1.1),
            ("duties", [OD([("activity", a)] + ([("sun", s)] if s else []) + [("hours", h), ("place", pl), ("role", "keeper"),
                                                                               ("days", EVERY_DAY)]) for a, s, h, pl in DUTIES]),
            ("friends", ["lange_pyotr"]), ("likes", LIKES)])
    return "    " + json.dumps(p, ensure_ascii=False)


def build():
    out = {}
    c = SP.Coast()
    t = tower(c)
    new = [home(c, t), tower_place(c, t), meteo(c), bench(c, t)]
    assert [p["id"] for p in new] == PLACE_IDS
    pd = SP.load(PLACES)
    places = [p for p in pd["places"] if p["id"] not in PLACE_IDS]
    # before south_people's, town_night's and town_activities' (the order of the add-ons: lighthouse_keeper, south_people,
    # town_night, town_activities; each keeps the later ones after its own, so any of them may run again)
    after = set(SP.PLACE_IDS) | set(TN.PLACE_IDS) | set(TA.PLACE_IDS)
    at = next((i for i, p in enumerate(places) if p["id"] in after), len(places))
    pd["places"] = places[:at] + new + places[at:]
    out[PLACES] = SP.dump(pd)

    text = open(ACTIVITIES, encoding="utf-8").read()
    lines = [l for l in text.split("\n") if not any('"id": "%s"' % a in l for a in ACT_IDS)]
    later = SP.ACT_IDS + TN.ACT_IDS + TA.ACT_IDS
    first = next((i for i, l in enumerate(lines) if any('"id": "%s"' % a in l for a in later)), None)
    rows = ["    " + r + "," for r in ACTIVITIES_ROWS]
    if first is None:
        sys.exit("lighthouse_keeper: run south_people.py first (its rows are the anchor)")
    lines[first:first] = rows
    out[ACTIVITIES] = "\n".join(lines)
    json.loads(out[ACTIVITIES])

    text = open(PEOPLE, encoding="utf-8").read()
    lines = [l for l in text.split("\n") if '"key": "%s"' % KEY not in l]
    end = max(i for i, l in enumerate(lines) if l.strip() == "]")
    last = max(i for i in range(end) if lines[i].strip())
    if not lines[last].rstrip().endswith(","):
        lines[last] = lines[last].rstrip() + ","
    lines[last + 1:end] = [person_line()]
    out[PEOPLE] = "\n".join(lines)
    json.loads(out[PEOPLE])
    return out


def main():
    out = build()
    stale = [p for p, t in out.items() if open(p, encoding="utf-8").read() != t]
    if "--check" in sys.argv:
        for p in stale:
            print("lighthouse_keeper: %s is not up to date (python3 tools/town/lighthouse_keeper.py)" % os.path.relpath(p, ROOT))
        sys.exit(1 if stale else 0)
    for p in stale:
        with open(p, "w", encoding="utf-8") as f:
            f.write(out[p])
        print("wrote", os.path.relpath(p, ROOT))
    print("lighthouse keeper: %s, %d places, %d duties" % (KEY, len(PLACE_IDS), len(DUTIES)))


if __name__ == "__main__":
    main()
