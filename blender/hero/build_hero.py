"""The hero for walking around in first person: the realistic Zoomer body (MakeHuman, humans thread)
dressed in the clothes of the first-person body of Dima's Murmuration (starlings) game: an open,
oversized camel corduroy overshirt with rolled sleeves over a sage hoodie, baggy black cargo trousers,
chunky off-white sneakers.

  in : $HUMANS_DIR/zoomer.blend          (blender/humans of the humans worktree, built by build_humans.py)
       $MURMURATION_FP                    (Murmuration's public/models/fp_body.glb)
  out: blender/hero/export/human_hero.glb (local only, LFS quota), renders/hero/*.png with --preview

The garments are moved from Murmuration's rig onto the Zoomer rest pose (A-pose) by per-bone
transforms (bone head to bone head, swing onto the target direction, stretch along the bone) blended
with the garments' own skin weights, pushed outside the body and each other, and reskinned from the
body. Body skin under the clothes is cut away so it can never poke through. The shoes have thick
soles: the soles go below the floor by SOLE_DROP, the pawn lifts the mesh by that much.

Run from the repo root:
    /Applications/Blender.app/Contents/MacOS/Blender -b -P blender/hero/build_hero.py -- [--preview]
"""
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
HUMANS_DIR = os.environ.get("HUMANS_DIR") or next(
    (p for p in (os.path.join(ROOT, "blender", "humans"),
                 os.path.expanduser("~/projects/lyudmilovsk-wt/humans/blender/humans"))
     if os.path.exists(os.path.join(p, "zoomer.blend"))), None)
MURMURATION_FP = os.environ.get("MURMURATION_FP") or os.path.expanduser(
    "~/projects/_murmuration_src/murmuration/public/models/fp_body.glb")
OUT = os.path.join(ROOT, "blender", "hero", "export", "human_hero.glb")
RENDERS = os.path.join(ROOT, "renders", "hero")
ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []

GARMENTS = ["Trousers", "Shoes", "Hoodie", "Overshirt"]          # inner to outer
GAP = {"Trousers": 0.006, "Shoes": 0.004, "Hoodie": 0.01, "Overshirt": 0.01}
OLD_CLOTHES = ("elvs_hooded_sweat_jacket1", "punkduck_male_classic_jeans", "punkduck_tennis_shoes")

# Murmuration bone -> (target head, target tail) on the Zoomer rig, as (bone, 'head'|'tail') pairs.
# Murmuration's bone segment is (its head, the head of the bone named second), so both rigs measure
# the same joints.
MAP = {
    "hips":  (("hips", "spine"), ("pelvis", "spine_01")),
    "spine": (("spine", "chest"), ("spine_01", "spine_03")),
    "chest": (("chest", "neck"), ("spine_03", "neck_01")),
    "neck":  (("neck", "head"), ("neck_01", "head")),
    "head":  (("head", "head:tail"), ("head", "head:tail")),
}
for s, z in ((".L", "_l"), (".R", "_r")):
    MAP.update({
        "shoulder" + s: (("shoulder" + s, "upper_arm" + s), ("clavicle" + z, "upperarm" + z)),
        "upper_arm" + s: (("upper_arm" + s, "forearm" + s), ("upperarm" + z, "lowerarm" + z)),
        "forearm" + s: (("forearm" + s, "hand" + s), ("lowerarm" + z, "hand" + z)),
        "hand" + s: (("hand" + s, "f_middle.01" + s), ("hand" + z, "middle_01" + z)),
        "thigh" + s: (("thigh" + s, "shin" + s), ("thigh" + z, "calf" + z)),
        "shin" + s: (("shin" + s, "foot" + s), ("calf" + z, "foot" + z)),
        "foot" + s: (None, None),               # the shoes: placed under the foot (foot_xform)
    })


def log(*a):
    print("HERO", *a, flush=True)


