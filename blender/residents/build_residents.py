"""Builds the residents' models: blender/residents/cache/looks.json -> one skinned glb per townsperson.

    Blender -b -P blender/residents/build_residents.py -- [--only key,key] [--limit N] [--shard I/N] [--force]
    (or python3 with pip bpy; docs/residents.md has the whole pipeline)

Needs: the MPFB2 extension with the assets (tools/residents/mhdata_assets.py), the wardrobe scan (wardrobe_scan.py)
and the atlases (atlases.py: layout.json). Writes blender/residents/export/<Model>.glb + <Model>.json (what was built,
triangles, height; local only, like the atlases: unreal/scripts/import_residents.py takes them into the game).

One person:
1. MakeHuman body from the look's macros and modelling targets (looks.py), no taller than MAX_BODY_M;
2. the game rig (56 bones, SK_Human_Skeleton: blender/humans/mpfb_pipeline.add_ue_rig);
3. eyes, eyebrows, eyelashes, teeth, tongue, hair, garments, shoes fitted by MPFB, accessories by accessories.py;
4. every shape key folded into the mesh, the body under the clothes and the helper geometry deleted;
5. the budget: the body away from the face and the heavy garments decimated to BUDGET triangles (kids KID_BUDGET),
   teeth always (nobody looks at 7000 triangles of teeth);
6. six face morphs for ULyudBodyAnimInstance (eyeBlinkLeft/Right, jawOpen, mouthFunnel, mouthSmile, browUp) from
   MakeHuman's expression units in the person's ancestry mix, carried over to brows, lashes, teeth, tongue, beards;
7. UVs into the atlas tiles and the tint in UV channels 1-2 (TintRG, TintBA: r, g | b, amount; linear colour);
8. all joined into one mesh with four material sections Skin, Cloth, Hair, Eyes and exported as glb (no images).
"""
import argparse
import json
import math
import os
import sys
import time

import bpy
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "blender", "humans"))
import wardrobe as W  # noqa: E402

LOOKS = os.path.join(HERE, "cache", "looks.json")
SCAN = os.path.join(HERE, "cache", "wardrobe")
ATLASES = os.path.join(HERE, "atlases")
EXPORT = os.path.join(HERE, "export")
BUILD_VERSION = 2          # bump when the build itself changes (forces every model to rebuild)

MAX_BODY_M = 1.88          # CharacterStandard.h MaxNpcHeight 195 cm with hair, shoes and headroom
BUDGET = 24000             # docs/characters.md: 24k triangles for an adult (18k for a hi3d child; MakeHuman children
KID_BUDGET = 24000         # have the adults' topology, the face alone is ~9k)
SECTIONS = ("Skin", "Cloth", "Hair", "Eyes")
LASH_RGB = (0.012, 0.010, 0.009)

# face morph name -> [(expression unit, weight)]  (data/targets/expression/units/<race>/<unit>.target.gz)
FACE = {
    "eyeBlinkLeft": [("eye-left-closure", 1.0)],
    "eyeBlinkRight": [("eye-right-closure", 1.0)],
    "jawOpen": [("mouth-open", 1.0)],
    "mouthFunnel": [("mouth-protusion", 0.8), ("mouth-pursing", 0.4)],
    "mouthSmile": [("mouth-corner-puller", 1.0)],
    "browUp": [("eyebrows-left-up", 1.0), ("eyebrows-right-up", 1.0)],
}
MORPH_TO = ("eyebrows", "eyelashes", "teeth", "tongue", "facial_hair")
HEAD_BONES = ("head", "neck_01", "jaw", "eye_l", "eye_r")
CAPS = {"suit": 5500, "dress": 5000, "outer": 4500, "top": 3500, "bottom": 3500, "shoes": 1600, "hat": 1500,
        "glasses": 1200, "hair": 4000}   # triangles per part before the body is touched


_P = None


def P():
    """blender/humans/mpfb_pipeline, with its rig files written once per process into a folder of its own (it
    rewrites them on every add_ue_rig: parallel builds read each other's half-written JSON)."""
    global _P
    if _P is None:
        import mpfb_pipeline
        mpfb_pipeline.CACHE = os.path.join(HERE, "cache", "rig_%d" % os.getpid())
        files = mpfb_pipeline._ue_rig_files()
        mpfb_pipeline._ue_rig_files = lambda: files
        _P = mpfb_pipeline
    return _P


def srgb_to_linear(c):
    c = float(c)
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def lin(rgb):
    return tuple(srgb_to_linear(c) for c in rgb[:3])


# ------------------------------------------------------------------------------------------------ scene

def ctx_override(active, selected):
    return bpy.context.temp_override(active_object=active, object=active, selected_objects=selected,
                                     selected_editable_objects=selected)


