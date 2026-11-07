#!/usr/bin/env python3
"""Checks the graphics levels (docs/graphics.md): Config/DefaultScalability.ini over the engine's BaseScalability.ini.

Scalability only sets the keys a level's section lists, so a key that one level of a group sets and another does not
keeps, at that other level, the value of whatever level came before: Epic's shadows stay on Low until the game restarts.
This lists every such key, and the merged value of every key the project sets, level by level. A gap is the project's
when the project sets the key at a level the engine leaves it out of; the engine's own (it leaves most Lumen keys out of
the levels that turn Lumen off) stay the engine's when the project only changes their values.

    tools/check_scalability.py                       engine at $UE_ROOT (default /Users/Shared/Epic Games/UE_5.8)
    tools/check_scalability.py --engine <UE root>
    tools/check_scalability.py --cvars <file>        also flag keys that are not console variables of this engine:
                                                     the file has a variable name first on each line (`DumpCVars`)
    tools/check_scalability.py --platform IOS        a platform's levels: the engine's and the project's
                                                     Config/<platform>/<platform>Scalability.ini on top (IOS, Android)
Exit status 1 when the project leaves a key it adds out of a level (the engine's own gaps are only listed).
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


def merge(into, more):
    """The sections of `more` over `into` (a platform's file over the general one)."""
    for name, keys in more.items():
        into.setdefault(name, {}).update(keys)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--engine", default=os.environ.get("UE_ROOT", "/Users/Shared/Epic Games/UE_5.8"))
    ap.add_argument("--project", default=PROJECT)
    ap.add_argument("--cvars", help="a list of this engine's console variables, one name first on each line")
    ap.add_argument("--platform", help="IOS, Android: merge the engine's and the project's <platform>Scalability.ini too")
    args = ap.parse_args()

    base_path = os.path.join(args.engine, "Engine", "Config", "BaseScalability.ini")
    if not os.path.exists(base_path):
        print(f"no {base_path}: pass --engine <UE root>", file=sys.stderr)
        return 2
    base = read_ini(base_path)
    project = read_ini(args.project)
    if args.platform:
        p = args.platform
        engine_platform = os.path.join(args.engine, "Engine", "Config", p, f"{p}Scalability.ini")
        project_platform = os.path.join(os.path.dirname(args.project), p, f"{p}Scalability.ini")
        for path, into in ((engine_platform, base), (project_platform, project)):
            if os.path.exists(path):
                merge(into, read_ini(path))
                print(f"(+ {path})")
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
            if not missing:
                continue
            in_project = [level for level in LEVELS if low in project.get(f"{group}@{level}", {})]
            ours = any(low not in base.get(f"{group}@{level}", {}) for level in in_project)
            gaps.append((ours, key, missing, in_project))
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
        for ours, key, missing, in_project in gaps:
            where = ", ".join("@" + m for m in missing)
            if ours:
                print(f"  ! PROJECT key {key} is not set at {where}")
            else:
                also = f" (the project sets it at {', '.join('@' + l for l in in_project)})" if in_project else ""
                print(f"  . engine key {key} is not set at {where}{also}")
            failed = failed or ours
    print("(* = set by the project's Config/DefaultScalability.ini%s; '.' lines are the engine's own gaps)"
          % (f" or Config/{args.platform}/{args.platform}Scalability.ini" if args.platform else ""))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
