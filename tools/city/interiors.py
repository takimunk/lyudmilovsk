#!/usr/bin/env python3
"""Town interiors: what every thing in every building does (docs/city_interiors.md, section 2).

Reads the plan (Content/City/city_plan.json), every building export (blender/export/city/<type>[__<plot>].json: its
`things`, or while a building declares none, things derived from its `spots`), the content tables
tools/city/content/*.json and the strings tools/city/content/strings/*.json, and writes:

    unreal/Lyudmilovsk/Content/City/interiors.json            things (world terms), acts, shops
    unreal/Lyudmilovsk/Content/Dev/teleports/interiors.json   developer-mode teleports into every enterable building
    unreal/Lyudmilovsk/Content/Localization/city/strings_<lang>.json
    unreal/Lyudmilovsk/Content/UI/quests.json                 the side quests of every content file's "quests" section
                                                              (quests.json first, then the others by name: quests_*.json,
                                                              x_*.json...) merged in (the other quests are kept byte for byte)

    python3 tools/city/interiors.py            write everything
    python3 tools/city/interiors.py --check    write nothing; fail on broken content; print a per-plot summary

Stdlib only, deterministic.
"""
import collections
import fnmatch
import glob
import json
import math
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
PLAN = os.path.join(CONTENT, "City", "city_plan.json")
EXPORTS = os.path.join(ROOT, "blender", "export", "city")
SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "content")
INVENTORY_CPP = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Source", "Lyudmilovsk", "Private", "LyudInventory.cpp")
OUT_THINGS = os.path.join(CONTENT, "City", "interiors.json")
OUT_TELEPORTS = os.path.join(CONTENT, "Dev", "teleports", "interiors.json")
OUT_STRINGS = os.path.join(CONTENT, "Localization", "city")
QUESTS = os.path.join(CONTENT, "UI", "quests.json")
MAP = os.path.join(CONTENT, "UI", "map.json")
FESTIVALS = os.path.join(CONTENT, "PeopleData", "festivals.json")
LANGS = ("ru", "en", "ja")

KINDS = ("seat", "bed", "shelf", "board", "counter", "machine", "play", "sport", "craft", "screen", "take", "door")
# a derived thing's look point over the spot's floor point (m) and whether it faces its user (object yaw = user yaw + 180)
KIND_LOOK = {"seat": (0.45, False), "bed": (0.5, False), "shelf": (1.3, True), "board": (1.5, True), "counter": (1.0, True),
             "machine": (1.1, True), "play": (0.8, True), "sport": (1.0, True), "craft": (0.9, True), "screen": (1.6, True),
             "take": (0.8, True), "door": (1.2, True)}
# spots that become things by themselves while a building declares none (the rest only when a content rule gives them a kind)
SPOT_KIND = {"seat": "seat", "bench": "seat", "counter": "counter", "table": "seat", "view": "board"}
# staff who sell over a counter, best first (a derived counter gets the plot's first of them)
SELLING = ("seller", "cashier", "barista", "waiter", "bartender", "bookseller", "fishmonger", "baker", "librarian", "receptionist",
           "clerk", "vendor", "renter", "repairman", "barber", "photographer", "nurse", "caretaker", "master", "cook",
           "lighthouse_keeper", "lifeguard", "guard")
# staff whose hours do not open a building to the public
SERVICE = ("cleaner", "janitor", "guard")
DAYS = ("mon", "tue", "wed", "thu", "fri", "sat", "sun")
PUBLIC_KINDS_SKIP = ("residential", "square", "park", "pool")
QUEST_EVENTS = ("do", "win", "visit", "get", "use", "have", "wear", "reach", "talk", "bird", "walk", "leave_computer")
ACT_FIELDS = ("prompt", "minutes", "price", "lines", "pose", "sound", "event", "target", "give", "daily", "needs_staff",
              "needs_item", "fade", "shop", "screen", "until", "title", "any_hour", "rotate", "collect", "closed", "game")
# a line's conditions (docs/city_interiors.md "Lines by the day and the hour"): hours [from, to), weekdays, a calendar date or
# range ("MM-DD" or ["MM-DD", "MM-DD"], wrapping the year), days since the game began (after N), every [n, r] game days
# festival ("any", "none", an id of PeopleData/festivals.json or a list of ids), weather (a word of WEATHER or a list: any of
# them), have ("set/piece"), count (["set", n]); "collect" ("set/piece") is no condition: the stamp the line gives when shown
LINE_CONDS = ("h", "d", "date", "after", "before", "once", "every", "festival", "weather", "have", "count", "collect")
WEATHER = ("rain", "snow", "fog", "cold", "warm", "clear", "wet", "dry")
# the content sections a content file may have
SECTIONS = ("acts", "shops", "rules", "defaults", "defaults_by_plot_kind", "quests", "collections", "closed_by", "regulars")
ROTATE = ("use", "daily")
# a seat's or bed's surface over its own floor (m): a bigger "height" was measured from the building's base
SEAT_HEIGHT = (0.05, 1.2)
SEAT_USUAL = {"seat": 0.45, "bed": 0.5}
MAIN_STRINGS = os.path.join(CONTENT, "Localization", "strings_ru.json")
# the townsfolk's names (npc.<key>.name, written by the town tools; merged by the game at load): a regular may be a resident
TOWN_STRINGS = os.path.join(CONTENT, "Localization", "town", "strings_ru.json")
PROP_ORDER = ("topic", "what", "service", "item", "to", "role", "shop")
MIN_GAP = 0.3
UNIT_TYPE = "promenade_unit"   # the promenade units' rooms (exports promenade_unit__<variant>.json)
FOOTPRINT_SLACK = 3.0

errors = []
warnings = []
RULES_USED = set()  # the id rules (<plot|type>/<id glob>) that matched a thing: --check fails on the others
FESTIVAL_IDS = set()  # PeopleData/festivals.json
STAMPS_USED = []  # (where, "set/piece") every collect of the content (acts, lines, things): checked against the collections
# a shop or a talk that needs a role the plot has no staff at all for (the ground-floor shops of a residential block): it
# runs without staff in these hours (a rule may give its own)
NO_STAFF_HOURS = ([9, 20], "mon-sat")


def err(msg):
    errors.append(msg)


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


# ------------------------------------------------------------------ geometry (tools/town/town_life.py conventions)

def ue_m(plan, p):
    """Map metres (X west, Y north, Z up) -> Unreal metres."""
    return [p[0], p[1] + plan["portal_face_y"] / 100.0, p[2]]


def local_to_world(front, yaw, local):
    """Building frame (x along the front, y into the plot, z up; metres from the plot's front point) -> Unreal metres."""
    a = math.radians(yaw)
    x, y, z = local
    return [front[0] + x * math.sin(a) - y * math.cos(a), front[1] - x * math.cos(a) - y * math.sin(a), front[2] + z]


def world_yaw(plot_yaw, local_yaw):
    """Kit yaw (0 = toward the street, -y; 90 = +x) -> Unreal degrees, in (-180, 180]."""
    w = (plot_yaw - local_yaw) % 360.0
    return round(w - 360.0 if w > 180.0 else w, 1)


def step(p, yaw, d):
    a = math.radians(yaw)
    return [p[0] + math.cos(a) * d, p[1] + math.sin(a) * d, p[2]]


def inside_poly(pt, poly):
    x, y = pt
    c = False
    j = len(poly) - 1
    for i in range(len(poly)):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / ((yj - yi) or 1e-9) + xi:
            c = not c
        j = i
    return c


def dist_poly(pt, poly):
    """0 inside, else the distance to the outline (m)."""
    if inside_poly(pt, poly):
        return 0.0
    best = 1e9
    for i in range(len(poly)):
        ax, ay = poly[i]
        bx, by = poly[(i + 1) % len(poly)]
        dx, dy = bx - ax, by - ay
        t = max(0.0, min(1.0, ((pt[0] - ax) * dx + (pt[1] - ay) * dy) / ((dx * dx + dy * dy) or 1e-9)))
        best = min(best, math.hypot(pt[0] - ax - t * dx, pt[1] - ay - t * dy))
    return best


# ------------------------------------------------------------------ hours

def day_index(d):
    return DAYS.index(d)


def days_set(spec):
    a, _, b = spec.partition("-")
    a, b = day_index(a), day_index(b or a)
    out, d = set(), a
    while True:
        out.add(d)
        if d == b:
            return out
        d = (d + 1) % 7


def days_spec(s):
    """A set of day indices -> 'mon-sun', a wrapping range 'tue-sun', or a list 'mon,wed'."""
    if len(s) == 7:
        return "mon-sun"
    for start in range(7):
        if start in s and (start - 1) % 7 not in s:
            run, d = [], start
            while d in s:
                run.append(d)
                d = (d + 1) % 7
            if len(run) == len(s):
                return DAYS[run[0]] if len(run) == 1 else "%s-%s" % (DAYS[run[0]], DAYS[run[-1]])
    return ",".join(DAYS[d] for d in sorted(s))


