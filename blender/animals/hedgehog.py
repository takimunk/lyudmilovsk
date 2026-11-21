"""The hedgehog of the yards and the old orchard (docs/animals.md "With people"; behaviour: Private/Animals/AnimalPetsSim.cpp,
drawn by ULyudPets): two static meshes, walking and curled into a ball. A small animal seen at dusk from a few metres, so it
is built from primitives in flat colours like the townsfolk's props, and goes the same way into the game:

    Blender -b -P blender/animals/hedgehog.py [-- --preview]     (or python3 with the bpy module)
        -> Saved/ClipSources/props/hedgehog.glb, hedgehog_ball.glb
           (PROPS=hedgehog,hedgehog_ball unreal/scripts/import_props.sh -> /Game/Characters/Humans/Props/SM_Prop_hedgehog[_ball])
        --preview: also Saved/ClipSources/props/preview_hedgehog.png (both, side and three-quarter)

Metres. Origin on the ground under the middle, the nose along +X, +Z up (the game turns it by the hedgehog's yaw and rolls it
a little with the steps). About 24 cm long and 12 cm high; the ball 15 cm across.

Every face has its own place in the UV square (a smart projection with a margin): the colours are flat, but nothing overlaps
should a texture ever be baked. The script checks that itself (CHECK lines; exit 1 on an overlap or a degenerate face).
"""
import math
import os
import random
import sys

import bpy
import bmesh
from mathutils import Matrix, Vector

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OUT = os.path.join(ROOT, "Saved", "ClipSources", "props")
PREVIEW = "--preview" in sys.argv

COLOURS = {
    "HH_Spines": (0.16, 0.115, 0.075),      # dark brown with a paler tip (the tips are a second material)
    "HH_Tips": (0.62, 0.54, 0.40),
    "HH_Fur": (0.50, 0.40, 0.29),           # the face, the belly, the legs
    "HH_Dark": (0.03, 0.025, 0.02),         # nose, eyes
    "HH_Ear": (0.36, 0.27, 0.21),
}


def material(name):
    m = bpy.data.materials.get(name)
    if m:
        return m
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = COLOURS[name] + (1.0,)
    b.inputs["Roughness"].default_value = 0.3 if name == "HH_Dark" else 0.85
    return m


class Build:
    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.mats = []

    def mat(self, name):
        if name not in self.mats:
            self.mats.append(name)
        return self.mats.index(name)

    def ellipsoid(self, mat, centre, radii, segs=12, rings=8, shape=None):
        """A UV sphere scaled to radii at centre; shape(v) may move each unit-sphere point before the scale."""
        made = bmesh.ops.create_uvsphere(self.bm, u_segments=segs, v_segments=rings, radius=1.0)
        for v in made["verts"]:
            p = v.co.copy()
            if shape:
                p = shape(p)
            v.co = Vector((centre[0] + p.x * radii[0], centre[1] + p.y * radii[1], centre[2] + p.z * radii[2]))
        i = self.mat(mat)
        for f in {f for v in made["verts"] for f in v.link_faces}:
            f.material_index = i

    def spine(self, base, normal, length, radius):
        """A three-sided spike: dark at the root, pale at the tip."""
        n = normal.normalized()
        a = n.orthogonal().normalized()
        b = n.cross(a)
        mid = base + n * (length * 0.62)
        tip = base + n * length
        ring0, ring1 = [], []
        for k in range(3):
            ang = k * 2.0 * math.pi / 3.0
            d = a * math.cos(ang) + b * math.sin(ang)
            ring0.append(self.bm.verts.new(base + d * radius))
            ring1.append(self.bm.verts.new(mid + d * radius * 0.45))
        top = self.bm.verts.new(tip)
        dark, pale = self.mat("HH_Spines"), self.mat("HH_Tips")
        for k in range(3):
            f = self.bm.faces.new((ring0[k], ring0[(k + 1) % 3], ring1[(k + 1) % 3], ring1[k]))
            f.material_index = dark
            f = self.bm.faces.new((ring1[k], ring1[(k + 1) % 3], top))
            f.material_index = pale

    def finish(self):
        me = bpy.data.meshes.new(self.name)
        low = min(v.co.z for v in self.bm.verts)        # stands on the ground: the lowest point at z 0
        for v in self.bm.verts:
            v.co.z -= low
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces[:])
        bmesh.ops.triangulate(self.bm, faces=self.bm.faces[:])
        self.bm.to_mesh(me)
        self.bm.free()
        for n in self.mats:
            me.materials.append(material(n))
        o = bpy.data.objects.new(self.name, me)
        bpy.context.scene.collection.objects.link(o)
        return o


