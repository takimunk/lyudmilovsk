"""Turns a raw hi3d prop (blender/props_hi3d/raw/<id>/model.glb, ~1-2M triangles, 4-8k textures) into a game mesh:
cleaned, set upright in the game's frame at its real size (props.json height/origin), decimated to the LOD0 budget,
given a fresh non-overlapping UV layout and baked from the high model: base colour, tangent normal and roughness at
the budget's texture size. Writes

  blender/props_hi3d/game/<id>.glb                     the low mesh, one material "M_<id>", UV0 = the bake layout
  blender/props_hi3d/game/T_Hi3d_<id>_{C,N,R}.png      base colour (sRGB), normal (OpenGL), R: roughness + metal mask in G
  blender/props_hi3d/game/<id>.json                    size, triangles, origin, footprint radius (placement reads it)
  blender/props_hi3d/raw/<id>/preview.png              4 views of the result + the UV layout over the colour (check it)

  Blender -b -P blender/props_hi3d/optimize.py -- <id> [<id> ...] [--front-yaw <deg>]

Frame (Blender, Z up, metres): the front of the object (the reference sheet's front view) looks down +X, so in
Unreal (x, -y, z) it looks down +X too: a placement's yaw is the way the statue faces. Origin "base" = centre of the
footprint at the bottom; "top" = centre of the top (a chandelier hangs from it). If the preview shows the wrong side in
front, rerun with --front-yaw 90/180/-90 and write the value into props.json ("front_yaw").
"""
import bpy  # noqa: I001 (first: bmesh needs it when run as the bpy module)
import bmesh
import json
import math
import os
import sys
from mathutils import Matrix, Vector

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
HERE = os.path.join(ROOT, "blender", "props_hi3d")
RAW = os.path.join(HERE, "raw")
OUT = os.path.join(HERE, "game")
HIGH_MAX = 600000          # the bake source is cut to this many triangles first (Cycles memory, bake time)


def log(*a):
    print("HI3DPROP", *a, flush=True)


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def meshes():
    return [o for o in bpy.context.scene.objects if o.type == "MESH"]


def select(objs, active=None):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = active or objs[0]


def tris(o):
    return sum(len(p.vertices) - 2 for p in o.data.polygons)


def bounds(o):
    vs = [o.matrix_world @ v.co for v in o.data.vertices]
    lo = Vector((min(v.x for v in vs), min(v.y for v in vs), min(v.z for v in vs)))
    hi = Vector((max(v.x for v in vs), max(v.y for v in vs), max(v.z for v in vs)))
    return lo, hi


def load(pid):
    bpy.ops.import_scene.gltf(filepath=os.path.join(RAW, pid, "model.glb"))
    objs = meshes()
    select(objs)
    if len(objs) > 1:
        bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for e in [x for x in bpy.context.scene.objects if x.type != "MESH"]:
        bpy.data.objects.remove(e, do_unlink=True)
    o.name = pid + "_high"
    return o


def place(o, s, front_yaw):
    """Real size, front down +X, origin at the base (or top)."""
    o.data.transform(Matrix.Rotation(math.radians(90.0 + front_yaw), 4, "Z"))   # glTF front (-Y here) -> +X
    lo, hi = bounds(o)
    k = s["height"] / max(hi.z - lo.z, 1e-6)
    o.data.transform(Matrix.Scale(k, 4))
    lo, hi = bounds(o)
    if s["origin"] == "top":
        top = [o.matrix_world @ v.co for v in o.data.vertices if v.co.z > hi.z - 0.04 * (hi.z - lo.z)]
        c = sum(top, Vector()) / len(top)
        o.data.transform(Matrix.Translation((-c.x, -c.y, -hi.z)))
    else:
        # the footprint: the bottom 5 % (outstretched arms and leaning parts don't shift the plinth off the origin)
        foot = [o.matrix_world @ v.co for v in o.data.vertices if v.co.z < lo.z + 0.05 * (hi.z - lo.z)]
        fx = [p.x for p in foot]
        fy = [p.y for p in foot]
        o.data.transform(Matrix.Translation((-(min(fx) + max(fx)) / 2, -(min(fy) + max(fy)) / 2, -lo.z)))


