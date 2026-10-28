"""A clean game human built by us, with the generated (hi3d) model only as the North Star.

    Blender -b -P blender/characters/clean_human.py -- nao [--preview] [--stage=base|fit|all]

Why (Dima, 2026-10-28): the hi3d people can't be fixed as they are: torn painted eyes, lumpy faces, no lids, nothing to
edit. Like the vestibules, the game mesh is ours and the generated model is the shape reference and the bake source.

How:
  1. Base: a MakeHuman body through MPFB2 (blender/humans/mpfb_pipeline.py): clean quad topology with edge loops round
     the eyes and mouth, real eyeballs, lids, lashes, teeth and tongue, the house skeleton (game_engine + jaw/eyes =
     SK_Human_Skeleton) with MakeHuman's weights, 52 ARKit face shapes + 15 visemes. character.json "clean.macro".
  2. T-pose: the arm chains rotated rigidly into the generated T (as rig_character.py does for its donor), baked into
     the body and every face shape, then made the rest pose.
  3. Fit: scaled, arms stretched, hips centred and legs moved (rig_character's fit_donor/centre_body/fit_legs); then
     every skin vertex is moved along its normal onto the model's surface where that is near and facing the same way,
     the rest filled in smoothly (eyes, lips' insides, hands, the scalp under hair, the legs under a skirt keep their
     MakeHuman form). Eyes, lashes, teeth ride along with the lids round them.
  4. Look: the model's colours baked onto the body's own UVs; the painted eyes on the lids replaced by skin.
  5. Skirt and hair: our own meshes (a skirt ring by ring from the model's outline; hair: the model's hair cut off the
     head, remeshed and decimated), weighted to pelvis/thighs and head.
"""
import json
import math
import os
import sys

import bpy
import bmesh
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "blender", "humans"))
import rig_character as R  # noqa: E402

BLENDER_RENDERS = os.path.join(ROOT, "renders", "characters", "clean")


def log(m):
    print("CLEAN " + m, flush=True)


# ---------------------------------------------------------------------------------------------- shape keys

def keys_of(obj):
    sk = obj.data.shape_keys
    return list(sk.key_blocks) if sk else []


def get_co(data, n):
    a = np.empty(n * 3)
    data.foreach_get("co", a)
    return a.reshape(n, 3)


def move_all(obj, fn):
    """Applies fn (object-space Nx3 -> Nx3) to the mesh and to every shape key, keeping each key's own delta."""
    n = len(obj.data.vertices)
    ks = keys_of(obj)
    base = get_co(obj.data.vertices, n)
    new = fn(base)
    d = new - base
    for k in ks:
        c = get_co(k.data, n)
        k.data.foreach_set("co", (c + d).ravel())
    obj.data.vertices.foreach_set("co", new.ravel())
    obj.data.update()


def warp(arm, meshes, fn):
    """rig_character.warp_donor for a keyed body: a world-space point map applied to the meshes and the bones."""
    for m in meshes:
        mw = np.array(m.matrix_world)
        inv = np.array(m.matrix_world.inverted())

        def f(co, mw=mw, inv=inv):
            w = co @ mw[:3, :3].T + mw[:3, 3]
            return fn(w) @ inv[:3, :3].T + inv[:3, 3]
        move_all(m, f)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    inv = arm.matrix_world.inverted()
    mw = arm.matrix_world
    for e in arm.data.edit_bones:
        roll = e.roll
        h = fn(np.array([tuple(mw @ e.head)]))[0]
        t = fn(np.array([tuple(mw @ e.tail)]))[0]
        e.head = inv @ Vector(h)
        e.tail = inv @ Vector(t)
        e.roll = roll
    bpy.ops.object.mode_set(mode="OBJECT")


