"""Builds the ranger's hut: hut.glb (shell), hut_interior.glb (furniture), hut_door.glb (leaf), manifest_hut.json, and with
flags the checks and previews. Headless, with the `bpy` pip module (5.3):

    python3 blender/north/hut_build.py                 # build + export + manifest + mesh checks + passage check
    python3 blender/north/hut_build.py --previews      # also render the previews to /mnt/project-files/north/
    python3 blender/north/hut_build.py --previews-only # render only (no export)

Output: blender/export/north/hut.glb, hut_interior.glb, hut_door.glb, manifest_hut.json. Frame: hut-local metres, origin on
the ground under the middle of the footprint (cabin + porch), Blender -Y is the front, Z up. The GLBs are glTF (Y up): the
exporter turns Blender (x, y, z) into glTF (x, z, -y); the manifest is in the Blender frame.
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import bpy  # noqa: E402,I001  (first: it makes `bmesh` and `mathutils` importable)
import bmesh  # noqa: E402
from mathutils import Vector  # noqa: E402
from mathutils.bvhtree import BVHTree  # noqa: E402

import hut_common as C  # noqa: E402
import hut_door as D  # noqa: E402
import hut_interior as I  # noqa: E402
import hut_shell as S  # noqa: E402

F = C.F
PREVIEW_DIR = os.environ.get("HUT_PREVIEW_DIR", "/mnt/project-files/north/")


def bbox(objs):
    lo = Vector((1e9,) * 3)
    hi = Vector((-1e9,) * 3)
    for o in objs:
        for v in o.data.vertices:
            p = o.matrix_world @ v.co
            lo = Vector(min(a, b) for a, b in zip(lo, p))
            hi = Vector(max(a, b) for a, b in zip(hi, p))
    return lo, hi


def r3(v):
    return [round(float(x), 3) for x in v]


def textures_of(objs):
    out = {}
    for o in objs:
        for m in o.data.materials:
            for n in m.node_tree.nodes:
                if n.type == "TEX_IMAGE" and n.image:
                    out[n.image.name] = "%dx%d" % tuple(n.image.size)
    return out


def build_scene():
    C.reset()
    main, ceil, roof, info = S.build()
    interior = [m.finish() for m in I.build()]
    door = D.build().finish()
    return main, ceil, roof, info, interior, door


def export_all(main, ceil, roof, interior, door):
    os.makedirs(C.OUT, exist_ok=True)
    shell = C.join_copy([main, ceil, roof], "hut")
    C.export_glb([shell], os.path.join(C.OUT, "hut.glb"))
    tris = {"hut": C.tri_count(shell)}
    lo, hi = bbox([shell])
    size = {"hut": r3(hi - lo)}
    tex = {"hut": textures_of([shell])}
    bpy.data.objects.remove(shell)
    C.export_glb(interior, os.path.join(C.OUT, "hut_interior.glb"))
    tris["hut_interior"] = sum(C.tri_count(o) for o in interior)
    lo, hi = bbox(interior)
    size["hut_interior"] = r3(hi - lo)
    tex["hut_interior"] = textures_of(interior)
    C.export_glb([door], os.path.join(C.OUT, "hut_door.glb"))
    tris["hut_door"] = C.tri_count(door)
    lo, hi = bbox([door])
    size["hut_door"] = r3(hi - lo)
    tex["hut_door"] = textures_of([door])
    return tris, size, tex


# ---- checks ---------------------------------------------------------------------------------------------------------

def sanity(objs):
    """Per object: non-manifold edges, closed islands with a negative volume (flipped normals), zero-area faces."""
    report = {}
    for o in objs:
        bm = bmesh.new()
        bm.from_mesh(o.data)
        bm.faces.ensure_lookup_table()
        nm = sum(1 for e in bm.edges if len(e.link_faces) != 2)
        bad_area = sum(1 for f in bm.faces if f.calc_area() < 1e-9)
        seen = set()
        flipped = 0
        closed = 0
        for f0 in bm.faces:
            if f0.index in seen:
                continue
            stack, isl = [f0], []
            seen.add(f0.index)
            while stack:
                f = stack.pop()
                isl.append(f)
                for e in f.edges:
                    for g in e.link_faces:
                        if g.index not in seen:
                            seen.add(g.index)
                            stack.append(g)
            if all(len(e.link_faces) == 2 for f in isl for e in f.edges):
                closed += 1
                vol = 0.0
                for f in isl:
                    vs = [v.co for v in f.verts]
                    for i in range(1, len(vs) - 1):
                        vol += vs[0].dot(vs[i].cross(vs[i + 1])) / 6.0
                if vol < 0:
                    flipped += 1
        report[o.name] = dict(nonmanifold_edges=nm, closed_islands=closed, flipped_islands=flipped, zero_area=bad_area)
        bm.free()
    return report


def bvh(objs):
    deps = bpy.context.evaluated_depsgraph_get()
    verts, polys = [], []
    for o in objs:
        oe = o.evaluated_get(deps)
        me = oe.to_mesh()
        me.calc_loop_triangles()
        base = len(verts)
        verts += [oe.matrix_world @ v.co for v in me.vertices]
        polys += [tuple(base + i for i in t.vertices) for t in me.loop_triangles]
        oe.to_mesh_clear()
    return BVHTree.FromPolygons(verts, polys)


def passage_check(shell_objs, interior, door, step=0.02):
    """A 0.7 x 0.7 x 1.8 m box from the porch steps, through the door (leaf open 95 deg), round the table and chair, to the
    bunk: no triangle of the box may meet the shell or any furniture."""
    hx, hz = 0.35, 1.8
    door.location = Vector(D.DOOR_HINGE)
    door.rotation_euler = (0, 0, math.radians(-95))
    bpy.context.view_layer.update()
    world = bvh(shell_objs + [o for o in interior if o.name != "Rug"] + [door])
    path = [(0.5, -5.2), (0.5, -1.0), (0.5, -0.6), (-0.30, 0.2), (-0.30, 1.8), (0.60, 1.9), (0.60, 2.4)]

    def ground(y):
        if y < -4.10:
            return 0.0
        if y < -3.80:
            return 0.133
        if y < -3.50:
            return 0.267
        return F

    hits = []
    n = 0
    tris = [(0, 1, 3), (0, 3, 2), (4, 6, 7), (4, 7, 5), (0, 4, 5), (0, 5, 1), (1, 5, 7), (1, 7, 3), (3, 7, 6), (3, 6, 2), (2, 6, 4), (2, 4, 0)]
    for (x0, y0), (x1, y1) in zip(path[:-1], path[1:]):
        L = math.hypot(x1 - x0, y1 - y0)
        k = max(1, int(L / step))
        for i in range(k + 1):
            t = i / k
            x, y = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
            z0 = max(ground(y + hx), ground(y)) + 0.003
            vs = [Vector((x + sx * hx, y + sy * hx, z0 + sz * hz)) for sz in (0, 1) for sy in (-1, 1) for sx in (-1, 1)]
            bt = BVHTree.FromPolygons(vs, tris)
            n += 1
            if world.overlap(bt):
                hits.append((round(x, 2), round(y, 2)))
    door.location = (0, 0, 0)
    door.rotation_euler = (0, 0, 0)
    return dict(samples=n, hits=len(hits), first_hits=hits[:5], box=[2 * hx, 2 * hx, hz], path=path)


# ---- the manifest ---------------------------------------------------------------------------------------------------

def manifest(tris, size, tex, interior, info, checks):
    bunk_head = [1.43, 2.98, round(F + 0.64, 3)]
    bunk_feet = [1.43, 1.40, round(F + 0.60, 3)]
    pieces = {}
    big = {"Stove", "Bunk", "Table", "Chair", "WoodBox", "Stool", "ShelfBack", "ShelfLeft"}
    for o in interior:
        lo, hi = bbox([o])
        pieces[o.name] = dict(tris=C.tri_count(o), min=r3(lo), max=r3(hi), collision="box" if o.name in big else "none")
    d = D.DOOR_HINGE
    q = I.MAP_QUAD
    return {
        "frame": "hut-local metres, Blender axes (-Y front = door and porch, +Z up); origin on the ground under the middle of the "
                 "footprint (cabin + porch). glTF Y-up files: glTF (x, y, z) = Blender (x, z, -y). Every object of every file has "
                 "its origin at the hut origin.",
        "builder": "blender/north/hut_build.py (hut_shell.py, hut_interior.py, hut_door.py, hut_common.py, hut_tex.py)",
        "entries": [
            {"kind": "hut", "variant": 0, "file": "hut.glb", "tris": tris["hut"], "size": size["hut"], "collision": "exact",
             "textures": tex["hut"], "seats": [],
             "notes": "shell: log walls both sides, floor, ceiling and beams, roof, porch, steps, footings, windows (HUT_Glass slot), "
                      "shutters, rain barrel, bench, ski rack, firewood lean-to, weather post, antenna and wire, flue above the ceiling"},
            {"kind": "hut_interior", "variant": 0, "file": "hut_interior.glb", "tris": tris["hut_interior"], "size": size["hut_interior"],
             "collision": "box", "textures": tex["hut_interior"], "seats": [[-0.95, 0.55, round(F + 0.475, 3), 180.0]],
             "notes": "one object per piece; box collision for the pieces with collision 'box' in `pieces`", "pieces": pieces},
            {"kind": "hut_door", "variant": 0, "file": "hut_door.glb", "tris": tris["hut_door"], "size": size["hut_door"],
             "collision": "none", "textures": tex["hut_door"], "seats": [],
             "notes": "leaf only, origin = hinge axis at the bottom; closed: leaf along +X from the origin, outside face toward -Y"},
        ],
        "front_yaw": 90.0,
        "front_yaw_note": "Unreal degrees of the mesh's front in its own frame (importer turns actors by yaw - front_yaw); Blender -Y front, default 90",
        "door": {"name": "hut_door", "at": [0.5, round(S.YF + S.R, 3), F], "out": [0, -1], "hole": [1.0, round(2.42 - F, 3)],
                 "wall": round(2 * S.R, 3), "leaves": 1, "hinge": "left", "frame": False, "open": False, "locked": False,
                 "note": "place_doors.py entry in hut-local Blender metres; the hut builds its own lining and casings (frame false)"},
        "door_detail": {
            "hinge": r3(d), "hinge_side": "left, seen from outside (standing on the porch facing the door)",
            "hinge_side_alyuddoor": "ALyudDoor's +X faces the swing side (the porch): seen from there the hinge is on the LEFT too; "
                                    "set Hinge = Left with +X toward the porch (-Y here)",
            "swing": "outward onto the porch (toward -Y), open angle 95 deg",
            "leaf_size": [D.LEAF_W, D.LEAF_T, D.LEAF_H], "leaf_origin_is": "hinge axis, bottom",
            "opening_clear": {"x": [0.0, 1.0], "width": 1.0, "height_from_floor": round(2.42 - F, 3), "z_top": 2.42},
            "opening_centre": [0.5, round(S.YF, 3), F], "wall_y": [S.YF, round(S.YFI, 3)], "wall_thickness": round(2 * S.R, 3),
            "threshold_height_over_floor": 0.0, "outward_normal": [0, -1, 0],
            "lining": "3 cm plank lining across the wall, 8 cm casings on both faces, all inside hut.glb (door actor: bBuildFrame off)",
        },
        "bed": {
            "kind": "bunk (two tier), lower bed for lying", "head": bunk_head, "feet": bunk_feet, "head_yaw_deg": 90.0,
            "note": "yaw = direction feet -> head seen from above, 0 = +X, 90 = +Y (head against the back wall); the person lies on the "
                    "back on the mattress, pillow top at head z; access from the open side facing the room (x < 0.98)",
            "stand_by_bed": [0.62, 2.0, F],
        },
        "chair": {"seat": [-0.95, 0.55, round(F + 0.475, 3)], "yaw_deg": 180.0, "note": "yaw = facing direction (toward the table, -X)"},
        "stool": {"seat": [-0.45, 2.42, round(F + 0.435, 3)], "yaw_deg": 0.0},
        "stove": {"flue_top": r3(info["flue_top"]), "smoke_origin": r3((info["flue_top"][0], info["flue_top"][1], info["flue_top"][2] + 0.05)),
                  "glow_slit": [-1.26, 2.58, round(F + 0.16, 3)], "kettle": [-1.50, 2.84, round(F + 0.79, 3)]},
        "lamps": [
            {"id": "kerosene", "pos": [-1.64, 0.14, round(I.TABLE_TOP + 0.20, 3)], "colour": "#ffb060", "kind": "flame"},
            {"id": "ceiling_bulb", "pos": list(I.BULB), "colour": "#ffe2b0", "kind": "bulb"},
            {"id": "stove_glow", "pos": [-1.26, 2.5, round(F + 0.16, 3)], "colour": "#ff7a30", "kind": "glow"},
        ],
        "emissive_slots": {"HUT_LampGlass": "kerosene lamp chimney", "HUT_BulbGlass": "the ceiling bulb", "HUT_Fire": "wick flame and the stove glow slit",
                           "HUT_Glass": "window panes (all windows share the slot): swap an emissive/lit glass at night"},
        "radio": {"pos": [-1.62, 0.84, round(I.TABLE_TOP + 0.10, 3)], "face_yaw_deg": 0.0, "note": "faces +X, toward the chair"},
        "wall_map": {"corners_seen_from_room": {"bottom_left": [q["x"], q["y1"], q["z0"]], "bottom_right": [q["x"], q["y0"], q["z0"]],
                                               "top_right": [q["x"], q["y0"], q["z1"]], "top_left": [q["x"], q["y1"], q["z1"]]},
                     "normal": [-1, 0, 0], "uv": "0..1 over the quad (bottom-left = 0,0), material HUT_Map (a placeholder the game replaces)",
                     "size_m": [round(q["y1"] - q["y0"], 3), round(q["z1"] - q["z0"], 3)]},
        "footprint": {"cabin": [[-2.1, -1.9], [2.1, -1.9], [2.1, 3.5], [-2.1, 3.5]],
                      "with_porch": [[-2.1, -3.5], [2.1, -3.5], [2.1, 3.5], [-2.1, 3.5]],
                      "with_steps": [[-2.1, -4.1], [2.1, -4.1], [2.1, 3.5], [-2.1, 3.5]],
                      "roof_outline": [[-2.5, -3.8], [2.5, -3.8], [2.5, 3.8], [-2.5, 3.8]],
                      "all_details_max_xy": [3.45, 4.45], "interior": {"x": [-1.88, 1.88], "y": [-1.68, 3.28]}},
        "level_pad_radius": {"flat_min": 5.0, "recommended": 6.5,
                             "note": "the farthest fixed detail is the lean-to corner (1.9, 4.45) at 4.84 m; the weather post, antenna "
                                     "anchor, steps and roof corners lie within 4.6 m; fits tools/north.py's 9-10 m pad"},
        "floor_height_over_ground": F, "porch_deck_height": F, "steps": {"count": 2, "riser": 0.133, "width": 1.5, "x": [-0.25, 1.25], "to_y": -4.1},
        "ceiling_height_over_floor": round(S.CEIL - F, 3), "roof_ridge_z": round(S.ZR + 0.13, 3),
        "free_spots": {"inside": [[0.35, -0.7, F], [-0.2, 1.9, F], [0.45, 2.75, F]], "porch": [[-1.2, -2.8, F], [0.6, -3.05, F]],
                       "note": "x, y, z of a standing person's feet"},
        "windows": [{"id": "front", "centre": [-1.0, S.YF, 1.645], "size": [0.8, 0.93], "wall": "front"},
                    {"id": "side", "centre": [-S.XO, 0.55, 1.735], "size": [0.8, 0.92], "wall": "left"}],
        "checks": checks,
    }


def run_checks(main, ceil, roof, interior, door):
    shell_objs = [main, ceil, roof]
    rep = sanity(shell_objs + interior + [door])
    bad = {k: v for k, v in rep.items() if v["flipped_islands"] or v["zero_area"]}
    ps = passage_check(shell_objs, interior, door)
    return {"flipped_or_degenerate": bad, "nonmanifold_edges_total": sum(v["nonmanifold_edges"] for v in rep.values()),
            "closed_islands_total": sum(v["closed_islands"] for v in rep.values()), "passage_box": ps}


def main_():
    argv = sys.argv[1:]
    main, ceil, roof, info, interior, door = build_scene()
    if "--previews-only" not in argv:
        tris, size, tex = export_all(main, ceil, roof, interior, door)
        checks = run_checks(main, ceil, roof, interior, door)
        mf = manifest(tris, size, tex, interior, info, checks)
        with open(os.path.join(C.OUT, "manifest_hut.json"), "w") as f:
            json.dump(mf, f, indent=1, ensure_ascii=False)
        print("TRIS", json.dumps(tris), "SIZE", json.dumps(size), flush=True)
        c2 = dict(checks)
        c2["passage_box"] = {k: v for k, v in checks["passage_box"].items() if k != "path"}
        print("CHECKS", json.dumps(c2), flush=True)
    if "--previews" in argv or "--previews-only" in argv:
        import hut_preview_run
        hut_preview_run.run(main, ceil, roof, interior, door, PREVIEW_DIR)


if __name__ == "__main__":
    main_()
