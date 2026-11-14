#!/usr/bin/env python3
"""Offline quest test suite (no engine): validates the quest data and plays every quest through a copy of the quest
state machine (engine.py, a mirror of Private/UI/LyudQuests.cpp).

    python3 tools/questsim/questsim.py [TREE] [-v]

TREE is a repository root (default: this one), so a branch can be checked from an exported tree
(git archive <ref> unreal/Lyudmilovsk docs tools | tar -x -C <dir>). Prints one 'ok   <check>' / 'FAIL <check>' line per
check (the problems indented under it), 'WARN' lines for engine behaviour worth knowing that is not a data error, and ends
with 'all ok' or the failure count (exit 1).
"""

import glob
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import engine  # noqa: E402
from engine import low  # noqa: E402

LANGS = ('ru', 'en', 'ja')
NO_TARGET_EVENTS = {'leave_computer', 'walk', 'bird', 'corvid_feed_thrown'}


def read(path):
    with open(path, encoding='utf-8') as f:
        return f.read()


def read_json(path):
    with open(path, encoding='utf-8') as f:
        return json.load(f)


class Tree:
    """Everything the checks need from one checkout."""

    def __init__(self, root):
        self.root = os.path.abspath(root)
        self.game = os.path.join(self.root, 'unreal', 'Lyudmilovsk')
        self.content = os.path.join(self.game, 'Content')
        self.src = os.path.join(self.game, 'Source', 'Lyudmilovsk')
        self.quests_json = read_json(os.path.join(self.content, 'UI', 'quests.json'))
        self.map_json = read_json(os.path.join(self.content, 'UI', 'map.json'))
        self.quests_cpp = read(os.path.join(self.src, 'Private', 'UI', 'LyudQuests.cpp'))
        self.rules = engine.Rules.detect(self.quests_cpp)
        self.defs = engine.load_defs(self.quests_json)
        self.cpp = {}
        for p in glob.glob(os.path.join(self.src, '**', '*.cpp'), recursive=True) + glob.glob(os.path.join(self.src, '**', '*.h'), recursive=True):
            self.cpp[os.path.relpath(p, self.src)] = read(p)
        self.places = {low(p['id']): p for p in self.map_json.get('places', [])}
        self.load_strings()
        self.load_items()
        self.load_dialogue()
        self.load_world()
        self.load_events()

    def opt_json(self, *rel):
        p = os.path.join(self.content, *rel)
        return read_json(p) if os.path.exists(p) else None

    # LyudText.cpp:80-107: strings_<lang>.json plus the same file in every subfolder of Content/Localization
    def load_strings(self):
        self.strings = {}
        base = os.path.join(self.content, 'Localization')
        for lang in LANGS:
            table = {}
            files = [os.path.join(base, 'strings_%s.json' % lang)] + sorted(glob.glob(os.path.join(base, '*', 'strings_%s.json' % lang)))
            for f in files:
                if os.path.exists(f):
                    for k, v in read_json(f).items():
                        table.setdefault(k, v)
            self.strings[lang] = table

    # LyudInventory.cpp MakeDefs: the item table is code
    def load_items(self):
        inv = self.cpp.get(os.path.join('Private', 'LyudInventory.cpp'), '')
        body = inv.split('MakeDefs()', 1)[1].split('AllDefs()', 1)[0] if 'MakeDefs()' in inv else inv
        items = set(re.findall(r'\b(?:Drink|Good|FeedGood|Wear|Bait)\(TEXT\("(\w+)"\)', body))
        items |= set(re.findall(r'\.Id = TEXT\("(\w+)"\)', body))
        items |= set(re.findall(r'TEXT\("(find_\w+)"\)', body))
        self.feeds = set(re.findall(r'\bFeedGood\(TEXT\("(\w+)"\)', body)) | {'bird_feed'}
        fish = self.cpp.get(os.path.join('Private', 'Fishing', 'FishingModel.cpp'), '')
        if 'LyudFishing::ItemOf' in body:
            items |= {'fish_' + f.lower() for f in re.findall(r'\{\s*"(\w+)",\s*D::', fish)}
        self.items = {low(i) for i in items}
        # a new game's pocket (LyudInventory.cpp: the clothes, lyud.StartFeed handfuls of bird_feed)
        m = re.search(r'Clothes\s*:\s*\{([^}]*)\}', inv)
        self.start_items = {c: 1 for c in re.findall(r'TEXT\("(\w+)"\)', m.group(1))} if m else {}
        m = re.search(r'CVarStartFeed\(TEXT\("[^"]+"\),\s*(\d+)', inv)
        if m and int(m.group(1)) > 0:
            self.start_items['bird_feed'] = int(m.group(1))

    # NpcDialogue.cpp: [Id] sections, 'key = a | b' lines
    def load_dialogue(self):
        self.npcs = {}
        self.dialogue = []  # (npc, key, [line keys], lineno)
        path = os.path.join(self.content, 'Dialogue', 'npc_dialogue.txt')
        npc = None
        for n, raw in enumerate(read(path).splitlines(), 1):
            line = raw.strip()
            if not line or line.startswith('#'):
                continue
            if line.startswith('[') and line.endswith(']'):
                npc = line[1:-1]
                self.npcs[low(npc)] = npc
                continue
            if '=' in line and npc:
                k, v = line.split('=', 1)
                self.dialogue.append((npc, k.strip(), [x.strip() for x in v.split('|') if x.strip()], n))

    def load_world(self):
        sp = self.opt_json('BirdsData', 'species.json') or {}
        self.species = {low(s['id']) for s in sp.get('species', [])}
        fe = self.opt_json('PeopleData', 'festivals.json') or {}
        self.festivals = {}
        for f in fe.get('festivals', []):
            self.festivals[low(f['id'])] = {low(p['id']): [low(s) for s in p.get('show', [])] for p in f.get('program', [])}
        fest_cpp = self.cpp.get(os.path.join('Private', 'Festival', 'LyudFestival.cpp'), '')
        # shows that are actors (SpawnShow's Show == TEXT("...")): only those can be watched (TickHero, Shows map)
        self.actor_shows = set(re.findall(r'Show == TEXT\("(\w+)"\)', fest_cpp)) - {'stage_lights'}
        self.people = set()
        for f in ('people.json', 'townsfolk.json'):
            d = self.opt_json('PeopleData', f) or {}
            self.people |= {p['key'] for p in d.get('people', []) if 'key' in p}
        self.teleports = set()
        for f in glob.glob(os.path.join(self.content, 'Dev', 'teleports', '*.json')):
            self.teleports |= {low(t['id']) for t in read_json(f).get('teleports', []) if 'id' in t}
        plan = self.opt_json('City', 'city_plan.json') or {}
        for k in ('plots', 'spots', 'promenade_units'):
            self.teleports |= {low(o['id']) for o in plan.get(k, []) if isinstance(o, dict) and 'id' in o}
        self.teleports |= set(self.places)
        inter = self.opt_json('City', 'interiors.json') or {}
        self.plots = {low(t['plot']) for t in inter.get('things', []) if t.get('plot')}
        acts = inter.get('acts', {})
        used = {t.get('act') for t in inter.get('things', [])}
        self.act_events = {}
        for aid, a in acts.items():
            if aid in used and a.get('event'):
                self.act_events.setdefault(low(a['event']), set()).add(low(a.get('target', '')))
        self.scenes = self.opt_json('UI', 'quest_scenes.json')
        cmds = set()
        for text in self.cpp.values():
            cmds |= set(re.findall(r'(?:FAutoConsoleCommand\w*|TAutoConsoleVariable<[^>]*>|FAutoConsoleVariableRef)\s+\w+\(\s*TEXT\("([^"]+)"\)', text))
        self.commands = {low(c) for c in cmds}

    # the events the game reports: every TEXT("...") on a line that calls Notify / HandleEvent / HeroEvent
    def load_events(self):
        self.emitted = {}
        for rel, text in self.cpp.items():
            if os.path.basename(rel) in ('LyudDev.cpp',):
                continue  # the developer mode replays any objective's event: not a source
            for n, line in enumerate(text.splitlines(), 1):
                if re.search(r'\b(Notify|HandleEvent|HeroEvent)\(', line) and 'void ' not in line:
                    for word in re.findall(r'TEXT\("([a-z_]+)"\)', line):
                        self.emitted.setdefault(word, '%s:%d' % (rel, n))
                    # City/LyudCityThings.cpp reports a model struct's own event (Name(A.Event)): the struct's default
                    # (CityThingsModel.h FTopicAnswer: std::string Event = "ask")
                    if re.search(r'Name\(\w+\.Event\)', line):
                        for other in self.cpp.values():
                            for word in re.findall(r'std::string\s+Event\s*=\s*"([a-z_]+)"', other):
                                self.emitted.setdefault(word, '%s:%d (the struct\'s default event)' % (rel, n))
                    if 'Name(O.Event)' in line:  # City/LyudCityThings.cpp: the acts' own events (interiors.json)
                        for ev in self.act_events:
                            self.emitted.setdefault(ev, '%s:%d (interiors.json acts)' % (rel, n))


