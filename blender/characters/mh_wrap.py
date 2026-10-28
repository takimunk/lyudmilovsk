"""MetaHuman's own face mesh wrapped onto a generated character's head, for a 1:1 likeness
(MetaHuman's fitting stays inside its face space; a template import takes our geometry as it is).

    Blender -b -P blender/characters/mh_wrap.py -- nao [--preview]

In:  blender/characters/metahuman/SKM_Face.fbx (MetaHuman's archetype face, exported from UE by
     unreal/scripts/metahuman_character.py export step), the hi3d model and face_marks.json of <id>.
Out: blender/characters/<id>/export/mh/template_{head,eyeLeft,eyeRight,teeth}.fbx, same vertices and UVs as
     MetaHuman's sections, for MetaHumanCharacterEditorSubsystem.import_from_template (match_vertices_by_u_vs).

How: similarity from the eyes (scale by their distance), the model's depth onto ours; a Gaussian RBF puts our eyes and
mouth corners on the model's; then every face vertex goes along its normal onto the model's surface where that is
near and faces the same way (the lids, the mouth's inside, the scalp under hair and the ears keep MetaHuman's form,
filled in smoothly); eyeballs, teeth and the rest ride along rigidly with the skin round them.
"""
import json
import math
import os
import sys

import bpy
import bmesh
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import rig_character as R  # noqa: E402
import clean_human as C  # noqa: E402

SECTIONS = {0: "head", 1: "teeth", 2: "saliva", 3: "eyeLeft", 4: "eyeRight", 5: "eyeshell", 6: "eyelashes",
            7: "eyeEdge", 8: "cartilage"}
EXPORT = ("head", "eyeLeft", "eyeRight", "teeth")


def log(m):
    print("WRAP " + m, flush=True)


def load_face():
    bpy.ops.import_scene.fbx(filepath=os.path.join(HERE, "metahuman", "SKM_Face.fbx"))
    mesh = next(o for o in bpy.data.objects if o.type == "MESH")
    for o in list(bpy.data.objects):
        if o is not mesh:
            bpy.data.objects.remove(o)
    co = R.world_co(mesh)
    mesh.parent = None
    for md in list(mesh.modifiers):
        mesh.modifiers.remove(md)
    mesh.matrix_world = np.eye(4).tolist()
    R.set_world_co(mesh, co)
    sec = np.full(len(co), -1)
    for p in mesh.data.polygons:
        if p.material_index in SECTIONS:
            sec[list(p.vertices)] = p.material_index
    return mesh, sec


def face_lum(high):
    """Luminance of the model's texture under each face: dark ones are hair, earrings, painted eyes and brows."""
    me = high.data
    img = next(n.image for n in me.materials[0].node_tree.nodes if n.type == "TEX_IMAGE" and n.image)
    w, h = img.size
    px = np.array(img.pixels[:], np.float32).reshape(h, w, 4)
    uv = np.empty(len(me.loops) * 2)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    start = np.empty(len(me.polygons), int)
    me.polygons.foreach_get("loop_start", start)
    u = uv[start]                                              # one corner per face is enough at 600k faces
    c = px[np.clip((u[:, 1] * h).astype(int), 0, h - 1), np.clip((u[:, 0] * w).astype(int), 0, w - 1)]
    return c[:, :3] @ np.array([0.2126, 0.7152, 0.0722], np.float32)


def probe_skin(co, nrm, idx, bvh, rng, lum, dark=0.22):
    t = np.zeros(len(co))
    conf = np.zeros(len(co))
    for i in idx:
        p, d = Vector(co[i]), Vector(nrm[i])
        best = None
        for sign in (1, -1):
            h = bvh.ray_cast(p, d * sign, rng)
            if h[0] is not None and h[1].dot(d) > 0.55 and lum[h[2]] > dark:
                v = (h[0] - p).length * sign
                if best is None or abs(v) < abs(best):
                    best = v
        if best is not None:
            t[i], conf[i] = best, 1.0
    return t, conf