def role_hours(staff, role):
    for r, _n, hours, days in staff or []:
        if r == role:
            return list(hours), days
    return None


def plot_hours(staff):
    """The public hours of a place: the earliest start and the latest end of its public staff, the union of their days."""
    pub = [s for s in staff or [] if s[0] not in SERVICE] or list(staff or [])
    if not pub:
        return [0, 24], "mon-sun"
    starts, ends, days = [], [], set()
    for _r, _n, (h0, h1), d in pub:
        if h1 <= h0:  # a night shift or round the clock
            return [0, 24], days_spec(set().union(*[days_set(x[3]) for x in pub]))
        starts.append(h0)
        ends.append(h1)
        days |= days_set(d)
    return [min(starts), max(ends)], days_spec(days)


# ------------------------------------------------------------------ inputs

def item_ids():
    """Every item id of the item table (LyudInventory.cpp MakeDefs); fish_<species> come from LyudFishing and are not listed."""
    src = open(INVENTORY_CPP, encoding="utf-8").read()
    body = src[src.index("MakeDefs()"):src.index("const TArray<FLyudItemDef>& ULyudInventory::AllDefs")]
    ids = set(re.findall(r'\b(?:Good|FeedGood|Drink|Wear|Bait)\(TEXT\("(\w+)"\)', body))
    ids |= set(re.findall(r'\.Id = TEXT\("(\w+)"\)', body))
    m = re.search(r"for \(const TCHAR\* Find : \{(.*?)\}\)", body, re.S)
    if m:
        ids |= set(re.findall(r'TEXT\("(\w+)"\)', m.group(1)))
    return ids


MAIN = {}  # the hand-written table (Content/Localization/strings_ru.json): its keys may be used, never redefined here


def load_strings():
    MAIN.update(load(MAIN_STRINGS))
    table, where = {}, {}
    for path in sorted(glob.glob(os.path.join(SRC, "strings", "*.json"))):
        for k, v in load(path).items():
            if k.startswith("_"):
                continue
            if k in table:
                err("string %s is in both %s and %s" % (k, where[k], os.path.basename(path)))
            if k in MAIN:
                err("string %s (%s) is already in the hand-written table, which would win: drop it here" % (k, os.path.basename(path)))
            table[k], where[k] = v, os.path.basename(path)
            for l in LANGS:
                if not isinstance(v, dict) or not str(v.get(l, "")).strip():
                    err("string %s (%s): no %s" % (k, os.path.basename(path), l))
            if isinstance(v, dict):
                ph = sorted(re.findall(r"\{\d+\}", v.get("ru", "")))
                for l in LANGS[1:]:
                    if sorted(re.findall(r"\{\d+\}", v.get(l, ""))) != ph:
                        err("string %s: placeholders differ in %s" % (k, l))
    return table


CONTENT_EXTRA = {"collections": {}, "closed_by": {}, "quests": [], "regulars": {}}


def content_files():
    """Every content table: quests.json first (its quests keep their place), then the others by name (x_*.json last)."""
    paths = sorted(glob.glob(os.path.join(SRC, "*.json")))
    return sorted(paths, key=lambda p: (os.path.basename(p) != "quests.json", os.path.basename(p)))


def load_content():
    """Merge the content tables: acts, shops, rules, defaults, collections, closed_by, quests (a key defined twice is an error:
    one owner per thing). Any file may have any section, so a building's own file (x_<building>.json) brings its acts, rules,
    collections and side quests together."""
    acts, shops, rules, defaults, by_plot_kind = {}, {}, {}, {}, {}
    origin = {}
    quest_from = {}
    for path in content_files():
        name = os.path.basename(path)
        try:
            data = load(path)
        except ValueError as e:
            err("%s: not JSON (%s)" % (name, e))
            continue
        for k in data:
            if not k.startswith("_") and k not in SECTIONS:
                err("%s: unknown section %s (%s)" % (name, k, ", ".join(SECTIONS)))
        for q in data.get("quests") or []:
            if not isinstance(q, dict) or not q.get("id") or not isinstance(q.get("stages"), list) or not q["stages"] \
                    or not all(isinstance(st, dict) and st.get("id") and isinstance(st.get("objectives"), list) for st in q["stages"]):
                err("%s: a quest needs an id and stages, each with an id and objectives: %s" % (name, str(q)[:80]))
                continue
            if q.get("id") in quest_from:
                err("quest %s is in both %s and %s" % (q.get("id"), quest_from[q.get("id")], name))
                continue
            quest_from[q.get("id")] = name
            CONTENT_EXTRA["quests"].append(q)
        for section, into in (("acts", acts), ("shops", shops), ("rules", rules), ("defaults", defaults),
                              ("collections", CONTENT_EXTRA["collections"]), ("closed_by", CONTENT_EXTRA["closed_by"]),
                              ("regulars", CONTENT_EXTRA["regulars"])):
            for k, v in (data.get(section) or {}).items():
                if k.startswith("_doc"):
                    continue
                if (section, k) in origin:
                    err("%s %s is in both %s and %s" % (section[:-1], k, origin[(section, k)], name))
                origin[(section, k)] = name
                into[k] = v
        for pk, d in (data.get("defaults_by_plot_kind") or {}).items():
            by_plot_kind.setdefault(pk, {}).update(d)
    return acts, shops, rules, defaults, by_plot_kind


def building_export(btype, plot_id):
    for name in ("%s__%s.json" % (btype, plot_id.lower().replace("-", "_")), "%s.json" % btype):
        p = os.path.join(EXPORTS, name)
        if os.path.exists(p):
            return load(p)
    return None


# ------------------------------------------------------------------ things

def thing_props(t):
    if isinstance(t.get("props"), dict):
        return dict(t["props"])
    return {k: v for k, v in t.items() if k not in ("id", "kind", "pos", "yaw", "props")}


def id_rules(rules, plot, btype, tid):
    """The rules naming this thing, low to high priority: type globs, type exact, plot globs, plot exact."""
    out = []
    for scope in (btype, plot):
        exact, globs = None, []
        for k, v in rules.items():
            if "/" not in k:
                continue
            s, _, pat = k.partition("/")
            if s != scope:
                continue
            if pat == tid:
                exact = v
                RULES_USED.add(k)
            elif any(c in pat for c in "*?[") and fnmatch.fnmatchcase(tid, pat):
                globs.append((pat, v))
                RULES_USED.add(k)
        out += [v for _p, v in sorted(globs)] + ([exact] if exact else [])
    return out


def prop_rule(rules, kind, props):
    """The best kind+prop rule: the one with the most conditions, all met (`shelf:topic=birds`, `counter:service=bar,role=clerk`);
    a rule for any kind (`*:what=piano`) counts a little less than one for this kind. A value may be a glob, and `id` is the
    thing's id (`machine:what=jukebox,id=*radio*`: the hall radios declared as jukeboxes)."""
    best, best_n = None, 0
    for k in sorted(rules):
        v = rules[k]
        if "/" in k or ":" not in k:
            continue
        rk, _, cond = k.partition(":")
        if rk not in (kind, "*"):
            continue
        conds = [c.partition("=") for c in cond.split(",")]
        n = len(conds) - (0.5 if rk == "*" else 0.0)
        if all(fnmatch.fnmatchcase(str(props.get(a)), b) for a, _, b in conds) and n > best_n:
            best, best_n = v, n
    return best


def derive_things(export, rules, plot, btype):
    """Things from the spots while the building declares none (flagged auto): seats, counters, views; other spots only when
    a rule (<plot>/<spot id> or <type>/<spot id>) gives them a kind."""
    out = []
    for sp in export.get("spots", []):
        patch = {}
        for r in id_rules(rules, plot, btype, sp["id"]):
            patch.update(r)
        kind = patch.get("kind") or SPOT_KIND.get(sp["kind"])
        if not kind or patch.get("skip"):
            continue
        rise, faces_user = KIND_LOOK[kind]
        x, y, z = sp["pos"]
        uyaw = sp.get("yaw", 0.0) or 0.0
        out.append({"id": sp["id"], "kind": kind, "pos": [x, y, z + rise], "yaw": (uyaw + 180.0) % 360.0 if faces_user else uyaw,
                    "props": {"topic": "view"} if sp["kind"] == "view" and kind == "board" else {}, "spot": sp["kind"], "auto": 1})
    return out


