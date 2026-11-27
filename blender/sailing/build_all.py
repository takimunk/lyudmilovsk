#!/usr/bin/env python3
"""One command for every sailing-school mesh (docs/yacht_school.md "Meshes (part C)", blender/sailing/README.md):

  python3 blender/sailing/build_all.py [--only boats,gear,house] [--previews DIR] [--no-export]
  (or: Blender -b -P blender/sailing/build_all.py -- [same flags])

  1. textures.py: the procedural pictures -> blender/sailing/out/textures/ (+ atlas_cells.json, committed)
  2. boats.py, gear.py, boathouse.py: every part, linted (lint problems are printed and stored in the manifest)
  3. kits: out/sailing_boats.glb, out/sailing_gear.glb, out/sailing_house.glb (one node per mesh at its own origin;
     Interchange takes one scene per process, so import_sailing.sh runs one process per kit)
  4. manifest.json (committed): every mesh with its kit, triangles, bounds, material slots, collision, LOD links
  5. layout json: boathouse_layout.json (committed): props, doors, lamps, spots in the building frame
  6. --previews DIR: the review renders (Cycles CPU)
"""
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)


def args():
    a = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    opt = {"only": ["boats", "gear", "house"], "previews": None, "export": True, "shots": None}
    i = 0
    while i < len(a):
        if a[i] == "--only":
            opt["only"] = a[i + 1].split(",")
            i += 1
        elif a[i] == "--previews":
            opt["previews"] = a[i + 1]
            i += 1
        elif a[i] == "--shots":
            opt["shots"] = a[i + 1].split(",")
            i += 1
        elif a[i] == "--no-export":
            opt["export"] = False
        i += 1
    return opt


def main():
    opt = args()
    import textures
    textures.main()
    signs = os.path.join(HERE, "..", "..", "tools", "make_sailing_signs.py")
    if os.path.exists(signs):
        subprocess.run([sys.executable, signs], check=False)
    import sailkit as K
    K.reset()
    reg = K.Registry()
    if "boats" in opt["only"]:
        import boats
        for b in ("luch", "optimist", "cadet"):
            boats.build(reg, b)
    if "gear" in opt["only"]:
        import gear
        gear.build(reg)
    if "house" in opt["only"]:
        import boathouse
        boathouse.build(reg)
    if opt["export"]:
        for kit in sorted({r["kit"] for r in reg.rows}):
            reg.export(kit, os.path.join(K.OUT, kit + ".glb"))
    man_path = os.path.join(HERE, "manifest.json")
    old = {}
    if os.path.exists(man_path):
        with open(man_path) as f:
            old = {r["asset"]: r for r in json.load(f).get("meshes", [])}
    for r in reg.rows:
        old[r["asset"]] = r
        if r["asset"] in reg.problems:
            r["lint"] = reg.problems[r["asset"]]
        else:
            r.pop("lint", None)
    built = {r["asset"] for r in reg.rows}
    rows = sorted(old.values(), key=lambda r: (r["kit"], r["asset"]))
    import boatspec as B
    man = {"_doc": "Every sailing-school mesh (blender/sailing/build_all.py): kit glb, triangles, bounds (Blender metres: "
                   "Unreal X = x, Y = -y, Z = z), material slots, collision ('none' | 'box' with collision_boxes in the "
                   "mesh frame, rot_z_deg about +Z | 'exact'), lod_of (goes into LOD1 of that mesh). lint = what the "
                   "mesh check found (empty = clean).",
           "meshes": rows}
    with open(man_path, "w") as f:
        f.write(B.compact_json(man) + "\n")
    tot = sum(r["tris"] for r in reg.rows)
    print("SAILING built %d meshes, %d triangles; lint problems in %d" % (len(reg.rows), tot, len(reg.problems)))
    for k, v in sorted(reg.problems.items()):
        print("  LINT", k, "; ".join(v))
    if opt["previews"]:
        import shots
        shots.run(reg, opt["previews"], opt["shots"])


main()