def pose_to_rest(arm, meshes):
    """Bakes the armature's current pose into every mesh (basis and face shapes alike) and makes it the rest pose.
    Modifier apply refuses meshes with shape keys, so the posed basis is read from the depsgraph with the keys at 0."""
    dg = bpy.context.evaluated_depsgraph_get()
    deltas = {}
    for m in meshes:
        vals = [k.value for k in keys_of(m)]
        for k in keys_of(m):
            k.value = 0.0
        bpy.context.view_layer.update()
        dg = bpy.context.evaluated_depsgraph_get()
        ev = m.evaluated_get(dg)
        n = len(m.data.vertices)
        posed = get_co(ev.data.vertices, n)
        deltas[m] = posed - get_co(m.data.vertices, n)
        for k, v in zip(keys_of(m), vals):
            k.value = v
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="POSE")
    bpy.ops.pose.select_all(action="SELECT")
    bpy.ops.pose.armature_apply(selected=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    for m, d in deltas.items():
        move_all(m, lambda co, d=d: co + d)


def t_pose(arm, meshes):
    pb = arm.pose.bones
    for side, chain in R.ARM_CHAINS.items():
        out = Vector((1 if side == "l" else -1, 0, 0))
        for name in chain:
            bpy.context.view_layer.update()
            b = pb[name]
            mw = arm.matrix_world @ b.matrix
            head = mw.translation
            child = [c for c in b.children if c.name in chain]
            tail = (arm.matrix_world @ child[0].matrix).translation if child else (arm.matrix_world @ b.tail)
            q = (tail - head).normalized().rotation_difference(out)
            rot = Matrix.Translation(head) @ q.to_matrix().to_4x4() @ Matrix.Translation(-head)
            b.matrix = arm.matrix_world.inverted() @ rot @ mw
    pose_to_rest(arm, meshes)


# ---------------------------------------------------------------------------------------------- the base

def build_base(cid, spec):
    import mpfb_pipeline as P
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.preferences.addon_enable(module="bl_ext.user_default.mpfb")
    c = spec["clean"]
    s = {"name": cid, "macro": c["macro"], "targets": c.get("targets", {}), "skin": c.get("skin", "young_caucasian_female2"),
         "eyes": c.get("eyes", "high-poly"), "eyelashes": c.get("eyelashes", "eyelashes01"),
         "eyebrows": c.get("eyebrows", "eyebrow001")}
    if isinstance(c.get("hair"), str) and c["hair"] != "shell":        # a MakeHuman hairstyle, tinted later
        s["hair"] = c["hair"]
    bm, arm, objs = P.build(s)
    if objs.get("eyebrows") and not c.get("keep_eyebrows", False):     # brows come painted from the model
        bpy.data.objects.remove(objs.pop("eyebrows"))
    root, body = P.bake_for_export(bm)
    for o in list(bpy.data.objects):                                   # keep only the export copy
        if o not in (root, body):
            bpy.data.objects.remove(o)
    for a in list(bpy.data.actions):
        bpy.data.actions.remove(a)
    log("base: %d verts, %d faces, %d shape keys, materials %s" % (
        len(body.data.vertices), len(body.data.polygons), len(keys_of(body)), [m.name for m in body.data.materials]))
    return root, body


def body_mask(body):
    """Vertices of the skin (the basemesh's material), not the eyes, lashes, teeth or tongue."""
    skin = [i for i, m in enumerate(body.data.materials) if m.name.endswith(".body")]
    n = len(body.data.vertices)
    mask = np.zeros(n, bool)
    for p in body.data.polygons:
        if p.material_index in skin:
            mask[list(p.vertices)] = True
    return mask


# ---------------------------------------------------------------------------------------------- the fit

def group_weight(obj, names):
    idx = {g.index for g in obj.vertex_groups if g.name in names}
    w = np.zeros(len(obj.data.vertices))
    for v in obj.data.vertices:
        for g in v.groups:
            if g.group in idx:
                w[v.index] += g.weight
    return w


def vertex_normals(obj):
    n = len(obj.data.vertices)
    a = np.empty(n * 3)
    obj.data.vertices.foreach_get("normal", a)
    return a.reshape(n, 3)


def neighbours(obj, mask):
    n = len(obj.data.vertices)
    e = np.empty(len(obj.data.edges) * 2, int)
    obj.data.edges.foreach_get("vertices", e)
    e = e.reshape(-1, 2)
    e = e[mask[e[:, 0]] & mask[e[:, 1]]]
    nb = [[] for _ in range(n)]
    for a, b in e:
        nb[a].append(b)
        nb[b].append(a)
    return nb


def diffuse(values, conf, nb, iterations=300, smooth=4):
    """Fills unknown values (conf 0) from their neighbours and eases the known ones by conf; then a few smoothing
    passes over all (the generated surface is lumpy)."""
    idx = [i for i in range(len(nb)) if nb[i]]
    ii = np.concatenate([np.full(len(nb[i]), i) for i in idx])
    jj = np.concatenate([nb[i] for i in idx])
    cnt = np.bincount(ii, minlength=len(nb)).astype(float)
    has = cnt > 0
    v = values.copy()
    c = conf[:, None]

    def avg(x):
        s = np.zeros_like(x)
        np.add.at(s, ii, x[jj])
        out = x.copy()
        out[has] = s[has] / cnt[has, None]
        return out
    for _ in range(iterations):
        v = c * values + (1 - c) * avg(v)
    for _ in range(smooth):
        v = 0.5 * v + 0.5 * avg(v)
    return v


def surface_fit(arm, body, high, spec, marks):
    """Every skin vertex onto the model's surface along its normal, where that surface is near and faces the same way."""
    c = spec["clean"]
    fit = c.get("fit", {})
    out_max, in_max = fit.get("out", 0.035), fit.get("in", 0.02)
    mask = body_mask(body)
    co = R.world_co(body)
    nrm = vertex_normals(body)
    bvh = BVHTree.FromObject(high, bpy.context.evaluated_depsgraph_get())
    n = len(co)
    raw = np.zeros((n, 3))
    conf = np.zeros(n)
    for i in np.nonzero(mask)[0]:
        p, d = Vector(co[i]), Vector(nrm[i])
        best = None
        for sign, lim in ((1, out_max), (-1, in_max)):
            h = bvh.ray_cast(p, d * sign, lim)
            if h[0] is not None and h[1].dot(d) > 0.3:
                t = (h[0] - p).length * sign
                if best is None or abs(t) < abs(best):
                    best = t
        if best is not None:
            raw[i] = nrm[i] * best
            conf[i] = 1.0
    hit = conf.sum()
    # keep MakeHuman's own form where the model's is unreliable or hidden
    pts = R.bone_points(arm)
    keep = group_weight(body, {g.name for g in body.vertex_groups
                               if g.name.startswith(("hand_", "thumb", "index", "middle", "ring", "pinky"))}) > 0.3
    for e in ("eye_l", "eye_r"):
        h = np.array(pts[e][0])
        keep |= np.linalg.norm((co - h) * [1, 1.3, 1.5], axis=1) < fit.get("eye_r", 0.024)
    keep |= (group_weight(body, {"head", "jaw"}) > 0.5) & (nrm[:, 1] > 0.3) & (co[:, 2] < pts["eye_l"][0].z)  # mouth's inside
    head = pts["head"][0]
    brow_z = (pts["eye_l"][0].z + 0.018)
    ear_y = head.y + fit.get("ear_y", 0.01)
    scalp = (co[:, 2] > brow_z) | ((co[:, 2] > pts["neck_01"][0].z + 0.04) & (co[:, 1] > ear_y))
    keep |= scalp & (group_weight(body, {"head"}) > 0.5)
    skirt = c.get("skirt")
    if skirt:
        hip = pts["thigh_l"][0].z + 0.06
        keep |= (co[:, 2] > skirt["hem_z"] - 0.01) & (co[:, 2] < hip) & (np.abs(co[:, 0]) > 0.0)
    conf[keep] = 0.0
    nb = neighbours(body, mask)
    disp = diffuse(raw, conf, nb, iterations=fit.get("iterations", 400), smooth=fit.get("smooth", 6))
    # the face only takes the model's broad form: its features stay MakeHuman's (the generated ones are lumpy)
    face = (group_weight(body, {"head", "jaw"}) > 0.3) & mask
    for _ in range(fit.get("face_smooth", 40)):
        d2 = diffuse(disp, np.zeros(n), nb, iterations=1, smooth=0)
        disp[face] = d2[face]
    disp[~mask] = 0.0
    # eyes, lashes, teeth, tongue ride with the skin round them
    kd = KDTree(int(mask.sum()))
    bi = np.nonzero(mask)[0]
    for j, i in enumerate(bi):
        kd.insert(co[i], j)
    kd.balance()
    for i in np.nonzero(~mask)[0]:
        near = kd.find_n(co[i], 8)
        w = np.array([1.0 / max(d, 1e-4) for _, _, d in near])
        disp[i] = (w[:, None] * disp[bi[[j for _, j, _ in near]]]).sum(0) / w.sum()
    log("surface fit: %d of %d skin verts hit the model, %d kept; mean |d| %.4f m, max %.4f" % (
        hit, mask.sum(), (keep & mask).sum(), np.linalg.norm(disp[mask], axis=1).mean(), np.linalg.norm(disp, axis=1).max()))
    inv = np.array(body.matrix_world.inverted())
    move_all(body, lambda o: o + disp @ inv[:3, :3].T)


def mh_marks(body, arm, cid):
    """The MakeHuman face's eyes and mouth, found the same way as the model's (face_rig's unlit front shot)."""
    import subprocess
    import face_rig
    shot = os.path.join(ROOT, "Saved", "FaceShots", cid + "_mh.png")
    os.makedirs(os.path.dirname(shot), exist_ok=True)
    cz = face_rig.face_shot(body, arm, shot)
    dbg = os.path.join(BLENDER_RENDERS, cid + "_mh_marks.png")
    out = subprocess.run(["python3", os.path.join(HERE, "face_marks.py"), shot, "%.5f" % cz, str(face_rig.SHOT), dbg],
                         capture_output=True, text=True)
    if out.returncode != 0:
        raise RuntimeError("face_marks failed: " + out.stderr[-800:])
    return json.loads(out.stdout.strip().splitlines()[-1])


def face_points(m):
    e = m["eyes"]
    mo = m["mouth"]
    return np.array([[e[0]["x"], e[0]["z"]], [e[1]["x"], e[1]["z"]], [mo["x"], mo["z"]],
                     [mo["x"] + mo["w"] / 2, mo["z"]], [mo["x"] - mo["w"] / 2, mo["z"]]])


def eye_centres(body):
    """Our eyeballs' middles (x, z), left (+X) first."""
    mats = [i for i, m in enumerate(body.data.materials) if "-poly" in m.name or m.name.endswith("eyes")]
    co = R.world_co(body)
    vs = set()
    for p in body.data.polygons:
        if p.material_index in mats:
            vs.update(p.vertices)
    e = co[sorted(vs)]
    return [e[e[:, 0] > 0].mean(0), e[e[:, 0] < 0].mean(0)]


def face_warp(high, body, ours, theirs, neck_z, sigma=0.03):
    """Moves the MODEL's painted eyes and mouth onto ours (the model is only a bake source, so bending it costs
    nothing; our face keeps its clean MakeHuman features and proportions): the whole head shifted by the mean offset
    (faded in over the neck), then the rest by a Gaussian RBF on the face's front."""
    a, b = face_points(theirs), face_points(ours)
    d = b - a
    shift = d.mean(0)
    d = d - shift
    k = lambda p, q: np.exp(-((p[:, None, :] - q[None, :, :]) ** 2).sum(-1) / (2 * sigma ** 2))
    w = np.linalg.solve(k(a, a) + 1e-6 * np.eye(len(a)), d)
    co = R.world_co(high)
    head = np.clip((co[:, 2] - neck_z) / 0.06, 0, 1)
    co[:, 0] += shift[0] * head
    co[:, 2] += shift[1] * head
    off = k(co[:, [0, 2]] - shift, a) @ w
    front = np.clip(-co[:, 1] / 0.04, 0, 1) * head
    co[:, 0] += off[:, 0] * front
    co[:, 2] += off[:, 1] * front
    R.set_world_co(high, co)
    # depth: the model may hold its head further back or forward than our body does over the same hips
    bvh = BVHTree.FromObject(high, bpy.context.evaluated_depsgraph_get())
    bc, bn = R.world_co(body), vertex_normals(body)
    zs = [e["z"] for e in ours["eyes"]] + [ours["mouth"]["z"]]
    f = body_mask(body) & (np.abs(bc[:, 0]) < 0.035) & (bc[:, 2] > min(zs) - 0.01) & (bc[:, 2] < max(zs) + 0.01) & (bn[:, 1] < -0.7)
    dy = []
    for p in bc[f]:
        h = bvh.ray_cast(Vector(p) + Vector((0, 0.12, 0)), Vector((0, -1, 0)), 0.3)   # from inside the head, outward
        if h[0] is not None and h[1].y < -0.3:
            dy.append(h[0].y - p[1])
    dy = float(np.median(dy)) if len(dy) > 20 else 0.0
    co[:, 1] -= dy * head
    R.set_world_co(high, co)
    log("model's face moved onto ours: head %s, depth %+.4f, then eyes %s, mouth %s (m)" % (
        np.round(shift, 4).tolist(), -dy, np.round(d[:2], 4).tolist(), np.round(d[2:], 4).tolist()))


def warp_body_only(body, fn):
    mw = np.array(body.matrix_world)
    inv = np.array(body.matrix_world.inverted())
    move_all(body, lambda co: fn(co @ mw[:3, :3].T + mw[:3, 3]) @ inv[:3, :3].T + inv[:3, 3])


def load_high(cid, spec):
    m = R.load_model(os.path.join(HERE, cid, "gen", spec.get("model", "model") + ".glb"), spec["height"])
    R.clean(m)
    R.decimate(m, spec["clean"].get("high_faces", 600000))
    m.name = "hi3d"
    return m


def fit(arm, body, high, spec):
    mco = R.world_co(high)
    target = R.silhouette(mco)
    # the donor share is of the skin's top (a MakeHuman hairstyle mustn't make the body shorter)
    bc = R.world_co(body)
    target["donor_height_share"] = spec.get("donor_height_share", 0.985) * bc[:, 2].max() / bc[body_mask(body), 2].max()
    R.warp_donor = lambda a, b, fn: warp(a, [b], fn)
    R.fit_donor(arm, body, target)
    R.centre_body(arm, body, mco)
    R.fit_legs(arm, body, mco, target["height"])


# ---------------------------------------------------------------------------------------------- the look

MISS = (1.0, 0.0, 1.0)
DEBUG = "--debug" in sys.argv


def img_px(img):
    a = np.empty(img.size[0] * img.size[1] * 4, np.float32)
    img.pixels.foreach_get(a)
    return a.reshape(img.size[1], img.size[0], 4)


def bake_image(name, size, fill):
    img = bpy.data.images.new(name, size, size, alpha=False, float_buffer=False)
    img.pixels.foreach_set(np.tile(np.array((*fill, 1.0), np.float32), size * size))
    return img


def fill_masked(rgb, k, smooth=40):
    """Replaces the texels under mask k (0..1) by their surroundings: pull-push over a pyramid, then smoothing."""
    known = k < 0.05

    def pp(c, kn):
        n = c.shape[0]
        if n <= 4 or kn.all():
            return np.where(kn[..., None], c, c[kn].mean(0) if kn.any() else 0)
        w = kn.astype(np.float32)
        c2 = (c * w[..., None]).reshape(n // 2, 2, n // 2, 2, 3).sum((1, 3))
        w2 = w.reshape(n // 2, 2, n // 2, 2).sum((1, 3))
        k2 = w2 > 0
        c2 = np.where(k2[..., None], c2 / np.maximum(w2, 1e-6)[..., None], 0)
        up = pp(c2, k2).repeat(2, 0).repeat(2, 1)
        return np.where(kn[..., None], c, up)
    f = pp(rgb, known)
    ys, xs = np.nonzero(~known)
    if len(ys):
        y0, y1, x0, x1 = max(ys.min() - 2, 1), min(ys.max() + 3, rgb.shape[0] - 1), max(xs.min() - 2, 1), min(xs.max() + 3, rgb.shape[1] - 1)
        sub, kn = f[y0 - 1:y1 + 1, x0 - 1:x1 + 1].copy(), known[y0 - 1:y1 + 1, x0 - 1:x1 + 1]
        for _ in range(smooth):
            avg = (sub[:-2, 1:-1] + sub[2:, 1:-1] + sub[1:-1, :-2] + sub[1:-1, 2:]) / 4
            inner = sub[1:-1, 1:-1]
            sub[1:-1, 1:-1] = np.where(kn[1:-1, 1:-1, None], inner, avg)
        f[y0 - 1:y1 + 1, x0 - 1:x1 + 1] = sub
    return rgb * (1 - k[..., None]) + f * k[..., None]


def tint_hair(body, arm, tex, spec):
    """A MakeHuman hairstyle takes the model's hair colour: the median of the baked scalp (the model's hair over it)."""
    hm = [m for m in body.data.materials if m.name.split(".", 1)[-1] == spec["clean"].get("hair")]
    if not hm:
        return
    co = R.world_co(body)
    pts = R.bone_points(arm)
    scalp = (group_weight(body, {"head"}) > 0.5) & (co[:, 2] > pts["eye_l"][0].z + 0.05) & body_mask(body)
    uv = body.data.uv_layers[0].data
    n = tex.shape[0]
    px = []
    for lp in body.data.loops:
        if scalp[lp.vertex_index]:
            u, v = uv[lp.index].uv
            px.append(tex[min(int(v * n), n - 1), min(int(u * n), n - 1), :3])
    rgb = np.median(np.array(px), 0) if px else np.array(spec["clean"].get("hair_rgb", (0.05, 0.04, 0.04)))
    rgb = np.array(spec["clean"].get("hair_rgb", rgb))
    for m in hm:
        for nd in m.node_tree.nodes:
            if nd.type == "TEX_IMAGE" and nd.image and nd.image.name.find("normal") < 0:
                im = nd.image.copy()
                im.name = "%s_hair_tint" % body.name
                a = img_px(im).reshape(-1, 4).copy()
                l = a[:, :3] @ np.array([0.2126, 0.7152, 0.0722], np.float32)
                mean = l[a[:, 3] > 0.5].mean() if (a[:, 3] > 0.5).any() else max(l.mean(), 1e-4)
                rel = np.clip((l / max(mean, 1e-4) - 1.0) * 0.8 + 1.0, 0.4, 1.6)
                a[:, :3] = np.clip(np.outer(rel, rgb), 0, 1)
                im.pixels.foreach_set(a.ravel())
                im.pack()
                nd.image = im
    log("hair tinted to %s" % np.round(rgb, 3).tolist())


def skin_copy(body):
    """The body's skin alone, unkeyed, unrigged: the bake target (same UVs as the body)."""
    low = body.copy()
    low.data = body.data.copy()
    bpy.context.scene.collection.objects.link(low)
    low.parent = None
    low.matrix_world = body.matrix_world
    for md in list(low.modifiers):
        low.modifiers.remove(md)
    if low.data.shape_keys:
        low.shape_key_clear()
    skin = body_mask(body)
    bm = bmesh.new()
    bm.from_mesh(low.data)
    bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not skin[v.index]], context="VERTS")
    bm.to_mesh(low.data)
    bm.free()
    return low, skin


def bake_look(body, high, arm, cid, spec, ours, theirs):
    """The model's colours on our UVs (two passes: near, then far for what the near one missed: hands, scalp under
    hair, legs under a skirt), the model's painted eyes round our openings replaced by MakeHuman's lids tinted to
    the model's skin, and whatever still missed filled the same way."""
    size = spec.get("texture", 2048)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.samples = 1
    sc.cycles.device = "CPU"
    low, skin = skin_copy(body)
    co = R.world_co(low)
    nrm = vertex_normals(low)
    # mask colours: R = round the eyes (the painted eye to replace), G = cheeks (to match the skin tone)
    col = np.zeros((len(co), 4), np.float32)
    col[:, 3] = 1
    ew, eh = theirs["eyes"][0]["w"], theirs["eyes"][0]["h"]
    for e in ours["eyes"]:
        r = np.sqrt(((co[:, 0] - e["x"]) / (ew * 0.55)) ** 2 + ((co[:, 2] - e["z"]) / (eh * 0.72)) ** 2)
        front = co[:, 1] < e.get("y", -0.05) + 0.03
        col[:, 0] = np.maximum(col[:, 0], np.clip((1.3 - r) / 0.3, 0, 1) * front)
        col[:, 2] = np.maximum(col[:, 2], np.clip((2.1 - r) / 0.4, 0, 1) * front)   # where painted lashes may reach
        cheek = (np.abs(co[:, 0] - e["x"]) < 0.012) & (co[:, 2] < e["z"] - 0.025) & (co[:, 2] > e["z"] - 0.04) & front & (nrm[:, 1] < -0.5)
        col[cheek, 1] = 1
    side = (group_weight(low, {"head", "jaw", "neck_01"}) > 0.3) & (co[:, 2] < ours["eyes"][0]["z"] + 0.01)
    col2 = np.zeros((len(co), 4), np.float32)
    col2[:, 3] = 1
    col2[side, 0] = 1.0                                      # the face below the brows takes no far pass
    col2[side & (np.abs(co[:, 0]) > 0.035), 1] = 1.0         # its sides lose stray dark paint (a braid, hair)
    attr = low.data.color_attributes.new("mask", "FLOAT_COLOR", "POINT")
    attr.data.foreach_set("color", col.ravel())
    attr = low.data.color_attributes.new("mask2", "FLOAT_COLOR", "POINT")
    attr.data.foreach_set("color", col2.ravel())
    mat = bpy.data.materials.new("bake_target")
    mat.use_nodes = True
    nt = mat.node_tree
    for n in list(nt.nodes):
        if n.type != "OUTPUT_MATERIAL":
            nt.nodes.remove(n)
    em = nt.nodes.new("ShaderNodeEmission")
    at = nt.nodes.new("ShaderNodeAttribute")
    at.attribute_name = "mask"
    nt.links.new(at.outputs["Color"], em.inputs["Color"])
    nt.links.new(em.outputs["Emission"], nt.nodes["Material Output"].inputs["Surface"])
    tn = nt.nodes.new("ShaderNodeTexImage")
    nt.nodes.active = tn
    low.data.materials.clear()
    low.data.materials.append(mat)

    def bake(img, **kw):
        tn.image = img
        bpy.ops.object.select_all(action="DESELECT")
        if kw.get("use_selected_to_active"):
            high.select_set(True)
        low.select_set(True)
        bpy.context.view_layer.objects.active = low
        bpy.ops.object.bake(margin=16, **kw)
        if DEBUG:
            img.filepath_raw = os.path.join(BLENDER_RENDERS, img.name + ".png")
            img.file_format = "PNG"
            img.save()
        return img_px(img)
    high.hide_render = False
    near = bake(bake_image(cid + "_near", size, MISS), type="DIFFUSE", pass_filter={"COLOR"},
                use_selected_to_active=True, cage_extrusion=0.015, max_ray_distance=0.04)
    far = bake(bake_image(cid + "_far", size, MISS), type="DIFFUSE", pass_filter={"COLOR"},
               use_selected_to_active=True, cage_extrusion=0.08, max_ray_distance=0.16)
    mask = bake(bake_image(cid + "_mask", size, (0, 0, 0)), type="EMIT")
    at.attribute_name = "mask2"
    mask2 = bake(bake_image(cid + "_mask2", size, (0, 0, 0)), type="EMIT")
    out = near.copy()
    m = near[..., :3].max(-1) < 0.004                       # unbaked texels stay black
    face_side = mask2[..., 0] > 0.5
    out[m & ~face_side] = far[m & ~face_side]
    gap = (m & face_side).astype(np.float32)                 # under the model's hair: filled from the skin round it
    # replace the painted eye, and dark paint (lashes, liner) a little round it, by the skin round them
    lum = out[..., :3] @ np.array([0.2126, 0.7152, 0.0722], np.float32)
    ck = (mask[..., 1] > 0.5) & (lum > 0.08)
    skin_l = float(np.median(lum[ck])) if ck.sum() > 50 else 0.6
    dark = np.clip((0.85 * skin_l - lum) / (0.35 * skin_l), 0, 1)
    k = np.maximum(np.maximum(np.clip(mask[..., 0], 0, 1), np.clip(np.maximum(mask[..., 2], mask2[..., 1]), 0, 1) * dark), gap)
    out[..., :3] = fill_masked(out[..., :3], k)
    out[..., 3] = 1
    img = bpy.data.images.new(cid + "_body", size, size, alpha=False)
    img.pixels.foreach_set(out.ravel())
    img.filepath_raw = os.path.join(HERE, cid, "export", cid + "_body.png")
    img.file_format = "PNG"
    img.save()
    img.pack()
    for n in body.data.materials[0].node_tree.nodes:
        if n.type == "TEX_IMAGE":
            n.image = img
    bpy.data.objects.remove(low)
    tint_hair(body, arm, out, spec)
    log("look baked %d: the far pass filled %.1f%%, skin luminance %.3f from %d cheek texels, %d texels repainted" % (
        size, 100 * m.mean(), skin_l, ck.sum(), (k > 0.5).sum()))


# ---------------------------------------------------------------------------------------------- hair and skirt

def bake_onto(obj, high, cid, part, size, ext=0.01, dist=0.03):
    """Smart-UV the part and bake the model's colours onto it; one material <cid>.<part>."""
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.004)
    bpy.ops.object.mode_set(mode="OBJECT")
    mat = bpy.data.materials.new("%s.%s" % (cid, part))
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Roughness"].default_value = 0.6
    tn = nt.nodes.new("ShaderNodeTexImage")
    tn.image = bpy.data.images.new("%s_%s" % (cid, part), size, size, alpha=False)
    nt.nodes.active = tn
    obj.data.materials.clear()
    obj.data.materials.append(mat)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.samples = 1
    high.hide_render = False
    high.select_set(True)
    bpy.ops.object.bake(type="DIFFUSE", pass_filter={"COLOR"}, use_selected_to_active=True, cage_extrusion=ext,
                        max_ray_distance=dist, margin=8)
    img = tn.image
    nt.links.new(tn.outputs["Color"], bsdf.inputs["Base Color"])
    img.filepath_raw = os.path.join(HERE, cid, "export", "%s_%s.png" % (cid, part))
    img.file_format = "PNG"
    img.save()
    img.pack()


