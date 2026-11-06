#!/usr/bin/env python3
"""The town's life (docs/town_life.md): its places and the people who live in them, from the town plan.

Reads
  unreal/Lyudmilovsk/Content/City/city_plan.json          the plan: plots, spots, stairs, terraces, wall dwellings
  blender/export/city/<building type>/flats.json          the buildings' flats (the buildings thread), when there
  unreal/Lyudmilovsk/Content/PeopleData/town_plan.json    hand / other threads' extras: seat points, paths, seats
  tools/town/names.json                                   international given names and surnames (ru/en/ja)
  unreal/Lyudmilovsk/Content/PeopleData/people.json       the story's people: their models are the townsfolk's pool
Writes
  unreal/Lyudmilovsk/Content/PeopleData/town_places.json  places for the people's simulation (PeopleSim LoadTownPlaces)
  unreal/Lyudmilovsk/Content/PeopleData/townsfolk.json    the townsfolk (PeopleSim LoadTownsfolk)
  unreal/Lyudmilovsk/Content/Localization/town/strings_<lang>.json   npc.<key>.name|role (LyudText merges them)

    python3 tools/town/town_life.py [--seed N] [--wall-occupancy 0.55] [--house-occupancy 0.8] [--summary]

Everything is seeded: the same plan and seed give the same town (names, homes, jobs, days).
"""
import argparse
import json
import collections
import math
import os
import random
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
PLAN = os.path.join(CONTENT, "City", "city_plan.json")
EXTRA = os.path.join(CONTENT, "PeopleData", "town_plan.json")
PEOPLE = os.path.join(CONTENT, "PeopleData", "people.json")
NAMES = os.path.join(ROOT, "tools", "town", "names.json")
FLATS_DIR = os.path.join(ROOT, "blender", "export", "city")
OUT_PLACES = os.path.join(CONTENT, "PeopleData", "town_places.json")
OUT_FOLK = os.path.join(CONTENT, "PeopleData", "townsfolk.json")
OUT_STRINGS = os.path.join(CONTENT, "Localization", "town")

# plot kinds whose staff work indoors (a shell: not drawn while inside) or out in the open (drawn at their post)
SHELL_KINDS = {"restaurant", "cafe", "shop", "school", "clinic", "culture", "hotel", "office", "culture_club", "workshop", "pier"}
OPEN_KINDS = {"market", "kiosk", "fair", "lifeguard"}
# a duty skipped on a public holiday (SetDayOff): the offices, the school, the clinic's day hours
HOLIDAY_OFF_KINDS = {"school", "office", "clinic"}
# promenade units have no staff hints in the plan
UNIT_STAFF = {
    "cafe": [["barista", 2, [8, 21], "mon-sun"], ["cook", 1, [10, 20], "mon-sun"]],
    "shop": [["seller", 1, [9, 19], "mon-sat"]],
    "kiosk": [["renter", 1, [9, 20], "mon-sun"]],
    "workshop": [["repairman", 1, [9, 18], "mon-sat"]],
}
# the restaurant thread's place id and the three it named
PLOT_ALIAS = {"P-RESTAURANT": "restaurant"}
PINNED = {
    ("restaurant", "waiter", 0): ("ilya", "sokolov", "m"),
    ("restaurant", "waiter", 1): ("ruta", "petrauskaite", "f"),
    ("restaurant", "cook", 0): ("rashid", "mamedov", "m"),
}

DAYS = {"mon": 0, "tue": 1, "wed": 2, "thu": 3, "fri": 4, "sat": 5, "sun": 6}

# occupations (npc.<key>.role): ru, en, ja
ROLES = {
    "waiter": ("официант", "waiter", "ウェイター"),
    "waitress": ("официантка", "waitress", "ウェイトレス"),
    "cook": ("повар", "cook", "料理人"),
    "manager": ("управляющий", "manager", "支配人"),
    "manager_f": ("управляющая", "manager", "支配人"),
    "barista": ("бариста", "barista", "バリスタ"),
    "seller": ("продавец", "shop assistant", "店員"),
    "seller_f": ("продавщица", "shop assistant", "店員"),
    "cashier": ("кассир", "cashier", "レジ係"),
    "butcher": ("мясник", "butcher", "肉屋"),
    "fishmonger": ("торговец рыбой", "fishmonger", "魚屋"),
    "fishmonger_f": ("торговка рыбой", "fishmonger", "魚屋"),
    "vendor": ("торговец на ярмарке", "fair vendor", "屋台の売り子"),
    "vendor_f": ("торговка на ярмарке", "fair vendor", "屋台の売り子"),
    "carousel_operator": ("смотритель карусели", "carousel operator", "回転木馬の係員"),
    "projectionist": ("киномеханик", "projectionist", "映写技師"),
    "dance_teacher": ("учитель танцев", "dance teacher", "ダンスの先生"),
    "clerk": ("служащий", "office clerk", "事務員"),
    "clerk_f": ("служащая", "office clerk", "事務員"),
    "teacher": ("учитель", "teacher", "教師"),
    "teacher_f": ("учительница", "teacher", "教師"),
    "director": ("директор школы", "headteacher", "校長"),
    "janitor": ("завхоз", "caretaker", "用務員"),
    "doctor": ("врач", "doctor", "医師"),
    "nurse": ("медбрат", "nurse", "看護師"),
    "nurse_f": ("медсестра", "nurse", "看護師"),
    "coach": ("тренер", "coach", "コーチ"),
    "lifeguard": ("спасатель", "lifeguard", "ライフガード"),
    "receptionist": ("администратор", "receptionist", "受付係"),
    "cleaner": ("уборщик", "cleaner", "清掃員"),
    "cleaner_f": ("уборщица", "cleaner", "清掃員"),
    "guard": ("охранник", "guard", "警備員"),
    "lighthouse_keeper": ("смотритель маяка", "lighthouse keeper", "灯台守"),
    "renter": ("прокатчик", "rentals attendant", "レンタル係"),
    "repairman": ("мастер", "repairman", "修理屋"),
    "away": ("работает в городе", "works out of town", "町の外で働く"),
    "student": ("студент", "student", "大学生"),
    "student_f": ("студентка", "student", "大学生"),
    "pupil": ("школьник", "schoolboy", "小学生"),
    "pupil_f": ("школьница", "schoolgirl", "小学生"),
    "pensioner": ("пенсионер", "pensioner", "年金生活者"),
    "pensioner_f": ("пенсионерка", "pensioner", "年金生活者"),
    "artist": ("художник", "artist", "画家"),
    "artist_f": ("художница", "artist", "画家"),
    "writer": ("писатель", "writer", "作家"),
    "writer_f": ("писательница", "writer", "作家"),
    "musician": ("музыкант", "musician", "音楽家"),
    "programmer": ("программист", "programmer", "プログラマー"),
    "fisherman": ("рыбак", "fisherman", "漁師"),
    "homemaker": ("домохозяин", "homemaker", "主夫"),
    "homemaker_f": ("домохозяйка", "homemaker", "主婦"),
    "unemployed": ("ищет работу", "between jobs", "求職中"),
}
# a role's feminine form, when it has one
def role_key(role, female):
    return role + "_f" if female and role + "_f" in ROLES else role


