"""The townsfolk's own lines (docs/town_talk.md): builds the game's files from the authoring sources in tools/barks/lines/
(one file per theme, every line as key | ru | en | ja side by side, so the three languages are written and reviewed together):
  unreal/Lyudmilovsk/Content/Dialogue/town_barks.txt                 (when each line is said: structure, keys only)
  unreal/Lyudmilovsk/Content/Localization/barks/strings_<lang>.json  (the words; LyudText merges them in)
and checks the conditions against the game's data (activities, places, jobs; the holidays when festivals.json is there).
Edit the sources, never the built files; then run tools/barksim/run.sh (a simulated day of the town) and
tools/check_localization.py.

    python3 tools/barks/build_barks.py [--festivals path/to/festivals.json]
"""
import collections
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "lines")
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
FEST = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "PeopleData", "festivals.json")
if "--festivals" in sys.argv:
    FEST = sys.argv[sys.argv.index("--festivals") + 1]
if not os.path.exists(FEST):
    print("no %s: the festival = and phase = values aren't checked" % os.path.relpath(FEST, REPO))
    FEST = None

LANGS = ["ru", "en", "ja"]
COND_KEYS = ["to", "on", "near", "job", "age", "sex", "doing", "at", "pose", "manner", "met", "gather", "chrono", "need", "hours", "months",
             "weekday", "sun", "weather", "festival", "phase", "job2", "age2", "sex2", "doing2", "weight"]
EVENTS = {"rest_sit", "rest_carry", "rest_serve", "rest_bill", "rest_bye",
          # somebody noticed the player (PeopleManners.h ENoticeEvent, docs/town_manners.md)
          "hero_runs", "hero_close", "hero_lamp", "hero_sneaks", "hero_rides", "hero_fishes", "hero_swims", "hero_sits"}

sets = []          # [{name, conds: [(k, v)], say: [keys], talk: [[keys]], file}]
strings = collections.OrderedDict()   # key -> (ru, en, ja)
problems = []


def add_string(key, parts, where):
    if key in strings:
        problems.append("%s: key %s twice" % (where, key))
    if len(parts) != 3 or any(not p.strip() for p in parts):
        problems.append("%s: %s needs ru | en | ja" % (where, key))
    for p in parts:
        if "|" in p:
            problems.append("%s: %s has a stray |" % (where, key))
    if not re.match(r"^town\.[a-z0-9_]+\.[a-z0-9_]+$", key):
        problems.append("%s: bad key %s" % (where, key))
    strings[key] = tuple(p.strip() for p in parts)


for fn in sorted(os.listdir(SRC)):
    if not fn.endswith(".txt"):
        continue
    cur = None
    talk = None
    for n, raw in enumerate(open(os.path.join(SRC, fn), encoding="utf-8"), 1):
        line = raw.rstrip("\n")
        where = "%s:%d" % (fn, n)
        if not line.strip() or line.startswith("##"):
            continue
        if line.startswith("[") and line.endswith("]"):
            cur = {"name": line[1:-1], "conds": [], "say": [], "talk": [], "file": fn}
            if any(s["name"] == cur["name"] for s in sets):
                problems.append("%s: set [%s] twice" % (where, cur["name"]))
            sets.append(cur)
            talk = None
            continue
        if cur is None:
            problems.append("%s: text before a set" % where)
            continue
        if line.startswith("say "):
            key, *parts = [x for x in line[4:].split(" | ")]
            add_string(key.strip(), parts, where)
            cur["say"].append(key.strip())
            talk = None
        elif line.strip() == "talk":
            talk = []
            cur["talk"].append(talk)
        elif line.startswith("- "):
            if talk is None:
                problems.append("%s: talk line outside a talk" % where)
                continue
            key, *parts = [x for x in line[2:].split(" | ")]
            add_string(key.strip(), parts, where)
            talk.append(key.strip())
        elif "=" in line:
            k, v = [x.strip() for x in line.split("=", 1)]
            if k not in COND_KEYS:
                problems.append("%s: unknown condition %s" % (where, k))
            cur["conds"].append((k, v))
        else:
            problems.append("%s: can't read: %s" % (where, line))

# ---- what the game knows (for checking the condition values)
D = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content")
acts = {a["id"] for a in json.load(open(os.path.join(D, "PeopleData", "activities.json"), encoding="utf-8"))["activities"]}
kinds, ids = set(), set()
for fn in ["town_places.json", "people_places.json"]:
    p = os.path.join(D, "PeopleData", fn)
    if os.path.exists(p):
        for pl in json.load(open(p, encoding="utf-8")).get("places", []):
            kinds.add(pl.get("kind", ""))
            ids.add(pl.get("id", ""))
