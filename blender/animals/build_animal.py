"""One animal breed (Cat, Laika, Shepherd, Mutt), sculpted, furred, rigged and animated, for Unreal.

    Blender -b --python blender/animals/build_animal.py -- Cat [--previews] [--no-export] [--quick]

The body is a signed-distance sculpt over the breed's skeleton (anatomy.py), meshed with OpenVDB and decimated to
four LODs that share one UV layout; the coat is painted procedurally in 3-d (coats.py) into one texture per coat;
the fur is shells: copies of each LOD pushed out along the normals and combed toward the tail and down, each shell's
height in the vertex colour (R) for the masked fur material in Unreal (16-20 shells up close, none far away).
Clips (gaits.py) are generated from a gait and pose solver: in place, 30 fps; locomotion clips are one stride cycle.

Output (blender/animals/export/<Breed>/): <Breed>.glb (LOD0..3 named LODn_SK_<Breed>, armature, clips),
T_<Breed>_<Coat>.png per coat, T_<Breed>_Eye_<n>.png, <Breed>_rig.json (sizes, clip speeds, coats), previews/*.png.
Frame: facing +X, left +Y, up +Z, metres; the origin on the ground between the feet.
"""
import json
import math
import os
import sys
import time
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import bmesh  # noqa: E402
import bpy  # noqa: E402
import mathutils  # noqa: E402
import numpy as np  # noqa: E402

import anatomy as AN  # noqa: E402
import gen_fit as GF  # noqa: E402
import coats as CO  # noqa: E402
import gaits as GA  # noqa: E402
import sdf  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
BREED = argv[0] if argv else "Cat"
FLAGS = set(argv[1:])
QUICK = "--quick" in FLAGS
B = AN.BREEDS[BREED]
B["name"] = BREED
OUT = os.path.join(ROOT, "blender", "animals", "export", BREED)
os.makedirs(os.path.join(OUT, "previews"), exist_ok=True)
FPS = 30
CAT = B["kind"] == "cat"
# triangle budgets of the body per LOD (the shells multiply them up close)
LOD_TRIS = [11000, 4600, 1700, 650] if CAT else [12000, 5000, 1900, 700]
LOD_SHELLS = [B["shells"], max(6, B["shells"] // 2), 3, 0]
TEX = 1024
# a generated body (hi3d, blender/animals/gen/<Breed>/model.glb, gen.json): its own shape and texture, the breed's
# skeleton fitted onto it; the fur shells grow from a coarse copy (the budget is ~20k triangles up close)
GEN = "--gen" in FLAGS
GEN_GLB = os.path.join(ROOT, "blender", "animals", "gen", BREED, "model.glb")
if GEN:
    LOD_TRIS = [14000, 5000, 1800, 700]
    LOD_SHELLS = [6, 4, 0, 0]
    TEX = 2048
    # a generated body can stay as it was generated (Maru, 2026-10-29: the shells made her look nothing like her hi3d
    # model): its own painted coat, no shells, more triangles
    if B.get("gen_lods"):
        LOD_TRIS, LOD_SHELLS = list(B["gen_lods"]), [0, 0, 0, 0]


def log(*a):
    print("ANIMAL", BREED, *a, flush=True)


# ====================================================================== mesh
def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def link(ob):
    bpy.context.scene.collection.objects.link(ob)
    return ob


def apply_mods(ob):
    bpy.context.view_layer.objects.active = ob
    for o in bpy.context.selected_objects:
        o.select_set(False)
    ob.select_set(True)
    for m in list(ob.modifiers):
        bpy.ops.object.modifier_apply(modifier=m.name)


def to_object(name, verts, quads, tris):
    me = bpy.data.meshes.new(name)
    faces = [tuple(int(i) for i in q) for q in quads] + [tuple(int(i) for i in t) for t in tris]
    me.from_pydata([tuple(v) for v in verts], [], faces)
    me.validate()
    ob = link(bpy.data.objects.new(name, me))
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    return ob


def decimate(ob, tris):
    n = sum(len(p.vertices) - 2 for p in ob.data.polygons)
    if n > tris:
        mod = ob.modifiers.new("Decimate", "DECIMATE")
        mod.ratio = tris / n
        mod.use_collapse_triangulate = True
        mod.delimit = set()            # a generated body's many UV islands must not stop the collapse
        apply_mods(ob)
    for poly in ob.data.polygons:
        poly.use_smooth = True


def decimate_face(ob, tris, face_tris):
    """decimate(), but the face (the head forward of the ears, round the eyes and the muzzle) keeps ~face_tris of the
    budget: an even collapse left a generated head a few hundred triangles, the eyes in faceted pits."""
    J = B["joints"]
    head, tip = np.array(J["head"], float), np.array(J["head_end"], float)
    L = float(np.linalg.norm(tip - head))
    P = verts_np(ob)
    fwd = (tip - head) / L
    t = (P - head) @ fwd                                     # along the head, 0 at the skull's back
    d = np.linalg.norm((P - head) - t[:, None] * fwd, axis=1)  # off its axis
    face = (t > -L * 0.15) & (d < L * 0.75)
    n = sum(len(p.vertices) - 2 for p in ob.data.polygons)
    nf = sum(len(p.vertices) - 2 for p in ob.data.polygons if face[list(p.vertices)].all())
    if n <= tris or nf == 0:
        decimate(ob, tris)
        return
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bm.faces.ensure_lookup_table()
    head_faces = [f for f in bm.faces if all(face[v.index] for v in f.verts)]
    # the rest first (to what's left of the budget), then the face, each with the other side pinned
    for part, budget in ((False, tris - face_tris), (True, face_tris)):
        grp = ob.vertex_groups.new(name="dec")
        idx = [i for i in range(len(ob.data.vertices)) if bool(face[i]) == part]
        grp.add(idx, 1.0, "REPLACE")
        cur = sum(len(p.vertices) - 2 for p in ob.data.polygons)
        have = sum(len(p.vertices) - 2 for p in ob.data.polygons if all(bool(face[v]) == part for v in p.vertices))
        mod = ob.modifiers.new("Decimate", "DECIMATE")
        mod.ratio = max(0.01, min(1.0, (cur - have + budget) / cur))
        mod.use_collapse_triangulate = True
        mod.delimit = set()
        mod.vertex_group = "dec"
        mod.vertex_group_factor = 1000.0
        apply_mods(ob)
        ob.vertex_groups.remove(ob.vertex_groups["dec"])
        P = verts_np(ob)
        t = (P - head) @ fwd
        d = np.linalg.norm((P - head) - t[:, None] * fwd, axis=1)
        face = (t > -L * 0.15) & (d < L * 0.75)
    bm.free()
    for poly in ob.data.polygons:
        poly.use_smooth = True
    nf = sum(len(p.vertices) - 2 for p in ob.data.polygons if face[list(p.vertices)].all())
    log("decimated with the face kept: %d tris, face %d" % (sum(len(p.vertices) - 2 for p in ob.data.polygons), nf))


def copy_object(ob, name):
    o = ob.copy()
    o.data = ob.data.copy()
    o.name = name
    o.data.name = name
    return link(o)


def smart_uv(ob):
    bpy.context.view_layer.objects.active = ob
    for o in bpy.context.selected_objects:
        o.select_set(False)
    ob.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.006, area_weight=0.0, scale_to_bounds=False)
    bpy.ops.object.mode_set(mode="OBJECT")


def verts_np(ob):
    me = ob.data
    a = np.zeros(len(me.vertices) * 3)
    me.vertices.foreach_get("co", a)
    return a.reshape(-1, 3)


def normals_np(ob):
    me = ob.data
    a = np.zeros(len(me.vertices) * 3)
    me.vertices.foreach_get("normal", a)
    return a.reshape(-1, 3)


# ====================================================================== skin weights
def weights_for(P, piece):
    """bone -> weights (N) from the distance to each bone's primitives; the mouth's two lips are split by the slit."""
    d = piece.bone_distances(P)
    if GEN:
        # a generated tail often hangs close by the hind legs: the legs' skin must not follow the tail
        H = float(B["joints"]["chest"][2])
        for k in d:
            if k.startswith("tail_"):
                d[k] = d[k] + H * 0.035
    fall = B["voxel"] * (4.0 if CAT else 3.2)
    W = sdf.skin_weights(d, falloff=fall, keep=4)
    # the lips: above the slit -> head, below -> jaw (no blend across the mouth)
    corner, front = B["_mouth"]
    ax = front - corner
    L = float(np.linalg.norm(ax))
    ax = ax / L
    rel = P - corner
    t = rel @ ax
    near = (t > -0.15 * L) & (t < 1.25 * L) & (np.abs(P[:, 1]) < B["muzzle"][1] * 1.6)
    up = np.cross(np.cross(ax, (0, 0, 1)), ax)
    up = up / np.linalg.norm(up)
    h = rel @ up
    band = near & (np.abs(h) < B["muzzle"][2] * 1.6)
    names = list(W)
    if "jaw" in W and "head" in W:
        hj = W["head"] + W["jaw"]
        lower = band & (h < 0)
        upper = band & (h >= 0)
        W["jaw"] = np.where(lower, hj, np.where(upper, 0.0, W["jaw"]))
        W["head"] = np.where(upper, hj, np.where(lower, 0.0, W["head"]))
    tot = sum(W[n] for n in names)
    W = {n: W[n] / np.maximum(tot, 1e-9) for n in names}
    if B.get("kind") == "dog" and "head" in W:
        # the skull is rigid (Dima, 2026-10-29: a turning head bent the muzzle out of shape): where the head and the
        # jaw hold most of a vertex they take all of it, and the blend into the neck is kept to a short band
        hj = W["head"] + W.get("jaw", 0)
        k = np.clip((hj - 0.3) / 0.35, 0, 1)
        new_hj = hj + (1 - hj) * k
        rest = np.maximum(1 - hj, 1e-9)
        for n in names:
            if n in ("head", "jaw"):
                W[n] = W[n] / np.maximum(hj, 1e-9) * new_hj
            else:
                W[n] = W[n] / rest * (1 - new_hj)
    return W


def set_groups(ob, W):
    for bone, w in W.items():
        g = ob.vertex_groups.get(bone) or ob.vertex_groups.new(name=bone)
        idx = np.nonzero(w > 2e-3)[0]
        for i in idx:
            g.add([int(i)], float(w[i]), "REPLACE")


# ====================================================================== eyes, teeth, whiskers
def uv_sphere(name, c, r, look, seg=24, rings=16):
    me = bpy.data.meshes.new(name)
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=seg, v_segments=rings, radius=r, calc_uvs=True)
    bm.to_mesh(me)
    bm.free()
    ob = link(bpy.data.objects.new(name, me))
    q = mathutils.Vector((0, 0, 1)).rotation_difference(mathutils.Vector(look))
    me.transform(mathutils.Matrix.Translation(mathutils.Vector(c)) @ q.to_matrix().to_4x4())
    for p in me.polygons:
        p.use_smooth = True
    # UVs from the look axis: v = 1 at the front pole, u round it (eye_texture's layout)
    lk = np.array(look) / np.linalg.norm(look)
    ref = np.cross(lk, (0, 0, 1))
    ref = ref / np.linalg.norm(ref)
    ref2 = np.cross(lk, ref)
    if not me.uv_layers:
        me.uv_layers.new(name="UVMap")
    uvl = me.uv_layers.active.data
    for poly in me.polygons:
        cc = np.array(poly.center) - c
        az0 = (math.atan2(cc @ ref2, cc @ ref) / (2 * math.pi)) % 1.0
        for li in poly.loop_indices:
            d = np.array(me.vertices[me.loops[li].vertex_index].co) - c
            d = d / np.linalg.norm(d)
            a = math.acos(max(-1.0, min(1.0, d @ lk)))
            az = (math.atan2(d @ ref2, d @ ref) / (2 * math.pi)) % 1.0
            if abs(az - az0) > 0.5:
                az += 1.0 if az < az0 else -1.0
            uvl[li].uv = (az, 1.0 - a / math.pi)
    return ob


