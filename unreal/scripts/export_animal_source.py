"""Exports a generated breed back out of Unreal for a rebuild when its raw hi3d model.glb is gone (2026-10-29: the dogs'
were lost with old worktrees and the hi3d links had expired): SK_<Breed>'s LOD0 as OBJ (through a dynamic mesh: the FBX
exporter asserts in a commandlet) with one material per section, and its first coat (the generated
texture) and normal map as PNG, into blender/animals/gen/<Breed>/ue/. blender/animals/ue_source.py turns them into a
model.glb that build_animal.py --gen takes as it took the generated one.

UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script="export_animal_source.py Laika Shepherd Mutt"
"""
import os
import sys

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
COATS = {"Laika": "wolf", "Shepherd": "black_tan", "Mutt": "ginger_white", "Cat": "tabby", "Maru": "cream"}


def export(asset, path, exporter=None):
    t = unreal.AssetExportTask()
    t.object = asset
    t.filename = path
    t.automated = True
    t.prompt = False
    t.replace_identical = True
    if exporter:
        t.exporter = exporter
    ok = unreal.Exporter.run_asset_export_task(t)
    unreal.log_warning("EXPORT %s -> %s %s" % (asset.get_name(), path, ok))
    return ok


def obj(sk, path):
    """LOD0 as OBJ in Unreal's centimetres and axes: v per vertex id, vt per triangle corner, usemtl per section."""
    gs = unreal.GeometryScript_AssetUtils
    mesh = unreal.DynamicMesh()
    opts = unreal.GeometryScriptCopyMeshFromAssetOptions()
    lod = unreal.GeometryScriptMeshReadLOD()
    lod.lod_index = 0
    gs.copy_mesh_from_skeletal_mesh(sk, mesh, opts, lod)
    mats = [str(s.material_slot_name) for s in sk.materials]
    q = unreal.GeometryScript_MeshQueries

    def pick(r, kind):
        return next(x for x in (r if isinstance(r, tuple) else (r,)) if type(x).__name__ == kind)
    P = pick(q.get_all_vertex_positions(mesh, False), "GeometryScriptVectorList").convert_vector_list_to_array()
    T = pick(q.get_all_triangle_indices(mesh, False), "GeometryScriptTriangleList").convert_triangle_list_to_array()
    M = pick(mesh.get_all_triangle_material_i_ds(), "GeometryScriptIndexList").convert_index_list_to_array()
    with open(path, "w") as f:
        f.write("# SK %s LOD0 materials %s\n" % (sk.get_name(), mats))
        for p in P:
            f.write("v %.5f %.5f %.5f\n" % (p.x, p.y, p.z))
        cur = None
        n = 0
        for t, (i3, m) in enumerate(zip(T, M)):
            if not q.is_valid_triangle_id(mesh, t):
                continue
            if m != cur:
                f.write("usemtl %s\n" % (mats[m] if 0 <= m < len(mats) else "M%d" % m))
                cur = m
            u = [x for x in q.get_triangle_u_vs(mesh, 0, t) if type(x).__name__ == "Vector2D"]
            for w in u:
                f.write("vt %.6f %.6f\n" % (w.x, 1.0 - w.y))
            n += 3
            f.write("f %d/%d %d/%d %d/%d\n" % (i3.x + 1, n - 2, i3.y + 1, n - 1, i3.z + 1, n))
    ids, tris = P, T
    unreal.log_warning("EXPORT %s -> %s: %d verts, %d tris, materials %s" % (sk.get_name(), path, len(ids), len(tris), mats))


def main(breeds):
    for b in breeds:
        out = os.path.join(ROOT, "blender", "animals", "gen", b, "ue")
        os.makedirs(out, exist_ok=True)
        sk = unreal.load_asset("/Game/Animals/%s/SK_%s" % (b, b))
        obj(sk, os.path.join(out, "SK_%s.obj" % b))
        for name in ("T_%s_%s" % (b, COATS[b]), "T_%s_Normal" % b):
            tex = unreal.load_asset("/Game/Animals/%s/%s" % (b, name))
            export(tex, os.path.join(out, name + ".png"))


main([a for a in sys.argv[1:] if not a.startswith("-")] or ["Laika", "Shepherd", "Mutt"])
