"""SK_FeralPigeon_Hero: the player's feral pigeon, made from a generated model (hi3d) and scanned feathers (Higgsfield),
on the species' own skeleton (SKEL_FeralPigeon, its clips, ABirdActor). See gen.json for every generation.

    Blender -b --python blender/birds/hero_pigeon/build_hero_pigeon.py [-- --previews] [--no-bake]

Needs: gen/hi3d_pigeon.glb (the raw model), gen/hi3d_dec.blend (decimate.py), gen/cut/ (cut_feathers.py) and
blender/birds/export/FeralPigeon/FeralPigeon_Hero.glb (build_bird.py FeralPigeon --hero: the rig in its bind pose, the
legs, and the old body to take skin weights from).

What it does:
  * body: the generated mesh put into the bind pose (fit.py: frame, size, neck lifted to the eye sockets), its wings
    bent onto the skeleton's wing (leading edge and height per span station) and cut back to the coverts, its tail cut
    back to the tail coverts, its legs cut off; reduced to ~30k triangles; skin weights from the old body (nearest face).
    Albedo and roughness from the generated textures (4096 / 2048), a normal map baked from the 2M-triangle original.
  * flight feathers: one two-sided alpha card per feather bone (p1-p10, s1-s17, tf.*), cut from the scans, packed in
    a 4096 atlas; each card on its own bone, stacked like a real wing (inner over outer, secondaries over primaries).
  * legs: the old body's scaled legs and toes (M_Bare), as Dima asked (generated legs are not used).
  * material slots: Body, Skull (the head feathers without the bill: hidden in first person), Feathers, Bare.
Output (blender/birds/export/FeralPigeon/): FeralPigeon_Hero.glb, T_FeralPigeon_Hero_{Albedo,Normal,Params}.png,
T_FeralPigeon_HeroFeathers_{Albedo,Normal,Params}.png, previews/hero_*.png. make_morphs.py makes the colour morphs.
"""
import json
import math
import os
import sys

import bpy
import bmesh
import mathutils
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import fit  # noqa: E402

ARGV = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
GEN = os.path.join(HERE, "gen")
OUT = os.path.join(HERE, "..", "export", "FeralPigeon")
REF = os.path.join(OUT, "FeralPigeon_Hero.glb")
REF_COPY = os.path.join(GEN, "FeralPigeon_Hero_template.glb")   # the template rig/body, kept: OUT's copy is overwritten
FIT = dict(fit.FIT, neck_deg=-75.0, neck_pivot=(0.055, 0.085), neck_blend=0.05, head_shift=(0.0134, -0.0629))
BODY_TRIS = 30000
TEX = 4096
LEG_BONES = ("thigh", "shin", "tarsus", "toes", "hallux")


# ====================================================================== the rig (template)
def load_template():
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=REF_COPY)
    new = [o for o in bpy.data.objects if o not in before]
    arm = next(o for o in new if o.type == "ARMATURE")
    old = next(o for o in new if o.type == "MESH" and o.name.startswith("SK_"))
    for o in new:
        if o.type == "MESH" and o is not old:
            bpy.data.objects.remove(o)
    old.data.transform(old.matrix_world)
    old.matrix_world.identity()
    return arm, old


def bone_ws(arm, name):
    b = arm.data.bones[name]
    return np.array(arm.matrix_world @ b.head_local), np.array(arm.matrix_world @ b.tail_local)


def coords(ob):
    co = np.empty(len(ob.data.vertices) * 3)
    ob.data.vertices.foreach_get("co", co)
    return co.reshape(-1, 3)


def dominant_groups(ob):
    names = {g.index: g.name for g in ob.vertex_groups}
    out = []
    for v in ob.data.vertices:
        out.append(names[max(v.groups, key=lambda g: g.weight).group] if len(v.groups) else "")
    return np.array(out)


def wing_profile(old):
    """Per span station y (metres from the midline): the template wing's leading edge x and height z, from its flight
    feather and covert vertices."""
    co = coords(old)
    g = dominant_groups(old)
    wingish = np.array([n.split(".")[0] in ("humerus", "forearm", "hand", "primaries") or (n[:1] in "ps" and n[1:2].isdigit()) for n in g])
    ys = np.arange(0.03, 0.345, 0.01)
    le, z = [], []
    for y in ys:
        s = co[wingish & (np.abs(np.abs(co[:, 1]) - y) < 0.006)]
        le.append(s[:, 0].max() if len(s) else np.nan)
        z.append(np.percentile(s[:, 2], 80) if len(s) else np.nan)
    le, z = np.array(le), np.array(z)
    ok = ~np.isnan(le)
    le = np.interp(ys, ys[ok], le[ok])
    z = np.interp(ys, ys[ok], z[ok])
    return ys, le, z