# ------------------------------------------------------------------------------ scene
def load_zoomer():
    bpy.ops.wm.open_mainfile(filepath=os.path.join(HUMANS_DIR, "zoomer.blend"))
    rig = bpy.data.objects["zoomer.rig"]
    body = bpy.data.objects["zoomer"]
    for o in list(bpy.data.objects):
        if any(o.name.endswith(c) for c in OLD_CLOTHES) or (o.type == "MESH" and not o.data.materials):
            bpy.data.objects.remove(o)
    for m in [m for m in body.modifiers if m.type == "MASK"]:
        body.modifiers.remove(m)
    if rig.animation_data:
        rig.animation_data.action = None
    for a in list(bpy.data.actions):
        bpy.data.actions.remove(a)
    for pb in rig.pose.bones:
        pb.matrix_basis = Matrix()
    sys.path.insert(0, HUMANS_DIR)
    import mpfb_pipeline as mp                      # folds the modelling keys, keeps the face keys
    for c in rig.children:
        if c.type == "MESH":
            mp.bake_body_targets(c)
    # what the masks hid: helper geometry (everything outside "body") and the scalp under the hair
    gi = {g.name: g.index for g in body.vertex_groups}
    hair = [n for n in gi if n.startswith("Delete.") and "hair" in n]
    dead = set()
    for v in body.data.vertices:
        gs = {g.group: g.weight for g in v.groups}
        if gs.get(gi["body"], 0) <= 0 or any(gs.get(gi[h], 0) > 0 for h in hair):
            dead.add(v.index)
    bm = bmesh.new()
    bm.from_mesh(body.data)
    bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[bm.verts[i] for i in dead], context="VERTS")
    bm.to_mesh(body.data)
    bm.free()
    log("body: dropped %d helper/scalp vertices" % len(dead))
    return rig, body


def import_murmuration():
    bpy.context.scene.render.fps = FPS
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=MURMURATION_FP)
    new = [o for o in bpy.data.objects if o not in before]
    mr = next(o for o in new if o.type == "ARMATURE")
    if mr.animation_data:
        mr.animation_data.action = None
    mr.data.pose_position = "REST"
    garments = {}
    for o in new:
        if o.name in GARMENTS:
            garments[o.name] = o
        elif o is not mr:
            bpy.data.objects.remove(o)
    return mr, garments


def rest_points(obj):
    """World vertex positions of a skinned object in its armature's rest pose."""
    dg = bpy.context.evaluated_depsgraph_get()
    ev = obj.evaluated_get(dg)
    me = ev.to_mesh()
    co = np.array([obj.matrix_world @ v.co for v in me.vertices])
    ev.to_mesh_clear()
    return co


# ------------------------------------------------------------------------------ transfer
def joint(arm, spec):
    name, _, end = spec.partition(":")
    b = arm.data.bones[name]
    return arm.matrix_world @ (b.tail_local if end == "tail" else b.head_local)


ROT = Matrix.Rotation(math.pi, 4, "Z")              # Murmuration faces +Y in Blender, Zoomer -Y


def seg_xform(hm, tm, hz, tz):
    dm = (tm - hm).normalized()
    dz = (tz - hz).normalized()
    s = (tz - hz).length / max(1e-6, (tm - hm).length)
    R = dm.rotation_difference(dz).to_matrix().to_4x4()
    S = Matrix.Identity(4)
    for i in range(3):
        for j in range(3):
            S[i][j] += (s - 1) * dm[i] * dm[j]
    return Matrix.Translation(hz) @ R @ S @ Matrix.Translation(-hm)


