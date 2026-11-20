#!/usr/bin/env python3
"""Records, the jukebox and the town radio «Волна Людмиловска» (docs/music.md): writes the game's data from the sources.

Sources:  tools/music/content.json  (tracks, records, shops, which city acts are players/radios, the station's schedule)
          tools/music/strings.json  (every word, {"key": {"ru", "en", "ja"}}; Russian is the source)
Writes:   unreal/Lyudmilovsk/Content/City/music.json                        (ULyudMusic, Private/Music/MusicModel.h)
          unreal/Lyudmilovsk/Content/Localization/music/strings_<lang>.json (merged at load like Localization/town)
          tools/city/content/x_music.json + tools/city/content/strings/x_music.json
                                                         (the jukeboxes' records and stamps, the records shop's stock)
Then run python3 tools/city/interiors.py (the city's data picks the city part up).

  python3 tools/music/music.py            write everything
  python3 tools/music/music.py --check    write nothing; fail when a generated file is stale or a check fails
  python3 tools/music/music.py --report   also list every city act the music treats as a jukebox, player, radio, request box
"""
import fnmatch
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HERE = os.path.join(ROOT, "tools", "music")
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
OUT_DATA = os.path.join(CONTENT, "City", "music.json")
OUT_STRINGS = os.path.join(CONTENT, "Localization", "music", "strings_%s.json")
OUT_CITY = os.path.join(ROOT, "tools", "city", "content", "x_music.json")
OUT_CITY_STRINGS = os.path.join(ROOT, "tools", "city", "content", "strings", "x_music.json")
INVENTORY_CPP = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Source", "Lyudmilovsk", "Private", "LyudInventory.cpp")
INTERIORS = os.path.join(CONTENT, "City", "interiors.json")
LANGS = ("ru", "en", "ja")

errors = []


def err(msg):
    errors.append(msg)


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def dump(obj, compact_lists=False):
    return json.dumps(obj, ensure_ascii=False, indent=1) + "\n"


def placeholders(s):
    return sorted(set(re.findall(r"\{(\d+)\}", s)))


def numbered(strings, prefix):
    """prefix.1, prefix.2, ... in order while they exist."""
    out = []
    n = 1
    while "%s.%d" % (prefix, n) in strings:
        out.append("%s.%d" % (prefix, n))
        n += 1
    return out


def items_in_cpp():
    with open(INVENTORY_CPP, encoding="utf-8") as f:
        return set(re.findall(r'TEXT\("([a-z0-9_]+)"\)', f.read()))


