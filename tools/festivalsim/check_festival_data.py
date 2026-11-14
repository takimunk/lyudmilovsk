#!/usr/bin/env python3
"""The festivals' links to the rest of the game's data (docs/festivals.md), outside the engine:
venues and gathering places (town_places.json), the people's gathering activities (activities.json), the side quests
(quests.json, the doc's list), every word in ru/en/ja, the restaurant's dishes (menu.json), the sounds and the music
(made by tools/festival_sounds.py / audio/sounds.json, imported or pending), and the decor kinds, shows and faces the
game's code knows. One line per check: "ok   <check>" or "FAIL <check>"; exits 1 on a failure.
    python3 tools/festivalsim/check_festival_data.py
"""
import json
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
GAME = os.path.join(ROOT, "unreal", "Lyudmilovsk")
CONTENT = os.path.join(GAME, "Content")
CODE = os.path.join(GAME, "Source", "Lyudmilovsk", "Private", "Festival", "LyudFestival.cpp")

fails = 0
checks = 0


def check(ok, what, bad=None):
    global fails, checks
    checks += 1
    if bad:
        what += ": " + ", ".join(sorted(set(str(b) for b in bad)))[:600]
    print(("ok   " if ok else "FAIL ") + what)
    if not ok:
        fails += 1


def load(*parts):
    with open(os.path.join(*parts), encoding="utf-8") as f:
        return json.load(f)


fests = load(CONTENT, "PeopleData", "festivals.json")["festivals"]
ids = [f["id"] for f in fests]
code = open(CODE, encoding="utf-8").read()
check(len(ids) == len(set(ids)), "festival ids are unique", [i for i in ids if ids.count(i) > 1])

phases = [(f, p) for f in fests for p in f.get("program", [])]

# ---- venues: the people's life spreads a gathering over every place of the kind (or the place of that id)
town = load(CONTENT, "PeopleData", "town_places.json")["places"]
kinds = {}
for pl in town:
    if not pl.get("shell"):
        kinds.setdefault(pl.get("kind"), []).append(pl)
town_ids = {pl.get("id") for pl in town}
venues = {p.get("venue", "square") for f, p in phases}
check(all(v in kinds or v in town_ids for v in venues), "every phase's venue is a kind (or id) of town_places.json, so its gathering has places",
      [v for v in venues if v not in kinds and v not in town_ids])
decor_venues = {v for f in fests for v in f.get("decor", {}).get("venues", [])}
check(all(v in kinds or v in town_ids for v in decor_venues), "every decorations venue is a kind (or id) of town_places.json",
      [v for v in decor_venues if v not in kinds and v not in town_ids])
# a concert's watchers stand in the square's audience (LyudFestival.cpp: S-SQUARE-AUDIENCE when the plan's stage is built)
audience = re.findall(r'TEXT\("(S-[A-Z0-9-]+)"\)', code)
check(all(a in town_ids for a in audience), "the place ids the festival code sends people to exist in town_places.json (%s)" % ", ".join(sorted(set(audience))),
      [a for a in audience if a not in town_ids])
# the fallback kinds when a plan has no spot of a venue's own kind (VenueKinds): the venue itself comes first
vk = re.search(r"TArray<FString> VenueKinds\(const FString& Want\)(.*?)\n\t}", code, re.S)
listed = set(re.findall(r'Want == TEXT\("([a-z_]+)"\)', vk.group(1))) if vk else set()
check(vk is not None and venues | decor_venues <= listed, "every venue has its fallback kinds in the code (VenueKinds)", sorted((venues | decor_venues) - listed))

