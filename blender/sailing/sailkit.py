"""Shared kit of the sailing school's Blender builds (docs/yacht_school.md "Meshes (part C)"): material slots, a mesh
builder on bmesh (primitives that keep material, UVs and vertex colour together), the lint every asset passes, the
glTF export and the manifest.

Units metres, Z up, transforms applied. Boats: the boat frame of boatspec.py (+X forward, +Y port, origin waterline
amidships). Buildings / props: x along the front, -Y the front face (seen from the sea for the boathouse), origin on the
floor/ground. Exported with export_yup: Unreal gets X = x, Y = -y, Z = z (cm).

Material slots
  SL_*  flat slots (MAT_SPEC: colour, roughness, metallic, emissive) -> MI_SL_<slot> on M_Sail_Flat;
        UV0 in metres (box projection) like the town's buildings.
  SL_Hull, SL_HullDeck  gelcoat; COLOR_0.R = tint mask (1 = takes the per-boat tint from custom primitive data 3..5),
        COLOR_0.G = baked cavity/AO (1 = open). M_Sail_Hull.
  SL_Sail  sailcloth: UV0 = 0..1 over the class's sail picture (unique texels per side), UV1 = (distance from luff 0..1,
        height 0..1) for the vertex-shader belly / flutter, COLOR_0.B = 1 on the starboard face, 0 on the port face
        (the sail number boxes differ per side). M_Sail_Cloth (unreal/scripts/make_sailing_materials.py).
  CT_*  the town's building slots (blender/city/citymats.py, /Game/City/Materials/MI_CT_<slot>): the boathouse and the
        outdoor structures share the town's concrete, plaster, timber, steel.
  SLP_<name>  picture slots: UV0 0..1 over one picture (signs, the sail mosaic, the blackboard, charts, the knot board);
        pictures from tools/make_sailing_signs.py (words) and blender/sailing/textures.py (no words).
"""
import json
import math
import os
import sys

import bpy  # noqa: F401  (first: bmesh needs it with the pip module)
import bmesh
import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(HERE, "out")
TEX = os.path.join(OUT, "textures")
sys.path.insert(0, os.path.join(ROOT, "blender", "city"))
sys.path.insert(0, HERE)