# ====================================================================== the body
def bend_wings(p, prof_rig, prof_hi):
    """The generated wings (straight, high, ahead) onto the skeleton's (swept, level): every point of a wing moves by
    the difference of the two leading edges and heights at its span station, fading in from the body's side."""
    ys, le_r, z_r = prof_rig
    _, le_h, z_h = prof_hi
    ay = np.abs(p[:, 1])
    w = np.clip((ay - 0.03) / 0.07, 0, 1)
    w = w * w * (3 - 2 * w)
    dx = np.interp(ay, ys, le_r - le_h)
    dz = np.interp(ay, ys, z_r - z_h) + 0.003      # the coverts lie a little over the flight feathers
    if os.environ.get("NOBEND"):
        w = w * 0
    q = p.copy()
    q[:, 0] += dx * w
    q[:, 2] += dz * w
    return q


def hi_profile(co):
    ys = np.arange(0.03, 0.345, 0.01)
    le, z = [], []
    for y in ys:
        s = co[(np.abs(np.abs(co[:, 1]) - y) < 0.006) & (co[:, 0] > -0.06)]
        le.append(s[:, 0].max() if len(s) else np.nan)
        z.append(np.median(s[:, 2]) if len(s) else np.nan)
    le, z = np.array(le), np.array(z)
    ok = ~np.isnan(le)
    return ys, np.interp(ys, ys[ok], le[ok]), np.interp(ys, ys[ok], z[ok])


def place(co, prof_rig, prof_hi=None):
    p = fit.fit_points(co, FIT)
    prof_hi = prof_hi or hi_profile(p)
    return bend_wings(p, prof_rig, prof_hi), prof_hi


def covert_depth(ay):
    """How far back from the leading edge the generated wing is kept (the coverts), by span station; 0 = all cut."""
    return np.interp(ay, [0.0, 0.045, 0.058], [1.0, 1.0, 0.0])


def cut_body(ob, prof_rig):
    """Keeps the generated body without its flight feathers (wings beyond the coverts, the tail beyond its coverts)
    and without its legs."""
    co = coords(ob)
    ys, le_r, _ = prof_rig
    ay = np.abs(co[:, 1])
    le = np.interp(ay, ys, le_r)
    wing = ay > 0.045
    cut = wing & ((le - co[:, 0]) > covert_depth(ay))
    cut |= ay > 0.26
    # the tail: the coverts reach about a third down the tail
    cut |= (co[:, 0] < -0.088) & (ay < 0.09)
    # the legs: everything hanging below the belly line behind the breast
    cut |= (co[:, 2] < 0.024) & (ay < 0.05) & (co[:, 0] < 0.04)
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[bm.verts[i] for i in np.nonzero(cut)[0]], context="VERTS")
    # loose bits left by the cut
    islands = []
    seen = set()
    for f in bm.faces:
        if f.index in seen:
            continue
        stack, isl = [f], []
        seen.add(f.index)
        while stack:
            g = stack.pop()
            isl.append(g)
            for e in g.edges:
                for h in e.link_faces:
                    if h.index not in seen:
                        seen.add(h.index)
                        stack.append(h)
        islands.append(isl)
    bm.faces.index_update()
    islands.sort(key=len, reverse=True)
    small = [f for isl in islands[1:] if len(isl) < 400 for f in isl]
    bmesh.ops.delete(bm, geom=small, context="FACES")
    bm.to_mesh(ob.data)
    bm.free()
    c2 = coords(ob)
    print("BODY span after cut", np.abs(c2[:, 1]).max().round(3), "cut verts", int(cut.sum()), "of", len(cut))
    print("BODY after cut", len(ob.data.polygons), "faces,", len(islands), "islands")


def decimate(ob, tris):
    bpy.context.view_layer.objects.active = ob
    m = ob.modifiers.new("dec", "DECIMATE")
    m.ratio = min(1.0, tris / max(1, len(ob.data.polygons)))
    m.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier="dec")
    # smooth normals (the import's custom split normals do not survive the reduction)
    with bpy.context.temp_override(object=ob, active_object=ob, selected_objects=[ob], selected_editable_objects=[ob]):
        bpy.ops.mesh.customdata_custom_splitnormals_clear()
        bpy.ops.object.shade_smooth()
    print("BODY", len(ob.data.polygons), "tris")


def resized(img, size, name, path):
    out = img.copy()
    out.name = name
    out.scale(size, size)
    out.filepath_raw = path
    out.file_format = "PNG"
    out.save()
    return out