def main():
    argv = sys.argv[sys.argv.index("--") + 1:]
    cid = argv[0]
    spec = json.load(open(os.path.join(HERE, cid, "character.json")))
    marks = json.load(open(os.path.join(HERE, cid, "face_marks.json")))
    out = os.path.join(HERE, cid, "export", "mh")
    os.makedirs(out, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    face, sec = load_face()
    high = R.load_model(os.path.join(HERE, cid, "gen", spec.get("model", "model") + ".glb"), spec["height"])
    R.clean(high)
    R.decimate(high, 600000)
    co = R.world_co(face)
    head = sec == 0

    # 1. similarity from the eyes
    eL, eR = co[sec == 3].mean(0), co[sec == 4].mean(0)
    if eL[0] < eR[0]:
        eL, eR = eR, eL
    te = marks["eyes"]
    tL, tR = (np.array([te[0]["x"], 0, te[0]["z"]]), np.array([te[1]["x"], 0, te[1]["z"]]))
    s = abs(tL[0] - tR[0]) / abs(eL[0] - eR[0])
    mid = (eL + eR) / 2
    tmid = (tL + tR) / 2
    co = (co - mid) * s
    co[:, 0] += tmid[0]
    co[:, 2] += tmid[2]
    co[:, 1] += mid[1] * s - mid[1] * s                      # depth fixed below
    bvh = BVHTree.FromObject(high, bpy.context.evaluated_depsgraph_get())
    front = head & (np.abs(co[:, 0] - tmid[0]) < 0.03) & (np.abs(co[:, 2] - tmid[2]) < 0.05)
    dy = []
    for p in co[front]:
        h = bvh.ray_cast(Vector((p[0], p[1] - 0.3, p[2])), Vector((0, 1, 0)), 0.6)
        if h[0] is not None:
            dy.append(h[0].y - p[1])
    # the front-most of our face points meets the model's front
    dy = np.array(dy)
    shift = float(np.percentile(dy, 50)) if len(dy) else 0.0
    co[:, 1] += shift
    log("similarity: scale %.3f, depth shift %+.3f m (%d probes)" % (s, shift, len(dy)))

    # 2. RBF on the eyes and mouth corners (x, z)
    sal = co[sec == 2]
    mo = marks["mouth"]
    ours = np.array([[co[sec == 3].mean(0)[0], co[sec == 3].mean(0)[2]], [co[sec == 4].mean(0)[0], co[sec == 4].mean(0)[2]],
                     [sal[:, 0].max(), np.median(sal[:, 2])], [sal[:, 0].min(), np.median(sal[:, 2])]])
    if ours[0, 0] < ours[1, 0]:
        ours[[0, 1]] = ours[[1, 0]]
    theirs = np.array([[te[0]["x"], te[0]["z"]], [te[1]["x"], te[1]["z"]],
                       [mo["x"] + mo["w"] / 2, mo["z"]], [mo["x"] - mo["w"] / 2, mo["z"]]])
    d = theirs - ours
    sig = 0.03
    k = lambda p, q: np.exp(-((p[:, None, :] - q[None, :, :]) ** 2).sum(-1) / (2 * sig ** 2))
    d[2:] = 0.0                       # the mouth: the skin fit places the lips (the saliva's ends aren't the corners)
    w = np.linalg.solve(k(ours, ours) + 1e-6 * np.eye(4), d)
    off = k(co[:, [0, 2]], ours) @ w
    frontw = np.clip((tmid[1] + 0.1 - co[:, 1]) / 0.05, 0, 1)
    co[:, 0] += off[:, 0] * frontw
    co[:, 2] += off[:, 1] * frontw
    log("RBF: eyes %s, mouth corners %s m" % (np.round(d[:2], 4).tolist(), np.round(d[2:], 4).tolist()))
    # 2b. the eye openings to the model's painted ones: lid margins (skin lying on the eyeball) moved by a local RBF
    mw = spec.get("mh_wrap", {})
    kw, kh = mw.get("eye_open", (0.92, 0.85))
    for i, tm in ((3, te[0]), (4, te[1])):
        ball = co[sec == i]
        c = ball.mean(0)
        r = float(np.median(np.linalg.norm(ball - c, axis=1)))
        dist = np.linalg.norm(co - c, axis=1)
        rim = np.nonzero(head & (np.abs(dist - r) < 0.003) & (co[:, 1] < c[1] - 0.4 * r))[0]
        if len(rim) < 8:
            continue
        sx = 1 if c[0] > 0 else -1
        pts = {"outer": rim[np.argmax(co[rim, 0] * sx)], "inner": rim[np.argmax(-co[rim, 0] * sx)],
               "top": rim[np.argmax(co[rim, 2])], "bottom": rim[np.argmax(-co[rim, 2])]}
        # widths only: the rim's top/bottom picks include lid skin lying on the ball, so heights would close the eye
        goal = {"outer": (c[0] + sx * tm["w"] / 2 * kw, None), "inner": (c[0] - sx * tm["w"] / 2 * kw, None),
                "top": (None, co[pts["top"], 2]), "bottom": (None, co[pts["bottom"], 2])}
        src = np.array([[co[v, 0], co[v, 2]] for v in pts.values()])
        dd = np.array([[(g[0] - co[v, 0]) if g[0] is not None else 0.0, (g[1] - co[v, 2]) if g[1] is not None else 0.0]
                       for v, g in zip(pts.values(), goal.values())])
        sg = mw.get("eye_sigma", 0.011)
        kk = lambda p, q: np.exp(-((p[:, None, :] - q[None, :, :]) ** 2).sum(-1) / (2 * sg ** 2))
        ww = np.linalg.solve(kk(src, src) + 1e-6 * np.eye(len(src)), dd)
        near = head & (dist < 0.04) & (co[:, 1] < c[1] + 0.005)
        o2 = kk(co[near][:, [0, 2]], src) @ ww
        co[near, 0] += o2[:, 0]
        co[near, 2] += o2[:, 1]
        log("eye %d opening: %s m" % (i, np.round(dd, 4).tolist()))
    R.set_world_co(face, co)

    lum = face_lum(high)
    # 3. the face's skin onto the model's surface: a few passes, each probing nearer and smoothing less
    mw = spec.get("mh_wrap", {})
    nb = C.neighbours(face, head)
    co0 = co.copy()
    for rng, smooth in mw.get("passes", ((0.04, 25), (0.02, 12), (0.01, 6))):
        R.set_world_co(face, co)
        face.data.update()
        nrm = C.vertex_normals(face)
        t, conf = probe_skin(co, nrm, np.nonzero(head)[0], bvh, rng, lum)
        keep = np.zeros(len(co), bool)
        for c in (co[sec == 3].mean(0), co[sec == 4].mean(0)):
            keep |= np.linalg.norm((co - c) * [1, 1.2, 1.6], axis=1) < mw.get("eye_keep", 0.017)
        keep |= nrm[:, 1] > 0.3                                  # the mouth's inside, the back of the head
        keep |= co[:, 2] > tmid[2] + mw.get("brow_keep", 0.016)  # forehead and scalp under the bangs
        ear = (np.abs(co[:, 0] - tmid[0]) > 0.065) & (co[:, 1] > tmid[1] + 0.02)
        keep |= ear                                              # ears behind the hair
        keep |= co[:, 2] < mo["z"] - 0.07                        # the neck under the chin (the model has a collar)
        keep |= np.sqrt(((co[:, 0] - mo["x"]) / mw.get("mouth_rx", 0.028)) ** 2 +
                        ((co[:, 2] - mo["z"]) / mw.get("mouth_rz", 0.013)) ** 2) < 1.0   # the lips keep their form
        conf[keep] = 0.0
        disp = C.diffuse(t[:, None] * nrm, conf, nb, iterations=400, smooth=smooth)
        disp[~head] = 0
        co = co + disp
        log("pass %.3f m: %d of %d on the model, mean |d| %.4f" % (rng, conf.sum(), head.sum(), np.linalg.norm(disp[head], axis=1).mean()))
    disp = co - co0
    for _ in range(mw.get("final_smooth", 10)):              # no folds where neighbours met different surfaces
        d2 = C.diffuse(disp, np.zeros(len(co)), nb, iterations=1, smooth=0)
        disp[head] = 0.5 * disp[head] + 0.5 * d2[head]
    # the lips move as one piece (a field interpolated between nose and chin would pull them apart)
    rr = np.sqrt(((co0[:, 0] - mo["x"]) / mw.get("mouth_rx", 0.028)) ** 2 + ((co0[:, 2] - mo["z"]) / mw.get("mouth_rz", 0.013)) ** 2)
    lipsm, ring = head & (rr < 1.0), head & (rr >= 1.0) & (rr < 1.4) & (nrm[:, 1] < 0)
    if ring.any():
        blend = np.clip((1.2 - rr) / 0.2, 0, 1)[:, None] * head[:, None]
        disp = disp * (1 - blend) + disp[ring].mean(0) * blend
    co = co0 + disp
    # 4. the other sections ride rigidly with the skin round them
    for i, name in SECTIONS.items():
        if i == 0:
            continue
        m = sec == i
        if not m.any():
            continue
        c = co[m].mean(0)
        near = head & (np.linalg.norm(co - c, axis=1) < 0.03)
        co[m] += disp[near].mean(0) if near.any() else 0
    # teeth and saliva stay behind the lips
    lips = head & (np.abs(co[:, 0] - mo["x"]) < 0.018) & (np.abs(co[:, 2] - mo["z"]) < 0.012)
    inner = (sec == 1) | (sec == 2)
    if lips.any() and inner.any():
        need = co[lips, 1].min() + spec.get("mh_wrap", {}).get("teeth_gap", 0.007) - co[inner, 1].min()
        if need > 0:
            co[inner, 1] += need
            log("teeth pushed back %.4f m" % need)
    R.set_world_co(face, co)
    log("skin fit: %d of %d head verts on the model, mean |d| %.4f m" % (conf.sum(), head.sum(), np.linalg.norm(disp[head], axis=1).mean()))

    # 5. export the sections MetaHuman's template import takes, back in its own frame (inverse of the similarity is
    #    not needed: alignment_options rescale and place it)
    for i, name in SECTIONS.items():
        if name not in EXPORT:
            continue
        part = face.copy()
        part.data = face.data.copy()
        bpy.context.scene.collection.objects.link(part)
        bm = bmesh.new()
        bm.from_mesh(part.data)
        bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.material_index != i], context="FACES")
        bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
        bm.to_mesh(part.data)
        bm.free()
        part.name = "template_" + name
        bpy.ops.object.select_all(action="DESELECT")
        part.select_set(True)
        bpy.context.view_layer.objects.active = part
        path = os.path.join(out, "template_%s.fbx" % name)
        bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                                 global_scale=1.0, mesh_smooth_type="FACE", use_mesh_modifiers=False)
        log("wrote %s: %d verts" % (os.path.relpath(path, HERE), len(part.data.vertices)))
        bpy.data.objects.remove(part)

    if "--preview" in argv:
        sc, cam = C.scene_setup()
        h = 0.0
        ez = tmid[2]
        for o, tag in ((high, "hi3d"), (face, "wrap")):
            high.hide_render = o is not high
            face.hide_render = o is not face
            for yaw in (0, 35, 90):
                C.shoot(os.path.join(C.BLENDER_RENDERS, "%s_mhwrap_%s_%d.png" % (cid, tag, yaw)), cam, (0, 0, ez - 0.03), 0.8, yaw,
                        res=(600, 600))
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "mh_wrap.blend"))


if __name__ == "__main__":
    main()