def bone_xforms(mr, zr):
    xf = {}
    for mb, (mseg, zseg) in MAP.items():
        if mseg is None:
            continue
        hm, tm = (ROT @ joint(mr, mseg[0]), ROT @ joint(mr, mseg[1]))
        hz, tz = joint(zr, zseg[0]), joint(zr, zseg[1])
        xf[mb] = seg_xform(hm, tm, hz, tz)
    for side, z in ((".L", "_l"), (".R", "_r")):
        # the shoe: ankle onto the ankle, scaled to the foot's length, soles flat
        am = ROT @ joint(mr, "foot" + side)
        az = joint(zr, "foot" + z)
        toe_z = joint(zr, "ball" + z + ":tail")
        lm = foot_length_m[side]
        lz = (Vector((toe_z.x, toe_z.y, 0)) - Vector((az.x, az.y, 0))).length
        k = lz / lm
        yaw_z = math.atan2(toe_z.x - az.x, -(toe_z.y - az.y))
        S = Matrix.Diagonal((k, k, 1.0, 1.0))
        xf["foot" + side] = Matrix.Translation(az) @ Matrix.Rotation(-yaw_z, 4, "Z") @ S @ Matrix.Translation(-am)
        log("shoe", side, "scale %.3f" % k, "yaw %.1f" % math.degrees(yaw_z))
    # every other bone follows its nearest mapped ancestor
    for b in mr.data.bones:
        if b.name in xf:
            continue
        p = b.parent
        while p is not None and p.name not in xf:
            p = p.parent
        xf[b.name] = xf[p.name] if p is not None else xf["hips"]
    return xf


foot_length_m = {}


def measure_murm_feet(mr, shoes_co, shoes):
    """Ankle-to-toe length of each Murmuration shoe's foot (the toe tip of the shoe, minus a sole rim)."""
    for side in (".L", ".R"):
        a = ROT @ joint(mr, "foot" + side)
        pts = shoes_co[(shoes_co[:, 0] > 0) == (side == ".L")]
        # the toe: furthest point forward (-Y after the turn)
        toe_y = pts[:, 1].min()
        foot_length_m[side] = abs(a.y - toe_y) - 0.03          # ~3 cm of toe cap in front of the toes


def transfer(obj, co, mr, xf):
    """New positions: the Murmuration skin weights blend the per-bone transforms."""
    names = [g.name for g in obj.vertex_groups]
    W = np.zeros((len(co), len(names)))
    for v in obj.data.vertices:
        for g in v.groups:
            W[v.index, g.group] = g.weight
    W /= np.maximum(W.sum(1, keepdims=True), 1e-9)
    out = np.zeros_like(co)
    homo = np.c_[co, np.ones(len(co))]
    for j, n in enumerate(names):
        if not W[:, j].any():
            continue
        M = np.array(xf.get(n, xf["hips"]))
        out += W[:, j:j + 1] * (homo @ M.T)[:, :3]
    return out


# ------------------------------------------------------------------------------ fitting
def mesh_bvh(co, faces):
    return BVHTree.FromPolygons([Vector(p) for p in co], faces, all_triangles=False)


def push_out(co, faces_self, bvh, gap, iters=120, radius=0.035):
    """Moves cloth that is inside (or closer than gap to) the surface in bvh outward. The push is a
    smooth field over space (Gaussian, radius), not per vertex, so stacked parts of a garment
    (pockets on a panel, a collar on a yoke, the rolled cuff) move together instead of shredding."""
    from mathutils.kdtree import KDTree
    kd = KDTree(len(co))
    for i, p in enumerate(co):
        kd.insert(Vector(p), i)
    kd.balance()
    rows, cols, ws = [], [], []
    for i, p in enumerate(co):
        for _, j, d in kd.find_range(Vector(p), radius):
            rows.append(i)
            cols.append(j)
            ws.append(math.exp(-(d / (0.5 * radius)) ** 2))
    rows, cols, ws = np.array(rows), np.array(cols), np.array(ws)
    wsum = np.zeros(len(co))
    np.add.at(wsum, rows, ws)

    def smooth(d):
        out = np.zeros_like(d)
        np.add.at(out, rows, d[cols] * ws[:, None])
        return out / wsum[:, None]

    def need(pts):
        d = np.zeros_like(pts)
        n = 0
        for i, p in enumerate(pts):
            loc, nrm, _, _ = bvh.find_nearest(Vector(p), 0.25)
            if loc is None:
                continue
            side = (Vector(p) - loc).dot(nrm)
            if side < gap:
                d[i] = np.array(nrm) * (gap - side)
                n += 1
        return d, n

    disp = np.zeros_like(co)
    for it in range(iters):
        d, n = need(co + disp)
        if it % 30 == 0 or n == 0:
            log("  push-out pass %d: %d vertices inside" % (it, n))
        if n == 0:
            break
        sm = smooth(d)
        # the smoothed push never undershoots a vertex's own need by more than half
        scale = np.ones(len(co))
        own = np.linalg.norm(d, axis=1)
        got = np.linalg.norm(sm, axis=1)
        mask = own > 1e-6
        scale[mask] = np.maximum(1.0, 0.5 * own[mask] / np.maximum(got[mask], 1e-6))
        disp += smooth(sm * scale[:, None])
    d, n = need(co + disp)
    log("  push-out left %d vertices inside (pushed hard)" % n)
    return co + disp + d