def ue_m(plan, p):
    """Map metres (X west, Y north, Z up) -> Unreal metres (the people's frame)."""
    return [round(p[0], 2), round(p[1] + plan["portal_face_y"] / 100.0, 2), round(p[2], 2)]


def from_ue_cm(p):
    return [round(p[0] / 100.0, 2), round(p[1] / 100.0, 2), round(p[2] / 100.0, 2)]


def days_mask(spec):
    if isinstance(spec, list):
        return spec
    a, _, b = spec.partition("-")
    a, b = DAYS[a], DAYS[b or a]
    out, d = [], a
    while True:
        out.append(d)
        if d == b:
            return out
        d = (d + 1) % 7


def offset(p, yaw, fwd, side):
    y = math.radians(yaw)
    return [round(p[0] + math.cos(y) * fwd - math.sin(y) * side, 2), round(p[1] + math.sin(y) * fwd + math.cos(y) * side, 2), p[2]]


# --- places ----------------------------------------------------------------------------------------------------

def spot_slots(spot, pos, extra):
    """Slots of a gathering spot: the public spaces thread's seat points if it gave them, else laid out here."""
    kind, yaw, seats = spot["kind"], spot.get("yaw", 0.0), int(spot.get("seats") or 0)
    if extra and extra.get("slots"):
        return [{"kind": s.get("kind", "bench"), "pos": s["pos"], "yaw": s.get("yaw", yaw), "height": s.get("height", 0.45)} for s in extra["slots"]]
    if extra and extra.get("path"):
        return [{"kind": "stand", "pos": p, "yaw": yaw, "capacity": 2} for p in extra["path"]]
    out = []
    if kind == "bench":
        for i in range(max(1, seats)):
            out.append({"kind": "bench", "pos": offset(pos, yaw, 0.0, (i - (seats - 1) / 2) * 0.62), "yaw": yaw, "height": 0.45})
    elif kind == "fountain":
        r = 9.0 if spot["id"] == "S-FOUNTAIN" else 4.0
        for i in range(max(1, seats)):
            a = 360.0 * i / max(1, seats)
            out.append({"kind": "bench", "pos": offset(pos, a, r + 0.35, 0.0), "yaw": a, "height": 0.45})
    elif kind in ("square", "park", "fair", "beach", "pond", "playground", "stage", "bonfire", "promenade", "pier", "sunrise_point", "sunset_point"):
        n = max(seats, {"promenade": 24, "stage": 12, "square": 30}.get(kind, 6))
        spread = {"square": 34.0, "park": 70.0, "fair": 50.0, "beach": 120.0, "pond": 10.0, "playground": 8.0, "stage": 14.0,
                  "bonfire": 5.0, "promenade": 0.0, "pier": 5.0, "sunrise_point": 6.0, "sunset_point": 9.0}[kind]
        rnd = random.Random(spot["id"])
        for i in range(n):
            if kind == "promenade":
                # along the embankment, south to north
                p = [pos[0] - 4.0 + rnd.uniform(-2.5, 2.5), pos[1] - 520.0 + 1040.0 * (i + 0.5) / n, pos[2]]
            elif kind == "bonfire":
                a = 360.0 * i / n
                p = offset(pos, a, spread, 0.0)
            elif kind in ("beach", "park", "fair"):
                p = offset(pos, yaw, rnd.uniform(-0.25, 0.25) * spread, rnd.uniform(-1, 1) * spread)
            else:
                a, r = rnd.uniform(0, 360), spread * math.sqrt(rnd.random())
                p = offset(pos, a, r, 0.0)
            face = yaw
            if kind == "bonfire":
                face = (360.0 * i / n + 180.0) % 360.0
            out.append({"kind": "stand", "pos": [round(p[0], 2), round(p[1], 2), pos[2]], "yaw": round(face, 1), "capacity": 2})
    return out


GIT_REFS = []
REFS_FIRST = False


