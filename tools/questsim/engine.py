"""A plain-Python copy of the quest state machine in ULyudQuests (Private/UI/LyudQuests.cpp).

Every rule cites the C++ lines of main (d2db630) it mirrors. A tree's own LyudQuests.cpp may carry extra rules (the
ornithologist branch: no skip-ahead on talk, "bring <item>@<Npc>", bare "*" targets, finishing a stage met as it begins);
Rules.detect() reads the source and switches each one on only where that tree has it, so the same simulator runs on any branch.

Unreal compares FName and FString case-insensitively (FName ==, FString ==, StartsWith, TArray<FString>::Contains): so does
this copy (every id is compared lowered).
"""

import re


class Rules:
    """Which variant of the engine a tree has."""

    def __init__(self, talk_no_skip=False, bring=False, star_any=False, chain_met=False):
        self.talk_no_skip = talk_no_skip  # skip-ahead never on a talk event
        self.bring = bring                # "bring <item>@<Npc>" objectives, met by a talk with the item in the pocket
        self.star_any = star_any          # a bare "*" target matches anything (main: FString::StartsWith("") is false)
        self.chain_met = chain_met        # a stage already met when it begins finishes at once (after an objective-driven finish)

    @staticmethod
    def detect(cpp_text):
        return Rules(
            talk_no_skip=bool(re.search(r'!\(E\.Get<0>\(\)\s*==\s*TEXT\("talk"\)\)', cpp_text)),
            bring=bool(re.search(r'O\.Event\s*==\s*TEXT\("bring"\)', cpp_text)),
            star_any=bool(re.search(r'Prefix\.IsEmpty\(\)\s*\|\|', cpp_text)),
            chain_met=bool(re.search(r'may be met already', cpp_text)),
        )

    def names(self):
        return [n for n in ('talk_no_skip', 'bring', 'star_any', 'chain_met') if getattr(self, n)]


def low(s):
    return (s or '').lower()


def event_matches(want, got):
    """LyudQuests.cpp:164-173 EventMatches: equal, or one of 'a|b' (ParseIntoArray drops empty pieces)."""
    if low(want) == low(got):
        return True
    alts = [a for a in want.split('|') if a]
    return len(alts) > 1 and low(got) in [low(a) for a in alts]


def target_matches(want, got, rules):
    """LyudQuests.cpp:175-186 TargetMatches: empty = any, 'x*' = prefix, else equal."""
    if want == '':
        return True
    if want.endswith('*'):
        prefix = want[:-1]
        if prefix == '':
            # FString::StartsWith is false for an empty prefix: main's bare "*" matches nothing
            return rules.star_any
        return low(got).startswith(low(prefix))
    return low(want) == low(got)


class Objective:
    def __init__(self, o):
        # LyudQuests.cpp:51-69 ParseObjective
        self.id = o.get('id', '')
        self.event = o.get('event', '')
        self.target = o.get('target', '')
        need = o.get('need', 1)
        self.need = max(1, int(need)) if isinstance(need, (int, float)) else 1
        self.place = o.get('place', self.target if self.event == 'reach' else '')


class Stage:
    def __init__(self, s):
        self.id = s.get('id', '')
        self.objectives = [Objective(o) for o in s.get('objectives', [])]
        r = s.get('reward', {}) or {}
        self.rubles = int(r.get('rubles', 0) or 0)
        self.items = [(k, int(v)) for k, v in (r.get('items', {}) or {}).items()]
        self.starts = list(r.get('start', []) or [])


class QuestDef:
    def __init__(self, q):
        # LyudQuests.cpp:86-131
        self.id = q.get('id', '')
        self.main = q.get('kind') == 'main'
        self.auto_start = bool(q.get('auto_start', False))
        so = q.get('start_on')
        self.start_event = so.get('event', '') if so else ''
        self.start_target = so.get('target', '') if so else ''
        self.stages = [Stage(s) for s in q.get('stages', [])]


def load_defs(quests_json):
    # a quest without stages is dropped (LyudQuests.cpp:128-131)
    return [d for d in (QuestDef(q) for q in quests_json.get('quests', [])) if d.stages]


