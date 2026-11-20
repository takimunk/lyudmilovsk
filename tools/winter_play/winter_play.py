#!/usr/bin/env python3
"""Winter play (docs/winter_play.md, interactives backlog item 18): the skating rink on the park pond, the banya by the
pier's root and the walruses' ice hole off the pier, snowballs and snowmen. Writes the game's data from the sources.

Sources:  tools/winter_play/content.json   (places in the terrace frame, prices, the schedule, the model's numbers)
          tools/winter_play/strings.json   (every word, {"key": {"ru", "en", "ja"}}; Russian is the source)
          unreal/Lyudmilovsk/Content/City/public_spaces.json  (the pond: centre, radii)
Writes:   unreal/Lyudmilovsk/Content/City/winter_play.json                       (ULyudWinterPlay, Private/Winter/WinterPlayModel.h)
          unreal/Lyudmilovsk/Content/PeopleData/winter_spots.json                (the people's places: the rink, the banya, the ice hole)
          unreal/Lyudmilovsk/Content/Localization/winter_play/strings_<lang>.json (merged at load like Localization/town)

  python3 tools/winter_play/winter_play.py           write everything
  python3 tools/winter_play/winter_play.py --check   write nothing; fail when a generated file is stale or a check fails

Heights: the banya's floor, the deck and the ice hole's depth come from tools/terrain.py (the built ground when
Saved/World/terrain_out.bin is there, else the macro shape; the game traces the real ground and deck at play start and
only uses these for the people's places, which it lifts onto whatever it finds).
"""
import json
import math
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOLS = os.path.join(ROOT, "tools")
HERE = os.path.join(TOOLS, "winter_play")
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
PUBLIC = os.path.join(CONTENT, "City", "public_spaces.json")
OUT_DATA = os.path.join(CONTENT, "City", "winter_play.json")
OUT_SPOTS = os.path.join(CONTENT, "PeopleData", "winter_spots.json")
OUT_STRINGS = os.path.join(CONTENT, "Localization", "winter_play", "strings_%s.json")
ENGINE_DIR = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Source", "Lyudmilovsk", "Private", "Winter")
INVENTORY_CPP = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Source", "Lyudmilovsk", "Private", "LyudInventory.cpp")
OUT_TELEPORTS = os.path.join(CONTENT, "Dev", "teleports", "winter.json")
CITY_X = os.path.join(TOOLS, "city", "content", "x_winter.json")
CITY_X_STRINGS = os.path.join(TOOLS, "city", "content", "strings", "x_winter.json")
LANGS = ("ru", "en", "ja")

sys.path.insert(0, TOOLS)
_err = sys.stderr
sys.stderr = open(os.devnull, "w")      # terrain.py warns on import when the built ground is missing; said below instead
try:
    import city_plan as C  # noqa: E402
    import terrain as T  # noqa: E402
    import world_map as W  # noqa: E402
finally:
    sys.stderr.close()
    sys.stderr = _err

errors = []


def err(msg):
    errors.append(msg)


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def dump(obj):
    return json.dumps(obj, ensure_ascii=False, indent=1) + "\n"


def r3(v):
    return [round(float(c), 3) for c in v]


def placeholders(s):
    return sorted(set(re.findall(r"\{(\d+)\}", s)))


class Frame:
    """The terrace frame (s = metres inland from the sea line, y along the coast) to Unreal metres (x = map X, y = map Y +
    the public spaces' face_y, z)."""

    def __init__(self, face_y):
        self.face_y = face_y

    def map(self, s, y):
        return C.to_map(s, y)

    def ue(self, s, y, z):
        x, yy = C.to_map(s, y)
        return r3((x, yy + self.face_y, z))

    def ground(self, s, y):
        x, yy = C.to_map(s, y)
        return float(T.ground_z(x, yy))

    def yaw_of(self, s0, y0, s1, y1):
        """Unreal yaw (deg) of the way from (s0, y0) to (s1, y1)."""
        ax, ay = C.to_map(s0, y0)
        bx, by = C.to_map(s1, y1)
        return round(math.degrees(math.atan2(by - ay, bx - ax)), 2)


