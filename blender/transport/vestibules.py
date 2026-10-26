"""Builds the stations' vestibules as our own game models, with the Hi3D station crowns
(design/transport/gen/top_<style>.glb, ~290k triangles, two 8k textures) only as the reference for the shape and the
source the texture is baked from. Each vestibule stands on the connector of the procedural station part
(docs/transport.md, "Station layout and the vestibule connector").

  Blender -b -P blender/transport/vestibules.py [-- style ...]

Rule (Dima, 2026-10-26): what people walk into is rebuilt clean and low-poly, with real openings, walls of real
thickness, collision and UVs; the generated model is the reference and the bake source.

For each style, from the Hi3D model scaled to 21 m across (less if it would rise over 16 m above its floor), its terrace
at z=0:
- the terrace: a slab along the model's terrace outline, its top at 0, its edge down to -0.6 m;
- the railing round it (chrome posts and rails), open at the connector's gap and the street entrance;
- the room, built by its shape: "cyl" (a round room), "box", "revolve" (the dome: the shell turned from the model's
  profile) or "lantern" (the amber glass bulb: flat facets of glass between ribs); 20 cm walls (the lantern: glass);
  the windows where the model's texture shows them (round or square holes with a pane of TR_Glass), a Double door hole
  (tools/character_standard.py) where the model shows its door, a floor (TR_Terrazzo), a ceiling;
- the canopy (arches, folds, star, saucer, wing): the model's own parts above the terrace that are not the room or the
  railing, remeshed watertight and decimated to a budget;
- the texture: the outside (terrace, walls, roof, canopy) unwrapped into one 2k atlas, the model's colour baked onto it;
  the inside walls are plaster (TR_Interior), the railing TR_Chrome, the glass TR_Glass;
- turned so the connector faces +X with its point at the origin; nothing at X>0 or under -0.6 m;
- design/transport/game/SM_TR_Vestibule_<Style>.glb and Content/Transport/vestibules.json (UE cm).
blender/transport/check_vestibules.py then walks a player through each.

Frame: Blender (x, y, z) m -> Unreal (x*100, -y*100, z*100); the connector's way out is +X in both.
"""
import bmesh
import bpy
import json
import math
import os
import sys
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import character_standard  # noqa: E402

GEN = os.path.join(ROOT, "design", "transport", "gen")
OUT = os.path.join(ROOT, "design", "transport", "game")
JSON = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Transport", "vestibules.json")

D_MAX = 21.0            # m across
H_MAX = 15.9            # m over the floor (the spec: 16)
TEX = 2048
WALL = 0.2              # the room's walls
GAP = 1.5               # half the connector's clear gap
ENTRANCE = 1.2          # half the street entrance's gap
DOOR_W, DOOR_H = (v / 100.0 for v in character_standard.door_hole_for("Double"))
BASE = -0.6             # nothing below
RAIL_H, RAIL_IN = 1.05, 0.15
CANOPY_TRIS = 7000

# per style: the room's shape; the azimuth (degrees, the model's frame after scaling, 0 = +X, 90 = +Y) of the door the
# model shows, of the connector and of the street entrance (both moved to where a player walks in clear); ceiling:
# the room's own ceiling under an open canopy; door_shift: the model's door is that far right of straight out
STYLES = {
    "arches":  {"room": "cyl", "door": -90, "conn": -40, "street": 140, "room_r": 5.4},
    "dome":    {"room": "revolve", "door": -90, "conn": -40, "street": 140},
    "folded":  {"room": "cyl", "door": -90, "conn": 0, "street": 140, "ceiling": 3.35, "room_r": 5.4},
    "lantern": {"room": "lantern", "door": 90, "conn": 40, "street": -140, "door_shift": 0.9},
    "saucer":  {"room": "cyl", "door": -90, "conn": -40, "street": 140, "ceiling": 3.3, "door_shift": 3.0,
                "room_r": 5.6},
    "star":    {"room": "cyl", "door": -90, "conn": 0, "street": 140, "ceiling": 3.4, "room_r": 5.4},
    "wing":    {"room": "box", "door": 0, "conn": 0, "street": 90, "trim_y": True, "ceiling": 3.6,
                "windows": False, "ribbon": (2.55, 3.3), "turn": 0, "edge_extra": 1.6,
                "room_half": (4.95, 4.1)},
}
PALETTE = {"TR_Glass": (0.3, 0.4, 0.45, 0.25), "TR_Interior": (0.78, 0.74, 0.66, 1), "TR_Terrazzo": (0.62, 0.58, 0.52, 1),
           "TR_Chrome": (0.8, 0.8, 0.82, 1),
           # the game's tiled surfaces (metre UVs; KitMaterials: TR_Tx_<name> -> tx_<name>)
           "TR_Tx_Concrete": (0.6, 0.6, 0.58, 1), "TR_Tx_Enamel": (0.9, 0.87, 0.8, 1), "TR_Tx_Amber": (0.85, 0.5, 0.15, 1),
           "TR_Tx_Mosaic": (0.5, 0.45, 0.6, 1), "TR_Tx_Tactile": (0.9, 0.7, 0.1, 1)}
SLOT = {k: i + 1 for i, k in enumerate(PALETTE)}       # slot 0: the baked outside


def log(*a):
    print("VESTIBULE", *a, flush=True)


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def load(name):
    bpy.ops.import_scene.gltf(filepath=os.path.join(GEN, name + ".glb"))
    objs = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    if len(objs) > 1:
        bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    o.parent = None
    return o


def bounds_of(vs):
    vs = list(vs)
    return (Vector((min(v.x for v in vs), min(v.y for v in vs), min(v.z for v in vs))),
            Vector((max(v.x for v in vs), max(v.y for v in vs), max(v.z for v in vs))))


def bounds(o):
    return bounds_of(v.co for v in o.data.vertices)


def transform(o, m):
    o.data.transform(m)
    o.data.update()


def tris(o):
    return sum(len(p.vertices) - 2 for p in o.data.polygons)


def activate(o):
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True)
    bpy.context.view_layer.objects.active = o


def apply_mod(o, mod):
    activate(o)
    bpy.ops.object.modifier_apply(modifier=mod.name)


def decimate(o, n):
    have = tris(o)
    if have > n:
        mod = o.modifiers.new("dec", "DECIMATE")
        mod.decimate_type = "COLLAPSE"
        mod.ratio = n / have
        mod.use_collapse_triangulate = True
        apply_mod(o, mod)
    return tris(o)


def floor_level(o, hmax):
    """The terrace: the height with the most up-facing area in the lower part."""
    hist = {}
    for p in o.data.polygons:
        if p.normal.z > 0.97 and p.center.z < hmax:
            k = round(p.center.z, 1)
            hist[k] = hist.get(k, 0.0) + p.area
    k = max(hist, key=hist.get)
    a = z = 0.0
    for p in o.data.polygons:
        if p.normal.z > 0.97 and abs(p.center.z - k) < 0.15:
            a += p.area
            z += p.area * p.center.z
    return z / a


def bvh_of(o):
    bm = bmesh.new()
    bm.from_mesh(o.data)
    bm.transform(o.matrix_world)
    t = BVHTree.FromBMesh(bm)
    bm.free()
    return t


def ray(t, org, d, far=60.0):
    loc, nor, idx, dist = t.ray_cast(org, d, far)
    return (loc, idx, dist) if loc is not None else (None, None, None)


def az(a):
    return Vector((math.cos(math.radians(a)), math.sin(math.radians(a)), 0.0))


def material(name, col=None):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    if col:
        m.diffuse_color = col
    return m


def new_object(name, bm, mats):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for m in mats:
        me.materials.append(m)
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    return o


def quad(bm, ps, mi, flip=False):
    vs = [bm.verts.new(p) for p in ps]
    f = bm.faces.new(vs[::-1] if flip else vs)
    f.material_index = mi
    return f


def box(bm, centre, size, mi, rot=None):
    """A closed box (size x, y, z) at centre, turned by the 3x3 matrix rot."""
    r = rot or Matrix.Identity(3)
    h = Vector(size) / 2
    cs = [centre + r @ Vector((sx * h.x, sy * h.y, sz * h.z)) for sz in (-1, 1) for sy in (-1, 1) for sx in (-1, 1)]
    v = [bm.verts.new(c) for c in cs]
    for ids in ((0, 2, 3, 1), (4, 5, 7, 6), (0, 1, 5, 4), (2, 6, 7, 3), (0, 4, 6, 2), (1, 3, 7, 5)):
        f = bm.faces.new([v[i] for i in ids])
        f.material_index = mi
    return v


def beam(bm, a, b, w, h, mi):
    """A box w x h in section from a to b."""
    d = b - a
    L = d.length
    if L < 1e-4:
        return
    x = d.normalized()
    up = Vector((0, 0, 1)) if abs(x.z) < 0.95 else Vector((1, 0, 0))
    y = up.cross(x).normalized()
    z = x.cross(y)
    box(bm, (a + b) / 2, (L, w, h), mi, Matrix((x, y, z)).transposed())


# --- the glass in the model's texture ------------------------------------------------------------------------------

def texels(o):
    for m in o.data.materials:
        if m and m.use_nodes:
            for n in m.node_tree.nodes:
                if n.type == "BSDF_PRINCIPLED":
                    link = n.inputs["Base Color"].links
                    if link and link[0].from_node.type == "TEX_IMAGE" and link[0].from_node.image:
                        img = link[0].from_node.image
                        return img.size[0], img.size[1], list(img.pixels[:])
    return 0, 0, None