# ===================================================================== checks

class Report:
    def __init__(self, verbose):
        self.fails = 0
        self.checks = 0
        self.verbose = verbose

    def check(self, name, problems):
        self.checks += 1
        if problems:
            self.fails += 1
            print('FAIL %s' % name)
            for p in problems[:40]:
                print('       %s' % p)
            if len(problems) > 40:
                print('       ... %d more' % (len(problems) - 40))
        else:
            print('ok   %s' % name)

    def warn(self, text):
        print('WARN %s' % text)


def check_schema(t):
    probs = []
    for i, q in enumerate(t.quests_json.get('quests', [])):
        qid = q.get('id')
        where = 'quest #%d %s' % (i, qid)
        if not isinstance(qid, str) or not re.fullmatch(r'[a-z0-9_]+', qid or ''):
            probs.append('%s: id must be lowercase [a-z0-9_]' % where)
        if q.get('kind') not in ('main', 'side'):
            probs.append('%s: kind must be main or side, is %r' % (where, q.get('kind')))
        if not q.get('stages'):
            probs.append('%s: no stages (LyudQuests.cpp:128 drops it)' % where)
        so = q.get('start_on')
        if so is not None and (not isinstance(so, dict) or not so.get('event')):
            probs.append('%s: start_on needs an event' % where)
        if so and '|' in so.get('event', ''):
            probs.append('%s: start_on event %r: alternatives are not supported there (LyudQuests.cpp:471 compares ==)' % (where, so['event']))
        if q.get('auto_start') and so:
            probs.append('%s: both auto_start and start_on' % where)
        for s in q.get('stages', []):
            sw = '%s stage %s' % (where, s.get('id'))
            if not re.fullmatch(r'[a-z0-9_]+', s.get('id') or ''):
                probs.append('%s: stage id must be lowercase [a-z0-9_]' % sw)
            if not s.get('objectives'):
                probs.append('%s: no objectives (it would finish on any event)' % sw)
            unknown = set(s) - {'id', 'objectives', 'reward', 'dev', 'comment', 'no_skip'}
            if unknown:
                probs.append('%s: unknown fields %s' % (sw, sorted(unknown)))
            for o in s.get('objectives', []):
                ow = '%s objective %s' % (sw, o.get('id'))
                if not re.fullmatch(r'[a-z0-9_]+', o.get('id') or ''):
                    probs.append('%s: objective id must be lowercase [a-z0-9_]' % ow)
                if not o.get('event'):
                    probs.append('%s: no event' % ow)
                need = o.get('need', 1)
                if not isinstance(need, int) or need < 1:
                    probs.append('%s: need must be an integer >= 1, is %r' % (ow, need))
                unknown = set(o) - {'id', 'event', 'target', 'need', 'place', 'comment'}
                if unknown:
                    probs.append('%s: unknown fields %s' % (ow, sorted(unknown)))
            r = s.get('reward')
            if r is not None:
                unknown = set(r) - {'rubles', 'items', 'start'}
                if unknown:
                    probs.append('%s: unknown reward fields %s' % (sw, sorted(unknown)))
                for k, v in (r.get('items') or {}).items():
                    if not isinstance(v, int) or v < 1:
                        probs.append('%s: reward item %s count %r' % (sw, k, v))
    return probs