def clear_scene(keep_materials):
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    for coll in (bpy.data.meshes, bpy.data.armatures, bpy.data.materials, bpy.data.images, bpy.data.node_groups,
                 bpy.data.shape_keys if hasattr(bpy.data, "shape_keys") else []):
        for d in list(coll):
            if d.users == 0 and d not in keep_materials:
                try:
                    coll.remove(d)
                except Exception:
                    pass


def section_materials():
    mats = {}
    for s in SECTIONS:
        m = bpy.data.materials.get(s) or bpy.data.materials.new(s)
        m.use_fake_user = True
        mats[s] = m
    return mats


# ------------------------------------------------------------------------------------------------ data

class Data:
    def __init__(self, atlases=ATLASES):
        with open(os.path.join(atlases, "layout.json"), encoding="utf-8") as f:
            self.layout = json.load(f)
        self.tile = {}
        for s, sec in self.layout["sections"].items():
            for key, rect in sec["tiles"].items():
                self.tile[key] = (s, rect)
        self.scan = {}
        for fn in os.listdir(SCAN):
            if fn.endswith(".json"):
                with open(os.path.join(SCAN, fn), encoding="utf-8") as f:
                    d = json.load(f)
                self.scan["%s/%s" % (d["subdir"], d["name"])] = d
        self.units = {}            # (unit, race) -> deltas (N, 3) in basemesh space

    def unit_deltas(self, bm, unit, race):
        key = (unit, race)
        if key not in self.units:
            s = P().services()
            path = os.path.join(s["LocationService"].get_mpfb_data("targets"), "expression", "units", race, unit + ".target.gz")
            sk = bm.data.shape_keys
            basis = sk.key_blocks[0]
            n = len(basis.data)
            b = np.empty(n * 3)
            basis.data.foreach_get("co", b)
            s["TargetService"].load_target(bm, path, weight=0.0, name="x_" + unit + "_" + race)
            k = bm.data.shape_keys.key_blocks["x_" + unit + "_" + race]
            c = np.empty(n * 3)
            k.data.foreach_get("co", c)
            self.units[key] = (c - b).reshape(-1, 3)
            bm.shape_key_remove(k)
        return self.units[key]


# ----------------------------------------------------------------------------------------------- body

def make_body(look, name):
    s = P().services()
    macro = dict(s["TargetService"].get_default_macro_info_dict())
    macro.update({k: v for k, v in look["macro"].items() if k != "race"})
    macro["race"] = dict(look["macro"]["race"])
    troot = s["LocationService"].get_mpfb_data("targets")
    for attempt in range(4):
        bm = s["HumanService"].create_human(macro_detail_dict=macro)
        bm.name = name
        for rel, w in sorted(look["targets"].items()):
            s["TargetService"].load_target(bm, os.path.join(troot, rel + ".target.gz"), weight=float(w))
        h = body_height(bm)
        if h <= MAX_BODY_M or attempt == 3:
            return bm, h
        macro["height"] = max(0.0, macro["height"] - 0.1 * (1 + attempt))
        mesh = bm.data
        bpy.data.objects.remove(bm, do_unlink=True)
        bpy.data.meshes.remove(mesh)
    return bm, h


def body_height(bm):
    dg = bpy.context.evaluated_depsgraph_get()
    ev = bm.evaluated_get(dg)
    me = ev.to_mesh()
    co = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get("co", co)
    ev.to_mesh_clear()
    z = co.reshape(-1, 3)[:, 2]
    return float(z.max() - z.min()) * bm.matrix_world.to_scale()[2]


def mixed_coords(obj):
    """Vertex positions with every shape key mixed in (what the eye sees), shape (N, 3)."""
    me = obj.data
    n = len(me.vertices)
    co = np.empty(n * 3)
    if not me.shape_keys:
        me.vertices.foreach_get("co", co)
        return co.reshape(-1, 3)
    blocks = me.shape_keys.key_blocks
    blocks[0].data.foreach_get("co", co)
    base = co.copy()
    tot = base.copy()
    c = np.empty(n * 3)
    for k in blocks[1:]:
        if k.value == 0.0 or k.mute:
            continue
        k.data.foreach_get("co", c)
        tot += (c - base) * k.value
    return tot.reshape(-1, 3)


def fold_keys(obj):
    co = mixed_coords(obj)
    if obj.data.shape_keys:
        obj.shape_key_clear()
    obj.data.vertices.foreach_set("co", co.reshape(-1))
    obj.data.update()


def apply_modifiers(obj, kinds):
    for m in list(obj.modifiers):
        if m.type in kinds:
            with ctx_override(obj, [obj]):
                bpy.ops.object.modifier_apply(modifier=m.name)


