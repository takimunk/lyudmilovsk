"""Mesh helpers shared by build_food.py and build_stalls.py: build a mesh face by face with its uvs mapped into an atlas cell
(food_atlas.CELLS), primitives (box, tube along a path, lathe), one material M_FoodKit over the atlas, booleans, lint, preview
renders and the glb export. Blender axes, metres (Unreal: x*100, -y*100, z*100 cm).
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "birds"))
import food_atlas as AT  # noqa: E402

OUT = os.path.join(HERE, "export")


class Builder:
    """Faces with per-corner uvs (0..1 inside a cell); lint_part per face (a separate piece for the lint)."""

    def __init__(self):
        self.verts, self.faces, self.uvs, self.parts = [], [], [], []
        self.part = 0

    def new_part(self):
        self.part += 1
        return self

    def face(self, cell, pts, uv):
        r = AT.uv_rect(cell)
        base = len(self.verts)
        self.verts.extend(Vector(p) for p in pts)
        self.faces.append(tuple(range(base, base + len(pts))))
        self.uvs.append([(r[0] + (r[2] - r[0]) * min(max(u, 0.0), 1.0), r[1] + (r[3] - r[1]) * min(max(v, 0.0), 1.0)) for u, v in uv])
        self.parts.append(self.part)

    def box(self, cell, lo, hi, scale=1.0, skip=()):
        """An axis-aligned box lo..hi (metres); each side's uvs planar, scale texels per metre-ish (repeats clamp to the cell)."""
        x0, y0, z0 = lo
        x1, y1, z1 = hi
        c = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
        sides = {"-z": (0, 3, 2, 1), "+z": (4, 5, 6, 7), "-y": (0, 1, 5, 4), "+y": (2, 3, 7, 6), "-x": (3, 0, 4, 7), "+x": (1, 2, 6, 5)}
        for k, idx in sides.items():
            if k in skip:
                continue
            pts = [c[i] for i in idx]
            self.face(cell, pts, _planar_uv(pts, scale))
        return self

    def tube(self, cell, path, radii, sides=8, cap0=None, cap1=None, uv_len=1.0, twist=0.0, ups=None):
        """Rings round a polyline path (list of points), radius per point; caps: None (open), a cell name (flat fan)."""
        n = len(path)
        frames = _frames(path, ups)
        rings = []
        for i, (p, r) in enumerate(zip(path, radii)):
            t, nrm, b = frames[i]
            ring = []
            for k in range(sides):
                a = 2 * math.pi * k / sides + twist * i
                ring.append(Vector(p) + (nrm * math.cos(a) + b * math.sin(a)) * r)
            rings.append(ring)
        length = [0.0]
        for i in range(1, n):
            length.append(length[-1] + (Vector(path[i]) - Vector(path[i - 1])).length)
        L = max(length[-1], 1e-6)
        for i in range(n - 1):
            for k in range(sides):
                k1 = (k + 1) % sides
                pts = [rings[i][k], rings[i][k1], rings[i + 1][k1], rings[i + 1][k]]
                u0, u1 = k / sides, (k + 1) / sides
                v0, v1 = min(1.0, length[i] / L * uv_len), min(1.0, length[i + 1] / L * uv_len)
                self.face(cell, pts, [(u0, v0), (u1, v0), (u1, v1), (u0, v1)])
        for cap, ring, flip in ((cap0, rings[0], True), (cap1, rings[-1], False)):
            if not cap:
                continue
            c = sum(ring, Vector()) / len(ring)
            for k in range(sides):
                k1 = (k + 1) % sides
                a0, a1 = 2 * math.pi * k / sides, 2 * math.pi * k1 / sides
                pts = [c, ring[k], ring[k1]] if not flip else [c, ring[k1], ring[k]]
                uv = [(0.5, 0.5), (0.5 + 0.45 * math.cos(a0), 0.5 + 0.45 * math.sin(a0)), (0.5 + 0.45 * math.cos(a1), 0.5 + 0.45 * math.sin(a1))]
                self.face(cap, pts, uv if not flip else [uv[0], uv[2], uv[1]])
        return rings

    def lathe(self, cell, profile, axis="z", sides=24, centre=(0, 0, 0), cap_cell=None, u_rep=1.0):
        """Revolve profile [(r, h)] (bottom to top) round an axis through centre; caps at the ends where r > 0."""
        cx, cy, cz = centre

        def pt(r, h, a):
            if axis == "z":
                return (cx + r * math.cos(a), cy + r * math.sin(a), cz + h)
            if axis == "y":
                return (cx + r * math.cos(a), cy + h, cz + r * math.sin(a))
            return (cx + h, cy + r * math.cos(a), cz + r * math.sin(a))
        L = [0.0]
        for i in range(1, len(profile)):
            L.append(L[-1] + math.hypot(profile[i][0] - profile[i - 1][0], profile[i][1] - profile[i - 1][1]))
        tot = max(L[-1], 1e-6)
        sign = -1 if axis == "y" else 1
        for i in range(len(profile) - 1):
            (r0, h0), (r1, h1) = profile[i], profile[i + 1]
            for k in range(sides):
                a0, a1 = 2 * math.pi * k / sides, 2 * math.pi * (k + 1) / sides
                pts = [pt(r0, h0, a0), pt(r0, h0, a1), pt(r1, h1, a1), pt(r1, h1, a0)]
                uv = [(k / sides * u_rep, L[i] / tot), ((k + 1) / sides * u_rep, L[i] / tot), ((k + 1) / sides * u_rep, L[i + 1] / tot), (k / sides * u_rep, L[i + 1] / tot)]
                if r0 < 1e-7:
                    pts, uv = pts[1:], uv[1:]
                elif r1 < 1e-7:
                    pts, uv = pts[:3], uv[:3]
                if sign < 0:
                    pts, uv = pts[::-1], uv[::-1]
                self.face(cell, pts, [(min(u, 1.0), v) for u, v in uv])
        for (r, h), top in ((profile[0], False), (profile[-1], True)):
            if r < 1e-7 or not cap_cell:
                continue
            ring = [pt(r, h, 2 * math.pi * k / sides) for k in range(sides)]
            uv = [(0.5 + 0.48 * math.cos(2 * math.pi * k / sides), 0.5 + 0.48 * math.sin(2 * math.pi * k / sides)) for k in range(sides)]
            if top == (sign > 0):
                self.face(cap_cell, ring, uv)
            else:
                self.face(cap_cell, ring[::-1], uv[::-1])

    def build(self, name, merge=0.0):
        me = bpy.data.meshes.new(name)
        me.from_pydata([tuple(v) for v in self.verts], [], self.faces)
        uvl = me.uv_layers.new(name="UVMap")
        li = 0
        for f, uvs in zip(me.polygons, self.uvs):
            for k, li in enumerate(f.loop_indices):
                uvl.data[li].uv = uvs[k]
        attr = me.attributes.new("lint_part", "INT", "FACE")
        attr.data.foreach_set("value", self.parts)
        ob = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(ob)
        ob.data.materials.append(material())
        if merge > 0:
            bm = bmesh.new()
            bm.from_mesh(me)
            bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=merge)
            bm.to_mesh(me)
            bm.free()
        me.validate()
        for p in me.polygons:
            p.use_smooth = True
        return ob