def check_unique(t):
    probs = []
    seen = set()
    for d in t.defs:
        if low(d.id) in seen:
            probs.append('quest id %s twice (FindDef finds only the first)' % d.id)
        seen.add(low(d.id))
        sids = [s.id for s in d.stages]
        for s in set(sids):
            if sids.count(s) > 1:
                probs.append('%s: stage id %s twice (same journal key quest.%s.%s.log)' % (d.id, s, d.id, s))
        for s in d.stages:
            oids = [o.id for o in s.objectives]
            for o in set(oids):
                if oids.count(o) > 1:
                    probs.append('%s.%s: objective id %s twice in one stage' % (d.id, s.id, o))
    pids = [low(p['id']) for p in t.map_json.get('places', [])]
    for p in set(pids):
        if pids.count(p) > 1:
            probs.append('map.json place %s twice' % p)
    return probs


def objective_keys(d):
    keys = ['quest.%s.title' % d.id, 'quest.%s.desc' % d.id, 'quest.%s.done' % d.id]
    for s in d.stages:
        keys.append('quest.%s.%s.log' % (d.id, s.id))
        for o in s.objectives:
            keys.append('quest.%s.%s.obj' % (d.id, o.id))
    return keys


def check_text(t):
    probs = []
    keys = set()
    for d in t.defs:
        keys |= set(objective_keys(d))
    keys.add('ui.journal.progress')
    keys.add('ui.journal.found')
    for pid in t.places:
        keys.add('place.%s.name' % t.places[pid]['id'])
    for d in t.defs:
        for s in d.stages:
            for item, _ in s.items:
                keys.add('item.%s.name' % item)
    for k in sorted(keys):
        missing = [lang for lang in LANGS if not t.strings[lang].get(k)]
        if missing:
            probs.append('%s missing in %s' % (k, ','.join(missing)))
    # placeholders: the same in every language
    for k in sorted(keys):
        ph = {lang: sorted(set(re.findall(r'\{\d+\}', t.strings[lang].get(k, '')))) for lang in LANGS}
        if any(t.strings[lang].get(k) for lang in LANGS) and len({tuple(v) for lang, v in ph.items() if t.strings[lang].get(k)}) > 1:
            probs.append('%s placeholders differ: %s' % (k, ph))
    return probs


def concrete(target):
    return target[:-1] if target.endswith('*') else target


def target_problem(t, ev, target):
    """Why a game event 'ev' never carries 'target' (None if it can)."""
    def any_of(pool, tgt):
        if tgt == '*' or tgt == '':
            return True
        if tgt.endswith('*'):
            return any(x.startswith(low(tgt[:-1])) for x in pool)
        return low(tgt) in pool

    e = low(ev)
    if e in NO_TARGET_EVENTS:
        return None if target == '' else 'event %s carries no target' % ev
    if e == 'talk':
        return None if any_of(set(t.npcs), target) else 'talk %s: no [%s] in npc_dialogue.txt' % (target, target)
    if e == 'reach':
        p = t.places.get(low(target))
        if not p:
            return 'reach %s: no such place in map.json' % target
        if not p.get('r'):
            return 'reach %s: the place has radius 0, UpdatePosition (LyudQuests.cpp:602) never reports it' % target
        return None
    if e in ('get', 'have', 'use', 'wear', 'feeder_fill'):
        if '|' in target:
            return '%s %s: a target has no alternatives' % (ev, target)
        if e == 'have' and '*' in target:
            return 'have %s: BeginStage counts the pocket by the literal id (LyudQuests.cpp:395), a wildcard counts 0' % target
        return None if any_of(t.items, target) else '%s %s: no such item (LyudInventory.cpp MakeDefs)' % (ev, target)
    if e == 'bring':
        item, sep, npc = target.partition('@')
        if not sep:
            return 'bring %s: needs <item>@<Npc>' % target
        if low(item) not in t.items:
            return 'bring %s: no item %s' % (target, item)
        if low(npc) not in t.npcs:
            return 'bring %s: no [%s] in npc_dialogue.txt' % (target, npc)
        return None
    if e == 'feeder_fill_at':
        item, sep, place = target.partition('@')
        if not sep or low(item) not in t.items or low(place) not in t.places:
            return 'feeder_fill_at %s: needs <feed item>@<map place>' % target
        if not t.places[low(place)].get('r'):
            return 'feeder_fill_at %s: place has radius 0 (the hero is never inside it)' % target
        return None
    if e in ('feeder_visit', 'bird_seen', 'bird_tamed', 'corvid_snatch', 'corvid_gift'):
        return None if any_of(t.species, target) else '%s %s: no such species (BirdsData/species.json)' % (ev, target)
    if e == 'bird_call':
        sp, sep, who = target.partition('.')
        listeners = {low(li.get('person', '')) for li in (t.scenes or {}).get('listeners', [])}
        if not sep or low(sp) not in t.species:
            return 'bird_call %s: needs <Species>.<person>' % target
        if low(who) not in listeners:
            return 'bird_call %s: %s is not a listener in quest_scenes.json' % (target, who)
        return None
    if e in ('corvid_gift_taken', 'corvid_store_dug', 'corvid_find'):
        return None if target == '' or any_of(t.items, target) else '%s %s: no such item' % (ev, target)
    if e == 'festival':
        return None if any_of(set(t.festivals), target) else 'festival %s: no such festival (festivals.json)' % target
    if e == 'festival_visit':
        f, _, ph = low(target).partition('.')
        if f not in t.festivals or (ph and ph not in t.festivals[f]):
            return 'festival_visit %s: no festival.phase like that (festivals.json program)' % target
        return None
    if e == 'festival_watch':
        f, sep, show = low(target).partition('.')
        if not sep:
            show, f = f, None
        if show not in t.actor_shows:
            return 'festival_watch %s: %s is not a show the hero can watch (LyudFestival.cpp SpawnShow)' % (target, show)
        if f is not None and (f not in t.festivals or not any(show in shows for shows in t.festivals[f].values())):
            return 'festival_watch %s: festival %s has no %s show' % (target, f, show)
        return None
    if e == 'festival_lantern':
        return None if low(target) in ('sky_lantern', 'water_lantern') else 'festival_lantern %s: sky_lantern or water_lantern' % target
    if e == 'festival_wish':
        return None if low(target) == 'tanabata' else 'festival_wish %s: only tanabata' % target
    if e == 'visit':
        return None if any_of(t.plots, target) else 'visit %s: no plot with things (City/interiors.json)' % target
    if e in t.act_events:
        return None if any_of(t.act_events[e], target) else '%s %s: no act in City/interiors.json with that target' % (ev, target)
    return None