# --------------------------------------------------------------------------------------------- budget

def tris(obj):
    return sum(len(p.vertices) - 2 for p in obj.data.polygons)


def protected_faces(obj, bones):
    gi = {g.index: g.name for g in obj.vertex_groups}
    prot = np.zeros(len(obj.data.vertices), bool)
    for v in obj.data.vertices:
        for g in v.groups:
            if gi.get(g.group) in bones and g.weight > 0.05:
                prot[v.index] = True
                break
    n = 0
    for p in obj.data.polygons:
        if any(prot[i] for i in p.vertices):
            n += len(p.vertices) - 2
    return n


def decimate(obj, ratio, protect=None, soft=()):
    """Collapse-decimates obj to ratio of its faces; vertices in the bone groups `protect` are kept (as far as the
    collapse lets them: their edges cost twenty times more), those in `soft` partly."""
    if ratio >= 0.98:
        return
    vg = None
    if protect:
        weights = np.ones(len(obj.data.vertices))
        gi = {g.index: g.name for g in obj.vertex_groups}
        for v in obj.data.vertices:
            for g in v.groups:
                name = gi.get(g.group)
                if name in protect and g.weight > 0.05:
                    weights[v.index] = 0.0
                elif name in soft and g.weight > 0.3:
                    weights[v.index] = min(weights[v.index], 0.6)
        vg = obj.vertex_groups.new(name="_decimate")
        for i, w in enumerate(weights):
            vg.add([i], float(w), "REPLACE")
    mod = obj.modifiers.new("_dec", "DECIMATE")
    mod.decimate_type = "COLLAPSE"
    mod.ratio = ratio
    mod.use_collapse_triangulate = False
    if vg is not None:
        mod.vertex_group = vg.name
        mod.vertex_group_factor = 10.0
    # the decimate must run before the armature
    while obj.modifiers.find(mod.name) > 0:
        with ctx_override(obj, [obj]):
            bpy.ops.object.modifier_move_up(modifier=mod.name)
    with ctx_override(obj, [obj]):
        bpy.ops.object.modifier_apply(modifier=mod.name)
    g = obj.vertex_groups.get("_decimate")
    if g is not None:
        obj.vertex_groups.remove(g)


# ---------------------------------------------------------------------------------------------- morphs

def body_morphs(data, bm, look):
    """{morph: deltas (N, 3)} for the basemesh as built (before any vertex is deleted)."""
    race = look["macro"]["race"]
    out = {}
    for morph, units in FACE.items():
        d = None
        for unit, w in units:
            for r, rw in race.items():
                if rw <= 1e-3:
                    continue
                part = data.unit_deltas(bm, unit, r) * (w * rw)
                d = part if d is None else d + part
        out[morph] = d
    return out


def world(obj, co):
    m = np.array(obj.matrix_world)
    return co @ m[:3, :3].T + m[:3, 3]


def transfer_morphs(src_co, src_morphs, obj, k=4, exact=False):
    """Gives obj the morphs of the nearest source vertices (inverse distance weights; world space)."""
    from mathutils import kdtree
    tree = kdtree.KDTree(len(src_co))
    for i, c in enumerate(src_co):
        tree.insert(c, i)
    tree.balance()
    n = len(obj.data.vertices)
    co = np.empty(n * 3)
    obj.data.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    wco = world(obj, co)
    idx = np.zeros((n, k), int)
    wts = np.zeros((n, k))
    for i, c in enumerate(wco):
        found = tree.find_n(c, k)
        if exact and found and found[0][2] < 1e-5:
            idx[i, 0], wts[i, 0] = found[0][1], 1.0
            continue
        for j, (_, fi, dist) in enumerate(found):
            idx[i, j] = fi
            wts[i, j] = 1.0 / (dist + 1e-4) ** 2
    wts /= np.maximum(wts.sum(1, keepdims=True), 1e-12)
    inv = np.linalg.inv(np.array(obj.matrix_world)[:3, :3])
    if obj.data.shape_keys is None:
        obj.shape_key_add(name="Basis", from_mix=False)
    for name, d in src_morphs.items():
        dd = (d[idx] * wts[..., None]).sum(1) @ inv.T          # world delta -> object space
        if not np.any(np.abs(dd) > 1e-6):
            continue
        key = obj.shape_key_add(name=name, from_mix=False)
        key.data.foreach_set("co", (co + dd).reshape(-1))
        key.value = 0.0