def resolve(thing, rules, defaults, by_plot_kind, plot, acts):
    """kind, props, act and its own hours for a thing: id rules (plot over type), then kind+prop, then the kind's default."""
    kind, props = thing["kind"], dict(thing.get("props") or {})
    act, hours, days = None, None, None
    extra = {}
    for r in id_rules(rules, plot["id"], plot["building"], thing["id"]):
        kind = r.get("kind", kind)
        props.update(r.get("props") or {})
        act = r.get("act", act)
        hours, days = r.get("hours", hours), r.get("days", days)
        for k in ("collect", "closed"):
            if r.get(k):
                extra[k] = r[k]
    thing["extra"] = extra
    if not act and props.get("shop"):
        act = "buy_" + props["shop"]
    if not act:
        r = prop_rule(rules, kind, dict(props, id=thing["id"]))
        if r:
            act = r.get("act")
            hours, days = r.get("hours", hours), r.get("days", days)
    if not act and kind == "take" and props.get("item"):
        act = "take_" + props["item"] if "take_" + props["item"] in acts else "take_any"
    if not act:
        act = by_plot_kind.get(plot["kind"], {}).get(kind) or defaults.get(kind)
    if kind == "counter" and not props.get("role"):
        role = next((s for s in SELLING if any(r[0] == s for r in plot.get("staff") or [])), "")
        if role:
            props["role"] = role
    return kind, props, act, hours, days


# ------------------------------------------------------------------ acts

def line_key(line):
    """The string key of an act's line as written: a plain key, or {"k": key, <conditions>}."""
    return line["k"] if isinstance(line, dict) else line


def check_line_conds(where, cond):
    for k, v in cond.items():
        if k not in LINE_CONDS:
            err("%s: unknown line condition %s" % (where, k))
        elif k == "h" and not (isinstance(v, list) and len(v) == 2 and all(isinstance(x, (int, float)) and 0 <= x <= 24 for x in v)):
            err("%s: h must be [from, to] hours" % where)
        elif k == "d" and not (isinstance(v, str) and re.match(r"^(%s)(-(%s))?(,(%s)(-(%s))?)*$" % ((("|".join(DAYS)),) * 4), v)):
            err("%s: d must be a weekday list (mon-fri, sat,sun)" % where)
        elif k == "date":
            ds = v if isinstance(v, list) else [v]
            if len(ds) not in (1, 2) or not all(isinstance(x, str) and re.match(r"^(0?[1-9]|1[0-2])-(0?[1-9]|[12]\d|3[01])$", x) for x in ds):
                err("%s: date must be \"MM-DD\" or [\"MM-DD\", \"MM-DD\"]" % where)
        elif k in ("after", "before") and not (isinstance(v, int) and not isinstance(v, bool) and v >= 0):
            err("%s: %s must be whole days since the game began" % (where, k))
        elif k == "before" and isinstance(cond.get("after"), int) and v <= cond["after"]:
            err("%s: before %s is not after %s: the line would never show" % (where, v, cond["after"]))
        elif k == "once" and not isinstance(v, bool):
            err("%s: once must be true or false" % where)
        elif k == "every" and not (isinstance(v, list) and len(v) == 2 and all(isinstance(x, int) for x in v) and v[0] > 1 and 0 <= v[1] < v[0]):
            err("%s: every must be [n > 1, 0 <= r < n]" % where)
        elif k == "festival":
            fs = v if isinstance(v, list) else [v]
            if not fs or not all(isinstance(x, str) for x in fs) or (isinstance(v, list) and any(x in ("any", "none") for x in fs)):
                err("%s: festival must be \"any\", \"none\", a festival id or a list of ids" % where)
            for x in fs:
                if isinstance(x, str) and x not in ("any", "none") and FESTIVAL_IDS and x not in FESTIVAL_IDS:
                    err("%s: festival %s is not in PeopleData/festivals.json" % (where, x))
        elif k == "weather":
            ws = v if isinstance(v, list) else [v]
            if not ws or not all(w in WEATHER for w in ws):
                err("%s: weather must be one of %s or a list of them" % (where, ", ".join(WEATHER)))
        elif k in ("have", "collect") and not (isinstance(v, str) and re.match(r"^[\w.-]+/[\w.*{}-]+$", v)):
            err("%s: %s must be \"set/piece\"" % (where, k))
        elif k == "count" and not (isinstance(v, list) and len(v) == 2 and isinstance(v[0], str) and isinstance(v[1], int) and v[1] > 0):
            err("%s: count must be [\"set\", n > 0]" % where)
        elif k == "have":
            STAMPS_USED.append((where + " (have)", v))
        elif k == "count":
            STAMPS_USED.append((where + " (count)", v[0] + "/*"))


def expand_lines(spec, strings, where="lines"):
    """A prefix (all <prefix>.<n> strings, in order) or a list of keys, where an entry '<prefix>.*' stands for that prefix's
    strings (a board's generic notices followed by a building's own: ["city.look.club.*", "city.reg.cog.*"]). An entry may
    be {"k": key or "<prefix>.*", <conditions>}: each line it gives is written as {"k": key, <conditions>} (the game shows it
    only while they hold); plain keys stay plain strings, so old data reads the same."""
    if isinstance(spec, list):
        out = []
        for s in spec:
            if isinstance(s, dict):
                cond = {k: v for k, v in s.items() if k != "k"}
                check_line_conds(where, cond)
                keys = expand_lines(s["k"][:-2], strings) if s.get("k", "").endswith(".*") else [s.get("k", "")]
                for k in keys:
                    c = dict(cond)
                    if c.get("collect", "").endswith("/*"):
                        # one stamp per line: "films/*" on city.do.cinema.* gives films/1, films/2... (the key's last part)
                        c["collect"] = c["collect"][:-1] + k.rsplit(".", 1)[-1]
                    out.append(dict({"k": k}, **c) if c else k)
            else:
                out += expand_lines(s[:-2], strings) if s.endswith(".*") else [s]
        return out
    if not spec:
        return []
    pat = re.compile(re.escape(spec) + r"\.(\d+)$")
    found = sorted((int(m.group(1)), k) for k in strings for m in [pat.match(k)] if m)
    if not found:
        err("act lines %s.<n>: no such strings" % spec)
    return [k for _n, k in found]


# mini-games (docs/city_interiors.md "Mini-games"): an act's "game" plays instead of showing its lines
GAME_TYPES = ("aim", "rally", "duel")
GAME_FIELDS = ("type", "title", "keys", "rounds", "need", "speed", "zone", "shrink", "pace", "vs", "moves", "win", "lose", "draw",
               "collect", "prize", "daily", "sounds")
GAME_DEFAULTS = {"aim": {"rounds": 5, "need": 5, "speed": 0.6, "zone": 0.2},
                 "rally": {"rounds": 1, "need": 3, "speed": 0.9, "zone": 0.45},
                 "duel": {"rounds": 3, "need": 1, "speed": 0.6, "zone": 0.2}}
GAME_RANGES = {"rounds": (1, 9), "need": (1, 20), "speed": (0.1, 3.0), "shrink": (0.5, 1.0), "pace": (1.0, 1.5), "daily": (0, 20)}
GAME_ZONE = {"aim": (0.03, 0.9), "rally": (0.1, 1.5), "duel": (0.0, 1.5)}
# sounds that exist (ULyudSoundscapeSubsystem pools): a hit, a miss (none), the end of a won game
GAME_SOUNDS = {"aim": {"hit": "rest_clink", "miss": "", "end": "rest_bell"}, "rally": {"hit": "mouse_clicks", "miss": "", "end": "rest_bell"},
               "duel": {"hit": "mouse_clicks", "miss": "", "end": "rest_bell"}}
REG_STYLES = ("bold", "careful", "tricky", "steady")
REG_FIELDS = ("name", "style", "skill", "plays", "plots", "hello", "win", "lose")
TOWN = {}  # the townsfolk's names (Localization/town): keys a regular may use


def has_string(key, strings):
    return key in strings or key in MAIN or key in TOWN