def body_textures(ob):
    """Albedo 4096 (sRGB), Params 2048 (R roughness, G gloss: as M_BirdPlumage reads it) from the generated maps."""
    nodes = ob.data.materials[0].node_tree.nodes
    alb = next(n.image for n in nodes if n.type == "TEX_IMAGE" and n.outputs[0].links[0].to_socket.name == "Base Color")
    mr = next(n.image for n in nodes if n.type == "TEX_IMAGE" and n.image is not alb)
    resized(alb, TEX, "T_FeralPigeon_Hero_Albedo", os.path.join(OUT, "T_FeralPigeon_Hero_Albedo.png"))
    small = mr.copy()
    small.scale(2048, 2048)
    px = np.empty(2048 * 2048 * 4, np.float32)
    small.pixels.foreach_get(px)
    px = px.reshape(-1, 4)
    rough = px[:, 1]
    par = np.stack([rough, np.clip(1.0 - rough, 0, 1) * 0.35, np.zeros_like(rough), np.ones_like(rough)], 1)
    im = bpy.data.images.new("T_FeralPigeon_Hero_Params", 2048, 2048, alpha=True)
    im.colorspace_settings.name = "Non-Color"
    im.pixels.foreach_set(par.ravel())
    im.filepath_raw = os.path.join(OUT, "T_FeralPigeon_Hero_Params.png")
    im.file_format = "PNG"
    im.save()


def bake_normal(low, prof_rig, prof_hi):
    """Tangent-space normals of the 2M-triangle original (placed the same way) onto the reduced body, 2048."""
    names = set(bpy.data.objects.keys())
    bpy.ops.import_scene.gltf(filepath=os.path.join(GEN, "hi3d_pigeon.glb"))
    high = next(o for o in bpy.data.objects if o.name not in names and o.type == "MESH")
    high.data.transform(high.matrix_world)
    high.parent = None
    high.matrix_world.identity()
    co = coords(high)
    p, _ = place(co, prof_rig, prof_hi)
    high.data.vertices.foreach_set("co", p.ravel())
    high.data.update()
    img = bpy.data.images.new("T_FeralPigeon_Hero_Normal", 2048, 2048, alpha=False)
    img.colorspace_settings.name = "Non-Color"
    mat = low.data.materials[0]
    tn = mat.node_tree.nodes.new("ShaderNodeTexImage")
    tn.image = img
    mat.node_tree.nodes.active = tn
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.samples = 1
    sc.cycles.device = "CPU"
    sc.render.bake.use_selected_to_active = True
    sc.render.bake.cage_extrusion = 0.004
    sc.render.bake.max_ray_distance = 0.012
    sc.render.bake.margin = 8
    bpy.ops.object.select_all(action="DESELECT")
    high.select_set(True)
    low.select_set(True)
    bpy.context.view_layer.objects.active = low
    bpy.ops.object.bake(type="NORMAL", normal_space="TANGENT")
    img.filepath_raw = os.path.join(OUT, "T_FeralPigeon_Hero_Normal.png")
    img.file_format = "PNG"
    img.save()
    mat.node_tree.nodes.remove(tn)
    bpy.data.objects.remove(high)
    print("BAKED normal")


# ====================================================================== flight feather cards
CUT = json.load(open(os.path.join(GEN, "cut", "cut.json")))


def feather_for(bone):
    """Which scanned feather a feather bone wears."""
    base, side = bone.split(".")[0], bone.split(".")[-1]
    if bone.startswith("tf."):
        k = int(bone.split(".")[2])          # 1 = the middle pair
        return f"tail_0_{5 + k}"             # 6..11: the right half, outer vane on the image's right
    i = int(base[1:])
    if base[0] == "p":
        return f"wing_0_{min(i, 10)}"        # p1 (innermost) .. p10 (outermost, the narrow one)
    if i <= 12:
        return f"wing_1_{i - 1}"             # s1 (by the wrist) .. s12
    return f"tail_1_{i - 13}"                # tertials and humerals: the barred coverts


def load_rgba(path):
    img = bpy.data.images.load(path)
    w, h = img.size
    px = np.empty(w * h * 4, np.float32)
    img.pixels.foreach_get(px)
    bpy.data.images.remove(img)
    return px.reshape(h, w, 4)   # bottom row first (Blender)