def skin_bvh(body):
    low, _ = skin_copy(body)
    tree = BVHTree.FromObject(low, bpy.context.evaluated_depsgraph_get())
    bpy.data.objects.remove(low)
    return tree


def rig_to(obj, arm, weights):
    """Parents a part to the armature with fixed per-vertex weights {bone: array}."""
    obj.parent = arm
    obj.matrix_parent_inverse = arm.matrix_world.inverted()
    for bone, w in weights.items():
        g = obj.vertex_groups.new(name=bone)
        for i, x in enumerate(w):
            if x > 1e-3:
                g.add([i], float(x), "REPLACE")
    md = obj.modifiers.new("Armature", "ARMATURE")
    md.object = arm


def make_hair(body, high, arm, cid, spec):
    """The model's hair (all of its head that stands off our skin) as its own clean, closed, decimated mesh."""
    h = spec["clean"].get("hair", {})
    pts = R.bone_points(arm)
    min_z = h.get("min_z", pts["neck_01"][0].z + 0.0)
    tree = skin_bvh(body)
    me = high.data
    n = len(me.polygons)
    cen = np.empty(n * 3)
    me.polygons.foreach_get("center", cen)
    cen = cen.reshape(n, 3)
    keep = np.zeros(n, bool)
    off = h.get("off", 0.005)
    ez, ey = pts["eye_l"][0].z, pts["eye_l"][0].y
    face = (cen[:, 2] < ez + 0.012) & (np.abs(cen[:, 0]) < 0.045) & (cen[:, 1] < ey + 0.01)   # the face's front is skin
    hc = np.array(pts["head"][0])
    near_head = np.linalg.norm((cen - hc)[:, :2], axis=1) < h.get("radius", 0.14)
    for i in np.nonzero((cen[:, 2] > min_z) & ~face & near_head)[0]:
        p = Vector(cen[i])
        loc, nrm, _, d = tree.find_nearest(p)
        if loc is not None and d > off and (p - loc).dot(nrm) > 0:
            keep[i] = True
    hair = high.copy()
    hair.data = high.data.copy()
    bpy.context.scene.collection.objects.link(hair)
    bm = bmesh.new()
    bm.from_mesh(hair.data)
    bm.faces.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if not keep[f.index]], context="FACES")
    bm.to_mesh(hair.data)
    bm.free()
    bpy.context.view_layer.objects.active = hair
    for md, kw in (("SOLIDIFY", {"thickness": h.get("thickness", 0.004), "offset": -1}),
                   ("REMESH", {"mode": "VOXEL", "voxel_size": h.get("voxel", 0.004)})):
        m = hair.modifiers.new(md.lower(), md)
        for k, v in kw.items():
            setattr(m, k, v)
        bpy.ops.object.modifier_apply(modifier=m.name)
    R.clean(hair, min_island=200)
    for _ in range(2):
        m = hair.modifiers.new("smooth", "SMOOTH")
        m.factor, m.iterations = 0.5, 4
        bpy.ops.object.modifier_apply(modifier=m.name)
    R.decimate(hair, h.get("faces", 3200))
    for p in hair.data.polygons:
        p.use_smooth = True
    hair.name = "%s_hair" % cid
    bake_onto(hair, high, cid, "hair", h.get("texture", 1024))
    rig_to(hair, arm, {"head": np.ones(len(hair.data.vertices))})
    log("hair: %d of the model's head faces, -> %d faces" % (keep.sum(), len(hair.data.polygons)))
    return hair