def _planar_uv(pts, scale):
    a, b, c = Vector(pts[0]), Vector(pts[1]), Vector(pts[-1])
    eu, ev = (b - a), (c - a)
    lu, lv = max(eu.length, 1e-9), max(ev.length, 1e-9)
    eu.normalize()
    ev = ev - eu * ev.dot(eu)
    ev.normalize()
    out = []
    for p in pts:
        d = Vector(p) - a
        out.append((min(1.0, d.dot(eu) * scale), min(1.0, d.dot(ev) * scale)))
    return out


def _frames(path, ups=None):
    out = []
    n = len(path)
    prev_n = None
    for i in range(n):
        p0 = Vector(path[max(i - 1, 0)])
        p1 = Vector(path[min(i + 1, n - 1)])
        t = (p1 - p0).normalized()
        if prev_n is None:
            up = Vector(ups[i]) if ups else (Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((1, 0, 0)))
            nrm = (up - t * up.dot(t)).normalized()
        else:
            nrm = (prev_n - t * prev_n.dot(t)).normalized()
        b = t.cross(nrm)
        out.append((t, nrm, b))
        prev_n = nrm
    return out


_MAT = None


def material():
    global _MAT
    if _MAT and _MAT.name in bpy.data.materials:
        return _MAT
    m = bpy.data.materials.new("M_FoodKit")
    m.use_nodes = True
    nt = m.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")

    def img(name, cs):
        p = os.path.join(OUT, name)
        if not os.path.exists(p):
            return None
        im = bpy.data.images.load(p, check_existing=True)
        im.colorspace_settings.name = cs
        n = nt.nodes.new("ShaderNodeTexImage")
        n.image = im
        return n
    c, r, nn = img("T_FoodKit_C.png", "sRGB"), img("T_FoodKit_R.png", "Non-Color"), img("T_FoodKit_N.png", "Non-Color")
    if c:
        nt.links.new(c.outputs["Color"], bsdf.inputs["Base Color"])
    if r:
        nt.links.new(r.outputs["Color"], bsdf.inputs["Roughness"])
    if nn:
        nm = nt.nodes.new("ShaderNodeNormalMap")
        nt.links.new(nn.outputs["Color"], nm.inputs["Color"])
        nt.links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
    _MAT = m
    return m