def check_events(t):
    probs = []
    pseudo = {'have'} | ({'bring'} if t.rules.bring else set())
    for d in t.defs:
        uses = []
        if d.start_event:
            uses.append(('start_on', d.start_event, d.start_target))
        for s in d.stages:
            for o in s.objectives:
                for alt in [a for a in o.event.split('|') if a]:
                    uses.append(('%s.%s' % (s.id, o.id), alt, o.target))
        for where, ev, tgt in uses:
            if low(ev) not in pseudo and low(ev) not in t.emitted:
                probs.append('%s %s: event %s is never reported by the game (no Notify/HandleEvent with it)' % (d.id, where, ev))
                continue
            if low(ev) == 'bring' and not t.rules.bring:
                probs.append('%s %s: bring needs the engine rule (not in this LyudQuests.cpp)' % (d.id, where))
    return probs


def check_targets(t):
    probs = []
    for d in t.defs:
        if d.start_event:
            p = target_problem(t, d.start_event, d.start_target)
            if p:
                probs.append('%s start_on: %s' % (d.id, p))
        for s in d.stages:
            for o in s.objectives:
                alts = [a for a in o.event.split('|') if a]
                for alt in alts:
                    p = target_problem(t, alt, o.target)
                    if p:
                        probs.append('%s.%s.%s: %s' % (d.id, s.id, o.id, p))
                if o.place and low(o.place) not in t.places:
                    probs.append('%s.%s.%s: place %s is not in map.json (no marker)' % (d.id, s.id, o.id, o.place))
            for item, _ in s.items:
                if low(item) not in t.items:
                    probs.append('%s.%s: reward item %s does not exist' % (d.id, s.id, item))
    return probs


def check_starts(t):
    probs = []
    ids = {low(d.id) for d in t.defs}
    graph = {}
    started_by_reward = set()
    for d in t.defs:
        graph[low(d.id)] = []
        for s in d.stages:
            for q in s.starts:
                if low(q) not in ids:
                    probs.append('%s.%s: reward starts %s, no such quest' % (d.id, s.id, q))
                else:
                    graph[low(d.id)].append(low(q))
                    started_by_reward.add(low(q))
    # cycles
    color = {}

    def dfs(n, path):
        color[n] = 1
        for m in graph.get(n, []):
            if color.get(m) == 1:
                probs.append('reward start cycle: %s' % ' -> '.join(path + [n, m]))
            elif not color.get(m):
                dfs(m, path + [n])
        color[n] = 2
    for n in graph:
        if not color.get(n):
            dfs(n, [])
    for d in t.defs:
        if not d.auto_start and not d.start_event and low(d.id) not in started_by_reward:
            probs.append('%s: never starts (no auto_start, no start_on, no reward starts it)' % d.id)
    return probs


def overlap(t, a, b):
    """Can one event target match both objective targets a and b?"""
    if a == '' or b == '':
        return True
    pa, pb = a.endswith('*'), b.endswith('*')
    la, lb = low(concrete(a)), low(concrete(b))
    if pa and pb:
        return la.startswith(lb) or lb.startswith(la)
    if pa:
        return lb.startswith(la) if la or t.rules.star_any else False
    if pb:
        return la.startswith(lb) if lb or t.rules.star_any else False
    return la == lb


def check_skip_ahead(t):
    """An event a later stage waits for finishes the stages before it (LyudQuests.cpp:488-505): the same event+target in an
    earlier stage then skips that stage (or every stage in between)."""
    probs = []
    for d in t.defs:
        for i, si in enumerate(d.stages):
            for j in range(i + 1, len(d.stages)):
                sj = d.stages[j]
                for oi in si.objectives:
                    evs_i = [a for a in oi.event.split('|') if a]
                    if oi.event == 'have':
                        evs_i = ['get']  # a have counts the get events
                    if oi.event == 'bring':
                        evs_i = ['talk']
                    for oj in sj.objectives:
                        if oj.event == 'have':
                            continue  # never triggers skip-ahead (:493)
                        for ev in evs_i:
                            if t.rules.talk_no_skip and low(ev) == 'talk':
                                continue
                            if event_alt_match(oj.event, ev) and overlap(t, oi.target, oj.target):
                                probs.append('%s: stage %s objective %s (%s %s) is also wanted by later stage %s objective %s (%s %s): '
                                             'doing it skips %s' % (d.id, si.id, oi.id, ev, oi.target, sj.id, oj.id, oj.event, oj.target,
                                                                    'stage ' + si.id if j == i + 1 else 'stages %s..%s' % (si.id, d.stages[j - 1].id)))
    return probs


def event_alt_match(want, got):
    return engine.event_matches(want, got)


# ------------------------------------------------------------------ playthrough

def concrete_target(t, ev, target):
    e = low(ev)
    if target in ('', '*'):
        pools = {'talk': t.npcs, 'reach': t.places, 'feeder_visit': t.species, 'feeder_fill': t.feeds}
        pool = sorted(pools.get(e, []))
        return pool[0] if pool and target == '*' else ('x' if target == '*' else '')
    if target.endswith('*'):
        prefix = low(target[:-1])
        pools = {'get': t.items, 'have': t.items, 'use': t.items, 'wear': t.items, 'feeder_fill': t.items,
                 'reach': t.places, 'talk': t.npcs, 'visit': t.plots}
        cand = sorted(x for x in pools.get(e, []) if x.startswith(prefix))
        return cand[0] if cand else prefix + 'x'
    return target


