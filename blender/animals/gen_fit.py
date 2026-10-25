"""A generated animal (hi3d, blender/animals/gen/<Breed>/model.glb) brought into the breed's frame and fitted with the
breed's skeleton (anatomy.py): turned to face +X, scaled to the breed's height, set on the ground, and the joints moved
onto the body it actually has (the paws onto its paws, the legs' columns, the nose, the tail along its tail's own
centre line), so the clips (gaits.py) and the skin weights (the signed-distance primitives round the fitted joints) fit it.
"""
import math

import bmesh
import bpy
import numpy as np


def import_glb(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in new if o.type == "MESH"]
    for o in bpy.context.selected_objects:
        o.select_set(False)
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    ob.parent = None
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for o in new:
        if o != ob and o.name in bpy.data.objects:
            bpy.data.objects.remove(o)
    return ob


def verts(ob):
    a = np.zeros(len(ob.data.vertices) * 3)
    ob.data.vertices.foreach_get("co", a)
    return a.reshape(-1, 3)


def set_verts(ob, P):
    ob.data.vertices.foreach_set("co", P.ravel())
    ob.data.update()


def orient(ob, B):
    """Long axis -> X with the head (the end the highest points are near: the ears) at +X; feet on z = 0; scaled so
    the top of the head is where the breed's ears end; centred between the paws."""
    P = verts(ob)
    lo, hi = P.min(0), P.max(0)
    ext = hi - lo
    up = 2 if ext[2] >= min(ext[0], ext[1]) * 0.5 else 1     # glTF Y-up is already Z-up after import
    hor = [i for i in range(3) if i != up]
    longi = hor[0] if ext[hor[0]] >= ext[hor[1]] else hor[1]
    side = hor[1] if longi == hor[0] else hor[0]
    Q = np.stack([P[:, longi], P[:, side], P[:, up]], 1)
    # the head end: the highest tenth of the points lies toward it
    top = Q[Q[:, 2] > np.quantile(Q[:, 2], 0.97)]
    mid = (Q[:, 0].min() + Q[:, 0].max()) / 2
    if top[:, 0].mean() < mid:
        Q[:, 0] *= -1
        Q[:, 1] *= -1
    # a proper rotation (no mirror): if the axis swap flipped handedness, flip the side axis
    M = np.zeros((3, 3))
    M[0, longi] = 1
    M[1, side] = 1
    M[2, up] = 1
    if np.linalg.det(M) < 0:
        Q[:, 1] *= -1
    Q[:, 2] -= Q[:, 2].min()
    target = float(B["joints"]["ear_end"][2])
    Q *= target / Q[:, 2].max()
    feet = Q[Q[:, 2] < target * 0.06]
    Q[:, 0] -= (feet[:, 0].min() + feet[:, 0].max()) / 2
    Q[:, 1] -= feet[:, 1].mean()
    set_verts(ob, Q)
    return Q


def cluster_paws(Q, H):
    """The four paws: points near the ground split front/hind by x and left/right by y."""
    low = Q[Q[:, 2] < H * 0.07]
    xm = np.median(low[:, 0])
    out = {}
    for name, fx, sy in (("LF", 1, 1), ("RF", 1, -1), ("LH", -1, 1), ("RH", -1, -1)):
        sel = low[((low[:, 0] - xm) * fx > 0) & (low[:, 1] * sy > 0)]
        out[name] = sel.mean(0) if len(sel) else None
    return out


def leg_column(Q, paw, z0, z1, r):
    """Mean x, y of the points within r of the vertical line over the paw, between heights z0 and z1."""
    sel = Q[(Q[:, 2] > z0) & (Q[:, 2] < z1) & (np.hypot(Q[:, 0] - paw[0], Q[:, 1] - paw[1]) < r)]
    return sel[:, :2].mean(0) if len(sel) > 10 else paw[:2]