def glass_faces(o, amber):
    """The faces whose colour is a window: dark and dull (the lantern: also its amber), on walls, not roofs."""
    w, h, px = texels(o)
    uv = o.data.uv_layers.active.data
    out = set()
    for poly in o.data.polygons:
        if poly.center.z < 0.05 or (not amber and abs(poly.normal.z) > 0.6):
            continue
        u = sum(uv[l].uv[0] for l in poly.loop_indices) / poly.loop_total
        v = sum(uv[l].uv[1] for l in poly.loop_indices) / poly.loop_total
        x, y = min(w - 1, max(0, int(u % 1.0 * w))), min(h - 1, max(0, int(v % 1.0 * h)))
        k = (y * w + x) * 4
        r, g, b = px[k], px[k + 1], px[k + 2]
        hi, lo = max(r, g, b), min(r, g, b)
        sat = (hi - lo) / hi if hi > 1e-4 else 0.0
        if (hi < 0.32 and sat < 0.45) or (amber and sat > 0.55 and hi > 0.55 and r >= g > b):
            out.add(poly.index)
    return out


def clusters(o, faces):
    """Connected groups of the given faces (by shared edges)."""
    me = o.data
    by_edge = {}
    for i in faces:
        for ek in me.polygons[i].edge_keys:
            by_edge.setdefault(ek, []).append(i)
    left = set(faces)
    groups = []
    while left:
        s = left.pop()
        g, todo = [s], [s]
        while todo:
            i = todo.pop()
            for ek in me.polygons[i].edge_keys:
                for j in by_edge.get(ek, ()):
                    if j in left:
                        left.remove(j)
                        g.append(j)
                        todo.append(j)
        groups.append(g)
    return groups


# --- the analysis of the model -------------------------------------------------------------------------------------

