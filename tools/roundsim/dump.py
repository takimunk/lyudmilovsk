"""Content/City/rounds.json as plain lines for roundtest.cpp (no JSON parser there):
  bundle x y z | window from until deadline spare | route id terrace | stop id terrace x y z person | post ... | postman x y z"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
d = json.load(open(os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "City", "rounds.json"), encoding="utf-8"))
p = d["paper"]
print("bundle %.1f %.1f %.1f" % tuple(p["bundle"]))
print("paperwin %.2f %.2f %.2f %d" % (p["from"], p["until"], p["deadline"], p["spare"]))
for r in p["routes"]:
    print("route %s %s" % (r["id"], r["terrace"][1:]))
    for s in r["stops"]:
        print("stop %s %s %.1f %.1f %.1f -" % (s["id"], s["terrace"][1:], s["at"][0], s["at"][1], s["at"][2]))
q = d["post"]
print("postwin %.2f %.2f %.2f %d" % (q["from"], q["until"], q["deadline"], q["parcels"]))
print("postman %.1f %.1f %.1f" % tuple(q["door"]["at"]))
for s in q["stops"]:
    print("post %s %s %.1f %.1f %.1f %s" % (s["id"], s["terrace"][1:], s["at"][0], s["at"][1], s["at"][2], s["person"]))