def jaw_rigid(src_co, src_morphs, obj, lower_only=True, share=1.0):
    """Teeth and tongue only ride the jaw: jawOpen moves each lower piece (connected part below the bite) by the
    mean of what the nearest skin does; the upper teeth stay. (Per vertex the tips took the lower lip along and
    turned the teeth into fangs.)"""
    import bmesh
    if "jawOpen" not in src_morphs:
        return
    tmp = {"jawOpen": src_morphs["jawOpen"]}
    transfer_morphs(src_co, tmp, obj, k=6)
    keys = obj.data.shape_keys
    if keys is None or "jawOpen" not in keys.key_blocks:
        return
    n = len(obj.data.vertices)
    base = np.empty(n * 3)
    keys.key_blocks[0].data.foreach_get("co", base)
    base = base.reshape(-1, 3)
    jaw = np.empty(n * 3)
    keys.key_blocks["jawOpen"].data.foreach_get("co", jaw)
    d = jaw.reshape(-1, 3) - base
    b = bmesh.new()
    b.from_mesh(obj.data)
    b.verts.ensure_lookup_table()
    comp = np.full(n, -1)
    c = 0
    for v in b.verts:
        if comp[v.index] >= 0:
            continue
        stack = [v]
        comp[v.index] = c
        while stack:
            cur = stack.pop()
            for e in cur.link_edges:
                o = e.other_vert(cur)
                if comp[o.index] < 0:
                    comp[o.index] = c
                    stack.append(o)
        c += 1
    b.free()
    mid = np.median(base[:, 2])
    out = np.zeros_like(d)
    for i in range(c):
        m = comp == i
        if lower_only and base[m, 2].mean() > mid:
            continue
        out[m] = d[m].mean(0) * share
    keys.key_blocks["jawOpen"].data.foreach_set("co", (base + out).reshape(-1))


# ------------------------------------------------------------------------------------------- uvs/tints

def set_uvs(obj, data, tile_key, face_tints):
    """UVs into the atlas tile; TintRG/TintBA from face_tints: per face (r, g, b, amount)."""
    section, rect = data.tile[tile_key]
    me = obj.data
    uvl = me.uv_layers.active or me.uv_layers.new(name="UVMap")
    uvl.name = "UVMap"
    n = len(me.loops)
    uv = np.empty(n * 2)
    uvl.data.foreach_get("uv", uv)
    uv = np.clip(uv.reshape(-1, 2), 0.0, 1.0)
    u0, v0, u1, v1 = rect
    uv[:, 0] = u0 + uv[:, 0] * (u1 - u0)
    uv[:, 1] = v0 + uv[:, 1] * (v1 - v0)
    uvl.data.foreach_set("uv", uv.reshape(-1))
    # per-loop tint from per-face tint
    loop_face = np.empty(n, int)
    for p in me.polygons:
        loop_face[p.loop_start:p.loop_start + p.loop_total] = p.index
    ft = np.asarray(face_tints, dtype=np.float64).reshape(-1, 4)
    lt = ft[loop_face]
    for name, cols in (("TintRG", (0, 1)), ("TintBA", (2, 3))):
        lay = me.uv_layers.get(name) or me.uv_layers.new(name=name)
        lay.data.foreach_set("uv", lt[:, cols].reshape(-1))
    # drop any other UV layer (keep the three in order)
    for lay in list(me.uv_layers):
        if lay.name not in ("UVMap", "TintRG", "TintBA"):
            me.uv_layers.remove(lay)
    return section


def assign(obj, mats, section):
    me = obj.data
    me.materials.clear()
    me.materials.append(mats[section])
    me.polygons.foreach_set("material_index", [0] * len(me.polygons))


def uniform(n, rgb, amount):
    return [tuple(rgb) + (amount,)] * n


# ----------------------------------------------------------------------------------------------- build

def add(bm, folder, subdir, atype):
    obj = P().add_asset(bm, folder, subdir, atype)
    obj["res_kind"] = subdir
    obj["res_name"] = folder
    return obj