class Model:
    """The Hi3D model in the vestibule's frame (metres, terrace at z=0, centred) and what is measured on it."""

    def __init__(self, style, cfg):
        self.style, self.cfg = style, cfg
        o = load("top_" + style)
        lo, hi = bounds(o)
        s = D_MAX / max(hi.x - lo.x, hi.y - lo.y)
        c = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
        transform(o, Matrix.Scale(s, 4) @ Matrix.Translation(-c))
        lo, hi = bounds(o)
        fz = floor_level(o, hi.z * 0.4)
        if hi.z - fz > H_MAX:
            k = H_MAX / (hi.z - fz)
            transform(o, Matrix.Scale(k, 4))
            fz *= k
        transform(o, Matrix.Translation((0, 0, -fz)))
        o.name = "HI"
        self.o = o
        self.t = bvh_of(o)
        self.glass = glass_faces(o, style == "lantern")
        self.lo, self.hi = bounds(o)

    def edge(self, a):
        """The terrace's edge at azimuth a: walk out along the floor (a low step or a porch on it counts as floor)."""
        d = az(a)
        r, last = 0.5, None
        while r < 15.0:
            h, _, _ = ray(self.t, Vector((0, 0, 0.6)) + d * r, Vector((0, 0, -1)), 1.2)
            if h is not None and -0.08 < h.z < 0.4:
                last = r
            elif last is not None and r - last > 1.0:
                break
            r += 0.05
        return last

    def measure(self):
        t, cfg = self.t, self.cfg
        # the room's middle a metre up: halfway between opposite walls
        c = Vector((0.0, 0.0, 1.2))
        for _ in range(4):
            for d in (Vector((1, 0, 0)), Vector((0, 1, 0))):
                _, _, a = ray(t, c, d)
                _, _, b = ray(t, c, -d)
                if a and b:
                    c += d * (a - b) * 0.5
        self.rc = Vector((c.x, c.y, 0.0))
        walls = []
        for i in range(72):
            a = 5.0 * i
            _, _, d = ray(t, Vector((c.x, c.y, 1.2)), az(a))
            walls.append((a, d or 3.0))
        med = sorted(d for _, d in walls)[len(walls) // 2]
        self.walls = [(a, min(d, med * 1.3)) for a, d in walls]
        self.R = med
        # how high the walls go, and the room's top straight up
        wall_top, z = None, 0.5
        while z < 16.0 and wall_top is None:
            n = 0
            for i in range(16):
                h, hn, _, _ = t.ray_cast(Vector((c.x, c.y, z)), az(22.5 * i), med * 1.4)
                n += h is not None and abs(hn.z) < 0.7
            if n < 12:
                wall_top = z
            z += 0.1
        loc, nor, _, _ = t.ray_cast(c, Vector((0, 0, 1)), 40.0)
        self.top = loc.z if loc is not None else 3.0
        self.wall_top = wall_top or self.top
        self.ceiling = cfg.get("ceiling")
        # the room's height: its ceiling, else its walls (a round room with its own roof), else its shell's top
        if self.ceiling:
            self.H = self.ceiling
        elif cfg["room"] == "cyl":
            self.H = min(self.top, self.wall_top)
        else:
            self.H = self.top
        # the terrace's outline
        self.outline = [(a, self.edge(a) or 0.0) for a in [3.75 * i for i in range(96)]]
        log(self.style, "room centre", tuple(round(v, 2) for v in self.rc), "R", round(self.R, 2), "H", round(self.H, 2),
            "walls to", round(self.wall_top, 2), "top", round(self.top, 2))

    def profile(self, step=0.2):
        """The room's radius up its height (revolve, lantern): the median distance from its axis to the shell."""
        prof = []
        z = 0.05
        while z < self.H - 0.05:
            ds = []
            for i in range(24):
                _, _, d = ray(self.t, Vector((self.rc.x, self.rc.y, z)), az(15 * i + 7.5), 30.0)
                if d:
                    ds.append(d)
            if len(ds) < 12:
                break
            prof.append((z, sorted(ds)[len(ds) // 2]))
            z += step
        return prof

    def openings(self, near_surface):
        """The window openings: groups of glass faces on the room's shell (near_surface(p) says it is), each as
        (centre, outward normal, width, height)."""
        o = self.o
        me = o.data
        faces = [i for i in self.glass if near_surface(me.polygons[i].center)]
        out = []
        for g in clusters(o, faces):
            area = sum(me.polygons[i].area for i in g)
            if area < 0.05:
                continue
            c = sum((me.polygons[i].center * me.polygons[i].area for i in g), Vector()) / area
            n = sum((me.polygons[i].normal * me.polygons[i].area for i in g), Vector())
            radial = Vector((c.x - self.rc.x, c.y - self.rc.y, 0)).normalized()
            if n.length < 1e-6 or n.normalized().dot(radial) < 0:
                n = radial if n.length < 1e-6 or n.dot(radial) < 0 else n
            n = n.normalized()
            u = Vector((0, 0, 1)).cross(n).normalized() if abs(n.z) < 0.95 else Vector((1, 0, 0))
            v = n.cross(u)
            pts = [o.data.vertices[k].co for i in g for k in me.polygons[i].vertices]
            us = [(p - c).dot(u) for p in pts]
            vs = [(p - c).dot(v) for p in pts]
            out.append((c, n, max(us) - min(us), max(vs) - min(vs)))
        return out


# --- the parts -----------------------------------------------------------------------------------------------------

def terrace(bm, outline, mi_top):
    """The slab: its top at 0 along the outline, its edge down to BASE."""
    ring = [Vector((r * math.cos(math.radians(a)), r * math.sin(math.radians(a)), 0)) for a, r in outline if r > 0.5]
    top = [bm.verts.new(p) for p in ring]
    bot = [bm.verts.new(p + Vector((0, 0, BASE))) for p in ring]
    f = bm.faces.new(top)
    f.material_index = mi_top
    f.normal_update()
    if f.normal.z < 0:
        f.normal_flip()
    n = len(ring)
    c = sum(ring, Vector()) / n
    for i in range(n):
        j = (i + 1) % n
        s = bm.faces.new((top[j], top[i], bot[i], bot[j]))
        s.material_index = mi_top
        s.normal_update()
        if s.normal.dot(s.calc_center_median() - c) < 0:
            s.normal_flip()


def railing(bm, outline, skip, mi):
    """Posts and two rails RAIL_IN inside the outline, not where skip(p) (the gaps), with a post at each end."""
    pts = []
    for a, r in outline:
        if r > 0.5:
            pts.append(Vector(((r - RAIL_IN) * math.cos(math.radians(a)), (r - RAIL_IN) * math.sin(math.radians(a)), 0)))
    n = len(pts)
    runs, cur = [], []
    start = next((i for i in range(n) if skip(pts[i])), None)
    order = list(range(n)) if start is None else [(start + k) % n for k in range(n + 1)]
    for i in order:
        if skip(pts[i]):
            if len(cur) > 1:
                runs.append(cur)
            cur = []
        else:
            cur.append(pts[i])
    if len(cur) > 1:
        runs.append(cur)
    for run in runs:
        acc = 0.0
        box(bm, run[0] + Vector((0, 0, RAIL_H / 2)), (0.06, 0.06, RAIL_H), mi)
        for a, b in zip(run, run[1:]):
            for z, s in ((RAIL_H, 0.06), (0.5, 0.035)):
                beam(bm, a + Vector((0, 0, z)), b + Vector((0, 0, z)), s, s, mi)
            acc += (b - a).length
            if acc > 1.5 or b is run[-1]:
                box(bm, b + Vector((0, 0, RAIL_H / 2)), (0.06, 0.06, RAIL_H), mi)
                acc = 0.0


def ring_wall(bm, pts_out, pts_in, z0, z1, mi_out, mi_in, mi_top):
    """A closed wall between an outer and an inner ring of points, from z0 to z1."""
    n = len(pts_out)
    ob = [bm.verts.new((p.x, p.y, z0)) for p in pts_out]
    ot = [bm.verts.new((p.x, p.y, z1)) for p in pts_out]
    ib = [bm.verts.new((p.x, p.y, z0)) for p in pts_in]
    it = [bm.verts.new((p.x, p.y, z1)) for p in pts_in]
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((ob[i], ob[j], ot[j], ot[i])).material_index = mi_out
        bm.faces.new((ib[j], ib[i], it[i], it[j])).material_index = mi_in
        bm.faces.new((ot[i], ot[j], it[j], it[i])).material_index = mi_top
        bm.faces.new((ob[j], ob[i], ib[i], ib[j])).material_index = mi_top


def slab(bm, pts, z0, z1, mi_top, mi_bottom, mi_side):
    """A closed slab over a ring of points."""
    b = [bm.verts.new((p.x, p.y, z0)) for p in pts]
    t = [bm.verts.new((p.x, p.y, z1)) for p in pts]
    bm.faces.new(t).material_index = mi_top
    bm.faces.new(b[::-1]).material_index = mi_bottom
    n = len(pts)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((b[i], b[j], t[j], t[i])).material_index = mi_side


def revolve_shell(bm, prof, rc, seg, mi_out, mi_in, rot0=0.0):
    """A closed shell turned from prof [(z, r)]: outside at r, inside WALL in, closed at the bottom and the top."""
    def ring(z, r):
        return [bm.verts.new((rc.x + r * math.cos(math.radians(rot0 + 360.0 * i / seg)),
                              rc.y + r * math.sin(math.radians(rot0 + 360.0 * i / seg)), z)) for i in range(seg)]
    outs = [ring(z, r) for z, r in prof]
    ins = [ring(z, max(0.05, r - WALL)) for z, r in prof]
    for rings, mi, flip in ((outs, mi_out, False), (ins, mi_in, True)):
        for a, b in zip(rings, rings[1:]):
            for i in range(seg):
                j = (i + 1) % seg
                vs = (a[i], a[j], b[j], b[i])
                bm.faces.new(vs[::-1] if flip else vs).material_index = mi
    # the bottom: outer to inner; the top: closed by caps
    for i in range(seg):
        j = (i + 1) % seg
        bm.faces.new((outs[0][j], outs[0][i], ins[0][i], ins[0][j])).material_index = mi_out
    bm.faces.new(outs[-1]).material_index = mi_out
    bm.faces.new(ins[-1][::-1]).material_index = mi_in
    return outs, ins


def cutters(bm, ops, depth=1.0):
    """The opening cutters: boxes and 16-sided cylinders along each opening's normal."""
    for kind, c, n, w, h in ops:
        u = Vector((0, 0, 1)).cross(n).normalized() if abs(n.z) < 0.95 else Vector((1, 0, 0))
        v = n.cross(u)
        rot = Matrix((n, u, v)).transposed()
        if kind in ("box", "door"):
            box(bm, c, (depth, w, h), 0, rot)
        else:
            r = (w + h) / 4
            k = 16
            a = [bm.verts.new(c + rot @ Vector((-depth / 2, r * math.cos(2 * math.pi * i / k), r * math.sin(2 * math.pi * i / k)))) for i in range(k)]
            b = [bm.verts.new(c + rot @ Vector((depth / 2, r * math.cos(2 * math.pi * i / k), r * math.sin(2 * math.pi * i / k)))) for i in range(k)]
            bm.faces.new(a[::-1])
            bm.faces.new(b)
            for i in range(k):
                j = (i + 1) % k
                bm.faces.new((a[i], a[j], b[j], b[i]))


def panes(bm, ops, mi):
    """A pane of glass, 2 cm thick, in each window opening (not the door)."""
    for kind, c, n, w, h in ops:
        if kind == "door":
            continue
        u = Vector((0, 0, 1)).cross(n).normalized() if abs(n.z) < 0.95 else Vector((1, 0, 0))
        v = n.cross(u)
        rot = Matrix((n, u, v)).transposed()
        if kind == "box":
            box(bm, c, (0.02, w + 0.04, h + 0.04), mi, rot)
        else:
            r = (w + h) / 4 + 0.02
            k = 16
            a = [bm.verts.new(c + rot @ Vector((-0.01, r * math.cos(2 * math.pi * i / k), r * math.sin(2 * math.pi * i / k)))) for i in range(k)]
            b = [bm.verts.new(c + rot @ Vector((0.01, r * math.cos(2 * math.pi * i / k), r * math.sin(2 * math.pi * i / k)))) for i in range(k)]
            bm.faces.new(a[::-1]).material_index = mi
            bm.faces.new(b).material_index = mi
            for i in range(k):
                j = (i + 1) % k
                bm.faces.new((a[i], a[j], b[j], b[i])).material_index = mi


def boolean_cut(o, cut):
    mod = o.modifiers.new("cut", "BOOLEAN")
    mod.operation = "DIFFERENCE"
    mod.solver = "EXACT"
    mod.object = cut
    mod.use_self = True
    mod.material_mode = "TRANSFER"
    apply_mod(o, mod)


# --- one vestibule -------------------------------------------------------------------------------------------------

def build(style, cfg):
    reset()
    M = Model(style, cfg)
    M.measure()
    if cfg.get("room_r"):
        M.R = cfg["room_r"]           # a bigger hall than the model's (the tower's core rises in it)
    t, rc = M.t, M.rc
    mats = [material("EXT_" + style)] + [material(k, v) for k, v in PALETTE.items()]
    EXT, GLS, INT, TER, CHR = range(5)

    # --- the door: where the model shows it, or the nearest place along the wall with a clear way out to the terrace
    def low_clutter(q):
        return q.z < 0.4 and t.find_nearest(Vector((q.x, q.y, 2.1)), 0.3)[0] is None

    def way_out(hit, n):
        side = Vector((-n.y, n.x, 0))
        for k in range(4, 22):
            for lat in (-0.45, 0.0, 0.45):
                for z in (0.42, 0.9, 1.4, 1.75):
                    c = Vector((hit.x, hit.y, z)) + n * (0.1 * k) + side * lat
                    for q, _, _, _ in t.find_nearest_range(c, 0.34):
                        if (Vector((q.x, q.y, 0)) - rc).length < M.R + (0.95 if q.z < 1.6 else 0.6) or low_clutter(q):
                            continue
                        if os.environ.get("TR_WHY"):
                            log(style, "door way blocked at", tuple(round(x, 2) for x in q), "k", k)
                        return False
            fl, _, _ = ray(t, Vector((hit.x, hit.y, 0.5)) + n * (0.1 * k), Vector((0, 0, -1)), 1.0)
            if fl is None or abs(fl.z) > 0.4:
                if os.environ.get("TR_WHY"):
                    log(style, "door way: no floor", k, fl)
                return False
        return True

    cands = [(cfg["door"], cfg.get("door_shift", 0.0))]
    if cfg["room"] == "box":
        cands += [(a, sh) for a in (cfg["door"], cfg["door"] + 90, cfg["door"] - 90, cfg["door"] + 180)
                  for sh in (0.0, 1.2, -1.2, 2.4, -2.4)]
    else:
        cands += [(cfg["door"] + da, 0.0) for da in sorted(range(-90, 95, 10), key=abs)]
    for a, sft in cands:
        n_out = az(a)
        u = Vector((-n_out.y, n_out.x, 0))
        hit, _, _ = ray(t, rc + Vector((0, 0, 1.2)) + u * sft, n_out)
        if hit is not None and way_out(hit, n_out):
            break
    else:
        raise RuntimeError(style + ": no clear way out of any door")
    log(style, "door azimuth", a, "shift", sft)

    # --- the room
    kind = cfg["room"]
    bm = bmesh.new()
    ops = []                      # (kind, centre, normal, w, h): openings
    H = M.H
    if kind in ("cyl", "box"):
        if kind == "cyl":
            seg = 40
            R = M.R + 0.03
            outer = [rc + az(360.0 * i / seg) * R for i in range(seg)]
            inner = [rc + az(360.0 * i / seg) * (R - WALL) for i in range(seg)]
        else:
            # a rectangle: its half sizes from the walls straight out along X and Y
            hx = (min(d for a, d in M.walls if abs(((a + 180) % 360) - 180) < 3) +
                  min(d for a, d in M.walls if abs(((a - 180 + 180) % 360) - 180) < 3)) / 2 + 0.03
            hy = (min(d for a, d in M.walls if abs(((a - 90 + 180) % 360) - 180) < 3) +
                  min(d for a, d in M.walls if abs(((a - 270 + 180) % 360) - 180) < 3)) / 2 + 0.03
            if cfg.get("room_half"):
                hx, hy = cfg["room_half"]     # a bigger hall than the model's (the tower's core rises in it)
            R = min(hx, hy)
            outer = [rc + Vector((sx * hx, sy * hy, 0)) for sx, sy in ((1, 1), (-1, 1), (-1, -1), (1, -1))]
            inner = [rc + Vector((sx * (hx - WALL), sy * (hy - WALL), 0)) for sx, sy in ((1, 1), (-1, 1), (-1, -1), (1, -1))]
        ring_wall(bm, outer, inner, 0.0, H, EXT, INT, EXT)
        # the roof slab over it, a little wider
        roof = [rc + (p - rc) * ((R + 0.15) / R) for p in outer]
        slab(bm, roof, H, H + 0.18, EXT, INT, EXT)
        # the windows: the model's glass on its wall
        near = lambda p: 0.45 < p.z < H - 0.2 and abs((Vector((p.x, p.y, 0)) - rc).length - M.R) < 0.5 \
            if kind == "cyl" else 0.45 < p.z < H - 0.2
        for c, n, w, h in (M.openings(near) if cfg.get("windows", True) else []):
            if w < 0.25 or h < 0.25:
                continue
            w, h = min(w, 2.4), min(h, H - 0.8)
            radial = Vector((c.x - rc.x, c.y - rc.y, 0)).normalized()
            if kind == "box":
                # square to the nearest wall
                radial = Vector((1, 0, 0)) if abs(c.x - rc.x) / hx > abs(c.y - rc.y) / hy else Vector((0, 1, 0))
                radial *= 1 if (c - rc).dot(radial) > 0 else -1
            z = min(max(c.z, 0.6 + h / 2), H - 0.3 - h / 2)
            p = rc + radial * (R - WALL / 2) + Vector((0, 0, z)) if kind == "cyl" else \
                Vector((c.x, c.y, z)) + radial * 0.0
            if kind == "box":
                if abs(radial.x) > 0.5:
                    p = Vector((rc.x + radial.x * (hx - WALL / 2), c.y, z))
                else:
                    p = Vector((c.x, rc.y + radial.y * (hy - WALL / 2), z))
            if abs((p - hit).dot(u)) < DOOR_W / 2 + max(w, h) / 2 + 0.25 and (p - hit).dot(n_out) > -1.0:
                continue                        # not over the door
            ops.append(("round" if 0.7 < w / h < 1.4 else "box", p, radial, w, h))
        if kind == "box" and cfg.get("ribbon"):
            # a ribbon of windows along every wall (the model's dark band under the roof)
            z0, z1 = cfg["ribbon"]
            for nrm, half, other in ((Vector((1, 0, 0)), hy, hx), (Vector((-1, 0, 0)), hy, hx),
                                     (Vector((0, 1, 0)), hx, hy), (Vector((0, -1, 0)), hx, hy)):
                p = rc + nrm * (other - WALL / 2) + Vector((0, 0, (z0 + z1) / 2))
                ops.append(("box", p, nrm, 2 * half - 0.8, z1 - z0))
    elif kind == "revolve":
        prof = M.profile(0.35)
        prof = [(z, r + 0.03) for z, r in prof]
        if prof[-1][0] < M.H - 0.1:
            prof.append((M.H, 0.25))
        revolve_shell(bm, prof, rc, 36, EXT, INT)
        R = prof[0][1]
        prof_r = lambda z: min(prof, key=lambda q: abs(q[0] - z))[1]
        near = lambda p: 0.45 < p.z < M.H - 0.6 and abs((Vector((p.x, p.y, 0)) - rc).length - prof_r(p.z)) < 0.5
        for c, n, w, h in M.openings(near):
            if w < 0.2 or h < 0.2 or abs(n.z) > 0.7 or c.z > M.H - 1.5:
                continue                        # not the skylight round the top
            if abs((c - hit).dot(u)) < DOOR_W / 2 + max(w, h) / 2 + 0.25 and c.z < DOOR_H + 0.5 and (c - hit).dot(n_out) > -1.0:
                continue
            ops.append(("round" if 0.6 < w / h < 1.6 else "box", c, n, min(w, 1.2), min(h, 1.2)))
    elif kind == "lantern":
        prof = M.profile(0.4)
        gz = [M.o.data.polygons[i].center.z for i in M.glass]
        g0 = max(0.5, sorted(gz)[len(gz) // 20])
        # the facets: twelve, turned to the model's ribs (where the shell is farthest at 1.2 m)
        far = max(range(72), key=lambda i: M.walls[i][1])
        rot0 = (M.walls[far][0]) % 30.0
        seg = 12
        k = 1.0 / math.cos(math.pi / seg)   # a polygon's corner radius for the median (face) radius
        prof = [(z, r * k + 0.03) for z, r in prof]
        if prof[-1][0] < M.H - 0.1:
            prof.append((M.H, 0.4))
        R = prof[0][1]
        # the plinth up to g0, solid; glass over it; the ribs on the facets' corners
        low = [q for q in prof if q[0] <= g0] or prof[:1]
        high = [q for q in prof if q[0] >= g0]
        outs, ins = revolve_shell(bm, low + [(g0, min(prof, key=lambda q: abs(q[0] - g0))[1])], rc, seg, EXT, INT, rot0)
        gb = bmesh.new()
        go, gi = revolve_shell(gb, high, rc, seg, GLS, GLS, rot0)
        gl = new_object("glass", gb, mats)
        for i in range(seg):
            a = math.radians(rot0 + 360.0 * i / seg)
            d = Vector((math.cos(a), math.sin(a), 0))
            for (z0, r0), (z1, r1) in zip(high, high[1:]):
                beam(bm, rc + d * (r0 + 0.04) + Vector((0, 0, z0)), rc + d * (r1 + 0.04) + Vector((0, 0, z1)), 0.14, 0.14, EXT)
        for z in (g0, high[-1][0] - 0.05):
            r = min(prof, key=lambda q: abs(q[0] - z))[1]
            for i in range(seg):
                a0, a1 = math.radians(rot0 + 360.0 * i / seg), math.radians(rot0 + 360.0 * (i + 1) / seg)
                beam(bm, rc + Vector((math.cos(a0), math.sin(a0), 0)) * (r + 0.04) + Vector((0, 0, z)),
                     rc + Vector((math.cos(a1), math.sin(a1), 0)) * (r + 0.04) + Vector((0, 0, z)), 0.14, 0.14, EXT)
    room = new_object("room", bm, mats)

    # the door, square to the wall where it is
    if kind == "box":
        nd = Vector((1, 0, 0)) if abs(n_out.x) > abs(n_out.y) else Vector((0, 1, 0))
        nd *= 1 if nd.dot(n_out) > 0 else -1
        n_out = nd
        u = Vector((-n_out.y, n_out.x, 0))
    loc, nor, _, _ = bvh_of(gl if kind == "lantern" else room).ray_cast(rc + Vector((0, 0, 1.2)) + u * sft, n_out, 30.0)
    if loc is None:
        raise RuntimeError(style + ": the new room has no wall toward the door")
    door_o = Vector((loc.x, loc.y, 0.0))
    cb = bmesh.new()
    door_op = ("box", door_o + Vector((0, 0, DOOR_H / 2 - 0.05)), n_out, DOOR_W, DOOR_H + 0.1)
    cutters(cb, ops + [door_op], depth=1.2 if kind != "lantern" else 2.0)
    cut = new_object("cut", cb, [])
    boolean_cut(room, cut)
    if kind == "lantern":
        boolean_cut(gl, cut)
    bpy.data.objects.remove(cut)
    log(style, "room", kind, "R", round(R, 2), "H", round(H, 2), "windows", len(ops), "door at",
        tuple(round(v, 2) for v in door_o))

    # the room's floor and the threshold through the wall
    bm = bmesh.new()
    fr = [rc + (Vector((v.co.x, v.co.y, 0)) - rc) for v in room.data.vertices if abs(v.co.z) < 1e-4]
    seg = 48
    rr = []
    rt = bvh_of(room)
    for i in range(seg):
        _, _, d = ray(rt, rc + Vector((0, 0, 0.3)), az(360.0 * i / seg), 30.0)
        rr.append(rc + az(360.0 * i / seg) * ((d or R - WALL) + 0.01) + Vector((0, 0, 0.01)))
    fl = [bm.verts.new(p) for p in rr]
    bm.faces.new(fl).material_index = TER
    quad(bm, [door_o + u * su * (DOOR_W / 2 + 0.01) + n_out * sn + Vector((0, 0, 0.01)) for su, sn in
              ((-1, -WALL - 0.3), (1, -WALL - 0.3), (1, 0.3), (-1, 0.3))], TER)
    panes(bm, ops, GLS)
    base = new_object("base", bm, mats)

    # --- the canopy: the model's own parts over the terrace that are neither the room nor the railing
    canopy = make_canopy(M, room, R, H, mats)

    # --- the railing comes last, once the gaps are known (after the turn)
    return M, mats, room, base, canopy, (gl if kind == "lantern" else None), door_o, n_out, R, H


def make_canopy(M, room, R, H, mats):
    o = M.o
    me = o.data
    rc = M.rc
    edge_r = dict(M.outline)
    keep = []
    prof = M.profile() if M.cfg["room"] in ("revolve", "lantern") else []
    k = 1.0 / math.cos(math.pi / 12) if M.cfg["room"] == "lantern" else 1.0
    prof_r = lambda z: (min(prof, key=lambda q: abs(q[0] - z))[1] * k) if prof and z <= prof[-1][0] + 0.3 else 0.6
    for p in me.polygons:
        c = p.center
        if c.z < 0.06:
            continue
        rel = Vector((c.x - rc.x, c.y - rc.y, 0))
        a = (math.degrees(math.atan2(c.y, c.x)) % 360.0)
        er = edge_r.get(round(a / 3.75) % 96 * 3.75, 10.0)
        # the room: inside or on its shell (its own roof included)
        if M.cfg["room"] in ("revolve", "lantern"):
            # the shell turned from the model's profile (the lantern's corners stand out to the facets' corners)
            if c.z < H + 0.4 and rel.length < prof_r(c.z) + 0.6:
                continue
        elif (rel.length < R + 0.6 and c.z < H + 0.6) or (rel.length < R + 0.9 and c.z < 1.6):
            continue                    # the room, and the plinth or steps round its foot
        # the railing, the medallion, steps and porches: low, with nothing over them (a canopy's foot has)
        if (Vector((c.x, c.y, 0)).length > er - 0.6 and c.z < 1.35) or c.z < 0.4:
            up = M.t.find_nearest(Vector((c.x, c.y, 2.1)), 0.5)
            if up[0] is None:
                continue
        keep.append(p.index)
    if len(keep) < 50:
        return None
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.faces.ensure_lookup_table()
    ks = set(keep)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.index not in ks], context="FACES")
    c = new_object("canopy", bm, [mats[0]])
    # the shells are open sheets: thickened first so the voxels see them, then remeshed watertight
    mod = c.modifiers.new("thick", "SOLIDIFY")
    mod.thickness = 0.12
    mod.offset = 0.0
    apply_mod(c, mod)
    mod = c.modifiers.new("remesh", "REMESH")
    mod.mode = "VOXEL"
    mod.voxel_size = 0.05
    mod.adaptivity = 0.0
    apply_mod(c, mod)
    # drop crumbs (bits of rail, loose specks)
    bm = bmesh.new()
    bm.from_mesh(c.data)
    islands = []
    seen = set()
    for f in bm.faces:
        if f in seen:
            continue
        isl, todo = [], [f]
        seen.add(f)
        while todo:
            g = todo.pop()
            isl.append(g)
            for e in g.edges:
                for h in e.link_faces:
                    if h not in seen:
                        seen.add(h)
                        todo.append(h)
        islands.append(isl)
    # crumbs, and low clutter (a porch, a planter): islands that don't rise over 2.2 m
    drop = [f for isl in islands if sum(f.calc_area() for f in isl) < 2.0 or
            max(v.co.z for g in isl for v in g.verts) < 2.2 for f in isl]
    bmesh.ops.delete(bm, geom=drop, context="FACES")
    bm.to_mesh(c.data)
    bm.free()
    n = decimate(c, CANOPY_TRIS)
    log(M.style, "canopy", n, "triangles from", len(keep), "faces;", len(islands), "islands,", len(drop), "faces dropped")
    return c


# --- the texture ---------------------------------------------------------------------------------------------------

def bake(style, M, objs):
    """Unwraps the outside (slot 0) of the objects into one atlas and bakes the model's colour onto it."""
    ext = []
    for o in objs:
        bm = bmesh.new()
        bm.from_mesh(o.data)
        bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.material_index != 0], context="FACES")
        if not bm.faces:
            bm.free()
            continue
        e = new_object(o.name + "_ext", bm, [o.data.materials[0]])
        e.matrix_world = o.matrix_world
        ext.append(e)
        bm = bmesh.new()
        bm.from_mesh(o.data)
        bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.material_index == 0], context="FACES")
        bm.to_mesh(o.data)
        bm.free()
    activate(ext[0])
    for e in ext:
        e.select_set(True)
    bpy.ops.object.join()
    E = bpy.context.view_layer.objects.active
    E.name = "EXT"
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.004, area_weight=0.0, scale_to_bounds=True)
    bpy.ops.object.mode_set(mode="OBJECT")
    img = bpy.data.images.new("T_TR_Vestibule_" + style.capitalize(), TEX, TEX)
    m = bpy.data.materials.new("MI_TR_Vestibule_" + style.capitalize())
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = img
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.7
    nt.nodes.active = tex
    E.data.materials.clear()
    E.data.materials.append(m)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.samples = 1
    sc.cycles.device = "CPU"
    sc.render.bake.use_selected_to_active = True
    sc.render.bake.cage_extrusion = 0.3
    sc.render.bake.max_ray_distance = 0.8
    sc.render.bake.margin = 6
    bpy.ops.object.select_all(action="DESELECT")
    M.o.select_set(True)
    E.select_set(True)
    bpy.context.view_layer.objects.active = E
    bpy.ops.object.bake(type="DIFFUSE", pass_filter={"COLOR"})
    img.pack()
    return E


