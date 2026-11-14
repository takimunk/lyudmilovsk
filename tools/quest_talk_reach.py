#!/usr/bin/env python3
"""Offline reachability of the people's talk: which written lines can never be said, or are shadowed.

  python3 tools/quest_talk_reach.py [--root <repo>] [--strict] [--verbose]

It complements tools/check_quest_talk.py (syntax, keys, known words): this one simulates the engine. For every person
([Id] section of Content/Dialogue/npc_dialogue.txt) it enumerates the states of the quests that person has talk for (and
the quests named by their conditions): inactive, each stage, done; the pocket (the items of have:/no: conditions and of
`bring` objectives to them), the flags of flag: conditions, the sun (day, low, night, cloudy) and whether the first
conversation has been held. In each state that the quests' rules allow, it presses E repeatedly with
UNpcDialogueSubsystem::StartTalk's selection and counting rules (NpcDialogue.cpp) and ULyudQuests::TalkStage
(UI/LyudQuests.cpp) and records which talk entries and line keys were ever said.

Reports:
  1. quest./aside. talk entries never selected (with the reason when one can be derived)
  2. everyday `again` exchanges and `sun.<state>` lines never said within 30 everyday talks; a talk entry's `.again`
     exchanges never said
  3. `line.<person>.` keys of Content/Localization/quests/strings_ru.json that nothing references (orphans)
  4. remarks (quest_scenes.json) on a stage the quest does not have, or on `done` of a quest that can never be done
Exit code 0, or with --strict 1 when 1 or 4 found anything.

Two models are run. The *lenient* one holds every state still for 12 presses. The *staged* one adds what the quests do
while you talk: a stage whose objectives are all talks to this person (or bring-to-this-person with the items in the
pocket) is over after its talk (1 press; 2 on the first stage of a quest this person's talk begins, whose starting talk
does not count), an inactive quest that this person's talk begins is begun by that press (1 press), and a stage whose
objectives are all `have` cannot be on while the pocket holds every one of them (BeginStage/HandleEvent finish it).
Entries never said even in the lenient model are findings; those said only in the lenient model are listed apart as
"staged" findings (they rest on objective counts following the pocket, which save edits or dev jumps can break).

Not modelled (treated as reachable / free): dev commands (Quest.SetStage, DevJump); flags are free booleans once
something can set them (remark.<id>, pickup.<id>, diary.<note>), without their own timing; another quest's state is only
tied to this one through the `reward.start` chain (a quest that only a reward begins is inactive until that stage is
finished, and is begun by it); the pocket is free apart from the rules above; counts start from zero in every state
(a variant spent in an earlier state only frees the entries behind it, so zero counts are the most permissive start);
line keys built in code at run time are not seen by the orphan search (it looks for the literal key in Source/ and
Content/ outside Localization/).
"""
import argparse
import collections
import glob
import itertools
import json
import os
import re
import sys

SUN = ("day", "low", "night", "cloudy")
PRESSES = 12
EVERYDAY = 30


def lc(s):
    return (s or "").lower()


# ------------------------------------------------------------------ the dialogue file (UNpcDialogueSubsystem::Reload)

class Talk:
    def __init__(self, section, quest, stages, when, aside, n):
        self.section, self.quest, self.stages, self.when, self.aside = section, quest, stages, when, aside
        self.lines = []          # line keys (with line.)
        self.again = []          # list of exchanges
        self.line_no = n         # where it was first written
        self.lines_no = None
        self.again_no = []
        self.overwritten = []    # (line no, keys) of earlier plain lines replaced by a later one
        self.index = 0

    def is_done(self):
        return "done" in [lc(s) for s in self.stages]

    def name(self):
        s = "%s.%s.%s" % ("aside" if self.aside else "quest", self.quest, ",".join(self.stages))
        if self.when:
            s += ".if." + "+".join(self.when)
        return s

    def where(self):
        return "npc_dialogue.txt:%d [%s] %s" % (self.lines_no or self.line_no, self.section, self.name())


