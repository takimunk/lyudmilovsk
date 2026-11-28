"""The hero's haircuts and cap (docs/look.md): shells over the hero's own skull, for the shadow's head.

The hero is seen from inside, so his head is only ever a shadow (UHeroShadowMesh). The barber's cuts and the cap are
therefore plain low meshes that cast it: ULyudLook hides the body's own grown-out hair on the shadow and puts one of
these on the head bone. They are built from the hero's export, never modelled by hand, so they fit whatever head the
hero build gives:

  blender -b -P blender/hero/build_looks.py -- [--glb blender/hero/export/human_hero.glb] [--preview]
  python3 blender/hero/build_looks.py --standin        (pip bpy: a stand-in head, the cloud's smoke test)

  blender/hero/export/looks/SM_HeroHair_crop.glb      clippers all over: 3 mm
  blender/hero/export/looks/SM_HeroHair_short.glb     short back and sides: 18 mm on top, 4 mm below the temples
  blender/hero/export/looks/SM_HeroHair_parted.glb    a side parting: 30 mm on top swept to his left, 14 mm at the sides
  blender/hero/export/looks/SM_HeroCap.glb            a flat cap: a crown pulled forward and a short peak
  blender/hero/export/looks/looks_report.json         sizes, triangle counts and the UV checks of each

Each mesh is in the body's own space (the head where the body's bind pose has it), one material, one UV island per
part, every island inside 0..1 and none over another (checked here: the build fails otherwise). The output is local
only, like the hero's export; unreal/scripts/import_looks.sh brings it into /Game/Characters/Hero/Look.

How a shell is made: a dome of rings around the skull's top is shrink-wrapped onto the head (the skin only: the body's
own hair and lashes are left out), cut at a hairline that runs high over the forehead, past the temples above the ears
and low at the nape, then pushed out along the skull's normals by the cut's length. The front is where the face is
(found from the nose: the head's foremost point), so the script does not care which way the export looks.
"""
import json
import math
import os
import sys

import bpy
import bmesh
from mathutils import Vector
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
STANDIN = "--standin" in ARGS
PREVIEW = "--preview" in ARGS
GLB = os.path.join(HERE, "export", "human_hero.glb")
if "--glb" in ARGS:
    GLB = os.path.abspath(ARGS[ARGS.index("--glb") + 1])
OUT = os.path.join(HERE, "export", "looks_standin" if STANDIN else "looks")
if "--out" in ARGS:
    OUT = os.path.abspath(ARGS[ARGS.index("--out") + 1])

HEAD_BONES = ("head",)
NOT_SKULL = ("hair", "eyebrow", "eyelash", "lash", "brow")
SEGMENTS = 40          # around the head
RINGS = 14             # from the crown down

# The hairline, as the angle down from the crown (degrees) at the front, the sides and the back.
HAIRLINE = {"front": 52.0, "side": 86.0, "back": 118.0}
# The cap sits higher, level all round, a little lower at the back.
CAPLINE = {"front": 58.0, "side": 66.0, "back": 78.0}

# top / side: the hair's length there (m); sweep: the top pushed sideways (m, to his left); fringe: degrees the front
# hairline comes down on the parting's far side.
CUTS = {
    "crop": {"top": 0.003, "side": 0.003, "sweep": 0.0, "fringe": 0.0},
    "short": {"top": 0.018, "side": 0.004, "sweep": 0.0, "fringe": 0.0},
    "parted": {"top": 0.030, "side": 0.014, "sweep": 0.012, "fringe": 9.0},
}
CAP = {"thick": 0.008, "forward": 0.016, "flat": 0.55, "peak_len": 0.062, "peak_half": 0.074, "peak_drop": 0.016}


def log(*a):
    print("LOOKS", *a, flush=True)