def build(content, public):
    F = Frame(public["face_y"])
    out = {"_doc": "Built by tools/winter_play/winter_play.py from tools/winter_play/content.json (docs/winter_play.md); "
                   "do not edit by hand. Unreal metres (x = map X, y = map Y + face_y, z), yaw Unreal degrees (0 = +x = the "
                   "sea). Heights are guesses from tools/terrain.py: the game traces the ground and the pier's deck.",
           "version": 1}
    spots = []

    # --- the rink: the park pond's ice (ALyudPublicSpaces draws the ice; this is where skating happens on it)
    pond = next((w for w in public["waters"] if w["id"] == "pond"), None)
    if not pond:
        err("public_spaces.json has no pond")
        return out, spots
    rk = content["rink"]
    rx, ry = pond["r"]
    pc = pond["at"]
    inset = rk["loop_inset_m"]
    out["rink"] = {"at": r3(pc), "r": [rx, ry], "yaw": pond.get("yaw", 0.0), "kerb": pond.get("kerb", 0.35)}
    loop = []
    n = rk["loop_points"]
    for j in range(n + 1):
        a = 2 * math.pi * j / n
        loop.append(r3((pc[0] + math.cos(a) * (rx - inset), pc[1] + math.sin(a) * (ry - inset), pc[2])))
    out["rink"]["loop"] = loop
    slots = []
    for j in range(rk["slots"]):
        a = 2 * math.pi * (j + 0.5) / rk["slots"]
        # anticlockwise round the rink (seen from above): the way the skaters go
        yaw = math.degrees(math.atan2(math.cos(a) * (ry - inset), -math.sin(a) * (rx - inset)))
        slots.append({"pos": r3((pc[0] + math.cos(a) * (rx - inset), pc[1] + math.sin(a) * (ry - inset), pc[2])),
                      "yaw": round(yaw, 2), "height": 0.0, "kind": "skate"})
    spots.append({"id": "S-RINK", "kind": "rink", "pos": r3(pc), "yaw": 0.0, "seats": len(slots), "slots": slots,
                  "path": loop, "season": "winter", "name_ru": "Каток на пруду", "name_en": "The pond rink"})

    # --- the banya by the pier's root, on the sand under the seawall, its door to the sea
    bn = content["banya"]
    bs, by = bn["s"], bn["y"]
    gz = F.ground(bs, by)
    yaw = F.yaw_of(bs, by, bs - 1.0, by)               # the door faces the sea (-s)
    out["banya"] = {"at": F.ue(bs, by, gz), "yaw": yaw, "size": bn["size_m"], "door": bn["door"],
                    "steam_room": bn["steam_room"], "dressing_room": bn["dressing_room"]}
    door_s = bs - bn["size_m"][0] / 2.0 - 1.2
    spots.append({"id": "S-BANYA", "kind": "banya", "pos": F.ue(door_s, by, F.ground(door_s, by)), "yaw": yaw,
                  "seats": bn["people"], "indoor": True,
                  "slots": [{"pos": F.ue(bs, by + (k - (bn["people"] - 1) / 2.0) * 0.6, gz + 0.9), "yaw": yaw,
                             "height": 0.9, "kind": "banya_shelf"} for k in range(bn["people"])],
                  "name_ru": "Баня у пирса", "name_en": "The banya by the pier"})

    # --- the walruses' ice hole off the pier's north side: the first point out along the pier deep enough at winter
    # low water (docs/winter.md "Winter low water"), with a ladder down from the deck
    ih = content["ice_hole"]
    need = W.SEA_Z - ih["winter_low_water_m"] - ih["depth_m"]
    hole_y = ih["pier_y"] + ih["pier_half_width_m"] + ih["off_deck_m"]
    hs = None
    s = ih["search_s"][0]
    while s >= ih["search_s"][1]:
        if F.ground(s, hole_y) <= need:
            hs = s
            break
        s -= 1.0
    shallow = hs is None
    if shallow:
        hs = ih["search_s"][1]
    deck_z = W.SEA_Z + ih["deck_over_datum_m"]
    edge_y = ih["pier_y"] + ih["pier_half_width_m"]
    # the hole is one tile of the sea ice's solid grid (ALyudSeaIce: 3 m tiles on the world grid, Unreal metres): its
    # middle snapped to the grid, clear of the deck's edge; the ladder on the deck's side face in front of it
    tile = ih["tile_m"]
    hx, _ = F.map(hs, hole_y)
    hx = round(hx / tile) * tile
    deck_edge_ue = F.map(hs, edge_y)[1] + F.face_y
    hy = math.ceil((deck_edge_ue + tile * 0.5 + 0.25) / tile) * tile
    bed = float(T.ground_z(hx, hy - F.face_y))
    out["ice_hole"] = {"at": r3((hx, hy, W.SEA_Z)), "r": ih["radius_m"], "tile": tile, "bed_z": round(bed, 2),
                       "shallow_guess": shallow,
                       # the ladder stands in the hole at its pier side (a climber's body must hang in open water, not
                       # in the ice), hooked over the deck's edge; the climb steps off back onto the deck (top_back_m)
                       "ladder": {"at": r3((hx, hy - tile * 0.5 + 0.08, bed)), "yaw": 90.0, "deck_z": round(deck_z, 2),
                                  "top_back_m": round(hy - tile * 0.5 + 0.08 - (deck_edge_ue - 0.55), 2),
                                  "deck_edge_y": round(deck_edge_ue, 3)},
                       "deck_stand": r3((hx, deck_edge_ue - 1.2, deck_z))}

    def yaw_to(ax, ay, bx, by):
        return round(math.degrees(math.atan2(by - ay, bx - ax)), 2)
    hslots = []
    for k in range(ih["swimmers"]):
        a = 2 * math.pi * (k + 0.5) / ih["swimmers"]
        px, py = hx + math.cos(a) * tile * 0.28, hy + math.sin(a) * tile * 0.28
        hslots.append({"pos": r3((px, py, W.SEA_Z)), "yaw": yaw_to(px, py, hx, hy), "height": 0.0, "kind": "walrus"})
    for k in range(ih["watchers"]):
        px = hx + (k - (ih["watchers"] - 1) / 2.0) * 1.1
        hslots.append({"pos": r3((px, deck_edge_ue - 1.0, deck_z)), "yaw": 90.0, "height": 0.0, "kind": "stand"})
    spots.append({"id": "S-ICEHOLE", "kind": "ice_hole", "pos": r3((hx, deck_edge_ue - 1.0, deck_z)), "yaw": 90.0,
                  "seats": len(hslots), "slots": hslots, "season": "winter",
                  "name_ru": "Прорубь у пирса", "name_en": "The ice hole by the pier"})

    # --- the rest of the model's numbers, passed through
    for k in ("ice", "skating", "banya_rules", "walrus", "snowball", "snowman", "items", "stamps", "prices", "radio", "fight", "banya_shop"):
        if k in content:
            out[k] = content[k]
    return out, spots