def clean(o, size):
    bm = bmesh.new()
    bm.from_mesh(o.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0004 * size)
    # specks: loose islands of a few faces far smaller than the object (hi3d's floating crumbs)
    seen, drop = set(), []
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
        if len(isl) < 40:
            pts = [v.co for g in isl for v in g.verts]
            ext = max((max(p[i] for p in pts) - min(p[i] for p in pts)) for i in range(3))
            if ext < 0.01 * size:
                drop.extend(isl)
    bmesh.ops.delete(bm, geom=list(set(drop)), context="FACES")
    bm.to_mesh(o.data)
    bm.free()
    log("clean: dropped %d speck faces" % len(drop))


def decimate(o, n):
    t = tris(o)
    if t <= n:
        return t
    m = o.modifiers.new("dec", "DECIMATE")
    m.decimate_type = "COLLAPSE"
    m.ratio = n / t
    m.use_collapse_triangulate = True
    if o.vertex_groups.get("keep"):
        # a built plinth stays exact (its 3-5 cm steps would collapse, the low top would sink under the cage and bake
        # black): the decimate leaves the "keep" group alone
        m.vertex_group = "keep"
        m.invert_vertex_group = True
    select([o])
    bpy.ops.object.modifier_apply(modifier=m.name)
    return tris(o)


def flatten_metal(o):
    """For the colour bake: metals and glass would bake black/empty in the diffuse pass, so the high model's shaders
    lose metallic and transmission. What fed Metallic is returned, so metal_mask() can bake it afterwards (stone plinths
    and columns stay dielectric in the game; props.json "metallic" scales the mask)."""
    srcs = []
    for mat in o.data.materials:
        if not mat or not mat.use_nodes:
            continue
        for nd in mat.node_tree.nodes:
            if nd.type != "BSDF_PRINCIPLED":
                continue
            ins = nd.inputs["Metallic"]
            srcs.append((mat, nd, ins.links[0].from_socket if ins.links else None, float(ins.default_value)))
            for name, val in (("Metallic", 0.0), ("Transmission Weight", 0.0), ("Transmission", 0.0), ("Alpha", 1.0)):
                if name in nd.inputs:
                    for l in list(nd.inputs[name].links):
                        mat.node_tree.links.remove(l)
                    nd.inputs[name].default_value = val
    return srcs


def metal_mask(high, low, srcs, img_r, size):
    """Bakes the high model's metallic (as emission) and packs it into the roughness map's green channel:
    T_Hi3d_<id>_R = R roughness, G metal mask (the game: Metallic = G * props.json metallic)."""
    import numpy as np
    for mat, nd, src, val in srcs:
        nt = mat.node_tree
        em = nd.inputs.get("Emission Color") or nd.inputs.get("Emission")
        for l in list(em.links):
            nt.links.remove(l)
        if src is not None:
            nt.links.new(src, em)
        else:
            em.default_value = (val, val, val, 1.0)
        if "Emission Strength" in nd.inputs:
            nd.inputs["Emission Strength"].default_value = 1.0
    m = new_image("metal_mask", size, (0, 0, 0, 1), True)
    mat = low.data.materials[0]
    tn = mat.node_tree.nodes.new("ShaderNodeTexImage")
    tn.image = m
    for n in mat.node_tree.nodes:
        n.select = False
    tn.select = True
    mat.node_tree.nodes.active = tn
    select([high, low], active=low)
    bpy.ops.object.bake(type="EMIT")
    a = np.array(img_r.pixels[:], dtype=np.float32).reshape(-1, 4)
    mm = np.array(m.pixels[:], dtype=np.float32).reshape(-1, 4)
    a[:, 1] = np.clip(mm[:, 0], 0.0, 1.0)
    img_r.pixels[:] = a.ravel()
    mat.node_tree.nodes.remove(tn)
    bpy.data.images.remove(m)
    log("baked M into R.g: mean metal %.2f, metal > 0.5 on %.0f%% of texels" % (float(a[:, 1].mean()),
        100.0 * float((a[:, 1] > 0.5).mean())))