def eyes():
    out = []
    e = B["eye"]
    if B.get("eyes_at"):
        # a generated head: each eyeball sits in the eye the texture shows, its front a little proud of the skin
        for (s, k), (c0, r, nk) in zip(AN.SIDES, B["eyes_at"] if AN.SIDES[0][1] > 0 else B["eyes_at"][::-1]):
            look = nk * 0.6 + np.array([1.0, 0, 0]) * 0.4
            look = look / np.linalg.norm(look)
            out.append((uv_sphere("Eye_" + s, c0 - nk * r * 0.62, r, look), "head"))
        return out
    for s, k in AN.SIDES:
        a = math.radians(B["eye_out"])
        look = np.array((math.cos(a), k * math.sin(a), 0.05))
        # set into the socket: only the front cap shows between the lids
        c = np.array((e[0], e[1] * k, e[2])) - look * B["eye_r"] * 0.18
        ob = uv_sphere("Eye_" + s, c, B["eye_r"], look)
        out.append((ob, "head"))
    return out


def mouth_frame():
    """The mouth line (corner -> front), its up and its side, from the template fitted onto the body."""
    corner, front = (np.array(v, float) for v in B["_mouth"])
    ax = front - corner
    L = float(np.linalg.norm(ax))
    ax = ax / L
    up = np.cross(np.cross(ax, (0, 0, 1)), ax)
    up = up / np.linalg.norm(up)
    return corner, front, ax, up, L


def cut_mouth(ob):
    """A generated head is closed: opening the jaw stretched the skin across the lips. The faces that bridge the lip
    line (a vertex above it and one below, inside the mouth's band) are cut away, so the lips part."""
    corner, front, ax, up, L = mouth_frame()
    P = verts_np(ob)
    rel = P - corner
    t = rel @ ax
    h = rel @ up
    band = (t > -0.05 * L) & (t < 1.08 * L) & (np.abs(P[:, 1]) < B["muzzle"][1] * 1.6) & (np.abs(h) < B["muzzle"][2] * 0.5)
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bm.faces.ensure_lookup_table()
    cut = []
    for f in bm.faces:
        ids = [v.index for v in f.verts]
        if band[ids].all() and (h[ids] > 0).any() and (h[ids] <= 0).any():
            cut.append(f)
    bmesh.ops.delete(bm, geom=cut, context="FACES_ONLY")
    bm.to_mesh(ob.data)
    bm.free()
    log("mouth cut: %d faces" % len(cut))