def build_atlas(names):
    items = []
    for n in sorted(set(names)):
        a = load_rgba(os.path.join(GEN, "cut", n + ".png"))
        items.append((n, a))
    # one scale for every feather: the tallest fits a third of the atlas height
    tallest = max(a.shape[0] for _, a in items)
    k = min(1.0, TEX * 0.32 / tallest)
    atlas = np.zeros((TEX, TEX, 4), np.float32)
    rects = {}
    items.sort(key=lambda t: -t[1].shape[0])
    x = y = shelf = 0
    pad = 8
    for n, a in items:
        h, w = int(a.shape[0] * k), int(a.shape[1] * k)
        if x + w + pad > TEX:
            x, y, shelf = 0, y + shelf + pad, 0
        if y + h > TEX:
            raise RuntimeError("feather atlas full")
        # nearest-neighbour resample is enough here (k ~ 0.9-1)
        yi = np.clip((np.arange(h) / k).astype(int), 0, a.shape[0] - 1)
        xi = np.clip((np.arange(w) / k).astype(int), 0, a.shape[1] - 1)
        atlas[y:y + h, x:x + w] = a[yi][:, xi]
        rects[n] = (x / TEX, y / TEX, (x + w) / TEX, (y + h) / TEX, w / max(h, 1))
        x += w + pad
        shelf = max(shelf, h)
    # dilate colour into the empty space (no dark fringes once mipmapped)
    a = atlas[..., 3]
    filled = a > 0.02
    for _ in range(12):
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            sh = np.roll(np.roll(atlas, dy, 0), dx, 1)
            shf = np.roll(np.roll(filled, dy, 0), dx, 1)
            take = ~filled & shf
            atlas[take, :3] = sh[take, :3]
            filled = filled | take
    return rects, atlas


def save_rgba(arr, name, colorspace="sRGB"):
    h, w = arr.shape[:2]
    im = bpy.data.images.new(name, w, h, alpha=True)
    im.colorspace_settings.name = colorspace
    im.pixels.foreach_set(arr.astype(np.float32).ravel())
    im.filepath_raw = os.path.join(OUT, name + ".png")
    im.file_format = "PNG"
    im.save()
    return im


def feather_lengths(arm, old):
    """glTF has no bone lengths (Blender's importer makes the tails up), so each feather's length is read off the
    template's own card for that bone: the farthest of its vertices along the bone."""
    co = coords(old)
    g = dominant_groups(old)
    out = {}
    for b in arm.data.bones:
        h, t = bone_ws(arm, b.name)
        d = (t - h) / max(np.linalg.norm(t - h), 1e-9)
        s = co[g == b.name]
        if len(s):
            out[b.name] = float(((s - h) @ d).max())
    return out


def feather_cards(arm, rects, lengths):
    """One card per feather bone: the quill at the bone's head, the tip at its tail, the outer vane toward the wing's
    leading edge (primaries) or its tip (secondaries, tail), in the wing / tail plane, curved a little (camber)."""
    verts, faces, uvs, groups = [], [], [], []
    up = np.array([0.0, 0.0, 1.0])
    for b in arm.data.bones:
        n = b.name
        base = n.split(".")[0]
        is_p = base[:1] == "p" and base[1:].isdigit()
        is_s = base[:1] == "s" and base[1:].isdigit()
        is_t = n.startswith("tf.")
        if not (is_p or is_s or is_t):
            continue
        h, t = bone_ws(arm, n)
        d = (t - h) / np.linalg.norm(t - h)
        L = lengths[n]
        side = 1.0 if n.endswith(".L") or ".L." in n else -1.0
        c = np.cross(d, up)
        c /= np.linalg.norm(c)
        outer = np.array([1.0, 0.0, 0.0]) if is_p else np.array([0.0, side, 0.0])
        if np.dot(c, outer) < 0:
            c = -c
        u0, v0, u1, v1, aspect = rects[feather_for(n)]
        Lc = L * 1.04
        W = Lc * aspect
        # stacking: inner over outer, secondaries over primaries, the tail's middle pair on top
        i = int(base[1:]) if not is_t else int(n.split(".")[2])
        if is_p:
            lift = 0.0004 * (10 - i)
        elif is_s:
            lift = 0.0045 + 0.00035 * i
        else:
            lift = 0.0004 * (7 - i)
        nu, nv = 3, 7
        start = len(verts)
        for j in range(nv + 1):
            v = j / nv
            for k in range(nu + 1):
                u = k / nu
                across = (u - 0.5) * W
                # the rachis sits a little toward the outer vane in the scans; camber: the vane edges curve down
                camber = -0.10 * W * (2 * u - 1) ** 2 * (0.4 + 0.6 * v)
                p = h - d * 0.03 * Lc + d * v * Lc + c * across + up * (lift + camber)
                verts.append(p)
                uvs.append((u0 + (u1 - u0) * u, v0 + (v1 - v0) * v))
                groups.append(n)
        for j in range(nv):
            for k in range(nu):
                a = start + j * (nu + 1) + k
                faces.append((a, a + 1, a + nu + 2, a + nu + 1))
    me = bpy.data.meshes.new("cards")
    me.from_pydata([tuple(v) for v in verts], [], faces)
    uvl = me.uv_layers.new(name="UVMap")
    for poly in me.polygons:
        for li in poly.loop_indices:
            uvl.data[li].uv = uvs[me.loops[li].vertex_index]
    ob = bpy.data.objects.new("cards", me)
    bpy.context.scene.collection.objects.link(ob)
    for gname in sorted(set(groups)):
        ob.vertex_groups.new(name=gname)
    for vi, gname in enumerate(groups):
        ob.vertex_groups[gname].add([vi], 1.0, "REPLACE")
    print("CARDS", len(faces), "quads")
    return ob