# --- assembling, the turn, the gaps, the file ----------------------------------------------------------------------

def poly_r(poly, phi):
    """The distance from the origin to a star-shaped polygon along the direction phi (radians)."""
    d = Vector((math.cos(phi), math.sin(phi)))
    best = 0.0
    n = len(poly)
    for i in range(n):
        a, b = Vector(poly[i]), Vector(poly[(i + 1) % n])
        e = b - a
        den = d.x * e.y - d.y * e.x
        if abs(den) < 1e-9:
            continue
        t = (a.x * e.y - a.y * e.x) / den
        u = (a.x * d.y - a.y * d.x) / den
        if t > 0 and -1e-6 <= u <= 1 + 1e-6:
            best = max(best, t)
    return best


def inside(poly, p):
    x, y = p
    c = False
    n = len(poly)
    for i in range(n):
        x1, y1 = poly[i]
        x2, y2 = poly[(i + 1) % n]
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1:
            c = not c
    return c


def metre_uv(o, slots):
    """Metre UVs (a box projection) on the faces of the given material slots."""
    me = o.data
    uv = me.uv_layers.get("UVMap") or me.uv_layers.new(name="UVMap")
    for poly in me.polygons:
        if poly.material_index not in slots:
            continue
        n = poly.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        for li in poly.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            if ax == 2:
                uv.data[li].uv = (co.x, co.y)
            elif ax == 1:
                uv.data[li].uv = (co.x, co.z)
            else:
                uv.data[li].uv = (co.y, co.z)