def mouth_parts():
    """Every dog's default inside of the mouth: a dark mouth, upper teeth on the head, lower teeth and the tongue on
    the jaw (materials Mouth_Inside, Mouth_Teeth, Mouth_Tongue). Sized from the muzzle; hidden while the mouth is shut."""
    corner, front, ax, up, L = mouth_frame()
    side = np.cross(up, ax)
    hw = float(B["muzzle"][1]) * 0.78                    # half the width inside the lips
    out = []

    def mesh_from(name, verts, faces, mat):
        me = bpy.data.meshes.new(name)
        me.from_pydata([tuple(v) for v in verts], [], faces)
        me.update()
        for p in me.polygons:
            p.use_smooth = True
        me.uv_layers.new(name="UVMap")
        ob = link(bpy.data.objects.new(name, me))
        ob.data.materials.append(mat)
        return ob

    def ellipsoid(name, c, a, b, cc, mat, seg=16, rings=8):
        bm = bmesh.new()
        bmesh.ops.create_uvsphere(bm, u_segments=seg, v_segments=rings, radius=1.0)
        V = [np.array(v.co) for v in bm.verts]
        F = [[v.index for v in f.verts] for f in bm.faces]
        bm.free()
        Vw = [c + ax * v[0] * a + side * v[1] * b + up * v[2] * cc for v in V]
        return mesh_from(name, Vw, F, mat)

    def tooth_row(name, z_sign, mat, n=7):
        V, F = [], []
        for sdir in (1, -1):
            for i in range(n):
                u = i / (n - 1)                                   # 0 at the front, 1 at the corner
                tpos = front - ax * (L * (0.06 + 0.86 * u))
                w = hw * (0.35 + 0.65 * math.sqrt(u))             # the jaw's U
                base = tpos + side * (w * sdir) + up * (z_sign * float(B["muzzle"][2]) * 0.12)
                size = L * (0.035 if i == 1 else 0.022)            # the canine is bigger
                tip = base - up * z_sign * size * (2.2 if i == 1 else 1.2)
                k = len(V)
                r = size * 0.5
                ring = [base + ax * r * math.cos(a) + side * r * math.sin(a) for a in np.linspace(0, 2 * math.pi, 6, endpoint=False)]
                V.extend(ring + [tip])
                for j in range(6):
                    F.append([k + j, k + (j + 1) % 6, k + 6])
        return mesh_from(name, V, F, mat)

    m_in = preview_material("Mouth_Inside", (0.08, 0.02, 0.025), 0.5)
    m_te = preview_material("Mouth_Teeth", (0.86, 0.83, 0.74), 0.3)
    m_to = preview_material("Mouth_Tongue", (0.62, 0.22, 0.25), 0.35)
    mid = (corner + front) / 2 - up * float(B["muzzle"][2]) * 0.25
    inside = ellipsoid("Mouth_Inside", mid - ax * L * 0.05, L * 0.55, hw * 0.95, float(B["muzzle"][2]) * 0.55, m_in)
    P = verts_np(inside)
    hgt = (P - corner) @ up
    out.append((inside, {"head": (hgt > 0).astype(float), "jaw": (hgt <= 0).astype(float)}))
    out.append((tooth_row("Mouth_TeethUp", 1, m_te), "head"))
    out.append((tooth_row("Mouth_TeethLow", -1, m_te), "jaw"))
    tongue = ellipsoid("Mouth_Tongue", mid - up * float(B["muzzle"][2]) * 0.1 + ax * L * 0.08, L * 0.42, hw * 0.62,
                       float(B["muzzle"][2]) * 0.16, m_to)
    out.append((tongue, "jaw"))
    log("mouth parts: inside, teeth up/low, tongue (muzzle %s, mouth %.1f cm)" % (np.round(B["muzzle"], 3).tolist(), L * 100))
    return out


# ====================================================================== the build
def build_body():
    t0 = time.time()
    piece = AN.body_piece(B)
    v, q, tr = piece.mesh()
    log("sdf %d quads %.1fs" % (len(q), time.time() - t0))
    ob = to_object("Body", v, q, tr)
    decimate(ob, LOD_TRIS[0] if not QUICK else 6000)
    return ob, piece


def build_body_gen():
    """The generated body: cleaned, turned and scaled into the breed's frame, the skeleton fitted onto it; a ~400k copy
    kept for baking the normal map; LOD0 decimated to the budget (the generated UVs and texture kept)."""
    t0 = time.time()
    ob = GF.import_glb(GEN_GLB)
    GF.clean(ob)
    img = GF.base_color_image(ob)
    ob.data.materials.clear()
    for poly in ob.data.polygons:
        poly.material_index = 0
    Q = GF.orient(ob, B)
    facts = GF.fit(B, Q)
    B["gen"] = True
    log("fitted", facts)
    # its own eyes (the template's place is only a guess on a generated head): bare of fur there, eyeballs set in them
    found = GF.find_eyes(ob, img, B) if img is not None else None
    if found:
        (cl, rl, nl, ml), (cr, rr, nr, mr) = found
        log("eyes found: left %s r %.1f mm (%d texels), right %s r %.1f mm; template was %s" % (
            np.round(cl, 3), rl * 1000, ml, np.round(cr, 3), rr * 1000, np.round(B["eye"], 3)))
        # each eye where it is (a generated head is seldom quite symmetric); B["eye"] the mean, for the template code
        B["eye"] = (cl + cr * np.array([1, -1, 1])) / 2
        B["eye_r"] = (rl + rr) / 2
        n = (nl + nr * np.array([1, -1, 1])) / 2
        B["eye_n"] = n / np.linalg.norm(n)
        B["eyes_at"] = [(np.array(cl), rl, np.array(nl)), (np.array(cr), rr, np.array(nr))]
    else:
        log("eyes not found in the generated texture: the template's place is used")
    ob.name = "Body"
    high = copy_object(ob, "High")
    decimate(high, 400000)
    if B.get("face_tris"):
        decimate_face(ob, LOD_TRIS[0] + B["face_tris"], B["face_tris"])
    else:
        decimate(ob, LOD_TRIS[0])
    piece = AN.body_piece(B)          # only for the skin weights (and the mouth, nose and eye places)
    log("generated body %d tris, %.1fs" % (sum(len(p.vertices) - 2 for p in ob.data.polygons), time.time() - t0))
    return ob, piece, high, img


def bake_normal(high, low, size, name):
    """The high copy's detail into a tangent-space normal map on the low mesh's UVs (Cycles, selected to active)."""
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.samples = 1
    sc.cycles.device = "CPU"
    im = bpy.data.images.new(name, size, size, alpha=False, float_buffer=False)
    im.colorspace_settings.name = "Non-Color"
    m = bpy.data.materials.new("BakeTarget")
    m.use_nodes = True
    t = m.node_tree.nodes.new("ShaderNodeTexImage")
    t.image = im
    m.node_tree.nodes.active = t
    saved = list(low.data.materials)
    low.data.materials.clear()
    low.data.materials.append(m)
    for o in bpy.context.selected_objects:
        o.select_set(False)
    high.hide_render = False
    high.select_set(True)
    low.select_set(True)
    bpy.context.view_layer.objects.active = low
    H = float(B["joints"]["chest"][2])
    bpy.ops.object.bake(type="NORMAL", use_selected_to_active=True, cage_extrusion=H * 0.03, max_ray_distance=H * 0.06,
                        normal_space="TANGENT", margin=8)
    im.filepath_raw = os.path.join(OUT, name + ".png")
    im.file_format = "PNG"
    im.save()
    low.data.materials.clear()
    for mm in saved:
        low.data.materials.append(mm)
    return im


def image_array(img, size):
    """An image as a (size x size x 3) linear array (row 0 = v 0)."""
    im = img.copy()
    im.scale(size, size)
    px = np.array(im.pixels[:]).reshape(size, size, 4)[..., :3]
    bpy.data.images.remove(im)
    return px


def preview_material(name, color, rough=0.6):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    p = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    p.inputs["Base Color"].default_value = (*color, 1)
    p.inputs["Roughness"].default_value = rough
    return m