def build_one(look, data, mats, out_dir):
    t0 = time.time()
    name = look["model"]
    clear_scene(set(mats.values()))
    bm, height = make_body(look, name)
    morph_src = body_morphs(data, bm, look)
    arm = P().add_ue_rig(bm)
    arm.name = arm.data.name = "Armature_" + name

    parts = []          # (obj, kind)
    eyes = add(bm, "low-poly", "eyes", "Eyes")
    parts.append((eyes, "eyes"))
    parts.append((add(bm, look["eyebrows"], "eyebrows", "Eyebrows"), "eyebrows"))
    parts.append((add(bm, look["eyelashes"], "eyelashes", "Eyelashes"), "eyelashes"))
    parts.append((add(bm, "teeth_base", "teeth", "Teeth"), "teeth"))
    parts.append((add(bm, "tongue01", "tongue", "Tongue"), "tongue"))
    if look.get("hair"):
        parts.append((add(bm, look["hair"], "hair", "Hair"), "hair"))
    for g in look["clothes"]:
        parts.append((add(bm, g, "clothes", "Clothes"), "clothes"))
    parts += add_accessories(look, bm, arm)

    # the body as the face morphs see it: positions (world) of every basemesh vertex, before deletions
    full_co = world(bm, mixed_coords(bm))
    in_body = body_group_mask(bm)
    src_idx = np.nonzero(in_body)[0]
    src_co = full_co[src_idx]
    src_morphs = {m: world_delta(bm, d)[src_idx] for m, d in morph_src.items()}

    # fold everything, delete the hidden body and the helpers
    for o in [bm] + [p for p, _ in parts]:
        fold_keys(o)
    oidx = bm.data.attributes.new("oidx", "INT", "POINT")
    oidx.data.foreach_set("value", np.arange(len(bm.data.vertices)))
    apply_modifiers(bm, {"MASK"})
    if len(bm.data.vertices) and not hasattr(bm.data.attributes.get("oidx"), "data"):
        raise RuntimeError("lost the vertex index attribute")
    remove_loose(bm)

    # UVs, tints, materials (before the budget: the decimation carries UVs and tints along, island by island)
    skin_t = float(look["skin_tint"]) * 0.5
    sec = set_uvs(bm, data, "skins/" + look["skin"], uniform(len(bm.data.polygons), (skin_t, skin_t, skin_t), 1.0))
    assign(bm, mats, sec)
    for p, k in parts:
        me = p.data
        nf = len(me.polygons)
        if k == "eyes":
            sec = set_uvs(p, data, "eyes/" + look["iris"], uniform(nf, (0.5, 0.5, 0.5), 0.0))
        elif k in ("teeth", "tongue"):
            sec = set_uvs(p, data, "%s/%s" % (k, p["res_name"]), uniform(nf, (0.5, 0.5, 0.5), 0.0))
        elif k == "eyebrows":
            sec = set_uvs(p, data, "eyebrows/" + p["res_name"], uniform(nf, look["brow_rgb"], 1.0))
        elif k == "eyelashes":
            sec = set_uvs(p, data, "eyelashes/" + p["res_name"], uniform(nf, LASH_RGB, 1.0))
        elif k == "hair":
            sec = set_uvs(p, data, "hair/" + p["res_name"], uniform(nf, look["hair_rgb"], 1.0))
        elif k == "clothes":
            sec = set_uvs(p, data, "clothes/" + p["res_name"], garment_tints(p, look, data))
        else:
            sec = accessory_uvs(p, look, data)
        assign(p, mats, sec)

    # garments under garments (after the tints: the slot scan counts the faces of the whole garment)
    hide_under_layers(parts)

    # budget: teeth always, then caps per part, then the body away from the face and hands
    budget = KID_BUDGET if look["age"] < 13 else BUDGET
    for p, k in parts:
        if k == "teeth":
            decimate(p, 0.12)
        elif k == "clothes":
            kind = W.GARMENTS.get(p["res_name"], {}).get("kind")
            cap = CAPS.get(kind, 5000)
            if tris(p) > cap:
                decimate(p, cap / tris(p))
        elif k == "hair" and tris(p) > CAPS["hair"]:
            decimate(p, CAPS["hair"] / tris(p))
    total = tris(bm) + sum(tris(p) for p, _ in parts)
    over = total - budget
    if over > 200:
        # the body: never below its head plus 55 % of the rest (thin arms and fingers collapse to nothing below that)
        head_f = protected_faces(bm, HEAD_BONES)
        floor = head_f + 0.55 * (tris(bm) - head_f)
        cut = min(over, tris(bm) - floor)
        if cut > 200:
            before = tris(bm)
            decimate(bm, (before - cut) / before, protect=HEAD_BONES, soft=("hand_l", "hand_r"))
            over -= before - tris(bm)
    if over > 200:
        # then clothes and hair, each by its share, never below 55 % of what it has
        cl = [p for p, k in parts if k in ("clothes", "hair") and tris(p) > 400]
        tot_cl = sum(tris(p) for p in cl) or 1
        for p in cl:
            t = tris(p)
            target = max(0.55 * t, t - over * t / tot_cl)
            decimate(p, target / t)
    if os.environ.get("RES_DEBUG"):
        print("budget %s: %d -> %d (budget %d)" % (name, total, tris(bm) + sum(tris(p) for p, _ in parts), budget))

    # face morphs: the body from its own (kept) vertices, the rest from the nearest body vertices
    bco = np.empty(len(bm.data.vertices) * 3)
    bm.data.vertices.foreach_get("co", bco)
    transfer_morphs(src_co, src_morphs, bm, k=4, exact=True)
    for p, k in parts:
        kind = k if k != "accessory" else ("facial_hair" if p.get("tint_slot") == "facial_hair" else "accessory")
        if kind in ("teeth", "tongue"):
            jaw_rigid(src_co, src_morphs, p, lower_only=(kind == "teeth"), share=1.0 if kind == "teeth" else 0.8)
        elif kind in MORPH_TO:
            transfer_morphs(src_co, src_morphs, p, k={"eyelashes": 1, "eyebrows": 2}.get(kind, 4))

    # one mesh
    mesh = join([bm] + [p for p, _ in parts], bm)
    clean_groups(mesh, arm)
    mesh.name = mesh.data.name = "SK_" + name
    for poly in mesh.data.polygons:
        poly.use_smooth = True
    stats = dict(key=look["key"], model=name, hash=look["hash"], build=BUILD_VERSION, height_m=round(height, 3),
                 tris=tris(mesh), verts=len(mesh.data.vertices),
                 sections={s: 0 for s in SECTIONS},
                 morphs=[k.name for k in mesh.data.shape_keys.key_blocks[1:]] if mesh.data.shape_keys else [])
    for p in mesh.data.polygons:
        stats["sections"][mesh.data.materials[p.material_index].name] += len(p.vertices) - 2
    path = os.path.join(out_dir, name + ".glb")
    export(arm, mesh, path)
    stats["seconds"] = round(time.time() - t0, 2)
    with open(os.path.join(out_dir, name + ".json"), "w", encoding="utf-8") as f:
        json.dump(stats, f, indent=1)
    return stats