# name: (hex sRGB, roughness, metallic, emissive)
MAT_SPEC = {
    "SL_Hull": ("#F2F0EA", 0.25, 0.0, 0.0),        # glossy gelcoat (tinted per boat where COLOR_0.R = 1)
    "SL_HullDeck": ("#ECEAE2", 0.55, 0.0, 0.0),    # non-skid deck and cockpit (tinted where COLOR_0.R = 1)
    "SL_NonSkid": ("#DCDCD6", 0.85, 0.0, 0.0),     # moulded non-skid pattern areas (cockpit floor, hiking decks)
    "SL_Stripe": ("#1C2C55", 0.3, 0.0, 0.0),       # the boot stripe (navy gelcoat)
    "SL_WoodDeck": ("#B07A3E", 0.3, 0.0, 0.0),     # varnished plywood decks (cadet), plank lines in SL_Dark
    "SL_PaintSteel": ("#2F6F6A", 0.45, 0.3, 0.0),  # sea-green painted steel: the boat door, the stair, the door frames
    "SL_Grime": ("#8C877B", 0.95, 0.0, 0.0),       # splash staining along the wall feet (thin layer on the plaster)
    "SL_Sand": ("#CDBB97", 0.95, 0.0, 0.0),        # sand drifted against the walls
    "SL_Stain": ("#6F675A", 0.6, 0.0, 0.0),        # paint and oil stains on the hall floor
    "SL_Puddle": ("#55585A", 0.05, 0.0, 0.0),      # puddles on the park slab (wet, glossy)
    "SL_Foil": ("#F4F4F0", 0.3, 0.0, 0.0),         # rudder and board blades (white)
    "SL_FoilWood": ("#9A6A3C", 0.35, 0.0, 0.0),    # varnished plywood blades (optimist, cadet)
    "SL_Alu": ("#B9BEC2", 0.3, 1.0, 0.0),          # anodised spars
    "SL_AluDull": ("#9EA3A6", 0.5, 1.0, 0.0),      # the launch's riveted aluminium
    "SL_Steel": ("#C9CDD0", 0.2, 1.0, 0.0),        # stainless fittings, shackles, pintles, chainplates
    "SL_Black": ("#1E1F21", 0.55, 0.0, 0.0),       # plastic fittings, mast cap, blocks' cheeks, tiller grip
    "SL_Rope": ("#E6DFCC", 0.8, 0.0, 0.0),         # sheets and lines (cream braid)
    "SL_RopeRed": ("#B5302A", 0.8, 0.0, 0.0),
    "SL_RopeBlue": ("#2D4F8E", 0.8, 0.0, 0.0),
    "SL_Strap": ("#1F2F55", 0.85, 0.0, 0.0),       # navy webbing: hiking straps, toe straps
    "SL_Wood": ("#A8743F", 0.35, 0.0, 0.0),        # varnished ash: tillers, thwarts, gunwales
    "SL_WoodRaw": ("#B99063", 0.75, 0.0, 0.0),     # bare pine: trestles, racks, duckboards
    "SL_WoodPaint": ("#3E6B54", 0.6, 0.0, 0.0),    # green-painted wood (trolleys' cradles, racks)
    "SL_Buoyancy": ("#C2362B", 0.6, 0.0, 0.0),     # red buoyancy bags, life jackets
    "SL_Orange": ("#F05A1A", 0.45, 0.0, 0.0),      # race marks, buoys
    "SL_Yellow": ("#E9B92A", 0.45, 0.0, 0.0),      # training buoys, the man-overboard dummy
    "SL_White": ("#F3F2EE", 0.5, 0.0, 0.0),        # painted white parts
    "SL_Red": ("#B3241F", 0.5, 0.0, 0.0),          # painted red parts (signal flags' red is in the flag picture)
    "SL_Blue": ("#2F5B9A", 0.5, 0.0, 0.0),
    "SL_Green": ("#3C7A45", 0.55, 0.0, 0.0),
    "SL_Rubber": ("#262626", 0.9, 0.0, 0.0),       # tyres, fenders, the launch's rubbing strake
    "SL_Canvas": ("#5F6B4E", 0.9, 0.0, 0.0),       # khaki tarpaulin covers, sail bags
    "SL_CanvasBlue": ("#3D5675", 0.9, 0.0, 0.0),
    "SL_Galv": ("#9DA19F", 0.45, 0.8, 0.0),        # galvanised steel: trolleys' frames, the winch, bollards' caps
    "SL_Iron": ("#3A3B3C", 0.6, 0.6, 0.0),         # cast iron bollards, the anchor block's ring
    "SL_Brass": ("#B48E4E", 0.3, 0.9, 0.0),        # the bell, the barometer's case, the cups
    "SL_Glass": ("#B8C8CC", 0.05, 0.0, 0.0),       # small glass (the barometer's face cover)
    "SL_Paper": ("#EEE8D8", 0.9, 0.0, 0.0),
    "SL_Engine": ("#4F6F86", 0.4, 0.2, 0.0),       # the outboard's painted cowl («Вихрь» blue-grey)
    "SL_PaintTin": ("#C8C2B0", 0.35, 0.8, 0.0),
    "SL_Dark": ("#2C2A28", 0.7, 0.0, 0.0),         # blackboard frame, locker vents, shadows
    "SL_Cork": ("#B68A5A", 0.9, 0.0, 0.0),         # the notice board's cork
    "SL_Lamp": ("#FFF2D8", 0.4, 0.0, 6.0),         # opal lamp glass (lit)
    "SL_Sail": ("#F6F5F0", 0.7, 0.0, 0.0),         # sailcloth (picture slot, see above)
}
# slots whose UV0 is a 0..1 picture (no metre projection): SL_Sail + every SLP_*
def is_picture(slot):
    return slot == "SL_Sail" or slot.startswith("SLP_") or slot.startswith("SL_Flag")


