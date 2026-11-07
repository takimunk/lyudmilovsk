"""Sea-life builder (non-fish underwater animals) for the same import path and material as the fish.

    Blender -b --python blender/sealife/build_sealife.py -- <Id> [--lint] [--preview]
    Blender -b --python blender/sealife/build_sealife.py -- --contact [Id ...]     (export/contact_sealife.png, 2 columns)

Reads blender/sealife/species/<Id>.py (SPEC dict + build(K) composing tubes / domes / plates / eye caps from sealife_geo.Kit),
builds LOD0 and LOD1 (build() called again with K.lod = 1, same rect keys -> same texture), paints <Id>_D.png and writes
blender/fish/export/<Id>.glb, <Id>_LOD1.glb, <Id>_D.png (+ _preview, _close, _uv .png) with the fish conventions, so
unreal/scripts/import_fish.py and M_LyudFish pick them up unchanged.  The Blender object / glb / material / uv-image code is
build_fish.py's own (loaded below without running its main()).
Prints  TRIS <Id> lod0 N lod1 M   SIZE <Id> L x H x W m (+ the measured real length)   LINT <Id> ok|<problems>.
"""
import importlib.util
import math
import os
import sys
import time
import types

HERE = os.path.dirname(os.path.abspath(__file__))
FISH = os.path.join(os.path.dirname(HERE), "fish")
for p in (HERE, FISH):
    if p not in sys.path:
        sys.path.insert(0, p)

import numpy as np  # noqa: E402

import sealife_geo as G  # noqa: E402
import sealife_paint as P  # noqa: E402

OUT = os.path.join(FISH, "export")


def load_build_fish():
    """build_fish.py ends with a bare main() call: execute everything above it as a library module."""
    path = os.path.join(FISH, "build_fish.py")
    src = open(path).read()
    cut = src.rstrip().rsplit("\nmain()", 1)[0]
    mod = types.ModuleType("build_fish_lib")
    mod.__file__ = path
    exec(compile(cut, path, "exec"), mod.__dict__)
    return mod