# ====================================================================== legs, weights, slots
def covert_cards(old, rects):
    """The template's wing coverts (every card on a feather bone that is not that bone's flight feather, and the alula):
    their geometry and skin weights as built (they fold with the wing the way the template's do), re-mapped onto the
    scanned coverts: the outermost covert on each secondary (the greater covert, where the front wing bar is) barred,
    the rest plain."""
    ob = old.copy()
    ob.data = old.data.copy()
    ob.name = "coverts"
    bpy.context.scene.collection.objects.link(ob)
    co = coords(ob)
    g = dominant_groups(ob)
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bm.faces.ensure_lookup_table()
    seen, islands = set(), []
    for f in bm.faces:
        if f.index in seen:
            continue
        stack, isl = [f], []
        seen.add(f.index)
        while stack:
            h = stack.pop()
            isl.append(h)
            for e in h.edges:
                for k in e.link_faces:
                    if k.index not in seen:
                        seen.add(k.index)
                        stack.append(k)
        islands.append(isl)

    def bone_of(isl):
        from collections import Counter
        return Counter(g[v.index] for f in isl for v in f.verts).most_common(1)[0][0]

    def is_wing(n):
        b = n.split(".")[0]
        return (b[:1] in "ps" and b[1:].isdigit()) or b in ("hand", "humerus", "forearm", "primaries")
    by_bone = {}
    for isl in islands:
        n = bone_of(isl)
        if is_wing(n) and isl[0].material_index == 0:
            by_bone.setdefault(n, []).append(isl)
    keep, plain, barred = [], sorted(k for k in rects if k.startswith("tail_1_")), []
    barred = plain[:7]
    plain = plain[7:]
    uvl = bm.loops.layers.uv.active
    count = 0
    for n, isls in by_bone.items():
        b = n.split(".")[0]
        flight = b[:1] in "ps" and b[1:].isdigit()
        vids = [sorted({v.index for f in isl for v in f.verts}) for isl in isls]
        reach = [np.linalg.norm(co[v] - co[v].mean(0), axis=1).max() for v in vids]
        order = np.argsort(reach)[::-1]
        if flight:
            order = order[1:]          # the longest card on a feather bone is the feather itself (scanned card instead)
        for rank, j in enumerate(order):
            isl, v = isls[j], vids[j]
            P = co[v]
            c = P.mean(0)
            # the card's own axes: its length (rachis) and width, the root the end nearer the body
            w, vec = np.linalg.eigh(np.cov((P - c).T))
            ax, ac = vec[:, 2], vec[:, 1]
            if np.dot(ax, c - np.array([0.0, 0.0, 0.08])) * 0 + np.dot(ax, P[np.argmax(np.abs(P[:, 1]))] - c) < 0:
                ax = -ax
            # the root is the end nearer the wing's leading edge (coverts point back and out)
            if ax[0] > 0:
                ax = -ax
            a = (P - c) @ ax
            q = (P - c) @ ac
            pick = barred[(count + rank) % len(barred)] if (b[:1] == "s" and rank == 0) else plain[(count + rank) % len(plain)]
            u0, v0, u1, v1 = rects[pick][:4]
            v0 = v0 + (v1 - v0) * 0.33     # the downy base and the quill stay hidden under the next row
            lut = dict(zip(v, zip((q - q.min()) / max(np.ptp(q), 1e-6), (a - a.min()) / max(np.ptp(a), 1e-6))))
            for f in isl:
                for lp in f.loops:
                    uu, vv = lut[lp.vert.index]
                    lp[uvl].uv = (u0 + (u1 - u0) * uu, v0 + (v1 - v0) * vv)
            keep.extend(isl)
            count += 1
    keep_set = set(f.index for f in keep)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.index not in keep_set], context="FACES")
    bm.to_mesh(ob.data)
    bm.free()
    print("COVERTS", count, "cards,", len(ob.data.polygons), "faces")
    return ob


def take_legs(old):
    """The template's legs and toes (its bare-skin faces whose vertices hang on the leg bones)."""
    ob = old.copy()
    ob.data = old.data.copy()
    ob.name = "legs"
    bpy.context.scene.collection.objects.link(ob)
    g = dominant_groups(ob)
    legv = np.array([n.split(".")[0] in LEG_BONES for n in g])
    bare = [i for i, m in enumerate(ob.data.materials) if "Bare" in m.name]
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    kill = [f for f in bm.faces if f.material_index not in bare or not all(legv[v.index] for v in f.verts)]
    bmesh.ops.delete(bm, geom=kill, context="FACES")
    bm.to_mesh(ob.data)
    bm.free()
    print("LEGS", len(ob.data.polygons), "faces")
    return ob


