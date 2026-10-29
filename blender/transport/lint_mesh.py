"""Overlap lint for the cable car's meshes (Dima's 3D glitch rule): faces lying on other faces (z-fighting), the thing
that makes floors flicker in the game.

  Blender -b -P blender/transport/lint_mesh.py -- [glob ...]        (default: design/transport/game/SM_TR_*.glb)

For every face: is there another face, not sharing a vertex with it, within GAP of its middle, facing the same way,
whose plane is within GAP? Those pairs flicker. Prints per mesh the count and area of such faces and the worst
spots (LINT lines); exit code 1 when any mesh has them.
"""
import bmesh
import bpy
import glob
import os
import sys
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
GAP = 0.012          # m: closer than this and parallel = z-fighting at a distance in the game


def lint(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=path)
    objs = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    bm = bmesh.new()
    for o in objs:
        me = o.data.copy()
        me.transform(o.matrix_world)
        bm.from_mesh(me)
    # the glTF importer splits vertices where the normals differ (flat shading): weld them again, or neighbouring faces
    # of a gently curved surface look like strangers lying on each other
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bm.faces.ensure_lookup_table()
    bm.normal_update()
    t = BVHTree.FromBMesh(bm)
    bad = []
    for f in bm.faces:
        a = f.calc_area()
        if a < 2e-4:
            continue
        c = f.calc_center_median()
        n = f.normal
        vs = {v.index for v in f.verts}
        for loc, nor, idx, dist in t.find_nearest_range(c, GAP):
            if idx == f.index:
                continue
            g = bm.faces[idx]
            gs = {v.index for v in g.verts}
            if vs & gs and vs != gs:          # neighbours; the very same corners twice is a doubled face
                continue
            if g.normal.dot(n) < 0.985:        # facing the same way (opposite faces cull each other)
                continue
            if abs((c - g.verts[0].co).dot(g.normal)) > GAP:
                continue
            d = loc - c
            if (d - g.normal * d.dot(g.normal)).length > 0.002:   # the middle isn't over g: they only touch at an edge
                continue
            bad.append((a, c.copy(), f.material_index, g.material_index))
            break
    area = sum(b[0] for b in bad)
    name = os.path.basename(path)[:-4]
    print("LINT %s: %d faces on faces, %.2f m2" % (name, len(bad), area), flush=True)
    mats = [m.name for o in objs for m in o.data.materials]
    for a, c, mf, mg in sorted(bad, key=lambda b: -b[0])[:6]:
        print("LINT   %.3f m2 at (%.2f, %.2f, %.2f) %s on %s" % (a, c.x, c.y, c.z, mats[mf] if mf < len(mats) else mf,
                                                              mats[mg] if mg < len(mats) else mg), flush=True)
    return len(bad)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    pats = argv or [os.path.join(ROOT, "design", "transport", "game", "SM_TR_*.glb")]
    paths = sorted(p for pat in pats for p in glob.glob(pat if os.path.isabs(pat) else os.path.join(ROOT, pat)))
    total = sum(1 for p in paths if lint(p) > 0)
    print("LINT done: %d of %d meshes with faces on faces" % (total, len(paths)), flush=True)
    sys.exit(1 if total else 0)


main()
