#!/usr/bin/env python3
"""Checks the graphics levels (docs/graphics.md): Config/DefaultScalability.ini over the engine's BaseScalability.ini.

Scalability only sets the keys a level's section lists, so a key that one level of a group sets and another does not
keeps, at that other level, the value of whatever level came before: Epic's shadows stay on Low until the game restarts.
This lists every such key (of the project's own sections and of the engine's), and the merged value of every key the
project sets, level by level.

    tools/check_scalability.py                       engine at $UE_ROOT (default /Users/Shared/Epic Games/UE_5.8)
    tools/check_scalability.py --engine <UE root>
    tools/check_scalability.py --cvars <file>        also flag keys that are not console variables of this engine:
                                                     the file has a variable name first on each line (`DumpCVars`)
Exit status 1 when the project leaves a key out of a level (the engine's own gaps are only listed).
"""
import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROJECT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Config", "DefaultScalability.ini")
LEVELS = ["0", "1", "2", "3", "Cine"]
GROUPS = ["ViewDistanceQuality", "AntiAliasingQuality", "ShadowQuality", "GlobalIlluminationQuality",
          "ReflectionQuality", "PostProcessQuality", "TextureQuality", "EffectsQuality", "FoliageQuality",
          "ShadingQuality", "LandscapeQuality"]


def read_ini(path):
    """{section: {key (lower case): (key, value)}} in file order; later lines win."""
    sections = {}
    current = None
    with open(path, encoding="utf-8-sig") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith(";") or line.startswith("#"):
                continue
            m = re.match(r"^\[(.+)\]$", line)
            if m:
                current = sections.setdefault(m.group(1).strip(), {})
                continue
            if current is None or "=" not in line:
                continue
            key, value = line.split("=", 1)
            key = key.strip().lstrip("+-.!")
            current[key.lower()] = (key, value.split(";")[0].strip())
    return sections


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--engine", default=os.environ.get("UE_ROOT", "/Users/Shared/Epic Games/UE_5.8"))
    ap.add_argument("--project", default=PROJECT)
    ap.add_argument("--cvars", help="a list of this engine's console variables, one name first on each line")
    args = ap.parse_args()

    base_path = os.path.join(args.engine, "Engine", "Config", "BaseScalability.ini")
    if not os.path.exists(base_path):
        print(f"no {base_path}: pass --engine <UE root>", file=sys.stderr)
        return 2
    base = read_ini(base_path)
    project = read_ini(args.project)
    known = None
    if args.cvars:
        known = set()
        with open(args.cvars, encoding="utf-8", errors="replace") as f:
            for line in f:
                m = re.match(r"\s*([A-Za-z][\w.]*)", line)
                if m:
                    known.add(m.group(1).lower())

    failed = False
    for section in project:
        group = section.split("@")[0]
        if "@" in section and (group not in GROUPS or section.split("@", 1)[1] not in LEVELS):
            print(f"! [{section}]: not a scalability level section (groups {', '.join(GROUPS)}; levels @0..@3, @Cine)")
            failed = True

    for group in GROUPS:
        merged = {}
        for level in LEVELS:
            name = f"{group}@{level}"
            values = dict(base.get(name, {}))
            values.update(project.get(name, {}))
            merged[level] = values
        keys = {}
        for level in LEVELS:
            for low, (key, _) in merged[level].items():
                keys.setdefault(low, key)
        if not keys:
            continue
        own = {low for level in LEVELS for low in project.get(f"{group}@{level}", {})}
        gaps = []
        for low, key in sorted(keys.items()):
            missing = [level for level in LEVELS if low not in merged[level]]
            if missing:
                gaps.append((low in own, key, missing))
        if own:
            print(f"[{group}]  {'  '.join('@' + l for l in LEVELS)}")
            for low in sorted(own):
                cells = []
                for level in LEVELS:
                    v = merged[level].get(low)
                    mark = "*" if low in project.get(f"{group}@{level}", {}) else ""
                    cells.append((v[1] if v else "-") + mark)
                flag = ""
                if known is not None and low not in known:
                    flag = "   <- not a console variable of this engine"
                    failed = True
                print(f"  {keys[low]:<62} {'  '.join(c.rjust(6) for c in cells)}{flag}")
        for is_own, key, missing in gaps:
            who = "PROJECT" if is_own else "engine"
            print(f"  {'!' if is_own else '.'} {who} key {key} is not set at {', '.join('@' + m for m in missing)}")
            failed = failed or is_own
    print("(* = set by Config/DefaultScalability.ini; '.' lines are the engine's own gaps)")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