# ------------------------------------------------------------------------------ the head
def clear_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def standin_head():
    """A head-sized egg with a nose, 1.62 m up, weighted to a bone called head: enough to run every step."""
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=32, v_segments=20, radius=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * 0.078, v.co.y * 0.097, v.co.z * 0.115))
        if v.co.y < -0.085 and abs(v.co.z + 0.01) < 0.03 and abs(v.co.x) < 0.02:
            v.co.y -= 0.022                                   # the nose: the front is -Y
        v.co.z += 1.62
    me = bpy.data.meshes.new("standin_head")
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new("standin_head", me)
    bpy.context.scene.collection.objects.link(ob)
    me.materials.append(bpy.data.materials.new("Skin"))
    ob.vertex_groups.new(name="head").add(range(len(me.vertices)), 1.0, "REPLACE")
    return [ob]


def load_head():
    if STANDIN:
        return standin_head()
    if not os.path.exists(GLB):
        raise SystemExit("LOOKS no hero export at %s (blender/hero/build_hero.py makes it; --standin for a test head)" % GLB)
    bpy.ops.import_scene.gltf(filepath=GLB)
    return [o for o in bpy.context.scene.objects if o.type == "MESH"]


def skull_mesh(objs):
    """The head's skin as one world-space mesh: faces whose verts all belong to the head bone, without hair and lashes."""
    out = bmesh.new()
    n = 0
    for ob in objs:
        groups = {g.index for g in ob.vertex_groups if g.name.lower() in HEAD_BONES}
        if not groups:
            continue
        me = ob.data
        bad = {i for i, m in enumerate(me.materials) if m and any(w in m.name.lower() for w in NOT_SKULL)}
        on_head = [False] * len(me.vertices)
        for v in me.vertices:
            w = sum(g.weight for g in v.groups if g.group in groups)
            on_head[v.index] = w > 0.5
        mw = ob.matrix_world
        made = {}
        for p in me.polygons:
            if p.material_index in bad or not all(on_head[i] for i in p.vertices):
                continue
            vs = []
            for i in p.vertices:
                if (ob.name, i) not in made:
                    made[(ob.name, i)] = out.verts.new(mw @ me.vertices[i].co)
                vs.append(made[(ob.name, i)])
            try:
                out.faces.new(vs)
                n += 1
            except ValueError:
                pass
    if n < 50:
        raise SystemExit("LOOKS found no head: %d faces weighted to %s" % (n, HEAD_BONES))
    out.verts.ensure_lookup_table()
    out.normal_update()
    return out


class Head:
    """Where the skull is: its top, its middle, how wide and deep, and which way the face looks."""

    def __init__(self, bm):
        self.bvh = BVHTree.FromBMesh(bm)
        co = [v.co for v in bm.verts]
        self.top = max(c.z for c in co)
        low = min(c.z for c in co)
        self.height = self.top - low
        # the brain case: the upper 45% of the head (the jaw and the neck's rim would pull the middle down and forward)
        upper = [c for c in co if c.z > self.top - 0.45 * self.height]
        xs, ys = [c.x for c in upper], [c.y for c in upper]
        self.cx, self.cy = 0.5 * (min(xs) + max(xs)), 0.5 * (min(ys) + max(ys))
        self.rx, self.ry = 0.5 * (max(xs) - min(xs)), 0.5 * (max(ys) - min(ys))
        self.rz = 0.45 * self.height
        self.c = Vector((self.cx, self.cy, self.top - self.rz))
        # the face: the side where the lower head (the nose, the chin) reaches further from the middle
        lower = [c for c in co if c.z < self.top - 0.45 * self.height]
        fy = max(lower, key=lambda c: abs(c.y - self.cy)) if lower else Vector((0, -1, 0))
        fx = max(lower, key=lambda c: abs(c.x - self.cx)) if lower else Vector((0, 0, 0))
        if abs(fy.y - self.cy) - self.ry >= abs(fx.x - self.cx) - self.rx:
            self.front = Vector((0.0, math.copysign(1.0, fy.y - self.cy), 0.0))
        else:
            self.front = Vector((math.copysign(1.0, fx.x - self.cx), 0.0, 0.0))
        self.left = Vector((0, 0, 1)).cross(self.front)          # his own left

    def on_skull(self, direction):
        """The skull's surface and normal seen from far out along a direction from the middle."""
        hit = self.bvh.ray_cast(self.c + direction * 1.0, -direction, 2.0)
        if hit[0] is None:
            near = self.bvh.find_nearest(self.c + direction * max(self.rx, self.ry, self.rz))
            return near[0], direction
        n = hit[1] if hit[1].dot(direction) > 0 else -hit[1]
        return hit[0], n


