#!/usr/bin/env python3
"""The yacht school's words and the boathouse's things (docs/yacht_school.md, part G).

Reads:   tools/sailing/strings.json, strings_talk.json, strings_quests.json  ({"key": {"ru", "en", "ja"}}; Russian is the source)
         tools/sailing/things.json                                          (where the boathouse's things stand, school frame)
         unreal/Lyudmilovsk/Content/City/sailing.json                       (the placed school: tools/sailing/sailing.py)
Writes:  unreal/Lyudmilovsk/Content/Localization/sailing/strings_<lang>.json (merged at load like Localization/town)
         tools/city/content/x_sailing_place.json                            (the place S-SAIL-SCHOOL for tools/city/interiors.py)

    python3 tools/sailing/words.py            write
    python3 tools/sailing/words.py --check    write nothing; fail when an output is stale or a check fails

Checks: every language of every key, the same {n} placeholders in all three, no key of the city's table (city.*: those
live in tools/city/content/strings/x_sailing.json), every key of strings_needed.json (here or in the city's file), every
item of ours in LyudInventory.cpp, in the icon grid and with its two words, the shops' goods. After a change run
tools/city/interiors.py (the things, the quests' dev blocks). Stdlib only, deterministic.
"""
import collections
import json
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
SOURCES = ("strings.json", "strings_talk.json", "strings_quests.json")
OUT_STRINGS = os.path.join(CONTENT, "Localization", "sailing", "strings_%s.json")
OUT_PLACE = os.path.join(ROOT, "tools", "city", "content", "x_sailing_place.json")
CITY_STRINGS = os.path.join(ROOT, "tools", "city", "content", "strings", "x_sailing.json")
CITY_CONTENT = os.path.join(ROOT, "tools", "city", "content", "x_sailing.json")
COUNTERS = os.path.join(ROOT, "tools", "city", "content", "counters.json")
INVENTORY_CPP = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Source", "Lyudmilovsk", "Private", "LyudInventory.cpp")
LANGS = ("ru", "en", "ja")
# our items (game-items skill): every one in LyudInventory.cpp, with words and a cell of the icon grid
ITEMS = ("sail_card", "life_jacket", "sail_gloves", "knot_cord", "bailer", "sail_pennant", "sail_book", "brass_screws",
         "yacht_varnish", "lucky_whistle", "bottle_letter", "race_mark")
# who sells what (shop id of tools/city/content/counters.json -> items)
SELLS = {"sail_bosun": ("sail_gloves", "knot_cord", "sail_book", "bailer"), "store_sports": ("sail_gloves",),
         "hardware": ("brass_screws", "yacht_varnish")}

errors = []


def err(msg):
    errors.append(msg)


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f, object_pairs_hook=collections.OrderedDict)


def dump(obj):
    return json.dumps(obj, ensure_ascii=False, indent=1) + "\n"


def read_strings():
    out = collections.OrderedDict()
    for name in SOURCES:
        path = os.path.join(HERE, name)
        if not os.path.exists(path):
            continue
        for k, v in load(path).items():
            if k.startswith("_"):
                continue
            if k in out:
                err("%s: key %s twice" % (name, k))
            if k.startswith("city."):
                err("%s: %s belongs to tools/city/content/strings/x_sailing.json" % (name, k))
            if not isinstance(v, dict) or any(not v.get(l) for l in LANGS):
                err("%s: %s lacks a language" % (name, k))
                continue
            holes = [sorted(set(re.findall(r"\{(\d+)\}", v[l]))) for l in LANGS]
            if holes[0] != holes[1] or holes[0] != holes[2]:
                err("%s: %s: placeholders differ between the languages" % (name, k))
            out[k] = v
    return out