def feed_objective(sim, t, o, have_count):
    ev = [a for a in o.event.split('|') if a][0]
    tgt = concrete_target(t, ev, o.target)
    e = low(ev)
    if e == 'have':
        sim.inv_add(tgt, max(1, o.need - have_count))
    elif e == 'bring':
        item, _, npc = o.target.partition('@')
        if sim.count(item) < o.need:
            sim.inv_add(item, o.need - sim.count(item))
        sim.handle_event('talk', npc, 1)
    elif e == 'use':
        if sim.count(tgt) < 1:
            sim.inv_add(tgt, 1)
        sim.inv_remove(tgt, 1)
    elif e == 'get':
        sim.inv_add(tgt, 1)
    elif e == 'reach':
        if (t.places.get(low(tgt)) or {}).get('r'):  # a place with radius 0 is never reached (LyudQuests.cpp:602)
            sim.inside = set()  # walks away first, then in
            sim.reach(tgt)
    else:
        sim.handle_event(ev, tgt, 1)


def start_event_of(t, d):
    return d.start_event, concrete_target(t, d.start_event, d.start_target)


def play(sim, t, quest, problems, warns, depth=0):
    """Drive one quest from its start to done, feeding each current objective its event in order."""
    d = sim.find_def(quest)
    if sim.state(quest) == engine.INACTIVE:
        if d.start_event:
            ev, tgt = start_event_of(t, d)
            if low(ev) == 'reach':
                sim.inside = set()
                sim.reach(tgt)
            else:
                sim.handle_event(ev, tgt, 1)
        else:
            parents = [p for p in t.defs if any(low(quest) in [low(x) for x in s.starts] for s in p.stages)]
            if parents and depth < 8:
                play(sim, t, parents[0].id, problems, warns, depth + 1)
    if sim.state(quest) == engine.INACTIVE:
        problems.append('%s: could not be started' % quest)
        return
    mark = len(sim.trace)
    for _ in range(400):
        p = sim.find_progress(quest)
        if p.state != engine.ACTIVE:
            break
        s = d.stages[p.stage]
        if sim.stage_met(p, d):
            # met but not finished: only another event finishes it (BeginStage pre-count, LyudQuests.cpp:387-403)
            warns.append('%s.%s: met as it begins (pocket/place) but waits for some unrelated event to finish' % (d.id, s.id))
            sim.handle_event('__any_event__', '', 1)
            continue
        before = (p.stage, list(p.counts))
        for i, o in enumerate(s.objectives):
            if p.state != engine.ACTIVE or d.stages[p.stage] is not s:
                break
            if p.counts[i] < o.need:
                feed_objective(sim, t, o, p.counts[i])
                break
        if (p.stage, list(p.counts)) == before and p.state == engine.ACTIVE:
            problems.append('%s.%s: stuck, feeding its objectives changes nothing (counts %s)' % (d.id, s.id, p.counts))
            return
    else:
        problems.append('%s: not done after 400 steps' % quest)
        return
    trace = sim.trace[mark:]
    begun = {x[2] for x in trace if x[0] == 'begin' and x[1] == d.id}
    begun |= {x[2] for x in sim.trace[:mark] if x[0] == 'begin' and x[1] == d.id}
    for i, s in enumerate(d.stages):
        if i not in begun:
            problems.append('%s.%s: the stage never began on the way to done' % (d.id, s.id))
    for x in trace:
        if x[0] == 'skip' and x[1] == d.id:
            problems.append('%s: playing in order, %s %s at stage %s skipped ahead to stage %s'
                            % (d.id, x[4], x[5], d.stages[x[2]].id, d.stages[x[3]].id))


def check_playthrough(t):
    probs, warns = [], []
    for d in t.defs:
        sim = engine.Sim(t.defs, t.rules, t.start_items)
        play(sim, t, d.id, probs, warns)
    return probs, sorted(set(warns))


# ------------------------------------------------------------------ dialogue, scenes, dev blocks

def check_dialogue_lines(t):
    probs = []
    for npc, key, lines, n in t.dialogue:
        for k in lines:
            missing = [lang for lang in LANGS if not t.strings[lang].get('line.' + k)]
            if missing:
                probs.append('npc_dialogue.txt:%d [%s] %s: line.%s missing in %s' % (n, npc, key, k, ','.join(missing)))
    return probs


def check_dialogue_quests(t):
    probs = []
    for npc, key, lines, n in t.dialogue:
        if not key.startswith(('quest.', 'aside.')):
            continue
        # quest.<quest>.<stages>[.if.<cond>+<cond>][.again], aside.<quest>.<stages>[.again] (docs/quests.md "Branching talk";
        # the conditions themselves are checked by tools/check_quest_talk.py)
        plain = key[:-len('.again')] if key.endswith('.again') else key
        plain, has_if, conds = plain.partition('.if.')
        parts = plain.split('.')
        if len(parts) != 3 or (has_if and not all(c.strip() for c in conds.split('+'))):
            probs.append('npc_dialogue.txt:%d [%s] %s: needs quest.<quest>.<stage>[,<stage>...][.if.<cond>+<cond>][.again]' % (n, npc, key))
            continue
        d = next((x for x in t.defs if low(x.id) == low(parts[1])), None)
        if not d:
            probs.append('npc_dialogue.txt:%d [%s] %s: no quest %s' % (n, npc, key, parts[1]))
            continue
        for st in parts[2].split(','):
            st = st.strip()
            if st == 'done' or any(low(s.id) == low(st) for s in d.stages):
                continue
            if st.endswith('_empty'):
                base = st[:-len('_empty')]
                stage = next((s for s in d.stages if low(s.id) == low(base)), None)
                brings = [o for o in (stage.objectives if stage else []) if o.event == 'bring']
                if not stage:
                    probs.append('npc_dialogue.txt:%d [%s] %s: no stage %s for %s' % (n, npc, key, base, st))
                elif not brings:
                    probs.append('npc_dialogue.txt:%d [%s] %s: stage %s has no bring objective, TalkStage never says %s' % (n, npc, key, base, st))
                elif not any(low(o.target.partition('@')[2]) == low(npc) for o in brings):
                    probs.append('npc_dialogue.txt:%d [%s] %s: the bring of stage %s is to somebody else' % (n, npc, key, base))
                continue
            probs.append('npc_dialogue.txt:%d [%s] %s: quest %s has no stage %s' % (n, npc, key, d.id, st))
    return probs