def read_input(rel):
    """A file of the repo: from the working tree, else from one of the --ref branches (another thread's, not merged yet)."""
    path = os.path.join(ROOT, rel)
    if os.path.exists(path) and not REFS_FIRST:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    for ref in GIT_REFS:
        try:
            out = subprocess.run(["git", "-C", ROOT, "show", "%s:%s" % (ref, rel)], capture_output=True, check=True).stdout
            return json.loads(out.decode("utf-8"))
        except (subprocess.CalledProcessError, ValueError):
            continue
    if os.path.exists(path):
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    return None


def flats_of_type(btype, plot_id):
    """The buildings thread's flats: a per-plot variant first (<type>__<plot id: lower case, '-' -> '_'>.json), else the type's."""
    return read_input("blender/export/city/%s__%s.json" % (btype, plot_id.lower().replace("-", "_"))) or read_input("blender/export/city/%s.json" % btype)


def local_to_world(front, yaw, local):
    """A building's local point (the buildings thread's frame: x along the front to the right of someone in the street facing
    it, y inland away from the street, z up; metres from the plot's front point) -> Unreal metres."""
    a = math.radians(yaw)
    x, y, z = local
    return [round(front[0] + x * math.sin(a) - y * math.cos(a), 2), round(front[1] - x * math.cos(a) - y * math.sin(a), 2), round(front[2] + z, 2)]


def flat_slots(beds, sleepers_max):
    """A flat's slots for the simulation (all at its door: the flat is a shell)."""
    out = []
    if beds >= 2:
        out.append({"kind": "bed", "capacity": 2, "tag": "bedroom"})
        for _ in range(beds - 2):
            out.append({"kind": "bed", "tag": "bedroom"})
        out.append({"kind": "bed", "tag": "bedroom"})
    else:
        out.append({"kind": "bed", "tag": "bedroom"})
    out += [{"kind": "sofa", "tag": "living"}, {"kind": "armchair", "tag": "living"}, {"kind": "desk_seat", "tag": "living"}]
    out += [{"kind": "kitchen_seat", "tag": "kitchen"} for _ in range(max(2, sleepers_max))]
    return out


