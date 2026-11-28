#!/usr/bin/env python3
"""The flight home from Europa (docs/spacecraft.md): checks the flight model's numbers and writes the game's copy.

Source:  tools/spacecraft/flight.json   (every number of Private/Spacecraft/FlightModel.h; guide: MODEL_NOTES.md)
Writes:  unreal/Lyudmilovsk/Content/Spacecraft/flight.json   (FConfig::LoadJson)

The same numbers are built into the model (FConfig's defaults): tools/spacecraftsim/run.sh fails when they differ, so a
change goes into flight.json AND FlightModel.h, then this tool, then run.sh.

  python3 tools/spacecraft/flight.py            check and write
  python3 tools/spacecraft/flight.py --check    write nothing; fail when the game's copy is stale or a check fails
"""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "tools", "spacecraft", "flight.json")
OUT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Spacecraft", "flight.json")
STRINGS = os.path.join(ROOT, "tools", "spacecraft", "strings.json")

PHASES = ["parked", "prelaunch", "ascent", "coast", "approach", "entry", "chute", "splash", "afloat"]
SWITCHES = ["power", "air", "gyro", "seal", "arm"]
LAMPS = ["power", "air", "gyro", "hatch", "armed", "engine", "sep", "zero_g", "burn", "comm", "blackout", "g_high", "hull_hot",
         "drogue", "main_chute", "alt_low", "jets", "splash", "beacon", "o2_low", "co2_high", "batt_low", "master_caution", "record"]
COLOURS = ("green", "amber", "red", "white")
SOUNDS = ["liftoff", "stage_left", "engine_cutoff", "turn_start", "correction_burn_on", "correction_burn_off", "approach_start",
          "module_sep", "entry_interface", "blackout_start", "peak_g", "blackout_end", "drogue", "main_chute", "whiteout",
          "real_world", "jets", "splash"]
LINES = ["liftoff", "weightless", "europa_behind", "planet_grows", "ocean", "island", "fire", "chute", "home"]
COMPUTED = {"blackout_start", "blackout_end", "peak_g", "line.hero.flight_island.1"}
SECTIONS = ["times", "nap", "planet", "europa", "g", "heat", "chute", "attitude", "panel", "record", "afloat"]
CURVES = ["track_deg", "cabin_temp_c", "fuel_pct"]

errors = []


def err(msg):
    errors.append(msg)


