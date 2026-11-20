"""Builds the Aton coast's props (docs/south.md) as game meshes, with the North's kit and atlas (blender/north).

    python3 blender/south/build_south.py [module] [kind ...]      every props_*.py module here, or the one named
                                                                  (props_walk), and of its kinds all or the ones named
                                                                  (cliff_stair boardwalk:1 ...)
    /Applications/Blender.app/Contents/MacOS/Blender -b --python blender/south/build_south.py -- [module] [kind ...]

Each module blender/south/props_<group>.py has KINDS = {kind: (builder(variant) -> (Builder, extra), {"variants": [...],
"collision": "exact" | "box" | "none", ...})}, exactly like blender/north/kinds.py. Writes
blender/export/south/<kind>[_v<variant>].glb and one manifest per module, blender/export/south/manifest_<group>.json
(the same rows as the North's manifest.json: kind, variant, file, tris, size, collision, seats, notes, bounds and the
kind's own fields such as axis and collision_boxes). The GLBs carry material names only (M_North_Atlas,
M_South_Text_<id>): unreal/scripts/import_south.py assigns the materials by slot name.
"""
import glob
import importlib
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "north"))
import bpy  # noqa: E402

import northkit as K  # noqa: E402

EXPORT = os.path.normpath(os.path.join(HERE, "..", "export", "south"))


def build_one(mod, kind, variant):
    fn, meta = mod.KINDS[kind]
    b, extra = fn(variant)
    ob = K.make_object(b)
    tris = K.tri_count(ob)
    lint = K.uv_lint(ob, kind)
    if lint:
        print("  UV lint %s v%d: %d faces: %s" % (kind, variant, len(lint), lint[:4]))
    mn, mx = K.bbox(ob)
    name = kind if len(meta["variants"]) == 1 else "%s_v%d" % (kind, variant)
    K.export_glb(ob, os.path.join(EXPORT, name + ".glb"))
    me = ob.data
    bpy.data.objects.remove(ob)
    bpy.data.meshes.remove(me)
    entry = {"kind": kind, "variant": variant, "file": name + ".glb", "tris": tris,
             "size": [round(mx.x - mn.x, 3), round(mx.y - mn.y, 3), round(mx.z - mn.z, 3)],
             "collision": meta["collision"], "seats": extra.get("seats", []), "notes": extra.get("notes", meta.get("notes", ""))}
    if meta.get("body"):
        entry["body_size"] = meta["body"]
    entry.update(extra.get("manifest", {}))
    entry["bounds_min"] = [round(v, 3) for v in mn]
    entry["bounds_max"] = [round(v, 3) for v in mx]
    entry["materials"] = b.mat_slots
    entry["uv_lint"] = len(lint)
    print("%-16s v%d  %5d tris  size %s  %s" % (kind, variant, tris, entry["size"], meta["collision"]))
    return entry


def build_module(name, args):
    mod = importlib.import_module(name)
    group = name[len("props_"):]
    path = os.path.join(EXPORT, "manifest_%s.json" % group)
    old = {}
    if os.path.exists(path):
        with open(path) as f:
            old = {(e["kind"], e["variant"]): e for e in json.load(f).get("meshes", [])}
    for kind, (_fn, meta) in mod.KINDS.items():
        for v in meta["variants"]:
            if not args or kind in args or "%s:%d" % (kind, v) in args:
                old[(kind, v)] = build_one(mod, kind, v)
    order = {k: i for i, k in enumerate(mod.KINDS)}
    meshes = [old[k] for k in sorted(old, key=lambda k: (order.get(k[0], 99), k[1])) if k[0] in mod.KINDS]
    out = {"frame": "metres, origin on the ground under the middle unless the kind's notes say otherwise, Blender -Y is the "
                    "front, Z up; seats [x, y, z, yaw]: yaw = degrees of the sitter's facing, counter-clockwise seen from "
                    "above, 0 = toward the object's front (-Y); size and bounds are in Blender axes (x, y, z)",
           "front_yaw": 90.0, "asset_names": "SM_South_<Kind>[_V<n>] (_V only for kinds with several variants)",
           "atlas": "M_North_Atlas (the North's, blender/north/gen); text slots M_South_Text_<id> from "
                    "textures/localized/<lang>/south_<id>.png",
           "meshes": meshes}
    os.makedirs(EXPORT, exist_ok=True)
    with open(path, "w") as f:
        json.dump(out, f, indent=1, ensure_ascii=False)
        f.write("\n")
    print("%s: %d meshes" % (os.path.basename(path), len(meshes)))


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    args = [a for a in argv if not a.startswith("--")]
    mods = sorted(os.path.splitext(os.path.basename(p))[0] for p in glob.glob(os.path.join(HERE, "props_*.py")))
    named = [a for a in args if a in mods]
    kinds = [a for a in args if a not in mods]
    for name in named or mods:
        build_module(name, kinds)


if __name__ == "__main__":
    main()
