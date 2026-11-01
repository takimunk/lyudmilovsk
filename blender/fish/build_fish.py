"""Parametric fish builder.

    Blender -b --python blender/fish/build_fish.py -- <Id> [--lint] [--preview]

Reads blender/fish/species/<Id>.py (SPEC dict), builds LOD0 + LOD1 (same UVs / colours, rebuilt at lower resolution),
paints export/<Id>_D.png (1024x512 RGBA) and exports export/<Id>.glb and export/<Id>_LOD1.glb.
Prints   TRIS <Id> lod0 N lod1 M   SIZE <Id> L x H x W m   LINT <Id> ok|<problems>.
"""
import importlib.util
import json
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import numpy as np

import fish_geo as G
import fish_paint as P

OUT = os.path.join(HERE, "export")


def load_spec(sid):
    path = os.path.join(HERE, "species", sid + ".py")
    spec = importlib.util.spec_from_file_location("fish_species_" + sid, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    S = mod.SPEC
    assert S["id"] == sid, "species id must match the file name"
    return S


# ------------------------------------------------------------------ blender objects
def make_materials(png):
    import bpy
    img = bpy.data.images.load(png, check_existing=False)
    img.colorspace_settings.name = "sRGB"
    img.alpha_mode = "STRAIGHT"
    img.pack() if False else None
    mats = {}
    for name in ("Body", "Fins"):
        m = bpy.data.materials.new(name)
        m.use_nodes = True
        nt = m.node_tree
        bsdf = nt.nodes["Principled BSDF"]
        tex = nt.nodes.new("ShaderNodeTexImage")
        tex.image = img
        tex.interpolation = "Linear"
        tex.extension = "CLIP" if False else "EXTEND"
        nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
        if name == "Body":
            bsdf.inputs["Roughness"].default_value = 0.38
        else:
            bsdf.inputs["Roughness"].default_value = 0.6
            nt.links.new(tex.outputs["Alpha"], bsdf.inputs["Alpha"])
            m.use_backface_culling = False
            try:
                m.blend_method = "CLIP"
                m.alpha_threshold = 0.3
            except Exception:
                pass
        mats[name] = m
    return mats, img


def make_object(name, parts, mats, meta):
    import bpy
    V, F, UV, COL, mat, pid, mir = [], [], [], [], [], [], []
    off = 0
    kinds, prect, ppaired = {}, {}, {}
    for k, p in enumerate(parts):
        V.append(p.V)
        F.append(p.F + off)
        UV.append(p.uv)
        COL.append(np.stack([p.R, p.G, p.B, np.ones(len(p.V))], 1))
        mat.append(np.full(len(p.F), 1 if p.kind == "fin" else 0))
        pid.append(np.full(len(p.F), k))
        mir.append(p.mirror)
        kinds[str(k)] = p.kind
        prect[str(k)] = p.rect_key
        ppaired[str(k)] = bool(p.B.max() > 0.5)
        off += len(p.V)
    V, F, UV, COL = (np.concatenate(a) for a in (V, F, UV, COL))
    mat, pid, mir = (np.concatenate(a) for a in (mat, pid, mir))
    me = bpy.data.meshes.new(name)
    me.from_pydata(V.tolist(), [], F.tolist())
    uvl = me.uv_layers.new(name="UVMap")
    uvl.data.foreach_set("uv", UV[F].reshape(-1, 2).ravel())
    ca = me.color_attributes.new("Col", "FLOAT_COLOR", "POINT")
    ca.data.foreach_set("color", COL.ravel())
    me.color_attributes.active_color = ca
    me.color_attributes.render_color_index = 0 if me.color_attributes[0].name == "Col" else list(me.color_attributes.keys()).index("Col")
    a = me.attributes.new("part", "INT", "FACE")
    a.data.foreach_set("value", pid.astype(np.int32))
    a = me.attributes.new("mirror", "BOOLEAN", "FACE")
    a.data.foreach_set("value", mir.astype(bool))
    me.polygons.foreach_set("material_index", mat.astype(np.int32))
    me.materials.append(mats["Body"])
    me.materials.append(mats["Fins"])
    try:
        me.polygons.foreach_set("use_smooth", np.ones(len(F), bool))
    except Exception:
        pass
    me.update()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    meta = dict(meta, part_kinds=kinds, part_rect=prect, part_paired=ppaired)
    ob["fish_meta"] = json.dumps(meta)
    return ob


def strip_lint_attrs(ob):
    for n in ("part", "mirror"):
        if n in ob.data.attributes:
            ob.data.attributes.remove(ob.data.attributes[n])


def export_glb(ob, path):
    import bpy
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    props = bpy.ops.export_scene.gltf.get_rna_type().properties
    kw = dict(filepath=path, export_format="GLB", use_selection=True, export_yup=True, export_apply=False, export_materials="EXPORT",
              export_normals=True, export_texcoords=True, export_vertex_color="ACTIVE", export_all_vertex_colors=False,
              export_active_vertex_color_when_no_material=True, export_attributes=False, export_animations=False,
              export_cameras=False, export_lights=False, export_image_format="AUTO", export_extras=False)
    bpy.ops.export_scene.gltf(**{k: v for k, v in kw.items() if k in props or k == "filepath"})


def patch_glb(path):
    """Fins -> alphaMode MASK (cutoff 0.3, two-sided); Body -> opaque, single-sided."""
    import struct
    raw = open(path, "rb").read()
    n = struct.unpack("<I", raw[12:16])[0]
    j = json.loads(raw[20:20 + n])
    for m in j["materials"]:
        if m["name"].startswith("Fins"):
            m["alphaMode"], m["alphaCutoff"], m["doubleSided"] = "MASK", 0.3, True
        else:
            m.pop("alphaMode", None)
            m["doubleSided"] = False
    js = json.dumps(j, separators=(",", ":")).encode()
    js += b" " * (-len(js) % 4)
    rest = raw[20 + n:]
    out = raw[:12] + struct.pack("<I", len(js)) + raw[16:20] + js + rest
    out = out[:8] + struct.pack("<I", len(out)) + out[12:]
    open(path, "wb").write(out)


# ------------------------------------------------------------------ preview
def uv_image(path, tex, parts):
    """Texture (2x) with the LOD0 uv edges: cyan = real faces, magenta = mirrored copies, plus the rect outlines."""
    t = tex[..., :3] * tex[..., 3:4] + 0.35 * (1 - tex[..., 3:4])
    img = np.repeat(np.repeat(t, 2, 0), 2, 1) * 0.8
    h, w = img.shape[:2]

    def line(a, b, col):
        n = int(max(abs(b[0] - a[0]), abs(b[1] - a[1]))) + 1
        xs = np.linspace(a[0], b[0], n).astype(int)
        ys = np.linspace(a[1], b[1], n).astype(int)
        ok = (xs >= 0) & (xs < w) & (ys >= 0) & (ys < h)
        img[ys[ok], xs[ok]] = col
    for p in parts:
        U = p.uv
        X = U[:, 0] * w
        Y = (1 - U[:, 1]) * h
        for k in np.argsort(-p.mirror.astype(int), kind="stable"):
            f = p.F[k]
            col = (1.0, 0.2, 0.9) if p.mirror[k] else (0.1, 0.95, 1.0)
            for a, b in ((0, 1), (1, 2), (2, 0)):
                line((X[f[a]], Y[f[a]]), (X[f[b]], Y[f[b]]), col)
    P.write_png(path, img)


def render_preview(path, objs, L, name, close_path=None):
    """Preview sheets: 4 views (left side, top, 3/4 front-left, belly / blind side) with a 10 cm scale bar, and a close-up sheet."""
    import bpy
    import mathutils
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    sc.render.resolution_x, sc.render.resolution_y = 800, 450
    sc.render.resolution_percentage = 100
    sc.view_settings.view_transform = "Standard"
    sc.view_settings.look = "None"
    try:
        sc.eevee.taa_render_samples = 24
    except Exception:
        pass
    w = bpy.data.worlds.new("w")
    w.use_nodes = True
    bg = w.node_tree.nodes["Background"]
    bg.inputs[0].default_value = (0.21, 0.21, 0.21, 1)
    bg.inputs[1].default_value = 1.0
    sc.world = w
    for m in bpy.data.materials:
        if m.name.startswith("Fins"):
            try:
                m.blend_method = "BLEND"
                m.surface_render_method = "DITHERED"
            except Exception:
                pass
    co = np.concatenate([np.array([v.co[:] for v in o.data.vertices]) for o in objs])
    lo, hi = co.min(0), co.max(0)
    c = (lo + hi) / 2
    bars = []
    for i in range(10):
        bpy.ops.mesh.primitive_cube_add(size=1)
        b = bpy.context.object
        b.scale = (0.01, 0.0035, 0.0035)
        m = bpy.data.materials.new("bar%d" % i)
        col = (0.02, 0.02, 0.02, 1) if i % 2 else (0.95, 0.95, 0.95, 1)
        m.diffuse_color = col
        m.use_nodes = True
        m.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = col
        b.data.materials.append(m)
        bars.append(b)
    cam_d = bpy.data.cameras.new("cam")
    cam = bpy.data.objects.new("cam", cam_d)
    sc.collection.objects.link(cam)
    sc.camera = cam
    sun_d = bpy.data.lights.new("sun", "SUN")
    sun_d.energy = 3.2
    sun = bpy.data.objects.new("sun", sun_d)
    sc.collection.objects.link(sun)
    extent = max(L * 1.22, 0.13)
    fh = extent * 450 / 800
    D = 3.0

    def shoot(views, out):
        tmp = []
        for i, v in enumerate(views):
            cam.location = v["loc"]
            cam_d.type = v["type"]
            cam_d.clip_start, cam_d.clip_end = 0.002, 10
            if v["type"] == "ORTHO":
                cam_d.ortho_scale = v.get("scale", extent)
                cam.rotation_euler = v["rot"]
            else:
                cam_d.lens = 50
                d = mathutils.Vector(v["look"]) - mathutils.Vector(v["loc"])
                cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
            for k, b in enumerate(bars):
                b.hide_render = "bar" not in v
                if "bar" in v:
                    b.location = (v["bar"][0] + (-0.045 + 0.01 * k), v["bar"][1], v["bar"][2])
            sun.rotation_euler = cam.rotation_euler.copy()
            sun.rotation_euler.rotate_axis("X", math.radians(-28))
            sun.rotation_euler.rotate_axis("Y", math.radians(24))
            fp = os.path.join(OUT, "_tmp_%s_%d.png" % (name, i))
            sc.render.filepath = fp
            bpy.ops.render.render(write_still=True)
            tmp.append(fp)
        tiles = []
        for fp in tmp:
            im = bpy.data.images.load(fp)
            im.colorspace_settings.name = "Non-Color"
            a = np.array(im.pixels[:], np.float32).reshape(im.size[1], im.size[0], 4)[::-1, :, :3]
            tiles.append(a)
            bpy.data.images.remove(im)
            os.remove(fp)
        P.write_png(out, np.concatenate([np.concatenate(tiles[:2], 1), np.concatenate(tiles[2:4], 1)], 0))

    V = mathutils.Vector
    views = [
        dict(loc=(c[0], c[1] + D, c[2]), rot=(math.pi / 2, 0, math.pi), type="ORTHO", bar=(c[0], hi[1] + 0.003, c[2] - fh * 0.38)),
        dict(loc=(c[0], c[1], c[2] + D), rot=(0, 0, 0), type="ORTHO", bar=(c[0], c[1] - fh * 0.38, hi[2] + 0.003)),
        dict(loc=tuple(V(c) + V((L * 0.55, L * 0.5, L * 0.3)).normalized() * L * 1.9), look=tuple(c), type="PERSP",
             bar=(c[0], hi[1] + 0.02, lo[2])),
        dict(loc=(c[0], c[1], c[2] - D), rot=(math.pi, 0, 0), type="ORTHO", bar=(c[0], c[1] + fh * 0.38, lo[2] - 0.003)),
    ]
    shoot(views, path)
    if close_path:
        hx = 0.5 * L - 0.09 * L
        tx = -0.5 * L + 0.88 * L * 0.0 + (-0.5 * L * 0.0) - L * 0.40
        head = V((hx, 0, 0))
        tail = V((-L * 0.40, 0, 0))
        r = L * 0.22
        cl = [
            dict(loc=tuple(head + V((1, 1.1, 0.5)).normalized() * r * 1.5), look=tuple(head), type="PERSP"),
            dict(loc=tuple(head + V((0, 1, 0)) * r * 1.6), look=tuple(head), type="PERSP"),
            dict(loc=tuple(tail + V((-1, 1.0, 0.5)).normalized() * r * 1.6), look=tuple(tail), type="PERSP"),
            dict(loc=tuple(head + V((1, -0.3, -0.9)).normalized() * r * 1.6), look=tuple(head), type="PERSP"),
        ]
        shoot(cl, close_path)


# ------------------------------------------------------------------ main
def main():
    import bpy
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    sid = argv[0]
    do_lint = "--lint" in argv
    do_prev = "--preview" in argv
    S = load_spec(sid)
    L = S["length"]
    os.makedirs(OUT, exist_ok=True)
    t0 = time.time()
    bpy.ops.wm.read_factory_settings(use_empty=True)

    layout = G.plan_layout(S)
    lod0 = G.tuned_lod(S, 0, S.get("tris", 2000))
    lod1 = G.tuned_lod(S, 1, int(S.get("tris", 2000) * 0.30))
    body, parts0 = G.finalize(S, 0, layout, lod0)
    _, parts1 = G.finalize(S, 1, layout, lod1)
    tex = P.paint_texture(S, body, layout)
    png = os.path.join(OUT, sid + "_D.png")
    P.write_png(png, tex)

    mats, img = make_materials(png)
    meta = dict(length=L, tend=body.tend, rects={k: list(v) for k, v in layout.rects.items()}, body_h=layout.body_h, flat=bool(S.get("flat")))
    ob0 = make_object(sid, parts0, mats, meta)
    ob1 = make_object(sid + "_LOD1", parts1, mats, meta)
    ob1.hide_viewport = False

    summary = []
    t0s = len(ob0.data.polygons)
    t1s = len(ob1.data.polygons)
    problems = None
    if do_lint:
        import lint_fish
        res0, line0 = lint_fish.lint(lint_fish.from_object(ob0), sid + " lod0", verbose=True)
        res1, line1 = lint_fish.lint(lint_fish.from_object(ob1), sid + " lod1", verbose=True)
        bad = {k: res0[k] + res1[k] for k in res0 if res0[k] + res1[k]}
        problems = bad
    strip_lint_attrs(ob0)
    strip_lint_attrs(ob1)
    export_glb(ob0, os.path.join(OUT, sid + ".glb"))
    export_glb(ob1, os.path.join(OUT, sid + "_LOD1.glb"))
    for fn in (sid + ".glb", sid + "_LOD1.glb"):
        patch_glb(os.path.join(OUT, fn))

    print("TRIS %s lod0 %d lod1 %d" % (sid, t0s, t1s))
    co = np.array([v.co[:] for v in ob0.data.vertices])
    d = co.max(0) - co.min(0)
    print("SIZE %s %.3f x %.3f x %.3f m" % (sid, d[0], d[2], d[1]))
    if do_lint:
        print("LINT %s ok" % sid if not problems else "LINT %s %s" % (sid, " ".join("%s %d" % kv for kv in problems.items())))
    if do_prev:
        uv_image(os.path.join(OUT, sid + "_uv.png"), tex, parts0)
        render_preview(os.path.join(OUT, sid + "_preview.png"), [ob0], L, sid, os.path.join(OUT, sid + "_close.png"))
        print("PREVIEW", os.path.join(OUT, sid + "_preview.png"))
    print("DONE %s %.1fs layout kv %.2f fs %.2f" % (sid, time.time() - t0, layout.kv, layout.fs))


main()
