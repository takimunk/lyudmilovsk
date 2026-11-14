#!/usr/bin/env python3
"""Completability audit of the town's side quests and collections, from the GENERATED data.

    python3 tools/city/quest_audit.py          # errors and warnings; exit 1 on an error
    python3 tools/city/quest_audit.py -q       # errors only

Reads Content/City/interiors.json, Content/UI/quests.json (the lines marked "from": "tools/city/interiors.py"), the plan's
staff (Content/City/city_plan.json), the item table and the strings, and proves for every step that it can happen:

* do/win <act>: a thing in town runs an act with that target, its staff works at the plot, the thing is open at least one
  hour a week; a needs_item has a source; win: the act has a game, and a regular plays it (or it is played alone);
* visit <plot>: the plot has things;  reach <place>: in map.json;
* have/get/use <item>: something gives it (an act's give, a shop's goods, a game prize, an earlier stage's or another
  quest's reward) on an open, staffed thing;
* ask <topic>: the topic is on a counter (act or thing topics) whose staff works there and is open, its conditions can
  hold (h/d against the counter's hours, heard/did/read/have/count reachable);
* read <text>: the text is on a thing's read act, its pages have one that always holds;
* the order: a later stage waiting for an event an earlier stage's step already raises (the quest engine skips ahead on it:
  docs/city_interiors.md "A quest's last stage should wait for a do"), a last stage on a bare visit;
* rewards are known items; a `use` of an item only one quest reward gives, by two quests;
* the first step is discoverable: the quest starts on something that exists, and the first stage's journal line exists;
* collections: every declared piece has a source on an open thing (act/thing/line/game/text/topic stamps), every given
  piece is declared, a `count` condition never asks for more than the pieces.
"""
import collections
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import interiors as I  # noqa: E402  (main() is guarded; we only use its readers)

DAYS = ["mon", "tue", "wed", "thu", "fri", "sat", "sun"]
ERRORS, WARNINGS = [], []


def err(m):
    ERRORS.append(m)


def warn(m):
    WARNINGS.append(m)


def hours_set(h):
    a, b = int(h[0]), int(h[1])
    if a == b:
        return set()
    if (a, b) == (0, 24):
        return set(range(24))
    if a < b:
        return set(range(a, b))
    return set(range(a, 24)) | set(range(0, b))


def week(t, act):
    """The (day, hour) pairs a thing runs its act in."""
    if act.get("any_hour"):
        return {(d, h) for d in range(7) for h in range(24)}
    ds = I.days_of(t["days"])
    return {(d, h) for d in ds for h in hours_set(t["hours"])}


def cond_week(c):
    """The (day, hour) pairs line/topic conditions h and d allow."""
    hs = hours_set(c["h"]) if "h" in c else set(range(24))
    ds = I.days_of(c["d"]) if "d" in c else set(range(7))
    return {(d, h) for d in ds for h in hs}