LAYER = {"bottom": 1, "suit": 1, "dress": 1, "top": 2, "outer": 3}


def hide_under_layers(parts):
    """Garments under other garments: faces wholly covered go, covered vertices left over sink 4 mm under the cover
    (a coat over a shirt showed the shirt's stripes through every fold)."""
    from mathutils import Vector
    from mathutils.bvhtree import BVHTree
    cl = [(p, LAYER.get(W.GARMENTS.get(p["res_name"], {}).get("kind"), 0)) for p, k in parts if k == "clothes"]
    cl = [(p, l) for p, l in cl if l > 0]
    for inner, li in cl:
        outers = [p for p, l in cl if l > li]
        if not outers:
            continue
        trees = []
        for o in outers:
            n = len(o.data.vertices)
            co = np.empty(n * 3)
            o.data.vertices.foreach_get("co", co)
            co = world(o, co.reshape(-1, 3))
            trees.append(BVHTree.FromPolygons(co.tolist(), [tuple(pp.vertices) for pp in o.data.polygons]))
        me = inner.data
        n = len(me.vertices)
        co = np.empty(n * 3)
        me.vertices.foreach_get("co", co)
        co = co.reshape(-1, 3)
        nr = np.empty(n * 3)
        me.vertices.foreach_get("normal", nr)
        mw = np.array(inner.matrix_world)
        wco = world(inner, co)
        wn = nr.reshape(-1, 3) @ mw[:3, :3].T
        wn /= np.maximum(np.linalg.norm(wn, axis=1, keepdims=True), 1e-9)
        covered = np.zeros(n, bool)
        new = wco.copy()
        for i in range(n):
            c, d = Vector(wco[i]), Vector(wn[i])
            for t in trees:
                hit = t.ray_cast(c, d, 0.07)
                if hit[0] is not None:
                    covered[i] = True
                    if hit[3] < 0.004:
                        new[i] = wco[i] - wn[i] * (0.004 - hit[3])
                    break
                back = t.ray_cast(c, -d, 0.02)
                if back[0] is not None:                       # the cover is under this vertex: it pokes through
                    covered[i] = True
                    new[i] = np.array(back[0]) - wn[i] * 0.004
                    break
        if not covered.any():
            continue
        inv = np.linalg.inv(mw)
        loc = new @ inv[:3, :3].T + inv[:3, 3]
        me.vertices.foreach_set("co", loc.reshape(-1))
        import bmesh
        b = bmesh.new()
        b.from_mesh(me)
        b.verts.ensure_lookup_table()
        gone = [f for f in b.faces if all(covered[v.index] for v in f.verts)]
        if gone:
            bmesh.ops.delete(b, geom=gone, context="FACES")
        loose = [v for v in b.verts if not v.link_faces]
        if loose:
            bmesh.ops.delete(b, geom=loose, context="VERTS")
        b.to_mesh(me)
        b.free()
        me.update()
        if os.environ.get("RES_DEBUG"):
            print("layers: %s under %s: %d faces hidden" % (inner["res_name"], [o["res_name"] for o in outers], len(gone)))