def arm_bone_for(name):
    """A flight feather's bone -> the arm bone it sits on (the body's own feathers must not follow one feather)."""
    base, side = name.split(".")[0], name.split(".")[-1]
    if name.startswith("primaries") or (base[:1] == "p" and base[1:].isdigit()):
        return f"hand.{side}"
    if base[:1] == "s" and base[1:].isdigit():
        return f"forearm.{side}" if int(base[1:]) <= 14 else f"humerus.{side}"
    return None


def transfer_weights(dst, old):
    """Skin weights of the template's body onto the new one: nearest face, interpolated; the template's flight feather
    weights first folded onto their arm bones."""
    src = old.copy()
    src.data = old.data.copy()
    bpy.context.scene.collection.objects.link(src)
    names = {g.index: g.name for g in src.vertex_groups}
    adds = []
    for v in src.data.vertices:
        for g in v.groups:
            tgt = arm_bone_for(names[g.group])
            if tgt:
                adds.append((tgt, v.index, g.weight))
    for tgt, vi, w in adds:
        src.vertex_groups[tgt].add([vi], w, "ADD")
    for nm in list(names.values()):
        if arm_bone_for(nm):
            src.vertex_groups.remove(src.vertex_groups[nm])
    for vg in src.vertex_groups:
        if vg.name not in dst.vertex_groups:
            dst.vertex_groups.new(name=vg.name)
    bpy.context.view_layer.objects.active = dst
    m = dst.modifiers.new("dt", "DATA_TRANSFER")
    m.object = src
    m.use_vert_data = True
    m.data_types_verts = {"VGROUP_WEIGHTS"}
    m.vert_mapping = "POLYINTERP_NEAREST"
    m.layers_vgroup_select_src = "ALL"
    m.layers_vgroup_select_dst = "NAME"
    bpy.ops.object.modifier_apply(modifier="dt")
    bpy.data.objects.remove(src)
    # one continuous skin (unlike the template's separate feathers): soft weight borders, so a wing swept or folded at
    # the shoulder stretches the feathers there instead of tearing a wall between body and arm
    smooth_weights(dst, 12, 0.5)


def smooth_weights(ob, repeat, factor):
    """Laplacian smoothing of every vertex group over the mesh edges, then at most 4 bones a vertex, normalised."""
    me = ob.data
    nv, ng = len(me.vertices), len(ob.vertex_groups)
    W = np.zeros((nv, ng), np.float32)
    for v in me.vertices:
        for g in v.groups:
            W[v.index, g.group] = g.weight
    e = np.empty(len(me.edges) * 2, np.int64)
    me.edges.foreach_get("vertices", e)
    e = e.reshape(-1, 2)
    deg = np.bincount(e.ravel(), minlength=nv).astype(np.float32)[:, None]
    for _ in range(repeat):
        acc = np.zeros_like(W)
        np.add.at(acc, e[:, 0], W[e[:, 1]])
        np.add.at(acc, e[:, 1], W[e[:, 0]])
        avg = np.where(deg > 0, acc / np.maximum(deg, 1), W)
        W = W * (1 - factor) + avg * factor
    # keep the 4 largest
    if ng > 4:
        idx = np.argsort(-W, axis=1)[:, 4:]
        np.put_along_axis(W, idx, 0.0, axis=1)
    W /= np.maximum(W.sum(1, keepdims=True), 1e-6)
    for gi, vg in enumerate(ob.vertex_groups):
        vg.remove(list(range(nv)))
        nz = np.nonzero(W[:, gi] > 1e-3)[0]
        for vi in nz:
            vg.add([int(vi)], float(W[vi, gi]), "REPLACE")


def assign_slots(body, eye_mid):
    """Skull: every face of the head (head / jaw-weighted, above the gape) except the bill and the eyes' own rims."""
    co = coords(body)
    hg = body.vertex_groups.get("head")
    w = np.zeros(len(co))
    for v in body.data.vertices:
        for g in v.groups:
            if g.group == hg.index:
                w[v.index] = g.weight
    ex, ez = eye_mid[0], eye_mid[2]
    bill = (co[:, 0] > ex + 0.011) & (co[:, 2] > ez - 0.03) & (co[:, 2] < ez + 0.006) & (np.abs(co[:, 1]) < 0.012)
    # the head and the throat in front of the camera (everything ahead of the neck's base, above the breast)
    skull_v = ((w > 0.15) | ((co[:, 0] > ex - 0.02) & (co[:, 2] > ez - 0.035))) & ~bill
    for poly in body.data.polygons:
        if all(skull_v[i] for i in poly.vertices):
            poly.material_index = 1


