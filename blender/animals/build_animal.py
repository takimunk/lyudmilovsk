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
        apply_mods(ob)
    for poly in ob.data.polygons:
        poly.use_smooth = True


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
    return {n: W[n] / np.maximum(tot, 1e-9) for n in names}


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
    for s, k in AN.SIDES:
        a = math.radians(B["eye_out"])
        look = np.array((math.cos(a), k * math.sin(a), 0.05))
        # set into the socket: only the front cap shows between the lids
        c = np.array((e[0], e[1] * k, e[2])) - look * B["eye_r"] * 0.18
        ob = uv_sphere("Eye_" + s, c, B["eye_r"], look)
        out.append((ob, "head"))
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
    for k in (1.0, -1.0):
        d = np.linalg.norm(A.P - np.array([e[0], e[1] * k, e[2]]), axis=1) / B["eye_r"]
        L = L * (0.08 + 0.92 * smooth01((d - 1.05) / 0.7))
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
    body, piece = build_body()
    H = B["joints"]["chest"][2]
    smart_uv(body)
    # LODs: decimated copies (they keep the UVs)
    lods = [body]
    for li in (1, 2, 3):
        o = copy_object(body, "LOD%d" % li)
        decimate(o, LOD_TRIS[li])
        lods.append(o)
    # weights and attributes per LOD
    WL, AL = [], []
    for o in lods:
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
    for ci in range(ncoats):
        rgb = np.zeros((TEX, TEX, 3))
        rgb[m] = CO.paint(BREED, ci, At, scale)
        rgb = bleed(rgb, cov)
        coat_imgs.append(save_png("T_%s_%s" % (BREED, CO.COATS[BREED][ci]), rgb))
        log("coat", CO.COATS[BREED][ci])
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
    for li, o in enumerate(lods):
        o.data.materials.append(mat_body)
        o.data.materials.append(mat_fur)
        L, Cc = fur_field(AL[li])
        W = WL[li]
        if LOD_SHELLS[li]:
            W = add_shells(o, LOD_SHELLS[li], L, Cc, W)
        skin_to(o, arm, W)
        o.name = "LOD%d_SK_%s" % (li, BREED)
        if li > 0:
            o.hide_render = True
    eye_mat = preview_material("Eye", (0.5, 0.45, 0.1), 0.1)
    eye_img = bpy.data.images.load(os.path.join(OUT, "T_%s_Eye_0.png" % BREED))
    eye_mat = body_material(eye_img)
    eye_mat.name = "Eye"
    for ob, bone in eyes():
        ob.data.materials.append(eye_mat)
        skin_to(ob, arm, {bone: np.ones(len(ob.data.vertices))})
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
    eyes_src = [o for o in bpy.data.objects if o.name.startswith("Eye_")]
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
           "coats": CO.COATS[BREED], "eyes": len(CO.EYES[BREED]), "strand_scale": B.get("strand_scale", 9.0 if CAT else 7.0),
           "clips": {n: {"seconds": lib.clips[n][0], "loop": lib.clips[n][1]} for n in acts},
           "speeds_cms": {k: round(float(v) * 100, 1) for k, v in lib.speeds.items()},
           "lod_tris": [sum(len(p.vertices) - 2 for p in o.data.polygons) for o in lods], "shells": LOD_SHELLS,
           "tails": [n for n, *_ in AN.bones(B) if n.startswith("tail_")]}
    json.dump(rig, open(os.path.join(OUT, BREED + "_rig.json"), "w"), indent=1)
    log("rig", rig["lod_tris"], rig["speeds_cms"])


main()