def line_at(line, cos_front):
    """The hairline's angle from the crown at a bearing (cos_front: 1 the forehead, 0 the temples, -1 the nape)."""
    if cos_front >= 0:
        return line["side"] + (line["front"] - line["side"]) * cos_front ** 1.5
    return line["side"] + (line["back"] - line["side"]) * (-cos_front) ** 1.2


def dome(head, line, fringe=0.0):
    """Rings from the crown down to the line, on the skull. Returns (bmesh, per-vert (polar 0..1, bearing cos, side))."""
    bm = bmesh.new()
    info = {}
    crown_p, crown_n = head.on_skull(Vector((0, 0, 1)))
    crown = bm.verts.new(crown_p)
    info[crown] = (0.0, 0.0, 0.0, crown_n)
    rings = []
    for r in range(1, RINGS + 1):
        ring = []
        for s in range(SEGMENTS):
            a = 2.0 * math.pi * s / SEGMENTS
            flat = head.front * math.cos(a) + head.left * math.sin(a)
            limit = line_at(line, math.cos(a))
            if fringe and math.cos(a) > 0:
                # the fringe falls over the forehead on the side the hair is combed to
                limit += fringe * max(0.0, math.sin(a) * 0.6 + 0.4) * math.cos(a)
            polar = math.radians(limit) * r / RINGS
            d = (Vector((0, 0, 1)) * math.cos(polar) + flat * math.sin(polar)).normalized()
            # an ellipsoid's direction, so the rings keep their spacing on a long head
            d = Vector((d.x * head.rx, d.y * head.ry, d.z * head.rz)).normalized()
            p, n = head.on_skull(d)
            v = bm.verts.new(p)
            info[v] = (r / RINGS, math.cos(a), math.sin(a), n)
            ring.append(v)
        rings.append(ring)
    for s in range(SEGMENTS):
        bm.faces.new((crown, rings[0][s], rings[0][(s + 1) % SEGMENTS]))
    for r in range(RINGS - 1):
        for s in range(SEGMENTS):
            t = (s + 1) % SEGMENTS
            bm.faces.new((rings[r][s], rings[r + 1][s], rings[r + 1][t], rings[r][t]))
    bm.normal_update()
    return bm, info


def polar_uv(bm, info, box):
    """One island: the dome flattened from above into box (u0, v0, u1, v1). A dome never folds under this."""
    uv = bm.loops.layers.uv.verify()
    u0, v0, u1, v1 = box
    for f in bm.faces:
        for lp in f.loops:
            polar, c, s, _ = info[lp.vert]
            r = 0.49 * polar
            lp[uv].uv = (u0 + (u1 - u0) * (0.5 + r * s), v0 + (v1 - v0) * (0.5 + r * c))


def build_hair(head, name, cut):
    bm, info = dome(head, HAIRLINE, cut["fringe"])
    for v, (polar, c, s, n) in info.items():
        # long on top, the sides' length from the temples down; thinning to nothing at the very edge
        top = 1.0 - min(1.0, max(0.0, (polar - 0.45) / 0.3))
        top = top * top * (3 - 2 * top)
        length = cut["side"] + (cut["top"] - cut["side"]) * top
        edge = min(1.0, (1.0 - polar) / 0.12) if polar > 0.88 else 1.0
        v.co = v.co + n * (length * (0.35 + 0.65 * edge))
        v.co += head.left * (cut["sweep"] * top)
    polar_uv(bm, info, (0.02, 0.02, 0.98, 0.98))
    return finish(bm, name, "Hair", (0.09, 0.07, 0.05))