def render(name, cam, tgt, lens=50, res=(1000, 750)):
    sc = bpy.context.scene
    for e in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
        try:
            sc.render.engine = e
            break
        except TypeError:
            pass
    sc.render.resolution_x, sc.render.resolution_y = res
    sc.view_settings.view_transform = "AgX"
    w = sc.world or bpy.data.worlds.new("W")
    sc.world = w
    w.use_nodes = True
    bg = next(n for n in w.node_tree.nodes if n.type == "BACKGROUND")
    bg.inputs["Color"].default_value = (0.55, 0.6, 0.66, 1)
    bg.inputs["Strength"].default_value = 0.8
    cd = bpy.data.cameras.get("PCam") or bpy.data.cameras.new("PCam")
    cd.lens = lens
    cd.clip_start = 0.005
    co = bpy.data.objects.get("PCam") or link(bpy.data.objects.new("PCam", cd))
    co.location = cam
    d = mathutils.Vector(tgt) - mathutils.Vector(cam)
    co.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    sc.camera = co
    if not bpy.data.objects.get("PSun"):
        sd = bpy.data.lights.new("PSun", "SUN")
        sd.energy = 3.2
        sd.color = (1.0, 0.93, 0.84)
        sun = link(bpy.data.objects.new("PSun", sd))
        sun.rotation_euler = mathutils.Vector((-0.5, 0.6, -0.65)).to_track_quat("-Z", "Y").to_euler()
    if not bpy.data.objects.get("PGround"):
        bpy.ops.mesh.primitive_plane_add(size=6)
        g = bpy.context.active_object
        g.name = "PGround"
        g.data.materials.append(preview_material("PGround", (0.32, 0.30, 0.26), 0.9))
    sc.render.filepath = os.path.join(OUT, "previews", name + ".png")
    bpy.ops.render.render(write_still=True)
    log("preview", sc.render.filepath)


# ====================================================================== where on the animal (for the coat and the fur)
def part_weights(W):
    def tot(prefixes):
        return sum((w for n, w in W.items() if any(n.startswith(p) for p in prefixes)), np.zeros(len(next(iter(W.values())))))
    return dict(head=tot(["head", "jaw"]), ear=tot(["ear"]), tail=tot(["tail"]),
                body=tot(["pelvis", "spine", "chest", "neck", "scapula", "root"]),
                leg=tot(["upperarm", "forearm", "hand", "thigh", "shin", "foot"]), paw=tot(["paw", "toe"]))


def attributes(P, N, W):
    """Coat and fur attributes of points P (N x 3) with normals N and skin weights W (coats.Attr)."""
    J = B["joints"]
    pw = part_weights(W)
    tb = J["tail"][0][0]
    nose_f = J["head_end"][0]
    s_ = np.clip((P[:, 0] - tb) / (nose_f - tb), 0, 1)
    prof = B["trunk"]
    f0, f1 = B["trunk_f"]
    S = np.array([q[0] for q in prof])
    sf = np.clip((P[:, 0] - f0) / (f1 - f0), 0, 1)
    zc = (np.interp(sf, S, [q[1] for q in prof]) + np.interp(sf, S, [q[2] for q in prof])) * 0.5
    theta = np.arctan2(np.abs(P[:, 1]), P[:, 2] - zc)
    legtop = J["chest"][2] * 0.75
    legt = np.clip(1.0 - P[:, 2] / legtop, 0, 1)
    T = np.array(J["tail"])
    seg = np.linalg.norm(np.diff(T, axis=0), axis=1)
    cum = np.concatenate([[0], np.cumsum(seg)])
    # nearest point on the tail polyline -> arc length
    best = np.full(len(P), 1e9)
    arc = np.zeros(len(P))
    for i in range(len(T) - 1):
        a, b = T[i], T[i + 1]
        d = b - a
        t = np.clip(((P - a) @ d) / (d @ d), 0, 1)
        q = a + t[:, None] * d
        dist = np.linalg.norm(P - q, axis=1)
        m = dist < best
        best = np.where(m, dist, best)
        arc = np.where(m, cum[i] + t * seg[i], arc)
    tailt = arc / cum[-1]
    eye = B["eye"]
    er = B["eye_r"]
    hr = B["head_r"]
    headup = np.clip((P[:, 2] - eye[2]) / (hr[2] * 1.2), -1, 1)
    infront = smooth01((P[:, 0] - (eye[0] - er * 0.5)) / (er * 1.5))
    muzzle = pw["head"] * infront * smooth01((eye[2] - er * 0.6 - P[:, 2]) / (er * 0.8) + 0.5)
    nc = B.get("_nose_c", J["head_end"] + np.array([-B["nose"][0] * 0.6, 0, -B["nose"][2] * 0.2]))
    dn = np.linalg.norm((P - nc) / (np.array(B["nose"]) * (np.array([1.3, 1.2, 1.25]) if CAT else np.array([1.7, 1.45, 1.5]))), axis=1)
    nose = smooth01((1.0 - dn) / (0.5 if CAT else 0.3) + (0.0 if CAT else 0.35)) * (N @ np.array([0.6, 0, 0.4]) > -0.2)
    pad = pw["paw"] * smooth01((-N[:, 2] - 0.35) / 0.3) * (P[:, 2] < B["leg_r"]["paw"][2] * 1.3)
    ear_out = np.stack([np.full(len(P), 0.95), np.sign(P[:, 1]) * 0.3, np.full(len(P), 0.1)], 1)
    inner = pw["ear"] * smooth01(((N * ear_out).sum(1) - 0.1) / 0.4) * smooth01((P[:, 2] - J["ear"][2]) / (er * 0.8) + 0.4)
    corner, front = B["_mouth"]
    ax = (front - corner) / np.linalg.norm(front - corner)
    up = np.cross(np.cross(ax, (0, 0, 1)), ax)
    up = up / np.linalg.norm(up)
    hgt = (P - corner) @ up
    t = (P - corner) @ ax
    lips = smooth01(1.0 - np.abs(hgt) / (B["muzzle"][2] * 0.22)) * (t > -0.01) * (t < np.linalg.norm(front - corner) * 1.3) * pw["head"].clip(0, 1) \
        + smooth01(1.0 - np.abs(hgt) / (B["muzzle"][2] * 0.22)) * pw["head"] * 0
    lips = np.clip(lips + smooth01(1.0 - np.abs(hgt) / (B["muzzle"][2] * 0.22)) * (t > -0.01) * (t < np.linalg.norm(front - corner) * 1.3) * W.get("jaw", 0), 0, 1)
    return CO.Attr(dict(P=P, N=N, s=s_, theta=theta, legt=legt, tailt=tailt, headup=headup, muzzle=np.clip(muzzle, 0, 1),
                        nose=np.clip(nose, 0, 1), pad=np.clip(pad, 0, 1), inner_ear=np.clip(inner, 0, 1), lips=lips,
                        front=(P[:, 0] > 0).astype(float), **pw))


def smooth01(x):
    t = np.clip(x, 0, 1)
    return t * t * (3 - 2 * t)


