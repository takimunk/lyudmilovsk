"""Garment adjustments on top of fitted MPFB clothes, done in rest pose before export.

loosen(): re-sets each garment vertex's distance from the body surface from a per-vertex rule
(bone-weight regions and height), then smooths the change, so a fitted women's hoodie becomes an
oversized men's hoodie, slim jeans become wide jeans, and so on. Garments keep their UVs, textures
and interpolated skin weights.
"""
import bmesh
import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree


def body_bvh(basemesh):
    """BVH of the body surface in rest pose (helper geometry hidden, delete-masks off)."""
    saved = []
    for m in basemesh.modifiers:
        saved.append((m, m.show_viewport))
        if m.type == "MASK" and m.name != "Hide helpers":
            m.show_viewport = False
        if m.type == "ARMATURE":
            m.show_viewport = False
    dg = bpy.context.evaluated_depsgraph_get()
    dg.update()
    ev = basemesh.evaluated_get(dg)
    me = ev.to_mesh()
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.transform(basemesh.matrix_world)
    bvh = BVHTree.FromBMesh(bm)
    bm.free()
    ev.to_mesh_clear()
    for m, v in saved:
        m.show_viewport = v
    return bvh


def group_weights(obj, names):
    """Sum of the weights of the given vertex groups per vertex (numpy array)."""
    idx = {g.index for g in obj.vertex_groups if g.name in names}
    w = np.zeros(len(obj.data.vertices))
    if not idx:
        return w
    for v in obj.data.vertices:
        for g in v.groups:
            if g.group in idx:
                w[v.index] += g.weight
    return np.clip(w, 0, 1)


def _neighbours(me):
    nb = [[] for _ in me.vertices]
    for e in me.edges:
        a, b = e.vertices
        nb[a].append(b)
        nb[b].append(a)
    return nb


def loosen(garment, basemesh, rule, blend=None, smooth=8, keep_inside=0.004):
    """rule(co, weights) -> wanted distance from the body (metres) for each vertex (vectorised:
    co is Nx3 world coords, weights a dict of group-name -> N array). blend (N array 0..1, optional)
    says how much of the rule to apply per vertex (0 keeps the vertex where it is)."""
    bvh = body_bvh(basemesh)
    me = garment.data
    mw = garment.matrix_world
    n = len(me.vertices)
    co = np.array([tuple(mw @ v.co) for v in me.vertices])
    names = {g.name for g in garment.vertex_groups}
    weights = {nm: group_weights(garment, [nm]) for nm in names}
    want = np.asarray(rule(co, weights), dtype=float)
    if blend is None:
        blend = np.ones(n)
    nb = _neighbours(me)
    signed = np.zeros(n)
    nrms = np.zeros((n, 3))
    for i in range(n):
        loc, nrm, _, dist = bvh.find_nearest(Vector(co[i]))
        if loc is None:
            continue
        d = Vector(co[i]) - loc
        signed[i] = d.length if d.dot(nrm) >= 0 else -d.length
        nrms[i] = tuple(nrm)
    # distance of the garment "around" each vertex: layered parts (lining, zipper flaps, pockets)
    # move together by the same amount instead of being flattened onto one surface
    ref = signed.copy()
    for _ in range(6):
        ref = 0.5 * ref + 0.5 * np.array([ref[nb[i]].mean() if nb[i] else ref[i] for i in range(n)])
    delta = np.maximum(0.0, np.maximum(want, keep_inside) - ref) * blend
    disp = nrms * delta[:, None]
    for _ in range(smooth):
        nd = disp.copy()
        for i in range(n):
            if nb[i]:
                nd[i] = 0.5 * disp[i] + 0.5 * disp[nb[i]].mean(axis=0)
        disp = nd
    inv = mw.inverted()
    for i, v in enumerate(me.vertices):
        v.co = inv @ Vector(co[i] + disp[i])
    me.update()
    if me.shape_keys:                     # keep the basis key in sync (garments rarely have keys)
        for kb in me.shape_keys.key_blocks:
            for i, d in enumerate(kb.data):
                d.co = me.vertices[i].co


def tint_image(img, rgb, strength=1.0, contrast=1.0):
    """Recolours a texture: keeps its luminance detail, replaces its hue with rgb."""
    px = np.array(img.pixels[:]).reshape(-1, 4)
    lum = px[:, :3] @ np.array([0.2126, 0.7152, 0.0722])
    m = lum.mean() if lum.mean() > 1e-4 else 1.0
    rel = np.clip(((lum / m) - 1.0) * contrast + 1.0, 0, 3)
    col = np.outer(rel, rgb)
    px[:, :3] = px[:, :3] * (1 - strength) + np.clip(col, 0, 1) * strength
    img.pixels[:] = px.ravel()
    img.update()


