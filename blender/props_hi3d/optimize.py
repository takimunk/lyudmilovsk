"""Turns a raw hi3d prop (blender/props_hi3d/raw/<id>/model.glb, ~1-2M triangles, 4-8k textures) into a game mesh:
cleaned, set upright in the game's frame at its real size (props.json height/origin), decimated to the LOD0 budget,
given a fresh non-overlapping UV layout and baked from the high model: base colour, tangent normal and roughness at
the budget's texture size. Writes

  blender/props_hi3d/game/<id>.glb                     the low mesh, one material "M_<id>", UV0 = the bake layout
  blender/props_hi3d/game/T_Hi3d_<id>_{C,N,R}.png      base colour (sRGB), normal (OpenGL), roughness (grey)
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
    select([o])
    bpy.ops.object.modifier_apply(modifier=m.name)
    return tris(o)


def flatten_metal(o):
    """For the colour bake: metals and glass would bake black/empty in the diffuse pass, so the high model's shaders
    lose metallic and transmission (the game puts metallic back from props.json)."""
    for mat in o.data.materials:
        if not mat or not mat.use_nodes:
            continue
        for nd in mat.node_tree.nodes:
            if nd.type != "BSDF_PRINCIPLED":
                continue
            for name, val in (("Metallic", 0.0), ("Transmission Weight", 0.0), ("Transmission", 0.0), ("Alpha", 1.0)):
                if name in nd.inputs:
                    for l in list(nd.inputs[name].links):
                        mat.node_tree.links.remove(l)
                    nd.inputs[name].default_value = val


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
    for k, kind in (("C", "DIFFUSE"), ("N", "NORMAL"), ("R", "ROUGHNESS")):
        for n in nodes.values():
            n.select = False
        nodes[k].select = True
        mat.node_tree.nodes.active = nodes[k]
        if kind == "DIFFUSE":
            b.use_pass_direct = False
            b.use_pass_indirect = False
            b.use_pass_color = True
            bpy.ops.object.bake(type="DIFFUSE")
        elif kind == "NORMAL":
            b.normal_space = "TANGENT"
            bpy.ops.object.bake(type="NORMAL")
        else:
            bpy.ops.object.bake(type="ROUGHNESS")
        log("baked", k)


def wire(mat, nodes, metallic):
    nt = mat.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    nt.links.new(nodes["C"].outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(nodes["R"].outputs["Color"], bsdf.inputs["Roughness"])
    nm = nt.nodes.new("ShaderNodeNormalMap")
    nt.links.new(nodes["N"].outputs["Color"], nm.inputs["Color"])
    nt.links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
    bsdf.inputs["Metallic"].default_value = metallic


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


def run(pid, s, front_yaw):
    reset()
    high = load(pid)
    place(high, s, front_yaw)
    size = max(bounds(high)[1] - bounds(high)[0])
    clean(high, size)
    t0 = tris(high)
    decimate(high, HIGH_MAX)
    flatten_metal(high)
    # low: a copy decimated to the budget, smooth shaded, a fresh UV layout (hi3d's own islands don't survive the
    # collapse cleanly: seams smear, so everything is re-baked onto the new layout)
    low = high.copy()
    low.data = high.data.copy()
    low.name = pid
    bpy.context.scene.collection.objects.link(low)
    n = decimate(low, s["tris"])
    select([low])
    bpy.ops.object.shade_smooth()
    me = low.data
    while me.uv_layers:
        me.uv_layers.remove(me.uv_layers[0])
    me.uv_layers.new(name="UVMap")
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.004, area_weight=0.0, scale_to_bounds=False)
    bpy.ops.uv.pack_islands(margin=0.004, rotate=True)
    bpy.ops.object.mode_set(mode="OBJECT")
    mat, imgs, nodes = bake_target(low, pid, s["tex"])
    bake(high, low, mat, imgs, nodes, size)
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