def fur_field(A):
    """Fur length (m) and comb direction (unit, the way it lies) per point."""
    F = B["fur"]
    L = (A.body * (F["body"] * (1 - smooth01((A.theta - 1.6) / 0.8)) + F["belly"] * smooth01((A.theta - 1.6) / 0.8))
         + A.head * (F["head"] * (1 - A.muzzle) + F["muzzle"] * A.muzzle)
         + A.ear * F["ear"] + A.leg * (F["leg"] * (1 - A.legt) + F["paw"] * A.legt) + A.paw * F["paw"] + A.tail * F["tail"])
    # a ruff round the neck (dogs), fuller cheeks (cats)
    ruff = A.body * smooth01((A.s - 0.72) / 0.08) * smooth01((0.92 - A.s) / 0.06)
    L = L + ruff * (F["neck"] - F["body"])
    cheek = A.head * (1 - A.muzzle) * smooth01((-A.headup + 0.2) / 0.4) * smooth01((np.abs(A.P[:, 1]) - B["head_r"][1] * 0.5) / (B["head_r"][1] * 0.4))
    L = L + cheek * (F["cheek"] - F["head"])
    L = L * (1 - A.nose) * (1 - A.pad * 0.8) * (1 - A.inner_ear * 0.5) * (1 - A.lips * 0.7)
    # bare round the eyes: the lids
    e = B["eye"]
    for k, at in zip((1.0, -1.0), B.get("eyes_at") or [None, None]):
        c, r = (at[0], at[1]) if at is not None else (np.array([e[0], e[1] * k, e[2]]), B["eye_r"])
        d = np.linalg.norm(A.P - c, axis=1) / r
        g = B.get("eye_bare", 1.0)                     # how far out the lids stay bare (in eye radii, x the default)
        L = L * (0.08 + 0.92 * smooth01((d - 1.05 * g) / (0.7 * g)))
    L = np.maximum(L, 0.15) * 0.01
    # comb: back toward the tail and down; down the legs; to the tip of the tail; back over the head; up the ears
    J = B["joints"]
    back = np.tile(np.array([-1.0, 0, -0.35]), (len(A.P), 1))
    down = np.tile(np.array([-0.15, 0, -1.0]), (len(A.P), 1))
    T = np.array(J["tail"])
    tdir = np.tile((T[-1] - T[0]) / np.linalg.norm(T[-1] - T[0]), (len(A.P), 1))
    side = np.stack([np.zeros(len(A.P)), np.sign(A.P[:, 1]), np.zeros(len(A.P))], 1)
    headc = np.tile(np.array([-1.0, 0, 0.1]), (len(A.P), 1)) + side * 0.5
    earc = np.tile(np.array([0.0, 0, 1.0]), (len(A.P), 1))
    belly = smooth01((A.theta - 1.4) / 0.6)[:, None]
    bodyc = back * (1 - belly) + (down * 0.6 + back * 0.4) * belly
    C = (bodyc * A.body[:, None] + down * (A.leg + A.paw)[:, None] + tdir * A.tail[:, None] + headc * A.head[:, None] + earc * A.ear[:, None])
    # flattened onto the skin
    C = C - A.N * (C * A.N).sum(1, keepdims=True)
    C = C / np.maximum(np.linalg.norm(C, axis=1, keepdims=True), 1e-6)
    return L, C


# ====================================================================== textures
def rasterize(ob, attrs_v, size):
    """Interpolates per-vertex arrays (name -> N x k) over the UV layout into size x size maps (+ coverage)."""
    me = ob.data
    me.calc_loop_triangles()
    uvl = me.uv_layers.active.data
    tris = np.array([t.vertices[:] for t in me.loop_triangles])
    loops = np.array([t.loops[:] for t in me.loop_triangles])
    uv = np.zeros(len(uvl) * 2)
    uvl.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    out = {k: np.zeros((size, size, v.shape[1])) for k, v in attrs_v.items()}
    cov = np.zeros((size, size), bool)
    for ti in range(len(tris)):
        tu = uv[loops[ti]] * size - 0.5
        x0, y0 = np.floor(tu.min(0)).astype(int)
        x1, y1 = np.ceil(tu.max(0)).astype(int)
        x0, y0 = max(x0, 0), max(y0, 0)
        x1, y1 = min(x1, size - 1), min(y1, size - 1)
        if x1 < x0 or y1 < y0:
            continue
        ys, xs = np.mgrid[y0:y1 + 1, x0:x1 + 1]
        a, b, c = tu
        d = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
        if abs(d) < 1e-12:
            continue
        w0 = ((b[1] - c[1]) * (xs - c[0]) + (c[0] - b[0]) * (ys - c[1])) / d
        w1 = ((c[1] - a[1]) * (xs - c[0]) + (a[0] - c[0]) * (ys - c[1])) / d
        w2 = 1 - w0 - w1
        m = (w0 >= -0.02) & (w1 >= -0.02) & (w2 >= -0.02)
        if not m.any():
            continue
        yy, xx = ys[m], xs[m]
        ww = np.stack([w0[m], w1[m], w2[m]], 1)
        for k, v in attrs_v.items():
            out[k][yy, xx] = ww @ v[tris[ti]]
        cov[yy, xx] = True
    return out, cov


def bleed(img, cov, it=8):
    """Grows the painted islands outward so mips and seams don't pick up the background."""
    img = img.copy()
    cov = cov.copy()
    for _ in range(it):
        acc = np.zeros_like(img)
        n = np.zeros(cov.shape)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            sc = np.roll(cov, (dy, dx), (0, 1))
            acc += np.roll(img, (dy, dx), (0, 1)) * sc[..., None]
            n += sc
        grow = (~cov) & (n > 0)
        img[grow] = acc[grow] / n[grow][:, None]
        cov = cov | grow
    img[~cov] = img[cov].mean(0)
    return img


def save_png(name, rgb, alpha=None, linear_to_srgb=True):
    h, w = rgb.shape[:2]
    c = np.clip(rgb, 0, 1)
    if linear_to_srgb:
        c = np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)
    a = np.ones((h, w, 1)) if alpha is None else alpha[..., None]
    px = np.concatenate([c, a], 2).astype(np.float32)        # row 0 = v 0 (Blender images are bottom-up, like our maps)
    im = bpy.data.images.new(name, w, h, alpha=True)
    im.pixels.foreach_set(px.ravel())
    im.filepath_raw = os.path.join(OUT, name + ".png")
    im.file_format = "PNG"
    im.save()
    return im


def strand_texture(size=256, seed=5):
    """The fur strands for the shell material, tiling: R = how far up a strand reaches at this texel (0 none .. 1),
    tapering from each strand's root out; G = a random per strand (tint)."""
    rng = np.random.default_rng(seed)
    cell = 2.2
    n = int(size / cell)
    R = np.zeros((size, size))
    G = np.zeros((size, size))
    ys, xs = np.mgrid[0:size, 0:size]
    for layer in range(2):
        cx = (np.arange(n)[None, :] + rng.random((n, n))) * cell
        cy = (np.arange(n)[:, None] + rng.random((n, n))) * cell
        ln = 0.55 + 0.45 * rng.random((n, n)) ** 0.6
        tint = rng.random((n, n))
        rad = cell * (0.75 + 0.2 * rng.random((n, n)))
        for j in range(n):
            for i in range(n):
                x0, y0 = cx[j, i], cy[j, i]
                r = rad[j, i]
                xa, xb = int(x0 - r) - 1, int(x0 + r) + 2
                ya, yb = int(y0 - r) - 1, int(y0 + r) + 2
                yy, xx = np.mgrid[ya:yb, xa:xb]
                d = np.hypot(xx - x0, yy - y0) / r
                v = ln[j, i] * np.clip(1 - d, 0, 1) ** 0.7
                yy %= size
                xx %= size
                better = v > R[yy, xx]
                R[yy[better], xx[better]] = v[better]
                G[yy[better], xx[better]] = tint[j, i]
    return R, G