# ---- the people's life: a gathering activity for every mode, the hours sane
acts = load(CONTENT, "PeopleData", "activities.json")
acts = acts.get("activities", acts) if isinstance(acts, dict) else acts
gather_modes = {a.get("mode", "mingle") for a in acts if a.get("gather")}
modes = {p.get("mode", "mingle") for f, p in phases}
check(modes <= gather_modes, "every phase's mode has a gathering activity in activities.json (%s)" % ", ".join(sorted(gather_modes)), sorted(modes - gather_modes))
bad = []
for f, p in phases:
    h = p.get("hours", [10, 11])
    if not (len(h) == 2 and 0 <= h[0] < h[1] and h[1] - h[0] < 24 and h[0] < 48):
        bad.append("%s.%s %s" % (f["id"], p["id"], h))
check(not bad, "every phase lasts under a day and starts by the next night (the people's life gets hours mod 24)", bad)
bad = ["%s.%s" % (f["id"], p["id"]) for f in fests for p in f.get("program", []) if p.get("day", 0) >= f.get("days", 1)]
check(not bad, "every phase's day is one of its festival's days", bad)
bad = []
for f in fests:
    seen = set()
    for p in f.get("program", []):
        if p["id"] in seen:
            bad.append("%s.%s" % (f["id"], p["id"]))
        seen.add(p["id"])
check(not bad, "phase ids are unique within a festival (the gatherings, shows and quest targets are keyed by them)", bad)

# ---- the shows, decor kinds and faces the game knows
SHOW_ACTORS = {"fireworks", "sky_lanterns", "meteors", "bonfire", "water_lanterns", "firecrackers", "color_powder"}
SOUNDS = {"music", "drums", "bells"}
shows = {s for f, p in phases for s in p.get("show", [])}
start_show = re.search(r"void ULyudFestivalSubsystem::StartShow\(.*?\n}\n", code, re.S).group(0)
actor_shows = set(re.findall(r'Show == TEXT\("([a-z_]+)"\)', start_show))
check(SHOW_ACTORS == actor_shows, "the shows with an actor are the ones StartShow puts on (%s)" % ", ".join(sorted(actor_shows)), sorted(SHOW_ACTORS ^ actor_shows))
check(shows <= SHOW_ACTORS | SOUNDS | {"stage_lights"}, "every show in festivals.json is put on by the game", sorted(shows - SHOW_ACTORS - SOUNDS - {"stage_lights"}))
decor = {k for f in fests for k in f.get("decor", {}).get("kinds", [])}
build = re.search(r"void ALyudFestivalDecor::Build\(.*?\n}\n", code, re.S).group(0)
known = set(re.findall(r'Kinds\.Contains\(TEXT\("([a-z_]+)"\)\)', build))
check(decor <= known, "every decoration kind in festivals.json is built by ALyudFestivalDecor::Build", sorted(decor - known))
faces = {p.get("face") for f, p in phases if p.get("face")}
face_code = re.search(r"FVector ULyudFestivalSubsystem::FacePoint\(.*?\n}\n", code, re.S).group(0)
known = set(re.findall(r'Face == TEXT\("([a-z_]+)"\)', face_code))
check(faces <= known, "every face in festivals.json has a face point (FacePoint)", sorted(faces - known))
check(all(p.get("face") for f, p in phases if p.get("mode") == "watch"), "every watched phase says what the watchers face",
      ["%s.%s" % (f["id"], p["id"]) for f, p in phases if p.get("mode") == "watch" and not p.get("face")])

# ---- the side quests: fest_<id> starts on its festival, every objective can happen at it
quests = [q for q in load(CONTENT, "UI", "quests.json")["quests"] if q["id"].startswith("fest_")]
qids = {q["id"] for q in quests}
bad = []
for q in quests:
    s = q.get("start_on", {})
    if s.get("event") != "festival" or "fest_" + s.get("target", "") != q["id"] or s.get("target") not in ids:
        bad.append(q["id"])
