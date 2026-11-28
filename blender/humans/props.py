"""The things the townsfolk's manners put in their hands (docs/town_manners.md): a phone, a cigarette, a camera, an
umbrella, a fishing rod, a brush, binoculars, a newspaper, a book, knitting, an ice cream, a paper cup, a shopping
bag, the painter's easel, the allotment's spade and watering can, the cleaner's mop and bucket, the night watchman's torch,
the yard keeper's broom and rake; the days' things (docs/town_life.md "The days"): the folding chess board set out on a
bench between two players, the birthday cake in its tied box carried home and the cake itself for a table, the guest's
carnations.

    Blender -b -P blender/humans/props.py [-- --preview] [--only chess,cake_box]
        -> Saved/ClipSources/props/<name>.glb (unreal/scripts/import_props.py -> /Game/Characters/Humans/Props/SM_Prop_<name>)
        --preview: also Saved/ClipSources/props/preview_<name>.png (two views each), to look them over before the import

Built from primitives, flat colours (no textures, no words on them: nothing to localize), 100-900 triangles each (the
chess board with its 28 pieces about 1100). UV0: metres, projected onto each face's own plane (the house's way, as
blender/city/interior_kit.py: never stretched, so a tiling texture can go on any of them later). Every prop is linted
as it is built (degenerate or sliver triangles, stretched or folded UVs, a face whose winding fights its normal,
faces in one plane over one another facing the same way, and a flat piece laid on a face: a decal is cut into the face
or lifted 12 mm, never laid on it); a prop that fails is not written, and the run ends with exit code 1. Faces wholly
covered by faces turned against them (a stack of bars' end caps) are never seen and are dropped first.
Metres. How each lies in its own frame is what ANpcBody::PlaceMannerProp expects (LyudGesture::HoldOf):

    umbrella, ice_cream, cup   origin at the grip, +Z up
    bag                        origin at the handles' top, hangs along -Z, its flat side along X
    phone                      origin in the middle, long axis X, the screen +Z
    cigarette, brush           along +X from the fingers (origin where they hold it)
    rod                        along +X from the grip (origin in the grip)
    camera, binoculars         between the hands along X, the side the holder looks into +Z (the lens -Z), the top +Y
    newspaper, book, knitting  between the hands along X, the pages the holder reads +Z, the top edge +Y
    easel                      origin on the ground under it, the canvas faces +X (the painter stands at +X)
    spade                      the handle's grip at the origin, the shaft along +X to the blade (the other hand at ~0.5 m)
    watering_can               origin at the top handle's grip, +Z up, the spout along +X
    mop                        the shaft's top at the origin, along +X to the head (1.32 m), the strands' face +Z
    torch                      the grip at the origin, the lens along +X (0.2 m)
    bucket                     origin on the ground under it, +Z up (stands by the cleaner like the easel)
    suitcase                   origin at the handle's grip, the case hangs along -Z, its broad sides +Y and -Y
    broom, rake                as the mop: the shaft's top at the origin, along +X to the head, the head's face +Z
    chess                      origin on the bench's seat under the board's middle, +Z up, the board's files along Y,
                               White's side -X (PeopleLife puts +X toward the second player: they sit side by side)
    cake_box                   origin at the string's loop (the grip), the box hangs along -Z (EHold::Hanging, as the bag)
    cake                       origin on the table under its middle, +Z up (for a table spot; the town's flats are shells)
    flowers                    origin at the grip round the stems, the blooms hang along -Z (carried heads down, Hanging)
"""
import math
import os
import sys

import bpy
import bmesh
from mathutils import Matrix, Vector

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OUT = os.path.join(ROOT, "Saved", "ClipSources", "props")
PREVIEW = "--preview" in sys.argv
ONLY = sys.argv[sys.argv.index("--only") + 1].split(",") if "--only" in sys.argv else None