# ====================================================================== shells
def add_shells(ob, n, L, C, W):
    """Appends n shells of ob's body faces (material 1, vertex colour FurH = height 0..1) and the skin weights for them."""
    me = ob.data
    nv = len(me.vertices)
    V = verts_np(ob)
    Nn = normals_np(ob)
    me.calc_loop_triangles()
    polys = [p.vertices[:] for p in me.polygons]
    uvl = me.uv_layers.active.data
    uv = np.zeros(len(uvl) * 2)
    uvl.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    loop_starts = [p.loop_start for p in me.polygons]
    bm = bmesh.new()
    bm.from_mesh(me)
    uv_layer = bm.loops.layers.uv.active
    col = bm.loops.layers.color.get("FurH") or bm.loops.layers.color.new("FurH")
    # the shell's height also rides in a second UV set (glTF drops a corner colour; Unreal reads TexCoord 1 .x)
    huv = bm.loops.layers.uv.get("FurUV") or bm.loops.layers.uv.new("FurUV")
    for lp in (l for f in bm.faces for l in f.loops):
        lp[col] = (0, 0, 0, 1)
        lp[huv].uv = (0.0, 0.0)
    shell_verts = []
    for i in range(1, n + 1):
        h = i / n
        pos = V + Nn * (L * h)[:, None] + C * (L * 0.75 * h ** 1.6)[:, None]
        vs = [bm.verts.new(tuple(p)) for p in pos]
        shell_verts.append(vs)
        for pi, poly in enumerate(polys):
            try:
                f = bm.faces.new([vs[k] for k in poly])
            except ValueError:
                continue                      # a doubled face the decimation left
            f.material_index = 1
            f.smooth = True
            for li, lp in enumerate(f.loops):
                lp[uv_layer].uv = tuple(uv[loop_starts[pi] + li])
                lp[col] = (h, h, h, 1)
                lp[huv].uv = (h, 0.0)
    bm.to_mesh(me)
    bm.free()
    me.uv_layers.active = me.uv_layers[0]
    me.uv_layers[0].active_render = True
    # skin weights: the same as the vertex the shell grew from
    Wn = {k: np.tile(v, n + 1) for k, v in W.items()}
    return Wn


def add_shells_from(ob, src, n, L, C, W, Wsrc):
    """Shells grown from a coarser copy src (its own fur field and weights), appended to ob: the shells' cost is the
    coarse mesh's, the body under them keeps its detail. Returns ob's weights extended by the shells'."""
    tmp = copy_object(src, "ShellTmp")
    tmp.data.materials.clear()
    for mm in ob.data.materials:
        tmp.data.materials.append(mm)
    for poly in tmp.data.polygons:
        poly.material_index = 0
    Ws = add_shells(tmp, n, L, C, Wsrc)
    # drop the copy's own body faces: keep only the shells (material 1)
    bm = bmesh.new()
    bm.from_mesh(tmp.data)
    nv0 = len(src.data.vertices)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.material_index == 0], context="FACES_ONLY")
    # and the shells over the eyes: the coarse copy's big triangles would spread the fur across them
    for c, r, _n in B.get("eyes_at") or []:
        cv = mathutils.Vector(c)
        bmesh.ops.delete(bm, geom=[f for f in bm.faces if (f.calc_center_median() - cv).length < r * (1.15 if B.get("eye_bare") else 1.9)], context="FACES_ONLY")
    bm.verts.ensure_lookup_table()
    loose = [v for v in bm.verts if not v.link_faces]
    keep_idx = [v.index for v in bm.verts if v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context="VERTS")
    bm.to_mesh(tmp.data)
    bm.free()
    Ws = {k: v[keep_idx] for k, v in Ws.items()}
    nv = len(ob.data.vertices)
    for o2 in bpy.context.selected_objects:
        o2.select_set(False)
    ob.select_set(True)
    tmp.select_set(True)
    bpy.context.view_layer.objects.active = ob
    if not tmp.data.uv_layers.get("FurUV") or not ob.data.uv_layers.get("FurUV"):
        for o3 in (ob, tmp):
            if not o3.data.uv_layers.get("FurUV"):
                o3.data.uv_layers.new(name="FurUV")
    bpy.ops.object.join()
    ob.data.uv_layers.active = ob.data.uv_layers[0]
    ob.data.uv_layers[0].active_render = True
    names = set(W) | set(Ws)
    zeros_a = np.zeros(nv)
    zeros_b = np.zeros(len(keep_idx))
    del nv0
    return {k: np.concatenate([W.get(k, zeros_a), Ws.get(k, zeros_b)]) for k in names}


# ====================================================================== the armature
def build_armature(name="Armature"):
    arm_data = bpy.data.armatures.new(name)
    arm = link(bpy.data.objects.new(name, arm_data))
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    eb = {}
    for bn, head, tail, parent in AN.bones(B):
        e = arm_data.edit_bones.new(bn)
        e.head = tuple(head)
        e.tail = tuple(tail)
        e.roll = 0.0
        if parent:
            e.parent = eb[parent]
            e.use_connect = False
        eb[bn] = e
    bpy.ops.object.mode_set(mode="OBJECT")
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
    return arm


def skin_to(ob, arm, W):
    set_groups(ob, W)
    ob.parent = arm
    mod = ob.modifiers.new("Armature", "ARMATURE")
    mod.object = arm


# ====================================================================== preview materials
def body_material(img):
    m = bpy.data.materials.new("Body")
    m.use_nodes = True
    nt = m.node_tree
    p = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    t = nt.nodes.new("ShaderNodeTexImage")
    t.image = img
    nt.links.new(t.outputs["Color"], p.inputs["Base Color"])
    p.inputs["Roughness"].default_value = 0.8
    return m


def fur_material(img, strands, scale):
    m = bpy.data.materials.new("Fur")
    m.use_nodes = True
    try:
        m.surface_render_method = "DITHERED"
    except Exception:
        m.blend_method = "HASHED"
    nt = m.node_tree
    p = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    tc = nt.nodes.new("ShaderNodeTexCoord")
    mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (scale, scale, 1)
    nt.links.new(tc.outputs["UV"], mp.inputs["Vector"])
    st = nt.nodes.new("ShaderNodeTexImage")
    st.image = strands
    st.interpolation = "Closest"
    nt.links.new(mp.outputs["Vector"], st.inputs["Vector"])
    ca = nt.nodes.new("ShaderNodeVertexColor")
    ca.layer_name = "FurH"
    sep = nt.nodes.new("ShaderNodeSeparateColor")
    nt.links.new(ca.outputs["Color"], sep.inputs["Color"])
    sep2 = nt.nodes.new("ShaderNodeSeparateColor")
    nt.links.new(st.outputs["Color"], sep2.inputs["Color"])
    gt = nt.nodes.new("ShaderNodeMath")
    gt.operation = "GREATER_THAN"
    nt.links.new(sep2.outputs[0], gt.inputs[0])
    nt.links.new(sep.outputs[0], gt.inputs[1])
    nt.links.new(gt.outputs[0], p.inputs["Alpha"])
    t = nt.nodes.new("ShaderNodeTexImage")
    t.image = img
    # roots darker (shadowed among the hairs), tips lighter
    mx = nt.nodes.new("ShaderNodeMix")
    mx.data_type = "RGBA"
    mx.blend_type = "MULTIPLY"
    nt.links.new(sep.outputs[0], mx.inputs["Factor"])
    rm = nt.nodes.new("ShaderNodeMapRange")
    nt.links.new(sep.outputs[0], rm.inputs[0])
    rm.inputs[3].default_value = 0.55
    rm.inputs[4].default_value = 1.05
    mul = nt.nodes.new("ShaderNodeVectorMath")
    mul.operation = "SCALE"
    nt.links.new(t.outputs["Color"], mul.inputs[0])
    nt.links.new(rm.outputs[0], mul.inputs["Scale"])
    nt.links.new(mul.outputs[0], p.inputs["Base Color"])
    p.inputs["Roughness"].default_value = 0.75
    return m