def spines_over(b, centre, radii, count, seed, keep, length=(0.022, 0.034), lean=0.0):
    """Spikes over the part of an ellipsoid where keep(unit point) is true, by a golden-angle spiral (even, no clumps)."""
    rnd = random.Random(seed)
    n_made = 0
    for k in range(count):
        z = 1.0 - 2.0 * (k + 0.5) / count
        r = math.sqrt(max(0.0, 1.0 - z * z))
        ang = k * math.pi * (3.0 - math.sqrt(5.0))
        u = Vector((r * math.cos(ang), r * math.sin(ang), z))
        if not keep(u):
            continue
        base = Vector((centre[0] + u.x * radii[0] * 0.97, centre[1] + u.y * radii[1] * 0.97, centre[2] + u.z * radii[2] * 0.97))
        normal = Vector((u.x / radii[0], u.y / radii[1], u.z / radii[2])).normalized()
        normal = (normal + Vector((-lean, 0.0, 0.0)) + Vector((rnd.uniform(-0.12, 0.12), rnd.uniform(-0.12, 0.12), rnd.uniform(-0.12, 0.12)))).normalized()
        b.spine(base, normal, rnd.uniform(*length), 0.0052)
        n_made += 1
    return n_made


def walking():
    b = Build("hedgehog")
    centre, radii = (-0.015, 0.0, 0.068), (0.105, 0.068, 0.058)

    def snouty(p):
        # the front of the body draws out into the face and drops a little
        if p.x > 0.35:
            t = (p.x - 0.35) / 0.65
            return Vector((p.x + 0.08 * t, p.y * (1.0 - 0.45 * t), p.z * (1.0 - 0.4 * t) - 0.22 * t))
        return p

    b.ellipsoid("HH_Fur", centre, radii, segs=14, rings=9, shape=snouty)
    # the snout: a short cone of fur to the nose, the nose itself, eyes, ears
    b.ellipsoid("HH_Fur", (0.098, 0.0, 0.046), (0.034, 0.022, 0.02), segs=8, rings=5)
    b.ellipsoid("HH_Dark", (0.131, 0.0, 0.047), (0.008, 0.008, 0.007), segs=6, rings=4)
    for s in (-1, 1):
        b.ellipsoid("HH_Dark", (0.094, s * 0.021, 0.061), (0.0055, 0.0045, 0.0055), segs=6, rings=4)
        b.ellipsoid("HH_Ear", (0.068, s * 0.036, 0.083), (0.011, 0.006, 0.013), segs=6, rings=4)
        # four short legs with the feet turned out a little
        for x in (0.045, -0.06):
            b.ellipsoid("HH_Fur", (x, s * 0.036, 0.016), (0.013, 0.011, 0.018), segs=6, rings=4)
            b.ellipsoid("HH_Fur", (x + 0.012, s * 0.04, 0.005), (0.017, 0.01, 0.006), segs=6, rings=3)
    # the coat of spines: the back and the flanks down to a skirt, not the face, lying back a little
    n = spines_over(b, centre, radii, 520, 11, lambda u: u.z > -0.22 and u.x < 0.5 + 0.25 * max(u.z, 0.0), lean=0.35)
    return b.finish(), n


def ball():
    b = Build("hedgehog_ball")
    centre, r = (0.0, 0.0, 0.07), 0.062
    b.ellipsoid("HH_Fur", centre, (r, r, r * 0.96), segs=12, rings=8)
    # all that shows of it: the tip of the nose low at the front, tucked in
    b.ellipsoid("HH_Fur", (0.052, 0.0, 0.03), (0.02, 0.017, 0.014), segs=6, rings=4)
    b.ellipsoid("HH_Dark", (0.07, 0.0, 0.028), (0.006, 0.006, 0.005), segs=6, rings=4)
    n = spines_over(b, centre, (r, r, r * 0.96), 420, 23, lambda u: not (u.x > 0.55 and u.z < -0.3) and u.z > -0.7, length=(0.026, 0.038))
    return b.finish(), n


def unwrap(o):
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(66.0), island_margin=0.002)
    bpy.ops.object.mode_set(mode="OBJECT")


