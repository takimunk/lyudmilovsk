#!/usr/bin/env python3
"""Prints benchmark results (tools/bench.sh, docs/benchmark.md) as Markdown tables.

  python3 tools/bench_report.py <result.json>                 every preset, overall and per segment
  python3 tools/bench_report.py --compare a.json b.json ...   one row per file (and preset): compare variants
  python3 tools/bench_report.py --compare --segment forest bench/results/<machine>/*.json
  python3 tools/bench_report.py --passes <result.json>       a run with -LyudBenchPasses: each pass against "base"
"""
import argparse
import json
import os


def load(path):
    with open(path) as f:
        return json.load(f)


def row(name, s):
    f, g = s["frame_ms"], s
    # Metal reports no draw or triangle counts in this engine build: those columns stay empty on a Mac
    draws = "%.0f" % g["draw_calls"]["median"] if g["draw_calls"]["max"] else "n/a"
    tris = "%.2fM" % (g["triangles"]["median"] / 1e6) if g["triangles"]["max"] else "n/a"
    return "| %s | %d | %.1f | %.2f | %.2f | %.2f | %.2f | %.2f | %.2f | %.2f | %s | %s | %.0f |" % (
        name, s["frames"], s["fps_mean"], f["median"], f["p95"], f["p99"], g["gpu_ms"]["median"], g["game_ms"]["median"],
        g["render_ms"]["median"], g["rhi_ms"]["median"], draws, tris, g["memory_mb"]["max"])


HEAD = ("| %s | frames | fps | frame med | p95 | p99 | GPU | game | render | RHI | draws | tris | mem MB |\n"
        "|---|---|---|---|---|---|---|---|---|---|---|---|---|")


def machine_line(r):
    m = r["machine"]
    return "%s, %s, %s (%s), %s GB, %s, %s, %s; game time %s; %d trees baked in %.0f s%s" % (
        m["device"] or m["platform"], m["cpu"], m["gpu"], m["rhi"], m["ram_gb"], m["os"], r["resolution"],
        "editor -game" if r.get("editor_game") else r["build"], r["game_time"], r["trees"], r["bake_seconds"],
        "; setup: " + r["setup"] if r.get("setup") else "")


def report(path):
    r = load(path)
    print("### %s (%s)\n" % (os.path.basename(path), r["label"]))
    print(machine_line(r) + "\n")
    for p in r["presets"]:
        print("**%s%s** (screen %d%%)\n" % (p["preset_name"], " " + p["pass"] if p.get("pass") else "", p["screen_percentage"]))
        print(HEAD % "segment")
        for name, s in p["segments"].items():
            print(row(name, s))
        print(row("**all**", p["overall"]))
        print()


def compare(paths, segment):
    print(HEAD % "run")
    for path in paths:
        r = load(path)
        for p in r["presets"]:
            s = p["overall"] if segment == "all" else p["segments"].get(segment)
            if s:
                print(row("%s %s%s" % (r["label"] or os.path.basename(path), p["preset_name"], " " + p["pass"] if p.get("pass") else ""), s))


def passes(path):
    """Mean over the rounds of each pass's frame and GPU medians, per segment, and the change against the pass "base"."""
    r = load(path)
    print(machine_line(r) + "\n")
    segs = ["all"] + list(r["presets"][0]["segments"].keys())
    for seg in segs:
        by = {}
        for p in r["presets"]:
            s = p["overall"] if seg == "all" else p["segments"][seg]
            by.setdefault((p["preset_name"], p["pass"]), []).append(s)
        print("#### %s\n" % seg)
        print("| pass | rounds: frame median ms | frame med | p95 | GPU med | vs base |\n|---|---|---|---|---|---|")
        for (preset, name), runs in by.items():
            fm = sum(x["frame_ms"]["median"] for x in runs) / len(runs)
            p95 = sum(x["frame_ms"]["p95"] for x in runs) / len(runs)
            gm = sum(x["gpu_ms"]["median"] for x in runs) / len(runs)
            base = by.get((preset, "base"))
            bm = sum(x["frame_ms"]["median"] for x in base) / len(base) if base else None
            vs = "%+.0f%%" % (100.0 * (fm - bm) / bm) if bm and name != "base" else ""
            print("| %s %s | %s | %.1f | %.1f | %.1f | %s |" % (preset, name, " / ".join("%.1f" % x["frame_ms"]["median"] for x in runs),
                                                          fm, p95, gm, vs))
        print()


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="+")
    ap.add_argument("--compare", action="store_true")
    ap.add_argument("--segment", default="all")
    ap.add_argument("--passes", action="store_true")
    a = ap.parse_args()
    if a.passes:
        for f in a.files:
            passes(f)
    elif a.compare:
        compare(a.files, a.segment)
    else:
        for f in a.files:
            report(f)