def make_hair_shell(body, high, arm, cid, spec):
    """Our own hair: a clean shell over the model's hair. Rays come in toward the head's middle on a grid of
    directions (azimuth x elevation); where the first thing they meet stands off our skin, that is hair, and the
    shell takes that point. Quads only where all four corners are hair: the open edges are the bangs and the bob's
    hem. Thickened inward, smoothed, the model's colours baked on, rigged to the head."""
    h = spec["clean"].get("hair_shell", {})
    pts = R.bone_points(arm)
    c = Vector(pts["head"][0]) + Vector(h.get("centre", (0, 0.005, 0.07)))
    bvh = BVHTree.FromObject(high, bpy.context.evaluated_depsgraph_get())
    tree = skin_bvh(body)
    na, ne = h.get("az", 120), h.get("el", 80)
    min_z = pts["neck_01"][0].z + h.get("min_dz", 0.0)
    els = np.radians(np.linspace(h.get("el_min", -75), 88, ne))
    azs = np.linspace(0, 2 * math.pi, na, endpoint=False)
    P = np.zeros((ne, na, 3))
    ok = np.zeros((ne, na), bool)
    off = h.get("off", 0.004)
    for i, el in enumerate(els):
        for j, az in enumerate(azs):
            d = Vector((math.cos(el) * math.sin(az), -math.cos(el) * math.cos(az), math.sin(el)))
            o = c + d * 0.3
            hit = bvh.ray_cast(o, -d, 0.3)
            if hit[0] is None:
                continue
            loc, nrm, _, dist = tree.find_nearest(hit[0])
            if loc is not None and dist > off and (hit[0] - loc).dot(nrm) > 0 and hit[0].z > min_z:
                P[i, j] = hit[0]
                ok[i, j] = True
    # tidy the mask: drop lone hair cells, fill lone holes
    for _ in range(2):
        nb = sum(np.roll(ok, s, 1) for s in (-1, 1)) + np.vstack([ok[1:], ok[-1:]]) + np.vstack([ok[:1], ok[:-1]])
        ok = np.where(ok, nb >= 2, nb >= 3) & (ok | (P != 0).any(-1))
    idx = -np.ones((ne, na), int)
    verts = []
    for i in range(ne):
        for j in range(na):
            if ok[i, j]:
                idx[i, j] = len(verts)
                verts.append(tuple(P[i, j]))
    ring = [idx[ne - 1, j] for j in range(na) if idx[ne - 1, j] >= 0]            # the crown closes the shell
    top = np.array([verts[k] for k in ring]).mean(0) + np.array([0, 0, 0.004])
    crown = len(verts)
    verts.append(tuple(top))
    faces, uvs = [], []
    for j in range(na):
        q = [idx[ne - 1, j], idx[ne - 1, (j + 1) % na]]
        if min(q) >= 0:
            faces.append([q[0], q[1], crown])
    for i in range(ne - 1):
        for j in range(na):
            q = [idx[i, j], idx[i, (j + 1) % na], idx[i + 1, (j + 1) % na], idx[i + 1, j]]
            if min(q) >= 0:
                faces.append(q)
                uvs.append([(j / na, i / ne), ((j + 1) / na, i / ne), ((j + 1) / na, (i + 1) / ne), (j / na, (i + 1) / ne)])
    me = bpy.data.meshes.new(cid + "_hair")
    me.from_pydata(verts, [], faces)
    me.update()
    hair = bpy.data.objects.new(cid + "_hair", me)
    bpy.context.scene.collection.objects.link(hair)
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    edge = [v for v in bm.verts if v.is_boundary]                 # the grid's staircase on the open edges
    for _ in range(h.get("edge_smooth", 12)):
        new = {}
        for v in edge:
            nb = [e.other_vert(v) for e in v.link_edges if e.is_boundary]
            if len(nb) == 2:
                new[v] = (nb[0].co + nb[1].co) * 0.25 + v.co * 0.5
        for v, co in new.items():
            v.co = co
    bm.to_mesh(me)
    bm.free()
    bpy.context.view_layer.objects.active = hair
    for md, kw in (("SMOOTH", {"factor": 0.6, "iterations": h.get("smooth", 6)}),):
        m = hair.modifiers.new(md.lower(), md)
        for k, v in kw.items():
            setattr(m, k, v)
        bpy.ops.object.modifier_apply(modifier=m.name)
    R.clean(hair, min_island=40)
    R.decimate(hair, h.get("faces", 2400))
    for p in hair.data.polygons:
        p.use_smooth = True
    bake_onto(hair, high, cid, "hair", h.get("texture", 1024), ext=0.012, dist=0.04)
    m = hair.modifiers.new("solidify", "SOLIDIFY")
    m.thickness, m.offset, m.use_rim = h.get("thickness", 0.012), -1, True
    bpy.ops.object.modifier_apply(modifier=m.name)
    rig_to(hair, arm, {"head": np.ones(len(hair.data.vertices))})
    log("hair shell: %d of %d directions are hair, %d faces" % (ok.sum(), ok.size, len(hair.data.polygons)))
    return hair