def check(o, spines):
    """Triangles, size, degenerate faces, and UV overlap (the UV square rasterised at 1024: texels two faces both cover)."""
    me = o.data
    tris = len(me.polygons)
    degenerate = sum(1 for p in me.polygons if p.area < 1e-9)
    xs = [v.co.x for v in me.vertices]
    ys = [v.co.y for v in me.vertices]
    zs = [v.co.z for v in me.vertices]
    size = (max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs))
    uv = me.uv_layers.active.data
    n = 1024
    grid = bytearray(n * n)
    over = covered = 0
    out_of_square = 0
    for p in me.polygons:
        pts = [uv[i].uv for i in p.loop_indices]
        if any(c < -1e-4 or c > 1.0001 for q in pts for c in q):
            out_of_square += 1
        (x0, y0), (x1, y1), (x2, y2) = [(q.x * n, q.y * n) for q in pts]
        area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
        if abs(area) < 1e-9:
            continue
        for iy in range(max(0, int(min(y0, y1, y2))), min(n - 1, int(max(y0, y1, y2))) + 1):
            for ix in range(max(0, int(min(x0, x1, x2))), min(n - 1, int(max(x0, x1, x2))) + 1):
                px, py = ix + 0.5, iy + 0.5
                w0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) / area
                w1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) / area
                w2 = 1.0 - w0 - w1
                # well inside the face only: neighbours of one island share their edge texels
                if w0 > 0.12 and w1 > 0.12 and w2 > 0.12:
                    k = iy * n + ix
                    if grid[k]:
                        over += 1
                    else:
                        grid[k] = 1
                        covered += 1
    ok = degenerate == 0 and over == 0 and out_of_square == 0 and min(zs) > -0.002
    print("CHECK %s: %d tris, %d spines, %.3f x %.3f x %.3f m, lowest %.3f, %d degenerate, UV: %d texels covered, %d overlapping, %d faces off the square -> %s" % (
        o.name, tris, spines, size[0], size[1], size[2], min(zs), degenerate, covered, over, out_of_square, "ok" if ok else "FAIL"))
    return ok


def preview(objs):
    scene = bpy.context.scene
    for o in list(scene.objects):
        if o.type in ("CAMERA", "LIGHT"):
            bpy.data.objects.remove(o)
    objs[1].location = (0.0, 0.26, 0.0)
    ground = bpy.data.meshes.new("ground")
    gb = bmesh.new()
    bmesh.ops.create_grid(gb, x_segments=1, y_segments=1, size=1.2)
    gb.to_mesh(ground)
    gb.free()
    gm = bpy.data.materials.new("HH_Ground")
    gm.use_nodes = True
    gm.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.2, 0.26, 0.13, 1.0)
    ground.materials.append(gm)
    g = bpy.data.objects.new("ground", ground)
    scene.collection.objects.link(g)
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 3.5
    sun.rotation_euler = (math.radians(52), 0.0, math.radians(35))
    scene.collection.objects.link(sun)
    world = bpy.data.worlds.new("w") if not scene.world else scene.world
    scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.55, 0.6, 0.7, 1.0)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.9
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.data.lens = 70
    scene.collection.objects.link(cam)
    scene.camera = cam
    scene.render.engine = "CYCLES"         # on the CPU: runs without a display (the cloud, a build machine)
    scene.cycles.samples = 48
    scene.cycles.use_denoising = False
    scene.render.resolution_x, scene.render.resolution_y = 1280, 720
    target = Vector((0.0, 0.13, 0.05))
    for tag, pos in (("side", Vector((0.25, -0.95, 0.2))), ("front", Vector((0.85, -0.35, 0.33)))):
        cam.location = pos
        cam.rotation_euler = (target - pos).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = os.path.join(OUT, "preview_hedgehog_%s.png" % tag)
        bpy.ops.render.render(write_still=True)
        print("PREVIEW", scene.render.filepath)
    objs[1].location = (0.0, 0.0, 0.0)
    bpy.data.objects.remove(g)


def main():
    os.makedirs(OUT, exist_ok=True)
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o)
    made = [walking(), ball()]
    ok = True
    for o, n in made:
        unwrap(o)
        ok = check(o, n) and ok
    for o, _ in made:
        bpy.ops.object.select_all(action="DESELECT")
        o.select_set(True)
        bpy.context.view_layer.objects.active = o
        out = os.path.join(OUT, o.name + ".glb")
        bpy.ops.export_scene.gltf(filepath=out, use_selection=True, export_format="GLB", export_yup=True)
        print("HEDGEHOG", out)
    if PREVIEW:
        preview([o for o, _ in made])
    if not ok:
        sys.exit(1)


main()