class Person:
    def __init__(self, sid, n):
        self.id, self.line_no = sid, n
        self.first, self.again, self.sun, self.talks = [], [], collections.OrderedDict(), []


def parse_exchange(value):
    return ["line." + p.strip() for p in value.split("|") if p.strip()]


def parse_dialogue(path):
    people = collections.OrderedDict()   # lower id -> Person (FName keys are case-insensitive; a repeated [Id] starts over)
    cur = None
    with open(path, encoding="utf-8") as f:
        for n, raw in enumerate(f, 1):
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            if line.startswith("[") and line.endswith("]"):
                sid = line[1:-1].strip()
                cur = Person(sid, n)
                people[lc(sid)] = cur
                continue
            if cur is None or "=" not in line:
                continue
            key, value = [x.strip() for x in line.split("=", 1)]
            k = lc(key)
            if k == "first":
                cur.first += parse_exchange(value)
            elif k == "again":
                cur.again.append(parse_exchange(value))
            elif k.startswith("sun."):
                cur.sun[lc(key[4:])] = (parse_exchange(value), n)   # TMap::Add replaces
            elif k.startswith("quest.") or k.startswith("aside."):
                aside = k.startswith("aside.")
                parts = [p for p in key.split(".") if p != ""]
                if len(parts) < 3:
                    continue
                quest = parts[1]
                stages = [s.strip() for s in parts[2].split(",") if s.strip() != ""]
                when, tail = [], 3
                if len(parts) > 4 and lc(parts[3]) == "if":
                    cond_end = len(parts) - 1 if lc(parts[-1]) == "again" else len(parts)
                    conds = ".".join(parts[4:cond_end])
                    when = [c for c in conds.split("+") if c != ""]
                    tail = cond_end
                t = None
                for x in cur.talks:
                    if lc(x.quest) == lc(quest) and [lc(s) for s in x.stages] == [lc(s) for s in stages] \
                            and [lc(c) for c in x.when] == [lc(c) for c in when] and x.aside == aside:
                        t = x
                        break
                if t is None:
                    t = Talk(cur.id, quest, stages, when, aside, n)
                    t.index = len(cur.talks)
                    cur.talks.append(t)
                if len(parts) > tail and lc(parts[tail]) == "again":
                    t.again.append(parse_exchange(value))
                    t.again_no.append(n)
                else:
                    if t.lines_no is not None:
                        t.overwritten.append((t.lines_no, t.lines))
                    t.lines = parse_exchange(value)
                    t.lines_no = n
    return people


# ------------------------------------------------------------------ the quests (ULyudQuests)

def target_matches(want, got):
    if not want:
        return True
    if want.endswith("*"):
        prefix = want[:-1]
        return prefix == "" or lc(got).startswith(lc(prefix))
    return lc(want) == lc(got)