def main():
    check = "--check" in sys.argv[1:]
    with open(SRC, encoding="utf-8") as f:
        d = json.load(f)
    if d.get("version") != 1:
        err("version must be 1")
    if d.get("phases") != PHASES:
        err("phases must be %s" % PHASES)
    for s in SECTIONS:
        sec = d.get(s)
        if not isinstance(sec, dict) or not sec:
            err("section %s is missing" % s)
            continue
        for k, v in sec.items():
            if isinstance(v, bool) or not isinstance(v, (int, float)):
                err("%s.%s is not a number" % (s, k))
    unknown = set(d) - set(SECTIONS) - set(CURVES) - {"_doc", "version", "phases", "switches", "lamps", "cues"}
    if unknown:
        err("unknown keys: %s" % sorted(unknown))
    if errors:
        return finish(check, d)

    t = d["times"]
    order = ["stage_left", "ascent_end", "look_back_start", "look_back_end", "turn_start", "turn_end", "burn", "approach", "module_sep",
             "entry", "chute", "main_chute", "whiteout_in", "whiteout_peak", "whiteout_out", "splash_phase", "touchdown", "end"]
    last = 0.0
    for k in order:
        if k not in t:
            err("times.%s is missing" % k)
        elif t[k] <= last:
            err("times.%s (%s) must come after %s" % (k, t[k], last))
        else:
            last = t[k]
    if not errors:
        if (t["ascent_end"], t["approach"], t["entry"], t["chute"], t["splash_phase"], t["end"]) != (75, 600, 720, 810, 885, 900):
            err("the phase times are not the contract's (docs/spacecraft.md): 75, 600, 720, 810, 885, 900")
        if t["burn"] + t["burn_seconds"] >= t["approach"]:
            err("the correction burn must end in the coast")
        nap = d["nap"]
        if nap["latest_start_before_approach"] <= nap["ends_before_approach"] or nap["scale"] <= 1:
            err("nap: latest start must be before its end; scale over 1")
    g = d["g"]
    if not (g["entry_peak"] <= 4.5 and d["times"]["entry"] < g["entry_peak_t"] < d["times"]["chute"]):
        err("g.entry_peak must be at most 4.5 and inside the entry")
    ch = d["chute"]
    if not (ch["v_entry_end"] > ch["v_drogue"] > ch["v_main"] > ch["v_touch"] > 0):
        err("chute: the speeds must fall from the entry's end to the touchdown")

    for name in CURVES:
        knots = d.get(name)
        if not isinstance(knots, list) or len(knots) < 2 or any(not isinstance(k, list) or len(k) != 2 for k in knots):
            err("%s must be a list of [t, value]" % name)
            continue
        if any(b[0] <= a[0] for a, b in zip(knots, knots[1:])):
            err("%s: the times must rise" % name)
        if knots[0][0] != 0:
            err("%s must start at t 0" % name)
    if not errors:
        tr = d["track_deg"]
        if any(b[1] > a[1] for a, b in zip(tr, tr[1:])) or tr[-1][1] != 0 or tr[-1][0] != d["times"]["touchdown"]:
            err("track_deg must only fall and reach 0 at the touchdown")
        if any(b[1] > a[1] for a, b in zip(d["fuel_pct"], d["fuel_pct"][1:])):
            err("fuel_pct must only fall")

    if d.get("switches") != SWITCHES:
        err("switches must be %s" % SWITCHES)
    lamps = d.get("lamps", [])
    if [l.get("id") for l in lamps] != LAMPS:
        err("lamps must be the 24 of the board, in its order: %s" % LAMPS)
    for i, l in enumerate(lamps):
        if l.get("colour") not in COLOURS:
            err("lamp %s: colour %r" % (l.get("id"), l.get("colour")))
        if (l.get("row"), l.get("col")) != (i // 6, i % 6):
            err("lamp %s: row/col must be %d/%d" % (l.get("id"), i // 6, i % 6))
        if not isinstance(l.get("fault"), bool) or not l.get("rule"):
            err("lamp %s needs fault and rule" % l.get("id"))
    if sorted(l["id"] for l in lamps if l.get("fault")) != ["batt_low", "o2_low"]:
        err("the fault lamps are o2_low and batt_low")

    cues = d.get("cues", [])
    ids = [c.get("id") for c in cues]
    if len(set(ids)) != len(ids):
        err("a cue id is there twice")
    last = 0.0
    for c in cues:
        cid, kind, when = c.get("id", ""), c.get("kind"), c.get("t")
        if kind not in ("sound", "say", "auto"):
            err("cue %s: kind %r" % (cid, kind))
        if kind == "say" and not re.fullmatch(r"line\.hero\.flight_[a-z_]+\.1", cid):
            err("cue %s: a say cue is line.hero.flight_<id>.1" % cid)
        if kind == "auto" and not re.fullmatch(r"auto\.[a-z_]+", cid):
            err("cue %s: an auto cue is auto.<id>" % cid)
        if when == "computed":
            if cid not in COMPUTED:
                err("cue %s: the model cannot find its time" % cid)
        elif isinstance(when, bool) or not isinstance(when, (int, float)) or not 0 <= when <= d["times"]["end"]:
            err("cue %s: t %r" % (cid, when))
        elif when < last:
            err("cue %s (%s) is listed before an earlier one (%s)" % (cid, when, last))
        else:
            last = when
    for s in SOUNDS:
        if s not in ids:
            err("no cue %s" % s)
    for l in LINES:
        if "line.hero.flight_%s.1" % l not in ids:
            err("no cue for the hero's line %s" % l)

    if os.path.exists(STRINGS):
        with open(STRINGS, encoding="utf-8") as f:
            words = json.load(f)
        need = ["flight.lamp.%s" % l for l in LAMPS] + ["flight.phase.%s" % p for p in PHASES] + ["flight.sw.%s" % s for s in SWITCHES]
        need += [c["id"] for c in cues if c.get("kind") == "say"] + ["flight.%s" % c["id"] for c in cues if c.get("kind") == "auto"]
        need += ["flight.auto.ready", "line.hero.flight_afloat.1"]
        for k in need:
            if k not in words:
                err("no words for %s (tools/spacecraft/strings.json)" % k)
    return finish(check, d)


def finish(check, d):
    if errors:
        for e in errors:
            print("flight.json: " + e)
        return 1
    text = json.dumps(d, ensure_ascii=False, indent=1) + "\n"
    old = None
    if os.path.exists(OUT):
        with open(OUT, encoding="utf-8") as f:
            old = f.read()
    if check:
        if old != text:
            print("Content/Spacecraft/flight.json is stale: run python3 tools/spacecraft/flight.py")
            return 1
        print("flight.json: ok (%d lamps, %d cues)" % (len(d["lamps"]), len(d["cues"])))
        return 0
    if old != text:
        os.makedirs(os.path.dirname(OUT), exist_ok=True)
        with open(OUT, "w", encoding="utf-8") as f:
            f.write(text)
        print("wrote " + os.path.relpath(OUT, ROOT))
    else:
        print("flight.json: up to date")
    return 0


if __name__ == "__main__":
    sys.exit(main())