def ellipsoid(name, centre, radii, cell, segs=16, rings=10):
    """A uv-sphere cutter scaled to radii, its uvs in a cell (the cut faces take them)."""
    bm = bmesh.new()
    bm.loops.layers.uv.new("UVMap")
    bmesh.ops.create_uvsphere(bm, u_segments=segs, v_segments=rings, radius=1.0, calc_uvs=True)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    uvl = me.uv_layers[0]
    uvl.name = "UVMap"
    r = AT.uv_rect(cell)
    for d in uvl.data:
        d.uv = (r[0] + (r[2] - r[0]) * d.uv[0], r[1] + (r[3] - r[1]) * d.uv[1])
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    ob.data.materials.append(material())
    ob.matrix_world = Matrix.Translation(centre) @ Matrix.Diagonal((*radii, 1.0))
    return ob


def subtract(target, cutters):
    """target minus the union of cutters (exact booleans, applied); the cutters are deleted."""
    if not cutters:
        return target
    bpy.context.view_layer.objects.active = target
    for c in cutters:
        mod = target.modifiers.new("bite", "BOOLEAN")
        mod.operation = "DIFFERENCE"
        mod.solver = "EXACT"
        mod.object = c
        bpy.ops.object.modifier_apply(modifier=mod.name)
    for c in cutters:
        bpy.data.objects.remove(c, do_unlink=True)
    return target


def tidy(ob, sharp_deg=40.0):
    """After booleans: slivers dissolved, edges sharper than sharp_deg kept sharp (smooth shading across a cut smears it)."""
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.dissolve_degenerate(bm, edges=bm.edges, dist=2e-5)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2e-5)
    bm.to_mesh(ob.data)
    bm.free()
    ob.data.set_sharp_from_angle(angle=math.radians(sharp_deg))
    return ob


def copy(ob, name):
    c = ob.copy()
    c.data = ob.data.copy()
    c.name = name
    c.data.name = name
    bpy.context.scene.collection.objects.link(c)
    return c


def join(obs, name):
    bpy.ops.object.select_all(action="DESELECT")
    for o in obs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = obs[0]
    bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    ob.name = name
    ob.data.name = name
    return ob


def tris(ob):
    return sum(len(p.vertices) - 2 for p in ob.data.polygons)


def lint(ob):
    try:
        import lint_bird
    except Exception as e:  # noqa: BLE001
        print("LINT unavailable", e)
        return {}
    res = lint_bird.lint(ob, label=ob.name, verbose=False, collect=True)
    keys = ("degenerate", "uv_zero", "uv_outside", "zfight", "inside")
    print("LINT %-28s tris %6d %s" % (ob.name, tris(ob), " ".join("%s %s" % (k, res.get(k, "-")) for k in keys)))
    return res


def export(objs, path):
    for ob in objs:
        if "lint_part" in ob.data.attributes:
            ob.data.attributes.remove(ob.data.attributes["lint_part"])
    bpy.ops.object.select_all(action="DESELECT")
    for ob in objs:
        ob.select_set(True)
    kw = dict(filepath=path, export_format="GLB", use_selection=True, export_yup=True, export_apply=True, export_normals=True,
              export_texcoords=True, export_materials="EXPORT", export_skins=False, export_animations=False,
              export_vertex_color="NONE", export_image_format="NONE")
    props = bpy.ops.export_scene.gltf.get_rna_type().properties
    bpy.ops.export_scene.gltf(**{k: v for k, v in kw.items() if k in props or k == "filepath"})
    print("EXPORTED", path, os.path.getsize(path), "bytes")


def preview(objs, path, eye, look, lens=50, res=(900, 700), lights=True):
    """An Eevee picture of objs (the rest hidden) from eye toward look."""
    scene = bpy.context.scene
    for o in scene.objects:
        o.hide_render = o not in objs and o.type == "MESH"
    cam = bpy.data.objects.get("PreviewCam") or bpy.data.objects.new("PreviewCam", bpy.data.cameras.new("PreviewCam"))
    if cam.name not in scene.collection.objects:
        scene.collection.objects.link(cam)
    cam.data.lens = lens
    cam.data.clip_start = 0.005
    cam.location = eye
    d = Vector(look) - Vector(eye)
    cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    scene.camera = cam
    if lights and not bpy.data.objects.get("PreviewSun"):
        sun = bpy.data.objects.new("PreviewSun", bpy.data.lights.new("PreviewSun", "SUN"))
        sun.data.energy = 3.5
        sun.rotation_euler = (math.radians(50), math.radians(10), math.radians(30))
        scene.collection.objects.link(sun)
        w = bpy.data.worlds.new("PreviewWorld")
        w.use_nodes = True
        w.node_tree.nodes["Background"].inputs[0].default_value = (0.55, 0.6, 0.68, 1)
        w.node_tree.nodes["Background"].inputs[1].default_value = 0.8
        scene.world = w
    engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else ("BLENDER_EEVEE" if "BLENDER_EEVEE" in engines else engines[0])
    scene.render.resolution_x, scene.render.resolution_y = res
    scene.render.filepath = path
    scene.view_settings.view_transform = "Standard"
    bpy.ops.render.render(write_still=True)