class Quests:
    def __init__(self, data):
        self.defs = collections.OrderedDict((lc(q["id"]), q) for q in data["quests"])
        self.starters = collections.defaultdict(list)   # quest -> [(starter quest, stage index)]
        for qid, q in self.defs.items():
            for i, s in enumerate(q.get("stages", [])):
                for y in (s.get("reward") or {}).get("start", []) or []:
                    self.starters[lc(y)].append((qid, i))
        self.own_start = {qid: bool(q.get("auto_start")) or bool((q.get("start_on") or {}).get("event"))
                          for qid, q in self.defs.items()}
        start = {qid for qid, v in self.own_start.items() if v}
        changed = True
        while changed:
            changed = False
            for qid in self.defs:
                if qid not in start and any(x in start for x, _ in self.starters[qid]):
                    start.add(qid)
                    changed = True
        self.startable = start

    def stages(self, q):
        return self.defs[q].get("stages", []) if q in self.defs else []

    def stage_ids(self, q):
        return [lc(s["id"]) for s in self.stages(q)]

    def doneable(self, q):
        return q in self.startable and len(self.stages(q)) > 0

    def talk_start_by(self, q, person):
        d = self.defs.get(q)
        so = (d or {}).get("start_on") or {}
        return bool(d) and len(self.stages(q)) > 0 and lc(so.get("event")) == "talk" and target_matches(so.get("target", ""), person)

    def domain(self, q):
        """States of a quest: ('I',), ('A', k), ('D',)."""
        out = [("I",)]
        if q in self.startable:
            out += [("A", k) for k in range(len(self.stages(q)))]
        if self.doneable(q):
            out.append(("D",))
        return out

    @staticmethod
    def past(state, j):
        return state[0] == "D" or (state[0] == "A" and state[1] > j)

    def consistent(self, assign):
        """The reward.start chain: a quest only a reward begins is inactive until a starter finished that stage; a
        finished stage has begun the quests of its reward (Start of an inactive quest)."""
        for y, st in assign.items():
            if st[0] != "I" and not self.own_start.get(y, False):
                ok = False
                for x, j in self.starters[y]:
                    if x not in assign or self.past(assign[x], j):
                        ok = True
                        break
                if not ok:
                    return False
            for x, j in self.starters[y]:
                if st[0] == "I" and x in assign and self.past(assign[x], j):
                    return False
        return True


# ------------------------------------------------------------------ the simulation of one person