def fit(B, Q):
    """Moves B's joints onto the generated body Q (N x 3, already oriented). Changes B in place."""
    J = B["joints"]
    H = float(J["chest"][2])
    paws = cluster_paws(Q, H)
    if any(v is None for v in paws.values()):
        raise RuntimeError("could not find four paws")
    nose_x = float(Q[:, 0].max())
    tail_x = float(Q[:, 0].min())
    front_x = (paws["LF"][0] + paws["RF"][0]) / 2
    hind_x = (paws["LH"][0] + paws["RH"][0]) / 2
    # x: piecewise-linear from the breed's anchors (tail end, hind paws, front paws, nose) to the body's
    T0 = np.array(J["tail"])
    a_src = [float(J["toe"][0]), float(J["paw"][0]), float(J["head_end"][0])]
    a_dst = [hind_x, front_x, nose_x]

    def mx(x):
        return float(np.interp(x, a_src, a_dst, left=None, right=None)) if a_src[0] <= x <= a_src[-1] else (
            a_dst[0] + (x - a_src[0]) * (a_dst[1] - a_dst[0]) / (a_src[1] - a_src[0]) if x < a_src[0]
            else a_dst[-1] + (x - a_src[-1]) * (a_dst[-1] - a_dst[-2]) / (a_src[-1] - a_src[-2]))
    # z: the back's height
    back = Q[(Q[:, 0] > hind_x) & (Q[:, 0] < front_x)]
    top_src = max(q[1] for q in B["trunk"])
    zs = float(np.quantile(back[:, 2], 0.98)) / top_src
    # y: the body's width over the chest
    chest = Q[np.abs(Q[:, 0] - front_x) < H * 0.25]
    ys = float(np.quantile(np.abs(chest[:, 1]), 0.97)) / max(q[3] for q in B["trunk"]) / 1.25
    ys = min(max(ys, 0.7), 1.5)
    legs_f = ("upperarm", "forearm", "hand", "paw", "paw_end")
    legs_h = ("shin", "foot", "toe", "toe_end")
    for k, v in list(J.items()):
        if k == "tail":
            continue
        p = np.array(v, float)
        p = np.array([mx(p[0]), p[1] * ys, p[2] * zs])
        J[k] = p
    # legs: each column's x, y from the body itself (the paw, the leg's middle, its top), the joints' heights kept
    for name, side in (("F", "LF"), ("H", "LH")):
        paw = (paws[side] + paws["R" + side[1]] * np.array([1, -1, 1])) / 2
        names = legs_f if name == "F" else legs_h
        lowc = leg_column(Q, paw, H * 0.05, H * 0.3, H * 0.18)
        lowc[1] = abs(lowc[1])
        for k in names:
            p = J[k].copy()
            t = min(max(p[2] / (H * 0.45), 0.0), 1.0)          # the low joints take the column's place fully
            if k not in ("paw_end", "toe_end"):
                p[0] = p[0] * t + lowc[0] * (1 - t)
            p[1] = p[1] * t + lowc[1] * (1 - t)
            J[k] = p
        dx = paw[0] - J[names[-2]][0]
        J[names[-2]] = np.array([paw[0], abs(paw[1]), J[names[-2]][2]])
        J[names[-1]] = J[names[-1]] + np.array([dx, 0, 0])
        J[names[-1]][1] = abs(paw[1])
    # the tail: along its own centre line, from where it leaves the rump
    base = np.array([mx(float(T0[0][0])), 0.0, float(T0[0][2]) * zs])
    # the tail's points: behind the rump and near the middle plane (the hind legs are off to the sides)
    leg_y = abs(float(J["thigh"][1]))
    tailpts = Q[(Q[:, 0] < base[0] + H * 0.02) & (np.abs(Q[:, 1]) < leg_y * 0.45)]
    n = len(T0)
    if len(tailpts) > 50:
        d = np.linalg.norm(tailpts - base, axis=1)
        L = float(np.quantile(d, 0.995))
        pts = [base]
        for i in range(1, n):
            r = L * i / (n - 1)
            ring = tailpts[np.abs(d - r) < L / (n - 1) * 0.6]
            pts.append(ring.mean(0) if len(ring) > 5 else pts[-1] + (pts[-1] - pts[-2] if len(pts) > 1 else np.array([-L / n, 0, 0])))
        J["tail"] = [np.array(p) for p in pts]
    else:
        J["tail"] = [np.array([mx(p[0]), p[1], p[2] * zs]) for p in T0]
    for k in ("eye",):
        e = np.array(B[k], float)
        B[k] = np.array([mx(e[0]), e[1] * ys, e[2] * zs])
    B["trunk_f"] = tuple(mx(f) for f in B["trunk_f"])
    B["trunk"] = [(q[0], q[1] * zs, q[2] * zs, q[3] * ys) for q in B["trunk"]]
    return dict(paws={k: [float(x) for x in v] for k, v in paws.items()}, zs=zs, ys=ys, nose_x=nose_x, tail_x=tail_x)


def clean(ob, weld=0.0005):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=weld)
    # loose specks: islands with few faces
    bm.faces.ensure_lookup_table()
    seen = set()
    small = []
    for f in bm.faces:
        if f.index in seen:
            continue
        stack, island = [f], []
        seen.add(f.index)
        while stack:
            g = stack.pop()
            island.append(g)
            for e in g.edges:
                for h in e.link_faces:
                    if h.index not in seen:
                        seen.add(h.index)
                        stack.append(h)
        if len(island) < 200:
            small.extend(island)
    if small:
        bmesh.ops.delete(bm, geom=small, context="FACES")
    # fins (edges of more than two faces) and the small holes round them stop the decimation early: cut them, fill
    bad = [e for e in bm.edges if len(e.link_faces) > 2]
    bmesh.ops.delete(bm, geom=list({f for e in bad for f in e.link_faces}), context="FACES")
    bmesh.ops.holes_fill(bm, edges=[e for e in bm.edges if e.is_boundary], sides=64)
    bmesh.ops.dissolve_degenerate(bm, edges=bm.edges, dist=1e-6)
    bm.to_mesh(ob.data)
    bm.free()


def base_color_image(ob):
    """The generated material's base colour (and normal, if any) images."""
    for slot in ob.material_slots:
        m = slot.material
        if not m or not m.use_nodes:
            continue
        p = next((n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
        if not p:
            continue
        img = None
        for link in m.node_tree.links:
            if link.to_node == p and link.to_socket.name == "Base Color" and link.from_node.type == "TEX_IMAGE":
                img = link.from_node.image
        if img:
            return img
    return None
