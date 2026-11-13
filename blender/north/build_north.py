"""Builds the North's camp and trail props (docs/north.md) as game meshes.

    python3 blender/north/build_north.py [kind ...]            all kinds, or the ones named (tent_dome log_bench:1 ...)
    python3 blender/north/build_north.py --textures            also regenerate the atlas and the sign textures first

Writes blender/export/north/<kind>[_v<variant>].glb and merges the kinds built into manifest.json
({kind, variant, file, tris, size [x, y, z], collision, seats, notes}). Shared atlas: blender/north/gen/north_atlas*.png
(northtex.py); sign textures: textures/localized/<lang>/north_*.png (tools/north_signs.py).
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy  # noqa: E402

import northkit as K  # noqa: E402
import kinds as props  # noqa: E402

MANIFEST = os.path.join(K.EXPORT, "manifest.json")


def build_one(kind, variant):
    fn, meta = props.KINDS[kind]
    b, extra = fn(variant)
    ob = K.make_object(b)
    tris = K.tri_count(ob)
    lint = K.uv_lint(ob, kind)
    if lint:
        print("  UV lint %s v%d: %d faces: %s" % (kind, variant, len(lint), lint[:4]))
    mn, mx = K.bbox(ob)
    name = kind if len(meta["variants"]) == 1 else "%s_v%d" % (kind, variant)
    path = os.path.join(K.EXPORT, name + ".glb")
    K.export_glb(ob, path)
    me = ob.data
    bpy.data.objects.remove(ob)
    bpy.data.meshes.remove(me)
    entry = {"kind": kind, "variant": variant, "file": name + ".glb", "tris": tris,
             "size": [round(mx.x - mn.x, 3), round(mx.y - mn.y, 3), round(mx.z - mn.z, 3)],
             "collision": meta["collision"], "seats": extra.get("seats", []), "notes": extra.get("notes", meta.get("notes", ""))}
    if meta.get("body"):
        entry["body_size"] = meta["body"]       # the object without its guy lines and pegs
    entry["bounds_min"] = [round(v, 3) for v in mn]
    entry["bounds_max"] = [round(v, 3) for v in mx]
    entry["materials"] = b.mat_slots
    print("%-14s v%d  %5d tris  size %s  %s" % (kind, variant, tris, entry["size"], meta["collision"]))
    return entry


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if "--textures" in sys.argv:
        import northtex
        northtex.write(K.GEN)
    want = []
    for kind, (fn, meta) in props.KINDS.items():
        for v in meta["variants"]:
            if not args or kind in args or "%s:%d" % (kind, v) in args:
                want.append((kind, v))
    old = {}
    if os.path.exists(MANIFEST):
        with open(MANIFEST) as f:
            old = {(e["kind"], e["variant"]): e for e in json.load(f).get("meshes", [])}
    for kind, v in want:
        old[(kind, v)] = build_one(kind, v)
    order = {k: i for i, k in enumerate(props.KINDS)}
    meshes = [old[k] for k in sorted(old, key=lambda k: (order.get(k[0], 99), k[1])) if k[0] in props.KINDS]
    out = {"frame": "metres, origin on the ground under the middle, Blender -Y is the front, Z up; seats [x, y, z, yaw]: yaw "
                    "= degrees of the sitter's facing, counter-clockwise seen from above, 0 = toward the object's front (-Y); "
                    "size and bounds are in Blender axes (x, y, z)",
           "front_yaw": 90.0, "asset_names": "SM_North_<Kind>[_V<n>] (_V only for kinds with several variants)", "atlas": "M_North_Atlas: no image in the GLB; the engine builds it from blender/north/gen/north_atlas.png + _n.png (2048); text slots M_North_Text_<id> from textures/localized/<lang>/north_<id>.png",
           "meshes": meshes}
    os.makedirs(K.EXPORT, exist_ok=True)
    with open(MANIFEST, "w") as f:
        json.dump(out, f, indent=1, ensure_ascii=False)
        f.write("\n")
    print("manifest: %d meshes" % len(meshes))


if __name__ == "__main__":
    main()