def snug_waist(co, body_layer, hem_z):
    """Under the hoodie the trousers hug the body (a belted waist), so the hoodie hem falls over them
    instead of fighting their baggy top."""
    bvh = mesh_bvh(*body_layer)
    out = co.copy()
    for i, p in enumerate(co):
        t = min(1.0, max(0.0, (p[2] - (hem_z - 0.04)) / 0.04))
        if t <= 0:
            continue
        loc, nrm, _, _ = bvh.find_nearest(Vector(p), 0.3)
        if loc is None:
            continue
        d = (Vector(p) - loc).dot(nrm)
        if d > 0.008:
            out[i] = np.array(p) + t * (np.array(loc + nrm * 0.008) - np.array(p))
    return out


def polys(me):
    return [list(p.vertices) for p in me.polygons]


def set_coords(obj, world_co):
    inv = obj.matrix_world.inverted()
    for v, p in zip(obj.data.vertices, world_co):
        v.co = inv @ Vector(p)
    obj.data.update()


def make_static(obj):
    """Unparents a garment, drops its modifiers and vertex groups (it gets the body's later)."""
    mw = obj.matrix_world.copy()
    obj.parent = None
    obj.matrix_world = mw
    for m in list(obj.modifiers):
        obj.modifiers.remove(m)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    obj.select_set(False)


def drop_material_faces(obj, mat_name):
    idx = [i for i, m in enumerate(obj.data.materials) if m and m.name.startswith(mat_name)]
    if not idx:
        return
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.material_index in idx], context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.to_mesh(obj.data)
    bm.free()
    for i in sorted(idx, reverse=True):
        obj.data.materials.pop(index=i)


def cut_covered(obj, covers, ring=True):
    """Deletes faces of obj that lie under the covering surfaces: a ray out along the normal hits one
    within 8 cm (five slightly spread rays all hit), or a ray inward hits one within 2.5 cm (the cover
    sank just under it). Keeps one ring of faces under each hem so no gap opens when cloth moves."""
    me = obj.data
    mw = obj.matrix_world
    nm = mw.to_3x3().inverted().transposed()
    covered = np.zeros(len(me.vertices), bool)
    jits = (Vector((0, 0, 0)), Vector((0.25, 0, 0)), Vector((-0.25, 0, 0)), Vector((0, 0, 0.25)), Vector((0, 0, -0.25)))
    for v in me.vertices:
        p = mw @ v.co
        n = (nm @ v.normal).normalized()
        if any(b.ray_cast(p + n * 0.001, -n, 0.025)[0] is not None for b in covers):
            covered[v.index] = True
            continue
        hits = 0
        for jit in jits:
            d = (n + jit).normalized()
            if any(b.ray_cast(p + d * 0.001, d, 0.08)[0] is not None for b in covers):
                hits += 1
        covered[v.index] = hits == 5
    keep = covered.copy()
    if ring:
        for e in me.edges:
            a, b = e.vertices
            if covered[a] != covered[b]:
                keep[a] = keep[b] = False
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.verts.ensure_lookup_table()
    dead = [f for f in bm.faces if all(keep[v.index] for v in f.verts)]
    bmesh.ops.delete(bm, geom=dead, context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.to_mesh(me)
    bm.free()
    log("%s: cut %d faces under the clothes" % (obj.name, len(dead)))


def skin_from_body(obj, body, rig):
    for g in list(obj.vertex_groups):
        obj.vertex_groups.remove(g)
    deform = {b.name for b in rig.data.bones if b.use_deform}
    for g in body.vertex_groups:
        if g.name in deform:
            obj.vertex_groups.new(name=g.name)
    dt = obj.modifiers.new("dt", "DATA_TRANSFER")
    dt.object = body
    dt.use_vert_data = True
    dt.data_types_verts = {"VGROUP_WEIGHTS"}
    dt.vert_mapping = "POLYINTERP_NEAREST"
    dt.layers_vgroup_select_src = "ALL"
    dt.layers_vgroup_select_dst = "NAME"
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=dt.name)
    # limit to 4 influences, normalised
    bpy.ops.object.vertex_group_limit_total(group_select_mode="ALL", limit=4)
    bpy.ops.object.vertex_group_normalize_all(lock_active=False)
    obj.parent = rig
    obj.matrix_parent_inverse = rig.matrix_world.inverted()
    am = obj.modifiers.new("Armature", "ARMATURE")
    am.object = rig