def new_image(name, size, colour, non_color):
    img = bpy.data.images.new(name, size, size, alpha=False, float_buffer=False)
    img.generated_color = colour
    if non_color:
        img.colorspace_settings.name = "Non-Color"
    return img


def bake_target(low, pid, size):
    imgs = {"C": new_image("T_Hi3d_%s_C" % pid, size, (0.5, 0.5, 0.5, 1), False),
            "N": new_image("T_Hi3d_%s_N" % pid, size, (0.5, 0.5, 1.0, 1), True),
            "R": new_image("T_Hi3d_%s_R" % pid, size, (0.6, 0.6, 0.6, 1), True)}
    mat = bpy.data.materials.new("M_" + pid)
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    nodes = {}
    for i, (k, img) in enumerate(imgs.items()):
        tn = nt.nodes.new("ShaderNodeTexImage")
        tn.image = img
        tn.location = (-600, 300 - 300 * i)
        nodes[k] = tn
    low.data.materials.clear()
    low.data.materials.append(mat)
    return mat, imgs, nodes


def bake(high, low, mat, imgs, nodes, size_m):
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    try:
        prefs = bpy.context.preferences.addons["cycles"].preferences
        prefs.compute_device_type = "METAL"
        prefs.get_devices()
        for d in prefs.devices:
            d.use = True
        sc.cycles.device = "GPU"
    except Exception as e:  # noqa: BLE001 - CPU is fine, only slower
        log("GPU not used:", e)
    sc.cycles.samples = 4
    b = sc.render.bake
    b.use_selected_to_active = True
    b.use_cage = False
    b.cage_extrusion = 0.012 * size_m
    b.max_ray_distance = 0.04 * size_m
    b.margin = 16
    b.margin_type = "EXTEND"
    select([high, low], active=low)
    import numpy as np
    near = (b.cage_extrusion, b.max_ray_distance)
    far = (0.03 * size_m, 0.09 * size_m)

    def pixels(img):
        return np.array(img.pixels[:], dtype=np.float32).reshape(-1, 4)

    def run_pass(kind):
        if kind == "DIFFUSE":
            b.use_pass_direct = False
            b.use_pass_indirect = False
            b.use_pass_color = True
        elif kind == "NORMAL":
            b.normal_space = "TANGENT"
        bpy.ops.object.bake(type=kind)

    def make_active(tn):
        for n in mat.node_tree.nodes:
            n.select = False
        tn.select = True
        mat.node_tree.nodes.active = tn

    # hit mask: where a near ray finds the high model. A deeply decimated fold (the muse's drapery and face, the bear's
    # chest) leaves the high surface outside the near cage: those texels bake black. They are filled from a second,
    # wider bake; everywhere else the near one is kept (a wide cage alone grabs neighbouring parts, a lyre onto a chest).
    saved = []
    for m in high.data.materials:
        if not m or not m.use_nodes:
            continue
        for nd in m.node_tree.nodes:
            if nd.type == "BSDF_PRINCIPLED":
                em = nd.inputs.get("Emission Color") or nd.inputs.get("Emission")
                st = nd.inputs.get("Emission Strength")
                saved.append((m, nd, em, [l.from_socket for l in em.links], tuple(em.default_value),
                              st.default_value if st else None))
                for l in list(em.links):
                    m.node_tree.links.remove(l)
                em.default_value = (1.0, 1.0, 1.0, 1.0)
                if st:
                    st.default_value = 1.0
    mask_img = new_image("hit_mask", imgs["C"].size[0], (0, 0, 0, 1), True)
    mtn = mat.node_tree.nodes.new("ShaderNodeTexImage")
    mtn.image = mask_img
    make_active(mtn)
    b.margin = 2
    bpy.ops.object.bake(type="EMIT")
    b.margin = 16
    hit = pixels(mask_img)[:, 0] > 0.5
    mat.node_tree.nodes.remove(mtn)
    bpy.data.images.remove(mask_img)
    for m, nd, em, links, dv, st in saved:
        em.default_value = dv
        for src in links:
            m.node_tree.links.new(src, em)
        if st is not None:
            nd.inputs["Emission Strength"].default_value = st
    # the misses inside islands (a texel the near bake missed but the far one reaches)
    for k, kind in (("C", "DIFFUSE"), ("N", "NORMAL"), ("R", "ROUGHNESS")):
        make_active(nodes[k])
        b.cage_extrusion, b.max_ray_distance = near
        run_pass(kind)
        a = pixels(imgs[k])
        b.cage_extrusion, b.max_ray_distance = far
        run_pass(kind)
        f = pixels(imgs[k])
        f[hit] = a[hit]
        imgs[k].pixels[:] = f.ravel()
        b.cage_extrusion, b.max_ray_distance = near
        log("baked", k, "(%.1f%% of texels from the wide cage)" % (100.0 * (1.0 - hit.mean())) if k == "C" else "")