def edit_textures(obj, fn, name):
    """Runs fn(pixels Nx4 array, width, height) on a copy of each colour texture of obj."""
    done = {}
    for mat in obj.data.materials:
        if not mat or not mat.use_nodes:
            continue
        for node in mat.node_tree.nodes:
            if node.type != "TEX_IMAGE" or not node.image:
                continue
            if node.image.colorspace_settings.name != "sRGB":     # normal / roughness maps
                continue
            src = node.image
            key = src.filepath or src.name
            if key not in done:
                w, h = src.size
                px = np.array(src.pixels[:]).reshape(-1, 4)
                fn(px, w, h)
                img = bpy.data.images.new(name, w, h, alpha=True)   # new image: no file path,
                img.pixels[:] = px.ravel()                         # so exporters use these pixels
                img.pack()
                done[key] = img
            node.image = done[key]


def tint_object(obj, rgb, strength=1.0, contrast=1.0, name=None, lo=0.4, hi=1.4):
    """Tints every colour texture of obj (keeps luminance detail, replaces the hue)."""
    def fn(px, w, h):
        lum = px[:, :3] @ np.array([0.2126, 0.7152, 0.0722])
        m = lum[px[:, 3] > 0.5].mean() if (px[:, 3] > 0.5).any() else max(lum.mean(), 1e-4)
        rel = np.clip(((lum / max(m, 1e-4)) - 1.0) * contrast + 1.0, lo, hi)
        px[:, :3] = px[:, :3] * (1 - strength) + np.clip(np.outer(rel, rgb), 0, 1) * strength
    edit_textures(obj, fn, (name or obj.name) + "_tint")


def chroma(px):
    return px[:, :3].max(axis=1) - px[:, :3].min(axis=1)


def recolour_saturated(obj, rgb, min_chroma=0.12, name=None):
    """Recolours only the saturated pixels (an iris, a tie pattern) keeping their luminance."""
    def fn(px, w, h):
        c = chroma(px)
        mask = np.clip((c - min_chroma) / 0.08, 0, 1)
        lum = px[:, :3] @ np.array([0.2126, 0.7152, 0.0722])
        m = lum[mask > 0.5].mean() if (mask > 0.5).any() else 0.3
        col = np.clip(np.outer(lum / max(m, 1e-4), rgb), 0, 1)
        px[:, :3] = px[:, :3] * (1 - mask[:, None]) + col * mask[:, None]
    edit_textures(obj, fn, (name or obj.name) + "_recol")


def loose_parts(obj):
    """Lists of polygon indices, one per connected part of obj's mesh."""
    me = obj.data
    parent = list(range(len(me.vertices)))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a
    for e in me.edges:
        a, b = find(e.vertices[0]), find(e.vertices[1])
        if a != b:
            parent[a] = b
    parts = {}
    for p in me.polygons:
        parts.setdefault(find(p.vertices[0]), []).append(p.index)
    return list(parts.values())


def split_tie(obj, new_name):
    """The tie of a shirt+tie garment is its own loose part: narrow in x, long in z, at the
    front centre. Gives it its own material (copy of the shirt's) and returns that material."""
    me = obj.data
    mw = obj.matrix_world
    best, best_w = None, 1e9
    for part in loose_parts(obj):
        vs = {v for pi in part for v in me.polygons[pi].vertices}
        pts = [mw @ me.vertices[v].co for v in vs]
        xs = [p.x for p in pts]
        zs = [p.z for p in pts]
        w, hgt = max(xs) - min(xs), max(zs) - min(zs)
        if hgt > 0.25 and abs(sum(xs) / len(xs)) < 0.03 and w < best_w:
            best, best_w = part, w
    mat = me.materials[0].copy()
    mat.name = new_name
    me.materials.append(mat)
    for pi in best or []:
        me.polygons[pi].material_index = len(me.materials) - 1
    return mat


def opaque(obj):
    """Garments render opaque (their texture alpha holds holes and wear marks)."""
    for mat in obj.data.materials:
        if mat and mat.use_nodes:
            for n in mat.node_tree.nodes:
                if n.type == "BSDF_PRINCIPLED":
                    for l in list(n.inputs["Alpha"].links):
                        mat.node_tree.links.remove(l)
                    n.inputs["Alpha"].default_value = 1.0