def check_scenes(t):
    probs = []
    sc = t.scenes
    if not sc:
        return probs
    for pk in sc.get('pickups', []):
        d = next((x for x in t.defs if low(x.id) == low(pk.get('quest', ''))), None)
        if not d:
            probs.append('pickup %s: no quest %s' % (pk.get('id'), pk.get('quest')))
            continue
        for st in pk.get('stages', []):
            if not any(low(s.id) == low(st) for s in d.stages):
                probs.append('pickup %s: quest %s has no stage %s' % (pk.get('id'), d.id, st))
        if low(pk.get('item', '')) not in t.items:
            probs.append('pickup %s: no item %s' % (pk.get('id'), pk.get('item')))
    for li in sc.get('listeners', []):
        who = li.get('person', '')
        if t.people and who not in t.people:
            probs.append('listener %s: no such person key (people.json/townsfolk.json)' % who)
        for r in li.get('reactions', []):
            if r.get('species') and low(r['species']) not in t.species:
                probs.append('listener %s: no species %s' % (who, r['species']))
            if r.get('quest'):
                d = next((x for x in t.defs if low(x.id) == low(r['quest'])), None)
                if not d:
                    probs.append('listener %s: no quest %s' % (who, r['quest']))
                elif r.get('stage') and r['stage'] != 'done' and not any(low(s.id) == low(r['stage']) for s in d.stages):
                    probs.append('listener %s: quest %s has no stage %s' % (who, d.id, r['stage']))
            for k in r.get('lines', []):
                missing = [lang for lang in LANGS if not t.strings[lang].get('line.' + k)]
                if missing:
                    probs.append('listener %s: line.%s missing in %s' % (who, k, ','.join(missing)))
    return probs


# talk stages that are the person's everyday first conversation on purpose (awakening: the owner's own first words are the
# step; the other stages of that quest do have quest talks)
EVERYDAY_TALK_STAGES = {('awakening', 'owner')}


def check_talk_lines(t):
    """Where the dialogue has quest talks (npc_dialogue.txt quest.<quest>.<stage>), a stage that sends the hero to talk to
    (or bring something to) somebody has words of theirs for it: else they greet him with everyday lines."""
    nd = t.cpp.get(os.path.join('Private', 'NpcDialogue.cpp'), '')
    if 'QuestTalks' not in nd:
        return []
    said = set()
    for npc, key, _, _ in t.dialogue:
        parts = key.split('.')
        if parts[0] == 'quest' and len(parts) >= 3:
            for st in parts[2].split(','):
                said.add((low(npc), low(parts[1]), low(st.strip())))
    probs = []
    talking = {q for _, q, _ in said}
    for d in t.defs:
        if low(d.id) not in talking:
            continue  # a quest written without quest talks
        for s in d.stages:
            if (low(d.id), low(s.id)) in EVERYDAY_TALK_STAGES:
                continue
            for o in s.objectives:
                who = o.target if low(o.event) == 'talk' else (o.target.partition('@')[2] if o.event == 'bring' else '')
                if who and '*' not in who and (low(who), low(d.id), low(s.id)) not in said:
                    probs.append('%s.%s: waits for a talk with %s, who has no quest.%s.%s line' % (d.id, s.id, who, d.id, s.id))
    return probs


def check_reward_double_count(t):
    """A reward item counted twice by a have right after it (see the engine WARN): only harmless while the have needs no
    more than the reward gives."""
    probs = []
    for d in t.defs:
        # start_on get X: Start counts the pocket (the item is in it already), then the same get counts again
        if low(d.start_event) == 'get' and '*' not in d.start_target:
            for o in d.stages[0].objectives:
                if o.event == 'have' and low(o.target) == low(d.start_target) and o.need > 1:
                    probs.append('%s starts on get %s and its first stage wants have x%d: the item that starts it counts twice'
                                 % (d.id, o.target, o.need))
        for k, s in enumerate(d.stages):
            for item, n in s.items:
                nxt = [(d.id, d.stages[k + 1])] if k + 1 < len(d.stages) else []
                nxt += [(q.id, q.stages[0]) for q in t.defs if low(q.id) in [low(x) for x in s.starts]]
                for qid, st in nxt:
                    for o in st.objectives:
                        if o.event == 'have' and low(o.target) == low(item) and o.need > n:
                            probs.append('%s.%s gives %d %s, %s.%s wants have x%d: the reward counts twice' % (d.id, s.id, n, item, qid, st.id, o.need))
    return probs


def check_dev(t):
    probs = []
    for q in t.quests_json.get('quests', []):
        for s in q.get('stages', []):
            dev = s.get('dev')
            if not dev:
                continue
            w = '%s.%s dev' % (q.get('id'), s.get('id'))
            unknown = set(dev) - {'tp', 'time', 'give', 'exec'}
            if unknown:
                probs.append('%s: unknown fields %s' % (w, sorted(unknown)))
            if 'tp' in dev and low(dev['tp']) not in t.teleports:
                probs.append('%s: tp %s is no teleport (Content/Dev/teleports, city_plan.json, map.json place)' % (w, dev['tp']))
            if 'time' in dev and not re.fullmatch(r'([01]\d|2[0-3]):[0-5]\d', dev['time']):
                probs.append('%s: time %r is not HH:MM' % (w, dev['time']))
            for item in (dev.get('give') or {}):
                if low(item) not in t.items:
                    probs.append('%s: give %s: no such item' % (w, item))
            for cmd in dev.get('exec', []):
                words = cmd.split()
                if not words or low(words[0]) not in t.commands:
                    probs.append('%s: exec %r: no such console command' % (w, cmd))
                elif low(words[0]) == 'quest.gotoperson' and (len(words) < 2 or words[1] not in t.people):
                    probs.append('%s: exec %r: no person key %s (people.json/townsfolk.json)' % (w, cmd, words[1] if len(words) > 1 else ''))
    return probs


# ------------------------------------------------------------------ engine self-tests (the copy behaves as the C++ says)