def build_game(where, g, strings, items):
    """An act's "game" with every field (defaults by type), or None (an error is recorded)."""
    if not isinstance(g, dict):
        err("%s: game must be an object" % where)
        return None
    for k in g:
        if k not in GAME_FIELDS:
            err("%s: game: unknown field %s" % (where, k))
    t = g.get("type")
    if t not in GAME_TYPES:
        err("%s: game type must be one of %s" % (where, ", ".join(GAME_TYPES)))
        return None
    d = GAME_DEFAULTS[t]
    out = {"type": t, "title": g.get("title") or "city.game.title", "keys": g.get("keys") or "city.game.keys.%s" % t}
    for k in ("rounds", "need", "daily"):
        v = g.get(k, d.get(k, 0))
        if not isinstance(v, int) or isinstance(v, bool) or not GAME_RANGES[k][0] <= v <= GAME_RANGES[k][1]:
            err("%s: game %s must be a whole number %s..%s" % (where, k, GAME_RANGES[k][0], GAME_RANGES[k][1]))
            v = d.get(k, 0)
        out[k] = v
    for k, dv in (("speed", d["speed"]), ("zone", d["zone"]), ("shrink", 0.88), ("pace", 1.05)):
        v = g.get(k, dv)
        lo, hi = GAME_ZONE[t] if k == "zone" else GAME_RANGES[k]
        if not isinstance(v, (int, float)) or isinstance(v, bool) or not lo <= v <= hi:
            err("%s: game %s must be %s..%s" % (where, k, lo, hi))
            v = dv
        out[k] = float(v)
    out["vs"] = g.get("vs", "")
    if t == "duel" and not out["vs"]:
        err("%s: a duel needs \"vs\" (the regulars' game it is)" % where)
    moves = g.get("moves", [])
    if t == "duel":
        if not (isinstance(moves, list) and len(moves) == 3 and all(isinstance(m, dict) and m.get("k") and m.get("tell") for m in moves)):
            err("%s: a duel needs three moves {\"k\", \"power\", \"tell\"}" % where)
            moves = []
        for m in moves:
            for key in (m["k"], m["tell"]):
                if not has_string(key, strings):
                    err("%s: game move string %s is missing" % (where, key))
            if not isinstance(m.get("power", 1), int) or not 1 <= m.get("power", 1) <= 5:
                err("%s: a move's power is 1..5" % where)
        out["moves"] = [{"k": m["k"], "power": int(m.get("power", 1)), "tell": m["tell"]} for m in moves]
    elif moves:
        err("%s: only a duel has moves" % where)
    for res in ("win", "lose", "draw"):
        lines = expand_lines(g.get(res) or [], strings, "%s game %s" % (where, res))
        if not lines:
            err("%s: game needs %s lines" % (where, res))
        for l in lines:
            if not has_string(line_key(l), strings):
                err("%s: game %s line %s is missing" % (where, res, line_key(l)))
            if isinstance(l, dict) and l.get("collect"):
                STAMPS_USED.append(("%s game %s line %s" % (where, res, l["k"]), l["collect"]))
        out[res] = lines
    for key in (out["title"], out["keys"]):
        if not has_string(key, strings):
            err("%s: game string %s is missing" % (where, key))
    out["collect"] = g.get("collect", "")
    if out["collect"]:
        check_line_conds(where + " game", {"collect": out["collect"]})
        STAMPS_USED.append((where + " game", out["collect"]))
        if "{opponent}" in out["collect"] and not out["vs"]:
            err("%s: game collect names {opponent} but nobody plays against the hero (no vs)" % where)
    prize = []
    for p_ in g.get("prize", []):
        if not (isinstance(p_, list) and len(p_) == 2 and isinstance(p_[1], int) and p_[1] > 0):
            err("%s: game prize is [[\"item\", n]]" % where)
            continue
        if p_[0] not in items:
            err("%s: game prize %s is not an item" % (where, p_[0]))
        prize.append([p_[0], p_[1]])
    out["prize"] = prize
    snd = dict(GAME_SOUNDS[t])
    for k, v in (g.get("sounds") or {}).items():
        if k not in snd or not isinstance(v, str):
            err("%s: game sounds are {\"hit\", \"miss\", \"end\"}: ids" % where)
            continue
        snd[k] = v
    out["sounds"] = snd
    return out


def build_regulars(src, strings, plot_ids):
    """The regulars of the clubs who play against the hero (a real resident's npc.<key>.name, or a name of our own)."""
    out = collections.OrderedDict()
    for rid in sorted(src):
        if rid.startswith("_"):
            continue
        r = src[rid]
        if not re.match(r"^[\w.-]+$", rid):
            err("regular %s: the id is letters, digits, _ . - only" % rid)
        for k in r:
            if k not in REG_FIELDS:
                err("regular %s: unknown field %s" % (rid, k))
        if not r.get("name") or not has_string(r["name"], strings):
            err("regular %s: name string %s is missing (a resident's npc.<key>.name, or one of ours)" % (rid, r.get("name")))
        if r.get("style", "steady") not in REG_STYLES:
            err("regular %s: style must be one of %s" % (rid, ", ".join(REG_STYLES)))
        skill = r.get("skill", 0.5)
        if not isinstance(skill, (int, float)) or not 0 <= skill <= 1:
            err("regular %s: skill is 0..1" % rid)
            skill = 0.5
        plays = r.get("plays") or []
        if not plays:
            err("regular %s plays nothing" % rid)
        for w in plays:
            if not has_string("city.game.vs.%s" % w, strings):
                err("regular %s: no word city.game.vs.%s for the game %s" % (rid, w, w))
        for pl in r.get("plots") or []:
            if pl not in plot_ids:
                err("regular %s: plot %s is not in the plan" % (rid, pl))
        for k in ("hello", "win", "lose"):
            if r.get(k) and not has_string(r[k], strings):
                err("regular %s: %s string %s is missing" % (rid, k, r[k]))
        out[rid] = {"name": r.get("name", ""), "style": r.get("style", "steady"), "skill": float(skill), "plays": list(plays),
                    "plots": list(r.get("plots") or []), "hello": r.get("hello", ""), "win": r.get("win", ""), "lose": r.get("lose", "")}
    return out


def opponents_at(regs, vs, plot):
    """The regulars who may play vs at plot (the game's rule, CityThingsModel.h Opponents): those found there or anywhere,
    else everybody who plays it."""
    if not vs:
        return []
    here = [rid for rid, r in regs.items() if vs in r["plays"] and (not r["plots"] or plot in r["plots"])]
    return here or [rid for rid, r in regs.items() if vs in r["plays"]]


def build_acts(acts_src, shops, strings, items):
    """Every act with all its fields (inheriting from `like`), plus buy_<shop> for every shop and take_any."""
    src = dict(acts_src)
    for sid, s in shops.items():
        src.setdefault("buy_" + sid, {"like": "_buy", "shop": sid, "needs_staff": s.get("role", ""),
                                      "title": s.get("title", "city.shop.%s.title" % sid)})
    out = {}

    def full(aid, seen=()):
        if aid in seen:
            err("act %s: `like` loops" % aid)
            return {}
        a = src.get(aid)
        if a is None:
            return None
        base = full(a["like"], seen + (aid,)) if a.get("like") else {}
        if base is None:
            err("act %s: like unknown act %s" % (aid, a["like"]))
            base = {}
        merged = dict(base)
        merged.update({k: v for k, v in a.items() if k != "like"})
        return merged

    for aid in sorted(src):
        if aid.startswith("_"):
            continue
        a = full(aid)
        act = {"prompt": a.get("prompt", "city.act.use.prompt"), "minutes": int(a.get("minutes", 0)), "price": int(a.get("price", 0)),
               "lines": expand_lines(a.get("lines", []), strings, "act " + aid), "pose": a.get("pose", "stand"), "sound": a.get("sound", ""),
               "event": a.get("event", "do"), "target": a.get("target", aid), "give": [list(g) for g in a.get("give", [])],
               "daily": int(a.get("daily", 0)), "needs_staff": a.get("needs_staff", ""), "needs_item": a.get("needs_item", ""),
               "fade": bool(a.get("fade", False)), "shop": a.get("shop", ""), "screen": a.get("screen", ""),
               "until": int(a.get("until", -1))}
        if a.get("title"):
            act["title"] = a["title"]
        act["rotate"] = a.get("rotate", "use")
        if act["rotate"] not in ROTATE:
            err("act %s: rotate must be one of %s" % (aid, ", ".join(ROTATE)))
        act["any_hour"] = bool(a.get("any_hour", False))  # explicit: the game's default is true for sit/lie poses
        if a.get("collect"):
            act["collect"] = a["collect"]
            check_line_conds("act " + aid, {"collect": a["collect"]})
            STAMPS_USED.append(("act " + aid, a["collect"]))
        if a.get("closed"):
            act["closed"] = a["closed"]  # resolved per thing (closed_act)
        if a.get("game"):
            g = build_game("act " + aid, a["game"], strings, items)
            if g:
                act["game"] = g
        for l in act["lines"]:
            if isinstance(l, dict) and l.get("collect"):
                STAMPS_USED.append(("act %s line %s" % (aid, l["k"]), l["collect"]))
        for k in a:
            if k not in ACT_FIELDS and k not in ("like",):
                err("act %s: unknown field %s" % (aid, k))
        for key in [act["prompt"]] + [line_key(l) for l in act["lines"]] + ([act["title"]] if act.get("title") else []):
            if key not in strings and key not in MAIN:
                err("act %s: string %s is missing" % (aid, key))
        if "{" in (strings.get(act["prompt"]) or {}).get("ru", MAIN.get(act["prompt"], "")):
            err("act %s: a prompt takes no placeholder (the game adds the price with city.act.prompt_price)" % aid)
        for it, _n in act["give"]:
            if it not in items:
                err("act %s gives unknown item %s" % (aid, it))
        if act["needs_item"] and act["needs_item"] not in items:
            err("act %s needs unknown item %s" % (aid, act["needs_item"]))
        if act["shop"] and act["shop"] not in shops:
            err("act %s opens unknown shop %s" % (aid, act["shop"]))
        out[aid] = act
    return out