def wire(mat, nodes, metallic):
    nt = mat.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    nt.links.new(nodes["C"].outputs["Color"], bsdf.inputs["Base Color"])
    sep = nt.nodes.new("ShaderNodeSeparateColor")
    nt.links.new(nodes["R"].outputs["Color"], sep.inputs["Color"])
    nt.links.new(sep.outputs["Red"], bsdf.inputs["Roughness"])
    mul = nt.nodes.new("ShaderNodeMath")
    mul.operation = "MULTIPLY"
    mul.inputs[1].default_value = metallic
    nt.links.new(sep.outputs["Green"], mul.inputs[0])
    nt.links.new(mul.outputs["Value"], bsdf.inputs["Metallic"])
    nm = nt.nodes.new("ShaderNodeNormalMap")
    nt.links.new(nodes["N"].outputs["Color"], nm.inputs["Color"])
    nt.links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])


def preview(low, pid, imgs, s):
    """4 views (front, left, back, three-quarter) and the UV layout drawn over the baked colour, side by side."""
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"       # Eevee needs a GPU context (headless Linux has none); 16 samples is plenty
    sc.cycles.samples = 16
    sc.render.resolution_x = sc.render.resolution_y = 768
    sc.render.film_transparent = False
    world = bpy.data.worlds.new("w")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.75, 0.75, 0.78, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 1.0
    sc.world = world
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 3.0
    sun.rotation_euler = (math.radians(50), 0, math.radians(30))
    sc.collection.objects.link(sun)
    lo, hi = bounds(low)
    c = (lo + hi) / 2
    d = max(hi - lo) * 1.15
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = d
    sc.collection.objects.link(cam)
    sc.camera = cam
    shots = []
    for name, yaw in (("front", 0), ("left", 90), ("back", 180), ("q", 35)):
        a = math.radians(yaw)
        cam.location = c + Vector((math.cos(a), math.sin(a), 0.25)) * d * 3
        cam.rotation_euler = (c - cam.location).to_track_quat("-Z", "Y").to_euler()
        f = os.path.join(RAW, pid, "_shot_%s.png" % name)
        sc.render.filepath = f
        bpy.ops.render.render(write_still=True)
        shots.append(f)
    # the UV layout drawn over the baked colour (numpy only: Blender's python has no PIL, the UV exporter needs a GPU)
    import numpy as np
    S = 768

    def px(path):
        im = bpy.data.images.load(path, check_existing=False)
        if im.size[0] != S or im.size[1] != S:
            im.scale(S, S)
        a = np.array(im.pixels[:], dtype=np.float32).reshape(S, S, 4)
        bpy.data.images.remove(im)
        return a

    tiles = [px(f) for f in shots]
    col = px(os.path.join(OUT, "T_Hi3d_%s_C.png" % pid))
    uv = low.data.uv_layers.active.data
    for poly in low.data.polygons:
        li = list(poly.loop_indices)
        for i in range(len(li)):
            u0, v0 = uv[li[i]].uv
            u1, v1 = uv[li[(i + 1) % len(li)]].uv
            n = int(max(abs(u1 - u0), abs(v1 - v0)) * S) + 2
            t = np.linspace(0.0, 1.0, n)
            xs = np.clip(((u0 + (u1 - u0) * t) * S).astype(int), 0, S - 1)
            ys = np.clip(((v0 + (v1 - v0) * t) * S).astype(int), 0, S - 1)
            col[ys, xs] = (0.0, 1.0, 1.0, 1.0)
    tiles.append(col)
    sheet = np.concatenate(tiles, axis=1)
    out = bpy.data.images.new("preview_" + pid, S * len(tiles), S, alpha=False)
    out.pixels[:] = sheet.ravel()
    out.filepath_raw = os.path.join(RAW, pid, "preview.png")
    out.file_format = "PNG"
    out.save()
    for f in shots:
        os.remove(f)
    log("preview", os.path.relpath(out.filepath_raw, ROOT))