# ====================================================================== clips
def make_actions(arm, lib, only=None):
    rig = lib.r
    rest = {b.name: np.array(b.matrix_local) for b in arm.data.bones}
    rest_inv = {k: np.linalg.inv(v) for k, v in rest.items()}
    acts = {}
    arm.animation_data_create()
    for name in lib.clips:
        if only and name not in only:
            continue
        frames = lib.sample(name, FPS)
        act = bpy.data.actions.new(name)
        act.use_fake_user = True
        arm.animation_data.action = act
        prev = {}
        for fi, pose in enumerate(frames):
            D, Pp = rig.solve(pose)
            T = {}
            for bn in rig.head0:
                M = np.eye(4)
                M[:3, :3] = D[bn]
                M[:3, 3] = Pp[bn] - D[bn] @ rig.head0[bn]
                T[bn] = M
            for bn in rig.head0:
                pb = arm.pose.bones[bn]
                par = rig.parent[bn]
                Tp = T[par] if par else np.eye(4)
                basis = rest_inv[bn] @ np.linalg.inv(Tp) @ T[bn] @ rest[bn]
                loc, q, _ = mathutils.Matrix(basis.tolist()).decompose()
                if bn in prev and prev[bn].dot(q) < 0:
                    q.negate()
                prev[bn] = q.copy()
                sc_ = pose["scale"].get(bn, 1.0)
                psc = pose["scale"].get(par, 1.0) if par else 1.0
                rel = sc_ / psc if psc else 1.0
                pb.location = loc
                pb.rotation_quaternion = q
                pb.scale = (rel, 1.0, rel)
                pb.keyframe_insert("location", frame=fi)
                pb.keyframe_insert("rotation_quaternion", frame=fi)
                pb.keyframe_insert("scale", frame=fi)
        act.frame_range = (0, max(len(frames) - 1, 1))
        act.use_frame_range = True
        acts[name] = act
    return acts


def show(arm, act, frame):
    arm.animation_data.action = act
    bpy.context.scene.frame_set(int(frame))