def build_cap(head, name):
    bm, info = dome(head, CAPLINE)
    crown_z = head.top + CAP["thick"]
    for v, (polar, c, s, n) in info.items():
        v.co = v.co + n * CAP["thick"]
        # the crown is flat and pulled forward over the peak
        over = v.co.z - (crown_z - CAP["flat"] * head.rz * 0.25)
        if over > 0:
            v.co.z -= over * CAP["flat"]
        v.co += head.front * (CAP["forward"] * (1.0 - polar) + 0.01 * max(0.0, c) * polar)
        v.co += n * (0.006 * math.sin(math.pi * min(1.0, polar * 1.2)))        # the crown's fullness
    polar_uv(bm, info, (0.02, 0.36, 0.66, 0.99))
    uv = bm.loops.layers.uv.verify()
    # the peak: a short stiff tongue from the front edge, drooping a little
    rim = [v for v, i in info.items() if abs(i[0] - 1.0) < 1e-6 and i[1] > 0.0]
    rim.sort(key=lambda v: info[v][2])
    outer = []
    for v in rim:
        c, s = info[v][1], info[v][2]
        reach = CAP["peak_len"] * c ** 0.7
        p = v.co + head.front * reach - Vector((0, 0, CAP["peak_drop"] * (reach / CAP["peak_len"])))
        p += head.left * (s * 0.004)
        outer.append(bm.verts.new(p))
    for i in range(len(rim) - 1):
        f = bm.faces.new((rim[i], rim[i + 1], outer[i + 1], outer[i]))
        for lp in f.loops:
            if lp.vert in info:
                k, w = rim.index(lp.vert), 0.0
            else:
                k, w = outer.index(lp.vert), 1.0
            lp[uv].uv = (0.70 + 0.28 * k / max(1, len(rim) - 1), 0.40 + 0.25 * w)
    bm.normal_update()
    return finish(bm, name, "Cap", (0.36, 0.36, 0.38))


def finish(bm, name, mat_name, colour):
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = True
    mat = bpy.data.materials.get(mat_name) or bpy.data.materials.new(mat_name)
    mat.use_nodes = True
    mat.use_backface_culling = False                        # two-sided: the shadow is cast whatever the sun's side
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (*colour, 1.0)
        bsdf.inputs["Roughness"].default_value = 0.85
    me.materials.append(mat)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


# ------------------------------------------------------------------------------ checks
def uv_check(ob, grid=384):
    """Every UV inside 0..1; no triangle of the UV layout over another (pixel centres counted on a grid);
    no degenerate or flipped-in-UV triangle. Returns a dict; "ok" is the verdict."""
    me = ob.data
    uv = me.uv_layers.active.data
    tris = []
    out_of_range = degenerate = 0
    winding = [0, 0]
    for p in me.polygons:
        t = [Vector(uv[li].uv) for li in p.loop_indices]
        if any(c.x < 0 or c.x > 1 or c.y < 0 or c.y > 1 for c in t):
            out_of_range += 1
        area = 0.5 * ((t[1].x - t[0].x) * (t[2].y - t[0].y) - (t[2].x - t[0].x) * (t[1].y - t[0].y))
        if abs(area) < 1e-9:
            degenerate += 1
            continue
        winding[0 if area > 0 else 1] += 1
        tris.append(t)
    cover = {}
    overlap = 0
    for t in tris:
        x0, x1 = min(c.x for c in t), max(c.x for c in t)
        y0, y1 = min(c.y for c in t), max(c.y for c in t)
        for gy in range(max(0, int(y0 * grid)), min(grid - 1, int(y1 * grid)) + 1):
            for gx in range(max(0, int(x0 * grid)), min(grid - 1, int(x1 * grid)) + 1):
                px, py = (gx + 0.5) / grid, (gy + 0.5) / grid
                d = []
                for i in range(3):
                    a, b = t[i], t[(i + 1) % 3]
                    d.append((b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x))
                eps = 1e-7
                if all(v > eps for v in d) or all(v < -eps for v in d):
                    cover[(gx, gy)] = cover.get((gx, gy), 0) + 1
    overlap = sum(1 for n in cover.values() if n > 1)
    used = len(cover) / float(grid * grid)
    # the cap's two islands may wind differently; inside one island a flipped triangle shows as overlap
    return {"triangles": len(me.polygons), "uv_out_of_range": out_of_range, "uv_degenerate": degenerate,
            "uv_overlap_cells": overlap, "uv_used": round(used, 3),
            "ok": out_of_range == 0 and degenerate == 0 and overlap == 0}