def build_shops(shops, strings, items):
    out = {}
    for sid in sorted(shops):
        s = shops[sid]
        if not s.get("role"):
            err("shop %s has no staff role" % sid)
        title = s.get("title", "city.shop.%s.title" % sid)
        if title not in strings:
            err("shop %s: title string %s is missing" % (sid, title))
        goods = []
        for g in s.get("goods", []):
            if g["item"] not in items:
                err("shop %s sells unknown item %s" % (sid, g["item"]))
            goods.append({"item": g["item"], "price": int(g["price"]), "count": int(g.get("count", 1))})
        if not goods:
            err("shop %s sells nothing" % sid)
        out[sid] = {"title": title, "role": s.get("role", ""), "goods": goods}
    return out


# ------------------------------------------------------------------ the plots

def places(plan):
    """Every plot with a building, and the promenade units (a room behind the sea wall's face, dressed as its variant; a unit
    whose variant is not built yet: one counter at its door)."""
    out = []
    for p in plan["plots"]:
        if not p.get("building"):
            continue
        out.append({"id": p["id"], "kind": p["kind"], "building": p["building"], "front": ue_m(plan, p["front"]), "yaw": p["yaw"],
                    "staff": p.get("staff") or [], "name_ru": p.get("name_ru", ""), "name_en": p.get("name_en", ""),
                    "export": building_export(p["building"], p["id"]), "w": p["w"], "d": p["d"]})
    for u in plan.get("promenade_units", []):
        w = u.get("w", 10.0)
        # the unit's room: promenade_unit__<variant> (blender/city/buildings/promenade_unit.py), its origin at the unit's
        # "front" (the bay's middle on the wall's face); rules name its things as promenade_unit/<thing id>
        export = building_export(UNIT_TYPE, u["variant"]) if u.get("variant") and u.get("front") else None
        if export:
            out.append({"id": u["id"], "kind": u["kind"], "building": UNIT_TYPE, "front": ue_m(plan, u["front"]), "yaw": u["yaw"],
                        "staff": u.get("staff") or [], "name_ru": u.get("name_ru", ""), "name_en": u.get("name_en", ""),
                        "export": export, "w": w, "d": 6.5, "unit": 1, "variant": u["variant"]})
            continue
        # no room built yet: one counter at its door
        fp = [[-w / 2, 0.0], [w / 2, 0.0], [w / 2, 6.0], [-w / 2, 6.0]]
        export = {"footprint": fp, "entrances": [{"id": "door", "pos": [0.0, 0.0, 0.0], "kind": "street"}],
                  "spots": [{"id": "counter", "kind": "counter", "pos": [0.0, 0.6, 0.0], "yaw": 180.0}]}
        out.append({"id": u["id"], "kind": u["kind"], "building": "unit_" + u["kind"], "front": ue_m(plan, u["door"]), "yaw": u["yaw"],
                    "staff": u.get("staff") or [], "name_ru": u.get("name_ru", ""), "name_en": u.get("name_en", ""),
                    "export": export, "w": w, "d": 6.0, "unit": 1})
    return out


def staff_variant(pl, props, kind, act, acts, tid):
    """An act whose needs_staff role does not work at this plot: the same act for the role that does (the counter's own
    `role`, else for a counter or a shop the plot's first selling role), or, at a plot with no staff at all, without staff
    (`<act>@open`, NO_STAFF_HOURS). Its `target` stays the original act's, so the quests still see it. None: no fix (an error)."""
    roles = [r[0] for r in pl["staff"]]
    if props.get("role") in roles and props["role"] not in SERVICE:
        alt = props["role"]
    elif (kind == "counter" or acts[act]["shop"]) and any(s in roles for s in SELLING if s not in SERVICE):
        alt = next(s for s in SELLING if s in roles and s not in SERVICE)
    elif not roles:
        alt = ""
    else:
        return None
    vid = "%s@%s" % (act, alt or "open")
    if vid not in acts:
        acts[vid] = dict(acts[act], needs_staff=alt)
    return vid


def normalise_height(tid, kind, props):
    """A seat's or bed's `height` is its surface over its own floor (m). Some building modules passed the absolute z (6..33 m
    over the building's base, equal to the thing's own z): the game would seat the hero metres under the floor and stand him
    up there. Such a height becomes the usual one (with a warning); anything still outside SEAT_HEIGHT is an error."""
    if "height" not in props:
        return
    try:
        h = float(props["height"])
    except (TypeError, ValueError):
        err("%s: height %r is not a number" % (tid, props["height"]))
        return
    if h > SEAT_HEIGHT[1]:
        usual = SEAT_USUAL.get(kind, 0.45)
        warnings.append("%s: height %.2f m is over the building's base, not the floor: %.2f used (fix the module's B.thing)" % (tid, h, usual))
        props["height"] = usual
        h = usual
    if not SEAT_HEIGHT[0] <= h <= SEAT_HEIGHT[1]:
        err("%s: height %.2f m is outside %.2f..%.1f (the seat's surface over its own floor)" % (tid, h, SEAT_HEIGHT[0], SEAT_HEIGHT[1]))


def closed_act(value, acts, strings, where):
    """A thing's "closed": an act id, or a line key (or "<prefix>.*"): an act of its own that only shows the words."""
    if value in acts:
        return value
    keys = expand_lines([value], strings, where + " closed")
    missing = [k for k in keys if k not in strings and k not in MAIN]
    if missing or not keys:
        err("%s: closed %s is neither an act nor a string" % (where, value))
        return None
    aid = "_closed:" + value
    if aid not in acts:
        acts[aid] = {"prompt": "city.act.look.prompt", "minutes": 0, "price": 0, "lines": keys, "pose": "stand", "sound": "",
                     "event": "look", "target": "closed", "give": [], "daily": 0, "needs_staff": "", "needs_item": "", "fade": False,
                     "shop": "", "screen": "", "until": -1, "rotate": "use", "any_hour": True}
    return aid


def build_things(plan, rules, defaults, by_plot_kind, acts, strings=None, closed_by=None):
    things, summary, skipped = [], collections.OrderedDict(), collections.Counter()
    strings = strings or {}
    closed_by = closed_by or {}
    for pl in places(plan):
        ex = pl["export"]
        if not ex:
            continue
        declared = ex.get("things") or []
        if declared:
            raw = [{"id": t["id"], "kind": t["kind"], "pos": list(t["pos"]), "yaw": t.get("yaw", 0.0) or 0.0, "props": thing_props(t)} for t in declared]
        else:
            raw = derive_things(ex, rules, pl["id"], pl["building"])
        fp = ex.get("footprint") or []
        kept_local = []
        seen = set()
        for t in raw:
            tid = "%s/%s" % (pl["id"], t["id"])
            auto = t.get("auto", 0)
            if t["kind"] not in KINDS:
                err("%s: unknown kind %s" % (tid, t["kind"]))
                continue
            if tid in seen:
                err("%s: duplicate thing id" % tid)
                continue
            seen.add(tid)
            if fp and dist_poly(t["pos"][:2], fp) > FOOTPRINT_SLACK:
                if auto:
                    skipped["outside"] += 1
                    continue
                err("%s: outside the building's footprint by %.1f m" % (tid, dist_poly(t["pos"][:2], fp)))
            near = next((o for o in kept_local if math.dist(o["pos"], t["pos"]) < MIN_GAP), None)
            if near:
                if auto:
                    skipped["crowded"] += 1
                    continue
                err("%s: %.2f m from %s (min %.1f)" % (tid, math.dist(near["pos"], t["pos"]), near["id"], MIN_GAP))
            kind, props, act, hours, days = resolve(t, rules, defaults, by_plot_kind, pl, acts)
            extra = t.pop("extra", {})
            if kind not in KINDS:
                err("%s: a rule gives unknown kind %s" % (tid, kind))
                continue
            if act not in acts:
                err("%s: unknown act %s" % (tid, act))
                continue
            if kind == "seat" and acts[act]["pose"] == "stand":
                # read in a reading pod, a check-up in a chair: the same act seated, still reported as the original
                seated = act + "_seated"
                if seated not in acts:
                    acts[seated] = dict(acts[act], pose="sit")
                act = seated
            a = acts[act]
            if a["needs_staff"] not in ("", "*") and not role_hours(pl["staff"], a["needs_staff"]):
                vid = staff_variant(pl, props, kind, act, acts, tid)
                if vid:
                    act, a = vid, acts[vid]
                    skipped["staff variant"] += 1
                    if not a["needs_staff"] and hours is None:
                        hours, days = NO_STAFF_HOURS
            if hours is None:
                rh = role_hours(pl["staff"], a["needs_staff"]) if a["needs_staff"] else None
                if a["needs_staff"] and not rh:
                    err("%s: act %s needs staff %s, who do not work at %s" % (tid, act, a["needs_staff"], pl["id"]))
                hours, days = rh or plot_hours(pl["staff"])
            elif days is None:
                days = plot_hours(pl["staff"])[1]
            if kind in SEAT_USUAL or "height" in props:
                normalise_height(tid, kind, props)
            if "switch" in props and not (props["switch"] in ("room", "hall") or re.match(r"^\d+(\.\d+)?$", str(props["switch"]))
                                          and 1 <= float(props["switch"]) <= 30):
                err("%s: switch must be room, hall or metres 1..30 (the lamps it turns on and off)" % tid)
            if kind == "counter" and a["shop"] and a["needs_staff"] and props.get("role") != a["needs_staff"]:
                props["role"] = a["needs_staff"]
            w = local_to_world(pl["front"], pl["yaw"], t["pos"])
            rec = {"id": tid, "plot": pl["id"], "kind": kind, "ue": [round(w[0] * 100, 1), round(w[1] * 100, 1), round(w[2] * 100, 1)],
                   "yaw": world_yaw(pl["yaw"], t["yaw"]), "props": {k: props[k] for k in sorted(props)}, "act": act,
                   "hours": list(hours), "days": days}
            if auto:
                rec["auto"] = 1
            if extra.get("collect"):
                rec["collect"] = extra["collect"].replace("{plot}", pl["id"])
                STAMPS_USED.append((tid, rec["collect"]))
            # what it says while its plot is shut: the thing's rule, its act's "closed", the closed_by of its plot, type or plot kind
            closed = extra.get("closed") or a.get("closed")
            for scope in (pl["id"], pl["building"], pl["kind"]):
                cb = closed_by.get(scope) or {}
                closed = closed or cb.get(kind) or cb.get("*")
            if closed and list(hours) != [0, 24]:
                rec["closed"] = closed_act(closed, acts, strings, tid)
                if not rec["closed"]:
                    del rec["closed"]
            things.append(rec)
            kept_local.append({"id": t["id"], "pos": t["pos"], "z": t["pos"][2]})
            s = summary.setdefault(pl["id"], {"auto": 0, "declared": 0, "kinds": collections.Counter(), "acts": set(), "lines": 0})
            s["auto" if auto else "declared"] += 1
            s["kinds"][kind] += 1
            if act not in s["acts"]:
                s["acts"].add(act)
                s["lines"] += len(a["lines"])
    return things, summary, skipped