# ------------------------------------------------------------------------------ animation
# Murmuration's own idle and walk, moved onto the hero's rig: each hero bone takes the world-space
# rotation its Murmuration bone made away from rest, after a fixed rest-pose correction (the
# directions of the two rest poses differ, e.g. arms hang 23 vs 41 degrees). The pelvis follows
# Murmuration's hips. Walk: one gait cycle, in place, left heel strike at 0, right at 0.5; the
# planted foot moves back WALK_STRIDE per cycle (the pawn plays it by distance so feet don't slip).
CLIPS = {"idle": ("Idle", 4.0), "walk": ("Walk", 1.1)}       # Murmuration clip -> (name, seconds)
WALK_STRIDE = 0.90                   # m per cycle: 2 x REACH 0.27 over STANCE 0.6 in fp_body.py
FPS = 30
RETARGET = {"pelvis": "hips", "spine_01": "spine", "spine_02": "spine", "spine_03": "chest",
            "neck_01": "neck", "head": "head"}
for _s, _z in ((".L", "_l"), (".R", "_r")):
    RETARGET.update({"clavicle" + _z: "shoulder" + _s, "upperarm" + _z: "upper_arm" + _s,
                     "lowerarm" + _z: "forearm_twist" + _s, "hand" + _z: "hand" + _s,
                     "thigh" + _z: "thigh" + _s, "calf" + _z: "shin" + _s, "foot" + _z: "foot" + _s,
                     "ball" + _z: "foot" + _s})
    for i in (1, 2, 3):
        RETARGET["thumb_%02d%s" % (i, _z)] = "thumb.%02d%s" % (i, _s)
        for zf, mf in (("index", "f_index"), ("middle", "f_middle"), ("ring", "f_ring"), ("pinky", "f_little")):
            RETARGET["%s_%02d%s" % (zf, i, _z)] = "%s.%02d%s" % (mf, i, _s)
# the rest-pose direction correction uses these segments (hero bone -> (hero joints, murm joints))
ALIGN = {}
for _s, _z in ((".L", "_l"), (".R", "_r")):
    ALIGN.update({
        "clavicle" + _z: (("clavicle" + _z, "upperarm" + _z), ("shoulder" + _s, "upper_arm" + _s)),
        "upperarm" + _z: (("upperarm" + _z, "lowerarm" + _z), ("upper_arm" + _s, "forearm" + _s)),
        "lowerarm" + _z: (("lowerarm" + _z, "hand" + _z), ("forearm" + _s, "hand" + _s)),
        "hand" + _z: (("hand" + _z, "middle_01" + _z), ("hand" + _s, "f_middle.01" + _s)),
        "thigh" + _z: (("thigh" + _z, "calf" + _z), ("thigh" + _s, "shin" + _s)),
        "calf" + _z: (("calf" + _z, "foot" + _z), ("shin" + _s, "foot" + _s)),
    })