kinds |= {"fair_stall", "club", "restaurant", "cafe"}
jobs = set()
for fn in ["townsfolk.json", "people.json"]:
    p = os.path.join(D, "PeopleData", fn)
    for per in json.load(open(p, encoding="utf-8")).get("people", []):
        if per.get("occupation"):
            jobs.add(per["occupation"])
        for du in per.get("duties", []):
            if du.get("role"):
                jobs.add(du["role"])
jobs |= {"stallholder"}
fests, phases = set(), set()
if FEST:
    for f in json.load(open(FEST, encoding="utf-8"))["festivals"]:
        fests.add(f["id"])
        for ph in f.get("program", []):
            phases.add(ph["id"])

VALID = {
    "to": {"hero"}, "near": {"hero"}, "on": EVENTS,
    "sex": {"female", "male"}, "sex2": {"female", "male"},
    "doing": acts | {"work", "free", "outing", "date"}, "doing2": acts | {"work", "free", "outing", "date"},
    "at": kinds | ids | {"outdoors", "indoors"},
    "pose": {"stand", "walk", "talk", "look", "sit", "computer", "eat", "read", "work", "shovel"},
    "manner": {r.get("manner", r["id"]) for r in json.load(open(os.path.join(D, "PeopleData", "manners.json"), encoding="utf-8"))["manners"]},
    "met": {"new", "seen", "known"},
    "gather": {"mingle", "watch", "sit", "fair"},
    "chrono": {"lark", "owl", "night", "usual"},
    "need": {"hunger", "tired", "social", "fun", "nature", "mind", "sun"},
    "weekday": {"mon", "tue", "wed", "thu", "fri", "sat", "sun"},
    "sun": {"day", "low", "night", "cloudy"},
    "weather": {"rain", "snow", "fog", "cold", "warm", "clear", "wet", "aurora", "rainbow", "fireworks"},
    "job": jobs, "job2": jobs,
}
if FEST:
    VALID["festival"] = fests | {"any"}
    VALID["phase"] = phases
for s in sets:
    for k, v in s["conds"]:
        if k in ("hours", "months"):
            for r in v.split():
                if not re.match(r"^\d+(\.\d+)?(-\d+(\.\d+)?)?$", r):
                    problems.append("[%s] %s: bad range %s" % (s["name"], k, r))
            continue
        if k == "weight":
            try:
                float(v)
            except ValueError:
                problems.append("[%s] bad weight %s" % (s["name"], v))
            continue
        if k in ("age", "age2"):
            for t in v.split():
                t = t.lstrip("!")
                if t not in ("kid", "teen", "adult", "elder") and not re.match(r"^\d+-\d+$", t):
                    problems.append("[%s] %s: bad age %s" % (s["name"], k, t))
            continue
        if k in VALID:
            for t in v.split():
                if t.lstrip("!") not in VALID[k]:
                    problems.append("[%s] %s: unknown value %s" % (s["name"], k, t))
    if not s["say"] and not s["talk"]:
        problems.append("[%s] has no lines" % s["name"])
    if s["say"] and s["talk"]:
        problems.append("[%s] has both say and talk" % s["name"])
    for t in s["talk"]:
        if len(t) < 2:
            problems.append("[%s] a talk of one line" % s["name"])