def split_by_texture_region(obj, region_fn, new_name):
    """Gives the faces whose UV centroid satisfies region_fn(u, v) their own material (a copy)."""
    me = obj.data
    uv = me.uv_layers.active.data
    base = me.materials[0]
    mat = base.copy()
    mat.name = new_name
    me.materials.append(mat)
    idx = len(me.materials) - 1
    for p in me.polygons:
        us = [uv[li].uv for li in p.loop_indices]
        u = sum(x[0] for x in us) / len(us)
        v = sum(x[1] for x in us) / len(us)
        if region_fn(u, v):
            p.material_index = idx
    return mat


def colourful_faces(obj, min_chroma=0.06, radius=6):
    """region_fn for split_by_texture_region: True where obj's first texture is colourful around
    the UV point (the tie pattern on a MakeHuman shirt+tie texture)."""
    for n in obj.data.materials[0].node_tree.nodes:
        if n.type == "TEX_IMAGE" and n.image:
            w, h = n.image.size
            px = np.array(n.image.pixels[:]).reshape(h, w, 4)
            c = (px[..., :3].max(axis=2) - px[..., :3].min(axis=2)) > min_chroma

            def fn(u, v):
                x, y = int(u % 1.0 * w), int(v % 1.0 * h)
                win = c[max(0, y - radius):y + radius + 1, max(0, x - radius):x + radius + 1]
                return win.mean() > 0.3 if win.size else False
            return fn
    return lambda u, v: False


def colourful_region_bbox(obj, min_chroma=0.06):
    """UV bounding box of the colourful pixels of obj's first texture (the tie on a shirt+tie)."""
    for n in obj.data.materials[0].node_tree.nodes:
        if n.type == "TEX_IMAGE" and n.image:
            w, h = n.image.size
            px = np.array(n.image.pixels[:]).reshape(h, w, 4)
            c = px[..., :3].max(axis=2) - px[..., :3].min(axis=2)
            ys, xs = np.nonzero(c > min_chroma)
            lo_u, hi_u = np.percentile(xs, [1, 99]) / w
            lo_v, hi_v = np.percentile(ys, [1, 99]) / h
            return lo_u - 0.01, hi_u + 0.01, lo_v - 0.01, hi_v + 0.01
    return None


def flat_fabric(mat, rgb, name, size=512, noise=0.05, stripes=None, seed=1):
    """Replaces mat's colour textures with a generated fabric texture (flat colour, fine noise,
    optional diagonal stripes (period px, width px, rgb)) keeping alpha opaque."""
    rng = np.random.default_rng(seed)
    n = rng.normal(0, noise, (size, size))
    n = (n + np.roll(n, 1, 0) + np.roll(n, 1, 1)) / 3
    col = np.ones((size, size, 3)) * np.array(rgb)
    if stripes:
        period, width, srgb = stripes
        yy, xx = np.mgrid[0:size, 0:size]
        st = ((xx + yy) % period < width)[..., None]
        col = np.where(st, np.array(srgb), col)
    col = np.clip(col * (1 + n[..., None]), 0, 1)
    img = bpy.data.images.new(name, size, size, alpha=True)
    img.pixels[:] = np.concatenate([col, np.ones((size, size, 1))], axis=2).ravel()
    img.pack()
    for node in mat.node_tree.nodes:
        if node.type == "TEX_IMAGE":
            node.image = img
    return img


def set_roughness(obj, rough, sheen=0.0):
    for mat in obj.data.materials:
        if mat and mat.use_nodes:
            for n in mat.node_tree.nodes:
                if n.type == "BSDF_PRINCIPLED":
                    n.inputs["Roughness"].default_value = rough
                    if sheen:
                        n.inputs["Sheen Weight"].default_value = sheen


def drop_normal_maps(obj):
    for mat in obj.data.materials:
        if mat and mat.use_nodes:
            nt = mat.node_tree
            for n in list(nt.nodes):
                if n.type == "NORMAL_MAP":
                    for l in list(n.outputs[0].links):
                        nt.links.remove(l)


def compress_below(obj, z_from_top=0.09, factor=0.3):
    """Shortens a hanging garment/beard: everything lower than (top - z_from_top) is squeezed up
    toward that level by `factor` (0.3 = keeps 30% of the length). Applies to every shape key."""
    me = obj.data
    mw = obj.matrix_world
    inv = mw.inverted()
    zs = [(mw @ v.co).z for v in me.vertices]
    cut = max(zs) - z_from_top
    delta = {}
    for v, z in zip(me.vertices, zs):
        if z < cut:
            w = mw @ v.co
            w.z = cut + (z - cut) * factor
            delta[v.index] = (inv @ w) - v.co
    for i, d in delta.items():
        me.vertices[i].co += d
    if me.shape_keys:
        for kb in me.shape_keys.key_blocks:
            for i, d in delta.items():
                kb.data[i].co += d
    me.update()