class Sim:
    def __init__(self, person, quests, flags_settable, items_known):
        self.p = person
        self.qs = quests
        self.flags_settable = flags_settable
        self.items_known = items_known
        self.pid = person.id

    # -- what TalkStage gives for this person
    def bring_objs(self, q, k):
        out = []
        for oi, o in enumerate(self.qs.stages(q)[k].get("objectives", [])):
            if o.get("event") == "bring" and "@" in (o.get("target") or ""):
                item, to = o["target"].split("@", 1)
                if lc(to) == lc(self.pid):
                    out.append((oi, lc(item), max(1, int(o.get("need", 1)))))
        return out

    def talk_stage(self, q, st, pocket, met):
        if st[0] == "D":
            return "done"
        if st[0] == "A":
            k = st[1]
            sid = self.qs.stage_ids(q)[k]
            for oi, item, need in self.bring_objs(q, k):
                if not met.get((q, oi)) and pocket.get(item, 0) < need:
                    return sid + "_empty"
            return sid
        if self.qs.talk_start_by(q, self.pid):
            return self.qs.stage_ids(q)[0]
        return None

    def cond_holds(self, cond, sun, assign, pocket, flags):
        if ":" not in cond:
            return lc(cond) == sun
        kind, arg = cond.split(":", 1)
        kind = lc(kind)
        if kind in ("have", "no"):
            has = pocket.get(lc(arg), 0) > 0
            return (kind == "have") == has
        if kind in ("done", "on"):
            st = assign.get(lc(arg), ("I",))
            return st[0] == ("D" if kind == "done" else "A")
        if kind == "flag":
            return flags.get(arg, False)
        return False

    # -- one state, several presses (StartTalk)
    def run(self, assign, pocket, met, flags, sun, held0, presses, said, said_again, everyday_said):
        p = self.p
        counts = collections.Counter()
        counts[lc(self.pid)] = held0
        stage_of = {q: self.talk_stage(q, st, pocket, met) for q, st in assign.items()}
        holds = [all(self.cond_holds(c, sun, assign, pocket, flags) for c in t.when) for t in p.talks]

        def count_key(t):
            s = "%s/%s%s.%s" % (self.pid, "aside." if t.aside else "", t.quest, t.stages[0] if t.stages else "")
            if t.when:
                s += "?" + "+".join(t.when)
            return lc(s)

        def stage_in(t, stage):
            return stage is not None and lc(stage) in [lc(s) for s in t.stages]

        selected_here = []
        for _ in range(presses):
            met_ = counts[lc(self.pid)] > 0 or len(p.first) == 0

            def spent(t):
                if not t.stages:
                    return True
                n = counts[count_key(t)]
                if t.aside:
                    return (not met_) or n > len(t.again)
                return len(t.again) == 0 and n > 0 and (len(t.when) > 0 or t.is_done())

            chosen = None
            for pas in (0, 1):
                if chosen:
                    break
                for t in p.talks:
                    if (pas == 0) != (not t.aside and not t.is_done()):
                        continue
                    stage = stage_of.get(lc(t.quest))
                    if stage_in(t, stage) and t.lines and not spent(t) and holds[t.index]:
                        chosen = t
                        break
            if chosen is not None and not chosen.when:
                stage = stage_of.get(lc(chosen.quest))
                for t in p.talks:
                    if lc(t.quest) == lc(chosen.quest) and t.aside == chosen.aside and t.when and stage_in(t, stage) \
                            and t.lines and not spent(t) and holds[t.index]:
                        chosen = t
                        break
            held = counts[lc(self.pid)]
            if chosen is not None:
                ck = count_key(chosen)
                qh = counts[ck]
                counts[ck] += 1
                selected_here.append(chosen.index)
                if qh > 0 and chosen.again:
                    i = (qh - 1) % len(chosen.again)
                    said.add(chosen.index)
                    said_again.add((chosen.index, i))
                    continue
                if qh == 0 or not chosen.is_done():
                    said.add(chosen.index)
                    continue
            counts[lc(self.pid)] = held + 1
            if held == 0 and p.first:
                everyday_said.add("first")
                continue
            if not p.again:
                everyday_said.add("first")
                continue
            if held % 3 == 2 and sun in p.sun:
                counts[lc(self.pid) + "/sun"] += 1
                everyday_said.add("sun." + sun)
                continue
            skip = 1 if p.first else 0
            everyday_said.add("again%d" % (max(0, held - skip - counts[lc(self.pid) + "/sun"]) % len(p.again)))
        return stage_of, holds

    # -- the states
    def relevant(self):
        qset = []
        for t in self.p.talks:
            for q in [lc(t.quest)] + [lc(c.split(":", 1)[1]) for c in t.when if lc(c.split(":", 1)[0]) in ("done", "on")]:
                if q in self.qs.defs and q not in qset:
                    qset.append(q)
        return qset

    def press_limit(self, assign, pocket, met):
        limit = PRESSES
        for q, st in assign.items():
            if st[0] == "I" and self.qs.talk_start_by(q, self.pid):
                limit = min(limit, 1)
            if st[0] != "A":
                continue
            k = st[1]
            objs = self.qs.stages(q)[k].get("objectives", [])
            if not objs:
                continue
            brings = {oi: (item, need) for oi, item, need in self.bring_objs(q, k)}
            talk_need = 0
            ok = True
            for oi, o in enumerate(objs):
                if o.get("event") == "talk" and target_matches(o.get("target", ""), self.pid):
                    talk_need = max(talk_need, max(1, int(o.get("need", 1))))
                elif oi in brings:
                    item, need = brings[oi]
                    if not met.get((q, oi)) and pocket.get(item, 0) < need:
                        ok = False
                else:
                    ok = False
            if ok:
                n = max(1, talk_need)
                if k == 0 and self.qs.talk_start_by(q, self.pid):
                    n += 1
                limit = min(limit, n)
        return limit

    def have_rule_ok(self, assign, pocket):
        for q, st in assign.items():
            if st[0] != "A":
                continue
            objs = self.qs.stages(q)[st[1]].get("objectives", [])
            if objs and all(o.get("event") == "have" for o in objs):
                if all(pocket.get(lc(o.get("target")), 0) >= max(1, int(o.get("need", 1))) for o in objs):
                    return False
        return True

    def simulate(self):
        p = self.p
        rel = self.relevant()
        # items: of conditions and of bring/have objectives of the relevant quests' stages
        levels = collections.defaultdict(lambda: {0, 1})
        for t in p.talks:
            for c in t.when:
                if ":" in c and lc(c.split(":", 1)[0]) in ("have", "no"):
                    levels[lc(c.split(":", 1)[1])]
        bring_slots = []
        for q in rel:
            for k, s in enumerate(self.qs.stages(q)):
                for oi, item, need in self.bring_objs(q, k):
                    levels[item] |= {need, need - 1}
                    bring_slots.append((q, k, oi))
                for o in s.get("objectives", []):
                    if o.get("event") == "have" and lc(o.get("target")) in levels:
                        need = max(1, int(o.get("need", 1)))
                        levels[lc(o["target"])] |= {need, need - 1}
        items = sorted(levels)
        flags = sorted({c.split(":", 1)[1] for t in p.talks for c in t.when
                        if ":" in c and lc(c.split(":", 1)[0]) == "flag"})
        flag_dom = [[False, True] if self.flags_settable(f) else [False] for f in flags]

        said = {"lenient": set(), "staged": set()}
        said_again = {"lenient": set(), "staged": set()}
        everyday = set()
        eligible = collections.defaultdict(collections.Counter)   # talk index -> Counter(selected in its eligible states)
        eligible_any = set()
        stages_seen = collections.defaultdict(set)
        sig_done = set()
        domains = [self.qs.domain(q) for q in rel]
        for combo in itertools.product(*domains):
            assign = dict(zip(rel, combo))
            if not self.qs.consistent(assign):
                continue
            slots = [(q, k, oi) for q, k, oi in bring_slots if assign[q] == ("A", k)]
            for metv in itertools.product([False, True], repeat=len(slots)):
                met = {(q, oi): v for (q, k, oi), v in zip(slots, metv)}
                # a stage whose objectives are all met is over: a met bring needs another objective still open
                bad = False
                for (q, k, oi), v in zip(slots, metv):
                    if v and all(met.get((q, j)) for j in range(len(self.qs.stages(q)[k]["objectives"]))):
                        bad = True
                if bad:
                    continue
                for pv in itertools.product(*[sorted(levels[i]) for i in items]):
                    pocket = dict(zip(items, pv))
                    stage_sig = tuple(self.talk_stage(q, assign[q], pocket, met) for q in rel)
                    for q, s in zip(rel, stage_sig):
                        if s is not None:
                            stages_seen[q].add(s)
                    limit = self.press_limit(assign, pocket, met)
                    have_ok = self.have_rule_ok(assign, pocket)
                    for fv in itertools.product(*flag_dom):
                        fl = dict(zip(flags, fv))
                        for sun in SUN:
                            hold_sig = tuple(all(self.cond_holds(c, sun, assign, pocket, fl) for c in t.when) for t in p.talks)
                            for held0 in (0, 1):
                                sig = (stage_sig, hold_sig, held0, limit, have_ok, sun)
                                if sig in sig_done:
                                    continue
                                sig_done.add(sig)
                                ev = set()
                                local = set()
                                stage_of, holds = self.run(assign, pocket, met, fl, sun, held0, PRESSES, local,
                                                           said_again["lenient"], ev)
                                said["lenient"] |= local
                                first_pick = None
                                for t in p.talks:
                                    st = stage_of.get(lc(t.quest))
                                    if st is not None and lc(st) in [lc(s) for s in t.stages] and holds[t.index]:
                                        eligible_any.add(t.index)
                                        if first_pick is None:
                                            first_pick = self.first_pick(assign, pocket, met, fl, sun, held0)
                                        if first_pick is not None and first_pick >= 0:
                                            eligible[t.index][first_pick] += 1
                                if have_ok:
                                    local = set()
                                    self.run(assign, pocket, met, fl, sun, held0, limit, local, said_again["staged"], set())
                                    said["staged"] |= local
        # everyday talks: no quest talk, 30 talks in each sun state
        for sun in SUN + ("",):
            counts_p = Person(p.id, p.line_no)
            counts_p.first, counts_p.again, counts_p.sun = p.first, p.again, p.sun
            Sim(counts_p, self.qs, self.flags_settable, self.items_known).run({}, {}, {}, {}, sun, 0, EVERYDAY,
                                                                             set(), set(), everyday)
        return said, said_again, everyday, eligible, eligible_any, stages_seen

    def first_pick(self, assign, pocket, met, fl, sun, held0):
        # which entry the first press of this state says (for "shadowed by"); -1: the everyday talk
        local = set()
        self.run(assign, pocket, met, fl, sun, held0, 1, local, set(), set())
        return next(iter(local), -1)


