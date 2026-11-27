"""Random dressing conditions and game moments, judged by the Python of the Blender side (blender/city/citykit.py
dress_visible), written for dresstest.cpp to judge again with the game's C++ (Private/City/CityDressModel.h).

    python3 tools/citydress/gen_cases.py OUT [N]
"""
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "blender", "city"))
import citykit  # noqa: E402

BITS = {w: 1 << i for i, w in enumerate(citykit.WEATHER_WORDS)}
FESTS = ["new_year", "sea_festival", "maslenitsa", "perseids"]


def rand_when(r):
    w = {}
    if r.random() < 0.35:
        w["weather"] = r.sample(citykit.WEATHER_WORDS, r.randint(1, 3))
    if r.random() < 0.2:
        w["weather_not"] = r.sample(citykit.WEATHER_WORDS, r.randint(1, 2))
    if r.random() < 0.35:
        w["months"] = r.sample(range(1, 13), r.randint(1, 6))
    if r.random() < 0.3:
        w["weekdays"] = r.sample(range(7), r.randint(1, 5))
    if r.random() < 0.45:
        w["hours"] = (round(r.uniform(0, 24) * 2) / 2, round(r.uniform(0, 24) * 2) / 2)
    if r.random() < 0.2:
        w["festival"] = r.choice(["any", "none"] + FESTS)
    if r.random() < 0.3:
        w["chance"] = round(r.uniform(0.05, 1.0), 3)
    if r.random() < 0.35:
        per = r.randint(1, 30)
        a = r.randint(0, per - 1)
        w["phase"] = (per, a, r.randint(a + 1, per))
    if r.random() < 0.2:
        w["open"] = r.random() < 0.5
    if r.random() < 0.25:
        w["visit"] = r.sample(["eat_home", "coffee", "read_library", "dominoes", "book_club"], r.randint(1, 2))
        w["keep"] = round(r.uniform(0.25, 12.0) * 4) / 4
    return citykit.dress_when(w)


def main():
    out = sys.argv[1]
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 20000
    r = random.Random(25)
    with open(out, "w") as f:
        for k in range(n):
            w = rand_when(r)
            fests = set(r.sample(FESTS, r.randint(0, 2))) if r.random() < 0.3 else set()
            now = {"since": r.randint(0, 900), "hour": round(r.uniform(0, 24), 2) + 0.003, "weekday": r.randint(0, 6),
                   "month": r.randint(1, 12), "weather": set(x for x in citykit.WEATHER_WORDS if r.random() < 0.3),
                   "festivals": fests, "festival_today": bool(fests) and r.random() < 0.8, "open": r.random() < 0.5}
            now["day"] = 20380 + now["since"]
            plot, did = r.choice(["P-CAFE", "P-SANATORIUM", "W1-U07"]), "set_%d" % r.randint(0, 40)
            ago = -1.0 if r.random() < 0.3 else round(r.uniform(0.0, 14.0), 2) + 0.003    # hours since the visit, -1 none
            if ago >= 0:
                now["visit_ago"] = {did: ago}
            exp = citykit.dress_visible(w, plot, did, now)
            h = w.get("hours", [0.0, 24.0])
            ph = w.get("phase", [0, 0, 0])
            when = [sum(BITS[x] for x in w.get("weather", [])), sum(BITS[x] for x in w.get("weather_not", [])),
                    sum(1 << (m - 1) for m in w.get("months", [])), sum(1 << d for d in w.get("weekdays", [])),
                    h[0], h[1], w.get("festival", "-") or "-", w.get("chance", 1.0), ph[0], ph[1], ph[2],
                    -1 if "open" not in w else int(w["open"]), w.get("keep", 0.0) if "visit" in w else 0.0]
            mom = [now["day"], now["hour"], now["weekday"], now["month"] * 100 + 15, now["since"], int(now["festival_today"]),
                   sum(BITS[x] for x in now["weather"]), int(now["open"]), ago, len(fests)] + sorted(fests)
            f.write(" ".join(str(v) for v in when) + " | " + " ".join(str(v) for v in mom) + " | %s %s %d\n" % (plot, did, exp))
    print("CITYDRESS %d cases -> %s" % (n, out))


if __name__ == "__main__":
    main()