def build_places(plan, extra, args):
    places, flats, workplaces = [], [], []
    extra_spots = {s["id"]: s for s in extra.get("spots", [])}
    extra_work = {w["id"]: w for w in extra.get("workplaces", [])}

    # the spots: the public spaces thread's (town_plan.json, Unreal metres, with seats and paths), then the plan's others
    done = set()
    fair_sellers = []
    for s in extra.get("spots", []):
        if s.get("season") == "summer" or s.get("sub") == "customers" and s.get("seller"):
            if s.get("seller"):
                fair_sellers.append(s["seller"])
            if s.get("season") == "summer":
                continue
        pos = s["pos"]
        pl = {"id": s["id"], "kind": s["kind"], "pos": pos, "yaw": s.get("yaw", 0.0), "island": 0, "slots": spot_slots(s, pos, s)}
        places.append(pl)
        done.add(s["id"])
    for s in plan["spots"]:
        if s["id"] in done:
            continue
        pos = from_ue_cm(s["ue"])
        pl = {"id": s["id"], "kind": s["kind"], "pos": pos, "yaw": s.get("yaw", 0.0), "island": 0, "slots": spot_slots(s, pos, None)}
        places.append(pl)
    # workplaces: plots with staff, promenade units
    work = []
    for p in plan["plots"]:
        if p["kind"] in ("residential", "square", "park", "pool"):
            continue
        work.append({"id": PLOT_ALIAS.get(p["id"], p["id"]), "plot": p["id"], "kind": "culture_club" if p["kind"] == "club" else p["kind"],
                     "door": ue_m(plan, p["front"]), "yaw": p["yaw"], "staff": p.get("staff") or [], "w": p["w"], "d": p["d"],
                     "centre": from_ue_cm(p["ue"])})
    for u in plan.get("promenade_units", []):
        work.append({"id": u["id"], "plot": u["id"], "kind": u["kind"], "door": from_ue_cm(u["door_ue"]), "yaw": u["yaw"],
                     "staff": u.get("staff") or UNIT_STAFF.get(u["kind"], []), "w": u.get("w", 10.0), "d": 6.0, "centre": from_ue_cm(u["door_ue"])})
    rest = read_input("unreal/Lyudmilovsk/Content/Restaurant/seats.json")
    if rest and "restaurant" not in extra_work:
        extra_work["restaurant"] = {"seats": rest.get("seats", rest if isinstance(rest, list) else [])}
    plot_type = {p["id"]: p.get("building") for p in plan["plots"]}
    for w in work:
        x = extra_work.get(w["id"], {})
        seats = [s for s in x.get("seats", []) if not s.get("hero")]
        shell = w["kind"] in SHELL_KINDS and not seats
        slots = []
        rnd = random.Random(w["id"])
        # the buildings thread's spots (building frame; a spot's yaw is the way its user faces, 0 = -y toward the street)
        bdata = flats_of_type(plot_type[w["plot"]], w["plot"]) if plot_type.get(w["plot"]) else None
        bspots = collections.defaultdict(list)
        if bdata:
            front = w["door"]
            for sp in bdata.get("spots", []):
                bspots[sp["kind"]].append({"pos": local_to_world(front, w["yaw"], sp["pos"]), "yaw": round((w["yaw"] - sp.get("yaw", 0.0)) % 360, 1)})
            ent = next((e for e in bdata.get("entrances", []) if e.get("kind") == "street"), None)
            if ent and shell:
                w["door"] = local_to_world(front, w["yaw"], ent["pos"])
        if w["kind"] == "kiosk" and bspots.get("counter"):
            # the seller behind the kiosk's counter, a customer at its window, the stop's bench
            c = bspots["counter"][0]
            for role, count, hours, days in w["staff"]:
                slots.append({"kind": "staff", "tag": role, "pos": c["pos"], "yaw": c["yaw"]})
            for sp in bspots.get("stand", []):
                slots.append({"kind": "visit", "pos": sp["pos"], "yaw": sp["yaw"]})
            for sp in bspots.get("bench", []):
                slots.append({"kind": "bench", "pos": sp["pos"], "yaw": sp["yaw"], "capacity": 2, "height": 0.45})
            roles = sorted({r[0] for r in w["staff"]})
            places.append({"id": w["id"], "kind": w["kind"], "door": w["door"], "yaw": w["yaw"], "island": 0, "slots": slots,
                           "roles": roles, "building": w["plot"]})
            workplaces.append(w)
            continue
        for role, count, hours, days in w["staff"]:
            if shell:
                slots.append({"kind": "staff", "tag": role, "capacity": count})
            else:
                for i in range(count):
                    # out in the open: at their post round the stall or the kiosk's window
                    yaw = w["yaw"]
                    if w["kind"] == "fair" and role == "vendor" and fair_sellers:
                        st = fair_sellers[(i * 7) % len(fair_sellers)]      # behind a stall's counter
                        p, yaw = st["pos"], st.get("yaw", yaw)
                    elif w["kind"] in ("market", "fair"):
                        p = offset(w["centre"], w["yaw"], rnd.uniform(-0.3, 0.3) * w["d"], rnd.uniform(-0.4, 0.4) * w["w"])
                    else:
                        p = offset(w["door"], w["yaw"], 0.6, (i - (count - 1) / 2) * 1.2)
                    slots.append({"kind": "staff", "tag": role, "pos": p, "yaw": yaw})
        if w["kind"] in ("restaurant", "cafe"):
            if seats:
                slots += [{"kind": "seat", "pos": s["pos"], "yaw": s.get("yaw", 0.0), "height": s.get("height", 0.45)} for s in seats]
            else:
                n = len(bspots.get("seat", []))
                slots.append({"kind": "seat", "capacity": n or (24 if w["kind"] == "restaurant" else 14)})
        if w["kind"] == "school":
            slots.append({"kind": "staff", "tag": "pupil", "capacity": 600})     # the classes
        if shell:
            # a hall's seats (the cinema's 350) hold its visitors; otherwise a dozen at a time
            n = len(bspots.get("seat", [])) if w["kind"] in ("culture_club", "culture", "clinic") else 0
            slots.append({"kind": "visit", "capacity": max(12, n)})
        else:
            for i in range(6):
                slots.append({"kind": "visit", "pos": offset(w["door"], w["yaw"], 2.0 + rnd.uniform(0, 3), rnd.uniform(-0.5, 0.5) * w["w"]), "yaw": (w["yaw"] + 180) % 360})
        roles = sorted({r[0] for r in w["staff"]})
        pl = {"id": w["id"], "kind": w["kind"], "door": w["door"], "yaw": w["yaw"], "island": 0, "slots": slots, "roles": roles,
              "building": w["plot"]}
        if shell:
            pl["shell"] = 1
        places.append(pl)
        workplaces.append(w)

    # homes: the flats in the terrace walls, the plot buildings' flats
    for d in plan["wall_dwellings"]:
        flats.append({"id": d["id"], "building": d["wall"], "door": from_ue_cm(d["door_ue"]), "yaw": d.get("yaw", 0.0),
                      "beds": d.get("beds", 2), "windows": [w["id"] for w in d.get("windows", [])], "terrace": d.get("terrace"),
                      "occupancy": args.wall_occupancy})
    for p in plan["plots"]:
        # homes are the residential plots' flats, and the flats a public building carries (the bridge house's 36 over
        # its offices, the sanatorium's rooms: long-stay guests, fewer of them)
        if not p.get("building") or (p["kind"] != "residential" and not (flats_of_type(p["building"], p["id"]) or {}).get("flats")):
            continue
        front = ue_m(plan, p["front"])
        data = flats_of_type(p["building"], p["id"])
        occ = args.house_occupancy * (0.4 if p["kind"] == "hotel" else 1.0)
        if data:
            # in by the building's street entrance (the flats themselves aren't walked: the entrance is the flat's door)
            ents = {e["id"]: e for e in data.get("entrances", [])}
            for f in data.get("flats", []):
                fid = "%s/%s" % (p["id"], f["flat"])
                e = ents.get(f.get("entrance")) or next(iter(ents.values()), None)
                door = local_to_world(front, p["yaw"], e["pos"] if e else [0, 0, 0])
                wins = [w if isinstance(w, str) else w.get("w") or w.get("id") for w in f.get("windows", [])]
                flats.append({"id": fid, "building": p["id"], "door": door, "yaw": p["yaw"], "beds": max(1, f.get("beds", 2)),
                              "windows": ["%s/%s" % (p["id"], w) if not w.startswith(p["id"]) else w for w in wins], "terrace": p.get("terrace"),
                              "occupancy": occ})
        else:
            # not designed yet: provisional flats behind the front door, no windows of their own
            storeys = max(2, min(9, int((p.get("height_max") or 12) / 3.2)))
            per = max(2, min(4, int(p["w"] / 14)))
            n = min(24, storeys * per)
            for i in range(n):
                fid = "%s/%d-%02d" % (p["id"], i // per + 1, i % per + 1)
                flats.append({"id": fid, "building": p["id"], "door": front, "yaw": p["yaw"], "beds": 3 if i % 3 == 0 else 2,
                              "windows": [], "terrace": p.get("terrace"), "occupancy": args.house_occupancy, "provisional": 1})
    for f in flats:
        pl = {"id": f["id"], "kind": "flat", "door": f["door"], "yaw": f["yaw"], "island": 0, "shell": 1, "building": f["building"],
              "windows": f["windows"], "slots": flat_slots(f["beds"], f["beds"] + 1)}
        places.append(pl)
    return places, flats, workplaces


# --- people -----------------------------------------------------------------------------------------------------

def load_pool():
    """The models the townsfolk borrow for now (the story people's own; a generator will make theirs later)."""
    with open(PEOPLE, encoding="utf-8") as f:
        people = json.load(f)["people"]
    female_keys = {"streamer", "kid_girl", "girl_a", "girl_b", "crowd_12", "nao"}
    unmade = {"Khuazheva_susanna"}
    pool = []
    for p in people:
        model = p.get("body") or p.get("mesh")
        if not model or model in unmade:
            continue
        pool.append({"model": model, "age": p["age"], "female": bool(p.get("female")) or p["key"] in female_keys})
    return pool


def pick_model(rnd, pool, age, female, used):
    cands = [m for m in pool if m["female"] == female and ((m["age"] < 16) == (age < 16))] or [m for m in pool if m["female"] == female]
    # nearest in age, with variety: the least used of the three nearest
    cands.sort(key=lambda m: abs(m["age"] - age) + rnd.random() * 12)
    best = min(cands[:3], key=lambda m: used.get(m["model"], 0))
    used[best["model"]] = used.get(best["model"], 0) + 1
    return best["model"]


class Names:
    def __init__(self, rnd):
        with open(NAMES, encoding="utf-8") as f:
            d = json.load(f)
        self.given = {"m": [g for g in d["given"] if g["sex"] == "m"], "f": [g for g in d["given"] if g["sex"] == "f"]}
        self.by_id = {g["id"]: g for g in d["given"]}
        self.sur = [s for s in d["surnames"] if not s.get("pinned_only")]
        self.sur_by_id = {s["id"]: s for s in d["surnames"]}
        self.rnd = rnd
        self.taken = set()

    def surname(self):
        return self.rnd.choice(self.sur)

    def given_name(self, sex):
        return self.rnd.choice(self.given[sex])

    def full(self, given, sur, female):
        out = {}
        for lang in ("ru", "en", "ja"):
            s = sur.get("f_" + lang) if female and sur.get("f_" + lang) else sur[lang]
            g = given[lang]
            out[lang] = "%s・%s" % (g, s) if lang == "ja" and given.get("origin") != "jp" else ("%s%s" % (s, g) if lang == "ja" else "%s %s" % (g, s))
        return out

    def key(self, given, sur):
        base = "t_%s_%s" % (sur["id"], given["id"])
        k, n = base, 2
        while k in self.taken:
            k, n = "%s%d" % (base, n), n + 1
        self.taken.add(k)
        return k


def chronotype(rnd, age):
    r = rnd.random()
    lark = 0.18 + (0.35 if age >= 60 else 0.0)
    owl = 0.2 if 16 <= age < 45 else 0.08
    night = 0.07 if 18 <= age < 60 else 0.0
    if age < 16:
        return "usual"
    if r < lark:
        return "lark"
    if r < lark + owl:
        return "owl"
    if r < lark + owl + night:
        return "night"
    return "usual"


def day_hours(rnd, ctype, age):
    if age < 12:
        return rnd.uniform(6.8, 7.3), rnd.uniform(20.5, 21.5), 0.25
    if age < 16:
        return rnd.uniform(6.8, 7.5), rnd.uniform(21.5, 22.8), 0.4
    if ctype == "lark":
        return rnd.uniform(4.4, 6.0), rnd.uniform(20.6, 22.3), rnd.uniform(0.3, 0.7)
    if ctype == "owl":
        return rnd.uniform(9.3, 11.8), rnd.uniform(0.8, 2.8), rnd.uniform(0.5, 1.1)
    if ctype == "night":
        return rnd.uniform(11.5, 13.5), rnd.uniform(3.6, 5.0), rnd.uniform(0.4, 0.9)
    return rnd.uniform(6.4, 8.4), rnd.uniform(22.4, 24.4) % 24.0, rnd.uniform(0.4, 1.0)


def shifts(role, count, hours, days):
    """A job's posts as one person's shifts each: long hours split into an early and a late shift (a day round the clock
    into three), and staff of a seven-day place take two days off each, in turns."""
    h0, h1 = hours
    span = (h1 - h0) % 24 or 24
    pieces = 1 if span <= 10 else (3 if span >= 20 else 2)
    week = days_mask(days)
    out = []
    for i in range(count):
        k = i % pieces
        a = (h0 + span * k / pieces) % 24
        b = (h0 + span * (k + 1) / pieces) % 24
        d = list(week)
        if len(week) >= 6:
            off = {(i * 2) % 7, (i * 2 + 1) % 7}
            d = [x for x in week if x not in off] or week
        out.append({"role": role, "hours": [round(a, 2), round(b, 2)], "days": d})
    # enough hands for every shift: a second person on each extra piece
    while pieces > 1 and len(out) < pieces:
        k = len(out)
        a = (h0 + span * k / pieces) % 24
        b = (h0 + span * (k + 1) / pieces) % 24
        out.append({"role": role, "hours": [round(a, 2), round(b, 2)], "days": week})
    return out


def temper(rnd, age, ctype):
    t = {k: round(min(1.0, max(0.0, rnd.gauss(0.5, 0.22))), 2) for k in ("social", "active", "studious", "gamer", "nature", "sun")}
    if age >= 60:
        t["gamer"] = round(t["gamer"] * 0.3, 2)
        t["active"] = round(t["active"] * 0.7, 2)
    if age < 30:
        t["gamer"] = round(min(1.0, t["gamer"] + 0.2), 2)
    if ctype == "lark":
        t["sun"] = round(min(1.0, t["sun"] + 0.25), 2)
    return t


def likes_for(rnd, age, female, ctype, occupation):
    likes = {}
    if age >= 62:
        likes["sit_bench"] = round(rnd.uniform(0.5, 1.1), 2)       # the grannies and grandads on the benches
        likes["chat"] = round(rnd.uniform(0.1, 0.5), 2)
        likes["play_club"] = -0.8
        likes["night_walk"] = -1.0
        if rnd.random() < 0.3:
            likes["fish_shore"] = round(rnd.uniform(0.6, 1.2), 2)
    if ctype == "lark":
        likes["watch_sunrise"] = round(rnd.uniform(0.4, 1.3), 2)
    if ctype == "night":
        likes["night_walk"] = round(rnd.uniform(0.9, 1.6), 2)
    elif ctype == "owl":
        likes["night_walk"] = round(rnd.uniform(0.0, 0.6), 2)
    if 12 <= age < 35 and rnd.random() < 0.45:
        likes["play_club"] = round(rnd.uniform(0.2, 0.9), 2)       # the computer club
        likes["play_town_club"] = round(rnd.uniform(0.2, 0.9), 2)
    hobbies = ["eat_out", "coffee", "stroll", "sea_air", "fish_shore", "read_library", "fair_visit", "swim_look", "shop_town"]
    for h in rnd.sample(hobbies, 2):
        likes[h] = round(max(likes.get(h, 0.0), rnd.uniform(0.2, 0.8)), 2)
    if occupation in ("artist", "writer"):
        likes["stroll"] = round(rnd.uniform(0.4, 0.9), 2)
    return likes


def household_plan(rnd, beds):
    """Who lives in a flat: (kind, members as (age, female))."""
    r = rnd.random()
    if beds <= 1:
        # a capsule or a bedsit: one person (a student, someone young, now and then an old bachelor)
        age = rnd.choice([rnd.randint(19, 34), rnd.randint(19, 50), rnd.randint(55, 80)])
        return "single", [(age, rnd.random() < 0.5)]
    if beds >= 3 and r < 0.45:
        a = rnd.randint(28, 48)
        kids = [(rnd.randint(4, 17), rnd.random() < 0.5) for _ in range(rnd.choice([1, 1, 2]))]
        return "family", [(a, False), (a + rnd.randint(-5, 5), True)] + kids
    if r < 0.36:
        age = rnd.choice([rnd.randint(19, 40), rnd.randint(19, 60), rnd.randint(62, 88)])
        return "single", [(age, rnd.random() < 0.55 if age > 62 else rnd.random() < 0.5)]
    if r < 0.66:
        a = rnd.randint(22, 60)
        return "couple", [(a, False), (min(90, max(19, a + rnd.randint(-6, 6))), True)]
    if r < 0.82:
        a = rnd.randint(63, 86)
        if rnd.random() < 0.6:
            return "elders", [(a, False), (min(90, a + rnd.randint(-5, 4)), True)]
        return "single", [(a, True)]                                     # a granny on her own
    if beds >= 2 and r < 0.92:
        a = rnd.randint(26, 46)
        return "parent", [(a, rnd.random() < 0.7), (rnd.randint(6, 16), rnd.random() < 0.5)]
    a = rnd.randint(19, 27)
    return "flatmates", [(a, rnd.random() < 0.5), (a + rnd.randint(-2, 3), rnd.random() < 0.5)]


def build_people(plan, places, flats, workplaces, args, rnd):
    names = Names(rnd)
    pool = load_pool()
    used = {}
    people = []
    place_ids = {p["id"] for p in places}
    has_school = "P-SCHOOL" in place_ids
    # the town's posts: (workplace, role, shift)
    posts = []
    for w in workplaces:
        for role, count, hours, days in w["staff"]:
            for i, s in enumerate(shifts(role, count, hours, days)):
                posts.append({"place": w["id"], "kind": w["kind"], "role": role, "hours": s["hours"], "days": s["days"], "index": i})
    rnd.shuffle(posts)
    # the pinned ones first in the list (the restaurant's three)
    posts.sort(key=lambda p: (p["place"], p["role"], p["index"]) not in PINNED)

    households = []
    for f in flats:
        if rnd.random() >= f["occupancy"]:
            continue
        kind, members = household_plan(rnd, f["beds"])
        households.append((f, kind, members))
    rnd.shuffle(households)
    # no more than the town can carry (the simulation is cheap; the bodies near the camera are not): empty flats stay dark
    total = 0
    for i, h in enumerate(households):
        total += len(h[2])
        if total > args.max_people:
            households = households[:i]
            break

    def new_person(age, female, sur, given=None, house=None, flat=None):
        sex = "f" if female else "m"
        g = given or names.given_name(sex)
        key = names.key(g, sur)
        ctype = chronotype(rnd, age)
        wake, bed, spread = day_hours(rnd, ctype, age)
        p = {"key": key, "town": 1, "mesh": None, "age": age, "female": 1 if female else 0, "household": house, "home": flat,
             "chronotype": ctype, "wake": round(wake, 2), "bed": round(bed, 2), "spread": round(spread, 2),
             "duties": [], "friends": [], "_name": names.full(g, sur, female), "_given": g["id"], "_sur": sur["id"]}
        p["temper"] = temper(rnd, age, ctype)
        p["walk_speed"] = round(1.35 if age < 16 else (rnd.uniform(0.85, 1.05) if age >= 75 else (rnd.uniform(1.0, 1.15) if age >= 62 else rnd.uniform(1.15, 1.4))), 2)
        return p

    # pinned staff get their households first (singles in the first free flats)
    hh_i = 0
    for (place, role, index), (g, s, sex) in PINNED.items():
        post = next((p for p in posts if (p["place"], p["role"], p["index"]) == (place, role, index) and not p.get("taken")), None)
        if not post or hh_i >= len(households):
            continue
        f, _, _ = households[hh_i]
        households[hh_i] = (f, "single", [(rnd.randint(24, 45), sex == "f")])
        households[hh_i] = households[hh_i] + ((names.by_id.get(g), names.sur_by_id.get(s), post),)
        hh_i += 1

    # the pinned people's full names are theirs alone
    reserved = {"t_%s_%s" % (h[3][1]["id"], h[3][0]["id"]) for h in households if len(h) > 3 and h[3][0] and h[3][1]}
    for h in households:
        f, kind, members = h[0], h[1], h[2]
        pinned = h[3] if len(h) > 3 else None
        house = "t_" + f["id"].replace("/", "_").lower()
        family_sur = pinned[1] if pinned and pinned[1] else names.surname()
        for mi, (age, female) in enumerate(members):
            # an international couple keeps their own surnames now and then; flatmates always
            sur = family_sur if (kind in ("family", "parent", "single") or (kind in ("couple", "elders") and rnd.random() < 0.7) or (mi > 0 and age < 18)) else names.surname()
            if kind == "flatmates":
                sur = names.surname() if mi else family_sur
            given = pinned[0] if pinned and pinned[0] and mi == 0 else None
            # no two of a household with one given name
            for _ in range(40):
                if given is None:
                    cand = names.given_name("f" if female else "m")
                    full = "t_%s_%s" % (sur["id"], cand["id"])
                    if full not in names.taken and full not in reserved and all(q["_given"] != cand["id"] for q in people if q["household"] == house):
                        given = cand
            p = new_person(age, female, sur, given, house, f["id"])
            p["_flat"] = f
            p["_kind"] = kind
            if pinned and mi == 0:
                p["_post"] = pinned[2]
                pinned[2]["taken"] = True
            people.append(p)

    # jobs: the town's posts to the working-age adults, then the rest
    adults = [p for p in people if 19 <= p["age"] <= 66 and "_post" not in p]
    rnd.shuffle(adults)
    ai = 0
    for post in posts:
        if post.get("taken"):
            continue
        # a night shift wants an owl, a dawn shift a lark (the fishmonger at 6, the receptionist at night)
        best = None
        for j in range(ai, min(ai + 8, len(adults))):
            c = adults[j]
            want = "lark" if post["hours"][0] < 7.5 else ("owl" if post["hours"][1] < post["hours"][0] or post["hours"][0] >= 16 else None)
            if want and c["chronotype"] == want:
                best = j
                break
        if best is None and ai < len(adults):
            best = ai
        if best is None:
            break
        adults[ai], adults[best] = adults[best], adults[ai]
        adults[ai]["_post"] = post
        post["taken"] = True
        ai += 1
    for p in people:
        post = p.get("_post")
        female = bool(p["female"])
        if post:
            act = "work_office" if post["kind"] in HOLIDAY_OFF_KINDS else "work_town"
            p["duties"].append({"activity": act, "place": post["place"], "role": post["role"], "hours": post["hours"], "days": post["days"]})
            p["occupation"] = post["role"]
            p["_work"] = post["place"]
            # their day fits their shift: up an hour before, to bed a while after
            h0, h1 = post["hours"]
            if h1 > h0 or h1 == 0:
                if not (h1 < h0):
                    p["wake"] = round(min(p["wake"], h0 - rnd.uniform(0.6, 1.4)) % 24, 2)
                    if (p["bed"] - h1) % 24 > 12:       # bed before the shift ends
                        p["bed"] = round((h1 + rnd.uniform(1.5, 4.0)) % 24, 2)
            else:
                # a night shift: sleep in the morning
                p["wake"] = round((h0 - rnd.uniform(4.0, 6.0)) % 24, 2)
                p["bed"] = round((h1 + rnd.uniform(0.8, 1.6)) % 24, 2)
                p["chronotype"] = "night"
        elif p["age"] < 18:
            if p["age"] >= 7:
                end = 13.5 if p["age"] < 12 else 14.5
                if has_school:
                    p["duties"].append({"activity": "school_town", "place": "P-SCHOOL", "role": "pupil", "hours": [8.25, end], "days": [0, 1, 2, 3, 4]})
                else:
                    p["duties"].append({"activity": "school", "hours": [8.25, end], "days": [0, 1, 2, 3, 4]})
                p["occupation"] = "pupil"
            else:
                p["occupation"] = "pupil"
                p["age"] = 7                    # no toddlers yet (no model for one): the youngest are first-formers
                p["duties"].append({"activity": "school_town" if has_school else "school", "place": "P-SCHOOL" if has_school else "",
                                    "role": "pupil", "hours": [8.25, 12.5], "days": [0, 1, 2, 3, 4]})
        elif p["age"] >= 63:
            p["occupation"] = "pensioner"
        elif p["age"] <= 24 and rnd.random() < 0.6:
            p["occupation"] = "student"
            p["duties"].append({"activity": "work_away", "hours": [round(rnd.choice([9.0, 9.5, 10.0]), 2), round(rnd.uniform(13.5, 16.0), 2)], "days": [0, 1, 2, 3, 4]})
        else:
            r = rnd.random()
            if r < 0.45:
                p["occupation"] = "away"
                h0 = round(rnd.choice([7.0, 7.5, 8.0, 8.5, 9.0]), 2)
                p["duties"].append({"activity": "work_away", "hours": [h0, round(h0 + rnd.uniform(7.5, 9.0), 2)], "days": [0, 1, 2, 3, 4]})
                p["wake"] = round(min(p["wake"], h0 - rnd.uniform(0.7, 1.2)), 2)
            elif r < 0.6:
                p["occupation"] = "fisherman"
                p["duties"].append({"activity": "work_sea", "hours": [round(rnd.uniform(4.0, 5.5), 2), round(rnd.uniform(10.5, 12.5), 2)], "days": rnd.sample(range(7), 5)})
                p["wake"] = round(p["duties"][-1]["hours"][0] - 0.6, 2)
                p["bed"] = round(rnd.uniform(20.5, 22.0), 2)
                p["chronotype"] = "lark"
            else:
                p["occupation"] = rnd.choice(["artist", "writer", "musician", "programmer", "homemaker", "unemployed"])
        p["likes"] = likes_for(rnd, p["age"], female, p["chronotype"], p["occupation"])
        p["mesh"] = pick_model(rnd, pool, p["age"], female, used)

    # friends: neighbours of the same building, colleagues, people of an age
    by_building, by_work = {}, {}
    for p in people:
        by_building.setdefault(p["_flat"]["building"], []).append(p)
        if p.get("_work"):
            by_work.setdefault(p["_work"], []).append(p)
    for p in people:
        cands = [q for q in by_building[p["_flat"]["building"]] if q is not p and q["household"] != p["household"] and abs(q["age"] - p["age"]) < 15]
        if p.get("_work"):
            cands += [q for q in by_work[p["_work"]] if q is not p]
        rnd.shuffle(cands)
        p["friends"] = sorted({q["key"] for q in cands[: rnd.randint(1, 4)]})
    return people


def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=1)
        f.write("\n")


