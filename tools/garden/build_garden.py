#!/usr/bin/env python3
"""Builds the allotment (docs/allotment.md) from tools/garden/data.py and tools/garden/words.py.

Writes:
  unreal/Lyudmilovsk/Content/Garden/garden.json                       the structure, read by ULyudGarden and tools/gardensim
  unreal/Lyudmilovsk/Content/Localization/garden/strings_<lang>.json  the words, merged by LyudText

Inputs besides data.py/words.py: Content/City/city_plan.json (the commune house's plot: where the allotment is),
Content/City/interiors.json (the market's control scales), Content/PeopleData/townsfolk.json (the neighbours),
Source/.../LyudInventory.cpp (the items), Source/.../Plants/*.cpp (the crops' species).

    python3 tools/garden/build_garden.py           # build
    python3 tools/garden/build_garden.py --check   # build into memory and fail if the committed files differ
"""
import argparse
import glob
import json
import math
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
SOURCE = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Source", "Lyudmilovsk")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import data as D  # noqa: E402
import words as W  # noqa: E402

LANGS = ("ru", "en", "ja")
NEEDS = ["dig", "sow", "water", "weed", "cover", "uncover", "pick", "clear"]
# town talk's bubbles (ULyudTownTalkSubsystem::SayLine): line.* keys, the barks' length limits
LINE_LIMIT = {"ru": 100, "en": 100, "ja": 48}


def load(rel):
    with open(os.path.join(CONTENT, rel), encoding="utf-8") as f:
        return json.load(f)


def needed_keys(crops, items):
    k = set()
    for it in items:
        k |= {"item.%s.name" % it["id"], "item.%s.desc" % it["id"]}
    k |= {"garden.crop." + c["id"] for c in crops}
    k |= {"garden.stage.%d" % s for s in range(5)} | {"garden.stage.spoilt", "garden.stage.dead"}
    k |= {"garden.soil.dry", "garden.soil.damp", "garden.soil.wet", "garden.weeds.some", "garden.weeds.many",
          "garden.cover.down", "garden.look.grass", "garden.look.dug", "garden.look.weedy", "garden.look.crop"}
    k |= {"garden.prompt." + p for p in ("dig", "sow", "water", "weed", "pick", "clear", "cover", "uncover", "fill", "rent",
                                         "no_spade", "no_seeds", "no_can", "can_empty", "barrel_ice", "board", "jam_ask",
                                         "contest", "sell", "scales_closed", "jam_wait", "look", "compost")}
    k |= {"garden.sow.title", "garden.sow.keys", "garden.sow.none"}
    k |= {"garden.toast." + t for t in ("rented", "no_money", "dug", "sown", "watered", "weeded", "picked", "cleared", "covered",
                                        "uncovered", "filled", "dead", "spoilt", "jam_given", "jam_ready", "contest_entered",
                                        "contest_already", "sold", "friend_seeds", "compost")}
    k |= {"garden.quality.%d" % q for q in (1, 2, 3)}
    k |= {"garden.diary." + d for d in ("rented", "first_harvest", "jam", "contest")}
    k |= {"garden.board.title", "garden.board.text", "garden.name"}
    k |= {"garden.letter.contest.%s" % p for p in ("1", "2", "3", "none")} | {"garden.letter.from"}
    k |= {"dev.group.garden", "dev.action.garden.kit", "dev.action.garden.grow_day", "dev.action.garden.report", "dev.tp.garden_bed"}
    k |= {"garden.place.%s" % p for p in ("1", "2", "3", "none")}
    lines = set()
    for n in NEEDS:
        lines |= {"garden.need.%s.%d" % (n, i) for i in (1, 2)}
    for s in range(4):
        lines |= {"garden.season.%d.%d" % (s, i) for i in (1, 2, 3)}
    lines |= {"garden.frost.%d" % i for i in (1, 2)} | {"garden.hello.%d" % i for i in (1, 2, 3)}
    lines |= {"garden.jam.ask.%d" % i for i in (1, 2)} | {"garden.jam.ready.%d" % i for i in (1, 2)}
    lines |= {"garden.praise.%d" % i for i in (1, 2)} | {"garden.gift.%d" % i for i in (1, 2)}
    return k, lines