def body_group_mask(bm):
    """True for the basemesh's own vertices (not the helper geometry)."""
    n = len(bm.data.vertices)
    vg = bm.vertex_groups.get("body")
    if vg is None:
        return np.ones(n, bool)
    m = np.zeros(n, bool)
    gi = vg.index
    for v in bm.data.vertices:
        for g in v.groups:
            if g.group == gi and g.weight > 0.5:
                m[v.index] = True
                break
    return m


def world_delta(obj, d):
    m = np.array(obj.matrix_world)[:3, :3]
    return d @ m.T


def remove_loose(obj):
    import bmesh
    b = bmesh.new()
    b.from_mesh(obj.data)
    loose = [v for v in b.verts if not v.link_faces]
    if loose:
        bmesh.ops.delete(b, geom=loose, context="VERTS")
        b.to_mesh(obj.data)
    b.free()


def garment_tints(obj, look, data):
    g = obj["res_name"]
    scan = data.scan.get("clothes/" + g, {})
    slots = scan.get("slots")
    nf = len(obj.data.polygons)
    if not slots or len(slots) != nf:
        print("build: %s: slot scan missing or stale (%s faces, scan %s): one tint" % (g, nf, len(slots or [])))
        slots = [W.GARMENTS[g]["slots"][-1][1]] * nf
    cache = {}
    out = []
    for s in slots:
        if s not in cache:
            rgb = look["tints"].get("%s:%s" % (g, s))
            cache[s] = (lin(rgb) + (1.0,)) if rgb and not s.startswith("keep") else (0.5, 0.5, 0.5, 0.0)
        out.append(cache[s])
    return out


# ---------------------------------------------------------------------------------------------- accessories

def add_accessories(look, bm, arm):
    """[(object, part kind)] for the look's accessories: accessories.py builds them fitted over the clothes (one
    fit of the body for all); the MakeHuman glasses (glasses:punkduck) are an MPFB asset."""
    acc = [a for a in look.get("accessories", []) if a != "mindfront_fishing_hat_01"]
    out = []
    if "glasses:punkduck" in acc:
        acc.remove("glasses:punkduck")
        out.append((add(bm, "punkduck_glasses", "clothes", "Clothes"), "clothes"))
    if not acc:
        return out
    try:
        import accessories as A
    except Exception as e:                                  # noqa: BLE001
        print("build: no accessories.py (%s)" % e)
        return out
    if hasattr(A, "_img_cache"):
        A._img_cache.clear()                               # its images went with the last person's scene
    hair_vol = W.HAIR.get(look.get("hair") or "", {}).get("volume", 0.0)
    ctx = dict(basemesh=bm, armature=arm, hair_volume=hair_vol, sex=look["sex"], age=look["age"])
    seed = int(look["hash"][:6], 16)
    kinds, params = [], {}
    hands = ["r", "l"]                                     # a cane and a bag: one in each hand
    for a in acc:
        kind, _, style = a.partition(":")
        if kind not in getattr(A, "KINDS", []):
            print("build: unknown accessory %s" % a)
            continue
        p = {"seed": seed}
        if style:
            p["style"] = style
        t = look.get("accessory_tints", {}).get(a)
        if t:
            p["tint"] = tuple(t)
        if kind in ("cane", "string_bag", "shopping_bag"):
            if not hands:
                continue                                   # two hands full already
            p["hand"] = hands.pop(0)
        kinds.append(kind)
        params[kind] = p
    try:
        objs = A.add_many(kinds, ctx, params)
    except Exception:                                       # noqa: BLE001
        import traceback
        traceback.print_exc()
        print("build: accessories failed for %s: %s" % (look["key"], kinds))
        return out
    for kind, o in zip(kinds, objs):
        o["res_kind"] = "accessory"
        o["res_name"] = next(a for a in acc if a.partition(":")[0] == kind)
        out.append((o, "accessory"))
    return out


def accessory_uvs(obj, look, data):
    if obj.get("res_kind") == "clothes":                   # an MPFB asset among the accessories (glasses)
        return set_uvs(obj, data, "clothes/" + obj["res_name"], uniform(len(obj.data.polygons), (0.5, 0.5, 0.5), 0.0))
    nf = len(obj.data.polygons)
    tile = "acc/" + obj["tile"]
    a = obj["res_name"]
    if obj.get("tint_slot") == "facial_hair":
        rgb = look["brow_rgb"]
        f = [tuple(rgb) + (1.0,)] * nf
    elif obj.get("tintable"):
        t = look.get("accessory_tints", {}).get(a) or list(obj.get("tint_rgb", (0.5, 0.5, 0.5)))
        f = [lin(t) + (1.0,)] * nf
    else:
        f = uniform(nf, (0.5, 0.5, 0.5), 0.0)
    return set_uvs(obj, data, tile, f)