INACTIVE, ACTIVE, DONE = 'inactive', 'active', 'done'


class Progress:
    def __init__(self, quest):
        self.quest = quest
        self.state = INACTIVE
        self.stage = 0
        self.counts = []


class Sim:
    """ULyudQuests + the bits of ULyudInventory it talks to (Add -> get, Remove -> use)."""

    def __init__(self, defs, rules, start_items=None):
        self.defs = defs
        self.rules = rules
        self.start_items = dict(start_items or {})
        self.reset()

    # ------------------------------------------------------------- inventory (LyudInventory.cpp:354-386)
    def count(self, item):
        return self.inv.get(low(item), 0)

    def inv_add(self, item, n):
        if n > 0:
            self.inv[low(item)] = self.count(item) + n
            self.handle_event('get', item, n)

    def inv_remove(self, item, n):
        if self.count(item) < n:
            return False
        self.inv[low(item)] -= n
        if self.inv[low(item)] <= 0:
            del self.inv[low(item)]
        self.handle_event('use', item, n)
        return True

    # ------------------------------------------------------------- quests
    def find_def(self, quest):
        for d in self.defs:  # FindByPredicate: the first of a duplicated id wins (LyudQuests.cpp:195-198)
            if low(d.id) == low(quest):
                return d
        return None

    def find_progress(self, quest):
        for p in self.progress:
            if low(p.quest) == low(quest):
                return p
        return None

    def state(self, quest):
        p = self.find_progress(quest)
        return p.state if p else INACTIVE

    def progress_of(self, quest):
        p = self.find_progress(quest)
        if not p:
            p = Progress(quest)
            self.progress.append(p)
        return p

    def reset(self):
        # LyudQuests.cpp:299-322 (+ the inventory's new game: the clothes and lyud.StartFeed handfuls)
        self.progress = []
        self.queue = []
        self.handling = False
        self.inside = set()
        self.rubles = 0
        self.trace = []
        self.tracked = ''
        self.inv = {low(k): v for k, v in self.start_items.items()}
        for d in self.defs:
            if d.auto_start:
                self.start(d.id)

    def start(self, quest):
        # LyudQuests.cpp:355-374
        d = self.find_def(quest)
        if not d or self.state(quest) != INACTIVE:
            return False
        p = self.progress_of(quest)
        p.state = ACTIVE
        p.stage = 0
        # :366-369 the first quest, or a main one while the tracked one is not active
        if not self.tracked or (d.main and self.state(self.tracked) != ACTIVE):
            self.tracked = d.id
        self.trace.append(('start', d.id))
        self.begin_stage(p, d)
        return True

    def begin_stage(self, p, d):
        # LyudQuests.cpp:376-404
        if not (0 <= p.stage < len(d.stages)):
            p.counts = []
            return
        s = d.stages[p.stage]
        p.counts = [0] * len(s.objectives)
        self.trace.append(('begin', d.id, p.stage))
        for i, o in enumerate(s.objectives):
            if o.event == 'have':
                p.counts[i] = self.count(o.target)  # Inv->Count(*O.Target): literal, no '*' or '|'
            elif o.event == 'reach' and low(o.target) in self.inside:
                p.counts[i] = o.need

    def finish_stage(self, p, d, how):
        # LyudQuests.cpp:406-452
        if not d.stages:
            return
        p.stage = max(0, min(p.stage, len(d.stages) - 1))
        s = d.stages[p.stage]
        self.trace.append(('finish', d.id, p.stage, how))
        for item, n in s.items:
            self.inv_add(item, n)  # reports get: queued while an event is handled, handled at once otherwise
        if s.rubles > 0:
            self.rubles += s.rubles
        if p.stage + 1 < len(d.stages):
            p.stage += 1
            self.begin_stage(p, d)
        else:
            p.state = DONE
            p.counts = []
            self.trace.append(('done', d.id))
            if low(self.tracked) == low(d.id):  # :440-444
                self.tracked = ''
                self.tracked = self.shown_quest()
        for q in list(s.starts):
            self.start(q)

    def shown_quest(self):
        # LyudQuests.cpp:629-652
        if self.tracked and self.state(self.tracked) == ACTIVE:
            return self.tracked
        any_q = ''
        for p in self.progress:
            if p.state == ACTIVE:
                d = self.find_def(p.quest)
                if d and d.main:
                    return p.quest
                if not any_q:
                    any_q = p.quest
        return any_q

    def stage_met(self, p, d):
        s = d.stages[p.stage]
        return len(s.objectives) > 0 and len(p.counts) == len(s.objectives) and all(
            c >= o.need for c, o in zip(p.counts, s.objectives))

    def handle_event(self, event, target='', n=1):
        # LyudQuests.cpp:454-551
        self.queue.append((event, target, n))
        if self.handling:
            return
        self.handling = True
        try:
            while self.queue:
                ev, tgt, num = self.queue.pop(0)
                self.trace.append(('event', ev, tgt, num))
                # quests that start on this event (:468-475): StartEvent == Event, no alternatives
                for d in self.defs:
                    if d.start_event and low(d.start_event) == low(ev) and target_matches(d.start_target, tgt, self.rules) \
                            and self.state(d.id) == INACTIVE:
                        self.start(d.id)
                qi = 0
                while qi < len(self.progress):  # by index: a finished stage may add quests (:476-477)
                    p = self.progress[qi]
                    qi += 1
                    if p.state != ACTIVE:
                        continue
                    d = self.find_def(p.quest)
                    if not d or not (0 <= p.stage < len(d.stages)):
                        continue
                    # skip-ahead (:488-505): a later stage wants this event -> the stages before it are done
                    if not (self.rules.talk_no_skip and low(ev) == 'talk'):
                        for later in range(p.stage + 1, len(d.stages)):
                            if any(o.event != 'have' and event_matches(o.event, ev) and target_matches(o.target, tgt, self.rules)
                                   for o in d.stages[later].objectives):
                                self.trace.append(('skip', d.id, p.stage, later, ev, tgt))
                                while p.state == ACTIVE and p.stage < later:
                                    self.finish_stage(p, d, 'skip')
                                break
                    if p.state != ACTIVE:
                        continue
                    s = d.stages[p.stage]
                    if len(p.counts) != len(s.objectives):  # SetNumZeroed (:511-515)
                        p.counts = (p.counts + [0] * len(s.objectives))[:len(s.objectives)]
                    all_met = True
                    for i, o in enumerate(s.objectives):
                        have = o.event == 'have'
                        if (event_matches(o.event, ev) or (have and low(ev) == 'get')) and target_matches(o.target, tgt, self.rules):
                            p.counts[i] += num
                        elif have and low(ev) == 'use' and target_matches(o.target, tgt, self.rules):
                            p.counts[i] = max(0, p.counts[i] - num)
                        if self.rules.bring and o.event == 'bring' and low(ev) == 'talk' and p.counts[i] < o.need:
                            item, sep, to = o.target.partition('@')
                            if sep and low(to) == low(tgt) and self.count(item) >= o.need:
                                self.inv_remove(item, o.need)  # its use event waits in the queue
                                p.counts[i] = o.need
                        all_met &= p.counts[i] >= o.need
                    if all_met:
                        self.finish_stage(p, d, 'objectives')
                        if self.rules.chain_met:
                            while p.state == ACTIVE and 0 <= p.stage < len(d.stages) and self.stage_met(p, d):
                                self.finish_stage(p, d, 'met')
        finally:
            self.handling = False

    def advance(self, quest):
        # LyudQuests.cpp:553-562 (Quest.Advance): FinishStage outside HandleEvent
        p = self.find_progress(quest)
        d = self.find_def(quest)
        if p and d and p.state == ACTIVE and d.stages:
            self.finish_stage(p, d, 'advance')

    def reach(self, place):
        # LyudQuests.cpp:597-627 UpdatePosition: in by the radius (the hero stands in one place at a time here)
        if low(place) not in self.inside:
            self.inside = {low(place)}
            self.handle_event('reach', place, 1)