# ------------------------------------------------------------------ main

def find_root(arg):
    if arg:
        return os.path.abspath(arg)
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.abspath(os.path.join(here, ".."))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--root", help="the repository (default: the parent of this script's folder)")
    ap.add_argument("--strict", action="store_true", help="exit 1 if a talk entry is never said or a remark is wrong")
    ap.add_argument("--verbose", action="store_true", help="also list each person's quest stages as this person sees them")
    args = ap.parse_args()
    root = find_root(args.root)
    content = os.path.join(root, "unreal", "Lyudmilovsk", "Content")
    if not os.path.isdir(content):
        print("quest_talk_reach: no %s (use --root <repo>)" % content)
        return 2

    with open(os.path.join(content, "UI", "quests.json"), encoding="utf-8") as f:
        qs = Quests(json.load(f))
    with open(os.path.join(content, "UI", "quest_scenes.json"), encoding="utf-8") as f:
        scenes = json.load(f)
    with open(os.path.join(content, "Localization", "quests", "strings_ru.json"), encoding="utf-8") as f:
        sru = json.load(f)
    sru = sru.get("strings", sru)
    with open(os.path.join(content, "PeopleData", "people.json"), encoding="utf-8") as f:
        pdata = json.load(f)
    people_keys = {lc(x.get("key") or x.get("id")) for x in pdata.get("people", [])}
    people = parse_dialogue(os.path.join(content, "Dialogue", "npc_dialogue.txt"))

    remarks = scenes.get("remarks", [])
    remark_ids = {r.get("id") for r in remarks if r.get("id")}
    pickup_ids = {p.get("id") for p in scenes.get("pickups", []) if p.get("id")}
    notes = set()
    for r in remarks:
        if r.get("diary"):
            notes.add(r["diary"])
    for li in scenes.get("listeners", []):
        for r in li.get("reactions", []):
            if r.get("diary"):
                notes.add(r["diary"])

    def flag_settable(f):
        # ULyudQuests::SetFlag is called for remark.<id>, pickup.<id> and diary.<note> (UI/LyudQuestDirector.cpp)
        kind, _, rest = f.partition(".")
        if kind == "remark":
            return rest in remark_ids
        if kind == "pickup":
            return rest in pickup_ids
        if kind == "diary":
            return rest in notes
        return False

    def cond_problem(c):
        if ":" not in c:
            return None if lc(c) in SUN else "'%s' is no sun state" % c
        kind, arg = c.split(":", 1)
        kind = lc(kind)
        if kind in ("done", "on"):
            if lc(arg) not in qs.defs:
                return "%s: no quest '%s'" % (c, arg)
            if lc(arg) not in qs.startable:
                return "%s: quest %s can never begin (no auto_start, start_on or reward.start)" % (c, arg)
            if kind == "done" and not qs.doneable(lc(arg)):
                return "%s: quest %s can never be done" % (c, arg)
            return None
        if kind == "flag":
            return None if flag_settable(arg) else "%s: nothing sets that flag (remark.<id>, pickup.<id>, diary.<note>)" % c
        if kind in ("have", "no"):
            return None
        return "'%s' is no known condition" % c

    never, staged_never, again_never, everyday_never, shadow_notes = [], [], [], [], []
    referenced = set()
    n_entries = 0
    seen_by = collections.OrderedDict()
    for pid, person in people.items():
        for k in person.first:
            referenced.add(k)
        for ex in person.again:
            referenced.update(ex)
        for ex, _ in person.sun.values():
            referenced.update(ex)
        for t in person.talks:
            referenced.update(t.lines)
            for ex in t.again:
                referenced.update(ex)
        n_entries += len(person.talks)
        sim = Sim(person, qs, flag_settable, None)
        said, said_again, everyday, eligible, eligible_any, stages_seen = sim.simulate()
        seen_by[person.id] = stages_seen
        for t in person.talks:
            q = lc(t.quest)
            if t.index in said["lenient"]:
                if t.index not in said["staged"]:
                    staged_never.append((t, "said only while the stage is held still: the talk itself ends the stage "
                                            "or the pocket would have ended it"))
                for i, n in enumerate(t.again_no):
                    if (t.index, i) not in said_again["lenient"]:
                        again_never.append((t, n, "never reached even holding the stage for %d talks" % PRESSES))
                    elif (t.index, i) not in said_again["staged"]:
                        again_never.append((t, n, "only if the stage stayed: the talk ends it (staged model)"))
                continue
            reason = None
            if not t.lines:
                reason = "has only .again lines (Lines is empty, StartTalk needs them)"
            elif q not in qs.defs:
                reason = "no quest '%s'" % t.quest
            elif q not in qs.startable:
                reason = "quest %s can never begin" % t.quest
            else:
                possible = stages_seen.get(q, set())
                bad = [s for s in t.stages if lc(s) not in possible]
                if len(bad) == len(t.stages):
                    why = []
                    for s in t.stages:
                        sl = lc(s)
                        ids = qs.stage_ids(q)
                        if sl == "done":
                            why.append("done: quest can never be done")
                        elif sl.endswith("_empty") and sl[:-6] in ids:
                            why.append("%s: no bring objective to %s in that stage" % (s, person.id))
                        elif sl in ids:
                            why.append("%s: TalkStage never gives it to %s" % (s, person.id))
                        else:
                            why.append("%s: quest %s has no such stage" % (s, t.quest))
                    reason = "; ".join(why)
                if reason is None:
                    probs = [x for x in (cond_problem(c) for c in t.when) if x]
                    if probs:
                        reason = "condition unsatisfiable: " + "; ".join(probs)
                if reason is None and t.index not in eligible_any:
                    reason = "conditions never hold while %s is on %s (the reward.start chain, the pocket a bring stage " \
                             "needs, or contradicting conditions)" % (t.quest, ",".join(t.stages))
                if reason is None:
                    by = eligible[t.index].most_common()
                    if by:
                        names = ", ".join("%s (npc_dialogue.txt:%d)" % (person.talks[i].name(), person.talks[i].lines_no or person.talks[i].line_no)
                                          for i, _ in by[:3] if i != t.index)
                        reason = "shadowed: whenever it could be said, StartTalk picks %s" % (names or "another entry")
                    else:
                        reason = "eligible, but never picked (an aside before the first conversation, or spent)"
            never.append((t, reason))
        for name in ["again%d" % i for i in range(len(person.again))] + ["sun." + s for s in person.sun]:
            if name not in everyday:
                if name.startswith("sun."):
                    why = "'%s' is no sun state (day, low, night, cloudy)" % name[4:] if name[4:] not in SUN else \
                        "no everyday 'again' exchanges: StartTalk returns the first words before it looks at the sun"
                else:
                    why = "never reached within %d everyday talks" % EVERYDAY
                everyday_never.append((person.id, name, why))
        for t in person.talks:
            for n, keys in t.overwritten:
                shadow_notes.append("npc_dialogue.txt:%d [%s] %s: plain lines replaced by line %d (Lines is overwritten)" %
                                    (n, person.id, t.name(), t.lines_no))

    # ---- remarks
    remark_problems = []
    for r in remarks:
        q = lc(r.get("quest"))
        for li in [r.get("lines", [])]:
            referenced.update("line." + k if not k.startswith("line.") else k for k in li)
        if q not in qs.defs:
            remark_problems.append("quest_scenes.json remark %s: no quest '%s'" % (r.get("id"), r.get("quest")))
            continue
        for s in r.get("stages", []):
            if lc(s) == "done":
                if not qs.doneable(q):
                    remark_problems.append("quest_scenes.json remark %s: on done of %s, which can never be done" % (r.get("id"), q))
            elif lc(s) not in qs.stage_ids(q):
                remark_problems.append("quest_scenes.json remark %s: quest %s has no stage '%s' (StageId never gives it)" % (r.get("id"), q, s))
            elif q not in qs.startable:
                remark_problems.append("quest_scenes.json remark %s: quest %s can never begin" % (r.get("id"), q))
    for li in scenes.get("listeners", []):
        for r in li.get("reactions", []):
            referenced.update("line." + k if not k.startswith("line.") else k for k in r.get("lines", []))

    # ---- orphans: line.<person>. keys of the quests' strings that nothing names
    prefixes = {lc(p.id) for p in people.values()} | people_keys | {lc(r.get("person")) for r in remarks} | \
               {lc(li.get("person")) for li in scenes.get("listeners", [])}
    prefixes.discard("")
    elsewhere = []
    for pattern in ("Source/**/*.cpp", "Source/**/*.h", "Content/**/*.json", "Content/**/*.txt", "Content/**/*.py"):
        for path in glob.glob(os.path.join(root, "unreal", "Lyudmilovsk", pattern), recursive=True):
            if os.sep + "Localization" + os.sep in path or path.endswith("npc_dialogue.txt") or path.endswith("quest_scenes.json"):
                continue
            try:
                with open(path, encoding="utf-8", errors="replace") as f:
                    elsewhere.append(f.read())
            except OSError:
                pass
    blob = "\n".join(elsewhere)
    orphans = []
    for key in sru:
        if not key.startswith("line."):
            continue
        speaker = key.split(".")[1] if key.count(".") >= 2 else ""
        if speaker not in prefixes or key in referenced:
            continue
        if key in blob or key[5:] in blob:
            continue
        orphans.append(key)

    # ---- the report
    def out(title, rows):
        print("== %s: %d" % (title, len(rows)))
        for r in rows:
            print("  " + r)

    out("1. talk entries never said", ["%s: %s" % (t.where(), why) for t, why in never])
    out("1b. talk entries said only if the stage were held still (staged model)", ["%s: %s" % (t.where(), why) for t, why in staged_never])
    out("2. a talk entry's .again exchange never said", ["npc_dialogue.txt:%d [%s] %s.again: %s" % (n, t.section, t.name(), why) for t, n, why in again_never])
    out("2b. everyday again/sun lines never said in %d talks" % EVERYDAY, ["[%s] %s: %s" % (a, b, c) for a, b, c in everyday_never])
    out("2c. plain lines written twice", shadow_notes)
    out("3. orphan line keys (Localization/quests/strings_ru.json)", orphans)
    out("4. remarks on stages that never come", remark_problems)
    if args.verbose:
        for sid, seen in seen_by.items():
            for q, st in seen.items():
                print("  [%s] %s: TalkStage gives %s" % (sid, q, ", ".join(sorted(st))))
    n_never = len(never)
    print("quest_talk_reach: %d people, %d entries, %d never said, %d orphans" % (len(people), n_entries, n_never, len(orphans)))
    if args.strict and (never or remark_problems):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