class Prop:
    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.mats = []          # (name, rgb, rough, emit)

    def mat(self, part, rgb, rough=0.6, emit=0.0):
        key = "prop.%s.%s" % (self.name, part)
        for i, m in enumerate(self.mats):
            if m[0] == key:
                return i
        self.mats.append((key, rgb, rough, emit))
        return len(self.mats) - 1

    def _take(self, geom, mi, m=None):
        verts = [g for g in geom if isinstance(g, bmesh.types.BMVert)]
        if m is not None:
            bmesh.ops.transform(self.bm, matrix=m, verts=verts)
        faces = set()
        for v in verts:
            faces.update(v.link_faces)
        for f in faces:
            f.material_index = mi
            f.smooth = False
        return verts

    def box(self, mi, lo, hi, rot=None):
        """A box from corner lo to corner hi; rot: a Matrix turning it about its centre."""
        c = (Vector(lo) + Vector(hi)) * 0.5
        s = Vector(hi) - Vector(lo)
        m = Matrix.Translation(c) @ (rot.to_4x4() if rot else Matrix.Identity(4)) @ Matrix.Diagonal((s.x, s.y, s.z, 1.0))
        g = bmesh.ops.create_cube(self.bm, size=1.0)
        return self._take(g["verts"], mi, m)

    def rod(self, mi, a, b, r0, r1=None, seg=10, smooth=True, caps=True):
        """A round bar from a to b, radius r0 at a and r1 at b."""
        a, b = Vector(a), Vector(b)
        r1 = r0 if r1 is None else r1
        d = b - a
        g = bmesh.ops.create_cone(self.bm, cap_ends=caps, cap_tris=False, segments=seg, radius1=r0, radius2=r1, depth=d.length)
        m = Matrix.Translation((a + b) * 0.5) @ d.to_track_quat("Z", "Y").to_matrix().to_4x4()
        verts = self._take(g["verts"], mi, m)
        if smooth:
            for v in verts:
                for f in v.link_faces:
                    if len(f.verts) == 4:
                        f.smooth = True
        return verts

    def ball(self, mi, c, r, scale=(1.0, 1.0, 1.0), seg=12):
        g = bmesh.ops.create_uvsphere(self.bm, u_segments=seg, v_segments=max(6, seg // 2), radius=r)
        verts = self._take(g["verts"], mi, Matrix.Translation(Vector(c)) @ Matrix.Diagonal((scale[0], scale[1], scale[2], 1.0)))
        for v in verts:
            for f in v.link_faces:
                f.smooth = True
        return verts

    def sheet(self, mi, pts, back=None):
        """A flat sheet through four corners (counter-clockwise seen from its front), with its back (another colour if given)."""
        vs = [self.bm.verts.new(Vector(p)) for p in pts]
        f = self.bm.faces.new(vs)
        f.material_index = mi
        vb = [self.bm.verts.new(Vector(p)) for p in reversed(pts)]
        fb = self.bm.faces.new(vb)
        fb.material_index = mi if back is None else back

    def lathe(self, mi, prof, c=(0.0, 0.0, 0.0), seg=6, rot=None, phase=0.0):
        """A solid of revolution about +Z (then turned by rot about c and moved to c): prof is (r, z) from the bottom up
        along the outside; r = 0 at either end closes it at a single pole, otherwise that end stays open."""
        m = Matrix.Translation(Vector(c)) @ (rot.to_4x4() if rot else Matrix.Identity(4))
        rings = []
        for r, z in prof:
            if r <= 0.0:
                rings.append([self.bm.verts.new(m @ Vector((0.0, 0.0, z)))] * seg)
            else:
                rings.append([self.bm.verts.new(m @ Vector((r * math.cos(phase + 2.0 * math.pi * j / seg),
                                                             r * math.sin(phase + 2.0 * math.pi * j / seg), z))) for j in range(seg)])
        for lo, hi in zip(rings, rings[1:]):
            for j in range(seg):
                k = (j + 1) % seg
                vs = []
                for v in (lo[j], lo[k], hi[k], hi[j]):
                    if v not in vs:
                        vs.append(v)
                if len(vs) >= 3:
                    f = self.bm.faces.new(vs)
                    f.material_index = mi
                    f.smooth = True
        return [v for ring in rings for v in set(ring)]

    def coplanar_pairs(self, gap=0.0003):
        """Pairs of (convex) faces in one plane (within gap, facing either way) that lie over one another by more than 2 mm2,
        with the area each covers of the other: [(fa, fb, overlap area, same facing)]."""
        def clip(subject, a, b):
            # Sutherland-Hodgman: the part of polygon subject left of the directed edge a->b (2D)
            out = []
            side = lambda pt: (b[0] - a[0]) * (pt[1] - a[1]) - (b[1] - a[1]) * (pt[0] - a[0])
            for k, cur in enumerate(subject):
                prev = subject[k - 1]
                sc, sp = side(cur), side(prev)
                if sc >= 0.0:
                    if sp < 0.0:
                        t = sp / (sp - sc)
                        out.append((prev[0] + (cur[0] - prev[0]) * t, prev[1] + (cur[1] - prev[1]) * t))
                    out.append(cur)
                elif sp >= 0.0:
                    t = sp / (sp - sc)
                    out.append((prev[0] + (cur[0] - prev[0]) * t, prev[1] + (cur[1] - prev[1]) * t))
            return out

        def area(poly):
            return 0.5 * sum(poly[k - 1][0] * poly[k][1] - poly[k][0] * poly[k - 1][1] for k in range(len(poly)))

        buckets = {}
        tris = []
        for f in self.bm.faces:
            n = f.normal.copy()
            if n.length < 0.5:
                continue
            canon = n if (n.x, n.y, n.z) > (0.0, 0.0, 0.0) else -n
            d = canon.dot(f.verts[0].co)
            key = (round(canon.x, 2), round(canon.y, 2), round(canon.z, 2), int(round(d / gap)))
            tris.append((f, canon, d))
            buckets.setdefault(key, []).append(len(tris) - 1)
        pairs = []
        for (kx, ky, kz, kd), members in buckets.items():
            near = []
            for dd in (-1, 0, 1):
                near += buckets.get((kx, ky, kz, kd + dd), [])
            for a_i in members:
                fa, na, da = tris[a_i]
                ref = Vector((0.0, 0.0, 1.0)) if abs(na.z) < 0.9 else Vector((0.0, 1.0, 0.0))
                u = ref.cross(na).normalized()
                v = na.cross(u)
                pa = [(w.co.dot(u), w.co.dot(v)) for w in fa.verts]
                if area(pa) < 0.0:
                    pa.reverse()
                for b_i in near:
                    if b_i <= a_i:
                        continue
                    fb, nb, db = tris[b_i]
                    if abs(na.dot(nb)) < 0.9995 or abs(da - db) > gap:
                        continue
                    pb = [(w.co.dot(u), w.co.dot(v)) for w in fb.verts]
                    if area(pb) < 0.0:
                        pb.reverse()
                    poly = pa
                    for k in range(len(pb)):
                        poly = clip(poly, pb[k - 1], pb[k])
                        if len(poly) < 3:
                            break
                    if len(poly) >= 3 and area(poly) > 2e-6:
                        pairs.append((fa, fb, area(poly), fa.normal.dot(fb.normal) > 0.0))
        return pairs

    def drop_hidden(self):
        """Back-to-back faces in one plane (a stack of bars' end caps meeting): a triangle wholly covered by faces turned
        against it is never seen and is dropped. A piece laid flat on a face (a decal) has its underside dropped the same
        way, so that is told apart: the faces under it are only partly covered. Returns (dropped, decals)."""
        cover = {}
        def corners(f):
            return sorted(tuple(round(c, 6) for c in v.co) for v in f.verts)

        for fa, fb, a, same in self.coplanar_pairs():
            # (a sheet's two sides, Prop.sheet: the same corners turned both ways, both seen)
            if not same and corners(fa) != corners(fb):
                cover[fa] = cover.get(fa, 0.0) + a
                cover[fb] = cover.get(fb, 0.0) + a
        hidden = [f for f, a in cover.items() if a >= f.calc_area() * 0.995]
        # a decal: the piece a dropped underside belongs to is a flat sheet on the face (under 12 mm thick and thin
        # against its breadth), not a bar standing on it
        decals = 0
        seen = set()
        for f in hidden:
            if f in seen:
                continue
            piece, todo = set(), [f]
            while todo:
                g = todo.pop()
                if g in piece:
                    continue
                piece.add(g)
                todo.extend(h for e in g.edges for h in e.link_faces if h not in piece)
            under = [g for g in hidden if g in piece]
            seen.update(under)
            n = f.normal
            ds = [v.co.dot(n) for g in piece for v in g.verts]
            thick = max(ds) - min(ds)
            foot = sum(g.calc_area() for g in under)
            if thick < 0.012 and thick < 0.25 * math.sqrt(foot):
                decals += len(under)
        bmesh.ops.delete(self.bm, geom=hidden, context="FACES_ONLY")
        bmesh.ops.delete(self.bm, geom=[v for v in self.bm.verts if not v.link_faces], context="VERTS")
        self.bm.normal_update()
        return len(hidden), decals

    def uv_and_lint(self):
        """UV0 in metres on each triangle's own plane (u along the level, v up the face; on a level face u is +X, v +Y),
        then the lint: returns the problems found (an empty list: clean)."""
        bm = self.bm
        uv = bm.loops.layers.uv.verify()
        bad = []
        degenerate = sliver = stretched = folded = 0
        for f in bm.faces:
            n = f.normal
            if n.length < 0.5:
                degenerate += 1
                continue
            ref = Vector((0.0, 0.0, 1.0)) if abs(n.z) < 0.9 else Vector((0.0, 1.0, 0.0))
            u = ref.cross(n).normalized()
            v = n.cross(u)
            for lp in f.loops:
                lp[uv].uv = (lp.vert.co.dot(u), lp.vert.co.dot(v))
            a, b, c = (lp.vert.co for lp in f.loops)
            area = (b - a).cross(c - a).length * 0.5
            longest = max((b - a).length, (c - b).length, (a - c).length)
            if area < 1e-9:
                degenerate += 1
                continue
            height = 2.0 * area / longest
            if height < longest * 0.004 and height < 0.0002:      # a needle: under 0.4% of its length and 0.2 mm across
                sliver += 1
            (ua, ub, uc) = (lp[uv].uv for lp in f.loops)
            uarea = ((ub - ua).x * (uc - ua).y - (uc - ua).x * (ub - ua).y) * 0.5
            if uarea <= 0.0:
                folded += 1
            elif not (0.98 < uarea / area < 1.02):
                stretched += 1
            # the winding must agree with the normal the mesh would get from it
            if (b - a).cross(c - a).normalized().dot(n) < 0.99:
                folded += 1
        coplanar = sum(1 for fa, fb, a, same in self.coplanar_pairs() if same)
        for what, k in (("degenerate", degenerate), ("sliver", sliver), ("stretched UV", stretched), ("folded UV", folded),
                        ("coplanar z-fighting", coplanar),
                        ("laid on a face (a decal: cut it in or lift it 12 mm)", self.decals)):
            if k:
                bad.append("%d %s" % (k, what))
        return bad

    def build(self):
        me = bpy.data.meshes.new("SM_Prop_" + self.name)
        self.bm.normal_update()
        self.dropped, self.decals = self.drop_hidden()        # on the faces as made (a sheet's two sides match there)
        bmesh.ops.triangulate(self.bm, faces=self.bm.faces[:])
        bmesh.ops.delete(self.bm, geom=[v for v in self.bm.verts if not v.link_faces], context="VERTS")
        self.bm.normal_update()
        self.lint = self.uv_and_lint()
        self.bm.to_mesh(me)
        self.bm.free()
        for key, rgb, rough, emit in self.mats:
            m = bpy.data.materials.new(key)
            m.use_nodes = True
            p = m.node_tree.nodes["Principled BSDF"]
            p.inputs["Base Color"].default_value = tuple(rgb) + (1.0,)
            p.inputs["Roughness"].default_value = rough
            if emit > 0.0:
                p.inputs["Emission Color"].default_value = tuple(rgb) + (1.0,)
                p.inputs["Emission Strength"].default_value = emit
            m.diffuse_color = tuple(rgb) + (1.0,)
            me.materials.append(m)
        o = bpy.data.objects.new("SM_Prop_" + self.name, me)
        bpy.context.scene.collection.objects.link(o)
        return o


def rot_x(deg):
    return Matrix.Rotation(math.radians(deg), 3, "X")


def rot_y(deg):
    return Matrix.Rotation(math.radians(deg), 3, "Y")


def rot_z(deg):
    return Matrix.Rotation(math.radians(deg), 3, "Z")


def phone():
    p = Prop("phone")
    body = p.mat("body", (0.03, 0.03, 0.035), 0.4)
    glass = p.mat("screen", (0.01, 0.012, 0.02), 0.12)
    lens = p.mat("lens", (0.1, 0.1, 0.11), 0.3)
    p.box(body, (-0.073, -0.035, -0.004), (0.073, 0.035, 0.0035))
    p.box(glass, (-0.070, -0.032, 0.003), (0.070, 0.032, 0.0042))          # (set into the body, not laid on it)
    p.rod(lens, (0.055, 0.02, -0.003), (0.055, 0.02, -0.0055), 0.006, seg=10)
    return p


def cigarette():
    p = Prop("cigarette")
    paper = p.mat("paper", (0.9, 0.89, 0.86), 0.8)
    filt = p.mat("filter", (0.72, 0.45, 0.2), 0.8)
    ember = p.mat("ember", (1.0, 0.25, 0.05), 0.9, emit=3.0)
    ash = p.mat("ash", (0.35, 0.34, 0.33), 0.95)
    p.rod(filt, (-0.02, 0, 0), (0.004, 0, 0), 0.004, seg=8)
    p.rod(paper, (0.004, 0, 0), (0.058, 0, 0), 0.004, seg=8)
    p.rod(ash, (0.058, 0, 0), (0.062, 0, 0), 0.004, 0.0036, seg=8)
    p.rod(ember, (0.062, 0, 0), (0.064, 0, 0), 0.0036, 0.003, seg=8)
    return p


def camera():
    p = Prop("camera")
    body = p.mat("body", (0.035, 0.035, 0.04), 0.5)
    top = p.mat("top", (0.55, 0.56, 0.58), 0.35)
    lens = p.mat("lens", (0.02, 0.02, 0.022), 0.3)
    glass = p.mat("glass", (0.05, 0.08, 0.14), 0.05)
    p.box(body, (-0.068, -0.04, -0.02), (0.068, 0.03, 0.02))
    p.box(top, (-0.068, 0.03, -0.02), (0.068, 0.043, 0.02))
    p.box(top, (-0.02, 0.043, -0.014), (0.02, 0.057, 0.014))            # the viewfinder's hump
    p.rod(top, (0.045, 0.043, 0.0), (0.045, 0.05, 0.0), 0.008, seg=10)  # the shutter button
    p.rod(lens, (0.0, -0.004, -0.02), (0.0, -0.004, -0.062), 0.031, 0.029, seg=16)
    p.rod(glass, (0.0, -0.004, -0.0605), (0.0, -0.004, -0.0635), 0.024, seg=16)     # (set into the lens)
    p.box(glass, (-0.012, 0.045, 0.0125), (0.012, 0.054, 0.0145))        # the eyepiece, set into the hump
    return p


def umbrella():
    p = Prop("umbrella")
    cloth = p.mat("cloth", (0.05, 0.09, 0.2), 0.7)
    inner = p.mat("inner", (0.03, 0.05, 0.11), 0.8)
    steel = p.mat("steel", (0.55, 0.56, 0.58), 0.35)
    wood = p.mat("handle", (0.2, 0.11, 0.05), 0.5)
    # the crook handle, the shaft, the tip
    p.rod(wood, (0, 0, -0.07), (0, 0, 0.06), 0.011, seg=10)
    p.rod(wood, (0, 0, -0.07), (0.0, -0.03, -0.1), 0.0104, seg=10)     # (a hair thinner each: no side lies on the last's)
    p.rod(wood, (0.0, -0.03, -0.1), (0.0, -0.06, -0.075), 0.0098, 0.0092, seg=10)
    p.rod(steel, (0, 0, 0.06), (0, 0, 0.86), 0.005, seg=8)
    p.rod(steel, (0, 0, 0.86), (0, 0, 0.92), 0.004, 0.002, seg=8)
    # the canopy: eight panels sagging a little between the ribs, the ribs and the stretchers under it
    n, r, top, rim = 8, 0.54, 0.86, 0.66
    apex = Vector((0, 0, top))
    tips = [Vector((r * math.cos(2 * math.pi * k / n), r * math.sin(2 * math.pi * k / n), rim)) for k in range(n)]
    for k in range(n):
        a, b = tips[k], tips[(k + 1) % n]
        # two rows per panel: a shoulder half way gives the dome its curve
        sa = apex.lerp(a, 0.5) + Vector((0, 0, 0.055))
        sb = apex.lerp(b, 0.5) + Vector((0, 0, 0.055))
        mid = (a + b) * 0.5 + Vector((0, 0, 0.018)) - (a + b).normalized() * 0.02
        smid = (sa + sb) * 0.5
        for tri in ((apex, sa, sb),):
            vs = [p.bm.verts.new(v) for v in tri]
            p.bm.faces.new(vs).material_index = cloth
            vs = [p.bm.verts.new(v) for v in reversed(tri)]
            p.bm.faces.new(vs).material_index = inner
        for quad in ((sa, a, mid, smid), (smid, mid, b, sb)):
            vs = [p.bm.verts.new(v) for v in quad]
            p.bm.faces.new(vs).material_index = cloth
            vs = [p.bm.verts.new(v) for v in reversed(quad)]
            p.bm.faces.new(vs).material_index = inner
        p.rod(steel, apex - Vector((0, 0, 0.004)), sa - Vector((0, 0, 0.004)), 0.002, seg=4, smooth=False, caps=False)
        p.rod(steel, sa - Vector((0, 0, 0.004)), a - Vector((0, 0, 0.004)), 0.002, seg=4, smooth=False, caps=False)
        p.rod(steel, (0, 0, 0.52), sa - Vector((0, 0, 0.006)), 0.0018, seg=4, smooth=False, caps=False)
    p.rod(steel, (0, 0, 0.5), (0, 0, 0.54), 0.009, seg=8)       # the runner
    return p


def rod():
    p = Prop("rod")
    cork = p.mat("cork", (0.62, 0.45, 0.27), 0.85)
    blank = p.mat("blank", (0.05, 0.07, 0.06), 0.35)
    steel = p.mat("steel", (0.6, 0.6, 0.62), 0.3)
    p.rod(cork, (-0.22, 0, 0), (0.16, 0, 0), 0.013, seg=10)
    p.rod(steel, (-0.23, 0, 0), (-0.22, 0, 0), 0.014, seg=10)
    p.rod(blank, (0.16, 0, 0), (1.3, 0, 0), 0.007, 0.004, seg=8)
    p.rod(blank, (1.3, 0, 0), (2.7, 0, 0), 0.004, 0.0012, seg=6)
    # the reel under the grip, the rings along the blank
    p.rod(steel, (0.2, 0, -0.012), (0.2, 0, -0.05), 0.004, seg=6)
    p.rod(steel, (0.2, -0.016, -0.065), (0.2, 0.016, -0.065), 0.026, seg=14)
    p.rod(cork, (0.2, 0.016, -0.065), (0.2, 0.03, -0.085), 0.003, seg=6)
    for x, r in ((0.6, 0.012), (1.1, 0.009), (1.6, 0.007), (2.1, 0.005), (2.69, 0.004)):
        p.rod(steel, (x, 0, -0.004), (x, 0, -0.004 - r * 2), 0.0012, seg=4, smooth=False)
        p.rod(steel, (x - 0.001, 0, -0.004 - r * 2 - r), (x + 0.001, 0, -0.004 - r * 2 - r), r, seg=8)
    return p


def brush():
    p = Prop("brush")
    wood = p.mat("handle", (0.5, 0.08, 0.06), 0.4)
    steel = p.mat("ferrule", (0.6, 0.6, 0.62), 0.3)
    hair = p.mat("hair", (0.12, 0.3, 0.6), 0.7)
    p.rod(wood, (-0.09, 0, 0), (-0.02, 0, 0), 0.002, 0.0042, seg=8)
    p.rod(wood, (-0.02, 0, 0), (0.1, 0, 0), 0.0042, 0.0038, seg=8)
    p.rod(steel, (0.1, 0, 0), (0.125, 0, 0), 0.004, seg=8)
    p.rod(hair, (0.125, 0, 0), (0.15, 0, 0), 0.0045, 0.001, seg=8)
    return p


def binoculars():
    p = Prop("binoculars")
    body = p.mat("body", (0.03, 0.035, 0.03), 0.55)
    ring = p.mat("ring", (0.1, 0.1, 0.1), 0.4)
    glass = p.mat("glass", (0.08, 0.05, 0.14), 0.05)
    for s in (-1, 1):
        x = s * 0.034
        p.rod(body, (x, 0, 0.045), (x, 0, -0.01), 0.017, 0.02, seg=14)
        p.rod(body, (x, 0, -0.01), (x, 0, -0.075), 0.024, 0.027, seg=14)
        p.rod(ring, (x, 0, -0.075), (x, 0, -0.082), 0.028, seg=14)
        p.rod(glass, (x, 0, -0.0805), (x, 0, -0.083), 0.024, seg=14)        # (the glasses set into the rings)
        p.rod(ring, (x, 0, 0.045), (x, 0, 0.058), 0.015, seg=12)
        p.rod(glass, (x, 0, 0.0565), (x, 0, 0.0585), 0.011, seg=12)
    p.box(body, (-0.02, -0.008, -0.04), (0.02, 0.012, 0.03))
    p.rod(ring, (0, 0.012, 0.0), (0, 0.012, 0.02), 0.009, seg=10)       # the focus wheel
    return p


def _columns(p, ink, x0, x1, y0, y1, z, rows, tilt, flip):
    """Grey bars for lines of print on a page lying from x0 to x1 (tilted about the fold at x = 0); their vertices."""
    made = []
    for k in range(rows):
        y = y1 - (k + 0.5) * (y1 - y0) / rows
        h = (y1 - y0) / rows * 0.28
        a, b = Vector((x0, y - h, z)), Vector((x1, y + h, z))
        pts = [Vector((a.x, a.y, z)), Vector((b.x, a.y, z)), Vector((b.x, b.y, z)), Vector((a.x, b.y, z))]
        pts = [tilt @ q for q in pts]
        if flip:
            pts.reverse()
        vs = [p.bm.verts.new(q) for q in pts]
        p.bm.faces.new(vs).material_index = ink
        made += vs
    return made


def newspaper():
    p = Prop("newspaper")
    paper = p.mat("paper", (0.78, 0.76, 0.7), 0.9)
    ink = p.mat("ink", (0.3, 0.3, 0.3), 0.9)
    head = p.mat("headline", (0.08, 0.08, 0.08), 0.9)
    w, h = 0.29, 0.42
    for s in (-1, 1):
        tilt = rot_y(-s * 14.0)           # the two halves lean toward the reader (+Z)
        c = [tilt @ Vector(q) for q in ((0, -h / 2, 0), (s * w, -h / 2, 0), (s * w, h / 2, 0), (0, h / 2, 0))]
        if s < 0:
            c.reverse()
        p.sheet(paper, c)
        for z, flip in ((0.0008, s < 0), (-0.0008, s > 0)):
            lo, hi = sorted((s * 0.02, s * (w - 0.02)))
            mid = (lo + hi) * 0.5
            # a headline across the top, a picture's block, two columns of print
            _columns(p, head, lo, hi, h / 2 - 0.07, h / 2 - 0.025, z, 1, tilt, flip)
            _columns(p, ink, lo, mid - 0.008, -h / 2 + 0.03, h / 2 - 0.09, z, 13, tilt, flip)
            _columns(p, ink, mid + 0.008, hi, -h / 2 + 0.03, h / 2 - 0.2, z, 9, tilt, flip)
            _columns(p, ink, mid + 0.008, hi, h / 2 - 0.19, h / 2 - 0.09, z, 1, tilt, flip)
    return p


def book():
    p = Prop("book")
    cover = p.mat("cover", (0.35, 0.07, 0.08), 0.6)
    pages = p.mat("pages", (0.86, 0.83, 0.74), 0.9)
    ink = p.mat("ink", (0.35, 0.34, 0.32), 0.9)
    w, h = 0.125, 0.19
    for s in (-1, 1):
        tilt = rot_y(-s * 12.0)           # the halves lean toward the reader (+Z) about the spine
        lo, hi = sorted((0.0, s * w))
        e = h / 2 - (0.0006 if s > 0 else 0.0)        # (the halves' edges not in one plane where they meet at the spine)
        half = p.box(cover, (lo, -e, -0.016), (hi, e, -0.012))
        lo, hi = sorted((s * 0.002, s * (w - 0.005)))
        half += p.box(pages, (lo, -h / 2 + 0.005, -0.012), (hi, h / 2 - 0.005, 0.0))
        lo, hi = sorted((s * 0.012, s * (w - 0.015)))
        half += _columns(p, ink, lo, hi, -h / 2 + 0.02, h / 2 - 0.02, 0.0006, 12, Matrix.Identity(3), False)
        bmesh.ops.transform(p.bm, matrix=tilt.to_4x4(), verts=half)
    p.box(cover, (-0.004, -h / 2 - 0.0008, -0.018), (0.004, h / 2 + 0.0008, -0.012))      # the spine
    return p


def knitting():
    p = Prop("knitting")
    wool = p.mat("wool", (0.6, 0.12, 0.14), 0.95)
    wool2 = p.mat("stripe", (0.85, 0.8, 0.7), 0.95)
    needle = p.mat("needle", (0.7, 0.7, 0.72), 0.3)
    # two needles crossing between the hands, the piece hanging from them, the yarn's ball below
    p.rod(needle, (-0.15, -0.01, 0.005), (0.09, 0.03, 0.0), 0.002, seg=6)
    p.rod(needle, (0.15, -0.01, 0.005), (-0.09, 0.03, -0.004), 0.002, seg=6)
    p.ball(needle, (-0.15, -0.01, 0.005), 0.005, seg=6)
    p.ball(needle, (0.15, -0.01, 0.005), 0.005, seg=6)
    for k in range(6):
        y0 = 0.02 - k * 0.028
        p.box(wool if k % 3 else wool2, (-0.075, y0 - 0.028, -0.007), (0.075, y0, 0.0))
    p.ball(wool, (0.06, -0.2, -0.03), 0.035, seg=10)
    p.rod(wool, (0.03, -0.148, -0.004), (0.055, -0.17, -0.02), 0.0015, seg=4, smooth=False)
    return p


def ice_cream():
    p = Prop("ice_cream")
    waffle = p.mat("waffle", (0.72, 0.5, 0.25), 0.8)
    cream = p.mat("cream", (0.95, 0.92, 0.84), 0.6)
    p.rod(waffle, (0, 0, -0.065), (0, 0, 0.045), 0.004, 0.027, seg=12)
    p.rod(waffle, (0, 0, 0.045), (0, 0, 0.05), 0.029, seg=12)
    p.ball(cream, (0, 0, 0.066), 0.031, seg=12)
    p.ball(cream, (0, 0, 0.094), 0.019, scale=(1, 1, 1.15), seg=10)
    return p


def cup():
    p = Prop("cup")
    paper = p.mat("paper", (0.8, 0.74, 0.62), 0.8)
    lid = p.mat("lid", (0.92, 0.92, 0.92), 0.5)
    band = p.mat("band", (0.5, 0.2, 0.1), 0.8)
    p.rod(paper, (0, 0, -0.055), (0, 0, -0.01), 0.027, 0.032, seg=14)
    p.rod(band, (0, 0, -0.01), (0, 0, 0.025), 0.0325, 0.036, seg=14)
    p.rod(paper, (0, 0, 0.025), (0, 0, 0.05), 0.036, 0.038, seg=14)
    p.rod(lid, (0, 0, 0.05), (0, 0, 0.058), 0.04, 0.038, seg=14)
    return p


def bag():
    p = Prop("bag")
    cloth = p.mat("cloth", (0.62, 0.55, 0.42), 0.95)
    strap = p.mat("strap", (0.4, 0.33, 0.22), 0.95)
    bread = p.mat("bread", (0.75, 0.5, 0.22), 0.8)
    green = p.mat("greens", (0.2, 0.45, 0.15), 0.8)
    top, bot = -0.2, -0.52
    # the body: wider at the top than at the bottom, its load bulging it a little
    vs = p.box(cloth, (-0.17, -0.05, bot), (0.17, 0.05, top))
    for v in vs:
        if v.co.z < (top + bot) * 0.5:
            v.co.x *= 0.9
            v.co.y *= 1.25
    for y in (-0.05, 0.05):
        for x in (-0.09, 0.09):
            p.rod(strap, (x, y, top), (x * 0.35, y * 0.3, 0.0), 0.006, seg=6)
        p.rod(strap, (-0.09 * 0.35, y * 0.3, 0.0), (0.09 * 0.35, y * 0.3, 0.0), 0.006, seg=6)
    # a loaf and leeks sticking out
    p.rod(bread, (-0.06, 0.0, top - 0.12), (-0.11, 0.0, top + 0.13), 0.03, 0.022, seg=10)
    p.ball(bread, (-0.11, 0.0, top + 0.13), 0.022, seg=8)
    p.rod(green, (0.07, 0.01, top - 0.1), (0.1, 0.02, top + 0.16), 0.008, 0.012, seg=6)
    p.rod(green, (0.05, -0.01, top - 0.1), (0.06, -0.03, top + 0.13), 0.008, 0.011, seg=6)
    return p


def spade():
    """A garden spade for the allotment (EHold::Shaft): the D-handle's grip at the origin, the shaft along +X, the blade's
    edge at 1.08 m; the blade's face is +Z (the digger's side)."""
    p = Prop("spade")
    wood = p.mat("wood", (0.55, 0.4, 0.22), 0.75)
    steel = p.mat("steel", (0.42, 0.44, 0.46), 0.4)
    # the D-handle: a grip across, two cheeks to the shaft
    p.rod(wood, (0.0, -0.055, 0.0), (0.0, 0.055, 0.0), 0.014, seg=8)
    for y in (-0.055, 0.055):
        p.rod(steel, (0.0, y, 0.0), (0.1, y * 0.25, 0.0), 0.007, seg=6)
    p.rod(wood, (0.09, 0.0, 0.0), (0.78, 0.0, 0.0), 0.016, 0.017, seg=8)
    p.rod(steel, (0.74, 0.0, 0.0), (0.84, 0.0, 0.0), 0.019, 0.022, seg=8)
    # the blade: a slightly dished plate, a tread at its top
    p.box(steel, (0.82, -0.09, -0.004), (1.08, 0.09, 0.004))
    p.box(steel, (0.815, -0.095, -0.007), (0.835, 0.095, 0.012))     # (past the blade's faces: no flush faces to flicker)
    return p


def watering_can():
    """A tin watering can (EHold::Upright): the top handle's grip at the origin, the can hanging under it, the spout out
    along +X (ahead of the holder) with its rose."""
    p = Prop("watering_can")
    tin = p.mat("tin", (0.3, 0.46, 0.36), 0.5)
    dark = p.mat("rose", (0.2, 0.3, 0.24), 0.5)
    top, bot = -0.07, -0.33
    p.rod(tin, (0.0, 0.0, bot), (0.0, 0.0, top), 0.105, 0.095, seg=14)
    # the handle over the top: two posts and the grip
    for x in (-0.07, 0.07):
        p.rod(tin, (x, 0.0, top), (x * 0.9, 0.0, 0.0), 0.007, seg=6)
    p.rod(tin, (-0.065, 0.0, 0.0), (0.065, 0.0, 0.0), 0.011, seg=8)
    # the spout from low on the front, up and out; the rose
    p.rod(tin, (0.09, 0.0, bot + 0.05), (0.36, 0.0, top + 0.02), 0.02, 0.012, seg=8)
    p.rod(dark, (0.35, 0.0, top + 0.01), (0.4, 0.0, top + 0.045), 0.014, 0.034, seg=10)
    return p


def mop():
    """The cleaner's mop (EHold::Shaft): the shaft's top at the origin, the shaft along +X, a clamp and a head of grey
    cotton strands at 1.2-1.32 m, splayed a little (it has been on the floor)."""
    p = Prop("mop")
    wood = p.mat("wood", (0.62, 0.47, 0.28), 0.7)
    clamp = p.mat("clamp", (0.6, 0.62, 0.64), 0.4)
    cotton = p.mat("cotton", (0.72, 0.71, 0.66), 0.95)
    p.rod(wood, (0.0, 0.0, 0.0), (1.2, 0.0, 0.0), 0.014, 0.015, seg=8)
    p.rod(clamp, (1.17, 0.0, 0.0), (1.23, 0.0, 0.0), 0.024, seg=8)
    # the strands: a ring of flat twisted cords hanging past the clamp, splayed outward
    n = 14
    for k in range(n):
        a = 2.0 * math.pi * k / n
        cy, cz = math.cos(a), math.sin(a)
        p.rod(cotton, (1.2, 0.02 * cy, 0.02 * cz), (1.32 + 0.015 * math.sin(3 * a), 0.075 * cy, 0.075 * cz), 0.009, 0.007, seg=5)
    return p


def broom():
    """The yard keeper's birch broom (EHold::Shaft, as the mop): a long pale shaft along +X, a wire band and a bundle
    of thin dark twigs fanning out to 1.45 m, flattened a little across Z (the sweeping face +Z)."""
    p = Prop("broom")
    wood = p.mat("wood", (0.7, 0.58, 0.4), 0.7)
    wire = p.mat("wire", (0.4, 0.4, 0.42), 0.5)
    twig = p.mat("twig", (0.42, 0.3, 0.2), 0.9)
    p.rod(wood, (0.0, 0.0, 0.0), (1.1, 0.0, 0.0), 0.015, 0.016, seg=8)
    p.rod(wire, (1.04, 0.0, 0.0), (1.104, 0.0, 0.0), 0.034, seg=8)
    # the twigs: two staggered fans of thin cones, wider across Y than Z
    n = 16
    for k in range(n):
        a = 2.0 * math.pi * k / n
        cy, cz = math.cos(a), math.sin(a)
        end = 1.42 + 0.03 * math.sin(5 * a)
        a0, a1 = Vector((0.98, 0.025 * cy, 0.02 * cz)), Vector((end, 0.11 * cy, 0.05 * cz))
        vs = p.rod(twig, a0, a1, 0.007, 0.003, seg=4, smooth=False)
        # each twig turned about itself a little differently (the bundle's twigs never share a flat side)
        spin = Matrix.Translation(a0) @ Matrix.Rotation(0.37 * k, 4, a1 - a0) @ Matrix.Translation(-a0)
        bmesh.ops.transform(p.bm, matrix=spin, verts=vs)
    return p


def rake():
    """The yard keeper's leaf rake (EHold::Shaft, as the mop): a wooden shaft along +X, a cross bar at 1.4 m across
    Y and twelve iron tines bent towards -Z (they comb the leaves under the holder)."""
    p = Prop("rake")
    wood = p.mat("wood", (0.62, 0.47, 0.28), 0.7)
    iron = p.mat("iron", (0.3, 0.31, 0.33), 0.45)
    p.rod(wood, (0.0, 0.0, 0.0), (1.385, 0.0, 0.0), 0.014, 0.015, seg=8)   # (its end inside the bar, not on its face)
    p.rod(iron, (1.38, -0.22, 0.0), (1.38, 0.22, 0.0), 0.012, seg=6)
    for k in range(12):
        y = -0.2 + 0.4 * k / 11
        p.rod(iron, (1.38, y, 0.0), (1.42, y, -0.07), 0.004, 0.003, seg=4, smooth=False)
    return p


def torch():
    """The night watchman's torch (EHold::Pointing, along +X): a black rubber tube, a chrome head with a pale lens at
    the far end, a red switch on top."""
    p = Prop("torch")
    rubber = p.mat("rubber", (0.05, 0.05, 0.05), 0.85)
    chrome = p.mat("chrome", (0.75, 0.76, 0.78), 0.2)
    lens = p.mat("lens", (1.0, 0.95, 0.8), 0.1, emit=3.0)
    switch = p.mat("switch", (0.7, 0.08, 0.06), 0.5)
    p.rod(rubber, (-0.05, 0.0, 0.0), (0.12, 0.0, 0.0), 0.017, seg=10)
    p.rod(chrome, (0.12, 0.0, 0.0), (0.18, 0.0, 0.0), 0.019, 0.026, seg=12)
    p.rod(lens, (0.176, 0.0, 0.0), (0.186, 0.0, 0.0), 0.023, seg=12)      # (set into the head)
    p.box(switch, (0.0, -0.007, 0.015), (0.025, 0.007, 0.022))
    return p


def bucket():
    """The cleaner's galvanised bucket on the floor by them (a stand piece, like the easel): 0.3 m tall, a little grey
    water in it, the wire handle down on one side."""
    p = Prop("bucket")
    zinc = p.mat("zinc", (0.62, 0.64, 0.66), 0.45)
    water = p.mat("water", (0.28, 0.3, 0.3), 0.08)
    p.rod(zinc, (0.0, 0.0, 0.0), (0.0, 0.0, 0.3), 0.11, 0.145, seg=14, caps=False)
    p.rod(zinc, (0.0, 0.0, 0.0), (0.0, 0.0, 0.004), 0.11, seg=14, caps=True)                  # the bottom
    inner = p.rod(zinc, (0.0, 0.0, 0.004), (0.0, 0.0, 0.3), 0.105, 0.14, seg=14, caps=False)  # the inside, facing in
    bmesh.ops.reverse_faces(p.bm, faces=list({f for v in inner for f in v.link_faces}))
    p.rod(zinc, (0.0, 0.0, 0.27), (0.0, 0.0, 0.3), 0.148, 0.15, seg=14, caps=False)     # the rolled rim
    p.rod(water, (0.0, 0.0, 0.18), (0.0, 0.0, 0.181), 0.134, seg=14)
    for y in (-0.148, 0.148):
        p.ball(zinc, (0.0, y, 0.27), 0.012, seg=6)
    p.rod(zinc, (0.0, -0.148, 0.27), (0.13, -0.07, 0.3), 0.004, seg=5)
    p.rod(zinc, (0.13, -0.07, 0.3), (0.13, 0.07, 0.3), 0.004, seg=5)
    p.rod(zinc, (0.13, 0.07, 0.3), (0.0, 0.148, 0.27), 0.004, seg=5)
    return p


def suitcase():
    """A sanatorium guest's suitcase (EHold::Hanging): the handle's grip at the origin, the case hanging under it along
    -Z, its broad sides facing +Y and -Y (beside the leg)."""
    p = Prop("suitcase")
    hide = p.mat("leatherette", (0.36, 0.2, 0.12), 0.7)
    trim = p.mat("trim", (0.2, 0.12, 0.08), 0.6)
    metal = p.mat("locks", (0.7, 0.66, 0.55), 0.35)
    top, bot = -0.06, -0.44
    p.box(hide, (-0.28, -0.085, bot), (0.28, 0.085, top))
    # the corners' caps and the band round the lid's seam
    for x in (-0.28, 0.28):
        for z in (bot, top):
            p.box(trim, (x - 0.03 if x > 0 else x - 0.004, -0.089, z - 0.004 if z == bot else z - 0.03),
                  (x + 0.004 if x > 0 else x + 0.03, 0.089, z + 0.03 if z == bot else z + 0.004))
    p.box(trim, (-0.283, -0.004, bot - 0.002), (0.283, 0.004, top + 0.002))
    # two locks on the top, the handle between its posts
    for x in (-0.15, 0.15):
        p.box(metal, (x - 0.02, -0.012, top), (x + 0.02, 0.012, top + 0.008))
    for x in (-0.06, 0.06):
        p.rod(trim, (x, 0.0, top), (x * 0.9, 0.0, 0.0), 0.007, seg=6)
    p.rod(trim, (-0.056, 0.0, 0.0), (0.056, 0.0, 0.0), 0.011, seg=8)
    return p


def easel():
    p = Prop("easel")
    wood = p.mat("wood", (0.5, 0.36, 0.2), 0.7)
    canvas = p.mat("canvas", (0.9, 0.88, 0.82), 0.9)
    sea = p.mat("sea", (0.15, 0.35, 0.5), 0.8)
    sky = p.mat("sky", (0.95, 0.6, 0.3), 0.8)
    sun = p.mat("sun", (1.0, 0.85, 0.3), 0.7)
    apex = Vector((-0.08, 0.0, 1.62))
    for foot in ((0.14, -0.33, 0.0), (0.14, 0.33, 0.0), (-0.55, 0.0, 0.0)):
        p.rod(wood, foot, apex, 0.014, seg=6, smooth=False)
    # the ledge, the canvas on it leaning back with the front legs, the painting begun: a sunset over the sea
    lean = rot_y(-7.5)
    at = Vector((0.09, 0.0, 0.92))
    p.box(wood, at + Vector((-0.02, -0.3, -0.02)), at + Vector((0.035, 0.3, 0.0)))

    def on_canvas(lo, hi):
        return [at + lean @ Vector(q) for q in ((0.012, lo[0], lo[1]), (0.012, hi[0], lo[1]), (0.012, hi[0], hi[1]), (0.012, lo[0], hi[1]))]

    vs = p.box(canvas, (-0.008, -0.26, 0.0), (0.008, 0.26, 0.42))
    bmesh.ops.transform(p.bm, matrix=Matrix.Translation(at) @ lean.to_4x4(), verts=vs)
    for mi, lo, hi in ((sky, (-0.24, 0.2), (0.24, 0.4)), (sea, (-0.24, 0.03), (0.24, 0.2))):
        q = on_canvas(lo, hi)
        fq = p.bm.faces.new([p.bm.verts.new(v) for v in q])
        fq.material_index = mi
    q = on_canvas((0.04, 0.2), (0.11, 0.27))
    for v in q:
        v += lean @ Vector((0.0006, 0, 0))
    p.bm.faces.new([p.bm.verts.new(v) for v in q]).material_index = sun
    return p


# ---------------------------------------------------------------------------------------------- the days' things
def _board_half(p, x0, x1, xi0, xi1, frame, light, dark, rank0):
    """One half of the folding board: a shallow wooden tray 0.022 m high, open underneath (it lies on the seat), its
    top one grid (the frame round three sides and four ranks of squares up to the hinge) so no edge has a T-joint."""
    top = 0.022
    ys = [-0.16] + [-0.14 + 0.035 * k for k in range(9)] + [0.16]
    xs = sorted(set(round(x, 6) for x in [x0, xi0] + [xi0 + (xi1 - xi0) * k / 4.0 for k in range(5)] + [xi1, x1]))
    grid = {(i, j): p.bm.verts.new((x, y, top)) for i, x in enumerate(xs) for j, y in enumerate(ys)}
    for i in range(len(xs) - 1):
        for j in range(len(ys) - 1):
            cx, cy = (xs[i] + xs[i + 1]) * 0.5, (ys[j] + ys[j + 1]) * 0.5
            if xi0 < cx < xi1 and -0.14 < cy < 0.14:
                rank = rank0 + int((cx - xi0) / (xi1 - xi0) * 4.0)
                fil = int((cy + 0.14) / 0.035)
                mi = dark if (rank + fil) % 2 == 0 else light       # a1 dark, h1 light
            else:
                mi = frame
            f = p.bm.faces.new([grid[i, j], grid[i + 1, j], grid[i + 1, j + 1], grid[i, j + 1]])
            f.material_index = mi
    # the sides, down to the seat, from the same border vertices
    ring = [(i, 0) for i in range(len(xs))] + [(len(xs) - 1, j) for j in range(1, len(ys))] + \
        [(i, len(ys) - 1) for i in range(len(xs) - 2, -1, -1)] + [(0, j) for j in range(len(ys) - 2, 0, -1)]
    low = {k: p.bm.verts.new((grid[k].co.x, grid[k].co.y, 0.0)) for k in ring}
    for a, b in zip(ring, ring[1:] + ring[:1]):
        f = p.bm.faces.new([grid[b], grid[a], low[a], low[b]])
        f.material_index = frame


def _piece(p, kind, mi, at, toward):
    """A Staunton-ish piece, 6-sided, standing at at (its foot on the board), a knight looking along toward (+-X)."""
    prof = {
        "p": [(0.0085, 0.0), (0.0035, 0.017), (0.0058, 0.021), (0.0, 0.027)],
        "r": [(0.0095, 0.0), (0.0075, 0.022), (0.0095, 0.024), (0.0095, 0.031), (0.0, 0.031)],
        "b": [(0.009, 0.0), (0.004, 0.025), (0.006, 0.03), (0.0, 0.04)],
        "q": [(0.01, 0.0), (0.0045, 0.03), (0.0072, 0.036), (0.0045, 0.038), (0.0, 0.044)],
        "k": [(0.01, 0.0), (0.0048, 0.032), (0.0072, 0.038), (0.0045, 0.041), (0.0, 0.043)],
        "n": [(0.0095, 0.0), (0.0075, 0.006), (0.0, 0.008)],
    }[kind]
    p.lathe(mi, prof, c=at, seg=6)
    x, y, z = at
    if kind == "k":
        p.box(mi, (x - 0.0015, y - 0.0015, z + 0.041), (x + 0.0015, y + 0.0015, z + 0.054))
        p.box(mi, (x - 0.0011, y - 0.0045, z + 0.0465), (x + 0.0011, y + 0.0045, z + 0.0495))
    elif kind == "n":
        s = 1.0 if toward > 0 else -1.0
        neck = p.box(mi, (-0.0045, -0.0035, 0.0), (0.0045, 0.0035, 0.024), rot=rot_y(-12.0 * s))
        bmesh.ops.translate(p.bm, vec=Vector((x - 0.001 * s, y, z + 0.007)), verts=neck)
        hx = sorted((x - 0.004 * s, x + 0.011 * s))           # the head, the muzzle toward the other side
        p.box(mi, (hx[0], y - 0.003, z + 0.022), (hx[1], y + 0.003, z + 0.031))


def chess():
    """The folding chess board set out on a bench between two players (PeopleLife: the plan's chess on a bench), opened
    flat, 0.32 m square, a game of the Italian opening some fifteen moves in; the two pawns taken stand on the seat by
    the board. The origin on the seat under the board's middle, files along Y, White at -X."""
    p = Prop("chess")
    frame = p.mat("frame", (0.3, 0.17, 0.08), 0.55)
    light = p.mat("light", (0.82, 0.67, 0.44), 0.5)
    dark = p.mat("dark", (0.4, 0.23, 0.11), 0.5)
    brass = p.mat("hinge", (0.75, 0.6, 0.3), 0.35)
    white = p.mat("white", (0.88, 0.83, 0.7), 0.4)
    black = p.mat("black", (0.07, 0.05, 0.04), 0.35)
    _board_half(p, -0.16, -0.001, -0.14, -0.001, frame, light, dark, 0)
    _board_half(p, 0.001, 0.16, 0.001, 0.14, frame, light, dark, 4)
    for y in (-0.12, 0.12):                     # the hinges over the fold
        p.box(brass, (-0.006, y - 0.012, 0.0222), (0.006, y + 0.012, 0.0236))
    s = 0.139 / 4.0

    def square(f, r):
        x = -0.14 + s * (r + 0.5) if r < 4 else 0.001 + s * (r - 4 + 0.5)
        return (x, -0.14 + 0.035 * (f + 0.5), 0.022)

    # files a..h = 0..7 (along +Y), ranks 1..8 = 0..7 (along +X)
    game = [("r", 0, 0), ("b", 2, 0), ("q", 3, 0), ("r", 5, 0), ("k", 6, 0), ("n", 2, 2), ("n", 5, 2), ("b", 2, 3),
            ("p", 0, 1), ("p", 2, 1), ("p", 3, 2), ("p", 4, 3), ("p", 5, 1), ("p", 6, 1), ("p", 7, 2)]
    for kind, f, r in game:
        _piece(p, kind, white, square(f, r), 1.0)
        _piece(p, kind, black, square(f, 7 - r), -1.0)
    # the taken pawns (b2 and b7) on the seat beside the board
    _piece(p, "p", black, (-0.07, -0.2, 0.0), 1.0)
    _piece(p, "p", white, (0.07, 0.2, 0.0), -1.0)
    return p


def cake_box():
    """The birthday cake carried home (EHold::Hanging, as the bag): a square white card box with a blue lid, its pink
    print cut into the lid's top (its own faces of the lid, nothing laid on it), tied crosswise with red string knotted
    in a loop on top; the origin at the loop (the grip), the box hangs along -Z."""
    p = Prop("cake_box")
    card = p.mat("card", (0.93, 0.91, 0.86), 0.85)
    lid = p.mat("lid", (0.47, 0.64, 0.8), 0.8)
    rim = p.mat("lid_print", (0.95, 0.75, 0.8), 0.8)
    string = p.mat("string", (0.75, 0.14, 0.12), 0.9)
    bot, seam, top = -0.205, -0.13, -0.1
    p.box(card, (-0.13, -0.13, bot), (0.13, 0.13, seam + 0.002))
    # the lid: four sides down to the seam (open underneath: the box is in it) and a 3 x 3 grid on top, the middle cell
    # the print; the sides start from the grid's border vertices, so no edge has a T-joint
    e, q = 0.133, 0.08
    cs = [-e, -q, q, e]
    grid = {(i, j): p.bm.verts.new((x, y, top)) for i, x in enumerate(cs) for j, y in enumerate(cs)}
    for i in range(3):
        for j in range(3):
            f = p.bm.faces.new([grid[i, j], grid[i + 1, j], grid[i + 1, j + 1], grid[i, j + 1]])
            f.material_index = rim if (i, j) == (1, 1) else lid
    ring = [(i, 0) for i in range(4)] + [(3, j) for j in range(1, 4)] + [(i, 3) for i in range(2, -1, -1)] + [(0, j) for j in (2, 1)]
    low = {k: p.bm.verts.new((grid[k].co.x, grid[k].co.y, seam)) for k in ring}
    for a, b in zip(ring, ring[1:] + ring[:1]):
        f = p.bm.faces.new([grid[b], grid[a], low[a], low[b]])
        f.material_index = lid
    # the string: round, sunk a little into the card it is tied round (it presses in; no face lies on another)
    r = 0.0022
    sink = r * 0.5
    for axis in (0, 1):
        def run(a, b):
            if axis == 1:
                a, b = (a[1], a[0], a[2]), (b[1], b[0], b[2])
            p.rod(string, a, b, r, seg=6, smooth=False)
        lift = top + sink + 0.0012 * axis                      # one string over the other where they cross
        run((-e - sink, 0.0, lift), (e + sink, 0.0, lift))                                 # over the lid
        for sx in (-1.0, 1.0):
            run((sx * (e + sink), 0.0, lift), (sx * (e + sink), 0.0, seam))                # down the lid's side
            # down the box's side (sunk deeper: its outer side would lie in the lid's side's plane)
            run((sx * (0.13 + r * 0.3), 0.0, seam + 0.001), (sx * (0.13 + r * 0.3), 0.0, bot + 0.002))
    p.ball(string, (0.0, 0.0, top + 0.007), 0.007, seg=6)                                 # the knot
    for x in (-0.022, 0.022):
        p.rod(string, (x, 0.0, top + 0.006), (x * 0.25, 0.0, 0.0), 0.0018, seg=4, smooth=False)
    p.rod(string, (-0.008, 0.0, 0.0), (0.008, 0.0, 0.0), 0.0022, seg=4, smooth=False)
    return p


def cake():
    """The birthday cake on the table: a round cream cake on a white plate, pink rosettes round its top's edge, berries,
    and five lit candles; the origin on the table under its middle, +Z up."""
    p = Prop("cake")
    plate = p.mat("plate", (0.95, 0.95, 0.93), 0.3)
    side = p.mat("sponge_cream", (0.93, 0.83, 0.72), 0.7)
    cream = p.mat("cream", (0.98, 0.96, 0.9), 0.6)
    rose = p.mat("rosette", (0.93, 0.56, 0.64), 0.6)
    berry = p.mat("berry", (0.6, 0.05, 0.1), 0.35)
    wax = [p.mat("candle_blue", (0.35, 0.55, 0.9), 0.5), p.mat("candle_pink", (0.95, 0.6, 0.75), 0.5),
           p.mat("candle_yellow", (0.98, 0.85, 0.35), 0.5)]
    flame = p.mat("flame", (1.0, 0.72, 0.25), 0.5, emit=6.0)
    wick = p.mat("wick", (0.1, 0.08, 0.06), 0.9)
    p.lathe(plate, [(0.0, 0.0), (0.11, 0.0), (0.148, 0.007), (0.145, 0.009), (0.118, 0.004), (0.0, 0.004)][::-1], seg=20)
    p.rod(side, (0.0, 0.0, 0.004), (0.0, 0.0, 0.064), 0.12, seg=20, caps=False)
    p.lathe(cream, [(0.12, 0.064), (0.121, 0.068), (0.116, 0.071), (0.0, 0.071)], seg=20)
    for k in range(12):
        a = 2.0 * math.pi * (k + 0.5) / 12.0
        p.lathe(rose, [(0.011, 0.0), (0.008, 0.008), (0.0, 0.016)], c=(0.1 * math.cos(a), 0.1 * math.sin(a), 0.069), seg=6,
                phase=a)
    for k in range(6):
        a = 2.0 * math.pi * k / 6.0
        p.lathe(berry, [(0.0, -0.002), (0.007, 0.002), (0.006, 0.008), (0.0, 0.011)], c=(0.06 * math.cos(a), 0.06 * math.sin(a), 0.071), seg=6)
    for k in range(5):
        a = 2.0 * math.pi * k / 5.0 + 0.3
        x, y = 0.03 * math.cos(a), 0.03 * math.sin(a)
        p.rod(wax[k % 3], (x, y, 0.07), (x, y, 0.125), 0.0032, seg=6, smooth=False)
        p.rod(wick, (x, y, 0.125), (x, y, 0.13), 0.0008, seg=4, smooth=False)
        p.lathe(flame, [(0.0, 0.127), (0.0042, 0.133), (0.0028, 0.139), (0.0, 0.147)], c=(x, y, 0.0), seg=6)
    return p


def flowers():
    """A birthday guest's carnations (EHold::Hanging: carried heads down by the side, as people do walking): five red
    blooms on their stems in a paper cone, the grip at the origin round the stems, the blooms along -Z."""
    p = Prop("flowers")
    stem = p.mat("stems", (0.22, 0.45, 0.18), 0.8)
    leaf = p.mat("leaves", (0.3, 0.52, 0.25), 0.8)
    paper = p.mat("paper", (0.92, 0.9, 0.78), 0.85)
    red = p.mat("carnation", (0.78, 0.06, 0.1), 0.7)
    p.rod(stem, (0.0, 0.0, 0.06), (0.0, 0.0, -0.21), 0.009, 0.014, seg=6)
    # the paper cone open at the blooms' end: its outside, and its inside turned to face in
    p.lathe(paper, [(0.06, -0.19), (0.022, -0.04)], seg=8)
    inner = p.lathe(paper, [(0.058, -0.19), (0.02, -0.04)], seg=8)
    bmesh.ops.reverse_faces(p.bm, faces=list({f for v in inner for f in v.link_faces}))
    heads = [(0.0, 0.0, -0.255), (0.042, 0.0, -0.235), (-0.021, 0.037, -0.235), (-0.021, -0.037, -0.235), (0.02, 0.03, -0.215)]
    for k, (x, y, z) in enumerate(heads):
        p.rod(stem, (x * 0.4, y * 0.4, -0.17), (x, y, z + 0.034), 0.0025, seg=4, smooth=False)
        tilt = Vector((x, y, -0.25)).normalized().to_track_quat("-Z", "Y").to_matrix()
        p.lathe(red, [(0.0, -0.038), (0.019, -0.033), (0.024, -0.02), (0.014, -0.007), (0.006, 0.0)], c=(x, y, z + 0.034),
                seg=7, rot=tilt, phase=0.4 * k)
    for sgn in (-1.0, 1.0):
        blade = p.box(leaf, (-0.003, -0.0008, -0.07), (0.003, 0.0008, 0.02), rot=rot_y(14.0 * sgn))
        bmesh.ops.translate(p.bm, vec=Vector((0.035 * sgn, 0.0, -0.2)), verts=blade)
    return p


MAKERS = [phone, cigarette, camera, umbrella, rod, brush, binoculars, newspaper, book, knitting, ice_cream, cup, bag, easel,
          spade, watering_can, mop, torch, bucket, suitcase, broom, rake, chess, cake_box, cake, flowers]


def preview(o, name):
    """Two views of it on a grey ground (workbench: flat colours as the game's)."""
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.display.shading.light = "STUDIO"
    sc.display.shading.color_type = "MATERIAL"
    sc.render.resolution_x, sc.render.resolution_y = 700, 700
    sc.render.film_transparent = False
    lo = Vector((min(v.co[i] for v in o.data.vertices) for i in range(3)))
    hi = Vector((max(v.co[i] for v in o.data.vertices) for i in range(3)))
    c, size = (lo + hi) * 0.5, max((hi - lo).length, 0.05)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = size * 1.15
    cam.data.clip_start, cam.data.clip_end = 0.001, 100.0
    for tag, d in (("a", Vector((1.0, -1.4, 0.8))), ("b", Vector((-1.2, 1.0, -0.5)))):
        cam.location = c + d.normalized() * size * 3.0
        cam.rotation_euler = (c - cam.location).to_track_quat("-Z", "Y").to_euler()
        sc.render.filepath = os.path.join(OUT, "preview_%s_%s.png" % (name, tag))
        bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam)


def main():
    os.makedirs(OUT, exist_ok=True)
    failed = []
    for make in MAKERS:
        if ONLY and make.__name__ not in ONLY:
            continue
        bpy.ops.wm.read_factory_settings(use_empty=True)
        p = make()
        o = p.build()
        if p.lint:
            print("LINT %-11s %s: NOT written" % (p.name, ", ".join(p.lint)))
            failed.append(p.name)
            continue
        tris = len(o.data.polygons)
        bpy.ops.object.select_all(action="DESELECT")
        o.select_set(True)
        bpy.context.view_layer.objects.active = o
        out = os.path.join(OUT, p.name + ".glb")
        bpy.ops.export_scene.gltf(filepath=out, use_selection=True, export_format="GLB", export_yup=True)
        lo = [min(v.co[i] for v in o.data.vertices) for i in range(3)]
        hi = [max(v.co[i] for v in o.data.vertices) for i in range(3)]
        print("PROP %-11s %4d triangles (%d hidden dropped), %d materials, x %.3f..%.3f y %.3f..%.3f z %.3f..%.3f -> %s" % (
            p.name, tris, p.dropped, len(o.data.materials), lo[0], hi[0], lo[1], hi[1], lo[2], hi[2], out))
        if PREVIEW:
            preview(o, p.name)
    print("LINT %d props, %s" % (len(MAKERS) if not ONLY else len(ONLY), ("failed: " + ", ".join(failed)) if failed else "all clean"))
    if failed:
        sys.exit(1)


if __name__ == "__main__":
    main()
