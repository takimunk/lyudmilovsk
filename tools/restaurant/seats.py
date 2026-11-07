"""The restaurant's seats for the town life (docs/restaurant.md "Townspeople"): Content/Restaurant/layout.json (the building's frame)
put on the city plan's plot -> Content/Restaurant/seats.json, in the town plan's workplaces[].seats form: {"pos": [x, y, z] (Unreal
metres: the floor under the seat's centre; "front": under its front edge), "yaw": deg (the way the sitter faces), "height": m, "hero": true for the tables kept
free for the player, "table": id, "place": [x, y, z] (the plate's spot)}. Also the staff's spots (waiter_home, pass, cook).

  python3 tools/restaurant/seats.py
"""
import json
import math
import os

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")


def frame():
    cfg = json.load(open(os.path.join(CONTENT, "Restaurant", "restaurant.json")))
    city = json.load(open(os.path.join(CONTENT, "City", "city_plan.json")))
    p = next(p for p in city["plots"] if p["id"] == cfg.get("plot", "P-RESTAURANT"))
    ux, uy, uz = p["ue"][0] / 100.0, p["ue"][1] / 100.0, p["ue"][2] / 100.0 + cfg.get("podium", 0.6)
    yaw = p["yaw"]
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))

    def to_ue(q):
        # Blender (x, y, z) -> the mesh's (x, -y, z), turned by the plot's yaw, on the floor
        x, y = q[0], -q[1]
        return [round(ux + x * c - y * s, 3), round(uy + x * s + y * c, 3), round(uz + (q[2] if len(q) > 2 else 0.0), 3)]

    def yaw_ue(local):
        return round(yaw - local, 2)
    return to_ue, yaw_ue


def main():
    L = json.load(open(os.path.join(CONTENT, "Restaurant", "layout.json")))
    to_ue, yaw_ue = frame()
    seats = []
    for t in L["tables"]:
        for s in t["seats"]:
            # the seat's centre for the town life (layout's "at" is the floor under its front edge, 0.20 m toward the table)
            a = math.radians(s["yaw"])
            centre = [s["at"][0] - 0.2 * math.cos(a), s["at"][1] - 0.2 * math.sin(a), s["at"][2]]
            seats.append({"pos": to_ue(centre), "front": to_ue(s["at"]), "yaw": yaw_ue(s["yaw"]), "height": s.get("height", 0.46),
                          "hero": bool(t.get("hero")), "table": t["id"], "place": to_ue(s["place"])})
    out = {"_doc": __doc__.strip().splitlines()[0], "place_id": "restaurant", "seats": seats,
           "staff": {"waiter": {"pos": to_ue(L["waiter_home"]["at"]), "yaw": yaw_ue(L["waiter_home"]["yaw"])},
                     "pass": {"pos": to_ue(L["pass"]["waiter"]), "yaw": yaw_ue(L["pass"]["yaw"])},
                     "cook": {"pos": to_ue(L["cook"]["at"]), "yaw": yaw_ue(L["cook"]["yaw"])}},
           "entrance": {k: to_ue([v[0], v[1], v[2] if v[2] is not None else -0.6]) for k, v in L["entrance"].items()}}
    path = os.path.join(CONTENT, "Restaurant", "seats.json")
    with open(path, "w") as f:
        json.dump(out, f, indent=1)
    print("seats: %d (%d for the hero) -> %s" % (len(seats), sum(s["hero"] for s in seats), os.path.relpath(path, ROOT)))


if __name__ == "__main__":
    main()