STOP_HALF = 3.25        # the stop zone: 4.5 m and a metre each side
EDGE_RAIL = 0.45        # the railing this far in from a boarding edge (nothing within 1.35 m of a track's centre)
CORE = 2.6              # half the floor opening for the tower's core (5.2 m)


def finish(style, cfg, parts):
    M, mats, room, base, canopy, glass, door_o, n_out, R, H = parts
    rc = M.rc
    S = SLOT
    if cfg.get("trim_y"):
        lim = [M.edge(90), M.edge(-90)]
        out = []
        for a_, r in M.outline:
            sn = math.sin(math.radians(a_))
            if abs(sn) > 1e-3:
                r = min(r, (lim[0] if sn > 0 else lim[1]) / abs(sn))
            out.append((a_, r))
        M.outline = out
    objs = [o for o in (room, base, canopy, glass) if o]
    outline = [(r * math.cos(math.radians(a)), r * math.sin(math.radians(a))) for a, r in M.outline if r > 0.5]
    pts = outline + [(v.co.x, v.co.y) for o in objs for v in o.data.vertices if v.co.z > 0.02]

    # --- the turn: the line along X across the vestibule's narrowest way (no longer than 21.8 m along it)
    best = None
    for th in ([cfg["turn"]] if "turn" in cfg else range(0, 180, 5)):
        c, s_ = math.cos(math.radians(th)), math.sin(math.radians(th))
        xs = [x * c + y * s_ for x, y in pts]
        ys = [-x * s_ + y * c for x, y in pts]
        if max(xs) - min(xs) > 21.8:
            continue
        key = max(ys) - min(ys)
        if best is None or key < best[0]:
            best = (key, th, (max(xs) + min(xs)) / 2, (max(ys) + min(ys)) / 2)
    yext, th, cx, cy = best
    E = yext / 2 + 0.35 + cfg.get("edge_extra", 0.0)      # (edge_extra: a walk along the edge past canopy feet)
    G = 2 * (E + 1.05)
    rot = Matrix.Rotation(-math.radians(th), 4, "Z")
    m = Matrix.Translation((-cx, -cy, 0)) @ rot
    rot3 = rot.to_3x3()
    for o in objs:
        o.data.transform(m)
        o.data.update()
    M.o.data.transform(m)
    M.o.data.update()
    rc = m @ rc
    door_o = m @ door_o
    n_out = (rot3 @ n_out).normalized()
    outline = [tuple((m @ Vector((x, y, 0)))[:2]) for x, y in outline]
    log(style, "turned", th, "half width", round(E, 2), "track gap", round(G, 2))

    # --- the terrace: the model's outline, widened to a straight boarding edge along each track, cut at |y| = E
    def terr_r(phi):
        r = poly_r(outline, phi)
        c_, s_ = abs(math.cos(phi)), abs(math.sin(phi))
        rect = min(4.5 / c_ if c_ > 1e-6 else 1e9, E / s_ if s_ > 1e-6 else 1e9)
        r = max(r, rect)
        return min(r, E / s_) if s_ > 1e-6 else r
    P = []
    for i in range(720):
        phi = math.radians(i * 0.5)
        r = terr_r(phi)
        P.append((r * math.cos(phi), r * math.sin(phi)))

    # --- the tower's core: a 5.2 m opening on the line's axis (y = 0), outside a small room, or in the middle of a big
    # one (the dome, the lantern: the lift and the stair rise in the hall), clear of anything under 2.9 m
    blockers = [o for o in (room, canopy, glass) if o]
    big = R >= 4.8
    if cfg["room"] == "box":
        rv = [v.co for v in room.data.vertices if v.co.z < H - 0.05]
        room_half = ((max(v.x for v in rv) - min(v.x for v in rv)) / 2, (max(v.y for v in rv) - min(v.y for v in rv)) / 2)
        big = min(room_half) - WALL - 0.9 >= CORE
    low = [Vector(v.co) for o in (blockers if not big else [o for o in (canopy,) if o]) for v in o.data.vertices
           if 0.02 < v.co.z < 2.9]
    cand = None
    for strict in (True, False):          # first on the terrace as it is, then letting it grow a bay round the core
        if cand is not None:
            break
        for xi in range(0, 45):
            for sx in (1, -1):
                xc = sx * xi * 0.25
                q = Vector((xc, 0.0, 0))
                if big and cfg["room"] == "box":
                    if abs(xc - rc.x) + CORE > room_half[0] - WALL - 0.9 or abs(rc.y) + CORE > room_half[1] - WALL - 0.9:
                        continue
                    score = abs(xc - rc.x)
                elif big:
                    far = max((Vector((xc + dx * CORE, dy * CORE, 0)) - rc).length for dx in (-1, 1) for dy in (-1, 1))
                    if far > R - WALL - 1.0:
                        continue
                    score = (q - rc).length
                else:
                    corners = [(xc + dx * (CORE + 0.4), dy * (CORE + 0.4)) for dx in (-1, 0, 1) for dy in (-1, 0, 1)]
                    if strict and not all(inside(P, c_) for c_ in corners):
                        continue
                    if not strict and abs(xc) + CORE + 1.0 > 10.9:
                        continue                    # the length along the line stays within 22 m
                    if cfg["room"] == "box":
                        bx, by_ = room_half
                        dq = Vector((max(abs(rc.x - xc) - CORE - bx, 0), max(abs(rc.y) - CORE - by_, 0), 0)).length
                        if dq < 1.6:
                            continue
                    else:
                        dq = Vector((max(abs(rc.x - xc) - CORE, 0), max(abs(rc.y) - CORE, 0), 0)).length
                        if dq < R + 1.6:
                            continue                # a 1.6 m way between the core and the room
                    score = dq
                if any(abs(v.x - xc) < CORE + 0.2 and abs(v.y) < CORE + 0.2 for v in low):
                    continue
                if cand is None or score < cand[0]:
                    cand = (score, q)
    if cand is None:
        raise RuntimeError(style + ": no room for the tower's core on the line's axis")
    core = cand[1]
    corners = [(core.x + dx * (CORE + 0.4), dy * (CORE + 0.4)) for dx in (-1, 1) for dy in (-1, 1)]
    if not all(inside(P, c_) for c_ in corners):
        # the terrace grows round the core (a bay out at its end)
        bx0, bx1, by = core.x - CORE - 1.0, core.x + CORE + 1.0, CORE + 1.0

        def bay_r(phi):
            d = Vector((math.cos(phi), math.sin(phi)))
            best_ = 0.0
            for t_ in [k * 0.05 for k in range(1, 500)]:
                q_ = d * t_
                if bx0 <= q_.x <= bx1 and -by <= q_.y <= by:
                    best_ = t_
            return best_
        P = []
        for i in range(720):
            phi = math.radians(i * 0.5)
            r = max(terr_r(phi), bay_r(phi))
            P.append((r * math.cos(phi), r * math.sin(phi)))
        log(style, "terrace grown round the core")
    to = (n_out if big else (rc - core))
    cdir = Vector((1, 0, 0)) * (1 if to.x > 0 else -1) if abs(to.x) >= abs(to.y) else \
        Vector((0, 1, 0)) * (1 if to.y > 0 else -1)
    log(style, "core at", tuple(round(v, 2) for v in core), "in the room" if big else "on the terrace", "exit", tuple(cdir))

    # the stops: where both edges are clear of canopy feet for the stop zone (4.5 m and a metre each side)
    lowv = [Vector(v.co) for o in (canopy, room, glass) if o for v in o.data.vertices if v.co.z < 2.5]
    stop_x = 0.0
    for sx_ in sorted([k * 0.25 for k in range(-16, 17)], key=abs):
        if not any(abs(v.x - sx_) < STOP_HALF + 0.2 and abs(v.y) > E - 1.4 for v in lowv):
            stop_x = sx_
            break
    else:
        log(style, "no stop clear of the canopy on both edges")
    log(style, "stop at", stop_x)

    # the slab: top and edge baked from the model, the underside and the core's hole concrete
    bm = bmesh.new()
    top = [bm.verts.new((x, y, 0.0)) for x, y in P]
    bot = [bm.verts.new((x, y, BASE)) for x, y in P]
    bm.faces.new(top).material_index = 0
    bm.faces.new(bot[::-1]).material_index = S["TR_Tx_Concrete"]
    n = len(P)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((top[j], top[i], bot[i], bot[j])).material_index = 0
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    slabo = new_object("terrace", bm, mats)
    cb = bmesh.new()
    box(cb, core + Vector((0, 0, BASE / 2)), (2 * CORE, 2 * CORE, -BASE + 0.4), S["TR_Tx_Concrete"])
    cut = new_object("corecut", cb, mats)
    boolean_cut(slabo, cut)
    if big:
        boolean_cut(base, cut)
    bpy.data.objects.remove(cut)

    # the railing: round the outline, in along the boarding edges, open at the stops
    bm = bmesh.new()
    rail_pts = []
    for x, y in P[::2]:
        r = math.hypot(x, y)
        k = (r - RAIL_IN) / r
        qx, qy = x * k, y * k
        qy = max(-(E - EDGE_RAIL), min(E - EDGE_RAIL, qy))
        rail_pts.append((math.degrees(math.atan2(qy, qx)), math.hypot(qx, qy) + RAIL_IN))
    railing(bm, rail_pts, lambda p: abs(p.y) > E - EDGE_RAIL - 0.05 and abs(p.x - stop_x) < STOP_HALF, S["TR_Chrome"])
    # a rail round the core's hole, open toward the room (the core's own walls stand in the hole)
    # (none: the procedural core closes its hole)
    # the tactile line along each boarding edge
    xs_edge = [x for x, y in P if abs(abs(y) - E) < 1e-3]
    x0, x1 = (min(xs_edge), max(xs_edge)) if xs_edge else (-4.5, 4.5)
    for sy in (1, -1):
        quad(bm, [Vector((xa, sy * yb, 0.004)) for xa, yb in ((x0, E - 0.6), (x1, E - 0.6), (x1, E - 0.1), (x0, E - 0.1))],
             S["TR_Tx_Tactile"], flip=sy < 0)
    rail = new_object("railing", bm, mats)
    for o in (rail,):
        bm2 = bmesh.new()
        bm2.from_mesh(o.data)
        for f in bm2.faces:
            f.normal_update()
            if f.material_index == S["TR_Tx_Tactile"] and f.normal.z < 0:
                f.normal_flip()
        bm2.to_mesh(o.data)
        bm2.free()

    # nothing within 1.35 m of a track's centre: cut whatever of ours reaches past E - 0.3 above the floor
    for o in [room, canopy, glass]:
        if not o:
            continue
        bm = bmesh.new()
        bm.from_mesh(o.data)
        for sy in (1, -1):
            geom = list(bm.verts) + list(bm.edges) + list(bm.faces)
            bmesh.ops.bisect_plane(bm, geom=geom, plane_co=Vector((0, sy * (E - 0.3), 0)), plane_no=Vector((0, sy, 0)),
                                   dist=1e-4, clear_outer=True)
        bm.to_mesh(o.data)
        bm.free()

    tx = {S[k] for k in S if k.startswith("TR_Tx_")}
    objs = [o for o in (room, base, canopy, glass, slabo, rail) if o]
    for o in objs:
        metre_uv(o, tx)
    E_ = bake(style, M, [o for o in objs if o is not rail])
    activate(E_)
    for o in objs:
        o.select_set(True)
    bpy.ops.object.join()
    V = bpy.context.view_layer.objects.active
    name = "SM_TR_Vestibule_" + style.capitalize()
    V.name = V.data.name = name
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.remove_doubles(threshold=0.0005)
    bpy.ops.object.mode_set(mode="OBJECT")
    bm = bmesh.new()
    bm.from_mesh(V.data)
    for f in bm.faces:
        mn = V.data.materials[f.material_index].name
        p = f.calc_center_median()
        if mn.startswith("TR_Interior"):
            toward = Vector((rc.x, rc.y, min(max(p.z, 0.5), H - 0.3))) - p
            if f.normal.dot(toward) < 0:
                f.normal_flip()
        elif mn.startswith(("TR_Terrazzo", "TR_Tx_Tactile")) and f.normal.z < 0:
            f.normal_flip()
    bm.to_mesh(V.data)
    bm.free()
    V.data.update()
    bpy.data.objects.remove(M.o)
    export(V, name)
    lo, hi = bounds(V)
    nt = tris(V)

    def wall_spot(rel, inset):
        a_room = math.degrees(math.atan2(-n_out.y, -n_out.x)) + rel
        loc, _, _, d = bvh_of(V).ray_cast(Vector((rc.x, rc.y, 1.0)), az(a_room), 30.0)
        d = (d or R) - inset
        return Vector((rc.x, rc.y, 0)) + az(a_room) * d, a_room + 180.0

    spots = {}
    for key, rel, inset in (("diagram", 0.0, 0.15), ("kiosk", 70.0, 0.9), ("bench", -70.0, 0.6)):
        p, yaw = wall_spot(rel, inset)
        spots[key] = {"at": ue(p), "yaw": round((-yaw + 180.0) % 360.0 - 180.0, 1)}
    footprint = outline_pts([(v.co.x, v.co.y) for v in V.data.vertices], pad=0.05, clamp=False)
    def side(v):
        # a direction in Blender's frame -> the station frame's side name (UE: y flipped)
        return ("+x" if v.x > 0 else "-x") if abs(v.x) >= abs(v.y) else ("-y" if v.y > 0 else "+y")
    panels = []
    for sy in (1, -1):
        p = Vector((stop_x - (STOP_HALF + 0.8), sy * (E - 1.2), 0))
        panels.append({"at": ue(p)[:2], "yaw": 90.0 if sy < 0 else -90.0})
    data = {
        "mesh": name,
        "tris": nt,
        "frame": "station frame: origin at deck level on the line's centre line midway between the tracks, +X the "
                 "line's way out, +Y right of it (UE)",
        "height": round(hi.z * 100, 1),
        "length": round((hi.x - lo.x) * 100, 1),
        "track_gap": round(G * 100, 1),
        "edge_length": round((x1 - x0) * 100, 1),
        "stop_x": round(stop_x * 100, 1),
        "panels": panels,
        "tower_x": round(core.x * 100, 1),
        "floor_hole": [round((core.x - CORE) * 100, 1), round(-(core.y + CORE) * 100, 1),
                       round((core.x + CORE) * 100, 1), round(-(core.y - CORE) * 100, 1)],
        "core_exit": side(cdir),
        "core_in_room": big,
        "footprint": [[round(x * 100, 1), round(-y * 100, 1)] for x, y in footprint],
        "bounds": [ue(lo), ue(hi)],
        "centre": ue(Vector((rc.x, rc.y, 0))),
        "doors": [{"at": ue(door_o), "yaw": round(-math.degrees(math.atan2(n_out.y, n_out.x)), 1),
                   "hole": [round(DOOR_W * 100, 1), round(DOOR_H * 100, 1)]}],
        "room": {"centre": ue(Vector((rc.x, rc.y, 0))), "radius": round((R - WALL) * 100, 1), "height": round(H * 100, 1),
                 "box": [ue(Vector((rc.x - R, rc.y + R, 0))), ue(Vector((rc.x + R, rc.y - R, H)))]},
        "spots": spots,
    }
    data["tower"] = build_towers(style, cdir, mats)
    debug_render(V, style, rc, door_o)
    log(style, json.dumps(data))
    return data