def main():
    check = "--check" in sys.argv
    report = "--report" in sys.argv
    content = load(os.path.join(HERE, "content.json"))
    strings = load(os.path.join(HERE, "strings.json"))

    # ---- words
    for k, v in strings.items():
        for l in LANGS:
            if not isinstance(v, dict) or not v.get(l, "").strip():
                err("string %s has no %s" % (k, l))
        if isinstance(v, dict) and v.get("ru"):
            p = placeholders(v["ru"])
            for l in LANGS[1:]:
                if v.get(l) and placeholders(v[l]) != p:
                    err("string %s: placeholders differ in %s (%s vs %s)" % (k, l, placeholders(v[l]), p))

    def need(key, why):
        if key not in strings:
            err("missing string %s (%s)" % (key, why))
        return key

    # ---- tracks and records
    tracks = []
    for cue, t in content["tracks"].items():
        if not os.path.exists(os.path.join(ROOT, "audio", "music", cue + ".json")):
            err("track %s has no audio/music/%s.json" % (cue, cue))
        if not os.path.exists(os.path.join(CONTENT, "Audio", "Music", "MU_%s.uasset" % cue)):
            err("track %s has no Content/Audio/Music/MU_%s.uasset" % (cue, cue))
        side = load(os.path.join(ROOT, "audio", "music", cue + ".json")) if os.path.exists(os.path.join(ROOT, "audio", "music", cue + ".json")) else {}
        secs = side.get("seconds", t["seconds"])
        if abs(secs - t["seconds"]) > 0.5:
            err("track %s: %s s in content.json, %s s in its sidecar" % (cue, t["seconds"], secs))
        tracks.append({"cue": cue, "seconds": secs, "title": need("music.trk.%s.title" % cue, "track"),
                       "artist": need("music.trk.%s.artist" % cue, "track"), "pools": t["pools"]})
    cues = [t["cue"] for t in tracks]
    known_items = items_in_cpp()
    records = []
    for r in content["records"]:
        if r["cue"] not in cues:
            err("record %s: unknown cue %s" % (r["id"], r["cue"]))
        if r["id"] not in known_items:
            err("record %s is not in LyudInventory.cpp" % r["id"])
        need("item.%s.name" % r["id"], "record item")
        need("item.%s.desc" % r["id"], "record item")
        records.append({"id": r["id"], "cue": r["cue"], "note": need("music.rec.%s.note" % r["cue"], "record"),
                        "jukebox_line": need("city.jb.%s" % r["cue"], "jukebox line")})
        icon = os.path.join(ROOT, "textures", "localized", "ru", "icon_%s.jpg" % r["id"])
        if not os.path.exists(icon):
            print("note: %s has no icon picture yet (textures/localized/ru/icon_%s.jpg)" % (r["id"], r["id"]))

    # ---- the station
    st = content["station"]
    pools = set()
    for t in tracks:
        pools.update(t["pools"])
    pools.add("records")
    for seg in st["schedule"]:
        if seg["kind"] in ("music", "requests") and seg.get("pool") not in pools:
            err("schedule %s: unknown pool %s" % (seg["at"], seg.get("pool")))
        if seg.get("show"):
            need(seg["show"], "show name")
    news = numbered(strings, "music.radio.news")
    seasons = {s: numbered(strings, "music.radio.news.%s" % s) for s in ("winter", "spring", "summer", "autumn")}
    requests = numbered(strings, "music.radio.req")
    dedications = numbered(strings, "music.radio.req.to")
    ship = numbered(strings, "music.radio.ship.area")
    if len(news) < 9:
        err("only %d news items (music.radio.news.N)" % len(news))
    if not requests or not dedications:
        err("no request lines or dedications")
    if len(ship) < st["shipping_areas"]:
        err("only %d shipping areas, %d wanted" % (len(ship), st["shipping_areas"]))
    fixed = ["music.radio.name", "music.radio.on", "music.radio.off", "music.radio.now.music", "music.radio.now.news",
             "music.radio.now.weather", "music.radio.now.shipping", "music.radio.now.sign_on", "music.radio.now.sign_off",
             "music.radio.now.requests", "music.radio.now.off_air", "music.radio.cap.track", "music.radio.cap.sign_on.1",
             "music.radio.cap.sign_on.2", "music.radio.cap.sign_on.3", "music.radio.cap.sign_off.1", "music.radio.cap.sign_off.2",
             "music.radio.cap.time", "music.radio.news.intro", "music.radio.news.outro", "music.radio.news.paper",
             "music.radio.weather.intro", "music.radio.weather.now", "music.radio.weather.calm", "music.radio.tide.rising",
             "music.radio.tide.falling", "music.radio.ship.intro", "music.radio.ship.outro", "music.radio.ship.line",
             "music.radio.req.intro", "music.radio.req.outro", "music.radio.req.hero", "music.player.play", "music.player.more",
             "music.player.none", "music.player.stop", "music.jukebox.now", "music.req.posted", "music.req.need_card",
             "music.req.already", "music.req.heard", "dev.group.music", "dev.action.music.give_records",
             "dev.action.music.program", "dev.action.music.request", "dev.action.music.stop"]
    fixed += ["music.radio.weather.advice.%s" % a for a in ("rain", "snow", "fog", "cold", "warm", "clear", "wind", "storm")]
    fixed += ["music.sky.%s" % a for a in ("clear", "partly", "cloudy", "overcast", "rain", "snow", "fog", "storm")]
    fixed += ["music.wind.%s" % a for a in ("n", "ne", "e", "se", "s", "sw", "w", "nw")]
    fixed += ["music.sea.%d" % i for i in range(4)] + ["music.vis.good", "music.vis.moderate", "music.vis.poor"]
    for k in fixed:
        need(k, "used by the game")

    roles = {k: v for k, v in content["players"].items() if not k.startswith("_")}
    data = {
        "about": "Generated by tools/music/music.py from tools/music/content.json; do not edit (docs/music.md).",
        "tracks": tracks,
        "records": records,
        "roles": roles,
        "station": {
            "name": st["name"], "range_m": st["range_m"], "talk_bed_volume": st["talk_bed_volume"],
            "caption_seconds": st["caption_seconds"], "off_air": st["off_air"], "auto_on_hours": st.get("auto_on_hours", [7, 22]),
            "schedule": st["schedule"], "weather_pools": st["weather_pools"], "news_per_bulletin": st["news_per_bulletin"],
            "requests_per_hour": st["requests_per_hour"], "shipping_areas": st["shipping_areas"],
            "request_collect": st.get("request_collect", ""),
            "news": news, "news_season": seasons, "requests": requests, "dedications": dedications, "ship_areas": ship,
        },
    }

    # ---- the city side: the jukeboxes' records (one line and one stamp each) and the records shop
    rec_by_id = {r["id"]: r for r in content["records"]}
    city_acts = {
        "jukebox_records": {
            "prompt": "city.do.jukebox.prompt", "minutes": 3, "price": 5, "sound": "jukebox_click",
            "lines": [{"k": "city.jb.%s" % r["cue"], "collect": "records/%s" % r["cue"]} for r in content["records"]],
        }
    }
    shops = {}
    for sid, s in content["shops"].items():
        if "records" not in s:
            continue
        ids = [r["id"] for r in content["records"]] if s["records"] == "*" else s["records"]
        shops[sid] = {"title": need(s["title"], "shop title"), "role": s["role"],
                      "goods": [{"item": i, "price": rec_by_id[i]["price"], "count": 1} for i in ids]}
    city = {"_doc": "Generated by tools/music/music.py (docs/music.md): the jukeboxes' records (act jukebox_records, which the "
                    "jukebox act of activities.json is like), one stamp of the 'records' collection each, and the records shop's "
                    "stock. Do not edit.",
            "acts": city_acts, "shops": shops}
    city_strings = {k: v for k, v in strings.items() if k.startswith("city.")}
    tables = {l: {k: v[l] for k, v in strings.items() if not k.startswith("city.")} for l in LANGS}

    # store_books' records (counters.json is the city's; checked here so the two stay in step)
    counters = load(os.path.join(ROOT, "tools", "city", "content", "counters.json"))
    for sid, s in content["shops"].items():
        if "add" in s:
            have = {g["item"] for g in counters.get("shops", {}).get(sid, {}).get("goods", [])}
            for i in s["add"]:
                if i not in have:
                    err("shop %s (tools/city/content/counters.json) does not sell %s" % (sid, i))

    if report and os.path.exists(INTERIORS):
        inter = load(INTERIORS)
        by = {}
        for t in inter["things"]:
            a = t["act"].split("@")[0]

            def m(role):
                return any(fnmatch.fnmatchcase(a, g) for g in roles.get(role, []))
            r = "request" if m("request") else "jukebox" if m("jukebox") else "player" if m("player") else \
                "radio" if (m("radio") and not m("not_radio")) else None
            if r:
                by.setdefault(r, []).append("%s (%s%s)" % (t["id"], a, ", auto" if m("auto_on") else ""))
        for r in sorted(by):
            print("%s: %d" % (r, len(by[r])))
            for x in by[r]:
                print("   " + x)

    outputs = {OUT_DATA: dump(data), OUT_CITY: dump(city), OUT_CITY_STRINGS: dump(city_strings)}
    for l in LANGS:
        outputs[OUT_STRINGS % l] = dump(tables[l])
    if errors:
        for e in errors:
            print("ERROR: " + e)
        sys.exit(1)
    stale = []
    for path, text in outputs.items():
        old = open(path, encoding="utf-8").read() if os.path.exists(path) else None
        if old != text:
            stale.append(os.path.relpath(path, ROOT))
            if not check:
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, "w", encoding="utf-8") as f:
                    f.write(text)
    print("music: %d tracks, %d records, %d schedule segments, %d news, %d requests, %d strings" %
          (len(tracks), len(records), len(st["schedule"]), len(news) + sum(len(v) for v in seasons.values()), len(requests), len(strings)))
    if check and stale:
        print("stale: " + ", ".join(stale) + " (run python3 tools/music/music.py)")
        sys.exit(1)
    if not check and stale:
        print("wrote " + ", ".join(stale))


if __name__ == "__main__":
    main()
