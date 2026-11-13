"""Shared helpers of the ranger's hut builds (blender/north/hut*.py): materials (HUT_*), bmesh primitives with their own UVs,
export. Frame of every part: hut-local metres, origin on the ground under the middle of the footprint, Blender -Y is the
front (door, porch, steps), +Z up. Transforms are applied (vertices are in hut-local coordinates, every object's origin is
the hut origin, so a piece drops in at the hut actor's origin).

UVs are made when a face is made, never projected afterwards:
  * boards and boxes: planar in metres along the face, u along the grain (`grain` axis), tile = the material's `tile`;
  * cylinders and lathe parts: u round (a whole number of tile widths, no seam jump), v along, in metres / tile;
  * log ends and caps: radial 0..1 over the disc; the map quad: 0..1 over the quad.
Materials: SPEC. A textured one gets a base colour (+ a tangent normal map where `normal`); the rest are flat.
"""
import math
import os
import random

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "blender", "export", "north")
import hut_tex  # noqa: E402

F = 0.40            # top of the floor (and of the porch deck, and of the door's threshold) over the ground

# name: dict(tex, color, rough, metal, normal, tile, alpha, emit)  (tile = metres per texture repeat)
SPEC = {
    "HUT_LogOut": dict(tex="bark_out", normal=True, rough=0.92, tile=1.0),
    "HUT_LogIn": dict(tex="bark_in", normal=True, rough=0.8, tile=1.0),
    "HUT_LogEnd": dict(tex="endgrain", rough=0.9, tile=0.3),
    "HUT_PlankOut": dict(tex="plank_out", normal=True, rough=0.9, tile=1.2),
    "HUT_PlankIn": dict(tex="plank_in", normal=True, rough=0.7, tile=1.2),
    "HUT_FloorIn": dict(tex="floor_in", normal=True, rough=0.55, tile=1.2),
    "HUT_Metal": dict(tex="roof", rough=0.5, metal=0.45, tile=2.0),
    "HUT_Stone": dict(tex="stone", normal=True, rough=0.95, tile=1.0),
    "HUT_Iron": dict(tex="iron", rough=0.55, metal=0.7, tile=0.5),
    "HUT_Brick": dict(tex="brick", normal=True, rough=0.9, tile=0.8),
    "HUT_Wool": dict(tex="wool", rough=1.0, tile=0.5),
    "HUT_Rug": dict(tex="rug", rough=1.0, tile=1.0),
    "HUT_Paper": dict(tex="paper", rough=0.9, tile=0.3),
    "HUT_Map": dict(tex="map", rough=0.9, tile=1.0),
    "HUT_Tin": dict(tex="tin", rough=0.45, metal=0.5, tile=0.25),
    # flat colours
    "HUT_Glass": dict(color="#a9c4c8", rough=0.05, alpha=0.28),            # window panes: the game swaps a lit glass here
    "HUT_JarGlass": dict(color="#b9d3cd", rough=0.05, alpha=0.4),
    "HUT_LampGlass": dict(color="#f4ecd2", rough=0.05, alpha=0.5, emit=0.0),   # lamp chimney and bulb: emissive at night
    "HUT_BulbGlass": dict(color="#f8efd6", rough=0.05, alpha=0.5, emit=0.0),   # the ceiling bulb: emissive when it is on
    "HUT_Enamel": dict(color="#e9e6da", rough=0.25),
    "HUT_EnamelGreen": dict(color="#2f5a49", rough=0.25),
    "HUT_EnamelBlue": dict(color="#27466e", rough=0.25),
    "HUT_Brass": dict(color="#b08a3c", rough=0.35, metal=0.9),
    "HUT_Rubber": dict(color="#1c1b1a", rough=0.8),
    "HUT_Oilskin": dict(color="#7b6e2a", rough=0.5),
    "HUT_Olive": dict(color="#5d6a4a", rough=0.5, metal=0.3),
    "HUT_Linen": dict(color="#dcd7c8", rough=1.0),
    "HUT_Red": dict(color="#a8262a", rough=0.5),
    "HUT_Black": dict(color="#151515", rough=0.5),
    "HUT_Leather": dict(color="#4a3020", rough=0.7),
    "HUT_Hemp": dict(color="#9c8660", rough=1.0),
    "HUT_Jam": dict(color="#6e1424", rough=0.2),
    "HUT_Honey": dict(color="#c98a1c", rough=0.2),
    "HUT_Zinc": dict(color="#9aa0a0", rough=0.4, metal=0.7),
    "HUT_Wire": dict(color="#2a2a2a", rough=0.5, metal=0.4),
    "HUT_Fire": dict(color="#ff8a2a", rough=0.5, emit=0.0),
}