def load_species(sid):
    path = os.path.join(HERE, "species", sid + ".py")
    spec = importlib.util.spec_from_file_location("sealife_species_" + sid, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    assert mod.SPEC["id"] == sid, "species id must match the file name"
    return mod


def build_parts(mod, lod):
    K = G.Kit(mod.SPEC, lod)
    parts = mod.build(K)
    return K.finish(parts)


def measure(parts, spec):
    """The real-size measure named by SPEC['size'] = (what, metres): 'body_path' (centre-line length of the first body tube),
    'x' / 'y' / 'z' (bounding-box extent of the 'body' parts)."""
    what, want = spec["size"]
    body = [p for p in parts if p.kind == "body"]
    if what == "body_path":
        got = body[0].path.length
    else:
        co = np.concatenate([p.V for p in body])
        got = float(np.ptp(co[:, "xyz".index(what)]))
    return got, want


# ------------------------------------------------------------------ previews
def render_sheets(bf, objs, spec, sid, prev_path, close_path):
    """4-view sheet (left side, top, 3/4 front-left, below) with a scale bar of 10 alternating tenths of `bar` metres,
    and a close-up sheet (head 3/4, head from above, tail 3/4, head from below).  Framing from the bounding box."""
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
    ext = hi - lo
    span = max(ext[0], ext[1])
    bar = 10 ** math.floor(math.log10(span * 0.9))
    bars = []
    for i in range(10):
        bpy.ops.mesh.primitive_cube_add(size=1)
        b = bpy.context.object
        b.scale = (bar / 10, bar * 0.035, bar * 0.035)
        m = bpy.data.materials.new("bar%d" % i)
        colr = (0.02, 0.02, 0.02, 1) if i % 2 else (0.95, 0.95, 0.95, 1)
        m.use_nodes = True
        m.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = colr
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
    extent = max(ext[0], ext[1] * 16 / 9 * 0.9, ext[2] * 16 / 9 * 0.9) * 1.25
    fh = extent * 450 / 800
    D = span * 20 + 1
    V = mathutils.Vector

    def shoot(views, out):
        tiles = []
        for i, v in enumerate(views):
            cam.location = v["loc"]
            cam_d.type = v["type"]
            cam_d.clip_start, cam_d.clip_end = span * 0.002, D * 4
            if v["type"] == "ORTHO":
                cam_d.ortho_scale = extent
                cam.rotation_euler = v["rot"]
            else:
                cam_d.lens = 50
                d = V(v["look"]) - V(v["loc"])
                cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
            for k, b in enumerate(bars):
                b.hide_render = "bar" not in v
                if "bar" in v:
                    b.location = (v["bar"][0] + bar * (-0.45 + 0.1 * k), v["bar"][1], v["bar"][2])
            sun.rotation_euler = cam.rotation_euler.copy()
            sun.rotation_euler.rotate_axis("X", math.radians(-28))
            sun.rotation_euler.rotate_axis("Y", math.radians(24))
            fp = os.path.join(OUT, "_tmp_%s_%d.png" % (sid, i))
            sc.render.filepath = fp
            bpy.ops.render.render(write_still=True)
            im = bpy.data.images.load(fp)
            im.colorspace_settings.name = "Non-Color"
            a = np.array(im.pixels[:], np.float32).reshape(im.size[1], im.size[0], 4)[::-1, :, :3]
            tiles.append(a)
            bpy.data.images.remove(im)
            os.remove(fp)
        P.write_png(out, np.concatenate([np.concatenate(tiles[:2], 1), np.concatenate(tiles[2:4], 1)], 0))

    views = [
        dict(loc=(c[0], c[1] + D, c[2]), rot=(math.pi / 2, 0, math.pi), type="ORTHO", bar=(c[0], hi[1] + bar * 0.05, c[2] - fh * 0.40)),
        dict(loc=(c[0], c[1], c[2] + D), rot=(0, 0, 0), type="ORTHO", bar=(c[0], c[1] - fh * 0.40, hi[2] + bar * 0.05)),
        dict(loc=tuple(V(c) + V((0.55, 0.5, 0.35)).normalized() * span * 1.9), look=tuple(c), type="PERSP"),
        dict(loc=(c[0], c[1], c[2] - D), rot=(math.pi, 0, 0), type="ORTHO", bar=(c[0], c[1] + fh * 0.40, lo[2] - bar * 0.05)),
    ]
    shoot(views, prev_path)
    if close_path:
        head = V((hi[0] - spec.get("close", 0.18) * ext[0], 0, c[2]))
        tail = V((lo[0] + 0.15 * ext[0], 0, c[2]))
        r = span * 0.22
        cl = [
            dict(loc=tuple(head + V((1, 1.1, 0.6)).normalized() * r * 1.7), look=tuple(head), type="PERSP"),
            dict(loc=tuple(head + V((0.3, 0.2, 1)).normalized() * r * 1.7), look=tuple(head), type="PERSP"),
            dict(loc=tuple(tail + V((-1, 1.0, 0.7)).normalized() * r * 1.7), look=tuple(tail), type="PERSP"),
            dict(loc=tuple(head + V((1, -0.3, -0.9)).normalized() * r * 1.7), look=tuple(head), type="PERSP"),
        ]
        shoot(cl, close_path)


def contact(ids):
    from PIL import Image
    tiles = []
    for s in ids:
        p = os.path.join(OUT, s + "_preview.png")
        if os.path.exists(p):
            im = Image.open(p).convert("RGB")
            tiles.append(im.resize((im.width // 2, im.height // 2), Image.LANCZOS))
    if not tiles:
        return
    w, h = tiles[0].size
    rows = (len(tiles) + 1) // 2
    sheet = Image.new("RGB", (w * 2, h * rows), (96, 96, 96))
    for i, t in enumerate(tiles):
        sheet.paste(t, ((i % 2) * w, (i // 2) * h))
    out = os.path.join(OUT, "contact_sealife.png")
    sheet.save(out)
    print("CONTACT", out)


# ------------------------------------------------------------------ main
def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    if argv and argv[0] == "--contact":
        ids = argv[1:] or sorted(f[:-3] for f in os.listdir(os.path.join(HERE, "species")) if f.endswith(".py") and not f.startswith("_"))
        contact(ids)
        return
    import lint_sealife
    sid = argv[0]
    if "--dry" in argv:                       # plain python: geometry, atlas, texture, lint, uv image (no Blender)
        mod = load_species(sid)
        S = mod.SPEC
        bf = load_build_fish()
        parts0, parts1 = build_parts(mod, 0), build_parts(mod, 1)
        layout = G.Layout(parts0)
        layout.apply(parts0)
        layout.apply(parts1)
        lint_sealife.lint(parts0, layout, S, sid + " lod0")
        lint_sealife.lint(parts1, layout, S, sid + " lod1")
        n0, n1 = sum(len(p.F) for p in parts0), sum(len(p.F) for p in parts1)
        got, want = measure(parts0, S)
        co = np.concatenate([p.V for p in parts0])
        d = co.max(0) - co.min(0)
        print("TRIS %s lod0 %d lod1 %d" % (sid, n0, n1))
        print("SIZE %s %.4f x %.4f x %.4f m  (%s %.4f m, real %.4f m)" % (sid, d[0], d[2], d[1], S["size"][0], got, want))
        print("ATLAS %s %d rects, %.0f px/m" % (sid, len(layout.rects), layout.fs))
        if "--tex" in argv:
            tex = P.paint_texture(parts0, layout, S)
            P.write_png(os.path.join(OUT, sid + "_D.png"), tex)
            bf.uv_image(os.path.join(OUT, sid + "_uv.png"), tex, sorted(parts0, key=lambda p: not p.mirror.all()))
        return
    import bpy
    sid = argv[0]
    do_lint = "--lint" in argv
    do_prev = "--preview" in argv
    mod = load_species(sid)
    S = mod.SPEC
    t0 = time.time()
    bf = load_build_fish()
    os.makedirs(OUT, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)

    parts0 = build_parts(mod, 0)
    parts1 = build_parts(mod, 1)
    layout = G.Layout(parts0)
    missing = set(p.rect_key for p in parts1) - set(layout.rects)
    assert not missing, "LOD1 rect keys missing from LOD0: %s" % missing
    layout.apply(parts0)
    layout.apply(parts1)
    tex = P.paint_texture(parts0, layout, S)
    png = os.path.join(OUT, sid + "_D.png")
    P.write_png(png, tex)

    mats, img = bf.make_materials(png)
    meta = dict(length=S["length"], r_len=S.get("r_len", S["length"]), rects={k: list(v) for k, v in layout.rects.items()},
                sealife=True)
    ob0 = bf.make_object(sid, parts0, mats, meta)
    ob1 = bf.make_object(sid + "_LOD1", parts1, mats, meta)
    for ob in (ob0, ob1):
        assert "Col" in ob.data.color_attributes, "no Col attribute"

    n0, n1 = len(ob0.data.polygons), len(ob1.data.polygons)
    problems = {}
    if do_lint:
        r0, _ = lint_sealife.lint(parts0, layout, S, sid + " lod0")
        r1, _ = lint_sealife.lint(parts1, layout, S, sid + " lod1")
        problems = {k: r0[k] + r1[k] for k in r0 if r0[k] + r1[k]}
        tmax = S.get("tris_max", int(S["tris"] * 1.12))
        if n0 > tmax:
            problems["tris_over"] = n0 - tmax
        if not (0.18 <= n1 / n0 <= 0.45):
            problems["lod1_ratio"] = int(round(100 * n1 / n0))
        got, want = measure(parts0, S)
        if abs(got / want - 1) > 0.05:
            problems["size_off_pct"] = int(round(100 * (got / want - 1)))
    bf.strip_lint_attrs(ob0)
    bf.strip_lint_attrs(ob1)
    bf.export_glb(ob0, os.path.join(OUT, sid + ".glb"))
    bf.export_glb(ob1, os.path.join(OUT, sid + "_LOD1.glb"))
    for fnm in (sid + ".glb", sid + "_LOD1.glb"):
        bf.patch_glb(os.path.join(OUT, fnm))

    print("TRIS %s lod0 %d lod1 %d" % (sid, n0, n1))
    co = np.array([v.co[:] for v in ob0.data.vertices])
    d = co.max(0) - co.min(0)
    got, want = measure(parts0, S)
    print("SIZE %s %.4f x %.4f x %.4f m  (%s %.4f m, real %.4f m)" % (sid, d[0], d[2], d[1], S["size"][0], got, want))
    if do_lint:
        print("LINT %s ok" % sid if not problems else "LINT %s %s" % (sid, " ".join("%s %d" % kv for kv in problems.items())))
    print("ATLAS %s %d rects, %.0f px/m" % (sid, len(layout.rects), layout.fs))
    if do_prev:
        bf.uv_image(os.path.join(OUT, sid + "_uv.png"), tex, sorted(parts0, key=lambda p: not p.mirror.all()))
        render_sheets(bf, [ob0], S, sid, os.path.join(OUT, sid + "_preview.png"), os.path.join(OUT, sid + "_close.png"))
        print("PREVIEW", os.path.join(OUT, sid + "_preview.png"))
    print("DONE %s %.1fs" % (sid, time.time() - t0))


if __name__ == "__main__":
    main()