def material(name, img=None):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    if img is not None:
        t = m.node_tree.nodes.new("ShaderNodeTexImage")
        t.image = img
        p = m.node_tree.nodes["Principled BSDF"]
        m.node_tree.links.new(t.outputs["Color"], p.inputs["Base Color"])
        m.node_tree.links.new(t.outputs["Alpha"], p.inputs["Alpha"])
    return m


# ====================================================================== main
def main():
    os.makedirs(os.path.join(OUT, "previews"), exist_ok=True)
    if not os.path.exists(REF_COPY):
        import shutil
        shutil.copyfile(REF, REF_COPY)
    bpy.ops.wm.open_mainfile(filepath=os.path.join(GEN, "hi3d_dec.blend"))
    body = next(o for o in bpy.data.objects if o.type == "MESH")
    body.name = "hi3d_body"
    arm, old = load_template()
    old.name = "template_body"
    body.name = "SK_FeralPigeon_Hero"
    prof_rig = wing_profile(old)
    p, prof_hi = place(coords(body), prof_rig)
    body.data.vertices.foreach_set("co", p.ravel())
    body.data.update()
    eyes = fit.find_eyes(body)
    eye_mid = (eyes[0] + eyes[1]) / 2
    print("EYES mid", eye_mid.round(4), "rig", ((bone_ws(arm, "eye_l")[0] + bone_ws(arm, "eye_r")[0]) / 2).round(4))
    cut_body(body, prof_rig)
    decimate(body, BODY_TRIS)
    body_textures(body)
    if "--no-bake" not in ARGV:
        bake_normal(body, prof_rig, prof_hi)
    transfer_weights(body, old)
    # slots: 0 Body, 1 Skull (same look, hidden in first person), 2 Feathers, 3 Bare
    alb = bpy.data.images.load(os.path.join(OUT, "T_FeralPigeon_Hero_Albedo.png"))
    m_body = material("M_FeralPigeon_Plumage_Hero", alb)
    m_skull = material("M_FeralPigeon_Skull_Hero", alb)
    body.data.materials.clear()
    body.data.materials.append(m_body)
    body.data.materials.append(m_skull)
    assign_slots(body, eye_mid)
    names = [feather_for(b.name) for b in arm.data.bones if b.name.startswith("tf.") or (b.name[:1] in "ps" and b.name.split(".")[0][1:].isdigit())]
    names += ["tail_1_%d" % i for i in range(12)]
    rects, atlas = build_atlas(names)
    # which feather sits where in the atlas (make_morphs.py recolours by kind)
    json.dump({n: {"rect": r[:4], "kind": "tail" if n.startswith("tail_0") else "covert" if n.startswith("tail_1") else
                   "primary" if n.startswith("wing_0") else "secondary"} for n, r in rects.items()},
              open(os.path.join(GEN, "feather_rects.json"), "w"), indent=1)
    img_f = save_rgba(atlas, "T_FeralPigeon_HeroFeathers_Albedo")
    flat = np.zeros_like(atlas)
    flat[..., 0] = 0.5
    flat[..., 1] = 0.5
    flat[..., 2] = 1.0
    flat[..., 3] = 1.0
    save_rgba(flat, "T_FeralPigeon_HeroFeathers_Normal", "Non-Color")
    par = np.zeros_like(atlas)
    par[..., 0] = 0.72
    par[..., 1] = 0.12
    par[..., 3] = 1.0
    save_rgba(par, "T_FeralPigeon_HeroFeathers_Params", "Non-Color")
    cards = feather_cards(arm, rects, feather_lengths(arm, old))
    m_feath = material("M_FeralPigeon_Feathers_Hero", img_f)
    cards.data.materials.append(m_feath)
    legs = take_legs(old)
    coverts = covert_cards(old, rects)
    m_bare = legs.data.materials[[i for i, m in enumerate(legs.data.materials) if "Bare" in m.name][0]]
    # one mesh
    for o in (cards, legs, coverts):
        for poly in o.data.polygons:
            poly.material_index = 0
        o.data.materials.clear()
    cards.data.materials.append(m_feath)
    coverts.data.materials.append(m_feath)
    legs.data.materials.append(m_bare)
    body.data.materials.append(m_feath)
    body.data.materials.append(m_bare)
    bpy.data.objects.remove(old)
    bpy.ops.object.select_all(action="DESELECT")
    for o in (body, cards, legs, coverts):
        o.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.join()
    me = body.data
    me.calc_loop_triangles()
    print("TOTAL", len(me.loop_triangles), "tris,", len(me.vertices), "verts, slots", [m.name for m in me.materials])
    body.parent = arm
    for mo in list(body.modifiers):
        body.modifiers.remove(mo)
    mo = body.modifiers.new("Armature", "ARMATURE")
    mo.object = arm
    if "--previews" in ARGV:
        previews(arm, body)
    bpy.ops.object.select_all(action="DESELECT")
    body.select_set(True)
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    path = os.path.join(OUT, "FeralPigeon_Hero.glb")
    kw = dict(filepath=path, export_format="GLB", use_selection=True, export_yup=True, export_apply=False, export_normals=True,
              export_texcoords=True, export_materials="EXPORT", export_skins=True, export_animations=False, export_vertex_color="NONE",
              export_def_bones=False, export_image_format="NONE")
    props = bpy.ops.export_scene.gltf.get_rna_type().properties
    bpy.ops.export_scene.gltf(**{k: v for k, v in kw.items() if k in props or k == "filepath"})
    rig = json.load(open(os.path.join(OUT, "FeralPigeon_Hero_rig.json")))
    rig.update({"tris": len(me.loop_triangles), "atlas": TEX, "source": "hero_pigeon (hi3d + scanned feathers)", "firstPerson": True})
    json.dump(rig, open(os.path.join(OUT, "FeralPigeon_Hero_rig.json"), "w"), indent=1)
    print("EXPORTED", path, os.path.getsize(path), "bytes")


