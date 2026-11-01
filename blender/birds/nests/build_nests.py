"""Bird nest kit for Lyudmilovsk (open nests in four stages, nest boxes, tree hole, burrow, tufts, eggs, bill bits).

    Blender -b --factory-startup --python blender/birds/nests/build_nests.py -- [--only <Id>] [--previews] [--no-lint] [--no-export]

Writes blender/birds/nests/export/: nest_kit.glb, nest_kit.json, T_NestKit_{C,N,R}.png, previews/*.png (with --previews).
Everything is seeded (zlib.crc32 of the species id + stage). See README.md.
"""
import json
import math
import os
import sys
import time
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.dirname(HERE))
import bpy  # noqa: E402
import numpy as np  # noqa: E402

import lint_bird  # noqa: E402
import nest_atlas as AT  # noqa: E402
import nest_kinds as NK  # noqa: E402
import nest_misc as NM  # noqa: E402
import nest_specs as SP  # noqa: E402

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ONLY = argv[argv.index("--only") + 1] if "--only" in argv else None
PREVIEWS = "--previews" in argv
LINT = "--no-lint" not in argv
EXPORT = "--no-export" not in argv
OUT = os.path.join(HERE, "export")
os.makedirs(OUT, exist_ok=True)
TABLE = {s["id"]: s for s in json.load(open(os.path.join(HERE, "nest_table.json")))["species"]}
OPEN_KINDS = ("Cup", "Dome", "Platform", "Scrape", "Mound", "MudCup")
STAGES = ("B1", "B2", "Full", "Old")


def make_material(with_images=True):
    m = bpy.data.materials.new("M_NestKit")
    m.use_nodes = True
    nt = m.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    if with_images:
        def img(name, cs):
            p = os.path.join(OUT, name)
            im = bpy.data.images.load(p)
            im.colorspace_settings.name = cs
            im.name = name
            n = nt.nodes.new("ShaderNodeTexImage")
            n.image = im
            n.interpolation = "Linear"
            return n
        c = img("T_NestKit_C.png", "sRGB")
        r = img("T_NestKit_R.png", "Non-Color")
        nn = img("T_NestKit_N.png", "Non-Color")
        nt.links.new(c.outputs["Color"], bsdf.inputs["Base Color"])
        nt.links.new(r.outputs["Color"], bsdf.inputs["Roughness"])
        nm = nt.nodes.new("ShaderNodeNormalMap")
        nt.links.new(nn.outputs["Color"], nm.inputs["Color"])
        nt.links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
    return m


def lint_one(ob, names, label):
    if not LINT:
        return {"tris": len(ob.data.loop_triangles) if ob.data.loop_triangles else 0, "issues": 0}
    return lint_bird.lint(ob, AT.Atlas, label, names, verbose=bool(os.environ.get("NEST_LINT_V")))


REPORT = []     # one dict per mesh


def tri_count(ob):
    ob.data.calc_loop_triangles()
    return len(ob.data.loop_triangles)


def bounds(ob):
    co = np.empty(len(ob.data.vertices) * 3)
    ob.data.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    return co.min(0).tolist(), co.max(0).tolist(), co


def register(ob, names, info, budget):
    mn, mx, co = bounds(ob)
    t = tri_count(ob)
    res = lint_one(ob, names, ob.name) if LINT else {}
    row = dict(name=ob.name, tris=t, boundsMin=[round(x, 5) for x in mn], boundsMax=[round(x, 5) for x in mx],
               outerRadiusXY=round(float(np.hypot(co[:, 0], co[:, 1]).max()), 4), budget=budget, **info)
    for k in ("degenerate", "uv_zero", "uv_stretch", "uv_outside", "zfight", "inside", "issues", "crossing", "near_layers"):
        if k in res:
            row["lint_" + k] = res[k]
    REPORT.append(row)
    flag = "" if (not LINT or res.get("issues", 0) == 0) else "  <<< " + str({k: res[k] for k in ("degenerate", "uv_zero", "uv_stretch", "uv_outside", "zfight", "inside")})
    over = "  OVER BUDGET %d" % budget if t > budget else ""
    print(f"MESH {ob.name:34s} tris {t:6d}{over}{flag}", flush=True)
    if res.get("zfight_pairs") and res.get("issues"):
        print("     pairs", res["zfight_pairs"][:4])
    if res.get("examples") and res.get("issues"):
        print("     ex", res["examples"])
    return res


def egg_info(sid, row):
    import nest_preview as PV
    L, B = AT.EGGS[sid][:2]
    rin = PV.rin_of(sid)
    slots, rf = PV.pack_eggs(np.random.default_rng(zlib.crc32(sid.encode())), row["clutchMax"], L, B, rin * 0.72)
    return dict(cupInnerRadius=round(rin, 4), eggFloorZ=PV.floor_z(sid), clutchMax=row["clutchMax"], eggsFit=len(slots),
                eggFloorRadiusUsed=round(rf, 4),
                eggSlots=[[round(x, 4), round(y, 4), round(math.degrees(yw) % 180, 1)] for x, y, yw in slots])