def build():
    problems = []
    plan = load("City/city_plan.json")
    plot = next((p for p in plan["plots"] if p["id"] == D.SITE["plot"]), None)
    if not plot:
        return None, None, ["no plot %s in city_plan.json" % D.SITE["plot"]]
    yaw = math.radians(plot.get("yaw", 0.0))
    cx, cy, cz = plot["ue"]

    def world(local, rel=(0.0, 0.0)):
        # local metres (+x toward the plot's front, +y along it) -> Unreal cm, turned by the plot's yaw
        x = D.SITE["centre"][0] + rel[0] + local[0]
        y = D.SITE["centre"][1] + rel[1] + local[1]
        return [round(cx + (x * math.cos(yaw) - y * math.sin(yaw)) * 100.0, 1),
                round(cy + (x * math.sin(yaw) + y * math.cos(yaw)) * 100.0, 1), round(cz, 1)]

    deg = plot.get("yaw", 0.0)
    # nothing of the plan stands in the fenced square (other plots' footprints, spots, lamps)
    half = (D.SITE["size"][0] / 2 + 1.0, D.SITE["size"][1] / 2 + 1.0)
    centre = world((0.0, 0.0))
    for coll in ("plots", "spots", "lamps"):
        for p in plan.get(coll, []):
            if p is plot or "ue" not in p:
                continue
            dx, dy = (p["ue"][0] - centre[0]) / 100.0, (p["ue"][1] - centre[1]) / 100.0
            lx = dx * math.cos(-yaw) - dy * math.sin(-yaw)
            ly = dx * math.sin(-yaw) + dy * math.cos(-yaw)
            r = max(p.get("w", 0), p.get("d", 0)) / 2.0 if coll == "plots" else 0.5
            if abs(lx) < half[0] + r and abs(ly) < half[1] + r and abs(p["ue"][2] - cz) < 300:
                problems.append("%s %s stands in the allotment" % (coll, p.get("id")))
    folk = {p["key"]: p for p in load("PeopleData/townsfolk.json")["people"]}
    things = {t["id"]: t for t in load("City/interiors.json")["things"]}
    try:
        inv = open(os.path.join(SOURCE, "Private", "LyudInventory.cpp"), encoding="utf-8").read()
        items_known = set(re.findall(r'TEXT\("([a-z0-9_]+)"\)', inv))
    except OSError:
        items_known = set()
        problems.append("no LyudInventory.cpp")
    species_src = ""
    for f in glob.glob(os.path.join(SOURCE, "Private", "Plants", "*.cpp")):
        species_src += open(f, encoding="utf-8").read()

    crops = []
    for c in D.CROPS:
        e = {"id": c["id"], "species": c["species"], "seed": c["seed"], "yield": c["yield_"], "yield_count": c["yield_count"],
             "days": c["days"], "base": c["base"], "best": c["best"], "hot": c["hot"], "thirst": c["thirst"],
             "frost_kill": c["frost_kill"], "ripe_days": c["ripe_days"], "again": c.get("again", 0.0),
             "pickings": c.get("pickings", 1), "perennial": c.get("perennial", False), "seedling": c.get("seedling", False)}
        for it in (e["seed"], e["yield"]):
            if items_known and it not in items_known:
                problems.append("crop %s: no item %s in LyudInventory.cpp" % (c["id"], it))
        if 'TEXT("%s")' % c["species"] not in species_src:
            problems.append("crop %s: no species %s in Private/Plants" % (c["id"], c["species"]))
        crops.append(e)
    crop_ids = {c["id"] for c in crops}
    items = []
    for it in D.ITEMS:
        if items_known and it["id"] not in items_known:
            problems.append("item %s: not in LyudInventory.cpp" % it["id"])
        items.append({"id": it["id"], "price": it["price"], "sold": it.get("sold", "").split(), "edible": it.get("edible", False)})

    cw, cl = D.BED_SIZE[0] / D.CELLS[0], D.BED_SIZE[1] / D.CELLS[1]
    beds = []
    for b in D.BEDS:
        cells = []
        for j in range(D.CELLS[1]):
            for i in range(D.CELLS[0]):
                cells.append(world(((i + 0.5) * cw - D.BED_SIZE[0] / 2, (j + 0.5) * cl - D.BED_SIZE[1] / 2), b["at"]))
        e = {"id": b["id"], "keeper": b["keeper"], "ue": world((0.0, 0.0), b["at"]), "yaw": round(deg, 2), "cells": cells,
             "crops": b.get("crops", [])}
        if b["keeper"] != "hero" and b["keeper"] not in folk:
            problems.append("bed %s: no townsperson %s" % (b["id"], b["keeper"]))
        for c in e["crops"]:
            if c not in crop_ids:
                problems.append("bed %s: no crop %s" % (b["id"], c))
        if b["keeper"] != "hero" and len(e["crops"]) != len(cells):
            problems.append("bed %s: %d crops for %d cells" % (b["id"], len(e["crops"]), len(cells)))
        beds.append(e)
    if sum(1 for b in beds if b["keeper"] == "hero") != 1:
        problems.append("the hero must have exactly one bed")
    nb = dict(D.NEIGHBOUR)
    nbed = next((b for b in D.BEDS if b["id"] == nb["bed"]), None)
    if not nbed or nb["person"] not in folk:
        problems.append("neighbour %s: no bed or no townsperson" % nb["person"])
    neighbour = {"person": nb["person"], "bed": nb["bed"], "hours": nb["hours"], "days": nb["days"].split(),
                 "stand": world(nb["stand"], nbed["at"] if nbed else (0, 0)), "stand_yaw": round(deg + 180.0, 1)}
    props = [{"id": p["id"], "kind": p["kind"], "ue": world(p["at"]), "yaw": round(deg + p.get("yaw", 0.0), 1)} for p in D.PROPS]
    sc = things.get(D.MARKET["thing"])
    if not sc:
        problems.append("no market thing %s in interiors.json" % D.MARKET["thing"])
    market = {"thing": D.MARKET["thing"], "ue": sc["ue"] if sc else [0, 0, 0], "yaw": sc.get("yaw", 0.0) if sc else 0.0,
              "hours": D.MARKET["hours"]}
    for k in D.SELL:
        if k not in {c["yield"] for c in crops}:
            problems.append("sell %s: no crop yields it" % k)

    data = {
        "_doc": "Written by tools/garden/build_garden.py from tools/garden/data.py: do not edit by hand. The allotment behind the "
                "commune house: docs/allotment.md.",
        "rules": D.RULES,
        "cells": D.CELLS[0] * D.CELLS[1],
        "crops": crops,
        "items": items,
        "sell": D.SELL,
        "jam": D.JAM,
        "site": {"plot": D.SITE["plot"], "ue": centre, "yaw": round(deg, 2), "size": list(D.SITE["size"]),
                 "gate": world(D.SITE["gate"])},
        "bed_size": list(D.BED_SIZE),
        "cell_size": [round(cw, 3), round(cl, 3)],
        "beds": beds,
        "neighbour": neighbour,
        "props": props,
        "market": market,
    }

    keys, lines = needed_keys(crops, D.ITEMS)
    for k in sorted(keys | lines):
        if k not in W.STRINGS:
            problems.append("no words for %s" % k)
    for k, tr in W.STRINGS.items():
        marks = {lang: sorted(set(re.findall(r"\{\d\}", tr[lang]))) for lang in LANGS}
        if len({tuple(m) for m in marks.values()}) != 1:
            problems.append("placeholders differ in %s: %s" % (k, marks))
        for lang in LANGS:
            if not tr[lang].strip():
                problems.append("empty %s %s" % (lang, k))
            if k in lines and len(tr[lang]) > LINE_LIMIT[lang]:
                problems.append("%s %s is %d long (bubble limit %d)" % (lang, k, len(tr[lang]), LINE_LIMIT[lang]))
    for it in D.ITEMS:
        if it["id"] not in W.ICONS:
            problems.append("no icon prompt for %s" % it["id"])
    tables = {}
    for lang in LANGS:
        tables[lang] = {("line." + k) if k in lines else k: tr[lang] for k, tr in W.STRINGS.items()}
    return data, tables, problems


def dumps(obj, indent):
    return json.dumps(obj, ensure_ascii=False, indent=indent) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    data, tables, problems = build()
    for p in problems:
        print("PROBLEM", p, file=sys.stderr)
    if data is None:
        return 1
    outputs = {os.path.join(CONTENT, "Garden", "garden.json"): dumps(data, 1)}
    for lang, table in tables.items():
        outputs[os.path.join(CONTENT, "Localization", "garden", "strings_%s.json" % lang)] = dumps(table, 1)
    summary = "garden: %d keys, %d crops, %d items, %d beds" % (len(tables["ru"]), len(data["crops"]), len(data["items"]), len(data["beds"]))
    if args.check:
        stale = []
        for path, text in outputs.items():
            try:
                with open(path, encoding="utf-8") as f:
                    if f.read() != text:
                        stale.append(path)
            except OSError:
                stale.append(path)
        for s in stale:
            print("STALE", os.path.relpath(s, ROOT), file=sys.stderr)
        print("%s; %d problems, %d stale" % (summary, len(problems), len(stale)))
        return 1 if problems or stale else 0
    for path, text in outputs.items():
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            f.write(text)
    print("%s; %d problems" % (summary, len(problems)))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
