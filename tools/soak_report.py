#!/usr/bin/env python3
"""Per-lap summary of a tools/soak.sh run (or a comparison of two): frame median/p99 and memory per lap, SOAK object counts.
  python3 tools/soak_report.py <soak dir> [<other soak dir>]"""
import json, os, re, sys


def laps(d):
    j = json.load(open(os.path.join(d, 'bench.json')))
    out = []
    for r in j['presets']:
        o = r['overall']
        out.append((o.get('frame_ms', {}).get('median', o.get('median')), o.get('frame_ms', {}).get('p99', o.get('p99')), o.get('mem_mb', o.get('memory_mb', {}).get('max') if isinstance(o.get('memory_mb'), dict) else o.get('memory_mb'))))
    return out


def counts(d):
    rows = []
    for line in open(os.path.join(d, 'game.log'), errors='replace'):
        m = re.search(r'SOAK t=(\d+) .*?mean_ms=([\d.]+) actors=(\d+) components=(\d+) mids=(\d+) static_meshes=(\d+) textures=(\d+) rts=(\d+) audio=(\d+)', line)
        if m:
            rows.append(tuple(float(x) for x in m.groups()))
    return rows


for d in sys.argv[1:]:
    print('==', d)
    L = laps(d)
    for i, (med, p99, mem) in enumerate(L):
        print('lap %2d  median %6.2f  p99 %6.2f  mem %s' % (i + 1, med or 0, p99 or 0, mem))
    meds = sorted(m for m, _, _ in L if m)
    print('laps median of medians %.2f, mean %.2f' % (meds[len(meds) // 2], sum(meds) / len(meds)))
    C = counts(d)
    if C:
        a, b = C[0], C[-1]
        print('objects first -> last: actors %d -> %d, components %d -> %d, mids %d -> %d, static meshes %d -> %d, textures %d -> %d, rts %d -> %d, audio %d -> %d'
              % (a[2], b[2], a[3], b[3], a[4], b[4], a[5], b[5], a[6], b[6], a[7], b[7], a[8], b[8]))
    fp = [l.split('\t') for l in open(os.path.join(d, 'footprint.tsv')).read().split('\n')[1:] if l.strip()]
    fps = [int(x[1]) for x in fp if x[1].isdigit() and int(x[1]) > 0]
    if fps:
        print('footprint MB first %d, min %d, max %d, last %d' % (fps[0], min(fps), max(fps), fps[-1]))