# ------------------------------------------------------------------ collections

def build_collections(src, things, acts, strings, regs=None):
    """The collections (docs/city_interiors.md "Collections"): {"<set>": {"title": key, "pieces": [...]}}. Pieces omitted: every
    piece something in town gives. Every stamp named anywhere must be of a declared set (and a declared piece), and every
    declared piece must be given by some thing in town (or the set could never be complete)."""
    given = collections.defaultdict(set)
    seen_acts = set()
    for t in things:
        if t.get("collect"):
            given[t["collect"].partition("/")[0]].add(t["collect"].partition("/")[2])
        g = acts[t["act"]].get("game") or {}
        if g.get("collect"):
            # a game's win: "chess_wins/{opponent}" is every regular who may sit down at this thing
            gset, _, gpiece = g["collect"].partition("/")
            if "{opponent}" in gpiece:
                given[gset].update(gpiece.replace("{opponent}", o) for o in opponents_at(regs or {}, g["vs"], t["plot"]))
            else:
                given[gset].add(gpiece)
        for res in ("win", "lose", "draw"):
            for l in g.get(res, []):
                if isinstance(l, dict) and l.get("collect"):
                    given[l["collect"].partition("/")[0]].add(l["collect"].partition("/")[2])
        if t["act"] in seen_acts:
            continue
        seen_acts.add(t["act"])
        a = acts[t["act"]]
        for c in [a.get("collect", "")] + [l.get("collect", "") for l in a["lines"] if isinstance(l, dict)]:
            if c.endswith("/{line}"):
                # every line it shows is a piece of its own (the films of a programme, the records of a jukebox)
                given[c.partition("/")[0]].update(line_key(l) for l in a["lines"])
            elif c:
                given[c.partition("/")[0]].add(c.partition("/")[2])
    out = collections.OrderedDict()
    for sid in sorted(src):
        if sid.startswith("_"):
            continue
        c = src[sid]
        if not re.match(r"^[\w.-]+$", sid):
            err("collection %s: the id is letters, digits, _ . - only" % sid)
        title = c.get("title", "city.coll.%s.title" % sid)
        if title not in strings and title not in MAIN:
            err("collection %s: title string %s is missing" % (sid, title))
        for k in c:
            if k not in ("title", "pieces"):
                err("collection %s: unknown field %s" % (sid, k))
        pieces = [str(p) for p in c["pieces"]] if c.get("pieces") else sorted(given.get(sid, ()), key=lambda p: (len(p), p))
        if len(set(pieces)) != len(pieces):
            err("collection %s: a piece twice" % sid)
        for p in pieces:
            if p not in given.get(sid, ()):
                err("collection %s: nothing in town gives piece %s" % (sid, p))
        if not pieces:
            err("collection %s: no pieces (nothing in town gives one)" % sid)
        out[sid] = {"title": title, "pieces": pieces}
    for where, st in STAMPS_USED:
        sid, _, piece = st.partition("/")
        if sid not in out:
            err("%s: stamp %s of an undeclared collection (add it to a \"collections\" section)" % (where, st))
        elif piece not in ("*", "{line}") and "{plot}" not in piece and "{opponent}" not in piece and piece not in out[sid]["pieces"]:
            err("%s: stamp %s is not a piece of %s" % (where, st, sid))
    return out


# ------------------------------------------------------------------ teleports

def storey_name(n, lang):
    return {"ru": "%d-й этаж" % n, "en": "floor %d" % n, "ja": "%d階" % n}[lang]


def build_teleports(plan, things, strings, gen_strings):
    """One teleport per enterable building (inside its street door, facing in) and one per upper storey that has things."""
    tps = []
    by_plot = collections.defaultdict(list)
    for t in things:
        by_plot[t["plot"]].append(t)
    pf = plan["portal_face_y"] / 100.0
    for pl in places(plan):
        if (pl.get("unit") and not pl.get("variant")) or pl["kind"] in PUBLIC_KINDS_SKIP or not pl["export"]:
            continue
        ex = pl["export"]
        key = pl["id"].lower().replace("-", "_")
        name_key = "city.plot.%s.name" % key
        if pl.get("variant"):
            # a promenade unit: "<its trade> W1-U07" (the trades' names are in strings/promenade.json)
            tkey = "city.unit.%s.name" % pl["variant"]
            if tkey not in strings:
                err("teleport %s: no trade name %s" % (pl["id"], tkey))
                continue
            strings = dict(strings)
            strings[name_key] = {l: "%s %s" % (strings[tkey][l], pl["id"]) for l in LANGS}
        if name_key not in strings:
            err("teleport %s: no place name %s" % (pl["id"], name_key))
            continue
        ent = next((e for e in ex.get("entrances", []) if e.get("kind") == "street" and e.get("id") == "main"), None) or \
            next((e for e in ex.get("entrances", []) if e.get("kind") == "street"), None)
        if ent:
            local = [ent["pos"][0], ent["pos"][1] + 1.5, ent["pos"][2]]
        else:  # an open hall or a kiosk: in front of it, facing it
            local = [0.0, -2.0, 0.0]
        w = local_to_world(pl["front"], pl["yaw"], local)
        tid = "city." + key
        tps.append({"id": tid, "group": "interiors", "name": "dev.tp." + tid, "x": round(w[0], 2), "y": round(w[1] - pf, 2),
                    "z": round(w[2] * 100, 1), "yaw": world_yaw(pl["yaw"], 180.0)})
        gen_strings["dev.tp." + tid] = {l: strings[name_key][l] for l in LANGS}
        ground = pl["front"][2] * 100
        storeys = collections.OrderedDict()
        for t in sorted(by_plot.get(pl["id"], []), key=lambda t: (t["ue"][2], t["id"])):
            floor = t["ue"][2] - KIND_LOOK[t["kind"]][0] * 100
            if floor - ground > 250:
                storeys.setdefault(int(round((floor - ground) / 100.0)), (t, floor))
        merged = []
        for h, (t, floor) in storeys.items():
            if merged and h - merged[-1][0] < 3:
                continue
            merged.append((h, t, floor))
        for i, (h, t, floor) in enumerate(merged):
            n = i + 2
            p = step([t["ue"][0] / 100.0, t["ue"][1] / 100.0, floor / 100.0], t["yaw"], 1.2 if t["kind"] != "seat" else 0.0)
            if t["kind"] == "seat":
                p = step(p, t["yaw"], 0.9)
            sid = "%s.%d" % (tid, n)
            tps.append({"id": sid, "group": "interiors", "name": "dev.tp." + sid, "x": round(p[0], 2), "y": round(p[1] - pf, 2),
                        "z": round(floor, 1), "yaw": round(((t["yaw"] + 180.0) % 360.0) - (360.0 if (t["yaw"] + 180.0) % 360.0 > 180 else 0), 1)})
            gen_strings["dev.tp." + sid] = {l: "%s: %s" % (strings[name_key][l], storey_name(n, l)) if l != "ja"
                                            else "%s・%s" % (strings[name_key][l], storey_name(n, l)) for l in LANGS}
    return tps