def items_in_cpp():
    with open(INVENTORY_CPP, encoding="utf-8") as f:
        return set(re.findall(r'(?:Good|Wear|Misc|Feed|Place|Item)\(TEXT\("([a-z0-9_]+)"\)', f.read()))


def check_strings(content, strings):
    """Every key the content names exists in every language with the same placeholders."""
    for key, langs in strings.items():
        if key.startswith("_"):
            continue
        for lang in LANGS:
            if not str(langs.get(lang, "")).strip():
                err("string %s: no %s" % (key, lang))
        ph = placeholders(langs.get("ru", ""))
        for lang in ("en", "ja"):
            if placeholders(langs.get(lang, "")) != ph:
                err("string %s: %s placeholders %s, ru %s" % (key, lang, placeholders(langs.get(lang, "")), ph))

    def walk(v, where):
        if isinstance(v, dict):
            for k, x in v.items():
                walk(x, where + "." + k)
        elif isinstance(v, list):
            for i, x in enumerate(v):
                walk(x, "%s[%d]" % (where, i))
        elif isinstance(v, str) and v.startswith("winter.") and " " not in v:
            if v.endswith(".*"):
                if not any(k.startswith(v[:-1]) for k in strings):
                    err("%s: no strings %s" % (where, v))
            elif v not in strings:
                err("%s: string %s missing" % (where, v))
    walk(content, "content")

    # the engine's own keys: every LyudText key the winter play's C++ names, and SayOne(prefix, n)'s numbered lines
    src = ""
    for name in ("LyudWinterPlay.cpp", "LyudWinterPlayBuild.cpp"):
        path = os.path.join(ENGINE_DIR, name)
        if os.path.exists(path):
            with open(path, encoding="utf-8") as f:
                src += f.read()
    for key in set(re.findall(r'TEXT\("((?:winter|dev\.action\.winter)\.[a-z0-9_.]+)"\)', src)):
        if key.count(".") < 2 or key.startswith("winter.radio"):
            continue          # a dev action's id ("winter.kit"), not a string
        numbered = re.search(r'SayOne\(TEXT\("%s"\),\s*(\d+)\)' % re.escape(key), src)
        if numbered:
            for i in range(1, int(numbered.group(1)) + 1):
                if "%s.%d" % (key, i) not in strings:
                    err("engine: string %s.%d missing" % (key, i))
        elif key not in strings:
            err("engine: string %s missing" % key)