def engine_tests(t):
    """Small made-up quests, each pinning one rule of LyudQuests.cpp; returns problems (the copy is wrong) and warns
    (engine behaviour worth knowing)."""
    R = t.rules
    probs, warns = [], []

    def mk(quests):
        return engine.load_defs({'quests': quests})

    def obj(i, ev, tgt='', need=1):
        return {'id': i, 'event': ev, 'target': tgt, 'need': need}

    # alternatives and wildcards
    s = engine.Sim(mk([{'id': 'a', 'kind': 'side', 'auto_start': True, 'stages': [
        {'id': 's0', 'objectives': [obj('o', 'leave_computer|walk')]},
        {'id': 's1', 'objectives': [obj('o', 'use', 'drink_*')]}]}]), R)
    s.handle_event('walk')
    s.inv_add('drink_baikal', 1)
    s.inv_remove('drink_baikal', 1)
    if s.state('a') != engine.DONE:
        probs.append('alternatives "a|b" and prefix "x*" targets do not finish a quest')
    # have counts get and use, and the pocket at the stage's start
    s = engine.Sim(mk([{'id': 'h', 'kind': 'side', 'auto_start': True, 'stages': [
        {'id': 's0', 'objectives': [obj('o', 'have', 'apple', 2)]}]}]), R, {'apple': 1})
    p = s.find_progress('h')
    if p.counts != [1]:
        probs.append('BeginStage does not count the pocket for have (counts %s)' % p.counts)
    s.inv_remove('apple', 1)
    s.inv_add('apple', 1)
    if s.state('h') == engine.DONE or p.counts != [1]:
        probs.append('have: get adds, use takes away (counts %s)' % p.counts)
    s.inv_add('apple', 1)
    if s.state('h') != engine.DONE:
        probs.append('have: two apples in the pocket do not finish it')
    # skip-ahead
    s = engine.Sim(mk([{'id': 'k', 'kind': 'side', 'auto_start': True, 'stages': [
        {'id': 's0', 'objectives': [obj('o', 'talk', 'owner')]},
        {'id': 's1', 'objectives': [obj('o', 'reach', 'beach')]},
        {'id': 's2', 'objectives': [obj('o', 'reach', 'academ'), obj('p', 'reach', 'town')]}]}]), R)
    s.reach('academ')
    p = s.find_progress('k')
    # (UpdatePosition puts the place in Inside before the reach event: BeginStage counts it, the event once more)
    if p.stage != 2 or p.counts[0] < 1 or p.counts[1] != 0:
        probs.append('skip-ahead: an event of stage 3 at stage 1 does not jump there and count (stage %d counts %s)' % (p.stage, p.counts))
    # start_on and the reward's start; a reward item reaches other quests through the queue
    s = engine.Sim(mk([
        {'id': 'x', 'kind': 'side', 'start_on': {'event': 'festival', 'target': 'holi'}, 'stages': [
            {'id': 's0', 'objectives': [obj('o', 'festival_watch', 'holi.color_powder')], 'reward': {'items': {'apple': 1}, 'start': ['y']}}]},
        {'id': 'y', 'kind': 'side', 'stages': [{'id': 's0', 'objectives': [obj('o', 'have', 'apple')]}]},
        {'id': 'z', 'kind': 'side', 'auto_start': True, 'stages': [{'id': 's0', 'objectives': [obj('o', 'get', 'apple')]}]}]), R)
    s.handle_event('festival', 'holi')
    s.handle_event('festival_watch', 'holi.color_powder')
    if s.state('x') != engine.DONE or s.state('z') != engine.DONE:
        probs.append('start_on / reward items: x %s z %s' % (s.state('x'), s.state('z')))
    if s.state('y') == engine.INACTIVE:
        probs.append('reward start: y did not start')
    # --- engine behaviour (not data errors): reported as WARN
    # 0. y began (BeginStage counted the apple already in the pocket) before the reward's get waiting in the queue was
    #    handled: the one apple counts twice
    s = engine.Sim(mk([{'id': 'w', 'kind': 'side', 'auto_start': True, 'stages': [
        {'id': 's0', 'objectives': [obj('o', 'talk', 'owner')], 'reward': {'items': {'apple': 1}}},
        {'id': 's1', 'objectives': [obj('o', 'have', 'apple', 2)]}]}]), R)
    s.handle_event('talk', 'owner')
    if s.state('w') == engine.DONE:
        warns.append('engine: a have objective counts a reward item twice: FinishStage puts it in the pocket (its get waits in '
                     'the queue), BeginStage of the next stage or of a quest the reward starts counts the pocket '
                     '(LyudQuests.cpp:395), then the queued get adds it again (:524-527): one apple meets "have apple x2"')
    # 1. a stage met as it begins waits for another event (no re-check after BeginStage) unless chain_met
    s = engine.Sim(mk([{'id': 'm', 'kind': 'side', 'auto_start': True, 'stages': [
        {'id': 's0', 'objectives': [obj('o', 'talk', 'owner')]},
        {'id': 's1', 'objectives': [obj('o', 'have', 'apple')]},
        {'id': 's2', 'objectives': [obj('o', 'talk', 'clerk')]}]}]), R, {'apple': 1})
    s.handle_event('talk', 'owner')
    if s.find_progress('m').stage == 1:
        warns.append('engine: a stage whose objectives are met as it begins (have: the item already in the pocket) does not '
                     'finish until some later event arrives (LyudQuests.cpp:387-403 counts, :540 is the only finish)')
    # 2. Quest.Advance gives a reward item outside HandleEvent: the get is handled at once, re-entering while the stage is
    #    being finished (a have/get of the same item on that stage finishes it a second time)
    s = engine.Sim(mk([{'id': 'v', 'kind': 'side', 'auto_start': True, 'stages': [
        {'id': 's0', 'objectives': [obj('o', 'get', 'apple')], 'reward': {'items': {'apple': 1}}},
        {'id': 's1', 'objectives': [obj('o', 'talk', 'owner')]},
        {'id': 's2', 'objectives': [obj('o', 'talk', 'clerk')]}]}]), R)
    s.advance('v')
    fin = [x for x in s.trace if x[0] == 'finish' and x[1] == 'v']
    if len(fin) > 1:
        warns.append('engine: Quest.Advance / FinishStage outside HandleEvent (LyudQuests.cpp:553-562, :415-420) reports the '
                     'reward items\' get at once; with a get/have of that item on the stage the stage finishes twice (stage '
                     '%s skipped, its reward given again); a start_on or reward start during it adds to Progress while '
                     'FinishStage still holds P (dangling)' % ', '.join(str(x[2]) for x in fin[1:]))
    # 3. a main quest begun by the reward of the tracked quest is not tracked: FinishStage picks the next shown quest
    #    (a side one) before it starts the reward's quests, and Start tracks a main quest only when the tracked one is not active
    s = engine.Sim(mk([
        {'id': 'm1', 'kind': 'main', 'auto_start': True, 'stages': [{'id': 's0', 'objectives': [obj('o', 'talk', 'owner')], 'reward': {'start': ['m2']}}]},
        {'id': 'side', 'kind': 'side', 'auto_start': True, 'stages': [{'id': 's0', 'objectives': [obj('o', 'talk', 'clerk')]}]},
        {'id': 'm2', 'kind': 'main', 'stages': [{'id': 's0', 'objectives': [obj('o', 'talk', 'clerk')]}]}]), R)
    s.handle_event('talk', 'owner')
    if s.state('m2') == engine.ACTIVE and s.tracked != 'm2':
        warns.append('engine: when the tracked quest ends and its reward starts a main quest, the HUD follows a side quest '
                     'instead (FinishStage LyudQuests.cpp:440-444 picks ShownQuest before the Start loop :447-451; Start :366 '
                     'then sees an active tracked quest)')
    # 4. a bare "*" target
    if not R.star_any:
        warns.append('engine: a bare "*" target never matches (TargetMatches, LyudQuests.cpp:181-184: FString::StartsWith("") is false)')
    return probs, warns