def _lin(hx):
    hx = hx.lstrip("#")
    c = [int(hx[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return tuple(x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c)


def city_spec():
    import citymats
    return citymats.MATERIALS


def slot_colour(slot):
    if slot in MAT_SPEC:
        return MAT_SPEC[slot][0], MAT_SPEC[slot][1], MAT_SPEC[slot][2]
    cs = city_spec()
    if slot in cs:
        return cs[slot][0], cs[slot][1], 0.6 if slot == "CT_Aluminium" else 0.0
    if slot.startswith("SLP_"):
        return "#EEE8D8", 0.8, 0.0       # a picture slot without its picture (the notice board: the engine writes it)
    return "#FF00FF", 0.5, 0.0


V = Vector


def frame_from(x, up=(0, 0, 1)):
    """Orthonormal (x, y, z) with x along the given direction."""
    x = Vector(x).normalized()
    up = Vector(up)
    if abs(x.dot(up.normalized())) > 0.95:
        up = Vector((1, 0, 0)) if abs(x.x) < 0.9 else Vector((0, 1, 0))
    y = up.cross(x).normalized()
    z = x.cross(y)
    return x, y, z


class Mesh:
    """A bmesh with material slots, UV0 (explicit or metre-projected), UV1 (optional) and COLOR_0."""

    def __init__(self, name, uv1=False):
        self.name = name
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.uv1 = self.bm.loops.layers.uv.new("UV1") if uv1 else None
        self.col = self.bm.loops.layers.color.new("Col")
        self.explicit = self.bm.faces.layers.int.new("explicit")
        self.smooth_slots = set()
        self.slots = []
        self.colour = (0.0, 1.0, 0.0, 1.0)     # R tint mask, G open (AO), B side, A 1
        self.collision = []                     # boxes: dict(center, size, rot_z_deg)
        self.closed = True                      # lint: expect every island watertight
        self.uv_scale = 1.0                     # metre-UV scale (1 = metres)
        self.sharp_angle = None                 # degrees: smooth slots get sharp edges where faces meet steeper

    def mi(self, slot):
        if slot not in self.slots:
            self.slots.append(slot)
        return self.slots.index(slot)

    # ---------------------------------------------------------------- faces
    def face(self, pts, slot, uvs=None, uv1s=None, col=None, smooth=False):
        vs = [p if isinstance(p, bmesh.types.BMVert) else self.bm.verts.new(Vector(p)) for p in pts]
        try:
            f = self.bm.faces.new(vs)
        except ValueError:
            return None
        f.material_index = self.mi(slot)
        f.smooth = smooth
        c = col or self.colour
        for i, l in enumerate(f.loops):
            l[self.col] = c
            if uvs is not None:
                l[self.uv].uv = uvs[i]
            if uv1s is not None and self.uv1 is not None:
                l[self.uv1].uv = uv1s[i]
        if uvs is not None:
            f[self.explicit] = 1
        return f

    def quad_grid(self, P, slot, closed_u=False, uvf=None, uv1f=None, colf=None, smooth=False, flip=False):
        """Faces over a grid of points P[i][j] (i rows, j columns); closed_u wraps j. uvf(i, j) -> explicit uv (j runs to
        nJ on the wrapping column so a seam can be cut); uv1f likewise; colf(i, j) -> colour of the face (i, j)."""
        nI, nJ = len(P), len(P[0])
        V_ = [[self.bm.verts.new(Vector(P[i][j])) for j in range(nJ)] for i in range(nI)]
        jr = range(nJ) if closed_u else range(nJ - 1)
        out = []
        for i in range(nI - 1):
            for j in jr:
                idx = [(i, j), (i, j + 1), (i + 1, j + 1), (i + 1, j)]
                if flip:
                    idx = idx[::-1]
                vs = [V_[a][b % nJ] for a, b in idx]
                uvs = [uvf(a, b) for a, b in idx] if uvf else None
                uv1s = [uv1f(a, b) for a, b in idx] if uv1f else None
                col = colf(i, j) if colf else None
                out.append(self.face(vs, slot, uvs, uv1s, col, smooth))
        return V_, out

    # ---------------------------------------------------------------- primitives
    def box(self, c, size, slot, rot=None, faces="all", smooth=False, slots=None):
        """slots: {side: slot} overrides per side (sides xXyYzZ = -x +x -y +y -z +z)."""
        r = rot or Matrix.Identity(3)
        h = Vector(size) / 2
        c = Vector(c)
        v = [self.bm.verts.new(c + r @ Vector((sx * h.x, sy * h.y, sz * h.z))) for sz in (-1, 1) for sy in (-1, 1) for sx in (-1, 1)]
        sides = {"z": (0, 2, 3, 1), "Z": (4, 5, 7, 6), "y": (0, 1, 5, 4), "Y": (2, 6, 7, 3), "x": (0, 4, 6, 2), "X": (1, 3, 7, 5)}
        for k, ids in sides.items():
            if faces == "all" or k in faces:
                self.face([v[i] for i in ids], (slots or {}).get(k, slot), smooth=smooth)

    def bx(self, slot, x0, x1, y0, y1, z0, z1, faces="all", slots=None):
        if x1 - x0 < 1e-5 or y1 - y0 < 1e-5 or z1 - z0 < 1e-5:
            return
        self.box(((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), (x1 - x0, y1 - y0, z1 - z0), slot, faces=faces, slots=slots)

    def beam(self, a, b, w, h, slot, up=(0, 0, 1)):
        """A rectangular bar from a to b, w across, h along up."""
        a, b = Vector(a), Vector(b)
        x, y, z = frame_from(b - a, up)
        r = Matrix((x, y, z)).transposed()
        self.box((a + b) / 2, ((b - a).length, w, h), slot, rot=r)

    def tube(self, a, b, r, slot, seg=10, caps=True, r1=None, smooth=True, up=(0, 0, 1)):
        a, b = Vector(a), Vector(b)
        x, y, z = frame_from(b - a, up)
        rb_ = r if r1 is None else r1
        ra = [self.bm.verts.new(a + (y * math.cos(2 * math.pi * i / seg) + z * math.sin(2 * math.pi * i / seg)) * r) for i in range(seg)]
        rb = [self.bm.verts.new(b + (y * math.cos(2 * math.pi * i / seg) + z * math.sin(2 * math.pi * i / seg)) * rb_) for i in range(seg)]
        for i in range(seg):
            j = (i + 1) % seg
            self.face((ra[i], ra[j], rb[j], rb[i]), slot, smooth=smooth)
        if caps:
            self.face(ra[::-1], slot)
            self.face(rb, slot)

    def pipe(self, pts, r, slot, seg=6, caps=True, smooth=True):
        """A tube along a polyline (ropes, rails, frames); rings oriented by the bisector."""
        pts = [Vector(p) for p in pts]
        n = len(pts)
        rings = []
        prev_y = None
        for k in range(n):
            if k == 0:
                t = pts[1] - pts[0]
            elif k == n - 1:
                t = pts[-1] - pts[-2]
            else:
                t = (pts[k + 1] - pts[k]).normalized() + (pts[k] - pts[k - 1]).normalized()
            if t.length < 1e-9:
                t = pts[min(k + 1, n - 1)] - pts[max(k - 1, 0)]
            t = t.normalized()
            if prev_y is None:
                x, y, z = frame_from(t)
            else:
                z = t.cross(prev_y).normalized()
                if z.length < 1e-6:
                    x, y, z = frame_from(t)
                else:
                    y = z.cross(t).normalized()
            prev_y = y
            rings.append([self.bm.verts.new(pts[k] + (y * math.cos(2 * math.pi * i / seg) + z * math.sin(2 * math.pi * i / seg)) * r) for i in range(seg)])
        for k in range(n - 1):
            for i in range(seg):
                j = (i + 1) % seg
                self.face((rings[k][i], rings[k][j], rings[k + 1][j], rings[k + 1][i]), slot, smooth=smooth)
        if caps:
            self.face(rings[0][::-1], slot)
            self.face(rings[-1], slot)

    def lathe(self, prof, slot, seg=24, c=(0, 0, 0), cap0=True, cap1=True, sx=1.0, sy=1.0, smooth=True, axis=None):
        """Turns prof [(r, z), ...] round z at c (axis: a 3x3 matrix turning that z into another direction). A radius of
        0 makes a pole (one vertex, a fan of triangles); slot may be a list (one per band, the last repeats)."""
        slots = slot if isinstance(slot, (list, tuple)) else [slot]
        c = Vector(c)
        R = axis or Matrix.Identity(3)
        rings = []
        for r, z in prof:
            if r < 1e-5:
                rings.append([self.bm.verts.new(c + R @ Vector((0, 0, z)))] * seg)
            else:
                rings.append([self.bm.verts.new(c + R @ Vector((r * sx * math.cos(2 * math.pi * i / seg),
                                                                  r * sy * math.sin(2 * math.pi * i / seg), z)))
                              for i in range(seg)])
        for k in range(len(rings) - 1):
            sl = slots[min(k, len(slots) - 1)]
            for i in range(seg):
                j = (i + 1) % seg
                q = [rings[k][i], rings[k][j], rings[k + 1][j], rings[k + 1][i]]
                uq = []
                for v in q:
                    if v not in uq:
                        uq.append(v)
                if len(uq) >= 3:
                    self.face(uq, sl, smooth=smooth)
        if cap0 and prof[0][0] > 1e-3:
            self.face(rings[0][::-1], slots[0])
        if cap1 and prof[-1][0] > 1e-3:
            self.face(rings[-1], slots[-1])

    def sphere(self, c, r, slot, seg=16, rings=8, sz=1.0, smooth=True):
        prof = [(r * math.sin(math.pi * k / rings) if 0 < k < rings else 0.0, -r * sz * math.cos(math.pi * k / rings))
                for k in range(rings + 1)]
        self.lathe(prof, slot, seg, c, False, False, smooth=smooth)

    def prism(self, poly, z0, z1, slot, slot_top=None, slot_side=None, top=True, bottom=True, M=None):
        """An upright prism over a counter-clockwise polygon [(x, y)]; M transforms (local -> mesh)."""
        M = M or Matrix.Identity(4)
        lo = [self.bm.verts.new(M @ Vector((x, y, z0))) for x, y in poly]
        hi = [self.bm.verts.new(M @ Vector((x, y, z1))) for x, y in poly]
        n = len(poly)
        for i in range(n):
            j = (i + 1) % n
            self.face((lo[i], lo[j], hi[j], hi[i]), slot_side or slot)
        if bottom:
            self.face(lo[::-1], slot)
        if top:
            self.face(hi, slot_top or slot)

    def plate(self, poly, thick, slot, M, slot_edge=None):
        """A flat plate: polygon [(u, v)] in the plane of M (local x, y), thickness along local z centred."""
        self.prism(poly, -thick / 2, thick / 2, slot, slot_side=slot_edge, M=M)

    def picture(self, corners, slot, uvs=((0, 0), (1, 0), (1, 1), (0, 1))):
        """A picture quad (counter-clockwise seen from the front): corners bl, br, tr, tl."""
        return self.face(corners, slot, uvs=list(uvs))

    def add_box_collision(self, c, size, rot_z=0.0):
        self.collision.append({"center": [round(v, 4) for v in c], "size": [round(v, 4) for v in size],
                               "rot_z_deg": round(rot_z, 2)})

    def transform(self, M):
        bmesh.ops.transform(self.bm, matrix=M, verts=self.bm.verts)

    def merge(self, other, M=None):
        """Appends another Mesh's geometry (slots, UVs, colours kept)."""
        tmp = other.bm.copy()
        if M is not None:
            bmesh.ops.transform(tmp, matrix=M, verts=tmp.verts)
        me = bpy.data.meshes.new("_tmp")
        tmp.to_mesh(me)
        tmp.free()
        remap = [self.mi(s) for s in other.slots]
        for p in me.polygons:
            p.material_index = remap[p.material_index] if p.material_index < len(remap) else 0
        self.bm.from_mesh(me)
        bpy.data.meshes.remove(me)
        self.collision += other.collision

    # ---------------------------------------------------------------- finish
    def finish_uv(self):
        """Metre UVs (box projection) on every face that has no explicit UVs."""
        s = self.uv_scale
        for f in self.bm.faces:
            if f[self.explicit]:
                continue
            n = f.normal
            ax = max(range(3), key=lambda i: abs(n[i]))
            for l in f.loops:
                p = l.vert.co
                if ax == 2:
                    uv = (p.x, p.y * (1 if n.z > 0 else -1))
                elif ax == 1:
                    uv = (p.x * (-1 if n.y > 0 else 1), p.z)
                else:
                    uv = (p.y * (1 if n.x > 0 else -1), p.z)
                l[self.uv].uv = (uv[0] * s, uv[1] * s)

    def to_object(self, weld=True):
        bm = self.bm
        if weld:
            bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
        bm.normal_update()
        self.finish_uv()
        me = bpy.data.meshes.new(self.name)
        bm.to_mesh(me)
        for s in self.slots:
            mat = bpy.data.materials.get(s) or bpy.data.materials.new(s)
            hx = slot_colour(s)[0]
            mat.diffuse_color = _lin(hx) + (1,)
            me.materials.append(mat)
        if me.color_attributes:
            me.color_attributes.active_color = me.color_attributes[0]
            me.color_attributes.render_color_index = 0
        me.update()
        o = bpy.data.objects.new(self.name, me)
        bpy.context.scene.collection.objects.link(o)
        return o


# ==================================================================================================== lint
def lint(o, closed=True, picture_overlap=True):
    """Problems of one mesh object (list of strings; empty = clean):
      zero-area faces; picture-slot UVs outside 0..1 or overlapping (per material and per side, COLOR_0.B);
      closed=True: non-manifold edges and islands whose signed volume is negative (inside-out)."""
    me = o.data
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.faces.ensure_lookup_table()
    probs = []
    zero = [f.index for f in bm.faces if f.calc_area() < 1e-8]
    if zero:
        probs.append("%d zero-area faces" % len(zero))
    uvl = bm.loops.layers.uv.get("UVMap")
    coll = bm.loops.layers.color.get("Col") if hasattr(bm.loops.layers, "color") else None
    if coll is None:
        coll = bm.loops.layers.float_color.get("Col")
    pic = {i for i, m in enumerate(me.materials) if m and is_picture(m.name.split(".")[0])}
    if pic and uvl:
        import numpy as np
        R = 256
        grids = {}
        bad = 0
        for f in bm.faces:
            if f.material_index not in pic:
                continue
            uvs = [l[uvl].uv.copy() for l in f.loops]
            if any(u.x < -1e-4 or u.x > 1 + 1e-4 or u.y < -1e-4 or u.y > 1 + 1e-4 for u in uvs):
                bad += 1
                continue
            if not picture_overlap:
                continue
            side = 0
            if coll is not None:
                side = int(round(f.loops[0][coll][2] * 4))
            key = (f.material_index, side)
            g = grids.setdefault(key, np.zeros((R, R), np.int16))
            # rasterize the face's triangles by centres of texels (conservative: inner texels only)
            for k in range(1, len(uvs) - 1):
                a, b, c = uvs[0], uvs[k], uvs[k + 1]
                x0, x1 = int(max(0, math.floor(min(a.x, b.x, c.x) * R))), int(min(R - 1, math.ceil(max(a.x, b.x, c.x) * R)))
                y0, y1 = int(max(0, math.floor(min(a.y, b.y, c.y) * R))), int(min(R - 1, math.ceil(max(a.y, b.y, c.y) * R)))
                if x1 < x0 or y1 < y0:
                    continue
                xs, ys = np.meshgrid((np.arange(x0, x1 + 1) + 0.5) / R, (np.arange(y0, y1 + 1) + 0.5) / R)
                d = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y)
                if abs(d) < 1e-12:
                    continue
                l1 = ((b.y - c.y) * (xs - c.x) + (c.x - b.x) * (ys - c.y)) / d
                l2 = ((c.y - a.y) * (xs - c.x) + (a.x - c.x) * (ys - c.y)) / d
                l3 = 1 - l1 - l2
                e = 1e-3
                inside = (l1 > e) & (l2 > e) & (l3 > e)
                g[y0:y1 + 1, x0:x1 + 1] += inside.astype(np.int16)
        if bad:
            probs.append("%d picture faces with UVs outside 0..1" % bad)
        for key, g in grids.items():
            over = int((g > 1).sum())
            if over > 4:
                probs.append("picture UV overlap: %d texels (slot %s side %d)" % (over, me.materials[key[0]].name, key[1]))
    if closed:
        nm = [e for e in bm.edges if not e.is_manifold and not e.is_wire]
        if nm:
            probs.append("%d non-manifold edges" % len(nm))
        # inside-out islands
        seen = set()
        neg = 0
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
            if any(not e.is_manifold for g in isl for e in g.edges):
                continue
            vol = 0.0
            for g in isl:
                vs = [l.vert.co for l in g.loops]
                for k in range(1, len(vs) - 1):
                    vol += vs[0].dot(vs[k].cross(vs[k + 1])) / 6.0
            if vol < -1e-9:
                neg += 1
        if neg:
            probs.append("%d inside-out islands" % neg)
    bm.free()
    return probs


def tris(o):
    return sum(len(p.vertices) - 2 for p in o.data.polygons)


def bounds(o):
    xs = [v.co for v in o.data.vertices]
    if not xs:
        return [[0, 0, 0], [0, 0, 0]]
    return [[round(min(v[i] for v in xs), 3) for i in range(3)], [round(max(v[i] for v in xs), 3) for i in range(3)]]


# ==================================================================================================== registry / export
class Registry:
    """Collects every finished mesh object of a build with its manifest row; exports the kits."""

    def __init__(self):
        self.rows = []
        self.objects = {}
        self.problems = {}

    def add(self, mesh, kit, collision="none", closed=None, lod_of=None, note="", extra=None):
        closed = mesh.closed if closed is None else closed
        o = mesh.to_object()
        smooth = mesh.smooth_slots
        if smooth:
            for p in o.data.polygons:
                m = o.data.materials[p.material_index]
                if m and m.name in smooth:
                    p.use_smooth = True
            if mesh.sharp_angle:
                o.data.set_sharp_from_angle(angle=math.radians(mesh.sharp_angle))
        probs = lint(o, closed=closed)
        if probs:
            self.problems[o.name] = probs
        row = {"asset": o.name, "kit": kit, "tris": tris(o), "bounds": bounds(o), "slots": list(mesh.slots),
               "collision": collision}
        if mesh.collision and collision == "box":
            row["collision_boxes"] = mesh.collision
        if lod_of:
            row["lod_of"] = lod_of
        if note:
            row["note"] = note
        if extra:
            row.update(extra)
        self.rows.append(row)
        self.objects[o.name] = o
        mesh.bm.free()
        return o

    def export(self, kit, path):
        objs = [self.objects[r["asset"]] for r in self.rows if r["kit"] == kit]
        bpy.ops.object.select_all(action="DESELECT")
        for o in objs:
            o.hide_set(False)
            o.select_set(True)
        if not objs:
            return
        bpy.context.view_layer.objects.active = objs[0]
        os.makedirs(os.path.dirname(path), exist_ok=True)
        bpy.ops.export_scene.gltf(filepath=path, use_selection=True, export_format="GLB", export_yup=True,
                                  export_apply=True, export_vertex_color="ACTIVE", export_all_vertex_colors=False,
                                  export_materials="PLACEHOLDER" if False else "EXPORT", export_image_format="NONE")
        print("SAILING kit", kit, len(objs), "meshes ->", os.path.relpath(path, ROOT), flush=True)


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o)


def rz(a):
    return Matrix.Rotation(a, 4, "Z")


def ry(a):
    return Matrix.Rotation(a, 4, "Y")


def rx(a):
    return Matrix.Rotation(a, 4, "X")


def tr(x, y=0.0, z=0.0):
    return Matrix.Translation((x, y, z))
