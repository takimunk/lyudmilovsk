#!/usr/bin/env python3
"""Town interiors: what every thing in every building does (docs/city_interiors.md, section 2).

Reads the plan (Content/City/city_plan.json), every building export (blender/export/city/<type>[__<plot>].json: its
`things`, or while a building declares none, things derived from its `spots`), the content tables
tools/city/content/*.json and the strings tools/city/content/strings/*.json, and writes:

    unreal/Lyudmilovsk/Content/City/interiors.json            things (world terms), acts, shops
    unreal/Lyudmilovsk/Content/Dev/teleports/interiors.json   developer-mode teleports into every enterable building
    unreal/Lyudmilovsk/Content/Localization/city/strings_<lang>.json
    unreal/Lyudmilovsk/Content/UI/quests.json                 the side quests of tools/city/content/quests.json merged in
                                                              (the other quests are kept byte for byte)

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
QUEST_EVENTS = ("do", "visit", "get", "use", "have", "wear", "reach", "talk", "bird", "walk", "leave_computer")
ACT_FIELDS = ("prompt", "minutes", "price", "lines", "pose", "sound", "event", "target", "give", "daily", "needs_staff",
              "needs_item", "fade", "shop", "screen", "until", "title", "any_hour")
MAIN_STRINGS = os.path.join(CONTENT, "Localization", "strings_ru.json")
PROP_ORDER = ("topic", "what", "service", "item", "to", "role", "shop")
MIN_GAP = 0.3
UNIT_TYPE = "promenade_unit"   # the promenade units' rooms (exports promenade_unit__<variant>.json)
FOOTPRINT_SLACK = 3.0

errors = []
warnings = []
RULES_USED = set()  # the id rules (<plot|type>/<id glob>) that matched a thing: --check fails on the others
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


def load_content():
    """Merge the content tables: acts, shops, rules, defaults (a key defined twice is an error: one owner per thing)."""
    acts, shops, rules, defaults, by_plot_kind = {}, {}, {}, {}, {}
    origin = {}
    for path in sorted(glob.glob(os.path.join(SRC, "*.json"))):
        name = os.path.basename(path)
        if name == "quests.json":
            continue
        data = load(path)
        for section, into in (("acts", acts), ("shops", shops), ("rules", rules), ("defaults", defaults)):
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
    for r in id_rules(rules, plot["id"], plot["building"], thing["id"]):
        kind = r.get("kind", kind)
        props.update(r.get("props") or {})
        act = r.get("act", act)
        hours, days = r.get("hours", hours), r.get("days", days)
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

def expand_lines(spec, strings):
    """A prefix (all <prefix>.<n> strings, in order) or a list of keys, where an entry '<prefix>.*' stands for that prefix's
    strings (a board's generic notices followed by a building's own: ["city.look.club.*", "city.reg.cog.*"])."""
    if isinstance(spec, list):
        out = []
        for s in spec:
            out += expand_lines(s[:-2], strings) if s.endswith(".*") else [s]
        return out
    if not spec:
        return []
    pat = re.compile(re.escape(spec) + r"\.(\d+)$")
    found = sorted((int(m.group(1)), k) for k in strings for m in [pat.match(k)] if m)
    if not found:
        err("act lines %s.<n>: no such strings" % spec)
    return [k for _n, k in found]


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
               "lines": expand_lines(a.get("lines", []), strings), "pose": a.get("pose", "stand"), "sound": a.get("sound", ""),
               "event": a.get("event", "do"), "target": a.get("target", aid), "give": [list(g) for g in a.get("give", [])],
               "daily": int(a.get("daily", 0)), "needs_staff": a.get("needs_staff", ""), "needs_item": a.get("needs_item", ""),
               "fade": bool(a.get("fade", False)), "shop": a.get("shop", ""), "screen": a.get("screen", ""),
               "until": int(a.get("until", -1))}
        if a.get("title"):
            act["title"] = a["title"]
        act["any_hour"] = bool(a.get("any_hour", False))  # explicit: the game's default is true for sit/lie poses
        for k in a:
            if k not in ACT_FIELDS and k not in ("like",):
                err("act %s: unknown field %s" % (aid, k))
        for key in [act["prompt"]] + act["lines"] + ([act["title"]] if act.get("title") else []):
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


def build_things(plan, rules, defaults, by_plot_kind, acts):
    things, summary, skipped = [], collections.OrderedDict(), collections.Counter()
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
            if kind == "counter" and a["shop"] and a["needs_staff"] and props.get("role") != a["needs_staff"]:
                props["role"] = a["needs_staff"]
            w = local_to_world(pl["front"], pl["yaw"], t["pos"])
            rec = {"id": tid, "plot": pl["id"], "kind": kind, "ue": [round(w[0] * 100, 1), round(w[1] * 100, 1), round(w[2] * 100, 1)],
                   "yaw": world_yaw(pl["yaw"], t["yaw"]), "props": {k: props[k] for k in sorted(props)}, "act": act,
                   "hours": list(hours), "days": days}
            if auto:
                rec["auto"] = 1
            things.append(rec)
            kept_local.append({"id": t["id"], "pos": t["pos"], "z": t["pos"][2]})
            s = summary.setdefault(pl["id"], {"auto": 0, "declared": 0, "kinds": collections.Counter(), "acts": set(), "lines": 0})
            s["auto" if auto else "declared"] += 1
            s["kinds"][kind] += 1
            if act not in s["acts"]:
                s["acts"].add(act)
                s["lines"] += len(a["lines"])
    return things, summary, skipped


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
                    "z": round(w[2] * 100 + 100, 1), "yaw": world_yaw(pl["yaw"], 180.0)})
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
                        "z": round(floor + 100, 1), "yaw": round(((t["yaw"] + 180.0) % 360.0) - (360.0 if (t["yaw"] + 180.0) % 360.0 > 180 else 0), 1)})
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
                if ev == "do" and not (any(a.startswith(pref) for a in acts) if pref is not None else tgt in acts):
                    err("quest %s: do %s: no such act" % (qid, tgt))
                if ev == "visit" and tgt not in plot_ids:
                    err("quest %s: visit %s: no such plot" % (qid, tgt))
                if ev in ("get", "use", "have", "wear") and not (any(i.startswith(pref) for i in items) if pref is not None else tgt in items):
                    err("quest %s: %s %s: unknown item" % (qid, ev, tgt))
                if ev == "reach" and tgt not in map_places:
                    err("quest %s: reach %s: not a map place" % (qid, tgt))
    for k in keys:
        if k not in strings and k not in MAIN:
            err("quest string %s is missing" % k)


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
    items = item_ids()
    strings = load_strings()
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
    things, summary, skipped = build_things(plan, rules, defaults, by_plot_kind, acts)
    things.sort(key=lambda t: t["id"])
    used = {t["act"] for t in things}
    gen_strings = {}
    teleports = build_teleports(plan, things, strings, gen_strings)

    quests = load(os.path.join(SRC, "quests.json"))["quests"] if os.path.exists(os.path.join(SRC, "quests.json")) else []
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
    line_keys = {k for a in acts.values() for k in a["lines"]}
    print("things %d (auto %d, declared %d; derived spots skipped: %s), acts %d (%d used), shops %d, teleports %d, strings %d "
          "(%d act lines), quests %d" % (len(things), n_auto, len(things) - n_auto, dict(skipped) or "none", len(acts), len(used),
                                         len(shops), len(teleports), len(all_strings), len(line_keys), len(quests)))
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
    write(OUT_THINGS, dump_things(data))
    tp = collections.OrderedDict([("about", "Written by tools/city/interiors.py: x/y map metres (docs/world_map.md), z world cm (floor + 1 m), yaw Unreal degrees."),
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
