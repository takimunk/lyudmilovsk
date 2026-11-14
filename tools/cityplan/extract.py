#!/usr/bin/env python3
"""Cuts the engine-free parts out of the town's Unreal sources so tools/cityplan compiles the game's own code:

  extract.py <out dir>

  CityKit_free.cpp   Private/City/CityKit.cpp without its includes, the console variable, the OBJ writer and
                     FMesh::Build (the UStaticMesh part): every mesh builder as the game has it
  windows_free.inc   Private/World/LyudCityWindows.cpp's Frac/HashName and ALyudCityWindows::ScheduleLit,
                     Private/World/LyudCityBuilding.cpp's Hash-free InHours and ULyudWindowLights::SeededFlat
                     (as free functions over the shim's types)
  kit_input.txt      every prop of Content/City/public_spaces.json (kind, variant, its r), the fountain's stone, the
                     Ferris wheel's radius and hub height as ALyudFairRides sets them (LyudPublicSpaces.cpp)
  flats.txt          the wall flats of Content/City/city_plan.json and their window ids
Nothing is changed in the code itself: a cut that no longer matches fails loudly.
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.normpath(os.path.join(HERE, "..", "..", "unreal", "Lyudmilovsk", "Source", "Lyudmilovsk", "Private"))


def block(text, start):
    """From index start (at or before an opening brace) to its matching closing brace, inclusive."""
    i = text.index("{", start)
    depth = 0
    for j in range(i, len(text)):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                return text[start:j + 1]
    raise SystemExit("extract: unbalanced braces after %r" % text[start:start + 60])


def find(text, pattern, what):
    m = re.search(pattern, text, re.M)
    if not m:
        raise SystemExit("extract: %s not found (the source changed: update tools/cityplan/extract.py)" % what)
    return m.start()


def citykit():
    with open(os.path.join(SRC, "City", "CityKit.cpp"), encoding="utf-8") as f:
        t = f.read()
    t = re.sub(r"^#include .*$", "", t, flags=re.M)
    s = find(t, r"^static TAutoConsoleVariable<FString> CVarCityExportKit", "CVarCityExportKit")
    e = t.index(";\n", s) + 2
    t = t[:s] + t[e:]
    s = find(t, r"^namespace CityKit\s*\{\s*/\*\* The mesh as Wavefront OBJ", "the OBJ writer's namespace")
    t = t[:s] + t[s + len(block(t, s)):]
    s = find(t, r"^\tUStaticMesh\* FMesh::Build\(", "FMesh::Build")
    t = t[:s] + t[s + len(block(t, s)):]
    return '#include "CityKit.h"\n' + t


def windows():
    with open(os.path.join(SRC, "World", "LyudCityWindows.cpp"), encoding="utf-8") as f:
        w = f.read()
    with open(os.path.join(SRC, "World", "LyudCityBuilding.cpp"), encoding="utf-8") as f:
        b = f.read()
    out = []
    for name in ("uint32 HashName(const FString& S)", "float Frac(uint32 H, uint32 Salt)"):
        s = find(w, r"^\t" + re.escape(name), name)
        out.append(block(w, s).replace("for (TCHAR C : S)", "for (TCHAR C : S.S)"))
    s = find(w, r"^bool ALyudCityWindows::ScheduleLit\(", "ScheduleLit")
    out.append(block(w, s).replace("bool ALyudCityWindows::ScheduleLit(const FWindow& W, const FDateTime& Now, float SunAltitude) const",
                                   "bool ScheduleLit(const FWindow& W, const FDateTime& Now, float SunAltitude)"))
    s = find(b, r"^\tbool InHours\(float Hour, float From, float To\)", "InHours")
    out.append(block(b, s))
    s = find(b, r"^float ULyudWindowLights::SeededFlat\(", "SeededFlat")
    out.append(block(b, s).replace("float ULyudWindowLights::SeededFlat(const ALyudCityBuilding::FWindowSeed& D,",
                                   "float SeededFlat(const FWindowSeed& D,"))
    return "// cut from LyudCityWindows.cpp / LyudCityBuilding.cpp by tools/cityplan/extract.py\n" + "\n\n".join(out) + "\n"


def inputs():
    content = os.path.normpath(os.path.join(HERE, "..", "..", "unreal", "Lyudmilovsk", "Content"))
    with open(os.path.join(content, "City", "public_spaces.json"), encoding="utf-8") as f:
        ps = json.load(f)
    lines = []
    for p in ps["props"]:
        extra = ""
        r = p.get("r")
        if isinstance(r, list) and len(r) == 2:
            extra = " r2 %r %r" % (r[0], r[1])
        elif isinstance(r, (int, float)):
            extra = " r %r" % r
        lines.append("prop %s %d%s" % (p["kind"], int(p.get("variant", 0)), extra))
    for f in ps["fountains"]:
        if f.get("build_stone"):
            lines += ["stone %s %r" % (k, v) for k, v in f["stone"].items() if isinstance(v, (int, float))]
    with open(os.path.join(SRC, "City", "LyudPublicSpaces.cpp"), encoding="utf-8") as f:
        t = f.read()
    r = re.search(r"R\.Radius = ([0-9.]+)f;", t)
    hub = re.search(r"R\.Hub = At \+ FVector\(0, 0, ([0-9.]+)f\);", t)
    if not r or not hub:
        raise SystemExit("extract: the Ferris wheel's radius / hub not found in LyudPublicSpaces.cpp")
    lines.append("ride %s %s" % (r.group(1), float(hub.group(1)) / 100.0))
    with open(os.path.join(content, "City", "city_plan.json"), encoding="utf-8") as f:
        plan = json.load(f)
    flats = ["%s %s" % (d["id"], " ".join(w["id"] for w in d["windows"])) for d in plan["wall_dwellings"]]
    return "\n".join(lines) + "\n", "\n".join(flats) + "\n"


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    kit, flats = inputs()
    with open(os.path.join(out, "kit_input.txt"), "w", encoding="utf-8") as f:
        f.write(kit)
    with open(os.path.join(out, "flats.txt"), "w", encoding="utf-8") as f:
        f.write(flats)
    with open(os.path.join(out, "CityKit_free.cpp"), "w", encoding="utf-8") as f:
        f.write(citykit())
    with open(os.path.join(out, "windows_free.inc"), "w", encoding="utf-8") as f:
        f.write(windows())


if __name__ == "__main__":
    main()