# ------------------------------------------------------------------ quests

QUEST_MARK = '"from": "tools/city/interiors.py"'


def quest_line(q):
    body = {"id": q["id"], "kind": q.get("kind", "side")}
    for k in ("auto_start", "start_on"):
        if k in q:
            body[k] = q[k]
    body["stages"] = q["stages"]
    s = json.dumps(body, ensure_ascii=False, separators=(", ", ": "))
    return "  " + s[:-1] + ", " + QUEST_MARK + "}"


def check_quests(quests, acts, strings, items, plot_ids, map_places, existing_ids):
    keys = []
    for q in quests:
        qid = q["id"]
        if qid in existing_ids:
            err("quest %s already exists in quests.json (not ours)" % qid)
        keys += ["quest.%s.title" % qid, "quest.%s.desc" % qid, "quest.%s.done" % qid]
        evs = [q["start_on"]] if q.get("start_on") else []
        for st in q["stages"]:
            keys.append("quest.%s.%s.log" % (qid, st["id"]))
            for o in st["objectives"]:
                keys.append("quest.%s.%s.obj" % (qid, o["id"]))
                evs.append(o)
                if o.get("place") and o["place"] not in map_places:
                    err("quest %s: objective %s: place %s is not in map.json" % (qid, o["id"], o["place"]))
            rw = st.get("reward") or {}
            for it in (rw.get("items") or {}):
                if it not in items:
                    err("quest %s rewards unknown item %s" % (qid, it))
        for e in evs:
            for ev in e["event"].split("|"):
                tgt = e.get("target", "")
                if ev not in QUEST_EVENTS:
                    err("quest %s: unknown event %s" % (qid, ev))
                pref = tgt[:-1] if tgt.endswith("*") else None
                if ev in ("do", "win") and not (any(a.startswith(pref) for a in acts) if pref is not None else tgt in acts):
                    err("quest %s: %s %s: no such act" % (qid, ev, tgt))
                if ev == "visit" and tgt not in plot_ids:
                    err("quest %s: visit %s: no such plot" % (qid, tgt))
                if ev in ("get", "use", "have", "wear") and not (any(i.startswith(pref) for i in items) if pref is not None else tgt in items):
                    err("quest %s: %s %s: unknown item" % (qid, ev, tgt))
                if ev == "reach" and tgt not in map_places:
                    err("quest %s: reach %s: not a map place" % (qid, tgt))
    for k in keys:
        if k not in strings and k not in MAIN:
            err("quest string %s is missing" % k)


def quest_dev(quests, things, acts, shops, plan, strings, gen_strings):
    """Developer mode (docs/dev_mode.md): a "dev" block for every stage of our quests, so a jump to the stage lands where it is
    played and when it can be: a teleport in front of a thing that makes the stage's first objective ("city.q.<quest>.<n>"),
    the clock an hour after that thing opens, a weekday it is open (lyud.City.Weekday), the roubles its acts cost
    (Inventory.Earn) and the items a later step only checks (`have`, `use`). A stage's own "dev" in the content wins, key by
    key. Returns the teleports to add."""
    pf = plan["portal_face_y"] / 100.0
    tps = []
    things_of_plot = collections.defaultdict(list)
    for t in things:
        things_of_plot[t["plot"]].append(t)
    staff_of = {pl["id"]: {r[0] for r in pl["staff"]} for pl in places(plan)}

    def staffed(t):
        a = acts[t["act"]]
        return a["needs_staff"] in ("", "*") or a["needs_staff"] in staff_of.get(t["plot"], ())

    def makers(o, home):
        ev, tgt = o["event"].split("|")[0], o.get("target", "")
        match = (lambda x: x.startswith(tgt[:-1])) if tgt.endswith("*") else (lambda x: x == tgt)
        if ev == "do":
            by = [t for t in things if match(acts[t["act"]]["target"]) and staffed(t)]
        elif ev == "visit":
            by = list(things_of_plot.get(tgt, []))
        elif ev in ("have", "get"):
            by = [t for t in things if staffed(t) and (any(g[0] == tgt for g in acts[t["act"]]["give"])
                                                         or any(g["item"] == tgt for g in shops.get(acts[t["act"]]["shop"], {}).get("goods", [])))]
        else:
            by = []
        # the quest's own building first, then things without hours, then by id
        return sorted(by, key=lambda t: (t["plot"] != home, t["kind"] == "seat", t["id"]))

    for q in quests:
        home = (q.get("start_on") or {}).get("target", "")
        title = "quest.%s.title" % q["id"]
        last_tp = None
        for n, st in enumerate(q["stages"], 1):
            dev = {}
            give, earn, hours, days = {}, 0, None, None
            first = None
            for o in st["objectives"]:
                ev, tgt = o["event"].split("|")[0], o.get("target", "")
                if ev in ("have", "use") and not tgt.endswith("*") and (ev == "use" or n > 1):
                    # a check of the pocket (or using a thing from it) in a later stage: what an earlier stage would have got
                    give[tgt] = max(give.get(tgt, 0), int(o.get("need", 1)))
                    continue
                by = makers(o, home)
                if not by:
                    continue
                t = by[0]
                first = first or t
                a = acts[t["act"]]
                earn += a["price"] * int(o.get("need", 1))
                if ev in ("have", "get"):
                    earn += max([g["price"] for g in shops.get(a["shop"], {}).get("goods", []) if g["item"] == tgt] or [0]) * int(o.get("need", 1))
                if a["needs_item"]:
                    give[a["needs_item"]] = max(give.get(a["needs_item"], 0), 1)
                if not a["any_hour"] and t["hours"] != [0, 24]:
                    hours = hours or t["hours"]
                    days = days or t["days"]
            if first:
                kind = first["kind"]
                floor = first["ue"][2] - KIND_LOOK[kind][0] * 100
                if "height" in first["props"]:
                    floor = first["ue"][2] - float(first["props"]["height"]) * 100
                p = step([first["ue"][0] / 100.0, first["ue"][1] / 100.0, floor / 100.0], first["yaw"], 1.2 if kind not in ("seat", "bed") else 0.9)
                tid = "city.q.%s.%d" % (q["id"], n)
                yaw = (first["yaw"] + 180.0) % 360.0
                tps.append({"id": tid, "group": "interiors", "name": "dev.tp." + tid, "x": round(p[0], 2), "y": round(p[1] - pf, 2),
                            "z": round(floor, 1), "yaw": round(yaw - 360.0 if yaw > 180.0 else yaw, 1)})
                if title in strings:
                    gen_strings["dev.tp." + tid] = {l: "%s: %d" % (strings[title][l], n) for l in LANGS}
                else:
                    gen_strings["dev.tp." + tid] = {l: "%s: %d" % (q["id"], n) for l in LANGS}
                last_tp = tid
            if last_tp:
                dev["tp"] = last_tp
            if give:
                dev["give"] = give
            exec_ = []
            if hours:
                h = int(hours[0]) + 1 if hours[1] - hours[0] > 1 or hours[1] < hours[0] else int(hours[0])
                dev["time"] = "%02d:00" % (h % 24)
                if days and days != "mon-sun":
                    open_days = days_of(days)
                    exec_.append("lyud.City.Weekday %s" % DAYS[2 if 2 in open_days else min(open_days)])
            if earn:
                exec_.append("Inventory.Earn %d" % earn)
            if exec_:
                dev["exec"] = exec_
            own = st.get("dev") or {}
            dev.update(own)
            if dev:
                st["dev"] = dev
    return tps


def days_of(spec):
    """'mon-sun', 'tue-sun', 'fri-mon', 'mon,wed' -> the set of day indices (0 = Monday)."""
    out = set()
    for part in spec.split(","):
        a, _, b = part.partition("-")
        if a not in DAYS or (b and b not in DAYS):
            continue
        i, j = DAYS.index(a), DAYS.index(b or a)
        while True:
            out.add(i)
            if i == j:
                break
            i = (i + 1) % 7
    return out or set(range(7))