# ------------------------------------------------------------------------------------------------ join

def join(objs, active):
    for o in objs:
        o.hide_set(False) if hasattr(o, "hide_set") else None
    with ctx_override(active, objs):
        bpy.ops.object.join()
    return active


def clean_groups(mesh, arm):
    bones = {b.name for b in arm.data.bones}
    for vg in list(mesh.vertex_groups):
        if vg.name not in bones:
            mesh.vertex_groups.remove(vg)
    for a in list(mesh.data.attributes):
        if a.name == "oidx":
            mesh.data.attributes.remove(a)
    # one armature modifier (compare by name: bpy hands out a new wrapper on every access)
    keep = next((m.name for m in mesh.modifiers if m.type == "ARMATURE"), None)
    for name in [m.name for m in mesh.modifiers if m.name != keep]:
        mesh.modifiers.remove(mesh.modifiers[name])
    if keep is None:
        mod = mesh.modifiers.new("Armature", "ARMATURE")
        keep = mod.name
    mesh.modifiers[keep].object = arm
    if len(mesh.vertex_groups) < 50:
        raise RuntimeError("%s has only %d bone groups left" % (mesh.name, len(mesh.vertex_groups)))


def export(arm, mesh, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    sel = [arm, mesh]
    for o in bpy.data.objects:
        o.select_set(o in sel)
    bpy.context.view_layer.objects.active = arm
    kw = dict(filepath=path, use_selection=True, export_format="GLB", export_skins=True, export_morph=True,
              export_morph_normal=False, export_animations=False, export_def_bones=False, export_yup=True,
              export_apply=False, export_texcoords=True, export_normals=True, export_tangents=False,
              export_materials="EXPORT", export_image_format="NONE", export_vertex_color="NONE")
    try:
        bpy.ops.export_scene.gltf(**kw, export_try_sparse_sk=True)
    except TypeError:
        bpy.ops.export_scene.gltf(**kw)


# ------------------------------------------------------------------------------------------------- main

def enable_mpfb(shard):
    if shard:
        time.sleep(3.0 * shard)        # MPFB writes its settings on enable: don't let processes collide
    try:
        bpy.ops.preferences.addon_enable(module="bl_ext.user_default.mpfb")
    except Exception as e:                                  # noqa: BLE001
        print("build: enabling MPFB:", e)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--looks", default=LOOKS)
    ap.add_argument("--atlases", default=ATLASES)
    ap.add_argument("--out", default=EXPORT)
    ap.add_argument("--only", default="")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--shard", default="0/1")
    ap.add_argument("--force", action="store_true")
    a = ap.parse_args(argv)
    shard, nshards = (int(x) for x in a.shard.split("/"))
    with open(a.looks, encoding="utf-8") as f:
        looks = json.load(f)["looks"]
    keys = sorted(looks)
    if a.only:
        want = set(a.only.split(","))
        keys = [k for k in keys if k in want or looks[k]["model"] in want]
    keys = [k for i, k in enumerate(keys) if i % nshards == shard]
    if a.limit:
        keys = keys[:a.limit]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    enable_mpfb(shard)
    data = Data(a.atlases)
    mats = section_materials()
    os.makedirs(a.out, exist_ok=True)
    built = skipped = failed = 0
    t0 = time.time()
    for k in keys:
        lk = looks[k]
        side = os.path.join(a.out, lk["model"] + ".json")
        if not a.force and os.path.isfile(side) and os.path.isfile(os.path.join(a.out, lk["model"] + ".glb")):
            try:
                with open(side, encoding="utf-8") as f:
                    old = json.load(f)
                if old.get("hash") == lk["hash"] and old.get("build") == BUILD_VERSION:
                    skipped += 1
                    continue
            except Exception:                               # noqa: BLE001
                pass
        try:
            st = build_one(lk, data, mats, a.out)
            built += 1
            print("built %-40s %6d tris %s %.1fs" % (st["model"], st["tris"], st["sections"], st["seconds"]), flush=True)
        except Exception:                                   # noqa: BLE001
            import traceback
            traceback.print_exc()
            failed += 1
            print("FAILED", k, flush=True)
    print("build_residents shard %d/%d: %d built, %d up to date, %d failed in %.0f s" %
          (shard, nshards, built, skipped, failed, time.time() - t0))
    import shutil
    shutil.rmtree(os.path.join(HERE, "cache", "rig_%d" % os.getpid()), ignore_errors=True)


if __name__ == "__main__":
    main()