def budget_for(sid, kind):
    R = TABLE[sid]["radius"]
    if R >= 0.6:
        return 16000
    if R >= 0.25:
        return 8000
    return 3000


def main():
    t0 = time.time()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    cells = AT.build(OUT)
    print("atlas written", round(time.time() - t0, 1), "s", flush=True)
    lint_bird.PARAMETRIC = tuple(lint_bird.PARAMETRIC) + tuple(k for k in AT.CELLS if k.startswith("egg_"))
    mat = make_material()
    objs = {}
    ids = [i for i in TABLE if TABLE[i]["nest"] in OPEN_KINDS and (ONLY is None or ONLY == i)]
    for sid in ids:
        row = TABLE[sid]
        sp = SP.SPECS[sid]
        for stage in STAGES:
            mb = NK.build_open(sid, sp, stage, row["radius"])
            ob = mb.to_mesh(f"SM_Nest_{sid}_{'Full' if stage == 'Full' else stage}", mat)
            objs[ob.name] = ob
            info = dict(kind="OpenNest", species=sid, stage=stage, nestKind=row["nest"],
                        origin="centre of the cup floor (eggs rest here), +X forward, Z up", tableRadius=row["radius"])
            if stage == "Full":
                info.update(egg_info(sid, row))
            if row["nest"] == "MudCup":
                info.update(wallPlaneX=-sp["L"], note="half cup: the flat open back faces -X and sits on the wall plane x = wallPlaneX "
                            "(-0.3*radius; a plane at -radius would leave no cup). Cup axis vertical through the origin, front reaches +radius.")
            register(ob, mb.names, info, budget_for(sid, row["nest"]))
    for name, fn, info, budget in NM.all_misc(ONLY, TABLE):
        mb = fn()
        ob = mb.to_mesh(name, mat)
        objs[name] = ob
        register(ob, mb.names, info, budget)
    if EXPORT:
        export(objs)
    summary()
    if PREVIEWS:
        import nest_preview as PV
        PV.run(objs, TABLE, OUT, only=ONLY, mat=mat)
    print("done", round(time.time() - t0, 1), "s")


def export(objs):
    for ob in objs.values():
        if "lint_part" in ob.data.attributes:
            ob.data.attributes.remove(ob.data.attributes["lint_part"])
    bpy.ops.object.select_all(action="DESELECT")
    for ob in objs.values():
        ob.select_set(True)
    path = os.path.join(OUT, "nest_kit.glb" if ONLY is None else f"nest_kit_{ONLY}.glb")
    kw = dict(filepath=path, export_format="GLB", use_selection=True, export_yup=True, export_apply=False, export_normals=True,
              export_texcoords=True, export_materials="EXPORT", export_skins=False, export_animations=False,
              export_vertex_color="NONE", export_image_format="NONE")
    props = bpy.ops.export_scene.gltf.get_rna_type().properties
    bpy.ops.export_scene.gltf(**{k: v for k, v in kw.items() if k in props or k == "filepath"})
    print("EXPORTED", path, os.path.getsize(path), "bytes")
    man = dict(
        _doc="Nest kit manifest. Blender axes (Z up, forward +X) in the numbers below; the glb is exported with the glTF +Y-up "
             "conversion like build_bird.py. All sizes metres. 'atlas' cells are texels in the 2048 atlas (y up, v = y/2048).",
        material="M_NestKit", textures=["T_NestKit_C.png", "T_NestKit_N.png", "T_NestKit_R.png"], atlasSize=AT.SIZE,
        atlasCells=AT.CELLS, meshes={r["name"]: r for r in REPORT})
    mp = os.path.join(OUT, "nest_kit.json" if ONLY is None else f"nest_kit_{ONLY}.json")
    json.dump(man, open(mp, "w"), indent=1)
    print("MANIFEST", mp)


def summary():
    print("\n==== LINT SUMMARY ====")
    cols = ("degenerate", "uv_zero", "uv_stretch", "uv_outside", "zfight", "inside")
    print(f"{'mesh':36s} {'tris':>6s} " + " ".join(f"{c[:8]:>8s}" for c in cols))
    tot = {c: 0 for c in cols}
    over = 0
    for r in REPORT:
        for c in cols:
            tot[c] += r.get("lint_" + c, 0)
        over += r["tris"] > r["budget"]
        print(f"{r['name']:36s} {r['tris']:6d} " + " ".join(f"{r.get('lint_' + c, 0):8d}" for c in cols))
    print(f"TOTAL meshes {len(REPORT)}  " + " ".join(f"{c}={tot[c]}" for c in cols) + f"  over_budget={over}")


main()