def merge_quests(text, quests):
    """Drop our previous lines (marked), then append ours before the closing bracket of "quests"; the rest stays as it was."""
    lines = text.split("\n")
    lines = [l for l in lines if QUEST_MARK not in l]
    end = max(i for i, l in enumerate(lines) if l.strip() == "]")
    prev = end - 1
    while not lines[prev].strip():
        prev -= 1
    if lines[prev].rstrip().endswith(","):
        lines[prev] = lines[prev].rstrip()[:-1]
    if quests:
        lines[prev] = lines[prev] + ","
        ours = [quest_line(q) + ("," if i < len(quests) - 1 else "") for i, q in enumerate(quests)]
        lines = lines[:prev + 1] + ours + lines[prev + 1:]
    return "\n".join(lines)


# ------------------------------------------------------------------ output

def dump_things(data):
    """Compact but readable: one thing, act, shop or teleport per line."""
    out = ["{"]
    sections = list(data.items())
    for si, (name, val) in enumerate(sections):
        comma = "," if si < len(sections) - 1 else ""
        if isinstance(val, list):
            out.append(' "%s": [' % name)
            out += ["  " + json.dumps(v, ensure_ascii=False) + ("," if i < len(val) - 1 else "") for i, v in enumerate(val)]
            out.append(" ]" + comma)
        elif isinstance(val, dict):
            out.append(' "%s": {' % name)
            items = list(val.items())
            out += ["  %s: %s%s" % (json.dumps(k), json.dumps(v, ensure_ascii=False), "," if i < len(items) - 1 else "") for i, (k, v) in enumerate(items)]
            out.append(" }" + comma)
        else:
            out.append(" %s: %s%s" % (json.dumps(name), json.dumps(val, ensure_ascii=False), comma))
    out.append("}")
    return "\n".join(out) + "\n"


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    old = open(path, encoding="utf-8").read() if os.path.exists(path) else None
    if old != text:
        with open(path, "w", encoding="utf-8") as f:
            f.write(text)
        print("wrote", os.path.relpath(path, ROOT))
    else:
        print("same ", os.path.relpath(path, ROOT))


def main():
    check = "--check" in sys.argv
    plan = load(PLAN)
    FESTIVAL_IDS.update(f["id"] for f in load(FESTIVALS).get("festivals", []) if f.get("id"))
    items = item_ids()
    strings = load_strings()
    if os.path.exists(TOWN_STRINGS):
        TOWN.update(load(TOWN_STRINGS))
    acts_src, shops_src, rules, defaults, by_plot_kind = load_content()
    for k in defaults:
        if k not in KINDS:
            err("default for unknown kind %s" % k)
    for k in KINDS:
        if k not in defaults:
            err("no default act for kind %s" % k)
    shops = build_shops(shops_src, strings, items)
    acts = build_acts(acts_src, shops_src, strings, items)
    for k, a in list(defaults.items()) + [(k, a) for d in by_plot_kind.values() for k, a in d.items()]:
        if a not in acts:
            err("default act %s for %s is unknown" % (a, k))
    for k, r in rules.items():
        if r.get("act") and r["act"] not in acts:
            err("rule %s: unknown act %s" % (k, r["act"]))
        if r.get("kind") and r["kind"] not in KINDS:
            err("rule %s: unknown kind %s" % (k, r["kind"]))
    things, summary, skipped = build_things(plan, rules, defaults, by_plot_kind, acts, strings, CONTENT_EXTRA["closed_by"])
    all_plots = {p["id"] for p in plan["plots"]} | {u["id"] for u in plan.get("promenade_units", [])}
    regs = build_regulars(CONTENT_EXTRA["regulars"], strings, all_plots)
    for aid, a in sorted(acts.items()):
        g = a.get("game")
        if g and g["vs"] and not opponents_at(regs, g["vs"], ""):
            err("act %s: game vs %s: no regular plays it" % (aid, g["vs"]))
    colls = build_collections(CONTENT_EXTRA["collections"], things, acts, strings, regs)
    things.sort(key=lambda t: t["id"])
    used = {t["act"] for t in things}
    gen_strings = {}
    teleports = build_teleports(plan, things, strings, gen_strings)

    quests = CONTENT_EXTRA["quests"]
    qtext = open(QUESTS, encoding="utf-8").read()
    existing = [q["id"] for q in json.loads(qtext)["quests"]]
    ours = {q["id"] for q in quests}
    map_places = {p["id"] for p in load(MAP)["places"]}
    plot_ids = {p["id"] for p in plan["plots"]} | {u["id"] for u in plan.get("promenade_units", [])}
    check_quests(quests, acts, strings, items, plot_ids, map_places, [q for q in existing if q not in ours])
    # every quest step a thing must make must have a thing in town that makes it, with its staff on that plot
    staff_of = {pl["id"]: {r[0] for r in pl["staff"]} for pl in places(plan)}
    things_in = collections.Counter(t["plot"] for t in things)
    for q in quests:
        for o in ([q["start_on"]] if q.get("start_on") else []) + [o for st in q["stages"] for o in st["objectives"]]:
            tgt = o.get("target", "")
            if o["event"] == "visit" and not things_in[tgt]:
                err("quest %s: visit %s: that plot has no things, nothing can report the visit" % (q["id"], tgt))
            if o["event"] != "do":
                continue
            match = (lambda x: x.startswith(tgt[:-1])) if tgt.endswith("*") else (lambda x: x == tgt)
            by = [t for t in things if match(acts[t["act"]]["target"])]
            ok = [t for t in by if acts[t["act"]]["needs_staff"] in ("", "*") or acts[t["act"]]["needs_staff"] in staff_of.get(t["plot"], ())]
            if not by:
                err("quest %s: no thing in town does %s" % (q["id"], tgt))
            elif not ok:
                err("quest %s: do %s: every thing that does it needs staff its plot does not have" % (q["id"], tgt))
    for k, r in sorted(rules.items()):
        if "/" in k and k not in RULES_USED:
            err("rule %s matches no thing (the id changed? fix or drop it)" % k)

    teleports += quest_dev(quests, things, acts, shops, plan, strings, gen_strings)
    all_strings = dict(strings)
    for k, v in gen_strings.items():
        if k in all_strings:
            err("generated string %s clashes with a written one" % k)
        all_strings[k] = v

    # the summary
    print("%-14s %5s %5s  %-44s %4s %5s" % ("plot", "auto", "decl", "things by kind", "acts", "lines"))
    for pid, s in summary.items():
        kinds = " ".join("%s:%d" % (k, n) for k, n in sorted(s["kinds"].items()))
        print("%-14s %5d %5d  %-44s %4d %5d" % (pid, s["auto"], s["declared"], kinds[:44], len(s["acts"]), s["lines"]))
    n_auto = sum(1 for t in things if t.get("auto"))
    line_keys = {line_key(k) for a in acts.values() for k in a["lines"]}
    print("things %d (auto %d, declared %d; derived spots skipped: %s), acts %d (%d used), shops %d, teleports %d, strings %d "
          "(%d act lines), quests %d, collections %s, closed %d, games %d (things %d), regulars %d" % (len(things), n_auto, len(things) - n_auto, dict(skipped) or "none", len(acts), len(used),
                                         len(shops), len(teleports), len(all_strings), len(line_keys), len(quests),
                                         " ".join("%s:%d" % (k, len(v["pieces"])) for k, v in colls.items()) or "none",
                                         sum(1 for t in things if t.get("closed")), sum(1 for a in acts.values() if a.get("game")),
                                         sum(1 for t in things if acts[t["act"]].get("game")), len(regs)))
    for w in warnings:
        print("warning:", w)
    for e in errors:
        print("ERROR:", e)
    if errors:
        print("%d errors" % len(errors))
        sys.exit(1)
    if check:
        print("check ok")
        return

    data = collections.OrderedDict()
    data["about"] = "Written by tools/city/interiors.py from the plan, the building exports and tools/city/content: do not edit by hand (docs/city_interiors.md)."
    data["things"] = things
    data["acts"] = collections.OrderedDict(sorted(acts.items()))
    data["shops"] = collections.OrderedDict(sorted(shops.items()))
    data["collections"] = colls
    data["regulars"] = regs
    write(OUT_THINGS, dump_things(data))
    tp = collections.OrderedDict([("about", "Written by tools/city/interiors.py: x/y map metres (docs/world_map.md), z world cm (the floor, feet), yaw Unreal degrees."),
                                  ("teleports", teleports)])
    write(OUT_TELEPORTS, dump_things(tp))
    for l in LANGS:
        table = collections.OrderedDict((k, all_strings[k][l]) for k in sorted(all_strings))
        write(os.path.join(OUT_STRINGS, "strings_%s.json" % l), json.dumps(table, ensure_ascii=False, indent=1) + "\n")
    new = merge_quests(qtext, quests)
    json.loads(new)
    write(QUESTS, new)


if __name__ == "__main__":
    main()