# ---- write the structure file
HEADER = """# What the townsfolk say on their own (docs/town_talk.md): a greeting to the player walking past, a line said aloud by
# what they do and where, a short exchange between two of them, the restaurant's waiter and cook at the moments of the
# player's visit. Only structure: the words are in Content/Localization/barks/strings_<lang>.json (line.town.<scene>.<n>).
# Read by ULyudTownTalkSubsystem (Private/People/TownBarks.cpp, plain C++; tools/barksim tests it on the town's people).
# Re-read at every level start (Play). Proposals, not canon: change the words in the string tables, the moments here.
#
# [name]      a set of lines and when they are said. Every condition given must hold; the values of one condition are
#             alternatives (any of them), '!x' means not x.
# job =       occupation (townsfolk.json) or job role (a duty's role): pensioner, pupil, student, away, seller, waiter, cook, ...
# age =       kid (under 12), teen (12-17), adult (18-59), elder (60 and over), or years: 50-120
# sex =       female, male (for the Russian forms of the past tense: one form per key, never both in one string)
# doing =     activities.json ids (sit_bench, play_club, eat_out, gather_mingle, ...); work: on their duty at its place;
#             free: not on a duty; outing: at their household's weekend outing; date: at their
#             weekly date with a friend (docs/town_manners.md)
# at =        place kinds or place ids (bench, promenade, beach, club, restaurant, market, fair_stall, P-MARKET, ...);
#             outdoors, indoors: under the sky or a roof (alone it doesn't make a set more particular)
# pose =      stand, walk, talk, look, sit, computer, eat, read, work
# manner =    what their hands are doing now (manners.json: smoke, phone_call, umbrella, feed_birds, yawn, ...)
# met =       how well the speaker knows the player: new (never greeted him), seen (on 1-4 days), known (5 and more)
# gather =    at a holiday's gathering of this mode now: mingle, watch, sit, fair
# chrono =    lark, owl, night, usual                 need =  hunger, tired, social, fun, nature, mind, sun (over 0.75)
# hours =     5-9 (from-to; over midnight 22-4; several ranges: 6-9 17-20)    months = 10-4    weekday = mon ... sun
# sun =       day, low (sunrise or sunset), night, cloudy (overcast by day)
# weather =   rain, snow, fog, cold (under 2 C), warm (over 22 C), clear (few clouds), wet (ULyudWeatherSubsystem)
#             aurora, rainbow, fireworks: while people look up at one (UPeopleLifeSubsystem::GetSkyShow)
# festival =  festivals.json ids, or any: the festival on now (ULyudFestivalSubsystem::ActiveFestival); phase = its phase ids
# near = hero the player is within 10 m of the speaker (remarks about him)
# to = hero   a greeting: said to the player when he walks past close, once a day per person
# on =        said at an event instead of on its own: rest_sit (the player sat down at a table), rest_carry (the waiter takes
#             a tray from the kitchen pass; the cook says his line then), rest_serve, rest_bill, rest_bye (got up after eating)
# weight =    how often against the other sets that fit (1; every condition given already counts for a little more)
# say =       a line said alone: its key without 'line.'; a run of numbers in one: town.granny.1..8
# talk =      an exchange of two people, A | B | A ...; job2, age2, sex2, doing2: conditions on the second (B)
"""


def runs(keys):
    """town.x.1, town.x.2, town.x.3 -> town.x.1..3"""
    out = []
    i = 0
    while i < len(keys):
        m = re.match(r"^(.*)\.(\d+)$", keys[i])
        if not m:
            out.append(keys[i]); i += 1; continue
        base, a = m.group(1), int(m.group(2))
        j = i
        while j + 1 < len(keys):
            m2 = re.match(r"^(.*)\.(\d+)$", keys[j + 1])
            if not m2 or m2.group(1) != base or int(m2.group(2)) != a + (j + 1 - i):
                break
            j += 1
        out.append(keys[i] if j == i else "%s.%d..%d" % (base, a, a + j - i))
        i = j + 1
    return out


lines = [HEADER]
last_file = None
COMMENTS = {}
for fn in sorted(os.listdir(SRC)):
    if fn.endswith(".txt"):
        head = [l[2:].rstrip("\n") for l in open(os.path.join(SRC, fn), encoding="utf-8") if l.startswith("## ")]
        COMMENTS[fn] = [h for h in head if not set(h.strip()) <= {"-"}]
for s in sets:
    if s["file"] != last_file:
        last_file = s["file"]
        lines.append("")
        for c in COMMENTS.get(s["file"], []):
            lines.append("# " + c.strip())
    lines.append("")
    lines.append("[%s]" % s["name"])
    for k, v in s["conds"]:
        lines.append("%s = %s" % (k, v))
    for r in runs(s["say"]):
        lines.append("say = %s" % r)
    for t in s["talk"]:
        lines.append("talk = %s" % " | ".join(t))
out_txt = os.path.join(D, "Dialogue", "town_barks.txt")
open(out_txt, "w", encoding="utf-8").write("\n".join(lines).rstrip() + "\n")

os.makedirs(os.path.join(D, "Localization", "barks"), exist_ok=True)
for i, lang in enumerate(LANGS):
    table = collections.OrderedDict(("line." + k, v[i]) for k, v in strings.items())
    with open(os.path.join(D, "Localization", "barks", "strings_%s.json" % lang), "w", encoding="utf-8") as f:
        json.dump(table, f, ensure_ascii=False, indent=1)
        f.write("\n")

# ---- report
say_n = sum(len(s["say"]) for s in sets)
talk_n = sum(len(s["talk"]) for s in sets)
print("%d sets, %d keys (%d lines alone, %d exchanges of %d lines)" % (len(sets), len(strings), say_n, talk_n,
      sum(len(t) for s in sets for t in s["talk"])))
longest = sorted(strings.items(), key=lambda kv: -len(kv[1][0]))[:5]
for k, v in longest:
    print("  long ru %3d %s" % (len(v[0]), k))
longest_ja = sorted(strings.items(), key=lambda kv: -len(kv[1][2]))[:5]
for k, v in longest_ja:
    print("  long ja %3d %s" % (len(v[2]), k))
for p in problems:
    print("PROBLEM", p)
sys.exit(1 if problems else 0)