def make_skirt(body, high, arm, cid, spec):
    """A skirt ring by ring from the model's outline (rays in from outside at each height), tucked under the body at
    the top, closed with a thin hem; weighted from the pelvis at the waist toward the thighs at the hem."""
    k = spec["clean"]["skirt"]
    pts = R.bone_points(arm)
    top = k.get("top_z", pts["thigh_l"][0].z + 0.04)
    bvh = BVHTree.FromObject(high, bpy.context.evaluated_depsgraph_get())
    bc = R.world_co(body)
    ring = bc[np.abs(bc[:, 2] - top) < 0.01]
    cy = float((ring[:, 1].max() + ring[:, 1].min()) / 2)
    seg = k.get("segments", 48)
    ang = np.linspace(0, 2 * math.pi, seg, endpoint=False)

    def radii(z):
        r = np.full(seg, np.nan)
        for j, a in enumerate(ang):
            d = Vector((math.cos(a), math.sin(a), 0))
            o = Vector((0, cy, z)) + d * 0.6
            hit = bvh.ray_cast(o, -d, 0.6)
            if hit[0] is not None:
                r[j] = 0.6 - hit[3]
        return r
    rows, z = [], top
    prev = None
    stop = k.get("hem_z", 0.1) - 0.015
    while z > stop:
        r = radii(z)
        med = np.nanmedian(r)
        if prev is not None and med < 0.8 * np.nanmedian(prev):     # below the hem: the legs
            break
        rows.append((z, np.where(np.isnan(r), prev if prev is not None else med, r)))
        prev = rows[-1][1]
        z -= 0.01
    hem = rows[-1][0]
    keep = np.linspace(0, len(rows) - 1, k.get("rows", 10)).round().astype(int)
    rows = [rows[i] for i in keep]
    rows[0] = (rows[0][0], rows[0][1] * 0.96)                          # tucked under the body
    verts = [(math.cos(a) * r, cy + math.sin(a) * r, z) for z, rr in rows for a, r in zip(ang, rr)]
    faces = [(i * seg + j, i * seg + (j + 1) % seg, (i + 1) * seg + (j + 1) % seg, (i + 1) * seg + j)
             for i in range(len(rows) - 1) for j in range(seg)]
    me = bpy.data.meshes.new(cid + "_skirt")
    me.from_pydata(verts, [], faces)
    me.update()
    sk = bpy.data.objects.new(cid + "_skirt", me)
    bpy.context.scene.collection.objects.link(sk)
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = True
    bake_onto(sk, high, cid, "skirt", k.get("texture", 1024), ext=0.01, dist=0.03)
    m = sk.modifiers.new("solidify", "SOLIDIFY")
    m.thickness, m.offset = 0.004, -1
    bpy.context.view_layer.objects.active = sk
    bpy.ops.object.modifier_apply(modifier=m.name)
    co = R.world_co(sk)
    t = np.clip((top - co[:, 2]) / max(top - hem, 0.05), 0, 1)
    leg = 0.55 * t
    side = np.clip(co[:, 0] / 0.06, -1, 1)
    rig_to(sk, arm, {"pelvis": 1 - leg, "thigh_l": leg * (1 + side) / 2, "thigh_r": leg * (1 - side) / 2})
    log("skirt: %d rows from %.2f to the hem at %.2f m, %d faces" % (len(rows), top, hem, len(me.polygons)))
    return sk


