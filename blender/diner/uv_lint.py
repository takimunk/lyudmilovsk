"""UV lint of the diner's game meshes (Dima's 3D glitch rule): UV 0 is in metres, so every face's UV area should match its
3D area and its UV edges their 3D lengths. Flags faces whose texture would show stretched (ratio of an edge's UV length to its
3D length outside 0.5..2 against the face's own mean: shear/stretch), squashed to nothing (UV area < 5 % of the 3D area) or
blown up (UV area > 4x). Picture slots (DN_Sign_*: 0..1 over the face) are skipped.

  Blender -b -P blender/diner/uv_lint.py -- [glob ...]   (default: design/diner/game/SM_Diner_*.glb; DINER_OUT honoured)

Prints per mesh UVLINT <mesh> faces=<n> bad=<n> area=<m2> and the worst few; exit code 1 when bad area > 0.01 m2.
"""
import glob
import math
import os
import sys

import bpy

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
GAME = os.environ.get("DINER_OUT") or os.path.join(ROOT, "design", "diner", "game")


def lint(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=path)
    worst = []
    nbad = nf = 0
    bad_area = 0.0
    for o in [o for o in bpy.context.scene.objects if o.type == "MESH"]:
        me = o.data
        if not me.uv_layers:
            print("UVLINT %s: no UVs" % o.name)
            return 1
        uv = me.uv_layers[0].data
        for p in me.polygons:
            m = me.materials[p.material_index] if me.materials else None
            if m and m.name.startswith("DN_Sign_"):
                continue
            nf += 1
            a3 = p.area
            if a3 < 1e-7:
                continue
            li = list(p.loop_indices)
            co = [me.vertices[me.loops[i].vertex_index].co for i in li]
            uvs = [uv[i].uv for i in li]
            a2 = 0.0
            for k in range(1, len(uvs) - 1):
                a2 += abs((uvs[k] - uvs[0]).cross(uvs[k + 1] - uvs[0])) / 2
            ratios = []
            for k in range(len(li)):
                l3 = (co[(k + 1) % len(li)] - co[k]).length
                if l3 > 1e-5:
                    ratios.append((uvs[(k + 1) % len(li)] - uvs[k]).length / l3)
            why = None
            if a2 < 0.05 * a3:
                why = "squashed"
            elif a2 > 4.0 * a3:
                why = "blown"
            elif ratios and max(ratios) > 2.0 * max(1e-6, min(ratios)) and a3 > 2e-5:
                why = "stretched %.1fx" % (max(ratios) / max(1e-6, min(ratios)))
            if why:
                nbad += 1
                bad_area += a3
                c = p.center
                worst.append((a3, why, m.name if m else "-", tuple(round(v, 3) for v in c)))
    name = os.path.basename(path)[:-4]
    print("UVLINT %s faces=%d bad=%d area=%.4f" % (name, nf, nbad, bad_area))
    for a3, why, mat, c in sorted(worst, reverse=True)[:8]:
        print("UVLINT   %s %s area %.4f at %s" % (why, mat, a3, c))
    return 1 if bad_area > 0.01 else 0


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    files = sorted(f for g in (args or [os.path.join(GAME, "SM_Diner_*.glb")]) for f in glob.glob(g))
    rc = 0
    for f in files:
        rc |= lint(f)
    sys.exit(rc)


main()