def main():
    check = "--check" in sys.argv
    content = load(os.path.join(HERE, "content.json"))
    strings = load(os.path.join(HERE, "strings.json"))
    public = load(PUBLIC)
    data, spots = build(content, public)
    check_strings(content, strings)
    known = items_in_cpp()
    for it in content.get("items", {}):
        if it not in known:
            err("item %s is not in LyudInventory.cpp" % it)
        for suffix in ("name", "desc"):
            if "item.%s.%s" % (it, suffix) not in strings:
                err("item %s: string item.%s.%s missing" % (it, it, suffix))
    # the shops that sell them (tools/city/content/counters.json, a hand-kept table: the goods are added there)
    counters = load(os.path.join(TOOLS, "city", "content", "counters.json")).get("shops", {})
    for it, spec in content.get("items", {}).items():
        shop = spec.get("shop")
        if shop and not any(g.get("item") == it for g in counters.get(shop, {}).get("goods", [])):
            err("item %s: shop %s does not sell it (tools/city/content/counters.json)" % (it, shop))
    if data.get("ice_hole", {}).get("shallow_guess"):
        print("winter_play: note: no point along the pier is deep enough by tools/terrain.py's ground (the macro shape "
              "without Saved/World/terrain_out.bin?); the ice hole is put near the head; the game checks the real depth")
    outs = {OUT_DATA: dump(data),
            OUT_SPOTS: dump({"_doc": "Built by tools/winter_play/winter_play.py (docs/winter_play.md): the people's winter "
                                     "places, the same format as town_plan.json's spots. Do not edit by hand.",
                             "spots": spots})}
    # developer mode's points (.claude/skills/dev-mode): map metres (y = Unreal y - face_y), dropped onto the ground
    fy = public["face_y"]
    rk, bn, ih = data["rink"], data["banya"], data["ice_hole"]
    by = math.radians(bn["yaw"])
    door = (bn["at"][0] + math.cos(by) * (bn["door"][0] + 2.6), bn["at"][1] + math.sin(by) * (bn["door"][0] + 2.6))
    tps = [{"id": "winter.rink", "group": "winter", "name": "dev.tp.winter.rink",
            "x": round(rk["at"][0], 2), "y": round(rk["at"][1] - rk["r"][1] - 2.5 - fy, 2), "yaw": 90.0},
           {"id": "winter.banya", "group": "winter", "name": "dev.tp.winter.banya",
            "x": round(door[0], 2), "y": round(door[1] - fy, 2), "yaw": round(bn["yaw"] + 180.0, 1)},
           {"id": "winter.ice_hole", "group": "winter", "name": "dev.tp.winter.ice_hole",
            "x": round(ih["deck_stand"][0], 2), "y": round(ih["deck_stand"][1] - fy, 2), "yaw": 90.0}]
    outs[OUT_TELEPORTS] = dump({"about": "Written by tools/winter_play/winter_play.py: x/y map metres (docs/world_map.md), "
                                         "yaw Unreal degrees; outdoors, dropped onto the ground.", "teleports": tps})
    for t in tps:
        if t["name"] not in strings:
            err("teleport %s: string %s missing" % (t["id"], t["name"]))
    # the city's side (tools/city/interiors.py reads these; run it after): the collection "winter", given by the engine
    st = content["stamps"]
    title = "city.coll.%s.title" % st["set"]
    outs[CITY_X] = dump({"_doc": "Generated by tools/winter_play/winter_play.py (docs/winter_play.md): the winter play's stamps, "
                                 "given by the engine (ULyudWinterPlay). Do not edit; run tools/city/interiors.py after.",
                         "collections": {st["set"]: {"title": title, "pieces": st["pieces"],
                                                     "code": ["unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Winter/LyudWinterPlay.cpp"]}}})
    city_strings = {"_doc": "Generated by tools/winter_play/winter_play.py from its strings.json. Do not edit."}
    for k in sorted(strings):
        if k == title or k.startswith("city.coll.%s." % st["set"]):
            city_strings[k] = strings[k]
    if title not in strings:
        err("string %s (the collection's title) missing" % title)
    outs[CITY_X_STRINGS] = dump(city_strings)
    for lang in LANGS:
        outs[OUT_STRINGS % lang] = dump({k: v[lang] for k, v in sorted(strings.items()) if not k.startswith("_") and not k.startswith("city.")})
    if errors:
        for e in errors:
            print("ERROR:", e)
        sys.exit(1)
    stale = []
    for path, text in outs.items():
        old = open(path, encoding="utf-8").read() if os.path.exists(path) else None
        if old != text:
            stale.append(os.path.relpath(path, ROOT))
            if not check:
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, "w", encoding="utf-8") as f:
                    f.write(text)
    if check and stale:
        print("stale:", ", ".join(stale))
        sys.exit(1)
    print("winter_play: rink %s, banya at %s, ice hole at %s (bed %.2f m), %d strings%s" % (
        data["rink"]["r"], data["banya"]["at"], data["ice_hole"]["at"], data["ice_hole"]["bed_z"], len(strings),
        ("; wrote " + ", ".join(stale)) if stale and not check else ""))


if __name__ == "__main__":
    main()