def main():
    quiet = "-q" in sys.argv
    data = json.load(open(os.path.join(I.CONTENT, "City", "interiors.json"), encoding="utf-8"))
    qfile = json.load(open(I.QUESTS, encoding="utf-8"))
    plan = json.load(open(I.PLAN, encoding="utf-8"))
    map_places = {p["id"] for p in json.load(open(os.path.join(I.CONTENT, "UI", "map.json"), encoding="utf-8"))["places"]}
    items = I.item_ids()
    strings = {}
    for lang in I.LANGS:
        p = os.path.join(I.CONTENT, "Localization", "city", "strings_%s.json" % lang)
        for k, v in json.load(open(p, encoding="utf-8")).items():
            strings.setdefault(k, {})[lang] = v
    main_ru = json.load(open(I.MAIN_STRINGS, encoding="utf-8"))

    things, acts, shops = data["things"], data["acts"], data["shops"]
    topics, texts, colls, regulars = data["topics"], data["texts"], data["collections"], data["regulars"]
    quests = [q for q in qfile["quests"] if q.get("from") == "tools/city/interiors.py"]
    staff_of = {pl["id"]: {r[0] for r in pl["staff"]} for pl in I.places(plan)}

    def staffed(t):
        ns = acts[t["act"]]["needs_staff"]
        return ns in ("", "*") or ns in staff_of.get(t["plot"], ())

    live = [t for t in things if staffed(t) and week(t, acts[t["act"]])]
    live_ids = {t["id"] for t in live}
    by_target = collections.defaultdict(list)
    for t in live:
        by_target[acts[t["act"]]["target"]].append(t)
    plots_with_things = {t["plot"] for t in things}

    # ---- item sources (open, staffed things; game prizes; quest rewards)
    item_src = collections.defaultdict(set)
    for t in live:
        a = acts[t["act"]]
        for g in a["give"]:
            item_src[g[0]].add("act %s" % a["target"])
        for g in shops.get(a["shop"], {}).get("goods", []):
            item_src[g["item"]].add("shop %s" % a["shop"])
        for g in (a.get("game") or {}).get("prize", []):
            item_src[g[0]].add("prize %s" % a["target"])
    reward_src = collections.defaultdict(list)
    for q in quests:
        for st in q["stages"]:
            for it in (st.get("reward") or {}).get("items", {}):
                reward_src[it].append(q["id"])
                if it not in items:
                    err("quest %s: reward item %s is not in the item table" % (q["id"], it))

    # items that come from outside the town's things: the corvids' gifts (CorvidLife.cpp), the catch (LyudFishing)
    outside = {i for i in items if i.startswith("find_") or i.startswith("fish_")}

    def item_ok(it, quest=None, upto=None):
        if item_src.get(it) or it in outside or it.startswith("fish_"):
            return True
        for qid in reward_src.get(it, []):
            if qid != quest:
                return True
        if quest and upto is not None:
            q = next(x for x in quests if x["id"] == quest)
            for st in q["stages"][:upto]:
                if it in (st.get("reward") or {}).get("items", {}):
                    return True
        return False

    # ---- topics: where they are asked
    topic_at = collections.defaultdict(list)
    for t in live:
        a = acts[t["act"]]
        for tp in (t.get("topics") or a.get("topics") or []):
            topic_at[tp].append(t)
    heard_ok = {}

    def stamp_reachable(s):
        return s in pieces_given

    def conds_ok(c, ws, where, depth=0):
        """Line/topic conditions that can hold within the open week ws; returns a reason or ''."""
        if ("h" in c or "d" in c) and not (cond_week(c) & ws):
            return "its h/d %s/%s never meet the open hours" % (c.get("h"), c.get("d"))
        if "before" in c and int(c["before"]) <= int(c.get("after", 0)):
            return "before %s <= after %s" % (c["before"], c.get("after", 0))
        if c.get("did"):
            dt = c["did"]
            if not dt.startswith("!") and dt not in by_target:
                return "did %s: no open thing does it" % dt
        if c.get("heard") and depth < 4:
            h = c["heard"]
            if not h.startswith("!") and not topic_reach(h, depth + 1):
                return "heard %s: that topic can never be heard" % h
        if c.get("read") and c["read"] not in read_ok:
            return "read %s: that text can never be read" % c["read"]
        if c.get("have") and not stamp_reachable(c["have"]):
            return "have %s: that stamp is never given" % c["have"]
        if c.get("count"):
            s, n = c["count"]
            got = {p for p in pieces_given if p.startswith(s + "/")}
            if len(got) < int(n):
                return "count %s %s: only %d pieces can be had" % (s, n, len(got))
        return ""

    def topic_reach(tid, depth=0):
        if tid in heard_ok:
            return heard_ok[tid]
        heard_ok[tid] = False  # cycles: not reachable through themselves
        tp = topics.get(tid)
        ok = False
        if tp:
            for t in topic_at.get(tid, []):
                ws = week(t, acts[t["act"]])
                if conds_ok(tp, ws, tid, depth):
                    continue
                for ln in tp["lines"]:
                    c = ln if isinstance(ln, dict) else {}
                    if not conds_ok(c, ws & cond_week(tp) if ("h" in tp or "d" in tp) else ws, tid, depth):
                        ok = True
                        break
                if ok:
                    break
        heard_ok[tid] = ok
        return ok

    # ---- texts that can be read (on a live thing's read act, with a page that always holds)
    read_ok = set()
    for t in live:
        for tx in (acts[t["act"]].get("read") or {}).get("texts", []):
            if tx in texts:
                pages = texts[tx]["pages"]
                plain = any(isinstance(p, str) or (isinstance(p, dict) and set(p) == {"k"})
                            or (isinstance(p, list) and any(isinstance(x, str) for x in p)) for p in pages)
                if plain:
                    read_ok.add(tx)

    # ---- stamps given (first pass without conditions on stamps, then a check of the conditions)
    pieces_given = set()
    stamp_sources = []  # (stamp, conds dict, week, where)

    def add_stamp(s, c, ws, where):
        stamp_sources.append((s, c or {}, ws, where))

    for t in live:
        a = acts[t["act"]]
        ws = week(t, a)
        # the act's stamp and the thing's own both come (CityThingsModel Run: GiveStamp(A->Collect), GiveStamp(T.Collect))
        for c in {a.get("collect") or "", t.get("collect") or ""} - {""}:
            if c.endswith("/{line}"):
                for ln in a["lines"]:
                    k = ln if isinstance(ln, str) else ln["k"]
                    add_stamp(c[:-len("{line}")] + k, ln if isinstance(ln, dict) else {}, ws, t["id"])
            else:
                add_stamp(c.replace("{plot}", t["plot"]), {}, ws, t["id"])
        for ln in a["lines"]:
            if isinstance(ln, dict) and ln.get("collect"):
                add_stamp(ln["collect"], ln, ws, t["id"])
        g = a.get("game") or {}
        if g.get("collect"):
            if "{opponent}" in g["collect"]:
                vs = g.get("vs")
                here = [r for r, v in regulars.items() if vs in v["plays"] and (not v.get("plots") or t["plot"] in v["plots"])]
                if not here:
                    here = [r for r, v in regulars.items() if vs in v["plays"]]
                for r in here:
                    add_stamp(g["collect"].replace("{opponent}", r), {}, ws, t["id"])
            else:
                add_stamp(g["collect"], {}, ws, t["id"])
        for tx in (a.get("read") or {}).get("texts", []):
            if tx in read_ok and texts[tx].get("collect"):
                add_stamp(texts[tx]["collect"], {}, ws, "text " + tx)
        for tp in (t.get("topics") or a.get("topics") or []):
            if topics.get(tp, {}).get("collect"):
                add_stamp(topics[tp]["collect"], topics[tp], ws, "topic " + tp)
    pieces_given = {s for s, *_ in stamp_sources}
    # the conditions of each source (two passes so stamps asking for stamps settle)
    for _ in range(2):
        good = set()
        for s, c, ws, where in stamp_sources:
            if s.startswith("set/"):
                continue
            if not conds_ok({k: v for k, v in c.items() if k not in ("k", "lines", "collect")}, ws, where):
                good.add(s)
        pieces_given = good
    # topics are asked only when an answer line holds too
    for s, c, ws, where in stamp_sources:
        if where.startswith("topic ") and s in pieces_given and not topic_reach(where[6:]):
            pass  # topic_reach is reported per quest; stamps keep the simple check

    # ---- collections
    for cid, cdef in sorted(colls.items()):
        declared = cdef.get("pieces") or []
        got = {s.split("/", 1)[1] for s in pieces_given if s.split("/", 1)[0] == cid}
        all_given = {s.split("/", 1)[1] for s, *_ in stamp_sources if s.split("/", 1)[0] == cid}
        if len(set(declared)) != len(declared):
            err("collection %s: a piece declared twice: %s" % (cid, [p for p, n in collections.Counter(declared).items() if n > 1]))
        for p in declared:
            if p not in got:
                if p in all_given:
                    err("collection %s: piece %s: given only under conditions that never hold" % (cid, p))
                else:
                    err("collection %s: piece %s: nothing open in town gives it" % (cid, p))
        for p in sorted(all_given - set(declared)):
            if declared:
                err("collection %s: %s is given but not declared" % (cid, p))
        if cdef.get("title") not in strings:
            err("collection %s: title %s missing" % (cid, cdef.get("title")))
        # the number in the title or its words
        n = len(declared) or len(got)
        for lang in ("ru", "en"):
            txt = strings.get(cdef.get("title"), {}).get(lang, "")
            for m in re.findall(r"\d+", txt):
                if int(m) != n and int(m) > 1:
                    warn("collection %s: title says %s, it has %d pieces" % (cid, m, n))
    for s, c, ws, where in stamp_sources:
        if s.split("/", 1)[0] not in colls:
            err("%s gives a stamp of an undeclared collection: %s" % (where, s))
    # count conditions anywhere
    for aid, a in acts.items():
        for ln in a["lines"]:
            if isinstance(ln, dict) and ln.get("count"):
                s, n = ln["count"]
                tot = len(colls.get(s, {}).get("pieces") or [])
                if tot and int(n) > tot:
                    err("act %s: line %s asks for %s of %s, which has %d pieces" % (aid, ln["k"], n, s, tot))
    for tid, tp in topics.items():
        for c in [tp] + [ln for ln in tp["lines"] if isinstance(ln, dict)]:
            if c.get("count"):
                s, n = c["count"]
                tot = len(colls.get(s, {}).get("pieces") or []) or len({p for p in pieces_given if p.startswith(s + "/")})
                if int(n) > tot:
                    err("topic %s: %s asks for %s of %s, which has %d pieces" % (tid, c.get("k"), n, s, tot))

    # ---- quests
    ev_reason = {}
    n_steps = 0

    def maker_things(ev, tgt):
        if ev in ("do", "win"):
            if tgt.endswith("*"):
                return [t for k, v in by_target.items() if k.startswith(tgt[:-1]) for t in v]
            return by_target.get(tgt, [])
        if ev == "visit":
            return [t for t in live if t["plot"] == tgt]
        if ev == "ask":
            return topic_at.get(tgt, [])
        return []

    def raised_by(o):
        """The (event, target) pairs doing objective o raises besides its own: a use reports visit <plot> too."""
        ev, tgt = o["event"], o["target"]
        out = {(ev, tgt)}
        if ev in ("do", "win", "ask"):
            ts = maker_things(ev, tgt)
            plots = {t["plot"] for t in ts}
            if len(plots) == 1:
                out.add(("visit", plots.pop()))
            if ev == "win":
                out.add(("do", tgt))
        return out

    for q in quests:
        qid = q["id"]
        so = q.get("start_on") or {}
        sev, stg = so.get("event", ""), so.get("target", "")
        if not so and not q.get("auto_start"):
            err("quest %s: no start_on and no auto_start: it never starts" % qid)
        elif sev == "visit" and stg not in plots_with_things:
            err("quest %s: starts on visit %s, a plot without things" % (qid, stg))
        elif sev == "reach" and stg not in map_places:
            err("quest %s: starts on reach %s, not a map place" % (qid, stg))
        elif sev in ("do", "win", "ask") and not maker_things(sev, stg):
            err("quest %s: starts on %s %s, which nothing open makes" % (qid, sev, stg))
        elif sev == "ask" and not topic_reach(stg):
            err("quest %s: starts on ask %s, a topic that is never offered" % (qid, stg))
        for k in ("quest.%s.title" % qid, "quest.%s.desc" % qid, "quest.%s.done" % qid):
            if k not in strings and k not in main_ru:
                err("quest %s: string %s missing" % (qid, k))
        # the first step: where the quest starts, or said in the journal's first line
        if sev == "visit" and q["stages"]:
            for o in q["stages"][0]["objectives"]:
                ts = maker_things(o["event"], o["target"]) if o["event"] in ("do", "win", "ask", "visit") else None
                if ts and not any(t["plot"] == stg for t in ts):
                    warn("quest %s: starts at %s but its first step %s %s is elsewhere (%s): only the journal tells"
                         % (qid, stg, o["event"], o["target"], ", ".join(sorted({t["plot"] for t in ts}))))
        earlier = set()
        if so:
            earlier.add((sev, stg))
        start_plot = stg if sev == "visit" else None
        for n, st in enumerate(q["stages"]):
            if "quest.%s.%s.log" % (qid, st["id"]) not in strings:
                err("quest %s: stage %s has no journal line" % (qid, st["id"]))
            raised_here = set()
            for o in st["objectives"]:
                n_steps += 1
                ev, tgt = o["event"], o["target"]
                where = "quest %s stage %s (%s %s)" % (qid, st["id"], ev, tgt)
                if "quest.%s.%s.obj" % (qid, o["id"]) not in strings:
                    err("%s: objective words missing" % where)
                if ev in ("do", "win"):
                    ts = maker_things(ev, tgt)
                    if not ts:
                        anyt = [t for t in things if acts[t["act"]]["target"] == tgt]
                        err("%s: %s" % (where, "no thing in town does it" if not anyt else
                                        "its things are never open or lack staff: %s" % ", ".join(t["id"] for t in anyt[:3])))
                        continue
                    a = acts[ts[0]["act"]]
                    ni = a["needs_item"]
                    if ni and not item_ok(ni, qid, n):
                        err("%s: needs item %s, which nothing gives" % (where, ni))
                    if ev == "win":
                        games = [acts[t["act"]].get("game") for t in ts if acts[t["act"]].get("game")]
                        if not games:
                            err("%s: the act has no game: nothing wins it" % where)
                        else:
                            g = games[0]
                            if g.get("vs") and not any(g["vs"] in r["plays"] for r in regulars.values()):
                                err("%s: the game is vs %s and no regular plays it" % (where, g["vs"]))
                elif ev == "visit":
                    if tgt not in plots_with_things:
                        err("%s: that plot has no things" % where)
                    elif not maker_things(ev, tgt):
                        err("%s: nothing at that plot is ever open" % where)
                    if n == len(q["stages"]) - 1 and n > 0:
                        warn("%s: the last stage waits for a bare visit (the engine takes it as a skip-ahead)" % where)
                elif ev in ("have", "get", "use"):
                    if tgt not in items and not tgt.endswith("*"):
                        err("%s: unknown item" % where)
                    elif not tgt.endswith("*") and not item_ok(tgt, qid, n):
                        err("%s: nothing open in town gives %s (no shop, act, prize or other quest)" % (where, tgt))
                elif ev == "ask":
                    if tgt not in topics:
                        err("%s: no such topic" % where)
                    elif not topic_at.get(tgt):
                        err("%s: the topic is on no open, staffed counter" % where)
                    elif not topic_reach(tgt):
                        err("%s: the topic is never offered (its conditions or lines never hold)" % where)
                    elif (sev, stg) == ("ask", tgt) and n > 0 and topics[tgt].get("once"):
                        err("%s: a once topic the quest started on cannot be asked again" % where)
                elif ev == "read":
                    if tgt not in texts:
                        err("%s: no such text" % where)
                    elif tgt not in read_ok:
                        err("%s: the text is on no open thing or has no plain page" % where)
                elif ev == "reach":
                    if tgt not in map_places:
                        err("%s: not a map place" % where)
                else:
                    err("%s: event not checked by the audit" % where)
                # a later stage's step raised by an earlier one: the engine skips ahead (stage n done too soon)
                if n > 0 and ev != "have":
                    hit = [(e, t_) for (e, t_) in earlier if e == ev and t_ == tgt]
                    if hit:
                        err("%s: an earlier step (or the start) already raises %s %s: the quest skips ahead to this stage"
                            % (where, ev, tgt))
                raised_here |= raised_by(o)
            # the start's visit counts toward stage 0 too; a later stage's visit of the home plot fires at every use there
            earlier |= raised_here
        # a `have` stage the engine can jump over: a later stage's do/ask/... skips ahead (LyudQuests Notify) and finishes
        # the stages before it, so the item must be asked again at that stage or after (a have/use, or the act's needs_item)
        def needs_of(st):
            out = set()
            for o in st["objectives"]:
                if o["event"] in ("have", "use"):
                    out.add(o["target"])
                elif o["event"] in ("do", "win"):
                    out |= {acts[t["act"]]["needs_item"] for t in maker_things(o["event"], o["target"])} - {""}
            return out
        # a generic act (things in several plots) whose things are all away from the quest's own building: the journal
        # sends the hero to a room that cannot do it there (the school's "piano in the music room" with no piano at school)
        plots_per = []
        for st in q["stages"]:
            for o in st["objectives"]:
                if o["event"] in ("do", "win"):
                    plots_per.append({t["plot"] for t in maker_things(o["event"], o["target"])})
        home = collections.Counter(p for ps in plots_per if len(ps) == 1 for p in ps).most_common(1)
        if home and home[0][1] >= 2:
            hp = home[0][0]
            for st in q["stages"]:
                for o in st["objectives"]:
                    if o["event"] in ("do", "win"):
                        ps = {t["plot"] for t in maker_things(o["event"], o["target"])}
                        if len(ps) > 1 and hp not in ps:
                            warn("quest %s stage %s (%s %s): every other step is at %s, this generic act is only at %s"
                                 % (qid, st["id"], o["event"], o["target"], hp, ", ".join(sorted(ps)[:5])))
        stg_list = q["stages"]
        for n, st in enumerate(stg_list):
            for o in st["objectives"]:
                if o["event"] != "have":
                    continue
                for m in range(n + 1, len(stg_list)):
                    if not any(x["event"] != "have" for x in stg_list[m]["objectives"]):
                        continue
                    if not any(o["target"] in needs_of(stg_list[k]) for k in range(m, len(stg_list))):
                        warn("quest %s stage %s (have %s): a step of stage %s skips ahead over it and nothing later asks "
                             "for the item: the quest can end without it" % (qid, st["id"], o["target"], stg_list[m]["id"]))
                        break
        if not any(st.get("reward") for st in q["stages"]):
            warn("quest %s: no reward in any stage" % qid)

    # one `use` of an item only one reward gives, by two quests
    use_of = collections.defaultdict(list)
    for q in quests:
        for st in q["stages"]:
            for o in st["objectives"]:
                if o["event"] == "use":
                    use_of[o["target"]].append(q["id"])
    for it, qs in use_of.items():
        if len(qs) > 1 and not item_src.get(it) and len(reward_src.get(it, [])) < len(qs):
            err("item %s is used up by %s but only %d reward(s) give it" % (it, ", ".join(qs), len(reward_src.get(it, []))))

    for m in ERRORS:
        print("ERROR  " + m)
    if not quiet:
        for m in WARNINGS:
            print("warn   " + m)
    print("quest_audit: %d quests, %d steps, %d collections (%d pieces): %d errors, %d warnings"
          % (len(quests), n_steps, len(colls), sum(len(c.get("pieces") or []) for c in colls.values()), len(ERRORS), len(WARNINGS)))
    return 1 if ERRORS else 0


if __name__ == "__main__":
    sys.exit(main())