def add_plinth(o, pl):
    """A plinth built here when hi3d left it out (girl_dove came back as the figure alone): the figure, already scaled to
    `height` (props.json: the figure's height), is lifted onto a polished stone block size [x, y, z] m of colour `colour`
    (linear RGB) with a small chamfer-like cap. The block is joined to the high model before the bake, so it is baked
    like the rest."""
    sx, sy, sz = pl["size"]
    o.data.transform(Matrix.Translation((0, 0, sz)))
    # one closed stepped solid (base slab, block, cap): square rings lofted bottom to top. Three overlapping boxes left
    # coplanar inner faces that baked black (the slab's top ring, the block's top under the cap).
    bm = bmesh.new()
    rings = ((0.05, 0.0), (0.05, 0.12), (0.0, 0.12), (0.0, sz - 0.06), (0.03, sz - 0.06), (0.03, sz))
    loops = []
    for grow, z in rings:
        hx, hy = sx / 2 + grow, sy / 2 + grow
        loops.append([bm.verts.new((x, y, z)) for x, y in ((-hx, -hy), (hx, -hy), (hx, hy), (-hx, hy))])
    bm.faces.new(list(reversed(loops[0])))
    for a, b in zip(loops, loops[1:]):
        for i in range(4):
            bm.faces.new((a[i], a[(i + 1) % 4], b[(i + 1) % 4], b[i]))
    bm.faces.new(loops[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new("plinth")
    bm.to_mesh(me)
    bm.free()
    mat = bpy.data.materials.new("plinth_stone")
    mat.use_nodes = True
    b = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    # granite: fine speckle around the colour (the bake carries it; a flat colour read as plastic)
    nt = mat.node_tree
    nz = nt.nodes.new("ShaderNodeTexNoise")
    nz.inputs["Scale"].default_value = 140.0
    nz.inputs["Detail"].default_value = 6.0
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    c = pl["colour"]
    ramp.color_ramp.elements[0].position = 0.35
    ramp.color_ramp.elements[0].color = [x * 0.45 for x in c] + [1.0]
    ramp.color_ramp.elements[1].position = 0.65
    ramp.color_ramp.elements[1].color = [min(1.0, x * 1.5 + 0.03) for x in c] + [1.0]
    nt.links.new(nz.outputs["Fac"], ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], b.inputs["Base Color"])
    b.inputs["Roughness"].default_value = pl.get("roughness", 0.3)
    me.materials.append(mat)
    p = bpy.data.objects.new("plinth", me)
    bpy.context.scene.collection.objects.link(p)
    p.vertex_groups.new(name="keep").add(list(range(len(me.vertices))), 1.0, "REPLACE")
    o.vertex_groups.new(name="keep")
    select([o, p], active=o)
    bpy.ops.object.join()
    log("plinth %.2f x %.2f x %.2f m added" % (sx, sy, sz))


def run(pid, s, front_yaw):
    reset()
    high = load(pid)
    place(high, s, front_yaw)
    if s.get("plinth"):
        add_plinth(high, s["plinth"])
    size = max(bounds(high)[1] - bounds(high)[0])
    clean(high, size)
    t0 = tris(high)
    decimate(high, HIGH_MAX)
    srcs = flatten_metal(high)
    # low: a copy decimated to the budget, smooth shaded, a fresh UV layout (hi3d's own islands don't survive the
    # collapse cleanly: seams smear, so everything is re-baked onto the new layout)
    low = high.copy()
    low.data = high.data.copy()
    low.name = pid
    bpy.context.scene.collection.objects.link(low)
    # hi3d meshes carry thousands of edges shared by 3+ faces (inner fins). Collapse stalls on them (sundial: 1.07M ->
    # 49.9k for a 6k budget), and where it doesn't stall it still leaves folded, self-crossing patches that bake black
    # (the muse's face and neck, marks on the bear cub and the pioneer). So the low copy is voxel-remeshed into a clean
    # manifold shell first and collapsed from that; the bake still comes from the untouched high. A built plinth
    # ("keep" group) would melt into the remesh: those props collapse directly unless that stalls.
    n = decimate(low, s["tris"]) if s.get("plinth") else 10 ** 9
    if n > 1.2 * s["tris"]:
        low.data = high.data.copy()
        m = low.modifiers.new("remesh", "REMESH")
        m.mode = "VOXEL"
        vs = m.voxel_size = 0.003 * size
        select([low])
        bpy.ops.object.modifier_apply(modifier=m.name)
        log("voxel remesh %.1f mm -> %d tris" % (1000 * vs, tris(low)))
        n = decimate(low, s["tris"])
    select([low])
    # smooth, but hard edges (plinth corners, column steps) stay split: a fully smooth box bakes and shades its
    # faces dark and bent (the girl's plinth top came out black)
    bpy.ops.object.shade_smooth_by_angle(angle=math.radians(40))
    # collapse on hi3d's non-manifold shells can turn faces inside out: their bake rays then point away from the high
    # model and the texels come out black (the muse's face and neck). Make the low's normals consistent, outwards.
    bm = bmesh.new()
    bm.from_mesh(low.data)
    before = [f.normal.copy() for f in bm.faces]
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    flipped = sum(1 for f, n in zip(bm.faces, before) if f.normal.dot(n) < 0)
    bm.to_mesh(low.data)
    bm.free()
    log("low normals: %d of %d faces flipped outwards" % (flipped, len(low.data.polygons)))
    me = low.data
    while me.uv_layers:
        me.uv_layers.remove(me.uv_layers[0])
    me.uv_layers.new(name="UVMap")
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.004, area_weight=0.0, scale_to_bounds=False)
    # concave shapes, any rotation, scaled to fill the square (the default box packing left ~35 % of gull_stele's atlas empty)
    bpy.ops.uv.pack_islands(rotate=True, rotate_method="ANY", scale=True, margin_method="FRACTION", margin=0.004,
                            shape_method="CONCAVE")
    bpy.ops.object.mode_set(mode="OBJECT")
    mat, imgs, nodes = bake_target(low, pid, s["tex"])
    bake(high, low, mat, imgs, nodes, size)
    metal_mask(high, low, srcs, imgs["R"], s["tex"])
    os.makedirs(OUT, exist_ok=True)
    for k, img in imgs.items():
        img.filepath_raw = os.path.join(OUT, "T_Hi3d_%s_%s.png" % (pid, k))
        img.file_format = "PNG"
        img.save()
    wire(mat, nodes, s["metallic"])
    bpy.data.objects.remove(high, do_unlink=True)
    select([low])
    bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, pid + ".glb"), use_selection=True, export_format="GLB",
                              export_image_format="NONE", export_yup=True, export_apply=True)
    lo, hi = bounds(low)
    r = max(math.hypot(v.co.x, v.co.y) for v in low.data.vertices)
    info = {"id": pid, "tris": n, "raw_tris": t0, "tex": s["tex"], "origin": s["origin"], "front_yaw": front_yaw,
            "min": [round(x, 3) for x in lo], "max": [round(x, 3) for x in hi], "radius": round(r, 3)}
    json.dump(info, open(os.path.join(OUT, pid + ".json"), "w"), indent=1)
    log("done", json.dumps(info))
    preview(low, pid, imgs, s)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    fy = None
    if "--front-yaw" in argv:
        i = argv.index("--front-yaw")
        fy = float(argv[i + 1])
        del argv[i:i + 2]
    spec = json.load(open(os.path.join(HERE, "props.json")))["props"]
    for pid in argv or sorted(spec, key=lambda k: spec[k]["priority"]):
        if not os.path.exists(os.path.join(RAW, pid, "model.glb")):
            log(pid, "no raw model.glb yet")
            continue
        run(pid, spec[pid], fy if fy is not None else spec[pid].get("front_yaw", 0.0))


if __name__ == "__main__":
    main()
