"""The things the townsfolk's manners put in their hands (docs/town_manners.md): a phone, a cigarette, a camera, an
umbrella, a fishing rod, a brush, binoculars, a newspaper, a book, knitting, an ice cream, a paper cup, a shopping
bag, and the painter's easel.

    Blender -b -P blender/humans/props.py [-- --preview]
        -> Saved/ClipSources/props/<name>.glb (unreal/scripts/import_props.py -> /Game/Characters/Humans/Props/SM_Prop_<name>)
        --preview: also Saved/ClipSources/props/preview_<name>.png (two views each), to look them over before the import

Built from primitives, flat colours (no textures, no words on them: nothing to localize), 100-900 triangles each.
Metres. How each lies in its own frame is what ANpcBody::PlaceMannerProp expects (LyudGesture::HoldOf):

    umbrella, ice_cream, cup   origin at the grip, +Z up
    bag                        origin at the handles' top, hangs along -Z, its flat side along X
    phone                      origin in the middle, long axis X, the screen +Z
    cigarette, brush           along +X from the fingers (origin where they hold it)
    rod                        along +X from the grip (origin in the grip)
    camera, binoculars         between the hands along X, the side the holder looks into +Z (the lens -Z), the top +Y
    newspaper, book, knitting  between the hands along X, the pages the holder reads +Z, the top edge +Y
    easel                      origin on the ground under it, the canvas faces +X (the painter stands at +X)
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

    def build(self):
        me = bpy.data.meshes.new("SM_Prop_" + self.name)
        bmesh.ops.triangulate(self.bm, faces=self.bm.faces[:])
        self.bm.normal_update()
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
    p.box(glass, (-0.070, -0.032, 0.0035), (0.070, 0.032, 0.0042))
    p.rod(lens, (0.055, 0.02, -0.004), (0.055, 0.02, -0.0055), 0.006, seg=10)
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
    p.rod(glass, (0.0, -0.004, -0.062), (0.0, -0.004, -0.0635), 0.024, seg=16)
    p.box(glass, (-0.012, 0.045, 0.014), (0.012, 0.054, 0.0145))        # the eyepiece
    return p


def umbrella():
    p = Prop("umbrella")
    cloth = p.mat("cloth", (0.05, 0.09, 0.2), 0.7)
    inner = p.mat("inner", (0.03, 0.05, 0.11), 0.8)
    steel = p.mat("steel", (0.55, 0.56, 0.58), 0.35)
    wood = p.mat("handle", (0.2, 0.11, 0.05), 0.5)
    # the crook handle, the shaft, the tip
    p.rod(wood, (0, 0, -0.07), (0, 0, 0.06), 0.011, seg=10)
    p.rod(wood, (0, 0, -0.07), (0.0, -0.03, -0.1), 0.011, seg=10)
    p.rod(wood, (0.0, -0.03, -0.1), (0.0, -0.06, -0.075), 0.011, 0.01, seg=10)
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
        p.rod(glass, (x, 0, -0.082), (x, 0, -0.083), 0.024, seg=14)
        p.rod(ring, (x, 0, 0.045), (x, 0, 0.058), 0.015, seg=12)
        p.rod(glass, (x, 0, 0.058), (x, 0, 0.0585), 0.011, seg=12)
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
        half = p.box(cover, (lo, -h / 2, -0.016), (hi, h / 2, -0.012))
        lo, hi = sorted((s * 0.002, s * (w - 0.005)))
        half += p.box(pages, (lo, -h / 2 + 0.005, -0.012), (hi, h / 2 - 0.005, 0.0))
        lo, hi = sorted((s * 0.012, s * (w - 0.015)))
        half += _columns(p, ink, lo, hi, -h / 2 + 0.02, h / 2 - 0.02, 0.0006, 12, Matrix.Identity(3), False)
        bmesh.ops.transform(p.bm, matrix=tilt.to_4x4(), verts=half)
    p.box(cover, (-0.004, -h / 2, -0.018), (0.004, h / 2, -0.012))      # the spine
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


MAKERS = [phone, cigarette, camera, umbrella, rod, brush, binoculars, newspaper, book, knitting, ice_cream, cup, bag, easel]


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
    for make in MAKERS:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        p = make()
        o = p.build()
        tris = len(o.data.polygons)
        bpy.ops.object.select_all(action="DESELECT")
        o.select_set(True)
        bpy.context.view_layer.objects.active = o
        out = os.path.join(OUT, p.name + ".glb")
        bpy.ops.export_scene.gltf(filepath=out, use_selection=True, export_format="GLB", export_yup=True)
        lo = [min(v.co[i] for v in o.data.vertices) for i in range(3)]
        hi = [max(v.co[i] for v in o.data.vertices) for i in range(3)]
        print("PROP %-11s %4d triangles, %d materials, x %.3f..%.3f y %.3f..%.3f z %.3f..%.3f -> %s" % (
            p.name, tris, len(o.data.materials), lo[0], hi[0], lo[1], hi[1], lo[2], hi[2], out))
        if PREVIEW:
            preview(o, p.name)


if __name__ == "__main__":
    main()