_mats = {}
_images = {}


def _lin(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def hex_lin(h):
    h = h.lstrip("#")
    return tuple(_lin(int(h[i:i + 2], 16) / 255.0) for i in (0, 2, 4)) + (1.0,)


def image(name, arr, srgb=True):
    if name in _images:
        return _images[name]
    h, w = arr.shape[:2]
    img = bpy.data.images.new(name, w, h, alpha=False)
    img.colorspace_settings.name = "sRGB" if srgb else "Non-Color"      # (before the pixels: it clears a generated image)
    img.file_format = "JPEG" if srgb else "PNG"
    img.pixels.foreach_set(arr.astype(np.float32).ravel())
    img.update()
    _images[name] = img
    return img


def mat(name):
    """The bpy material of a HUT_* slot (built on first use)."""
    if name in _mats:
        return _mats[name]
    s = SPEC[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = s.get("rough", 0.6)
    bsdf.inputs["Metallic"].default_value = s.get("metal", 0.0)
    if "tex" in s:
        base, nrm = hut_tex.make(s["tex"])
        t = nt.nodes.new("ShaderNodeTexImage")
        t.image = image(name + "_BC", base, True)
        t.interpolation = "Linear"
        nt.links.new(t.outputs["Color"], bsdf.inputs["Base Color"])
        m.diffuse_color = (0.5, 0.5, 0.5, 1)
        if s.get("normal") and nrm is not None:
            tn = nt.nodes.new("ShaderNodeTexImage")
            tn.image = image(name + "_N", nrm, False)
            nm = nt.nodes.new("ShaderNodeNormalMap")
            nt.links.new(tn.outputs["Color"], nm.inputs["Color"])
            nt.links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
    else:
        bsdf.inputs["Base Color"].default_value = hex_lin(s["color"])
        m.diffuse_color = hex_lin(s["color"])
    if "alpha" in s:
        bsdf.inputs["Alpha"].default_value = s["alpha"]
        try:
            m.surface_render_method = "BLENDED"
        except Exception:
            pass
        try:
            m.blend_method = "BLEND"
        except Exception:
            pass
    if "emit" in s:
        bsdf.inputs["Emission Color"].default_value = hex_lin(s["color"])
        bsdf.inputs["Emission Strength"].default_value = s["emit"]
    _mats[name] = m
    return m


def tile_of(name):
    return SPEC[name].get("tile", 1.0)


def px_size(name):
    s = SPEC[name]
    return hut_tex.BUILDERS[s["tex"]][1] if "tex" in s else 0


# ---- the mesh builder -----------------------------------------------------------------------------------------------

class Mesh:
    """A bmesh with a UV layer and a list of the material names its faces use."""

    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.mats = []
        self.rng = random.Random(hash(name) & 0xffff)

    def mi(self, name):
        if name not in self.mats:
            self.mats.append(name)
        return self.mats.index(name)

    def vert(self, p, xf=None):
        v = Vector(p)
        if xf is not None:
            v = xf @ v
        return self.bm.verts.new(v)

    def face(self, vs, matname, uvs=None, grain=None, off=(0.0, 0.0), smooth=False, tile=None, ref=None):
        """A face over verts vs. uvs: explicit per-vertex UVs; else planar in metres (grain = the axis u follows).
        ref: a point inside a convex solid; the face is turned to look away from it."""
        try:
            f = self.bm.faces.new(vs)
        except ValueError:
            return None
        f.material_index = self.mi(matname)
        f.smooth = smooth
        f.normal_update()
        if ref is not None and (f.calc_center_median() - Vector(ref)).dot(f.normal) < 0:
            f.normal_flip()
        if uvs is None:
            self._planar(f, tile or tile_of(matname), grain, off)
        else:
            for l, uv in zip(f.loops, uvs):
                l[self.uv].uv = uv
        return f

    def _planar(self, f, tile, grain, off):
        n = f.normal
        t = None
        if grain is not None:
            g = Vector(grain)
            t = g - n * g.dot(n)
            if t.length < 1e-4:
                t = None
        if t is None:
            t = Vector((1, 0, 0)) if abs(n.z) > 0.7 else Vector((0, 0, 1)).cross(n)
        t = t.normalized()
        b = n.cross(t).normalized()
        for l in f.loops:
            p = l.vert.co
            l[self.uv].uv = (p.dot(t) / tile + off[0], p.dot(b) / tile + off[1])

    def finish(self, smooth_angle=None):
        """-> the bpy object (materials in first-use order, all of them real slots)."""
        me = bpy.data.meshes.new(self.name)
        bmesh.ops.remove_doubles(self.bm, verts=self.bm.verts, dist=0.0005)
        for e in self.bm.edges:                 # a flat face next to a smooth one (a log's end disc): a hard edge
            if len(e.link_faces) == 2 and e.link_faces[0].smooth != e.link_faces[1].smooth:
                e.smooth = False
        self.bm.normal_update()
        self.bm.to_mesh(me)
        for n in self.mats:
            me.materials.append(mat(n))
        me.update()
        ob = bpy.data.objects.new(self.name, me)
        bpy.context.scene.collection.objects.link(ob)
        self.bm.free()
        return ob


# ---- primitives -----------------------------------------------------------------------------------------------------

def box(m, c, size, matname, grain=None, endmat=None, faces="all", xf=None, off=None, tile=None):
    """A box centred at c, size (x, y, z). faces: sides kept ('xXyYzZ' = -x +x -y +y -z +z). grain: axis index 0/1/2 the board
    runs along (u follows it). endmat: material of the two faces across the grain (cut ends). xf: Matrix about the origin."""
    c = Vector(c)
    h = Vector(size) / 2
    vs = [m.vert(c + Vector((sx * h.x, sy * h.y, sz * h.z)), xf) for sz in (-1, 1) for sy in (-1, 1) for sx in (-1, 1)]
    ref = (xf @ c) if xf is not None else c
    sides = {"z": (0, 2, 3, 1), "Z": (4, 5, 7, 6), "y": (0, 1, 5, 4), "Y": (2, 6, 7, 3), "x": (0, 4, 6, 2), "X": (1, 3, 7, 5)}
    axis_of = {"x": 0, "X": 0, "y": 1, "Y": 1, "z": 2, "Z": 2}
    gv = None
    if grain is not None:
        a = [0, 0, 0]
        a[grain] = 1
        gv = Vector(a)
        if xf is not None:
            gv = xf.to_3x3() @ gv
    o = off or (m.rng.random(), m.rng.random())
    used = set()
    for k, ids in sides.items():
        if faces != "all" and k not in faces:
            continue
        used.update(ids)
        if endmat and grain is not None and axis_of[k] == grain:
            # the cut end: one disc-sized picture over the end face
            sz = [size[i] for i in range(3) if i != grain]
            f = m.face([vs[i] for i in ids], endmat, ref=ref, tile=max(sz) * 1.0, grain=None, off=(0.5, 0.5))
            if f:
                _centre_uv(m, f, max(sz))
        else:
            m.face([vs[i] for i in ids], matname, grain=gv, ref=ref, off=o, tile=tile)
    for i, v in enumerate(vs):
        if i not in used and v.is_valid and not v.link_faces:
            m.bm.verts.remove(v)


def _centre_uv(m, f, span):
    """Radial 0..1 UV over a cut end: the face's centre maps to 0.5."""
    cen = f.calc_center_median()
    n = f.normal
    t = Vector((1, 0, 0)) if abs(n.z) > 0.7 else Vector((0, 0, 1)).cross(n)
    t = t.normalized()
    b = n.cross(t).normalized()
    for l in f.loops:
        d = l.vert.co - cen
        l[m.uv].uv = (0.5 + d.dot(t) / span, 0.5 + d.dot(b) / span)


def bx(m, matname, x0, x1, y0, y1, z0, z1, **kw):
    box(m, ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), (x1 - x0, y1 - y0, z1 - z0), matname, **kw)


def axis_frame(a, b):
    a, b = Vector(a), Vector(b)
    e3 = (b - a).normalized()
    up = Vector((0, 0, 1)) if abs(e3.z) < 0.95 else Vector((1, 0, 0))
    e1 = up.cross(e3).normalized()
    e2 = e3.cross(e1)
    return e1, e2, e3


def cyl(m, a, b, r0, r1=None, matname="HUT_Iron", seg=12, endmat=None, caps=(True, True), smooth=True, tile=None,
        band=None, wob=0.0, rot0=0.0, split=None):
    """A (tapering) cylinder from a to b. u runs round it (a whole number of tiles), v along it in metres / tile.
    endmat: material of the end discs (log ends = HUT_LogEnd). band: (v0, v1) = constant v range, for label atlases.
    wob: random radius wobble 0..1 of r (log irregularity). split: (outward Vector, mat_out, mat_in): each side face takes
    the material by whether it looks outward or inward (a log wall: weathered outside, warm inside)."""
    r1 = r0 if r1 is None else r1
    a, b = Vector(a), Vector(b)
    e1, e2, e3 = axis_frame(a, b)
    L = (b - a).length
    tl = tile or tile_of(matname)
    nu = max(1, round(2 * math.pi * max(r0, r1) / tl))
    rad = [1.0 + wob * (m.rng.random() - 0.5) for _ in range(seg)] if wob else [1.0] * seg
    ra = [m.vert(a + (e1 * math.cos(2 * math.pi * i / seg + rot0) + e2 * math.sin(2 * math.pi * i / seg + rot0)) * r0 * rad[i]) for i in range(seg)]
    rb = [m.vert(b + (e1 * math.cos(2 * math.pi * i / seg + rot0) + e2 * math.sin(2 * math.pi * i / seg + rot0)) * r1 * rad[i]) for i in range(seg)]
    for i in range(seg):
        j = (i + 1) % seg
        u0, u1 = i / seg * nu, (i + 1) / seg * nu
        v0, v1 = (0.0, L / tl) if band is None else band
        mn = matname
        if split is not None:
            mid = 2 * math.pi * (i + 0.5) / seg + rot0
            radial = e1 * math.cos(mid) + e2 * math.sin(mid)
            mn = split[1] if radial.dot(split[0]) >= 0 else split[2]
        m.face([ra[i], ra[j], rb[j], rb[i]], mn, uvs=[(u0, v0), (u1, v0), (u1, v1), (u0, v1)], smooth=smooth)
    for end, ring, rr, sgn in ((0, ra, r0, -1), (1, rb, r1, 1)):
        if not caps[end]:
            continue
        cen = a if end == 0 else b
        vs = [m.vert(v.co) for v in ring]
        vs = vs[::-1] if end == 0 else vs
        em = endmat or matname
        uvs = []
        for v in vs:
            d = v.co - cen
            uvs.append((0.5 + d.dot(e1) / (2 * max(rr, 1e-4)), 0.5 + d.dot(e2) / (2 * max(rr, 1e-4))))
        if endmat is None:
            uvs = [(x * 0.2 + 0.4, y * 0.2 + 0.4) for x, y in uvs] if band is None else [(0.5, sum(band) / 2)] * len(vs)
        m.face(vs, em, uvs=uvs)


def lathe(m, prof, mats, seg=16, c=(0, 0, 0), caps=(True, True), tile=None, xf=None, smooth=True, sx=1.0, sy=1.0):
    """Turn prof [(r, z), ...] about the vertical axis at c. mats: a material per band (the last repeats)."""
    if isinstance(mats, str):
        mats = [mats]
    c = Vector(c)
    rings = [[m.vert(c + Vector((max(r, 1e-4) * sx * math.cos(2 * math.pi * i / seg), max(r, 1e-4) * sy * math.sin(2 * math.pi * i / seg), z)), xf)
              for i in range(seg)] for r, z in prof]
    vpos = [0.0]
    for k in range(1, len(prof)):
        vpos.append(vpos[-1] + math.hypot(prof[k][0] - prof[k - 1][0], prof[k][1] - prof[k - 1][1]))
    for k in range(len(rings) - 1):
        mn = mats[min(k, len(mats) - 1)]
        tl = tile or tile_of(mn)
        nu = max(1, round(2 * math.pi * max(prof[k][0], prof[k + 1][0], 1e-3) / tl))
        for i in range(seg):
            j = (i + 1) % seg
            u0, u1 = i / seg * nu, (i + 1) / seg * nu
            v0, v1 = vpos[k] / tl, vpos[k + 1] / tl
            m.face([rings[k][i], rings[k][j], rings[k + 1][j], rings[k + 1][i]], mn, uvs=[(u0, v0), (u1, v0), (u1, v1), (u0, v1)], smooth=smooth)
    for end in (0, 1):
        if not caps[end] or prof[0 if end == 0 else -1][0] < 1e-3:
            continue
        ring = rings[0] if end == 0 else rings[-1]
        mn = mats[0] if end == 0 else mats[-1]
        vs = [m.vert(v.co) for v in ring]
        vs = vs[::-1] if end == 0 else vs
        rr = prof[0 if end == 0 else -1][0]
        uvs = [(0.5 + (v.co.x - c.x) / (2 * rr) * 0.2 * 5 * 0.2 + 0.0, 0.5 + (v.co.y - c.y) / (2 * rr) * 0.2) for v in vs]
        uvs = [(0.4 + (v.co.x - c.x) / (2 * rr) * 0.2 * 1.0, 0.4 + (v.co.y - c.y) / (2 * rr) * 0.2) for v in vs]
        m.face(vs, mn, uvs=uvs)


def prism(m, poly, axis, a0, a1, matname, grain=None, tile=None, off=None, endmat=None):
    """A convex polygon extruded between a0 and a1 along `axis` (0, 1, 2). poly: points in the other two coordinates in the
    order of the remaining axes (y, z for x; x, z for y; x, y for z)."""
    others = [i for i in range(3) if i != axis]

    def P(pt, a):
        v = [0, 0, 0]
        v[axis] = a
        v[others[0]], v[others[1]] = pt
        return v

    lo = [m.vert(P(p, a0)) for p in poly]
    hi = [m.vert(P(p, a1)) for p in poly]
    cx = sum(p[0] for p in poly) / len(poly)
    cy = sum(p[1] for p in poly) / len(poly)
    ref = P((cx, cy), (a0 + a1) / 2)
    gv = None
    if grain is not None:
        a = [0, 0, 0]
        a[grain] = 1
        gv = Vector(a)
    o = off or (m.rng.random(), m.rng.random())
    n = len(poly)
    for i in range(n):
        j = (i + 1) % n
        m.face([lo[i], lo[j], hi[j], hi[i]], matname, grain=gv, ref=ref, off=o, tile=tile)
    m.face(lo[::-1], endmat or matname, grain=gv, ref=ref, off=o, tile=tile)
    m.face(hi, endmat or matname, grain=gv, ref=ref, off=o, tile=tile)


def softbox(m, c, size, matname, bevel=0.012, seg=2, xf=None, smooth=True, tile=None, grain=None):
    """A rounded box (cloth, boots, pillows): built on a scratch bmesh, bevelled, then copied with planar UVs."""
    tmp = bmesh.new()
    h = Vector(size) / 2
    cc = Vector(c)
    vs = [tmp.verts.new(cc + Vector((sx * h.x, sy * h.y, sz * h.z))) for sz in (-1, 1) for sy in (-1, 1) for sx in (-1, 1)]
    for ids in ((0, 2, 3, 1), (4, 5, 7, 6), (0, 1, 5, 4), (2, 6, 7, 3), (0, 4, 6, 2), (1, 3, 7, 5)):
        tmp.faces.new([vs[i] for i in ids])
    bmesh.ops.recalc_face_normals(tmp, faces=tmp.faces)
    bmesh.ops.bevel(tmp, geom=list(tmp.edges), offset=bevel, segments=seg, profile=0.5, affect="EDGES")
    bmesh.ops.recalc_face_normals(tmp, faces=tmp.faces)
    vmap = {}
    for v in tmp.verts:
        vmap[v.index] = m.vert(v.co, xf)
    tmp.verts.index_update()
    gv = None
    if grain is not None:
        a = [0, 0, 0]
        a[grain] = 1
        gv = Vector(a)
        if xf is not None:
            gv = xf.to_3x3() @ gv
    o = (m.rng.random(), m.rng.random())
    for f in tmp.faces:
        m.face([vmap[v.index] for v in f.verts], matname, smooth=smooth, grain=gv, off=o, tile=tile)
    tmp.free()


def quad(m, pts, matname, uvs=None, ref=None, smooth=False, **kw):
    return m.face([m.vert(p) for p in pts], matname, uvs=uvs, ref=ref, smooth=smooth, **kw)


def tube_path(m, pts, r, matname, seg=6):
    """A thin tube along a polyline (wires, cords)."""
    for a, b in zip(pts[:-1], pts[1:]):
        cyl(m, a, b, r, matname=matname, seg=seg, caps=(False, False), smooth=True, tile=0.5)


def rot_x(deg):
    return Matrix.Rotation(math.radians(deg), 4, "X")


def rot_y(deg):
    return Matrix.Rotation(math.radians(deg), 4, "Y")


def rot_z(deg):
    return Matrix.Rotation(math.radians(deg), 4, "Z")


def about(xf, pivot):
    """xf applied about a pivot point."""
    return Matrix.Translation(pivot) @ xf @ Matrix.Translation(-Vector(pivot))


def tri_count(ob):
    return sum(len(p.vertices) - 2 for p in ob.data.polygons)


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def export_glb(objs, path):
    """Writes the objects as one GLB (selection export; JPEG for base colours, PNG for normals; Y up)."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.export_scene.gltf(filepath=path, use_selection=True, export_format="GLB", export_yup=True, export_apply=True,
                              export_image_format="JPEG", export_jpeg_quality=88, export_cameras=False, export_lights=False, export_extras=False)


def join_copy(objs, name):
    """A joined duplicate of several objects (for a one-mesh export); the originals stay."""
    dups = []
    for o in objs:
        d = o.copy()
        d.data = o.data.copy()
        bpy.context.scene.collection.objects.link(d)
        dups.append(d)
    bpy.ops.object.select_all(action="DESELECT")
    for d in dups:
        d.select_set(True)
    bpy.context.view_layer.objects.active = dups[0]
    bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    ob.name = name
    ob.data.name = name
    return ob
