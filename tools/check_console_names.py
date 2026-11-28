#!/usr/bin/env python3
"""Fails when two static console objects (FAutoConsole*, TAutoConsoleVariable, RegisterConsole*) share a name.

A second registration of a name is refused by the console manager, and at quit both statics' destructors unregister
it: the second one crashes in FindConsoleObjectName, and on a Mac offscreen run the crash handler then waits forever
for a display (the bench exit hang of 2026-11-28, lyud.Pets.Report in LyudPets.cpp and AnimalFriends.cpp)."""
import collections, glob, os, re, sys

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'unreal', 'Lyudmilovsk', 'Source')
pat = re.compile(r'(?:FAutoConsole\w+|TAutoConsoleVariable<[^>]+>)\s+\w+\s*\(\s*(?:TEXT\()?\s*"([^"]+)"'
                 r'|RegisterConsole(?:Command|Variable)\w*\(\s*(?:TEXT\()?\s*"([^"]+)"')
seen = collections.defaultdict(list)
for f in glob.glob(os.path.join(root, '**', '*.cpp'), recursive=True):
    text = open(f, encoding='utf-8', errors='ignore').read()
    for m in pat.finditer(text):
        name = m.group(1) or m.group(2)
        seen[name.lower()].append('%s:%d' % (os.path.relpath(f, root), text.count('\n', 0, m.start()) + 1))
dups = {k: v for k, v in seen.items() if len(v) > 1}
for k, v in sorted(dups.items()):
    print('duplicate console name %s: %s' % (k, ', '.join(v)))
print('%d console names, %d duplicated' % (len(seen), len(dups)))
sys.exit(1 if dups else 0)