def main():
    reset()
    bpy.context.scene.render.fps = FPS
    t0 = time.time()
    high = gen_img = None
    if GEN:
        body, piece, high, gen_img = build_body_gen()
    else:
        body, piece = build_body()
        smart_uv(body)
    H = B["joints"]["chest"][2]
    # LODs: decimated copies (they keep the UVs)
    lods = [body]
    for li in (1, 2, 3):
        # a generated body's open seams stop a second collapse early: its LODs come straight from the 400k copy
        o = copy_object(high if GEN else body, "LOD%d" % li)
        decimate(o, LOD_TRIS[li])
        lods.append(o)
    # weights and attributes per LOD
    WL, AL = [], []
    for o in lods:
        if GEN and B.get("kind") == "dog":
            cut_mouth(o)
        Pv, Nv = verts_np(o), normals_np(o)
        W = weights_for(Pv, piece)
        WL.append(W)
        AL.append(attributes(Pv, Nv, W))
    if "--debug-attr" in FLAGS:
        A0d = AL[0]
        Pv = verts_np(lods[0])
        i = int(np.argmax(Pv[:, 0]))
        log("nose max", float(A0d.nose.max()), "nc", B.get("_nose_c"), "nose r", B["nose"], "tip vert", Pv[i], "muzzle max", float(A0d.muzzle.max()))
    log("lods", [sum(len(p.vertices) - 2 for p in o.data.polygons) for o in lods], "%.1fs" % (time.time() - t0))
    # coat textures from LOD0's layout
    A0 = AL[0]
    keys = ["P", "N", "s", "theta", "legt", "tailt", "headup", "muzzle", "nose", "pad", "inner_ear", "lips", "front",
            "head", "ear", "tail", "body", "leg", "paw"]
    av = {k: (getattr(A0, k) if getattr(A0, k).ndim == 2 else getattr(A0, k)[:, None]) for k in keys}
    maps, cov = rasterize(body, av, TEX)
    m = cov
    At = CO.Attr({k: (maps[k][m] if maps[k].shape[2] > 1 else maps[k][m][:, 0]) for k in keys})
    At.N = At.N / np.maximum(np.linalg.norm(At.N, axis=1, keepdims=True), 1e-6)
    scale = H / 0.25
    coat_imgs = []
    ncoats = len(CO.COATS[BREED]) if not QUICK else 1
    gen_px = detail = None
    if GEN and gen_img is not None:
        # the generated texture is the first coat; the others are painted and take its fine detail (the hairs, the
        # shading between them) as a high-pass of its brightness
        gen_px = image_array(gen_img, TEX)
        if gen_img.colorspace_settings.name == "sRGB":
            gen_px = np.where(gen_px <= 0.04045, gen_px / 12.92, ((gen_px + 0.055) / 1.055) ** 2.4)
        lum = gen_px @ np.array([0.2126, 0.7152, 0.0722])
        k = 9
        pad = np.pad(lum, k, mode="edge")
        cs = pad.cumsum(0).cumsum(1)
        box = (cs[2 * k:, 2 * k:] - cs[:-2 * k, 2 * k:] - cs[2 * k:, :-2 * k] + cs[:-2 * k, :-2 * k]) / (2 * k) ** 2
        detail = np.clip(lum / np.maximum(box[:TEX, :TEX], 1e-3), 0.55, 1.6)
    for ci in range(ncoats):
        if gen_px is not None and ci == 0:
            rgb = gen_px.copy()
        else:
            rgb = np.zeros((TEX, TEX, 3))
            rgb[m] = CO.paint(BREED, ci, At, scale)
            rgb = bleed(rgb, cov)
            if detail is not None:
                rgb = rgb * detail[..., None]
        coat_imgs.append(save_png("T_%s_%s" % (BREED, CO.COATS[BREED][ci]), rgb))
        log("coat", CO.COATS[BREED][ci])
    if GEN and high is not None:
        bake_normal(high, body, TEX, "T_%s_Normal" % BREED)
        bpy.data.objects.remove(high)
        log("normal baked %.1fs" % (time.time() - t0))
    for ci in range(len(CO.EYES[BREED]) if not QUICK else 1):
        save_png("T_%s_Eye_%d" % (BREED, ci), CO.eye_texture(BREED, ci))
    R, G = strand_texture()
    strands = save_png("T_%s_Strands" % BREED, np.stack([R, G, np.zeros_like(R)], 2), linear_to_srgb=False)
    log("textures %.1fs" % (time.time() - t0))
    # fur
    arm = build_armature()
    mat_body = body_material(coat_imgs[0])
    strand_scale = B.get("strand_scale", 9.0 if CAT else 7.0)
    mat_fur = fur_material(coat_imgs[0], strands, strand_scale)
    proxy = None
    if GEN and B.get("face_tris"):
        # a face-dense proxy: LOD2's big triangles grew the shells over the head in flat, angular plates
        proxy = copy_object(lods[0], "ShellProxy")
        decimate_face(proxy, 9000, 4000)
        Pv, Nv = verts_np(proxy), normals_np(proxy)
        proxy_W = weights_for(Pv, piece)
        proxy_A = attributes(Pv, Nv, proxy_W)
    elif GEN:
        proxy = copy_object(lods[2], "ShellProxy")
        proxy_W, proxy_A = WL[2], AL[2]
    for li, o in enumerate(lods):
        o.data.materials.append(mat_body)
        o.data.materials.append(mat_fur)
        L, Cc = fur_field(AL[li])
        W = WL[li]
        if LOD_SHELLS[li] and proxy is not None:
            Lp, Cp = fur_field(proxy_A)
            W = add_shells_from(o, proxy, LOD_SHELLS[li], Lp, Cp, W, proxy_W)
        elif LOD_SHELLS[li]:
            W = add_shells(o, LOD_SHELLS[li], L, Cc, W)
        skin_to(o, arm, W)
        o.name = "LOD%d_SK_%s" % (li, BREED)
        if li > 0:
            o.hide_render = True
    eye_mat = preview_material("Eye", (0.5, 0.45, 0.1), 0.1)
    eye_img = bpy.data.images.load(os.path.join(OUT, "T_%s_Eye_0.png" % BREED))
    eye_mat = body_material(eye_img)
    eye_mat.name = "Eye"
    if proxy is not None:
        bpy.data.objects.remove(proxy)
    for ob, bone in ([] if GEN and (B.get("eye_n") is None or B.get("painted_eyes")) else eyes()):
        ob.data.materials.append(eye_mat)
        skin_to(ob, arm, {bone: np.ones(len(ob.data.vertices))})
    if B.get("kind") == "dog":
        for ob, bone in mouth_parts():
            skin_to(ob, arm, bone if isinstance(bone, dict) else {bone: np.ones(len(ob.data.vertices))})
    log("built %.1fs" % (time.time() - t0), "LOD0 with shells", sum(len(p.vertices) - 2 for p in lods[0].data.polygons), "tris")
    if "--previews" in FLAGS or QUICK:
        L = 2.6 * H
        render("fur_side", (0.0, -L * 1.6, H * 1.0), (0.0, 0.0, H * 0.75), lens=55)
        render("fur_34", (L * 1.0, -L * 1.1, H * 1.5), (0.02, 0.0, H * 0.7), lens=55)
        hx = B["joints"]["head"] + (B["joints"]["head_end"] - B["joints"]["head"]) * 0.45
        hx = hx + np.array([0, 0, 0.01 * H / 0.25])
        hd = B["head_r"][1] * 9.0
        render("fur_head", tuple(hx + np.array([hd * 0.85, -hd * 0.55, hd * 0.12])), tuple(hx), lens=85, res=(700, 600))
        render("fur_head_front", tuple(hx + np.array([hd, 0.0, hd * 0.05])), tuple(hx), lens=85, res=(700, 600))

    B["name"] = BREED
    rig = GA.Rig(B)
    lib = GA.Library(rig)
    only = None
    if "--clips" in " ".join(FLAGS):
        only = next(a for a in FLAGS if a.startswith("--clips=")).split("=", 1)[1].split(",")
    acts = make_actions(arm, lib, only)
    log("clips", len(acts), "%.1fs" % (time.time() - t0), {k: round(v, 2) for k, v in lib.speeds.items()})
    if "--poses" in FLAGS:
        L = 2.6 * H
        for name, act in acts.items():
            n = int(act.frame_range[1])
            for fr in ([0, n // 4, n // 2, 3 * n // 4] if lib.clips[name][1] and n > 8 else [n // 2]):
                show(arm, act, fr)
                render("pose_%s_%02d" % (name, fr), (-0.05 * L, -L * 2.3, H * 1.1), (-0.05 * L, 0.0, H * 0.75), lens=50, res=(640, 420))
                if name in ("Sleep", "Rest", "Sit") and fr == (n // 4 if n > 8 else n // 2):
                    render("pose_%s_top" % name, (-0.05 * L, -0.4 * L, L * 2.2), (-0.05 * L, 0.0, 0.0), lens=50, res=(640, 420))

    if "--no-export" in FLAGS:
        return
    export(arm, lods, acts, lib, eye_mat)


def export(arm, lods, acts, lib, eye_mat):
    """One glb (LOD0..3 with the eyes joined in, the armature, every clip), and the rig facts for the importer."""
    arm.animation_data.action = None
    for pb in arm.pose.bones:
        pb.location = (0, 0, 0)
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.scale = (1, 1, 1)
    eyes_src = [o for o in bpy.data.objects if o.name.startswith(("Eye_", "Mouth_"))]
    for li, o in enumerate(lods):
        o.hide_render = False
        parts = [o]
        for e in eyes_src:
            c = copy_object(e, e.name + "_LOD%d" % li)
            if not c.data.uv_layers.get("FurUV"):
                c.data.uv_layers.new(name="FurUV")
            parts.append(c)
        for o2 in bpy.context.selected_objects:
            o2.select_set(False)
        for o2 in parts:
            o2.select_set(True)
        bpy.context.view_layer.objects.active = o
        bpy.ops.object.join()
        if o.data.color_attributes.get("FurH"):
            o.data.color_attributes.active_color = o.data.color_attributes["FurH"]
        # the eye faces have no FurH value: 0
    for e in eyes_src:
        bpy.data.objects.remove(e)
    for o in bpy.data.objects:
        if o.name in ("PCam", "PSun", "PGround"):
            bpy.data.objects.remove(o)
    bpy.ops.object.select_all(action="DESELECT")
    for o in lods:
        o.select_set(True)
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    path = os.path.join(OUT, BREED + ".glb")
    kw = dict(filepath=path, export_format="GLB", use_selection=True, export_yup=True, export_apply=False, export_normals=True,
              export_texcoords=True, export_materials="EXPORT", export_skins=True, export_animations=True,
              export_animation_mode="ACTIONS", export_force_sampling=True, export_optimize_animation_size=False,
              export_vertex_color="ACTIVE", export_all_vertex_colors=False, export_def_bones=False, export_frame_step=1,
              export_anim_single_armature=True, export_image_format="NONE")
    props = bpy.ops.export_scene.gltf.get_rna_type().properties
    kw = {k: v for k, v in kw.items() if k in props or k == "filepath"}
    bpy.ops.export_scene.gltf(**kw)
    log("exported", path, os.path.getsize(path), "bytes")
    J = B["joints"]
    eye = B["eye"]
    rig = {"breed": BREED, "kind": B["kind"], "withers_cm": float(J["chest"][2] * 100 + B["trunk"][7][1] * 0),
           "height_cm": float(max(q[1] for q in B["trunk"]) * 100), "length_cm": float((J["head_end"][0] - J["tail"][0][0]) * 100),
           "eye_l": [float(eye[0] * 100), float(-eye[1] * 100), float(eye[2] * 100)],
           "eyes_at": [[c[0] * 100, -c[1] * 100, c[2] * 100, r * 100, n[0], -n[1], n[2]] for c, r, n in B.get("eyes_at") or []]
           if B.get("painted_eyes") else [],
           "fur": bool(LOD_SHELLS[0]),
           "coats": CO.COATS[BREED], "eyes": len(CO.EYES[BREED]), "strand_scale": B.get("strand_scale", 9.0 if CAT else 7.0),
           "clips": {n: {"seconds": lib.clips[n][0], "loop": lib.clips[n][1]} for n in acts},
           "speeds_cms": {k: round(float(v) * 100, 1) for k, v in lib.speeds.items()},
           "lod_tris": [sum(len(p.vertices) - 2 for p in o.data.polygons) for o in lods], "shells": LOD_SHELLS,
           "tails": [n for n, *_ in AN.bones(B) if n.startswith("tail_")]}
    json.dump(rig, open(os.path.join(OUT, BREED + "_rig.json"), "w"), indent=1)
    log("rig", rig["lod_tris"], rig["speeds_cms"])


main()