def lower_deck(st, T, part, mats, mi_w, mi_b, mi_in):
    """Zenit's second line, 6.5 m under the saucer's deck at its own angle: a piece of the tower (3.5 m) with an
    opening out onto a ring deck round it, with its two boarding edges along that line."""
    S = SLOT
    net = json.load(open(os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Transport", "network_built.json")))
    st_ = [x for x in net["stations"] if x.get("style") == "saucer"][0]
    lv = sorted(st_["levels"], key=lambda l: l["z"])
    up, lo_ = lv[-1], lv[0]
    rel_ue = ((st_["axes"][lo_["line"]] - st_["axes"][up["line"]] + 180.0) % 360.0) - 180.0
    if abs(rel_ue) > 90.0:
        rel_ue -= math.copysign(180.0, rel_ue)          # a deck is the same either way along its line
    dz = lo_["z"] - up["z"]
    rot = Matrix.Rotation(math.radians(-rel_ue), 3, "Z")
    r_t = T.r_out(st["outer"], 0.0)
    E2 = r_t + 0.15 + 1.6
    X0, X1 = -5.5, 9.0
    H2 = 3.5
    data = {}

    def build(bm):
        T.ring(bm, st["outer"], st["inner"], 0.0, H2, mi_w, mi_in, mi_w)
        T.details(bm, st, 0.0, H2, mi_w, mi_b, beam, box)
        # the deck: a slab round the tower along the lower line, its edges at y' = +-E2
        pts = []
        for i in range(64):
            a = 2 * math.pi * i / 64
            d = Vector((math.cos(a), math.sin(a), 0))
            # a rectangle X0..X1 x -E2..E2 with rounded corners (2 m), its edges straight over the stop zone
            rr = []
            cxm, hw, rad = (X0 + X1) / 2, (X1 - X0) / 2, 2.0
            for t_ in [k * 0.02 for k in range(1, 800)]:
                q = d * t_
                dx_ = max(abs(q.x - cxm) - (hw - rad), 0.0)
                dy_ = max(abs(q.y) - (E2 - rad), 0.0)
                if math.hypot(dx_, dy_) <= rad:
                    rr.append(t_)
            pts.append(rot @ (d * max(rr)))
        top = [bm.verts.new((p.x, p.y, 0.0)) for p in pts]
        bot = [bm.verts.new((p.x, p.y, -0.5)) for p in pts]
        bm.faces.new(top).material_index = S["TR_Terrazzo"]
        bm.faces.new(bot[::-1]).material_index = S["TR_Tx_Concrete"]
        for i in range(64):
            j = (i + 1) % 64
            bm.faces.new((top[j], top[i], bot[i], bot[j])).material_index = S["TR_Tx_Enamel"]
        # an amber stripe under the edge, the saucer's belt
        for i in range(64):
            j = (i + 1) % 64
            a_, b_ = pts[i], pts[j]
            beam(bm, a_ * 1.005 + Vector((0, 0, -0.32)), b_ * 1.005 + Vector((0, 0, -0.32)), 0.06, 0.16, S["TR_Tx_Amber"])
        # the railing, open at the stops beside the tower
        rail = []
        for p in pts:
            q = rot.transposed() @ p
            k = (q.length - RAIL_IN) / q.length
            q = q * k
            q.y = max(-(E2 - EDGE_RAIL), min(E2 - EDGE_RAIL, q.y))
            rail.append((math.degrees(math.atan2(q.y, q.x)), q.length + RAIL_IN))
        rb = bmesh.new()
        railing(rb, rail, lambda p: abs(p.y) > E2 - EDGE_RAIL - 0.05 and abs(p.x) < STOP_HALF, S["TR_Chrome"])
        rme = bpy.data.meshes.new("r")
        rb.to_mesh(rme)
        rb.free()
        vs = [bm.verts.new(rot @ v.co) for v in rme.vertices]
        for f in rme.polygons:
            try:
                bm.faces.new([vs[i] for i in f.vertices]).material_index = S["TR_Chrome"]
            except ValueError:
                pass
        bpy.data.meshes.remove(rme)
        # the tactile lines
        for sy in (1, -1):
            ps = [rot @ Vector((xa, sy * yb, 0.004)) for xa, yb in ((-STOP_HALF, E2 - 0.6), (STOP_HALF, E2 - 0.6), (STOP_HALF, E2 - 0.1), (-STOP_HALF, E2 - 0.1))]
            f = bm.faces.new([bm.verts.new(p) for p in ps])
            f.material_index = S["TR_Tx_Tactile"]
            f.normal_update()
            if f.normal.z < 0:
                f.normal_flip()
        # the opening out of the core onto the deck: toward +x' (the deck's long end)
        n = rot @ Vector((1, 0, 0))
        a = math.atan2(n.y, n.x)
        rmid = (T.r_out(st["outer"], a) + T.r_in(st["inner"], a)) / 2
        return [("door", n * rmid + Vector((0, 0, 1.2)), n, 2.4, 2.4)]
    part("SM_TR_Tower_Saucer_Deck2", H2, build)
    side = lambda v: ("+x" if v.x > 0 else "-x") if abs(v.x) >= abs(v.y) else ("-y" if v.y > 0 else "+y")
    n = rot @ Vector((1, 0, 0))
    return {"mesh": "SM_TR_Tower_Saucer_Deck2", "h": H2 * 100, "floor_z": round(dz * 100, 1), "line": lo_["line"],
            "axis_yaw": round(rel_ue, 2), "track_gap": round(2 * (E2 + 1.05) * 100, 1),
            "edge_length": round(2 * STOP_HALF * 100, 1), "edge_x": 0.0, "stop_x": 0.0,
            "edge_y": round(E2 * 100, 1),
            "placement": "the mesh is already turned by axis_yaw: place it at the tower's axis, its bottom at "
                         "floor_z, with the station's own yaw (not + axis_yaw)",
            "deck_x": [X0 * 100, X1 * 100],
            "core_exit_dir": [round(n.x, 4), round(-n.y, 4)],
            "panels": [{"at": ue(rot @ Vector((-(STOP_HALF + 0.8), sy * (E2 - 1.2), 0)))[:2],
                        "yaw": round(rel_ue + (90.0 if sy < 0 else -90.0), 1)} for sy in (1, -1)],
            "note": "in the tower frame: its axis, the deck's floor at z=0 of this piece; x' along the lower line "
                    "at axis_yaw (UE degrees) from the station's +X"}


def build_towers(style, cdir, mats):
    """The tower kit of this style (blender/transport/towers.py): foot, shafts, neck (and the saucer's lower deck)."""
    import towers as T
    st = T.STYLES[style]
    S = SLOT
    surf = lambda k: S["TR_Glass"] if k == "glass" else S["TR_Tx_" + k[3:].capitalize()]
    mi_w, mi_b, mi_in = surf(st["wall"]), surf(st["band"]), S["TR_Interior"]
    if st["wall"] == "glass":
        mi_in = S["TR_Glass"]
    clear = 2 * T.CLEAR_SQ * 100 if st["inner"] == "square" else round(2 * T.CLEAR_R / math.sqrt(2) * 100, 1)
    out = {"clear": [clear, clear], "shafts": []}
    # the street door faces away from the room (the side the core's exit doesn't), between the style's fins and legs
    fa = T.fin_angles(st)
    cands = sorted([Vector((1, 0, 0)), Vector((-1, 0, 0)), Vector((0, 1, 0)), Vector((0, -1, 0))],
                   key=lambda v: -v.dot(-cdir))
    ft = st["feature"][1]["t"] if isinstance(st.get("feature"), tuple) else 0.5
    need = lambda v: math.atan((DOOR_W / 2 + ft / 2 + 0.05) / T.r_out(st["outer"], math.atan2(v.y, v.x)))
    ddir = next(v for v in cands if all(abs(((math.atan2(v.y, v.x) - a + math.pi) % (2 * math.pi)) - math.pi) > need(v)
                                        for a in fa))
    dyaw = math.atan2(ddir.y, ddir.x)

    def part(name, h, build):
        bm = bmesh.new()
        ops = build(bm) or []
        o = new_object(name, bm, mats)
        if ops:
            cb = bmesh.new()
            cutters(cb, ops, depth=1.4)
            cut = new_object("cut", cb, mats)
            for p_ in cut.data.polygons:
                p_.material_index = mi_w if st["wall"] != "glass" else S["TR_Tx_Enamel"]
            boolean_cut(o, cut)
            bpy.data.objects.remove(cut)
            bm = bmesh.new()
            panes(bm, [q for q in ops if q[0] != "door"], S["TR_Glass"])
            p2 = new_object(name + "_panes", bm, mats)
            activate(o)
            p2.select_set(True)
            bpy.ops.object.join()
            o = bpy.context.view_layer.objects.active
        metre_uv(o, {S[k] for k in S if k.startswith("TR_Tx_")})
        o.name = o.data.name = name
        export(o, name)
        rec = {"mesh": name, "h": round(h * 100, 1), "tris": tris(o)}
        kind = name.split("_")[-1].lower()
        if kind.startswith("shaft"):
            out["shafts"].append(rec)
        else:
            out[kind] = rec
        bpy.data.objects.remove(o)

    cap = style.capitalize()

    mi_solid = mi_w if st["wall"] != "glass" else S["TR_Tx_Enamel"]

    def base(bm):
        T.podium(bm, st, 3, S["TR_Terrazzo"], S["TR_Tx_Concrete"], mi_b)
    part("SM_TR_Tower_%s_Base" % style.capitalize(), 0.45, base)
    out["base"]["skirt"] = 200.0

    def foot(bm):
        pl = st.get("plinth", 0.0)
        if pl:
            T.ring(bm, st["outer"], st["inner"], 0.0, pl, mi_solid, S["TR_Interior"], mi_b)
            T.ring(bm, st["outer"], st["inner"], pl, T.FOOT_H, mi_w, mi_in, mi_b)
            T.band(bm, st["outer"], pl - 0.12, pl, 0.12, mi_b)
        else:
            T.ring(bm, st["outer"], st["inner"], 0.0, T.FOOT_H, mi_w, mi_in, mi_b)
        T.band(bm, st["outer"], 0.0, 0.45, 0.12, mi_b)
        T.details(bm, st, 0.45, T.FOOT_H, mi_w, mi_b, beam, box)
        T.feature(bm, st, "foot", 0.0, T.FOOT_H, mi_solid, mi_b, beam)
        a = dyaw
        n = Vector((math.cos(a), math.sin(a), 0))
        rmid = (T.r_out(st["outer"], a) + T.r_in(st["inner"], a)) / 2
        return [("door", n * rmid + Vector((0, 0, DOOR_H / 2 - 0.05)), n, DOOR_W, DOOR_H + 0.1)]
    part("SM_TR_Tower_%s_Foot" % cap, T.FOOT_H, foot)
    out["door"] = {"side": ("+x" if ddir.x > 0 else "-x") if abs(ddir.x) >= abs(ddir.y) else ("-y" if ddir.y > 0 else "+y"),
                   "hole": [round(DOOR_W * 100, 1), round(DOOR_H * 100, 1)],
                   # the wall the door is in: its inner and outer face's distance from the axis, and the door's middle
                   "wall": [round(T.r_in(st["inner"], dyaw) * 100, 1), round(T.r_out(st["outer"], dyaw) * 100, 1)],
                   "at": round((T.r_in(st["inner"], dyaw) + T.r_out(st["outer"], dyaw)) / 2 * 100, 1)}
    for h in T.SHAFT:
        def shaft(bm, h=h):
            T.ring(bm, st["outer"], st["inner"], 0.0, h, mi_w, mi_in, mi_w)
            T.details(bm, st, 0.0, h, mi_w, mi_b, beam, box)
            T.feature(bm, st, "shaft", 0.0, h, mi_solid, mi_b, beam)
            return T.window_ops(st, 0.0, h)
        part("SM_TR_Tower_%s_Shaft%d" % (cap, int(h)), h, shaft)

    def neck(bm):
        T.ring(bm, st["outer"], st["inner"], 0.0, T.NECK_H, mi_solid, S["TR_Interior"], mi_b, 1.0, 1.3)
        T.feature(bm, st, "neck", 0.0, T.NECK_H, mi_solid, mi_b, beam)
    part("SM_TR_Tower_%s_Neck" % cap, T.NECK_H, neck)
    if style == "saucer":
        out["decks"] = [lower_deck(st, T, part, mats, mi_w, mi_b, mi_in)]
    out["shafts"].sort(key=lambda r: -r["h"])
    out["neck_top"] = round(BASE * 100, 1)
    return out


def ue(p):
    return [round(p.x * 100, 1), round(-p.y * 100, 1), round(p.z * 100, 1)]


def hull(pts):
    pts = sorted(set((round(x, 2), round(y, 2)) for x, y in pts))
    if len(pts) < 3:
        return pts

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower, upper = [], []
    for p in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    for p in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]


def outline_pts(pts, n=32, pad=0.05, clamp=True):
    """The footprint: the convex hull's extreme points in n directions, pushed out by pad."""
    h = hull(pts)
    cx = sum(p[0] for p in h) / len(h)
    cy = sum(p[1] for p in h) / len(h)
    out = []
    for i in range(n):
        a = 2 * math.pi * i / n
        d = (math.cos(a), math.sin(a))
        p = max(h, key=lambda q: q[0] * d[0] + q[1] * d[1])
        r = math.hypot(p[0] - cx, p[1] - cy)
        q = (cx + (p[0] - cx) * (r + pad) / r, cy + (p[1] - cy) * (r + pad) / r) if r > 1e-6 else p
        if clamp:
            q = (min(0.0, q[0]), q[1])
        if not out or out[-1] != q:
            out.append(q)
    return out


def export(o, name):
    os.makedirs(OUT, exist_ok=True)
    activate(o)
    bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, name + ".glb"), use_selection=True, export_format="GLB",
                              export_image_format="JPEG", export_jpeg_quality=90, export_yup=True, export_apply=True)