def shape_check(ob, head, min_clear, max_out):
    """The shell is outside the skull everywhere (no poke-through to shade the scalp oddly) and not absurdly far from it."""
    inside = 0
    far = 0.0
    for v in ob.data.vertices:
        near = head.bvh.find_nearest(v.co)
        dist = (v.co - near[0]).length
        if (v.co - near[0]).dot(near[1]) < -1e-4 and dist > min_clear:
            inside += 1
        far = max(far, dist)
    zs = [v.co.z for v in ob.data.vertices]
    return {"verts_inside_skull": inside, "max_from_skull_mm": round(far * 1000, 1),
            "top_over_skull_mm": round((max(zs) - head.top) * 1000, 1), "ok": inside == 0 and far <= max_out}


def export(ob):
    os.makedirs(OUT, exist_ok=True)
    for o in bpy.context.scene.objects:
        o.select_set(o == ob)
    bpy.context.view_layer.objects.active = ob
    path = os.path.join(OUT, ob.name + ".glb")
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True, export_apply=True,
                              export_yup=True, export_texcoords=True, export_normals=True, export_materials="EXPORT")
    return path


def preview(objs, head_objs):
    """Four views of each look on the head (renders/hero/looks_<name>.png): what the Mac looks at before the import."""
    out = os.path.normpath(os.path.join(HERE, "..", "..", "renders", "hero"))
    os.makedirs(out, exist_ok=True)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.render.resolution_x, scene.render.resolution_y = 1400, 700
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 0.75
    for ob in objs:
        for o in objs:
            o.hide_render = o != ob
        centre = sum((Vector(c) for c in ob.bound_box), Vector()) / 8.0
        for k, (dx, dy) in enumerate(((0, -1), (1, 0), (0, 1), (-1, 0))):
            cam.location = centre + Vector((dx, dy, 0.15)) * 1.2
            cam.rotation_euler = (centre - cam.location).to_track_quat("-Z", "Y").to_euler()
            scene.render.filepath = os.path.join(out, "looks_%s_%d.png" % (ob.name, k))
            bpy.ops.render.render(write_still=True)
    log("previews in", out)


def main():
    clear_scene()
    head_objs = load_head()
    skull = skull_mesh(head_objs)
    head = Head(skull)
    log("head: top %.3f m, %.1f x %.1f x %.1f cm, face towards (%.0f, %.0f)" % (
        head.top, head.rx * 200, head.ry * 200, head.height * 100, head.front.x, head.front.y))
    made = []
    report = {"source": "stand-in head" if STANDIN else os.path.relpath(GLB, os.path.join(HERE, "..", "..")),
              "head": {"top_m": round(head.top, 4), "width_cm": round(head.rx * 200, 1), "depth_cm": round(head.ry * 200, 1),
                       "front": [head.front.x, head.front.y]}, "meshes": {}}
    ok = True
    for cut, spec in CUTS.items():
        ob = build_hair(head, "SM_HeroHair_" + cut, spec)
        made.append((ob, 0.004, spec["top"] + spec["sweep"] + 0.012))
    made.append((build_cap(head, "SM_HeroCap"), 0.004, CAP["peak_len"] + CAP["forward"] + 0.03))
    for ob, clear, max_out in made:
        r = uv_check(ob)
        r.update({"shape": shape_check(ob, head, clear, max_out)})
        r["file"] = os.path.basename(export(ob))
        report["meshes"][ob.name] = r
        good = r["ok"] and r["shape"]["ok"]
        ok = ok and good
        log("%s: %d tris, UV out %d, degenerate %d, overlap cells %d, inside skull %d, max %.1f mm from the skull: %s" % (
            ob.name, r["triangles"], r["uv_out_of_range"], r["uv_degenerate"], r["uv_overlap_cells"],
            r["shape"]["verts_inside_skull"], r["shape"]["max_from_skull_mm"], "ok" if good else "FAILED"))
    with open(os.path.join(OUT, "looks_report.json"), "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1)
        f.write("\n")
    if PREVIEW:
        preview([m[0] for m in made], head_objs)
    skull.free()
    if not ok:
        raise SystemExit("LOOKS FAILED: see the lines above")
    log("done:", OUT)


main()