def fpv_pose(arm, on):
    """In first person the wings are swept 45 degrees forward and drooped 0.2 rad (ABirdActor::SetFirstPerson)."""
    bpy.context.view_layer.update()
    for side, sy in (("L", 1), ("R", -1)):
        pb = arm.pose.bones[f"humerus.{side}"]
        pb.matrix_basis.identity()
        if on:
            bpy.context.view_layer.update()
            head = arm.matrix_world @ pb.head
            R = mathutils.Matrix.Rotation(-sy * math.radians(45), 4, "Z") @ mathutils.Matrix.Rotation(sy * 0.2, 4, "X")
            W = arm.matrix_world @ pb.matrix
            W = mathutils.Matrix.Translation(head) @ R @ mathutils.Matrix.Translation(-head) @ W
            pb.matrix = arm.matrix_world.inverted() @ W
    bpy.context.view_layer.update()


def previews(arm, body):
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    sc.render.resolution_x, sc.render.resolution_y = 1200, 800
    for m in body.data.materials:
        m.surface_render_method = "DITHERED"
        # as in Unreal: the body is one-sided (from inside the head it is not there), the feather cards two-sided
        m.use_backface_culling = "Feathers" not in m.name
    w = bpy.data.worlds.new("w")
    sc.world = w
    w.color = (0.55, 0.58, 0.62)
    sun = bpy.data.objects.new("PSun", bpy.data.lights.new("PSun", "SUN"))
    sc.collection.objects.link(sun)
    sun.data.energy = 4
    sun.rotation_euler = (0.7, 0.2, 0.6)
    cam = bpy.data.objects.new("PCam", bpy.data.cameras.new("PCam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    shots = {"34": ((0.35, -0.35, 0.30), (0.0, 0.0, 0.08)), "top": ((0.0, 0.0, 0.75), (-0.01, 0.0, 0.08)),
             "side": ((0.0, -0.55, 0.10), (0.0, 0.0, 0.08)), "below": ((0.1, -0.2, -0.45), (0.0, 0.0, 0.08)),
             "head": ((0.22, -0.12, 0.13), (0.09, 0.0, 0.09)), "fpv": ((0.104, 0.0, 0.096), (0.4, 0.0, 0.04)), "fpvcheck": ((-0.25, -0.25, 0.35), (0.05, 0.0, 0.08))}
    for name, (loc, tgt) in shots.items():
        fpv_pose(arm, name.startswith("fpv"))
        cam.location = loc
        cam.rotation_euler = (mathutils.Vector(tgt) - mathutils.Vector(loc)).to_track_quat("-Z", "Z" if name == "top" else "Y").to_euler()
        if name == "top":
            cam.rotation_euler = (0, 0, math.pi / 2)
        cam.data.lens = 9 if name == "fpv" else 50
        for m in body.data.materials:
            if "Skull" in m.name:
                a = m.node_tree.nodes["Principled BSDF"].inputs["Alpha"]
                for lk in list(a.links):
                    m.node_tree.links.remove(lk)
                a.default_value = 0.0 if name == "fpv" else 1.0
        cam.data.clip_start = 0.005
        sc.render.filepath = os.path.join(OUT, "previews", f"hero_{name}.png")
        bpy.ops.render.render(write_still=True)
    for o in (cam, sun):
        bpy.data.objects.remove(o)


main()