def debug_render(o, name, room, door):
    """TR_DEBUG=<dir>: textured pictures from outside, at the door and inside."""
    out = os.environ.get("TR_DEBUG")
    if not out:
        return
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.display.shading.light = "STUDIO"
    sc.display.shading.color_type = "TEXTURE"
    sc.display.shading.show_cavity = True
    sc.render.resolution_x = sc.render.resolution_y = 600
    cam = bpy.data.objects.new("dbgcam", bpy.data.cameras.new("dbgcam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.data.clip_start = 0.05
    for m in o.data.materials:
        if m.name.startswith("TR_"):
            m.diffuse_color = PALETTE.get(m.name.split(".")[0], (0.8, 0.8, 0.8, 1))[:3] + (1,)
    outv = (Vector((door.x, door.y, 0)) - Vector((room.x, room.y, 0))).normalized()

    def shot(frm, to, lens, k):
        cam.location = frm
        cam.rotation_euler = (to - frm).to_track_quat("-Z", "Y").to_euler()
        cam.data.lens = lens
        sc.render.filepath = os.path.join(out, "%s_%s.png" % (name, k))
        bpy.ops.render.render(write_still=True)
    shot(Vector((12, -14, 7)), Vector((-10, 0, 3)), 24, "out")
    shot(Vector((-24, 14, 9)), Vector((-10, 0, 3)), 24, "back")
    shot(door + outv * 5 + Vector((0, 0, 1.7)), door + Vector((0, 0, 1.4)), 20, "door")
    d = (door - room).length
    shot(room - outv * d * 0.85 + Vector((0, 0, 1.6)), door + Vector((0, 0, 1.4)), 14, "in")
    for m in o.data.materials:
        pass


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    data = json.load(open(JSON)) if os.path.isfile(JSON) else {}
    for style in argv or sorted(STYLES):
        parts = build(style, STYLES[style])
        data[style] = finish(style, STYLES[style], parts)
        json.dump(data, open(JSON, "w"), indent=1, sort_keys=True)
    data["_doc"] = ("Made by blender/transport/vestibules.py. Unreal cm in the connector frame: the origin is the middle "
                    "of the connector's gap on the terrace floor, +X out of the vestibule to the procedural part. "
                    "Blender (x, y, z) m -> UE (x*100, -y*100, z*100). docs/transport.md, 'Vestibule connector'.")
    os.makedirs(os.path.dirname(JSON), exist_ok=True)
    json.dump(data, open(JSON, "w"), indent=1, sort_keys=True)


main()