check(not bad, "every fest_<id> quest starts on the festival event of festival <id>, which exists", bad)
by_id = {f["id"]: f for f in fests}
bad = []
for q in quests:
    fid = q["id"][5:]
    f = by_id.get(fid)
    if not f:
        continue
    fshows = {s for p in f.get("program", []) for s in p.get("show", []) if s in SHOW_ACTORS}
    fphases = {p["id"] for p in f.get("program", [])}
    for st in q.get("stages", []):
        for o in st.get("objectives", []):
            ev, t = o.get("event"), o.get("target", "")
            if ev == "festival_visit":
                ok = t == fid or (t.startswith(fid + ".") and t[len(fid) + 1:] in fphases)
            elif ev == "festival_watch":
                ok = t in fshows or (t.startswith(fid + ".") and t[len(fid) + 1:] in fshows)
            elif ev == "festival_lantern":
                ok = (t == "sky_lantern" and "sky_lanterns" in fshows) or (t == "water_lantern" and "water_lanterns" in fshows)
            elif ev == "festival_wish":
                ok = t == fid and "wish_tree" in f.get("decor", {}).get("kinds", [])
            else:
                ok = False
            if not ok:
                bad.append("%s.%s (%s %s)" % (q["id"], o.get("id"), ev, t))
check(not bad, "every fest_* objective is an event its festival sends (a phase visited, a show with an actor watched, a lantern, a wish)", bad)
# the other way: a festival where the hero can do something of his own (a lantern, a wish) has its quest
interactive = [f["id"] for f in fests if {"sky_lanterns", "water_lanterns"} & {s for p in f.get("program", []) for s in p.get("show", [])}
               or "wish_tree" in f.get("decor", {}).get("kinds", [])]
# (a design choice, not checked: Loy Krathong's floating lantern has no quest of its own)
print("     (festivals where the hero lets go a lantern or ties a wish without a quest of their own: %s)"
      % (", ".join(i for i in interactive if "fest_" + i not in qids) or "none"))
doc = open(os.path.join(ROOT, "docs", "festivals.md"), encoding="utf-8").read()
doc_q = set(re.findall(r"`(fest_[a-z_]+)`", doc))
check(doc_q == qids, "the fest_ quests in docs/festivals.md are those of quests.json", sorted(doc_q ^ qids))

# ---- words in every language
langs = {}
for lang in ("ru", "en", "ja"):
    d = load(CONTENT, "Localization", "strings_%s.json" % lang)
    langs[lang] = d.get("strings", d)
need = set()
for f in fests:
    need |= {"festival.%s.name" % f["id"], "festival.%s.today" % f["id"]}
SOON = set(re.findall(r'Show == TEXT\("([a-z_]+)"\)', re.search(r"the shows' \"soon\" notes(.*?)Announced\.Add", code, re.S).group(1)))
for f, p in phases:
    if p.get("show") and p["show"][0] in SOON:
        need.add("festival.soon." + p["show"][0])
need |= set(re.findall(r'TEXT\("(ui\.festival\.[a-z_.]+)"\)', code))
for q in quests:
    need |= {"quest.%s.title" % q["id"], "quest.%s.desc" % q["id"], "quest.%s.done" % q["id"]}
    for st in q.get("stages", []):
        need.add("quest.%s.%s.log" % (q["id"], st["id"]))
        for o in st.get("objectives", []):
            need.add("quest.%s.%s.obj" % (q["id"], o["id"]))
for lang, d in langs.items():
    check(need <= set(d), "every festival word (%d keys: names, notes, prompts, quests) is in strings_%s.json" % (len(need), lang), sorted(need - set(d)))
bad = []
for k in sorted(need & set(langs["ru"]) & set(langs["en"]) & set(langs["ja"])):
    ph = [tuple(sorted(re.findall(r"\{\d+\}", langs[l][k]))) for l in ("ru", "en", "ja")]
    if len(set(ph)) != 1 or any(not langs[l][k].strip() for l in ("ru", "en", "ja")):
        bad.append(k)
check(not bad, "the festival words have the same placeholders in ru/en/ja and none is empty", bad)
bad = [k for k in need if k.startswith("ui.festival.") and "prompt" in k and "money" not in k and k in langs["en"]
       and ("{0}" in langs["en"][k]) != (k.endswith("lantern_prompt"))]
