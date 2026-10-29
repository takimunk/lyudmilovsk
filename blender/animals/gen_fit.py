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


def import_ue(obj_path, tex_path, keep="Body"):
    """A breed's body exported back out of Unreal (unreal/scripts/export_animal_source.py, for when its raw model.glb is
    gone): only the faces of the `keep` section (not the fur shells), centimetres to metres, Unreal's left-handed frame
    to Blender's (y negated; the importer's own winding then faces out), and a plain material with the generated coat, the way the
    generated glb came in."""
    before = set(bpy.data.objects)
    bpy.ops.wm.obj_import(filepath=obj_path, forward_axis="Y", up_axis="Z")
    new = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
    for o in bpy.context.selected_objects:
        o.select_set(False)
    for o in new:
        o.select_set(True)
    bpy.context.view_layer.objects.active = new[0]
    if len(new) > 1:
        bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    names = [m.name.split(".")[0] if m else "" for m in ob.data.materials]
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if names[f.material_index] != keep], context="FACES")
    for v in bm.verts:
        v.co = (v.co.x * 0.01, -v.co.y * 0.01, v.co.z * 0.01)
    bm.to_mesh(ob.data)
    bm.free()
    ob.data.materials.clear()
    m = bpy.data.materials.new("UESource")
    m.use_nodes = True
    p = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    t = m.node_tree.nodes.new("ShaderNodeTexImage")
    t.image = bpy.data.images.load(tex_path)
    t.image.colorspace_settings.name = "sRGB"
    m.node_tree.links.new(t.outputs["Color"], p.inputs["Base Color"])
    ob.data.materials.append(m)
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


def fit_head(B, Q, spec):
    """The head bones onto the head the body really has. fit() moves them with the back's height, and a generated dog
    that holds its head lower than the template (Laika: ~10 cm) kept its jaw pivot and mouth line in the skull, so the
    jaw dragged the whole face. spec (gen/<Breed>/head.json): the real mouth line, by hand off a side close-up. The
    template's mouth corner and nose tip go onto the real corner and the body's own nose tip by one turn-and-scale in
    the side plane; the head's sizes follow the scale, the upper neck half the move."""
    J = B["joints"]
    nose_x = float(Q[:, 0].max())
    tip = Q[(Q[:, 0] > nose_x - 0.008) & (np.abs(Q[:, 1]) < 0.025)]
    tip_r = np.array([nose_x, float(tip[:, 2].mean())])
    corner_r = np.array(spec["mouth"][0], float)
    # the template's nose tip and mouth corner in the head's own frame (measured on the three dog templates' sculpts:
    # the same shares of the head's length for all of them)
    h0, h1 = np.array([J["head"][0], J["head"][2]]), np.array([J["head_end"][0], J["head_end"][2]])
    L = float(np.linalg.norm(h1 - h0))
    ax = (h1 - h0) / L
    upv = np.array([-ax[1], ax[0]])
    tip_f = h1 + L * (0.09 * ax + 0.07 * upv)
    corner_f = h0 + L * (0.49 * ax - 0.15 * upv)
    a, b = tip_f - corner_f, tip_r - corner_r
    s = float(np.linalg.norm(b) / np.linalg.norm(a))
    ang = math.atan2(b[1], b[0]) - math.atan2(a[1], a[0])
    R = np.array([[math.cos(ang), -math.sin(ang)], [math.sin(ang), math.cos(ang)]]) * s

    def tf(p):
        p = np.array(p, float)
        xz = R @ (np.array([p[0], p[2]]) - corner_f) + corner_r
        return np.array([xz[0], p[1] * s, xz[1]])
    head0 = J["head"].copy()
    for k in ("head", "head_end", "jaw", "jaw_end", "ear", "ear_end"):
        J[k] = tf(J[k])
    J["neck_02"] = J["neck_02"] + (J["head"] - head0) * 0.5
    B["eye"] = tf(B["eye"])
    for k in ("head_r", "muzzle", "nose"):
        B[k] = tuple(r * s for r in B[k])
    B["muzzle_len"] *= s
    B["eye_r"] *= s ** 0.7
    B["ear"] = dict(B["ear"], w=B["ear"]["w"] * s)
    B["head_fit"] = spec
    return dict(head_scale=s, head_turn_deg=math.degrees(ang), head_move=[float(x) for x in J["head"] - head0], nose=tip_r.tolist())


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


def find_eyes(ob, img, B, size=1024):
    """Where the generated body's own eyes are (the texture's dark irises and pupils on the face, left and right of the
    middle, between the brow and the muzzle): their centres on the skin, radius and outward normals. The breed's
    template eye place only guesses; on a generated head it can be centimetres off, and then the fur grows over the
    real eyes (they look shut) and the eyeballs sit inside the head. None if they can't be told."""
    me = ob.data
    P = verts(ob)
    N = np.zeros(len(me.vertices) * 3)
    me.vertices.foreach_get("normal", N)
    N = N.reshape(-1, 3)
    uv = np.zeros(len(me.loops) * 2)
    me.uv_layers.active.data.foreach_get("uv", uv)
    vi = np.zeros(len(me.loops), dtype=np.int64)
    me.loops.foreach_get("vertex_index", vi)
    UV = np.zeros((len(me.vertices), 2))
    UV[vi] = uv.reshape(-1, 2)
    im = img.copy()
    im.scale(size, size)
    px = np.array(im.pixels[:]).reshape(size, size, 4)[..., :3]
    bpy.data.images.remove(im)
    iy = np.clip((UV[:, 1] % 1.0) * (size - 1), 0, size - 1).astype(int)
    ix = np.clip((UV[:, 0] % 1.0) * (size - 1), 0, size - 1).astype(int)
    lum = px[iy, ix] @ np.array([0.2126, 0.7152, 0.0722])
    J = B["joints"]
    head, tip = np.array(J["head"], float), np.array(J["head_end"], float)
    L = float(np.linalg.norm(tip - head))
    # the face between the ears' front and the start of the muzzle, above the mouth line, off the middle
    face = ((P[:, 0] > head[0] - L * 0.1) & (P[:, 0] < head[0] + L * 0.62) & (P[:, 2] > tip[2] + L * 0.05)
            & (P[:, 2] < head[2] + L * 0.55) & (np.abs(P[:, 1]) > L * 0.06) & (N[:, 0] > -0.2))
    if face.sum() < 200:
        return None
    out = []
    for k in (1.0, -1.0):
        side = face & (P[:, 1] * k > 0)
        thr = min(float(np.quantile(lum[side], 0.02)) * 1.8, 0.12)
        dark = side & (lum < thr)
        if dark.sum() < 30:
            return None
        Pd = P[dark]
        c = np.median(Pd, 0)
        for _ in range(3):
            near = np.linalg.norm(Pd - c, axis=1) < L * 0.12
            c = Pd[near].mean(0)
        d = np.linalg.norm(Pd[near] - c, axis=1)
        r = float(np.clip(np.quantile(d, 0.9) * 1.15, L * 0.035, L * 0.12))
        around = np.linalg.norm(P - c, axis=1) < r
        n = N[around].mean(0)
        n = n / max(np.linalg.norm(n), 1e-6)
        out.append((c, r, n, int(dark.sum())))
    return out