def planted_errors(t):
    """The checks themselves: plant one error of each kind in a copy of the data and see each check catch it."""
    import copy
    m = copy.copy(t)
    m.quests_json = copy.deepcopy(t.quests_json)
    qs = m.quests_json['quests']
    o = lambda i, ev, tgt='', **k: dict(id=i, event=ev, target=tgt, **k)  # noqa: E731
    qs.append({'id': 'zz_a', 'kind': 'side', 'start_on': {'event': 'talk', 'target': 'Nobody_here'}, 'stages': [
        {'id': 'one', 'objectives': [o('x', 'reach', 'club')]},
        {'id': 'two', 'objectives': [o('y', 'use', 'drink_*')]},
        {'id': 'three', 'objectives': [o('z', 'use', 'drink_baikal'), o('w', 'jump_twice')],
         'reward': {'start': ['zz_b'], 'items': {'no_such_item': 1}}}]})
    qs.append({'id': 'zz_b', 'kind': 'side', 'stages': [
        {'id': 'one', 'objectives': [o('x', 'have', 'drink_*')], 'reward': {'start': ['zz_a']},
         'dev': {'tp': 'no_such_teleport', 'exec': ['Quest.NoSuchCommand']}}]})
    qs.append({'id': 'zz_c', 'kind': 'sid', 'stages': [{'id': 'one', 'objectives': [o('x', 'talk', 'owner', need=0)]}]})
    qs.append({'id': 'zz_c', 'kind': 'side', 'stages': [{'id': 'one', 'objectives': [o('x', 'talk', 'owner')]}]})
    m.defs = engine.load_defs(m.quests_json)
    m.dialogue = t.dialogue + [('Owner', 'quest.zz_a.four', ['owner.no_such_line.1'], 0)]
    want = {
        'schema': check_schema(m), 'unique': check_unique(m), 'text': check_text(m), 'events': check_events(m),
        'targets': check_targets(m), 'starts': check_starts(m), 'skip': check_skip_ahead(m),
        'dialogue quests': check_dialogue_quests(m), 'dialogue lines': check_dialogue_lines(m), 'dev': check_dev(m),
    }
    probs = ['planted %s error not caught' % k for k, v in want.items() if not v]
    pp, _ = check_playthrough(m)
    if not any('zz_a' in p for p in pp):
        probs.append('planted unplayable quest not caught by the playthrough')
    return probs


def main(argv):
    verbose = '-v' in argv
    args = [a for a in argv if not a.startswith('-')]
    root = args[0] if args else os.path.normpath(os.path.join(HERE, '..', '..'))
    t = Tree(root)
    print('questsim: %s: %d quests, %d places, %d items, %d npcs; engine rules: %s'
          % (root, len(t.defs), len(t.places), len(t.items), len(t.npcs), ', '.join(t.rules.names()) or 'main'))
    r = Report(verbose)
    probs, warns = engine_tests(t)
    r.check('engine copy follows LyudQuests.cpp (alternatives, wildcards, have/get/use, skip-ahead, start_on, rewards)', probs)
    r.check('the checks catch errors planted in a copy of the data', planted_errors(t))
    r.check('quests.json schema', check_schema(t))
    r.check('unique quest, stage, objective and place ids', check_unique(t))
    r.check('every quest/objective/stage/place text key in ru, en, ja', check_text(t))
    r.check('every event the quests wait for is reported by the game', check_events(t))
    r.check('every target is real (npc, place with a radius, item, species, festival, plot...)', check_targets(t))
    r.check('reward start lists: real quests, no cycles, every quest can start', check_starts(t))
    r.check('no stage is skipped by its own event (same event+target in a later stage)', check_skip_ahead(t))
    play_probs, play_warns = check_playthrough(t)
    r.check('every quest plays from start to done, every stage begun, none skipped', play_probs)
    r.check('dialogue quest.<quest>.<stage> lines name real quests and stages', check_dialogue_quests(t))
    r.check('dialogue line keys exist in ru, en, ja', check_dialogue_lines(t))
    r.check('quest_scenes.json pickups and listeners', check_scenes(t))
    r.check('a talk stage has the person\'s quest lines (where the dialogue has quest talks)', check_talk_lines(t))
    r.check('no have objective is met by a reward item counted twice', check_reward_double_count(t))
    r.check('stage dev blocks: teleports, items, console commands, people', check_dev(t))
    for w in warns + play_warns:
        r.warn(w)
    if r.fails:
        print('%d of %d checks failed' % (r.fails, r.checks))
        return 1
    print('all ok (%d checks)' % r.checks)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