check(not bad, "the lantern prompts take their price as {0}, the wish prompt none (LyudFestival.cpp passes it)", bad)

# ---- the restaurant
menu = load(CONTENT, "Restaurant", "menu.json")["dishes"]
dish_ids = {d["id"] for d in menu}
used = {(f["id"], d) for f in fests for d in f.get("menu", []) + f.get("foods", [])}
check(all(d in dish_ids for f, d in used), "every festival dish and fair food is a dish of menu.json", ["%s:%s" % u for u in used if u[1] not in dish_ids])
fest_dishes = {d["id"] for d in menu if d.get("festival")}
on_menu = {d for f in fests for d in f.get("menu", [])}
check(fest_dishes <= on_menu, "every festival-only dish of menu.json is on some festival's menu (else it is never served)", sorted(fest_dishes - on_menu))
items = set()
for lang in ("ru", "en", "ja"):
    items |= {d for f, d in used if "item.%s.name" % d not in langs[lang]}
check(not items, "every festival dish has its item name in ru/en/ja", sorted(items))

# ---- sounds and music: imported, or made by their generator (pending import)
gen = open(os.path.join(ROOT, "tools", "festival_sounds.py"), encoding="utf-8").read()
made = set()
for name, a, b in re.findall(r"audio/sfx/festival/([a-z_]+?)_(\d\d)(?:\.\.(\d\d))?\.wav", gen):
    for i in range(int(a), int(b or a) + 1):
        made.add("S_%s_%02d" % (name, i))
refs = set(re.findall(r"/Game/Audio/Festival/(S_[a-z_]+_\d\d)\.", code))
refs |= {s for s in re.findall(r'TEXT\("(S_[a-z_]+_\d\d)"\)', code)}
for name, lo, hi in (("firework_boom", 1, 3), ("firecrackers", 1, 2)):
    if re.search(r"S_%s_%%02d" % name, code):
        refs |= {"S_%s_%02d" % (name, i) for i in range(lo, hi + 1)}
imported = set()
fdir = os.path.join(CONTENT, "Audio", "Festival")
if os.path.isdir(fdir):
    imported = {os.path.splitext(n)[0] for n in os.listdir(fdir) if n.endswith(".uasset")}
check(refs and all(r in imported or r in made for r in refs),
      "every festival sound the code loads (%d) is imported or made by tools/festival_sounds.py (%d pending import)" % (len(refs), len(refs - imported)),
      sorted(r for r in refs if r not in imported and r not in made))
sounds = load(ROOT, "audio", "sounds.json")
sounds = sounds.get("music", []) + sounds.get("sfx", []) if isinstance(sounds, dict) else sounds
sound_ids = {s.get("id") for s in sounds if isinstance(s, dict)}
mdir = os.path.join(CONTENT, "Audio", "Music")
music_imported = {os.path.splitext(n)[0] for n in os.listdir(mdir)} if os.path.isdir(mdir) else set()
music = re.search(r"void ULyudFestivalSubsystem::StartMusic\(.*?\n}\n", code, re.S).group(0)
m = re.search(r"I <= (\d+);.*?TEXT\(\"MU_festival_%d\"\)", music, re.S)
want = {"festival_%d" % i for i in range(1, int(m.group(1)) + 1)} if m else set()
check(bool(want), "StartMusic looks for the tunes MU_festival_1..%d" % len(want))
check(all(("MU_" + t) in music_imported or t in sound_ids for t in want),
      "the festival music (MU_festival_1, MU_festival_2) is imported or listed in audio/sounds.json (%d pending)" % len([t for t in want if "MU_" + t not in music_imported]),
      [t for t in want if "MU_" + t not in music_imported and t not in sound_ids])

print(("%d of %d checks FAILED" % (fails, checks)) if fails else "all ok (%d checks)" % checks)
sys.exit(1 if fails else 0)