def build_place(things_src, school):
    """The place S-SAIL-SCHOOL in the city interiors' building frame: its front point is the people's door, x runs along the
    north wall toward the sea, y into the boathouse (tools/city/interiors.py local_to_world)."""
    bh = school["boathouse"]
    c = bh["corners"]          # (s0, y0), (s0, y1), (s1, y1), (s1, y0)
    s0, s1 = bh["s"]
    y0, y1 = bh["y"]
    yaw_sea = school["yaw"]
    faces = {"sea": yaw_sea, "land": yaw_sea + 180.0, "north": yaw_sea + 90.0, "south": yaw_sea - 90.0}
    front = bh["door"]["pos"]
    plot_yaw = bh["door"]["yaw"]
    a = math.radians(plot_yaw)

    def world(s, y):
        u, v = (s - s0) / (s1 - s0), (y - y0) / (y1 - y0)
        return [c[0][0] + u * (c[3][0] - c[0][0]) + v * (c[1][0] - c[0][0]), c[0][1] + u * (c[3][1] - c[0][1]) + v * (c[1][1] - c[0][1])]

    things = []
    for t in things_src["things"]:
        if t.get("point"):
            pt = school["points"].get(t["point"])
            if not pt:
                err("things.json: %s: no point %s in sailing.json" % (t["id"], t["point"]))
                continue
            wx, wy = pt["pos"][0], pt["pos"][1]
        else:
            wx, wy = world(t["s"], t["y"])
        if t.get("level") == "floor":
            wz = bh["floor_z"] + t["h"]
        else:
            near = school["points"].get(t.get("near", ""))
            if not near:
                err("things.json: %s: level ground needs 'near', a key of school.points" % t["id"])
                continue
            wz = near["pos"][2] + t["h"]
        if t["face"] not in faces:
            err("things.json: %s: face %s" % (t["id"], t["face"]))
            continue
        dx, dy = wx - front[0], wy - front[1]
        local = [round(dx * math.sin(a) - dy * math.cos(a), 3), round(-dx * math.cos(a) - dy * math.sin(a), 3), round(wz - front[2], 3)]
        rec = collections.OrderedDict([("id", t["id"]), ("kind", t["kind"]), ("pos", local),
                                       ("yaw", round((plot_yaw - faces[t["face"]]) % 360.0, 1))])
        for k, v in (t.get("props") or {}).items():
            rec[k] = v
        things.append(rec)
    p = things_src["place"]
    place = collections.OrderedDict([
        ("kind", p["kind"]), ("building", p["building"]), ("front", [round(v, 3) for v in front]), ("yaw", plot_yaw),
        ("staff", p["staff"]), ("name_ru", p["name_ru"]), ("name_en", p["name_en"]), ("w", bh["size"][0]), ("d", bh["size"][1]),
        ("export", collections.OrderedDict([("entrances", [{"id": "main", "kind": "street", "pos": [0.0, 0.0, 0.0]}]),
                                            ("things", things)]))])
    return collections.OrderedDict([
        ("_doc", "Generated by tools/sailing/words.py from tools/sailing/things.json and Content/City/sailing.json: the yacht "
                 "school's boathouse as a place of the city interiors (it is not in the city plan). Do not edit; what the things "
                 "do is in x_sailing.json."),
        ("places", collections.OrderedDict([(p["id"], place)]))])


def check_items(strings):
    with open(INVENTORY_CPP, encoding="utf-8") as f:
        cpp = f.read()
    grid_ids = set()
    grids = os.path.join(HERE, "icon_grids.json")
    if os.path.exists(grids):
        for g in load(grids)["grids"]:
            grid_ids.update(g["ids"])
    for it in ITEMS:
        if 'TEXT("%s")' % it not in cpp:
            err("item %s is not in LyudInventory.cpp" % it)
        for suffix in ("name", "desc"):
            if "item.%s.%s" % (it, suffix) not in strings:
                err("item %s: no item.%s.%s" % (it, it, suffix))
        if it not in grid_ids:
            err("item %s: no cell in tools/sailing/icon_grids.json" % it)
    shops = {}
    for path in (COUNTERS, os.path.join(ROOT, "tools", "city", "content", "furnished.json")):
        shops.update(load(path).get("shops") or {})
    for shop, items in SELLS.items():
        sold = {g["item"] for g in (shops.get(shop) or {}).get("goods", [])}
        for it in items:
            if it not in sold:
                err("shop %s does not sell %s" % (shop, it))


def main():
    check = "--check" in sys.argv
    strings = read_strings()
    city = load(CITY_STRINGS) if os.path.exists(CITY_STRINGS) else {}
    need = load(os.path.join(HERE, "strings_needed.json"))["keys"]
    for k in need:
        if k not in strings and k not in city:
            err("no words for %s (%s)" % (k, need[k]))
    check_items(strings)
    sailing = load(os.path.join(CONTENT, "City", "sailing.json"))
    outs = collections.OrderedDict()
    for lang in LANGS:
        outs[OUT_STRINGS % lang] = dump(collections.OrderedDict((k, strings[k][lang]) for k in sorted(strings)))
    outs[OUT_PLACE] = dump(build_place(load(os.path.join(HERE, "things.json")), sailing["school"]))
    if errors:
        for e in errors:
            print("ERROR:", e)
        sys.exit(1)
    stale = []
    for path, text in outs.items():
        was = open(path, encoding="utf-8").read() if os.path.exists(path) else None
        if was != text:
            stale.append(os.path.relpath(path, ROOT))
            if not check:
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, "w", encoding="utf-8") as f:
                    f.write(text)
    if check and stale:
        print("stale:", ", ".join(stale))
        sys.exit(1)
    print("sailing words: %d keys x 3 languages (+ %d of the city's), %d things, %d items%s" % (
        len(strings), len([k for k in city if not k.startswith("_")]), len(load(os.path.join(HERE, "things.json"))["things"]),
        len(ITEMS), ("; wrote " + ", ".join(stale)) if stale and not check else ""))


if __name__ == "__main__":
    main()