def main():
    global REFS_FIRST
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=20261106)
    ap.add_argument("--wall-occupancy", type=float, default=0.5)
    ap.add_argument("--house-occupancy", type=float, default=0.75)
    ap.add_argument("--max-people", type=int, default=900)
    ap.add_argument("--summary", action="store_true")
    ap.add_argument("--ref", action="append", default=[], help="a branch to read inputs from that aren't in the working tree yet")
    ap.add_argument("--refs-first", action="store_true", help="the --ref branches win over the working tree (their inputs are newer)")
    args = ap.parse_args()
    GIT_REFS.extend(args.ref)
    REFS_FIRST = args.refs_first
    rnd = random.Random(args.seed)
    with open(PLAN, encoding="utf-8") as f:
        plan = json.load(f)
    extra = read_input(os.path.relpath(EXTRA, ROOT)) or {}
    places, flats, workplaces = build_places(plan, extra, args)
    people = build_people(plan, places, flats, workplaces, args, rnd)

    # the stairs between the terraces: the people's ways up and down (PeopleSim's town ground)
    levels = [{"id": "B", "z": -41.5}] + [{"id": t["id"], "z": t["z"]} for t in plan["terraces"]]
    stairs = []
    for s in plan["stairs"]:
        stairs.append({"id": s["id"], "bottom": from_ue_cm(s["bottom_ue"]), "top": from_ue_cm(s["top_ue"])})
    ys = [p["ue"][1] / 100.0 for p in plan["plots"]] + [s["ue"][1] / 100.0 for s in plan["spots"]]
    xs = [p["ue"][0] / 100.0 for p in plan["plots"]] + [s["ue"][0] / 100.0 for s in plan["spots"]] + [d["door_ue"][0] / 100.0 for d in plan["wall_dwellings"]]
    town = {"box": [round(min(xs) - 60, 1), round(min(ys) - 60, 1), round(max(xs) + 60, 1), round(max(ys) + 60, 1)], "levels": levels, "stairs": stairs}

    write(OUT_PLACES, {"_doc": "The town's places for the people's simulation, made by tools/town/town_life.py from Content/City/city_plan.json "
                       "(docs/town_life.md). Unreal metres. shell: a building without rooms to walk (inside = not drawn).",
                       "seed": args.seed, "town": town, "places": places})
    folk = []
    for p in people:
        q = {k: v for k, v in p.items() if not k.startswith("_")}
        folk.append(q)
    write(OUT_FOLK, {"_doc": "The townsfolk, made by tools/town/town_life.py (seeded; docs/town_life.md). Names: npc.<key>.name in "
                     "Content/Localization/town/strings_<lang>.json. mesh: a model borrowed from the shared pool until their own is made.",
                     "seed": args.seed, "people": folk})
    # names and occupations in the three languages
    for i, lang in enumerate(("ru", "en", "ja")):
        table = {}
        for p in people:
            table["npc.%s.name" % p["key"]] = p["_name"][lang]
            table["npc.%s.role" % p["key"]] = ROLES[role_key(p["occupation"], bool(p["female"]))][i] if p["occupation"] in ROLES else p["occupation"]
        write(os.path.join(OUT_STRINGS, "strings_%s.json" % lang), table)

    jobs = sum(1 for p in people if any(d.get("place") for d in p["duties"]) and p["occupation"] != "pupil")
    open_posts = sum(sum(c for _, c, _, _ in w["staff"]) for w in workplaces)
    ctypes = {}
    for p in people:
        ctypes[p["chronotype"]] = ctypes.get(p["chronotype"], 0) + 1
    print("town_life: %d places (%d flats, %d workplaces), %d households, %d people; %d of ~%d posts filled; %s"
          % (len(places), len(flats), len(workplaces), len({p["household"] for p in people}), len(people), jobs, open_posts, ctypes))
    if args.summary:
        for p in people[:40]:
            print("  %-34s %-26s %2d %-11s %-6s wake %5.2f bed %5.2f %s %s" % (p["key"], p["_name"]["en"], p["age"], p["occupation"],
                  p["chronotype"], p["wake"], p["bed"], p["home"], [(d["activity"], d.get("place"), d["hours"]) for d in p["duties"]]))


if __name__ == "__main__":
    main()