# ---------------------------------------------------------------------------------------------- preview

def scene_setup():
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    w = bpy.data.worlds.new("w")
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs[0].default_value = (0.55, 0.55, 0.58, 1)
    w.node_tree.nodes["Background"].inputs[1].default_value = 0.8
    sc.world = w
    for name, energy, rot in (("key", 3.0, (50, 0, 30)), ("fill", 1.0, (60, 0, -120))):
        sun = bpy.data.objects.new(name, bpy.data.lights.new(name, "SUN"))
        sun.data.energy = energy
        sun.rotation_euler = tuple(math.radians(a) for a in rot)
        sc.collection.objects.link(sun)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    return sc, cam


def shoot(path, cam, target, dist, yaw, ortho=None, res=(900, 1200), lens=85):
    sc = bpy.context.scene
    sc.render.resolution_x, sc.render.resolution_y = res
    a = math.radians(yaw)
    t = Vector(target)
    cam.location = t + Vector((dist * math.sin(a), -dist * math.cos(a), 0))
    cam.rotation_euler = (math.radians(90), 0, a)
    if ortho:
        cam.data.type, cam.data.ortho_scale = "ORTHO", ortho
    else:
        cam.data.type, cam.data.lens = "PERSP", lens
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)


EXPRESSIONS = {
    "neutral": {},
    "blink": {"eyeBlinkLeft": 1, "eyeBlinkRight": 1},
    "smile": {"mouthSmileLeft": 0.8, "mouthSmileRight": 0.8, "cheekSquintLeft": 0.4, "cheekSquintRight": 0.4},
    "talk": {"jawOpen": 0.3},
    "look": {"eyeLookOutLeft": 0.8, "eyeLookInRight": 0.8},
    "surprise": {"browInnerUp": 0.8, "browOuterUpLeft": 0.7, "browOuterUpRight": 0.7, "eyeWideLeft": 0.6,
                 "eyeWideRight": 0.6, "jawOpen": 0.2},
}