def bake_clips(zr, mr):
    """Bakes Murmuration's idle and walk onto the hero rig as actions Idle and Walk."""
    sc = bpy.context.scene
    sc.render.fps = FPS
    R = ROT.to_3x3()
    mw = mr.matrix_world.to_3x3()
    zw = zr.matrix_world
    mrest = {b.name: R @ mw @ b.matrix_local.to_3x3() for b in mr.data.bones}
    zrest = {b.name: (zw @ b.matrix_local) for b in zr.data.bones}
    # rest correction per hero bone (fingers and the hand's children take the hand's)
    corr = {}
    for zb, (zseg, mseg) in ALIGN.items():
        dz = (joint(zr, zseg[1]) - joint(zr, zseg[0])).normalized()
        dm = (R @ (joint(mr, mseg[1]) - joint(mr, mseg[0]))).normalized()
        corr[zb] = dz.rotation_difference(dm).to_matrix()
    for zb in RETARGET:
        if zb not in corr:
            side = zb[-2:]
            finger = any(f in zb for f in ("thumb", "index", "middle", "ring", "pinky"))
            corr[zb] = corr["hand" + side] if finger else Matrix.Identity(3)
    hips_rest = R @ (mr.matrix_world @ mr.data.bones["hips"].head_local)
    pelvis_rest = zrest["pelvis"].translation.copy()
    order = [b.name for b in zr.data.bones]                     # parents before children
    mr.data.pose_position = "POSE"
    if not zr.animation_data:
        zr.animation_data_create()
    for pb in zr.pose.bones:
        pb.rotation_mode = "QUATERNION"
    made = []
    for src, (dst, dur) in CLIPS.items():
        act = bpy.data.actions.get(src)
        if act is None:
            log("no Murmuration clip", src)
            continue
        mr.animation_data.action = act
        if hasattr(mr.animation_data, "action_slot") and act.slots:
            mr.animation_data.action_slot = act.slots[0]
        f0 = act.frame_range[0]
        n = int(round(dur * FPS))
        out = bpy.data.actions.new(dst)
        zr.animation_data.action = out
        for k in range(n + 1):
            t = (k % n) / FPS                                    # last key = first: a clean loop
            fr = f0 + t * sc.render.fps
            sc.frame_set(int(fr), subframe=fr - int(fr))
            mpose = {pb.name: R @ mw @ pb.matrix.to_3x3() for pb in mr.pose.bones}
            hips = R @ (mr.matrix_world @ mr.pose.bones["hips"].head)
            for zb in order:
                pb = zr.pose.bones[zb]
                mb = RETARGET.get(zb)
                rest = zrest[zb]
                if mb is None or mb not in mpose:
                    continue
                D = mpose[mb] @ mrest[mb].transposed()
                world = (D @ corr[zb] @ rest.to_3x3()).to_4x4()
                world.translation = pelvis_rest + (hips - hips_rest) if zb == "pelvis" else zw @ pb.head
                pb.matrix = zw.inverted() @ world
                bpy.context.view_layer.update()
            for zb in order:
                pb = zr.pose.bones[zb]
                if zb in RETARGET:
                    pb.keyframe_insert("rotation_quaternion", frame=k)
                    if zb == "pelvis":
                        pb.keyframe_insert("location", frame=k)
        out.use_fake_user = True
        made.append(out)
        log("baked %s: %d keys" % (dst, n + 1))
    zr.animation_data.action = None
    for pb in zr.pose.bones:
        pb.matrix_basis = Matrix()
    for a in list(bpy.data.actions):
        if a not in made:
            bpy.data.actions.remove(a)
    return made