def expressions(body, cam, cid, h):
    kb = body.data.shape_keys.key_blocks
    for name, keys in EXPRESSIONS.items():
        for k in kb:
            k.value = keys.get(k.name, 0.0)
        shoot(os.path.join(BLENDER_RENDERS, "%s_expr_%s.png" % (cid, name)), cam, (0, 0, h * 0.91), 0.75, 15, res=(600, 600))
    for k in kb:
        k.value = 0.0


def posed(arm, cam, cid, h):
    pb = arm.pose.bones

    def turn(name, deg, axis):
        bpy.context.view_layer.update()
        b = pb[name]
        mw = b.matrix.copy()
        head = mw.translation.copy()
        b.matrix = Matrix.Translation(head) @ Matrix.Rotation(math.radians(deg), 4, axis) @ Matrix.Translation(-head) @ mw
    turn("upperarm_l", 70, "Y")
    turn("upperarm_r", -70, "Y")
    turn("lowerarm_l", -30, "Z")
    turn("lowerarm_r", 30, "Z")
    turn("thigh_l", -30, "X")
    turn("calf_l", 40, "X")
    turn("thigh_r", 12, "X")
    turn("head", -15, "Z")
    shoot(os.path.join(BLENDER_RENDERS, "%s_clean_posed.png" % cid), cam, (0, 0, h * 0.5), 6, 30, ortho=h * 1.15)
    for b in pb:
        b.matrix_basis = Matrix()