# ------------------------------------------------------------------------------ main
def build():
    zr, body = load_zoomer()
    mr, garments = import_murmuration()
    drop_material_faces(garments["Hoodie"], "Skin")              # Murmuration's neck lives in the hoodie
    rest = {n: rest_points(o) for n, o in garments.items()}
    # turn Murmuration to face -Y like the Zoomer
    for n in rest:
        rest[n] = (np.c_[rest[n], np.ones(len(rest[n]))] @ np.array(ROT).T)[:, :3]
    measure_murm_feet(mr, rest["Shoes"], garments["Shoes"])
    xf = bone_xforms(mr, zr)
    moved = {n: transfer(garments[n], rest[n], mr, xf) for n in GARMENTS}
    clips = bake_clips(zr, mr)
    for n in GARMENTS:
        make_static(garments[n])
        set_coords(garments[n], moved[n])
    bpy.data.objects.remove(mr)

    # fit: each layer outside the body and the layers under it
    body_co = rest_points(body)
    layers = [(body_co, polys(body.data))]
    for n in GARMENTS:
        o = garments[n]
        co = np.array([o.matrix_world @ v.co for v in o.data.vertices])
        log("fit", n)
        # each garment clears the body; the overshirt also clears the hoodie (the trousers' waistband
        # under the hoodie is cut away instead: pushing the hem out over it crumples the rib)
        below = {"Hoodie": "Trousers", "Overshirt": "Hoodie"}.get(n)
        under = [layers[0]] + ([layers[GARMENTS.index(below) + 1]] if below else [])
        if n == "Trousers":
            co = snug_waist(co, layers[0], moved["Hoodie"][:, 2].min() + 0.03)
        allc = np.vstack([c for c, _ in under])
        off, allf = 0, []
        for c, f in under:
            allf += [[i + off for i in p] for p in f]
            off += len(c)
        co = push_out(co, polys(o.data), mesh_bvh(allc, allf), GAP[n])
        set_coords(o, co)
        layers.append((co, polys(o.data)))

    bvh = {n: mesh_bvh(c, f) for n, (c, f) in zip(GARMENTS, layers[1:])}
    cut_covered(body, list(bvh.values()))
    cut_covered(garments["Trousers"], [bvh["Hoodie"]])     # whatever of the waistband still pokes
    cut_covered(garments["Hoodie"], [bvh["Overshirt"]])    # the hood and back under the overshirt
    for n in GARMENTS:
        skin_from_body(garments[n], body, zr)
        garments[n].name = "hero." + n.lower()
    sole = min((garments["Shoes"].matrix_world @ v.co).z for v in garments["Shoes"].data.vertices)
    log("shoe soles at z = %.3f (pawn lifts the mesh by %.1f cm)" % (sole, -sole * 100))
    zr.name = "hero.rig"
    body.name = "hero"
    return zr, body, garments, clips


def export(rig, clips):
    sys.path.insert(0, HUMANS_DIR)
    import mpfb_pipeline as mp
    meshes = [c for c in rig.children if c.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for m in meshes:
        m.select_set(True)
    body = bpy.data.objects["hero"]
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.join()
    body.name = body.data.name = "SK_Hero"
    rig.name = "Armature_hero"
    mp.export_glb(rig, OUT, actions=clips)
    log("wrote", OUT, os.path.getsize(OUT) // 1024, "KB")


def preview(rig, tag=""):
    os.makedirs(RENDERS, exist_ok=True)
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE"
    sc.render.resolution_x, sc.render.resolution_y = 700, 1000
    w = bpy.data.worlds.new("w") if not sc.world else sc.world
    sc.world = w
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs[0].default_value = (0.6, 0.6, 0.62, 1)
    w.node_tree.nodes["Background"].inputs[1].default_value = 1.0
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 3
    sun.rotation_euler = (math.radians(50), 0, math.radians(30))
    sc.collection.objects.link(sun)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 2.0
    sc.collection.objects.link(cam)
    sc.camera = cam
    for name, yaw in (("front", 0), ("side", 90), ("back", 180), ("q", 35)):
        a = math.radians(yaw)
        cam.location = (4 * math.sin(a), -4 * math.cos(a), 0.92)
        cam.rotation_euler = (math.radians(90), 0, a)
        sc.render.filepath = os.path.join(RENDERS, "hero%s_%s.png" % (tag, name))
        bpy.ops.render.render(write_still=True)


if __name__ == "__main__":
    rig, body, garments, clips = build()
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ROOT, "blender", "hero", "hero.blend"))
    if "--preview" in ARGS:
        preview(rig)
    if "--no-export" not in ARGS:
        export(rig, clips)