def export(arm, body, parts, cid):
    """One skinned mesh (body, eyes, lashes, teeth, tongue, hair, skirt as material sections) with the face shapes,
    on the house skeleton: blender/characters/<id>/export/clean_<id>.glb."""
    for p in parts:
        for md in list(p.modifiers):
            p.modifiers.remove(md)
    bpy.ops.object.select_all(action="DESELECT")
    for p in parts:
        p.select_set(True)
    body.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.join()
    body.name = "SK_" + cid.capitalize()
    out = os.path.join(HERE, cid, "export", "clean_%s.glb" % cid)
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    body.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.gltf(filepath=out, use_selection=True, export_format="GLB", export_skins=True,
                              export_morph=True, export_morph_normal=False, export_animations=False,
                              export_def_bones=False, export_yup=True, export_apply=False,
                              export_image_format="AUTO", export_jpeg_quality=90)
    tris = sum(len(p.vertices) - 2 for p in body.data.polygons)
    log("wrote %s: %d verts, %d triangles, %d materials, %d shape keys" % (
        os.path.relpath(out, ROOT), len(body.data.vertices), tris, len(body.data.materials), len(keys_of(body))))


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    cid = [a for a in argv if not a.startswith("--")][0]
    stage = next((a.split("=")[1] for a in argv if a.startswith("--stage=")), "all")
    spec = json.load(open(os.path.join(HERE, cid, "character.json")))
    marks = json.load(open(os.path.join(HERE, cid, "face_marks.json"))) if os.path.exists(os.path.join(HERE, cid, "face_marks.json")) else None
    work = os.path.join(HERE, cid, "export")
    os.makedirs(work, exist_ok=True)
    os.makedirs(BLENDER_RENDERS, exist_ok=True)
    arm, body = build_base(cid, spec)
    t_pose(arm, [body])
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(work, cid + "_clean_base.blend"))
    if stage == "base":
        return
    high = load_high(cid, spec)
    fit(arm, body, high, spec)
    if marks:
        ours = mh_marks(body, arm, cid)
        for e, c in zip(ours["eyes"], eye_centres(body)):
            e["x"], e["z"] = float(c[0]), float(c[2])
        log("ours %s" % ours)
        face_warp(high, body, ours, marks, R.bone_points(arm)["neck_01"][0].z)
    surface_fit(arm, body, high, spec, marks)
    if marks:
        bake_look(body, high, arm, cid, spec, ours, marks)
    hs = spec["clean"].get("hair")
    parts = [] if isinstance(hs, str) and hs != "shell" else [make_hair_shell(body, high, arm, cid, spec) if hs == "shell" else make_hair(body, high, arm, cid, spec)]
    if spec["clean"].get("skirt"):
        parts.append(make_skirt(body, high, arm, cid, spec))
    for m in body.data.materials:
        b = next((n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
        if b is not None and not any(t in m.name for t in ("-poly", "lash")):
            b.inputs["Roughness"].default_value = 0.7
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(work, cid + "_clean_fit.blend"))
    if "--preview" in argv:
        sc, cam = scene_setup()
        h = spec["height"]
        for o, tag in ((high, "hi3d"), (body, "clean")):
            high.hide_render = o is not high
            for b in [body] + parts:
                b.hide_render = o is not body
            shoot(os.path.join(BLENDER_RENDERS, "%s_%s_front.png" % (cid, tag)), cam, (0, 0, h * 0.5), 6, 0, ortho=h * 1.15)
            shoot(os.path.join(BLENDER_RENDERS, "%s_%s_side.png" % (cid, tag)), cam, (0, 0, h * 0.5), 6, 90, ortho=h * 1.15)
            shoot(os.path.join(BLENDER_RENDERS, "%s_%s_face.png" % (cid, tag)), cam, (0, 0, h * 0.91), 0.9, 0, res=(800, 800))
            shoot(os.path.join(BLENDER_RENDERS, "%s_%s_face34.png" % (cid, tag)), cam, (0, 0, h * 0.91), 0.9, 35, res=(800, 800))
        expressions(body, cam, cid, h)
        posed(arm, cam, cid, h)
    high.hide_render = True
    bpy.data.objects.remove(high)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(work, cid + "_clean.blend"))
    export(arm, body, parts, cid)


if __name__ == "__main__":
    main()
